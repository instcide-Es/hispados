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

1. Cargar el controlador, en `AUTOEXEC.BAT` o a mano: `HSPRED` (o
   `LH HSPRED` con memoria superior). Si no encuentra una tarjeta que sepa
   usar, lo dice, enseña las tarjetas de red que hay (fabricante:modelo) y no
   se queda en la memoria.
2. `CONFRED` (también `IPCONFIG`): pide la dirección por DHCP (o la pone de
   `RED.INI`) y la enseña. No hace falta llamarlo antes de las otras: la
   primera orden de red que se use la pide.
3. `PING nombre|dirección [/N:n] [/T]` y
   `DESCARGAR [http://]servidor[:puerto]/ruta [archivo]`.

| Orden | Para qué |
|---|---|
| `HSPRED [/INT:xx] [/INFO] [/U]` | El controlador. `/INFO`: tarjeta, PCI, puertos, IRQ, MAC, enlace y velocidad, vector, memoria, programas que lo usan, contadores y la IP (también con el controlador ya cargado). `/INT:xx`: el vector (60h-80h; si no, el primero libre desde el 60h, nunca el 67h de EMS). `/U`: descargarlo (si ningún programa lo usa). |
| `CONFRED [/RENOVAR] [/LIBERAR]` | La configuración IP. `/RENOVAR` (`/RENEW`) la pide otra vez; `/LIBERAR` (`/RELEASE`) la olvida. `/ALL` se acepta (como en Windows) y hace lo mismo que sin nada. |
| `PING destino [/N:n] [/T]` | Eco de ICMP: 4 veces (o *n*, o hasta Esc con `/T`), con el tiempo (de 55 en 55 ms: el reloj de la BIOS) y el TTL, y el resumen de paquetes perdidos. Código de salida 1 si no contestó ninguno. |
| `DESCARGAR url [archivo]` | HTTP/1.0 sin cifrar. Barra de progreso si el servidor dice el tamaño (si no, los octetos que van llegando). Sigue hasta 4 redirecciones a `http://`. Si no se dice el archivo, el último trozo de la ruta en 8.3 (`INDEX.HTM` si no hay). Si algo falla (404, disco lleno, Esc, el servidor corta), lo dice y borra el archivo a medias. |

**HTTPS no**: las direcciones `https://` (cifradas, hoy casi todas) se
rechazan con un mensaje claro, y también una redirección a `https://`. Para
TLS hace falta criptografía (RSA/ECDHE, AES/ChaCha, SHA-2) y certificados: es
un trabajo grande, más propio de Hispa 32. `DESCARGAR` sirve para servidores
de la red de casa (otro PC con `python3 -m http.server`, un NAS) y para los
sitios de archivos antiguos que siguen dando HTTP.

`AYUDA` las lista y `AYUDA CONFRED` (o `CONFRED /?`), `AYUDA PING` y
`AYUDA DESCARGAR` las explican.

## Tarjetas

| Tarjeta | PCI | Estado |
|---|---|---|
| NE2000 PCI (Realtek RTL8029 y compatibles: Winbond 89C940, Compex, VIA 86C926...) | `10EC:8029`... | ✅ probada en QEMU (`-device ne2k_pci`). Datos por el «DMA remoto» de la tarjeta: sin búferes en el PC, la que menos ocupa. Sin estado del enlace. |
| Realtek RTL8139 (y D-Link DFE-538TX, Accton) | `10EC:8139` | ✅ probada en QEMU (`-device rtl8139`). Anillo de recepción de 8 KB. |
| Intel PRO/1000 de la familia 8254x («e1000»: 82540EM, 82545EM, 82541...) | `8086:100E`... | ✅ probada en QEMU (`-device e1000`). Registros por los puertos (IOADDR/IODATA) en las tarjetas de verdad; en QEMU, que solo los da en memoria, por el modo «irreal» (no funciona en modo V86: con HSPEMM lo dice y no se carga). |
| Realtek RTL8111/8168, 8101E/8102E (8136), 8169 («r8169») | `10EC:8168`... | ⚠️ escrita a partir de la documentación pública de la familia, **sin probar** (QEMU no la tiene). Es la del Lenovo A275. |

### Las tarjetas de los portátiles de las pruebas

Sacado de los informes de `DIAGNOSTICO` (`INFORMES\...\INFORME.TXT` y los
volcados `PCI\*.BIN`, clase 02h):

| Equipo | Ethernet (cable) | Inalámbrica (Wi-Fi) |
|---|---|---|
| Lenovo A275 | **Realtek RTL8111EP/8168EP** (`10EC:8168`, rev. 0Eh, subsistema `17AA:511F`; en 01:00.0, puertos 2400h, IRQ 5; con las funciones DASH 01:00.1-4: dos puertos serie, IPMI y un EHCI) | Realtek RTL8822BE (`10EC:B822`, en 03:00.0, clase FFh) |
| HP Mini 5103 (los dos) | **Marvell Yukon 88E8059 «Yukon Optima»** (`11AB:4381`, rev. 11h, subsistema `103C:1608`) | Broadcom BCM43224 (`14E4:4353`) |
| MacBook Air 2012 | — (no tiene; el adaptador de Apple por Thunderbolt es una Broadcom BCM57762, `tg3`) | Broadcom BCM43224 (`14E4:4353`) |

- **A275**: la RTL8168 está en `HSPRED`, sin probar. Lo que falta: probarla
  en el equipo (`HSPRED /INFO`, `CONFRED`, `PING` a la puerta de casa); si no
  recibe, el primer sospechoso es el firmware DASH de las 8111EP, que en
  Linux necesita un aviso de «arranque del controlador» por los registros OCP
  (`rtl8168ep_driver_start`) y una puesta a punto del PHY que cambia en cada
  revisión del chip. Habría que leer el `TxConfig` (la versión del chip) en
  `/INFO` y añadir lo de esa versión.
- **HP Mini 5103**: la Marvell Yukon 88E8059 no tiene documentación pública
  (Marvell solo la daba con acuerdo de confidencialidad); la única fuente es
  el controlador `sky2` de Linux (GPL-2.0, compatible, unas 5.000 líneas, con
  muchas diferencias entre modelos). Es posible pero largo: se deja para más
  adelante. Mientras, un adaptador USB-Ethernet no sirve todavía (HSPUSB no
  tiene la clase de red) y una tarjeta PCMCIA/ExpressCard tampoco.
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

### La pila TCP/IP: lib/red.inc

Escrita aquí, desde las RFC (791 IP, 792 ICMP, 768 UDP, 793 TCP, 826 ARP,
2131 DHCP, 1035 DNS, 1945 HTTP/1.0); se incluye en cada programa (unos 5 KB
de código). Habla con cualquier packet driver. Tiene: Ethernet II; ARP
(pregunta, contesta y recuerda 4 direcciones); IPv4 (sin fragmentos);
ICMP (contesta al eco y hace eco); UDP; cliente de DHCP; cliente de DNS
(registros A); TCP con una conexión de cliente: abrir (con reintentos del
SYN), enviar esperando el acuse y reenviando cada segundo, recibir en orden
con acuse inmediato (lo desordenado se tira y se repite el acuse, y el otro
lado reenvía), cerrar con FIN y RST. Ventana fija de 5.840 octetos (4
segmentos), MSS 1.460. Los paquetes que llegan esperan en una cola de 8
(los rellena el receptor desde la interrupción) y el programa los atiende en
`red_sondear`. Las rutinas están descritas al principio de `lib/red.inc`.

## Memoria

| | Residente en total | de ello, búferes de DMA |
|---|---|---|
| HSPRED con la NE2000 | 5.040 octetos (código, datos, la pila de 512 y el PSP) | ninguno |
| HSPRED con la RTL8139 | 14.784 | 9.744 (anillo de 8 KB y uno de envío) |
| HSPRED con la e1000 | unos 15.100 | 9.984 (descriptores y 4 búferes de 2 KB) |
| HSPRED con la RTL8168 | unos 13.500 | 8.192 (descriptores y búferes) |

El archivo `HSPRED.COM` ocupa 10 KB (la mitad es la instalación, que se
descarta). Cada orden de red, de 6 a 8 KB (la pila TCP/IP va dentro).

Con `LH`, el código va en la memoria superior y los búferes de DMA (las tres
tarjetas con *bus master*), en un bloque aparte de la convencional: en modo
V86 la memoria superior no es memoria física continua, y la tarjeta escribe
en la memoria física. La dirección física se pide por VCPI (DE06h) y se
comprueba que el bloque es continuo. Medido en QEMU con HSPEMM: `LH HSPRED`
deja 5.056 octetos en la memoria superior y, con la RTL8139, 9.776 en la
convencional (los búferes). Para cargarse arriba, `LH` necesita un hueco del
tamaño del archivo (10 KB): en QEMU con la ROM de arranque por red de la
tarjeta solo quedan 8 KB de memoria superior y se queda abajo (con
`romfile=` vacío, como en un portátil con el arranque por red apagado,
quedan 12 KB y cabe).

## Pruebas

`tests/run_red_tests.sh` (en `make test`), con la red de usuario de QEMU
(`-netdev user -device ne2k_pci|rtl8139|e1000`), dentro de un espacio de
nombres de red propio (`unshare -rn`): no hace falta Internet ni se toca la
red del equipo. Arranca un servidor HTTP (`python3 -m http.server` en
127.0.0.1, que el invitado ve como 10.0.2.2:8080) y un DNS falso
(`tests/red_dns.py` en 127.0.0.1:53 = 10.0.2.2:53). Con cada tarjeta:
`HSPRED` y la MAC de QEMU; DHCP (10.0.2.15, puerta 10.0.2.2, DNS 10.0.2.3);
`PING 10.0.2.2`; `DESCARGAR` de un archivo de 150.000 octetos (se compara
con el original con mtools), un 404, una dirección `https://` y una
redirección; DNS (PING y DESCARGAR por nombre, y un nombre que no existe);
`RED.INI` con `DHCP=NO`; `HSPRED /U`. Y con HSPEMM (modo V86): `LH HSPRED`
con la NE2000 y la RTL8139 (en la memoria superior, con los búferes en la
convencional) y la e1000, que lo dice y no se carga. Tres QEMU a la vez;
unos 4 minutos. `SOLO=v86` hace solo la parte de HSPEMM y `GUARDAR=carpeta`
deja allí lo que salió en la pantalla de cada QEMU. Si no se puede crear el
espacio de nombres ni usar el puerto 53, se saltan las pruebas de DNS (lo
dice).

## Pendiente

- **Probar en equipos de verdad**, sobre todo la RTL8168 del A275.
- **Marvell Yukon** (HP Mini 5103) y **Broadcom tg3** (el adaptador
  Thunderbolt de los Mac), a partir de los controladores de Linux.
- **Más tarjetas**: Intel e1000e (82574, ICH8 en adelante), virtio-net (para
  las máquinas virtuales), 3Com 3c59x/3c90x, AMD PCnet (VirtualBox y VMware).
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
