#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# Pruebas de Castel y de Hispa-Pentium (castel/pruebas.sh) en el PC de
# Albinyana, en RAM y en paralelo: cada parte («SOLO=…») en su propia copia,
# a la vez. Pensado para aliviar el VPS cuando hay muchas ramas probándose.
#
#   tools/pruebas_castel_pc.sh [-j N] [PARTE...]
#
# Sin PARTE: todas (16 06 07 castel32 caliente paquetes red acpi tls cd
# pantalla navegador dispositivos y 32, que es la única que lleva la octava).
# SRC: la carpeta del proyecto (por omisión, la de este guion); NOMBRE: la
# carpeta en la RAM del PC. Necesita WSL abierto en el PC (Tailscale) y sudo
# sin contraseña (usb.sh y cd.sh montan imágenes con losetup).
# Registros: build/pc-castel/<parte>.log (aquí) y un resumen al final.
# RAIZ: dónde van las copias en el PC. Es un tmpfs propio en /mnt/castelpc y no
# /dev/shm, porque xorriso se niega a crear imágenes bajo /dev (la parte cd
# fallaba allí, 22/11/2026) y así tampoco se llena el /dev/shm de WSL.
set -u
cd "${SRC:-$(dirname "$0")/..}"
NOMBRE=${NOMBRE:-castelpc}
J=14
if [ "${1:-}" = "-j" ]; then J=$2; shift 2; fi
PARTES=${*:-16 06 07 castel32 caliente paquetes red acpi tls cd pantalla navegador dispositivos 32}
DEST=${DEST:-carlos@100.126.100.79}
SSH="ssh -p ${PORT:-2222} -i ${KEY:-/root/.ssh/id_ed25519_pc_albinyana} -o BatchMode=yes -o ConnectTimeout=15 -o ServerAliveInterval=30 $DEST"
$SSH true 2>/dev/null || { echo "No llego al PC. ¿Está Ubuntu (WSL) abierto?"; exit 2; }
RAIZ=${RAIZ:-/mnt/castelpc}
$SSH "sudo mkdir -p $RAIZ && { mountpoint -q $RAIZ || sudo mount -t tmpfs -o size=60% tmpfs $RAIZ; } && sudo chmod 1777 $RAIZ" \
  || { echo "No se ha podido preparar $RAIZ en el PC"; exit 3; }
# 1) herramientas (la primera vez)
$SSH 'command -v grub-mkrescue >/dev/null && command -v xorriso >/dev/null && command -v parted >/dev/null && command -v qemu-system-i386 >/dev/null' \
  || $SSH 'sudo DEBIAN_FRONTEND=noninteractive apt-get install -y -qq grub-pc-bin grub-common xorriso parted dosfstools mtools curl zip unzip xxd gcc-multilib qemu-system-x86 qemu-utils socat nasm python3 >/dev/null' \
  || { echo "No se han podido instalar las herramientas en el PC"; exit 3; }
# 2) una copia del proyecto por parte, en la RAM del PC
echo "copiando el proyecto al PC..."
tar --exclude=./build --exclude=./.git -czf - . | $SSH "sudo rm -rf $RAIZ/$NOMBRE; mkdir -p $RAIZ/$NOMBRE/base && tar -xzf - -C $RAIZ/$NOMBRE/base"
# 3) las partes, de J en J; cada una en su copia y con su TMPDIR
echo "partes: $PARTES ($J a la vez)"
inicio=$(date +%s)
$SSH "cd $RAIZ/$NOMBRE && for p in $PARTES; do echo \$p; done | xargs -P $J -I{} bash -c '
  cp -a base c-{} && cd c-{}/castel && mkdir -p ../tmp && \
  sudo -E env TMPDIR=$RAIZ/$NOMBRE/c-{}/tmp TZ=UTC PATH=\$PATH:/usr/sbin:/sbin SOLO={} timeout -k 30 7200 ./pruebas.sh > ../../{}.log 2>&1; echo \"\$? {}\" > ../../{}.rc'"
echo "tiempo: $(( $(date +%s) - inicio )) s"
# 4) traer los registros y resumir
rm -rf build/pc-castel && mkdir -p build/pc-castel
$SSH "cd $RAIZ/$NOMBRE && tar -cf - *.log *.rc" | tar -xf - -C build/pc-castel
for p in $PARTES; do
  r=$(grep -h "bien, .* mal" build/pc-castel/$p.log 2>/dev/null | tail -1)
  printf '  %-13s %s\n' "$p" "${r:-SIN RESUMEN (mire build/pc-castel/$p.log)}"
done
$SSH "sudo rm -rf $RAIZ/$NOMBRE"
