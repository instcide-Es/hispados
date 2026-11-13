#!/bin/bash
# Prueba de VELOCIDAD (frenar el procesador) y LOADFIX (cargar por encima de
# los primeros 64 KB).
#
# La velocidad se mide con un programa aparte, VELTEST (bucle propio contra el
# reloj de la BIOS): VELOCIDAD 25% la deja en una cuarta parte, NORMAL la
# devuelve y quita el residente, «VELOCIDAD x programa» frena solo mientras
# corre el programa; los tics de la BIOS siguen a 18,2 Hz (contados contra el
# reloj de tiempo real); un programa que toma la INT 08h, reprogramando el
# temporizador o sin reprogramarlo, recibe sus interrupciones al ritmo que
# espera y sigue frenado; XT desactiva las cachés (CR0.CD) y las repone.
# LOADFIX: con un intérprete mínimo (PSPSH) los programas se cargan por
# debajo de 1000h; con LOADFIX, por encima.
#
# QEMU va con -icount (el tiempo virtual sale de las instrucciones
# ejecutadas): las medidas no dependen de lo cargado que esté el servidor.
# Con ICOUNT_OFF=1 se usa el reloj real (sale mucho más irregular).
# VELOCIDAD and LOADFIX test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=$(dirname "$IMG")
for p in VELTEST.COM PSPTEST.COM PSPSH.COM; do
  [ -f "$B/$p" ] || make -s B="$B" "$B/$p" >/dev/null 2>&1 || { echo "falta $B/$p (make nucprogs)"; exit 1; }
done
T=$(mktemp -d)
ICOUNT="-icount shift=6 -rtc clock=vm"
[ "${ICOUNT_OFF:-}" = 1 ] && ICOUNT=""
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$2" "$1"; then ok "$2"; else ko "$2"; fi; }

# lanza QEMU con una orden por línea; «@N» espera N segundos más. Entre
# órdenes se espera a que vuelva el símbolo del sistema (hasta 25 s).
run() {   # imagen registro  (órdenes por la entrada estándar)
  local img=$1 log=$2 c n i
  mkfifo $T/in
  timeout 300 qemu-system-i386 -display none -serial stdio -monitor none -parallel none $ICOUNT \
      -drive file=$img,format=raw,if=floppy -boot a -no-reboot < $T/in > $log.raw 2>&1 &
  local qp=$!
  exec 9> $T/in
  espera() {   # hasta que haya $1 símbolos del sistema en el registro
    for i in $(seq 1 250); do
      [ "$(tr -d '\r' < $log.raw | grep -c '^A:\\>')" -ge "$1" ] && return 0
      sleep 0.1
    done
    return 1
  }
  n=1
  espera $n
  while IFS= read -r c <&3; do
    case "$c" in
      @*) sleep "${c#@}"; continue;;
    esac
    printf '%s\r' "$c" >&9
    n=$((n + 1))
    espera $n || break
  done 3<&0
  sleep 0.5
  exec 9>&-
  kill $qp 2>/dev/null; wait $qp 2>/dev/null
  rm -f $T/in
  tr -d '\r' < $log.raw | iconv -f CP437 -t UTF-8 > $log
}
# la línea de VELTEST que sigue a la marca «ECHO =x=»: campo (VEL, TICS, ISR, V08, CD)
campo() {   # registro marca campo
  sed -n "/^=$2=\$/,/^=/p" "$1" | grep -m1 '^VEL=' | tr ' ' '\n' | sed -n "s/^$3=//p"
}
# ¿está a/b entre lo y hi por ciento?
entre() {   # a b lo hi texto
  local a=${1:-0} b=${2:-0}
  if [ "$b" -gt 0 ] && [ $((a * 100)) -ge $((b * $3)) ] && [ $((a * 100)) -le $((b * $4)) ]; then
    ok "$5 ($a de $b: $((a * 100 / b)) %)"
  else
    ko "$5 ($a de $b; tenía que estar entre el $3 y el $4 %)"
  fi
}
tics_bien() {   # valor texto
  if [ "${1:-0}" -ge 35 ] && [ "${1:-0}" -le 38 ]; then ok "$2 ($1 en 2 s)"; else ko "$2 (${1:-nada} en 2 s; tenían que ser 36 o 37)"; fi
}

echo "== Hispa-DOS VELOCIDAD y LOADFIX =="
cp "$IMG" $T/f.img
mcopy -o -i $T/f.img "$B/VELTEST.COM" "$B/PSPTEST.COM" ::

# ---- 1) VELOCIDAD: porcentaje, NORMAL, «x programa», ayuda y errores ----
run $T/f.img $T/l1 <<'EOF'
ECHO =a=
VELTEST T
ECHO =b=
VELOCIDAD 25%
VELTEST T
ECHO =c=
VELOCIDAD
ECHO =d=
VELOCIDAD NORMAL
VELTEST
ECHO =e=
VELOCIDAD 25% VELTEST T
ECHO =f=
VELTEST
ECHO =g=
VELOCIDAD 50% PSPTEST uno dos
IF ERRORLEVEL 5 ECHO nivel-5
ECHO =h=
VELOCIDAD
VELOCIDAD NORMAL
VELOCIDAD /?
AYUDA VELOCIDAD
VELOCIDAD 0%
VELOCIDAD 386 /X
VELOCIDAD 25% NOEXISTE
IF ERRORLEVEL 255 ECHO nivel-255
ECHO =i=
VELTEST
ECHO =j=
VELOCIDAD 10% /1
VELTEST T
ECHO =k=
VELOCIDAD
VELOCIDAD NORMAL
ECHO =fin=
EOF
L=$T/l1
v0=$(campo $L a VEL); vec0=$(campo $L a V08)
tics_bien "$(campo $L a TICS)" "sin freno: tics de la BIOS"
check $L "Velocidad: 25 % de la normal (medido: "
check $L "VELOCIDAD queda en la memoria. VELOCIDAD NORMAL lo quita."
entre "$(campo $L b VEL)" "$v0" 17 34 "VELOCIDAD 25%: una cuarta parte"
tics_bien "$(campo $L b TICS)" "con el freno, la INT 08h de siempre sigue a 18,2 Hz"
if [ -n "$vec0" ] && [ "$(campo $L b V08)" != "$vec0" ]; then ok "el residente tiene la INT 08h"; else ko "el residente tiene la INT 08h"; fi
if sed -n '/^=c=$/,/^=d=$/p' $L | grep -qF "Velocidad: 25 % de la normal" && sed -n '/^=c=$/,/^=d=$/p' $L | grep -qE "Medido ahora: (1[7-9]|2[0-9]|3[0-4]) % de la velocidad normal"; then ok "VELOCIDAD (sin nada) dice el nivel y lo que mide"; else ko "VELOCIDAD (sin nada) dice el nivel y lo que mide"; fi
check $L "Velocidad normal. VELOCIDAD se ha quitado de la memoria."
entre "$(campo $L d VEL)" "$v0" 85 118 "VELOCIDAD NORMAL: la de antes"
if [ -n "$vec0" ] && [ "$(campo $L d V08)" = "$vec0" ]; then ok "NORMAL devuelve la INT 08h ($vec0)"; else ko "NORMAL devuelve la INT 08h ($(campo $L d V08), era $vec0)"; fi
entre "$(campo $L e VEL)" "$v0" 17 34 "VELOCIDAD 25% VELTEST: frenado mientras corre"
tics_bien "$(campo $L e TICS)" "y los tics, a 18,2 Hz"
entre "$(campo $L f VEL)" "$v0" 85 118 "al volver el programa, la velocidad de antes"
if [ -n "$vec0" ] && [ "$(campo $L f V08)" = "$vec0" ]; then ok "y la INT 08h devuelta"; else ko "y la INT 08h devuelta ($(campo $L f V08))"; fi
check $L "ARGS=[ uno dos]"
check $L "nivel-5"
if sed -n '/^=h=$/,/^=i=$/p' $L | grep -qF "Velocidad normal (sin freno)."; then ok "tras «x programa» no queda residente"; else ko "tras «x programa» no queda residente"; fi
check $L "La velocidad ya es la normal."
n=$(grep -c "VELOCIDAD \[nivel \[/C\] \[/S\] \[/1\] \[programa \[argumentos\]\]\]" $L)
if [ "$n" -ge 2 ]; then ok "VELOCIDAD /? y AYUDA VELOCIDAD"; else ko "VELOCIDAD /? y AYUDA VELOCIDAD ($n)"; fi
check $L "VELOCIDAD: nivel no válido."
check $L "VELOCIDAD: opción no válida."
check $L "VELOCIDAD: no se encuentra el programa NOEXISTE"
check $L "nivel-255"
entre "$(campo $L i VEL)" "$v0" 85 118 "un programa que no existe no deja el freno puesto"
entre "$(campo $L j VEL)" "$v0" 5 18 "VELOCIDAD 10% /1 (una vez por tic): una décima parte"
tics_bien "$(campo $L j TICS)" "con /1, los tics a 18,2 Hz"
if sed -n '/^=k=$/,/^=fin=$/p' $L | grep -qF "Frena una vez por tic del reloj (18,2 Hz)."; then ok "y VELOCIDAD lo dice"; else ko "y VELOCIDAD dice que frena una vez por tic"; fi
[ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ] && cat $L

# ---- 2) procesadores con nombre, cachés, y programas que usan la INT 08h ----
f1=$fail
run $T/f.img $T/l2 <<'EOF'
ECHO =a=
VELTEST
VELOCIDAD
ECHO =b=
VELOCIDAD 386
VELOCIDAD
ECHO =c=
VELOCIDAD XT
VELTEST
ECHO =d=
VELOCIDAD NORMAL
VELTEST
ECHO =e=
VELOCIDAD XT /S VELTEST
ECHO =f=
VELOCIDAD 25%
VELTEST H
ECHO =g=
VELOCIDAD
VELTEST T
ECHO =h=
VELOCIDAD 25%
VELTEST R
ECHO =i=
VELTEST T
ECHO =j=
VELOCIDAD 25% VELTEST R
ECHO =k=
VELTEST T
ECHO =l=
VELOCIDAD 50%
VELOCIDAD 25% VELTEST
ECHO =m=
VELTEST
VELOCIDAD
ECHO =n=
VELOCIDAD NORMAL
VELTEST
ECHO =fin=
EOF
L=$T/l2
v0=$(campo $L a VEL); vec0=$(campo $L a V08)
ind() { sed -n "/^=$1=\$/,/^=$2=\$/p" $L | sed -n 's/^Velocidad medida (índice): //p' | tr -d . | tail -1; }
i0=$(ind a b)
if [ "${i0:-0}" -gt 0 ]; then ok "índice de velocidad sin freno: $i0"; else ko "índice de velocidad sin freno"; fi
check $L "Velocidad: como un 386 a 33 MHz (medido: "
if [ "${i0:-0}" -gt 4200 ]; then
  entre "$(ind b c)" 3200 75 130 "VELOCIDAD 386: índice cerca de 3.200"
else
  echo "  --   este QEMU no es más rápido que un 386 (índice $i0): no se comprueba el nivel 386"
fi
check $L "Velocidad: como un XT (8088 a 4,77 MHz) (medido: "
check $L "Las cachés del procesador están desactivadas."
if [ "$(campo $L c CD)" = 1 ]; then ok "VELOCIDAD XT: CR0.CD = 1"; else ko "VELOCIDAD XT: CR0.CD = 1 ($(campo $L c CD))"; fi
entre "$(campo $L c VEL)" "$v0" 0 12 "VELOCIDAD XT: muy lento"
if [ "$(campo $L d CD)" = 0 ]; then ok "NORMAL: CR0.CD = 0"; else ko "NORMAL: CR0.CD = 0"; fi
entre "$(campo $L d VEL)" "$v0" 85 118 "NORMAL tras XT: la de antes"
if [ "$(campo $L e CD)" = 0 ]; then ok "XT /S no toca las cachés"; else ko "XT /S no toca las cachés"; fi
# un programa que toma la INT 08h sin reprogramar el temporizador
tics_bien "$(campo $L f ISR)" "programa con la INT 08h (sin reprogramar): la recibe a 18,2 Hz"
tics_bien "$(campo $L f TICS)" "y los tics de la BIOS, a 18,2 Hz"
entre "$(campo $L f VEL)" "$v0" 15 38 "y sigue frenado al 25 %"
if sed -n '/^=g=$/,/^=h=$/p' $L | grep -qF "Otro programa usa el temporizador: frena una vez por tic (18,2 Hz)."; then ok "VELOCIDAD dice que ha pasado a frenar una vez por tic"; else ko "VELOCIDAD dice que ha pasado a frenar una vez por tic"; fi
entre "$(campo $L g VEL)" "$v0" 15 38 "después sigue al 25 %"
tics_bien "$(campo $L g TICS)" "y los tics, a 18,2 Hz"
# un programa que reprograma el temporizador a 200 Hz y toma la INT 08h
isr=$(campo $L h ISR)
if [ "${isr:-0}" -ge 380 ] && [ "${isr:-0}" -le 420 ]; then ok "programa con el temporizador a 200 Hz: recibe sus 200 Hz ($isr en 2 s)"; else ko "programa con el temporizador a 200 Hz ($isr en 2 s; tenían que ser unas 400)"; fi
tics_bien "$(campo $L h TICS)" "y los tics de la BIOS, a 18,2 Hz"
entre "$(campo $L h VEL)" "$v0" 15 38 "y sigue frenado al 25 %"
entre "$(campo $L i VEL)" "$v0" 15 38 "después sigue al 25 %"
tics_bien "$(campo $L i TICS)" "y los tics, a 18,2 Hz"
isr=$(campo $L j ISR)
if [ "${isr:-0}" -ge 380 ] && [ "${isr:-0}" -le 420 ]; then ok "VELOCIDAD 25% con ese programa: sus 200 Hz ($isr en 2 s)"; else ko "VELOCIDAD 25% con ese programa ($isr en 2 s)"; fi
entre "$(campo $L j VEL)" "$v0" 15 38 "frenado al 25 %"
entre "$(campo $L k VEL)" "$v0" 15 38 "al volver, el 25 % que había"
tics_bien "$(campo $L k TICS)" "y los tics, a 18,2 Hz"
# «x programa» con un residente ya puesto: al volver, el nivel de antes
entre "$(campo $L l VEL)" "$v0" 17 34 "con el 50 % puesto, VELOCIDAD 25% VELTEST va al 25 %"
entre "$(campo $L m VEL)" "$v0" 40 60 "y al volver, otra vez al 50 %"
if sed -n '/^=m=$/,/^=n=$/p' $L | grep -qF "Velocidad: 50 % de la normal"; then ok "VELOCIDAD dice 50 %"; else ko "VELOCIDAD dice 50 %"; fi
entre "$(campo $L n VEL)" "$v0" 85 118 "NORMAL al final: la de antes"
if [ -n "$vec0" ] && [ "$(campo $L n V08)" = "$vec0" ]; then ok "y la INT 08h devuelta"; else ko "y la INT 08h devuelta ($(campo $L n V08))"; fi
[ $fail -ne $f1 ] || [ -n "${VERBOSE:-}" ] && cat $L

# ---- 3) LOADFIX ----
f1=$fail
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDOS=HIGH\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
run $T/f.img $T/l3 <<'EOF'
ECHO =a=
PSPTEST
LOADFIX PSPTEST uno dos
IF ERRORLEVEL 5 ECHO nivel-5
LOADFIX HELLOEXE
LOADFIX BAT\PRUEBA.BAT hola
LOADFIX NOEXISTE
IF ERRORLEVEL 255 ECHO nivel-255
LOADFIX /?
AYUDA LOADFIX
VELOCIDAD 50% LOADFIX PSPTEST tres
ECHO =fin=
EOF
L=$T/l3
check $L "ARGS=[ uno dos]"
check $L "nivel-5"
check $L "Hello from a relocated .EXE program!"
check $L "Param 1=hola 2="
check $L "LOADFIX: no se encuentra el programa NOEXISTE"
check $L "nivel-255"
n=$(grep -c "LOADFIX \[unidad:\]\[ruta\]programa \[argumentos\]" $L)
if [ "$n" -ge 2 ]; then ok "LOADFIX /? y AYUDA LOADFIX"; else ko "LOADFIX /? y AYUDA LOADFIX ($n)"; fi
check $L "ARGS=[ tres]"
psps=$(grep -o 'PSP=[0-9A-F]*' $L | cut -d= -f2 | tr '\n' ' ')
# el primero va sin LOADFIX (con COMMAND.COM partido cae por debajo de
# 1000h); los dos siguientes, con LOADFIX, por encima
bad=0; i=0; for p in $psps; do
  if [ $i = 0 ]; then [ $((16#$p)) -lt $((16#1000)) ] || bad=1
  else [ $((16#$p)) -ge $((16#1000)) ] || bad=1; fi
  i=$((i+1)); done
if [ $i = 3 ] && [ $bad = 0 ]; then ok "con LOADFIX desde COMMAND.COM, por encima de 1000h ($psps)"; else ko "con LOADFIX desde COMMAND.COM ($psps)"; fi
[ $fail -ne $f1 ] || [ -n "${VERBOSE:-}" ] && cat $L
# con un intérprete mínimo, los programas caen en los primeros 64 KB
f1=$fail
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDOS=HIGH\r\nSHELL=A:\\PSPSH.COM\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
mcopy -o -i $T/f.img "$B/PSPSH.COM" ::
timeout 60 qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
    -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot < /dev/null > $T/l4.raw 2>&1 &
QP=$!
for i in $(seq 1 300); do grep -q "PSPSH: fin" $T/l4.raw 2>/dev/null && break; sleep 0.1; done
kill $QP 2>/dev/null; wait $QP 2>/dev/null
tr -d '\r' < $T/l4.raw | iconv -f CP437 -t UTF-8 > $T/l4
L=$T/l4
check $L "PSPSH: sin LOADFIX y con LOADFIX"
p1=$(grep -m1 'ARGS=\[ directo\]' $L | sed 's/^PSP=\([0-9A-F]*\).*/\1/')
p2=$(grep -m1 'con relleno\]' $L | sed 's/^PSP=\([0-9A-F]*\).*/\1/')
p3=$(grep 'ARGS=\[ directo\]' $L | sed -n '2s/^PSP=\([0-9A-F]*\).*/\1/p')
if [ -n "$p1" ] && [ $((16#$p1)) -lt $((16#1000)) ]; then ok "sin LOADFIX el programa se carga en los primeros 64 KB (PSP=$p1)"; else ko "sin LOADFIX el programa tenía que cargarse por debajo de 1000h (PSP=$p1)"; fi
if [ -n "$p2" ] && [ $((16#$p2)) -ge $((16#1000)) ] && [ $((16#$p2)) -le $((16#1010)) ]; then ok "con LOADFIX, justo por encima de 1000h (PSP=$p2)"; else ko "con LOADFIX tenía que cargarse justo por encima de 1000h (PSP=$p2)"; fi
check $L "RET=0005"
if [ -n "$p3" ] && [ "$p3" = "$p1" ]; then ok "al volver, el relleno se ha liberado (PSP=$p3 otra vez)"; else ko "al volver, el relleno se ha liberado (PSP=$p3, era $p1)"; fi
check $L "PSPSH: fin"
[ $fail -ne $f1 ] || [ -n "${VERBOSE:-}" ] && cat $L

rm -rf $T
exit $fail
