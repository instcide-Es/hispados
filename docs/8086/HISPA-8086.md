# Hispa-8086

*El sistema de la familia Hispa para los PC, XT y AT · noviembre de 2026*
*Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.*

Hispa-8086 es el sistema operativo de la familia [Rescoldo](../VISION.md) para
las máquinas más veteranas: las de **8086, 8088, V20, 80186 y 286**. Hecho
desde cero, en español, libre y todo en ensamblador, para que un PC o un XT de
256 K vuelva a servir: escribir, hacer cuentas, programar en BASIC y pasar los
archivos a un ordenador de hoy.

Este documento es el plan y el registro de las decisiones. El estudio en que
se apoya (qué se usaba en estas máquinas y qué se echa de menos) está en
[ESTUDIO.md](ESTUDIO.md).

## Dónde estamos

**Etapa 0, los cimientos: hecha** (`h8086/`, noviembre de 2026).

- Sector de arranque FAT12 (`arranque.asm`) y núcleo (`nucleo.asm`, 3,4 K),
  ensamblados con `cpu 8086`: el ensamblador rechaza cualquier instrucción
  posterior.
- Arranca desde disquete de 360 K, 720 K, 1,2 M y 1,44 M.
- Reconoce el procesador (8086/8088, V20/V30, 80186, 286, 386, 486 o más) y
  la memoria; si la máquina puede con un Hispa mayor, lo aconseja.
- Servicios de consola de INT 21h, vectores, versión y terminar; lee el
  directorio raíz y ejecuta programas .COM.
- Órdenes: AYUDA, VER, EQUIPO, DIR, TIPO, BORRAR, ACTUALIZAR.
- Prueba automática (`make -C h8086 prueba`): 36 comprobaciones en QEMU.

**Probado también en un procesador de 16 bits** (16/11/2026): en el emulador
8086tiny, que implementa un 8086/80186, arranca desde los disquetes de 360 K y
de 1,44 M, ejecuta el programa de prueba y lista el disco; se reconoce como
«80186 u 80188» (lo que ese emulador es) y, como debe, no aconseja cambiar de
sistema. QEMU, que es donde corre la prueba automática, no emula nada anterior
al 486.

**Lo que falta por comprobar de esta etapa:** un 8086 u 8088 estricto y un V20
(para la parte fina de la detección) y, sobre todo, un XT real con su
disquetera.

## Las decisiones

### 1. Dos núcleos en la familia, no uno por procesador

La familia agrupa por **clase de máquina**:

| Núcleo | Procesadores | Sistema |
|---|---|---|
| 16 bits, modo real | 8086, 8088, V20/V30, 80186/188, 286 | **Hispa-8086** |
| 32 bits | 386, 486 y superiores | **Hispa-DOS** (con Hispa Ventanas) |

Por qué no más:

- En modo real, un 286 es un 8086 rápido con dos cosas de más: los 64 K de
  memoria alta (HMA) y la memoria extendida. El mismo sistema lo detecta al
  arrancar y las aprovecha («modo 286»). Un sistema aparte para el 286 solo
  se justificaría por su modo protegido de 16 bits, que casi ningún programa
  de la época usó.
- Entre un 386 y un 486 no hay nada que el sistema tenga que hacer distinto.
- Cada sistema de más son pruebas, discos, manuales y actualizaciones que
  mantener, sin nada a cambio para quien lo usa.

«Hispa-286» no es, por tanto, otro producto: es Hispa-8086 en un 286.
(Decisión de Carlos, 16/11/2026, sobre la propuesta de tres ediciones del
15/11.)

### 2. Un núcleo nuevo, no una adaptación del de Hispa-DOS

Se midió antes de decidir: ensamblando Hispa-DOS con `cpu 8086`, el núcleo
(15.000 líneas, 33 K) tiene **1.026** instrucciones que un 8086 no entiende, y
el intérprete de órdenes (7.700 líneas, 25 K), **241**. Se podrían cambiar una
a una, pero el resultado seguiría siendo un sistema pensado para un 386 con 4
megas: 59 K residentes, más que MS-DOS 3.3 (54 K) en máquinas que a veces
tienen 256 K.

Hispa-8086 se escribe de nuevo, pequeño, reutilizando de Hispa-DOS lo que no
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

| | MS-DOS 3.3 | Hispa-DOS 0.9 | Hispa-8086 (objetivo) |
|---|---|---|---|
| Residente (núcleo + intérprete) | ~54 K | ~59 K | **menos de 32 K** |
| Hoy | | | 3,4 K (etapa 0) + variables |

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
sonido digital, sin CD: eso es de Hispa-DOS.

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

El nombre «Hispa Ventanas» se queda para el entorno gráfico de 32 bits.

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
  `ACTUALIZAR` en Hispa-DOS), con una sección por sistema y versión y sus
  requisitos: procesador mínimo, memoria, disco y edición.
- **Cada sistema mira la máquina** al arrancar y con `EQUIPO`.
- **Cada sistema recomienda el que le corresponde a esa máquina.** Hispa-8086
  en un 386 o un 486 funciona, pero aconseja pasar a Hispa-DOS con Hispa
  Ventanas (ya lo hace desde la etapa 0); Hispa-DOS en una máquina que no
  llegue dirá que su sitio es Hispa-8086. Es un consejo, una vez, sin
  insistir ni impedir nada: quien quiera un 486 con Hispa-8086 puede tenerlo.
- **`ACTUALIZAR` cambia de versión y también de sistema**, conservando los
  datos del usuario (como hace hoy entre versiones de Hispa-DOS).
- **Por dónde llega la actualización** depende de la máquina: por red donde
  la hay (Hispa-DOS); en estas, por disquete o por el cable serie o paralelo
  desde otro ordenador con `PASAR`, que es además la respuesta a lo que más
  se echa de menos (ver el estudio).
- **Los documentos son comunes**: .TXT y .CSV se abren en cualquier sistema
  de la familia.

## Etapas

| Etapa | Qué | Estado |
|---|---|---|
| 0 | Cimientos: arranque, núcleo mínimo, detección del equipo, .COM | **hecha** |
| 1 | Archivos por INT 21h (abrir, leer, escribir, crear, borrar), subdirectorios, FAT12 de escritura | |
| 2 | Memoria (bloques, 48h-4Ah), programas .EXE, EXEC completo, variables de entorno | |
| 3 | Intérprete de órdenes aparte (COMMAND.COM): órdenes en español, archivos .BAT, redirección | |
| 4 | Disco duro: particiones, FAT16; CONFIG.SYS y controladores de dispositivo | |
| 5 | Órdenes esenciales: FORMAT, SYS, FDISK, COPIAR, REVISAR, EDIT, **PASAR** (serie y paralelo) | |
| 6 | Modo 286: HMA y memoria extendida | |
| 7 | Hispa-BASIC | |
| 8 | Hispa Escritorio y sus aplicaciones: Pluma, Cuentas, Fichas, Gestor | |
| 9 | Juegos, manual, versión estable 1.0 | |

Cada etapa se cierra con sus pruebas automáticas y probada en una máquina real.
La compatibilidad se mide como en Hispa-DOS: con programas de la época que
aporta cada usuario (nunca se distribuyen con el proyecto).

## Cómo se construye y se prueba

```
make -C h8086            # los cuatro disquetes, en h8086/build/
make -C h8086 prueba     # los arranca en QEMU y comprueba la pantalla
```

Hace falta NASM, mtools, QEMU y socat, como para el resto del proyecto.

| Archivo | Qué es |
|---|---|
| `h8086/arranque.asm` | el sector de arranque (FAT12) |
| `h8086/nucleo.asm` | el núcleo: NUCLEO86.SYS |
| `h8086/prueba/hola.asm` | un programa .COM de prueba |
| `h8086/prueba/prueba.sh` | la prueba automática |
| `h8086/pon_arranque.py` | pone el sector de arranque conservando la BPB del disquete |

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
