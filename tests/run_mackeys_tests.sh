#!/bin/bash
# Prueba de las teclas de sustitución de TECLADO /MAC (los portátiles de Apple
# no tienen Supr, Inicio, Fin, RePág, AvPág ni Insertar):
#   Ctrl+Retroceso = Supr, Alt+Retroceso = Insertar,
#   Alt+izquierda/derecha = Inicio/Fin, Alt+arriba/abajo = RePág/AvPág
# 1) equipo «Apple» (SMBIOS): se activan solas; lo que da INT 16h (TECLAS.COM)
#    y la edición de la línea de órdenes con ellas
# 2) PC normal: no se activan; /MAC las pone y /NOMAC las quita
# 3) TECLADO US /MAC sin la distribución española
# 3b) «Apple» con la tabla SMBIOS por encima de 1 MB (cabecera «_SM_»)
# 4) «Apple» sin controlador de teclado (8042) y teclado USB de HSPUSB
# Mac replacement keys test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
APPLE='type=1,manufacturer=Apple Inc.,product=MacBookAir7,,2'
MSG="Teclado de Mac: Ctrl+Retroceso = Supr, Alt+flechas = Inicio/Fin/RePág/AvPág,"
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$1" $T/log.txt; then ok "$1"; else ko "$1"; fi; }
nocheck() { if grep -qF -- "$1" $T/log.txt; then ko "no debía salir: $1"; else ok "no sale: $1"; fi; }
sale() { if grep -qxF -- "$1" $T/log.txt; then ok "$2: $1"; else ko "$2: $1"; fi; }
# las teclas que ha visto TECLAS.COM (sin los Esc), en una línea
vistas() { grep -o 'X=[0-9A-F]* N=[0-9A-F]* I=[01]' $T/log.txt | grep -v 'X=011B' | tr '\n' ';'; }
teclas() { local v; v=$(vistas); if [ "$v" = "$1" ]; then ok "$2"; else ko "$2: $v"; fi; }
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon >/dev/null 2>&1; }
k() { local x; for x in "$@"; do mon "sendkey $x"; sleep 0.1; done; }
# combinaciones: con más calma (se pulsan y sueltan dos teclas)
c() { local x; for x in "$@"; do mon "sendkey $x"; sleep 0.3; done; }
t() {
  local s=$1 i ch
  for ((i = 0; i < ${#s}; i++)); do
    ch=${s:i:1}
    case "$ch" in
      ' ') ch=spc ;;
      '\') ch=backslash ;;
      '/') ch=slash ;;
      [A-Z]) ch=shift-$(printf '%s' "$ch" | tr A-Z a-z) ;;
    esac
    k "$ch"
  done
}
intro() { k ret; sleep "${1:-1}"; }
arranca() {
  T=$(mktemp -d)
  cp "$IMG" $T/f.img
  qemu-system-i386 -display none -serial file:$T/ser.txt -monitor unix:$T/mon,server,nowait \
      "$@" -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot &
  QP=$!
  sleep 6
}
# apagar y dejar el registro en UTF-8 con los retrocesos aplicados
acaba() {
  kill $QP 2>/dev/null; wait $QP 2>/dev/null
  tr -d '\r' < $T/ser.txt | iconv -f CP437 -t UTF-8 > $T/log.txt
  python3 - $T/log.txt <<'PYEOF'
import sys
p = sys.argv[1]
out = []
for line in open(p, encoding='utf-8', errors='replace').read().split('\n'):
    cur = []
    pos = 0
    for ch in line:
        if ch == '\b':
            pos = max(0, pos - 1)
        else:
            if pos < len(cur):
                cur[pos] = ch
            else:
                cur.append(ch)
            pos += 1
    out.append(''.join(cur).rstrip())
open(p, 'w', encoding='utf-8').write('\n'.join(out))
PYEOF
}
fin() {
  if [ $fail -ne $1 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/AUTOEXEC/,$p' $T/log.txt; fi
  rm -rf $T
}

# ---- 1) equipo de Apple: detección por SMBIOS ----
f0=$fail
arranca -smbios "$APPLE"
t "keyb sp"; intro 1.5
t "cd bin"; intro
t "teclas"; intro
c ctrl-backspace alt-left alt-right alt-up alt-down alt-backspace alt-backspace alt_r-up
k left backspace esc; sleep 0.5
t "hola mundo"; c alt-left; t "echo "; intro              # Alt+izquierda = Inicio
t "echo Xfin"; k left left left left; c ctrl-backspace; intro   # Ctrl+Retroceso = Supr
t "echo ab"; c alt-left alt-right; t "c"; intro           # Alt+derecha = Fin
t "echo abcd"; k left left left left; c alt-backspace; t "XY"; intro   # Alt+Retroceso = Insertar
t "keyb"; intro
acaba
echo "== Hispa-DOS teclas de Mac: equipo de Apple (SMBIOS) =="
check "Teclado español instalado."
check "$MSG"
check "Alt+Retroceso = Insertar"
teclas "X=53E0 N=5300 I=0;X=47E0 N=4700 I=0;X=4FE0 N=4F00 I=0;X=49E0 N=4900 I=0;X=51E0 N=5100 I=0;X=52E0 N=5200 I=1;X=52E0 N=5200 I=0;X=49E0 N=4900 I=0;X=4BE0 N=4B00 I=0;X=0E08 N=0E08 I=0;" \
  "Supr, Inicio, Fin, RePág, AvPág, Insertar (indicador), Alt derecho, flecha y Retroceso normales"
sale "hola mundo" "línea de órdenes, Alt+izquierda (Inicio)"
sale "fin" "línea de órdenes, Ctrl+Retroceso (Supr)"
sale "abc" "línea de órdenes, Alt+derecha (Fin)"
sale "XYcd" "línea de órdenes, Alt+Retroceso (Insertar)"
n=$(grep -cF "$MSG" $T/log.txt)
if [ "$n" -eq 2 ]; then ok "TECLADO a secas lo recuerda"; else ko "TECLADO a secas: $n"; fi
fin $f0

# ---- 2) PC normal: solo con /MAC ----
f0=$fail
arranca
t "keyb sp"; intro 1.5
t "cd bin"; intro
t "teclas"; intro
c ctrl-backspace; k esc; sleep 0.5
t "keyb sp "; k shift-7; t "mac"; intro 1.5               # «/» es Mayús+7
t "teclas"; intro
c ctrl-backspace alt-left; k esc; sleep 0.5
t "keyb "; k shift-7; t "nomac"; intro 1.5
t "teclas"; intro
c ctrl-backspace; k esc; sleep 0.5
t "keyb "; k shift-7; t "otra"; intro
acaba
echo "== Hispa-DOS teclas de Mac: PC normal, /MAC y /NOMAC =="
check "Teclado español instalado."
n=$(grep -cF "$MSG" $T/log.txt)
if [ "$n" -eq 1 ]; then ok "sin /MAC no se activan en un PC"; else ko "mensajes de Mac: $n"; fi
check "Teclado de Mac: desactivado."
teclas "X=0E7F N=0E7F I=0;X=53E0 N=5300 I=0;X=47E0 N=4700 I=0;X=0E7F N=0E7F I=0;" "Ctrl+Retroceso: normal, Supr con /MAC, normal con /NOMAC"
check "Uso: TECLADO [SP|ES|US] [/MAC|/NOMAC]"
fin $f0

# ---- 3) TECLADO US /MAC: sin la distribución española ----
f0=$fail
arranca
t "keyb us /mac"; intro 1.5
t "bin\\teclas"; intro
c alt-left ctrl-backspace; k semicolon esc; sleep 0.5
t "keyb sp"; intro
acaba
echo "== Hispa-DOS teclas de Mac: TECLADO US /MAC =="
check "Teclado activo: estadounidense (US)."
check "$MSG"
teclas "X=47E0 N=4700 I=0;X=53E0 N=5300 I=0;X=273B N=273B I=0;" "Inicio y Supr; la tecla ; sigue siendo la de EE. UU."
check "Teclado activo: español (SP)."
fin $f0

# ---- 3b) cabecera «_SM_» (SMBIOS de 32 bits) y tabla por encima del primer
# megabyte (con más de 600 octetos SeaBIOS la pone arriba): se lee con
# INT 15h AH=87h, como en los equipos con EFI
f0=$fail
RELLENO=$(printf 'relleno-%.0s' $(seq 90))
arranca -machine pc,smbios-entry-point-type=32 -smbios "$APPLE" -smbios "type=11,value=$RELLENO"
mon "pmemsave 0xf0000 0x10000 \"$T/fseg.bin\""; sleep 0.5
t "keyb us"; intro 1.5
t "bin\\teclas"; intro
c alt-right; k esc; sleep 0.5
acaba
echo "== Hispa-DOS teclas de Mac: SMBIOS «_SM_» con la tabla por encima de 1 MB =="
python3 - $T/fseg.bin <<'PYEOF' || fail=1
import struct, sys
d = open(sys.argv[1], 'rb').read()
alta = None
for i in range(0, len(d), 16):
    if d[i:i + 4] == b'_SM_':
        alta = struct.unpack_from('<I', d, i + 0x18)[0]
good = alta is not None and alta >= 0x100000
print(("  OK   " if good else "  FAIL ") + "la tabla está por encima de 1 MB (%s)" % (hex(alta) if alta else "sin _SM_"))
sys.exit(0 if good else 1)
PYEOF
check "Teclado activo: estadounidense (US)."
check "$MSG"
teclas "X=4FE0 N=4F00 I=0;" "Alt+derecha = Fin"
fin $f0

# ---- 4) Apple sin controlador de teclado (8042), teclado USB de HSPUSB ----
f0=$fail
T=$(mktemp -d)
cp "$IMG" $T/f.img
printf 'DEVICE=A:\\HISPADOS\\HSPUSB.SYS /F\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
Q="python3 tests/tools/qmp.py $T/qmp"
qemu-system-i386 -machine pc,i8042=off -smbios "$APPLE" -display none -serial file:$T/ser.txt \
    -monitor none -parallel none -qmp unix:$T/qmp,server,nowait \
    -vga none -device VGA,id=vga0 -device qemu-xhci,id=xhci \
    -device usb-kbd,bus=xhci.0,id=ukbd,display=vga0 \
    -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot &
QP=$!
sleep 10
$Q type vga0 "A:\\HISPADOS\\TECLADO SP" key vga0 ret wait 2 \
   type vga0 "CD BIN" key vga0 ret wait 1 \
   type vga0 "TECLAS" key vga0 ret wait 2 \
   combo vga0 ctrl+backspace combo vga0 alt+left combo vga0 alt+right \
   combo vga0 alt+up combo vga0 alt+down combo vga0 alt+backspace \
   key vga0 left key vga0 esc wait 1 \
   type vga0 "hola mundo" combo vga0 alt+left type vga0 "ECHO " key vga0 ret wait 1 \
   type vga0 "ECHO Xfin" key vga0 left key vga0 left key vga0 left key vga0 left \
   combo vga0 ctrl+backspace key vga0 ret wait 1
acaba
echo "== Hispa-DOS teclas de Mac: sin 8042, teclado USB (HSPUSB) =="
check "Sin controlador de teclado (8042): las teclas van al búfer de la BIOS."
check "Teclado español instalado."
check "$MSG"
teclas "X=53E0 N=5300 I=0;X=47E0 N=4700 I=0;X=4FE0 N=4F00 I=0;X=49E0 N=4900 I=0;X=51E0 N=5100 I=0;X=52E0 N=5200 I=1;X=4B00 N=4B00 I=1;" \
  "Supr, Inicio, Fin, RePág, AvPág e Insertar por el teclado USB"
sale "hola mundo" "línea de órdenes, Alt+izquierda (Inicio)"
sale "fin" "línea de órdenes, Ctrl+Retroceso (Supr)"
fin $f0
exit $fail
