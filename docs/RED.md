# Red en Hispa-DOS

Primer paso de la red (14/11/2026): un controlador residente con la interfaz
de controlador de paquetes («packet driver», `HSPRED`), una pila TCP/IP propia y tres órdenes:
`CONFRED` (`IPCONFIG`), `PING` y `DESCARGAR`. El objetivo es llegar a lo que
tenía Windows para Trabajo en Grupo 3.11 con TCP/IP: ver «Pendiente».

```
A:\>HSPRED
HSPRED 0.1: controlador de red de Hispa-DOS
Tarjeta Realtek RTL8139, MAC 52:54:00:12:34:56, IRQ 11, controlador de paquetes en INT 60h

A:\>CONFRED

Configuración de la red

  Tarjeta . . . . . . . : Realtek RTL8139 (INT 60h)
  Dirección MAC . . . . : 52:54:00:12:34:56
  Dirección IP  . . . . : 10.0.2.15
  Máscara de subred . . : 255.255.255.0
  Puerta de enlace  . . : 10.0.2.2
  Servidor DNS  . . . . : 10.0.2.3
  Obtenida  . . . . . . : por DHCP (servidor 10.0.2.2, concesión de 86.400 s)
  Fecha . . . . . . . . : 14/11/2026

A:\>PING 10.0.2.2

Haciendo ping a 10.0.2.2 con 32 octetos de datos:
Respuesta desde 10.0.2.2: octetos=32 tiempo<55ms TTL=255
...

A:\>DESCARGAR http://10.0.2.2:8080/PRUEBA.BIN
Servidor 10.0.2.2: 10.0.2.2:8080
Guardando en PRUEBA.BIN:
[█████████████████████████] 100%     150.000 de 150.000 octetos
Guardado en PRUEBA.BIN (150.000 octetos).
```

## Uso

1. Cargar el controlador, a mano o en `AUTOEXEC.BAT`: `HSPRED` (o
   `LH HSPRED` con memoria superior). Con un adaptador USB-Ethernet, además,
   `HSPUSB.SYS` en `CONFIG.SYS` (ver «Adaptadores USB-Ethernet»). Desde la 0.9.24 el `AUTOEXEC.BAT` que
   se instala ya no lo carga al arrancar (trae la línea `HSPRED /AUTO` con un
   `REM` delante): ocupa 13 KB de memoria convencional, que les faltan a los
   juegos y a los programas grandes. `ACTUALIZAR` lo carga solo; para tener
   la red siempre, quite ese `REM`. Si no encuentra una tarjeta que sepa
   usar, lo dice, enseña las tarjetas de red que hay (fabricante:modelo) y no
   se queda en la memoria.
2. `CONFRED` (también `IPCONFIG`): pide la dirección por DHCP (o la pone de
   `RED.INI`) y la enseña. No hace falta llamarlo antes de las otras: la
   primera orden de red que se use la pide.
3. `PING nombre|dirección [/N:n] [/T]` y
   `DESCARGAR [http://]servidor[:puerto]/ruta [archivo]`.

| Orden | Para qué |
|---|---|
| `HSPRED [/INT:xx] [/INFO] [/AUTO] [/USB] [/DEPURA[:archivo]] [/U]` | El controlador. `/AUTO`: para el `AUTOEXEC.BAT`: sin tarjeta sale en silencio y, con una tarjeta Wi-Fi, elige entre el cable y el Wi-Fi (ver «Cable o Wi-Fi»). `/USB`: usar el adaptador USB-Ethernet (el que lleva HSPUSB) aunque haya una tarjeta PCI que valga. `/INFO`: tarjeta, PCI (con la revisión), puertos, el chip exacto (familia de la RTL8168 por su `TxConfig`, número de pieza de la PCnet), IRQ, MAC, enlace y velocidad, vector, memoria, programas que lo usan, contadores y la IP (también con el controlador ya cargado). `/INT:xx`: el vector (60h-80h; si no, el primero libre desde el 60h, nunca el 67h de EMS). `/U`: descargarlo (si ningún programa lo usa). `/DEPURA[:archivo]`: todo lo que HSPRED escribe en la pantalla se añade además a un archivo (`HSPRED.LOG` si no se dice otro), línea a línea (si el equipo se cuelga, queda hasta donde llegó), y la Yukon cuenta su puesta en marcha paso a paso; con `/INFO`, es el informe para mandar a quien depura. Solo para la Marvell Yukon: `/MEMORIA` y `/PUERTOS` (por dónde van sus registros, sin probar cuál contesta) y `/SONDEO` (sin IRQ: mira la tarjeta en cada tic del reloj). |
| `CONFRED [/RENOVAR] [/LIBERAR]` | La configuración IP. `/RENOVAR` (`/RENEW`) la pide otra vez; `/LIBERAR` (`/RELEASE`) la olvida. `/ALL` se acepta (como en Windows) y hace lo mismo que sin nada. |
| `PING destino [/N:n] [/T]`, `PING /P` | `/P`: a la puerta de enlace (el router) que haya dado `CONFRED`, sin tener que escribir su dirección. Eco de ICMP: 4 veces (o *n*, o hasta Esc con `/T`), con el tiempo (de 55 en 55 ms: el reloj de la BIOS) y el TTL, y el resumen de paquetes perdidos. Código de salida 1 si no contestó ninguno. |
| `DESCARGAR url [archivo]` | HTTP/1.0 sin cifrar. Barra de progreso si el servidor dice el tamaño (si no, los octetos que van llegando). Sigue hasta 4 redirecciones a `http://`. Si no se dice el archivo, el último trozo de la ruta en 8.3 (`INDEX.HTM` si no hay). Si algo falla (404, disco lleno, Esc, el servidor corta), lo dice y borra el archivo a medias. |
| `CONSOLA [/PUERTO:n] [/CADUCA:s]` | Consola remota por red (servidor TCP, puerto 2323 por omisión): deja entrar a otra persona a este equipo, con un código de acceso de 6 cifras que solo se ve aquí (en pantalla desde el principio, de un solo uso, cambia tras 3 fallos y caduca a los 10 minutos). Abierta y controlada por el usuario. Ver [docs/CONSOLA.md](CONSOLA.md). |
| `ACTUALIZAR [url]` | Instalador por red. Mira por HTTP (`versiones.txt`) si hay una versión más nueva o una edición más rica que la instalada (`lib/version.inc`), según la CPU/RAM/disco del equipo; y, si el usuario quiere, **baja el paquete `.PAQ` y lo instala en el sitio** conservando `*.INI`/`*.REC`. La dirección sale, por orden, de lo que se escriba, de `ACTUALIZAR=` en `RED.INI`, o de la de por omisión. Ver más abajo. |

**HTTPS, en `DESCARGAR`, no**: las direcciones `https://` (cifradas, hoy casi
todas) se rechazan con un mensaje claro, y también una redirección a
`https://`. `DESCARGAR` sirve para servidores de la red de casa (otro PC con
`python3 -m http.server`, un NAS) y para los sitios de archivos antiguos que
siguen dando HTTP. **Las cifradas las baja `HTTPS`** (`HTTPS.EXE`, un programa
Hispa32 del disquete «Internet» de Hispa-486: necesita HSPDPMI): ver «La red
de los programas Hispa32», más abajo; `DESCARGAR` lo dice en su mensaje.

`AYUDA` las lista y `AYUDA CONFRED` (o `CONFRED /?`), `AYUDA PING`,
`AYUDA DESCARGAR` y `AYUDA ACTUALIZAR` las explican.

## ACTUALIZAR: instalador por red del sistema

`ACTUALIZAR` mira por la red si hay una versión más nueva **o una edición más
rica** de Hispa-DOS que la instalada y, si el equipo la aguanta y el usuario
quiere, la **descarga y la instala en el sitio**, conservando los datos y
ajustes del usuario (`*.INI`, `*.REC`) y sus carpetas. La versión del sistema
está en un solo sitio, `lib/version.inc` (`VER_STR` para enseñarla, `VER_NUM`
para comparar y `VER_EDI`, el nivel de riqueza de la edición instalada).

### Qué hace, paso a paso

1. Detecta el equipo: nivel de CPU (386/486/586), KiB de memoria extendida y
   MiB libres del disco de destino (rutinas sacadas de `EQUIPO`, en
   `lib/equipodet.inc`).
2. Baja del servidor el archivo `versiones.txt` (ver formato abajo).
3. De las versiones del archivo, se queda con las que **mejoran** lo instalado
   (más nuevas, o de una edición más rica) y que el equipo **cumple**
   (`cpu ≤ detectada`, `ram ≤ detectada`, `disco ≤ libre`). La **recomendada**
   es la de mayor edición/versión de ésas. La recomendación no es solo «no te
   pases para tu hardware»: también «tu máquina da para más de lo que tienes»
   (p. ej. aconsejar a un 486 con solo texto pasarse a la edición con Hispa
   Ventanas).
4. Enseña el equipo y el veredicto:
   - al día, si no hay nada mejor;
   - si hay algo mejor pero el equipo no llega, dice hasta qué versión/edición
     llega y por qué la siguiente no (sus requisitos);
   - si hay una recomendada instalable, la enseña con su nota y pregunta
     `¿Descargar e instalar la X.Y? (S/N)`.
5. Si se dice que **sí**: baja el `paquete=` a `C:\ACTUALIZA.PAQ`, lo
   desempaqueta **del todo** en `C:\ACTUALIZA\` y, **solo si todo está bien**,
   lo aplica en el sitio (reusa el motor de copia de `INSTALAR`,
   `lib/copiaarbol.inc`): sobrescribe los ficheros del sistema, conserva
   `*.INI`/`*.REC`, y guarda el `CONFIG.SYS`/`AUTOEXEC.BAT` del usuario como
   `.ANT`. Nunca formatea. Al acabar borra el temporal y pide reiniciar.

**Seguridad**: si falla la descarga o el desempaquetado, aborta sin tocar nada
del sistema (primero baja y desempaqueta entero en el temporal; solo cuando
está completo y la suma de control cuadra, aplica).

### Formato de versiones.txt

Texto plano (se saltan las líneas en blanco y las que empiezan por `;` o `#`),
una sección por versión/edición, con `clave=valor`:

```
[0.9.16]
cpu=386            ; nivel mínimo: 386 / 486 / 586 (Pentium)
ram=0              ; KiB mínimos de memoria extendida (0 = no exige)
disco=4            ; MiB libres mínimos en el disco de destino
nivel=0            ; "riqueza" de la edición (mayor = más completa)
edicion=Hispa-386 (con Hispa Ventanas)
gama=386           ; la gama: 386 (por omisión) o 486 (Hispa-486)
paquete=http://actualiza.clicgest.es/hispados-0.9.16.paq
nota=Novedades de esta versión
```

El encabezado `[mayor.menor.parche]` es la versión. Todas las claves son
opcionales: sin `cpu`/`ram`/`disco` no hay requisito; `nivel` por omisión 0;
`edicion` y `nota` solo se enseñan; `paquete` es la URL del `.PAQ` (sin ella la
versión solo se **avisa**, no se instala). Puede haber varias secciones (varias
ediciones). **Compatibilidad**: si el archivo trae el formato viejo (solo
`version=X`, con `descarga=`/`nota=` opcionales), se trata como una única
versión sin requisitos y sin paquete (solo avisa, como antes).

**Gamas** ([HISPA-486], 18/11/2026): cada instalación se actualiza en su
gama. La de la instalada es la clave `Gama=` de `\HISPADOS\EDICION.INI`
(sección `[Edicion]`; la pone la imagen de Hispa-486 con `Gama=486`; sin el
archivo o sin la clave, 386) y ACTUALIZAR la enseña («Tienes instalado
Hispa-DOS 0.9.25 (Hispa-486).»). Una sección sin `gama=` es de la 386: el
`versiones.txt` de siempre sigue valiendo para Hispa-386 y nunca hace bajar a
un Hispa-486 a Hispa-386. Las secciones de otra gama **no se miran** (ni se
ofrecen, ni se avisan, ni cuentan para «hasta dónde llega el equipo»); dentro
de la gama, todo sigue igual (`nivel`, requisitos). Se pasa de Hispa-386 a
Hispa-486 con la imagen de Hispa-486 e INSTALAR, no con ACTUALIZAR. Más, en
docs/486/HISPA-486.md, «Las actualizaciones por la red». Ejemplo, las dos
gamas de la misma versión:

```
[0.9.26]
cpu=386
edicion=Hispa-386 (con Hispa Ventanas)
paquete=http://actualiza.clicgest.es/hispados-0.9.26.paq

[0.9.26]
gama=486
cpu=386
edicion=Hispa-486
paquete=http://actualiza.clicgest.es/hispa486-0.9.26.paq
```

Para subir una versión en el servidor: cambia `lib/version.inc` y
`actualiza.clicgest.es/versiones.txt`, y sube el `.PAQ` generado con
`tools/paquete.py` (`make paq` hace `build/hispados.paq` de la imagen USB de
Hispa-386 y `make paq486`, `build/hispa486.paq` de la de Hispa-486).

### Formato del paquete .PAQ

Lo genera `tools/paquete.py` a partir de una carpeta con la **misma estructura**
que tendrá el sistema (`KERNEL.SYS`, `COMMAND.COM`, `CONFIG.SYS`,
`AUTOEXEC.BAT` en la raíz y `HISPADOS\...` con sus subcarpetas; en el de
Hispa-486, también `HISPA32\...`). Sin compresión. ACTUALIZAR aplica del
paquete `HISPADOS` y sus subcarpetas `JUEGOS`, `JUEGOS\NES`, `JUEGOS\SMS` y
`PROGS`, `HISPA32` (si la trae), y en la raíz `KERNEL.SYS`, `COMMAND.COM`,
`LEEME.TXT`, `CONFIG.SYS` y `AUTOEXEC.BAT`; otra carpeta del paquete no se
copia (hay que añadirla en `apply_update`).
Todos los enteros en *little-endian*:

```
Cabecera (12 octetos):
   0  magia   4 octetos  = "PAQ1"
   4  nfich   uint32     número de ficheros
   8  suma    uint32     suma de TODOS los octetos de datos, módulo 2^32
Por cada fichero, en orden:
   uint16   long_nombre   longitud del nombre (1..80)
   bytes    nombre        ruta relativa, separador '\' (sin cero final)
   uint32   long_datos    longitud del contenido
   bytes    datos         el contenido del fichero
```

`ACTUALIZAR` comprueba la magia, el número de ficheros y que la suma de control
cuadra antes de aplicar nada. Uso:
`python3 -I tools/paquete.py <carpeta> <salida.paq>`.

### La dirección y los límites

La dirección de `versiones.txt` sale, por este orden: (1) la que se escriba en
la línea de órdenes, (2) la clave `ACTUALIZAR=` de `RED.INI`, (3) una de por
omisión compilada: `http://actualiza.clicgest.es/versiones.txt` (servidor
propio por HTTP).

**Importante**: como `ACTUALIZAR` (de 16 bits) no habla HTTPS y **GitHub es
todo HTTPS**, el archivo y los paquetes no se pueden leer de GitHub: tienen que estar en un
servidor propio por HTTP. Las direcciones `https://` se rechazan con un mensaje
claro. Si falta la red (sin `HSPRED` o sin IP), lo dice y no hace nada.

En el disco el programa es `ACTUALIZ.COM` (los nombres de DOS tienen 8
letras); al escribir `ACTUALIZAR` el intérprete lo recorta a `ACTUALIZ`, así
que no hace falta alias.

**Riesgo conocido** (aplicar en el sitio): se sobrescribe el sistema vivo. Si
el `.PAQ` trae `COMMAND.COM` o `KERNEL.SYS` distintos del que corre, conviene
reiniciar enseguida (DOS recarga la parte transitoria de `COMMAND.COM` del
disco al volver al intérprete). Por eso el mensaje final pide reiniciar.

## Tarjetas

| Tarjeta | PCI | Estado |
|---|---|---|
| NE2000 PCI (Realtek RTL8029 y compatibles: Winbond 89C940, Compex, VIA 86C926...) | `10EC:8029`... | ✅ probada en QEMU (`-device ne2k_pci`). Datos por el «DMA remoto» de la tarjeta: sin búferes en el PC, la que menos ocupa. Sin estado del enlace. |
| Realtek RTL8139 (y D-Link DFE-538TX, Accton) | `10EC:8139` | ✅ probada en QEMU (`-device rtl8139`). Anillo de recepción de 8 KB. |
| Intel PRO/1000 de la familia 8254x («e1000»: 82540EM, 82545EM, 82541...) | `8086:100E`... | ✅ probada en QEMU (`-device e1000`) **y en VirtualBox** («Intel PRO/1000 MT Desktop (82540EM)»: confirmada por el responsable del proyecto el 16/11/2026). Registros por los puertos (IOADDR/IODATA) si la tarjeta los tiene (las de verdad y la de VirtualBox); en QEMU, que solo los da en memoria, por el modo «irreal» (no funciona en modo V86: con HSPEMM lo dice y no se carga). Hasta la 0.9.23 HSPRED no ponía la base de su GDT para ese modo: en QEMU sin KVM colaba, pero con KVM el equipo se colgaba y arrancando por UEFI (HispaBIOS) se reiniciaba una y otra vez al cargar HSPRED; corregido, con sus pruebas (`tests/run_red_tests.sh`, `tests/run_hispabios_tests.sh`). |
| Intel 82574L («e1000e») | `8086:10D3` | ✅ probada en QEMU (`-device e1000e`). Misma rutina que la e1000 (descriptores clásicos, interrupción INTx); en QEMU tiene los registros por puertos, así que va también con HSPEMM. Sin probar en VMware ni en una de verdad. |
| Realtek RTL8111/8168, 8101E/8102E/8105E (8136), 8169 («r8169») | `10EC:8168`, `10EC:8136`... | ✅ **funciona en un equipo de verdad**: Toshiba NB250-10N (`10EC:8136`, revisión PCI 05h), desde el 15/11/2026 (red y consola remota). QEMU no la tiene. En otras revisiones (8111E/F/G/H, la 8111EP del Lenovo A275), sin probar: ver «RTL8168: lo que falta para las revisiones modernas». |
| AMD PCnet-PCI II (Am79C970A) y PCnet-FAST III (Am79C973) | `1022:2000` | ✅ probada en QEMU (`-device pcnet`). ⚠️ Es la tarjeta por omisión de **VirtualBox** y la «vlance» de **VMware**, pero en ellos **no se ha probado** (aquí solo hay QEMU). |
| Marvell Yukon 2 «Optima» 88E8059 (y «Optima 2», 88E8079) | `11AB:4381`, `11AB:4382` | ⚠️ **Hecha y sin probar en un equipo de verdad** (18/11/2026): es la de cable del HP Mini 5103. QEMU no la emula; se prueba contra un modelo del chip (`tests/run_yukon_tests.sh`). Ver «Marvell Yukon 88E8059 (HP Mini 5103)». |
| Adaptador **USB**-Ethernet con la clase estándar CDC-ECM (Realtek RTL8152/RTL8153 y otros) | USB `0BDA:8153`... | ✅ probada en QEMU (`-device usb-net`), con las cuatro clases de controladora USB y detrás de un concentrador. ⚠️ En un adaptador de verdad, **sin probar**. La lleva `HSPUSB.SYS`: ver «Adaptadores USB-Ethernet (CDC-ECM)». |
| DEC 21140/21140A y 21143 «Tulip» | `1011:0009`, `1011:0019` | ✅ probada en QEMU (`-device tulip`, que se presenta como 21143). ⚠️ La 21140A es el «Adaptador de red heredado» de **Hyper-V**, pero en Hyper-V **no se ha probado**. En tarjetas de verdad, sin probar y sin elección del medio (ver «Límites de las tarjetas nuevas»). |

No se hacen (paravirtualizadas, sin nada que ver con las anteriores y muy
largas): **vmxnet3** de VMware, la tarjeta sintética de Hyper-V (**netvsc**
sobre VMBus, la de las máquinas de generación 2) y **virtio-net**. Tampoco
las DEC 21040/21041 (`1011:0002`, `1011:0014`): no tienen SROM con el mismo
formato ni puerto MII, y ningún programa de máquinas virtuales las ofrece.

### En una máquina virtual

Qué tarjeta elegir en cada programa para que Hispa-386 tenga red:

| Programa | Qué elegir | Estado |
|---|---|---|
| **VirtualBox** | Configuración › Red › Adaptador 1 › Avanzadas › Tipo de adaptador: **PCnet-FAST III (Am79C973)** (la de por omisión con el tipo de invitado «DOS» u «Otro») o **Intel PRO/1000 MT Desktop (82540EM)**. «Conectado a»: **NAT** (el invitado recibe 10.0.2.15 por DHCP, puerta 10.0.2.2, DNS 10.0.2.3) o **Adaptador puente** (la dirección la da el router de casa). | Intel PRO/1000 MT Desktop: confirmada (16/11/2026). PCnet: solo en QEMU. |
| **Hyper-V** | Máquina de **generación 1** y, en su configuración, Agregar hardware › **Adaptador de red heredado** (Legacy Network Adapter), conectado a un conmutador virtual (el «Default Switch» da DHCP). El «Adaptador de red» normal (sintético) no sirve; conviene quitarlo. | Solo en QEMU (`-device tulip`). |
| **VMware** (Workstation, Player, ESXi) | En el `.vmx`: `ethernet0.virtualDev = "vlance"` (PCnet) o `"e1000"` (82545EM); `"e1000e"` (82574L) también debería ir. `"vmxnet3"` no. | Sin probar. |
| **QEMU** | Cualquiera: `-device ne2k_pci`, `rtl8139`, `e1000`, `e1000e`, `pcnet` o `tulip`, con `-netdev user,id=n0` (y `,netdev=n0` en la tarjeta). | Todas probadas (`tests/run_red_tests.sh`). |

#### Probar en VirtualBox, paso a paso

1. Máquina nueva: tipo «Other», versión «DOS» (u «Other/Unknown»), 32 MB de
   memoria bastan. En Almacenamiento, un controlador de disquete con
   `hispados.img` (o el disco duro/USB de la versión instalable).
2. Configuración › Red › Adaptador 1: habilitado, «Conectado a: NAT»;
   Avanzadas › Tipo de adaptador: **PCnet-FAST III (Am79C973)**.
3. Arrancar y, en el símbolo del sistema: `HSPRED /INFO`. Tiene que decir
   `Tarjeta: AMD PCnet-FAST III (Am79C973)`, `PCI: ... 1022:2000`,
   `Chip (CSR88/89): 2625h`, la MAC que enseña VirtualBox en esa misma
   pantalla (08:00:27:...) y `Enlace: sí`.
4. `CONFRED`: con NAT, `Dirección IP: 10.0.2.15`, puerta `10.0.2.2`.
5. `PING 10.0.2.2` (con NAT contesta la puerta; con adaptador puente, la
   dirección del router) y, si hay un servidor HTTP a mano,
   `DESCARGAR http://.../archivo`.
6. Repetir con «PCnet-PCI II (Am79C970A)» (`Chip: 2621h`) si se quiere.

Si falla, lo que hay que apuntar:

| Qué sale | Qué significa |
|---|---|
| «No se ha encontrado ninguna tarjeta...» y una línea con `fabricante:modelo` | La tarjeta elegida no es de las admitidas (apuntar ese par). |
| «La tarjeta PCnet no contesta por sus puertos (modo de 32 bits, DWIO)» | Tras el reinicio y el paso a 32 bits, el CSR0 no vale 0004h: esa emulación no admite DWIO. Habría que añadir el modo de 16 bits (WIO), que es el que usa Linux. |
| «La tarjeta PCnet no termina su inicialización (INIT sin IDON)» | La tarjeta no lee el bloque de inicialización (DMA) o no pone IDON. |
| Se carga, pero `CONFRED` dice que no hay respuesta del DHCP | Mirar `HSPRED /INFO` después: si «enviados» sube y «recibidos» se queda en 0, no llegan tramas (interrupción o anillo de recepción); si «errores» sube, el envío falla. Apuntar la línea «Paquetes» entera y el IRQ. |
| `Enlace: no` | El cable virtual está desconectado (casilla «Cable conectado»). |

#### Probar en Hyper-V, paso a paso

1. Administrador de Hyper-V › Nuevo › Máquina virtual: **Generación 1**,
   32 MB o más de memoria (sin memoria dinámica), sin disco o con el disco
   de la versión instalable.
2. Configuración de la máquina › Agregar hardware › **Adaptador de red
   heredado** › Agregar; en «Conmutador virtual», elegir **Default Switch**
   (da DHCP y salida a Internet por NAT) o un conmutador externo. Quitar el
   «Adaptador de red» normal que pone el asistente.
3. Unidad de disquete: Hyper-V quiere un archivo `.vfd`; sirve una copia de
   `hispados.img` con el nombre cambiado a `hispados.vfd` (es la misma
   imagen de 1,44 MB).
4. Arrancar y escribir `HSPRED /INFO`. Tiene que decir
   `Tarjeta: DEC 21140 (Tulip)`, `PCI: ... 1011:0009` (la 21140A suele dar
   la revisión 20h), la MAC que enseña Hyper-V en Adaptador de red heredado ›
   Características avanzadas (las de Hyper-V empiezan por `00:15:5D`) y
   `Enlace: no se sabe` (es lo normal con esta tarjeta).
5. `CONFRED`: con el Default Switch da una dirección privada (normalmente
   `172.x.x.x`) y su puerta de enlace.
6. `PING` a la puerta de enlace que haya dicho `CONFRED`.

Si falla:

| Qué sale | Qué significa |
|---|---|
| «No se ha encontrado ninguna tarjeta...» sin ninguna línea de tarjeta | La máquina es de generación 2 o no tiene el adaptador heredado. |
| «La tarjeta Tulip no da su dirección MAC (SROM)» | La lectura de la SROM por el CSR9 no da una dirección válida ni en el octeto 20 ni en el 0. Es lo primero que puede ser distinto en Hyper-V. |
| La MAC no es la que enseña Hyper-V | La SROM tiene otro formato: apuntar la MAC que sale. |
| Se carga, pero el DHCP no contesta | `HSPRED /INFO`: si «enviados» sube y «recibidos» es 0, o no se envía de verdad o el filtro no deja pasar nada (la trama de configuración); si «errores» sube, los descriptores de recepción llegan sin «primero/último». Apuntar la línea «Paquetes» y el IRQ. |

Lo que HSPRED hace con la Tulip pensando en la emulación de Microsoft (en
los comentarios de `drivers/hspred.asm`, con el porqué): cada trama en un
solo descriptor con «primero» y «último»; anillos cerrados con el bit de fin
de anillo y, además, encadenados por el búfer 2 (como Linux, que es lo que se
sabe que funciona allí); la trama de configuración (filtro perfecto: 15 veces
la dirección de difusión y la de la tarjeta) enviada y esperada **antes** de
arrancar la recepción, y si la tarjeta no la acepta, modo promiscuo; la MAC,
de la SROM serie (con el número de bits de dirección detectado, 6 u 8, y
buscándola en el octeto 20 o en el 0, igual que Linux); puerto MII elegido en
el CSR6 (la 21140 no tiene otro).

#### Límites de las tarjetas nuevas

- **PCnet y Tulip solo se han probado contra la emulación de QEMU** (8.2).
  VirtualBox, VMware y Hyper-V emulan las mismas tarjetas con su propio
  código: lo que se ha hecho pensando en ellos sale de la documentación de
  las tarjetas y de lo que hace Linux, no de haberlo visto funcionar.
- **PCnet**: la emulación de QEMU, cuando el descriptor de recepción que le
  toca no está libre, salta a cualquier otro que lo esté (una tarjeta de
  verdad no lo hace); por si eso pasa con el anillo lleno, HSPRED mira el
  anillo entero cada vez (precaución sacada de leer el código de QEMU; no se
  ha visto fallar). Solo el modo de 32 bits (DWIO); sin multidifusión (filtro a
  cero); la velocidad no se dice (el enlace sí, por el LED 0, BCR4); el
  medio lo elige la tarjeta (ASEL). En una PCnet de verdad, sin probar.
- **Tulip**: el estado del enlace no se sabe (`Enlace: no se sabe`); sin
  multidifusión. En una tarjeta de verdad faltaría elegir el medio según la
  SROM (10BASE-T por el SIA de la 21143, o el PHY por MII) y el dúplex: tal
  como está, deja puesto el puerto MII/SYM, que es lo que quieren las
  emuladas. Las 21040/21041 no están.
- **82574L**: probada solo en QEMU; las Intel integradas en la placa (ICH8
  en adelante, I217/I218/I219) son otra cosa y no están.

### RTL8168: lo que falta para las revisiones modernas

La puesta en marcha de HSPRED (`r68_init`, `r68_start`) está comprobada en
la RTL810xE del Toshiba NB250 y **no se ha cambiado**. Repasada contra el
controlador `r8169` de Linux (6.6), esto es lo que hace distinto y puede
hacer falta en las 8111E-VL/F/G/H/EP; `HSPRED /INFO` enseña la revisión PCI,
el `TxConfig` entero, el XID (bits 20-31 sin el 4 y el 5: lo que mira Linux)
y la familia, y el estado del enlace, para saber cuál es antes de tocar nada:

| Qué | HSPRED | r8169 | Para quién importa |
|---|---|---|---|
| `Cfg9346` (50h) | desbloquea (C0h) y vuelve a bloquear | igual | — |
| `MaxTxPacketSize` (ECh) | 3Fh siempre | 3Fh hasta la 8168E; **27h** («EarlySize») de la 8168E-VL en adelante | 8111E-VL/F/G/H |
| `TxConfig` (40h) | 03000700h | lo mismo, más el bit 7 (`TXCFG_AUTO_FIFO`) de la 8168E-VL en adelante | 8111E-VL/F/G/H: sin él el envío puede no salir |
| `RxConfig` (44h) | 0000E70Eh (umbral de FIFO «sin umbral», bits 13-15: lo de la 8169/8101 antigua) | 8168C a F: bits 15 y 14 (`RX128_INT_EN`, `RX_MULTI_EN`) sin el 13; 8168G en adelante, además el bit 11 (`RX_EARLY_OFF`) | 8111G/H/EP |
| `Config2` (53h) bit 7 y `Config5` (56h) bit 0 (CLKREQ y ASPM) | no los toca (quedan como los dejó la BIOS) | los quita antes de configurar (8168E en adelante) | portátiles con ASPM activado: cortes o latencia |
| `CPlusCmd` (E0h) | no lo escribe | lo escribe (suma de comprobación, VLAN; en la 8169 antigua, más bits) | 8169/8110 PCI |
| `IntrMitigate` (E2h) | no lo escribe | a cero | si la BIOS lo dejó puesto, interrupciones con retraso |
| `MISC` (F0h) bit 19 (`RXDV_GATED_EN`) y `MCU` (D3h) bit 7 (`NOW_IS_OOB`) | no los toca | 8168G en adelante: cierra la puerta de recepción al iniciar y la **abre** al arrancar; quita `NOW_IS_OOB` | 8111G/H/EP: si quedó cerrada, no se recibe nada |
| Arranque del PHY y del PLL (`PMCH`, 6Fh; BMCR del PHY) | nada (se fía del encendido) | lo enciende y relanza la negociación | si otro sistema lo dejó apagado: `Enlace: no` |
| 8111EP (DASH) | nada | avisa al firmware (`rtl8168ep_driver_start`, registros OCP) | Lenovo A275 |
| `RxMaxSize` (DAh) | 1536 | 16383 (sin filtro) | — (las tramas normales caben) |

Ninguno se ha añadido: no se pueden probar aquí y el código de ahora es el
que funciona en el Toshiba. El orden razonable, cuando haya un equipo con
una 8111 moderna: (1) `TXCFG_AUTO_FIFO` y `MaxTxPacketSize` según el XID,
(2) `RxConfig` según la familia, (3) quitar ASPM/CLKREQ, (4) `RXDV_GATED_EN`
en la 8168G en adelante.

### Marvell Yukon 88E8059 (HP Mini 5103)

La tarjeta de cable del HP Mini 5103 es una **Marvell Yukon 2 «Optima»
88E8059** (`11AB:4381`, revisión PCI 11h, subsistema `103C:1608`), Gigabit,
por PCI Express. Marvell no publicó la documentación de la familia: lo que
se sabe sale de leer el controlador `sky2` de Linux (`sky2.c` y `sky2.h`,
GPL-2.0, que valen para unos veinte chips distintos); el código de HSPRED es
propio y solo lleva el camino de la Optima (identificador de chip BCh; la
Optima 2, BEh, sigue el mismo camino en `sky2` y se admite; las demás de la
familia —EC, EC Ultra, Extreme, FE, Supreme, Ultra 2, Prime— no: tienen
otros pasos de PHY, búfer RAM o sumas de comprobación, y HSPRED lo dice).

**Estado (18/11/2026): hecha, sin probar en el equipo de verdad.** QEMU no
la emula, así que se ha probado contra un simulador propio del chip (ver
«Pruebas»), que caza los fallos del ensamblador pero no demuestra que el
chip se porte como se cree. La prueba de verdad es la memoria USB de
`tools/prueba-cable` (más abajo).

Cómo está hecha (sección `.yukon` de `drivers/hspred.asm`, de quita y pon:
1.516 octetos residentes solo si la tarjeta es ésta):

- **Registros** (16 KB, BAR0, de 64 bits, en memoria): por **memoria** con
  el modo «irreal», como la e1000 (es lo que hace Linux, lo más seguro), o
  por los **puertos** (la BAR de E/S, 256 puertos: los 128 primeros son el
  bloque 0 y los otros 128 enseñan el bloque de 128 octetos que diga el
  registro RAP, el puerto 0; `sky2` no usa este camino: es el del
  controlador antiguo de Marvell, `sk98lin`, tal como se recuerda).
  Al cargar se prueban los dos leyendo el identificador del chip
  (`B2_CHIP_ID`, 11Bh): si por memoria sale un chip Yukon, por memoria; si
  no (o en modo V86, donde no se llega a la memoria de la tarjeta), por los
  puertos. `/MEMORIA` y `/PUERTOS` lo fuerzan. **Sin probar en el chip de
  verdad si los puertos con RAP funcionan**: `HSPRED /DEPURA` dice lo que se
  lee por cada camino, y de eso depende que vaya con HSPEMM.
- **Reinicio y encendido** (`yk_init`, se descarta): ASF apagado, reinicio
  por programa, borrado de errores PCI y AER, alimentación por VCC, relojes
  (`PCI_DEV_REG3` a cero, sin división), el «control de energía avanzado»
  (`PCI_DEV_REG4/5`, `PCI_CFG_REG_1`, la errata 4.107 de la carrera del
  estado en `B2_GP_IO`) y lo propio de la Optima: el temporizador de
  detección de enlace del PHY (`PSM_CONFIG_REG4`: 0,4 s; 1,1 s en la
  revisión 0, que además necesita que el PHY de PCI Express no se apague y
  un arreglo de la terminación del PHY de cobre), devolver el control del
  enlace PCI Express a como lo dejó la gestión de energía, y el registro de
  depuración 8 del PHY de PCI Express. La configuración PCI se escribe por
  la ventana 1C00h de los registros (con el permiso de `B2_TST_CTRL1`).
- **MAC**: la da el chip (`B2_MAC_1`, 100h) al salir del reinicio.
- **PHY** (integrado, por SMI a través del MAC): encendido
  (`PCI_DEV_REG1`, bit 26), reloj de 25 MHz, cruce de pares automático,
  autonegociación de 10/100/1000 en dúplex completo y medio, **sin control
  de flujo** (no se anuncia la pausa). La velocidad y el dúplex los copia el
  MAC del PHY él solo; cuando el PHY dice «enlace, y velocidad resuelta»
  (registro 17), HSPRED pone el hueco entre tramas de esa velocidad y
  arranca el envío y la recepción del MAC; si el enlace cae, los para. Al
  cargar se espera al enlace **hasta 5 segundos** (la autonegociación tarda
  de 2 a 4): así el primer `CONFRED` ya puede enviar. Sin cable, la carga
  tarda esos 5 segundos.
- **Anillos** de «elementos de lista» de 8 octetos (dirección, longitud,
  control y código; el bit 7 del código dice que el elemento vale), que
  aquí empiezan cada uno en una página (4 KB: Linux se los da a la tarjeta
  así, y no se sabe si con menos le vale): 16 de recepción (4 búferes de 1.520 octetos que van dando
  la vuelta), 16 de envío (un búfer: cada envío espera a que la tarjeta lo
  haya leído, por su índice «hecho» o por el que da en el anillo de estado;
  si no lo confirma, lo cuenta y sigue) y 64 de **estado**, que escribe la
  tarjeta (trama recibida con su longitud y su estado, hasta dónde ha
  enviado) y lee la interrupción. Las tramas más largas que el búfer las
  corta la tarjeta (si no, su recepción se cuelga: lo dice `sky2`).
- **Filtro**: las tramas para la MAC de la tarjeta y las de difusión y
  multidifusión (el filtro de multidifusión, apagado). Sin modo promiscuo
  (las demás tarjetas tampoco lo tienen: el controlador de paquetes solo da los modos
  1 a 3).
- **Interrupción** (INTx, la que diga la BIOS en la configuración PCI; se
  quita el bit que la tapa): solo el anillo de estado y el PHY. Las de error
  de hardware y de descriptores no se piden (un error sin tratar dejaría la
  interrupción pegada); `/INFO` enseña sus causas pendientes (`ISRC`).
  Con **`/SONDEO`** no se usa: la rutina se engancha al reloj (IRQ 0) y mira
  la tarjeta 18 veces por segundo (lento, pero la red y CONSOLA funcionan):
  es el plan B si la interrupción no llega.
- **Parar** (`/U`): sin interrupciones, MAC parado, colas y unidad de estado
  en reinicio y el chip entero en reinicio por programa.

`HSPRED /INFO` (también con el controlador ya cargado) enseña, además de lo
de siempre: por dónde van los registros, el identificador y la revisión del
chip, y un volcado para depurar a distancia: placa (`PMD`, búfer RAM),
interrupciones (`ISRC`, `IMSK`, `HWE_ISRC`), configuración PCI propia
(`REG1/3/4/5`, `PSM4`), MAC (`GP_STAT`, `GP_CTRL`, `RX_CTRL`, FIFO), los
índices de los tres anillos (lo que dice la tarjeta y lo que lleva el
controlador), los contadores de tramas del MAC (recibidas y enviadas, con
error de CRC, desbordamientos), once registros del PHY y los contadores del
controlador (interrupciones atendidas, última causa, elementos de estado
tratados, envíos sin confirmar), y la línea `Yukon, IRQ:` (interrupciones
que llegaron y no eran de la tarjeta, `ajenas`: si suben, el IRQ está
compartido con otro aparato; `ELCR`: bit n = IRQ n por nivel; `IMR` e `IRR`
de los dos PIC, esclavo:maestro: el bit 3 de la parte alta es el IRQ 11). Y
avisa con «(!)» de lo raro: elementos de estado sin leer (la interrupción no
llega), envíos sin confirmar, búfer RAM.

El aviso de elementos sin leer compara, **en el mismo momento** (con las
interrupciones cerradas), el índice de la tarjeta (`STAT_PUT_IDX`) y el que
lleva el residente, y si no coinciden vuelve a mirar durante unos 220 ms con
las interrupciones abiertas (el chip retrasa la interrupción unos
microsegundos con sus temporizadores); solo avisa si en ese tiempo no se han
leído, y entonces dice `PUT`, hasta dónde ha leído el residente, el código
del elemento siguiente (80h o más: ya vale) e `ISRC`. Los contadores del
controlador también se leen del residente justo antes de escribirlos.

**El aviso falso del 18/11/2026.** En el HP Mini (desde CONSOLA, con la
salida a un archivo) `/INFO` dijo «Hay elementos de estado sin leer» con la
tarjeta recibiendo bien (2.601 tramas). No era la interrupción: `/INFO`
comparaba el índice de la tarjeta, leído al final, con una **foto** del
residente sacada al principio, y mientras escribía el informe llegaron unas
40 tramas. Se ve en el propio informe: la foto decía `rx_put=0005` y el
registro de la tarjeta ya estaba en `PUT=000D` (ese registro solo lo escribe
la interrupción al devolver búferes: 40 más, que dan la vuelta a los 16),
igual que el anillo de estado (`i_estado=003C` → `PUT=0024`, 40 más en 64);
`ISRC=00000000` (nada pendiente), `causa=40000000` (la del anillo de estado,
la que se espera) e `irqs` frente a `elem_estado` (1.947 frente a 2.812: 1,4
elementos por interrupción, lo normal con la moderación del chip). Arreglado
como se dice arriba; `tests/yukon_sim.py` lo reproduce (tramas llegando
mientras `/INFO` escribe) y prueba que con la IRQ cortada sí avisa.

**«Descartados»** (`Paquetes: ... descartados`) no son errores: son tramas
que han llegado bien y que **ningún programa ha pedido** (el controlador de paquetes
las tira porque nadie se ha apuntado a su tipo): IPv6, tipos que no son IP
ni ARP, y todo lo que llega cuando no hay ningún programa de red abierto
(entre `CONFRED` y `CONSOLA`, por ejemplo). Los errores van aparte
(`errores`).

Cómo leer el informe si no va:

| Qué sale | Qué significa |
|---|---|
| «La tarjeta Yukon no contesta ni por memoria ni por los puertos» | Ningún camino lee un identificador de chip de la familia (B3h a BEh). `/DEPURA` dice lo que se lee por cada uno. |
| `PHY: 02=FFFF 03=FFFF` | El PHY no contesta por SMI: no ha salido del reinicio o sigue apagado (`REG1`, bit 26). |
| `Enlace: no` con el cable puesto y `11=` sin el bit 10 | La autonegociación no acaba: mirar `01=` (bit 5: acabada; bit 2: enlace) y `05=` (lo que anuncia el otro extremo). |
| `irqs=00000000` y «(!) Hay elementos de estado sin leer» | La tarjeta recibe y lo apunta, pero la interrupción no llega: IRQ equivocado o tapado (mirar `IMR` en `Yukon, IRQ:`). Con `/SONDEO` debería ir. |
| «(!) Hay elementos de estado sin leer» con `irqs` subiendo | Se pierden interrupciones de vez en cuando: `siguiente=` menor de 80h (el elemento aún no vale cuando se lee el índice), `ajenas` alto (IRQ compartido) o `ELCR` sin el bit del IRQ (por flanco: con otro aparato en la misma línea, el flanco se pierde). |
| `rx_uni`/`rx_dif` suben y `elem_estado` no | El MAC recibe, pero no llega al anillo de estado: cola de recepción o unidad de estado mal arrancadas (`CSR`, `GET`/`PUT`). |
| `tx_sin_confirmar` sube | La tarjeta no dice que ha leído lo enviado: `HECHO`/`DICHO` frente a `PUT` del envío; si `tx_uni`/`tx_dif` suben, sí que sale. |
| `ISRC` con los bits 0 a 3 o el 31 | Error de descriptores (0: envío, 2: recepción), del MAC (3) o de hardware (31, con el detalle en `HWE_ISRC`). |

Lo que no tiene (y `sky2` sí): control de flujo, sumas de comprobación por
la tarjeta, VLAN, RSS, tramas gigantes, Wake-on-LAN, el «enlace rápido» de
la Optima, el tratamiento de los errores de hardware y el vigilante que
relanza la recepción si se cuelga. Los otros chips Yukon 2 y los Yukon
antiguos (`skge`) tampoco.

#### La memoria USB de prueba del HP Mini

`tools/prueba-cable/hacer.sh` (después de `make usb`) hace
`hispados-usb-cable.zip`: la imagen USB de siempre con `CABLE.BAT` en la
raíz, que el `AUTOEXEC.BAT` llama al acabar. `CABLE.BAT` apunta
`EQUIPO /PCI` (cómo ve el equipo la tarjeta), carga
`HSPRED /DEPURA:C:\CABLE.TXT`, hace `CONFRED` y, si falla, apunta
`HSPRED /INFO`, descarga el controlador y lo repite con `/SONDEO`; con la
red en marcha, `PING /P` (a la puerta de enlace), `HSPRED /INFO` al archivo
y pregunta si abrir `CONSOLA` (puerto 2323) para entrar a distancia. Todo
queda en `C:\CABLE.TXT`, en la memoria USB. Las instrucciones para quien
hace la prueba, en `tools/prueba-cable/LEEME.TXT` (arrancar el HP desde la
memoria USB: F9). Probada en QEMU (18/11/2026), arrancando la imagen como
disco duro: con una e1000 corre entera (CONFRED, `PING /P` con 4 de 4,
`/INFO`, la pregunta de la consola) y sin tarjeta acaba en «HSPRED no ha
podido poner en marcha la tarjeta»; con la Yukon, solo en el HP Mini.

### Adaptadores USB-Ethernet (CDC-ECM)

Desde el 17/11/2026, un adaptador USB-Ethernet que ofrezca la **clase
estándar CDC-ECM** es una tarjeta más de HSPRED. El aparato lo lleva
`HSPUSB.SYS` (enumeración, transferencias y sondeo: `lib/usb_red.inc`, ver
[USB.md](USB.md)) y HSPRED solo pone el puente con el controlador de paquetes. Vale
para:

- los **Realtek RTL8152 y RTL8153** (los adaptadores «USB a Ethernet
  Gigabit» corrientes, muchos con un concentrador de 3 puertos; `0BDA:8152`,
  `0BDA:8153`, y los de Lenovo, Microsoft, Samsung o TP-Link que los llevan
  dentro). Su configuración 1 es el modo propio de Realtek y la **2**, la
  estándar: HSPUSB mira todas y elige la que trae CDC-ECM;
- cualquier otro que traiga CDC-ECM en alguna configuración (algunos ASIX
  AX88179A/AX88772D, los «gadgets» de Linux, `-device usb-net` de QEMU, que
  trae RNDIS en la 1 y CDC-ECM en la 2).

```
CONFIG.SYS:   DEVICE=A:\HISPADOS\HSPUSB.SYS /F /RED /INFO
  ...
  5 SS 2109:0817 09/00/00 -> concentrador de 4 puertos
    5.1 SS 0BDA:8153 02/06/00 0A/00/00 -> red (CDC-ECM)
Adaptador de red USB preparado (CDC-ECM), MAC 00:E0:4C:68:xx:xx: cargue HSPRED.

A:\>HSPRED
Tarjeta Adaptador USB (CDC-ECM), MAC 00:E0:4C:68:xx:xx, por HSPUSB, controlador de paquetes en INT 60h
A:\>HSPRED /INFO
Tarjeta:         Adaptador USB (CDC-ECM)
USB:             0BDA:8153, supervelocidad (USB 3), por xHCI (lo lleva HSPUSB)
IRQ:             no usa (HSPUSB mira si hay tramas en cada tic del reloj)
Dirección MAC:   00:E0:4C:68:xx:xx
Enlace:          sí
```

(Las líneas del RTL8153 son como **deberían** salir: no se ha probado con
uno de verdad; ver «Límites».) Después, `CONFRED`, `PING`, `DESCARGAR`,
`ACTUALIZAR` y `CONSOLA` como con cualquier tarjeta. Hace falta:

1. `HSPUSB.SYS` cargado en `CONFIG.SYS` **con la opción `/RED`** y
   **llevando él el USB** (con `/F` si la BIOS lo está usando; en un MacBook
   lo toma él solo), y el adaptador **conectado al arrancar** (no se busca
   después). HSPUSB dice «Adaptador de red USB preparado (CDC-ECM), MAC
   ...». **Sin `/RED` la red por USB está apagada**: HSPUSB no toca el
   adaptador (hace lo mismo que antes de tener la red) y solo dice
   «Adaptador de red USB 0BDA:8153: sin usar. Para usarlo, añada /RED a la
   línea de HSPUSB.SYS en CONFIG.SYS.»
2. `HSPRED` (a mano o en `AUTOEXEC.BAT`). Primero busca una tarjeta PCI de
   las suyas y, si no hay, el adaptador USB; con `HSPRED /USB`, solo el
   adaptador USB.

Si el adaptador no trae CDC-ECM en ninguna configuración (solo el modo
propio del fabricante, NCM o RNDIS), HSPUSB lo dice con su VID:PID, con
`/INFO` o sin él: «Adaptador de red USB 0B95:1790: aún no se admite (no
ofrece la clase estándar CDC-ECM)» y, en el árbol, «adaptador de red que aún
no se admite (sin CDC-ECM)».

#### Cómo va por dentro

- **Reconocer** (`role_net`, `net_parse`): se leen los descriptores de todas
  las configuraciones (`bNumConfigurations`) buscando una interfaz de
  comunicaciones 02h/06h con su descriptor funcional Ethernet (la MAC es la
  cadena `iMACAddress`, 12 cifras hexadecimales) y la interfaz de datos 0Ah
  que tenga los dos extremos bulk. Después: `SET_CONFIGURATION` de esa
  configuración, `SET_INTERFACE` de la de datos a la alternativa con los
  extremos (la 1) y `SET_ETHERNET_PACKET_FILTER` (lo dirigido a la tarjeta y
  la difusión; hay adaptadores que no dan enlace ni tramas hasta recibirlo).
- **Recibir**: no hay interrupción. La tubería bulk de entrada se lleva como
  una tubería más de HSPUSB, con las mismas rutinas que las del teclado y el
  ratón (en el xHCI, su anillo; en el EHCI, una cola de la lista periódica
  atendida en las 8 microtramas; en la OHCI, un ED; en la UHCI, 24 TD
  seguidos), con un búfer de una trama. Se deja pedida y se mira **en cada
  tic del reloj** (INT 08h, 18 veces por segundo) y **cada vez que el
  programa consulta el teclado** (INT 16h, AH=01h u 11h): las esperas de
  `lib/red.inc` lo hacen en cada vuelta (por si se pulsa Esc), así que la
  respuesta no espera al tic. Cuando llega una trama se entrega a HSPRED
  (que llama al receptor del programa, como en la interrupción de una
  tarjeta PCI), se pide la siguiente y se la espera hasta 2 ms (vienen a
  ráfagas), 8 por pasada como mucho.
- **Enviar**: síncrono, como las transferencias de la memoria USB (HSPUSB
  hace una cada vez). Si HSPUSB está ocupado en ese momento (en mitad de una
  lectura del disco USB, por ejemplo, desde una interrupción), el envío
  falla (`CANT_SEND`) y el programa lo repite; desde el receptor sí se puede
  enviar.
- **Enlace**: el adaptador avisa por su tubería de interrupción
  (`NETWORK_CONNECTION`); HSPUSB la lee y `HSPRED /INFO` dice «sí» o «no».
  Mientras no llegue ningún aviso, «no se sabe» (usb-net de QEMU no manda
  ninguno).
- **HSPUSB ↔ HSPRED**: `INT 2Fh AX=DB00h` (ver «API»).

#### Límites

- **Sin probar en un adaptador de verdad** (17/11/2026): todo se ha probado
  con `-device usb-net` de QEMU, que es de USB 1.1 (velocidad plena) y cuyo
  concentrador también lo es. No se ha podido probar: el adaptador a **alta
  velocidad** (USB 2.0, paquetes de 512) ni a **supervelocidad** (USB 3,
  paquetes de 1024), un **concentrador de USB 2.0 o 3.0** por medio (el de
  los adaptadores con puertos; ver USB.md), ni que lleguen los avisos del
  enlace. En el EHCI el camino del adaptador usa las rutinas ya probadas con
  el teclado (recepción) y con la memoria USB (envío), pero juntas y con
  tramas, no.
- En un PC con xHCI de Intel de las series 7 a 9 (el MacBook Air de 2012),
  si el adaptador no aparece a supervelocidad se puede probar `HSPUSB /2`:
  no enciende el USB 3 de los puertos y el adaptador (y su concentrador) va
  a alta velocidad.
- El adaptador tiene que estar conectado al cargar HSPUSB, y no se puede
  quitar con HSPRED cargado (los envíos fallarían tras 2 s cada uno).
- El adaptador es lo último que HSPUSB pone en marcha, con la memoria de
  trabajo que quede después del teclado, el ratón, la memoria USB y los
  concentradores: si no queda (muchos aparatos USB a la vez), lo dice
  («Adaptador de red USB: no queda memoria para él...») y no se usa; lo
  demás sigue igual. Ver USB.md, «Memoria».
- Un adaptador a la vez (el primero). Sin multidifusión.
- La velocidad del enlace (10/100/1000) no se dice.
- Un programa que no consulte el teclado mientras espera recibe solo con el
  tic del reloj: hasta 8 tramas cada 55 ms.

#### Lo que falta (no entra ahora)

| Qué | Para quién | Qué habría que hacer |
|---|---|---|
| **CDC-NCM** (02h/0Dh) | RTL8156 (2,5 Gb), los AX88179A y los móviles que comparten red | Lo mismo que ECM (mismas tuberías y sondeo), pero cada transferencia lleva varias tramas con una cabecera NTH16/NDP16: `GET_NTB_PARAMETERS`, desempaquetar al recibir y poner la cabecera al enviar. |
| **RTL8152/8153 en su modo propio** (configuración 1) | No hace falta mientras traigan ECM; daría el enlace y la velocidad seguros | Registros por peticiones de control de fabricante (PLA/USB OCP), arranque del PHY y del MAC, y una cabecera de 8 octetos por trama al recibir y al enviar. A partir de `r8152.c` de Linux (GPL-2.0): largo. |
| **ASIX AX88179/AX88178A** (`0B95:1790`, `0B95:178A`) | El otro chip de los adaptadores Gigabit baratos; los AX88179 antiguos **no** traen ECM | Como el anterior: registros por control de fabricante, PHY por MII, cabeceras propias en cada transferencia (`ax88179_178a.c` de Linux). |
| **AX88772** (`0B95:772x`, el adaptador USB de Apple `05AC:1402`) | Adaptadores de 100 Mbit | Igual, más sencillo (`asix_devices.c`). |
| **RNDIS** | Móviles Android con «anclaje USB» | Mensajes RNDIS por el extremo de control y cabecera de 44 octetos por trama. |
| Conectar y quitar con el driver cargado | — | Buscar el adaptador en `rescan` (hoy solo busca memorias) y soltar su bloque al quitarlo. |

### Las tarjetas de los portátiles de las pruebas

Sacado de los informes de `DIAGNOSTICO` (`INFORMES\...\INFORME.TXT` y los
volcados `PCI\*.BIN`, clase 02h):

| Equipo | Ethernet (cable) | Inalámbrica (Wi-Fi) |
|---|---|---|
| Lenovo A275 | **Realtek RTL8111EP/8168EP** (`10EC:8168`, rev. 0Eh, subsistema `17AA:511F`; en 01:00.0, puertos 2400h, IRQ 5; con las funciones DASH 01:00.1-4: dos puertos serie, IPMI y un EHCI) | Realtek RTL8822BE (`10EC:B822`, en 03:00.0, clase FFh) |
| HP Mini 5103 (los dos) | **Marvell Yukon 88E8059 «Yukon Optima»** (`11AB:4381`, rev. 11h, subsistema `103C:1608`): en HSPRED desde el 18/11/2026, **pendiente de probar** | Broadcom BCM43224 (`14E4:4353`) |
| MacBook Air 2012 | — (no tiene; el adaptador de Apple por Thunderbolt es una Broadcom BCM57762, `tg3`). Con un adaptador USB Realtek RTL8153 (`0BDA:8153`, «USB-C a Ethernet Gigabit con concentrador de 3 puertos»): por CDC-ECM, **pendiente de probar** | Broadcom BCM43224 (`14E4:4353`) |

- **A275**: la RTL8168 de `HSPRED` funciona en el Toshiba NB250 (otra
  revisión), pero en este equipo está sin probar. Lo que falta: probarla
  (`HSPRED /INFO`, que ya dice el `TxConfig`, el XID y la familia; `CONFRED`;
  `PING` a la puerta de casa); si no recibe, el primer sospechoso es el
  firmware DASH de las 8111EP, que en Linux necesita un aviso de «arranque
  del controlador» por los registros OCP (`rtl8168ep_driver_start`), y luego
  lo de la tabla de «RTL8168: lo que falta para las revisiones modernas».
- **HP Mini 5103**: la Marvell Yukon 88E8059 no tiene documentación pública
  (Marvell solo la daba con acuerdo de confidencialidad); la única fuente es
  el controlador `sky2` de Linux (GPL-2.0, compatible, unas 5.000 líneas, con
  muchas diferencias entre modelos). **Hecha el 18/11/2026** (solo el camino
  de la Optima) y pendiente de probar en el equipo: ver «Marvell Yukon
  88E8059 (HP Mini 5103)». Si no fuera, sirve un adaptador USB-Ethernet con
  CDC-ECM (los Realtek RTL8152/8153: ver «Adaptadores USB-Ethernet»).
- **Wi-Fi**: la Broadcom BCM43224 (HP Mini 5103 y MacBook Air de 2012) la
  lleva `HSPWIFI`, de Hispa-486, con WPA2: ver [WIFI.md](WIFI.md) (hecho el
  18/11/2026; **sin probar aún en la tarjeta de verdad**). La Realtek
  RTL8822BE del A275, todavía no.

Probar una tarjeta: `HSPRED /INFO` dice cuál ha encontrado y si hay enlace;
`CONFRED` hace el DHCP; `PING` a la puerta de enlace. Si no hay tarjeta que
sepa usar, `HSPRED` enseña las de red que hay con su `fabricante:modelo`,
para poder pedirla.

## Cable o Wi-Fi (Hispa-486, desde la 0.9.28)

El `AUTOEXEC.BAT` de Hispa-486 lleva `HSPRED /AUTO`, `HSPWIFI /AUTO` y
`WIFI /AUTO`, por este orden, antes del escritorio. Hasta la 0.9.27,
`HSPRED /AUTO` se quedaba siempre que hubiera tarjeta de cable, aunque no
hubiera cable enchufado; en un portátil con las dos tarjetas (como el Toshiba
NB250, con su Realtek RTL8136 de cable) el Wi-Fi solo llegaba si `HSPWIFI`
podía descargar `HSPRED`, y con otro residente encima (`HSPACPI`) no podía:
sin Wi-Fi y sin manera fácil de cambiarlo. Ahora:

| `Preferir=` (RED.INI) | `HSPRED /AUTO` | `HSPWIFI /AUTO` |
|---|---|---|
| `AUTO` (o nada) | Pone la tarjeta en marcha y espera hasta **3 segundos** al enlace. Con enlace (cable enchufado), o si la tarjeta no sabe decirlo (NE2000, Tulip, USB), se queda. Sin enlace y **con una tarjeta Wi-Fi** de las de HSPWIFI, sale sin quedarse y lo dice en una línea («HSPRED: no hay cable enchufado; la red irá por el Wi-Fi.», nivel de error 2). Sin enlace y sin Wi-Fi, se queda como antes (al enchufar el cable, va). | Se carga si hay tarjeta Wi-Fi y no se ha quedado el cable (aunque no haya red guardada: así se puede elegir desde la ventanita). Si `HSPRED` está y no tiene enlace en 3 s, lo descarga. |
| `CABLE` | Se queda siempre que haya tarjeta (como hasta la 0.9.27), sin esperar. | Con `HSPRED` cargado, calla (no lo quita). Sin `HSPRED`, se carga si hay tarjeta Wi-Fi. |
| `WIFI` | Con tarjeta Wi-Fi, sale sin quedarse («HSPRED: RED.INI dice Preferir=WIFI; ...»); sin ella, se queda. | Se carga si hay tarjeta Wi-Fi; si `HSPRED` está, lo descarga aunque tenga enlace. |

Las tarjetas Wi-Fi se buscan en el PCI (la BIOS, `INT 1Ah AX=B102h`) con la
tabla de `lib/wifi_ids.inc` (Broadcom 14E4:4353/4357/4727/0576/A8D8 y
Atheros 168C:0023/0024/0027/0029/002A/002B/002C/002D/002E), la misma que
usan `HSPWIFI /AUTO` y Hispa Ventanas. Lo de `RED.INI` lo lee
`lib/redpref.inc` (de `HSPRED` y de `HSPWIFI`). `HSPRED /Q` (lo que usa
`ACTUALIZAR` al cargarlo) no elige nada: se queda como siempre.

Si `HSPWIFI` no puede quitar `HSPRED` (otro residente cargado después), ya
no dice «Descárguelo antes: HSPRED /U» (que tampoco podía): dice que se elija
el Wi-Fi en Hispa Ventanas (o `Preferir=WIFI`) y se reinicie.

**En Hispa Ventanas**, la ventanita del icono de red (docs/WIFI.md, «En
Hispa Ventanas») dice por dónde va la red ahora (cable con o sin cable
enchufado, Wi-Fi, ninguna) y lleva **«Usar: Automático / Cable / Wi-Fi»**:
guarda `Preferir=` y, si para eso hace falta otro controlador (los
residentes no se pueden cambiar con el escritorio en marcha), pregunta
«Hace falta reiniciar para usar el Wi-Fi. ¿Reiniciar ahora?» (o «...el
cable»): **Sí** reinicia como «Reiniciar» del cuadro «Salir»; **No** lo deja
para el próximo arranque.

En Hispa-386 el `AUTOEXEC.BAT` trae la red con `REM` (a mano): nada cambia
mientras no se quite. Quien quite el `REM` de `HSPRED /AUTO` en un equipo
con Wi-Fi y sin cable verá que no se queda; con `Preferir=CABLE` se queda
siempre.

## RED.INI

`\HISPADOS\RED.INI` de la unidad de arranque (`sistema/RED.INI` en el
proyecto). Claves (sin importar mayúsculas; `;` y `#` empiezan comentarios):

| Clave | Valor |
|---|---|
| `DHCP=` | `SI` (lo normal; también `SÍ`, `YES`, `1`) o `NO` |
| `IP=` | la dirección (con `DHCP=NO`) |
| `MASCARA=` (`MASK=`) | la máscara; si no se dice, 255.255.255.0 |
| `PUERTA=` (`GATEWAY=`) | la puerta de enlace (el router) |
| `DNS=` | el servidor de nombres; con `DHCP=SI` también, y manda sobre el que da el DHCP |
| `ACTUALIZAR=` | la dirección HTTP del archivo `versiones.txt` que mira `ACTUALIZAR` (si no, la de por omisión). Opcional. |
| `WIFI=`, `WIFICLAVE=` | la red inalámbrica y su clave ya preparada (no la contraseña); las escribe la orden `WIFI` de Hispa-486: ver [WIFI.md](WIFI.md). Configuración de Hispa Ventanas las conserva. |
| `Preferir=` | `AUTO` (lo normal; también si no está), `CABLE` o `WIFI`: por dónde va la red al arrancar con `HSPRED /AUTO` y `HSPWIFI /AUTO` (ver «Cable o Wi-Fi»). La escribe la ventanita de la red de Hispa Ventanas («Usar:»); Configuración la conserva. |
| `HAYWIFI=SI` | solo para las pruebas: `HSPRED /AUTO` y la ventanita hacen como si hubiera una tarjeta Wi-Fi (QEMU no tiene ninguna de las admitidas). |

`RED.INI` no tiene secciones: las claves van sueltas (una línea `[Red]`, si
se pone, no estorba: las líneas que empiezan por `[` no cuentan). Si una
clave sale dos veces, vale la última (la ventanita añade `Preferir=` al
final y quita las de antes).

También se puede poner con ratón en **Hispa Ventanas**: Configuración ›
apartado **«Red»** (una casilla para el DHCP y, a mano, la IP, la máscara, la
puerta y el DNS). Escribe estas mismas claves en `RED.INI` (ver
docs/VENTANAS.md); los cambios se aplican al reiniciar o al recargar HSPRED.

La configuración que se consigue (por DHCP o de `RED.INI`) se guarda en
`HSPRED` y la usan todos los programas sin volver a pedirla, hasta
`CONFRED /RENOVAR` o reiniciar. La concesión de DHCP no se renueva sola (en
casa suele ser de un día o más): `CONFRED /RENOVAR`.

## API

### HSPRED: controlador de paquetes («packet driver», especificación de FTP Software 1.09)

El vector (`INT 60h` si no se dice otro) apunta a la entrada, que lleva
`PKT DRVR` 3 octetos después, como pide la especificación. Funciones (AH):

| AH | Función | |
|---|---|---|
| 01h | `driver_info` | BX = versión, CH = clase 1 (Ethernet DIX), DX = tipo 1, CL = 0, DS:SI = nombre de la tarjeta, AL = 2 (básicas y ampliadas) |
| 02h | `access_type` | AL = clase, BX = tipo (FFFFh: cualquiera), DL = 0, DS:SI y CX = tipo de paquete (hasta 8 octetos; 0 = todos), ES:DI = receptor -> AX = manejador. Hasta 8 a la vez |
| 03h | `release_type` | BX = manejador |
| 04h | `send_pkt` | DS:SI = trama (14 a 1514 octetos; las cortas se rellenan a 60) |
| 05h | `terminate` | descarga HSPRED si se puede |
| 06h | `get_address` | ES:DI, CX >= 6 -> la MAC, CX = 6 |
| 07h | `reset_interface` | no hace nada (vale) |
| 14h | `set_rcv_mode` | CX = 1 (nada), 2 (solo a esta tarjeta) o 3 (y difusión: lo normal); los demás modos (multidifusión, todos) dan BAD_MODE |
| 15h | `get_rcv_mode` | AX = modo |
| 16h, 17h | multidifusión | NO_MULTICAST |
| 18h | `get_statistics` | DS:SI = paquetes y octetos recibidos y enviados, errores y descartados (7 dobles palabras) |
| 19h | `set_address` | CANT_SET |

Errores: CF = 1 y DH = código (1 BAD_HANDLE ... 15 CANT_RESET, los de la
especificación). Los paquetes se entregan como dice la especificación: el
receptor recibe primero AX = 0, BX = manejador, CX = longitud y devuelve
ES:DI (0:0 si no lo quiere); se copia el paquete y se le llama otra vez con
AX = 1 y DS:SI = el búfer. Las dos llamadas se hacen desde la interrupción
de la tarjeta, con las interrupciones desactivadas y la pila propia de
HSPRED (512 octetos).

Por ser la interfaz estándar, sirven también los programas de DOS de otros
que usan controladores de paquetes: mTCP (DHCP, FTP, Telnet, IRC, HTGET...), WATTCP
y lo que se hizo con él, Arachne... Sin probar todavía con ellos (no van en
el proyecto).

### HSPRED: INT 2Fh AH=DAh (para los programas de Hispa-DOS)

| AX | |
|---|---|
| DA00h | AL = FFh si está, BX = 5248h («HR»), CL = vector del controlador de paquetes, ES:DI = configuración IP compartida (30 octetos: +0 indicadores (bit 0 válida, bit 1 por DHCP), +2 IP, +6 máscara, +10 puerta, +14 DNS, +18 servidor DHCP, +22 concesión en segundos, +26 día, mes y año) |
| DA01h | AL = enlace (0 no, 1 sí, FFh no se sabe), AH = velocidad (1 = 10, 2 = 100, 3 = 1000 Mbit/s; 0 no se sabe) |
| DA02h | descargar (BL = 1: aunque haya programas usándolo) -> AL = 0 hecho, 1 hay otro residente encima, 2 en uso |
| DA03h | ES:BX = datos del controlador (los que enseña `/INFO`) |

### HSPUSB: INT 2Fh AX=DB00h (el adaptador de red USB, para HSPRED)

Solo contesta si HSPUSB lleva un adaptador con CDC-ECM (si no lo hay, HSPUSB
no atiende INT 2Fh):

| | |
|---|---|
| Entrada | AX = DB00h |
| Salida | AL = FFh, BX = 5552h («UR»), ES:DI = bloque de datos del adaptador |

| Desplazamiento | Qué |
|---|---|
| +0 | la MAC (6 octetos) |
| +6, +8 | VID y PID del aparato USB |
| +10 | velocidad USB: 1 plena, 3 alta, 4 súper |
| +11 | clase de la controladora: 0 UHCI, 1 OHCI, 2 EHCI, 3 xHCI |
| +12 | enlace: 0 no, 1 sí, FFh no se sabe |
| +13 | versión del bloque (1) |
| +14 | **enviar** (puntero lejano, se llama con `CALL FAR`): DS:SI = trama, CX = octetos (14 a 1514; las cortas las rellena a 60) -> CF = 1 si no ha salido. Conserva los registros. |
| +18 | **receptor** (puntero lejano; lo escribe quien quiera recibir, 0:0 = nadie): HSPUSB lo llama con ES:DI = trama, CX = octetos (14 a 1514), con las interrupciones cerradas, desde su INT 08h o su INT 16h, con su pila. Tiene que conservar SS:SP. |
| +22, +24 | tramas recibidas con error o descartadas, y envíos fallidos (dos palabras) |

HSPRED deja su receptor (`us_rx`) al quedarse residente y lo quita al
descargarse (`HSPRED /U`).

### La pila TCP/IP: lib/red.inc

Escrita aquí, desde las RFC (791 IP, 792 ICMP, 768 UDP, 793 TCP, 826 ARP,
2131 DHCP, 1035 DNS, 1945 HTTP/1.0); se incluye en cada programa (unos 5 KB
de código). Habla con cualquier controlador de paquetes. Tiene: Ethernet II; ARP
(pregunta, contesta, recuerda 4 direcciones y **aprende** la MAC de quien
envía si está en nuestra red, para poder contestarle sin preguntar); IPv4
(sin fragmentos); ICMP (contesta al eco y hace eco); UDP; cliente de DHCP;
cliente de DNS (registros A); TCP con **una conexión a la vez**, de cliente o
de servidor: abrir (con reintentos del SYN), **escuchar** (apertura pasiva:
acepta un SYN entrante, contesta SYN+ACK y espera el ACK), enviar esperando el
acuse y reenviando cada segundo, recibir en orden con acuse inmediato (lo
desordenado se tira y se repite el acuse, y el otro lado reenvía), cerrar con
FIN y RST. Ventana fija de 5.840 octetos (4 segmentos), MSS 1.460. Los
paquetes que llegan esperan en una cola de 8 (los rellena el receptor desde la
interrupción) y el programa los atiende en `red_sondear`. Las rutinas están
descritas al principio de `lib/red.inc`.

**Lado servidor** (para `CONSOLA`): `tcp_escuchar BX=puerto` deja la conexión
en LISTEN y vuelve enseguida; `tcp_estado` devuelve el estado (2 =
establecida). Cuando llega un SYN a ese puerto (de cualquiera), `tcp_in` guarda
el extremo, contesta SYN+ACK y pasa a SYN-RECEIVED; con el ACK del cliente, a
establecida, y desde ahí `tcp_enviar`, la recepción por `[t_rutina]` y
`tcp_cerrar` son iguales que en el cliente. Mientras hay una conexión (o a
medio establecer), un SYN nuevo a ese puerto se descarta (una sola a la vez).
Estas rutinas van en `lib/red.inc` (cada programa lleva su copia de la pila),
igual que las del cliente: la pila no es residente.

## La red de los programas Hispa32 (Hispa-486)

Desde el 18/11/2026 los programas de 32 bits (Hispa32: docs/486/HISPA-486.md)
tienen red, y cifrada. Son tres piezas, las tres en el disquete «Internet»
de Hispa-486 (`make internet`), y un programa que las usa además del
Navegador (docs/486/NAVEGADOR.md):

```
A:\>HSPDPMI
A:\>HSPRED
A:\>B:\HW32 B:\HTTPS.EXE /V https://clicgest.es/login PAGINA.HTM
Servidor clicgest.es: 31.70.156.143:443 (cifrado)
Conexión segura: TLS 1.2, ECDHE-ECDSA, ChaCha20-Poly1305 (280 ms; 21942 miles de ciclos de cálculo)
HTTP/1.1 200 OK
...
Guardado en PAGINA.HTM (8764 octetos).
```

`HTTPS [/V] [/N] dirección [archivo]`: baja por HTTP o HTTPS, sigue hasta 5
redirecciones y guarda el archivo (sin nombre: el de la ruta en 8.3, o
`INDEX.HTM`). `/N`: no guarda, dice los octetos y su suma. `/V`: las
cabeceras, el cifrado y lo que ha costado el apretón de manos. Sale con 0 si
todo va bien (4: la conexión segura falla; 8: el servidor no da un 200).
(COMMAND.COM lanza los `.EXE` de Hispa32 con HW32 él solo: basta `HTTPS ...`
si HW32.COM está junto a COMMAND.COM o en el PATH.)

### 1. HW32: las tramas (utils/hw32k.inc)

Tres funciones nuestras en el KERNEL32 de HW32 (no son del API de Windows):
`HispaRedAbrir(info)`, `HispaRedEnviar(trama, octetos)` y `HispaRedCerrar()`.
Al abrir, HW32 busca el controlador de paquetes, pide los tipos IP y ARP y pone como
receptor un trozo de código de modo real que copia a un bloque de memoria
convencional (19 KB): desde la interrupción de la tarjeta, cada trama cae en
una ranura de un anillo de 12, y el programa (en modo protegido) lo lee
directamente de esa memoria, sin llamar a nadie. Enviar es una llamada al
controlador de paquetes por DPMI. `info` trae la MAC, el anillo y la configuración IP
compartida de HSPRED.

Al acabar el programa, **como sea** (`ExitProcess`, un error, que lo termine
el Administrador de tareas, Ctrl+C en la consola), HW32 suelta los tipos y el
bloque: si no, el controlador llamaría a un receptor que ya no está. Lo único
que no se cubre: un programa de consola que el procesador tumbe con una
excepción (dentro de Hispa Ventanas sí se cubre).

Los tipos IP y ARP son de uno solo a la vez: **un programa con la red abierta
cada vez** (el segundo recibe «la red no está disponible»). Dos programas de
red a la vez pedirán la pila residente de «Pendiente».

### 2. WSOCK32.DLL: la pila TCP/IP y Winsock 1.1 (w32/wsock32.c)

**Una pila nueva, en C**, y no la de `lib/red.inc`. El porqué: aquella es
ensamblador de 16 bits de modo real, con sus datos en el segmento del
programa, una sola conexión y el receptor en su propio código; usarla desde
un programa de 32 bits en modo protegido era pasar a modo real en cada
llamada y copiar cada dato, y aun así faltaban varias conexiones y los
sockets sin bloqueo que pide un navegador. Las DLL de Hispa32 se escriben en
C (como MSVCRT.DLL), TLS (BearSSL) es C y lo que hay que compartir con la de
16 bits no es el código sino lo que ya se comparte: el controlador de paquetes y la
configuración IP que guarda HSPRED.

- Ethernet II, ARP (8 direcciones; la trama que espera a que ARP conteste
  se guarda y sale al llegar la respuesta), IPv4 sin fragmentos, ICMP
  (contesta al eco), UDP, cliente de DHCP y de DNS (registros A, con una
  memoria de 8 nombres durante 10 minutos).
- TCP de cliente con hasta 8 conexiones: reenvíos (1 s, doblando, hasta 8
  veces; el SYN, 4), ventana según el sitio que queda (16 KB por conexión,
  8 segmentos como mucho, que es lo que cabe en el anillo), MSS negociado,
  cierre ordenado (el FIN sale tras los datos; la ficha se suelta cuando el
  otro lado acaba o a los 5 s), RST a lo que no es de nadie. Lo que llega
  desordenado se tira y se repite el acuse (el otro lado reenvía).
- La configuración IP es la de HSPRED (la que dejó `CONFRED`); si no hay,
  la consigue como `lib/red.inc` (RED.INI de la unidad de COMSPEC: DHCP o a
  mano, `DNS=` manda) y se la deja a HSPRED para los demás.
- No hay hilos: la pila avanza cada vez que el programa llama a una función.
  Las llamadas que esperan ceden el turno (`Sleep(0)`: dentro de Hispa
  Ventanas, a las demás tareas).
- **Winsock 1.1**, por nombre: `WSAStartup`, `WSACleanup`,
  `WSAGetLastError`, `socket` (TCP y UDP), `connect`, `send`, `recv`,
  `sendto`, `recvfrom`, `bind`, `closesocket`, `shutdown`, `select`,
  `ioctlsocket` (`FIONBIO`, `FIONREAD`), `gethostbyname`, `gethostname`,
  `getsockname`, `getpeername`, `setsockopt`, `getsockopt` (`SO_ERROR`),
  `inet_addr`, `inet_ntoa`, `htons` y compañía, `__WSAFDIsSet`. Los números
  de error son los de Winsock. Faltan: servidores (`listen` y `accept`
  devuelven `WSAEOPNOTSUPP`), las `WSAAsync*`, los datos urgentes y la
  importación por número (los programas de Visual C++ importan WSOCK32 así;
  los de MinGW, por nombre).

### 3. HSPTLS.DLL: TLS 1.2 con comprobación del certificado (w32/tls/)

La criptografía y el protocolo son de **BearSSL 0.6** (Thomas Pornin,
licencia MIT, compatible con la GPL: `terceros/bearssl`, solo los 60 archivos
que se enlazan, sin tocar), hecho para máquinas pequeñas: sin memoria
dinámica y con implementaciones de 32 bits. Lo nuestro (`hsptls.c`):

- **El perfil:** solo TLS 1.2; intercambio ECDHE (Curve25519, P-256, P-384);
  servidor con certificado ECDSA o RSA; ChaCha20-Poly1305 o AES-GCM. Es lo
  que piden hoy los servidores corrientes (clicgest.es: ECDHE-ECDSA con
  ChaCha20-Poly1305). Sin TLS 1.3, sin RSA como intercambio, sin CBC.
- **El certificado se comprueba:** la cadena hasta una autoridad raíz, el
  nombre del servidor y las fechas, con el reloj del equipo. Las raíces
  (`w32/tls/raices/*.pem`, 20: las de Let's Encrypt —ISRG Root X1, X2 y las
  nuevas Root YE y Root YR, que son las de clicgest.es—, DigiCert, GlobalSign,
  Google, Amazon, Sectigo/USERTrust, GoDaddy, Starfield, Microsoft, Certum,
  SSL.com) van dentro de la DLL (`anclas.h`; se rehace con
  `tools/mkanclas.sh`). Además vale como raíz cualquier certificado `*.CER`
  (X.509 en DER, de una autoridad) que haya **en la carpeta del programa**:
  para la intranet de una asociación, y para las pruebas. Sin revocación
  (ni CRL ni OCSP).
- **La memoria de certificados.** La cadena de clicgest.es son dos firmas
  ECDSA P-384, que con aritmética genérica de 32 bits son lo más caro con
  diferencia. La cadena que ya se dio por buena para un servidor (el resumen
  SHA-256 del nombre y de los certificados) se recuerda **una semana**
  (`HSPTLS.CCH`, junto al programa, 16 servidores): mientras tanto no se
  vuelve a comprobar, solo se lee la clave del servidor. Si el servidor
  cambia de certificado, o pasa la semana, se comprueba otra vez entera.
- **El azar.** No hay generador del sistema. Se junta (SHA-256) lo
  imprevisible que hay: el contador del reloj (PIT, 0,8 µs) y el de ciclos
  (TSC, si lo hay) cada vez que llega algo de la red o se llama a la DLL, la
  hora, las teclas y el ratón (el Navegador se los pasa) y una semilla que
  se guarda de una vez para otra (`HSPTLS.SEM`). En un equipo recién
  instalado y sin tocar es poco azar para lo que se pide hoy: es el punto
  flaco, y está dicho.
- Las sesiones se reanudan si el servidor guarda su identificador (los
  «tickets» no están).
- Los tiempos de las multiplicaciones dependen de los datos en un 486 (y
  BearSSL no puede evitarlo en ese procesador): no se promete resistencia a
  quien mida tiempos desde la misma red.

Las funciones (`TlsAbrir`, `TlsConectar`, `TlsLeer`, `TlsEscribir`,
`TlsCerrar`, `TlsError`, `TlsTextoError`, `TlsInfo`, `TlsAzar`,
`TlsCiclos`) van sobre un socket de WSOCK32 sin bloqueo; están descritas al
principio de `w32/tls/hsptls.c`. Los errores salen en español: «El
certificado del servidor ha caducado o aún no vale. Compruebe la fecha del
equipo.», «...no lo firma ninguna autoridad conocida.», «El certificado es de
otro sitio, no del que se ha pedido.».

### Lo que cuesta el apretón de manos

Medido en QEMU con `-icount` (cuenta instrucciones; `HTTPS /V` las enseña) y
contando en el anfitrión las multiplicaciones de 32×32 bits de cada
operación (BearSSL, implementaciones «m31» e «i31»):

| | Instrucciones | Multiplicaciones | 486DX2-66 (estimado) |
|---|---|---|---|
| Firma ECDSA P-384 (comprobar): cada certificado de la cadena de clicgest.es | 91 M | 4,8 M | 6,5 s |
| Firma ECDSA P-256 (comprobar): la del intercambio de claves | 13 M | 0,48 M | 0,8 s |
| Curve25519 (cada una de las dos del intercambio) | 2,6 M | 0,2 M | 0,2 s |
| **Apretón de manos con clicgest.es, la primera vez** (dos P-384) | **216 M** | 10,5 M | **unos 15 s** |
| **El mismo, con la cadena recordada** | **22 M** | 0,9 M | **1,4 s** |

La estimación: 2,75 ciclos por instrucción corriente y 40 por multiplicación
(en un 486, `MUL` de 32 bits tarda de 13 a 42 ciclos según el multiplicador),
a 66 MHz. En un Pentium a 100 MHz, la tercera parte más o menos. En QEMU en
este servidor, 0,3 s la primera vez. El cifrado de los datos
(ChaCha20-Poly1305) no se ha medido aparte; son operaciones de 32 bits sin
tablas.

### Tamaños y memoria

| | En disco | |
|---|---|---|
| `HW32.COM` | 32 KB | (+0,6 KB por las tres funciones de red) |
| `WSOCK32.DLL` | 23 KB | 24 KB de búferes por conexión TCP abierta |
| `HSPTLS.DLL` | 87 KB | 37 KB por conexión cifrada (los búferes de TLS y la cadena) |
| `HTTPS.EXE` | 7 KB | |
| Memoria convencional | | 19 KB con la red abierta (el anillo), además de los 16 KB de HW32 |

### Pruebas

`tests/run_red32_tests.sh` (sin Internet: servidor propio en el anfitrión,
`tests/tools/nav_servidor.py`, con certificados hechos en la prueba por
`tests/tools/nav_certs.sh`): DHCP, ARP y una descarga de 150 KB con su suma
(RTL8139 y NE2000), conexión rechazada, sockets sin bloqueo con `select`,
redirecciones, descargas cifradas ECDSA (AES-256-GCM) y RSA
(ChaCha20-Poly1305), el certificado recordado, guardar en un archivo, los
certificados que no valen (de otro nombre, sin autoridad conocida, caducado,
sin la raíz de pruebas), el reloj mal puesto, un 404, un servidor que no
existe y que después HSPRED se descarga (los programas soltaron la red).

## Memoria

| | Residente en total | de ello, búferes de DMA | Antes del 16/11/2026 |
|---|---|---|---|
| HSPRED con la NE2000 | 4.992 octetos (código, datos, la pila de 512 y el PSP) | ninguno | 5.040 |
| HSPRED con la RTL8139 | 14.736 | 9.744 (anillo de 8 KB y uno de envío) | 14.784 |
| HSPRED con la e1000 o la 82574L | 15.056 | 9.984 (descriptores y 4 búferes de 2 KB) | unos 15.100 |
| HSPRED con la RTL8168 | unos 13.400 | 8.192 (descriptores y búferes) | unos 13.500 |
| HSPRED con la PCnet | 13.424 | 7.840 (descriptores, bloque de inicialización, 4 búferes de recepción y uno de envío) | — |
| HSPRED con la Tulip | 13.328 | 7.776 (descriptores, 4 búferes de recepción y uno de envío) | — |
| HSPRED con un adaptador USB (CDC-ECM) | 5.152 | ninguno (los búferes son de HSPUSB: ver USB.md) | — |
| HSPRED con la Marvell Yukon | de 16.368 a 18.928 (16.512 en la prueba) | de 9.840 a 12.400 (tres anillos, cada uno al principio de una página, y 4 búferes de recepción y uno de envío repartidos por los huecos, también el de antes del primer anillo) | — |

La PCnet, la Tulip, el adaptador USB y la Yukon no les cuestan nada a las demás: su
parte residente (582, 556, 152 y 1.516 octetos) va en cuatro secciones «de quita y
pon» del archivo, ensambladas las cuatro para la misma dirección (justo detrás de la pila), y la instalación
deja en la memoria solo la de la tarjeta encontrada, si es una de ellas. Las
otras cuatro siguen juntas en la parte común (se podrían pasar al mismo
sistema: la NE2000 bajaría a unos 3 KB). Los nombres de las tarjetas ya no
son residentes (se copia el de la encontrada): de ahí los 48 octetos menos.

El adaptador USB tampoco gasta nada de lo común: como no tiene interrupción,
la rutina de interrupción de HSPRED se queda en un vector libre (del 80h
hacia abajo) al que no llama nadie, y así la descarga y lo demás son iguales
que con una tarjeta PCI.

La Yukon es la que más ocupa: sus tres anillos empiezan cada uno en una
página de 4 KB (así se los da Linux a la tarjeta; no se sabe si le vale con
menos), y entre el final del código y la primera página queda un hueco que
depende de dónde cargue DOS el programa. `yk_layout` pone los anillos en el
orden que deja más sitio (estado, recepción, envío) y mete los cinco búferes
en los huecos, empezando por ése (antes era relleno perdido: 10.240 a 14.320
octetos de DMA; ahora, 9.840 a 12.400). Con `LH`, el bloque de la convencional
se recorta (AH=4Ah) a lo que se usa. Cuando funcione en el equipo de verdad
se podrá probar a quitar la alineación (y a usar 2 búferes de recepción en
vez de 4): ver «Pendiente».

El archivo `HSPRED.COM` ocupa 21,0 KB (14,6 antes de la Yukon, de los que
1,5 son su parte residente y el resto su instalación, su informe de `/INFO`
y `/DEPURA`; lo que no es residente se descarta al instalarse). Cada orden de red, de 6 a 8 KB (la pila TCP/IP
va dentro).

Con `LH`, el código va en la memoria superior y los búferes de DMA (las
tarjetas con *bus master*: todas menos la NE2000), en un bloque aparte de la convencional: en modo
V86 la memoria superior no es memoria física continua, y la tarjeta escribe
en la memoria física. La dirección física se pide por VCPI (DE06h) y se
comprueba que el bloque es continuo. Medido en QEMU con HSPEMM: `LH HSPRED`
deja 5.008 octetos en la memoria superior (5.600 con la PCnet, 5.568 con la
Tulip) y, con la RTL8139, 9.776 en la convencional (los búferes; 7.872 con
la PCnet y 7.824 con la Tulip). Para cargarse arriba, `LH` necesita un hueco
del tamaño del archivo (21,0 KB desde que lleva la Yukon; antes, 14,6); si no
lo hay, se queda abajo.

## Pruebas

`tests/run_red_tests.sh` (en `make test`), con la red de usuario de QEMU
(`-netdev user -device ne2k_pci|rtl8139|e1000|pcnet|tulip|e1000e`), dentro de un espacio de
nombres de red propio (`unshare -rn`): no hace falta Internet ni se toca la
red del equipo. Arranca un servidor HTTP (`python3 -m http.server` en
127.0.0.1, que el invitado ve como 10.0.2.2:8080) y un DNS falso
(`tests/red_dns.py` en 127.0.0.1:53 = 10.0.2.2:53). Con cada tarjeta:
`HSPRED` y la MAC de QEMU; DHCP (10.0.2.15, puerta 10.0.2.2, DNS 10.0.2.3);
`PING 10.0.2.2`; `DESCARGAR` de un archivo de 150.000 octetos (se compara
con el original con mtools), un 404, una dirección `https://` y una
redirección; DNS (PING y DESCARGAR por nombre, y un nombre que no existe);
`RED.INI` con `DHCP=NO`; `HSPRED /U`. Y con HSPEMM (modo V86): `LH HSPRED`
con la NE2000, la RTL8139, la PCnet, la Tulip y la 82574L (en la memoria
superior, con los búferes en la convencional) y la e1000, que lo dice y no
se carga. Tres QEMU a la vez (`PARALELO=2`: de dos en dos); unos 8 minutos. `TARJETAS="pcnet tulip"` prueba
solo esas, `SOLO=tarjetas` deja fuera la parte de HSPEMM, `SOLO=v86` hace
solo ésa, `GUARDAR=carpeta` deja allí lo que salió en la pantalla de cada
QEMU y `VOLCAR=carpeta`, las tramas de cada uno (pcap, para `tcpdump -r`). Si no se puede crear el
espacio de nombres ni usar el puerto 53, se saltan las pruebas de DNS (lo
dice).

`tests/run_yukon_tests.sh` (en `make test`): la Marvell Yukon 88E8059, que
QEMU no emula. `tests/yukon_sim.py` hace con KVM (`/dev/kvm`; sin él, la
prueba se salta) una máquina mínima en modo real —1 MB de memoria, el PIC y
el temporizador del núcleo— donde corre `HSPRED.COM` tal cual: lo que pide al
DOS, a la BIOS y a la BIOS PCI lo contesta el propio programa de prueba, y
los registros de la tarjeta (en memoria y por puertos con RAP) los contesta
un **modelo del chip** escrito con lo que se sabe por `sky2`: PHY por SMI con
su autonegociación, unidades de precarga con sus elementos de lista, anillo
de estado, interrupción con su «entrar y salir», ventana de la configuración
PCI. El modelo vigila al controlador (anillos alineados, elementos con el
bit de «vale», índices dentro del anillo, escrituras con el chip en
reinicio, PHY usado en reinicio, configuración PCI escrita sin permiso). Se
prueba, por memoria y por puertos, con las revisiones 0 y 1 del chip, a 100
y a 1000 Mbit/s, con cable y sin él, con IRQ y con `/SONDEO`: carga y MAC,
`/DEPURA` al archivo, anillos y búferes sin pisarse entre ellos ni a lo
residente, máscara de interrupciones, enlace (también
quitar y poner el cable en marcha), `send_pkt` (60 octetos mínimos, la
vuelta al anillo, sin enlace dice que no), 40 tramas recibidas de varios
tamaños de una en una y a ráfagas (la vuelta a los anillos de recepción y
de estado), las que no son para la tarjeta o traen error, `HSPRED /INFO` con
el residente (también con tramas llegando mientras escribe: sin el aviso
falso del 18/11; y con la IRQ cortada: con el aviso), `/U` (la tarjeta queda en reinicio y los vectores como
estaban) y volver a cargar; también cargado arriba (como con `LH`: los
búferes en un bloque de la convencional, recortado a lo que usa); y que otro chip de la familia, o una tarjeta que
no contesta, lo dicen y no se quedan. Unos 15 segundos. **Lo que no prueba:**
que el chip de verdad se porte como el modelo (está hecho con las mismas
suposiciones que el controlador): eso, solo en el HP Mini.

`tests/run_red_usb_tests.sh` (en `make test`): la red por USB, con
`-device usb-net` de QEMU (RNDIS en su configuración 1, CDC-ECM en la 2), en
el mismo espacio de nombres de red y con los mismos servidores. El adaptador
en un puerto raíz y detrás de un concentrador (`usb-hub`), con xHCI, UHCI y
OHCI; con un EHCI y sus UHCI compañeras (el EHCI cede el adaptador, que es
de USB 1.1, a una UHCI); con teclado, ratón y memoria USB a la vez (se
descarga a la memoria USB y se copia de ella); con HSPEMM (`DEVICEHIGH` de
HSPUSB y `LH HSPRED`); un adaptador «sin CDC-ECM» (HSPUSB ensamblado con
`-DRED_SIN_ECM`); `/2` con el xHCI de Intel simulado (`-DINTEL_FAKE`); con
una tarjeta PCI además (HSPRED elige la PCI; `HSPRED /USB`, el adaptador);
`CONSOLA` (un servidor TCP) por el adaptador, con el cliente de
`tests/run_consola_tests.sh`; la memoria que queda libre con el adaptador
y sin él; y que el adaptador no deja a nadie sin teclado (tecleando por el
teclado USB con el `sendkey` del monitor de QEMU): el teclado en un EHCI con
el xHCI lleno, como en el MacBook Air (`tecladoehci`), dos teclados y dos
ratones a la vez (`dosteclados`, `dosratones`), 10 aparatos en uso
(`muchos`), y el adaptador sin memoria (`sinsitio`) o el EHCI sin lista
periódica (`sinlista`). En cada caso de red:
`HSPUSB /INFO`, `HSPRED`, `HSPRED /INFO`, `CONFRED` (DHCP), `PING`,
`DESCARGAR` (150.000 octetos comparados con mtools, y un 404), `PING` por
nombre, `HSPRED /U` y volver a cargarlo. Dos QEMU a la vez; unos 5 minutos.
`CASOS="xhci uhcihub"` hace solo ésos. Lo que no puede probar: ver «Límites»
de los adaptadores USB.

### Un fallo de la pila que sacó la PCnet (arreglado)

Con la PCnet la prueba de la redirección fallaba más de la mitad de las veces
(«El servidor no contesta»). No era la tarjeta: `tcp_abrir` (`lib/red.inc`)
sacaba el puerto local y el número de secuencia inicial solo del reloj de la
BIOS (tics de 55 ms), y dos conexiones abiertas en el mismo tic (la segunda
de una redirección, con una tarjeta que envía rápido) salían del mismo puerto
con el mismo número: el servidor tomaba el SYN por uno repetido de la
conexión recién cerrada y no contestaba (se ve en la captura:
`VOLCAR=carpeta tests/run_red_tests.sh` deja un `.pcap` por QEMU). Ahora se
suma un contador de conexiones. Con las otras tarjetas no salía porque
tardan más de un tic entre una conexión y la siguiente.

## Pendiente

- **Probar en VirtualBox (PCnet), Hyper-V (Tulip) y VMware**: lo de esas
  tarjetas solo se ha probado contra QEMU (ver «En una máquina virtual»).
- **Probar en más equipos de verdad**, sobre todo la RTL8168 del A275, y
  añadir lo de las 8111 modernas (ver la tabla de la RTL8168).
- **PCnet en modo de 16 bits (WIO)**, de reserva, por si alguna emulación
  (VMware) no admite el de 32; y Tulip de verdad: elección del medio por la
  SROM (10BASE-T, MII) y estado del enlace.
- **Marvell Yukon 88E8059** (HP Mini 5103): **probarla en el equipo** con
  la memoria USB de `tools/prueba-cable` (ver su apartado). Según salga:
  quitar lo que sobre (¿hace falta de verdad que los anillos empiecen en
  página?: son unos 2,5 KB más), saber si los registros por puertos
  (RAP) funcionan (para usarla con HSPEMM) y, si la interrupción va, decidir
  si `/SONDEO` se queda.
- **Broadcom tg3** (el adaptador Thunderbolt de los Mac), a partir del
  controlador de Linux.
- **Adaptadores USB**: probar el Realtek RTL8153 de verdad (MacBook Air), y
  CDC-NCM, los modos propios (RTL8152/8153, AX88179) y RNDIS: ver «Lo que
  falta» en «Adaptadores USB-Ethernet».
- **Más tarjetas**: Intel e1000e integradas (ICH8 en adelante: I217/I218/I219,
  con otro PHY), virtio-net (para las máquinas virtuales), 3Com 3c59x/3c90x.
  Tarjetas ISA (NE2000 ISA, 3c509) con su dirección en la línea de órdenes.
- **Wi-Fi**: **Decisión (Carlos, 17/11/2026): en Hispa-386 no se hace Wi-Fi**
  (HSPRED solo enseña la tarjeta); es de Hispa-486. Hecho el 18/11/2026 para
  la Broadcom BCM43224, con WPA2-Personal: `HSPWIFI` (otro controlador de paquetes, con
  la misma interfaz que HSPRED: las órdenes de red funcionan igual) y la
  orden `WIFI`. Ver [WIFI.md](WIFI.md). Falta probarlo en la tarjeta de
  verdad, y la Realtek RTL8822BE.
- **Marvell Yukon 88E8059** (11AB:4381, la de cable del HP Mini 5103): el
  17/11/2026 se dejó para Hispa-Pentium; **el 18/11/2026 Carlos pidió que
  funcione en todas las gamas** (Hispa-386 y por encima; además sirve para
  depurar el HP Mini a distancia con CONSOLA mientras se hace su Wi-Fi), y
  está en HSPRED desde ese día, pendiente de la prueba en el equipo.
- **WINSOCK.DLL (Winsock 1.1)** para Windows para Trabajo en Grupo 3.11 y
  Hispa Ventanas: una DLL de 16 bits (Open Watcom) sobre una pila TCP/IP en
  la memoria (la de `lib/red.inc` hecha residente, con varias conexiones y
  sockets UDP), para los navegadores y clientes de correo de la época. Para
  Windows 3.11 hay que ver también cómo convive con su propia pila de
  Microsoft (que usa NDIS, no controladores de paquetes).
- **Pila residente** con varias conexiones a la vez, ventana que crece y
  renovación de la concesión de DHCP.
- ~~Un navegador para Hispa Ventanas~~: hecho en Hispa-486 (18/11/2026,
  docs/486/NAVEGADOR.md), con HTTPS. Queda un cliente
  de FTP.
- `PING` con milisegundos (el temporizador 8254, en vez del reloj de la
  BIOS).
- ~~HTTPS (TLS)~~: hecho para los programas Hispa32 (18/11/2026,
  `HSPTLS.DLL`: «La red de los programas Hispa32», arriba). Los programas de
  16 bits (`DESCARGAR`, `ACTUALIZAR`) siguen sin cifrar.
