#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# haz_ref.py CARPETA: los libros de referencia de las pruebas de los
# formatos de Cuentas 2 (formatos.sh), hechos a mano (en el VPS no hay
# openpyxl, odfpy ni LibreOffice Calc), con la forma de los de verdad:
#   hoj386.hoj    como los guarda Cuentas de Hispa-386 (página 437): tres
#                 hojas (la segunda y la tercera, «plegadas»), fórmulas en
#                 español, bits de formato, anchos, altos, paneles, gráfico
#   excel.xlsx    como los de Excel: textos compartidos (con trozos de
#                 formato), fórmulas compartidas, estilos (formatos de serie y
#                 propios, letras, rellenos con trama, bordes), combinadas,
#                 columnas, alto de fila, fila oculta, panel inmovilizado,
#                 nombre definido, área de impresión, comentario, condicional
#   e1904.xlsx    uno con las fechas de 1904
#   libre.ods     como los de LibreOffice: filas y columnas repetidas (un
#                 millón de filas vacías), fórmulas «of:=», fechas, horas,
#                 porcentajes, moneda, text:s, varias líneas, unidas y
#                 cubiertas, anotación, estilos de número, nombre, paneles
#   pc.csv        «;» en UTF-8 con marca; coma.csv «,» en Windows-1252;
#   tab.csv       tabuladores en 437
# y esperado_*.txt: lo que tiene que salir en «CUENTAS --valores» (lo mira
# compara.py; «\\n» es un salto de línea dentro de la celda).
import sys, os, zipfile

d = sys.argv[1]
os.makedirs(d, exist_ok=True)

def esc(p, n, t):
    open(os.path.join(d, n), 'wb').write(t)

# ---- .HOJ de 386 ----
F_NEG, F_CUR = 1, 2
def bits(neg=0, cur=0, alin=0, color=0, fn=0, dec=0, baba=0, bder=0, fondo=0, comb=0):
    return neg | cur << 1 | alin << 2 | color << 4 | fn << 7 | dec << 10 | baba << 14 | bder << 15 | fondo << 16 | comb << 20
l = ['HOJA 1', 'M EUR', 'L 1', 'S Gastos',
     'A A1 140', 'A C1 60', 'H 1 30', 'P 0 1',
     'G 0 A1:B4 0', 'T Gastos del mes', 'X Concepto', 'Y Euros',
     'C A1 %d Concepto' % bits(neg=1, alin=2, fondo=1, baba=1),
     'C B1 %d Importe' % bits(neg=1, alin=3, color=1),
     'C A2 0 Pan', 'C B2 %d 3,5' % bits(fn=3, dec=2),
     'C A3 0 Leche', 'C B3 %d 1,25' % bits(fn=3, dec=2),
     'C A4 0 Año', 'C B4 %d =SUMA(B2:B3)' % bits(fn=3, dec=2, bder=1),
     'C C2 %d 0,21' % bits(fn=4),
     'C C3 %d 16/11/2026' % bits(fn=5),
     'C C4 0 =ANO(C3)',
     'C D1 %d Título unido' % bits(comb=2, alin=2),
     'C A6 0 =Datos!A1*2',
     'C A7 0 =SI(B4>4;"mucho";"poco")',
     'C A8 0 =PAGO(5%/12;12;-1000)',
     'C A9 %d 1234,5678' % bits(fn=2, dec=1),
     'S Datos', 'V 0 2 0 0', 'C A1 0 21', 'C A2 0 =A1+1', 'C B1 %d ñandú' % bits(cur=1, color=2),
     'S Otra', 'C A1 0 #ERROR', 'C A2 0 =1/0', 'C A3 0 =CONTAR.SI(Datos!A1:A2;">21")']
esc(d, 'hoj386.hoj', ('\r\n'.join(l) + '\r\n').encode('cp437'))
esc(d, 'esperado_hoj386.txt', '\n'.join([
    '[Gastos]', 'A1=Concepto', 'B2=3,50 EUR', 'B4=4,75 EUR', 'C2=21%', 'C3=16/11/2026', 'C4=2026', 'A4=Año',
    'D1=Título unido', 'A6=42', 'A7=mucho', 'A8=85,60748179', 'A9=1.234,6',
    '[Datos]', 'A2=22', 'B1=ñandú', '[Otra]', 'A1=#¡VALOR!', 'A2=#¡DIV/0!', 'A3=1']).encode() + b'\n')

# ---- .XLSX como los de Excel ----
NS = 'xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"'
def xlsx(nombre, hojas, ss, estilos, libro_extra='', fecha1904=False, comentarios=None):
    z = zipfile.ZipFile(os.path.join(d, nombre), 'w', zipfile.ZIP_DEFLATED)
    ct = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\r\n<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
          '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Default Extension="xml" ContentType="application/xml"/>'
          '<Default Extension="vml" ContentType="application/vnd.openxmlformats-officedocument.vmlDrawing"/>'
          '<Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/>')
    for i in range(len(hojas)):
        ct += '<Override PartName="/xl/worksheets/sheet%d.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>' % (i + 1)
    ct += '</Types>'
    z.writestr('[Content_Types].xml', ct)
    z.writestr('_rels/.rels', '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\r\n<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
               '<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/></Relationships>')
    wb = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\r\n<workbook %s><workbookPr%s/><bookViews><workbookView activeTab="0"/></bookViews><sheets>' % (NS, ' date1904="1"' if fecha1904 else '')
    rels = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\r\n<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
    for i, (n, x) in enumerate(hojas):
        wb += '<sheet name="%s" sheetId="%d" r:id="rId%d"/>' % (n, i + 1, i + 1)
        rels += '<Relationship Id="rId%d" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet%d.xml"/>' % (i + 1, i + 1)
        z.writestr('xl/worksheets/sheet%d.xml' % (i + 1), '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\r\n<worksheet %s>%s</worksheet>' % (NS, x))
    rels += ('<Relationship Id="rId90" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="styles.xml"/>'
             '<Relationship Id="rId91" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/sharedStrings" Target="sharedStrings.xml"/></Relationships>')
    wb += '</sheets>%s</workbook>' % libro_extra
    z.writestr('xl/workbook.xml', wb)
    z.writestr('xl/_rels/workbook.xml.rels', rels)
    z.writestr('xl/styles.xml', '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\r\n<styleSheet %s>%s</styleSheet>' % (NS, estilos))
    z.writestr('xl/sharedStrings.xml', '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\r\n<sst %s count="%d" uniqueCount="%d">%s</sst>' % (NS, len(ss), len(ss), ''.join(ss)))
    if comentarios:
        z.writestr('xl/worksheets/_rels/sheet1.xml.rels', '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\r\n<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
                   '<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/comments" Target="../comments1.xml"/></Relationships>')
        z.writestr('xl/comments1.xml', comentarios)
    z.close()

estilos = ('<numFmts count="2"><numFmt numFmtId="164" formatCode="#,##0.00\\ &quot;€&quot;"/><numFmt numFmtId="165" formatCode="0.0%"/></numFmts>'
           '<fonts count="3"><font><sz val="11"/><color theme="1"/><name val="Calibri"/><family val="2"/><scheme val="minor"/></font>'
           '<font><b/><i/><sz val="14"/><color rgb="FFC00000"/><name val="Arial"/></font><font><u/><strike/><sz val="9"/><color indexed="12"/><name val="Times New Roman"/></font></fonts>'
           '<fills count="4"><fill><patternFill patternType="none"/></fill><fill><patternFill patternType="gray125"/></fill>'
           '<fill><patternFill patternType="solid"><fgColor rgb="FFFFFF00"/><bgColor indexed="64"/></patternFill></fill>'
           '<fill><patternFill patternType="darkDown"><fgColor rgb="FF0000FF"/><bgColor rgb="FFCCFFCC"/></patternFill></fill></fills>'
           '<borders count="2"><border><left/><right/><top/><bottom/><diagonal/></border>'
           '<border><left style="thin"><color indexed="64"/></left><right style="medium"><color rgb="FFFF0000"/></right><top style="double"/><bottom style="dashed"><color auto="1"/></bottom><diagonal/></border></borders>'
           '<cellStyleXfs count="1"><xf numFmtId="0" fontId="0" fillId="0" borderId="0"/></cellStyleXfs>'
           '<cellXfs count="7"><xf numFmtId="0" fontId="0" fillId="0" borderId="0" xfId="0"/>'
           '<xf numFmtId="14" fontId="0" fillId="0" borderId="0" xfId="0" applyNumberFormat="1"/>'
           '<xf numFmtId="164" fontId="0" fillId="0" borderId="0" xfId="0" applyNumberFormat="1"/>'
           '<xf numFmtId="0" fontId="1" fillId="2" borderId="1" xfId="0" applyFont="1" applyFill="1" applyBorder="1" applyAlignment="1"><alignment horizontal="center" vertical="center" wrapText="1"/></xf>'
           '<xf numFmtId="165" fontId="2" fillId="3" borderId="0" xfId="0"/>'
           '<xf numFmtId="4" fontId="0" fillId="0" borderId="0" xfId="0" applyNumberFormat="1"/>'
           '<xf numFmtId="22" fontId="0" fillId="0" borderId="0" xfId="0" applyNumberFormat="1"/></cellXfs>'
           '<cellStyles count="1"><cellStyle name="Normal" xfId="0" builtinId="0"/></cellStyles>'
           '<dxfs count="1"><dxf><font><b/><color rgb="FF9C0006"/></font><fill><patternFill><bgColor rgb="FFFFC7CE"/></patternFill></fill></dxf></dxfs>')
ss = ['<si><t>Producto</t></si>', '<si><t>Precio</t></si>', '<si><r><rPr><b/></rPr><t>Pan </t></r><r><t>integral</t></r></si>',
      '<si><t>Leche</t></si>', '<si><t xml:space="preserve"> con blancos </t></si>', '<si><t>Año ñ €</t><rPh sb="0" eb="1"><t>X</t></rPh></si>']
h1 = ('<sheetPr><pageSetUpPr fitToPage="1"/></sheetPr><dimension ref="A1:E8"/><sheetViews><sheetView tabSelected="1" workbookViewId="0">'
      '<pane ySplit="1" topLeftCell="A2" activePane="bottomLeft" state="frozen"/><selection pane="bottomLeft" activeCell="B3" sqref="B3"/></sheetView></sheetViews>'
      '<sheetFormatPr defaultRowHeight="15"/><cols><col min="1" max="1" width="20.7109375" customWidth="1"/><col min="3" max="4" width="5" hidden="1" customWidth="1"/></cols>'
      '<sheetData><row r="1" ht="30" customHeight="1"><c r="A1" s="3" t="s"><v>0</v></c><c r="B1" s="3" t="s"><v>1</v></c><c r="E1" t="inlineStr"><is><t>en línea</t></is></c></row>'
      '<row r="2"><c r="A2" t="s"><v>2</v></c><c r="B2" s="2"><v>3.5</v></c><c r="C2"><f>B2*2</f><v>7</v></c><c r="D2"><f t="shared" ref="D2:D4" si="0">B2+C2</f><v>10.5</v></c></row>'
      '<row r="3"><c r="A3" t="s"><v>3</v></c><c r="B3" s="2"><v>1.25</v></c><c r="C3"><f>B3*2</f><v>2.5</v></c><c r="D3"><f t="shared" si="0"/><v>3.75</v></c></row>'
      '<row r="4" hidden="1"><c r="A4" t="s"><v>4</v></c><c r="B4" s="2"><f>SUM(B2:B3)</f><v>4.75</v></c><c r="D4"><f t="shared" si="0"/><v>4.75</v></c></row>'
      '<row r="5"><c r="A5" s="1"><v>46348</v></c><c r="B5" s="4"><v>0.215</v></c><c r="C5" t="b"><v>1</v></c><c r="D5" t="e"><v>#N/A</v></c><c r="E5" t="str"><f>_xlfn.CONCAT(A3,"!")</f><v>Leche!</v></c></row>'
      '<row r="6"><c r="A6" s="5"><v>1234567.891</v></c><c r="B6" s="6"><v>46348.5</v></c><c r="C6" t="s"><v>5</v></c><c r="D6"><f>Total*2</f><v>9.5</v></c><c r="E6"><f>IFERROR(1/0,"cero")</f><v>0</v></c></row>'
      '<row r="7"><c r="A7"><f>Datos!A1+1</f><v>0</v></c><c r="B7"><f>VLOOKUP("Leche",A2:B3,2,FALSE)</f><v>0</v></c></row>'
      '<row r="8"><c r="A8"><f>DATE(2026,11,22)</f></c></row></sheetData>'
      '<mergeCells count="1"><mergeCell ref="A8:C8"/></mergeCells>'
      '<conditionalFormatting sqref="B2:B4"><cfRule type="cellIs" dxfId="0" priority="1" operator="greaterThan"><formula>2</formula></cfRule></conditionalFormatting>'
      '<pageMargins left="0.7" right="0.7" top="0.75" bottom="0.75" header="0.3" footer="0.3"/><pageSetup paperSize="9" orientation="landscape" fitToWidth="1" fitToHeight="0"/>'
      '<headerFooter><oddHeader>&amp;C&amp;A</oddHeader><oddFooter>&amp;CPágina &amp;P de &amp;N</oddFooter></headerFooter>')
h2 = '<sheetData><row r="1"><c r="A1"><v>41</v></c></row></sheetData>'
libro_extra = ('<definedNames><definedName name="_xlnm.Print_Area" localSheetId="0">Hoja1!$A$1:$E$8</definedName>'
               '<definedName name="Total">Hoja1!$B$4</definedName></definedNames><calcPr calcId="191029"/>')
com = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\r\n<comments xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><authors><author>Carlos</author></authors>'
       '<commentList><comment ref="B2" authorId="0"><text><r><rPr><b/></rPr><t>Carlos:</t></r><r><t xml:space="preserve"> el precio con IVA</t></r></text></comment></commentList></comments>')
xlsx('excel.xlsx', [('Hoja1', h1), ('Datos', h2)], ss, estilos, libro_extra, comentarios=com)
esc(d, 'esperado_excel.txt', '\n'.join([
    '[Hoja1]', 'A1=Producto', 'B2=3,50 €', 'A2=Pan integral', 'D2=10,5', 'D3=3,75', 'D4=4,75', 'B4=4,75 €', 'A4= con blancos ',
    'A5=22/11/2026', 'B5=21,5%', 'C5=VERDADERO', 'D5=#N/A', 'E5=Leche!', 'E1=en línea', 'A6=1.234.567,89', 'B6=22/11/2026 12:00',
    'C6=Año ñ €', 'D6=9,5', 'E6=cero', 'A7=42', 'B7=1,25', 'A8=46348', '[Datos]', 'A1=41']).encode() + b'\n')
h3 = '<sheetData><row r="1"><c r="A1" s="1"><v>44886</v></c></row></sheetData>'
xlsx('e1904.xlsx', [('Hoja1', h3)], [], estilos, fecha1904=True)
esc(d, 'esperado_e1904.txt', b'[Hoja1]\nA1=22/11/2026\n')

# ---- .ODS como los de LibreOffice ----
NSO = ('xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" xmlns:style="urn:oasis:names:tc:opendocument:xmlns:style:1.0" '
       'xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0" xmlns:table="urn:oasis:names:tc:opendocument:xmlns:table:1.0" '
       'xmlns:fo="urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0" xmlns:dc="http://purl.org/dc/elements/1.1/" '
       'xmlns:number="urn:oasis:names:tc:opendocument:xmlns:datastyle:1.0" xmlns:of="urn:oasis:names:tc:opendocument:xmlns:of:1.2" '
       'xmlns:config="urn:oasis:names:tc:opendocument:xmlns:config:1.0" office:version="1.3"')
cont = ('<?xml version="1.0" encoding="UTF-8"?>\n<office:document-content %s><office:automatic-styles>'
        '<style:style style:name="co1" style:family="table-column"><style:table-column-properties fo:break-before="auto" style:column-width="3.5cm"/></style:style>'
        '<style:style style:name="co2" style:family="table-column"><style:table-column-properties fo:break-before="auto" style:column-width="2.258cm"/></style:style>'
        '<style:style style:name="ro1" style:family="table-row"><style:table-row-properties style:row-height="0.452cm" fo:break-before="auto" style:use-optimal-row-height="true"/></style:style>'
        '<style:style style:name="ro2" style:family="table-row"><style:table-row-properties style:row-height="1cm" fo:break-before="auto" style:use-optimal-row-height="false"/></style:style>'
        '<number:number-style style:name="N4"><number:number number:decimal-places="2" number:min-decimal-places="2" number:min-integer-digits="1" number:grouping="true"/></number:number-style>'
        '<number:percentage-style style:name="N11"><number:number number:decimal-places="1" number:min-integer-digits="1"/><number:text>%%</number:text></number:percentage-style>'
        '<number:currency-style style:name="N111P0" style:volatile="true"><number:number number:decimal-places="2" number:min-integer-digits="1" number:grouping="true"/><number:text> </number:text><number:currency-symbol number:language="es" number:country="ES">€</number:currency-symbol></number:currency-style>'
        '<number:date-style style:name="N37" number:automatic-order="true"><number:day number:style="long"/><number:text>/</number:text><number:month number:style="long"/><number:text>/</number:text><number:year number:style="long"/></number:date-style>'
        '<number:time-style style:name="N41"><number:hours number:style="long"/><number:text>:</number:text><number:minutes number:style="long"/></number:time-style>'
        '<style:style style:name="ce1" style:family="table-cell" style:parent-style-name="Default"><style:table-cell-properties fo:background-color="#ffff00" fo:border="0.06pt solid #000000"/>'
        '<style:paragraph-properties fo:text-align="center"/><style:text-properties fo:font-weight="bold" fo:color="#c9211e" style:font-name="Liberation Serif" fo:font-size="14pt"/></style:style>'
        '<style:style style:name="ce2" style:family="table-cell" style:parent-style-name="Default" style:data-style-name="N4"/>'
        '<style:style style:name="ce3" style:family="table-cell" style:parent-style-name="Default" style:data-style-name="N11"/>'
        '<style:style style:name="ce4" style:family="table-cell" style:parent-style-name="Default" style:data-style-name="N111P0"/>'
        '<style:style style:name="ce5" style:family="table-cell" style:parent-style-name="Default" style:data-style-name="N37"/>'
        '<style:style style:name="ce6" style:family="table-cell" style:parent-style-name="Default" style:data-style-name="N41"/>'
        '</office:automatic-styles><office:body><office:spreadsheet><table:calculation-settings table:automatic-find-labels="false"/>'
        '<table:table table:name="Hoja 1" table:style-name="ta1" table:print-ranges="\'Hoja 1\'.A1:\'Hoja 1\'.D6">'
        '<table:table-column table:style-name="co1" table:default-cell-style-name="Default"/>'
        '<table:table-column table:style-name="co2" table:number-columns-repeated="1023" table:default-cell-style-name="Default"/>'
        '<table:table-row table:style-name="ro2"><table:table-cell table:style-name="ce1" office:value-type="string"><text:p>Producto</text:p></table:table-cell>'
        '<table:table-cell table:style-name="ce1" office:value-type="string"><text:p>Precio</text:p></table:table-cell><table:table-cell table:number-columns-repeated="1022"/></table:table-row>'
        '<table:table-row table:style-name="ro1"><table:table-cell office:value-type="string"><text:p>Pan<text:s/>de<text:s text:c="2"/>molde</text:p>'
        '<office:annotation><dc:creator>Carlos</dc:creator><text:p>el de siempre</text:p></office:annotation></table:table-cell>'
        '<table:table-cell table:style-name="ce4" office:value-type="currency" office:currency="EUR" office:value="3.5"><text:p>3,50 €</text:p></table:table-cell>'
        '<table:table-cell table:formula="of:=[.B2]*2" office:value-type="float" office:value="7"><text:p>7</text:p></table:table-cell>'
        '<table:table-cell table:formula="of:=SUM([.B2:.C2])" office:value-type="float" office:value="10.5"><text:p>10,5</text:p></table:table-cell><table:table-cell table:number-columns-repeated="1020"/></table:table-row>'
        '<table:table-row table:style-name="ro1"><table:table-cell office:value-type="string"><text:p>Dos</text:p><text:p>líneas</text:p></table:table-cell>'
        '<table:table-cell table:style-name="ce3" office:value-type="percentage" office:value="0.215"><text:p>21,5%%</text:p></table:table-cell>'
        '<table:table-cell table:style-name="ce5" office:value-type="date" office:date-value="2026-11-22"><text:p>22/11/2026</text:p></table:table-cell>'
        '<table:table-cell table:style-name="ce6" office:value-type="time" office:time-value="PT14H30M00S"><text:p>14:30</text:p></table:table-cell><table:table-cell table:number-columns-repeated="1020"/></table:table-row>'
        '<table:table-row table:style-name="ro1"><table:table-cell table:number-columns-spanned="2" table:number-rows-spanned="1" office:value-type="string"><text:p>Unidas</text:p></table:table-cell>'
        '<table:covered-table-cell/><table:table-cell office:value-type="boolean" office:boolean-value="true"><text:p>VERDADERO</text:p></table:table-cell>'
        '<table:table-cell table:style-name="ce2" office:value-type="float" office:value="1234567.891"><text:p>1.234.567,89</text:p></table:table-cell><table:table-cell table:number-columns-repeated="1020"/></table:table-row>'
        '<table:table-row table:style-name="ro1" table:number-rows-repeated="2"><table:table-cell table:number-columns-repeated="1024"/></table:table-row>'
        '<table:table-row table:style-name="ro1"><table:table-cell table:formula="of:=[$Datos.A1]+1" office:value-type="float" office:value="0"><text:p>0</text:p></table:table-cell>'
        '<table:table-cell table:formula="of:=IF([.B2]&gt;2;&quot;caro&quot;;&quot;barato&quot;)" office:value-type="string" office:string-value="caro"><text:p>caro</text:p></table:table-cell>'
        '<table:table-cell table:formula="of:=Total*2" office:value-type="float" office:value="0"><text:p>0</text:p></table:table-cell>'
        '<table:table-cell table:formula="of:=VLOOKUP(&quot;Pan de  molde&quot;;[.A2:.B3];2;0)" office:value-type="float" office:value="0"><text:p>0</text:p></table:table-cell>'
        '<table:table-cell table:number-columns-repeated="1020"/></table:table-row>'
        '<table:table-row table:style-name="ro1" table:number-rows-repeated="1048568"><table:table-cell table:number-columns-repeated="1024"/></table:table-row>'
        '<table:table-row table:style-name="ro1"><table:table-cell table:number-columns-repeated="1024"/></table:table-row></table:table>'
        '<table:table table:name="Datos"><table:table-column table:number-columns-repeated="1024"/><table:table-row>'
        '<table:table-cell office:value-type="float" office:value="41"><text:p>41</text:p></table:table-cell><table:table-cell table:number-columns-repeated="1023"/></table:table-row>'
        '<table:table-row table:number-rows-repeated="1048575"><table:table-cell table:number-columns-repeated="1024"/></table:table-row></table:table>'
        '<table:named-expressions><table:named-range table:name="Total" table:base-cell-address="$\'Hoja 1\'.$A$1" table:cell-range-address="$\'Hoja 1\'.$B$2"/></table:named-expressions>'
        '</office:spreadsheet></office:body></office:document-content>') % NSO
sett = ('<?xml version="1.0" encoding="UTF-8"?>\n<office:document-settings %s><office:settings><config:config-item-set config:name="ooo:view-settings">'
        '<config:config-item-map-indexed config:name="Views"><config:config-item-map-entry><config:config-item-map-named config:name="Tables">'
        '<config:config-item-map-entry config:name="Hoja 1"><config:config-item config:name="CursorPositionX" config:type="int">1</config:config-item>'
        '<config:config-item config:name="CursorPositionY" config:type="int">2</config:config-item>'
        '<config:config-item config:name="HorizontalSplitMode" config:type="short">0</config:config-item>'
        '<config:config-item config:name="VerticalSplitMode" config:type="short">2</config:config-item>'
        '<config:config-item config:name="VerticalSplitPosition" config:type="int">1</config:config-item></config:config-item-map-entry>'
        '</config:config-item-map-named><config:config-item config:name="ActiveTable" config:type="string">Hoja 1</config:config-item>'
        '</config:config-item-map-entry></config:config-item-map-indexed></config:config-item-set></office:settings></office:document-settings>') % NSO
z = zipfile.ZipFile(os.path.join(d, 'libre.ods'), 'w', zipfile.ZIP_DEFLATED)
z.writestr(zipfile.ZipInfo('mimetype'), 'application/vnd.oasis.opendocument.spreadsheet', compress_type=zipfile.ZIP_STORED)
z.writestr('content.xml', cont)
z.writestr('styles.xml', '<?xml version="1.0" encoding="UTF-8"?>\n<office:document-styles %s><office:styles><style:style style:name="Default" style:family="table-cell"/></office:styles></office:document-styles>' % NSO)
z.writestr('settings.xml', sett)
z.writestr('META-INF/manifest.xml', '<?xml version="1.0" encoding="UTF-8"?>\n<manifest:manifest xmlns:manifest="urn:oasis:names:tc:opendocument:xmlns:manifest:1.0" manifest:version="1.3">'
           '<manifest:file-entry manifest:full-path="/" manifest:media-type="application/vnd.oasis.opendocument.spreadsheet"/>'
           '<manifest:file-entry manifest:full-path="content.xml" manifest:media-type="text/xml"/></manifest:manifest>')
z.close()
esc(d, 'esperado_libre.txt', '\n'.join([
    '[Hoja 1]', 'A1=Producto', 'A2=Pan de  molde', 'B2=3,50 €', 'C2=7', 'D2=10,5', 'A3=Dos\\nlíneas', 'B3=21,5%', 'C3=22/11/2026', 'D3=14:30',
    'A4=Unidas', 'C4=VERDADERO', 'D4=1.234.567,89', 'A7=42', 'B7=caro', 'C7=7', 'D7=3,5', '[Datos]', 'A1=41']).encode() + b'\n')

# ---- .CSV ----
esc(d, 'pc.csv', '﻿Nombre;Precio;Fecha\r\n"Pan; integral";3,50;22/11/2026\r\nLeche;1.234,5;\r\n"Con ""comillas""";=SUMA(B2:B3);\r\n"dos\r\nlíneas";;\r\n'.encode('utf-8'))
esc(d, 'esperado_pc.txt', '\n'.join(['A1=Nombre', 'A2=Pan; integral', 'B2=3,5', 'C2=22/11/2026', 'B3=1.234,50', 'A4=Con "comillas"', 'B4=1238', 'A5=dos\\nlíneas']).encode() + b'\n')
esc(d, 'coma.csv', 'Name,Price\r\nCafé €,3.5\r\n"a,b",1234.5\r\nTotal,=SUM(B2:B3)\r\n'.encode('cp1252'))
esc(d, 'esperado_coma.txt', '\n'.join(['A2=Café €', 'B2=3,5', 'A3=a,b', 'B3=1234,5', 'B4=1238']).encode() + b'\n')
esc(d, 'tab.csv', 'Año\tÑ\r\n1\t2\r\n'.encode('cp437'))
esc(d, 'esperado_tab.txt', '\n'.join(['A1=Año', 'B1=Ñ', 'A2=1', 'B2=2']).encode() + b'\n')
