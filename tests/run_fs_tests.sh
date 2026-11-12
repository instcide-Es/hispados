#!/bin/bash
# Pruebas del sistema de archivos del núcleo con BIN\FSTEST (INT 21h: handles,
# FCB, búsquedas, rutas, etiquetas, bloqueos), disco lleno y raíz llena en un
# disquete de 360 KB en B:, carpetas de un CD con muchas subcarpetas (ISO
# 9660), 600 operaciones al azar comprobadas desde fuera, y dos discos en el
# límite entre FAT12 y FAT16 (4084 y 4085 clústeres) llenos hasta el último
# clúster. Al final fsck.fat comprueba todos los discos.
# Kernel file system tests.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
cp "$IMG" $T/a.img
cp "$IMG" $T/a2.img
printf 'DEVICE=A:\\HISPADOS\\HSPCD.SYS\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/a.img $T/CONFIG.SYS ::CONFIG.SYS
dd if=/dev/zero of=$T/b.img bs=1k count=360 status=none
mformat -f 360 -i $T/b.img ::
# disquete protegido contra escritura (B: del segundo arranque)
dd if=/dev/zero of=$T/p.img bs=1k count=1440 status=none
mformat -f 1440 -i $T/p.img ::
head -c 100 /dev/zero | tr '\0' X > $T/X.TXT
mcopy -i $T/p.img $T/X.TXT ::X.TXT
mmd -i $T/p.img ::D1
cp $T/p.img $T/p.orig
# dos disquetes distintos para cambiarlos con ficheros abiertos
for n in 1 2; do
  dd if=/dev/zero of=$T/x$n.img bs=1k count=1440 status=none
  mformat -f 1440 -N ${n}${n}${n}${n}${n}${n}${n}${n} -v DISCO$n -i $T/x$n.img ::
done
mcopy -i $T/x1.img $T/X.TXT ::X.TXT
mcopy -i $T/x2.img $T/X.TXT ::OTRO.TXT
mcopy -i $T/x2.img $T/X.TXT ::OTRO2.TXT
cp $T/x2.img $T/x2.orig
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; }
python3 tests/tools/mkfat.py $T/c.img 4084 > /dev/null
python3 tests/tools/mkfat.py $T/d.img 4085 > /dev/null
mkdir -p $T/iso/MUCHAS
for i in $(seq -w 1 40); do
  mkdir $T/iso/MUCHAS/SUB$i
  printf 'fichero %s\r\n' $i > $T/iso/MUCHAS/SUB$i/F$i.TXT
done
printf 'ultimo\r\n' > $T/iso/MUCHAS/ZFINAL.TXT
genisoimage -quiet -V MUCHAS -o $T/cd.iso $T/iso 2>/dev/null
send() { sleep "${2:-1}"; printf '%s\r' "$1"; }
# segundo arranque (a la vez): los discos del límite FAT12/FAT16 en C: y D:
{
  sleep 5
  send "B:" 1
  send "A:\\BIN\\FSTEST P" 1
  send "A:\\BIN\\FSTEST E" 4
  sleep 3
  mon "eject -f fb"
  sleep 2
  printf 'x'
  sleep 3
  mon "change fb $T/p.img raw"
  sleep 3
  printf 'x'
  sleep 3
  mon "change fb $T/x1.img raw read-write"
  sleep 3
  send "DIR" 1
  send "A:\\BIN\\FSTEST X" 2
  sleep 3
  mon "change fb $T/x2.img raw read-write"
  sleep 3
  printf 'x'
  sleep 3
  send "DIR" 1
  send "C:" 1
  send "A:\\BIN\\FSTEST F" 1
  send "A:\\BIN\\FSTEST S" 1
  sleep 60
  send "D:" 1
  send "A:\\BIN\\FSTEST F" 1
  send "A:\\BIN\\FSTEST S" 1
  sleep 60
  send "ECHO fin limites" 1
  sleep 2
} | timeout 230 qemu-system-i386 -display none -serial stdio -monitor unix:$T/mon,server,nowait -parallel none \
      -drive file=$T/a2.img,format=raw,if=none,id=fa -device floppy,unit=0,drive=fa \
      -drive file=$T/p.img,format=raw,if=none,id=fb,readonly=on -device floppy,unit=1,drive=fb,drive-type=144 \
      -drive file=$T/c.img,format=raw,if=ide,index=0 \
      -drive file=$T/d.img,format=raw,if=ide,index=1 -boot a -no-reboot > $T/log2 2>&1 &
{
  sleep 5
  for i in 1 2 3 4 5; do send "BIN\\FSTEST L" 1; done
  send "BIN\\FSTEST" 2
  sleep 12
  send "BIN\\FSTEST S" 1
  sleep 35
  send "IF EXIST DOCS\\NUL ECHO carpeta-si"
  send "IF EXIST NADA\\NUL ECHO carpeta-no"
  send "B:" 1
  send "A:\\BIN\\FSTEST F" 1
  sleep 25
  send "A:" 1
  send "LISTAR C:\\MUCHAS" 2
  send "MOSTRAR C:\\MUCHAS\\SUB40\\F40.TXT" 3
  send "ARBOL C:\\" 2
  send "C:" 3
  send "CD MUCHAS"
  send "A:\\BIN\\FSTEST C" 3
  send "A:" 2
  send "ECHO fin fs" 4
  sleep 2
} | timeout 170 qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
      -drive file=$T/a.img,format=raw,if=floppy,index=0 \
      -drive file=$T/b.img,format=raw,if=floppy,index=1 \
      -drive file=$T/cd.iso,media=cdrom,if=ide,index=2 -boot a -no-reboot > $T/log 2>&1
wait
tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 > $T/log.txt
tr -d '\r' < $T/log2 | iconv -f CP437 -t UTF-8 > $T/log2.txt
fail=0
L=$T/log.txt
check() { if grep -qF -- "$1" $L; then echo "  OK   $1"; else echo "  FAIL $1"; fail=1; fi; }
checkn() { if grep -qF -- "$1" $L; then echo "  FAIL (no debía salir) $1"; fail=1; else echo "  OK   sin \"$1\""; fi; }
count() { # texto, veces
  n=$(grep -cF -- "$1" $L)
  if [ "$n" -eq "$2" ]; then echo "  OK   $2 x $1"; else echo "  FAIL $1: $n veces, se esperaban $2"; fail=1; fi
}
echo "== Hispa-DOS: sistema de archivos (FSTEST) =="
count "FSTEST L: 10 FCB abiertos" 5
for i in $(seq -w 1 26); do check "T$i OK"; done
for i in 31 32 33 34 41 50; do check "T$i OK"; done
count "FSTEST FIN" 4
checkn "FALLO"
count "carpeta-si" 2             # la orden y su salida
count "carpeta-no" 1             # solo la orden
# CD: la carpeta con 40 subcarpetas sale entera
for i in 01 23 24 25 39 40; do check "SUB$i           <DIR>"; done
check "ZFINAL   TXT"
count "C:\\MUCHAS>A:" 2           # la carpeta actual del CD no se pierde
check "fichero 40"
count "<DIR>" 42
check "fin fs"
echo "  -- disco protegido, disco quitado y límite FAT12/FAT16 (C: 4084 clústeres, D: 4085) --"
L=$T/log2.txt
for i in 70 71 72 73 60 61 80; do check "T$i OK"; done
check "OTRO2    TXT"
if cmp -s $T/x2.img $T/x2.orig; then echo "  OK   el disquete que se puso con ficheros abiertos no ha cambiado"; else echo "  FAIL se ha escrito en el disquete que se puso con ficheros abiertos"; fail=1; fi
if cmp -s $T/p.img $T/p.orig; then echo "  OK   el disquete protegido no ha cambiado"; else echo "  FAIL el disquete protegido ha cambiado"; fail=1; fi
for i in 31 32 33 34 50; do count "T$i OK" 2; done
count "FSTEST FIN" 7
checkn "FALLO"
check "fin limites"
for d in a b c d; do
  if fsck.fat -n $T/$d.img > $T/fsck.txt 2>&1; then echo "  OK   fsck.fat $d.img"; else echo "  FAIL fsck.fat $d.img"; cat $T/fsck.txt; fail=1; fi
done
for d in a c d; do
  if python3 tests/tools/fscheck.py $T/$d.img > $T/chk.txt 2>&1; then echo "  OK   contenido de S0..S7.DAT en $d.img"; else echo "  FAIL contenido de S0..S7.DAT en $d.img"; cat $T/chk.txt; fail=1; fi
done
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt $T/log2.txt; fi
rm -rf $T
exit $fail
