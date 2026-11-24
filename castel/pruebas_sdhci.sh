# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pruebas_sdhci.sh - (SDHCI) la parte 53 de pruebas.sh (SOLO=sdhci), que lee
# este archivo («.»): la función parte_sdhci, con los ayudantes de pruebas.sh
# (ok, ko, mira, sano, saca) y sus carpetas ($AQUI, $P, $G).
#
# Las tarjetas SD y el eMMC por una controladora SDHCI (nucleo32/sdhci.cas,
# sdacpi.cas; acpi/pentium/sdhci.c). QEMU da la sdhci-pci (de la norma 2.00
# o, con sd-spec-version=3, de la 3.00) con su tarjeta (sd-card: SDSC hasta 1
# GB, SDHC por encima; la imagen, de una potencia de 2):
#   1. leer y escribir por ADMA2 una SDSC con FAT32 (sin tabla): el
#      intérprete crea, copia y borra, y fsck.fat y mtools ven lo escrito;
#   2. una SDHC de 4 GB con GPT, por PIO («sdhcipio») y con la norma 3.00;
#   3. sin tarjeta (la ranura vacía): lo dice enseguida; sin «escritura», ni
#      un sector escrito;
#   4. el modo ACPI: una SSDT de prueba (tests/acpi/hazsdhci.py) dice la
#      controladora (80860F16, con su _CRS) y la SDIO (INT33BB, que no se
#      toca); con «sdhcisinpci», el núcleo solo la conoce por ACPI.BIN (la
#      llamada 160) y la tarjeta sale como disco nuevo (y, como QEMU no deja
#      el DMA sin el PCI, se ve el paso de ADMA2 a PIO); dada a mano
#      («sdhci=XXXXXXXX»); y aprendida: \SDHCI.TXT en la memoria USB, que vale
#      en el arranque siguiente sin ACPI.BIN, y no en otro equipo;
#   5. INSTALAR en la tarjeta SD (elegida la primera aunque haya un disco
#      SATA), con la FAT32 sana y el sistema dentro.
# Lo que QEMU no tiene: el eMMC (-device emmc es de QEMU 9.2; aquí, la 8.2),
# las controladoras de Intel (Bay Trail) y el modo ACPI de verdad.
parte_sdhci() {
echo "== Hispa-Pentium: las tarjetas SD y el eMMC (SDHCI: sdhci-pci de QEMU; el modo ACPI con una SSDT) (SDHCI) =="
local X=$P/sdhci S I K f INST base
rm -rf $X; mkdir -p $X
export MTOOLS_SKIP_CHECK=1
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $X/semilla.asm > /dev/null && nasm -f bin $X/semilla.asm -o $X/SEMILLA.BIN || { ko "semilla.cas no compila"; return; }
for u in uordenes:ORDENES uhola:HOLA; do
  python3 -I $AQUI/ccastel.py $AQUI/nucleo32/${u%%:*}.cas -o $X/x.asm > /dev/null && nasm -f bin $X/x.asm -o $X/${u##*:}.BIN || { ko "${u%%:*}.cas no compila"; return; }
done
if ! bash $AQUI/../acpi/pentium/hacer.sh $X/ACPI.BIN > $X/acpi.txt 2>&1; then ko "acpi/pentium/hacer.sh no hace ACPI.BIN: $(tail -n 1 $X/acpi.txt)"; return; fi
if ! $AQUI/nucleo32/disco.sh $X/d32.img 32 > $X/disco.txt 2>&1; then ko "no se puede hacer el disco: $(tail -2 $X/disco.txt)"; return; fi
# la tarjeta: lo de d32.img (FAT32 de 40 MB, sin tabla) en una de 64 MB
sdhc_tarjeta() { rm -f $1; truncate -s 64M $1; dd if=$X/d32.img of=$1 conv=notrunc status=none; }

# ---- 1. una SDSC de 64 MB, por ADMA2: leer y escribir ----
I=$X/sd1.img; sdhc_tarjeta $I
ESCRIBE=1 SD=$I MODULOS= $AQUI/nucleo32/probar.sh $X/SEMILLA.BIN sdhci "dir textos" ordenes escribe "echo Querida señora: ¿qué tal está? > maría.txt" \
  "copy textos\\largo.txt copia.txt" "md obra" "copy *.bat obra" "del obra\\lote.bat" salir disco apagar > $X/sd1.sal 2>&1
S=$X/sd1.sal
mira "SDHCI: la controladora del PCI (clase 08 05), la tarjeta SD (CMD0, CMD8, ACMD41, CID, CSD, SCR), 4 bits y alta velocidad, y la FAT32 encima (C:)" \
  "^sdhci: en el PCI, 00:0[0-9A-F]\.0: 1B36:0007, clase 080501$" "^sdhci: controladora en [0-9A-F]* (PCI), versión 2\.00, CAP 057834B4 00000000, reloj de 52 MHz, ADMA2" \
  "^sdhci: SCR 02 25 00 00 (la norma SD 2)$" "^sdhci: tarjeta SD QEMU! (SDSC), 64 MB; ancho 4 bits; 26 MHz; ADMA2$" \
  "^disco 0: tarjeta SD QEMU! (SDSC), 64 MB, FAT32 (SD)\.$" "^disco 0: tarjeta SD QEMU! (SDSC), 131072 sectores (64 MB), LBA, SD$" "^LARGO.TXT  *17600$"
mira "SDHCI, ADMA2: el intérprete escribe en la tarjeta: crea, escribe, corta, alarga y borra archivos y carpetas, y todo queda como estaba" \
  "^escribir: leído de vuelta: 10000 octetos, 0 distintos; no se deja escribir si se abrió para leer: cierto$" \
  "^carpetas: 70 archivos en PD: 70 hechos; la carpeta tiene 72 entradas" "^escribe: todo queda como estaba (los mismos clusters libres): cierto$" \
  "^ *3 archivo(s) copiado(s)\.$" "^\[código 5\]"
if sano $I; then ok "SDHCI: fsck.fat da la tarjeta por sana ($(tail -n 1 $P/fsck.txt | sed 's/^[^:]*: //'))"
else ko "SDHCI: fsck.fat encuentra la tarjeta mal:"; sed 's/^/        /' $P/fsck.txt | head -8; fi
saca $I TEXTOS/LARGO.TXT $X/largo.txt; saca $I COPIA.TXT $X/copia.txt; saca $I MARIA.TXT $X/maria.txt
if cmp -s $X/largo.txt $X/copia.txt && [ "$(iconv -f CP437 -t UTF-8 $X/maria.txt | tr -d '\r')" = "Querida señora: ¿qué tal está?" ] \
  && mdir -i $I ::OBRA 2> /dev/null | grep -q "^SINFIN  *BAT" && ! mdir -i $I ::OBRA 2> /dev/null | grep -q "^LOTE "; then
  ok "SDHCI: mtools ve lo escrito en la tarjeta: lo copiado, igual que el original; lo tecleado, con sus letras; lo borrado, borrado"
else ko "SDHCI: lo escrito no está bien en la tarjeta"; fi

# ---- 2. una SDHC de 4 GB con GPT, por PIO y con la norma 3.00 ----
I=$X/sd4g.img; rm -f $I; truncate -s 4G $I
parted -s $I mklabel gpt mkpart datos fat32 2048s 83967s mkpart windows ntfs 83968s 200000s > /dev/null 2>&1
dd if=$X/d32.img of=$I bs=512 seek=2048 conv=notrunc status=none
ESCRIBE=1 SD_NORMA=3 SD=$I ARRANQUE="ordenes sdhcipio" MODULOS= $AQUI/nucleo32/probar.sh $X/SEMILLA.BIN "copy textos\\largo.txt c:\\copia.txt" "md nueva" \
  "copy c:\\leeme.txt nueva" "type nueva\\leeme.txt" salir disco apagar > $X/sd4g.sal 2>&1
S=$X/sd4g.sal
mira "SDHCI: una SDHC de 4 GB (direcciones por bloques) con tabla GPT, por PIO, en una controladora de la norma 3.00: su partición FAT32 se monta" \
  "^sdhci: controladora en [0-9A-F]* (PCI), versión 3\.00, .*, PIO; " "^sdhci: tarjeta SD QEMU! (SDHC), 4096 MB; ancho 4 bits; 26 MHz; PIO$" \
  "^disco 0: tarjeta SD QEMU! (SDHC), 4096 MB, FAT32 (SD)\.$" "^sistema de archivos: FAT32, desde el sector 2048, " "^ *1 archivo(s) copiado(s)\.$" "^Este archivo está en un disco FAT"
dd if=$I of=$X/v.img bs=512 skip=2048 count=81920 status=none
saca $X/v.img COPIA.TXT $X/copia2.txt; saca $X/v.img NUEVA/LEEME.TXT $X/leeme2.txt; saca $X/d32.img LEEME.TXT $X/leeme1.txt
if sano $X/v.img && cmp -s $X/copia2.txt $X/largo.txt && cmp -s $X/leeme1.txt $X/leeme2.txt; then
  ok "SDHCI, PIO: lo escrito en la partición de la SDHC está sano (fsck.fat) e igual que el original"
else ko "SDHCI, PIO: lo escrito en la SDHC no está bien:"; sed 's/^/        /' $P/fsck.txt | head -6; fi

# ---- 3. sin tarjeta; sin permiso ----
QEMU_MAS="-device sdhci-pci" MODULOS= $AQUI/nucleo32/probar.sh $X/SEMILLA.BIN sdhci hora apagar > $X/vacia.sal 2>&1
S=$X/vacia.sal
mira "SDHCI: la ranura vacía: se dice que no hay tarjeta (y se prueba igual, por si la detecta otra patilla, como en los Bay Trail), sin colgarse" \
  "^sdhci: la ranura dice que no hay tarjeta (estado [0-9A-F]*): se prueba igual$" "^sdhci: sin tarjeta (no contesta; error 0001)$" "^llevo [0-9] segundos"
I=$X/sl.img; sdhc_tarjeta $I; cp $I $X/sl0.img
SOLO_LECTURA=1 ESCRIBE=1 SD=$I ARRANQUE=ordenes MODULOS= $AQUI/nucleo32/probar.sh $X/SEMILLA.BIN "echo hola > nuevo.txt" "del leeme.txt" "md cosas" salir apagar > $X/sl.sal 2>&1
S=$X/sl.sal
mira "SDHCI: sin «escritura», la tarjeta es de solo lectura" "^No puedo escribir en nuevo.txt: es de solo lectura\.$" "^No puedo borrar leeme.txt: es de solo lectura\.$"
if cmp -s $I $X/sl0.img; then ok "SDHCI: y la imagen de la tarjeta queda idéntica, octeto a octeto"; else ko "SDHCI: sin permiso, la tarjeta ha cambiado"; fi

# ---- 4. el modo ACPI ----
# (la dirección de la controladora: la que le pone SeaBIOS, la de la prueba 1)
base=$(sed -n 's/^sdhci: controladora en \([0-9A-F]*\) (PCI).*/\1/p' $X/sd1.sal | head -1)
python3 -I $AQUI/../tests/acpi/hazsdhci.py ${base:-FEBF1000} $X/sdhci.aml
I=$X/acpi.img; sdhc_tarjeta $I
SD=$I MODULOS=$X/ACPI.BIN,$X/ORDENES.BIN ARRANQUE="ordenes sdhcisinpci" QEMU_MAS="-acpitable file=$X/sdhci.aml" $AQUI/nucleo32/probar.sh $X/SEMILLA.BIN \
  ":pausa 4" "type c:\\textos\\pirata.txt" salir sdhci apagar > $X/acpi.sal 2>&1
S=$X/acpi.sal
mira "SDHCI, modo ACPI: ACPI.BIN halla en la SSDT la controladora (80860F16: su _CRS con Memory32Fixed e Interrupt, su _PS0) y la SDIO del Wi-Fi (INT33BB _UID 2, que no se toca)" \
  "^ACPI: SDHCI: ._SB_\.SDHB 80860F16 _UID 1 (SD), _STA Fh: memoria en ${base}h (4096 octetos), IRQ 46, _PS0 hecho$" \
  "^ACPI: SDHCI: ._SB_\.SDHC INT33BB _UID 2 (SDIO), _STA Fh: memoria en FEB00000h (4096 octetos) (la SDIO, la del Wi-Fi: no se toca)$" \
  "^ACPI: SDHCI: 2 controladoras SD/eMMC en ACPI$"
mira "SDHCI, modo ACPI: sin verla en el PCI («sdhcisinpci»), el núcleo la arranca con lo que le da ACPI.BIN (llamada 160) y la tarjeta es un disco nuevo, C:" \
  "^sdhci: en el PCI no hay controladoras SD/eMMC (clase 08 05)$" "sdhci: ACPI dice ._SB_\.SDHB (80860F16, SD), _STA Fh, en $base, IRQ 46$" \
  "^sdhci: controladora en $base (ACPI), versión 2\.00" "^disco [0-9]: tarjeta SD QEMU! (SDSC), 64 MB, C: (FAT32) (SD; de ACPI)\.$" "^un velero bergantín\.$"
mira "SDHCI: si el ADMA2 da error (en QEMU, sin el PCI no hay DMA), se sigue por PIO" "^sdhci: en $base el ADMA2 da error (0200): sigue por PIO$"
if ! grep -q "excepción\|\*\*\*\|ACPI Error\|ACPI Exception" $S; then ok "SDHCI, modo ACPI: ni una excepción del núcleo ni un error de ACPICA"
else ko "SDHCI, modo ACPI: hay excepciones o errores de ACPICA:"; grep "excepción\|\*\*\*\|ACPI Error\|ACPI Exception" $S | head -4 | sed 's/^/        /'; fi
SD=$I MODULOS=$X/ORDENES.BIN ARRANQUE="ordenes sdhcisinpci sdhci=$base sinacpi" $AQUI/nucleo32/probar.sh $X/SEMILLA.BIN "type c:\\textos\\pirata.txt" salir sdhci apagar > $X/mano.sal 2>&1
S=$X/mano.sal
mira "SDHCI: la controladora dada a mano en la línea del cargador («sdhci=$base»), sin ACPI: la tarjeta sale al arrancar" \
  "^sdhci: la de $base, dada en la línea del cargador$" "^sdhci: controladora en $base (la línea del cargador)" "^disco 0: tarjeta SD QEMU! (SDSC), 64 MB, FAT32 (SD)\.$" "^un velero bergantín\.$"
# aprendida: la memoria USB de arranque (X:, con ACPI.BIN) guarda \SDHCI.TXT
cp $X/d32.img $X/usb.img
mcopy -i $X/usb.img $X/ACPI.BIN ::ACPI.BIN
U="-device qemu-xhci,id=u -drive id=mu,file=$X/usb.img,format=raw,if=none -device usb-storage,bus=u.0,drive=mu"
# (con la memoria en el xHCI, SeaBIOS pone la controladora en otro sitio: el suyo, y su SSDT)
SD=$I MODULOS= ARRANQUE=sinacpi QEMU_MAS="$U" $AQUI/nucleo32/probar.sh $X/SEMILLA.BIN sdhci apagar > $X/donde.sal 2>&1
base=$(sed -n 's/^sdhci: controladora en \([0-9A-F]*\) (PCI).*/\1/p' $X/donde.sal | head -1)
python3 -I $AQUI/../tests/acpi/hazsdhci.py ${base:-FEBF1000} $X/sdhci2.aml
ESCRIBE=1 SD=$I MODULOS= ARRANQUE="ordenes escritura=usb unidad=X registro sdhcisinpci" QEMU_MAS="$U -acpitable file=$X/sdhci2.aml" $AQUI/nucleo32/probar.sh $X/SEMILLA.BIN \
  ":pausa 6" salir apagar > $X/aprende.sal 2>&1
S=$X/aprende.sal
saca $X/usb.img SDHCI.TXT $X/sdhci.txt; saca $X/usb.img ARRANQUE.TXT $X/arranque.txt
if grep -q "^sdhci: lo que ha dicho ACPI queda apuntado en .SDHCI.TXT" $S && grep -q "^equipo=[0-9A-F]\{8\} [0-9A-F]\{8\}"$'\r'"$" $X/sdhci.txt \
  && grep -q "^$base 46 sd"$'\r'"$" $X/sdhci.txt && ! grep -q "FEB00000" $X/sdhci.txt \
  && grep -q "^sdhci: ACPI dice ._SB_.SDHB" $X/arranque.txt && grep -q "^disco [0-9]: tarjeta SD QEMU! (SDSC), 64 MB, C: (FAT32) (SD; de ACPI)" $X/arranque.txt; then
  ok "SDHCI: lo que dice ACPI se aprende: X:\\SDHCI.TXT (el equipo, la dirección, la IRQ y si es SD o eMMC; la SDIO no), y ARRANQUE.TXT se guarda otra vez con ello"
else ko "SDHCI: no se aprende lo que dice ACPI (\\SDHCI.TXT, ARRANQUE.TXT):"; cat $X/sdhci.txt 2> /dev/null | sed 's/^/        /'; grep "sdhci:" $S | sed 's/^/        /' | head -6; fi
ESCRIBE=1 SD=$I MODULOS= ARRANQUE="ordenes escritura=usb unidad=X registro sdhcisinpci sinacpi" QEMU_MAS="$U" $AQUI/nucleo32/probar.sh $X/SEMILLA.BIN \
  "type c:\\textos\\pirata.txt" salir apagar > $X/recuerda.sal 2>&1
S=$X/recuerda.sal
mira "SDHCI: y en el arranque siguiente, SIN ACPI.BIN («sinacpi», como el Acer One 10), la controladora aprendida sale desde el principio" \
  "^sdhci: la de $base, aprendida de ACPI en otro arranque (.SDHCI.TXT)$" "^disco [0-9]: tarjeta SD QEMU! (SDSC), 64 MB, C: (FAT32) (SD; de ACPI)\.$" "^un velero bergantín\.$"
# (0.8.10, el Acer One 10 con «sinacpi»: ARRANQUE.TXT no decía nada de
# \SDHCI.TXT) lo que se hace con él queda siempre en el registro
saca $X/usb.img ARRANQUE.TXT $X/arranque2.txt
if grep -q "^sdhci: uso X:.SDHCI.TXT (de este equipo, [0-9A-F]\{8\} [0-9A-F]\{8\})$" $S && grep -q "^sdhci: uso X:.SDHCI.TXT" $X/arranque2.txt \
  && grep -q "^sdhci: la de $base, aprendida de ACPI en otro arranque" $X/arranque2.txt; then
  ok "SDHCI: y lo dice en ARRANQUE.TXT («uso X:\\SDHCI.TXT (de este equipo, ...)» y la controladora aprendida)"
else ko "SDHCI: ARRANQUE.TXT no dice qué hace con \\SDHCI.TXT:"; grep "sdhci" $X/arranque2.txt | sed 's/^/        /' | head -4; fi
ESCRIBE=1 SD=$I MODULOS= MAQUINA=q35 ARRANQUE="ordenes escritura=usb unidad=X sdhcisinpci sinacpi" QEMU_MAS="$U" $AQUI/nucleo32/probar.sh $X/SEMILLA.BIN salir apagar > $X/otro.sal 2>&1
S=$X/otro.sal
if grep -q "^sdhci: X:.SDHCI.TXT es de otro equipo (dice [0-9A-F]\{8\} [0-9A-F]\{8\}; este es [0-9A-F]\{8\} [0-9A-F]\{8\}): no se usa$" $S && ! grep -q "aprendida de ACPI" $S; then
  ok "SDHCI: en otro equipo (q35: otro puente principal) lo aprendido no se usa (no se tocan direcciones que allí son otra cosa)"
else ko "SDHCI: lo aprendido se usa en otro equipo:"; grep "sdhci" $S | sed 's/^/        /' | head -4; fi
cp $X/usb.img $X/usb2.img; mdel -i $X/usb2.img ::SDHCI.TXT
SD=$I MODULOS= ARRANQUE="ordenes escritura=usb unidad=X sdhcisinpci sinacpi" QEMU_MAS="${U/usb.img/usb2.img}" $AQUI/nucleo32/probar.sh $X/SEMILLA.BIN salir apagar > $X/noesta.sal 2>&1
S=$X/noesta.sal
mira "SDHCI: sin \\SDHCI.TXT (la memoria grabada otra vez), lo dice: «no está en X:»" "^sdhci: .SDHCI.TXT no está en X: (lo aprende un arranque con ACPI.BIN)$"

# ---- 5. INSTALAR en la tarjeta SD ----
if [ "$(id -u)" = 0 ] && command -v grub-install > /dev/null && [ -d /usr/lib/grub/i386-pc ]; then
  # (SDHCI_USB=imagen: una memoria ya hecha por usb.sh, para no hacerla otra vez)
  [ -f $G/usb.img ] || { [ -n "${SDHCI_USB:-}" ] && mkdir -p $G && cp "$SDHCI_USB" $G/usb.img; }
  [ -f $G/usb.img ] || $AQUI/nucleo32/usb.sh $G/usb.img > $X/usb.txt 2>&1
  K=$X/kit; mkdir -p $K
  INST=
  for f in ESCRIT8.BIN:NUCLEO.BIN INSTALAR.BIN ASISTE.BIN ORDENES.BIN BLOC.BIN CONSOLA.BIN EQUIPO.BIN HOLA.BIN INSTALA/LEEME.TXT:LEEME.TXT INSTALA/MANUAL.TXT:MANUAL.TXT INSTALA/BOOT.IMG:BOOT.IMG INSTALA/CORE.IMG:CORE.IMG; do
    mcopy -n -i $G/usb.img@@1M ::${f%%:*} $K/${f##*:} > /dev/null 2>&1
    INST=$INST${INST:+,}$K/${f##*:}
  done
  # (UEFI) y el arranque por UEFI del disco instalado: los GRUB EFI que van a \EFI\BOOT
  for f in BOOTIA32.EFI BOOTX64.EFI; do
    if mcopy -n -i $G/usb.img@@1M ::INSTALA/EFI/$f $K/$f > /dev/null 2>&1; then
      INST=$INST,$K/$f; printf '%s=EFI\\BOOT\r\n' $f >> $K/CARPETAS.TXT
    fi
  done
  [ -f $K/CARPETAS.TXT ] && INST=$INST,$K/CARPETAS.TXT
  # (INSTALAR, el de este árbol)
  python3 -I $AQUI/ccastel.py $AQUI/nucleo32/uinstala.cas -o $X/x.asm > /dev/null && nasm -f bin $X/x.asm -o $K/INSTALAR.BIN
  I=$X/inst.img; rm -f $I; truncate -s 512M $I
  truncate -s 300M $X/sata.img
  # (0.8.10, el Acer One 10 instalado en el eMMC: el sistema no veía su
  # disco) lo aprendido, \SDHCI.TXT, en el medio de instalación: dónde pone
  # SeaBIOS la controladora con estos mismos aparatos
  SD=$I SATA=$X/sata.img MODULOS= ARRANQUE=sinacpi $AQUI/nucleo32/probar.sh $X/SEMILLA.BIN sdhci apagar > $X/dondei.sal 2>&1
  basei=$(sed -n 's/^sdhci: controladora en \([0-9A-F]*\) (PCI).*/\1/p' $X/dondei.sal | head -1)
  printf '; Hispa-Pentium: las controladoras SD/eMMC que ha dicho ACPI en este equipo (sdacpi.cas)\r\nequipo=12378086 11001AF4\r\n%s 46 sd\r\n' "${basei:-FEBF1000}" > $K/SDHCI.TXT
  INST=$INST,$K/SDHCI.TXT
  CAPTURAS=$X ESCRIBE=1 SD=$I SATA=$X/sata.img MODULOS=$INST ARRANQUE="instalar unidad=M" $AQUI/nucleo32/probar.sh $X/SEMILLA.BIN ":pausa 1" \
    ":tecla ret" ":texto s1" ":tecla ret" ":texto s2" ":tecla ret" ":pausa 6" ":texto s3" ":tecla ret" > $X/inst.sal 2>&1
  S=$X/inst.sal
  if grep -q "Disco 1   SATA  QEMU HARDDISK  *300 MB" $X/s1.txt && grep -q "Disco 2   SD    tarjeta SD QEMU! (SDSC)  *512 MB" $X/s1.txt \
    && grep -q "(tarjeta SD: se puede instalar aquí sin tocar el eMMC)" $X/s1.txt && grep -q "Disco:      SD  tarjeta SD QEMU! (SDSC), 512 MB" $X/s2.txt \
    && grep -q "^instalar: disco 1, SD, 512 MB, se puede: 1$" $S && grep -q "Hispa-Pentium está instalado" $X/s3.txt; then
    ok "INSTALAR: la tarjeta SD sale en la lista (con lo que es) y, con un disco SATA también, es la elegida de entrada; se instala en ella"
  else ko "INSTALAR no ofrece bien la tarjeta SD:"; sed 's/^/        /' $X/s1.txt | sed -n 4,16p; fi
  dd if=$I of=$X/vol.img bs=512 skip=2048 conv=sparse status=none
  if fsck.fat -n $X/vol.img > $X/fsck.txt 2>&1 && mdir -i $X/vol.img ::HISPA 2> /dev/null | grep -q "^NUCLEO  *BIN" && cmp -s -n 440 $I $K/BOOT.IMG \
    && cmp -s <(mcopy -n -i $X/vol.img ::HISPA/ORDENES.BIN - 2> /dev/null) $K/ORDENES.BIN && cmp -s <(head -c 1M $X/sata.img) <(head -c 1M /dev/zero); then
    ok "INSTALAR en la tarjeta SD: la FAT32 sana (fsck.fat), el sistema en \\HISPA y GRUB en el MBR; el disco SATA, sin tocar"
  else ko "lo instalado en la tarjeta SD no está bien:"; sed 's/^/        /' $X/fsck.txt | head -6; fi
  # (0.8.10) \SDHCI.TXT, a la raíz de lo instalado, y la controladora en la
  # línea del núcleo de GRUB.CFG (« sdhci=...»): sin ACPI ni el PCI
  # («sdhcisinpci»), el sistema instalado ve su disco con esa línea
  rm -f $X/cfgi.txt $X/sdhcii.txt; mcopy -n -i $X/vol.img ::HISPA/GRUB.CFG $X/cfgi.txt > /dev/null 2>&1; mcopy -n -i $X/vol.img ::SDHCI.TXT $X/sdhcii.txt > /dev/null 2>&1
  if grep -q "multiboot /HISPA/NUCLEO.BIN escritorio escritura sdhci=$basei$" $X/cfgi.txt && grep -q "multiboot /HISPA/NUCLEO.BIN ordenes escritura sdhci=$basei$" $X/cfgi.txt \
    && grep -q "^$basei 46 sd" $X/sdhcii.txt && ! grep -q "sindisco.*sdhci=" $X/cfgi.txt && grep -q "^instalar: las controladoras SD/eMMC de .SDHCI.TXT, en el GRUB.CFG: sdhci=$basei$" $S; then
    ok "INSTALAR: \\SDHCI.TXT de la memoria va a la raíz de lo instalado y su controladora, a la línea del núcleo de GRUB.CFG (« sdhci=$basei»)"
  else ko "INSTALAR no pasa \\SDHCI.TXT a lo instalado:"; grep -n "NUCLEO.BIN" $X/cfgi.txt | sed 's/^/        /' | head -4; fi
  l=$(sed -n 's/^ *multiboot \/HISPA\/NUCLEO.BIN \(ordenes escritura.*\)$/\1/p' $X/cfgi.txt | head -1)
  ESCRIBE=1 SD=$I SATA=$X/sata.img MODULOS= ARRANQUE="$l sdhcisinpci sinacpi" $AQUI/nucleo32/probar.sh $X/SEMILLA.BIN "dir c:\\hispa\\equipo.bin" salir apagar > $X/arrinst.sal 2>&1
  S=$X/arrinst.sal
  mira "y arrancado con su línea («$l», «sdhcisinpci sinacpi»: como el eMMC del Acer, que solo dice ACPI), el sistema instalado ve su disco (C:, la tarjeta) y lo suyo" \
    "^sdhci: la de $basei, dada en la línea del cargador$" "^disco [0-9]: tarjeta SD QEMU! (SDSC), 512 MB, FAT32 (SD)\.$" "EQUIPO  *BIN"
  if ! grep -q "^arranque: no se ve ningún disco" $S; then ok "y no dice que no ve ningún disco"; else ko "dice que no ve el disco"; fi
  # (UEFI) el arranque por UEFI de lo instalado (las tabletas Bay Trail no tienen otro)
  if [ -f $K/BOOTIA32.EFI ]; then
    rm -f $X/cfg.txt; mcopy -n -i $X/vol.img ::HISPA/GRUB.CFG $X/cfg.txt > /dev/null 2>&1
    if mdir -i $X/vol.img ::EFI/BOOT 2> /dev/null | grep -q "^BOOTIA32 *EFI" && cmp -s <(mcopy -n -i $X/vol.img ::EFI/BOOT/BOOTIA32.EFI - 2> /dev/null) $K/BOOTIA32.EFI \
      && grep -q "^    multiboot2 /HISPA/NUCLEO.BIN escritorio escritura$" $X/cfg.txt && grep -q "^    multiboot /HISPA/NUCLEO.BIN escritorio escritura$" $X/cfg.txt \
      && grep -q "^    module2 /HISPA/ORDENES.BIN ORDENES.BIN$" $X/cfg.txt && grep -q "^set modo=800x600x32,800x600,auto$" $X/cfg.txt && grep -q "(arranca por la BIOS y por UEFI" $X/s2.txt; then
      ok "INSTALAR con los GRUB EFI: \\EFI\\BOOT\\BOOTIA32.EFI (y BOOTX64.EFI) en la tarjeta, y GRUB.CFG con las entradas de las dos maneras (multiboot y multiboot2)"
    else ko "INSTALAR no deja bien el arranque por UEFI:"; mdir -i $X/vol.img ::EFI/BOOT 2>&1 | sed 's/^/        /' | head -5; sed 's/^/        /' $X/cfg.txt | head -12; fi
    if [ -f /usr/share/OVMF/OVMF32_CODE_4M.fd ]; then
      cp /usr/share/OVMF/OVMF32_VARS_4M.fd $X/vars32.fd; cp $I $X/inst2.img
      V=$X maquina_uefi qemu-system-i386 inst32 "-machine q35 -m 512 -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF32_CODE_4M.fd -drive if=pflash,format=raw,file=$X/vars32.fd -device qemu-xhci,id=x -drive if=none,id=u,file=$X/inst2.img,format=raw -device usb-storage,bus=x.0,drive=u" \
        ":espera 90 ^escritorio: en marcha" ":pausa 2"
      S=$X/inst32.txt
      mira "lo instalado en la tarjeta arranca por UEFI de 32 bits (OVMF ia32, como el Acer One 10): su GRUB EFI, \\HISPA\\GRUB.CFG, multiboot2 y el escritorio" \
        "^arranque: por UEFI (32 bits, GOP .*la línea del cargador: escritorio escritura$" "^escritorio: en marcha"
    fi
  fi
else
  echo "  (sin root o sin grub-install para PC no se prueba INSTALAR en la tarjeta SD)"
fi
}
