# Hispa-Drive

La cuarta de las «consolas de cada época» (docs/VISION.md), después de
Hispa-Nes, Hispa-System e Hispa-Snes: un emulador **propio** de la consola
de 16 bits de 1988, para los cartuchos volcados en archivos `.MD`, `.BIN`,
`.GEN` o `.SMD`. Es un programa de Hispa Ventanas (`HISPADRV.HVP`, grupo
«Juegos») y corre también en **Hispa-Pentium**, con HVPRUN.

- Todo el código está escrito aquí, a partir de la **documentación pública
  del hardware**: el 68000 (el manual del programador del fabricante), el VDP
  de la consola (el sucesor del de la Master System), el YM2612, el bus del
  Z80, los puertos de los mandos y la cabecera de los cartuchos. Sala limpia:
  no lleva nada de otros emuladores (ni de Genesis Plus GX ni de PicoDrive).
- Lo que comparte con Hispa-System está en `programas/comun/`: el **Z80**
  (`z80.inc`) y el **SN76489** (`psg.inc`), los mismos archivos.
- Las **ROM comerciales no van en el proyecto**: quien conserve sus cartuchos
  usa sus propios volcados. Con el emulador va `PRUEBA.MD`, una ROM nuestra
  (la prueba de la consola, que también enseña los botones del mando).
- No se nombra la consola en el programa: en la ayuda es «la consola de 16
  bits de 1988»; «Mega Drive» solo como referencia del formato de los
  archivos.

## Cómo se usa

Se abre desde el grupo «Juegos». Los cartuchos van en su carpeta,
**`\HISPADOS\JUEGOS\MD`** (allí está `PRUEBA.MD`). Al entrar no se abre
ninguno: «Sin cartucho» hasta **Archivo › Abrir ROM**, que empieza en esa
carpeta y luego en la última de la que se abrió uno (`HISPADRV.INI`, al lado
del módulo: `Carpeta=...` y `Mando=3` o `Mando=6`). Como los otros, **empieza
en pausa** y se para solo (callado) con un menú, un diálogo, sin el foco o
minimizado.

| Menú | Qué hace |
|---|---|
| Archivo › Abrir ROM... | Un `.MD`, `.BIN`, `.GEN` o `.SMD` (con la cabecera de 512 octetos de las copiadoras; el `.SMD`, intercalado, se deshace solo) |
| Archivo › La prueba de la consola | Abre `PRUEBA.MD` |
| Archivo › Reiniciar (F2), Pausa o continuar (P) | Como en los otros |
| Tamaño › x1, x2, Pantalla completa (Alt+Intro) | 320×224, el doble, o sin la ventana a la mayor escala entera (Esc vuelve) |
| Mando › de 3 botones / de 6 botones | El mando que ve la consola (se apunta en el `.INI`). Algunos juegos antiguos no van con el de seis: por eso lo normal es el de tres |
| Sonido | Lo pone o lo quita |

| Consola | Teclado | Mando (HSPJOY) |
|---|---|---|
| Cruceta | Flechas | Cruceta o palanca |
| A, B, C | Z, X, C | Botones 1, 2, 3 |
| X, Y, Z (mando de seis) | A, S, D | Botones 4, 5, 6 (con más de cuatro) |
| Start | Intro | 7, 8 o 10 (con cuatro botones o menos: el 4) |
| Mode | M | 9 |

Los cartuchos **con pila** guardan su partida en un archivo `.SRM` (los 64 KB
de su RAM) **al lado de la ROM**: se lee al abrirla, se escribe sola dos
segundos después del último cambio, al cambiar de cartucho y al salir. La
RAM está donde dice la cabecera (`RA` en 1B0h); si no lo dice y la ROM es de
2 MB o menos, también se da en 200000h por si el juego la usa (y solo se
guarda si el juego escribe en ella). Con más de 2 MB, la enciende el juego
en A130F1h.

La **región** sale de la cabecera (1F0h): si el cartucho vale para América,
se presenta como consola americana (NTSC, 60 Hz); si solo para Japón, como
japonesa; si solo para Europa, como europea (PAL, 50 Hz, 313 líneas). Con
`/R=1`, `/R=2` o `/R=3` en los parámetros se fuerza Japón, América o Europa.

Parámetros del acceso directo: una ROM (se abre al entrar), `/6` (el mando
de seis), `/R=n`, `/1` (tamaño x1); para las pruebas, `/P` (paso a paso) y
`/T=n` (n cuadros a toda velocidad y el resultado en `HISPADRV.LOG`; con
`/V`, pintándolos).

### En Hispa-Pentium

Está en `\JUEGOS` de la memoria USB (con `PRUEBA.MD`); se abre desde **Mi
equipo** (un `.HVP` se abre con HVPRUN) o desde la Consola:
`HVPRUN.BIN \JUEGOS\HISPADRV.HVP \JUEGOS\PRUEBA.MD`. Lo nota como Hispa-Snes
(`aj_leer AJ_TXT_VERSION` da 0 en HVPRUN): entonces no toca puertos ni la
interrupción 31h, el reloj es el fino de HVPRUN (`aj_leer 20`) y no hay
sonido (HVPRUN aún no lo tiene) ni HSPJOY. Como el núcleo de Hispa-Pentium no
dice cuándo **se suelta** una tecla (solo la pulsación y sus repeticiones,
a los 0,5 s y luego 25 por segundo), una tecla vale unos 32 cuadros la
primera vez (lo que tarda en repetirse) y 4 en cada repetición: al soltar
una tecla que se acaba de pulsar, el botón sigue pulsado hasta medio
segundo. Pendiente: que el núcleo avise al soltar.

## Qué emula

| Parte | Estado |
|---|---|
| Procesador (68000) | Todas las instrucciones, con sus ciclos (también los de MULU/MULS y DIVU/DIVS, que dependen de los números). Excepciones (TRAP, CHK, división entre 0, privilegio, ilegal, línea A y F), interrupciones con sus vectores automáticos, STOP, las dos pilas. **Pasa 818.231 de las 821.970 pruebas de una instrucción de SingleStepTests** (99,5 %): lo que no coincide es ASR con más desplazamiento que bits y un número negativo (la prueba dice C=X=0; el manual del fabricante dice «el último bit que sale», que es lo que se hace aquí), dos pruebas de ASL.b y una de DIVU que son errores de dirección mal marcados. Sin hacer: los errores de bus y de dirección (los juegos no los usan) y la traza |
| Imagen (VDP) | Los dos planos (32, 64 o 128 casillas), desplazamiento horizontal de pantalla, por filas de 8 o por línea, y vertical de pantalla o por columnas de 16 puntos; la ventana (por arriba, abajo, izquierda o derecha); 80 objetos (64 con 256 puntos) de 1 a 4 casillas de lado, girados, con su lista enlazada, 20 por línea (16) y el límite de puntos, la máscara de x=0 y las banderas de demasiados y de choque; las prioridades de los planos y de los objetos; 320 y 256 puntos de ancho, 224 líneas (240 en PAL); la paleta de 64 colores de 9 bits a los 256 de Hispa Ventanas; las tres DMA (del 68000 a VRAM, CRAM y VSRAM, relleno y copia) con el tiempo que el 68000 se queda quieto; las interrupciones de línea (registro 10) y de cuadro, el estado y el contador H/V. Cada línea se pinta de una vez (después de dejar correr al 68000 el borde de la línea, para que los cambios de la interrupción de línea salgan en su sitio) |
| Sonido FM (YM2612) | Seis canales de cuatro operadores, los ocho algoritmos, la realimentación, la envolvente con sus velocidades y la escala por la nota (las tablas de la documentación del chip), la desafinación y el múltiplo, el LFO de volumen y de tono, el DAC del canal 6, el modo especial del canal 3 y los dos temporizadores con sus avisos. Las muestras salen directamente a la frecuencia de la tarjeta |
| Sonido (SN76489) | El de Hispa-System (`programas/comun/psg.inc`), mezclado con el FM |
| Z80 | El de Hispa-System (pasa zexdoc y zexall), con su RAM de 8 KB, el YM2612, el registro del banco y la ventana de 32 KB al cartucho; el 68000 le pide el bus y lo reinicia; la interrupción del VDP en cada cuadro |
| Mandos | Dos, de tres o de seis botones (la secuencia de TH, que vuelve a empezar tras 1,5 ms quieto), con el teclado y HSPJOY |
| Cartuchos | Hasta 4 MB, con la cabecera de las copiadoras y el formato `.SMD`; la RAM con pila (64 KB, `.SRM`) |

**Sin hacer** (pendiente): las sombras y los brillos del VDP (el modo del
registro 12; los juegos que lo usan salen sin sombras), el modo entrelazado
2 (el de dos jugadores de algún juego de carreras), los cambios a mitad de
una línea, la caché de la tabla de objetos del VDP (se vuelve a leer si se
escribe en ella a mitad de cuadro), la cola del puerto de datos (el tiempo
que tarda cada escritura), el mapeador de los cartuchos de más de 4 MB, las
partidas en EEPROM (unos pocos juegos deportivos), el SSG-EG y el modo CSM
del YM2612, el estéreo, la unidad de CD y el adaptador de 32 bits.

Compatibilidad: **solo se ha probado con las ROM nuestras**: en el proyecto
no hay ni se han usado ROM comerciales. Por cómo está hecho (el 68000
comprobado instrucción a instrucción, el VDP por líneas con sus
interrupciones), lo esperable es que la mayoría de los juegos vayan; los que
usan sombras, el entrelazado o trucos a mitad de línea se verán con fallos.

## Cómo está hecho

`programas/hispadrv.asm` es lo de Hispa Ventanas (la ventana, el menú, el
reloj, las teclas, HSPJOY, HSPSND, los archivos): lo mismo que Hispa-System.
La consola está en `programas/hispadrv/`, en archivos que no llaman a Hispa
Ventanas (por eso se montan también como programa de Linux para las pruebas,
`tests/hispadrv/mdnativo.asm`):

| Archivo | Qué |
|---|---|
| `m68k.inc` | El 68000: la memoria, las banderas, las excepciones y lo largo (MOVEM, BCD, DIV). Los registros, en memoria; el contador de programa en ESI como puntero del equipo (el PC es ESI menos `m_pcbase`); los ciclos que quedan en EDI y el código de la instrucción en EBX. La memoria se guarda **por palabras al revés** (cada palabra en el orden del x86): una palabra se lee de una vez, un octeto está en la dirección XOR 1 y un largo es ROL 16. Se ve por trozos de 64 KB (leer, escribir y ejecutar): la ROM y la RAM directas; lo demás, por rutinas. Las banderas salen de las del x86 (LAHF y SETO): N, Z y C están en el mismo sitio y el acarreo de la resta es el mismo; las 14 condiciones son las del x86 |
| `m68k_gen.inc` | **Generado** en la compilación por `tools/gen68k.py`: para cada uno de los 65.536 códigos se mira qué instrucción es, con su tamaño y sus modos de dirección, y a cada forma distinta (1.668) le toca una rutina en NASM (los números de registro los saca la propia rutina del código); y la tabla de 65.536 saltos, comprimida (8.500 tramos), que se desempaqueta al empezar. Así cada instrucción es un salto por tabla y unas pocas instrucciones del x86, sin descodificar nada al correr |
| `vdp.inc` | Los puertos, las DMA y el dibujo de cada línea, que va **ya en los colores del equipo**: cada casilla, de cuatro en cuatro puntos con tablas (los dos colores de cada octeto de un dibujo en cada paleta y su máscara de transparencia); el plano B de prioridad baja, sin mirar lo de debajo (solo está el fondo); lo de prioridad alta, apuntado y pintado después; los objetos, con una lista de qué objetos tocan cada línea, hecha una vez por cuadro |
| `bus.inc` | El cartucho (la copia por palabras, el `.SMD`, la cabecera, la región), el mapa de memoria del 68000, la RAM con pila, los mandos, el bus y la memoria del Z80 |
| `md.inc` | Un cuadro: 262 líneas (313 en PAL) de 488,57 ciclos del 68000 y 228 del Z80 |
| `ym.inc` | El YM2612; sus tablas (seno, ganancia, envolventes, desafinación) las hace `tools/genym.py` (así el emulador no necesita coma flotante: un 486SX no la tiene) |
| `datos.inc` | Las variables, con 4 KB sin usar delante (como en Hispa-Nes) |

`tools/mkhvp.py` busca ahora los `.inc` también en `build/gen/<programa>/`
(lo generado).

### Velocidad

La consola en sí cuesta poco; lo que cuesta es **pintar**. Medido con
`perf` en el núcleo como programa de Linux (instrucciones del x86 por cuadro,
sin sonido, como en Hispa-Pentium):

| ROM | Pintando cada cuadro | Sin pintar |
|---|---|---|
| `PRUEBA.MD` (un plano lleno, letras, cuatro objetos) | 0,97 millones | 0,19 millones |
| `CARGA.MD` (los dos planos llenos con desplazamiento por línea y por columnas, 80 objetos que se mueven, tres DMA por cuadro, FM en los seis canales) | 2,28 millones | 0,29 millones |

El sonido FM añade 0,2-0,5 millones por cuadro a 22.050 Hz. Un Pentium a 133
MHz hace del orden de 100-130 millones de instrucciones de estas por segundo:
**la consola corre a sus 60 cuadros por segundo con mucha holgura** (0,3
millones por cuadro), y se pintan los 60 en un juego normal (como
`PRUEBA.MD`, unos 100-130 por segundo de margen) y unos 45-55 en el peor
caso de `CARGA.MD` (si no llega, se salta el dibujo de alguno y el juego
sigue a su velocidad: la barra de estado dice cuántos se pintan). En QEMU
(sin KVM), dentro de Hispa Ventanas: `PRUEBA.MD` a 227 cuadros por segundo
pintando al doble; `CARGA.MD` a 185 pintando al doble y 291 sin pintar; en
Hispa-Pentium con HVPRUN, `PRUEBA.MD` a 280 pintando. **No se ha medido en
un Pentium de verdad**: `/T=600 /V` lo apunta en `HISPADRV.LOG`.

## Pruebas

`tests/run_hispadrv_tests.sh` (también en `make test`):

1. **El núcleo, fuera de QEMU**: el 68000 contra las pruebas de una
   instrucción de SingleStepTests (`tests/thirdparty/fetch_md.sh` las baja,
   unos 200 MB; `tests/hispadrv/sst68k.py` las pasa al formato de
   `mdnativo -t`); `PRUEBA.MD` (VRAM, CRAM, VSRAM, las tres DMA, las
   interrupciones de cuadro y de línea, el contador de líneas, el Z80 con su
   programa, su ventana al cartucho y sus interrupciones, los temporizadores
   del YM2612, los mandos de tres y de seis botones con un guion, la RAM del
   cartucho y que se guarda), su pantalla y la nota FM del final (un La de
   440 Hz); `CARGA.MD` y su pantalla; y lo que cuesta cada cuadro.
2. **En Hispa Ventanas**: `/T=600 /V` con `PRUEBA.MD`, todo bien dentro del
   módulo y 60 cuadros por segundo o más al doble; la velocidad con
   `CARGA.MD`.
3. **Con sonido** (Sound Blaster 16): la nota FM suena por HSPSND.
4. **En Hispa-Pentium**: `HVPRUN.BIN HISPADRV.HVP PRUEBA.MD /T=600 /V` desde
   la Consola, todo bien.

Las ROM de prueba (`tests/hispadrv/mdtest.s`, `carga.s` y los programas de
sus Z80, `z80prueba.s` y `z80carga.s`) son nuestras: las del 68000 se
ensamblan con GNU as (`binutils-m68k-linux-gnu`) si está; si no, se usan las
ya hechas (`tests/hispadrv/PRUEBA.MD` y `CARGA.MD`). Las del Z80, con
`tools/asmz80.py`; las letras, las de Hispa-System
(`sms/comun/letras.s`, pasadas por `tools/md_letras.py`).

## Pendiente

- Probarlo en equipos de verdad (un Pentium, el HP Mini) con cartuchos
  propios volcados.
- Las sombras y los brillos, el entrelazado y lo demás de «Sin hacer».
- Que el núcleo de Hispa-Pentium avise al soltar una tecla, y el sonido en
  HVPRUN.
- Guardar el estado de la consola en cualquier momento.
