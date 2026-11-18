#!/bin/bash
# [VTD] La protección de DMA (VT-d) que deja puesta un firmware UEFI cuando
# se arranca con HispaBIOS (MacBook Air 2017): lib/vtd.inc, en HSPSND y en
# AUDICION. En QEMU con -machine q35 -device intel-iommu; VTDON.COM
# (tests/progs/vtdon.asm) hace de firmware: enciende la traducción con todo
# cerrado. QEMU graba lo que sale por la tarjeta: con la DMA cerrada no
# suena nada (la controladora no puede leer el sonido de la memoria).
#   h0  HSPSND /NOVTD: no se toca y no suena (la prueba de que VTDON cierra)
#   hA  HSPSND: paso directo (no había entrada de contexto) y suena; /INFO
#       con el driver cargado lo dice
#   hP  VTDON P (el firmware tenía una entrada, cerrada): cambiada, y suena
#   hB  /SIMVTD:B: sin paso directo; se apaga la traducción y suena
#   hN  con la traducción apagada (una BIOS de siempre): no se toca nada
#   aA  AUDICION: el informe con las unidades, las faltas, las entradas de
#       contexto, el paso directo y CORB/RIRB después; suena
#   aB  AUDICION /SIM:VTB con VTDON P: apaga la traducción; suena
#   a0  AUDICION sin saber dónde está la unidad: CORB/RIRB no va y no suena
# /SIMVTD y /SIM:VTD (solo para las pruebas) dicen dónde está la unidad de
# QEMU (FED90000h), que no tiene MCHBAR. Ver docs/AUDICION.md.
# VT-d DMA protection left on by UEFI firmware: HSPSND and AUDICION open it.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
JOBS=${JOBS:-2}
T=$(mktemp -d)
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
has() { if grep -qF -- "$2" "$1"; then ok "$3$2"; else ko "$3$2"; fi; }
hasnt() { if grep -qF -- "$2" "$1"; then ko "$3(no debe salir) $2"; else ok "$3no sale: $2"; fi; }
if ! qemu-system-i386 -machine q35 -device intel-iommu,help >/dev/null 2>&1; then
  echo "== VT-d: este QEMU no tiene intel-iommu; no se prueba =="; exit 0
fi
QEMU() { # nombre
  echo qemu-system-i386 -machine q35 -device intel-iommu -display none -serial stdio -monitor none -parallel none \
    -audiodev wav,id=snd0,path=$T/$1/out.wav -device intel-hda -device hda-duplex,audiodev=snd0 \
    -drive file=$T/$1/f.img,format=raw,if=floppy -boot a -no-reboot
}
prep() {
  mkdir -p $T/$1; cp "$IMG" $T/$1/f.img
  MTOOLS_SKIP_CHECK=1 mcopy -o -i $T/$1/f.img build/VTDON.COM ::VTDON.COM
}
# ord nombre orden...: las órdenes, de una en una
ord() {
  local n=$1; shift
  prep $n
  python3 tests/tools/ordenes_drv.py $T/$n/log "$@" -- $(QEMU $n)
  tr -d '\r' < $T/$n/log | iconv -f CP437 -t UTF-8 > $T/$n/log.txt
}
# aud nombre respuestas [--pre X]... [--cmd X]: AUDICION con respuestas
aud() {
  local n=$1 ans=$2; shift 2
  prep $n
  python3 tests/tools/audicion_drv.py $T/$n/log "$ans" "$@" -- $(QEMU $n)
  tr -d '\r' < $T/$n/log | iconv -f CP437 -t UTF-8 > $T/$n/log.txt
  MTOOLS_SKIP_CHECK=1 mtype -i $T/$n/f.img ::SONIDO01.TXT 2>/dev/null | tr -d '\r' | iconv -f CP437 -t UTF-8 > $T/$n/rep.txt
}
# audio nombre -> ventanas de 50 ms con sonido y su frecuencia
audio() {
  python3 - $T/$1/out.wav > $T/$1/audio.txt 2>&1 <<'PY'
import sys, wave, struct
try:
    w = wave.open(sys.argv[1], 'rb')
except Exception as e:
    print("ventanas=0 hz=0 (sin grabación)"); sys.exit(0)
ch, rate, n = w.getnchannels(), w.getframerate(), w.getnframes()
raw = w.readframes(n)
v = struct.unpack('<%dh' % (len(raw) // 2), raw)[0::ch]
win = rate // 20
c, fr = 0, []
for i in range(0, len(v) - win, win):
    s = v[i:i + win]
    if max(abs(x) for x in s) < 300:
        continue
    c += 1
    fr.append(sum(1 for q in range(1, len(s)) if (s[q - 1] < 0) != (s[q] < 0)) * 10)
fr.sort()
print("ventanas=%d hz=%d" % (c, fr[len(fr) // 2] if fr else 0))
PY
}
campo() { sed -n "s/.*$2=\([0-9]*\).*/\1/p" $T/$1/audio.txt; }
entre() { # nombre campo mín máx texto
  local v; v=$(campo $1 $2); v=${v:-0}
  if [ "$v" -ge "$3" ] && [ "$v" -le "$4" ]; then ok "$5 ($2=$v)"; else ko "$5 ($2=$v, se esperaba $3..$4)"; fi
}
lote() { while [ "$(jobs -rp | wc -l)" -ge "$JOBS" ]; do sleep 0.5; done; }
H='A:\HISPADOS\HSPSND'
TONO='PLAYWAV /TONE 440 800'
lote; ord hA VTDON "$H /SIMVTD /NOSB" "$H /SIMVTD /INFO" "$TONO" &
lote; ord hP 'VTDON P' "$H /SIMVTD /NOSB /INFO" "$TONO" &
lote; ord hB 'VTDON P' "$H /SIMVTD:B /NOSB" "$TONO" &
lote; ord hN "$H /SIMVTD /NOSB /INFO" "$TONO" &
lote; aud aA "default=S" --pre VTDON --cmd 'AUDICION /SIM:VTD' &
lote; aud aB "default=S;P03=E" --pre 'VTDON P' --cmd 'AUDICION /SIM:VTB' &
lote; aud a0 "default=N;P02=E" --pre VTDON &
lote; ord h0 VTDON "$H /SIMVTD /NOVTD /NOSB" "$TONO" &
wait

echo "== VT-d: HSPSND con la DMA cerrada y /NOVTD: no suena =="
L=$T/h0/log.txt; audio h0
has $L "VTDON: VT-d encendida, la DMA cerrada." ""
has $L "Protección de DMA del firmware (VT-d, unidad en FED90000h): sin tocar (/NOVTD)." ""
entre h0 ventanas 0 0 "no suena nada"

echo "== VT-d: HSPSND abre el paso directo (sin entrada de contexto) =="
L=$T/hA/log.txt; audio hA
has $L "Protección de DMA del firmware (VT-d, unidad en FED90000h): abierta para esta tarjeta (paso directo)." ""
has $L "(paso directo: resultado 01h; estado del flujo de prueba " ""
if [ "$(grep -c 'abierta para esta tarjeta' $L)" = 2 ]; then ok "/INFO con el driver cargado también lo dice"; else ko "/INFO con el driver cargado también lo dice"; fi
hasnt $L "NO se ha podido abrir" ""
entre hA ventanas 12 18 "suena el tono"
entre hA hz 430 450 "a 440 Hz"

echo "== VT-d: el firmware tenía una entrada cerrada (VTDON P) =="
L=$T/hP/log.txt; audio hP
has $L "abierta para esta tarjeta (paso directo)." ""
has $L "(paso directo: resultado 02h; estado del flujo de prueba " ""
entre hP ventanas 12 18 "suena el tono"
entre hP hz 430 450 "a 440 Hz"

echo "== VT-d: sin paso directo (/SIMVTD:B), se apaga la traducción =="
L=$T/hB/log.txt; audio hB
has $L "Protección de DMA del firmware (VT-d, unidad en FED90000h): traducción apagada." ""
entre hB ventanas 12 18 "suena el tono"

echo "== VT-d: con la traducción apagada no se toca nada =="
L=$T/hN/log.txt; audio hN
hasnt $L "Protección de DMA" ""
entre hN ventanas 12 18 "suena el tono"

echo "== VT-d: AUDICION, el informe y el paso directo =="
R=$T/aA/rep.txt; audio aA
for w in "VT-d (protección de DMA del firmware): MCHBAR=00000000h, unidad de la gráfica 00000000h, unidad general FED90000h" \
  "unidad FED90000h: VER=00000010 CAP=" "GSTS=C0000000 RTADDR=" "VT-d: TRADUCCION ENCENDIDA: el firmware tiene cerrada la DMA." \
  "Faltas apuntadas (antes de tocar nada):" "falta de 00:03.0: motivo 02h, tipo 1 (0 escritura, 1 lectura), dirección " \
  "Tabla raíz: buses con tabla de contexto: 00" "(ninguna: el firmware no abre a nadie del bus 0)" \
  "VT-d: el firmware tenía cerrada la DMA; paso directo para HD Audio 00:03.0: resultado 01h" \
  "antes: 0000000000000000 0000000000000000)" "VT-d: órdenes por CORB/RIRB (por DMA) después: sí" \
  "VT-d, faltas después:" "Las órdenes al códec: por CORB/RIRB sí; inmediatas (lo que usa HSPSND) sí" \
  "SUENA: HD Audio 00:03.0 códec 0, pin 03h" "Línea sugerida: HSPSND"; do has $R "$w" "informe: "; done
hasnt $R "NO AVANZA" "informe: "
hasnt $R "traducción de DMA apagada" "informe: "
if [ "$(sed -n '/=== RESUMEN ===/,$p' $R | grep -c 'VT-d: ')" = 2 ]; then ok "informe: el resumen lo dice una vez"; else ko "informe: el resumen lo dice una vez"; fi
entre aA ventanas 150 600 "suena"
entre aA hz 430 450 "a 440 Hz"

echo "== VT-d: AUDICION /SIM:VTB (entradas cerradas): apaga la traducción =="
R=$T/aB/rep.txt; audio aB
for w in "VT-d: TRADUCCION ENCENDIDA" "Entradas de contexto del bus 0 (dispositivo: parte alta, parte baja):" "    00:03.0: " \
  "VT-d: traducción de DMA apagada: sí" "VT-d: órdenes por CORB/RIRB (por DMA) después: sí" "SUENA: HD Audio 00:03.0"; do has $R "$w" "informe: "; done
hasnt $R "paso directo para HD Audio" "informe: "
entre aB ventanas 10 600 "suena"

echo "== VT-d: AUDICION sin la unidad (como antes del arreglo): no va =="
R=$T/a0/rep.txt; audio a0
has $R "VT-d (protección de DMA del firmware): MCHBAR=00000000h, unidad de la gráfica 00000000h, unidad general 00000000h" "informe: "
has $R "Las órdenes al códec: por CORB/RIRB NO; inmediatas (lo que usa HSPSND) sí" "informe: "
hasnt $R "TRADUCCION ENCENDIDA" "informe: "
entre a0 ventanas 0 0 "no suena nada"

rm -rf $T
if [ $fail = 0 ]; then echo "VT-d: todo bien"; else echo "VT-d: HAY FALLOS"; fi
exit $fail
