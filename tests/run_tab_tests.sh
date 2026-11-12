#!/bin/bash
# Prueba del autocompletado con el tabulador en la línea de órdenes:
# Tab completa con la primera coincidencia; Tab otra vez, la siguiente (en
# vuelta); las carpetas acaban en «\»; sin coincidencias no cambia nada.
# Tab completion test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
cp "$IMG" $T/f.img
send() { printf '%b' "$1"; sleep "${2:-2}"; }
{
  sleep 6
  send 'cd hisp\t\r'                    # -> cd HISPADOS\
  send 'cd\r'
  send 'cd \\\r'
  send 'type read\t\r'                  # -> type README.TXT
  send 'echo hispados\\ed\t\r'          # -> HISPADOS\EDITAR.COM
  send 'echo hispados\\e\t\t\r'         # segunda coincidencia con E
  send 'echo hispados\\e\t\t\t\t\t\t\t\t\t\t\r'   # da la vuelta
  send 'echo zz\tfin\r'                 # sin coincidencias: no cambia
  send 'echo hispados\\alias.\t\r'      # con punto: ALIAS.INI
} | timeout 60 qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
  -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot > $T/log 2>&1
tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 > $T/log.txt
# aplicar los retrocesos como la pantalla (el autocompletado borra lo escrito)
python3 - $T/log.txt <<'PYEOF'
import sys
p = sys.argv[1]
out = []
for line in open(p, encoding='utf-8', errors='replace').read().split('\n'):
    cur = []
    pos = 0
    for ch in line:
        if ch == '\b':
            pos = max(0, pos - 1)
        else:
            if pos < len(cur):
                cur[pos] = ch
            else:
                cur.append(ch)
            pos += 1
    out.append(''.join(cur).rstrip())
open(p, 'w', encoding='utf-8').write('\n'.join(out))
PYEOF
fail=0
check() { if grep -qF -- "$1" $T/log.txt; then echo "  OK   $1"; else echo "  FAIL $1"; fail=1; fi; }
echo "== Hispa-DOS autocompletado con Tab =="
check "A:\\>cd HISPADOS\\"
check "A:\\HISPADOS"
check "A:\\>echo hispados\\EDITAR.COM"
check "zzfin"
check "hispados\\ALIAS.INI"
# la segunda coincidencia con E es distinta de la primera
a=$(grep -o '^A:.>echo hispados.E[A-Z.]*' $T/log.txt | sed -n 1p)
b=$(grep -o '^A:.>echo hispados.E[A-Z.]*' $T/log.txt | sed -n 2p)
if [ -n "$a" ] && [ -n "$b" ] && [ "$a" != "$b" ]; then echo "  OK   Tab otra vez: $a -> $b"; else echo "  FAIL Tab otra vez ($a / $b)"; fail=1; fi
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/AUTOEXEC/,$p' $T/log.txt; fi
rm -rf $T
exit $fail
