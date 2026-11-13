# Almacén (tests/run_juegos2_tests.sh lo incluye): el nivel 1 con el teclado
# (andar, empujar, una estantería para, deshacer, reiniciar, resolverlo), el
# aviso y el paso al nivel 2, los récords en el disco y en su diálogo, Re Pág
# y Av Pág (el nivel 3 sigue cerrado), un paso con el ratón, /N=24 y que al
# abrirlo otra vez sigue en el nivel en que se quedó.
# La zona de la ventana empieza en 142,105; el nivel 1 (8x5 casillas de 28)
# se dibuja desde 262,223 y el nivel 2 (6x5), desde 290,223.
echo "== Almacén: andar, empujar, deshacer, resolver un nivel y récords en el disco =="
disco
centro_ini '[Acceso]' 'Nombre=Almacen' 'Programa=A:\HISPADOS\JUEGOS\ALMACEN.HVP' 'Parametros=' '' \
  '[Acceso]' 'Nombre=Almacen 24' 'Programa=A:\HISPADOS\JUEGOS\ALMACEN.HVP' 'Parametros=/N=24'
{ entrar
  shot a0
  tecla ret; sleep 2.5
  aparca
  shot a1
  # ---- andar; la estantería para ----
  tecla left left; sleep 0.5
  shot a2
  # ---- empujar la caja una casilla ----
  tecla right right right; sleep 0.5
  shot a3
  # ---- deshacer: la caja vuelve ----
  tecla backspace; sleep 0.5
  shot a4
  # ---- reiniciar y resolverlo: tres pasos a la derecha ----
  tecla r; sleep 0.5
  shot a5
  tecla right right right; sleep 1.5
  shot a6
  tecla ret; sleep 1
  shot a7                                   # ya en el nivel 2
  # ---- Récords... ----
  tecla alt-j; sleep 0.5
  shot a8m
  tecla up up ret; sleep 1
  shot a8
  tecla ret; sleep 0.8
  # ---- Re Pág: el nivel 1, con su marca; Av Pág dos veces: el 3 está cerrado ----
  tecla pgup; sleep 0.6
  shot a9
  tecla pgdn; sleep 0.6
  tecla pgdn; sleep 1
  shot a10
  tecla ret; sleep 0.8
  # ---- el ratón: un clic a la izquierda del mozo (nivel 2) ----
  clic 332 321; aparca
  shot a11
  tecla alt-f4; sleep 1.5
  # ---- /N=24: el último nivel ----
  tecla right ret; sleep 2.5
  aparca
  shot a12
  tecla alt-f4; sleep 1.5
  # ---- otra vez sin parámetros: sigue en el nivel 2 ----
  tecla left ret; sleep 2.5
  shot a13
  tecla alt-f4; sleep 1.5
  salir
} | run 240 -vga std

chk $T/a0.ppm tam 800 600 marco 300 380 452 190 activa titulo 300 380 activa "Juegos" \
  texto 338 483 9 12 fina "Almacen" px 357 453 10 "icono de Almacén: la caja de madera" px 369 447 11 "icono: la caja que ya está en su marca"
chk $T/a1.ppm marco 140 60 468 397 activa titulo 140 60 activa "Almacén" \
  texto 150 113 0 8 fina "Nivel: 1/24" texto 254 113 0 8 fina "Movimientos: 0" texto 390 113 0 8 fina "Empujones: 0" \
  texto 510 113 7 8 fina "Mejor: -" \
  px 326 292 12 "el mozo (su mono añil), en la casilla 2,2" px 381 286 10 "la caja de madera, en 4,2" \
  px 444 293 4 "la marca del suelo, en 6,2" px 349 282 15 "una baldosa" px 263 233 1 "una estantería" \
  px 150 300 8 "fuera del almacén, el fondo"
chk $T/a2.ppm texto 254 113 0 8 fina "Movimientos: 1" texto 390 113 0 8 fina "Empujones: 0" \
  px 298 292 12 "el mozo, en 1,2: la estantería lo para" px 326 292 15 "y no donde estaba" px 263 289 1 "la estantería sigue ahí"
chk $T/a3.ppm texto 254 113 0 8 fina "Movimientos: 4" texto 390 113 0 8 fina "Empujones: 1" \
  px 409 286 10 "la caja, empujada a 5,2" px 382 292 12 "el mozo, en 4,2" px 354 292 15 "3,2, libre"
chk $T/a4.ppm texto 254 113 0 8 fina "Movimientos: 3" texto 390 113 0 8 fina "Empujones: 0" \
  px 381 286 10 "deshacer: la caja vuelve a 4,2" px 354 292 12 "y el mozo a 3,2" px 409 286 15 "5,2, libre"
chk $T/a5.ppm texto 254 113 0 8 fina "Movimientos: 0" texto 390 113 0 8 fina "Empujones: 0" \
  px 326 292 12 "reiniciar: el mozo, en 2,2" px 381 286 10 "y la caja en 4,2"
chk $T/a6.ppm titulo 178 234 activa "Nivel resuelto" \
  texto 204 275 0 8 fina "Nivel 1 resuelto con 3 movimientos y 2 empujones." \
  texto 204 293 0 8 fina "¡Es su mejor marca en este nivel!" \
  texto 254 113 0 8 fina "Movimientos: 3" texto 390 113 0 8 fina "Empujones: 2" texto 510 113 7 8 fina "Mejor: 3"
chk $T/a7.ppm marco 140 60 468 397 activa texto 150 113 0 8 fina "Nivel: 2/24" texto 254 113 0 8 fina "Movimientos: 0" \
  texto 510 113 7 8 fina "Mejor: -" px 353 286 10 "nivel 2: la caja, en 2,2" px 382 320 12 "y el mozo, en 3,3" \
  px 332 265 4 "la marca, en 1,1"
chk $T/a8.ppm titulo 174 162 activa "Récords" \
  texto 200 203 0 8 fina "Mejor marca de cada nivel: movimientos (empujones)" \
  texto 200 239 0 8 fina " 1: 3 (2)         2: --            3: --" texto 200 365 0 8 fina "22: --           23: --           24: --"
chk $T/a9.ppm texto 150 113 0 8 fina "Nivel: 1/24" texto 510 113 7 8 fina "Mejor: 3" px 381 286 10 "nivel 1 otra vez: la caja, en 4,2"
chk $T/a10.ppm titulo 190 234 activa "Almacén" texto 216 275 0 8 fina "Ese nivel todavía está cerrado: resuelva antes" \
  texto 150 113 0 8 fina "Nivel: 2/24"
chk $T/a11.ppm texto 150 113 0 8 fina "Nivel: 2/24" texto 254 113 0 8 fina "Movimientos: 1" \
  px 354 320 12 "un clic a su izquierda: el mozo, en 2,3" px 382 320 15 "y no en 3,3"
chk $T/a12.ppm marco 140 60 468 397 activa titulo 140 60 activa "Almacén" texto 150 113 0 8 fina "Nivel: 24/24" \
  texto 254 113 0 8 fina "Movimientos: 0" \
  px 437 356 11 "nivel 24: una caja que ya está en su marca, verde" px 242 362 12 "y el mozo, en 1,6"
chk $T/a13.ppm marco 140 60 468 397 activa texto 150 113 0 8 fina "Nivel: 2/24" texto 254 113 0 8 fina "Movimientos: 0" \
  px 353 286 10 "sigue en el nivel 2"
rec ALMACEN.REC "Nivel=2"; rec ALMACEN.REC "Mejor=3(,0){23}"; rec ALMACEN.REC "Empujones=2(,0){23}"
ini "Programa=A:\\HISPADOS\\JUEGOS\\ALMACEN.HVP"; ini "Parametros=/N=24"
muestra a1 almacen; muestra a3 almacen_empuja; muestra a6 almacen_resuelto; muestra a8 almacen_records
muestra a8m almacen_menu; muestra a11 almacen_nivel2; muestra a12 almacen_nivel24
salida_bien
