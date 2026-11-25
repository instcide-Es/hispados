#!/bin/bash
# Prueba del Makefile (auditoría del 25/11/2026: con «make -j» salían
# programas de 0 octetos dentro de las imágenes). Sin «make -j» (no se usa en
# las pruebas): mira la forma del Makefile y lo que pasa si NASM falla.
#   1) ninguna regla vuelve a escribir build/gen/lib/util.inc ni red.inc (solo
#      las suyas, que van antes que nada como requisito de $(B));
#   2) .DELETE_ON_ERROR: si NASM falla, su salida a medias se borra (antes
#      quedaba un .COM de 0 octetos que la vez siguiente se daba por hecho);
#   3) tools/sin_vacios.py para la imagen si hay un programa vacío;
#   4) tools/cp437.py escribe con otro nombre y lo cambia al final.
# Makefile test (no parallel build needed).
set -u
cd "$(dirname "$0")/.."
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
T=$(mktemp -d)
trap 'rm -rf $T' EXIT
echo "== Makefile: sin programas vacíos =="

# 1) en seco: quién escribe util.inc y red.inc
make -n -B B=$T/b $T/b/HELLO.COM $T/b/PING.COM $T/b/FDISK.COM > $T/n.txt 2>&1
u=$(grep -c "cp437.py lib/util.inc" $T/n.txt)
r=$(grep -c "cp437.py lib/red.inc" $T/n.txt)
if [ "$u" = 1 ]; then ok "util.inc se pasa a CP437 una sola vez"; else ko "util.inc se escribe $u veces"; fi
if [ "$r" = 1 ]; then ok "red.inc se pasa a CP437 una sola vez"; else ko "red.inc se escribe $r veces"; fi
if grep -q "cp437.py lib/util.inc .*&&.*nasm" $T/n.txt; then ko "una regla de NASM aún reescribe util.inc"; else ok "ninguna regla de NASM reescribe util.inc"; fi
# y antes que ningún NASM
pu=$(grep -n "cp437.py lib/util.inc" $T/n.txt | head -1 | cut -d: -f1)
pn=$(grep -n "nasm " $T/n.txt | head -1 | cut -d: -f1)
if [ -n "$pu" ] && [ -n "$pn" ] && [ "$pu" -lt "$pn" ]; then ok "util.inc va antes que el primer NASM"; else ko "util.inc no va antes que NASM ($pu, $pn)"; fi
if grep -q "^\.DELETE_ON_ERROR:" Makefile; then ok ".DELETE_ON_ERROR en el Makefile"; else ko "falta .DELETE_ON_ERROR"; fi

# 2) NASM que falla a mitad tras crear su salida vacía: no queda el .COM
cat > $T/nasm_malo.sh <<'EOS'
#!/bin/bash
# crea la salida vacía y falla (como un NASM que muere con «Bus error»)
while [ $# -gt 0 ]; do [ "$1" = "-o" ] && : > "$2"; shift; done
exit 135
EOS
chmod +x $T/nasm_malo.sh
make B=$T/b NASM=$T/nasm_malo.sh $T/b/HELLO.COM > $T/m.txt 2>&1
if [ -e $T/b/HELLO.COM ]; then ko "quedó $T/b/HELLO.COM ($(stat -c%s $T/b/HELLO.COM) octetos) tras fallar NASM"; else ok "si NASM falla, no queda un HELLO.COM a medias"; fi
make B=$T/b $T/b/HELLO.COM > $T/m2.txt 2>&1
if [ -s $T/b/HELLO.COM ]; then ok "la vez siguiente se hace de verdad ($(stat -c%s $T/b/HELLO.COM) octetos)"; else ko "la vez siguiente no se rehace HELLO.COM"; fi

# 3) sin_vacios
: > $T/VACIO.COM
printf 'x' > $T/LLENO.COM
: > $T/DATOS.DAT
if python3 tools/sin_vacios.py $T/LLENO.COM $T/DATOS.DAT; then ok "sin_vacios deja pasar lo lleno (y los datos vacíos)"; else ko "sin_vacios para sin motivo"; fi
if python3 tools/sin_vacios.py $T/LLENO.COM $T/VACIO.COM 2>$T/sv.txt; then ko "sin_vacios deja pasar un .COM vacío"; else ok "sin_vacios para con un .COM de 0 octetos"; fi
if python3 tools/sin_vacios.py $T/NOESTA.SYS 2>/dev/null; then ko "sin_vacios deja pasar uno que no está"; else ok "sin_vacios para si falta un .SYS"; fi
if grep -q 'SIN_VACIOS) $(filter $(B)/%,$^)' Makefile && [ "$(grep -c 'SIN_VACIOS) $(filter' Makefile)" -ge 3 ]; then
  ok "las imágenes (disquete, USB y Hispa-486) pasan por sin_vacios"
else ko "alguna imagen no pasa por sin_vacios"; fi

# 4) cp437.py: el mismo contenido no se reescribe; uno nuevo, con cambio de nombre
printf 'hola á\n' > $T/a.asm
python3 tools/cp437.py $T/a.asm $T/g/a.asm
t1=$(stat -c%Y.%N $T/g/a.asm 2>/dev/null; stat -c%i $T/g/a.asm)
sleep 1
python3 tools/cp437.py $T/a.asm $T/g/a.asm
t2=$(stat -c%Y.%N $T/g/a.asm 2>/dev/null; stat -c%i $T/g/a.asm)
if [ "$t1" = "$t2" ]; then ok "cp437.py no toca lo que ya está igual"; else ko "cp437.py reescribe lo que ya está igual"; fi
printf 'adiós\n' > $T/a.asm
python3 tools/cp437.py $T/a.asm $T/g/a.asm
i3=$(stat -c%i $T/g/a.asm)
if [ "$i3" != "$(echo "$t2" | tail -1)" ] && [ "$(od -An -tx1 $T/g/a.asm | tr -d ' \n')" = "616469a2730a" ]; then
  ok "cp437.py escribe aparte y cambia el nombre (otro nodo, contenido bien)"
else ko "cp437.py no escribe aparte"; fi
ls $T/g | grep -q tmp && ko "quedó un .tmp de cp437.py" || ok "sin .tmp sueltos"

exit $fail
