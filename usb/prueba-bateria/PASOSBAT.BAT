@ECHO OFF
REM Los pasos de la prueba de la bateria, con HSPACPI ya cargado (los llaman
REM PRUBAT.BAT y COMPLETO.BAT).
ECHO.
ECHO ---- PASO 1 de 4: la bateria, ahora ----
C:\HISPADOS\HSPACPI /E
ECHO ======== paso 1: como esta ahora ======== >> C:\BATERIA.LOG
C:\HISPADOS\HSPACPI /E >> C:\BATERIA.LOG
ECHO.
ECHO ---- PASO 2 de 4 ----
ECHO Si el cargador esta ENCHUFADO, DESENCHUFELO. Si no lo esta, ENCHUFELO.
ECHO Espere 15 segundos y despues pulse una tecla.
PAUSE
C:\HISPADOS\HSPACPI /E
ECHO ======== paso 2: tras cambiar el cargador ======== >> C:\BATERIA.LOG
C:\HISPADOS\HSPACPI /E >> C:\BATERIA.LOG
ECHO Fijese: arriba tiene que haber cambiado "con cable" / "sin cable".
ECHO.
ECHO ---- PASO 3 de 4 ----
ECHO Deje el cargador como estaba al principio.
ECHO Espere 15 segundos y despues pulse una tecla.
PAUSE
C:\HISPADOS\HSPACPI /E
ECHO ======== paso 3: el cargador, como al principio ======== >> C:\BATERIA.LOG
C:\HISPADOS\HSPACPI /E >> C:\BATERIA.LOG
ECHO ======== el registro hasta aqui ======== >> C:\BATERIA.LOG
C:\HISPADOS\HSPACPI /REG:C:\BATERIA.LOG
ECHO.
ECHO ---- PASO 4 de 4: el escritorio ----
ECHO Ahora se abre el escritorio de Hispa-486. MIRE LA BARRA, junto a la hora:
ECHO   - tiene que salir la PILA con su porcentaje;
ECHO   - enchufe o desenchufe el cargador y espere 20 segundos: con cable,
ECHO     sobre la pila sale un rayo blanco.
ECHO Hagale una FOTO a la barra. Para volver aqui: Inicio y, abajo del todo,
ECHO "Salir de Hispa Ventanas...".
PAUSE
C:\HISPADOS\HSPRATON
C:\HISPADOS\VENTANAS
ECHO ======== paso 4: despues del escritorio ======== >> C:\BATERIA.LOG
C:\HISPADOS\HSPACPI /E >> C:\BATERIA.LOG
C:\HISPADOS\HSPACPI /REG:C:\BATERIA.LOG
