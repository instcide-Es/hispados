#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/reproductor/paquete.sh SALIDA
# El paquete «reproduc», el Reproductor (tools/hpq.py, OPCIONALES; de tipo
# «multimedia»), para el actualizador de Hispa-Pentium: hace REPRODUC.BIN
# (hacer.sh), su acceso del menú Inicio (REPRODUC.MNU, con su icono
# REPRODUC.ICP de reproduc32.png) y su LEEME, y deja en SALIDA REPRODUC.HPQ y
# su línea del catálogo, CATALOGO.LIN. Pide la base 0.8.6 (el sonido, las
# llamadas 190 a 193); con el núcleo de la rama «reproductor» (las llamadas
# 134 a 136 y el sello de REPRODUC.BIN) tiene además el CD de música, el
# mando de la bandeja, soltar desde Mi equipo y lo guardado en \HISPA\REPRODUC.
set -e
AQUI=$(dirname "$(readlink -f "$0")")
RAIZ=$(readlink -f "$AQUI/../../..")
SAL=$(mkdir -p "${1:?uso: paquete.sh SALIDA}" && readlink -f "$1")
HPQ=${HPQ:-$RAIZ/tools/hpq.py}
W=$(mktemp -d)
trap 'rm -rf $W' EXIT
mkdir -p $W/c/BIN
bash $AQUI/hacer.sh $W/c/BIN/REPRODUC.BIN > /dev/null
iconv -f UTF-8 -t CP437 $AQUI/LEEME.txt | sed 's/$/\r/' > $W/c/REPRODUC.TXT
python3 -I $RAIZ/tools/hpicono.py $AQUI/reproduc32.png $W/c/REPRODUC.ICP
printf 'nombre=Reproductor\r\nprograma=%s\r\nargumentos=\r\nicono=%s\r\nescritorio=si\r\ntipo=Multimedia\r\n' \
  "${REPRODUC_PROGRAMA:-\\HISPA\\REPRODUC.BIN}" "${REPRODUC_ICONO:-\\HISPA\\MENU\\REPRODUC.ICP}" > $W/c/REPRODUC.MNU
[ -n "${REPRODUC_CARPETA:-}" ] && cp -r $W/c/. "$REPRODUC_CARPETA"/
python3 -I "$HPQ" opcionales "$SAL" $W/c reproduc > /dev/null
python3 -I "$HPQ" linea "$SAL/REPRODUC.HPQ" > "$SAL/CATALOGO.LIN"
echo "paquete.sh: $SAL/REPRODUC.HPQ ($(stat -c %s "$SAL/REPRODUC.HPQ") octetos) y CATALOGO.LIN"
