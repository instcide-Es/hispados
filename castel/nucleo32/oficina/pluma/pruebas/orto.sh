#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pluma/pruebas/orto.sh [CUANTAS] [VUELTAS_FUZZ]: el corrector de la oficina
# de 32 bits (oficina/comun/orto.c) contra el de Hispa-386
# (programas/comun/ortografia.inc, ensamblado como en tests/tools/
# ortografia_prueba.py: necesita nasm y ld) con las mismas órdenes
# (orto_compara.py); el fuzz de la carga de DICCIO.DAT (orto_fuzz.py); y lo
# que tarda. Con ASan y UBSan. Dice «N bien, M mal».
set -u
AQUI=$(dirname "$(readlink -f "$0")")
OF=$(readlink -f "$AQUI/../..")
RAIZ=$(readlink -f "$OF/../../..")
T=$(mktemp -d)
trap 'rm -rf $T' EXIT
nok=0; nko=0
ok() { echo "  bien: $1"; nok=$((nok + 1)); }
ko() { echo "  MAL:  $1"; nko=$((nko + 1)); }
SRC="$OF/comun/orto.c $OF/comun/texto.c $OF/comun/so_anfitrion.c $AQUI/orto_arnes.c"
gcc -std=gnu11 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare \
  -I$OF/comun $SRC -o $T/asan -lm && gcc -std=gnu11 -O2 -I$OF/comun $SRC -o $T/rapido -lm || { ko "no se compila"; exit 1; }
r=$(python3 -I $AQUI/orto_compara.py $T/asan ${1:-20000} 2>&1 | tail -3)
case "$r" in *" 0 distintas"*) ok "igual que el de Hispa-386: $(echo "$r" | sed -n 2p)";; *) ko "distinto del de Hispa-386: $r";; esac
r=$(python3 -I $AQUI/orto_fuzz.py $T/asan ${2:-280} 2>&1 | tail -2)
case "$r" in *" 0 fallos"*) ok "fuzz de DICCIO.DAT: $(echo "$r" | head -1)";; *) ko "fuzz de DICCIO.DAT: $r";; esac
D=$RAIZ/programas/comun/DICCIO.DAT
m=$(printf 'D %s\nV 1000000 casa\nV 1000000 diciéndoselo\n' "$D" | iconv -f utf-8 -t cp437 | $T/rapido | tail -2 | awk '{print $2}' | tr '\n' ' ')
echo "  (consultar una palabra: $m microsegundos)"
python3 -I -c "
import random
random.seed(3)
w=['haver','tanbien','exelente','canpo','recivir','desir','jente','cavallo','ormiga','zapatto','gerra','keso','absorver','alludar','enpezar','diciendoselo','porqe']
print('D $D')
for i in range(20000): print('S', random.choice(w))
" > $T/s.txt
t0=$(date +%s%N); $T/rapido < $T/s.txt > $T/s.sal; t1=$(date +%s%N)
echo "  (proponer: $(( (t1 - t0) / 20000 / 1000 )) microsegundos de media)"
grep -q "^haber,hacer" $T/s.sal && ok "las propuestas («haver» -> haber, hacer...)" || ko "las propuestas"
f=$(printf 'D %s\nF Esto es una prueva de testo bien escrito y mal escrrito, con ARBOL y Madrid y madrid.\n' "$D" | iconv -f utf-8 -t cp437 | $T/asan | tail -1)
[ "$f" = "prueva,escrrito,madrid" ] && ok "las faltas de un párrafo (prueva, escrrito, madrid; ARBOL y Madrid valen)" || ko "las faltas: «$f»"
echo "== Corrector: $nok bien, $nko mal =="
exit $((nko > 0))
