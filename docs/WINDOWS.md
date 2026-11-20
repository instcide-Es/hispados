# Windows 3.1x sobre Hispa-DOS: estado e investigación

Windows es propiedad de Microsoft y **no se incluye** en Hispa-DOS: cada usuario
instala su copia. Esta página recoge lo que hace falta en el DOS para que
funcione y en qué punto está.

## Guía corta: instalar Windows 3.11 en Hispa-DOS

1. Hispa-386 instalado en el disco duro (`INSTALAR`), al día (`ACTUALIZAR`).
   Arranque con la opción **Normal** del menú (con «Juegos», HSPEMM tiene la
   memoria superior ocupada y Windows en modo 386 mejorado no puede
   arrancar).
2. Ejecute el programa de instalación de su copia de Windows (en el CD
   español de Trabajo en Grupo 3.11 se llama `INSTALAR.EXE`; desde la unidad
   del CD se ejecuta el de Windows y no el de Hispa-DOS) y elija la
   **instalación rápida**, con las opciones que propone.
3. Al acabar, pulse «Reiniciar el equipo». Al arrancar, Hispa-DOS ve que hay
   Windows y aplica él solo lo que Windows quería cambiar (ver
   [WINCONF](#winconf-los-cambios-de-config-sys-y-autoexec-bat)): sale
   «WINCONF - cambios de Windows 3.1x...» y el equipo se reinicia una vez
   más.
4. En el menú de arranque hay una opción nueva, **4. Windows 3.1x (sin los
   emuladores de Hispa-DOS)**, que es la que sale sola. Escriba `WIN`.

Windows arranca en modo 386 mejorado, sin avisos, con el ratón PS/2. Al
salir de Windows, el ratón vuelve a Hispa-DOS (Hispa Ventanas, EDITAR...).

En un Hispa-DOS instalado antes de noviembre de 2026 (sin la línea de WINCONF
en `AUTOEXEC.BAT`), tras instalar Windows escriba una vez `WINCONF`: hace lo
mismo y reinicia.

### Qué carga Hispa-DOS con la opción «Windows»

Con Windows, el hardware es de los controladores de Windows: Hispa-DOS no le
quita memoria, interrupciones ni puertos con sus emuladores.

| | Opción «Windows» | Notas |
|---|---|---|
| HSPXMS (memoria extendida), núcleo en la HMA | sí | Windows lo necesita |
| HSPCD (lectores de CD), HSPUSB | sí (las líneas de «Normal», `INCLUDE=NORMAL`) | HSPUSB sin opciones no toca el USB si la BIOS lo lleva |
| TECLADO, HSPRATON | sí | el ratón se aparta solo mientras Windows está en marcha |
| SMARTDRV, IFSHLP.SYS, `SET TEMP` | sí | los pone Windows (sus líneas de `CONFIG.WIN`/`AUTOEXEC.WIN`) |
| HSPSND (sonido; con `/SB`, la Sound Blaster emulada) | **no** | el sonido es del controlador de Windows de su tarjeta |
| HSPJOY (mandos), HSPEMM (EMS), HSPDPMI | **no** | |

Para dejar libre la tarjeta de sonido no hay que hacer nada más: con la
opción «Windows» HSPSND no se carga. (Con la opción «Juegos», `HSPSND /SB`
no emula nada si el equipo tiene una Sound Blaster de verdad: usa la real.)
Los controladores de Windows para su tarjeta se instalan como siempre, desde
el Panel de control de Windows («Controladores»).

### Si algo falla

- `C:\>WIN` dice «Comando o nombre de archivo incorrecto»: no se han
  aplicado los cambios de Windows. Escriba `WINCONF` (o `C:\WINDOWS\WIN`).
- Para volver a como estaba: `C:\CONFIG.SIN` y `C:\AUTOEXEC.SIN` son los
  archivos de antes de WINCONF; los de Windows quedan como
  `C:\WINDOWS\CONFIG.APL` y `AUTOEXEC.APL`.
- Windows se queda en el logotipo con `DEVICE=C:\WINDOWS\IFSHLP.SYS`: el
  núcleo de Hispa-DOS es anterior al 20/11/2026; `ACTUALIZAR`.

## WINCONF: los cambios de CONFIG.SYS y AUTOEXEC.BAT

El programa de instalación de Windows para Trabajo en Grupo 3.11 **no toca un
CONFIG.SYS con menú** (varias configuraciones, el `[menu]` de MS-DOS 6, como
el de Hispa-DOS): lo dice su `INSTALAR.TXT` («para preservar la integridad de
la configuración del usuario»). Deja lo que quería cambiar en
`C:\WINDOWS\CONFIG.WIN` y `C:\WINDOWS\AUTOEXEC.WIN`, que son los archivos
enteros con sus cambios. Con el disco de Carlos (instalación rápida):

- `AUTOEXEC.WIN`: `C:\WINDOWS\SMARTDRV.EXE` al principio, `PATH C:\HISPADOS`
  cambiada por `PATH C:\WINDOWS;C:\HISPADOS` y `SET TEMP=C:\WINDOWS\TEMP`;
- `CONFIG.WIN`: al final, `DEVICE=C:\WINDOWS\SMARTDRV.EXE /DOUBLE_BUFFER`,
  `DEVICE=C:\WINDOWS\IFSHLP.SYS` y `STACKS=9,256`.

Por eso `WIN` no se encontraba (el `PATH` seguía sin `C:\WINDOWS`). No era
cosa del intérprete: `PATH %PATH%;C:\WINDOWS`, `SET PATH=C:\WINDOWS;%PATH%`,
`PATH=...` y las variables como `%ARRANQUE%` dentro de `PATH` funcionan, y la
búsqueda recorre todas las carpetas. Con MS-DOS 6.22 y un CONFIG.SYS con menú
pasa lo mismo; Microsoft pedía mezclarlos a mano.

`WINCONF.COM` (en `\HISPADOS`; `utils/winconf.asm`) lo hace solo. El
`AUTOEXEC.BAT` de Hispa-DOS lo llama al arrancar si hay `C:\WINDOWS\WIN.COM`
(`WINCONF /AUTO`: en silencio si no hay nada que hacer):

- **CONFIG.SYS:** una opción `MENUITEM=WINDOWS,...` detrás de las otras,
  `MENUDEFAULT=WINDOWS` (con los mismos segundos) y, al final, el bloque
  `[windows]` con `INCLUDE=NORMAL` y las líneas nuevas de Windows. Sin menú,
  las líneas van al final, como lo haría Windows.
- **AUTOEXEC.BAT:** la línea `PATH` de Windows en lugar de la nuestra (para
  todas las opciones: `WIN` se encuentra siempre), `IF "%CONFIG%"=="WINDOWS"
  GOTO WINDOWS` tras las otras y, antes de `:COMUN`, el bloque `:WINDOWS` con
  la línea de TECLADO, la de HSPRATON y las demás nuevas de Windows
  (SMARTDRV, `SET TEMP`).
- **SYSTEM.INI:** comenta `device=vshare.386` (ver abajo).
- Guarda los anteriores como `CONFIG.SIN` y `AUTOEXEC.SIN`, cambia los `.WIN`
  a `.APL` y reinicia (el `AUTOEXEC.BAT` en marcha ha cambiado y el
  CONFIG.SYS nuevo vale desde el arranque).
- Si ya no hay `.WIN` pero sí `.APL` y faltan sus líneas (ACTUALIZAR pone un
  CONFIG.SYS y un AUTOEXEC.BAT nuevos), las vuelve a poner.

Solo cuentan las líneas de Windows (las que llevan su carpeta, `:\WINDOWS`,
y `STACKS=`) que no están en el archivo, sin mirar mayúsculas ni espacios, y
que el usuario no ha desactivado con `REM`. Así no vuelve lo que el usuario
haya cambiado después, ni lo de una versión anterior de Hispa-DOS. Con otra
carpeta: `WINCONF C:\WIN311`.

## El ratón: HSPRATON y el MOUSE.DRV de Windows

`MOUSE.DRV` («Microsoft o IBM PS/2», el que elige el programa de instalación)
usa el ratón **de la BIOS**: INT 15h C205h (inicializar), C203h, C207h (su
rutina), C200h (activar), y la IRQ 12 que atiende la BIOS (INT 74h), que le
pasa cada paquete. HSPRATON sin opciones (y con `/DIRECTO`) va por la «vía
directa» (rueda): la IRQ 12 es suya y la BIOS no recibe nada, así que
Windows arrancaba **sin ratón**.

Como hacía el `MOUSE.COM` de Microsoft, HSPRATON escucha INT 2Fh:

- **AX=1605h** (Windows arranca; en modo 386 mejorado y en modo estándar,
  `DX` bit 0 = 1, igual). Primero pasa la llamada a los demás y, si nadie ha
  dicho que no (`CX` = 0), devuelve el ratón a la BIOS: INT 74h otra vez la
  de la BIOS (si sigue siendo la nuestra; si no, nuestra IRQ 12 la pasa a la
  BIOS mientras tanto) y el dispositivo reiniciado por la BIOS (C205h:
  paquetes de 3 bytes, fuera del modo IntelliMouse y del absoluto del
  touchpad). Por la vía BIOS, el ratón se para (C200h) hasta que Windows
  pone su rutina.
- **AX=1606h** (Windows termina): lo recupera con las funciones de la BIOS
  (lo de la carga, `dir_init`, no se queda residente: serían 1,5 KB más, y
  con eso los bloques de DMA de HSPSND cruzaban una frontera de 64 KB y
  ocupaban 16 KB más de memoria convencional). Quita la rutina que dejó
  Windows en la BIOS (ya no existe) poniendo la nuestra (después de C205h:
  hay BIOS que con C205h la olvidan), reinicia el ratón y, si era un
  IntelliMouse, lo pone en el modo de la rueda (frecuencias 200, 100 y 80
  con C202h); lo activa (C200h) y, por la vía directa, la IRQ 12 vuelve a
  ser nuestra con paquetes de 4 bytes. Un touchpad en modo absoluto
  (`/BORDE`) vuelve como ratón normal, sin el borde.

Probado con el disco de Carlos (Trabajo en Grupo 3.11, `mouse=*vmd`): el
puntero se mueve y pulsa en Windows, dos veces seguidas, y al salir
`HSPRATON /INFO` dice «vía directa ... paquetes de 4 bytes» con los
paquetes contando. Prueba automática: `tests/run_mouse_tests.sh ... windows`
con `WINSIM.COM` (`tests/progs/winsim.asm`), que hace lo de Windows: 1605h,
el ratón por la BIOS (cuenta los paquetes de los `mouse_move` del monitor),
lo deja como Windows y 1606h; después MOUSETST comprueba INT 33h y la
rueda.

Dentro de una sesión MS-DOS de Windows, INT 33h de HSPRATON responde pero no
recibe movimiento (el ratón es de Windows).

### Los demás residentes con Windows

- **HSPEMM:** en 1605h entrega a Windows la rutina que apaga y enciende el
  modo V86 (como EMM386) y se llama EMMQXXX0 mientras tanto; si hay memoria
  superior o EMS en uso, avisa y dice que no (`CX`=1). Es lo que pasa con la
  opción «Juegos» (DOS=UMB y LH): use «Windows» o «Normal».
- **HSPDPMI:** solo se carga en «Juegos» (donde Windows no arranca). Dentro
  de Windows no se le llega a preguntar: WIN386 contesta él a INT 2Fh 1687h
  en sus máquinas virtuales.
- **HSPSND:** en 1605h para la Sound Blaster emulada y en 1606h la vuelve a
  poner. Con la opción «Windows» no se carga.
- **HSPUSB:** en modo V86 sin VCPI (Windows 386 mejorado) no hace nada (no
  llega a los registros de las controladoras). Si la BIOS lleva el USB
  (emulación del teclado y el ratón PS/2), Windows los usa como PS/2. En un
  equipo **sin controlador 8042** (un MacBook) no hay arreglo: Windows 3.x
  habla con el teclado y el ratón por los puertos 60h/64h y no tiene
  controladores USB.

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
| Programas con poca pila dentro de una sesión MS-DOS | INT 21h trabaja en pilas propias del núcleo (como MS-DOS), que van en los datos de instancia: cada máquina virtual tiene las suyas (ver abajo) |
| VSHARE.386 escribe en los datos internos de MS-DOS | El núcleo deja 0FFh en el octeto 4 de su segmento: VSHARE se niega a cargarse (ver «Limitaciones») |

### Con `DOS=HIGH`

Windows arranca igual con el núcleo en la HMA (`DOS=HIGH`): Administrador de
programas, una sesión MS-DOS (en ella `MEM` dice que el DOS está en la HMA),
salir de la sesión y salir de Windows. Dentro de la sesión, un programa que
cierra la línea A20 por XMS (HMATEST /A, de las pruebas) no estorba: el
núcleo la vuelve a abrir, por el XMS de Windows, en cada llamada. Todos los
datos que Windows instancia o recorre (SDA, CDS, SFT, pilas, `win_inst`)
siguen en la memoria convencional, en el segmento del DOS.

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

### Datos de instancia (INT 2Fh 1605h)

Además de lo que DOSMGR instancia por su cuenta (CDS, SDA, enlace de la SFT,
lista de dispositivos), el núcleo declara en `win_inst` lo que solo él
conoce y debe ser propio de cada máquina virtual:

- las carpetas actuales de cada unidad (`cur_paths`);
- la columna de la consola y la tecla pendiente;
- **las pilas de INT 21h** (`k_inst`: disco, consola y auxiliar, sus marcas
  «en uso» y los punteros de pila guardados durante INT 24h e INT 25h/26h).
  Una máquina virtual puede quedarse parada dentro del DOS (esperando una
  tecla, por ejemplo) mientras otra lo llama: si compartieran la pila, la
  segunda pisaría la de la primera.

El formato se obtuvo observando las respuestas de un MS-DOS 6.22 original
(sala limpia: solo comportamiento de la interfaz, nada de su código) y el
uso que hace DOSMGR de ellas.

## Limitaciones conocidas

- **VSHARE.386** (compartición de ficheros de Trabajo en Grupo, el SHARE de
  Windows) no funciona: escribe en posiciones fijas de los datos de MS-DOS.
  El núcleo deja 0FFh en el octeto 4 de su segmento para que no se cargue;
  con 0 (la disposición de MS-DOS) Windows se queda colgado en el logotipo
  (probado el 20/11/2026). Con `device=vshare.386` activo, Windows avisa en
  cada arranque («No se puede cargar un dispositivo especificado en el
  archivo SYSTEM.INI ... vshare.386»): WINCONF comenta esa línea. No lo
  provoca nada de nuestro DOS que confunda al instalador (la pone siempre
  Trabajo en Grupo 3.11, con IFSMGR y VCACHE, que sí se cargan); sin él,
  Windows funciona igual con una sola máquina.
- **IFSHLP.SYS** (lo pide `CONFIG.WIN`): con el núcleo de la rama v0.1-nucleo
  del 20/11/2026 Windows arranca con él; con el núcleo que tenía instalado
  Carlos (19/11) se quedaba en el logotipo.
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
