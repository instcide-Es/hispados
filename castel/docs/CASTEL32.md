# Castel de 32 bits — el modelo plano para Hispa-Pentium

*Fase 5 de CIMIENTOS.md. 18/11/2026. Define cómo Castel genera código de 32 bits
para el 386 en adelante, en modo protegido con la memoria plana: la base para
escribir el núcleo de Hispa-Pentium (la gama XP, sin DOS). Lo de 16 bits
(CASTEL.md) no cambia; esto es un modo nuevo que se pide con `modelo`.*

Regla de oro, la de siempre: **el núcleo, pequeño.** Lo que el lenguaje no sabe
decir se hace en un bloque `ensamblador`, ahora con los registros del 386.

---

## 1. Dos modelos nuevos

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

Los dos comparten **toda la generación de 32 bits** (lo de esta nota); solo
cambian el arranque (sección 8) y adónde va lo que se escribe. A los dos se les
llama aquí **«el modelo plano»**. `modelo`, `núcleo` e `hispa32` no son palabras
reservadas (solo cuentan detrás de `modelo`, al principio del programa).

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

## 7. La biblioteca mínima de 32 bits

Lo justo para que un programa plano escriba y acabe, dentro del propio compilador
(no hay módulos aparte todavía): **`escribir`** (números y textos),
**`escribir_car`** y **`salir`**. Por debajo:

- `castel_salida(ECX octetos desde ESI)` manda el texto adonde toque;
- `castel_salir(código en AL)` acaba el programa.

En `modelo núcleo`, `castel_salida` escribe en la **pantalla de texto VGA** (con
su cursor, subiendo las líneas al llegar abajo) y en el **puerto serie COM1** (a
115200, 8N1: así `probar32.sh` lo lee); `castel_salir` lo dice por el puerto
`0xF4` (el `isa-debug-exit` de QEMU) y para el procesador. En `modelo hispa32`,
`castel_salida` es `WriteFile` y `castel_salir` es `ExitProcess`. Como siempre,
**solo van al programa las rutinas que se usan**.

Lo demás (textos, memoria, archivos) se escribirá en Castel cuando haga falta,
como la biblioteca de 16 bits (CASTEL.md §12), a medida que crezca Hispa-Pentium.

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

- `compilar.sh` reconoce los dos modelos y saca `NOMBRE.BIN` (núcleo) o
  `NOMBRE.EXE` (hispa32).
- **`probar32.sh PROGRAMA`** lo ejecuta y enseña lo que escribe y su código de
  salida: un núcleo con `qemu-system-i386 -kernel` (la salida por el puerto
  serie); un `.EXE` con HW32 dentro de Hispa-DOS.
- **`nucleo32/probar.sh`** arranca la semilla y le teclea órdenes (por el monitor
  de QEMU, como un teclado de verdad): sirve para ver el reloj, el teclado y las
  excepciones.
- `pruebas.sh` (parte 8) comprueba: los ejemplos de 16 bits sin DOS, compilados
  como núcleos de 32 bits, escriben lo mismo; los de `ejemplos32/` (núcleos en
  QEMU y programas Hispa32 con HW32) dan su salida esperada y su código; cuentas
  al azar con las reglas de 32 bits (`azar32.py ... plano`); y la semilla, con su
  IDT, su reloj, su teclado y su excepción de división por cero. Y, como siempre,
  **los dos compiladores** (`ccastel.py` y CASTEL, el escrito en Castel) sacan el
  **mismo** ensamblador también para los programas de 32 bits, y **ENSAMBLA** da
  el **mismo archivo que NASM** para ellos (las instrucciones del 386: prefijos
  de tamaño `66h`, SIB, `0F`, saltos largos...).

---

## 10. Lo que queda para más adelante

- **Más biblioteca de 32 bits** escrita en Castel (textos, memoria, un montón),
  a medida que crezca Hispa-Pentium.
- **Paginación** (la MMU del 386) y el salto a modo usuario: cuando el núcleo lo
  necesite.
- **ELF32** de salida, si algún día hay que enlazar con código de fuera.
- **El núcleo de Hispa-Pentium de verdad**: la semilla (`nucleo32/semilla.cas`)
  es el primer ladrillo; a partir de aquí, todo en Castel.
