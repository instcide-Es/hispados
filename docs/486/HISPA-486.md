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
| 3 | USER32/GDI32 sobre Hispa Ventanas: ventanas, mensajes, controles, menús, diálogos, dibujo | un programa Win32 de ventanas sencillo (el «Hola, mundo» de ventana). **3a hecha (17/11/2026)**: un Win32 de ventanas corre dentro de Hispa Ventanas y su `MessageBoxA` es un cuadro de Hispa Ventanas. **3b hecha (17/11/2026)**: el «Hola, mundo» de ventana (clase, ventana, bucle de mensajes, `WndProc`, `WM_PAINT` con texto y dibujos) |
| 4 | El escritorio de Hispa-486: menú de inicio, barra de tareas, escritorio con iconos, «Mi PC» (Hispa Ventanas 2) | arranca a su escritorio |
| 5 | DirectX hasta el 7 (el del 98): DirectDraw (pantalla completa y doble búfer), DirectSound (sobre HSPSND), DirectInput (teclado, ratón, mandos); Direct3D por programa (sin aceleradora), luego con aceleradoras de la época | un juego DirectX sencillo; después, juegos 2D del 98 |
| 6 | Navegador web (HTML 3.2 y CSS sencillo, imágenes; HTTP y, con la red cifrada, HTTPS) | hispados.es en el navegador |
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
4. Fase 3: USER32 y GDI32 sobre Hispa Ventanas. La 3a y la 3b (el «Hola,
   mundo» de ventana) están hechas; sigue la 3c (controles y menús: las
   clases BUTTON, EDIT, STATIC y LISTBOX sobre los controles de Hispa
   Ventanas, `WM_COMMAND`, `CreateMenu`) y que los programas de C con
   biblioteca carguen MSVCRT.DLL dentro de Hispa Ventanas.

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
3. **3c** — controles y menús (un programa con botones, una caja de texto y un
   menú).
4. **3d** — GDI completa para los programas sencillos de la época (dibujo,
   mapas de bits, tipos de letra) y diálogos desde recursos (`.rsrc`:
   `DialogBoxParamA`, `LoadIconA`, `LoadStringA`).

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
