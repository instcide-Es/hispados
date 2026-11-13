# Tragabolas (lo incluye tests/run_juegos2_tests.sh). Paso a paso (/P: todo
# avanza un paso con cada flecha) y con la semilla fija (/S=7):
#  - se abre desde el grupo: ventana, rótulos, el Tragón, los erizos, las
#    bolitas, las campanas y el seto;
#  - cuatro pasos a la izquierda: 40 puntos y las bolitas ya no están; el
#    seto lo para (los erizos sí se mueven); pausa con P y el menú Juego;
#  - parado contra el seto, los erizos acaban pinchándolo: baja una vida y,
#    a la tercera, «¡Nuevo récord!»; TRAGABOL.REC queda en el disco, la
#    partida siguiente (Intro) enseña el récord y «Mejores puntuaciones...»
#    también;
#  - en el jardín 2 (/N=2): el camino hasta una campana (220 puntos); los
#    erizos se duermen (grises), se cuenta lo que les queda y no pinchan;
#  - sin /P, con el reloj: una flecha y anda solo hasta el seto; P lo para.
echo "== Tragabolas: jugar paso a paso (semilla fija), campana, vidas y récord en el disco =="
disco
centro_ini '[Acceso]' 'Nombre=Tragabolas' 'Programa=A:\HISPADOS\JUEGOS\TRAGABOL.HVP' 'Parametros=/S=7 /P' '' \
  '[Acceso]' 'Nombre=Tragabolas 2' 'Programa=A:\HISPADOS\JUEGOS\TRAGABOL.HVP' 'Parametros=/S=7 /P /N=2' '' \
  '[Acceso]' 'Nombre=Tragabolas 3' 'Programa=A:\HISPADOS\JUEGOS\TRAGABOL.HVP' 'Parametros=/S=7'
{ entrar
  shot t0
  # ---- el jardín 1: bolitas, seto, pausa y menú ----
  tecla ret; sleep 2
  aparca
  shot t1
  tecla left left left left
  shot t2
  tecla left left                           # el seto: no pasa
  shot t3
  tecla p; sleep 0.5
  shot t4
  tecla p; sleep 0.5
  tecla alt-j; sleep 0.5
  shot t5
  tecla esc
  # ---- quieto contra el seto: los erizos lo pinchan tres veces ----
  for i in 1 2 3 4 5 6; do tecla left left left left left left left left left left; done
  shot t6
  for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do tecla left left left left left left left left left left; done
  sleep 0.5
  shot t7
  tecla ret; sleep 0.5                      # otra partida
  shot t8
  tecla alt-j down down ret; sleep 1        # Mejores puntuaciones...
  shot t9
  tecla ret; sleep 0.5
  tecla alt-f4; sleep 1
  # ---- el jardín 2: hasta la campana de abajo a la izquierda ----
  tecla right ret; sleep 2
  aparca
  shot u1
  tecla left left up up left left down down
  tecla left left left left left up left left
  shot u2
  tecla down down                           # la campana
  shot u3
  tecla up up
  shot u4
  tecla alt-y ret; sleep 1                  # Cómo se juega...
  shot u5
  tecla ret; sleep 0.5
  tecla alt-f4; sleep 1
  # ---- sin /P: con una flecha anda solo, hasta el seto, y P lo para ----
  tecla right ret; sleep 2
  tecla left; sleep 2.5
  tecla p; sleep 0.5
  shot v1
  tecla alt-f4; sleep 1
  salir
} | run 300 -vga std

chk $T/t0.ppm tam 800 600 marco 300 380 452 190 activa titulo 300 380 activa "Juegos" \
  texto 326 483 9 12 fina "Tragabolas" px 355 456 2 "icono de Tragabolas: el Tragón" px 372 456 10 "icono de Tragabolas: una bolita" px 360 444 11 "icono de Tragabolas: el seto"
# -- el jardín 1 --
chk $T/t1.ppm marco 40 30 520 449 activa titulo 40 30 activa "Tragabolas" \
  texto 50 83 0 8 fina "Puntos: 0" texto 186 83 0 8 fina "Jardín: 1" texto 290 83 0 8 fina "Vidas: 3" \
  texto 386 83 0 8 fina "Récord: 0" texto 50 456 1 8 fina "Pulse una flecha para empezar" \
  px 300 392 2 "el Tragón, en su sitio" px 260 283 13 "el erizo granate" px 300 283 12 "el erizo añil" \
  px 340 283 1 "el erizo marrón" px 280 399 10 "una bolita" px 80 138 10 "una campana" \
  px 55 111 11 "el seto" px 300 310 8 "el suelo, crema"
chk $T/t2.ppm texto 50 83 0 8 fina "Puntos: 40" px 220 392 2 "cuatro pasos a la izquierda: el Tragón" \
  px 280 399 8 "la bolita ya no está" px 300 392 8 "donde empezó no queda nadie" px 200 392 11 "delante, el seto"
chk $T/t3.ppm texto 50 83 0 8 fina "Puntos: 40" px 220 392 2 "el seto lo para" px 200 392 11 "el seto sigue ahí" \
  px 260 283 8 "el erizo granate ha salido de su sitio" cambia 50 109 500 340 $T/t2.ppm 100
chk $T/t4.ppm texto 280 260 4 8 negrita "Pausa" texto 208 282 0 8 fina "P o Espacio para seguir"
# (el menú: sin la letra subrayada de cada orden)
chk $T/t5.ppm texto 73 79 9 12 fina "ueva partida      F2" texto 65 127 0 8 fina "Mejores p" \
  px 200 87 12 "la primera orden del menú Juego, marcada"
# -- las vidas y el récord --
chk $T/t6.ppm texto 290 83 0 8 fina "Vidas: 2" texto 50 83 0 8 fina "Puntos: 40"
chk $T/t7.ppm texto 290 83 0 8 fina "Vidas: 0" texto 244 260 4 8 negrita "¡Nuevo récord!" \
  texto 224 282 0 8 fina "Intro: otra partida" texto 386 83 0 8 fina "Récord: 40"
chk $T/t8.ppm texto 50 83 0 8 fina "Puntos: 0" texto 290 83 0 8 fina "Vidas: 3" texto 386 83 0 8 fina "Récord: 40" \
  texto 50 456 1 8 fina "Pulse una flecha para empezar" px 300 392 2 "el Tragón, otra vez en su sitio" \
  px 280 399 10 "y las bolitas, todas"
chk $T/t9.ppm titulo 288 207 activa "Mejores puntuaciones" texto 314 248 0 8 fina "1.   40" texto 314 266 0 8 fina "2.   0"
rec TRAGABOL.REC "Tragabolas - mejores puntuaciones \(Hispa Ventanas\)"; rec TRAGABOL.REC "Puntos=40,0,0,0,0"
# -- el jardín 2 y la campana --
chk $T/u1.ppm marco 40 30 520 449 activa titulo 40 30 activa "Tragabolas" texto 386 83 0 8 fina "Récord: 40" \
  px 300 392 2 "el Tragón" px 115 151 11 "un seto del jardín 2" px 80 418 10 "la campana de abajo a la izquierda"
chk $T/u2.ppm texto 50 83 0 8 fina "Puntos: 160" px 80 372 2 "el Tragón, encima de la campana"
chk $T/u3.ppm texto 50 83 0 8 fina "Puntos: 220" texto 50 456 1 8 fina "Los erizos duermen: 32" \
  px 80 412 2 "el Tragón, donde estaba la campana" px 200 403 7 "un erizo dormido, gris" \
  px 520 418 10 "la otra campana de abajo sigue"
chk $T/u4.ppm texto 50 456 1 8 fina "Los erizos duermen: 30" px 80 372 2 "dos pasos arriba" \
  px 80 418 8 "la campana ya no está" px 200 403 7 "el erizo sigue dormido en su sitio" texto 290 83 0 8 fina "Vidas: 3"
chk $T/u5.ppm titulo 154 198 activa "Cómo se juega" texto 180 239 0 8 fina "Lleve al Tragón con las flechas y tráguese todas las"
chk $T/v1.ppm texto 50 83 0 8 fina "Puntos: 40" px 220 392 2 "con el reloj: ha andado solo hasta el seto" \
  texto 280 260 4 8 negrita "Pausa" texto 290 83 0 8 fina "Vidas: 3"
muestra t1 tragabolas; muestra t3 tragabolas_seto; muestra t4 tragabolas_pausa; muestra t5 tragabolas_menu
muestra t7 tragabolas_record; muestra t9 tragabolas_mejores; muestra u3 tragabolas_campana; muestra u5 tragabolas_ayuda
salida_bien
