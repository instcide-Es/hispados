#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pintura/pruebas/formatos.sh [CARPETA] - las pruebas de los formatos de
# imagen de Pintura 2 (pintura/formatos.c) en el anfitrión, con ASan y UBSan:
#   0. formatos.c compila como para Hispa-Pentium (las banderas de hacer.sh,
#      sin avisos; solo -c) y el arnés (prueba_formatos.c) en el anfitrión;
#   1. ida y vuelta con las imágenes de prueba del arnés: PNG y BMP idénticos
#      (con alfa), PCX idéntico (lo opaco), GIF idéntico con 256 colores o
#      menos, JPG con PSNR >= 30 dB a calidad 90 (y a 30 pesa menos);
#   2. con ImageMagick (si está): lo que escribimos lo lee él igual, y lo que
#      hace él (y haz_muestras.py: PCX de 16 colores, ICO con máscara...) lo
#      leemos igual que él;
#   3. el fuzz de los lectores (FUZZ=n vueltas por muestra; 300 si no se
#      dice) y cabeceras con medidas enormes.
# Cada cosa, «bien: ...» o «mal: ...» («aviso:» no cuenta); al final,
# «== N bien, M mal ==». SEMILLA=n: otra semilla para el fuzz. Sin «make -j».
set -u
AQUI=$(dirname "$(readlink -f "$0")")
PIN=$(readlink -f "$AQUI/..")
OFI=$(readlink -f "$AQUI/../..")
RAIZ=$(readlink -f "$AQUI/../../../../..")
T=${1:-$(mktemp -d)}
FUZZ=${FUZZ:-300}
mkdir -p "$T/obj" "$T/m"
O=$T/obj
M=$T/m
nok=0; fail=0
ok() { echo "  bien: $*"; nok=$((nok + 1)); }
ko() { echo "  mal: $*"; fail=$((fail + 1)); }
aviso() { echo "  aviso: $*"; }
# lo que dice el arnés (sus «bien:» y «mal:», a la cuenta)
cuenta() {
	local f=$1
	cat "$f"
	nok=$((nok + $(grep -c '^  bien:' "$f")))
	fail=$((fail + $(grep -c '^  mal:' "$f")))
}
fin() {
	echo "== $nok bien, $fail mal =="
	exit $((fail > 0))
}

# ---- 0. compilar ----
BP="-m32 -std=gnu11 -march=i586 -O2 -ffreestanding -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables
    -fno-builtin -fno-tree-loop-distribute-patterns -ffunction-sections -fdata-sections -DOFI_PENTIUM
    -I$OFI/comun/inc -I$OFI/comun -I$PIN -I$RAIZ/terceros/miniz -I$RAIZ/terceros/stb"
# shellcheck disable=SC2086
if gcc $BP -Wall -Wextra -c "$PIN/formatos.c" -o "$O/formatos_pentium.o" 2> "$O/pentium.txt"; then
	if [ -s "$O/pentium.txt" ]; then ko "formatos.c para Hispa-Pentium: con avisos: $(head -3 "$O/pentium.txt")"
	else ok "formatos.c compila para Hispa-Pentium (-m32 -ffreestanding, sin avisos): $(size "$O/formatos_pentium.o" | awk 'NR==2 {print $1 " de código, " $2 " de datos"}')"; fi
	# lo que pide de fuera: solo lo de la biblioteca común y lo que da so_pentium.c
	raro=$(nm -u "$O/formatos_pentium.o" | awk '{print $2}' | grep -vxE 'memcpy|memmove|memset|memcmp|strlen|abs|__udivdi3|__umoddi3|__divdi3|__moddi3|so_[a-z_]+|buf_[a-z0-9]+|vfmt|fmt|extension|iguales|imagen_[a-z_]+|tdefl_[a-z_]+|mz_free')
	[ -z "$raro" ] && ok "para Hispa-Pentium no pide nada de fuera de la biblioteca común" || ko "pide lo que no hay en Hispa-Pentium: $raro"
else
	ko "formatos.c no compila para Hispa-Pentium: $(head -5 "$O/pentium.txt")"
fi
BF="-std=gnu11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined"
INC="-I$OFI/comun -I$PIN -I$RAIZ/terceros/miniz -I$RAIZ/terceros/stb"
printf '#include "miniz_cfg.h"\n#include "miniz.c"\n' > "$O/miniz_prueba.c"
OBJS=""
for c in comun/texto.c comun/so_anfitrion.c comun/imagen.c pintura/formatos.c pintura/pruebas/prueba_formatos.c "$O/miniz_prueba.c"; do
	case $c in /*) f=$c ;; *) f=$OFI/$c ;; esac
	W="-Wall -Wextra -Wno-unused-parameter -Wno-sign-compare"
	case $c in *imagen.c|*miniz*|*so_anfitrion*|*texto.c) W="-w" ;; esac
	o=$O/$(basename "$c" .c).o
	# shellcheck disable=SC2086
	gcc $BF $W $INC -c "$f" -o "$o" 2> "$O/err.txt" || { ko "no compila $c: $(head -5 "$O/err.txt")"; fin; }
	[ -s "$O/err.txt" ] && ko "$c compila con avisos: $(head -3 "$O/err.txt")"
	OBJS="$OBJS $o"
done
# shellcheck disable=SC2086
gcc $BF -o "$O/prueba_formatos" $OBJS -lm 2> "$O/err.txt" || { ko "no se enlaza el arnés: $(head -5 "$O/err.txt")"; fin; }
P=$O/prueba_formatos
export ASAN_OPTIONS=detect_leaks=1:abort_on_error=0 UBSAN_OPTIONS=print_stacktrace=1

# corre el arnés: que acabe bien y sin quejas de ASan ni UBSan
corre() {
	local que=$1; shift
	timeout 120 "$P" "$@" > "$O/sal.txt" 2> "$O/err.txt"
	local r=$?
	cuenta "$O/sal.txt"
	if [ $r -ne 0 ] || grep -qE 'ERROR: AddressSanitizer|runtime error|LeakSanitizer' "$O/err.txt"; then
		[ $r = 124 ] && ko "$que: se cuelga (más de 120 s)" || ko "$que: acaba con $r: $(grep -E 'ERROR|runtime error|SUMMARY' "$O/err.txt" | head -3)"
		return 1
	fi
	return 0
}

# ---- 1. ida y vuelta ----
corre "ida y vuelta" ida "$M" && ok "ida y vuelta: sin quejas de ASan ni UBSan, sin fugas"

# ---- 2. con ImageMagick ----
python3 -I "$AQUI/haz_muestras.py" "$M" || ko "haz_muestras.py"
# compara ARCHIVO [CUADRO] [PSNR]: lo nuestro frente a lo que lee ImageMagick
compara() {
	local f=$1 k=${2:-} psnr=${3:-} q=$1 wh
	[ -n "$k" ] && q="$f[$k]"
	wh=$(identify -format '%w %h\n' "$q" 2> /dev/null | head -1)
	convert "$q" -depth 8 "rgba:$f.rgba" 2> /dev/null || { ko "ImageMagick no lee $(basename "$f")"; return; }
	# shellcheck disable=SC2086
	timeout 60 "$P" compara "$f" "$f.rgba" $wh $psnr > "$O/sal.txt" 2> "$O/err.txt"
	cuenta "$O/sal.txt"
	grep -qE 'ERROR: AddressSanitizer|runtime error' "$O/err.txt" && ko "compara $(basename "$f"): $(grep -E 'ERROR|runtime error' "$O/err.txt" | head -2)"
}
# el cuadro más grande (el que leemos de los ICO)
grande() {
	identify -format '%p %w\n' "$1" 2> /dev/null | sort -k2 -n -s | tail -1 | awk '{print $1}'
}
if command -v convert > /dev/null && command -v identify > /dev/null; then
	# lo que escribimos
	for f in "$M"/w_*.png "$M"/w_*.bmp "$M"/w_*.gif "$M"/w_*.pcx; do compara "$f"; done
	for f in "$M"/w_*.jpg; do compara "$f" "" 35; done
	# lo que hace ImageMagick
	S=$M/w_impar.png; A=$M/w_degradado.png
	convert "$S" -monochrome "$M/im_1bit.pcx"
	convert "$S" -colors 200 "$M/im_8bits.pcx"
	convert "$S" -type TrueColor "$M/im_24bits.pcx"
	convert "$A" "$M/im_32bits.pcx"
	convert "$S" -colors 200 -type Palette "BMP3:$M/im_8bits.bmp"
	convert "$S" -colors 200 -type Palette -compress None "BMP3:$M/im_8crudo.bmp"
	convert "$S" -colors 16 -type Palette -compress RLE "BMP3:$M/im_4rle.bmp"
	convert "$S" -colors 2 -type Palette "BMP3:$M/im_1bit.bmp"
	convert "$S" -colors 16 -type Palette "BMP3:$M/im_4bits.bmp"
	convert "$S" -type TrueColor "BMP3:$M/im_24bits.bmp"
	convert "$A" -type TrueColorAlpha "BMP:$M/im_32bits.bmp"
	convert "$S" -colors 128 "$M/im.gif"
	convert "$A" "$M/im_transp.gif"
	convert "$S" -compress None "$M/im.tga"
	convert "$S" -compress RLE "$M/im_rle.tga"
	convert "$A" -compress RLE "$M/im_alfa_rle.tga"
	convert "$A" -compress None "$M/im_alfa.tga"
	for s in 16 32 48; do convert "$A" -resize "${s}x${s}!" "$M/im_$s.ico"; done
	convert "$A" -define icon:auto-resize=16,48,32 "$M/im_varias.ico"
	convert "$A" -resize '256x256!' "$M/im_256png.ico"
	# (los de 1 bit: ImageMagick no mira su paleta y lee 1 = negro; nosotros, como GIMP, la de la cabecera)
	compara "$M/im_1bit.pcx" "" invertido
	compara "$M/p_1bit.pcx" "" invertido
	for f in "$M"/im_[28]*.pcx "$M"/im_32*.pcx "$M"/p_[248]*.pcx "$M"/im_*.bmp "$M"/b_*.bmp "$M"/im*.gif "$M"/im*.tga; do compara "$f"; done
	for f in "$M"/im_*.ico "$M"/i_*.ico "$M"/i_*.cur; do compara "$f" "$(grande "$f")"; done
	# (¿lleva un PNG el de 256? ¿y BMP los demás?)
	python3 -I - "$M/im_256png.ico" "$M/im_32.ico" <<'PY' && ok "im_256png.ico lleva un PNG dentro e im_32.ico un BMP" || ko "los ICO de ImageMagick no llevan lo que se esperaba"
import sys, struct
def dentro(r):
    d = open(r, 'rb').read()
    off = struct.unpack_from('<I', d, 18)[0]
    return d[off:off + 4]
sys.exit(0 if dentro(sys.argv[1]) == b'\x89PNG' and dentro(sys.argv[2])[:4] == b'\x28\0\0\0' else 1)
PY
else
	aviso "sin ImageMagick (convert, identify): no se compara con él"
fi

# ---- 3. el fuzz y las medidas enormes ----
for g in "png:w_degradado.png w_pocos_alfa.png" "jpg:w_impar.jpg w_uno.jpg" "bmp:w_degradado.bmp w_impar.bmp im_8bits.bmp im_4bits.bmp b_rle8.bmp b_rle4.bmp" \
	 "gif:w_pocos_alfa.gif w_impar.gif im_transp.gif" "pcx:w_impar.pcx w_pocos.pcx im_1bit.pcx p_4bits.pcx p_4planos.pcx p_8crudo.pcx p_24sobra.pcx im_32bits.pcx" \
	 "ico:im_varias.ico im_256png.ico i_bmp4.ico i_bmp1.ico i_cur8.cur" "tga:im.tga im_rle.tga im_alfa_rle.tga"; do
	fmt=${g%%:*}
	lista=""
	for f in ${g#*:}; do [ -f "$M/$f" ] && lista="$lista $M/$f"; done
	[ -z "$lista" ] && { aviso "fuzz de $fmt: sin muestras"; continue; }
	# shellcheck disable=SC2086
	corre "fuzz de $fmt" fuzz "$FUZZ" "${SEMILLA:-2026}" $lista && ok "fuzz de $fmt ($FUZZ vueltas por muestra): sin quejas de ASan ni UBSan ni cuelgues"
done
OFI_MEMORIA=64 corre "medidas enormes" enormes && ok "medidas enormes: sin quejas, con un tope de 64 MB de memoria"
fin
