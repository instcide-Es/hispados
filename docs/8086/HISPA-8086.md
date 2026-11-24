# Hispa-8086

*El sistema de la familia Hispa para los PC, XT y AT · noviembre de 2026*
*Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.*

Hispa-8086 es el miembro de la familia **Hispa-DOS** (proyecto
[Rescoldo](../VISION.md)) para las máquinas más veteranas, y hermano pequeño
de Hispa-386: las de **8086, 8088, V20, 80186 y 286**. Hecho
desde cero, en español, libre y todo en ensamblador, para que un PC o un XT de
256 K vuelva a servir: escribir, hacer cuentas, programar en BASIC y pasar los
archivos a un ordenador de hoy.

Este documento es el plan y el registro de las decisiones. El estudio en que
se apoya (qué se usaba en estas máquinas y qué se echa de menos) está en
[ESTUDIO.md](ESTUDIO.md).

## Dónde estamos

**Etapas 0 a 4 y 6 hechas, y la 5 casi** (`h8086/`, noviembre de 2026).
Versión 0.13.

**Lo nuevo en la 0.13**:

- **El disquete de Oficina**, con dos programas para trabajar: **PLUMA**,
  un procesador de textos (cartas y documentos con negrita, subrayado y
  cursiva, párrafos centrados o justificados, y documentos más grandes que
  la memoria), y **CUENTAS**, una hoja de cálculo (sumas que no fallan con
  los céntimos, fórmulas como las de hoy y como las de VisiCalc y 1-2-3, y
  abre y guarda .CSV).
- **El disquete de Juegos**, con dos juegos nuestros: **DERRIBO**, de pala,
  pelota y ladrillos, y **MARCIANADA**, de marcianitos. Van igual de
  deprisa en un PC de 1981 que en uno de hoy y se ven bien en un monitor
  de un solo color.
- **Un arreglo del disco**: al escribir un archivo grande, en cuanto la
  lista de los grupos del disco (la FAT) pasaba de un sector al siguiente,
  el sistema leía el sector que no era; la copia quedaba mal desde la
  mitad o el archivo se cortaba. Ya no pasa, y si falla la escritura de la
  FAT, ahora se avisa.

- **Arranque, de disquete o de disco duro**: un solo sector de arranque para
  FAT12 y FAT16, disquetes y particiones, y un sector de particiones (MBR)
  propio. El núcleo ocupa 14,9 K en el disco. En memoria, el núcleo con
  sus variables, su pila, la tabla de archivos y la reserva de sectores son
  unos 17 K, y el intérprete de órdenes con su entorno, otros
  13 K: en una máquina de 640 K quedan 607 K para los
  programas, y en una de 256 K, 224 K (esto último, echando la
  cuenta). Todo ensamblado con `cpu 8086`: el ensamblador rechaza cualquier
  instrucción posterior. Disquetes de 160, 180, 320 y 360 K, 720 K, 1,2 M y
  1,44 M.
- **El equipo**: reconoce el procesador (8086/8088, V20/V30, 80186, 286, 386,
  486 o más) y la memoria; si la máquina puede con un Hispa mayor, lo
  aconseja.
- **CONFIG.SYS** al arrancar: `FILES=`, `BUFFERS=`, `DEVICE=`, `SHELL=`,
  `DOS=ALTO`/`DOS=BAJO`, `PROCESADOR=8086` y `REM` (ver «CONFIG.SYS», más
  abajo).
- **En un 286 (o más) con memoria por encima del mega** (desde la 0.10): un
  gestor de memoria extendida **XMS 2.0** dentro del núcleo, la puerta A20
  abierta y comprobada de verdad, y **`DOS=ALTO`**, que sube el núcleo entero
  a la memoria alta (HMA) y deja unos 19 K más de memoria convencional;
  DISCORAM puede ir en la extendida (`/E`). Ver «El 286: memoria extendida,
  A20 y HMA».
- **INT 21h**, lo que un programa de DOS espera encontrar:
  - consola (01h, 02h, 06h a 0Bh), vectores (25h, 35h), versión (30h),
    terminar (00h, 4Ch, 4Dh, INT 20h) y **terminar quedándose residente**
    (31h e INT 27h);
  - **archivos**: crear (3Ch), abrir (3Dh), cerrar (3Eh), leer (3Fh),
    escribir y cortar (40h), borrar (41h), moverse (42h), atributos (43h),
    qué hay detrás de un número de archivo (44h con AL=0), renombrar y mover
    a otro directorio (56h), la fecha y la hora de un archivo (57h), buscar
    con comodines (4Eh, 4Fh, 1Ah, 2Fh);
  - **directorios**: crear (39h), quitar (3Ah), cambiar (3Bh), cuál es (47h);
  - sitio libre (36h), fecha y hora, **y ponerlas** (2Ah a 2Dh), verificar
    lo que se escribe (2Eh, 54h), volcar al disco (0Dh);
  - **memoria por bloques**, como en MS-DOS: pedir (48h), soltar (49h),
    cambiar el tamaño (4Ah);
  - **programas**: lanzar otro y esperar (4Bh), su código de salida (4Dh),
    qué programa corre (50h, 51h, 62h).
- **INT 25h e INT 26h**: leer y escribir sectores «en crudo», como en DOS.
- **Dispositivos**, como en DOS: NUL, CON, AUX, PRN, COM1 y LPT1 se abren
  como archivos (`COPIAR carta.txt PRN`, `> NUL`, `COPIAR CON nota.txt`), y
  **`DEVICE=`** carga controladores de carácter y de bloque; **DISCORAM.SYS**
  es un disco en memoria (ver «Dispositivos»).
- **Disco**: A: y B: (disquetes; con una sola disquetera, **B: es la misma
  con otro disquete** y el sistema pide el cambio), **C:**, el disco duro (la
  primera partición FAT del primer disco) y D: y E: para los controladores;
  FAT12 y **FAT16** de hasta 32 megas, de lectura y escritura, con
  subdirectorios que crecen. **Una reserva de sectores** (`BUFFERS=`) evita
  releer la FAT y los directorios. Detecta el cambio de disquete (un XT no
  avisa: si no hay archivos abiertos y pasan dos segundos, vuelve a leer sus
  medidas). Lee también los disquetes de DOS 1, que no llevan sus medidas en
  el primer sector.
- **Programas .COM y .EXE** (con su tabla de reubicación), cada uno con su
  prefijo (PSP), sus parámetros y su entorno. Un programa puede lanzar otro;
  al terminar se cierran sus archivos y se suelta su memoria.
- **El intérprete de órdenes es un programa aparte**, COMMAND.COM
  (9,6 K), con las órdenes de MS-DOS 3.3 en español y con su nombre
  clásico (ver «Las órdenes»), variables de entorno, ruta de búsqueda
  (`PATH`) e indicador configurable.
- **Archivos por lotes** (.BAT) con ECO, REM, PAUSA, %0 a %9, **SI** (IF),
  **IR A** (GOTO), **PARA** (FOR), **LLAMAR** (CALL), **DESPLAZAR** (SHIFT)
  y **AUTOEXEC.BAT** al arrancar; redirección (`>`, `>>`, `<`) y tuberías
  (`|`).
- **Programas del disquete**: EDITAR (un editor de pantalla completa),
  FORMATEA, SISTEMA, PARTIR, REVISAR, ETIQUETA, ATRIBUTO, ARBOL, XCOPIAR,
  MAS, ORDENAR, TECLADO (el teclado español, residente) y PASAR (archivos
  por el puerto serie).
- **El disquete de Oficina** (`build/oficina8086-NNN.img`, sin sistema, en
  las mismas cuatro medidas que el del sistema): **PLUMA**, el procesador
  de textos (21 K): ajuste de línea, negrita, subrayado y cursiva con
  atributos de vídeo, párrafos centrados y justificados, documentos mayores
  que la memoria, deshacer, bloques, buscar sin tildes, la vista de la
  página, Epson, IBM y texto con la ñ y las tildes bien, exportar a UTF-8 y
  RTF, y las teclas de WordStar (todo en [PLUMA.md](PLUMA.md)); y
  **CUENTAS**, la hoja de cálculo, con su ejemplo GASTOS.CSV (ver
  «CUENTAS», más abajo, y [CUENTAS.md](CUENTAS.md)).
- **El disquete de Juegos** (`build/juegos8086-NNN.img`, sin sistema, en las
  mismas cuatro medidas): **DERRIBO**, de pala, pelota y ladrillos, y
  **MARCIANADA**, de marcianitos, con sus instrucciones en `JUEGOS.TXT`
  (ver «Juegos», más abajo).
- **Prueba automática** (`make -C h8086 prueba`): 825 comprobaciones en
  QEMU, en treinta y tres partes (con las de PLUMA, CUENTAS y los juegos) (y la batería entera se pasa también con el
  núcleo en la memoria alta, `MODO=alto`, y como en un 8086, `MODO=8086`): arrancando de disquete y de disco duro, con
  discos partidos y formateados por el propio sistema, disquetes de siete
  medidas, una sola disquetera cambiando de disquete, discos estropeados a
  propósito que REVISAR arregla, un controlador de bloque y otro de carácter
  cargados con DEVICE=, el puerto paralelo, el teclado español tecla a
  tecla, el editor, y «el otro ordenador» por el puerto serie. Todo lo que
  se escribe en los discos se mira después desde fuera: cada archivo tiene
  su contenido exacto y `fsck.fat` no encuentra ningún error.

**Probado también en un procesador de 16 bits** (16/11/2026): en el emulador
8086tiny, que implementa un 8086/80186, arranca desde los disquetes de 360 K y
de 1,44 M, ejecuta el programa de prueba y lista el disco (ya con el sistema
de archivos de la etapa 1); se reconoce como «80186 u 80188» (lo que ese
emulador es) y, como debe, no aconseja cambiar de sistema. QEMU, que es donde
corre la prueba automática, no emula nada anterior al 486.

**Probado con las ROM originales de IBM** (16/11/2026, revisión independiente
de la versión 0.7 en el emulador PCjs, que reproduce el 8088 y las máquinas de
IBM con su BIOS de fábrica):

| Máquina | Lo probado |
|---|---|
| IBM PC 5150 (BIOS de 24/04/1981, 8088, MDA, 64 K) | arranca del disquete de 360 K; EQUIPO y DIR. Con solo 64 K quedan 44 K libres |
| IBM XT 5160 (BIOS de 08/11/1982, 8088, MDA y CGA, 256 K y 640 K) | EQUIPO («8086 u 8088»), MOSTRAR, programas .COM y .EXE, lotes con IF, GOTO y parámetros, `>` y `>>`, COPIAR, BORRAR, directorios, LIMPIAR, VER, ACTUALIZAR |
| XT con disco duro de 10 megas (controladora Xebec original) | `DIR C:`, `C:` y MOSTRAR desde C: |
| IBM AT 5170 (80286), disquetes de 1,2 M y 1,44 M | «Procesador: 80286», DIR, ACTUALIZAR |

De esa revisión salió un fallo, arreglado en la 0.7.1: `DIR C:` desde `A:`
decía el espacio libre de A: en vez del de C:.

**La 0.8, también con las ROM de IBM** (16/11/2026, segunda revisión
independiente en PCjs):

| Máquina | Lo probado, y bien |
|---|---|
| IBM PC 5150 (también con 64 K) y XT 5160, con MDA y con CGA | arranque del disquete de 360 K |
| IBM AT 5170 | arranque de 360 K, 720 K, 1,2 M y 1,44 M |
| XT con disco duro (controladora Xebec), disco en blanco | `PARTIR CREAR /A` y `FORMATEA C: /S /A`: partición activa FAT12 (tipo 01h), tabla y BPB correctos vistos desde fuera, y **arranque de C: sin disquete** |
| XT, disquetera de 360 K | `FORMATEA A: /S /A` **graba de verdad las pistas** (quedan rellenas con F6h) y el disquete arranca; `SISTEMA A: /A` |
| XT | lotes con FOR, LLAMAR y DESPLAZAR; `DIR \| ORDENAR \| MAS`; CONFIG.SYS con FILES=20; INT 25h y 26h |

Sin probar allí: nuestro sector de particiones (el disco lo fabricó el
emulador con el suyo). De esa revisión salieron dos cosas:

- En `DIR | ORDENAR` salía en la lista el temporal de la tubería. Desde la
  0.9 los temporales se crean ocultos.
- En el XT con la Xebec, PARTIR dijo «305 cilindros, 2 caras, 17 sectores: 5
  megas» de un disco de 4 caras y 10 megas. Repasado: PARTIR, FORMATEA y el
  núcleo leen la geometría igual y como manda la BIOS (INT 13h/08h: DH es la
  última cara, los seis bits bajos de CL el último sector, y CH con los dos
  bits altos de CL el último cilindro; a cada uno se le suma uno; DL, el
  número de discos, no se usa). La batería lo comprueba ahora con un disco
  de 306 cilindros, 4 caras y 17 sectores, que sale con sus 4 caras y sus 10
  megas. En el XT, esos números no salen del disco sino de la tabla de tipos
  de la controladora (la de INT 41h), según sus microinterruptores: si dicen
  «tipo de 2 caras», la BIOS da 2 caras y todo el sistema (también FORMATEA,
  que usa el mismo camino) trabaja con medio disco; la partición FAT12 de 5
  megas que salió es coherente con eso. El cilindro de menos (305 de 306) es
  el que la BIOS se guarda para pruebas. La explicación es una deducción
  del código y de cómo funciona esa BIOS: **queda por confirmar en PCjs**,
  poniendo en la controladora el tipo que corresponde al disco.

**Lo de la 0.8 y la 0.9 solo está probado en QEMU** (un 486 con la BIOS
SeaBIOS). Falta pasarlo por las ROM de IBM (PCjs) y por un procesador de 16
bits (8086tiny), como se hizo con la 0.7. Y hay cosas que QEMU no deja probar
de verdad:

- **El formato de bajo nivel de los disquetes.** La BIOS de QEMU contesta
  «hecho» a la orden de grabar una pista (INT 13h/05h) sin grabar nada, y su
  disquetera no sabe devolver un error de lectura. La prueba recorre el
  camino entero, pero que la pista quede grabada y que una pista mala se
  detecte solo se verá en una disquetera real. Lo que sí se prueba es qué se
  hace con una pista mala: FORMATEA tiene un parámetro solo para eso
  (`/M:n`, da por mala la pista n).
- **Las unidades de 1,2 M con disquetes de 360 K**, y en general el cambio de
  densidad (INT 13h/17h y 18h).
- **AUX y COM1** (el puerto serie como dispositivo) y los errores de la
  impresora (sin papel, apagada): en QEMU la impresora siempre contesta.
- **TECLADO en un teclado de 83 teclas** (PC y XT) y en una BIOS de IBM: la
  prueba manda los códigos de un teclado de 102 teclas.
- **VERIFICAR ON** se prueba (se copia con él puesto), pero la BIOS de QEMU
  no repasa nada de verdad.

**Lo que falta por comprobar:** todo lo anterior en máquinas reales; PASAR
por un puerto serie de verdad; un V20 y un 80186; y, sobre todo, un XT real
con su disquetera y su disco duro.

**Límites de hoy:**

- Particiones de hasta 32 megas (65.535 sectores), una sola por disco,
  dentro de los primeros 2 gigas, y un solo disco duro.
- De 8 a 64 archivos abiertos entre todos los programas (FILES=). Sin la
  vieja interfaz de archivos por FCB.
- La consola de INT 21h (01h a 0Ch) y los números de archivo 0 a 2 van a la
  BIOS directamente: un controlador que se llame CON (un ANSI.SYS) se carga
  y se puede abrir por su nombre, pero no sustituye a la consola.
- A un controlador de bloque no se le pregunta si han cambiado el disco
  (órdenes 1 y 2): vale para discos fijos, como DISCORAM. Los controladores
  tienen que ser binarios (.SYS), no .EXE. Dos unidades como mucho (D:, E:).
- En un PC o un XT (sin reloj) la fecha hay que darla con FECHA cada vez que
  se enciende, y no pasa sola al día siguiente a medianoche.
- Las mayúsculas con tilde no existen en la página 437 (salvo É): TECLADO
  las escribe sin ella. No hay más páginas de códigos.
- REVISAR no arregla los archivos cruzados (lo dice, como CHKDSK) ni repasa
  la superficie del disco.
- EDITAR: un archivo de hasta unos 55 K, líneas de hasta 250 letras (las más
  largas se parten al leer), los tabuladores se ven como un carácter.
- DIR no ordena ni tiene `/S` ni `/A`.
- La memoria extendida: sin bloques de memoria superior (UMB, las funciones
  10h a 12h de XMS dicen «no la hay»), 16 bloques a la vez, y hasta unos
  63 megas (lo que da INT 15h/88h, que en un 286 son 15). Un programa que
  cuente con que la memoria «da la vuelta» al mega (algunos .EXE
  comprimidos con EXEPACK) puede fallar con `DOS=ALTO`, como en MS-DOS 5;
  con `DOS=BAJO` A20 está cerrada mientras nadie la pida.
- VOL enseña la etiqueta sin los espacios que lleve dentro (ETIQUETA la
  enseña bien).

## Las decisiones

### 1. Dos núcleos en la familia, no uno por procesador

La familia agrupa por **clase de máquina**:

| Núcleo | Procesadores | Sistema |
|---|---|---|
| 16 bits, modo real | 8086, 8088, V20/V30, 80186/188, 286 | **Hispa-8086** |
| 32 bits | 386 o superior | **Hispa-386** (con su escritorio, Hispa Ventanas) |

**Los nombres** (Carlos, 16/11/2026). **Hispa-DOS** es la familia: los
sistemas operativos con DOS por debajo, «hasta llegar a un kernel que no
dependa de DOS». Cada miembro se llama por el procesador MÍNIMO que pide:

- **Hispa-8086**: del 8086 al 286 (este documento).
- **Hispa-386**: 386 o superior. Es el sistema de 32 bits que ya existe;
  **Hispa Ventanas** es el nombre de su escritorio, que va dentro.
- **Hispa-486**: reservado para más adelante (lo de mediados y finales de los
  90). El número definitivo será el del procesador mínimo en el que vaya bien.

Después vendrá un núcleo nuevo, sin DOS. La orden VER y el repositorio del
sistema de 32 bits siguen diciendo «Hispa-DOS 0.9.x»: es el nombre de la
familia.

Por qué no más:

- En modo real, un 286 es un 8086 rápido con dos cosas de más: los 64 K de
  memoria alta (HMA) y la memoria extendida. El mismo sistema lo detecta al
  arrancar y las aprovecha («modo 286»). Un sistema aparte para el 286 solo
  se justificaría por su modo protegido de 16 bits, que casi ningún programa
  de la época usó.
- Entre un 386 y un 486 no hay nada que el sistema tenga que hacer distinto.
- Cada sistema de más son pruebas, discos, manuales y actualizaciones que
  mantener, sin nada a cambio para quien lo usa.

«Hispa-286» no es, por tanto, otro producto: es Hispa-8086 en un 286 (el
nombre dice el procesador mínimo, no el único).
(Decisión de Carlos, 16/11/2026, sobre la propuesta de tres ediciones del
15/11.)

### 2. Un núcleo nuevo, no una adaptación del de Hispa-386

Se midió antes de decidir: ensamblando Hispa-386 con `cpu 8086`, el núcleo
(15.000 líneas, 33 K) tiene **1.026** instrucciones que un 8086 no entiende, y
el intérprete de órdenes (7.700 líneas, 25 K), **241**. Se podrían cambiar una
a una, pero el resultado seguiría siendo un sistema pensado para un 386 con 4
megas: 59 K residentes, más que MS-DOS 3.3 (54 K) en máquinas que a veces
tienen 256 K.

Hispa-8086 se escribe de nuevo, pequeño, reutilizando de Hispa-386 lo que no
depende del procesador: el diseño, las órdenes en español, los mensajes, los
formatos y las pruebas.

### 3. Todo en ensamblador

Para ganar memoria y velocidad a 4,77 MHz. Lo vigila el ensamblador: todos los
fuentes llevan `cpu 8086`. La única excepción es el tramo que distingue un 386
de un 486, que solo se ejecuta cuando ya se sabe que el procesador es de 32
bits. ([Castel](../../castel/README.md), el lenguaje de la familia, que ya tiene su primer compilador,
tendrá que poder generar código de 8086 para que las aplicaciones grandes se
escriban en él.)

### 4. El objetivo de compatibilidad: MS-DOS 3.30

Es el DOS de estas máquinas: casi todo lo que se escribió para un PC, un XT o
un AT funciona con la interfaz de la versión 3.30 (INT 21h hasta la función
62h, más las de la 3.3). Hispa-8086 contesta «3.30» a quien pregunta la
versión. Los programas que escriben directamente en la pantalla o llaman a la
BIOS —los más populares lo hacían— tienen que funcionar igual: el sistema no
se pone en medio.

### 5. Presupuesto de memoria

| | MS-DOS 3.3 | Hispa-386 0.9 | Hispa-8086 (objetivo) |
|---|---|---|---|
| Residente (núcleo + intérprete) | ~54 K | ~59 K | **menos de 32 K** |
| 0.9 | | | unos 30 K: 17 del núcleo (con 4 sectores de reserva) y 13 del intérprete |
| Hoy (0.10), sin 286 o con `DOS=BAJO` | | | quedan 618.768 octetos libres de 640 K (unos 35 K ocupados: núcleo con el gestor de memoria extendida, la reserva de sectores e intérprete) |
| Hoy (0.10), `DOS=ALTO` en un 286 con más de un mega | | | quedan 638.000 octetos libres (el núcleo sube a la HMA: 19.232 octetos más que abajo) |

Reglas: el intérprete de órdenes tiene una parte que se puede pisar y volver
a cargar (como COMMAND.COM); los búferes de disco se piden según la memoria
que haya; nada que no se use ocupa sitio.

### 6. Disquetes: el sistema base cabe en 360 K

Una unidad no lee más densidad que la suya, y la BIOS de un XT no sabe de alta
densidad:

| Disquete | Máquinas |
|---|---|
| 5¼ de 360 K (y 160, 180 y 320 K) | PC y XT: **la medida de referencia** |
| 3½ de 720 K | XT tardíos, portátiles, PS/2 modelo 30 |
| 5¼ de 1,2 M | AT (286) |
| 3½ de 1,44 M | 286 tardíos en adelante |

El sector de arranque lee las medidas del propio disquete (su BPB), así que el
mismo código arranca de todos. **Regla:** el sistema base (núcleo, intérprete,
órdenes esenciales y la transferencia de archivos) cabe siempre en un disquete
de 360 K; las aplicaciones van en un segundo disquete. Se publican las cuatro
imágenes. Pendiente: las medidas de una cara y ocho sectores del PC original
(160, 180 y 320 K).

### 7. Qué hardware se atiende

De época, y solo de época: disquete; disco duro por la BIOS (MFM/RLL, IDE,
XT-IDE con CompactFlash); teclado de XT y de AT; pantalla MDA, Hercules, CGA,
EGA y VGA en texto (y los gráficos de CGA y Hercules para el entorno y los
juegos); puerto serie y paralelo; ratón serie; altavoz del PC. Sin USB, sin
sonido digital, sin CD: eso es de Hispa-386.

### 8. El entorno: Hispa Escritorio *(nombre provisional)*

Un entorno que junta las aplicaciones, como GEM en los Amstrad, pero **en modo
de texto**: ventanas, menús y diálogos hechos con caracteres.

- Funciona igual en MDA, Hercules, CGA, EGA y VGA (GEM y Windows 1.0 pedían
  tarjeta gráfica, y cada una con su controlador).
- A 4,77 MHz, pintar texto es inmediato; pintar gráficos de CGA, no.
- Cabe: el objetivo es que el entorno ocupe menos de 40 K y deje sitio a la
  aplicación en una máquina de 256 K.
- Con teclado siempre; con ratón, si lo hay.

Las aplicaciones del entorno comparten una biblioteca de ventanas en texto, así
que todas se manejan igual (mismo menú, mismas teclas). Los gráficos se dejan
para lo que los necesita: los juegos y, más adelante, un modo gráfico opcional.

El nombre «Hispa Ventanas» es el del escritorio gráfico de Hispa-386.

### 9. Hispa-BASIC: compatible con GW-BASIC y en español

Un intérprete propio, escrito por nosotros en ensamblador:

- **Compatible**: ejecuta los programas de GW-BASIC tal cual, los escritos en
  texto y los guardados en binario (con sus mismos códigos internos y su
  formato de números), que es como están los listados de las revistas y los
  disquetes de la época. La referencia de comportamiento es GW-BASIC 3.23.
- **En español**: los mensajes y los errores, en español. Y las órdenes
  también pueden escribirse en español (`IMPRIMIR`, `SI… ENTONCES`, `PARA…
  SIGUIENTE`): cada palabra española se guarda con el mismo código interno
  que la inglesa, así que un programa escrito en español es, por dentro, un
  programa de GW-BASIC normal y funciona en cualquier otro; y `LISTAR` lo
  enseña en el idioma que se elija.
- **Sala limpia**, como todo el proyecto: se hace observando el comportamiento
  y la documentación. El fuente que Microsoft publicó en 2020 (licencia MIT)
  es una referencia histórica que se puede consultar; el código es nuestro.

### 10. Una familia que se actualiza entre sí

Todos los sistemas Hispa comparten el mismo mecanismo:

- **Una lista de versiones común** (`versiones.txt`, la que ya lee
  `ACTUALIZAR` en Hispa-386), con una sección por sistema y versión y sus
  requisitos: procesador mínimo, memoria, disco y edición.
- **Cada sistema mira la máquina** al arrancar y con `EQUIPO`.
- **Cada sistema recomienda el que le corresponde a esa máquina.** Hispa-8086
  en un 386 o un 486 funciona, pero aconseja pasar a Hispa-386 (ya lo hace
  desde la etapa 0); Hispa-386 en una máquina que no llegue dirá que su sitio
  es Hispa-8086. Es un consejo, una vez, sin
  insistir ni impedir nada: quien quiera un 486 con Hispa-8086 puede tenerlo.
- **`ACTUALIZAR` cambia de versión y también de sistema**, conservando los
  datos del usuario (como hace hoy entre versiones de Hispa-386).
- **Por dónde llega la actualización** depende de la máquina: por red donde
  la hay (Hispa-386); en estas, por disquete o por el cable serie o paralelo
  desde otro ordenador con `PASAR`, que es además la respuesta a lo que más
  se echa de menos (ver el estudio).
- **Los documentos son comunes**: .TXT y .CSV se abren en cualquier sistema
  de la familia.

## Etapas

| Etapa | Qué | Estado |
|---|---|---|
| 0 | Cimientos: arranque, núcleo mínimo, detección del equipo, .COM | **hecha** |
| 1 | Archivos por INT 21h (abrir, leer, escribir, crear, borrar, buscar), subdirectorios, FAT12 de escritura | **hecha** |
| 2 | Memoria por bloques (48h-4Ah), programas .EXE, lanzar programas (4Bh), entorno | **hecha** |
| 3 | Intérprete de órdenes aparte (COMMAND.COM): órdenes en español, archivos .BAT, AUTOEXEC.BAT | **hecha**: SI, IR A, PARA, LLAMAR, DESPLAZAR, redirección, tuberías, entorno, PATH e indicador |
| 4 | Disco duro: particiones, FAT16; CONFIG.SYS y controladores de dispositivo | **hecha**: C: con FAT16, arranque del disco duro, CONFIG.SYS con FILES=, BUFFERS=, SHELL= y DEVICE= (controladores de carácter y de bloque), la cadena de dispositivos |
| 5 | Órdenes esenciales: FORMAT, SYS, FDISK, COPIAR, REVISAR, EDIT, **PASAR** (serie y paralelo) | **casi**: todas las de MS-DOS 3.3 que hacen falta, EDITAR y TECLADO; falta PASAR por el puerto paralelo (y las menores: IMPRIMIR, COMPARAR, COPIADISCO) |
| 6 | Modo 286: HMA y memoria extendida | **hecha** (0.10): gestor XMS 2.0 dentro del núcleo, A20, `DOS=ALTO`, DISCORAM `/E`; sin UMB |
| 7 | Hispa-BASIC | **primera entrega** (0.11): `BASIC.COM`, compatible con GW-BASIC (lee sus programas en texto y en binario) y con las órdenes también en español; ver docs/8086/BASIC.md. Faltan la 0.12 (doble precisión completa, PRINT USING, archivos, ON ERROR) y la 0.13 (pantalla y sonido) |
| 8 | Hispa Escritorio y sus aplicaciones: Pluma, Cuentas, Fichas, Gestor | **oficina hecha** (0.13): PLUMA, el procesador de textos, y CUENTAS, la hoja de cálculo, en el disquete de Oficina ([PLUMA.md](PLUMA.md), [CUENTAS.md](CUENTAS.md)); faltan Fichas, Gestor y el Escritorio |
| 9 | Juegos, manual, versión estable 1.0 | **empezada** (0.13): dos juegos, DERRIBO y MARCIANADA, en el disquete de Juegos; faltan el manual y la 1.0 |

Cada etapa se cierra con sus pruebas automáticas y probada en una máquina real.
La compatibilidad se mide como en Hispa-386: con programas de la época que
aporta cada usuario (nunca se distribuyen con el proyecto).

## Las órdenes

Cada orden tiene su nombre en español y el clásico de MS-DOS. Las de dentro
del intérprete:

| Orden | Clásico | |
|---|---|---|
| `AYUDA`, `VER`, `EQUIPO`, `ACTUALIZAR` | `HELP`, `VERSION` | la lista de órdenes, la versión, el procesador y la memoria |
| `MEMORIA` | `MEM` | la memoria total y la libre; en un 286 con más de un mega, también la memoria alta (HMA: libre, de un programa o del sistema) y la extendida, con la que queda libre |
| `DIR [ruta] [/P] [/W]` | `LISTAR` | los archivos, con su tamaño, su fecha y su hora; `/P` se para a cada pantalla, `/W` solo los nombres, cinco por línea (los directorios, entre corchetes) |
| `MOSTRAR archivo` | `TYPE` | enseña un texto |
| `COPIAR origen [destino]` | `COPY` | conserva la fecha y la hora del original; con comodines, directorios enteros, nombre nuevo con comodines (`COPIAR *.TXT *.BAK`) y dispositivos: `COPIAR CON nota.txt` (se acaba con Ctrl+Z e Intro), `COPIAR nota.txt PRN` |
| `MOVER origen [destino]` | `MOVE` | cambia el archivo de directorio sin copiarlo; a otra unidad, lo copia y borra el original |
| `BORRAR archivo` | `DEL` | con comodines; `*.*` pregunta antes |
| `RENOMBRAR viejo nuevo` | `REN` | con comodines: `REN *.TXT *.BAK` |
| `CD`, `CREARDIR`, `BORRARDIR` | `MD`, `RD` | los directorios |
| `VOL [unidad:]` | | cómo se llama el disco |
| `FECHA [dd-mm-aaaa]`, `HORA [hh:mm:ss]` | `DATE`, `TIME` | las dicen y las ponen (sin nada, preguntan) |
| `VERIFICAR [ON\|OFF]` | `VERIFY` | repasar lo que se escribe en el disco |
| `PON [NOMBRE=valor]` | `SET` | las variables de entorno (512 octetos) |
| `RUTA [dir;dir]` | `PATH` | dónde se buscan los programas, además de en el directorio de trabajo; `RUTA ;` la quita |
| `INDICADOR [texto]` | `PROMPT` | `$P` unidad y directorio, `$N` unidad, `$G` `>`, `$L` `<`, `$B` `\|`, `$Q` `=`, `$D` fecha, `$T` hora, `$_` línea nueva, `$$` `$`. Sin nada, `$P$G` |
| `LIMPIAR` | `CLS` | limpia la pantalla |
| `ECO`, `REM`, `PAUSA`, `SI`, `IR A`, `PARA`, `LLAMAR`, `DESPLAZAR` | `ECHO`, `PAUSE`, `IF`, `GOTO`, `FOR`, `CALL`, `SHIFT` | para los lotes (ver «Lotes y tuberías») |

Y los programas del disquete (COMMAND.COM traduce el nombre clásico, sin
duplicar archivos):

| Orden | Clásico | |
|---|---|---|
| `EDITAR archivo` | `EDIT` | el editor de texto (ver «EDITAR») |
| `FORMATEA`, `SISTEMA`, `PARTIR` | `FORMAT`, `SYS`, `FDISK` | ver «Preparar discos» |
| `REVISAR [unidad:] [/F]` | `CHKDSK` | repasa el disco (ver más abajo) |
| `ETIQUETA [unidad:] [nombre]` | `LABEL` | dice, pone o quita (`/B`) el nombre del disco |
| `ATRIBUTO [+R -R +A -A +S -S +H -H] archivos` | `ATTRIB` | solo leer, cambiado, del sistema, oculto; sin signos, los enseña |
| `ARBOL [directorio] [/A]` | `TREE` | los directorios en árbol; con `/A`, también los archivos |
| `XCOPIAR origen [destino] [/S]` | `XCOPY` | copia un directorio entero, con la fecha y la hora de cada archivo; con `/S`, con sus subdirectorios, que crea en el destino |
| `MAS`, `ORDENAR` | `MORE`, `SORT` | los filtros |
| `TECLADO [/N]` | `KEYB` | el teclado español (ver «TECLADO») |
| `PASAR` | | archivos por el puerto serie |

### REVISAR (CHKDSK)

Recorre todos los directorios siguiendo en la FAT la cadena de grupos de cada
archivo y dice lo que encuentra: un tamaño que no concuerda con los grupos,
una cadena rota, dos archivos cruzados (comparten un grupo) y grupos
perdidos (ocupados, pero de ningún archivo). Sin `/F` solo mira. Con `/F`
corrige: ajusta el tamaño, corta la cadena rota y guarda cada cadena perdida
en un archivo `FILE0000.CHK`, `FILE0001.CHK`... en la raíz, para ver si tiene
algo que valga. Los cruzados no los arregla: hay que copiarlos y borrar los
dos, como con CHKDSK. Al acabar dice lo que hay en el disco (total,
directorios, archivos, ocultos, defectuosos, libre) y la memoria total y
libre. Lee la FAT entera a la memoria (hasta 128 K) y usa un bit por grupo;
las medidas del disco se las pide al núcleo, así que vale para cualquier
unidad, también la de un controlador.

## La línea que se teclea

Toda línea que se teclea (una orden, una línea de un programa de BASIC, la
respuesta a un INPUT, lo de `COPIAR CON`...) se puede corregir con las teclas
de cursor antes de dar a Intro:

| | |
|---|---|
| izquierda, derecha | mueven el cursor por la línea |
| Inicio, Fin | al principio y al final |
| Supr, Retroceso | borran la letra del cursor y la de delante |
| Ins | intercalar (lo normal) o escribir encima |
| arriba, abajo | las líneas tecleadas antes (las que quepan en 384 octetos) |
| F3 | lo que falta de la última línea, como en DOS |
| Esc | borra la línea |

Valen las teclas grises del teclado de 101 teclas y las del teclado numérico
(sin BloqNum), que son las únicas del de 83. Lo hace el núcleo, en la lectura
de líneas (INT 21h/0Ah y la lectura de la consola con 3Fh), así que sirve a
cualquier programa; con la entrada o la salida redirigidas no hay cursor que
mover y solo vale Retroceso. La línea puede ocupar más de un renglón. Un
programa puede dejar puesto el texto con el que empieza la línea siguiente
(INT 21h/E8h con AL=8 y DS:DX = el texto, acabado en 0): así hace `EDIT` en
BASIC.

Las lecturas de una tecla (INT 21h/01h, 06h, 07h y 08h) dan las teclas que
no son letra (las flechas, las de función) en dos veces, como en DOS: un 0
y, a la lectura siguiente, el código de la tecla. Así las ve `INKEY$`.

## EDITAR

```
EDITAR archivo
```

Un editor de pantalla completa en modo de texto (MDA, Hercules, CGA, EGA o
VGA): veinticuatro líneas de texto y, abajo, una línea con las teclas, la
línea y la columna, el modo y un asterisco si hay cambios sin guardar.

| | |
|---|---|
| flechas, Inicio, Fin, RePág, AvPág | moverse |
| Ins | insertar o sobrescribir |
| Supr, Retroceso | borrar (al final o al principio de la línea, la juntan con la de al lado) |
| F2 | guardar |
| F3 | buscar, desde detrás del cursor hasta el final, sin distinguir mayúsculas; F3 e Intro, el siguiente |
| F4, F5, F6 | copiar la línea, cortarla, pegarla encima |
| F10 o Esc | salir (si hay cambios, pregunta) |

Ocupa 2 K. El texto está entero en la memoria; la línea del cursor se
trabaja en un búfer aparte y vuelve al texto al salir de ella, así que
teclear no mueve lo que hay detrás: va bien a 4,77 MHz.

## TECLADO

```
TECLADO         pone el teclado español
TECLADO /N      vuelve al de la BIOS (el americano)
```

Se queda residente (0,7 K) y atiende la interrupción del teclado antes que
la BIOS: las teclas que cambian en el teclado español las mete él en la cola
del teclado; las demás pasan a la BIOS como siempre. Pone la ñ y la ç, `º ª
¡ ¿`, los signos en su sitio, con AltGr (o Ctrl+Alt) `| @ # ~ [ ] { } \ ¬`,
y los acentos (agudo, grave, diéresis y circunflejo) como teclas muertas: la
tecla del acento no escribe hasta la vocal siguiente. Es la distribución de 102 teclas.
Las flechas y las demás teclas de cursor no las toca. (Por la interrupción
del teclado, que es el camino de los PC y XT y el de `TECLADO /9`, el
prefijo E0h de las teclas grises lo atiende él y se lo deja apuntado a la
BIOS: si lo leyeran los dos, en un AT la tecla contaría dos veces.)
`TECLADO` en AUTOEXEC.BAT lo deja puesto desde el arranque.

## Dispositivos

Como en MS-DOS, cada dispositivo tiene una cabecera (la siguiente de la
cadena, atributos, sus dos rutinas y su nombre) y se le habla con
«peticiones»: un paquete con la orden (iniciar, leer, escribir...) que se le
pasa a su rutina de estrategia y luego a la de interrupción
(`h8086/dispositivos.inc`).

- **Los del núcleo**: `NUL`, `CON`, `AUX` (y `COM1`), `PRN` (y `LPT1`). Un
  archivo que se abre con uno de esos nombres, con cualquier ruta y
  extensión, es el dispositivo: `COPIAR carta.txt PRN`, `DIR > PRN`,
  `orden > NUL`, `COPIAR CON nota.txt`. Los números de archivo 3 y 4 son AUX
  y PRN.
- **`DEVICE=archivo [parámetros]`** en CONFIG.SYS carga un controlador al
  estilo de DOS: un binario con su cabecera al principio. Se carga en lo
  alto del núcleo, se le manda la orden 0 (iniciar), que devuelve hasta
  dónde se queda residente, y:
  - si es **de carácter**, se enlaza detrás de NUL y se abre por su nombre;
  - si es **de bloque**, cada unidad que diga tener recibe la letra
    siguiente (D:, E:), con las medidas (BPB) que dé; los sectores se le
    piden de uno en uno (órdenes 4 y 8).
- **DISCORAM.SYS** es el ejemplo, y sirve: `DEVICE=\DISCORAM.SYS 64` deja un
  disco en memoria de 64 K (de 16 a 512) en D:, con formato y vacío. Lo que
  ocupa se le quita a los programas. Con **`/E`**
  (`DEVICE=\DISCORAM.SYS 1000 /E`), en un 286 con memoria extendida, el disco
  va en la extendida (hasta 4.000 K): se lo pide al gestor XMS del núcleo y
  mueve cada sector con su función de mover, y en la memoria convencional
  solo deja el controlador (unos 400 octetos). Sin memoria extendida, `/E`
  lo dice y no se instala. Es 1,1 K de controlador
  (`h8086/ordenes/discoram.asm`, comentado para que sirva de modelo).

## La reserva de sectores (BUFFERS=)

Entre el núcleo y el disco hay una reserva pequeña con los últimos sectores
leídos **de la FAT y de los directorios**, que son los que se piden una y
otra vez: lo que está en ella no se vuelve a leer del disquete. Los sectores
de datos de los archivos no entran (pasan una vez y echarían a los que
sirven). `BUFFERS=n` dice cuántos sectores guarda: de 0 (sin reserva) a 30;
sin decir nada, 4. Cada uno son 516 octetos de memoria residente. Con
`DOS=ALTO` la reserva sube a la HMA con el resto del núcleo y no gasta
memoria convencional: sin decir nada, entonces, son 20.

**Escribir no se retrasa**: cada sector que el núcleo da por escrito va al
disco en el momento y, si tiene copia en la reserva, se pone al día. Lo
único que se retrasa es lo de siempre: el sector que se está usando, que se
escribe al cambiar de sector y al cerrar el archivo. Sacar el disquete no
pierde más que antes.

La reserva de un disquete se olvida cuando el disquete es otro: si cambian
sus medidas o su número de serie; siempre, si no lleva número de serie (los
de DOS 3 y anteriores); y cuando la BIOS avisa del cambio (los AT).

Medido en la batería (QEMU, disquete de 360 K: `DIR` dos veces, `COPIAR` un
archivo y `MOSTRAR`lo), sectores pedidos de verdad al disco:

| | leídos | escritos |
|---|---|---|
| `BUFFERS=0` (como la 0.8) | 73 | 9 |
| sin decir nada (4) | 9 | 9 |
| `BUFFERS=20` | 9 | 9 |

## Una sola disquetera, y los disquetes del PC original

En una máquina con una sola disquetera existen A: y B:, que son la misma
unidad con otro disquete, como en DOS: cuando hace falta el de la otra, el
núcleo lo pide («Ponga el disquete de la unidad B: y pulse una tecla...»)
por la BIOS, aunque la consola esté redirigida. Así se puede copiar de un
disquete a otro (`COPIAR A:*.* B:`), con un cambio por cada trozo. FORMATEA
y SISTEMA piden ellas el disquete y se lo dicen al núcleo.

FORMATEA hace también los disquetes de **160, 180 y 320 K** (`/F:160`...),
de una cara o de ocho sectores por pista. Y el núcleo lee los que formateó
DOS 1, que no llevan sus medidas en el primer sector: las deduce del primer
octeto de la FAT (FEh 160 K, FCh 180 K, FFh 320 K, FDh 360 K).

## Preparar discos (PARTIR, FORMATEA, SISTEMA)

Las tres son programas del disquete (no van dentro del intérprete), tienen su
nombre clásico (FDISK, FORMAT, SYS: COMMAND.COM lo traduce, sin duplicar los
archivos) y **preguntan antes de destruir nada**; con `/A` («automático») no
preguntan, para los lotes y las pruebas. Trabajan con la BIOS, por debajo del
núcleo (son discos que el núcleo todavía no conoce), y al acabar le avisan
para que vuelva a mirar los discos (INT 21h/E8h con AL=1): no hace falta
reiniciar.

Instalar el sistema en un disco duro vacío, arrancando del disquete:

```
PARTIR CREAR        crea la partición de DOS y la marca activa
FORMATEA C: /S      le da formato y copia el sistema
```

y el disco duro arranca solo.

### PARTIR (FDISK)

```
PARTIR                 enseña el disco y su tabla de particiones
PARTIR CREAR           crea la partición de DOS
PARTIR ACTIVAR n       la partición n pasa a ser la que arranca
PARTIR BORRAR n        borra la partición n
```

CREAR solo actúa en un disco sin particiones: hace una partición primaria
desde la segunda pista hasta donde llega este sistema (32 megas: 65.535
sectores), en cilindros enteros; de tipo 04h (FAT16) si tiene 16 megas o más
y 01h (FAT12) si tiene menos, como MS-DOS 3.3; activa; con su primer sector a
cero, para que nadie tome por bueno un formato viejo. Si el sector de
particiones no tenía código (un disco nuevo), le pone el nuestro
(`h8086/mbr.asm`, que busca la partición activa y la arranca). No hace
particiones extendidas ni más de una: es un PARTIR sencillo.

### FORMATEA (FORMAT)

```
FORMATEA unidad: [/S] [/V:etiqueta] [/F:medida] [/R] [/A]
     /S            copia además el sistema: el disco queda de arranque
     /V:etiqueta   el nombre del disco, hasta 11 letras
     /F:medida     360, 720, 1200 o 1440 (un disquete)
     /R            rápido: sin grabar ni repasar las pistas
     /A            automático: no pregunta
```

- **Un disquete (A: o B:)** recibe formato de verdad: se prepara la unidad
  para esa medida (INT 13h/18h, o 17h en los primeros AT, y la tabla de
  parámetros de INT 1Eh con los sectores por pista y el hueco que tocan), se
  graba cada pista (INT 13h/05h) y se repasa leyéndola entera. La pista que
  falla se apunta en la FAT como defectuosa y el disquete sirve con lo demás
  (salvo que falle el principio, donde van el arranque, la FAT y el
  directorio). Si la BIOS no sabe grabar pistas, se dice y se hace el formato
  rápido. Sin `/F:`, un disquete que ya tiene formato conserva su medida, y
  uno nuevo recibe la mayor de la unidad (en un PC o un XT, que no saben
  decir qué unidad tienen, 360 K).
- **El disco duro (C:)** solo recibe formato lógico: FAT16 con grupos de 2 K
  si la partición tiene 16 megas o más, y FAT12 con los grupos justos si
  tiene menos; pone en la tabla de particiones el tipo que corresponde.
- Después escribe el sector de arranque, las dos FAT y el directorio raíz, y
  lo comprueba leyendo. Al acabar dice los octetos totales, los que hay en
  sectores defectuosos y los libres (lo que dice el propio núcleo del disco
  nuevo).
- Se niega si hay archivos abiertos en esa unidad. Devuelve 0 si ha ido
  bien, 4 si no y 5 si se contesta que no.

### SISTEMA (SYS)

```
SISTEMA unidad: [/A]
```

Deja de arranque un disco **que ya tiene formato**, sin tocar sus archivos:
escribe el sector de arranque de Hispa-8086 conservando las medidas del disco
(su BPB) y copia a la raíz `NUCLEO86.SYS` y `COMMAND.COM`, que toma de la
unidad en la que está la propia orden. En el disco duro, además, deja dichos
en el sector de arranque los sectores que hay delante de la partición y las
medidas del disco según la BIOS, marca la partición como activa y, si el
sector de particiones no tiene código, le pone el nuestro.

Los dos archivos se leen a la memoria antes de empezar (también en
`FORMATEA /S`): con una sola disquetera se puede cambiar de disquete cuando
lo pide.

### El sector de arranque

`h8086/arranque.asm` es el mismo para todo: disquetes y particiones, FAT12 y
FAT16. Quien lo instala (FORMATEA, SISTEMA o `pon_arranque.py`) conserva la
BPB del disco y deja dicho en el octeto 509 si la FAT es de 16 bits. Para
saltar a un sector de la partición le suma los «sectores ocultos» de la BPB;
en un disco duro, las medidas (sectores por pista y caras) se las pregunta a
la BIOS, porque las de la BPB son las de la máquina en la que se dio formato.
La FAT no la carga entera (una FAT16 son hasta 64 K): trae dos sectores cada
vez. Al arrancar del disco duro, el núcleo trabaja en C: (`PATH` y `COMSPEC`
apuntan allí) y A: sigue siendo la primera disquetera.

## Lotes y tuberías

Lo que entiende COMMAND.COM además de sus órdenes:

| | |
|---|---|
| `%0` a `%9` | el nombre del lote y sus parámetros; `%%` es un `%` |
| `IF [NOT] EXIST archivo orden` | también `IF [NOT] ERRORLEVEL n` e `IF [NOT] a==b` |
| `SI [NO] EXISTE archivo orden` | lo mismo en español: `SI [NO] NIVEL n`, `SI [NO] a==b`. Las palabras se pueden mezclar |
| `GOTO etiqueta`, `IR A etiqueta` | salta a la línea `:etiqueta` |
| `FOR %%v IN (lista) DO orden` | la orden, una vez por cada elemento de la lista (separados por espacios o comas); un elemento con `*` o `?` vale por todos los archivos que casen. También `PARA %%v EN (lista) HAZ orden`. Desde el teclado, con un solo `%`. Un FOR no puede llevar otro dentro, como en DOS |
| `LLAMAR lote [parámetros]` (`CALL`) | ejecuta otro lote y vuelve a la línea siguiente (sin LLAMAR, un lote que nombra a otro le pasa el mando y no vuelve). Se pueden encadenar: cada llamada guarda el lote que llama en un bloque de memoria de 160 octetos |
| `DESPLAZAR` (`SHIFT`) | corre los parámetros un sitio: `%1` pasa a ser `%0`, `%2` a ser `%1`... |
| `ECO`, `REM`, `PAUSA`, `@` | como en DOS (`ECHO`, `PAUSE`) |
| `orden > archivo`, `>> archivo`, `< archivo` | redirección |
| `orden \| orden` | tubería: lo que escribe la primera lo lee la segunda. Como en DOS, con archivos temporales (`\TUBO1.$$$` y `\TUBO2.$$$`, en la raíz de la unidad de trabajo, ocultos, que se borran al acabar): hace falta poder escribir en ese disco |

Los filtros: **MAS** (`MORE`, y también `más`) enseña el texto pantalla a
pantalla (una tecla sigue, Esc lo deja) y **ORDENAR** (`SORT`) ordena las
líneas sin distinguir mayúsculas (`/R`, al revés; `/+n`, desde la columna
n); los dos admiten también un archivo: `MAS carta.txt`. ORDENAR ordena en
memoria lo que quepa en su segmento (unos 60 K de texto).

Criterio de nombres: todo tiene nombre español y nombre clásico, y valen los
dos. `IF` es `SI` y `GOTO` es `IR A` (también `IR` a secas), como `FOR` es
`PARA... EN... HAZ`; dentro de `SI`, `NO`, `EXISTE` y `NIVEL` valen por
`NOT`, `EXIST` y `ERRORLEVEL`.

## El 286: memoria extendida, A20 y HMA

Desde la 0.10, Hispa-8086 saca partido de un 286 (o más) que tenga memoria
por encima del mega. En un 8088, un 8086, un V20 o un 80186 todo sigue igual:
la detección del procesador es lo primero, y si no encuentra un 286 no se
llega a nada de esto (`h8086/alta.inc`).

**Lo que hay:**

- **Un gestor de memoria extendida XMS 2.0**, como HIMEM.SYS: INT 2Fh/4300h
  dice que lo hay y 4310h da su punto de entrada; por él, la versión (00h),
  la HMA (pedirla y soltarla, 01h y 02h), A20 para todos y para uno (03h a
  06h, la local se cuenta) y cómo está (07h), cuánta queda (08h) y los
  bloques: pedir (09h), soltar (0Ah), mover (0Bh, también entre la memoria
  convencional y la extendida), bloquear y desbloquear (0Ch, 0Dh, que dan
  su dirección), qué hay de uno (0Eh) y cambiarle el tamaño (0Fh, que lo
  muda y copia si no puede crecer donde está). El tamaño de la extendida lo
  da INT 15h/88h; los 64 K primeros son la HMA y el resto se reparte en K.
- **La puerta A20** se abre por el controlador del teclado (8042, como en el
  AT) y por el puerto 92h (PS/2 y posteriores), y **se comprueba de
  verdad**: con ella cerrada, FFFF:0010 es el mismo octeto que 0000:0000; se
  cambia uno y se mira el otro.
- **`DOS=ALTO`** en CONFIG.SYS sube a la HMA el núcleo entero: el código, sus
  variables, su pila, la tabla de archivos y la reserva de sectores (con 20
  sectores, sin decir nada). Abajo solo se queda **el puente**, unos 350
  octetos: las entradas de INT 20h, 21h, 25h, 26h, 27h y 2Fh y la del gestor,
  que se aseguran de que A20 esté abierta y saltan arriba. Para eso el
  núcleo se ensambla desde la dirección 10h y corre con su segmento un
  párrafo por debajo de donde se carga: la misma imagen vale en 006F:0010 y
  en FFFF:0010, y subir es copiarla y saltar.
- **MEMORIA** enseña la convencional, la HMA y la extendida (total y libre);
  **DISCORAM `/E`** pone el disco en la extendida.
- **`PROCESADOR=8086`** hace como si no hubiera 286: ni gestor, ni A20, ni
  HMA, ni se toca INT 2Fh. Es el interruptor para probar que todo va sin lo
  nuevo (la batería lo hace).

Medido en la batería (QEMU, 640 K de memoria convencional y 4 megas en
total), la memoria libre para los programas que dice MEMORIA:

| | octetos libres |
|---|---|
| núcleo abajo (`DOS=BAJO`, o un 8086, o `PROCESADOR=8086`) | 618.768 |
| núcleo en la HMA (`DOS=ALTO`) | 638.000 (19.232 más) |
| con `FILES=20` y `BUFFERS=10`, núcleo abajo | unos 600 K |
| un DISCORAM `/E` de 1.000 K | cuesta 400 octetos de memoria convencional; el resto sale de la extendida |

**Dos decisiones:**

1. **Dentro del núcleo, no un MEMEXT.SYS con `DEVICE=`.** El gestor cuesta
   unos 1,4 K y, con `DOS=ALTO`, va arriba con el núcleo, así que no gasta
   memoria convencional; un controlador aparte se quedaría abajo y no podría
   subir el núcleo (que necesita A20 antes de que se carguen los
   controladores). Y lo útil es que la memoria esté ahí sin saber poner
   un `DEVICE=`: en un 286 con memoria extendida el gestor está siempre;
   `DOS=ALTO` es lo único que se pide. Lo que se paga: en un 8086 esos 1,4 K
   de código están en el núcleo sin servir para nada.
2. **Ni una instrucción de 286.** Mover memoria por encima del mega pide
   entrar en modo protegido, y en un 286 no hay manera limpia de volver: hay
   que reiniciar el procesador por el 8042 y recogerlo, y cada BIOS lo hace
   a su modo. Lo hace mejor la propia BIOS (INT 15h/87h), que existe en
   todos los AT y conoce su máquina. Así todo Hispa-8086 se sigue
   ensamblando con `cpu 8086`, y no hay ningún tramo que un 8088 pudiera
   ejecutar por error. La BIOS del AT cierra A20 al volver de copiar: por
   eso se la llama desde el puente, que está abajo, y es él quien la vuelve
   a abrir.

Probado en QEMU con 1, 2, 4 y 16 megas y en su máquina sin PCI (`isapc`):
A20 de verdad, 256 K movidos a la extendida y de vuelta y comparados, un
bloque que crece de 256 a 384 K sin perder nada, llenar la extendida y
soltarla, la HMA para un programa, y la batería entera con el núcleo arriba
(`MODO=alto`) y como un 8086 (`MODO=8086`). **Falta probarlo en un 286
real** y en PCjs con la BIOS del IBM AT: la puerta A20 de los AT de verdad
y la vuelta del modo protegido de su BIOS son justo lo que QEMU no reproduce.

## CONFIG.SYS

Al arrancar, antes de lanzar el intérprete, el núcleo lee `\CONFIG.SYS` de
la unidad de arranque, si existe (`h8086/config.inc`):

| | |
|---|---|
| `FILES=n` | cuántos archivos pueden estar abiertos a la vez: de 8 (lo que hay sin decir nada) a 64. Cada uno son 28 octetos de memoria residente |
| `BUFFERS=n` | cuántos sectores guarda la reserva de sectores: de 0 a 30; sin decir nada, 4 (ver «La reserva de sectores») |
| `DEVICE=archivo [parámetros]` | carga un controlador de dispositivo (ver «Dispositivos») |
| `DOS=ALTO` (o `HIGH`), `DOS=BAJO` (o `LOW`) | con `ALTO`, en un 286 con más de un mega, el núcleo se sube a la memoria alta (ver «El 286»); sin decir nada, se queda abajo |
| `PROCESADOR=8086` | hace como si el procesador fuera un 8086 aunque sea un 286 o más: ni A20, ni HMA, ni memoria extendida, ni INT 2Fh. Sirve para probar que todo va sin lo del 286 |
| `SHELL=ruta [parámetros]` | el intérprete que se lanza en vez de `\COMMAND.COM`, y lo que se le pasa; como en DOS, para que ejecute AUTOEXEC.BAT hay que ponerle `/P`. Si no se puede lanzar, se dice y se usa COMMAND.COM |
| `REM ...` | un comentario; las líneas vacías no cuentan |

Lo que no entiende lo dice (`CONFIG.SYS: no se entiende esta línea: ...`) y
sigue con la siguiente.

CONFIG.SYS se lee dos veces: primero todo menos los `DEVICE=`, que decide lo
que miden la tabla de archivos y la reserva, y luego los `DEVICE=`, que se
cargan detrás.

## Pasar archivos (PASAR)

Lo que más se echa de menos al restaurar una de estas máquinas es meter y
sacar archivos (ver el [estudio](ESTUDIO.md)). `PASAR` lo hace por el puerto
serie con **XMODEM**, un protocolo que tiene cualquier programa de terminal:
en el otro ordenador no hace falta nada nuestro.

```
PASAR ENVIAR archivo      lo manda
PASAR RECIBIR archivo     lo recibe
     /2                   por COM2 (si no, COM1)
     /V:19200             la velocidad, de 300 a 115200 (si no, 9600)
```

Qué hace falta: un cable serie cruzado («null modem») y, en un ordenador de
hoy, un adaptador de USB a serie. Siempre 8 bits, sin paridad, 1 bit de
parada.

En el otro ordenador:

- **Linux**: `sx archivo < /dev/ttyUSB0 > /dev/ttyUSB0` para enviar y
  `rx archivo < /dev/ttyUSB0 > /dev/ttyUSB0` para recibir (paquete `lrzsz`),
  tras `stty -F /dev/ttyUSB0 9600 raw -echo`; o `minicom`, con Ctrl+A S.
- **Windows**: Tera Term (Archivo › Transferir › XMODEM) o cualquier otro
  terminal con XMODEM.
- **Otro PC antiguo**: el propio `PASAR`, uno enviando y otro recibiendo.

Cómo va: XMODEM manda bloques de 128 octetos, cada uno con su número y su
comprobación (un CRC de 16 bits, o la suma de 8 bits de los XMODEM más
antiguos: PASAR entiende los dos); el que recibe contesta «bien» o «otra
vez», así que una línea con ruido no estropea el archivo. El último bloque
se rellena con el octeto 1Ah, que al recibir se quita: un archivo que acabe
de verdad en 1Ah los pierde (los de texto y los programas, no).

Pendiente: el puerto paralelo (más rápido, y lo tienen todas estas máquinas)
y un XMODEM de bloques de 1 K.

## CUENTAS (la hoja de cálculo, en el disquete de Oficina)

```
CUENTAS [hoja] [/G destino] [/I destino]
```

Una hoja de 256 columnas por 9.999 filas en la que solo ocupa memoria lo que
tiene algo. Usa números exactos en coma fija de 64 bits: 0,1 + 0,2 da 0,3, y
REDONDEAR(2,675;2) da 2,68. Admite fórmulas en español y a la manera de hoy
(`=SUMA(A1:A10)`), y también las de VisiCalc y 1-2-3 (`+A1`,
`@SUM(A1...A10)`). Solo recalcula lo que depende de lo cambiado, en el orden
de las dependencias, y avisa de las referencias circulares. Tiene deshacer
(Ctrl+Z); copiar con referencias relativas y fijas (`$A$1`); formatos de
euros, pesetas, tanto por ciento y fecha; ordenar e imprimir. Guarda en su
formato (.CTA), en .CSV con «;» y en .DIF, según la extensión. Con `/G` o
`/I` convierte o imprime sin pantalla. Ocupa 16,4 K (16.790 octetos). La historia de las hojas
de cálculo de entonces, lo que tomamos de ellas y lo que hacemos mejor están
en [CUENTAS.md](CUENTAS.md).

## Juegos (el disquete de Juegos)

Los juegos van en su propio disquete (`build/juegos8086-NNN.img`, sin
sistema, con las instrucciones para jugar en `JUEGOS.TXT`). Se arranca con
el de Hispa-8086 y se pone en B: (con una sola disquetera, el sistema lo
pide cuando hace falta). Los dos van a la misma velocidad en cualquier
máquina (los lleva el reloj del equipo, el 8254, no el procesador), se
juegan con teclado o palanca y no distinguen nada solo por el color: cada
cosa tiene su forma.

| Juego | Qué es |
|---|---|
| `DERRIBO.COM` | **Derribo**, un juego nuestro de pala, pelota y ladrillos, con diez muros propios. CGA de 320×200 a cuatro colores (y EGA y VGA), o texto con MDA y Hercules; monocromo con `/M` para los monitores de fósforo; ladrillos y premios que se distinguen por la forma y por una letra; también con ratón; la mejor puntuación en `DERRIBO.REC`. 6 K. Cómo se hacían estos juegos en el PC de 1981 a 1986, qué hace DERRIBO y cómo se prueba: [JUEGO-DERRIBO.md](JUEGO-DERRIBO.md) |
| `MARCIANA.COM` | **Marcianada**, de marcianitos: la flota que baja, la churrera y las murallas. CGA (o EGA, VGA), MDA o Hercules en modo texto, monocromo con `/M`; el récord en `MARCIANA.REC`. 6,2 K. Ver [JUEGO-MARCIANADA.md](JUEGO-MARCIANADA.md) |

## Cómo se construye y se prueba

```
make -C h8086            # los disquetes (sistema, Oficina y Juegos), en h8086/build/
make -C h8086 prueba     # los arranca en QEMU y comprueba la pantalla
```

Hace falta NASM, mtools, QEMU y socat, como para el resto del proyecto, y
`fsck.fat` (dosfstools) para repasar los discos desde fuera. La prueba va por
partes (treinta y tres, con las de PLUMA, CUENTAS y los juegos) y tarda
unos 55 minutos;
`SOLO="formatea partir" ./prueba/prueba.sh` corre solo las que se digan.

| Archivo | Qué es |
|---|---|
| `h8086/arranque.asm` | el sector de arranque (FAT12 y FAT16, disquete y disco duro) |
| `h8086/mbr.asm` | el sector de particiones del disco duro |
| `h8086/nucleo.asm` | el núcleo: arranque, el puente, INT 21h, consola, el equipo |
| `h8086/alta.inc` | en un 286: el gestor de memoria extendida (XMS 2.0) y la subida a la HMA |
| `h8086/config.inc` | CONFIG.SYS, y cargar los controladores (DEVICE=) |
| `h8086/dispositivos.inc` | la cadena de dispositivos: NUL, CON, AUX, PRN; las peticiones |
| `h8086/disco.inc` | sectores (también INT 25h y 26h), FAT, unidades, directorios, rutas y archivos abiertos |
| `h8086/archivos.inc` | las funciones de archivos y directorios de INT 21h |
| `h8086/proceso.inc` | la memoria por bloques y los programas (.COM, .EXE, lanzar, terminar) |
| `h8086/command.asm` | el intérprete de órdenes: COMMAND.COM |
| `h8086/ordenes/pasar.asm` | PASAR: archivos por el puerto serie (XMODEM) |
| `h8086/ordenes/formatea.asm` | FORMATEA: dar formato a disquetes y al disco duro |
| `h8086/ordenes/sistema.asm` | SISTEMA: dejar un disco de arranque |
| `h8086/ordenes/partir.asm` | PARTIR: las particiones del disco duro |
| `h8086/ordenes/util.inc` | lo común de las tres: pantalla, preguntar, sectores por la BIOS, copiar el sistema |
| `h8086/ordenes/mas.asm`, `ordenar.asm` | los filtros MAS y ORDENAR |
| `h8086/ordenes/revisar.asm`, `etiqueta.asm`, `atributo.asm`, `arbol.asm`, `xcopiar.asm` | REVISAR, ETIQUETA, ATRIBUTO, ARBOL y XCOPIAR |
| `h8086/ordenes/editar.asm` | EDITAR, el editor de texto |
| `h8086/ordenes/teclado.asm` | TECLADO, el teclado español (residente) |
| `h8086/ordenes/discoram.asm` | DISCORAM.SYS, el disco en memoria (controlador de bloque) |
| `h8086/prueba/hola.asm` | un programa .COM de prueba |
| `h8086/prueba/arch.asm` | la prueba de la interfaz de archivos, en doce pasos |
| `h8086/prueba/exe.asm` | un .EXE hecho a mano: reubicación, memoria y lanzar otro programa |
| `h8086/prueba/n8.asm` | prueba de INT 25h/26h y de FILES= |
| `h8086/prueba/BUCLE.BAT`, `SUB.BAT`, `SUB2.BAT` | los lotes de la prueba de FOR, LLAMAR y DESPLAZAR |
| `h8086/prueba/cuenta.asm`, `saludo.asm` | la cuenta de sectores leídos y un controlador de carácter, para la prueba |
| `h8086/prueba/partes_09.sh`, `partes_10.sh` | las partes de la prueba que llegaron con la 0.9 y con la 0.10 |
| `h8086/prueba/partes_11.sh`, `v86_flechas.js` | las teclas de cursor: en la línea de órdenes, BASIC, INKEY$ y EDITAR, con las teclas grises y las del teclado numérico, sin TECLADO y con él; y lo mismo en el emulador v86 |
| `h8086/prueba/xms.asm` | XMS.COM: prueba del gestor de memoria extendida, de A20 y de la HMA |
| `h8086/prueba/v86_teclado.js`, `v86_discos.js` | TECLADO y dos disqueteras en el emulador v86 de la web (opcional) |
| `h8086/programas/cuentas.asm`, `programas/cuentas/*.inc` | CUENTAS, la hoja de cálculo (ver [CUENTAS.md](CUENTAS.md)) |
| `h8086/prueba/partes_cuentas.sh` | las partes de la prueba de CUENTAS |
| `h8086/prueba/prueba.sh` | la prueba automática, por partes: `SOLO="partir sistema" ./prueba/prueba.sh` corre solo esas |
| `h8086/pon_arranque.py` | pone el sector de arranque conservando la BPB del disquete |
| `h8086/prueba/haz_duro.py` | hace el disco duro de prueba (una partición FAT16) |
| `h8086/prueba/xmodem.py` | «el otro ordenador» de la prueba de PASAR |
| `h8086/pluma/` | PLUMA, el procesador de textos (ver [PLUMA.md](PLUMA.md)), y su manual PLUMA.TXT |
| `h8086/prueba/partes_pluma.sh` | las partes de la prueba de PLUMA |
| `h8086/juegos/derribo.asm` | DERRIBO, el juego de pala y ladrillos (`DERRIBO /P`: la prueba del motor) |
| `h8086/juegos/marciana.asm` | MARCIANADA (con `-DPRUEBA`, `MARPRUEB.COM`: la prueba del motor) |
| `h8086/juegos/JUEGOS.TXT` | las instrucciones de los juegos, que van en el disquete de Juegos |
| `h8086/prueba/partes_juegos.sh`, `partes_marcianos.sh` | las partes de la prueba de DERRIBO y de MARCIANADA |

### Cómo reconoce el procesador

Cada prueba usa solo lo que ya se sabe que el procesador tiene:

1. `PUSH SP`: hasta el 80186 se guarda el valor ya restado; del 286 en
   adelante, el de antes. Separa los dos grupos.
2. En el grupo antiguo: un desplazamiento de 33 posiciones deja cero en el
   8086 y no en el 80186 (que solo mira cinco bits de la cuenta); `AAD` con
   base 16 distingue el V20 de NEC (que siempre usa base 10) del 8086.
3. En el grupo moderno: en un 286 en modo real no se pueden poner a uno los
   bits 12 a 14 de las banderas; en un 386, sí.
4. Ya en un 386 (y solo entonces, con sus instrucciones): el 486 deja cambiar
   el bit AC de las banderas de 32 bits.
