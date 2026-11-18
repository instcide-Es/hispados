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
  2000, es el sistema siguiente: **Hispa-Pentium**, la gama de Windows XP (y se
  escribirá en [Castel](../../castel/)).

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
| 3 | USER32/GDI32 sobre Hispa Ventanas: ventanas, mensajes, controles, menús, diálogos, dibujo | un programa Win32 de ventanas sencillo (el «Hola, mundo» de ventana). **3a hecha (17/11/2026)**: un Win32 de ventanas corre dentro de Hispa Ventanas y su `MessageBoxA` es un cuadro de Hispa Ventanas. **3b hecha (17/11/2026)**: el «Hola, mundo» de ventana (clase, ventana, bucle de mensajes, `WndProc`, `WM_PAINT` con texto y dibujos). **3c hecha (17/11/2026)**: KERNEL32 entero y carga de DLL dentro de Hispa Ventanas (MSVCRT.DLL), los controles de serie, los menús, los recursos y los diálogos de plantilla: una calculadora de Windows en C que se maneja con el ratón y el teclado |
| 4 | El escritorio de Hispa-486: menú de inicio, barra de tareas, escritorio con iconos, «Mi PC» (Hispa Ventanas 2) | arranca a su escritorio. **4a hecha (18/11/2026)**: la barra de tareas y el menú Inicio (`VENTANAS /486` o `Escritorio=486` en VENTANAS.INI); el escritorio empieza despejado, con todos los programas en el menú. Falta: los iconos del escritorio y «Mi PC» |
| 5 | DirectX hasta el 7 (el del 98): DirectDraw (pantalla completa y doble búfer), DirectSound (sobre HSPSND), DirectInput (teclado, ratón, mandos); Direct3D por programa (sin aceleradora), luego con aceleradoras de la época | un juego DirectX sencillo; después, juegos 2D del 98 |
| 6 | Navegador web (HTML 3.2 y CSS sencillo, imágenes; HTTP y, con la red cifrada, HTTPS) | hispados.es en el navegador. **6a hecha (18/11/2026)**, adelantada por un pedido de Carlos («ya lo podemos usar para la biblio»): la red de los programas Hispa32 (`WSOCK32.DLL`: TCP/IP y Winsock 1.1), HTTPS (`HSPTLS.DLL`: TLS 1.2 con certificados) y **el Navegador** (`NAVEGA.EXE`), con el que se usa la biblioteca de ClicGest: [NAVEGADOR.md](NAVEGADOR.md). Falta: imágenes, hojas de estilo de fuera |
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
3. HW32 y MSVCRT.DLL en el sistema instalado (Hispa-386 0.9.23).
4. Fase 3: USER32 y GDI32 sobre Hispa Ventanas. La 3a, la 3b (el «Hola,
   mundo» de ventana) y la 3c (KERNEL32 y las DLL dentro de Hispa Ventanas,
   los controles, los menús, los recursos y los diálogos) están hechas; sigue
   la 3d: GDI completa para los programas sencillos de la época (mapas de
   bits, tipos de letra, regiones), los iconos y los cursores del programa,
   los aceleradores y los diálogos comunes (abrir y guardar), probando con
   programas reales pequeños.

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
- **Lo que sigue:** imágenes (GIF, JPEG y PNG pequeños), las hojas de estilo
  de fuera cuando sean pequeñas, subir archivos, y la pila residente para
  que dos programas usen la red a la vez.

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
  «Mi PC» con las unidades), «Ejecutar...» y «Documentos» en el menú, y que
  el instalador de Hispa-486 ponga la clave.

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
