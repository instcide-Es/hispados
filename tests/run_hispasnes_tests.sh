#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# Prueba de Hispa-Snes (docs/HISPASNES.md): el emulador HISPASNS.HVP.
#  1. nucleo: el núcleo fuera de QEMU, como programa de Linux
#     (tests/hispasnes/snesnativo.asm, solo con NASM):
#      - el arranque propio del SPC700 (programas/hispasnes/arranque.s, con
#        tools/asm816.py) da los mismos 64 octetos que lleva spc.inc;
#      - las pruebas «de un solo paso» del 65C816 y del SPC700
#        (SingleStepTests, MIT, que no van en el proyecto: las baja
#        tests/thirdparty/fetch_snes.sh; sin red, se saltan): registros,
#        memoria y ciclos de cada instrucción;
#      - BUSTEST.SFC: NMI, WAI, IRQ de los temporizadores, multiplicar y
#        dividir, $2180, DMA, HDMA directo e indirecto, MVN, la RAM del
#        cartucho («TODO BIEN» en su RAM);
#      - SPCTEST.SFC: el trato del arranque, MUL/DIV/DAA del SPC700 («SPC700
#        BIEN») y la melodía (las frecuencias de cada nota en el sonido);
#      - las pantallas de PPU1, PPU2, PPU3 (modo 7), PPU4 (mosaico), BUSTEST, SPCTEST y
#        PRUEBA.SFC, punto por punto como las guardadas en
#        tests/hispasnes/esperado/.
#  2. ventanas: en Hispa Ventanas (QEMU, el disquete con el módulo y las
#     ROM): /T con BUSTEST.SFC y SPCTEST.SFC dentro del módulo (lo que dicen,
#     en HISPASNS.LOG), y la velocidad pintando cada cuadro al doble con
#     PRUEBA.SFC.
#  3. sonido: PRUEBA.SFC a su velocidad, con HSPSND y una Sound Blaster 16 y
#     luego con HD Audio: la melodía suena en la grabación de QEMU.
#  4. pentium: en Hispa-Pentium, con HVPRUN (el disco de nucleo32/disco.sh
#     con la oficina y el emulador en \OFICINA, lanzado desde la Consola):
#     BUSTEST da lo mismo que en Hispa Ventanas, y la velocidad pintando.
# uso: run_hispasnes_tests.sh [imagen]     SOLO="nucleo ventanas" para algunas
#      ACTUALIZA=1 vuelve a guardar las pantallas esperadas
# Hispa-Snes (16-bit console emulator module) tests.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=build
T=$(mktemp -d)
ESP=tests/hispasnes/esperado
DL=tests/thirdparty/dl/snes
NAT=$B/snesnativo
export MTOOLS_SKIP_CHECK=1
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.3}"; }
send() { printf '%s\r' "$1"; sleep "${2:-1.5}"; }
quiero() { [ -z "${SOLO:-}" ] || [[ " $SOLO " == *" $1 "* ]]; }
# el valor de CLAVE= en HISPASNS.LOG (sacado a $T/snes.log)
del_log() { sed -n "s/^$1=//p" $T/snes.log | head -1; }
# lo que ha dejado una ROM de prueba en su RAM: «HSN» y el texto
srm_texto() { python3 -c "import sys; d=open(sys.argv[1],'rb').read(); print(d[4:d.index(0,4)].decode() if d[:3]==b'HSN' else '')" "$1" 2> /dev/null; }

make -s $B/snesnativo $B/PPU1.SFC $B/PPU2.SFC $B/PPU3.SFC $B/PPU4.SFC $B/BUSTEST.SFC $B/SPCTEST.SFC $B/PRUEBA.SFC $B/HISPASNS.HVP > /dev/null || { echo "no se puede hacer lo de Hispa-Snes"; exit 1; }

if quiero nucleo; then
  echo "== Hispa-Snes: el núcleo, fuera de QEMU =="
  # el arranque propio del SPC700
  python3 tools/asm816.py programas/hispasnes/arranque.s $T/arr.bin
  a=$(python3 -c "import sys; print(open(sys.argv[1],'rb').read()[0xFFC0:].hex())" $T/arr.bin)
  b=$(python3 -c "
import re,sys
t=open('programas/hispasnes/spc.inc').read().split('spc_arranque:')[1]
print(''.join('%02x'%int(x,16) for x in re.findall(r'0x([0-9A-F]{2})', t)[:64]))")
  if [ "$a" = "$b" ]; then ok "el arranque del SPC700 (arranque.s) es el que lleva spc.inc (64 octetos)"; else ko "arranque.s y spc.inc no dan lo mismo"; fi
  # SingleStepTests
  tests/thirdparty/fetch_snes.sh > /dev/null 2>&1
  if [ -d $DL/65816 ] && [ "$(ls $DL/65816 | wc -l)" -ge 512 ]; then
    r=$(python3 tests/hispasnes/sst.py $NAT $DL/65816 $DL/spc700 100 | tail -1)
    case "$r" in
      *"mal: 0, con otros ciclos: 400") ok "SingleStepTests ($r: los de WAI y STP, que se paran hasta el final del tramo)";;
      *"mal: 0,"*) ok "SingleStepTests ($r)";;
      *) ko "SingleStepTests: $r";;
    esac
  else
    echo "  (sin las pruebas de un solo paso: sin red)"
  fi
  # BUSTEST y SPCTEST: lo que dejan en su RAM
  $NAT $B/BUSTEST.SFC 60 $T/bus.ppm - - $T/bus.srm > /dev/null
  t=$(srm_texto $T/bus.srm)
  if [ "$t" = "TODO BIEN" ]; then ok "BUSTEST: NMI, WAI, IRQ V y H+V, MUL/DIV, \$2180, DMA, HDMA, MVN y SRAM bien"; else ko "BUSTEST: «$t»"; fi
  $NAT $B/SPCTEST.SFC 120 $T/spc.ppm - $T/spc.raw $T/spc.srm > /dev/null
  t=$(srm_texto $T/spc.srm)
  if [ "$t" = "SPC700 BIEN" ]; then ok "SPCTEST: el programa sube al SPC700 por el arranque y sus cuentas (MUL, DIV, DAA) salen bien"; else ko "SPCTEST: «$t»"; fi
  notas=$(python3 - $T/spc.raw <<'P'
import struct, sys
d = open(sys.argv[1], 'rb').read()
s = struct.unpack('<%dh' % (len(d) // 2), d)[0::2]      # (la izquierda)
# cada nota dura 8000 muestras (un cuarto de segundo): las del medio de cada una
out = []
for k in range(4000 + 8000 * 1, len(s) - 8000, 8000):
    tr = s[k - 2000:k + 2000]
    c = sum(1 for i in range(1, len(tr)) if (tr[i - 1] < 0) != (tr[i] < 0))
    out.append(round(c * 4))
# la escala de la ROM (tono 0385h, 03F3h...: 440, 494, 556, 588, 660 Hz):
# cuatro notas seguidas de ella, cada una a menos de 12 Hz
esc = [440, 494, 556, 588, 660]
bien = any(all(abs(out[i] - esc[j + i]) < 12 for i in range(4)) for j in range(2))
print(('BIEN ' if bien else 'MAL ') + ' '.join(str(x) for x in out[:4]))
P
)
  case "$notas" in
    BIEN*) ok "la melodía de SPCTEST (BRR, ADSR, tono y temporizador): ${notas#BIEN } Hz";;
    *) ko "la melodía de SPCTEST: «${notas#MAL }» Hz (440, 494, 556, 588, 660)";;
  esac
  # las pantallas
  for p in ppu1:PPU1:10 ppu2:PPU2:10 ppu3:PPU3:60 ppu4:PPU4:10 bustest:BUSTEST:60 spctest:SPCTEST:60 demo:PRUEBA:60; do
    n=${p%%:*}; r=${p#*:}; c=${r#*:}; r=${r%%:*}
    $NAT $B/$r.SFC $c $T/$n.ppm > /dev/null
    if [ -n "${ACTUALIZA:-}" ]; then gzip -9 -n -c $T/$n.ppm > $ESP/$n.ppm.gz; fi
    if zcat $ESP/$n.ppm.gz | cmp -s - $T/$n.ppm; then ok "la pantalla de $r.SFC es la guardada"; else ko "la pantalla de $r.SFC no es la guardada"; cp $T/$n.ppm $B/hispasnes_$n.ppm; fi
  done
fi

# ---- en QEMU: el disquete con el módulo y las ROM ----
disco() {
  cp "$IMG" $T/f.img
  # (sitio: fuera lo que aquí no se usa)
  mdel -i $T/f.img ::HISPADOS/HISPA.WAV ::HISPADOS/AUDICION.COM ::HISPADOS/JUEGOS/HISPANES.HVP \
    ::HISPADOS/JUEGOS/HISPASMS.HVP ::HISPADOS/JUEGOS/SOLITARI.HVP > /dev/null 2>&1
  printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS
  mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
  mcopy -o -i $T/f.img $B/HISPASNS.HVP ::HISPADOS/JUEGOS/HISPASNS.HVP
  mmd -i $T/f.img ::HISPADOS/JUEGOS/SNES 2> /dev/null
  mcopy -o -i $T/f.img $B/BUSTEST.SFC $B/SPCTEST.SFC $B/PRUEBA.SFC ::HISPADOS/JUEGOS/SNES/
  local nmod=$(mdir -b -i $T/f.img ::HISPADOS/JUEGOS ::HISPADOS/PROGS 2>/dev/null | grep -c '\.HVP')
  printf '%s\r\n' '[Centro]' 'Ventana=36,52,452,190' 'Activo=0' "Modulos=$nmod" '' \
    '[Grupo]' 'Nombre=Juegos' 'Ventana=300,380,452,190' 'Estado=1' '' \
    '[Acceso]' 'Nombre=Hispa-Snes' 'Programa=A:\HISPADOS\JUEGOS\HISPASNS.HVP' "Parametros=$1" > $T/CENTRO.INI
  mcopy -o -i $T/f.img $T/CENTRO.INI ::HISPADOS/CENTRO.INI
}
# corre QEMU con el disquete: run SEGUNDOS ESPERA [opciones de QEMU]
run() {
  local tmo=$1 espera=$2; shift 2
  rm -f $T/mon
  { sleep 6
    send 'KEYB SP' 2
    send 'A:\HISPADOS\HSPRATON' 2
    [ -n "${CON_SND:-}" ] && send 'A:\HISPADOS\HSPSND' 3
    send 'VENTANAS' 6
    mon "sendkey ret" 0.3
    sleep $espera
    [ -n "${DESPIERTA:-}" ] && { mon "sendkey right" 0.3; sleep $DESPIERTA; }
    mon "screendump $T/s.ppm" 1
    mon quit
  } | timeout $tmo qemu-system-i386 -vga std -global VGA.xres=800 -global VGA.yres=600 -display none -serial stdio \
      -monitor unix:$T/mon,server,nowait -parallel none -m 32 -drive file=$T/f.img,format=raw,if=floppy,index=0 \
      -boot a -no-reboot "$@" > $T/raw 2>&1
  mtype -i $T/f.img ::HISPADOS/JUEGOS/HISPASNS.LOG 2> /dev/null | tr -d '\r' > $T/snes.log
}

if quiero ventanas; then
  echo "== Hispa-Snes en Hispa Ventanas (QEMU) =="
  disco 'A:\HISPADOS\JUEGOS\SNES\BUSTEST.SFC /T=120'
  run 200 25
  if [ "$(del_log Texto)" = "TODO BIEN" ] && [ "$(del_log Mapa)" = 1 ]; then ok "BUSTEST.SFC dentro del módulo: TODO BIEN (HISPASNS.LOG)"; else ko "BUSTEST.SFC dentro del módulo: $(tr '\n' ' ' < $T/snes.log)"; fi
  disco 'A:\HISPADOS\JUEGOS\SNES\SPCTEST.SFC /T=120'
  run 200 25
  if [ "$(del_log Texto)" = "SPC700 BIEN" ]; then ok "SPCTEST.SFC dentro del módulo: SPC700 BIEN"; else ko "SPCTEST.SFC dentro del módulo: $(tr '\n' ' ' < $T/snes.log)"; fi
  disco 'A:\HISPADOS\JUEGOS\SNES\PRUEBA.SFC /T=600 /V'
  run 300 60
  cps=$(del_log CuadrosPorSegundo)
  echo "  (velocidad en este QEMU: ${cps:-?} cuadros por segundo con PRUEBA.SFC pintando cada uno en la $(del_log Modo), $(del_log Milisegundos) ms los 600)"
  if [ "${cps:-0}" -ge 60 ]; then ok "PRUEBA.SFC, pintando cada cuadro: 60 por segundo o más"; else ko "solo ${cps:-?} cuadros por segundo"; fi
fi

if quiero sonido; then
  echo "== Hispa-Snes: el sonido, con HSPSND =="
  for tarjeta in "sb16:-device sb16,audiodev=snd0" "hda:-device intel-hda -device hda-micro,audiodev=snd0"; do
    n=${tarjeta%%:*}; opc=${tarjeta#*:}
    disco 'A:\HISPADOS\JUEGOS\SNES\PRUEBA.SFC'
    CON_SND=1 DESPIERTA=6 run 200 12 -audiodev wav,id=snd0,path=$T/$n.wav $opc
    m=$(python3 -c "
import struct, sys
d = open(sys.argv[1], 'rb').read()[44:]
s = struct.unpack('<%dh' % (len(d) // 2), d[:len(d) // 2 * 2])
print(max((abs(v) for v in s), default=0), len(s))" $T/$n.wav 2> /dev/null)
    set -- $m
    if [ "${1:-0}" -gt 1500 ]; then ok "la melodía de PRUEBA.SFC suena por HSPSND ($n: la muestra mayor, ${1:-0})"; else ko "no suena con $n (${m:-sin grabación})"; fi
  done
fi

if quiero pentium; then
  echo "== Hispa-Snes en Hispa-Pentium, con HVPRUN (QEMU) =="
  if bash castel/nucleo32/disco.sh $T/of.img 16 oficina > $T/disco.txt 2>&1; then
    mcopy -o -i $T/of.img $B/HISPASNS.HVP $B/BUSTEST.SFC $B/PRUEBA.SFC ::OFICINA/
    pent() {
      CAPTURAS=$T ESCRIBE=1 DISCO=$T/of.img ARRANQUE=escritorio MODULOS= timeout 400 bash castel/nucleo32/probar.sh "" \
        ":pausa 2" ":clic 40 585" ":pausa 1" ":clic 70 213" ":pausa 3" ":escribe HVPRUN \\OFICINA\\HISPASNS.HVP $1" ":tecla ret" \
        ":pausa $2" ":captura p" > $T/pent.txt 2>&1
      mtype -i $T/of.img ::OFICINA/HISPASNS.LOG 2> /dev/null | tr -d '\r' > $T/snes.log
    }
    pent '\OFICINA\BUSTEST.SFC /T=120' 40
    if [ "$(del_log Texto)" = "TODO BIEN" ]; then ok "BUSTEST.SFC en HVPRUN: TODO BIEN, sin puertos ni modo real (el reloj fino de HVPRUN)"; else ko "BUSTEST.SFC en HVPRUN: $(tr '\n' ' ' < $T/snes.log)"; fi
    pent '\OFICINA\PRUEBA.SFC /T=600 /V' 50
    cps=$(del_log CuadrosPorSegundo)
    echo "  (velocidad en Hispa-Pentium con QEMU: ${cps:-?} cuadros por segundo con PRUEBA.SFC pintando en la $(del_log Modo))"
    if [ "${cps:-0}" -ge 60 ]; then ok "en HVPRUN, pintando cada cuadro: 60 por segundo o más"; else ko "en HVPRUN, solo ${cps:-?} cuadros por segundo"; fi
  else
    ko "disco.sh no puede hacer el disco de Hispa-Pentium: $(tail -1 $T/disco.txt)"
  fi
fi

rm -rf $T
exit $fail
