/* =========================================================================
 * Automatización Inteligente (168294) · Universidad de Pamplona
 * Laboratorio 1 · Espacio de nombres unificado con señales reales
 *
 * Un ESP32 con cuatro señales:
 *   - entrada discreta  (pulsador)        -> publica en  <base>/medicion/<VAR_DI>
 *   - entrada continua  (potenciómetro)   -> publica en  <base>/medicion/<VAR_AI>
 *   - salida discreta   (LED)             <- recibe en   <base>/cmd/<VAR_DO>
 *   - salida continua   (LED por PWM)     <- recibe en   <base>/cmd/<VAR_AO>
 * Después de ejecutar una orden, confirma en <base>/salida/<variable>.
 * <base> = RUTA/DISPOSITIVO
 *
 * Funciona en ESP32 clásico y en ESP32-S3, con el núcleo Arduino-ESP32
 * versión 2.x o 3.x.
 *
 * Librería necesaria (Gestor de librerías del Arduino IDE):
 *   PubSubClient, de Nick O'Leary
 *
 * Placa:
 *   ESP32 clásico  -> "ESP32 Dev Module"
 *   ESP32-S3       -> "ESP32S3 Dev Module"
 * =========================================================================
 */

#include <Arduino.h>
#include <esp_arduino_version.h>
#include <WiFi.h>
#include <PubSubClient.h>

/* =========================================================================
 * 1. LO QUE USTED DEBE CAMBIAR
 * =========================================================================
 */

// --- Red -----------------------------------------------------------------
const char* WIFI_SSID  = "CAMBIE_LA_RED";
const char* WIFI_CLAVE = "CAMBIE_LA_CLAVE";      // no la suba al repositorio
const char* BROKER_IP  = "192.168.0.000";        // IP del computador con Mosquitto
const uint16_t BROKER_PUERTO = 1883;

// --- Dirección IP fija (opcional) ----------------------------------------
// Déjela comentada para recibir la IP del enrutador (recomendado).
// #define USAR_IP_FIJA
#ifdef USAR_IP_FIJA
  IPAddress IP_FIJA(192, 168, 0, 120);
  IPAddress PUERTA (192, 168, 0, 1);
  IPAddress MASCARA(255, 255, 255, 0);
#endif

// --- Lugar en el espacio de nombres --------------------------------------
// Jerarquía acordada, desde la empresa hasta la celda.
// Sin barra al inicio ni al final.
#define RUTA         "empresa/sitio/area/linea/celda"
#define DISPOSITIVO  "esp32-01"

// Nombres de las clases de dato. Cámbielos solo si el acuerdo usa otros.
#define CLASE_MEDICION "medicion"
#define CLASE_SALIDA   "salida"
#define CLASE_CMD      "cmd"
#define CLASE_ESTADO   "estado"
#define CLASE_INFO     "info"

// Nombre de cada variable dentro de la jerarquía
#define VAR_DI  "pulsador"
#define VAR_AI  "potenciometro"
#define VAR_DO  "led"
#define VAR_AO  "brillo"

// --- Pines ---------------------------------------------------------------
// Entrada continua: SOLO pines del ADC1 (GPIO 32-39 en ESP32, GPIO 1-10 en S3).
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  const int PIN_DI = 4;
  const int PIN_AI = 5;
  const int PIN_DO = 6;
  const int PIN_AO = 7;
#else
  const int PIN_DI = 27;
  const int PIN_AI = 34;
  const int PIN_DO = 26;
  const int PIN_AO = 25;
#endif

// --- Escala de la entrada continua ---------------------------------------
// De milivoltios medidos en el pin a unidades de ingeniería.
const float MV_MIN  = 0,  MV_MAX  = 3100;
const float ING_MIN = 0,  ING_MAX = 100;
#define UNIDAD_AI  "%"

// --- Tiempos -------------------------------------------------------------
const unsigned long PERIODO_AI_MS = 2000;    // entrada continua
const unsigned long PERIODO_DI_MS = 10000;   // refresco de la entrada discreta
const unsigned long ANTIRREBOTE_MS = 30;

/* =========================================================================
 * 2. DE AQUÍ PARA ABAJO NO HACE FALTA TOCAR NADA
 * =========================================================================
 */

#define BASE        RUTA "/" DISPOSITIVO
#define T_DI        BASE "/" CLASE_MEDICION "/" VAR_DI
#define T_AI        BASE "/" CLASE_MEDICION "/" VAR_AI
#define T_DO_SAL    BASE "/" CLASE_SALIDA   "/" VAR_DO
#define T_AO_SAL    BASE "/" CLASE_SALIDA   "/" VAR_AO
#define T_DO_CMD    BASE "/" CLASE_CMD      "/" VAR_DO
#define T_AO_CMD    BASE "/" CLASE_CMD      "/" VAR_AO
#define T_CMD_TODOS BASE "/" CLASE_CMD      "/#"
#define T_ESTADO    BASE "/" CLASE_ESTADO
#define T_INFO      BASE "/" CLASE_INFO

const int PWM_FREQ = 5000;   // Hz
const int PWM_BITS = 8;      // 0-255
const int PWM_CANAL = 0;     // solo se usa con el núcleo 2.x

WiFiClient   red;
PubSubClient mqtt(red);

int  estadoDI = -1;            // último valor publicado de la entrada discreta
int  lecturaDI = -1;           // última lectura cruda, para el antirrebote
unsigned long cambioDI = 0;
unsigned long ultimoAI = 0, ultimoDI = 0;
int  valorDO = 0;              // 0 o 1
int  valorAO = 0;              // 0 a 100 %

// ----------------------------------------------------------------- salidas

void pwmIniciar() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(PIN_AO, PWM_FREQ, PWM_BITS);
#else
  ledcSetup(PWM_CANAL, PWM_FREQ, PWM_BITS);
  ledcAttachPin(PIN_AO, PWM_CANAL);
#endif
}

void pwmEscribir(int porcentaje) {
  int ciclo = map(porcentaje, 0, 100, 0, (1 << PWM_BITS) - 1);
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(PIN_AO, ciclo);
#else
  ledcWrite(PWM_CANAL, ciclo);
#endif
}

void publicarSalidas() {
  char m[64];
  snprintf(m, sizeof(m), "{\"valor\":%d,\"unidad\":\"bool\"}", valorDO);
  mqtt.publish(T_DO_SAL, m, true);                 // retenido
  snprintf(m, sizeof(m), "{\"valor\":%d,\"unidad\":\"%%\"}", valorAO);
  mqtt.publish(T_AO_SAL, m, true);                 // retenido
}

// Lee el número de la orden. Acepta "1", "60" o {"valor":60}.
bool leerNumero(const char* texto, float& salida) {
  const char* p = strstr(texto, "\"valor\"");
  if (p) p = strchr(p, ':');
  p = p ? p + 1 : texto;
  while (*p == ' ') p++;
  if (!((*p >= '0' && *p <= '9') || *p == '-' || *p == '.')) {
    if (strncmp(p, "true", 4) == 0)  { salida = 1; return true; }
    if (strncmp(p, "false", 5) == 0) { salida = 0; return true; }
    return false;
  }
  salida = atof(p);
  return true;
}

void alRecibir(char* topico, byte* contenido, unsigned int largo) {
  char texto[64];
  unsigned int n = min(largo, (unsigned int)sizeof(texto) - 1);
  memcpy(texto, contenido, n);
  texto[n] = '\0';
  Serial.printf("Orden recibida  %s  ->  %s\n", topico, texto);

  float v;
  if (!leerNumero(texto, v)) {
    Serial.println("  contenido no valido. Se ignora.");
    return;
  }

  if (strcmp(topico, T_DO_CMD) == 0) {
    valorDO = (v >= 0.5) ? 1 : 0;
    digitalWrite(PIN_DO, valorDO ? HIGH : LOW);
  } else if (strcmp(topico, T_AO_CMD) == 0) {
    valorAO = constrain((int)(v + 0.5), 0, 100);
    pwmEscribir(valorAO);
  } else {
    return;
  }
  // Confirmación: lo que quedó en la salida, no lo que se pidió.
  publicarSalidas();
}

// ---------------------------------------------------------------- entradas

void publicarDI(int valor) {
  char m[64];
  snprintf(m, sizeof(m), "{\"valor\":%d,\"unidad\":\"bool\"}", valor);
  mqtt.publish(T_DI, m);
  Serial.printf("%s  %s\n", T_DI, m);
}

float leerAI(float& mV) {
  uint32_t suma = 0;
  for (int i = 0; i < 16; i++) suma += analogReadMilliVolts(PIN_AI);
  mV = suma / 16.0;
  return ING_MIN + (mV - MV_MIN) * (ING_MAX - ING_MIN) / (MV_MAX - MV_MIN);
}

// -------------------------------------------------------------------- red

void conectarWiFi() {
  Serial.printf("\nConectando a la red %s", WIFI_SSID);
  WiFi.mode(WIFI_STA);
#ifdef USAR_IP_FIJA
  WiFi.config(IP_FIJA, PUERTA, MASCARA);
#endif
  WiFi.begin(WIFI_SSID, WIFI_CLAVE);
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }
  Serial.printf("\n  conectado. IP del ESP32: %s\n", WiFi.localIP().toString().c_str());
}

void conectarBroker() {
  while (!mqtt.connected()) {
    // Identificador único: dispositivo + final de la dirección física
    String id = String(DISPOSITIVO "-") + WiFi.macAddress().substring(12);
    id.replace(":", "");
    Serial.printf("Conectando al broker %s:%u ... ", BROKER_IP, BROKER_PUERTO);

    // LWT: si el ESP32 se desconecta sin despedirse, el bróker publica
    // "offline", retenido, en el tópico de estado.
    bool ok = mqtt.connect(id.c_str(), NULL, NULL, T_ESTADO, 1, true, "offline");
    if (ok) {
      Serial.println("listo.");
      mqtt.publish(T_ESTADO, "online", true);

      char info[320];
      snprintf(info, sizeof(info),
               "{\"dispositivo\":\"%s\",\"modelo\":\"%s\",\"ip\":\"%s\","
               "\"entradas\":{\"%s\":\"GPIO%d\",\"%s\":\"GPIO%d\"},"
               "\"salidas\":{\"%s\":\"GPIO%d\",\"%s\":\"GPIO%d PWM\"}}",
               DISPOSITIVO, ESP.getChipModel(), WiFi.localIP().toString().c_str(),
               VAR_DI, PIN_DI, VAR_AI, PIN_AI, VAR_DO, PIN_DO, VAR_AO, PIN_AO);
      mqtt.publish(T_INFO, info, true);

      mqtt.subscribe(T_CMD_TODOS, 1);   // órdenes con QoS 1
      publicarSalidas();                // valor real de las salidas al conectar
      estadoDI = -1;                    // fuerza publicar la entrada discreta
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
  Serial.printf("Base de tópicos: %s\n", BASE);

  pinMode(PIN_DI, INPUT_PULLUP);
  pinMode(PIN_DO, OUTPUT);
  digitalWrite(PIN_DO, LOW);            // salidas en cero al encender
  analogSetPinAttenuation(PIN_AI, ADC_11db);
  pwmIniciar();
  pwmEscribir(0);

  conectarWiFi();
  mqtt.setServer(BROKER_IP, BROKER_PUERTO);
  mqtt.setBufferSize(512);              // los tópicos del UNS son largos
  mqtt.setCallback(alRecibir);
  conectarBroker();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) conectarWiFi();
  if (!mqtt.connected()) conectarBroker();
  mqtt.loop();
  unsigned long ahora = millis();

  // Entrada discreta: se publica al cambiar y cada PERIODO_DI_MS.
  // Con pull-up, el pulsador presionado lee LOW. Se invierte: presionado = 1.
  int crudo = (digitalRead(PIN_DI) == LOW) ? 1 : 0;
  if (crudo != lecturaDI) { lecturaDI = crudo; cambioDI = ahora; }
  if (ahora - cambioDI >= ANTIRREBOTE_MS && lecturaDI != estadoDI) {
    estadoDI = lecturaDI;
    publicarDI(estadoDI);
    ultimoDI = ahora;
  }
  if (ahora - ultimoDI >= PERIODO_DI_MS && estadoDI >= 0) {
    publicarDI(estadoDI);
    ultimoDI = ahora;
  }

  // Entrada continua: cada PERIODO_AI_MS.
  if (ahora - ultimoAI >= PERIODO_AI_MS) {
    ultimoAI = ahora;
    float mV;
    float v = leerAI(mV);
    const char* calidad = (mV < MV_MIN - 50 || mV > MV_MAX + 50) ? "dudosa" : "buena";
    char m[96];
    snprintf(m, sizeof(m), "{\"valor\":%.1f,\"unidad\":\"%s\",\"calidad\":\"%s\"}",
             v, UNIDAD_AI, calidad);
    mqtt.publish(T_AI, m);
    Serial.printf("%s  %s\n", T_AI, m);
  }
}
