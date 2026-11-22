# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pruebas_reproductor.sh - la parte 33 (SOLO=reproductor, el Reproductor) de
# pruebas.sh, que lee este archivo («.»): la función parte_reproductor, con
# los ayudantes de pruebas.sh (ok, ko, mira, miracap) y sus carpetas ($AQUI,
# $P, $G). Todo lo que suena es nuestro: tonos, escalas y acordes que hace
# nucleo32/reproductor/pruebas/haz_pruebas.py (los MP3, OGG, FLAC y Opus los
# codifica el ffmpeg del anfitrión, que también da la decodificación de
# referencia: solo para comparar).
#
#  1. En el anfitrión (con ASan y UBSan): cada formato decodificado frente a
#     ffmpeg; sin huecos entre dos mitades de un seno (WAV, FLAC, MP3 y OGG);
#     la imagen .BIN de CD con su .CUE y el FLAC entero con su .CUE; buscar;
#     el ecualizador, el remuestreador y la velocidad con tonos; las
#     etiquetas (ID3v2, Vorbis, FLAC; la carátula y ReplayGain); las listas;
#     y el fuzz de los decodificadores, las etiquetas y las listas.
#  2. REPRODUC.BIN (hacer.sh): sin CMOV ni FCOMI; y el paquete «reproduc».
#  3. En QEMU, desde la Consola, con las tres tarjetas (HD Audio, AC'97 y la
#     Sound Blaster 16): lo que sale por la tarjeta (-audiodev wav) es lo que
#     hace el Reproductor en el anfitrión, entero y sin un corte
#     (pruebas/sigue.py).
#  4. Lo que cuesta decodificar (REPRODUCE /MIDE) en un Atom N270 de QEMU
#     (-cpu n270 con -icount: un procesador de mil millones de instrucciones
#     por segundo, como el Atom N455 del HP Mini 5103).
#  5. El CD: READ TOC y READ CD (la llamada 134) por IDE y por SATA (AHCI),
#     en crudo sobre un CD de datos (QEMU no tiene CD de música): lo de
#     dentro de cada sector es lo de la imagen.
#  6. En el escritorio: la ventana (la pantalla, los botones, la lista), el
#     icono de la bandeja (la llamada 135), el modo pequeño, y que no se
#     corte con otro programa gastando todo el procesador y la ventana
#     arrastrándose por la pantalla.

# sigue_bien NOMBRE GRABADO REFERENCIA: lo grabado es la referencia entera,
# sin saltos (pruebas/sigue.py)
sigue_bien() {
  local q=$1 g=$2 r=$3 l
  l=$(python3 -I $AQUI/nucleo32/reproductor/pruebas/sigue.py $g $r 2>&1)
  echo "$q: $l" >> $Y/sigue.txt
  if echo "$l" | awk '{ for (i = 1; i <= NF; i++) { split($i, x, "="); v[x[1]] = x[2] } } END { exit !(v["trozos"] >= 20 && v["saltos"] == 0 && v["peor_correlacion"] >= 0.999) }'; then
    ok "$q: lo que sale por la tarjeta es lo del anfitrión, sin un corte ($l)"
  else ko "$q: $l"; fi
}

parte_reproductor() {
echo "== (REPRODUCTOR) Hispa-Pentium: el Reproductor (REPRODUC.BIN, la orden REPRODUCE, las llamadas 134 a 136) =="
local Y=$P/repro R C F l n f s
local RD=$AQUI/nucleo32/reproductor
mkdir -p $Y $G
: > $Y/sigue.txt
export MTOOLS_SKIP_CHECK=1
# ---- 1. en el anfitrión ----
R=$Y/repro-anfitrion
if ! bash $RD/anfitrion.sh $R asan > $Y/anfitrion.txt 2>&1; then ko "anfitrion.sh no compila: $(tail -n 3 $Y/anfitrion.txt)"; return; fi
F=$Y/f
if ! python3 -I $RD/pruebas/haz_pruebas.py $F > $Y/haz.txt 2>&1; then ko "haz_pruebas.py: $(tail -n 2 $Y/haz.txt)"; return; fi
C="python3 -I $RD/pruebas/compara.py"
export ASAN_OPTIONS=detect_leaks=0
# cada formato frente a ffmpeg (a su frecuencia, en 16 bits): los sin
# pérdida, igual octeto a octeto; los otros, casi (la coma flotante)
for f in escala.wav:999:WAV esc_24.wav:999:"WAV de 24 bits" esc_f32.wav:999:"WAV de coma flotante" esc_8.wav:999:"WAV de 8 bits mono a 22 kHz" \
         esc_ms.wav:999:"WAV MS-ADPCM" esc_ima.wav:40:"WAV IMA-ADPCM" escala.flac:999:FLAC esc_car.flac:999:"FLAC con carátula" \
         escala.mp3:90:"MP3 de 192 kbit/s" esc_vbr.mp3:90:"MP3 de tasa variable (con Xing y LAME)" escala.mp2:70:MP2 \
         escala.ogg:90:"OGG Vorbis" esc_48.ogg:90:"OGG Vorbis a 48 kHz"; do
  n=${f%%:*}; s=${f#*:}; s=${s%%:*}; q=${f##*:}
  $R decodifica $F/$n $Y/o.wav > $Y/dec.txt 2>&1
  ffmpeg -hide_banner -loglevel error -y -i $F/$n -map 0:a -c:a pcm_s16le $Y/ref.wav
  l=$($C $Y/o.wav $Y/ref.wav --desfase 600)
  if echo "$l" | awk -v s=$s '{ for (i = 1; i <= NF; i++) { split($i, x, "="); v[x[1]] = x[2] } } END { split(v["tramas"], t, "/"); exit !(t[1] == t[2] && v["desfase"] == 0 && v["snr"] + 0 >= s) }'; then
    ok "anfitrión: $q, como ffmpeg (las mismas tramas; $l)"
  else ko "anfitrión: $q: $l (se esperaba una SNR de $s dB o más)"; fi
done
# (lo de seis canales, mezclado a dos como dice decod.c)
$R decodifica $F/esc_51.flac $Y/o.wav > /dev/null 2>&1
ffmpeg -hide_banner -loglevel error -y -i $F/esc_51.flac -af "pan=stereo|FL=0.5*FL+0.3536*FC+0.3536*BL|FR=0.5*FR+0.3536*FC+0.3536*BR" -c:a pcm_s16le $Y/ref.wav
l=$($C $Y/o.wav $Y/ref.wav --desfase 20)
case "$l" in *"desfase=0 snr="[7-9][0-9]*|*"desfase=0 snr=999"*) ok "anfitrión: un FLAC de 5.1, mezclado a estéreo ($l)" ;; *) ko "anfitrión: FLAC 5.1: $l" ;; esac
# sin huecos: las dos mitades de un seno de 1 kHz, una tras otra, por el
# motor (a 48 kHz): tiene que salir el seno entero, sin un salto
ffmpeg -hide_banner -loglevel error -y -i $F/seno.wav -af aresample=48000:resampler=soxr:precision=28 $Y/seno48.wav
for x in wav flac mp3 ogg; do
  $R motor $Y/m.wav $F/seno_a.$x $F/seno_b.$x > $Y/motor.txt 2>&1
  l=$($C $Y/m.wav $Y/seno48.wav --desfase 50)
  s=60; [ $x = ogg ] && s=38
  if echo "$l" | awk -v s=$s '{ for (i = 1; i <= NF; i++) { split($i, x, "="); v[x[1]] = x[2] } } END { split(v["tramas"], t, "/"); exit !(t[1] >= t[2] - 2 && v["desfase"] == 0 && v["snr"] + 0 >= s) }'; then
    ok "anfitrión: sin huecos ($x): las dos mitades de un seno, seguidas como si fueran una ($l)"
  else ko "anfitrión: sin huecos ($x): $l"; fi
done
# el CD: la imagen .BIN con su .CUE, y el FLAC entero con su .CUE: las tres pistas, seguidas
ffmpeg -hide_banner -loglevel error -y -f s16le -ar 44100 -ac 2 -i $F/TONOS.BIN -af aresample=48000:resampler=soxr:precision=28 $Y/t48.wav
for x in TONOS.CUE:"la imagen .BIN de un CD de música con su hoja .CUE" ALBUM.CUE:"un FLAC entero con su hoja .CUE"; do
  $R motor $Y/m.wav $F/${x%%:*} > $Y/motor.txt 2>&1
  l=$($C $Y/m.wav $Y/t48.wav --desfase 20)
  if grep -q "^cancion 2: Tono 3" $Y/motor.txt && echo "$l" | grep -q "desfase=0 snr=[6-9][0-9]"; then ok "anfitrión: ${x#*:}: sus tres pistas, sin huecos ($l)"
  else ko "anfitrión: ${x#*:}: $l"; head -4 $Y/motor.txt | sed 's/^/        /'; fi
done
# el fundido (4 s) entre canciones (no entre las pistas de un mismo disco):
# 10 + 10 + 12 s, menos dos fundidos de 4 s, son 24 s
REPRO_FUNDIDO=4 $R motor $Y/m.wav $F/escala.ogg $F/escala.flac $F/TONOS.CUE > $Y/motor.txt 2>&1
l=$(sed -n 's/^salida=\([0-9]*\) .*/\1/p' $Y/motor.txt)
if [ -n "$l" ] && [ "$l" -ge 1150000 ] && [ "$l" -le 1154000 ]; then ok "anfitrión: el fundido de 4 s entre canciones (y no entre las pistas de un disco): $l tramas (24 s)"
else ko "anfitrión: el fundido: «$l» tramas (se esperaban unas 1.152.000)"; fi
# el módulo: suena (30 s) y dice lo que es
$R motor $Y/m.wav $F/PRUEBA.MOD > $Y/motor.txt 2>&1
if grep -q "^salida=14[0-9][0-9][0-9][0-9][0-9] " $Y/motor.txt && grep -q "duracion=30 fallo=0" $Y/motor.txt; then ok "anfitrión: un módulo MOD propio (cuatro canales), con libxmp-lite: 30 s"
else ko "anfitrión: MOD: $(head -2 $Y/motor.txt)"; fi
# buscar: en un FLAC y en un MP3 de tasa fija, a la trama (frente a lo de
# decodificarlo entero); en un OGG, frente a ffmpeg
for x in escala.flac escala.mp3; do
  $R decodifica $F/$x $Y/entero.wav > /dev/null 2>&1
  $R busca $F/$x 4321 $Y/o.wav > /dev/null 2>&1
  l=$(python3 -I -c "
import sys
exec(open('$RD/pruebas/compara.py').read().split('def main')[0])
h, a = lee('$Y/o.wav'); h, b = lee('$Y/entero.wav')
d = 4321 * 44100 // 1000
print('distinto=%d de %d' % (sum(1 for i in range(0, len(a)) if a[i] != b[2 * d + i]), len(a)))")
  case "$l" in "distinto=0 de "*) ok "anfitrión: buscar en $x: a las 4,321 s justas ($l)" ;; *) ko "anfitrión: buscar en $x: $l" ;; esac
done
$R dsp > $Y/dsp.txt 2>&1
if awk '/^eq banda/ { b++; if ($(NF-1) + 0 < 5.9 || $(NF-1) + 0 > 6.1) m++ } END { exit !(b == 10 && m == 0) }' $Y/dsp.txt; then
  ok "anfitrión: el ecualizador: cada una de las 10 bandas, +6 dB en su frecuencia"
else ko "anfitrión: el ecualizador: $(grep '^eq' $Y/dsp.txt | tr '\n' ' ')"; fi
l=$(grep "^rm" $Y/dsp.txt)
if echo "$l" | awk '{ for (i = 1; i <= NF; i++) if ($i == "a" && $(i+2) == "dB,") { d = $(i+1) + 0; if (d < -0.05 || d > 0.05) m++ } } END { exit m }' && echo "$l" | grep -q "3 kHz a -[6-9][0-9]"; then
  ok "anfitrión: remuestrear de 44,1 a 48 kHz: el tono, a 0 dB; nada a otras frecuencias ($l)"
else ko "anfitrión: remuestrear: $l"; fi
l=$(grep "^velocidad 1,5" $Y/dsp.txt)
if echo "$l" | grep -q "^velocidad 1,5: 3[0-2][0-9][0-9][0-9] tramas" && echo "$l" | grep -q "440 Hz a -\?0\.[0-9]* dB, 660 Hz a -[4-9][0-9]"; then
  ok "anfitrión: la velocidad a 1,5: dura dos tercios y el tono sigue en 440 Hz ($l)"
else ko "anfitrión: velocidad: $l"; fi
# las etiquetas
$R etiquetas $F/esc_vbr.mp3 > $Y/et.txt 2>&1
if grep -q "^titulo=Escala de prueba$" $Y/et.txt && grep -q "^artista=Hispa-DOS$" $Y/et.txt && grep -q "^genero=Rock$" $Y/et.txt && grep -q "^rg_pista=-650 (1)" $Y/et.txt && grep -q "^caratula=5825 (89 50 4e 47)" $Y/et.txt; then
  ok "anfitrión: ID3v2.4 de un MP3: título, artista, género, ReplayGain (-6,50 dB) y la carátula (PNG)"
else ko "anfitrión: ID3v2: $(tr '\n' ' ' < $Y/et.txt)"; fi
for x in escala.ogg:"los comentarios de un OGG Vorbis y su METADATA_BLOCK_PICTURE" esc_car.flac:"los de un FLAC y su bloque PICTURE"; do
  $R etiquetas $F/${x%%:*} > $Y/et.txt 2>&1
  if grep -q "^album=Tonos de Pentium$" $Y/et.txt && grep -q "^anyo=2026$" $Y/et.txt && grep -q "^caratula=5825" $Y/et.txt; then ok "anfitrión: ${x#*:}"
  else ko "anfitrión: ${x#*:}: $(tr '\n' ' ' < $Y/et.txt)"; fi
done
$R lista $F/lista.m3u > $Y/l.txt 2>&1
if [ "$(wc -l < $Y/l.txt)" = 3 ] && grep -q "^0|escala.mp3|Hispa-DOS|Escala MP3|10|" $Y/l.txt; then ok "anfitrión: una .M3U con #EXTINF (artista, título y duración; la radio de Internet, saltada)"
else ko "anfitrión: .M3U: $(tr '\n' ' ' < $Y/l.txt)"; fi
$R lista $F/lista.pls > $Y/l.txt 2>&1
if [ "$(wc -l < $Y/l.txt)" = 2 ] && grep -q "^0|escala.wav||La escala en WAV|10|" $Y/l.txt; then ok "anfitrión: una .PLS"; else ko "anfitrión: .PLS: $(tr '\n' ' ' < $Y/l.txt)"; fi
# el fuzz (los decodificadores, las etiquetas, las listas y las hojas .CUE)
l=$(python3 -I $RD/pruebas/fuzz.py $R $F ${FUZZ_REPRO:-400} 2>&1 | tail -n 1)
case "$l" in "fuzz: "*" 0 malos"*) ok "anfitrión: $l (con ASan y UBSan)" ;; *) ko "anfitrión: $l" ;; esac
unset ASAN_OPTIONS
# ---- 2. REPRODUC.BIN y el paquete ----
if bash $RD/hacer.sh $Y/REPRODUC.BIN > $Y/hacer.txt 2>&1; then ok "hacer.sh: REPRODUC.BIN ($(stat -c %s $Y/REPRODUC.BIN) octetos), para el Pentium (-march=i586: sin CMOV ni FCOMI)"
else ko "hacer.sh no hace REPRODUC.BIN: $(tail -n 3 $Y/hacer.txt)"; return; fi
if bash $RD/paquete.sh $Y/paq > $Y/paq.txt 2>&1 && grep -q "^paquete=reproduc 1.0 REPRODUC.HPQ" $Y/paq/CATALOGO.LIN && grep -q "^tipo=reproduc multimedia" $Y/paq/CATALOGO.LIN; then
  ok "paquete.sh: el paquete «reproduc» 1.0 (multimedia) y su línea del catálogo"
else ko "paquete.sh: $(tail -n 2 $Y/paq.txt)"; fi
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $Y/semilla.asm > /dev/null && nasm -f bin $Y/semilla.asm -o $Y/SEMILLA.BIN || { ko "semilla.cas no compila"; return; }
for u in uordenes:ORDENES uconsola:CONSOLA; do
  python3 -I $AQUI/ccastel.py $AQUI/nucleo32/${u%%:*}.cas -o $Y/${u%%:*}.asm > /dev/null && nasm -f bin $Y/${u%%:*}.asm -o $Y/${u##*:}.BIN
done
bash $AQUI/nucleo32/sonido/hacer.sh $Y > /dev/null 2>&1 || { ko "no se hace SONIDO.BIN"; return; }
cp $F/escala.ogg $Y/ESCALA.OGG; cp $F/esc_vbr.mp3 $Y/ESCVBR.MP3; cp $F/escala.flac $Y/ESCALA.FLA; cp $F/TONOS.CUE $F/TONOS.BIN $F/PRUEBA.MOD $Y/
# ---- 3. desde la Consola, con las tres tarjetas ----
for tarjeta in "hda:48000:-device intel-hda -device hda-duplex,audiodev=snd0" "ac97:48000:-device AC97,audiodev=snd0" "sb16:44100:-device sb16,audiodev=snd0"; do
  n=${tarjeta%%:*}; hz=${tarjeta#*:}; hz=${hz%%:*}; dev=${tarjeta#*:*:}
  rm -f "${Y:?}/$n.wav"
  MEMORIA=128 MODULOS=$Y/ORDENES.BIN,$Y/SONIDO.BIN,$Y/REPRODUC.BIN,$Y/ESCALA.OGG,$Y/TONOS.CUE,$Y/TONOS.BIN ARRANQUE=ordenes \
    QEMU_MAS="-audiodev wav,id=snd0,path=$Y/$n.wav,out.frequency=$hz $dev" \
    $AQUI/nucleo32/probar.sh $Y/SEMILLA.BIN "reproduce M:\\ESCALA.OGG M:\\TONOS.CUE /volumen 100" ":pausa 26" > $Y/$n.sal 2>&1
  (cd $F && REPRO_HZ=$hz REPRO_RG=1 $R motor $Y/ref_$n.wav escala.ogg TONOS.CUE > /dev/null 2>&1)
  tr -d '\r' < $Y/$n.sal > $Y/$n.txt
  S=$Y/$n.txt
  mira "$n: REPRODUCE desde la Consola: lo que suena y por dónde va" "^reproductor: suena: Escala de prueba$" "^reproductor: suena: Tono 3$"
  sigue_bien "$n: la escala (OGG) y las tres pistas del CD (.CUE/.BIN), a $hz Hz" $Y/$n.wav $Y/ref_$n.wav
done
# ---- 4. lo que cuesta, en un Atom de QEMU ----
MEMORIA=128 MODULOS=$Y/ORDENES.BIN,$Y/REPRODUC.BIN,$Y/ESCVBR.MP3,$Y/ESCALA.FLA,$Y/ESCALA.OGG,$Y/PRUEBA.MOD ARRANQUE=ordenes QEMU_MAS="-cpu n270 -icount shift=0,sleep=off" \
  $AQUI/nucleo32/probar.sh $Y/SEMILLA.BIN "reproduce /mide M:\\ESCVBR.MP3 M:\\ESCALA.FLA M:\\ESCALA.OGG M:\\PRUEBA.MOD" ":pausa 40" > $Y/mide.sal 2>&1
tr -d '\r' < $Y/mide.sal | grep "^MIDE:" > $Y/mide.txt
if [ "$(wc -l < $Y/mide.txt)" = 4 ] && awk -F'[(%]' '{ split($2, x, " "); gsub(",", ".", x[1]); if (x[1] + 0 > 10) m++ } END { exit m }' $Y/mide.txt; then
  ok "lo que cuesta decodificar en un Atom N270 de QEMU (mil millones de instrucciones por segundo): $(sed 's/^MIDE: //; s/ de sonido en [0-9]* ms//; s/; [0-9]* M ciclos//' $Y/mide.txt | tr '\n' ' ')"
else ko "REPRODUCE /MIDE en el Atom: $(tr '\n' ' ' < $Y/mide.txt)"; fi
# y la ventana entera sonando (con el espectro) y en pequeño, en el mismo Atom
CAPTURAS=$G MEMORIA=256 MODULOS=$Y/ORDENES.BIN,$Y/CONSOLA.BIN,$Y/SONIDO.BIN,$Y/INICIO.WAV,$Y/REPRODUC.BIN,$Y/ESCVBR.MP3,$Y/ESCALA.OGG,$Y/PRUEBA.MOD \
  ARRANQUE=escritorio QEMU_MAS="-cpu n270 -icount shift=0 -audiodev wav,id=snd0,path=$Y/ic.wav,out.frequency=48000 -device intel-hda -device hda-duplex,audiodev=snd0" \
  $AQUI/nucleo32/probar.sh $Y/SEMILLA.BIN ":pausa 8" ":doble 60 186" ":pausa 6" ":escribe reproduce /ventana M:\\ESCVBR.MP3 M:\\ESCALA.OGG M:\\PRUEBA.MOD" ":tecla ret" \
  ":pausa 30" ":tecla ctrl-m" ":pausa 15" > $Y/ic.sal 2>&1
tr -d '\r' < $Y/ic.sal | grep "^reproductor: gasta el" > $Y/gasta.txt
l=$(grep "visualización espectro)" $Y/gasta.txt | tail -n 1 | sed 's/.*gasta el \([0-9,]*\) %.*/\1/')
k=$(grep "pequeño)" $Y/gasta.txt | tail -n 1 | sed 's/.*gasta el \([0-9,]*\) %.*/\1/')
if [ -n "$l" ] && [ -n "$k" ] && echo "$l $k" | awk '{ gsub(",", ".", $1); gsub(",", ".", $2); exit !($1 + 0 < 15 && $2 + 0 < 8) }'; then
  ok "la ventana entera sonando en el Atom N270 de QEMU: el $l % con el espectro; en pequeño, el $k %"
else ko "lo que gasta la ventana en el Atom: «$l» y «$k»: $(tr '\n' ' ' < $Y/gasta.txt)"; fi
# ---- 5. el CD: READ TOC y READ CD por IDE y por SATA ----
mkdir -p $Y/iso/d
head -c 300000 $F/escala.wav > $Y/iso/d/DATOS.BIN
xorriso -as mkisofs -quiet -V PRUEBA -o $Y/iso/prueba.iso $Y/iso/d > /dev/null 2>&1
l=$(python3 -I -c "
import zlib
d = open('$Y/iso/prueba.iso', 'rb').read()
print(' '.join('%08x' % (zlib.crc32(d[s * 2048:(s + 1) * 2048]) & 0xffffffff) for s in range(16, 22)))")
for via in "IDE:-cdrom $Y/iso/prueba.iso:C" "SATA (AHCI):-device ahci,id=ahci -drive id=cd0,if=none,file=$Y/iso/prueba.iso,media=cdrom -device ide-cd,drive=cd0,bus=ahci.0:C"; do
  n=${via%%:*}; q=${via#*:}; letra=${q##*:}; q=${q%:*}
  MEMORIA=128 MODULOS=$Y/ORDENES.BIN,$Y/REPRODUC.BIN ARRANQUE=ordenes QEMU_MAS="$q" \
    $AQUI/nucleo32/probar.sh $Y/SEMILLA.BIN ":pausa 4" "reproduce /pruebacd $letra: 16 6" ":pausa 3" > $Y/cd.sal 2>&1
  tr -d '\r' < $Y/cd.sal > $Y/cd.txt
  s=$(grep "^PRUEBACD: sector" $Y/cd.txt | awk '{ print $5 }' | tr '\n' ' ' | sed 's/ $//')
  if grep -q "^PRUEBACD: pista 1: lba 0, control 4$" $Y/cd.txt && grep -q "^PRUEBACD: pista 170: lba [0-9]*, control" $Y/cd.txt && [ "$s" = "$l" ]; then
    ok "el CD por $n: READ TOC (la pista de datos y el final) y READ CD en crudo (2352 octetos por sector: lo de dentro, como la imagen)"
  else ko "el CD por $n: «$s» (se esperaba «$l»): $(grep "PRUEBACD" $Y/cd.txt | head -3 | tr '\n' ' ')"; fi
done
# ---- 6. en el escritorio ----
# (con KVM, si lo hay: un procesador de verdad, con otro programa gastándolo
# entero; sin él, QEMU emula el x87 tan despacio que lo de comparar es otra
# cosa: entonces sin el otro programa)
local KVM= OTRO=()
if [ -w /dev/kvm ] && qemu-system-i386 -accel help 2>/dev/null | grep -q kvm; then
  KVM="-enable-kvm"
  OTRO=(":doble 60 186" ":pausa 4" ":escribe reproduce /ocupa 70" ":tecla ret" ":pausa 1" ":clic 185 583" ":pausa 1")
fi
cp $F/escala.ogg $Y/ESCALA.OGG
(cd $F && REPRO_RG=1 $R motor $Y/ref_esc.wav escala.ogg TONOS.CUE PRUEBA.MOD > /dev/null 2>&1)
rm -f "${Y:?}/esc.wav"
CAPTURAS=$G MEMORIA=256 MODULOS=$Y/ORDENES.BIN,$Y/CONSOLA.BIN,$Y/SONIDO.BIN,$Y/INICIO.WAV,$Y/REPRODUC.BIN,$Y/ESCALA.OGG,$Y/TONOS.CUE,$Y/TONOS.BIN,$Y/PRUEBA.MOD,$Y/ESCVBR.MP3 \
  ARRANQUE=escritorio QEMU_MAS="$KVM -audiodev wav,id=snd0,path=$Y/esc.wav,out.frequency=48000 -device intel-hda -device hda-duplex,audiodev=snd0" \
  $AQUI/nucleo32/probar.sh $Y/SEMILLA.BIN ":pausa 6" ":doble 60 186" ":pausa 4" "${OTRO[@]}" \
  ":escribe reproduce /ventana M:\\ESCALA.OGG M:\\TONOS.CUE M:\\PRUEBA.MOD" ":tecla ret" ":pausa 5" ":captura re1" \
  ":arrastra 300 42 360 90" ":pausa 1" ":arrastra 360 90 200 60" ":pausa 1" ":arrastra 200 60 400 120" ":pausa 1" ":arrastra 400 120 300 42" ":pausa 6" ":captura re2" \
  ":tecla ctrl-m" ":pausa 2" ":captura re3" ":pausa 30" > $Y/esc.sal 2>&1
tr -d '\r' < $Y/esc.sal > $Y/esc.txt
S=$Y/esc.txt
mira "en el escritorio: REPRODUCE /VENTANA abre la ventana" "^reproductor: ventana 760x[0-9]*; 5 canciones; la tarjeta a 48000 Hz$"
[ -n "$KVM" ] && mira "y, a la vez, otro programa gasta todo el procesador (REPRODUCE /OCUPA 70, con KVM)" "^reproductor: OCUPA: gasto el procesador 70 s$"
# la pantalla (azul oscuro), Reproducir/Pausa (verde) y el icono de la nota en la bandeja
miracap "la ventana: la «pantalla» con la carátula, los botones grandes (Pausa, en verde) y la nota del Reproductor en la bandeja" re1 \
  rgb 40 80 15304F rgb 250 270 2E9E40 rgb 726 585 2468C8
sigue_bien "en el escritorio${KVM:+, con otro programa gastando el procesador,} con la ventana arrastrándose y el modo pequeño" $Y/esc.wav $Y/ref_esc.wav
if ! grep -q "excepción\|\*\*\*" $S; then ok "en el escritorio, ni una excepción ni una queja del núcleo"
else ko "el núcleo se ha quejado:"; grep "excepción\|\*\*\*" $S | head -4 | sed 's/^/        /'; fi
}
