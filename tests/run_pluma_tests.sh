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
  tecla ctrl-w ctrl-t; escribe "Hispa Pluma"; tecla ret ctrl-w ctrl-q
  escribe "Esto es una prueba de texto normal que tiene que partirse sola al llegar al final de la linea de la hoja, por palabras."; tecla ret
  tecla ctrl-n; escribe "negrita "; tecla ctrl-n ctrl-k; escribe "cursiva "; tecla ctrl-k ctrl-s
  escribe "subrayado"; tecla ctrl-s; escribe " normal"; tecla ret
  tecla ctrl-d; escribe "A la derecha"; tecla ret ctrl-q
  clic 557 127; escribe "En rojo"            # el botón del color rojo
  aparca; shot e1
  tecla ctrl-g; sleep 1.2; escribe "carta"; tecla ret; sleep 2; shot e2
  escribe " y mas"; aparca; shot d1; tecla ctrl-z; shot d2; tecla ctrl-y; shot d3; tecla ctrl-z   # deshacer y rehacer
  tecla ret; escribe "uno"; tecla left right; escribe "dos"; tecla ctrl-z ctrl-z; shot d4   # dos cambios, deshechos los dos
  tecla ctrl-y ctrl-y; shot d5; tecla ctrl-z ctrl-z                                        # rehechos, y otra vez fuera
  tecla ctrl-e ctrl-c ctrl-end ret ctrl-v; sleep 0.5; tecla ctrl-v ctrl-v ctrl-v; sleep 1; aparca; shot e3
  tecla ctrl-b; sleep 1; escribe "hoja"; tecla ret; sleep 1; shot e4
  sal; } | run 240 -vga std

chk $T/v0.ppm titulo 70 40 activa "Pluma - (sin nombre)"
chk $T/v0.ppm busca 72 518 400 540 0 6 "Línea 1 de 1, columna 1    0 letras"
# el párrafo largo, partido por las palabras (tinta sobre blanco)
chk $T/e1.ppm busca 80 183 90 198 0 9 "Esto es una prueba de texto normal que tiene que partirse sola al"
chk $T/e1.ppm busca 80 199 90 214 0 9 "llegar al final de la linea de la hoja, por palabras."
chk $T/e1.ppm busca 280 215 300 230 0 9 "normal"          # detrás de las letras con formato
chk $T/e1.ppm busca 555 231 570 246 0 9 "A la derecha"    # arrimado a la derecha
chk $T/e1.ppm busca 80 247 90 262 13 9 "En rojo"
chk $T/e1.ppm nobusca 80 151 660 178 0 9 "Hispa Pluma"    # (el título no es de letra normal)
chk $T/e1.ppm rgb 290 178 FFFFFF "blanco a la izquierda del título centrado"
chk $T/e1.ppm busca 72 518 400 540 0 6 "Línea 6 de 6, columna 8    185 letras"
chk $T/e2.ppm titulo 70 40 activa "Pluma - carta.PLU"
chk $T/d1.ppm busca 72 518 500 540 0 6 "191 letras"
chk $T/d2.ppm busca 72 518 500 540 0 6 "185 letras"       # deshecha la tanda entera
chk $T/d2.ppm nobusca 80 247 400 262 13 9 "En rojo y mas"
chk $T/d3.ppm busca 80 247 90 262 13 9 "En rojo y mas"    # y rehecha
chk $T/d4.ppm busca 72 518 500 540 0 6 "185 letras"       # deshacer, dos pasos
chk $T/d5.ppm busca 72 518 500 540 0 6 "192 letras"       # y rehacer, los dos
chk $T/e3.ppm busca 72 518 500 540 0 6 "926 letras"       # 185 + 1 + 4 x 185
chk $T/e4.ppm busca 72 138 660 168 9 2 "hoja"             # lo encontrado, elegido
muestra m1 pluma_menu; muestra v0 pluma_vacia; muestra e1 pluma_formato; muestra e3 pluma_pegado; muestra e4 pluma_buscar
mcopy -n -i $T/f.img ::CARTA.PLU $T/carta.plu 2>/dev/null
if python3 - $T/carta.plu <<'PY'
import sys,struct
d=open(sys.argv[1],'rb').read()
n=struct.unpack_from('<I',d,8)[0]
t,a=d[12:12+n],d[12+n:12+2*n]
ok = d[:8]==b'PLUMA1\x1a\x00' and n==186 and len(d)==12+3*n and t.endswith(b'En rojo\n')
ok = ok and t.startswith(b'Hispa Pluma\n') and a[0]==0 and d[12+2*n]==0x60 and a[11]&3==1     # 24 puntos; centrado
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
  clic 112 75; sleep 1; shot m1; clic 200 261; sleep 4; shot a3; tecla ret; sleep 1   # Archivo > Imprimir con formato
  clic 112 75; sleep 1; shot m2; clic 200 281; sleep 25; shot a4; tecla ret; sleep 1   # Archivo > Imprimir como se ve
  sal; } | PARALELO=file:$T/prn.txt run 200 -vga std
chk $T/a1.ppm titulo 70 40 activa "Pluma - A:\\CARTA.PLU"
chk $T/a1.ppm igual 76 146 592 120 $T/e1.ppm 76 146 80     # como antes de guardar (salvo el cursor)
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
chk $T/a4.ppm busca 150 250 650 310 0 8 "El documento se ha enviado a la impresora."
muestra m2 pluma_menu_archivo
# lo impreso «como se ve»: se rehace la hoja a partir de las tiras de puntos
if python3 - $T/prn.txt $B/pluma_impresa.pgm <<'PY'
import sys
d=open(sys.argv[1],'rb').read()
i=d.rfind(b'\x1b@\x1bA\x08')
if i<0: sys.exit(1)
i+=5; filas=[]
while i<len(d) and d[i]!=12:
    if d[i:i+5]==b'\x1b*\x05\x40\x02':            # una tira con puntos, y su avance
        col=d[i+5:i+5+576]; i+=5+576
        for r in range(8): filas.append([1 if c&(0x80>>r) else 0 for c in col])
        if d[i:i+2]!=b'\r\n': sys.exit(2)
        i+=2
    elif d[i:i+2]==b'\r\n':                         # una tira en blanco: solo el avance
        filas+=[[0]*576 for _ in range(8)]; i+=2
    else: sys.exit(2)
tinta=sum(map(sum,filas))
open(sys.argv[2],'wb').write(b'P5 576 %d 255\n'%len(filas)+bytes(0 if v else 255 for f in filas for v in f))
# el título, de 24 puntos y centrado: tinta en el centro de las primeras filas y nada en los lados
arriba=filas[:32]
centro=sum(sum(f[200:376]) for f in arriba); lados=sum(sum(f[:150])+sum(f[426:]) for f in arriba)
ok = d[-1]==12 and tinta>3000 and centro>150 and lados==0 and len(filas)>=96
sys.exit(0 if ok else 3)
PY
then ok "impresora: la hoja como se ve, punto por punto (título centrado de 24 puntos)"; convert $B/pluma_impresa.pgm $B/pluma_impresa.png
else ko "impresora: la hoja «como se ve» no ha salido bien"; fi

echo "== Pluma: un archivo de texto llano =="
printf 'Primera linea\r\nSegunda\r\n' > $T/NOTA.TXT
mcopy -o -i $T/f.img $T/NOTA.TXT ::NOTA.TXT
reabre 'A:\NOTA.TXT'
{ arranca
  tecla ctrl-end; shot t1
  tecla ctrl-l; sleep 1; escribe "linea"; tecla ret; sleep 1; escribe "raya"; tecla ret; sleep 1.5; shot t3; tecla ret; sleep 1   # Reemplazar
  tecla ctrl-i; sleep 1; escribe "2"; tecla ret; sleep 1; escribe "x"; tecla ctrl-end                                              # Ir a la línea 2 (Ctrl+I: Ventanas lo distingue de Tab)
  escribe "fin"; tecla ctrl-g; sleep 2.5
  clic 677 53; sleep 1.2; shot t2       # cerrar: guardado, no pregunta
  sal; } | run 120 -vga std
chk $T/t1.ppm busca 80 151 90 166 0 9 "Primera linea"
chk $T/t1.ppm busca 80 167 90 182 0 9 "Segunda"
if [ "$(mtype -i $T/f.img ::NOTA.TXT | od -An -c | tr -s ' \n' ' ')" = "$(printf 'Primera raya\r\nxSegunda\r\nfin' | od -An -c | tr -s ' \n' ' ')" ]; then
  ok "NOTA.TXT: texto llano, con lo reemplazado, lo de «Ir a» y lo añadido"
else
  ko "NOTA.TXT no es lo esperado"; mtype -i $T/f.img ::NOTA.TXT | od -c | head
fi
chk $T/t3.ppm busca 150 250 650 320 0 8 "1 cambio."
chk $T/t2.ppm nobusca 70 40 690 60 9 4 "Pluma"            # la ventana se ha cerrado sin preguntar

echo "== Pluma: listas, sangría, justificado, la brocha y las marcas =="
reabre ''
mdel -i $T/f.img ::LISTA.PLU 2>/dev/null
{ arranca
  tecla ctrl-n; escribe "gordo"; tecla ctrl-n; escribe " fino"; tecla ret
  clic 448 127; escribe "uno"; tecla ret; escribe "dos"; tecla ret; clic 448 127      # viñetas
  clic 422 127; escribe "tres"; tecla ret; escribe "cuatro"; tecla ret; clic 422 127  # numeración
  clic 500 127; clic 500 127; escribe "sangrado"; tecla ret; clic 474 127; clic 474 127   # sangría: dos pasos y fuera
  tecla ctrl-j; escribe "Esto es una prueba de texto normal que tiene que partirse sola al llegar al final de la linea de la hoja, por palabras."
  clic 96 163; clic 282 99                                    # en «gordo»; la brocha
  ir 133 163; mon "mouse_button 1" 0.25; ir 150 163; ir 165 163; mon "mouse_button 0" 0.5   # y se pasa por «fino»
  aparca; shot l1
  ir 372 99; shot l3; clic 372 99; aparca; shot l2            # el nombre del botón ¶; las marcas a la vista
  tecla ctrl-g; sleep 1.2; escribe "lista"; tecla ret; sleep 2
  tecla ctrl-p; sleep 4; tecla ret; sleep 1
  sal; } | PARALELO=file:$T/prn2.txt run 200 -vga std
chk $T/l1.ppm busca 104 167 112 182 0 9 "uno"             # detrás de su viñeta
chk $T/l1.ppm busca 104 199 112 214 0 9 "tres"
chk $T/l1.ppm busca 84 199 100 214 0 9 "1."
chk $T/l1.ppm busca 84 215 100 230 0 9 "2."
chk $T/l1.ppm busca 144 231 152 246 0 9 "sangrado"        # dos pasos de sangría
chk $T/l1.ppm busca 640 247 648 262 0 9 "al"              # justificado: la línea acaba en el margen derecho
chk $T/l1.ppm busca 80 263 90 278 0 9 "llegar al final de la linea de la hoja, por palabras."   # la última, no
chk $T/l3.ppm busca 72 518 600 540 0 6 "Mostrar u ocultar los espacios y las marcas de párrafo"
chk $T/l2.ppm nobusca 80 263 90 278 0 9 "llegar al final"  # con las marcas, los espacios llevan su punto
muestra l1 pluma_listas; muestra l2 pluma_marcas
mcopy -n -i $T/f.img ::LISTA.PLU $T/lista.plu 2>/dev/null
if python3 - $T/lista.plu $T/prn2.txt <<'PY'
import sys,struct
d=open(sys.argv[1],'rb').read()
n=struct.unpack_from('<I',d,8)[0]
t,a=d[12:12+n],d[12+n:12+2*n]
m=[a[i] for i,c in enumerate(t) if c==10]                 # las marcas de los párrafos
ok = m==[0,4,4,0x80,0x80,0x20,3] 
ok = ok and a[0]==1 and a[5]==0 and all(x==1 for x in a[6:10])   # «fino», en negrita por la brocha
l=open(sys.argv[2],'rb').read().split(b'\r\n')
ok = ok and l[1]==b'     - uno' and l[3]==b'    1. tres' and l[4]==b'    2. cuatro' and l[5]==b'            sangrado'
sys.exit(0 if ok else 1)
PY
then ok "LISTA.PLU: los párrafos (viñetas, números, sangría, justificado), la brocha y la impresión"
else ko "LISTA.PLU o lo impreso no es lo esperado"; xxd $T/lista.plu | tail -12; xxd $T/prn2.txt | head -12; fi

echo "== Pluma: los tipos de letra =="
reabre ''
mdel -i $T/f.img ::TIPOS.PLU 2>/dev/null
tam() { clic 194 127; sleep 1.2; tecla home; local i; for i in $(seq 1 $1); do tecla down; done; [ "${2:-}" ] && shot $2; tecla ret; sleep 0.8; }
estilo() { clic 424 99; sleep 1.2; tecla home; local i; for i in $(seq 1 $1); do tecla down; done; tecla ret; sleep 0.8; }
tipo() { clic 108 127; sleep 1.2; tecla home; local i; for i in $(seq 1 $1); do tecla down; done; [ "${2:-}" ] && shot $2; tecla ret; sleep 0.8; }
{ arranca
  tipo 1 f0; escribe "El veloz murcielago hindu comia feliz cardillo y kiwi."; tecla ret      # Roble
  tipo 9; escribe "El veloz murcielago hindu comia feliz cardillo y kiwi."; tecla ret         # Llano
  tipo 0; escribe "El veloz murcielago hindu comia feliz cardillo y kiwi."; tecla ret         # Hispa
  tipo 18; escribe "Una fiesta "; tipo 19; escribe "y a mano"; tecla ret
  tecla ctrl-w; tipo 2; escribe "Grande"; tecla ctrl-n; escribe " negra "; tecla ctrl-n ctrl-k ctrl-s; escribe "cursiva"
  aparca; shot f1
  tecla ctrl-n ctrl-k ctrl-s ret; tam 5 f3; escribe "Enorme"; tecla ret; tam 0; escribe "menuda"; tam 1      # 48 puntos, 9 y el normal
  tecla ret; estilo 2; escribe "Titular"                                                      # el estilo «Título 1»
  aparca; shot f4
  tecla ctrl-g; sleep 1.2; escribe "tipos"; tecla ret; sleep 2
  sal; } | run 240 -vga std
chk $T/f0.ppm busca 295 200 310 216 0 9 "Roble negra"                                         # el diálogo con la lista
chk $T/f1.ppm busca 80 183 90 198 0 9 "El veloz murcielago hindu comia feliz cardillo y kiwi."   # la tercera, en Hispa
chk $T/f1.ppm nobusca 80 151 90 182 0 9 "El veloz"                                              # las otras, no
chk $T/f1.ppm busca 72 518 400 540 0 6 "Línea 5 de 5, columna 21"
muestra f0 pluma_lista_tipos; muestra f1 pluma_tipos; muestra f4 pluma_tamanos; muestra f3 pluma_lista_tam
mcopy -n -i $T/f.img ::TIPOS.PLU $T/tipos.plu 2>/dev/null
if python3 - $T/tipos.plu <<'PY'
import sys,struct
d=open(sys.argv[1],'rb').read()
n=struct.unpack_from('<I',d,8)[0]
t,a,p=d[12:12+n],d[12+n:12+2*n],d[12+2*n:]
ok = len(p)==n and p[0]==1 and p[60]==9 and p[120]==0 and p[t.index(b'fiesta')]==18 and p[t.index(b'a mano')]==19
ok = ok and p[t.index(b'Titular')]==0x62 and a[t.index(b'Titular')]==1
ok = ok and p[t.index(b'Enorme')]==0xA2 and p[t.index(b'menuda')]==0x22
ok = ok and p[t.index(b'Grande')]==0x62 and a[t.index(b'Grande')]==0 and a[t.index(b'negra')]==1 and a[t.index(b'cursiva')]==6
sys.exit(0 if ok else 1)
PY
then ok "TIPOS.PLU: el tipo y el tamaño de cada letra, detrás del formato"; else ko "TIPOS.PLU no es lo esperado"; xxd $T/tipos.plu | tail -14; fi

rm -rf $T
exit $fail
