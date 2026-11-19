#!/bin/bash
# HSPXMS con un mapa de memoria E820 con huecos (como el MacBook Air 2012, que
# reserva 2 MB en 512 MB): E820SIM.SYS da el mapa de pega y XMSHUECO reserva
# toda la memoria XMS y mira que ningún bloque caiga en un hueco. También
# MEM y, sin el mapa de pega, que con el de QEMU todo siga en un solo trozo.
# XMS with a memory map with holes.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
fail=0
corre() {   # $1 = nombre, $2 = CONFIG.SYS
  cp "$IMG" $T/f.img
  printf "$2" > $T/CONFIG.SYS
  mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
  mcopy -o -i $T/f.img build/E820SIM.SYS ::E820SIM.SYS
  mcopy -o -i $T/f.img build/XMSHUECO.COM ::XMSHUECO.COM
  {
    sleep 5
    printf 'A:\\XMSHUECO\r'; sleep 4
    printf 'MEM\r'; sleep 2
  } | timeout 30 qemu-system-i386 -m 1G -display none -serial stdio -monitor none \
        -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot > $T/$1.log 2>&1
  tr -d '\r' < $T/$1.log | iconv -f CP437 -t UTF-8 > $T/$1.txt
}
yes() { if grep -qF -- "$2" $T/$1.txt; then echo "  OK   $1: $2"; else echo "  FAIL $1: $2"; fail=1; fi; }
no()  { if grep -qF -- "$2" $T/$1.txt; then echo "  FAIL $1: sobra $2"; fail=1; else echo "  OK   $1: sin $2"; fi; }
corre hueco 'DEVICE=A:\\E820SIM.SYS\r\nDEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\n'
corre qemu 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\n'
echo "== Hispa-DOS XMS con huecos en el mapa de memoria =="
yes hueco "E820SIM: mapa de memoria de pega"
# 1 MB + HMA - 512 MB, 514 MB - 768 MB + 16 KB y 768 MB + 20 KB - 1 GB - 128 KB
yes hueco "HSPXMS 0.1: gestor XMS 3.0, 1045308 KB de memoria extendida, en 3 trozos (la BIOS reserva 2052 KB en medio)"
yes hueco "XMSHUECO: ningún bloque en los huecos de la BIOS"
yes hueco "XMSHUECO: se ha podido reservar toda la memoria libre"
no  hueco "XMSHUECO: bloque en un hueco"
yes hueco "XMSHUECO: el mayor bloque acaba justo en el hueco de 512 MB"
yes hueco "XMSHUECO: ese bloque no crece dentro del hueco"
yes hueco "KB libres de memoria extendida (XMS)"
# con el mapa de QEMU (-m 1G): un solo trozo, como siempre
yes qemu "HSPXMS 0.1: gestor XMS 3.0, 1047360 KB de memoria extendida"
no  qemu "trozos"
# (en QEMU ahí hay memoria de verdad: XMSHUECO sabe ver un bloque en esas
# direcciones, que es lo que pasaba en el Mac cuando solo se miraba E801h)
yes qemu "XMSHUECO: bloque en un hueco de la BIOS en"
yes qemu "XMSHUECO: se ha podido reservar toda la memoria libre"
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/hueco.txt; echo ---; cat $T/qemu.txt; fi
rm -rf $T
exit $fail
