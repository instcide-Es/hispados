#!/bin/bash
# [REINICIO-SILENCIO] Ctrl+Alt+Supr, REINICIAR y APAGAR callan el sonido antes
# de que la BIOS arranque otra vez (lib/silencio.inc). En los portátiles de
# Carlos (HP Mini 5103 con IDT, Toshiba NB250 con Realtek ALC269) el
# reinicio en caliente solo reinicia el procesador: la tarjeta HD Audio
# seguía encendida (EAPD, pines con salida, amplificadores a 0 dB, el flujo
# de HSPSND leyendo un búfer que la BIOS machaca) y el altavoz del PC, que
# suena por el códec, con la puerta abierta: «lluvia de interferencias» que
# se va apagando mientras arranca la BIOS.
#  QEMU sí reinicia todo (no sale el ruido), así que se mira el estado justo
# cuando se pide el reinicio: con -action reboot=shutdown,shutdown=pause la
# máquina se para en ese instante y se lee el puerto 61h (bits 0 y 1 a 0: la
# puerta del altavoz cerrada) y lo que le ha llegado al códec (hda-output
# con debug=3 escribe cada orden; la traza hda_audio_running, el flujo):
#   - el flujo parado («run 0» lo último), los amplificadores en silencio
#     (300h B080h), el pin sin salida (707h 0) y el grupo en D3 (705h 3);
#  en estos casos:
#   - Hispa-DOS con HSPSND tocando (PLAYWAV) y Ctrl+Alt+Supr;
#   - sin HSPSND, con la tarjeta como la deja la BIOS de los portátiles
#     (HDAVIVO.COM: fuera de reinicio, pin con salida, EAPD) y el altavoz del
#     PC sonando (PITIDO.COM): lo hace el núcleo solo; también con DOS=HIGH
#     (el código en la HMA);
#   - con HSPEMM (modo V86) y HSPSND /SB (el flujo de la Sound Blaster
#     emulada siempre en marcha): Ctrl+Alt+Supr y REINICIAR (lo hace la
#     función 10h de HSPSND, la única que llega en modo V86);
#   - APAGAR (se para al apagarse);
#   - Hispa-8086: SOUND de Hispa-BASIC sonando, con HDAVIVO, y Ctrl+Alt+Supr
#     (solo la puerta del altavoz: un 8086 no tiene tarjeta de sonido)
#     (con el núcleo abajo y en la memoria alta, DOS=ALTO);
#  y con el reinicio de verdad (sin parar): el altavoz y la tarjeta suenan
#  antes (grabación -audiodev wav), el equipo vuelve a arrancar y después
#  ya no suena nada.
# Ctrl+Alt+Del, REINICIAR and APAGAR silence the sound before the BIOS restarts.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=$(dirname "$IMG")
H86=${H86:-h8086/build/hispa8086-1440.img}
for f in $B/PITIDO.COM $B/HDAVIVO.COM $H86; do
  [ -f $f ] || { echo "falta $f (make para-pruebas; cd h8086 && make)"; exit 1; }
done
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
HDA="-device intel-hda -device hda-output,audiodev=snd0,debug=3"
EMMCFG='DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDEVICE=A:\\HISPADOS\\HSPEMM.SYS NOEMS\r\nDOS=UMB\r\n'
HIGHCFG='DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDOS=HIGH\r\n'
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.12}"; }
# lo que ha quedado: VM parada, puerto 61h y lo que le ha llegado al códec
estado() {
  printf 'info status\ni /b 0x61\n' | socat - UNIX-CONNECT:$T/mon 2>/dev/null | tr -d '\r' > $T/mon.txt
  grep -v '^audio:' $T/err > $T/err.txt 2>/dev/null
}
mira() {   # $1 = flujo (1 si estaba sonando), $2 = parada (1: se paró al reiniciar), $3 = 0: sin mirar el códec
  if [ "${2:-1}" = 1 ]; then
    if grep -qa "VM status: paused (shutdown)" $T/mon.txt; then ok "parado en el reinicio"; else ko "no se ha reiniciado (o no se ha parado)"; fi
  fi
  local p; p=$(grep -ao 'portb\[0x0061\] = 0x[0-9a-f]*' $T/mon.txt | sed 's/.*= //')
  if [ -n "$p" ] && [ $(( p & 3 )) -eq 0 ]; then ok "altavoz del PC: puerta cerrada (61h = $p)"; else ko "altavoz del PC: 61h = ${p:-?}"; fi
  if [ "$1" = 1 ]; then
    if grep -a hda_audio_running $T/err.txt | tail -1 | grep -q "run 0"; then ok "el flujo de HD Audio, parado"; else ko "el flujo sigue en marcha"; fi
  fi
  [ "${3:-1}" = 0 ] && return
  local w
  for w in "verb 0x300, payload 0xb080|amplificadores en silencio" "nid 3 (out), verb 0x707, payload 0x0|el pin sin salida" \
           "nid 3 (out), verb 0x70c, payload 0x0|EAPD apagado" "nid 1 (func), verb 0x705, payload 0x3|el códec en D3"; do
    if grep -qaF -- "${w%%|*}" $T/err.txt; then ok "${w#*|}"; else ko "${w#*|} (falta «${w%%|*}»)"; fi
  done
}
# dos "CONFIG.SYS" "órdenes (#)" cad(1/0) [reset]
dos() {
  local cfg=$1 cmds=$2 cad=$3 reb=${4:-shutdown}
  T=$(mktemp -d)
  cp "$IMG" $T/f.img
  [ -n "$cfg" ] && { printf "$cfg" > $T/CONFIG.SYS; mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS; }
  mcopy -o -i $T/f.img $B/PITIDO.COM $B/HDAVIVO.COM ::
  qemu-system-i386 -display none -serial unix:$T/serie,server,nowait -monitor unix:$T/mon,server,nowait \
      -m 32 -net none -audiodev wav,id=snd0,path=$T/out.wav -machine pcspk-audiodev=snd0 $HDA \
      -action reboot=$reb -action shutdown=pause -trace hda_audio_running \
      -drive file=$T/f.img,format=raw,if=floppy -boot a > /dev/null 2> $T/err &
  Q=$!
  sleep 0.5
  { sleep 5; IFS='#'; for l in $cmds; do printf '%s\r' "$l"; sleep 3; done; unset IFS
    [ "$cad" = 1 ] && mon "sendkey ctrl-alt-delete"
    sleep ${ESPERA:-4}; [ "$reb" = reset ] && { printf 'VER\r'; sleep 2; }; } | \
    timeout 60 socat - UNIX-CONNECT:$T/serie > $T/log 2>/dev/null
  estado
  mon quit 0.5; wait $Q 2>/dev/null
  tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 > $T/log.txt 2>/dev/null
}
# h86 imagen "líneas tecleadas (#)" [reset]
h86() {
  local img=$1 txt=$2 reb=${3:-shutdown} l i c k
  T=$(mktemp -d)
  cp "$img" $T/a.img
  mcopy -o -i $T/a.img $B/HDAVIVO.COM ::
  qemu-system-i386 -display none -monitor unix:$T/mon,server,nowait -cpu 486 -m 4 \
      -audiodev wav,id=snd0,path=$T/out.wav -machine pcspk-audiodev=snd0 $HDA \
      -action reboot=$reb -action shutdown=pause \
      -drive file=$T/a.img,format=raw,if=floppy,index=0 -boot a > /dev/null 2> $T/err &
  Q=$!
  sleep 6
  IFS='#'; for l in $txt; do
    for ((i=0; i<${#l}; i++)); do
      c="${l:$i:1}"
      case "$c" in ' ') k=spc;; ',') k=comma;; [A-Z]) k="shift-${c,,}";; *) k="$c";; esac
      mon "sendkey $k"
    done
    mon "sendkey ret" 1.5
  done; unset IFS
  sleep 2
  mon "sendkey ctrl-alt-delete" ${ESPERA:-4}
  estado
  mon "pmemsave 0xb8000 4000 \"$T/scr.bin\"" 0.5
  mon quit 0.5; wait $Q 2>/dev/null
  python3 - $T/scr.bin > $T/scr.txt <<'PY'
import sys
d = open(sys.argv[1], 'rb').read()
for f in range(25):
    print(bytes(d[f * 160:(f + 1) * 160:2]).decode('cp437').rstrip())
PY
}
# grabación: $1 Hz sonando antes del reinicio y ninguno después
grabado() {
  python3 tests/tools/audio.py check $T/out.wav > $T/audio.txt 2>&1
  python3 - $T/audio.txt $1 <<'EOF' || fail=1
import re, sys
txt = open(sys.argv[1]).read(); hz = float(sys.argv[2])
tr = [(float(a), float(b), float(h)) for a, b, h in re.findall(r'tramo ([\d.]+)-([\d.]+) s: (\d+) Hz', txt)]
m = [t for t in tr if abs(t[2] - hz) / hz < 0.05]
if m and m[0][1] - m[0][0] > 1:
    print("  OK   antes del reinicio suena %d Hz (%.1f s)" % (hz, m[0][1] - m[0][0]))
else:
    print("  FAIL antes del reinicio no suena %d Hz (tramos: %s)" % (hz, tr)); sys.exit(1)
despues = [t for t in tr if t[0] > m[0][1] + 0.05]
if despues:
    print("  FAIL después del reinicio suena algo: %s" % despues); sys.exit(1)
print("  OK   después del reinicio no suena nada")
EOF
}

echo "== Hispa-DOS: HSPSND tocando (HD Audio) y Ctrl+Alt+Supr =="
dos "" 'A:\HISPADOS\HSPSND#PLAYWAV /TONE 440 20000' 1
grep -qF "Tono 440 Hz en Intel HD Audio" $T/log.txt && ok "PLAYWAV sonando" || ko "PLAYWAV no suena"
mira 1
[ $fail -ne 0 ] && { cat $T/log.txt; tail -30 $T/err.txt; }
rm -rf $T

echo "== Hispa-DOS sin HSPSND: la tarjeta como la deja la BIOS y el altavoz sonando =="
dos "" 'A:\HDAVIVO#A:\PITIDO' 1
grep -qF "HDAVIVO: lista" $T/log.txt && ok "HDAVIVO: la tarjeta, encendida" || ko "HDAVIVO no ha podido"
grep -qF "PITIDO: sonando" $T/log.txt && ok "PITIDO: el altavoz, sonando" || ko "PITIDO no ha sonado"
mira 0
rm -rf $T

echo "== Hispa-DOS con DOS=HIGH (el núcleo en la HMA): lo mismo =="
dos "$HIGHCFG" 'MEM#A:\HDAVIVO#A:\PITIDO' 1
grep -qF "Hispa-DOS está cargado en la memoria alta (HMA)" $T/log.txt && ok "MEM: el núcleo en la HMA" || ko "MEM: el núcleo no está en la HMA"
mira 0
rm -rf $T

echo "== Hispa-DOS con HSPEMM (modo V86) y HSPSND /SB: Ctrl+Alt+Supr =="
dos "$EMMCFG" 'A:\HISPADOS\HSPSND /SB#A:\PITIDO' 1
grep -qF "Sound Blaster 16 emulada" $T/log.txt && ok "la Sound Blaster emulada, en marcha" || ko "sin la Sound Blaster emulada"
mira 1
rm -rf $T

echo "== Hispa-DOS con HSPEMM y HSPSND /SB: REINICIAR =="
ESPERA=7 dos "$EMMCFG" 'A:\HISPADOS\HSPSND /SB#A:\PITIDO#A:\HISPADOS\REINICIA' 0
grep -qF "Reiniciando el equipo..." $T/log.txt && ok "REINICIAR" || ko "REINICIAR no ha empezado"
mira 1
rm -rf $T

echo "== Hispa-DOS: APAGAR con HSPSND y el altavoz sonando =="
ESPERA=8 dos "" 'A:\HISPADOS\HSPSND#A:\PITIDO#A:\HISPADOS\APAGAR' 0
if grep -qa "VM status: paused (shutdown)" $T/mon.txt; then ok "apagado"; else ko "no se ha apagado"; fi
mira 0 0
rm -rf $T

echo "== Hispa-DOS: el reinicio de verdad, con el altavoz grabado =="
dos "" 'A:\PITIDO' 1 reset
[ $(grep -c "AUTOEXEC" $T/log.txt) -ge 2 ] && ok "vuelve a arrancar" || ko "no vuelve a arrancar"
grabado 880
rm -rf $T

echo "== Hispa-DOS: el reinicio de verdad con HSPSND tocando, grabado =="
dos "" 'A:\HISPADOS\HSPSND#PLAYWAV /TONE 440 20000' 1 reset
[ $(grep -c "AUTOEXEC" $T/log.txt) -ge 2 ] && ok "vuelve a arrancar" || ko "no vuelve a arrancar"
grabado 440
rm -rf $T

for MODO in bajo alto; do
  DONDE="abajo"; [ $MODO = alto ] && DONDE="en la memoria alta, DOS=ALTO"
  echo "== Hispa-8086 (el núcleo $DONDE): SOUND de Hispa-BASIC y Ctrl+Alt+Supr =="
  IMG86=$H86
  if [ $MODO = alto ]; then
    IMG86=$(mktemp); python3 - $H86 $IMG86 <<'PY'
import sys
d = bytearray(open(sys.argv[1], 'rb').read())
i = d.index(b'HCFG:'); d[i + 5] = 1
open(sys.argv[2], 'wb').write(d)
PY
  fi
  h86 $IMG86 'hdavivo#basic#SOUND 440, 300'
  grep -qF "SOUND 440, 300" $T/scr.txt && ok "SOUND sonando" || ko "no se ha llegado a SOUND"
  # Hispa-8086 solo cierra la puerta del altavoz (un 8086 no tiene tarjeta
  # de sonido; b0c640b): el códec no se mira
  mira 0 1 0
  rm -rf $T
  [ $MODO = alto ] && rm -f $IMG86
done

echo "== Hispa-8086: el reinicio de verdad, grabado =="
ESPERA=8 h86 $H86 'basic#SOUND 440, 300' reset
grep -qF "Hispa-8086 0.12 - de la familia Hispa-DOS" $T/scr.txt && ok "vuelve a arrancar" || ko "no vuelve a arrancar"
grabado 440
rm -rf $T

exit $fail
