#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# blocprueba/prueba_len.sh: prueba BlocLen (bloclen.cas), los colores según
# el lenguaje del Bloc de notas, con corre32 (en Linux): los .LEN de
# bloc/lenguajes, el lenguaje de cada nombre de archivo, cada muestra de
# muestras/ coloreada como dice su .esp (una letra por octeto y lo que abre y
# cierra cada línea: ver plen.cas), un .LEN roto, uno enorme y uno con el
# nombre repetido (no revientan; el primero vale), lo que tarda colorear
# 10 MB de C y que CASTEL32 saca lo mismo que ccastel.py.
# Dice «bien: ...» o «mal: ...» por cada cosa; acaba con 0 si todo va bien.
# Con REHAZ=1 escribe los .esp con lo que sale (para revisarlos a mano).
set -u
B=$(cd "$(dirname "$0")" && pwd)             # castel/nucleo32/blocprueba
C=$(cd "$B/../.." && pwd)                    # castel/
T=${BLOC_TMP:-/tmp/blocprueba}/len
L=$C/nucleo32/bloc/lenguajes
mkdir -p $T
mal=0
bien() { echo "bien: $*"; }
mal() { echo "mal: $*"; mal=1; }
# lo que sale, de la página de Hispa (la 437 con Á, Ó, €...) a UTF-8
hispa() { python3 -I -c "import sys; sys.path.insert(0, '$C/../tools'); import hispa437; sys.stdout.write(''.join(hispa437.letra(b) for b in sys.stdin.buffer.read()))"; }
P() { $C/corre32/corre32.sh -C / $T/PLEN.BIN "$@" 2> /dev/null | tr -d '\r'; }
if ! { python3 -I $C/ccastel.py $B/plen.cas -o $T/plen.asm -I $C/nucleo32 > /dev/null && nasm -f bin $T/plen.asm -o $T/PLEN.BIN; }; then
  mal "plen.cas (con bloclen.cas) no compila"; exit 1
fi
# 1. los lenguajes y el de cada nombre
P /LISTA $L/ a.cas PROG.BAS x/config.sys 'C:\DOS\AUTOEXEC.BAT' P.PY notas.txt h.htm s.css j.js k.ini z.asm m.c f.h SIN | hispa > $T/lista.txt
cat > $T/lista.esp <<'L2'
lenguajes: 10
  0 C (1) «//» «/*» «*/» distingue
  1 Castel (1) «;» «(*» «*)» distingue
  2 CSS (1) «/*» «*/»
  3 Ensamblador NASM (1) «;»
  4 Hispa-BASIC / QBasic (1) «'»
  5 HTML (1) «<!--» «-->»
  6 INI (3) «;»
  7 JavaScript (1) «//» «/*» «*/» distingue
  8 Python (2) «#» distingue
  9 Órdenes .BAT y CONFIG.SYS (0) «::»
a.cas -> Castel
PROG.BAS -> Hispa-BASIC / QBasic
x/config.sys -> Órdenes .BAT y CONFIG.SYS
C:\DOS\AUTOEXEC.BAT -> Órdenes .BAT y CONFIG.SYS
P.PY -> Python
notas.txt -> ninguno
h.htm -> HTML
s.css -> CSS
j.js -> JavaScript
k.ini -> INI
z.asm -> Ensamblador NASM
m.c -> C
f.h -> C
SIN -> ninguno
L2
if cmp -s $T/lista.txt $T/lista.esp; then bien "los 10 lenguajes de bloc/lenguajes (por orden, con cómo se pliega, sus comentarios y si distinguen mayúsculas) y el de cada nombre (por la extensión o el nombre entero)"
else mal "los lenguajes o el de cada nombre:"; diff $T/lista.esp $T/lista.txt | head -20; fi
# 2. las muestras
n=0; malas=
for f in $B/muestras/*; do
  case $f in *.esp) continue ;; esac
  P /COLOREA $L/ $f > $T/m.txt
  if [ "${REHAZ:-}" = 1 ]; then cp $T/m.txt $f.esp; fi
  if cmp -s $T/m.txt $f.esp; then n=$((n + 1)); else malas="$malas $(basename $f)"; diff $f.esp $T/m.txt | head -10; fi
done
if [ -z "$malas" ]; then bien "las $n muestras salen con sus estilos y lo que abre y cierra cada línea, como en su .esp (comentarios anidados de Castel, «sino si» y «si no:», escapes, las cadenas largas de Python y JS, HTML con atributos de dos líneas, el IF…THEN de una línea y el de bloque, END SUB, REM...)"
else mal "estas muestras no salen como su .esp:$malas"; fi
# 3. un .LEN roto, uno enorme y uno con el nombre repetido (se leen los de
# siempre: CASTEL, BASIC, NASM, C... porque corre32 no recorre carpetas)
R=$T/rotos; rm -rf $R; mkdir -p $R
head -c 20000 /dev/urandom > $R/CASTEL.LEN                        # roto: basura, sin nombre
python3 -I - $R <<'PY'
import sys, random
r = random.Random(7)
d = sys.argv[1]
with open(d + '/BASIC.LEN', 'w') as f:                           # enorme: 3 MB, 200.000 palabras
    f.write('nombre=Enorme\nextensiones=ENO\nbloque=(( ))\nlargas=' + ' '.join('x' * 9 for _ in range(10)) + '\n')
    f.write('comentario=' + ' '.join(['#'] * 20) + '\n')
    for i in range(20000):
        f.write('claves=' + ' '.join('p%d_%d' % (i, k) + 'z' * r.randrange(0, 90) for k in range(10)) + '\n')
    f.write('=' * 5000 + '\n' + 'a' * 300000 + '\n')
with open(d + '/NASM.LEN', 'w') as f:                            # el nombre de otro
    f.write('nombre=enorme\nclaves=mov\n')
with open(d + '/C.LEN', 'w') as f:
    f.write('nombre=Pequeño\nextensiones=PEQ\nclaves=hola\n')
PY
P /VARIAS $R/ $L/ $B/muestras/MUESTRA.CAS | hispa > $T/rotos.txt
if [ "$(sed -n 1p $T/rotos.txt)" = "lenguajes: 12" ] && grep -q '^  Enorme$' $T/rotos.txt && grep -q '^  Pequeño$' $T/rotos.txt \
  && tail -n $(wc -l < $B/muestras/MUESTRA.CAS.esp) $T/rotos.txt | cmp -s - $B/muestras/MUESTRA.CAS.esp; then
  bien "un .LEN de basura (no vale), uno de 3 MB con 200.000 palabras (se queda con las que caben), uno con un nombre repetido (vale el primero): no revientan, y Castel, cargado detrás, se colorea igual"
else mal "con los .LEN rotos:"; head -16 $T/rotos.txt; fi
# 4. lo que tarda: 10 MB de C
python3 -I -c "
import sys
m = open(sys.argv[1], 'rb').read()
with open(sys.argv[2], 'wb') as f:
    while f.tell() < 10 * 1024 * 1024: f.write(m)" $B/muestras/MUESTRA.C $T/GRANDE.C
r=$(P /TIEMPO $L/ $T/GRANDE.C)
ms=$(echo "$r" | sed -n 's/^tiempo: \([0-9]*\) ms.*/\1/p')
if [ -n "$ms" ] && [ "$ms" -lt 30000 ]; then bien "colorear 10 MB de C (y contar lo que abre y cierra cada línea): $ms ms en este equipo, con corre32"
else mal "colorear 10 MB de C: $r"; fi
# 5. CASTEL32
if BLOC_TMP=$T $B/igual32.sh $B/plen.cas > $T/igual.txt 2>&1; then bien "CASTEL32 saca lo mismo que ccastel.py con bloclen.cas (plen.cas)"
else mal "$(cat $T/igual.txt)"; fi
exit $mal
