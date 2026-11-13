# Solitario (juegos/solitario.asm, SOLITARI.HVP). Fragmento de
# tests/run_juegos2_tests.sh. La ventana está en (106,30) y su zona empieza
# en (108,75): arriba (y=103), el mazo en x=118, el descarte en x=200 y los
# cuatro montones de salida en x=364, 446, 528 y 610; las siete columnas de
# la francesa, en y=215, cada 82 puntos desde x=118 (cartas de 72x100; lo
# tapado asoma 6 puntos y lo destapado 26). Las seis de la española, centradas
# (desde x=159). El índice de una carta está en (x+2, y+4) («indice» de
# tests/tools/ventanas_check.py); el rojo de la francesa es el 196 del cubo
# de la paleta y el negro, el 16.
# Dos sesiones:
#  - la baraja francesa, la de por omisión. Con la semilla 7: el reparto (7
#    columnas, 28 cartas, 24 en el mazo), sacar carta del mazo (clic y
#    Espacio), un arrastre que no vale (rojo sobre rojo: la carta vuelve),
#    uno que vale (la de debajo se destapa sola), deshacer, pasar el mazo
#    hasta el as de tréboles y subirlo con doble clic. La partida de prueba
#    (/P): una dama a una columna vacía no vale, un rey sí; rojo sobre rojo
#    no vale; la dama roja sobre el rey negro, deshacer y repetir; subir con
#    doble clic y con el botón derecho, ganar, el aviso, «Récords...» de la
#    francesa y SOLITARI.REC;
#  - la baraja española, elegida en el menú: se recuerda (SOLITARI.REC), el
#    menú la marca, reparte 6 columnas (21 cartas, 19 en el mazo) y la
#    partida de prueba de siempre se gana con sus reglas.
echo "== Solitario: baraja francesa: repartir, mazo, arrastrar, deshacer, subir, ganar y récords =="
disco
centro_ini '[Acceso]' 'Nombre=Solitario' 'Programa=A:\HISPADOS\JUEGOS\SOLITARI.HVP' 'Parametros=/S=7' '' \
  '[Acceso]' 'Nombre=Solitario prueba' 'Programa=A:\HISPADOS\JUEGOS\SOLITARI.HVP' 'Parametros=/P'
{ entrar
  # ---- semilla 7 ----
  tecla ret; sleep 3
  aparca
  shot s1
  clic 154 153; aparca                      # el mazo: una carta al descarte
  shot s2
  tecla spc; sleep 0.5                      # y otra con Espacio
  shot s3
  arrastra 154 260 400 290; aparca          # el 8 de corazones sobre el 9 de diamantes: no vale
  shot s4
  arrastra 482 280 154 282; aparca          # el 7 de tréboles sobre el 8 de corazones
  shot s5
  tecla backspace; sleep 0.6                # deshacer: vuelve y se tapa la de debajo
  shot s6
  tecla ctrl-z; sleep 0.6                   # y otra vez: el 4 de tréboles, al mazo
  shot s7
  for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16; do tecla spc; sleep 0.3; done
  aparca
  shot s8                                   # en el descarte, el as de tréboles
  doble 236 153; aparca                     # y arriba
  shot s9
  tecla alt-f4; sleep 1
  # ---- la partida de prueba ----
  tecla right ret; sleep 3
  aparca
  shot s10
  arrastra 236 280 318 280; aparca          # la dama de diamantes a un hueco: no vale
  shot s11
  clic 154 153; aparca                      # el mazo: el rey de corazones
  arrastra 236 153 318 265; aparca          # a la columna vacía: vale
  shot s12
  arrastra 236 280 318 300; aparca          # la dama de diamantes sobre él: rojo sobre rojo
  shot s13
  arrastra 236 280 154 300; aparca          # sobre el rey de picas: vale
  shot s14
  tecla backspace; sleep 0.6                # deshacer
  shot s15
  arrastra 236 280 154 300; aparca
  doble 154 300; aparca                     # la dama de diamantes, arriba
  shot s16
  derecho 236 265; aparca                   # el rey de diamantes, con el derecho
  doble 154 265; aparca                     # el rey de picas
  shot s17
  ir 300 450
  doble 318 265; sleep 1.5; aparca          # y el de corazones: ganada
  shot s18
  tecla ret; sleep 0.8
  shot s19
  tecla alt-j up up ret; sleep 1            # Récords...
  shot s20
  tecla ret; sleep 0.6
  tecla f2; sleep 0.8                       # otra partida de prueba
  shot s21
  tecla alt-f4; sleep 1
  salir
} | run 300 -vga std

R=196; N=16
chk $T/s1.ppm marco 106 30 588 567 activa titulo 106 30 activa "Solitario" \
  texto 118 81 8 2 fina "Movimientos: 0" texto 364 81 8 2 fina "Tiempo: 0:00" texto 618 81 8 2 fina "Mazo: 24" \
  px 124 110 12 "el mazo, boca abajo (añil)" px 154 153 10 "el rombo del dorso: su centro" \
  px 120 105 9 "y el margen blanco del dorso" px 118 103 2 "la esquina, redondeada" px 118 110 102 "el filete gris" \
  px 236 153 2 "el descarte, vacío" px 400 150 $R "el montón de los corazones, vacío: su pinta" \
  px 646 150 $N "y el de los tréboles" \
  indice 120 219 $R "8" indice 202 225 $R "7" indice 284 231 $R "9" indice 366 237 $R "9" \
  indice 448 243 $N "7" indice 530 249 $R "2" indice 612 255 $N "8" \
  px 640 216 9 "la séptima columna: 6 tapadas (el margen blanco de la primera)" px 640 250 12 "y el añil de la sexta" \
  indice_rev 666 338 $N "8" px 660 400 2 "debajo, el tapete"
chk $T/s2.ppm texto 118 81 8 2 fina "Movimientos: 1" texto 618 81 8 2 fina "Mazo: 23" indice 202 107 $R "6"
chk $T/s3.ppm texto 118 81 8 2 fina "Movimientos: 2" texto 618 81 8 2 fina "Mazo: 22" indice 202 107 $N "4"
chk $T/s4.ppm texto 118 81 8 2 fina "Movimientos: 2" igual 108 100 588 400 $T/s3.ppm 108 100 0
chk $T/s5.ppm texto 118 81 8 2 fina "Movimientos: 3" indice 120 245 $N "7" indice 120 219 $R "8" \
  cambia 446 215 72 120 $T/s3.ppm 300 px 450 240 9 "la carta de debajo del 7 se ha destapado"
chk $T/s6.ppm texto 118 81 8 2 fina "Movimientos: 2" igual 108 100 588 400 $T/s3.ppm 108 100 0
chk $T/s7.ppm texto 118 81 8 2 fina "Movimientos: 1" igual 108 100 588 400 $T/s2.ppm 108 100 0
chk $T/s8.ppm texto 118 81 8 2 fina "Movimientos: 17" texto 618 81 8 2 fina "Mazo: 7" indice 202 107 $N "A" \
  px 236 153 $N "el as: su pinta grande"
chk $T/s9.ppm texto 118 81 8 2 fina "Movimientos: 18" indice 612 107 $N "A" indice 202 107 $R "10" \
  px 646 153 $N "el as de tréboles, en su montón"
# -- la partida de prueba --
chk $T/s10.ppm marco 106 30 588 567 activa texto 118 81 8 2 fina "Movimientos: 0" texto 618 81 8 2 fina "Mazo: 1" \
  indice 366 107 $R "Q" indice 448 107 $R "J" indice 530 107 $N "Q" indice 612 107 $N "K" \
  indice 120 219 $N "K" indice 202 225 $R "Q" px 230 219 12 "el rey de diamantes, tapado" \
  px 134 265 $N "el recuadro de la figura" px 318 265 2 "la tercera columna, vacía"
chk $T/s11.ppm texto 118 81 8 2 fina "Movimientos: 0" igual 108 100 588 400 $T/s10.ppm 108 100 0
chk $T/s12.ppm texto 118 81 8 2 fina "Movimientos: 2" texto 618 81 8 2 fina "Mazo: 0" indice 284 219 $R "K" \
  px 236 153 2 "el descarte, vacío otra vez"
chk $T/s13.ppm texto 118 81 8 2 fina "Movimientos: 2" igual 108 100 588 400 $T/s12.ppm 108 100 0
chk $T/s14.ppm texto 118 81 8 2 fina "Movimientos: 3" indice 120 245 $R "Q" indice 120 219 $N "K" \
  indice 202 219 $R "K" px 216 265 $R "el rey de diamantes, destapado: su recuadro"
chk $T/s15.ppm texto 118 81 8 2 fina "Movimientos: 2" igual 108 100 588 400 $T/s12.ppm 108 100 0
chk $T/s16.ppm texto 118 81 8 2 fina "Movimientos: 4" indice 448 107 $R "Q" indice 120 219 $N "K" px 154 330 2 "el rey de picas, otra vez solo"
chk $T/s17.ppm texto 118 81 8 2 fina "Movimientos: 6" indice 448 107 $R "K" indice 530 107 $N "K" \
  px 154 265 2 "la primera columna, vacía" px 236 265 2 "y la segunda"
chk $T/s18.ppm titulo 202 216 activa "Solitario resuelto" texto 228 257 0 8 fina "¡Las cincuenta y dos arriba!" \
  texto 228 293 0 8 fina "Es el mejor tiempo." texto 228 311 0 8 fina "Nunca se había hecho con menos movimientos."
chk $T/s19.ppm marco 106 30 588 567 activa texto 118 81 8 2 fina "Movimientos: 7" indice 366 107 $R "K" \
  px 318 265 2 "la tercera columna, vacía"
chk $T/s20.ppm titulo 288 216 activa "Récords (francesa)" texto 314 257 0 8 fina "Partidas jugadas:  2" \
  texto 314 275 0 8 fina "Partidas ganadas:  1" texto 314 311 0 8 fina "Menos movimientos:  7"
chk $T/s21.ppm texto 118 81 8 2 fina "Movimientos: 0" texto 618 81 8 2 fina "Mazo: 1" texto 364 81 8 2 fina "Tiempo: 0:00" \
  indice 202 225 $R "Q"
rec SOLITARI.REC "Baraja=francesa"
rec SOLITARI.REC "Francesa jugadas=2"; rec SOLITARI.REC "Francesa ganadas=1"; rec SOLITARI.REC "Francesa tiempo=[1-9][0-9]*"
rec SOLITARI.REC "Francesa movimientos=7"; rec SOLITARI.REC "Jugadas=0"; rec SOLITARI.REC "Movimientos=0"
muestra s1 solitario; muestra s5 solitario_columnas; muestra s10 solitario_prueba; muestra s14 solitario_arrastre
muestra s18 solitario_ganado; muestra s20 solitario_records
salida_bien

echo "== Solitario: baraja española (elegida en el menú y recordada) =="
disco
centro_ini '[Acceso]' 'Nombre=Solitario prueba' 'Programa=A:\HISPADOS\JUEGOS\SOLITARI.HVP' 'Parametros=/P' '' \
  '[Acceso]' 'Nombre=Solitario' 'Programa=A:\HISPADOS\JUEGOS\SOLITARI.HVP' 'Parametros=/S=7'
{ entrar
  tecla ret; sleep 3                        # la de prueba, francesa
  aparca
  tecla alt-j down down down down; sleep 0.6
  shot e0                                   # el menú: la francesa, marcada
  tecla ret; sleep 1.5                      # Baraja española
  aparca
  shot e1
  # la partida de prueba de siempre (rey de oros en el mazo; caballo y rey
  # de copas en la segunda columna, x=241; rey de espadas en la primera, x=159)
  arrastra 277 280 359 280; aparca          # el caballo a un hueco: no vale
  shot e2
  arrastra 277 280 195 300; aparca          # sobre el rey de espadas: vale
  shot e3
  doble 195 300; aparca                     # el caballo de copas, arriba
  derecho 277 265; aparca                   # el rey de copas, con el derecho
  doble 195 265; aparca                     # el rey de espadas
  clic 154 153                              # el mazo: el rey de oros
  ir 300 450
  doble 236 153; sleep 1.5; aparca          # y arriba: ganada
  shot e4
  tecla ret; sleep 0.8
  tecla alt-j; sleep 0.6
  shot e5                                   # el menú: la española, marcada
  tecla esc; sleep 0.4
  tecla alt-f4; sleep 1
  tecla right ret; sleep 3                  # el otro acceso: sale la española
  aparca
  shot e6
  tecla alt-f4; sleep 1
  salir
} | run 300 -vga std

chk $T/e1.ppm marco 106 30 588 567 activa texto 118 81 8 2 fina "Movimientos: 0" texto 618 81 8 2 fina "Mazo: 1" \
  indice 366 107 4 "11" indice 448 107 4 "10" indice 530 107 0 "11" indice 612 107 0 "12" \
  indice 161 219 0 "12" indice 243 225 4 "11" px 270 219 12 "el rey de copas, tapado" \
  px 140 265 2 "la primera columna, centrada: a su izquierda, el tapete" px 646 300 2 "y no hay séptima columna"
chk $T/e2.ppm texto 118 81 8 2 fina "Movimientos: 0" igual 108 100 588 400 $T/e1.ppm 108 100 0
chk $T/e3.ppm texto 118 81 8 2 fina "Movimientos: 1" indice 161 245 4 "11" indice 161 219 0 "12" indice 243 219 4 "12"
chk $T/e4.ppm titulo 202 216 activa "Solitario resuelto" texto 228 257 0 8 fina "¡Las cuarenta arriba!"
chk $T/e6.ppm marco 106 30 588 567 activa texto 118 81 8 2 fina "Movimientos: 0" texto 618 81 8 2 fina "Mazo: 19" \
  px 140 265 2 "seis columnas, centradas" px 690 300 2 "y no hay séptima"
rec SOLITARI.REC "Baraja=española"
rec SOLITARI.REC "Jugadas=1"; rec SOLITARI.REC "Ganadas=1"; rec SOLITARI.REC "Movimientos=6"
rec SOLITARI.REC "Francesa jugadas=0"
muestra e1 solitario_espanola; muestra e6 solitario_espanola_reparto
salida_bien

# ---- el tamaño de la ventana: se cambia con el ratón, las cartas cambian de
# escala (grande 72x100, mediana 64x88, pequeña 56x76), se sigue jugando, se
# recuerda y se maximiza. La mediana: cada 72 desde x = 108 + (zona - 496)/2,
# arriba en y=103 y las columnas en y=199. La pequeña: cada 62 desde x = 108 +
# (zona - 428)/2, arriba en y=99 y las columnas en y=183 ----
echo "== Solitario: cambiar el tamaño de la ventana (escalas), recordarlo y maximizar =="
disco
centro_ini '[Acceso]' 'Nombre=Solitario prueba' 'Programa=A:\HISPADOS\JUEGOS\SOLITARI.HVP' 'Parametros=/P'
{ entrar
  tecla ret; sleep 3
  aparca
  shot z1
  arrastra 691 594 646 490; aparca          # la esquina: 541x461, la mediana
  shot z2
  arrastra 233 240 161 260; aparca          # la dama de diamantes sobre el rey de picas
  shot z3
  arrastra 646 490 566 450; aparca          # 460x420: la pequeña
  shot z4
  doble 153 247; aparca                     # la dama, arriba
  derecho 215 220; aparca                   # el rey de diamantes
  doble 153 220; aparca                     # el de picas
  clic 153 137                              # el mazo: el de corazones
  ir 300 450
  doble 215 137; sleep 1.5; aparca          # y arriba: ganada
  shot z5
  tecla ret; sleep 0.8
  arrastra 569 453 300 300; aparca          # lo más pequeño: 444x415
  shot z6
  tecla alt-f4; sleep 1
  tecla ret; sleep 3                        # otra vez: con el tamaño de antes
  aparca
  shot z7
  doble 300 41; sleep 1.5; aparca           # doble clic en el título: maximizada
  shot z8
  tecla alt-f4; sleep 1
  salir
} | run 300 -vga std

chk $T/z1.ppm marco 106 30 588 567 activa indice 120 219 $N "K"
chk $T/z2.ppm marco 106 30 541 461 activa texto 128 81 8 2 fina "Movimientos: 0" \
  indice 130 203 $N "K" indice 202 208 $R "Q" indice 346 107 $R "Q" px 160 147 10 "el mazo: el rombo del dorso"
chk $T/z3.ppm texto 128 81 8 2 fina "Movimientos: 1" indice 130 229 $R "Q" indice 202 203 $R "K"
chk $T/z4.ppm marco 106 30 460 420 activa texto 122 81 8 2 fina "Movimientos: 1" \
  indice 124 187 $N "K" indice 124 211 $R "Q" indice 186 187 $R "K" indice 310 103 $R "Q"
chk $T/z5.ppm titulo 202 216 activa "Solitario resuelto" texto 228 257 0 8 fina "¡Las cincuenta y dos arriba!"
chk $T/z6.ppm marco 106 30 444 415 activa texto 114 81 8 2 fina "Movimientos: 6"
chk $T/z7.ppm marco 106 30 444 415 activa texto 114 81 8 2 fina "Movimientos: 0" indice 116 187 $N "K"
chk $T/z8.ppm texto 118 79 8 2 fina "Movimientos: 0" indice 120 217 $N "K" indice 202 223 $R "Q"
rec SOLITARI.REC "Ventana=106,30,444,415"; rec SOLITARI.REC "Francesa ganadas=1"; rec SOLITARI.REC "Francesa movimientos=6"
muestra z2 solitario_mediana; muestra z4 solitario_pequena; muestra z8 solitario_maximizada
salida_bien

# ---- en 640x480 (VENTANAS /640): se abre entera, con la escala mediana, en
# (62,30) de 516x450; la zona empieza en (64,75) y las columnas, en x=72 ----
echo "== Solitario: en una pantalla de 640x480 =="
disco
printf '%s\r\n' '[Centro]' 'Ventana=20,40,300,150' 'Activo=0' "Modulos=$NMOD" '' \
  '[Grupo]' 'Nombre=Juegos' 'Ventana=200,300,300,150' 'Estado=1' '' \
  '[Acceso]' 'Nombre=Solitario prueba' 'Programa=A:\HISPADOS\JUEGOS\SOLITARI.HVP' 'Parametros=/P' > $T/CENTRO.INI
mcopy -o -i $T/f.img $T/CENTRO.INI ::HISPADOS/CENTRO.INI
{ sleep 6
  send 'KEYB SP' 2
  send 'A:\HISPADOS\HSPRATON' 2
  send 'BIN\XMSTEST' 2.5
  send 'VENTANAS /640' 5
  RX=320; RY=240
  aparca
  tecla ret; sleep 3
  aparca
  shot v1
  clic 104 147; aparca                      # el mazo: el rey de corazones
  arrastra 176 147 248 260; aparca          # a la tercera columna, vacía
  arrastra 176 245 104 265; aparca          # la dama de diamantes sobre el rey de picas
  shot v2
  doble 104 280; aparca                     # y arriba las cuatro
  derecho 176 240; aparca
  doble 104 240; aparca
  ir 300 400
  doble 248 240; sleep 1.5; aparca
  shot v3
  tecla ret; sleep 0.8
  tecla alt-f4; sleep 1
  clic 596 13; sleep 1; tecla ret; sleep 2.5
  shot zz
  send 'ECHO TRAS-SALIR'
  send 'BIN\XMSTEST' 2.5
  send 'ECHO SIGUE-VIVO'
  mon quit
} | run 300 -vga std

chk $T/v1.ppm tam 640 480 marco 62 30 516 450 activa titulo 62 30 activa "Solitario" \
  texto 72 81 8 2 fina "Movimientos: 0" texto 504 81 8 2 fina "Mazo: 1" \
  indice 74 203 $N "K" indice 146 208 $R "Q" indice 290 107 $R "Q" indice 506 107 $N "K"
chk $T/v2.ppm texto 72 81 8 2 fina "Movimientos: 3" indice 74 229 $R "Q" indice 218 203 $R "K" indice 146 203 $R "K"
chk $T/v3.ppm titulo 122 156 activa "Solitario resuelto"
rec SOLITARI.REC "Ventana=62,30,516,450"; rec SOLITARI.REC "Francesa movimientos=7"
muestra v1 solitario_640
salida_bien
