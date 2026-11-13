# HSPEMM.SYS: EMS, VCPI y memoria superior

`HSPEMM.SYS` es el «EMM386» de Hispa-DOS: un monitor de modo virtual 8086
escrito desde cero en NASM a partir de las especificaciones públicas (LIM EMS
4.0, VCPI 1.0, XMS 3.0 y el manual del programador del Intel 386). Da:

- **EMS 4.0** (INT 67h): los juegos y programas que piden memoria expandida
  (Aladdin: «EMS allocation error» sin ella).
- **VCPI 1.0** (INT 67h AX=DExxh): los extensores de DOS (DOS/4GW de Doom,
  Duke Nukem 3D, Mortal Kombat…) y `HSPDPMI`, que con HSPEMM cargado pasa a
  ser cliente VCPI (Hispa Ventanas sigue funcionando).
- **Memoria superior (UMB)**: `DOS=UMB`, `DEVICEHIGH=` y `LH`/`LOADHIGH`.

## Uso

En `CONFIG.SYS`, justo después de `HSPXMS.SYS`:

    DEVICE=C:\HISPADOS\HSPXMS.SYS
    DEVICE=C:\HISPADOS\HSPEMM.SYS
    DOS=HIGH,UMB
    DEVICEHIGH=C:\HISPADOS\HSPCD.SYS

| Opción | Efecto |
|---|---|
| *número* | KB para EMS y VCPI (por omisión, la mitad de la memoria extendida libre, 32 MB como mucho) |
| `NOEMS` | sin marco de página: no hay EMS (el dispositivo se llama `EMMQXXX0`), pero sí VCPI y 64 KB más de memoria superior |
| `NOUMB` | no ofrece memoria superior |
| `FRAME=xxxx` | segmento del marco de página (por omisión, los 64 KB libres más altos de C000-DFFF; normalmente D000) |
| `I=xxxx-yyyy` | incluir esa zona en la búsqueda de memoria superior libre (por omisión solo C000-DFFF) |
| `X=xxxx-yyyy` | excluirla (una tarjeta con memoria propia) |

Al arrancar dice lo que ha encontrado:

    HSPEMM 0.1: EMS 4.0 y VCPI, 15696 KB; marco de página en D000; memoria superior: 12 KB ( CD00-CFFF)

`MEM` y `MEM /C` enseñan la memoria superior; `LH programa` carga un programa
en ella (si no cabe, abajo, como siempre) y `DEVICEHIGH=` un controlador (si
al instalarse arriba falla o no cabe, se carga abajo).

## Lo que no funciona en modo V86

Con HSPEMM el procesador está en modo protegido y DOS corre en modo V86. Lo
que necesita el modo real «de verdad» no puede funcionar, y lo dice:

- `HSPUSB` llevando él el USB (`/U`, `/F`): usa el modo irreal. Sin opciones
  (USB por la BIOS) sigue igual.
- `HSPCD` con lectores **SATA (AHCI)**; los IDE funcionan.
- `HSPSND` con **Intel HD Audio**; Sound Blaster funciona (también con
  `LH HSPSND`).
- Programas que pasan a modo protegido por su cuenta (sin VCPI ni DPMI): el
  monitor los termina con «HSPEMM: el programa quiere pasar a modo
  protegido por su cuenta…».
- `HSPDPMI` necesita con HSPEMM un Pentium o superior (páginas de 4 MB) y que
  HSPEMM esté cargado justo tras HSPXMS.
- **Windows en modo 386 mejorado** arranca con HSPEMM cargado (probado con
  Windows para Trabajo en Grupo 3.11): al arrancar pregunta (INT 2Fh
  AX=1605h) y HSPEMM le entrega la rutina que apaga el modo V86 y lo vuelve
  a encender al salir (lo mismo que hace EMM386); mientras Windows está en
  marcha el dispositivo pasa a llamarse `EMMQXXX0` (si Windows ve un
  `EMMXXXX0` exige la «importación global de EMM», que no está hecha) y el
  EMS de las sesiones de MS-DOS lo da el propio Windows. Al salir, EMS y VCPI
  siguen como estaban. Solo puede apagarse si no hay memoria superior
  entregada (`DOS=UMB`, `DEVICEHIGH`, `LH`) ni páginas EMS/VCPI en uso; si
  las hay, HSPEMM lo avisa y Windows no arranca: quite `DOS=UMB` para usar
  Windows con HSPEMM.

Por lo anterior, en los `CONFIG.SYS` de la distribución HSPEMM viene con `REM`.

## Diseño

Un solo fichero, `drivers/hspemm.asm`, con tres partes:

1. **Parte residente** (modo V86, unos 1,6 KB de memoria convencional): la
   cabecera de dispositivo `EMMXXXX0` (los programas detectan el EMS
   abriéndolo y mirando el segmento del vector 67h), el vector de INT 67h, el
   de INT 15h (AH=87h), el de INT 2Fh (Windows), el gestor de UMB y las dos
   rutinas de cambio de modo. Llama al monitor con una instrucción no válida
   en modo V86 (`ARPL`), que el monitor reconoce por el segmento.
2. **Inicialización** (se descarta): mira la memoria superior (ROM de
   ampliación con firma 55 AA y páginas de contenido uniforme), elige el
   marco y los UMB, pide a HSPXMS un bloque bloqueado, monta en él el
   directorio y las tablas de páginas, copia el monitor (con la función de
   mover de XMS) y pasa a modo V86.
3. **Monitor** (32 bits, anillo 0), en la dirección lineal 110000h:
   - IOPL = 3: `CLI`, `STI`, `PUSHF`, `POPF`, `INT` e `IRET` no se atrapan;
     `INT n` entra por la IDT (256 puertas abiertas al anillo 3) y se
     **refleja** a la tabla de vectores del modo real, igual que las IRQ (el
     PIC sigue en 08h/70h). Excepción o interrupción se distinguen por la
     profundidad de la pila de anillo 0 (código de error o no).
   - #GP: se emulan `HLT` (con el procesador parado de verdad hasta la
     siguiente interrupción), `MOV` con CR0/CR2/CR3 y DR0-DR7, `CLTS`, `LMSW`
     y la E/S de los puertos vigilados. `LGDT`/`LIDT` terminan el programa
     (o reinician el equipo si es la BIOS quien lo hace).
   - **A20 virtual**: puertos 92h y 60h/64h (órdenes D0h, D1h, DDh, DFh). La
     línea de verdad está siempre abierta; cerrarla es mapear el mega 17
     sobre el primero, así que la HMA sigue funcionando (y `DOS=HIGH`).
   - **INT 15h AH=87h** (los movimientos de HSPXMS): copia con dos ventanas
     de 17 páginas; las direcciones del primer mega son las que ve el
     programa (con sus UMB y su marco).
   - **DMA (8237)**: los registros de dirección y de página se vigilan; si
     la dirección cae en un UMB o en el marco EMS se programa la dirección
     física (y se devuelve la «virtual» al leerla). Además el núcleo pasa por
     un búfer bajo las lecturas y escrituras de disco de la BIOS con destino
     por encima de A000 (DMA de la disquetera, controladoras USB/AHCI).
   - **8254**: los modos 6 y 7 se escriben como 2 y 3 (son lo mismo, pero
     QEMU deja el temporizador parado con ellos; Aladdin usa el 7).
   - **EMS**: 40h-48h, 4Bh-54h, 57h-5Dh. `pagemap` es una permutación de las
     páginas: tramos de cada handle, libres y, al final, las de VCPI.
   - **VCPI**: DE00h-DE0Ch y la entrada de modo protegido (DE03h-DE05h,
     DE0Ch). Las páginas de 4 KB salen de las de EMS libres (cuatro por
     página).

Mapa lineal: primer mega identidad salvo UMB y marco; 100000-10FFFF la HMA (o
el espejo); 110000-123FFF el monitor (también en la primera tabla de páginas
de los clientes VCPI); 400000… las ventanas.

En el núcleo: `DOS=UMB` (pide todos los UMB a XMS 10h y los cuelga de la
cadena de MCB con «puentes» de dueño 8 y nombre `SC`, como MS-DOS),
INT 21h AX=5800h-5803h (estrategia: primer, mejor y último ajuste, solo
arriba o arriba primero; y enlace), `DEVICEHIGH` y el búfer bajo del disco.
En el intérprete: `LH`/`LOADHIGH` y `MEM`.

### Con DOS=HIGH

`DOS=HIGH,UMB` funciona con HSPEMM: el núcleo en la HMA y la memoria superior
en la cadena de MCB. `DOS=` se lee antes que el resto de CONFIG.SYS, así que
da igual en qué línea esté.

- La HMA es memoria de verdad (identidad) bajo el monitor. «Cerrar A20» en
  modo V86 es mapear el primer mega sobre ella: el código del núcleo deja de
  verse, igual que en modo real, y los trozos de entrada del núcleo (que
  están abajo) lo notan por la firma de FFFF:0010 y la vuelven a abrir por
  XMS 05h; HSPXMS lo hace por INT 15h o los puertos 92h/60h/64h, que el
  monitor atiende. `HMATEST /A` y `/C` pasan con HSPEMM cargado.
- Todo lo que añade HSPEMM al núcleo en datos está en `.bss` (memoria baja):
  la tabla de zonas de UMB, la estrategia, el enlace y el búfer de paso de
  los sectores (la BIOS necesita direcciones físicas por debajo de A000).
  El código va en `.text` (se va a la HMA con el resto).
- La memoria superior se cuelga de la cadena cuando el código del núcleo ya
  está en su sitio definitivo (en la HMA, o abajo si no la hay): mientras
  espera en lo alto de la memoria convencional, el final de la cadena de MCB
  todavía va a cambiar.
- Windows 386 mejorado: apagar el modo V86 no afecta a la HMA.

## Pruebas

- `tests/run_emm_tests.sh`: EMSTEST, VCPITEST, UMBTEST y EMMMISC
  (`tests/progs/`), opciones, HSPDPMI sobre VCPI, avisos.
- `tests/run_emmhigh_tests.sh`: `DOS=HIGH,UMB` + HSPEMM + `DEVICEHIGH` + `LH`,
  A20 cerrada con el núcleo en la HMA, y la estrategia de reserva
  (`EMMMISC S`), con y sin memoria superior.
- `EMM=1 tests/run_dpmi_tests.sh`, `EMM=1 tests/run_sound_tests.sh` (Sound
  Blaster con `LH HSPSND`: el búfer de DMA en un UMB),
  `EMM=1 tests/run_ventanas_tests.sh` (y `_ventanas3_`, `_ventanas4_`).
- Juegos (con copias propias, fuera del repositorio):
  `tests/tools/emm_game.sh` y `tests/tools/mkhdd.py`. Probados en QEMU:
  Aladdin (EMS), Doom, Duke Nukem 3D y Mortal Kombat (DOS/4GW por VCPI).

## Pendiente

- Importación global de EMM para Windows 386 mejorado con UMB en uso.
- 55h/56h de EMS (mapear y saltar/llamar) y juegos de registros alternativos.
- `INSTALLHIGH`; `DEVICEHIGH /L:` y `LH /L:` (se cargan sin más).
- Liberar las páginas VCPI de un cliente que termina sin devolverlas.
- DMA hacia páginas EMS por encima de 16 MB o que crucen páginas no seguidas.
