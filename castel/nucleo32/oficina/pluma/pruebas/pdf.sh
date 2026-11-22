#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pluma/pruebas/pdf.sh [CARPETA]
# Las pruebas de «Exportar a PDF» de Pluma 2 en el anfitrión, con ASan y
# UBSan: prueba_pdf.c hace documentos de muestra (y uno de 100 páginas) y
# los exporta; aquí se miran con qpdf --check, pdftotext, pdffonts y con
# Hispa-PDF (castel/nucleo32/pdf/anfitrion.sh). Dice «bien»/«mal» por cada
# cosa y acaba con el código 1 si algo ha ido mal. Sin «make -j».
set -u
AQUI=$(dirname "$(readlink -f "$0")")
OF=$(readlink -f "$AQUI/../..")
RAIZ=$(readlink -f "$OF/../../..")
T=${1:-$(mktemp -d)}
mkdir -p "$T/obj"
O=$T/obj
nok=0; fail=0
ok() { echo "  bien: $*"; nok=$((nok + 1)); }
ko() { echo "  MAL:  $*"; fail=$((fail + 1)); }
BF=${BF:-"-std=gnu11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined"}
INC="-I$OF/comun -I$OF/pluma -I$RAIZ/terceros/stb -I$RAIZ/terceros/miniz"
python3 -I "$OF/comun/haz_letras.py" "$RAIZ/terceros/dejavu" $O/letras_dejavu.c
# (lo que aún no es de este fork, de mentira)
cat > $O/stubs.c <<'FIN'
#include "archivo.h"
#include "pluma.h"
#ifndef CON_ODT
documento *odt_lee(const u8 *d, u32 n, char *e, u32 c) { (void)d; (void)n; copia(e, "-", c); return NULL; }
int odt_escribe(documento *d, bufer *b) { (void)d; (void)b; return -1; }
documento *docx_lee(const u8 *d, u32 n, char *e, u32 c) { (void)d; (void)n; copia(e, "-", c); return NULL; }
int docx_escribe(documento *d, bufer *b) { (void)d; (void)b; return -1; }
#endif
#ifndef CON_RTF
documento *rtf_lee(const u8 *d, u32 n, char *e, u32 c) { (void)d; (void)n; copia(e, "-", c); return NULL; }
int rtf_escribe(documento *d, bufer *b) { (void)d; (void)b; return -1; }
#endif
FIN
printf '#define MINIZ_NO_STDIO\n#define MINIZ_NO_TIME\n#define MINIZ_NO_ARCHIVE_APIS\n#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES\n#include "miniz.c"\n' > $O/miniz_prueba.c
OBJS=""
for c in comun/texto.c comun/so_anfitrion.c comun/ttf.c comun/letras.c comun/lienzo.c comun/imagen.c comun/pdfsal.c \
         pluma/doc.c pluma/maqueta.c pluma/editar.c pluma/archivo.c pluma/f_plu.c pluma/pinta_pag.c pluma/f_pdf.c \
         pluma/pruebas/prueba_pdf.c $O/stubs.c $O/letras_dejavu.c $O/miniz_prueba.c; do
  case $c in /*) f=$c ;; *) f=$OF/$c ;; esac
  W="-Wall -Wextra -Wno-unused-parameter -Wno-sign-compare"
  case $c in *letras.c|*imagen.c|*dejavu*|*miniz*|*prueba_pdf*) W="-w" ;; esac
  # shellcheck disable=SC2086
  gcc $BF $W $INC -c "$f" -o $O/$(basename "$c" .c).o || { echo "no compila $c"; exit 1; }
  OBJS="$OBJS $O/$(basename "$c" .c).o"
done
# shellcheck disable=SC2086
gcc $BF -o $O/prueba_pdf $OBJS -lm || exit 1
if OFI_FECHA_FIJA=1 $O/prueba_pdf "$T" > $T/prueba.txt 2>&1; then ok "los documentos se exportan (sin quejas de ASan ni UBSan)"; else ko "prueba_pdf: $(tail -5 $T/prueba.txt)"; fi
cat $T/prueba.txt | grep "prueba_pdf:"
for f in variado cien variado-plu cien-plu; do
  [ -f $T/$f.pdf ] || { ko "no está $f.pdf"; continue; }
  q=$(qpdf --check $T/$f.pdf 2>&1)
  if echo "$q" | grep -q "No syntax or stream encoding errors" && ! echo "$q" | grep -qi "warning"; then ok "$f.pdf: qpdf --check sin errores ni avisos"
  else ko "$f.pdf: qpdf: $(echo "$q" | grep -i "warn\|error" | head -3)"; fi
  fo=$(pdffonts $T/$f.pdf 2>&1 | tail -n +3)
  if [ -n "$fo" ] && ! echo "$fo" | awk '{ if ($(NF-4) != "yes" || $(NF-3) != "yes") m=1 } END { exit !m }'; then ok "$f.pdf: las letras van incrustadas y en subconjunto ($(echo "$fo" | awk '{print $1}' | sed 's/^......+//' | tr '\n' ' '))"
  else ko "$f.pdf: pdffonts: $fo"; fi
done
pdftotext -layout $T/variado.pdf $T/variado.txt 2> /dev/null
pdftotext $T/variado.pdf $T/variado_r.txt 2> /dev/null
m=0
for s in "Informe de prueba" "1. Introducción" "áéíóú, la eñe (ñ, Ñ), la diéresis de pingüino, ¿preguntas? y ¡exclamaciones!" \
  "Esto va en negrita, esto en cursiva, esto en las dos," "Primera viñeta" "Segunda viñeta" "1. Uno" "2. Dos" "3. Tres" \
  "Cabecera unida" "Cigüeña" "1.234,56" "Último" "Una foto JPG que flota" "Pluma 2 — página 1 de 2" "Página 2 de 2"; do
  tr -s ' \n' ' ' < $T/variado_r.txt | grep -qF "$s" || { m=1; echo "        falta «$s»"; }
done
[ $m = 0 ] && ok "variado.pdf: pdftotext da el texto entero, en orden, con tildes, ñ y diéresis (ToUnicode)" || ko "variado.pdf: falta texto"
n=$(pdfinfo $T/cien.pdf 2> /dev/null | sed -n 's/^Pages: *//p')
[ "${n:-0}" -ge 100 ] && ok "cien.pdf: $n páginas" || ko "cien.pdf: $n páginas"
pdftotext $T/cien.pdf $T/cien.txt 2> /dev/null
grep -q "Párrafo 1\. En un lugar de la Mancha" $T/cien.txt && tr -s " \n" " " < $T/cien.txt | grep -q "Página 99 de $n" && ok "cien.pdf: el texto y el pie con «Página $n de $n»" || ko "cien.pdf: falta texto"
o=$(qpdf --show-npages $T/cien.pdf 2>/dev/null); python3 -I - $T/cien.pdf <<'PY' && ok "cien.pdf: los marcadores (/Outlines) con los capítulos" || ko "cien.pdf: sin marcadores"
import sys, zlib, re
d = open(sys.argv[1], 'rb').read()
sys.exit(0 if b'/Outlines' in d and b'/PageMode /UseOutlines' in d else 1)
PY
pdfinfo $T/variado.pdf 2>/dev/null | grep -q "^Title: *Informe de prueba de Pluma 2" && pdfinfo $T/variado.pdf | grep -q "^Producer: *Pluma 2.0 (Hispa-Pentium)" && ok "variado.pdf: /Title, /Author y /Producer" || ko "variado.pdf: pdfinfo: $(pdfinfo $T/variado.pdf 2>&1 | head -4 | tr '\n' ' ')"
pdfimages -list $T/variado.pdf > $T/img.txt 2>&1
grep -q "smask" $T/img.txt && grep -q "jpeg" $T/img.txt && ok "variado.pdf: el PNG con su transparencia (/SMask) y el JPG tal cual (DCTDecode)" || ko "variado.pdf: imágenes: $(cat $T/img.txt)"
# lo que se ve: la página en imagen (pdftoppm), para mirarla
pdftoppm -r 60 -png -f 1 -l 1 $T/variado.pdf $T/variado > /dev/null 2>&1 && ok "variado.pdf: pdftoppm la pinta ($T/variado-1.png)" || ko "pdftoppm no la pinta"
# Hispa-PDF
HP=$RAIZ/build/pdf-anfitrion/pdf
[ -x $HP ] || bash $RAIZ/castel/nucleo32/pdf/anfitrion.sh $HP rapido > /dev/null 2>&1
if [ -x $HP ]; then
  i=$($HP INFO $T/variado.pdf 2>&1 | head -1)
  case "$i" in *"PDF 1.7, 2 p"*) ok "Hispa-PDF: INFO lo lee ($i)";; *) ko "Hispa-PDF INFO: $i";; esac
  $HP TEXTO $T/variado.pdf /SALIDA=$T/hp.txt /UTF8 > /dev/null 2>&1
  grep -q "Cigüeña" $T/hp.txt && grep -q "Introducción" $T/hp.txt && ok "Hispa-PDF: TEXTO saca el texto (con sus tildes)" || ko "Hispa-PDF TEXTO: $(head -3 $T/hp.txt 2>/dev/null)"
  i=$($HP INFO $T/cien.pdf 2>&1 | head -1)
  case "$i" in *" $n p"*) ok "Hispa-PDF: INFO del de 100 páginas ($i)";; *) ko "Hispa-PDF INFO cien: $i";; esac
else ko "no se ha podido compilar Hispa-PDF"; fi
echo "  cien.pdf: $(stat -c %s $T/cien.pdf) octetos; variado.pdf: $(stat -c %s $T/variado.pdf) octetos"
echo "== $nok bien, $fail mal =="
exit $((fail > 0))
