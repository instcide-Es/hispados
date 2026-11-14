#!/bin/bash
# Prueba de AUDICION (la prueba de sonido con preguntas de sí o no) en QEMU.
# tests/tools/audicion_drv.py hace de persona: contesta por la consola serie
# cuando ve cada pregunta. QEMU graba en un .wav lo que sale por la tarjeta:
# se comprueba que cuando se contesta «S» de verdad sonó (440 Hz), que el
# informe se crea con los volcados (se lee con mtools) y que el resumen
# cuadra con las respuestas.
#   dup    intel-hda + hda-duplex, todo que sí; segunda vez, Esc: SONIDO02.TXT
#   out    intel-hda + hda-output: R y N a la primera; suena a 44,1 kHz
#   ich9   ich9-intel-hda con el códec en la dirección 2: todo que no
#   micro  hda-micro (un altavoz)
#   ac97   Intel AC'97
#   sb16   Sound Blaster 16
#   nada   sin tarjeta: el altavoz del PC, /?, AYUDA y el informe en carpeta
#   hsp    con HSPSND cargado: avisa, pregunta y sigue
#   v86    con HSPEMM (modo V86): solo el altavoz del PC
#   sim1-4 /SIM: las variantes de GPIO, EAPD y «snoop», los auriculares y
#          «todo abierto»
#   mac1-2 [CIRRUS] /SIM:MAC: el códec Cirrus CS4206 del MacBook Air 2012
#          (simulado con su volcado real) y una controladora que a la primera
#          no arranca el flujo: todos los intentos tienen que avanzar; el
#          widget de proceso y las GPIO de Apple, primero; y HSPSND /SIMMAC
#          con ese códec (sin lo aprendido, con /NOGPIO y con lo aprendido)
#   alc1-3 [TOSHIBA] /SIM:ALC269: el Realtek ALC269 del Toshiba NB250
#          (lib/alcsim.asm, con su volcado real): el volcado sale igual que
#          el de Carlos, las órdenes de ganancia (por COM1, «ALCSIM ...») van
#          dentro de 0..87 y el «más bajo» es el paso 61 (19,5 dB menos); en
#          la grabación de QEMU el segundo tono baja de verdad (x0,106);
#          alc2 con la hipótesis «el DAC vuelve a 0 dB al programarle el
#          flujo» (/SIM:ALC269R): también baja; alc3 contesta como Carlos
#          (no baja) y HSPSND /SIMALC lo hace por programa
# AUDICION test: simulated answers over the serial console, recorded audio.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
JOBS=${JOBS:-3}
T=$(mktemp -d)
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
has() { if grep -qF -- "$2" "$1"; then ok "$3$2"; else ko "$3$2"; fi; }
hasnt() { if grep -qF -- "$2" "$1"; then ko "$3(no debe salir) $2"; else ok "$3no sale: $2"; fi; }

# run nombre respuestas [--cmd X] [--pre X]... -- dispositivos
run() {
  local n=$1 ans=$2; shift 2
  local pre=()
  while [ "$1" != "--" ]; do pre+=("$1"); shift; done
  shift
  mkdir -p $T/$n
  [ -f $T/$n/f.img ] || cp "$IMG" $T/$n/f.img
  python3 tests/tools/audicion_drv.py $T/$n/log "$ans" "${pre[@]}" -- \
    qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
    -audiodev wav,id=snd0,path=$T/$n/out.wav "$@" \
    -drive file=$T/$n/f.img,format=raw,if=floppy -boot a -no-reboot
  tr -d '\r' < $T/$n/log | iconv -f CP437 -t UTF-8 > $T/$n/log.txt
}
# informe nombre archivo -> $T/nombre/rep.txt (y rep.bin, tal cual)
informe() {
  MTOOLS_SKIP_CHECK=1 mtype -i $T/$1/f.img "::$2" > $T/$1/rep.bin 2>/dev/null
  tr -d '\r' < $T/$1/rep.bin | iconv -f CP437 -t UTF-8 > $T/$1/rep.txt
}
# audio nombre -> ventanas de 50 ms: cuántas hay de cada clase y la frecuencia
audio() {
  python3 - $T/$1/out.wav > $T/$1/audio.txt 2>&1 <<'EOF'
import sys, wave, struct
try:
    w = wave.open(sys.argv[1], 'rb')
except Exception as e:
    print("sin grabación:", e); sys.exit(0)
ch, rate, n = w.getnchannels(), w.getframerate(), w.getnframes()
raw = w.readframes(n)
v = struct.unpack('<%dh' % (len(raw) // 2), raw)
L, R = v[0::ch], v[1::ch] if ch > 1 else v[0::ch]
win = rate // 20
c = dict(ambos=0, bajo=0, izq=0, der=0, fuerte=0)
fr = []
for i in range(0, len(L) - win, win):
    l = max(abs(x) for x in L[i:i + win]); r = max(abs(x) for x in R[i:i + win])
    s = L[i:i + win] if l >= r else R[i:i + win]
    if max(l, r) < 300:
        continue
    zc = sum(1 for q in range(1, len(s)) if (s[q - 1] < 0) != (s[q] < 0))
    hz = zc / 2 / (len(s) / rate)
    fr.append(hz)
    if l > 6000 and r < 300: c['izq'] += 1
    elif r > 6000 and l < 300: c['der'] += 1
    elif 600 < l < 10500 and 600 < r < 10500: c['bajo'] += 1    # (QEMU baja el volumen en línea recta: 54/74)
    elif l > 10500 and r > 10500: c['ambos'] += 1
    if l > 20000: c['fuerte'] += 1
fr.sort()
med = fr[len(fr) // 2] if fr else 0
fuera = sum(1 for f in fr if abs(f - med) > 0.06 * med)
print("ventanas=%d ambos=%d bajo=%d izq=%d der=%d fuerte=%d hz=%d fuera=%d" % (len(fr), c['ambos'], c['bajo'], c['izq'], c['der'], c['fuerte'], med, fuera))
EOF
}
campo() { sed -n "s/.*$2=\([0-9]*\).*/\1/p" $T/$1/audio.txt; }
entre() { # nombre campo mín máx texto
  local v; v=$(campo $1 $2); v=${v:-0}
  if [ "$v" -ge "$3" ] && [ "$v" -le "$4" ]; then ok "$5 ($2=$v)"; else ko "$5 ($2=$v, se esperaba $3..$4)"; fi
}

HDA="-device intel-hda -device hda-duplex,audiodev=snd0"
SI="/segundo/=S;/Suena solo/=S;/entero/=S;/callado del todo/=S;/Enchufe/=S;/auriculares/=S"
lote() { while [ "$(jobs -rp | wc -l)" -ge "$JOBS" ]; do sleep 0.5; done; }
lote; run dup "default=S" -- $HDA &
lote; run out "default=S;P02=RN" -- -device intel-hda -device hda-output,audiodev=snd0 &
lote; run ich9 "default=N;P01=S" -- -device ich9-intel-hda -device hda-duplex,audiodev=snd0,cad=2 &
lote; run micro "default=S" -- -device intel-hda -device hda-micro,audiodev=snd0 &
lote; run ac97 "default=S" -- -device AC97,audiodev=snd0 &
lote; run sb16 "default=S" -- -device sb16,audiodev=snd0 &
lote; run nada "default=S" --pre 'AUDICION /?' --pre 'AYUDA AUDICION' --pre 'AYUDA' --cmd 'AUDICION A:\DOCS' -- -machine pcspk-audiodev=snd0 &
lote; run sim1 "default=N;P01=S;P07=S;$SI" --cmd 'AUDICION /SIM' -- $HDA &
lote; run sim2 "default=N;P10=S" --cmd 'AUDICION /sim' -- $HDA &
lote; run sim3 "default=N;P03=S" --cmd 'AUDICION /SIM' -- $HDA &
lote; run sim4 "default=N;P13=S;/segundo/=S" --cmd 'AUDICION /SIM' -- $HDA &
# [CIRRUS] el MacBook Air 2012 de Carlos, simulado
lote; run mac1 "default=N;P01=S" --cmd 'AUDICION /SIM:MAC' --post 'A:\HISPADOS\HSPSND /SIMMAC /INFO' --post 'A:\HISPADOS\HSPSND /SIMMAC /INFO /NOGPIO' -- $HDA &
lote; run mac2 "default=S" --cmd 'AUDICION /SIM:MAC' --post 'A:\HISPADOS\HSPSND /SIMMAC /INFO' -- $HDA &
# [TOSHIBA] el Toshiba NB250 de Carlos (Realtek ALC269), simulado
lote; run alc1 "default=S" --cmd 'AUDICION /SIM:ALC269' --post 'A:\HISPADOS\HSPSND /SIMALC /INFO' -- $HDA &
lote; run alc2 "default=S" --cmd 'AUDICION /SIM:ALC269R' -- $HDA &
lote; run alc3 "default=S;P03=N;P04=N" --cmd 'AUDICION /SIM:ALC269' --post 'A:\HISPADOS\HSPSND /SIMALC /INFO' -- $HDA &
# con HSPSND cargado (avisa y sigue) y con HSPEMM (modo V86: no hay HD Audio)
lote; run hsp "default=S;P02=E" --pre 'A:\HISPADOS\HSPSND' -- $HDA &
mkdir -p $T/v86; cp "$IMG" $T/v86/f.img
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDEVICE=A:\\HISPADOS\\HSPEMM.SYS\r\n' > $T/v86/CONFIG.SYS
mcopy -o -i $T/v86/f.img $T/v86/CONFIG.SYS ::CONFIG.SYS
lote; run v86 "default=S" -- $HDA &
wait
# segunda pasada sobre el mismo disquete: no sobrescribe, y Esc guarda
cp $T/dup/log.txt $T/dup/log1.txt; cp $T/dup/out.wav $T/dup/out1.wav
run dup "default=S;P02=E" -- $HDA
cp $T/dup/log.txt $T/dup/log2.txt; cp $T/dup/out1.wav $T/dup/out.wav

echo "== AUDICION: Intel HD Audio (hda-duplex), todo que sí =="
L=$T/dup/log1.txt
informe dup SONIDO01.TXT; R=$T/dup/rep.txt
has $L "AUDICION 1.0 - prueba de sonido de Hispa-DOS" ""
has $L "Se oirán sonidos. Después de cada uno, pulse:" ""
has $L "R  para repetirlo" ""
has $L "P01. ¿Ha oído el pitido? (S/N/R) S" ""
has $L "P02. ¿Lo oye? (S/N/R) S" ""
has $L "Informe guardado en A:\\SONIDO01.TXT. Súbalo al FTP en HispaDOS/informes." ""
for w in "INFORME DE SONIDO DE HISPA-DOS" "Fecha: " "BIOS (fabricante | versión | fecha): SeaBIOS" "Equipo (fabricante | modelo | versión): QEMU" \
  "Procesador: " "KB convencional" "PCI 00:04.0  8086:2668  subsistema 1AF4:1100  clase 0403 (HD Audio)" "Intel: TCSEL (44h) = " \
  "BAR: FEBB0000" "   F0: " "GCAP=4401h" "Las órdenes al códec: por CORB/RIRB sí; inmediatas (lo que usa HSPSND) sí" \
  "Identificador (fabricante:dispositivo) 1AF4:0022 (QEMU)" "Grupo de función 01h: tipo 01h (audio)" \
  "GPIO 00000000h: 0 GPIO, 0 GPO, 0 GPI" "Nodos: del 02h, 4 en total" \
  "Nodo 02h [Salida de audio (DAC)] caps 0000001Dh: estéreo amp-salida amp-propio formato-propio" \
  "Amp. salida: 80034A4Ah (0 dB en el paso 74, 74 pasos de 4/4 dB, silencio sí)" \
  "PCM 000201FCh: 16000 22050 32000 44100 48000 88200 96000 Hz; bits: 16" \
  "Nodo 03h [Pin] caps 00400101h: estéreo lista-conexiones" "Capacidades del pin 00000010h: SALIDA" \
  "conector externo | salida de línea | exterior" "color verde | asociación 1 secuencia 0" "Conexiones (1): 02h*" \
  "Nodo 05h [Pin]" "entrada de línea" "=== SOUND BLASTER (ISA) ===" "=== ALTAVOZ DEL PC ===" \
  "Intento: HD Audio 00:04.0 códec 0, pin 03h (salida de línea), ruta 03h 02h (el último es el DAC), control del pin 40h, EAPD no hay, GPIO no hay, 48000 Hz 16 bits estéreo" \
  "Amplificadores a 0 dB y sin silencio: 02h/salida=paso 74" "(la posición del flujo avanza)" \
  "Volumen: amplificador del nodo 02h (salida), 0 dB en el paso 74 de 74, silencio sí" \
  "=== RESUMEN ===" "Altavoz del PC (pitido): sí" "SUENA: HD Audio 00:04.0 códec 0, pin 03h" "  Bajar el volumen: sí" \
  "  Silencio: sí" "  Solo izquierda: sí" "  Solo derecha: sí" "  8000 Hz: el DAC no la admite" "  22050 Hz suena igual de agudo: sí" \
  "  44100 Hz suena igual de agudo: sí" "  Sonido largo (5 s) sin cortes: sí" "Línea sugerida: HSPSND" \
  "Con esa línea HSPSND debería sonar en este equipo."; do has $R "$w" "informe: "; done
hasnt $R "FALTA en HSPSND" "informe: "
hasnt $R "sin respuesta a la orden" "informe: "
hasnt $R "NO AVANZA" "informe: "
if [ "$(grep -c $'\r$' $T/dup/rep.bin)" = "$(wc -l < $T/dup/rep.bin)" ] && [ -s $T/dup/rep.bin ]; then ok "informe: todas las líneas acaban en CR+LF"; else ko "informe: líneas sin CR+LF"; fi
if LC_ALL=C grep -q $'\xa8Lo oye?' $T/dup/rep.bin; then ok "informe: texto en CP437"; else ko "informe: texto en CP437"; fi
audio dup
entre dup ambos 150 400 "suena el tono por los dos canales"
entre dup hz 430 450 "a 440 Hz"
entre dup fuera 0 6 "todas las frecuencias de muestreo dan el mismo tono"
entre dup bajo 30 60 "la prueba de volumen baja el volumen (dos segundos más bajo)"
entre dup izq 15 30 "solo izquierda"
entre dup der 15 30 "solo derecha"
# segunda pasada: SONIDO02.TXT, con Esc; el primero, intacto
has $T/dup/log2.txt "(Esc: prueba terminada antes de acabar)" "2.ª vez: "
has $T/dup/log2.txt "Informe guardado en A:\\SONIDO02.TXT." "2.ª vez: "
cmp -s <(MTOOLS_SKIP_CHECK=1 mtype -i $T/dup/f.img ::SONIDO01.TXT 2>/dev/null) $T/dup/rep.bin && ok "2.ª vez: SONIDO01.TXT no se ha tocado" || ko "2.ª vez: SONIDO01.TXT ha cambiado"
informe dup SONIDO02.TXT
has $R "P01. ¿Ha oído el pitido? (S/N/R) S" "SONIDO02.TXT: "
has $R "(La prueba se terminó con Esc: lo que no sale arriba no se ha probado.)" "SONIDO02.TXT: "
hasnt $R "NINGUNA salida analógica" "SONIDO02.TXT: "

echo "== AUDICION: hda-output; R y N a la primera, suena a 44,1 kHz =="
L=$T/out/log.txt; informe out SONIDO01.TXT; R=$T/out/rep.txt
has $L "P02. ¿Lo oye? (S/N/R) R" ""
has $R "P02. ¿Lo oye? (S/N/R) N  (repetido 1)" "informe: "
has $R "P03. ¿Lo oye? (S/N/R) S" "informe: "
has $R "Identificador (fabricante:dispositivo) 1AF4:0012 (QEMU)" "informe: "
has $R "SUENA: HD Audio 00:04.0 códec 0, pin 03h (salida de línea), ruta 03h 02h (el último es el DAC), control del pin 40h, EAPD no hay, GPIO no hay, 44100 Hz 16 bits estéreo" "informe: "
has $R "  48000 Hz suena igual de agudo: sí" "informe: "
has $R "FALTA en HSPSND: solo suena a 44100 Hz (a 48000 Hz no)." "informe: "
has $R "ATENCION: el camino que suena necesita lo marcado con FALTA" "informe: "
audio out; entre out hz 430 450 "suena a 440 Hz"; entre out ambos 150 400 "por los dos canales"

echo "== AUDICION: ICH9 con el códec en la dirección 2, todo que no =="
L=$T/ich9/log.txt; informe ich9 SONIDO01.TXT; R=$T/ich9/rep.txt
has $L "HD Audio 00:04.0, códec 2: 1AF40022 (QEMU)" ""
has $L "(todas las salidas a la vez)" ""
has $R "8086:293E" "informe: "
has $R "--- Códec 2 de 00:04.0 ---" "informe: "
has $R "Intento: HD Audio 00:04.0 códec 2, pin 03h" "informe: "
has $R "44100 Hz 16 bits estéreo" "informe: "
has $R "Intento: HD Audio 00:04.0 códec 2, TODO ABIERTO (todas las salidas, todos los DAC, EAPD encendido), GPIO no hay" "informe: "
has $R "NO suena: HD Audio 00:04.0 códec 2 (3 intentos)" "informe: "
has $R "HD Audio: NINGUNA salida analógica ha sonado con las combinaciones probadas." "informe: "
hasnt $R "NO AVANZA" "informe: "
hasnt $R "SUENA:" "informe: "
hasnt $R "Línea sugerida" "informe: "
audio ich9; entre ich9 hz 430 450 "aun así el tono salió (440 Hz)"; entre ich9 ambos 45 75 "tres intentos de 1 s"

echo "== AUDICION: hda-micro (un altavoz) =="
L=$T/micro/log.txt; informe micro SONIDO01.TXT; R=$T/micro/rep.txt
has $L "  Salida: altavoz" ""
has $R "| ALTAVOZ |" "informe: "
has $R "SUENA: HD Audio 00:04.0 códec 0, pin 03h (altavoz" "informe: "
has $R "Línea sugerida: HSPSND" "informe: "
audio micro; entre micro hz 430 450 "suena a 440 Hz"

echo "== AUDICION: Intel AC'97 =="
L=$T/ac97/log.txt; informe ac97 SONIDO01.TXT; R=$T/ac97/rep.txt
has $L "AC'97 00:04.0, códec 8384:7600" ""
has $L "Ahora, la tarjeta de sonido AC'97." ""
has $R "8086:2415" "informe: "
has $R "clase 0401 (audio / AC'97)" "informe: "
has $R "Códec: 8384:7600 (" "informe: "
has $R "Registros del mezclador (00h-7Eh):" "informe: "
has $R "  70: " "informe: "
has $R "(la posición del flujo avanza)" "informe: "
has $R "AC'97: suena: sí" "informe: "
audio ac97; entre ac97 hz 425 455 "suena a 440 Hz"; entre ac97 ventanas 15 30 "un segundo"

echo "== AUDICION: Sound Blaster 16 =="
L=$T/sb16/log.txt; informe sb16 SONIDO01.TXT; R=$T/sb16/rep.txt
has $L "Sound Blaster en 0220h, DSP 4.05, DMA 1" ""
has $L "Ahora, la Sound Blaster." ""
has $R "Mezclador: 04=" "informe: "
has $R " 80=" "informe: "
has $R "Sound Blaster: suena: sí" "informe: "
audio sb16; entre sb16 hz 425 455 "suena a 440 Hz"; entre sb16 ventanas 15 30 "un segundo"

echo "== AUDICION: sin tarjeta; /?, AYUDA y el informe en una carpeta =="
L=$T/nada/log.txt
n=$(grep -c "AUDICION \[carpeta\]" $L)
if [ "$n" -ge 2 ]; then ok "AUDICION /? y AYUDA AUDICION"; else ko "AUDICION /? y AYUDA AUDICION ($n)"; fi
has $L "AUDICION (prueba de sonido con preguntas de sí o no)." "AYUDA: "
has $L "No se ha encontrado ninguna tarjeta de sonido." ""
has $L "Informe guardado en A:\\DOCS\\SONIDO.TXT." ""
informe nada DOCS/SONIDO.TXT; R=$T/nada/rep.txt
has $R "Fichero: A:\\DOCS\\SONIDO.TXT" "informe: "
has $R "Ninguna (clase 04h, subclases 01h, 03h y 80h)." "informe: "
has $R "Altavoz del PC (pitido): sí" "informe: "
hasnt $R "HD Audio" "informe: "
if MTOOLS_SKIP_CHECK=1 mdir -i $T/nada/f.img ::SONIDO01.TXT > /dev/null 2>&1; then ko "con carpeta no se escribe en la raíz"; else ok "con carpeta no se escribe en la raíz"; fi
audio nada; entre nada hz 860 900 "el pitido del altavoz del PC (880 Hz)"; entre nada ventanas 8 12 "medio segundo"

echo "== AUDICION /SIM: variantes de GPIO, EAPD y «snoop»; auriculares =="
informe sim1 SONIDO01.TXT; R=$T/sim1/rep.txt
has $R "Códec 0: 3 pines con salida; GPIO: 3" "sim1: "
has $R "Pin 03h: nada enchufado o sin conexión; no se prueba ahora." "sim1: "
has $R "control del pin C0h, EAPD encendido, GPIO máscara 07h dirección 07h datos 07h, 48000 Hz 16 bits estéreo" "sim1: "
has $R "«snoop» activado (PCI 42h)" "sim1: "
has $R "GPIO como la BIOS: máscara 00h dirección 00h datos 00h" "sim1: "
has $R "GPIO máscara 07h dirección 07h datos 00h" "sim1: "
has $R "GPIO máscara 07h dirección 07h datos 01h" "sim1: "
has $R "P07. ¿Lo oye? (S/N/R) S" "sim1: "
has $R "Línea sugerida: HSPSND /GPIO:02" "sim1: "
has $R "P16. Enchufe unos auriculares si tiene. ¿Los ha enchufado? (S/N) S" "sim1: "
has $R "Pin 03h: presencia antes 0, después 0" "sim1: "
has $R "Detección de auriculares: NO cambia al enchufar" "sim1: "
has $R "Auriculares: suenan por el pin 03h" "sim1: "
has $R "Altavoz: control del pin 03h puesto a 00h" "sim1: "
has $R "  El altavoz calla al apagar su pin: sí" "sim1: "
hasnt $R "FALTA en HSPSND" "sim1: "
informe sim2 SONIDO01.TXT; R=$T/sim2/rep.txt
has $R "EAPD APAGADO, GPIO como la BIOS: máscara 00h dirección 00h datos 00h" "sim2: "
has $R "Línea sugerida: HSPSND /NOGPIO /EAPD:0" "sim2: "
has $R "  Bajar el volumen: NO" "sim2: "
has $R "Auriculares: no se han probado (no se enchufaron)" "sim2: "
informe sim3 SONIDO01.TXT; R=$T/sim3/rep.txt
has $R "FALTA en HSPSND: activar «snoop» (registro PCI 42h) en AMD/ATI." "sim3: "
has $R "ATENCION: el camino que suena necesita lo marcado con FALTA" "sim3: "
# suena solo con todo abierto: se busca la salida una a una y, como ninguna
# suena por separado, se sigue con todo abierto y el volumen por programa
informe sim4 SONIDO01.TXT; R=$T/sim4/rep.txt
has $R "P13. ¿Lo oye? (S/N/R) S" "sim4: "
has $R "SUENA: HD Audio 00:04.0 códec 0, TODO ABIERTO" "sim4: "
has $R "Suena con todo abierto: se prueban una a una las salidas sin declarar." "sim4: "
has $R "Intento: HD Audio 00:04.0 códec 0, pin 03h (otra salida)" "sim4: "
has $R "P14. ¿Lo oye? (S/N/R) N" "sim4: "
has $R "(ninguna salida sin declarar suena por separado: se sigue con todo abierto)" "sim4: "
has $R "Volumen: el camino no tiene amplificador regulable; se prueba por programa" "sim4: "
has $R "FALTA en HSPSND: solo ha sonado con TODO ABIERTO" "sim4: "
audio sim4; entre sim4 bajo 30 60 "sim4: el volumen por programa baja (dos segundos más bajo)"; entre sim4 fuerte 15 30 "sim4: y sube"

echo "== [CIRRUS] AUDICION /SIM:MAC: el Cirrus CS4206 del MacBook Air 2012, todo que no =="
informe mac1 SONIDO01.TXT; R=$T/mac1/rep.txt; L=$T/mac1/log.txt
for w in "Identificador (fabricante:dispositivo) 1013:4206 (Cirrus Logic)  revisión 00100302h" \
  "Grupo de audio 01h: subsistema 106B5600h" "GPIO ahora: máscara 08h dirección 08h datos 00h" \
  "Nodo 0Bh [Pin] caps 00410101h" "Configuración por defecto 90100120h: fijo (interno) | ALTAVOZ" \
  "Nodo 11h [Del fabricante] caps 00F00040h: proceso" "   00: 001E 0008 000A 106A 0007 0000 0000 0000" \
  "Cirrus Logic CS4206 de Apple (subsistema del códec 106B5600h): widget de proceso 11h puesto en marcha (estado 01h, DAC (coef. 03h) 146Ah)" \
  "Amplificador de los altavoces por la GPIO 3; el de los auriculares, por la GPIO 1 (se prueba primero)" \
  "Intento: HD Audio 00:04.0 códec 0, pin 0Bh (altavoz interno), ruta 0Bh 04h (el último es el DAC), control del pin 40h, EAPD no hay, GPIO de Apple (los amplificadores): máscara 0Ah dirección 0Ah datos 0Ah, 48000 Hz 16 bits estéreo" \
  "GPIO como la BIOS: máscara 08h dirección 08h datos 00h" "GPIO máscara 0Fh dirección 0Fh datos 08h" \
  "(a la primera no arrancó; se ha reiniciado el flujo y a la segunda sí." \
  "NO suena: HD Audio 00:04.0 códec 0 (9 intentos)"; do has $R "$w" "mac1: "; done
hasnt $R "NO AVANZA" "mac1: "
hasnt $R "AVISO: en algún intento la posición del flujo no avanzó" "mac1: "
# el primer intento, con las GPIO de Apple
if grep -m1 "^Intento:" $R | grep -q "GPIO de Apple"; then ok "mac1: las GPIO de Apple, las primeras"; else ko "mac1: las GPIO de Apple, las primeras"; fi
n=$(grep -c "^Intento: .*Hz 16 bits estéreo$" $R); a=$(grep -c "(la posición del flujo avanza)" $R)
if [ "$n" -ge 9 ] && [ "$a" = "$n" ]; then ok "mac1: los $n intentos seguidos avanzan todos"; else ko "mac1: intentos $n, avanzan $a"; fi
audio mac1; entre mac1 hz 430 450 "mac1: el tono sale por QEMU (440 Hz)"
has $L "Cirrus CS4206 de Apple (subsistema del códec 106B:5600): amplificador de los altavoces por la GPIO 3 y el de los auriculares por la GPIO 1; widget de proceso 11h puesto en marcha." "HSPSND /SIMMAC: "
has $L "  GPIO: las de los amplificadores, encendidos (máscara, dirección y datos 0Ah)." "HSPSND /SIMMAC: "
has $L "subsistema 106B5600, GPIO 4: datos 0A másc. 0A dir. 0A" "HSPSND /SIMMAC: "
has $L "Intel HD Audio en FEBB0000, códec 0, salidas: 0B 09" "HSPSND /SIMMAC: "
has $L "Pin 0B: altavoz, interno (90100120) -> 0B>04" "HSPSND /SIMMAC: "
has $L "  GPIO: sin tocar (/NOGPIO)." "HSPSND /SIMMAC /NOGPIO: "
has $L "subsistema 106B5600, GPIO 4: datos 00 másc. 08 dir. 08" "HSPSND /SIMMAC /NOGPIO: "
hasnt $L "Configuración aprendida en la prueba" "HSPSND /SIMMAC (no sonó nada): "
hasnt $L "Ya está instalado" "HSPSND /SIMMAC no queda residente: "

echo "== [CIRRUS] AUDICION /SIM:MAC: suena a la primera; lo aprendido, a HSPSND =="
informe mac2 SONIDO01.TXT; R=$T/mac2/rep.txt; L=$T/mac2/log.txt
has $R "P02. ¿Lo oye? (S/N/R) S" "mac2: "
has $R "SUENA: HD Audio 00:04.0 códec 0, pin 0Bh (altavoz interno), ruta 0Bh 04h (el último es el DAC), control del pin 40h, EAPD no hay, GPIO de Apple (los amplificadores): máscara 0Ah dirección 0Ah datos 0Ah, 48000 Hz 16 bits estéreo" "mac2: "
has $R "Auriculares: suenan por el pin 09h" "mac2: "
if grep -qx "Línea sugerida: HSPSND" $R; then ok "mac2: Línea sugerida: HSPSND (sin /GPIO: ya lo sabe)"; else ko "mac2: Línea sugerida: HSPSND (sin /GPIO)"; fi
hasnt $R "NO AVANZA" "mac2: "
MTOOLS_SKIP_CHECK=1 mtype -i $T/mac2/f.img ::HISPADOS/SONIDO.INI 2>/dev/null | tr -d '\r' | iconv -f CP437 -t UTF-8 > $T/mac2/ini.txt
has $T/mac2/ini.txt "[HDA 8086:2668 1AF4:1100 1013:4206 106B:5600]" "mac2: SONIDO.INI: "
has $T/mac2/ini.txt "GPIO=0Ah 0Ah 0Ah" "mac2: SONIDO.INI: "
has $L "Configuración aprendida en la prueba (" "HSPSND /SIMMAC con lo aprendido: "
has $L "  GPIO: las de la prueba de sonido (SONIDO.INI)." "HSPSND /SIMMAC con lo aprendido: "
has $L "widget de proceso 11h puesto en marcha." "HSPSND /SIMMAC con lo aprendido: "
has $L "  EAPD como siempre, GPIO máscara 0Ah dirección 0Ah datos 0Ah" "HSPSND /SIMMAC con lo aprendido: "
audio mac2; entre mac2 hz 430 450 "mac2: suena a 440 Hz"

echo "== [TOSHIBA] AUDICION /SIM:ALC269: el Realtek ALC269 del Toshiba NB250 =="
# bajada nombre texto: en la grabación, tramos al nivel normal y al «más
# bajo»: el bajo tiene que ser 19,5 dB menos (x0,106: entre 0,08 y 0,14)
bajada() {
  python3 - $T/$1/out.wav "$2" <<'EOF' || fail=1
import sys, wave, struct
w = wave.open(sys.argv[1], 'rb')
ch, rate, n = w.getnchannels(), w.getframerate(), w.getnframes()
v = struct.unpack('<%dh' % (n * ch), w.readframes(n))
L = v[0::ch]
win = rate // 20
p = [max(abs(x) for x in L[i:i + win]) for i in range(0, n - win, win)]
m = sorted(p)[len(p) * 3 // 4] if p else 0
bajo = sorted(x for x in p if 0.05 * m < x < 0.2 * m)
r = bajo[len(bajo) // 2] / m if bajo and m else 0
bien = m > 8000 and len(bajo) >= 30 and 0.08 < r < 0.14
print("  %s %s (normal %d, bajo %s en %d ventanas: x%.3f)" % ("OK  " if bien else "FAIL", sys.argv[2], m,
      bajo[len(bajo) // 2] if bajo else '-', len(bajo), r))
sys.exit(0 if bien else 1)
EOF
}
# ganancia nombre: las órdenes de ganancia (3h) a la salida del nodo 02h ->
# sus pasos; todos dentro de 0..87 (57h) y el «más bajo», 61 (3Dh)
ganancia() {
  local g; g=$(sed -n 's/^ALCSIM 0023[89AB]0\([0-9A-F][0-9A-F]\)$/\1/p' $T/$1/log.txt | sort -u | tr '\n' ' ')
  local malos=0 x
  for x in $g; do [ $((16#$x & 0x7F)) -le 87 ] || malos=1; done
  if [ -n "$g" ] && [ $malos = 0 ]; then ok "$1: las ganancias del DAC 02h, dentro de 0..87 ($g)"; else ko "$1: ganancias del DAC 02h: «$g»"; fi
  if echo " $g" | grep -q " 3D "; then ok "$1: el «más bajo» es el paso 61 (3Dh)"; else ko "$1: no se manda el paso 61"; fi
}
informe alc1 SONIDO01.TXT; R=$T/alc1/rep.txt; L=$T/alc1/log.txt
for w in "Identificador (fabricante:dispositivo) 10EC:0269 (Realtek)  revisión 00100100h" \
  "Grupo de audio 01h: subsistema 1179FF6Ch  capacidades 00010F0Fh" "Nodos: del 02h, 34 en total" \
  "Amp. salida: 00025757h (0 dB en el paso 87, 87 pasos de 3/4 dB, silencio NO)  ahora izq/der 57/57" \
  "Nodo 14h [Pin] caps 0040018Dh" "Conexiones (2): 0Ch* 0Dh" "Conexiones (7): 18h* 19h 1Ah 1Bh 1Dh 0Bh 12h" \
  "   08: 0310 0000 BBCC BBCC 0210 4040 8814 B60B" \
  "Intento: HD Audio 00:04.0 códec 0, pin 14h (altavoz interno), ruta 14h 0Ch 02h (el último es el DAC), control del pin 40h, EAPD encendido, GPIO máscara 03h dirección 03h datos 03h, 48000 Hz 16 bits estéreo" \
  "Amplificadores a 0 dB y sin silencio: 02h/salida=paso 87 0Ch/entrada 0=paso 0 14h/salida=paso 0" \
  "Volumen: amplificador del nodo 02h (salida), 0 dB en el paso 87 de 87, silencio NO" \
  "  Comprobación: escrito el paso 61 (19,5 dB menos); el códec dice 3Dh/3Dh al escribirlo, 3Dh/3Dh con el flujo ya programado y 3Dh/3Dh al volver a escribirlo" \
  "  Bajar el volumen: sí" "  Subir el volumen: sí"; do has $R "$w" "alc1: "; done
hasnt $R "NO GUARDA" "alc1: "
ganancia alc1
bajada alc1 "alc1: en QEMU, el «más bajo» suena 19,5 dB más bajo"
has $L "Pin 14: altavoz, interno (99131110) EAPD=1 -> 14>0C>02" "HSPSND /SIMALC: "
has $L "Configuración aprendida en la prueba (" "HSPSND /SIMALC con lo aprendido: "
has $L "Amplificador de volumen: nodo 02 (salida), 0 dB=57 de 57 pasos de 0,75 dB" "HSPSND /SIMALC: "
informe alc2 SONIDO01.TXT; R=$T/alc2/rep.txt
has $R "  Comprobación: escrito el paso 61 (19,5 dB menos); el códec dice 3Dh/3Dh al escribirlo, 57h/57h con el flujo ya programado y 3Dh/3Dh al volver a escribirlo" "alc2: "
has $R "  (al programar el flujo el códec lo ha cambiado: se vuelve a escribir cada vez)" "alc2: "
ganancia alc2
bajada alc2 "alc2 (el DAC vuelve a 0 dB al programarle el flujo): baja igual"
informe alc3 SONIDO01.TXT; R=$T/alc3/rep.txt; L=$T/alc3/log.txt
has $R "  Bajar el volumen: NO" "alc3: "
MTOOLS_SKIP_CHECK=1 mtype -i $T/alc3/f.img ::HISPADOS/SONIDO.INI 2>/dev/null | tr -d '\r' | iconv -f CP437 -t UTF-8 > $T/alc3/ini.txt
has $T/alc3/ini.txt "[HDA 8086:2668 1AF4:1100 10EC:0269 1179:FF6C]" "alc3: SONIDO.INI: "
has $T/alc3/ini.txt "Volumen=por programa" "alc3: SONIDO.INI: "
has $L "Configuración aprendida en la prueba (" "HSPSND /SIMALC con lo aprendido: "
has $L ", volumen por programa" "HSPSND /SIMALC con «Volumen=por programa»: "

echo "== AUDICION con HSPSND cargado y en modo V86 (HSPEMM) =="
L=$T/hsp/log.txt; informe hsp SONIDO01.TXT; R=$T/hsp/rep.txt
has $L "HSPSND está cargado. Esta prueba maneja la tarjeta directamente y" "HSPSND: "
has $L "¿Continuar de todos modos? (S/N) S" "HSPSND: "
has $L "HSPSND estaba cargado: reinicie el equipo para volver a usarlo." "HSPSND: "
has $R "HSPSND estaba cargado (tarjeta tipo 4); se le ha pedido parar." "HSPSND: "
has $R "Identificador (fabricante:dispositivo) 1AF4:0022 (QEMU)" "HSPSND: "
has $R "Intento: HD Audio 00:04.0 códec 0, pin 03h" "HSPSND: "
audio hsp; entre hsp hz 430 450 "HSPSND: el tono suena igual (440 Hz)"
L=$T/v86/log.txt; informe v86 SONIDO01.TXT; R=$T/v86/rep.txt
has $L "HSPEMM" "V86: "
has $R "Procesador en modo V86 (hay un gestor de memoria tipo EMM386 cargado):" "V86: "
has $R "PCI 00:04.0  8086:2668" "V86: "
has $R "Altavoz del PC (pitido): sí" "V86: "
has $R "HD Audio: no se puede probar en modo V86 (arranque sin HSPEMM)." "V86: "
hasnt $R "Intento:" "V86: "
has $L "Informe guardado en A:\\SONIDO01.TXT." "V86: "

if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then
  for n in dup out ich9 micro ac97 sb16 nada sim1 sim2 sim3 sim4 mac1 mac2 alc1 alc2 alc3 hsp v86; do
    echo "---- $n ----"; cat $T/$n/log.txt; cat $T/$n/audio.txt 2>/dev/null
    MTOOLS_SKIP_CHECK=1 mtype -i $T/$n/f.img ::SONIDO01.TXT 2>/dev/null | tr -d '\r' | iconv -f CP437 -t UTF-8 | grep -v '^   [0-9A-F][0-9A-F]: '
  done
fi
rm -rf $T
exit $fail
