#!/bin/bash
# Prueba del motor de JavaScript de Hispa-486 (HSPJS.DLL, que es QuickJS, y
# JS.EXE, su consola; docs/486/NAVEGADOR.md, «JavaScript»), en QEMU:
#  - tests/js/motor.js: lo que usa el JavaScript de ClicGest (ES2020: flechas,
#    clases, plantillas, async, Map/Set, expresiones regulares, JSON) y las
#    cuentas de Math (x87);
#  - las pruebas de QuickJS (terceros/quickjs/tests): el lenguaje, las
#    clausuras, los bucles, lo de serie (test_builtin) y BigInt;
#  - una excepción sin recoger (sale con nivel 1), un bucle sin fin cortado
#    con /T y la memoria y el tiempo de /M.
# Cada orden espera a que salga en la consola su marca de fin (no tiempos
# fijos). GUARDAR=carpeta deja allí lo que ha salido.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=build
make -s $B/HW32.COM $B/MSVCRT.DLL $B/HSPJS.DLL $B/JS.EXE > /dev/null 2>&1 || {
  echo "== Hispa-486: JavaScript (HSPJS.DLL): sin gcc de 32 bits, se salta =="; exit 0; }
T=$(mktemp -d)
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -q -- "$2" $T/$1.txt; then ok "$2"; else ko "$2"; fi; }
checkn() { if grep -qF -- "$2" $T/$1.txt; then ko "(no debía salir) $2"; else ok "sin «$2»"; fi; }
trap '[ -n "${GUARDAR:-}" ] && cp $T/*.txt "$GUARDAR"/ 2>/dev/null; rm -rf $T' EXIT
echo "== Hispa-486: JavaScript (HSPJS.DLL y JS.EXE) =="
export MTOOLS_SKIP_CHECK=1
cp "$IMG" $T/a.img
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/a.img $T/CONFIG.SYS ::CONFIG.SYS
dd if=/dev/zero of=$T/b.img bs=1474560 count=1 2> /dev/null
mformat -i $T/b.img -f 1440 ::
for f in HW32.COM MSVCRT.DLL HSPJS.DLL JS.EXE; do mcopy -i $T/b.img $B/$f ::; done
Q=terceros/quickjs/tests
mcopy -i $T/b.img tests/js/motor.js ::MOTOR.JS
mcopy -i $T/b.img $Q/test_language.js ::TLANG.JS
mcopy -i $T/b.img $Q/test_closure.js ::TCLOS.JS
mcopy -i $T/b.img $Q/test_loop.js ::TLOOP.JS
mcopy -i $T/b.img $Q/test_builtin.js ::TBUILT.JS
mcopy -i $T/b.img $Q/test_bigint.js ::TBIGINT.JS
printf 'let x = 1;\r\nfunction f() { throw new TypeError("a proposito"); }\r\nf();\r\n' > $T/EXC.JS
printf 'for (;;) {}\r\n' > $T/BUCLE.JS
mcopy -i $T/b.img $T/EXC.JS $T/BUCLE.JS ::
# corre NOMBRE TOPE «orden»...: cada orden, y luego ECHO ---FIN<n>; espera a
# que salga (o al tope de segundos de esa orden)
corre() {
  local n=$1 tope=$2 k=0 c; shift 2
  mkfifo $T/ent
  qemu-system-i386 -display none -serial stdio -monitor none -parallel none -m 16 \
    -drive file=$T/a.img,format=raw,if=floppy,index=0 -drive file=$T/b.img,format=raw,if=floppy,index=1 \
    -boot a -no-reboot < $T/ent > $T/$n.raw 2>&1 &
  local q=$!
  exec 7> $T/ent
  sleep 6
  for c in "$@"; do
    k=$((k + 1))
    printf '%s\r' "$c" >&7
    printf 'ECHO ---FIN%d\r' $k >&7
    local s=0
    while [ $s -lt $((tope * 2)) ] && ! tr -d '\r' < $T/$n.raw | grep -q "^---FIN$k"; do sleep 0.5; s=$((s + 1)); done
  done
  exec 7>&-
  kill $q 2> /dev/null; wait $q 2> /dev/null
  rm -f $T/ent
  tr -d '\r' < $T/$n.raw | iconv -f CP437 -t UTF-8 -c > $T/$n.txt
}
H='B:\HW32 B:\JS.EXE'
corre uno 240 'A:\HISPADOS\HSPDPMI' 'B:' \
  "$H" \
  "$H /M MOTOR.JS" \
  'ECHO ---LANG' "$H TLANG.JS" 'IF ERRORLEVEL 1 ECHO NIVEL-1' \
  'ECHO ---CLOS' "$H TCLOS.JS" 'IF ERRORLEVEL 1 ECHO NIVEL-1' \
  'ECHO ---LOOP' "$H TLOOP.JS" 'IF ERRORLEVEL 1 ECHO NIVEL-1' \
  'ECHO ---BUILT' "$H /M TBUILT.JS" 'IF ERRORLEVEL 1 ECHO NIVEL-1' \
  'ECHO ---BIGINT' "$H TBIGINT.JS" 'IF ERRORLEVEL 1 ECHO NIVEL-1' \
  'ECHO ---EXC' "$H EXC.JS" 'IF ERRORLEVEL 1 ECHO NIVEL-1' \
  'ECHO ---BUCLE' "$H /T:3 BUCLE.JS" 'IF ERRORLEVEL 1 ECHO NIVEL-1' \
  'ECHO ---ORDEN' "$H /E \"print([1,2,3].map(function (x) { return x * 2; }).join('-'))\"" \
  'ECHO SIGUE-VIVO'
sed -n '/^---LANG/,/^---CLOS/p' $T/uno.txt > $T/lang.txt
sed -n '/^---CLOS/,/^---LOOP/p' $T/uno.txt > $T/clos.txt
sed -n '/^---LOOP/,/^---BUILT/p' $T/uno.txt > $T/loop.txt
sed -n '/^---BUILT/,/^---BIGINT/p' $T/uno.txt > $T/built.txt
sed -n '/^---BIGINT/,/^---EXC/p' $T/uno.txt > $T/bigint.txt
sed -n '/^---EXC/,/^---BUCLE/p' $T/uno.txt > $T/exc.txt
sed -n '/^---BUCLE/,/^---ORDEN/p' $T/uno.txt > $T/bucle.txt
echo "-- JS.EXE y lo que usa ClicGest --"
check uno "JS de Hispa-486 (QuickJS $(cat terceros/quickjs/VERSION))"
check uno "MOTOR-OK"
checkn uno "FALLA"
check uno "Memoria: "
echo "-- las pruebas de QuickJS --"
for p in lang clos loop built bigint; do
  if grep -q "^NIVEL-1\|^Error" $T/$p.txt; then ko "test $p: $(grep -m1 'Error\|assert' $T/$p.txt)"; else ok "test $p sin errores"; fi
done
echo "-- excepciones, el tope de tiempo y una orden suelta --"
check exc "Error: TypeError: a proposito"
check exc "at f (EXC.JS:2"
check exc "^NIVEL-1"
check bucle "interrupted"
check bucle "^NIVEL-1"
check uno "2-4-6"
check uno "SIGUE-VIVO"
exit $fail
