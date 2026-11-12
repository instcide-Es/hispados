#!/bin/bash
# Particiones extendidas: unidades lógicas en la cadena de EBR.
# Dos discos duros hechos con python + mformat:
#   disco 1 (120 MB): primaria FAT16 (06h) + extendida (05h) con tres lógicas:
#                     FAT16 < 32 MB (04h), FAT12 (01h) y FAT16 (06h)
#   disco 2 (64 MB):  primaria FAT16 LBA (0Eh) + extendida de Linux (85h; en
#                     la segunda pasada, 0Fh) con dos lógicas (0Eh y 06h)
# Letras como en MS-DOS: C: y D: las primarias de cada disco; E: F: G: las
# lógicas del primero; H: I: las del segundo. En cada una: DIR, leer un
# archivo puesto con mtools, escribir, copiar y crear una carpeta; al final
# se comprueba con mtools que cada cosa está en su partición y fsck.fat.
# Además FORMAT de una lógica (sectores ocultos = 63, desde su EBR).
# Extended partitions: logical drives in the EBR chain, two disks.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=build
T=$(mktemp -d)
cp "$IMG" $T/f.img

# tabla de particiones: (inicio, sectores, tipo, etiqueta, sectores por grupo)
cat > $T/mk.py <<'EOF'
import struct, sys, subprocess
out, total, prim, exttype, logs = sys.argv[1], int(sys.argv[2]), sys.argv[3], int(sys.argv[4], 16), sys.argv[5:]
def chs(lba):
    c, r = divmod(lba, 16 * 63)
    h, s = divmod(r, 63)
    if c > 1023: c, h, s = 1023, 15, 62
    return bytes([h, (s + 1) | ((c >> 8) << 6), c & 255])
def entry(boot, typ, start, size, base=0):
    return bytes([boot]) + chs(base + start) + bytes([typ]) + chs(base + start + size - 1) + struct.pack('<II', start, size)
def table(ents):
    s = bytearray(512)
    for i, e in enumerate(ents):
        s[446 + 16 * i:462 + 16 * i] = e
    s[510:512] = b'\x55\xAA'
    return bytes(s)
parts = []          # (inicio absoluto, sectores, etiqueta, spc)
ptyp, psize, plabel, pspc = prim.split(':')
ptyp, psize, pspc = int(ptyp, 16), int(psize), int(pspc)
ext = 63 + psize
ext += -ext % 63
img = open(out, 'wb')
img.truncate(total * 512)
img.write(table([entry(0x80, ptyp, 63, psize), entry(0, exttype, ext, total - ext)]))
parts.append((63, psize, plabel, pspc))
rel = 0             # EBR actual, desde el principio de la extendida
for i, l in enumerate(logs):
    typ, size, label, spc = l.split(':')
    typ, spc = int(typ, 16), int(spc)
    size = int(size) if size != '*' else total - ext - rel - 63
    ents = [entry(0, typ, 63, size, ext + rel)]
    nxt = rel + 63 + size
    nxt += -nxt % 63
    if i + 1 < len(logs):
        nsize = logs[i + 1].split(':')[1]
        nsize = int(nsize) + 63 if nsize != '*' else total - ext - nxt
        # la entrada 2: el siguiente EBR, desde el principio de la extendida
        e = bytearray(entry(0, 0x05, nxt, nsize, ext))
        ents.append(bytes(e))
    img.seek((ext + rel) * 512)
    img.write(table(ents))
    parts.append((ext + rel + 63, size, label, spc))
    rel = nxt
img.close()
for start, size, label, spc in parts:
    subprocess.run(['mformat', '-i', f'{out}@@{start * 512}', '-T', str(size), '-h', '16', '-s', '63',
                    '-H', '63', '-c', str(spc), '-v', label, '::'], check=True)
    print(start, size, label)
EOF
python3 $T/mk.py $T/d1.img 245760 06:80577:D1PRIM:4 05 04:61425:D1LOG1:4 01:16065:D1LOG2:8 '06:*:D1LOG3:4' > $T/p1 || exit 1
python3 $T/mk.py $T/d2.img 131072 0E:32193:D2PRIM:4 85 0E:40257:D2LOG1:4 '06:*:D2LOG2:4' > $T/p2 || exit 1
# letra -> imagen, inicio y etiqueta (el orden de MS-DOS)
{ sed -n 1p $T/p1 | sed "s|^|C $T/d1.img |"; sed -n 1p $T/p2 | sed "s|^|D $T/d2.img |"
  sed -n 2p $T/p1 | sed "s|^|E $T/d1.img |"; sed -n 3p $T/p1 | sed "s|^|F $T/d1.img |"
  sed -n 4p $T/p1 | sed "s|^|G $T/d1.img |"; sed -n 2p $T/p2 | sed "s|^|H $T/d2.img |"
  sed -n 3p $T/p2 | sed "s|^|I $T/d2.img |"; } > $T/mapa
while read -r L im st sz lb; do
  printf 'soy %s\r\n' "$lb" > $T/ID.TXT
  mcopy -i "$im@@$((st * 512))" $T/ID.TXT ::ID.TXT
done < $T/mapa

run() {   # guion salida tiempo
  { sleep 5
    while IFS= read -r c; do
      case "$c" in
        @*) sleep "${c#@}";;
        *) printf '%s\r' "$c";;
      esac
      sleep 0.6
    done < "$1"
    sleep 2
  } | timeout "$3" qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
        -drive file=$T/f.img,format=raw,if=floppy -drive file=$T/d1.img,format=raw,if=ide,index=0 \
        -drive file=$T/d2.img,format=raw,if=ide,index=1 -boot a -no-reboot > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 > "$2"
}
: > $T/g1
for L in C D E F G H I; do
  cat >> $T/g1 <<EOF
ECHO unidad-$L
DIR $L:
TYPE $L:\\ID.TXT
ECHO escrito en $L > $L:\\ESCRITO.TXT
MD $L:\\CARPETA
COPY A:\\HISPADOS\\EDITAR.COM $L:\\CARPETA
TYPE $L:\\ESCRITO.TXT
EOF
done
cat >> $T/g1 <<'EOF'
ECHO unidad-J
DIR J:
ECHO fin-uno
EOF
run $T/g1 $T/l1 80

fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
sec() { sed -n "/^unidad-$1\$/,/^unidad-/p" $T/l1; }
echo "== Hispa-DOS: particiones extendidas (unidades lógicas), dos discos =="
while read -r L im st sz lb; do
  if sec $L | grep -qF " El volumen de la unidad $L es $lb" && sec $L | grep -qxF "soy $lb"; then ok "$L: es $lb (DIR y TYPE de ID.TXT)"
  else ko "$L: debía ser $lb"; fi
  if sec $L | grep -qxF "escrito en $L"; then ok "$L: escribir y leer ESCRITO.TXT"; else ko "$L: escribir y leer ESCRITO.TXT"; fi
  o="$im@@$((st * 512))"
  if mtype -i "$o" ::ESCRITO.TXT 2>/dev/null | tr -d '\r' | grep -qxF "escrito en $L" &&
     mcopy -n -i "$o" ::CARPETA/EDITAR.COM $T/E.$L 2>/dev/null && cmp -s $T/E.$L $B/EDIT.COM; then
    ok "$L: lo escrito está en su partición (sector $st): ESCRITO.TXT y CARPETA\\EDITAR.COM"
  else ko "$L: lo escrito no está (bien) en la partición del sector $st"; fi
  dd if="$im" of=$T/p.img bs=512 skip=$st count=$sz status=none
  if fsck.fat -n $T/p.img > $T/fsck 2>&1; then ok "$L: fsck.fat ($(grep -o 'FAT[0-9]*' $T/fsck | head -1; file -b $T/p.img | grep -o 'FAT ([0-9]* bit)'))"
  else ko "$L: fsck.fat"; cat $T/fsck; fi
done < $T/mapa
if sed -n '/^unidad-J$/,$p' $T/l1 | grep -qF "Especificación de unidad no válida"; then ok "J: no existe"; else ko "J: no debía existir"; fi
grep -qx "fin-uno" $T/l1 && ok "fin-uno" || ko "fin-uno"
# las tablas de particiones y los EBR siguen igual (nadie escribió en ellos)
python3 $T/mk.py $T/r1.img 245760 06:80577:D1PRIM:4 05 04:61425:D1LOG1:4 01:16065:D1LOG2:8 '06:*:D1LOG3:4' > /dev/null
e=0
for s in 0 80640 142128 158256; do
  cmp -s <(dd if=$T/d1.img bs=512 skip=$s count=1 status=none) <(dd if=$T/r1.img bs=512 skip=$s count=1 status=none) || e=1
done
[ $e = 0 ] && ok "MBR y EBR del disco 1 intactos" || ko "MBR y EBR del disco 1 cambiados"

# segunda pasada: extendida 0Fh en el disco 2 y FORMAT de una lógica
python3 - $T/d2.img <<'EOF'
import sys
f = open(sys.argv[1], 'r+b')
f.seek(446 + 16 + 4); assert f.read(1) == b'\x85'
f.seek(446 + 16 + 4); f.write(b'\x0F')
EOF
cat > $T/g2 <<'EOF'
ECHO dos-H
TYPE H:\ID.TXT
TYPE I:\ESCRITO.TXT
ECHO dos-F
FORMAT F: /V:NUEVA /AUTOTEST
@6
DIR F:
ECHO otra vez > F:\NUEVO.TXT
TYPE F:\NUEVO.TXT
TYPE G:\ID.TXT
TYPE E:\ID.TXT
ECHO fin-dos
EOF
run $T/g2 $T/l2 45
echo "-- extendida 0Fh y FORMAT de una unidad lógica --"
if grep -qxF "soy D2LOG1" $T/l2 && grep -qxF "escrito en I" $T/l2; then ok "extendida de tipo 0Fh: H: e I: siguen ahí"; else ko "extendida de tipo 0Fh: H: e I:"; fi
grep -qF "Formato completado." $T/l2 && ok "FORMAT F: (lógica FAT12)" || ko "FORMAT F: (lógica FAT12)"
grep -qF " El volumen de la unidad F es NUEVA" $T/l2 && grep -qxF "otra vez" $T/l2 && ok "F: formateada: etiqueta NUEVA, escribir y leer" || ko "F: formateada: etiqueta NUEVA, escribir y leer"
grep -qxF "soy D1LOG3" $T/l2 && grep -qxF "soy D1LOG1" $T/l2 && ok "las vecinas (E: y G:) no se han tocado" || ko "las vecinas (E: y G:) no se han tocado"
read -r L im st sz lb < <(grep '^F ' $T/mapa)
dd if="$im" of=$T/p.img bs=512 skip=$st count=$sz status=none
h=$(python3 -c "import sys,struct; b=open(sys.argv[1],'rb').read(512); print(struct.unpack_from('<I',b,0x1C)[0])" $T/p.img)
[ "$h" = 63 ] && ok "F: sectores ocultos del sector de arranque = 63 (desde su EBR)" || ko "F: sectores ocultos = $h (debían ser 63)"
if fsck.fat -n $T/p.img > $T/fsck 2>&1 && mtype -i $T/p.img ::NUEVO.TXT 2>/dev/null | tr -d '\r' | grep -qxF "otra vez"; then ok "F: fsck.fat y NUEVO.TXT tras el FORMAT"
else ko "F: fsck.fat tras el FORMAT"; cat $T/fsck; fi
for l in E G; do
  read -r L im st sz lb < <(grep "^$l " $T/mapa)
  dd if="$im" of=$T/p.img bs=512 skip=$st count=$sz status=none
  fsck.fat -n $T/p.img > $T/fsck 2>&1 && ok "$l: fsck.fat tras el FORMAT de F:" || { ko "$l: fsck.fat tras el FORMAT de F:"; cat $T/fsck; }
done
grep -qx "fin-dos" $T/l2 && ok "fin-dos" || ko "fin-dos"
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then echo "--- 1 ---"; cat $T/l1; echo "--- 2 ---"; cat $T/l2; fi
rm -rf $T
[ $fail = 0 ] && echo "TODO OK" || echo "HAY FALLOS"
exit $fail
