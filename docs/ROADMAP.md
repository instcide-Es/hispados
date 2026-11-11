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
- Particiones extendidas y unidades lógicas; `SYS` para hacer arrancable una unidad ya formateada.
- Funciones FCB (0Fh-29h) para programas antiguos.
- ✅ `COMMAND.COM`: redirecciones `<`, `>`, `>>`, tuberías `|`, `COPY`, `DEL`, `REN`, `MD`, `RD`, `PAUSE`.
- ✅ `COMMAND.COM`: ficheros `.BAT` y `AUTOEXEC.BAT`, `SET` con asignación, `IF`, `GOTO`, `FOR`, `CALL`, `SHIFT`, `PROMPT`, `PATH`, `/C`, `/P`.
- Ctrl+C y Ctrl+Break, INT 23h e INT 24h completas.
- Mensajes en español además de en inglés.

## 0.3 — Drivers y memoria

- `CONFIG.SYS`: `DEVICE=`, `DEVICEHIGH=`, `FILES=`, `BUFFERS=`, `LASTDRIVE=`, `SHELL=`, `DOS=HIGH,UMB`.
- Interfaz de drivers de DOS (`.SYS` de carácter y de bloque, *strategy*/*interrupt*), cabeceras y cadena de dispositivos reales.
- INT 2Fh multiplex completo.
- **HSPXMS.SYS**: gestor XMS 3.0 (compatible con HIMEM), A20 y HMA.
- **HSPEMM.EXE**: UMB y EMS 4.0 en modo V86 (como EMM386).

## 0.4 — Compatibilidad con Windows 3.1

Windows 3.1 se apoya en estructuras internas de DOS, no solo en la API pública. Hay que implementarlas igual que las documenta la Interrupt List:

- Lista de listas (52h) completa: DPB, SFT encadenadas, CDS, cabecera de dispositivos NUL.
- *Swappable Data Area* (5D06h/5D0Bh), secciones críticas INT 2Ah, `ErrorMode`, `InDOS`.
- Funciones de red y redirector 5Dh/5Eh/5Fh y difusión de arranque de Windows (INT 2Fh 1605h/1606h).
- Objetivos de prueba: Windows 3.1 en **modo estándar** primero (con HSPXMS.SYS) y después en **386 mejorado**, en QEMU y 86Box.
- Windows 3.1 no se distribuye: cada persona instala su propia copia.

## 0.5 — Drivers para hardware actual (DOS)

| Área | Driver | Notas |
|---|---|---|
| Vídeo | `HSPVBE` | VESA VBE 2.0/3.0, framebuffer lineal y modos panorámicos; API VBE para juegos y programas. |
| Ratón | `HSPMOUSE.COM` (INT 33h) | PS/2; USB a través de la emulación *legacy* de la BIOS y, más adelante, pila USB propia (UHCI/OHCI/EHCI/xHCI, HID). |
| Sonido | `HSPSB` | Tarjetas HDA y AC'97 con emulación de Sound Blaster 16 en modo V86 (atrapando puertos de E/S), para que los juegos de DOS suenen. |
| Red | Packet drivers | Especificación de *packet driver* para Intel e1000/e1000e, Realtek 8139/8169 y virtio-net. TCP/IP con aplicaciones compatibles (como mTCP) y una pila propia `HSPNET`. |
| Discos | `HSPAHCI`, `HSPNVME` | Acceso a SATA (AHCI) y NVMe cuando la BIOS no los ofrece por INT 13h. |
| USB almacenamiento | `HSPUSB` | Memorias USB como unidades de DOS. |

## 0.6 — Drivers para Windows 3.1

| Área | Driver | Notas |
|---|---|---|
| Vídeo | `HSPVBE.DRV` + VxD | Driver de pantalla de Windows 3.1 sobre VBE: alta resolución y color de 16/24/32 bits. |
| Ratón | `HSPMOUSE.DRV` | Usa INT 33h de HispaDOS; puntero absoluto en máquinas virtuales. |
| Sonido | `HSPWAVE.DRV` | Driver MMSYSTEM (wave y MIDI por software) para HDA/AC'97. |
| Red | `WINSOCK.DLL` (Winsock 1.1) | TCP/IP sobre los *packet drivers* de la fase 0.5, para navegadores y clientes de correo de la época. |

Herramientas previstas para el código Win16: **Open Watcom** (libre), que genera ejecutables y drivers de 16 bits para Windows 3.x.

## Arranque en PCs actuales (UEFI)

Muchos PCs modernos ya no traen CSM (arranque de BIOS clásica). Para ellos se estudiará un cargador UEFI que prepare un entorno BIOS compatible (por ejemplo con SeaBIOS como CSM) antes de arrancar HispaDOS.
