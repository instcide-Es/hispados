# Castel de 32 bits — el modelo plano para Hispa-Pentium

*Fase 5 de CIMIENTOS.md. 18/11/2026. Define cómo Castel genera código de 32 bits
para el 386 en adelante, en modo protegido con la memoria plana: la base para
escribir el núcleo de Hispa-Pentium (la gama XP, sin DOS). Lo de 16 bits
(CASTEL.md) no cambia; esto es un modo nuevo que se pide con `modelo`.
Puesto al día el mismo 18/11 con el **`modelo usuario`** (los programas del anillo 3 de
Hispa-Pentium), la **biblioteca de 32 bits** (`biblioteca32/`) y el núcleo que ha crecido
desde la semilla ([../../docs/PENTIUM.md](../../docs/PENTIUM.md)).*

Regla de oro, la de siempre: **el núcleo, pequeño.** Lo que el lenguaje no sabe
decir se hace en un bloque `ensamblador`, ahora con los registros del 386.

---

## 1. Tres modelos nuevos

El programa dice al principio, entre sus declaraciones, de qué modelo es:

```
módulo Núcleo
    modelo núcleo         ; un núcleo de 32 bits, arrancable (multiboot)
    ...
```

```
módulo Programa
    modelo hispa32        ; un programa Win32 de consola (PE), para HW32
    ...
```

- **`modelo núcleo`** da un **binario plano** (`NUCLEO.BIN`) con una cabecera
  **multiboot**: lo arranca QEMU con `-kernel`, o GRUB, en modo protegido. No hay
  DOS ni BIOS por debajo: el programa *es* el sistema. La semilla de
  Hispa-Pentium (`nucleo32/semilla.cas`) es de este modelo.
- **`modelo hispa32`** da un **programa Win32 de consola** (un `.EXE` con cabecera
  PE), que corre sobre Hispa-DOS con **HW32** (el cargador Win32 de Hispa-486) y
  KERNEL32: escribe con `WriteFile` y acaba con `ExitProcess`. Sirve para escribir
  utilidades de 32 bits antes de tener el núcleo entero. `ejemplos32/hola32.cas` es
  de este modelo.
- **`modelo usuario`** da un **programa de usuario de Hispa-Pentium** (`USUARIO.BIN`,
  un binario plano con su cabecera): corre en el **anillo 3**, en su propio espacio
  de direcciones, a partir de `0x40000000`. Lo carga el núcleo (del disco, o de los
  módulos de multiboot) y habla con él por la **llamada al sistema** (`int 0x80`):
  `escribir` va por ella, y `salir` también; lo demás (el teclado, los archivos
  —leer, escribir, crear, borrar, las carpetas—, lanzar otros programas, pedir
  memoria) lo da el módulo `Llamadas`
  (`nucleo32/llamadas.cas`), que se importa. `nucleo32/uordenes.cas` (el intérprete
  de órdenes de Hispa-Pentium), `uhola.cas`, `ueco.cas`... son de este modelo.

```
módulo Hola
    modelo usuario        ; un programa del anillo 3 de Hispa-Pentium
    ...
```

Los tres comparten **toda la generación de 32 bits** (lo de esta nota); solo
cambian el arranque (sección 8) y adónde va lo que se escribe. A los tres se les
llama aquí **«el modelo plano»**. `modelo`, `núcleo`, `hispa32` y `usuario` no son
palabras reservadas (solo cuentan detrás de `modelo`, al principio del programa).

El modelo lo pone **el programa** (el archivo que se compila); sus módulos se
compilan como diga él. No se mezcla con `modelo grande` (ese es de 16 bits).

---

## 2. Qué cambia respecto a los 16 bits

| | 16 bits (`.COM`, `modelo grande`) | 32 bits (`modelo núcleo`, `modelo hispa32`) |
|---|---|---|
| La palabra de la máquina | 16 bits | **32 bits** |
| Las cuentas de a diario | en 16 bits (con `nat32` para lo que no cabe) | **en 32 bits**, siempre |
| Un puntero | cercano, 16 bits (dentro del segmento) | **plano, 32 bits** (toda la memoria) |
| El registro de un valor | `AX` (`DX:AX` los de 32) | **`EAX`**, entero |
| La pila, el marco | `SP`, `BP` | **`ESP`, `EBP`** |
| Memoria | cercana + lejana (`leer_byte`...) | **una sola, plana**: un puntero llega a cualquier sitio |
| BIOS/DOS (`interrupción`, `acarreo`...) | sí | **no** (es el metal: se usa `ensamblador`) |

Lo que **no cambia**: los tipos y su sintaxis, las sentencias (`si`, `mientras`,
`para`, `según`...), las estructuras y los arreglos, `ref`, `escribir`, los bloques
`ensamblador` (con `{nombre}`). El mismo lenguaje; otro tamaño de máquina.

---

## 3. Los tipos, en 32 bits

Los mismos de CASTEL.md §3, con dos diferencias:

- **Un puntero ocupa 4 octetos** (es plano, llega a los 4 GB). `tamaño(puntero a
  T)` es 4; un texto (puntero a byte) también. Las estructuras con punteros
  cambian de tamaño en consecuencia.
- **`nat16`/`ent16`/`byte` siguen existiendo** (para describir el hardware y las
  estructuras tal cual: un registro de 16 bits de un dispositivo, un campo de un
  sector). Pero **las cuentas se hacen en 32 bits**: al leer un `nat16` o un
  `byte` se ensancha a 32 (con ceros), un `ent16` con su signo; el resultado se
  recorta al guardarlo si el destino es más corto.

`nat32`/`ent32` son los de a diario. Siguen valiendo para todo; ahora **no
cuestan más** que los de 16 (son el tamaño de la máquina).

### La trampa del tamaño, al revés que en 16 bits

En 16 bits, `a * b` con `a`, `b` de 16 bits se hacía en 16 (y podía recortarse).
En el modelo plano **todas las cuentas se hacen en 32 bits**, así que
`a * b` con `a`, `b` de `nat16` cabe entero (hasta ~4.000 millones) aunque el
resultado se guarde luego en algo más corto. El tipo de la cuenta es de 32 bits
en cuanto hay un número por medio; con signo si alguno de los dos lo lleva.

---

## 4. Cómo queda en la máquina

- **Modo protegido plano, de 32 bits.** El código y los datos ven toda la memoria
  por su dirección lineal; no hay segmentación que estorbe (los selectores de
  código y datos tienen base 0 y 4 GB de límite: sección 8). `CS`, `DS`, `ES`,
  `SS` apuntan al mismo espacio plano.
- Una expresión deja su valor en **`EAX`**, ya entero: un `byte` o un `nat16` se
  ensancha con ceros, un `ent16` con su signo. Calcular puede gastar `EAX`,
  `EBX`, `ECX`, `EDX`, `ESI`, `EDI`; se conservan `EBP` y `ESP`.
- **Llamada**: los argumentos se apilan de derecha a izquierda, **cuatro octetos
  cada uno** (de un `ref`, la dirección, de 4 octetos); la función llamada los
  quita al volver (`ret n`); el resultado vuelve en `EAX`. Marco con `EBP`: el
  primer parámetro en `[ebp+8]`, el siguiente en `[ebp+12]`...; las locales en
  `[ebp-4]` hacia abajo, cada una en un múltiplo de cuatro octetos.
- **Direccionamiento**: un lugar es una base (nada, `ebp` o `ebx`), un índice
  opcional (`esi`, con su escala 1/2/4/8) y un desplazamiento: `[ebp-8]`,
  `[G_tabla+esi*4]`, `[ebx+esi*2+12]`. Se aprovecha el **SIB** del 386 para
  indexar arreglos de elementos de 1, 2, 4 u 8 octetos sin multiplicar.
- **Nombres en el ensamblador generado**: los mismos que en 16 bits (`F_nombre`,
  `G_nombre`, `T_n`, `L_n`, `X_nombre`). De los módulos importados, con el módulo
  delante (`F_Módulo__nombre`).
- No hay rutinas de 32 bits «en dos palabras»: multiplicar, dividir y desplazar
  son instrucciones del 386 (`imul`, `idiv`/`div`, `shl`/`shr`/`sar` con cuenta).

---

## 5. El sistema, sin BIOS ni DOS

En el modelo plano **no están** `interrupción()`, `acarreo()`, `banderas()`,
`reg_bx()`... (son del modo real: la BIOS y el DOS no existen aquí), ni la memoria
lejana (`leer_byte`, `poner_byte`, `leer_palabra`, `poner_palabra`): la memoria es
plana, y a cualquier sitio se llega con un **puntero** normal:

```
variable pantalla: puntero a nat16
pantalla := puntero(0xB8000)              ; la memoria de texto VGA
pantalla[80 * 24 + 79] := 0x4E21          ; una '!' en la esquina
```

Lo que queda del lenguaje sí está: `escribir`, `escribir_car`, `salir`,
`meter_puerto`, `sacar_puerto` (los puertos de E/S siguen siendo puertos),
`dirección`, `valor`, `puntero`, `tamaño`, las conversiones y `fin_programa`.
`escribir` saca su texto por donde toque según el modelo (sección 8).

Si el compilador ve uno de los que no están, lo dice:

```
error: en 32 bits no hay memoria lejana: leer_byte() no está (la memoria es
plana: se usa un puntero, como puntero(0xB8000))
error: interrupción() es del modo real (BIOS y DOS): en 32 bits no está (se usa
un bloque «ensamblador»)
```

Para bajar al metal (cargar la IDT, parar el procesador, hablar con un
dispositivo), un bloque **`ensamblador`**, con los registros del 386:

```
ensamblador
    lidt {idtr}
    sti
fin
```

`{variable}`, `{CONSTANTE}`, `{función}` siguen valiendo; una variable de 32 bits
es `[ebp-4]` o `[G_x]` de cuatro octetos.

---

## 6. Las interrupciones (núcleo)

En `modelo núcleo` una función puede **atender una interrupción** del procesador
(una excepción, o una línea del controlador: el reloj, el teclado). Se escribe
`interrupción función`:

```
interrupción función reloj()
    tics := tics + 1
    sacar_puerto(0x20, 0x20)          ; «fin de interrupción» al 8259
fin
```

- No devuelve nada y **no se la llama** (la llama el procesador): su dirección se
  pone en la tabla de interrupciones (IDT) con `dirección(reloj)`, y acaba con
  `iretd`. El compilador le guarda **todos** los registros al entrar (`pushad`) y
  los repone al salir, así que puede hacer lo que quiera sin estropear nada.
- Puede llevar **un** parámetro, un número: el **código de error** que algunas
  excepciones dejan en la pila (el procesador lo apila; el compilador lo recoge
  en `[ebp+32]`, detrás de lo que guarda `pushad`, y lo quita al volver).

```
interrupción función fallo_de_página(código: nat32)
    ...
fin
```

La IDT, el controlador (8259) y el reloj (8253) se preparan desde Castel, con
estructuras y `ensamblador` para las dos instrucciones que faltan (`lidt`,
`sti`): ver `nucleo32/semilla.cas`, que lo hace entero.

---

## 7. La biblioteca de 32 bits

Dentro del propio compilador va lo justo para que un programa plano escriba y
acabe: **`escribir`** (números y textos), **`escribir_car`** y **`salir`**. Por
debajo:

- `castel_salida(ECX octetos desde ESI)` manda el texto adonde toque;
- `castel_salir(código en AL)` acaba el programa.

En `modelo núcleo`, `castel_salida` escribe en la **pantalla de texto VGA** (con
su cursor, subiendo las líneas al llegar abajo) y en el **puerto serie COM1** (a
115200, 8N1: así `probar32.sh` lo lee); `castel_salir` lo dice por el puerto
`0xF4` (el `isa-debug-exit` de QEMU) y para el procesador. En `modelo hispa32`,
`castel_salida` es `WriteFile` y `castel_salir` es `ExitProcess`. En `modelo
usuario`, las dos son la llamada al sistema: `int 0x80` con `EAX = 1` (escribir
`ECX` octetos desde `EBX`) y con `EAX = 2` (acabar con el código de `EBX`). Como
siempre, **solo van al programa las rutinas que se usan**.

Lo demás está **escrito en Castel**, en `biblioteca32/` (el compilador la busca
ahí sola; CASTEL, dentro de Hispa-DOS, con `-I BIB32`). Vale para los tres modelos
(no toca el sistema: solo memoria):

**`Textos32`** (`textos32.cas`): la de textos de 16 bits (CASTEL.md §12) con los
largos y las cuentas de 32 bits — `longitud`, `copiar`, `copiar_hasta`,
`concatenar`, `añadir_car`, `comparar`, `iguales`, `iguales_sin_caja`,
`empieza_por`, `buscar_car`, `buscar`, `a_mayúsculas`, `mayúscula`, `minúscula`,
`es_letra`, `es_cifra`, `es_espacio`, `natural_a_texto`, `número_a_texto`,
`hex_a_texto`, `texto_a_natural`, `texto_a_número` — y dos más para trocear una
línea de órdenes: `saltar_espacios(t)` y `palabra(t, n, destino, cabe)` (la palabra
número `n`).

**`Mem32`** (`mem32.cas`): memoria a granel. `llenar(p, n, con)` (de cuatro en
cuatro si se puede), `llenar32(p, n, con)`, `copiar_memoria(destino, origen, n)`
(aunque se pisen, como el `memmove` de C), `comparar_memoria(a, b, n)`,
`todo_igual(p, n, con)` y `redondear(n, a)`.

**`Formato`** (`formato.cas`): escribir con formato, un `printf` sencillo. La
plantilla lleva marcas: `%d` (entero), `%u` (natural), `%x`/`%X` (hexadecimal),
`%s` (texto), `%c` (letra) y `%%`; con ancho (`%8u`), con ceros (`%08X`) o a la
izquierda (`%-10s`). Castel no tiene funciones con un número variable de
argumentos: los datos van en un arreglo de `nat32` (un texto, como `nat32(t)`):

```
importar Formato
...
imprimir2("fallo de página en 0x%08X (%s)\n", dir, nat32("escritura"))
n := formatear(línea, 80, "%5u marcos libres", datos)      ; datos: arreglo de nat32
```

`formatear(destino, cabe, plantilla, datos)` lo deja en un texto (corta lo que no
cabe); `imprimir(plantilla, datos)` lo escribe; `imprimir1` a `imprimir4`, con uno
a cuatro datos sueltos. `ejemplos32/biblio32.cas` prueba las tres.

---

## 8. El arranque y el enlazado

El compilador no necesita un enlazador: saca un **solo archivo** con todo dentro,
y lo ensambla ENSAMBLA (o NASM). El arranque va delante del código; lo pone el
compilador según el modelo.

### `modelo núcleo` → un binario plano multiboot (`org 0x100000`)

- Una **cabecera multiboot** (la magia `0x1BADB002`): QEMU la reconoce con
  `-kernel`, carga el binario en `0x100000`, y salta a su arranque **ya en modo
  protegido de 32 bits** (multiboot lo deja así).
- El arranque carga una **GDT** mínima (código `0x08` y datos `0x10`, planos, de
  4 GB), pone `ESP` en la pila del núcleo, borra la pantalla, prepara el puerto
  serie y llama a `principal()`. Al volver (o con `salir`), `castel_salir`.
- No hace falta formato ELF: el binario plano vale tal cual (la cabecera
  multiboot dice dónde empieza y acaba). Es lo más barato y es lo que arranca en
  QEMU con una orden.
- Detrás de la cabecera van **cuatro palabras a cero**: el sitio donde multiboot
  deja que el núcleo le pida al cargador un **modo gráfico** (la clase de modo, el
  ancho, el alto y los bits por punto). A cero y sin su bandera, no se pide nada
  (modo texto, como siempre). `nucleo32/modo_grafico.py` hace una copia del
  núcleo que sí lo pide, para GRUB en los PC de verdad (PENTIUM.md §11).

### `modelo usuario` → un programa del anillo 3 (`org 0x40000000`)

- Delante, una **cabecera** (detrás del salto del principio): la magia `HPU3`
  (`0x33555048`), dónde va (`0x40000000`), dónde acaba lo que hay en el archivo
  (`_datos_fin`), dónde acaba lo que empieza a cero (`_vacio_fin`) y por dónde se
  empieza (`C_arranque`). Con eso el núcleo sabe cuántas páginas ponerle y qué
  copiar en ellas (`nucleo32/usuario.cas`).
- El arranque borra sus variables y llama a `principal()`; al volver,
  `castel_salir`, que es la llamada al sistema de acabar. **La pila no la pone el
  programa**: se la da el núcleo, arriba del todo de su zona (acaba en
  `0xC0000000`), y crece sola hacia abajo, hasta 4 MB (PENTIUM.md §6). (Hasta el
  19/11 eran 16 K fijos dentro de las variables del programa.)

### `modelo hispa32` → un programa Win32 de consola (PE, `ImageBase 0x400000`)

- Las **cabeceras PE a mano** (MZ + PE + una sección en `0x401000`) y la tabla de
  lo que se importa de **KERNEL32** (`GetStdHandle`, `WriteFile`, `ExitProcess`):
  justo lo que HW32 (el cargador de Hispa-486, `utils/hw32.asm`) sabe cargar.
- El arranque pide el manejador de salida (`GetStdHandle(-11)`) y llama a
  `principal()`. `escribir` va por `WriteFile`; `salir`, por `ExitProcess`.
- Sale un `.EXE` que se ejecuta con `HW32 PROGRAMA.EXE` dentro de Hispa-DOS.

Un **binario plano con `org`** (núcleo) o un **PE** (hispa32): las dos salidas que
pedía la Fase 5, y las dos baratas. Un ELF32 de verdad queda para cuando haga
falta enlazar con otras cosas; de momento no hace falta.

---

## 9. Probar y comprobar

- `compilar.sh` reconoce los tres modelos y saca `NOMBRE.BIN` (núcleo o usuario) o
  `NOMBRE.EXE` (hispa32).
- **`probar32.sh PROGRAMA`** lo ejecuta y enseña lo que escribe y su código de
  salida: un núcleo con `qemu-system-i386 -kernel` (la salida por el puerto
  serie); un `.EXE` con HW32 dentro de Hispa-DOS.
- **`nucleo32/probar.sh`** arranca el núcleo de Hispa-Pentium y le teclea órdenes
  (por el monitor de QEMU, como un teclado de verdad), con los programas de
  usuario como módulos de multiboot (`-initrd`) y, con `DISCO=imagen`, un disco
  duro IDE (lo hace **`nucleo32/disco.sh`**, con FAT y los programas dentro):
  sirve para ver el reloj, el teclado, las excepciones, la memoria, las tareas, el
  anillo 3, el disco y el intérprete de órdenes. (Teclea en el teclado español,
  que es el del núcleo; con `ESCRIBE=1`, lo que se escriba queda en la imagen.)
- `pruebas.sh` (parte 8) comprueba: los ejemplos de 16 bits sin DOS, compilados
  como núcleos de 32 bits, escriben lo mismo; los de `ejemplos32/` (núcleos en
  QEMU y programas Hispa32 con HW32) dan su salida esperada y su código; cuentas
  al azar con las reglas de 32 bits (`azar32.py ... plano`); y el núcleo de
  Hispa-Pentium, con su IDT, su reloj, su teclado, sus excepciones, la paginación y
  el fallo de página, el montón, las tareas y los programas del anillo 3
  (`SOLO=32 ./pruebas.sh` hace solo esto y los errores). Y, como siempre,
  **los dos compiladores** (`ccastel.py` y CASTEL, el escrito en Castel) sacan el
  **mismo** ensamblador también para los programas de 32 bits, y **ENSAMBLA** da
  el **mismo archivo que NASM** para ellos (las instrucciones del 386: prefijos
  de tamaño `66h`, SIB, `0F`, saltos largos, `ltr`, los registros de control...).

---

## 10. Lo que queda para más adelante

- (La **biblioteca de 32 bits**, la **paginación** y el **modo usuario** ya están:
  ver la sección 7 y [PENTIUM.md](../../docs/PENTIUM.md).)
- **Llamar a una función por su dirección** (un tipo «función»): hoy, para que una
  tarea empiece en una función, se da `nat32(dirección(f))` y se arma su pila a
  mano; para las tablas de rutinas de un núcleo hará falta llamarlas desde Castel.
- **Sumar a un arreglo** (`tabla + 4`, como con un puntero): hoy hay que escribir
  `dirección(tabla[4])`.
- **Funciones de interrupción públicas** (`público interrupción función`): hoy cada
  módulo pone en la IDT las suyas.
- **ELF32** de salida, si algún día hay que enlazar con código de fuera.
- **Tablas de datos grandes**: un bloque `ensamblador` no puede pasar de 6000
  octetos en CASTEL (la letra del entorno gráfico de Hispa-Pentium va en cuatro
  bloques seguidos), y un arreglo con valor inicial, de unos 4000.
- **El sitio de CASTEL para los textos** (los nombres y los textos entre
  comillas de todo el programa, con sus módulos): eran 40 K y, desde la 0.5 de
  Hispa-Pentium, son 64 K (`compilador/base.cas`): el núcleo ya no cabía. Es una
  tabla del compilador, no del lenguaje: lo que saca no cambia. Con la 0.6
  (doce módulos más: los discos y el USB) han crecido otras tres: **48 módulos**
  por programa (eran 24; el núcleo, con los de la biblioteca, tiene 35), **48 K
  para los nombres** (eran 40) y **160 K para el árbol** (eran 128: lo
  declarado en todos los módulos se queda en él mientras se compila). Con eso
  CASTEL usa 272 K para sus tablas, casi todo lo que deja libre Hispa-8086 en
  640 K: el siguiente estirón del núcleo pedirá otra cosa (soltar del árbol lo
  que ya no hace falta, o compilar por módulos). Y su **archivo temporal** (el
  código de todas las funciones, que se lee dos veces) pasa de 800 K con el
  núcleo: ya no cabe en un disquete de 1,44 MB con los fuentes o con la salida;
  las pruebas lo ponen, con la salida, en uno de 2,88.
- **El núcleo de Hispa-Pentium**: desde la 0.4 tiene su entorno gráfico (el
  escritorio, PENTIUM.md §10; con menús, portapapeles e iconos desde la 0.5) y,
  desde la 0.6, los discos SATA y el USB de los PC de verdad (PENTIUM.md §7 y
  §11: doce módulos más, unas 4.000 líneas, con las estructuras que quieren
  las controladoras escritas como `puntero a nat32` sobre páginas del núcleo),
  escrito también en Castel sin tocar el lenguaje.
  Ha crecido desde la semilla (PENTIUM.md) y ya
  lee y escribe discos FAT, carga los programas del disco y tiene su intérprete de
  órdenes en el anillo 3 (con archivos .BAT, Ctrl+C y el teclado español), todo
  sin tocar el lenguaje; de los compiladores, solo el arranque de `modelo usuario`
  (la pila ya la pone el núcleo: sección 8). Lo que más se ha echado de menos al
  escribirlo: llamar a una función por su dirección (el reloj llama así, con dos
  líneas de ensamblador, a quien acaba una tarea cortada con Ctrl+C).
