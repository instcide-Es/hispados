#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# cuentas/pruebas/formatos.sh CUENTAS [CARPETA] - las pruebas de los formatos
# de Cuentas 2 (.HOJ, .XLSX, .ODS y .CSV) en el anfitrión. CUENTAS es Cuentas
# 2 compilado para Linux con ASan y UBSan (cuentas/anfitrion.sh):
#   1. los libros de referencia (haz_ref.py: un .HOJ de Cuentas de Hispa-386
#      con hojas plegadas, un .XLSX como los de Excel, otro con las fechas de
#      1904, un .ODS como los de LibreOffice y tres .CSV) se leen con lo que
#      llevan (compara.py contra esperado_*.txt);
#   2. un libro hecho con un guion (prueba.c: formatos, combinadas,
#      comentario, condicional, gráfico, nombre, paneles, área de impresión,
#      dos hojas) se guarda en .HOJ, .XLSX, .ODS y .CSV;
#   3. ida y vuelta: cada libro a .HOJ, .XLSX y .ODS y otra vez a .HOJ: lo que
#      se ve en cada celda es lo mismo, y lo que lleva (--info) también (salvo
#      lo que ese formato no guarda: los gráficos en .XLSX y .ODS, los
#      condicionales en .ODS); los anchos y los altos, los mismos; un .HOJ
#      guardado otra vez como .HOJ, octeto a octeto igual;
#   4. un .HOJ de Cuentas 2 leído por Cuentas de 386: delante va lo suyo (las
#      líneas que entiende), y sin las «2x» se lee lo esencial;
#   5. lo escrito es un ZIP y un XML bien hechos (python3: zipfile y
#      xml.dom.minidom; las partes de las relaciones existen; el «mimetype»
#      del .ODS, el primero y sin comprimir; y «unzip -t», si está);
#   6. con FUZZ=n, n vueltas de fuzz.py (XLSX, ODS, HOJ y CSV estropeados).
# Al final, «== N bien, M mal».
set -u
AQUI=$(dirname "$(readlink -f "$0")")
C=$(readlink -f "${1:?uso: formatos.sh CUENTAS [CARPETA]}")
T=${2:-$(mktemp -d)}
mkdir -p "$T"
nok=0; fail=0
ok() { echo "  bien: $1"; nok=$((nok + 1)); }
ko() { echo "  MAL:  $1"; fail=$((fail + 1)); }
export ASAN_OPTIONS=detect_leaks=1:exitcode=99 UBSAN_OPTIONS=print_stacktrace=1:exitcode=98
san() { grep -a -q "runtime error\|Sanitizer\|LeakSanitizer" "$1"; }
python3 -I "$AQUI/haz_ref.py" "$T" || { ko "haz_ref.py"; echo "== $nok bien, $fail mal =="; exit 1; }
# ---- 1. los de referencia ----
for x in hoj386.hoj excel.xlsx e1904.xlsx libre.ods pc.csv coma.csv tab.csv; do
	b=${x%.*}
	$C --valores "$T/$x" > "$T/v_$b.txt" 2>&1
	if ! san "$T/v_$b.txt" && python3 -I "$AQUI/compara.py" "$T/v_$b.txt" "$T/esperado_$b.txt" > "$T/c_$b.txt"; then
		ok "se lee $x con lo que lleva"
	else ko "$x: $(head -3 "$T/c_$b.txt" | tr '\n' ' ')"; grep -a "runtime error\|Sanitizer" "$T/v_$b.txt" | head -3; fi
done
t=$($C --info "$T/excel.xlsx" 2>&1)
case "$t" in *"2 hojas, 28 celdas, 12 fórmulas, 0 gráficos, 1 combinadas, 1 comentarios, 1 condicionales, 1 nombres"*) ok "excel.xlsx: lo que lleva (${t#*; })" ;; *) ko "excel.xlsx: $t" ;; esac
t=$($C --info "$T/libre.ods" 2>&1)
case "$t" in *"2 hojas, 18 celdas, 6 fórmulas, 0 gráficos, 1 combinadas, 1 comentarios, 0 condicionales, 1 nombres"*) ok "libre.ods: lo que lleva (${t#*; })" ;; *) ko "libre.ods: $t" ;; esac
t=$($C --info "$T/hoj386.hoj" 2>&1)
case "$t" in *"3 hojas, 22 celdas, 8 fórmulas, 1 gráficos, 1 combinadas"*) ok "hoj386.hoj: lo que lleva (${t#*; })" ;; *) ko "hoj386.hoj: $t" ;; esac
# lo de la hoja: anchos, paneles, ocultas, área, comentario, página...
$C --convierte "$T/excel.xlsx" "$T/x.hoj" > /dev/null 2>&1
if grep -a -q "^2W	0	206" "$T/x.hoj" && grep -a -q "^2Y	2" "$T/x.hoj" && grep -a -q "^2R	0	40" "$T/x.hoj" && grep -a -q "^2Z	3" "$T/x.hoj" &&
   grep -a -q "^2I	1	0" "$T/x.hoj" && grep -a -q "^2P	A1:E8" "$T/x.hoj" && grep -a -q "^2K	B2	Carlos	Carlos: el precio con IVA" "$T/x.hoj" &&
   grep -a -q "^2X	1	0	0	0	0	0	100	1	0	1.778" "$T/x.hoj" && grep -a -q "^2J	B2:B4	0	0	1	0	2	" "$T/x.hoj"; then
	ok "excel.xlsx: anchos, columnas y filas ocultas, alto, panel, área, comentario, página apaisada, condicional"
else ko "excel.xlsx: lo de la hoja no casa"; grep -a "^2[WYRZIPKXJ]" "$T/x.hoj" | head -12; fi
if grep -a -q "^2E	[0-9]*	Arial	28	1	1	0	0	C00000	2	1	1	0	General	1	0	FFFF00	0	1	0	4	0	2	FF0000	6	0" "$T/x.hoj" &&
   grep -a -q "Times New Roman	18	0	0	1	1	FF	.*0.0%	8	0	CCFFCC	FF" "$T/x.hoj"; then
	ok "excel.xlsx: las letras, los rellenos (sólido y con trama), los bordes y la alineación"
else ko "excel.xlsx: los estilos no casan"; grep -a "^2E" "$T/x.hoj" | head -8; fi
$C --convierte "$T/libre.ods" "$T/o.hoj" > /dev/null 2>&1
if grep -a -q "^2W	0	132" "$T/o.hoj" && grep -a -q "^2D	85" "$T/o.hoj" && grep -a -q "^2I	1	0" "$T/o.hoj" && grep -a -q "^2P	A1:D6" "$T/o.hoj" &&
   grep -a -q "^2K	A2	Carlos	el de siempre" "$T/o.hoj" && grep -a -q "^2M	A4:B4" "$T/o.hoj" && grep -a -q "^2R	0	38" "$T/o.hoj" &&
   grep -a -q "Liberation Serif	28	1	0	0	0	C9211E	2" "$T/o.hoj"; then
	ok "libre.ods: anchos (y el de siempre), alto, panel, área, anotación, unidas, estilo de celda"
else ko "libre.ods: lo de la hoja no casa"; grep -a "^2[WDRIPKME]" "$T/o.hoj" | head -12; fi
# ---- 2. uno hecho con un guion ----
cat > "$T/g.txt" <<EOF
pon A1 Concepto
pon B1 Importe
pon C1 Fecha
pon A2 Pan
pon B2 3,5
pon C2 22/11/2026
pon A3 Leche
pon B3 1,25
pon C3 14:30
pon B4 =SUMA(B2:B3)
pon A5 12%
pon B5 =B4*(1+A5)
pon C5 VERDADERO
pon A6 =SI(B4>4;"mucho";"poco")
pon B6 =B2/0
pon A7 dos espacios  y ñ €
estilo A1:C1 negrita fondo=FFFF00 trama=1 borde=1 alin=centro tam=14
estilo B2:B5 color=C00000 letra=DejaVu_Serif cursiva subrayado
fmt B2:B5 #,##0.00 "€"
estilo A6 ajustar valin=arriba bab=4 tachado
ancho A 150
ancho C 120
alto 3 40
combina A9:C9
pon A9 Título combinado
comenta B2 El precio, con IVA
inmoviliza A2
condicional B2:B4 mayor 2 simbolo=1 negrita fondo=FFC0C0
grafico columnas A1:B3 titulo=Ventas
nombre Total =Hoja1!\$B\$4
pon D1 =Total*2
area A1:C9
hoja Datos
pon A1 =Hoja1!B4+1
pon A2 texto con "comillas"; y punto y coma
pon A3 =CONCATENAR(A2;"!")
pon B1 1E+20
pon B2 0,1
pon B3 =B2*3
guarda $T/g.hoj
guarda $T/g.xlsx
guarda $T/g.ods
guarda $T/g.csv
EOF
$C --guion "$T/g.txt" > "$T/g.sal" 2>&1
if ! san "$T/g.sal" && grep -a -q "== 4 bien, 0 mal" "$T/g.sal" && [ -s "$T/g.hoj" ] && [ -s "$T/g.xlsx" ] && [ -s "$T/g.ods" ] && [ -s "$T/G.CSV" -o -s "$T/g.csv" ]; then
	ok "un libro hecho con el guion, guardado en .HOJ, .XLSX, .ODS y .CSV"
else ko "el guion: $(tail -3 "$T/g.sal" | tr '\n' ' ')"; fi
# ---- 3. ida y vuelta ----
quita_g() { sed -e 's/, [0-9]* gráficos//; s/, [0-9]* condicionales//; s/, [0-9]* estilos//; s/^cuentas: [^:]*: [^;]*; //'; }
for base in g.hoj hoj386.hoj excel.xlsx libre.ods; do
	$C --valores "$T/$base" > "$T/a.txt" 2>&1
	ia=$($C --info "$T/$base" 2>&1 | quita_g)
	for f in hoj xlsx ods; do
		rm -f "$T/r.$f" "$T/r_$f.hoj"
		$C --convierte "$T/$base" "$T/r.$f" > "$T/x.txt" 2>&1 && $C --convierte "$T/r.$f" "$T/r_$f.hoj" >> "$T/x.txt" 2>&1
		$C --valores "$T/r_$f.hoj" > "$T/b.txt" 2>&1
		ib=$($C --info "$T/r_$f.hoj" 2>&1 | quita_g)
		if ! san "$T/x.txt" && ! san "$T/b.txt" && cmp -s "$T/a.txt" "$T/b.txt" && [ "$ia" = "$ib" ]; then
			ok "$base -> .$f -> .HOJ: lo mismo en cada celda y lo mismo dentro ($ib)"
		else ko "$base -> .$f -> .HOJ: «$ia» / «$ib»"; diff "$T/a.txt" "$T/b.txt" | head -6; grep -a "runtime error\|Sanitizer\|cuentas:" "$T/x.txt" | head -3; fi
		# los anchos, los altos, las combinadas, los nombres y los comentarios
		if [ "$f" != hoj ] && grep -a -q "^2H" "$T/$base" 2> /dev/null; then
			if diff <(grep -a "^2[WRMNKIYZ]" "$T/$base" | sort) <(grep -a "^2[WRMNKIYZ]" "$T/r_$f.hoj" | sort) > "$T/d.txt"; then
				ok "$base -> .$f -> .HOJ: los mismos anchos, altos, combinadas, nombres, comentarios y paneles"
			else ko "$base -> .$f: $(head -4 "$T/d.txt" | tr '\n' ' ')"; fi
		fi
	done
	if [ "${base##*.}" = hoj ]; then
		$C --convierte "$T/$base" "$T/otra.hoj" > /dev/null 2>&1 && $C --convierte "$T/otra.hoj" "$T/otra2.hoj" > /dev/null 2>&1
		if cmp -s "$T/otra.hoj" "$T/otra2.hoj"; then ok "$base guardado otra vez como .HOJ: octeto a octeto igual"; else ko "$base: el .HOJ cambia al guardarlo otra vez"; fi
	fi
done
# los estilos, de ida y vuelta por XLSX (todo lo de cada estilo, celda a celda)
estilos_de() { python3 -I - "$1" <<'PY'
import sys
L = open(sys.argv[1], 'rb').read().decode('utf-8', 'replace').split('\r\n')
hoja, est, out = None, {}, []
for l in L:
    if l.startswith('S '): hoja, est = l[2:], {}
    c = l.split('\t')
    if c[0] == '2E': est[c[1]] = '\t'.join(c[2:])
    elif c[0] == '2C': out.append('%s!%s %s' % (hoja, c[1], est.get(c[2], '?')))
print('\n'.join(sorted(out)))
PY
}
estilos_de "$T/g.hoj" > "$T/e0.txt"
$C --convierte "$T/g.hoj" "$T/s.xlsx" > /dev/null 2>&1 && $C --convierte "$T/s.xlsx" "$T/s.hoj" > /dev/null 2>&1
estilos_de "$T/s.hoj" > "$T/e1.txt"
if cmp -s "$T/e0.txt" "$T/e1.txt"; then ok "g.hoj -> .XLSX -> .HOJ: cada celda con el mismo estilo (letra, color, relleno, bordes, alineación, formato)"
else ko "los estilos por XLSX cambian:"; diff "$T/e0.txt" "$T/e1.txt" | head -6; fi
$C --convierte "$T/g.hoj" "$T/s.ods" > /dev/null 2>&1 && $C --convierte "$T/s.ods" "$T/s2.hoj" > /dev/null 2>&1
estilos_de "$T/s2.hoj" > "$T/e2.txt"
if cmp -s "$T/e0.txt" "$T/e2.txt"; then ok "g.hoj -> .ODS -> .HOJ: cada celda con el mismo estilo"
else ko "los estilos por ODS cambian:"; diff "$T/e0.txt" "$T/e2.txt" | head -6; fi
# el CSV: ida y vuelta de lo que se ve (una hoja)
$C --convierte "$T/pc.csv" "$T/pc2.csv" > /dev/null 2>&1 && $C --convierte "$T/pc2.csv" "$T/pc3.csv" > /dev/null 2>&1
P2=$T/pc2.csv; [ -f "$P2" ] || P2=$T/PC2.CSV
if [ -f "$P2" ] && cmp -s "$P2" "$T/pc3.csv" && $C --valores "$P2" 2>&1 | grep -q "A2=Pan; integral"; then ok "un .CSV de ida y vuelta: el mismo"
else ko "el .CSV de ida y vuelta cambia"; fi
# ---- 4. lo que lee Cuentas de 386 de un .HOJ de Cuentas 2 ----
grep -a -v "^2" "$T/g.hoj" > "$T/solo386.hoj"
$C --valores "$T/solo386.hoj" > "$T/s386.txt" 2>&1
if head -1 "$T/g.hoj" | grep -q "^HOJA 1" && grep -a -q "^S Hoja1" "$T/g.hoj" && grep -a -q "^C B4 [0-9]* =SUMA(B2:B3)" "$T/g.hoj" &&
   grep -a -q "^A A1 150" "$T/g.hoj" && grep -a -q "^G 0 A1:B3 0" "$T/g.hoj" &&
   python3 -I "$AQUI/compara.py" "$T/s386.txt" /dev/stdin <<'EOF'
[Hoja1]
A1=Concepto
B4=4,75 €
A6=mucho
C2=22/11/2026
A9=Título combinado
[Datos]
A1=5,75
EOF
then ok "un .HOJ de Cuentas 2 visto por Cuentas de 386 (sin las líneas «2x»): lo esencial"
else ko "lo de 386 de un .HOJ de Cuentas 2"; head -5 "$T/s386.txt"; fi
# ---- 5. ZIP y XML bien hechos ----
if python3 -I - "$T/g.xlsx" "$T/g.ods" "$T/r.xlsx" "$T/r.ods" > "$T/zip.txt" 2>&1 <<'PY'
import sys, zipfile, xml.dom.minidom, posixpath
for f in sys.argv[1:]:
    z = zipfile.ZipFile(f)
    assert z.testzip() is None, f
    nombres = set(z.namelist())
    for n in nombres:
        if n.endswith(('.xml', '.rels', '.vml')):
            xml.dom.minidom.parseString(z.read(n))
    if f.endswith('.ods'):
        i = z.infolist()[0]
        assert i.filename == 'mimetype' and i.compress_type == 0 and z.read('mimetype') == b'application/vnd.oasis.opendocument.spreadsheet', f
        assert 'content.xml' in nombres and 'META-INF/manifest.xml' in nombres
    else:
        assert '[Content_Types].xml' in nombres and 'xl/workbook.xml' in nombres
        for n in nombres:
            if n.endswith('.rels'):
                base = posixpath.dirname(posixpath.dirname(n))
                d = xml.dom.minidom.parseString(z.read(n))
                for r in d.getElementsByTagName('Relationship'):
                    t = posixpath.normpath(posixpath.join(base, r.getAttribute('Target')))
                    assert t in nombres, (f, n, t)
print('bien')
PY
then ok "lo escrito (.XLSX y .ODS) es un ZIP y un XML bien hechos, con sus partes y relaciones"
else ko "ZIP o XML mal: $(tail -2 "$T/zip.txt" | tr '\n' ' ')"; fi
if command -v unzip > /dev/null; then
	if unzip -tq "$T/g.xlsx" > /dev/null 2>&1 && unzip -tq "$T/g.ods" > /dev/null 2>&1; then ok "unzip -t: sin errores"; else ko "unzip -t se queja"; fi
fi
# ---- 6. el fuzz ----
if [ -n "${FUZZ:-}" ]; then
	if python3 -I "$AQUI/fuzz.py" "$C" "$T" "$FUZZ" > "$T/fuzz.txt" 2>&1; then ok "fuzz de XLSX, ODS, HOJ y CSV: $(tail -1 "$T/fuzz.txt")"
	else ko "el fuzz: $(tail -3 "$T/fuzz.txt" | tr '\n' ' ')"; fi
fi
echo "== $nok bien, $fail mal =="
exit $((fail > 0))
