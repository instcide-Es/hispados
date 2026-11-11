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
| KRNL386 inicia su montón global | ✅ EXEC carga los EXE con MINALLOC = MAXALLOC = 0 al final del bloque mayor, como MS-DOS (encontrado por Fable 5.2) |
| Instalación gráfica (copia, impresora, red, aplicaciones) | ✅ «Windows para Trabajo en Grupo 3.11 está instalado» |
| IOCTL 440Dh/60h en el CD | ✅ error, como con MSCDEX (con un BPB de ceros DOSX dividía por cero) |
| IOCTL 4402h/4403h (IFSHLP.SYS) e INT 2Fh 1302h | ✅ implementados |
| `WIN` (modo 386 mejorado, WIN386) | ❌ «ERROR: No se ha aceptado esta versión de MS-DOS» |
| SMARTDRV en AUTOEXEC.BAT | ❌ «No se puede cargar SMARTDrive cuando se ejecuta… Windows» |

## El fallo actual: WIN386 y DOSMGR

Windows para Trabajo en Grupo 3.11 solo funciona en modo 386 mejorado. WIN386
anuncia su arranque con INT 2Fh AX=1605h y su componente DOSMGR consulta al
núcleo con AX=1607h, BX=0015h. Con un DOS 5 o posterior espera que el propio
núcleo conteste (datos de instancia y tabla de parches: TempDS, TempBX, InDOS,
MachineID, secciones críticas, último MCB). Hispa-DOS todavía no lo hace.

## Herramientas

- `make TRACE=1`: el núcleo registra cada INT 21h (`[AXAX=resultado]`, con
  nombres de fichero, texto de la función 09h y la línea de órdenes de 4Bh) y
  cada INT 2Fh (`{AX}`) por COM1.
- QEMU `-d int -D fichero`: interrupciones y excepciones en modo protegido
  (incluidas las llamadas DPMI, INT 31h). Ocupa mucho: borrarlo al acabar.
- QEMU `-s` + `gdb` (`set architecture i386`, `hbreak *lineal`,
  `watch *(unsigned char*)lineal`) y el monitor (`xp`, `pmemsave`).
- La parte gráfica del instalador se lanza a mano (desde `C:\WINDOWS`) con
  `SYSTEM\DOSX.EXE krnl386.exe /b /q:D:\ /3 /i:1`.
