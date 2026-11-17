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
sleep 3
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.3}"; }
for ((i=0; i<${ESPERA:-240}; i++)); do
  mon "pmemsave 0xb8000 4000 \"$T/scr.bin\"" 0.3
  python3 -c "
import sys
d=open('$T/scr.bin','rb').read()
print('\n'.join(bytes(d[f*160:(f+1)*160:2]).decode('cp437').rstrip() for f in range(25)))" > $T/scr.txt
  grep -q "FIN-PRUEBA" $T/scr.txt && break
  sleep 1
done
mon quit 0.5; wait $QPID 2>/dev/null
grep -q "FIN-PRUEBA" $T/scr.txt || { echo "NO ACABÓ; la pantalla:"; cat $T/scr.txt; }
fallos=0
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
  if [ ! -f "$gw" ] && [ -x "$PCB" ]; then
    abs=$(realpath "$f"); (cd $T && timeout 30 $PCB -n "$abs" < /dev/null 2>&1) | \
      python3 -c "
import sys
t=sys.stdin.buffer.read().decode('latin-1').replace('\r','').split('\n')
while t and t[-1].strip(' \x00\x1a') in ('', 'Ok'): t.pop()
print('\n'.join(l.rstrip() for l in t))" > "$gw"
  fi
  if [ -f "$gw" ]; then
    if diff -q "$gw" $T/$n.mio > /dev/null; then echo "  OK   $n"; else echo "  FAIL $n"; diff "$gw" $T/$n.mio | head -${DIFF:-15}; fallos=1; fi
  else
    echo "  ---- $n (sin referencia):"; cat $T/$n.mio
  fi
  cp $T/$n.mio "${f%.bas}.mio" 2>/dev/null
done
rm -rf $T
exit $fallos
