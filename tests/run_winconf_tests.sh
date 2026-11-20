#!/bin/bash
# Windows 3.1x sobre Hispa-DOS: lo que deja su programa de instalación y WINCONF
# (docs/WINDOWS.md). En el disquete de pruebas se ponen el CONFIG.SYS y el
# AUTOEXEC.BAT de Hispa-DOS como quedan instalados (sin las líneas de
# SOLO-MEDIO) y, en A:\WINDOWS, lo que deja el instalador de Trabajo en Grupo
# 3.11 con un CONFIG.SYS con menú: CONFIG.WIN y AUTOEXEC.WIN (los archivos
# enteros con sus cambios: la PATH con su carpeta delante, SET TEMP, una orden
# suya y STACKS=), un SYSTEM.INI con device=vshare.386 y un WIN.COM (HELLO).
#   1) Al arrancar, AUTOEXEC.BAT llama a WINCONF /AUTO (hay WIN.COM): aplica
#      los cambios y reinicia. CONFIG.SYS queda con la opción «Windows» (la de
#      omisión) y su bloque; AUTOEXEC.BAT, con la PATH de Windows y el bloque
#      :WINDOWS; SYSTEM.INI, con vshare.386 comentado; los de antes, .SIN; los
#      .WIN, .APL.
#   2) Tras el reinicio: la opción Windows (CONFIG=WINDOWS), PATH con las dos
#      carpetas, la orden de Windows hecha, WIN se encuentra desde A:\, y
#      WINCONF no vuelve a hacer nada (sale una sola vez).
#   3) Una actualización pone los archivos de serie otra vez: WINCONF vuelve a
#      poner lo de Windows desde los .APL (igual que la primera vez).
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
export MTOOLS_SKIP_CHECK=1
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$2" "$1"; then ok "$2"; else ko "$2"; fi; }
checkn() { if grep -qF -- "$2" "$1"; then ko "(no debe salir) $2"; else ok "no sale: $2"; fi; }
send() { sleep "${2:-1.5}"; printf '%s\r' "$1"; }
txt() { tr -d '\r' < $1 | iconv -f CP437 -t UTF-8 > $1.txt; }

cp "$IMG" $T/f.img
python3 - "$T" <<'EOF'
import sys, os, re
T = sys.argv[1]
def dos(lines):
    return ('\r\n'.join(lines) + '\r\n').encode('cp437')
def instalado(f):
    t = open(f, encoding='utf-8').read()
    t = re.sub(r'(?ms)^[^\n]*\[SOLO-MEDIO.*?SOLO-MEDIO\][^\n]*\n', '', t)
    return t.rstrip('\n').split('\n')
cfg = instalado('floppy/CONFIG.SYS')
aut = instalado('floppy/AUTOEXEC.BAT')
open(os.path.join(T, 'CONFIG.SYS'), 'wb').write(dos(cfg))
open(os.path.join(T, 'AUTOEXEC.BAT'), 'wb').write(dos(aut))
# lo que haría el instalador de Windows (como en el disco de Carlos)
win = []
for l in aut:
    if l.startswith('PATH '):
        win += ['PATH A:\\WINDOWS;' + l[5:], 'SET TEMP=A:\\WINDOWS\\TEMP', 'A:\\WINDOWS\\HOLA de-windows']
    else:
        win.append(l)
open(os.path.join(T, 'AUTOEXEC.WIN'), 'wb').write(dos(win))
open(os.path.join(T, 'CONFIG.WIN'), 'wb').write(dos(cfg + ['STACKS=9,256']))
open(os.path.join(T, 'SYSTEM.INI'), 'wb').write(dos(['[386Enh]', 'device=ifsmgr.386', 'device=vcache.386',
                                                      'device=vshare.386', 'local=CON']))
EOF
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
mcopy -o -i $T/f.img $T/AUTOEXEC.BAT ::AUTOEXEC.BAT
mmd -i $T/f.img ::WINDOWS
for f in CONFIG.WIN AUTOEXEC.WIN SYSTEM.INI; do mcopy -i $T/f.img $T/$f ::WINDOWS/$f; done
mcopy -i $T/f.img build/HELLO.COM ::WINDOWS/WIN.COM
mcopy -i $T/f.img build/HELLO.COM ::WINDOWS/HOLA.COM

# ------------------------------------------------------------------ 1 y 2
# (sin -no-reboot: WINCONF reinicia)
arranque() {
  sleep 40
  send "ECHO =A1=" 1
  send "SET" 1
  send "WIN /PRUEBA" 2
  send "ECHO =A2=" 1
}
arranque | timeout 70 qemu-system-i386 -display none -serial stdio -monitor none \
    -drive file=$T/f.img,format=raw,if=floppy -boot a > $T/l1 2>&1
txt $T/l1
echo "== Windows 3.1x: WINCONF =="
echo "-- primer arranque: se aplican los cambios --"
check $T/l1.txt "WINCONF - cambios de Windows 3.1x para Hispa-DOS"
check $T/l1.txt "Cambios propuestos por Windows: A:\\WINDOWS\\CONFIG.WIN"
check $T/l1.txt "Cambios propuestos por Windows: A:\\WINDOWS\\AUTOEXEC.WIN"
check $T/l1.txt "Puesto al día: A:\\CONFIG.SYS (el anterior queda como A:\\CONFIG.SIN)"
check $T/l1.txt "SYSTEM.INI: desactivada la línea device=vshare.386."
check $T/l1.txt "Hispa-DOS se reinicia para usarlos"
n=$(grep -c "WINCONF - cambios" $T/l1.txt)
[ "$n" = 1 ] && ok "WINCONF solo actúa una vez (en el segundo arranque, en silencio)" || ko "WINCONF ha salido $n veces"
echo "-- tras el reinicio: la opción Windows --"
awk 'index($0,"=A1=") {p=1; next} p && index($0,"=A2=") {exit} p' $T/l1.txt > $T/a1
check $T/a1 "CONFIG=WINDOWS"
check $T/a1 "PATH=A:\\WINDOWS;A:\\HISPADOS"
check $T/a1 "TEMP=A:\\WINDOWS\\TEMP"
check $T/a1 "Args:[ /PRUEBA]"
checkn $T/a1 "Comando o nombre de archivo incorrecto"
check $T/l1.txt "Args:[ de-windows]"
# los archivos
mtype -i $T/f.img ::CONFIG.SYS | tr -d '\r' > $T/c1
mtype -i $T/f.img ::AUTOEXEC.BAT | tr -d '\r' > $T/b1
mtype -i $T/f.img ::WINDOWS/SYSTEM.INI | tr -d '\r' > $T/s1
check $T/c1 "MENUITEM=WINDOWS,Windows 3.1x (sin los emuladores de Hispa-DOS)"
check $T/c1 "MENUDEFAULT=WINDOWS,5"
check $T/c1 "[windows]"
check $T/c1 "INCLUDE=NORMAL"
check $T/c1 "STACKS=9,256"
checkn $T/c1 "MENUDEFAULT=NORMAL"
check $T/b1 'IF "%CONFIG%"=="WINDOWS" GOTO WINDOWS'
check $T/b1 ":WINDOWS"
grep -q "^PATH A:\\\\WINDOWS;A:\\\\HISPADOS$" $T/b1 && ok "AUTOEXEC.BAT: PATH A:\\WINDOWS;A:\\HISPADOS" || ko "AUTOEXEC.BAT sin la PATH de Windows"
[ "$(grep -c '^PATH ' $T/b1)" = 1 ] && ok "una sola línea PATH" || ko "varias líneas PATH"
# el bloque :WINDOWS: teclado y ratón, lo de Windows y nada de sonido ni mandos
awk '/^:WINDOWS$/ {p=1; next} p && /^GOTO / {exit} p' $T/b1 > $T/blk
grep -q "TECLADO SP" $T/blk && grep -q "HSPRATON" $T/blk && ok "bloque :WINDOWS con TECLADO y HSPRATON" || ko "bloque :WINDOWS sin TECLADO o HSPRATON"
grep -q "SET TEMP=A:\\\\WINDOWS\\\\TEMP" $T/blk && grep -q "HOLA de-windows" $T/blk && ok "bloque :WINDOWS con lo de Windows" || ko "bloque :WINDOWS sin lo de Windows"
grep -qE "HSPSND|HSPJOY|HSPDPMI|HSPEMM" $T/blk && ko "bloque :WINDOWS con emuladores" || ok "bloque :WINDOWS sin HSPSND, HSPJOY ni emuladores"
grep -q "^;device=vshare.386" $T/s1 && ! grep -q "^device=vshare.386" $T/s1 && ok "SYSTEM.INI: vshare.386 comentado" || ko "SYSTEM.INI: vshare.386"
grep -q "^device=vcache.386" $T/s1 && ok "SYSTEM.INI: lo demás, igual" || ko "SYSTEM.INI: vcache.386 tocado"
mtype -i $T/f.img ::CONFIG.SIN | tr -d '\r' | cmp -s - <(tr -d '\r' < $T/CONFIG.SYS) && ok "CONFIG.SIN = el de antes" || ko "CONFIG.SIN"
mdir -i $T/f.img ::WINDOWS > $T/dw
grep -qE "^CONFIG +APL" $T/dw && grep -qE "^AUTOEXEC +APL" $T/dw && ! grep -qE " WIN +[0-9]" $T/dw && ok ".WIN pasados a .APL" || ko ".WIN/.APL"

# ------------------------------------------------------------------ 3
md5c=$(md5sum < $T/c1); md5b=$(md5sum < $T/b1)
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
mcopy -o -i $T/f.img $T/AUTOEXEC.BAT ::AUTOEXEC.BAT
arranque | timeout 70 qemu-system-i386 -display none -serial stdio -monitor none \
    -drive file=$T/f.img,format=raw,if=floppy -boot a > $T/l3 2>&1
txt $T/l3
echo "-- tras una actualización (archivos de serie): otra vez desde los .APL --"
check $T/l3.txt "Cambios propuestos por Windows: A:\\WINDOWS\\CONFIG.APL"
check $T/l3.txt "PATH=A:\\WINDOWS;A:\\HISPADOS"
[ "$(mtype -i $T/f.img ::CONFIG.SYS | tr -d '\r' | md5sum)" = "$md5c" ] && ok "CONFIG.SYS como la primera vez" || ko "CONFIG.SYS distinto"
[ "$(mtype -i $T/f.img ::AUTOEXEC.BAT | tr -d '\r' | md5sum)" = "$md5b" ] && ok "AUTOEXEC.BAT como la primera vez" || ko "AUTOEXEC.BAT distinto"

if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then echo "--- salida ---"; cat $T/l1.txt; cat $T/b1; fi
rm -rf $T
exit $fail
