#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <secretos.h>
#include <time.h>
#include <sys/time.h>
#include <algorithm>

// --- HORA (NTP) ----------------------------------------------------------
const char* NTP_SERVIDOR = "pool.ntp.org";
const char* ZONA_HORARIA = "<-05>5";   // Colombia: UTC-5

// --- RED -----------------------------------------------------------------
const char* WIFI_SSID     = SECRETO_WIFI_SSID;
const char* WIFI_CLAVE    = SECRETO_WIFI_CLAVE;

//--- DATOS DEL SERVIDOR DONDE CORRE MOSQUITTO ----------------------------
const char* BROKER_IP     = "15.229.1.164";
const uint16_t BROKER_PUERTO = 1883;
const char* USER          = SECRETO_MQTT_USER;
const char* PASS          = SECRETO_MQTT_PASS;

// --- WIFI ------------------------------------------------------------
WiFiClient   red;
PubSubClient mqtt(red);
unsigned long ultimoEnvio = 0;

// Último valor aplicado a cada salida
int estadoLED = 0;
int estadoPWM = 0;


// --- UNS -------------------------------

// General:
#define EMPRESA           "unipamplona"
#define SITIO             "villa-del-rosario"
#define AREA              "lab-automatizacion"
#define LINEA             "banco-profe"
#define RAIZ              EMPRESA "/" SITIO "/" AREA "/" LINEA

// Personal:
#define CELDA             "estacion-01"
#define DISPOSITIVO       "esp32s3-01"
#define CLASE_MEDICION         "medicion"
#define CLASE_SALIDA           "salida"
#define CLASE_CMD              "comando"
#define CLASE_ESTADO           "estado"
#define CLASE_INFO             "info"

// Tópicos:
#define TOPICO_MEDICION   RAIZ "/" CELDA "/" DISPOSITIVO "/" CLASE_MEDICION
#define TOPICO_SALIDA     RAIZ "/" CELDA "/" DISPOSITIVO "/" CLASE_SALIDA
#define TOPICO_CMD        RAIZ "/" CELDA "/" DISPOSITIVO "/" CLASE_CMD
#define TOPICO_ESTADO     RAIZ "/" CELDA "/" DISPOSITIVO "/" CLASE_ESTADO
#define TOPICO_INFO       RAIZ "/" CELDA "/" DISPOSITIVO "/" CLASE_INFO

// --- ELEGIR SENSOR -------------------------------------------
// define SENSOR_SIMULADO          
#define MONTAJE_FISICO        // Interruptor, Potenciómetro, LED_ONOFF y LED_PWM


// --- PERIODO DE MUESTREO -------------------------------------------------
const unsigned long PERIODO_MS = 500;


// --- DEFINICIONES DE SENSORES ------------------------------------------------
#if defined(MONTAJE_FISICO)
  // --- Entradas 
  const int PIN_PULSADOR   = 5;    // GPIO5 digital
  const int PIN_POT        = 6;    // GPIO6 ADC1_CH5

  // --- Salidas 
  const int PIN_LED_ONOFF  = 11;   // GPIO11 digital
  const int PIN_LED_PWM    = 9;    // GPIO9 PWM (LEDC)

  // PWM con el periférico LEDC del ESP32
  const int PWM_CANAL      = 0;     // canal LEDC 0..7
  const int PWM_FRECUENCIA = 5000;  // Hz
  const int PWM_RESOLUCION = 8;     // bits -> ciclo útil de 0 a 255

  // El ADC del ESP32-S3 es de 12 bits: 0..4095
  const int ADC_MAX        = 4095;

  const char* NOMBRE_SENSOR = "montaje fisico";

#else
  const char* NOMBRE_SENSOR = "simulado";
#endif

void iniciarSensor() {
#if defined(MONTAJE_FISICO)
  pinMode(PIN_PULSADOR, INPUT_PULLUP);  // Presionado = LOW
  pinMode(PIN_LED_ONOFF, OUTPUT);
  analogSetPinAttenuation(PIN_POT, ADC_11db);   // rango completo 0..3,3 V
  analogRead(PIN_POT); 
  ledcSetup(PWM_CANAL, PWM_FRECUENCIA, PWM_RESOLUCION);
  ledcAttachPin(PIN_LED_PWM, PWM_CANAL);
#endif
}

// --- FUNCIONES DE LECTURA DE SENSORES ------------------------------------------------
float leerTemperatura() {
#if defined(MONTAJE_FISICO)
  // Mediana de 9 lecturas
  int m[9];
  for (int i = 0; i < 9; i++) m[i] = analogRead(PIN_POT);
  std::sort(m, m + 9);
  Serial.printf("ADC origin: %d   mV: %d\n", m[4], analogReadMilliVolts(PIN_POT));
  float porcentaje_pt = m[4] * 100.0 / ADC_MAX;
  return 20.0 + porcentaje_pt * 0.1;  // 20..30 °C
#else
  // Onda lenta alrededor de 25 °C, más un poco de ruido.
  float fase = millis() / 30000.0;
  return 25.0 + 3.0 * sin(fase) + (random(-40, 41) / 100.0);
#endif
}

int leerPulsador() {
#if defined(MONTAJE_FISICO)
  bool presionado = digitalRead(PIN_PULSADOR) == HIGH;
  return presionado ? 1 : 0;
#else
  return random(0, 2);  // 0 o 1
#endif
}

// --- FUNCIONES DE SALIDAS ------------------------------------------------
int escribirLED_ONOFF(int valor) {
#if defined(MONTAJE_FISICO)
  digitalWrite(PIN_LED_ONOFF, valor ? HIGH : LOW);
  return valor;
#else
  return valor;
#endif
}

int escribirLED_PWM(int valor) {
#if defined(MONTAJE_FISICO)
  valor = constrain(valor, 0, 255);
  ledcWrite(PWM_CANAL, valor);
  return valor;
#else
  return valor;
#endif
}

// --- FUNCIONES DE HORA ----------------------------------------------------
// Pide la hora a un servidor NTP
void sincronizarHora() {
  configTzTime(ZONA_HORARIA, NTP_SERVIDOR, "time.google.com");
  Serial.print("Sincronizando hora");
  struct tm ahora;
  while (!getLocalTime(&ahora)) Serial.print(".");
  Serial.println(&ahora, "\n  Hora local: %Y-%m-%d %H:%M:%S");
}

// Milisegundos desde 1970-01-01 00:00 UTC (tiempo Unix).
uint64_t epochMs() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return (uint64_t)tv.tv_sec * 1000ULL + tv.tv_usec / 1000;
}

// --- COMANDOS Y ESTADO DE SALIDAS ------------------------------------------------

// Busca "clave":numero dentro del JSON. Devuelve false si la clave no está.
bool leerEntero(const char* json, const char* clave, int& valor) {
  char patron[24];
  snprintf(patron, sizeof(patron), "\"%s\":", clave);
  const char* p = strstr(json, patron);
  if (!p) return false;
  valor = atoi(p + strlen(patron));
  return true;
}

// Publica lo que de verdad quedó aplicado en las salidas (retenido).
void publicarSalidas() {
  char carga[96];
  snprintf(carga, sizeof(carga),
           "{\"led\":%d,\"pwm\":%d,\"ts\":%llu}",
           estadoLED, estadoPWM, (unsigned long long)epochMs());
  mqtt.publish(TOPICO_SALIDA, carga, true);
  Serial.printf("Salida  -> %s\n", carga);
}

// Cuando llega un mensaje a TOPICO_CMD | Ejemplos:  {"led":1}   {"pwm":128}   {"led":0,"pwm":255}
void alRecibirComando(char* topico, byte* carga, unsigned int largo) {
  char json[128];
  if (largo >= sizeof(json)) largo = sizeof(json) - 1;
  memcpy(json, carga, largo);
  json[largo] = '\0';
  Serial.printf("Comando <- %s\n", json);

  int v;
  if (leerEntero(json, "led", v)) estadoLED = escribirLED_ONOFF(v ? 1 : 0);
  if (leerEntero(json, "pwm", v)) estadoPWM = escribirLED_PWM(v);
  publicarSalidas();
}


// --- FUNCIONES DE RED -------------------------------------------------------------

void conectarWiFi() {
  Serial.printf("\nConectando a la red %s", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_CLAVE);
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }
  Serial.printf("\n  Conectado. IP del ESP32: %s\n", WiFi.localIP().toString().c_str());
}

void conectarBroker() {
  while (!mqtt.connected()) {
    String idCliente = String(RAIZ "-" CELDA "-" DISPOSITIVO);
    Serial.printf("Conectando al broker %s:%u ... ", BROKER_IP, BROKER_PUERTO);

    // Declara la Conexión
    bool ok = mqtt.connect(idCliente.c_str(),
                           USER, PASS,          // usuario y contraseña
                           TOPICO_ESTADO,       // tópico del testamento
                           1,                   // QoS 1
                           true,                // retenido
                           "offline");          // lo que dirá el testamento

    if (ok) {
      Serial.println("listo.");
      mqtt.publish(TOPICO_ESTADO, "online", true); // Certificado Nacimiento: retenido

      // Configuración JSON con la info del dispositivo
      char info[160];
      snprintf(info, sizeof(info),
               "{\"Celda\":\"%s\",\"Dispositivo\":\"%s\",\"Tipo\":\"%s\",\"IP\":\"%s\"}",
               CELDA, DISPOSITIVO, NOMBRE_SENSOR,
               WiFi.localIP().toString().c_str());
      mqtt.publish(TOPICO_INFO, info, true);      // Certificado Info: retenido

      mqtt.subscribe(TOPICO_CMD, 1);              // Escucha comandos para las salidas
      publicarSalidas();                          // Reporta el estado actual

    } else {
      Serial.printf("falló, rc=%d. Reintento en 3 s.\n", mqtt.state());
      delay(3000);
    }
  }
}

// ---- CONFIGURACIÓN Y BUCLE PRINCIPAL ------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n== Automatizacion Inteligente 168294 · Laboratorio 2 ==");
  Serial.printf("Tópicos disponibles:\n");
  Serial.printf("  %s\n", TOPICO_MEDICION);
  Serial.printf("  %s\n", TOPICO_SALIDA);
  Serial.printf("  %s\n", TOPICO_CMD);
  Serial.printf("  %s\n", TOPICO_ESTADO);
  Serial.printf("  %s\n", TOPICO_INFO);

  iniciarSensor();
  conectarWiFi();
  sincronizarHora();

  mqtt.setServer(BROKER_IP, BROKER_PUERTO);
  mqtt.setCallback(alRecibirComando);
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
  int pulsador = leerPulsador();

  // Usamos el servidor NTP para obtener el timestamp en milisegundos desde 1970-01-01 00:00 UTC.
  char carga[128];
  snprintf(carga, sizeof(carga),
           "{\"temperatura\":%.2f,\"unidad\":\"C\",\"pulsador\":%d,\"ts\":%llu}",
           t, pulsador, (unsigned long long)epochMs());
  // Mensaje queda así: {"temperatura":24.87,"unidad":"C","pulsador":0,"ts":1793721600123}

  mqtt.publish(TOPICO_MEDICION, carga);   // QoS 0, sin retener
  Serial.printf("Medicion -> %s\n", carga);
}
