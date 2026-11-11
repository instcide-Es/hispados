# Windows 3.1x sobre Hispa-DOS: estado e investigación

Windows es propiedad de Microsoft y **no se incluye** en Hispa-DOS: cada usuario
instala su copia. Esta página recoge lo que hace falta en el DOS para que
funcione y en qué punto está.

## Probado con Windows para Trabajo en Grupo 3.11 (español, CD)

El instalador del CD español se llama `INSTALAR.EXE` (como nuestro
`INSTALAR.COM`; desde la unidad del CD se ejecuta el de Windows).

| Paso | Estado |
|---|---|
| El instalador acepta el gestor XMS | ✅ HSPXMS devuelve la revisión 3.50 (con la 1.00 decía «controlador de memoria extendida no compatible») |
| Memoria extendida solo por XMS | ✅ HSPXMS engancha INT 15h AH=88h y devuelve 0 KB, como HIMEM |
| Copia de archivos desde el CD (HSPCD + ISO 9660) a FAT16 | ✅ |
| `FILES=` suficiente | ✅ La SFT tiene el formato de DOS 4+ (cabecera + entradas de 3Bh); KRNL386 la recorre desde la lista de listas |
| DOSX (modo estándar) arranca KRNL386 | ✅ |
| KRNL386 inicia su montón global | ❌ fallo de protección en DOSX (ver abajo) |

## El fallo actual

La parte gráfica del instalador se lanza con (desde `C:\WINDOWS`):

    SYSTEM\DOSX.EXE krnl386.exe /b /q:D:\ /3 /i:1

KRNL386, ya en modo protegido, hace estas llamadas DPMI: `0000h` (un
descriptor), `0500h` (memoria libre), `0501h` (bloque de ~284 KB en memoria
extendida, base lineal 126030h) y `0008h` (límite del selector 027Fh). DOSX da
una protección general (código 0) en el `IRET` con el que vuelve de `0008h`
(CS:IP 0053:2B9A, lineal 1134BAh).

Lo comprobado:

- 027Fh es el selector de datos y pila de KRNL386 (SS = 027Fh). KRNL386 cambia
  su base en la LDT (rutina 22B1h, escribe en la LDT por un alias) a la del
  bloque nuevo y pide el límite con `0008h`; DOSX recarga SS y el `IRET` saca
  el marco de la base nueva, donde no está (en la antigua está bien:
  IP=BF3A CS=0307 FL=3246; en la nueva, IP=0 CS=0) -> CS nulo -> GP(0).
- Historia del descriptor (punto de vigilancia hardware de gdb sobre
  114F38h): DOSX lo crea para el segmento de pila del programa (base 178C0h);
  KRNL386 lo mueve a 19EA0h con la pila en 02FFh (bien) y después a memoria
  extendida **con la pila en 027Fh** (mal).
- DOSX devuelve selectores correctos en `0000h` (031Fh); no hay solapamiento
  de bloques XMS ni efecto de la línea A20 (la LDT está intacta y la memoria
  baja equivalente, vacía).
- No depende de la memoria (16 MB y 32 MB, igual) ni de los residentes de
  AUTOEXEC.BAT.
- Con FreeDOS 1.3 + HIMEMX, DOSX ni siquiera arranca («Asegúrese de que esté
  instalado HIMEM.SYS»), así que no sirve de referencia directa.

Hipótesis pendiente: alguna respuesta del DOS hace que KRNL386 vaya por un
camino de inicio distinto del de MS-DOS (por ejemplo, el tamaño del bloque de
memoria convencional que recibe al ejecutarse, el estado de la HMA o un dato
de la lista de listas). Siguiente paso: localizar en KRNL386 la decisión que
lo lleva a mover su DGROUP con la pila dentro, comparando con lo que devuelve
MS-DOS 6.22 en cada llamada de las que hace antes (la traza `make TRACE=1`
las registra por el puerto serie, con los nombres de fichero y la línea de
órdenes de cada ejecución).

## Herramientas

- `make TRACE=1`: el núcleo registra cada INT 21h por COM1.
- QEMU `-d int -D fichero`: interrupciones y excepciones en modo protegido
  (incluidas las llamadas DPMI, INT 31h).
- QEMU `-s` + `gdb` (`set architecture i386`, `hbreak *lineal`,
  `watch *(unsigned char*)lineal`) y el monitor (`xp`, `pmemsave`).
