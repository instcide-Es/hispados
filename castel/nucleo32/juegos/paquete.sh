#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/juegos/paquete.sh SALIDA
# (JUEGOS) El paquete «juegos» (tools/hpq.py, OPCIONALES; de tipo «juegos»),
# para el actualizador de Hispa-Pentium: hace los juegos (hacer.sh) y deja en
# SALIDA JUEGOS.HPQ y su línea del catálogo, CATALOGO.LIN. Con HPQ=otro/hpq.py,
# ese (el de la 0.8.5 pone el tipo en la cabecera con --tipo). Necesita base
# >= 0.8.4 (las llamadas 181 y 182: las teclas y la pantalla completa); el
# sonido (190-192), si lo hay.
set -e
AQUI=$(dirname "$(readlink -f "$0")")
RAIZ=$(readlink -f "$AQUI/../../..")
SAL=$(mkdir -p "${1:?uso: paquete.sh SALIDA}" && readlink -f "$1")
HPQ=${HPQ:-$RAIZ/tools/hpq.py}
W=$(mktemp -d)
trap 'rm -rf $W' EXIT
mkdir -p $W/c/BIN
bash $AQUI/hacer.sh $W/c/BIN
iconv -f UTF-8 -t CP437 $AQUI/LEEME.txt | sed 's/$/\r/' > $W/c/JUEGOS.TXT
if grep -q "'juegos', '1.0'" "$HPQ"; then
  # (lo que lleva, como dice OPCIONALES de hpq.py)
  python3 -I "$HPQ" opcionales "$SAL" $W/c juegos > /dev/null
else
  TIPO=()
  grep -q -- "--tipo" "$HPQ" && TIPO=(--tipo juegos)
  ARCH=()
  for j in BLOQUES ROMPEMUR PINBALL CULEBRA MINAS SOLITARI TRAGABOL ALMACEN; do
    ARCH+=("$W/c/BIN/$j.BIN=\\HISPA\\JUEGOS\\$j.BIN" "$W/c/$j.MNU=\\HISPA\\MENU\\$j.MNU" "$W/c/$j.ICP=\\HISPA\\MENU\\$j.ICP")
  done
  python3 -I "$HPQ" crea "$SAL/JUEGOS.HPQ" --nombre juegos --version 1.0 \
    --descripcion "Los juegos: Hispa-Pinball (La Feria), Bloques, Rompemuros, Culebra, Campo de minas, Solitario, Tragabolas y Almacén" \
    --necesita "base >= 0.8.4" "${TIPO[@]}" "${ARCH[@]}" "$W/c/JUEGOS.TXT=\\HISPA\\JUEGOS\\LEEME.TXT" > /dev/null
fi
python3 -I "$HPQ" linea "$SAL/JUEGOS.HPQ" > "$SAL/CATALOGO.LIN"
echo "paquete.sh: $SAL/JUEGOS.HPQ ($(stat -c %s "$SAL/JUEGOS.HPQ") octetos) y CATALOGO.LIN"
