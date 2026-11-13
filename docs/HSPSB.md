# Sound Blaster emulada: `HSPSND /SB`

Los portátiles y PC actuales no tienen Sound Blaster: tienen Intel HD Audio
(o AC'97 los de hace unos años). Los juegos de DOS no saben nada de esas
tarjetas: escriben en los puertos de la Sound Blaster (220h), programan el
controlador de DMA (8237) y esperan su IRQ. `HSPSND /SB` les da una **Sound
Blaster 16 emulada** (con AdLib) que suena por la tarjeta de verdad.

Escrito desde cero (sala limpia) con la documentación pública: *Sound Blaster
Series Hardware Programming Guide* (Creative), hojas de datos del 8237 y del
8259, del YM3812 (OPL2), y las especificaciones de Intel HD Audio y AC'97.

## Uso

Hace falta `HSPEMM.SYS` (los puertos solo se pueden atrapar en modo V86):

    CONFIG.SYS     DEVICE=C:\HISPADOS\HSPXMS.SYS
                   DEVICE=C:\HISPADOS\HSPEMM.SYS
    AUTOEXEC.BAT   C:\HISPADOS\HSPSND /SB

    HSPSND 0.1 - controlador de sonido de Hispa-DOS
    Intel HD Audio en FEBB0000, códec 0, salidas: 03
    Volumen: 80 %
    Sound Blaster 16 emulada en 220h, IRQ 5, DMA 1 y 5 (DSP 4.05)
    Variable puesta: BLASTER=A220 I5 D1 H5 T6

- Si hay una Sound Blaster de verdad, `/SB` no hace nada (lo dice).
- Sin HSPEMM lo dice y HSPSND queda cargado sin emular.
- La tarjeta emulada va siempre en 220h, DMA 1 y 5, IRQ 5 (IRQ 7 si la 5 es
  la del HD Audio o el AC'97). En el programa de configuración del juego:
  «Sound Blaster 16» (o «Sound Blaster» / «Sound Blaster Pro»), puerto 220,
  IRQ 5, DMA 1; música «AdLib» o «Sound Blaster» (FM).
- `SONAR`, el volumen (`SONIDO`, teclas) y los programas que usan la API de
  HSPSND siguen funcionando a la vez.
- Memoria: 55 KB residentes con HD Audio y 51 KB con AC'97 (el anillo de
  salida son 32 KB); con
  `LH HSPSND /SB` va a la memoria superior si cabe.
- De paso, con HSPEMM cargado `HSPSND` (sin `/SB`) ya funciona con HD Audio
  en modo V86 (antes solo las Sound Blaster).

## Qué emula

| Parte | Hecho |
|---|---|
| DSP 4.05 (22Ch/22Ah/22Eh/226h) | Reinicio, versión (E1h), E0h, E4h/E8h, altavoz (D1h/D3h/D8h), IRQ de prueba (F2h/F3h) |
| Reproducción de 8 bits por DMA | Bloque único (14h, 91h), autoinicio (1Ch, 90h, longitud con 48h), frecuencia por constante de tiempo (40h) o en Hz (41h/42h), pausa y seguir (D0h/D4h), salir del autoinicio (DAh), silencio (80h). Estéreo de la SB Pro (registro 0Eh del mezclador) |
| SB16 (Bxh/Cxh) | 8 y 16 bits, mono y estéreo, con y sin signo, bloque único y autoinicio, por el DMA de 8 bits (Cxh) o el de 16 (Bxh, canal 5); D5h/D6h/D9h |
| DMA (8237) | Canales 1 y 5 «cedidos»: dirección, cuenta, modo (autoinicio) y máscara; las lecturas de dirección y cuenta avanzan como en una tarjeta de verdad |
| IRQ | 5 (o 7): se entrega al vector del programa como si llegara del 8259; su EOI no llega al controlador de verdad; el ISR del 8259 (OCW3) la enseña «en servicio» |
| Mezclador (224h/225h) | Maestro, voz y FM (registros de la SB Pro 22h/04h/26h y de la SB16 30h-35h), reinicio, IRQ/DMA (80h/81h) y estado de IRQ (82h) |
| FM OPL2 / AdLib (388h/389h, 220h/221h, 228h/229h) | 9 canales de 2 operadores: 4 formas de onda, envolventes, KSL, KSR, realimentación, trémolo, vibrato y percusión simplificada; los temporizadores vencen al arrancar (detección) |
| Salida | Remuestreo con interpolación lineal a los 48 kHz (o 44,1) de la tarjeta, 16 bits estéreo |

No hace: grabación, ADPCM (se sustituye por silencio de la misma duración,
con su IRQ), MIDI/MPU-401, OPL3 (se presenta como OPL2), DAC directo (10h),
y el puerto de juegos.

## Límites

- **Programas en modo protegido**: los juegos con extensor de DOS (DOS/4GW:
  Doom, Duke Nukem 3D…) hacen la E/S de sonido desde modo protegido y, como
  clientes VCPI, corren en anillo 0: el monitor no puede atraparles los
  puertos. Con ellos no suena todavía. Siguiente paso: atrapar la E/S en
  `HSPDPMI` (sus clientes corren en anillo 3; hay que darles un mapa de
  puertos en su TSS) y que los extensores usen DPMI en vez de VCPI. Es lo
  que hacen SBEMU y VSBHDA con su servidor DPMI.
- **Windows 3.x en modo 386 mejorado** apaga el modo V86 de HSPEMM: sin
  monitor no hay puertos atrapados ni ventana para los registros del HD
  Audio. HSPSND lo ve venir (INT 2Fh AX=1605h), para la tarjeta emulada (una
  interrupción del HD Audio sin atender colgaría el equipo) y la vuelve a
  arrancar al salir de Windows (1606h). Dentro de Windows no hay emulación.
  Probado con `EMMMISC W`, que hace de Windows; con Windows de verdad, no.
- La IRQ se entrega cuando HSPSND tiene el control: en la interrupción del
  HD Audio o del AC'97 (cada 5 ms), en el reloj (18 veces por segundo) o al
  volver de un puerto atrapado. Si el BIOS no ha dado IRQ a la tarjeta (o
  no llega), solo queda el reloj: suena con más retraso (unos 100 ms) y los
  bloques de menos de 55 ms llegan a trompicones.
- El sonido sale con un adelanto fijo de unos 30 ms (lo que tarda en sonar
  lo escrito en el anillo).
- Probado solo en QEMU (intel-hda + hda-duplex y AC97). **Falta probarlo en
  hardware real.**

## Diseño

Tres piezas:

1. **`drivers/hspemm.asm`** (lo marcado con `[SB]`): el monitor solo atrapa.
   Servicios en INT 67h, AH=DDh:
   - `AL=00h` ¿está? → AH=0, BX=5342h («SB»), CX=versión.
   - `AL=01h` vigilar puertos: DS:SI = pares (primero, último) acabados en
     FFFFh (8 como mucho); ES:BX = rutina. Cada `IN`/`OUT` de un programa a
     esos puertos se convierte en una llamada a la rutina, como una
     interrupción: en la pila del programa quedan `[SP]`=datos (bit 0: OUT;
     bit 1: 16 bits; bit 2: 32 bits), `[SP+2]`=puerto, y después IP, CS e
     indicadores. La rutina quita las dos palabras y acaba con `IRET`; el
     valor va en AL/AX/EAX. La E/S hecha desde el segmento de la rutina no
     se desvía (así HSPSND llega al 8259 de verdad).
   - `AL=02h` mapear memoria física en páginas del primer mega: BX = página
     lineal, CX = cuántas, EDX = dirección física (con el bit 0 a uno es RAM
     normal: así se deshace, con la dirección que daba VCPI DE06h).
     Con esto HSPSND ve los registros del HD Audio (que están por encima de
     1 MB) en una página de su propia memoria.
   - `AL=03h` ceder un canal de DMA: BL = canal, ES:DI = estructura de 16
     octetos. El monitor anota en ella lo que el programa escribe en el 8237
     (dirección lineal, cuenta, modo, máscara) y contesta con ella las
     lecturas de dirección y cuenta «actuales», que lleva HSPSND.
2. **`lib/hspsb.inc`** (dentro de `HSPSND.COM`, solo queda residente con
   `/SB`): el DSP, el mezclador, el 8259 virtual de la IRQ emulada, la voz
   de DMA, la voz de la API de HSPSND y el anillo de salida.
3. **`lib/hspopl.inc`** (+ `hspopl_st.inc`, `hspopl_tab.inc` generado por
   `tools/mkopl.py`): la síntesis FM.

### El anillo de salida

HD Audio y AC'97 tocan sin parar un anillo de 8192 tramas (16 bits estéreo,
32 KB) repartido en 32 entradas de 1 KB (así ninguna cruza una página: en la
memoria superior las páginas no van seguidas en la memoria física; la
dirección física de cada una se pide por VCPI DE06h). Cada entrada avisa al
acabar (interrupción cada 256 tramas, 5,3 ms).

`e_pump` (en la interrupción de la tarjeta, en INT 08h y al arrancar una
reproducción) mira por dónde va la tarjeta y llena el anillo hasta `e_lead`
tramas por delante (1536 con interrupciones de la tarjeta; 4608 si solo hay
reloj). Lo nuevo se pone a cero y se le suma la FM; cada voz (la del DSP y
la de la API) lleva su propio cursor y suma sus muestras desde él: así un
bloque único que empieza justo después de acabar otro sigue sin hueco donde
se quedó el anterior, aunque esa zona del anillo ya esté «escrita».

La voz del DSP lee del búfer del programa (dirección del 8237, que con
HSPEMM es lineal: vale también si el búfer está en un UMB), avanza dirección
y cuenta en la estructura cedida, recarga al acabar la cuenta si el canal
está en autoinicio (o lo tapa), y al acabar el bloque del DSP apunta una
IRQ. Las IRQ pendientes se cuentan: si en un llenado acaban dos bloques, se
entregan una tras otra.

### La IRQ

Al salir de cualquier rutina de HSPSND (puerto atrapado, reloj, tarjeta), si
hay una IRQ pendiente, no hay otra entregada sin EOI, el programa
interrumpido tenía las interrupciones abiertas y la IRQ no está tapada en el
8259 (puerto 21h), en vez de `IRET` se salta al vector del programa (08h +
IRQ) con el marco de la interrupción en curso todavía en la pila: para el
programa es una interrupción recién llegada. El puerto 20h está vigilado: el
primer EOI (20h, o 60h+IRQ) que no corresponda a una interrupción de verdad
de más prioridad se da por el de la IRQ emulada y no llega al 8259.

### Rendimiento: el estado, lejos del código

El estado que cambia a cada muestra (voces, DSP, FM, estructuras de DMA) y
la pila de las rutinas están en un bloque aparte (segmento `e_seg`, en GS),
en páginas enteras sin código: una página para el estado y la lista de
descriptores y ocho para el anillo, todas puestas a cero al arrancar.
Escribir a cada muestra en una página que tiene código (o que lo tuvo:
QEMU guarda su traducción hasta que se pisa) lo frena todo cientos de veces
en los emuladores, y en los procesadores actuales vacía la cola de
ejecución («código que se modifica»). Con el estado junto al código, en
QEMU el sonido salía a trompicones y el equipo se quedaba sin interrupciones.

## Pruebas

`tests/run_sbemu_tests.sh` (en `make test`): QEMU **sin** `sb16`, con
`intel-hda` + `hda-duplex`, con `AC97`, y con `LH HSPSND /SB`. `SBTEST.COM`
(`tests/progs/sbtest.asm`) hace lo que un juego y cuenta las IRQ; QEMU graba
lo que sale por la tarjeta y `tests/tools/audio.py` mide los tonos:

| Prueba | Suena (medido en la grabación) |
|---|---|
| Bloque único 14h, 8 bits mono, 10989 Hz | 498-500 Hz, 0,55-0,60 s; IRQ a los 11 tics |
| Autoinicio 1Ch, 8 bits mono, 22222 Hz, 9 bloques | 804-808 Hz, 0,75-0,80 s; 9 IRQ; la cuenta del DMA avanza |
| SB16 B6h, 16 bits estéreo, 44100 Hz, 25 bloques | 1198-1206 Hz, 0,50-0,55 s; 25 IRQ |
| AdLib: detección por temporizadores y una nota | 599-600 Hz, 0,65-0,70 s |
| `SONAR /TONE 440 500` con `/SB` cargado | 436-440 Hz, 0,45-0,50 s |

Y además: HD Audio en modo V86 sin `/SB` (440 Hz, 0,75 s), `/SB` sin HSPEMM
(lo dice y carga sin emular), `/SB` con una `sb16` de verdad (no emula, y
SBTEST pasa igual sobre la tarjeta de QEMU) y «Windows» apagando y
encendiendo el modo V86 (`EMMMISC W`) con `/SB` cargado: después todo suena
igual. En total 96 comprobaciones.

`SBTEST` vale también con una Sound Blaster de verdad (lee `BLASTER`).

Con un juego: `tests/juegos/lab.sh --emm --sb --tarjeta hda --wav /tmp/j.wav
JUEGO` (el laboratorio de juegos, con el ISO privado). Probados así en QEMU,
con HD Audio y sin `sb16` ni `adlib` (13/11/2026):

- **Wolfenstein 3D**: detecta la tarjeta (engancha la IRQ 5), suena la
  música (FM) en la portada, los menús y la partida, y los disparos y las
  puertas (voz digitalizada por DMA). Comparado segundo a segundo con la
  misma sesión sobre la `sb16` + `adlib` de QEMU, el nivel y el número de
  cruces por cero de la grabación son parecidos (música de portada: 2500-3500
  cruces por segundo frente a 1500-3000; en la partida, 9000-13000 frente a
  12000-15000).
- **Prince of Persia**: suena la música de la presentación.
- **Doom** (DOS/4GW): arranca y se juega, pero en silencio (ver «Límites»).

## Pendiente

- Probar en hardware real (HD Audio de portátiles: IRQ de PCI, LPIB).
- Programas en modo protegido: E/S atrapada en HSPDPMI (ver «Límites»).
- OPL3 (estéreo, 18 canales, 4 operadores) y percusión con el ruido real.
- ADPCM (74h-77h, 16h/17h), DAC directo (10h), grabación.
- Elegir puerto, IRQ y DMA (`/SB:A240,I7…` o respetar una `BLASTER` ya
  puesta); MPU-401.
- Un filtro paso bajo al remuestrear (ahora, interpolación lineal).
- Anillo más pequeño cuando hay interrupciones de la tarjeta (ahorrar
  memoria convencional).
- Entregar la IRQ en cuanto el programa abra las interrupciones (hoy espera
  a la siguiente entrada en HSPSND).
