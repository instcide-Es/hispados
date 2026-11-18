# HispaBIOS: Hispa-DOS en equipos que solo arrancan por UEFI

**Estado (18/11/2026):** versión 0.3. Probada en QEMU con OVMF (64 y 32 bits,
i440fx y q35, hasta 8 GB, pantalla de 1920x1200). La 0.2 arrancó en dos equipos
de verdad —ThinkPad E14 (Raptor Lake, 1920x1200, la pantalla en 256 GB) y
MacBook Air de 2017 (1440x900)— pero «a 1 FPS» y sin Hispa Ventanas; la 0.3
lleva el arreglo (ver «La lentitud de la 0.2») y **está pendiente de probar en
esos equipos** con la imagen de prueba del FTP, carpeta `prueba-hispabios`.

## Qué es

Los equipos sin CSM (sin «arranque heredado») no tienen BIOS: no hay INT 13h,
INT 10h ni INT 16h, y Hispa-DOS, que es un DOS de modo real, no puede arrancar.
HispaBIOS es una aplicación UEFI (`\EFI\BOOT\BOOTX64.EFI` y `BOOTIA32.EFI` en la
memoria USB) que **hace de BIOS**: pide al firmware la memoria baja, pone en
ella una BIOS de modo real propia, baja el procesador al modo real y arranca el
sector de arranque de siempre. Cuando DOS pide algo a la BIOS (disco, teclado,
pantalla), la BIOS vuelve al modo del firmware, se lo pide a él (Block I/O,
entrada de texto, GOP) y baja otra vez. En los equipos con BIOS o CSM no
interviene: la memoria arranca por el MBR.

| Fichero | Qué es |
|---|---|
| `hispabios/efi64.asm` | La parte UEFI de 64 bits: memoria, pantalla, discos, el registro y el despachador de las peticiones |
| `hispabios/efi32.asm` | Lo mismo para firmware de 32 bits (tabletas y portátiles con Atom) |
| `hispabios/bios64.asm`, `bios_int.inc` | La BIOS de modo real y los cambios de modo (con `-DARQ32`, para firmware de 32 bits) |
| `hispabios/pe.inc` | La cabecera PE/COFF, escrita a mano |
| `hispabios/hblog.asm` | `HBLOG.COM`: enseña y guarda (`\HBLOG.TXT`) el registro de la BIOS de modo real |

## Lo que hace al arrancar (efi64.asm)

1. Quita el **vigilante** del firmware (`SetWatchdogTimer(0)`), que reinicia el
   equipo a los 5 minutos de arrancar un cargador.
2. Lee el **mapa de memoria** (`GetMemoryMap`): primero pregunta el tamaño y
   pide el búfer con hueco de sobra (el mapa de un equipo actual tiene cientos
   de entradas y crece al pedir el propio búfer); respeta el tamaño de entrada
   que diga el firmware (48 octetos en OVMF, no 40). En pantalla enseña lo que
   hay por debajo de 1 MB; el mapa entero va al registro.
3. Pide al firmware, **página a página, la memoria baja** (0 a 9FFFFh,
   `AllocatePages(AllocateAddress)`). La memoria convencional de DOS llega
   hasta la primera página (de la 1 en adelante) que el firmware no da.
4. Elige **dónde va la BIOS de modo real** (60 KB seguidos): 90000h o, si el
   firmware usa esa zona (es corriente que tenga reservadas las últimas páginas
   antes de A0000h), 70000h o 60000h. Hay una copia de la BIOS ensamblada para
   cada sitio (`BIOS64.BIN`, `BIOS64_7.BIN`, `BIOS64_6.BIN`). En 80000h no puede
   ir: ahí se copia el sector de arranque de Hispa-DOS (`RELOC_SEG`), y por lo
   mismo esa página tiene que estar libre.
5. Busca la pantalla (GOP, 640x400 o más, color de 32 bits) y los discos (Block
   I/O de 512 octetos; el de arranque es el 80h), copia la BIOS y la arranca.

Desde la 0.3, además: **presta a DOS las páginas sueltas** de memoria baja que
usa el firmware, **copia las entradas de ACPI y SMBIOS** donde las ve un
programa de modo real, prepara la pantalla (**escritura combinada**, medida de
la velocidad) y el **marco de VBE**. Todo queda en el registro.

### Modo seguro

Si al arrancar se va pulsando la tecla **S**, HispaBIOS no hace nada de lo que
depende del equipo: ni el alias de escritura combinada, ni cambiar de modo de
pantalla, ni prestar páginas. Es la salida si un firmware se lleva mal con algo
de eso (sale `MODO SEGURO` y queda en el registro).

## La memoria baja: páginas prestadas

Casi todos los firmware se quedan una página suelta de la memoria baja (la del
arranque de los otros procesadores): 87000h en el ThinkPad E14, 58000h en el
MacBook Air, que dejaba a DOS con **352 KB**. Como la página 0, esas páginas
se **prestan**: en cada cambio de modo se intercambia su contenido
(`paginas_cambiar` en `bios64.asm`), el firmware ve siempre lo suyo y DOS lo
suyo. Solo las que son memoria normal del firmware según el mapa (tipos 0 a 4,
con atributos de caché), por debajo de la BIOS de modo real y como mucho
cuatro; el resto sigue cortando la memoria convencional. Con ello los dos
equipos dan 576 KB. También se presta la del sector de arranque (80000h).

## ACPI y SMBIOS

Con UEFI la RSDP de ACPI y la entrada de SMBIOS solo están en la tabla de
configuración del firmware; EQUIPO decía «ACPI: no hay» y HSPACPI (la batería)
no habría encontrado nada. HispaBIOS copia las dos entradas a una «EBDA» de
1 KB dentro de su zona (40:0Eh apunta a ella): la RSDP en +10h y la de SMBIOS
(«_SM_» o «_SM3_») en +40h. Si además F0000h es memoria libre que se puede
escribir (lo que haría un CSM) las pone también allí; en OVMF no lo es, y por
eso EQUIPO busca SMBIOS también en la EBDA. Las tablas en sí no se mueven.

## La pantalla

La pantalla de texto de DOS (80x25) se dibuja ampliada sobre el marco del
firmware (GOP); solo las celdas que cambian.

- **Escritura combinada (WC).** El firmware deja el marco sin caché (UC) y cada
  punto escrito va al bus por separado. HispaBIOS se hace unas tablas de
  páginas propias —copia del nivel 4 del firmware más una entrada con el marco
  en páginas de 2 MB de tipo WC (la entrada 5 del PAT)— y pinta por ese
  **alias**, que solo está puesto mientras dibuja. Vale igual con el marco por
  encima de 4 GB. No se tocan los MTRR ni las tablas del firmware.
- **Medida.** Al arrancar pinta la pantalla entera en negro por los dos caminos
  y apunta lo que tarda cada uno; usa el más rápido.
- **Si aun así es lenta** (más de 100 ms la pantalla entera): pasa al modo más
  grande del firmware que se pinte en ese tiempo (de 800x600 en adelante; el
  equipo lo amplía) y, si no hay, amplía el texto solo por un entero.
- Cada fila de puntos se prepara en memoria normal y va a la pantalla de una
  vez (`pintar_celda`).

### Modos gráficos: VBE

Los equipos que solo tienen UEFI no llevan VBE ni VGA: Hispa Ventanas decía «no
se ha podido poner el modo gráfico». La BIOS de HispaBIOS da **VBE 2.0**
(INT 10h 4F00h a 4F03h) con modos de **color directo de 32 bits y marco
lineal**: 640x480 (112h), 800x600 (115h) y 1024x768 (118h), los que quepan en
la pantalla. El marco es memoria normal **por debajo de 4 GB** (un programa de
32 bits no llega a una pantalla que está en 256 GB); en cada tic la parte UEFI
compara cada línea con lo ya pintado y lleva a la pantalla las que han
cambiado, ampliadas por un entero y centradas (800x600 en 1920x1200: al doble).
No hay modos de 8 bits (su paleta se pone por los puertos de la VGA, que no
existen) ni ventanas de 64 KB en A0000h. INT 10h AH = 00h vuelve al texto.

## El teclado y el reloj

- DOS mira el teclado en cada letra que escribe (Ctrl+C) y cada pregunta era
  un viaje al firmware con 1 ms de espera: era lo que más frenaba. Ahora INT
  16h AH = 01h pregunta al firmware **como mucho cada octavo de tic** (7 ms).
- El reloj de DOS es el PIT por el 8259 y LINT0; los tics que se pierden se
  recuperan con el TSC. Si la IRQ 0 no llega (hay placas con el 8254 parado),
  los tics se dan al entrar en las interrupciones de la BIOS (`k_tics`) y la
  espera de tecla no usa HLT. `HBLOG` dice si llega.

## Cuando algo falla

No se queda parado: dice en pantalla **qué falta y qué hacer**, guarda el
registro en la memoria USB y espera una tecla (o 2 minutos) antes de volver al
firmware.

| Aviso | Causa | Qué hacer |
|---|---|---|
| `ESTE EQUIPO NO DEJA LIBRE LA MEMORIA BAJA QUE NECESITA HISPA-DOS` | No hay 60 KB seguidos libres en 60000h, 70000h ni 90000h, o el firmware usa la página 80000h | Activar el CSM / arranque heredado, si el equipo lo tiene |
| `ESTE EQUIPO DEJA MUY POCA MEMORIA CONVENCIONAL` | Menos de 256 KB seguidos desde abajo | Lo mismo |
| `ESTE EQUIPO NO DA UNA PANTALLA GRAFICA (GOP)...` | Sin GOP, o con un formato de color que no es de 32 bits | Lo mismo |
| `NO SE ENCUENTRA EL DISCO DE ARRANQUE` | El firmware no da la memoria USB como disco de sectores de 512 | — |
| `No se ha podido leer el mapa de memoria...` | `GetMemoryMap` falla (sale el código) | — |

### El registro de la BIOS: `HBLOG`

Mientras DOS funciona, la BIOS de modo real apunta lo que sirve para depurar
la velocidad: cuántas veces ha dibujado la pantalla y lo que ha tardado (el
último dibujo, el más lento, el total), los viajes al firmware, las preguntas
por el teclado, si llega la IRQ 0 y **cada llamada de VBE con lo que se
contestó** (también un modo de la VGA que no hay). `HBLOG` (en `\HISPADOS`) lo
enseña y lo guarda en `\HBLOG.TXT`; lo pide por INT 15h AX = 4844h.

### El registro: `HISPABIOS.LOG`

Si algo falla, HispaBIOS deja en la raíz de la memoria USB el archivo
**`HISPABIOS.LOG`** (texto; DOS lo enseña como `HISPAB~1.LOG`) con:

- el fabricante y la revisión del firmware (`FirmwareVendor`,
  `FirmwareRevision`) y la versión de UEFI;
- el **mapa de memoria completo** (inicio, páginas, tipo y atributos de cada
  tramo);
- las páginas bajas que el firmware no ha dado, dónde va la BIOS y cuánta
  memoria convencional queda;
- las páginas prestadas, la pantalla (GOP) y **los modos que da el firmware**,
  **el tipo de caché del marco (MTRR y PAT), el alias de escritura combinada y
  lo que cuesta pintar la pantalla** por cada camino, el marco de VBE, las
  tablas de ACPI y SMBIOS, la memoria extendida, el vector del 8259 y los
  discos;
- todo lo que ha salido en pantalla, hasta el aviso del fallo.

Si todo va bien **no se escribe nada** en la memoria. Para un equipo que se
queda parado sin llegar a avisar está la **imagen de prueba**
(`tools/mkprueba_hispabios.sh`, ensamblada con `-DREGISTRO_PASOS`): guarda el
registro en cada paso, así que el archivo dice hasta dónde llegó.

De momento el registro y el mapa solo los tiene la parte de 64 bits;
`efi32.asm` tiene la elección de la base, los avisos y el vigilante.

## El fallo de la 0.9.26 en un PC actual

Arrancando por UEFI en un PC actual, HispaBIOS 0.1 se quedaba en:

    HispaBIOS 0.1 (x64) - paso 2: la memoria baja
    Mapa de memoria por debajo de 1 MB (inicio, paginas, tipo):

**Causa:** la convención de llamadas de UEFI en x64 exige la **pila alineada a
16 octetos** en cada llamada al firmware. `hex64` (la rutina que escribe los
números) llamaba a `escribir` con la pila descuadrada en 8, y la primera cifra
del mapa —justo después de ese título— era la primera llamada al firmware con
la pila mal (también `memoria_ext`). El firmware de los equipos actuales está
compilado con SSE y guarda datos alineados en la pila (`movaps`): con la pila
mal, excepción y equipo parado. OVMF (el firmware de QEMU) se compila sin SSE
y el del MacBook de 2012 tampoco tropezaba: por eso no se había visto.

**Arreglo:** todas las llamadas al firmware van por la macro `FW`; `escribir`
alinea la pila ella misma, venga de donde venga, y `memoria_ext` la lleva
cuadrada. Con `-DPRUEBA_PILA` la macro comprueba la alineación en cada llamada
y avisa por el puerto serie (`PILA-MAL`): la prueba arranca hasta `C:\>` y
exige que no salga ni una vez. (Con la 0.1 instrumentada igual salen 13.)

De paso, lo que también podía parar un equipo actual y ahora no: el búfer fijo
de 16 KB para el mapa de memoria, la BIOS puesta en 90000h sin mirar si el
firmware había dado esas páginas (se escribía encima de lo suyo), una línea
por cada página no dada (160 líneas que se llevaban la pantalla; ahora, por
tramos), el vigilante de los 5 minutos y la espera muda de 60 segundos.

## La lentitud de la 0.2 («1 FPS») y Hispa Ventanas

En el ThinkPad E14 y en el MacBook Air de 2017 la 0.2 arrancaba, pero la
pantalla iba a tirones, y `VENTANAS` contestaba «no se ha podido poner el modo
gráfico». Causas y arreglos:

1. **El teclado** (medido en QEMU). DOS mira el teclado en cada letra que
   escribe; cada mirada era un viaje al firmware (dos cambios de modo, 1 ms de
   espera y lo que tarde su teclado). `DIR C:\HISPADOS` (75 líneas) en QEMU:
   **11 s con la 0.2, 1 a 2 s con la 0.3**; con la 0.3 sin la medida del
   teclado, otra vez 10 a 12 s. En el Mac, además, el firmware usa el 8259
   (vector 68h) y los tics que caían durante esos viajes se los quedaba él:
   la pantalla, que se dibuja en el tic, se refrescaba muy de tarde en tarde.
2. **La pantalla sin escritura combinada** (no se puede medir en QEMU, donde la
   pantalla es memoria normal: lo dirá el registro del equipo). Con el marco
   en UC, pintar 1920x1200 entera —cada vez que el texto sube una línea— es
   del orden de un segundo. Arreglo: el alias WC, la medida y el modo menor.
3. **Sin VBE**: Hispa Ventanas caía al modo 12h de la VGA, que tampoco hay.
   Arreglo: VBE de 32 bits con el marco por debajo de 4 GB.

Queda por saber en los equipos de verdad (lo dirán `HISPABIOS.LOG` y
`HBLOG.TXT`): lo que cuesta pintar con y sin WC, si llega la IRQ 0 y si el
ratón funciona (con UEFI no hay ratón USB: HSPUSB no se carga).

## Pruebas

`tests/run_hispabios_tests.sh` (en `make test`; necesita los paquetes `ovmf` y
`ovmf-ia32`). Además del arranque de siempre (64 y 32 bits, «Juegos», modo
«irreal»), ensambla variantes de `BOOTX64.EFI` con las opciones de prueba:

| Opción de `efi64.asm` | Para qué |
|---|---|
| `-DPRUEBA_PILA` | Avisa por COM1 de una llamada al firmware con la pila sin alinear |
| `-DPRUEBA_OCUPA=dir,n,...` | Ocupa antes esas páginas, como un equipo cuyo firmware las usa (también en `efi32.asm`) |
| `-DPRUEBA_SIN_GOP` | Como si no hubiera pantalla gráfica |
| `-DREGISTRO_PASOS` | Guarda `HISPABIOS.LOG` en cada paso (la imagen de prueba) |
| `-DPRUEBA_LENTA=us` | Como si pintar un millón de puntos costara eso (pantalla lenta) |
| `-DPRUEBA_SIN_WC` | Sin el alias de escritura combinada |
| `-DPRUEBA_SIN_PRESTAR` | Sin prestar a DOS las páginas bajas del firmware |
| `-DPRUEBA_SEGURO` | Como si se hubiera pulsado S (modo seguro) |
| `-DPANTALLA_NATIVA` | No cambiar de modo de pantalla aunque sea lenta |

Casos: equipo «actual» (q35, 8 GB) con la pila comprobada; memoria baja
ocupada (BIOS en 70000h y en 60000h, en 64 y 32 bits) hasta `C:\>` y con el
registro completo; páginas sueltas prestadas (58000h, 80000h y 87000h: 576 KB);
sin memoria baja libre y sin GOP (aviso, qué hacer y registro guardado);
pantalla de 1920x1200 (el alias WC, la velocidad de `DIR`, `HBLOG`, EQUIPO con
ACPI y SMBIOS, Hispa Ventanas en VBE con captura); pantalla lenta simulada
(modo menor del firmware) con `VBEPRU.COM` (`tests/progs/vbepru.asm`: las
llamadas de VBE de Hispa Ventanas, el marco lineal y el registro).

OJO con QEMU y KVM: con el 8259 del núcleo (KVM) la IRQ 0 por LINT0 no llega
tras enmascarar y abrir LINT0, cosa que un equipo de verdad sí hace; las
pruebas van sin KVM.

## Pendiente

- Probar la 0.3 en el ThinkPad E14 y el MacBook Air (imagen de prueba) y ver
  `HISPABIOS.LOG` y `HBLOG.TXT`.
- **Ratón** con UEFI: no hay ratón USB (el USB lo lleva el firmware). Haría
  falta sacarlo del firmware (Simple Pointer) y darlo por INT 33h / INT 15h C2h.
- **Alt y Ctrl** con las teclas de función (Alt+F4 en Hispa Ventanas): el
  teclado del firmware da el estado de las teclas, la BIOS aún no lo traduce.
- Modos VBE de 8 bits (con la paleta por INT 10h 4F09h) para juegos.
- Si en algún equipo la IRQ 0 no llega: el temporizador del APIC local.
- La memoria extendida se pide a potencias de dos desde 1 MB (en OVMF, 4 MB;
  64 MB como mucho): usar el mapa para pedir el tramo entero.
- En `efi32.asm`: el registro, el mapa, VBE, la escritura combinada, las
  páginas prestadas y las tablas (la BIOS de 32 bits contesta que no hay VBE).
