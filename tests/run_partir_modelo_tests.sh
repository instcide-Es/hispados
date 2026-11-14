#!/bin/bash
# PARTIR y el modelo de los discos (fallo del Toshiba NB250: «PARTIR» se
# quedaba congelado tras el título). El modelo se pregunta con IDENTIFY por
# los puertos que da la BIOS (EDD, la DPTE); en ese equipo la controladora SATA
# está en modo AHCI y los puertos no contestaban: la espera acababa por tiempo,
# pero la rutina del reloj devolvía ES cambiado y el MBR se leía (y se
# escribía) en otra parte de la memoria. Ahora:
#   1) /IDE:1F0,3F6,10: preguntar por un canal sin disco (el esclavo, que no
#      existe): avisa «Esc lo salta», acaba por tiempo, sigue sin modelo y la
#      tabla de particiones se lee bien (antes: «No hay particiones»); el
#      sistema sigue funcionando.
#   2) Sin opciones: el disco IDE del PCI (canal compatible 1F0h, que cuadra
#      con la controladora) sí da su modelo.
#   3) /SINMODELO: no se pregunta nada.
#   4) Un disco SATA en una controladora AHCI (q35, ich9-ahci): no se tocan
#      puertos IDE y la tabla se lee bien.
# PARTIR and the disk model query (timeouts, port validation, /SINMODELO).
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$2" "$1"; then ok "$3$2"; else ko "$3$2"; fi; }
checkn() { if grep -qF -- "$2" "$1"; then ko "$3(no debe salir) $2"; else ok "$3no sale: $2"; fi; }
send() { sleep "${2:-1.5}"; printf '%s\r' "$1"; }
part() { awk -v a="$2" -v b="$3" 'index($0, a) {p=1; next} p && index($0, b) {exit} p' "$1"; }
cp "$IMG" $T/f.img
dd if=/dev/zero of=$T/hdd.img bs=1M count=64 status=none
echo "63,40000,6,*" | sfdisk -q $T/hdd.img > /dev/null 2>&1
cp $T/hdd.img $T/sata.img
{
  sleep 4
  send "ECHO =M1=" 1
  send "PARTIR /IDE:1F0,3F6,10 /STATUS" 1
  send "ECHO =M2=" 5
  send "PARTIR /STATUS" 1
  send "ECHO =M3=" 3
  send "PARTIR /SINMODELO /STATUS" 1
  send "ECHO =M4=" 3
  send "DIR A:\\HISPADOS\\PARTIR.COM" 1
  send "ECHO =M5=" 2
} | timeout 50 qemu-system-i386 -display none -serial stdio -monitor none -no-reboot \
    -drive file=$T/f.img,format=raw,if=floppy -drive file=$T/hdd.img,format=raw,if=ide,index=0 -boot a > $T/l1 2>&1
tr -d '\r' < $T/l1 | iconv -f CP437 -t UTF-8 > $T/l1.txt
echo "== PARTIR: el modelo de los discos =="
part $T/l1.txt "=M1=" "=M2=" > $T/m1
check $T/m1 "(Leyendo el modelo de los discos; Esc lo salta)" "canal sin disco: "
check $T/m1 "Disco actual: 1/1  (64 MB, Disco duro)" "canal sin disco: "
check $T/m1 "PRI DOS  FAT16" "canal sin disco: la tabla se lee bien: "
checkn $T/m1 "No hay particiones definidas." "canal sin disco: "
part $T/l1.txt "=M2=" "=M3=" > $T/m2
check $T/m2 "Disco actual: 1/1  (64 MB, QEMU HARDDISK)" "IDE del PCI: "
checkn $T/m2 "Esc lo salta" "IDE del PCI: "
part $T/l1.txt "=M3=" "=M4=" > $T/m3
check $T/m3 "Disco actual: 1/1  (64 MB, Disco duro)" "/SINMODELO: "
check $T/m3 "PRI DOS  FAT16" "/SINMODELO: "
part $T/l1.txt "=M4=" "=M5=" > $T/m4
check $T/m4 "PARTIR   COM" "después, el sistema sigue bien: "
{
  sleep 5
  send "ECHO =N1=" 1
  send "PARTIR /STATUS" 1
  send "ECHO =N2=" 4
} | timeout 40 qemu-system-i386 -M q35 -display none -serial stdio -monitor none -no-reboot \
    -drive file=$T/f.img,format=raw,if=floppy \
    -drive if=none,id=sd,format=raw,file=$T/sata.img -device ide-hd,drive=sd,bus=ide.0 -boot a > $T/l2 2>&1
tr -d '\r' < $T/l2 | iconv -f CP437 -t UTF-8 > $T/l2.txt
part $T/l2.txt "=N1=" "=N2=" > $T/n1
check $T/n1 "Disco actual: 1/1  (64 MB" "AHCI: "
check $T/n1 "PRI DOS  FAT16" "AHCI: "
checkn $T/n1 "Esc lo salta" "AHCI: "
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/l1.txt; echo ---; cat $T/l2.txt; fi
rm -rf $T
exit $fail
