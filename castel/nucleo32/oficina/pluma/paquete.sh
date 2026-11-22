#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# oficina/pluma/paquete.sh SALIDA
# El paquete «pluma» 2.0, Pluma 2, para el actualizador de Hispa-Pentium
# (tools/hpq.py, OPCIONALES; tipo oficina): hace PLUMA.BIN (hacer.sh), su
# acceso del menú (tipo=Oficina) con su icono, el LEEME y el diccionario, y
# deja en SALIDA PLUMA.HPQ y su línea del catálogo, CATALOGO.LIN.
set -e
AQUI=$(dirname "$(readlink -f "$0")")
RAIZ=$(readlink -f "$AQUI/../../../..")
SAL=$(readlink -f "${1:?uso: paquete.sh SALIDA}")
W=$(mktemp -d)
trap 'rm -rf $W' EXIT
mkdir -p "$SAL" $W/c/PLUMA $W/c/MENU
bash $AQUI/hacer.sh $W/c/PLUMA.BIN
cp $RAIZ/programas/comun/DICCIO.DAT $W/c/PLUMA/DICCIO.DAT
iconv -f UTF-8 -t CP437 $AQUI/LEEME.txt | sed 's/$/\r/' > $W/c/PLUMA/LEEME.TXT
python3 -I $RAIZ/tools/hpicono.py $AQUI/pluma32.png $W/c/MENU/PLUMA.ICP
printf 'nombre=Pluma\r\nprograma=%s\r\nargumentos=\r\nicono=%s\r\nescritorio=si\r\ntipo=Oficina\r\n' \
  "${PLUMA_PROGRAMA:-\\HISPA\\PLUMA.BIN}" "${PLUMA_ICONO:-\\HISPA\\MENU\\PLUMA.ICP}" > $W/c/MENU/PLUMA.MNU
[ -n "${PLUMA_CARPETA:-}" ] && cp -r $W/c/. "$PLUMA_CARPETA"/
python3 -I $RAIZ/tools/hpq.py opcionales "$SAL" $W/c pluma > /dev/null
python3 -I $RAIZ/tools/hpq.py linea "$SAL/PLUMA.HPQ" > "$SAL/CATALOGO.LIN"
echo "paquete.sh: $SAL/PLUMA.HPQ ($(stat -c %s "$SAL/PLUMA.HPQ") octetos) y CATALOGO.LIN"
