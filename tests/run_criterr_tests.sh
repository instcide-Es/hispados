#!/bin/bash
# Prueba del error crítico (INT 24h): «¿Anular, Reintentar, Ignorar,
# Descartar?» con la disquetera B: vacía y con un disquete protegido contra
# escritura.
# Critical error (INT 24h) test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
cp "$IMG" $T/f.img
dd if=/dev/zero of=$T/b.img bs=1k count=1440 status=none
mformat -i $T/b.img -f 1440 -v DATOSB ::
printf 'Hola desde B\r\n' > $T/HOLA.TXT; mcopy -i $T/b.img $T/HOLA.TXT ::HOLA.TXT
cp $T/b.img $T/ro.img
cp $T/ro.img $T/ro.orig
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; }
typ() { python3 - "$1" <<'EOF' | socat - UNIX-CONNECT:$T/mon >/dev/null 2>&1
import sys
m = {' ': 'spc', '.': 'dot', ':': 'shift-semicolon', '\\': 'backslash', '*': 'shift-8'}
for ch in sys.argv[1]:
    k = m.get(ch, ch.lower())
    if ch.isupper(): k = 'shift-' + k
    print('sendkey ' + k)
print('sendkey ret')
EOF
}
key() { printf 'sendkey %s\n' "$1" | socat - UNIX-CONNECT:$T/mon >/dev/null 2>&1; }
qemu-system-i386 -display none -serial file:$T/log -monitor unix:$T/mon,server,nowait -parallel none \
  -drive if=none,id=fa,file=$T/f.img,format=raw -drive if=none,id=fb \
  -device floppy,unit=0,drive=fa -device floppy,unit=1,drive=fb -boot a -no-reboot &
QP=$!
sleep 7
# 1) B: vacía: Reintentar sigue fallando; con el disco puesto, Reintentar lista
typ "DIR B:"; sleep 3
key r; sleep 3
mon "change fb $T/b.img raw"; sleep 1
key r; sleep 3
# 2) Descartar: la orden falla con su mensaje
mon "eject -f fb"; sleep 1
typ "TYPE B:HOLA.TXT"; sleep 3
key d; sleep 2
# 3) Anular en un programa externo: vuelve al indicador
typ "XCOPIAR B:\\*.* A:\\"; sleep 3
key a; sleep 2
typ "ECHO sigue vivo"; sleep 2
# 4) protegido contra escritura
mon "change fb $T/ro.img raw read-only"; sleep 1
typ "DIR B:"; sleep 3
typ "COPIAR A:\\README.TXT B:"; sleep 3
key d; sleep 2
kill $QP 2>/dev/null; wait $QP 2>/dev/null
tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 > $T/log.txt
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$1" $T/log.txt; then ok "$1"; else ko "$1"; fi; }
echo "== Hispa-DOS error crítico (INT 24h) =="
check "Error al leer la unidad B: la unidad no está preparada"
check "¿Anular, Reintentar, Ignorar, Descartar? "
n=$(grep -c "la unidad no está preparada" $T/log.txt)
if [ "$n" -ge 3 ]; then ok "Reintentar vuelve a preguntar ($n avisos)"; else ko "Reintentar vuelve a preguntar ($n avisos)"; fi
check "El volumen de la unidad B es DATOSB"
check "La unidad no está preparada"
check "sigue vivo"
check "Error al escribir en la unidad B: el disco está protegido contra escritura"
if cmp -s $T/ro.img $T/ro.orig; then ok "el disquete protegido no ha cambiado"; else ko "el disquete protegido ha cambiado"; fi
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/>DIR B:/,$p' $T/log.txt; fi
rm -rf $T
exit $fail
