# Broker MQTT en la nube · Variante AWS (EC2)

Esta guía reemplaza **solo** los pasos que dependen del proveedor (cuenta, máquina, puerto, nombre y conexión). Desde la instalación de Docker en adelante todo es igual que en [`README.md`](README.md). La única diferencia es que en AWS el usuario de Ubuntu se llama `ubuntu`, no `azureuser`.

| | Azure for Students | AWS · Plan gratuito |
|---|---|---|
| Tarjeta | No | **Sí**, solo para verificar identidad |
| Crédito | USD 100, 12 meses | Hasta USD 200 (100 al registrarse + 20 por actividad guiada) |
| Duración | 12 meses | 6 meses o hasta agotar el crédito |
| Al terminar | Se suspenden los recursos | **La cuenta se cierra** si no pasa a plan de pago |
| Riesgo de cobro | Ninguno | Ninguno mientras siga en plan gratuito |

---

## Paso 1 · Crear la cuenta en el plan gratuito

Registro: <https://signup.aws.amazon.com/billing/signup?type=register>

1. **Correo raíz y nombre de la cuenta** (`uns-apellido`). AWS envía un código al correo; ingréselo.
2. **Contraseña del usuario raíz.** Guárdela bien: es la llave maestra de la cuenta.
3. **Información de contacto:** tipo *Personal*, país Colombia, dirección y celular.
4. **Método de pago:** tarjeta débito o crédito. AWS puede hacer una retención pequeña que luego devuelve.
5. **Verificación de identidad:** código por SMS o llamada.
6. **Plan de cuenta** (la pantalla `#/accountplan`): elija **Plan gratuito** (*Free plan*), **no** *Plan de pago*.
7. **Soporte:** *Básico (gratuito)*.

> 📸 **Captura A1** · `img/aws-01-plan-gratuito.png` — pantalla de elección de plan con *Plan gratuito* marcado.

Entre a <https://console.aws.amazon.com>. En la parte superior derecha, cambie el idioma a español si lo desea (⚙️ → *Idioma*).

---

## Paso 2 · Elegir la región

Arriba a la derecha, junto al nombre de la cuenta:

- **América del Sur (São Paulo) `sa-east-1`**: menor latencia desde Colombia.
- **EE. UU. Este (Norte de Virginia) `us-east-1`**: la más barata y con más disponibilidad.

**Todo lo que cree queda en esa región.** Si después «no aparece» la instancia, casi siempre es porque cambió de región sin darse cuenta.

---

## Paso 3 · Lanzar la instancia EC2

Busque **EC2** en la barra superior → **Lanzar instancia**.

| Campo | Valor |
|---|---|
| Nombre | `vm-broker-uns` |
| Imagen (AMI) | **Ubuntu Server 24.04 LTS**, con la etiqueta *Apto para la capa gratuita* |
| Tipo de instancia | **t3.micro** (o `t4g.micro` si la AMI es ARM). Debe decir *Apto para la capa gratuita* |
| Par de claves | **Crear nuevo par de claves** → nombre `clave-broker`, tipo RSA, formato **.pem** → se descarga solo. **No se puede volver a descargar** |
| Almacenamiento | Deje los 8 GB gp3 por defecto |

### Configuración de red (el equivalente al paso 3 de Azure)

En **Configuración de red → Editar**:

1. **Crear grupo de seguridad**, nombre `sg-broker-uns`.
2. Regla 1 (ya viene): **SSH**, TCP 22, origen `0.0.0.0/0` (necesario para conectarse desde el navegador en el paso 5).
3. **Agregar regla de grupo de seguridad:**

| Tipo | Protocolo | Puerto | Origen | Descripción |
|---|---|---|---|---|
| TCP personalizado | TCP | `1883` | `0.0.0.0/0` | MQTT |

Pulse **Lanzar instancia**. En un minuto el estado pasa a *En ejecución* y las comprobaciones a *2/2*.

> 📸 **Captura A2** · `img/aws-02-tipo-instancia.png` — AMI y tipo con la etiqueta *Apto para la capa gratuita*.
> 📸 **Captura A3** · `img/aws-03-grupo-seguridad.png` — reglas 22 y 1883 del grupo de seguridad.

> **Si le aparece la regla 1883 pero el broker no responde:** en AWS el cortafuegos es el **grupo de seguridad**. Revíselo en *Instancias → su instancia → Seguridad*.

---

## Paso 4 · El nombre del broker

Seleccione la instancia y copie **DNS de IPv4 pública**, algo como:

```
ec2-54-233-10-20.sa-east-1.compute.amazonaws.com
```

Ese es el host del broker en el ESP32, `publicador.py`, Node-RED y MQTT Explorer.

> ⚠️ **Diferencia importante con Azure:** si **detiene** la instancia, AWS le asigna otra IP y otro nombre al iniciarla. Hay dos opciones:
>
> - **Para una sola clase:** no la detenga mientras se trabaja, y al iniciarla de nuevo actualice el host en los clientes.
> - **Para todo el semestre:** *EC2 → Red y seguridad → Direcciones IP elásticas → Asignar* y luego *Asociar* a la instancia. La IP queda fija. Consume crédito, como toda IPv4 pública en AWS.

---

## Paso 5 · Entrar a la instancia

### Opción A · Desde el navegador (recomendada en clase)

Instancia → **Conectar → EC2 Instance Connect → Conectar**. Se abre una terminal ya dentro de Ubuntu, sin manejar archivos `.pem`.

> 📸 **Captura A4** · `img/aws-04-instance-connect.png` — terminal de EC2 Instance Connect abierta.

### Opción B · Desde la terminal del computador

```bash
# Linux / macOS
chmod 400 clave-broker.pem
ssh -i clave-broker.pem ubuntu@ec2-54-233-10-20.sa-east-1.compute.amazonaws.com
```

```powershell
# Windows (PowerShell). Si sale "UNPROTECTED PRIVATE KEY FILE", corrija permisos:
icacls .\clave-broker.pem /inheritance:r /grant:r "$($env:USERNAME):(R)"
ssh -i .\clave-broker.pem ubuntu@ec2-54-233-10-20.sa-east-1.compute.amazonaws.com
```

---

## Pasos 6 a 10 · Igual que en `README.md`

Continúe con **Paso 6 · Instalar Docker** del `README.md`. Todo funciona sin cambios: mismo `docker-compose.yml`, mismo `mosquitto.conf` y mismos usuarios con `mosquitto_passwd`. Solo cambie `uns-unipamplona.eastus.cloudapp.azure.com` por el DNS de su instancia.

---

## Paso 11 · Cuidar el crédito y terminar

| Acción | Dónde |
|---|---|
| Ver crédito restante | Consola → *Facturación y administración de costos → Créditos* |
| Ganar USD 20 adicionales y recibir alertas | *Explorar AWS* → actividad **Configurar un presupuesto** (AWS Budgets) |
| Pausar | EC2 → Instancia → **Estado → Detener** (cambia la IP, ver paso 4) |
| Borrar todo | **Estado → Terminar**. Si asignó IP elástica: *Disociar* y luego *Liberar* |

---

## Errores frecuentes específicos de AWS

| Síntoma | Causa |
|---|---|
| El tipo `t3.micro` no aparece o sale bloqueado | En plan gratuito solo se permiten tipos marcados como aptos; revise el filtro *Apto para la capa gratuita* |
| «No veo mi instancia» | Está en otra región. Revise el selector arriba a la derecha |
| Instance Connect falla | La regla SSH no permite `0.0.0.0/0`, o la AMI no es Ubuntu/Amazon Linux |
| `Permission denied (publickey)` | Usuario equivocado: en Ubuntu es `ubuntu`, no `ec2-user` ni `azureuser` |
| Funcionaba ayer y hoy no conecta | Se detuvo la instancia y cambió el DNS público. Use IP elástica |
| `verificar_stack.py` da FALLA en 1883 | Falta la regla 1883 en el grupo de seguridad |

---

## Referencias

- Plan gratuito de AWS (créditos, duración, cierre de cuenta): <https://aws.amazon.com/free/>
- Anuncio del nuevo Free Tier: <https://aws.amazon.com/blogs/aws/aws-free-tier-update-new-customers-can-get-started-and-explore-aws-with-up-to-200-in-credits>
- Tipos de instancia aptos para la capa gratuita: <https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/ec2-free-tier-usage.html>
- Grupos de seguridad de EC2: <https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/ec2-security-groups.html>
- EC2 Instance Connect: <https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/connect-linux-inst-eic.html>
- Direcciones IP elásticas: <https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/elastic-ip-addresses-eip.html>
