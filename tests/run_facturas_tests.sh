#!/bin/bash
# Prueba de Facturas (FACTURAS.HVP), del grupo «Productividad».
#
# Abre Hispa Ventanas con Facturas (clave «Reabrir=» de CENTRO.INI) y:
#  - rellena «Mis datos» y hace una factura emitida con tres líneas (una con
#    cantidad decimal que redondea al medio céntimo y un descuento negativo);
#    comprueba los importes, la base, el IVA y el total;
#  - la guarda como texto y mira el archivo;
#  - apunta una factura recibida (un gasto), la marca pagada y mira la lista
#    y el resumen del año;
#  - arranca otra vez: los datos siguen ahí (FACTURAS.DAT); imprime la factura
#    y mira lo que ha salido por la impresora (LPT1 a un archivo).
#
# Necesita qemu-system-i386, mtools, socat e ImageMagick (como las demás).
set -u
IMG=${1:-build/hispados.img}
. tests/tools/hvp_arnes.sh
HVP=$B/FACTURAS.HVP
[ -f $HVP ] || NASM=${NASM:-nasm} python3 tests/../tools/mkhvp.py programas/facturas.asm $HVP > /dev/null

echo "== Facturas: una emitida, una recibida, texto y resumen =="
prepara $HVP
{ arranca
  shot v0
  clic 590 96; sleep 1                  # «Mis datos»
  escribe "Taller Rescoldo"; tecla tab; escribe "12345678Z"; tecla tab
  escribe "Calle Mayor 1"; tecla tab; escribe "43700 Albinyana"; tecla tab
  escribe "Pago por transferencia"; tecla ret; sleep 1.2
  tecla insert; sleep 1.2               # nueva: número y fecha, ya puestos
  tecla tab tab; escribe "Ana Soler"; tecla tab; escribe "11111111H"; tecla tab
  escribe "Plaza 2"; tecla ret; sleep 2     # sin importe: pide sola la primera línea
  shot l0
  escribe "Reparar portatil"; tecla tab; escribe "1,5"; tecla tab
  escribe "33,33"; tecla ret; sleep 1.2
  tecla insert; sleep 1; escribe "Disco duro"; tecla tab tab; escribe "49,90"; tecla ret; sleep 1.2
  tecla insert; sleep 1; escribe "Descuento"; tecla tab tab; escribe "-10"; tecla ret; sleep 1.2
  aparca; shot f1
  clic 554 96; sleep 1.2; tecla ret; sleep 2; shot f2; tecla ret; sleep 1   # «Texto»
  tecla esc; sleep 1
  clic 406 96; sleep 1                  # «Recibidas»
  # la recibida, con su importe en el mismo diálogo y el IVA incluido (lo
  # normal en un recibo): 65,70 que se pagan = 54,30 + 11,40 de IVA
  tecla insert; sleep 1.2; escribe "A77"; tecla tab tab; escribe "Luz y Fuerza"
  tecla tab tab tab tab tab tab tab; escribe "Luz noviembre"; tecla tab; escribe "65,70"; shot r0; tecla ret; sleep 1.5
  tecla p; sleep 0.8
  aparca; shot r1
  tecla esc; sleep 1; shot r2
  clic 510 96; sleep 1.2; shot s1; tecla ret; sleep 1   # «Resumen»
  sal; } | run 240 -vga std

chk $T/v0.ppm titulo 50 36 activa "Facturas"
chk $T/v0.ppm busca 56 120 400 160 1 8 "No hay facturas emitidas."
# la factura, como en el papel (tinta sobre blanco)
chk $T/f1.ppm busca 60 138 400 160 0 9 "12345678Z"
chk $T/f1.ppm busca 400 135 740 160 0 9 "2026-001"
chk $T/f1.ppm busca 100 195 400 220 0 9 "Ana Soler"
chk $T/f1.ppm busca 400 265 740 285 0 9 "50,00"       # 1,5 x 33,33 = 49,995
chk $T/f1.ppm busca 400 281 740 301 0 9 "49,90"
chk $T/f1.ppm busca 400 297 740 317 0 6 "-10,00"      # (la línea elegida)
chk $T/f1.ppm busca 400 318 740 345 0 9 "89,90"       # la base
chk $T/f1.ppm busca 400 336 740 363 0 9 "IVA 21 %:"
chk $T/f1.ppm busca 400 336 740 363 0 9 "18,88"       # 18,879
chk $T/f1.ppm busca 60 380 400 410 0 9 "Pago por transferencia"
chk $T/l0.ppm titulo 180 205 activa "Línea de la factura"
chk $T/r0.ppm busca 190 150 640 480 0 8 "Los precios llevan el IVA incluido"
chk $T/r0.ppm busca 190 150 640 480 0 8 "Importe:"
# la recibida: pagada
chk $T/r1.ppm busca 400 140 740 160 0 9 "A77"
chk $T/r1.ppm busca 100 195 400 220 0 9 "Taller Rescoldo"  # el cliente soy yo
chk $T/r1.ppm busca 400 170 740 195 0 9 "Estado: Pagada"
chk $T/r1.ppm busca 400 280 740 310 0 9 "54,30"       # la base: 65,70 / 1,21
chk $T/r1.ppm busca 400 300 740 330 0 9 "11,40"       # y el IVA, lo que falta
chk $T/r1.ppm busca 60 280 400 310 1 9 "(precios con el IVA incluido)"
# la lista de recibidas y el resumen de abajo
chk $T/r2.ppm busca 52 130 400 152 8 2 "A77"
chk $T/r2.ppm busca 400 130 748 152 8 2 "65,70"
chk $T/r2.ppm busca 52 530 748 552 0 6 "1 factura    Total: 65,70    Pendiente: 0,00"
chk $T/r2.ppm busca 52 548 748 572 0 6 "ingresos 108,78    gastos 65,70    saldo 43,08"
chk $T/s1.ppm busca 90 300 720 330 0 8 "89,90"       # 4º trimestre: ingresos
chk $T/s1.ppm busca 90 300 720 330 0 8 "7,48"        # IVA repercutido - soportado
muestra v0 facturas_vacia; muestra f1 facturas_emitida; muestra f2 facturas_texto
muestra l0 facturas_linea; muestra r0 facturas_datos; muestra r1 facturas_recibida; muestra r2 facturas_lista; muestra s1 facturas_resumen

# el archivo de texto
mtype -i $T/f.img ::2026001.TXT 2>/dev/null | tr -d '\r' | iconv -f CP437 -t UTF-8 > $T/fac.txt
ct() { if grep -qE -- "$1" $T/fac.txt; then ok "texto: $1"; else ko "texto: falta «$1»"; fi; }
ct '^Taller Rescoldo +FACTURA$'
ct 'Número: 2026-001$'
ct '^Cliente: Ana Soler$'
ct '^Reparar portatil +1,5 +33,33 +50,00$'
ct '^Descuento +1 +-10,00 +-10,00$'
ct 'Base imponible: +89,90$'
ct 'IVA 21 %: +18,88$'
ct 'TOTAL: +108,78$'
ct '^Pago por transferencia$'
if mdir -i $T/f.img ::HISPADOS/PROGS/FACTURAS.DAT > /dev/null 2>&1; then ok "FACTURAS.DAT guardado"; else ko "falta FACTURAS.DAT"; fi

echo "== Facturas: los datos siguen al volver a arrancar; imprimir =="
printf '[Centro]\r\nReabrir=A:\\HISPADOS\\PROGS\\FACTURAS.HVP\r\n' > $T/CENTRO.INI
mcopy -o -i $T/f.img $T/CENTRO.INI ::HISPADOS/CENTRO.INI
rm -f $T/prn.txt
{ arranca
  shot p0
  tecla ret; sleep 1.2
  clic 630 96; sleep 3; shot p1; tecla ret; sleep 1     # «Imprimir»
  sal; } | PARALELO=file:$T/prn.txt run 120 -vga std
chk $T/p1.ppm busca 216 270 586 310 0 8 "La factura se ha enviado a la impresora."
chk $T/p0.ppm busca 52 130 400 152 8 2 "2026-001"
chk $T/p0.ppm busca 400 130 748 152 8 2 "108,78"
muestra p1 facturas_impresa
if [ -s $T/prn.txt ] && tr -d '\r' < $T/prn.txt | iconv -f CP437 -t UTF-8 | grep -qE 'TOTAL: +108,78$'; then
  ok "impresora: sale la factura con su total"
else
  ko "impresora: no ha salido la factura"
fi
if tail -c 1 $T/prn.txt 2>/dev/null | od -An -tu1 | grep -qw 12; then ok "impresora: salto de página al final"; else ko "impresora: falta el salto de página"; fi

echo "== Facturas: sin impresora, lo dice =="
mcopy -o -i $T/f.img $T/CENTRO.INI ::HISPADOS/CENTRO.INI
{ arranca
  tecla ret; sleep 1.2
  clic 630 96; sleep 3; shot n1; tecla ret; sleep 1
  sal; } | run 120 -vga std
chk $T/n1.ppm busca 150 250 650 300 0 8 "No se ha podido imprimir."
muestra n1 facturas_sin_impresora

if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/fac.txt; fi
rm -rf $T
exit $fail
