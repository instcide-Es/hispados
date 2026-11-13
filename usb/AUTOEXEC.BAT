@ECHO OFF
PROMPT $P$G
PATH C:\HISPADOS
TECLADO SP
C:\HISPADOS\HSPRATON
REM Mandos de juego (USB, puerto de juegos...): solo se queda si hay alguno.
REM Se configuran en C:\HISPADOS\MANDO.INI; MANDO los enseña.
C:\HISPADOS\HSPJOY
C:\HISPADOS\HSPSND /VOL:80
SYSINFO
ECHO.
ECHO Bienvenido a Hispa-DOS. Escriba AYUDA para ver las órdenes.
IF EXIST C:\INSTALL.BAT ECHO Para instalarlo en un disco duro vacío escriba INSTALL;
IF EXIST C:\INSTALL.BAT ECHO en una unidad ya formateada, INSTALAR unidad: (por ejemplo INSTALAR D:).
