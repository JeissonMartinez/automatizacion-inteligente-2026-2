#!/usr/bin/env python3
"""
Automatización Inteligente (168294) · Universidad de Pamplona
Laboratorio 1 · Sesión 2B — Dispositivo de respaldo

Hace exactamente lo mismo que el sketch del ESP32: publica temperatura por
MQTT, con mensaje retenido de estado y testamento. Sirve para seguir el
laboratorio cuando el entorno de Arduino no está listo.

Instalación:
    pip install paho-mqtt

Uso:
    python publicador.py                      # contra localhost
    python publicador.py 192.168.0.50         # contra el broker de un compañero

Se detiene con Ctrl+C. Fíjese en lo que pasa en MQTT Explorer cuando lo
detiene con Ctrl+C frente a cuando cierra la ventana de un tirón.
"""

import json
import math
import random
import socket
import sys
import time

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("Falta la librería. Ejecute:  pip install paho-mqtt")
    sys.exit(1)

# ---------------------------------------------------------------------------
# LO QUE USTED DEBE CAMBIAR
# ---------------------------------------------------------------------------

RAIZ = "lab"
EQUIPO = "equipo-A"          # equipo-A, equipo-B o equipo-C
DISPOSITIVO = "pc-apellido"  # cambie apellido por el suyo

PERIODO_S = 2.0

# ---------------------------------------------------------------------------

BROKER = sys.argv[1] if len(sys.argv) > 1 else "localhost"
PUERTO = 1883

BASE = f"{RAIZ}/{EQUIPO}/{DISPOSITIVO}"
TOPICO_TEMP = f"{BASE}/temperatura"
TOPICO_ESTADO = f"{BASE}/estado"
TOPICO_INFO = f"{BASE}/info"


def ip_local() -> str:
    """La IP con la que este computador sale a la red del laboratorio."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("8.8.8.8", 80))
        return s.getsockname()[0]
    except OSError:
        return "desconocida"
    finally:
        s.close()


def main() -> int:
    cliente = mqtt.Client(client_id=f"{EQUIPO}-{DISPOSITIVO}")

    # EL TESTAMENTO se declara ANTES de conectar. Si este programa muere sin
    # despedirse, el broker publica "offline" por su cuenta.
    cliente.will_set(TOPICO_ESTADO, "offline", qos=1, retain=True)

    print(f"Conectando a {BROKER}:{PUERTO} ...")
    try:
        cliente.connect(BROKER, PUERTO, keepalive=15)
    except OSError as e:
        print(f"\nNo se pudo conectar: {e}")
        print("Revise, en este orden:")
        print("  1. docker compose ps   ->  ¿ai-mosquitto está corriendo?")
        print("  2. ¿La IP es la correcta? Use la del tablero, no localhost,")
        print("     si está apuntando al broker de un compañero.")
        print("  3. El cortafuegos del computador destino puede estar")
        print("     bloqueando el puerto 1883.")
        return 1

    cliente.loop_start()
    print(f"Conectado. Publicando en {TOPICO_TEMP}\n")

    # Certificado de nacimiento, retenido.
    cliente.publish(TOPICO_ESTADO, "online", retain=True)
    cliente.publish(
        TOPICO_INFO,
        json.dumps({
            "equipo": EQUIPO,
            "dispositivo": DISPOSITIVO,
            "sensor": "simulado",
            "unidad": "C",
            "ip": ip_local(),
        }),
        retain=True,
    )

    t0 = time.time()
    try:
        while True:
            fase = (time.time() - t0) / 30.0
            valor = 25.0 + 3.0 * math.sin(fase) + random.uniform(-0.4, 0.4)
            carga = json.dumps({
                "valor": round(valor, 2),
                "unidad": "C",
                "uptime_ms": int((time.time() - t0) * 1000),
            })
            cliente.publish(TOPICO_TEMP, carga)
            print(carga)
            time.sleep(PERIODO_S)

    except KeyboardInterrupt:
        # Despedida ordenada: se avisa antes de desconectar, así que el
        # broker NO llega a publicar el testamento.
        print("\nDespedida ordenada.")
        cliente.publish(TOPICO_ESTADO, "offline", retain=True)
        time.sleep(0.3)
        cliente.loop_stop()
        cliente.disconnect()
        return 0


if __name__ == "__main__":
    sys.exit(main())
