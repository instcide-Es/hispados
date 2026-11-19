#!/bin/bash
# Prueba del reparto de las letras de unidad (docs/UNIDADES.md):
#   la C: es del disco duro interno (reservada aunque no tenga partición ni
#   formato), la memoria USB de arranque es la X:, el CD/DVD va en la primera
#   libre detrás de las particiones, y PARTIR, FORMATEA, SISTEMA e INSTALAR
#   no tocan la memoria de la que se ha arrancado (solo con /FORZAR).
#  a) memoria USB + disco interno con FAT + lector de CD: interno C:, CD D:,
#     memoria X:; arrancan «Normal», «Juegos» y «Prueba» (el informe queda en
#     la memoria) y Hispa Ventanas abre.
#  b) memoria USB + disco interno vacío: C: reservada, con su mensaje;
#     FORMATEA C: pide PARTIR; tras PARTIR y FORMATEA, DIR C: va. La memoria
#     no se deja partir ni formatear; con /FORZAR, sí pregunta.
#  c) memoria USB sola: es la C:, como siempre.
#  d) disquete + disco interno (dos particiones) + CD: A:, C:, D: y el CD E:.
#  e) LETRAS=CLASICO (la memoria C:, el disco D:) y LETRAS=CD:R,USB:U.
#  f) la imagen de Hispa-486 con la memoria en la X:: el escritorio y un
#     programa del menú Inicio (CENTRO.INI sin unidad).
#  (HispaBIOS con un disco de más: tests/run_hispabios_tests.sh.)
#  uso: run_letras_tests.sh [hispados-usb.img] [hispados.img]   (SOLO=a,b...)
# Drive letter assignment tests.
set -u
cd "$(dirname "$0")/.."
USBIMG=${1:-build/hispados-usb.img}
FLOPPY=${2:-build/hispados.img}
USB486=${USB486:-build/hispa486-usb.img}
SOLO=${SOLO:-a,b,c,d,e,f}
T=$(mktemp -d)
trap 'rm -rf $T' EXIT
export MTOOLS_SKIP_CHECK=1
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$2" "$1"; then ok "$2"; else ko "$2"; fi; }
checkn() { if grep -qF -- "$2" "$1"; then ko "(no debe salir) $2"; else ok "no sale: $2"; fi; }
send() { printf '%s\r' "$1"; sleep "${2:-1.5}"; }
txt() { tr -d '\r' < $1 | iconv -f CP437 -t UTF-8 -c > $1.txt; }
part() { awk -v a="$2" -v b="$3" 'index($0, a) {p=1; next} p && index($0, b) {exit} p' "$1"; }
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.3}"; }
toca() { case ",$SOLO," in *,$1,*) return 0;; esac; return 1; }
# máquina N TIEMPO opciones... (lo que se teclea llega por la entrada)
maquina() {
  local n=$1 tmo=$2; shift 2
  rm -f $T/mon
  # (la pantalla, de 800x600 como en las demás pruebas: QEMU_PANTALLA)
  timeout $tmo qemu-system-i386 ${QEMU_PANTALLA--vga std -global VGA.xres=800 -global VGA.yres=600} -display none -serial stdio -monitor unix:$T/mon,server,nowait \
    -parallel none -no-reboot "$@" > $T/l$n 2>&1
  txt $T/l$n
}
# disco duro de 64 MB con N particiones FAT16 (1 o 2), cada una con QUIEN.TXT
disco() {
  local img=$1 n=$2
  dd if=/dev/zero of=$img bs=1M count=64 status=none
  if [ $n = 1 ]; then echo 'start=63, type=6, bootable' | sfdisk -q $img 2>/dev/null
  else printf 'start=63, size=60417, type=6, bootable\nstart=60480, type=6\n' | sfdisk -q $img 2>/dev/null; fi
  mformat -i $img@@$((63*512)) -T $([ $n = 1 ] && echo 131009 || echo 60417) -h 16 -s 63 -H 63 -v INTERNO ::
  printf 'Soy el disco duro interno\r\n' > $T/q.txt; mcopy -i $img@@$((63*512)) $T/q.txt ::QUIEN.TXT
  if [ $n = 2 ]; then
    mformat -i $img@@$((60480*512)) -T 70592 -h 16 -s 63 -H 60480 -v SEGUNDA ::
    printf 'Soy la segunda\r\n' > $T/q.txt; mcopy -i $img@@$((60480*512)) $T/q.txt ::QUIEN.TXT
  fi
}
usb() { echo "-device qemu-xhci,id=xhci -drive if=none,id=usbd,format=raw,file=$1 -device usb-storage,bus=xhci.0,drive=usbd,bootindex=0"; }
hd() { echo "-drive if=none,id=hd,format=raw,file=$1 -device ide-hd,drive=hd,bootindex=1"; }
mkdir -p $T/iso; printf 'Hola desde el CD\r\n' > $T/iso/CD.TXT
genisoimage -quiet -V DISCO -o $T/cd.iso $T/iso 2>/dev/null
CD="-drive file=$T/cd.iso,media=cdrom,if=ide,index=2"
# CONFIG.SYS de una imagen USB: añadir una línea al final
config_mas() { mtype -i $1@@1048576 ::CONFIG.SYS > $T/c.sys; printf '%s\r\n' "$2" >> $T/c.sys; mcopy -o -i $1@@1048576 $T/c.sys ::CONFIG.SYS; }

echo "== Hispa-DOS: las letras de unidad =="
# ------------------------------------------------------------------ a
if toca a; then
cp "$USBIMG" $T/usb.img; tests/tools/sinmenu.sh $T/usb.img || exit 1
disco $T/hdd.img 1
{ sleep 9
  send "ECHO =A1=" 1; send "UNIDADES" 2
  send "ECHO =A2=" 1; send "SET" 1.5; send "TYPE C:\\QUIEN.TXT" 1.5; send "TYPE D:\\CD.TXT" 3
  send "DIR X:\\INSTALL.BAT" 1.5
  send "ECHO =A3=" 1; send "VENTANAS" 9
  mon "screendump $T/a.ppm" 1
  mon "sendkey alt-f4" 2; mon "sendkey ret" 4
  send "ECHO =A4=" 1
  mon quit; } | maquina a 70 $(usb $T/usb.img) $(hd $T/hdd.img) $CD
echo "-- a) memoria USB + disco interno con FAT + CD --"
part $T/la.txt "=A1=" "=A2=" > $T/s
check $T/s "  C:  Disco duro interno (disco 2), FAT16"
check $T/s "  D:  Lector de CD/DVD"
check $T/s "  X:  Memoria USB (disco 1), FAT16, 511 MB  <- de aquí ha arrancado Hispa-DOS"
part $T/la.txt "=A2=" "=A3=" > $T/s
check $T/s "COMSPEC=X:\\COMMAND.COM"
check $T/s "ARRANQUE=X:"
check $T/s "PATH=X:\\HISPADOS"
check $T/s "Soy el disco duro interno"
check $T/s "Hola desde el CD"
check $T/s "INSTALL  BAT"
check $T/la.txt "HSPXMS 0.1: gestor XMS 3.0"
check $T/la.txt "Primera unidad de CD/DVD: D:"
check $T/la.txt "Bienvenido a Hispa-DOS"
checkn $T/la.txt "Comando o nombre de archivo incorrecto"
checkn $T/la.txt "No se encuentra"
# (en modo texto la captura de QEMU mide 720x400; Hispa Ventanas, 640x480 o más)
if [ -s $T/a.ppm ] && [ "$(head -c 2 $T/a.ppm)" = P6 ] && [ "$(sed -n 2p $T/a.ppm)" != "720 400" ]; then
  ok "Hispa Ventanas abre (pantalla gráfica de $(sed -n 2p $T/a.ppm | tr ' ' x))"; else ko "Hispa Ventanas no ha abierto"; fi
part $T/la.txt "=A3=" "=A4=" > $T/s
check $T/s "Hispa Ventanas ha terminado."
# «Juegos» y «Prueba»
for b in JUEGOS PRUEBA; do
  cp "$USBIMG" $T/usb.img; tests/tools/sinmenu.sh $T/usb.img $b || exit 1
  { sleep 12
    if [ $b = PRUEBA ]; then sleep 25; printf 'N'; sleep 3; fi    # (sin la prueba de sonido)
    send "ECHO =J1=" 1; send "SET" 1.5; send "MEM" 2
    mon quit; } | maquina j 80 $(usb $T/usb.img) $(hd $T/hdd.img) $CD
  echo "-- a) «$b» --"
  check $T/lj.txt "CONFIG=$b"
  check $T/lj.txt "ARRANQUE=X:"
  checkn $T/lj.txt "Comando o nombre de archivo incorrecto"
  checkn $T/lj.txt "No se encuentra"
  if [ $b = JUEGOS ]; then check $T/lj.txt "HSPEMM"; check $T/lj.txt "PATH=X:\\HISPADOS"
  else
    if mdir -i $T/usb.img@@1048576 ::INFORMES/EQUIPO01/INFORME.TXT > /dev/null 2>&1; then ok "el informe, en la memoria USB (\\INFORMES\\EQUIPO01)"; else ko "sin informe en la memoria USB"; fi
    if mdir -i $T/hdd.img@@$((63*512)) ::INFORMES > /dev/null 2>&1; then ko "hay INFORMES en el disco duro"; else ok "nada de INFORMES en el disco duro"; fi
  fi
done
fi
# ------------------------------------------------------------------ b
if toca b; then
cp "$USBIMG" $T/usb.img; tests/tools/sinmenu.sh $T/usb.img || exit 1
dd if=/dev/zero of=$T/hdd.img bs=1M count=64 status=none
md5sum < $T/usb.img > $T/usb_antes
{ sleep 9
  send "ECHO =B1=" 1; send "UNIDADES" 2; send "DIR C:" 1.5; send "FORMATEA C: /AUTOTEST" 2
  send "ECHO =B2=" 1; send "FORMATEA X: /AUTOTEST" 2; send "PARTIR 1 /DEL:1" 3; send "SISTEMA X:" 2
  send "SISTEMA A: X:" 2; send "INSTALAR X:" 2
  send "ECHO =B3=" 1; send "PARTIR 1 /DEL:1 /FORZAR" 3; send "NO" 2
  send "ECHO =B4=" 1; send "PARTIR /PRI" 4; send "UNIDADES" 2; send "DIR C:" 2
  send "FORMATEA C: /AUTOTEST" 6; send "DIR C:" 2; send "COPY X:\\LEEME.TXT C:\\" 2; send "DIR C:" 2
  send "ECHO =B5=" 1
  mon quit; } | maquina b 80 $(usb $T/usb.img) $(hd $T/hdd.img)
echo "-- b) memoria USB + disco interno vacío --"
part $T/lb.txt "=B1=" "=B2=" > $T/s
check $T/s "  C:  Disco duro interno (disco 2): RESERVADA, aún sin partición de DOS (PARTIR o INSTALAR)"
check $T/s "  X:  Memoria USB (disco 1)"
check $T/s "No se puede leer la unidad C: es el disco duro del ordenador, pero no tiene ninguna"
check $T/s "partición que Hispa-DOS sepa leer: está vacío, o tiene otro sistema"
check $T/s "de DOS. Créela antes con PARTIR (o use INSTALAR, que lo hace todo)."
checkn $T/s "Formato completado."
echo "-- la memoria de arranque no se toca --"
part $T/lb.txt "=B2=" "=B3=" > $T/s
check $T/s "X: es la memoria USB desde la que ha arrancado Hispa-DOS: no se puede"
check $T/s "partir ni formatear. (Quien sepa lo que hace: /FORZAR.)"
check $T/s "ordenador es otro (opción 5 o PARTIR n). Quien sepa lo que hace: /FORZAR."
check $T/s "No se puede transferir el sistema a la unidad de arranque."
check $T/s "partir ni formatear, ni cambiarle el sistema. (Quien sepa lo que hace: /FORZAR.)"
check $T/s "No se puede instalar sobre la unidad de arranque."
checkn $T/s "Formato completado."
checkn $T/s "Partición eliminada"
checkn $T/s "Sistema transferido"
part $T/lb.txt "=B3=" "=B4=" > $T/s
check $T/s "es la MEMORIA USB DE ARRANQUE"
check $T/s "escriba SI y pulse Intro"
check $T/s "No se ha cambiado nada."
if md5sum < $T/usb.img | cmp -s - $T/usb_antes; then ok "la memoria USB, intacta (ni un octeto)"; else ko "la memoria USB ha cambiado"; fi
echo "-- PARTIR y FORMATEA sobre la C: --"
part $T/lb.txt "=B4=" "=B5=" > $T/s
check $T/s "Partición DOS primaria creada."
check $T/s "  C:  Disco duro interno (disco 2), FAT16, 63 MB"
check $T/s "No se puede leer la unidad C: el disco no tiene formato. Use FORMATEA."
check $T/s "Formato completado."
check $T/s " Directorio de C:\\"
check $T/s "LEEME    TXT"
dd if=$T/hdd.img of=$T/p.img bs=512 skip=63 status=none
if fsck.fat -n $T/p.img > $T/fsck 2>&1; then ok "fsck.fat de la C: nueva"; else ko "fsck.fat"; cat $T/fsck; fi
fi
# ------------------------------------------------------------------ c
if toca c; then
cp "$USBIMG" $T/usb.img; tests/tools/sinmenu.sh $T/usb.img || exit 1
{ sleep 9
  send "ECHO =C1=" 1; send "UNIDADES" 2; send "SET" 1.5; send "FORMATEA C: /AUTOTEST" 2
  send "ECHO =C2=" 1
  mon quit; } | maquina c 40 $(usb $T/usb.img)
echo "-- c) memoria USB sola: es la C: --"
part $T/lc.txt "=C1=" "=C2=" > $T/s
check $T/s "  C:  Memoria USB (disco 1), FAT16, 511 MB  <- de aquí ha arrancado Hispa-DOS"
check $T/s "COMSPEC=C:\\COMMAND.COM"
check $T/s "ARRANQUE=C:"
check $T/s "PATH=C:\\HISPADOS"
check $T/s "C: es la memoria USB desde la que ha arrancado Hispa-DOS: no se puede"
check $T/lc.txt "Bienvenido a Hispa-DOS"
checkn $T/lc.txt "Comando o nombre de archivo incorrecto"
fi
# ------------------------------------------------------------------ d
if toca d; then
cp "$FLOPPY" $T/f.img
# (el disquete de las pruebas va lleno: UNIDADES.COM no va en él de serie)
mcopy -o -i $T/f.img build/UNIDADES.COM ::HISPADOS/UNIDADES.COM || ko "UNIDADES.COM no cabe en el disquete"
disco $T/hdd.img 2
{ sleep 4
  send "ECHO =D1=" 1; send "HISPADOS\\UNIDADES" 2; send "TYPE C:\\QUIEN.TXT" 1.5; send "TYPE D:\\QUIEN.TXT" 1.5
  send "ECHO =D2=" 1
  mon quit; } | maquina d 30 -drive file=$T/f.img,format=raw,if=floppy -drive file=$T/hdd.img,format=raw,if=ide -boot a
echo "-- d) disquete + disco interno con dos particiones --"
part $T/ld.txt "=D1=" "=D2=" > $T/s
check $T/s "  A:  Disquete  <- de aquí ha arrancado Hispa-DOS"
check $T/s "  C:  Disco duro interno (disco 1), FAT16, 29 MB"
check $T/s "  D:  Disco duro interno (disco 1), FAT16, 34 MB"
check $T/s "Soy el disco duro interno"
check $T/s "Soy la segunda"
# con la memoria USB y el CD: la segunda partición conserva la D: y el CD va a la E:
cp "$USBIMG" $T/usb.img; tests/tools/sinmenu.sh $T/usb.img || exit 1
{ sleep 9
  send "ECHO =D3=" 1; send "UNIDADES" 2; send "TYPE D:\\QUIEN.TXT" 1.5; send "TYPE E:\\CD.TXT" 3
  send "ECHO =D4=" 1
  mon quit; } | maquina d2 40 $(usb $T/usb.img) $(hd $T/hdd.img) $CD
echo "-- d) memoria USB + dos particiones + CD: el CD, detrás (E:) --"
part $T/ld2.txt "=D3=" "=D4=" > $T/s
check $T/s "  C:  Disco duro interno (disco 2), FAT16, 29 MB"
check $T/s "  D:  Disco duro interno (disco 2), FAT16, 34 MB"
check $T/s "  E:  Lector de CD/DVD"
check $T/s "  X:  Memoria USB (disco 1)"
check $T/s "Soy la segunda"
check $T/s "Hola desde el CD"
fi
# ------------------------------------------------------------------ e
if toca e; then
cp "$USBIMG" $T/usb.img; tests/tools/sinmenu.sh $T/usb.img || exit 1
config_mas $T/usb.img "LETRAS=CLASICO"
disco $T/hdd.img 1
{ sleep 9
  send "ECHO =E1=" 1; send "UNIDADES" 2; send "SET" 1.5; send "TYPE D:\\QUIEN.TXT" 1.5; send "TYPE E:\\CD.TXT" 3
  send "FORMATEA C: /AUTOTEST" 2
  send "ECHO =E2=" 1
  mon quit; } | maquina e 40 $(usb $T/usb.img) $(hd $T/hdd.img) $CD
echo "-- e) LETRAS=CLASICO: como siempre (la memoria C:, el disco D:) --"
part $T/le.txt "=E1=" "=E2=" > $T/s
check $T/s "  C:  Memoria USB (disco 1), FAT16, 511 MB  <- de aquí ha arrancado Hispa-DOS"
check $T/s "  D:  Disco duro (disco 2), FAT16, 63 MB"
check $T/s "  E:  Lector de CD/DVD"
check $T/s "Reparto de letras: el clásico de MS-DOS"
check $T/s "ARRANQUE=C:"
check $T/s "Soy el disco duro interno"
check $T/s "Hola desde el CD"
check $T/s "C: es la memoria USB desde la que ha arrancado Hispa-DOS: no se puede"
checkn $T/le.txt "Comando o nombre de archivo incorrecto"
cp "$USBIMG" $T/usb.img; tests/tools/sinmenu.sh $T/usb.img || exit 1
config_mas $T/usb.img "LETRAS=CD:R,USB:U"
{ sleep 9
  send "ECHO =E3=" 1; send "UNIDADES" 2; send "SET" 1.5; send "TYPE R:\\CD.TXT" 3
  send "ECHO =E4=" 1
  mon quit; } | maquina e2 40 $(usb $T/usb.img) $(hd $T/hdd.img) $CD
echo "-- e) LETRAS=CD:R,USB:U --"
part $T/le2.txt "=E3=" "=E4=" > $T/s
check $T/s "  C:  Disco duro interno (disco 2), FAT16, 63 MB"
check $T/s "  R:  Lector de CD/DVD"
check $T/s "  U:  Memoria USB (disco 1)"
check $T/s "ARRANQUE=U:"
check $T/s "Hola desde el CD"
checkn $T/le2.txt "Comando o nombre de archivo incorrecto"
# la letra pedida para el CD, ocupada: la siguiente, y se avisa
cp "$USBIMG" $T/usb.img; tests/tools/sinmenu.sh $T/usb.img || exit 1
config_mas $T/usb.img "LETRAS=CD:X"
{ sleep 9
  send "ECHO =E5=" 1; send "UNIDADES" 2
  send "ECHO =E6=" 1
  mon quit; } | maquina e3 40 $(usb $T/usb.img) $(hd $T/hdd.img) $CD
echo "-- e) LETRAS=CD:X con la X: ocupada (la memoria): la siguiente libre, y se avisa --"
check $T/le3.txt "LETRAS: la X: está ocupada; el CD/DVD será la D:"
check $T/le3.txt "Bienvenido a Hispa-DOS"
part $T/le3.txt "=E5=" "=E6=" > $T/s
check $T/s "  X:  Memoria USB (disco 1)"
check $T/s "  D:  Lector de CD/DVD"
fi
# ------------------------------------------------------------------ f
if toca f; then
if [ -f "$USB486" ]; then
cp "$USB486" $T/usb.img; tests/tools/sinmenu.sh $T/usb.img || exit 1
disco $T/hdd.img 1
tecla() { local k; for k in "$@"; do mon "sendkey $k" 0.3; done; }
{ sleep 26; mon "screendump $T/f1.ppm" 1
  tecla ctrl-esc; sleep 1.2
  tecla down down down down down; sleep 0.4; tecla right; sleep 1.2
  tecla ret; sleep 8; mon "screendump $T/f2.ppm" 1
  tecla alt-f4; sleep 2
  tecla ctrl-esc; sleep 1; tecla up; sleep 0.4; tecla ret; sleep 2; tecla down down; sleep 0.4; tecla ret; sleep 5
  send "ECHO =F1=" 1; send "TYPE X:\\HISPADOS\\CENTRO.INI" 3
  send "ECHO =F2=" 1
  mon quit; } | maquina f 100 -m 32 $(usb $T/usb.img) $(hd $T/hdd.img)
echo "-- f) Hispa-486 con la memoria en la X: --"
python3 -I tests/tools/ventanas_check.py $T/f1.ppm tam 800 600 texto 26 578 8 4 negrita "Inicio" || fail=1
python3 -I tests/tools/ventanas_check.py $T/f2.ppm titulo 60 50 activa "Calculadora" || fail=1
part $T/lf.txt "=F1=" "=F2=" > $T/s
check $T/s "Programa=\\HISPADOS\\PROGS\\PLUMA.HVP"
checkn $T/s "Programa=X:"
checkn $T/s "Programa=C:"
else echo "-- f) sin $USB486 (make usb486): se salta --"; fi
fi
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then
  for f in $T/l*.txt; do echo "--- $f ---"; grep -v '^[║╔╚╟]' $f; done
fi
exit $fail
