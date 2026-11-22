#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# blocprueba/prueba_bloc.sh: prueba el Bloc de notas (BLOC.BIN, ubloc.cas)
# en Linux, con corre32, sin ventana: «BLOC /PRUEBA GUION» hace lo que dice
# cada guion de guiones/ (teclas, abrir, guardar, menús) sobre una copia de
# guiones/datos/, y lo que escribe (el texto, el estado, los avisos y los
# archivos guardados) tiene que ser lo de su .ESP (REHAZ=1 los rehace: hay
# que mirarlos a mano). Y BlocMem y BlocTxt al azar (pmem, ptxt), un archivo
# de 11 MB (abrir, ir al final, buscar, reemplazar 250.000 y deshacer, en
# segundos) y que CASTEL32 saque lo mismo que ccastel.py con ubloc.cas.
# Escribe «bien: ...» o «mal: ...» y acaba con 1 si algo va mal.
set -u
B=$(cd "$(dirname "$0")" && pwd)
C=$(cd "$B/../.." && pwd)                   # castel/
R=$C/corre32/corre32.sh
T=${BLOC_TMP:-/tmp/blocprueba}/bloc
rm -rf $T; mkdir -p $T
mal=0
bien() { echo "bien: $*"; }
malo() { echo "mal: $*"; mal=1; }
comp() {   # programa.cas SALIDA.BIN
  python3 -I $C/ccastel.py $1 -o $T/x.asm -I $C/nucleo32 > $T/compila.txt 2>&1 && nasm -f bin $T/x.asm -o $2 2>> $T/compila.txt
}
# ---- BlocMem y BlocTxt ----
for p in pmem ptxt; do
  if comp $B/$p.cas $T/$p.bin; then
    r=$($R $T/$p.bin 2>&1 | grep -av '^corre32:' | iconv -f CP437 -t UTF-8)
    case "$r" in bien:*) echo "$r" ;; *) malo "$p: $r" ;; esac
  else malo "$p.cas no compila: $(head -3 $T/compila.txt)"; fi
done
# ---- el Bloc ----
if ! comp $C/nucleo32/ubloc.cas $T/BLOC.BIN; then
  malo "ubloc.cas no compila: $(head -5 $T/compila.txt)"; exit 1
fi
bien "BLOC.BIN compila ($(stat -c %s $T/BLOC.BIN) octetos)"
for g in $B/guiones/*.GUI; do
  n=$(basename $g .GUI)
  rm -rf $T/g; mkdir -p $T/g
  cp $B/guiones/datos/* $T/g/
  cp $g $T/g/GUION.TXT
  (cd $T/g && $R $T/BLOC.BIN /PRUEBA GUION.TXT 2>&1 | grep -av '^corre32:' > $T/$n.sal)
  # y lo guardado: cada archivo que haya cambiado, en hexadecimal
  for f in $T/g/*; do
    b=$(basename $f)
    [ "$b" = GUION.TXT ] && continue
    if ! cmp -s $f $B/guiones/datos/$b 2> /dev/null; then echo "archivo $b: $(od -An -c $f | tr -s ' ' | tr -d '\n')" >> $T/$n.sal; fi
  done
  if [ -n "${REHAZ:-}" ]; then cp $T/$n.sal $B/guiones/$n.ESP; fi
  if cmp -s $T/$n.sal $B/guiones/$n.ESP; then
    bien "el guion $n: $(sed -n 's/^# //p' $g | head -1)"
  else
    malo "el guion $n no da lo esperado:"; diff -a $B/guiones/$n.ESP $T/$n.sal | head -12 | iconv -f CP437 -t UTF-8 2> /dev/null | sed 's/^/        /'
  fi
done
# ---- 11 MB: abrir, ir al final, buscar, reemplazar todas y deshacer ----
mkdir -p $T/gr
python3 -I -c "
import sys
sys.stdout.buffer.write(''.join('línea %06d con algo de texto para llenar\r\n' % i for i in range(250000)).encode('cp437'))" > $T/gr/GRANDE.TXT
cat > $T/gr/GUION.TXT <<'G'
abre GRANDE.TXT
estado
teclas {ctrl-fin}
estado
teclas {ctrl-f}000777{intro}
estado
teclas {esc}{ctrl-h}texto{tab}{tab}{tab}{tab}{tab}TEXTO{tab}{tab}{intro}
estado
teclas {ctrl-z}
estado
G
t0=$(date +%s%N)
(cd $T/gr && $R $T/BLOC.BIN /PRUEBA GUION.TXT 2>&1 | grep -av '^corre32:' > $T/grande.sal)
t1=$(date +%s%N)
ms=$(( (t1 - t0) / 1000000 ))
if grep -q "líneas 250001 página 437 crlf pestañas" <(iconv -f CP437 -t UTF-8 $T/grande.sal) \
  && grep -q "lín 250001 col 1 sel 11000000-11000000" <(iconv -f CP437 -t UTF-8 $T/grande.sal) \
  && grep -q "lín 778 col 13 sel 34195-34201" <(iconv -f CP437 -t UTF-8 $T/grande.sal) \
  && grep -q "Reemplazadas: 250000" <(iconv -f CP437 -t UTF-8 $T/grande.sal) \
  && [ "$(iconv -f CP437 -t UTF-8 $T/grande.sal | grep '^estado' | tail -1 | grep -c cambiado)" = 0 ] && [ $ms -lt 20000 ]; then
  bien "11 MB (250.000 líneas): abrir, ir al final, buscar, reemplazar 250.000 de una vez y deshacerlo de una vez (sin cambios otra vez), en $ms ms con corre32"
else
  malo "el archivo de 11 MB ($ms ms):"; iconv -f CP437 -t UTF-8 $T/grande.sal | head -12 | sed 's/^/        /'
fi
# ---- CASTEL32 ----
BLOC_TMP=$T $B/igual32.sh $C/nucleo32/ubloc.cas $B/pmem.cas $B/ptxt.cas || mal=1
exit $mal
