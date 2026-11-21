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
`reg_bx()`... (son del modo real: la BIOS y el DOS no existen aquí). La memoria es
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

**La memoria «por párrafos»** (desde el 20/11/2026). `leer_byte(p, d)`,
`poner_byte(p, d, v)`, `leer_palabra(p, d)` y `poner_palabra(p, d, v)` también
están, con el mismo sentido que en el modo real: el octeto (o la palabra) de la
dirección **`p * 16 + d`**, solo que aquí `p` y `d` son de 32 bits. Así,
`leer_palabra(0xB800, 0)` es la primera letra de la pantalla, como en el DOS
(`ejemplos32/parrafos.cas`). Sirve para que lo escrito para 16 bits con memoria
lejana corra igual en 32: el compilador CASTEL y el ensamblador ENSAMBLA guardan
así sus tablas, y en 32 bits sus «segmentos» son párrafos de la memoria plana
(sección 11). Hasta el 19/11 eran un error («en 32 bits no hay memoria lejana»).
Se genera en cuatro instrucciones: con un desplazamiento constante,
`shl eax, 4` y `movzx eax, word [eax+6]`.

Si el compilador ve uno de los que no están, lo dice:

```
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

**`Json`** (`json.cas`, desde el 20/11/2026): leer y escribir JSON. `json_leer(texto,
largo)` trocea un documento entero en **piezas** (objetos, listas, textos, números,
`true`, `false`, `null`; sin copiar nada: cada pieza dice dónde está en el texto),
en la memoria que se le da antes con `json_memoria(sitio, cuántas)`. Luego se
pregunta con `json_campo(objeto, "clave")`, `json_elemento(lista, i)`,
`json_cuántos`, `json_ruta(pieza, "data.obras.0.titulo")`, `json_texto` (en la
página 437 de la casa, con los escapes y el UTF-8 hechos; lo que no está en ella,
«?»), `json_número`, `json_verdad`, `json_nulo` y sus atajos `json_texto_campo`...
Una pieza que no está es `NINGUNA`, y preguntar por ella no falla. Para escribir:
`json_empezar(sitio, cabe)`, `json_abre`, `json_clave`, `json_pon_texto` (de la
página 437 a UTF-8, escapado), `json_pon_número`, `json_cierra`... y los atajos
`json_campo_texto("email", correo)`. Y dos sueltas: `a_utf8(letra, destino)` y
`de_unicode(código)`. `ejemplos32/json32.cas` la prueba (y es lo que usan los
programas que hablan con una API web).

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
  lo que se importa de **KERNEL32**: lo del arranque (`GetStdHandle`, `WriteFile`,
  `ExitProcess`) y, desde el 20/11, lo que hace falta para los archivos, los
  argumentos y la memoria (`ReadFile`, `CreateFileA`, `CloseHandle`,
  `SetFilePointer`, `DeleteFileA`, `GetCommandLineA`, `GetModuleFileNameA`,
  `VirtualAlloc`), siempre las mismas once: un bloque `ensamblador` llama a
  cualquiera con `call [C_CreateFileA]` (así lo hace `biblioteca32/hw32`). Todo
  lo sabe cargar HW32 (el cargador de Hispa-486, `utils/hw32.asm`).
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
- **Tablas de datos grandes**: en el CASTEL de 16 bits un bloque `ensamblador` no
  puede pasar de 6000 octetos (la letra del entorno gráfico de Hispa-Pentium va en
  cuatro bloques seguidos), y un arreglo con valor inicial, de unos 4000 (unas
  1000 piezas). En CASTEL32 (sección 11), 60.000 y unas 4000 piezas.
- **El sitio de CASTEL (el de 16 bits) para sus tablas**: lo que sigue es su
  historia hasta el 19/11/2026, cuando se quedó sin sitio. **Desde el 20/11 el
  núcleo lo compila CASTEL32** (sección 11), que tiene cada tabla de casi 1 MB, y
  esto ya no limita nada del núcleo. (Los nombres y los textos entre
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
  las pruebas lo ponen, con la salida, en uno de 2,88. Con la 0.7 (instalar en
  el disco) no se ha tocado nada: lo nuevo son dos programas del anillo 3
  aparte y un módulo corto en el núcleo, que usa 2.644 párrafos de 3.072 para
  los nombres, 2.809 de 4.096 para los textos y 8.718 de 10.240, lo más, para
  el árbol (`CASTEL ... --gasto`). Con la 0.8 (la red) tampoco: la red es un
  programa del anillo 3 aparte (`RED.BIN`, con seis módulos suyos) y el
  núcleo solo gana un módulo corto (`red.cas`); las pruebas compilan la red en
  una cuarta tanda de disquetes. Con la 0.8 entera (el USB en caliente, la
  red y apagar de verdad juntos, más el AHCI del ICH7) el núcleo ya no cabía:
  3.075 párrafos de nombres (eran 3.072), 10.153 del árbol (de 10.240) y
  3.400 de lo declarado (de 3.584). Más sitio para todo ya no cabe en lo que
  deja libre Hispa-8086 (con más tablas, CASTEL no arranca: «no queda memoria
  libre para las tablas del compilador»), así que se han repartido los mismos
  K: **52 K para los nombres** (eran 48) y **60 K para los textos** (eran 64;
  el núcleo usa 50). Queda muy poco margen (nombres al 92 %, árbol al 99 %,
  declarados al 95 %, textos al 83 %): el siguiente estirón del núcleo pedirá
  ya lo de soltar del árbol lo que no haga falta o compilar por módulos (o eso
  se pensaba: lo que ha pedido es CASTEL32). Un programa no puede declarar un
  arreglo de más de 64 K: lo que haga falta mayor se pide con `más_memoria`.
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

---

## 11. CASTEL32: el compilador de Castel, de 32 bits

*20/11/2026.* Castel nació para escribir los sistemas del 386 en adelante, pero su
compilador escrito en Castel (CASTEL, `compilador/`) era un programa de **16 bits**:
corría en Hispa-8086 con 640 K, y con el núcleo de Hispa-Pentium 0.8 sus tablas
estaban al límite (árbol 10.153 de 10.240 párrafos, nombres al 92 %, declarados al
95 %; sección 10). No era por diseño, sino por cómo se llegó: el autohospedaje se
hizo antes que los 32 bits. **CASTEL32** es el mismo compilador (y su ensamblador,
ENSAMBLA) compilado con la parte de atrás de 32 bits.

### La decisión

- **El que manda es CASTEL32.** El núcleo de Hispa-Pentium y sus programas los
  compila CASTEL32 (en las pruebas, dentro de Hispa-Pentium: `pruebas.sh`, parte
  15). **Ningún límite futuro del núcleo depende ya del CASTEL de 16 bits**: si un
  día el núcleo no le cabe, no pasa nada.
- **El CASTEL de 16 bits sigue** (CASTEL.EXE, `compilador/castel.cas`): compila
  todo lo de 16 bits y los programas de 32 que caben, y se sigue compilando a sí
  mismo en Hispa-8086 (partes 5 y 6). Pero ya no compila el núcleo en las pruebas.
- **Los mismos fuentes para todos.** No hay un compilador de 32 bits aparte: son
  los módulos de `compilador/` (y `ensamblador/montador.cas`) tal cual. Lo que
  cambia entre uno y otro es poquísimo y está en módulos pequeños que el programa
  importa **antes que nada** (así, al buscar un módulo ya cargado, vale el
  primero):

| | 16 bits (Hispa-8086, DOS) | Hispa-Pentium (anillo 3) | Hispa-386/486 (HW32) |
|---|---|---|---|
| el programa | `compilador/castel.cas` (`modelo grande`) | `compilador/32/pentium/castel32.cas` (`modelo usuario`) | `compilador/32/hw32/castelw.cas` (`modelo hispa32`) |
| su ensamblador | `ensamblador/ensambla.cas` | `ensamblador/32/pentium/ensamb32.cas` | `ensamblador/32/hw32/ensambw.cas` |
| las medidas | `compilador/medidas.cas` | `compilador/32/medidas.cas` | la misma |
| el sistema (Lejos, Archivos, Argumentos) | `biblioteca/` (la 21h del DOS) | `biblioteca32/pentium/` (las llamadas del núcleo) | `biblioteca32/hw32/` (KERNEL32) |
| lo que sale | CASTEL.EXE, ENSAMBLA.COM | CASTEL32.BIN, ENSAMB32.BIN | CASTELW.EXE, ENSAMBW.EXE |

  Lo común: `Mando` (la línea de órdenes), los once módulos del compilador y
  `Montador` (ENSAMBLA por dentro). Lo que se tocó en ellos para esto: los
  segmentos son del tipo **`Segmento`** (de Lejos: `nat16` en 16 bits, `nat32`
  en 32), los tamaños de las tablas vienen de **`Medidas`**, y la carpeta del
  propio programa la dice `Argumentos.ruta_del_programa`.
- **Cómo se ven las tablas en 32 bits.** El compilador guarda sus tablas en
  memoria lejana: un nudo del árbol es el párrafo `seg_ar + n`, y se lee con
  `leer_palabra(seg_ar + n, 6)`. En 32 bits eso es la memoria «por párrafos»
  (sección 5): `p * 16 + d` en la memoria plana. El Lejos de 32 bits pide los
  bloques al sistema (`más_memoria` en Hispa-Pentium, `VirtualAlloc` en HW32) y da
  su párrafo, un `nat32`. Así el código no cambia, y las tablas pasan de los 272 K
  que cabían en 640 K a casi 6 MB.
- **Por qué PE para Hispa-386/486, y no DPMI.** Las dos maneras corren en modo
  protegido de 32 bits sobre HSPDPMI; pero un programa DPMI propio habría pedido
  un modelo nuevo en los dos compiladores (su arranque: pasar a modo protegido,
  pedir los selectores, hablar con el DOS por la 0300h). El `modelo hispa32` ya
  estaba en los dos compiladores, y **HW32** ya carga programas PE y les da
  archivos, línea de órdenes y memoria por KERNEL32 (y es lo que ejecuta FASM,
  NASM y el resto de Hispa-486: está muy probado). Solo ha hecho falta que el
  arranque de hispa32 importe ocho funciones más de KERNEL32 (sección 8) y
  escribir `biblioteca32/hw32` (tres módulos cortos). Menos trabajo y más robusto.

### Las medidas (`compilador/32/medidas.cas`)

Las referencias siguen siendo de 16 bits (un nudo, un nombre o un texto es un
párrafo **dentro de su tabla**), así que cada tabla puede llegar a 64 K párrafos:
se le dan **61.440 (960 K)** a los nombres, los textos, el árbol, los tipos y lo
declarado; 200 módulos por programa; 31.738 datos y otras tantas etiquetas; 1000
locales por función; 64 K y 16.000 líneas de ensamblador por función; bloques
`ensamblador` de 60.000 octetos. Con el núcleo de Hispa-Pentium 0.8
(`CASTEL32 SEMILLA.CAS --gasto`):

```
  gasto de las tablas, de lo que hay (en párrafos de 16 octetos; los módulos, uno a uno):
    nombres         3075 de 61440 (5 %)
    textos          3171 de 61440 (5 %)
    árbol (lo más)  10153 de 61440 (16 %)
    tipos           878 de 61440 (1 %)
    declarados      3400 de 61440 (5 %)
    módulos         39 de 200 (19 %)
```

(El de 16 bits, con el mismo núcleo: árbol 10.153 de 10.240, el 99 %.) El árbol,
lo que más gasta, tiene sitio para un núcleo **seis veces** mayor; lo demás, para
uno veinte veces mayor. Si algún día hace falta más, el camino está claro y no
toca el lenguaje: referencias de 32 bits (un tipo `Ref` que sea `nat16` en 16 bits
y `nat32` en 32, como `Segmento`, y nudos de 32 octetos en vez de 16).

### El arranque y el punto fijo

```
ccastel.py + NASM  ->  CASTEL32.BIN, ENSAMB32.BIN            (paso 1)
CASTEL32 se compila a sí mismo, ENSAMB32 lo ensambla  ->  C2  (paso 2)
C2 se compila a sí mismo  ->  C3                             (paso 3)
```

El paso 2 da **el mismo ensamblador que el paso 1** (el de ccastel.py) y el mismo
binario que NASM; el paso 3, **el mismo que el 2, octeto a octeto**. Las pruebas
lo hacen tres veces: en Linux (con `corre32`, abajo), **dentro de Hispa-Pentium** y,
con CASTELW, **dentro de Hispa-DOS con HW32**. Y CASTEL32 saca exactamente lo mismo
que ccastel.py en **todos** los programas de las pruebas (los ejemplos de 16 y 32
bits, la biblioteca entera, ENSAMBLA, las cuentas al azar, el núcleo con sus
programas, el CASTEL de 16 bits y él mismo) y dice los mismos errores.

Las cifras (QEMU sin aceleración, en el servidor de desarrollo):

| | |
|---|---|
| CASTEL32.BIN / ENSAMB32.BIN | 134.808 / 24.876 octetos |
| CASTELW.EXE / ENSAMBW.EXE | 136.192 / 26.112 octetos |
| el núcleo y sus 19 programas, compilados dentro de Hispa-Pentium | 21 s (el núcleo solo: unos 8 s) |
| ... y ensamblados con ENSAMB32 | 7 s (el núcleo solo: unos 3 s; NASM en Linux tarda 14) |
| CASTEL32 compilándose a sí mismo dentro de Hispa-Pentium | 5 s |
| CASTELW compilándose a sí mismo y ensamblándose, en Hispa-DOS desde disquete | 35 s |
| el núcleo con CASTEL32 en Linux (corre32) | 2 s |

### Correr y probar

- **Dentro de Hispa-Pentium**: CASTEL32.BIN y ENSAMB32.BIN son programas del disco
  como los demás: `CASTEL32 SEMILLA.CAS -o \S\SEMILLA.ASM -I \BIB32 --gasto` y
  `ENSAMB32 S\SEMILLA.ASM S\SEMILLA.BIN`. Hispa-Pentium no les dice de qué carpeta
  se han cargado: los módulos se buscan en la del programa que se compila y en las
  de `-I` (hasta cinco). La línea de órdenes, hasta 127 letras.
- **En Hispa-386/486**: `HW32 CASTELW.EXE PROGRAMA.CAS ...` (con HSPDPMI cargado),
  y `HW32 ENSAMBW.EXE PROGRAMA.ASM`. CASTELW busca también en su propia carpeta.
- **`corre32/`** (`corre32.sh PROGRAMA.BIN [args]`): corre en **Linux** un programa
  del anillo 3 de Hispa-Pentium, con ptrace (cada `int 0x80` se para, se anula y se
  hace allí, con los números de `nucleo32/usuario.cas`). Para probar deprisa: el
  núcleo entero en 2 s. No sustituye a las pruebas dentro de Hispa-Pentium.
- **`pruebas.sh`, parte 15** (`SOLO=castel32 ./pruebas.sh`, unos 4 minutos):
  todo lo de arriba.

---

## 12. El camino a x86-64 (Hispa-64)

> **Hecho el 21/11/2026: ver [CASTEL64.md](CASTEL64.md).** Los puntos 1 a 4 de abajo
> están hechos (y el 5, salvo las referencias de 32 bits en las tablas): `modelo
> linux64` y `modelo núcleo64`, `nat64`/`ent64`, `Generador64` en ccastel.py y el
> módulo `Gen64` en el compilador escrito en Castel, `bits 64` en los dos
> ensambladores, y **CASTEL64**, un programa de Linux x86-64 que se compila a sí mismo
> hasta el punto fijo y saca lo mismo que ccastel.py en todos los programas de 16, 32
> y 64 bits. Lo que se afinó del plan: el primer destino no fue el núcleo sino
> **Linux x86-64** (así se prueba todo deprisa, sin emular, antes de tener Hispa-64);
> el núcleo (`núcleo64`) lleva multiboot y multiboot2 con su paso de 32 a 64 bits en
> el arranque; las cuentas son de 32 bits salvo que haya uno de 64 (como LP64), y
> por eso los mismos fuentes del compilador corren en 64 sin tocarlos; y `Gen64` va
> en un módulo propio (con uno vacío para el CASTEL de 16 bits, que no tiene sitio
> para sus textos). CASTEL32 y CASTELW también sacan ya 64 bits. Lo que sigue es lo
> que se escribió el 20/11, tal cual.

El compilador ya está partido como hace falta: **una parte de delante** (léxico,
gramática, tipos, lo declarado: `lexico`, `sintactico`, `tipos`, `tablas`,
`cuentas`, `base`) que no sabe nada de la máquina, y **partes de atrás** que
generan: la de 16 bits (`gen_expr` y los suyos en `expr.cas` y `sent.cas`) y la de
32 (`gen_expr32p`, `llama_función32`... en los mismos módulos), elegidas por el
modelo (`plano`). La de 64 bits será una tercera, y el plan es:

1. **El ensamblador**, primero (`ensambla.py` y luego Montador): `bits 64`, el
   prefijo REX (registros `rax`..`r15`, operandos de 64 bits), el direccionamiento
   relativo a RIP y los inmediatos de 64 bits (`mov rax, imm64`); comprobado contra
   NASM con unas `formas64.asm`, como las de 32.
2. **La parte de atrás en ccastel.py**: un `Generador64` que herede de
   `Generador32` (casi todo es igual cambiando `e` por `r`), con punteros de 8
   octetos, las cuentas en 64 bits y una convención de llamada propia sencilla (los
   argumentos en la pila, de ocho en ocho, como ahora de cuatro en cuatro; ya habrá
   tiempo de pasarlos en registros). El tipo `nat64`/`ent64` en la parte de delante
   (los literales ya se calculan en 64 bits: `cuentas.cas`).
3. **Un modelo nuevo, `modelo hispa64`**: un núcleo que arranca en modo largo
   (multiboot2, o multiboot con su paso de 32 a 64 bits en el arranque que pone el
   compilador, como hoy pone la GDT del de 32) y, luego, los programas de su anillo 3.
4. **La misma parte de atrás en el compilador escrito en Castel** (un módulo
   `Gen64` al lado de las de 16 y 32), comprobada como siempre: el mismo
   ensamblador que ccastel.py en todos los programas.
5. Y, cuando Hispa-64 tenga archivos, **CASTEL64**: el compilador compilado para
   64 bits, con un `Medidas` y un `Lejos` suyos (y ahí sí, referencias de 32 o 64
   bits). Hasta entonces, el compilador de Hispa-64 es CASTEL32 (o ccastel.py): un
   compilador de 32 bits puede sacar código de 64 sin problema.

Lo que se ha hecho el 20/11 para que esto salga barato: los segmentos con su tipo
(`Segmento`), las medidas aparte (`Medidas`), el sistema aparte (Lejos, Archivos y
Argumentos por sistema) y la línea de órdenes y el ensamblador como módulos
(`Mando`, `Montador`): un CASTEL64 será otro programa de veinte líneas como
`castel32.cas`.
