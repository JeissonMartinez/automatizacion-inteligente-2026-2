#!/usr/bin/env python3
"""
Verificación del stack de la Semana 1.
Uso:  python verificar_stack.py            (verifica localhost)
      python verificar_stack.py 192.168.1.50   (verifica el stack del laboratorio)

No requiere instalar nada: solo biblioteca estándar de Python.
"""

import socket
import sys

SERVICIOS = [
    ("Mosquitto (MQTT)", 1883),
    ("Mosquitto (WebSockets)", 9001),
    ("Node-RED", 1880),
    # Semana 5:
    # ("InfluxDB", 8086),
    # ("Grafana", 3000),
]

VERDE = "\033[92m"
ROJO = "\033[91m"
FIN = "\033[0m"


def puerto_abierto(host: str, puerto: int, timeout: float = 3.0) -> bool:
    try:
        with socket.create_connection((host, puerto), timeout=timeout):
            return True
    except OSError:
        return False


def main() -> int:
    host = sys.argv[1] if len(sys.argv) > 1 else "localhost"
    print(f"\nVerificando el stack en {host}\n" + "-" * 46)

    fallas = 0
    for nombre, puerto in SERVICIOS:
        if puerto_abierto(host, puerto):
            print(f"  {VERDE}OK{FIN}     {nombre:<26} puerto {puerto}")
        else:
            print(f"  {ROJO}FALLA{FIN}  {nombre:<26} puerto {puerto}")
            fallas += 1

    print("-" * 46)
    if fallas == 0:
        print(f"{VERDE}Todo en orden. Abra http://{host}:1880 y siga con la guía.{FIN}\n")
        return 0

    print(f"{ROJO}{fallas} servicio(s) sin responder.{FIN}")
    print("\nQué revisar, en este orden:")
    print("  1. docker compose ps      -> ¿los contenedores dicen 'running'?")
    print("  2. docker compose logs    -> ¿algún error en rojo?")
    print("  3. docker compose down && docker compose up -d")
    print("  4. Si sigue fallando, use el stack del laboratorio:")
    print("     python verificar_stack.py <IP-del-laboratorio>\n")
    return 1


if __name__ == "__main__":
    sys.exit(main())
