#!/bin/bash
# Hace, en la carpeta $1, unos PDF pequeños «de verdad» (hechos por otros
# programas, no por hazpdf.py) con lo que haya en la máquina: Chrome
# (CHROME.PDF), Ghostscript (GS.PDF), LibreOffice Draw (LIBREOFF.PDF) y qpdf
# (QPDF.PDF: la factura de hazpdf.py con flujos de objetos y «linealizada»; y
# QPDF2.PDF: el libro largo sin comprimir). Dice cuáles ha hecho y cuáles no.
# $2: la carpeta con los PDF de hazpdf.py (para qpdf).
D=$1; P=${2:-}
mkdir -p $D/src
cat > $D/src/carta.html <<'HTML'
<html><head><meta charset="utf-8"><title>Carta de la asociación</title>
<style>body{font-family:Liberation Serif,serif;font-size:12pt;margin:2cm} h1{font-family:Liberation Sans,sans-serif;font-size:18pt}
table{border-collapse:collapse} td,th{border:1px solid #000;padding:3pt 8pt;font-family:Liberation Sans,sans-serif;font-size:10pt} .d{text-align:right}</style></head>
<body><h1>Asociación de Jubilados</h1>
<p>Estimado socio: le escribimos para recordarle que la <b>excursión a Tordera</b> será el próximo jueves. ¡No falte! ¿Vendrá con su señora?</p>
<p>El precio es de 25 € por persona e incluye el autobús, la comida y la visita <i>guiada</i> al mercado.</p>
<table><tr><th>Concepto</th><th>Importe</th></tr><tr><td>Autobús</td><td class="d">12,50</td></tr><tr><td>Comida</td><td class="d">10,00</td></tr><tr><td>Guía</td><td class="d">2,50</td></tr></table>
<p style="font-family:Liberation Mono,monospace">Total: 25,00 euros</p>
<p>Atentamente, la Junta.</p></body></html>
HTML
cat > $D/src/carta.ps <<'PS'
%!PS
/Times-Roman findfont 14 scalefont setfont 72 760 moveto (Carta hecha con PostScript y Ghostscript) show
/Helvetica findfont 11 scalefont setfont 72 730 moveto (Segunda linea, en Helvetica, con un recuadro debajo.) show
/Courier-Bold findfont 10 scalefont setfont 72 700 moveto (Total:   25,00) show
newpath 72 650 moveto 300 650 lineto 300 690 lineto 72 690 lineto closepath stroke
0.8 setgray 320 650 60 40 rectfill
showpage
/Times-Roman findfont 14 scalefont setfont 72 760 moveto (Pagina dos) show showpage
PS
cat > $D/src/lo.fodg <<'ODG'
<?xml version="1.0" encoding="UTF-8"?>
<office:document xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" xmlns:style="urn:oasis:names:tc:opendocument:xmlns:style:1.0" xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0" xmlns:draw="urn:oasis:names:tc:opendocument:xmlns:drawing:1.0" xmlns:fo="urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0" xmlns:svg="urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0" office:version="1.2" office:mimetype="application/vnd.oasis.opendocument.graphics">
<office:automatic-styles>
<style:page-layout style:name="PM0"><style:page-layout-properties fo:margin="1cm" fo:page-width="21cm" fo:page-height="29.7cm" style:print-orientation="portrait"/></style:page-layout>
<style:style style:name="gr1" style:family="graphic"><style:graphic-properties draw:stroke="none" draw:fill="none"/></style:style>
<style:style style:name="gr2" style:family="graphic"><style:graphic-properties draw:stroke="solid" svg:stroke-color="#000000" draw:fill="solid" draw:fill-color="#ccddff"/></style:style>
<style:style style:name="T1" style:family="text"><style:text-properties fo:font-size="18pt" fo:font-weight="bold" style:font-name="Liberation Sans" fo:font-family="'Liberation Sans'"/></style:style>
<style:style style:name="T2" style:family="text"><style:text-properties fo:font-size="12pt" style:font-name="Liberation Serif" fo:font-family="'Liberation Serif'"/></style:style>
</office:automatic-styles>
<office:master-styles><style:master-page style:name="Default" style:page-layout-name="PM0"/></office:master-styles>
<office:body><office:drawing>
<draw:page draw:name="page1" draw:master-page-name="Default">
<draw:rect draw:style-name="gr2" svg:width="17cm" svg:height="1.5cm" svg:x="2cm" svg:y="2cm"/>
<draw:frame draw:style-name="gr1" svg:width="17cm" svg:height="1.2cm" svg:x="2.3cm" svg:y="2.2cm"><draw:text-box><text:p><text:span text:style-name="T1">Asociación de Jubilados</text:span></text:p></draw:text-box></draw:frame>
<draw:frame draw:style-name="gr1" svg:width="17cm" svg:height="6cm" svg:x="2cm" svg:y="5cm"><draw:text-box>
<text:p><text:span text:style-name="T2">Estimada señora: le recordamos la excursión del próximo jueves a Tordera. ¿Vendrá con su marido? ¡Anímese, que quedan pocas plazas!</text:span></text:p>
<text:p><text:span text:style-name="T2">El precio es de 25 € por persona; incluye el autobús y la comida.</text:span></text:p>
<text:p><text:span text:style-name="T2">Atentamente, la Junta.</text:span></text:p>
</draw:text-box></draw:frame>
<draw:line draw:style-name="gr2" svg:x1="2cm" svg:y1="12cm" svg:x2="19cm" svg:y2="12cm"/>
</draw:page></office:drawing></office:body></office:document>
ODG
hecho() { [ -s "$D/$1" ] && echo "  hecho $1 (con $2)" || { rm -f "$D/$1"; echo "  (sin $1: no hay $2 o no ha funcionado)"; }; }
C=$(command -v google-chrome || command -v chromium || command -v chromium-browser)
[ -n "$C" ] && timeout 90 "$C" --headless --no-sandbox --disable-gpu --no-pdf-header-footer --print-to-pdf=$D/CHROME.PDF $D/src/carta.html > /dev/null 2>&1
hecho CHROME.PDF Chrome
command -v ps2pdf > /dev/null && ps2pdf $D/src/carta.ps $D/GS.PDF > /dev/null 2>&1
hecho GS.PDF Ghostscript
if command -v soffice > /dev/null; then
  timeout 180 soffice -env:UserInstallation=file://$D/src/perfil --headless --convert-to pdf --outdir $D/src $D/src/lo.fodg > /dev/null 2>&1
  [ -s $D/src/lo.pdf ] && mv $D/src/lo.pdf $D/LIBREOFF.PDF
fi
hecho LIBREOFF.PDF LibreOffice
if command -v qpdf > /dev/null && [ -n "$P" ]; then
  qpdf --object-streams=generate --linearize $P/FACTURA.PDF $D/QPDF.PDF > /dev/null 2>&1
  qpdf --object-streams=disable --stream-data=uncompress $P/LARGO.PDF $D/QPDF2.PDF > /dev/null 2>&1
fi
hecho QPDF.PDF qpdf
hecho QPDF2.PDF qpdf
for x in pdflatex mutool cupsfilter; do command -v $x > /dev/null || echo "  (no hay $x en esta máquina)"; done
rm -rf $D/src
