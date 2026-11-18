@ECHO OFF
REM Prueba de la bateria por ACPI, segunda manera: HSPACPI /COMPLETO (arranca
REM ACPI como lo haria Windows XP). Todo queda en C:\BATERIA.LOG.
CLS
ECHO ============ PRUEBA DE LA BATERIA, SEGUNDA MANERA (/COMPLETO) ============
ECHO Si la pantalla se queda PARADA mas de un minuto: hagale una FOTO
ECHO y apague el equipo con el boton.
ECHO Durante esta prueba, fijese en el VENTILADOR y en si el equipo se
ECHO calienta mas de lo normal: si es asi, apague y digamelo.
ECHO.
C:\HISPADOS\HSPACPI /U
ECHO ======== prueba de la bateria: modo completo ======== >> C:\BATERIA.LOG
C:\HISPADOS\HSPACPI /COMPLETO /V /REG:C:\BATERIA.LOG
IF ERRORLEVEL 1 GOTO MAL
CALL C:\PASOSBAT.BAT
ECHO.
ECHO ============ HECHO ============
ECHO Escriba APAGAR y suba el archivo BATERIA.LOG de esta memoria USB al FTP
ECHO (carpeta prueba-bateria), con las fotos.
GOTO FIN
:MAL
ECHO.
ECHO De la segunda manera tampoco se ha podido. Lo que ha pasado esta en
ECHO BATERIA.LOG. Hagale una foto a esta pantalla, escriba APAGAR y suba
ECHO BATERIA.LOG al FTP (carpeta prueba-bateria).
:FIN
