# Hispa-8086 a dieta

El encargo: «En 8086 hay que ahorrar bits como si no hubiera un mañana».
Hispa-8086 es para máquinas muy justas (8086 y 8088 con 256 a 640 K,
disquetes de 360 K), así que se ha repasado entero para que ocupe menos en
memoria y en el disco **sin quitar ninguna función**. Rama `h8086-adelgaza`
(desde `b0c640b`), 20/11/2026.

## Lo que se ha medido y cómo

- **Los archivos**: lo que mide cada uno en `build/`.
- **El sitio libre** de cada disquete tal como sale de `make` (con todos sus
  archivos), según `mdir`.
- **La memoria residente**: un programa de prueba, `MIDE.COM`
  (`h8086/prueba/mide.asm`), dice dónde empieza el prefijo del intérprete
  (todo lo de delante es del núcleo, más la tabla de vectores y lo de la
  BIOS, 1.792 octetos), dónde empieza el suyo (lo de en medio es lo que deja
  el intérprete mientras corre un programa, con los entornos) y el bloque
  libre mayor. En QEMU (486, 640 K de memoria convencional), sin
  CONFIG.SYS, con `DOS=ALTO` y con `PROCESADOR=8086` (como un 8086: sin
  gestor de memoria extendida ni HMA).

La batería lo comprueba desde ahora (parte `adelgaza`, en
`h8086/prueba/partes_12.sh`): el núcleo deja menos de 20.000 octetos y el
intérprete menos de 5.000.

## Antes y después

### En memoria

| | antes | después | ahorro |
|---|---:|---:|---:|
| Núcleo, abajo, en un 8086 (o con `PROCESADOR=8086`) | 21.808 | **18.064** | 3.744 |
| Núcleo, abajo, en un 286 o más (con el gestor de memoria extendida) | 21.808 | **19.248** | 2.560 |
| Núcleo con `DOS=ALTO` (lo que queda abajo: el puente) | 1.056 | 1.056 | 0 |
| Intérprete mientras corre un programa (con los entornos) | 14.256 | **4.416** | 9.840 |
| Libre para un programa, 8086 | 616.464 | **630.048** | 13.584 |
| Libre para un programa, 286 sin `DOS=ALTO` | 616.464 | **628.864** | 12.400 |
| Libre para un programa, `DOS=ALTO` | 637.216 | **647.056** | 9.840 |
| DISCORAM sin `/E`: lo que ocupa además del disco | 1.168 | **416** | 752 |

(En octetos. «Libre» es el bloque mayor que tiene un programa recién
lanzado; en un PC de 256 K, los 13 K de más son un 6 % de la memoria.)

EQUIPO, al arrancar, dice ahora 614 K libres para los programas en vez de
602 K (en QEMU, sin CONFIG.SYS).

### En el disco

| archivo | antes | después |
|---|---:|---:|
| NUCLEO86.SYS | 15.316 | 14.992 |
| COMMAND.COM | 10.603 | 11.016 |
| BASIC.COM | 27.333 | 27.128 |
| EDITAR.COM | 2.099 | 2.043 |
| FORMATEA.COM | 5.873 | 5.578 |
| SISTEMA.COM | 3.217 | 2.922 |
| PARTIR.COM | 3.223 | 2.970 |
| DISCORAM.SYS | 1.159 | 1.210 |
| las demás órdenes | igual | igual |

| disquete | libre antes | libre después | ahorro |
|---|---:|---:|---:|
| 360 K | 268.288 | **271.360** | 3.072 |
| 720 K | 635.904 | **638.976** | 3.072 |
| 1,2 M | 1.125.376 | **1.127.424** | 2.048 |
| 1,44 M | 1.369.088 | **1.371.136** | 2.048 |

En el disco se gana por grupos enteros (1 K en los de 360 y 720 K, 512
octetos en los de 1,2 y 1,44 M): un archivo solo ahorra sitio si baja de un
grupo. Por eso algunos recortes se han buscado justo hasta ahí (EDITAR, de
2.099 a 2.043: un grupo menos en todos los disquetes). COMMAND.COM crece
413 octetos (lo que hace falta para volver a leer su parte transitoria),
pero es el que más memoria devuelve.

## Qué se ha hecho

### El núcleo: el arranque se olvida (como el INIT de Hispa-DOS)

Lo que solo se usa al arrancar (reconocer el procesador, poner los
vectores, buscar las unidades, leer CONFIG.SYS y cargar los DEVICE=, el
saludo, poner en marcha el gestor de memoria extendida y subir a la HMA) va
en su sección, `.init` (`h8086/inicio.inc`, y `config.inc` entero), detrás
de lo residente en el archivo. El puente salta allí; lo primero que hace es
copiarse a lo alto de la memoria convencional y seguir desde allí con su
propio segmento, mientras DS, ES y SS son los del núcleo. Así las variables
del núcleo, que en memoria van justo detrás de lo residente (donde estaba el
arranque en el archivo), se pueden poner a cero, y al acabar esa memoria es
de los programas. Desde el arranque, las rutinas del núcleo se llaman con
`k_nombre`, que pasa por `lejos` (una llamada lejana que acaba en una
cercana); los controladores de DEVICE= no llegan a lo alto (siempre dejan
64 K), así que no lo pisan. **2,2 K menos.**

### El núcleo: el gestor de memoria extendida, solo si hace falta

El gestor XMS (INT 2Fh y sus 16 funciones) no sirve de nada en un 8086,
que no tiene memoria extendida. Va en su sección, `.xms`, detrás del
arranque; config.inc lo copia detrás de la reserva de sectores solo si hay
un 286 con memoria extendida (y entonces, con `DOS=ALTO`, sube a la HMA con
lo demás). Para poder ir donde caiga se ensambla desde 0: sus saltos son
relativos y su tabla de funciones se suma a donde está. **1,2 K menos en un
8086.**

### El núcleo: lo que eran ceros en el archivo

El sector por el que pasan los discos con el núcleo en la HMA y la tabla de
INT 15h/87h (560 octetos de ceros) ya no van en el archivo: con el núcleo
arriba usan el sitio que queda libre abajo, y abajo la tabla va con las
variables (y el sector no hace falta). **512 octetos menos en memoria (sin
`DOS=ALTO`) y 560 en el disco.**

### El intérprete: parte residente y parte transitoria (como COMMAND.COM)

Antes el intérprete entero (10,6 K de código y textos, más 3,4 K de
variables y pila) se quedaba en memoria mientras corría cualquier programa.
Ahora va en dos partes:

- **La residente** (unos 3,7 K con el prefijo): las variables, la pila,
  unas tablas pequeñas y lo que lanza los programas y trae la otra parte.
- **La transitoria** (el código de las órdenes y todos los textos, unos
  10 K): corre en lo alto de la memoria, con su propio segmento. Al lanzar
  un programa se suelta; al volver se pide otra vez y se mira su suma: si
  el programa no la ha pisado (lo normal: un programa usa la memoria de
  abajo), se sigue sin más; si la ha pisado, se vuelve a leer de
  COMMAND.COM (el de la variable COMSPEC).

Los textos de la parte transitoria se escriben con `pon_cad`, que los lee
de CS si su dirección es de las de esa parte (desde `T_BASE`). Las tablas de
las órdenes de dentro y de los nombres clásicos (FORMAT es FORMATEA...), que
antes eran punteros a nombres, van ahora seguidas (el nombre y la rutina),
también en la parte transitoria. **9,8 K menos mientras corre un
programa.**

### Las órdenes

- **FORMATEA, SISTEMA y PARTIR** llevaban dentro el sector de particiones
  (446 octetos) y el de arranque (512), con sus ceros del final. Ahora solo
  llevan lo que no es cero (lo mide el Makefile, `sectores.inc`) y el resto
  lo ponen a cero al empezar. SISTEMA y PARTIR bajan de 3 K: un grupo menos
  en los disquetes de 360 K.
- **EDITAR**: rutinas comunes en vez de código repetido (el cursor y la
  tecla, el texto de la línea de abajo, una pantalla arriba o abajo, «hay
  cambios»...). 56 octetos menos: un grupo menos en todos los disquetes.
- **DISCORAM**, sin `/E`, ponía el disco detrás de todo el controlador,
  también de lo que solo sirve para arrancar (leer los parámetros, dar
  formato, los mensajes). Ahora el disco va justo detrás de lo residente:
  el formato se prepara detrás y se baja a su sitio al acabar. El archivo
  crece 51 octetos; en memoria, 752 menos.

### Hispa-BASIC

Rutinas comunes para lo que más se repetía (copiar el acumulador, la
multiplicación de la serie, el final de muchas rutinas, el estado del
tokenizador, que va en tres octetos seguidos y se pone con una palabra), y
llamadas al final cambiadas por saltos. 205 octetos menos: un grupo menos
en los disquetes de 1,2 y 1,44 M. El intérprete da exactamente lo mismo
(la comparación con GW-BASIC sigue igual).

## Lo que el usuario puede notar

- **Más memoria libre**: EQUIPO y MEMORIA lo dicen.
- **«Ponga el disquete con COMMAND.COM y pulse una tecla...»**: si un
  programa pisa la parte transitoria del intérprete (uno que use toda la
  memoria, o en una máquina con muy poca) y en la unidad no está el
  disquete con COMMAND.COM, el intérprete lo pide al volver, como MS-DOS.
  Con el disquete puesto, sigue donde estaba (en medio de un lote también).
  Si se pulsa la tecla enseguida, el núcleo puede no haberse dado cuenta
  aún del cambio de disquete (lo mira cada dos segundos): lo vuelve a pedir.
- Nada más: las órdenes, los mensajes, CONFIG.SYS, los lotes, BASIC y los
  programas hacen y dicen lo mismo que antes.

## Lo que no se ha tocado, y por qué

- **Ninguna instrucción posterior al 8086** (todo sigue con `cpu 8086`; el
  único tramo de 386 es la prueba que distingue el 386 del 486). Nada
  depende de la velocidad. Las copias con prefijo de segmento van sin
  `REP` (el 8086 pierde el prefijo si una interrupción corta un `REP` con
  dos prefijos).
- **Los textos** no se han acortado: son lo que ve el usuario (y la
  batería los busca). Los largos y poco usados (AYUDA, ACTUALIZAR, el
  consejo de arranque) ya no están residentes: van en la parte transitoria,
  así que no hacía falta sacarlos a un archivo de mensajes.
- **Lo configurable** (FILES=, BUFFERS=, el historial de la línea de
  órdenes, el tamaño del entorno, las pilas) mide lo mismo: cambiarlo sería
  cambiar lo que hace el sistema.
- Las demás órdenes (REVISAR, PASAR, XCOPIAR...) ya estaban apretadas: lo
  que se les podría quitar no llega a ahorrar un grupo del disco, y en
  memoria solo están mientras corren.

## Las pruebas

`make prueba` entero después de cada pieza, también con `MODO=alto` (el
núcleo en la HMA) y `MODO=8086` (como un 8086). Las partes del emulador v86
(las de la web) ya fallaban antes de empezar en este VPS (la página tiene
encima una capa que no deja hacer clic): no tienen que ver con esto. Las
ROM originales del IBM PC, XT y AT (PCjs) no están en el VPS: queda
pasarlo por ellas, como se hizo con la 0.7 y la 0.8.
