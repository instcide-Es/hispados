#!/bin/bash
# Prueba del autocompletado con el tabulador en la línea de órdenes:
# Tab completa con la primera coincidencia; Tab otra vez, la siguiente (en
# vuelta); las carpetas acaban en «\»; sin coincidencias no cambia nada.
# En la orden (primera palabra, también con la línea vacía o tras «|») solo
# salen programas (.COM, .EXE, .BAT); con ruta, también las carpetas.
# Tab completion test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
cp "$IMG" $T/f.img
# \TP: programas mezclados con otros ficheros y una carpeta (en este orden);
# \TV: ningún programa
printf '\xC3' > $T/ret.com              # RET: no hace nada
printf '@echo BBAT\r\n' > $T/b.bat
echo nota > $T/nota.txt
mmd -i $T/f.img ::TP ::TV ::TV/DD
mcopy -i $T/f.img $T/nota.txt ::TP/NOTA.TXT
mcopy -i $T/f.img $T/ret.com ::TP/A.COM
mmd -i $T/f.img ::TP/SUB
mcopy -i $T/f.img $T/b.bat ::TP/B.BAT
mcopy -i $T/f.img $T/nota.txt ::TP/LEER.DOC
mcopy -i $T/f.img $T/ret.com ::TP/C.EXE
mcopy -i $T/f.img $T/nota.txt ::TV/X.TXT
mcopy -i $T/f.img $T/nota.txt ::TV/Y.CO
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
  # en la orden (primera palabra): solo programas
  send 'hispados\\vel\t\r'              # con ruta -> HISPADOS\VELOCIDA.COM
  send 'cd \\tp\r'
  send '\t\r' 1                         # línea vacía: A.COM
  send '\t\t\r' 1                       # B.BAT
  send '\t\t\t\r' 1                     # C.EXE
  send '\t\t\t\t\r' 1                   # vuelve a empezar: A.COM
  send '   \t\r' 1                      # con espacios delante: A.COM
  send 'b\t\r' 1                        # con prefijo: B.BAT
  send 'n\tfin\r' 1                     # NOTA.TXT no es un programa
  send 's\tfin\r' 1                     # carpeta sin ruta: no sale
  send 'echo hola|\t\t\r'               # tras la tubería: B.BAT
  send 'echo hola | c\t\r'              # C.EXE
  send 'echo \t\t\t\t\tF\r' 1           # parámetro: todo (5.º = LEER.DOC)
  send 'echo \t\t\tP\r' 1               # parámetro: también carpetas (SUB\)
  send 'cd \\\r'
  send 'tp\\\t\t\r' 1                   # con ruta: A.COM, SUB\ (no NOTA.TXT)
  send 'tp\\s\tfin\r' 1                 # carpeta intermedia en la orden
  send 'cd \\tv\r'
  send '\tvacia\r' 1                    # sin programas: no cambia nada
  send '\t\t\tvacia2\r' 1
  send 'echo \tQ\r' 1                   # parámetro: X.TXT
} | timeout 90 qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
  -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot > $T/log 2>&1
# (sin el pitido de la BIOS cuando no hay nada que completar)
tr -d '\r\a' < $T/log | iconv -f CP437 -t UTF-8 > $T/log.txt
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
checkl() { if grep -qxF -- "$1" $T/log.txt; then echo "  OK   $1"; else echo "  FAIL $1"; fail=1; fi; }
echo "== Tab en la orden: solo programas =="
checkl "A:\\>hispados\\VELOCIDA.COM"
# línea vacía en \TP: A.COM, B.BAT, C.EXE y otra vez A.COM, en orden
got=$(grep '^A:.TP>' $T/log.txt | sed -n 1,5p | tr '\n' '|')
exp='A:\TP>A.COM|A:\TP>B.BAT|A:\TP>C.EXE|A:\TP>A.COM|A:\TP>   A.COM|'
if [ "$got" = "$exp" ]; then echo "  OK   línea vacía: A.COM, B.BAT, C.EXE, A.COM (y con espacios)"; else echo "  FAIL línea vacía: $got"; fail=1; fi
checkl "A:\\TP>B.BAT"
checkl "A:\\TP>nfin"
checkl "A:\\TP>sfin"
checkl "A:\\TP>echo hola|B.BAT"
checkl "A:\\TP>echo hola | C.EXE"
checkl "A:\\>tp\\SUB\\"
checkl "A:\\>tp\\SUB\\fin"
checkl "A:\\TV>vacia"
checkl "A:\\TV>vacia2"
echo "== Tab en los parámetros: todo =="
checkl "A:\\TP>echo LEER.DOCF"
checkl "A:\\TP>echo SUB\\P"
checkl "A:\\TV>echo DD\\Q"
# la segunda coincidencia con E es distinta de la primera
a=$(grep -o '^A:.>echo hispados.E[A-Z.]*' $T/log.txt | sed -n 1p)
b=$(grep -o '^A:.>echo hispados.E[A-Z.]*' $T/log.txt | sed -n 2p)
if [ -n "$a" ] && [ -n "$b" ] && [ "$a" != "$b" ]; then echo "  OK   Tab otra vez: $a -> $b"; else echo "  FAIL Tab otra vez ($a / $b)"; fail=1; fi
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/AUTOEXEC/,$p' $T/log.txt; fi
rm -rf $T
exit $fail
