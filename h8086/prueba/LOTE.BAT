@eco off
eco Lote: empiezo %0 [%1] [%2] [%3] 100%%
hola desde el lote
if errorlevel 7 eco Nivel: 7 o mas
if not errorlevel 8 eco Nivel: menos de 8
if errorlevel 8 eco NO DEBE SALIR 1
if exist leeme.txt eco Existe leeme
if not exist nohay.txt eco No existe nohay
if exist nohay.txt eco NO DEBE SALIR 2
if %1==uno goto salto
eco NO DEBE SALIR 3
:salto
if not %2==dos eco NO DEBE SALIR 4
eco Lote: acabo
eco Primera linea > sale.txt
ver >> sale.txt
