#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pluma/pruebas/rtf.sh PLUMA [VUELTAS_FUZZ]: el RTF de Pluma 2 (pluma/f_rtf.c)
# con el Pluma del anfitrión (con ASan y UBSan): leer un RTF como los de
# Word y LibreOffice (haz_rtf.py), la ida y vuelta RTF -> PLU -> RTF -> PLU
# (lo segundo, igual que lo primero), que lo escrito sea un RTF bien hecho
# (llaves que casan, solo ASCII) y el fuzz del lector (rtf_fuzz.py). Si hay
# LibreOffice Writer, además: lo que escribe Pluma lo lee LibreOffice con el
# mismo texto, y lo que escribe LibreOffice lo lee Pluma. Dice «N bien, M mal».
set -u
AQUI=$(dirname "$(readlink -f "$0")")
P=$(readlink -f "${1:?uso: rtf.sh PLUMA [VUELTAS]}")
V=${2:-300}
T=$(mktemp -d)
trap 'rm -rf $T' EXIT
nok=0; nko=0
ok() { echo "  bien: $1"; nok=$((nok + 1)); }
ko() { echo "  MAL:  $1"; nko=$((nko + 1)); }
cd $T
python3 -I $AQUI/haz_rtf.py rico.rtf
i=$($P --info rico.rtf 2>&1)
case "$i" in *"20 párrafos, 2 páginas"*"1 tablas, 1 imágenes"*) ok "lee el RTF de prueba: 20 párrafos, 2 páginas, una tabla y una imagen";; *) ko "lee el RTF de prueba: $i";; esac
t=$($P --texto rico.rtf 2>&1)
for s in "Título principal" "ñandú €" "cohete: 🚀" "Otra línea	con tabulación." "Primera viñeta" "Precio	12,50" "A2 vertical	B2 ancha" "	B3	C3" "Segunda página."; do
  case "$t" in *"$s"*) ;; *) ko "falta «$s» en el texto"; s=MAL;; esac
done
[ "$s" != MAL ] && ok "el texto, con sus tildes, \\uN (también un emoji), saltos de línea, tabulaciones y las celdas"
$P --convierte rico.rtf r1.plu > /dev/null 2>&1 && $P --convierte r1.plu r2.rtf > /dev/null 2>&1 && \
$P --convierte r2.rtf r3.plu > /dev/null 2>&1 && $P --convierte r3.plu r4.rtf > /dev/null 2>&1 && \
$P --convierte r4.rtf r5.plu > /dev/null 2>&1
cmp -s r2.rtf r4.rtf && cmp -s r3.plu r5.plu && ok "ida y vuelta RTF -> PLU -> RTF -> PLU: la segunda vuelta, igual que la primera" || ko "la ida y vuelta cambia"
[ "$($P --texto r1.plu 2>&1)" = "$($P --texto r5.plu 2>&1)" ] && ok "el texto no cambia en la ida y vuelta" || ko "el texto cambia en la ida y vuelta"
grep -q '\\clvmgf' r2.rtf && grep -q '\\clvmrg' r2.rtf && grep -q 'cellx6000' r2.rtf && grep -q '\\pngblip' r2.rtf && grep -q 'fldinst PAGE' r2.rtf && grep -q 'fldinst NUMPAGES' r2.rtf && \
  grep -q '\\pnlvlblt' r2.rtf && grep -q '\\pndec' r2.rtf && grep -q '\\tldot\\tqr\\tx8000' r2.rtf && grep -q '\\clcbpat' r2.rtf && grep -q '\\pagebb' r2.rtf && \
  ok "lo escrito lleva la unión a lo alto, la imagen, los campos, las listas, la tabulación, el fondo y el salto" || ko "a lo escrito le falta algo"
python3 -I - r2.rtf <<'PY' && ok "lo escrito es un RTF bien hecho (solo ASCII; las llaves casan)" || ko "lo escrito no es un RTF bien hecho"
import sys
d = open(sys.argv[1], 'rb').read()
h = 0
i = 0
while i < len(d):
    c = d[i:i + 1]
    if c == b'\\':
        i += 2
        continue
    if c == b'{':
        h += 1
    elif c == b'}':
        h -= 1
        if h < 0:
            sys.exit(1)
    i += 1
sys.exit(0 if h == 0 and max(d) < 128 and d.startswith(b'{\\rtf1') else 1)
PY
# con LibreOffice Writer (si está): solo como referencia
L="-env:UserInstallation=file://$T/perfil"
echo hola > p.txt
if command -v soffice > /dev/null && timeout 120 soffice $L --headless --convert-to odt --outdir $T $T/p.txt > /dev/null 2>&1 && [ -f p.odt ]; then
  timeout 120 soffice $L --headless --convert-to txt:Text --outdir $T/lo $T/r2.rtf > /dev/null 2>&1
  if [ -f lo/r2.txt ] && grep -q "Primera viñeta" lo/r2.txt && grep -q "B2 ancha" lo/r2.txt; then ok "LibreOffice abre el RTF de Pluma con su texto"; else ko "LibreOffice no lee el RTF de Pluma"; fi
  timeout 120 soffice $L --headless --convert-to rtf --outdir $T/lo $T/rico.rtf > /dev/null 2>&1
  t2=$($P --texto lo/rico.rtf 2>&1)
  case "$t2" in *"Primera viñeta"*"Segunda página."*) ok "Pluma lee el RTF que escribe LibreOffice";; *) ko "Pluma no lee el RTF de LibreOffice";; esac
else
  echo "  (sin LibreOffice Writer en este equipo: las comparaciones con él se saltan)"
fi
r=$(python3 -I $AQUI/rtf_fuzz.py $P rico.rtf r2.rtf --vueltas $V 2>&1 | tail -3)
case "$r" in *" 0 fallos"*) ok "fuzz del lector RTF con ASan y UBSan: $(echo "$r" | head -1)";; *) ko "fuzz del lector RTF: $r";; esac
echo "== RTF: $nok bien, $nko mal =="
exit $((nko > 0))
