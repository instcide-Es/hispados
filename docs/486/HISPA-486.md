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
| 3 | USER32/GDI32 sobre Hispa Ventanas: ventanas, mensajes, controles, menús, diálogos, dibujo | un programa Win32 de ventanas sencillo (el «Hola, mundo» de ventana) |
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
4. Fase 3: USER32 y GDI32 sobre Hispa Ventanas.

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
