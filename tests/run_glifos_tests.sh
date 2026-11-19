#!/bin/bash
# Prueba de las letras que la 437 no tiene y Hispa Ventanas sí (Á, Ó, €...:
# tools/hispa437.py, docs/VENTANAS.md). Arranca con el teclado español (KEYB
# SP) y, en el Bloc de notas:
#  - escribe con las teclas muertas las mayúsculas con acento (´ ` ^ ¨ y la
#    vocal) y el euro con AltGr+E: «Últimos 33,00 €», «ÁÍÓÚ ÀÈÒÙ ÊÔ ËÏ»; se
#    ven con la letra de Ventanas y se guardan con sus códigos;
#  - abre GLIFOS.TXT, con las 26 letras nuevas, y se ven todas;
#  - al salir de Ventanas, KEYB vuelve a la 437 de modo texto: ´ y Mayús+A
#    dan «A» (no el recuadro de B6h).
# Deja capturas en build/glifos_*.png.
# Test of the extra Hispa Ventanas letters (Á, Ó, €...).
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=build
T=$(mktemp -d)
CHK="python3 tests/tools/ventanas_check.py"
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$1" $T/log.txt; then ok "$1"; else ko "$1"; fi; }
chk() { $CHK "$@" || fail=1; }
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.3}"; }
tecla() { local k; for k in "$@"; do mon "sendkey $k" 0.3; done; }
shot() { sleep 0.4; mon "screendump $T/$1.ppm" 0.6; }
send() { printf '%s\r' "$1"; sleep "${2:-1.5}"; }
muestra() { [ -f $T/$1.ppm ] && convert $T/$1.ppm $B/glifos_$2.png; }
RX=400; RY=300
ir() {
  local dx=$(( $1 - RX )) dy=$(( $2 - RY )) sx sy
  while [ $dx -ne 0 ] || [ $dy -ne 0 ]; do
    sx=$dx; [ $sx -gt 60 ] && sx=60; [ $sx -lt -60 ] && sx=-60
    sy=$dy; [ $sy -gt 60 ] && sy=60; [ $sy -lt -60 ] && sy=-60
    mon "mouse_move $sx $sy" 0.07
    dx=$(( dx - sx )); dy=$(( dy - sy ))
  done
  RX=$1; RY=$2; sleep 0.3
}
clic() { ir $1 $2; mon "mouse_button 1" 0.25; mon "mouse_button 0" 0.5; }
aparca() { ir 16 320; }
run() {
  local tmo=$1; shift
  rm -f $T/mon
  timeout $tmo qemu-system-i386 ${QEMU_PANTALLA--vga std -global VGA.xres=800 -global VGA.yres=600} -display none -serial stdio -monitor unix:$T/mon,server,nowait \
    -parallel none -m 16 -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a -no-reboot "$@" > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 > $T/log.txt
}
TODAS="ÀÁÂÃ ÈÊË ÌÍÎÏ ÒÓÔÕ ÙÚÛ ãõ Œœ ©® €…"
TIPEADO1="Últimos 33,00 €"
TIPEADO2="ÁÍÓÚ ÀÈÒÙ ÊÔ ËÏ á"

echo "== Hispa Ventanas: Á, Ó, €... (las letras que la 437 no tiene) =="
export MTOOLS_SKIP_CHECK=1
cp "$IMG" $T/f.img
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
python3 -c "
import sys; sys.path.insert(0, 'tools'); import hispa437
open('$T/GLIFOS.TXT', 'wb').write(bytes(hispa437.codigo(c) for c in '$TODAS') + b'\r\n')"
mcopy -o -i $T/f.img $T/GLIFOS.TXT ::GLIFOS.TXT
{ sleep 6
  send 'KEYB SP' 2
  send 'A:\HISPADOS\HSPDPMI' 2
  send 'A:\HISPADOS\HSPRATON' 2
  send 'VENTANAS' 4
  tecla right ret; sleep 1                  # el grupo «Accesorios»
  tecla ret; sleep 1.2; aparca              # el Bloc de notas
  # «Últimos 33,00 €»: ´ y Mayús+U; AltGr+E
  tecla apostrophe shift-u l t i m o s spc 3 3 comma 0 0 spc alt_r-e ret
  # «ÁÍÓÚ ÀÈÒÙ ÊÔ ËÏ á»
  tecla apostrophe shift-a apostrophe shift-i apostrophe shift-o apostrophe shift-u spc
  tecla bracket_left shift-a bracket_left shift-e bracket_left shift-o bracket_left shift-u spc
  tecla shift-bracket_left shift-e shift-bracket_left shift-o spc
  tecla shift-apostrophe shift-e shift-apostrophe shift-i spc apostrophe a
  shot g1
  tecla alt-a down down down ret; sleep 1.2 # Archivo > Guardar como...
  tecla g l i f o s 2 ret; sleep 1.5
  shot g2
  tecla alt-a down ret; sleep 1.2           # Archivo > Abrir...
  tecla shift-g shift-l shift-i shift-f shift-o shift-s dot shift-t shift-x shift-t ret; sleep 1.5
  tecla ctrl-end; aparca                    # (el cursor, a la línea de abajo)
  shot g3
  tecla alt-f4; sleep 1                     # fuera el Bloc
  clic 475 65; sleep 0.8; tecla ret; sleep 2  # y Ventanas
  send 'ECHO TRAS-SALIR'
  # en modo texto, ´ y Mayús+A = A
  tecla e c h o spc shift-z apostrophe shift-a shift-z ret; sleep 1
  mon quit
} | run 200 -vga std

check "Teclado español instalado."
check "TRAS-SALIR"
echo "-- escrito con el teclado (teclas muertas y AltGr+E) --"
# (la última letra de la segunda línea la toca el cursor)
chk $T/g1.ppm texto 156 138 0 9 fina "$TIPEADO1" texto 156 154 0 9 fina "${TIPEADO2% *}"
chk $T/g2.ppm titulo 150 90 activa "Bloc de notas - glifos2.TXT"
echo "-- GLIFOS.TXT: las 26 letras nuevas --"
chk $T/g3.ppm titulo 150 90 activa "Bloc de notas - GLIFOS.TXT" texto 156 138 0 9 fina "$TODAS"
if mtype -i $T/f.img ::GLIFOS2.TXT > $T/g2.txt 2> /dev/null && python3 - $T/g2.txt "$TIPEADO1" "$TIPEADO2" <<'PY'
import sys; sys.path.insert(0, 'tools'); import hispa437
e = lambda t: bytes(hispa437.codigo(c) for c in t)
sys.exit(0 if open(sys.argv[1], 'rb').read() == e(sys.argv[2]) + b'\r\n' + e(sys.argv[3]) else 1)
PY
then ok "GLIFOS2.TXT guardado con los códigos de Ventanas (Ú = D7h, € = F4h...)"
else ko "GLIFOS2.TXT: $(od -An -tx1 $T/g2.txt 2> /dev/null | head -2)"; fi
echo "-- fuera de Ventanas, KEYB vuelve a la 437 de modo texto --"
if grep -q "^ZAZ" $T/log.txt; then ok "´ y Mayús+A en modo texto: «ZAZ»"; else ko "en modo texto no sale «ZAZ»: $(grep -a 'Z.Z' $T/log.txt | head -2)"; fi
muestra g1 escrito; muestra g3 todas
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then tail -30 $T/log.txt; fi
[ -n "${GUARDAR:-}" ] && cp $T/*.ppm "$GUARDAR"/
rm -rf $T
exit $fail
