# Hispa-System

La segunda de las «consolas de cada época» (docs/VISION.md): un emulador
**propio**, compatible con los cartuchos de la Master System, que corre como
programa de Hispa Ventanas (`HISPASMS.HVP`, grupo «Juegos»), con un juego
nuestro, **Buzo** (`BUZO.SMS`).

- Todo el código está escrito aquí, a partir de la **documentación pública
  del hardware**: el procesador Z80 (el manual de su fabricante y los
  documentos públicos de lo que hace sin estar documentado), el VDP de la
  consola (315-5124 y 315-5246, un TMS9918 con un modo más), el chip de
  sonido SN76489, los puertos y los mapeadores de los cartuchos (la wiki de
  SMS Power!). Sala limpia: no lleva nada del fabricante de la consola ni de
  otros emuladores.
- Las **ROM comerciales no van en el proyecto**. Quien conserve sus cartuchos
  usa sus propios volcados (archivos `.SMS`). La marca de la consola solo se
  cita para decir con qué cartuchos es compatible.
- Buzo es nuestro (GPL), escrito en ensamblador de Z80 para el hardware de
  la consola: funciona aquí, en otros emuladores y en una consola de verdad.

## Cómo se usa

Se abre desde el grupo «Juegos». Si `BUZO.SMS` está en su carpeta
(`\HISPADOS\JUEGOS`), arranca Buzo; si no, la ventana queda en negro con
«Sin cartucho».

| Menú | Qué hace |
|---|---|
| Archivo › Abrir ROM... | El diálogo de archivos de Hispa Ventanas: se elige un `.SMS` |
| Archivo › Buzo | Abre el juego de muestra |
| Archivo › Reiniciar (F2) | El botón de «reset» de la consola |
| Archivo › Pausa (P) | Para y sigue el emulador (también se para solo al minimizar) |
| Tamaño › x1, x2 | La ventana, a la medida de la imagen: 256×192 o el doble. Si la ventana se estira, la imagen sale al doble si cabe, centrada |
| Sonido | Lo pone o lo quita |
| Ayuda | Teclas y mandos; Acerca de |

| Consola | Teclado | Mando (HSPJOY) |
|---|---|---|
| Cruceta | Flechas | Cruceta o palanca |
| Botón 1 | Z | Botón 1 (y el 4, si hay más de cuatro) |
| Botón 2 | X | Botón 2 (y el 3, si hay más de cuatro) |
| Pausa (el botón de la consola) | Intro | Botón 3 o 4; con más de cuatro botones, del 7 al 10 |

Con `HSPJOY` cargado (docs/MANDOS.md) valen los mandos que hagan de jugador
1 y de jugador 2; el teclado es siempre el jugador 1. Abajo, la barra de
estado dice el cartucho y los cuadros por segundo que salen (y, entre
paréntesis, cuántos se pintan si el equipo no llega a pintarlos todos).

Los cartuchos **con pila** guardan su partida en un archivo `.SAV` (32 KB,
los dos bancos de la RAM del cartucho) al lado de la ROM. Como en la
consola no hay nada en la cabecera que diga si un cartucho lleva pila, se
guarda si el juego ha encendido alguna vez esa RAM: se lee al abrir la ROM y
se escribe sola dos segundos después del último cambio (o a los treinta si el
juego no para de escribir), al cambiar de cartucho y al cerrar la ventana.

En los parámetros del acceso directo se puede poner una ROM (se abre al
entrar), `/1` (tamaño x1 aunque la pantalla sea grande) y `/M=1` o `/M=2`
(el mapeador de Sega o el de Codemasters, si no se reconoce solo). Para las
pruebas: `/P` (paso a paso: la consola solo avanza con cada tecla, seis
cuadros con el botón y seis sin él; Espacio, treinta cuadros) y `/T=n` (n
cuadros a toda velocidad y el resultado en `HISPASMS.LOG`; con `/V`,
pintando cada uno en la ventana).

## Qué emula

| Parte | Estado |
|---|---|
| Procesador (Z80) | Todas las instrucciones, con sus ciclos: las principales, CB, ED, DD/FD y DDCB/FDCB, y las no documentadas (IXH, IXL, IYH, IYL; SLL; las DDCB que copian el resultado a un registro; las ED repetidas; `IN F,(C)` y `OUT (C),0`). Las banderas 3 y 5 de todas, también las de `BIT n,(HL)`, que salen del registro oculto («MEMPTR»), y las de las instrucciones de bloque de entrada y salida. R cuenta. Interrupciones IM 0, 1 y 2 (en la consola, IM 1: RST 38h), la NMI del botón de pausa, HALT, y el retraso de una instrucción tras EI. **Pasa zexdoc y zexall enteras** |
| Imagen (modo 4) | Por líneas: fondo de 32×28 con desplazamiento horizontal y vertical, las dos filas de arriba fijas y las ocho columnas de la derecha fijas (registro 0, bits 6 y 7), la columna de la izquierda tapada (bit 5), dibujos girados en los dos sentidos, las dos paletas y la prioridad sobre los objetos; 64 objetos de 8×8 u 8×16 y al doble, corridos 8 puntos a la izquierda (bit 3), ocho por línea con la bandera de «más de ocho» y la de choque; el contador de líneas y su interrupción, la de cuadro, los contadores de línea y de punto (7Eh y 7Fh), el búfer de lectura del puerto de datos. 192 líneas, 262 por cuadro (NTSC) |
| Imagen (modos del TMS9918) | Los cuatro: gráfico I, gráfico II (con sus máscaras), texto de 40 columnas y multicolor; 32 objetos de 8×8 o 16×16 (y al doble), cuatro por línea con el aviso del quinto (y su número), el desplazamiento de 32 puntos del bit de «reloj temprano»; la paleta fija de 16 colores pasada a la de la consola. Con ellos van los juegos de la consola anterior que la Master System acepta y algunos de los primeros |
| Paleta | La de la consola: 32 colores de 64 (2 bits de rojo, de verde y de azul). Cada uno pasa al más parecido de los 256 de Hispa Ventanas |
| Sonido (SN76489) | Tres canales de tono (periodo de 10 bits) y uno de ruido (blanco o periódico, con su registro de desplazamiento de 16 bits, a tres velocidades o a la del tercer tono), atenuación en pasos de 2 dB. Un periodo de 0 o 1 deja la salida arriba (así tocan los juegos sonidos grabados). Por HSPSND, al ritmo de la tarjeta: 48.000 Hz en estéreo de 16 bits con HD Audio y AC'97, 22.050 Hz con las Sound Blaster |
| Mandos | Dos, con los puertos DCh y DDh; las patillas TH y TR que se ponen de salida en el puerto 3Fh se leen como en una consola de fuera de Japón (con eso miran los juegos de qué región es la consola). El botón de reinicio, suelto |
| Cartuchos | El mapeador **de Sega** (bancos de 16 KB en 0000h, 4000h y 8000h, el primer KB fijo, y la RAM del cartucho de 32 KB, con pila o sin ella, en 8000h) y el **de Codemasters** (se reconoce por la cabecera de sus juegos, en 7FE0h). Las ROM de 32 KB o menos, sin mapeador, también. Hasta 4 MB; la cabecera de 512 octetos de las copiadoras se salta |

**Sin hacer** (pendiente): la **Game Gear** (su paleta de 12 bits, la
ventana de 160×144, el botón Start en el puerto 00h y el sonido estéreo); los
modos de 224 y 240 líneas de la segunda versión del VDP; la unidad FM
japonesa; los cambios de paleta o de desplazamiento a mitad de una línea (se
dibuja una línea entera de una vez); y el mapeador de algunos cartuchos
coreanos.

Compatibilidad: con el mapeador de Sega y el de Codemasters se cubre casi
todo el catálogo de la consola. **Solo se ha probado con ROM de prueba y con
Buzo**: en el proyecto no hay ni se han usado ROM comerciales. Lo esperable,
por cómo está hecho (por líneas, con el procesador exacto y las
interrupciones de línea en su línea), es que los juegos se vean bien,
incluidos los que parten la pantalla con la interrupción de línea; los pocos
que dependen del punto exacto de una línea en el que pasa algo pueden verse
con algún fallo.

## Cómo está hecho

`programas/hispasms.asm` es lo que lo une a Hispa Ventanas; la consola está
en `programas/hispasms/`, en archivos que no llaman a Hispa Ventanas (por eso
se pueden montar también como programa de Linux para las pruebas):

| Archivo | Qué |
|---|---|
| `z80.inc` | El procesador. Una rutina por instrucción, hechas con macros de NASM (las de DD y FD salen de las mismas que las de HL), y tablas de 256 saltos. Los registros del Z80 están en memoria; el contador de programa en ESI y el tiempo que queda en EDI. Las banderas salen de las del x86 (`LAHF`: S, Z, H y C están en el mismo sitio; P/V del desbordamiento o de una tabla de paridad). La memoria se ve por una tabla de punteros, uno por cada KB |
| `vdp.inc` | La imagen, los puertos y `sms_cuadro`: las 262 líneas de un cuadro. Cada línea visible se pinta de una vez (el fondo, de ocho en ocho puntos con tablas que pasan cada octeto de un plano a ocho puntos; luego los objetos, del primero al último) y luego corre el procesador sus 228 ciclos |
| `psg.inc` | El sonido. Las muestras de cada línea, a la frecuencia que pide la ventana |
| `mapas.inc` | Los cartuchos: la ROM (repetida hasta una potencia de 2), los mapeadores y la memoria de la consola |
| `datos.inc` | Las variables, con 4 KB sin usar delante (como en Hispa-Nes: escribir en la misma página que el código que se ejecuta es carísimo) |

Lo de Hispa Ventanas (`hispasms.asm`) es lo mismo que en Hispa-Nes: el
reloj (59,92 cuadros por segundo, con el contador 0 del 8254; si el equipo
no llega, no pinta ese cuadro), la imagen a los colores de Hispa Ventanas y
al doble con `dc_copiar`, las teclas por `tec_mapa`, los mandos por HSPJOY.
El sonido, en cambio, ya usa la interfaz nueva de HSPSND: `snd_info` para
saber la tarjeta, trozos del tamaño exacto que toca la tarjeta con
`snd_pcm2` y, antes de mandar uno, la función 08h (`snd_orden`) para saber
si cabe; si no, se queda para la vuelta siguiente: la ventana nunca espera.

### Velocidad

Un cuadro son 262 líneas de 228 ciclos del Z80 (unas 15.000 instrucciones
del Z80) y 192 líneas de imagen. En el servidor de pruebas, el núcleo como
programa de Linux da unos 5.000 cuadros por segundo con Buzo; en QEMU (sin
KVM), dentro de Hispa Ventanas y pintando cada cuadro en la ventana al
doble, unos **500 cuadros por segundo** (y unos 1.300 sin pintar), así que
va a sus 60 con mucha holgura. **No se ha medido en un equipo de verdad**:
la prueba apunta lo que da en cada equipo (`/T=600 /V`). Si un equipo no
llega, se salta el dibujo de algunos cuadros y el juego sigue a su
velocidad (en la barra de estado se ve cuántos pinta).

## El juego propio: Buzo

`sms/buzo/buzo.s` (el programa, la música y los efectos) y
`sms/buzo/dibujos.s` (los dibujos, en texto), con las letras propias de
`sms/comun/letras.s`. Se ensamblan en la compilación con `tools/asmz80.py`
(un ensamblador de Z80 hecho para esto, como `tools/asm6502.py` para
Hispa-Nes: dos pasadas, etiquetas locales con `@`, `.org`, `.pad`, `.byte`,
`.word`, `.res`, `.incbin`, `.include`, `.tiles` para los dibujos de 4
planos en texto y `.cabecera_sega` para la cabecera del cartucho con su
suma; todas las instrucciones, también las no documentadas). Sin
dependencias. Sale `BUZO.SMS`, de 32 KB.

El buzo nada por el fondo del mar, que corre hacia la izquierda, y le salen
al paso peces (que se le acercan poco a poco) y medusas (que suben y bajan).
Cruceta: nadar. Botón 1 o 2: arpón (dos a la vez). Pausa: parar y seguir; en
el título, empezar. Un pez son 10 puntos y una medusa 20; cada 100 puntos
salen más y más deprisa. Un choque quita una vida (hay tres) y el buzo
parpadea un rato sin que le pase nada. La mejor puntuación se guarda en la
RAM con pila del cartucho (`BUZO.SAV`: «BUZ» y la puntuación).

Usa lo que hay que probar: el desplazamiento horizontal con las dos filas
del marcador fijas; la **interrupción de línea** en la 127, que cambia el
desplazamiento para que el fondo del mar corra el doble que el agua
(paralaje); la interrupción de cuadro con HALT; objetos de 8×16; la NMI del
botón de pausa; los cuatro canales de sonido (la música, una melodía propia
en la menor, en dos; el arpón en el tercero; los golpes en el ruido); y la
RAM del cartucho con pila. Con los mismos botones en los mismos cuadros la
partida es siempre igual.

Los siguientes juegos irán en `sms/` y en `SMSROMS` del Makefile.

## Pruebas

`tests/run_hispasms_tests.sh`:

1. **El núcleo, fuera de QEMU** (`tests/hispasms/smsnativo.asm`: los mismos
   `.inc` como programa de Linux de 32 bits, hecho solo con NASM):
   - el ensamblador: `tests/hispasms/asmz80_todas.s`, las 1.149 formas de
     las instrucciones, tiene que dar lo de `asmz80_todas.hex` (comprobado,
     al hacerlo, con otro ensamblador);
   - **zexdoc y zexall** (Frank D. Cringle, GPL 2), las pruebas de cada
     instrucción del Z80 contra uno de verdad, como programas de CP/M
     (`smsnativo -c`: memoria plana y las funciones de escribir del
     sistema). No van en el proyecto: las baja
     `tests/thirdparty/fetch_sms.sh` (sin red, se saltan). Unos 20 segundos
     cada una;
   - `VDPTEST.SMS` (`tests/hispasms/vdptest.s`), nuestra: VRAM y su búfer,
     la RAM y su espejo, el mapeador y el primer KB fijo, la RAM del
     cartucho, los mandos y las patillas TH, la bandera de cuadro y el
     contador de líneas, las interrupciones de línea y de cuadro con IM 1 y
     HALT, el retraso de EI, más de ocho objetos en una línea y el choque;
     y una pantalla del modo 4 con casi todo, que tiene que ser la guardada;
   - `TMSTEST.SMS` (`tests/hispasms/tmstest.s`): el modo gráfico II, sus
     colores, objetos de 16×16 al doble y el aviso del quinto objeto;
   - `MAPTEST` (`tests/hispasms/maptest.s`), dos veces: con el mapeador de
     Sega y con el de Codemasters (y sin su cabecera no se reconoce);
   - Buzo con las teclas de la prueba: las imágenes que tienen que verse,
     guardadas en `tests/hispasms/esperado/`, y la mejor puntuación en la
     RAM del cartucho al acabar.
2. **En Hispa Ventanas, paso a paso**: se juega con el teclado y cada
   pantalla es, punto por punto, la esperada (al doble y a tamaño normal);
   la pausa de la consola; menús y diálogos; abrir ROM; un archivo que no
   vale; la partida guardada (`BUZO.SAV`) y la mejor puntuación al volver.
3. **A su velocidad**, con una Sound Blaster 16: Intro (Pausa) empieza la
   partida, los arpones y la música suenan por HSPSND (en la grabación de
   QEMU) y se sale con el juego en marcha; y otra vez con HD Audio (48.000 Hz
   en estéreo).
4. **Con un mando** de HSPJOY (`NESJOY.COM`, el de Hispa-Nes, que ahora
   puede pulsar otro botón: `NESJOY 1`): su botón 1 empieza la partida sin
   tocar el teclado. (La pausa del mando no se prueba así: es la NMI, que
   salta al pulsar, y este mando está pulsado desde antes de abrir el
   cartucho.)
5. **`/T`**: `VDPTEST.SMS` dentro del módulo da lo mismo que fuera, y la
   velocidad en QEMU con Buzo.

## Pendiente

- Probarlo en equipos de verdad (un Pentium y un 486) y con cartuchos
  propios volcados.
- La Game Gear (el mismo núcleo con su paleta y su ventana más pequeña).
- Los modos de 224 y 240 líneas, el sonido FM y los efectos a mitad de línea.
- Más juegos propios (plataformas, puzles).
- Guardar el estado de la consola en cualquier momento.
