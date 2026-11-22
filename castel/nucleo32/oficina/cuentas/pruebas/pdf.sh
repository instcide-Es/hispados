#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# oficina/cuentas/pruebas/pdf.sh CUENTAS CARPETA
# Las pruebas del dibujo y de exportar a PDF de Cuentas 2 en el anfitrión
# (CUENTAS: el de anfitrion.sh, con ASan y UBSan; sus objetos, en
# obj-asan al lado; la lista de fuentes, la de CUENTAS_FUENTES o fuentes.lst):
#   1. pruebas/prueba_dibujo.c: celdas con tramas, bordes, combinadas, lo que
#      se sale, el condicional y un comentario, los cinco gráficos (cada serie
#      con su trama y su marcador) y la vista preliminar, en un lienzo; deja
#      las capturas en CARPETA/capturas (.ppm y .png);
#   2. libros hechos con un guion (CUENTAS --guion) y exportados a PDF:
#      qpdf --check, pdftotext (el texto con € y tildes, en su página),
#      pdffonts (las letras dentro), pdfinfo (cuántas páginas: el área de
#      impresión, los saltos puestos a mano, a lo ancho y apaisado).
# Acaba con «== N bien, M mal».
AQUI=$(dirname "$(readlink -f "$0")")
CU=$(dirname "$AQUI")
OFI=$(dirname "$CU")
RAIZ=$(readlink -f "$OFI/../../..")
BIN=$(readlink -f "${1:?uso: pdf.sh CUENTAS CARPETA}")
W=${2:?uso: pdf.sh CUENTAS CARPETA}
O=$(dirname "$BIN")/obj-asan
LISTA=${CUENTAS_FUENTES:-$CU/fuentes.lst}
mkdir -p "$W/capturas"
W=$(readlink -f "$W")
bien=0; mal=0
ok() { bien=$((bien + 1)); }
ko() { mal=$((mal + 1)); echo "mal: $1"; }

# ---- 1. el dibujo en un lienzo ----
OBJS=""
while read -r c w; do
	case "$c" in ""|\#*|cuentas/principal) continue;; esac
	OBJS="$OBJS $O/$(echo $c | tr / _).o"
done < "$LISTA"
BF="-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined"
if gcc -std=gnu11 $BF -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -I$OFI/comun -I$CU -c $AQUI/prueba_dibujo.c -o $W/prueba_dibujo.o 2> $W/dibujo_cc.txt &&
   gcc $BF -o $W/prueba_dibujo $W/prueba_dibujo.o $OBJS $O/comun_so_anfitrion.o $O/letras_dejavu.o -lm 2>> $W/dibujo_cc.txt; then
	$W/prueba_dibujo "$W/capturas" > $W/dibujo.txt 2>&1
	r=$(grep -a "prueba_dibujo ==" $W/dibujo.txt | tail -1)
	b=$(echo "$r" | sed -n 's/.*== \([0-9]*\) bien, \([0-9]*\) mal.*/\1/p')
	m=$(echo "$r" | sed -n 's/.*== \([0-9]*\) bien, \([0-9]*\) mal.*/\2/p')
	if [ -n "$b" ] && ! grep -aq "runtime error\|Sanitizer" $W/dibujo.txt; then
		bien=$((bien + b)); mal=$((mal + m))
		grep -a "^mal:" $W/dibujo.txt
	else ko "prueba_dibujo: $(tail -3 $W/dibujo.txt)"; fi
	grep -a "pintar 40 x 12" $W/dibujo.txt
	for f in $W/capturas/*.ppm; do convert "$f" "${f%.ppm}.png" 2> /dev/null; done
else ko "prueba_dibujo no compila: $(tail -3 $W/dibujo_cc.txt)"; fi

# ---- 2. exportar a PDF ----
# (mira el PDF: qpdf, las letras dentro y cuántas páginas)
mira_pdf() {	# PDF PAGINAS
	if [ ! -f "$1" ]; then ko "no está $(basename $1)"; return 1; fi
	if qpdf --check "$1" 2>&1 | grep -q "No syntax or stream encoding errors"; then ok; else ko "qpdf se queja de $(basename $1)"; fi
	if pdffonts "$1" 2> /dev/null | tail -n +3 | awk '{print $(NF-4)}' | grep -qv yes; then ko "$(basename $1): hay letras sin incrustar"; else ok; fi
	p=$(pdfinfo "$1" 2> /dev/null | sed -n 's/^Pages: *//p')
	if [ "$p" = "$2" ]; then ok; else ko "$(basename $1): $p páginas (se esperaban $2)"; fi
}
tiene() {	# PDF PAGINA TEXTO...
	local f=$1 pg=$2; shift 2
	local t
	t=$(pdftotext -layout -f $pg -l $pg "$f" - 2> /dev/null)
	for x in "$@"; do
		case "$t" in *"$x"*) ok ;; *) ko "$(basename $f), página $pg: falta «$x»" ;; esac
	done
}
no_tiene() {
	local f=$1 pg=$2; shift 2
	local t
	t=$(pdftotext -layout -f $pg -l $pg "$f" - 2> /dev/null)
	for x in "$@"; do
		case "$t" in *"$x"*) ko "$(basename $f), página $pg: sobra «$x»" ;; *) ok ;; esac
	done
}
guion() {	# NOMBRE (el guion, por la entrada)
	cat > $W/$1.txt
	$BIN --guion $W/$1.txt > $W/$1.sal 2>&1
	if grep -aq "runtime error\|Sanitizer" $W/$1.sal; then ko "$1: ASan o UBSan: $(grep -a 'runtime error\|Sanitizer' $W/$1.sal | head -2)"; fi
	grep -aq " 0 mal" $W/$1.sal || ko "$1: el guion: $(grep -a 'mal' $W/$1.sal | head -3)"
}
rm -f $W/*.pdf
# un libro pequeño: el €, las tildes, el condicional con su símbolo, un gráfico
guion p1 <<EOF
pon A1 Ventas del año (€)
estilo A1 negrita tam=14
pon A2 Mes
pon B2 Norte
pon C2 Sur
pon A3 Enero
pon B3 1200,5
pon C3 950
pon A4 Febrero
pon B4 1340
pon C4 1010
pon A5 Marzo
pon B5 1500
pon C5 1225,75
pon A6 Total
pon B6 =SUMA(B3:B5)
pon C6 =SUMA(C3:C5)
pon D3 3,50 €
estilo A2:C2 negrita fondo=DCE6F5 trama=6 bab=2
estilo B3:C6 borde=1
condicional B3:C5 mayor 1300 simbolo=1 negrita fondo=FFE0E0
comenta A6 La suma
grafico columnas A2:C5 titulo=Ventas por zona| en=A9
guarda $W/p1.pdf
EOF
mira_pdf $W/p1.pdf 1
tiene $W/p1.pdf 1 "Ventas del año (€)" "Febrero" "▲ 1340" "4040,5" "3185,75" "3,50 €" "Ventas por zona" "Hoja1" "Página 1 de 1"
# muchas filas: varias páginas, sin partir una fila
{ echo "pon A1 Título"; for i in $(seq 2 200); do echo "pon A$i Fila número $i"; echo "pon B$i $((i * 3))"; done; echo "guarda $W/p2.pdf"; } | guion p2
n2=$(pdfinfo $W/p2.pdf 2> /dev/null | sed -n 's/^Pages: *//p')
if [ -n "$n2" ] && [ "$n2" -ge 4 ] && [ "$n2" -le 6 ]; then ok; else ko "p2.pdf: $n2 páginas (200 filas: de 4 a 6)"; fi
mira_pdf $W/p2.pdf "$n2"
tiene $W/p2.pdf 1 "Título" "Fila número 2 "
tiene $W/p2.pdf "$n2" "Fila número 200" "Página $n2 de $n2"
# el área de impresión y un salto puesto a mano
{ for i in $(seq 1 30); do echo "pon A$i Fila $i."; echo "pon E$i fuera$i"; done; echo "area A1:C10"; echo "salto 6"; echo "guarda $W/p3.pdf"; } | guion p3
mira_pdf $W/p3.pdf 2
tiene $W/p3.pdf 1 "Fila 1." "Fila 5."
no_tiene $W/p3.pdf 1 "Fila 6." "fuera1"
tiene $W/p3.pdf 2 "Fila 6." "Fila 10."
no_tiene $W/p3.pdf 2 "Fila 11." "fuera"
# a lo ancho y apaisada: primero hacia abajo y luego a la derecha
{ echo "horizontal"; for c in A B C D E F G H I J K L M N O P Q R S T U V W X Y Z AA AB AC AD; do for i in 1 2 3; do echo "pon $c$i Col$c-$i"; done; done; echo "guarda $W/p4.pdf"; } | guion p4
n4=$(pdfinfo $W/p4.pdf 2> /dev/null | sed -n 's/^Pages: *//p')
w4=$(pdfinfo $W/p4.pdf 2> /dev/null | sed -n 's/^Page size: *\([0-9.]*\) x \([0-9.]*\).*/\1 \2/p')
case "$w4" in 841.89*" 595.28"*|842*" 595"*) ok ;; *) ko "p4.pdf: no es A4 apaisado ($w4)" ;; esac
if [ -n "$n4" ] && [ "$n4" -ge 2 ] && [ "$n4" -le 4 ]; then ok; else ko "p4.pdf: $n4 páginas (30 columnas apaisado: de 2 a 4)"; fi
mira_pdf $W/p4.pdf "$n4"
tiene $W/p4.pdf 1 "ColA-1" "ColA-3"
tiene $W/p4.pdf "$n4" "ColAD-3"
no_tiene $W/p4.pdf 1 "ColAD-1"
# los cinco gráficos en el PDF (sin errores: qpdf)
guion p5 <<EOF
pon A1 Mes
pon B1 Uno
pon C1 Dos
pon A2 Ene
pon B2 3
pon C2 5
pon A3 Feb
pon B3 4
pon C3 2
pon A4 Mar
pon B4 6
pon C4 7
grafico columnas A1:C4 titulo=Columnas| en=E1
grafico barras A1:C4 titulo=Barras| en=E20
grafico lineas A1:C4 titulo=Líneas| en=E40
grafico sectores A1:B4 titulo=Sectores| en=E60
grafico dispersion B1:C4 titulo=Dispersión| en=E80
guarda $W/p5.pdf
EOF
n5=$(pdfinfo $W/p5.pdf 2> /dev/null | sed -n 's/^Pages: *//p')
mira_pdf $W/p5.pdf "$n5"
t5=$(pdftotext $W/p5.pdf - 2> /dev/null)
for x in Columnas Barras Líneas Sectores Dispersión "%"; do case "$t5" in *"$x"*) ok ;; *) ko "p5.pdf: falta «$x»" ;; esac; done
echo "pdf.sh == $bien bien, $mal mal"
[ $mal = 0 ]
