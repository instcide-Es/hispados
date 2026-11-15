#!/bin/bash
# Prueba del remapeo de teclas (TECLAS.INI) de TECLADO/KEYB. Se pone un
# \HISPADOS\TECLAS.INI con un remapeo (58=2D: la tecla F12, make 58, actúa como
# la tecla X, make 2D), se carga TECLADO y se comprueba que F12 escribe «x».
# El monitor de QEMU pulsa teclas de verdad (sendkey -> INT 09h -> INT 15h 4Fh)
# y se lee el eco por el puerto serie, decodificado en la página de códigos 437.
# Key remap (TECLAS.INI) test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
fail=0
T=$(mktemp -d)
cp "$IMG" $T/f.img

# TECLAS.INI de prueba: F12 (make 58) -> X (make 2D). CRLF, como en el disco.
printf '; prueba de remapeo\r\n58=2D\r\n' > $T/TECLAS.INI
mcopy -o -i $T/f.img $T/TECLAS.INI ::HISPADOS/TECLAS.INI || { echo "mcopy falló"; rm -rf $T; exit 1; }

qemu-system-i386 -display none -serial file:$T/ser.txt -monitor unix:$T/mon,server,nowait \
    -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot &
QP=$!
sleep 4
k() { for x in "$@"; do printf 'sendkey %s\n' "$x" | socat - UNIX-CONNECT:$T/mon >/dev/null 2>&1; sleep 0.1; done; }

# 1) ANTES de cargar TECLADO: F12 no escribe nada (teclado de la BIOS).
#    Se teclea «rem ab» con F12 en medio; F12 no mete carácter.
k r e m spc a f12 b ret
sleep 1
# 2) Cargar TECLADO SP: lee \HISPADOS\TECLAS.INI y activa el remapeo.
k t e c l a d o spc s p ret
sleep 1.5
# 3) DESPUÉS: F12 escribe «x» -> «rem axb»; y el español sigue (ñ con «;»).
k r e m spc a f12 b ret
sleep 1
k r e m spc semicolon ret
sleep 1

kill $QP 2>/dev/null; wait $QP 2>/dev/null

echo "== Hispa-DOS remapeo de teclas (TECLAS.INI) =="
python3 - $T/ser.txt <<'EOF' || fail=1
import sys
t = open(sys.argv[1], 'rb').read().decode('cp437').replace('\r', '')
ok = True
checks = [
    ("rem ab",  True),   # antes: F12 no escribe nada
    ("Remapeo de teclas de TECLAS.INI activo.", True),  # mensaje al instalar
    ("rem axb", True),   # después: F12 -> x
    ("rem ñ",   True),   # el teclado español sigue funcionando
]
for want, present in checks:
    if (want in t) == present:
        print("  OK   " + want)
    else:
        print("  FAIL " + want); ok = False
if not ok:
    print("---- salida ----")
    print(t)
sys.exit(0 if ok else 1)
EOF
rm -rf $T
exit $fail
