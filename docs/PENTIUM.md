# Hispa-Pentium — el núcleo de la gama XP, escrito en Castel

*18/11/2026. Hispa-Pentium es el miembro de la familia Hispa-DOS para los PC de
2000 en adelante (la gama de Windows XP, [ROADMAP](ROADMAP.md)): un **núcleo propio
de 32 bits, sin DOS por debajo**, con memoria virtual, escrito en
[Castel](../castel/README.md). Esta nota cuenta lo que hay hecho: la semilla de la
Fase 5 de Castel (IDT, reloj, teclado, excepciones) ha crecido hasta tener memoria
con paginación, tareas con el reloj repartiendo el procesador y programas en el
anillo 3. Todo está en `castel/nucleo32/`.*

Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.

---

## 1. Cómo se arranca

```
cd castel
nucleo32/probar.sh "" ayuda memoria paginas turnos "ejecutar 0" apagar
```

`nucleo32/probar.sh` compila el núcleo (`semilla.cas` con sus módulos) y los tres
programas de usuario, arranca QEMU con `-kernel SEMILLA.BIN -initrd
"uhola.bin,umalo.bin,ucli.bin"` y le teclea las órdenes por el monitor de QEMU (como
un teclado de verdad). Lo que escribe el núcleo sale por la pantalla y por el
puerto serie, y el script lo enseña con el código con que acaba. Sin QEMU a mano:
`./compilar.sh nucleo32/semilla.cas` da `SEMILLA.BIN`, que arranca cualquier
cargador multiboot (GRUB: `multiboot /SEMILLA.BIN` y un `module` por programa).

```
Hispa-Pentium (semilla), un núcleo de 32 bits escrito en Castel
memoria: 32 MB, 7867 marcos libres; paginación activa; 3 programas de usuario.
Escriba «ayuda».
> ejecutar 0
ejecuto uhola.bin (la tarea 3, en el anillo 3)
hola desde el anillo 3: soy la tarea 3, en 0x40000000 de mi espacio
1 + 2 + ... + 1000 = 500500
he dormido 10 tics: cierto
el núcleo no me deja enseñar su memoria: cierto
ni lo que no tengo: cierto
y acabo con el código 3
uhola.bin ha acabado con el código 3; marcos libres: 7866 antes y 7866 después
> ejecutar 1
ejecuto umalo.bin (la tarea 4, en el anillo 3)
soy un programa malo: voy a leer la memoria del núcleo
he leído
*** excepción 14: fallo de página en 0x00100000 (lectura, anillo 3, protección) en 0x40000057, en el anillo 3
*** se acaba la tarea 4 (umalo.bin)
umalo.bin ha acabado con el código 255; marcos libres: 7866 antes y 7866 después
```

---

## 2. Las piezas

El núcleo son unas 2.300 líneas de Castel en ocho módulos (más la biblioteca de 32
bits, `castel/biblioteca32/`: Textos32, Mem32 y Formato, CASTEL32.md §7). Sale un
binario de unos 20 K.

| Módulo | Qué hace |
|---|---|
| `cpu.cas` (Cpu) | La **GDT** nueva: código y datos del anillo 0 (0x08, 0x10), del anillo 3 (0x1B, 0x23) y el **TSS** (0x28, cargado con `ltr`). La **IDT** (`pon_puerta`, con puertas del anillo 0 o llamables desde el 3), los dos **8259** (IRQ en 0x20-0x2F) y el **reloj 8253** a 100 Hz. Lo que solo se dice en ensamblador: CR0, CR2, CR3, `cli`/`sti` (`sin_interrupciones` y `reponer`), `hlt`. |
| `multiboot.cas` (Multiboot) | Lo que cuenta el cargador: la memoria alta, el **mapa de memoria** región a región y los **módulos** que ha cargado (los programas de usuario). |
| `marcos.cas` (Marcos) | El **gestor de marcos físicos**: un mapa de bits, un bit por marco de 4 K (hasta 1 GB). Al empezar suelta lo que el mapa dice libre y tapa el primer mega, el propio núcleo, la información del cargador y sus módulos. `reservar_marco` (lo da a cero) y `soltar_marco` (no cuenta dos veces). |
| `paginas.cas` (Paginas) | La **paginación**: directorio y tablas de páginas, `mapear`, `desmapear`, `traducir`; el directorio del núcleo (la identidad) y uno por programa (`nuevo_espacio`, `soltar_espacio`); la **zona a la demanda**; y `usuario_puede`, para que la llamada al sistema no lea donde no debe. |
| `monton.cas` (Monton) | El **montón del núcleo**: `reservar` y `liberar`, con cabecera, pie, relleno y comprobación (sección 4). |
| `tareas.cas` (Tareas) | Las **tareas**: fichas, colas, el **reloj que cambia de tarea**, `ceder`, `dormir`, `bloquear`/`despertar_tarea`, `terminar` y `esperar` (sección 5). |
| `usuario.cas` (Usuario) | Los **programas del anillo 3**: los carga de los módulos de multiboot en su espacio de direcciones y atiende la **llamada al sistema** (`int 0x80`, sección 6). |
| `semilla.cas` (Semilla) | El programa: arranca todo lo anterior; las **excepciones** (con el fallo de página, que se atiende), el **teclado** y la **consola** con sus órdenes (sección 7). |

Y los programas de usuario, también en Castel (`modelo usuario`, CASTEL32.md §1):
`uhola.cas` (escribe, suma, duerme, pide su número de tarea y prueba que la llamada
al sistema no le deja enseñar la memoria del núcleo), `umalo.cas` (lee la memoria
del núcleo) y `ucli.cas` (hace `cli` en el anillo 3).

---

## 3. La memoria

Las direcciones (lo mismo en todos los espacios, menos la parte del usuario):

| Desde | Hasta | Qué |
|---|---|---|
| `0x00000000` | la memoria que haya (hasta 1 GB) | **la identidad**: la memoria de verdad, tal cual. El núcleo, sus variables, la pantalla y los marcos (un marco se toca por su dirección física). Solo el anillo 0. |
| `0x40000000` | `0xBFFFFFFF` | **el programa de usuario** (anillo 3): cada uno, en su directorio. |
| `0xC0000000` | `0xC07FFFFF` | **el montón del núcleo** (8 MB). |
| `0xD0000000` | `0xD03FFFFF` | **la zona a la demanda** (4 MB): vacía; el fallo de página pone un marco donde se toque. |

- Los marcos salen del **mapa de memoria de multiboot**; sin él, de la memoria
  alta que diga el cargador. Con QEMU y 32 MB: 8160 marcos, unos 7870 libres al
  arrancar.
- El directorio del núcleo lleva la identidad con páginas de 4 K (sin páginas
  grandes: vale para el 386) y, hechas desde el principio, las tablas del montón y
  de la zona a la demanda. Así, lo que se ponga ahí después lo ven todos los
  espacios de direcciones, que **copian las entradas del núcleo** al hacerse.
- La paginación se enciende con `CR0.PG` sobre ese directorio. Cambiar de espacio
  es cambiar CR3 (lo hace el cambio de tarea si la siguiente tiene otro). Quitar
  una página del directorio en uso vuelve a cargar CR3 (la TLB se vacía); no se
  usa `invlpg`, que es del 486.
- **El fallo de página** (excepción 14) mira CR2 y el código de error. Si es del
  núcleo, en la zona a la demanda y la página no estaba, le pone un marco a cero y
  vuelve: la instrucción se repite y ahora llega. Si no, dice dónde ha sido y por
  qué (`lectura`/`escritura`, `núcleo`/`anillo 3`, `no presente`/`protección`) y en
  qué instrucción; si era del anillo 3, se acaba esa tarea y el núcleo sigue; si era
  del núcleo, se para (código 14).

## 4. El montón

Bloques seguidos, cada uno con una **cabecera** (magia, tamaño, estado, lo pedido),
lo pedido, un **relleno** de `0xCD` (4 octetos por lo menos) y un **pie** (tamaño y
otra magia). Se toma el primer hueco que valga (partiéndolo si sobra); si no hay,
el montón crece poniendo marcos debajo, de página en página. Al soltar se juntan
los huecos vecinos (nunca hay dos seguidos) y, si lo suelto es lo de arriba del
todo, el montón baja y **devuelve los marcos**.

Al soltar se comprueba todo: que el puntero sea de un bloque, las dos magias, el
tamaño, el estado y que el relleno siga intacto. Un bloque que **se suelta dos
veces**, un puntero que **no es de un bloque** o lo **escrito más allá de lo
pedido** se dicen, y el montón no se toca. `revisar()` recorre el montón entero con
las mismas comprobaciones y cuenta lo que hay.

## 5. Las tareas

Cada tarea tiene su ficha (estado, nombre, su pila del núcleo de 8 K, del montón,
su directorio, cuándo despierta, quién la espera, con qué ha acabado, sus turnos y
sus tics) y la pila guarda lo demás: **lo que hay en la pila de una tarea parada es
lo que dejó la interrupción que la paró** (lo que apila el procesador y lo que
guarda `pushad`). La ficha guarda dónde empieza eso, su «marco».

**Cambiar de tarea es cambiar de marco**: las dos funciones de interrupción que
cambian (el reloj, IRQ 0, y la 0x30, la de `ceder()`) guardan su EBP (que, nada más
entrar, apunta a lo de `pushad`) en la ficha de la que corre, eligen la siguiente y
ponen en EBP el de ella; su salida normal (`mov esp, ebp`, `popad`, `iretd`) la
reanuda. Una tarea nueva lleva su marco hecho a mano, como si la hubieran parado
justo al empezar (y, encima, adónde vuelve su función al acabar). Para el anillo 3,
el marco lleva además la pila y el selector de usuario, y el TSS da al procesador
la pila del núcleo de la tarea que corre.

- **Apropiativa**: a cada tic (100 por segundo), si hay otra tarea lista, le toca a
  ella (por turno, de una cola). Dos tareas que cuentan sin parar y no ceden nunca
  se alternan cien veces por segundo (la orden `turnos`).
- Si no hay nadie listo, corre la **ociosa**: un `hlt` detrás de otro.
- **`dormir(n)`**: la tarea pasa a la lista de las dormidas hasta el tic que toca
  (el reloj las despierta).
- **`bloquear()` y `despertar_tarea(t)`**: esperar a algo. La consola espera así a
  las teclas: el teclado la despierta.
- **`terminar(c)`** y **`esperar(t)`**: una tarea acaba con su código (también la
  que vuelve de su función, con 0); quien la espera la recoge: su pila vuelve al
  montón y, si es del anillo 3, su espacio de direcciones entero vuelve a los
  marcos libres.

## 6. El anillo 3

Un programa de usuario es un binario plano de Castel con `modelo usuario`, con una
cabecera (`HPU3`, dónde va, dónde acaba lo del archivo, dónde lo que empieza a
cero, por dónde se empieza). El cargador lo trae como **módulo de multiboot** y el
núcleo, para ejecutarlo, le hace un **directorio de páginas nuevo**, le pone marcos
desde `0x40000000` hasta el final de sus variables (páginas del usuario), copia lo
del archivo y hace su tarea, que empieza en el anillo 3.

La **llamada al sistema** es la `int 0x80` (una puerta que se puede llamar desde el
anillo 3):

| EAX | Qué | Devuelve |
|---|---|---|
| 1 | escribir ECX octetos desde EBX | los escritos, o `0xFFFFFFFF` si no son del programa |
| 2 | acabar con el código de EBX | (no vuelve) |
| 3 | dormir EBX tics | 0 |
| 4 | los tics desde que arrancó | |
| 5 | el número de la tarea | |

Los registros se leen de lo que guardó `pushad` y lo que se devuelve se deja donde
estaba EAX. Antes de escribir, el núcleo mira, página a página, que el trozo sea del
programa (del usuario y presente): la memoria del núcleo, o lo que no tiene, no lo
enseña. Lo que un programa no puede hacer (leer la memoria del núcleo: fallo de
página de protección; `cli` o tocar un puerto: protección general) acaba con él
(código 255) y el núcleo sigue.

Los registros de datos (DS, ES...) llevan siempre el selector de datos del usuario,
plano: el anillo 0 lo puede usar, y así pasar a una tarea del anillo 3 no obliga a
tocarlos. Solo la pila del núcleo usa el del anillo 0.

## 7. La consola

| Orden | Qué hace |
|---|---|
| `ayuda` | las órdenes |
| `hora` | los segundos y centésimas desde que arrancó |
| `memoria` | los marcos libres y el montón (bloques, octetos, huecos, páginas; reservas, liberaciones y fallos vistos) |
| `mapa` | el mapa de memoria de multiboot |
| `paginas` | la paginación: CR0, el directorio, la identidad (páginas y tablas), el montón y la zona a la demanda |
| `monton` | prueba el montón: 3000 vueltas de reservar y liberar al azar (con lo de dentro comprobado), y mira que todo vuelve a estar como antes: «montón sin fugas» |
| `estropear` | estropea el montón a propósito (escribe de más, suelta dos veces, suelta lo que no es suyo) y enseña que se ve |
| `toca` | escribe en la zona a la demanda: el fallo de página se atiende y la escritura llega |
| `turnos` | dos tareas que cuentan sin ceder durante un segundo: cuántas veces se han alternado |
| `dormir` | tres tareas que duermen 20, 40 y 60 tics (hechas en otro orden) |
| `tareas` | las tareas, con su estado, su anillo, sus turnos y sus tics |
| `programas` | los módulos que ha traído el cargador |
| `ejecutar N` | ejecuta el programa N en el anillo 3 y espera a que acabe; dice su código y los marcos libres antes y después |
| `teclas` | las teclas pulsadas |
| `fallo` | lee donde no hay nada: el fallo de página del núcleo, que lo para (código 14) |
| `cero` | divide por cero (código 9) |
| `apagar` | acaba (código 5) |

(El teclado es el de EE. UU., sin acentos: por eso las órdenes se escriben sin ellos.)

---

## 8. Las pruebas

`castel/pruebas.sh` (la parte 8; `SOLO=32 ./pruebas.sh` hace solo esa y la de los
errores) arranca el núcleo en QEMU varias veces y comprueba: que arranca, lee el
teclado, el reloj corre y la división por cero llega a su excepción; que el mapa de
memoria da los marcos; que la **paginación está activa** con la identidad; que un
**fallo de página se atiende** en la zona a la demanda; que el **montón no pierde**
ni bloques ni marcos tras 3000 vueltas y que **ve lo que lo estropea**; que **dos
tareas se alternan** con el reloj; que las dormilonas despiertan en orden; que un
**programa del anillo 3** escribe por la `int 0x80`, duerme y acaba con su código, y
que su espacio de direcciones vuelve entero; que la llamada al sistema no enseña la
memoria del núcleo; que un programa que lee la memoria del núcleo o hace `cli` se
acaba y el núcleo sigue; y que un fallo de página del núcleo se dice con su
dirección. Además, como con todo lo de Castel: **CASTEL** (el compilador escrito en
Castel, dentro de Hispa-8086) saca el **mismo ensamblador** que `ccastel.py` para el
núcleo y los programas de usuario, y **ENSAMBLA** da el **mismo binario que NASM**.

## 9. Por dónde seguir

1. **Un disco y un sistema de archivos**: un controlador ATA (PIO) y leer FAT, para
   cargar los programas del disco y no solo de multiboot.
2. **Más llamadas al sistema**: leer el teclado desde el anillo 3, abrir y leer
   archivos, crear otro programa y esperarlo; y que `escribir` vaya por una consola
   con su propio cerrojo (hoy cada tarea tapa las interrupciones para escribir).
3. **Un intérprete de órdenes en el anillo 3**, escrito en Castel con `modelo
   usuario`, en vez de la consola del núcleo.
4. **Memoria del usuario que crece** (un `sbrk`) y una pila que crece a la demanda;
   y páginas compartidas o copiadas al escribir cuando haya procesos que se dupliquen.
5. **Prioridades y un planificador mejor**, semáforos y colas de mensajes entre
   tareas.
6. **El lenguaje**: llamar a funciones por su dirección, sumar a un arreglo como a
   un puntero y funciones de interrupción públicas (CASTEL32.md §10), que hoy se
   rodean a mano.
7. **Más tarde**: el cargador de PE (los programas Win32 de Hispa-486, HW32) sobre
   este núcleo, que es la base de la gama XP.
