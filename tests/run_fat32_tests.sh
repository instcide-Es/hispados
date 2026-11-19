#!/bin/bash
# Discos grandes y FAT32 (como MS-DOS 7.1 / Windows 95 OSR2 y 98), en discos
# dispersos (no ocupan lo que miden):
#  1) 4 GB: FDISK sin /FAT32 sigue dando FAT16 (2.047 MB, lo de Hispa-386);
#     el menú pregunta por la compatibilidad con discos grandes; con /FAT32,
#     una primaria FAT32 (0Bh); FORMAT /S (clústeres de 4 KB), MD, COPY, DIR
#     (libres de más de 2 GB), ETIQUETA, REVISAR; fsck.fat limpio; arranque
#     desde la FAT32; SYS sobre una FAT32 sin sistema y PARTIR /ARRANQUE:C:
#     (lo que usa INSTALAR: lee el sector de arranque con 7305h).
#  2) 40 GB: primaria de 20 GB (0Ch, más allá del cilindro 1024) y unidad
#     lógica FAT32 en la extendida; FORMAT de las dos; fsck.fat de las dos.
#  3) 200 GB (más de 128 GB: LBA de 48 bits en la controladora): una sola
#     FAT32 de 200 GB con clústeres de 32 KB; REVISAR por ventanas (el mapa de
#     6,5 millones de clústeres no cabe de una vez); arranque desde ella.
#  4) un disco FAT32 hecho por mkfs.fat (Linux): leer, escribir, borrar,
#     renombrar; fsck.fat limpio y los archivos iguales; REVISAR ve un vínculo
#     cruzado y cadenas perdidas puestos a mano.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
fail=0
Q="qemu-system-i386 -display none -serial stdio -monitor none"
send() { sleep "${2:-0.8}"; printf '%s\r' "$1"; }
# esperar (como mucho $3 s) a que el registro $1 tenga $2 veces el texto $4
# (en CP437: solo letras ASCII)
espera() { local i=0; while [ "$(grep -aoF -- "$4" "$1" 2>/dev/null | wc -l)" -lt "$2" ] && [ $i -lt $(( $3 * 2 )) ]; do sleep 0.5; i=$((i + 1)); done; }
conv() { tr -d '\r' < "$1" | iconv -f CP437 -t UTF-8 > "$1.txt"; }
check() { if grep -qF -- "$2" "$1.txt"; then echo "  OK   $2"; else echo "  FAIL $2"; fail=1; fi; }
checkn() { if grep -qF -- "$2" "$1.txt"; then echo "  FAIL (no debía salir) $2"; fail=1; else echo "  OK   sin «$2»"; fi; }
show() { if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then for f in "$@"; do echo "--- $f ---"; cat "$f.txt"; done; fi; }
# la partición n (1-4, o 5 = la primera lógica) de un disco disperso, a un
# archivo aparte (solo lo escrito) para fsck.fat
part() {
  python3 - "$1" "$2" "$3" <<'PY'
import os, struct, sys
src, n, out = sys.argv[1], int(sys.argv[2]), sys.argv[3]
f = open(src, 'rb')
def tab(lba):
    f.seek(lba*512); s = f.read(512)
    return [struct.unpack_from('<BxxxBxxxII', s, 446+16*i) for i in range(4)]
t = tab(0)
if n <= 4:
    _, ty, st, sz = t[n-1]
else:
    ext = [e for e in t if e[1] in (5, 15)][0]
    _, ty, st, sz = tab(ext[2])[0]; st += ext[2]
fi = f.fileno(); fo = os.open(out, os.O_WRONLY|os.O_CREAT|os.O_TRUNC, 0o644)
os.ftruncate(fo, sz*512)
pos, end = st*512, (st+sz)*512
while pos < end:
    try: d = os.lseek(fi, pos, os.SEEK_DATA)
    except OSError: break
    if d >= end: break
    h = min(os.lseek(fi, d, os.SEEK_HOLE), end)
    os.lseek(fi, d, 0); os.lseek(fo, d-st*512, 0); m = h-d
    while m:
        b = os.read(fi, min(m, 1 << 20)); os.write(fo, b); m -= len(b)
    pos = h
print('%02X' % ty)
PY
}
fsck() {  # disco partición nombre
  local ty; ty=$(part "$1" "$2" $T/p.img)
  if /usr/sbin/fsck.fat -n $T/p.img > $T/fsck.txt 2>&1; then echo "  OK   fsck.fat limpio: $3 (tipo $ty)"
  else echo "  FAIL fsck.fat: $3 (tipo $ty)"; cat $T/fsck.txt; fail=1; fi
  rm -f $T/p.img
}
boot_c() {  # disco registro
  { sleep 4; send "VER" 1; send "DIR" 2; send "MEM" 2; send "APAGAR" 1; sleep 3; } |
    timeout 25 $Q -drive file="$1",format=raw,if=ide -boot c -no-reboot > "$2" 2>&1
  conv "$2"
}
cp "$IMG" $T/f.img

# ---- 1) 4 GB ----
truncate -s 4G $T/d4.img
# (sin /FAT32: FAT16 como siempre; luego se borra)
{ sleep 4
  send "FDISK /PRI" 3
  send "FDISK /DEL:1" 3
  send "FDISK" 2
  send "N" 2
  printf '\033'; sleep 1
  send "FDISK /FAT32 /PRI" 3
  send "FORMAT C: /S /V:GRANDE /AUTOTEST" 1
  espera $T/l1 1 120 "n disponibles en el disco"
  send "C:" 1
  send "MD DOCS" 1
  send "COPY A:\\COMMAND.COM C:\\DOCS\\OTRO.COM" 3
  send "COPY A:\\KERNEL.SYS C:\\NUCLEO.BIN" 3
  send "DIR" 2
  send "ETIQUETA C:NUEVA" 2
  send "VOL C:" 1
  send "REVISAR C:" 1
  espera $T/l1 2 120 "n disponibles en el disco"
  send "A:" 1
  send "A:\\HISPADOS\\APAGAR" 1
  sleep 3
} | timeout 400 $Q -drive file=$T/f.img,format=raw,if=floppy \
      -drive file=$T/d4.img,format=raw,if=ide -boot a -no-reboot > $T/l1 2>&1
conv $T/l1
boot_c $T/d4.img $T/l1b
echo "== Hispa-DOS FAT32: disco de 4 GB =="
check $T/l1 "PRI DOS  FAT16"
check $T/l1 "2.047"
check $T/l1 "¿Desea activar la compatibilidad con discos grandes (S/N)?"
check $T/l1 "PRI DOS  FAT32      "
check $T/l1 "Formato completado."
check $T/l1 "Sistema transferido"
check $T/l1 "4.096 bytes en cada unidad de asignación."
check $T/l1 "NUCLEO   BIN"
check $T/l1 "El volumen de la unidad C es NUEVA"
# DIR con 7303h: más de 2 GB libres (con 36h saldrían 2.147.155.968)
if grep -qE "^ +4\.[0-9]{3}\.[0-9]{3}\.[0-9]{3} bytes libres" $T/l1.txt; then echo "  OK   DIR: más de 4 GB libres"; else echo "  FAIL DIR: más de 4 GB libres"; fail=1; fi
check $T/l1 "bytes en 1 directorios"
check $T/l1 "bytes en 3 archivos de usuario"
checkn $T/l1 "La cadena de asignación no es válida"
checkn $T/l1 "perdidas"
fsck $T/d4.img 1 "FAT32 de 4 GB"
echo "-- arranque desde la FAT32 --"
check $T/l1b "Hispa-DOS 0.9.28 - núcleo compatible con MS-DOS 6.22"
check $T/l1b " Directorio de C:\\"
check $T/l1b "DOCS            <DIR>"
check $T/l1b "bytes del mayor programa ejecutable"
show $T/l1 $T/l1b

# SYS sobre una FAT32 sin sistema
truncate -s 0 $T/d4.img; truncate -s 4G $T/d4.img
{ sleep 4
  send "FDISK /FAT32 /PRI" 3
  send "FORMAT C: /AUTOTEST" 1
  espera $T/l2 1 120 "n disponibles en el disco"
  send "SYS C:" 4
  send "FDISK /ARRANQUE:C:" 3
  send "A:\\HISPADOS\\APAGAR" 1
  sleep 3
} | timeout 200 $Q -drive file=$T/f.img,format=raw,if=floppy \
      -drive file=$T/d4.img,format=raw,if=ide -boot a -no-reboot > $T/l2 2>&1
conv $T/l2
boot_c $T/d4.img $T/l2b
echo "== Hispa-DOS FAT32: SYS =="
check $T/l2 "Sistema transferido a la unidad C:"
check $T/l2 "es la activa: el ordenador arrancará desde ella"
check $T/l2b "Versión de DOS notificada: 6.22"
check $T/l2b "COMMAND  COM"
fsck $T/d4.img 1 "FAT32 tras SYS"
show $T/l2 $T/l2b
rm -f $T/d4.img

# ---- 2) 40 GB: primaria y lógica ----
truncate -s 40G $T/d40.img
{ sleep 4
  send "FDISK /FAT32 /PRI:20480" 3
  send "FDISK /FAT32 /EXT" 3
  send "FDISK /FAT32 /LOG" 3
  send "FDISK /STATUS" 2
  send "FORMAT C: /S /V:PRIMARIA /AUTOTEST" 1
  espera $T/l3 1 200 "n disponibles en el disco"
  send "FORMAT D: /V:LOGICA /AUTOTEST" 1
  espera $T/l3 2 200 "n disponibles en el disco"
  send "COPY A:\\COMMAND.COM D:\\" 3
  send "DIR D:" 2
  send "A:\\HISPADOS\\APAGAR" 1
  sleep 3
} | timeout 500 $Q -drive file=$T/f.img,format=raw,if=floppy \
      -drive file=$T/d40.img,format=raw,if=ide -boot a -no-reboot > $T/l3 2>&1
conv $T/l3
echo "== Hispa-DOS FAT32: disco de 40 GB (primaria y lógica) =="
check $T/l3 "PRI DOS  FAT32 LBA"
check $T/l3 "LOG DOS  FAT32"
check $T/l3 "16.384 bytes en cada unidad de asignación."
check $T/l3 "El volumen de la unidad D es LOGICA"
check $T/l3 "COMMAND  COM"
fsck $T/d40.img 1 "primaria de 20 GB"
fsck $T/d40.img 5 "lógica"
show $T/l3
rm -f $T/d40.img

# ---- 3) 200 GB ----
truncate -s 200G $T/d200.img
{ sleep 4
  send "FDISK /FAT32 /PRI" 3
  send "FORMAT C: /S /V:ENORME /AUTOTEST" 1
  espera $T/l4 1 300 "n disponibles en el disco"
  send "C:" 1
  send "MD DOCS" 1
  send "COPY A:\\COMMAND.COM C:\\DOCS\\OTRO.COM" 3
  send "DIR" 2
  send "REVISAR C:" 1
  espera $T/l4 2 300 "n disponibles en el disco"
  send "A:\\HISPADOS\\APAGAR" 1
  sleep 3
} | timeout 700 $Q -drive file=$T/f.img,format=raw,if=floppy \
      -drive file=$T/d200.img,format=raw,if=ide -boot a -no-reboot > $T/l4 2>&1
conv $T/l4
boot_c $T/d200.img $T/l4b
echo "== Hispa-DOS FAT32: disco de 200 GB =="
check $T/l4 "PRI DOS  FAT32 LBA           0   204.799    C:"
check $T/l4 "32.768 bytes en cada unidad de asignación."
check $T/l4 "214.695.870.464 bytes de espacio total en disco"
check $T/l4 "214.695.673.856 bytes libres"
check $T/l4 "6.551.992 unidades de asignación disponibles en el disco"
checkn $T/l4 "perdidas"
checkn $T/l4 "no es válida"
fsck $T/d200.img 1 "FAT32 de 200 GB"
check $T/l4b " El volumen de la unidad C es ENORME"
check $T/l4b "DOCS            <DIR>"
show $T/l4 $T/l4b
rm -f $T/d200.img

# ---- 4) FAT32 de Linux (mkfs.fat) ----
truncate -s 4G $T/lx.img
echo 'start=2048, type=c' | /usr/sbin/sfdisk -q $T/lx.img
SECS=$(( $(stat -c %s $T/lx.img)/512 - 2048 ))
mkfs.fat -F 32 -n LINUX32 --offset 2048 $T/lx.img $((SECS / 2)) > /dev/null   # (en KB)
P=$T/lx.img@@1M
printf 'Hola desde Linux\r\n' > $T/hola.txt
head -c 300000 /dev/urandom > $T/azar.bin
mcopy -i $P $T/hola.txt ::HOLA.TXT
mcopy -i $P $T/azar.bin ::AZAR.BIN
mmd -i $P ::DOCS
mcopy -i $P $T/hola.txt ::DOCS/DENTRO.TXT
mcopy -i $T/f.img ::COMMAND.COM $T/COMMAND.COM
{ sleep 4
  send "C:" 1
  send "TYPE HOLA.TXT" 1
  send "TYPE DOCS\\DENTRO.TXT" 1
  send "COPY AZAR.BIN COPIA.BIN" 4
  send "MD NUEVA" 1
  send "COPY A:\\COMMAND.COM NUEVA" 3
  send "DEL HOLA.TXT" 1
  send "REN AZAR.BIN AZAR2.BIN" 1
  send "DIR" 2
  send "REVISAR" 1
  espera $T/l5 1 120 "n disponibles en el disco"
  send "A:\\HISPADOS\\APAGAR" 1
  sleep 3
} | timeout 300 $Q -drive file=$T/f.img,format=raw,if=floppy \
      -drive file=$T/lx.img,format=raw,if=ide -boot a -no-reboot > $T/l5 2>&1
conv $T/l5
echo "== Hispa-DOS FAT32: disco de mkfs.fat =="
check $T/l5 "Hola desde Linux"
check $T/l5 " El volumen de la unidad C es LINUX32"
check $T/l5 "AZAR2    BIN"
check $T/l5 "COPIA    BIN"
checkn $T/l5 "no es válida"
checkn $T/l5 "perdidas"
fsck $T/lx.img 1 "lo escrito por Hispa-DOS"
mcopy -n -i $P ::COPIA.BIN $T/c.bin && cmp -s $T/c.bin $T/azar.bin && echo "  OK   COPIA.BIN igual" || { echo "  FAIL COPIA.BIN distinta"; fail=1; }
mcopy -n -i $P ::NUEVA/COMMAND.COM $T/c.com && cmp -s $T/c.com $T/COMMAND.COM && echo "  OK   NUEVA\\COMMAND.COM igual" || { echo "  FAIL NUEVA\\COMMAND.COM distinto"; fail=1; }
if mdir -i $P ::HOLA.TXT > /dev/null 2>&1; then echo "  FAIL HOLA.TXT sigue"; fail=1; else echo "  OK   HOLA.TXT borrado"; fi
show $T/l5
# estropeado a mano: HOLA... ya no está; DENTRO.TXT apunta al primer clúster
# de AZAR2.BIN (vínculo cruzado) y tres cadenas perdidas
python3 - $T/lx.img <<'PY'
import struct, sys
f = open(sys.argv[1], 'r+b'); base = 2048*512
f.seek(base); bs = f.read(512)
rsv = struct.unpack_from('<H', bs, 14)[0]; nf = bs[16]; spf = struct.unpack_from('<I', bs, 36)[0]
spc = bs[13]; fat = base + rsv*512; data = base + (rsv + nf*spf)*512
def put(c, v):
    for i in range(nf): f.seek(fat + i*spf*512 + c*4); f.write(struct.pack('<I', v))
def dirsec(c):
    f.seek(data + (c-2)*spc*512); return bytearray(f.read(512))
put(50000, 50001); put(50001, 0x0FFFFFFF); put(60000, 0x0FFFFFFF); put(70000, 0x0FFFFFFF)
root = struct.unpack_from('<I', bs, 44)[0]
r = dirsec(root); e = {bytes(r[i:i+11]): i for i in range(0, 512, 32)}
a = e[b'AZAR2   BIN']; d = e[b'DOCS       ']
docs = struct.unpack_from('<H', r, d+20)[0] << 16 | struct.unpack_from('<H', r, d+26)[0]
s = dirsec(docs); e2 = {bytes(s[i:i+11]): i for i in range(0, 512, 32)}
t = e2[b'DENTRO  TXT']
s[t+20:t+22] = r[a+20:a+22]; s[t+26:t+28] = r[a+26:a+28]
f.seek(data + (docs-2)*spc*512); f.write(s)
PY
{ sleep 4; send "REVISAR C: /F" 1; espera $T/l6 1 120 "n disponibles en el disco"; send "A:\\HISPADOS\\APAGAR" 1; sleep 3; } | timeout 200 $Q -drive file=$T/f.img,format=raw,if=floppy \
      -drive file=$T/lx.img,format=raw,if=ide -boot a -no-reboot > $T/l6 2>&1
conv $T/l6
echo "== Hispa-DOS FAT32: REVISAR con errores =="
check $T/l6 "REVISAR solo informa"
check $T/l6 "Tiene un vínculo cruzado en la unidad de asignación"
check $T/l6 "unidades de asignación perdidas en"
check $T/l6 "bytes de espacio en disco se liberarían."
show $T/l6
exit $fail
