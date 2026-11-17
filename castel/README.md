# Castel — el compilador (etapa 0)

**Castel** es el lenguaje de sistemas en español (familia Pascal/Oberon) para escribir
los sistemas operativos de Rescoldo. Esta carpeta es el **primer compilador** (etapa 0):
escrito en Python en el PC, lee Castel y **saca ensamblador para NASM**, que se ensambla a un
`.COM` de DOS de 16 bits, solo con instrucciones del 8086. Es el espíritu de CIMIENTOS.md:
algo vivo pronto, afinar sobre vivo.

Definición del lenguaje: [docs/CASTEL.md](docs/CASTEL.md). Estrategia y fases:
[docs/CIMIENTOS.md](docs/CIMIENTOS.md).

Estado (17/11/2026): hechas la **Fase 1** (subconjunto mínimo), la **Fase 3** (crecer el
lenguaje) y la **Fase 2** (el hito real: la orden **MAS** de Hispa-8086, escrita en Castel
y comprobada contra la de ensamblador). Lo siguiente es la Fase 4, el autohospedaje.

## Qué hay aquí

- `ccastel.py` — el compilador: léxico (UTF-8, palabras clave con sus acentos) → análisis
  sintáctico descendente → tipos y comprobaciones → NASM de 16 bits para `.COM`.
- `compilar.sh ENTRADA.cas [SALIDA.COM]` — compila a `.asm` y ensambla a `NOMBRE.COM`.
- `probar.sh PROGRAMA.COM [argumentos]` — lo ejecuta dentro de Hispa-8086 (QEMU) y enseña
  lo que escribe.
- `pruebas.sh` — **todas las pruebas** (errores, ejemplos y MAS contra MAS). Tarda unos 40 s.
  Con `AZAR=3 ./pruebas.sh`, además, tres programas de cuentas al azar (`azar.py`): unas
  380 cuentas cada uno, comparadas con lo que tienen que dar.
- `ejemplos/` — los programas de ejemplo; `ejemplos/esperado/` — lo que tiene que escribir
  cada uno; `ejemplos/errores/` — programas mal escritos, con el error que tienen que dar.
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
- **Tipos**: `nat8`/`byte`, `nat16`, `ent16`, `bool`; `arreglo [N] de T` (también de
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

## Lo que NO entra todavía

`nat32/ent32`, punteros lejanos como tipo, varios módulos (`público`, importar), copiar
arreglos y estructuras enteros, programas `.EXE`. Ver CASTEL.md §11.

## Lo siguiente: Fase 4, el autohospedaje

Reescribir el compilador en Castel. Lo que hace falta antes, por orden:

1. **Leer y escribir archivos y la línea de órdenes** desde Castel: ya se puede (con
   `interrupción`), falta empaquetarlo en una pequeña biblioteca (abrir, leer, escribir,
   cerrar; textos: longitud, copiar, comparar, número↔texto).
2. **Varios archivos** (módulos con `público`/importar), o al menos «incluir»: el compilador
   no cabe cómodo en un solo fuente.
3. **Memoria**: las tablas de símbolos del compilador piden más de 64 K o una reserva de
   memoria sencilla (un montón); y casi seguro `nat32` para tamaños y posiciones de archivo.
4. **Decidir la salida** del compilador autohospedado: seguir sacando texto para NASM (hoy
   NASM hace además el trabajo de los saltos largos) o un ensamblador propio mínimo; en
   Hispa-8086 no hay NASM, así que para compilar *dentro* del sistema hace falta lo segundo.
5. **Reescribir** léxico → sintáctico → generador en Castel, compilarlo con `ccastel.py`
   (etapa 0) y, con el resultado, compilarse a sí mismo: las dos salidas tienen que ser
   idénticas. `pruebas.sh` vale tal cual para comprobar el compilador nuevo.
