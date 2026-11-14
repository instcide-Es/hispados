#!/bin/bash
# Prueba de PROTECTOR (PROTECTO.COM), el protector de pantalla en modo texto
# («Lluvia verde»). Arranca en QEMU, escribe las órdenes por el puerto serie
# (que no cuenta como teclado), pulsa teclas y mueve el ratón con el monitor
# de QEMU, saca capturas y la memoria de texto (pmemsave 0xB8000) y las
# compara:
#  - está en \HISPADOS del disquete y de la imagen USB; AYUDA PROTECTOR;
#  - sin cargar: «no está en la memoria»; los minutos van de 1 a 60;
#  - PROTECTOR /S:n (n segundos, para las pruebas) se queda en la memoria y
#    PROTECTOR dice cómo está;
#  - pasado el tiempo salta: caracteres verdes que caen (dos capturas
#    distintas, con verdes claros y oscuros y alguna cabeza blanca) y la
#    pantalla del programa, intacta, debajo;
#  - una tecla lo quita: la pantalla, exactamente la de antes (texto y
#    puntos) y la tecla no llega al intérprete;
#  - PROTECTOR /YA lo enseña al momento; mover el ratón lo quita;
#  - PROTECTOR 1: salta al minuto (con PROTECTOR 2 primero, para ver que los
#    minutos se cambian);
#  - no salta con gráficos: dentro de Hispa Ventanas no sale, y al volver
#    al modo de texto sí;
#  - PROTECTOR /NO lo quita de la memoria (y ya no salta).
# Deja capturas en build/protector_*.png.
# Text-mode screen saver (PROTECTOR) test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=$(dirname "$IMG")
T=$(mktemp -d)
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$1" $T/log.txt; then ok "$1"; else ko "$1"; fi; }
checkn() { if grep -qF -- "$1" $T/log.txt; then ko "(no debía salir) $1"; else ok "sin \"$1\""; fi; }
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.3}"; }
tecla() { local k; for k in "$@"; do mon "sendkey $k" 0.3; done; }
shot() { mon "screendump $T/$1.ppm" 0.6; }
mem() { mon "pmemsave 0xb8000 4000 \"$T/$1.bin\"" 0.5; }
send() { printf '%s\r' "$1"; sleep "${2:-1.5}"; }
muestra() { [ -f $T/$1.ppm ] && convert $T/$1.ppm $B/protector_$2.png; }
# comprobaciones de las capturas (PPM de QEMU)
cat > $T/mira.py <<'EOF'
import re, sys
def lee(r):
    d = open(r, 'rb').read()
    m = re.match(rb'P6\s+(\d+)\s+(\d+)\s+(\d+)\s', d)
    return int(m.group(1)), int(m.group(2)), d[m.end():]
op = sys.argv[1]
if op == 'verde':           # puntos verdes claros, verdes oscuros y blancos
    w, h, d = lee(sys.argv[2])
    claro = oscuro = blanco = otro = 0
    for i in range(0, len(d), 3):
        r, g, b = d[i], d[i + 1], d[i + 2]
        if r > 200 and g > 200 and b > 200: blanco += 1
        elif g > 150 and r < g - 60 and b < g - 60: claro += 1
        elif 30 < g <= 150 and r < g // 2 and b < g // 2: oscuro += 1
        elif r + g + b > 60: otro += 1
    print(w, h, claro, oscuro, blanco, otro)
elif op == 'dif':           # puntos distintos entre dos capturas
    w, h, a = lee(sys.argv[2]); w2, h2, b = lee(sys.argv[3])
    if (w, h) != (w2, h2): print(-1); sys.exit()
    print(sum(1 for i in range(0, len(a), 3) if a[i:i + 3] != b[i:i + 3]))
elif op == 'tam':
    w, h, d = lee(sys.argv[2]); print(w, h)
elif op == 'columnas':      # columnas de texto con algo verde (de 80)
    w, h, d = lee(sys.argv[2])
    cw = w // 80
    cols = set()
    for y in range(0, h, 2):
        for x in range(0, w):
            o = (y * w + x) * 3
            if d[o + 1] > 60 and d[o] < d[o + 1] // 2: cols.add(x // cw)
    print(len(cols))
EOF
PY="python3 $T/mira.py"
verde() {  # captura nombre: verdes claros y oscuros, y nada de otros colores
  local v; v=$($PY verde $T/$1.ppm); set -- $1 $v
  if [ "$4" -gt 2000 ] && [ "$5" -gt 1000 ] && [ "$6" -gt 20 ] && [ "$7" -lt 200 ]; then
    ok "$1: lluvia verde ($4 puntos verde claro, $5 verde oscuro, $6 blancos, $7 de otros colores)"
  else
    ko "$1: no se ve la lluvia verde (verde claro $4, oscuro $5, blancos $6, otros $7)"
  fi
}
igual_bin() { if cmp -s $T/$1.bin $T/$2.bin; then ok "$3"; else ko "$3"; fi; }
difiere() { local n; n=$($PY dif $T/$1.ppm $T/$2.ppm); if [ "$n" -gt "$3" ]; then ok "$4 ($n puntos)"; else ko "$4 (solo $n puntos)"; fi; }
parecida() { local n; n=$($PY dif $T/$1.ppm $T/$2.ppm); if [ "$n" -ge 0 ] && [ "$n" -le "$3" ]; then ok "$4 ($n puntos distintos: el cursor parpadea)"; else ko "$4 ($n puntos distintos)"; fi; }
run() {
  local tmo=$1; shift
  rm -f $T/mon
  timeout $tmo qemu-system-i386 -display none -serial stdio -monitor unix:$T/mon,server,nowait \
    -parallel none -m 16 -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a -vga std "$@" > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 | grep -v '^audio:' > $T/log.txt
}

echo "== PROTECTOR: en el disco =="
cp "$IMG" $T/f.img
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS   # (para Hispa Ventanas)
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
if mdir -i $T/f.img ::HISPADOS/PROTECTO.COM > /dev/null 2>&1; then
  ok "PROTECTO.COM está en \\HISPADOS del disquete ($(mdir -i $T/f.img ::HISPADOS/PROTECTO.COM | grep -i '^PROTECTO' | awk '{print $3}') octetos)"
else
  ko "falta PROTECTO.COM en \\HISPADOS del disquete"
fi
if [ -f $B/hispados-usb.img ]; then
  if MTOOLS_SKIP_CHECK=1 mdir -i $B/hispados-usb.img@@1048576 ::HISPADOS/PROTECTO.COM > /dev/null 2>&1; then
    ok "y en \\HISPADOS de la imagen USB"
  else
    ko "PROTECTO.COM no está en \\HISPADOS de la imagen USB"
  fi
fi

echo "== PROTECTOR: salta, se ve la lluvia, una tecla la quita =="
{ sleep 6
  send 'A:\HISPADOS\HSPRATON' 2
  send 'AYUDA PROTECTOR'
  send 'PROTECTOR'
  send 'PROTECTOR 61'
  send 'PROTECTOR /S:8' 2
  send 'PROTECTOR'
  send 'CLS' 1
  send 'ECHO PANTALLA-DE-ANTES' 1
  send 'DIR A:\HISPADOS\P*.COM' 2
  tecla shift                               # (el puerto serie no cuenta: una tecla)
  sleep 1
  mem t0; shot t0
  sleep 5                                   # todavía no
  shot t1
  sleep 6                                   # ya
  shot l1
  mem l1
  sleep 1.5
  shot l2
  tecla x; sleep 1
  shot t2; mem t2
  send 'ECHO TRAS-LA-LLUVIA' 2
  # ---- /YA y el ratón ----
  send 'CLS' 1
  send 'PROTECTOR /YA' 1
  mem y0                                    # (ya llueve: la página del programa)
  sleep 1
  shot y1
  mon "mouse_move 30 10" 1
  shot y2; mem y2
  sleep 3
  shot y3                                   # el ratón no la vuelve a sacar enseguida
  send 'ECHO TRAS-EL-RATON' 2
  # ---- PROTECTOR 2, luego PROTECTOR 1: salta al minuto ----
  send 'PROTECTOR 2'
  send 'PROTECTOR 1'
  send 'PROTECTOR'
  tecla shift
  sleep 50
  shot m0                                   # a los 50 s, todavía no
  sleep 15
  shot m1                                   # pasado el minuto, sí
  tecla spc; sleep 1
  send 'ECHO TRAS-EL-MINUTO' 2
  # ---- con gráficos (Hispa Ventanas) no salta ----
  send 'PROTECTOR /S:3' 1
  send 'VENTANAS' 7
  sleep 6
  shot g0
  for i in 1 2 3 4 5 6 7 8 9; do mon "mouse_move -60 -60" 0.07; done
  for i in 1 2 3 4 5 6 7 8 9 10 11 12; do mon "mouse_move 63 1" 0.07; done   # «Salir», arriba a la derecha
  sleep 0.5
  mon "mouse_button 1" 0.25; mon "mouse_button 0" 1
  tecla ret; sleep 3
  send 'CLS' 1
  send 'ECHO DE-VUELTA' 1
  sleep 5
  shot g1                                   # de vuelta en el texto, sí
  tecla spc; sleep 1
  # ---- /NO ----
  send 'PROTECTOR /NO'
  send 'PROTECTOR'
  send 'PROTECTOR /YA'
  sleep 1
  shot n0
  send 'ECHO FIN-PROTECTOR'
  mon quit
} | run 260

check "Protector de pantalla en modo texto"
check "PROTECTOR /YA       lo enseña ahora"
check "PROTECTOR no está en la memoria. Para ponerlo: PROTECTOR minutos (de 1 a 60)."
check "PROTECTOR: los minutos van de 1 a 60."
check "PROTECTOR puesto: la «Lluvia verde» sale tras 8 segundos sin tocar el teclado"
check "PROTECTOR está en la memoria: salta tras 8 segundos sin tocar el teclado ni el ratón."
check "El ratón también lo quita."
# -- antes de saltar, la pantalla del programa; luego, la lluvia --
v=$($PY verde $T/t1.ppm); set -- $v
if [ "$3" -lt 50 ]; then ok "a los 5 segundos, todavía no ha saltado"; else ko "ha saltado antes de tiempo ($3 puntos verdes)"; fi
parecida t0 t1 200 "y la pantalla sigue igual"
verde l1
verde l2
difiere l1 l2 3000 "la lluvia se mueve: dos capturas distintas"
n=$($PY columnas $T/l1.ppm)
if [ "$n" -ge 30 ]; then ok "lluvia en muchas columnas ($n de 80)"; else ko "pocas columnas con lluvia ($n)"; fi
igual_bin t0 l1 "mientras llueve, la memoria de texto del programa no se toca"
# -- la tecla --
igual_bin t0 t2 "tras la tecla, la memoria de texto es la de antes"
parecida t0 t2 200 "y la pantalla se ve igual que antes"
check "TRAS-LA-LLUVIA"
if grep -q "xECHO\|Comando o nombre de archivo incorrecto" $T/log.txt; then
  ko "la tecla que quita la lluvia ha llegado al intérprete"
else
  ok "la tecla que quita la lluvia no llega al intérprete"
fi
# -- /YA y el ratón --
verde y1
igual_bin y0 y2 "al mover el ratón, la pantalla de antes (texto)"
v=$($PY verde $T/y2.ppm); set -- $v
if [ "$3" -lt 50 ]; then ok "y la lluvia ya no se ve"; else ko "al mover el ratón sigue la lluvia ($3 puntos verdes)"; fi
v=$($PY verde $T/y3.ppm); set -- $v
if [ "$3" -lt 50 ]; then ok "y no vuelve a salir enseguida"; else ko "ha vuelto a salir"; fi
check "TRAS-EL-RATON"
# -- minutos --
check "PROTECTOR: ahora salta tras 2 minutos."
check "PROTECTOR: ahora salta tras 1 minuto."
check "PROTECTOR está en la memoria: salta tras 1 minuto sin tocar"
v=$($PY verde $T/m0.ppm); set -- $v
if [ "$3" -lt 50 ]; then ok "PROTECTOR 1: a los 50 segundos, todavía no"; else ko "PROTECTOR 1: ha saltado antes del minuto"; fi
verde m1
check "TRAS-EL-MINUTO"
# -- gráficos --
s=$($PY tam $T/g0.ppm)
v=$($PY verde $T/g0.ppm); set -- $v
if [ "$s" = "800 600" ] && [ "$3" -lt 2000 ]; then ok "dentro de Hispa Ventanas (gráficos) no salta"; else ko "en gráficos: $s, $3 puntos verdes"; fi
verde g1
check "DE-VUELTA"
# -- /NO --
check "PROTECTOR quitado de la memoria."
n=$(grep -c "PROTECTOR no está en la memoria" $T/log.txt)
if [ "$n" -ge 3 ]; then ok "tras /NO ya no está (ni /YA lo saca)"; else ko "tras /NO: «no está» solo $n veces"; fi
v=$($PY verde $T/n0.ppm); set -- $v
if [ "$3" -lt 50 ]; then ok "y no sale la lluvia"; else ko "tras /NO sale la lluvia"; fi
check "FIN-PROTECTOR"
muestra l1 lluvia_1; muestra l2 lluvia_2; muestra t0 antes; muestra t2 despues
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/Ejecutando/,$p' $T/log.txt; fi
[ -n "${GUARDAR:-}" ] && cp $T/*.ppm "$GUARDAR"/
rm -rf $T
exit $fail
