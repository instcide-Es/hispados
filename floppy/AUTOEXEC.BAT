@ECHO OFF
PROMPT $P$G
PATH A:\HISPADOS
REM CONFIG = la opción elegida en el menú de CONFIG.SYS (NORMAL o JUEGOS)
IF "%CONFIG%"=="JUEGOS" GOTO JUEGOS

REM ---- Normal ----
TECLADO SP
A:\HISPADOS\HSPRATON
REM Mandos de juego (USB, puerto de juegos...): solo se queda si hay alguno.
REM Se configuran en A:\HISPADOS\MANDO.INI; MANDO los enseña.
A:\HISPADOS\HSPJOY
A:\HISPADOS\HSPSND /VOL:80
GOTO COMUN

:JUEGOS
REM ---- Juegos: lo mismo, en la memoria superior si cabe (LH). HSPSND pone
REM la variable BLASTER; con /SB, si el equipo no tiene Sound Blaster (HD
REM Audio o AC'97), da a los juegos una emulada (si la tiene, usa la real). ----
REM (El mayor, primero: así caben más en la memoria superior.)
LH A:\HISPADOS\HSPSND /SB /VOL:80
LH A:\HISPADOS\HSPJOY
LH A:\HISPADOS\HSPRATON
LH TECLADO SP

:COMUN
SYSINFO
ECHO.
ECHO Bienvenido a Hispa-DOS. Escriba AYUDA para ver las órdenes.
