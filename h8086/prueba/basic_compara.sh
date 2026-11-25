#!/bin/bash
# Prueba de Hispa-BASIC: ejecuta unos programas .BAS en Hispa-8086 (QEMU) y
# compara lo que escriben con lo que escribe GW-BASIC (PC-BASIC, si está).
#   prueba/basic_compara.sh prueba/basic/*.bas
# Cada programa se ejecuta con «BASIC PROG.BAS < FIN.TXT > PROG.SAL» (FIN.TXT
# lleva SYSTEM). Lo que sale de PC-BASIC se guarda en prueba/basic/*.gw
# (para no necesitarlo siempre). Copyright (C) 2026 Carlos Sanz y
# contribuidores. GPL-2.0 o posterior.
set -u
cd "$(dirname "$0")/.."
T=$(mktemp -d)
PCB=${PCBASIC:-/root/.claude/jobs/basic-ref/venv/bin/pcbasic}
cp build/hispa8086-1440.img $T/a.img
mcopy -o -i $T/a.img build/BASIC.COM ::BASIC.COM
printf 'SYSTEM\r\n' > $T/FIN.TXT
mcopy -o -i $T/a.img $T/FIN.TXT ::FIN.TXT
{
  printf '@ECO OFF\r\n'
  for f in "$@"; do
    n=$(basename "$f" .bas | tr a-z A-Z)
    sed 's/$/\r/' "$f" > $T/$n.BAS
    mcopy -o -i $T/a.img $T/$n.BAS ::$n.BAS
    printf 'BASIC %s.BAS < FIN.TXT > %s.SAL\r\n' "$n" "$n"
  done
  printf 'ECO FIN-PRUEBA\r\n'
} > $T/AUTOEXEC.BAT
mcopy -o -i $T/a.img $T/AUTOEXEC.BAT ::AUTOEXEC.BAT
rm -f $T/mon
qemu-system-i386 -display none -monitor unix:$T/mon,server,nowait -cpu ${CPU:-486} -m 4 \
  -drive file=$T/a.img,format=raw,if=floppy,index=0 -boot a -no-reboot > /dev/null 2>&1 &
QPID=$!
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.3}"; }
# (que conteste el monitor de QEMU: hasta 20 s; si no, se dice una vez y se
# acaba, en vez de 240 lecturas fallidas de la pantalla)
for ((i=0; i<40; i++)); do
  printf 'info status\n' | socat - UNIX-CONNECT:$T/mon 2> /dev/null | grep -q "VM status" && break
  kill -0 $QPID 2> /dev/null || i=40
  sleep 0.5
done
if [ $i -ge 40 ]; then echo "  FAIL QEMU no arranca (o su monitor no contesta): la prueba de BASIC se salta"; kill $QPID 2> /dev/null; rm -rf $T; exit 1; fi
sleep 2
for ((i=0; i<${ESPERA:-240}; i++)); do
  rm -f $T/scr.bin
  mon "pmemsave 0xb8000 4000 \"$T/scr.bin\"" 0.3
  python3 -c "
import os
d=open('$T/scr.bin','rb').read() if os.path.exists('$T/scr.bin') else b''
print('\n'.join(bytes(d[f*160:(f+1)*160:2]).decode('cp437').rstrip() for f in range(25)))" > $T/scr.txt
  grep -q "FIN-PRUEBA" $T/scr.txt && break
  sleep 1
done
mon quit 0.5; wait $QPID 2>/dev/null
grep -q "FIN-PRUEBA" $T/scr.txt || { echo "NO ACABÓ; la pantalla:"; cat $T/scr.txt; }
fallos=0
[ -x "$PCB" ] || echo "  (no hay PC-BASIC en $PCB: se compara con los .gw y .esperado guardados; PCBASIC=ruta lo indica)"
for f in "$@"; do
  n=$(basename "$f" .bas | tr a-z A-Z)
  mcopy -o -i $T/a.img ::$n.SAL $T/$n.SAL 2>/dev/null || { echo "  FAIL $n: no hay salida"; fallos=1; continue; }
  # lo nuestro, sin el saludo (hasta «octetos libres») ni el «Ok» del final
  python3 - $T/$n.SAL > $T/$n.mio <<'PY'
import sys
t=open(sys.argv[1],'rb').read().decode('cp437').replace('\r','').split('\n')
i=next((k for k,l in enumerate(t) if 'octetos libres' in l), -1)
t=t[i+1:]
while t and t[-1] in ('', 'Ok'): t.pop()
print('\n'.join(l.rstrip() for l in t))
PY
  gw="${f%.bas}.gw"
  # lo que no tiene GW-BASIC (las palabras en español...): lo esperado, a mano
  [ -f "${f%.bas}.esperado" ] && gw="${f%.bas}.esperado"
  if [ ! -f "$gw" ] && [ -x "$PCB" ]; then
    abs=$(realpath "$f"); (cd $T && timeout 30 $PCB -n "$abs" < /dev/null 2>&1) | \
      python3 -c "
import sys
t=sys.stdin.buffer.read().decode('latin-1').replace('\r','').split('\n')
o=[]
for l in t:                         # (hasta el primer «Ok»: detrás, PC-BASIC lee basura)
    if l.strip(' \x00\x1a\xff').startswith('Ok'): break
    o.append(l.rstrip())
while o and not o[-1]: o.pop()
print('\n'.join(o))" > "$gw"
  fi
  # los mensajes de GW-BASIC, en el español de Hispa-BASIC (docs/8086/BASIC.md)
  [ -f "$gw" ] && [ "${gw%.esperado}" = "$gw" ] && sed -e 's/^Overflow$/Desbordamiento/' -e 's/^Division by zero$/División por cero/' \
    -e 's/^Syntax error in \([0-9]*\)$/Error de sintaxis en \1/' -e 's/^Out of DATA in \([0-9]*\)$/No quedan DATOS en \1/' \
    -e 's/^Type mismatch in \([0-9]*\)$/Tipos que no casan en \1/' "$gw" > "$gw.es"
  [ -f "$gw.es" ] && [ "${gw%.esperado}" = "$gw" ] && gw="$gw.es"
  if [ -f "$gw" ]; then
    if diff -q "$gw" $T/$n.mio > /dev/null; then echo "  OK   $n"; else echo "  FAIL $n"; diff "$gw" $T/$n.mio | head -${DIFF:-15}; fallos=1; fi
  else
    echo "  ---- $n (sin referencia):"; cat $T/$n.mio
  fi
  cp $T/$n.mio "${f%.bas}.mio" 2>/dev/null
done
rm -rf $T
exit $fallos
