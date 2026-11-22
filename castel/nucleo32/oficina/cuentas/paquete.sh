#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# oficina/cuentas/paquete.sh SALIDA
# El paquete «cuentas» 2.0, Cuentas 2, para el actualizador de Hispa-Pentium
# (tools/hpq.py, OPCIONALES; tipo oficina): hace CUENTAS.BIN (hacer.sh), su
# acceso del menú (tipo=Oficina) con su icono y el LEEME, y deja en SALIDA
# CUENTAS.HPQ y su línea del catálogo, CATALOGO.LIN. Con CUENTAS_CARPETA=dir,
# deja también lo de dentro del paquete en esa carpeta.
set -e
AQUI=$(dirname "$(readlink -f "$0")")
RAIZ=$(readlink -f "$AQUI/../../../..")
SAL=$(readlink -f "${1:?uso: paquete.sh SALIDA}")
W=$(mktemp -d)
trap 'rm -rf $W' EXIT
mkdir -p "$SAL" $W/c/CUENTAS $W/c/MENU
bash $AQUI/hacer.sh $W/c/CUENTAS.BIN
iconv -f UTF-8 -t CP437 $AQUI/LEEME.txt | sed 's/$/\r/' > $W/c/CUENTAS/LEEME.TXT
python3 -I $RAIZ/tools/hpicono.py $AQUI/cuentas32.png $W/c/MENU/CUENTAS.ICP
printf 'nombre=Cuentas\r\nprograma=%s\r\nargumentos=\r\nicono=%s\r\nescritorio=si\r\ntipo=Oficina\r\n' \
  "${CUENTAS_PROGRAMA:-\\HISPA\\CUENTAS.BIN}" "${CUENTAS_ICONO:-\\HISPA\\MENU\\CUENTAS.ICP}" > $W/c/MENU/CUENTAS.MNU
[ -n "${CUENTAS_CARPETA:-}" ] && cp -r $W/c/. "$CUENTAS_CARPETA"/
python3 -I $RAIZ/tools/hpq.py opcionales "$SAL" $W/c cuentas > /dev/null
python3 -I $RAIZ/tools/hpq.py linea "$SAL/CUENTAS.HPQ" > "$SAL/CATALOGO.LIN"
echo "paquete.sh: $SAL/CUENTAS.HPQ ($(stat -c %s "$SAL/CUENTAS.HPQ") octetos) y CATALOGO.LIN"
