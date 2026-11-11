# HispaDOS

**Sistema operativo libre compatible con MS-DOS 6.22, escrito desde cero, con drivers para hardware actual.**
*A free, from-scratch MS-DOS 6.22 compatible operating system with drivers for modern hardware.* — [English below](#english)

```
HispaDOS 0.1 - MS-DOS 6.22 compatible kernel

A:\>DIR

 Volume in drive A is HISPADOS
 Directory of A:\

KERNEL   SYS         7,288 11-11-26   4:37a
COMMAND  COM         2,839 11-11-26   4:37a
DOCS            <DIR>      11-11-26   4:37a
HELLOEXE EXE           368 11-11-26   4:37a
```

## Objetivos

1. **Compatibilidad con MS-DOS 6.22**: la misma API (INT 21h, INT 2Fh, PSP, MCB, FCB, drivers `.SYS`), para que los programas de DOS funcionen sin cambios.
2. **Compatibilidad con Windows 3.1** (modo estándar y 386 mejorado) sobre HispaDOS.
3. **Drivers actuales** para DOS y para Windows 3.1: vídeo (VESA/VBE, alta resolución), ratón (PS/2 y USB), sonido (HDA/AC'97 y compatibilidad Sound Blaster) y red con TCP/IP.
4. **Código abierto** y sin código de Microsoft: todo se escribe a partir de documentación pública (Ralf Brown's Interrupt List, especificación FAT, formato MZ, especificaciones de hardware).

El plan completo, por fases, está en [docs/ROADMAP.md](docs/ROADMAP.md).

## Estado (versión 0.2 en desarrollo)

| Pieza | Estado |
|---|---|
| Sector de arranque FAT12 | ✅ carga `KERNEL.SYS` |
| Núcleo: consola (INT 21h 01-0Ch), vectores, fecha/hora, versión 6.22 | ✅ |
| Sistema de ficheros FAT12: abrir/leer/buscar, subdirectorios, CD | ✅ solo lectura |
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
| Estructuras internas de DOS 4+: SDA, CDS, cadena de DPB, lista de listas; 1Fh/32h | ✅ |
| Sonido: `HSPSND` (Intel HD Audio y AC'97) y `PLAYWAV` | ✅ |
| Imagen para memoria USB (arrancar o instalar con `INSTALL`) | ✅ |
| Drivers USB nativos, UMB/EMS, Windows 3.1, vídeo y red | 🔜 ver hoja de ruta |
| Drivers, XMS, Windows 3.1 | 🔜 ver hoja de ruta |

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

`make usb` crea `build/hispados-usb.img` (64 MB). Grábala con Rufus (modo DD), balenaEtcher o `dd` y arranca el PC desde la memoria (en equipos UEFI, con el modo *Legacy/CSM*). Sirve para **probar** HispaDOS sin tocar el disco duro o para **instalarlo** con `INSTALL`, que solo actúa sobre un disco duro vacío.

La imagen también arranca en VirtualBox, 86Box, Bochs o en un PC real con BIOS (o UEFI con CSM) desde disquete o USB en modo disquete.

## Estructura

```
boot/      MBR y sectores de arranque FAT12/FAT16
kernel/    núcleo (KERNEL.SYS): INT 21h, FAT, memoria, ejecución de programas
shell/     intérprete de órdenes (COMMAND.COM)
utils/     utilidades: FDISK, FORMAT
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

HispaDOS is a free operating system written from scratch that aims to be **compatible with MS-DOS 6.22** and **Windows 3.1**, with **drivers for current hardware**: video (VESA/VBE), mouse (PS/2, USB), audio (HDA/AC'97 with Sound Blaster compatibility) and TCP/IP networking, both for DOS and Windows 3.1.

It contains no Microsoft code: everything is implemented from public documentation.

**Status (0.2, in progress):** boots from floppy or hard disk (own MBR and FAT12/FAT16 boot sectors); kernel with console, read-only FAT12/FAT16 with subdirectories, several drives (MBR partitions, LBA), file handles, MCB memory management and `.COM`/`.EXE` loading; `COMMAND.COM` with the basic internal commands and PATH search; `FDISK` and `FORMAT /S` to install HispaDOS on a hard disk. See [docs/ROADMAP.md](docs/ROADMAP.md) for the plan.

```sh
make && make run     # build the floppy image and boot it in QEMU
make test            # automated end-to-end test over the serial port
```

Contributions are welcome. Please never submit Microsoft code or disassembly. Licensed under GPL-2.0-or-later.
