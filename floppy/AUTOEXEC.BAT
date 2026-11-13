@ECHO OFF
PROMPT $P$G
PATH A:\HISPADOS
TECLADO SP
A:\HISPADOS\HSPRATON
REM Mandos de juego (USB, puerto de juegos...): solo se queda si hay alguno.
REM Se configuran en A:\HISPADOS\MANDO.INI; MANDO los enseña.
A:\HISPADOS\HSPJOY
A:\HISPADOS\HSPSND /VOL:80
SYSINFO
ECHO.
ECHO Bienvenido a Hispa-DOS. Escriba AYUDA para ver las órdenes.
