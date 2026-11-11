@ECHO OFF
PROMPT $P$G
PATH C:\;C:\BIN
KEYB SP
C:\DRIVERS\HSPRATON
C:\DRIVERS\HSPSND
SYSINFO
ECHO.
ECHO Bienvenido a Hispa-DOS. Escriba AYUDA para ver las órdenes.
IF EXIST C:\INSTALL.BAT ECHO Para instalarlo en un disco duro vacío escriba INSTALL;
IF EXIST C:\INSTALL.BAT ECHO en una unidad ya formateada, INSTALAR unidad: (por ejemplo INSTALAR D:).
