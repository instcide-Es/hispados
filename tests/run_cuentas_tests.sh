#!/bin/bash
# Prueba de Cuentas (CUENTAS.HVP), la hoja de cálculo: que cuente bien.
#
# Abre Hispa Ventanas con Cuentas y una hoja (.CSV) llena de fórmulas, y mira
# en la pantalla el resultado de cada celda: decimales exactos (0,1 + 0,2),
# redondeo de multiplicaciones y divisiones, signos, paréntesis y prioridad,
# las funciones (SUMA, PROMEDIO, MAX, MIN, CONTAR, REDONDEAR, ABS) y «%»,
# números que casi no caben en la celda (se quitan decimales) y los que no
# caben (almohadillas, nunca un número cortado), los errores (#ERROR, #CIRC,
# #REF), una cadena de cien fórmulas y una serie que suma las dos celdas de
# antes. Luego teclea una cuenta a mano, guarda y mira el .CSV.
# (El cálculo en sí se prueba a fondo, contra un modelo exacto, con
# tests/tools/cuentas_calc.py, que esta prueba también pasa.)
#
# Necesita qemu-system-i386, mtools, socat e ImageMagick (como las demás).
set -u
IMG=${1:-build/hispados.img}
. tests/tools/hvp_arnes.sh
HVP=$B/CUENTAS.HVP
[ -f $HVP ] || NASM=${NASM:-nasm} python3 tools/mkhvp.py programas/cuentas.asm $HVP > /dev/null

# la celda COL (0=A) FILA (0=la 1) enseña TEXTO: los números, arrimados a la
# derecha y sin nada delante; con «izq», arrimado a la izquierda
celda() {
  local x0=$(( 118 + 88 * $1 )) y=$(( 136 + 18 * $2 )) t="$3" x
  if [ "${4:-}" = izq ]; then
    chk $T/h1.ppm texto $(( x0 + 3 )) $y 0 9 fina "$t"
  elif [ ${#t} -ge 10 ]; then
    chk $T/h1.ppm texto $(( x0 + 85 - 8 * ${#t} )) $y 0 9 fina "$t"
  else
    chk $T/h1.ppm texto $(( x0 + 85 - 8 * ${#t} - 8 )) $y 0 9 fina " $t"
  fi
}

echo "== Cuentas: el cálculo, contra el modelo exacto =="
python3 tests/tools/cuentas_calc.py ${CUENTAS_AZAR:-6000} || fail=1

echo "== Cuentas: una hoja de fórmulas =="
prepara $HVP
cat > $T/h.txt <<'CSV'
12,5;=A1*3;=A1-B1
0,1;=A2+0,2;=SUMA(A1:A1)
10;=A3/3;=7/2
=A3/3*3;=2/3;=-7/2
100;=A5*21/100;=1/3+1/3+1/3
1234567,89;=A6*2;=2*3+4*5
-5;=A7*A7;=10-2-3
=SUMA(A1:A3);=SUMA(A1:B2);=100/8/5
=1/0;=A9+1;=C9+1
=(1+2)*(3+4);=-(2+3)*2;=Z101
=46000000000*2;=0,00004+0;texto
=0,00005*1;=1,005*100;=C11+5
3,14159265;=A13*2;=0,125*8
=19,99*3;=0,07*100;=1,23456789*1
=1000000*1000000;=50000*50000;=-0,00001*1
=123456,789*1;=1234567,891*1;=2,5*2,5
=-1234567,891*1;=a1+b1;=9,995*1
CSV
# a la derecha: las funciones (D), una cadena de cien fórmulas (E: cada una,
# la de arriba más uno), lo que vale su última celda y su suma (F), y una
# serie donde cada celda suma las dos de antes (G), que sin apuntar lo ya
# calculado no acabaría nunca
python3 - $T/h.txt > $T/HOJA.CSV <<'PY'
import sys
base = [l.rstrip('\n').split(';') for l in open(sys.argv[1])]
d = ['=PROMEDIO(A1:A3)', '=MAX(A1:A7)', '=MIN(A1:A7)', '=CONTAR(A1:C3)', '=A5*21%', '=REDONDEAR(A3/3;2)',
     '=ABS(A7)', '=suma(a1:a3;c6)']
f = ['=E100', '=G40', '=SUMA(E1:E100)', '=PROMEDIO(E1:E100)']
for i in range(100):
    fila = (base[i] if i < len(base) else ['', '', ''])
    fila += ['"%s"' % d[i] if i < len(d) and ';' in d[i] else d[i] if i < len(d) else '']
    fila += ['1' if i == 0 else '=E%d+1' % i]
    fila += [f[i] if i < len(f) else '']
    fila += ['1' if i < 2 else '=G%d+G%d' % (i, i - 1)] if i < 40 else ['']
    sys.stdout.write(';'.join(fila) + '\r\n')
PY
mcopy -o -i $T/f.img $T/HOJA.CSV ::HOJA.CSV
printf '[Centro]\r\nReabrir=A:\\HISPADOS\\PROGS\\CUENTAS.HVP A:\\HOJA.CSV\r\n' > $T/CENTRO.INI
mcopy -o -i $T/f.img $T/CENTRO.INI ::HISPADOS/CENTRO.INI
{ arranca
  tecla right right right down down down down down down down down; aparca; shot h1   # (a una celda vacía)
  escribe "=a6*0,21"; tecla ret; escribe "=d9+a6"; tecla ret
  escribe "=redondear(d9;1)"; tecla ret; aparca; shot h2
  tecla ctrl-g; sleep 2.5
  sal; } | run 150 -vga std
muestra h1 cuentas_hoja; muestra h2 cuentas_tecleado
chk $T/h1.ppm titulo 80 50 activa "Cuentas - A:\\HOJA.CSV"
#      A                      B                       C
celda 0 0 "12,5";        celda 1 0 "37,5";        celda 2 0 "-25"
celda 0 1 "0,1";         celda 1 1 "0,3";         celda 2 1 "12,5"
celda 0 2 "10";          celda 1 2 "3,3333";      celda 2 2 "3,5"
celda 0 3 "10";          celda 1 3 "0,6667";      celda 2 3 "-3,5"
celda 0 4 "100";         celda 1 4 "21";          celda 2 4 "1"
celda 0 5 "1234567,89";  celda 1 5 "2469135,78";  celda 2 5 "26"
celda 0 6 "-5";          celda 1 6 "25";          celda 2 6 "5"
celda 0 7 "22,6";        celda 1 7 "50,4";        celda 2 7 "2,5"
celda 0 8 "#ERROR" izq;  celda 1 8 "#ERROR" izq;  celda 2 8 "#CIRC" izq
celda 0 9 "21";          celda 1 9 "-10";         celda 2 9 "#REF" izq
celda 0 10 "##########"; celda 1 10 "0";          celda 2 10 "texto" izq
celda 0 11 "0,0001";     celda 1 11 "100,5";      celda 2 11 "5"
celda 0 12 "3,1416";     celda 1 12 "6,2832";     celda 2 12 "1"
celda 0 13 "59,97";      celda 1 13 "7";          celda 2 13 "1,2346"
celda 0 14 "#ERROR" izq; celda 1 14 "2500000000"; celda 2 14 "0"
celda 0 15 "123456,789"; celda 1 15 "1234567,89"; celda 2 15 "6,25"
celda 0 16 "-1234567,9"; celda 1 16 "50";         celda 2 16 "9,995"
#      D: las funciones           E: la cadena      F
celda 3 0 "7,5333";      celda 4 0 "1";           celda 5 0 "100"         # la celda 100 de la cadena
celda 3 1 "1234567,89";  celda 4 1 "2";           celda 5 1 "102334155"   # la 40 de la serie
celda 3 2 "-5";          celda 4 2 "3";           celda 5 2 "5050"
celda 3 3 "9";           celda 4 3 "4";           celda 5 3 "50,5"
celda 3 4 "21";          celda 3 5 "3,33";        celda 3 6 "5";          celda 3 7 "48,6"
celda 4 16 "17"
# lo tecleado a mano: 21 % de 1234567,89 y su suma
cp $T/h2.ppm $T/h1.ppm
celda 3 8 "259259,257"                            # 259259,2569
celda 3 9 "1493827,15"                            # 1493827,1469
celda 3 10 "259259,3"
if mtype -i $T/f.img ::HOJA.CSV | tr -d '\r' | sed -n '6p;9,11p' | cut -d';' -f1-5 | tr '\n' '|' | grep -qxF '1234567,89;=A6*2;=2*3+4*5;"=REDONDEAR(A3/3;2)"|=1/0;=A9+1;=C9+1;=a6*0,21;=E8+1|=(1+2)*(3+4);=-(2+3)*2;=Z101;=d9+a6;=E9+1|=46000000000*2;=0,00004+0;texto;"=redondear(d9;1)"|'; then
  ok "HOJA.CSV: guarda las fórmulas como se teclearon"
else
  ko "HOJA.CSV no es lo esperado"; mtype -i $T/f.img ::HOJA.CSV | head -3
fi

rm -rf $T
exit $fail
