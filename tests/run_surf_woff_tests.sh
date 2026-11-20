#!/bin/bash
# Prueba de las letras de la web del Navegador completo (w32/surf/woff.c):
# WOFF 1.0 y WOFF2 a TTF, en el anfitrión, sin Internet ni QEMU.
#  a) un recorte de DejaVu Sans (tests/woff, hecho con fontTools: genera.py)
#     en TTF, WOFF, WOFF2, WOFF2 con hmtx transformada y WOFF2 sin transformar:
#     cada glifo sale con los mismos puntos (stbtt_GetGlyphShape), avance, lsb,
#     caja y cmap que el TTF original, y no queda memoria suelta.
#  b) cortados (todos los tamaños al principio y al final, y uno de cada 7):
#     NULL; 1500 con bytes estropeados y 2500
#     con glyf/loca/hmtx transformadas estropeadas (metidas en un Brotli sin
#     comprimir): ni se cuelga ni lee fuera (ASan + UBSan) ni pierde memoria.
#  c) sin BROTLI.DIC (o con uno que no es): el WOFF 1.0 va igual y el WOFF2
#     que usa el diccionario da NULL, sin colgarse.
#  d) woff.c y Brotli compilan con las banderas de NAVEGA2.EXE (SURF_CF).
#  uso: run_surf_woff_tests.sh
# WOFF/WOFF2 font decoder tests (host only).
set -u
cd "$(dirname "$0")/.."
T=$(mktemp -d)
trap 'rm -rf $T' EXIT
W=tests/woff
BR=terceros/brotli/c
fail=0
mal() { echo "FALLO: $*"; fail=1; }

# el decodificador y Brotli, con ASan/UBSan y la memoria contada (cuenta.h)
SAN="-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined"
for f in w32/surf/woff.c $BR/common/*.c $BR/dec/*.c; do
  gcc $SAN -w -I$BR/include -include $W/cuenta.h -DBROTLI_EXTERNAL_DICTIONARY_DATA -c $f -o $T/$(basename $f .c).o || mal "no compila $f"
done
gcc $SAN -w -Iw32/surf -Iterceros/stb -I$BR/include -o $T/prueba $W/prueba.c $T/*.o -lm || mal "no compila la prueba"
[ $fail = 0 ] || { echo "RESULTADO: FALLO"; exit 1; }
export ASAN_OPTIONS=detect_leaks=1:abort_on_error=0 WOFF_CARPETA=w32/surf/recursos WOFF_DIC=w32/surf/recursos/BROTLI.DIC

echo "== a) mismos glifos que el TTF"
for f in letra.ttf letra.woff letra.woff2 letra_hmtx.woff2 letra_nula.woff2; do
  $T/prueba compara $W/letra.ttf $W/$f || mal "compara $f"
done
echo "== b) rotos y cortados"
for f in letra.woff letra.woff2 letra_hmtx.woff2 letra_nula.woff2; do
  $T/prueba rotos $W/$f || mal "rotos $f"
done
for f in letra.woff2 letra_hmtx.woff2; do
  $T/prueba glifos $W/$f || mal "glifos $f"
done
echo "== c) sin BROTLI.DIC"
WOFF_CARPETA=$T/no-esta $T/prueba compara $W/letra.ttf $W/letra.woff || mal "WOFF 1.0 sin diccionario"
# (letra.woff2 usa el diccionario: lleva una descripción en inglés, genera.py)
WOFF_CARPETA=$T/no-esta $T/prueba info $W/letra.woff2; r=$?
[ $r = 1 ] || mal "WOFF2 sin diccionario: salió con $r (tenía que dar NULL)"
echo 'no soy el diccionario' > $T/BROTLI.DIC
WOFF_CARPETA=$T $T/prueba info $W/letra.woff2; r=$?
[ $r = 1 ] || mal "WOFF2 con un BROTLI.DIC que no es: salió con $r (tenía que dar NULL)"
echo "== d) con las banderas de NAVEGA2.EXE"
CF="-m32 -march=i486 -Os -std=gnu99 -ffreestanding -fbuiltin -nostdinc -isystem $(gcc -m32 -print-file-name=include) -Iw32/surf/inc \
  -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables -fno-common -ffunction-sections -fdata-sections -DNDEBUG"
for f in w32/surf/woff.c $BR/common/*.c $BR/dec/*.c; do
  gcc $CF -w -I$BR/include -DBROTLI_EXTERNAL_DICTIONARY_DATA -c $f -o $T/x_$(basename $f .c).o || mal "freestanding: $f"
done
gcc $CF -Wall -Wextra -Werror -I$BR/include -c w32/surf/woff.c -o $T/x_woff.o || mal "woff.c con avisos"
ld -m elf_i386 -r --gc-sections -e woff_a_sfnt -u woff_carpeta -o $T/x.o $T/x_*.o || mal "no enlaza"
echo "   código: $(size $T/x.o | awk 'NR==2{print $1+$2}') bytes (woff.c + Brotli)"

if [ $fail = 0 ]; then echo "RESULTADO: BIEN"; else echo "RESULTADO: FALLO"; fi
exit $fail
