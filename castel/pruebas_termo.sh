# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pruebas_termo.sh - la parte 46 (SOLO=termo: el Termómetro, TERMO.BIN,
# nucleo32/termo/, y su memoria USB: «TERMO=1 usb.sh») de pruebas.sh, que lee
# este archivo («.»): la función parte_termo, con los ayudantes de
# pruebas.sh (ok, ko, mira) y sus carpetas ($AQUI, $P).
# QEMU no da temperaturas del procesador: la zona térmica de mentira de
# tests/acpi/termo-qemu.py (-acpitable) lee sus grados de la CMOS (63h), que
# se cambia por el monitor. Se arranca la memoria de verdad (GRUB, xHCI):
#  1. -cpu Haswell: el Termómetro se abre solo; dice que el procesador no da
#     temperatura; la zona y el ventilador de ACPI y el SMBIOS; «Calentar»
#     con la zona a 91 °C (de 100): la parada de seguridad, el enfriamiento y
#     el resumen (.TXT); el CSV y el .LOG con sus sucesos.
#  2. Calentando, la máquina se mata sin apagar (kill -9): la FAT, sana
#     (fsck.fat) y el .LOG y el .CSV con sus últimas líneas de la carga.
#  3. Otra vez, con KVM y el procesador del anfitrión si hay /dev/kvm (si no,
#     -cpu phenom): el aviso de que la vez anterior se apagó durante la
#     prueba, con su temperatura; y el MSR que no está (la excepción 13 la
#     recoge el núcleo: el rdmsr «seguro»).
# Necesita ser root (usb.sh), qemu-system-x86_64, socat, mtools y fsck.fat.

# termo_qemu IMAGEN NOMBRE SEGUNDOS "CPU" "t:orden del monitor"...
termo_qemu() {
  local img=$1 n=$2 seg=$3 cpu=$4 t=0 o w q; shift 4
  rm -f $Y/$n.mon
  # shellcheck disable=SC2086
  qemu-system-x86_64 $cpu -nic none -m 512 -display none -monitor unix:$Y/$n.mon,server,nowait -serial file:$Y/$n.sal -no-reboot \
    -acpitable file=$Y/termo.aml -device qemu-xhci -drive file=$img,format=raw,if=none,id=u -device usb-storage,drive=u,bootindex=1 > /dev/null 2>&1 &
  q=$!
  QPID=$q
  sleep 1
  printf 'o /b 0x70 0x63\no /b 0x71 45\n' | socat - UNIX-CONNECT:$Y/$n.mon > /dev/null 2>&1
  for o in "$@"; do
    w=${o%%:*}
    while [ $t -lt $w ]; do sleep 1; t=$((t + 1)); done
    printf '%s\n' "${o#*:}" | socat - UNIX-CONNECT:$Y/$n.mon > /dev/null 2>&1
  done
  while [ $t -lt $seg ]; do sleep 1; t=$((t + 1)); done
  kill -9 $q 2> /dev/null
  wait $q 2> /dev/null
  QPID=
  tr -d '\r' < $Y/$n.sal | iconv -f CP437 -t UTF-8 > $Y/$n.txt 2> /dev/null || tr -d '\r' < $Y/$n.sal > $Y/$n.txt
  S=$Y/$n.txt
}

parte_termo() {
echo "== (TERMO) Hispa-Pentium: el Termómetro (TERMO.BIN) y su memoria USB =="
local Y=$P/termo S C="o /b 0x70 0x63" L
mkdir -p $Y
export MTOOLS_SKIP_CHECK=1
if [ "$(id -u)" != 0 ] || ! command -v qemu-system-x86_64 > /dev/null; then echo "  (sin ser root o sin qemu-system-x86_64: sin el Termómetro)"; return; fi
python3 -I $AQUI/../tests/acpi/termo-qemu.py $Y/termo.aml
if TERMO=1 USB_MB=64 bash $AQUI/nucleo32/usb.sh $Y/termo.img > $Y/usb.txt 2>&1 && mtype -i $Y/termo.img@@1M ::boot/grub/grub.cfg | grep -q "escritorio .*termo"; then
  ok "usb.sh con TERMO=1: la memoria de 64 MB con TERMO.BIN y el menú que arranca el escritorio con «termo»"
else ko "usb.sh con TERMO=1: $(tail -n 2 $Y/usb.txt)"; return; fi
# 1. la parada de seguridad
termo_qemu $Y/termo.img t1 150 "-cpu Haswell" "44:sendkey 1" "46:sendkey c" "58:$C" "58:o /b 0x71 91" "70:$C" "70:o /b 0x71 50"
mira "se abre solo al arrancar; el procesador emulado no da temperatura y se dice; la zona de ACPI, su ventilador y el SMBIOS" \
  "escritorio: lanzo .TERMO.BIN" "termo: procesador «Intel Core Processor (Haswell)»" \
  "termo: suceso: sensor «Zona ACPI ._TZ_.TZ01»: 45,0 °C (límite 100,0 °C)" "termo: suceso: ventilador 1: 2400 r.p.m." "equipo «QEMU "
mira "Calentar (5 min): la tarea hija carga; a 91 °C de 100, la parada de seguridad; luego el enfriamiento y el resumen" \
  "termo: calentar 300 s (la carga en una tarea hija)" "termo: carga en marcha" "termo: fin de la carga a los [0-9]* s (PARADA DE SEGURIDAD); pico 91,0 °C" \
  "termo: carga parada" "termo: resumen: PARADA DE SEGURIDAD" "PELIGRO: se calienta demasiado"
L=$(mdir -b -i $Y/termo.img@@1M ::TERMO 2> /dev/null | tr -d ' ')
if echo "$L" | grep -q 'LOG$' && echo "$L" | grep -q 'CSV$' && echo "$L" | grep -q 'TXT$' \
   && mtype -i $Y/termo.img@@1M $(echo "$L" | grep 'CSV$') | grep -q '^[0-9/]*;[0-9:]*;[0-9]*;carga;91,0;' \
   && mtype -i $Y/termo.img@@1M $(echo "$L" | grep 'LOG$') | grep -q 'SUCESO ACABA la carga .*PARADA DE SEGURIDAD' \
   && mtype -i $Y/termo.img@@1M $(echo "$L" | grep 'LOG$') | grep -q ' ARRANQUE '; then
  ok "en la memoria, \\TERMO: el CSV (91,0 en la carga), el .LOG (el arranque, los sucesos) y el resumen .TXT"
else ko "\\TERMO de la memoria: $(echo $L)"; fi
# 2. el apagón, calentando
mdeltree -i $Y/termo.img@@1M ::TERMO > /dev/null 2>&1
termo_qemu $Y/termo.img t2 70 "-cpu Haswell" "46:sendkey c" "56:$C" "56:o /b 0x71 80"
dd if=$Y/termo.img of=$Y/part.img bs=1M skip=1 status=none
L=$(mdir -b -i $Y/part.img ::TERMO 2> /dev/null | tr -d ' ')
if fsck.fat -n $Y/part.img > $Y/fsck.txt 2>&1 && mtype -i $Y/part.img $(echo "$L" | grep 'LOG$') | tail -n 1 | grep -q 'DATOS fase=carga max=80,0' \
   && mtype -i $Y/part.img $(echo "$L" | grep 'CSV$') | tail -n 1 | grep -q ';carga;80,0;'; then
  ok "el equipo se apaga de golpe calentando: la FAT sana y el .LOG y el .CSV acaban en la carga, a 80 °C"
else ko "el apagón: $(tail -n 2 $Y/fsck.txt | tr '\n' ' ')"; fi
# 3. la vez siguiente
local cpu="-cpu phenom"
[ -w /dev/kvm ] && cpu="-enable-kvm -cpu host"
termo_qemu $Y/termo.img t3 50 "$cpu"
mira "al arrancar otra vez: «la última vez se apagó durante la prueba, a 80 °C»; el MSR que no está, recogido ($cpu)" \
  "termo: aviso: La última vez .*durante la PRUEBA DE ESFUERZO (calentando), a las [0-9:]*, con el más caliente a 80,0 °C" \
  "termo: MSR 19Ch sin sensor: "
[ -w /dev/kvm ] && mira "con KVM: el MSR 19Ch, que este procesador no tiene, da la excepción 13 y el núcleo la recoge" "termo: MSR 19Ch sin sensor: no lo tiene (lectura recogida por el núcleo)"
}
