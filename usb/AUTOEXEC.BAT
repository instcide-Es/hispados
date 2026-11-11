@ECHO OFF
PROMPT $P$G
PATH C:\;C:\BIN
KEYB SP
C:\DRIVERS\HSPSND
ECHO.
ECHO Bienvenido a HispaDOS. Escribe HELP para ver las ordenes.
IF EXIST C:\INSTALL.BAT ECHO Para instalarlo en un disco duro vacio escribe INSTALL;
IF EXIST C:\INSTALL.BAT ECHO en una unidad ya formateada, INSTALAR unidad: (por ejemplo INSTALAR D:).
