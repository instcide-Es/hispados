#!/bin/bash
# Pruebas del núcleo: memoria (4Ah), EXEC (entorno, MINALLOC, .COM sin
# memoria), país (38h), fecha y hora, Ctrl+C / INT 23h (IRET y RETF),
# INT 28h y un controlador CON cargado con DEVICE= (teclado y tabuladores).
# Los programas están en tests/progs/nuc*.asm (make nucprogs).
# Kernel tests: memory, EXEC, country, Ctrl+C / INT 23h, INT 28h, CON driver.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=build
for f in NUCTEST.COM NUCHIJO.COM NUCEXE.EXE NUCBRK.COM NUCCON.SYS NUCDRV.SYS; do
  [ -f $B/$f ] || make -s nucprogs
done
T=$(mktemp -d)
cp "$IMG" $T/f.img
for f in NUCTEST.COM NUCHIJO.COM NUCEXE.EXE NUCBRK.COM NUCCON.SYS NUCDRV.SYS; do
  mcopy -o -i $T/f.img $B/$f ::BIN/$f
done
# un .COM de 65.200 bytes (cabe en 64 KB, pero no con el PSP y la pila)
python3 -c "import sys; sys.stdout.buffer.write(b'\xC3' + bytes(65199))" > $T/NUCBIG.COM
mcopy -o -i $T/f.img $T/NUCBIG.COM ::BIN/NUCBIG.COM
cp $T/f.img $T/con.img
printf 'DEVICE=A:\\BIN\\NUCCON.SYS\r\nDEVICE=A:\\BIN\\NUCDRV.SYS /hola\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/con.img $T/CONFIG.SYS ::CONFIG.SYS

# guion: una orden por línea (se envía con Intro); "@N" espera N segundos;
# "^C" envía Ctrl+C; "^Z" envía Ctrl+Z e Intro; "#texto" envía el texto sin Intro
run() {   # imagen guion salida tiempo
  { sleep 4
    while IFS= read -r c; do
      case "$c" in
        @*) sleep "${c#@}";;
        ^C) printf '\003';;
        ^Z) printf '\032\r';;
        \#*) printf '%s' "${c#\#}";;
        *) printf '%s\r' "$c";;
      esac
      sleep 0.7
    done < "$2"
    sleep 2
  } | timeout "$4" qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
        -drive file="$1",format=raw,if=floppy -boot a -no-reboot > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 > "$3"
}

cat > $T/g1 <<'EOF'
BIN\MCBWALK
BIN\NUCTEST
@4
BIN\MCBWALK
ECHO fin-uno
EOF
cat > $T/g2 <<'EOF'
BIN\NUCBRK R
@1
^C
#x
^C
#q
@1
BIN\NUCBRK P
@1
^C
@1
BIN\NUCBRK O
@1
^C
@1
ECHO tras-o
BIN\NUCBRK B
@1
^C
@1
ECHO tras-b
BIN\NUCBRK I
@1
#z
@1
BIN\NUCBRK K
@1
#a
#b
#q
@1
BIN\NUCBRK T
BIN\NUCBRK Z
hola
^Z
q
BIN\NUCBRK D
IF ERRORLEVEL 255 ECHO tras-division
#abc
^C
ECHO tras-indicador
ECHO fin-dos
EOF
cat > $T/g3 <<'EOF'
BIN\NUCBRK K
@1
#a
#b
#q
@1
BIN\NUCBRK O
@1
^C
@1
ECHO tras-o
BIN\NUCBRK T
BIN\NUCBRK R
@1
^C
#q
@1
ECHO fin-tres
EOF
run $T/f.img $T/g1 $T/l1 21
run $T/f.img $T/g2 $T/l2 57
run $T/con.img $T/g3 $T/l3 30

fail=0
check()  { if grep -qF -- "$2" "$1"; then echo "  OK   $2"; else echo "  FAIL $2"; fail=1; fi; }
checkx() { if grep -qxF -- "$2" "$1"; then echo "  OK   $2"; else echo "  FAIL $2"; fail=1; fi; }
checkn() { if grep -qF -- "$2" "$1"; then echo "  FAIL (no debía salir) $2"; fail=1; else echo "  OK   sin \"$2\""; fi; }
echo "== Hispa-DOS: núcleo (memoria, EXEC, país, hora) =="
for t in 1F-DL 3001 38-FIJAR 38-FIJAR-OTRO 38-24H 38-MAYUS 6501-CORTO 6501-ENTERO 4A-MAXIMO 4A-DUENO 4B-SUBF \
         ENTORNO-VACIO ENTORNO-NOMBRE EXE-MINALLOC COM-SIN-MEMORIA EXE-SIN-MEMORIA 2C-CENTESIMAS 2C-HORA PSP-0006; do
  check $T/l1 "NUC $t OK"
done
check  $T/l1 "NUC fin"
checkn $T/l1 "FALLO"
checkx $T/l1 "fin-uno"
# tras las pruebas no queda memoria perdida: MCBWALK da lo mismo antes y después
grep -E '^[0-9A-F]{4} [MZ] [0-9A-F]{4} [0-9A-F]{4} ' $T/l1 > $T/mcb
n=$(wc -l < $T/mcb)
if [ "$n" -ge 6 ] && [ $((n % 2)) -eq 0 ] && [ "$(head -n $((n / 2)) $T/mcb)" = "$(tail -n $((n / 2)) $T/mcb)" ]; then
  echo "  OK   cadena de MCB igual antes y después (sin bloques perdidos)"
else echo "  FAIL cadena de MCB distinta tras las pruebas"; fail=1; fi
echo "== Hispa-DOS: Ctrl+C, INT 23h, INT 28h =="
check  $T/l2 "BRK-R sigue, veces=2"
checkn $T/l2 "BRK-A MAL"
check  $T/l2 "BRK-P retorno=0103"
checkn $T/l2 "BRK-O MAL"
checkx $T/l2 "tras-o"
checkn $T/l2 "BRK-B MAL"
checkx $T/l2 "tras-b"
check  $T/l2 "BRK-I OK"
check  $T/l2 "[a][b][q]"
check  $T/l2 "T:x     y"
check  $T/l2 "BRK-Z fin, leidos=0006"
checkn $T/l2 "BRK-Z MAL"
check  $T/l2 "Error de división"
checkn $T/l2 "BRK-D MAL"
checkx $T/l2 "tras-division"
checkx $T/l2 "tras-indicador"
checkn $T/l2 "Reiniciando el int"
checkx $T/l2 "fin-dos"
echo "== Hispa-DOS: controlador CON cargado con DEVICE= =="
check  $T/l3 "NUCDRV: A:\\BIN\\NUCDRV.SYS /hola"
check  $T/l3 "[a][b][q]"
check  $T/l3 "BRK-K fin"
checkn $T/l3 "BRK-O MAL"
checkx $T/l3 "tras-o"
check  $T/l3 "T:x     y"
check  $T/l3 "BRK-R sigue, veces=1"
checkx $T/l3 "fin-tres"
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then
  echo "--- 1 ---"; cat $T/l1; echo "--- 2 ---"; cat $T/l2; echo "--- 3 ---"; cat $T/l3
fi
rm -rf $T
exit $fail
