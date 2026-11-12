#!/bin/bash
# Casos límite de los drivers (revisión de noviembre):
#  1) HSPUSB y HSPCD no cambian FS/GS del programa que lee (usan el modo irreal);
#  2) HSPUSB rechaza una memoria con FAT32 sin particiones (no es FAT12/16)
#     en vez de dar al núcleo un BPB imposible;
#  3) HSPRAM no escribe más allá de la memoria que tiene: si el disco pedido
#     no cabe, lo dice y no se instala;
#  4) HSPUSB con páginas de trabajo («scratchpad») tomadas de la memoria XMS
#     (el xHCI de QEMU no las pide: se fuerza al ensamblar).
# Driver edge cases found in the November review.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
fail=0
T=$(mktemp -d)
nasm -f bin -o $T/FSGS.COM tests/progs/fsgs.asm || exit 1
check() { if grep -qF -- "$1" $T/log.txt; then echo "  OK   $1"; else echo "  FAIL $1"; fail=1; fi; }
checkn() { if grep -qF -- "$1" $T/log.txt; then echo "  FAIL (no debía salir) $1"; fail=1; else echo "  OK   sin \"$1\""; fi; }
conv() { tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 > $T/log.txt; }
show() { if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi; }
mkstick() {  # FAT16 sin particiones con HOLA.TXT
  dd if=/dev/zero of=$T/usb.img bs=1M count=32 status=none
  mformat -i $T/usb.img -T 65536 -h 64 -s 32 -v USBDATA ::
  head -c 2000 /dev/urandom > $T/HOLA.TXT
  mcopy -i $T/usb.img $T/HOLA.TXT ::HOLA.TXT
}

# ---- 1) FS/GS con USB ----
cp "$IMG" $T/f.img
mcopy -o -i $T/f.img $T/FSGS.COM ::BIN/FSGS.COM
printf 'DEVICE=A:\\HISPADOS\\HSPUSB.SYS /F\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
mkstick
{ sleep 7; printf 'A:\\BIN\\FSGS D:\\HOLA.TXT\r'; sleep 3; } |
  timeout 20 qemu-system-i386 -display none -serial stdio -monitor none \
    -device qemu-xhci,id=xhci -drive if=none,id=u,format=raw,file=$T/usb.img \
    -device usb-storage,bus=xhci.0,drive=u \
    -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot > $T/log 2>&1
conv
echo "== Hispa-DOS drivers: FS y GS tras leer de la memoria USB =="
check "Disco USB: 32 MB en la unidad D:"
check "FS y GS conservados"
show

# ---- 1b) FS/GS con el CD por AHCI ----
cp "$IMG" $T/f.img
mcopy -o -i $T/f.img $T/FSGS.COM ::BIN/FSGS.COM
printf 'DEVICE=A:\\HISPADOS\\HSPCD.SYS\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
mkdir -p $T/iso; head -c 3000 /dev/urandom > $T/iso/DATOS.BIN
genisoimage -quiet -V PRUEBA -o $T/cd.iso $T/iso 2>/dev/null
{ sleep 6; printf 'A:\\BIN\\FSGS C:\\DATOS.BIN\r'; sleep 3; } |
  timeout 20 qemu-system-i386 -display none -serial stdio -monitor none \
    -device ahci,id=ahci -drive if=none,id=cd,file=$T/cd.iso,media=cdrom -device ide-cd,bus=ahci.0,drive=cd \
    -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot > $T/log 2>&1
conv
echo "== Hispa-DOS drivers: FS y GS tras leer del CD (AHCI) =="
check "FS y GS conservados"
show

# ---- 2) memoria USB con FAT32 sin particiones ----
cp "$IMG" $T/f.img
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
printf 'DEVICE=A:\\HISPADOS\\HSPUSB.SYS /F\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
dd if=/dev/zero of=$T/usb.img bs=1M count=64 status=none
mkfs.fat -F 32 -n FAT32 $T/usb.img > /dev/null
{ sleep 7; printf 'ECHO sigue-vivo\r'; sleep 1; printf 'LISTAR D:\\\r'; sleep 2; printf 'ECHO y-sigue\r'; sleep 1; } |
  timeout 20 qemu-system-i386 -display none -serial stdio -monitor none \
    -device qemu-xhci,id=xhci -drive if=none,id=u,format=raw,file=$T/usb.img \
    -device usb-storage,bus=xhci.0,drive=u \
    -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot > $T/log 2>&1
conv
echo "== Hispa-DOS drivers: memoria USB con FAT32 =="
check "El disco USB no tiene un sistema de archivos FAT12/FAT16."
checkn "Disco USB: 64 MB en la unidad"
if grep -q "^sigue-vivo" $T/log.txt && grep -q "^y-sigue" $T/log.txt; then echo "  OK   el sistema sigue en marcha"; else echo "  FAIL el sistema no sigue en marcha"; fail=1; fi
show

# ---- 3) disco RAM que no cabe ----
cp "$IMG" $T/f.img
printf 'DEVICE=A:\\HISPADOS\\HSPRAM.SYS 400\r\nDEVICE=A:\\HISPADOS\\HSPRAM.SYS 400\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
{ sleep 5; printf 'ECHO sigue-vivo\r'; sleep 1; printf 'LISTAR C:\\\r'; sleep 2; } |
  timeout 14 qemu-system-i386 -display none -serial stdio -monitor none \
    -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot > $T/log 2>&1
conv
echo "== Hispa-DOS drivers: disco RAM que no cabe =="
check "Disco RAM de Hispa-DOS: 400 KB en la unidad C:"
check "Disco RAM de Hispa-DOS: no hay memoria para 400 KB; no se instala."
if grep -q "^sigue-vivo" $T/log.txt; then echo "  OK   el sistema arranca"; else echo "  FAIL el sistema no arranca"; fail=1; fi
check "El volumen de la unidad C es HSPRAMDISK"
show

# ---- 4) HSPUSB con páginas de trabajo en XMS ----
python3 tools/cp437.py drivers/hspusb.asm $T/hspusb.asm
if nasm -f bin -i build/gen/lib/ -i lib/ -DFORCE_SCRATCH=37 -o $T/HSPUSB.SYS $T/hspusb.asm; then
  for XMS in con sin; do
    cp "$IMG" $T/f.img
    mcopy -o -i $T/f.img $T/HSPUSB.SYS ::HISPADOS/HSPUSB.SYS
    if [ $XMS = con ]; then
      printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDEVICE=A:\\HISPADOS\\HSPUSB.SYS /F\r\n' > $T/CONFIG.SYS
    else
      printf 'DEVICE=A:\\HISPADOS\\HSPUSB.SYS /F\r\n' > $T/CONFIG.SYS
    fi
    mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
    mkstick
    { sleep 7; printf 'COPIAR D:\\HOLA.TXT A:\\\r'; sleep 3; printf 'A:\\BIN\\XMSTEST\r'; sleep 3; } |
      timeout 22 qemu-system-i386 -display none -serial stdio -monitor none \
        -device qemu-xhci,id=xhci -drive if=none,id=u,format=raw,file=$T/usb.img \
        -device usb-storage,bus=xhci.0,drive=u \
        -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot > $T/log 2>&1
    conv
    echo "== Hispa-DOS drivers: HSPUSB con 37 páginas de trabajo, $XMS HSPXMS =="
    if [ $XMS = con ]; then
      check "Disco USB: 32 MB en la unidad D:"
      if cmp -s <(mtype -i $T/f.img ::HOLA.TXT 2>/dev/null) $T/HOLA.TXT; then echo "  OK   fichero leído intacto"; else echo "  FAIL fichero leído"; fail=1; fi
      # 37 + 2 páginas + 4 KB de alineación = 160 KB bloqueados
      check "XMS test OK"
      check "XMS realloc OK"
    else
      check "El controlador xHCI necesita memoria extendida: cargue HSPXMS.SYS"
      check "No XMS driver"
    fi
    show
  done
else
  echo "  FAIL no se ensambla HSPUSB con FORCE_SCRATCH"; fail=1
fi
# ---- 5) con la línea A20 cerrada (un programa la abre y la cierra por XMS) ----
nasm -f bin -o $T/A20OFF.COM tests/progs/a20off.asm || exit 1
for DEV in usb ahci hda; do
  cp "$IMG" $T/f.img
  mcopy -o -i $T/f.img $T/A20OFF.COM ::BIN/A20OFF.COM
  if [ $DEV = usb ]; then
    printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDEVICE=A:\\HISPADOS\\HSPUSB.SYS /F\r\n' > $T/CONFIG.SYS
    mkstick
    QDEV="-device qemu-xhci,id=xhci -drive if=none,id=u,format=raw,file=$T/usb.img -device usb-storage,bus=xhci.0,drive=u"
    SRC='D:\HOLA.TXT'; REF=$T/HOLA.TXT; DST=HOLA.TXT
  elif [ $DEV = ahci ]; then
    printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDEVICE=A:\\HISPADOS\\HSPCD.SYS\r\n' > $T/CONFIG.SYS
    QDEV="-device ahci,id=ahci -drive if=none,id=cd,file=$T/cd.iso,media=cdrom -device ide-cd,bus=ahci.0,drive=cd"
    SRC='C:\DATOS.BIN'; REF=$T/iso/DATOS.BIN; DST=DATOS.BIN
  else
    printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\n' > $T/CONFIG.SYS
    QDEV="-audiodev wav,id=snd0,path=$T/out.wav -device intel-hda -device hda-output,audiodev=snd0"
  fi
  mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
  {
    sleep 7
    if [ $DEV = hda ]; then
      printf 'A:\\HISPADOS\\HSPSND /NOSB\r'; sleep 3
      printf 'A:\\BIN\\A20OFF\r'; sleep 2
      printf 'PLAYWAV /TONE 440 800\r'; sleep 4
    else
      printf 'A:\\BIN\\A20OFF\r'; sleep 2
      printf 'COPIAR %s A:\\\r' "$SRC"; sleep 4
    fi
    printf 'A:\\BIN\\A20OFF\r'; sleep 2
  } | timeout 26 qemu-system-i386 -display none -serial stdio -monitor none $QDEV \
        -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot > $T/log 2>&1
  conv
  echo "== Hispa-DOS drivers: $DEV con la línea A20 cerrada =="
  if [ "$(grep -c '^A20 cerrada' $T/log.txt)" = 2 ]; then echo "  OK   A20 cerrada antes y después"; else echo "  FAIL A20 cerrada antes y después"; fail=1; fi
  if [ $DEV = hda ]; then
    python3 tests/tools/audio.py check $T/out.wav > $T/audio.txt 2>&1
    if grep -qE "tramo .*: 4[34][0-9] Hz" $T/audio.txt; then echo "  OK   suena el tono de 440 Hz"; else echo "  FAIL no suena el tono"; cat $T/audio.txt; fail=1; fi
  else
    if cmp -s <(mtype -i $T/f.img ::$DST 2>/dev/null) $REF; then echo "  OK   fichero leído intacto"; else echo "  FAIL fichero leído"; fail=1; fi
  fi
  show
done
rm -rf $T
exit $fail
