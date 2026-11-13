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
                   C:\HISPADOS\HSPDPMI /SB    (juegos con DOS/4GW)

    HSPSND 0.1 - controlador de sonido de Hispa-DOS
    Intel HD Audio en FEBB0000, códec 0, salidas: 03
    Volumen: 80 %
    Sound Blaster 16 emulada en 220h, IRQ 5, DMA 1 y 5 (DSP 4.05)
    Variable puesta: BLASTER=A220 I5 D1 H5 T6

- Si hay una Sound Blaster de verdad, `/SB` no hace nada (lo dice).
- `HSPDPMI /SB` ([SB-PM]) solo se queda residente si HSPSND está emulando la
  tarjeta (32 KB; con `LH`, arriba si cabe). Hace falta para los juegos con
  DOS/4GW (Doom, Duke Nukem 3D, Mortal Kombat…): sin él van por VCPI y no
  suenan. La opción «Juegos» del disquete y de la memoria USB ya lo carga.
- Si el juego está configurado con la IRQ 7 (Doom y Duke Nukem 3D vienen
  así) y deja la 5 tapada, la IRQ se le entrega en la 7. Mejor configurarlo
  con la 5.
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

- **Programas en modo protegido** (DOS/4GW: Doom, Duke Nukem 3D, Mortal
  Kombat…): suenan si usan **DPMI**, es decir, con `HSPDPMI` cargado
  (DOS/4GW lo prefiere a VCPI si lo encuentra). Sin HSPDPMI van por VCPI,
  en anillo 0, y no hay forma de atraparles los puertos: en silencio. Ver
  «Programas en modo protegido (DPMI)». Los extensores que no usan DPMI
  aunque lo haya (o que traen su propio servidor) siguen sin sonido.
- **Windows 3.x en modo 386 mejorado** apaga el modo V86 de HSPEMM: sin
  monitor no hay puertos atrapados ni ventana para los registros del HD
  Audio. HSPSND lo ve venir (INT 2Fh AX=1605h), para la tarjeta emulada (una
  interrupción del HD Audio sin atender colgaría el equipo) y la vuelve a
  arrancar al salir de Windows (1606h). Dentro de Windows no hay emulación.
  Probado con `EMMMISC W`, que hace de Windows; con Windows de verdad, no.
- La IRQ se entrega cuando HSPSND tiene el control: en la interrupción del
  HD Audio o del AC'97 (cada 5 ms), en el reloj (18 veces por segundo) o al
  volver de un puerto atrapado. Si la BIOS no ha dado IRQ a la tarjeta (el
  HD Audio Realtek del ThinkPad A275: «IRQ sin asignar»), HSPSND enciende la
  interrupción periódica del reloj de tiempo real (IRQ 8) a 256 Hz y la usa
  en su lugar ([SB-EOI], ver «La IRQ»). Si la BIOS está esperando con el RTC
  (INT 15h AH=83h/86h) se le deja y a 1024 Hz, como espera.
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

Además ([SB-EOI], 13/11/2026, por Prince of Persia en el ThinkPad A275):

- **El final de cada bloque, cuando suena.** La voz lee del DMA hasta
  100 ms antes de que suene; si la IRQ se apuntara entonces, al leer de golpe
  saldrían varias seguidas y, si el manejador del programa abre las
  interrupciones, se anidarían (con DOS/4GW, «pila de modo real agotada»).
  Cada bloque deja apuntada en una cola (8) la trama del anillo en la que
  acaba, y su IRQ se entrega cuando esa trama ya ha sonado.
- **Sin IRQ de la tarjeta, el RTC.** Antes solo quedaba el reloj, y si el
  programa lo engancha y llama al de antes desde su manejador (con las
  interrupciones cerradas, como Prince of Persia), la IRQ no se podía
  entregar hasta que el programa tocara un puerto de la SB con las
  interrupciones abiertas: los sonidos acababan tarde o nunca. La
  interrupción del RTC llega con las interrupciones abiertas cada 3,9 ms.
- **El bit de ocupado del DSP parpadea.** En una Sound Blaster de verdad el
  bit 7 de 22Ch se enciende y se apaga aunque el DSP esté libre. Prince of
  Persia, para parar un sonido, espera a verlo encendido y luego apagado; con
  el bit siempre apagado se quedaba esperando a que el sonido acabara (el
  príncipe, agachado, no se levantaba). Ahora se enciende una de cada cuatro
  lecturas.

El EOI no era el problema: el que se da por el de la IRQ emulada solo se
traga si en el 8259 no hay en servicio ninguna IRQ de más prioridad (0-4,
también la cascada), y la prueba del teclado (KBSB) no pierde ninguna tecla
ni con el HD Audio sin IRQ.

### Programas en modo protegido (DPMI) [SB-PM]

Los juegos con DOS/4GW (y los demás extensores que usan DPMI) corren como
clientes de `HSPDPMI` en anillo 3. Si HSPSND emula la tarjeta (HSPEMM se lo
dice a HSPDPMI con INT 67h AX=DD04h al entrar cada cliente):

1. Los clientes corren con **IOPL 0** y el **mapa de E/S de la TSS** de
   HSPDPMI marca los puertos que HSPEMM atrapa para HSPSND (220h-22Fh,
   388h-38Bh, 20h) y los del 8237 de los canales cedidos (00h-0Fh, 81h-83h,
   87h, 89h-8Bh, C0h-DFh). Los de encima de 3FFh quedan fuera del mapa (se
   hacen desde el anillo 0, tal cual).
2. Cada IN/OUT a esos puertos da un fallo de protección; HSPDPMI descodifica
   la instrucción (con prefijos 66h, de 8, 16 y 32 bits) y la **hace en modo
   V86** desde un trozo suyo (`sbpm_rmio`), donde el monitor de HSPEMM la
   atrapa y la lleva a HSPSND igual que la de un programa de modo real (y
   las del 8237 a la estructura del canal cedido). La dirección del búfer
   que el juego da al 8237 es la de su memoria de DOS (DPMI 0100h), que en
   el espacio lineal del cliente y en el de V86 es la misma.
3. Las lecturas del **estado de la AdLib** (220h, 228h, 388h) las contesta
   HSPDPMI sin ir a V86, leyendo el octeto de estado de la FM de HSPSND
   (HSPSND le da su dirección a HSPEMM con AX=DD05h): los juegos leen el
   388h decenas de veces por cada escritura (en QEMU, 1000 escrituras de la
   FM pasaron de 3,6 s a 0,2 s).
4. La **IRQ** la entrega HSPSND como siempre, saltando al vector de modo
   real; si el cliente tiene manejador de modo protegido (DPMI 0205h),
   HSPDPMI tiene enganchado ese vector y lo lleva a él.
5. Con IOPL 0 también fallan **CLI y STI**: se emulan cambiando IF en el
   marco. Y como POPF e IRET no pueden cambiar IF con IOPL 0, tras emular un
   CLI se va **paso a paso** (TF) hasta que vuelvan a abrirse: antes de cada
   POPF o IRET se mira en la pila el IF que van a sacar y después se pone en
   el marco. Sin esto Duke Nukem 3D (bibliotecas de Apogee: `pushfd / cli /
   ... / popfd`) se quedaba sin interrupciones y colgado. Los trozos con las
   interrupciones cerradas son cortos; un bucle largo con CLI iría lento.

Y un fallo de HSPDPMI que salía con cualquier Sound Blaster (también con la
de verdad, con DOS/4GW y Doom): al llegar una IRQ con el cliente fuera de
la pila bloqueada, el manejador se ponía otra vez en la cima de esa pila y
pisaba el marco de vuelta de otro manejador aún en marcha (DOS/4GW cambia de
pila en sus manejadores): «pila de modo real agotada» o «error interno».
Ahora lo que está en uso queda reservado hasta que el manejador vuelve.

Y otro, al cargarlo con `LH` en un 386/486 (sin PSE): el directorio de
páginas de VCPI (8 KB, tras el programa) no se miraba si cabía en el bloque
de memoria superior y se ponía encima de la pila: se colgaba al instalarse.
Ahora, si no cabe, va en un bloque de 12 KB aparte, abajo (se suelta con
`HSPDPMI /D`).

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
- **Doom** (DOS/4GW): sin HSPDPMI, en silencio (VCPI). Con HSPDPMI, ver
  abajo.

`tests/run_sbpm_tests.sh` ([SB-PM]): `SBPM.COM` (`tests/progs/sbpm.asm`) es
SBTEST en modo protegido de 32 bits por DPMI, con el manejador de la IRQ de
modo protegido: con HD Audio, AC'97, `LH` y HD Audio sin IRQ (`SINIRQ.COM`
le quita la IRQ como la BIOS del A275), y de referencia con una `sb16` de
verdad (con y sin HSPEMM: IOPL 3). Además del sonido medido: IOPL 0, CLI,
STI, PUSHFD/CLI/POPFD y 1000 escrituras FM en menos de un segundo.

`tests/run_sbeoi_tests.sh` ([SB-EOI]): `KBSB.COM` (`tests/progs/kbsb.asm`)
hace como un juego (INT 9 propio, IRQ 5 en autoinicio con bloques cortos y
un manejador que abre las interrupciones, la FM sonando y el reloj
enganchado al estilo de Prince of Persia); por QMP se pulsan y sueltan 240
teclas (flechas con E0) y no se puede perder ninguna. Comprueba también que
el bit de ocupado parpadea y que la IRQ de un bloque único de 0,1 s llega a
su hora sin tocar puertos. Con HD Audio, HD Audio sin IRQ, AC'97, `LH`, una
`sb16` de verdad y sin sonido.

Juegos con DOS/4GW, con HSPEMM + HSPSND /SB + HSPDPMI, HD Audio sin `sb16`
(QEMU, 13/11/2026; segundos con sonido en ~33 s de grabación):

| Juego | Con IRQ del HD Audio | Sin IRQ (como el A275) | Notas |
|---|---|---|---|
| Doom | 8-16 s | 9-11 s | config. con SB 220/5/1 (`-config`) |
| Doom II | 8 s | — | igual; con su `DEFAULT.CFG` (IRQ 7) arranca pero en silencio |
| Duke Nukem 3D | 17-19 s | 18 s | con su `DUKE3D.CFG` (IRQ 7) también: 18 s; música FM |
| Mortal Kombat | 13 s | 13 s | su SETUP: «Sound Blaster (SB)» |
| Prince of Persia (modo real) | sí | sí | ya no se queda agachado |

Con todos los juegos de la colección en «Juegos» (HSPEMM + HSPSND /SB, HD
Audio), con HSPDPMI cargado y sin él, salen igual salvo los que necesitan
más memoria convencional (The Incredible Machine pide 550 KB: HSPDPMI ocupa
32 KB si no cabe arriba) y los que ya fallaban a ratos por la velocidad.

## Pendiente

- Probar en hardware real (HD Audio de portátiles: IRQ de PCI, LPIB).
- Extensores que no usan DPMI aunque lo haya (o con su propio servidor).
- OPL3 (estéreo, 18 canales, 4 operadores) y percusión con el ruido real.
- ADPCM (74h-77h, 16h/17h), DAC directo (10h), grabación.
- Elegir puerto, IRQ y DMA (`/SB:A240,I7…` o respetar una `BLASTER` ya
  puesta); MPU-401.
- Un filtro paso bajo al remuestrear (ahora, interpolación lineal).
- Anillo más pequeño cuando hay interrupciones de la tarjeta (ahorrar
  memoria convencional).
- Entregar la IRQ en cuanto el programa abra las interrupciones (con el RTC
  ya es como mucho 3,9 ms después; antes podía esperar
  a la siguiente entrada en HSPSND).
