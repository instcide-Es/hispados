# Castel — el compilador

**Castel** es el lenguaje de sistemas en español (familia Pascal/Oberon) para escribir
los sistemas operativos de Rescoldo. En esta carpeta hay **dos compiladores que hacen lo
mismo**: el primero (etapa 0), `ccastel.py`, escrito en Python en el PC; y **el compilador
escrito en Castel** (`compilador/`), que corre dentro de Hispa-8086 y **se compila a sí
mismo**. Los dos leen Castel y sacan ensamblador de 16 bits, solo con instrucciones del
8086; y hay **un ensamblador propio**, también escrito en Castel, que da el mismo programa
que NASM. Es el espíritu de CIMIENTOS.md: algo vivo pronto, afinar sobre vivo.

Definición del lenguaje: [docs/CASTEL.md](docs/CASTEL.md) (y, para los 32 bits,
[docs/CASTEL32.md](docs/CASTEL32.md)). Estrategia y fases: [docs/CIMIENTOS.md](docs/CIMIENTOS.md).

Estado (18/11/2026): hechas la **Fase 1** (subconjunto mínimo), la **Fase 3** (crecer el
lenguaje), la **Fase 2** (el hito real: la orden **MAS** de Hispa-8086, escrita en Castel
y comprobada contra la de ensamblador), la **Fase 4 entera, el autohospedaje** (primero lo
que hacía falta —números de 32 bits, programas en varios archivos y una biblioteca escrita
en Castel— y después el **ensamblador propio**, el **compilador escrito en Castel** y el
**arranque en tres pasos**: el compilador, dentro de Hispa-8086, se compila a sí mismo y el
resultado, compilado otra vez, sale idéntico) y la **Fase 5: Castel de 32 bits**
([docs/CASTEL32.md](docs/CASTEL32.md)) — el modelo plano del 386 en modo protegido, con dos
salidas (`modelo núcleo`, un binario multiboot arrancable en QEMU; `modelo hispa32`, un
programa Win32 de consola para HW32). Los dos compiladores y ENSAMBLA entienden 32 bits, y
hay una **semilla de Hispa-Pentium** (`nucleo32/semilla.cas`), que ya es un núcleo de
verdad, todo en Castel ([../docs/PENTIUM.md](../docs/PENTIUM.md)): IDT, reloj, teclado y
excepciones; marcos físicos con el mapa de memoria de multiboot; **paginación** con el fallo
de página atendido; un montón con comprobación; **tareas** con el reloj repartiendo el
procesador; **programas del anillo 3** (el `modelo usuario`) con su llamada al sistema; y,
desde el 19/11, **disco duro (ATA) con FAT12/16/32**, programas que se cargan del disco y un
**intérprete de órdenes en el anillo 3** (DIR, TYPE, VER, CLS, MEM, AYUDA y lanzar programas);
y ese mismo día, **escribir en el disco** (COPY, DEL, REN, MD, RD, `>` y `>>`), CD, PATH,
archivos .BAT, la historia de la línea, Ctrl+C, el **teclado español** y los nombres largos.
Y, desde la 0.4, **un escritorio** (el primer entorno gráfico: modo de 32 bits, ratón,
ventanas, barra de tareas con «Inicio», menú Inicio, un Bloc de notas y una Consola), también
en Castel; y, desde la 0.5, con los menús del botón secundario, seleccionar, cortar, copiar y
pegar, iconos en el tapiz, «Mi equipo» y ventanas que cambian de tamaño.
Para él hay una **biblioteca de 32 bits** en Castel (`biblioteca32/`: textos, memoria y
escribir con formato).

## Qué hay aquí

- `ccastel.py` — el compilador de la etapa 0, en Python: léxico (UTF-8, palabras clave con
  sus acentos) → análisis sintáctico descendente → tipos y comprobaciones → NASM de 16 bits.
  Lee el programa y los módulos que importa, y lo deja todo en un solo `.asm`.
- `compilador/` — **el compilador escrito en Castel** (`castel.cas` y sus módulos): lo
  mismo que `ccastel.py`, parte por parte, para correr dentro de Hispa-8086. Ver más abajo.
  `compilador/pruebas/prueba_frente.sh` prueba aparte su lector y su gramática (el árbol
  que montan, contra el de `ccastel.py`).
- `ensambla.py` y `ensamblador/ensambla.cas` — **el ensamblador propio**: la referencia en
  Python y ENSAMBLA, el de verdad, en Castel. Dan el mismo archivo que NASM
  ([docs/ENSAMBLADOR.md](docs/ENSAMBLADOR.md)).
- `biblioteca/` — **la biblioteca, escrita en Castel**: `textos.cas`, `archivos.cas`,
  `memoria.cas`, `argumentos.cas` y `lejos.cas` (CASTEL.md §12). Se usan con `importar`.
- `biblioteca32/` — la de **32 bits**: `textos32.cas`, `mem32.cas` y `formato.cas` (un
  `printf` sencillo) (CASTEL32.md §7).
- `nucleo32/` — **Hispa-Pentium**: el núcleo (`semilla.cas` y sus módulos: `cpu`, `multiboot`,
  `marcos`, `paginas`, `monton`, `tareas`, `consola`, `ata`, `fat`, `fecha`, `usuario`; y, desde la
  0.4, el entorno gráfico: `video`, `grafico`, `letra`, `raton`, `ventanas`, `terminal` y
  `escritorio`; y `emergente`, los menús, desde la 0.5; y, desde la 0.6, los discos y el USB
  de los PC de verdad: `pci`, `ahci` (SATA), `discos`, `unidades` (las letras: C:, X:, M:),
  `usbbase`, `xhci`, `ehci`, `uhci`, `usb`, `usbhid` (teclado y ratón), `usbalm` (memorias) y
  `registro`; y, desde la 0.7, `ajustes`: los de un sistema instalado en el disco), el módulo de
  las llamadas al sistema para los programas (`llamadas.cas`), el intérprete de órdenes
  (`uordenes.cas`) y los demás programas de usuario (`uhola.cas`, `ueco.cas`, `umalo.cas`,
  `ucli.cas`, `uprueba.cas`, `uescribe.cas`, `ubucle.cas`; y los del escritorio: `ubloc.cas`, el
  Bloc de notas, `uconsola.cas`, la Consola, y `uequipo.cas`, Mi equipo, con `barramenu.cas`, la
  barra de menús de sus ventanas; y, desde la 0.7, `uinstala.cas`, el programa que **instala el
  sistema en el disco duro**, y `uasiste.cas`, el asistente del primer arranque); `disco.sh`, que hace la imagen de un
  disco FAT con todos ellos; `probar.sh`, que arranca el núcleo en QEMU (con ese disco) y le
  teclea órdenes (en el teclado español) y, en el escritorio, le mueve el ratón y saca
  capturas; `pantalla.py`, que deja lo que sale como se ve en la pantalla; `mira_captura.py`,
  que mira las capturas del escritorio; `hazletra.py`, que hace `letra.cas`; `usb.sh`, que
  hace una memoria USB con GRUB para probarlo en un PC, y `modo_grafico.py`, que hace la
  copia del núcleo que le pide a GRUB el modo gráfico ([../docs/PENTIUM.md](../docs/PENTIUM.md)).
- `ejemplos32/` — ejemplos de 32 bits (`plano.cas`, `biblio32.cas`, `hola32.cas`).
- `compilar.sh ENTRADA.cas [SALIDA.COM] [-I CARPETA]` — compila (con `ccastel.py`) a
  `.asm` y ensambla (con NASM) a `NOMBRE.COM`, o a `NOMBRE.EXE` si el programa es de
  «modelo grande» (`-I`: una carpeta más donde buscar módulos).
- `probar.sh PROGRAMA.COM [argumentos]` — lo ejecuta dentro de Hispa-8086 (QEMU) y enseña
  lo que escribe.
- `pruebas.sh` — **todas las pruebas** (errores, ejemplos, MAS contra MAS, CONTAR y NUMERA,
  el ensamblador propio contra NASM, el compilador en Castel contra `ccastel.py` y el
  arranque en tres pasos, y lo de 32 bits con Hispa-Pentium, su escritorio, sus discos y su
  USB, y la instalación en el disco duro). Tarda unos cuarenta y cinco minutos (`SIN_ARRANQUE=1 ./pruebas.sh` se salta el arranque; `SOLO=32 ./pruebas.sh`, solo los
  errores y lo de 32 bits, con el núcleo de Hispa-Pentium, su disco —también escribiendo en
  él—, su intérprete de órdenes y su teclado: unos diez minutos, casi todo en teclear las
  órdenes tecla a tecla; `SOLO=06 ./pruebas.sh`, solo lo de la 0.6: los discos SATA, las
  unidades y el USB; `SOLO=07 ./pruebas.sh`, solo lo de la 0.7: instalar en el disco duro,
  arrancar de él y el asistente del primer arranque). Con `AZAR=3 ./pruebas.sh`, además, tres programas de
  cuentas al azar de 16 bits (`azar.py`, unas 380 cuentas cada uno) y tres de 32 bits
  (`azar32.py`, unas 200), comparadas con lo que tienen que dar.
- `ejemplos/` — los programas de ejemplo; `ejemplos/esperado/` — lo que tiene que escribir
  cada uno; `ejemplos/errores/` — programas mal escritos, con el error que tienen que dar;
  `ejemplos/partes/` — los módulos que importan `modulos.cas` y algunos de los errores.
- `docs/` — la definición del lenguaje y la estrategia.

```
$ ./compilar.sh ejemplos/pares.cas
$ ./probar.sh ejemplos/PARES.COM
1 impar
2 par
...
$ ./pruebas.sh
...
== 128 bien, 0 mal ==
```

## Qué entiende Castel hoy

- **Módulo** con `principal` (que puede devolver el código de salida del programa).
- **Varios archivos**: `importar Módulo` trae lo `público` de otro archivo (constantes,
  tipos, variables y funciones), que se usa por su nombre o como `Módulo.nombre`. Todo
  acaba en un solo `.COM`, y de cada módulo solo entra lo que se usa.
- **Programas grandes**: con `modelo grande` el programa es un `.EXE` con un segmento de
  código por módulo y otro para los datos y la pila (CASTEL.md §11 bis).
- **Tipos**: `nat8`/`byte`, `nat16`, `ent16`, `bool`; **`nat32` y `ent32`** (32 bits en
  `DX:AX`, solo con instrucciones del 8086: variables, parámetros, resultados, arreglos,
  campos, todas las cuentas, comparaciones, conversiones, `escribir` y `para`);
  `arreglo [N] de T` (también de
  arreglos y de estructuras); `estructura ... fin`; `puntero a T`; `tipo Nombre = ...`.
- **Declaraciones**: `constante` (con cuentas entre constantes y textos), `variable`
  (globales que empiezan a cero o con valor inicial —número, lista, texto—; locales en la
  pila, también arreglos y estructuras), `función` con y sin valor, que puede llamarse a sí
  misma; parámetros por valor y **por referencia** (`ref`).
- **Sentencias**: `:=`; `si`/`sino si`/`sino`; `mientras`; `para ... desde ... hasta ...
  [paso n]` (también hacia atrás); `según`/`caso` (valores, listas e intervalos)/`si no`;
  `romper`/`seguir`; `devolver`; `ensamblador ... fin`.
- **Expresiones**: `+ - * div mod`; bits `yb ob xob nob << >>`; comparaciones; `y o no` con
  atajo; `a[i]`, `e.campo`, `p.campo`, `p[i]`, `valor(p)`, `dirección(x)`, `nulo`, cuentas
  con punteros (de elemento en elemento); conversiones `nat8() nat16() ent16() puntero()`;
  `tamaño()`. Con signo si hay un `ent16` por medio; si no, sin signo.
- **Textos**: como `puntero a byte` y `arreglo de byte`, acabados en 0, en la página 437.
- **El sistema, sin ensamblador**: `interrupción(n, registro: valor, ...)` con los registros
  por su nombre (devuelve AX), y `acarreo()`, `banderas()`, `reg_bx()`...; `salir(código)`;
  puertos (`meter_puerto`, `sacar_puerto`); memoria lejana (`leer_byte`, `poner_byte`,
  `leer_palabra`, `poner_palabra`); `escribir(...)` con varios argumentos y `escribir_car`.
- **Ensamblador en línea** dentro de una función o suelto en el módulo, que nombra lo de
  Castel entre llaves (`{variable}`, `{CONSTANTE}`, `{función}`) o por su nombre en NASM
  (`G_variable`, `F_función`).
- **Errores en español, con línea y columna** y la línea señalada; y comprobación de tipos
  (texto a número, número a puntero, bool con números, índice constante fuera del arreglo,
  constante que no cabe, campo que no existe, argumentos de más o de menos...):

```
ejemplos/errores/indice_fuera.cas:8:11: error: índice 10 fuera del arreglo (va de 0 a 9)
            a[10] := 2
              ^
```

## Cómo genera el código

`.COM` con `org 0x100`, `cpu 8086`, un solo segmento. Cada valor se calcula en AX; los
argumentos van a la pila (de derecha a izquierda) y la función llamada los quita (`ret n`);
marco con `BP`. No es un compilador que optimice, pero evita lo más tonto: opera sobre la
memoria cuando puede (`inc byte [G_col]`, `cmp byte [bx], 32`, `test`), compara y salta sin
fabricar el `bool`, no monta marco en las funciones que no lo necesitan, deja fuera las
rutinas de apoyo que no se usan y no mete en el archivo las variables sin valor inicial
(las pone a cero al arrancar). Los saltos condicionales que no llegan (más de 127 octetos)
los arregla NASM solo. Detalles, en CASTEL.md §10.

## La Fase 2: MAS, en Castel

`ejemplos/mas.cas` es la orden **MAS** (MORE) de Hispa-8086, reescrita entera en Castel a
partir de `h8086/ordenes/mas.asm`: mismos mensajes, mismas teclas, mismo comportamiento con
archivo, `<` y tubería. Sin una línea de ensamblador.

| | Octetos |
|---|---|
| `MAS.COM`, en ensamblador (h8086) | 305 |
| `MAS.COM`, en Castel | 488 |

`pruebas.sh` pone las dos en el mismo disquete de Hispa-8086 y les pasa lo mismo (un texto
corto y uno largo con tabuladores, líneas de 80 y de 100 letras, más de un bloque de 512
octetos y un 1Ah con texto detrás; por archivo, por `<` y por tubería; un archivo que no
existe; pausas contestadas con espacio y con Esc) y compara, octeto a octeto, lo que escriben
en la salida, el nivel de error con que acaban y la pantalla en cada una de las 15 pausas.
Sale todo igual.

## La Fase 4, primera parte: lo que el compilador necesita

Para escribir el compilador en Castel hacían falta tres cosas, y están hechas:

1. **Números de 32 bits** (`nat32`, `ent32`), para tamaños y posiciones de archivo y para
   las cuentas largas. En el 8086 van en dos registros (`DX:AX`): sumar y restar con el
   acarreo, comparar con `cmp`/`sbb`, y multiplicar, dividir y desplazar con rutinas de
   apoyo que solo entran en el programa si se usan. `ejemplos/largos.cas` lo prueba todo, y
   `azar32.py` fabrica cuentas al azar y las compara con lo que tienen que dar.
2. **Varios archivos**: `importar` y `público` (CASTEL.md §11). El compilador lee el programa
   y sus módulos y saca un solo `.asm`; no hay enlazador ni archivos intermedios.
3. **Una biblioteca escrita en Castel** (`biblioteca/`, CASTEL.md §12), sin ensamblador:
   `Textos` (longitud, copiar, comparar, concatenar, buscar, número↔texto), `Archivos`
   (abrir, crear, leer, grabar, cerrar, borrar, situar, y leer y escribir poco a poco
   con un `Lector` y un `Escritor`),
   `Memoria` (un montón dentro del segmento: reservar y soltar) y `Argumentos` (la línea de
   órdenes).

Y dos programas de verdad hechos con todo eso: **`ejemplos/contar.cas`, la orden CONTAR**
(cuenta líneas, palabras y octetos de uno o más archivos, como el `wc` de Unix; lee de los
archivos, de `<` o de una tubería, y lleva los totales en 32 bits) y **`ejemplos/numera.cas`,
NUMERA** (un TYPE con el número de cada línea delante). Cada una ocupa menos de 1,5 K, y
`pruebas.sh` las ejecuta en Hispa-8086 y compara lo que escriben con la misma cuenta hecha
en Python.

## La Fase 4, segunda parte: el autohospedaje

Tres piezas, y las tres están hechas y probadas (`pruebas.sh`):

### 1. El ensamblador propio

El compilador sigue sacando **texto de ensamblador** (el mismo que antes entendía solo
NASM), y un ensamblador nuestro lo convierte en el programa: `ensambla.py` (la referencia,
en Python) y **ENSAMBLA** (`ensamblador/ensambla.cas`, escrito en Castel, un `.COM` de
17 K). Entienden la parte de NASM que el compilador usa y dan **el mismo archivo que NASM,
octeto a octeto**: la forma más corta de cada instrucción y los mismos saltos cortos y
largos. Detalles en [docs/ENSAMBLADOR.md](docs/ENSAMBLADOR.md).

### 2. El compilador, escrito en Castel

`compilador/castel.cas` es `ccastel.py` pasado a Castel, función por función: **CASTEL**.

```
A:\> CASTEL PROG.CAS            (deja PROG.ASM; -o OTRO.ASM, -I CARPETA, --todo, --gasto)
A:\> ENSAMBLA PROG.ASM          (deja PROG.COM, o PROG.EXE si es de modelo grande)
A:\> PROG
```

| Archivo | Qué es | En `ccastel.py` |
|---|---|---|
| `base.cas` | los nombres («átomos»), los textos, el árbol y los errores | las listas y diccionarios de Python |
| `lexico.cas` | el lector | 1. `Lexico` |
| `sintactico.cas` | la gramática (el árbol) y los archivos de los módulos | 2. `Sintactico` y 5. `Cargador` |
| `cuentas.cas` | cuentas de 64 bits para las constantes | los enteros sin límite de Python |
| `tablas.cas` | los tipos, lo declarado en cada módulo, las locales | `Tipo`, `Modulo`, `Var` |
| `tipos.cas` | tipos y comprobaciones: `resuelve`, `const`, `tipo`, `compatible` | 3 y media 4 |
| `salida.cas` | las líneas que se generan y su limpieza | `self.lineas` y `limpia()` |
| `expr.cas` | generar: lugares, operandos, expresiones, llamadas | 4, de «lugares» a `gen_int` |
| `sent.cas` | generar: sentencias y funciones | 4, `gen_funcion`, `gen_sent`... |
| `apoyo.cas` | las rutinas de apoyo (lo escribe `haz_apoyo.py` desde `ccastel.py`) | `APOYO` |
| `programa.cas` | declarar, generar todo, ver qué va y escribirlo | `declara`, `genera` |

Saca **el mismo ensamblador que `ccastel.py`, letra a letra** (los 24 ejemplos, la
biblioteca entera, ENSAMBLA y él mismo) y **dice los mismos errores**, con su archivo, su
línea y su columna. Las decisiones que pedía un 8086 con 640 K:

- **No cabe en un `.COM`**: el compilador son unos 77 K de código. Por eso existe el
  **«modelo grande»** (CASTEL.md §11 bis): un `.EXE` con un segmento de código por módulo
  (el mayor ocupa 14 K) y otro de datos y pila. CASTEL.EXE ocupa 95 K.
- **Las tablas no caben en el segmento de datos**: van a **memoria lejana** (módulo `Lejos`:
  bloques pedidos al DOS, leídos y escritos con `leer_palabra`/`poner_palabra`). Todo son
  referencias de 16 bits a párrafos de esos bloques: cada nombre distinto se guarda una vez
  (un «átomo»), cada nudo del árbol ocupa 16 octetos, cada tipo y cada nombre declarado,
  otros 16. Pide unos 380 K de bloques; para compilarse a sí mismo usa menos de la mitad
  (`CASTEL ... --gasto` lo dice al acabar).
- **El árbol entero no cabe**: de cada módulo se guarda el árbol de las declaraciones; el
  cuerpo de cada función se analiza al cargar (para decir los errores de sintaxis en su
  sitio) y se suelta, y se vuelve a leer del archivo cuando toca generarla.
- **El ensamblador generado tampoco cabe** (700 K el del propio compilador): las líneas de
  cada función se guardan en memoria mientras se genera, se limpian (`limpia()` solo junta
  líneas vecinas de un mismo trozo) y van a un archivo temporal (`CASTEL.$$$`). Al final se
  lee dos veces: una para ver a quién nombra cada trozo vivo (qué va al programa) y otra
  para copiarlo.
- **Los nombres de archivo del DOS**: `importar Argumentos` busca `ARGUMENT.CAS` (las ocho
  primeras letras) en la carpeta del programa, en las de `-I` y en la del propio CASTEL.
- Sus límites (función de hasta unas 4000 líneas de ensamblador, 160 variables por función,
  24 módulos, 1000 piezas en una lista de valores iniciales...) los dice con un mensaje si
  se tocan. La columna de un error se queda en 255 en las líneas más largas.

### 3. El arranque en tres pasos

```
paso 1   ccastel.py   compila  compilador/castel.cas   ->  CASTEL.EXE       (en el PC, con NASM)
paso 2   CASTEL.EXE   compila  CASTEL.CAS              ->  C2.ASM -> C2.EXE (en Hispa-8086, con ENSAMBLA)
paso 3   C2.EXE       compila  CASTEL.CAS              ->  C3.ASM -> C3.EXE (en Hispa-8086, con ENSAMBLA)
```

Los pasos 2 y 3 dan **exactamente el mismo archivo** (el ensamblador y el `.EXE`), que es
además el del paso 1: el compilador se sostiene solo. `pruebas.sh` hace cada paso en un
Hispa-8086 con dos disquetes: en A: el sistema, el compilador que toca, ENSAMBLA y los
fuentes (y ahí deja CASTEL su temporal); en B:, lo que sale (el ensamblador del compilador
son 700 K: con su temporal no cabe en un solo disquete). También funciona en un disco duro
(C:). En QEMU, compilarse a sí mismo le lleva a CASTEL unos seis segundos, y a ENSAMBLA
ensamblar el resultado (37.000 líneas, 35 pasadas), tres.

Una cosa pendiente, que no es de Castel sino de Hispa-8086: después de que CASTEL compile
un programa, **en ese mismo arranque** leer archivos grandes va decenas de veces más despacio
(ensamblar el compilador pasa de tres segundos a cuatro minutos); reiniciando vuelve a ir
bien. Por eso `pruebas.sh` compila y ensambla en dos arranques. No se ha encontrado la causa
(no pasa con programas pequeños que crean, leen, se sitúan y borran archivos, ni por pedir
memoria): queda por mirar en el núcleo.

## Lo que NO entra todavía

Punteros lejanos como tipo y copiar arreglos y estructuras enteros. Ver CASTEL.md §13.

## Lo siguiente

1. **Usarlo**: llevar CASTEL y ENSAMBLA al disquete de Hispa-8086 y escribir en Castel más
   órdenes del sistema (ya no hace falta el PC para compilarlas).
2. **Que el lenguaje crezca escrito en sí mismo**: a partir de aquí cada mejora se hace en
   `compilador/` (y, mientras se quiera conservar la etapa 0, también en `ccastel.py`: las
   pruebas comparan los dos).
3. **Hispa-Pentium**: el núcleo de la gama XP, en Castel. Ya tiene paginación, tareas,
   anillo 3, disco con FAT (se lee y se escribe), un intérprete de órdenes en el anillo 3
   con sus archivos .BAT, Ctrl+C y el teclado español y, desde la 0.4, un **escritorio**
   (ventanas, ratón, barra de tareas, menú Inicio, Bloc de notas y Consola), todo en Castel
   ([../docs/PENTIUM.md](../docs/PENTIUM.md)); y, desde la 0.6, los discos SATA (AHCI), las
   unidades con letra y el USB (xHCI, EHCI y UHCI: teclado, ratón, concentradores y
   memorias); y, desde la 0.7, **se instala en el disco duro** (el programa INSTALAR, el
   arranque del disco con GRUB y el asistente del primer arranque). Lo siguiente: probarlo
   instalado en un PC de verdad; y crear nombres largos, tuberías y una caché de disco de
   verdad.
