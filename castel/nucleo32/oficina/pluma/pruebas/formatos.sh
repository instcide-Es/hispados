#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pluma/pruebas/formatos.sh [CARPETA] - las pruebas de los formatos de
# Pluma 2 (ODT y DOCX, y .PLU) en el anfitrión, con ASan y UBSan:
#   1. compila VUELCA (los fuentes de fuentes.lst con pruebas/vuelca.c en vez
#      de principal.c): abre un documento y saca lo que lleva;
#   2. hace los documentos de prueba (haz_docs.py: .ODT y .DOCX como los de
#      LibreOffice y Word) y mira que se lee todo lo esencial;
#   3. ida y vuelta: cada uno a ODT, DOCX y PLU, y otra vez a los tres: lo
#      que lleva tiene que ser lo mismo (VUELCA da lo mismo);
#   4. lo escrito por Pluma es un ZIP y un XML bien hechos (python3, zipfile
#      y xml.dom.minidom), con el «mimetype» del ODT el primero y sin comprimir;
#   5. un .PLU de Hispa-386 (programas/pluma.asm) -> ODT -> DOCX -> PLU: el
#      mismo texto.
# Al final, «N bien, M mal». Sin «make -j». FUZZ=n: además, n vueltas del
# fuzz (fuzz.py).
set -u
AQUI=$(dirname "$(readlink -f "$0")")
PLUMA=$(readlink -f "$AQUI/..")
OFI=$(readlink -f "$AQUI/../..")
RAIZ=$(readlink -f "$AQUI/../../../../..")
T=${1:-$(mktemp -d)}
mkdir -p "$T/obj"
nok=0; fail=0
ok() { echo "  bien: $1"; nok=$((nok + 1)); }
ko() { echo "  MAL:  $1"; fail=$((fail + 1)); }
BF="-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined"
INC="-I$OFI/comun -I$PLUMA -I$RAIZ/terceros/miniz -I$RAIZ/terceros/stb"
OBJS=""
python3 -I "$OFI/comun/haz_letras.py" "$RAIZ/terceros/dejavu" "$T/obj/letras_dejavu.c"
mal=0
while read -r c w; do
	case "$c" in ""|\#*) continue;; esac
	[ "$c" = pluma/principal ] && c=pluma/pruebas/vuelca
	W="-Wall -Wno-unused-parameter -Wno-sign-compare"
	[ "${w:-}" = "-w" ] && W="-w"
	o=$T/obj/$(echo $c | tr / _).o
	# shellcheck disable=SC2086
	gcc -std=gnu11 $BF $W $INC -c "$OFI/$c.c" -o "$o" 2> "$T/obj/err.txt" || { cat "$T/obj/err.txt" | head -5; mal=1; }
	OBJS="$OBJS $o"
done < "$PLUMA/fuentes.lst"
# shellcheck disable=SC2086
gcc -std=gnu11 $BF $INC -c "$OFI/comun/so_anfitrion.c" -o "$T/obj/so.o" && gcc -std=gnu11 $BF -w -I"$OFI/comun" -c "$T/obj/letras_dejavu.c" -o "$T/obj/ld.o" &&
	gcc $BF -o "$T/vuelca" $OBJS "$T/obj/so.o" "$T/obj/ld.o" -lm 2> "$T/obj/err.txt" || { head -5 "$T/obj/err.txt"; mal=1; }
[ $mal = 0 ] && ok "VUELCA compila (ASan y UBSan)" || { ko "no compila"; echo "== $nok bien, $fail mal =="; exit 1; }
V=$T/vuelca
export ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
python3 -I "$AQUI/haz_docs.py" "$T" > /dev/null
mira() {	# mira QUE ARCHIVO patrón...
	local q=$1 f=$2 m=0 p
	shift 2
	for p in "$@"; do grep -q -- "$p" "$f" || { echo "        falta: $p"; m=1; }; done
	[ $m = 0 ] && ok "$q" || ko "$q"
}
for x in odt docx; do
	$V "$T/prueba.$x" > "$T/v_$x.txt" 2> "$T/e_$x.txt" || { ko "VUELCA prueba.$x: $(head -3 $T/e_$x.txt)"; continue; }
	mira "prueba.$x: el texto, los títulos, el formato de letra (negrita, cursiva, subrayado, color, resaltado, índice, tachado, versalitas)" "$T/v_$x.txt" \
		"Título 1.*tit.*Título del documento" "N c=ff000000 r=ff000000}con negrita" " K S c=ff000000 r=ff000000}cursiva subrayada" \
		"32 c=c00000 r=ffff00}rojo resaltado grande" " T sup c=ff000000 r=ff000000}2" "DejaVu Sans Mono 24 vers" "ñandú"
	mira "prueba.$x: el párrafo (justificado, primera línea, interlineado), la cita, el estilo propio, la tabulación con puntos" "$T/v_$x.txt" \
		"al=3 si=0 sd=0 sp=28 .* int=0/150" "\[Cita\] .* si=28 sd=28" "\[Mi estilo\]" "tab=227/2/\."
	mira "prueba.$x: las listas (viñetas con un subnivel y números)" "$T/v_$x.txt" "lista=1 niv=0 .*Viñeta uno" "lista=1 niv=1 .*Subviñeta" "lista=2 niv=0 .*Primero" "lista=2 niv=0 .*Tercero"
	mira "prueba.$x: la tabla (2x3, una celda unida a lo ancho y otra a lo alto, el fondo, dos líneas en una celda, bordes, centrada)" "$T/v_$x.txt" \
		"tabla 0: 3x2 bordes=1 alin=1 anchos 113 227" "T(0,0) ac=2,1 fondo=dde8f4: .*Celda unida" "T(0,1) cub" "T(1,0) af=1,2: .*Abajo" "T(2,0) cub" "b1<BR>.*b1 segunda línea"
	mira "prueba.$x: la imagen (PNG de 32x24, flotando a la izquierda, 85x64 pt), la nota al pie, el salto de página, el salto de línea" "$T/v_$x.txt" \
		"<IMG tipo=1 ajuste=1 85x64pt 32x24px" "<NOTA .*Esto es la nota al pie.>" "SALTO.*Segunda página" "<BR>.*Tras un salto de línea"
	mira "prueba.$x: la página, el encabezado y el pie con sus campos, el título y el autor" "$T/v_$x.txt" \
		"página 595x842" "^ENC .*Cabecera de prueba" "^PIE .*Página .*<PAG>.* de .*<PAGS>" "título «Documento de prueba» autor «Pluma»"
done
# 3. ida y vuelta
for x in odt docx; do
	for a in odt docx plu; do
		$V --convierte "$T/prueba.$x" "$T/i_$x.$a" > "$T/c.txt" 2>&1 || { ko "prueba.$x -> $a: $(head -3 $T/c.txt)"; continue; }
		$V "$T/i_$x.$a" > "$T/vi_$x.$a.txt" 2>&1
		for b in odt docx; do
			$V --convierte "$T/i_$x.$a" "$T/ii_$x.$a.$b" > "$T/c.txt" 2>&1 || { ko "$x -> $a -> $b: $(head -3 $T/c.txt)"; continue; }
			$V "$T/ii_$x.$a.$b" > "$T/vii.txt" 2>&1
			if diff -q "$T/vi_$x.$a.txt" "$T/vii.txt" > /dev/null; then ok "$x -> $a -> $b: lo mismo que $x -> $a"
			else ko "$x -> $a -> $b no es lo mismo que $x -> $a:"; diff "$T/vi_$x.$a.txt" "$T/vii.txt" | head -6 | sed 's/^/        /'; fi
		done
	done
	# y lo de Pluma 2 (.PLU) es lo mismo que lo leído del original
	if diff -q "$T/v_$x.txt" "$T/vi_$x.plu.txt" > /dev/null; then ok "$x -> PLU: lo mismo que el original (el .PLU no pierde nada)"
	else ko "$x -> PLU pierde algo:"; diff "$T/v_$x.txt" "$T/vi_$x.plu.txt" | head -6 | sed 's/^/        /'; fi
done
# 4. el ZIP y el XML de lo que escribe Pluma
python3 -I - "$T" <<'PY' > "$T/zip.txt" 2>&1 && ok "lo escrito: ZIP y XML bien hechos (mimetype del ODT el primero y sin comprimir; [Content_Types], rels y partes del DOCX)" || { ko "lo escrito no está bien:"; head -5 "$T/zip.txt"; }
import sys, zipfile, xml.dom.minidom, glob
t = sys.argv[1]
for f in glob.glob(t + '/i_*.odt') + glob.glob(t + '/ii_*.odt') + glob.glob(t + '/i_*.docx') + glob.glob(t + '/ii_*.docx'):
    z = zipfile.ZipFile(f)
    assert z.testzip() is None, f
    n = z.namelist()
    if f.endswith('.odt'):
        i = z.infolist()[0]
        assert i.filename == 'mimetype' and i.compress_type == 0 and z.read('mimetype') == b'application/vnd.oasis.opendocument.text', f
        for p in ('content.xml', 'styles.xml', 'meta.xml', 'META-INF/manifest.xml'):
            assert p in n, (f, p)
    else:
        for p in ('[Content_Types].xml', '_rels/.rels', 'word/document.xml', 'word/styles.xml', 'word/_rels/document.xml.rels'):
            assert p in n, (f, p)
    for p in n:
        if p.endswith('.xml') or p.endswith('.rels'):
            xml.dom.minidom.parseString(z.read(p))
print('bien')
PY
# 5. un .PLU de Hispa-386
printf 'PLUMA1\032\000' > "$T/v386.plu"
python3 -I - "$T/v386.plu" <<'PY'
import sys, struct
t = 'Carta de prueba\nUn texto con ñ y acentos áéíóú.\nA la derecha\n'.encode('cp437')
atr = bytes([0] * len(t))
atr = bytearray(atr); atr[15] = 1; atr[len(t) - 1] = 2          # (el primer párrafo, centrado; el último, a la derecha)
tip = bytes([9 | 3 << 5] * len(t))                                 # Llano, 24 puntos
open(sys.argv[1], 'ab').write(struct.pack('<I', len(t)) + t + bytes(atr) + tip + b'PL22')
PY
$V --convierte "$T/v386.plu" "$T/v386.odt" > /dev/null 2>&1 && $V --convierte "$T/v386.odt" "$T/v386.docx" > /dev/null 2>&1 &&
	$V --convierte "$T/v386.docx" "$T/v386b.plu" > /dev/null 2>&1 && $V "$T/v386.plu" > "$T/a.txt" 2>&1 && $V "$T/v386b.plu" > "$T/b.txt" 2>&1
if diff -q "$T/a.txt" "$T/b.txt" > /dev/null && grep -q "al=1 .*Carta de prueba" "$T/a.txt" && grep -q "Llano 48.*ñ y acentos áéíóú" "$T/a.txt"; then
	ok "un .PLU de Hispa-386 -> ODT -> DOCX -> PLU: lo mismo (texto, alineación, tipo y tamaño)"
else ko "el .PLU de Hispa-386 no vuelve igual"; diff "$T/a.txt" "$T/b.txt" | head -5; fi
# 6. el fuzz (con FUZZ=n)
if [ -n "${FUZZ:-}" ]; then
	python3 -I "$AQUI/fuzz.py" "$V" "$T" "$FUZZ" > "$T/fuzz.txt" 2>&1 && ok "fuzz de ODT y DOCX: $(tail -1 $T/fuzz.txt)" || { ko "el fuzz: $(tail -3 $T/fuzz.txt)"; }
fi
echo "== $nok bien, $fail mal =="
exit $((fail > 0))
