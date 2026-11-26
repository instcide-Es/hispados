#!/bin/bash
# HSPUSB, la ruta de DOS (petición de leer/escribir del controlador): el
# sector, sumado al comienzo de la partición, no puede dar la vuelta a los
# 32 bits ni pasar del final del disco (antes, FFFFF800h + 2048 leía o
# escribía el MBR). Lo de siempre sigue: leer, escribir, fsck.fat limpio.
# Comprueba además que HSPUSB sigue validando bien los CSW de QEMU.
# HSPUSB: LBA bounds on the DOS path.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
fail=0
nasm -f bin -o $T/USBLBA.COM tests/progs/usblba.asm || exit 1
cp "$IMG" $T/f.img
printf 'DEVICE=A:\\HISPADOS\\HSPUSB.SYS /F\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/f.img $T/CONFIG.SYS $T/USBLBA.COM ::
dd if=/dev/zero of=$T/usb.img bs=1M count=32 status=none
python3 - $T/usb.img <<'EOF'
import struct, sys
f = open(sys.argv[1], 'r+b')
f.write(bytes(446) + bytes([0x80, 0, 0, 0, 0x06, 0, 0, 0]) + struct.pack('<II', 2048, 65536 - 2048)
        + bytes(48) + b'\x55\xAA')
EOF
mformat -i $T/usb.img@@1M -T 63488 -h 64 -s 32 -H 2048 -v USBDATA ::
printf 'Hola desde la memoria USB\r\n' > $T/HOLA.TXT
mcopy -i $T/usb.img@@1M $T/HOLA.TXT ::HOLA.TXT
{ sleep 7
  for c in "USBLBA" "TYPE X:\\HOLA.TXT" "COPY A:\\COMMAND.COM X:\\" "DIR X:\\" "ECHO fin-usblba"; do
    printf '%s\r' "$c"; sleep 3
  done
  sleep 2
} | timeout 50 qemu-system-i386 -display none -serial stdio -monitor none \
      -device qemu-xhci,id=xhci -drive if=none,id=u,format=raw,file=$T/usb.img \
      -device usb-storage,bus=xhci.0,drive=u \
      -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot > $T/log 2>&1
tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 > $T/log.txt
echo "== HSPUSB: límites del LBA en la ruta de DOS =="
for want in "Disco USB: 32 MB en la unidad X:" "USBLBA: sector 0 leido" "USBLBA: FFFFF800h rechazado" \
            "Hola desde la memoria USB" "COMMAND  COM" "fin-usblba"; do
  if grep -qF -- "$want" $T/log.txt; then echo "  OK   $want"; else echo "  FAIL $want"; fail=1; fi
done
dd if=$T/usb.img of=$T/part.img bs=1M skip=1 status=none
if fsck.fat -n $T/part.img > $T/fsck 2>&1; then echo "  OK   fsck.fat de la memoria USB"; else echo "  FAIL fsck.fat"; cat $T/fsck; fail=1; fi
if cmp -s <(mtype -i $T/part.img ::COMMAND.COM) <(mtype -i $T/f.img ::COMMAND.COM); then echo "  OK   COMMAND.COM copiado intacto"
else echo "  FAIL copia de COMMAND.COM"; fail=1; fi
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi
exit $fail
