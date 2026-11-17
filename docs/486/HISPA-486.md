# Hispa-486

El tercer sistema de la familia Hispa-DOS: el de mediados y finales de los 90.
Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.

## Qué es

- **Para quién:** los 486 y Pentium (y lo que vino después) con 8 MB o más.
  El nombre definitivo será el del procesador mínimo con el que vaya bien de
  verdad (si pide Pentium, se cambia).
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
  2000, es el sistema siguiente (y se escribirá en [Castel](../../castel/)).

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
| 1 | **Cargador PE** (`HW32.EXE`): cabeceras, secciones, importaciones, TLS no; KERNEL32 mínimo de consola (salida, entrada, salida del programa, línea de órdenes, memoria, montón, errores, versión) | un «Hola» de consola Win32 hecho por nosotros corre en Hispa-DOS |
| 2 | KERNEL32 de consola completo: archivos (CreateFile, ReadFile, WriteFile, SetFilePointer, FindFirstFile...), directorios, hora y fecha, entorno, consola (colores, cursor, eventos de teclado) | programas de consola reales de la época (compiladores, utilidades) |
| 3 | USER32/GDI32 sobre Hispa Ventanas: ventanas, mensajes, controles, menús, diálogos, dibujo | un programa Win32 de ventanas sencillo (el «Hola, mundo» de ventana) |
| 4 | El escritorio de Hispa-486: menú de inicio, barra de tareas, escritorio con iconos, «Mi PC» (Hispa Ventanas 2) | arranca a su escritorio |
| 5 | DirectX hasta el 7 (el del 98): DirectDraw (pantalla completa y doble búfer), DirectSound (sobre HSPSND), DirectInput (teclado, ratón, mandos); Direct3D por programa (sin aceleradora), luego con aceleradoras de la época | un juego DirectX sencillo; después, juegos 2D del 98 |
| 6 | Navegador web (HTML 3.2 y CSS sencillo, imágenes; HTTP y, con la red cifrada, HTTPS) | hispados.es en el navegador |
| 7 | Consolas de su época: Hispa-Drive, Hispa-SNes, Hispa-BoyColor (con juegos propios) | los tres con un juego cada uno |
| 8 | Arranque UEFI nativo (cargador propio, disco y pantalla sin BIOS) | arranca en un PC sin CSM |
| 9 | Programas propios de la época: la oficina «-486», audio y vídeo | versión 1.0 |

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

- **Hispa-8086** (8086 a 286) e **Hispa-386** (386+, con Hispa Ventanas) siguen
  su camino; Hispa-486 lleva todo lo de Hispa-386.
- **Castel**: cuando esté maduro, las piezas nuevas de Hispa-486 se podrán
  escribir en él (el cargador y las DLL empiezan en ensamblador, como todo
  Hispa-DOS).
