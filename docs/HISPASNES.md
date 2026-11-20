# Hispa-Snes

La tercera de las «consolas de cada época» (docs/VISION.md), después de
Hispa-Nes y de Hispa-System: un emulador **propio** de la consola de 16 bits
de 1990, para los cartuchos volcados en archivos `.SFC` o `.SMC`. Es un
programa de Hispa Ventanas (`HISPASNS.HVP`, grupo «Juegos») y corre también
en **Hispa-Pentium**, con HVPRUN.

- Todo el código está escrito aquí, a partir de la **documentación pública
  del hardware**: el 65C816 (el manual de su fabricante), la PPU, el DMA y el
  HDMA, los registros de la CPU de la consola, el SPC700 y el DSP del sonido
  (las descripciones públicas de la comunidad: «fullsnes», los documentos de
  anomie, la wiki de SNESdev). Sala limpia: no lleva nada del fabricante de la
  consola ni de otros emuladores (ni de Snes9x ni de bsnes/higan).
- El **arranque del procesador de sonido** (los 64 octetos de FFC0h, que en
  la consola vienen de fábrica) es nuestro: `programas/hispasnes/arranque.s`,
  que hace lo mismo por fuera (el mismo trato por los puertos, que es lo que
  usan los juegos para mandarle su programa) con su propio código.
- Las **ROM comerciales no van en el proyecto**: quien conserve sus cartuchos
  usa sus propios volcados. Con el emulador va `PRUEBA.SFC`, una ROM nuestra
  (fondos, objetos, suma de colores, una ventana, HDMA y una melodía).
- No se nombra la consola en el programa: en la ayuda es «la consola de 16
  bits de 1990»; `.SFC` y `.SMC` son solo el formato de los archivos.

## Cómo se usa

Se abre desde el grupo «Juegos». Los cartuchos van en su propia carpeta,
**`\HISPADOS\JUEGOS\SNES`** (la del módulo y dentro `SNES`; allí está
`PRUEBA.SFC`). Al entrar no se abre ninguno: la ventana queda en negro con
«Sin cartucho» hasta **Archivo › Abrir ROM**, que empieza en esa carpeta y
luego en la última de la que se abrió uno (se apunta en `HISPASNS.INI`, al
lado del módulo: `Carpeta=A:\...`, y también `Saltar=n`, lo de «Pintar»).

Como Hispa-System, **empieza en pausa**: al abrir un cartucho corre un
segundo y se queda quieto con el letrero «En pausa — Pulse una tecla o haga
clic»; sigue con cualquier tecla, un clic en la imagen o un botón del mando.
Se para solo (y callado) con un menú abierto, un diálogo, sin el foco o
minimizado.

| Menú | Qué hace |
|---|---|
| Archivo › Abrir ROM... | El diálogo de archivos: un `.SFC` o `.SMC` (con la cabecera de 512 octetos de las copiadoras o sin ella) |
| Archivo › Reiniciar (F2) | El botón de reinicio de la consola |
| Archivo › Pausa o continuar (P) | Para y sigue (con el letrero) |
| Imagen › x1, x2 | La ventana a la medida: 256×224 o el doble |
| Imagen › Pantalla completa (Alt+Intro) | Sin la ventana alrededor, a la mayor escala entera (Esc vuelve) |
| Imagen › Pintar los que dé tiempo / todos / 1 de cada 2, 3 o 4 | El **salto de cuadros**: la consola corre siempre a su velocidad, pero se pintan menos cuadros. «Los que dé tiempo» (lo normal) deja de pintar alguno, como mucho tres seguidos, solo si el equipo no llega |
| Sonido | Lo pone o lo quita |
| Ayuda | Teclas y mandos; Acerca de |

| Consola | Teclado | Mando (HSPJOY) |
|---|---|---|
| Cruceta | Flechas | Cruceta o palanca |
| B | Z | Botón 1 |
| A | X | Botón 2 |
| Y | A | Botón 3 (con más de cuatro botones) |
| X | S | Botón 4 (con más de cuatro) |
| L | Q | Botón 5 |
| R | W | Botón 6 |
| Select | Espacio | Botón 7 o 9 (con cuatro o menos: el 3) |
| Start | Intro | Botón 8 o 10 (con cuatro o menos: el 4) |

Los cartuchos **con pila** guardan la partida en un archivo `.SRM` al lado de
la ROM (el mismo formato que usan los demás emuladores: la RAM del cartucho
tal cual, del tamaño que dice su cabecera). Se lee al abrir la ROM y se
escribe sola dos segundos después del último cambio (o a los treinta si el
juego no para de escribir), al cambiar de cartucho y al cerrar la ventana.

Si el cartucho lleva **un chip especial** (el procesador de gráficos GSU, el
segundo procesador SA-1, los coprocesadores matemáticos DSP, el descompresor
S-DD1, el reloj S-RTC, el OBC1 u otros), al abrirlo sale un aviso claro:
«Este cartucho lleva un procesador de gráficos (GSU), que Hispa-Snes todavía
no emula: puede que no arranque o que se vea o suene mal». Se puede seguir
igualmente.

Parámetros del acceso directo: una ROM (se abre al entrar), `/1` (x1 aunque
la pantalla sea grande) y `/S=n` (pintar 1 de cada n). Para las pruebas:
`/P` (paso a paso) y `/T=n` (n cuadros a toda velocidad y el resultado en
`HISPASNS.LOG`: tiempo, cuadros por segundo, suma de la imagen y de la RAM, y
lo que una ROM de prueba haya dejado escrito en su RAM; con `/V`, pintando
cada cuadro).

### En Hispa-Pentium (HVPRUN)

`HVPRUN \JUEGOS\HISPASNS.HVP [\JUEGOS\ALGO.SFC]` (o, en Mi equipo, un doble
clic en `HISPASNS.HVP`, que lo abre con HVPRUN). Allí no hay modo real ni
puertos: el emulador lo nota (en HVPRUN, `aj_leer` no da el texto de la
versión de Ventanas) y entonces no busca HSPJOY (INT 2Fh), no lee el 8254
(toma el **reloj fino de HVPRUN**, `aj_leer 20`, que se ha añadido para esto)
y va **sin sonido** (HVPRUN aún no lo tiene). Las teclas de Hispa-Pentium
llegan como pulsaciones sin aviso de soltar: cada una vale unos cuadros (como
en Hispa Ventanas cuando falta el mapa de teclas), así que conviene dejarla
pulsada (la repetición del teclado la mantiene).

El cambio en HVPRUN (marcado `[HISPA-SNES]`): `castel/nucleo32/hvprun/
pfaltan.inc` (`aj_leer` con 20) y `pent.inc` (`p_reloj_fino`: los tics de
Hispa-Pentium, a 100 por segundo, pasados a tics del 8254). Nada más: lo de
pintar rápido y no dormir mientras el programa trabaja ya estaba (`ceder`
marca actividad).

## Qué emula

| Parte | Estado |
|---|---|
| Procesador (65C816) | Todas las instrucciones en los dos modos (emulación del 6502 y nativo), con el acumulador y los índices de 8 o de 16 bits, el modo decimal (también en 16 bits), MVN/MVP, WAI/STP, las interrupciones (NMI, IRQ, BRK, COP) y los ciclos de cada instrucción (con los de más de la página directa, el cruce de página y los saltos). **Pasa las pruebas de un solo paso** (SingleStepTests: registros, memoria y ciclos, en los 512 archivos) |
| Memoria y cartuchos | LoROM, HiROM y ExHiROM (la cabecera se busca en los tres sitios y se queda la que más pinta tiene: suma, modo, vector, título); ROM de tamaños que no son potencia de 2 (con su espejo); la RAM del cartucho de 2 a 128 KB (con espejos también la pequeña); la RAM de 128 KB y su puerto ($2180-$2183); la ROM rápida (MEMSEL: 6 ciclos del reloj en vez de 8) |
| DMA y HDMA | Los 8 canales, los 8 modos, de A a B y de B a A, con el tiempo que para al procesador; HDMA directo e indirecto, con repetición |
| CPU de la consola | NMI con su bandera ($4210), IRQ por H, por V y por H+V ($4207-$420A, $4211), el borrado vertical y horizontal ($4212), multiplicar y dividir ($4202-$4206), los mandos leídos solos ($4218-$421F) y uno a uno ($4016/$4017), los contadores de línea y punto ($2137, $213C, $213D) |
| Imagen (PPU) | Los modos 0 a 7: fondos de 2, 4 y 8 bits por punto, dibujos de 8x8 y 16x16, volteados, prioridades, el fondo 3 delante en el modo 1, mapas de 32x32 a 64x64, el desplazamiento por columnas (modos 2, 4 y 6), el color directo; el **modo 7** (girar y escalar, con sus tres formas de lo de fuera, volteado y EXTBG); los 128 objetos (8x8 a 64x64, las dos tablas de dibujos, x negativa, 32 por línea, la rotación de prioridad); las dos ventanas con sus cuatro lógicas, la pantalla principal y la secundaria, la **suma y la resta de colores** (y a la mitad), el color fijo, recortar a negro y el brillo; la VRAM con su avance y su recolocación de direcciones, la CGRAM y la OAM con sus «latch». Línea a línea: lo que el programa cambia en el borrado horizontal (HDMA) se ve en la línea siguiente |
| Sonido (SPC700) | Todas las instrucciones con sus ciclos (**pasa las pruebas de un solo paso** del SPC700), los tres temporizadores, los cuatro puertos, el arranque (propio) y la RAM de debajo |
| Sonido (DSP) | Las 8 voces con muestras BRR (los cuatro filtros, la vuelta), ADSR y GAIN (con su tabla de velocidades), el tono con su modulación, el ruido, el eco con su filtro de 8 coeficientes y su realimentación, el volumen; 32.000 muestras por segundo en estéreo, que se pasan a la frecuencia de la tarjeta (48.000 en estéreo con HD Audio y AC'97, 22.050 con la Sound Blaster 16) |

**Sin hacer** (pendiente):
- los **chips especiales** de algunos cartuchos (GSU, SA-1, DSP-1 a 4,
  S-DD1, SPC7110, CX4, ST010/018, S-RTC, OBC1): se avisa al abrirlos;
- los modos de alta resolución (5 y 6, y el «pseudo-hires»): se pintan a 256
  de ancho tomando un dibujo de 8 por cada entrada del mapa (se ven, pero no
  bien); el entrelazado; el mosaico del modo 7 (el de los fondos de los
  modos 0 a 6 sí está); las 239 líneas («overscan»: se enseñan las 224 de
  arriba);
- lo que pasa a mitad de una línea (se pinta la línea entera de una vez), el
  límite de 34 dibujos de objetos por línea y la bandera de «time over»;
- la curva con la que el DSP pasa de una muestra a otra (aquí, una recta: se
  oye un poco más «brillante»); el contador global de la envolvente (cada
  velocidad cuenta por su cuenta);
- los tiempos finos de la memoria (el procesador gasta 6 u 8 ciclos del
  reloj por ciclo suyo según de dónde lee el código; los registros de 12 no
  se cuentan aparte); el «bus abierto» se aproxima con la parte alta de la
  dirección.

Compatibilidad: **solo se ha probado con nuestras ROM de prueba**; en el
proyecto no hay ni se han usado ROM comerciales. Por cómo está hecho (el
procesador exacto en sus ciclos, el DMA y el HDMA, las interrupciones en su
punto de la línea y el sonido con el SPC700 al día cada vez que el 65C816
toca sus puertos), lo esperable es que la mayoría de los juegos sin chip
especial arranquen y se vean y suenen bien; los que juegan con lo que pasa a
mitad de línea o con la alta resolución tendrán algún fallo de imagen.

## Cómo está hecho

`programas/hispasnes.asm` es lo que lo une a Hispa Ventanas (lo mismo que en
Hispa-System: la ventana, el reloj, las teclas, los mandos de HSPJOY, el
sonido por HSPSND, los archivos); la consola está en `programas/hispasnes/`,
en archivos que no llaman a Hispa Ventanas (por eso se montan también como
programa de Linux para las pruebas):

| Archivo | Qué |
|---|---|
| `cpu.inc` | El 65C816. Cada instrucción está escrita **cinco veces** con macros de NASM, una por cada combinación de modos (emulación; nativo con M y X a 0 o a 1), cada una con su tabla de 256 saltos: ninguna instrucción mira en qué modo está; REP, SEP, XCE, PLP y RTI cambian de tabla. A en EBX, X en ECX, Y en EBP, el contador de programa como puntero (ESI), los ciclos en EDI; N y Z como «el último resultado» |
| `memoria.inc` | Leer y escribir: los 16 MB en 2.048 trozos de 8 KB, cada uno con un puntero a la memoria (la RAM, la ROM, la RAM del cartucho) o a cero (los registros y lo que no hay, que van por `bus.inc`) |
| `bus.inc` | Los registros, el DMA, el HDMA, los mandos y **el reloj de la consola**: `snes_cuadro` corre las 262 líneas (312 en PAL) de 1.364 ciclos del reloj; en cada una, el procesador hasta el punto de la IRQ si la hay, hasta el borrado horizontal (entonces se pinta la línea y se hace el HDMA) y hasta el final. El SPC700 va detrás y se pone al día cuando el 65C816 lee o escribe sus puertos y al final de cada línea |
| `ppu.inc` | La imagen. Los dibujos de la VRAM se pasan cuando hace falta a una **caché de un octeto por punto** (una para cada número de bits; escribir en la VRAM marca los que cambian). Cada capa se pinta en la línea con su **altura** (según el modo y la prioridad): un punto solo se queda si está por encima de lo que hay, en la pantalla principal y, si hace falta para la suma de colores, en la secundaria. Lo corriente (dibujos de 8x8 o 16x16, con máscara o sin ella) va por un camino sin llamadas por dibujo. Al final, cada punto pasa a su color: sin suma, de una tabla con el brillo ya puesto; con suma, los tres componentes de una vez |
| `spc.inc` | El SPC700 (una rutina por instrucción y su tabla), los registros de F0h a FFh, los temporizadores (se cuentan cuando se miran, sin gastar nada mientras tanto) y el arranque |
| `dsp.inc` | El DSP: una muestra cada 32 ciclos del SPC700 |
| `mapas.inc` | La cabecera, el tipo de cartucho, el espejo de la ROM y los mapas de memoria |
| `arranque.s` | El arranque del SPC700 (fuente; sus 64 octetos van en `spc.inc`) |
| `datos.inc` | Las variables (con 4 KB sin usar delante, como en Hispa-Nes) |

La paleta: la consola tiene 32.768 colores y Hispa Ventanas 256. Al empezar
se busca, para cada color de 12 bits (4.096), el más parecido de los 256, y
de ahí sale la tabla de los 32.768 (`lut15`); cada color de la CGRAM se pasa
con ella (y con el brillo) cuando cambia.

### El ensamblador: tools/asm816.py

Un ensamblador propio para los dos procesadores de la consola (65C816 y
SPC700, este con la sintaxis de su fabricante), para las ROM de prueba y el
arranque: dos pasadas (más si hace falta, hasta que no cambie nada),
etiquetas locales con `@`, constantes, `.lorom`/`.hirom`, `.org`, `.base`
(código que se ensambla para otra dirección: el del SPC700 que se le sube),
`.a8`/`.a16`/`.i8`/`.i16` (y REP/SEP con un número los ponen solos),
`.byte`, `.word`, `.long`, `.ascii`, `.res`, `.incbin`, `.include`, `.tiles`
(dibujos de 2, 4 u 8 bits por punto en texto: las letras propias de
`sms/comun/letras.s` sirven tal cual) y `.cabecera` (la cabecera del
cartucho con su suma). Nunca corta una dirección larga a corta (lo dice).

## Velocidad

Una prueba que tiene de todo a la vez (`PRUEBA.SFC`: el procesador siempre
trabajando, el SPC700 mirando su temporizador sin parar, la melodía con eco,
tres fondos, objetos, la suma de colores con la pantalla secundaria, una
ventana y HDMA en cada línea):

| Dónde | Cuadros por segundo |
|---|---|
| El núcleo como programa de Linux, en el servidor de pruebas | unos 1.250 (pintando) |
| Hispa Ventanas en QEMU (sin KVM), pintando cada cuadro al doble | unos **110** |
| Hispa-Pentium con HVPRUN en QEMU, pintando cada cuadro (x1) | unos **105** |

Al empezar daba 37 en QEMU (al doble); lo que más costaba era la suma de colores punto
a punto, las ventanas (se calculaban punto a punto en cada línea) y pintar
los dibujos llamando a una rutina por cada uno: ahora la suma va con los tres
componentes de una vez, las ventanas se hacen por tramos y solo cuando
cambian, y los fondos se pintan de un tirón. Los temporizadores del SPC700 y
la envolvente del DSP ya no dividen.

**No se ha medido en un equipo de verdad.** QEMU sin KVM, en este servidor,
va más o menos como un Pentium II de los rápidos; por eso lo esperable es
que en un **Pentium II a 233-300 MHz** vaya a su velocidad o cerca, y si no
llega, «Pintar los que dé tiempo» deja de pintar algún cuadro (el juego sigue
a su velocidad; la barra de estado dice cuántos se pintan) o se puede elegir
«Pintar 1 de cada 2». Los juegos con más voces sonando, más objetos o el modo
7 en toda la pantalla gastan algo más que la prueba. Para medirlo en cada
equipo: el acceso con `/T=600 /V` y una ROM, y mirar `HISPASNS.LOG`.

## Pruebas

`tests/run_hispasnes_tests.sh` (con `SOLO="nucleo ventanas sonido pentium"`
para algunas partes):

1. **El núcleo, fuera de QEMU** (`tests/hispasnes/snesnativo.asm`, los mismos
   `.inc` como programa de Linux de 32 bits, hecho solo con NASM):
   - el arranque propio del SPC700 (`arranque.s`) es el que lleva `spc.inc`;
   - las **pruebas de un solo paso** (SingleStepTests, de Thomas Harte y
     otros, licencia MIT): miles de casos de cada instrucción del 65C816 (en
     emulación y en nativo) y del SPC700, sacados de procesadores de verdad,
     con los registros, la memoria y los ciclos de antes y de después. No van
     en el proyecto: `tests/thirdparty/fetch_snes.sh` baja el principio de
     cada archivo (unos 100 casos de cada uno; 75.000 en total) y
     `tests/hispasnes/sst.py` los pasa a binario. Sin red, se saltan;
   - `BUSTEST.SFC` (`tests/hispasnes/bustest.s`): NMI y WAI, la bandera del
     borrado vertical, la IRQ por V y por H+V (con los contadores), multiplicar
     y dividir, el puerto de la RAM, DMA, HDMA directo e indirecto, MVN y la
     RAM del cartucho con espejo; deja «TODO BIEN» en su RAM;
   - `SPCTEST.SFC` (`spctest.s`): sube un programa al SPC700 con el trato del
     arranque, este hace MUL, DIV y DAA y lo devuelve por los puertos («SPC700
     BIEN»), y toca una melodía con una muestra BRR propia y eco: se miden las
     frecuencias de las notas en el sonido;
   - las pantallas de `PPU1` (modo 0 y texto), `PPU2` (modo 1 con tres fondos,
     objetos, suma de colores a la mitad, una ventana y HDMA), `PPU3` (modo 7
     girado, la ventana del color con resta y el brillo), `PPU4` (el mosaico),
   `BUSTEST`, `SPCTEST`
     y `PRUEBA.SFC`, punto por punto como las de `tests/hispasnes/esperado/`.
2. **En Hispa Ventanas** (QEMU, el disquete con el módulo): `/T` con
   `BUSTEST.SFC` y `SPCTEST.SFC` dentro del módulo, y la velocidad con
   `PRUEBA.SFC` pintando cada cuadro.
3. **El sonido**: `PRUEBA.SFC` a su velocidad con HSPSND, con una Sound
   Blaster 16 y con HD Audio: la melodía en la grabación de QEMU.
4. **En Hispa-Pentium**: el disco de `nucleo32/disco.sh` con la oficina y el
   emulador en `\OFICINA`, desde la Consola: `BUSTEST.SFC` da lo mismo que en
   Hispa Ventanas (la misma suma de la imagen) y la velocidad pintando.

Las ROM de prueba son nuestras, en `tests/hispasnes/*.s`, con lo común
(`comun.s`: el arranque, el DMA a la VRAM, las letras y escribir texto;
`sonido.s`: subir un programa al SPC700). `PRUEBA.SFC` (`demo.s`) es la
pantalla de PPU2 con la melodía de SPCTEST.

## En la imagen

`HISPASNS.HVP` mide casi 200 KB: **no cabe en el disquete de arranque**
(que ya va justo), así que va solo en la imagen USB de Hispa-386 y de
Hispa-486 y en los `.PAQ` (`HVPJ` en el Makefile: los juegos que van solo
ahí; `tools/mkusb.py` copia todos los `.HVP` de `build/`), con la carpeta
`\HISPADOS\JUEGOS\SNES` y `PRUEBA.SFC`. En Hispa-486, el acceso está en el
grupo «Juegos» de `sistema/486/CENTRO.INI`. En Hispa-Pentium, la memoria USB
de `nucleo32/usb.sh` lleva `\JUEGOS\HISPASNS.HVP` y `\JUEGOS\PRUEBA.SFC`
(se abren desde Mi equipo, con HVPRUN).

## Pendiente

- Probarlo en equipos de verdad (un Pentium II, el HP Mini, el Toshiba) y con
  cartuchos volcados de verdad.
- Los chips especiales, empezando por el DSP-1 (el más usado) y el SA-1.
- Los modos de alta resolución bien hechos, el mosaico del modo 7 y el
  entrelazado.
- La curva de paso entre muestras del DSP y su contador global.
- Guardar el estado de la consola en cualquier momento.
- Sonido en HVPRUN (cuando Hispa-Pentium lo tenga).
