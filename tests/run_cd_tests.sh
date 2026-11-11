#!/bin/bash
# Prueba del lector de CD/DVD: HSPCD.SYS (ATAPI) + ISO 9660 en el núcleo,
# con un DVD virtual en QEMU; también SYSINFO (debe ver el lector).
# CD/DVD test: ATAPI driver and ISO 9660 support.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
cp "$IMG" $T/f.img
printf 'DEVICE=A:\\DRIVERS\\HSPCD.SYS\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
mkdir -p $T/iso/DOCS
printf 'Hola desde el DVD de HispaDOS\r\n' > $T/iso/LEEME.TXT
printf 'Fichero en una carpeta del DVD\r\n' > $T/iso/DOCS/DENTRO.TXT
head -c 70000 /dev/urandom > $T/iso/GRANDE.BIN
genisoimage -quiet -V DISCO_HISPA -o $T/disco.iso $T/iso 2>/dev/null
send() { sleep "${2:-1.5}"; printf '%s\r' "$1"; }
{
  sleep 5
  send "LISTAR C:\\" 2
  send "MOSTRAR C:\\LEEME.TXT"
  send "CD C:\\DOCS"
  send "MOSTRAR C:DENTRO.TXT"
  send "COPIAR C:\\GRANDE.BIN A:\\" 3
  send "BORRAR C:\\LEEME.TXT"
  send "CREARDIR C:\\NUEVA"
  send "SYSINFO" 3
} | timeout 40 qemu-system-i386 -display none -serial stdio -monitor none \
      -drive file=$T/f.img,format=raw,if=floppy \
      -drive file=$T/disco.iso,media=cdrom,if=ide,index=2 -boot a -no-reboot > $T/log 2>&1
tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 > $T/log.txt
fail=0
check() {
  if grep -qF -- "$1" $T/log.txt; then echo "  OK   $1"; else echo "  FAIL $1"; fail=1; fi
}
echo "== HispaDOS CD/DVD tests =="
check "HSPCD 0.1 - lectores de CD/DVD de HispaDOS"
check "Lector: QEMU DVD-ROM"
check "Primera unidad de CD/DVD: C:"
check " El volumen de la unidad C es DISCO_HISPA"
check "LEEME    TXT"
check "DOCS            <DIR>"
check "GRANDE   BIN        70.000"
check "Hola desde el DVD de HispaDOS"
check "Fichero en una carpeta del DVD"
check "1 archivo(s) copiado(s)"
check "Acceso denegado"
check "No se puede crear el directorio"
check "IDE"
check "Propiedades del sistema"
check "QEMU DVD-ROM"
check "Procesador   QEMU Virtual CPU"
if cmp -s <(mtype -i $T/f.img ::GRANDE.BIN) $T/iso/GRANDE.BIN; then echo "  OK   GRANDE.BIN copiado del DVD sin errores"; else echo "  FAIL contenido de GRANDE.BIN"; fail=1; fi
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi
rm -rf $T
exit $fail
