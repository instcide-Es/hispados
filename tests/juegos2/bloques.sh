# Bloques (tests/run_juegos2_tests.sh): paso a paso y con la semilla 7 salen
# siempre las mismas piezas (Codo, Peldaño, Viga, Gancho, Losa...).
#  - se abre desde el grupo: ventana, panel, la pieza arriba y el cartel;
#  - Intro empieza: sale la sombra; con cuatro piezas (girar, mover, soltar)
#    se completa la fila de abajo: 97 puntos y una fila;
#  - pausa, el menú Juego y la ayuda;
#  - soltando ocho piezas más el pozo se llena: «¡Nuevo récord!» con 194
#    puntos y BLOQUES.REC en el disco; la partida siguiente enseña el récord;
#  - el ratón: el botón derecho suelta la pieza y un clic la gira;
#  - al abrirlo otra vez, el récord sale del archivo.
echo "== Bloques: piezas con la semilla fija, una fila, récord en el disco y ratón =="
disco
centro_ini '[Acceso]' 'Nombre=Bloques' 'Programa=A:\HISPADOS\JUEGOS\BLOQUES.HVP' 'Parametros=/S=7 /P'
{ entrar
  shot b0
  tecla ret; sleep 2.5
  aparca
  shot b1
  tecla ret; sleep 0.6                      # empieza: fuera el cartel
  shot b2
  tecla up left left left left left left spc; sleep 0.5      # el Codo, girado, al rincón
  shot b3
  tecla right spc                           # el Peldaño
  tecla left spc                            # la Viga, tumbada entre los dos
  tecla up up right right right right spc; sleep 0.6         # el Gancho: fila completa
  shot b4
  tecla p; sleep 0.6
  shot b5
  tecla p; sleep 0.4
  tecla alt-j; sleep 0.6                    # el menú Juego
  shot b6
  tecla esc; sleep 0.4
  tecla alt-y ret; sleep 1                  # Ayuda, Cómo se juega...
  shot b7
  tecla ret; sleep 0.8
  tecla p; sleep 0.5                        # (la ayuda la dejó en pausa)
  tecla spc spc spc spc spc spc spc spc; sleep 0.8           # hasta arriba
  shot b8
  tecla ret; sleep 0.8                      # otra partida
  shot b9
  tecla ret; sleep 0.5
  derecho 276 300; aparca                   # el botón derecho suelta la Losa
  shot b10
  clic 276 300; aparca                      # un clic gira el Gancho
  shot b11
  tecla alt-f4; sleep 1.2
  tecla ret; sleep 2.5                      # se abre otra vez: el récord, del archivo
  shot b12
  tecla alt-f4; sleep 1.2
  salir
} | run 240 -vga std

chk $T/b0.ppm tam 800 600 marco 300 380 452 190 activa titulo 300 380 activa "Juegos" \
  texto 338 483 9 12 fina "Bloques" px 350 456 1 "icono de Bloques: la pared del pozo" px 361 445 4 "icono de Bloques: la pieza que cae"
chk $T/b1.ppm marco 150 60 412 431 activa titulo 150 60 activa "Bloques" \
  texto 400 117 0 6 fina "Siguiente" texto 400 241 0 6 fina "Puntos" texto 408 262 0 9 negrita "0" \
  texto 400 291 0 6 fina "Filas" texto 408 312 0 9 negrita "0" texto 400 341 0 6 fina "Nivel" texto 408 362 0 9 negrita "1" \
  texto 400 391 0 6 fina "Récord" texto 408 412 0 9 negrita "0" texto 400 443 1 6 fina "P: pausa" \
  texto 248 247 4 8 negrita "Bloques" texto 220 269 0 8 fina "Intro: empezar" \
  px 251 122 10 "el Codo, arriba: azulejo ocre" px 256 127 4 "con su rombo terracota" px 276 142 10 "su otra casilla" \
  px 276 127 8 "la casilla de al lado, vacía" px 469 168 13 "la siguiente: el Peldaño, granate" \
  px 164 300 1 "el marco del pozo" px 160 300 6 "el fondo, arena" px 171 462 8 "el fondo del pozo, crema"
chk $T/b2.ppm px 256 247 8 "sin cartel" px 249 440 7 "la sombra del Codo, abajo" px 269 460 7 "la sombra, la otra casilla" \
  px 251 122 10 "el Codo sigue arriba"
chk $T/b3.ppm texto 408 262 0 9 negrita "19" px 171 442 10 "el Codo, girado, en el rincón" px 191 442 10 "su esquina" \
  px 171 462 10 "y abajo" px 191 462 8 "el hueco que deja" px 271 122 13 "arriba, el Peldaño" px 469 178 2 "la siguiente: la Viga"
chk $T/b4.ppm texto 408 262 0 9 negrita "97" texto 408 312 0 9 negrita "1" texto 408 362 0 9 negrita "1" \
  px 171 462 10 "tras quitar la fila: el Codo, una fila más abajo" px 271 462 13 "el Peldaño" px 331 462 11 "el Gancho" \
  px 196 467 8 "un hueco" px 171 442 8 "encima, nada" px 251 122 4 "arriba, la Losa" px 256 127 10 "con su rombo ocre"
chk $T/b5.ppm texto 256 247 4 8 negrita "Pausa" texto 224 269 0 8 fina "P para seguir"
chk $T/b6.ppm texto 183 109 9 12 fina "ueva partida" px 300 117 12 "la primera orden del menú Juego, marcada" \
  texto 175 157 0 8 fina "Empezar en el nivel" texto 175 225 0 8 fina "Mejores p"
chk $T/b7.ppm titulo 138 162 activa "Cómo se juega" \
  texto 164 203 0 8 fina "Encaje las piezas que caen: cada fila completa desaparece."
chk $T/b8.ppm texto 220 247 4 8 negrita "¡Nuevo récord!" texto 200 269 0 8 fina "Intro: otra partida" \
  texto 408 262 0 9 negrita "194" texto 408 312 0 9 negrita "1" texto 408 412 0 9 negrita "194" \
  px 251 122 10 "el pozo, lleno hasta arriba: el Codo ya no cabe" px 291 142 1 "debajo, el Cazo"
rec BLOQUES.REC "Puntos=194,0,0,0,0"; rec BLOQUES.REC "Nivel=1"
chk $T/b9.ppm texto 408 262 0 9 negrita "0" texto 408 412 0 9 negrita "194" texto 220 269 0 8 fina "Intro: empezar" \
  px 171 462 8 "el pozo, vacío"
chk $T/b10.ppm texto 408 262 0 9 negrita "20" px 251 462 4 "botón derecho: la Losa, abajo" px 271 442 4 "su otra esquina" \
  px 291 142 11 "arriba, el Gancho"
chk $T/b11.ppm px 271 162 11 "clic: el Gancho, girado" px 291 162 11 "su pie" px 251 142 8 "donde estaba ya no está" \
  texto 408 262 0 9 negrita "20"
chk $T/b12.ppm marco 150 60 412 431 activa titulo 150 60 activa "Bloques" texto 408 412 0 9 negrita "194" \
  texto 408 262 0 9 negrita "0" texto 220 269 0 8 fina "Intro: empezar"
muestra b1 bloques; muestra b4 bloques_fila; muestra b5 bloques_pausa; muestra b6 bloques_menu
muestra b8 bloques_record; muestra b11 bloques_raton
salida_bien
