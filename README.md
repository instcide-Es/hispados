# Hispa-DOS

**Sistema operativo libre compatible con MS-DOS 6.22, escrito desde cero, con drivers para hardware actual.**
*A free, from-scratch MS-DOS 6.22 compatible operating system with drivers for modern hardware.* — [English below](#english)

```
Hispa-DOS 0.9 - núcleo compatible con MS-DOS 6.22
(C) 2026 colaboradores de Hispa-DOS - GPL-2.0 o posterior

Iniciando Hispa-DOS...  F5 = arranque limpio  ·  F8 = menú de arranque
HSPXMS 0.1: gestor XMS 3.0, 129856 KB de memoria extendida

Intérprete de órdenes de Hispa-DOS 0.9
Escriba AYUDA para ver la lista de órdenes.

A:\>DIR /W

 El volumen de la unidad A es HISPADOS
 Directorio de A:\

KERNEL.SYS     COMMAND.COM    README.TXT     AUTOEXEC.BAT   [HISPADOS]
[BAT]          [DOCS]         [BIN]          HELLOEXE.EXE   CONFIG.SYS
       10 archivo(s)         48.931 bytes
                1.296.384 bytes libres
```

## Objetivos

1. **Compatibilidad con MS-DOS 6.22**: la misma API (INT 21h, INT 2Fh, PSP, MCB, FCB, drivers `.SYS`), para que los programas de DOS funcionen sin cambios.
2. **Compatibilidad con Windows 3.1** (modo estándar y 386 mejorado) sobre Hispa-DOS.
3. **Drivers actuales** para DOS y para Windows 3.1: vídeo (VESA/VBE, alta resolución), ratón (PS/2 y USB), sonido (HDA/AC'97 y compatibilidad Sound Blaster) y red con TCP/IP.
4. **Código abierto** y sin código de Microsoft: todo se escribe a partir de documentación pública (Ralf Brown's Interrupt List, especificación FAT, formato MZ, especificaciones de hardware).

El plan completo, por fases, está en [docs/ROADMAP.md](docs/ROADMAP.md).

## Descargar y probar

Las imágenes listas para usar están en **[Releases](https://github.com/instcide-Es/hispados/releases)**:

| Archivo | Para qué |
|---|---|
| `hispados-disquete.img` | Disquete de arranque de 1,44 MB. En VirtualBox, VMware, 86Box o QEMU, como disquete; en un PC antiguo, grábalo en un disquete (Rufus o `dd`). |
| `hispados-usb.zip` | Imagen de 64 MB para memoria USB: descomprímela y grábala con Rufus (modo DD), balenaEtcher o `dd`. Arranca en PCs con BIOS (o UEFI con CSM/Legacy) y permite instalar en el disco duro con `INSTALAR`. |

Probar en QEMU: `qemu-system-i386 -fda hispados-disquete.img`.

**Requisitos:** procesador **386 o superior** (386, 486, Pentium...) y 640 KB de memoria; para Hispa Ventanas, además memoria extendida (2 MB o más). En un 8086, 186 o 286 Hispa-DOS lo dice al arrancar en vez de colgarse. Una versión para 8086/286 está en la lista de ideas.

La 0.9 es una **versión de pruebas**: pasa más de mil comprobaciones automáticas en emuladores, pero falta probarla en más PCs reales. Si encuentras un fallo, abre una *issue* contando el equipo (o el emulador) y qué pasó.

## Estado (versión 0.9 de pruebas)

| Pieza | Estado |
|---|---|
| Sector de arranque FAT12 | ✅ carga `KERNEL.SYS` |
| Núcleo: consola (INT 21h 01-0Ch), vectores, fecha/hora, versión 6.22 | ✅ |
| Sistema de ficheros FAT12: abrir/leer/buscar, subdirectorios, CD | ✅ |
| Handles (3Dh-46h), SFT y JFT en el PSP | ✅ |
| Memoria con cadena de MCB (48h/49h/4Ah) | ✅ |
| Ejecución de `.COM` y `.EXE` (MZ con reubicaciones), entorno, código de retorno | ✅ |
| `COMMAND.COM`: VER, DIR, TYPE, CD, ECHO, SET, MEM, DATE, TIME, VOL, CLS, HELP, EXIT, búsqueda en PATH | ✅ |
| Discos duros: MBR, particiones, FAT16, LBA, varias unidades (A:, C:, D:…) | ✅ |
| `FDISK.COM`: crear, activar, borrar y ver particiones (menú y `/STATUS` `/PRI` `/ACT` `/DEL` `/MBR`) | ✅ |
| `FORMAT.COM`: FAT12 (disquetes) y FAT16 (discos duros), `/S` para disco de arranque, `/V` | ✅ |
| Arranque desde disco duro (MBR + sector de arranque FAT16) | ✅ |
| INT 25h/26h, IOCTL 440Dh (parámetros de unidad) | ✅ |
| Escritura FAT12/FAT16: crear, escribir, truncar, borrar, renombrar/mover, MKDIR/RMDIR, atributos, fecha | ✅ |
| Entrada/salida estándar redirigible (funciones 01h-0Ah por los handles 0 y 1) | ✅ |
| `COMMAND.COM`: COPY (comodines, `+`, `COPY CON`), DEL/ERASE, REN, MD, RD, PAUSE, `>` `>>` `<` y tuberías `\|` | ✅ |
| Ficheros por lotes: `%0`-`%9`, `%VAR%`, `@`, ECHO ON/OFF, IF, GOTO, FOR, CALL, SHIFT; `AUTOEXEC.BAT` | ✅ |
| SET, PATH, PROMPT (`$P $G $N $D $T $V $_`…), `COMMAND /C` y `/P` | ✅ |
| `CONFIG.SYS`: DEVICE(HIGH), INSTALL, SHELL, SET, LASTDRIVE, FILES, BUFFERS… | ✅ |
| Drivers `.SYS` reales (carácter y bloque, también MZ), cadena de dispositivos, lista de listas | ✅ |
| `HSPRAM.SYS` (disco RAM) y `HSPTEST.SYS` (dispositivo de ejemplo) | ✅ |
| Funciones FCB (DOS 1.x), SDA (5D06h/5D0Bh), EXEC 4B01h/4B03h, 26h/55h, 65h (país CP437), Ctrl+C | ✅ |
| Probado con programas reales de FreeDOS: EDIT, DEBUG, MEM, XCOPY, FC, DELTREE, LABEL, SORT, FIND, MORE, TREE, ATTRIB, CHOICE, NANSI.SYS, CTMOUSE | ✅ |
| `HSPXMS.SYS`: gestor XMS 3.0 propio (A20, HMA, bloques, movimientos, 32 bits) | ✅ |
| `DOS=HIGH`: el código del núcleo en la memoria alta (HMA); unos 33 KB más de memoria convencional. INT 2Fh 4A01h/4A02h (HMA libre) y `MEM` lo dice | ✅ |
| `HSPDPMI.COM`: servidor DPMI 0.9 propio (clientes de 16 y 32 bits, memoria de XMS; los programas de DJGPP funcionan sin CWSDPMI) | ✅ |
| `HSPEMM.SYS`: monitor V86 propio (como EMM386): EMS 4.0, VCPI 1.0 (DOS/4GW: Doom, Duke Nukem 3D…) y memoria superior (`DOS=UMB`, `DEVICEHIGH`, `LH`); `HSPDPMI` pasa a cliente VCPI. Ver [docs/HSPEMM.md](docs/HSPEMM.md) | ✅ en pruebas |
| Estructuras internas de DOS 4+: SDA, CDS, cadena de DPB, lista de listas; 1Fh/32h | ✅ |
| Sonido: `HSPSND` (Sound Blaster 2.0/Pro/16, Intel HD Audio y AC'97; pone la variable `BLASTER`) y `PLAYWAV`/`SONAR` | ✅ |
| Volumen: `HSPSND /VOL:n`, `SONIDO` (`n`, `+`, `-`, `/M` silencio, `/A` con las flechas) y teclas Ctrl+Alt+↑/↓/Fin y multimedia; `HSPSND /INFO` diagnostica la tarjeta (códec HD Audio, salidas, EAPD, GPIO, camino elegido) | ✅ |
| Imagen para memoria USB (arrancar o instalar con `INSTALL`) | ✅ |
| `HSPUSB.SYS`: memorias USB nativas (controladora xHCI, almacenamiento masivo) | ✅ |
| `HSPUSB.SYS`: concentradores USB (hubs, también encadenados), `/INFO` (árbol de dispositivos) y arranque desde memoria USB sin perder el disco (`/U`) | ✅ |
| `HSPCD.SYS`: lectores de CD/DVD (ATAPI) e ISO 9660 en el núcleo (solo lectura) | ✅ |
| `SYS.COM`, `INSTALAR` (instalar en un disco duro), `KEYB SP` (teclado español, AltGr) | ✅ |
| Órdenes y mensajes en español (AYUDA, COPIAR, BORRAR, LISTAR…), historial de órdenes y edición de la línea (flechas, Inicio, Fin, Supr, Insertar, Ctrl+flechas por palabras, Tab) | ✅ |
| Teclas que faltan en los portátiles de Apple (`TECLADO SP /MAC`, automático si el equipo es de Apple): Ctrl+Retroceso = Supr, Alt+Retroceso = Insertar, Alt+←/→ = Inicio/Fin, Alt+↑/↓ = RePág/AvPág | ✅ |
| `EDIT`/`EDITAR` (editor), `GESTOR` (gestor de archivos), `SYSINFO`/`INFO` (propiedades del sistema) | ✅ |
| Teclado y ratón USB nativos (HID) y ratón PS/2 y touchpads de portátil (`HSPRATON`, INT 33h: por la BIOS o, si no lo ofrece, directamente por el controlador 8042; `HSPRATON /INFO` enseña el diagnóstico y `/BIOS` o `/DIRECTO` fuerzan la vía) | ✅ |
| CD/DVD también en SATA (AHCI); menú de arranque propio con F5 y F8 | ✅ |
| SHARE en el núcleo (bloqueos y modos de compartición) | ✅ |
| Sistema en `\HISPADOS` con nombres en español y `ALIAS.INI` con los clásicos (EDIT, FORMAT, XCOPY…) | ✅ |
| Órdenes: MOVER, ATRIBUTO, ARBOL, BUSCAR, ORDENAR, PAGINAR, ELEGIR, BORRARARBOL, EXPANDIR, ETIQUETA; MEM `/C /F /M /D /P` | ✅ |
| Cambiar de disco sin reiniciar: disquetes (360 KB a 2,88 MB), CD/DVD y memorias USB; «La unidad no está preparada» | ✅ |
| Windows for Workgroups 3.11 en modo 386 mejorado (cada uno con su copia; ver [docs/WINDOWS.md](docs/WINDOWS.md)) | ✅ en pruebas |
| **Hispa Ventanas**, etapas 2 y 3: `VENTANAS` abre el entorno gráfico propio (VESA 640×480 a 1024×768 de 256 colores o color verdadero, o VGA de 16 colores; fuente y aspecto propios) con ventanas que se mueven, cambian de tamaño, se maximizan y se minimizan, menús, cuadros de diálogo y los primeros programas: Centro de programas (con grupos y accesos directos, también a los juegos y programas de Hispa-DOS, que se ejecutan con toda la memoria y vuelven a Hispa Ventanas), Bloc de notas, Calculadora y Reloj, el volumen en la barra y, como programas aparte (módulos .HVP que se abren como ventanas), los primeros juegos propios, Culebra, Campo de minas, Bloques, Solitario (baraja española), Tragabolas y Almacén, y Pintura, el programa de dibujo (lee y guarda BMP). Ver [docs/VENTANAS.md](docs/VENTANAS.md) | ✅ en pruebas |
| Sound Blaster emulada en tarjetas modernas, EHCI/UHCI, vídeo VESA y red TCP/IP | 🔜 ver hoja de ruta |

## Sonido y volumen

`HSPSND` (lo carga `AUTOEXEC.BAT`) detecta una Sound Blaster, Intel HD Audio o AC'97. En HD Audio saca el sonido a la vez por los altavoces internos, los auriculares y la salida de línea, enciende el amplificador externo (EAPD y las patillas GPIO del códec) y, si el códec no admite la frecuencia del archivo, la cambia él.

| Orden | Qué hace |
|---|---|
| `HSPSND /VOL:n` | Volumen del 0 al 100 % (80 si no se dice). Con el driver ya cargado cambia el volumen sin cargar otro. |
| `HSPSND /INFO` | Muestra en una pantalla la tarjeta, el códec, cada salida (altavoz, auriculares, línea…), EAPD, GPIO, el camino y el amplificador elegidos. Si no suena, una foto de esta pantalla dice por qué. |
| `HSPSND /GPIO:hh` `/NOGPIO` `/EAPD:0` | HD Audio: valor de las GPIO (por omisión todas a 1), no tocarlas, o EAPD apagado, para los portátiles que lo tienen al revés. Se pueden cambiar con el driver ya cargado. |
| `HSPSND /NOTECLAS` | Sin teclas de volumen. |
| `SONIDO` | Muestra el volumen. `SONIDO 60` lo pone; `SONIDO +` y `SONIDO -` lo cambian un 10 %; `SONIDO /M` silencia y otra vez lo devuelve; `SONIDO /A` lo ajusta con las flechas. |
| `SONAR [/VOL:n] archivo.wav` | Toca un WAV; `SONAR /TONE 440` un tono de prueba. |

Teclas, en cualquier programa: **Ctrl+Alt+↑** y **Ctrl+Alt+↓** suben y bajan el volumen un 10 %, **Ctrl+Alt+Fin** lo silencia, y también valen las teclas de volumen de los teclados multimedia (las de Fn de muchos portátiles).

## Memoria: `DOS=HIGH`

Con estas dos líneas en `CONFIG.SYS` (vienen puestas en el disquete y en la memoria USB), el código del núcleo se va a la memoria alta (HMA, los primeros 64 KB por encima de 1 MB) y deja **unos 33 KB más de memoria convencional** (de los 52 KB que ocupa el núcleo se quedan 20 abajo):

```
DEVICE=A:\HISPADOS\HSPXMS.SYS
DOS=HIGH
```

`MEM` lo confirma («Hispa-DOS está cargado en la memoria alta (HMA)»). También se aceptan `DOS=LOW` (lo de siempre) y `DOS=HIGH,UMB` (la parte `UMB` aún no hace nada). Si no hay gestor XMS o la HMA ya es de otro, Hispa-DOS lo dice y se queda en la memoria convencional. Con F5 (arranque limpio) no se usa.

En la memoria convencional se queda lo que tienen que ver los programas, la BIOS y Windows: los vectores y unos trozos de entrada (INT 20h, 21h, 25h-27h, 29h, 2Fh), la lista de listas, la SDA, la CDS, los DPB, la tabla de ficheros (SFT), las cabeceras de los dispositivos, los búferes de sector (la BIOS no puede llevar un disquete por DMA por encima de 1 MB), las pilas del núcleo y los datos de instancia de Windows. La línea A20 queda abierta; si un programa la cierra, el núcleo la vuelve a abrir en cuanto se le llama. Los 28 KB de HMA que sobran se ofrecen a otros programas por INT 2Fh AX=4A01h/4A02h, como en MS-DOS.

## Compilar y probar

Hace falta `nasm`, `mtools` y, para probar, `qemu-system-i386`:

```sh
sudo apt install nasm mtools qemu-system-x86
make          # crea build/hispados.img (disquete de 1,44 MB)
make run      # arranca en QEMU
make run-hdd  # arranca con un disco duro vacío de 64 MB (build/hdd.img)
make usb      # imagen para memoria USB: build/hispados-usb.img
make test     # pruebas automáticas por el puerto serie
tests/thirdparty/fetch.sh   # (opcional) descarga programas de FreeDOS para
                            # la prueba de compatibilidad
```

Para instalar en un disco duro (real o virtual), arranca desde el disquete y ejecuta:

```
FDISK /PRI          (o FDISK para el menú)
FORMAT C: /S
```

Después el disco arranca solo.

### Memoria USB

`make usb` crea `build/hispados-usb.img` (64 MB). Grábala con Rufus (modo DD), balenaEtcher o `dd` y arranca el PC desde la memoria (en equipos UEFI, con el modo *Legacy/CSM*). Sirve para **probar** Hispa-DOS sin tocar el disco duro o para **instalarlo** con `INSTALL`, que solo actúa sobre un disco duro vacío.

La imagen también arranca en VirtualBox, 86Box, Bochs o en un PC real con BIOS (o UEFI con CSM) desde disquete o USB en modo disquete.

## Estructura

```
boot/      MBR y sectores de arranque FAT12/FAT16
kernel/    núcleo (KERNEL.SYS): INT 21h, FAT, memoria, ejecución de programas
shell/     intérprete de órdenes (COMMAND.COM)
utils/     utilidades: FDISK, FORMAT, SYS, KEYB, INSTALAR, EDIT, GESTOR, SYSINFO…
drivers/   drivers de dispositivo (.SYS)
lib/       rutinas comunes de las utilidades
tests/     programas y disco de prueba, prueba automática
docs/      hoja de ruta y documentación técnica
```

## Contribuir

Se aceptan *pull requests*. Normas importantes:

- **No copiar código de Microsoft** (ni del MS-DOS publicado por Microsoft ni de desensamblados). Todo se implementa a partir de documentación pública.
- Cada función nueva de INT 21h debe ir acompañada de una comprobación en `tests/`.
- Windows 3.1 y otros programas de terceros no se incluyen en el repositorio: cada uno usa su propia copia legal.

## Licencia

GPL-2.0-or-later. Ver [LICENSE](LICENSE).

---

## English

Hispa-DOS is a free operating system written from scratch that aims to be **compatible with MS-DOS 6.22** and **Windows 3.1**, with **drivers for current hardware**: video (VESA/VBE), mouse (PS/2, USB), audio (HDA/AC'97 with Sound Blaster compatibility) and TCP/IP networking, both for DOS and Windows 3.1.

It contains no Microsoft code: everything is implemented from public documentation.

**Status (0.9, test release):** MS-DOS 6.22 compatible kernel and shell (FAT12/FAT16, hard disks, CD/DVD, native USB, SHARE, critical error handler, batch files, CONFIG.SYS and `.SYS` drivers), XMS, `DOS=HIGH` (kernel code in the HMA, about 33 KB more conventional memory), Sound Blaster/HD Audio/AC'97, PS/2 and USB mouse and keyboard, and the usual utilities with Spanish names (and the classic ones). Windows for Workgroups 3.11 runs in 386 enhanced mode. Ready-to-use images are in [Releases](https://github.com/instcide-Es/hispados/releases). See [docs/ROADMAP.md](docs/ROADMAP.md) and [docs/VENTANAS.md](docs/VENTANAS.md) for what comes next.

```sh
make && make run     # build the floppy image and boot it in QEMU
make test            # automated end-to-end test over the serial port
```

Contributions are welcome. Please never submit Microsoft code or disassembly. Licensed under GPL-2.0-or-later.
