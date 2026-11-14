#!/bin/bash
# Instalar desde la memoria USB en el disco duro (el caso de Carlos: un
# portátil sin disquetera que arranca de la memoria hecha con tools/mkusb.py).
# Al arrancar desde USB la BIOS pone la memoria como disco 80h (C:) y el disco
# duro como 81h (D:). Aquí la memoria va como disco IDE 0 (-boot c) y el disco
# duro, vacío (200 MB), como IDE 1:
#   1) PARTIR ve los dos discos y empieza por el disco duro; la memoria sale
#      como «Memoria USB de arranque (no tocar)»; borrar en ella pide escribir
#      SI (se contesta NO y no cambia nada). Con el menú: crear una partición
#      primaria de 100 MB en el disco duro y activarla (opción 2). INSTALAR, sin
#      unidad, la propone, le da formato y copia. La tabla del disco duro
#      cambia y la de la memoria NO; el disco duro lleva el MBR de Hispa-DOS y
#      su sector de arranque la unidad 80h, sus sectores ocultos y su geometría.
#   2) Arrancar SOLO con el disco duro: Hispa-DOS arranca de C: con su
#      AUTOEXEC.BAT y su CONFIG.SYS, sin la letra D: de la memoria.
#   3) Un solo disco, arrancando de él: PARTIR como antes (disco 1/1, sin la
#      opción 5); cambiar el disco de arranque pide SI: con SI se crean la
#      extendida y una unidad lógica, que se puede formatear y usar.
#   4) Un disco duro con una partición de otro sistema (NTFS, activa) y sitio
#      libre: INSTALL ofrece crear ahí la de Hispa-DOS sin borrar nada, la
#      deja activa (la otra, no) y arranca solo.
# Installing from the USB stick onto the hard disk (disk 81h).
set -u
cd "$(dirname "$0")/.."
USBIMG=${1:-build/hispados-usb.img}
[ -f "$USBIMG" ] || { echo "falta $USBIMG (make usb)"; exit 1; }
T=$(mktemp -d)
export MTOOLS_SKIP_CHECK=1
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$2" "$1"; then ok "$2"; else ko "$2"; fi; }
checkn() { if grep -qF -- "$2" "$1"; then ko "(no debe salir) $2"; else ok "no sale: $2"; fi; }
send() { sleep "${2:-1.5}"; printf '%s\r' "$1"; }
key() { sleep "${2:-1}"; printf '%s' "$1"; }
txt() { tr -d '\r' < $1 | iconv -f CP437 -t UTF-8 > $1.txt; }
# lo que hay entre las marcas «=X1=» y «=X2=» (ECHO =X1= ... ECHO =X2=)
part() { awk -v a="$2" -v b="$3" 'index($0, a) {p=1; next} p && index($0, b) {exit} p' "$1"; }
QEMU="timeout 120 qemu-system-i386 -display none -serial stdio -monitor none -no-reboot"

cp "$USBIMG" $T/usb.img
tests/tools/sinmenu.sh $T/usb.img || exit 1
dd if=/dev/zero of=$T/hdd.img bs=1M count=200 status=none
sfdisk -d $T/usb.img > $T/usb_antes.txt 2>/dev/null
dd if=$T/usb.img bs=512 count=1 status=none | md5sum > $T/usb_mbr_antes

# ------------------------------------------------------------------ 1
{
  sleep 8
  send "ECHO =S1=" 1
  send "PARTIR /STATUS" 1
  send "ECHO =S2=" 3
  send "PARTIR 1 /DEL:1" 1
  send "NO" 3
  send "ECHO =S3=" 1
  send "PARTIR" 1
  key "1" 3             # crear...
  key "1" 1             # ...una primaria
  send "100" 2          # tamaño: 100 MB (el resto, para la extendida de 3)
  key $'\e' 3
  key "2" 1             # activar
  send "1" 2
  key $'\e' 3
  key "4" 1             # mostrar
  key $'\r' 3
  key $'\e' 1           # salir
  send "ECHO =S4=" 2
  send "INSTALAR" 1
  send "S" 5            # dar formato a D: e instalar
  sleep 25
  send "ECHO =S5=" 1
  send "PARTIR /STATUS" 1
  send "ECHO =S6=" 3
} | $QEMU -drive file=$T/usb.img,format=raw,if=ide,index=0 \
      -drive file=$T/hdd.img,format=raw,if=ide,index=1 -boot c > $T/l1 2>&1
txt $T/l1
echo "== Instalar desde la memoria USB en el disco duro =="
echo "-- PARTIR con dos discos (arrancando de la memoria) --"
part $T/l1.txt "=S1=" "=S2=" > $T/s1
check $T/s1 "Disco actual: 2/2  (200 MB"
check $T/s1 "No hay particiones definidas."
part $T/l1.txt "=S2=" "=S3=" > $T/s2
check $T/s2 "es la MEMORIA USB DE ARRANQUE"
check $T/s2 "escriba SI y pulse Intro"
check $T/s2 "No se ha cambiado nada."
checkn $T/s2 "Partición eliminada"
part $T/l1.txt "=S3=" "=S4=" > $T/s3
check $T/s3 "Discos duros que ve la BIOS:"
check $T/s3 "Memoria USB de arranque (no tocar)"
check $T/s3 "Se ha arrancado desde una memoria USB (el disco 1): se empieza por el disco 2"
check $T/s3 "5. Cambiar la unidad de disco actual"
check $T/s3 "Disco actual: 2/2"
check $T/s3 "1. Crear una partición DOS primaria"
check $T/s3 "Partición DOS primaria creada."
check $T/s3 "Partición activa establecida."
grep -q "PRI DOS  FAT16 .* D:" $T/s3 && ok "la partición nueva es D:" || ko "la partición nueva no sale como D:"
checkn $T/s3 "¡Atención! El disco"
echo "-- INSTALAR sin unidad --"
part $T/l1.txt "=S4=" "=S5=" > $T/s4
check $T/s4 "Unidad para Hispa-DOS: D: (disco 2, partición 1"
check $T/s4 "La unidad D: no tiene formato"
check $T/s4 "Formato completado."
check $T/s4 "Sistema transferido a la unidad D:"
check $T/s4 "Hispa-DOS instalado en la unidad D:"
check $T/s4 "La partición 1 del disco 2 es la activa"
check $T/s4 "Quite la memoria USB"
part $T/l1.txt "=S5=" "=S6=" > $T/s5
check $T/s5 "PRI DOS  FAT16"
# las tablas: la del disco duro, cambiada; la de la memoria, NO
sfdisk -d $T/usb.img > $T/usb_despues.txt 2>/dev/null
dd if=$T/usb.img bs=512 count=1 status=none | md5sum > $T/usb_mbr_despues
if cmp -s $T/usb_antes.txt $T/usb_despues.txt && cmp -s $T/usb_mbr_antes $T/usb_mbr_despues; then
  ok "la tabla de particiones de la memoria USB no ha cambiado"; else ko "la tabla de la memoria USB ha cambiado"; diff $T/usb_antes.txt $T/usb_despues.txt; fi
python3 - $T/hdd.img build/mbr.bin > $T/hdd_tabla.txt <<'EOF'
import struct, sys
d = open(sys.argv[1], 'rb').read(512 * 64)
mbr = open(sys.argv[2], 'rb').read()[:446]
ents = [struct.unpack('<B3sB3sII', d[446 + 16 * i:462 + 16 * i]) for i in range(4)]
used = [e for e in ents if e[2]]
print('particiones', len(used))
st, ty, act, size = used[0][4], used[0][2], used[0][0], used[0][5]
print('tipo %02x activa %02x inicio %d sectores %d' % (ty, act, st, size))
print('mbr', 'hispados' if d[:446] == mbr else 'otro', 'firma', d[510:512].hex())
bs = open(sys.argv[1], 'rb').read((st + 1) * 512)[st * 512:]
spt, heads, hidden = struct.unpack('<HHI', bs[0x18:0x20])
print('arranque unidad %02x ocultos %d spt %d cabezas %d firma %s' % (bs[0x24], hidden, spt, heads, bs[510:512].hex()))
EOF
cat $T/hdd_tabla.txt | sed 's/^/       /'
grep -q "^particiones 1$" $T/hdd_tabla.txt && grep -q "^tipo 06 activa 80 inicio 63 " $T/hdd_tabla.txt \
  && ok "disco duro: una partición FAT16 (06h) activa desde el sector 63" || ko "tabla del disco duro"
grep -q "^mbr hispados firma 55aa$" $T/hdd_tabla.txt && ok "disco duro: MBR de Hispa-DOS" || ko "MBR del disco duro"
grep -q "^arranque unidad 80 ocultos 63 spt 63 cabezas 16 firma 55aa$" $T/hdd_tabla.txt \
  && ok "sector de arranque: unidad 80h, 63 ocultos, geometría del disco (63/16)" || ko "sector de arranque del disco duro"
P=$T/p.img
dd if=$T/hdd.img of=$P bs=512 skip=63 status=none
if fsck.fat -n $P > $T/fsck 2>&1; then ok "fsck.fat del disco instalado"; else ko "fsck.fat"; cat $T/fsck; fi
for f in AUTOEXEC.BAT CONFIG.SYS; do
  mtype -i $P ::$f > $T/$f 2>/dev/null
  [ -s $T/$f ] || ko "falta $f en el disco duro"
  if grep -qi "[ABD]:\\\\" $T/$f; then ko "$f del disco duro con otra letra que C:"; grep -i "[ABD]:\\\\" $T/$f; else ok "$f del disco duro: solo C:"; fi
done
grep -q "^PATH C:\\\\HISPADOS" $T/AUTOEXEC.BAT && ok "AUTOEXEC.BAT: PATH C:\\HISPADOS" || ko "AUTOEXEC.BAT sin PATH C:\\HISPADOS"

# ------------------------------------------------------------------ 2
{
  sleep 8
  send "ECHO =A1=" 1
  send "VER" 1
  send "SET" 1
  send "DIR C:\\HISPADOS\\PARTIR.COM" 2
  send "ECHO =A2=" 1
} | timeout 40 qemu-system-i386 -display none -serial stdio -monitor none -no-reboot \
      -drive file=$T/hdd.img,format=raw,if=ide -boot c > $T/l2 2>&1
txt $T/l2
echo "-- arranque solo desde el disco duro --"
check $T/l2.txt "Hispa-DOS 0.9 - núcleo compatible con MS-DOS 6.22"
check $T/l2.txt "HSPXMS 0.1: gestor XMS 3.0"
check $T/l2.txt "Bienvenido a Hispa-DOS"
checkn $T/l2.txt "Para instalarlo en el disco duro"
part $T/l2.txt "=A1=" "=A2=" > $T/a1
check $T/a1 "PATH=C:\\HISPADOS"
check $T/a1 "CONFIG=NORMAL"
check $T/a1 " Directorio de C:\\HISPADOS"
checkn $T/l2.txt "No se encuentra"
checkn $T/l2.txt "Error"

# ------------------------------------------------------------------ 3
cp $T/hdd.img $T/hdd3.img
{
  sleep 8
  send "ECHO =B1=" 1
  send "PARTIR /STATUS" 1
  send "ECHO =B2=" 3
  send "PARTIR" 1
  key $'\e' 2
  send "ECHO =B3=" 1
  send "PARTIR /EXT" 1
  send "SI" 3
  send "PARTIR /LOG" 1
  send "SI" 3
  send "ECHO =B4=" 1
  send "FORMATEA D: /V:LOGICA /AUTOTEST" 6
  send "COPY C:\\LEEME.TXT D:\\" 2
  send "DIR D:\\" 2
  send "ECHO =B5=" 1
} | timeout 60 qemu-system-i386 -display none -serial stdio -monitor none -no-reboot \
      -drive file=$T/hdd3.img,format=raw,if=ide -boot c > $T/l3 2>&1
txt $T/l3
echo "-- un solo disco, arrancando de él --"
part $T/l3.txt "=B1=" "=B2=" > $T/b1
check $T/b1 "Disco actual: 1/1  (200 MB"
check $T/b1 "Disco de arranque (el sistema en uso)"
checkn $T/b1 "Discos duros que ve la BIOS"
part $T/l3.txt "=B2=" "=B3=" > $T/b2
check $T/b2 "4. Mostrar la información de las particiones"
checkn $T/b2 "5. Cambiar"
checkn $T/b2 "Memoria USB"
part $T/l3.txt "=B3=" "=B4=" > $T/b3
check $T/b3 "es el DISCO DE ARRANQUE"
check $T/b3 "Partición DOS extendida creada."
check $T/b3 "Unidad lógica DOS creada."
grep -q "LOG DOS  FAT16 .* D:" $T/b3 && ok "la unidad lógica es D:" || ko "la unidad lógica no sale como D:"
part $T/l3.txt "=B4=" "=B5=" > $T/b4
check $T/b4 "Formato completado."
check $T/b4 " El volumen de la unidad D es LOGICA"
check $T/b4 "LEEME    TXT"
sfdisk -d $T/hdd3.img 2>/dev/null | grep -c "type=" > $T/n3
[ "$(cat $T/n3)" = 3 ] && ok "sfdisk: primaria, extendida y lógica" || { ko "sfdisk ve $(cat $T/n3) particiones"; sfdisk -d $T/hdd3.img; }
LS=$(sfdisk -d $T/hdd3.img 2>/dev/null | awk -F'[=,]' '/hdd3.img5/{gsub(/ /,"",$2); print $2}')
if [ -n "$LS" ] && mdir -i $T/hdd3.img@@$((LS * 512)) ::LEEME.TXT > /dev/null 2>&1; then
  ok "LEEME.TXT en la unidad lógica (sector $LS)"; else ko "LEEME.TXT en la unidad lógica"; fi

# ------------------------------------------------------------------ 4
dd if=/dev/zero of=$T/hdd4.img bs=1M count=200 status=none
# una partición «de Windows» (NTFS, 07h) activa en la mitad del disco, con
# la tabla hecha por un programa que no pone código de arranque (ceros)
python3 - $T/hdd4.img <<'EOF'
import struct, sys
f = open(sys.argv[1], 'r+b')
e = bytes([0x80, 1, 1, 0, 0x07, 15, 63, 0xC7]) + struct.pack('<II', 63, 200 * 1008 - 63)
f.write(bytes(446) + e + bytes(48) + b'\x55\xAA')
f.seek(63 * 512); f.write(b'NTFS DE PRUEBA'.ljust(512, b'\0'))
EOF
dd if=$T/hdd4.img bs=512 skip=63 count=1 status=none | md5sum > $T/ntfs_antes
{
  sleep 8
  send "ECHO =C1=" 1
  send "INSTALL" 1
  send "S" 4            # crear la partición en el espacio libre
  send "S" 5            # dar formato e instalar
  sleep 25
  send "ECHO =C2=" 1
} | $QEMU -drive file=$T/usb.img,format=raw,if=ide,index=0 \
      -drive file=$T/hdd4.img,format=raw,if=ide,index=1 -boot c > $T/l4 2>&1
txt $T/l4
echo "-- disco duro con una partición de otro sistema --"
part $T/l4.txt "=C1=" "=C2=" > $T/c1
check $T/c1 "ninguna primaria de DOS"
check $T/c1 "No se borra nada de lo que ya hay"
check $T/c1 "Unidad para Hispa-DOS: D: (disco 2, partición 2"
check $T/c1 "Hispa-DOS instalado en la unidad D:"
check $T/c1 "La partición 2 del disco 2 es la activa"
check $T/c1 "Hispa-DOS esta instalado."
python3 - $T/hdd4.img <<'EOF' > $T/t4
import struct, sys
d = open(sys.argv[1], 'rb').read(512)
for i in range(4):
    e = d[446 + 16 * i:462 + 16 * i]
    if e[4]: print(i + 1, '%02x' % e[4], '%02x' % e[0], struct.unpack('<I', e[8:12])[0])
EOF
sed 's/^/       /' $T/t4
grep -q "^1 07 00 63$" $T/t4 && grep -q "^2 06 80 " $T/t4 && ok "NTFS intacta y ya no activa; la de Hispa-DOS, activa" || ko "tabla del disco con NTFS"
dd if=$T/hdd4.img bs=512 skip=63 count=1 status=none | md5sum > $T/ntfs_despues
cmp -s $T/ntfs_antes $T/ntfs_despues && ok "el arranque de la partición NTFS no se ha tocado" || ko "se ha escrito en la partición NTFS"
{
  sleep 8
  send "ECHO =D1=" 1
  send "DIR C:\\" 2
  send "ECHO =D2=" 1
} | timeout 40 qemu-system-i386 -display none -serial stdio -monitor none -no-reboot \
      -drive file=$T/hdd4.img,format=raw,if=ide -boot c > $T/l5 2>&1
txt $T/l5
check $T/l5.txt "Bienvenido a Hispa-DOS"
check $T/l5.txt " El volumen de la unidad C es HISPADOS"

if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then
  for i in 1 2 3 4 5; do echo "--- arranque $i ---"; sed -n '/=[SABCD]1=/,$p' $T/l$i.txt; done
fi
rm -rf $T
exit $fail
