# Guía Semana 07 (complemento) - Conectarse a la VM por SSH desde cualquier red

En la guía de creación de la VM, la regla de entrada del puerto **22 (SSH)** quedó con el origen **Mi IP**. Eso significa que AWS solo acepta conexiones desde la dirección IP pública que tenía nuestro computador **el día en que se creó la regla**.

El problema aparece apenas cambiamos de red: de la universidad a la casa, o al hotspot del celular. La IP pública cambia, AWS descarta la conexión sin avisar y la terminal se queda esperando hasta que sale un error como:

```
ssh: connect to host 15.229.1.164 port 22: Operation timed out
```

La solución rápida sería volver a la consola de AWS y actualizar **Mi IP** cada vez. En esta guía vamos a configurar algo mejor: **AWS Systems Manager - Session Manager**, para entrar a la VM desde cualquier red **sin tener abierto el puerto 22**.

> Se espera que ya se tenga la VM creada y funcionando, y el archivo `.pem` del par de claves guardado, como se explicó en la [Guía para crear la VM](./Guia_Crear_VM_Broker.md).


## ¿Qué es lo que vamos a hacer?

Con SSH normal, nuestro computador **entra** a la VM por el puerto 22. Por eso el grupo de seguridad tiene que dejarlo abierto, y por eso hay que decirle desde qué IP.

Con Session Manager el sentido se invierte: es la **VM la que sale** hacia los servicios de AWS y mantiene abierto un canal con ellos. Cuando queremos conectarnos, nuestro computador le habla a AWS (no a la VM), y AWS nos pasa por ese canal.

| | SSH directo | SSH por Session Manager |
| --- | --- | --- |
| ¿Quién inicia la conexión? | Nuestro computador hacia la VM | La VM hacia AWS |
| Regla de entrada del puerto 22 | Necesaria | **Ninguna** |
| ¿Depende de nuestra IP? | Sí | No |
| ¿Depende de la IP pública de la VM? | Sí | No, usa el ID de la instancia |
| ¿Cómo nos identificamos? | Archivo `.pem` | Credenciales de AWS **y** archivo `.pem` |

Es decir, quedamos con **más seguridad** (nadie de Internet puede siquiera intentar conectarse al 22) y con **más comodidad** (funciona desde cualquier red).

El procedimiento tiene tres partes:
1. **En AWS:** darle permiso a la VM para usar Session Manager y probarlo desde el navegador (pasos 1 a 3).
2. **En nuestro computador:** instalar las herramientas y crear un acceso para la terminal (pasos 4 a 8).
3. **Cerrar el puerto 22** en el grupo de seguridad (paso 9).

> Session Manager no tiene costo adicional. El agente que necesita la VM ya viene instalado en Ubuntu Server de AWS.


## Paso 1: Crear un rol para la VM

Un **rol de IAM** es un conjunto de permisos que se le asigna a un servicio de AWS (en este caso, a nuestra VM) para que pueda usar otros servicios. Aquí, la VM necesita permiso para registrarse en Systems Manager.

1. En la barra superior, buscar **IAM** → menú **Roles** → **Crear rol**.
2. Configurar:

| Campo | Valor |
| --- | --- |
| Tipo de entidad de confianza | **Servicio de AWS** |
| Caso de uso | **EC2** |

3. **Siguiente**. En el buscador de políticas, escribir `AmazonSSMManagedInstanceCore` y marcar la casilla.
4. **Siguiente**. Poner como nombre: `rol-vm-ssm`.
5. **Crear rol**.


## Paso 2: Asignar el rol a la VM

1. Ir a **EC2** → **Instancias** → seleccionar la instancia (`vm-broker-uns`).
2. **Acciones** → **Seguridad** → **Modificar el rol de IAM**.
3. Elegir `rol-vm-ssm` → **Actualizar el rol de IAM**.

En la pestaña **Seguridad** de la instancia, el campo *Rol de IAM* debe mostrar ahora `rol-vm-ssm`.

Mientras estamos aquí, copiamos el **ID de la instancia**, que aparece en la pestaña **Detalles** y tiene la forma `i-0d06aec1f7e34b6ba`. Lo vamos a necesitar más adelante.

> El agente de la VM revisa sus permisos cada cierto tiempo, así que puede tardar **entre 5 y 10 minutos** en registrarse. Si pasado ese tiempo el paso 3 no funciona, reiniciar la instancia: **Estado de la instancia** → **Reiniciar**. Usar **Reiniciar** y no *Detener* + *Iniciar*, porque esto último le cambia la IP pública a la VM, y los ESP32 dejarían de encontrar el broker.


## Paso 3: Probar desde el navegador

Antes de configurar nuestro computador, comprobamos que la VM ya quedó registrada.

1. Con la instancia seleccionada → **Conectar**.
2. Pestaña **Administrador de sesiones** (*Session Manager*) → **Conectar**.

Se abre una terminal en el navegador, con un indicador como este:

```
$
```

Esa terminal entra con un usuario llamado `ssm-user`, no con `ubuntu`. Para trabajar como siempre:

```bash
sudo su - ubuntu
```

Y el indicador cambia al conocido `ubuntu@ip-172-31-40-1:~$`.

> Esto ya es útil por sí solo: si algún día no tenemos nuestro computador o el `.pem` a la mano, podemos administrar la VM desde cualquier navegador iniciando sesión en AWS.

Para cerrar la sesión: `exit` dos veces, o el botón **Terminar**.


## Paso 4: Crear un usuario de IAM para la terminal

Para usar Session Manager desde la terminal de nuestro computador, la terminal necesita **credenciales de AWS**: una *clave de acceso* (*access key*) que demuestre que somos nosotros.

> **No se deben crear claves de acceso para el usuario raíz (root).** Esas claves tendrían control total de la cuenta: quien las obtenga podría crear máquinas, borrar todo o generar cobros. Por eso creamos un usuario aparte que **solo** pueda abrir sesiones en las VM.

1. **IAM** → **Usuarios** → **Crear usuario**.
2. Nombre: `cli-laboratorio`. **No** marcar la opción de acceso a la consola (este usuario solo se usará desde la terminal).
3. **Siguiente** → en permisos, dejar todo como está → **Siguiente** → **Crear usuario**.

Ahora le damos sus permisos:

4. Entrar al usuario `cli-laboratorio` → pestaña **Permisos** → **Agregar permisos** → **Crear política insertada**.
5. Cambiar el editor a **JSON**, borrar lo que aparece y pegar:

```json
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Sid": "AbrirSesiones",
      "Effect": "Allow",
      "Action": "ssm:StartSession",
      "Resource": [
        "arn:aws:ec2:sa-east-1:*:instance/*",
        "arn:aws:ssm:sa-east-1:*:document/AWS-StartSSHSession",
        "arn:aws:ssm:sa-east-1:*:document/SSM-SessionManagerRunShell"
      ]
    },
    {
      "Sid": "CerrarSusPropiasSesiones",
      "Effect": "Allow",
      "Action": ["ssm:TerminateSession", "ssm:ResumeSession"],
      "Resource": "arn:aws:ssm:*:*:session/${aws:username}-*"
    },
    {
      "Sid": "ConsultarEstado",
      "Effect": "Allow",
      "Action": ["ssm:DescribeInstanceInformation", "ssm:DescribeSessions"],
      "Resource": "*"
    }
  ]
}
```

6. **Siguiente** → nombre de la política: `acceso-sesiones-vm` → **Crear política**.

Esta política dice, en resumen: *este usuario puede abrir sesiones en las instancias de São Paulo, cerrar solo sus propias sesiones y consultar qué instancias están disponibles*. Nada más.

Por último, creamos la clave de acceso:

7. Pestaña **Credenciales de seguridad** → sección *Claves de acceso* → **Crear clave de acceso**.
8. Caso de uso: **Interfaz de línea de comandos (CLI)** → marcar la casilla de confirmación → **Siguiente** → **Crear clave de acceso**.
9. Aparecen dos valores: el **ID de clave de acceso** (empieza por `AKIA...`) y la **clave de acceso secreta**. Pulsar **Descargar archivo .csv**.

> ⚠️ La clave secreta **solo se muestra una vez**. Si se pierde, hay que borrar esa clave y crear otra. Igual que el `.pem`, este archivo **no se sube al repositorio, no se envía por chat y no se comparte con nadie**, ni siquiera con los compañeros de grupo: cada quien crea la suya.


## Paso 5: Instalar las herramientas en nuestro computador

Se necesitan dos programas:
- **AWS CLI**: la herramienta de línea de comandos de AWS.
- **Session Manager Plugin**: el complemento que le permite a la AWS CLI abrir las sesiones.

### En macOS (terminal zsh)

Con [Homebrew](https://brew.sh) instalado:

```zsh
brew install awscli
brew install --cask session-manager-plugin
```

### En Windows (PowerShell)

```powershell
winget install --id Amazon.AWSCLI -e
winget install --id Amazon.SessionManagerPlugin -e
```

Al terminar, **cerrar PowerShell y abrir uno nuevo**, para que reconozca los programas recién instalados.

> Si `winget` no está disponible o falla, se pueden descargar los instaladores directamente:
> - AWS CLI: https://awscli.amazonaws.com/AWSCLIV2.msi
> - Session Manager Plugin: https://s3.amazonaws.com/session-manager-downloads/plugin/latest/windows/SessionManagerPluginSetup.exe

### Verificar (igual en los dos sistemas)

```zsh
aws --version
session-manager-plugin
```

El primero muestra algo como `aws-cli/2.x.x ...`. El segundo debe responder:

```
The Session Manager plugin was installed successfully. Use the AWS CLI to start a session.
```


## Paso 6: Configurar las credenciales en la terminal

Este comando es igual en macOS y en Windows:

```zsh
aws configure
```

Pide cuatro datos, que se toman del archivo `.csv` descargado en el paso 4:

| Pregunta | Respuesta |
| --- | --- |
| `AWS Access Key ID` | El ID de clave de acceso (`AKIA...`) |
| `AWS Secret Access Key` | La clave de acceso secreta |
| `Default region name` | `sa-east-1` |
| `Default output format` | `json` |

Las credenciales quedan guardadas en la carpeta personal del usuario (`~/.aws/` en macOS, `C:\Users\<usuario>\.aws\` en Windows), fuera de cualquier repositorio.

Verificamos que AWS nos reconoce:

```zsh
aws sts get-caller-identity
```

La respuesta debe terminar en `user/cli-laboratorio`:

```json
{
    "UserId": "AIDA...",
    "Account": "123456789012",
    "Arn": "arn:aws:iam::123456789012:user/cli-laboratorio"
}
```

Y verificamos que la VM aparece registrada en Systems Manager:

```zsh
aws ssm describe-instance-information --query "InstanceInformationList[].[InstanceId,PingStatus]" --output table
```

Debe aparecer el ID de nuestra instancia con el estado `Online`:

```
-------------------------------------
|  DescribeInstanceInformation      |
+----------------------+------------+
|  i-0d06aec1f7e34b6ba |  Online    |
+----------------------+------------+
```


## Paso 7: Primera sesión desde la terminal

Reemplazar el ID por el de nuestra instancia (paso 2):

```zsh
aws ssm start-session --target i-0d06aec1f7e34b6ba
```

Debe aparecer:

```
Starting session with SessionId: cli-laboratorio-0a1b2c3d4e5f
$
```

Igual que en el navegador, entramos como `ssm-user`. Con esto ya tenemos acceso a la VM desde la terminal y desde cualquier red. Salimos con `exit`.

Pero todavía no es el SSH de siempre: no sirve para copiar archivos con `scp` ni para Visual Studio Code. Eso lo resolvemos en el siguiente paso.


## Paso 8: Usar `ssh` normal a través de Session Manager

El cliente SSH tiene un archivo de configuración, `~/.ssh/config`, donde se le pueden poner apodos a los servidores e indicarle **cómo** llegar a ellos. Vamos a crear el apodo `vm-broker` y decirle que, en lugar de ir directo a la IP, pase por Session Manager.

Primero, movemos el `.pem` a la carpeta `.ssh` para tenerlo siempre en el mismo lugar. Desde la carpeta donde lo tenemos guardado:

### En macOS (terminal zsh)

```zsh
mkdir -p ~/.ssh
mv clave-broker.pem ~/.ssh/
chmod 400 ~/.ssh/clave-broker.pem
```

Ahora agregamos la configuración. **Antes de ejecutarlo**, reemplazar `i-0d06aec1f7e34b6ba` por el ID de nuestra instancia, y `clave-broker.pem` por el nombre de nuestro archivo:

```zsh
cat >> ~/.ssh/config <<'EOF'

Host vm-broker
    HostName i-0d06aec1f7e34b6ba
    User ubuntu
    IdentityFile ~/.ssh/clave-broker.pem
    ProxyCommand sh -c "aws ssm start-session --target %h --document-name AWS-StartSSHSession --parameters 'portNumber=%p' --region sa-east-1"
EOF
chmod 600 ~/.ssh/config
```

### En Windows (PowerShell)

```powershell
New-Item -ItemType Directory -Force "$HOME\.ssh" | Out-Null
Move-Item .\clave-broker.pem "$HOME\.ssh\"
```

El `.pem` conserva los permisos que se le ajustaron con `icacls` en la guía anterior.

Ahora agregamos la configuración. **Antes de ejecutarlo**, reemplazar `i-0d06aec1f7e34b6ba` por el ID de nuestra instancia, y `clave-broker.pem` por el nombre de nuestro archivo:

```powershell
$config = @'

Host vm-broker
    HostName i-0d06aec1f7e34b6ba
    User ubuntu
    IdentityFile ~/.ssh/clave-broker.pem
    ProxyCommand C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe "aws ssm start-session --target %h --document-name AWS-StartSSHSession --parameters portNumber=%p --region sa-east-1"
'@
Add-Content -Path "$HOME\.ssh\config" -Value $config -Encoding ascii
```

> Se usa `-Encoding ascii` a propósito. Si el archivo `config` se guarda en otra codificación (por ejemplo, desde el Bloc de notas o con `Out-File`), SSH no lo entiende y lo ignora sin avisar.

### ¿Qué dice cada línea?

| Línea | Significado |
| --- | --- |
| `Host vm-broker` | El apodo que vamos a escribir en lugar de la IP |
| `HostName i-...` | A dónde llegar: el **ID de la instancia**, no su IP |
| `User ubuntu` | El usuario de Ubuntu, el mismo de siempre |
| `IdentityFile ...` | El `.pem`: SSH lo sigue usando para demostrar quiénes somos ante Ubuntu |
| `ProxyCommand ...` | *Cómo* llegar: en lugar de ir directo, abrir un túnel con Session Manager. `%h` se reemplaza por el `HostName` y `%p` por el puerto 22 |

Fíjense en que el `.pem` **sigue siendo necesario**. Session Manager solo reemplaza el *camino*: AWS revisa nuestras credenciales para dejarnos pasar por el túnel, y luego Ubuntu revisa el `.pem` como siempre. Son dos controles en lugar de uno.


## Paso 9: Conectarse y cerrar el puerto 22

A partir de ahora, la conexión es igual en macOS y en Windows, y desde cualquier red:

```zsh
ssh vm-broker
```

La primera vez pregunta si confiamos en el servidor (`Are you sure you want to continue connecting?`). Responder `yes`.

Debe aparecer la bienvenida de Ubuntu y el indicador `ubuntu@ip-172-31-40-1:~$`, igual que con la conexión directa.

El apodo también sirve para copiar archivos:

```zsh
scp docker-compose.yml vm-broker:~/
```

Y en Visual Studio Code, con la extensión **Remote - SSH**, `vm-broker` aparece en la lista de servidores.

### Cerrar el puerto 22

Con `ssh vm-broker` funcionando, la regla de entrada del 22 ya no se necesita:

1. **EC2** → **Grupos de seguridad** → `broker-uns`.
2. **Editar reglas de entrada**.
3. En la regla de tipo **SSH** (puerto 22) → **Eliminar**.
4. **Guardar reglas**.

Comprobamos las dos cosas:
- `ssh vm-broker` **sigue funcionando**, porque no usa el puerto 22 desde afuera.
- La conexión directa de antes (`ssh -i clave-broker.pem ubuntu@IP_PUBLICA`) **ya no funciona**, y es lo esperado: el puerto está cerrado para todo Internet.

> ⚠️ La regla del **puerto 1883 no se toca**. Los ESP32 y los clientes MQTT se conectan directamente a Mosquitto por ese puerto, y desde redes que no conocemos de antemano. Por eso esa regla debe tener como origen `0.0.0.0/0` (*Anywhere-IPv4*). Lo que protege al broker es el usuario y la contraseña de Mosquitto.


## Problemas frecuentes

| Mensaje | Causa probable | Qué hacer |
| --- | --- | --- |
| En el navegador, el botón **Conectar** de *Administrador de sesiones* está deshabilitado | La VM todavía no se registra en Systems Manager | Esperar 10 minutos. Revisar que el rol `rol-vm-ssm` esté asignado (paso 2). Reiniciar la instancia |
| `TargetNotConnected` | Lo mismo: la VM no está registrada | Igual que la fila anterior. `aws ssm describe-instance-information` debe mostrarla `Online` |
| `SessionManagerPlugin is not found` | El plugin no está instalado, o la terminal se abrió antes de instalarlo | Repetir el paso 5 y abrir una terminal nueva |
| `Unable to locate credentials` | Falta `aws configure` | Repetir el paso 6 |
| `AccessDeniedException ... ssm:StartSession` | La política del usuario no quedó bien, o la región no es `sa-east-1` | Revisar el JSON del paso 4 y la región en `aws configure` |
| `Permission denied (publickey)` | El túnel funcionó, pero Ubuntu no aceptó la clave | Revisar `IdentityFile` (ruta y nombre del `.pem`) y que diga `User ubuntu` |
| `ssh: Could not resolve hostname vm-broker` | SSH no está leyendo el archivo `config` | Revisar que el archivo se llame exactamente `config`, sin `.txt`. En Windows, repetir el paso 8 con `-Encoding ascii` |


## Algunos conceptos y explicaciones

#### 1. Rol de IAM y usuario de IAM

Los dos son formas de dar permisos en AWS, pero a quién se los dan es diferente:
- Un **rol** se le asigna a un **servicio**. Aquí, a la VM, para que pueda registrarse en Systems Manager. No tiene contraseña ni claves: AWS se las entrega a la VM automáticamente.
- Un **usuario** representa a una **persona o programa** que se conecta desde afuera. Aquí, a nuestra terminal. Se identifica con su clave de acceso.

#### 2. Mínimo privilegio

El usuario `cli-laboratorio` solo puede abrir sesiones en las VM, y nada más. Si alguien llegara a obtener su clave de acceso, no podría crear máquinas, ver la facturación ni borrar nada, y además tendría que tener también nuestro `.pem` para entrar a Ubuntu. Darle a cada credencial solo los permisos que necesita es lo que se conoce como **principio de mínimo privilegio**.

#### 3. ¿Por qué ya no importa la IP pública de la VM?

En `~/.ssh/config`, el `HostName` es el **ID de la instancia**, no su IP. Session Manager encuentra la VM por ese ID. Así que, aunque la VM se detenga y al volver a encenderse reciba otra IP pública, `ssh vm-broker` sigue funcionando sin cambiar nada.

Esto no aplica para los ESP32: ellos sí se conectan a Mosquitto por la IP pública. Para que esa IP no cambie, AWS ofrece las **direcciones IP elásticas**, que quedan fijas aunque la instancia se detenga.
