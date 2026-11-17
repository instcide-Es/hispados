#!/bin/bash
# Prueba de las asociaciones de Hispa Ventanas (\HISPADOS\ASOCIA.INI) en el
# Administrador de archivos: un BMP se abre en Mirador y deja ASOCIA.INI con
# los de serie; un tipo que no se conoce pregunta «Abrir con…» y, con «Sí»,
# lo guarda; «Editar» con un BMP va a Pintura.
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=build
T=$(mktemp -d)
CHK="python3 tests/tools/ventanas_check.py"
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
chk() { $CHK "$@" || fail=1; }
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.3}"; }
tecla() { local k; for k in "$@"; do mon "sendkey $k" 0.3; done; }
shot() { sleep 0.4; mon "screendump $T/$1.ppm" 0.6; }
send() { printf '%s\r' "$1"; sleep "${2:-1.5}"; }
muestra() { [ -f $T/$1.ppm ] && convert $T/$1.ppm $B/asocia_$2.png; }
RX=400; RY=300
ir() {
  local dx=$(( $1 - RX )) dy=$(( $2 - RY )) sx sy
  while [ $dx -ne 0 ] || [ $dy -ne 0 ]; do
    sx=$dx; [ $sx -gt 60 ] && sx=60; [ $sx -lt -60 ] && sx=-60
    sy=$dy; [ $sy -gt 60 ] && sy=60; [ $sy -lt -60 ] && sy=-60
    mon "mouse_move $sx $sy" 0.07
    dx=$(( dx - sx )); dy=$(( dy - sy ))
  done
  RX=$1; RY=$2; sleep 0.3
}
# el ratón, a la esquina de arriba a la izquierda (tras volver de un programa)
origen() { local i; for i in $(seq 1 16); do mon "mouse_move -60 -60" 0.05; done; RX=0; RY=0; sleep 0.3; }
clic() { ir $1 $2; mon "mouse_button 1" 0.25; mon "mouse_button 0" 0.5; }
derecho() { ir $1 $2; mon "mouse_button 2" 0.25; mon "mouse_button 0" 0.5; }
doble() { ir $1 $2; mon "mouse_button 1" 0.1; mon "mouse_button 0" 0.1; mon "mouse_button 1" 0.1; mon "mouse_button 0" 0.8; }
aparca() { ir 16 470; }
# a la raíz de una unidad, escribiéndola en la dirección («c» y «:»), y el foco a la lista
unidad() { clic 300 98; tecla ctrl-a $1 shift-dot ret; sleep 2.5; clic 500 400; }
# teclea un texto: letras, cifras y punto
escribe() {
  local t="$1" i c
  for ((i=0; i<${#t}; i++)); do
    c="${t:$i:1}"
    case "$c" in
      '.') tecla dot;;
      '*') tecla kp_multiply;;
      *) tecla "$c";;
    esac
  done
}
run() {
  local tmo=$1; shift
  rm -f $T/mon
  timeout $tmo qemu-system-i386 -display none -serial stdio -monitor unix:$T/mon,server,nowait \
    -parallel none -m 16 -drive file=$T/f.img,format=raw,if=floppy,index=0 \
    -drive file=$T/c.img,format=raw,if=ide -boot a -no-reboot "$@" > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 | grep -v '^audio:' > $T/log.txt
}
HD="$T/c.img@@$((63*512))"
# un archivo del disco duro (o del disquete) es igual que el de aquí
igual() {
  local img=$1 ruta=$2 orig=$3
  rm -rf $T/sale
  if mcopy -n -i $img ::$ruta $T/sale 2>/dev/null && cmp -s $T/sale $orig; then
    ok "$ruta, copiado bien"
  else
    ko "$ruta no está o no es igual que el original"
  fi
}
esta() { if mdir -i $1 ::$2 > /dev/null 2>&1; then ok "está $2"; else ko "falta $2"; fi; }
noesta() { if mdir -i $1 ::$2 > /dev/null 2>&1; then ko "no debía estar $2"; else ok "ya no está $2"; fi; }
fsck_bien() {
  local img=$1 nombre=$2 skip=${3:-0}
  dd if=$img of=$T/part.img bs=512 skip=$skip status=none
  if fsck.fat -n $T/part.img > $T/fsck.txt 2>&1; then
    ok "fsck.fat: $nombre sin errores"
  else
    ko "fsck.fat: $nombre: $(tail -3 $T/fsck.txt | tr '\n' ' ')"
  fi
}


echo "== Asociaciones: abrir por la extensión y «Abrir con…» =="
cp "$IMG" $T/f.img
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
mdel -i $T/f.img ::HISPADOS/ASOCIA.INI > /dev/null 2>&1
python3 tests/tools/mkhdd.py $T/c.img 16 > /dev/null
mmd -i $HD ::DATOS
convert -size 40x30 xc:red -type palette BMP3:$T/FOTO.BMP
printf 'algo raro\r\n' > $T/RARO.XYZ
mcopy -i $HD $T/FOTO.BMP $T/RARO.XYZ ::DATOS/
FI() { echo $(( 142 + 18 * $1 )); }
AR() { echo $(( 122 + 18 * $1 )); }
LX=300
{ sleep 6
  send 'KEYB SP' 2
  send 'A:\HISPADOS\HSPRATON' 2
  send 'VENTANAS' 6
  doble 102 129; sleep 1
  tecla end ret; sleep 4
  clic 130 $(AR 5); sleep 2
  tecla f10; sleep 0.7; tecla right right; sleep 0.7; tecla d; sleep 1
  doble $LX $(FI 0); sleep 2
  # ---- FOTO.BMP: Intro -> Mirador ----
  tecla f ret; sleep 5
  aparca
  shot b1
  tecla alt-f4; sleep 2
  # ---- RARO.XYZ: no se sabe -> «Abrir con» ----
  tecla r ret; sleep 2
  aparca
  shot b2
  tecla ret; sleep 1.5                      # el primero: Bloc de notas
  shot b3
  tecla ret; sleep 2                        # «Sí»: usar siempre
  aparca
  shot b4
  tecla alt-f4; sleep 2
  # ---- otra vez RARO.XYZ: ya va solo al Bloc ----
  tecla r ret; sleep 2
  aparca
  shot b5
  tecla alt-f4; sleep 2
  # ---- Editar con FOTO.BMP: Pintura ----
  tecla f shift-f10; sleep 1; tecla e; sleep 5
  aparca
  shot b6
  tecla alt-f4; sleep 2
} | run 200
echo "== las ventanas =="
for s in b1 b2 b3 b4 b5 b6; do [ -f $T/$s.ppm ] || ko "no hay captura $s"; done
muestra b1 mirador; muestra b2 abrir_con; muestra b3 siempre; muestra b6 pintura
chk $T/b1.ppm titulo 40 34 activa "Mirador - FOTO.BMP"
chk $T/b2.ppm busca 250 180 550 400 0 8 "¿Con qué programa se abre?" busca 260 250 530 270 0 9 "Pluma"
chk $T/b3.ppm busca 160 260 640 320 0 8 "¿Usar siempre este programa para los .XYZ?"
chk $T/b5.ppm titulo 150 90 activa "Bloc de notas - C:\\DATOS\\RARO.XYZ"
chk $T/b6.ppm titulo 100 60 activa "Pintura - C:\\DATOS\\FOTO.BMP"
echo "== ASOCIA.INI =="
if mtype -i $T/f.img ::HISPADOS/ASOCIA.INI > $T/asocia.ini 2>/dev/null; then ok "ASOCIA.INI escrito (no estaba)"; else ko "no se ha escrito ASOCIA.INI"; fi
grep -q "^BMP=PROGS.MIRADOR.HVP" $T/asocia.ini && ok "BMP=PROGS\\MIRADOR.HVP (de serie)" || ko "falta BMP=PROGS\\MIRADOR.HVP"
grep -q "^XYZ=:BLOC" $T/asocia.ini && ok "XYZ=:BLOC («Abrir con» y «Sí»)" || ko "falta XYZ=:BLOC"
[ $(grep -c "^XYZ=" $T/asocia.ini) = 1 ] && ok "XYZ una sola vez" || ko "XYZ repetido o ausente"
grep -q "^PLU=PROGS.PLUMA.HVP" $T/asocia.ini && ok "PLU=PROGS\\PLUMA.HVP (de serie)" || ko "falta PLU"
echo "== Configuración › Tipos de archivo =="
# Configuración: marco (124,110), zona en (126,135); «Sistema» es el 6.º
# apartado (x=196, y=162+34*5); el botón «Tipos de archivo...» en (373,448)
{ sleep 6
  send 'KEYB SP' 2
  send 'A:\HISPADOS\HSPRATON' 2
  send 'VENTANAS' 6
  doble 102 129; sleep 1
  aparca
  tecla right right right right ret; sleep 2
  clic 196 332; sleep 1
  clic 373 448; sleep 1.5
  aparca
  shot c1
  tecla ret; sleep 1                        # «(Añadir un tipo...)»
  escribe doc; tecla ret; sleep 1
  shot c2
  tecla down ret; sleep 1.5                 # Pluma
  shot c3
  tecla end up ret; sleep 1                 # XYZ (el penúltimo)
  tecla end ret; sleep 1.5                  # «Ninguno»: se quita
  shot c4
  tecla esc; sleep 1
} | run 120
for s in c1 c2 c3 c4; do [ -f $T/$s.ppm ] || ko "no hay captura $s"; done
muestra c1 tipos; muestra c3 tipos_doc
mtype -i $T/f.img ::HISPADOS/ASOCIA.INI > $T/asocia2.ini 2>/dev/null
grep -q "^DOC=PROGS.PLUMA.HVP" $T/asocia2.ini && ok "Tipos de archivo: DOC añadido con Pluma" || ko "falta DOC=PROGS\\PLUMA.HVP"
grep -q "^XYZ=" $T/asocia2.ini && ko "XYZ seguía (se quitó con «Ninguno»)" || ok "Tipos de archivo: XYZ quitado con «Ninguno»"
grep -q "^BMP=PROGS.MIRADOR.HVP" $T/asocia2.ini && ok "lo demás sigue (BMP)" || ko "se ha perdido BMP"
echo "CAPTURAS=$T" > /dev/null
[ -n "${GUARDA:-}" ] && echo "CAPTURAS=$T" || rm -rf $T
exit $fail
