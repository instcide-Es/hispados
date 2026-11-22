# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pruebas_touchpad.sh - (TOUCHPAD) la parte 50 de pruebas.sh (SOLO=touchpad),
# que lee este archivo («.»): la función parte_touchpad, con los ayudantes de
# pruebas.sh (ok, ko, mira, miracap, saca) y sus carpetas ($AQUI, $P).
#
# El touchpad (nucleo32/touchpad.cas, raton.cas, RATON.BIN y Panel de control >
# Ratón). QEMU no tiene touchpad Synaptics, así que:
#   1. en Linux, con corre32: RATON /ANALIZA con los paquetes hechos a mano de
#      nucleo32/tpsintetico.py (mover, la aceleración, tocar, doble toque,
#      arrastrar, dos y tres dedos, el borde derecho arriba y abajo, la zona,
#      el borde de abajo, dos dedos, la palma, al escribir, los botones, el
#      clickpad, los descolocados, perdido, las medidas que se aprenden)
#   2. en QEMU, el ratón PS/2 de siempre: no es un touchpad (ni Synaptics, ni
#      ALPS, ni Elantech) y sigue con su rueda (IntelliMouse); RATON lo dice;
#      RATON /TOUCHPAD 3 graba lo que llega (moviendo el ratón de QEMU) en
#      TOUCHPAD.TXT, que RATON /ANALIZA sabe leer
#   3. en QEMU con «tp_simulado» (un Synaptics de mentira): RATON /INYECTA le
#      manda paquetes: el borde derecho baja el Bloc de notas, dos dedos lo
#      suben y un toque hace clic (abre el menú Archivo); y el Panel de control
#      > Ratón enseña la sección Touchpad, que se cambia y se guarda en PANEL.INI
parte_touchpad() {
echo "== Hispa-Pentium: el touchpad (Synaptics en modo absoluto: el borde para desplazar, tocar para hacer clic, dos dedos, la palma; RATON /TOUCHPAD) (TOUCHPAD) =="
local X=$P/touchpad l hubo S
rm -rf $X; mkdir -p $X/sint
export MTOOLS_SKIP_CHECK=1
G=$X
for u in uraton:RATON upanel:PANEL; do
  python3 -I $AQUI/ccastel.py $AQUI/nucleo32/${u%%:*}.cas -o $X/x.asm > /dev/null && nasm -f bin $X/x.asm -o $X/${u##*:}.BIN || { ko "${u%%:*}.cas no compila"; return; }
done
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $X/semilla.asm > /dev/null && nasm -f bin $X/semilla.asm -o $X/SEMILLA.BIN || { ko "semilla.cas no compila"; return; }

# ---- 1. con corre32: los paquetes hechos a mano ----
python3 -I $AQUI/nucleo32/tpsintetico.py prueba $X/RATON.BIN $X/sint > $X/sint.txt 2>&1
hubo=0
while IFS= read -r l; do
  case "$l" in bien:*) ok "touchpad.cas: ${l#bien: }"; hubo=1 ;; mal:*) ko "touchpad.cas: ${l#mal: }"; hubo=1 ;; esac
done < $X/sint.txt
[ $hubo = 0 ] && ko "tpsintetico.py no dice nada: $(tail -3 $X/sint.txt)"

if ! $AQUI/nucleo32/disco.sh $X/d.img 32 panel > $X/disco.txt 2>&1; then ko "no se puede hacer el disco: $(tail -2 $X/disco.txt)"; return; fi
python3 -c "
import sys
for i in range(1, 401): sys.stdout.write('linea %03d de un texto largo\r\n' % i)" > $X/LARGO.TXT
python3 -I $AQUI/nucleo32/tpsintetico.py inyecta $X
mcopy -o -i $X/d.img $X/RATON.BIN $X/PANEL.BIN $X/LARGO.TXT $X/TODO.TPD ::/

# ---- 2. el ratón PS/2 de QEMU: ni touchpad ni cambios ----
cp $X/d.img $X/r.img
CAPTURAS=$G ESCRIBE=1 DISCO=$X/r.img ARRANQUE="escritorio" MODULOS= MEMORIA=128 $AQUI/nucleo32/probar.sh $X/SEMILLA.BIN ":pausa 4" ":doble 62 185" ":pausa 3" \
  "raton" ":pausa 2" ":captura tr1" "raton /touchpad 3" ":pausa 1" ":monitor mouse_move 20 10" ":pausa 1" ":monitor mouse_move -20 -10" ":pausa 1" ":monitor mouse_move 0 0 1" ":pausa 3" ":captura tr2" \
  "type largo.txt" ":pausa 3" ":raton 300 300" ":monitor mouse_move 0 0 1" ":pausa 1" ":monitor mouse_move 0 0 1" ":pausa 1" ":captura tr3" > $X/r.sal 2>&1
tr -d '\r' < $X/r.sal > $X/r.txt
S=$X/r.txt
mira "el ratón PS/2 de QEMU sigue siendo un IntelliMouse (con rueda), tras las preguntas de Synaptics, ALPS y Elantech" "^escritorio: el ratón PS/2 tiene rueda$"
if ! grep -q "^ratón: touchpad\|^ratón: Touchpad" $S; then ok "y no se toma por un touchpad"; else ko "el ratón de QEMU se toma por un touchpad: $(grep '^ratón:' $S | head -2)"; fi
miracap "RATON: «Touchpad: ninguno» y cómo está puesto" tr1 texto 0 0 800 500 "Touchpad: ninguno"
mira "RATON /TOUCHPAD 3 graba tres segundos y lo guarda (TOUCHPAD.TXT)" "^ratón: grabando lo que llega del touchpad, 3 segundos" "^ratón: grabados [1-9][0-9]* octetos del touchpad en C:.TOUCHPAD.TXT"
miracap "y lo dice en la Consola" tr2 texto 0 0 800 500 "guardado en TOUCHPAD.TXT"
if saca $X/r.img TOUCHPAD.TXT $X/tp.txt && grep -q "^aj xmax " $X/tp.txt && grep -q "^@[0-9]* o 0[89A-F] " $X/tp.txt; then
  ok "TOUCHPAD.TXT: la cabecera (lo que se sabe del touchpad, como «aj») y cada paquete con su tic («@n o 08 14 0A 00»: los del IntelliMouse)"
  $AQUI/corre32/corre32.sh -C $X $X/RATON.BIN /ANALIZA tp.txt > $X/tp.sal 2>&1
  if grep -q "^fin: .*perdido=1" $X/tp.sal; then ok "RATON /ANALIZA lo lee entero (y, como no son del modo absoluto, «perdido»: lo que haría el núcleo, volver al relativo)"
  else ko "RATON /ANALIZA no lee TOUCHPAD.TXT: $(tail -2 $X/tp.sal)"; fi
else ko "TOUCHPAD.TXT no está o no es lo que toca: $(head -12 $X/tp.txt 2> /dev/null)"; fi
miracap "y la rueda sigue moviendo la Consola (dos pasos, seis líneas)" tr3 negrita 90 60 600 90 "Consola - historial: 6 l"
if ! grep -q "excepción\|\*\*\*" $S; then ok "ni una excepción ni una queja del núcleo"; else ko "el núcleo se ha quejado: $(grep 'excepción\|\*\*\*' $S | head -2)"; fi

# ---- 3. el touchpad de mentira (tp_simulado) ----
cp $X/d.img $X/s.img
CAPTURAS=$G ESCRIBE=1 DISCO=$X/s.img ARRANQUE="escritorio tp_simulado" MODULOS= MEMORIA=128 $AQUI/nucleo32/probar.sh $X/SEMILLA.BIN ":pausa 4" ":doble 62 185" ":pausa 3" \
  "raton /inyecta todo.tpd /tras 9" ":pausa 1" "bloc largo.txt" ":pausa 3" ":raton 300 300" ":pausa 5" ":captura ts1" ":pausa 6" ":captura ts2" ":raton 150 127" ":pausa 8" ":captura ts3" \
  ":tecla esc" ":pausa 1" ":tecla alt-f4" ":pausa 2" "panel /raton" ":pausa 4" ":raton 790 590" ":captura ts4" ":clic 246 335" ":pausa 1" \
  ":clic 640 366" ":pausa 1" ":tecla esc" ":tecla down" ":tecla down" ":pausa 1" ":clic 640 432" ":pausa 1" ":tecla esc" ":tecla down" ":pausa 1" ":raton 790 590" ":captura ts5" > $X/s.sal 2>&1
tr -d '\r' < $X/s.sal > $X/s.txt
S=$X/s.txt
mira "con «tp_simulado», el núcleo dice que hay un touchpad (de mentira)" "^ratón: touchpad simulado (tp_simulado)"
miracap "el borde derecho, de arriba abajo: el Bloc de notas baja 16 pasos de rueda, 48 líneas (la 049 arriba)" ts1 texto 110 170 400 200 "linea 049 de un texto largo"
miracap "dos dedos hacia arriba: sube 9 pasos, 27 líneas (la 022 arriba)" ts2 texto 110 170 400 200 "linea 022 de un texto largo"
miracap "un toque encima del menú Archivo: el clic lo abre" ts3 texto 0 0 800 600 "Guardar"
miracap "y lo cambiado se ve: la casilla sin marcar, «Rápida» y «Ancho (12 %)»" ts5 texto15 515 350 800 385 "Rápida" texto15 515 415 800 450 "Ancho (12 %)"
miracap "Panel de control > Ratón: la sección Touchpad (Synaptics), con lo que se marca y la velocidad y el ancho del borde" ts4 \
  negrita15 230 290 600 320 "Touchpad (Synaptics)" texto15 230 320 520 350 "Tocar para hacer clic" texto15 230 345 520 375 "Borde derecho: desplazar" negrita15 515 320 800 350 "Velocidad"
mira "tocar para hacer clic, quitado; la velocidad, Rápida; el ancho del borde, Ancho (12 %)" "^panel: el ajuste 23, a 0$" "^panel: el ajuste 27, a 7$" "^panel: el ajuste 29, a 12$"
if saca $X/s.img PANEL.INI $X/panel.ini && tr -d '\r' < $X/panel.ini > $X/panel.txt && grep -q "^touchpad_toque=0$" $X/panel.txt && grep -q "^touchpad_velocidad=7$" $X/panel.txt && \
   grep -q "^touchpad_zona=12$" $X/panel.txt && grep -q "^touchpad_borde=1$" $X/panel.txt; then ok "PANEL.INI guarda lo del touchpad (y se pone al arrancar)"
else ko "PANEL.INI no guarda lo del touchpad: $(grep touchpad $X/panel.txt 2> /dev/null | tr '\n' ' ')"; fi
if ! grep -q "excepción\|\*\*\*" $S; then ok "ni una excepción ni una queja del núcleo"; else ko "el núcleo se ha quejado: $(grep 'excepción\|\*\*\*' $S | head -2)"; fi
[ -n "${CAPTURAS_A:-}" ] && { mkdir -p "$CAPTURAS_A"; cp $G/t[rs]*.ppm "$CAPTURAS_A"/ 2> /dev/null; }
}
