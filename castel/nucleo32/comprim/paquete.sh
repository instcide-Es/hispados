#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/comprim/paquete.sh SALIDA
# El paquete «comprim», Comprimidos (tools/hpq.py, OPCIONALES; de tipo
# «herramientas»), para el actualizador de Hispa-Pentium: hace Comprimidos
# (hacer.sh) y deja en SALIDA COMPRIM.HPQ y su línea del catálogo,
# CATALOGO.LIN. Con HPQ=otro/hpq.py, ese (el de la 0.8.5, que pone el tipo
# en la cabecera con --tipo). Necesita base >= 0.8.4.
set -e
AQUI=$(dirname "$(readlink -f "$0")")
RAIZ=$(readlink -f "$AQUI/../../..")
SAL=$(mkdir -p "${1:?uso: paquete.sh SALIDA}" && readlink -f "$1")
HPQ=${HPQ:-$RAIZ/tools/hpq.py}
W=$(mktemp -d)
trap 'rm -rf $W' EXIT
mkdir -p $W/c/BIN
bash $AQUI/hacer.sh $W/c/BIN
iconv -f UTF-8 -t CP437 $AQUI/LEEME.txt | sed 's/$/\r/' > $W/c/COMPRIM.TXT
TIPO=()
grep -q -- "--tipo" "$HPQ" && TIPO=(--tipo herramientas)
python3 -I "$HPQ" crea "$SAL/COMPRIM.HPQ" --nombre comprim --version 1.0.1 \
  --descripcion "Comprimidos: los .ZIP como una carpeta (sacar, añadir y quitar, con contraseña) y las órdenes ZIP y UNZIP" \
  --necesita "base >= 0.8.4" "${TIPO[@]}" \
  $W/c/BIN/COMPRIM.BIN='\HISPA\COMPRIM.BIN' $W/c/BIN/ZIP.BIN='\HISPA\ZIP.BIN' $W/c/BIN/UNZIP.BIN='\HISPA\UNZIP.BIN' \
  $W/c/COMPRIM.TXT='\HISPA\COMPRIM.TXT' $W/c/COMPRIM.MNU='\HISPA\MENU\COMPRIM.MNU' $W/c/COMPRIM.ICP='\HISPA\MENU\COMPRIM.ICP' > /dev/null
python3 -I "$HPQ" linea "$SAL/COMPRIM.HPQ" > "$SAL/CATALOGO.LIN"
echo "paquete.sh: $SAL/COMPRIM.HPQ ($(stat -c %s "$SAL/COMPRIM.HPQ") octetos) y CATALOGO.LIN"
