#!/bin/sh
# Simulador de un dispositivo del UNS (respaldo cuando no hay ESP32 a la mano).
# Publica lo mismo que esp32_uns.ino: estado, potenciómetro y pulsador.
# Se lanza con:  docker compose --profile simulador up -d
# Se detiene con: docker compose --profile simulador stop simulador

BASE="${RUTA_SIMULADOR}/simulador-01"
PUB="mosquitto_pub -h mosquitto -u ${MQTT_USUARIO} -P ${MQTT_CLAVE}"

$PUB -r -q 1 -t "$BASE/estado" -m online
trap '$PUB -r -q 1 -t "$BASE/estado" -m offline; exit 0' TERM INT

i=0
while true; do
  # Potenciómetro: rampa de 0 a 100 % con algo de ruido
  v=$(( (i * 3) % 100 + RANDOM % 5 ))
  $PUB -t "$BASE/medicion/potenciometro" -m "{\"valor\":$v,\"unidad\":\"%\",\"calidad\":\"buena\"}"
  # Pulsador: cambia cada 10 s
  if [ $((i % 5)) -eq 0 ]; then
    $PUB -t "$BASE/medicion/pulsador" -m "{\"valor\":$(( (i / 5) % 2 )),\"unidad\":\"bool\"}"
  fi
  i=$((i + 1))
  sleep 2 & wait $!
done
