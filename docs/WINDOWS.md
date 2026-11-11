# Windows 3.1x sobre Hispa-DOS: estado e investigación

Windows es propiedad de Microsoft y **no se incluye** en Hispa-DOS: cada usuario
instala su copia. Esta página recoge lo que hace falta en el DOS para que
funcione y en qué punto está.

## Windows para Trabajo en Grupo 3.11 (español, CD): funciona

Se instala desde el CD y **arranca en modo 386 mejorado** hasta el
Administrador de programas. Probado: menús, diálogos, Solitario, una sesión
MS-DOS (con el COMMAND.COM de Hispa-DOS), salir de Windows y SMARTDRV.

El instalador del CD español se llama `INSTALAR.EXE` (como nuestro
`INSTALAR.COM`; desde la unidad del CD se ejecuta el de Windows).

### Qué ha hecho falta en Hispa-DOS

| Síntoma | Causa y arreglo |
|---|---|
| «Controlador de memoria extendida no compatible» | HSPXMS devuelve la revisión 3.50 (era 1.00) y engancha INT 15h AH=88h (0 KB), como HIMEM |
| «files= 20 o superior» | SFT con el formato de DOS 4+ (cabecera + entradas de 3Bh) |
| Fallo de DOSX al iniciar KRNL386 | EXEC carga los EXE con MINALLOC = MAXALLOC = 0 al final del bloque mayor, como MS-DOS |
| «División por cero» al instalar desde el CD | IOCTL 440Dh/60h devuelve error en las unidades ISO 9660 (como con MSCDEX) |
| IFSMGR / IFSHLP.SYS | IOCTL 4402h/4403h para drivers de carácter |
| WIN386 | INT 2Fh 1302h (cambiar el manejador de INT 13h) |
| «No se ha aceptado esta versión de MS-DOS» | INT 2Fh 1605h/1606h (inicio y fin de Windows, datos de instancia) y 1607h BX=15h (DOSMGR): ver abajo |
| Cuelgue en el logotipo y reinicio al abrir una sesión MS-DOS | La SFT va en dos bloques (5 + 35 entradas) con el segundo fuera del segmento del núcleo; tabla SFT de FCB en la lista de listas |
| SMARTDRV: «no se puede cargar cuando se ejecuta Windows» | INT 21h 2Bh/2Dh validan la fecha y la hora (AL=FFh si no valen): SMARTDRV detecta DESQview con una fecha imposible |
| VSHARE.386 escribe en los datos internos de MS-DOS | El núcleo deja 0FFh en el octeto 4 de su segmento: VSHARE se niega a cargarse (ver «Limitaciones») |

### INT 2Fh 1607h BX=0015h (DOSMGR)

Con un DOS 5 o posterior, DOSMGR de WIN386 consulta al núcleo con CX:

- **CX=0:** el núcleo contesta CX=1, DX = segmento de datos y ES:BX → tabla
  de 7 palabras: versión y desplazamientos de TempDS, TempBX, InDOS,
  MachineID, lista de parches de sección crítica y último MCB.
- **CX=1:** parches pedidos en DX. Se contesta AX=B97Ch, DX=A2ABh y BX = los
  parches que el DOS da por hechos. Si no se dan por hechos los bits 1 y 2,
  DOSMGR busca secuencias de código de MS-DOS y aborta.
- **CX=3, DX=1:** CX = tamaño de una entrada de la CDS.
- **CX=5, ES:DI = cabecera de dispositivo:** BX:CX = tamaño del driver.
- **CX=2 y CX=4:** no se contestan (tampoco lo hace MS-DOS 6.22). DOSMGR
  instancia entonces él mismo la CDS, la SDA (INT 21h 5D06h), el enlace de la
  SFT y la lista de dispositivos.

El formato se obtuvo observando las respuestas de un MS-DOS 6.22 original
(sala limpia: solo comportamiento de la interfaz, nada de su código) y el
uso que hace DOSMGR de ellas.

## Limitaciones conocidas

- **VSHARE.386** (compartición de ficheros de Trabajo en Grupo) no funciona:
  escribe en posiciones fijas de los datos de MS-DOS. Con `device=vshare.386`
  activo, Windows avisa en cada arranque («No se puede cargar un
  dispositivo...»). Solución: comentar esa línea en `SYSTEM.INI`.
- **Secciones críticas:** Hispa-DOS aún no emite INT 2Ah 80h/81h. Con una
  sola sesión MS-DOS funciona; con varias sesiones haciendo E/S de disco a la
  vez, el núcleo podría corromper sus variables globales. Pendiente.
- Sin probar todavía: aplicaciones grandes (Word, Excel), red de Trabajo en
  Grupo, acceso a archivos de 32 bits, impresión, varias sesiones MS-DOS.

## Herramientas

- `make TRACE=1`: el núcleo registra cada INT 21h (`[AXAX=resultado]`, con
  nombres de fichero, texto de la función 09h y la línea de órdenes de 4Bh) y
  cada INT 2Fh (`{AX}`, `{AX:BX}` en las 16xxh) por COM1.
- QEMU `-d int -D fichero`: interrupciones y excepciones en modo protegido.
  Ocupa mucho: borrarlo al acabar.
- QEMU `-s` + `gdb` (`set architecture i386`, `hbreak *lineal`,
  `watch *(unsigned char*)lineal`) y el monitor (`xp`, `pmemsave`).
- La parte gráfica del instalador se lanza a mano (desde `C:\WINDOWS`) con
  `SYSTEM\DOSX.EXE krnl386.exe /b /q:D:\ /3 /i:1`.
