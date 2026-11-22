#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/wifi/hacer.sh SALIDA [chip]
# Hace WIFI.BIN, el servicio Wi-Fi de Hispa-Pentium (docs/PENTIUM.md, «El
# Wi-Fi»): servicio.c de aquí con el núcleo Wi-Fi de Hispa-486 (wifi/: la
# gestión 802.11, WPA2 y la radio simulada, propios; brcmsmac y brcmutil, ISC,
# y bcma, GPL-2.0, de Linux 5.10, para las Broadcom; ath9k_hw de Linux, ISC,
# para las Atheros), los mismos fuentes que HSPWIFI.BIN con -DHL_PENTIUM,
# enlazado para 0x40000000 como programa del anillo 3 (como TLS.BIN). Los
# objetos, en build/wifi-pentium/ (solo lo que ha cambiado). Necesita gcc (con
# -m32), nasm, ld y objcopy. Sin «make -j».
# Con «chip», WIFISIM.BIN, el de las pruebas: el mismo, con el modelo de la
# tarjeta de tests/wifi/tarjeta_sim.c dentro (el controlador de verdad contra
# una BCM43224 de mentira; en build/wifi-pentium-chip/).
set -e
# (0.8.6) la fecha que pone __DATE__ (servicio.c y wifi/nucleo/nucleo.c): la
# del paquete wifi 0.8.5, para que WIFI.BIN salga igual octeto a octeto cada
# vez que se compila (si no, cada día cambia un octeto y el CD no lleva el
# mismo paquete que el catálogo). Al subir la versión del paquete, poner aquí su día
export SOURCE_DATE_EPOCH=${SOURCE_DATE_EPOCH:-1795262400}    # 21/11/2026 12:00 UTC
AQUI=$(dirname "$(readlink -f "$0")")
RAIZ=$(readlink -f "$AQUI/../../..")
SAL=$(readlink -f "${1:?uso: hacer.sh SALIDA}")
cd "$RAIZ"
O=build/wifi-pentium
CHIP=
if [ "${2:-}" = chip ]; then O=build/wifi-pentium-chip; CHIP="-DHL_SIMULA_ES -DCHIP_SIMULADO -Itests/wifi"; fi
mkdir -p $O
BASEF="-m32 -std=gnu11 -march=i586 -Os -ffreestanding -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables
       -fno-strict-aliasing -fno-common -mno-80387 -nostdinc -isystem $(gcc -m32 -print-file-name=include)
       -ffunction-sections -fdata-sections -D__HISPA__ -DHL_PENTIUM $CHIP"
WINC="-Iwifi/compat -Iwifi/compat/include -Iwifi/linux/include -Iwifi/linux/brcminc -Iwifi/linux/brcmsmac
      -Iwifi/linux/brcmsmac/phy -Iwifi/linux/bcma -Iwifi/linux -Iwifi/nucleo"
WCF="$BASEF $WINC -DCONFIG_BCMA_HOST_PCI -DCONFIG_BCMA_HOST_PCI_POSSIBLE -DCONFIG_BCMA_DRIVER_PCI"
AINC="-Iwifi/compat -Iwifi/compat/include -Iwifi/linux/include -Iwifi/linux/ath -Iwifi/linux/ath/ath9k -Iwifi/linux -Iwifi/nucleo"
ACF="$BASEF $AINC -DCONFIG_ATH_DEBUG -DCONFIG_ATH9K_RFKILL"
# las cabeceras: si cambia alguna, todo otra vez
CABS=$(ls tests/wifi/*.h wifi/compat/*.h wifi/nucleo/*.h wifi/linux/brcmsmac/*.h wifi/linux/brcmsmac/phy/*.h wifi/linux/bcma/*.h \
       wifi/linux/include/linux/bcma/*.h wifi/linux/ath/*.h wifi/linux/ath/ath9k/*.h castel/nucleo32/wifi/*.h)
NUEVA=0
for h in $CABS; do [ -f $O/.hecho ] && [ $h -nt $O/.hecho ] && NUEVA=1; done
[ -f $O/.hecho ] || NUEVA=1
OBJS=
# cc OBJETO FUENTE BANDERAS...: solo si ha cambiado
cc() {
  local o=$O/$1 f=$2; shift 2
  if [ $NUEVA = 1 ] || [ ! -f $o ] || [ $f -nt $o ]; then
    # shellcheck disable=SC2068
    nice gcc $@ -c $f -o $o
  fi
  OBJS="$OBJS $o"
}
LINUXF="-w -Werror=implicit-function-declaration -Werror=int-conversion -Werror=incompatible-pointer-types -Werror=return-type"
PROPIAF="-Wall -Wno-unused -Wno-packed-not-aligned"
for f in wifi/linux/brcmsmac/*.c wifi/linux/brcmsmac/phy/*.c wifi/linux/brcmutil/*.c wifi/linux/bcma/*.c wifi/linux/cordic.c; do
  # shellcheck disable=SC2086
  cc $(basename $f .c).o $f $WCF $LINUXF
done
PROPIOS="wifi/compat/base.c wifi/nucleo/radio_brcm.c wifi/nucleo/w80211.c wifi/nucleo/wpa.c wifi/nucleo/sim.c castel/nucleo32/wifi/servicio.c"
[ -n "$CHIP" ] && PROPIOS="$PROPIOS tests/wifi/tarjeta_sim.c"
for f in $PROPIOS; do
  # shellcheck disable=SC2086
  cc $(basename $f .c).o $f $WCF $PROPIAF
done
for f in wifi/linux/ath/*.c; do
  # shellcheck disable=SC2086
  cc ath_$(basename $f .c).o $f $ACF $LINUXF
done
for f in wifi/linux/ath/ath9k/*.c; do
  # shellcheck disable=SC2086
  cc ath9k_$(basename $f .c).o $f $ACF $LINUXF
done
# shellcheck disable=SC2086
cc radio_ath9k.o wifi/nucleo/radio_ath9k.c $ACF $PROPIAF
nasm -f elf32 castel/nucleo32/wifi/entrada.asm -o $O/entrada.o
# shellcheck disable=SC2086
ld -m elf_i386 --no-warn-rwx-segments -z noexecstack --gc-sections -T castel/nucleo32/wifi/pentium.ld -o $O/wifi.elf \
  $O/entrada.o $OBJS
objcopy -O binary $O/wifi.elf "$SAL"
touch $O/.hecho
