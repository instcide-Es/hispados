#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# haz_fodt.py SALIDA.fodt: un documento de prueba en OpenDocument plano
# (para que LibreOffice lo pase a .ODT y .DOCX, solo como referencia, en las
# pruebas de Pluma 2: pruebas/formatos.sh): títulos, formato de letra,
# listas, una tabla con celdas unidas y fondo, una imagen, una nota al pie,
# un salto de página, tabulaciones y encabezado y pie con el número de página.
import base64, struct, sys, zlib

def png(an, al):
    filas = b''.join(b'\0' + b''.join(bytes([(x * 8) & 255, (y * 8) & 255, 160]) for x in range(an)) for y in range(al))
    def tr(t, d):
        return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    return b'\x89PNG\r\n\x1a\n' + tr(b'IHDR', struct.pack('>IIBBBBB', an, al, 8, 2, 0, 0, 0)) + \
        tr(b'IDAT', zlib.compress(filas)) + tr(b'IEND', b'')

def documento(img=None):
    if img is None:
        img = base64.b64encode(png(32, 24)).decode()
    return '''<?xml version="1.0" encoding="UTF-8"?>
<office:document xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" xmlns:style="urn:oasis:names:tc:opendocument:xmlns:style:1.0"
 xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0" xmlns:table="urn:oasis:names:tc:opendocument:xmlns:table:1.0"
 xmlns:draw="urn:oasis:names:tc:opendocument:xmlns:drawing:1.0" xmlns:fo="urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0"
 xmlns:xlink="http://www.w3.org/1999/xlink" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:meta="urn:oasis:names:tc:opendocument:xmlns:meta:1.0"
 xmlns:svg="urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0" office:version="1.3" office:mimetype="application/vnd.oasis.opendocument.text">
<office:meta><dc:title>Documento de prueba</dc:title><dc:creator>Pluma</dc:creator></office:meta>
<office:styles>
 <style:style style:name="Standard" style:family="paragraph"><style:text-properties fo:font-size="12pt" style:font-name="DejaVu Serif" fo:font-family="'DejaVu Serif'"/></style:style>
 <style:style style:name="Heading_20_1" style:display-name="Heading 1" style:family="paragraph" style:parent-style-name="Standard" style:default-outline-level="1">
  <style:paragraph-properties fo:margin-top="0.4cm" fo:margin-bottom="0.2cm"/><style:text-properties fo:font-size="18pt" fo:font-weight="bold" fo:color="#2f5496"/></style:style>
 <style:style style:name="Quotations" style:family="paragraph" style:parent-style-name="Standard"><style:paragraph-properties fo:margin-left="1cm" fo:margin-right="1cm"/><style:text-properties fo:font-style="italic"/></style:style>
 <style:style style:name="Mi_20_estilo" style:display-name="Mi estilo" style:family="paragraph" style:parent-style-name="Standard"><style:text-properties fo:color="#008000"/></style:style>
 <text:list-style style:name="Vinetas"><text:list-level-style-bullet text:level="1" text:bullet-char="•"><style:list-level-properties text:list-level-position-and-space-mode="label-alignment"><style:list-level-label-alignment text:label-followed-by="listtab" fo:text-indent="-0.635cm" fo:margin-left="1.27cm"/></style:list-level-properties></text:list-level-style-bullet>
  <text:list-level-style-bullet text:level="2" text:bullet-char="◦"><style:list-level-properties text:list-level-position-and-space-mode="label-alignment"><style:list-level-label-alignment fo:text-indent="-0.635cm" fo:margin-left="1.905cm"/></style:list-level-properties></text:list-level-style-bullet></text:list-style>
 <text:list-style style:name="Numeros"><text:list-level-style-number text:level="1" style:num-format="1" style:num-suffix="."><style:list-level-properties text:list-level-position-and-space-mode="label-alignment"><style:list-level-label-alignment fo:text-indent="-0.635cm" fo:margin-left="1.27cm"/></style:list-level-properties></text:list-level-style-number></text:list-style>
</office:styles>
<office:automatic-styles>
 <style:style style:name="P1" style:family="paragraph" style:parent-style-name="Standard"><style:paragraph-properties fo:text-align="center"/></style:style>
 <style:style style:name="P2" style:family="paragraph" style:parent-style-name="Standard"><style:paragraph-properties fo:text-align="justify" fo:text-indent="1cm" fo:line-height="150%"/></style:style>
 <style:style style:name="P3" style:family="paragraph" style:parent-style-name="Standard"><style:paragraph-properties fo:break-before="page"/></style:style>
 <style:style style:name="P4" style:family="paragraph" style:parent-style-name="Standard"><style:paragraph-properties><style:tab-stops><style:tab-stop style:position="8cm" style:type="right" style:leader-style="dotted" style:leader-text="."/></style:tab-stops></style:paragraph-properties></style:style>
 <style:style style:name="T1" style:family="text"><style:text-properties fo:font-weight="bold"/></style:style>
 <style:style style:name="T2" style:family="text"><style:text-properties fo:font-style="italic" style:text-underline-style="solid"/></style:style>
 <style:style style:name="T3" style:family="text"><style:text-properties fo:color="#c00000" fo:background-color="#ffff00" fo:font-size="16pt"/></style:style>
 <style:style style:name="T4" style:family="text"><style:text-properties style:text-position="super 58%" style:text-line-through-style="solid"/></style:style>
 <style:style style:name="T5" style:family="text"><style:text-properties fo:font-variant="small-caps" fo:font-family="'DejaVu Sans Mono'"/></style:style>
 <style:style style:name="Tabla1" style:family="table"><style:table-properties style:width="12cm" table:align="center"/></style:style>
 <style:style style:name="Tabla1.A" style:family="table-column"><style:table-column-properties style:column-width="4cm"/></style:style>
 <style:style style:name="Tabla1.B" style:family="table-column"><style:table-column-properties style:column-width="8cm"/></style:style>
 <style:style style:name="Ce1" style:family="table-cell"><style:table-cell-properties fo:border="0.5pt solid #000000" fo:background-color="#dde8f4"/></style:style>
 <style:style style:name="Ce2" style:family="table-cell"><style:table-cell-properties fo:border="0.5pt solid #000000"/></style:style>
 <style:style style:name="fr1" style:family="graphic"><style:graphic-properties style:wrap="right" style:horizontal-pos="left"/></style:style>
 <style:page-layout style:name="pm1"><style:page-layout-properties fo:page-width="21cm" fo:page-height="29.7cm" fo:margin-top="2cm" fo:margin-bottom="2cm" fo:margin-left="3cm" fo:margin-right="2cm"/>
  <style:header-style><style:header-footer-properties fo:min-height="0.5cm" fo:margin-bottom="0.3cm"/></style:header-style>
  <style:footer-style><style:header-footer-properties fo:min-height="0.5cm" fo:margin-top="0.3cm"/></style:footer-style></style:page-layout>
</office:automatic-styles>
<office:master-styles><style:master-page style:name="Standard" style:page-layout-name="pm1">
 <style:header><text:p text:style-name="P1">Cabecera de prueba</text:p></style:header>
 <style:footer><text:p text:style-name="P1">Página <text:page-number text:select-page="current">1</text:page-number> de <text:page-count>2</text:page-count></text:p></style:footer>
</style:master-page></office:master-styles>
<office:body><office:text>
 <text:h text:style-name="Heading_20_1" text:outline-level="1">Título del documento</text:h>
 <text:p text:style-name="P2">Un párrafo justificado <text:span text:style-name="T1">con negrita</text:span>, <text:span text:style-name="T2">cursiva subrayada</text:span>, <text:span text:style-name="T3">rojo resaltado grande</text:span>, x<text:span text:style-name="T4">2</text:span> tachado, <text:span text:style-name="T5">versalitas</text:span> y la ñ del ñandú.<text:note text:id="n1" text:note-class="footnote"><text:note-citation>1</text:note-citation><text:note-body><text:p>Esto es la nota al pie.</text:p></text:note-body></text:note> Dos  espacios<text:s text:c="3"/>y tres.<text:line-break/>Tras un salto de línea.</text:p>
 <text:list text:style-name="Vinetas"><text:list-item><text:p>Viñeta uno</text:p></text:list-item><text:list-item><text:p>Viñeta dos</text:p>
  <text:list><text:list-item><text:p>Subviñeta</text:p></text:list-item></text:list></text:list-item></text:list>
 <text:list text:style-name="Numeros"><text:list-item><text:p>Primero</text:p></text:list-item><text:list-item><text:p>Segundo</text:p></text:list-item><text:list-item><text:p>Tercero</text:p></text:list-item></text:list>
 <text:p text:style-name="Quotations">Una cita en cursiva con sangrías.</text:p>
 <text:p text:style-name="Mi_20_estilo">Un párrafo con un estilo propio.</text:p>
 <text:p text:style-name="P4">Capítulo<text:tab/>7</text:p>
 <table:table table:name="Tabla1" table:style-name="Tabla1"><table:table-column table:style-name="Tabla1.A"/><table:table-column table:style-name="Tabla1.B"/>
  <table:table-row><table:table-cell table:style-name="Ce1" table:number-columns-spanned="2"><text:p>Celda unida</text:p></table:table-cell><table:covered-table-cell/></table:table-row>
  <table:table-row><table:table-cell table:style-name="Ce2" table:number-rows-spanned="2"><text:p>Abajo</text:p></table:table-cell><table:table-cell table:style-name="Ce2"><text:p>b1</text:p><text:p>b1 segunda línea</text:p></table:table-cell></table:table-row>
  <table:table-row><table:covered-table-cell/><table:table-cell table:style-name="Ce2"><text:p>b2</text:p></table:table-cell></table:table-row>
 </table:table>
 <text:p text:style-name="Standard"><draw:frame draw:style-name="fr1" draw:name="Imagen1" text:anchor-type="char" svg:width="3cm" svg:height="2.25cm"><draw:image><office:binary-data>''' + img + '''</office:binary-data></draw:image></draw:frame>Un texto al lado de una imagen que flota a la izquierda.</text:p>
 <text:p text:style-name="P3">Segunda página, tras un salto.</text:p>
</office:text></office:body></office:document>
'''

if __name__ == '__main__':
    open(sys.argv[1], 'w', encoding='utf-8').write(documento())
