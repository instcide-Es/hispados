#!/bin/bash
# Prueba de DELTREE / BORRARARBOL: carpetas con subcarpetas y archivos de solo
# lectura, ocultos y de sistema; /Y, comodines, responder N y errores.
# DELTREE test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
cp "$IMG" $T/f.img
I=$T/f.img
echo uno > $T/a.txt
for d in ARBOL ARBOL/SUB1 ARBOL/SUB1/SUB2 ARBOL/SUB3 OTRO TMP1 TMP2 GUARDA; do mmd -i $I ::$d; done
for f in ARBOL/A.TXT ARBOL/SUB1/B.TXT ARBOL/SUB1/SUB2/C.TXT ARBOL/SUB3/D.TXT OTRO/E.TXT TMP1/F.TXT GUARDA/G.TXT; do mcopy -i $I $T/a.txt ::$f; done
mattrib -i $I +r ::ARBOL/SUB1/B.TXT
mattrib -i $I +h +s ::ARBOL/SUB1/SUB2/C.TXT
{ sleep 6
  printf 'DELTREE ARBOL\r'; sleep 2; printf 'S\r'; sleep 3
  printf 'BORRARARBOL /Y OTRO\r'; sleep 3
  printf 'DELTREE /Y TMP?\r'; sleep 3
  printf 'DELTREE GUARDA\r'; sleep 2; printf 'N\r'; sleep 2
  printf 'DELTREE NOEXISTE\r'; sleep 2
} | timeout 35 qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
  -drive file=$I,format=raw,if=floppy -boot a -no-reboot > $T/log 2>&1
tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 > $T/log.txt
fail=0
check() { if grep -qF -- "$1" $T/log.txt; then echo "  OK   $1"; else echo "  FAIL $1"; fail=1; fi; }
gone() { if mdir -i $I ::$1 >/dev/null 2>&1; then echo "  FAIL $1 sigue ahí"; fail=1; else echo "  OK   $1 borrado"; fi; }
echo "== Hispa-DOS DELTREE / BORRARARBOL =="
check "¿Eliminar ARBOL y todo su contenido? (S/N)"
check "Eliminando ARBOL..."
check "Eliminando OTRO..."
check "Eliminando TMP1..."
check "Eliminando TMP2..."
check "No se encontró el archivo"
gone ARBOL; gone OTRO; gone TMP1; gone TMP2
if mtype -i $I ::GUARDA/G.TXT >/dev/null 2>&1; then echo "  OK   GUARDA se conserva (N)"; else echo "  FAIL GUARDA"; fail=1; fi
if fsck.fat -n $I >/dev/null 2>&1; then echo "  OK   fsck.fat"; else echo "  FAIL fsck.fat"; fail=1; fi
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi
rm -rf $T
exit $fail
