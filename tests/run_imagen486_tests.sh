#!/bin/bash
# Prueba de la imagen de Hispa-486 (build/hispa486-usb.img, «make usb486»;
# docs/486/HISPA-486.md, «La imagen»), de instalarla en un disco duro y de
# actualizarla.
#  1) Arrancar la memoria USB de Hispa-486: el AUTOEXEC.BAT abre el escritorio
#     de Hispa-486 (la barra de tareas abajo con «Inicio», sin el Centro de
#     programas ni la barra de arriba); el menú Inicio lleva el grupo
#     «Hispa32» con sus programas; desde él se abre la calculadora de 32 bits
#     (CALC32.EXE), que calcula, y «Hola, mundo». Al salir de Hispa Ventanas,
#     el intérprete de órdenes con el aviso de INSTALL.
#  2) INSTALL desde ella en un disco duro vacío: el disco queda con
#     \HISPA32, EDICION.INI, la clave de VENTANAS.INI, el CENTRO.INI con el
#     grupo «Hispa32» y el AUTOEXEC.BAT que arranca el escritorio (solo C:).
#  3) Arrancar solo el disco duro: el escritorio de Hispa-486, con Hispa32.
#  4) Actualizar (A) un Hispa-486 con la memoria de Hispa-486: se conservan
#     sus ajustes (no hay .ANT de VENTANAS.INI).
#  5) Actualizar (A) un Hispa-386 con la memoria de Hispa-486: pasa a
#     Hispa-486; su VENTANAS.INI y su CENTRO.INI quedan como .ANT.
#  6) Actualizar (A) un Hispa-486 con la memoria de Hispa-386: no se deja
#     (no se toca nada).
#  7) ACTUALIZAR por la red en el Hispa-486 instalado: dice «(Hispa-486)»,
#     no mira la sección de Hispa-386 (gama sin poner), recomienda la de
#     «gama=486» y la instala, con su \HISPA32.
#  8) Instalar desde disquetes: el de arranque, «Instalación 2», «Oficina»
#     (aquí, no) y al final «Hispa-486» (sí): el disco queda de Hispa-486,
#     con el AUTOEXEC.BAT que arranca el escritorio.
#  (GUARDAR=carpeta deja las capturas.)
#  uso: run_imagen486_tests.sh [hispa486-usb.img] [hispados-usb.img]
# Hispa-486 USB image, hard disk install and update tests.
set -u
cd "$(dirname "$0")/.."
USB=${1:-build/hispa486-usb.img}
USB386=${2:-build/hispados-usb.img}
FLOPPY=${FLOPPY:-build/hispados.img}               # los disquetes de la 8.ª
IN2=${INSTAL2:-build/hispados-instal2.img}
D486=${DISQ486:-build/hispa486-disquete.img}
# sin imagen dada, la de ahora (make usb486; sin gcc de 32 bits no se puede)
if [ $# -eq 0 ] && [ -z "${ACT_NS:-}" ]; then
  make -s usb486 > /dev/null 2>&1 || { echo "== Hispa-486: la imagen USB: sin gcc de 32 bits (make usb486), se salta =="; exit 0; }
fi
[ -f "$USB" ] || { echo "falta $USB (make usb486)"; exit 1; }
[ -f "$USB386" ] || { echo "falta $USB386 (make usb)"; exit 1; }
# la red de la 7.ª, en un espacio de nombres propio (no usa Internet)
if [ -z "${ACT_NS:-}" ] && unshare -rn true 2>/dev/null; then
  ACT_NS=1 exec unshare -rn "$0" "$@"
fi
[ -n "${ACT_NS:-}" ] && ip link set lo up 2>/dev/null
T=$(mktemp -d)
export MTOOLS_SKIP_CHECK=1
PORT=${PORT:-8097}
OFF=32256                 # la partición del disco duro empieza en el sector 63
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$2" "$1"; then ok "$2"; else ko "$2"; fi; }
checkn() { if grep -qF -- "$2" "$1"; then ko "(no debe salir) $2"; else ok "no sale: $2"; fi; }
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.3}"; }
tecla() { local k; for k in "$@"; do mon "sendkey $k" 0.3; done; }
shot() { sleep 0.4; mon "screendump $T/$1.ppm" 0.6; }
send() { printf '%s\r' "$1"; sleep "${2:-1.5}"; }
chk() { python3 -I tests/tools/ventanas_check.py "$@" || fail=1; }
txt() { tr -d '\r' < $1 | iconv -f CP437 -t UTF-8 -c > $1.txt; }
# lo que hay en el disco duro (la partición, por mtools)
hd() { mtype -i $T/hdd.img@@$OFF "$@" 2>/dev/null | tr -d '\r' | iconv -f CP437 -t UTF-8 -c; }
hdtiene() { if mdir -i $T/hdd.img@@$OFF "::$1" > /dev/null 2>&1; then ok "en el disco duro: $1"; else ko "falta en el disco duro: $1"; fi; }
hdnotiene() { if mdir -i $T/hdd.img@@$OFF "::$1" > /dev/null 2>&1; then ko "(no debe estar) $1"; else ok "no está: $1"; fi; }
# máquina N TIEMPO opciones-de-QEMU... (lo que se teclea, por la entrada)
maquina() {
  local n=$1 tmo=$2; shift 2
  rm -f $T/mon
  timeout $tmo qemu-system-i386 -display none -serial stdio -monitor unix:$T/mon,server,nowait \
    -parallel none -m 32 -no-reboot "$@" > $T/l$n 2>&1
  txt $T/l$n
}
USBD="-drive file=$T/usb.img,format=raw,if=ide,index=0"
HDD="-drive file=$T/hdd.img,format=raw,if=ide,index=1"
# salir de Hispa Ventanas por el menú Inicio («Salir de Hispa Ventanas...», el
# último) y aceptar
salir() { tecla ctrl-esc; sleep 1; tecla up; sleep 0.4; tecla ret; sleep 2; tecla ret; sleep 5; }

cp "$USB" $T/usb.img
tests/tools/sinmenu.sh $T/usb.img || exit 1
cp $T/usb.img $T/usb0.img
dd if=/dev/zero of=$T/hdd.img bs=1M count=200 status=none

echo "== Hispa-486: la imagen USB, instalarla y actualizarla =="
# ------------------------------------------------------------------ 1
# 800x600. La barra: de y = 572 a 599; «Inicio», en 4,577. El menú Inicio
# sube desde la barra; los grupos, primero (Principal, Accesorios, Juegos,
# Sonido, Productividad, Hispa32), y el submenú a su derecha.
{ sleep 22; shot a1                                 # el escritorio de Hispa-486
  tecla ctrl-esc; sleep 1.2; shot a2                # el menú Inicio
  tecla down down down down down; sleep 0.4; tecla right; sleep 1.2; shot a3   # «Hispa32»
  tecla ret; sleep 8; shot a4                       # «Calculadora»: CALC32.EXE
  tecla 6 kp_multiply 7; sleep 0.5; tecla ret; sleep 1.5; shot a5      # 6 × 7
  tecla alt-f4; sleep 2
  tecla ctrl-esc; sleep 1; tecla down down down down down; sleep 0.4; tecla right; sleep 0.8
  tecla down; sleep 0.4; tecla ret; sleep 8; shot a6    # «Hola, mundo»: HOLA32.EXE
  tecla alt-f4; sleep 2
  salir
  send 'ECHO TRAS-SALIR' 2
  mon quit; } | maquina 1 150 $USBD -boot c
echo "-- 1) la memoria de Hispa-486 arranca a su escritorio --"
chk $T/a1.ppm tam 800 600 px 400 572 10 "el filete de la barra de tareas" px 84 590 1 "la barra de tareas" \
  texto 26 578 8 4 negrita "Inicio" nopx 400 10 1 "sin barra arriba" \
  nobusca 0 0 500 300 1 6 "Centro de programas"
echo "-- el menú Inicio, con el grupo «Hispa32» --"
chk $T/a2.ppm busca 0 0 260 572 9 12 "Principal" busca 0 0 260 572 0 8 "Accesorios" \
  busca 0 0 260 572 0 8 "Productividad" busca 0 0 260 572 0 8 "Hispa32" \
  busca 0 0 260 572 0 8 "Salir de Hispa Ventanas..." nobusca 0 0 800 572 0 8 "Windows"
chk $T/a3.ppm busca 250 0 600 572 9 12 "Calculadora" \
  busca 250 0 600 572 0 8 "Hola, mundo" busca 250 0 600 572 0 8 "Controles" \
  busca 250 0 600 572 0 8 "Menús y diálogos" busca 250 0 600 572 0 8 "Cuadros de mensaje"
echo "-- la calculadora de 32 bits, desde el menú --"
chk $T/a4.ppm titulo 60 50 activa "Calculadora" busca 88 574 260 582 0 8 "Calculadora"
chk $T/a5.ppm busca 68 98 256 132 0 9 "42"
chk $T/a6.ppm titulo 48 52 activa "Hola desde Win32" texto 60 87 16 231 fina "¡Hola, mundo de Win32!" busca 88 574 300 582 0 8 "Hola desde Win32"
check $T/l1.txt "Hispa Ventanas ha terminado."
if sed -n '/Hispa Ventanas ha terminado/,$p' $T/l1.txt | grep -qF 'TRAS-SALIR'; then ok "después, el intérprete de órdenes"; else ko "sin el intérprete tras salir"; fi
check $T/l1.txt "Para instalarlo en el disco duro escriba INSTALL"
[ -n "${GUARDAR:-}" ] && for f in $T/a[0-9].ppm; do cp $f "$GUARDAR"/i486_$(basename $f); done 2>/dev/null

# ------------------------------------------------------------------ 2
cp $T/usb0.img $T/usb.img
{ sleep 22; salir
  send 'ECHO =I1=' 1
  send 'INSTALL' 4
  send 'S' 4                # crear la partición con todo el disco
  send 'S' 6                # darle formato e instalar
  sleep 30
  send 'ECHO =I2=' 2
  mon quit; } | maquina 2 150 $USBD $HDD -boot c
echo "-- 2) INSTALL de Hispa-486 en un disco duro vacío --"
check $T/l2.txt "Hispa-DOS instalado en la unidad D:"
check $T/l2.txt "Hispa-DOS esta instalado."
checkn $T/l2.txt "Hispa-486 (el escritorio con el menú Inicio"     # (el disquete: aquí no)
for f in HISPA32/CALC32.EXE HISPA32/HOLA32.EXE HISPA32/CTL32.EXE HISPA32/MENU32.EXE HISPA32/AVISO32.EXE \
         HISPADOS/EDICION.INI HISPADOS/VENTANAS.INI HISPADOS/CENTRO.INI HISPADOS/HW32.COM HISPADOS/MSVCRT.DLL; do
  hdtiene $f
done
hd ::HISPADOS/EDICION.INI > $T/edi.txt
grep -qx 'Gama=486' $T/edi.txt && ok "EDICION.INI: Gama=486" || ko "EDICION.INI sin Gama=486"
hd ::HISPADOS/VENTANAS.INI > $T/ven.txt
grep -qx 'Escritorio=486' $T/ven.txt && ok "VENTANAS.INI: Escritorio=486" || ko "VENTANAS.INI sin Escritorio=486"
hd ::HISPADOS/CENTRO.INI > $T/cen.txt
grep -qx 'Nombre=Hispa32' $T/cen.txt && ok "CENTRO.INI: el grupo «Hispa32»" || ko "CENTRO.INI sin el grupo Hispa32"
grep -qx 'Programa=C:\\HISPA32\\CALC32.EXE' $T/cen.txt && ok "CENTRO.INI: la calculadora, en C:\\HISPA32" || ko "CENTRO.INI sin CALC32.EXE"
hd ::AUTOEXEC.BAT > $T/aut.txt
grep -qx 'C:\\HISPADOS\\VENTANAS' $T/aut.txt && ok "AUTOEXEC.BAT: arranca el escritorio" || ko "AUTOEXEC.BAT sin C:\\HISPADOS\\VENTANAS"
if grep -qi "[ABD]:\\\\" $T/aut.txt; then ko "AUTOEXEC.BAT con otra letra que C:"; else ok "AUTOEXEC.BAT: solo C:"; fi
cp $T/hdd.img $T/hdd486.img

# ------------------------------------------------------------------ 3
{ sleep 20; shot h1
  tecla ctrl-esc; sleep 1.2; shot h2
  tecla esc; sleep 0.8
  salir
  send 'VER' 2
  mon quit; } | maquina 3 90 -drive file=$T/hdd.img,format=raw,if=ide -boot c
echo "-- 3) el disco duro arranca a su escritorio de Hispa-486 --"
chk $T/h1.ppm tam 800 600 px 400 572 10 "el filete de la barra de tareas" texto 26 578 8 4 negrita "Inicio" \
  nopx 400 10 1 "sin barra arriba"
chk $T/h2.ppm busca 0 0 260 572 0 8 "Hispa32" busca 0 0 260 572 9 12 "Principal"
check $T/l3.txt "Hispa Ventanas ha terminado."
check $T/l3.txt "Hispa-DOS versión 0.9.25"
[ -n "${GUARDAR:-}" ] && for f in $T/h[0-9].ppm; do cp $f "$GUARDAR"/i486_$(basename $f); done 2>/dev/null

# instalar en un disco que ya tiene Hispa-DOS: INSTALL, [A] y S (USB de 486)
actualiza() {
  local n=$1 usb=$2 espera=$3
  cp $usb $T/usb.img
  { sleep $espera
    [ "$usb" = "$T/usb0.img" ] && salir
    send "ECHO =U$n=" 1
    send 'INSTALL' 4
    send 'A' 4
    send 'S' 4
    sleep 25
    send "ECHO =V$n=" 2
    mon quit; } | maquina $n 150 $USBD $HDD -boot c
}
# ------------------------------------------------------------------ 4
cp $T/hdd486.img $T/hdd.img
printf '[Pantalla]\r\nDibujo=liso\r\n[Escritorio]\r\nEscritorio=486\r\n' > $T/v4.ini
mcopy -o -i $T/hdd.img@@$OFF $T/v4.ini ::HISPADOS/VENTANAS.INI
actualiza 4 $T/usb0.img 22
echo "-- 4) actualizar un Hispa-486 con la memoria de Hispa-486 --"
check $T/l4.txt "ya tiene Hispa-DOS instalado."
check $T/l4.txt "Hispa-DOS actualizado en la unidad D:"
checkn $T/l4.txt "pasa a Hispa-486"
hd ::HISPADOS/VENTANAS.INI > $T/ven.txt
grep -qx 'Dibujo=liso' $T/ven.txt && ok "VENTANAS.INI del usuario conservado" || ko "VENTANAS.INI del usuario perdido"
hdnotiene HISPADOS/VENTANAS.ANT
hdtiene HISPA32/CALC32.EXE

# ------------------------------------------------------------------ 5
# un Hispa-386: el mismo disco sin lo de la edición, con sus ajustes
cp $T/hdd486.img $T/hdd.img
mdel -i $T/hdd.img@@$OFF ::HISPADOS/EDICION.INI
mdeltree -i $T/hdd.img@@$OFF ::HISPA32
printf '[Pantalla]\r\nDibujo=liso\r\n' > $T/v5.ini
mcopy -o -i $T/hdd.img@@$OFF $T/v5.ini ::HISPADOS/VENTANAS.INI
printf '[Centro]\r\nActivo=0\r\n[Grupo]\r\nNombre=Lo mio\r\n' > $T/c5.ini
mcopy -o -i $T/hdd.img@@$OFF $T/c5.ini ::HISPADOS/CENTRO.INI
hd ::AUTOEXEC.BAT | grep -v 'HISPADOS\\VENTANAS' | iconv -f UTF-8 -t CP437 | sed 's/$/\r/' > $T/a5.bat
mcopy -o -i $T/hdd.img@@$OFF $T/a5.bat ::AUTOEXEC.BAT
actualiza 5 $T/usb0.img 22
echo "-- 5) actualizar un Hispa-386 con la memoria de Hispa-486: pasa a Hispa-486 --"
check $T/l5.txt "Esa unidad tiene Hispa-386 y este origen es de Hispa-486"
check $T/l5.txt "Tu VENTANAS.INI anterior se ha guardado como VENTANAS.ANT."
check $T/l5.txt "Tu CENTRO.INI anterior se ha guardado como CENTRO.ANT."
check $T/l5.txt "Hispa-DOS actualizado en la unidad D:"
hd ::HISPADOS/VENTANAS.ANT | grep -qx 'Dibujo=liso' && ok "VENTANAS.ANT: el del usuario" || ko "VENTANAS.ANT"
hd ::HISPADOS/CENTRO.ANT | grep -qx 'Nombre=Lo mio' && ok "CENTRO.ANT: el del usuario" || ko "CENTRO.ANT"
hd ::HISPADOS/VENTANAS.INI | grep -qx 'Escritorio=486' && ok "VENTANAS.INI: el de Hispa-486" || ko "VENTANAS.INI sin Escritorio=486"
hd ::HISPADOS/CENTRO.INI | grep -qx 'Nombre=Hispa32' && ok "CENTRO.INI: el de Hispa-486" || ko "CENTRO.INI sin Hispa32"
hd ::HISPADOS/EDICION.INI | grep -qx 'Gama=486' && ok "EDICION.INI puesto" || ko "sin EDICION.INI"
hdtiene HISPA32/CALC32.EXE
hd ::AUTOEXEC.BAT | grep -qx 'C:\\HISPADOS\\VENTANAS' && ok "AUTOEXEC.BAT: arranca el escritorio" || ko "AUTOEXEC.BAT sin VENTANAS"

# ------------------------------------------------------------------ 6
cp $T/hdd486.img $T/hdd.img
cp "$USB386" $T/u386.img
tests/tools/sinmenu.sh $T/u386.img || exit 1
md5sum < $T/hdd.img > $T/md5a
actualiza 6 $T/u386.img 12
echo "-- 6) un Hispa-486 no se actualiza con la memoria de Hispa-386 --"
check $T/l6.txt "Esa unidad tiene Hispa-486 y este origen es de Hispa-386"
check $T/l6.txt "No se ha instalado nada."
checkn $T/l6.txt "Hispa-DOS actualizado"
md5sum < $T/hdd.img > $T/md5b
cmp -s $T/md5a $T/md5b && ok "el disco duro no ha cambiado" || ko "el disco duro ha cambiado"

# ------------------------------------------------------------------ 7
cp $T/hdd486.img $T/hdd.img
printf 'DHCP=SI\r\nACTUALIZAR=http://10.0.2.2:%s/versiones.txt\r\n' "$PORT" > $T/RED.INI
mcopy -o -i $T/hdd.img@@$OFF $T/RED.INI ::HISPADOS/RED.INI
mkdir -p $T/www $T/p386/HISPADOS $T/p486/HISPADOS $T/p486/HISPA32
printf 'de 386\r\n' > $T/p386/HISPADOS/NUEVO386.TXT
printf 'de 486\r\n' > $T/p486/HISPADOS/NUEVO486.TXT
printf 'de 486\r\n' > $T/p486/HISPA32/NUEVO32.TXT
python3 -I tools/paquete.py $T/p386 $T/www/h386.paq > /dev/null || ko "paquete.py (386)"
python3 -I tools/paquete.py $T/p486 $T/www/h486.paq > /dev/null || ko "paquete.py (486)"
{ printf '[0.9.99]\ncpu=386\nnota=Paquete de Hispa-386\npaquete=http://10.0.2.2:%s/h386.paq\n\n' "$PORT"
  printf '[0.9.98]\ngama=486\ncpu=386\nedicion=Hispa-486\nnota=Paquete de Hispa-486\npaquete=http://10.0.2.2:%s/h486.paq\n' "$PORT"
} > $T/www/versiones.txt
python3 -m http.server --bind 127.0.0.1 --directory $T/www $PORT > $T/http.log 2>&1 &
HTTP_PID=$!
sleep 1
{ sleep 20; salir
  send 'ECHO =R1=' 1
  send 'ACTUALIZAR' 14
  send 'S' 20
  send 'ECHO =R2=' 2
  mon quit; } | maquina 7 120 -drive file=$T/hdd.img,format=raw,if=ide -boot c -netdev user,id=n0 -device ne2k_pci,netdev=n0
kill $HTTP_PID 2>/dev/null
echo "-- 7) ACTUALIZAR en el Hispa-486 instalado: su gama --"
check $T/l7.txt "Tienes instalado Hispa-DOS 0.9.25 (Hispa-486)."
check $T/l7.txt "Paquete de Hispa-486"
check $T/l7.txt "¿Descargar e instalar la 0.9.98?"
checkn $T/l7.txt "Paquete de Hispa-386"
checkn $T/l7.txt "0.9.99"
hdtiene HISPADOS/NUEVO486.TXT
hdtiene HISPA32/NUEVO32.TXT
hdnotiene HISPADOS/NUEVO386.TXT

# ------------------------------------------------------------------ 8
cp "$FLOPPY" $T/f.img; cp "$IN2" $T/i2.img; cp "$D486" $T/d4.img
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDOS=HIGH\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
dd if=/dev/zero of=$T/hdd.img bs=1M count=32 status=none
cambia() { mon "change floppy0 $1 raw" 2; }
# espera N TEXTO: hasta que TEXTO haya salido N veces por la consola (como mucho 2 min)
espera() { local i=0; until [ "$(grep -caF -- "$2" $T/l8 2>/dev/null)" -ge $1 ] || [ $i -ge 240 ]; do sleep 0.5; i=$((i+1)); done; sleep 0.5; }
{ sleep 4
  send 'FDISK /PRI' 2
  send 'FORMAT C: /V:DESTINO /AUTOTEST' 4
  send 'INSTALAR C:' 1
  espera 1 '(N para seguir sin'
  cambia $T/i2.img; send 'S' 1                       # «Instalación 2»
  espera 1 'pulse una tecla.'
  cambia $T/f.img; printf ' '
  espera 1 '(N para terminar sin'
  send 'N' 1                                         # «Oficina»: no
  espera 1 '(N para quedarse con'
  cambia $T/d4.img; send 'S' 1                       # «Hispa-486»: sí
  espera 2 'pulse una tecla.'
  cambia $T/f.img; printf ' '; sleep 3
  send 'ECHO =F2=' 2
  mon quit; } | maquina 8 200 -drive file=$T/f.img,format=raw,if=floppy -drive file=$T/hdd.img,format=raw,if=ide -boot a
echo "-- 8) desde disquetes, con el de Hispa-486 al final --"
check $T/l8.txt "Hispa-DOS instalado en la unidad C:"
check $T/l8.txt "va en el disquete «Hispa-486»."
check $T/l8.txt "Hispa-486: "
OFF8=
for off in 32256 1048576; do mdir -i $T/hdd.img@@$off ::HISPADOS > /dev/null 2>&1 && OFF8=$off; done
if [ -n "$OFF8" ]; then
  OFF=$OFF8
  for f in HISPA32/CALC32.EXE HISPADOS/EDICION.INI HISPADOS/VENTANAS.INI HISPADOS/CENTRO.INI HISPADOS/HW32.COM; do hdtiene $f; done
  hd ::AUTOEXEC.BAT > $T/aut8.txt
  grep -qx 'C:\\HISPADOS\\VENTANAS' $T/aut8.txt && ok "AUTOEXEC.BAT: el de Hispa-486" || ko "AUTOEXEC.BAT sin C:\\HISPADOS\\VENTANAS"
  if grep -qi "[AB]:\\\\" $T/aut8.txt; then ko "AUTOEXEC.BAT con A: o B:"; else ok "AUTOEXEC.BAT: solo C:"; fi
else
  ko "el disco duro no tiene \\HISPADOS"
fi

if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then
  for i in 1 2 3 4 5 6 7 8; do echo "--- máquina $i ---"; tail -40 $T/l$i.txt; done
fi
rm -rf $T
exit $fail
