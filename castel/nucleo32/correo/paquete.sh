#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/correo/paquete.sh SALIDA
# El paquete «correo», el cliente de correo, para el actualizador de
# Hispa-Pentium (tools/hpq.py, OPCIONALES; tipo «internet»): hace CORREO.BIN
# (hacer.sh), su LEEME (CORREO.TXT) y su acceso del menú Inicio con su icono,
# y deja en SALIDA CORREO.HPQ y su línea del catálogo, CATALOGO.LIN.
set -e
AQUI=$(dirname "$(readlink -f "$0")")
RAIZ=$(readlink -f "$AQUI/../../..")
SAL=$(readlink -f "${1:?uso: paquete.sh SALIDA}")
W=$(mktemp -d)
trap 'rm -rf $W' EXIT
mkdir -p "$SAL" $W/c
bash $AQUI/hacer.sh $W/c/CORREO.BIN
iconv -f UTF-8 -t CP437//TRANSLIT $AQUI/LEEME.txt | sed 's/$/\r/' > $W/c/CORREO.TXT
python3 -I $RAIZ/tools/hpicono.py $AQUI/correo32.png $W/c/CORREO.ICP
printf 'nombre=Correo\r\nprograma=%s\r\nargumentos=\r\nicono=%s\r\nescritorio=si\r\ntipo=Internet\r\n' \
  "${CORREO_PROGRAMA:-\\HISPA\\CORREO.BIN}" "${CORREO_ICONO:-\\HISPA\\MENU\\CORREO.ICP}" > $W/c/CORREO.MNU
[ -n "${CORREO_CARPETA:-}" ] && cp -r $W/c/. "$CORREO_CARPETA"/
python3 -I $RAIZ/tools/hpq.py opcionales "$SAL" $W/c correo > /dev/null
python3 -I $RAIZ/tools/hpq.py linea "$SAL/CORREO.HPQ" > "$SAL/CATALOGO.LIN"
echo "paquete.sh: $SAL/CORREO.HPQ ($(stat -c %s "$SAL/CORREO.HPQ") octetos) y CATALOGO.LIN"
