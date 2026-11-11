@ECHO OFF
REM Instalador de Hispa-DOS desde la memoria USB.
REM Al arrancar desde USB, la memoria es el disco 1 (C:) y el disco duro
REM interno es el disco 2. Solo se instala en un disco duro VACIO.
ECHO.
ECHO  Instalador de Hispa-DOS
ECHO  ======================
ECHO.
FDISK 2 /EMPTY
IF ERRORLEVEL 2 GOTO nodisco
IF ERRORLEVEL 1 GOTO ocupado
ECHO.
ECHO Se creara una particion en el disco duro y se le dara formato.
ECHO Pulsa Ctrl+C para cancelar o cualquier tecla para seguir.
PAUSE
FDISK 2 /PRI
IF ERRORLEVEL 1 GOTO error
FORMAT D: /S /V:HISPADOS
IF ERRORLEVEL 1 GOTO error
INSTALAR D:
IF ERRORLEVEL 1 GOTO error
ECHO.
ECHO Hispa-DOS esta instalado. Quita la memoria USB y reinicia el ordenador.
GOTO fin
:ocupado
ECHO El disco duro ya tiene particiones: no se toca nada.
ECHO Si quieres instalar igualmente, usa FDISK 2 para borrarlas o para
ECHO hacer sitio, y vuelve a ejecutar INSTALL.
GOTO fin
:nodisco
ECHO No se encuentra un segundo disco (el disco duro interno).
GOTO fin
:error
ECHO La instalacion no se ha completado.
:fin
