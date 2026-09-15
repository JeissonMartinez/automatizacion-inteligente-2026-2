# Semana 2 · Sesión 2B — Dispositivos publicando por MQTT

Código del Laboratorio 1. Un dispositivo publica temperatura a Mosquitto, con
mensaje retenido de estado y testamento.

## Qué hay aquí

| Archivo | Para qué |
|---|---|
| `arduino-ide/esp32_mqtt/esp32_mqtt.ino` | Sketch para el Arduino IDE |
| `platformio/` | El mismo sketch como proyecto de PlatformIO, con `platformio.ini` |
| `publicador.py` | Equivalente en Python, para seguir el laboratorio sin ESP32 |
| `flujo-recepcion-dispositivos.json` | Flujo de Node-RED que recibe, contextualiza y separa los estados |

Los tres publican los mismos tópicos con el mismo contenido, así que se pueden
mezclar en el mismo laboratorio.

## Tópicos que publica

```
lab/<equipo>/<dispositivo>/temperatura   {"valor":25.4,"unidad":"C","uptime_ms":42000}
lab/<equipo>/<dispositivo>/estado        online | offline      (retenido)
lab/<equipo>/<dispositivo>/info          {"equipo":...,"sensor":...}   (retenido)
```

> **La jerarquía es provisional.** Cada equipo usa la que propuso en su bitácora.
> En la sesión 4A se comparan las tres y se rediseña al espacio de nombres
> unificado.

## Arduino IDE

1. *Archivo → Preferencias → Gestor de URLs adicionales*, agregar
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
2. *Herramientas → Placa → Gestor de tarjetas*, buscar **esp32** e instalar.
3. *Herramientas → Gestionar librerías*, instalar **PubSubClient** de Nick O'Leary.
4. Abrir `arduino-ide/esp32_mqtt/esp32_mqtt.ino`, cambiar el primer bloque,
   escoger la placa y cargar.
5. Monitor serie a **115200** baudios.

## PlatformIO

```bash
cd platformio
pio run -e esp32dev -t upload -t monitor    # ESP32 clásico
pio run -e esp32s3  -t upload -t monitor    # ESP32-S3
```

En VS Code con la extensión PlatformIO IDE, el entorno se escoge en la barra
inferior. Las librerías las resuelve `platformio.ini` solo.

## Sin ESP32

```bash
pip install paho-mqtt
python publicador.py                 # contra localhost
python publicador.py 192.168.0.50    # contra el broker de un compañero
```

## Sensores

El sketch trae cuatro modos, seleccionables con un `#define` al inicio. En la
sesión 2B se usa `SENSOR_SIMULADO`, porque los sensores todavía no están
cableados.

| Modo | Hardware | Librería |
|---|---|---|
| `SENSOR_SIMULADO` | ninguno | — |
| `SENSOR_MAX31865` | PT100 de 3 hilos por SPI | Adafruit MAX31865 |
| `SENSOR_MAX31855` | termopar tipo K por SPI | Adafruit MAX31855 |
| `SENSOR_LAZO_4_20MA` | transmisor de 4–20 mA sobre resistencia de 150 Ω | — |

### Advertencia del lazo de 4–20 mA

La fuente del lazo es de 12 a 24 V y **esa tensión nunca toca el ESP32**. La
resistencia va entre el borne negativo del transmisor y el negativo de la
fuente. El ESP32 mide sobre la resistencia, con su GND unido al negativo de la
fuente.

Sobre 150 Ω, el rango de 4 a 20 mA da de 0,6 a 3,0 V, que sí cabe en la entrada
analógica. El código usa `analogReadMilliVolts()`, que aplica la calibración de
fábrica del chip; `analogRead()` a secas es no lineal y produce errores de
varios grados.

## Errores frecuentes

| Síntoma | Causa |
|---|---|
| No conecta al WiFi | El ESP32 solo habla 2,4 GHz, no 5 GHz |
| `rc=-2` al conectar al broker | La IP del broker está mal. No es `localhost` ni `mosquitto`, es la IP del computador en la red |
| `rc=-2` con la IP correcta | El cortafuegos del computador bloquea el puerto 1883 entrante |
| No aparece el puerto serie | Cable de solo carga, o falta el controlador CP210x o CH340 |
| Se reinicia en bucle | Alimentación insuficiente. Use un puerto USB directo, no un concentrador |
| El testamento no aparece | Se cerró con `Ctrl+C`, que es una despedida ordenada. Hay que cortar de un tirón |
