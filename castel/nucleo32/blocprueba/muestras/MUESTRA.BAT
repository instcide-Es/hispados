@echo off
rem un comentario
:: otro
:inicio
set RUTA=%PATH%;C:\DOS
if exist %1 goto inicio
for %%a in (*.txt) do type %%a
echo rem no es comentario
