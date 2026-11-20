@ECHO OFF
REM Prueba del Wi-Fi, paso 2: la conexion, la direccion (DHCP), salir a
REM Internet y guardar el registro en C:\WIFI.LOG.
ECHO ======== prueba del Wi-Fi: paso 2 (WIFI) ======== >> C:\WIFI.LOG
C:\HISPADOS\WIFI >> C:\WIFI.LOG
C:\HISPADOS\WIFI
ECHO ======== CONFRED ======== >> C:\WIFI.LOG
C:\HISPADOS\CONFRED
C:\HISPADOS\CONFRED >> C:\WIFI.LOG
ECHO ======== PING ======== >> C:\WIFI.LOG
C:\HISPADOS\PING 8.8.8.8 /N:3
C:\HISPADOS\PING 8.8.8.8 /N:3 >> C:\WIFI.LOG
C:\HISPADOS\PING hispados.es /N:4
C:\HISPADOS\PING hispados.es /N:4 >> C:\WIFI.LOG
ECHO ======== DESCARGAR ======== >> C:\WIFI.LOG
C:\HISPADOS\DESCARGA http://actualiza.hispados.es/versiones.txt C:\VERSION.TXT >> C:\WIFI.LOG
ECHO ======== registro del controlador ======== >> C:\WIFI.LOG
C:\HISPADOS\WIFI REGISTRO C:\WIFI.LOG
ECHO.
ECHO PRUEBA TERMINADA. Todo esta guardado en el archivo WIFI.LOG de esta
ECHO memoria USB. Apague el equipo (escriba APAGAR), lleve la memoria a
ECHO otro ordenador y suba WIFI.LOG al FTP, carpeta prueba-wifi.
