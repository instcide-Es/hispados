@ECHO OFF
PROMPT $P$G
PATH C:\HISPADOS
REM CONFIG = la opción elegida en el menú de CONFIG.SYS (NORMAL, JUEGOS o PRUEBA)
IF "%CONFIG%"=="JUEGOS" GOTO JUEGOS
IF "%CONFIG%"=="PRUEBA" GOTO PRUEBA

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
REM [SB-PM] Los juegos con DOS/4GW (Doom, Duke Nukem 3D, Mortal Kombat...)
REM solo oyen la Sound Blaster emulada a través de HSPDPMI. Con /SB solo se
REM queda si HSPSND la está emulando (si no, no gasta memoria).
LH C:\HISPADOS\HSPDPMI /SB
LH C:\HISPADOS\HSPJOY
LH C:\HISPADOS\HSPRATON
LH TECLADO SP
GOTO COMUN

:PRUEBA
REM ---- Prueba del equipo y del sonido: DIAGNOSTICO guarda el informe del
REM equipo y después ofrece la prueba de sonido (AUDICION), con preguntas
REM de sí o no. Luego hay que subir la carpeta INFORMES al FTP. ----
TECLADO SP
C:\HISPADOS\DIAGNOST
GOTO FIN

:COMUN
SYSINFO
ECHO.
ECHO Bienvenido a Hispa-DOS. Escriba AYUDA para ver las órdenes.
IF EXIST C:\INSTALL.BAT ECHO Para instalarlo en un disco duro vacío escriba INSTALL;
IF EXIST C:\INSTALL.BAT ECHO en una unidad ya formateada, INSTALAR unidad: (por ejemplo INSTALAR D:).
:FIN
