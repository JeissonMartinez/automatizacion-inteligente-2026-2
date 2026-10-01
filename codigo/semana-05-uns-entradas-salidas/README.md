# Laboratorio 1 · Espacio de nombres unificado con señales reales

Código de referencia del complemento del Laboratorio 1. Un ESP32 con cuatro señales reales, publicando bajo la jerarquía acordada por los tres equipos, y un tablero en Node-RED que muestra las entradas y opera las salidas.

La guía completa es `AutoIntelig168294-Lab_1-Espacio_de_nombres_unificado.pdf`, en `actividades/`.

## Qué hay aquí

| Archivo | Para qué |
|---|---|
| `arduino-ide/esp32_uns/esp32_uns.ino` | Programa del ESP32 para el Arduino IDE |
| `platformio/` | El mismo programa como proyecto de PlatformIO |
| `flujo-tablero-uns.json` | Flujo de Node-RED con el tablero de Dashboard 2.0 |

## Tópicos

`<base>` es `RUTA/DISPOSITIVO`, definidos en el bloque de configuración del programa.

| Tópico | Sentido | QoS | Retenido |
|---|---|---|---|
| `<base>/medicion/pulsador` | Publica | 0 | No |
| `<base>/medicion/potenciometro` | Publica | 0 | No |
| `<base>/salida/led` | Publica la confirmación | 0 | Sí |
| `<base>/salida/brillo` | Publica la confirmación | 0 | Sí |
| `<base>/cmd/led` | Recibe `0` o `1` | 1 | No |
| `<base>/cmd/brillo` | Recibe `0` a `100` | 1 | No |
| `<base>/estado` | `online` / `offline` por LWT | 1 | Sí |
| `<base>/info` | Modelo, IP y pines | 0 | Sí |

## Pines

| Señal | ESP32 | ESP32-S3 |
|---|---|---|
| Entrada discreta (pulsador a GND) | GPIO 27 | GPIO 4 |
| Entrada continua (ADC1) | GPIO 34 | GPIO 5 |
| Salida discreta (LED + 220 Ω) | GPIO 26 | GPIO 6 |
| Salida continua (LED + 220 Ω, PWM) | GPIO 25 | GPIO 7 |

El programa escoge los pines según la placa seleccionada al compilar.

## Dashboard 2.0

```bash
docker exec ai-nodered sh -c "cd /data && npm install @flowfuse/node-red-dashboard@1.30.2"
docker restart ai-nodered
```

Licencia Apache 2.0. Versión fija a propósito, igual que las imágenes del `docker-compose.yml`.

Después de importar el flujo, edite solo el nodo **Configuración**. El tablero queda en `http://localhost:1880/dashboard`.

## Prueba sin tablero

```bash
docker exec -it ai-mosquitto mosquitto_sub -h localhost -t 'RUTA/esp32-01/#' -v
docker exec ai-mosquitto mosquitto_pub -h localhost -q 1 -t 'RUTA/esp32-01/cmd/led' -m 1
```
