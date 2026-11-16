@eco off
eco Bucle: empiezo
for %%a in (uno dos,tres) do eco Elemento %%a.
for %%f in (*.exe h*.com nohay.*) do eco Archivo %%f
para %%x en (a b) haz if %%x==b eco Para: %%x
llamar sub alfa beta
eco Bucle: he vuelto con %1
call sub gamma
if exist nohay.txt call sub NO DEBE SALIR
:otra
if "%1"=="" goto fin
eco Queda %1
desplazar
goto otra
:fin
eco Bucle: acabo
