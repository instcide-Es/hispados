#!/bin/bash
# [SB-EOI] El teclado mientras suena la Sound Blaster emulada (HSPSND /SB):
# KBSB.COM (tests/progs/kbsb.asm) hace como un juego (su INT 9 sin la BIOS,
# la IRQ 5 de la SB en autoinicio con bloques cortos, la FM sonando y el
# reloj enganchado) y cuenta las pulsaciones y sueltas que le llegan. Por
# QMP se pulsan y sueltan teclas (letras y flechas, con códigos E0) a ritmo
# de juego. No se puede perder ninguna: si se pierde un código de soltar,
# en el juego la tecla «se queda pulsada» (Prince of Persia se atascaba).
# Con intel-hda (sin sb16), con AC'97, con LH HSPSND /SB, con una sb16 de
# verdad (referencia) y sin sonido.
# Keyboard while the emulated Sound Blaster plays (lost key releases).
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=$(dirname "$IMG")
[ -f $B/KBSB.COM ] || { echo "falta $B/KBSB.COM (make emmprogs)"; exit 1; }
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$1" $T/log.txt; then ok "$1"; else ko "$1"; fi; }
checkn() { if grep -qF -- "$1" $T/log.txt; then ko "(no debía salir) $1"; else ok "sin \"$1\""; fi; }
N=60          # pulsaciones de cada tecla
# run "CONFIG.SYS" "DISPOSITIVOS" "carga" "orden"
run() {
  local cfg=$1 dev=$2 load=$3 cmd=$4
  T=$(mktemp -d)
  cp "$IMG" $T/f.img
  printf "$cfg" > $T/CONFIG.SYS
  mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
  mcopy -o -i $T/f.img $B/KBSB.COM ::KBSB.COM
  mcopy -o -i $T/f.img $B/SINIRQ.COM ::SINIRQ.COM
  { sleep 6; IFS='#'; for l in $load; do printf '%s\r' "$l"; sleep 3; done; unset IFS; sleep 1; printf '%s\r' "$cmd"; sleep 34; } | \
    timeout 60 qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
        -m 32 -net none -audiodev none,id=snd0 $dev -qmp unix:$T/qmp,server,nowait \
        -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a -no-reboot > $T/raw 2>&1 &
  local q=$!
  python3 - $T/qmp $T/raw $N <<'EOF'
import json, socket, sys, time
sock, raw, n = sys.argv[1], sys.argv[2], int(sys.argv[3])
t0 = time.time()
while time.time() - t0 < 30:          # esperar a que KBSB esté listo
    try:
        if b'KBSB: listo' in open(raw, 'rb').read():
            break
    except OSError:
        pass
    time.sleep(0.2)
else:
    print('  (KBSB no ha arrancado)'); sys.exit(0)
time.sleep(0.5)
s = socket.socket(socket.AF_UNIX); s.connect(sock); f = s.makefile('rw'); f.readline()
def cmd(c, a=None):
    m = {'execute': c}
    if a is not None: m['arguments'] = a
    f.write(json.dumps(m) + '\n'); f.flush()
    while True:
        r = json.loads(f.readline())
        if 'return' in r or 'error' in r: return r
def key(k, down):
    cmd('input-send-event', {'events': [{'type': 'key', 'data': {'down': down, 'key': {'type': 'qcode', 'data': k}}}]})
cmd('qmp_capabilities')
# a ritmo de juego: pulsar, mantener un poco, soltar; a veces dos a la vez
for i in range(n):
    key('right', True); time.sleep(0.03)
    key('a', True); time.sleep(0.02)
    key('right', False); time.sleep(0.01)
    key('a', False); time.sleep(0.04)
EOF
  wait $q
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 > $T/log.txt
}
fin() {
  if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi
  rm -rf $T
}
# como Prince of Persia: el bit de ocupado parpadea y la IRQ de un bloque de
# 0,1 s llega a su hora (1-5 tics) sin tocar los puertos
comopop() {
  check "KBSB: el bit de ocupado del DSP parpadea"
  if grep -q "IRQ del bloque único a los [1-5] tics" $T/log.txt; then ok "la IRQ del bloque único llega a su hora"; else ko "la IRQ del bloque único llega a su hora"; fi
}
cuenta() {
  check "KBSB: pulsadas $((2*N)), soltadas $((2*N)), E0 $((2*N))"
  check "KBSB: teclas que quedan pulsadas: 0"
}
EMM='DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDEVICE=A:\\HISPADOS\\HSPEMM.SYS\r\nDOS=UMB\r\n'
EMMNOEMS='DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDEVICE=A:\\HISPADOS\\HSPEMM.SYS NOEMS\r\nDOS=UMB\r\n'
HDA="-device intel-hda -device hda-duplex,audiodev=snd0"

echo "== Hispa-DOS: teclado y Sound Blaster emulada (HD Audio) =="
run "$EMM" "$HDA" 'A:\HISPADOS\HSPSND /SB' 'A:\KBSB'
check "Sound Blaster 16 emulada en 220h"
checkn "el DSP no responde"
comopop
cuenta
if grep -q "IRQ 5 [0-9]\{4,\}" $T/log.txt; then ok "llegan las IRQ 5"; else ko "llegan las IRQ 5"; fi
fin

# sin IRQ de la tarjeta (como el ThinkPad A275: la BIOS no se la da al HD
# Audio): la IRQ emulada solo se entrega con el reloj y en los puertos
echo "== Hispa-DOS: teclado y Sound Blaster emulada (HD Audio sin IRQ, como el A275) =="
run "$EMMNOEMS" "$HDA" 'A:\SINIRQ#LH A:\HISPADOS\HSPSND /SB' 'A:\KBSB'
check "SINIRQ: tarjetas de sonido sin IRQ"
check "Sound Blaster 16 emulada en 220h"
comopop
cuenta
if grep -q "IRQ 5 [0-9]\{4,\}" $T/log.txt; then ok "llegan las IRQ 5"; else ko "llegan las IRQ 5"; fi
fin

echo "== Hispa-DOS: teclado y Sound Blaster emulada (AC'97) =="
run "$EMM" "-device AC97,audiodev=snd0" 'A:\HISPADOS\HSPSND /SB' 'A:\KBSB'
check "Sound Blaster 16 emulada en 220h"
comopop
cuenta
if grep -q "IRQ 5 [0-9]\{4,\}" $T/log.txt; then ok "llegan las IRQ 5"; else ko "llegan las IRQ 5"; fi
fin

echo "== Hispa-DOS: teclado y Sound Blaster emulada (LH HSPSND /SB, como «Juegos») =="
run "$EMMNOEMS" "$HDA" 'LH A:\HISPADOS\HSPSND /SB' 'A:\KBSB'
check "Sound Blaster 16 emulada en 220h"
comopop
cuenta
if grep -q "IRQ 5 [0-9]\{4,\}" $T/log.txt; then ok "llegan las IRQ 5"; else ko "llegan las IRQ 5"; fi
fin

echo "== Hispa-DOS: teclado y una Sound Blaster 16 de verdad (referencia) =="
run "$EMMNOEMS" "-device sb16,audiodev=snd0 -device adlib,audiodev=snd0" 'A:\HISPADOS\HSPSND' 'A:\KBSB'
# (la sb16 de QEMU no hace parpadear el bit de ocupado: solo la IRQ)
if grep -q "IRQ del bloque único a los [1-5] tics" $T/log.txt; then ok "la IRQ del bloque único llega a su hora"; else ko "la IRQ del bloque único llega a su hora"; fi
cuenta
fin

echo "== Hispa-DOS: teclado sin sonido (HSPEMM) =="
run "$EMMNOEMS" "" 'REM' 'A:\KBSB N'
cuenta
fin
exit $fail
