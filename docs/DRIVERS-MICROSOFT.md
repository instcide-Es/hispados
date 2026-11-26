# Los controladores de Microsoft en la familia Hispa-DOS

*Estudio y plan · 26 de noviembre de 2026*
*Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.*

> **El encargo de Carlos:** «Vamos a hacer todos los sistemas operativos que
> tenemos compatibles con los drivers de Microsoft. Empiezas desde abajo
> hasta arriba.»

Es decir: que cada Hispa pueda cargar los controladores **de su época**
(los de Microsoft y, sobre todo, los de los fabricantes, que se escribían
para los modelos de Microsoft) cuando no haya uno nuestro. Así, una tarjeta
de red, de sonido o una controladora SCSI para la que no tenemos controlador
propio sigue sirviendo con el disquete que traía.

Este documento dice, para cada sistema, qué modelos de controlador le
corresponden, qué hay ya, qué falta, cuánto cuesta y qué ganaríamos. Al
final, el orden de los hitos, de abajo arriba. El primero (Hispa-8086) está
**hecho** (rama `h8086-drivers`, ver «Hito 1»).

## Las reglas (las de siempre, y aquí más que nunca)

- **Nada de código de Microsoft**: ni copiado, ni desensamblado, ni de
  filtraciones, ni «mirado para entenderlo». Tampoco el código de MS-DOS que
  Microsoft ha publicado con licencia MIT: el proyecto es de sala limpia y se
  queda así.
- **Solo documentación pública**: los manuales de MS-DOS (*Programmer's
  Reference*, el capítulo de los controladores de dispositivo), la *Ralf
  Brown's Interrupt List*, la especificación de los «packet drivers» de FTP
  Software (1.09), la de NDIS 2.0 que publicaron 3Com y Microsoft, la DDK y la
  WDK tal como están documentadas en MSDN/Microsoft Learn, y libros como
  *Writing MS-DOS Device Drivers* (Lai) o *Undocumented DOS* (Schulman), que
  cuentan el comportamiento.
- **Código ajeno, solo con licencia compatible con GPL-2.0**: FreeDOS (GPL),
  ReactOS (GPL-2.0 o posterior), ndiswrapper (GPL). Wine es LGPL: se puede
  estudiar y usar como biblioteca aparte, pero no mezclar su código con el
  del núcleo sin pensarlo. Si se reutiliza algo, se dice en el commit.
- **Los controladores de otros no entran en el repositorio.** Cada usuario
  usa los suyos (los del disquete de su tarjeta). Para **probar**, solo
  controladores libres: los «packet drivers» de Crynwr (GPL), los de FreeDOS
  (GPL), los de ReactOS compilados para Windows (GPL). La batería los
  descarga a una carpeta fuera del repositorio (`h8086/prueba/trae_drivers.sh`)
  y, si no están, se salta esas partes diciéndolo.

## Los modelos de controlador, época por época

| Época | Modelos de Microsoft (y de la industria) | Qué se cargaba así |
|---|---|---|
| DOS 2-6 (PC, XT, AT) | **controladores de dispositivo** `.SYS` (de carácter y de bloque, con DEVICE= en CONFIG.SYS); **programas residentes** (INT 27h/31h); **packet drivers** (FTP Software) y **NDIS 2** (3Com/Microsoft) para la red; **MSCDEX** + controlador de CD; ratón (INT 33h); EMS/XMS | discos SCSI y de quitar y poner, ANSI, relojes de tarjeta, red, CD-ROM, ratón, escáneres |
| Windows 3.1x | `.386` (VxD de 32 bits del modo mejorado) y `.DRV` (de 16 bits: pantalla, impresora, sonido MMSYSTEM, red) | todo lo de Windows 3.11 |
| Windows 95/98/ME | **VxD** (`.VXD`, `.386`) y **WDM 1.0/1.1** (`.SYS`, desde 98); NDIS 3/4/5 | la mayoría del hardware de 1995-2001 |
| Windows XP | **WDM** (`.SYS` con IRP, `ntoskrnl.exe`, `hal.dll`), **NDIS 5.1**, StorPort/SCSIPort, KS (sonido), USB; **no KMDF** (llegó con Vista, como añadido en XP) | el hardware de 2001-2009, el último con controladores de 32 bits |
| Windows 10/11 x64 | WDM, **KMDF/UMDF 2**, **NDIS 6.x**, WDDM (gráficos), StorPort; **firma obligatoria** (Microsoft firma los de terceros) | lo de hoy |

## Sistema por sistema

### Hispa-8086 (PC, XT y AT; gama DOS de los 80)

**Le corresponde**: lo de DOS 2 a 3.3, en lo que cabe en un 8086: los
controladores `.SYS` de carácter y de bloque, los residentes, los packet
drivers, el ratón (INT 33h) y, como mucho, un CD-ROM con MSCDEX (raro en un
XT).

**Lo que había** (0.14): DEVICE= cargaba los `.SYS` binarios, de carácter y
de bloque, con su cabecera y su INIT; las lecturas y escrituras de sector
(órdenes 4 y 8); residentes con 31h e INT 27h. Faltaba lo que piden los de
verdad: el cambio de disco, IOCTL, la lista de listas, la consola por un
ANSI.SYS, los `.EXE` y llamarlos como los llama DOS.

**Lo que hay ahora** (rama `h8086-drivers`, «Hito 1»): todo eso, y
probado con controladores libres de verdad: el packet driver NE2000 de
Crynwr con la NE2000 de QEMU (manda y recibe), NANSI.SYS (un ANSI.SYS de
FreeDOS) como consola, SRDISK (un disco en memoria XMS de FreeDOS, comprimido
con UPX) y CuteMouse. Ver «Hito 1».

**Lo que falta** (pequeño): CLOCK$ (los relojes de tarjeta de los XT, con su
`CLOCK.SYS`), abrir y cerrar dispositivos de carácter (órdenes 13 y 14),
INT 28h (los residentes que trabajan «cuando DOS espera», como un PRINT), la
interfaz de FCB y el redirector de INT 2Fh/11h (MSCDEX y la red de
Microsoft; este último no cabe en un 8086 con la regla de los 16 K: iría como
programa aparte).

**Qué se gana**: la red de los XT y AT (cualquier tarjeta con su packet
driver: 3Com, NE1000/2000, WD8003, SMC...), con la que funcionan mTCP y WATTCP
(FTP, telnet, IRC, HTTP); discos SCSI y de quitar y poner con su `.SYS`
(Iomega, Adaptec con ASPI); el ratón de cualquier marca; ANSI; discos en
memoria; los relojes de tarjeta.

### Hispa-386 (MS-DOS 6.22 y Windows 3.11)

**Le corresponde**: todo lo de DOS 6.22 (lo del 8086, más DEVICEHIGH, UMB,
EMS/XMS, NDIS 2 con PROTMAN, MSCDEX) y lo de Windows 3.11: `.386` (VxD) y
`.DRV`.

**Lo que hay** (rama `worktree-v0.1-nucleo`, `kernel/kernel.asm`): DEVICE= y
DEVICEHIGH= con `.SYS` reales (carácter y bloque, también MZ), INIT, MEDIA
CHECK, lecturas y escrituras, lectura no destructiva, IOCTL de carácter
(4402h/4403h), la lista de listas completa (DPB, SFT de DOS 4, CLOCK$, CDS,
NUL), la SDA, SHARE, residentes, y memoria propia (HSPXMS, HSPEMM con VCPI y
UMB, HSPDPMI). **Windows 3.11 de verdad** (el del usuario) corre en modo 386
mejorado con sus propios VxD (IFSMGR, VCACHE...), y su MOUSE.DRV está
probado. Controladores propios: red (HSPRED, packet driver de NE2000 PCI,
RTL8139/8168, e1000, PCnet, Tulip, Yukon), sonido (HSPSND: SB, HD Audio,
AC'97), CD (HSPCD), USB (HSPUSB), disco en memoria (HSPRAM).

**Lo que faltaba y ya está** (rama `dos-drivers`, «Hito 2»): llamar al
controlador con DS:SI en su cabecera (sin eso, SRDISK de FreeDOS, comprimido
con UPX, **reiniciaba la máquina** al cargarlo, igual que en Hispa-8086),
BUILD BPB tras un cambio de disco y tratar «no se sabe» como MS-DOS (cambiado,
si no hay nada a medio escribir). Prueba: `tests/run_drivers_otros_tests.sh`.

**Lo que falta**:

1. El resto de **IOCTL de bloque con el controlador** (4404h/4405h, 4408h y
   440Dh pasados al `.SYS` cuando su cabecera dice que los entiende), para
   los de discos de quitar y poner (ZIP, magneto-ópticos, SCSI con ASPI).
   Está escrito (unos 350 octetos), pero **no cabe**: el núcleo de
   Hispa-386 está en el límite de KSEG (moviendo INIT_ORG, la imagen cabe
   pero `.init` se pasa del segmento). Hay que adelgazar el núcleo antes.
   *Pequeño, después de hacer sitio.*
2. **El redirector de INT 2Fh/11xx y la API de MSCDEX (15xx)**: hoy el CD lo
   lee el núcleo (ISO 9660 dentro). Con el redirector, funcionan MSCDEX y
   SHSUCDX con cualquier controlador de CD de fabricante (los `.SYS` ATAPI,
   SCSI y de puerto paralelo de los 90), y además la red de Microsoft
   (NET.EXE) y otros redirectores. *Mediano* (unas 25 funciones bien
   documentadas en la RBIL y en *Undocumented DOS*).
3. **NDIS 2**: hoy la red es por packet driver. NDIS 2 son PROTMAN.DOS (el
   gestor de protocolos, que es nuestro, escrito con la especificación de
   3Com/Microsoft) y el controlador `.DOS` de la tarjeta, que es del usuario;
   con DIS_PKT (GPL) encima, cualquier tarjeta con controlador NDIS sirve
   para todo lo que usa packet drivers. *Mediano.*
4. **INT 2Ah** (secciones críticas, pendiente) y las llamadas de INT 2Fh que
   usan los controladores de Windows (1605h ya está). *Pequeño.*
5. **Windows 3.11 con los controladores del fabricante**: como el Windows es
   el de Microsoft, sus `.386` y `.DRV` los carga Windows, no nosotros: lo
   que hay que asegurar es que el DOS que hay debajo se porta como espera
   WIN386 (y se va viendo con cada controlador). Probar: pantalla (los `.DRV`
   de Cirrus, S3, Trident de la época), sonido (Sound Blaster de Creative,
   que el ROADMAP pide «convivir con los originales»), red (Trabajo en Grupo
   con NDIS 2/3), impresoras (los `.DRV` de HP y Epson). *Mediano, de mucho
   probar.*

**Qué se gana**: CD-ROM de cualquier marca con su `.SYS`; red NDIS (tarjetas
sin packet driver); impresoras, pantallas y sonido en Windows 3.11 con sus
controladores originales.

**Cómo probarlo legalmente**: SHSUCDX y UDVD2 (FreeDOS, GPL) para el
redirector de CD; DIS_PKT (GPL) y los NDIS de ReactOS no sirven aquí
(son de NT): para NDIS 2 hay que escribir un `.DOS` de prueba propio para la
NE2000; los controladores de Windows 3.11, solo con los del usuario (no hay
libres de esa época salvo excepciones como los de VGA genérica).

### Hispa-486 (gama Windows 95/98, con HW32)

**Le corresponde**: VxD (Windows 95) y WDM 1.0 (Windows 98), y NDIS 3/4/5.

**Lo que hay** (misma rama): HW32, un cargador PE de **modo usuario**
(cliente DPMI, al estilo de HX): KERNEL32 (~180 funciones), USER32, GDI32 y
varias DLL. **No carga nada en modo núcleo** (ni `.SYS` ni `.VXD`).
Controladores propios: Wi-Fi (HSPWIFI: Broadcom BCM4322x/4313 con brcmsmac,
Atheros AR9285 y familia con ath9k, Realtek RTL8723BS), y los de red y
sonido de Hispa-386. docs/WIFI.md ya estudió el «como ndiswrapper» y lo dejó
«para más adelante»: pediría NDIS 5.1 más ntoskrnl/HAL, «más de 200
funciones», y «con código cerrado cada fallo es a ciegas».

**Qué haría falta**:

- **VxD: inviable con sentido.** Un VxD es parte del VMM de Windows 95
  (servicios VMM, VxDCall por INT 20h, el modelo de máquinas virtuales): para
  cargarlos habría que escribir un VMM compatible entero. Sin documentación
  completa pública y sin beneficio (lo que hay en VxD de esa época está
  también en WDM para 98). **Se descarta.**
- **WDM 1.0 y NDIS 5 en un «subsistema» dentro de HW32**: el `.SYS` es un PE
  que importa de `ntoskrnl.exe`, `hal.dll` y `ndis.sys`. Con HW32 ya se
  carga un PE; faltaría darle esas importaciones (IRP, objetos de
  dispositivo, DPC, IRQL, memoria contigua, spinlocks, puertos y E/S
  mapeada, interrupciones; y NDIS: miniport, paquetes, OID), corriendo en el
  anillo 0 de DPMI o con permisos de E/S. Es lo que hace ndiswrapper (GPL)
  en Linux, y su código **se puede estudiar y reutilizar**. *Grande: meses.*
  Lo que se gana: las tarjetas de red y Wi-Fi de 1998-2003 con su `.SYS`
  NDIS 5 (que son muchas). Sonido y USB con WDM: *muy grande* (KS, USBD),
  y para eso es mejor escribir el nuestro.
- **Cómo probarlo**: los miniport NDIS de ReactOS (GPL: `ne2000.sys`,
  `pcnet.sys`, `rtl8139.sys`) compilados para Windows; con la NE2000 y la
  PCnet de QEMU.

### Hispa-Pentium (gama Windows XP, núcleo propio en Castel)

**Le corresponde**: WDM de XP, NDIS 5.1, StorPort/SCSIPort, KS; no KMDF.

**Lo que hay**: WIN32.BIN carga PE de usuario en el anillo 3 (Win32 de modo
usuario, y las DLL redistribuibles de Visual C++ si el usuario las pide).
**Nada de modo núcleo de Windows**: ni IRP, ni HAL, ni exportaciones de
ntoskrnl. Controladores propios: discos (ATA, AHCI, ATAPI, SD/eMMC), USB
(xHCI, EHCI, UHCI, HID, almacenamiento, RNDIS/NCM), red (e1000, RTL8139,
RTL8111, Yukon), Wi-Fi (los de Hispa-486 más Intel `radio_iwl`), sonido (HD
Audio, AC'97, SB16), Bluetooth, I2C-HID. Sin impresoras.

**Qué haría falta**: un **«ndiswrapper» de Castel**: un cargador de `.SYS`
PE en el anillo 0 con las importaciones de ntoskrnl/HAL/NDIS de XP, como
módulo del núcleo. Es el mismo trabajo que el de Hispa-486, pero en mejor
sitio (núcleo propio de 32 bits, con interrupciones y memoria nuestras), y es
donde más rinde: **los `.SYS` NDIS 5.1 de Wi-Fi de XP** cubren cientos de
tarjetas de 2003-2009 (Intel 2100/2200/3945/4965, Broadcom, Realtek, Ralink,
Atheros) y las de red cableada. *Grande (meses)*, con dos avisos de
docs/WIFI.md que siguen valiendo: cada fallo de un controlador cerrado se
depura a ciegas, y lo que se gane hay que probarlo tarjeta a tarjeta.
Almacenamiento (StorPort/SCSIPort, para RAID de la época): *grande*, menos
útil (AHCI e IDE ya son nuestros). Sonido (KS/PortCls): *muy grande*; mejor
los nuestros. **Impresoras**: aquí sí conviene otra cosa: los controladores
de impresora de XP son DLL de modo usuario (GDI, «unidrv»), y no hacen falta
para las de hoy (IPP, PCL, PostScript): mejor un filtro propio.

**Cómo probarlo**: los miniport NDIS de ReactOS (GPL) en QEMU (NE2000,
PCnet, RTL8139, e1000 de ReactOS); en real, los de Wi-Fi del usuario.

### Hispa-64 (x86-64, POSIX)

**Le corresponde**: WDM/KMDF/NDIS 6 de Windows 10/11 x64.

**Lo que hay** (rama `hispa64`): núcleo propio en Castel 64, ELF64 y POSIX
con musl; NVMe, AHCI, xHCI, HID, almacenamiento USB, I2C-HID, red (Intel,
Realtek, virtio, USB CDC-ECM), HD Audio (OSS); impresión prevista con IPP
Everywhere, sin controladores. Nada de Windows.

**Lo honesto**: cargar los controladores de Windows 10/11 es **inviable** en
la práctica:

- **KMDF** es una biblioteca de Microsoft (Wdf01000.sys) que el controlador
  llama por una tabla de cientos de funciones; habría que reescribirla
  entera, con su modelo de objetos, colas y energía. Además está NDIS 6
  (NetAdapterCx en los nuevos), WDDM para los gráficos y PnP/energía
  completos.
- **La firma**: los `.SYS` de x64 los firma Microsoft; nosotros no tenemos
  que verificarla (no somos Windows), pero muchos controladores comprueban
  cosas del sistema (versión, servicios, el registro, WMI) y fallan si no
  las encuentran.
- Los fabricantes de hoy publican controladores para Linux (o los publica
  Linux): **portar de Linux** (GPL-2.0, como ya hacemos con ath9k y
  brcmsmac) es mucho más rentable que imitar Windows.

Lo que sí tiene sentido en Hispa-64: el **firmware** que los controladores
de Windows llevan dentro (el de las Wi-Fi Intel, Realtek, MediaTek), que se
puede cargar desde los archivos del usuario con nuestro controlador, igual
que hace Linux con `linux-firmware` (solo si su licencia lo permite
redistribuirlo; si no, que lo ponga el usuario).

## Los hitos, de abajo arriba

| # | Sistema | Hito | Esfuerzo | Lo que se gana |
|---|---|---|---|---|
| 1 | Hispa-8086 | `.SYS` como en DOS 3.3 (cambio de disco, IOCTL, 52h, `.EXE`, consola por ANSI.SYS, llamada con DS en la cabecera) y packet drivers probados | **hecho** (días) | red con cualquier packet driver; discos y ratón de fabricante; ANSI |
| 1b | Hispa-8086 | CLOCK$, órdenes 13/14, INT 28h; MSCDEX como programa aparte | pequeño (días) | relojes de tarjeta de XT, PRINT, CD en un AT |
| 2 | Hispa-386 | DS:SI en la cabecera al llamar (los `.SYS` con UPX), BUILD BPB y «no se sabe» (**hecho**, rama `dos-drivers`); IOCTL de bloque con el controlador (escrito, falta sitio en KSEG) | pequeño (días) | discos de quitar y poner SCSI/ZIP con su `.SYS`; los `.SYS` comprimidos |
| 3 | Hispa-386 | redirector INT 2Fh/11xx + API de MSCDEX | mediano (2-4 semanas) | cualquier CD-ROM de fabricante con MSCDEX/SHSUCDX; red de Microsoft |
| 4 | Hispa-386 | NDIS 2 (PROTMAN propio) + DIS_PKT | mediano (2-3 semanas) | tarjetas de red con controlador NDIS; Trabajo en Grupo |
| 5 | Hispa-386 | Windows 3.11 con los `.DRV`/`.386` de fabricante: probar pantalla, sonido, red e impresora, y arreglar lo que falle debajo | mediano, de mucho probar | Windows 3.11 completo en máquinas reales |
| 6 | Hispa-Pentium | «ndiswrapper» de Castel: `.SYS` NDIS 5.1 de XP en el anillo 0 (ReactOS para probar; ndiswrapper, GPL, para estudiar) | grande (2-4 meses) | cientos de tarjetas de red y Wi-Fi de 2003-2009 |
| 7 | Hispa-486 | lo mismo sobre HW32 (WDM 1.0/NDIS 5 de 98), aprovechando el del 6 | grande (1-2 meses tras el 6) | red y Wi-Fi de 1998-2003 |
| — | Hispa-486 | VxD de Windows 95 | **se descarta** (sería un VMM entero) | — |
| — | Hispa-64 | KMDF/NDIS 6/WDDM de Windows 10/11 | **se descarta** (inviable; portar de Linux rinde más) | — |

El orden pone el 6 delante del 7 a propósito: es el mismo trabajo y en
Hispa-Pentium está en su sitio (núcleo propio de 32 bits); en Hispa-486
se aprovecharía después, encima de DPMI.

## Hito 1, hecho: Hispa-8086 carga los controladores de DOS

Rama `h8086-drivers` (desde `hispa-8086`, la 0.14). Todo en
`h8086/config.inc`, `h8086/dispositivos.inc`, `h8086/disco.inc` y
`h8086/nucleo.asm`; el núcleo sigue por debajo de los 16 K.

Lo que se ha añadido, todo con la RBIL y los manuales:

- **La llamada, como la hace DOS**: estrategia e interrupción con ES:BX en
  la petición y **DS:SI en la cabecera del controlador**. Los controladores
  comprimidos con UPX (SRDISK de FreeDOS lo está) cuentan con ello: con DS en
  el núcleo, el descompresor se destrozaba y la máquina se reiniciaba.
- **INIT como en MS-DOS 5**: la línea de DEVICE= acaba en retorno y salto
  (NANSI decía «Possible options» porque buscaba el final de la línea), y en
  el campo 14 se le dice hasta dónde puede llegar.
- **Controladores `.EXE`** (los de MS-DOS 5 en adelante): se aplican las
  reubicaciones y se quita la cabecera.
- **El cambio de disco**: antes de cada operación en una unidad de un
  controlador se le pregunta (orden 1, MEDIA CHECK); si dice que ha cambiado
  (o que no lo sabe y no hay archivos abiertos), se le piden las medidas del
  nuevo (orden 2, BUILD BPB, con el primer sector de la FAT si es «de IBM»),
  y se olvida lo guardado del anterior.
- **IOCTL** (INT 21h/44h): 00h con los atributos de verdad, 01h, 02h/03h y
  04h/05h (órdenes 3 y 12), 06h/07h (órdenes 6 y 10; de un archivo, si queda
  algo por leer), 08h (orden 15), 09h, 0Ah, 0Bh, 0Dh (la genérica, orden 19)
  y 0Eh/0Fh. A cada controlador solo se le pide lo que su cabecera dice que
  entiende (bits 14, 11 y 6).
- **La lista de listas** (INT 21h/52h): la primera cabecera de memoria (en
  -2), CON, cuántas unidades de bloque, la última letra y **la cabecera de
  NUL en 22h**, de donde sale la cadena; los controladores de bloque se
  enlazan también en la cadena, como en MS-DOS.
- **La consola por un CON cargado** (un ANSI.SYS): si un controlador de
  carácter dice ser la entrada o la salida de la consola, la consola de los
  programas (INT 21h 01h a 0Ch, números de archivo 0 a 2) va por él. Y
  **INT 29h**, la salida rápida.
- **Hasta cinco unidades de controladores** (D: a H:; antes, dos).

Lo que la batería prueba (`h8086/prueba/partes_drivers.sh`):

| Parte | Controlador | Lo que se comprueba |
|---|---|---|
| `drivers` | DISQUITO.SYS y SALUDOX.SYS (nuestros, de prueba), SALUDO.SYS, DISCORAM.SYS | uno de bloque «de quitar y poner» con dos discos: MEDIA CHECK y BUILD BPB al cambiar (4405h), lo escrito sigue en cada disco; uno `.EXE` con su reubicación; IOCTL 4400h-440Dh; la cadena de 52h |
| `drivers_ansi` | NANSI.SYS (FreeDOS, GPL) | se carga, entiende su línea y pone los colores de las órdenes ANSI |
| `drivers_red` | NE2000.COM, PKTSTAT, TERMIN (Crynwr, GPL) | se instala en la NE2000 de QEMU, PAQUETE (nuestro) pregunta por ARP a la pasarela de QEMU y recibe la respuesta; PKTSTAT cuenta; TERMIN lo quita y la memoria vuelve a ser la de antes |
| `drivers_srdisk` | SRDXMS.SYS y SRDISK.EXE (FreeDOS, GPL, comprimido con UPX) | disco en memoria XMS de 200 K, se copia en él y se le cambia a 300 K sin perder el archivo |
| `drivers_raton` | CTMOUSE.EXE (FreeDOS, GPL) | el ratón se instala (en el puerto PS/2) |

Los packet drivers **ya funcionaban** con la 0.14 (bastaba con los
residentes); lo que faltaba era probarlo. Los de terceros se descargan con
`h8086/prueba/trae_drivers.sh` a `~/.cache/hispa-drivers/descargas` (fuera
del repositorio).

## Hito 2, empezado: Hispa-386

Rama `dos-drivers` (desde `worktree-v0.1-nucleo`; no toca nada de lo que
arregla `dos-0931`). En `kernel/kernel.asm`:

- `l_devcall` llama a la estrategia y a la interrupción con **DS:SI en la
  cabecera del controlador**, como DOS. Con DS en el núcleo, SRDXMS.SYS de
  FreeDOS (comprimido con UPX) reiniciaba la máquina al arrancar: es el mismo
  fallo que tenía Hispa-8086. Los controladores propios (HSPXMS, HSPEMM,
  HSPCD, HSPUSB, HSPRAM, HSPTEST) guardan la petición con `cs:` y no dependen
  de DS.
- `media_check`: si el controlador dice que el disco ha cambiado, se le pide
  **BUILD BPB** (orden 2) y se apunta la BPB que da; y «no se sabe» (0) se
  toma como cambiado si no hay FAT a medio escribir, como en MS-DOS. Sin eso,
  al cambiar de tamaño un disco de SRDISK, DIR daba el tamaño viejo y TYPE
  leía basura.
- Prueba: `tests/run_drivers_otros_tests.sh` (en `make test`), con SRDISK bajado
  por `tests/thirdparty/fetch_drivers.sh`; con el núcleo de antes da 7 fallos
  de 7, con el nuevo, 7 bien.

Queda del hito: las IOCTL de bloque pasadas al controlador (ver arriba: no
caben en KSEG sin adelgazar antes el núcleo).
