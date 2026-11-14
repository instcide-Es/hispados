# El mezclador por fuentes: HSPSND y el Mezclador de Hispa Ventanas

Primera pieza del grupo **«Sonido»** de Hispa Ventanas. Diseño propio, de sala
limpia: la tabla de registros del CT1745/CT1345 de la Sound Blaster, la del
AC'97 y la del HD Audio son documentación pública; los nombres, los textos,
el aspecto y el formato del fichero son de Hispa-DOS.

Lo que hay:

- **HSPSND** (versión 4 de su interfaz) lleva el volumen, el silencio y el
  balance de siete **fuentes**, las casillas «Sonar» y «Grabar» y los ajustes
  (graves, agudos, ganancias, ajuste automático y refuerzo del micrófono), y
  los traduce a cada tarjeta (`lib/hspmez.inc`).
- **Un solo fichero de estado**: la sección `[Mezclador]` de
  `\HISPADOS\SONIDO.INI`, el mismo fichero en el que AUDICION apunta lo que
  ha aprendido. Se guarda siempre, sin preguntar; HSPSND lo pone al cargarse.
- **MEZCLAD.HVP «Mezclador»**, en el grupo nuevo **«Sonido»** del Centro de
  programas (con el Reproductor de sonido). Se abre también con **doble clic
  en el altavoz** de la barra de arriba y con el botón **«Mezclador...»** de
  su ventanita y de Configuración > Sonido.
- **Configuración > Sonido** dice la tarjeta y si está la **Sound Blaster
  para juegos** (`HSPSND /SB`).

## Las fuentes

| N.º | Fuente | Qué es | Balance | Casillas |
|---|---|---|---|---|
| 0 | General | todo lo que sale (el volumen de siempre: funciones 05h-07h, el altavoz de la barra, Ctrl+Alt+flechas, `SONIDO`) | sí | Silencio |
| 1 | Sonidos | la onda: lo que se toca con la función 01h (SONAR, el Reproductor, la campanita, Hispa-Nes) y la voz digital de la Sound Blaster emulada | sí | Silencio |
| 2 | Música | la FM (OPL de la Sound Blaster; con `/SB`, la FM emulada) | sí | Silencio, Grabar (SB16) |
| 3 | CD | la entrada analógica del CD | sí | Silencio, Sonar, Grabar |
| 4 | Línea | la entrada de línea | sí | Silencio, Sonar, Grabar |
| 5 | Micrófono | el micrófono (un canal) | no | Silencio, Sonar, Grabar |
| 6 | Altavoz | el altavoz del PC, si pasa por la tarjeta (un canal) | no | Silencio |

- **Escala única de 0 a 100 %** en pantalla y en el fichero; el balance, de 0
  a 100 (50 en medio; a un lado, el otro canal baja en proporción).
- **La curva** es la de siempre de HSPSND: 100 % = 0 dB y cada punto menos,
  0,47 dB menos (46,5 dB menos en el 1 %); el 0 % es silencio.
- **«Sonar»**: la entrada pasa a los altavoces. **«Grabar»**: es fuente de
  grabación. Una fuente en silencio ni suena ni se graba.
- **Valores de fábrica**: General 80, Sonidos 90, Música 85, CD 85 (suena),
  Línea 85 y Micrófono 85 **sin sonar** (para que no se acoplen), Altavoz 50;
  se graba del micrófono; graves y agudos en medio; ganancias x1; ajuste
  automático del micrófono puesto; sin refuerzo.

## Cómo lo lleva cada tarjeta

| Tarjeta | General | Sonidos | Música | CD, Línea, Micrófono | Altavoz | Tonos y demás |
|---|---|---|---|---|---|---|
| Sound Blaster 16 (CT1745) | 30h/31h | 32h/33h | 34h/35h | 36h-39h, 3Ah; «Sonar» en 3Ch, «Grabar» en 3Dh/3Eh | 3Bh (4 pasos) | graves 46h/47h, agudos 44h/45h, ganancias 3Fh-42h, ajuste del micrófono 43h |
| Sound Blaster Pro (CT1345) | 22h | 04h | 26h | 28h, 2Eh (sin «Sonar», a 0), 0Ah; graba de una (0Ch) | — | — |
| Sound Blaster 2.0 | por programa | por programa | 06h si tiene el CT1335 | CD en 08h si tiene el CT1335 | — | — |
| AC'97 | 02h y 04h | 18h (PCM) | por programa (solo con `/SB`) | 12h, 10h, 0Eh (silencio = «Sonar» quitado); graba de una (1Ah) | 0Ah | ganancia de grabación 1Ch, refuerzo del micrófono (0Eh bit 6), tonos 08h si los tiene |
| HD Audio | amplificadores del códec del camino (o por programa) | por programa | por programa (solo con `/SB`) | entradas del mezclador del códec que vienen de un pin de CD, línea o micrófono, si las hay | — | — |

- **SB16**: los volúmenes son de 5 bits (pasos de 2 dB): `31 - (atenuación + 1 dB) / 2 dB`,
  en los bits 7-3. Los tonos, 4 bits (8 = sin cambio). El altavoz, 2 bits
  repartidos por igual (50 % = −6 dB). 43h bit 0 = 0 con el ajuste automático.
- **AC'97**: 1,5 dB por paso; los registros con ganancia (PCM, CD, línea,
  micrófono) tienen el 0 dB en 08h. Al cargarse, HSPSND mira qué registros
  tiene el códec (los que no existen se leen como 0): solo salen esas fuentes.
  (El AC'97 de QEMU no guarda CD, línea, micrófono ni altavoz: allí solo salen
  General y Sonidos.)
- **HD Audio**: General, como siempre, en los amplificadores del camino de
  salida, ahora cada canal por su lado (el balance). Sonidos y Música se
  multiplican por programa en la mezcla de HSPSND. Al instalarse, HSPSND busca
  en los mezcladores (nodos de tipo 2) de los caminos que usa las entradas
  que vienen de un pin de entrada con dispositivo CD, entrada de línea o
  micrófono (y conexión física), pone el pin como entrada (el micrófono con
  su alimentación) y lleva su volumen y «Sonar» en el amplificador de esa
  entrada del mezclador. Con HD Audio, HSPSND no elige de dónde se graba (no
  graba): «Grabar» no sale.
  Un cambio de General (05h, 07h, las teclas de volumen, SONIDO, la barra de
  Ventanas) son solo dos órdenes al códec (el amplificador, izquierda y
  derecha); las entradas del mezclador del códec se escriben solo cuando cambia
  algo que no es General (0Bh, 0Dh, 0Eh). Cada orden espera la respuesta como
  mucho de 1 a 2 ms, medidos con el temporizador (antes eran 0x100000
  vueltas: casi un segundo en un equipo real por cada orden sin respuesta, y
  Ventanas se quedaba parada dos segundos al mover el volumen). Si la
  interfaz de órdenes se queda ocupada, se limpia; un nodo que no contesta
  dos veces seguidas ya no se usa (hasta 0Eh con AL=1) y, si es el del
  volumen, General pasa a hacerse por programa.
- **Hispa Ventanas**, mientras se arrastra la barra del volumen, se lo da a
  HSPSND como mucho cada dos vueltas del reloj (55-110 ms) y lo último al
  soltar: la barra y el ratón siguen al momento.
- **Sound Blaster emulada** (`/SB`, docs/HSPSB.md): lo que el juego escribe en
  el mezclador emulado (22h, 04h, 26h, 30h-35h) **se multiplica** por lo del
  usuario (Sonidos para la voz digital, Música para la FM, General si va por
  programa): ninguno pisa al otro. El juego lee lo suyo; el Mezclador, lo del
  usuario.
- Con una **Sound Blaster de verdad**, un juego que escribe en el CT1745 sí
  cambia los registros (es la tarjeta): el Mezclador lo enseña y, si se toca
  algo, HSPSND vuelve a escribirlo todo (función 0Eh, AL=1).

## La interfaz de HSPSND (versión 4)

INT 2Fh AX=D700h → ES:BX = punto de entrada (llamada lejana, AH = función).
Las funciones 00h-08h siguen igual; **la 00h devuelve BX = 4**.

| AH | Entrada | Salida |
|---|---|---|
| 09h controles | — | AL = fuentes que hay (bit n = fuente n), AH = las que lleva la tarjeta (las demás, por programa); BX = ajustes que hay: bit 0 balance, 1 graves, 2 agudos, 3 ganancia de entrada, 4 ganancia de salida, 5 ajuste automático del micrófono, 6 se graba de una sola fuente a la vez, 7 el altavoz en cuatro pasos, 8 refuerzo del micrófono; CX = fuentes que se pueden grabar; DX = fuentes con «Sonar» |
| 0Ah leer fuente | AL = fuente | AL = volumen, AH = casillas (bit 0 silencio, bit 1 «Sonar», bit 2 «Grabar»), BL = balance |
| 0Bh poner fuente | AL = fuente, BL = volumen, BH = casillas, CL = balance (FFh: no se cambia) | como 0Ah. Si la tarjeta graba de una sola fuente, marcar «Grabar» quita las demás |
| 0Ch leer ajuste | AL = ajuste: 0 graves, 1 agudos (0-100, 50 en medio), 2 ganancia de entrada, 3 de salida (0-3: x1, x2, x4, x8), 4 ajuste automático del micrófono, 5 refuerzo del micrófono (0/1) | AL = valor |
| 0Dh poner ajuste | AL = ajuste, BL = valor | AL = valor |
| 0Eh | AL = 0: todo a los valores de fábrica; AL = 1: volver a escribirlo todo en la tarjeta | — |
| 0Fh cambios | AL = 0: ¿hay cambios sin guardar? AL = 1: ya están guardados; AL = 2: los hay | AL = 1 si los hay |

CF = 1 si la fuente o el ajuste no existen. Todo se aplica al salir de la
llamada (como el volumen). HSPSND apunta que hay **cambios sin guardar** con
cualquier cambio (las funciones 04h-07h y 0Bh-0Eh y las teclas de volumen):
así se guarda lo cambie quien lo cambie. `HSPSND /INFO` enseña el mezclador
(«Mezclador: General 80 %, Sonidos 90 %, ...»).

En Hispa Ventanas, los módulos llaman a estas funciones con la entrada
**`snd_mezcla`** de la interfaz (la 82: EAX, EBX, ECX y EDX van y vuelven; si
puede haber cambiado General, Hispa Ventanas vuelve a leer el volumen y
repinta el altavoz) y abren el Mezclador con **`mez_abrir`** (la 83; EAX=1:
en la vista Completa). `ventanas/hvpmez.inc` tiene lo común: leer todo,
poner una fuente o un ajuste y guardar.

## El fichero: `[Mezclador]` en `\HISPADOS\SONIDO.INI`

```
[Mezclador]
; Lo guarda Hispa Ventanas cada vez que cambia algo y HSPSND lo pone al
; cargarse. Volumen y balance de 0 a 100 (balance: 50 = en medio).
General=80,50
Sonidos=90,50
Musica=85,50
CD=85,50
Linea=85,50
Microfono=85,50
Altavoz=50,50
Silencio=
Sonar=CD
Grabar=Microfono
Graves=50
Agudos=50
GananciaEntrada=0
GananciaSalida=0
AjusteMicrofono=1
RefuerzoMicrofono=0
Vista=completa
```

- Texto CP437 con CR+LF, como el resto de SONIDO.INI (docs/AUDICION.md). Las
  claves, sin acentos y sin importar las mayúsculas; sin espacios antes del
  `=`.
- `Fuente=volumen,balance` (el balance se puede omitir). `Silencio`, `Sonar` y
  `Grabar`: la lista de las fuentes que tienen esa casilla, separadas por
  comas (vacía: ninguna; si la clave no está, quedan las de fábrica). Las
  claves que falten o se salgan de sus límites se quedan como de fábrica.
  `Vista` (`sencilla` o `completa`) solo la usa el Mezclador.
- **Quién lo escribe**: Hispa Ventanas. Configuración (que está siempre en
  marcha, con la ventana oculta) mira cada segundo si HSPSND tiene cambios
  sin guardar (función 0Fh) y, si los tiene, escribe la sección; el Mezclador
  hace lo mismo y al cerrarse. La sección se reemplaza en su sitio; si no
  está, va detrás de los comentarios del principio. Lo demás del fichero (lo
  aprendido en la prueba de sonido) no se toca, y AUDICION tampoco toca
  `[Mezclador]` cuando reescribe sus secciones.
- **Quién lo lee**: HSPSND al cargarse (de su unidad o de la de arranque,
  como lo aprendido, los primeros 8 KB; también con `/NOAPRENDIDO`, que solo
  quita lo aprendido). `/VOL:n` manda sobre `General`.
- Fuera de Hispa Ventanas (las teclas de volumen, `SONIDO`) el cambio queda
  apuntado en HSPSND y se guarda la próxima vez que se entra en Hispa
  Ventanas.

## El Mezclador (MEZCLAD.HVP)

- **Vista Completa**: una columna por fuente que tenga la tarjeta, con su
  nombre, el tanto por ciento, la **guía vertical** con su rombo y, si es
  estéreo, el **balance** (una raya con I y D). Debajo, las casillas con texto:
  **«Silencio»** en todas y **«Sonar»** y **«Grabar»** donde la tarjeta las
  tiene (nada se dice solo con colores: en silencio, además de la casilla, el
  tanto por ciento y la guía salen en gris). Los **graves y agudos**, en una
  fila aparte, solo si la tarjeta los tiene. Y **una frase** que dice qué falta
  y por qué («Graves y agudos no salen: esta tarjeta no los tiene.», «Música
  (FM) solo sale con la Sound Blaster para juegos (HSPSND /SB).», ...).
- **Vista Sencilla**: solo General (con su balance y su silencio).
- **Avanzado...**: ganancia de entrada y de salida (un botón que pasa de x1 a
  x2, x4 y x8), ajuste automático del micrófono y refuerzo del micrófono
  (+20 dB), lo que tenga la tarjeta.
- **De fábrica** (pregunta antes).
- **Teclado**: Tab pasa por las columnas y sus casillas; en una columna,
  flechas arriba y abajo (de 5 en 5), RePág y AvPág (de 10 en 10), Inicio
  (100) y Fin (0); izquierda y derecha, el balance. Espacio marca la casilla
  que tiene el foco (en una columna, su silencio); **M** silencio, **S**
  «Sonar» y **G** «Grabar» de la columna en la que se está. Esc cierra.
- **Ratón**: arrastrar el rombo o hacer clic en la guía; doble clic en el
  balance lo deja en medio. Al soltar General o Sonidos suena la campanita.
- **Ayuda > Cómo se usa...**: un recorrido de ocho pasos (qué es, la columna
  General, mover el volumen, el balance, el silencio, «Sonar» y «Grabar», lo
  que no sale, vistas, Avanzado y guardar), con «< Anterior», «Siguiente >» e
  Intro, que **marca con un recuadro en la ventana del Mezclador** el control
  del que habla. Ratón y teclado, siempre explicados.
- Si HSPSND no está (o es anterior a la versión 4), lo dice y no hay más.

## Pruebas

`tests/run_mezclador_tests.sh` (con `make emmprogs` para MEZTEST y SBTEST):

- **(a) Sound Blaster 16**: el Mezclador se abre con doble clic en el altavoz;
  con el teclado y el ratón se mueven General, Sonidos, su balance, el
  silencio de Música, «Grabar» del CD, «Sonar» de la línea, el altavoz, la
  ganancia y el ajuste del micrófono; `MEZTEST C` lee los registros 30h-47h
  del CT1745 y se comparan, uno a uno, con lo que sale de la cuenta. También
  con las órdenes de MEZTEST (la interfaz) y los valores de fábrica.
- **(b) HD Audio y AC'97**: en la grabación de QEMU, Sonidos al 30 % suena más
  bajo que al 100 %; el balance a la izquierda calla el canal derecho; Sonidos
  o General en silencio no suenan. Con `/SB`, la FM de SBTEST no suena con la
  Música en silencio y suena sin él.
- **(c)** SONIDO.INI queda con lo movido y, al arrancar otra vez, HSPSND lo
  pone (y los registros cuadran); `/VOL:n` manda sobre General; con secciones
  de AUDICION delante, también.
- **(d)** Lo que un juego escribe en el mezclador emulado se queda allí
  (`MEZTEST P`) y no cambia lo del usuario.
- Y Configuración > Sonido, el grupo «Sonido» y la frase de lo que falta con
  HD Audio. Capturas en `build/mezclador_*.png`.

`tests/run_volumen_tests.sh` (VOLTIME, `make emmprogs`): lo que tarda un cambio
de volumen. (a) 50 llamadas a 05h y 50 teclas de volumen con HD Audio, AC'97 y
Sound Blaster 16: decenas de microsegundos cada una y, con HD Audio, dos
órdenes por cambio. (b) Con un nodo que no contesta (`HSPSND /SIMSORDO:nn`,
solo para pruebas) el primer cambio tarda unos milisegundos (antes, en QEMU,
0,6-0,7 s cada uno) y los demás nada, y el volumen sigue funcionando por
programa (en la grabación). (c) En Ventanas, con ese códec, se arrastra la
barra y al soltar ya dice el valor nuevo (`build/volumen_arrastre.png`).
