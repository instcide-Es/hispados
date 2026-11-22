# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pruebas_juegos.sh - la parte 39 (SOLO=juegos, los juegos: el paquete
# «juegos», nucleo32/juegos/) de pruebas.sh, que lee este archivo («.»): la
# función parte_juegos, con los ayudantes de pruebas.sh (ok, ko, mira, miracap)
# y sus carpetas ($AQUI, $P, $G).
#
#  1. En el anfitrión (con ASan y UBSan): la física de Hispa-Pinball (la bola
#     no atraviesa nada a ninguna velocidad, conserva lo razonable, el flipper
#     golpea y no se deja atravesar) y la música de sintes.c; los 24 niveles de
#     Almacén, con solución (tools/almacen_niveles.py).
#  2. hacer.sh: los ocho programas, sin CMOV ni FCOMI; paquete.sh: el paquete
#     «juegos» 1.0 (de tipo juegos) y su línea del catálogo.
#  3. En QEMU, en un disco con \JUEGOS y los accesos en \MENU (como los deja
#     el paquete): Inicio > Todos los programas > Juegos, con los ocho y sus
#     iconos, y uno abierto desde ahí; cada juego desde la Consola, con teclas
#     grabadas (Bloques: una Viga que hace cuatro filas a la vez y la reserva;
#     Culebra: hasta la fruta y contra el borde; Campo de minas: el tablero de
#     prueba, ganado; Solitario: subir las que se puedan, ganado; Tragabolas:
#     bolitas; Almacén: el primer nivel; Rompemuros y el pinball, jugando
#     solos; el pinball también lanzado a mano y a 1024 x 600, a pantalla
#     completa), mirando lo que dicen por el puerto serie (F12: su estado) y
#     las capturas; los récords en \AJUSTES\JUEGOS del disco; el sonido (la
#     música de Bloques sale por la tarjeta) y, sin el servicio de sonido, en
#     silencio sin fallar.
#  4. Lo que gastan en un Atom N270 de QEMU (-cpu n270 con -icount: mil
#     millones de instrucciones por segundo, como el Atom N455 del HP Mini
#     5103): el pinball a 60 cuadros por segundo a 1024 x 600 y Rompemuros.

# juega NOMBRE ORDEN... : arranca el escritorio con el disco $Y/j.img (lo que
# se escribe se queda), abre la Consola y teclea las órdenes; lo del puerto
# serie, a $Y/NOMBRE.txt
juega() {
  local n=$1; shift
  CAPTURAS=$G ESCRIBE=1 DISCO=$Y/j.img MEMORIA=${JMEM:-128} MODULOS= ARRANQUE="escritorio${JARR:+ $JARR}" \
    QEMU_MAS="${JQEMU:--audiodev wav,id=snd0,path=$Y/$n.wav -device intel-hda -device hda-duplex,audiodev=snd0}" \
    $AQUI/nucleo32/probar.sh $Y/SEMILLA.BIN ":pausa 4" ":doble 40 190" ":pausa 3" "$@" > $Y/$n.sal 2>&1
  tr -d '\r' < $Y/$n.sal > $Y/$n.txt
  S=$Y/$n.txt
}

# lo que dice la última línea de estado (F12) de un juego: el valor de «clave=»
estado_de() { grep "^$1: estado=" $S | tail -n 1 | tr ' ' '\n' | sed -n "s/^$2=//p"; }

parte_juegos() {
echo "== (JUEGOS) Hispa-Pentium: los juegos (el paquete «juegos»: Bloques, Rompemuros, Hispa-Pinball y los de Hispa-386) =="
local Y=$P/juegos J=$AQUI/nucleo32/juegos S l n f c k
mkdir -p $Y $G
export MTOOLS_SKIP_CHECK=1
# ---- 1. en el anfitrión ----
if bash $J/prueba/anfitrion.sh $Y/anf > $Y/anf.txt 2>&1; then :; fi
while IFS= read -r l; do
  case "$l" in "OK "*) ok "anfitrión: pinball: ${l#OK  }" ;; "MAL "*) ko "anfitrión: pinball: ${l#MAL }" ;; esac
done < <(grep -E "^(OK|MAL) " $Y/anf.txt)
grep -q "^== todo bien ==" $Y/anf.txt || ko "anfitrión: las pruebas de la física no acaban bien: $(tail -n 3 $Y/anf.txt | tr '\n' ' ')"
if python3 -I - $Y/anf/sonido.raw <<'PY'
import struct, sys
d = open(sys.argv[1], 'rb').read()
s = struct.unpack('<%dh' % (len(d) // 2), d)
v = [max(abs(x) for x in s[i:i + 2205]) for i in range(0, len(s) - 2205, 2205)]
suena = sum(1 for x in v[:30] if x > 2000)
sys.exit(0 if len(v) >= 30 and suena >= 28 else 1)
PY
then ok "anfitrión: sintes.c: la música de dos voces suena entera (cada décima de segundo, con sonido)"
else ko "anfitrión: sintes.c: la música no suena como debe"; fi
if timeout 600 python3 -I $AQUI/../tools/almacen_niveles.py $AQUI/../juegos/almacen.asm > $Y/almacen.txt 2>&1; then
  ok "Almacén: los 24 niveles (los de Hispa-386) están bien hechos y tienen solución (tools/almacen_niveles.py)"
else ko "Almacén: los niveles: $(tail -n 2 $Y/almacen.txt | tr '\n' ' ')"; fi
# ---- 2. los programas y el paquete ----
rm -rf "${Y:?}/c"
if JUEGOS_CARPETA='\JUEGOS' JUEGOS_ICONOS='\MENU' bash $J/hacer.sh $Y/c/BIN > $Y/hacer.txt 2>&1 && [ "$(ls $Y/c/BIN/*.BIN | wc -l)" = 8 ]; then
  ok "hacer.sh: los ocho programas ($(ls $Y/c/BIN | tr '\n' ' ')), para el Pentium (-march=i586: sin CMOV ni FCOMI), con sus accesos y sus iconos"
else ko "hacer.sh no hace los juegos: $(tail -n 3 $Y/hacer.txt)"; return; fi
if bash $J/paquete.sh $Y/paq > $Y/paq.txt 2>&1 && grep -q "^paquete=juegos 1.0 JUEGOS.HPQ" $Y/paq/CATALOGO.LIN && grep -q "^tipo=juegos juegos" $Y/paq/CATALOGO.LIN \
   && [ "$(python3 -I $AQUI/../tools/hpq.py mira $Y/paq/JUEGOS.HPQ | grep -c ' bien$')" = 25 ]; then
  ok "paquete.sh: el paquete «juegos» 1.0 (de tipo juegos): los ocho en \\HISPA\\JUEGOS, sus accesos y sus iconos en \\HISPA\\MENU y el LEEME"
else ko "paquete.sh: $(tail -n 2 $Y/paq.txt)"; fi
# el disco: los programas en \JUEGOS, los accesos en \MENU (sin instalar, el menú los busca ahí) y el sonido
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $Y/semilla.asm > /dev/null && nasm -f bin $Y/semilla.asm -o $Y/SEMILLA.BIN || { ko "semilla.cas no compila"; return; }
$AQUI/nucleo32/disco.sh $Y/base.img 32 > $Y/disco.txt 2>&1 || { ko "disco.sh no hace el disco: $(tail -n 2 $Y/disco.txt)"; return; }
bash $AQUI/nucleo32/sonido/hacer.sh $Y/son > /dev/null 2>&1 || { ko "no se hace SONIDO.BIN"; return; }
cp $Y/base.img $Y/j.img
mmd -i $Y/j.img ::JUEGOS ::MENU
mcopy -i $Y/j.img $Y/c/BIN/*.BIN ::JUEGOS/
mcopy -i $Y/j.img $Y/c/*.MNU $Y/c/*.ICP ::MENU/
mcopy -i $Y/j.img $Y/son/SONIDO.BIN $Y/son/INICIO.WAV ::
# ---- 3. en el escritorio ----
# el menú Inicio: Todos los programas > Juegos; y Campo de minas abierto desde ahí
juega menu ":clic 40 585" ":pausa 1" ":raton 100 496" ":pausa 1" ":raton 250 496" ":raton 300 453" ":pausa 1" ":captura jm1" ":clic 545 418" ":pausa 4" ":captura jm2"
miracap "Inicio > Todos los programas > Juegos: los ocho, por orden, cada uno con su icono (los de Hispa-386 y los nuevos)" jm1 \
  texto 486 355 494 363 "Almacén" texto 486 381 494 389 "Bloques" texto 486 407 494 415 "Campo de minas" texto 486 433 494 441 "Culebra" \
  texto 486 459 494 467 "Hispa-Pinball" texto 486 485 494 493 "Rompemuros" texto 486 511 494 519 "Solitario" texto 486 537 494 545 "Tragabolas" \
  rgb 471 366 E8B33A 20 rgb 471 392 24364A 20 rgb 471 470 2E8C83 20 rgb 471 496 16213A 20
mira "Campo de minas, abierto desde el menú (y sus récords, en la carpeta de ajustes)" "^minas: ventana [0-9]* x [0-9]*" "^minas: partida nueva: Pequeño" 'AJUSTES.JUEGOS.MINAS.REC'
# Bloques: una Viga en el pozo de prueba (cuatro filas casi llenas): girar, a la derecha y soltar
local soltar=()
for k in $(seq 1 40); do soltar+=(":tecla spc" ":pausa 0.1"); done
juega bloques ":escribe \\juegos\\bloques /prueba /s=1" ":tecla ret" ":pausa 4" ":captura jb1" ":tecla ret" ":pausa 2" ":tecla up" ":tecla right" ":tecla right" ":tecla right" ":tecla right" ":tecla right" \
  ":pausa 1" ":captura jb2" ":tecla spc" ":pausa 0.2" ":captura jb3" ":pausa 1" ":tecla c" ":pausa 1" ":tecla f12" ":captura jb4" ":pausa 4" \
  "${soltar[@]}" ":pausa 2"
mira "Bloques: la Viga, girada y a la derecha, quita las cuatro filas a la vez (¡Cuádruple!: 800 puntos y 2 por casilla al soltarla; el pozo queda limpio: 1.000 más)" \
  "^bloques: 4 filas a la vez (llevo 4); 8[0-2][0-9] puntos$" "^bloques: estado=juega puntos=18[0-2][0-9] filas=4 nivel=1 piezas=3 pieza=[A-Za-zñó]* reserva=Viga"
miracap "Bloques: el pozo con sus cuatro filas casi llenas (azulejos con su dibujo), la reserva, las tres siguientes con su nombre y los puntos" jb2 \
  negrita 195 108 210 120 Reserva negrita 522 108 538 120 Siguientes texto 178 200 190 212 Puntos nrgb 310 441 111C2E 12
miracap "Bloques: las cuatro filas se han ido (el pozo, vacío)" jb3 rgb 310 441 111C2E 12 rgb 330 461 111C2E 12 rgb 480 481 111C2E 12
mira "Bloques: Espacio una y otra vez hasta que no cabe una pieza: se acaba la partida, y es récord" "^bloques: fin de la partida: [0-9]* puntos, 4 filas, nivel 1, [0-9]* piezas; récord 1$"
if python3 -I - $Y/bloques.wav <<'PY'
import sys, wave, struct
w = wave.open(sys.argv[1]); r = w.getframerate(); c = w.getnchannels()
d = w.readframes(w.getnframes()); s = struct.unpack('<%dh' % (len(d) // 2), d)
v = [max(abs(x) for x in s[i:i + r * c // 10]) for i in range(0, len(s) - r * c // 10, r * c // 10)]
# (la música empieza con la partida —tras el sonido de inicio del escritorio y
# un silencio—: hay cuatro segundos seguidos en los que casi todas las décimas suenan)
ok = any(sum(1 for x in v[i:i + 40] if x > 800) >= 34 for i in range(30, len(v) - 40))
sys.exit(0 if ok else 1)
PY
then ok "Bloques: la música (la suya, sintetizada) sale por la tarjeta (HD Audio), seguida"
else ko "Bloques: la música no sale por la tarjeta"; fi
# Culebra (paso a paso): dónde está la fruta, y luego hasta ella y contra el borde
juega culebra1 ":escribe \\juegos\\culebra /p /s=1" ":tecla ret" ":pausa 4" ":tecla ret" ":pausa 1" ":tecla f12" ":pausa 1"
f=$(estado_de culebra fruta)
c=()
if [ -n "$f" ]; then
  fx=${f%,*}; fy=${f#*,}
  if [ "$fy" -lt 9 ]; then for ((k = 0; k < 9 - fy; k++)); do c+=(":tecla up"); done
  elif [ "$fy" -gt 9 ]; then for ((k = 0; k < fy - 9; k++)); do c+=(":tecla down"); done
  elif [ "$fx" -lt 13 ]; then c+=(":tecla up"); fi
  if [ "$fx" -gt 13 ]; then for ((k = 0; k < fx - 13; k++)); do c+=(":tecla right"); done
  else for ((k = 0; k < 13 - fx; k++)); do c+=(":tecla left"); done; fi
  [ "$fy" = 9 ] && [ "$fx" -lt 13 ] && c+=(":tecla down")
fi
juega culebra ":escribe \\juegos\\culebra /p /s=1" ":tecla ret" ":pausa 4" ":tecla ret" ":pausa 1" "${c[@]}" ":pausa 1" ":tecla f12" ":captura jc1" \
  ":tecla up" ":tecla up" ":tecla up" ":tecla up" ":tecla up" ":tecla up" ":tecla up" ":tecla up" ":tecla up" ":tecla up" ":tecla up" ":tecla up" ":tecla up" ":tecla up" ":tecla up" ":tecla up" \
  ":tecla up" ":tecla up" ":tecla up" ":tecla up" ":pausa 1" ":captura jc2"
mira "Culebra: hasta la fruta (en ${f:-?}, con las flechas, paso a paso): se la come (10 puntos) y crece; y contra el borde, se acaba (récord)" \
  "^culebra: estado=1 puntos=10 largo=[45] frutas=1 " "^culebra: fin de la partida: 10 puntos, 1 frutas, nivel 1; récord 1$"
miracap "Culebra: el tablero de damero, la culebra (cabeza azul petróleo, collar ocre) y los puntos" jc1 negrita 114 106 126 118 "Puntos: 10"
# Campo de minas: el tablero de prueba (dos minas en dos esquinas): Intro en la del medio lo levanta todo
juega minas ":escribe \\juegos\\minas /p" ":tecla ret" ":pausa 4" ":tecla ret" ":pausa 2" ":tecla f12" ":captura jn1"
mira "Campo de minas: el tablero de prueba, ganado de un golpe (la primera nunca tiene mina; lo de alrededor se levanta solo)" \
  "^minas: ganada en [0-9]* s (Prueba); récord 1$" "^minas: estado=2 tablero=Prueba 9x9 minas=2 banderas=2 levantadas=79 "
# Solitario: todo subido menos los reyes; S los sube y se gana
juega solitario ":escribe \\juegos\\solitari /casi /s=1" ":tecla ret" ":pausa 5" ":captura js1" ":escribe s" ":pausa 2" ":tecla f12" ":captura js2"
mira "Solitario: «S» sube las cartas que se pueden (los cuatro reyes) y la partida se gana" "^solitario: ¡ganada! 4 movimientos en [0-9]* s (baraja francesa)$" \
  "^solitario: estado=1 baraja=francesa movimientos=4 mazo=0 descarte=0 salidas=52 "
miracap "Solitario: la ventana con su menú, las cartas de Hispa-386 (los reyes, de dos cabezas) en las columnas y lo subido arriba" js1 texto 88 62 100 74 Juego
# Tragabolas: a la izquierda unos segundos: bolitas
juega tragabolas ":escribe \\juegos\\tragabol /s=1" ":tecla ret" ":pausa 4" ":tecla left" ":pausa 3" ":tecla f12" ":captura jt1"
l=$(estado_de tragabolas puntos)
if [ -n "$l" ] && [ "$l" -ge 20 ] && grep -q "^tragabolas: jardín 1 (vuelta 1): 208 bolitas$" $S; then ok "Tragabolas: el jardín 1 (el de Hispa-386: 208 bolitas y campanas), el Tragón a la izquierda: $l puntos"
else ko "Tragabolas: «$l» puntos: $(grep "^tragabolas" $S | tail -n 2 | tr '\n' ' ')"; fi
# Almacén: el primer nivel, en tres pasos
juega almacen ":escribe \\juegos\\almacen" ":tecla ret" ":pausa 4" ":tecla right" ":tecla right" ":tecla right" ":pausa 1" ":tecla f12" ":captura ja1"
mira "Almacén: el primer nivel, resuelto en tres pasos y dos empujones (y la marca, guardada)" "^almacen: nivel 1 resuelto: 3 movimientos, 2 empujones$" \
  "^almacen: estado=1 nivel=1 movimientos=3 empujones=2 cajas=1/1"
# Rompemuros: la pala sola, unos segundos
juega rompemuros ":escribe \\juegos\\rompemur /sola /s=3" ":tecla ret" ":pausa 20" ":tecla f12" ":captura jr1"
l=$(estado_de rompemuros quedan); n=$(estado_de rompemuros puntos)
if [ -n "$l" ] && [ "$l" -lt 65 ] && [ "$n" -gt 0 ] && [ "$(estado_de rompemuros bolas)" -ge 1 ]; then ok "Rompemuros: la bola en juego rompe ladrillos (el nivel 1: quedan $l de 65; $n puntos)"
else ko "Rompemuros: quedan «$l», puntos «$n»"; fi
miracap "Rompemuros: el muro, la pala, la bola y los puntos" jr1 negrita 100 80 300 110 "Puntos:"
# el pinball, a 1024 x 600 (la pantalla del HP Mini) y a pantalla completa: lanzado a mano y luego, en otra, jugando solo
JARR="modo=1024x600" JMEM=256 PANTALLA=1024x600 juega pinball1 ":escribe \\juegos\\pinball /s=2" ":tecla ret" ":pausa 12" ":captura jp1" ":tecla ret" ":pausa 2" \
  ":monitor sendkey spc 900" ":pausa 2" ":captura jp2" ":monitor sendkey z 300" ":pausa 1" ":monitor sendkey minus 300" ":pausa 1" ":tecla f12"
mira "Hispa-Pinball: la mesa en perspectiva a 1024 x 600, a pantalla completa; Intro y la bola al lanzador; Espacio mantenido la lanza con toda la fuerza" \
  "^juego: pantalla completa, 1024 x 600$" "^pinball: mesa de 1024 x 600" "^pinball: bola 1 al lanzador$" "^pinball: bola lanzada (fuerza 100 %)$"
miracap "Hispa-Pinball: la mesa en perspectiva (las hondas blancas), y a la derecha el marcador con las misiones" jp1 \
  negrita 776 170 790 182 Misiones texto 792 192 804 204 "1. Enciende F-E-R-I-A" rgb 330 440 D3D1C8 30
JARR="modo=1024x600" JMEM=256 PANTALLA=1024x600 juega pinball2 ":escribe \\juegos\\pinball /solo /s=3" ":tecla ret" ":pausa 40" ":tecla f12" ":captura jp3"
n=$(estado_de pinball puntos)
if grep -q "^pinball: el flipper [a-z]* golpea la bola" $S && [ -n "$n" ] && [ "$n" -gt 0 ]; then
  ok "Hispa-Pinball, jugando solo: los flippers golpean la bola, puntos ($n), $(grep -c "^pinball: montaña rusa" $S) veces la montaña rusa; $(estado_de pinball estado), bola $(estado_de pinball bola)"
else ko "Hispa-Pinball, jugando solo: $(grep "^pinball" $S | tail -n 3 | tr '\n' ' ')"; fi
# los récords, en \AJUSTES\JUEGOS del disco
for f in BLOQUES:'^Puntos1=[0-9]*|' CULEBRA:'^Sin1=10|' MINAS:'^Prueba1=[0-9]*|' SOLITARI:'^Francesa ganadas=1' ALMACEN:'^Mejor1=3'; do
  rm -f $Y/rec.txt
  mcopy -n -i $Y/j.img "::AJUSTES/JUEGOS/${f%%:*}.REC" $Y/rec.txt > /dev/null 2>&1
  if [ -f $Y/rec.txt ] && tr -d '\r' < $Y/rec.txt | grep -q "${f#*:}"; then ok "los récords: \\AJUSTES\\JUEGOS\\${f%%:*}.REC (sin cuentas: la carpeta de ajustes de la unidad del sistema) lleva «$(tr -d '\r' < $Y/rec.txt | grep "${f#*:}" | head -n 1)»"
  else ko "los récords: ${f%%:*}.REC: $( [ -f $Y/rec.txt ] && tr -d '\r' < $Y/rec.txt | tr '\n' ' ')"; fi
done
# sin el servicio de sonido: en silencio, y sin fallar
cp $Y/j.img $Y/k.img
mdel -i $Y/j.img ::SONIDO.BIN
juega mudo ":escribe \\juegos\\culebra /s=1" ":tecla ret" ":pausa 4" ":tecla ret" ":pausa 1" ":tecla f12"
mira "sin el servicio de sonido (SONIDO.BIN): el juego lo dice y juega en silencio" "^juego: sin sonido" "^culebra: estado=1 .* sonido=0$"
cp $Y/k.img $Y/j.img
# ---- 4. lo que gastan, en un Atom N270 de QEMU ----
for g in "pinball /solo /mide /s=3:pinball:1024x600:15" "rompemur /sola /mide /s=3:rompemuros:800x600:10"; do
  IFS=: read -r o q pan tope <<< "$g"
  JARR="modo=$pan" JMEM=256 PANTALLA=$pan JQEMU="-cpu n270 -icount shift=0" juega atom_$q ":escribe \\juegos\\$o" ":tecla ret" ":pausa 30"
  l=$(grep "^juego: gasta el" $S | tail -n 2 | head -n 1)
  n=$(echo "$l" | sed -n 's/^juego: gasta el \([0-9]*\),\([0-9]\) %.*/\1.\2/p')
  if [ -n "$n" ] && awk -v n=$n -v t=$tope 'BEGIN { exit !(n < t) }' && echo "$l" | grep -q "(60 cuadros por segundo"; then
    ok "en el Atom N270 de QEMU (-icount), $q a $pan: ${l#juego: }"
  else ko "en el Atom N270 de QEMU, $q: «$l» (se esperaba menos del $tope % y 60 cuadros por segundo)"; fi
done
if ! cat $Y/*.txt 2> /dev/null | grep -q "excepción\|^\*\*\*"; then ok "los juegos: ni una excepción ni una queja del núcleo"
else ko "el núcleo se ha quejado:"; cat $Y/*.txt | grep "excepción\|^\*\*\*" | head -4 | sed 's/^/        /'; fi
}
