# Red en Hispa-DOS

Primer paso de la red (14/11/2026): un controlador residente con la interfaz
de *packet driver* (`HSPRED`), una pila TCP/IP propia y tres órdenes:
`CONFRED` (`IPCONFIG`), `PING` y `DESCARGAR`. El objetivo es llegar a lo que
tenía Windows para Trabajo en Grupo 3.11 con TCP/IP: ver «Pendiente».

```
A:\>HSPRED
HSPRED 0.1: controlador de red de Hispa-DOS (packet driver)
Tarjeta Realtek RTL8139, MAC 52:54:00:12:34:56, IRQ 11, packet driver en INT 60h

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
| `HSPRED [/INT:xx] [/INFO] [/USB] [/U]` | El controlador. `/USB`: usar el adaptador USB-Ethernet (el que lleva HSPUSB) aunque haya una tarjeta PCI que valga. `/INFO`: tarjeta, PCI (con la revisión), puertos, el chip exacto (familia de la RTL8168 por su `TxConfig`, número de pieza de la PCnet), IRQ, MAC, enlace y velocidad, vector, memoria, programas que lo usan, contadores y la IP (también con el controlador ya cargado). `/INT:xx`: el vector (60h-80h; si no, el primero libre desde el 60h, nunca el 67h de EMS). `/U`: descargarlo (si ningún programa lo usa). |
| `CONFRED [/RENOVAR] [/LIBERAR]` | La configuración IP. `/RENOVAR` (`/RENEW`) la pide otra vez; `/LIBERAR` (`/RELEASE`) la olvida. `/ALL` se acepta (como en Windows) y hace lo mismo que sin nada. |
| `PING destino [/N:n] [/T]` | Eco de ICMP: 4 veces (o *n*, o hasta Esc con `/T`), con el tiempo (de 55 en 55 ms: el reloj de la BIOS) y el TTL, y el resumen de paquetes perdidos. Código de salida 1 si no contestó ninguno. |
| `DESCARGAR url [archivo]` | HTTP/1.0 sin cifrar. Barra de progreso si el servidor dice el tamaño (si no, los octetos que van llegando). Sigue hasta 4 redirecciones a `http://`. Si no se dice el archivo, el último trozo de la ruta en 8.3 (`INDEX.HTM` si no hay). Si algo falla (404, disco lleno, Esc, el servidor corta), lo dice y borra el archivo a medias. |
| `CONSOLA [/PUERTO:n]` | Consola remota por red (servidor TCP, puerto 2323 por omisión): deja entrar a otra persona a este equipo, con un código de acceso de 6 cifras por conexión que solo se ve aquí. Abierta y controlada por el usuario. Ver [docs/CONSOLA.md](CONSOLA.md). |
| `ACTUALIZAR [url]` | Instalador por red. Mira por HTTP (`versiones.txt`) si hay una versión más nueva o una edición más rica que la instalada (`lib/version.inc`), según la CPU/RAM/disco del equipo; y, si el usuario quiere, **baja el paquete `.PAQ` y lo instala en el sitio** conservando `*.INI`/`*.REC`. La dirección sale, por orden, de lo que se escriba, de `ACTUALIZAR=` en `RED.INI`, o de la de por omisión. Ver más abajo. |

**HTTPS no**: las direcciones `https://` (cifradas, hoy casi todas) se
rechazan con un mensaje claro, y también una redirección a `https://`. Para
TLS hace falta criptografía (RSA/ECDHE, AES/ChaCha, SHA-2) y certificados: es
un trabajo grande, más propio de Hispa 32. `DESCARGAR` sirve para servidores
de la red de casa (otro PC con `python3 -m http.server`, un NAS) y para los
sitios de archivos antiguos que siguen dando HTTP.

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

Para subir una versión en el servidor: cambia `lib/version.inc` y
`actualiza.clicgest.es/versiones.txt`, y sube el `.PAQ` generado con
`tools/paquete.py`.

### Formato del paquete .PAQ

Lo genera `tools/paquete.py` a partir de una carpeta con la **misma estructura**
que tendrá el sistema (`KERNEL.SYS`, `COMMAND.COM`, `CONFIG.SYS`,
`AUTOEXEC.BAT` en la raíz y `HISPADOS\...` con sus subcarpetas). Sin compresión.
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

**Importante**: como no hay HTTPS (ver abajo) y **GitHub es todo HTTPS**, el
archivo y los paquetes no se pueden leer de GitHub: tienen que estar en un
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

### Adaptadores USB-Ethernet (CDC-ECM)

Desde el 17/11/2026, un adaptador USB-Ethernet que ofrezca la **clase
estándar CDC-ECM** es una tarjeta más de HSPRED. El aparato lo lleva
`HSPUSB.SYS` (enumeración, transferencias y sondeo: `lib/usb_red.inc`, ver
[USB.md](USB.md)) y HSPRED solo pone el puente con el packet driver. Vale
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
CONFIG.SYS:   DEVICE=A:\HISPADOS\HSPUSB.SYS /F /INFO
  ...
  5 SS 2109:0817 09/00/00 -> concentrador de 4 puertos
    5.1 SS 0BDA:8153 02/06/00 0A/00/00 -> red (CDC-ECM)
Adaptador de red USB preparado (CDC-ECM), MAC 00:E0:4C:68:xx:xx: cargue HSPRED.

A:\>HSPRED
Tarjeta Adaptador USB (CDC-ECM), MAC 00:E0:4C:68:xx:xx, por HSPUSB, packet driver en INT 60h
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

1. `HSPUSB.SYS` cargado en `CONFIG.SYS` **llevando él el USB** (con `/F` si
   la BIOS lo está usando; en un MacBook lo toma él solo) y el adaptador
   **conectado al arrancar** (no se busca después). HSPUSB dice
   «Adaptador de red USB preparado (CDC-ECM), MAC ...».
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
| HP Mini 5103 (los dos) | **Marvell Yukon 88E8059 «Yukon Optima»** (`11AB:4381`, rev. 11h, subsistema `103C:1608`) | Broadcom BCM43224 (`14E4:4353`) |
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
  muchas diferencias entre modelos). Es posible pero largo: se deja para más
  adelante. Mientras, sirve un adaptador USB-Ethernet con CDC-ECM (los
  Realtek RTL8152/8153: ver «Adaptadores USB-Ethernet»); una tarjeta
  PCMCIA/ExpressCard, no.
- **Wi-Fi** (las tres): nada por ahora (ver «Pendiente»).

Probar una tarjeta: `HSPRED /INFO` dice cuál ha encontrado y si hay enlace;
`CONFRED` hace el DHCP; `PING` a la puerta de enlace. Si no hay tarjeta que
sepa usar, `HSPRED` enseña las de red que hay con su `fabricante:modelo`,
para poder pedirla.

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

También se puede poner con ratón en **Hispa Ventanas**: Configuración ›
apartado **«Red»** (una casilla para el DHCP y, a mano, la IP, la máscara, la
puerta y el DNS). Escribe estas mismas claves en `RED.INI` (ver
docs/VENTANAS.md); los cambios se aplican al reiniciar o al recargar HSPRED.

La configuración que se consigue (por DHCP o de `RED.INI`) se guarda en
`HSPRED` y la usan todos los programas sin volver a pedirla, hasta
`CONFRED /RENOVAR` o reiniciar. La concesión de DHCP no se renueva sola (en
casa suele ser de un día o más): `CONFRED /RENOVAR`.

## API

### HSPRED: packet driver (especificación de FTP Software 1.09)

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
que usan *packet drivers*: mTCP (DHCP, FTP, Telnet, IRC, HTGET...), WATTCP
y lo que se hizo con él, Arachne... Sin probar todavía con ellos (no van en
el proyecto).

### HSPRED: INT 2Fh AH=DAh (para los programas de Hispa-DOS)

| AX | |
|---|---|
| DA00h | AL = FFh si está, BX = 5248h («HR»), CL = vector del packet driver, ES:DI = configuración IP compartida (30 octetos: +0 indicadores (bit 0 válida, bit 1 por DHCP), +2 IP, +6 máscara, +10 puerta, +14 DNS, +18 servidor DHCP, +22 concesión en segundos, +26 día, mes y año) |
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
de código). Habla con cualquier packet driver. Tiene: Ethernet II; ARP
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

La PCnet, la Tulip y el adaptador USB no les cuestan nada a las demás: su
parte residente (582, 556 y 152 octetos) va en tres secciones «de quita y
pon» del archivo, ensambladas las tres para la misma dirección (justo detrás de la pila), y la instalación
deja en la memoria solo la de la tarjeta encontrada, si es una de ellas. Las
otras cuatro siguen juntas en la parte común (se podrían pasar al mismo
sistema: la NE2000 bajaría a unos 3 KB). Los nombres de las tarjetas ya no
son residentes (se copia el de la encontrada): de ahí los 48 octetos menos.

El adaptador USB tampoco gasta nada de lo común: como no tiene interrupción,
la rutina de interrupción de HSPRED se queda en un vector libre (del 80h
hacia abajo) al que no llama nadie, y así la descarga y lo demás son iguales
que con una tarjeta PCI.

El archivo `HSPRED.COM` ocupa 14,6 KB (antes 13,7; lo que no es residente
se descarta al instalarse). Cada orden de red, de 6 a 8 KB (la pila TCP/IP
va dentro).

Con `LH`, el código va en la memoria superior y los búferes de DMA (las
tarjetas con *bus master*: todas menos la NE2000), en un bloque aparte de la convencional: en modo
V86 la memoria superior no es memoria física continua, y la tarjeta escribe
en la memoria física. La dirección física se pide por VCPI (DE06h) y se
comprueba que el bloque es continuo. Medido en QEMU con HSPEMM: `LH HSPRED`
deja 5.008 octetos en la memoria superior (5.600 con la PCnet, 5.568 con la
Tulip) y, con la RTL8139, 9.776 en la convencional (los búferes; 7.872 con
la PCnet y 7.824 con la Tulip). Para cargarse arriba, `LH` necesita un hueco
del tamaño del archivo (13,7 KB); si no lo hay, se queda abajo.

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
se carga. Tres QEMU a la vez; unos 8 minutos. `TARJETAS="pcnet tulip"` prueba
solo esas, `SOLO=tarjetas` deja fuera la parte de HSPEMM, `SOLO=v86` hace
solo ésa, `GUARDAR=carpeta` deja allí lo que salió en la pantalla de cada
QEMU y `VOLCAR=carpeta`, las tramas de cada uno (pcap, para `tcpdump -r`). Si no se puede crear el
espacio de nombres ni usar el puerto 53, se saltan las pruebas de DNS (lo
dice).

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
`tests/run_consola_tests.sh`; y la memoria que queda libre con el adaptador
y sin él. En cada caso de red:
`HSPUSB /INFO`, `HSPRED`, `HSPRED /INFO`, `CONFRED` (DHCP), `PING`,
`DESCARGAR` (150.000 octetos comparados con mtools, y un 404), `PING` por
nombre, `HSPRED /U` y volver a cargarlo. Dos QEMU a la vez; unos 3 minutos.
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
- **Marvell Yukon** (HP Mini 5103) y **Broadcom tg3** (el adaptador
  Thunderbolt de los Mac), a partir de los controladores de Linux.
- **Adaptadores USB**: probar el Realtek RTL8153 de verdad (MacBook Air), y
  CDC-NCM, los modos propios (RTL8152/8153, AX88179) y RNDIS: ver «Lo que
  falta» en «Adaptadores USB-Ethernet».
- **Más tarjetas**: Intel e1000e integradas (ICH8 en adelante: I217/I218/I219,
  con otro PHY), virtio-net (para las máquinas virtuales), 3Com 3c59x/3c90x.
  Tarjetas ISA (NE2000 ISA, 3c509) con su dirección en la línea de órdenes.
- **Wi-Fi**: las tarjetas de los portátiles (Broadcom BCM43224, Realtek
  RTL8822BE) necesitan su firmware, la gestión 802.11 y WPA2 (cifrado AES y
  el acuerdo de claves). No hay nada parecido en DOS; es para mucho después,
  o para Hispa 32.
- **WINSOCK.DLL (Winsock 1.1)** para Windows para Trabajo en Grupo 3.11 y
  Hispa Ventanas: una DLL de 16 bits (Open Watcom) sobre una pila TCP/IP en
  la memoria (la de `lib/red.inc` hecha residente, con varias conexiones y
  sockets UDP), para los navegadores y clientes de correo de la época. Para
  Windows 3.11 hay que ver también cómo convive con su propia pila de
  Microsoft (que usa NDIS, no packet drivers).
- **Pila residente** con varias conexiones a la vez, ventana que crece y
  renovación de la concesión de DHCP.
- **Un navegador** para Hispa Ventanas (HTML sencillo por HTTP) y un cliente
  de FTP.
- `PING` con milisegundos (el temporizador 8254, en vez del reloj de la
  BIOS).
- HTTPS (TLS): ver arriba.
