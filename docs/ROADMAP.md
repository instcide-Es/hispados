# Hoja de ruta / Roadmap

Cada fase termina con pruebas automáticas en QEMU (`make test`) y, cuando toque, en hardware real.

## 0.1 — Núcleo mínimo ✅

- Sector de arranque FAT12 que carga `KERNEL.SYS` en `0070:0000`.
- INT 21h: consola, vectores, fecha/hora, versión 6.22, FAT12 de solo lectura con subdirectorios, handles, MCB, `EXEC` de `.COM` y `.EXE`, terminar y código de retorno, `TRUENAME`, `InDOS`.
- `COMMAND.COM` con las órdenes internas básicas y búsqueda en PATH.

## 0.2 — DOS de uso diario

- ✅ Discos duros: MBR con particiones, FAT16, INT 13h extendida (LBA), varias unidades (C:, D:…).
- ✅ `FDISK` (menú y línea de órdenes), `FORMAT` (FAT12/FAT16, `/S`, `/V`), MBR y arranque FAT16 propios.
- ✅ INT 25h/26h e IOCTL 440Dh; el núcleo relee las particiones tras FDISK sin reiniciar.
- ✅ Escritura FAT12/FAT16: crear, escribir, truncar, borrar, renombrar, `MKDIR`/`RMDIR`, atributos, fecha y hora (comprobada con `fsck.fat`).
- ✅ Particiones extendidas (05h, 0Fh, 85h) y unidades lógicas FAT12/FAT16 en la cadena de EBR, con las letras de MS-DOS (primarias de cada disco y después las lógicas de cada disco). `PARTIR` todavía no las crea.
- ✅ Funciones FCB (0Fh-28h) para programas antiguos, también con FCB extendidos y etiquetas de volumen.
- ✅ Compatibilidad comprobada con programas de FreeDOS (EDIT, DEBUG, MEM, XCOPY, FC, DELTREE, LABEL, SORT, FIND, MORE, TREE, ATTRIB, CHOICE, NANSI.SYS, CTMOUSE).
- ✅ `COMMAND.COM`: redirecciones `<`, `>`, `>>`, tuberías `|`, `COPY`, `DEL`, `REN`, `MD`, `RD`, `PAUSE`.
- ✅ `COMMAND.COM`: ficheros `.BAT` y `AUTOEXEC.BAT`, `SET` con asignación, `IF`, `GOTO`, `FOR`, `CALL`, `SHIFT`, `PROMPT`, `PATH`, `/C`, `/P`.
- ✅ Ctrl+C con INT 23h como DOS. Pendiente: Ctrl+Break (INT 1Bh) e INT 24h interactiva («Abort, Retry, Fail?»).
- Mensajes en español además de en inglés.
- ✅ `VELOCIDAD` (frenar el procesador para juegos antiguos: por el temporizador y, en 486 o superior, desactivando las cachés) y `LOADFIX` (cargar por encima de los primeros 64 KB); `tests/run_velocidad_tests.sh`. Salen en la lista de `AYUDA`, y `AYUDA VELOCIDAD` y `AYUDA LOADFIX` las explican.

## 0.3 — Drivers y memoria

- ✅ `CONFIG.SYS`: `DEVICE=`, `DEVICEHIGH=`, `INSTALL=`, `SHELL=`, `SET`, `LASTDRIVE=` (y `FILES=`, `BUFFERS=`… aceptadas).
- ✅ **`DOS=HIGH`** (y `DOS=LOW`; `DOS=HIGH,UMB` con HSPEMM cuelga además la memoria superior de la cadena de MCB): el código del núcleo y sus datos privados (historial, bloqueos de SHARE, texto de CONFIG.SYS) pasan a la HMA, que se pide al gestor XMS (función 01h) en cuanto lo carga un `DEVICE=`, como MS-DOS. Hasta entonces el código espera en lo más alto de la memoria convencional, para no dejar un hueco debajo de los controladores. El núcleo ocupa 20 KB de memoria convencional en vez de 52 (33 KB más libres). Abajo quedan los trozos de entrada de las interrupciones, la lista de listas, la SDA, la CDS, los DPB, la SFT, las cabeceras de dispositivo, los búferes de sector, las pilas y los datos de instancia de Windows. A20 queda abierta (XMS 05h) y se comprueba en cada entrada al núcleo y a la vuelta de los controladores, INT 13h, 23h, 24h y 28h. INT 21h AX=3306h (DH bit 4), INT 2Fh AX=4A01h/4A02h (HMA libre para otros) y `MEM`. Sin XMS o con la HMA ocupada: aviso y a la memoria convencional. Probado con HSPDPMI, Hispa Ventanas y Windows para Trabajo en Grupo 3.11 en modo 386 mejorado (`tests/run_hma_tests.sh`).
- Pendiente con la HMA: llevar allí los búferes de disco (`BUFFERS=`) con un búfer de paso abajo para los disquetes, y más datos del núcleo (caché de ISO 9660, tabla de unidades).
- ✅ Interfaz de drivers de DOS (`.SYS` de carácter y de bloque, *strategy*/*interrupt*, también en formato MZ), cabeceras y cadena de dispositivos reales; drivers propios `HSPRAM.SYS` y `HSPTEST.SYS`.
- INT 2Fh multiplex completo.
- ✅ **HSPXMS.SYS**: gestor XMS 3.0 (compatible con HIMEM), A20 y HMA; probado con XMSTEST y con MEM de FreeDOS. También funciona HIMEMX de FreeDOS.
- ✅ **Menú de configuraciones de CONFIG.SYS** como MS-DOS 6.22 (`[menu]`, `MENUITEM`, `SUBMENU`, `MENUDEFAULT`, `MENUCOLOR`, `[common]`, `INCLUDE`, variable `CONFIG`) y opción **«Juegos»** en el disquete y la memoria USB (HSPEMM, `DOS=HIGH,UMB`, `DEVICEHIGH`/`LH`). `HSPUSB` funciona también en modo V86 (INT 15h AH=87h y VCPI). Pendiente: probarlo en equipos de verdad.
- ✅ **HSPEMM.SYS**: monitor V86 propio: EMS 4.0, VCPI 1.0 y UMB (`DOS=UMB`, `DEVICEHIGH`, `LH`), ver [HSPEMM.md](HSPEMM.md). Pendiente: importación global de EMM para Windows 386 mejorado con UMB en uso.

## 0.4 — Compatibilidad con Windows 3.1

Windows 3.1 se apoya en estructuras internas de DOS, no solo en la API pública. Hay que implementarlas igual que las documenta la Interrupt List:

- ✅ Lista de listas (52h) con cadena de DPB, CDS de DOS 4+ y cabecera NUL; 1Fh y 32h. Pendiente: SFT en el formato de DOS encadenadas desde la lista de listas.
- ✅ *Swappable Data Area* (5D06h/5D0Bh) con `ErrorMode`, `InDOS`, PSP y DTA en sus desplazamientos. Pendiente: secciones críticas INT 2Ah.
- Funciones de red y redirector 5Dh/5Eh/5Fh y difusión de arranque de Windows (INT 2Fh 1605h/1606h).
- Objetivos de prueba: Windows 3.1 en **modo estándar** primero (con HSPXMS.SYS) y después en **386 mejorado**, en QEMU y 86Box.
- Windows 3.1 no se distribuye: cada persona instala su propia copia.

## 0.5 — Drivers para hardware actual (DOS)

| Área | Driver | Notas |
|---|---|---|
| Vídeo | `HSPVBE` | VESA VBE 2.0/3.0, framebuffer lineal y modos panorámicos; API VBE para juegos y programas. |
| Ratón | `HSPMOUSE.COM` (INT 33h) | PS/2; USB a través de la emulación *legacy* de la BIOS y, más adelante, pila USB propia (UHCI/OHCI/EHCI/xHCI, HID). |
| Sonido | `HSPSB` | **Primera versión hecha (13/11/2026), en pruebas: `HSPSND /SB`, ver [HSPSB.md](HSPSB.md).** Tarjetas HDA y AC'97 con emulación de Sound Blaster 16 en modo V86 (HSPEMM atrapa los puertos de E/S y virtualiza el DMA), para que los juegos de DOS suenen: DSP 4.05 (8 y 16 bits, mono y estéreo, bloque único y autoinicio), IRQ, mezclador y FM OPL2 por síntesis. Falta: hardware real, los juegos en modo protegido (atrapar la E/S en HSPDPMI), OPL3, ADPCM y MPU-401. |
| Red | Packet drivers | Especificación de *packet driver* para Intel e1000/e1000e, Realtek 8139/8169 y virtio-net. TCP/IP con aplicaciones compatibles (como mTCP) y una pila propia `HSPNET`. |
| Discos | `HSPAHCI`, `HSPNVME` | Acceso a SATA (AHCI) y NVMe cuando la BIOS no los ofrece por INT 13h. |
| USB | `HSPUSB` | **Hecho (13/11/2026): las cuatro clases de controladora** (UHCI, OHCI, EHCI con traspaso a las compañeras, xHCI), varias a la vez, con una capa común: memorias USB como unidades de DOS, teclado, ratón, mandos y concentradores. Ver [USB.md](USB.md). Falta: probar UHCI/OHCI/EHCI en hardware real, concentradores de USB 2.0 con aparatos de USB 1.x en un EHCI (hecho según la norma, sin probar), más de un xHCI, conectar en caliente teclado, ratón y mandos. |
| Mandos de juego | `HSPJOY` | **Primera versión hecha (13/11/2026), en pruebas: ver [MANDOS.md](MANDOS.md).** Hecho: USB (HID genéricos con su descriptor de informe y tipo Xbox 360) dentro de HSPUSB; `HSPJOY.COM` (puerto de juegos analógico con calibración; impresora: Atari/Commodore, Mega Drive, NES, Super Nintendo y PlayStation; INT 15h 84h; interfaz INT 2Fh D9h para HSPEMM y Hispa Ventanas; mando a teclado con `MANDO.INI`) y `MANDO.COM`. Falta: probar con hardware real, el puerto 201h virtual (con HSPEMM), los mandos digitales del puerto de juegos (GrIP, SideWinder), el puerto serie (diseño en MANDOS.md), conectar en caliente, y la API de Hispa Ventanas y el apartado «Mando» de Configuración. Pedido por Carlos (13/11/2026): mandos **USB** (HID genéricos y tipo Xbox 360) por HSPUSB; **puerto de juegos** de la tarjeta de sonido o suelto (201h: analógicos de 2 y 4 ejes, y digitales como Gravis GamePad Pro y SideWinder); **puerto serie** (SpaceOrb y volantes); **puerto de impresora** con cable casero DB9/DB25 (joysticks Atari y Commodore, y mandos originales de NES, Super Nintendo, PlayStation y Mega Drive). Los juegos de DOS lo ven como un mando clásico por INT 15h 84h y, con HSPEMM, por el puerto 201h virtual. Mando a teclado para juegos sin soporte de mando. API en Hispa Ventanas (juegos propios, Hispa-Nes...) y apartado «Mando» en Configuración. Después de juntar HSPEMM. |

## 0.6 — Drivers para Windows 3.1

| Área | Driver | Notas |
|---|---|---|
| Vídeo | `HSPVBE.DRV` + VxD | Driver de pantalla de Windows 3.1 sobre VBE: alta resolución y color de 16/24/32 bits. |
| Ratón | `HSPMOUSE.DRV` | Usa INT 33h de Hispa-DOS; puntero absoluto en máquinas virtuales. |
| Sonido | `HSPWAVE.DRV` (+ mezclador) | Driver MMSYSTEM (wave y MIDI por software) para HDA/AC'97 que habla con HSPSND, y driver de mezclador para que el control de volumen de Windows mueva el de HSPSND. Criterio de Carlos (13/11/2026): **convivir con los originales** — la tarjeta original con los controladores de Creative (instalador, utilidades de DOS y drivers de Windows 3.11) tiene que funcionar sobre Hispa-DOS como en MS-DOS, HSPSND no debe estorbarles (si están cargados o Windows toma la tarjeta, se aparta), y el usuario tiene que poder usar lo nuestro tanto en Hispa Ventanas como en Windows 3.11. |
| Red | `WINSOCK.DLL` (Winsock 1.1) | TCP/IP sobre los *packet drivers* de la fase 0.5, para navegadores y clientes de correo de la época. |

Herramientas previstas para el código Win16: **Open Watcom** (libre), que genera ejecutables y drivers de 16 bits para Windows 3.x.

## Arranque en PCs actuales (UEFI)

Muchos PCs modernos ya no traen CSM (arranque de BIOS clásica). Para ellos se estudiará un cargador UEFI que prepare un entorno BIOS compatible (por ejemplo con SeaBIOS como CSM) antes de arrancar Hispa-DOS.
