#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/hvprun/hacer.sh CARPETA
# Hace en CARPETA el ejecutor de los programas de Hispa Ventanas para
# Hispa-Pentium (HVPRUN.BIN) y los programas de la oficina de Hispa-386 que
# corren con él (FACTURAS.HVP, PLUMA.HVP, CUENTAS.HVP, MIRADOR.HVP), con lo
# que necesitan al lado (TIPOS.DAT, DICCIO.DAT). Todo sale de las mismas
# fuentes que en Hispa-386 (ventanas/*.inc y programas/*.asm), sin tocarlas:
# los fuentes se pasan a la página 437 (tools/cp437.py --ventanas), como hace
# el Makefile para VENTANAS.COM.
# Necesita python3 y nasm.
set -e
export LC_ALL=C.UTF-8
AQUI=$(dirname "$(readlink -f "$0")")
RAIZ=$(readlink -f "$AQUI/../../..")
SAL=${1:?uso: hacer.sh CARPETA}
mkdir -p "$SAL"
SAL=$(readlink -f "$SAL")
G=$(mktemp -d)
trap 'rm -rf $G' EXIT
for f in "$RAIZ"/ventanas/*.inc "$AQUI"/*.inc "$AQUI"/hvprun.asm; do
  python3 -I "$RAIZ/tools/cp437.py" --ventanas "$f" "$G/$(basename "$f")"
done
python3 "$RAIZ/tools/mkfuente.py" "$RAIZ/ventanas/fuente.txt" "$G/fuente.inc" "$G/fuente.ppm" > /dev/null
nasm -f bin -w+error=number-overflow -i "$G/" -o "$SAL/HVPRUN.BIN" "$G/hvprun.asm"
# los programas: los mismos .HVP que en Hispa-386 (tools/mkhvp.py)
for p in facturas:FACTURAS pluma:PLUMA cuentas:CUENTAS mirador:MIRADOR; do
  f=${p%%:*}; N=${p##*:}
  python3 "$RAIZ/tools/mkhvp.py" "$RAIZ/programas/$f.asm" "$G/hvp/$N.HVP" > /dev/null
  cp "$G/hvp/$N.HVP" "$SAL/"
done
cp "$RAIZ/programas/comun/TIPOS.DAT" "$RAIZ/programas/comun/DICCIO.DAT" "$SAL/"
