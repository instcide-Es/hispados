#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/actualiza.sh CARPETA CLAVE_PRIVADA
# Hace lo que se publica en el servidor de las actualizaciones de
# Hispa-Pentium (http://actualiza.hispados.es/pentium/): en CARPETA, los
# paquetes del sistema (.HPQ: los mismos que van en la memoria USB y en el
# CD, hechos de lo que se instala), su CATALOGO.TXT y la firma del catálogo,
# CATALOGO.FIR (Ed25519, con CLAVE_PRIVADA: tools/hpq.py). PAQUETES no usa
# nada traído de Internet sin esa firma.
# Para publicar una versión nueva de un paquete: subir su número en
# tools/hpq.py (SISTEMA, VERSION_SISTEMA), hacer esto otra vez y copiar la
# carpeta entera al servidor (los .HPQ y el catálogo firmado, juntos: si el
# catálogo no va con sus paquetes, las sumas no cuadran y no se instala).
set -eu
export LC_ALL=C.UTF-8
AQUI=$(dirname "$(readlink -f "$0")")
SAL=${1:?uso: actualiza.sh CARPETA CLAVE_PRIVADA}
CLAVE=${2:?uso: actualiza.sh CARPETA CLAVE_PRIVADA}
W=$(mktemp -d /dev/shm/pgraf-act.XXXX)
trap 'rm -rf $W' EXIT
mkdir $W/f $W/m $W/i
MEDIO=usb
. $AQUI/sistema.sh
mkdir -p "$SAL"
rm -f "$SAL"/*.HPQ "$SAL"/CATALOGO.TXT "$SAL"/CATALOGO.FIR
cp $W/paq/*.HPQ $W/paq/CATALOGO.TXT "$SAL"/
python3 -I $AQUI/../../tools/hpq.py firma "$SAL" "$CLAVE" > /dev/null
python3 -I $AQUI/../../tools/hpq.py comprueba "$SAL" $AQUI/clave.cas
echo "actualiza.sh: $SAL: $(ls "$SAL"/*.HPQ | wc -l) paquetes, CATALOGO.TXT y CATALOGO.FIR"
