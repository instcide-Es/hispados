# Hispa-486

El tercer sistema de la familia Hispa-DOS: el de mediados y finales de los 90.
Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.

## Qué es

- **Para quién:** los 486 y Pentium (y lo que vino después) con 8 MB o más.
  Es la gama de Windows 95/98.
- **Qué hace:** lo que hacía un PC de 1995-1999. Programas de Windows de 32
  bits (Win32: los de Windows 95), juegos con DirectX, navegador web, escritorio
  con **menú de inicio** y **barra de tareas**. Todavía con DOS por debajo
  (Hispa-DOS, el núcleo de Hispa-386), como Windows 95.
- **Objetivo de comportamiento:** Windows 95 y Windows 98 (mismo API Win32;
  el 98 añade funciones y DirectX 6-7), igual que MS-DOS 6.22 lo es de
  Hispa-DOS. Se empieza por lo común a los dos y se sigue con lo del 98. Sala limpia: se observa lo que hace y se lee la documentación
  pública (los manuales del API Win32 de la época y los proyectos libres que ya
  la reimplementaron, Wine y ReactOS, para saber qué hace cada función); nunca
  se copia código de nadie.
- **Lo que no es:** un núcleo nuevo. El núcleo sin DOS, con la tecnología de los
  2000, es el sistema siguiente: **Hispa-Pentium**, la gama de Windows XP, que se
  escribe en [Castel](../../castel/): su núcleo ya arranca, con paginación, tareas y
  programas en el anillo 3 ([../PENTIUM.md](../PENTIUM.md)).

## Cómo se construye: por encima de Hispa-386

Hispa-486 no empieza de cero: es Hispa-386 (núcleo Hispa-DOS, HSPXMS, HSPDPMI,
Hispa Ventanas, controladores) más lo que hace falta para los programas de
32 bits. Así cada pieza nueva se puede probar enseguida en el sistema que ya
funciona, y lo que se hace para Hispa-486 también mejora Hispa-386.

La pieza central es un **cargador de programas Win32 encima de HSPDPMI**
(como hacía HX DOS Extender): abre un `.EXE` PE, lo coloca en memoria de 32
bits, resuelve sus importaciones con funciones nuestras (KERNEL32, USER32,
GDI32...) y lo ejecuta en modo protegido. Los programas de consola salen por
la consola de Hispa-DOS; los de ventanas, por Hispa Ventanas.

## Fases

Cada fase, un hito pequeño y comprobable, con su prueba automática.

| Fase | Qué | Hito |
|---|---|---|
| 0 | Este documento; la lista de funciones por orden de uso (`tools/importaciones.py`, con los programas que aporte cada uno) | documento hecho (17/11/2026) |
| 1 | **Cargador PE** (`HW32.COM`): cabeceras, secciones, importaciones; KERNEL32 mínimo de consola | **hecha (17/11/2026)**: PEHOLA (a mano) y HOLAC (en C) corren en Hispa-DOS |
| 2 | KERNEL32 de consola completo: archivos (CreateFile, ReadFile, WriteFile, SetFilePointer, FindFirstFile...), directorios, hora y fecha, entorno, consola (colores, cursor, eventos de teclado) | **hecha (17/11/2026)**: KERNEL32 de consola (unas 180 funciones; K32PRUE da lo de Windows 98), carga de DLL y MSVCRT.DLL propia; **NASM 2.16 para Win32 ensambla en Hispa-DOS** |
| 3 | USER32/GDI32 sobre Hispa Ventanas: ventanas, mensajes, controles, menús, diálogos, dibujo | un programa Win32 de ventanas sencillo (el «Hola, mundo» de ventana). **3a hecha (17/11/2026)**: un Win32 de ventanas corre dentro de Hispa Ventanas y su `MessageBoxA` es un cuadro de Hispa Ventanas. **3b hecha (17/11/2026)**: el «Hola, mundo» de ventana (clase, ventana, bucle de mensajes, `WndProc`, `WM_PAINT` con texto y dibujos). **3c hecha (17/11/2026)**: KERNEL32 entero y carga de DLL dentro de Hispa Ventanas (MSVCRT.DLL), los controles de serie, los menús, los recursos y los diálogos de plantilla: una calculadora de Windows en C que se maneja con el ratón y el teclado. **3d hecha (18/11/2026)**: GDI completo (figuras, letras de cualquier tamaño, mapas de bits y DIB, regiones, ROP, paletas), barras de desplazamiento, caret, aceleradores, iconos y cursores, portapapeles, diálogos comunes y COMCTL32.DLL: **FASMW 1.73, el flat assembler de Windows, abre, edita y ensambla dentro de Hispa Ventanas** |
| 4 | El escritorio de Hispa-486: menú de inicio, barra de tareas, escritorio con iconos, «Mi PC» (Hispa Ventanas 2) | arranca a su escritorio. **4a hecha (18/11/2026)**: la barra de tareas y el menú Inicio (`VENTANAS /486` o `Escritorio=486` en VENTANAS.INI); el escritorio empieza despejado, con todos los programas en el menú. Falta: los iconos del escritorio y «Mi PC» |
| 5 | DirectX hasta el 7 (el del 98): DirectDraw (pantalla completa y doble búfer), DirectSound (sobre HSPSND), DirectInput (teclado, ratón, mandos); Direct3D por programa (sin aceleradora), luego con aceleradoras de la época | un juego DirectX sencillo; después, juegos 2D del 98 |
| 6 | Navegador web (HTML 3.2 y CSS sencillo, imágenes; HTTP y, con la red cifrada, HTTPS) | hispados.es en el navegador. **6a hecha (18/11/2026)**, adelantada por un pedido de Carlos («ya lo podemos usar para la biblio»): la red de los programas Hispa32 (`WSOCK32.DLL`: TCP/IP y Winsock 1.1), HTTPS (`HSPTLS.DLL`: TLS 1.2 con certificados) y **el Navegador** (`NAVEGA.EXE`), con el que se usa la biblioteca de ClicGest: [NAVEGADOR.md](NAVEGADOR.md). Después, ClicGest entero: imágenes, subir y bajar archivos, galletas guardadas. Falta: hojas de estilo de fuera |
| 7 | Consolas de su época: Hispa-Drive, Hispa-SNes, Hispa-BoyColor (con juegos propios) | los tres con un juego cada uno |
| 8 | Arranque UEFI nativo (cargador propio, disco y pantalla sin BIOS) | arranca en un PC sin CSM |
| 9 | Programas propios de la época: la oficina «-486», audio y vídeo | versión 1.0 |

## Cómo está hecho (fases 1 y 2)

- `utils/hw32.asm`: el cargador. Entra en modo protegido como cliente DPMI de
  32 bits, pide un bloque para la imagen, sus trampolines, lo suyo (TIB, línea
  de órdenes, entorno) y la pila, y hace los selectores del programa con base
  «bloque − ImageBase»: el programa ve las direcciones de siempre aunque no
  traiga reubicados. Cada importación apunta a un trampolín «push n; jmp lejos
  despacho»; el despacho llama a la función nuestra y vuelve quitando los
  argumentos (stdcall).
- `utils/hw32k.inc`: KERNEL32 de consola (se comporta como Windows 98, 4.10):
  consola (colores y cursor por la BIOS, rellenar en la memoria de la
  pantalla, teclado como eventos con pulsar y soltar), archivos y directorios
  (por DOS, con los mismos números de error, que son los de Win32), búsquedas
  (un DTA por búsqueda), hora (FILETIME desde 1601, sin husos), montón (clases
  de potencias de dos con listas de sueltos; lo de más de 1 MB, en su bloque de
  DPMI), VirtualAlloc, entorno de DOS, TLS, secciones críticas (un hilo),
  GetProcAddress (crea trampolines nuevos) y textos.
- Las DLL: HW32 carga las que pida el programa (de su carpeta, de la de HW32
  o de la de ahora), las reubica, resuelve sus importaciones (que pueden traer
  otras) y llama a sus DllMain antes del programa (primero aquellas de las que
  dependen las demás). `GetModuleHandle`, `LoadLibrary` (de las ya cargadas) y
  `GetProcAddress` las conocen.
- `w32/msvcrt.c`: **MSVCRT.DLL** propia, en C (se compila con `gcc -m32` y
  `elf2pe --dll`): arranque (`__getmainargs`, `_initterm`, `exit`...),
  archivos con búfer en modo texto y binario, `printf` y `scanf`, textos,
  ctype, `qsort`, números (también de 64 bits y con coma), hora, `stat`,
  `setlocale` («C») y matemáticas por el x87; los mensajes, en español. Las
  DLL de Win32 de Hispa-486 se escriben en C para empezar (gcc es solo la
  herramienta, como NASM); cuando Castel tenga 32 bits se podrán pasar a él.
- `tools/elf2pe.py`: un enlazador pequeño de objetos ELF de `gcc -m32` a PE,
  para hacer los programas de prueba en C sin compilador de Windows.
- Prueba: `tests/run_hw32_tests.sh` (PEHOLA, PEFALTA, HOLAC, K32PRUE con los
  resultados de Windows 98, K32CON mirando la pantalla, DLLPRUE/USADLL, que no
  se pierde memoria y, si se ha bajado con `tests/thirdparty/fetch_win32.sh`,
  NASM 2.16 para Win32 ensamblando desde un disco duro).

## Lo siguiente

1. Más programas reales de consola (de MinGW y de Visual C++), para ver qué
   funciones piden («falta la función...») y completarlas. Ya van: NASM y
   NDISASM 2.16 (MinGW) y **TCC 0.9.27, el compilador de C de Windows, que
   compila y ejecuta programas dentro de Hispa-DOS** (17/11/2026). Para verlo,
   `HW32 /T programa` traza cada llamada a KERNEL32 con sus argumentos.
   Lo que falta: lanzar otros procesos (`_spawn`, `CreateProcess`) y los
   nombres largos (los programas que buscan archivos de nombre largo los
   encuentran por su recorte 8+3, como en MS-DOS).
2. ~~Que COMMAND.COM ejecute los PE solo~~: hecho (17/11/2026): un .EXE con cabecera PE se lanza con HW32 (de la carpeta de COMSPEC, del PATH o de la actual); si no hay HW32, lo dice y no se lanza.
3. HW32 y MSVCRT.DLL en el sistema instalado (Hispa-386 0.9.23). La imagen
   y la instalación propias de Hispa-486: hechas (18/11/2026), ver «La
   imagen, la instalación y las actualizaciones de Hispa-486».
4. Fase 3: USER32 y GDI32 sobre Hispa Ventanas. La 3a, la 3b (el «Hola,
   mundo» de ventana), la 3c (KERNEL32 y las DLL dentro de Hispa Ventanas,
   los controles, los menús, los recursos y los diálogos) y la 3d (el GDI de
   los programas sencillos, los iconos y los cursores, los aceleradores, los
   diálogos comunes y COMCTL32.DLL, con FASMW de prueba) están hechas. Lo
   que sigue, en «Lo que queda tras la 3d».

## Wi-Fi (hecho el 18/11/2026; sin probar en la tarjeta)

Pedido de Carlos: «Wifi y Explorador web es lo único que necesitamos para que
el HP Mini pueda ser útil» (para la web de la biblioteca). `HSPWIFI` lleva la
Broadcom BCM43224 del HP Mini 5103 con WPA2-Personal y se presenta como un
packet driver Ethernet, así que todo lo que ya usaba `HSPRED` (y el
navegador) funciona igual por Wi-Fi; la red se elige con la orden `WIFI`. El
controlador de la radio son los fuentes de `brcmsmac` de Linux sin tocar,
sobre una capa propia; la gestión 802.11 y WPA2 son nuestras. Va en un
núcleo de 32 bits en memoria extendida (2 MB), con un residente de 4,6 KB.
Todo, en [docs/WIFI.md](../WIFI.md). Falta la prueba en el equipo de verdad
(hay una imagen preparada) y la ventana de Hispa Ventanas para elegir red.

## La batería por ACPI (hecha el 18/11/2026; sin probar en los equipos)

«HP Mini no se ve el indicador de la batería»: ni el HP Mini 5103 ni el
Toshiba NB250 tienen APM, que es por donde Hispa Ventanas pregunta la batería.
`HSPACPI` (nuevo, [docs/ACPI.md](../ACPI.md)) la lee por **ACPI** —ejecuta los
métodos de la batería que trae el propio equipo, con ACPICA de Intel en un
núcleo de 32 bits cargado en memoria extendida, como el de `HSPWIFI`— y
contesta él a INT 15h AX=530Ah como una BIOS con APM: el indicador de la
barra funciona sin tocarlo. `HSPACPI /AUTO` va en el `AUTOEXEC.BAT` de
Hispa-486 antes de `VENTANAS` (solo se queda si el equipo tiene una batería
que la BIOS no enseña). Residente: 1,6 KB; `HSPACPI.BIN`: 146 KB en disco;
3,1 MB de memoria extendida. Probado con las tablas ACPI de verdad de los dos
equipos y un controlador embebido simulado, y en QEMU hasta la pila de la
barra (`tests/run_acpi_tests.sh`); la prueba en los equipos, con la imagen de
`tools/mkprueba_bateria.sh` (FTP, `prueba-bateria`).

## Fase 6a: la red, HTTPS y el Navegador (hecha, 18/11/2026)

Pedido de Carlos: «Si le ponemos un explorador web al 486 ya lo podemos usar
para la biblio» (la biblioteca de la asociación, en ClicGest).

- **Qué hay:** el segundo disquete de Hispa-486, «Internet» (`make internet`,
  279 KB): el **Navegador** (`NAVEGA.EXE`, en Inicio › Internet), `HTTPS.EXE`
  (descargas cifradas desde la consola) y las dos DLL que les dan la red:
  `WSOCK32.DLL` (una pila TCP/IP propia en C y el subconjunto de Winsock 1.1)
  y `HSPTLS.DLL` (TLS 1.2 de cliente sobre BearSSL, con comprobación del
  certificado). Todo son programas Hispa32: en lo que ve el usuario se llaman
  así, no «de Windows».
- **Dónde se cuenta:** el Navegador, en [NAVEGADOR.md](NAVEGADOR.md) (qué
  entiende, cómo se maneja, cómo va con ClicGest, medidas y límites); la red
  y TLS, en [docs/RED.md](../RED.md), «La red de los programas Hispa32».
- **Lo que ha tocado de lo que había:**
  - HW32 (`utils/hw32k.inc`): tres funciones nuestras en KERNEL32
    (`HispaRedAbrir`, `HispaRedEnviar`, `HispaRedCerrar`) para las tramas del
    packet driver, y soltar la red al acabar el programa.
  - Hispa Ventanas: los colores de GDI admiten `PALETTEINDEX(n)` (los de su
    paleta: así un programa Hispa32 se pinta con la crema, la arena y la
    terracota de verdad, no con lo más parecido del cubo de colores); y el
    grupo «Internet» del Centro de programas, que se crea solo la primera vez
    que `NAVEGA.EXE` está en `\HISPADOS\INTERNET` (`ventanas/accesos.inc`,
    `inet_arranque`; clave `Internet=1`).
  - `tools/elf2pe.py`: `--exp a,b,c` (una DLL que solo exporta eso) y
    `w32/crt0.c` (el arranque de los programas de consola con `main`).
- **Pruebas:** `tests/run_red32_tests.sh` (la red y TLS desde la consola) y
  `tests/run_navegador_tests.sh` (el Navegador dentro de Hispa Ventanas),
  las dos sin Internet, con un servidor propio en el anfitrión.
- **Después (18/11/2026), «haz el navegador compatible con ClicGest.es»:**
  galletas que se guardan al cerrar (la de «Mantener la sesión iniciada»),
  imágenes (PNG, GIF y JPEG, con los formatos de Mirador tal cual), subir
  archivos (`multipart/form-data`, con el diálogo de abrir), descargas al
  disco (el diálogo de guardar, y abrir el PDF con el Lector), `pattern`,
  `<meta http-equiv=refresh>`, Mayús+Tab, y el User-Agent
  `HispaNavegador/0.3 (Hispa-486)`. Para eso usa de Hispa Ventanas
  `SetDIBitsToDevice` (mapas de 8 bits de arriba abajo con `DIB_PAL_COLORS`
  y la tabla 0..255: los octetos son colores de su paleta), `GetKeyState`
  (Mayús+Tab), `GetOpenFileNameA` y `GetSaveFileNameA` (de `COMDLG32`: el
  diálogo de elegir archivo de Hispa Ventanas, que deja la carpeta de
  trabajo como estaba) y `WinExec` (abre otro programa como un acceso
  directo: un `.HVP` se abre dentro; en `ventanas/win32n.inc`). Las cuatro
  primeras se hicieron primero para el Navegador y luego las dio, más
  completas, la fase 3d (`win32b.inc` y `win32x.inc`): queda una sola de
  cada una. Todo en [NAVEGADOR.md](NAVEGADOR.md).
- **Lo que sigue:** las hojas de estilo de fuera cuando sean pequeñas, y la
  pila residente para que dos programas usen la red a la vez.

## Fase 4a: la barra de tareas y el menú Inicio (hecha, 18/11/2026)

Pedido de Carlos al probar la demo: «en 486 se echa de menos la barra de
inicio abajo»; después: «todas las aplicaciones en el menú de inicio para
que quede el escritorio despejado» y «en 386 no hace falta poner ese menú de
inicio, porque así respeta la estética de Win 3.11».

- **Qué es:** el escritorio propio de Hispa-486 dentro de Hispa Ventanas.
  Abajo, una barra de tareas con el botón «Inicio», un botón por ventana
  abierta (un clic la trae delante, la minimiza o la recupera) y, a la
  derecha, la red, el volumen, la batería y la hora. El menú Inicio lleva
  todos los programas: cada grupo del Centro de programas es un submenú con
  sus accesos; debajo, Configuración, los administradores de archivos y de
  tareas, «Centro de programas» y «Salir de Hispa Ventanas...». El
  escritorio arranca despejado: el Centro no se abre (se abre desde Inicio,
  para crear grupos y accesos) y no hay barra arriba. Los programas de
  Windows (fase 3) salen en la barra como cualquier ventana.
- **Cómo se pone:** es un ajuste de la instalación de Hispa-486, no un botón
  de Configuración: la opción `VENTANAS /486` o la clave `Escritorio=486` en
  `\HISPADOS\VENTANAS.INI` (sección `[Escritorio]`). **La imagen y la
  instalación de Hispa-486 tienen que llevar una de las dos** (lo natural:
  la clave en el VENTANAS.INI que instalan; Configuración la conserva).
  Hispa-386 no lleva ninguna y su escritorio, el de la gama de Windows 3.1,
  sigue exactamente igual.
- **Aspecto:** el de Hispa Ventanas (marrón con filete ocre, terracota,
  crema, el rombo). Hace lo que hacían los escritorios de mediados de los 90,
  sin copiar el dibujo de ninguno.
- **Cómo está hecho:** `ventanas/barra.inc`, en la parte alta de
  VENTANAS.COM (unos 6 KB; en la parte baja, 132 octetos de ganchos). La
  barra es una ventana más, siempre encima de las que tienen título; no hay
  que avisarle de nada: mira las ventanas una vez por vuelta. Los detalles,
  en docs/VENTANAS.md, «El escritorio de Hispa-486: barra de tareas y menú
  Inicio».
- **Prueba:** `tests/run_barra_tests.sh` (en `make test`): 800×600 con
  `/486`, VGA de 640×480 y 16 colores con la clave de VENTANAS.INI y un
  programa de Windows, y el escritorio de Hispa-386 sin el ajuste.
- **Lo que sigue de la fase 4:** iconos en el escritorio (accesos directos y
  «Mi PC» con las unidades) y «Ejecutar...» y «Documentos» en el menú. La
  clave la ponen ya la imagen y el instalador de Hispa-486 (ver «La imagen,
  la instalación y las actualizaciones»).

## La imagen, la instalación y las actualizaciones de Hispa-486 (18/11/2026)

La imagen de Hispa-486 sale del proyecto, igual que la de Hispa-386: nada
se hace a mano. Lo que Hispa-486 tiene de más que Hispa-386 (hoy) son unos
pocos archivos; los programas y el núcleo son los mismos.

### Qué lleva de más

| Dónde | Qué | De dónde sale |
|---|---|---|
| `\AUTOEXEC.BAT` | el de Hispa-386 y, en el arranque normal, en vez de las líneas de la red con `REM`: `HSPRED /AUTO`, `HSPWIFI /AUTO` y `WIFI /AUTO` (la red puesta sola: ver «La red al arrancar»), `IF EXIST C:\HISPADOS\HSPACPI.COM C:\HISPADOS\HSPACPI /AUTO` (la batería por ACPI: docs/ACPI.md) y `C:\HISPADOS\VENTANAS`: arranca al escritorio | `usb/AUTOEXEC.BAT` (o `floppy/`), cambiado por `autoexec486()` de `tools/mkusb.py` |
| `\HISPADOS\VENTANAS.INI` | `[Escritorio]` `Escritorio=486`: la barra de tareas y el menú Inicio; `Fondo=FONDO.BMP` y `Ajuste=ajustada`: la imagen de fondo | `sistema/486/VENTANAS.INI` |
| `\HISPADOS\FONDO.BMP` | **la imagen de fondo del escritorio** (ver «La imagen de fondo»): 800×600, 8 bits, 481.078 octetos (470 KB) | `sistema/486/FONDO.BMP`, que `tools/mkfondo.py` saca de `sistema/486/HISPADOS.JPG` (`make fondo486`) |
| `\HISPADOS\CENTRO.INI` | los grupos de siempre más **«Hispa32»** (Calculadora, Hola, mundo, Controles, Menús y diálogos, Cuadros de mensaje) e **«Internet»** (Navegador), con `Internet=1` | `sistema/486/CENTRO.INI` |
| `\HISPADOS\INTERNET\` | **el Navegador** (docs/486/NAVEGADOR.md): `NAVEGA.EXE`, `HTTPS.EXE`, `HSPTLS.DLL`, `WSOCK32.DLL`, `MSVCRT.DLL`, `NAVEGA.INI` y `LEEME.TXT` (lo del disquete «Internet» menos `HW32.COM`, que ya está en `\HISPADOS`) | `build/` y `w32/navega/` (la tabla `INTERNET` de `tools/mkusb.py`) |
| `\HISPADOS\COMCTL32.DLL`, `UCRT.DLL` | las DLL de la fase 3d, al lado de `HW32.COM` y `MSVCRT.DLL`: los controles comunes y el CRT de Visual C++ 2015 | `build/` (la tabla `DLL486` de `tools/mkusb.py`; también en el disquete «Hispa-486») |
| `\HISPADOS\EDICION.INI` | `[Edicion]` `Gama=486` `Nombre=Hispa-486`: la **edición** instalada | `sistema/486/EDICION.INI` |
| `\HISPA32\` | los programas de 32 bits de ejemplo: `CALC32.EXE`, `HOLA32.EXE`, `CTL32.EXE`, `MENU32.EXE`, `AVISO32.EXE` | `build/WINCALC.EXE`, `WINHOLA`, `WINCTL`, `WINMENU`, `WINGUI` (se renombran al copiarlos) |

En lo visible se dice **«Hispa32»**, no «Windows» ni «Win32» (decisión de
Carlos). Los programas se siguen llamando `WIN*.EXE` en `build/`, en sus
reglas del Makefile y en sus pruebas (las de las fases 3a-3c); solo al
copiarlos a la imagen toman el nombre sin «WIN» (la tabla `HISPA32` de
`tools/mkusb.py`). Lo que todavía dice «Win32» son los títulos de algunas
ventanas de esos programas («Hola desde Win32», «Controles de Win32»,
«Prueba de Win32», el «Acerca de» de los menús): están en `tests/progs/` y
los miran las pruebas de la fase 3; se cambiarán con ellas.

El `CENTRO.INI` de `sistema/486/` es el que hace Hispa Ventanas la primera
vez (los grupos Principal, Accesorios, Juegos, Sonido y Productividad, con
sus accesos) más el grupo «Hispa32». `mkusb.py` quita los accesos a
programas que no estén en la imagen y pone `Modulos=` al número de módulos
.HVP de `JUEGOS` y `PROGS`; si alguno no tiene acceso en la plantilla (un
programa nuevo), pone `Modulos=0` y Hispa Ventanas lo añade solo a su grupo
al arrancar (docs/VENTANAS.md, «CENTRO.INI»). Así un programa nuevo no
obliga a tocar la plantilla, aunque conviene ponerlo en ella para que salga
en su sitio.

Hispa-386 no cambia: no lleva nada de esto (ni el Navegador ni la red al
arrancar: su `AUTOEXEC.BAT` sigue con las líneas de la red con `REM`) y su
escritorio es el de la gama de Windows 3.1 (sin menú Inicio). El controlador
Wi-Fi y su microcódigo ya iban en las dos (en `\HISPADOS`).

### El Navegador y su página de inicio

El `NAVEGA.INI` de la imagen es el del disquete «Internet»
(`w32/navega/NAVEGA.INI`): sin `Inicio=`, así que el Navegador arranca en su
página de bienvenida, con el favorito **«ClicGest: la biblioteca»**
(`https://clicgest.es/biblioteca`) y cómo se maneja. Se eligió así, y no
empezar directamente en la biblioteca, porque: es el mismo archivo que el
del disquete (el disco duro queda igual se instale de la memoria o de los
disquetes); la primera vez el equipo aún no tiene la red elegida (el Wi-Fi
se elige a mano una vez) y la bienvenida no da un error; y la biblioteca
queda a un clic. Quien quiera arrancar en ella pone en
`C:\HISPADOS\INTERNET\NAVEGA.INI` la línea
`Inicio=https://clicgest.es/biblioteca` (al actualizar, el `NAVEGA.INI` se
conserva, como todo `*.INI`).

### La red al arrancar

En el arranque normal («Normal» del menú de CONFIG.SYS), el `AUTOEXEC.BAT`
de Hispa-486 pone la red solo, eligiendo la que haya, **el cable primero**:

```
C:\HISPADOS\HSPRED /AUTO
C:\HISPADOS\HSPWIFI /AUTO
C:\HISPADOS\WIFI /AUTO
C:\HISPADOS\VENTANAS
```

- `HSPRED /AUTO`: si hay tarjeta de cable de las admitidas, se queda
  (13 KB de memoria convencional); si no, sale en silencio sin ocupar nada.
- `HSPWIFI /AUTO` (nuevo, docs/WIFI.md): solo se carga si hay **una red
  guardada** en `RED.INI` (la clave `WIFI=`, que escribe `WIFI CONECTAR`) y
  **una tarjeta Wi-Fi** de las admitidas (se pregunta a la BIOS del PCI, sin
  cargar los 400 KB del núcleo: el arranque sin Wi-Fi no se alarga). Si
  `HSPRED` está cargado y su tarjeta **tiene enlace**, o no lo sabe (NE2000,
  Tulip), se queda el cable y `HSPWIFI` calla. Si **no tiene enlace** (no
  hay cable) y no le llega en 3 segundos, `HSPWIFI` descarga `HSPRED` y se
  carga él. En todos los casos en que no se carga, no dice nada.
- `WIFI /AUTO`: conecta con la red guardada (sin `HSPWIFI`, calla).

Las dos a la vez no van (docs/WIFI.md, «Límites»); así siempre queda una
sola. La primera vez, en el HP Mini, se elige la red a mano: `HSPWIFI`
(si se cargó `HSPRED` y no hay cable, `HSPWIFI` lo descarga solo),
`WIFI REDES` y `WIFI CONECTAR número`; desde el siguiente arranque, sola.
La dirección (DHCP) la pide el primer programa que usa la red (el Navegador
o `CONFRED`), como siempre.

El microcódigo de la Broadcom (`BCM43XX.FW`, `BCM43XXH.FW` y su licencia
`BCM43XX.TXT`) se queda en `\HISPADOS`, que es donde lo busca `HSPWIFI` (en
la unidad de arranque) y lo que INSTALAR y ACTUALIZAR ya copian enteros: no
hizo falta otra carpeta. Su licencia (`LICENCE.broadcom_bcm43xx`) deja
repartirlo completo y sin cambios con una copia de ella, que va al lado.

### La imagen de fondo

El escritorio de Hispa-486 sale con una imagen de fondo (pedido de Carlos,
18/11/2026): **un monitor de los 90 con Hispa-DOS e Hispa Ventanas en la
pantalla**. La imagen es de Carlos Sanz (`sistema/486/HISPADOS.JPG`,
1376×768, 560 KB). Cómo se pinta, las formas de ponerla, la memoria y
cómo se cambia (Configuración › Escritorio; Mirador), en docs/VENTANAS.md,
«La imagen de fondo del escritorio»; aquí, lo propio de la edición.

- **El archivo que se instala** no es el JPEG (Hispa Ventanas no lo
  descodifica al arrancar), sino `\HISPADOS\FONDO.BMP`: un BMP de 8 bits de
  **800×600** (el modo normal de Hispa Ventanas), ya con los colores fijos
  de su paleta (el cubo de 6×6×6 y los 24 grises) y difuminado
  (Floyd-Steinberg). Se lee tal cual, sin convertir nada: **481.078 octetos
  (470 KB)** en el disco y 480.000 (469 KB) de memoria extendida mientras
  Hispa Ventanas está en marcha; nada de memoria convencional.
- **De 1376×768 a 800×600:** la foto es más ancha que la pantalla (16:9
  frente a 4:3). Se ha **recortado por los lados** (`mkfondo.py --modo
  recortar`, lo normal: quedan los 1024 puntos del centro, con el monitor y
  el teclado enteros; lo que se pierde es fondo oscuro) para que llene la
  pantalla, en vez de dejar dos franjas del color del escritorio arriba y
  abajo. Con `--modo ajustar` saldría entera en 800×447 (358 KB) con esas
  franjas. La barra de tareas tapa los 28 puntos de abajo.
- **En otras resoluciones:** con `Ajuste=ajustada`, en 640×480 se reduce
  (cada punto, la media de los que le caen encima) y en 1024×768 se amplía;
  la proporción es la misma (4:3), así que siempre llena la pantalla. En la
  VGA de 16 colores no hay imagen: queda el dibujo de rombos.
- **Cómo se rehace** (solo si cambia la foto; el BMP va en el repositorio,
  así que `make usb486` no necesita leer JPEG):

  ```
  make fondo486   # python3 tools/mkfondo.py sistema/486/HISPADOS.JPG sistema/486/FONDO.BMP
  ```

- **Dónde va:** en la imagen USB, en `\HISPADOS` (la clave `Fondo=FONDO.BMP`
  no lleva unidad: es de la carpeta de VENTANAS.COM, así que vale igual
  arrancando de la memoria y del disco duro instalado). En los disquetes,
  en el de **«Hispa-486»** (`\HISPADOS\FONDO.BMP`: INSTALAR copia todo su
  `\HISPADOS`), que tenía sitio de sobra: pasa de 33 KB a unos 505 KB
  ocupados de 1.440 (quedan 919 KB libres). No cabe en el de arranque ni hace falta. El paquete de
  ACTUALIZAR (`make paq486`) la lleva como cualquier otro archivo de
  `\HISPADOS`.
- **Quien ya tiene Hispa-486** y actualiza: recibe `FONDO.BMP`, pero su
  `VENTANAS.INI` se conserva (es suyo), así que no le cambia el escritorio;
  la pone, si quiere, en Configuración › Escritorio › «Elegir imagen...»
  (`C:\HISPADOS\FONDO.BMP`). En una instalación nueva, o al pasar de
  Hispa-386 a Hispa-486 (que estrena `VENTANAS.INI`), sale puesta.
- **Quitarla:** Configuración › Escritorio › «Sin fondo» (vuelve el dibujo
  de Configuración › Pantalla).
- **Hispa-386 no cambia:** no lleva `FONDO.BMP` ni la clave; su escritorio
  es el de siempre. Puede poner cualquier imagen igual que Hispa-486.

Pruebas: `tests/run_imagen486_tests.sh` (al arrancar la memoria, la pantalla
se compara con `sistema/486/FONDO.BMP`; el disco duro instalado lleva
`\HISPADOS\FONDO.BMP` y el disquete «Hispa-486» también) y
`tests/run_fondo_tests.sh` (todo lo demás).

### Cómo se hace

```
make usb486     # build/hispa486-usb.img y build/hispa486-disquete.img
make paq486     # build/hispa486.paq (el paquete de ACTUALIZAR)
```

`make usb486` (`python3 tools/mkusb.py 486`) hace la misma imagen de 512 MB
que `make usb` con lo de la tabla. Necesita lo mismo que `make usb` más
`gcc -m32` (los programas de 32 bits). También hace el disquete
**«Hispa-486»** (`build/hispa486-disquete.img`, etiqueta `HISPA486`): para
instalar desde disquetes son los de Hispa-386 (el de arranque,
`hispados-disquete.img`; «Instalación 2», `hispados-instal2.img`; y
«Oficina», `hispados-oficina.img`) y este, el último. Lleva `\HISPA32`, los
tres .INI de la edición y la imagen de fondo (`FONDO.BMP`) en `\HISPADOS` y
el `AUTOEXEC.BAT` que arranca el escritorio.

Tamaños (18/11/2026, con el Navegador y la red): la imagen USB,
512 MB (536.870.912 octetos) con 3,5 MB de archivos (3.665.995 octetos en
142 archivos; la de Hispa-386, 3.384.791 en 121); en `.zip`, 2,2 MB
(2.267.184 octetos). El disquete «Hispa-486», 33 KB ocupados de 1.440; el
de «Internet», 283 KB (282.691 octetos en 8 archivos).

### Cómo se publica

1. `make usb486` (y `make test`, o al menos `tests/run_imagen486_tests.sh`,
   `tests/run_barra_tests.sh` y las de las fases 3; mejor en el PC con
   `tools/pruebas_pc.sh`, que ya compila `usb486`).
2. La descarga: `cd build && zip -9 hispa486-usb.zip hispa486-usb.img`
   (y, si se quieren, los cuatro disquetes en `hispa486-disquetes.zip`).
3. La Release de GitHub (`hispa486-0.x`) con el `.zip` y sus sumas
   (`sha256sum`), y la demo de hispados.es: la imagen tal cual en
   `v86/hispa486.img`.
4. Para ACTUALIZAR: `make paq486`, subir `build/hispa486.paq` al servidor y
   poner su sección, con `gama=486`, en `versiones.txt` (ver abajo).

### La instalación (INSTALAR)

**La edición la decide el origen**, sin preguntar nada: lo que se instala
desde la memoria de Hispa-486 es Hispa-486 y desde la de Hispa-386, Hispa-386.
INSTALAR copia `\HISPADOS` entero (así llegan `EDICION.INI`, `VENTANAS.INI`
y `CENTRO.INI`), `\HISPADOS\INTERNET` (el Navegador) y la carpeta `\HISPA32`
si el origen las trae, y el
`AUTOEXEC.BAT` del origen (con las letras pasadas a C:). Es lo mismo que ya
hacía con todo lo demás; así una memoria USB de otra edición futura no
necesita otro instalador.

Al **actualizar** ([A]) un disco que ya tiene Hispa-DOS se mira
`\HISPADOS\EDICION.INI` en el origen y en el disco:

| Disco | Origen | Qué hace |
|---|---|---|
| Hispa-386 | Hispa-386 | lo de siempre |
| Hispa-486 | Hispa-486 | lo de siempre: se conservan los `*.INI` (su `CENTRO.INI`, su `VENTANAS.INI`...) |
| Hispa-386 | Hispa-486 | **pasa a Hispa-486**: lo dice antes de confirmar; su `VENTANAS.INI` y su `CENTRO.INI` se guardan como `VENTANAS.ANT` y `CENTRO.ANT` (como ya se hacía con `CONFIG.SYS` y `AUTOEXEC.BAT`) y se ponen los de Hispa-486 |
| Hispa-486 | Hispa-386 (memoria USB) | **no se deja**: lo dice y no toca nada. Con [N] (instalación nueva, se borra todo) sí se puede volver a Hispa-386 |

**Desde disquetes** (los de Hispa-386), la elección es una **pregunta**: al
final, después de «Instalación 2» y «Oficina», INSTALAR ofrece el disquete
«Hispa-486»; con S se pone (y si el disco era un Hispa-386 que se actualiza,
con los `.ANT` de arriba); con N queda Hispa-386. El `AUTOEXEC.BAT` del
disquete «Hispa-486» solo sustituye al de Hispa-386 si este se acaba de
poner (en una instalación nueva sobre un disco que ya tenía uno propio, se
respeta el suyo, como siempre). El `CENTRO.INI` del disquete es el de la
imagen USB: si no se ha puesto «Oficina», sus accesos (Pluma, Cuentas...)
quedan en el menú sin programa hasta que se copie (`XCOPIAR A:\HISPADOS
C:\HISPADOS /S` con el disquete «Oficina»).

Después, si el disco ha quedado de Hispa-486, INSTALAR ofrece el disquete
**«Internet»** (`make internet`, `build/internet.img`): sus `*.EXE`, `*.DLL`,
`NAVEGA.INI` y `LEEME.TXT` van a `\HISPADOS\INTERNET` (al actualizar se
conserva el `NAVEGA.INI` que hubiera). Con N, el acceso «Navegador» queda en
el menú sin programa hasta que se copie (`XCOPIAR A:\*.* C:\HISPADOS\INTERNET\`).
Así, desde disquetes son cinco: el de arranque, «Instalación 2», «Oficina»,
«Hispa-486» e «Internet» (y el de arranque otra vez entre uno y otro).

### Las actualizaciones por la red (ACTUALIZAR)

Cada instalación se actualiza **en su gama**:

- La gama instalada es la clave `Gama=` de `\HISPADOS\EDICION.INI` (sin el
  archivo o sin la clave, 386). ACTUALIZAR la enseña: «Tienes instalado
  Hispa-DOS 0.9.25 (Hispa-486).».
- En `versiones.txt`, cada sección puede llevar `gama=486`; sin ella es de
  la gama 386 (así el `versiones.txt` de hoy sigue valiendo para todas las
  instalaciones de Hispa-386, y un Hispa-486 no se «actualiza» a Hispa-386).
- Las secciones de **otra** gama no se miran: ni se ofrecen ni se avisan.
  Dentro de la gama, todo es como antes (versión, `nivel`, requisitos).
- El paquete de Hispa-486 (`make paq486`) trae también `\HISPA32` y
  `\HISPADOS\INTERNET`; al aplicarlo se copian, si están. Sus `*.INI` solo se ponen si el disco no los
  tiene (los del usuario se conservan).
- Para pasar de Hispa-386 a Hispa-486 no se usa ACTUALIZAR: se arranca la
  memoria de Hispa-486 e INSTALAR, [A] (ver arriba).

```
[0.9.26]
cpu=386
edicion=Hispa-386 (con Hispa Ventanas)
paquete=http://actualiza.clicgest.es/hispados-0.9.26.paq

[0.9.26]
gama=486
cpu=386
edicion=Hispa-486
paquete=http://actualiza.clicgest.es/hispa486-0.9.26.paq
```

(El número de la sección es el de Hispa-DOS, `lib/version.inc`, el mismo
para las dos gamas; «Hispa-486 0.1» es el nombre de la entrega.) El formato
completo, en docs/RED.md, «Formato de versiones.txt».

### Las pruebas

- `tests/run_imagen486_tests.sh` (en `make test`; sin `gcc -m32` se salta):
  arranca `build/hispa486-usb.img` en QEMU y mira el escritorio (la barra de
  tareas, sin el Centro ni la barra de arriba), el menú Inicio con el grupo
  «Hispa32» y sus cinco programas, la calculadora de 32 bits abierta desde
  él (6 × 7 = 42) y «Hola, mundo»; instala en un disco duro vacío (y mira
  lo que queda en él) y arranca solo el disco duro (su escritorio);
  actualiza un Hispa-486 con la memoria de Hispa-486 (conserva los ajustes),
  un Hispa-386 con la de Hispa-486 (pasa, con los `.ANT`) y un Hispa-486 con
  la de Hispa-386 (no se deja, el disco no cambia); ACTUALIZAR por la red
  en el Hispa-486 instalado (dice su gama, no mira la sección de 386, baja e
  instala la de 486 con su `\HISPA32` y su `\HISPADOS\INTERNET`,
  conservando el `NAVEGA.INI`); la instalación desde los disquetes, con
  «Hispa-486» e «Internet» al final; y **la red**: el disco duro instalado
  lleva `\HISPADOS\INTERNET`, el Wi-Fi con su microcódigo y las tres líneas
  de la red en el `AUTOEXEC.BAT`; la memoria arranca con una e1000
  (`HSPRED` se queda, `HSPWIFI` calla), Inicio › Internet › Navegador abre
  una página del servidor de pruebas del Navegador (`tests/tools/nav_servidor.py`,
  por HTTP, sin Internet) y `CONFRED` enseña la dirección del DHCP; el disco
  duro arranca sin tarjeta de red y sin ningún aviso. Y que Hispa-386 sigue
  con su `AUTOEXEC.BAT` (la red a mano) y sin `\HISPADOS\INTERNET`.
- `tests/run_wifi_tests.sh`: `HSPWIFI /AUTO` calla sin red guardada, sin
  tarjeta y con `HSPRED` con enlace; con el cable quitado (`set_link off`
  de QEMU) descarga `HSPRED` y se carga.
- `tests/run_actualizar_tests.sh`: en un Hispa-386, una sección con
  `gama=486` no se mira.
- Siguen igual las de Hispa-386: `run_usb_tests.sh`, `run_instalar_usb_tests.sh`,
  `run_instalar_tests.sh`, `run_actualizar_*_tests.sh`.

### Lo que habrá que añadir con las piezas que vienen

- ~~Navegador~~ y ~~Wi-Fi~~: hechos (18/11/2026; arriba, «El Navegador y su
  página de inicio» y «La red al arrancar»). Si llegan más tarjetas Wi-Fi
  con microcódigo grande, valorar entonces una carpeta propia
  (`\HISPADOS\FIRMWARE`) o un disquete.
- **FAT32**: si la imagen USB pasa a FAT32 (más de 2 GB o para el
  navegador), `tools/mkusb.py` tiene que formatear en FAT32 (`mformat -F`,
  con el sector de arranque FAT32 propio en vez de `boot16.bin`, y el tipo
  de partición 0Bh/0Ch en el MBR, hoy 06h fijo) para las dos ediciones; y
  `tests/tools/sinmenu.sh` y las pruebas que montan la imagen con
  `@@1048576` o `OFF=32256` siguen valiendo si no cambia el comienzo de la
  partición. INSTALAR llama a FORMATEA y a SISTEMA: lo que cambie en ellos
  para FAT32 le llega solo; el `/V:HISPADOS /AUTOTEST` de FORMATEA, a revisar.

## Discos grandes y FAT32 (hecha, 18/11/2026)

Pedido de Carlos: «Hispa-486 tendría que ser compatible con discos duros
grandes». Es lo que trajo MS-DOS 7.1 (el DOS de Windows 95 OSR2 y 98): FAT32
y discos de más de 128 GB. Está en el núcleo de siempre, así que también lo
tiene Hispa-386; allí **FAT16 sigue siendo lo de por omisión** (FDISK solo
crea FAT32 si se pide).

- **Núcleo** (`kernel/kernel.asm`): monta las particiones 0Bh y 0Ch,
  primarias y lógicas (y una FAT32 sin particiones); FAT de 28 bits, raíz en
  cadena de clústeres, FSInfo (libres y siguiente libre, que se escriben al
  acabar cada función), FAT sin espejo (bit 7 de +28h); clústeres de 32 bits
  en la SFT, la DTA y las FCB (la parte alta en +14h de la entrada). INT 21h
  **7302h** (DPB ampliado), **7303h** (espacio libre sin tope) y **7305h**
  (sectores absolutos de 32 bits); 36h da como mucho 2 GB, como MS-DOS 7.1;
  INT 25h/26h no valen en una FAT32 (los programas de disco viejos la
  estropearían); 440Dh/60h con CH=48h da el BPB de 53 octetos y, en una
  partición 0Bh/0Ch sin formato, uno de FAT32 (así FORMAT sabe qué le toca).
  Los discos van por INT 13h con LBA (paquete de 64 bits): por encima de
  128 GB la BIOS usa el LBA de 48 bits de la controladora; con la tabla de
  particiones de MS-DOS (32 bits) el tope es 2 TB.
- **FDISK** (`PARTIR`): `/FAT32` o, en el menú, la pregunta «¿Desea activar
  la compatibilidad con discos grandes (S/N)?» cuando el disco pasa de
  512 MB (Intro = no). Con ella, las unidades de 512 MB o más son FAT32 (0Bh,
  o 0Ch si pasan del cilindro 1024), sin el tope de 2.047 MB; primarias y
  lógicas. `PARTIR /INSTALAR` (INSTALAR) hace la misma pregunta.
- **FORMAT**: FAT32 si la partición es 0Bh/0Ch, si pasa de 2 GB o con
  `/FAT32`; clústeres como los de Microsoft (512 octetos hasta 260 MB, 4 KB
  hasta 8 GB, 8 KB hasta 16 GB, 16 KB hasta 32 GB, 32 KB por encima); 32
  sectores reservados con FSInfo en el 1 y copias en el 6 y el 7; escribe con
  7305h; `/S` con el arranque de FAT32 (`boot/boot32.asm`: un sector, LBA o
  CHS). Las cifras, en 64 bits («214.695.870.464 bytes»).
- **SYS**, **ETIQUETA**: leen y escriben con 7305h; en FAT32 el BPB ampliado
  va en +40h (unidad, firma, serie, etiqueta) y se escribe también la copia
  del sector de arranque.
- **REVISAR** (CHKDSK): como el CHKDSK de MS-DOS 7.1, en FAT32 **informa sin
  corregir** (con /F lo dice). La FAT se lee a trozos y de cada clúster se
  guarda un bit; si el mapa no cabe (200 GB son 6,5 millones de clústeres),
  se repite el recorrido por ventanas. Encuentra cadenas rotas, tamaños que
  no cuadran, vínculos cruzados y unidades perdidas.
- **DIR** y el intérprete, el **Gestor** y el **Administrador de archivos**:
  espacio libre con 7303h (más de 2 GB sin desbordar).
- **HSPUSB**: memorias USB con FAT32 (particiones 0Bh/0Ch o sin particiones);
  el núcleo la monta leyendo su sector de arranque. Los discos IDE/SATA van
  por la BIOS (no hay camino AHCI propio para discos: el de HSPCD es solo
  para CD).
- **Memoria**: el núcleo crece unos 2,5 KB (sin DOS=HIGH un programa recibe
  572 KB en vez de 575; con DOS=HIGH el código de FAT32 va a la HMA).
- **Prueba:** `tests/run_fat32_tests.sh` (en `make test`): discos dispersos
  de 4, 40 y 200 GB (FDISK, FORMAT, copiar, DIR, libres, ETIQUETA, REVISAR,
  SYS, arrancar de la FAT32, primaria y lógica), un disco de `mkfs.fat`
  leído y escrito, `fsck.fat` limpio en todo lo nuestro y REVISAR con un
  vínculo cruzado y cadenas perdidas puestos a mano.
  55 comprobaciones, unos 5 minutos (los discos no ocupan lo que miden).

### Lo que faltaría: nombres largos (LFN)

FAT32 y los nombres largos son cosas distintas (los nombres largos de
Windows 95 también van en FAT12/16). Hoy las entradas de nombre largo
(atributo 0Fh) se respetan: no se enseñan, REVISAR no las toma por archivos
y los archivos se ven por su nombre corto. Para tenerlos haría falta:

1. En el núcleo, las funciones 71xxh de INT 21h (las de Windows 95 en DOS:
   714Eh/714Fh/71A1h buscar, 716Ch abrir/crear, 7139h/713Ah/713Bh carpetas,
   7141h borrar, 7156h renombrar, 7143h atributos, 7147h carpeta actual,
   7160h nombre completo, 71A0h información del volumen): leer y escribir
   la cadena de entradas 0Fh (UCS-2, suma de comprobación del nombre corto)
   delante de la entrada 8.3, y generar el nombre corto («NOMBRE~1.TXT»).
2. Que al borrar o renombrar con las funciones de siempre se marquen
   también sus entradas de nombre largo (si no, quedan huérfanas, como en
   MS-DOS 6.22; SCANDISK las limpia).
3. COMMAND.COM (DIR con el nombre largo a la derecha, como Windows 95),
   COPY/MOVER/REN con comillas, el Administrador de archivos y HW32
   (`FindFirstFileA`, `CreateFileA`... con nombres largos de verdad).
4. REVISAR: comprobar las cadenas de nombre largo (orden, suma de
   comprobación) y quitar las huérfanas.

## Fase 1: el cargador PE

- **Nombre:** `HW32.EXE` («Hispa Win32»). Se usa como `HW32 PROGRAMA.EXE
  parámetros`; más adelante, COMMAND.COM lo llama solo cuando ve un PE.
- **Necesita:** un 386 o más, HSPXMS y HSPDPMI cargados (como Hispa Ventanas).
- **Memoria:** pide a DPMI (INT 31h, 0501h) un bloque del tamaño de la imagen;
  si no puede cargarlo en su `ImageBase`, usa los **reubicados** del programa
  (`.reloc`); si no los tiene, hace que los selectores de código y datos tengan
  como base `bloque - ImageBase`, así el programa ve sus direcciones de
  siempre.
- **Importaciones:** cada función importada apunta a la nuestra si la hay; si no,
  a una que dice «Hispa-486: falta KERNEL32!NombreFunción» y acaba el programa
  (así se sabe qué hacer después, con la lista de `tools/importaciones.py`).
- **KERNEL32 de la fase 1:** GetStdHandle, WriteFile, WriteConsoleA, ReadFile,
  ReadConsoleA, ExitProcess, GetCommandLineA, GetModuleHandleA, GetProcAddress
  (de lo nuestro), GetLastError, SetLastError, VirtualAlloc, VirtualFree,
  GetProcessHeap, HeapCreate, HeapAlloc, HeapReAlloc, HeapFree, HeapSize,
  GetVersion, GetVersionExA, GetEnvironmentStrings(A), FreeEnvironmentStrings,
  GetACP, GetOEMCP, GetCPInfo, lstrlenA, lstrcpyA, lstrcatA, lstrcmpA,
  MultiByteToWideChar, WideCharToMultiByte (página 437/1252), GetTickCount,
  GetSystemTime, GetLocalTime, Sleep, SetConsoleCtrlHandler, GetFileType,
  SetHandleCount, GetStartupInfoA, IsBadReadPtr/IsBadWritePtr, Raise/
  UnhandledExceptionFilter (lo justo).
- **Prueba:** programas PE hechos por nosotros (NASM con la cabecera PE escrita a
  mano, y un compilador de C cruzado si está en el equipo de pruebas), en QEMU
  con Hispa-DOS: que escriban lo esperado y acaben con su código de salida.

## Fase 3: programas de Windows con ventanas (diseño)

Un programa Win32 de ventanas tiene que correr **dentro de Hispa Ventanas**,
como una tarea más (multitarea cooperativa), no saliendo a Hispa-DOS como los
de consola. Lo que hay que resolver:

- **Cargarlo.** Los módulos .HVP se cargan reubicados y comparten los
  segmentos de Hispa Ventanas (base del programa, 4 GB). Los .EXE de Windows de
  la época casi nunca traen reubicados (`/FIXED`), así que cada programa de
  Windows lleva **sus propios selectores** (código, datos y FS para su TIB), con
  base «bloque − ImageBase», como en HW32. El cargador de PE de HW32 (cabeceras,
  secciones, reubicados, importaciones, DLL) se saca a un archivo común que
  usen los dos.
- **Las llamadas.** Igual que en HW32: cada importación, un trampolín que salta
  lejos al despacho de Hispa Ventanas; el despacho pasa a los segmentos del
  entorno, llama a la función (USER32/GDI32/KERNEL32 hechas sobre la interfaz
  de `ventanas/api.inc`) y vuelve con RETF quitando los argumentos.
- **Las tareas.** El cambio de tarea de Hispa Ventanas tiene que guardar y
  devolver también DS, ES, FS y SS de las tareas de Windows. `GetMessage` es el
  punto donde la tarea cede el turno.
- **USER32 sobre Hispa Ventanas.** Ventanas (`CreateWindowExA`, clases con
  `RegisterClassA`, `DefWindowProcA`), mensajes (`GetMessageA`,
  `DispatchMessageA`, `PostQuitMessage`, traducción de los mensajes de Hispa
  Ventanas a WM_* de Windows), cuadros (`MessageBoxA`), menús y controles
  (BUTTON, EDIT, STATIC, LISTBOX) con los de `controles.inc`.
- **GDI32.** `BeginPaint`/`EndPaint`, `TextOutA`, `SetTextColor`, pinceles y
  plumas, `Rectangle`, `LineTo`, mapas de bits (`BitBlt`, `StretchDIBits`) sobre
  la superficie de dibujo de Hispa Ventanas.

Pasos, cada uno con su prueba:

1. **3a** — un PE que solo llama a `MessageBoxA` sale como cuadro de Hispa
   Ventanas (cargador común, selectores propios, despacho, tarea).
2. **3b** — «Hola, mundo» de ventana: `RegisterClassA`, `CreateWindowExA`,
   bucle de mensajes, `WM_PAINT` con `TextOutA`, cerrar.
3. **3c** — KERNEL32 y las DLL dentro de Hispa Ventanas, controles, menús,
   recursos (`.rsrc`: `LoadStringA`, `LoadMenuA`) y diálogos de plantilla
   (`DialogBoxParamA`): una calculadora en C con MSVCRT.
4. **3d** — GDI completa para los programas sencillos de la época (dibujo,
   mapas de bits, tipos de letra), iconos y cursores del programa,
   aceleradores y diálogos comunes.

### Cómo está hecho (fase 3a)

Está en `ventanas/win32.inc` (va en la parte alta de `VENTANAS.COM`: unos
2 KB de código de 32 bits). Prueba: `tests/run_win32gui_tests.sh`.

- **Quién lo abre.** Hispa Ventanas mira la cabecera del `.EXE` que se va a
  abrir (desde un acceso directo del Centro, el Administrador de archivos o
  «Reabrir»): si es un PE de 386 con **subsistema 2** (ventanas), lo abre
  dentro, como un módulo .HVP (`es_hvp` → `es_pe_gui`; `hvp_abrir` →
  `pe_abrir`). Si es un PE de **consola**, sale a Hispa-DOS como cualquier
  programa, pero el lanzador lo ejecuta con `COMSPEC /C` (que es quien lo pasa
  a HW32) y **sin quitar HSPDPMI** (bit 4 de `lz_band`): antes, un Win32 de
  consola abierto desde Hispa Ventanas se ejecutaba como si fuera de DOS y
  colgaba el equipo.
- **La carga.** Un bloque de DPMI (0501h) con la imagen, 8 KB de trampolines,
  4 KB de lo suyo (la TIB y la línea de órdenes) y su pila (lo que pida la
  cabecera, entre 64 y 256 KB). Tres selectores propios (código, datos y pila,
  FS) con base «bloque − ImageBase» y 4 GB; FS, con la base en la TIB. No se
  reubica nada. En esta fase no se cargan DLL: lo que no sea de KERNEL32 o
  USER32 «falta».
- **Los punteros.** Hispa Ventanas tiene un único segmento de datos de 4 GB
  con la base de `VENTANAS.COM`; el programa, otro con la base «bloque −
  ImageBase». Un puntero del programa se convierte en puntero de Hispa
  Ventanas sumándole `PE_DELTA` = (bloque − ImageBase) − base de Hispa
  Ventanas (la suma da la vuelta por 2^32 y cae en la misma dirección
  lineal). Así USER32 y KERNEL32 leen y escriben la memoria del programa con
  el segmento de Hispa Ventanas, sin cambiar de segmento.
- **Los trampolines.** Cada importación apunta a un trampolín de 28 octetos
  dentro del espacio del programa: `push trampolín; jmp lejos pe_despacho` y,
  detrás, su ficha: el programa, la función (su ficha en `w32_tabla`), el
  nombre y la DLL. El número que se empuja es el propio trampolín (como
  puntero de Hispa Ventanas): el despacho sabe de qué programa es la llamada
  sin mirar qué tarea corre (hará falta en la 3b, cuando un `WndProc` se
  llame desde otra tarea).
- **Dos pilas.** La tarea empieza en la pila de Hispa Ventanas (`pe_tarea`),
  apunta su ESP en la ficha (`PE_ESPV0`, `PE_ESPV`), pasa a la pila y a los
  segmentos del programa y salta a su entrada con RETF. En la pila del
  programa deja la vuelta a un trampolín de salida, por si la entrada hace
  `ret`.
- **El despacho** (`pe_despacho`). Llega con la pila y los segmentos del
  programa: guarda DS, ES, FS, GS, EBP, EBX, ESI y EDI en la pila del
  programa; pone los segmentos de Hispa Ventanas (también en FS y GS: mientras
  corre lo demás no queda puesto ningún selector del programa); pasa a la pila
  de Hispa Ventanas (`PE_ESPV`), donde guarda el ESP del programa (y el de la
  llamada anterior, `PE_ESPP`: se puede anidar); llama a la función con
  EBP = los argumentos ya convertidos, EDX = la ficha del programa y EBX = el
  trampolín; vuelve a la pila del programa, recoloca la vuelta lejos encima de
  los argumentos y hace RETF (stdcall). Como solo se cambia de tarea dentro
  de las funciones (en la pila y con los segmentos de Hispa Ventanas), el
  cambio de tarea de Hispa Ventanas no se ha tocado.
- **Acabar.** `ExitProcess` (o el `ret` de la entrada) vuelve al ESP del
  principio de la tarea y esta termina como cualquier otra (`tar_fin`
  destruye sus ventanas). Los selectores (DPMI 0001h) y el bloque (0502h) los
  suelta `pe_limpiar` en el bucle del sistema, también si el programa lo ha
  terminado el Administrador de tareas (que lo lista con el título de su
  cuadro y lo puede terminar a la fuerza).
- **Lo que falta.** Una función que no está tiene un trampolín que, si se
  llama, saca el cuadro «Hispa-486: falta la función NOMBRE (DLL)» y termina
  el programa.
- **Las funciones de la 3a.** USER32: `MessageBoxA` (`dlg_mensaje`: MB_OK,
  MB_OKCANCEL, MB_YESNOCANCEL, MB_YESNO —con un tipo nuevo de cuadro,
  `DM_SI_NO`, que no se puede cerrar sin contestar— y MB_RETRYCANCEL como
  aceptar y cancelar; devuelve los números de Windows; los textos pasan de la
  página 1252 a la 437, tabla de `tools/mk1252.py`; sin título, «Error»).
  KERNEL32: `ExitProcess`, `GetModuleHandleA/W` (NULL), `GetCommandLineA`,
  `GetStartupInfoA`, `GetVersion` (Windows 98), `GetLastError`,
  `SetLastError`, `GetTickCount`, `Sleep` (cede el turno),
  `GetModuleFileNameA`, `lstrlenA`, `lstrcpyA` y `lstrcatA`.
- **Los programas de prueba.** `tests/progs/wingui.c` (WINGUI.EXE y, con
  `-DFALTA`, WINFALTA.EXE), con `gcc -m32` y `tools/elf2pe.py --gui` (la
  opción nueva pone el subsistema 2).

Decisiones y límites de la 3a (para las fases siguientes):

- El cargador de PE de Hispa Ventanas es propio y pequeño, no el de HW32
  sacado a un archivo común como decía el diseño: HW32 lee por un búfer de
  memoria convencional con selectores aparte y aquí todo es un puntero de
  Hispa Ventanas, así que compartirlo era más código que escribirlo. Cuando
  haga falta cargar DLL dentro de Hispa Ventanas (MSVCRT.DLL para los
  programas de C), se verá si se unifican.
- No hay protección: un fallo del programa es un fallo de Hispa Ventanas,
  como con los módulos .HVP. El Administrador de tareas no puede arrebatar el
  control a un programa de Windows que no llame a ninguna función (sí a los
  .HVP).
- Hasta 4 programas de Windows a la vez (`PE_MAX`); cada vez que se abre uno
  es una copia nueva.
- `ExitProcess` desde una llamada de vuelta que corra en otra tarea (3b)
  habrá que resolverlo entonces: hoy vuelve al principio de la tarea propia.
  (Resuelto en la 3b: las llamadas de vuelta solo corren en la tarea propia.)
- El cuadro de `MessageBoxA` no tiene dueña (la 3a no tiene ventanas): si se
  activa otra ventana, queda detrás. (En la 3b ya la tiene.)

### Cómo está hecho (fase 3b)

El «Hola, mundo» de ventana: un programa de Windows registra su clase, crea
su ventana, atiende sus mensajes y pinta. Está en `ventanas/win32u.inc`
(USER32 y GDI32; va detrás de `win32.inc`, en la parte alta, sin tocar la
baja: `VENTANAS.COM` queda en unos 106 KB)
y en `win32.inc` la llamada de vuelta. Prueba:
`tests/run_win32hola_tests.sh` con `tests/progs/winhola.c`.

- **Una ventana de Windows es una ventana de Hispa Ventanas** cuyo
  procedimiento es `w32_ven_proc`. En las ocho palabras de la clase de su
  ficha lleva lo de Windows: el programa (`V_D0`), su `WndProc` (`V_D1`), su
  clase (`V_D2`), su estilo (`V_D3`), un bloque del montón con el título (en
  la página 437, para el marco), `GWL_USERDATA` y los octetos de más de la
  clase (`V_D4`), unas banderas (`V_D5`) y el estilo extendido (`V_D6`). El
  `HWND` es el número de la ficha (100h + 4 × número). El marco, el título,
  los botones, moverla, cambiarle el tamaño, minimizarla y maximizarla son
  los de Hispa Ventanas.
- **Lo de cada programa** (`PE_W32`, un bloque del montón): sus clases (12:
  nombre, `WndProc`, estilo, pincel de fondo, cursor, icono y octetos de
  más; delante, la «clase» de Hispa Ventanas que usa `ven_crear`), sus
  objetos de GDI (96: los 20 primeros son los de `GetStockObject`), sus
  contextos de dispositivo (8) y las funciones de sus temporizadores (8).
  La memoria queda apuntada a su tarea: si el Administrador de tareas lo
  termina a la fuerza, se suelta con ella.
- **La llamada de vuelta** (`pe_llamar`, de Hispa Ventanas al `WndProc`).
  Se está en la pila de Hispa Ventanas de la tarea, dentro de una llamada
  del programa (`pe_despacho` dejó apuntado en `PE_ESPP` dónde estaba la pila
  del programa). `pe_llamar` guarda `PE_ESPV` y `PE_ESPP` en la pila de
  Hispa Ventanas y pone `PE_ESPV` = la pila de ahora: lo que el `WndProc`
  llame entrará por `pe_despacho` de ahí para abajo. En la pila del programa,
  por debajo de `PE_ESPP`, deja los cuatro argumentos y la vuelta (cercana) a
  un trampolín del programa (`PE_VUELTA`); carga FS, ES, GS, SS:ESP y DS del
  programa y salta lejos al `WndProc` con RETF. El `WndProc` vuelve con su
  `ret 16` al trampolín, que empuja su dirección y salta lejos a `pe_volver`:
  segmentos de Hispa Ventanas, ESP = `PE_ESPV`, se reponen `PE_ESPP` y
  `PE_ESPV` y se vuelve con el resultado en EAX. Se anida sin más: el
  `WndProc` llama a `SendMessageA`, que vuelve a llamar al `WndProc`
  (`winhola` lo hace en `WM_CREATE`), o a `MessageBoxA`, cuyo bucle entrega
  `WM_PAINT` a la ventana de debajo.
- **Solo en su tarea.** El `WndProc` solo se llama desde la tarea del
  programa (`w32_en_tarea`: la tarea que corre es la suya y la pila es la de
  esa tarea; el Administrador de tareas, al terminar un programa, se hace
  pasar por él). Hispa Ventanas envía mensajes desde la tarea del sistema
  (`WM_SIZE` y `WM_MOVE` al arrastrar, `WM_SETFOCUS` y `WM_KILLFOCUS` al
  cambiar la activa): esos, `w32_ven_proc` los apunta en la cola de la tarea
  del programa con la marca `W32_DIFERIDO` y se entregan dentro de
  `GetMessageA` (o del bucle de un cuadro). De `WM_SIZE` y `WM_MOVE` basta
  uno en la cola: al entregarlo se mira cómo está la ventana. Así
  `ExitProcess` dentro de un `WndProc` siempre está en su tarea (vuelve al
  principio de la tarea, que acaba) y un `MessageBoxA` dentro de un
  `WndProc` nunca deja parado el bucle del sistema.
- **Los mensajes.** La cola de la tarea lleva los mensajes de Hispa
  Ventanas; `GetMessageA` (`w32_tomar`: donde la tarea cede el turno, con
  `msg_mirar` y `tar_ceder`) los traduce: el ratón (wParam = `MK_*`: los
  botones que lleva apretados y Mayús y Ctrl; el doble clic solo con
  `CS_DBLCLKS`, si no, otro `WM_LBUTTONDOWN`), `WM_SIZE` (wParam = normal,
  minimizada o maximizada), `WM_MOVE`, `WM_TIMER` (lParam = la función),
  `WM_PAINT`, `WM_CLOSE`. Las teclas: un `WM_KEYDOWN` de Hispa Ventanas (las
  que no escriben) o un `WM_CHAR` pasan a ser `WM_KEYDOWN` con su tecla
  virtual, y detrás un `WM_KEYUP` inventado; `TranslateMessage` pone delante
  el `WM_CHAR` (página 1252). Lo inventado, y lo que se mira con
  `PM_NOREMOVE`, espera en una cola de cuatro del programa (`PE_PEND`). Los
  mensajes de `PostMessageA` van a la cola de la tarea con la marca
  `W32_CRUDO` (ya son de Windows). `PostQuitMessage` es una bandera: cuando
  la cola se vacía, `GetMessageA` da `WM_QUIT` y devuelve 0. Lo que llega a
  una ventana de Windows por dentro (un cuadro que reparte, lo diferido) pasa
  por `w32_ven_proc`, que traduce igual y llama al `WndProc` (una tecla: los
  tres mensajes seguidos).
- **[RUEDA] La rueda del ratón.** El `WM_MOUSEWHEEL` de Hispa Ventanas
  (pasos, positivo hacia abajo; el punto en la zona) pasa a ser el de
  Windows (0x020A): wParam alto = el delta, 120 por paso y positivo hacia
  arriba (el signo cambia), wParam bajo = `MK_*` (los botones apretados);
  lParam = el punto en la pantalla. Llega a la ventana que hay debajo del
  cursor (como en Windows 10 con «desplazar las ventanas inactivas»; el
  Windows de la época lo mandaba a la del foco: aquí es lo mismo que hacen
  las ventanas de Hispa Ventanas). Si su `WndProc` lo pasa a
  `DefWindowProcA`, se lo da a la madre si es una ventana de Windows (como
  Windows). Las listas, cuadros de texto y listas desplegables de serie son
  controles de Hispa Ventanas: se desplazan solos (tres líneas por paso).
  `SystemParametersInfoA` solo contesta `SPI_GETWHEELSCROLLLINES` y
  `SPI_GETWHEELSCROLLCHARS`: 3 (lo demás, FALSE). Prueba:
  `tests/run_rueda_tests.sh` con `tests/progs/winrueda.c`.
- **Crear y destruir.** `CreateWindowExA` busca la clase, pide el bloque,
  hace la plantilla (con título: `E_TITULO` y `E_CERRAR`, y `E_MAXIM`,
  `E_MINIM` y `E_TAMANO` según `WS_MAXIMIZEBOX`, `WS_MINIMIZEBOX` y
  `WS_THICKFRAME`; `WS_CHILD`: `E_HIJA`, dentro de la zona de su madre;
  `WS_POPUP` sin título: sin marco; `CW_USEDEFAULT`: en escalera y 5/8 de la
  pantalla) y llama a `ven_crear` (oculta); luego `WM_NCCREATE` y `WM_CREATE`
  con su `CREATESTRUCTA` (en la pila de Hispa Ventanas: el programa la ve
  restando `PE_DELTA`) y, con `WS_VISIBLE`, la muestra. `ShowWindow`:
  `WM_SHOWWINDOW`, `ven_mostrar` (que la activa: `WM_ACTIVATE` y
  `WM_SETFOCUS`) y, la primera vez, `WM_SIZE` y `WM_MOVE`. Cerrar: la X pone
  `WM_CLOSE` en la cola; `DefWindowProcA` llama a `ven_destruir`, que envía
  `WM_DESTROY` (al programa: `WM_DESTROY` y `WM_NCDESTROY`; después se sueltan
  su bloque, sus contextos y sus temporizadores).
- **Pintar.** Hispa Ventanas tiene un solo contexto de dibujo, que no vale
  de un turno a otro, así que un `HDC` es una ficha (ventana, lo que había
  que pintar, colores, modo del fondo, pluma, pincel, dónde está la pluma) y
  cada función de GDI abre el contexto de Hispa Ventanas (`reg_ventana`,
  recortado en `BeginPaint` a lo que había que pintar), dibuja con las `dc_*`
  y lo cierra. `BeginPaint` da la zona por pintada, marca las hijas y envía
  `WM_ERASEBKGND` (`DefWindowProcA` la borra con el pincel de la clase).
  Los colores RGB van al cubo de 6×6×6 o a los grises de la paleta de Hispa
  Ventanas (`w32_color`). La letra es la de Hispa Ventanas (8×16, fija).
- **Las funciones de la 3b.** USER32: `RegisterClassA/ExA`,
  `UnregisterClassA`, `CreateWindowExA`, `ShowWindow`, `UpdateWindow`,
  `DestroyWindow`, `DefWindowProcA`, `GetMessageA`, `PeekMessageA` (si no hay
  nada, deja pasar una vez a los demás), `TranslateMessage`,
  `DispatchMessageA`, `PostQuitMessage`, `PostMessageA`, `SendMessageA`,
  `GetClientRect`, `GetWindowRect`, `InvalidateRect`, `ValidateRect`,
  `MoveWindow`, `SetWindowTextA`, `GetWindowTextA`, `GetWindowTextLengthA`,
  `GetWindowLongA`, `SetWindowLongA`, `SetTimer`, `KillTimer`, `MessageBoxA`
  con dueña, `LoadCursorA`, `LoadIconA`, `SetCursor`, `GetSystemMetrics`,
  `GetSysColor`, `GetSysColorBrush`, `GetDC`, `ReleaseDC`, `BeginPaint`,
  `EndPaint`, `FillRect`, `DrawTextA`, `IsWindow`, `IsWindowVisible`,
  `IsIconic`, `IsZoomed`, `GetParent`, `SetFocus`, `GetFocus`, `MessageBeep` y
  `wsprintfA`. GDI32: `TextOutA`, `SetTextColor`, `SetBkColor`, `SetBkMode`,
  `SetTextAlign`, `GetTextColor`, `GetBkColor`, `GetStockObject`,
  `CreateSolidBrush`, `CreatePen`, `SelectObject`, `DeleteObject`,
  `Rectangle`, `MoveToEx`, `LineTo`, `SetPixel`, `GetTextExtentPoint32A`,
  `GetTextExtentPointA` y `GetTextMetricsA`.

Decisiones y límites de la 3b (para las fases siguientes):

- Las clases de serie (BUTTON, EDIT, STATIC, LISTBOX), los menús y los
  recursos son la 3c y la 3d: `CreateWindowExA` solo conoce las clases que
  registra el programa.
- Un `HWND` solo vale para las ventanas del propio programa; no hay
  `FindWindowA` ni mensajes entre programas.
- Sin ventana no hay temporizador (`SetTimer` con NULL devuelve 0), y son los
  16 de Hispa Ventanas para todos.
- `GetMessageA` y `PeekMessageA` no filtran por ventana ni por mensaje.
- De una tecla que escribe solo se sabe el carácter: la tecla virtual sale
  bien para las letras, las cifras y el espacio (los signos, 0); no hay
  `WM_SYSKEYDOWN` (Alt+letra es de los menús de Hispa Ventanas) ni
  `GetKeyState`.
- Plumas de un punto y lisas; pinceles lisos; una sola letra (`CreateFontA`
  falta). Las coordenadas son las de la zona, sin transformaciones.
- El cursor y el icono de la clase no se usan (flecha e icono de Hispa
  Ventanas).
- Al destruir una ventana, Hispa Ventanas destruye primero las hijas
  (Windows avisa primero a la madre).
- Si a una ventana que tiene abierto un `MessageBoxA` le llega `WM_CLOSE`
  (se lo pide el Administrador de tareas), el `WndProc` la puede destruir con
  el cuadro aún abierto: el cuadro sigue hasta que se contesta (en Windows se
  iría con su dueña).
- KERNEL32 dentro de Hispa Ventanas sigue siendo el mínimo de la 3a: sin
  montón (`HeapAlloc`, `GlobalAlloc`) ni archivos, y sin cargar DLL
  (MSVCRT.DLL): los programas de C con biblioteca aún no van.

### Cómo está hecho (fase 3c)

Un programa de ventanas hecho en C con MSVCRT.DLL, con sus controles, su
menú, sus recursos y sus diálogos: la calculadora de la prueba. Son cuatro
partes, cada una con su prueba.

**1. KERNEL32 y las DLL: HW32 es el módulo del programa.** En vez de escribir
otro KERNEL32 para Hispa Ventanas, se usa el de los programas de consola
entero: `HW32.COM` lleva en 103h la firma «HW32MOD» y una tabla
(`mod_cargar`, `mod_fin`, `mod_d`), y Hispa Ventanas lo carga **como módulo
de cada programa de Windows** (`pe_modulo`, en `ventanas/win32.inc`):

- Lee `HW32.COM` (de al lado de `VENTANAS.COM`, de al lado del programa o de
  la carpeta de ahora) a un bloque de memoria extendida, con 4 KB detrás para
  su pila; le hace dos selectores de 32 bits (código y datos, con la base en
  «copia − 100h»: HW32 corre tal cual, con su `ORG 100h`); rellena `mod_d`
  (sus selectores, el entorno, el PSP, la ficha del programa y tres entradas
  lejanas de Hispa Ventanas) y llama lejos a `mod_cargar`. Sin `HW32.COM` el
  programa no se abre y se dice por qué.
- `mod_cargar` hace lo que hace HW32 al arrancar (el búfer de DOS —16 KB de
  memoria convencional, con DPMI 0100h—, los selectores de la BIOS y del
  entorno) y carga el programa **con el cargador de siempre**: el bloque, los
  selectores del programa, las secciones, los reubicados, las importaciones,
  las DLL (de la carpeta del programa, de la de HW32 o de la de ahora) y el
  trampolín de inicio con sus `DllMain`. Mientras carga usa una pila suya (el
  cargador cuenta con que la pila y los datos son el mismo segmento). Vuelve
  con los selectores del programa, su base, su ImageBase, su entrada y su
  pila; si no puede, con el porqué («falta la DLL...»), que sale en un aviso.
- Las llamadas del programa a KERNEL32 (y las de MSVCRT.DLL) van, como en
  HW32, a un trampolín que salta lejos al `despacho` **de su copia de HW32**:
  se atienden en la pila del programa, con DS = HW32, ES = el programa y
  GS = el búfer de DOS, sin pasar por Hispa Ventanas (y sin ceder el turno).
  Cada programa tiene su copia: su montón, sus handles, su TLS, sus búsquedas
  y su último error son suyos.
- Lo del entorno se lo pide HW32 a Hispa Ventanas por `md_resolver`
  (`pe_resolver`): al importar, las funciones de USER32 y GDI32 (cada una, su
  trampolín a `pe_despacho`; la que no hay, con su «falta») y las pocas de
  KERNEL32 que se hacen aquí en vez de allí (`Sleep`, que cede el turno, y
  `FindResourceA`, `LoadResource`, `LockResource` y `SizeofResource`);
  `GetModuleHandleA("USER32")` y `GetProcAddress` también preguntan. Los
  trampolines de Hispa Ventanas están ahora en un bloque del montón.
- Dentro de Hispa Ventanas no hay consola: en modo módulo, `int_rm` no
  escribe en la pantalla de texto ni lee del teclado por la BIOS ni por los
  handles 0 a 2 (lo escrito se da por escrito; `printf` no rompe nada), no
  ejecuta otros programas (`CreateProcessA` falla), apunta los archivos que se
  abren y, tras una búsqueda, devuelve la DTA a la de Hispa Ventanas.
  `ExitProcess` (y `exit` de MSVCRT) salta lejos a `pe_salir_mod`; una
  función que falta, `FatalAppExitA` o una excepción sin atender, a
  `pe_falta_mod`, que lo dice en un cuadro y termina el programa.
- Al acabar (o si lo termina el Administrador de tareas), `pe_limpiar` llama
  a `mod_fin`: HW32 cierra los archivos que quedaron abiertos y suelta todos
  los bloques de DPMI que pidió (el programa, sus DLL, el montón,
  `VirtualAlloc`: los lleva apuntados en `blq_tab`), el búfer de DOS y los
  selectores; Hispa Ventanas suelta luego su copia de HW32.
- Prueba: `tests/run_win32crt_tests.sh` (WINCRT.EXE: el montón,
  `VirtualAlloc`, archivos y búsquedas, la hora, el entorno, TLS,
  `GetProcAddress`, MSVCRT con coma flotante y archivos con búfer, una DLL de
  al lado del programa y otra de la carpeta de HW32; no queda memoria ni al
  salir con `exit`; la DLL que falta; sin `HW32.COM`). `tests/run_hw32_tests.sh`
  sigue pasando: HW32 de consola es el mismo archivo.

**2. Los controles de serie** (`ventanas/win32c.inc`). Un control de Windows
**es** un control de Hispa Ventanas (`c_boton`, `c_edit`, `c_estatico`,
`c_lista`), hijo de la ventana de Windows; el COMBOBOX sencillo es una clase
de aquí (`c_w32combo`) que abre un menú emergente (`ctx_menu`).

- `CreateWindowExA` con BUTTON, EDIT, STATIC, LISTBOX o COMBOBOX (si el
  programa no ha registrado una clase con ese nombre) hace el control con
  `ven_crear` y le cambia el procedimiento por `w32_ctl_proc`, que llama al
  de su clase. `V_D0` a `V_D7` son de su clase de Hispa Ventanas, así que un
  control no lleva dentro a su programa: es el de su tarea (`w32_ven` lo
  admite como HWND por eso; `w32_ctl_prog`). Su texto (página 437) está en un
  bloque del montón (`V_TEXTO`).
- El ratón y el teclado los atiende el control (el foco, Tab entre los que
  tienen `WS_TABSTOP`, el espacio y el Intro en los botones): el programa no
  ve esos mensajes, ve el aviso. El control avisa a su madre con `WM_COMMAND`
  y `w32_traducir` pone en `lParam` su HWND: `BN_CLICKED`, `LBN_SELCHANGE`,
  `LBN_DBLCLK`, `CBN_SELCHANGE` y `EN_CHANGE` (`w32_ctl_proc` mira si el
  cuadro de texto ha cambiado con cada mensaje).
- Lo que el control no quiere (una tecla, un carácter) **sube a su madre**,
  como en Hispa Ventanas: la ventana recibe `WM_KEYDOWN` y `WM_CHAR` aunque el
  foco esté en un botón (la calculadora se maneja así con el teclado).
- Los mensajes de Windows a un control (`SendMessageA`, `SetWindowTextA`,
  `GetWindowTextA`...) pasan por `w32_ctl_enviar`: `WM_SETTEXT`, `WM_GETTEXT`,
  `WM_GETTEXTLENGTH`; `BM_GETCHECK`, `BM_SETCHECK`, `BM_CLICK`; `EM_SETSEL`,
  `EM_GETMODIFY`, `EM_SETMODIFY`, `EM_GETLINECOUNT`, `EM_REPLACESEL`, cortar,
  copiar y pegar; `LB_ADDSTRING`, `LB_RESETCONTENT`, `LB_SETCURSEL`,
  `LB_GETCURSEL`, `LB_GETTEXT`, `LB_GETTEXTLEN`, `LB_GETCOUNT`; y los `CB_*`
  equivalentes. Los textos van de la página 1252 a la 437 y vuelven; en los
  cuadros de varias líneas, CR LF <-> LF.
- USER32: `GetDlgItem`, `GetDlgCtrlID`, `SendDlgItemMessageA`,
  `SetDlgItemTextA`, `GetDlgItemTextA`, `SetDlgItemInt`, `GetDlgItemInt`,
  `CheckDlgButton`, `IsDlgButtonChecked`, `CheckRadioButton`, `EnableWindow`,
  `IsWindowEnabled`, `SetWindowPos`, `IsDialogMessageA` (devuelve 0: el Tab ya
  lo lleva Hispa Ventanas); `SetFocus` y `GetFocus` valen con controles.
- Prueba: `tests/run_win32ctl_tests.sh` (WINCTL.EXE, con el ratón y el
  teclado).

**3. Los menús** (`ventanas/win32m.inc`). Un HMENU es una ficha del programa
(8 menús de 16 órdenes, con el texto ya en la página 437). `SetMenu` (o el
menú que se da a `CreateWindowExA`, o el de la clase: `lpszMenuName`) hace con
ellas la tabla de la barra de menús de Hispa Ventanas (`V_MENU`), y se rehace
cada vez que el programa cambia algo. La barra, los desplegables, Alt+letra y
las flechas son los de Hispa Ventanas; al elegir llega `WM_COMMAND`. La marca
de `CheckMenuItem` es un «*» delante del texto; las órdenes en gris se le
dicen al desplegable al abrirse (`w32_menu_abierto`, desde `menu_abrir`, y
`ctx_es_gris`). Al ganar o perder la barra, las hijas se quedan donde estaban
dentro de su zona y la ventana recibe `WM_SIZE`. USER32: `CreateMenu`,
`CreatePopupMenu`, `AppendMenuA`, `DestroyMenu`, `SetMenu`, `GetMenu`,
`GetSubMenu`, `DrawMenuBar`, `CheckMenuItem`, `EnableMenuItem`,
`GetMenuState`, `GetMenuItemCount`.

**4. Los recursos y los diálogos** (`ventanas/win32m.inc`). `w32_recurso`
busca en la sección `.rsrc` del programa (tipo, nombre o número, el primer
idioma). `LoadStringA`, `LoadMenuA`, `LoadIconA` y `LoadCursorA` (del
programa: se sabe si están; el icono y el cursor que se ven siguen siendo los
de Hispa Ventanas) y `FindResourceA` salen de ahí; los textos, de UTF-16 a la
página 1252.

- Un diálogo de plantilla es una ventana de Windows de la clase «#32770» (la
  0 de cada programa) con la marca `W32V_DIALOGO`: su «WndProc» es el DLGPROC
  y, si este devuelve 0, `w32_proc` hace lo que haría `DefDlgProc`
  (`w32_dlg_defecto`: el fondo, `WM_CLOSE` y Esc = IDCANCEL, Intro = el botón
  por defecto o IDOK). Sus controles salen de la plantilla, con las medidas
  en unidades de diálogo (con la letra de 8×16, dos puntos cada una); sale
  centrado. `WM_INITDIALOG` lleva en `wParam` el primer control donde para el
  tabulador y, si el DLGPROC devuelve distinto de 0, ese control toma el foco.
- `DialogBoxParamA` se queda en un bucle de mensajes propio, con la dueña
  desactivada, hasta `EndDialog`; `CreateDialogParamA` vuelve enseguida y el
  diálogo convive con las demás ventanas del programa.
- `tools/elf2pe.py --res recursos.rc` mete la sección `.rsrc`: la hace
  `tools/rcmini.py` de un texto sencillo parecido a un `.rc` (STRING, ICON,
  MENU con POPUP e ITEM, DIALOG con LTEXT, EDITTEXT, PUSHBUTTON...). Además,
  `-l UNA.DLL` importa lo que esa DLL exporte y `-t ventanas/win32.inc`, lo
  que haya en la tabla de USER32 y GDI32: ya no hace falta escribir las
  listas de importaciones a mano.
- Pruebas: `tests/run_win32menu_tests.sh` (WINMENU.EXE: los menús por
  programa y de los recursos, los textos, el diálogo modal y el que no lo es)
  y la final, `tests/run_win32calc_tests.sh` (WINCALC.EXE: la calculadora, con
  el ratón y con el teclado).

Con todo, `VENTANAS.COM` queda en unos 113 KB: lo nuevo (unos 8 KB de código
de 32 bits; el cargador y el KERNEL32 mínimo de la 3a se han ido) va en la
parte alta, y de la baja solo se ha tocado una llamada en `menu_abrir`.

Decisiones y límites de la 3c (para las fases siguientes):

- Hace falta `HW32.COM` (y `MSVCRT.DLL` para los programas de C) en el disco:
  al lado de `VENTANAS.COM`, del programa o en la carpeta de trabajo. Cada
  programa de Windows abierto gasta, además de lo suyo, unos 36 KB de memoria
  extendida (su copia de HW32), 16 KB de memoria convencional (su búfer de
  DOS), 1 MB de montón en cuanto pide memoria y la pila que diga su cabecera
  (hasta 1 MB).
- Los límites de KERNEL32 son los de HW32: `LoadLibraryA` solo da las DLL que
  el programa ya importa; no hay hilos ni `CreateProcessA` (dentro de Hispa
  Ventanas, ni por COMMAND.COM); los nombres largos, por su recorte 8+3.
  `Sleep` importado cede el turno; el de `GetProcAddress` también (lo da
  Hispa Ventanas). Las demás funciones de KERNEL32 no ceden el turno: un
  programa que lea un archivo grande para a los demás mientras tanto.
- El programa comparte con Hispa Ventanas la tabla de archivos de DOS
  (FILES) y la carpeta de trabajo: si la cambia, la cambia para todos.
- Los controles son los de Hispa Ventanas, con su aspecto: los estilos de
  Windows que no tienen equivalente se pasan por alto (`ES_READONLY`,
  `ES_PASSWORD`, `SS_CENTER`, las listas con varias columnas o varias
  elegidas, `LB_DELETESTRING`, el COMBOBOX con caja de texto). Un control
  desactivado no atiende, pero se pinta igual. No se puede cambiar el
  procedimiento de un control (`GWL_WNDPROC`: 0), ni hay `WM_CTLCOLOR*` ni
  `WM_SETFONT`. Una lista lleva hasta 200 líneas de 39 letras; un COMBOBOX,
  24; un cuadro de texto de una línea, 255 letras.
- El programa no ve los mensajes de ratón y de teclado de sus controles
  (`GetMessageA` no los da; los reparte Hispa Ventanas), así que tampoco hay
  aceleradores (`TranslateAcceleratorA` falta). En cambio recibe en la
  ventana madre las teclas que el control no quiere, cosa que Windows no hace.
- Menús: un nivel de desplegables (uno dentro de otro sale como una orden que
  no hace nada); 8 menús de 16 órdenes de 31 letras por programa; 32 órdenes
  por desplegable para el gris. Faltan `InsertMenuA`, `ModifyMenuA`,
  `TrackPopupMenu` y los mensajes `WM_INITMENU*`.
- Recursos: solo los del `.EXE` (no los de una DLL); las plantillas de
  diálogo extendidas (DLGTEMPLATEEX) no se cargan; los iconos, los cursores y
  los mapas de bits de los recursos no se dibujan (`LoadBitmapA` falta); no
  hay tablas de aceleradores.
- Diálogos: la letra es la de Hispa Ventanas (DS_SETFONT se salta); el fondo,
  el de los cuadros de Hispa Ventanas; `DialogBoxIndirectParamA` y
  `DWL_MSGRESULT` faltan; `DS_CENTER` o no, salen centrados en la pantalla.

### Cómo está hecho (fase 3d)

Lo que piden los programas sencillos de Windows 95/98 además de lo de la 3c:
el GDI de verdad, los iconos y los cursores, los aceleradores, los diálogos
comunes y los controles comunes. La prueba con un programa de verdad es
**FASMW 1.73.32** (el entorno de ventanas del flat assembler, licencia de
fasm, de tipo BSD; lo baja `tests/thirdparty/fetch_win32.sh`): dentro de
Hispa Ventanas abre un archivo con su diálogo, lo enseña con colores en su
editor propio, se escribe en él y lo ensambla en «otro hilo» con su barra de
progreso; el `.COM` que sale funciona en Hispa-DOS. De paso, **FASM.EXE**
(el de consola) también ensambla con HW32.

Todo va en la parte alta de `VENTANAS.COM` (de la baja solo se han tocado
unas decenas de octetos de `dlg_archivo`: el patrón de lo que lista y las
carpetas primero):
`ventanas/win32g.inc` (contextos y figuras), `win32t.inc` (letras y texto),
`win32b.inc` (mapas de bits y DIB), `win32r.inc` (regiones y recortes),
`win32ga.inc` (las funciones de GDI32) y `win32x.inc` (USER32 de más,
COMDLG32, ADVAPI32 y SHELL32); sus variables, en `bss4.inc`.
`VENTANAS.COM` pasa de 122 a unos 162 KB (y unos 30 KB más de variables);
para la parte baja quedan 1,2 KB libres.

**El GDI** (`win32g.inc` y los demás):

- **Abrir un contexto** (`wg_abrir`) deja en la región de Hispa Ventanas
  (`reg`) los rectángulos donde se puede pintar, ya en la superficie (la zona
  de la ventana, lo que había que pintar en `BeginPaint`, sin las barras de
  desplazamiento y cortado con el recorte del contexto), y en `wg_ox`,
  `wg_oy` lo que se suma a las coordenadas lógicas (el origen de la zona, el
  de la vista y el de la ventana: `SetViewportOrgEx`, `SetWindowOrgEx`).
  Las primitivas (`wg_punto`, `wg_tramo`, `wg_linea`, `wg_blt`) escriben en
  la superficie recortando con esa región y con la ROP2 del contexto.
- **Los contextos de memoria** (`CreateCompatibleDC`): al abrirlos, la
  superficie de Hispa Ventanas pasa a ser su mapa de bits y al cerrar vuelve
  a ser la pantalla. Un mapa (HBITMAP) es un octeto por punto con los colores
  de la paleta de Hispa Ventanas (como la pantalla), o 0 y 1 si es
  monocromo; los de más de 48 KB, en su bloque de DPMI (apuntado en el
  programa: se suelta al acabar).
- **Los colores y las ROP:** COLORREF -> el gris o el color del cubo de
  6×6×6 más cercano (por tablas; también `PALETTEINDEX` con la paleta del
  contexto; con la de serie, `PALETTEINDEX(n)` es el color n de la paleta de
  Hispa Ventanas, como lo usa el Navegador; y un RGB que es justo uno de los
  16 colores propios de Hispa Ventanas, crema, arena, terracota..., se
  queda en ese color y no en el más parecido del cubo). Las ROP2 (`SetROP2`) y las ROP3 de `BitBlt` se hacen con los
  colores RGB de los puntos, como en una pantalla de color verdadero, y el
  resultado vuelve a la paleta: así funcionan las máscaras (`SRCAND` y
  `SRCPAINT` con un mapa monocromo) y el XOR. De monocromo a color, 0 es el
  color del texto y 1 el del fondo; de color a monocromo, 1 es lo del color
  de fondo del origen.
- **Plumas y pinceles:** plumas lisas, de rayas, de puntos y sus mezclas (de
  un punto; las anchas, lisas, como en Windows 95), de cualquier ancho;
  pinceles lisos, los seis rayados (`CreateHatchBrush`; el hueco, del color
  de fondo si es opaco) y de dibujo (`CreatePatternBrush`,
  `CreateDIBPatternBrushPt`, 8×8).
- **Las figuras** (Rectangle, Ellipse, RoundRect, Arc, Pie, Chord) se hacen
  por filas: cada fila es un tramo que se rellena con el pincel, y su borde
  (los puntos que no tienen sus cuatro vecinos dentro) se pinta con la
  pluma. Las elipses salen de una raíz cuadrada entera de 64 bits; los
  quesos y las cuerdas, filtrando por el sector (productos cruzados) o por el
  lado de la cuerda. Polygon y PolyPolygon, por filas con sus cortes
  (ALTERNATE y WINDING); Polyline, PolylineTo, PolyPolyline y PolyBezier (16
  trozos por curva).
- **Las letras:** una letra de Windows (`CreateFontA`) es la de Hispa
  Ventanas (8×16, página 437) estirada a su tamaño: alto de la celda (de
  lfHeight; negativo, el de las letras: × 16/13; de 14 a 18, la de siempre,
  que se ve mejor; como poco 12) y ancho (la mitad, si no se da), con
  negrita, cursiva, subrayado y tachado; todas son de paso fijo. TextOut,
  ExtTextOut (opaco, recortado, con avances), TabbedTextOut, las W (de
  UTF-16 a la página 1252 y de ahí a la 437), GetTextMetrics,
  GetTextExtentPoint32 y GetTextExtentExPoint, GetCharWidth. DrawText parte
  las palabras (`DT_WORDBREAK`), expande los tabuladores, subraya la letra
  de «&», centra, pone «...» si no cabe y calcula el rectángulo.
- **Los DIB** de 1, 4, 8, 16 (5-5-5 y 5-6-5), 24 y 32 bits se pasan a la
  paleta fila a fila al dibujarlos (`SetDIBitsToDevice`, `StretchDIBits`, de
  abajo arriba o al revés). Con `CreateDIBSection` el programa escribe sus
  puntos directamente; al copiarlos se leen de ahí, y al pintar en ellos con
  GDI se pasan al mapa y vuelven (solo los puntos que han cambiado, para no
  perder colores). `GetDIBits`, `SetDIBits`, `CreateDIBitmap` y
  `LoadBitmapA` (de los recursos).
- **BitBlt y StretchBlt** con cualquier ROP3 (estirando con el punto más
  cercano, y al revés si el ancho o el alto es negativo); de una ventana se
  copia antes lo que hay en la pantalla. PatBlt, GetPixel, SetPixel.
- **Regiones:** listas de hasta 64 rectángulos que no se pisan
  (`CreateRectRgn`; las elípticas y las de esquinas redondas, por franjas;
  las de polígono, su caja); `CombineRgn` (y, o, xor, menos), `OffsetRgn`,
  `PtInRegion`, `FillRgn`, `InvalidateRgn`... El recorte de un contexto
  (`SelectClipRgn`, `ExtSelectClipRgn`, `IntersectClipRect`,
  `ExcludeClipRect`, `GetClipBox`) es una región suya.
- **Lo demás:** `SaveDC` y `RestoreDC`, `GetDeviceCaps` (8 bits con paleta,
  96 puntos por pulgada), las paletas (`CreatePalette`, `SelectPalette`,
  `RealizePalette`: el color más cercano, sin cambiar la de la pantalla),
  `GetObjectA` de todo, `GetCurrentObject`, `GetObjectType`, y de USER32
  `FillRect`, `FrameRect`, `InvertRect`, `DrawFocusRect` y `DrawEdge`.

**USER32** (`win32x.inc`):

- **Barras de desplazamiento:** una ventana con `WS_VSCROLL` o `WS_HSCROLL`
  lleva barras `c_barra` de Hispa Ventanas como hijas, en el borde de su
  zona, y para el programa su zona es lo que queda (`w32_cli`). Lo que se
  aprieta (flechas, pista, pulgar, rueda) llega como `WM_VSCROLL` o
  `WM_HSCROLL` con su `SB_*`; `SetScrollInfo`, `GetScrollInfo`,
  `SetScrollPos`, `SetScrollRange`... Se ven siempre (como con
  `SIF_DISABLENOSCROLL`): así la zona no cambia al cambiar el recorrido.
- **El caret:** uno por programa, invertido, sin parpadear (las capturas de
  las pruebas salen siempre igual); se quita mientras se pinta la ventana.
- **El teclado:** `GetKeyState` (cómo estaban Mayús., Ctrl y Alt con el
  último mensaje del teclado), `GetAsyncKeyState`, `GetKeyboardState`,
  `ToAscii` (el carácter de la última tecla: FASMW escribe así), `VkKeyScanA`,
  `MapVirtualKeyA`; F1 a F12 con Mayús., Ctrl o Alt ya llegan como teclas
  virtuales.
- **Los aceleradores:** `LoadAcceleratorsA` (de los recursos),
  `CreateAcceleratorTableA`, `TranslateAcceleratorA` (teclas virtuales con
  sus mayúsculas, o caracteres) -> `WM_COMMAND`.
- **Iconos y cursores:** de los recursos (el mejor de 32 de ancho, con su
  máscara, a 32×32) o los de serie (de Hispa Ventanas). `DrawIcon`,
  `DrawIconEx`, `LoadImageA`. El icono de la clase (o `WM_SETICON`,
  `SetClassLongA`) va a su botón de la barra de tareas de Hispa-486, a 16
  colores. El cursor de la clase se pone sobre la zona; `SetCursor` con uno
  de serie (flecha, escribir, esperar, cruz, flechas dobles: formas nuevas
  de `tamano.inc`) o con uno de los recursos (a la mitad, en la forma
  `CUR_PROG`).
- **El portapapeles** es el de Hispa Ventanas (texto: `CF_TEXT` y
  `CF_OEMTEXT`); `GetClipboardData` da un bloque que `GlobalLock` y
  `GlobalSize` entienden.
- **Menús:** las plantillas MENUEX (FASMW las usa); `TrackPopupMenu` con el
  menú emergente de Hispa Ventanas (con `TPM_RETURNCMD`, espera atendiendo
  los mensajes de la tarea); `CheckMenuRadioItem`.
- **Lo demás:** rectángulos (`SetRect`, `IntersectRect`, `UnionRect`...),
  coordenadas (`ClientToScreen`, `MapWindowPoints`, `AdjustWindowRect`...),
  `SetCapture`, `GetWindowPlacement`, `FindWindowA` (en el programa),
  `SystemParametersInfoA`, `GetClassNameA`, `GetWindow`, `EnumChildWindows`,
  `CharUpperA` y compañía, `wvsprintfA`, `MessageBoxIndirectA`...
  `ScrollWindow` repinta toda la zona. Los textos fijos (STATIC) quitan los
  «&».

**COMDLG32, ADVAPI32 y SHELL32** (en `win32x.inc`, como USER32: DLL 3, 4 y
5 de la tabla). `GetOpenFileNameA` y `GetSaveFileNameA` son el diálogo de
archivos de Hispa Ventanas: el patrón del filtro elegido, primero todas las
carpetas, la carpeta de empezar, la extensión de serie, «¿Quiere
reemplazarlo?» y «No se encuentra ese archivo»; devuelve la ruta entera
(también si se escribe una ruta con su unidad o desde `\`), sin pasar de
`nMaxFile`. La carpeta de trabajo, que en Hispa Ventanas es de todos los
programas, queda siempre como estaba (como si llevara `OFN_NOCHANGEDIR`).
`ChooseColorA` (los 16 colores de siempre) y `ChooseFontA` (el tamaño,
negrita y cursiva) son diálogos con una lista. El registro no hay (dice que
no está) y SHELL32 tiene lo justo (`ShellAboutA`, arrastrar archivos: nada).

**COMCTL32.DLL** (`w32/comctl32.c`, en C como MSVCRT.DLL): la barra de
estado (partes, textos, tabuladores para centrar), las pestañas (arriba o
abajo, con sus octetos de más, imágenes y `TCN_SELCHANGE`), la barra de
progreso y las listas de imágenes. `InitCommonControls` lleva el número 17
(`tools/elf2pe.py --ordinal`): muchos programas la importan así. Va con
MSVCRT.DLL, al lado de HW32.COM.

**HW32** (`utils/hw32hilo.inc`): hay un solo hilo, así que `CreateThread`
ejecuta el hilo entero antes de volver (vale para los programas que lanzan
un trabajo y esperan a que acabe, como el ensamblador de FASMW);
`ExitThread` dentro de él vuelve a `CreateThread`; `WaitForSingleObject`,
las exclusiones y los eventos están siempre libres. Los `.INI`
(`GetPrivateProfileStringA`, `GetPrivateProfileIntA` y
`WritePrivateProfileStringA`, con listas de secciones y claves y borrar), y
`MulDiv`. Y un arreglo de la fase 1: el tamaño de las cabeceras se leía de
`SizeOfHeapCommit` en vez de `SizeOfHeaders` (los PE de FASM, que lo tienen
a 0, no cargaban).

**UCRT.DLL** (`w32/ucrt.c` con `w32/msvcrt.c`): el CRT «universal» de los
programas de Visual C++ 2015 y siguientes. HW32 lleva a ella todas las
`api-ms-win-crt-*.dll`, `VCRUNTIME140.DLL` y `UCRTBASE.DLL`
(`alias_ucrt`), y KERNEL32 tiene lo que pide su arranque
(`GetStartupInfoW`, `InitializeSListHead`, `IsProcessorFeaturePresent`,
`IsDebuggerPresent`, `MoveFileExA`, `FormatMessageA`). Está empezada: el
buscaminas («Mines») de los rompecabezas de Simon Tatham (MIT) ya carga con
ella, pero se cierra al empezar sin decir nada; queda averiguar dónde (haría
falta la traza de HW32 dentro de Hispa Ventanas).

**Herramientas:** `tools/rcmini.py` hace también ACCELERATORS, CURSOR y
BITMAP; `tools/elf2pe.py`, `--ordinal NOMBRE=N` y las importaciones de
COMDLG32, ADVAPI32 y SHELL32 de la tabla.

**Pruebas** (en `make test`):

- `tests/run_win32gdi_tests.sh` (WINGDI.EXE, `tests/progs/wingdi.c`): una
  cuadrícula con todas las figuras, plumas, rayados, ROP2, bordes, letras,
  DrawText, mapas en memoria, máscaras, StretchDIBits, CreateDIBSection y un
  recorte elíptico con agujero, mirada punto a punto, y 12 comprobaciones de
  valores (GetTextMetrics, CombineRgn, GetDIBits, las paletas...).
- `tests/run_win32var_tests.sh` (WINVAR.EXE, `winvar.c` y `winvar.rc`, con
  sus dibujos de `mkwinvar.py`), en el escritorio de Hispa-486: el icono en
  la barra de tareas y dibujado, el cursor, la barra de desplazamiento, el
  caret, el portapapeles, los aceleradores (F5, Mayús.+F5, Ctrl+letra, un
  carácter de control), TrackPopupMenu, abrir, color y letra.
- `tests/run_win32fasmw_tests.sh`: FASMW (si se ha bajado): su menú
  MENUEX, sus pestañas y su barra de estado, abrir `HOLA486.ASM`, escribir en
  su editor, ensamblar con Ctrl+F9 («2 passes, 29 bytes.») y, en el disco,
  el `.COM` y su `FASMW.INI`.
- Las pruebas de Win32 que usan el disquete de siempre quitan
  `HISPA.WAV` de su copia para tener sitio (HW32, MSVCRT y los programas).

Decisiones y límites de la 3d (para las fases siguientes):

- Una sola letra (la de Hispa Ventanas) a cualquier tamaño, de paso fijo:
  los nombres de las letras no cambian nada y no hay `EnumFontFamilies`; las
  letras pequeñas (menos de 12 puntos de alto) no se leen y se agrandan; no
  hay texto girado.
- Los colores son los 256 de la paleta de Hispa Ventanas: una paleta del
  programa no cambia la de la pantalla (se usa el color más cercano), y lo
  pintado con GDI en un DIB de 8 bits toma el más cercano de los suyos.
- Las regiones son rectángulos: las elípticas, por franjas (64); si una
  operación da más de 64, los últimos se juntan en su caja.
- No se imprime (`PrintDlgA` dice que no; `StartDocA` falta).
- `ScrollWindow` repinta todo en vez de copiar; el caret no parpadea.
- Un hilo de verdad no hay: un programa que deja un hilo en marcha esperando
  algo (un servidor, un temporizador propio) no va.
- COMCTL32 tiene solo la barra de estado, las pestañas, el progreso y las
  listas de imágenes (ni barras de herramientas, ni listas de columnas, ni
  árboles).
- `VENTANAS.COM` ha crecido unos 40 KB: en el disquete de Hispa-386 queda
  poco sitio (las pruebas quitan `HISPA.WAV`). La imagen de Hispa-486 (y su
  disquete «Hispa-486») lleva COMCTL32.DLL y UCRT.DLL en `\HISPADOS`, al
  lado de MSVCRT.DLL (la tabla `DLL486` de `tools/mkusb.py`).

## Red inalámbrica (Wi-Fi)

Decisión de Carlos (17/11/2026): el Wi-Fi no se hace en Hispa-386; es de
Hispa-486 en adelante. Hace falta el firmware de cada tarjeta (las de los
portátiles de prueba: Broadcom BCM43224 14E4:4353 del MacBook Air, Realtek
RTL8822BE), la gestión 802.11 (buscar redes, asociarse) y WPA2 (AES y el acuerdo
de claves de cuatro pasos). Irá detrás de la pila de red de Hispa-486 (Winsock),
con una ventana de Hispa Ventanas para elegir la red.

## Relación con los demás

- La familia: **Hispa-8086** (8086 a 286, gama MS-DOS de los 80), **Hispa-386**
  (386+, Hispa Ventanas, gama MS-DOS 6.22 + Windows 3.1), **Hispa-486** (gama
  Windows 95/98) e **Hispa-Pentium** (gama Windows XP: núcleo propio sin DOS,
  de 32 bits con memoria virtual). Hispa-486 lleva todo lo de Hispa-386, y lo
  que se haga en Win32 para Hispa-486 (KERNEL32, USER32, GDI32, DirectX) es la
  base de los programas de Hispa-Pentium.
- **Castel**: cuando esté maduro, las piezas nuevas de Hispa-486 se podrán
  escribir en él (el cargador y las DLL empiezan en ensamblador, como todo
  Hispa-DOS).
