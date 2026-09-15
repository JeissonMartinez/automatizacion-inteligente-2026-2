/* =========================================================================
 * Automatización Inteligente (168294) · Universidad de Pamplona
 * Laboratorio 1 · Sesión 2B — Su primer dispositivo industrial
 *
 * Publica una lectura de temperatura por MQTT, con mensaje retenido de
 * estado y testamento. Funciona en ESP32 clásico y en ESP32-S3.
 *
 * Librería necesaria (Gestor de librerías del Arduino IDE):
 *   PubSubClient, de Nick O'Leary
 *
 * Placa:
 *   ESP32 clásico  -> "ESP32 Dev Module"
 *   ESP32-S3       -> "ESP32S3 Dev Module"
 * =========================================================================
 */

#include <WiFi.h>
#include <PubSubClient.h>

/* =========================================================================
 * 1. LO QUE USTED DEBE CAMBIAR
 * =========================================================================
 */

// --- Red -----------------------------------------------------------------
const char* WIFI_SSID     = "CAMBIE_LA_RED";
const char* WIFI_CLAVE    = "CAMBIE_LA_CLAVE";

// Dirección IP del computador donde corre Mosquitto.
// NO es "localhost" ni "mosquitto". El ESP32 es otra máquina.
// Averígüela con  ipconfig  (Windows)  o  ip a  (Linux/macOS).
const char* BROKER_IP     = "192.168.0.000";
const uint16_t BROKER_PUERTO = 1883;

// --- Su identidad en el espacio de nombres -------------------------------
// Esta jerarquía es PROVISIONAL. Cada equipo usa la que propuso en su
// bitácora. En la sesión 4A se comparan las tres y se rediseña.
#define RAIZ         "lab"
#define EQUIPO       "equipo-A"
#define DISPOSITIVO  "esp32-01"

// --- Qué sensor está conectado -------------------------------------------
// Deje descomentado UNO solo.
#define SENSOR_SIMULADO          // sesión 2B: sin hardware de sensor
// #define SENSOR_MAX31865       // PT100 por SPI
// #define SENSOR_MAX31855       // termopar tipo K por SPI
// #define SENSOR_LAZO_4_20MA    // transmisor de 4-20 mA sobre resistencia

// --- Cada cuánto publica -------------------------------------------------
const unsigned long PERIODO_MS = 2000;

/* =========================================================================
 * 2. DE AQUÍ PARA ABAJO NO HACE FALTA TOCAR NADA EN LA SESIÓN 2B
 * =========================================================================
 */

#define TOPICO_TEMP   RAIZ "/" EQUIPO "/" DISPOSITIVO "/temperatura"
#define TOPICO_ESTADO RAIZ "/" EQUIPO "/" DISPOSITIVO "/estado"
#define TOPICO_INFO   RAIZ "/" EQUIPO "/" DISPOSITIVO "/info"

WiFiClient   red;
PubSubClient mqtt(red);
unsigned long ultimoEnvio = 0;

// ------------------------------------------------------------------ sensor

#if defined(SENSOR_MAX31865)
  #include <Adafruit_MAX31865.h>
  // CS, DI, DO, CLK  — ajuste los pines a su cableado
  Adafruit_MAX31865 rtd = Adafruit_MAX31865(10, 11, 13, 12);
  const float RREF = 430.0;   // 430 Ω en el módulo de PT100
  const float RNOMINAL = 100.0;
  const char* NOMBRE_SENSOR = "PT100 MAX31865";

#elif defined(SENSOR_MAX31855)
  #include <Adafruit_MAX31855.h>
  Adafruit_MAX31855 termopar(12, 10, 13);   // CLK, CS, DO
  const char* NOMBRE_SENSOR = "termopar K MAX31855";

#elif defined(SENSOR_LAZO_4_20MA)
  // El transmisor entrega 4-20 mA. Sobre una resistencia de 150 Ω eso son
  // 0,6 a 3,0 V, que sí caben en la entrada analógica del ESP32.
  //
  //  ¡ADVERTENCIA!  La fuente del lazo es de 12 a 24 V. Esa tensión NUNCA
  //  toca el ESP32. La resistencia va entre el borne negativo del
  //  transmisor y el negativo de la fuente, y el ESP32 mide SOBRE la
  //  resistencia, con su GND unido al negativo de la fuente.
  const int   PIN_LAZO   = 34;      // entrada solo-lectura, sin pull-up
  const float R_SHUNT    = 150.0;   // ohmios, 1 % de tolerancia
  const float TEMP_MIN   = 0.0;     // °C que corresponden a 4 mA
  const float TEMP_MAX   = 100.0;   // °C que corresponden a 20 mA
  const char* NOMBRE_SENSOR = "transmisor 4-20 mA";

#else
  const char* NOMBRE_SENSOR = "simulado";
#endif

void iniciarSensor() {
#if defined(SENSOR_MAX31865)
  rtd.begin(MAX31865_3WIRE);
#elif defined(SENSOR_MAX31855)
  termopar.begin();
#elif defined(SENSOR_LAZO_4_20MA)
  analogSetPinAttenuation(PIN_LAZO, ADC_11db);
#endif
}

// Devuelve grados Celsius, o NAN si la lectura no es válida.
float leerTemperatura() {
#if defined(SENSOR_MAX31865)
  float t = rtd.temperature(RNOMINAL, RREF);
  if (rtd.readFault()) { rtd.clearFault(); return NAN; }
  return t;

#elif defined(SENSOR_MAX31855)
  return termopar.readCelsius();   // devuelve NAN si el termopar está abierto

#elif defined(SENSOR_LAZO_4_20MA)
  // analogReadMilliVolts usa la calibración de fábrica del chip.
  // analogRead() a secas es no lineal y da errores de varios grados.
  uint32_t suma = 0;
  for (int i = 0; i < 16; i++) suma += analogReadMilliVolts(PIN_LAZO);
  float mV = suma / 16.0;
  float mA = mV / R_SHUNT;                       // V/R = I, en mA sobre ohmios
  if (mA < 3.5 || mA > 20.5) return NAN;         // lazo abierto o fuera de rango
  return TEMP_MIN + (mA - 4.0) * (TEMP_MAX - TEMP_MIN) / 16.0;

#else
  // Onda lenta alrededor de 25 °C, más un poco de ruido.
  float fase = millis() / 30000.0;
  return 25.0 + 3.0 * sin(fase) + (random(-40, 41) / 100.0);
#endif
}

// -------------------------------------------------------------------- red

void conectarWiFi() {
  Serial.printf("\nConectando a la red %s", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_CLAVE);
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }
  Serial.printf("\n  conectado. IP del ESP32: %s\n", WiFi.localIP().toString().c_str());
}

void conectarBroker() {
  while (!mqtt.connected()) {
    String idCliente = String(EQUIPO "-" DISPOSITIVO);
    Serial.printf("Conectando al broker %s:%u ... ", BROKER_IP, BROKER_PUERTO);

    // Aquí se declara el TESTAMENTO. Son los cinco últimos argumentos:
    //   tópico, calidad de servicio, retención, mensaje
    // Si este dispositivo se cae sin despedirse, el broker publica
    // "offline" en TOPICO_ESTADO por su cuenta.
    bool ok = mqtt.connect(idCliente.c_str(),
                           NULL, NULL,          // sin usuario ni contraseña
                           TOPICO_ESTADO,       // tópico del testamento
                           1,                   // QoS 1
                           true,                // retenido
                           "offline");          // lo que dirá el testamento

    if (ok) {
      Serial.println("listo.");
      // Certificado de nacimiento: retenido, para que quien se suscriba
      // después sepa de inmediato que este dispositivo está vivo.
      mqtt.publish(TOPICO_ESTADO, "online", true);

      char info[160];
      snprintf(info, sizeof(info),
               "{\"equipo\":\"%s\",\"dispositivo\":\"%s\",\"sensor\":\"%s\","
               "\"unidad\":\"C\",\"ip\":\"%s\"}",
               EQUIPO, DISPOSITIVO, NOMBRE_SENSOR,
               WiFi.localIP().toString().c_str());
      mqtt.publish(TOPICO_INFO, info, true);
    } else {
      Serial.printf("falló, rc=%d. Reintento en 3 s.\n", mqtt.state());
      delay(3000);
    }
  }
}

// ------------------------------------------------------------------ ciclo

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n== Automatizacion Inteligente 168294 · Laboratorio 1 ==");
  Serial.printf("Publicando en %s\n", TOPICO_TEMP);

  iniciarSensor();
  conectarWiFi();
  mqtt.setServer(BROKER_IP, BROKER_PUERTO);
  conectarBroker();
}

void loop() {
  if (!mqtt.connected()) conectarBroker();
  mqtt.loop();

  if (millis() - ultimoEnvio < PERIODO_MS) return;
  ultimoEnvio = millis();

  float t = leerTemperatura();
  if (isnan(t)) {
    Serial.println("Lectura no valida. No se publica.");
    return;
  }

  // El dispositivo no sabe qué hora es: no tiene reloj de tiempo real.
  // Publica su tiempo encendido y la marca de tiempo se agrega después,
  // en el borde. Eso es contextualización, y es el tema de la unidad 3.
  char carga[128];
  snprintf(carga, sizeof(carga),
           "{\"valor\":%.2f,\"unidad\":\"C\",\"uptime_ms\":%lu}",
           t, millis());

  mqtt.publish(TOPICO_TEMP, carga);   // QoS 0, sin retener
  Serial.println(carga);
}
