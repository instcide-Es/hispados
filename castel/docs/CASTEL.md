# Castel — definición del lenguaje

*Castel: un corte de «Castellano». Un lenguaje de sistemas en español, de la familia Pascal/Oberon,
para escribir sistemas operativos. Este documento dice cómo se escribe y qué hace cada cosa;
es lo que entiende hoy el compilador (`ccastel.py`). Primera versión: 15/11/2026 (Fase 0).
Puesto al día el 17/11/2026 con la Fase 3 (arreglos, estructuras, punteros, `ref`, `paso`,
`según`, bits, `interrupción`), la Fase 2 (la orden MAS, escrita en Castel) y la primera parte
de la Fase 4 (números de 32 bits, programas en varios archivos y la biblioteca escrita en
Castel).*

Regla de oro: **el núcleo, pequeño.** Lo que el lenguaje no sabe decir se hace en un bloque
`ensamblador`, y lo que queda para más adelante está en la sección 13.

---

## 1. Forma de un programa

Un fichero fuente es un **módulo**. Dentro van las declaraciones (constantes, tipos, variables,
funciones y, si hace falta, bloques de ensamblador), en el orden que se quiera, con una
condición: **las constantes y los tipos se declaran antes de usarlos** (las funciones y las
variables globales se pueden usar antes de su declaración).

```
módulo Saludo

    constante LIMITE = 10

    función principal()
        variable i: ent16
        para i desde 1 hasta LIMITE hacer
            escribir(i, "\n")
        fin
    fin

fin Saludo.
```

- `módulo <Nombre>` abre; `fin <Nombre>.` cierra (el nombre tiene que coincidir).
- La función `principal()` es por donde empieza el programa. No lleva parámetros. Si lleva
  tipo (`función principal(): byte`), lo que devuelve es el código de salida del programa
  (el «nivel de error» del DOS); si no, el programa acaba con 0.
- Un programa puede repartirse en **varios archivos**: el que tiene `principal()` es el
  programa, y los demás son módulos que él trae con `importar` (sección 11).

---

## 2. Cómo se escribe (léxico)

- **Mayúsculas y minúsculas importan**: `Dato` y `dato` son distintos. Las palabras clave
  van en minúscula y con sus acentos (`módulo`, `función`, `según`).
- **Comentarios**: de línea, `;` hasta el fin de línea (como en ensamblador); de bloque,
  `(* ... *)`, que se pueden anidar.
- **Identificadores**: letra (también las acentuadas y la `ñ`) o `_`, seguida de letras,
  dígitos o `_`: `tamaño`, `n_filas`, `PantallaTexto`.
- **Palabras reservadas** (no valen como nombre): `módulo fin constante variable función
  devolver tipo si entonces sino mientras hacer para desde hasta paso romper seguir según caso
  ensamblador arreglo estructura puntero público importar y o no yb ob xob nob div mod cierto
  falso nulo`. Ojo con `y`, `o`, `no`, `desde`, `paso`...: una variable no puede llamarse `y`.
  Tampoco se pueden usar como nombre los tipos básicos ni las funciones propias del lenguaje
  (sección 8).
- **Números**: decimal (`42`), hexadecimal (`0xB800`) y carácter (`'A'` es 65; la letra se
  toma de la página 437, la del PC: `'ñ'` es 164). Caben en 32 bits (hasta `4294967295`); el
  que no cabe en 16 es, él solo, un número de 32 bits (sección 6).
- **Textos**: `"entre comillas dobles"`, en la página 437, terminados en un 0 que pone el
  compilador. Dentro de un texto o de un carácter: `\n` (10), `\r` (13), `\t` (9), `\0`,
  `\e` (27, Esc), `\"`, `\'`, `\\` y `\xHH` (el octeto HH en hexadecimal).
- **Una sentencia por línea**; no hay `;` al final. Los bloques se cierran con `fin`. En una
  misma línea se puede cerrar un bloque (`si x = 0 entonces devolver fin`). Dentro de unos
  paréntesis de llamada se puede cambiar de línea después de una coma.

---

## 3. Tipos

### Sencillos
| Tipo    | Qué es                                   | Octetos |
|---------|-------------------------------------------|---------|
| `nat8`  | natural (sin signo) de 8 bits (0..255)    | 1       |
| `byte`  | = `nat8`                                  | 1       |
| `nat16` | natural de 16 bits (0..65535)             | 2       |
| `ent16` | entero con signo de 16 bits               | 2       |
| `nat32` | natural de 32 bits (0..4294967295)        | 4       |
| `ent32` | entero con signo de 32 bits (±2147483647) | 4       |
| `bool`  | `cierto` o `falso`                        | 1       |

En modo real la palabra de la máquina es de 16 bits: `nat16`/`ent16` son los de a diario;
`byte` es para la memoria, los textos y el hardware; y `nat32`/`ent32`, para lo que no cabe en
65535: el tamaño de un archivo, una posición dentro de él, un sector del disco, una cuenta
larga. Los de 32 bits valen para todo lo que valen los demás (variables, parámetros,
resultados de funciones, elementos de arreglos, campos, `ref`, punteros a ellos), pero cuestan
más: cada cuenta son dos instrucciones, o una llamada a una rutina (multiplicar, dividir). Por
eso no son los de a diario: se usan donde hacen falta.

### Arreglos
```
variable tabla: arreglo [10] de nat16          ; diez elementos: tabla[0] ... tabla[9]
variable rejilla: arreglo [3] de arreglo [4] de byte      ; rejilla[i][j]
```
El tamaño es una constante. Los elementos van de `0` a `N-1`. El índice puede ser cualquier
cuenta; si es una constante, el compilador comprueba que cae dentro (con un índice calculado
no hay comprobación al ejecutar: es un lenguaje de sistemas).

### Estructuras
```
tipo Fecha = estructura
    día, mes: byte
    año: nat16
fin
```
Los campos se guardan seguidos, en el orden en que se escriben y sin huecos (así una
estructura puede describir tal cual un sector de disco o una cabecera). Se llega a un campo
con el punto: `hoy.año`, `socios[i].alta.mes`.

### Punteros
```
variable p: puntero a nat16
variable n: puntero a Nodo
```
Un puntero guarda una dirección de memoria: en modo real, **cercana** (16 bits, dentro del
segmento del programa). `nulo` es el puntero que no apunta a nada (0). Con un puntero se puede:
- `valor(p)` — lo apuntado (se lee y se le asigna: `valor(p) := 5`).
- `p[i]` — el elemento número `i` contando desde donde apunta (`p[0]` es `valor(p)`).
- `p.campo` — si apunta a una estructura, su campo directamente (sin `valor`).
- `p + n`, `p - n` — otro puntero, `n` **elementos** más allá (no octetos): con un `puntero a
  nat16`, `p + 1` es la palabra siguiente. Dos punteros a byte se pueden restar.
- comparar con `=`, `<>`, `<`... y con `nulo`.

Un tipo puede apuntarse a sí mismo (`sig: puntero a Nodo` dentro de `Nodo`).

### Tipos con nombre
`tipo <Nombre> = <tipo>` le pone nombre a cualquier tipo: `tipo Palabra = nat16`,
`tipo Lista = arreglo [3] de Socio`. Dos estructuras distintas son tipos distintos aunque
tengan los mismos campos; para lo demás, el nombre es solo un nombre.

### Textos
Un texto es una ristra de bytes acabada en 0. No hay un tipo aparte:
- un texto escrito (`"hola"`) vale como `puntero a byte` (la dirección de sus letras);
- para tener sitio donde cambiarlo, un `arreglo [N] de byte`, que puede llevar valor inicial
  (`variable nombre: arreglo [16] de byte = "mundo"`);
- el nombre de un arreglo, usado donde se espera un puntero, es la dirección de su primer
  elemento: a una función con un parámetro `t: puntero a byte` se le puede pasar tanto
  `"hola"` como `nombre`. `escribir()` enseña como texto los dos.

---

## 4. Declaraciones

```
constante <NOMBRE> = <cuenta con constantes o texto>
tipo      <Nombre> = <tipo>
variable  <nombre>: <tipo>
variable  a, b, c: ent16                        ; varias del mismo tipo
variable  n: nat16 = 5                          ; con valor inicial
variable  t: arreglo [4] de nat16 = (1, 2, 3)   ; lista: lo que falte, a cero
variable  s: arreglo [8] de byte = "hola"       ; texto: lo que falte, a cero

función <nombre>(<parámetros>): <tipo>          ; función que devuelve un valor
    ...
    devolver <expresión>
fin

función <nombre>(<parámetros>)                  ; procedimiento (no devuelve nada)
    ...
fin
```

- **Constantes**: un número, un texto o una cuenta hecha con números y otras constantes
  (`constante CELDAS = FILAS * COLUMNAS`, `constante MASCARA = 0x0F << 4`). Se calculan al
  compilar. Dentro de una función también se pueden declarar, pero valen para todo el módulo.
- **Variables globales** (las del módulo): las que no llevan valor inicial **empiezan a
  cero** (y no ocupan sitio en el archivo del programa). El valor inicial tiene que ser
  constante; puede ser una dirección (`= dirección(otra)`, `= "texto"` para un puntero).
- **Variables locales** (dentro de una función): viven en la pila mientras dura la llamada y
  **no empiezan a cero**. Se declaran en cualquier punto de la función y valen para toda ella.
  Una sencilla puede llevar valor inicial (`variable i: nat16 = 0`), que es una asignación.
  También pueden ser arreglos y estructuras.
- **`público`** delante de `constante`, `tipo`, `variable` o `función` (solo en las
  declaraciones del módulo, no dentro de una función): eso lo pueden usar los módulos que
  importen este (sección 11). Lo que no lo lleva es solo de su archivo.
- **Parámetros**: `nombre: tipo`, separados por comas. Se pasan **por valor** (la función
  recibe una copia). Con `ref` delante se pasan **por referencia**: la función trabaja sobre
  la variable del que llama y puede cambiarla.
  ```
  función cambia(ref a: ent16, ref b: ent16)
  ```
  A un `ref` hay que pasarle una variable (o un elemento, o un campo) de su mismo tipo. Los
  arreglos y las estructuras se pasan siempre con `ref` (no se copian).
- Una función devuelve un tipo sencillo (también `nat32` y `ent32`) o un puntero. Puede
  llamarse a sí misma.

---

## 5. Sentencias

### Asignación
```
i := 0
tabla[i + 1] := tabla[i] * 2
hoy.año := 2026
valor(p) := 'A'
```
El símbolo es `:=`. `=` es solo comparar, nunca asignar. Los arreglos y las estructuras no se
asignan enteros (elemento a elemento, o campo a campo).

### Decisión
```
si <condición> entonces
    ...
sino si <condición> entonces
    ...
sino
    ...
fin
```

### Bucles
```
mientras <condición> hacer
    ...
fin

para <variable> desde <inicio> hasta <fin> hacer            ; de uno en uno
    ...
fin

para <variable> desde <inicio> hasta <fin> paso <n> hacer   ; de n en n; n puede ser negativo
    ...
fin
```
- La variable del `para` es una variable numérica sencilla (local o global), de cualquier
  tamaño: también `nat32` y `ent32`. El `paso` es una constante distinta de 0; si es negativo
  se cuenta hacia atrás (`desde 10 hasta 1 paso -1`).
- El `<fin>` se calcula **una sola vez**, antes de empezar. Si ya de entrada se ha pasado, no
  se entra ninguna vez.
- Se puede llegar hasta el borde del tipo sin que se desboque (`para b desde 0 hasta 255`,
  con `b: byte`, da sus 256 vueltas y acaba; y lo mismo hasta `4294967295` con un `nat32`).
- `romper` sale del bucle; `seguir` salta a la vuelta siguiente.

### Elegir entre casos
```
según <expresión>
    caso 1: escribir("uno")                 ; lo que sigue a los dos puntos, en la misma línea
    caso 2, 3:                              ; ...o debajo, las líneas que hagan falta
        escribir("dos ")
        escribir("o tres")
    caso 'a'..'z', 'A'..'Z': letras := letras + 1         ; intervalos, con «..»
    si no:
        escribir("otro")
fin
```
La expresión es un número de 16 bits (uno de 32 se convierte antes: `según nat16(n)`); los
valores de los casos, constantes (sin repetir). Se ejecuta el
caso que coincide y se sigue después del `fin` (no hay que poner nada para no «caer» al caso
siguiente). `si no:` es opcional y va el último. `según` no es un bucle: un `romper` dentro de
él sale del bucle que lo envuelve.

### Devolver y llamar
```
devolver <expresión>        ; en una función con tipo
devolver                    ; en un procedimiento (termina antes)
ordena(lista)               ; una llamada como sentencia
```

### Bajar al metal: ensamblador en línea
```
ensamblador
    cli
    mov al, 0x20
    out 0x20, al
    sti
fin
```
Entre `ensamblador` y la línea `fin` va ensamblador de NASM tal cual. Es la válvula de escape:
**lo que el lenguaje no sepa expresar, se hace a mano aquí.** Reglas:

- **Para nombrar cosas de Castel** se escribe el nombre entre llaves, y el compilador pone lo
  que corresponde:
  | Se escribe      | Si es...                    | Queda                        |
  |-----------------|-----------------------------|------------------------------|
  | `{cuenta}`      | una variable global         | `[G_cuenta]`                 |
  | `{n}`           | un parámetro                | `[bp+4]`, `[bp+6]`...        |
  | `{r}`           | una variable local          | `[bp-2]`...                  |
  | `{LIMITE}`      | una constante numérica      | el número                    |
  | `{AVISO}`       | una constante de texto      | la etiqueta del texto        |
  | `{dobla}`       | una función                 | `F_dobla` (para `call`)      |
  | `{n+2}`         | una variable, 2 octetos más allá | `[bp+6]`, `[G_n+2]`...  |

  (`mov ax, {n}`, `inc word {cuenta}`, `mov dx, {AVISO}`. De un parámetro `ref`, `{x}` es
  la palabra donde está **la dirección** de la variable. De una variable de 32 bits, `{n}`
  es su palabra baja y `{n+2}` la alta. Lo de otro módulo, con su nombre: `{Textos.longitud}`.)
- Los **nombres en NASM**, por si se prefiere escribirlos directamente: una variable global
  `x` es la etiqueta `G_x` (o sea, `[G_x]` su contenido); una función `f` es `F_f`. Los
  nombres con acentos valen igual (`G_año`). Eso es en el programa; en un módulo importado
  llevan el nombre del módulo (`F_Textos__longitud`, `G_Archivos__error_dos`), así que dentro
  de un módulo es mejor escribir siempre `{nombre}`, que sale bien en los dos casos.
- Un bloque puede ir **dentro de una función** o **suelto en el módulo** (para rutinas
  enteras en ensamblador o tablas de datos; se ensambla en el sitio donde está escrito).
- Hay que **conservar** `BP`, `SP`, `DS`, `SS` y la bandera de dirección a cero. `AX`, `BX`,
  `CX`, `DX`, `SI`, `DI` y `ES` se pueden gastar. Si el bloque está en una función que
  devuelve un valor y no hay `devolver`, vale lo que el bloque deje en `AX` (en `DX:AX` si
  devuelve un número de 32 bits).
- Las **etiquetas** propias, con punto delante (`.otra:`): son locales y valen dentro de su
  bloque. Sin punto, que no empiecen por `L_`, `F_`, `G_`, `T_`, `R_` ni `X_` (son los
  nombres que pone el compilador).

---

## 6. Expresiones

- **Aritméticas**: `+  -  *  div  mod` (`div` = división entera, `mod` = resto).
- **De bits**: `yb` `ob` `xob` (y, o, o-exclusivo bit a bit), `nob` (invierte los bits),
  `<<` `>>` (desplazar a la izquierda y a la derecha: de 0 a 15 bits uno de 16, de 0 a 31
  uno de 32).
- **Comparaciones**: `=  <>  <  <=  >  >=` (dan `bool`; no se encadenan: `a < b y b < c`).
- **Lógicas**: `y`  `o`  `no`, sobre `bool`. Con atajo: en `a y b`, si `a` es falso, `b` ni se
  mira (vale para escribir `p <> nulo y valor(p) = 0`).
- **Unarias**: `-x`, `nob x`, `no c`.
- **Acceso**: `a[i]`, `e.campo`, `valor(p)`, `nombre(argumentos)`.
- **Agrupar**: con paréntesis.

Precedencia, de más fuerte a más floja:

| Nivel | Operadores                              |
|-------|------------------------------------------|
| 1     | `a[i]`  `e.campo`  llamada               |
| 2     | `-x`  `nob x`                            |
| 3     | `*`  `div`  `mod`  `yb`  `<<`  `>>`      |
| 4     | `+`  `-`  `ob`  `xob`                    |
| 5     | `=  <>  <  <=  >  >=`                    |
| 6     | `no`                                     |
| 7     | `y`                                      |
| 8     | `o`                                      |

Así `x yb 0xFF = 27` es `(x yb 0xFF) = 27`, y `n >> 4 yb 0x0F` es `(n >> 4) yb 0x0F`. Dentro
de un mismo nivel se va de izquierda a derecha.

### De 16 o de 32 bits, con signo o sin signo
Cada cuenta (cada `+`, cada `*`, cada comparación) se hace **en 16 bits, o en 32 si alguno de
sus dos operandos es de 32** (`nat32`, `ent32` o un número escrito que no cabe en 16, como
`100000`); el otro, si es más corto, se ensancha antes: con su signo si es `ent16`, con ceros
si es `byte` o `nat16`. Un `byte` se ensancha con ceros al leerlo y se recorta al guardarlo
(`250 + 10` en un byte da 4).

**Ojo, que es la trampa de siempre:** el tamaño de la cuenta lo ponen los operandos, no el
sitio donde se guarda. Con `a` y `b` de 16 bits y `total` de 32,
```
total := a * b              ; la cuenta es de 16 bits: si pasa de 65535, se recorta
total := nat32(a) * b       ; la cuenta es de 32 bits: sale entera
```
Sumar, restar y multiplicar dan los mismos bits con signo o sin él; donde cambia es al
**comparar, dividir, desplazar a la derecha y enseñar**:

- si alguno de los dos operandos es `ent16` o `ent32`, la operación es **con signo**;
- si no (`byte`, `nat16`, `nat32`, punteros), **sin signo**;
- un número escrito tal cual (`5`, `-1`, `100000`) se adapta al otro operando.

`>>` sobre un `ent16` o un `ent32` conserva el signo; sobre un natural mete ceros. En los
desplazamientos manda lo que se desplaza: `n << 20` es de 32 bits si `n` lo es, y la cuenta
de bits puede ser de cualquier tipo. Mezclar en una misma comparación un natural grande con
un entero con signo da lo que da la máquina (40000 visto con signo en 16 bits es negativo):
mejor no mezclarlos, o convertir antes.

Dividir con signo redondea hacia cero y el resto lleva el signo del dividendo (`-7 div 2` es
`-3` y `-7 mod 2` es `-1`), igual en 16 que en 32 bits.

Las cuentas **entre constantes** las hace el compilador con el tamaño que haga falta:
`constante DIA = 24 * 60 * 60` es 86400, un número de 32 bits.

### Comprobación de tipos
El compilador no deja:
- guardar un texto o un puntero en un número, ni un número en un puntero (hay que decirlo:
  `nat16(p)`, `puntero(n)`);
- mezclar `bool` con números (`si n entonces` no vale: `si n <> 0 entonces`; `y`/`o` son
  para condiciones y `yb`/`ob` para bits);
- mezclar punteros a tipos distintos, ni pasar a un `ref` algo de otro tipo;
- una constante que no cabe (`b := 256` con `b: byte`), un índice constante fuera del
  arreglo, un campo que no existe, llamar con argumentos de más o de menos, usar como valor
  lo que no devuelve nada, dividir por una constante que es 0.

Entre `byte`, `nat16`, `ent16`, `nat32` y `ent32` se asigna libremente (se recorta o se
ensancha): guardar uno de 32 bits en una variable de 16 se queda con la palabra baja, sin
avisar (si es una constante que no cabe, sí: es un error). Lo mismo al pasar un parámetro o
al devolver un valor.

### Conversiones
```
nat8(x)  byte(x)    ; se queda con el byte bajo
nat16(x)            ; los 16 bits bajos, vistos sin signo (también de un puntero)
ent16(x)            ; los 16 bits bajos, vistos con signo
nat32(x)            ; x, ensanchado a 32 bits (con su signo si x es ent16), visto sin signo
ent32(x)            ; lo mismo, visto con signo
puntero(x)          ; un número (o un puntero a otra cosa) como puntero: vale para cualquier
                    ; «puntero a ...» (p := puntero(0x81))
```
Para partir uno de 32 bits en sus dos palabras: `nat16(n)` es la baja y `nat16(n >> 16)` la
alta; y para juntarlas, `nat32(alta) << 16 ob baja`.

---

## 7. Llamar al sistema: `interrupción`

```
interrupción(<número>, <registro>: <valor>, <registro>: <valor>, ...)
```
Carga los registros que se digan, llama a la interrupción y **devuelve AX**. El número es una
constante. Los registros se dan **por su nombre**, en cualquier orden y solo los que hagan
falta: `ax bx cx dx si di`, sus mitades `ah al bh bl ch cl dh dl`, y `es` y `ds`.

```
interrupción(0x21, ah: 2, dl: 'A')                          ; escribir una letra (DOS)
tecla := interrupción(0x16, ah: 0)                          ; esperar una tecla (BIOS)
leídos := interrupción(0x21, ah: 0x3F, bx: manejador, cx: 512, dx: buf)
si acarreo() entonces ... fin
```
Un texto o un arreglo dados a un registro dan su dirección (`dx: buf`, `dx: "NUL"`).

Lo que deja la interrupción, además de AX, se pregunta **después**, con:

| Función       | Da                                                        |
|---------------|-----------------------------------------------------------|
| `acarreo()`   | `bool`: la bandera de acarreo (el «ha fallado» del DOS)   |
| `banderas()`  | las banderas enteras (`nat16`)                            |
| `reg_bx()` `reg_cx()` `reg_dx()` `reg_si()` `reg_di()` `reg_es()` | ese registro |

Regla: hablan de **la última `interrupción` ejecutada en esa misma función** (el compilador
solo guarda las banderas y los registros en las funciones que los preguntan, y solo los que
preguntan: lo que no se usa no cuesta).

Detalles: `ds` se cambia solo durante la llamada y se repone después; en la interrupción 10h
se protege `BP` (hay BIOS que lo estropean). Para las pocas interrupciones que no vuelven
como las demás (25h y 26h dejan las banderas en la pila), `ensamblador`.

---

## 8. Funciones propias del lenguaje

```
escribir(a, b, ...)         ; enseña por la salida del DOS cada cosa según su tipo: los números
                            ; en decimal (los ent16 y ent32, con signo), los textos (puntero o
                            ; arreglo de byte) como texto, los bool como «cierto»/«falso»
escribir_car(c)             ; una sola letra, por su código
salir(código)               ; acaba el programa ya, con ese código de salida (salir() = 0)

dirección(variable)         ; la dirección de una variable, un elemento o un campo, como
                            ; «puntero a» su tipo (de un arreglo: puntero a su elemento)
valor(p)                    ; lo apuntado por p
puntero(n)                  ; un número como puntero
tamaño(tipo o variable)     ; cuántos octetos ocupa (constante)
nat8(x) byte(x) nat16(x) ent16(x) nat32(x) ent32(x)     ; conversiones
fin_programa()              ; la dirección del primer octeto libre detrás del programa y sus
                            ; variables (un puntero): de ahí hasta la pila, memoria sin dueño
                            ; (es donde pone su montón el módulo Memoria)

interrupción(n, reg: v...)  ; sección 7
acarreo() banderas() reg_bx() reg_cx() reg_dx() reg_si() reg_di() reg_es()

meter_puerto(puerto)        ; lee un byte de un puerto (IN) -> byte
sacar_puerto(puerto, dato)  ; escribe un byte en un puerto (OUT)

leer_byte(seg, desp)        ; memoria LEJANA: el byte que hay en seg:desp
poner_byte(seg, desp, dato)
leer_palabra(seg, desp)     ; lo mismo, de 16 bits
poner_palabra(seg, desp, dato)
```

**Memoria cercana y lejana.** Los punteros de Castel son cercanos: valen para todo lo que
está en el segmento del programa (sus variables, su pila, su PSP: `puntero(0x81)` es la
línea de órdenes). Para lo de fuera (la pantalla en `0xB800`, las variables de la BIOS en
`0x40`, los vectores) están `leer_byte`/`poner_byte`/`leer_palabra`/`poner_palabra`, con el
segmento y el desplazamiento por separado. Un tipo «puntero lejano» queda para más adelante.

---

## 9. Dos ejemplos

Una estructura, un arreglo, un `ref` y un `según`:

```
módulo Cuenta

    tipo Recuento = estructura
        letras, cifras, otros: nat16
    fin

    función mira(c: byte, ref r: Recuento)
        según c
            caso 'a'..'z', 'A'..'Z': r.letras := r.letras + 1
            caso '0'..'9':           r.cifras := r.cifras + 1
            si no:                   r.otros := r.otros + 1
        fin
    fin

    función principal()
        variable r: Recuento
        variable t: puntero a byte
        r.letras := 0
        r.cifras := 0
        r.otros := 0
        t := "Hispa-8086, de 1981"
        mientras valor(t) <> 0 hacer
            mira(valor(t), r)
            t := t + 1
        fin
        escribir(r.letras, " letras, ", r.cifras, " cifras y ", r.otros, " más\n")
    fin

fin Cuenta.
```

Y una orden de verdad: **`ejemplos/mas.cas` es la orden MAS (MORE) de Hispa-8086**, entera,
sin una línea de ensamblador: lee la línea de órdenes con un puntero, abre y lee el archivo
con `interrupción`, mira el acarreo, cuenta líneas y columnas, y pone el aviso «-- Más --»
por la BIOS. Hace lo mismo que la de ensamblador (`h8086/ordenes/mas.asm`), octeto a octeto.

---

## 10. Cómo queda en la máquina (para quien baje al ensamblador)

- El programa es un `.COM`: código, datos y pila en un solo segmento (`CS = DS = SS`),
  empezando en `0x100`. Solo instrucciones del 8086.
- Al arrancar: se ponen a cero las variables globales sin valor inicial, se llama a
  `principal` y se acaba con la función 4Ch del DOS.
- Una expresión deja su valor en `AX`; una de 32 bits, en `DX:AX` (`DX`, la palabra alta).
  Calcular puede gastar `AX BX CX DX SI DI ES`.
- **Los números de 32 bits** ocupan en memoria cuatro octetos, la palabra baja primero (como
  los guarda el procesador y como los quiere el DOS). Sumar y restar son `add`/`adc` y
  `sub`/`sbb`; comparar, `cmp` y `sbb`; los bits, una instrucción por palabra. Multiplicar,
  dividir, el resto y desplazar una cuenta de bits que no se sabe al compilar llaman a unas
  rutinas de apoyo (`castel_mul32`, `castel_div32u`, `castel_div32s`, `castel_shl32`,
  `castel_shr32`, `castel_sar32`) que solo van al programa si se usan. Todo con instrucciones
  del 8086.
- **Llamada**: los argumentos se apilan de derecha a izquierda, una palabra cada uno (de un
  `ref`, la dirección; de uno de 32 bits, dos: primero la alta, para que queden como en
  memoria); la función llamada los quita al volver (`ret n`); el resultado vuelve en `AX`
  (en `DX:AX` el de 32 bits). Dentro, `BP` es el marco: el primer parámetro en `[bp+4]`, el
  siguiente en `[bp+6]` (o en `[bp+8]` si el primero era de 32 bits)... y las locales en
  `[bp-2]` hacia abajo.
- Nombres en el ensamblador generado: `F_nombre` (funciones), `G_nombre` (globales),
  `T_n` (textos), `R_xx` (lo que guarda `interrupción`), `L_n` y `X_nombre` (saltos). Lo de
  los módulos importados lleva el módulo delante: `F_Textos__longitud`.
- De los módulos importados **solo va al programa lo que se usa**: la función que nadie llama,
  la variable y el texto que nadie nombra, se quedan fuera. (Un programa que solo usa
  `longitud()` no carga con toda la biblioteca.)

---

## 11. Varios archivos: los módulos

Un programa grande no cabe cómodo en un solo archivo. Cada archivo es un **módulo**; uno de
ellos, el que tiene `principal()`, es el programa, y trae a los demás con `importar`:

```
; --- medidas.cas ---
módulo Medidas

    público constante PULGADA = 254             ; lo ve quien importe Medidas

    variable usos: nat16                        ; sin «público»: solo de este archivo

    público función a_décimas(pulgadas: nat16): nat32
        usos := usos + 1
        devolver nat32(pulgadas) * PULGADA
    fin

fin Medidas.

; --- programa.cas ---
módulo Programa

    importar Medidas

    función principal()
        escribir(a_décimas(10), " ", PULGADA, "\n")
    fin

fin Programa.
```

- **`importar Nombre`** (o varios: `importar Textos, Archivos`) va entre las declaraciones del
  módulo, mejor al principio. Trae **lo público** de ese módulo: sus constantes, tipos,
  variables y funciones declarados con `público` delante. Lo demás no se ve desde fuera, ni
  siquiera con el nombre del módulo.
- Lo importado se usa **por su nombre, sin más** (`a_décimas(10)`). También se puede escribir
  con el módulo delante, **`Medidas.a_décimas(10)`**, y hay que hacerlo en dos casos:
  - si **dos módulos importados** traen el mismo nombre: el compilador no elige, pide que se
    diga cuál (`Figuras.escala(5)` o `Medidas.escala(5)`);
  - si **el propio archivo** declara ese nombre: entonces manda lo propio, y lo del módulo
    sigue estando con su nombre entero. (Así, si mañana la biblioteca gana una función que se
    llama como una del programa, el programa sigue compilando igual.)
- **Dónde se busca**: `importar Textos` busca el archivo `textos.cas` (o `Textos.cas`), por
  este orden: en la carpeta del archivo que lo importa, en las carpetas dadas al compilador
  con `-I carpeta`, y en `biblioteca/`, al lado del compilador. El módulo de dentro tiene que
  llamarse igual (`módulo Textos`). Un archivo con `principal()` es un programa, no un módulo:
  al buscar, no cuenta.
- Un módulo puede importar otros; lo que él importa no pasa a quien lo importa a él (cada
  archivo dice lo que usa). Dos módulos no pueden importarse el uno al otro, ni en círculo.
- Un tipo público se ve entero (con sus campos). Una variable pública se puede leer y cambiar
  desde fuera; si no se quiere eso, se deja sin `público` y se da una función que la devuelva.
- Los módulos no tienen código «de arranque»: sus variables empiezan a cero o con su valor
  inicial, y si algo hay que preparar, lo hace la primera función que lo necesite (así lo
  hace Memoria).
- Todo acaba en **un solo `.COM`**: no hay archivos intermedios ni enlazador. Los errores de
  dentro de un módulo se dicen con su archivo, su línea y su columna.

---

## 12. La biblioteca, escrita en Castel

En `biblioteca/` hay cuatro módulos escritos en Castel (sin ensamblador: los archivos van con
`interrupción`). Se usan con `importar`; lo que dan, con sus comentarios, está en cada archivo.

**`Textos`** (`textos.cas`) — textos acabados en 0, como `puntero a byte`:

| Función | Qué hace |
|---|---|
| `longitud(t)` | cuántas letras tiene |
| `copiar(destino, origen)` · `copiar_hasta(destino, origen, cabe)` | copia (la segunda no se pasa del sitio; da `falso` si corta) |
| `concatenar(destino, añadido)` · `añadir_car(destino, c)` | añade al final |
| `comparar(a, b)` | 0 si son iguales; menos de 0 o más de 0 según cuál va antes |
| `iguales(a, b)` · `iguales_sin_caja(a, b)` · `empieza_por(t, principio)` | dan `bool` |
| `buscar_car(t, c)` · `buscar(t, trozo)` | un puntero a donde está, o `nulo` |
| `a_mayúsculas(t)` · `mayúscula(c)` · `minúscula(c)` | con la ñ y las letras de casa |
| `es_letra(c)` · `es_cifra(c)` · `es_espacio(c)` | dan `bool` |
| `natural_a_texto(n, destino)` · `número_a_texto(n, destino)` | un `nat32` / un `ent32`, en decimal |
| `hex_a_texto(n, cifras, destino)` | en hexadecimal, con las cifras que se digan |
| `texto_a_natural(t, ref n)` · `texto_a_número(t, ref n)` | lee `123`, `-45`, `0x7B`; `falso` si no es un número o no cabe |

**`Archivos`** (`archivos.cas`) — los archivos del DOS. Un archivo abierto es un número (su
manejador); `ENTRADA`, `SALIDA` y `ERRORES` ya están abiertos:

| Función | Qué hace |
|---|---|
| `abrir(nombre, modo)` | `modo`: `LEER`, `ESCRIBIR` o `LEER_Y_ESCRIBIR`. Da el manejador, o `MAL` |
| `crear(nombre)` | lo crea (o lo vacía) y lo abre para escribir. Da el manejador, o `MAL` |
| `leer(m, destino, cuántos)` | da los octetos leídos (0 al final) |
| `grabar(m, origen, cuántos)` · `grabar_texto(m, t)` | escribe octetos / un texto |
| `cerrar(m)` · `borrar(nombre)` | dan `bool` |
| `situar(m, posición, desde)` | se coloca (`DESDE_EL_PRINCIPIO`, `DESDE_DONDE_VA`, `DESDE_EL_FINAL`); la posición es un `ent32` y devuelve en un `nat32` dónde queda |
| `largo(m)` | el tamaño del archivo, en un `nat32` |
| `empezar_lector(l, m)` · `leer_car(l)` · `leer_línea(l, destino, cabe)` | leer letra a letra o línea a línea con un `Lector` (lee del disco de 512 en 512) |
| `empezar_escritor(e, m)` · `poner_car(e, c)` · `poner_texto(e, t)` · `poner_número(e, n)` · `poner_línea(e)` · `vaciar(e)` | escribir poco a poco con un `Escritor` (manda al disco de 512 en 512; al acabar, `vaciar`) |
| `error_dos` | variable: el código de error del DOS de la última que falló |

**`Memoria`** (`memoria.cas`) — un montón dentro del segmento del programa:

| Función | Qué hace |
|---|---|
| `reservar(n)` | pide `n` octetos: un `puntero a byte` (se convierte: `p := puntero(reservar(tamaño(Nodo)))`), o `nulo` si no queda sitio |
| `soltar(p)` | lo devuelve (`soltar(puntero(p))`) |
| `memoria_libre()` · `memoria_usada()` | cuánto puede crecer y cuánto ocupa |
| `llenar(p, n, con)` · `copiar_memoria(destino, origen, n)` | octetos a granel |

**`Argumentos`** (`argumentos.cas`) — lo escrito detrás del nombre del programa:
`cuántos_argumentos()`, `argumento(n, destino, cabe)` y `línea_de_órdenes()`.

Los usan cuatro programas de `ejemplos/`: `biblio.cas` (textos y montón), `ficheros.cas`
(archivos) y dos órdenes de verdad: **`contar.cas`**, CONTAR, que cuenta líneas, palabras y
octetos de uno o más archivos (como el `wc` de Unix), con totales de 32 bits; y
**`numera.cas`**, NUMERA, que enseña un texto con el número de cada línea delante. Con todo
dentro, cada una ocupa menos de 1,5 K.

---

## 13. Lo que queda para más adelante

- **Autohospedaje** (segunda parte de la Fase 4): el compilador, escrito en Castel, y un
  ensamblador propio mínimo para no depender de NASM dentro de Hispa-8086. Lo que hacía falta
  antes (32 bits, módulos, textos, archivos y memoria) ya está. Ver CIMIENTOS.md y el README.
- **Punteros lejanos** (`seg:desp`) como tipo. Mientras, `leer_byte` y compañía.
- **Copiar arreglos y estructuras enteros** (`a := b`) y valores iniciales de estructuras.
- **Programas `.EXE`** y más de un segmento (las tablas del compilador pueden pedir más de
  64 K); y el salto a 32 bits de verdad, con los registros del 386 (Fase 5).
- **Comprobaciones al ejecutar** opcionales (índices, desbordamientos), para depurar.
- **Más biblioteca** según haga falta: escribir en archivos con memoria intermedia, carpetas,
  fechas.
