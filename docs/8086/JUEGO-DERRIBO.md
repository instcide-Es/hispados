# DERRIBO: un juego de pala y ladrillos para Hispa-8086

*Noviembre de 2026 · Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.*

DERRIBO es un juego **nuestro** del género de «romper muros» (pala, pelota y
ladrillos), hecho para Hispa-8086 en ensamblador de 8086. El nombre, los diez
muros, el aspecto, los premios y los sonidos son propios; no copia ningún
juego concreto del género. Fuente: `h8086/juegos/derribo.asm`; va en el
disquete de juegos (`h8086/build/juegos8086-360.img`) con unas instrucciones
(`JUEGOS.TXT`).

## Cómo se hacían en el IBM PC (1981-1986)

**El ordenador.** El IBM PC 5150 (1981) y el XT (1983) llevaban un 8088 a
4,77 MHz: unas 0,3 instrucciones por microsegundo, y con un bus de 8 bits que
hace que leer o escribir una palabra cueste el doble. Los juegos del género
salían en disquete de 160 o 360 K y pedían 64 o 128 K.

**La pantalla.** Había dos tarjetas de IBM:

- La **MDA** (monocromo), solo texto de 80×25, con caracteres muy nítidos
  (9×14) en un monitor de fósforo verde (el IBM 5151) y después también
  ámbar. Sin gráficos. La **Hercules** (1982) añadió a la MDA unos gráficos
  de 720×348 a dos colores, y fue la tarjeta de muchos PC de oficina.
- La **CGA**, con texto de 40×25 y 80×25 y dos modos gráficos: **320×200 a
  4 colores** (el fondo y una de dos paletas fijas: verde, rojo y marrón, o
  azul claro, morado y blanco, cada una normal o brillante) y 640×200 a dos.
  La memoria va entrelazada: las líneas pares en B800:0000 y las impares en
  B800:2000, 80 octetos por línea, cuatro puntos por octeto. En 80 columnas
  de texto, escribir mientras la tarjeta pinta deja «nieve» en la pantalla;
  en los modos gráficos, no.

Muchísimos jugaban **sin color**: con la MDA o la Hercules, o con la CGA
conectada por la salida compuesta a un monitor monocromo (o a la tele en
blanco y negro). Por la salida compuesta los colores se ven como grises, y
en 640×200 los puntos sueltos dan los «colores de artefacto» de la señal
NTSC; el modo 5 de la BIOS (320×200 «en blanco y negro») quita la señal de
color de la salida compuesta para que no los haya. Los juegos que se veían
bien en monocromo eran los que no fiaban nada al tono.

**El sonido.** Solo el altavoz del PC: el canal 2 del temporizador 8253 hace
una onda cuadrada (1.193.182 Hz entre el divisor que se le ponga) y el bit 1
del puerto 61h la deja llegar al altavoz. Un pitido cada vez, sin volumen.

**El tiempo.** El 8253 (8254 en el AT) da, por el canal 0, la interrupción
del reloj 18,2 veces por segundo. Muchos juegos de los primeros años medían
el tiempo con **bucles de espera** calibrados para el 8088 a 4,77 MHz: en un
AT o en cualquier «turbo» iban demasiado deprisa, y por eso los PC de los 80
traían el botón *turbo* para volver a 4,77. Los mejores reprogramaban el
canal 0 para tener más tics por segundo (y seguían llamando al reloj de la
BIOS 18,2 veces para que la hora no se adelantara).

**El teclado.** La BIOS solo da la última tecla, con su repetición: para
saber qué teclas están *apretadas* (las dos flechas a la vez, o una flecha
mientras se pulsa Espacio) los juegos cambiaban la INT 9 por la suya, leían
el código del puerto 60h (al pulsar y al soltar) y en el PC y el XT
contestaban al teclado con el bit 7 del puerto 61h.

**La palanca y el ratón.** La tarjeta de juegos (puerto 201h) mide cada eje
con un condensador: se escribe en el puerto y se cuenta cuánto tarda en
bajar el bit; lo contado depende de la velocidad de la máquina, así que hay
que calibrar con la palanca en el centro. El ratón de Microsoft (1983) venía
con su controlador (MOUSE.COM), que se usa por la INT 33h.

**La física.** Lo habitual era mover la pelota a saltos de varios puntos y
mirar solo dónde caía: a mucha velocidad atravesaba ladrillos o la pala; el
rebote en la pala tenía pocos ángulos, y se podía quedar rebotando para
siempre de lado a lado o entre dos ladrillos que no se rompen.

## Qué hace DERRIBO

| | En la época | DERRIBO |
|---|---|---|
| Pantalla | CGA 320×200 o texto | CGA 320×200 a 4 colores (también EGA y VGA, que la imitan); con MDA o Hercules se ve solo y juega en texto de 80×25 con caracteres gráficos; `/T` lo pide aunque haya CGA |
| Sin color | lo que saliera | **monocromo** (`/M`, o M en la portada): todo en un solo color y el modo 5 de la CGA; y aun en color, nada depende del tono (ver abajo) |
| Velocidad | bucles: más deprisa en un AT | el 8254 a **72,8 tics por segundo** (divisor 16384, cuatro veces lo normal, y la BIOS sigue recibiendo sus 18,2); el juego da **un paso por tic**, y si una máquina lenta no llega a pintar todos, da los que faltan antes de pintar: la misma velocidad en un 8088 a 4,77 MHz, en un 286 o en QEMU |
| Colisiones | saltos de varios puntos | la pelota avanza **como mucho un punto** cada vez y se mira **cada eje por separado**: si en horizontal se mete en algo, se deshace ese paso y rebota en horizontal; luego igual en vertical. No puede atravesar nada a ninguna velocidad |
| Pala | dos o tres ángulos | **16 zonas**: según dónde da la pelota sale de casi vertical (72°) a tendida (25°), hacia el lado de la pala en que ha dado. Nunca horizontal |
| Bucles eternos | podían pasar | si una pelota rebota 24 veces sin romper nada (paredes, ladrillos que no se rompen o la propia pala), se le tuerce el ángulo |
| Teclado | la BIOS | INT 9 propia: flechas o Z/X apretadas, y Espacio, P, S, M y Esc; se deja todo como estaba al salir |
| Ratón y palanca | según el juego | ratón por la INT 33h si hay controlador (lo mueve y su botón lanza); palanca por el puerto 201h, calibrada al empezar (izquierda y derecha y el botón) |
| Sonido | altavoz | altavoz, con melodías cortas (rebote, golpe, ladrillo roto, metal, premio, pelota perdida, muro derribado) que suenan solas desde la interrupción del reloj, sin parar el juego; S lo quita y lo pone |
| Récord | a veces | la mejor puntuación en `DERRIBO.REC` (texto); con el disquete protegido no se guarda y el juego sigue |

### Ladrillos y premios, sin fiarse del color

Carlos es daltónico, y en la época muchos jugaban en monocromo: cada cosa se
distingue por la **forma**, y el color solo acompaña.

| Ladrillo | Forma (CGA) | En texto |
|---|---|---|
| un golpe | lleno | `█████` |
| dos golpes | hueco, con borde y un **2** dentro | `[ 2 ]` |
| tres golpes | lleno, con un **3** calado | `▓▓3▓▓` |
| no se rompe | a cuadros | `▒▒▒▒▒` |

Al golpear uno de tres golpes pasa a dibujarse como el de dos, y el de dos
como el de uno: el ladrillo dice siempre lo que le queda. Los premios son
cápsulas con su **letra**: **A** pala ancha, **L** pelota lenta, **T** tres
pelotas, **V** una vida más (en texto, `(A)`). Uno de cada cuatro ladrillos
rotos suelta uno, si no hay otro cayendo.

En color se usa la paleta de azul claro, morado y blanco, brillante: además
de distintas de forma, sus brillos son bien distintos (unos 210, 135 y 255
sobre 255 si se pasan a gris), así que también se distinguen en un monitor
monocromo por la salida compuesta. Con `/M`, todo es de un solo color.

### Los muros

Diez, propios: el andamio, el tejado, ventanas, la reja, la cometa, las
columnas, el cofre (un tesoro rodeado de ladrillos que no se rompen y
abierto por abajo), la escalera, almenas y la fortaleza. Al acabar el
décimo se vuelve al primero, un poco más deprisa. La pelota acelera un poco
cada ocho golpes, y cada muro empieza algo más rápido que el anterior.

Se guardan de forma compacta: cada muro es simétrico, así que solo se
guardan las siete columnas de la izquierda (la séptima es la del centro), y
cada casilla va con un código de longitud variable (`0` nada, `10` un golpe,
`110` dos, `1110` tres, `1111` no se rompe). En el fuente se escriben como
un dibujo (`"..#333"`) y una macro de NASM los convierte en bits: los diez
caben en **137 octetos**.

### Puntos

10 por golpe a un ladrillo que se rompe, 50 por premio cogido, y 100 por el
número del muro al derribarlo. Tres vidas (hasta nueve con los premios V).

## Tamaño

`DERRIBO.COM` ocupa **6.116 octetos** (todo: los dos modos de pantalla, el
monocromo, ratón, palanca, sonido, los diez muros y la prueba del motor).
Las imágenes de los ladrillos y los premios no van en el archivo: se dibujan
al arrancar a partir de cuatro reglas y seis letras de 3×5 puntos (12
octetos). Necesita unos 7 K de memoria en total.

En la CGA original, la BIOS no tiene en los modos gráficos las letras de la
128 en adelante (las acentuadas): DERRIBO copia de la ROM las que usa (á, é,
í, ó, ú, ñ, ¡), les pone el acento y se las da a la BIOS por la INT 1Fh
mientras juega. En EGA y VGA no hace falta.

## Pruebas

En la batería de Hispa-8086 (`make -C h8086 prueba`, o solo esta parte:
`SOLO=derribo ./prueba/prueba.sh`), en `h8086/prueba/partes_juegos.sh`:

- **El motor, sin pantalla** (`DERRIBO /P`): juega solo los diez muros dos
  veces (a casi 5 puntos por tic, más deprisa que nada del juego, y a la
  velocidad de empezar), con tres pelotas y la pala de pared a pared, y
  mira **en cada paso** que ninguna pelota esté dentro de un ladrillo o una
  pared, que no pase la pala y que no salte más de un punto. Tienen que
  caer los veinte muros (no hay bucles eternos) y 0 fallos; y los ladrillos
  que cuenta tienen que ser los que la prueba cuenta en el dibujo del
  fuente. (Comprobado al revés: con pasos de cuatro puntos, la prueba da
  decenas de miles de fallos.)
- **En QEMU, en gráficos**: modo 4, los tres colores, los cuatro ladrillos
  de la portada distintos por la forma, la pelota pegada a la pala en medio,
  que sale con Espacio y se mueve, Esc y vuelta al sistema.
- **El récord**: `DERRIBO.REC` con la puntuación, y que la portada siguiente
  la lee.
- **Monocromo** (`/M`): modo 5, un solo color en la portada y en el juego, y
  los ladrillos aún distintos por la forma.
- **En texto** (`/T`, lo mismo que con MDA o Hercules): portada, leyenda,
  fila de puntos, el muro, la pala, la pausa y el fin de la partida.
- Solo instrucciones del 8086 (`cpu 8086`), tamaño por debajo de 8 K, y el
  disquete de juegos sin errores (`fsck.fat`).

Falta probarlo en un PC de verdad: un 8088 con CGA (y la salida compuesta a
un monitor monocromo), una MDA o Hercules (QEMU no tiene MDA: el modo de
texto se prueba en la VGA), el ratón y la palanca.
