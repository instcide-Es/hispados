# Las letras de unidad

Cómo reparte Hispa-DOS las letras (A:, C:, D:... X:) entre los disquetes, el
disco duro, el CD/DVD y las memorias USB, cómo se cambia y cómo se protege la
memoria de la que se ha arrancado.

## El reparto normal

| Letra | Qué es |
|-------|--------|
| A:, B: | Disquetes. |
| C: | **El disco duro interno**: su primera partición FAT. Si el disco no tiene ninguna (está sin particionar, sin formato, o es de otro sistema), la C: queda **reservada** para él igualmente. |
| D:, E:... | Las demás particiones de los discos internos, en el orden de MS-DOS (primero las primarias de cada disco, luego las unidades lógicas). Detrás, en la **primera letra libre**, los lectores de CD/DVD (HSPCD) y los demás controladores de bloque (HSPRAM...). |
| X:, W:, V:... | Las **memorias USB**, de la X: hacia abajo. La primera, la de arranque. |

Ejemplos:

- Memoria USB de arranque + disco duro con una partición + un lector: C: el
  disco, D: el CD, X: la memoria.
- Lo mismo con dos lectores: D: y E:.
- Disco con dos particiones (C: y D:) y un lector: el CD es E:. Quien ya tenía
  su segunda partición en D: la conserva.
- Memoria USB + disco duro vacío (o con Windows/Linux, sin FAT): C: reservada,
  X: la memoria.
- Memoria USB sola (el equipo no tiene disco duro): la memoria es la C:, como
  siempre.
- Disquete de arranque + disco duro: A: y C:, como siempre.
- Equipo sin ningún disco duro (solo disquete): la C: está libre y la toma
  el primer lector de CD/DVD o disco RAM, como siempre. Las memorias USB
  que lleva HSPUSB van a la X: también ahí.
- Lo instalado en el disco duro arranca de C:, como siempre.

La orden **UNIDADES** enseña qué es cada letra y de cuál se ha arrancado;
`EQUIPO /DISCOS` (y el informe de DIAGNOSTICO) llevan la misma lista.

    X:\>UNIDADES
    Unidades de Hispa-DOS
      A:  Disquete
      C:  Disco duro interno (disco 2), FAT16, 63 MB
      D:  Lector de CD/DVD
      X:  Memoria USB (disco 1), FAT16, 511 MB  <- de aquí ha arrancado Hispa-DOS

### La C: reservada

Cuando el disco duro interno no tiene ninguna partición FAT, la C: existe pero
no se puede leer:

    X:\>DIR C:
    No se puede leer la unidad C: es el disco duro del ordenador, que todavía no
    tiene partición ni formato. Use INSTALAR (o PARTIR y después FORMATEA).

- `INSTALAR` (o `INSTALAR C:`) lo hace todo: crea la partición, le da formato
  y copia el sistema.
- A mano: `PARTIR` crea la partición (el núcleo vuelve a repartir las letras
  al momento, sin reiniciar) y `FORMATEA C:` le da formato.
- `FORMATEA C:` sobre la C: reservada no hace nada: pide crear antes la
  partición (no da formato al disco entero). INT 25h/26h tampoco la leen ni
  la escriben.

Una partición FAT que existe pero no tiene formato dice:
«No se puede leer la unidad C: el disco no tiene formato. Use FORMATEA.»

### Cómo sabe cuál es el disco interno y cuál la memoria USB

El disco de arranque es el que la BIOS pasa al arrancar (DL). Un disco de la
BIOS es una **memoria USB** (o un disco extraíble) si:

1. lo dice la BIOS: INT 13h AH=48h (EDD 3.0) con interfaz «USB» o «1394», o
   con la bandera de «medio extraíble»; o
2. es el de arranque y su sector de arranque lleva la marca **`HISPAUSB`** en
   el nombre OEM (octetos 3 a 10). La ponen `tools/mkusb.py` y las imágenes
   que salen de ella (las de prueba, las de HispaBIOS). Muchas BIOS no dicen
   nada del USB (SeaBIOS, la de QEMU, tampoco): por eso la marca. FORMATEA,
   SISTEMA e INSTALAR escriben «HISPADOS»: el disco donde se instala no la
   lleva (SISTEMA la conserva si la unidad ya la tenía).

El **disco interno** es el primer disco de la BIOS que no es una memoria USB.
Si todos lo son (o solo está la memoria de arranque), la de arranque hace de
disco interno y es la C:.

Con **HispaBIOS** (equipos UEFI) el disco de arranque llega siempre como 80h
y los demás detrás: vale lo mismo, por la marca.

## Cambiarlo: LETRAS= en CONFIG.SYS

Una línea, con una o varias opciones separadas por comas:

    LETRAS=CD:R,USB:U
    LETRAS=CLASICO

| Opción | Efecto |
|--------|--------|
| `CD:R` | El primer lector de CD/DVD es la R: (los siguientes, las libres detrás). Si la letra está ocupada se usa la siguiente libre y se avisa en una línea al arrancar. De la D: a la X:. |
| `USB:U` | Las memorias USB, de la U: hacia abajo. |
| `CLASICO` | El reparto de siempre, el de MS-DOS: todo seguido desde la C: en el orden de la BIOS, sin reservas (arrancando de una memoria USB, la memoria es C: y el disco duro D:). Para el equipo o el programa que lo necesite. |
| `INTERNO` | El reparto normal (por si se quiere dejar escrito). |

Las letras se reparten antes de cargar ningún controlador. `LASTDRIVE` no hace
falta para la X:: el núcleo admite hasta la X: aunque no se ponga (los
CONFIG.SYS de los medios llevan `LASTDRIVE=Z`).

Las letras llegan hasta la **X:** (24 unidades). Y: y Z: no se usan.

## La memoria de arranque no se borra por error

PARTIR, FORMATEA, SISTEMA e INSTALAR se niegan a tocar la memoria USB de la
que se ha arrancado (su unidad y cualquier otra partición del mismo disco):

    X: es la memoria USB desde la que ha arrancado Hispa-DOS: no se puede
    partir ni formatear. (Quien sepa lo que hace: /FORZAR.)

Con **`/FORZAR`** se puede (`PARTIR 1 /DEL:1 /FORZAR`, `FORMATEA X: /FORZAR`,
`SISTEMA A: X: /FORZAR`); PARTIR pide además escribir SI. En el menú de PARTIR
la memoria sale como «Memoria USB de arranque (no tocar)» y se empieza por el
disco duro. Vale también con `LETRAS=CLASICO` y cuando la memoria es la C:.

El disco duro de arranque (el sistema instalado) sigue como antes: PARTIR pide
escribir SI.

## Los archivos de arranque no llevan la letra

Como la memoria USB puede ser la X: (o la C:), sus archivos no dicen «C:»:

- **CONFIG.SYS**: rutas sin unidad, `DEVICE=\HISPADOS\HSPXMS.SYS` (mientras se
  lee CONFIG.SYS la unidad actual es la de arranque).
- **AUTOEXEC.BAT**: con la variable **`ARRANQUE`**, que pone el núcleo con la
  unidad de la que se arrancó (`ARRANQUE=X:`): `PATH %ARRANQUE%\HISPADOS`,
  `%ARRANQUE%\HISPADOS\HSPRATON`...
- `COMSPEC` y el `PATH` inicial llevan la unidad de arranque de verdad.
- **CENTRO.INI** (Hispa Ventanas): `Programa=` y `Carpeta=` pueden ir sin
  unidad (`Programa=\HISPADOS\PROGS\PLUMA.HVP`); Hispa Ventanas les pone la
  suya al leerlo y, si está en la memoria USB de arranque y esta no es la C:,
  las guarda otra vez sin ella. Así el mismo archivo vale en la memoria y,
  una vez instalado, en el disco duro. (Desde un disquete o desde el disco
  duro se guardan con su unidad, como siempre.)

En el repositorio, `usb/CONFIG.SYS` y `usb/AUTOEXEC.BAT` siguen escritos con
`C:\`; `tools/mkusb.py` (función `sin_letra`) los cambia al hacer la imagen, y
quita la unidad al CENTRO.INI de Hispa-486. INSTALAR los copia tal cual: en el
disco duro valen igual (ahí `ARRANQUE=C:`).

Los programas preguntan la unidad de arranque al núcleo (INT 21h AX=3305h):
INSTALAR, ACTUALIZAR, EQUIPO/DIAGNOSTICO (\INFORMES), HSPSND, HSPACPI, WIFI...

## Para programadores: INT 21h AX=3368h y AX=3369h

Propias de Hispa-DOS. Antes de llamar, BX = 0; si vuelve BX = 4448h ('HD'),
la función existe.

**AX=3368h** — el arranque y el reparto:

- DL = unidad de arranque (1 = A:); DH = su disco de la BIOS.
- CH = disco de la BIOS que es el duro interno (0 = no hay).
- CL: bit 0 = se arrancó de una memoria USB (o disco extraíble); bit 1 =
  reparto normal (0 = clásico); bit 2 = la C: está reservada (sin partición).

**AX=3369h**, DL = unidad (0 = A:) — qué es:

- AL = 0 no existe, 1 disquete, 2 partición de un disco de la BIOS, 3 CD/DVD
  (HSPCD), 4 de un controlador de soporte extraíble (HSPUSB), 5 de otro
  controlador, 6 la C: reservada.
- AH: bit 0 = es la de arranque; bit 1 = del disco duro interno; bit 2 = de
  una memoria USB.
- CL = disco de la BIOS; CH = tipo de partición; SI:DI = primer sector;
  DX = tamaño en MB.

## En el núcleo

`kernel/kernel.asm`, marcas `[LETRAS]`: `hd_plan` (clasifica los discos y
elige el interno), `disk_usb` (EDD y la marca), `hd_slot`/`slot_asc`/
`slot_desc` (la letra de cada partición), `c_reserva` (la C: reservada),
`ext_slot` (la letra de las unidades de los controladores: HSPCD, HSPUSB,
otros), `uni_scan` (LETRAS=). La tabla de unidades (`NDRV`) pasó de 16 a 24.

## Lo que cuesta

- La tabla de unidades de 24 (antes 16) ocupa 1,2 KB más de memoria
  convencional (2 KB sin DOS=HIGH, por el código nuevo).
- El núcleo tiene que caber, al arrancar, por debajo de `INIT_ORG` (ahora
  F400h): quedan unos 250 octetos de margen. Si hace falta más, `.init` ocupa
  1,8 KB de los 2 KB que tiene reservados.
- UNIDADES.COM va en la imagen para memorias USB (y en lo que se instala
  desde ella); en el disquete de arranque no (no sobra sitio): ahí,
  `EQUIPO /DISCOS`.

## Programas antiguos y letras altas

- Un juego instalado en C: que guarda su ruta («C:\JUEGO») sigue igual.
- Los instaladores de los 90 que solo ofrecen de la C: a la F: no ven la X:;
  no hace falta: se instala en la C:.
- Algún programa viejo da por hecho que el CD es la D:. Lo es con un disco de
  una sola partición; si no, `LETRAS=CD:D` no puede quitarle la D: a una
  partición (se usa la siguiente libre y se avisa): ahí toca reinstalar el
  programa diciéndole la letra buena, como en MS-DOS.
- Si algún equipo o programa se lleva mal con el reparto: `LETRAS=CLASICO`.

## Pruebas

`tests/run_letras_tests.sh`: memoria USB + disco con FAT + CD (Normal, Juegos,
Prueba, Hispa Ventanas), disco vacío (C: reservada, PARTIR, FORMATEA),
protección de la memoria y /FORZAR, memoria sola, disquete, dos particiones,
LETRAS=CLASICO y LETRAS=CD:R,USB:U, e Hispa-486 con la memoria en la X:.
