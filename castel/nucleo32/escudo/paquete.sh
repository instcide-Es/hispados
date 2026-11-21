#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/escudo/paquete.sh SALIDA
# El paquete «escudo», el antivirus, para el actualizador de Hispa-Pentium
# (tools/hpq.py, OPCIONALES): hace Escudo (hacer.sh) y deja en SALIDA
# ESCUDO.HPQ y su línea del catálogo, CATALOGO.LIN (para añadirla a
# CATALOGO.TXT y firmarlo otra vez; nucleo32/actualiza.sh lo hace solo).
# Las firmas van aparte, en los paquetes «escbase» y «escdia» (los hace
# tools/escudo_firmas.py con las bases de ClamAV, una vez al día).
# Necesita base >= 0.8.3, red y tls (para traer las firmas).
set -e
AQUI=$(dirname "$(readlink -f "$0")")
SAL=$(readlink -f "${1:?uso: paquete.sh SALIDA}")
W=$(mktemp -d)
trap 'rm -rf $W' EXIT
mkdir -p "$SAL"
bash $AQUI/hacer.sh $W/c
python3 -I $AQUI/../../../tools/hpq.py opcionales "$SAL" $W/c > /dev/null
python3 -I $AQUI/../../../tools/hpq.py linea "$SAL/ESCUDO.HPQ" > "$SAL/CATALOGO.LIN"
echo "paquete.sh: $SAL/ESCUDO.HPQ ($(stat -c %s "$SAL/ESCUDO.HPQ") octetos) y CATALOGO.LIN"
