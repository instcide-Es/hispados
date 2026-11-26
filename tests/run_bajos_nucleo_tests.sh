#!/bin/bash
# Hallazgos bajos de la auditoría del 25-11 (núcleo, COMMAND, MOVER, HSPEMM):
#  1) IF ERRORLEVEL 256 (o más) no se cumple nunca (antes valía módulo 256);
#  2) SET con el entorno lleno: si el valor nuevo no cabe, se queda el viejo
#     (antes se borraba: PATH desaparecía);
#  3) el bit «no heredar» de 3Dh (80h): el hijo no recibe ese handle;
#  4) un .EXE con la tabla de reubicaciones cortada no se ejecuta;
#  5) MOVER /Y a una unidad llena: el destino viejo sigue (antes se borraba
#     antes de copiar);
#  6) un CONFIG.SYS con un SUBMENU que se apunta a sí mismo (y cuenta atrás 0)
#     arranca igual (antes, para siempre en el menú);
#  7) XMS 10h (memoria superior de HSPEMM) con DX = 0: B0h, sin bloque vacío.
# Low-severity audit findings.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
fail=0
Q="qemu-system-i386 -display none -serial stdio -monitor none -parallel none"
conv() { tr -d '\r' < "$1" | iconv -f CP437 -t UTF-8 -c > "$1.txt"; }
check() { if grep -qF -- "$2" "$1.txt"; then echo "  OK   $2"; else echo "  FAIL $2"; fail=1; fi; }
checkn() { if grep -qF -- "$2" "$1.txt"; then echo "  FAIL (no debía salir) $2"; fail=1; else echo "  OK   sin «$2»"; fi; }
show() { if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then for f in "$@"; do echo "--- $f ---"; cat "$f.txt"; done; fi; }
nasm -f bin -o $T/INHER.COM tests/progs/inher.asm || exit 1
nasm -f bin -o $T/XMS10.COM tests/progs/xms10.asm || exit 1
# TRUNC.EXE: dice 100 reubicaciones y el archivo acaba antes
python3 - $T/TRUNC.EXE <<'PY'
import struct, sys
code = bytes([0x0E, 0x1F, 0xBA, 0x0C, 0x00, 0xB4, 0x09, 0xCD, 0x21, 0xB8, 0x00, 0x4C, 0xCD, 0x21]) + b'TRUNC-EJECUTADO\r\n$'
code = code[:2] + bytes([0xBA, 14, 0]) + code[5:]
size = 64 + len(code)
hdr = struct.pack('<2sHHHHHHHHHHHHH', b'MZ', size % 512, (size + 511) // 512, 100, 4, 0x10, 0xFFFF,
                  0x10, 0x100, 0, 0, 0, 0x30, 0)
hdr += bytes(64 - len(hdr))
open(sys.argv[1], 'wb').write(hdr + code)
PY
cp "$IMG" $T/f.img
mcopy -o -i $T/f.img $T/INHER.COM $T/TRUNC.EXE ::
head -c 120000 /dev/urandom > $T/BIG.BIN
mcopy -o -i $T/f.img $T/BIG.BIN ::BIG.BIN
# B: casi lleno, con DST.TXT
mformat -C -i $T/b.img -f 1440 ::
printf 'viejo' > $T/DST.TXT
mcopy -i $T/b.img $T/DST.TXT ::DST.TXT
# (relleno de tamaño fijo, con tope: el disquete de 1,44 MB tiene unos
# 1.457.000 libres; quedan unos 50 KB, menos que BIG.BIN)
RELL=1400000
if [ $RELL -le 0 ] || [ $RELL -gt 2000000 ]; then echo "  FAIL relleno fuera de límites"; exit 1; fi
head -c $RELL /dev/zero > $T/RELLENO.BIN
mcopy -i $T/b.img $T/RELLENO.BIN ::RELLENO.BIN || { echo "  FAIL no cabe el relleno"; exit 1; }
rm -f $T/RELLENO.BIN
v100=$(printf 'v%.0s' $(seq 100)); p115=$(printf 'p%.0s' $(seq 115))
{ sleep 5
  for c in 'IF ERRORLEVEL 256 ECHO MAL-256' 'IF ERRORLEVEL 0 ECHO BIEN-0' 'IF NOT ERRORLEVEL 300 ECHO BIEN-300' \
           'SET PATH=A:\XX'; do printf '%s\r' "$c"; sleep 1; done
  for i in 1 2 3 4 5 6 7 8 9 10 11; do printf 'SET V%s=%s\r' $i "$v100"; sleep 0.6; done
  printf 'SET PATH=%s\r' "$p115"; sleep 1
  printf 'SET\r'; sleep 1
  for c in 'INHER' 'TRUNC' 'ECHO sigue-trunc' 'A:\HISPADOS\MOVER /Y A:\BIG.BIN B:\DST.TXT' 'ECHO fin-bajos'; do printf '%s\r' "$c"; sleep 3; done
  sleep 1
} | timeout 70 $Q -drive file=$T/f.img,format=raw,if=floppy,index=0 -drive file=$T/b.img,format=raw,if=floppy,index=1 \
      -boot a -no-reboot > $T/l1 2>&1
conv $T/l1
echo "== Hispa-DOS bajos: IF ERRORLEVEL, SET, no heredar, .EXE cortado, MOVER =="
if grep -qx "MAL-256" $T/l1.txt; then echo "  FAIL IF ERRORLEVEL 256 se ha cumplido"; fail=1; else echo "  OK   IF ERRORLEVEL 256 no se cumple"; fi
check $T/l1 "BIEN-0"
check $T/l1 "BIEN-300"
check $T/l1 "No queda espacio en el entorno"
if grep -qxF "PATH=A:\\XX" $T/l1.txt; then echo "  OK   PATH sigue (A:\\XX)"; else echo "  FAIL PATH ha cambiado o desaparecido"; fail=1; fi
check $T/l1 "INHER: el de 80h no se hereda"
check $T/l1 "INHER: el de 00h se hereda"
check $T/l1 "INHER: padre fin"
checkn $T/l1 "TRUNC-EJECUTADO"
check $T/l1 "sigue-trunc"
check $T/l1 "fin-bajos"
v=$(mtype -i $T/b.img ::DST.TXT 2>/dev/null)
if [ "$v" = viejo ]; then echo "  OK   B:\\DST.TXT sigue (MOVER no ha cabido)"; else echo "  FAIL B:\\DST.TXT = «${v:0:20}»"; fail=1; fi
if mdir -i $T/f.img ::BIG.BIN >/dev/null 2>&1; then echo "  OK   A:\\BIG.BIN sigue"; else echo "  FAIL A:\\BIG.BIN ha desaparecido"; fail=1; fi
if mdir -i $T/b.img ::MOVER\$\$\$.TMP >/dev/null 2>&1; then echo "  FAIL queda MOVER\$\$\$.TMP"; fail=1; else echo "  OK   sin temporal en B:"; fi
show $T/l1

# 6) y 7): SUBMENU que se apunta a sí mismo; XMS 10h con HSPEMM
cp "$IMG" $T/g.img
printf '[MENU]\r\nSUBMENU=MENU,Otra vez este\r\nMENUDEFAULT=MENU,0\r\n[COMMON]\r\nDEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDEVICE=A:\\HISPADOS\\HSPEMM.SYS\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/g.img $T/CONFIG.SYS $T/XMS10.COM ::
{ sleep 10; printf 'XMS10\r'; sleep 2; printf 'ECHO fin-menu\r'; sleep 2; } |
  timeout 30 $Q -m 32 -drive file=$T/g.img,format=raw,if=floppy -boot a -no-reboot > $T/l2 2>&1
conv $T/l2
echo "== Hispa-DOS bajos: SUBMENU en bucle y XMS 10h con DX = 0 =="
check $T/l2 "HSPEMM 0.1: EMS 4.0 y VCPI, "
check $T/l2 "XMS10: 0 parrafos -> B0h"
check $T/l2 "XMS10: 1 parrafo bien"
check $T/l2 "fin-menu"
show $T/l2
exit $fail
