#!/bin/bash
# Prueba del USB nativo (HSPUSB.SYS): controlador xHCI de QEMU con una memoria
# USB (FAT16 sin particiones y, en otra pasada, con tabla de particiones).
# Se lee, se escribe y después se comprueba la memoria con fsck.fat y mtools.
# Native USB test: read and write a USB stick through our own xHCI driver.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
fail=0
for KIND in super mbr; do
  T=$(mktemp -d)
  cp "$IMG" $T/f.img
  printf 'DEVICE=A:\\HISPADOS\\HSPUSB.SYS /F\r\n' > $T/CONFIG.SYS
  mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
  dd if=/dev/zero of=$T/usb.img bs=1M count=32 status=none
  if [ $KIND = super ]; then
    OFF=0
    mformat -i $T/usb.img -T 65536 -h 64 -s 32 -v USBDATA ::
  else
    OFF=1048576
    python3 - $T/usb.img <<'EOF'
import struct, sys
f = open(sys.argv[1], 'r+b')
f.write(bytes(446) + bytes([0x80, 0, 0, 0, 0x06, 0, 0, 0]) + struct.pack('<II', 2048, 65536 - 2048)
        + bytes(48) + b'\x55\xAA')
EOF
    mformat -i $T/usb.img@@$OFF -T 63488 -h 64 -s 32 -H 2048 -v USBDATA ::
  fi
  printf 'Hola desde la memoria USB\r\n' > $T/HOLA.TXT
  mcopy -i $T/usb.img@@$OFF $T/HOLA.TXT ::HOLA.TXT
  {
    sleep 7
    for c in "MOSTRAR D:\\HOLA.TXT" "CREARDIR D:\\HISPA" "COPIAR A:\\HISPADOS\\*.COM D:\\HISPA" \
             "COPIAR A:\\README.TXT D:\\" "BORRAR D:\\HOLA.TXT" "LISTAR D:\\HISPA"; do
      printf '%s\r' "$c"; sleep 3
    done
    sleep 2
  } | timeout 50 qemu-system-i386 -display none -serial stdio -monitor none \
        -device qemu-xhci,id=xhci -drive if=none,id=u,format=raw,file=$T/usb.img \
        -device usb-storage,bus=xhci.0,drive=u \
        -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot > $T/log 2>&1
  tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 > $T/log.txt
  echo "== Hispa-DOS native USB tests ($KIND) =="
  for want in "HSPUSB 0.1 - USB nativo de Hispa-DOS (xHCI)" "Disco USB: 32 MB en la unidad D:" \
              "Hola desde la memoria USB" " Directorio de D:\\HISPA" "EDITAR   COM" "TECLADO  COM"; do
    if grep -qF -- "$want" $T/log.txt; then echo "  OK   $want"; else echo "  FAIL $want"; fail=1; fi
  done
  P=$T/part.img
  dd if=$T/usb.img of=$P bs=1M skip=$((OFF / 1048576)) status=none
  if fsck.fat -n $P > $T/fsck 2>&1; then echo "  OK   fsck.fat de la memoria USB"; else echo "  FAIL fsck.fat"; cat $T/fsck; fail=1; fi
  if cmp -s <(mtype -i $P ::HISPA/EDITAR.COM) build/EDIT.COM; then echo "  OK   EDITAR.COM copiado intacto"; else echo "  FAIL copia de EDITAR.COM"; fail=1; fi
  if mdir -i $P ::HOLA.TXT > /dev/null 2>&1; then echo "  FAIL HOLA.TXT no se borró"; fail=1; else echo "  OK   HOLA.TXT borrado"; fi
  if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi
  rm -rf $T
done
exit $fail
