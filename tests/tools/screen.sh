#!/bin/bash
# Arranca una imagen en QEMU, teclea con teclas reales (sendkey) y guarda
# capturas de pantalla VGA en PNG.
# uso: screen.sh imagen salida_prefijo guion
#   guion: líneas "type TEXTO", "key NOMBRE" (ret, esc, alt-f, f10...),
#          "wait SEGUNDOS", "shot NOMBRE"
# Boots an image, types real keys via the QEMU monitor and saves PNG screenshots.
set -u
IMG=$1; OUT=$2; SCRIPT=$3
T=$(mktemp -d)
cp "$IMG" $T/disk.img
qemu-system-i386 -display none -monitor unix:$T/mon,server,nowait \
    -drive file=$T/disk.img,format=raw,if=floppy ${QEMU_EXTRA:-} -boot a &
QP=$!
sleep 4
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; }
keyname() {
  case "$1" in
    " ") echo spc;; ".") echo dot;; ",") echo comma;; "/") echo slash;;
    "\\") echo backslash;; "-") echo minus;; "=") echo equal;; ":") echo shift-semicolon;;
    ";") echo semicolon;; "*") echo shift-8;; "?") echo shift-slash;; ">") echo shift-dot;;
    "<") echo shift-comma;; "|") echo shift-backslash;; "_") echo shift-minus;;
    "$") echo shift-4;; "[") echo bracket_left;; "]") echo bracket_right;;
    "%") echo shift-5;; "!") echo shift-1;; "(") echo shift-9;; ")") echo shift-0;;
    "\"") echo shift-apostrophe;; "'") echo apostrophe;; "+") echo shift-equal;;
    [A-Z]) echo "shift-$(echo "$1" | tr A-Z a-z)";;
    *) echo "$1";;
  esac
}
while IFS= read -r line; do
  cmd=${line%% *}; arg=${line#* }
  case "$cmd" in
    type) for ((i=0; i<${#arg}; i++)); do mon "sendkey $(keyname "${arg:$i:1}")"; sleep 0.05; done;;
    key)  mon "sendkey $arg"; sleep 0.2;;
    wait) sleep "$arg";;
    shot) mon "screendump $T/s.ppm"; sleep 0.5; convert $T/s.ppm "${OUT}_$arg.png";;
  esac
done < "$SCRIPT"
kill $QP 2>/dev/null
cp $T/disk.img "${OUT}_disk.img"
rm -rf $T
