#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/navega/hacer.sh CARPETA
# Hace el Navegador de Hispa-Pentium (docs/PENTIUM.md, «El Navegador») y deja
# en CARPETA todo lo que va en \HISPA\NAVEGA: NAVEGA.BIN (NetSurf, QuickJS y
# la biblioteca de C, en un programa del anillo 3: navega.mk), NAVEGA.INI,
# LEEME.TXT, RECURSOS\ (las hojas de estilo de NetSurf, sus textos en
# español, la página de inicio y el diccionario de Brotli) y FUENTES\ (las
# letras DejaVu, recortadas: terceros/dejavu); y, al lado de CARPETA, el acceso
# del menú Inicio (NAVEGA.MNU y su icono, NAVEGA.ICP). Los objetos, en
# build/navega-pentium/ (solo lo que ha cambiado: la primera vez, unos
# minutos). Necesita gcc (con -m32), nasm, ld, objcopy y make. Sin «make -j».
set -e
AQUI=$(dirname "$(readlink -f "$0")")
RAIZ=$(readlink -f "$AQUI/../../..")
SAL=$(readlink -f "${1:?uso: hacer.sh CARPETA}")
cd "$RAIZ"
make -s -f castel/nucleo32/navega/navega.mk build/navega-pentium/NAVEGA.BIN > build/navega-pentium.log 2>&1 || { tail -20 build/navega-pentium.log; exit 1; }
T=$(stat -c %s build/navega-pentium/NAVEGA.BIN)
# (lo que el núcleo deja cargar: MAX_PROGRAMA de usuario.cas)
MAX=$(( $(sed -n 's/^ *constante MAX_PROGRAMA = \(0x[0-9A-Fa-f]*\).*/\1/p' castel/nucleo32/usuario.cas) ))
[ "$T" -le "$MAX" ] || { echo "hacer.sh: NAVEGA.BIN mide $T y el núcleo no carga más de $MAX"; exit 1; }
mkdir -p "$SAL/RECURSOS" "$SAL/FUENTES"
cp build/navega-pentium/NAVEGA.BIN "$SAL/"
for f in terceros/netsurf/netsurf/resources/default.css terceros/netsurf/netsurf/resources/quirks.css \
         terceros/netsurf/netsurf/resources/adblock.css terceros/netsurf/netsurf/resources/internal.css \
         w32/surf/recursos/MESSAGES w32/surf/recursos/BROTLI.DIC; do
  n=$(basename "$f" | tr a-z A-Z)
  cp "$f" "$SAL/RECURSOS/$n"
done
cp $AQUI/recursos/INICIO.HTM "$SAL/RECURSOS/INICIO.HTM"
cp terceros/dejavu/*.TTF "$SAL/FUENTES/"
# los textos: en la página 437 y con los finales de línea de DOS
iconv -f UTF-8 -t CP437 $AQUI/NAVEGA.INI | sed 's/$/\r/' > "$SAL/NAVEGA.INI"
iconv -f UTF-8 -t CP437 $AQUI/LEEME.txt | sed 's/$/\r/' > "$SAL/LEEME.TXT"
# el acceso del menú Inicio y del tapiz (accesos.cas)
python3 -I tools/hpicono.py $AQUI/navega32.png "$SAL/../NAVEGA.ICP"
printf 'nombre=Navegador\r\nprograma=%s\r\nargumentos=\r\nicono=%s\r\nescritorio=si\r\n' \
  "${NAVEGA_PROGRAMA:-\\HISPA\\NAVEGA\\NAVEGA.BIN}" "${NAVEGA_ICONO:-\\HISPA\\MENU\\NAVEGA.ICP}" > "$SAL/../NAVEGA.MNU"
