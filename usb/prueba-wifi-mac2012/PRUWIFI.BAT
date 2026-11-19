@ECHO OFF
REM Prueba del Wi-Fi de Hispa-486 en el MacBook Air 2012 (docs/WIFI.md).
REM No borra la pantalla y lo apunta todo en C:\WIFI.LOG (en la memoria
REM USB), paso a paso: si el equipo se cuelga o la pantalla deja de pintar,
REM el archivo dice hasta donde se llego.
ECHO ============ PRUEBA DEL WI-FI EN EL MACBOOK AIR 2012 ============
ECHO Van a salir muchas lineas (es normal). Si la pantalla se queda PARADA
ECHO mas de un minuto: hagale una FOTO y apague con el boton.
ECHO ======== prueba Wi-Fi Mac 2012 (@VERSION@, @FECHA@ @HORA@) ======== >> C:\WIFI.LOG
C:\HISPADOS\HSPWIFI /PANTALLA /REG:C:\WIFI.LOG
IF ERRORLEVEL 1 GOTO MAL
ECHO ======== HSPWIFI ha vuelto al DOS; ahora WIFI REDES ======== >> C:\WIFI.LOG
ECHO.
ECHO La tarjeta Wi-Fi esta en marcha. Buscando redes...
C:\HISPADOS\WIFI REDES >> C:\WIFI.LOG
ECHO ======== WIFI REDES hecho; ahora en la pantalla ======== >> C:\WIFI.LOG
C:\HISPADOS\WIFI REDES
ECHO ======== registro del controlador ======== >> C:\WIFI.LOG
C:\HISPADOS\WIFI REGISTRO C:\WIFI.LOG
ECHO ======== fin de PRUWIFI ======== >> C:\WIFI.LOG
ECHO.
ECHO SI HA VISTO LA LISTA DE REDES, ESCRIBA:   WIFI CONECTAR n   (n = el
ECHO numero de su red; pide la contrasena) y despues:   PASO2
ECHO Al final, para probar el escritorio con el Wi-Fi puesto:   VENTANAS
GOTO FIN
:MAL
ECHO ======== HSPWIFI ha salido con error ======== >> C:\WIFI.LOG
ECHO.
ECHO El Wi-Fi NO ha arrancado. Hagale una foto a esta pantalla, apague
ECHO (escriba APAGAR) y suba WIFI.LOG al FTP.
:FIN
