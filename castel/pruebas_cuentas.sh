# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pruebas_cuentas.sh - la parte 44 (SOLO=cuentas, CUENTAS32) de pruebas.sh, que
# lee este archivo («.»): la función parte_cuentas, Cuentas 2 (CUENTAS.BIN, la
# hoja de cálculo de 32 bits en C portable: castel/nucleo32/oficina/cuentas),
# con los ayudantes de pruebas.sh (ok, ko, saca) y sus carpetas ($AQUI, $P, $G).
#   1. en el anfitrión, con ASan y UBSan: la batería de fórmulas (la de
#      pruebas/formulas.txt, a mano, y la de pruebas/haz_formulas.py, con
#      Python), el fuzz del analizador de fórmulas, los formatos (.HOJ de
#      Hispa-386 y de Cuentas 2, .XLSX y .ODS de referencia, ida y vuelta, .CSV,
#      y su fuzz: pruebas/formatos.sh), el PDF (pruebas/pdf.sh: qpdf, pdftotext,
#      pdffonts) y la ventana con un guion de sucesos (la de mentira de
#      comun/so_anfitrion.c): escribir, negrita, combinar y centrar, €, un
#      gráfico, formato condicional, un comentario, guardar en .HOJ, .XLSX, .ODS
#      y .CSV, exportar a PDF, deshacer y cerrar; y lo guardado, mirado desde fuera;
#   2. en QEMU (el escritorio a 800 x 600, la Consola en (60, 186)): CUENTAS,
#      escribir números y fórmulas, negrita, €, un gráfico, guardar (.HOJ y
#      .XLSX), exportar a PDF, el cuadro Formato de celdas, cerrar y volver a
#      abrir los dos; lo que deja en el disco, mirado desde fuera;
#   3. lo que tarda en un Atom (QEMU -cpu n270 -icount shift=1): CUENTAS --mide
#      10000 (recalcular una hoja de 10.000 fórmulas y desplazarse).
# Con CUENTAS_CAPTURAS=carpeta, las capturas (en .png) van también allí (las de
# docs/pentium/cuentas/). FUZZ_CUENTAS=vueltas (300 de ordinario).

parte_cuentas() {
echo "== Cuentas 2 (CUENTAS.BIN: la hoja de cálculo de 32 bits, en C portable) =="
local V=$P/cuentas CU=$AQUI/nucleo32/oficina/cuentas S r t
local HCU=$V/cuentas-anfitrion/cuentas
mkdir -p $V/a $V/q $V/cap $G
export MTOOLS_SKIP_CHECK=1
if ! bash $CU/anfitrion.sh $HCU asan > $V/anf.txt 2>&1 || ! bash $CU/hacer.sh $V/CUENTAS.BIN > $V/hacer.txt 2>&1; then
  ko "no se puede compilar Cuentas 2: $(tail -n 3 $V/anf.txt $V/hacer.txt)"
  return
fi
ok "CUENTAS.BIN se compila (-march=i586, sin CMOV: $(stat -c %s $V/CUENTAS.BIN) octetos) y Cuentas 2 en el anfitrión, con ASan y UBSan"
# ---- 1. en el anfitrión ---- (CUENTAS_SOLO_QEMU=1: sin esta, ni la 3)
if [ -z "${CUENTAS_SOLO_QEMU:-}" ]; then
cu_partes() {     # el nombre, la orden; cuenta sus «bien»/«mal»
  local que=$1 sal=$V/$2.txt; shift 2
  "$@" > $sal 2>&1
  r=$(grep -a "bien, .* mal" $sal | tail -1)
  case "$r" in *" 0 mal"*) if grep -a -q "runtime error\|Sanitizer" $sal; then ko "$que: ASan o UBSan se quejan"; grep -a "runtime error\|Sanitizer" $sal | head -3 | sed 's/^/        /'; else ok "$que: ${r#*== }"; fi ;;
    *) ko "$que: ${r:-sin resumen}"; grep -a -i "^mal:\|FAIL\|runtime error\|Sanitizer" $sal | head -6 | sed 's/^/        /' ;; esac
}
cu_partes "anfitrión: la batería de fórmulas a mano (prioridades, errores, circulares, referencias, nombres, funciones, filas y columnas, deshacer, series, ordenar, pegado especial)" formulas $HCU --guion $CU/pruebas/formulas.txt
python3 -I $CU/pruebas/haz_formulas.py $V/g_python.txt 60 > /dev/null
cu_partes "anfitrión: la batería calculada con Python (dinero, estadística, redondeos, fechas, textos)" fpy $HCU --guion $V/g_python.txt
python3 -I $CU/pruebas/fuzz_formulas.py $V/g_fuzz.txt ${FUZZ_CUENTAS:-300}0 5 > /dev/null
cu_partes "anfitrión: el fuzz del analizador de fórmulas (${FUZZ_CUENTAS:-300}0 fórmulas al azar; lo que se lee, de ida y vuelta)" fuzz $HCU --guion $V/g_fuzz.txt
cu_partes "anfitrión: los formatos (.HOJ de Hispa-386 y de Cuentas 2, .XLSX y .ODS de referencia, ida y vuelta, .CSV, fuzz)" formatos env FUZZ=${FUZZ_CUENTAS:-300} bash $CU/pruebas/formatos.sh $HCU $V/tf
cu_partes "anfitrión: exportar a PDF (qpdf, pdftotext, pdffonts; páginas, área, saltos; las celdas y los gráficos pintados)" pdf bash $CU/pruebas/pdf.sh $HCU $V/tp
# la ventana, con un guion (las posiciones, las de una ventana de 1024 x 700)
rm -f $V/q/prueba.*
sed "s#CARPETA#$V/q#" > $V/q/guion.txt <<'EOF'
escribe Ventas del primer trimestre
tecla ret
escribe Mes
tecla tab
escribe Norte
tecla tab
escribe Sur
tecla tab
escribe Total
tecla ret
escribe Enero
tecla tab
escribe 1250,5
tecla tab
escribe 800
tecla tab
escribe =B3+C3
tecla ret
escribe Febrero
tecla tab
escribe 980
tecla tab
escribe 1100
tecla tab
escribe =B4+C4
tecla ret
escribe Marzo
tecla tab
escribe 1430,25
tecla tab
escribe 950
tecla tab
escribe =B5+C5
tecla ret
escribe Total
tecla tab
escribe =SUMA(B3:B5)
tecla tab
escribe =SUMA(C3:C5)
tecla tab
escribe =SUMA(D3:D5)
tecla ret
escribe 22/11/2026
tecla tab
escribe =PROMEDIO(B3:C5)
tecla ret
tecla ctrl-inicio
tecla mayus-der
tecla mayus-der
tecla mayus-der
clic 244 79
clic 275 13
elige 15
tecla abajo
tecla abajo
tecla der
tecla mayus-abajo
tecla mayus-abajo
tecla mayus-abajo
tecla mayus-der
tecla mayus-der
clic 484 79
tecla ctrl-inicio
tecla abajo
tecla mayus-abajo
tecla mayus-abajo
tecla mayus-abajo
tecla mayus-der
tecla mayus-der
tecla f11
tecla ret
tecla ctrl-inicio
tecla abajo
tecla abajo
tecla der
tecla der
tecla der
tecla mayus-abajo
tecla mayus-abajo
clic 275 13
elige 19
tecla tab
escribe 2300
tecla ret
tecla ctrl-inicio
tecla mayus-abajo
tecla mayus-abajo
tecla mayus-abajo
tecla mayus-abajo
tecla mayus-abajo
tecla mayus-abajo
tecla mayus-der
tecla mayus-der
tecla mayus-der
clic 275 13
elige 8
tecla ctrl-inicio
tecla abajo
tecla abajo
clic 205 13
elige 12
escribe El mejor mes|de las dos tiendas
tecla ret
captura h1
tecla ctrl-1
captura h2
tecla esc
tecla f12
escribe CARPETA/prueba.hoj
tecla ret
tecla f12
escribe CARPETA/prueba.xlsx
tecla ret
tecla f12
escribe CARPETA/prueba.ods
tecla ret
tecla f12
escribe CARPETA/prueba.csv
tecla ret
tecla ret
clic 37 13
elige 6
escribe CARPETA/prueba.pdf
tecla ret
tecla ctrl-fin
escribe esto se deshace
tecla ret
tecla ctrl-z
clic 37 13
elige 11
captura h3
tecla esc
cerrar
tecla tab ret
EOF
OFI_GUION=$V/q/guion.txt OFI_CAPTURAS=$V/cap OFI_VENTANA=1024x700 OFI_83=0 timeout 300 $HCU > $V/ventana.txt 2>&1
S=$V/ventana.txt
if ! grep -q "runtime error\|Sanitizer" $S && grep -q "cuentas: guardado.*prueba.hoj" $S && grep -q "cuentas: guardado.*prueba.xlsx" $S && \
   grep -q "cuentas: guardado.*prueba.ods" $S && grep -q "cuentas: guardado.*prueba.csv" $S && grep -q "cuentas: pdf.*prueba.pdf" $S && \
   grep -q "cuadro «Asistente para gráficos»" $S && grep -q "cuadro «Formato condicional»" $S && grep -q "cuadro «Formato de celdas»" $S && grep -q "cuentas: adiós" $S; then
  ok "anfitrión: la ventana con un guion: escribir, negrita, combinar y centrar, €, gráfico, formato condicional, comentario, autoajustar, los cuatro formatos, PDF, deshacer, vista preliminar y cerrar (sin quejas de ASan)"
else ko "anfitrión: la ventana con un guion:"; grep -a "runtime error\|Sanitizer\|aviso «" $S | head -5 | sed 's/^/        /'; fi
for f in hoj xlsx ods; do
  t=$($HCU --info $V/q/prueba.$f 2>&1)
  x=$($HCU --valores $V/q/prueba.$f 2>&1 | tr '\n' ' ')
  case "$t" in *"7 fórmulas"*"1 combinadas, 1 comentarios"*) ;; *) ko "prueba.$f: «$t»"; continue ;; esac
  case "$x" in *"D6=6.510,75 €"*"A7=22/11/2026"*"B7=1085,125"*) ok "lo guardado en .${f^^}, mirado desde fuera: $(echo $t | sed 's/.*; //') y sus valores" ;; *) ko "prueba.$f: «$x»" ;; esac
done
case "$($HCU --info $V/q/prueba.hoj 2>&1)" in *"1 gráficos"*"1 condicionales"*) ok "el .HOJ lleva también el gráfico y el formato condicional" ;; *) ko "el .HOJ sin gráfico o sin condicional" ;; esac
grep -q "^Total;3.660,75 €;2.850,00 €;6.510,75 €" $V/q/prueba.csv && ok "el .CSV: lo que se ve, con «;»" || ko "el .CSV: $(head -c 200 $V/q/prueba.csv)"
if command -v pdftotext > /dev/null && [ -f $V/q/prueba.pdf ]; then
  t=$(pdftotext $V/q/prueba.pdf - 2> /dev/null | tr '\n' ' ')
  q=$(qpdf --check $V/q/prueba.pdf 2>&1 | grep -c "No syntax or stream encoding errors")
  case "$t" in *"Ventas del primer trimestre"*"6.510,75 €"*) [ "$q" = 1 ] && ok "el PDF exportado: qpdf no ve errores y salen las celdas (con el €)" || ko "el PDF: qpdf se queja" ;;
    *) ko "el PDF exportado da «$t»" ;; esac
else ko "no está prueba.pdf (o falta pdftotext)"; fi
fi
# ---- 2. en QEMU ---- (con la ventana maximizada: lo de dentro empieza en (4, 28))
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $V/semilla.asm > /dev/null && nasm -f bin $V/semilla.asm -o $V/SEMILLA.BIN || { ko "semilla.cas no compila"; return; }
bash $AQUI/nucleo32/disco.sh $V/v.img 32 > $V/disco.txt 2>&1 || { ko "disco.sh: $(tail -n 2 $V/disco.txt)"; return; }
mcopy -i $V/v.img $V/CUENTAS.BIN ::
CAPTURAS=$G ESCRIBE=1 DISCO=$V/v.img ARRANQUE=escritorio MODULOS= MEMORIA=128 $AQUI/nucleo32/probar.sh $V/SEMILLA.BIN ":pausa 3" ":doble 60 186" ":pausa 4" \
  ":escribe CUENTAS" ":tecla ret" ":hasta 180 1 cuentas: lista" ":pausa 2" \
  ":escribe Gastos de QEMU" ":tecla ret" ":escribe Luz" ":tecla tab" ":escribe 45,5" ":tecla ret" \
  ":escribe Agua" ":tecla tab" ":escribe 23,75" ":tecla ret" ":escribe Gas" ":tecla tab" ":escribe 61" ":tecla ret" \
  ":escribe Total" ":tecla tab" ":escribe =SUMA(B2:B4)" ":tecla ret" ":escribe IVA" ":tecla tab" ":escribe =REDONDEAR(B5*21%;2)" ":tecla ret" \
  ":tecla ctrl-home" ":tecla ctrl-n" ":tecla down" ":tecla right" ":tecla shift-down" ":tecla shift-down" ":tecla shift-down" ":tecla shift-down" \
  ":clic 449 107" ":pausa 1" ":tecla ctrl-home" ":tecla down" ":tecla shift-down" ":tecla shift-down" ":tecla shift-right" \
  ":tecla f11" ":pausa 3" ":tecla ret" ":pausa 3" ":captura cu1" \
  ":tecla ctrl-g" ":pausa 3" ":escribe PRUEBA.HOJ" ":tecla ret" ":hasta 60 1 cuentas: guardado" \
  ":tecla f12" ":pausa 3" ":escribe PRUEBA.XLSX" ":tecla ret" ":hasta 60 2 cuentas: guardado" \
  ":clic 604 72" ":pausa 3" ":tecla ret" ":hasta 180 1 cuentas: pdf" ":pausa 2" ":captura cu2" \
  ":tecla alt-f" ":pausa 2" ":tecla down" ":tecla ret" ":pausa 3" ":captura cu3" ":tecla esc" ":pausa 1" \
  ":clic 787 13" ":hasta 30 1 cuentas: adiós" ":pausa 2" ":escribe CUENTAS PRUEBA.HOJ" ":tecla ret" ":hasta 180 1 cuentas: abierto PRUEBA.HOJ" ":pausa 3" ":captura cu4" \
  ":clic 787 13" ":hasta 30 2 cuentas: adiós" ":pausa 2" ":escribe CUENTAS PRUEBA.XLS" ":tecla ret" ":hasta 180 1 cuentas: abierto PRUEBA.XLS" ":pausa 3" ":captura cu5" \
  ":clic 787 13" ":hasta 30 3 cuentas: adiós" > $V/qemu.sal 2>&1
S=$V/qemu.sal
[ -n "${CUENTAS_CAPTURAS:-}" ] && mkdir -p "$CUENTAS_CAPTURAS" && cp $S "$CUENTAS_CAPTURAS/qemu.sal"
mira_cu() { if grep -a -q -- "$2" $S; then ok "$1"; else ko "$1: falta «$2»"; grep -a "cuentas" $S | tail -6 | sed 's/^/        /'; fi; }
mira_cu "en QEMU: Cuentas 2 abre su ventana en el escritorio" "cuentas: lista"
mira_cu "en QEMU: el asistente para gráficos (F11)" "cuadro «Asistente para gráficos»"
mira_cu "en QEMU: guardado como PRUEBA.HOJ" "cuentas: guardado .*PRUEBA.HOJ"
mira_cu "en QEMU: guardado como .XLSX (el núcleo aún da 8+3: PRUEBA.XLS)" "cuentas: guardado .*PRUEBA.XLS"
mira_cu "en QEMU: exportado a PDF" "cuentas: pdf .*PRUEBA.PDF"
mira_cu "en QEMU: el menú Formato (Alt+F) y su primera, el cuadro Formato de celdas" "cuadro «Formato de celdas»"
mira_cu "en QEMU: cerrar y volver a abrir PRUEBA.HOJ" "cuentas: abierto PRUEBA.HOJ"
mira_cu "en QEMU: y PRUEBA.XLS (el .XLSX, por lo de dentro)" "cuentas: abierto PRUEBA.XLS"
if ! grep -a -q "excepción\|\*\*\*" $S; then ok "en QEMU: ni una excepción ni una queja del núcleo"
else ko "el núcleo se ha quejado:"; grep -a "excepción\|\*\*\*" $S | head -4 | sed 's/^/        /'; fi
if [ -x "$HCU" ]; then
  for f in HOJ XLS; do
    if saca $V/v.img PRUEBA.$f $V/a/qemu.$f; then
      t=$($HCU --info $V/a/qemu.$f 2>&1)
      x=$($HCU --valores $V/a/qemu.$f 2>&1 | tr '\n' ' ')
      case "$t$x" in *"2 fórmulas"*"A1=Gastos de QEMU"*"B5=130,25 €"*"B6=27,35"*) ok "PRUEBA.$f (hecho en QEMU), mirado desde fuera: el total con su €, el IVA redondeado$(case "$t" in *"1 gráficos"*) echo " y el gráfico";; esac)" ;;
        *) ko "PRUEBA.$f: «$t» «$x»" ;; esac
    else ko "no está PRUEBA.$f en el disco de QEMU"; fi
  done
  if saca $V/v.img PRUEBA.PDF $V/a/qemu.pdf && command -v pdftotext > /dev/null; then
    t=$(pdftotext $V/a/qemu.pdf - 2> /dev/null | tr '\n' ' ')
    case "$t" in *"Gastos de QEMU"*"130,25 €"*) ok "PRUEBA.PDF (hecho en QEMU): pdftotext saca sus celdas" ;; *) ko "PRUEBA.PDF da «$t»" ;; esac
  else ko "no está PRUEBA.PDF en el disco de QEMU"; fi
fi
if [ -n "${CUENTAS_CAPTURAS:-}" ]; then
  mkdir -p "$CUENTAS_CAPTURAS"
  for c in cu1 cu2 cu3 cu4 cu5; do [ -f $G/$c.ppm ] && convert $G/$c.ppm "$CUENTAS_CAPTURAS/$c.png" 2> /dev/null; done
  for c in h1 h2 h3; do [ -f $V/cap/$c.ppm ] && convert $V/cap/$c.ppm "$CUENTAS_CAPTURAS/$c.png" 2> /dev/null; done
fi
# ---- 3. lo que tarda en un Atom ----
[ -n "${CUENTAS_SOLO_QEMU:-}" ] && return
QEMU_MAS="-cpu n270 -icount shift=1" ESCRIBE=1 DISCO=$V/v.img ARRANQUE=escritorio MODULOS= MEMORIA=256 $AQUI/nucleo32/probar.sh $V/SEMILLA.BIN ":pausa 3" ":doble 60 186" ":pausa 4" \
  ":escribe CUENTAS --mide 10000" ":tecla ret" ":hasta 1200 1 mide: memoria" ":pausa 3" > $V/mide.sal 2>&1
[ -n "${CUENTAS_CAPTURAS:-}" ] && cp $V/mide.sal "$CUENTAS_CAPTURAS/mide.sal"
t=$(grep -a "recalcular todo" $V/mide.sal | head -1)
ms=$(echo "$t" | sed -n 's/.* en \([0-9]*\) ms.*/\1/p')
av=$(grep -a "Av Pág en" $V/mide.sal | sed -n 's/.*: \([0-9]*\) ms cada una.*/\1/p' | head -1)
if [ -n "$ms" ] && [ "$ms" -le 3000 ] && [ -n "$av" ] && [ "$av" -le 400 ]; then
  ok "en un Atom (QEMU -cpu n270 -icount): recalcular 10.000 fórmulas, $ms ms; Av Pág, $av ms cada una ($(grep -a "cambiar A1" $V/mide.sal | sed 's/.*: //' | head -1))"
else ko "en un Atom: «$t» «Av Pág: $av ms»"; fi
}
