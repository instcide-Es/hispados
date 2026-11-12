#!/bin/bash
# INT 21h no trabaja sobre la pila del programa: PILTEST.COM (tests/progs)
# se pone una pila de 96 octetos con una zona de guarda debajo y crea,
# escribe, lee, busca, renombra y borra archivos, hace directorios, usa FCB,
# INT 25h y EXEC. Se prueba en el disquete (FAT12) y en un disco duro (FAT16).
# INT 21h must not run on the caller's stack (tiny stack + guard area).
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=build
[ -f $B/PILTEST.COM ] || make -s nucprogs
T=$(mktemp -d)
cp "$IMG" $T/f.img
mcopy -o -i $T/f.img $B/PILTEST.COM ::BIN/PILTEST.COM
# disco duro de 40 MB con una partición FAT16
python3 - $T/c.img <<'PY'
import sys, struct
n = 40 * 2048
mbr = bytearray(512)
mbr[446:462] = bytes([0x80, 1, 1, 0, 0x06, 15, 63, 80]) + struct.pack('<II', 63, n - 63)
mbr[510:512] = b'\x55\xAA'
with open(sys.argv[1], 'wb') as f:
    f.write(mbr); f.truncate(n * 512)
PY
mformat -i $T/c.img@@$((63*512)) -T $((40*2048-63)) -h 16 -s 63 -H 63 -v DURO ::
mcopy -i $T/c.img@@$((63*512)) $B/PILTEST.COM ::PILTEST.COM

run() {   # guion salida tiempo
  { sleep 5
    while IFS= read -r c; do
      case "$c" in
        @*) sleep "${c#@}";;
        *) printf '%s\r' "$c";;
      esac
      sleep 0.7
    done < "$1"
    sleep 2
  } | timeout "$3" qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
        -drive file=$T/f.img,format=raw,if=floppy -drive file=$T/c.img,format=raw,if=ide \
        -boot a -no-reboot > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 > "$2"
}
cat > $T/g1 <<'EOF'
BIN\PILTEST
@4
ECHO fin-disquete
C:
PILTEST
@4
A:
ECHO fin-duro
EOF
run $T/g1 $T/l1 30

fail=0
check()  { if grep -qF -- "$2" "$1"; then echo "  OK   $2"; else echo "  FAIL $2"; fail=1; fi; }
checkn() { if grep -qF -- "$2" "$1"; then echo "  FAIL (no debía salir) $2"; fail=1; else echo "  OK   sin \"$2\""; fi; }
sed -n '1,/^fin-disquete$/p' $T/l1 > $T/la
sed -n '/^fin-disquete$/,$p' $T/l1 > $T/lb
for d in a:disquete b:duro; do
  l=$T/l${d%%:*}
  echo "== Hispa-DOS: pila propia del núcleo en INT 21h (${d#*:}) =="
  for t in LIGERAS CREAR-ESCRIBIR LEER BUSCAR RENOMBRAR-BORRAR DIRECTORIOS FCB VARIAS INT25 EXEC; do
    check $l "PIL $t OK"
  done
  check  $l "PIL hijo en marcha"
  check  $l "PIL fin"
  checkn $l "FALLO"
  # de la pila del programa solo se usan los registros guardados (24) y lo
  # que apilen las interrupciones del equipo
  u=$(grep -o 'PIL pila usada=[0-9]*' $l | head -1 | grep -o '[0-9]*$')
  if [ -n "$u" ] && [ "$u" -le 72 ]; then echo "  OK   pila del programa usada: $u octetos de 96"
  else echo "  FAIL pila del programa usada: ${u:-?} octetos de 96"; fail=1; fi
done
check $T/l1 "fin-duro"
# los archivos de prueba no quedan y los discos están sanos
if mdir -i $T/f.img :: 2>/dev/null | grep -qi 'PILA'; then echo "  FAIL quedan archivos PILA* en A:"; fail=1
else echo "  OK   no quedan archivos de prueba en A:"; fi
dd if=$T/c.img of=$T/p.img bs=512 skip=63 status=none
for i in f p; do
  if fsck.fat -n $T/$i.img > $T/fsck 2>&1; then echo "  OK   fsck.fat $i.img"
  else echo "  FAIL fsck.fat $i.img"; cat $T/fsck; fail=1; fi
done
[ "${KEEP:-}" ] && echo "(queda $T)" || rm -rf $T
[ $fail = 0 ] && echo "TODO OK" || echo "HAY FALLOS"
exit $fail
