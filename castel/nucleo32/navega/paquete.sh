#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/navega/paquete.sh SALIDA
# El paquete «navega», el Navegador, para el actualizador de Hispa-Pentium
# (tools/hpq.py, OPCIONALES; «navegador» no cabe: los nombres de paquete son
# de 8 letras, como su registro .PQI): hace el Navegador (hacer.sh) y deja en SALIDA
# NAVEGA.HPQ y su línea del catálogo, CATALOGO.LIN (para añadirla a
# CATALOGO.TXT y firmarlo otra vez; nucleo32/actualiza.sh lo hace solo).
# Necesita base >= 0.8.3 (el núcleo con la rueda, el x87 de cada programa y
# programas de hasta 8 MB), red y tls.
set -e
AQUI=$(dirname "$(readlink -f "$0")")
SAL=$(readlink -f "${1:?uso: paquete.sh SALIDA}")
W=$(mktemp -d)
trap 'rm -rf $W' EXIT
mkdir -p "$SAL" $W/c/NAVEGA
bash $AQUI/hacer.sh $W/c/NAVEGA
python3 -I $AQUI/../../../tools/hpq.py opcionales "$SAL" $W/c > /dev/null
python3 -I $AQUI/../../../tools/hpq.py linea "$SAL/NAVEGA.HPQ" > "$SAL/CATALOGO.LIN"
echo "paquete.sh: $SAL/NAVEGA.HPQ ($(stat -c %s "$SAL/NAVEGA.HPQ") octetos) y CATALOGO.LIN"
