@ECHO OFF
REM DIAGNOSTICO (DIAGNOST.BAT): guarda el informe completo del equipo en una
REM carpeta nueva de la unidad de arranque, \INFORMES\EQUIPO01 (EQUIPO02 la
REM vez siguiente...): INFORME.TXT (versión, fecha, EQUIPO, MEM /C, SET,
REM HSPSND /INFO, HSPRATON /INFO, MANDO, CONFIG.SYS y AUTOEXEC.BAT) y los
REM volcados en binario (PCI, ACPI, SMBIOS, USB, vídeo, CMOS, discos...).
REM Lo hace EQUIPO /INFORME. Suba la carpeta al FTP, a HispaDOS/informes.
REM   DIAGNOSTICO /NOISA      sin sondear los puertos ISA (y /NOPCI, /NOVESA...)
REM   DIAGNOSTICO /NOSONIDO   sin ofrecer la prueba de sonido (AUDICION)
REM   DIAGNOSTICO /?          ayuda
EQUIPO /INFORME %1 %2 %3 %4
REM EQUIPO sale con 2 si en la prueba de sonido algo ha sonado y AUDICION lo
REM ha guardado en \HISPADOS\SONIDO.INI: HSPSND se carga ya con eso, sin
REM reiniciar (también para Hispa Ventanas).
IF NOT ERRORLEVEL 2 GOTO FIN
HSPSND /VOL:80
IF ERRORLEVEL 1 GOTO NOSND
ECHO.
ECHO Sonido configurado con lo aprendido: ya puede usarlo, también en Hispa Ventanas.
GOTO FIN
:NOSND
ECHO.
ECHO Para tener sonido (también en Hispa Ventanas), reinicie y elija
ECHO «Normal» o «Juegos» en el menú de arranque.
:FIN
