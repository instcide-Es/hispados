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

**Etapas 0 a 3 hechas y la 4 empezada** (`h8086/`, noviembre de 2026).
Versión 0.7.1.

- **Arranque**: sector de arranque FAT12 y núcleo (6,8 K en el disco). En
  memoria, el núcleo con sus variables, la copia de la FAT y la pila son unos
  14 K, y el intérprete de órdenes, otros 8 K: en una máquina de 640 K
  quedan 618 K para los programas, ensamblados
  con `cpu 8086`: el ensamblador rechaza cualquier instrucción posterior.
  Disquetes de 360 K, 720 K, 1,2 M y 1,44 M.
- **El equipo**: reconoce el procesador (8086/8088, V20/V30, 80186, 286, 386,
  486 o más) y la memoria; si la máquina puede con un Hispa mayor, lo
  aconseja.
- **INT 21h**, lo que un programa de DOS espera encontrar:
  - consola (01h, 02h, 06h a 0Bh), vectores (25h, 35h), versión (30h),
    terminar (00h, 4Ch, 4Dh, INT 20h);
  - **archivos**: crear (3Ch), abrir (3Dh), cerrar (3Eh), leer (3Fh),
    escribir y cortar (40h), borrar (41h), moverse (42h), atributos (43h),
    renombrar (56h), buscar con comodines (4Eh, 4Fh, 1Ah, 2Fh);
  - **directorios**: crear (39h), quitar (3Ah), cambiar (3Bh), cuál es (47h);
  - sitio libre (36h), fecha y hora (2Ah, 2Ch), volcar al disco (0Dh);
  - **memoria por bloques**, como en MS-DOS: pedir (48h), soltar (49h),
    cambiar el tamaño (4Ah);
  - **programas**: lanzar otro y esperar (4Bh), su código de salida (4Dh),
    qué programa corre (50h, 51h, 62h).
- **Disco**: tres unidades, A: y B: (disquetes) y **C:, el disco duro** (la
  primera partición FAT del primer disco); FAT12 y **FAT16** de hasta 32
  megas, de lectura y escritura, con subdirectorios que crecen. La FAT se lee
  por sectores, no entera, así que el núcleo ocupa 4 K menos de memoria.
  Detecta el cambio de disquete (un XT no avisa: si no hay archivos abiertos y
  pasan dos segundos, vuelve a leer sus medidas y su FAT).
- **Programas .COM y .EXE** (con su tabla de reubicación), cada uno con su
  prefijo (PSP), sus parámetros y su entorno (las variables y, detrás, su
  propia ruta). Un programa puede lanzar otro; al terminar se cierran sus
  archivos y se suelta su memoria.
- **El intérprete de órdenes es un programa aparte**, COMMAND.COM (4,7 K),
  que el núcleo lanza al arrancar y que se queda solo con la memoria que
  ocupa. Las órdenes llevan los mismos nombres que en el resto de la familia
  (el español y el clásico): AYUDA, VER, EQUIPO, DIR, MOSTRAR, COPIAR, BORRAR
  (con comodines), RENOMBRAR, CD, CREARDIR, BORRARDIR, LIMPIAR, ACTUALIZAR.
- **Archivos por lotes** (.BAT) con ECO, REM, PAUSA, los parámetros %0 a %9,
  IF [NOT] EXIST / ERRORLEVEL / a==b y GOTO, y **AUTOEXEC.BAT** al arrancar.
- **Redirección**: `orden > archivo`, `>> archivo` y `< archivo` (INT 21h/46h:
  un archivo hace de pantalla o de teclado para todos los programas).
- **PASAR**, la orden que lleva y trae archivos por el puerto serie con el
  protocolo XMODEM (ver más abajo «Pasar archivos»).
- **Prueba automática** (`make -C h8086 prueba`): 102 comprobaciones en QEMU,
  también sobre un disco duro de 20 megas con una partición FAT16 y con otro
  programa haciendo de «el otro ordenador» por el puerto serie.
  Un .EXE hecho a mano (`prueba/exe.asm`) comprueba la reubicación, la
  memoria y el lanzamiento de otro programa; otro (`prueba/arch.asm`) ejerce
  la interfaz de archivos en doce pasos; después se mira el disquete desde fuera: cada archivo tiene su
  contenido exacto y `fsck.fat` no encuentra ningún error, en las cuatro
  medidas.

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

**Lo que falta por comprobar:** PASAR por un puerto serie de verdad, un V20 y
un 80186 (para la parte fina de la detección) y, sobre todo, un XT real con su
disquetera.

**Límites de hoy:** particiones de hasta 32 megas, una sola por disco y un
solo disco duro; se arranca desde disquete (aún no hay FDISK, FORMAT ni SYS);
ocho archivos abiertos entre todos los programas, sin programas residentes ni la vieja interfaz de
archivos por FCB.

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
bits. (Cuando exista [Castel](../../README.md), el lenguaje de la familia,
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
| Hoy | | | unos 21 K: 12 del núcleo y 9 del intérprete |

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
| 3 | Intérprete de órdenes aparte (COMMAND.COM): órdenes en español, archivos .BAT, AUTOEXEC.BAT | **hecha**, con %1, IF, GOTO y redirección (faltan las tuberías «\|», FOR y CALL) |
| 4 | Disco duro: particiones, FAT16; CONFIG.SYS y controladores de dispositivo | **a medias**: C: con FAT16 hecho; faltan CONFIG.SYS y los controladores |
| 5 | Órdenes esenciales: FORMAT, SYS, FDISK, COPIAR, REVISAR, EDIT, **PASAR** (serie y paralelo) | **empezada**: PASAR por el puerto serie |
| 6 | Modo 286: HMA y memoria extendida | |
| 7 | Hispa-BASIC | |
| 8 | Hispa Escritorio y sus aplicaciones: Pluma, Cuentas, Fichas, Gestor | |
| 9 | Juegos, manual, versión estable 1.0 | |

Cada etapa se cierra con sus pruebas automáticas y probada en una máquina real.
La compatibilidad se mide como en Hispa-386: con programas de la época que
aporta cada usuario (nunca se distribuyen con el proyecto).

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

## Cómo se construye y se prueba

```
make -C h8086            # los cuatro disquetes, en h8086/build/
make -C h8086 prueba     # los arranca en QEMU y comprueba la pantalla
```

Hace falta NASM, mtools, QEMU y socat, como para el resto del proyecto.

| Archivo | Qué es |
|---|---|
| `h8086/arranque.asm` | el sector de arranque (FAT12) |
| `h8086/nucleo.asm` | el núcleo: arranque, INT 21h, consola, el equipo |
| `h8086/disco.inc` | sectores, FAT12, directorios, rutas y archivos abiertos |
| `h8086/archivos.inc` | las funciones de archivos y directorios de INT 21h |
| `h8086/proceso.inc` | la memoria por bloques y los programas (.COM, .EXE, lanzar, terminar) |
| `h8086/command.asm` | el intérprete de órdenes: COMMAND.COM |
| `h8086/ordenes/pasar.asm` | PASAR: archivos por el puerto serie (XMODEM) |
| `h8086/prueba/hola.asm` | un programa .COM de prueba |
| `h8086/prueba/arch.asm` | la prueba de la interfaz de archivos, en doce pasos |
| `h8086/prueba/exe.asm` | un .EXE hecho a mano: reubicación, memoria y lanzar otro programa |
| `h8086/prueba/prueba.sh` | la prueba automática |
| `h8086/pon_arranque.py` | pone el sector de arranque conservando la BPB del disquete |
| `h8086/prueba/haz_duro.py` | hace el disco duro de prueba (una partición FAT16) |
| `h8086/prueba/xmodem.py` | «el otro ordenador» de la prueba de PASAR |

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
