@ECHO OFF
PROMPT $P$G
PATH C:\HISPADOS
REM CONFIG = la opción elegida en el menú de CONFIG.SYS (NORMAL o JUEGOS)
IF "%CONFIG%"=="JUEGOS" GOTO JUEGOS

REM ---- Normal ----
TECLADO SP
C:\HISPADOS\HSPRATON
REM Mandos de juego (USB, puerto de juegos...): solo se queda si hay alguno.
REM Se configuran en C:\HISPADOS\MANDO.INI; MANDO los enseña.
C:\HISPADOS\HSPJOY
C:\HISPADOS\HSPSND /VOL:80
GOTO COMUN

:JUEGOS
REM ---- Juegos: lo mismo, en la memoria superior si cabe (LH). HSPSND pone
REM la variable BLASTER; con /SB, si el equipo no tiene Sound Blaster (HD
REM Audio o AC'97), da a los juegos una emulada (si la tiene, usa la real). ----
REM (El mayor, primero: así caben más en la memoria superior.)
LH C:\HISPADOS\HSPSND /SB /VOL:80
LH C:\HISPADOS\HSPJOY
LH C:\HISPADOS\HSPRATON
LH TECLADO SP

:COMUN
SYSINFO
ECHO.
ECHO Bienvenido a Hispa-DOS. Escriba AYUDA para ver las órdenes.
IF EXIST C:\INSTALL.BAT ECHO Para instalarlo en un disco duro vacío escriba INSTALL;
IF EXIST C:\INSTALL.BAT ECHO en una unidad ya formateada, INSTALAR unidad: (por ejemplo INSTALAR D:).
