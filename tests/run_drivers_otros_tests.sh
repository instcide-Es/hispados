#!/bin/bash
# Prueba de los controladores de DOS de otros (docs/DRIVERS-MICROSOFT.md):
# SRDXMS.SYS de FreeDOS (GPL), comprimido con UPX, que necesita que al driver
# se le llame con DS:SI en su cabecera, como hace DOS (antes, la máquina se
# reiniciaba al cargarlo); y SRDISK.EXE, que le cambia el tamaño: el driver
# contesta «no se sabe» a MEDIA CHECK y da las medidas nuevas con BUILD BPB.
# Si no están (tests/thirdparty/fetch_drivers.sh los baja), se salta.
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
SRD=tests/thirdparty/dl/srdisk/BIN
echo "== Hispa-DOS: controladores de otros (SRDISK, de FreeDOS) =="
if [ ! -f tests/thirdparty/dl/srdisk/.ok ]; then
  echo "  (se salta: falta SRDISK; tests/thirdparty/fetch_drivers.sh lo baja)"
  exit 0
fi
FLP=$(mktemp --suffix=.img); cp "$IMG" "$FLP"
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDEVICE=A:\\SRDXMS.SYS\r\n' > build/cfg_drivers_test.sys
mcopy -o -i "$FLP" build/cfg_drivers_test.sys ::CONFIG.SYS
mcopy -o -i "$FLP" $SRD/SRDXMS.SYS $SRD/SRDISK.EXE ::
LOG=$(mktemp)
send() { sleep "${2:-1.5}"; printf '%s\r' "$1"; }
{
  sleep 5
  send "SRDISK 200" 3
  send "COPY README.TXT X:" 2
  send "DIR X:" 2
  send "SRDISK 300" 3
  send "DIR X:" 2
  send "TYPE X:README.TXT" 2
  sleep 2
} | timeout 40 qemu-system-i386 -display none -serial stdio -monitor none -m 16 \
      -drive file="$FLP",format=raw,if=floppy -boot a -no-reboot > "$LOG" 2>&1
tr -d '\r' < "$LOG" | iconv -f CP437 -t UTF-8 > "$LOG.txt"
fail=0
check() {
  if grep -qF -- "$1" "$LOG.txt"; then echo "  OK   $1"; else echo "  FAIL $1"; fail=1; fi
}
check "Installed RAMDrive X:"
check "XMS: Allocated 200 K"
check "1 archivo(s) copiado(s)"
check "202.240 bytes libres"
check "Disk resized"
check "304.128 bytes libres"
check "Welcome to Hispa-DOS!"
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then echo "--- salida ---"; cat "$LOG.txt"; fi
rm -f "$FLP" "$LOG" "$LOG.txt"
exit $fail
