#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/usb.sh IMAGEN
# Hace la imagen de una memoria USB (64 MB, o USB_MB; FAT16, arranque BIOS con GRUB) con
# Hispa-Pentium, para probarlo en un PC de verdad: se graba tal cual en la
# memoria (dd, Rufus en modo «imagen DD», balenaEtcher...) y se arranca de ella.
# GRUB carga el núcleo por multiboot. Desde la 0.6 el núcleo lee él la
# memoria USB (xHCI y EHCI): es su unidad X:, la del sistema («unidad=X»), y
# en ella se puede guardar («escritura=usb»: solo en las memorias USB; en los
# discos de dentro del PC NO se escribe, aunque se ven: C:). Por si el USB de
# un equipo no se deja, GRUB carga también, como módulos, los programas (el
# intérprete de órdenes, el Bloc de notas, la Consola, el servicio de red
# RED.BIN y sus programas IPCONFIG, PING y DESCARGAR...) y un LEEME.TXT:
# hacen de unidad M:, en memoria y de solo lectura (usuario.cas), que es la
# del sistema si no hay X:. Con «registro», lo que el núcleo encuentra al
# arrancar (discos, USB) queda en X:\ARRANQUE.TXT. Las entradas del menú:
#   - el escritorio a 800x600 (el de siempre);
#   - el escritorio «a la medida de la pantalla»: GRUB pone el modo que el
#     panel dice que prefiere (lo lee de su EDID): «set gfxpayload=auto»;
#   - el escritorio a 1024x600, a 1366x768 y a 1024x768 (copias del núcleo que
#     le piden a GRUB ese modo en su cabecera: nucleo32/modo_grafico.py; si la
#     BIOS no lo da por VBE, GRUB pone el preferido o el que más se parezca);
#   - los modos de vídeo que da la BIOS de ese equipo (la orden «videoinfo»
#     de GRUB), para saber cuáles se pueden pedir;
#   - el escritorio «como en la 0.5»: sin tocar el USB ni mirar los discos
#     ni poner en marcha la red ni el servicio de ACPI («sinusb sindisco
#     sinred sinacpi»), por si con lo
#     nuevo un equipo no arranca;
#   - el intérprete de órdenes y la consola del núcleo, en modo texto (la
#     consola enseña en la pantalla lo que encuentra: discos y USB);
#   - desde la 0.7, «Instalar Hispa-Pentium en el disco duro»: el núcleo en
#     modo texto con «instalar», que lanza INSTALAR.BIN (uinstala.cas). Todo
#     lo que se instala viene con el cargador, como módulos (la unidad M:):
#     NUCLEO.BIN (el núcleo que pide 800 x 600), los programas, el asistente
#     del primer arranque (ASISTE.BIN), los textos y el arranque del disco,
#     que se hace aquí porque el núcleo no puede: BOOT.IMG (el primer sector
#     de GRUB, para el MBR) y CORE.IMG (el resto de GRUB, con sus módulos
#     dentro, para el hueco de delante de la primera partición; busca en los
#     discos \HISPA\GRUB.CFG y sigue lo que diga). Con «instalar», el núcleo
#     no deja escribir en la memoria USB ni un sector. Lo que solo vale
#     para instalar (esos textos, BOOT.IMG, CORE.IMG y CARPETAS.TXT) está en
#     la carpeta \INSTALA de la memoria. Con ellos van la oficina
#     (nucleo32/hvprun): HVPRUN.BIN y lo de \OFICINA, con
#     CARPETAS.TXT, que le dice a INSTALAR lo que va a C:\OFICINA.
#   - y el servicio de ACPI, ACPI.BIN (acpi/pentium/hacer.sh): apagar de
#     verdad y la batería; con «registro», lo suyo queda en X:\ACPI.TXT.
# (UEFI) Desde la 0.8.8 arranca también por UEFI (x64 y, si está su GRUB,
# ia32: los Bay Trail como el Acer One 10): en la misma partición FAT (la
# tabla sigue siendo la del MBR, como la de Rufus «MBR para BIOS o UEFI»)
# van \EFI\BOOT\BOOTX64.EFI y BOOTIA32.EFI, el GRUB EFI (nucleo32/uefi.sh),
# que lee el mismo /boot/grub/grub.cfg: con «$grub_platform» = efi, sus
# entradas son las de multiboot2 y la pantalla es la del firmware (GOP). Y
# LEEME-UEFI.TXT (Secure Boot apagado). Sin grub-efi-amd64-bin ni
# grub-efi-ia32-bin, solo BIOS, como antes; SIN_UEFI=1, también.
# Necesita ser root (losetup, mount), python3, nasm, parted, dosfstools y
# grub-pc-bin (grub-install --target=i386-pc); (UEFI) y grub-efi-amd64-bin
# y grub-efi-ia32-bin (grub-mkimage).
set -eu
export LC_ALL=C.UTF-8
AQUI=$(dirname "$(readlink -f "$0")")
OUT=${1:?uso: usb.sh IMAGEN}
W=$(mktemp -d /dev/shm/pgraf-usb.XXXX)
L=
trap '[ -n "$L" ] && { umount $W/m 2>/dev/null || true; losetup -d $L 2>/dev/null || true; }; rm -rf $W' EXIT
mkdir $W/f $W/m $W/i
MEDIO=usb
. $AQUI/sistema.sh
. $AQUI/uefi.sh                 # (UEFI)
IMG=$W/usb.img
truncate -s ${USB_MB:-64}M $IMG   # (USB_MB=256: con sitio para las firmas de Escudo)
parted -s $IMG mklabel msdos mkpart primary fat16 1MiB 100% set 1 boot on
L=$(losetup -fP --show $IMG)
# (la partición tarda un momento en aparecer)
for _ in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do [ -b ${L}p1 ] && break; sleep 0.1; done
[ -b ${L}p1 ] || { partprobe $L 2> /dev/null; sleep 0.5; }
mkfs.fat -F 16 -n HISPAPENT ${L}p1 > /dev/null
mount ${L}p1 $W/m
# (TERMO) con TERMO=1, la memoria del Termómetro: TERMO.BIN (termo/hacer.sh)
# en la raíz y como módulo, y en el menú, primero (y solo, a los 5 s), el
# escritorio con «termo», que lo abre al arrancar
if [ -n "${TERMO:-}" ]; then
  bash $AQUI/termo/hacer.sh $W/f/TERMO.BIN
  MODULOS="$MODULOS  module /TERMO.BIN TERMO.BIN
"
  # (TERMO) X:\TERMO: «Enviar a Carlos» (el testigo), «Terminal» (la llave de
  # la memoria) (termo/prepara_memoria.sh: lo de cada memoria es distinto)
  bash $AQUI/termo/prepara_memoria.sh $W/termo
fi
cp $W/f/* $W/m/
[ -n "${TERMO:-}" ] && cp -r $W/termo $W/m/TERMO
# lo que solo es para instalar (los textos del sistema instalado, lo que va a
# otra carpeta y el arranque del disco), en \INSTALA
mkdir $W/m/INSTALA
cp -r $W/i/* $W/m/INSTALA/
mkdir $W/m/PAQUETES
cp $W/paq/*.HPQ $W/paq/CATALOGO.TXT $W/m/PAQUETES/
# la oficina de Hispa-386 (Facturas, Pluma, Cuentas, Mirador: programas de
# Hispa Ventanas) y su ejecutor, HVPRUN.BIN: el menú Inicio los abre de ahí
# (ya están hechos, más arriba, para la entrada de instalar)
cp $W/of/HVPRUN.BIN $W/m/
mkdir -p $W/m/OFICINA
cp $W/of/*.HVP $W/of/*.DAT $W/m/OFICINA/
# [WIN32] las DLL de los programas de Windows y sus ejemplos
cp -r $W/w32/WIN32 $W/w32/HISPA32 $W/m/
# [HISPA-SNES] el emulador de la consola de 16 bits (docs/HISPASNES.md: un
# programa de Hispa Ventanas, como la oficina) en \JUEGOS, con PRUEBA.SFC, la
# ROM nuestra: se abre desde Mi equipo con HVPRUN (solo en la memoria USB)
RAIZ=$(readlink -f $AQUI/../..)
mkdir -p $W/m/JUEGOS $W/sn
python3 -I $RAIZ/tools/mkhvp.py $RAIZ/programas/hispasnes.asm $W/sn/HISPASNS.HVP > /dev/null
python3 -I $RAIZ/tools/asm816.py $RAIZ/tests/hispasnes/demo.s $W/sn/PRUEBA.SFC
cp $W/sn/HISPASNS.HVP $W/sn/PRUEBA.SFC $W/m/JUEGOS/
# [HISPA-DRIVE] y el de la de 1988 (docs/HISPADRIVE.md), con su ROM de
# prueba (PRUEBA.MD, la ya ensamblada: tests/hispadrv/), igual
mkdir -p $W/md
python3 -I $RAIZ/tools/gen68k.py $RAIZ/build/gen/hispadrv/m68k_gen.inc 2> /dev/null || { mkdir -p $RAIZ/build/gen/hispadrv; python3 -I $RAIZ/tools/gen68k.py $RAIZ/build/gen/hispadrv/m68k_gen.inc 2> /dev/null; }
python3 -I $RAIZ/tools/genym.py $RAIZ/build/gen/hispadrv/ym_tablas.inc
python3 -I $RAIZ/tools/mkhvp.py $RAIZ/programas/hispadrv.asm $RAIZ/build/HISPADRV.HVP > /dev/null
cp $RAIZ/build/HISPADRV.HVP $W/m/JUEGOS/
cp $RAIZ/tests/hispadrv/PRUEBA.MD $W/m/JUEGOS/
# (SONIDO) y los de 8 bits, que desde el 21/11 corren en HVPRUN (y suenan):
# Hispa-Nes con HISPA1.NES e Hispa-System con BUZO.SMS, nuestras
python3 -I $RAIZ/tools/mkhvp.py $RAIZ/programas/hispanes.asm $W/sn/HISPANES.HVP > /dev/null
python3 -I $RAIZ/tools/asm6502.py $RAIZ/nes/hispa1/hispa1.s $W/sn/HISPA1.NES > /dev/null
python3 -I $RAIZ/tools/mkhvp.py $RAIZ/programas/hispasms.asm $W/sn/HISPASMS.HVP > /dev/null
python3 -I $RAIZ/tools/asmz80.py $RAIZ/sms/buzo/buzo.s $W/sn/BUZO.SMS > /dev/null
cp $W/sn/HISPANES.HVP $W/sn/HISPA1.NES $W/sn/HISPASMS.HVP $W/sn/BUZO.SMS $W/m/JUEGOS/
mkdir -p $W/m/boot/grub
{ cat <<'G'
set timeout=8
set default=0
# (UEFI) por UEFI, solo la pantalla del firmware (GOP, o UGA en los Mac
# viejos): con all_video, en QEMU, GRUB usaría la VBE de Bochs por su cuenta
if [ "$grub_platform" = efi ]; then
  insmod efi_gop
  insmod efi_uga
else
  insmod all_video
fi
# [EDID] la medida elegida en Configuración (\PANTALLA.ENV, la escribe el
# escritorio): la usa la primera entrada. prueba=1, se va a probar; 2, se esta
# probando (si se vuelve a arrancar sin darla por buena, vuelve la de antes)
set modo=
if [ -f /PANTALLA.ENV ]; then
  load_env -f /PANTALLA.ENV pantalla antes prueba
  if [ "$prueba" = 2 ]; then
    set pantalla="$antes"
    set antes=
    set prueba=0
    save_env -f /PANTALLA.ENV pantalla antes prueba
  elif [ "$prueba" = 1 ]; then
    set prueba=2
    save_env -f /PANTALLA.ENV prueba
  fi
  if [ "$pantalla" = auto ]; then
    set modo=auto
  elif [ -n "$pantalla" ]; then
    set modo="${pantalla}x32,${pantalla},800x600x32,800x600,auto"
  fi
fi
G
  # el rótulo, el núcleo, lo que se le dice y, si hace falta, el modo que
  # se le pide a GRUB (después de «multiboot», que pone el de la cabecera)
  entrada() {
    printf 'menuentry "%s" {\n  multiboot /%s %s\n%s' "$1" "$2" "$3" "$MODULOS"
    [ -n "${4:-}" ] && printf '  set gfxpayload=%s\n' "$4"
    [ -n "${5:-}" ] && printf '%s\n' "$5"
    printf '  boot\n}\n'
  }
  N="escritura=usb unidad=X registro"
  # (TERMO) la memoria del Termómetro arranca sola a los 5 s
  if [ -n "${TERMO:-}" ]; then echo "set timeout=5"; fi
  # (UEFI) por UEFI: multiboot2 y module2, y la pantalla del firmware (GOP:
  # «auto», la que dice la pantalla (su EDID) o, si no lo dice, la que ha
  # dejado el firmware; o la elegida en Configuración, o la que se pide; con
  # «keep» no: GRUB, con la consola de texto de EFI, no tiene modo que
  # guardar y el núcleo se quedaría sin pantalla). El cambio de medida en marcha no va (no hay BIOS): Configuración
  # la guarda para el próximo arranque, como en un PC sin la VBE de Bochs
  if [ -n "$EFI_PLAT" ]; then
    M2=$(a_module2 "$MODULOS")
    M2="$M2
"
    I2=$(a_module2 "$INSTALA")
    I2="$I2
"
    echo 'if [ "$grub_platform" = efi ]; then'
    # (TERMO) y por UEFI, también primero
    if [ -n "${TERMO:-}" ]; then
      menu_efi "Termometro (UEFI): medir la temperatura (la pantalla a su medida)" ESCRIT8.BIN "escritorio $N amedida termo" "$M2" auto
      menu_efi "Termometro (UEFI, 1024 x 600)" ESCRIT6.BIN "escritorio $N termo" "$M2" "1024x600x32,1024x600,auto"
      menu_efi "Termometro (UEFI, 800 x 600)" ESCRIT8.BIN "escritorio $N termo" "$M2" "800x600x32,800x600,auto"
      menu_efi "Termometro SIN ACPI (UEFI)" ESCRIT8.BIN "escritorio $N sinacpi termo" "$M2" auto
    fi
    menu_efi "Hispa-Pentium 0.8 (UEFI): el escritorio (la medida de la pantalla, o la elegida en Configuracion)" ESCRIT8.BIN "escritorio $N" "$M2" auto '  if [ "$modo" = auto ]; then
    set gfxpayload=auto
  elif [ -n "$modo" ]; then
    echo "Hispa-Pentium: la pantalla, a $pantalla (si el firmware no la da, la que hay)"
    set gfxpayload="${pantalla}x32,${pantalla},auto"
  fi'
    menu_efi "Hispa-Pentium 0.8 (UEFI): el escritorio a la medida de la pantalla" ESCRIT8.BIN "escritorio $N amedida" "$M2" auto
    menu_efi "Hispa-Pentium 0.8 (UEFI): el escritorio (1024 x 600)" ESCRIT6.BIN "escritorio $N" "$M2" "1024x600x32,1024x600,auto"
    menu_efi "Hispa-Pentium 0.8 (UEFI): el escritorio (1366 x 768)" ESCRIT13.BIN "escritorio $N" "$M2" "1366x768x32,1366x768,auto"
    menu_efi "Hispa-Pentium 0.8 (UEFI): el escritorio (1024 x 768)" ESCRIT10.BIN "escritorio $N" "$M2" "1024x768x32,1024x768,auto"
    menu_efi "Hispa-Pentium 0.8 (UEFI): el escritorio SIN tocar el USB, los discos, la red ni ACPI" ESCRIT8.BIN "escritorio sindisco sinusb sinred sinacpi" "$M2" auto
    cat <<'G'
menuentry "Ver los modos de video que da el firmware (GOP) de este equipo" {
  insmod videoinfo
  set pager=1
  videoinfo
  echo
  echo "Los modos de 32 bits valen. Pulse Intro para volver."
  read nada
}
G
    menu_efi "Hispa-Pentium 0.8 (UEFI): el interprete de ordenes (texto, en la pantalla del firmware)" SEMILLA.BIN "ordenes $N" "$M2" auto
    menu_efi "Hispa-Pentium 0.8 (UEFI): la consola del nucleo: dice los discos y el USB que encuentra" SEMILLA.BIN "$N" "$M2" auto
    menu_efi "Instalar Hispa-Pentium 0.8 en el disco duro (arrancara por la BIOS: Legacy/CSM)" SEMILLA.BIN "instalar unidad=M" "$I2" auto '  echo "Ojo: el disco instalado arranca por la BIOS de siempre (Legacy/CSM);"
  echo "por UEFI, todavia no. Sin Legacy/CSM, siga usando la memoria USB."
  sleep 5'
    cat <<'G'
menuentry "Volver a la configuracion del firmware (UEFI)" {
  fwsetup
}
else
G
  fi
  # (TERMO) la memoria del Termómetro, por la BIOS: primero (y sola, a los 5 s)
  if [ -n "${TERMO:-}" ]; then
    entrada "Termometro: medir la temperatura (la pantalla a su medida)" ESCRIT8.BIN "escritorio $N amedida termo" "auto"
    entrada "Termometro (1024 x 600)" ESCRIT6.BIN "escritorio $N termo"
    entrada "Termometro (800 x 600)" ESCRIT8.BIN "escritorio $N termo"
    entrada "Termometro SIN ACPI (si se queda colgado al arrancar: sin zonas ni bateria)" ESCRIT8.BIN "escritorio $N sinacpi termo"
  fi
  # [EDID] la primera, con la medida de PANTALLA.ENV si la hay (y lo dice
  # antes de arrancar); «a la medida», con «amedida» (el núcleo sabe así
  # que la que pone GRUB es la de la pantalla)
  entrada "Hispa-Pentium 0.8: el escritorio (800 x 600, o la elegida en Configuracion)" ESCRIT8.BIN "escritorio $N" "" '  if [ -n "$modo" ]; then
    echo "Hispa-Pentium: la pantalla, a $pantalla (si la BIOS no la da, otra)"
    set gfxpayload=$modo
  fi'
  entrada "Hispa-Pentium 0.8: el escritorio a la medida de la pantalla" ESCRIT8.BIN "escritorio $N amedida" "auto"
  entrada "Hispa-Pentium 0.8: el escritorio (1024 x 600)" ESCRIT6.BIN "escritorio $N"
  entrada "Hispa-Pentium 0.8: el escritorio (1366 x 768)" ESCRIT13.BIN "escritorio $N"
  entrada "Hispa-Pentium 0.8: el escritorio (1024 x 768)" ESCRIT10.BIN "escritorio $N"
  entrada "Hispa-Pentium 0.8: el escritorio SIN tocar el USB, los discos, la red ni ACPI (como la 0.5)" ESCRIT8.BIN "escritorio sindisco sinusb sinred sinacpi"
  cat <<'G'
menuentry "Ver los modos de video que da la BIOS de este equipo" {
  insmod videoinfo
  set pager=1
  videoinfo
  echo
  echo "Los modos de 32, 24 o 16 bits (Direct color) valen. Pulse Intro para volver."
  read nada
}
G
  entrada "Hispa-Pentium 0.8: el interprete de ordenes (modo texto)" SEMILLA.BIN "ordenes $N"
  entrada "Hispa-Pentium 0.8: la consola del nucleo (modo texto): dice los discos y el USB que encuentra" SEMILLA.BIN "$N"
  printf 'menuentry "Instalar Hispa-Pentium 0.8 en el disco duro" {\n  multiboot /SEMILLA.BIN instalar unidad=M\n%s  boot\n}\n' "$INSTALA"
  [ -n "$EFI_PLAT" ] && echo fi         # (UEFI)
} > $W/m/boot/grub/grub.cfg
grub-install --target=i386-pc --boot-directory=$W/m/boot --modules="part_msdos fat multiboot all_video videoinfo read echo" $L > $W/grub.txt 2>&1 || { cat $W/grub.txt; exit 1; }
# (UEFI) el GRUB EFI, en \EFI\BOOT, y lo que hay que saber
if [ -n "$EFI_PLAT" ]; then
  haz_efi $W/m || { echo "usb.sh: no se puede hacer el GRUB EFI"; exit 1; }
  leeme_uefi $W/m
  echo "usb.sh: (UEFI) arranca también por UEFI: $EFI_PLAT"
fi
sync
umount $W/m
losetup -d $L
L=
cp $IMG "$OUT"
echo "usb.sh: $OUT, $(($(stat -c %s "$OUT") / 1048576)) MB"
