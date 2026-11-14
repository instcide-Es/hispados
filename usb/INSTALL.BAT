@ECHO OFF
REM Instalador de Hispa-DOS desde la memoria USB.
REM Al arrancar desde USB, la memoria es el disco 1 (C:) y el disco duro
REM interno es el disco 2. INSTALAR sin unidad busca el disco duro (nunca
REM toca esta memoria), ofrece crear la particion si no tiene, le da
REM formato, copia el sistema y lo deja listo para arrancar solo.
INSTALAR
IF ERRORLEVEL 6 GOTO fin
IF ERRORLEVEL 1 GOTO error
ECHO.
ECHO Hispa-DOS esta instalado.
GOTO fin
:error
ECHO La instalacion no se ha completado.
ECHO Para hacer las particiones a mano: PARTIR (opcion 5 para elegir el disco).
:fin
