@ECHO OFF
PROMPT $P$G
PATH C:\HISPADOS
REM Windows 3.1x: su programa de instalación no toca un CONFIG.SYS con menú
REM y deja sus cambios en \WINDOWS\CONFIG.WIN y AUTOEXEC.WIN. WINCONF los
REM pone (con la opción «Windows» en el menú, sin los emuladores de
REM Hispa-DOS) y reinicia; si ya están, no hace nada. Guía: docs/WINDOWS.md.
IF EXIST C:\WINDOWS\WIN.COM C:\HISPADOS\WINCONF /AUTO
REM CONFIG = la opción elegida en el menú de CONFIG.SYS (NORMAL, JUEGOS o PRUEBA)
IF "%CONFIG%"=="JUEGOS" GOTO JUEGOS
IF "%CONFIG%"=="PRUEBA" GOTO PRUEBA
REM [SOLO-MEDIO: la opción «Instalar» (INSTALAR quita estas líneas en el
REM disco duro, hasta la otra marca)
IF "%CONFIG%"=="INSTALAR" GOTO INSTALAR
REM SOLO-MEDIO]

REM ---- Normal ----
TECLADO SP
C:\HISPADOS\HSPRATON
REM Mandos de juego (USB, puerto de juegos...): solo se queda si hay alguno.
REM Se configuran en C:\HISPADOS\MANDO.INI; MANDO los enseña.
C:\HISPADOS\HSPJOY
C:\HISPADOS\HSPSND /VOL:80
REM Red por cable: el controlador (HSPRED) ya no se carga al arrancar: ocupa
REM 13 KB de memoria convencional, que les faltan a los juegos y programas
REM grandes. ACTUALIZAR lo carga solo cuando lo necesita; para CONFRED, PING,
REM DESCARGAR o CONSOLA, antes y a mano: HSPRED. Para tener la red siempre,
REM quite el REM de la línea siguiente (con /AUTO, si no hay tarjeta sale en
REM silencio y no gasta memoria; en un equipo con Wi-Fi, sin cable enchufado
REM tampoco se queda, salvo con Preferir=CABLE en RED.INI):
REM C:\HISPADOS\HSPRED /AUTO
REM Wi-Fi (tarjetas Broadcom BCM43224/43225, con WPA2): quite el REM de las
REM dos líneas siguientes. La red se elige una vez con WIFI REDES y WIFI
REM CONECTAR; queda guardada en RED.INI y WIFI /AUTO la pone al arrancar.
REM (No va con HSPEMM: solo en la configuración «Normal».)
REM C:\HISPADOS\HSPWIFI
REM C:\HISPADOS\WIFI /AUTO
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
REM Protector de pantalla en modo texto (la «Lluvia verde») tras 5 minutos
REM sin tocar nada; quite el REM para ponerlo (PROTECTOR /? explica más):
REM LH C:\HISPADOS\PROTECTO 5
GOTO COMUN

:PRUEBA
REM ---- Prueba del equipo y del sonido: DIAGNOSTICO guarda el informe del
REM equipo y después ofrece la prueba de sonido (AUDICION), con preguntas
REM de sí o no. Luego hay que subir la carpeta INFORMES al FTP. ----
TECLADO SP
C:\HISPADOS\DIAGNOST
GOTO FIN

REM [SOLO-MEDIO
:INSTALAR
REM ---- Instalar en el disco duro: el programa de instalación (pantallas
REM como las del SETUP de MS-DOS 6.22). ----
TECLADO SP
C:\HISPADOS\INSTALAR /SETUP
GOTO FIN
REM SOLO-MEDIO]

:COMUN
REM Red por cable (docs/RED.md): primero el controlador, a mano:
REM C:\HISPADOS\HSPRED; después, CONFRED (IPCONFIG), PING y DESCARGAR.
REM (ACTUALIZAR lo carga solo.)
SYSINFO
ECHO.
ECHO Bienvenido a Hispa-DOS. Escriba AYUDA para ver las órdenes.
REM [SOLO-MEDIO
ECHO Para instalarlo en el disco duro: reinicie y elija «Instalar Hispa-DOS en
ECHO el disco duro» en el menú de arranque (o escriba INSTALAR /SETUP).
REM SOLO-MEDIO]
:FIN
