#!/bin/bash
# Prueba de quitar y poner memorias USB sin reiniciar (HSPUSB.SYS, xHCI).
# 1) memoria UNO; 2) se quita: la unidad no está preparada; 3) se pone DOS:
# se lee y se escribe en ella; 4) se cambia DOS por TRES de golpe (sin pasar
# por «vacía»). Al final, fsck.fat de todas y UNO sin tocar.
# USB stick hot-swap test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
cp "$IMG" $T/f.img
printf 'DEVICE=A:\\HISPADOS\\HSPUSB.SYS\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
for n in UNO DOS TRES; do
  dd if=/dev/zero of=$T/$n.img bs=1M count=16 status=none
  mformat -i $T/$n.img -T 32768 -h 64 -s 32 -v MEM$n ::
  printf 'Memoria %s\r\n' $n > $T/$n.TXT
  mcopy -i $T/$n.img $T/$n.TXT ::$n.TXT
done
cp $T/UNO.img $T/UNO.orig
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; }
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
qemu-system-i386 -display none -serial file:$T/log -monitor unix:$T/mon,server,nowait -parallel none \
  -device qemu-xhci,id=xhci -drive if=none,id=u1,format=raw,file=$T/UNO.img \
  -device usb-storage,bus=xhci.0,drive=u1,id=m1 \
  -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot ${QTRACE:-} &
QP=$!
sleep 8
typ "DIR D:"; sleep 2
mon "device_del m1"; sleep 2
typ "DIR D:"; sleep 2
typ "A"; sleep 1
mon "drive_add 0 if=none,id=u2,format=raw,file=$T/DOS.img"
mon "device_add usb-storage,bus=xhci.0,drive=u2,id=m2"; sleep 3
typ "DIR D:"; sleep 3
typ "TYPE D:DOS.TXT"; sleep 2
typ "COPIAR A:\\HISPADOS\\EDITAR.COM D:"; sleep 3
mon "device_del m2"; sleep 1
mon "drive_add 0 if=none,id=u3,format=raw,file=$T/TRES.img"
mon "device_add usb-storage,bus=xhci.0,drive=u3,id=m3"; sleep 3
typ "DIR D:"; sleep 3
typ "TYPE D:TRES.TXT"; sleep 2
typ "MD D:\\NUEVA"; sleep 2
kill $QP 2>/dev/null; wait $QP 2>/dev/null
tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 > $T/log.txt
fail=0
check() { if grep -qF -- "$1" $T/log.txt; then echo "  OK   $1"; else echo "  FAIL $1"; fail=1; fi; }
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
echo "== Hispa-DOS cambio de memoria USB =="
check "El volumen de la unidad D es MEMUNO"
check "La unidad no está preparada"
check "El volumen de la unidad D es MEMDOS"
check "Memoria DOS"
check "El volumen de la unidad D es MEMTRES"
check "Memoria TRES"
if cmp -s <(mtype -i $T/DOS.img ::EDITAR.COM) build/EDIT.COM; then ok "EDITAR.COM copiado a la memoria DOS"; else ko "EDITAR.COM en la memoria DOS"; fi
if mdir -i $T/TRES.img ::NUEVA > /dev/null 2>&1; then ok "carpeta NUEVA creada en la memoria TRES"; else ko "carpeta NUEVA en la memoria TRES"; fi
if mdir -i $T/DOS.img ::NUEVA > /dev/null 2>&1; then ko "NUEVA en la memoria equivocada"; else ok "la memoria DOS no tiene NUEVA"; fi
if cmp -s $T/UNO.img $T/UNO.orig; then ok "la memoria UNO sigue intacta"; else ko "la memoria UNO ha cambiado"; fi
for n in UNO DOS TRES; do
  if fsck.fat -n $T/$n.img > /dev/null 2>&1; then ok "fsck.fat memoria $n"; else ko "fsck.fat memoria $n"; fi
done
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/A:\\>DIR D:/,$p' $T/log.txt; fi
rm -rf $T
exit $fail
