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
# Después, lo demás de la hoja, en tres sesiones: una hoja .HOJ con las
# funciones nuevas, textos, fechas y formatos (y el texto largo que sigue
# sobre las celdas vacías); elegir rangos con el ratón, con Mayús y por el
# rótulo, con su suma en el pie; negrita, cursiva, color, alineación, moneda,
# porcentaje, decimales y el diálogo «Formato de celdas»; el ancho de una
# columna, arrastrando y por el menú; guardar y volver a abrir el .HOJ;
# copiar y pegar con las referencias corridas (y las de «$», quietas);
# rellenar; insertar y eliminar filas y columnas con las fórmulas corregidas;
# deshacer y rehacer; ordenar; buscar; imprimir (lo que sale por el puerto
# paralelo); los gráficos; autosuma, corregir con F2, cortar, ir a una celda
# y la fecha de hoy. Las capturas quedan en build/cuentas_*.png.
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
  local x0=$(( 98 + 88 * $1 )) y=$(( 180 + 18 * $2 )) t="$3" x
  if [ "${4:-}" = izq ]; then
    chk $T/h1.ppm texto $(( x0 + 3 )) $y 0 9 fina "$t"
  elif [ ${#t} -ge 10 ]; then
    chk $T/h1.ppm texto $(( x0 + 85 - 8 * ${#t} )) $y 0 9 fina "$t"
  else
    chk $T/h1.ppm texto $(( x0 + 85 - 8 * ${#t} - 8 )) $y 0 9 fina " $t"
  fi
}

# SOLO="imprimir graficos" bash tests/run_cuentas_tests.sh pasa solo esas partes
# (calculo hoja hoj varios senalar ordenar bordes imprimir...); sin SOLO, todas
quiero() { [ -z "${SOLO:-}" ] || [[ " $SOLO " == *" $1 "* ]]; }
prepara $HVP

# ---------------------------------------------------------------------------
# Lo nuevo: funciones, textos, fechas, formato, rangos, el archivo .HOJ...
# ---------------------------------------------------------------------------
reabre() { printf '[Centro]\r\nReabrir=A:\\HISPADOS\\PROGS\\CUENTAS.HVP %s\r\n' "$1" > $T/CENTRO.INI
           mcopy -o -i $T/f.img $T/CENTRO.INI ::HISPADOS/CENTRO.INI; }
# pulsar, arrastrar y soltar
arrastra() { ir $1 $2; mon "mouse_button 1" 0.3; ir $3 $4; mon "mouse_button 0" 0.5; }
# la celda de la captura CAP, con la columna A de ANCHO_A puntos: COL FILA
# TEXTO [izq|cen|der] [tinta fondo] [negrita]
ANCHO_A=88
cel() {
  local cap=$1 c=$2 f=$3 t="$4" al=${5:-der} fg=${6:-0} bg=${7:-9} tipo=${8:-fina} x0 an x
  if [ $c -eq 0 ]; then x0=98; an=$ANCHO_A; else x0=$(( 98 + ANCHO_A + 88 * (c - 1) )); an=${ANCHO_COL:-88}; fi
  case $al in
    izq) x=$(( x0 + 3 ));;
    cen) x=$(( x0 + (an - 8 * ${#t}) / 2 ));;
    *)   x=$(( x0 + an - 3 - 8 * ${#t} ));;
  esac
  chk $T/$cap.ppm texto $x $(( 180 + 18 * f )) $fg $bg $tipo "$t"
}
PIE="62 528 700 552 0 6"          # dónde está el pie: tinta sobre arena
hoj() { if grep -qxF -- "$1" $T/func.txt; then ok "FUNC.HOJ: «$1»"; else ko "FUNC.HOJ: falta «$1»"; fi; }

if quiero calculo; then
echo "== Cuentas: el cálculo, contra el modelo exacto =="
python3 tests/tools/cuentas_calc.py ${CUENTAS_AZAR:-6000} || fail=1
fi

if quiero hoja; then
echo "== Cuentas: una hoja de fórmulas =="
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
=(1+2)*(3+4);=-(2+3)*2;=Z1001
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
chk $T/h1.ppm titulo 60 36 activa "Cuentas - A:\\HOJA.CSV"
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
if mtype -i $T/f.img ::HOJA.CSV | tr -d '\r' | sed -n '6p;9,11p' | cut -d';' -f1-5 | tr '\n' '|' | grep -qxF '1234567,89;=A6*2;=2*3+4*5;"=REDONDEAR(A3/3;2)"|=1/0;=A9+1;=C9+1;=a6*0,21;=E8+1|=(1+2)*(3+4);=-(2+3)*2;=Z1001;=d9+a6;=E9+1|=46000000000*2;=0,00004+0;texto;"=redondear(d9;1)"|'; then
  ok "HOJA.CSV: guarda las fórmulas como se teclearon"
else
  ko "HOJA.CSV no es lo esperado"; mtype -i $T/f.img ::HOJA.CSV | head -3
fi

fi

if quiero hoj; then
echo "== Cuentas: una hoja .HOJ con funciones, textos, fechas y formatos =="
python3 - > $T/FUNC.HOJ <<'HOJ'
import sys
celdas = """A1 0 Lista de la compra de noviembre
D1 0 Total
A2 0 pan
B2 0 2
C2 0 1,2
D2 0 =B2*C2
A3 0 vino
B3 0 1
C3 0 7,5
D3 0 =B3*C3
A4 0 Pan
B4 0 3
C4 0 1,25
D4 0 =B4*C4
A5 0 leche
B5 0 6
C5 0 0,89
D5 0 =B5*C5
A6 0 =CONTAR.SI(A2:A5;"pan")
B6 0 =SUMAR.SI(A2:A5;"pan";D2:D5)
C6 0 =SI(D3>D2;"caro";"barato")
D6 0 =A2&" y "&A3
A7 0 =RAIZ(16)
B7 0 =2^10
C7 0 =RESIDUO(10;3)
D7 0 =ENTERO(-2,5)
A8 0 =MAYUSC(A3)
B8 0 =LARGO(A1)
C8 0 =Y(B2>1;C2<2)
D8 640 16/11/2026
A9 0 =D8+30
B9 0 =$B$2+B$3+$B4
C9 0 =CONTARA(A1:A5)
D9 0 =SI(A9>D8;"después";"antes")
A10 2176 =SUMA(D2:D5)
B10 2433 =SUMA(D2:D5)
C10 2560 =C5
D10 0 =NO(FALSO)+VERDADERO
A11 0 =MAX(B2:B5;10)&"x"
B11 0 =PROMEDIO(B2:B5)
C11 0 =D8
D11 640 =D8+45
F1 2 Columna ancha"""
t = 'HOJA 1\r\nM EUR\r\nA F1 120\r\nesto no se entiende y se salta\r\n' + ''.join('C %s\r\n' % l for l in celdas.split('\n'))
sys.stdout.buffer.write(t.encode('cp437'))
HOJ
mcopy -o -i $T/f.img $T/FUNC.HOJ ::FUNC.HOJ
reabre 'A:\FUNC.HOJ'
{ arranca
  tecla ctrl-end down down down; aparca; shot f1                 # (el cursor, a una celda vacía)
  arrastra 230 207 330 261; aparca; shot f2                       # de B2 a C5, con el ratón
  clic 230 170; aparca; shot f2b                                  # el rótulo de la columna B: entera
  tecla ctrl-home down right shift-down shift-down shift-down; aparca; shot f2c   # B2:B5, con Mayús
  tecla ctrl-n right shift-down shift-down shift-down; clic 222 124   # negrita; C2:C5, moneda
  tecla ctrl-home down down; clic 355 124                         # A3, en rojo
  tecla up; clic 164 124                                          # A2, centrado
  tecla down down down ctrl-k                                     # A5, en cursiva
  arrastra 186 170 226 170                                        # la columna A, 40 puntos más ancha
  tecla ctrl-home right right right right; escribe "0,21"; tecla ret up
  clic 248 124; clic 274 124                                      # E1: por cien, y un decimal más
  tecla down; escribe "1234567,5"; tecla ret up
  clic 367 71; sleep 0.8; clic 380 93; sleep 1.5                  # Formato > Celdas...
  clic 214 257; aparca; shot f2d; clic 343 430; sleep 1           # con millares; Aceptar
  tecla ctrl-home right down down down down down down down down down down down down shift-right shift-down
  clic 367 71; sleep 0.8; clic 380 345; sleep 1                   # B13:C14, con un borde alrededor
  tecla ctrl-end down down down; aparca; shot f3
  tecla ctrl-g; sleep 2.5
  clic 686 48; sleep 1.5; aparca; shot f4                         # la ventana, a toda la pantalla
  sal; } | run 240 -vga std
muestra f1 cuentas_funciones; muestra f2 cuentas_rango; muestra f2d cuentas_formato_dlg; muestra f3 cuentas_formato; muestra f4 cuentas_grande
chk $T/f1.ppm titulo 60 36 activa "Cuentas - A:\\FUNC.HOJ"
chk $T/f1.ppm texto 101 180 0 9 fina "Lista de la compra de noviembre"     # sigue sobre B1 y C1, vacías
cel f1 3 0 "Total" izq
cel f1 0 1 "pan" izq;     cel f1 1 1 "2";      cel f1 2 1 "1,2";    cel f1 3 1 "2,4"
cel f1 0 5 "2";           cel f1 1 5 "6,15";   cel f1 2 5 "caro" izq; cel f1 3 5 "pan y vino" izq
cel f1 0 6 "4";           cel f1 1 6 "1024";   cel f1 2 6 "1";      cel f1 3 6 "-3"
cel f1 0 7 "VINO" izq;    cel f1 1 7 "31";     cel f1 2 7 "1";      cel f1 3 7 "16/11/2026"
cel f1 0 8 "46372";       cel f1 1 8 "6";      cel f1 2 8 "5";      cel f1 3 8 "después" izq
cel f1 0 9 "18,99";       cel f1 1 9 "18,99 EUR" der 0 9 negrita;   cel f1 2 9 "89,00%"; cel f1 3 9 "2"
cel f1 0 10 "10x" izq;    cel f1 1 10 "3";     cel f1 2 10 "46342"; cel f1 3 10 "31/12/2026"
chk $T/f1.ppm nobusca 540 178 700 198 0 9 "Columna ancha"      # (en cursiva: no es la letra normal)
# lo elegido con el ratón: en petróleo con letra clara (menos la celda activa), y su suma en el pie
cel f2 1 2 "1" der 8 2;  cel f2 2 2 "7,5" der 8 2;  cel f2 1 3 "3" der 8 2;  cel f2 2 3 "1,25" der 8 2
chk $T/f2.ppm busca $PIE "B2:C5    Suma: 22,84    Promedio: 2,855    Números: 8"
chk $T/f2b.ppm busca $PIE "B1:B1000    Suma: 1101,14    Promedio: 110,114    Números: 10"
chk $T/f2c.ppm busca $PIE "B2:B5    Suma: 12    Promedio: 3    Números: 4"
# los formatos puestos a mano (la columna A mide ahora 128)
ANCHO_A=128
cel f3 1 1 "2" der 0 9 negrita;   cel f3 1 4 "6" der 0 9 negrita
cel f3 2 1 "1,20 EUR";  cel f3 2 2 "7,50 EUR";  cel f3 2 4 "0,89 EUR"
cel f3 0 2 "vino" izq 13 9
cel f3 0 1 "pan" cen
cel f3 4 0 "21,0%";     cel f3 4 1 "1.234.568"
cel f3 3 1 "2,4";       cel f3 3 7 "16/11/2026"
chk $T/f3.ppm nobusca 98 250 230 272 0 9 "leche"               # en cursiva
chk $T/f4.ppm busca 0 420 40 560 0 15 "21"                     # maximizada: se ven más filas
mtype -i $T/f.img ::FUNC.HOJ | tr -d '\r' > $T/func.txt
hoj "HOJA 1"; hoj "M EUR"; hoj "A A1 128"; hoj "A F1 120"
hoj "C B2 1 2"; hoj "C C2 2432 1,2"; hoj "C A3 16 vino"; hoj "C A2 8 pan"; hoj "C A5 2 leche"
hoj "C E1 1536 0,21"; hoj "C E2 256 1234567,5"; hoj "C D8 640 16/11/2026"; hoj "C B10 2433 =SUMA(D2:D5)"
hoj 'C B6 0 =SUMAR.SI(A2:A5;"pan";D2:D5)'; hoj "C F1 2 Columna ancha"
hoj "C B12 16384 "; hoj "C A13 32768 "; hoj "C C13 32768 "; hoj "C B14 16384 "; hoj "C C14 49152 "   # las rayas del borde
chk $T/f3.ppm rgb 240 431 181710 "la raya de abajo del borde, en tinta"
chk $T/f3.ppm rgb 240 413 8F7F68 "y la raya de en medio, de las de la rejilla"
fi

if quiero hoj; then
echo "== Cuentas: abrir lo guardado; copiar y pegar, rellenar, insertar, eliminar, ordenar, buscar, imprimir =="
rm -f $T/prn.txt
{ arranca
  tecla ctrl-end down down down; aparca; shot g1
  tecla ctrl-home down right right right shift-down shift-down shift-down ctrl-c   # D2:D5...
  tecla right right ctrl-v                                        # ...a F2: las fórmulas, corridas
  tecla ctrl-home right down down down down down down down down ctrl-c   # B9, con sus «$»...
  tecla right down ctrl-v                                         # ...a C10
  tecla ctrl-home right right right right down down down down down down down down down down down down
  escribe "=b2*2"; tecla ret up shift-down shift-down shift-down ctrl-j   # E13, rellenada hasta E16
  tecla ctrl-home down down; aparca; shot g2
  clic 291 71; sleep 0.8; clic 305 93; sleep 1                    # una fila nueva en la 3
  aparca; shot g3; tecla ctrl-z; shot g3z; tecla ctrl-y; shot g3y
  tecla ctrl-home right right; clic 175 71; sleep 0.8; clic 200 297; sleep 1   # fuera la columna C
  tecla ctrl-home down shift-down shift-down shift-down shift-down shift-right
  clic 401 96; sleep 1                                            # A2:B6, de menor a mayor
  tecla ctrl-home; aparca; shot g4
  tecla ctrl-b; sleep 1; escribe "vino"; tecla ret; sleep 1; aparca; shot g5
  tecla ctrl-home ctrl-p; sleep 4; shot g6; tecla ret; sleep 1
  tecla ctrl-g; sleep 2.5
  tecla ctrl-home down shift-down shift-down shift-down shift-right             # A2:B5: los nombres y las cantidades
  clic 291 71; sleep 0.8; clic 330 189; sleep 1.5; aparca; shot k1                 # Insertar > Gráfico de barras
  clic 300 48; clic 291 71; sleep 0.8; clic 330 209; sleep 1.5; aparca; shot k2   # ...de líneas
  clic 300 48; clic 291 71; sleep 0.8; clic 330 229; sleep 1.5; aparca; shot k3   # ...de sectores
  sal; } | PARALELO=file:$T/prn.txt run 300 -vga std
muestra k1 cuentas_barras; muestra k2 cuentas_lineas; muestra k3 cuentas_sectores
muestra g2 cuentas_pegado; muestra g3 cuentas_insertada; muestra g4 cuentas_ordenada; muestra g6 cuentas_impresa
chk $T/g1.ppm igual 62 160 640 340 $T/f3.ppm 62 160 30         # se abre como se guardó
ANCHO_A=128
ANCHO_COL=120 cel g2 5 1 "2962962"                             # F2: =D2*E2
cel g2 2 9 "15,5"                                              # C10: =$B$2+C$3+$B5
cel g2 4 12 "4";  cel g2 4 13 "2";  cel g2 4 14 "6";  cel g2 4 15 "12"
# la fila nueva: lo de debajo, una fila más abajo, y las fórmulas, corregidas
cel g3 0 3 "vino" izq 13 9;  cel g3 3 3 "7,5";  cel g3 0 6 "2";  cel g3 1 6 "6,15"
chk $T/g3z.ppm igual 62 160 640 340 $T/g2.ppm 62 160 0         # deshecha
chk $T/g3y.ppm igual 62 160 640 340 $T/g3.ppm 62 160 0         # y rehecha
# sin la columna C y con A2:B6 ordenado: leche, pan, Pan, vino y la fila vacía
cel g4 1 1 "6" der 0 9 negrita;  cel g4 0 2 "pan" cen;  cel g4 1 2 "2" der 0 9 negrita
cel g4 0 3 "Pan" izq;  cel g4 0 4 "vino" izq 13 9;  cel g4 2 1 "#REF" izq
chk $T/g5.ppm busca $PIE "A5: un texto"                        # lo encontrado
chk $T/g6.ppm busca 150 250 650 310 0 8 "La hoja se ha enviado a la impresora."
mtype -i $T/f.img ::FUNC.HOJ | tr -d '\r' > $T/func.txt
hoj "C E2 0 =C2*D2"; hoj "C E6 0 =C6*D6"                       # (lo pegado en F, ahora en E)
hoj 'C B10 0 =$B$2+B$4+$B5'                                    # B9, una fila más abajo
hoj "C D14 0 =B2*2"; hoj "C D17 0 =B6*2"                       # lo rellenado
hoj 'C A7 0 =CONTAR.SI(A2:A6;"pan")'; hoj 'C B7 0 =SUMAR.SI(A2:A6;"pan";C2:C6)'
hoj "C C5 0 =B5*#REF"; hoj "C A5 16 vino"; hoj "C A2 2 leche"; hoj "C B2 1 6"; hoj "A A1 128"
if python3 - $T/prn.txt <<'IMP'
import re, sys
d = open(sys.argv[1], 'rb').read()
ok = d.startswith(b'\x1b@') and d.endswith(b'\x0c')
l = d[2:].split(b'\r\n')
p = [re.sub(rb'\x1b[EF45]', b'', x) for x in l]              # sin los códigos de negrita y cursiva
# la cabecera: el nombre, la fecha y la página; y una línea en blanco
ok = ok and p[0].startswith(b'FUNC.HOJ ') and re.fullmatch(rb'\d\d/\d\d/\d{4}', p[0][35:45]) is not None
ok = ok and p[0].endswith('Página 1 de 1'.encode('cp437')) and len(p[0]) == 80 and p[1] == b''
ok = ok and p[2].startswith(b'Lista de la compra de noviembTotal')      # (cortado donde empieza la celda siguiente)
ok = ok and p[3][:28] == b'leche'.ljust(17) + b'6'.rjust(11)            # A, de 16 letras; B, de 11
ok = ok and p[4][:28] == (b' ' * 6 + b'pan').ljust(17) + b'2'.rjust(11)  # centrado
ok = ok and p[6].startswith(b'vino') and b'#REF' in p[3]
ok = ok and all(len(x) <= 80 for x in p)
ok = ok and l[3].startswith(b'\x1b4leche\x1b5') and b'\x1bE6\x1bF' in l[3]   # la cursiva y la negrita, con ESC/P
sys.exit(0 if ok else 1)
IMP
then ok "impresora: la hoja como texto, con su cabecera, las columnas alineadas, la negrita y la cursiva"; else ko "impresora: no ha salido lo esperado"; xxd $T/prn.txt | head -30; fi
# los gráficos de A2:B5 (leche 6, pan 2, Pan 3, vino 1), en su ventana
chk $T/k1.ppm titulo 130 100 activa "Gráfico de barras"
chk $T/k1.ppm busca 132 146 628 484 0 9 "leche"
chk $T/k1.ppm busca 132 146 628 484 0 9 "vino"
chk $T/k1.ppm rgb 215 300 1F4E5A "la barra de la leche, en petróleo"
chk $T/k2.ppm titulo 130 100 activa "Gráfico de líneas"
chk $T/k3.ppm titulo 130 100 activa "Gráfico de sectores"
chk $T/k3.ppm busca 132 146 628 458 0 9 "leche: 50%"
chk $T/k3.ppm busca 132 146 628 458 0 9 "pan: 17%"
chk $T/k3.ppm busca 132 146 628 458 0 9 "vino: 8%"
fi

if quiero varios; then
echo "== Cuentas: autosuma, corregir con F2, rellenar a la derecha, cortar, insertar columnas, eliminar filas, ir a, ancho y CSV =="
reabre ''
{ arranca
  escribe "10"; tecla ret; escribe "20"; tecla ret; escribe "30"; tecla ret
  clic 364 96; sleep 0.5; aparca; shot a1; tecla ret             # Autosuma en A4: propone la suma de encima
  tecla ctrl-home f2; escribe "5"; tecla home; escribe "2"; tecla ret   # A1: de 10 a 2105, con el cursor
  tecla ctrl-home right; escribe "=a1*2"; tecla ret up shift-right shift-right ctrl-d   # B1, hasta D1
  tecla ctrl-home down ctrl-x down down down down ctrl-v          # el 20 de A2, cortado y pegado en A6
  tecla ctrl-home right; clic 291 71; sleep 0.8; clic 305 113; sleep 1   # una columna nueva en la B
  aparca; shot a2
  tecla ctrl-home down down down down down; clic 175 71; sleep 0.8; clic 200 277; sleep 1   # fuera la fila 6
  tecla f5; sleep 1; escribe "c20"; tecla ret; sleep 1            # Ir a C20
  clic 291 71; sleep 0.8; clic 305 161; sleep 1                   # Insertar > Fecha de hoy
  clic 367 71; sleep 0.8; clic 380 237; sleep 1; tecla backspace backspace; escribe "20"; tecla ret; sleep 1   # ancho: 20 letras
  aparca; shot a3
  tecla ctrl-g; sleep 1.2; escribe "x.csv"; tecla ret; sleep 2; aparca; shot a4   # avisa: el .CSV pierde los formatos
  tecla ret; sleep 2.5
  sal; } | run 240 -vga std
muestra a1 cuentas_autosuma; muestra a3 cuentas_ira; muestra a4 cuentas_aviso_csv
chk $T/a4.ppm busca 100 200 700 400 0 8 "¿Guardar como .CSV de todos modos?"
ANCHO_A=88
chk $T/a1.ppm busca 130 140 700 162 0 9 "=SUMA(A1:A3)"            # en la barra de fórmulas, sin aceptar aún
cel a2 0 0 "2105";  cel a2 0 3 "2135";  cel a2 0 5 "20"           # (A2, cortada, ya no suma)
cel a2 2 0 "4210";  cel a2 3 0 "8420";  cel a2 4 0 "16840"        # =A1*2, =C1*2, =D1*2: corridas una columna
chk $T/a3.ppm busca $PIE "C20: un número"
chk $T/a3.ppm texto 482 162 0 15 fina "D"                         # la C mide 168: el rótulo de la D, más allá
mtype -i $T/f.img ::X.CSV | tr -d '\r' > $T/x.txt
if [ "$(sed -n '1p;2p;3p;4p;6p' $T/x.txt | tr '\n' '|')" = '2105;;=A1*2;=C1*2;=D1*2|;;;;|30;;;;|=SUMA(A1:A3);;;;|;;;;|' ] &&
   sed -n '20p' $T/x.txt | grep -qE '^;;[0-9]{2}/[0-9]{2}/[0-9]{4};;$' && [ $(wc -l < $T/x.txt) -eq 20 ]
then ok "X.CSV: lo tecleado, las fórmulas corregidas y la fecha de hoy"
else ko "X.CSV no es lo esperado"; head -22 $T/x.txt; fi
fi

if quiero senalar; then
echo "== Cuentas: señalar celdas al escribir una fórmula, elegir letras, Ctrl+flechas, Mayús+Tab y deshacer varios pasos =="
reabre ''
{ arranca
  escribe "5"; tecla ret; escribe "7"; tecla ret
  escribe "="; tecla up up; escribe "+"; tecla up ret             # A3: =A1+A2, con las flechas
  tecla ctrl-home right; escribe "=suma("; arrastra 142 189 142 225; aparca; shot p1   # B1: el rango, con el ratón
  escribe ")"; tecla ret
  escribe "=suma("; tecla left shift-up; escribe ")"; tecla ret   # B2: A2 y, con Mayús, hasta A1
  tecla ctrl-home right right; escribe "abcdef"; tecla ret up
  tecla f2 shift-left shift-left shift-left; aparca; shot p2; escribe "X"   # C1: «def», elegido, por una X
  tecla home shift-right shift-right ctrl-c end ctrl-v ret        # y «ab», copiado al final
  tecla ctrl-home ctrl-down; aparca; shot p3                      # Ctrl+abajo: al final de los datos
  tecla ctrl-down; shot p4; tecla ctrl-up tab tab shift-tab; shot p5
  tecla ctrl-home right right right; escribe "1"; tecla ret; escribe "2"; tecla ret; escribe "3"; tecla ret
  tecla ctrl-z ctrl-z; aparca; shot p6; tecla ctrl-y; shot p7     # dos pasos deshechos; uno, rehecho
  tecla ctrl-g; sleep 1.2; escribe "y.csv"; tecla ret; sleep 2.5
  sal; } | run 240 -vga std
muestra p1 cuentas_senalar; muestra p2 cuentas_letras
chk $T/p1.ppm busca 130 140 700 162 0 9 "=suma(A1:A3"             # lo señalado, en la barra de fórmulas
chk $T/p2.ppm busca 130 140 700 162 8 2 "ef"                      # las letras elegidas, en claro sobre petróleo
chk $T/p3.ppm busca $PIE "A3: una fórmula"
chk $T/p4.ppm busca $PIE "A1000: celda vacía"
chk $T/p5.ppm busca $PIE "B3: celda vacía"
cel p6 3 0 "1";  cel p7 3 1 "2"
chk $T/p6.ppm rgb 400 207 FFFFFF "D2, vacía otra vez"
mtype -i $T/f.img ::Y.CSV | tr -d '\r' > $T/y.txt
if [ "$(tr '\n' '|' < $T/y.txt)" = '5;=suma(A1:A3);abcXab;1|7;=suma(A1:A2);;2|=A1+A2;;;|' ]
then ok "Y.CSV: las fórmulas señaladas, el texto corregido y lo deshecho"
else ko "Y.CSV no es lo esperado"; cat $T/y.txt; fi
fi

if quiero ordenar; then
echo "== Cuentas: ordenar por dos columnas, con títulos, y las fórmulas corregidas =="
printf 'HOJA 1\r\nC A1 0 Nombre\r\nC B1 0 Ciudad\r\nC C1 0 Edad\r\nC D1 0 Doble\r\nC A2 0 Ana\r\nC B2 0 Reus\r\nC C2 0 30\r\nC D2 0 =C2*2\r\nC A3 0 Luis\r\nC B3 0 Alba\r\nC C3 0 25\r\nC D3 0 =C3*2\r\nC A4 0 Eva\r\nC B4 0 Reus\r\nC C4 0 41\r\nC D4 0 =C4*2\r\nC A5 0 Pau\r\nC B5 0 Alba\r\nC C5 0 33\r\nC D5 0 =C5*2\r\n' > $T/ORD.HOJ
mcopy -o -i $T/f.img $T/ORD.HOJ ::ORD.HOJ
reabre 'A:\ORD.HOJ'
{ arranca
  tecla shift-down shift-down shift-down shift-down shift-right shift-right shift-right   # A1:D5
  clic 458 71; sleep 0.8; clic 480 93; sleep 1.5                  # Herramientas > Ordenar...
  tecla delete; escribe "b"; clic 414 266; escribe "c"; clic 454 266; clic 208 336   # por B; después por C, de mayor a menor; con títulos
  aparca; shot o1; clic 340 379; sleep 1.5
  tecla ctrl-end down; aparca; shot o2
  tecla ctrl-g; sleep 2.5
  sal; } | run 200 -vga std
muestra o1 cuentas_ordenar_dlg; muestra o2 cuentas_ordenada2
ANCHO_A=88
cel o2 0 0 "Nombre" izq;  cel o2 0 1 "Pau" izq;  cel o2 0 2 "Luis" izq;  cel o2 0 3 "Eva" izq;  cel o2 0 4 "Ana" izq
cel o2 2 1 "33";  cel o2 3 1 "66";  cel o2 3 4 "60"
mtype -i $T/f.img ::ORD.HOJ | tr -d '\r' > $T/func.txt
hoj "C A1 0 Nombre"; hoj "C A2 0 Pau"; hoj "C D2 0 =C2*2"; hoj "C A5 0 Ana"; hoj "C D5 0 =C5*2"
fi

if quiero bordes; then
echo "== Cuentas: ordenar por tres columnas, los demás bordes, ocho deshacer seguidos y el aviso del diario =="
printf 'HOJA 1\r\nC A1 0 x\r\nC B1 0 1\r\nC C1 0 3\r\nC A2 0 x\r\nC B2 0 1\r\nC C2 0 1\r\nC A3 0 x\r\nC B3 0 2\r\nC C3 0 5\r\nC A4 0 x\r\nC B4 0 1\r\nC C4 0 2\r\n' > $T/G.HOJ
mcopy -o -i $T/f.img $T/G.HOJ ::G.HOJ
reabre 'A:\G.HOJ'
{ arranca
  tecla shift-down shift-down shift-down shift-right shift-right   # A1:C4
  clic 458 71; sleep 0.8; clic 480 93; sleep 1.5                  # Ordenar...: por A, después por B y por C (esta, de mayor a menor)
  clic 414 266; escribe "b"; clic 414 298; escribe "c"; clic 454 298; clic 340 379; sleep 1.5
  tecla ctrl-home down down down down down right shift-right shift-down   # B6:C7, con todos los bordes
  clic 367 71; sleep 0.8; clic 380 365; sleep 1
  tecla ctrl-home right right right right right down shift-right shift-down   # F2:G3, con borde abajo
  clic 367 71; sleep 0.8; clic 380 385; sleep 1
  tecla ctrl-home down down down down down down down down down right shift-right shift-down   # B10:C11: todos, y luego ninguno
  clic 367 71; sleep 0.8; clic 380 365; sleep 1; clic 367 71; sleep 0.8; clic 380 325; sleep 1
  tecla ctrl-home right right right right
  for n in 1 2 3 4 5 6 7 8 9; do escribe $n; tecla ret; done    # E1...E9: nueve cambios
  tecla ctrl-z ctrl-z ctrl-z ctrl-z ctrl-z ctrl-z ctrl-z ctrl-z; aparca; shot u1   # ocho, deshechos
  tecla ctrl-y ctrl-y ctrl-y ctrl-y ctrl-y ctrl-y ctrl-y ctrl-y; shot u2          # y rehechos
  tecla ctrl-g; sleep 2.5
  tecla ctrl-e ctrl-n; sleep 3; aparca; shot u3; tecla ret        # toda la hoja en negrita: no cabe en el diario, y lo dice
  sal; } | run 300 -vga std
muestra u1 cuentas_ocho; muestra u3 cuentas_aviso_diario
ANCHO_A=88
cel u1 4 0 "1";  chk $T/u1.ppm nobusca 450 196 540 216 0 9 "2"   # solo queda el primero
cel u2 4 1 "2";  cel u2 4 8 "9"
chk $T/u3.ppm busca 100 200 700 400 0 8 "Este cambio toca tantas celdas que no se podrá deshacer,"
mtype -i $T/f.img ::G.HOJ | tr -d '\r' > $T/func.txt
hoj "C C1 0 3"; hoj "C C2 0 2"; hoj "C C3 0 1"; hoj "C C4 0 5"   # B, de menor a mayor; a igual B, C de mayor a menor
hoj "C B6 49152 "; hoj "C C7 49152 "; hoj "C B5 16384 "; hoj "C A6 32768 "   # todos los bordes de B6:C7
hoj "C F3 16384 "; hoj "C G3 16384 "; hoj "C E9 0 9"
if grep -qE '^C (B|C)1[01] |^C (B|C)9 |^C A1[01] |^C F2 |^C G2 ' $T/func.txt; then ko "G.HOJ: quedan bordes donde se quitaron (o sobran)"; else ok "G.HOJ: «ninguno» quita los bordes, y «abajo» solo pone el de abajo"; fi
fi

if quiero imprimir; then
echo "== Cuentas: imprimir en varias páginas, como texto y como se ve; la vista preliminar y la rejilla =="
# ocho columnas (no caben en el ancho del papel: dos tiras) y sesenta filas
# (dos tramos): cuatro páginas, primero hacia abajo y luego a la derecha
python3 - > $T/IMP.HOJ <<'HOJ'
import sys
celdas = """A1 1 Ventas del año
F1 0 Este texto largo sigue en la otra página
A2 2 cursiva
B2 16392 centro
C2 16396 derecha
A3 32768 pan
B3 0 12,5
C3 32769 7
G3 0 siete
H3 0 8
A4 32768 vino
B4 16384 3
C4 49152 =B3+B4
A45 0 abajo
G45 0 esquina
A60 0 fin"""
sys.stdout.buffer.write(('HOJA 1\r\n' + ''.join('C %s\r\n' % l for l in celdas.split('\n'))).encode('cp437'))
HOJ
mcopy -o -i $T/f.img $T/IMP.HOJ ::IMP.HOJ
reabre 'A:\IMP.HOJ'
rm -f $T/prn.txt
{ arranca
  clic 190 96; sleep 1.5; aparca; shot v1                         # el botón de la vista preliminar
  tecla pgdn; sleep 1; shot v2; tecla pgdn pgdn pgdn; sleep 1; shot v3   # otra página; y no pasa de la última
  tecla esc; sleep 1; shot v4                                     # de vuelta en la hoja
  tecla ctrl-p; sleep 6; shot i1; tecla ret; sleep 1              # como texto
  clic 100 71; sleep 0.8; shot m1; clic 140 201; sleep ${IMP_ESPERA:-30}; shot i2; tecla ret; sleep 1   # Archivo > Imprimir como se ve
  tecla shift-right shift-right shift-down shift-down shift-down shift-down   # A1:C5...
  clic 232 71; sleep 0.8; shot m2; clic 260 113; sleep 1          # ...con la rejilla (Ver > Rejilla al imprimir)
  clic 232 71; sleep 0.8; shot m3; clic 260 93; sleep 1.5; aparca; shot v5   # y su vista preliminar
  clic 100 71; sleep 0.8; clic 140 201; sleep 12; shot i3; tecla ret; sleep 1
  tecla ctrl-g; sleep 2.5
  sal; } | PARALELO=file:$T/prn.txt run 300 -vga std
muestra v1 cuentas_preliminar; muestra v2 cuentas_preliminar2; muestra v5 cuentas_preliminar_rejilla
muestra m1 cuentas_menu_archivo; muestra m2 cuentas_menu_ver; muestra v4 cuentas_tras_preliminar
chk $T/v1.ppm busca $PIE "Vista preliminar: página 1 de 4    Av Pág y Re Pág: otra página    Esc: volver"
chk $T/v1.ppm nobusca 98 178 300 200 0 9 "Ventas del año"         # la hoja de trabajo no se ve
chk $T/v1.ppm px 300 300 9 "el papel de la vista preliminar"
chk $T/v1.ppm px 300 527 9 "que cabe entero en la ventana"; chk $T/v1.ppm px 300 533 7 "(debajo, el fondo)"
chk $T/v2.ppm busca $PIE "Vista preliminar: página 2 de 4"
chk $T/v3.ppm busca $PIE "Vista preliminar: página 4 de 4"
chk $T/v4.ppm texto 541 180 0 9 fina "Este texto"                  # otra vez la hoja
chk $T/v4.ppm busca $PIE "A1: un texto"
chk $T/m2.ppm busca 200 80 500 130 0 8 "ejilla al imprimir: no"
chk $T/m3.ppm busca 200 80 500 130 0 8 "ejilla al imprimir: sí"
chk $T/v5.ppm busca $PIE "Vista preliminar: página 1 de 1"
chk $T/i1.ppm busca 150 250 650 310 0 8 "La hoja se ha enviado a la impresora."
chk $T/i2.ppm busca 150 250 650 310 0 8 "La hoja se ha enviado a la impresora."
chk $T/i3.ppm busca 150 250 650 310 0 8 "La hoja se ha enviado a la impresora."
mtype -i $T/f.img ::IMP.HOJ | tr -d '\r' > $T/func.txt
hoj "I 1"; hoj "C C4 49152 =B3+B4"
# lo que ha recibido la impresora: primero el texto (cuatro páginas) y luego
# dos trabajos «como se ve», que se rehacen punto por punto (un PPM por página)
if python3 - $T/prn.txt $T/pag <<'IMP' > $T/imp.log
import re, sys
d = open(sys.argv[1], 'rb').read()
INI = b'\x1b@\x1bA\x08'
i = d.find(INI)
if not d.startswith(b'\x1b@') or i < 0: sys.exit('no empieza como debe')
# ---- como texto ----
pags = d[2:i].split(b'\x0c')
if len(pags) != 5 or pags[4] != b'': sys.exit('texto: no son cuatro páginas')
P = [[re.sub(rb'\x1b[EF45]', b'', x) for x in p.split(b'\r\n')] for p in pags[:4]]
C = [p.split(b'\r\n') for p in pags[:4]]
def cab(l, n):
    return (l.startswith(b'IMP.HOJ ') and re.fullmatch(rb'\d\d/\d\d/\d{4}', l[35:45]) is not None
            and l.endswith(('Página %d de 4' % n).encode('cp437')) and len(l) == 80)
t = lambda s: s.encode('cp437')
ok = all(cab(P[n][0], n + 1) and P[n][1] == b'' for n in range(4))
ok = ok and [len(p) for p in P] == [61, 5, 61, 5]                 # la cabecera, sus filas y lo de detrás del último salto
ok = ok and P[0][2] == t('Ventas del año').ljust(60) + t('Este texto largo sig')   # F1, hasta el borde del papel
ok = ok and C[0][2].startswith(t('\x1bEVentas del año\x1bF'))
ok = ok and P[0][3] == b'cursiva'.ljust(14) + b'centro'.ljust(14) + b'derecha' and C[0][3].startswith(b'\x1b4cursiva\x1b5')
ok = ok and P[0][4] == b'pan' + b'12,5'.rjust(20) + b'7'.rjust(12) and t('\x1bE7\x1bF') in C[0][4]
ok = ok and P[0][5] == b'vino' + b'3'.rjust(19) + b'15,5'.rjust(12)
ok = ok and P[0][46] == b'abajo' and P[1][2] == b'' and P[1][3] == b'fin'
ok = ok and P[2][2] == b'' and P[2][4] == b'siete' + b'8'.rjust(18) and P[2][46] == b'esquina'
ok = ok and P[3][2] == b'' and P[3][3] == b''
if not ok: sys.exit('texto: no es lo esperado')
print('fecha', P[0][0][35:45].decode())
# ---- como se ve ----
def trabajo(i):
    """las páginas (filas de 576 puntos) del trabajo que empieza en d[i]"""
    i += 5; pags = []; filas = []
    while i < len(d) and d[i:i+5] != INI:
        if d[i:i+5] == b'\x1b*\x05\x40\x02':        # una tira con puntos, y su avance
            col = d[i+5:i+5+576]; i += 5 + 576
            for r in range(8): filas.append([1 if c & (0x80 >> r) else 0 for c in col])
            if d[i:i+2] != b'\r\n': sys.exit('como se ve: una tira sin su avance')
            i += 2
        elif d[i:i+2] == b'\r\n':                    # una tira en blanco: solo el avance
            filas += [[0] * 576 for _ in range(8)]; i += 2
        elif d[i] == 12:
            pags.append(filas); filas = []; i += 1
        else: sys.exit('como se ve: algo que no se entiende en %d' % i)
    if filas: sys.exit('como se ve: falta el salto de página')
    return pags, i
g1, i = trabajo(i)
if d[i:i+5] != INI: sys.exit('falta el segundo trabajo')
g2, i = trabajo(i)
if i != len(d) or len(g1) != 4 or len(g2) != 1: sys.exit('como se ve: %d y %d páginas' % (len(g1), len(g2)))
if max(len(p) for p in g1 + g2) > 760: sys.exit('una página pasa de 760 puntos')
for n, p in enumerate(g1 + g2):
    p += [[0] * 576 for _ in range(760 - len(p))]
    open('%s%d.ppm' % (sys.argv[2], n + 1), 'wb').write(b'P6 576 760 255\n' +
        b''.join(b'\x1a\x14\x10' if v else b'\xff\xff\xff' for f in p for v in f))
q = g2[0]
# la rejilla, punteada (un punto sí y otro no): la raya de encima de la fila 1 y la de la izquierda de la B
ok = all(q[26][x] == (1 - x % 2) for x in range(0, 265)) and not any(q[26][265:])
ok = ok and all(q[y][88] == 1 - y % 2 for y in range(99, 116)) and all(q[y][0] == 1 - y % 2 for y in range(27, 116))
ok = ok and not any(q[y][88] for y in range(27, 44))               # (el título, que sigue sobre B1, tapa su raya)
# el contorno de B3:C4, liso: arriba (debajo de la fila 2), abajo, izquierda y derecha
ok = ok and all(q[26 + 36][88:265]) and all(q[26 + 72][88:265])
ok = ok and all(q[y][88] for y in range(62, 99)) and all(q[y][264] for y in range(62, 99))
ok = ok and not any(q[26 + 91])                                   # debajo de la raya que cierra, nada
# sin rejilla (el primer trabajo) no hay más rayas que el contorno
p = g1[0]
ok = ok and not any(p[26][0:576]) and all(p[62][88:265]) and not any(p[62][0:88])
if not ok: sys.exit('como se ve: la rejilla o los bordes no son lo esperado')
IMP
then ok "impresora: cuatro páginas de texto (dos tiras de columnas por dos tramos de filas), con cabecera, negrita y cursiva"
     ok "impresora: como se ve, cuatro páginas y una con rejilla punteada y bordes lisos"
else ko "impresora: no ha salido lo esperado ($(tail -1 $T/imp.log 2>/dev/null))"; xxd $T/prn.txt | head -20; fi
F=$(sed -n 's/^fecha //p' $T/imp.log)
for n in 1 2 3 4 5; do [ -f $T/pag$n.ppm ] && convert $T/pag$n.ppm $B/cuentas_impresa$n.png; done
if [ -f $T/pag5.ppm ]; then
# página 1: las columnas A a F y las filas 1 a 40
chk $T/pag1.ppm texto 0 2 0 9 fina "IMP.HOJ";  chk $T/pag1.ppm texto 248 2 0 9 fina "$F"
chk $T/pag1.ppm texto 472 2 0 9 fina "Página 1 de 4"
chk $T/pag1.ppm texto 3 27 0 9 negrita "Ventas del año"
chk $T/pag1.ppm texto 443 27 0 9 fina "Este texto"                # F1, que se sale del papel
chk $T/pag1.ppm nobusca 0 40 80 50 0 9 "cursiva"                  # (en cursiva: no es la letra normal)
chk $T/pag1.ppm texto 108 45 0 9 fina "centro";  chk $T/pag1.ppm texto 205 45 0 9 fina "derecha"
chk $T/pag1.ppm texto 141 63 0 9 fina "12,5";  chk $T/pag1.ppm texto 253 63 0 9 negrita "7"
chk $T/pag1.ppm texto 229 81 0 9 fina "15,5"
chk $T/pag2.ppm texto 472 2 0 9 fina "Página 2 de 4"
chk $T/pag2.ppm texto 3 99 0 9 fina "abajo";   chk $T/pag2.ppm texto 3 369 0 9 fina "fin"   # las filas 45 y 60
# página 3: las columnas G y H; el texto largo de F1 sigue aquí
chk $T/pag3.ppm texto 472 2 0 9 fina "Página 3 de 4"
chk $T/pag3.ppm texto 3 27 0 9 fina "largo sigue en la otr"
chk $T/pag3.ppm texto 3 63 0 9 fina "siete";   chk $T/pag3.ppm texto 165 63 0 9 fina "8"
chk $T/pag4.ppm texto 472 2 0 9 fina "Página 4 de 4";  chk $T/pag4.ppm texto 3 99 0 9 fina "esquina"
chk $T/pag5.ppm texto 472 2 0 9 fina "Página 1 de 1";  chk $T/pag5.ppm texto 3 27 0 9 negrita "Ventas del año"
fi
fi

if quiero graficos; then
echo "== Cuentas: gráficos de varias series, con escala, título, leyenda y rótulos; se guardan, se imprimen y se exportan =="
python3 - > $T/GRAF.HOJ <<'HOJ'
import sys
celdas = """B1 0 Norte
C1 0 Sur
D1 0 Este
A2 0 Ene
B2 0 10
C2 0 4
D2 0 6
A3 0 Feb
B3 0 20
C3 0 8
D3 0 -5
A4 0 Mar
B4 0 15
C4 0 12
D4 0 9
A5 0 Abr
B5 0 30
C5 0 16
D5 0 3
F1 0 x
G1 0 y
F2 0 1
G2 0 2
F3 0 2
G3 0 4
F4 0 3
G4 0 6
F5 0 4
G5 0 8"""
t = 'HOJA 1\r\nG 0 A1:D5 0\r\nT Ventas por zonas\r\nX Meses\r\nY Miles\r\n' + ''.join('C %s\r\n' % l for l in celdas.split('\n'))
sys.stdout.buffer.write(t.encode('cp437'))
HOJ
# (sitio en el disquete para el .BMP, de 231 K: fuera lo que esta prueba no usa)
for f in HISPADOS/AUDICION.COM HISPADOS/EQUIPO.COM HISPADOS/HSPUSB.SYS HISPADOS/HSPEMM.SYS HISPADOS/PROGS/ARCHIVOS.HVP HISPADOS/PROGS/PINTURA.HVP HISPADOS/PROGS/MEZCLAD.HVP; do mdel -i $T/f.img ::$f 2>/dev/null; done
mdeltree -i $T/f.img ::DOCS 2>/dev/null
mcopy -o -i $T/f.img $T/GRAF.HOJ ::GRAF.HOJ
reabre 'A:\GRAF.HOJ'
rm -f $T/prn.txt
gmenu() { clic 240 134; sleep 0.8; clic 260 $1; sleep 1.5; }   # una orden del menú «Gráfico» de la ventana del gráfico
{ arranca
  clic 232 71; sleep 0.8; shot m1; clic 260 161; sleep 1.5; aparca; shot c1   # Ver > Gráfico de la hoja: el guardado, de barras
  clic 240 134; sleep 0.8; shot m2; clic 260 176; sleep 1.5; aparca; shot c2   # Gráfico > Barras apiladas
  gmenu 196; aparca; shot c3                                      # líneas
  gmenu 216; aparca; shot c4                                      # sectores
  gmenu 156; gmenu 284; aparca; shot c5                           # barras, con las series por filas
  gmenu 264; gmenu 312; shot d1; escribe "Zonas"; tecla ret; sleep 1.5; aparca; shot c6   # por columnas; y otro título
  clic 170 134; sleep 0.8; shot m3; clic 190 156; sleep 14; shot i1; tecla ret; sleep 1   # Archivo > Imprimir
  clic 170 134; sleep 0.8; clic 190 176; sleep 1.5; shot d2; tecla ret; sleep 9; shot i2; tecla ret; sleep 1   # Guardar como imagen
  clic 170 134; sleep 0.8; clic 190 204; sleep 1; shot c7         # Cerrar
  tecla ctrl-g; sleep 2.5
  tecla ctrl-home right right right right right shift-right shift-down shift-down shift-down shift-down   # F1:G5...
  clic 291 71; sleep 0.8; shot m4; clic 310 269; sleep 1.5; aparca; shot c8     # ...Insertar > Gráfico de dispersión
  clic 300 48; tecla ctrl-home right right right right right right down down down down
  escribe "16"; tecla ret                                         # G5, de 8 a 16: el gráfico lo sigue
  tecla ctrl-home; clic 506 96; sleep 1.5; aparca; shot c9        # el botón del gráfico: el de la hoja
  clic 300 48; clic 291 71; sleep 0.8; clic 310 93; sleep 1       # una fila nueva encima: el rango se corre
  clic 100 71; sleep 0.8; clic 140 153; sleep 1.5; escribe "g2"; tecla ret; sleep 2.5   # Archivo > Guardar como: G2.HOJ
  sal; } | PARALELO=file:$T/prn.txt run 300 -vga std
for n in 1 2 3 4 5 6 7 8 9; do muestra c$n cuentas_grafico$n; done
muestra m1 cuentas_menu_ver2; muestra m2 cuentas_menu_grafico; muestra m3 cuentas_menu_grafico_archivo; muestra d1 cuentas_grafico_titulos
muestra d2 cuentas_grafico_bmp_dlg; muestra m4 cuentas_menu_insertar; muestra i1 cuentas_grafico_impreso; muestra i2 cuentas_grafico_guardado
GV="132 146 628 480 0 9"          # la zona de la ventana del gráfico: tinta sobre blanco
# el gráfico guardado con la hoja: barras de tres series (B, C y D), con los meses de la columna A
chk $T/m1.ppm busca 200 80 500 180 0 8 "ráfico de la hoja"
chk $T/c1.ppm titulo 130 100 activa "Gráfico de barras"
for t in "Norte" "Sur" "Este" "Ene" "Feb" "Mar" "Abr" "Meses" "30" "20" "10" "-10"; do chk $T/c1.ppm busca $GV "$t"; done
chk $T/c1.ppm busca 300 146 330 156 0 9 " "                       # (algo hay donde el título: está en negrita)
chk $T/c1.ppm texto 316 151 0 9 negrita "Ventas por zonas"
chk $T/c1.ppm rgb 210 340 1F4E5A "la barra de Norte, en petróleo"
chk $T/c1.ppm rgb 231 362 C2542D "la de Sur, en terracota"
chk $T/c1.ppm rgb 341 392 E8B33A "la de Este en febrero (-5), en ocre y por debajo del cero"
# apiladas: la escala llega a 50 (30 + 16 + 3 en abril); líneas; sectores, con los tantos por ciento de la primera serie
chk $T/c2.ppm titulo 130 100 activa "Gráfico de barras apiladas"
chk $T/c2.ppm busca $GV "50";  chk $T/c2.ppm busca $GV "Este"
chk $T/c2.ppm rgb 490 250 C2542D "abril: Sur, encima de Norte"
chk $T/c2.ppm rgb 490 320 1F4E5A "abril: Norte, abajo"
chk $T/c3.ppm titulo 130 100 activa "Gráfico de líneas"
chk $T/c3.ppm busca $GV "Norte";  chk $T/c3.ppm busca $GV "-10"
chk $T/c4.ppm titulo 130 100 activa "Gráfico de sectores"
for t in "Ene: 13%" "Feb: 27%" "Mar: 20%" "Abr: 40%"; do chk $T/c4.ppm busca $GV "$t"; done
# las series por filas: ahora los meses son las series y las zonas, los puntos
for t in "Ene" "Abr" "Norte" "Sur" "Este"; do chk $T/c5.ppm busca $GV "$t"; done
chk $T/c5.ppm rgb 285 300 6B8E3D "la cuarta serie (abril), en oliva"
chk $T/m2.ppm busca 200 140 500 330 0 8 "arras a";  chk $T/m2.ppm busca 200 140 500 330 0 8 "ítulo y rótulos de los ejes..."
chk $T/d1.ppm busca 150 150 700 400 0 8 "Eje horizontal:"
chk $T/c6.ppm texto 360 151 0 9 negrita "Zonas"                   # el título nuevo
chk $T/i1.ppm busca 150 250 650 310 0 8 "La hoja se ha enviado a la impresora."
chk $T/i2.ppm busca 150 250 650 310 0 8 "El gráfico se ha guardado."
chk $T/c7.ppm nobusca $GV "Meses";  chk $T/c7.ppm titulo 60 36 activa "Cuentas - A:\\GRAF.HOJ"   # la ventana del gráfico, cerrada
# de dispersión, de F1:G5 (las X y las Y); cambia una cantidad y el gráfico la sigue
chk $T/c8.ppm titulo 130 100 activa "Gráfico de dispersión"
for t in "1,5" "3,5" "8" "7"; do chk $T/c8.ppm busca $GV "$t"; done
chk $T/c9.ppm busca $GV "15";  chk $T/c9.ppm busca $GV "20";  chk $T/c9.ppm nobusca $GV "7"
mtype -i $T/f.img ::GRAF.HOJ | tr -d '\r' > $T/func.txt
hoj "G 0 A1:D5 0"; hoj "T Zonas"; hoj "X Meses"; hoj "Y Miles"
mtype -i $T/f.img ::G2.HOJ | tr -d '\r' > $T/func.txt
hoj "G 4 F2:G6 0"; hoj "C G6 0 16"                                # el gráfico nuevo (sin títulos), con el rango corrido una fila
if grep -q '^T ' $T/func.txt; then ko "G2.HOJ: un gráfico nuevo no hereda el título del anterior"; else ok "G2.HOJ: el gráfico nuevo no lleva título"; fi
# la imagen exportada: un .BMP de 576 x 400 y 256 colores
mcopy -o -i $T/f.img ::GRAF.BMP $T/GRAF.BMP 2>/dev/null
if python3 - $T/GRAF.BMP <<'BMP'
import struct, sys
d = open(sys.argv[1], 'rb').read()
if d[:2] != b'BM' or len(d) != 54 + 1024 + 576 * 400: sys.exit('no es un BMP del tamaño esperado')
tam, _, off, cab, w, h, planos, bits, comp = struct.unpack_from('<IIIIiiHHI', d, 2)
if (tam, off, cab, w, h, planos, bits, comp) != (len(d), 1078, 40, 576, 400, 1, 8, 0): sys.exit('la cabecera no es la esperada')
if d[54 + 8:54 + 12] != bytes([0x5A, 0x4E, 0x1F, 0]): sys.exit('la paleta no es la de la pantalla')   # petróleo: azul, verde, rojo
px = lambda x, y: d[off + (h - 1 - y) * w + x]
alto = lambda x, c: sum(1 for y in range(h) if px(x, y) == c)
n = [alto(83 + 109 * i, 2) for i in range(4)]                     # las cuatro barras de Norte: 10, 20, 15 y 30
if not (60 < n[0] < 100 and abs(n[1] - 2 * n[0]) <= 2 and abs(2 * n[2] - 3 * n[0]) <= 3 and abs(n[3] - 3 * n[0]) <= 2):
    sys.exit('las barras no guardan la proporción: %r' % n)
if not (20 < alto(110, 4) < 40 and 40 < alto(137, 10) < 60): sys.exit('faltan las barras de Sur o de Este')
if sum(1 for x in range(3, 19) for y in range(100, 300) if px(x, y) == 0) < 40: sys.exit('falta el rótulo del eje vertical')
BMP
then ok "GRAF.BMP: 576 x 400, 256 colores, con las barras a escala y el rótulo del eje vertical"; convert $T/GRAF.BMP $B/cuentas_grafico_bmp.png
else ko "GRAF.BMP no es lo esperado"; fi
# el gráfico impreso: una página «como se ve», con cada serie rayada a su manera
if python3 - $T/prn.txt $T/gpag.ppm <<'IMP'
import sys
d = open(sys.argv[1], 'rb').read()
if not d.startswith(b'\x1b@\x1bA\x08'): sys.exit('no empieza como debe')
i = 5; f = []
while i < len(d) and d[i] != 12:
    if d[i:i+5] == b'\x1b*\x05\x40\x02':
        col = d[i+5:i+5+576]; i += 5 + 576
        for r in range(8): f.append([1 if c & (0x80 >> r) else 0 for c in col])
        if d[i:i+2] != b'\r\n': sys.exit('una tira sin su avance')
        i += 2
    elif d[i:i+2] == b'\r\n':
        f += [[0] * 576 for _ in range(8)]; i += 2
    else: sys.exit('algo que no se entiende en %d' % i)
if i != len(d) - 1 or not 426 <= len(f) <= 440: sys.exit('no es una página con el gráfico (%d filas)' % len(f))
open(sys.argv[2], 'wb').write(b'P6 576 %d 255\n' % len(f) + b''.join(b'\x1a\x14\x10' if v else b'\xff\xff\xff' for l in f for v in l))
parte = lambda x0, x1, y0, y1: sum(f[y][x] for y in range(y0, y1) for x in range(x0, x1)) / ((x1 - x0) * (y1 - y0))
norte, sur, este = parte(72, 95, 226, 300), parte(99, 122, 276, 300), parte(126, 149, 258, 300)
if not (norte == 1 and 0.2 < sur < 0.35 and 0.03 < este < 0.15): sys.exit('los rayados no son los esperados: %r' % ((norte, sur, este),))
IMP
then ok "impresora: el gráfico, con Norte lisa, Sur rayada y Este punteada"; convert $T/gpag.ppm $B/cuentas_grafico_impreso_pag.png
     chk $T/gpag.ppm texto 0 2 0 9 fina "GRAF.HOJ";  chk $T/gpag.ppm texto 472 2 0 9 fina "Página 1 de 1"
     for t in "Norte" "Este" "Ene" "Meses" "30" "-10"; do chk $T/gpag.ppm busca 0 26 576 432 0 9 "$t"; done
else ko "impresora: el gráfico no ha salido como se esperaba"; fi
fi

if quiero formato32; then
echo "== Cuentas: color de fondo, alto de fila y combinar y centrar; cómo salen al imprimir =="
# 3145737: combinada con las tres de su derecha, centrada y en negrita; los fondos: ocre, crema y petróleo
printf 'HOJA 1\r\nH 3 36\r\nH 4 10\r\nC A1 3145737 Informe trimestral\r\nC A2 655360 ocre\r\nC B2 524288 5\r\nC C2 131072 oscuro\r\nC A3 0 alta\r\nC A4 0 baja\r\nC A5 0 normal\r\nC E5 0 x\r\n' > $T/F32.HOJ
mcopy -o -i $T/f.img $T/F32.HOJ ::F32.HOJ
reabre 'A:\F32.HOJ'
rm -f $T/prn.txt
fmenu() { clic 367 71; sleep 0.8; clic 380 $1; sleep 1.2; }       # una orden del menú Formato
{ arranca
  tecla ctrl-end; aparca; shot s1                                 # (el cursor, a E5)
  tecla ctrl-home down down down down down down; escribe "Total"; tecla ret up
  tecla shift-right shift-right; fmenu 277; shot s2a              # A7:C7, combinadas y centradas
  clic 554 124; sleep 0.8                                         # con el fondo turquesa
  tecla right; shot s2b; tecla left; shot s2c                     # a la derecha: D7; a la izquierda: de vuelta en A7
  arrastra 80 279 80 299                                          # la fila 5, veinte puntos más alta
  tecla ctrl-home down down down down down; fmenu 257; tecla backspace backspace; escribe "30"; tecla ret; sleep 1   # la 6, de 30
  tecla ctrl-end; aparca; shot s3
  tecla ctrl-home; fmenu 277; tecla ctrl-end; shot s4             # A1 deja de estar combinada...
  tecla ctrl-z; sleep 0.5; shot s5                                # ...y deshacer la vuelve a combinar
  tecla ctrl-home ctrl-p; sleep 5; tecla ret; sleep 1             # como texto
  clic 100 71; sleep 0.8; clic 140 201; sleep 16; tecla ret; sleep 1   # y como se ve
  tecla ctrl-g; sleep 2.5
  sal; } | PARALELO=file:$T/prn.txt run 300 -vga std
muestra s1 cuentas_fondos; muestra s3 cuentas_altos; muestra s4 cuentas_sin_combinar; muestra s2a cuentas_combinada
ANCHO_A=88
chk $T/s1.ppm texto 202 180 0 9 negrita "Informe trimestral"       # centrado sobre A1:D1
chk $T/s1.ppm px 186 188 9 "dentro de la combinada no hay rayas de la rejilla"
chk $T/s1.ppm px 274 206 7 "(entre B2 y C2, sí)"
chk $T/s1.ppm texto 101 198 0 10 fina "ocre"                      # sobre fondo ocre
chk $T/s1.ppm texto 263 198 0 8 fina "5"                          # sobre crema
chk $T/s1.ppm texto 277 198 0 2 fina "oscuro"                     # sobre petróleo
chk $T/s1.ppm px 150 206 10 "el fondo ocre llena la celda"
chk $T/s1.ppm texto 101 234 0 9 fina "alta"                       # la fila 3, de 36 puntos: lo escrito, abajo
chk $T/s1.ppm nobusca 98 240 190 270 0 9 "baja"                   # la fila 4, de 10: no cabe lo escrito
chk $T/s1.ppm texto 101 262 0 9 fina "normal"                     # y la 5, detrás
chk $T/s1.ppm busca 62 108 700 140 0 6 "Fondo:"                   # las muestras del fondo, en la barra
chk $T/s2b.ppm texto 210 298 0 14 fina "Total"                    # centrado sobre A7:C7, y con el fondo turquesa
chk $T/s2b.ppm busca $PIE "D7: celda vacía"                       # las tapadas se saltan
chk $T/s2c.ppm busca $PIE "A7: un texto"
chk $T/s3.ppm texto 210 330 0 14 fina "Total"                     # con las filas 5 y 6 más altas (38 y 30 puntos)
chk $T/s3.ppm px 330 338 14 "el fondo llega hasta C7"
chk $T/s3.ppm texto 101 282 0 9 fina "normal"
chk $T/s4.ppm px 186 188 7 "sin combinar, vuelve la raya entre A1 y B1"
chk $T/s4.ppm nobusca 190 178 400 198 0 9 "Informe"
chk $T/s5.ppm texto 202 180 0 9 negrita "Informe trimestral"
mtype -i $T/f.img ::F32.HOJ | tr -d '\r' > $T/func.txt
hoj "H 3 36"; hoj "H 4 10"; hoj "H 5 38"; hoj "H 6 30"
hoj "C A1 3145737 Informe trimestral"; hoj "C A7 3014664 Total"; hoj "C B7 917504 "; hoj "C C7 917504 "; hoj "C C2 131072 oscuro"
if python3 - $T/prn.txt $T/fpag.ppm <<'IMP'
import re, sys
d = open(sys.argv[1], 'rb').read()
INI = b'\x1b@\x1bA\x08'
i = d.find(INI)
if not d.startswith(b'\x1b@') or i < 0: sys.exit('no empieza como debe')
l = [re.sub(rb'\x1b[EF45]', b'', x) for x in d[2:i].split(b'\r\n')]
# como texto: el título, centrado en el sitio de sus cuatro columnas (47 letras), y en negrita
if l[2] != b' ' * 14 + b'Informe trimestral' or b'\x1bEInforme trimestral\x1bF' not in d: sys.exit('texto: el título combinado')
if l[3] != b'ocre' + b'5'.rjust(19) + b' oscuro' or l[8] != b'Total'.center(35).rstrip(): sys.exit('texto: las filas')
i += 5; f = []
while i < len(d) and d[i] != 12:
    if d[i:i+5] == b'\x1b*\x05\x40\x02':
        col = d[i+5:i+5+576]; i += 5 + 576
        for r in range(8): f.append([1 if c & (0x80 >> r) else 0 for c in col])
        i += 2
    elif d[i:i+2] == b'\r\n':
        f += [[0] * 576 for _ in range(8)]; i += 2
    else: sys.exit('algo que no se entiende en %d' % i)
if i != len(d) - 1: sys.exit('sobra algo detrás de la página')
f += [[0] * 576 for _ in range(240 - len(f))]
open(sys.argv[2], 'wb').write(b'P6 576 %d 255\n' % len(f) + b''.join(b'\x1a\x14\x10' if v else b'\xff\xff\xff' for r in f for v in r))
parte = lambda x0, x1, y0, y1: sum(f[y][x] for y in range(y0, y1) for x in range(x0, x1)) / ((x1 - x0) * (y1 - y0))
ocre, crema, oscuro, blanco = parte(44, 84, 46, 60), parte(92, 132, 46, 60), parte(240, 260, 46, 60), parte(300, 340, 46, 60)
if not (0.12 < ocre < 0.26 and 0.03 < crema < 0.1 and oscuro == 1 and blanco == 0): sys.exit('los fondos: %r' % ((ocre, crema, oscuro, blanco),))
turq = parte(180, 260, 180, 192)
if not 0.18 < turq < 0.32: sys.exit('el fondo de la combinada: %r' % turq)
IMP
then ok "impresora: el título combinado y centrado (texto); los fondos, tramados o lisos según su color (como se ve)"
     convert $T/fpag.ppm $B/cuentas_impresa_fondos.png
     chk $T/fpag.ppm texto 104 27 0 9 negrita "Informe trimestral"
     chk $T/fpag.ppm texto 3 81 0 9 fina "alta"                   # la fila alta: 26 + 18 + 18 + 36 - 17
     chk $T/fpag.ppm texto 3 129 0 9 fina "normal"                # detrás de la de 10 puntos, y con sus 38: 82 + 10 + 38 - 17 + 16
else ko "impresora: los fondos, los altos o las combinadas no han salido como se esperaba"; fi
fi

if quiero paneles; then
echo "== Cuentas: inmovilizar paneles (los títulos, siempre a la vista); se guarda con la hoja =="
python3 - > $T/PAN.HOJ <<'HOJ'
import sys
l = ['C A1 1 Mes', 'C B1 1 Ventas', 'C C1 1 Gastos'] + ['C %s1 1 col%s' % (c, c) for c in 'DEFGHIJ']
for f in range(2, 61):
    l += ['C A%d 0 f%d' % (f, f), 'C B%d 0 %d' % (f, f * 10), 'C H%d 0 %d' % (f, f * 100), 'C J%d 0 %d' % (f, f * 1000)]
sys.stdout.buffer.write(('HOJA 1\r\n' + ''.join(x + '\r\n' for x in l)).encode('cp437'))
HOJ
mcopy -o -i $T/f.img $T/PAN.HOJ ::PAN.HOJ
reabre 'A:\PAN.HOJ'
ira() { tecla f5; sleep 1; escribe "$1"; tecla ret; sleep 1; }
vmenu() { clic 232 71; sleep 0.8; clic 260 133; sleep 1.2; }       # Ver > Inmovilizar paneles
{ arranca
  vmenu; sleep 1; shot n0; tecla ret                              # en A1 no hay nada que inmovilizar: lo dice
  tecla right down; vmenu; clic 232 71; sleep 0.8; shot n1m; tecla esc esc   # desde B2: la fila 1 y la columna A, quietas
  ira b50; aparca; shot n1                                        # abajo del todo: los títulos siguen arriba
  ira j50; aparca; shot n2                                        # y a la derecha: la columna A sigue a la izquierda
  clic 300 189; aparca; shot n3                                   # un clic en la fila quieta: la celda que se ve ahí
  clic 140 300; aparca; shot n4                                   # y otro en la columna quieta
  tecla ctrl-g; sleep 2.5
  vmenu; tecla ctrl-home; aparca; shot n5                         # sueltos otra vez
  clic 100 71; sleep 0.8; clic 140 153; sleep 1.5; escribe "pan2"; tecla ret; sleep 2.5   # Guardar como: PAN2.HOJ
  sal; } | run 240 -vga std
muestra n1 cuentas_paneles; muestra n2 cuentas_paneles2; muestra n1m cuentas_menu_ver3
ANCHO_A=88
chk $T/n0.ppm busca 100 200 700 400 0 8 "quietas. (En A1 no hay nada que inmovilizar.)"
chk $T/n1m.ppm busca 200 80 500 180 0 8 "nmovilizar paneles: sí"
chk $T/n1.ppm busca $PIE "B50: un número"
chk $T/n1.ppm texto 101 180 0 9 negrita "Mes";  chk $T/n1.ppm texto 189 180 0 9 negrita "Ventas"   # la fila 1, en su sitio
chk $T/n1.ppm busca 98 440 190 520 0 9 "f50"                      # y debajo, las filas de la 34 en adelante
chk $T/n1.ppm nobusca 98 195 190 215 0 9 "f2"
chk $T/n1.ppm busca 98 195 190 215 0 9 "f34"
chk $T/n1.ppm px 186 300 0 "la raya de la columna inmovilizada"
chk $T/n1.ppm px 300 197 0 "y la de la fila"
chk $T/n2.ppm busca $PIE "J50: un número"
chk $T/n2.ppm texto 101 180 0 9 negrita "Mes"                     # la esquina no se mueve
chk $T/n2.ppm busca 98 440 190 520 0 9 "f50"                      # la columna A, quieta
chk $T/n2.ppm nobusca 186 178 700 198 0 9 "Ventas"                # la B se ha ido por la izquierda...
chk $T/n2.ppm busca 186 440 700 520 0 9 "49000"                   # ...y se ve la J
chk $T/n2.ppm busca 186 160 700 180 0 6 "J"
chk $T/n3.ppm busca 62 528 700 552 0 6 "1: un texto"              # (la celda de la fila 1 que haya bajo el clic)
chk $T/n3.ppm nobusca $PIE "50"
chk $T/n4.ppm busca 62 528 700 552 0 6 ": un texto"
chk $T/n4.ppm busca $PIE "A39: un texto"                        # (la fila que se ve ahí, con la vista corrida)
mtype -i $T/f.img ::PAN.HOJ | tr -d '\r' > $T/func.txt
hoj "P 1 1"
chk $T/n5.ppm busca $PIE "A1: un texto"
chk $T/n5.ppm rgb 186 300 8F7F68 "sin inmovilizar, la raya vuelve a ser de las de la rejilla"
mtype -i $T/f.img ::PAN2.HOJ | tr -d '\r' > $T/func.txt
if grep -q '^P ' $T/func.txt; then ko "PAN2.HOJ: no debería llevar paneles inmovilizados"; else ok "PAN2.HOJ: sin paneles inmovilizados"; fi
hoj "C J60 0 60000"
# la hoja guardada con los paneles se abre con ellos puestos
reabre 'A:\PAN.HOJ'
{ arranca
  ira b40; aparca; shot n6
  sal; } | run 120 -vga std
chk $T/n6.ppm busca $PIE "B40: un número"
chk $T/n6.ppm texto 101 180 0 9 negrita "Mes";  chk $T/n6.ppm px 186 300 0 "la raya de la columna inmovilizada"
fi

if quiero libro; then
echo "== Cuentas: varias hojas en un libro (pestañas, añadir, renombrar, mover, eliminar); todo en el mismo .HOJ =="
reabre ''
dclic() { ir $1 $2; mon "mouse_button 1" 0.08; mon "mouse_button 0" 0.08; mon "mouse_button 1" 0.08; mon "mouse_button 0" 0.6; }
# teclea una fórmula con «!» (Hoja2!A1), que «escribe» no sabe
escribe_h() { local t="$1"; while [ "${t#*!}" != "$t" ]; do escribe "${t%%!*}"; tecla shift-1; t="${t#*!}"; done; escribe "$t"; }
TABS="62 512 500 540"             # la fila de las pestañas
{ arranca
  escribe "uno"; tecla ret; escribe "=a1"; tecla ret
  clic 127 526; sleep 1.2; aparca; shot b1                        # la pestaña «+»: una hoja nueva, Hoja2
  escribe "dos"; tecla tab; escribe "5"; tecla ret
  clic 80 526; sleep 1.2                                          # la pestaña de Hoja1
  tecla ctrl-home right; escribe_h "=hoja2!b1*2"; tecla tab; escribe_h "=suma(hoja2!a1:b3)"; tecla ret   # celdas de la otra hoja
  aparca; shot b2
  tecla ctrl-home ctrl-c; clic 140 526; sleep 1.2; tecla ctrl-home down down ctrl-v; tecla ctrl-end; aparca; shot b3   # copiar «uno» y pegarlo en A3 de Hoja2
  clic 80 545; sleep 1; aparca; shot b3p                          # (un clic en el pie, debajo de una pestaña, no cambia de hoja)
  tecla ctrl-pgup; sleep 1; shot b4; tecla ctrl-pgdn; sleep 1     # con el teclado: a la anterior y vuelta
  dclic 140 526; sleep 1.2; shot b5; escribe "Ventas"; tecla ret; sleep 1.2   # doble clic: el nombre
  clic 367 71; sleep 0.8; shot bm; clic 380 433; sleep 1.2; aparca; shot b6   # Formato > Mover la hoja a la izquierda
  tecla ctrl-home right; escribe "7"; tecla ret                   # y en ella, B1 pasa de 5 a 7
  clic 291 71; sleep 0.8; clic 310 297; sleep 1.2                 # Insertar > Hoja nueva: Hoja2, la tercera
  escribe "tres"; tecla ret; aparca; shot b7
  clic 175 71; sleep 0.8; clic 200 317; sleep 1.2; shot b8; tecla ret; sleep 1.2; aparca; shot b9   # Edición > Eliminar hoja
  tecla ctrl-home; clic 291 71; sleep 0.8; clic 310 93; sleep 1.2; tecla ctrl-end down; aparca; shot b9b   # una fila nueva encima
  clic 100 71; sleep 0.8; clic 140 153; sleep 1.5; escribe "lib"; tecla ret; sleep 2.5   # Guardar como: LIB.HOJ
  sal; } | run 240 -vga std
muestra b1 cuentas_libro1; muestra b6 cuentas_libro_movida; muestra b5 cuentas_libro_nombre; muestra b9 cuentas_libro_final; muestra bm cuentas_menu_formato
ANCHO_A=88
chk $T/b1.ppm busca $TABS 0 7 "Hoja1"                             # Hoja1, detrás; la nueva, delante (en blanco y en negrita)
chk $T/b1.ppm px 101 189 9 "la hoja nueva, vacía"
chk $T/b1.ppm busca $PIE "A1: celda vacía"
chk $T/b2.ppm texto 101 198 0 9 fina "uno";  chk $T/b2.ppm busca $TABS 0 7 "Hoja2"   # (A2: =A1)
cel b2 1 0 "10";  cel b2 2 0 "5"                                  # =Hoja2!B1*2 y =SUMA(Hoja2!A1:B3)
chk $T/b3.ppm texto 101 180 0 9 fina "dos";  chk $T/b3.ppm texto 101 216 0 9 fina "uno"   # lo copiado en la otra hoja
chk $T/b3p.ppm texto 101 216 0 9 fina "uno";  chk $T/b3p.ppm busca $TABS 0 7 "Hoja1"   # sigue en Hoja2
chk $T/b4.ppm texto 101 198 0 9 fina "uno";  chk $T/b4.ppm busca $PIE "A1: un texto"      # Hoja1: el cursor, donde estaba
chk $T/b5.ppm busca 100 150 700 450 0 8 "Nombre de la hoja (sin espacios):"
chk $T/b6.ppm busca $TABS 0 7 "Hoja1";  chk $T/b6.ppm nobusca $TABS 0 7 "Ventas"   # Ventas, la activa, ahora la primera
chk $T/b6.ppm texto 101 180 0 9 fina "dos"
chk $T/b7.ppm busca $TABS 0 7 "Ventas";  chk $T/b7.ppm busca $TABS 0 7 "Hoja1";  chk $T/b7.ppm texto 101 180 0 9 fina "tres"
chk $T/b8.ppm busca 100 200 700 400 0 8 "¿Eliminar esta hoja con todo lo que tiene?"
chk $T/b9.ppm texto 101 198 0 9 fina "uno";  chk $T/b9.ppm nobusca $TABS 0 7 "Hoja2"   # sin la tercera: se ve Hoja1
cel b9 2 0 "7"                                                    # la suma de la hoja renombrada, con su B1 cambiada
cel b9b 1 1 "14";  cel b9b 2 1 "7";  chk $T/b9b.ppm texto 101 216 0 9 fina "uno"   # una fila más abajo, y siguen valiendo
mtype -i $T/f.img ::LIB.HOJ | tr -d '\r' > $T/func.txt
# (al renombrar Hoja2, las fórmulas que la nombraban dicen Ventas; al insertar la fila en Hoja1, las celdas de Ventas no se corren)
if [ "$(grep -vE '^V ' $T/func.txt | tr '\n' '|')" = 'HOJA 1|M EUR|L 2|S Ventas|C A1 0 dos|C B1 0 7|C A3 0 uno|S Hoja1|C A2 0 uno|C B2 0 =Ventas!b1*2|C C2 0 =suma(Ventas!a1:b3)|C A3 0 =A2|' ]
then ok "LIB.HOJ: las dos hojas, cada una con lo suyo, las fórmulas con el nombre nuevo y cuál se veía"
else ko "LIB.HOJ no es lo esperado"; cat $T/func.txt; fi
# el libro guardado se abre con sus hojas
reabre 'A:\LIB.HOJ'
{ arranca
  aparca; shot b10
  clic 80 526; sleep 1.2; aparca; shot b11                        # la pestaña de Ventas
  sal; } | run 120 -vga std
chk $T/b10.ppm texto 101 198 0 9 fina "uno";  chk $T/b10.ppm busca $TABS 0 7 "Ventas"   # se abre por la que se veía
cel b10 1 1 "14";  cel b10 2 1 "7"                                # y calcula con las celdas de la otra, que está plegada
chk $T/b11.ppm texto 101 216 0 9 fina "uno";  chk $T/b11.ppm texto 263 180 0 9 fina "7";  chk $T/b11.ppm busca $TABS 0 7 "Hoja1"
fi

if quiero contexto; then
echo "== Cuentas: el botón secundario (el menú contextual) =="
reabre ''
{ arranca
  escribe "10"; tecla ret; escribe "20"; tecla ret; escribe "30"; tecla ret
  derecho 140 206; shot k1                     # A2, que no está elegida: se va a ella
  tecla c; sleep 0.5                           # Copiar
  derecho 230 188; sleep 0.5; shot k2          # B1: ya hay algo que pegar
  tecla p; sleep 0.8; tecla down; aparca; shot k3   # Pegar (y a la celda de abajo, para ver la pegada sin el cursor)
  arrastra 140 188 140 224
  derecho 140 206; shot k4                     # dentro de lo elegido (A1:A3): sigue elegido
  tecla esc
  derecho 140 170; shot k5                     # el rótulo de la columna A: entera, y su menú
  tecla esc
  derecho 88 526; shot k6                      # la pestaña de la hoja
  tecla esc
  clic 230 206; tecla shift-f10; sleep 0.6; shot k7   # con el teclado: junto a la celda activa (B2)
  tecla esc
  derecho 300 110; sleep 0.5; shot k8          # en la barra de botones no hay menú
  sal; } | run 150 -vga std
chk $T/k1.ppm busca $PIE "A2: un número" busca 140 206 240 440 0 8 "opiar" busca 140 206 240 440 7 8 "egar" \
  busca 140 206 240 440 0 8 "Insertar c" busca 140 206 240 440 0 8 "liminar filas" busca 140 206 240 520 0 8 "Formato de celda"
chk $T/k2.ppm busca 230 188 330 420 0 8 "egar"
chk $T/k3.ppm busca $PIE "B2: celda vac"
cel k3 1 0 "20"
chk $T/k4.ppm busca $PIE "A1:A3" busca 140 206 240 440 0 8 "egar"
chk $T/k5.ppm busca $PIE "A1:A1000" busca 140 170 240 400 0 8 "ncho de columna..." nobusca 140 170 240 400 0 8 "liminar filas"
chk $T/k6.ppm busca 80 300 200 560 0 8 "oja nueva" busca 80 300 200 560 7 8 "liminar hoja" busca 80 300 200 560 0 8 "ombre de la hoja..."
chk $T/k7.ppm busca 190 200 330 500 0 8 "Formato de celda"
chk $T/k8.ppm nobusca 280 100 420 420 0 8 "Formato de celda"
muestra k1 cuentas_contexto; muestra k5 cuentas_contexto_columna; muestra k6 cuentas_contexto_hoja
fi

# (GUARDA=carpeta deja ahí las capturas y lo demás de la prueba, para mirarlo)
[ -n "${GUARDA:-}" ] && { rm -rf "$GUARDA"; cp -r $T "$GUARDA"; }
rm -rf $T
exit $fail
