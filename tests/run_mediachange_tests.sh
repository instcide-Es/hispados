#!/bin/bash
# Prueba del cambio de disco sin reiniciar: CD/DVD (HSPCD) y disquete en B:.
# Tras cambiar el disco, la unidad tiene que mostrar el contenido nuevo.
# Media change test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
cp "$IMG" $T/f.img
printf 'DEVICE=A:\\HISPADOS\\HSPCD.SYS\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
mkdir -p $T/cd1 $T/cd2
printf 'Primer disco\r\n' > $T/cd1/UNO.TXT
printf 'Segundo disco\r\n' > $T/cd2/DOS.TXT
genisoimage -quiet -V PRIMERO -o $T/cd1.iso $T/cd1
genisoimage -quiet -V SEGUNDO -o $T/cd2.iso $T/cd2
for n in 1 2; do
  dd if=/dev/zero of=$T/fl$n.img bs=1k count=1440 status=none
  mformat -i $T/fl$n.img -f 1440 -v DISQUETE$n ::
done
printf 'Disquete uno\r\n' > $T/A1.TXT; mcopy -i $T/fl1.img $T/A1.TXT ::A1.TXT
printf 'Disquete dos\r\n' > $T/B2.TXT; mcopy -i $T/fl2.img $T/B2.TXT ::B2.TXT
# CD en IDE (por defecto) o en SATA/AHCI (CDBUS=ahci)
if [ "${CDBUS:-ide}" = ahci ]; then
  CDARGS="-device ahci,id=ahci -drive id=cd,file=$T/cd1.iso,media=cdrom,if=none -device ide-cd,bus=ahci.0,drive=cd,id=lector"
  CDDEV=cd
else
  CDARGS="-drive file=$T/cd1.iso,media=cdrom,if=ide,index=2"
  CDDEV=ide1-cd0
fi
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; }
qemu-system-i386 -display none -serial file:$T/log -monitor unix:$T/mon,server,nowait -parallel none \
  -drive file=$T/f.img,format=raw,if=floppy,index=0 -drive file=$T/fl1.img,format=raw,if=floppy,index=1 \
  $CDARGS -boot a -no-reboot ${QTRACE:-} &
QP=$!
sleep 7
ser() { printf 'sendkey %s\n' "$@" | socat - UNIX-CONNECT:$T/mon >/dev/null 2>&1; }
typ() { python3 - "$1" <<'EOF' | socat - UNIX-CONNECT:$T/mon >/dev/null 2>&1
import sys
m = {' ': 'spc', '.': 'dot', ':': 'shift-semicolon', '\\': 'backslash'}
for ch in sys.argv[1]:
    k = m.get(ch, ch.lower())
    if ch.isupper(): k = 'shift-' + k
    print('sendkey ' + k)
print('sendkey ret')
EOF
}
typ "DIR C:"; sleep 2
typ "DIR B:"; sleep 2
mon "change $CDDEV $T/cd2.iso"
mon "change floppy1 $T/fl2.img"
sleep 3
typ "DIR C:"; sleep 2
typ "TYPE C:DOS.TXT"; sleep 2
typ "DIR B:"; sleep 2
typ "TYPE B:B2.TXT"; sleep 2
kill $QP 2>/dev/null; wait $QP 2>/dev/null
tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 > $T/log.txt
fail=0
check() { if grep -qF -- "$1" $T/log.txt; then echo "  OK   $1"; else echo "  FAIL $1"; fail=1; fi; }
echo "== Hispa-DOS cambio de disco (CD en ${CDBUS:-ide}) =="
check "El volumen de la unidad C es PRIMERO"
check "UNO      TXT"
check "El volumen de la unidad B es DISQUETE1"
check "El volumen de la unidad C es SEGUNDO"
check "DOS      TXT"
check "Segundo disco"
check "El volumen de la unidad B es DISQUETE2"
check "Disquete dos"
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi
rm -rf $T
exit $fail
