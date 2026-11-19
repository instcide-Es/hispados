# Hispa-Pentium — el núcleo de la gama XP, escrito en Castel

*18/11/2026; puesto al día el 19/11/2026 (la 0.4, con el escritorio). Hispa-Pentium es el miembro de la
familia Hispa-DOS para los PC de 2000 en adelante (la gama de Windows XP, [ROADMAP](ROADMAP.md)): un **núcleo propio
de 32 bits, sin DOS por debajo**, con memoria virtual, escrito en
[Castel](../castel/README.md). Esta nota cuenta lo que hay hecho: la semilla de la
Fase 5 de Castel (IDT, reloj, teclado, excepciones) ha crecido hasta tener memoria
con paginación, tareas con el reloj repartiendo el procesador y programas en el
anillo 3; y, desde la noche del 18 al 19/11, **disco duro (ATA) y sistema de
archivos (FAT12/16/32)**, programas que **se cargan del disco**, más llamadas al
sistema (teclado, archivos, lanzar y esperar programas, memoria que crece) y un
**intérprete de órdenes en el anillo 3** (DIR, TYPE, VER, CLS, MEM, AYUDA y lanzar
programas por su nombre). En la tanda del 19/11 de madrugada: **se escribe en el
disco** (crear, escribir, alargar, cortar y borrar archivos; carpetas; COPY, DEL,
REN, MD, RD y la redirección `>` y `>>`), el intérprete tiene **carpeta de trabajo
(CD), PATH, archivos .BAT, historia de la línea y Ctrl+C**, el **teclado es el
español** (acentos con tecla muerta, ñ, ç, ¿ ¡) y se leen los **nombres largos**
(VFAT). Y en la del 19/11 por la mañana (la **0.4**): el primer **entorno gráfico**,
un **escritorio** con el aire de los de principios de los 2000 y diseño propio
(modo gráfico de 32 bits, ratón, ventanas con su barra de título redondeada, barra
de tareas con el botón «Inicio», menú Inicio de dos columnas, reloj, un Bloc de
notas y una Consola con el intérprete de órdenes dentro: sección 10); y lo que
hace falta para probarlo en **PC de verdad** (sección 11): una memoria USB con
GRUB, los programas del cargador como **unidad en memoria** cuando no hay disco, y
**no escribir en el disco del PC** si no se pide. Todo está en `castel/nucleo32/`.*

![El escritorio de Hispa-Pentium](pentium/escritorio-ventanas.png)

Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.

---

## 1. Cómo se arranca

**El escritorio** (sección 10), en QEMU, con su ventana (en un PC con escritorio;
o por VNC con `-display vnc=:1`):

```
cd castel
nucleo32/disco.sh /dev/shm/pentium.img
./compilar.sh nucleo32/semilla.cas /dev/shm/SEMILLA.BIN
qemu-system-i386 -kernel /dev/shm/SEMILLA.BIN -append "escritorio escritura" -m 32 -rtc base=localtime \
  -drive file=/dev/shm/pentium.img,format=raw,if=ide,index=0
```

(`escritorio` arranca el entorno gráfico; `escritura` deja escribir en el disco,
que si no es de solo lectura: sección 7. Con `modo=1024x768`, ese tamaño; con
`tapiz=1` o `tapiz=2`, otro fondo. Desde el intérprete de órdenes, la orden
`ESCRITORIO`; desde la consola del núcleo, `escritorio`.)

**La consola de texto**, con un disco:

```
cd castel
nucleo32/disco.sh /dev/shm/pentium.img            # un disco FAT16 con el intérprete y los programas
DISCO=/dev/shm/pentium.img ARRANQUE=ordenes MODULOS= nucleo32/probar.sh "" dir "type leeme.txt" "eco hola y adios" malo mem salir apagar
ESCRIBE=1 DISCO=/dev/shm/pentium.img ARRANQUE=ordenes MODULOS= nucleo32/probar.sh "" "md cartas" "echo hola > cartas\\una.txt" "dir cartas" salir apagar
fsck.fat -n /dev/shm/pentium.img                  # el disco, visto desde fuera: sano
```

`nucleo32/disco.sh IMAGEN [12|16|32|mbr]` hace la imagen de un disco duro con
`mkfs.fat` y `mtools`: FAT16 (16 MB) si no se dice otra cosa, FAT12 (4 MB), FAT32
(40 MB) o un disco con tabla de particiones y la FAT16 en la primera. Dentro van
`ORDENES.BIN` (el intérprete de órdenes), `HOLA.BIN`, `ECO.BIN`, `MALO.BIN`,
`CLI.BIN`, `PRUEBA.BIN`, `ESCRIBE.BIN`, `BUCLE.BIN`, tres archivos de órdenes
(`LOTE.BAT`, `SINFIN.BAT`, `VISTO.BAT`), `LEEME.TXT` y las carpetas `TEXTOS` y
`LARGOS` (esta, con nombres largos).

`nucleo32/probar.sh` compila el núcleo (`semilla.cas` con sus módulos), arranca
QEMU con `-kernel SEMILLA.BIN`, con la imagen de `DISCO=` como disco IDE (y con
`escritura` en la línea del núcleo, salvo con `SOLO_LECTURA=1`) y le
teclea las órdenes por el monitor de QEMU (como un teclado de verdad: el español,
tecla a tecla, con sus acentos; `:tecla ctrl-c` o `:tecla up ret` pulsan esas
teclas tal cual; y, para el escritorio, `:clic X Y`, `:doble X Y`, `:arrastra X1 Y1
X2 Y2`, `:raton X Y`, `:escribe texto`, `:pausa S` y `:captura NOMBRE`, que deja
la pantalla en `$CAPTURAS/NOMBRE.ppm`). **Lo que se escribe en el disco no llega a la imagen** (se queda
en una copia de QEMU) **salvo con `ESCRIBE=1`**. Lo que se escribe sale por la pantalla y por el puerto serie, y el
script lo enseña con el código con que acaba el núcleo. Con `ARRANQUE=ordenes`
(lo que QEMU pasa con `-append`), el núcleo lanza el intérprete de órdenes nada
más arrancar; sin eso, se queda en su consola (la del núcleo, sección 9), y el
intérprete se lanza con la orden `ordenes`. Sin `DISCO=` el núcleo arranca igual,
con los programas de siempre como módulos de multiboot (`-initrd`; `MODULOS=`
para que no lleve ninguno).

**Para verlo y teclear uno mismo** (en un terminal de texto; se sale con Esc y
luego 2, `quit`; o tecleando `salir` y `apagar`):

```
cd castel
nucleo32/disco.sh /dev/shm/pentium.img
./compilar.sh nucleo32/semilla.cas /dev/shm/SEMILLA.BIN
qemu-system-i386 -kernel /dev/shm/SEMILLA.BIN -append "ordenes escritura" -m 32 -display curses -rtc base=localtime \
  -drive file=/dev/shm/pentium.img,format=raw,if=ide,index=0 -device isa-debug-exit,iobase=0xf4,iosize=0x04
```

(Así, con `escritura`, **sí se escribe en la imagen**: lo que se cree o se borre queda en
`/dev/shm/pentium.img`, y se puede mirar luego con `mdir -i /dev/shm/pentium.img ::`
o comprobar con `fsck.fat -n`. El teclado es el español. **Ojo con `-display
curses`**: ahí QEMU no recibe teclas, sino las letras que le manda el terminal, y
las convierte en teclas de un teclado de EE. UU.: hay que arrancar con
`-append "ordenes escritura teclado=us"` (probado: van todas las letras y signos del ASCII,
con su `\` y su `>`; no la eñe ni los acentos). Con `-k es` QEMU las convierte al
teclado español y la eñe sale, pero no sabe dar las de Alt Gr ni las vocales con
acento. Para ver el teclado español entero, la ventana de QEMU en un PC con
escritorio (sin `-display curses`) o un PC de verdad: ahí cada tecla llega como
es.) Sin QEMU: `SEMILLA.BIN` lo arranca cualquier cargador multiboot
(GRUB: `multiboot /SEMILLA.BIN ordenes`), y el disco es el primero del canal IDE
primario.

```
Hispa-Pentium (semilla), un núcleo de 32 bits escrito en Castel
memoria: 32 MB, 7862 marcos libres; paginación activa; 0 programas de usuario.
disco 0: QEMU HARDDISK, 16 MB, FAT16.
lanzo ORDENES.BIN del disco (la tarea 3, en el anillo 3)

Hispa-Pentium: intérprete de órdenes (un programa del anillo 3, la tarea 3).
Escriba AYUDA para ver las órdenes.
C:\>md cartas
C:\>cd cartas
C:\CARTAS>echo Querida señora: ¿qué tal está? > maría.txt
C:\CARTAS>type \leeme.txt >> maría.txt
C:\CARTAS>copy maría.txt copia.txt
        1 archivo(s) copiado(s).
C:\CARTAS>ren copia.txt otra.txt
C:\CARTAS>dir
 Directorio de C:\CARTAS

.             <DIR>       19/11/2026  03:23
..            <DIR>       19/11/2026  03:23
MARIA    TXT         150  19/11/2026  03:23
OTRA     TXT         150  19/11/2026  03:23
       2 archivos           300 octetos
       2 carpetas      16631808 octetos libres
C:\CARTAS>del otra.txt
C:\CARTAS>cd ..
C:\>lote uno dos
lote: empiezo con uno y dos (y el 100%)
desde el lote
(ECO.BIN ha acabado con el código 3)
lote: una línea
lote: y otra

Pulse una tecla para seguir . . . 
lote: fin
C:\>bucle
bucle: doy vueltas sin fin (Ctrl+C para acabar)
^C
C:\>salir
Hasta luego.
ORDENES.BIN ha acabado con el código 0; marcos libres: 7862 antes y 7862 después
```

(Lo de arriba sale de `ESCRIBE=1 DISCO=/dev/shm/pentium.img ARRANQUE=ordenes MODULOS=
nucleo32/probar.sh "" "md cartas" "cd cartas" "echo Querida señora: ¿qué tal está? >
maría.txt" ... "lote uno dos" ":tecla spc" bucle :espera ":tecla ctrl-c" salir
apagar`; después, `fsck.fat -n /dev/shm/pentium.img` da el disco por sano y `mdir -i
/dev/shm/pentium.img ::CARTAS` enseña `MARIA.TXT`.)

---

## 2. Las piezas

El núcleo son unas 9.000 líneas de Castel en diecinueve módulos (más la biblioteca
de 32 bits, `castel/biblioteca32/`: Textos32, Mem32 y Formato, CASTEL32.md §7). Sale
un binario de unos 88 K (casi la mitad, el entorno gráfico).

| Módulo | Qué hace |
|---|---|
| `cpu.cas` (Cpu) | La **GDT** nueva: código y datos del anillo 0 (0x08, 0x10), del anillo 3 (0x1B, 0x23) y el **TSS** (0x28, cargado con `ltr`). La **IDT** (`pon_puerta`, con puertas del anillo 0 o llamables desde el 3), los dos **8259** (IRQ en 0x20-0x2F) y el **reloj 8253** a 100 Hz. Lo que solo se dice en ensamblador: CR0, CR2, CR3, `cli`/`sti` (`sin_interrupciones` y `reponer`), `hlt`. |
| `multiboot.cas` (Multiboot) | Lo que cuenta el cargador: la memoria alta, el **mapa de memoria** región a región, los **módulos** que ha cargado y lo que se le ha dicho al núcleo al arrancarlo (`-append`). |
| `marcos.cas` (Marcos) | El **gestor de marcos físicos**: un mapa de bits, un bit por marco de 4 K (hasta 1 GB). Al empezar suelta lo que el mapa dice libre y tapa el primer mega, el propio núcleo, la información del cargador, sus textos (lo que se le dice al núcleo y los nombres de los módulos: QEMU los deja justo detrás del núcleo) y sus módulos. `reservar_marco` (lo da a cero) y `soltar_marco` (no cuenta dos veces). |
| `paginas.cas` (Paginas) | La **paginación**: directorio y tablas de páginas, `mapear`, `desmapear`, `traducir`; el directorio del núcleo (la identidad) y uno por programa (`nuevo_espacio`, `soltar_espacio`); la **zona a la demanda**; y `usuario_puede`, para que la llamada al sistema no toque lo que no es del programa. |
| `monton.cas` (Monton) | El **montón del núcleo**: `reservar` y `liberar`, con cabecera, pie, relleno y comprobación (sección 4). |
| `tareas.cas` (Tareas) | Las **tareas**: fichas, colas, el **reloj que cambia de tarea**, `ceder`, `dormir`, `bloquear`/`despertar_tarea`, `terminar` y `esperar`; los **cerrojos** (`tomar`, `soltar`) y las tareas huérfanas (sección 5). |
| `consola.cas` (Consola) | El **teclado** (IRQ 1: el español, con teclas muertas, Alt Gr y Bloq Mayús; su cola de letras, `leer_tecla`; Ctrl+C) y la **pantalla**, cada uno con su cerrojo: `escribir_trozo` (una ristra entera sin que se cuele otra tarea), `borrar_pantalla` (secciones 5 y 9). |
| `ata.cas` (Ata) | Los **discos duros ATA** del canal primario, en PIO y con LBA de 28 bits: `identificar` (IDENTIFY: modelo y sectores), `leer_sectores`, `escribir_sectores` y `asentar` (sección 7). |
| `fat.cas` (Fat) | El **sistema de archivos FAT12/16/32**, para leer y escribir: montar (disco entero o primera partición FAT), abrir por la ruta, leer, escribir, cortar, borrar, cambiar de nombre, hacer y borrar carpetas, los nombres largos, los clusters libres (sección 7). |
| `fecha.cas` (Fecha) | La **fecha y la hora** del reloj de pila del PC (la CMOS), como las guarda la FAT. |
| `usuario.cas` (Usuario) | Los **programas del anillo 3**: los carga **del disco** (o de los módulos de multiboot) en su espacio de direcciones y atiende la **llamada al sistema** (`int 0x80`, sección 6). |
| `video.cas` (Video) | El **modo gráfico**: el que haya dejado el cargador (multiboot) o el de la **VBE de Bochs/QEMU**; la memoria de la pantalla, a la vista del núcleo; y, si el modo no es de 32 bits, el volcado a 24 o a 16 (sección 10). |
| `grafico.cas` (Grafico), `letra.cas` (Letra) | **Dibujar**: los lienzos; rectángulos, degradados, esquinas redondas, líneas, círculos, el disquete de la casa, texto con la **letra de 8x16** (la de Hispa Ventanas, hecha por `hazletra.py`) y copiar de un lienzo a otro. |
| `raton.cas` (Raton) | El **ratón PS/2** (IRQ 12): dónde está la flecha y la cola de clics. |
| `ventanas.cas` (Ventanas) | El **gestor de ventanas**: un lienzo por ventana, el marco, el orden, componer solo lo que cambia con doble búfer, la flecha, y repartir el ratón y las teclas. |
| `terminal.cas` (Terminal) | Los **terminales**: dos tubos para que una ventana tenga dentro al intérprete de órdenes y a sus programas. |
| `escritorio.cas` (Escritorio) | El **escritorio**: el tapiz, la barra de tareas, el menú Inicio, el reloj, los cuadros de apagar y de configuración, y la tarea que lo mueve todo. |
| `semilla.cas` (Semilla) | El programa: arranca todo lo anterior; las **excepciones** (con el fallo de página, que se atiende) y la **consola del núcleo** con sus órdenes (sección 9). |

Y, para el anillo 3 (`modelo usuario`, CASTEL32.md §1), también en Castel:

| Archivo | En el disco | Qué es |
|---|---|---|
| `llamadas.cas` (Llamadas) | — | El **módulo de las llamadas al sistema** para los programas: `tecla`, `leer_línea` (con su historia), `abrir`, `crear`, `leer`, `escribir_archivo`, `mover`, `cortar`, `cerrar`, `borrar`, `renombrar`, `crear_carpeta`, `cambiar_carpeta`, `redirigir`, `entrada`, `lanzar`, `esperar`, `argumentos`, `dato`, `más_memoria`...; y las **ventanas** (`ventana`, `suceso`, `rectángulo`, `degradado`, `texto`, `mapa`...) y los terminales. Un programa lo importa y no escribe ni una `int 0x80`. |
| `ubloc.cas` | `BLOC.BIN` | El **Bloc de notas** del escritorio (sección 10). |
| `uconsola.cas` | `CONSOLA.BIN` | La **Consola** del escritorio: el intérprete de órdenes dentro de una ventana (sección 10). |
| `uordenes.cas` | `ORDENES.BIN` | El **intérprete de órdenes** (sección 8). |
| `uhola.cas` | `HOLA.BIN` | Escribe, suma, duerme, pide su número de tarea y prueba que la llamada al sistema no le deja enseñar la memoria del núcleo. |
| `ueco.cas` | `ECO.BIN` | Enseña sus argumentos (y acaba con cuántas palabras eran). |
| `umalo.cas`, `ucli.cas` | `MALO.BIN`, `CLI.BIN` | Los que hacen lo que no deben: leer la memoria del núcleo y hacer `cli`. |
| `uprueba.cas` | `PRUEBA.BIN` | Prueba las llamadas al sistema desde dentro: la memoria que crece, leer un archivo a trozos, abrir de más, punteros que no son suyos, lanzar y esperar, escribir dos a la vez. |
| `uescribe.cas` | `ESCRIBE.BIN` | Prueba **escribir en el disco** desde dentro: crear, escribir a trozos, leer de vuelta, escribir en medio, cortar, alargar con hueco, añadir; lo que no se puede; las carpetas; con `lleno`, llena el disco y lo vacía. |
| `ubucle.cas` | `BUCLE.BIN` | El que no acaba nunca (da vueltas, duerme, espera teclas o escribe sin parar): para probar **Ctrl+C**. |

---

## 3. La memoria

Las direcciones (lo mismo en todos los espacios, menos la parte del usuario):

| Desde | Hasta | Qué |
|---|---|---|
| `0x00000000` | la memoria que haya (hasta 1 GB) | **la identidad**: la memoria de verdad, tal cual. El núcleo, sus variables, la pantalla y los marcos (un marco se toca por su dirección física). Solo el anillo 0. |
| `0x40000000` | `0xBFFFFFFF` | **el programa de usuario** (anillo 3): cada uno, en su directorio. Su código y sus datos; detrás, lo que pida de más (`más_memoria`, sección 6); y arriba del todo (desde `0xBFC00000`), **su pila**, que crece hacia abajo. |
| `0xC0000000` | `0xC3FFFFFF` | **el montón del núcleo** (hasta 64 MB: de él salen los lienzos de las ventanas, el doble búfer y el tapiz). |
| `0xD0000000` | `0xD03FFFFF` | **la zona a la demanda** (4 MB): vacía; el fallo de página pone un marco donde se toque. |
| `0xE0000000` | `0xE0FFFFFF` | **la pantalla gráfica** (16 MB): la memoria de la tarjeta, que está muy por encima de la RAM, puesta aquí cuando se entra en un modo gráfico. Sus tablas están hechas desde el arranque: la ven todos los espacios de direcciones. |

- Los marcos salen del **mapa de memoria de multiboot**; sin él, de la memoria
  alta que diga el cargador. Con QEMU y 32 MB: 8160 marcos, unos 7835 libres al
  arrancar.
- El directorio del núcleo lleva la identidad con páginas de 4 K (sin páginas
  grandes: vale para el 386) y, hechas desde el principio, las tablas del montón y
  de la zona a la demanda. Así, lo que se ponga ahí después lo ven todos los
  espacios de direcciones, que **copian las entradas del núcleo** al hacerse.
- La paginación se enciende con `CR0.PG` sobre ese directorio. Cambiar de espacio
  es cambiar CR3 (lo hace el cambio de tarea si la siguiente tiene otro). Quitar
  una página del directorio en uso vuelve a cargar CR3 (la TLB se vacía); no se
  usa `invlpg`, que es del 486. **También se vacía al quitar una página del
  núcleo** (del montón o de la zona a la demanda) aunque el directorio puesto sea
  el de un programa: el núcleo trabaja ahora dentro de las llamadas al sistema,
  con el directorio del programa, y esas páginas están en todos. (Sin eso, el
  montón que bajaba y volvía a subir escribía en el marco viejo: salió al lanzar
  dos programas a la vez desde el anillo 3.)
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
- **Padres y huérfanas**: cada tarea sabe quién la hizo. Si un programa acaba sin
  esperar a los que lanzó, se quedan huérfanos y **los recoge el núcleo** cuando
  acaban (`recoger_huérfanas`, al hacer una tarea nueva y en la consola).
- **Los cerrojos** (`tomar(c)` y `soltar(c)`): lo que es de todos y tarda (la
  pantalla, el teclado, el canal del disco, la FAT) lo usa una tarea cada vez. La
  que encuentra el cerrojo tomado **espera en su cola, sin tapar las
  interrupciones**: el reloj sigue repartiendo el procesador entre las demás. Quien
  lo suelta se lo da a la primera de la cola. La misma tarea lo puede tomar varias
  veces.
- **La consola con su cerrojo** (`consola.cas`): la llamada al sistema `escribir`
  saca su ristra entera con el cerrojo de la pantalla tomado, así que dos
  programas que escriben a la vez no se mezclan las letras (las pruebas lanzan dos
  que escriben 120 líneas cada uno sin parar: salen las 240 enteras). La tecla de
  retroceso mueve el cursor un sitio atrás (también en la pantalla). Lo que
  escribe el núcleo con `escribir()` va directo, sin cerrojo (una excepción
  siempre puede decir lo suyo); una tarea del núcleo que quiere una línea entera la
  envuelve en `tomar_consola()` y `soltar_consola()` (antes tapaba las
  interrupciones). Leer el teclado también va con cerrojo: si varias tareas leen,
  esperan en fila.
- **Ctrl+C acaba los programas en marcha.** Cuando el teclado lo ve, marca los
  programas del anillo 3 (menos los que han dicho que lo atienden ellos: un
  intérprete de órdenes) para acabar, y despierta a los que dormían o esperaban
  una tecla o a otro programa. **Una tarea solo se acaba cuando no está a medias
  de nada del núcleo**: al entrar o al salir de una llamada al sistema, o cuando
  el reloj la pilla corriendo en el anillo 3 (así se para también un programa que
  da vueltas sin llamar nunca al sistema). Se le cierran sus archivos (lo escrito
  queda en el disco), sale `^C` y acaba con el código 254; quien la esperaba
  sigue. A los que lo atienden ellos les llega como una tecla (el 3) si no había
  nadie a quien acabar, y se les apunta para que puedan preguntarlo (`hubo_corte`).
- **La carpeta de trabajo** es de cada tarea (un texto en su ficha: «» la raíz, o
  `\UNA\OTRA`); la hereda de quien la lanza, igual que **adónde va lo que
  escribe** (a la pantalla, o a un archivo: la redirección).

## 6. El anillo 3

Un programa de usuario es un binario plano de Castel con `modelo usuario`, con una
cabecera (`HPU3`, dónde va, dónde acaba lo del archivo, dónde lo que empieza a
cero, por dónde se empieza). El núcleo lo carga **de un archivo del disco** (lo lee
entero a su montón) o de un módulo de multiboot: le hace un **directorio de páginas
nuevo**, le pone marcos desde `0x40000000` hasta el final de sus variables (páginas
del usuario), copia lo del archivo y hace su tarea, que empieza en el anillo 3, con
sus argumentos guardados aparte (un texto).

La **llamada al sistema** es la `int 0x80` (una puerta que se puede llamar desde el
anillo 3). Lo que se pide va en EAX y sus datos en EBX, ECX y EDX; lo que se
devuelve, en EAX. Un programa en Castel no la escribe: `escribir` y `salir` son del
lenguaje, y lo demás lo da el módulo **Llamadas** (`llamadas.cas`).

| EAX | Qué | En `Llamadas` | Devuelve |
|---|---|---|---|
| 1 | escribir ECX octetos desde EBX | `escribir()`, `escribir_trozo` | los escritos |
| 2 | acabar con el código de EBX | `salir()`, volver de `principal` | (no vuelve) |
| 3 | dormir EBX tics | `dormir` | 0 |
| 4 | los tics desde que arrancó | `tics` | |
| 5 | el número de la tarea | `mi_tarea` | |
| 6 | **leer una tecla** (EBX = 0: la espera; 1: solo si la hay) | `tecla`, `tecla_si_hay`, `leer_línea` | la letra (página 437); 0x100 + su código si no es letra (las flechas...); o 0 |
| 7 | **abrir** el archivo o la carpeta de nombre EBX, del modo ECX (0 leer; 1 leer y escribir; 2 crearlo o dejarlo vacío; 3 escribir al final, creándolo si no está) | `abrir`, `abrir_para`, `crear` | su manejador |
| 8 | **leer** del archivo EBX, a ECX, hasta EDX octetos | `leer` | los leídos (0: el final) |
| 9 | **cerrar** el archivo EBX | `cerrar` | |
| 10 | **lanzar** el programa de nombre EBX con los argumentos ECX | `lanzar` | el número de su tarea |
| 11 | **esperar** a que acabe la tarea EBX (una lanzada por uno mismo) | `esperar` | su código de salida |
| 12 | la entrada siguiente de la carpeta EBX, a ECX (32 octetos) | `entrada` | 1, o 0 si no hay más |
| 13 | lo que mide el archivo EBX | `largo_de` | |
| 14 | los argumentos del programa, a EBX (caben ECX) | `argumentos` | su largo |
| 15 | un dato del sistema (marcos libres y totales, páginas del programa, versión, tareas, la FAT, los clusters libres...) | `dato` | |
| 16 | borrar la pantalla | `borrar_pantalla` | |
| 17 | **mover el final de la memoria** del programa EBX octetos (con signo) | `más_memoria`, `menos_memoria` | el final de antes |
| 18 | **escribir** en el archivo EBX, desde ECX, EDX octetos | `escribir_archivo` | los escritos (menos si se llena el disco) |
| 19 | **mover la posición** del archivo EBX a ECX octetos (con signo) del principio (EDX = 0), de donde está (1) o del final (2) | `mover` | la posición nueva |
| 20 | **cortar** el archivo EBX en su posición (o alargarlo con ceros hasta ella) | `cortar` | 0 |
| 21 | **borrar** el archivo de nombre EBX | `borrar` | 0 |
| 22 | **cambiar el nombre** EBX por el nombre ECX (un archivo, también a otra carpeta) | `renombrar` | 0 |
| 23 | **hacer la carpeta** de nombre EBX | `crear_carpeta` | 0 |
| 24 | **borrar la carpeta** de nombre EBX (vacía) | `borrar_carpeta` | 0 |
| 25 | la **carpeta de trabajo** pasa a ser la de nombre EBX | `cambiar_carpeta` | 0 |
| 26 | la carpeta de trabajo, a EBX (caben ECX) | `carpeta_actual` | su largo |
| 27 | **redirigir**: lo que escriba el programa (y los que lance) va al archivo EBX, abierto para escribir (0xFFFFFFFF: otra vez a la pantalla) | `redirigir` | 0 |
| 28 | la entrada siguiente de la carpeta EBX, a ECX, y su **nombre largo**, a EDX (256 octetos) | `entrada_larga` | 1, o 0 si no hay más |
| 29 | **Ctrl+C**: lo atiende el programa (EBX = 1) o lo acaba (0) | `atender_corte` | 0 |
| 30 | ¿se ha pulsado Ctrl+C desde la última vez que se preguntó? | `hubo_corte` | 1 o 0 |
| 31 | el teclado: el español (EBX = 1) o el de EE. UU. (0); con otra cosa, solo dice cuál hay | `poner_teclado`, `teclado_español` | 1 o 0 |
| 32 | **una ventana nueva**: su nombre en EBX, y el ancho (ECX) y el alto (EDX) de lo de dentro | `ventana` | su número |
| 33 | cerrar la ventana EBX | `cerrar_ventana` | 0 |
| 34 | **dibujar** en la ventana EBX lo que dice la orden de ECX (qué, dónde, lo que mide, dos colores, unos datos y un número): un rectángulo, un degradado, uno redondeado, un borde, una línea, texto (fino, en negrita o sobre fondo), un mapa de puntos de 32 bits, desplazar un trozo, el disquete | `rectángulo`, `degradado`, `redondeado`, `marco`, `línea`, `texto`, `texto_negrita`, `texto_fondo`, `mapa`, `desplazar`, `disquete` | 0 |
| 35 | **el suceso siguiente** de la ventana EBX, a ECX (16 octetos); con EDX = 1, lo espera | `suceso` | 1, o 0 si no hay |
| 36 | cambiar el nombre (EBX: la ventana; ECX: el nombre) | `título` | 0 |
| 37 | **los terminales** (sección 10): EBX = 0, hacer uno; 1, leer lo que han escrito los de dentro (a ECX, hasta EDX octetos); 2, una tecla para ellos (ECX); 3, Ctrl+C | `hacer_terminal`, `leer_terminal`, `teclear`, `cortar_terminal` | |
| 38 | **poner en marcha el escritorio** | `escritorio` | 1 o 0 |

(La 11, con ECX = 1, no se queda esperando: da `SIGUE` si la tarea no ha acabado
—`acabada`—. La 15 da además el ancho y el alto de la pantalla gráfica y si el
escritorio está en marcha. Con un terminal, la 1, la 6 y la 16 van a él.)

- **Los errores** son números de `0xFFFFFFF0` para arriba (`es_error(r)`): no
  existe (`NO_HAY`), no hay disco, demasiados archivos abiertos, el manejador no
  vale o el disco falla, no es un programa, no hay memoria, no vale lo pedido; y
  los de escribir: no queda sitio (`DISCO_LLENO`), ya existe, está en uso, es de
  solo lectura, la carpeta no está vacía, el nombre no vale, es una carpeta, no
  es una carpeta. `texto_error(e)` lo dice en español.
- **Los nombres de archivo** que no empiezan por una barra se entienden **desde la
  carpeta de trabajo** de la tarea; `.` y `..` valen en cualquier sitio de la ruta
  (los resuelve el núcleo antes de ir al disco). `C:` delante se admite.
- **Un archivo abierto para escribir es de uno solo**: mientras tanto no se puede
  abrir otra vez (ni para leer), ni borrar, ni cambiar de nombre («está en uso»).
  Varios pueden leer el mismo a la vez. Si la tarea acaba (o la acaban) sin
  cerrarlo, el núcleo lo cierra por ella y lo escrito queda.
- **La redirección** (27) es del núcleo: lo que la tarea escribe con `escribir` va
  al archivo en vez de a la pantalla, con los **finales de línea del DOS** (un
  avance de línea suelto sale como retorno de carro y avance). La heredan los
  programas que lance; lo que dice el núcleo de ellos (una excepción, `^C`) sigue
  saliendo por la pantalla.
- **Lo que el programa da como dirección se comprueba** antes de tocarlo, página a
  página: tiene que ser suyo (y, para leer del disco en él, escribible). Un nombre
  de archivo se copia al núcleo mirando cada página por la que pasa. La memoria del
  núcleo, o lo que no tiene, ni se enseña ni se pisa.
- **Con las interrupciones abiertas**: mientras el núcleo atiende una llamada, el
  reloj sigue cambiando de tarea (leer del disco tarda). Lo de todos va con su
  cerrojo o tapándolas un momento (sección 5).
- **Los archivos abiertos son de la tarea que los abrió** (16 a la vez en todo el
  sistema): otra tarea no puede usar su manejador, y al acabar la tarea (o si el
  núcleo la acaba) se le cierran solos.
- **Lanzar y esperar**: `lanzar` da el número de la tarea nueva; `esperar` solo
  vale para las que ha lanzado uno mismo, y da su código de salida (255 si el
  núcleo tuvo que acabarla). Lo que no se espera lo recoge el núcleo.
- **La pila crece sola**: la pila del programa no va dentro de sus variables: el
  núcleo se la pone arriba del todo de su zona (acaba en `0xC0000000`), con una
  sola página; cuando la pila baja a una página que no tiene, el **fallo de página
  le pone una nueva** y el programa sigue sin enterarse (hasta 4 MB). Solo se
  atiende lo que cae de por donde va la pila (ESP) para arriba, o justo debajo
  (lo que una instrucción apila): una dirección cualquiera de esa zona sigue
  siendo un fallo. Si el programa le da al núcleo un arreglo local que aún no ha
  tocado (para leer del disco en él), el núcleo le pone esas páginas antes. Una
  pila que se desboca llega a su tope (la última página no se pone nunca) y el
  programa se acaba, no el núcleo.
- **La memoria que crece** (el `sbrk` de siempre): `más_memoria(n)` pone páginas
  nuevas, a cero, detrás de lo que tiene el programa, y `menos_memoria(n)` las
  devuelve (tocar luego ahí es un fallo de página, y se acaba el programa). Al
  acabar, todo vuelve a los marcos libres.

Lo que un programa no puede hacer (leer la memoria del núcleo: fallo de página de
protección; `cli` o tocar un puerto: protección general) acaba con él (código 255)
y el núcleo sigue.

Los registros de datos (DS, ES...) llevan siempre el selector de datos del usuario,
plano: el anillo 0 lo puede usar, y así pasar a una tarea del anillo 3 no obliga a
tocarlos. Solo la pila del núcleo usa el del anillo 0.

## 7. El disco y los archivos

**El controlador ATA** (`ata.cas`): el canal IDE primario (puertos `0x1F0`-`0x1F7`
y `0x3F6`), sus dos discos, en **PIO** y con **LBA de 28 bits** (hasta 128 GB).
`identificar()` manda IDENTIFY a cada uno: si contesta, guarda su modelo y cuántos
sectores tiene (un lector de CD, ATAPI, se reconoce por su firma y se deja). 
`leer_sectores(disco, lba, n, destino)` manda la orden de leer (hasta 255 sectores
de una vez) y trae cada sector de palabra en palabra por el puerto de datos. No
usa la interrupción del disco (se le dice que no la dé): mira el estado hasta que
el dato está, con un tope de vueltas por si el disco no contesta. Un cerrojo hace
que las tareas usen el canal de una en una.

**La FAT** (`fat.cas`). Leer:

- **FAT12, FAT16 y FAT32**, según el número de clusters (como manda la norma).
  El volumen puede ser el **disco entero** o la **primera partición FAT** de la
  tabla de particiones del MBR (clases 01, 04, 06, 0B, 0C, 0E).
- **Rutas** con `\` o `/`, con carpetas (`TEXTOS\PIRATA.TXT`), sin distinguir
  mayúsculas; `C:` delante se admite. Nombres de 8+3, y los largos (abajo).
- Un archivo o una carpeta abiertos son un **manejador**; de un archivo se leen
  octetos (siguiendo su cadena de clusters por la FAT; en FAT12, con las entradas
  de 12 bits a caballo entre dos sectores); de una carpeta, sus entradas una a una
  (nombre, atributos, tamaño, fecha y hora). La raíz de FAT12/16, que no va por
  clusters, se lee igual.
- En la memoria hay **un sector de datos y uno de la FAT**; cada lectura va al
  disco sector a sector. Es sencillo y lento: para cargar programas de unos K,
  sobra.
- Los **clusters libres**: en FAT12/16 se cuentan en la FAT; en FAT32, lo que diga
  su sector de información.

Al arrancar, el núcleo identifica los discos y monta el primero si es FAT (lo dice
en una línea). Con `sindisco` en la línea del cargador, ni los mira.

**En el disco no se escribe si no se pide.** En un PC de verdad, el disco del canal
IDE primario es el del dueño, con sus cosas. Así que, de fábrica, el disco es **de
solo lectura**: el controlador no manda ni una orden de escribir, y crear, escribir,
borrar, cambiar de nombre o hacer carpetas da «es de solo lectura». Solo si el
cargador se lo dice al núcleo (**`escritura`** en su línea: `-append "ordenes
escritura"`, o en la entrada de GRUB) se escribe. (`probar.sh` lo dice él solo
cuando se le da una imagen de disco; con `SOLO_LECTURA=1`, no.)

**Sin disco: la unidad en memoria.** Si no hay disco FAT (un PC sin IDE, arrancado
de una memoria USB con GRUB: sección 11) pero el cargador ha traído archivos como
**módulos de multiboot**, esos módulos hacen de **unidad C: en memoria, de solo
lectura**: `DIR` los enseña, `TYPE` los lee, los programas se lanzan por su nombre
(también `ordenes` desde la consola del núcleo, y el Bloc de notas y la Consola
desde el escritorio) y un programa los abre y los lee como archivos. No tiene
carpetas, y no se puede crear ni borrar nada en ella (`usuario.cas`).

**Escribir** (desde el 19/11):

- **El controlador** (`escribir_sectores`): la orden de escribir (hasta 255
  sectores de una vez), cada sector de palabra en palabra al puerto de datos,
  esperando a que el disco lo pida y, al final, a que lo haya tomado. `asentar`
  manda FLUSH CACHE (que el disco pase a la superficie lo que tenga guardado).
- **Los archivos**: un archivo se abre para leer, para leer y escribir, para
  crearlo (si ya estaba, queda vacío) o para añadir al final. Al escribir más allá
  de su final **crece**: se le dan clusters libres (se buscan en la FAT desde donde
  se dio el último) y se enlazan a su cadena. La posición se puede **mover** a
  cualquier sitio, también más allá del final: al escribir ahí, lo de en medio
  queda **a cero**. **Cortar** deja el archivo con el tamaño de su posición y
  suelta los clusters que sobran. **Borrar** suelta toda su cadena y tacha su
  entrada (y los trozos de su nombre largo).
- **Las carpetas**: una entrada nueva va al primer hueco de su carpeta (una
  tachada, o el final); si no queda sitio, la carpeta **crece** un cluster (a cero).
  La raíz de FAT12/16, que es fija, no crece: se llena. Una **carpeta nueva** lleva
  su cluster a cero con sus entradas `.` y `..`. Solo se borra una carpeta
  **vacía**, que no esté abierta ni sea la de trabajo de nadie. **Cambiar el
  nombre** es cambiarlo en su entrada; un archivo, además, puede pasar a otra
  carpeta (una carpeta no: habría que cambiar su `..`).
- **Las dos copias de la FAT** se escriben siempre (todas las que diga el
  volumen; una FAT32 que solo lleva una al día, esa). En **FAT32**, el **sector de
  información** lleva la cuenta de clusters libres y por dónde buscar el
  siguiente: se pone al día al cerrar un archivo, al borrar y al tocar carpetas.
  (En FAT12, las entradas de 12 bits a caballo entre dos sectores se escriben
  igual que se leen.)
- **La fecha y la hora** de lo que se crea o se escribe son las del reloj del PC
  (`fecha.cas`); al escribir se marca además el atributo «por archivar».
- **Nombres**: al crear, **8+3** (en mayúsculas, sin las letras que la FAT no
  admite; la ñ y la ç valen; `CON`, `PRN`, `AUX`, `NUL`, `COM1`... no, que son
  de los dispositivos del DOS). No se crean nombres largos.
- **Cuándo llega al disco**: los datos, en el momento (cada sector se escribe
  según se llena; no hay nada esperando en la memoria más que el sector de la FAT
  que se está tocando, que se escribe al acabar cada llamada). La **entrada del
  archivo** (su tamaño y su primer cluster) se pone al día **al cerrarlo** (o al
  cortarlo), como en el DOS: si se apaga con un archivo a medio escribir, quedan
  clusters sueltos que `fsck` recoge, pero nada cruzado.

**Los nombres largos (VFAT)**, al leer: los trozos de nombre largo que van delante
de una entrada se juntan (comprobando su número y la suma del nombre corto) y se
pasan de Unicode a la página 437 (lo que no está en ella sale como `_`). `DIR` los
enseña a la derecha, y un archivo o una carpeta se puede **abrir por su nombre
largo** (sin distinguir mayúsculas) igual que por el corto. Al borrar o cambiar de
nombre, los trozos se tachan con la entrada (que no queden sueltos).

## 8. El intérprete de órdenes

`uordenes.cas` (`ORDENES.BIN` en el disco): **un programa del anillo 3**, escrito
en Castel con `modelo usuario`, que no toca nada del núcleo: todo lo hace por las
llamadas al sistema. Lee una línea del teclado y hace lo que dice; las órdenes
valen en mayúsculas o en minúsculas. La petición de órdenes enseña la carpeta de
trabajo (`C:\TEXTOS>`).

| Orden | Qué hace |
|---|---|
| `AYUDA` (o `?`) | las órdenes |
| `VER` | la versión del sistema |
| `CLS` | borra la pantalla |
| `MEM` | la memoria que hay, la libre, lo que ocupa el intérprete y las tareas |
| `FECHA` | la fecha y la hora del reloj del PC |
| `TECLADO [ES\|US]` | el teclado: el español o el de EE. UU. (sin nada, dice cuál hay) |
| `DIR [carpeta]` | los archivos de una carpeta (la de trabajo si no se dice): nombre, tamaño, fecha, hora y, si lo tiene, su nombre largo; cuántos son, lo que ocupan y lo que queda libre. Con comodines, los que casen: `DIR *.TXT`, `DIR TEXTOS\P*` |
| `TYPE archivo` | enseña un archivo de texto |
| `COPY origen destino` | copia un archivo en otro, o a una carpeta; con comodines, varios a una carpeta (`COPY \TEXTOS\*.TXT .`). Sin destino, a la carpeta de trabajo |
| `DEL archivo` | borra un archivo, o los que casen (`DEL *.BAK`) |
| `REN viejo nuevo` | cambia el nombre de un archivo o de una carpeta; si el nuevo lleva carpeta (`REN A.TXT OTRA\A.TXT`), el archivo se mueve a ella |
| `MD carpeta`, `RD carpeta` | hace una carpeta; borra una carpeta vacía |
| `CD [carpeta]` | cambia de carpeta de trabajo (`CD ..` sube una, `CD \` va a la raíz; también `CD..` y `CD\`); sin nada, dice cuál es |
| `PATH [carpetas]` | las carpetas donde se buscan los programas, separadas con `;` (de fábrica, `C:\`); `PATH ;` lo deja vacío; sin nada, lo enseña |
| `ECHO texto` | escribe el texto (`ECHO.`, una línea en blanco; `ECHO OFF` y `ECHO ON`, en los archivos de órdenes) |
| `REM ...` | un comentario |
| `PAUSE` | «Pulse una tecla para seguir . . .» y la espera |
| `ESCRITORIO` | pone en marcha el escritorio (sección 10); este intérprete, el de la pantalla de texto, acaba |
| `SALIR` | acaba el intérprete (se vuelve a la consola del núcleo; en una ventana de la Consola, la cierra) |
| cualquier otro nombre | **un programa** (`HOLA` o `HOLA.BIN`) o **un archivo de órdenes** (`LOTE` o `LOTE.BAT`): se busca en la carpeta de trabajo y luego en las del PATH (con una carpeta delante, `TEXTOS\ECO2`, solo ahí); lo que siga son sus argumentos. Un programa se lanza y se le espera; si acaba con un código distinto de 0, se dice. Si no existe: «Orden o nombre de programa desconocido». |

(`DEL`, `REN`, `MD`, `RD`, `CD`, `COPY`, `PAUSE` y `SALIR` también valen como
`ERASE`/`BORRAR`, `RENAME`, `MKDIR`, `RMDIR`, `CHDIR`, `COPIAR`, `PAUSA` y `EXIT`.)

- **La redirección**: detrás de cualquier orden, `> archivo` manda al archivo lo
  que escriba (creándolo, o vaciándolo si estaba), y `>> archivo` lo añade al
  final: `DIR > LISTA.TXT`, `ECHO otra línea >> NOTAS.TXT`, `HOLA > SALIDA.TXT`
  (también lo que escribe un programa, y los que este lance). El archivo queda
  con los finales de línea del DOS. No hay `<` ni tuberías.
- **Los archivos de órdenes (.BAT)**: una orden por línea, las mismas que se
  teclean. Cada línea se enseña antes de hacerla, salvo con `ECHO OFF` o si
  empieza por `@`. `%1` a `%9` son los argumentos (`%0`, el nombre; `%%`, un `%`).
  Un archivo de órdenes que llama a otro pasa a él (no vuelve). No hay `IF`,
  `GOTO`, `FOR` ni variables.
- **La línea se corrige** con Retroceso y Supr, moviéndose con las flechas, Inicio
  y Fin; Esc la borra. **La historia**: las flechas de arriba y abajo traen las
  ocho últimas líneas. (Está en `leer_línea`, de `llamadas.cas`: la tiene cualquier
  programa que lea líneas.)
- **Ctrl+C** acaba el programa que corre (sale `^C` y vuelve la petición de
  órdenes) y, si lo había lanzado un archivo de órdenes, también el archivo. Sin
  programa en marcha, deja la línea que se estaba escribiendo. En `PAUSE`, acaba
  el archivo de órdenes.

Si el programa lanzado hace lo que no debe, el núcleo lo acaba, lo dice, y el
intérprete sigue con su petición de órdenes.

## 9. La consola del núcleo

Es la tarea 1, del anillo 0: lo que queda de la semilla, para mirar el núcleo por
dentro. (El intérprete de órdenes de verdad es el de la sección 8.)

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
| `ejecutar N` | ejecuta el módulo N en el anillo 3 y espera a que acabe; dice su código y los marcos libres antes y después |
| `disco` | los discos del canal primario (modelo, sectores) y el sistema de archivos (qué FAT, dónde empieza, clusters, los libres; sectores leídos y escritos, y archivos abiertos) |
| `dir [CARPETA]` | las entradas de una carpeta del disco |
| `lanzar PROGRAMA.BIN [ARGUMENTOS]` | carga el programa del disco, lo ejecuta en el anillo 3 con sus argumentos y espera a que acabe |
| `ordenes` | lanza el intérprete de órdenes (`ORDENES.BIN`) |
| `teclas` | las teclas pulsadas |
| `teclado es`, `teclado us` | el teclado: el español (el de fábrica) o el de EE. UU. |
| `escritorio` | pone en marcha el escritorio (sección 10) |
| `fallo` | lee donde no hay nada: el fallo de página del núcleo, que lo para (código 14) |
| `cero` | divide por cero (código 9) |
| `apagar` | acaba (código 5) |

(Las órdenes de esta consola se escriben sin acentos, como cuando el teclado era
el de EE. UU.)

### El teclado

Es **el español** (`consola.cas`), con las letras de la **página 437**, la del PC
y la que usa Hispa-DOS (y Castel para sus textos):

- `ñ Ñ ç Ç ¿ ¡ º ª ·` y los signos donde están en un teclado español (`-` y `_`
  junto a Mayús, `'` y `?` junto al 0, `<` y `>` junto a la Z...).
- **Los acentos, con tecla muerta**: `´` y luego la vocal da `á é í ó ú`; igual
  `` ` `` (`à è ì ò ù`), `^` (`â ê î ô û`) y `¨` (`ä ë ï ö ü`). El acento y un
  espacio dan el acento solo; con otra letra, el acento y la letra. La 437 no
  tiene `Á Í Ó Ú` (sí `É`, `Ü`, `Ä`, `Ö`): salen sin acento, como en Hispa-DOS.
- **Alt Gr**: `\ | @ # ~ ¬ [ ] { }`. **Bloq Mayús**, para las letras (también ñ y ç).
- Las flechas, Inicio, Fin, Supr, RePág, AvPág, Insert y F1 a F10 llegan a los
  programas como `0x100` más su código (`TECLA_ARRIBA`...).
- **Ctrl** con una letra da su número de control; **Ctrl+C** acaba los programas
  en marcha (sección 5).
- El de EE. UU. sigue estando: `teclado us` en esta consola, `TECLADO US` en el
  intérprete, o `teclado=us` en lo que se le dice al núcleo al arrancar.

---

## 10. El entorno gráfico: el escritorio

El primer entorno gráfico de Hispa-Pentium (la 0.4): un escritorio con la
comodidad de los de principios de los 2000, **de diseño propio**. No lleva nada de
nadie: ni logotipos, ni fondos, ni iconos, ni los colores de otro sistema. La
paleta es la de la casa para esta gama (**azules y verdes**, con las esquinas
redondas), el logo es el **disquete de Hispa-DOS** y la letra es la de 8x16 de
Hispa Ventanas (diseño propio, con sus tildes y su eñe).

| | |
|---|---|
| ![El menú Inicio](pentium/escritorio-inicio.png) | ![El Bloc de notas y la Consola](pentium/escritorio-ventanas.png) |
| ![Configuración](pentium/escritorio-configuracion.png) | ![Apagar el equipo](pentium/escritorio-apagar.png) |

### Lo que se ve

- **El tapiz**: tres a elegir («Olas», «Azul» y «Verde»), con el disquete y el
  nombre abajo a la derecha.
- **La barra de tareas**, abajo: el botón **«Inicio»** (verde, redondeado), un
  **botón por cada ventana abierta** (pulsarlo la trae delante; si ya lo estaba, la
  minimiza; si estaba minimizada, la recupera) y, a la derecha, la **bandeja con el
  reloj** (el de la CMOS; se pone al día cada minuto).
- **El menú Inicio**, de dos columnas: a la izquierda, en blanco, los **Programas**
  (Bloc de notas, Consola); a la derecha, en azul muy claro, los **Documentos** (los
  `.TXT` de la raíz del disco: se abren con el Bloc de notas) y **Configuración**;
  y abajo, **«Apagar el equipo»**. Se abre con el botón o con la **tecla del logo**;
  la opción bajo el ratón se resalta; con las flechas, Intro y Esc también se anda
  por él; un clic fuera lo cierra.
- **Las ventanas**: la barra de título azul con degradado y las esquinas de arriba
  redondeadas, el disquete, el nombre y **tres botones** (minimizar, maximizar o
  volver al tamaño de antes, y cerrar, este en el naranja del disquete), un borde y
  una sombra suave. Se **mueven** arrastrando el título, pasan **delante** al
  pulsarlas, y **dos clics en el título** las maximizan. La ventana activa lleva el
  título de azul vivo; las demás, de azul gris.
- **«Apagar el equipo»**: Apagar, Reiniciar o Cancelar. Apagar deja la pantalla de
  «Ya puede apagar el equipo» (en QEMU, además, lo apaga).
- **Configuración**: lo que hay (la pantalla, la memoria, las tareas), el teclado
  (español o de EE. UU.) y el tapiz.
- **El Bloc de notas** (`BLOC.BIN`, `ubloc.cas`): escribir un texto con el teclado
  español y **guardarlo en el disco** («Guardar», F2 o Ctrl+S; en la página 437 y
  con los finales de línea del DOS). Las flechas, Inicio, Fin, Supr, Retroceso; un
  clic pone el cursor; las líneas largas siguen debajo. Abre `NOTAS.TXT`, o el
  archivo que se le diga (los documentos del menú). Si se cierra con cambios sin
  guardar, avisa.
- **La Consola** (`CONSOLA.BIN`, `uconsola.cas`): una ventana de 80 x 25 letras con
  el **intérprete de órdenes dentro** (el mismo `ORDENES.BIN`, sin tocarle una
  línea), y lo que este lance. Ctrl+C acaba el programa que corra; `SALIR` cierra
  la ventana.

### Cómo está hecho

**El modo gráfico** (`video.cas`). Un modo con la memoria de la pantalla seguida
(«framebuffer lineal»). Sale de uno de dos sitios:

1. **del cargador**, si ya lo ha dejado puesto y lo dice en la información de
   multiboot (GRUB, cuando el núcleo se lo pide en su cabecera: sección 11);
2. de la **VBE de Bochs** (la VGA de QEMU, Bochs y VirtualBox): QEMU con `-kernel`
   no da nunca un modo gráfico, así que el núcleo lo pide él por los dos puertos de
   esa tarjeta (`0x1CE`, el índice; `0x1CF`, el dato), y busca la memoria de la
   pantalla en la primera zona de su dispositivo PCI (`1234:1111`).

800 x 600 si no se dice otra cosa (`modo=1024x768` en la línea del cargador). La
memoria de la pantalla se pone en la zona de los gráficos del núcleo (sección 3).
**Todo se dibuja con 32 bits por punto**; si el modo que hay es de 24 o de 16 (hay
tarjetas que no dan otro), se dibuja en un lienzo de 32 bits aparte y `volcar()`
pasa a la pantalla de verdad lo que se ha tocado. (`bits=24` o `bits=16` en la
línea del cargador lo fuerzan con la VBE de Bochs, para probarlo.)

**Dibujar** (`grafico.cas`), en Castel. Un **lienzo** es un rectángulo de puntos
(`0x00RRGGBB`) en la memoria, con su ancho, su alto y los puntos que hay de una
fila a la siguiente (así, un trozo de un lienzo es otro lienzo). Rectángulos,
degradados (de arriba abajo y de lado a lado), **esquinas redondas**, bordes,
líneas, círculos, oscurecer (las sombras), el disquete (hecho con rectángulos, a
cualquier tamaño), **texto** con la letra de 8x16 (`letra.cas`: 256 letras de la
página 437, con las de más de Ventanas) y **copiar** de un lienzo a otro. Todo se
recorta a los bordes del lienzo. Las filas se llenan y se copian con `rep stosd` y
`rep movsd` (unas líneas de ensamblador); lo demás es Castel.

**El ratón** (`raton.cas`): el PS/2, por la segunda boca del 8042 y la IRQ 12. La
interrupción lleva la cuenta de dónde está la flecha y apunta cada cambio de botón
en una cola, con el sitio donde fue (un clic no se pierde aunque se tarde en
atenderlo).

**El gestor de ventanas** (`ventanas.cas`), en el núcleo. Cada ventana tiene **su
propio lienzo**, con el marco pintado y lo de dentro, donde dibuja su dueño: así,
componer la pantalla es solo copiar, y nadie tiene que volver a pintar cuando otra
ventana le pasa por encima. Se compone en un **doble búfer** (el tapiz y, encima,
las ventanas de atrás adelante, con su sombra y sus esquinas) y de ahí a la
pantalla; y **solo lo que ha cambiado**: una lista corta de rectángulos «sucios»
(los que se tocan se juntan). La flecha se pinta al final, en la pantalla. La barra
de tareas, el menú y los cuadros son ventanas como las demás (la barra y el menú,
sin marco y siempre delante).

Una tarea del núcleo, **`escritorio`**, da el **pulso** cien veces por segundo:
recoge los clics y lo que se ha movido el ratón, y las teclas, y los reparte.
Mover, traer delante, minimizar, maximizar y cerrar lo hace el gestor; lo demás va
a la **cola de sucesos** de la ventana que toca (una tecla a la activa; un clic,
donde cae), y su dueño los recoge. Al maximizar, la ventana cambia de lienzo y su
dueño recibe «tamaño» para volver a pintar. Con el escritorio en marcha **las
teclas son suyas**: quien lea de la consola de texto se queda esperando, y Ctrl+C
es una tecla más (cada Consola tiene el suyo). Todo lo de las ventanas va con un
cerrojo: los programas dibujan a la vez que el escritorio compone.

**Los programas** abren su ventana y dibujan por las **llamadas al sistema** 32 a
36 (sección 6): `ventana`, `rectángulo`, `degradado`, `texto`, `mapa`, `suceso`...
de `llamadas.cas`. Al acabar un programa (o si el núcleo lo acaba), sus ventanas se
cierran solas.

```
importar Llamadas
...
v := ventana("Hola", 300, 120)
degradado(v, 0, 0, 300, 120, 0xEAF3FE, 0xC4DCF8)
texto_negrita(v, 20, 20, "¡Hola, escritorio!", 0x14294A)
mientras suceso(v, s, cierto) y s.clase <> EV_CERRAR hacer fin
```

**Los terminales** (`terminal.cas`). Para que la Consola tenga dentro al intérprete
de órdenes sin tocarlo: un terminal son **dos tubos**. El programa que lo hace (la
Consola) lee por uno lo que escriben los de dentro y lo pinta, letra a letra, como
una pantalla de texto; y mete por el otro las teclas que llegan a su ventana. Los
programas que lanza **heredan el terminal**: su `escribir` va al tubo en vez de a
la pantalla y su `tecla` sale de él, y no se enteran. Ctrl+C en un terminal acaba
los programas de dentro (menos los que lo atienden ellos); al cerrar la ventana, se
acaban todos.

**Lo que dice el núcleo** con el escritorio en marcha (lo que lanza, los avisos, una
excepción) sale por el **puerto serie**, como siempre.

### Lo que falta

Cambiar el tamaño de una ventana arrastrando su borde; los iconos en el tapiz; más
programas (un explorador de archivos, lo primero); menús dentro de las ventanas;
copiar y pegar; letras de otros tamaños y de ancho variable; la rueda del ratón y
el doble clic con su ajuste; y que el tapiz pueda ser una imagen del disco.

## 11. En un PC de verdad: GRUB y la memoria USB

`nucleo32/usb.sh IMAGEN` hace la imagen de una **memoria USB** (64 MB, FAT16,
arranque BIOS con GRUB) para probar Hispa-Pentium en un PC: se graba tal cual en la
memoria y se arranca de ella. (Necesita ser root, `parted`, `dosfstools` y
`grub-pc-bin`.) El menú de GRUB:

| Entrada | Qué arranca |
|---|---|
| el escritorio (800 x 600) | `ESCRIT8.BIN escritorio sindisco` |
| el escritorio (1024 x 768) | `ESCRIT10.BIN escritorio sindisco` |
| el intérprete de órdenes (modo texto) | `SEMILLA.BIN ordenes sindisco` |
| la consola del núcleo (modo texto) | `SEMILLA.BIN sindisco` |

- **Los programas vienen como módulos.** El núcleo no sabe leer todavía una
  memoria USB (ni un disco SATA), y en casi todos los portátiles no hay IDE
  clásico: GRUB carga con el núcleo `ORDENES.BIN`, `BLOC.BIN`, `CONSOLA.BIN`,
  `HOLA.BIN`, `ECO.BIN`, `PRUEBA.BIN`, `BUCLE.BIN` y `LEEME.TXT`, y hacen de
  **unidad C: en memoria** (sección 7). No se puede guardar nada: el Bloc de notas
  lo dice («es de solo lectura»).
- **`sindisco`**: el núcleo ni mira los discos del PC. Y aunque los mirara, **no
  escribe en ellos** sin `escritura` (sección 7).
- **El modo gráfico, de GRUB.** En un PC de verdad no está la tarjeta de QEMU: el
  modo lo tiene que poner el cargador antes de entrar en el núcleo (con la VBE de
  la BIOS, que desde el modo protegido ya no se puede llamar). Multiboot lo prevé:
  el núcleo lo pide en su **cabecera** (la bandera de vídeo y el ancho, el alto y
  los bits que prefiere). El compilador de Castel deja en la cabecera el sitio
  para ello, a cero (CASTEL32.md §8), y **`nucleo32/modo_grafico.py NUCLEO.BIN
  SALIDA.BIN [ANCHO ALTO [BITS]]`** hace una copia del núcleo que lo pide:
  `ESCRIT8.BIN` y `ESCRIT10.BIN` son eso. GRUB pone el modo (o el que más se le
  parezca) y le dice al núcleo dónde está la memoria de la pantalla; el escritorio
  arranca en él. El núcleo sin tocar (`SEMILLA.BIN`) pide lo de siempre, modo
  texto: con él van QEMU con `-kernel` y las dos entradas de texto.
- **Probado en QEMU** arrancando la imagen como una memoria USB (`-device qemu-xhci
  -device usb-storage`): GRUB, el modo de 800 x 600 puesto por él y el escritorio
  con sus programas. **En un PC de verdad, todavía no** (19/11/2026): lo que se
  espera es que vaya donde haya teclado y ratón PS/2 (o que la BIOS los imite: los
  portátiles con panel táctil PS/2) y una VBE con modos de 32, 24 o 16 bits.
- **Sin GRUB**, otro camino para más adelante: un cargador propio de 16 bits que
  pida el modo a la VBE (`int 10h`, `4F02h`) antes de pasar a modo protegido y
  deje al núcleo la misma información.

## 12. Las pruebas

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
dirección.

**Con disco** (imágenes hechas con `nucleo32/disco.sh`, dadas a QEMU como disco
IDE): que IDENTIFY dice el disco que hay; que la **FAT16 se monta**, se lee su raíz
y una carpeta (saltando los nombres largos); que un **programa se carga del disco**,
corre, recibe sus argumentos, acaba con su código y devuelve toda su memoria; que
lo que no es un programa o no está no se lanza. Que el **intérprete de órdenes**
(del anillo 3, cargado del disco) hace **AYUDA, VER, MEM, DIR, TYPE y CLS** (la
pantalla se mira), **lanza programas por su nombre** con sus argumentos (también de
una carpeta) y los espera, dice lo que no conoce, y sigue cuando un programa se
acaba a la fuerza. Desde dentro de un programa (`PRUEBA.BIN`): la **memoria que
crece** y se devuelve; **leer un archivo de varios clusters** a trozos de 777
octetos (la suma de sus octetos es la del archivo de verdad); la **pila que crece
sola** (200 llamadas anidadas con 4000 octetos cada una, y leer del disco a un
arreglo local sin estrenar) y la que se desboca, que acaba con el programa; el tope de archivos
abiertos; que las **direcciones que no son del programa no se tocan**; **lanzar dos
programas a la vez y esperarlos**; que **dos programas que escriben a la vez no se
mezclan** (240 líneas enteras: el cerrojo de la consola); que tocar memoria
devuelta acaba con el programa; y que, al salir, **el núcleo ha recogido** el
archivo que quedó abierto y el programa que nadie esperó (los mismos marcos libres,
ningún archivo abierto, solo las dos tareas del núcleo). Y lo mismo, en corto, con
**FAT12, FAT32 y un disco con tabla de particiones**; el arranque directo al
intérprete (`-append ordenes`); y el núcleo sin disco.

**Escribiendo en el disco** (cada prueba, en una copia de la imagen, con
`ESCRIBE=1`; luego la miran desde fuera `fsck.fat` y `mtools`), con **FAT16, FAT12,
FAT32 y la FAT16 de una partición**: desde dentro de un programa (`ESCRIBE.BIN`),
crear un archivo y escribirlo a trozos de 777 octetos, leerlo de vuelta igual,
escribir en medio, **cortarlo** (los clusters que sobran quedan libres),
**alargarlo dejando un hueco** (que se lee a cero), **añadir al final**, crear
encima y borrar; lo que no se puede (abrir dos veces para escribir, borrar o
renombrar lo abierto, nombres que no valen, carpetas que no están); **carpetas**:
hacerlas, entrar, meter 70 archivos (la carpeta crece), mover un archivo a otra,
cambiar nombres, vaciarlas y borrarlas, y que al final quedan **los mismos
clusters libres**. Con las órdenes: `ECHO ... >` y `>>`, `COPY` (uno, y varios con
comodines), `MD`, `CD`, `REN`, `DEL`, `RD`, un programa y `DIR` redirigidos a un
archivo, y lo que tienen que decir cuando no puede ser. En FAT12, además, **llenar
el disco** hasta el último cluster y vaciarlo. Después, que **`fsck.fat` da el
disco por sano** (las dos FAT iguales, el sector de información de FAT32 con su
cuenta, ningún cluster suelto), que **`mtools` ve lo mismo** (lo creado con sus
finales de línea, lo copiado igual que el original octeto a octeto, lo renombrado
y lo borrado), y que **al volver a arrancar** el núcleo lee lo que escribió.

**El intérprete**: `CD` y la carpeta de trabajo (en la petición, en las rutas, en
lo que se crea, heredada por los programas), carpetas dentro de carpetas, `PATH`
(con él, sin él y con varias carpetas), un **archivo .BAT** (sus líneas, `ECHO`,
`REM`, `PAUSE`, `%1`, programas y redirección dentro; `ECHO OFF` y `@`), la
**historia** y la corrección de la línea (lo que queda en la pantalla, deshaciendo
los retrocesos: `nucleo32/pantalla.py`), `FECHA` (la del reloj del PC). **Ctrl+C**
con un programa que da vueltas sin llamar al sistema, uno que duerme, uno que
espera teclas, uno lanzado desde un .BAT (que también se acaba), uno que escribía
en un archivo (queda cerrado, con su tamaño, y el disco sano), en `PAUSE`, sin
programa en marcha, y desde la consola del núcleo; y que después no quedan
archivos abiertos, ni tareas, ni memoria sin devolver.

**El teclado español**: una línea con todas sus letras y signos (`ñ ç ¿ ¡ º ª`,
los cuatro acentos con su tecla muerta, Alt Gr, Bloq Mayús) tecleada tecla a
tecla y mandada a un archivo, que se compara **octeto a octeto** con lo que tiene
que ser en la página 437; el cambio al de EE. UU. y vuelta. **Los nombres
largos**: `DIR` los enseña (también con eñe), se abre por ellos, y al renombrar o
borrar no quedan trozos sueltos (lo mira `fsck.fat`).

Además, como con todo lo de Castel: **CASTEL** (el compilador escrito en Castel,
dentro de Hispa-8086) saca el **mismo ensamblador** que `ccastel.py` para el núcleo
(con sus módulos nuevos), el intérprete de órdenes y los demás programas de
usuario, y **ENSAMBLA** da el **mismo binario que NASM**. (El lenguaje no se ha
tocado; de los compiladores, solo el arranque de `modelo usuario`, en los dos: ya
no pone la pila, que es del núcleo. En la tanda del 19/11 de madrugada, ni eso. En
la de la 0.4, cuatro palabras a cero en la cabecera multiboot de `modelo núcleo`,
en los dos: el sitio para pedir el modo gráfico, sección 11.)

**Sin disco, y sin permiso** (la tanda de la 0.4): con los programas como módulos
de multiboot y sin disco, `ordenes` lanza el intérprete que venga, `DIR` enseña los
módulos, `TYPE` los lee, los programas se lanzan por su nombre, y crear, borrar o
hacer carpetas se niega («es de solo lectura»); con un disco y sin `escritura`,
todo lo que escribe se niega y **la imagen queda idéntica octeto a octeto**; con
`sindisco`, los discos ni se miran.

**El escritorio**, con capturas de la pantalla (el `screendump` del monitor de
QEMU) que mira **`nucleo32/mira_captura.py`**: el tamaño, el color de puntos
sueltos y **textos** buscados con la propia letra del sistema, en cualquier color
(como las pruebas de Hispa Ventanas). Una sesión entera, con el ratón y el teclado
movidos por el monitor de QEMU: el escritorio recién arrancado (el tapiz, «Inicio»,
el reloj con la hora del PC, la flecha); el **menú Inicio** con sus dos columnas;
lanzar el **Bloc de notas**, teclear un texto con eñe, acentos y «¿?», **guardarlo
con F2** y que el archivo esté luego en la imagen del disco **octeto a octeto** (y
`fsck.fat` la dé por sana); **arrastrar** la ventana, **maximizarla** con dos
clics (y que el programa vuelva a pintar lo suyo), devolverla a su tamaño,
**minimizarla** y recuperarla con su botón de la barra; la **Consola** con el
intérprete dentro (una orden, un programa que da vueltas y Ctrl+C, `VER`) y que
`SALIR` la cierra; **Configuración** y el cambio de tapiz; **cerrar** el Bloc de
notas con su botón; y **«Apagar el equipo»**, que apaga. Además: la orden
`ESCRITORIO` del intérprete; abrir un documento del menú; 1024 x 768 con 24 bits
por punto; que sin tarjeta gráfica que valga se dice y la consola sigue; que
`modo_grafico.py` deja bien la cabecera; y la **memoria USB con GRUB** arrancada en
QEMU como tal (el modo, puesto por GRUB; los programas, de los módulos). Las
capturas se quedan en una carpeta con `CAPTURAS_A=carpeta ./pruebas.sh`.

Los disquetes de las pruebas de los dos compiladores y los dos ensambladores van
ahora en **dos tandas** (lo de Hispa-Pentium, aparte): el ensamblador de la semilla
pasa de medio mega y ya no cabía con lo demás.

Con todo, `pruebas.sh` entero da **281 bien, 0 mal** (19/11/2026; unos diecinueve
minutos en el VPS).

## 13. Por dónde seguir

Hecho en la tanda del 18-19/11/2026: el disco (ATA) y la FAT, cargar programas del
disco, las llamadas al sistema nuevas, la consola con cerrojo, el intérprete de
órdenes del anillo 3, la memoria del usuario que crece (`más_memoria`) y la pila
que crece sola. Y en la del 19/11 de madrugada: **escribir en el disco** (ATA y
FAT12/16/32, con `COPY`, `DEL`, `REN`, `MD`, `RD` y la redirección), el
intérprete con **`CD`, `PATH`, archivos `.BAT`, historia y Ctrl+C**, el **teclado
español** y los **nombres largos** al leer. Y en la del 19/11 por la mañana (la
0.4): **el escritorio** (sección 10), la unidad en memoria, el permiso de escritura
y la memoria USB con GRUB (sección 11). Lo siguiente:

0. **El escritorio, más** (lo que falta, al final de la sección 10) y **los PC de
   verdad**: probar la memoria USB en los portátiles; teclado y ratón **USB** (los
   que no tienen PS/2 ni lo imita su BIOS); leer la propia memoria USB y los discos
   **SATA (AHCI)**, para no depender de los módulos; apagar de verdad (ACPI).

1. **El intérprete, más**: `IF`, `GOTO`, `FOR`, `CALL` y variables en los `.BAT`;
   la redirección de la entrada (`<`) y las tuberías (`|`); `COPY CON`, `MOVE`,
   `XCOPY`, `ATTRIB`; comillas para los nombres con espacios; `NUL`, `CON` y
   `PRN` como nombres de dispositivo.
2. **La FAT, más**: **crear nombres largos** (hoy se leen y se abre por ellos,
   pero lo nuevo es de 8+3); mover carpetas de una carpeta a otra; los atributos
   (`ATTRIB`); la marca de «volumen sin cerrar bien» y revisar el disco al
   arrancar; formatear.
3. **Páginas compartidas o copiadas al escribir**, cuando haya procesos que se
   dupliquen; y cargar los programas a la demanda (hoy se leen enteros).
4. **El disco, mejor**: una caché de sectores de verdad (hoy, uno de datos y uno de
   FAT: escribir un archivo grande va sector a sector), leer y escribir varios
   sectores de una vez, la interrupción del disco en vez de mirar el estado, el
   canal secundario, LBA de 48 bits, y discos SATA (AHCI) y USB, que es lo que
   llevan los PC de la gama XP en adelante.
5. **El teclado, más**: las luces (Bloq Mayús, Bloq Num), el teclado numérico con
   Bloq Num, la repetición a medida, y otras distribuciones cargadas de un archivo.
   Una consola con otra página de códigos (la 850, para `Á Í Ó Ú`) pide cambiar
   también la letra de la pantalla.
6. **Prioridades y un planificador mejor**, semáforos y colas de mensajes entre
   tareas (los cerrojos ya están). Programas en segundo plano de verdad (hoy
   Ctrl+C acaba todos los que no lo atienden).
7. **El lenguaje**: llamar a funciones por su dirección, sumar a un arreglo como a
   un puntero y funciones de interrupción públicas (CASTEL32.md §10), que hoy se
   rodean a mano.
8. **Más tarde**: el cargador de PE (los programas Win32 de Hispa-486, HW32) sobre
   este núcleo, que es la base de la gama XP.
