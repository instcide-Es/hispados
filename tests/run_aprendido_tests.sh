#!/bin/bash
# Prueba de lo aprendido en la prueba de sonido (\HISPADOS\SONIDO.INI): lo
# que AUDICION guarda cuando se contesta que SÍ suena y cómo HSPSND lo usa
# antes de buscar la salida él solo. En QEMU con intel-hda; las respuestas,
# simuladas por la consola serie (tests/tools/audicion_drv.py); el sonido, de
# la grabación de QEMU; SONIDO.INI se lee con mtools.
#   a        AUDICION /SIM (GPIO, EAPD y auriculares de pega) con hda-duplex:
#            suena con GPIO 02h y el control del pin C0h, que NO es lo que
#            haría HSPSND solo; la sección nueva va primero y las de los
#            demás equipos quedan igual (la vieja del mismo equipo, fuera);
#            código de salida 2
#   normal   el disquete de uso en «Normal» con ese SONIDO.INI: HSPSND (en
#            AUTOEXEC.BAT) dice «aprendida», /INFO enseña ese camino y suena
#   modelo   la misma tarjeta en otro modelo de equipo (SMBIOS): se usa, y
#            se avisa
#   otra     otro códec (hda-output) y una AC'97 con el mismo SONIDO.INI: no
#            se usa y HSPSND busca solo (y suena)
#   noapr    HSPSND /NOAPRENDIDO: como siempre, también en /INFO del residente
#   e        segunda prueba en el mismo equipo (EAPD apagado, GPIO como la
#            BIOS, el volumen que no baja): reemplaza su sección, nada más
#   ac97     AUDICION en una AC'97 y HSPSND justo después (sección [AC97 ...])
#   prueba   la opción «Prueba» del menú: DIAGNOSTICO, la prueba de sonido y
#            HSPSND cargado con lo aprendido sin reiniciar; SONAR suena
#   pruebano «Prueba» sin que suene nada: «NO SUENA» y el aviso de reiniciar
# Learned sound configuration (SONIDO.INI) test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=$(dirname "$IMG")
DISQ=$B/hispados-disquete.img
JOBS=${JOBS:-3}
T=$(mktemp -d)
fail=0
export MTOOLS_SKIP_CHECK=1
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
has() { if grep -qF -- "$2" "$1"; then ok "$3$2"; else ko "$3$2"; fi; }
hasnt() { if grep -qF -- "$2" "$1"; then ko "$3(no debe salir) $2"; else ok "$3no sale: $2"; fi; }
cuantas() { grep -cF -- "$2" "$1"; }
[ -f "$DISQ" ] || { echo "== Lo aprendido en la prueba de sonido =="; ko "falta $DISQ (make usb)"; exit 1; }

# la máquina de la prueba (SMBIOS fijo, para que «Equipo=» no dependa de QEMU)
EQ="-smbios type=1,manufacturer=Prueba,product=Portatil,version=1"
HDA="-device intel-hda -device hda-duplex,audiodev=snd0"

# aud nombre respuestas [opciones del conductor] -- dispositivos: AUDICION
# con las respuestas simuladas (ver run_audicion_tests.sh); el disco es
# $T/nombre/f.img
aud() {
  local n=$1 ans=$2; shift 2
  local pre=()
  while [ "$1" != "--" ]; do pre+=("$1"); shift; done
  shift
  python3 tests/tools/audicion_drv.py $T/$n/log "$ans" "${pre[@]}" -- \
    qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
    -audiodev wav,id=snd0,path=$T/$n/out.wav "$@" \
    -drive file=$T/$n/f.img,format=raw,if=floppy -boot a -no-reboot
  tr -d '\r' < $T/$n/log | iconv -f CP437 -t UTF-8 -c > $T/$n/log.txt
}
# sesion nombre "opciones de QEMU" orden...: arranca $T/nombre/f.img y teclea
# las órdenes, cada una cuando ha acabado la anterior
espera() {  # nombre texto [segundos]
  local n=0
  until tr -d '\r' < $T/$1/log 2>/dev/null | grep -aq -- "$2"; do
    sleep 0.5; n=$((n + 1)); [ $n -ge $((2 * ${3:-120})) ] && return 1
  done
  return 0
}
sesion() {
  local d=$1 qa=$2 k=0 c; shift 2
  : > $T/$d/log
  rm -f $T/$d/in; mkfifo $T/$d/in
  timeout 300 qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
    -audiodev wav,id=snd0,path=$T/$d/out.wav -no-reboot \
    -drive file=$T/$d/f.img,format=raw,if=floppy -boot a $qa < $T/$d/in > $T/$d/log 2>&1 &
  local qp=$!
  exec {fd}> $T/$d/in
  espera $d '^A:\\>' 90; sleep 1
  for c in "$@"; do
    k=$((k + 1)); printf '%s\r' "$c" >&$fd; sleep 0.3
    printf 'ECHO hecho-%s\r' $k >&$fd; espera $d "^hecho-$k" 120
  done
  sleep 0.5
  exec {fd}>&-
  kill $qp 2>/dev/null; wait $qp 2>/dev/null
  tr -d '\r' < $T/$d/log | iconv -f CP437 -t UTF-8 -c | grep -v '^audio:' > $T/$d/log.txt
}
# ini nombre -> $T/nombre/ini.txt (UTF-8, sin CR) e ini.bin (tal cual)
ini() {
  mtype -i $T/$1/f.img ::HISPADOS/SONIDO.INI > $T/$1/ini.bin 2>/dev/null
  tr -d '\r' < $T/$1/ini.bin | iconv -f CP437 -t UTF-8 > $T/$1/ini.txt
}
# tono nombre hz: ¿suena ese tono en la grabación?
tono() {
  python3 tests/tools/audio.py check $T/$1/out.wav > $T/$1/audio.txt 2>&1
  if python3 - $T/$1/audio.txt $2 <<'EOF'
import re, sys
t = open(sys.argv[1]).read(); w = int(sys.argv[2])
sys.exit(0 if any(abs(int(h) - w) < w * 0.06 for h in re.findall(r'tramo [\d.]+-[\d.]+ s: (\d+) Hz', t)) else 1)
EOF
  then ok "$1: suena el tono de $2 Hz"; else ko "$1: no suena el tono de $2 Hz ($(tr '\n' ' ' < $T/$1/audio.txt))"; fi
}
lote() { while [ "$(jobs -rp | wc -l)" -ge "$JOBS" ]; do sleep 0.5; done; }
disq() {  # nombre bloque: el disquete de uso con esa opción del menú
  mkdir -p $T/$1; cp "$DISQ" $T/$1/f.img
  bash tests/tools/sinmenu.sh $T/$1/f.img $2 || ko "$1: sinmenu.sh"
}

# ---- a: la primera prueba en este equipo ----
# SONIDO.INI de antes: otro equipo (HD Audio), una AC'97 y una sección vieja
# de ESTE equipo con otro códec (mismo «Equipo=» y misma controladora: fuera)
mkdir -p $T/a; cp "$IMG" $T/a/f.img
cat > $T/a/viejo.txt <<'EOF'
; Mi SONIDO.INI (comentario propio: se conserva)

[HDA 1022:157A 17AA:511F 10EC:0298 17AA:511F]
; de otro equipo: no se toca
Estado=SUENA
Fecha=01/11/2026 10:00
Equipo=LENOVO | 20KCS0FT02 | ThinkPad A275
Tipo=HDA
Ruta=14h(0) 0Ch(0) 02h
Control=40h

[AC97 8086:2415 8086:0000 8384:7609]
Estado=SUENA
Equipo=Otro | Viejo | 1
EAPD=invertido

[HDA 8086:2668 1AF4:1100 1AF4:0099 1AF4:0099]
; vieja, de este mismo equipo: se quita
Estado=SUENA
Equipo=Prueba | Portatil | 1
Ruta=05h 02h
EOF
sed 's/$/\r/' $T/a/viejo.txt > $T/a/SONIDO.INI
mcopy -o -i $T/a/f.img $T/a/SONIDO.INI ::HISPADOS/SONIDO.INI
printf '@ECHO OFF\r\nAUDICION /SIM\r\nIF ERRORLEVEL 2 ECHO Nivel 2: ha sonado\r\n' > $T/a/APR.BAT
mcopy -o -i $T/a/f.img $T/a/APR.BAT ::APR.BAT
SI="/segundo/=S;/Suena solo/=S;/entero/=S;/callado del todo/=S;/Enchufe/=S;/auriculares/=S"
# (P01 el pitido; P02 la de partida, P03 «snoop», P04 GPIO como la BIOS, P05
# GPIO a 0, P06 GPIO 01h, P07 GPIO 02h: suena)
lote; aud a "default=N;P01=S;P07=S;$SI" --cmd 'A:\APR.BAT' -- $EQ $HDA &

# ---- ac97 ----
mkdir -p $T/ac97; cp "$IMG" $T/ac97/f.img
lote; aud ac97 "default=S" --post 'A:\HISPADOS\HSPSND /INFO' --post 'SONAR /TONE 1000 600' -- $EQ -device AC97,audiodev=snd0 &

# ---- prueba / pruebano: la opción «Prueba» del menú del disquete ----
disq prueba PRUEBA
lote; aud prueba "default=S" --nada --post 'SONAR /TONE 1000 600' --post 'HSPSND /INFO' -- $EQ $HDA &
disq pruebano PRUEBA
lote; aud pruebano "default=N;/prueba de sonido/=S" --nada --post 'MEM /C' -- $EQ $HDA &
wait

echo "== Lo aprendido: AUDICION /SIM guarda SONIDO.INI (a) =="
L=$T/a/log.txt; ini a; I=$T/a/ini.txt
has $L "SUENA: HD Audio 00:04.0 códec 0, pin 03h (salida de línea), ruta 03h 02h (el último es el DAC), control del pin C0h, EAPD encendido, GPIO máscara 07h dirección 07h datos 02h, 48000 Hz" "a: "
has $L "Lo que ha sonado se ha guardado en A:\\HISPADOS\\SONIDO.INI:" "a: "
has $L "HSPSND usará ese mismo camino al cargarse (HSPSND /NOAPRENDIDO no lo usa)." "a: "
has $L "Nivel 2: ha sonado" "a: código de salida 2: "
for w in "[HDA 8086:2668 1AF4:1100 1AF4:0022 1AF4:0022]" "Estado=SUENA" "Fecha=" "Equipo=Prueba | Portatil | 1" "Tipo=HDA" \
  "Controladora=8086:2668" "Subsistema=1AF4:1100" "Posicion=00:04.0" "Codec=0" "IdCodec=1AF4:0022" "SubCodec=1AF4:0022" \
  "Altavoz=03h (salida de línea)" "Ruta=03h(0) 02h" "Control=C0h" "Auriculares=03h" "RutaAuriculares=03h(0) 02h" \
  "ControlAuriculares=C0h" "EAPD=encendido" "GPIO=07h 07h 02h" "Snoop=no" "Frecuencia=48000" "Formato=16 bits estéreo" \
  "Volumen=amplificador 02h salida"; do has $I "$w" "a: SONIDO.INI: "; done
has $I "; Mi SONIDO.INI (comentario propio: se conserva)" "a: SONIDO.INI: "
for w in "[HDA 1022:157A 17AA:511F 10EC:0298 17AA:511F]" "; de otro equipo: no se toca" "Equipo=LENOVO | 20KCS0FT02 | ThinkPad A275" \
  "Ruta=14h(0) 0Ch(0) 02h" "[AC97 8086:2415 8086:0000 8384:7609]" "Equipo=Otro | Viejo | 1"; do has $I "$w" "a: lo de otros equipos sigue: "; done
hasnt $I "[HDA 8086:2668 1AF4:1100 1AF4:0099 1AF4:0099]" "a: la vieja de este equipo (otro códec) "
hasnt $I "Ruta=05h 02h" "a: "
# el orden: los comentarios, la nueva y después las demás
o1=$(grep -n "Mi SONIDO.INI" $I | cut -d: -f1); o2=$(grep -nF "[HDA 8086:2668" $I | cut -d: -f1); o3=$(grep -nF "[HDA 1022:157A" $I | cut -d: -f1)
if [ -n "$o1" ] && [ -n "$o2" ] && [ -n "$o3" ] && [ "$o1" -lt "$o2" ] && [ "$o2" -lt "$o3" ]; then ok "a: la sección nueva va primero (tras los comentarios)"; else ko "a: orden de las secciones ($o1 $o2 $o3)"; fi
if [ "$(grep -c $'\r$' $T/a/ini.bin)" = "$(wc -l < $T/a/ini.bin)" ] && [ -s $T/a/ini.bin ]; then ok "a: SONIDO.INI con CR+LF"; else ko "a: SONIDO.INI sin CR+LF"; fi
if LC_ALL=C grep -q $'est\x82reo' $T/a/ini.bin; then ok "a: SONIDO.INI en CP437"; else ko "a: SONIDO.INI en CP437"; fi
cp $T/a/f.img $T/a/aprendido.img      # (lo usan los demás casos)
cp $T/a/ini.bin $T/a/ini1.bin

# ---- normal, modelo, otra, noapr y e: con ese SONIDO.INI ----
for c in normal modelo otra otraac noapr; do
  if [ $c = noapr ]; then mkdir -p $T/$c; cp "$IMG" $T/$c/f.img; else disq $c NORMAL; fi
  mcopy -o -i $T/$c/f.img $T/a/ini1.bin ::HISPADOS/SONIDO.INI
done
lote; sesion normal "$EQ $HDA" "HSPSND /INFO" "SONAR /TONE 1000 600" &
lote; sesion modelo "-smbios type=1,manufacturer=Prueba,product=Otro,version=2 $HDA" "HSPSND /INFO" &
lote; sesion otra "$EQ -device intel-hda -device hda-output,audiodev=snd0" "HSPSND /INFO" "SONAR /TONE 1000 600" &
lote; sesion otraac "$EQ -device AC97,audiodev=snd0" "HSPSND /INFO" "SONAR /TONE 1000 600" &
lote; sesion noapr "$EQ $HDA" "A:\\HISPADOS\\HSPSND /NOAPRENDIDO /INFO" "SONAR /TONE 1000 600" "A:\\HISPADOS\\HSPSND /INFO" &
# e: la segunda prueba, en el mismo disco que (a)
mkdir -p $T/e; cp $T/a/aprendido.img $T/e/f.img; mcopy -o -i $T/e/f.img $T/a/APR.BAT ::APR.BAT
# (P01 el pitido; con N a todo menos P10: EAPD apagado y GPIO como la BIOS;
# «¿El segundo ha sonado más bajo...?» N: el volumen, por programa)
lote; aud e "default=N;P01=S;P10=S;/Suena solo/=S" --cmd 'A:\APR.BAT' -- $EQ $HDA &
wait

echo "== Lo aprendido: HSPSND en «Normal» con SONIDO.INI (normal) =="
L=$T/normal/log.txt
has $L "Configuración aprendida en la prueba (" "normal: al cargarse: "
has $L "Intel HD Audio en " "normal: "
has $L "  ruta 03h(0) 02h, control del pin C0h" "normal: /INFO: "
has $L "  EAPD encendido, GPIO máscara 07h dirección 07h datos 02h" "normal: /INFO: "
has $L "Pin 03: línea" "normal: /INFO: "
hasnt $L "es de otra tarjeta" "normal: "
hasnt $L "no cuadra" "normal: "
hasnt $L "otro modelo de equipo" "normal: "
has $L "Tono 1000 Hz en Intel HD Audio" "normal: "
tono normal 1000

echo "== Lo aprendido: la misma tarjeta en otro modelo de equipo (modelo) =="
L=$T/modelo/log.txt
has $L "Configuración aprendida en la prueba (" "modelo: "
has $L "  ruta 03h(0) 02h, control del pin C0h" "modelo: "
has $L "(La prueba se hizo en otro modelo de equipo con la misma tarjeta.)" "modelo: "

echo "== Lo aprendido: otra tarjeta con el mismo SONIDO.INI (otra) =="
for c in otra otraac; do
  L=$T/$c/log.txt
  has $L "Lo aprendido en la prueba de sonido (SONIDO.INI) es de otra tarjeta:" "$c: "
  has $L "se busca la salida automáticamente." "$c: "
  hasnt $L "Configuración aprendida en la prueba (" "$c: "
done
has $T/otra/log.txt "Intel HD Audio en " "otra: "
has $T/otra/log.txt "Tono 1000 Hz en Intel HD Audio" "otra: "
tono otra 1000
has $T/otraac/log.txt "Audio Intel AC'97 en E/S " "otraac: "
has $T/otraac/log.txt "Tono 1000 Hz en Intel AC'97" "otraac: "
tono otraac 1000

echo "== Lo aprendido: HSPSND /NOAPRENDIDO (noapr) =="
L=$T/noapr/log.txt
n=$(cuantas $L "/NOAPRENDIDO: no se usa lo aprendido en la prueba de sonido.")
if [ "$n" -ge 2 ]; then ok "noapr: lo dice al cargarse y en /INFO del residente ($n)"; else ko "noapr: /NOAPRENDIDO dicho $n veces"; fi
hasnt $L "Configuración aprendida en la prueba (" "noapr: "
has $L "Pin 03: línea, conector (" "noapr: busca solo: "
has $L "Tono 1000 Hz en Intel HD Audio" "noapr: "
tono noapr 1000

echo "== Lo aprendido: segunda prueba en el mismo equipo (e) =="
L=$T/e/log.txt; ini e; I=$T/e/ini.txt
has $L "EAPD APAGADO, GPIO como la BIOS: máscara 00h dirección 00h datos 00h" "e: "
has $L "Nivel 2: ha sonado" "e: "
n=$(cuantas $I "[HDA 8086:2668 1AF4:1100 1AF4:0022 1AF4:0022]")
if [ "$n" = 1 ]; then ok "e: una sola sección de este equipo"; else ko "e: $n secciones de este equipo"; fi
for w in "EAPD=apagado" "GPIO=00h 00h 00h" "Volumen=por programa" "Control=C0h"; do has $I "$w" "e: SONIDO.INI: "; done
hasnt $I "GPIO=07h 07h 02h" "e: SONIDO.INI: "
hasnt $I "EAPD=encendido" "e: SONIDO.INI: "
for w in "; Mi SONIDO.INI (comentario propio: se conserva)" "[HDA 1022:157A 17AA:511F 10EC:0298 17AA:511F]" "; de otro equipo: no se toca" \
  "Ruta=14h(0) 0Ch(0) 02h" "[AC97 8086:2415 8086:0000 8384:7609]" "EAPD=invertido"; do has $I "$w" "e: sigue: "; done
n=$(grep -c '^\[' $I)
if [ "$n" = 3 ]; then ok "e: tres secciones en total"; else ko "e: $n secciones en total"; fi

echo "== Lo aprendido: AC'97 (ac97) =="
L=$T/ac97/log.txt; ini ac97; I=$T/ac97/ini.txt
for w in "[AC97 8086:2415 1AF4:1100 8384:7600]" "Estado=SUENA" "Tipo=AC97" "IdCodec=8384:7600" "EAPD=normal" "Equipo=Prueba | Portatil | 1"; do has $I "$w" "ac97: SONIDO.INI: "; done
has $I "; SONIDO.INI - lo que ha aprendido la prueba de sonido de Hispa-DOS" "ac97: SONIDO.INI nuevo: "
has $L "Configuración aprendida en la prueba (" "ac97: HSPSND: "
has $L "  EAPD normal" "ac97: HSPSND /INFO: "
has $L "Tono 1000 Hz en Intel AC'97" "ac97: "
tono ac97 1000

echo "== Lo aprendido: la opción «Prueba» del menú (prueba) =="
L=$T/prueba/log.txt; ini prueba; I=$T/prueba/ini.txt
has $L "¿Quiere hacer ahora la prueba de sonido? (S/N) S" "prueba: "
has $L "Lo que ha sonado se ha guardado en A:\\HISPADOS\\SONIDO.INI:" "prueba: "
has $L "Configuración aprendida en la prueba (" "prueba: HSPSND cargado: "
has $L "Sonido configurado con lo aprendido: ya puede usarlo, también en Hispa Ventanas." "prueba: "
hasnt $L "reinicie y elija" "prueba: "
has $L "Tono 1000 Hz en Intel HD Audio" "prueba: SONAR sin reiniciar: "
has $L "  ruta 03h(0) 02h, control del pin 40h" "prueba: /INFO: "
tono prueba 1000
has $I "Ruta=03h(0) 02h" "prueba: SONIDO.INI: "
if mdir -i $T/prueba/f.img ::INFORMES/EQUIPO01/SONIDO.TXT > /dev/null 2>&1; then ok "prueba: el informe de sonido, en \\INFORMES\\EQUIPO01"; else ko "prueba: falta \\INFORMES\\EQUIPO01\\SONIDO.TXT"; fi

echo "== Lo aprendido: «Prueba» sin que suene nada (pruebano) =="
L=$T/pruebano/log.txt; ini pruebano; I=$T/pruebano/ini.txt
has $L "que no ha sonado nada:" "pruebano: "
has $L "Para tener sonido (también en Hispa Ventanas), reinicie y elija" "pruebano: "
hasnt $L "Sonido configurado con lo aprendido" "pruebano: "
hasnt $L "Configuración aprendida en la prueba (" "pruebano: "
has $I "Estado=NO SUENA" "pruebano: SONIDO.INI: "
hasnt $I "Estado=SUENA" "pruebano: SONIDO.INI: "

if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then
  for n in a e normal modelo otra otraac noapr ac97 prueba pruebano; do
    echo "---- $n ----"; tail -60 $T/$n/log.txt; cat $T/$n/ini.txt 2>/dev/null; cat $T/$n/audio.txt 2>/dev/null
  done
fi
rm -rf $T
exit $fail
