#!/bin/bash
# Prueba de EQUIPO (informe del hardware en texto) y de DIAGNOSTICO (la
# carpeta \INFORMES\EQUIPOnn con INFORME.TXT y los volcados en binario), en
# QEMU con varias máquinas: -M pc y -M q35, sin PCI (-M isapc), con UHCI,
# OHCI, EHCI y xHCI, con Sound Blaster 16, HD Audio y AC'97, con y sin AdLib,
# en modo real y en V86 (HSPEMM), desde el disquete y desde la imagen USB.
# Las carpetas de los informes se leen después con mtools.
# EQUIPO / DIAGNOSTICO hardware report test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=$(dirname "$IMG")
T=$(mktemp -d)
fail=0
export MTOOLS_SKIP_CHECK=1
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
# check archivo texto [rótulo]; nocheck: que NO esté
check() { if grep -qF -- "$2" "$1"; then ok "${3:-$2}"; else ko "${3:-$2}"; fi; }
nocheck() { if grep -qF -- "$2" "$1"; then ko "${3:-sin $2}"; else ok "${3:-sin $2}"; fi; }
espera() {
  local n=0
  until tr -d '\r' < $T/log 2>/dev/null | grep -aq -- "$1"; do
    sleep 0.5; n=$((n + 1)); [ $n -ge $((2 * ${2:-120})) ] && return 1
  done
  return 0
}
# sesion "opciones de QEMU" orden...: arranca y teclea las órdenes, cada una
# cuando ha acabado la anterior. ">orden" no espera a que acabe, "?texto"
# espera a que salga ese texto y "!x" pulsa teclas sin Intro.
sesion() {
  local qa=$1 n=0 c; shift
  : > $T/log
  rm -f $T/in; mkfifo $T/in
  timeout 600 qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
    -audiodev none,id=snd0 -no-reboot $qa < $T/in > $T/log 2>&1 &
  local qp=$!
  exec 3> $T/in
  espera '^[AC]:\\>' 90; sleep 1
  for c in "$@"; do
    case "$c" in
      \?*) espera "${c:1}" 240; sleep 0.5 ;;
      \!*) printf '%s' "${c:1}" >&3; sleep 1 ;;
      \>*) printf '%s\r' "${c:1}" >&3; sleep 0.5 ;;
      *)   n=$((n + 1)); printf '%s\r' "$c" >&3; sleep 0.3
           printf 'ECHO hecho-%s\r' $n >&3; espera "^hecho-$n" 240 ;;
    esac
  done
  sleep 0.5
  exec 3>&-
  kill $qp 2>/dev/null; wait $qp 2>/dev/null
  tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 > $T/log.txt
}
FD="-drive file=$T/f.img,format=raw,if=floppy -boot a"
texto() { tr -d '\r' < "$1" | iconv -f CP437 -t UTF-8 > "$1.u8"; }

echo "== EQUIPO: -M pc, UHCI, Sound Blaster 16 y AdLib =="
cp "$IMG" $T/f.img
sesion "-M pc $FD -device piix3-usb-uhci -device sb16,audiodev=snd0 -device adlib,audiodev=snd0" \
  "EQUIPO /?" "EQUIPO /PCI /ISA" "EQUIPO /CPU /BIOS /DISCOS /RES > E.TXT" "AYUDA" "AYUDA EQUIPO" \
  "AYUDA DIAGNOSTICO" "EQUIPO /NADA"
L=$T/log.txt
check $L "EQUIPO: informe completo del hardware del equipo, en texto." "EQUIPO /? y AYUDA EQUIPO"
check $L "Bus PCI presente (mecanismo 1, puertos CF8h/CFCh)"
check $L "clase 0C0300: USB UHCI"
check $L "UHCI LEGSUP="
check $L "clase 060100: Puente ISA"
check $L "clase 010180: IDE"
check $L "clase 030000: Vídeo VGA"
check $L "clase 020000: Red Ethernet"
check $L "IRQ 11, patilla D" "IRQ y patilla del UHCI"
check $L "BAR4 E/S C" "BAR de E/S"
check $L "Sound Blaster en 0220h: DSP 4.05 (Sound Blaster 16 o posterior); IRQ 5, DMA 1 5"
check $L "AdLib/OPL (388h): responde"
check $L "Controlador de teclado 8042 (puerto 64h): presente"
check $L "COM1 en 03F8h"
check $L "EQUIPO (informe del hardware), DIAGNOSTICO (lo guarda para enviarlo)." "AYUDA los nombra"
check $L "DIAGNOSTICO: guarda un informe completo del equipo para enviarlo." "AYUDA DIAGNOSTICO"
check $L "Parámetro no válido."
nocheck $L "==== PROCESADOR ====" "con > la salida no va a la pantalla"
if mtype -i $T/f.img ::E.TXT > $T/E.TXT 2>/dev/null; then texto $T/E.TXT; else : > $T/E.TXT.u8; fi
E=$T/E.TXT.u8
check $E "==== PROCESADOR ====" "EQUIPO > E.TXT: el archivo tiene el informe"
check $E "Fabricante: "
check $E "Familia 6, modelo"
check $E "Banderas: FPU "
check $E "Fecha de la BIOS (F000:FFF5): "
check $E "Sistema, fabricante: QEMU" "SMBIOS: sistema"
check $E "Sistema, modelo: Standard PC (i440FX + PIIX, 1996)" "SMBIOS: modelo"
check $E "ACPI: RSDP en "
check $E "  DSDT  rev. " "ACPI: lista de tablas"
check $E "Mapa de memoria (INT 15h E820h):"
check $E "tipo 1 libre"
check $E "INT 15h E801h: AX="
check $E "Memoria convencional (INT 12h): 6"
check $E "Disqueteras según la CMOS (10h): A: 1,44 MB"
check $E "Unidad de arranque de DOS: A:"
check $E "XMS: no hay gestor"
check $E "HSPSND: no está cargado"
nocheck $E "==== PCI ====" "solo los apartados pedidos"

echo "== EQUIPO: -M pc, OHCI y AC'97, sin AdLib =="
cp "$IMG" $T/f.img
sesion "-M pc $FD -device pci-ohci -device AC97,audiodev=snd0" "EQUIPO /PCI /ISA /VESA"
check $L "clase 0C0310: USB OHCI"
check $L "OHCI revisión 1.0: HcControl="
check $L "InterruptRouting=0"
check $L "clase 040100: Audio AC'97"
check $L "AC'97: estado global (NABM+30h)="
check $L "Sound Blaster: no responde ninguna en 220h, 240h, 260h ni 280h"
check $L "AdLib/OPL (388h): no responde"
check $L "VESA VBE 3.0, memoria de vídeo"
check $L "0101:640x480x8pL" "lista de modos VESA"
check $L "EDID de la pantalla"
nocheck $L "USB UHCI"

echo "== EQUIPO: -M q35, EHCI y HD Audio =="
cp "$IMG" $T/f.img
sesion "-M q35 $FD -device ich9-usb-ehci1 -device intel-hda -device hda-duplex,audiodev=snd0" "EQUIPO /BIOS /PCI"
check $L "clase 0C0320: USB EHCI"
check $L "EHCI versión 1.00, 6 puertos"
check $L "USBLEGSUP=" "USBLEGSUP del EHCI (propiedad de la BIOS)"
check $L "clase 040300: HD Audio"
check $L "GCAP=4401"
check $L "códec 0: ID 1AF4" "ID del códec HD Audio"
check $L "clase 010601: SATA AHCI"
check $L "clase 0C0500: SMBus"
check $L "Capacidades:"
check $L "MMCONFIG (tabla MCFG de ACPI): base B0000000h"
check $L "Sistema, modelo: Standard PC (Q35 + ICH9, 2009)"

echo "== EQUIPO: -M q35, xHCI con un teclado USB =="
cp "$IMG" $T/f.img
sesion "-M q35 $FD -device qemu-xhci -device usb-kbd" "EQUIPO /PCI"
check $L "clase 0C0330: USB xHCI"
check $L "xHCI versión 1.00, 8 puertos, 64 ranuras"
check $L "USB 3.00: puertos 1-4"
check $L "USB 2.00: puertos 5-8"
check $L " conectado" "puerto con el teclado conectado"
nocheck $L "USB EHCI"

echo "== EQUIPO: -M q35 en modo V86 (HSPEMM): registros por INT 15h 87h =="
cp "$IMG" $T/f.img
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDEVICE=A:\\HISPADOS\\HSPEMM.SYS\r\nDOS=HIGH,UMB\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
sesion "-M q35 $FD -device qemu-xhci -device pci-ohci" "EQUIPO /CPU /BIOS /PCI /RES"
check $L "Modo: virtual 8086"
check $L "xHCI versión 1.00, 8 puertos, 64 ranuras" "xHCI leído en V86"
check $L "OHCI revisión 1.0" "OHCI leído en V86"
check $L "  DSDT  rev. 1" "tablas ACPI leídas en V86"
check $L "suma bien"
nocheck $L "suma MAL"
check $L "XMS: versión 3.00"
check $L "EMS: versión 4.0"
check $L "VCPI: versión 1.0"
check $L "núcleo en la memoria alta (HMA)"

echo "== EQUIPO: -M isapc (sin PCI) =="
cp "$IMG" $T/f.img
sesion "-M isapc -cpu 486 $FD" "EQUIPO /CPU /PCI"
check $L "No hay bus PCI: los puertos CF8h/CFCh no responden"
check $L "Fabricante: "

echo "== DIAGNOSTICO: disquete, -M pc con xHCI y Sound Blaster 16 =="
if [ -f $B/hispados-disquete.img ]; then cp $B/hispados-disquete.img $T/f.img; else cp "$IMG" $T/f.img; fi
# una AUDICION de pega, para la pregunta del final (AUDICION carpeta: la de
# verdad guarda ahí SONIDO.TXT); la de verdad se quita de esta copia, porque
# pregunta y esperaría respuestas
MTOOLS_SKIP_CHECK=1 mdel -i $T/f.img ::HISPADOS/AUDICION.COM > /dev/null 2>&1
printf '@ECHO OFF\r\nECHO audicion de pega %%1\r\nECHO informe de sonido> %%1\\SONIDO.TXT\r\n' > $T/AUDICION.BAT
mcopy -o -i $T/f.img $T/AUDICION.BAT ::HISPADOS/AUDICION.BAT
# dos tablas SSDT añadidas, para ver que las repetidas se numeran
printf 'tabla de prueba uno' > $T/s1.bin; printf 'tabla de prueba dos' > $T/s2.bin
sesion "-M pc $FD -device qemu-xhci -device usb-mouse -device sb16,audiodev=snd0 \
  -acpitable sig=SSDT,rev=1,oem_id=PRUEBA,oem_table_id=TABLA1,data=$T/s1.bin \
  -acpitable sig=SSDT,rev=1,oem_id=PRUEBA,oem_table_id=TABLA2,data=$T/s2.bin" \
  ">DIAGNOSTICO" "?prueba de sonido? (S/N)" "!S" "?Suba esa carpeta" \
  "DIAGNOSTICO /NOSONIDO /NOISA /NOVESA"
check $L "Informe guardado en A:\\INFORMES\\EQUIPO01. Suba esa carpeta al FTP en HispaDOS/informes"
check $L "Informe guardado en A:\\INFORMES\\EQUIPO02. Suba esa carpeta al FTP en HispaDOS/informes" "la segunda vez, otra carpeta"
check $L "audicion de pega A:\\INFORMES\\EQUIPO01" "AUDICION lanzada con la carpeta de su informe"
mkdir -p $T/out
mcopy -s -n -i $T/f.img ::INFORMES $T/out/ > /dev/null 2>&1
D=$T/out/INFORMES/EQUIPO01
if [ -f $D/INFORME.TXT ]; then texto $D/INFORME.TXT; ok "existe INFORMES\\EQUIPO01\\INFORME.TXT"; else : > $D.u8 2>/dev/null; mkdir -p $D; : > $D/INFORME.TXT.u8; ko "existe INFORMES\\EQUIPO01\\INFORME.TXT"; fi
R=$D/INFORME.TXT.u8
check $R "Informe de diagnóstico de Hispa-DOS"
check $R "Fecha: "
check $R "==== VER ===="
check $R "Hispa-DOS versión"
for s in "PROCESADOR" "BIOS Y MEMORIA" "VIDEO (VESA)" "PCI" "DISQUETERAS Y DISCOS (INT 13h)" "RESIDENTES Y SISTEMA" "ISA Y PUERTOS" \
         "MEM /C" "SET" "HSPSND /INFO" "HSPRATON /INFO" "MANDO" "TYPE A:\\CONFIG.SYS" "TYPE A:\\AUTOEXEC.BAT" "FIN DEL INFORME"; do
  check $R "==== $s ====" "apartado $s"
done
check $R "clase 0C0330: USB xHCI"
check $R "Programas en la memoria convencional" "salida de MEM /C"
check $R "PATH=A:\\HISPADOS" "salida de SET"
check $R "Tarjeta: Sound Blaster 16 en 220h" "salida de HSPSND /INFO"
check $R "HSPSND: cargado, tarjeta 2 (Sound Blaster 16)"
check $R "Sound Blaster: la está usando HSPSND"
check $R "XMS: versión 3.00"
check $R "DEVICE=A:\\HISPADOS\\HSPXMS.SYS" "CONFIG.SYS copiado"
check $R "A:\\HISPADOS\\HSPSND /VOL:80" "AUTOEXEC.BAT copiado"
check $R "8042, octeto de órdenes (orden 20h): "
check $R "Ratón o panel táctil PS/2: "
check $R "Prueba de sonido lanzada"
check $R "-> ACPI\\DSDT.BIN"
nocheck $R "suma MAL"
nocheck $R "no se ha podido"
# los volcados
tam() { if [ -f "$D/$1" ] && [ "$(stat -c %s "$D/$1")" = "$2" ]; then ok "$1 ($2 octetos)"; else ko "$1 ($2 octetos)"; fi; }
empieza() { if [ -f "$D/$1" ] && [ "$(head -c ${#2} "$D/$1")" = "$2" ]; then ok "$1 empieza por $2"; else ko "$1 empieza por $2"; fi; }
tam PCI/00_00_0.BIN 256
if [ "$(xxd -p -l 4 $D/PCI/00_00_0.BIN 2>/dev/null)" = "86803712" ]; then ok "PCI/00_00_0.BIN es el 8086:1237"; else ko "PCI/00_00_0.BIN es el 8086:1237"; fi
n=$(ls $D/PCI/*.BIN 2>/dev/null | wc -l); m=$(grep -c "  clase " $R)
if [ "$n" -ge 6 ] && [ "$n" = "$m" ]; then ok "un volcado por función PCI ($n)"; else ko "un volcado por función PCI ($n de $m)"; fi
empieza ACPI/DSDT.BIN DSDT
empieza ACPI/FACP.BIN FACP
empieza ACPI/RSDT.BIN RSDT
empieza ACPI/FACS.BIN FACS
empieza ACPI/RSDP.BIN "RSD PTR "
empieza ACPI/SSDT1.BIN SSDT
empieza ACPI/SSDT2.BIN SSDT
check $R "OEM PRUEBA TABLA2" "las SSDT repetidas, cada una en su archivo"
if python3 -c "
import sys,struct
d=open('$D/ACPI/DSDT.BIN','rb').read()
sys.exit(0 if len(d)==struct.unpack('<I',d[4:8])[0] and sum(d)%256==0 else 1)" 2>/dev/null; then ok "DSDT.BIN: longitud y suma correctas"; else ko "DSDT.BIN: longitud y suma correctas"; fi
tam CMOS.BIN 128
tam IVT.BIN 1024
tam BDA.BIN 256
tam VBE.BIN 512
empieza VBE.BIN VESA
tam EDID.BIN 128
if [ -s $D/SMBIOS.BIN ] && [ -s $D/SMBIOSEP.BIN ]; then ok "SMBIOS.BIN y SMBIOSEP.BIN"; else ko "SMBIOS.BIN y SMBIOSEP.BIN"; fi
empieza SMBIOSEP.BIN _SM
if [ -s $D/VBEMODOS.BIN ] && [ -s $D/ROMBIOS.TXT ] && [ -s $D/ROMVIDEO.TXT ]; then ok "VBEMODOS.BIN, ROMBIOS.TXT y ROMVIDEO.TXT"; else ko "VBEMODOS.BIN, ROMBIOS.TXT y ROMVIDEO.TXT"; fi
check $D/ROMBIOS.TXT "SeaBIOS" "ROMBIOS.TXT tiene los textos de la BIOS"
if ls $D/USB/*.BIN > /dev/null 2>&1; then ok "registros del xHCI en USB\\"; else ko "registros del xHCI en USB\\"; fi
if [ -s $D/SONIDO.TXT ]; then ok "SONIDO.TXT en la misma carpeta"; else ko "SONIDO.TXT en la misma carpeta"; fi
D2=$T/out/INFORMES/EQUIPO02
if [ -f $D2/INFORME.TXT ]; then texto $D2/INFORME.TXT; else mkdir -p $D2; : > $D2/INFORME.TXT.u8; fi
check $D2/INFORME.TXT.u8 "==== FIN DEL INFORME ====" "EQUIPO02: informe completo"
check $D2/INFORME.TXT.u8 "==== PCI ===="
nocheck $D2/INFORME.TXT.u8 "==== ISA Y PUERTOS ====" "/NOISA quita el apartado"
nocheck $D2/INFORME.TXT.u8 "==== VIDEO (VESA) ====" "/NOVESA quita el apartado"
nocheck $D2/INFORME.TXT.u8 "==== AUDICION (prueba de sonido) ====" "/NOSONIDO no ofrece la prueba de sonido"
if [ ! -f $D2/VBE.BIN ] && [ -f $D2/PCI/00_00_0.BIN ]; then ok "EQUIPO02 con sus propios volcados"; else ko "EQUIPO02 con sus propios volcados"; fi
if fsck.fat -n $T/f.img > /dev/null 2>&1; then ok "el disquete queda íntegro (fsck.fat)"; else ko "el disquete queda íntegro (fsck.fat)"; fi
if [ -n "$(MTOOLS_SKIP_CHECK=1 mdir -b -i $T/f.img ::INFORME.TXT 2>/dev/null)" ]; then ko "nada suelto en la raíz"; else ok "nada suelto en la raíz"; fi

echo "== DIAGNOSTICO: imagen USB arrancada como disco duro (C:) =="
if [ -f $B/hispados-usb.img ]; then
  cp $B/hispados-usb.img $T/u.img
  # (sin AUDICION: así no se pregunta por la prueba de sonido)
  MTOOLS_SKIP_CHECK=1 mdel -i $T/u.img@@1048576 ::HISPADOS/AUDICION.COM > /dev/null 2>&1
  sesion "-M pc -drive file=$T/u.img,format=raw,if=ide -boot c -device pci-ohci" "DIAGNOSTICO"
  check $L "Informe guardado en C:\\INFORMES\\EQUIPO01. Suba esa carpeta al FTP en HispaDOS/informes"
  nocheck $L "prueba de sonido" "sin AUDICION no se pregunta"
  rm -rf $T/out; mkdir -p $T/out
  mcopy -s -n -i $T/u.img@@1048576 ::INFORMES $T/out/ > /dev/null 2>&1
  D=$T/out/INFORMES/EQUIPO01
  if [ -f $D/INFORME.TXT ]; then texto $D/INFORME.TXT; else mkdir -p $D; : > $D/INFORME.TXT.u8; fi
  R=$D/INFORME.TXT.u8
  check $R "==== FIN DEL INFORME ===="
  check $R "Unidad de arranque de DOS: C:"
  check $R "Disco 80h: CHS "
  check $R "; EDD "
  check $R "01F0h maestro: QEMU HARDDISK" "identificación ATA del disco"
  check $R "clase 0C0310: USB OHCI"
  check $R "==== TYPE C:\\CONFIG.SYS ===="
  check $R "DEVICE=C:\\HISPADOS\\HSPUSB.SYS"
  tam ATA1F0_0.BIN 512
  tam EDD80.BIN 74
  tam PCI/00_00_0.BIN 256
else
  echo "  (no hay $B/hispados-usb.img: haga «make usb»; se salta)"
fi

if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi
rm -rf $T
exit $fail
