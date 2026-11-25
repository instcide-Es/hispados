# Hispa-8086: la prueba de CUENTAS, la hoja de cálculo.
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# (la carga prueba.sh: usa sus arranca, escribe, tecla, pantalla, esta...)

# un CSV de la prueba, con los retornos de DOS
csv_dos() { printf '%s\r\n' "$@"; }

p_cuentas() {
echo "== Hispa-8086: CUENTAS (la hoja de cálculo) =="
cp $IMG/hispa8086-360.img $T/a.img
cp $IMG/oficina8086-360.img $T/b.img
# las cuentas: 0,1 + 0,2; un tercio por tres; redondeos de moneda; fechas;
# un círculo; las fórmulas a la manera de VisiCalc y 1-2-3; SI, MEDIA...
csv_dos '0,1;0,2;=A1+B1;=C1=0,3' \
        '=1/3;=A2*3;=REDONDEAR(2,675;2);=REDONDEAR(-2,675;2)' \
        '=B3+1;=A3+1;+A1*10;@SUM(A1...C1)' \
        '10;20;30;@AVG(A4..C4)' \
        '=SUMA(A4:C4;5);=MEDIA(A4:C4);=MAX(A4:C4)-MIN(A4:C4);=CONTAR(A1:D2;A8:D8)' \
        '=SI(A4>15;1;2);=SI(A4<15;1;2);=2^10;=ENTERO(-2,5)' \
        '24/11/2026;=A7+7;21%;=12345678901*1' \
        'hola;=10/0;=ABS(-3,5)*2;1.234,5' > $T/E.CSV
mcopy -o -i $T/b.img $T/E.CSV ::
cp build/CUENTAS.COM $T/
QMAS="-drive file=$T/b.img,format=raw,if=floppy,index=1" arranca $T/a.img 486
escribe "limpiar"; escribe "b:cuentas b:e.csv /i b:s.txt"; sleep 3
escribe "b:cuentas b:e.csv /g b:s.cta"; sleep 3
escribe "b:cuentas b:s.cta /g b:s2.csv"; sleep 3
escribe "b:cuentas b:e.csv /g b:s.dif"; sleep 3
escribe "b:cuentas b:s.dif /g b:s3.csv"; sleep 3
escribe "b:cuentas b:nohay.csv /g b:x.csv"; sleep 2
pantalla k0
esta k0 "CUENTAS: no se ha podido leer o escribir el archivo." "un archivo que no existe: "
apaga
[ -n "${CU_DEPURA:-}" ] && cp $T/b.img "$CU_DEPURA"
mtype -i $T/b.img ::S.TXT > $T/s.txt 2>/dev/null
mtype -i $T/b.img ::S2.CSV > $T/s2.csv 2>/dev/null
mtype -i $T/b.img ::S3.CSV > $T/s3.csv 2>/dev/null
mtype -i $T/b.img ::S.DIF > $T/s.dif 2>/dev/null
cp $T/s.txt $T/k_impreso.txt
fila() { sed -n "${1}p" $T/s.txt | tr -d '\r'; }
mira() { if [ "$(fila $1)" = "$2" ]; then ok "$3"; else ko "$3: sale «$(fila $1)», se esperaba «$2»"; fi; }
mira 1 '       0,1       0,2       0,3         1' "0,1 + 0,2 da 0,3 exacto (y la comparación =0,3 da 1)"
mira 2 '  0,333333         1      2,68     -2,68' "un tercio por tres da 1; REDONDEAR(2,675;2) da 2,68 (y -2,68: la mitad, lejos del cero)"
mira 3 '     #CIRC     #CIRC         1       0,6' "un círculo sale #CIRC (no da vueltas); +A1*10 y @SUM(A1...C1) de VisiCalc"
mira 4 '        10        20        30        20' "@AVG(A4..C4) de 1-2-3"
mira 5 '        65        20        20        10' "SUMA con un número suelto, MEDIA, MAX-MIN y CONTAR de dos rangos (los errores y los textos no cuentan)"
mira 6 '         2         1      1024        -3' "SI, la potencia y ENTERO(-2,5) hacia abajo"
mira 7 '24/11/2026     46357       21%##########' "las fechas (la serie, como en las hojas de hoy: 46350 + 7), un tanto por ciento, y un número que no cabe: almohadillas"
mira 8 'hola          #DIV/0         7    1234,5' "un texto, la división por cero y un número con punto de miles"
# lo guardado en el formato propio y vuelto a leer: lo mismo en CSV
if grep -q ';"=REDONDEAR(2,675;2)";' $T/s2.csv; then ok "guardar .CTA y volver a abrirlo: las fórmulas siguen ahí (y en el CSV, entre comillas si llevan «;»)"; else ko "el .CTA vuelto a leer no tiene las fórmulas: $(head -3 $T/s2.csv)"; fi
if grep -q '^24/11/2026;=A7+7;21%;=12345678901\*1' $T/s2.csv; then ok "la fecha y el tanto por ciento se guardan como se escribieron (CSV)"; else ko "fila 7 del CSV: $(sed -n 7p $T/s2.csv)"; fi
if [ "$(sed -n 1p $T/s2.csv | tr -d '\r')" = '0,1;0,2;=A1+B1;=C1=0,3' ]; then ok "CSV con «;» y coma decimal: la fila 1, igual que la de entrada"; else ko "CSV fila 1: $(sed -n 1p $T/s2.csv)"; fi
# DIF: la cabecera de VisiCalc y los valores (no las fórmulas)
if head -12 $T/s.dif | tr -d '\r' | tr '\n' '|' | grep -q '^TABLE|0,1|""|VECTORS|0,4|""|TUPLES|0,8|""|DATA|0,0|""|'; then ok "DIF: la cabecera (TABLE, VECTORS 4, TUPLES 8, DATA)"; else ko "DIF: cabecera $(head -12 $T/s.dif | tr -d '\r' | tr '\n' '|')"; fi
if tr -d '\r' < $T/s.dif | tr '\n' '|' | grep -q '|0,0.3|V|'; then ok "DIF: 0,1 + 0,2 sale como valor, 0.3 con punto"; else ko "DIF: no está el 0.3"; fi
if [ "$(sed -n 1p $T/s3.csv | tr -d '\r')" = '0,1;0,2;0,3;1' ]; then ok "DIF vuelto a leer: los valores"; else ko "DIF leído: $(sed -n 1p $T/s3.csv)"; fi
if [ "$(sed -n 8p $T/s3.csv | tr -d '\r')" = 'hola;#DIV/0;7;1234,5' ] || [ "$(sed -n 8p $T/s3.csv | tr -d '\r')" = 'hola;;7;1234,5' ]; then ok "DIF: los textos y los errores"; else ko "DIF leído, fila 8: $(sed -n 8p $T/s3.csv)"; fi
if grep -q "^\s*cpu 8086" programas/cuentas.asm && ! grep -q "^\s*cpu [123]86" programas/cuentas.asm programas/cuentas/*.inc; then ok "programas/cuentas.asm se ensambla con «cpu 8086» (y nada de 286)"; else ko "CUENTAS no va solo con instrucciones del 8086"; fi
tam=$(stat -c %s build/CUENTAS.COM)
if [ $tam -lt 20480 ]; then ok "CUENTAS.COM ocupa $tam octetos"; else ko "CUENTAS.COM ha crecido a $tam octetos"; fi
libre=$(mdir -i build/oficina8086-360.img :: | awk '/bytes free/ { gsub(/ /, "", $0); sub(/bytesfree/, "", $0); print }')
if [ "${libre:-0}" -gt 200000 ]; then ok "el disquete de Oficina (360 K) tiene sitio: $libre octetos libres"; else ko "el disquete de Oficina se llena ($libre)"; fi
}

# el color (atributo) de la fila $1, columna $2 de la pantalla, en hexadecimal
atributo() {
  mon "pmemsave 0xb8000 4000 \"$T/atr.bin\"" 0.4
  python3 -c "import sys; d=open(sys.argv[1],'rb').read(); print('%02x' % d[($1*80+$2)*2+1])" $T/atr.bin $1 $2
}
p_cuentas_pantalla() {
echo "== Hispa-8086: CUENTAS en la pantalla =="
cp $IMG/hispa8086-360.img $T/a.img
cp $IMG/oficina8086-360.img $T/b.img
QMAS="-drive file=$T/b.img,format=raw,if=floppy,index=1" arranca $T/a.img 486
escribe "limpiar"; escribe "b:cuentas"; sleep 2
pantalla c1
esta c1 "LISTO" "arranca y pinta: "
esta c1 "         A         B         C         D         E         F         G" "los rótulos de las columnas: "
esta c1 "  21" "veintiuna filas a la vista: "
esta c1 " F1 Ayuda F2 Editar F3 Copiar F4 Pegar F5 Ir a F8 Marcar F9 Calcular F10 Menú" "la línea de las teclas, abajo: "
a=$(atributo 3 5); b=$(atributo 3 15); c=$(atributo 2 20)
if [ "$a" = 70 ] && [ "$b" = 07 ] && [ "$c" = 70 ]; then ok "la celda del cursor, en vídeo inverso (no depende del color); los rótulos también"; else ko "atributos: cursor $a, al lado $b, rótulos $c"; fi
escribe "0,1"; escribe "0,2"; escribe "=a1+a2"; sleep 0.5
pantalla c2
esta c2 "   3        0,3" "0,1 + 0,2 en la pantalla: "
tecla up f3 right f4; sleep 0.5
pantalla c3
esta c3 "B3 =B1+B2" "F3 y F4: la fórmula copiada mueve sus referencias: "
tecla ctrl-z; sleep 0.5
pantalla c4
esta c4 "Deshecho (Ctrl+Z otra vez: rehacer)." "Ctrl+Z: "
noesta c4 "B3 =B1+B2" "y la copia ya no está: "
tecla left slash f; sleep 0.5
pantalla c5
esta c5 "General  Decimales fijos  Euros  Pesetas  Tanto por ciento  Fecha" "el menú de formatos (/F): "
tecla e; sleep 0.5
pantalla c6
esta c6 "A3 (Euros) =A1+A2" "el formato, arriba: "
esta c6 "   3      0,30ε" "euros con dos decimales y su signo (en VGA se dibuja el €): "
tecla f5; escribe "a10"; escribe "=a10+1"; sleep 0.5
pantalla c7
esta c7 "Referencia circular en A10" "una referencia circular: aviso, "
esta c7 "  10      #CIRC" "y #CIRC en la celda (sin dar vueltas): "
tecla up; escribe "=suma(a1"; sleep 0.5
pantalla c8
esta c8 "Fórmula mal escrita" "una fórmula mal escrita no se acepta: "
esta c8 "=suma(a1" "y se puede corregir: "
tecla end shift-0 ret; sleep 0.5
pantalla c9
esta c9 "  10        0,1" "corregida, se calcula: "
noesta c9 "Fórmula mal escrita" "y el aviso se va: "
tecla slash g; escribe "b:p.cta"; sleep 1.5
pantalla c10
esta c10 "Guardado." "/G guarda: "
tecla slash s; sleep 0.5
escribe "b:cuentas b:p.cta /i b:p.txt"; sleep 2
apaga
mtype -i $T/b.img ::P.TXT | tr -d '\r' > $T/p.txt
cp $T/p.txt $T/k_p.txt
if grep -q '^  0,30 EUR' $T/p.txt; then ok "lo guardado en .CTA conserva el formato (impreso: «0,30 EUR»)"; else ko "P.TXT: $(head -3 $T/p.txt)"; fi
}

p_cuentas_ordenar() {
echo "== Hispa-8086: CUENTAS, ordenar, formatos y anchos =="
cp $IMG/hispa8086-360.img $T/a.img
cp $IMG/oficina8086-360.img $T/b.img
QMAS="-drive file=$T/b.img,format=raw,if=floppy,index=1" arranca $T/a.img 486
escribe "limpiar"; escribe "b:cuentas"; sleep 2
escribe "pera"; escribe "Manzana"; escribe "naranja"; escribe "uva"
tecla right up up up up
escribe "30"; escribe "1000"; escribe "4,5"; escribe "250"
tecla right up up up up
escribe "=b1*2"; tecla up f3 shift-down shift-down shift-down f4; sleep 0.5
tecla esc home shift-right shift-right shift-down shift-down shift-down; sleep 0.3
tecla slash o a; escribe "a"; sleep 1
pantalla o1
esta o1 "   1 Manzana         1000      2000" "ordenar por la primera columna, sin distinguir mayúsculas: "
esta o1 "   4 uva              250       500" "(y las fórmulas se mueven con su fila): "
tecla slash o d; escribe "b"; sleep 1
pantalla o2
esta o2 "   1 Manzana         1000      2000" "descendente por la columna del cursor: "
esta o2 "   2 uva              250       500" ""
esta o2 "   4 naranja          4,5         9" ""
tecla esc home right slash f p; sleep 0.5
tecla down slash f t; escribe "1"; sleep 0.5
tecla down slash f d; escribe "3"; sleep 0.5
tecla ctrl-right ctrl-right; sleep 0.5
pantalla o3
esta o3 "1.000₧" "pesetas, con el punto de los miles y ₧: "
esta o3 "25000,0%" "tanto por ciento con un decimal: "
esta o3 "30,000" "decimales fijos: "
esta o3 "         A          B          C" "Ctrl+→ ensancha la columna: "
esta o3 "B3 (Fijo 3) 30" "y arriba, el formato con sus decimales: "
apaga
}

p_cuentas_grande() {
echo "== Hispa-8086: CUENTAS con una hoja grande (memoria llena y juntar los huecos) =="
cp $IMG/hispa8086-360.img $T/a.img
rm -f $T/b.img; mformat -C -f 1440 -v GRANDE -i $T/b.img ::     # (en 360 K no cabría lo impreso)
mcopy -i $T/b.img build/CUENTAS.COM ::
# 26 columnas por 500 filas: 13.000 números (unos 208 K de celdas)
python3 - $T/G.CSV <<'PY'
import sys
with open(sys.argv[1], 'w', newline='') as f:
    for r in range(500):
        f.write(';'.join('%d,%02d' % divmod(r * 26 + c + 1, 100) for c in range(26)))
        f.write(';=SUMA(A1:Z500)\r\n' if r == 0 else '\r\n')
PY
mcopy -o -i $T/b.img $T/G.CSV ::
QMAS="-drive file=$T/b.img,format=raw,if=floppy,index=1" arranca $T/a.img 486
escribe "limpiar"; escribe "b:cuentas b:g.csv"; sleep 8
pantalla g1
esta g1 "  21       5,21      5,22" "abre un CSV de 13.000 celdas: "
tecla f8 end; sleep 1
pantalla g2
esta g2 "A1:AA500" "F8 y Fin marcan hasta la última celda: "
tecla slash f e; sleep 15
tecla home; sleep 1; tecla f5; escribe "aa1"; sleep 1
tecla slash g; escribe "b:g.cta"; sleep 6
pantalla g3
esta g3 "Guardado." "13.000 celdas con otro formato (el montón se junta varias veces) y guardadas: "
tecla slash s; sleep 0.5
escribe "b:cuentas b:g.cta /i b:g.txt"; sleep 10
escribe "b:cuentas b:g.cta /g b:g2.csv"; sleep 10
pantalla g4
apaga
[ -n "${CU_DEPURA:-}" ] && cp $T/b.img "$CU_DEPURA"
mtype -i $T/b.img ::G.TXT | tr -d '\r' > $T/g.txt
# la suma de 0,01 a 130,00: 13.000 x 13.001 / 2 céntimos = 845.006,50
mtype -i $T/b.img ::G2.CSV | tr -d '\r' > $T/g2.csv
if head -1 $T/g.txt | grep -q '^  0,01 EUR  0,02 EUR' && [ "$(head -1 $T/g2.csv | cut -d';' -f27)" = '=SUMA(A1:Z500)' ]; then ok "tras juntar los huecos, todo sigue: las celdas en euros y la fórmula"; else ko "la hoja grande: $(head -1 $T/g.txt | cut -c1-60) / $(head -1 $T/g2.csv | cut -d';' -f27)"; fi
if [ "$(sed -n 500p $T/g.txt | grep -o '130,00 EUR$')" = "130,00 EUR" ] && [ "$(sed -n 500p $T/g2.csv | cut -d';' -f26)" = "130" ]; then ok "la última celda, Z500, en su sitio y en euros"; else ko "fila 500: $(sed -n 500p $T/g.txt | rev | cut -c1-30 | rev)"; fi
}

# (del núcleo, pero salió al probar CUENTAS con hojas grandes) un archivo que
# hace pasar la FAT de un sector a otro con el primero cambiado: antes, al
# volcarlo, se perdía cuál era el siguiente y se leía otro sector
p_fat_sectores() {
echo "== Hispa-8086: archivos grandes en 1,44 M (la FAT de un sector a otro) =="
cp $IMG/hispa8086-1440.img $T/a.img
head -c 200000 /dev/urandom > $T/GRANDE.DAT
mcopy -i $T/a.img $T/GRANDE.DAT ::
arranca $T/a.img 486
escribe "limpiar"; escribe "copiar grande.dat copia.dat"; sleep 6
apaga
mtype -i $T/a.img ::COPIA.DAT > $T/copia.dat 2>/dev/null
if cmp -s $T/copia.dat $T/GRANDE.DAT; then ok "COPIAR de 200.000 octetos en 1,44 M: la copia es idéntica"; else ko "la copia de 200.000 octetos no es igual"; fi
if fsck.fat -n $T/a.img > $T/fsck.txt 2>&1; then ok "fsck.fat no encuentra ningún error"; else ko "fsck.fat encuentra errores"; cat $T/fsck.txt; fi
}
