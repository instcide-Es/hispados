#!/bin/bash
# Prueba de Pluma (PLUMA.HVP), el procesador de textos, del grupo «Productividad».
#
# Abre Hispa Ventanas con Pluma (clave «Reabrir=» de CENTRO.INI) y:
#  - escribe un título grande y centrado, un párrafo largo (que se parte solo
#    por las palabras), letras en negrita, cursiva y subrayado, un párrafo a
#    la derecha y otro en rojo; lo guarda como CARTA.PLU;
#  - copia todo y lo pega cuatro veces (con su formato), y busca una palabra;
#  - arranca otra vez abriendo CARTA.PLU: se ve igual que antes de guardar;
#    lo imprime en texto llano y con formato (LPT1 a un archivo);
#  - abre un .TXT hecho fuera, le añade texto y lo guarda: sigue siendo texto.
#
# Necesita qemu-system-i386, mtools, socat e ImageMagick (como las demás).
set -u
IMG=${1:-build/hispados.img}
. tests/tools/hvp_arnes.sh
HVP=$B/PLUMA.HVP
[ -f $HVP ] || NASM=${NASM:-nasm} python3 tools/mkhvp.py programas/pluma.asm $HVP > /dev/null
reabre() { printf '[Centro]\r\nReabrir=A:\\HISPADOS\\PROGS\\PLUMA.HVP %s\r\n' "$1" > $T/CENTRO.INI
           mcopy -o -i $T/f.img $T/CENTRO.INI ::HISPADOS/CENTRO.INI; }

echo "== Pluma: escribir con formato, guardar, copiar y pegar, buscar =="
prepara $HVP
{ arranca
  shot v0
  tecla ctrl-l ctrl-t; escribe "Hispa Pluma"; tecla ret ctrl-l ctrl-q
  escribe "Esto es una prueba de texto normal que tiene que partirse sola al llegar al final de la linea de la hoja, por palabras."; tecla ret
  tecla ctrl-n; escribe "negrita "; tecla ctrl-n ctrl-k; escribe "cursiva "; tecla ctrl-k ctrl-s
  escribe "subrayado"; tecla ctrl-s; escribe " normal"; tecla ret
  tecla ctrl-d; escribe "A la derecha"; tecla ret ctrl-q
  clic 437 86; escribe "En rojo"            # el botón del color rojo
  aparca; shot e1
  tecla ctrl-g; sleep 1.2; escribe "carta"; tecla ret; sleep 2; shot e2
  escribe " y mas"; aparca; shot d1; tecla ctrl-z; shot d2; tecla ctrl-z; shot d3; tecla ctrl-z   # deshacer y rehacer
  tecla ctrl-a ctrl-c ctrl-end ret ctrl-v; sleep 0.5; tecla ctrl-v ctrl-v ctrl-v; sleep 1; aparca; shot e3
  tecla ctrl-b; sleep 1; escribe "hoja"; tecla ret; sleep 1; shot e4
  sal; } | run 240 -vga std

chk $T/v0.ppm titulo 70 40 activa "Pluma - (sin nombre)"
chk $T/v0.ppm busca 72 518 400 540 0 6 "Línea 1 de 1, columna 1    0 letras"
# el párrafo largo, partido por las palabras (tinta sobre blanco)
chk $T/e1.ppm busca 80 155 90 170 0 9 "Esto es una prueba de texto normal que tiene que partirse sola al"
chk $T/e1.ppm busca 80 171 90 186 0 9 "llegar al final de la linea de la hoja, por palabras."
chk $T/e1.ppm busca 280 187 300 202 0 9 "normal"          # detrás de las letras con formato
chk $T/e1.ppm busca 555 203 570 218 0 9 "A la derecha"    # arrimado a la derecha
chk $T/e1.ppm busca 80 219 90 234 13 9 "En rojo"
chk $T/e1.ppm nobusca 80 123 660 150 0 9 "Hispa Pluma"    # (el título no es de letra normal)
chk $T/e1.ppm rgb 290 150 FFFFFF "blanco a la izquierda del título centrado"
chk $T/e1.ppm busca 72 518 400 540 0 6 "Línea 6 de 6, columna 8    185 letras"
chk $T/e2.ppm titulo 70 40 activa "Pluma - carta.PLU"
chk $T/d1.ppm busca 72 518 500 540 0 6 "191 letras"
chk $T/d2.ppm busca 72 518 500 540 0 6 "185 letras"       # deshecha la tanda entera
chk $T/d2.ppm nobusca 80 219 400 234 13 9 "En rojo y mas"
chk $T/d3.ppm busca 80 219 90 234 13 9 "En rojo y mas"    # y rehecha
chk $T/e3.ppm busca 72 518 500 540 0 6 "926 letras"       # 185 + 1 + 4 x 185
chk $T/e4.ppm busca 72 110 660 140 9 2 "hoja"             # lo encontrado, elegido
muestra v0 pluma_vacia; muestra e1 pluma_formato; muestra e3 pluma_pegado; muestra e4 pluma_buscar
mcopy -n -i $T/f.img ::CARTA.PLU $T/carta.plu 2>/dev/null
if python3 - $T/carta.plu <<'PY'
import sys,struct
d=open(sys.argv[1],'rb').read()
n=struct.unpack_from('<I',d,8)[0]
t,a=d[12:12+n],d[12+n:12+2*n]
ok = d[:8]==b'PLUMA1\x1a\x00' and n==186 and len(d)==12+2*n and t.endswith(b'En rojo\n')
ok = ok and t.startswith(b'Hispa Pluma\n') and a[0]==8 and a[11]&3==1          # grande; centrado
ok = ok and a[t.index(b'negrita')]==1 and a[t.index(b'cursiva')]==2 and a[t.index(b'subrayado')]==4
ok = ok and a[t.index(b'A la derecha')+12]&3==2 and a[t.index(b'En rojo')]==0x10
sys.exit(0 if ok else 1)
PY
then ok "CARTA.PLU: el texto y el formato de cada letra"; else ko "CARTA.PLU no es lo esperado"; fi

echo "== Pluma: abrir el documento guardado; imprimir =="
reabre 'A:\CARTA.PLU'
rm -f $T/prn.txt
{ arranca
  shot a1
  tecla ctrl-p; sleep 4; shot a2; tecla ret; sleep 1
  clic 112 75; sleep 1; clic 200 205; sleep 4; shot a3; tecla ret; sleep 1   # Archivo > Imprimir con formato
  sal; } | PARALELO=file:$T/prn.txt run 150 -vga std
chk $T/a1.ppm titulo 70 40 activa "Pluma - A:\\CARTA.PLU"
chk $T/a1.ppm igual 76 118 592 120 $T/e1.ppm 76 118 80     # como antes de guardar (salvo el cursor)
chk $T/a2.ppm busca 150 250 650 310 0 8 "El documento se ha enviado a la impresora."
chk $T/a3.ppm busca 150 250 650 310 0 8 "El documento se ha enviado a la impresora."
if python3 - $T/prn.txt <<'PY'
import sys
d=open(sys.argv[1],'rb').read()
llano,_,fmt=d.partition(b'\x0c')
l=llano.split(b'\r\n')
ok = l[0]==b' '*(4+25)+b'Hispa Pluma'                     # (576-176)/2 puntos = 25 letras
ok = ok and l[1]==b'    Esto es una prueba de texto normal que tiene que partirse sola al'
ok = ok and l[3]==b'    negrita cursiva subrayado normal'
ok = ok and l[4]==b' '*(4+60)+b'A la derecha' and l[5]==b'    En rojo'
ok = ok and fmt.startswith(b'\x1b@') and fmt.endswith(b'\x0c')
ok = ok and b'\x1bW\x01Hispa Pluma\r\n' in fmt and b'\x1bW\x00Esto es' in fmt
ok = ok and b'\x1bEnegrita \x1bF\x1b4cursiva \x1b5\x1b-\x01subrayado\x1b-\x00 normal\r\n' in fmt
sys.exit(0 if ok else 1)
PY
then ok "impresora: texto llano y con formato (ESC/P)"; else ko "impresora: no ha salido lo esperado"; xxd $T/prn.txt | head -40; fi

echo "== Pluma: un archivo de texto llano =="
printf 'Primera linea\r\nSegunda\r\n' > $T/NOTA.TXT
mcopy -o -i $T/f.img $T/NOTA.TXT ::NOTA.TXT
reabre 'A:\NOTA.TXT'
{ arranca
  tecla ctrl-end; shot t1
  escribe "fin"; tecla ctrl-g; sleep 2.5
  clic 677 53; sleep 1.2; shot t2       # cerrar: guardado, no pregunta
  sal; } | run 120 -vga std
chk $T/t1.ppm busca 80 123 90 138 0 9 "Primera linea"
chk $T/t1.ppm busca 80 139 90 154 0 9 "Segunda"
if [ "$(mtype -i $T/f.img ::NOTA.TXT | od -An -c | tr -s ' \n' ' ')" = "$(printf 'Primera linea\r\nSegunda\r\nfin' | od -An -c | tr -s ' \n' ' ')" ]; then
  ok "NOTA.TXT: sigue siendo texto llano, con lo añadido"
else
  ko "NOTA.TXT no es lo esperado"; mtype -i $T/f.img ::NOTA.TXT | od -c | head
fi
chk $T/t2.ppm nobusca 70 40 690 60 9 4 "Pluma"            # la ventana se ha cerrado sin preguntar

rm -rf $T
exit $fail
