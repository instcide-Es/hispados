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
# (UEFI) Desde la 0.8.9 arranca también por UEFI (x64 y, si está su GRUB,
# ia32: los Bay Trail como el Acer One 10): en la misma partición FAT (la
# tabla sigue siendo la del MBR, como la de Rufus «MBR para BIOS o UEFI»)
# van \EFI\BOOT\BOOTX64.EFI y BOOTIA32.EFI, el GRUB EFI (nucleo32/uefi.sh),
# que lee el mismo /boot/grub/grub.cfg: con «$grub_platform» = efi, sus
# entradas son las de multiboot2 y la pantalla es la del firmware (GOP). Y
# LEEME-UEFI.TXT (Secure Boot apagado). Sin grub-efi-amd64-bin ni
# grub-efi-ia32-bin, solo BIOS, como antes; SIN_UEFI=1, también.
# (SDHCI) Con PRUEBA_SDHCI=1, la memoria de prueba del eMMC y la tarjeta SD: sus
# cuatro entradas primero (sin ACPI, con ACPI, la consola y por PIO).
# (RTL8723BS) Con PRUEBA_WIFI_SDIO=1, la del Wi-Fi por SDIO de las tabletas: sus
# tres entradas primero (sin ACPI, con ACPI —aprende \SDIO.TXT— y las órdenes)
# y un RED.INI con Registro=SI (RED.LOG) y Preferir=WIFI.
# (0.8.12) Las entradas con ACPI de las dos llevan «fichaequipo»: ACPI.BIN deja
# la ficha técnica en X:\EQUIPO (las tablas ACPI y PCI.TXT) y en ACPI.TXT los
# aparatos de las I2C (la pantalla táctil y el acelerómetro).
# Necesita ser root (losetup, mount), python3, nasm, parted, dosfstools y
# grub-pc-bin (grub-install --target=i386-pc); (UEFI) y grub-efi-amd64-bin
# y grub-efi-ia32-bin (grub-mkimage).
set -eu
# (RTL8723BS, 0.8.12) con PRUEBA_SDHCI y PRUEBA_WIFI_SDIO a la vez (la memoria
# de instalar del Acer One 10), las del eMMC y la SD son la 1 a la 4 y las del
# Wi-Fi, la 5 a la 7
if [ -n "${PRUEBA_SDHCI:-}" ]; then W1="5. "; W2="6. "; W3="7. "; else W1="1. "; W2="2. "; W3="3. "; fi
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
instala_efi $W/i                # (SDHCI) y el del disco instalado
IMG=$W/usb.img
truncate -s ${USB_MB:-64}M $IMG   # (USB_MB=256: con sitio para las firmas de Escudo)
parted -s $IMG mklabel msdos mkpart primary fat16 1MiB 100% set 1 boot on
# El dispositivo loop y su partición: con otras pruebas haciendo lo mismo a
# la vez, la partición puede tardar, no aparecer (el examen de particiones,
# ocupado) o desaparecer justo después (udev, al quitar la de otro loop que
# acaba de soltarse con el mismo número): se espera, se pide otra vez y, si
# no, se suelta y se prueba con otro loop (hasta 6 veces)
loop_listo() {
  local i
  for i in $(seq 30); do
    if [ -b ${L}p1 ] && mkfs.fat -F 16 -n HISPAPENT ${L}p1 > $W/mkfs.txt 2>&1 && mount ${L}p1 $W/m 2>> $W/mkfs.txt; then return 0; fi
    [ $((i % 5)) = 0 ] && { partprobe $L 2> /dev/null || partx -u $L 2> /dev/null || true; }
    sleep 0.2
  done
  return 1
}
for intento in 1 2 3 4 5 6; do
  L=$(losetup -fP --show $IMG) || { L=; [ $intento = 6 ] && { echo "usb.sh: no hay un loop libre"; exit 1; }; sleep $intento; continue; }
  loop_listo && break
  echo "usb.sh: $L sin su partición (vez $intento): $(tail -n 1 $W/mkfs.txt 2> /dev/null)" >&2
  losetup -d $L 2> /dev/null || true
  L=
  [ $intento = 6 ] && { echo "usb.sh: no se puede preparar la partición de la memoria"; exit 1; }
  sleep $intento
done
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
# (RTL8723BS) la memoria de prueba del Wi-Fi por SDIO: RED.LOG y el Wi-Fi primero
[ -n "${PRUEBA_WIFI_SDIO:-}" ] && printf 'Registro=SI\r\nPreferir=WIFI\r\n' > $W/m/RED.INI
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
      # (TERMO) para aislar un congelado (el Acer One 10): sin el 8042, o con
      # ACPI pero sin mirar sus dispositivos
      menu_efi "Termometro SIN el 8042 (UEFI)" ESCRIT8.BIN "escritorio $N sin8042 termo" "$M2" auto
      menu_efi "Termometro con ACPI sin mirar sus dispositivos (UEFI)" ESCRIT8.BIN "escritorio $N acpisindisp termo" "$M2" auto
      # (RAM) la prueba completa de la memoria, sola al abrirse (termo/ram.c)
      menu_efi "Termometro + prueba COMPLETA de la memoria RAM (UEFI; sola, de 15 min a 1 h)" ESCRIT8.BIN "escritorio $N amedida termo ramcompleta" "$M2" auto
    fi
    # (SDHCI) con PRUEBA_SDHCI=1, la memoria de prueba del eMMC y la tarjeta SD
    # (las tabletas Bay Trail: Acer One 10, Medion S2217), primero: sin ACPI
    # (el Acer se congela con ACPI.BIN), con ACPI, la consola y por PIO
    if [ -n "${PRUEBA_SDHCI:-}" ]; then
      menu_efi "1. eMMC y SD, SIN ACPI: el escritorio (pruebe esta primero)" ESCRIT8.BIN "escritorio $N sinacpi" "$M2" auto
      menu_efi "2. eMMC y SD, CON ACPI: el escritorio (si con la 1 no salen; puede congelarse)" ESCRIT8.BIN "escritorio $N fichaequipo" "$M2" auto
      menu_efi "3. eMMC y SD, SIN ACPI: la consola del nucleo (texto: dice los discos)" SEMILLA.BIN "$N sinacpi" "$M2" auto
      menu_efi "4. eMMC y SD, SIN ACPI, despacio (PIO, 1 bit, sin alta velocidad)" ESCRIT8.BIN "escritorio $N sinacpi sdhcipio sdhci1bit sdhcilento" "$M2" auto
    fi
    if [ -n "${PRUEBA_WIFI_SDIO:-}" ]; then
      menu_efi "${W1}Wi-Fi (Realtek por SDIO), SIN ACPI: el escritorio" ESCRIT8.BIN "escritorio $N sinacpi conred" "$M2" auto
      menu_efi "${W2}Wi-Fi, CON ACPI: aprende la controladora y como se enciende (puede congelarse)" ESCRIT8.BIN "escritorio $N conred fichaequipo" "$M2" auto
      menu_efi "${W3}Wi-Fi, SIN ACPI: el interprete de ordenes (texto: REDWIFI /REDES...)" SEMILLA.BIN "ordenes $N sinacpi conred" "$M2" auto
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
    menu_efi "Instalar Hispa-Pentium 0.8 en el disco duro (o en una tarjeta SD)" SEMILLA.BIN "instalar unidad=M" "$I2" auto
    cat <<'G'
menuentry "Diagnostico (UEFI): la pantalla, la memoria, el PCI y ACPI (para hacer una foto)" {
  set pager=1
  echo "== Hispa-Pentium: diagnostico del arranque por UEFI =="
  echo "-- la pantalla (GOP): los modos y el de ahora (su direccion) --"
  videoinfo
  echo "-- el procesador (cpuid -l: 64 bits) --"
  if cpuid -l; then echo "64 bits: si"; else echo "64 bits: no"; fi
  echo "-- las tablas de ACPI (HPET: el reloj que sustituye al 8254) --"
  lsacpi
  echo "-- la tabla del sistema de EFI --"
  lsefisystab
  echo "-- el PCI (clase 0300: la pantalla; 0C03: USB; 0108: NVMe) --"
  lspci
  echo "-- el mapa de memoria --"
  lsmmap
  echo
  echo "Haga una foto de cada pantalla. Pulse Intro para volver."
  read nada
}
G
    cat <<'G'
menuentry "Volver a la configuracion del firmware (UEFI)" {
  fwsetup
}
else
G
  fi
  # (TERMO) la memoria del Termómetro, por la BIOS: primero (y sola, a los 5 s).
  # Con «auto», la VBE de GRUB pone la medida que dice la pantalla (su EDID)
  # y, si no la dice, 640 x 480 (el MSI GT80, con su panel de 1920 x 1080, se
  # veía enorme): GRUB no deja saber desde el menú si hay EDID, así que la
  # primera pide 1920 x 1080, 1366 x 768 o 1024 x 768 (la primera que dé la
  # BIOS) y solo si no, «auto»; la de la pantalla (EDID), en la segunda
  if [ -n "${TERMO:-}" ]; then
    entrada "Termometro: medir la temperatura (1920 x 1080, 1366 x 768 o 1024 x 768)" ESCRIT8.BIN "escritorio $N termo" "1920x1080x32,1920x1080x24,1366x768x32,1366x768x24,1024x768x32,1024x768x24,auto"
    entrada "Termometro (la medida que diga la pantalla; si no la dice, 640 x 480)" ESCRIT8.BIN "escritorio $N amedida termo" "auto"
    entrada "Termometro (1024 x 600)" ESCRIT6.BIN "escritorio $N termo"
    entrada "Termometro (800 x 600)" ESCRIT8.BIN "escritorio $N termo"
    entrada "Termometro SIN ACPI (si se queda colgado al arrancar: sin zonas ni bateria)" ESCRIT8.BIN "escritorio $N sinacpi termo"
    entrada "Termometro SIN el 8042 (el teclado y el raton PS/2)" ESCRIT8.BIN "escritorio $N sin8042 termo"
    entrada "Termometro + prueba COMPLETA de la memoria RAM (sola al abrirse; de 15 min a 1 h)" ESCRIT8.BIN "escritorio $N termo ramcompleta" "1920x1080x32,1920x1080x24,1366x768x32,1366x768x24,1024x768x32,1024x768x24,auto"
  fi
  # (SDHCI) la memoria de prueba del eMMC y la SD, por la BIOS (el Medion con la HispaBIOS)
  if [ -n "${PRUEBA_SDHCI:-}" ]; then
    entrada "1. eMMC y SD, SIN ACPI: el escritorio (pruebe esta primero)" ESCRIT8.BIN "escritorio $N sinacpi"
    entrada "2. eMMC y SD, CON ACPI: el escritorio (si con la 1 no salen; puede congelarse)" ESCRIT8.BIN "escritorio $N fichaequipo"
    entrada "3. eMMC y SD, SIN ACPI: la consola del nucleo (texto: dice los discos)" SEMILLA.BIN "$N sinacpi"
    entrada "4. eMMC y SD, SIN ACPI, despacio (PIO, 1 bit, sin alta velocidad)" ESCRIT8.BIN "escritorio $N sinacpi sdhcipio sdhci1bit sdhcilento"
  fi
  if [ -n "${PRUEBA_WIFI_SDIO:-}" ]; then
    entrada "${W1}Wi-Fi (Realtek por SDIO), SIN ACPI: el escritorio" ESCRIT8.BIN "escritorio $N sinacpi conred"
    entrada "${W2}Wi-Fi, CON ACPI: aprende la controladora y como se enciende (puede congelarse)" ESCRIT8.BIN "escritorio $N conred fichaequipo"
    entrada "${W3}Wi-Fi, SIN ACPI: el interprete de ordenes (texto: REDWIFI /REDES...)" SEMILLA.BIN "ordenes $N sinacpi conred"
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
