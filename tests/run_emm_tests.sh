#!/bin/bash
# Prueba de HSPEMM.SYS (monitor V86: EMS 4.0, VCPI 1.0 y memoria superior):
#  1. HSPXMS + HSPEMM + DOS=UMB + DEVICEHIGH: clientes propios de EMS
#     (EMSTEST), de VCPI (VCPITEST: entra en modo protegido y vuelve) y de UMB
#     (UMBTEST, con LH), XMS por el monitor (XMSTEST), A20 virtual, HLT, CR0,
#     MEM /C con la memoria superior, corte de quien pasa a modo protegido por
#     su cuenta y aviso a Windows (INT 2Fh AX=1605h) cuando no puede apagarse.
#  2. NOEMS y tamaño: sin EMS (dispositivo EMMQXXX0) pero con VCPI; sin UMB en
#     uso, «Windows» apaga y enciende el modo V86.
#  3. FRAME=, X= y NOUMB; sin HSPXMS y cargado dos veces.
#  4. HSPDPMI como cliente VCPI (DPMI16 y DPMI32), HSPCD y HSPUSB (que también funciona).
# HSPEMM test: V86 monitor, EMS, VCPI, UMB.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=build
for f in EMSTEST.COM VCPITEST.COM UMBTEST.COM EMMMISC.COM DPMI16.COM DPMI32.COM; do
  [ -f $B/$f ] || { echo "falta $B/$f (make emmprogs nucprogs)"; exit 1; }
done
T=$(mktemp -d)
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$1" $T/log.txt; then ok "$1"; else ko "$1"; fi; }
checkn() { if grep -qF -- "$1" $T/log.txt; then ko "(no debía salir) $1"; else ok "sin \"$1\""; fi; }
count() { if [ "$(grep -cF -- "$1" $T/log.txt)" = "$2" ]; then ok "$1 ($2 veces)"; else ko "$1 ($2 veces)"; fi; }
# run "CONFIG.SYS" TIEMPO orden...   (cada orden: «segundos|texto»)
run() {
  local cfg=$1 tmo=$2; shift 2
  cp "$IMG" $T/f.img
  printf "$cfg" > $T/CONFIG.SYS
  mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
  for f in EMSTEST VCPITEST UMBTEST EMMMISC DPMI16 DPMI32 XMSTEST; do
    mcopy -o -i $T/f.img $B/$f.COM ::BIN/$f.COM
  done
  { sleep 6
    for c in "$@"; do printf '%s\r' "${c#*|}"; sleep "${c%%|*}"; done
  } | timeout $tmo qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
        -m 32 -net none -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a -no-reboot \
        ${QEMU_EXTRA:-} > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 -c > $T/log.txt
  [ -n "${VERBOSE:-}" ] && cat $T/log.txt
}
X='DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\n'
E='DEVICE=A:\\HISPADOS\\HSPEMM.SYS'

echo "== HSPEMM: EMS 4.0, VCPI, UMB y monitor V86 =="
run "$X$E"'\r\nDOS=UMB\r\nDEVICEHIGH=A:\\HISPADOS\\HSPTEST.SYS\r\nFILES=30\r\n' 80 \
  '2|A:\BIN\EMSTEST' '2|A:\BIN\VCPITEST' '2|LH A:\BIN\UMBTEST' '2|LOADHIGH A:\BIN\UMBTEST' \
  '2|A:\BIN\XMSTEST' \
  '2|A:\BIN\EMMMISC A' '2|A:\BIN\EMMMISC H' '2|A:\BIN\EMMMISC C' '2|A:\BIN\EMMMISC S' '2|A:\BIN\EMMMISC P' \
  '1|IF ERRORLEVEL 255 ECHO NIVEL-255' '2|A:\BIN\EMMMISC W' '2|MEM' '2|MEM /C' \
  '2|A:\BIN\EMSTEST' '2|A:\BIN\VCPITEST'
check "HSPEMM 0.1: EMS 4.0 y VCPI, "
# (el marco va donde deje el mayor bloque de memoria superior seguido: D000
# con la SeaBIOS de QEMU 8, D800 con la de QEMU 10; tiene que ser el mismo
# que ve EMSTEST)
fr=$(grep -o "marco de página en [CDE][0-9A-F]00" $T/log.txt | head -1 | sed 's/.* //')
if [ -n "$fr" ]; then ok "marco de página en $fr"; else ko "marco de página en C000-EC00"; fi
check "memoria superior: "
check "HSPTEST.SYS: dispositivo de prueba instalado"
check "EMS detectado OK"
check "EMS marco en ${fr:-D000}"
for s in mapear errores contexto "mapa multiple" nombres reasignar mover; do check "EMS $s OK"; done
count "EMS test OK" 2
check "VCPI detectado OK"
check "VCPI paginas OK"
check "VCPI modo protegido OK"
count "VCPI test OK" 2
count "(memoria superior)" 2
checkn "UMB FALLO"
check "XMS test OK"
check "XMS overlap OK"
check "A20 virtual OK"
check "HLT OK"
check "CR0 y DR OK"
check "Estrategia de memoria OK (con UMB)"
check "HSPEMM: el programa quiere pasar a modo protegido por su cuenta"
check "Programa terminado."
check "NIVEL-255"
checkn "LGDT no se ha cortado"
check "HSPEMM: hay memoria superior (UMB), EMS o VCPI en uso y Windows"
check "Windows: rechazado (CX distinto de 0)"
check "bytes de memoria superior (UMB) en total"
check "Programas en la memoria superior (UMB):"
checkn "FALLO"
checkn "error interno"

echo "== HSPEMM NOEMS 2048: VCPI sin EMS; Windows apaga el modo V86 =="
run "$X$E"' NOEMS 2048\r\nFILES=30\r\n' 45 \
  '2|A:\BIN\EMSTEST' '2|A:\BIN\VCPITEST' '2|A:\BIN\UMBTEST' '3|A:\BIN\EMMMISC W' \
  '2|A:\BIN\VCPITEST' '2|A:\BIN\XMSTEST'
check "HSPEMM 0.1: VCPI, 2048 KB"
check "EMS: no hay gestor (EMMXXXX0)"
count "VCPI test OK" 2
check "UMB test OK"
check "Windows: en modo real"
check "Windows: modo V86 apagado y encendido OK"
check "XMS test OK"
checkn "FALLO"

echo "== HSPEMM FRAME=D000 X=C000-CFFF 1024, y NOUMB =="
# (B000-B7FF y E000-EFFF también se miran por omisión: fuera)
run "$X$E"' frame=d000 x=c000-cfff x=b000-b7ff x=e000-efff 1024\r\nFILES=30\r\n' 30 '2|A:\BIN\EMSTEST' '2|A:\BIN\UMBTEST'
check "HSPEMM 0.1: EMS 4.0 y VCPI, 1024 KB; marco de página en D000"
checkn "memoria superior: "
check "EMS test OK"
check "UMB: no hay memoria superior libre"
run "$X$E"' NOUMB\r\nDOS=UMB\r\nFILES=30\r\n' 30 '2|LH A:\BIN\UMBTEST' '2|MEM /C'
checkn "memoria superior: "
checkn "(memoria superior)"
check "UMB: no hay memoria superior libre"
check "No hay memoria superior (UMB) disponible."

echo "== HSPEMM sin HSPXMS, y cargado dos veces =="
run "$E"'\r\n' 20 '2|A:\BIN\EMSTEST'
check "HSPEMM: falta el gestor XMS (cargue antes HSPXMS.SYS); no se instala."
check "EMS: no hay gestor (EMMXXXX0)"
run "$X$E"'\r\n'"$E"'\r\n' 20 '2|A:\BIN\EMSTEST'
check "HSPEMM: el procesador ya está en modo protegido; no se instala."
check "EMS test OK"

echo "== HSPEMM con HSPDPMI (cliente VCPI), HSPCD y HSPUSB =="
QEMU_EXTRA="-device qemu-xhci" run "$X$E"'\r\nDEVICE=A:\\HISPADOS\\HSPUSB.SYS\r\nFILES=30\r\n' 75 \
  '2|A:\HISPADOS\HSPDPMI' '11|A:\BIN\DPMI16' '10|A:\BIN\DPMI32' '2|A:\BIN\EMSTEST' \
  '2|A:\HISPADOS\HSPDPMI /D' '2|A:\BIN\VCPITEST'
check "Con HSPEMM cargado: el modo protegido se pide por VCPI."
check "DPMI16 terminado, fallos: 00"
check "DPMI32 terminado, fallos: 00"
check "EMS test OK"
check "HSPDPMI: descargado; la memoria queda libre."
check "VCPI test OK"
# HSPUSB funciona también en modo V86 (tests/run_modojuegos_tests.sh)
check "Controlador xHCI en "
checkn "No funciona"
checkn "error interno"

rm -rf $T
exit $fail
