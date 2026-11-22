#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/mirador/paquete.sh SALIDA
# El paquete «mirador», el Mirador (tools/hpq.py, OPCIONALES; de tipo
# «multimedia»), para el actualizador de Hispa-Pentium: hace el Mirador
# (hacer.sh) y deja en SALIDA MIRADOR.HPQ y su línea del catálogo,
# CATALOGO.LIN. Con HPQ=otro/hpq.py, ese (el de la 0.8.5, que pone el tipo
# en la cabecera con --tipo). Necesita base >= 0.8.6 (la llamada 172, el tapiz con imagen).
set -e
AQUI=$(dirname "$(readlink -f "$0")")
RAIZ=$(readlink -f "$AQUI/../../..")
SAL=$(mkdir -p "${1:?uso: paquete.sh SALIDA}" && readlink -f "$1")
HPQ=${HPQ:-$RAIZ/tools/hpq.py}
W=$(mktemp -d)
trap 'rm -rf $W' EXIT
mkdir -p $W/c/BIN
bash $AQUI/hacer.sh $W/c/BIN
iconv -f UTF-8 -t CP437 $AQUI/LEEME.txt | sed 's/$/\r/' > $W/c/MIRADOR.TXT
TIPO=()
grep -q -- "--tipo" "$HPQ" && TIPO=(--tipo multimedia)
python3 -I "$HPQ" crea "$SAL/MIRADOR.HPQ" --nombre mirador --version 1.0.1 \
  --descripcion "El Mirador: ver imágenes (BMP, PNG, JPG, GIF animados, ICO...), girar, diapositivas, pantalla completa y fondo de escritorio" \
  --necesita "base >= 0.8.6" "${TIPO[@]}" \
  $W/c/BIN/MIRADOR.BIN='\HISPA\MIRADOR.BIN' $W/c/MIRADOR.TXT='\HISPA\MIRADOR.TXT' \
  $W/c/MIRADOR.MNU='\HISPA\MENU\MIRADOR.MNU' $W/c/MIRADOR.ICP='\HISPA\MENU\MIRADOR.ICP' > /dev/null
python3 -I "$HPQ" linea "$SAL/MIRADOR.HPQ" > "$SAL/CATALOGO.LIN"
echo "paquete.sh: $SAL/MIRADOR.HPQ ($(stat -c %s "$SAL/MIRADOR.HPQ") octetos) y CATALOGO.LIN"
