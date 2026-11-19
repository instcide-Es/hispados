@ECHO OFF
PROMPT $P$G
PATH A:\HISPADOS
REM CONFIG = la opción elegida en el menú de CONFIG.SYS (NORMAL, JUEGOS o PRUEBA)
IF "%CONFIG%"=="JUEGOS" GOTO JUEGOS
IF "%CONFIG%"=="PRUEBA" GOTO PRUEBA
REM [SOLO-MEDIO: la opción «Instalar» (INSTALAR quita estas líneas en el
REM disco duro, hasta la otra marca)
IF "%CONFIG%"=="INSTALAR" GOTO INSTALAR
REM SOLO-MEDIO]

REM ---- Normal ----
TECLADO SP
A:\HISPADOS\HSPRATON
REM Mandos de juego (USB, puerto de juegos...): solo se queda si hay alguno.
REM Se configuran en A:\HISPADOS\MANDO.INI; MANDO los enseña.
A:\HISPADOS\HSPJOY
A:\HISPADOS\HSPSND /VOL:80
REM Red por cable: el controlador (HSPRED) ya no se carga al arrancar: ocupa
REM 13 KB de memoria convencional, que les faltan a los juegos y programas
REM grandes. ACTUALIZAR lo carga solo cuando lo necesita; para CONFRED, PING,
REM DESCARGAR o CONSOLA, antes y a mano: HSPRED. Para tener la red siempre,
REM quite el REM de la línea siguiente (con /AUTO, si no hay tarjeta sale en
REM silencio y no gasta memoria; en un equipo con Wi-Fi, sin cable enchufado
REM tampoco se queda, salvo con Preferir=CABLE en RED.INI):
REM A:\HISPADOS\HSPRED /AUTO
GOTO COMUN

:JUEGOS
REM ---- Juegos: lo mismo, en la memoria superior si cabe (LH). HSPSND pone
REM la variable BLASTER; con /SB, si el equipo no tiene Sound Blaster (HD
REM Audio o AC'97), da a los juegos una emulada (si la tiene, usa la real). ----
REM (El mayor, primero: así caben más en la memoria superior.)
LH A:\HISPADOS\HSPSND /SB /VOL:80
REM [SB-PM] Los juegos con DOS/4GW (Doom, Duke Nukem 3D, Mortal Kombat...)
REM solo oyen la Sound Blaster emulada a través de HSPDPMI. Con /SB solo se
REM queda si HSPSND la está emulando (si no, no gasta memoria).
LH A:\HISPADOS\HSPDPMI /SB
LH A:\HISPADOS\HSPJOY
LH A:\HISPADOS\HSPRATON
LH TECLADO SP
REM Protector de pantalla en modo texto (la «Lluvia verde») tras 5 minutos
REM sin tocar nada; quite el REM para ponerlo (PROTECTOR /? explica más):
REM LH A:\HISPADOS\PROTECTO 5
GOTO COMUN

:PRUEBA
REM ---- Prueba del equipo y del sonido: DIAGNOSTICO guarda el informe del
REM equipo y después ofrece la prueba de sonido (AUDICION), con preguntas
REM de sí o no. Luego hay que subir la carpeta INFORMES al FTP. ----
TECLADO SP
A:\HISPADOS\DIAGNOST
GOTO FIN

REM [SOLO-MEDIO
:INSTALAR
REM ---- Instalar en el disco duro: el programa de instalación (pantallas
REM como las del SETUP de MS-DOS 6.22). ----
TECLADO SP
A:\HISPADOS\INSTALAR /SETUP
GOTO FIN
REM SOLO-MEDIO]

:COMUN
REM Red por cable (docs/RED.md): primero el controlador, a mano:
REM A:\HISPADOS\HSPRED; después, CONFRED (IPCONFIG), PING y DESCARGAR.
REM (ACTUALIZAR lo carga solo.)
SYSINFO
ECHO.
ECHO Bienvenido a Hispa-DOS. Escriba AYUDA para ver las órdenes.
REM [SOLO-MEDIO
ECHO Para instalarlo en el disco duro: reinicie y elija «Instalar Hispa-DOS en
ECHO el disco duro» en el menú de arranque (o escriba INSTALAR /SETUP).
REM SOLO-MEDIO]
:FIN
