# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pruebas_uefi.sh - la parte 48 de pruebas.sh (SOLO=uefi): (UEFI) arrancar
# Hispa-Pentium por UEFI, que pruebas.sh lee («.»): la función parte_uefi,
# con los ayudantes de pruebas.sh (ok, ko, mira, miracap) y sus carpetas
# ($AQUI, $P, $G).
#
# Con QEMU y OVMF (el firmware UEFI de QEMU: el paquete ovmf, y ovmf-ia32 para
# el de 32 bits), y GRUB EFI (grub-efi-amd64-bin, grub-efi-ia32-bin):
# 1. la memoria USB (usb.sh) lleva \EFI\BOOT\BOOTX64.EFI (y BOOTIA32.EFI si
#    hay GRUB de 32 bits), LEEME-UEFI.TXT y el menú de las dos maneras;
# 2. por UEFI de 64 bits (q35, con la memoria en el xHCI y un disco SATA,
#    AHCI): el núcleo arranca por multiboot2, el escritorio a la medida que
#    deja el firmware (GOP), ACPI.BIN con la RSDP que le da el núcleo (la de
#    GRUB) y la batería de la SSDT de prueba, el disco SATA (C:) y la memoria
#    USB (X:); y la pantalla en Configuración no se cambia en marcha;
# 3. la consola del núcleo (modo texto) por UEFI: pintada en la pantalla del
#    firmware (textouefi.cas), con lo que dice al arrancar;
# 4. por UEFI de 32 bits (como el Acer One 10), si hay OVMF32 y GRUB ia32;
# 5. la misma memoria por la BIOS (SeaBIOS): como siempre, por multiboot;
# 6. el CD (cd.sh) por UEFI: sin instalar, al escritorio.
# Sin OVMF o sin GRUB EFI, lo dice y no prueba nada (no es un fallo).

# como «maquina» de la parte del CD, con el QEMU que se diga (el de 64 bits
# para el OVMF de 64), «:tecla» y «:texto_uefi» (el texto de 80 x 25 de la
# consola pintada, del marco que dice el núcleo, a NOMBRE.txt)
maquina_uefi() {
  local q=$1 n=$2 o=$3 p i s x m rx=400 ry=300 sx sy dx dy
  shift 3
  rm -f $V/mon
  # shellcheck disable=SC2086
  $q $o -display none -monitor unix:$V/mon,server,nowait -serial file:$V/$n.sal -rtc base=localtime > $V/$n.qemu 2>&1 &
  QPID=$!
  for ((i = 0; i < 40; i++)); do [ -S $V/mon ] && break; sleep 0.1; done
  for p in "$@"; do
    case "$p" in
      ":pausa "*) sleep ${p:7} ;;
      ":tecla "*) for k in ${p:7}; do printf 'sendkey %s\n' $k | socat - UNIX-CONNECT:$V/mon > /dev/null 2>&1; sleep 0.15; done ;;
      ":monitor "*) printf '%s\n' "${p:9}" | socat - UNIX-CONNECT:$V/mon > /dev/null 2>&1; sleep 0.3 ;;
      ":captura "*) printf 'screendump %s\n' $G/${p:9}.ppm | socat - UNIX-CONNECT:$V/mon > /dev/null 2>&1; sleep 0.6 ;;
      ":texto_uefi "*) m=$(tr -d '\r' < $V/$n.sal | sed -n 's/^uefi: el texto, en el marco 0x\([0-9A-F]*\)$/\1/p' | head -1)
        if [ -n "$m" ]; then
          printf 'pmemsave 0x%s 4000 "%s"\n' $m $V/p.bin | socat - UNIX-CONNECT:$V/mon > /dev/null 2>&1; sleep 0.4
          python3 -I -c "
import sys
d = open(sys.argv[1], 'rb').read()
for f in range(25): print('|' + bytes(d[f * 160:(f + 1) * 160:2]).decode('cp437').rstrip())" $V/p.bin > $V/${p:12}.txt
        else : > $V/${p:12}.txt; fi ;;
      ":raton "*|":clic "*) set -- ${p#:* }; dx=$(($1 - rx)); dy=$(($2 - ry))
        while [ $dx -ne 0 ] || [ $dy -ne 0 ]; do
          sx=$dx; [ $sx -gt 50 ] && sx=50; [ $sx -lt -50 ] && sx=-50
          sy=$dy; [ $sy -gt 50 ] && sy=50; [ $sy -lt -50 ] && sy=-50
          printf 'mouse_move %s %s\n' $sx $sy | socat - UNIX-CONNECT:$V/mon > /dev/null 2>&1; sleep 0.05
          dx=$((dx - sx)); dy=$((dy - sy))
        done
        rx=$1; ry=$2; sleep 0.25
        if [ "${p:0:5}" = ":clic" ]; then
          printf 'mouse_button 1\n' | socat - UNIX-CONNECT:$V/mon > /dev/null 2>&1; sleep 0.2
          printf 'mouse_button 0\n' | socat - UNIX-CONNECT:$V/mon > /dev/null 2>&1; sleep 0.4
        fi ;;
      ":espera "*) s=${p:8}; s=${s%% *}; x=${p#:espera $s }
        for ((i = 0; i < s * 2; i++)); do tr -d '\r' < $V/$n.sal | iconv -f CP437 -t UTF-8 2> /dev/null | grep -q -- "$x" && break; sleep 0.5; done ;;
    esac
  done
  printf 'quit\n' | socat - UNIX-CONNECT:$V/mon > /dev/null 2>&1
  wait $QPID 2> /dev/null
  QPID=
  tr -d '\r' < $V/$n.sal | iconv -f CP437 -t UTF-8 > $V/$n.txt 2> /dev/null
  S=$V/$n.txt
}

parte_uefi() {
echo "== Hispa-Pentium: arrancar por UEFI (GRUB EFI, multiboot2, OVMF) (UEFI) =="
local V=$P/uefi OV=/usr/share/OVMF F64 F32 SSDT PUESTA i
V=$P/uefi
rm -rf $V; mkdir -p $V $G
export MTOOLS_SKIP_CHECK=1
F64=$OV/OVMF_CODE_4M.fd
F32=$OV/OVMF32_CODE_4M.fd
if [ ! -f /usr/lib/grub/x86_64-efi/multiboot2.mod ] || [ ! -f $F64 ] || ! command -v qemu-system-x86_64 > /dev/null; then
  echo "  (sin GRUB EFI de 64 bits (grub-efi-amd64-bin), OVMF (ovmf) o qemu-system-x86_64 no se prueba el arranque por UEFI)"
  return 0
fi
if [ "$(id -u)" != 0 ]; then echo "  (sin ser root no se hace la memoria USB: sin la parte de UEFI)"; return 0; fi
# 1. la memoria USB
if bash $AQUI/nucleo32/usb.sh $V/usb.img > $V/usb.txt 2>&1 && grep -q "arranca también por UEFI: x86_64-efi" $V/usb.txt; then
  ok "usb.sh hace la memoria USB que arranca también por UEFI ($(sed -n 's/^usb.sh: (UEFI) arranca también por UEFI: //p' $V/usb.txt))"
else ko "usb.sh no hace la memoria con UEFI:"; tail -n 4 $V/usb.txt | sed 's/^/        /'; return 0; fi
mdir -b -i $V/usb.img@@1M ::EFI/BOOT > $V/efi.txt 2>&1
mcopy -n -i $V/usb.img@@1M ::boot/grub/grub.cfg $V/grub.cfg 2> /dev/null
if grep -q 'BOOTX64.EFI' $V/efi.txt && mdir -i $V/usb.img@@1M ::LEEME-UEFI.TXT > /dev/null 2>&1 && mdir -i $V/usb.img@@1M ::boot/grub/i386-pc/normal.mod > /dev/null 2>&1 \
  && grep -q '^if \[ "$grub_platform" = efi \]; then$' $V/grub.cfg && grep -q '^  multiboot2 /ESCRIT8.BIN escritorio' $V/grub.cfg && grep -q '^  module2 /ORDENES.BIN ORDENES.BIN$' $V/grub.cfg \
  && grep -q '^  multiboot /ESCRIT8.BIN escritorio' $V/grub.cfg; then
  ok "en la misma partición FAT (tabla del MBR): \\EFI\\BOOT\\BOOTX64.EFI$(grep -q BOOTIA32 $V/efi.txt && echo ' y BOOTIA32.EFI'), LEEME-UEFI.TXT, el GRUB de la BIOS; y el menú, con multiboot2/module2 por UEFI y multiboot por la BIOS"
else ko "a la memoria le falta algo de UEFI:"; cat $V/efi.txt | sed 's/^/        /' | head -5; fi
if grep -q 'Secure Boot' $AQUI/nucleo32/uefi/LEEME-UEFI.TXT && grep -q 'Disabled' $AQUI/nucleo32/uefi/LEEME-UEFI.TXT && grep -q 'volver a encender' $AQUI/nucleo32/uefi/LEEME-UEFI.TXT; then
  ok "LEEME-UEFI.TXT dice que hay que apagar el Secure Boot, cómo, y que se puede volver a encender"
else ko "LEEME-UEFI.TXT no dice lo del Secure Boot"; fi
SSDT="-acpitable file=$AQUI/../tests/acpi/bateria-qemu.aml"
PUESTA=(":monitor o /b 0x70 0x60" ":monitor o /b 0x71 0x37" ":monitor o /b 0x70 0x61" ":monitor o /b 0x71 0x02" ":monitor o /b 0x70 0x62" ":monitor o /b 0x71 0x00")
truncate -s 64M $V/sata.img
mkfs.fat -F 16 -n SATAUEFI $V/sata.img > /dev/null
printf 'hola desde el disco SATA\r\n' > $V/HOLA.TXT
mcopy -i $V/sata.img $V/HOLA.TXT ::/
# 2. por UEFI de 64 bits: el escritorio
cp $OV/OVMF_VARS_4M.fd $V/vars64.fd
cp $V/usb.img $V/u1.img
# (OVMF pone las zonas PCI de 64 bits, como la del xHCI de QEMU, por encima
# de los 4 GB, y el núcleo, de 32 bits, no las usa: sin su ventana de 64
# bits, abajo, como en casi todos los PC de verdad; PENTIUM.md, «UEFI»)
local SIN64="-fw_cfg name=opt/ovmf/X-PciMmio64Mb,string=0"
local Q64="-machine q35 -m 512 $SIN64 -drive if=pflash,format=raw,readonly=on,file=$F64 -drive if=pflash,format=raw,file=$V/vars64.fd $SSDT"
local DISCOS="-device qemu-xhci,id=x -drive if=none,id=u,file=$V/u1.img,format=raw -device usb-storage,bus=x.0,drive=u -drive if=none,id=s,file=$V/sata.img,format=raw -device ide-hd,drive=s,bus=ide.0"
maquina_uefi qemu-system-x86_64 e64 "$Q64 $DISCOS -S" "${PUESTA[@]}" ":monitor cont" ":espera 90 ^escritorio: en marcha" ":pausa 4" ":espera 30 ^Lectura 1:" ":captura uefi1"
mira "por UEFI de 64 bits (OVMF, q35): GRUB EFI arranca la primera entrada por multiboot2 y el escritorio se pone a la medida del firmware (GOP)" \
  "^escritorio: en marcha, [0-9]* x [0-9]*"
local AN AL
AN=$(sed -n 's/^escritorio: en marcha, \([0-9]*\) x \([0-9]*\).*/\1/p' $S | head -1)
AL=$(sed -n 's/^escritorio: en marcha, \([0-9]*\) x \([0-9]*\).*/\2/p' $S | head -1)
if [ -f $G/uefi1.ppm ] && [ "$(head -c 20 $G/uefi1.ppm | sed -n 2p)" = "$AN $AL" ]; then
  ok "y la pantalla de QEMU mide eso mismo: $AN x $AL (la que dejó el GOP)"
else ko "la captura no mide $AN x $AL: $(head -c 20 $G/uefi1.ppm 2> /dev/null | sed -n 2p)"; fi
miracap "y se ve el escritorio: Mi equipo y el botón de Inicio" uefi1 texto 10 30 140 90 "Mi equipo" negrita 20 $((AL - 30)) 90 $((AL - 8)) Inicio
mira "ACPI.BIN, con la RSDP que le da el núcleo (la copia de GRUB: sin BIOS no está en su zona), lee las tablas de OVMF y la batería de la SSDT de prueba" \
  "^acpi: el servicio de ACPI (ACPI.BIN) en marcha" "^ACPI: RSDP en [0-9A-F]*h" "^ACPI: SSDT .*HISPA  BATPRUEB" "^Lectura 1: 55 % (2750 de 5000 mAh)"
if grep -q "rsdp=" $S || grep -q "^ACPI: RSDP en 000[EF]" $S; then
  if grep -q "^ACPI: RSDP en 000[EF]" $S; then ko "la RSDP sale de la zona de la BIOS (E0000h-FFFFFh), no de GRUB"; else ok "(la RSDP no es la de la zona de la BIOS)"; fi
else ok "(la RSDP no es la de la zona de la BIOS)"; fi
mira "los discos: el SATA (AHCI) y la memoria USB (xHCI), que es X:, la del sistema" "^usb: controladora xHCI" "QEMU HARDDISK, 64 MB, FAT16 (SATA)" \
  "QEMU HARDDISK, 64 MB, FAT16 (USB)" "^unidades: .*C: (disco [0-9], SATA, FAT16).*X: (disco [0-9], USB, FAT16); la del sistema, X:"
if ! grep -q "excepción\|\*\*\*" $S; then ok "por UEFI, ni una excepción ni una queja del núcleo"; else ko "por UEFI, el núcleo se queja:"; grep "excepción\|\*\*\*" $S | head -3 | sed 's/^/        /'; fi
# 2b. la medida guardada (PANTALLA.ENV: 800 x 600) se la pide GRUB al
# firmware; y Configuración > Pantalla: no se cambia en marcha, y lo dice
cp $OV/OVMF_VARS_4M.fd $V/vars64.fd
cp $V/usb.img $V/u1.img
grub-editenv $V/PANTALLA.ENV create && grub-editenv $V/PANTALLA.ENV set pantalla=800x600 antes= prueba=0 && mcopy -o -i $V/u1.img@@1M $V/PANTALLA.ENV ::/
maquina_uefi qemu-system-x86_64 conf "$Q64 $DISCOS" ":espera 90 ^escritorio: en marcha" ":pausa 4" ":clic 40 585" ":clic 300 238" ":raton 700 470" ":clic 336 230" ":pausa 2" ":captura uefi6"
mira "la medida guardada en PANTALLA.ENV (800 x 600) se la pide GRUB al firmware (GOP) al arrancar" "^escritorio: en marcha, 800 x 600 con 32 bits"
miracap "Configuración > Pantalla, por UEFI: no se cambia en marcha (aunque QEMU tenga la VBE de Bochs), lo dice, y se guarda para el próximo arranque" uefi6 \
  texto 150 160 175 180 "Arrancado por UEFI: no se cambia en marcha, la pone el" texto 150 178 175 198 "firmware al arrancar. Se guarda para el próximo arranque."
# 3. la consola del núcleo, en modo texto, por UEFI: la entrada 9 del menú
cp $OV/OVMF_VARS_4M.fd $V/vars64.fd
cp $V/usb.img $V/u1.img
maquina_uefi qemu-system-x86_64 t64 "$Q64 $DISCOS" ":espera 40 automatically" ":pausa 1" ":tecla down down down down down down down down ret" \
  ":espera 60 ^Escriba «ayuda»" ":pausa 2" ":texto_uefi t64" ":captura uefi2"
mira "la consola del núcleo por UEFI: sin modo de texto, se pinta en la pantalla del firmware (textouefi.cas)" \
  "^uefi: sin modo de texto: la consola se pinta en la pantalla del firmware" "^uefi: el texto, en el marco 0x" "^Escriba «ayuda»"
if grep -q "Escriba «ayuda»" $V/t64.txt 2> /dev/null && grep -q "FAT16 (SATA)" $V/t64.txt; then
  ok "y el texto de la consola (80 x 25, en su marco de RAM) dice los discos y «Escriba «ayuda».»"
else ko "el texto de la consola no está en su marco:"; grep -v '^|$' $V/t64.txt 2> /dev/null | head -5 | sed 's/^/        /'; fi
# (a 1280 x 800, las letras van al doble: 16 x 32; se mira la última línea, «> »)
if [ -f $G/uefi2.ppm ] && python3 -I - $G/uefi2.ppm <<'PY'
import sys
d = open(sys.argv[1], 'rb').read()
cab = d.split(b'\n', 3)
an, al = map(int, cab[1].split())
px = cab[3]
# algo gris (AAAAAA: el texto de la consola) en la pantalla
gris = sum(1 for i in range(0, len(px), 3 * 7) if px[i:i + 3] == b'\xaa\xaa\xaa')
sys.exit(0 if gris > 200 else 1)
PY
then ok "y se ve pintado en la pantalla gráfica del firmware (letras grises, AAAAAAh, sobre negro)"; else ko "la consola no se ve en la pantalla (uefi2)"; fi
# 4. por UEFI de 32 bits (los Bay Trail como el Acer One 10)
if [ -f $F32 ] && [ -f /usr/lib/grub/i386-efi/multiboot2.mod ]; then
  cp $OV/OVMF32_VARS_4M.fd $V/vars32.fd
  cp $V/usb.img $V/u2.img
  maquina_uefi qemu-system-i386 e32 "-machine q35 -m 512 -drive if=pflash,format=raw,readonly=on,file=$F32 -drive if=pflash,format=raw,file=$V/vars32.fd $SSDT -device qemu-xhci,id=x -drive if=none,id=u,file=$V/u2.img,format=raw -device usb-storage,bus=x.0,drive=u" \
    ":espera 90 ^escritorio: en marcha" ":pausa 3" ":captura uefi3"
  mira "por UEFI de 32 bits (OVMF ia32, BOOTIA32.EFI): el escritorio" "^escritorio: en marcha" "^acpi: el servicio de ACPI (ACPI.BIN) en marcha"
else
  echo "  (sin OVMF de 32 bits ($F32) o sin GRUB EFI de 32 bits (grub-efi-ia32-bin): no se prueba UEFI ia32)"
fi
# 5. la misma memoria, por la BIOS (SeaBIOS), como disco: multiboot, como siempre
cp $V/usb.img $V/u3.img
maquina_uefi qemu-system-i386 bios "-machine pc -m 512 -drive file=$V/u3.img,format=raw,if=ide,index=0" ":espera 60 ^escritorio: en marcha" ":pausa 2" ":captura uefi4"
mira "y la misma memoria por la BIOS (SeaBIOS): el GRUB de siempre, el escritorio a 800 x 600" "^escritorio: en marcha, 800 x 600"
if ! grep -q "^uefi:" $S; then ok "por la BIOS, nada de UEFI (multiboot de siempre: la consola es la de texto)"; else ko "por la BIOS dice cosas de UEFI: $(grep '^uefi:' $S | head -1)"; fi
# 6. el CD por UEFI
if bash $AQUI/nucleo32/cd.sh $V/cd.iso > $V/cd.txt 2>&1 && xorriso -indev $V/cd.iso -report_el_torito plain > $V/torito.txt 2>&1 && grep -q "UEFI" $V/torito.txt && grep -q "BIOS" $V/torito.txt; then
  ok "cd.sh hace el CD con dos El Torito: el de la BIOS y el de UEFI"
  cp $OV/OVMF_VARS_4M.fd $V/vars64.fd
  maquina_uefi qemu-system-x86_64 cd "-machine q35 -m 512 $SIN64 -drive if=pflash,format=raw,readonly=on,file=$F64 -drive if=pflash,format=raw,file=$V/vars64.fd -cdrom $V/cd.iso" \
    ":espera 120 ^escritorio: en marcha" ":pausa 3" ":captura uefi5"
  mira "el CD por UEFI (OVMF, q35, el lector SATA): sin instalar, al escritorio, con lo de M:" "^escritorio: en marcha"
else ko "cd.sh no hace el CD con UEFI:"; tail -n 3 $V/cd.txt $V/torito.txt | sed 's/^/        /'; fi
[ -n "${CAPTURAS_A:-}" ] && { mkdir -p "$CAPTURAS_A"; cp $G/uefi*.ppm "$CAPTURAS_A"/ 2> /dev/null; }
return 0
}
