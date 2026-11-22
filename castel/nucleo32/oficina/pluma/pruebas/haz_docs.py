#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# haz_docs.py CARPETA: los documentos de prueba de los formatos de Pluma 2
# (pruebas/formatos.sh), hechos a mano como los hacen LibreOffice y Word
# (en el VPS no está LibreOffice Writer para hacerlos con él):
#   prueba.fodt  OpenDocument plano (haz_fodt.py)
#   prueba.odt   lo mismo, en su ZIP (mimetype, content.xml, styles.xml,
#                meta.xml, Pictures/, META-INF/manifest.xml)
#   prueba.docx  lo mismo en Word: estilos, numbering.xml, tabla con gridSpan
#                y vMerge, imagen flotante, nota al pie, encabezado y pie con
#                PAGE y NUMPAGES, salto de página, tabulación con puntos
import os, re, sys, zipfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import haz_fodt

sal = sys.argv[1]
os.makedirs(sal, exist_ok=True)
png = haz_fodt.png(32, 24)
fodt = haz_fodt.documento()
open(os.path.join(sal, 'prueba.fodt'), 'w', encoding='utf-8').write(fodt)

# ---- el ODT ----
ns = re.search(r'<office:document (.*?) office:mimetype', fodt, re.S).group(1)
def trozo(et):
    m = re.search(r'<%s>(.*?)</%s>' % (et, et), fodt, re.S)
    return m.group(1) if m else ''
auto = trozo('office:automatic-styles')
pl = re.search(r'<style:page-layout .*?</style:page-layout>', auto, re.S).group(0)
auto_cont = auto.replace(pl, '')
cuerpo = re.sub(r'<draw:image><office:binary-data>.*?</office:binary-data></draw:image>',
                '<draw:image xlink:href="Pictures/foto.png" xlink:type="simple" xlink:show="embed" xlink:actuate="onLoad"/>', trozo('office:body'), flags=re.S)
cab = '<?xml version="1.0" encoding="UTF-8"?>\n'
content = cab + '<office:document-content %s><office:automatic-styles>%s</office:automatic-styles><office:body>%s</office:body></office:document-content>' % (ns, auto_cont, cuerpo)
styles = cab + '<office:document-styles %s><office:styles>%s</office:styles><office:automatic-styles>%s%s</office:automatic-styles><office:master-styles>%s</office:master-styles></office:document-styles>' % (
    ns, trozo('office:styles'), pl, re.search(r'<style:style style:name="P1".*?</style:style>', auto, re.S).group(0), trozo('office:master-styles'))
meta = cab + '<office:document-meta %s><office:meta>%s</office:meta></office:document-meta>' % (ns, trozo('office:meta'))
man = cab + '''<manifest:manifest xmlns:manifest="urn:oasis:names:tc:opendocument:xmlns:manifest:1.0" manifest:version="1.3">
<manifest:file-entry manifest:full-path="/" manifest:media-type="application/vnd.oasis.opendocument.text"/>
<manifest:file-entry manifest:full-path="content.xml" manifest:media-type="text/xml"/>
<manifest:file-entry manifest:full-path="styles.xml" manifest:media-type="text/xml"/>
<manifest:file-entry manifest:full-path="meta.xml" manifest:media-type="text/xml"/>
<manifest:file-entry manifest:full-path="Pictures/foto.png" manifest:media-type="image/png"/>
</manifest:manifest>'''
with zipfile.ZipFile(os.path.join(sal, 'prueba.odt'), 'w') as z:
    z.writestr(zipfile.ZipInfo('mimetype'), 'application/vnd.oasis.opendocument.text', compress_type=zipfile.ZIP_STORED)
    for n, d in (('content.xml', content), ('styles.xml', styles), ('meta.xml', meta), ('Pictures/foto.png', png), ('META-INF/manifest.xml', man)):
        z.writestr(n, d, compress_type=zipfile.ZIP_DEFLATED)

# ---- el DOCX ----
W = ('xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships" '
     'xmlns:wp="http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing" xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main" '
     'xmlns:pic="http://schemas.openxmlformats.org/drawingml/2006/picture" xmlns:mc="http://schemas.openxmlformats.org/markup-compatibility/2006"')
def r(t, rpr=''):
    return '<w:r>%s<w:t xml:space="preserve">%s</w:t></w:r>' % (('<w:rPr>%s</w:rPr>' % rpr) if rpr else '', t)
doc = cab + '<w:document %s><w:body>' % W
doc += '<w:p><w:pPr><w:pStyle w:val="Ttulo1"/></w:pPr>%s</w:p>' % r('Título del documento')
doc += ('<w:p><w:pPr><w:jc w:val="both"/><w:ind w:firstLine="567"/><w:spacing w:line="360" w:lineRule="auto"/></w:pPr>' + r('Un párrafo justificado ') +
        r('con negrita', '<w:b/>') + r(', ') + r('cursiva subrayada', '<w:i/><w:u w:val="single"/>') + r(', ') +
        r('rojo resaltado grande', '<w:color w:val="C00000"/><w:highlight w:val="yellow"/><w:sz w:val="32"/>') + r(', x') +
        r('2', '<w:vertAlign w:val="superscript"/><w:strike/>') + r(' tachado, ') + r('versalitas', '<w:smallCaps/><w:rFonts w:ascii="DejaVu Sans Mono" w:hAnsi="DejaVu Sans Mono"/>') +
        r(' y la ñ del ñandú.') + '<w:r><w:rPr><w:vertAlign w:val="superscript"/></w:rPr><w:footnoteReference w:id="1"/></w:r>' +
        r(' Dos  espacios    y tres.') + '<w:r><w:br/></w:r>' + r('Tras un salto de línea.') + '</w:p>')
for t, lv, num in (('Viñeta uno', 0, 1), ('Viñeta dos', 0, 1), ('Subviñeta', 1, 1), ('Primero', 0, 2), ('Segundo', 0, 2), ('Tercero', 0, 2)):
    doc += '<w:p><w:pPr><w:pStyle w:val="Prrafodelista"/><w:numPr><w:ilvl w:val="%d"/><w:numId w:val="%d"/></w:numPr></w:pPr>%s</w:p>' % (lv, num, r(t))
doc += '<w:p><w:pPr><w:pStyle w:val="Cita"/></w:pPr>%s</w:p>' % r('Una cita en cursiva con sangrías.')
doc += '<w:p><w:pPr><w:pStyle w:val="Miestilo"/></w:pPr>%s</w:p>' % r('Un párrafo con un estilo propio.')
doc += '<w:p><w:pPr><w:tabs><w:tab w:val="right" w:leader="dot" w:pos="4536"/></w:tabs></w:pPr>%s<w:r><w:tab/></w:r>%s</w:p>' % (r('Capítulo'), r('7'))
tc = lambda w, cont, extra='': '<w:tc><w:tcPr><w:tcW w:w="%d" w:type="dxa"/>%s</w:tcPr>%s</w:tc>' % (w, extra, cont)
p = lambda t: '<w:p>%s</w:p>' % r(t)
doc += ('<w:tbl><w:tblPr><w:tblStyle w:val="Tablaconcuadrcula"/><w:tblW w:w="6804" w:type="dxa"/><w:jc w:val="center"/></w:tblPr>'
        '<w:tblGrid><w:gridCol w:w="2268"/><w:gridCol w:w="4536"/></w:tblGrid>'
        '<w:tr>' + tc(6804, p('Celda unida'), '<w:gridSpan w:val="2"/><w:shd w:val="clear" w:color="auto" w:fill="DDE8F4"/>') + '</w:tr>'
        '<w:tr>' + tc(2268, p('Abajo'), '<w:vMerge w:val="restart"/>') + tc(4536, p('b1') + p('b1 segunda línea')) + '</w:tr>'
        '<w:tr>' + tc(2268, '<w:p/>', '<w:vMerge/>') + tc(4536, p('b2')) + '</w:tr></w:tbl>')
doc += ('<w:p><w:r><w:drawing><wp:anchor distT="0" distB="0" distL="114300" distR="114300" simplePos="0" relativeHeight="1" behindDoc="0" locked="0" layoutInCell="1" allowOverlap="1">'
        '<wp:simplePos x="0" y="0"/><wp:positionH relativeFrom="column"><wp:align>left</wp:align></wp:positionH><wp:positionV relativeFrom="paragraph"><wp:posOffset>0</wp:posOffset></wp:positionV>'
        '<wp:extent cx="1080000" cy="810000"/><wp:wrapSquare wrapText="bothSides"/><wp:docPr id="1" name="Imagen 1"/>'
        '<a:graphic><a:graphicData uri="http://schemas.openxmlformats.org/drawingml/2006/picture"><pic:pic><pic:nvPicPr><pic:cNvPr id="1" name="foto.png"/><pic:cNvPicPr/></pic:nvPicPr>'
        '<pic:blipFill><a:blip r:embed="rId10"/><a:stretch><a:fillRect/></a:stretch></pic:blipFill><pic:spPr><a:xfrm><a:off x="0" y="0"/><a:ext cx="1080000" cy="810000"/></a:xfrm>'
        '<a:prstGeom prst="rect"><a:avLst/></a:prstGeom></pic:spPr></pic:pic></a:graphicData></a:graphic></wp:anchor></w:drawing></w:r>' + r('Un texto al lado de una imagen que flota a la izquierda.') + '</w:p>')
doc += '<w:p>%s<w:r><w:br w:type="page"/></w:r>%s</w:p>' % (r('Antes del salto.'), r('Segunda página, tras un salto.'))
doc += ('<w:sectPr><w:headerReference w:type="default" r:id="rId4"/><w:footerReference w:type="default" r:id="rId5"/>'
        '<w:pgSz w:w="11906" w:h="16838"/><w:pgMar w:top="1134" w:right="1134" w:bottom="1134" w:left="1701" w:header="567" w:footer="567" w:gutter="0"/></w:sectPr>')
doc += '</w:body></w:document>'
styles = cab + ('<w:styles %s><w:docDefaults><w:rPrDefault><w:rPr><w:rFonts w:asciiTheme="minorHAnsi" w:hAnsiTheme="minorHAnsi"/><w:sz w:val="22"/></w:rPr></w:rPrDefault>'
                '<w:pPrDefault><w:pPr><w:spacing w:after="160" w:line="259" w:lineRule="auto"/></w:pPr></w:pPrDefault></w:docDefaults>'
                '<w:style w:type="paragraph" w:default="1" w:styleId="Normal"><w:name w:val="Normal"/><w:rPr><w:rFonts w:ascii="DejaVu Serif" w:hAnsi="DejaVu Serif"/><w:sz w:val="24"/></w:rPr></w:style>'
                '<w:style w:type="paragraph" w:styleId="Ttulo1"><w:name w:val="heading 1"/><w:basedOn w:val="Normal"/><w:next w:val="Normal"/><w:pPr><w:keepNext/><w:spacing w:before="240" w:after="120"/><w:outlineLvl w:val="0"/></w:pPr><w:rPr><w:b/><w:color w:val="2F5496"/><w:sz w:val="36"/></w:rPr></w:style>'
                '<w:style w:type="paragraph" w:styleId="Cita"><w:name w:val="Quote"/><w:basedOn w:val="Normal"/><w:pPr><w:ind w:left="567" w:right="567"/></w:pPr><w:rPr><w:i/></w:rPr></w:style>'
                '<w:style w:type="paragraph" w:styleId="Miestilo"><w:name w:val="Mi estilo"/><w:basedOn w:val="Normal"/><w:rPr><w:color w:val="008000"/></w:rPr></w:style>'
                '<w:style w:type="paragraph" w:styleId="Prrafodelista"><w:name w:val="List Paragraph"/><w:basedOn w:val="Normal"/><w:pPr><w:ind w:left="720"/></w:pPr></w:style>'
                '<w:style w:type="table" w:styleId="Tablaconcuadrcula"><w:name w:val="Table Grid"/><w:tblPr><w:tblBorders><w:top w:val="single" w:sz="4" w:space="0" w:color="auto"/><w:insideH w:val="single" w:sz="4" w:space="0" w:color="auto"/></w:tblBorders></w:tblPr></w:style>'
                '</w:styles>') % W
numbering = cab + ('<w:numbering %s><w:abstractNum w:abstractNumId="0"><w:lvl w:ilvl="0"><w:start w:val="1"/><w:numFmt w:val="bullet"/><w:lvlText w:val="•"/><w:pPr><w:ind w:left="720" w:hanging="360"/></w:pPr></w:lvl>'
                   '<w:lvl w:ilvl="1"><w:start w:val="1"/><w:numFmt w:val="bullet"/><w:lvlText w:val="o"/><w:pPr><w:ind w:left="1440" w:hanging="360"/></w:pPr></w:lvl></w:abstractNum>'
                   '<w:abstractNum w:abstractNumId="1"><w:lvl w:ilvl="0"><w:start w:val="1"/><w:numFmt w:val="decimal"/><w:lvlText w:val="%%1."/><w:pPr><w:ind w:left="720" w:hanging="360"/></w:pPr></w:lvl></w:abstractNum>'
                   '<w:num w:numId="1"><w:abstractNumId w:val="0"/></w:num><w:num w:numId="2"><w:abstractNumId w:val="1"/></w:num></w:numbering>') % W
foot = cab + ('<w:footnotes %s><w:footnote w:type="separator" w:id="-1"><w:p><w:r><w:separator/></w:r></w:p></w:footnote>'
              '<w:footnote w:type="continuationSeparator" w:id="0"><w:p><w:r><w:continuationSeparator/></w:r></w:p></w:footnote>'
              '<w:footnote w:id="1"><w:p><w:r><w:rPr><w:vertAlign w:val="superscript"/></w:rPr><w:footnoteRef/></w:r>%s</w:p></w:footnote></w:footnotes>') % (W, r(' Esto es la nota al pie.'))
hdr = cab + '<w:hdr %s><w:p><w:pPr><w:jc w:val="center"/></w:pPr>%s</w:p></w:hdr>' % (W, r('Cabecera de prueba'))
ftr = cab + ('<w:ftr %s><w:p><w:pPr><w:jc w:val="center"/></w:pPr>%s<w:r><w:fldChar w:fldCharType="begin"/></w:r><w:r><w:instrText xml:space="preserve"> PAGE </w:instrText></w:r>'
             '<w:r><w:fldChar w:fldCharType="separate"/></w:r><w:r><w:t>1</w:t></w:r><w:r><w:fldChar w:fldCharType="end"/></w:r>%s<w:fldSimple w:instr=" NUMPAGES "><w:r><w:t>2</w:t></w:r></w:fldSimple></w:p></w:ftr>') % (W, r('Página '), r(' de '))
core = cab + '<cp:coreProperties xmlns:cp="http://schemas.openxmlformats.org/package/2006/metadata/core-properties" xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:title>Documento de prueba</dc:title><dc:creator>Pluma</dc:creator></cp:coreProperties>'
rels = cab + ('<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
              '<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="styles.xml"/>'
              '<Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/numbering" Target="numbering.xml"/>'
              '<Relationship Id="rId3" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/footnotes" Target="footnotes.xml"/>'
              '<Relationship Id="rId4" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/header" Target="header1.xml"/>'
              '<Relationship Id="rId5" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/footer" Target="footer1.xml"/>'
              '<Relationship Id="rId10" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/image" Target="media/image1.png"/></Relationships>')
ct = cab + ('<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
            '<Default Extension="xml" ContentType="application/xml"/><Default Extension="png" ContentType="image/png"/>'
            '<Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/></Types>')
toprels = cab + ('<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
                 '<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="word/document.xml"/></Relationships>')
with zipfile.ZipFile(os.path.join(sal, 'prueba.docx'), 'w', zipfile.ZIP_DEFLATED) as z:
    for n, d in (('[Content_Types].xml', ct), ('_rels/.rels', toprels), ('word/document.xml', doc), ('word/styles.xml', styles),
                 ('word/numbering.xml', numbering), ('word/footnotes.xml', foot), ('word/header1.xml', hdr), ('word/footer1.xml', ftr),
                 ('word/_rels/document.xml.rels', rels), ('word/media/image1.png', png), ('docProps/core.xml', core)):
        z.writestr(n, d)
print('haz_docs.py: prueba.fodt, prueba.odt y prueba.docx en', sal)
