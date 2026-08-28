# Automatización Inteligente · 168294

Electiva de Ingeniería Mecatrónica · Universidad de Pamplona · Campus Villa del Rosario
Semestre 2026-2 · Ing. Jeisson Harvey Martínez Flórez

---

## Qué hay aquí

| Carpeta | Contenido |
|---|---|
| `docs/` | Contenido programático de la asignatura |
| `diapositivas/` | Las diapositivas de cada tema, en PDF |
| `codigo/` | Código y configuraciones que usamos en el laboratorio |
| `actividades/` | Guías de laboratorio y talleres |

Este repositorio es **de consulta**. Sus entregas y su bitácora van al repositorio de su equipo.

---

## Cómo empezar

```bash
git clone <url-de-este-repositorio>
cd automatizacion-inteligente-2026-2
```

Cuando publique material nuevo, actualice con:

```bash
git pull
```

---

## Levantar el entorno del curso

Necesita un motor de contenedores instalado. Ver el correo de prerrequisitos.

```bash
cd codigo/semana-02-stack-mqtt
docker compose up -d      # levantar
docker compose ps         # verificar: dos contenedores en estado running
python verificar_stack.py # tres líneas en verde
docker compose down       # apagar al terminar
```

Node-RED queda en http://localhost:1880

> Si algo falla, el error más frecuente es que el nodo MQTT apunte a `localhost`.
> Dentro de la red de contenedores el broker se llama `mosquitto`, no `localhost`.

---

## Su repositorio de equipo

Cada equipo tiene el suyo. Ahí van:

```
bitacoras/<su-apellido>.md      una entrada por sesión, todo el semestre
entregas/semana-NN/             lo que se pida en cada laboratorio
```

### La bitácora

Tres a cinco líneas por sesión:

```markdown
## Semana N · sesión A

**Qué hice:**

**Qué se rompió y cómo lo resolví:**

**Qué no entendí todavía:**
```

Sea honesto en «qué no entendí». Nadie pierde puntos por eso; los pierde por callarlo hasta la semana 8.

**Si usó un asistente de inteligencia artificial**, declare qué le pidió y qué verificó. Está permitido y esa transparencia se evalúa.

---

## Reglas de entrega

- Una entrega que solo funciona en el computador de quien la hizo no se considera válida. Por eso trabajamos con contenedores y control de versiones.
- Toda entrega admite defensa sobre evidencia grabada. Grabe un video corto de su solución funcionando, desde la primera semana.
- El portafolio completo de este repositorio se evalúa en el Corte 3. El criterio es la reproducibilidad: ¿puede un tercero levantar lo que usted hizo?

---

## Software del curso

Todo es libre o gratuito. No necesita comprar ninguna licencia.

Mosquitto · Node-RED · InfluxDB · Grafana · Python con NumPy, pandas y scikit-learn · AASX Package Explorer · Eclipse BaSyx · Arduino IDE o PlatformIO · MQTT Explorer · Git · VS Code

---

## Contacto

Asesorías presenciales: lunes 9:00–11:00 · miércoles 10:00–12:00 · jueves 14:00–15:00
Lugar: GM 105 o Biblioteca

Traiga la pregunta trabajada: qué intentó, qué error obtuvo y qué ya descartó.
