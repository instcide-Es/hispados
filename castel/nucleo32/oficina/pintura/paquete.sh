#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# oficina/pintura/paquete.sh SALIDA
# El paquete «pintura» 2.0, Pintura 2, para el actualizador de Hispa-Pentium
# (tools/hpq.py, OPCIONALES; tipo multimedia): hace PINTURA.BIN (hacer.sh),
# su acceso del menú (tipo=Multimedia) con su icono y el LEEME, y deja en
# SALIDA PINTURA.HPQ y su línea del catálogo, CATALOGO.LIN.
set -e
AQUI=$(dirname "$(readlink -f "$0")")
RAIZ=$(readlink -f "$AQUI/../../../..")
SAL=$(readlink -f "${1:?uso: paquete.sh SALIDA}")
W=$(mktemp -d)
trap 'rm -rf $W' EXIT
mkdir -p "$SAL" $W/c/PINTURA $W/c/MENU
bash $AQUI/hacer.sh $W/c/PINTURA.BIN
iconv -f UTF-8 -t CP437 $AQUI/LEEME.txt | sed 's/$/\r/' > $W/c/PINTURA/LEEME.TXT
python3 -I $RAIZ/tools/hpicono.py $AQUI/pintura32.png $W/c/MENU/PINTURA.ICP
printf 'nombre=Pintura\r\nprograma=%s\r\nargumentos=\r\nicono=%s\r\nescritorio=si\r\ntipo=Multimedia\r\n' \
  "${PINTURA_PROGRAMA:-\\HISPA\\PINTURA.BIN}" "${PINTURA_ICONO:-\\HISPA\\MENU\\PINTURA.ICP}" > $W/c/MENU/PINTURA.MNU
[ -n "${PINTURA_CARPETA:-}" ] && cp -r $W/c/. "$PINTURA_CARPETA"/
python3 -I $RAIZ/tools/hpq.py opcionales "$SAL" $W/c pintura > /dev/null
python3 -I $RAIZ/tools/hpq.py linea "$SAL/PINTURA.HPQ" > "$SAL/CATALOGO.LIN"
echo "paquete.sh: $SAL/PINTURA.HPQ ($(stat -c %s "$SAL/PINTURA.HPQ") octetos) y CATALOGO.LIN"
