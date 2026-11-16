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
  clic 327 71; sleep 0.8; clic 340 93; sleep 1.5                  # Formato > Celdas...
  clic 214 257; aparca; shot f2d; clic 343 430; sleep 1           # con millares; Aceptar
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
hoj() { if grep -qxF -- "$1" $T/func.txt; then ok "FUNC.HOJ: «$1»"; else ko "FUNC.HOJ: falta «$1»"; fi; }
hoj "HOJA 1"; hoj "M EUR"; hoj "A A1 128"; hoj "A F1 120"
hoj "C B2 1 2"; hoj "C C2 2432 1,2"; hoj "C A3 16 vino"; hoj "C A2 8 pan"; hoj "C A5 2 leche"
hoj "C E1 1536 0,21"; hoj "C E2 256 1234567,5"; hoj "C D8 640 16/11/2026"; hoj "C B10 2433 =SUMA(D2:D5)"
hoj 'C B6 0 =SUMAR.SI(A2:A5;"pan";D2:D5)'; hoj "C F1 2 Columna ancha"

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
  clic 251 71; sleep 0.8; clic 265 93; sleep 1                    # una fila nueva en la 3
  aparca; shot g3; tecla ctrl-z; shot g3z; tecla ctrl-y; shot g3y
  tecla ctrl-home right right; clic 175 71; sleep 0.8; clic 200 297; sleep 1   # fuera la columna C
  tecla ctrl-home down shift-down shift-down shift-down shift-down shift-right
  clic 375 96; sleep 1                                            # A2:B6, de menor a mayor
  tecla ctrl-home; aparca; shot g4
  tecla ctrl-b; sleep 1; escribe "vino"; tecla ret; sleep 1; aparca; shot g5
  tecla ctrl-home ctrl-p; sleep 4; shot g6; tecla ret; sleep 1
  tecla ctrl-g; sleep 2.5
  tecla ctrl-home down shift-down shift-down shift-down shift-right             # A2:B5: los nombres y las cantidades
  clic 251 71; sleep 0.8; clic 290 189; sleep 1.5; aparca; shot k1                 # Insertar > Gráfico de barras
  clic 300 48; clic 251 71; sleep 0.8; clic 290 209; sleep 1.5; aparca; shot k2   # ...de líneas
  clic 300 48; clic 251 71; sleep 0.8; clic 290 229; sleep 1.5; aparca; shot k3   # ...de sectores
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
import sys
d = open(sys.argv[1], 'rb').read()
l = d.split(b'\r\n')
ok = l[0].startswith(b'Lista de la compra de noviembTotal') and d.endswith(b'\x0c')   # (cortado donde empieza la celda siguiente)
ok = ok and l[1][:28] == b'leche'.ljust(17) + b'6'.rjust(11)            # A, de 16 letras; B, de 11
ok = ok and l[2][:28] == (b' ' * 6 + b'pan').ljust(17) + b'2'.rjust(11)  # centrado
ok = ok and l[4].startswith(b'vino') and b'#REF' in l[1]
ok = ok and all(len(x) <= 80 for x in l)
sys.exit(0 if ok else 1)
IMP
then ok "impresora: la hoja, con las columnas alineadas"; else ko "impresora: no ha salido lo esperado"; xxd $T/prn.txt | head -30; fi
# los gráficos de A2:B5 (leche 6, pan 2, Pan 3, vino 1), en su ventana
chk $T/k1.ppm titulo 130 100 activa "Gráfico de barras"
chk $T/k1.ppm busca 132 146 628 458 0 9 "leche"
chk $T/k1.ppm busca 132 146 628 458 0 9 "vino"
chk $T/k1.ppm rgb 215 300 1F4E5A "la barra de la leche, en petróleo"
chk $T/k2.ppm titulo 130 100 activa "Gráfico de líneas"
chk $T/k3.ppm titulo 130 100 activa "Gráfico de sectores"
chk $T/k3.ppm busca 132 146 628 458 0 9 "leche: 50%"
chk $T/k3.ppm busca 132 146 628 458 0 9 "pan: 17%"
chk $T/k3.ppm busca 132 146 628 458 0 9 "vino: 8%"

echo "== Cuentas: autosuma, corregir con F2, rellenar a la derecha, cortar, insertar columnas, eliminar filas, ir a, ancho y CSV =="
reabre ''
{ arranca
  escribe "10"; tecla ret; escribe "20"; tecla ret; escribe "30"; tecla ret
  clic 338 96; sleep 0.5; aparca; shot a1; tecla ret             # Autosuma en A4: propone la suma de encima
  tecla ctrl-home f2; escribe "5"; tecla home; escribe "2"; tecla ret   # A1: de 10 a 2105, con el cursor
  tecla ctrl-home right; escribe "=a1*2"; tecla ret up shift-right shift-right ctrl-d   # B1, hasta D1
  tecla ctrl-home down ctrl-x down down down down ctrl-v          # el 20 de A2, cortado y pegado en A6
  tecla ctrl-home right; clic 251 71; sleep 0.8; clic 265 113; sleep 1   # una columna nueva en la B
  aparca; shot a2
  tecla ctrl-home down down down down down; clic 175 71; sleep 0.8; clic 200 277; sleep 1   # fuera la fila 6
  tecla f5; sleep 1; escribe "c20"; tecla ret; sleep 1            # Ir a C20
  clic 251 71; sleep 0.8; clic 265 161; sleep 1                   # Insertar > Fecha de hoy
  clic 327 71; sleep 0.8; clic 340 237; sleep 1; tecla backspace backspace; escribe "20"; tecla ret; sleep 1   # ancho: 20 letras
  aparca; shot a3
  tecla ctrl-g; sleep 1.2; escribe "x.csv"; tecla ret; sleep 2.5
  sal; } | run 240 -vga std
muestra a1 cuentas_autosuma; muestra a3 cuentas_ira
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

rm -rf $T
exit $fail
