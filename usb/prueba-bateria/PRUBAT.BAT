@ECHO OFF
REM Prueba de la bateria por ACPI (HSPACPI, docs/ACPI.md) en el equipo de
REM verdad. Todo queda en C:\BATERIA.LOG (en la memoria USB) para subirlo.
CLS
ECHO ============ PRUEBA DE LA BATERIA (HSPACPI) ============
ECHO Se va a leer la bateria del portatil. Abajo iran saliendo lineas.
ECHO Si la pantalla se queda PARADA mas de un minuto: hagale una FOTO
ECHO y apague el equipo con el boton.
ECHO.
ECHO ======== prueba de la bateria: modo minimo ======== >> C:\BATERIA.LOG
C:\HISPADOS\HSPACPI /V /REG:C:\BATERIA.LOG
IF ERRORLEVEL 1 GOTO MAL
CALL C:\PASOSBAT.BAT
ECHO.
ECHO ============ HECHO ============
ECHO Si la pila HA SALIDO en la barra y cambiaba con el cargador: perfecto.
ECHO Si NO ha salido, o el numero no era el de verdad, escriba:  COMPLETO
ECHO (prueba la otra manera de leerla). Si no, escriba APAGAR.
ECHO Despues suba el archivo BATERIA.LOG de esta memoria USB al FTP
ECHO (carpeta prueba-bateria).
GOTO FIN
:MAL
ECHO.
ECHO De la primera manera NO se ha podido leer la bateria. Lo que ha pasado
ECHO esta guardado en BATERIA.LOG. Ahora se prueba la segunda manera.
PAUSE
CALL C:\COMPLETO.BAT
:FIN
