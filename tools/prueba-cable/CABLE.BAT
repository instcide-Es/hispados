@ECHO OFF
REM Prueba de la red por cable del HP Mini 5103 (Marvell Yukon 88E8059).
REM La llama el AUTOEXEC.BAT de la memoria USB de prueba (tools/prueba-cable).
REM Lo que sale se guarda en C:\CABLE.TXT: es el archivo que hay que subir.
ECHO.
ECHO ==================================================================
ECHO   PRUEBA DE LA RED POR CABLE DEL HP MINI (Marvell Yukon 88E8059)
ECHO ==================================================================
ECHO   Enchufe ahora el cable de red (del HP Mini al router).
ECHO   Lo que salga se guarda en C:\CABLE.TXT, en esta memoria USB.
ECHO.
PAUSE
ECHO ==== PRUEBA DEL CABLE (HP Mini, Marvell Yukon) ==== >> C:\CABLE.TXT
VER >> C:\CABLE.TXT
ECHO ---- 0. EQUIPO /PCI (las tarjetas que ve el equipo) ---- >> C:\CABLE.TXT
EQUIPO /PCI >> C:\CABLE.TXT
ECHO ---- 1. HSPRED /DEPURA (con IRQ) ---- >> C:\CABLE.TXT
HSPRED /DEPURA:C:\CABLE.TXT
IF ERRORLEVEL 1 GOTO NOCARGA
ECHO.
ECHO Pidiendo la dirección al router (CONFRED)...
CONFRED
IF ERRORLEVEL 1 GOTO SINIRQ
GOTO CONRED

:SINIRQ
ECHO ---- CONFRED ha fallado (con IRQ). HSPRED /INFO: ---- >> C:\CABLE.TXT
HSPRED /INFO /DEPURA:C:\CABLE.TXT
ECHO.
ECHO No ha ido a la primera. Segundo intento, sin interrupción (/SONDEO)...
HSPRED /U
ECHO ---- 2. HSPRED /SONDEO /DEPURA (sin IRQ) ---- >> C:\CABLE.TXT
HSPRED /SONDEO /DEPURA:C:\CABLE.TXT
IF ERRORLEVEL 1 GOTO NOCARGA
ECHO.
ECHO Pidiendo la dirección al router (CONFRED)...
CONFRED
IF ERRORLEVEL 1 GOTO SINRED

:CONRED
ECHO ---- CONFRED ---- >> C:\CABLE.TXT
CONFRED >> C:\CABLE.TXT
ECHO.
PING /P /N:4
ECHO ---- PING /P (a la puerta de enlace) ---- >> C:\CABLE.TXT
PING /P /N:4 >> C:\CABLE.TXT
ECHO ---- HSPRED /INFO al final ---- >> C:\CABLE.TXT
HSPRED /INFO /DEPURA:C:\CABLE.TXT
ECHO.
ECHO ==================================================================
ECHO   La tarjeta tiene dirección: LA RED POR CABLE FUNCIONA.
ECHO   (Arriba, en el PING, debe poner «Respuesta desde...».)
ECHO   El informe está en C:\CABLE.TXT: súbalo al FTP, a la carpeta
ECHO   HispaDOS/prueba-cable-hpmini (lo explica LEEME.TXT).
ECHO ==================================================================
ECHO.
ECHO Si quiere que entremos a mirar a distancia, se puede abrir ahora la
ECHO consola remota (CONSOLA, puerto 2323). Solo entra quien reciba de usted
ECHO el código de 6 cifras que saldrá en esta pantalla; Esc la cierra.
ELEGIR /C:SN ¿Abrir la consola remota
IF ERRORLEVEL 2 GOTO FIN
ECHO ---- Se abre CONSOLA ---- >> C:\CABLE.TXT
CONSOLA
GOTO FIN

:SINRED
ECHO ---- CONFRED ha fallado también con /SONDEO. HSPRED /INFO: ---- >> C:\CABLE.TXT
HSPRED /INFO /DEPURA:C:\CABLE.TXT
ECHO.
ECHO ==================================================================
ECHO   La red todavía NO va en este equipo (para eso es la prueba).
ECHO   El informe está en C:\CABLE.TXT: súbalo al FTP, a la carpeta
ECHO   HispaDOS/prueba-cable-hpmini (lo explica LEEME.TXT).
ECHO ==================================================================
GOTO FIN

:NOCARGA
ECHO ---- HSPRED no se ha cargado ---- >> C:\CABLE.TXT
ECHO.
ECHO ==================================================================
ECHO   HSPRED no ha podido poner en marcha la tarjeta (lo dice arriba).
ECHO   El informe está en C:\CABLE.TXT: súbalo al FTP, a la carpeta
ECHO   HispaDOS/prueba-cable-hpmini (lo explica LEEME.TXT).
ECHO ==================================================================

:FIN
ECHO.
ECHO Para repetir la prueba: CABLE. Para apagar: APAGAR.
