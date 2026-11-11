#!/bin/bash
# Prueba del teclado español (KEYB SP) con pulsaciones reales: el monitor de
# QEMU envía las teclas (sendkey, pasan por INT 09h) y se lee el eco por el
# puerto serie, decodificado en la página de códigos 437.
# Spanish keyboard test with real key presses.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
cp "$IMG" $T/f.img
qemu-system-i386 -display none -serial file:$T/ser.txt -monitor unix:$T/mon,server,nowait \
    -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot &
QP=$!
sleep 4
k() { for x in "$@"; do printf 'sendkey %s\n' "$x" | socat - UNIX-CONNECT:$T/mon >/dev/null 2>&1; sleep 0.08; done; }
# KEYB SP (aún con el teclado de EE. UU.)
k shift-k shift-e shift-y shift-b spc shift-s shift-p ret
sleep 1.5
# REM [ñ á ¿ ¡ @ / è ü < Ñ ç]  (AltGr+` = [)
k shift-r shift-e shift-m spc alt_r-bracket_left
k semicolon                          # ñ
k apostrophe a                       # ´ + a = á
k shift-equal                        # ¿
k equal                              # ¡
k alt_r-2                            # @
k shift-7                            # /
k bracket_left e                     # ` + e = è
k shift-apostrophe u                 # ¨ + u = ü
k less                               # <
k shift-semicolon                    # Ñ
k backslash                          # ç
k alt_r-bracket_right                # ]
k ret
sleep 1.5
k shift-k shift-e shift-y shift-b ret
sleep 1.5
kill $QP 2>/dev/null; wait $QP 2>/dev/null
fail=0
echo "== HispaDOS Spanish keyboard tests =="
python3 - $T/ser.txt <<'EOF' || fail=1
import sys
t = open(sys.argv[1], 'rb').read().decode('cp437').replace('\r', '')
ok = True
for want in ("Teclado español instalado.", "REM [ñá¿¡@/èü<Ñç]", "Teclado activo: español (SP)."):
    if want in t:
        print("  OK   " + want)
    else:
        print("  FAIL " + want); ok = False
if not ok:
    print(t)
sys.exit(0 if ok else 1)
EOF
rm -rf $T
exit $fail
