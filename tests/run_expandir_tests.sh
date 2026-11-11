#!/bin/bash
# Prueba de EXPANDIR (EXPAND): SZDD (con mscompress, si está instalado),
# KWAJ métodos 0 y 1 (generados aquí) y, si se indica MSDOS_DISKS con los
# disquetes de un MS-DOS 6.x (no forman parte del proyecto), KWAJ método 3
# comparado con el EXPAND.EXE original.
# EXPAND test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
cp "$IMG" $T/f.img
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
head -c 30000 /dev/urandom | base64 > $T/DATOS.TXT
# KWAJ 0 (sin comprimir) y 1 (XOR FFh), con nombre y extensión guardados
python3 - $T <<'EOF'
import struct, sys
d = sys.argv[1]
data = open(d + '/DATOS.TXT', 'rb').read()
for m, out in ((0, 'K0.TX_'), (1, 'K1.TX_')):
    body = data if m == 0 else bytes(b ^ 0xFF for b in data)
    extra = struct.pack('<I', len(data)) + b'KDATOS\0' + b'TXT\0'
    hdr = b'KWAJ\x88\xf0\x27\xd1' + struct.pack('<HHH', m, 14 + len(extra), 0x19) + extra
    open(d + '/' + out, 'wb').write(hdr + body)
EOF
mcopy -i $T/f.img $T/K0.TX_ ::K0.TX_
mcopy -i $T/f.img $T/K1.TX_ ::K1.TX_
CMDS=("MD SALIDA" "EXPANDIR K0.TX_ SALIDA\\K0.TXT" "EXPAND K1.TX_ SALIDA\\K1.TXT" "EXPANDIR K1.TX_" "EXPANDIR NOEXISTE.EX_")
HAVE_SZ=0
if command -v mscompress >/dev/null; then
  mkdir $T/sz && cp $T/DATOS.TXT $T/sz/ && (cd $T/sz && mscompress DATOS.TXT >/dev/null 2>&1)
  if [ -f $T/sz/DATOS.TXT_ ]; then
    mcopy -i $T/f.img $T/sz/DATOS.TXT_ ::SZ.TX_; HAVE_SZ=1
    CMDS+=("EXPANDIR SZ.TX_ SALIDA\\SZ.TXT")
  fi
fi
HAVE_MS=0
if [ -n "${MSDOS_DISKS:-}" ] && [ -f "$MSDOS_DISKS/disco1.img" ]; then
  mtype -i $MSDOS_DISKS/disco1.img ::EXPAND.EXE > $T/EXPMS.EXE
  mtype -i $MSDOS_DISKS/disco1.img ::PAIS.TX_ > $T/PAIS.TX_
  mtype -i $MSDOS_DISKS/disco2.img ::HIMEM.SY_ > $T/HIMEM.SY_
  for f in EXPMS.EXE PAIS.TX_ HIMEM.SY_; do mcopy -i $T/f.img $T/$f ::$f; done
  CMDS+=("MD REF" "EXPMS PAIS.TX_ REF\\PAIS.TXT" "EXPMS HIMEM.SY_ REF\\HIMEM.SYS"
         "EXPANDIR PAIS.TX_ SALIDA\\PAIS.TXT" "EXPANDIR HIMEM.SY_ SALIDA\\HIMEM.SYS")
  HAVE_MS=1
fi
{ sleep 6; for c in "${CMDS[@]}"; do printf '%s\r' "$c"; sleep 4; done; } | \
  timeout 120 qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
  -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot > $T/log 2>&1
tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 > $T/log.txt
echo "== Hispa-DOS EXPANDIR =="
same() { if mtype -i $T/f.img ::$1 2>/dev/null | cmp -s - $2; then ok "$3"; else ko "$3"; fi; }
same SALIDA/K0.TXT $T/DATOS.TXT "KWAJ método 0"
same SALIDA/K1.TXT $T/DATOS.TXT "KWAJ método 1 (EXPAND)"
same KDATOS.TXT $T/DATOS.TXT "nombre original guardado en KWAJ"
if grep -qF "No se encontró el archivo." $T/log.txt; then ok "archivo que no existe"; else ko "archivo que no existe"; fi
if [ $HAVE_SZ = 1 ]; then same SALIDA/SZ.TXT $T/DATOS.TXT "SZDD (mscompress)"; else echo "  --   SZDD: mscompress no está instalado"; fi
if [ $HAVE_MS = 1 ]; then
  for n in PAIS.TXT HIMEM.SYS; do
    mtype -i $T/f.img ::REF/$n > $T/r.bin
    same SALIDA/$n $T/r.bin "KWAJ método 3: $n igual que EXPAND de MS-DOS"
  done
else
  echo "  --   KWAJ método 3: sin disquetes de MS-DOS (MSDOS_DISKS)"
fi
if fsck.fat -n $T/f.img >/dev/null 2>&1; then ok "fsck.fat"; else ko "fsck.fat"; fi
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi
rm -rf $T
exit $fail
