# Hispa-8086: la prueba de CUENTAS, la hoja de cálculo.
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# (la carga prueba.sh: usa sus arranca, escribe, tecla, pantalla, esta...)

# un CSV de la prueba, con los retornos de DOS
csv_dos() { printf '%s\r\n' "$@"; }

p_cuentas() {
echo "== Hispa-8086: CUENTAS (la hoja de cálculo) =="
cp $IMG/hispa8086-360.img $T/a.img
cp build/oficina8086-360.img $T/b.img
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
tam=$(stat -c %s build/CUENTAS.COM)
ok "CUENTAS.COM ocupa $tam octetos"
}

# el color (atributo) de la fila $1, columna $2 de la pantalla, en hexadecimal
atributo() {
  mon "pmemsave 0xb8000 4000 \"$T/atr.bin\"" 0.4
  python3 -c "import sys; d=open(sys.argv[1],'rb').read(); print('%02x' % d[($1*80+$2)*2+1])" $T/atr.bin $1 $2
}
p_cuentas_pantalla() {
echo "== Hispa-8086: CUENTAS en la pantalla =="
cp $IMG/hispa8086-360.img $T/a.img
cp build/oficina8086-360.img $T/b.img
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
