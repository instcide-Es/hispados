# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pruebas_pluma.sh - la parte 41 (SOLO=pluma, PLUMA32) de pruebas.sh, que lee
# este archivo («.»): la función parte_pluma, Pluma 2 (PLUMA.BIN, el
# procesador de textos de 32 bits en C portable: castel/nucleo32/oficina),
# con los ayudantes de pruebas.sh (ok, ko, saca) y sus carpetas ($AQUI, $P, $G).
#   1. en el anfitrión, con ASan y UBSan: las pruebas de cada parte
#      (oficina/pluma/pruebas: formatos ODT y DOCX con su fuzz, el PDF —qpdf,
#      pdftotext, pdffonts y Hispa-PDF—, el RTF con su fuzz y el corrector
#      contra el de Hispa-386), y la ventana con un guion de sucesos (la de
#      mentira de comun/so_anfitrion.c): escribir, estilo, negrita, una tabla,
#      una imagen, guardar, exportar a PDF y deshacer; y lo guardado, de ida
#      y vuelta por ODT, DOCX, RTF y PLU sin perder el texto;
#   2. en QEMU (el escritorio a 800 x 600, la Consola en (60, 186)): PLUMA,
#      escribir con formato, una tabla, una imagen, guardar (PRUEBA.PLU),
#      exportar a PDF, cerrar, volver a abrir y las propuestas del corrector
#      con el botón derecho; lo que deja en el disco, mirado desde fuera;
#   3. lo que tarda en un Atom (QEMU -cpu n270 -icount shift=1: unos 500
#      millones de instrucciones por segundo, menos que un N455 de verdad):
#      escribir en un documento de 100 páginas (PLUMA --mide 100 40).
# Con PLUMA_CAPTURAS=carpeta, las capturas de QEMU (en .png) van también allí
# (las de docs/pentium/pluma/).

parte_pluma() {
echo "== Pluma 2 (PLUMA.BIN: el procesador de textos de 32 bits, en C portable) =="
local V=$P/pluma PL=$AQUI/nucleo32/oficina/pluma S r t
local HPL=$V/pluma-anfitrion DIC=$AQUI/../programas/comun/DICCIO.DAT
mkdir -p $V/a $V/q $V/cap $G
export MTOOLS_SKIP_CHECK=1
if ! bash $PL/anfitrion.sh $HPL asan > $V/anf.txt 2>&1 || ! bash $PL/hacer.sh $V/PLUMA.BIN > $V/hacer.txt 2>&1; then
  ko "no se puede compilar Pluma 2: $(tail -n 3 $V/anf.txt $V/hacer.txt)"
  return
fi
ok "PLUMA.BIN se compila (-march=i586, sin CMOV: $(stat -c %s $V/PLUMA.BIN) octetos) y Pluma 2 en el anfitrión, con ASan y UBSan"
# ---- 1. en el anfitrión ---- (PLUMA_SOLO_QEMU=1: sin esta, ni la 3)
if [ -z "${PLUMA_SOLO_QEMU:-}" ]; then
pl_partes() {     # el nombre, la orden; cuenta sus «bien»/«mal»
  local que=$1 sal=$V/$2.txt; shift 2
  "$@" > $sal 2>&1
  r=$(grep -a "bien, .* mal" $sal | tail -1)
  case "$r" in *" 0 mal"*) ok "$que: ${r#*== }" ;; *) ko "$que: ${r:-sin resumen}"; grep -a -i "mal:\|FAIL\|runtime error\|Sanitizer" $sal | head -5 | sed 's/^/        /' ;; esac
}
pl_partes "anfitrión: ODT y DOCX (leer, escribir, ida y vuelta, un .PLU de 386, fuzz)" formatos env FUZZ=${FUZZ_PLUMA:-200} bash $PL/pruebas/formatos.sh $V/tf
pl_partes "anfitrión: exportar a PDF (qpdf, pdftotext, pdffonts, Hispa-PDF)" pdf bash $PL/pruebas/pdf.sh $V/tp
pl_partes "anfitrión: RTF (ida y vuelta y fuzz)" rtf bash $PL/pruebas/rtf.sh $HPL ${FUZZ_PLUMA:-200}
pl_partes "anfitrión: el corrector en C contra el de Hispa-386 (y el fuzz de DICCIO.DAT)" orto bash $PL/pruebas/orto.sh 4000 60
# la ventana, con un guion (las posiciones, las de una ventana de 1024 x 700)
convert -size 240x160 gradient:'#3B74D6-#F2C14E' $V/q/foto.png 2> /dev/null || python3 -I - $V/q/foto.png <<'PY'
import sys, zlib, struct
w, h = 240, 160
raw = b''.join(b'\0' + bytes(c for x in range(w) for c in (x * 255 // w, y * 255 // h, 128)) for y in range(h))
def ch(t, d):
    return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
open(sys.argv[1], 'wb').write(b'\x89PNG\r\n\x1a\n' + ch(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) + ch(b'IDAT', zlib.compress(raw)) + ch(b'IEND', b''))
PY
rm -f $V/q/prueba.plu $V/q/prueba.pdf
cat > $V/q/guion.txt <<EOF
clic 130 79
elige 2
escribe Informe de prueba
tecla ret
escribe Este documento lo ha escrito Pluma 2 con su guion de pruebas. Lleva
tecla ctrl-n
escribe negrita
tecla ctrl-n
escribe ,
tecla ctrl-k
escribe cursiva
tecla ctrl-k
escribe  y una tabla:
tecla ret
clic 441 13
elige 0
tecla ret
escribe Nombre
tecla tab
escribe Cantidad
tecla tab
escribe Precio
tecla tab
escribe Manzanas
tecla tab
escribe 12
tecla tab
escribe 3,50 €
tecla ctrl-fin
escribe Y una imagen:
clic 205 13
elige 14
tecla tab
escribe $V/q/foto.png
tecla ret
captura h1
tecla f12
escribe $V/q/prueba.plu
tecla ret
clic 14 13
elige 7
escribe $V/q/prueba.pdf
tecla ret
escribe  (y esto se deshace)
tecla ctrl-z
captura h2
clic 248 13
elige 0
captura h3
tecla esc
cerrar
tecla tab ret
EOF
OFI_GUION=$V/q/guion.txt OFI_CAPTURAS=$V/cap OFI_VENTANA=1024x700 OFI_83=0 PLUMA_DICCIO=$DIC timeout 300 $HPL > $V/ventana.txt 2>&1
S=$V/ventana.txt
if ! grep -q "runtime error\|Sanitizer" $S && grep -q "pluma: guardado.*prueba.plu" $S && grep -q "pluma: pdf.*prueba.pdf" $S && grep -q "cuadro «Fuentes»" $S && grep -q "pluma: adiós" $S; then
  ok "anfitrión: la ventana con un guion: estilo, negrita, cursiva, tabla, imagen, guardar, exportar a PDF, deshacer, el cuadro Fuentes y cerrar (sin quejas de ASan)"
else ko "anfitrión: la ventana con un guion:"; grep -a "runtime error\|Sanitizer\|aviso" $S | head -5 | sed 's/^/        /'; fi
t=$($HPL --info $V/q/prueba.plu 2>&1)
case "$t" in *"1 tablas, 1 imágenes"*) ok "lo guardado (prueba.plu) lleva la tabla y la imagen: ${t#*: }" ;; *) ko "prueba.plu: «$t»" ;; esac
if command -v pdftotext > /dev/null && [ -f $V/q/prueba.pdf ]; then
  t=$(pdftotext $V/q/prueba.pdf - 2> /dev/null | tr '\n' ' ')
  q=$(qpdf --check $V/q/prueba.pdf 2>&1 | grep -c "No syntax or stream encoding errors")
  case "$t" in *"Informe de prueba"*negrita*cursiva*Manzanas*"3,50 €"*) [ "$q" = 1 ] && ok "el PDF exportado: qpdf no ve errores y el texto sale entero (con el €)" || ko "el PDF: qpdf se queja" ;;
    *) ko "el PDF exportado da «$t»" ;; esac
else ko "no está prueba.pdf (o falta pdftotext)"; fi
# ida y vuelta de lo guardado por los formatos
$HPL --texto $V/q/prueba.plu > $V/a/t0.txt 2>&1
bien=1; por=
for f in odt docx rtf; do
  $HPL --convierte $V/q/prueba.plu $V/a/p.$f > /dev/null 2>&1 && $HPL --convierte $V/a/p.$f $V/a/p_$f.plu > /dev/null 2>&1 && \
    $HPL --texto $V/a/p_$f.plu > $V/a/t_$f.txt 2>&1 && cmp -s $V/a/t0.txt $V/a/t_$f.txt || { bien=0; por="$por $f"; }
  t=$($HPL --info $V/a/p_$f.plu 2>&1); case "$t" in *"1 tablas, 1 imágenes"*) ;; *) bien=0; por="$por $f($t)" ;; esac
done
[ $bien = 1 ] && ok "lo guardado, de ida y vuelta por ODT, DOCX y RTF: el mismo texto, con su tabla y su imagen" || ko "ida y vuelta:$por"
fi
# ---- 2. en QEMU ----
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $V/semilla.asm > /dev/null && nasm -f bin $V/semilla.asm -o $V/SEMILLA.BIN || { ko "semilla.cas no compila"; return; }
bash $AQUI/nucleo32/disco.sh $V/v.img 32 > $V/disco.txt 2>&1 || { ko "disco.sh: $(tail -n 2 $V/disco.txt)"; return; }
mcopy -i $V/v.img $V/PLUMA.BIN $DIC :: && mcopy -i $V/v.img $V/q/foto.png ::FOTO.PNG
CAPTURAS=$G ESCRIBE=1 DISCO=$V/v.img ARRANQUE=escritorio MODULOS= MEMORIA=128 $AQUI/nucleo32/probar.sh $V/SEMILLA.BIN ":pausa 3" ":doble 60 186" ":pausa 4" \
  ":escribe PLUMA" ":tecla ret" ":hasta 180 1 pluma: lista" ":pausa 2" \
  ":escribe Informe de QEMU" ":tecla home" ":tecla shift-end" ":tecla ctrl-n" ":tecla ctrl-t" ":tecla ctrl-o" ":tecla ctrl-o" ":tecla end" ":tecla ret" \
  ":tecla ctrl-q" ":tecla ctrl-w" ":tecla ctrl-w" ":tecla ctrl-n" \
  ":escribe Escrito en Hispa-Pentium con Pluma 2: una tabla, una imagen y una falta de ortografia." ":tecla ret" ":pausa 2" \
  ":clic 438 72" ":pausa 3" ":tecla ret" ":pausa 2" ":escribe Uno" ":tecla tab" ":escribe Dos" ":tecla tab" ":escribe Tres" ":tecla tab" \
  ":escribe 1" ":tecla tab" ":escribe 2" ":tecla tab" ":escribe 3" ":tecla ctrl-end" ":pausa 1" \
  ":clic 469 72" ":pausa 3" ":tecla tab" ":escribe C:\\FOTO.PNG" ":tecla ret" ":pausa 4" ":captura pl1" \
  ":tecla ctrl-g" ":pausa 3" ":escribe PRUEBA.PLU" ":tecla ret" ":hasta 60 1 pluma: guardado" \
  ":clic 531 72" ":pausa 3" ":tecla ret" ":hasta 180 1 pluma: pdf" ":pausa 2" ":captura pl2" \
  ":tecla ctrl-home" ":tecla f7" ":pausa 4" ":captura pl3" ":tecla esc" ":pausa 1" \
  ":clic 787 13" ":hasta 30 1 pluma: adiós" ":pausa 2" ":escribe PLUMA PRUEBA.PLU" ":tecla ret" ":hasta 180 1 pluma: abierto" ":pausa 3" ":captura pl4" \
  ":tecla alt-f" ":pausa 2" ":tecla ret" ":pausa 3" ":captura pl5" ":tecla esc" ":pausa 1" > $V/qemu.sal 2>&1
S=$V/qemu.sal
[ -n "${PLUMA_CAPTURAS:-}" ] && mkdir -p "$PLUMA_CAPTURAS" && cp $S "$PLUMA_CAPTURAS/qemu.sal"
mira_pl() { if grep -a -q -- "$2" $S; then ok "$1"; else ko "$1: falta «$2»"; grep -a "pluma" $S | tail -6 | sed 's/^/        /'; fi; }
mira_pl "en QEMU: Pluma 2 abre su ventana en el escritorio (con el diccionario)" "pluma: lista"
mira_pl "en QEMU: la tabla (el cuadro Insertar tabla)" "cuadro «Insertar tabla»"
mira_pl "en QEMU: la imagen (C:\\FOTO.PNG)" "Imagen insertada"
mira_pl "en QEMU: guardado como PRUEBA.PLU" "pluma: guardado .*PRUEBA.PLU"
mira_pl "en QEMU: exportado a PDF" "pluma: pdf .*PRUEBA.PDF"
mira_pl "en QEMU: cerrar y volver a abrir PRUEBA.PLU" "pluma: abierto PRUEBA.PLU"
mira_pl "en QEMU: la ortografía (F7: la primera falta, con sus propuestas)" "cuadro «Ortografía: español»"
mira_pl "en QEMU: el cuadro Fuentes (Alt+F y la primera)" "cuadro «Fuentes»"
if ! grep -a -q "excepción\|\*\*\*" $S; then ok "en QEMU: ni una excepción ni una queja del núcleo"
else ko "el núcleo se ha quejado:"; grep -a "excepción\|\*\*\*" $S | head -4 | sed 's/^/        /'; fi
if saca $V/v.img PRUEBA.PLU $V/a/qemu.plu; then
  t=$($HPL --info $V/a/qemu.plu 2>&1)
  x=$($HPL --texto $V/a/qemu.plu 2>&1 | tr '\n' ' ')
  case "$t$x" in *"1 tablas, 1 imágenes"*"Informe de QEMU"*Uno*Dos*Tres*ortografia*) ok "PRUEBA.PLU (hecho en QEMU), mirado desde fuera: su texto, la tabla y la imagen" ;;
    *) ko "PRUEBA.PLU: «$t» «$x»" ;; esac
else ko "no está PRUEBA.PLU en el disco de QEMU"; fi
if saca $V/v.img PRUEBA.PDF $V/a/qemu.pdf && command -v pdftotext > /dev/null; then
  t=$(pdftotext $V/a/qemu.pdf - 2> /dev/null | tr '\n' ' ')
  case "$t" in *"Informe de QEMU"*Tres*) ok "PRUEBA.PDF (hecho en QEMU): pdftotext saca su texto" ;; *) ko "PRUEBA.PDF da «$t»" ;; esac
else ko "no está PRUEBA.PDF en el disco de QEMU"; fi
if [ -n "${PLUMA_CAPTURAS:-}" ]; then
  mkdir -p "$PLUMA_CAPTURAS"
  for c in pl1 pl2 pl3 pl4 pl5; do [ -f $G/$c.ppm ] && convert $G/$c.ppm "$PLUMA_CAPTURAS/$c.png" 2> /dev/null; done
  for c in h1 h2 h3; do [ -f $V/cap/$c.ppm ] && convert $V/cap/$c.ppm "$PLUMA_CAPTURAS/$c.png" 2> /dev/null; done
fi
# ---- 3. lo que tarda en un Atom ----
[ -n "${PLUMA_SOLO_QEMU:-}" ] && return
QEMU_MAS="-cpu n270 -icount shift=1" ESCRIBE=1 DISCO=$V/v.img ARRANQUE=escritorio MODULOS= MEMORIA=256 $AQUI/nucleo32/probar.sh $V/SEMILLA.BIN ":pausa 3" ":doble 60 186" ":pausa 4" \
  ":escribe PLUMA --mide 100 40" ":tecla ret" ":hasta 900 1 Av P" ":pausa 3" > $V/mide.sal 2>&1
t=$(grep -a "ms por letra" $V/mide.sal | head -1)
ms=$(echo "$t" | sed -n 's/.*: \([0-9]*\) ms por letra.*/\1/p')
if [ -n "$ms" ] && [ "$ms" -le 50 ]; then ok "en un Atom (QEMU -cpu n270 -icount): escribir en 100 páginas, $ms ms por letra (${t#*: })"
else ko "en un Atom: «$t»"; fi
}
