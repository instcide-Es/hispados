@ECHO OFF
REM Prueba del Wi-Fi de Hispa-486 en el equipo de verdad (docs/WIFI.md),
REM paso 1: poner en marcha la tarjeta y buscar redes. Todo queda en
REM C:\WIFI.LOG (en la memoria USB) para subirlo.
CLS
ECHO ============ PRUEBA DEL WI-FI DE HISPA-486 (paso 1 de 2) ============
ECHO Se va a poner en marcha la tarjeta Wi-Fi. Abajo iran saliendo lineas.
ECHO Si la pantalla se queda PARADA mas de un minuto: hagale una FOTO
ECHO y apague el equipo con el boton.
ECHO ======== prueba del Wi-Fi: arranque ======== >> C:\WIFI.LOG
C:\HISPADOS\HSPWIFI /PANTALLA /REG:C:\WIFI.LOG
IF ERRORLEVEL 1 GOTO MAL
CLS
ECHO La tarjeta Wi-Fi esta en marcha. Buscando redes...
ECHO ======== WIFI REDES ======== >> C:\WIFI.LOG
C:\HISPADOS\WIFI REDES >> C:\WIFI.LOG
C:\HISPADOS\WIFI REDES
ECHO.
ECHO AHORA ESCRIBA:    WIFI CONECTAR n       (n = el numero de su red)
ECHO Le pedira la contrasena del Wi-Fi (al escribirla salen asteriscos).
ECHO Cuando diga "Conectado" (o si da un error), ESCRIBA:    PASO2
ECHO (Si su red no sale en la lista, escriba PASO2 igualmente.)
GOTO FIN
:MAL
ECHO.
ECHO El Wi-Fi NO ha arrancado. Lo que ha pasado esta guardado en el archivo
ECHO WIFI.LOG de esta memoria USB. Hagale una foto a esta pantalla, apague
ECHO el equipo (escriba APAGAR) y suba WIFI.LOG al FTP (carpeta prueba-wifi).
:FIN
