#!/bin/bash
# Hispa-DOS - imagen de prueba de HispaBIOS para un equipo de verdad que
# arranca por UEFI (docs/HISPABIOS.md): el reloj, la velocidad de la pantalla,
# Hispa Ventanas (VBE) y el registro. A partir de
# build/hispados-usb.img (make usb): los mismos BOOTX64.EFI y BOOTIA32.EFI
# ensamblados con -DREGISTRO_PASOS, que guardan \HISPABIOS.LOG en la memoria
# USB en cada paso aunque todo vaya bien (la imagen normal solo lo guarda si
# algo falla). Deja en la carpeta de destino el .zip con la imagen, los LEEME
# y los .EFI sueltos.
#   tools/mkprueba_hispabios.sh [carpeta de destino] [acer | raton | medion]
# (por omisión, build/prueba-hispabios). Con «acer», la misma imagen con otro
# nombre (hispados-usb-acer-one-10.zip) y solo lo del firmware de 32 bits
# (LEEME-ACER.TXT, sueltos/BOOTIA32.EFI): no toca lo que haya de la de 64.
# Con «raton» (HispaBIOS 0.5, el ratón con UEFI), hispados-usb-raton.zip con
# LEEME-RATON.TXT (como LEEME.TXT), MOUSETST.COM y PRUEBARA.BAT, y los dos .EFI.
# Con «medion» (HispaBIOS 0.6, el teclado USB del firmware: MEDION S2217),
# hispados-usb-medion.zip con LEEME-MEDION.TXT (y LEEME.TXT en la carpeta),
# TECLAPRU.COM y un AUTOEXEC.BAT que lanza sola la prueba del teclado (20 s,
# no hace falta teclado para que acabe) y guarda HBLOG.TXT; sueltos/BOOTIA32.EFI.
set -eu
cd "$(dirname "$0")/.."
B=build
D=${1:-$B/prueba-hispabios}
[ -f $B/hispados-usb.img ] || { echo "falta $B/hispados-usb.img (make usb)"; exit 1; }
mkdir -p "$D" "$D/sueltos" $B/prueba-hispabios-tmp
D=$(realpath "$D")
T=$B/prueba-hispabios-tmp
IMG=$T/hispados-usb.img
cp --sparse=always $B/hispados-usb.img $IMG
P="$IMG@@$((2048 * 512))"
export MTOOLS_SKIP_CHECK=1
FECHA=$(date +%d/%m/%Y); HORA=$(date +%H:%M)
VERSION=$(git describe --always --dirty 2>/dev/null || echo "sin git")
export FECHA HORA VERSION
txt() { python3 -c "
import os, sys
t = open(sys.argv[1], encoding='utf-8').read().replace('\r\n', '\n').replace('\n', '\r\n')
for k in ('FECHA', 'HORA', 'VERSION'): t = t.replace('@' + k + '@', os.environ[k])
open(sys.argv[2], 'wb').write(t.encode('cp437'))" "$1" "$2"; }
nasm -f bin -I hispabios/ -I $B/ -DREGISTRO_PASOS -o $T/BOOTX64.EFI hispabios/efi64.asm
mcopy -o -i "$P" $T/BOOTX64.EFI ::EFI/BOOT/BOOTX64.EFI
nasm -f bin -I hispabios/ -I $B/ -DREGISTRO_PASOS -o $T/BOOTIA32.EFI hispabios/efi32.asm
mcopy -o -i "$P" $T/BOOTIA32.EFI ::EFI/BOOT/BOOTIA32.EFI
txt usb/prueba-hispabios/LEEME.TXT $T/LEEME-HB.TXT
mcopy -o -i "$P" $T/LEEME-HB.TXT ::
txt usb/prueba-hispabios/LEEME-ACER.TXT $T/LEEME-AC.TXT
mcopy -o -i "$P" $T/LEEME-AC.TXT ::
# PRUEBAHB.BAT: guarda el registro de la BIOS (HBLOG), arranca Hispa Ventanas
# y lo guarda otra vez al salir (con las llamadas de VBE que haya hecho)
printf '@ECHO OFF\r\nC:\r\nCD \\\r\nC:\\HISPADOS\\HBLOG\r\nC:\\HISPADOS\\VENTANAS\r\nC:\\HISPADOS\\HBLOG\r\n' > $T/PRUEBAHB.BAT
mcopy -o -i "$P" $T/PRUEBAHB.BAT ::
Z=hispados-usb-hispabios.zip
[ "${2:-}" = acer ] && Z=hispados-usb-acer-one-10.zip
if [ "${2:-}" = raton ]; then
  # PRUEBARA.BAT: lo que dice HSPRATON, el registro (HBLOG), MOUSETST (mover y
  # pulsar; una tecla acaba), Hispa Ventanas, y otra vez el registro y HSPRATON
  # (con las cuentas de lo que ha llegado)
  Z=hispados-usb-raton.zip
  txt usb/prueba-hispabios/LEEME-RATON.TXT $T/LEEME-RA.TXT
  mcopy -o -i "$P" $T/LEEME-RA.TXT ::
  mcopy -o -i "$P" $B/MOUSETST.COM ::MOUSETST.COM
  printf '@ECHO OFF\r\nC:\r\nCD \\\r\nC:\\HISPADOS\\HSPRATON /INFO > \\RATON.TXT\r\nC:\\HISPADOS\\HBLOG > NUL\r\nECHO Mueva el raton o el panel; pulse sus botones. Una tecla acaba.\r\n\\MOUSETST\r\nC:\\HISPADOS\\VENTANAS\r\nC:\\HISPADOS\\HBLOG > NUL\r\nC:\\HISPADOS\\HSPRATON /INFO >> \\RATON.TXT\r\nECHO Hecho: suba HISPABIOS.LOG, HBLOG.TXT y RATON.TXT\r\n' > $T/PRUEBARA.BAT
  mcopy -o -i "$P" $T/PRUEBARA.BAT ::
fi
if [ "${2:-}" = medion ]; then
  # AUTOEXEC.BAT: tras PATH, TECLAPRU (la espera con HLT del núcleo, 20 s;
  # deja \TECLAPRU.TXT) y HBLOG (deja \HBLOG.TXT con las teclas y los tics
  # dados al firmware), sin que haga falta teclear nada
  Z=hispados-usb-medion.zip
  txt usb/prueba-hispabios/LEEME-MEDION.TXT $T/LEEME-ME.TXT
  mcopy -o -i "$P" $T/LEEME-ME.TXT ::
  nasm -f bin -o $T/TECLAPRU.COM tests/progs/teclapru.asm
  mcopy -o -i "$P" $T/TECLAPRU.COM ::TECLAPRU.COM
  mcopy -o -i "$P" ::AUTOEXEC.BAT $T/AUTOEXEC.BAT
  python3 -c "
import sys
t = open(sys.argv[1], 'rb').read()
a = b'PATH C:\\HISPADOS\r\n'
assert t.count(a) == 1, 'AUTOEXEC.BAT sin la linea de PATH'
t = t.replace(a, a + b'C:\\TECLAPRU\r\nC:\\HISPADOS\\HBLOG > NUL\r\n')
open(sys.argv[1], 'wb').write(t)" $T/AUTOEXEC.BAT
  mcopy -o -i "$P" $T/AUTOEXEC.BAT ::AUTOEXEC.BAT
fi
[ ! -e "$D/$Z" ] || mv -f "$D/$Z" "$D/$Z.ant"
(cd $T && zip -q -9 "$D/$Z" hispados-usb.img)
cp $T/BOOTIA32.EFI "$D/sueltos/"
if [ "${2:-}" = raton ]; then
  txt usb/prueba-hispabios/LEEME-RATON.TXT "$D/LEEME.TXT"
  cp $T/BOOTX64.EFI "$D/sueltos/"
elif [ "${2:-}" = medion ]; then
  txt usb/prueba-hispabios/LEEME-MEDION.TXT "$D/LEEME.TXT"
else
  txt usb/prueba-hispabios/LEEME-ACER.TXT "$D/LEEME-ACER.TXT"
fi
if [ -z "${2:-}" ]; then
  txt usb/prueba-hispabios/LEEME.TXT "$D/LEEME.TXT"
  cp $T/BOOTX64.EFI "$D/sueltos/"
fi
ls -l "$D" "$D/sueltos"
echo "imagen de prueba: $IMG (y $D/$Z)"
