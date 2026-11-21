#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/pdf/paquete.sh SALIDA
# El paquete «pdf», Hispa-PDF, para el actualizador de Hispa-Pentium
# (tools/hpq.py, OPCIONALES): hace PDF.BIN (hacer.sh) y la ventana
# HISPAPDF.BIN (uhpdf.cas), su acceso del menú (tipo=Oficina) con su icono,
# y deja en SALIDA PDF.HPQ y su línea del catálogo, CATALOGO.LIN.
set -e
AQUI=$(dirname "$(readlink -f "$0")")
RAIZ=$(readlink -f "$AQUI/../../..")
SAL=$(readlink -f "${1:?uso: paquete.sh SALIDA}")
W=$(mktemp -d)
trap 'rm -rf $W' EXIT
mkdir -p "$SAL" $W/c/PDF $W/c/MENU
bash $AQUI/hacer.sh $W/c/PDF.BIN
python3 -I $AQUI/../../ccastel.py $AQUI/../uhpdf.cas -o $W/uhpdf.asm > /dev/null
nasm -f bin $W/uhpdf.asm -o $W/c/HISPAPDF.BIN
iconv -f UTF-8 -t CP437 $AQUI/LEEME.txt | sed 's/$/\r/' > $W/c/PDF/LEEME.TXT
python3 -I $RAIZ/tools/hpicono.py $AQUI/pdf32.png $W/c/MENU/HISPAPDF.ICP
printf 'nombre=Hispa-PDF\r\nprograma=%s\r\nargumentos=\r\nicono=%s\r\nescritorio=no\r\ntipo=Oficina\r\n' \
  "${PDF_PROGRAMA:-\\HISPA\\HISPAPDF.BIN}" "${PDF_ICONO:-\\HISPA\\MENU\\HISPAPDF.ICP}" > $W/c/MENU/HISPAPDF.MNU
[ -n "${PDF_CARPETA:-}" ] && cp -r $W/c/. "$PDF_CARPETA"/
python3 -I $RAIZ/tools/hpq.py opcionales "$SAL" $W/c pdf > /dev/null
python3 -I $RAIZ/tools/hpq.py linea "$SAL/PDF.HPQ" > "$SAL/CATALOGO.LIN"
echo "paquete.sh: $SAL/PDF.HPQ ($(stat -c %s "$SAL/PDF.HPQ") octetos) y CATALOGO.LIN"
