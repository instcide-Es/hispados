#!/bin/bash
# Prueba de la edición de la línea de órdenes (función 0Ah del núcleo) con
# pulsaciones reales (sendkey del monitor de QEMU): flechas izquierda/derecha,
# Inicio, Fin, Ctrl+flechas, Supr, Retroceso en medio, Insertar (sobrescribir),
# historial, Tab en medio de la línea y una línea de más de 80 columnas.
# Lo que queda en la línea se comprueba con lo que escribe ECHO y, en la
# línea larga, también con la memoria de pantalla.
# Command line editing test (real key presses).
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
cp "$IMG" $T/f.img
qemu-system-i386 -display none -serial file:$T/ser.txt -monitor unix:$T/mon,server,nowait \
    -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot &
QP=$!
sleep 6
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon >/dev/null 2>&1; }
k() { local x; for x in "$@"; do mon "sendkey $x"; sleep 0.08; done; }
# teclear un texto (letras, cifras, espacio, \ y .) con el teclado de EE. UU.
t() {
  local s=$1 i c
  for ((i = 0; i < ${#s}; i++)); do
    c=${s:i:1}
    case "$c" in
      ' ') c=spc ;;
      '\') c=backslash ;;
      '.') c=dot ;;
      [A-Z]) c=shift-$(printf '%s' "$c" | tr A-Z a-z) ;;
    esac
    k "$c"
  done
}
intro() { k ret; sleep 1; }

t "echo hla"; k left left; t "o"; intro                       # hola
t "echo abcdef"; k home ctrl-right delete delete; intro       # cdef
t "echo abcd"; k left left left left insert; t "XY"; intro    # XYcd
t "echo 14"; k left; t "23"; intro                            # 1234 (vuelve a insertar)
t "echo holaX mundo"; k ctrl-left left backspace; intro       # hola mundo
t "echo fin"; k home end; t "al"; intro                       # final
t "zzz"; k left up; intro                                     # el historial cambia la línea entera
t "echo uno dos tres"; k ctrl-left ctrl-left delete delete delete delete; intro   # uno tres
t "echo abc"; k home right right right right right; t "X"; k end; t "Y"; k left left delete; intro  # XabY
t "echo hisp z"; k left left tab; intro                       # HISPADOS\ z
t "echo hispados\\e y"; k left left tab; sleep 0.5; intro     # 1.ª coincidencia + « y»
t "echo hispados\\e z"; k left left tab; sleep 0.5; k tab; sleep 0.5; intro   # 2.ª + « z»
t "echo hispados\\ed.z"; k left left tab; sleep 0.5; intro    # EDITAR.COM.z
t "echo sinzz z"; k left left tab; t "q"; intro               # sin coincidencias: sinzzq z
# línea de más de 80 columnas: el cursor sube y baja de fila
D=01234567890123456789012345678901234567890123456789012345678901234567890123456789
t "echo $D"; k home ctrl-right; t "X"; k end; t "YW"; k backspace; t "Z"
k left left left left left left left left left left left left left left left
k delete end
sleep 0.5
mon "pmemsave 0xb8000 4000 \"$T/scr.bin\""; sleep 0.5
intro
kill $QP 2>/dev/null; wait $QP 2>/dev/null
tr -d '\r' < $T/ser.txt | iconv -f CP437 -t UTF-8 > $T/log.txt
# aplicar los retrocesos como la pantalla
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
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
# línea exacta: lo que escribe ECHO o la línea de órdenes como queda en pantalla
sale() { if grep -qxF -- "$1" $T/log.txt; then ok "$2: $1"; else ko "$2: $1"; fi; }
echo "== Hispa-DOS edición de la línea de órdenes =="
sale "hola" "flecha izquierda e insertar en medio"
sale "A:\\>echo hola" "la línea como queda en pantalla"
sale "cdef" "Inicio, Ctrl+derecha y Supr"
sale "XYcd" "Insertar: sobrescribir"
sale "1234" "la línea siguiente vuelve a insertar"
sale "hola mundo" "Ctrl+izquierda y Retroceso en medio"
sale "final" "Inicio y Fin"
n=$(grep -cx "final" $T/log.txt)
if [ "$n" -eq 2 ]; then ok "el historial reemplaza la línea entera"; else ko "historial tras mover el cursor: $n"; fi
sale "uno tres" "Ctrl+izquierda dos veces y Supr"
sale "XabY" "flecha derecha, Fin y Supr"
sale "HISPADOS\\ z" "Tab con el cursor en medio"
sale "A:\\>echo HISPADOS\\ z" "Tab en medio: la línea en pantalla"
sale "hispados\\EDITAR.COM.z" "Tab en medio con carpeta"
sale "sinzzq z" "Tab en medio sin coincidencias"
a=$(grep -x 'hispados.E[A-Z0-9.]* y' $T/log.txt | head -1)
b=$(grep -x 'hispados.E[A-Z0-9.]* z' $T/log.txt | head -1)
if [ -n "$a" ] && [ -n "$b" ] && [ "${a% y}" != "${b% z}" ]; then ok "Tab otra vez en medio: $a -> $b"; else ko "Tab otra vez en medio ($a / $b)"; fi
L="X${D:0:67}${D:68}YZ"
sale "$L" "línea de más de 80 columnas"
sale "A:\\>echo $L" "línea larga como queda (eco)"
python3 - $T/scr.bin "A:\\>echo $L" <<'PYEOF' || fail=1
import sys
d = open(sys.argv[1], 'rb').read()
txt = d[0::2].decode('cp437')
want = sys.argv[2]
i = txt.rfind(want)
good = i >= 0 and i % 80 == 0 and txt[i + len(want):i + len(want) + 40].strip() == ''
print(("  OK   " if good else "  FAIL ") + "línea larga: la pantalla (dos filas, sin restos)")
if not good:
    for r in range(25):
        print("     |" + txt[r * 80:(r + 1) * 80].rstrip())
sys.exit(0 if good else 1)
PYEOF
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/AUTOEXEC/,$p' $T/log.txt; fi
rm -rf $T
exit $fail
