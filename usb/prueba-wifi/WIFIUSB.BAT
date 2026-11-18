@ECHO OFF
REM Prueba del Wi-Fi con consola remota por el adaptador USB de red
REM (Realtek RTL8153): la red va por el cable (HSPRED) y el Wi-Fi se carga
REM al lado (/JUNTO) para poder mirarlo desde fuera con CONSOLA.
ECHO ======== prueba del Wi-Fi con consola por cable ======== >> C:\WIFI.LOG
C:\HISPADOS\HSPRED /USB
IF ERRORLEVEL 1 GOTO SINCABLE
C:\HISPADOS\CONFRED
C:\HISPADOS\CONFRED >> C:\WIFI.LOG
ECHO.
ECHO Ahora se abre la consola remota. Apunte la DIRECCION y el CODIGO que
ECHO salgan y digaselos a quien le ayuda. (Esc la cierra.)
ECHO Desde la consola: HSPWIFI /JUNTO /REG:C:\WIFI.LOG   y luego WIFI REDES...
C:\HISPADOS\CONSOLA
GOTO FIN
:SINCABLE
ECHO No se ha encontrado el adaptador USB de red (o no tiene el cable).
ECHO Reinicie y elija la primera opcion del menu (prueba sin cable).
:FIN
