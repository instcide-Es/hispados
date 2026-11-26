#!/bin/bash
# Trae los controladores LIBRES (GPL) con que se prueba que Hispa-8086 carga
# los de DOS: no van en el repositorio (cada usuario usa los suyos). Los deja
# en $DRV_LIBRES (por omisión, ~/.cache/hispa-drivers/descargas):
#   - los «packet drivers» de Crynwr (pktd11.zip, GPL): NE2000.COM, PKTSTAT...
#     (crynwr.com ya no contesta: la copia del Internet Archive);
#   - de FreeDOS 1.3 (GPL-2.0): NANSI (un ANSI.SYS), CuteMouse (el ratón) y
#     SRDISK (un disco en memoria).
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
set -eu
D=${DRV_LIBRES:-$HOME/.cache/hispa-drivers/descargas}
mkdir -p "$D"; cd "$D"
[ -f pktd11.zip ] || curl -fsSL -o pktd11.zip https://web.archive.org/web/2015/http://www.crynwr.com/drivers/pktd11.zip
mkdir -p pktd11 && unzip -o -q pktd11.zip -d pktd11
FD=https://www.ibiblio.org/pub/micro/pc-stuff/freedos/files/repositories/1.3
for p in base/nansi base/ctmouse drivers/srdisk; do
  n=$(basename $p)
  [ -f $n.zip ] || curl -fsSL -o $n.zip $FD/$p.zip
  mkdir -p $n && unzip -o -q $n.zip -d $n
done
echo "Controladores libres en $D"
