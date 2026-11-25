#!/bin/bash
# COPY uniendo archivos cuando el destino es también un origen:
#  - COPY A+B A: se añade B detrás de A (como MS-DOS), A no se vacía;
#  - COPY C+D D: se perdería D al crearlo: no se hace nada y se avisa;
#  - COPY E+F G: la unión de siempre;
#  - COPY W\*.TXT W\TODO.TXT con TODO.TXT el primero del comodín: se le añaden
#    los demás; si no es el primero: no se hace nada; si no existía: la unión,
#    sin copiarse a sí mismo cuando el comodín llega al recién creado.
# COPY concatenation onto one of its sources.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
fail=0
cp "$IMG" $T/f.img
mk() { printf '%s' "$2" > $T/x; mcopy -o -i $T/f.img $T/x "::$1"; }
mmd -i $T/f.img ::U ::V ::W
mk A.TXT uno; mk B.TXT dos; mk C.TXT tres; mk D.TXT cuatro; mk E.TXT cinco; mk F.TXT seis
mk W/TODO.TXT todo; mk W/X1.TXT equis1; mk W/X2.TXT equis2
mk V/Y1.TXT ye1; mk V/TODO.TXT viejo; mk V/Y2.TXT ye2
mk U/Z1.TXT zeta1; mk U/Z2.TXT zeta2
{ sleep 5
  for c in 'COPY A.TXT+B.TXT A.TXT' 'COPY C.TXT+D.TXT D.TXT' 'COPY E.TXT+F.TXT G.TXT' \
           'COPY W\*.TXT W\TODO.TXT' 'COPY V\*.TXT V\TODO.TXT' 'COPY U\*.TXT U\TODO.TXT' 'ECHO fin-union'; do
    printf '%s\r' "$c"; sleep 1.5
  done; sleep 2
} | timeout 40 qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
      -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot > $T/log 2>&1
tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 > $T/log.txt
echo "== Hispa-DOS COPY: unir sobre uno de los orígenes =="
igual() {  # archivo esperado
  local v; v=$(mtype -i $T/f.img "::$1" 2>/dev/null)
  if [ "$v" = "$2" ]; then echo "  OK   $1 = «$2»"; else echo "  FAIL $1 = «$v» (se esperaba «$2»)"; fail=1; fi
}
igual A.TXT unodos
igual B.TXT dos
igual D.TXT cuatro
igual G.TXT cincoseis
igual W/TODO.TXT todoequis1equis2
igual V/TODO.TXT viejo
igual U/TODO.TXT zeta1zeta2
n=$(grep -c "El destino es también uno de los orígenes" $T/log.txt)
if [ "$n" = 2 ]; then echo "  OK   dos avisos de destino entre los orígenes"; else echo "  FAIL avisos de destino entre los orígenes: $n (2)"; fail=1; fi
if grep -qF "fin-union" $T/log.txt; then echo "  OK   fin-union"; else echo "  FAIL fin-union"; fail=1; fi
if /usr/sbin/fsck.fat -n $T/f.img > $T/fsck.txt 2>&1; then echo "  OK   fsck.fat limpio"
else echo "  FAIL fsck.fat"; cat $T/fsck.txt; fail=1; fi
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi
exit $fail
