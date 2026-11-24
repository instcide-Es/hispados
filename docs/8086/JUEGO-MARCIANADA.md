# Marcianada: un juego de marcianitos para Hispa-8086

Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.

`MARCIANA.COM`, en el disquete de juegos de Hispa-8086
(`h8086/build/juegos8086-360.img`). Fuente: `h8086/juegos/marciana.asm`.
Las instrucciones para jugar están en el propio disquete, en `JUEGOS.TXT`
(`h8086/juegos/JUEGOS.TXT`).

Es un juego **nuestro** del género de los «marcianitos» (la flota que baja
en formación, el cañón abajo y las defensas en medio), no la copia de
ninguno: ni el nombre, ni los bichos, ni los dibujos, ni los sonidos salen
de un juego concreto.

## El juego

Una flota de 40 bichos (8 columnas y 5 filas) baja de lado a lado, y cada
vez que toca un borde baja 4 puntos. Abajo, tu **churrera**, que tira
**churros**. Entre la flota y tú, cuatro **murallas** almenadas que se van
comiendo los disparos (los tuyos y los suyos) y la propia flota cuando llega
a ellas.

| Quién | Dónde | Puntos | Cómo se reconoce (sin mirar el color) |
|---|---|---|---|
| **Boina** | la fila de arriba | 30 | una boina con rabillo, ojos y patas |
| **Botijo** | las dos de en medio | 20 | redondo, con asa arriba y pitorro a un lado |
| **Abanico** | las dos de abajo | 10 | varillas abiertas en abanico; aletea |
| **Casco** | las Boinas desde la oleada 2 (y más filas después) | +5 por quitarlo | una cúpula con visera: hacen falta dos tiros |
| **Abanico suelto** | desde la oleada 3 | 50 | uno de abajo deja la flota y baja dando bandazos; se come las murallas |
| **Bombona** | cruza por arriba | 50, 100, 150 o 300 | una bombona de butano tumbada; suelta un premio |
| **Corazón** (premio) | cae de la Bombona | una vida (hasta 6) | un corazón macizo de 8 × 8 |
| **Churros** (premio) | cae de la Bombona | 15 s de tiro doble | dos palos con estrías |

Ellos tiran **aceitunas** (redondas, de 3 × 4, caen a 60 puntos por segundo)
y **guindillas** (en zigzag, de 3 × 6, al doble de velocidad; desde la
oleada 2, y cada vez más). Tus churros son una raya recta de un punto de
ancho. Cada oleada empieza 8 puntos más abajo (hasta la quinta), la flota va
más deprisa cuanto menos quedan y cuanto más alta es la oleada, y tira más a
menudo y más balas a la vez (de 3 a 6). Si la flota llega al suelo, se acabó
la partida. Al empezar cada oleada suena una escala andaluza (la frigia
dominante, en mi); la flota marca el paso con un chasquido de castañuela.

Teclas: flechas o Z y X; Espacio, flecha arriba o Control para tirar; P,
pausa; S, sonido; Esc, acabar (y en la portada, salir). La palanca del PC
(puerto 201h) vale si la hay: se calibra sola al arrancar (el centro es
donde esté al empezar), y cualquiera de sus dos botones dispara.

## Cómo se hacían estos juegos en el IBM PC (1981-1986)

**El vídeo.** La CGA (1981) tenía 16 K de memoria en B800h y, para juegos,
el modo 4: 320 × 200 a 4 colores, dos bits por punto, cuatro puntos por
octeto. Las filas pares van en B800:0000 y las impares en B800:2000 (80
octetos por fila). Los colores no se elegían: fondo libre y una de dos
paletas fijas, verde/rojo/marrón o azul claro/morado/blanco, cada una con o
sin intensidad. No había sprites ni desplazamiento por hardware: todo se
pintaba con el procesador, octeto a octeto. Lo barato era mover las cosas de
cuatro en cuatro puntos (un octeto entero, sin correr bits) y borrar el
rastro con un borde en blanco alrededor del dibujo, en vez de borrar y
volver a pintar (que parpadea). Mover un punto suelto obliga a correr los
bits, y muchos juegos guardaban el dibujo ya corrido de las cuatro maneras
posibles. En el modo de texto de 80 columnas la CGA de IBM hacía «nieve» si
se escribía en la memoria mientras pintaba; en los modos gráficos, no.
Esperar al retrazado vertical (puerto 3DAh, bit 3) para pintar evitaba ver
los dibujos a medias.

**Sin color.** Muchos PC de entonces no tenían monitor de color: la tarjeta
de las oficinas era la MDA (solo texto, 80 × 25, fósforo verde del monitor
5151) y la CGA iba a menudo a un monitor compuesto monocromo (verde o
ámbar) o a la televisión. Con la MDA los juegos o no funcionaban o eran de
letras; la Hercules (1982) añadió gráficos de 720 × 348 a un color, pero
pocos juegos la usaban. En un monitor compuesto en blanco y negro, el modo 5
de la CGA (el 4 sin la señal de color) da tres grises distintos. Los juegos
que se veían bien en esos monitores eran los que distinguían las cosas por
la forma y por el brillo, no por el tono.

**El tiempo.** El 8088 iba a 4,77 MHz (unas 0,3 o 0,4 millones de
instrucciones por segundo). Muchos juegos medían el tiempo con bucles de
espera, y en cuanto salieron los PC más rápidos (los «turbo» de 8 MHz, el AT
de 6 y 8 MHz) iban tan deprisa que no se podía jugar: de ahí el botón
«turbo» para ir más despacio. Lo bien hecho era usar el temporizador 8253
(el 8254 en el AT): su canal 0 interrumpe 18,2 veces por segundo (INT 8);
se podía reprogramar para que interrumpiera más a menudo, pero entonces el
reloj de la BIOS se adelantaba si no se tenía cuidado.

**El sonido.** El altavoz del PC solo hace una onda cuadrada: el canal 2
del temporizador da la frecuencia (puerto 42h, con el modo puesto en 43h) y
los bits 0 y 1 del puerto 61h lo conectan. Una nota cada vez; los efectos
eran barridos de frecuencia y «ruido» cambiando la frecuencia al azar en
cada interrupción.

**El teclado.** La BIOS (INT 16h) da una tecla tras otra, con el retardo
de repetición: no sirve para moverse y tirar a la vez. Los juegos ponían su
propia INT 9, leían el código de la tecla del puerto 60h (al pulsar y al
soltar) y llevaban una tabla de qué teclas estaban pulsadas. En el PC y el
XT hay que «dar el recibo» de la tecla con un pulso en el bit 7 del puerto
61h.

**La palanca.** La tarjeta de juegos (puerto 201h) no da la posición: se
escribe en el puerto, un monoestable por eje cambia de estado al cabo de un
tiempo que depende de la resistencia del potenciómetro, y el programa cuenta
vueltas hasta que cambia. La cuenta depende de la velocidad del procesador:
hay que calibrarla.

## Qué hace Marcianada, y qué mejoramos hoy

- **Igual de deprisa en cualquier máquina.** El canal 0 del 8254 se pone a
  60 Hz (divisor 19 886) y cada interrupción es un tic de juego. Todo lo que
  se mueve lo hace en tics, no en vueltas de bucle. El programa espera el
  tic con `HLT`, y si una máquina lenta se retrasa, hace los tics que debe
  de una vez (hasta seis seguidos; si se retrasara más, se pierden en vez de
  ir a trompicones). El reloj de la BIOS **no se adelanta**: cada tic se suma
  el divisor a una cuenta, y cuando pasa de 65 536 se llama a la INT 8 de la
  BIOS (que da su hora y apaga el motor de la disquetera); los demás tics
  terminan sin ella. Al salir se deja el canal 0 como estaba.
- **Sin parpadeo, dentro de lo que da la CGA.** Se pinta solo lo que cambia
  y en el sitio: la flota, la churrera y la Bombona se mueven como mucho 4
  puntos de lado por tic y se pintan con un octeto en blanco a cada lado,
  que borra su rastro sin borrar antes; lo que se mueve hacia abajo (las
  balas, los premios, el Abanico suelto) borra solo sus propios puntos y se
  vuelve a pintar. La flota (por la que pasa casi todo el trabajo) va de
  octeto en octeto, sin correr bits. Antes de pintar se espera el retrazado
  vertical. El marcador se escribe solo cuando cambia.
- **Varias teclas a la vez** con una INT 9 propia y su tabla de teclas
  (incluido el pulso del puerto 61h del XT), y la de la BIOS se devuelve al
  salir y mientras se escribe el récord (si hace falta cambiar de disquete,
  el núcleo lo pide y espera una tecla con INT 16h). Ctrl+C no corta el
  juego.
- **Sonido** desde la misma interrupción del reloj: efectos (tono que sube o
  baja, o ruido de un registro de desplazamiento) y melodías de notas; S lo
  quita y lo pone.
- **Daltonismo y monitores monocromos.** Nada se distingue solo por el
  color: cada bicho tiene su silueta (y dos dibujos, para que ande), el
  casco cambia la forma del bicho, tus disparos son rayas rectas y los suyos
  bolas o zigzags, los premios son un corazón y unos churros. Con
  **`/M`** el juego se pone en el modo 5 (en un monitor compuesto, sin
  color) y todos los dibujos se pasan al color más claro al arrancar; las
  murallas van rayadas para no confundirse con nada. Con una **MDA o una
  Hercules** (se ve en INT 11h) el juego va en **modo texto**, con letras
  gráficas de la página 437 en una rejilla de 80 × 25 que es la misma que la
  de los gráficos (cada letra, 4 × 8 puntos); **`/T`** lo pide también en una
  tarjeta de color. En modo texto las murallas se ven más o menos llenas
  (█ ▓ ▒ ░) según lo que les quede, los premios parpadean y la Bombona va en
  vídeo inverso.
- **El récord, en el disco**, al lado del programa (`MARCIANA.REC`, dos
  octetos): la ruta sale del entorno del programa, como en DOS 3. Si el
  disquete está protegido, no se guarda y no pasa nada.
- **Pausa** (P) que para todo, y **salir** (Esc) dejando el vídeo, el reloj,
  el teclado y el altavoz como estaban.

### Lo que no tiene

- Los gráficos de la Hercules (720 × 348): con una Hercules va en modo
  texto. Hacerlo de verdad pide otra forma de pintar (un bit por punto,
  cuatro bancos de filas) y no compensa en bytes.
- El modo 6 (640 × 200 a un color): el modo 5 con todo en el color claro da
  lo mismo en un monitor monocromo, sin otra manera de pintar.
- En una CGA de IBM, el modo texto pedido con `/T` puede hacer «nieve»
  (el modo texto está pensado para la MDA y la Hercules, que no la hacen).
- La palanca no se ha podido probar en la batería (QEMU no tiene puerto de
  juegos); se ha comprobado que, sin palanca, el juego la da por ausente.

## Cómo está hecho

Un solo `.COM` de **6 207 octetos** (las variables, 560 octetos, y la pila,
512, no van en el archivo: en memoria, menos de 8 K). Todo en ensamblador de 8086 con `cpu 8086`.

- **Dibujos**: filas de texto en el fuente (`F '..3311..'`: un carácter por
  punto, 0 a 3 el color), que una macro de NASM pasa a dos bits por punto.
  Una tabla de 18 tipos da para cada dibujo sus puntos, su ancho y alto, y
  sus letras y atributo en modo texto. Una sola rutina, `pinta`, los pone
  en la CGA (copiando con borde, sumando o borrando solo sus puntos, y
  corriendo los bits si la x no cae en un octeto) o en modo texto.
- **Murallas**: un mapa de un bit por punto (24 × 12 cada una); los choques
  se miran punto a punto en el mapa, no en la pantalla, y cada golpe quita
  un bocado de 5 × 4 con los bordes al azar.
- **Choques** con la flota: por cuentas (columna = x / 12, fila = y / 16),
  no mirando la pantalla. El churro avanza 4 puntos por tic mirando cada
  punto, así que no atraviesa una muralla delgada.
- **El motor se prueba sin pantalla**: con `-DPRUEBA` el mismo fuente da
  `MARPRUEB.COM`, que pinta en un búfer de memoria en vez de en la CGA y
  comprueba ocho cosas (ver «Pruebas»).

## Pruebas

`SOLO=marcianos ./prueba/prueba.sh` en `h8086/` (es la parte `marcianos`
de la batería de Hispa-8086; `prueba/partes_marcianos.sh`). Arranca el
disquete de 360 K con el de juegos en B: y comprueba (38 puntos):

- que el fuente lleva `cpu 8086` y nada más, y que el `.COM` cabe en 8 K;
- **el motor** (`MARPRUEB.COM`): un churro mata al Abanico de encima y da
  10 puntos; por el hueco entre dos columnas pasa sin tocar a nadie; un
  churro desde abajo se para en la muralla y le quita el punto donde da (y
  no los de arriba); una aceituna desde arriba, igual; la Boina con casco
  necesita dos tiros (5 + 30 puntos); una guindilla en la churrera quita una
  vida; la flota que llega al suelo acaba la partida; y el récord se escribe
  en el disco y se vuelve a leer;
- **el juego en la CGA**: la portada en el modo 4, con tres colores; la
  flota, las murallas y la churrera pintadas donde deben (mirando la
  pantalla que saca QEMU); el reloj de la BIOS sigue a 18,2 tics por segundo
  con el 8254 a 60 Hz; la churrera se mueve; P lo para todo y otra P sigue;
  al salir, el modo de texto y el indicador del sistema;
- **en modo texto** (`/T`, lo que se ve con una MDA o una Hercules): la
  portada, la flota con sus letras, la churrera, las murallas, y S quita el
  sonido;
- **en monocromo** (`/M`): el modo 5 y todo de un solo color;
- el récord, en `B:MARCIANA.REC`, y la portada lo enseña al volver a entrar;
  y `fsck.fat` da por bueno el disquete de juegos.

Además, a mano, con QEMU contando instrucciones (`-icount shift=10`, una
máquina de un millón de instrucciones por segundo) y sin contarlas, la flota
llega a los bordes en los mismos tics del reloj de la BIOS (en 330 tics,
2 de diferencia, lo que da el muestreo): la velocidad no depende del
procesador. En un 8088 a 4,77 MHz lo más pesado es mover la flota entera
(unos 25 ms la primera vez, cuando están los 40; cada vez menos), que se
hace una vez cada 13 tics al principio.

Así se ve en modo texto (con `/T`; es la rejilla de los gráficos, letra por
letra):

```
                    PUNTOS 00000  RECORD 00030  OLEADA 01


                                  ══ ══ ══ ══ ══ ══ ══ ══

                                  ô) ô) ô) ô) ô) ô) ô) ô)

                                  ô) ô) ô) ô) ô) ô) ô) ô)

                                  \/ \/ \/ \/ \/ \/ \/ \/

                                  \/ \/ \/ \/ \/ \/ \/ \/

                                    •





         ▓▓▓▓▓▓            ▓▓▓▓▓▓            ▓▓▓▓▓▓            ▓▓▓▓▓▓
         ██░░██            ██░░██            ██░░██            ██░░██

                                      ▄██▄
────────────────────────────────────────────────────────────────────────────────
                    VIDAS 3   SONIDO SI   P PAUSA  ESC SALIR
```
