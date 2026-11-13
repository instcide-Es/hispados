#!/bin/bash
# Prueba de la comprobación del sonido de INSTALAR: el último paso de la
# instalación es AUDICION /RAPIDA (la versión corta de la prueba de sonido)
# contra el disco recién instalado, para que HSPSND use desde el primer
# arranque el camino que suena. En QEMU con intel-hda + hda-duplex; las
# respuestas, simuladas por la consola serie (tests/tools/audicion_drv.py);
# el sonido, de la grabación de QEMU; los ficheros del disco, con mtools.
#   si       desde el disquete de uso en «Normal» (HSPSND cargado: AUDICION
#            lo para y lo dice): PARTIR, FORMATEA e INSTALAR C: contestando
#            que sí; C:\HISPADOS\SONIDO.INI con la sección de este equipo y
#            C:\HISPADOS\SONIDO.TXT, sin el volcado en pantalla; el disquete,
#            sin SONIDO.INI
#   usb      lo mismo desde la memoria USB (el disco duro es D:), con un
#            SONIDO.INI de otro equipo en la memoria: en el disco quedan las
#            dos secciones y la memoria queda como estaba
#   arranque si y usb: arrancar solo desde el disco instalado; HSPSND dice
#            «aprendida», /INFO enseña la ruta y SONAR suena
#   esc      Esc en la primera pregunta: no se apunta nada (el SONIDO.INI que
#            venía en el disquete se copia tal cual) y la instalación acaba
#   no       INSTALAR C: /NOSONIDO: ni la prueba ni SONIDO.INI ni SONIDO.TXT
#   nada     sin tarjeta de sonido: lo dice, no pregunta nada y acaba
# INSTALAR sound check test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=$(dirname "$IMG")
DISQ=$B/hispados-disquete.img
USBIMG=$B/hispados-usb.img
JOBS=${JOBS:-3}
T=$(mktemp -d)
fail=0
export MTOOLS_SKIP_CHECK=1
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
has() { if grep -qF -- "$2" "$1"; then ok "$3$2"; else ko "$3$2"; fi; }
hasnt() { if grep -qF -- "$2" "$1"; then ko "$3(no debe salir) $2"; else ok "$3no sale: $2"; fi; }
for f in "$DISQ" "$USBIMG"; do
  [ -f "$f" ] || { echo "== INSTALAR: comprobación del sonido =="; ko "falta $f (make usb)"; exit 1; }
done

EQ="-smbios type=1,manufacturer=Prueba,product=Portatil,version=1"
HDA="-device intel-hda -device hda-duplex,audiodev=snd0"
FIN="Ya puede arrancar desde ella."
SEC="[HDA 8086:2668 1AF4:1100 1AF4:0022 1AF4:0022]"
# un SONIDO.INI de otro equipo (en el disquete o en la memoria USB)
printf '; de antes\r\n\r\n[HDA 1022:157A 17AA:511F 10EC:0298 17AA:511F]\r\nEstado=SUENA\r\nEquipo=LENOVO | 20KCS0FT02 | ThinkPad A275\r\nRuta=14h(0) 0Ch(0) 02h\r\n' > $T/otro.ini

# instala nombre respuestas "órdenes previas" orden -- dispositivos de QEMU
# (el disquete o la memoria van en los dispositivos; el disco es $T/n/hdd.img)
instala() {
  local n=$1 ans=$2 pre=$3 cmd=$4; shift 4; shift
  local pa=()
  local IFS='|'
  for c in $pre; do pa+=(--pre "$c"); done
  unset IFS
  python3 tests/tools/audicion_drv.py $T/$n/log "$ans" "${pa[@]}" --cmd "$cmd" --fin "$FIN" -- \
    qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
    -audiodev wav,id=snd0,path=$T/$n/inst.wav "$@" -no-reboot
  tr -d '\r' < $T/$n/log | iconv -f CP437 -t UTF-8 -c | grep -v '^audio:' > $T/$n/log.txt
}
# arranca nombre: solo el disco instalado, en «Normal»; teclea las órdenes,
# cada una cuando ha acabado la anterior
espera() {  # nombre texto [segundos]
  local n=0
  until tr -d '\r' < $T/$1/blog 2>/dev/null | grep -aq -- "$2"; do
    sleep 0.5; n=$((n + 1)); [ $n -ge $((2 * ${3:-120})) ] && return 1
  done
  return 0
}
arranca() {
  local d=$1 k=0 c; shift
  : > $T/$d/blog
  rm -f $T/$d/in; mkfifo $T/$d/in
  timeout 200 qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
    -audiodev wav,id=snd0,path=$T/$d/out.wav -no-reboot $EQ $HDA \
    -drive file=$T/$d/hdd.img,format=raw,if=ide -boot c < $T/$d/in > $T/$d/blog 2>&1 &
  local qp=$!
  exec {fd}> $T/$d/in
  espera $d '^C:\\>' 90; sleep 1
  for c in "$@"; do
    k=$((k + 1)); printf '%s\r' "$c" >&$fd; sleep 0.3
    printf 'ECHO hecho-%s\r' $k >&$fd; espera $d "^hecho-$k" 90
  done
  sleep 0.5
  exec {fd}>&-
  kill $qp 2>/dev/null; wait $qp 2>/dev/null
  tr -d '\r' < $T/$d/blog | iconv -f CP437 -t UTF-8 -c | grep -v '^audio:' > $T/$d/blog.txt
}
# en el disco instalado (la partición empieza en el sector 63 o en el 2048)
part() {
  local off
  for off in 32256 1048576; do
    mdir -i $T/$1/hdd.img@@$off :: > /dev/null 2>&1 && { echo "$T/$1/hdd.img@@$off"; return; }
  done
  echo "$T/$1/hdd.img"
}
saca() {  # nombre fichero -> $T/nombre/fichero.txt (UTF-8, sin CR); 1 si no está
  mtype -i "$(part $1)" ::HISPADOS/$2 > $T/$1/$2.bin 2>/dev/null || { rm -f $T/$1/$2.bin; return 1; }
  tr -d '\r' < $T/$1/$2.bin | iconv -f CP437 -t UTF-8 > $T/$1/$2.txt
}
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
nuevo() {  # nombre imagen: carpeta, copia de la imagen y disco vacío de 32 MB
  mkdir -p $T/$1; cp "$2" $T/$1/f.img
  dd if=/dev/zero of=$T/$1/hdd.img bs=1M count=32 status=none
}
PREC="PARTIR /PRI|FORMATEA C: /V:DESTINO /AUTOTEST"

# ---- si: desde el disquete de uso, en «Normal» (con HSPSND cargado) ----
nuevo si "$DISQ"; bash tests/tools/sinmenu.sh $T/si/f.img NORMAL || ko "si: sinmenu.sh"
lote; instala si "default=S" "$PREC" "INSTALAR C:" -- $EQ $HDA \
  -drive file=$T/si/f.img,format=raw,if=floppy -drive file=$T/si/hdd.img,format=raw,if=ide -boot a &
# ---- usb: desde la memoria USB (el disco duro es D:) ----
nuevo usb "$USBIMG"; bash tests/tools/sinmenu.sh $T/usb/f.img NORMAL || ko "usb: sinmenu.sh"
mcopy -o -i $T/usb/f.img@@1048576 $T/otro.ini ::HISPADOS/SONIDO.INI
USBDEV="-device qemu-xhci,id=xhci -drive if=none,id=usbd,format=raw,file=$T/usb/f.img -device usb-storage,bus=xhci.0,drive=usbd,bootindex=0"
lote; instala usb "default=S" "PARTIR 2 /PRI|FORMATEA D: /V:DESTINO /AUTOTEST" "INSTALAR D:" -- $EQ $HDA $USBDEV \
  -drive if=none,id=hd,format=raw,file=$T/usb/hdd.img -device ide-hd,drive=hd,bootindex=1 &
# ---- esc: Esc en la primera pregunta (con un SONIDO.INI en el disquete) ----
nuevo esc "$IMG"; mcopy -o -i $T/esc/f.img $T/otro.ini ::HISPADOS/SONIDO.INI
lote; instala esc "default=S;P01=E" "$PREC" "INSTALAR C:" -- $EQ $HDA \
  -drive file=$T/esc/f.img,format=raw,if=floppy -drive file=$T/esc/hdd.img,format=raw,if=ide -boot a &
# ---- no: /NOSONIDO ----
nuevo no "$IMG"
lote; instala no "default=S" "$PREC" "INSTALAR C: /nosonido" -- $EQ $HDA \
  -drive file=$T/no/f.img,format=raw,if=floppy -drive file=$T/no/hdd.img,format=raw,if=ide -boot a &
# ---- nada: sin tarjeta de sonido ----
nuevo nada "$IMG"
lote; instala nada "default=S" "$PREC" "INSTALAR C:" -- $EQ \
  -drive file=$T/nada/f.img,format=raw,if=floppy -drive file=$T/nada/hdd.img,format=raw,if=ide -boot a &
wait

for c in si usb; do
  echo "== INSTALAR con la comprobación del sonido ($c) =="
  L=$T/$c/log.txt
  [ $c = si ] && D=C || D=D
  has $L "Hispa-DOS instalado en la unidad $D: (" "$c: "
  has $L "Por último, la comprobación del sonido" "$c: "
  has $L "Vamos a comprobar que su equipo suena. Pulse S si oye el sonido," "$c: "
  has $L "Si no tiene altavoces ni auriculares, pulse Esc para saltar este paso." "$c: "
  has $L "P01. ¿Ha oído el pitido? (S/N/R) S" "$c: "
  has $L "P02. ¿Lo oye? (S/N/R) S" "$c: "
  has $L "P03. ¿El segundo ha sonado más bajo que el primero? (S/N/R) S" "$c: "
  has $L "P04. ¿Suena solo por la izquierda? (S/N/R) S" "$c: "
  has $L "P05. ¿Suena solo por la derecha? (S/N/R) S" "$c: "
  hasnt $L "P06." "$c: la versión corta acaba ahí: "
  hasnt $L "Configuración PCI" "$c: sin el volcado en pantalla: "
  hasnt $L "HD Audio 00:04.0, códec 0:" "$c: sin el volcado en pantalla: "
  hasnt $L "=== RESUMEN ===" "$c: sin el resumen en pantalla: "
  has $L "El sonido de su equipo ha quedado configurado." "$c: "
  has $L "(HSPSND se ha detenido para la prueba: volverá a sonar al reiniciar.)" "$c: "
  has $L "$FIN" "$c: "
  if saca $c SONIDO.INI; then
    I=$T/$c/SONIDO.INI.txt
    for w in "$SEC" "Estado=SUENA" "Equipo=Prueba | Portatil | 1" "Ruta=03h(0) 02h" "Control=40h" "Volumen=amplificador 02h salida"; do
      has $I "$w" "$c: $D:\\HISPADOS\\SONIDO.INI: "; done
  else ko "$c: falta $D:\\HISPADOS\\SONIDO.INI en el disco instalado"; fi
  if saca $c SONIDO.TXT; then
    R=$T/$c/SONIDO.TXT.txt
    for w in "Fichero: $D:\\HISPADOS\\SONIDO.TXT" "=== CONTROLADORAS DE SONIDO (PCI) ===" "Configuración PCI (256 octetos):" \
      "=== PRUEBAS (versión corta, AUDICION /RAPIDA) ===" "  Solo izquierda: sí" "  Solo derecha: sí" "=== RESUMEN ===" \
      "Lo que ha sonado se ha guardado en $D:\\HISPADOS\\SONIDO.INI:"; do has $R "$w" "$c: $D:\\HISPADOS\\SONIDO.TXT: "; done
  else ko "$c: falta $D:\\HISPADOS\\SONIDO.TXT en el disco instalado"; fi
done
# el disquete de arranque, sin tocar; la memoria USB, con su SONIDO.INI de antes
if mdir -i $T/si/f.img ::HISPADOS/SONIDO.INI > /dev/null 2>&1; then ko "si: el disquete tiene SONIDO.INI"; else ok "si: el disquete sigue sin SONIDO.INI"; fi
mtype -i $T/usb/f.img@@1048576 ::HISPADOS/SONIDO.INI > $T/usb/memoria.ini 2>/dev/null
if cmp -s $T/usb/memoria.ini $T/otro.ini; then ok "usb: el SONIDO.INI de la memoria USB queda como estaba"; else ko "usb: el SONIDO.INI de la memoria USB ha cambiado"; fi
if [ -f $T/usb/SONIDO.INI.txt ]; then
  has $T/usb/SONIDO.INI.txt "[HDA 1022:157A 17AA:511F 10EC:0298 17AA:511F]" "usb: en el disco sigue el otro equipo: "
  o1=$(grep -nF "$SEC" $T/usb/SONIDO.INI.txt | cut -d: -f1); o2=$(grep -nF "[HDA 1022:157A" $T/usb/SONIDO.INI.txt | cut -d: -f1)
  if [ -n "$o1" ] && [ -n "$o2" ] && [ "$o1" -lt "$o2" ]; then ok "usb: la sección de este equipo va primero"; else ko "usb: orden de las secciones ($o1 $o2)"; fi
fi

# ---- arranque: solo desde el disco instalado ----
for c in si usb; do
  lote; arranca $c "HSPSND /INFO" "SONAR /TONE 1000 600" &
done
wait
for c in si usb; do
  echo "== Primer arranque desde el disco instalado ($c) =="
  L=$T/$c/blog.txt
  has $L "Configuración aprendida en la prueba (" "$c: HSPSND al cargarse: "
  has $L "  ruta 03h(0) 02h, control del pin 40h" "$c: HSPSND /INFO: "
  hasnt $L "es de otra tarjeta" "$c: "
  hasnt $L "no cuadra" "$c: "
  has $L "Tono 1000 Hz en Intel HD Audio" "$c: "
  tono $c 1000
done

echo "== INSTALAR: Esc salta la comprobación del sonido (esc) =="
L=$T/esc/log.txt
has $L "Por último, la comprobación del sonido" "esc: "
has $L "P01. ¿Ha oído el pitido? (S/N/R)" "esc: "
hasnt $L "P02." "esc: "
has $L "Se ha saltado la comprobación del sonido. Puede hacerla después con la" "esc: "
has $L "opción «Prueba» del menú de arranque o con la orden AUDICION." "esc: "
hasnt $L "El sonido de su equipo ha quedado configurado." "esc: "
has $L "$FIN" "esc: la instalación acaba igual: "
if saca esc SONIDO.INI && cmp -s $T/esc/SONIDO.INI.bin $T/otro.ini; then ok "esc: C:\\HISPADOS\\SONIDO.INI es el del disquete, sin tocar"; else ko "esc: C:\\HISPADOS\\SONIDO.INI ha cambiado"; fi
if saca esc SONIDO.TXT; then has $T/esc/SONIDO.TXT.txt "(Versión corta saltada con Esc: no se toca SONIDO.INI.)" "esc: SONIDO.TXT: "
else ko "esc: falta C:\\HISPADOS\\SONIDO.TXT"; fi

echo "== INSTALAR /NOSONIDO (no) =="
L=$T/no/log.txt
has $L "Hispa-DOS instalado en la unidad C: (" "no: "
has $L "$FIN" "no: "
hasnt $L "Por último, la comprobación del sonido" "no: "
hasnt $L "P01." "no: "
if saca no SONIDO.INI || saca no SONIDO.TXT; then ko "no: hay SONIDO.INI o SONIDO.TXT en el disco"; else ok "no: ni SONIDO.INI ni SONIDO.TXT en el disco"; fi

echo "== INSTALAR sin tarjeta de sonido (nada) =="
L=$T/nada/log.txt
has $L "Por último, la comprobación del sonido" "nada: "
has $L "No se ha encontrado ninguna tarjeta de sonido que se pueda probar." "nada: "
has $L "No se ha podido comprobar el sonido. Puede hacerlo después con la" "nada: "
hasnt $L "P01." "nada: "
has $L "$FIN" "nada: "
if saca nada SONIDO.INI; then ko "nada: hay SONIDO.INI en el disco"; else ok "nada: sin SONIDO.INI en el disco"; fi

if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then
  for n in si usb esc no nada; do
    echo "---- $n ----"; tail -50 $T/$n/log.txt; tail -30 $T/$n/blog.txt 2>/dev/null; cat $T/$n/audio.txt 2>/dev/null
  done
fi
rm -rf $T
exit $fail
