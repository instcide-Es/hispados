#!/bin/bash
# Arranca Hispa-DOS (disquete) con un disco duro de programas, teclea órdenes
# por el puerto serie y va guardando capturas de la pantalla.
# uso: emm_game.sh DISCO.img PREFIJO "CONFIG.SYS (\n)" guion
#   guion (un fichero), líneas:
#     cmd TEXTO      teclea TEXTO y Intro por el puerto serie (la consola)
#     key NOMBRE     tecla de verdad (sendkey de QEMU: ret, esc, spc, y...)
#     wait SEGUNDOS
#     info ORDEN     orden del monitor de QEMU; la respuesta, a PREFIJO.info
#     shot NOMBRE    captura en PREFIJO_NOMBRE.png
#   variables: MEM (MB, 32), QEMU_EXTRA, IMG (disquete)
# Boots with a hard disk of programs, types commands and takes screenshots.
set -u
HDD=$(readlink -f "$1"); OUT=$(readlink -f "$(dirname "$2")")/$(basename "$2"); CFG=$3; SCRIPT=$(readlink -f "$4")
cd "$(dirname "$0")/../.."
IMG=${IMG:-build/hispados.img}
T=$(mktemp -d)
cp "$IMG" $T/f.img
printf "$CFG" | sed 's/$/\r/' > $T/CONFIG.SYS
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
mkfifo $T/in
qemu-system-i386 -display none -serial stdio -monitor unix:$T/mon,server,nowait -parallel none \
    -m ${MEM:-32} -audiodev none,id=snd -device sb16,audiodev=snd${SB:-} -device adlib,audiodev=snd \
    -drive file=$T/f.img,format=raw,if=floppy,index=0 \
    -drive file="$HDD",format=raw,if=ide,snapshot=on -boot a -no-reboot \
    ${QEMU_EXTRA:-} < $T/in > $T/raw 2>&1 &
QP=$!
exec 3> $T/in
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; }
sleep ${ARRANQUE:-7}
while IFS= read -r line; do
  cmd=${line%% *}; arg=${line#* }
  case "$cmd" in
    cmd)  printf '%s\r' "$arg" >&3; sleep 1.5;;
    key)  mon "sendkey $arg"; sleep 0.3;;
    wait) sleep "$arg";;
    info) printf '%s\n' "$arg" | socat -t1 - UNIX-CONNECT:$T/mon >> "${OUT}.info" 2>&1;;
    shot) mon "screendump $T/s.ppm"; sleep 0.5
          # (sin ImageMagick se queda en PPM)
          if command -v convert > /dev/null; then convert $T/s.ppm "${OUT}_$arg.png"; else cp $T/s.ppm "${OUT}_$arg.ppm"; fi;;
  esac
done < "$SCRIPT"
exec 3>&-
kill $QP 2>/dev/null
wait $QP 2>/dev/null
tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 -c > "${OUT}.txt"
rm -rf $T
