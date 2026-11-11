#!/bin/bash
# Prueba de MEM y sus opciones (/C, /F, /M, /D, /P) con HSPXMS y HSPRATON
# cargados, y de los alias (KEYB = TECLADO) desde cualquier carpeta.
# MEM options and command aliases test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
cp "$IMG" $T/f.img
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
{ sleep 6
  for c in "A:\\HISPADOS\\HSPRATON" "MEM" "MEM /C" "MEM /F" "MEM /M COMMAND" "MEM /MODULE:HSPRATON" \
           "MEM /M NOEXISTE" "MEM /D" "MEM /X" "CD DOCS" "KEYB" "TECLADO" "PLAYWAV" "NOEXISTE" "CD \\"; do
    printf '%s\r' "$c"; sleep 1.5
  done
} | timeout 40 qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
  -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot > $T/log 2>&1
tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 > $T/log.txt
fail=0
check() { if grep -qF -- "$1" $T/log.txt; then echo "  OK   $1"; else echo "  FAIL $1"; fail=1; fi; }
echo "== Hispa-DOS MEM y alias =="
check "bytes de memoria convencional en total"
check "Programas en la memoria convencional:"
check "  COMMAND  "
check "  HSPRATON "
check "  Total          654.336 (639K)"
check "Bloques libres de la memoria convencional:"
check "  Mayor bloque"
check "El programa COMMAND usa esta memoria:"
check "Entorno"
check "El programa HSPRATON usa esta memoria:"
check "El programa NOEXISTE no está en la memoria."
check "Memoria convencional, bloque a bloque:"
check "Núcleo de Hispa-DOS"
check "XMSXXXX0  (carácter)"
check "Parámetro no válido. Uso: MEM"
# alias: KEYB y TECLADO son el mismo programa, también desde A:\DOCS
n=$(grep -c "^Teclado activo:" $T/log.txt)
if [ "$n" -ge 2 ]; then echo "  OK   KEYB = TECLADO (desde A:\\DOCS)"; else echo "  FAIL alias KEYB/TECLADO: $n"; fail=1; fi
check "PLAYWAV 0.1 - Hispa-DOS"
check "Comando o nombre de archivo incorrecto"
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi
rm -rf $T
exit $fail
