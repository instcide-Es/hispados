#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# blocprueba/igual32.sh PROGRAMA.cas...: compila cada programa (de nucleo32
# o de blocprueba, con los módulos de nucleo32) con ccastel.py y con CASTEL32
# (en Linux, con corre32) y mira que saquen el mismo ensamblador, como la
# parte 15 de pruebas.sh. CASTEL32 se hace la primera vez en $BLOC_TMP/c32.
set -u
AQUI=$(cd "$(dirname "$0")/../.." && pwd)        # castel/
T=${BLOC_TMP:-/tmp/blocprueba}
W=$T/c32
R=$AQUI/corre32/corre32.sh
mkdir -p $W
if [ ! -f $W/castel32.bin ]; then
  python3 -I $AQUI/ccastel.py $AQUI/compilador/32/pentium/castel32.cas -o $W/castel32.asm -I $AQUI/compilador/32 \
    -I $AQUI/biblioteca32/pentium -I $AQUI/compilador -I $AQUI/nucleo32 > /dev/null && nasm -f bin $W/castel32.asm -o $W/castel32.bin || exit 2
fi
up() { basename $1 .cas | tr a-z A-Z | cut -c1-8; }
F=$W/f; rm -rf $F; mkdir -p $F/NUC $F/BIB32 $F/S
for f in $AQUI/biblioteca32/*.cas; do cp $f $F/BIB32/$(up $f).CAS; done
for f in $AQUI/nucleo32/*.cas; do cp $f $F/NUC/$(up $f).CAS; done
mal=0
for p in "$@"; do
  N=$(up $p)
  cp $p $F/NUC/$N.CAS
  python3 -I $AQUI/ccastel.py $p -I $AQUI/nucleo32 -o $W/$N.ref > /dev/null 2>&1 || { echo "mal: $p no compila con ccastel.py"; mal=1; continue; }
  (cd $F/NUC && $R -C $F $W/castel32.bin $N.CAS -o \\S\\$N.ASM -I \\BIB32 > $F/S/$N.SAL 2>&1)
  if cmp -s $W/$N.ref $F/S/$N.ASM; then echo "bien: CASTEL32 saca lo mismo que ccastel.py en $(basename $p)"
  else echo "mal: CASTEL32 no saca lo mismo que ccastel.py en $(basename $p):"; head -5 $F/S/$N.SAL | iconv -f CP437 -t UTF-8; mal=1; fi
done
exit $mal
