#!/bin/bash
# ACTUALIZAR EN EL SITIO: INSTALAR sobre un disco que YA tiene Hispa-DOS.
# El caso normal de Carlos: arranca desde el USB NUEVO y actualiza el disco
# interno sin perder sus datos ni sus ajustes.
#   1) Primero se instala Hispa-DOS en un disco duro vacío (como en
#      run_instalar_usb: PARTIR crea y activa una primaria, INSTALAR da formato
#      y copia). Queda un disco arrancable con el sistema.
#   2) Se «ensucia» ese disco con DATOS y AJUSTES del usuario: un RED.INI propio
#      (con una IP y una clave ACTUALIZAR= reconocibles), un VENTANAS.INI, un
#      JUEGOS\MIO.TXT y un fichero suelto en la raíz. Además se estropean a
#      propósito VENTANAS.COM y ACTUALIZ.COM (se ponen «viejos»), para ver que
#      la actualización SÍ los sustituye por los del origen.
#   3) Se arranca otra vez desde el USB y se ejecuta «INSTALAR D:» hacia ese
#      disco: como ya hay Hispa-DOS, sale el menú y se elige [A] Actualizar.
#   4) Se comprueba que:
#      - los binarios del sistema están y son los del origen (KERNEL.SYS,
#        COMMAND.COM, VENTANAS.COM, ACTUALIZ.COM: VENTANAS/ACTUALIZ ya NO son
#        los «viejos», se han sustituido),
#      - el RED.INI del usuario CONSERVA su contenido (no lo pisó el de serie),
#      - VENTANAS.INI, JUEGOS\MIO.TXT y el fichero suelto SIGUEN ahí,
#      - existe CONFIG.ANT y AUTOEXEC.ANT (copias de los anteriores).
#   5) Se arranca SOLO el disco actualizado: Hispa-DOS arranca y el RED.INI
#      sigue siendo el del usuario.
# Update-in-place test: INSTALAR onto a disk that already has Hispa-DOS.
set -u
cd "$(dirname "$0")/.."
USBIMG=${1:-build/hispados-usb.img}
[ -f "$USBIMG" ] || { echo "falta $USBIMG (make usb)"; exit 1; }
for f in KERNEL.SYS COMMAND.COM VENTANAS.COM ACTUALIZAR.COM; do
  [ -f build/$f ] || { echo "falta build/$f (make)"; exit 1; }
done
T=$(mktemp -d)
export MTOOLS_SKIP_CHECK=1
OFF=32256                 # la primaria empieza en el sector 63 (63*512)
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$2" "$1"; then ok "$2"; else ko "$2"; fi; }
checkn() { if grep -qF -- "$2" "$1"; then ko "(no debe salir) $2"; else ok "no sale: $2"; fi; }
send() { sleep "${2:-1.5}"; printf '%s\r' "$1"; }
key() { sleep "${2:-1}"; printf '%s' "$1"; }
txt() { tr -d '\r' < $1 | iconv -f CP437 -t UTF-8 > $1.txt; }
part() { awk -v a="$2" -v b="$3" 'index($0, a) {p=1; next} p && index($0, b) {exit} p' "$1"; }
QEMU="timeout 150 qemu-system-i386 ${QEMU_PANTALLA--vga std -global VGA.xres=800 -global VGA.yres=600} -display none -serial stdio -monitor none -no-reboot"
HDD=$T/hdd.img
dd() { command dd "$@"; }
mdos() { mtype -i $HDD@@$OFF "$@" 2>/dev/null; }

cp "$USBIMG" $T/usb.img
tests/tools/sinmenu.sh $T/usb.img || exit 1
command dd if=/dev/zero of=$HDD bs=1M count=200 status=none

echo "== ACTUALIZAR en el sitio (INSTALAR sobre un disco con Hispa-DOS) =="

# ------------------------------------------------------------------ 1
# instalación nueva (como run_instalar_usb, caso 1)
{
  sleep 8
  send "PARTIR" 1
  key "1" 3             # crear...
  key "1" 1             # ...una primaria
  send "100" 2          # 100 MB
  key $'\e' 3
  key "2" 1             # activar
  send "1" 2
  key $'\e' 3
  key $'\e' 1           # salir
  send "ECHO =INST=" 2
  send "INSTALAR" 1
  send "S" 5            # dar formato a D: e instalar
  sleep 28
  send "ECHO =FIN1=" 1
} | $QEMU -drive file=$T/usb.img,format=raw,if=ide,index=0 \
      -drive file=$HDD,format=raw,if=ide,index=1 -boot c > $T/l1 2>&1
txt $T/l1
echo "-- 1) instalación nueva en el disco duro --"
part $T/l1.txt "=INST=" "=FIN1=" > $T/s1
check $T/s1 "Hispa-DOS instalado en la unidad D:"
check $T/s1 "Quite la memoria USB"
if mdos ::HISPADOS/VENTANAS.COM > /dev/null; then ok "el disco quedó con \\HISPADOS instalado"; else ko "no se instaló Hispa-DOS en el disco"; fi

# ------------------------------------------------------------------ 2
# ensuciar el disco con datos y ajustes del usuario
echo "-- 2) datos y ajustes del usuario en el disco --"
printf 'DHCP=NO\r\nIP=192.168.77.77\r\nACTUALIZAR=http://miservidor.casa/ultima.txt\r\n' > $T/RED.INI
printf 'TEMA=OSCURO\r\nFONDO=MIFOTO.BMP\r\n' > $T/VENTANAS.INI
printf 'Esto es mio y no se toca.\r\n' > $T/MIO.TXT
printf 'Carta del usuario, no del sistema.\r\n' > $T/MICARTA.TXT
printf 'VIEJO-NO-VALIDO\r\n' > $T/VIEJO.BIN
mcopy -o -i $HDD@@$OFF $T/RED.INI ::HISPADOS/RED.INI
mcopy -o -i $HDD@@$OFF $T/VENTANAS.INI ::HISPADOS/VENTANAS.INI
mcopy -o -i $HDD@@$OFF $T/VIEJO.BIN ::HISPADOS/VENTANAS.COM
mcopy -o -i $HDD@@$OFF $T/VIEJO.BIN ::HISPADOS/ACTUALIZ.COM
mmd -i $HDD@@$OFF ::JUEGOS 2>/dev/null
mcopy -o -i $HDD@@$OFF $T/MIO.TXT ::JUEGOS/MIO.TXT
mcopy -o -i $HDD@@$OFF $T/MICARTA.TXT ::MICARTA.TXT
if mdos ::HISPADOS/RED.INI | grep -qF "192.168.77.77"; then ok "RED.INI del usuario puesto"; else ko "no se pudo poner el RED.INI del usuario"; fi

# ------------------------------------------------------------------ 3
# arrancar desde el USB y ACTUALIZAR el disco (menú -> [A])
{
  sleep 8
  send "ECHO =ACT=" 1
  send "INSTALAR D:" 2
  send "A" 3           # [A] Actualizar
  send "S" 4           # ¿Continuar? Sí
  sleep 26
  send "ECHO =FIN3=" 1
} | $QEMU -drive file=$T/usb.img,format=raw,if=ide,index=0 \
      -drive file=$HDD,format=raw,if=ide,index=1 -boot c > $T/l3 2>&1
txt $T/l3
echo "-- 3) INSTALAR D: en modo ACTUALIZAR --"
part $T/l3.txt "=ACT=" "=FIN3=" > $T/s3
check $T/s3 "ya tiene Hispa-DOS instalado"
check $T/s3 "[A] Actualizar"
check $T/s3 "Se va a ACTUALIZAR Hispa-DOS en la unidad D:"
check $T/s3 "no se formatea nada"
check $T/s3 "guardado como CONFIG.ANT"
check $T/s3 "Hispa-DOS actualizado en la unidad D:"
check $T/s3 "Conservados tus datos y ajustes:"
check $T/s3 "Quita el USB"
checkn $T/s3 "Formato completado."
checkn $T/s3 "La partición 1 del disco 2 es la activa"

# ------------------------------------------------------------------ 4
echo "-- 4) binarios actualizados y datos del usuario conservados --"
# binarios del sistema presentes y del origen
for pair in "::KERNEL.SYS|build/KERNEL.SYS" "::COMMAND.COM|build/COMMAND.COM" \
            "::HISPADOS/VENTANAS.COM|build/VENTANAS.COM" "::HISPADOS/ACTUALIZ.COM|build/ACTUALIZAR.COM"; do
  src=${pair#*|}; dst=${pair%|*}
  mcopy -i $HDD@@$OFF "$dst" $T/bin.out 2>/dev/null
  if [ -f $T/bin.out ] && cmp -s $T/bin.out "$src"; then ok "$dst presente y del origen"; else ko "$dst no coincide con $src"; fi
  rm -f $T/bin.out
done
# VENTANAS.COM y ACTUALIZ.COM ya NO son los «viejos»
if mdos ::HISPADOS/VENTANAS.COM | grep -qF "VIEJO-NO-VALIDO"; then ko "VENTANAS.COM sigue siendo el viejo (no se actualizó)"; else ok "VENTANAS.COM se sustituyó por el nuevo"; fi
if mdos ::HISPADOS/ACTUALIZ.COM | grep -qF "VIEJO-NO-VALIDO"; then ko "ACTUALIZ.COM sigue siendo el viejo"; else ok "ACTUALIZ.COM se sustituyó por el nuevo"; fi
# datos del usuario conservados
mdos ::HISPADOS/RED.INI > $T/red.txt
check $T/red.txt "192.168.77.77"
check $T/red.txt "miservidor.casa"
mdos ::HISPADOS/VENTANAS.INI > $T/vent.txt
check $T/vent.txt "TEMA=OSCURO"
mdos ::JUEGOS/MIO.TXT > $T/mio.txt
check $T/mio.txt "Esto es mio y no se toca."
mdos ::MICARTA.TXT > $T/carta.txt
check $T/carta.txt "Carta del usuario, no del sistema."
# copias de CONFIG.SYS y AUTOEXEC.BAT anteriores
if mdos ::CONFIG.ANT | grep -q .; then ok "CONFIG.ANT (copia del CONFIG.SYS anterior)"; else ko "falta CONFIG.ANT"; fi
if mdos ::AUTOEXEC.ANT | grep -q .; then ok "AUTOEXEC.ANT (copia del AUTOEXEC.BAT anterior)"; else ko "falta AUTOEXEC.ANT"; fi
# CONFIG.SYS nuevo instalado y con la letra C:
mdos ::CONFIG.SYS > $T/cfg.txt
if [ -s $T/cfg.txt ]; then ok "CONFIG.SYS nuevo instalado"; else ko "falta el CONFIG.SYS nuevo"; fi
if grep -qi "[ABD]:\\\\" $T/cfg.txt; then ko "el CONFIG.SYS nuevo tiene otra letra que C:"; else ok "el CONFIG.SYS nuevo usa C:"; fi

# ------------------------------------------------------------------ 5
echo "-- 5) arrancar el disco actualizado --"
{
  sleep 8
  send "ECHO =B1=" 1
  send "TYPE C:\\HISPADOS\\RED.INI" 2
  send "DIR C:\\" 2
  send "ECHO =B2=" 1
} | timeout 50 qemu-system-i386 ${QEMU_PANTALLA--vga std -global VGA.xres=800 -global VGA.yres=600} -display none -serial stdio -monitor none -no-reboot \
      -drive file=$HDD,format=raw,if=ide -boot c > $T/l5 2>&1
txt $T/l5
check $T/l5.txt "Bienvenido a Hispa-DOS"
part $T/l5.txt "=B1=" "=B2=" > $T/b1
check $T/b1 "192.168.77.77"
check $T/b1 "CONFIG   ANT"
check $T/b1 "MICARTA  TXT"
checkn $T/l5.txt "Error"

if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then
  for i in 1 3 5; do echo "--- arranque $i ---"; cat $T/l$i.txt; done
fi
rm -rf $T
exit $fail
