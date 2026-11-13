#!/bin/bash
# DOS=HIGH,UMB con HSPEMM: el núcleo en la HMA y, a la vez, el monitor V86
# (EMS, VCPI y memoria superior).
#  1. HSPXMS + HSPEMM + DOS=HIGH,UMB + DEVICEHIGH: el núcleo está en la HMA,
#     hay memoria superior, LH carga arriba, EMS y VCPI funcionan, cerrar la
#     línea A20 (virtual: un espejo del primer mega sobre la HMA) no deja al
#     núcleo sin su código (HMATEST /A y /C, lotes, COPY, XMS), la estrategia
#     de reserva (INT 21h AX=5800h-5803h) y HSPDPMI como cliente VCPI.
#  2. Lo mismo con DOS=HIGH,UMB en la primera línea de CONFIG.SYS (antes de
#     los DEVICE=), y con DOS=UMB a secas (núcleo abajo).
#  3. Sin HSPEMM: la estrategia de reserva sin memoria superior.
# DOS=HIGH,UMB together with HSPEMM.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=build
for f in EMSTEST.COM VCPITEST.COM UMBTEST.COM EMMMISC.COM DPMI16.COM HMATEST.COM; do
  [ -f $B/$f ] || { echo "falta $B/$f (make nucprogs)"; exit 1; }
done
T=$(mktemp -d)
cp "$IMG" $T/base.img
for f in EMSTEST VCPITEST UMBTEST EMMMISC DPMI16 DPMI32 HMATEST XMSTEST; do
  mcopy -o -i $T/base.img $B/$f.COM ::BIN/$f.COM
done
printf '@ECHO OFF\r\nA:\\BIN\\HMATEST /C\r\nECHO lote-uno\r\nA:\\BIN\\HELLO lote\r\nA:\\BIN\\HMATEST /C\r\nCOPY A:\\DOCS\\INFO.TXT A:\\HMACOPIA.TXT\r\nA:\\BIN\\HMATEST /C\r\nTYPE A:\\HMACOPIA.TXT\r\nDEL A:\\HMACOPIA.TXT\r\nIF NOT EXIST A:\\HMACOPIA.TXT ECHO lote-fin\r\n' > $T/HMA.BAT
mcopy -o -i $T/base.img $T/HMA.BAT ::BAT/HMA.BAT
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$1" $T/log.txt; then ok "$1"; else ko "$1"; fi; }
checkn() { if grep -qF -- "$1" $T/log.txt; then ko "(no debía salir) $1"; else ok "sin \"$1\""; fi; }
count() { if [ "$(grep -cF -- "$1" $T/log.txt)" = "$2" ]; then ok "$1 ($2 veces)"; else ko "$1 ($2 veces)"; fi; }
# run "CONFIG.SYS" TIEMPO orden...   (cada orden: «segundos|texto»)
run() {
  local cfg=$1 tmo=$2; shift 2
  cp $T/base.img $T/f.img
  printf "$cfg" > $T/CONFIG.SYS
  mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
  { sleep 6
    for c in "$@"; do printf '%s\r' "${c#*|}"; sleep "${c%%|*}"; done
  } | timeout $tmo qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
        -m 32 -net none -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a -no-reboot \
        > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 -c > $T/log.txt
  [ -n "${VERBOSE:-}" ] && cat $T/log.txt
}
X='DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\n'
E='DEVICE=A:\\HISPADOS\\HSPEMM.SYS\r\n'
D='DEVICEHIGH=A:\\HISPADOS\\HSPTEST.SYS\r\n'

echo "== DOS=HIGH,UMB + HSPEMM + DEVICEHIGH + LH =="
run "$X$E"'DOS=HIGH,UMB\r\n'"$D"'FILES=30\r\n' 110 \
  '2|MEM' '2|MEM /C' '2|A:\BIN\HMATEST' '3|A:\BIN\HMATEST /A' '2|A:\BIN\HMATEST /C' \
  '2|A:\BIN\HELLO cerrada' '6|A:\BAT\HMA.BAT' \
  '2|LH A:\BIN\UMBTEST' '2|A:\BIN\EMSTEST' '2|A:\BIN\VCPITEST' '2|A:\BIN\XMSTEST' \
  '2|A:\BIN\EMMMISC A' '2|A:\BIN\EMMMISC H' '2|A:\BIN\EMMMISC S' '2|A:\BIN\EMMMISC W' \
  '2|A:\HISPADOS\HSPDPMI' '11|A:\BIN\DPMI16' '10|A:\BIN\DPMI32' '2|A:\BIN\HMATEST' '2|A:\BIN\EMSTEST'
check "HSPEMM 0.1: EMS 4.0 y VCPI, "
check "HSPTEST.SYS: dispositivo de prueba instalado"
check "Hispa-DOS está cargado en la memoria alta (HMA)"
check "bytes de memoria superior (UMB) en total"
check "Programas en la memoria superior (UMB):"
count "DOSHMA=1" 2
check "HMATEST A20 OK: 12 llamadas con A20 cerrada"
check "Hello from a .COM program! Args:[ cerrada]"
check "lote-uno"
check "Hello from a .COM program! Args:[ lote]"
check "lote-fin"
check "(memoria superior)"
count "EMS test OK" 2
check "VCPI test OK"
check "XMS test OK"
check "XMS registers OK"
check "A20 virtual OK"
check "HLT OK"
check "Estrategia de memoria OK (con UMB)"
check "Windows: rechazado (CX distinto de 0)"
check "Con HSPEMM cargado: el modo protegido se pide por VCPI."
check "DPMI16 terminado, fallos: 00"
check "DPMI32 terminado, fallos: 00"
checkn "FALLO"
checkn "error interno"
checkn "HSPEMM: el programa"

echo "== DOS=HIGH,UMB en la primera línea, y DOS=UMB a secas =="
run 'DOS=HIGH,UMB\r\n'"$X$E$D"'FILES=30\r\n' 45 \
  '2|MEM' '2|MEM /C' '2|A:\BIN\HMATEST' '2|LH A:\BIN\UMBTEST' '2|A:\BIN\EMSTEST' '2|A:\BIN\EMMMISC S'
check "Hispa-DOS está cargado en la memoria alta (HMA)"
check "DOSHMA=1"
check "Programas en la memoria superior (UMB):"
check "(memoria superior)"
check "EMS test OK"
check "Estrategia de memoria OK (con UMB)"
checkn "FALLO"
run "$X$E"'dos=umb\r\n'"$D"'FILES=30\r\n' 40 \
  '2|MEM /C' '2|A:\BIN\HMATEST' '2|LH A:\BIN\UMBTEST' '2|A:\BIN\EMSTEST' '2|A:\BIN\EMMMISC S'
checkn "cargado en la memoria alta"
check "DOSHMA=0"
check "Programas en la memoria superior (UMB):"
check "(memoria superior)"
check "EMS test OK"
check "Estrategia de memoria OK (con UMB)"
checkn "FALLO"

echo "== Sin HSPEMM: DOS=HIGH,UMB y la estrategia de reserva =="
run "$X"'DOS=HIGH,UMB\r\nFILES=30\r\n' 30 '2|MEM /C' '2|A:\BIN\EMMMISC S' '2|A:\BIN\HMATEST'
check "Estrategia de memoria OK (sin UMB)"
check "No hay memoria superior (UMB) disponible."
check "DOSHMA=1"
checkn "FALLO"
run '' 25 '2|A:\BIN\EMMMISC S'
check "Estrategia de memoria OK (sin UMB)"

rm -rf $T
exit $fail
