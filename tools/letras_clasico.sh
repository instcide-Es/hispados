#!/bin/bash
# Pone LETRAS=CLASICO en el CONFIG.SYS de una imagen de memoria USB (la
# partición empieza en el sector 2048): las letras, como siempre (la memoria
# de arranque es la C: aunque el equipo tenga disco duro; docs/UNIDADES.md).
# Para las imágenes de prueba (tools/mkprueba_*.sh, tools/prueba-cable), cuyos
# guiones e instrucciones dicen «C:\».
#   uso: letras_clasico.sh imagen
# Adds LETRAS=CLASICO to the CONFIG.SYS of a USB image.
set -eu
IMG="$1@@1048576"
T=$(mktemp -d)
trap 'rm -rf $T' EXIT
export MTOOLS_SKIP_CHECK=1
mtype -i "$IMG" ::CONFIG.SYS > $T/c.sys
python3 - $T/c.sys <<'PY'
import sys
t = open(sys.argv[1], 'rb').read().decode('cp437').replace('\r\n', '\n')
if 'LETRAS=CLASICO' not in t.upper():
    l = t.split('\n')
    linea = 'LETRAS=CLASICO'
    for i, x in enumerate(l):
        if x.strip().lower() == '[common]':
            l.insert(i + 1, linea)
            break
    else:
        l.insert(0, linea)
    t = '\n'.join(l)
open(sys.argv[1], 'wb').write(t.replace('\n', '\r\n').encode('cp437'))
PY
mcopy -o -i "$IMG" $T/c.sys ::CONFIG.SYS
