#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# Prueba de Hispa-Drive (docs/HISPADRIVE.md): el emulador HISPADRV.HVP de la
# consola de 16 bits de 1988, con sus ROM de prueba propias (PRUEBA.MD y
# CARGA.MD, tests/hispadrv/*.s).
#  1. nucleo: el núcleo fuera de QEMU (tests/hispadrv/mdnativo.asm, los
#     mismos .inc como programa de Linux, solo con NASM):
#      - el 68000 contra las pruebas de una instrucción de SingleStepTests
#        (más de 800.000; no van en el proyecto: las baja
#        tests/thirdparty/fetch_md.sh; sin red, se saltan). Se aceptan las
#        que se sabe que no coinciden (docs/HISPADRIVE.md: ASR con más
#        desplazamiento que bits, dos de ASL.b);
#      - PRUEBA.MD: el VDP, las DMA, las interrupciones, el Z80, el YM2612,
#        los mandos (de tres y de seis botones, con un guion) y la RAM del
#        cartucho (y que se guarda: la segunda vez dice «ENCENDIDO: 0002»);
#        su pantalla, la guardada; y la nota FM del final, un La de 440 Hz;
#      - CARGA.MD: su pantalla, la guardada, y lo que cuesta cada cuadro.
#  2. ventanas: en Hispa Ventanas (QEMU): /T=600 /V con PRUEBA.MD (todo
#     BIEN dentro del módulo, al doble, 60 c/s o más) y la velocidad con
#     CARGA.MD, pintando y sin pintar.
#  3. sonido: lo mismo, a su velocidad y con una Sound Blaster 16: la nota
#     FM suena por HSPSND (en la grabación de QEMU, unos 440 Hz).
#  4. pentium: en Hispa-Pentium, con HVPRUN.BIN (desde la Consola): /T=600
#     /V con PRUEBA.MD, todo BIEN.
# uso: run_hispadrv_tests.sh [imagen]    SOLO="nucleo ventanas" para algunas
#      ACTUALIZA=1 vuelve a guardar las pantallas esperadas (sus sumas)
# Deja capturas en build/hispadrv_*.png.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=build
T=$(mktemp -d)
NAT=$B/mdnativo
ESP=tests/hispadrv/esperado
DL=tests/thirdparty/dl/md
fail=0
export MTOOLS_SKIP_CHECK=1
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
toca() { [ -z "${SOLO:-}" ] || [[ " $SOLO " == *" $1 "* ]]; }
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.3}"; }
tecla() { local k; for k in "$@"; do mon "sendkey $k" 0.3; done; }
shot() { sleep 0.4; mon "screendump $T/$1.ppm" 0.6; }
send() { printf '%s\r' "$1"; sleep "${2:-1.5}"; }
# texto de la salida de la ROM de prueba (FF0000h: «HDRV» y el texto)
texto_mem() { python3 -c "
import sys
d = open(sys.argv[1], 'rb').read()
print(d[4:1000].split(b'\0')[0].decode('latin1'))" "$1"; }
suma() { md5sum "$1" | cut -c1-12; }

# ---- 1. el núcleo, fuera de QEMU ----
if toca nucleo; then
  echo "== Hispa-Drive: el núcleo (68000, VDP, Z80, YM2612) fuera de QEMU =="
  if ! $NAT -t /dev/null > /dev/null 2>&1 && [ ! -x $NAT ]; then
    ko "no está $NAT (make mdprogs)"
  else
    # el 68000: SingleStepTests
    bash tests/thirdparty/fetch_md.sh > /dev/null 2>&1
    n=0; mal=0; tb=0; tt=0
    if ls $DL/*.json.gz > /dev/null 2>&1; then
      for f in $DL/*.json.gz; do
        nom=$(basename $f .json.gz)
        python3 -I tests/hispadrv/sst68k.py $f $T/sst.bin 2> /dev/null
        r=$($NAT -t $T/sst.bin | tail -1)
        b=$(echo "$r" | sed 's/bien: \([0-9]*\)\/.*/\1/'); t=$(echo "$r" | sed 's/bien: [0-9]*\/\([0-9]*\).*/\1/')
        tb=$((tb + b)); tt=$((tt + t)); n=$((n + 1))
        if [ "$b" != "$t" ]; then
          case $nom in
            ASR.b|ASR.w|ASR.l|ASL.b|DIVU) [ $((b * 100)) -ge $((t * 75)) ] || { mal=1; echo "        $nom: $r"; } ;;
            *) mal=1; echo "        $nom: $r" ;;
          esac
        fi
      done
      if [ $mal = 0 ]; then ok "el 68000: $tb de $tt pruebas de una instrucción ($n archivos; las que faltan, las sabidas)"; else ko "el 68000: $tb de $tt"; fi
    else
      echo "  (sin red: no se prueba el 68000 con SingleStepTests)"
    fi
    # PRUEBA.MD, con un guion: B y Start (mando de tres)
    python3 -c "
import struct
open('$T/g3.bin','wb').write(struct.pack('<H', 0x0090) * 400)
open('$T/g6.bin','wb').write(struct.pack('<H', 0x8000 | 0x0400 | 0x0100 | 0x0041) * 400)"
    $NAT $B/PRUEBA.MD 300 $T/p.ppm $T/g3.bin $T/m.bin $T/son.raw
    t=$(texto_mem $T/m.bin)
    if echo "$t" | grep -q "FIN. FALLOS: 0000" && [ "$(echo "$t" | grep -c BIEN)" = 6 ]; then
      ok "PRUEBA.MD: VRAM, DMA, interrupciones, Z80, YM2612 y RAM del cartucho, BIEN"
    else ko "PRUEBA.MD: $(echo "$t" | tr '\n' '|')"; fi
    if echo "$t" | grep -q "MANDO: 6F13 6F13"; then ok "el mando de tres botones: B y Start (6F13), y sin la secuencia de seis"; else ko "mando de tres: $(echo "$t" | grep MANDO)"; fi
    $NAT $B/PRUEBA.MD 300 $T/p6.ppm $T/g6.bin $T/m6.bin
    if texto_mem $T/m6.bin | grep -q "MANDO: 7E22 7A20"; then ok "el de seis: arriba y A (7E22), y la tercera vez X y Z (7A20)"; else ko "mando de seis: $(texto_mem $T/m6.bin | grep MANDO)"; fi
    # la RAM del cartucho, otra vez (como el .SRM)
    dd if=$T/m.bin of=$T/sram.bin bs=1 skip=$((0x12000)) count=65536 status=none
    $NAT $B/PRUEBA.MD 300 $T/p2.ppm - $T/m2.bin - $T/sram.bin
    if texto_mem $T/m2.bin | grep -q "ENCENDIDO: 0002"; then ok "la RAM del cartucho, guardada y vuelta a poner: «ENCENDIDO: 0002»"; else ko "RAM del cartucho: $(texto_mem $T/m2.bin | grep ENCEN)"; fi
    # la pantalla
    convert $T/p.ppm $B/hispadrv_prueba.png 2> /dev/null
    for r in prueba:p carga:c; do
      nom=${r%%:*}; f=${r##*:}
      [ $nom = carga ] && $NAT $B/CARGA.MD 600 $T/c.ppm
      s=$(suma $T/$f.ppm)
      if [ -n "${ACTUALIZA:-}" ]; then mkdir -p $ESP; echo "$s" > $ESP/$nom.suma; fi
      if [ "$s" = "$(cat $ESP/$nom.suma 2> /dev/null)" ]; then ok "${nom^^}.MD: la pantalla es la guardada ($s)"; else ko "${nom^^}.MD: la pantalla ha cambiado ($s)"; fi
    done
    convert $T/c.ppm $B/hispadrv_carga.png 2> /dev/null
    # las sombras y los brillos (SOMBRAS.MD): cada zona, con la luz que le toca
    $NAT $B/SOMBRAS.MD 10 $T/s.ppm
    convert $T/s.ppm $B/hispadrv_sombras.png 2> /dev/null
    r=$(python3 -c "
import sys
d = open(sys.argv[1], 'rb').read().split(b'\\n', 3)
w = int(d[1].split()[0]); px = d[3]
def p(x, y):
    i = (y * w + x) * 3
    return tuple(px[i:i + 3])
gris, verde = p(300, 10), p(50, 165)
def mitad(a, b): return all(abs(2 * x - y) <= 40 for x, y in zip(a, b))
def mas(a, b): return all(x >= y for x, y in zip(a, b)) and sum(a) > sum(b)
mal = []
for nom, x, y, f in [('el plano B bajo, a la mitad', 10, 10, lambda c: mitad(c, gris)),
                     ('el plano A bajo, a la mitad', 100, 85, lambda c: c[1] == c[2] == 0 and 64 <= c[0] <= 160),
                     ('lo transparente alto deja normal', 155, 165, lambda c: c == gris),
                     ('la luz sobre la sombra: normal', 50, 50, lambda c: c == gris),
                     ('la luz sobre lo normal: brillo', 210, 50, lambda c: mas(c, gris)),
                     ('la sombra tapada por un plano alto', 250, 110, lambda c: c == gris),
                     ('la sombra de un objeto alto', 270, 50, lambda c: mitad(c, gris)),
                     ('un objeto bajo en la sombra, a la mitad', 50, 152, lambda c: mitad(c, verde)),
                     ('un objeto alto, normal', 90, 152, lambda c: c == verde),
                     ('el color 14, siempre normal', 130, 152, lambda c: c == p(130, 165)),
                     ('la luz sobre una fila normal: brillo', 20, 165, lambda c: mas(c, gris))]:
    if not f(p(x, y)): mal.append('%s %s' % (nom, p(x, y)))
print('|'.join(mal) or 'BIEN')" $T/s.ppm)
    if [ "$r" = BIEN ]; then ok "SOMBRAS.MD: las sombras y los brillos (planos, ventana de luz, objetos 3Eh y 3Fh, color 14), BIEN"; else ko "SOMBRAS.MD: $r"; fi
    # la nota FM: 440 Hz en el último segundo
    hz=$(python3 -c "
import struct, sys
d = open('$T/son.raw', 'rb').read()
s = struct.unpack('<%dh' % (len(d) // 2), d)[-22050:]
print(sum(1 for i in range(1, len(s)) if s[i-1] < 0 <= s[i]), max(s))")
    set -- $hz
    if [ "$1" -ge 436 ] && [ "$1" -le 444 ] && [ "$2" -gt 3000 ]; then ok "la nota FM del YM2612: $1 Hz (un La), hasta $2"; else ko "la nota FM: $1 Hz, hasta $2"; fi
    # lo que cuesta un cuadro (instrucciones del x86), si hay perf
    if command -v perf > /dev/null 2>&1; then
      for r in PRUEBA CARGA; do
        a=$(perf stat -x, -e instructions:u $NAT $B/$r.MD 600 $T/x.ppm - - 0 2>&1 | grep instructions | cut -d, -f1)
        c=$(perf stat -x, -e instructions:u $NAT $B/$r.MD 600 - - - 0 2>&1 | grep instructions | cut -d, -f1)
        echo "  ($r.MD: $(( a / 600 / 1000 )) mil instrucciones del x86 por cuadro pintándolo, $(( c / 600 / 1000 )) mil sin pintar; sin sonido)"
      done
    fi
  fi
fi

# ---- 2 y 3. en Hispa Ventanas (QEMU) ----
disco() {   # disco PARÁMETROS
  cp "$IMG" $T/f.img
  # sitio para el módulo (400 K) y las ROM: fuera lo que aquí no se usa
  mdel -i $T/f.img ::HISPADOS/JUEGOS/HISPASMS.HVP ::HISPADOS/JUEGOS/HISPANES.HVP ::HISPADOS/AUDICION.COM \
    ::HISPADOS/JUEGOS/SMS/BUZO.SMS ::HISPADOS/JUEGOS/NES/HISPA1.NES ::HISPADOS/HISPA.WAV \
    ::HISPADOS/PROGS/MIRADOR.HVP ::HISPADOS/PROGS/ARCHIVOS.HVP ::HISPADOS/PROGS/PINTURA.HVP ::HISPADOS/PROGS/TAREAS.HVP > /dev/null 2>&1
  printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS
  mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
  mmd -i $T/f.img ::HISPADOS/JUEGOS/MD 2> /dev/null
  mcopy -o -i $T/f.img $B/HISPADRV.HVP ::HISPADOS/JUEGOS/HISPADRV.HVP
  mcopy -o -i $T/f.img $B/PRUEBA.MD ::HISPADOS/JUEGOS/MD/PRUEBA.MD
  mcopy -o -i $T/f.img $B/CARGA.MD ::HISPADOS/JUEGOS/MD/CARGA.MD
  local nmod=$(mdir -b -i $T/f.img ::HISPADOS/JUEGOS ::HISPADOS/PROGS 2>/dev/null | grep -c '\.HVP')
  printf '%s\r\n' '[Centro]' 'Ventana=36,52,452,190' 'Activo=0' "Modulos=$nmod" '' \
    '[Grupo]' 'Nombre=Juegos' 'Ventana=300,380,452,190' 'Estado=1' '' \
    '[Acceso]' 'Nombre=Hispa-Drive' 'Programa=A:\HISPADOS\JUEGOS\HISPADRV.HVP' "Parametros=$1" > $T/CENTRO.INI
  mcopy -o -i $T/f.img $T/CENTRO.INI ::HISPADOS/CENTRO.INI
}
run() {
  local tmo=$1; shift
  rm -f $T/mon
  timeout $tmo qemu-system-i386 -vga std -global VGA.xres=800 -global VGA.yres=600 -display none -serial stdio \
    -monitor unix:$T/mon,server,nowait -parallel none -m 32 -drive file=$T/f.img,format=raw,if=floppy,index=0 \
    -boot a -no-reboot "$@" > $T/raw 2>&1
}
entrar() {
  sleep 6
  send 'KEYB SP' 2
  send 'A:\HISPADOS\HSPRATON' 2
  local o; for o in "$@"; do send "$o" 3; done
  send 'VENTANAS' 6
}
del_log() { sed -n "s/^$1=//p" $T/md.log | head -1; }
saca_log() { mtype -i $T/f.img ::HISPADOS/JUEGOS/HISPADRV.LOG 2> /dev/null | tr -d '\r' > $T/md.log; }

if toca ventanas; then
  echo "== Hispa-Drive en Hispa Ventanas: PRUEBA.MD dentro del módulo (/T=600 /V) =="
  if [ ! -f "$IMG" ] || [ ! -f $B/HISPADRV.HVP ]; then ko "falta $IMG o $B/HISPADRV.HVP (make $IMG mdprogs)"; else
  disco 'A:\HISPADOS\JUEGOS\MD\PRUEBA.MD /T=600 /V'
  { entrar; tecla ret; sleep 45; shot v1; mon quit; } | run 200
  saca_log
  if del_log Texto | grep -q "FIN. FALLOS: 0000" && [ "$(del_log Texto | grep -o BIEN | wc -l)" = 6 ]; then
    ok "HISPADRV.LOG: todo BIEN también dentro del módulo"
  else ko "HISPADRV.LOG: $(tr '\n' ' ' < $T/md.log)"; fi
  cps=$(del_log CuadrosPorSegundo)
  if [ "$(del_log Modo)" = "ventana x2" ] && [ "${cps:-0}" -ge 60 ]; then ok "al doble, pintando cada cuadro: $cps cuadros por segundo en este QEMU"; else ko "Modo=$(del_log Modo) $cps c/s"; fi
  [ -f $T/v1.ppm ] && convert $T/v1.ppm $B/hispadrv_ventana.png
  echo "== La velocidad con CARGA.MD (los dos planos, 80 objetos, DMA y FM) =="
  for v in "/V:pintando al doble" ":sin pintar"; do
    disco "A:\\HISPADOS\\JUEGOS\\MD\\CARGA.MD /T=600 ${v%%:*}"
    { entrar; tecla ret; sleep 45; mon quit; } | run 200
    saca_log
    echo "  (CARGA.MD, ${v##*:}: $(del_log CuadrosPorSegundo) cuadros por segundo en este QEMU, $(del_log Milisegundos) ms los 600)"
  done
  fi
fi

if toca sonido; then
  echo "== Hispa-Drive con sonido: la nota FM por HSPSND (Sound Blaster 16) =="
  if [ ! -f "$IMG" ] || [ ! -f $B/HISPADRV.HVP ]; then ko "falta $IMG o $B/HISPADRV.HVP"; else
  disco 'A:\HISPADOS\JUEGOS\MD\PRUEBA.MD'
  { entrar 'A:\HISPADOS\HSPSND'
    tecla ret; sleep 10
    tecla spc; sleep 15
    shot s1
    mon quit
  } | run 200 -audiodev wav,id=snd0,path=$T/md.wav -device sb16,audiodev=snd0
  r=$(python3 -c "
import struct, sys
d = open('$T/md.wav', 'rb').read()
can, hz = struct.unpack_from('<HI', d, 22)          # (la cabecera del WAV)
d = d[44:]
s = struct.unpack('<%dh' % (len(d) // 2), d[:len(d) // 2 * 2])
canal = s[0::can][-hz * 4:]                         # (un canal, los últimos 4 s)
z = sum(1 for i in range(1, len(canal)) if canal[i-1] < 0 <= canal[i])
print(len(canal), max(canal or [0]), z * hz // max(1, len(canal)))" 2> /dev/null)
  set -- $r 0 0 0
  if [ "${2:-0}" -gt 2000 ] && [ "${3:-0}" -ge 420 ] && [ "${3:-0}" -le 460 ]; then
    ok "suena la nota FM: unos $3 Hz en la grabación (la mayor, $2)"
  else ko "la grabación: $1 muestras, la mayor $2, $3 Hz"; fi
  fi
fi

if toca pentium; then
  echo "== Hispa-Drive en Hispa-Pentium (HVPRUN.BIN, desde la Consola) =="
  if bash castel/nucleo32/disco.sh $T/pent.img 16 oficina > $T/disco.txt 2>&1; then
    mcopy -i $T/pent.img $B/HISPADRV.HVP ::HISPADRV.HVP
    mcopy -i $T/pent.img $B/PRUEBA.MD ::PRUEBA.MD
    CAPTURAS=$T ESCRIBE=1 DISCO=$T/pent.img ARRANQUE=escritorio MODULOS= timeout 300 castel/nucleo32/probar.sh "" \
      ":pausa 3" ":clic 40 585" ":pausa 1" ":clic 70 328" ":pausa 4" ":escribe HVPRUN.BIN HISPADRV.HVP PRUEBA.MD /T=600 /V" \
      ":pausa 1" ":tecla ret" ":pausa 40" ":captura pe1" > $T/pent.sal 2>&1
    mtype -i $T/pent.img ::HISPADRV.LOG 2> /dev/null | tr -d '\r' > $T/md.log
    if del_log Texto | grep -q "FIN. FALLOS: 0000"; then ok "HVPRUN: PRUEBA.MD, todo BIEN dentro de Hispa-Pentium"; else ko "HVPRUN: $(tr '\n' ' ' < $T/md.log)"; fi
    cps=$(del_log CuadrosPorSegundo)
    if [ "${cps:-0}" -ge 60 ]; then ok "pintando cada cuadro en la ventana: $cps cuadros por segundo en este QEMU"; else ko "solo ${cps:-?} c/s"; fi
    [ -f $T/pe1.ppm ] && convert $T/pe1.ppm $B/hispadrv_pentium.png
  else ko "disco.sh no puede hacer el disco: $(tail -1 $T/disco.txt)"; fi
fi
rm -rf $T
exit $fail
