# Castel — el compilador (etapa 0)

**Castel** es el lenguaje de sistemas en español (familia Pascal/Oberon) para escribir
los sistemas operativos de Rescoldo. Esta carpeta es el **primer compilador** (etapa 0):
escrito en Python en el PC, lee Castel y **saca ensamblador para NASM**, que se ensambla a un
`.COM` de DOS de 16 bits, solo con instrucciones del 8086. Es el espíritu de CIMIENTOS.md:
algo vivo pronto, afinar sobre vivo.

Definición del lenguaje: [docs/CASTEL.md](docs/CASTEL.md). Estrategia y fases:
[docs/CIMIENTOS.md](docs/CIMIENTOS.md).

Estado (17/11/2026): hechas la **Fase 1** (subconjunto mínimo), la **Fase 3** (crecer el
lenguaje), la **Fase 2** (el hito real: la orden **MAS** de Hispa-8086, escrita en Castel
y comprobada contra la de ensamblador) y la **primera parte de la Fase 4**: lo que hace
falta para que el compilador se pueda escribir en Castel (números de 32 bits, programas en
varios archivos y una biblioteca escrita en Castel). Lo siguiente es la segunda parte: el
ensamblador propio y el compilador en Castel.

## Qué hay aquí

- `ccastel.py` — el compilador: léxico (UTF-8, palabras clave con sus acentos) → análisis
  sintáctico descendente → tipos y comprobaciones → NASM de 16 bits para `.COM`. Lee el
  programa y los módulos que importa, y lo deja todo en un solo `.asm`.
- `biblioteca/` — **la biblioteca, escrita en Castel**: `textos.cas`, `archivos.cas`,
  `memoria.cas` y `argumentos.cas` (CASTEL.md §12). Se usan con `importar`.
- `compilar.sh ENTRADA.cas [SALIDA.COM] [-I CARPETA]` — compila a `.asm` y ensambla a
  `NOMBRE.COM` (`-I`: una carpeta más donde buscar módulos).
- `probar.sh PROGRAMA.COM [argumentos]` — lo ejecuta dentro de Hispa-8086 (QEMU) y enseña
  lo que escribe.
- `pruebas.sh` — **todas las pruebas** (errores, ejemplos, MAS contra MAS y CONTAR). Tarda
  cerca de un minuto. Con `AZAR=3 ./pruebas.sh`, además, tres programas de cuentas al azar
  de 16 bits (`azar.py`, unas 380 cuentas cada uno) y tres de 32 bits (`azar32.py`, unas
  200), comparadas con lo que tienen que dar.
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
== 85 bien, 0 mal ==
```

## Qué entiende Castel hoy

- **Módulo** con `principal` (que puede devolver el código de salida del programa).
- **Varios archivos**: `importar Módulo` trae lo `público` de otro archivo (constantes,
  tipos, variables y funciones), que se usa por su nombre o como `Módulo.nombre`. Todo
  acaba en un solo `.COM`, y de cada módulo solo entra lo que se usa.
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

Y un programa de verdad hecho con todo eso: **`ejemplos/contar.cas`, la orden CONTAR**
(cuenta líneas, palabras y octetos de uno o más archivos, como el `wc` de Unix; lee de los
archivos, de `<` o de una tubería, y lleva los totales en 32 bits). Ocupa menos de 1,5 K y
`pruebas.sh` la ejecuta en Hispa-8086 y compara lo que cuenta con lo que cuenta Python.

## Lo que NO entra todavía

Punteros lejanos como tipo, copiar arreglos y estructuras enteros, programas `.EXE`. Ver
CASTEL.md §13.

## Lo siguiente: la segunda parte de la Fase 4

1. **Decidir la salida** del compilador autohospedado: seguir sacando texto para NASM (hoy
   NASM hace además el trabajo de los saltos largos) o un ensamblador propio mínimo; en
   Hispa-8086 no hay NASM, así que para compilar *dentro* del sistema hace falta lo segundo.
2. **Reescribir** léxico → sintáctico → generador en Castel, compilarlo con `ccastel.py`
   (etapa 0) y, con el resultado, compilarse a sí mismo: las dos salidas tienen que ser
   idénticas. `pruebas.sh` vale tal cual para comprobar el compilador nuevo.
3. Lo que eso pida por el camino: casi seguro, más memoria que un segmento para las tablas
   (programas `.EXE` o punteros lejanos) y escribir en archivos con memoria intermedia.
