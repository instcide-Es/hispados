# Solitario (juegos/solitario.asm, SOLITARI.HVP). Fragmento de
# tests/run_juegos2_tests.sh. La zona de la ventana empieza en (92,85): el
# mazo, el descarte y los cuatro montones de salida están en y=113 y las seis
# columnas en y=205, cada 74 puntos desde x=106 (cartas de 60x80).
#  - con la semilla 7: el reparto de siempre, sacar carta del mazo (clic y
#    Espacio), un arrastre que no vale (la carta vuelve), uno que vale (la
#    de debajo se destapa sola), la cuenta de movimientos y deshacer;
#  - la partida de prueba (/P): arrastrar el caballo de copas sobre el rey de
#    espadas, deshacerlo, repetirlo, subir con doble clic y con el botón
#    derecho, ganar, el aviso, SOLITARI.REC en el disco y «Récords...».
echo "== Solitario: repartir, mazo, arrastrar, deshacer, ganar y récords =="
disco
centro_ini '[Acceso]' 'Nombre=Solitario' 'Programa=A:\HISPADOS\JUEGOS\SOLITARI.HVP' 'Parametros=/S=7' '' \
  '[Acceso]' 'Nombre=Solitario prueba' 'Programa=A:\HISPADOS\JUEGOS\SOLITARI.HVP' 'Parametros=/P'
{ entrar
  shot s0
  # ---- semilla 7 ----
  tecla ret; sleep 2.5
  aparca
  shot s1
  clic 136 153; aparca                      # el mazo: una carta al descarte
  shot s2
  tecla spc; sleep 0.5                      # y otra con Espacio
  shot s3
  arrastra 210 153 432 280; aparca          # el 5 de espadas, sobre el 6 de copas
  shot s4
  arrastra 136 245 214 262; aparca          # el 6 de espadas sobre el rey: no vale
  shot s5
  arrastra 358 263 432 290; aparca          # el 4 de oros, sobre el 5 de espadas
  shot s6
  tecla backspace; sleep 0.6                # deshacer: vuelve y se tapa la de debajo
  shot s7
  tecla ctrl-z; sleep 0.6                   # y otra vez: el 5, al descarte
  shot s8
  tecla alt-f4; sleep 1
  # ---- la partida de prueba ----
  tecla right ret; sleep 2.5
  aparca
  shot s10
  arrastra 210 251 358 270; aparca          # el caballo a un hueco: no vale
  shot s11
  arrastra 210 251 140 268; aparca          # sobre el rey de espadas: vale
  shot s12
  tecla backspace; sleep 0.6                # deshacer
  shot s13
  arrastra 210 251 140 268; aparca
  doble 136 265; aparca                     # el caballo de copas, arriba
  shot s14
  derecho 210 245; aparca                   # el rey de copas, con el derecho
  doble 136 245; aparca                     # el rey de espadas
  shot s15
  clic 136 153                              # el mazo: el rey de oros
  ir 300 400
  ir 210 153
  shot s16                                  # (el ratón, encima de la carta)
  doble 210 153; sleep 1.5; aparca          # y arriba: ganada
  shot s17
  tecla ret; sleep 0.8
  shot s18
  tecla alt-j up up ret; sleep 1            # Récords...
  shot s19
  tecla ret; sleep 0.6
  tecla f2; sleep 0.8                       # otra partida de prueba
  shot s20
  tecla alt-f4; sleep 1
  salir
} | run 300 -vga std

chk $T/s0.ppm tam 800 600 marco 300 380 452 190 activa texto 330 483 9 12 fina "Solitario"
# -- el reparto de la semilla 7 --
chk $T/s1.ppm marco 90 40 462 429 activa titulo 90 40 activa "Solitario" \
  texto 106 91 8 2 fina "Movimientos: 0" texto 254 91 8 2 fina "Tiempo: 0:00" texto 472 91 8 2 fina "Mazo: 19" \
  px 110 120 12 "el mazo, boca abajo (añil)" px 136 153 12 "el rombo del dorso: su centro" px 136 147 10 "y su aro ocre" \
  px 210 153 2 "el descarte, vacío" px 284 153 10 "el montón de los oros, vacío: su moneda" \
  texto 110 206 0 9 negrita "6" texto 184 212 4 9 negrita "12" px 190 207 12 "una carta tapada debajo del rey" \
  texto 480 236 4 9 negrita "4" px 500 420 2 "el tapete, azul petróleo"
chk $T/s2.ppm texto 106 91 8 2 fina "Movimientos: 1" texto 472 91 8 2 fina "Mazo: 18" texto 184 114 0 9 negrita "10"
chk $T/s3.ppm texto 106 91 8 2 fina "Movimientos: 2" texto 472 91 8 2 fina "Mazo: 17" texto 184 114 0 9 negrita "5"
chk $T/s4.ppm texto 106 91 8 2 fina "Movimientos: 3" texto 406 246 0 9 negrita "5" px 430 240 9 "debajo sigue el 6 de copas" \
  texto 184 114 0 9 negrita "10"
chk $T/s5.ppm texto 106 91 8 2 fina "Movimientos: 3" igual 92 110 458 357 $T/s4.ppm 92 110 0
chk $T/s6.ppm texto 106 91 8 2 fina "Movimientos: 4" texto 406 262 4 9 negrita "4" \
  cambia 328 205 60 100 $T/s4.ppm 300 px 334 219 9 "la carta de debajo del 4 de oros se ha destapado"
chk $T/s7.ppm texto 106 91 8 2 fina "Movimientos: 3" igual 92 110 458 357 $T/s4.ppm 92 110 0
chk $T/s8.ppm texto 106 91 8 2 fina "Movimientos: 2" igual 92 110 458 357 $T/s3.ppm 92 110 0
# -- la partida de prueba --
chk $T/s10.ppm marco 90 40 462 429 activa texto 106 91 8 2 fina "Movimientos: 0" texto 472 91 8 2 fina "Mazo: 1" \
  texto 258 114 4 9 negrita "11" texto 332 114 4 9 negrita "10" texto 406 114 0 9 negrita "11" texto 480 114 0 9 negrita "12" \
  texto 110 206 0 9 negrita "12" texto 184 212 4 9 negrita "11" px 190 207 12 "el rey de copas, tapado"
chk $T/s11.ppm texto 106 91 8 2 fina "Movimientos: 0" igual 92 110 458 357 $T/s10.ppm 92 110 0
chk $T/s12.ppm texto 106 91 8 2 fina "Movimientos: 1" texto 110 222 4 9 negrita "11" px 162 215 9 "debajo, el rey de espadas" \
  texto 184 206 4 9 negrita "12"
chk $T/s13.ppm texto 106 91 8 2 fina "Movimientos: 0" igual 92 110 458 357 $T/s10.ppm 92 110 0
chk $T/s14.ppm texto 106 91 8 2 fina "Movimientos: 2" texto 332 114 4 9 negrita "11" px 112 270 9 "el rey de espadas, otra vez solo" texto 110 206 0 9 negrita "12"
chk $T/s15.ppm texto 106 91 8 2 fina "Movimientos: 4" texto 332 114 4 9 negrita "12" texto 406 114 0 9 negrita "12" \
  px 136 245 2 "la primera columna, vacía" px 210 245 2 "y la segunda"
chk $T/s16.ppm texto 106 91 8 2 fina "Movimientos: 5" texto 472 91 8 2 fina "Mazo: 0" texto 184 114 4 9 negrita "12" \
  px 136 144 3 "el mazo, vacío: el rombo hueco" px 136 153 2 "y su centro"
chk $T/s17.ppm titulo 202 216 activa "Solitario resuelto" texto 228 257 0 8 fina "¡Las cuarenta arriba!" \
  texto 228 293 0 8 fina "Es el mejor tiempo." texto 228 311 0 8 fina "Nunca se había hecho con menos movimientos."
chk $T/s18.ppm marco 90 40 462 429 activa texto 106 91 8 2 fina "Movimientos: 6" texto 258 114 4 9 negrita "12" \
  px 210 153 2 "el descarte, vacío"
chk $T/s19.ppm titulo 288 216 activa "Récords" texto 314 257 0 8 fina "Partidas jugadas:  2" \
  texto 314 275 0 8 fina "Partidas ganadas:  1" texto 314 311 0 8 fina "Menos movimientos:  6"
chk $T/s20.ppm texto 106 91 8 2 fina "Movimientos: 0" texto 472 91 8 2 fina "Mazo: 1" texto 254 91 8 2 fina "Tiempo: 0:00" \
  texto 184 212 4 9 negrita "11"
rec SOLITARI.REC "Jugadas=2"; rec SOLITARI.REC "Ganadas=1"; rec SOLITARI.REC "Tiempo=[1-9][0-9]*"; rec SOLITARI.REC "Movimientos=6"
muestra s1 solitario; muestra s6 solitario_columnas; muestra s10 solitario_prueba; muestra s12 solitario_arrastre
muestra s17 solitario_ganado; muestra s19 solitario_records
salida_bien
