# Hispa-Nes

La primera de las «consolas de cada época» (docs/VISION.md): un emulador
**propio**, compatible con los cartuchos de la NES, que corre como programa
de Hispa Ventanas (`HISPANES.HVP`, grupo «Juegos»), con un juego nuestro,
**Cometas** (`HISPA1.NES`).

- Todo el código está escrito aquí, a partir de la **documentación pública
  del hardware** (el procesador 6502, los registros de imagen y de sonido,
  los circuitos de los cartuchos y el formato iNES). Sala limpia: no lleva
  nada del fabricante de la consola ni de otros emuladores.
- Las **ROM comerciales no van en el proyecto**. Quien conserve sus cartuchos
  usa sus propios volcados (archivos `.NES`). La marca de la consola solo se
  cita para decir con qué cartuchos es compatible.
- Los juegos que van con Hispa-DOS son nuestros (GPL), escritos en
  ensamblador de 6502 para el hardware de la consola: funcionan aquí, en
  otros emuladores y en una consola de verdad.

## Cómo se usa

Se abre desde el grupo «Juegos». Si `HISPA1.NES` está en su carpeta
(`\HISPADOS\JUEGOS`), arranca Cometas; si no, la ventana queda en negro con
«Sin cartucho».

| Menú | Qué hace |
|---|---|
| Archivo › Abrir ROM... | El diálogo de archivos de Hispa Ventanas: se elige un `.NES` |
| Archivo › Cometas | Abre el juego de muestra |
| Archivo › Reiniciar (F2) | El botón de «reset» de la consola |
| Archivo › Pausa (P) | Para y sigue (también se para sola al minimizar) |
| Tamaño › x1, x2 | La ventana, a la medida de la imagen: 256×240 o el doble. La ventana también se puede estirar: la imagen sale al doble si cabe, centrada |
| Sonido | Lo pone o lo quita |
| Ayuda | Teclas y mandos; Acerca de |

| Consola | Teclado | Mando (HSPJOY) |
|---|---|---|
| Cruceta | Flechas | Cruceta o palanca |
| A | X | Botón 1 (y el 4, si hay más de cuatro) |
| B | Z | Botón 2 (y el 3) |
| Select | Mayúsculas | Botón 3; con más de cuatro botones, el 7 o el 9 |
| Start | Intro | Botón 4; con más de cuatro botones, el 8 o el 10 |

Con `HSPJOY` cargado (docs/MANDOS.md) valen los mandos que haga de jugador
1 y de jugador 2 (USB, puerto de juegos o cable de impresora); el teclado es
siempre el jugador 1. Abajo, la barra de estado dice el cartucho y los
cuadros por segundo que salen (y, entre paréntesis, cuántos se pintan si el
equipo no llega a pintarlos todos).

Los cartuchos **con pila** guardan su partida en un archivo `.SAV` (8 KB) al
lado de la ROM: se lee al abrirla y se escribe sola dos segundos después del
último cambio (o cada treinta, si el juego no para de escribir), al cambiar
de cartucho y al cerrar la ventana.

En los parámetros del acceso directo se puede poner una ROM (se abre al
entrar) y `/1` (tamaño x1 aunque la pantalla sea grande). Para las pruebas:
`/P` (paso a paso: la consola solo avanza con cada tecla, seis cuadros con
el botón y seis sin él; Espacio, treinta cuadros; S hace de Select) y `/T=n`
(n cuadros a toda velocidad y el resultado en `HISPANES.LOG`; con `/V`,
pintando cada uno en la ventana).

## Qué emula

| Parte | Estado |
|---|---|
| Procesador (6502 sin modo decimal) | Todas las instrucciones oficiales y las no oficiales que usan los programas (NOP de varios tamaños, LAX, SAX, DCP, ISB, SLO, RLA, SRE, RRA, ANC, ALR, AXS y el segundo SBC), con sus ciclos, incluidos los de cambio de página. NMI, IRQ, BRK. Las instrucciones que leen, cambian y escriben en el cartucho escriben lo que había (lo que necesitan los registros en serie). Faltan las no oficiales raras (ARR, las «inestables») y el retraso de una instrucción de CLI y PLP |
| Imagen | Por líneas: fondo con desplazamiento en los dos sentidos (registros v, t, x, w), 64 objetos de 8×8 y 8×16 con sus espejos y su prioridad (ocho por línea, con el aviso de «más de ocho»), los 8 puntos de la izquierda tapados o no, y el **aviso del objeto 0 en su punto exacto**. Cambios de desplazamiento a mitad de imagen: valen desde la línea siguiente. Sin hacer: colores reforzados y grises del registro 2001h, y los efectos que cambian algo a mitad de una línea |
| Paleta | Propia: calculada desde la señal de televisión que saca la consola (tools/hispanes_paleta.py). Cada color pasa al más parecido de los 256 de Hispa Ventanas |
| Sonido | Dos canales de pulsos (ciclo, envolvente, barrido), triángulo, ruido y muestras (con su interrupción), secuenciador de cuadros con su interrupción en su ciclo. A 22.050 Hz y 8 bits por HSPSND |
| Mandos | Dos, con su registro de desplazamiento |
| Cartuchos | Los tipos (en iNES, «mapper») **0, 1, 2, 3 y 4**, y además 7 y 66. El 4, con su contador de líneas (cuenta al final de la parte visible de cada línea y cuando se mueve la dirección de la memoria de imagen con la imagen apagada). RAM de 8 KB en 6000h, con pila o sin ella; «trainer» de 512 octetos; tablas de nombres en espejo horizontal, vertical, una sola o las cuatro; dibujos en ROM o en RAM. Hasta 3 MB |

Compatibilidad: con esos siete tipos de cartucho se cubre la mayor parte del
catálogo de la consola (los juegos de los primeros años y casi todos los
más conocidos). **Solo se ha probado con ROM de prueba y con Cometas**: en
el proyecto no hay ni se han usado ROM comerciales. Lo esperable, por cómo
está hecho (por líneas, no punto a punto): los juegos que parten la pantalla
con el objeto 0 o con el contador de líneas van bien; los pocos que cambian
la paleta o el desplazamiento varias veces dentro de una misma línea, o que
dependen del momento exacto de una interrupción, pueden verse con algún
fallo. Con una ROM que pida un cartucho que no está, Hispa-Nes dice su número.

## Cómo está hecho

`programas/hispanes.asm` es lo que lo une a Hispa Ventanas; la consola está
en `programas/hispanes/`, en archivos que no llaman a Hispa Ventanas (por eso
se pueden montar también como programa de Linux para las pruebas):

| Archivo | Qué |
|---|---|
| `cpu.inc` | El procesador. Una rutina por instrucción (hechas con macros: modo de direccionamiento × operación) y una tabla de 256 saltos. A en BL, el acarreo en BH, X e Y en CL y CH, el contador de programa como puntero en ESI, el tiempo que queda en EDI. N y Z se guardan como «el último resultado». La memoria se ve por una tabla de punteros, uno por cada 8 KB |
| `ppu.inc` | La imagen, los registros 2000h-401Fh, los mandos y `nes_cuadro`: las 262 líneas de un cuadro. Cada línea visible se pinta de una vez (33 dibujos de fondo con una tabla que pasa un octeto de plano a ocho puntos; los objetos, del último al primero) y luego corre el procesador sus 341 puntos (3 por ciclo), en dos tramos: en el punto 260 la imagen pasa a la línea siguiente y cuenta el cartucho |
| `apu.inc` | El sonido. El secuenciador avanza en su ciclo exacto (el procesador se para justo ahí); las muestras salen una vez por línea, cada canal con su cuenta de ciclos |
| `mapas.inc` | El formato iNES, encender y reiniciar, y los cartuchos |
| `datos.inc` | Las variables. Empiezan con 4 KB sin usar: escribir en la misma página de memoria en la que hay código que se ejecuta sale carísimo (el procesador, o QEMU, cree que el programa se cambia a sí mismo; sin ese hueco iba 60 veces más lento) |

Lo de Hispa Ventanas (`hispanes.asm`):

- **El reloj.** 60,1 cuadros por segundo. El tic de la BIOS (18,2 por
  segundo) es muy basto: se afina leyendo el contador 0 del 8254 (con la
  orden de «leer atrás», que dice también si está en modo 2 o en modo 3).
  El bucle del programa mira los mensajes, emula un cuadro cuando toca y
  **cede el turno** (`ceder`), sin esperar a un mensaje. Si el equipo no
  llega, no pinta ese cuadro (hasta tres seguidos; el aviso del objeto 0 se
  calcula igual) y, si se queda muy atrás (un diálogo, el disco), vuelve a
  contar desde ahí.
- **La pantalla.** La imagen de la consola (256×240, un octeto por punto
  con su color, 0-63) pasa a los colores de Hispa Ventanas, al doble si
  toca, y se dibuja con `dc_copiar`.
- **Las teclas.** Con `WM_KEYDOWN` no se sabe cuándo se suelta una tecla:
  Hispa Ventanas apunta ahora las teclas pulsadas (`tec_mapa`) y el emulador
  las mira en cada cuadro, si su ventana tiene el foco. En un equipo cuya
  BIOS no avise (INT 15h, AH=4Fh), cada pulsación vale unos cuadros.
- **El sonido.** HSPSND toca trozos, uno detrás de otro (tiene sitio para
  cuatro), y hace esperar a quien le manda uno que no cabe. La ventana no
  espera: lleva la cuenta de cuándo acaba cada trozo y solo manda otro si
  quedan como mucho dos sonando (si no, ese se pierde). El trozo tiene que
  ser de la medida que la tarjeta toca de una vez: 1.024 muestras con una
  Sound Blaster 16 (46 ms; el sonido va unas décimas de segundo por detrás
  de la imagen), hasta 900 con AC'97 y HD Audio. Con una **Sound Blaster de
  8 bits** el trozo entero serían 4.096 muestras, más de lo que HSPSND admite
  de una vez: se oye entrecortado, y por eso ahí Hispa-Nes empieza sin
  sonido (se puede poner en el menú). Arreglarlo es cosa de HSPSND (una
  función que diga cuánto sitio queda y trozos del tamaño que se le dé).

Para esto Hispa Ventanas tiene cuatro entradas más en su interfaz (76; ver
docs/VENTANAS.md): `ceder`, `tec_mapa`, `snd_pcm` y `dos_dir_actual`.

### Velocidad

Un cuadro de Cometas son alrededor de un millón de instrucciones de x86
(procesador, 240 líneas de imagen y sonido): 60 cuadros por segundo piden
unos 60 millones por segundo, lo que da un Pentium de 100 MHz o más. En un
486 rápido no llegará a pintarlos todos: se salta el dibujo de algunos y el
juego sigue a su velocidad (en la barra de estado se ve cuántos pinta).
**No se ha medido en un equipo de verdad**; en QEMU (sin KVM, en el servidor
de pruebas) salen unos 900 cuadros por segundo sin pintar y unos 300
pintando cada uno en la ventana al doble, así que va a sus 60 con holgura.
La prueba apunta lo que da en cada equipo (`/T=600 /V`).

## Los juegos propios

Se escriben en ensamblador de 6502 y se ensamblan en la compilación con
`tools/asm6502.py`, un ensamblador pequeño hecho para esto (dos pasadas;
etiquetas locales con `@`; `.org`, `.pad`, `.byte`, `.word`, `.res`,
`.incbin`, `.include`; y `.tiles`, que deja escribir los dibujos de 8×8 en
texto). Sin dependencias: no hace falta cc65.

**Cometas** (`nes/hispa1/hispa1.s`, `dibujos.s` → `HISPA1.NES`, 24 KB): la
nave va por un campo de estrellas que corre hacia la izquierda y le salen
cometas al paso. Cruceta: mover; A: disparar (tres disparos a la vez);
Start: empezar y pausa. Diez puntos por cometa; cada cien salen más
seguidos; tres vidas. La mejor puntuación se guarda en la RAM con pila del
cartucho (`HISPA1.SAV`). Usa lo que hay que probar: marcador fijo arriba y
estrellas que se desplazan debajo (corte con el objeto 0), objetos de
16×16, las dos tablas de nombres, la NMI, el mando, dos canales de sonido y
la RAM con pila. Con los mismos botones en los mismos cuadros la partida es
siempre igual.

Los siguientes (plataformas, puzles) irán en `nes/` y en `NESROMS` del
Makefile.

## Pruebas

`tests/run_hispanes_tests.sh` (unos seis minutos):

1. **El núcleo, fuera de QEMU** (`tests/hispanes/nesnativo.asm`: los mismos
   `.inc` como programa de Linux de 32 bits, hecho solo con NASM):
   - `CPUTEST.NES` (`tests/hispanes/cputest.s`), nuestra: 24 pruebas de
     procesador, imagen, sonido y mando; deja el resultado en 6000h y la
     pantalla verde o roja;
   - `MAPTESTn.NES` (`tests/hispanes/maptest.s` y `mkmaptest.py`), también
     nuestras, una por cada tipo de cartucho (1, 2, 3, 4, 7 y 66): piden
     cada banco de programa y de dibujos y miran que es el que se ve; el
     espejo de las tablas de nombres en el 1 y el 7; y el contador de líneas
     del 4 (con el tope en 3, 120 interrupciones en dos cuadros);
   - ROM de prueba de libre distribución, que **no van en el proyecto**
     (las baja `tests/thirdparty/fetch_nes.sh`; sin red se saltan):
     `nestest` (8.991 instrucciones, oficiales y no oficiales, comparadas
     una a una con el registro de referencia: registros, banderas y ciclos)
     y doce de las de «blargg» (instrucciones oficiales en un cartucho del
     tipo 1, sonido, retorno vertical y NMI, y el contador de líneas del
     tipo 4);
   - Cometas con las teclas de la prueba: las imágenes que tienen que
     verse, que están guardadas en `tests/hispanes/esperado/`.
2. **En Hispa Ventanas, paso a paso**: se juega con el teclado y cada
   pantalla es, punto por punto, la esperada (al doble y a tamaño normal);
   menús, diálogos, abrir ROM, archivos que no valen, la partida guardada.
3. **A su velocidad**, con una Sound Blaster 16: teclas mantenidas, el
   sonido en la grabación de QEMU y salir con el juego en marcha.
4. **Con un mando**: QEMU no tiene mandos que se puedan mover, así que un
   programa de prueba (`tests/progs/nesjoy.asm`) le entrega a HSPJOY un mando
   con la cruceta a la derecha y el botón 4 pulsado, como hace HSPUSB; la
   partida tiene que empezar sin tocar el teclado.
5. **`/T`**: `CPUTEST.NES` dentro del módulo da lo mismo que fuera, y la
   velocidad en QEMU.

Otras ROM de prueba públicas con las que se ha mirado a mano: pasan las once
del aviso del objeto 0, las tres de tiempos de las ramas y la de tiempos de
las instrucciones oficiales; no pasan las que piden exactitud de un ciclo en
el retorno vertical o en el sonido, las de lecturas y escrituras «de más»
del procesador, ni la de tiempos del contador de líneas.

## Pendiente

- Probarlo en equipos de verdad (un Pentium y un 486) y con cartuchos
  propios volcados; afinar con lo que salga.
- Sonido sin cortes con cualquier tarjeta (hace falta una función nueva en
  HSPSND) y con menos retraso.
- Colores reforzados y grises; efectos a mitad de línea.
- Más cartuchos (los siguientes más usados: 9, 10, 11, 71) y guardar el
  estado de la consola en cualquier momento.
- El segundo mando por teclado y elegir las teclas.
- Hispa-System e Hispa-Boy: el mismo armazón con otra consola dentro.
