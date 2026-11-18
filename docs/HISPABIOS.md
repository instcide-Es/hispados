# HispaBIOS: Hispa-DOS en equipos que solo arrancan por UEFI

**Estado (18/11/2026):** versión 0.2. Probada en QEMU con OVMF (64 y 32 bits,
i440fx y q35, hasta 8 GB) y, la 0.1, en el MacBook Air de 2012. En un PC actual
la 0.1 se quedaba parada al empezar (ver «El fallo de la 0.9.26»); la 0.2 lleva
el arreglo y **está pendiente de probar en ese equipo** con la imagen de prueba
del FTP, carpeta `prueba-hispabios`.

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

### El registro: `HISPABIOS.LOG`

Si algo falla, HispaBIOS deja en la raíz de la memoria USB el archivo
**`HISPABIOS.LOG`** (texto; DOS lo enseña como `HISPAB~1.LOG`) con:

- el fabricante y la revisión del firmware (`FirmwareVendor`,
  `FirmwareRevision`) y la versión de UEFI;
- el **mapa de memoria completo** (inicio, páginas, tipo y atributos de cada
  tramo);
- las páginas bajas que el firmware no ha dado, dónde va la BIOS y cuánta
  memoria convencional queda;
- la pantalla (GOP), la memoria extendida, el vector del 8259 y los discos;
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

Casos: equipo «actual» (q35, 8 GB) con la pila comprobada; memoria baja
ocupada (BIOS en 70000h y en 60000h, en 64 y 32 bits) hasta `C:\>` y con el
registro completo; página 80000h ocupada; sin memoria baja libre y sin GOP
(aviso, qué hacer y registro guardado).

## Pendiente

- Probar la 0.2 en el PC actual (imagen de prueba) y ver su `HISPABIOS.LOG`.
  Lo siguiente que puede fallar ahí ya no es de la parte UEFI sino de la BIOS
  de modo real: el reloj (PIT y 8259 por LINT0; hay placas actuales con el
  8254 parado por ahorro de energía) y el teclado.
- La memoria extendida se pide a potencias de dos desde 1 MB (en OVMF, 4 MB):
  usar el mapa para pedir el tramo entero.
- El registro y el mapa de memoria en `efi32.asm`.
