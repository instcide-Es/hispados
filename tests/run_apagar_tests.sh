#!/bin/bash
# Prueba de APAGAR y REINICIAR (utils/apagar.asm) y del cuadro «Salir» de
# Hispa Ventanas (ventanas/apagado.inc), en QEMU:
#  - /? y AYUDA; /INFO en -M pc (PIIX4: FADT 1.0, \_S5 en la DSDT, sin
#    RESET_REG) y en -M q35 (ICH9: FADT 3, RESET_REG CF9h <- 0Fh);
#  - APAGAR en -M pc y en -M q35: QEMU termina solo (sin -no-shutdown) en
#    pocos segundos, y un archivo escrito justo antes (una copia de 24 KB y
#    un ECHO, desde un .BAT que acaba con APAGAR) está entero en la imagen;
#  - sin ACPI (-machine acpi=off): en -M pc no hay tablas y la APM de SeaBIOS
#    no apaga: queda la pantalla final «Ya puede apagar el equipo.» (captura
#    y comprobación de los colores) y QEMU sigue en marcha; en -M q35, según
#    la versión de SeaBIOS, o pone sus propias tablas (\_S5 en una SSDT) y se
#    apaga por ACPI, o no hay ninguna y se apaga por APM;
#    APAGAR /NOACPI apaga por APM (la de SeaBIOS);
#  - REINICIAR con -no-reboot: QEMU termina (pc: controlador del teclado;
#    q35: RESET_REG); cada método por separado (/SOLO:TECLADO, /SOLO:CF9,
#    /SOLO:FALLO) y, sin -no-reboot, que el equipo vuelve a arrancar;
#  - Hispa Ventanas: el botón «Salir» abre el cuadro (captura), Esc lo
#    cancela, Alt+F4 en el escritorio lo abre, «Volver a Hispa-DOS» vuelve al
#    indicador; «Reiniciar el equipo» y «Apagar el equipo» (-M pc y -M q35)
#    guardan CENTRO.INI, quitan HSPDPMI y reinician o apagan;
#  - [APAGAR-USB] arrancando de una memoria USB (SeaBIOS con xHCI y con EHCI;
#    OVMF de 64 y de 32 bits con HispaBIOS): lo cambiado en Configuración
#    (VENTANAS.INI) y CENTRO.INI están en la memoria después de «Apagar el
#    equipo», y fsck.fat la da por sana; APAGAR desde DOS igual (con UEFI,
#    por ResetSystem del firmware); con CENTRO.INI de solo lectura, Hispa
#    Ventanas avisa antes de apagar («Cancelar» se queda, «Aceptar» apaga);
#    un cambio de Configuración hecho justo antes de «Apagar» (sin esperar a
#    que se guarde solo a los 3 s) también está: WM_ENDSESSION.
# Deja capturas en build/apagar_*.png.
# Shutdown / restart test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=$(dirname "$IMG")
T=$(mktemp -d)
CHK="python3 tests/tools/ventanas_check.py"
export MTOOLS_SKIP_CHECK=1
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$1" $T/log.txt; then ok "${2:-$1}"; else ko "${2:-$1}"; fi; }
checkn() { if grep -qF -- "$1" $T/log.txt; then ko "(no debía salir) $1"; else ok "sin \"$1\""; fi; }
# (la línea entera: /INFO también nombra el mensaje final)
checkxn() { if grep -qxF -- "$1" $T/log.txt; then ko "(no debía salir) $1"; else ok "sin la línea \"$1\""; fi; }
chk() { $CHK "$@" || fail=1; }
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.3}"; }
tecla() { local k; for k in "$@"; do mon "sendkey $k" 0.3; done; }
shot() { sleep 0.4; mon "screendump $T/$1.ppm" 0.6; }
muestra() { [ -f $T/$1.ppm ] && convert $T/$1.ppm $B/apagar_$2.png 2>/dev/null; }
espera() {
  local n=0
  until tr -d '\r' < $T/log 2>/dev/null | grep -aq -- "$1"; do
    sleep 0.25; n=$((n + 1)); [ $n -ge $((4 * ${2:-60})) ] && return 1
  done
  return 0
}
# arranca "opciones de QEMU": en segundo plano, con la consola serie en
# $T/log y el teclado de la consola en el descriptor 3
arranca() {
  : > $T/log; rm -f $T/in $T/mon $T/rc; mkfifo $T/in
  ( timeout 120 qemu-system-i386 -display none -serial stdio -monitor unix:$T/mon,server,nowait \
      -parallel none -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a $1 < $T/in > $T/log 2>&1
    echo $? > $T/rc ) &
  QP=$!
  exec 3> $T/in
  espera '^A:\\>' 60
  sleep 0.5
}
orden() { date +%s%N > $T/t0; printf '%s\r' "$1" >&3; sleep "${2:-0.3}"; }
# ¿termina QEMU solo antes de $1 segundos? -> $T/seg (lo que ha tardado
# desde la última orden tecleada)
termina() {
  local t0 n=0
  t0=$(cat $T/t0)
  until [ -f $T/rc ]; do
    sleep 0.1; n=$((n + 1))
    [ $n -ge $(( $1 * 10 )) ] && { echo 999 > $T/seg; return 1; }
  done
  echo $(( ($(date +%s%N) - t0) / 1000000 )) > $T/seg
  return 0
}
cierra() {
  exec 3>&-
  if [ ! -f $T/rc ]; then mon quit; fi
  wait $QP 2>/dev/null
  tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 | grep -v '^audio:' > $T/log.txt
}
segundos() { awk -v m=$(cat $T/seg) 'BEGIN { printf "%.1f", m / 1000 }'; }
# la imagen con HSPXMS (para Hispa Ventanas) y un .BAT que escribe y apaga
imagen() {
  cp "$IMG" $T/f.img
  printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS
  mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
  printf '@ECHO OFF\r\nCOPY A:\\HISPADOS\\EQUIPO.COM A:\\COPIA.COM\r\nECHO escrito justo antes de apagar> A:\\ANTES.TXT\r\nA:\\HISPADOS\\APAGAR\r\n' > $T/GUARDA.BAT
  mcopy -o -i $T/f.img $T/GUARDA.BAT ::GUARDA.BAT
}
# lo escrito antes de apagar, entero en la imagen
escrito() {
  mcopy -o -i $T/f.img ::COPIA.COM $T/copia.com 2>/dev/null
  if cmp -s $T/copia.com $B/EQUIPO.COM; then ok "$1: COPIA.COM ($(stat -c %s $B/EQUIPO.COM) octetos) entera en la imagen"
  else ko "$1: COPIA.COM no está entera en la imagen"; fi
  if [ "$(mtype -i $T/f.img ::ANTES.TXT 2>/dev/null | tr -d '\r')" = "escrito justo antes de apagar" ]; then
    ok "$1: ANTES.TXT en la imagen"; else ko "$1: ANTES.TXT no está en la imagen"; fi
  if fsck.fat -n $T/f.img > $T/fsck.txt 2>&1; then ok "$1: el disquete sin errores (fsck.fat)"
  else ko "$1: fsck.fat encuentra errores"; cat $T/fsck.txt; fi
}

# ---------------------------------------------------------------------------
echo "== APAGAR y REINICIAR: ayuda y /INFO (-M pc) =="
imagen
arranca "-M pc -no-reboot"
orden 'APAGAR /?' 1; orden 'REINICIAR /?' 1; orden 'AYUDA APAGAR' 1; orden 'AYUDA ORDENADOR' 1
orden 'APAGAR /X' 1
orden 'APAGAR /INFO' 3; orden 'REINICIAR /INFO' 3
orden 'ECHO FIN-INFO'; espera '^FIN-INFO' 20
cierra
check "Apaga el equipo."
check "APAGAR [/INFO] [/NOUEFI] [/NOACPI] [/NOAPM] [/SOLO:método]"
check "Reinicia el equipo."
check "REINICIAR [/INFO] [/NOACPI | /SOLO:método]"
check "APAGAR y REINICIAR (apagar o reiniciar el equipo)" "AYUDA: APAGAR y REINICIAR en la lista"
check "Opción no válida: /X"
check "ACPI: RSDP en "
check "FADT en " "FADT encontrada"
check "SMI_CMD 00B2h, ACPI_ENABLE F1h, ACPI_DISABLE F0h"
check "PM1a_CNT 0604h"
check "\_S5: en la DSDT"
check "SLP_TYPa = 00h, SLP_TYPb = 00h"
check "RESET_REG: no hay" "pc: sin RESET_REG"
check "APM: versión 1.2"
check "Para apagar: ACPI (S5); si no apaga, APM; si no apaga, el mensaje"
check "  el controlador del teclado (64h <- FEh)"
checkn "Parámetro no válido"

echo "== APAGAR en -M pc: QEMU se apaga solo; lo escrito antes, en el disco =="
imagen
arranca "-M pc"
orden 'GUARDA'
if termina 20; then ok "pc: QEMU ha terminado solo $(segundos) s después de GUARDA (con la espera de 2 s)"
else ko "pc: QEMU no se ha apagado"; fi
cierra
check "Escribiendo en el disco lo que queda pendiente..."
check "Apagando el equipo..."
checkxn "Ya puede apagar el equipo."
escrito pc

echo "== APAGAR en -M q35 =="
imagen
arranca "-M q35"
orden 'APAGAR /INFO' 3
orden 'GUARDA'
if termina 20; then ok "q35: QEMU ha terminado solo $(segundos) s después de GUARDA"
else ko "q35: QEMU no se ha apagado"; fi
cierra
check "ACPI_ENABLE 02h, ACPI_DISABLE 03h" "q35: SMI_CMD y ACPI_ENABLE del ICH9"
check "RESET_REG: E/S 00000CF9h <- 0Fh" "q35: RESET_REG CF9h"
check "\_S5: en la DSDT"
checkxn "Ya puede apagar el equipo."
escrito q35

echo "== APAGAR /NOACPI en -M pc: por APM =="
imagen
arranca "-M pc"
orden 'APAGAR /NOACPI'
if termina 20; then ok "pc: APM (SeaBIOS) apaga QEMU en $(segundos) s"
else ko "pc: APM no ha apagado"; fi
cierra

echo "== Sin ACPI (-machine acpi=off) =="
imagen
arranca "-M pc -machine acpi=off"
orden 'APAGAR /INFO' 3
orden 'APAGAR' 7
shot fin
if [ -f $T/rc ]; then ko "pc sin ACPI: QEMU ha terminado (no debía: SeaBIOS no apaga sin tablas)"
else ok "pc sin ACPI: QEMU sigue en marcha con la pantalla final"; fi
cierra
check "ACPI: no hay (no se encuentra la RSDP)"
check "Para apagar: APM; si no apaga, el mensaje"
check "Ya puede apagar el equipo." "pc sin ACPI: «Ya puede apagar el equipo.» (también por la consola serie)"
if [ -f $T/fin.ppm ]; then
  python3 - $T/fin.ppm <<'EOF' && ok "pantalla final: 720x400, letras naranjas grandes centradas sobre negro" || ko "pantalla final"
import re, sys
d = open(sys.argv[1], 'rb').read()
m = re.match(rb'P6\s+(\d+)\s+(\d+)\s+(\d+)\s', d)
w, h = int(m.group(1)), int(m.group(2)); p = d[m.end():]
def px(x, y):
    i = (y * w + x) * 3
    return tuple(p[i:i + 3])
assert (w, h) == (720, 400), (w, h)
nar = [(x, y) for y in range(h) for x in range(w) if px(x, y)[0] > 200 and 100 < px(x, y)[1] < 180 and px(x, y)[2] < 40]
assert len(nar) > 20000, len(nar)               # letras grandes
xs = [x for x, y in nar]; ys = [y for x, y in nar]
assert abs((min(xs) + max(xs)) / 2 - 360) < 30, (min(xs), max(xs))   # centrado
assert abs((min(ys) + max(ys)) / 2 - 200) < 30, (min(ys), max(ys))
assert max(xs) - min(xs) > 400 and max(ys) - min(ys) > 200
assert px(5, 5) == (0, 0, 0) and px(714, 394) == (0, 0, 0)
EOF
fi
muestra fin final
imagen
arranca "-M q35 -machine acpi=off"
orden 'APAGAR /INFO' 3
orden 'APAGAR'
if termina 20; then ok "q35 sin las tablas de QEMU: se apaga en $(segundos) s"
else ko "q35 sin las tablas de QEMU: no se ha apagado"; fi
cierra
# (según la versión de SeaBIOS: pone sus propias tablas, con \_S5 en una
# SSDT, y se apaga por ACPI; o no hay ninguna y se apaga por APM)
if grep -qF "ACPI: no hay" $T/log.txt; then
  check "Para apagar: APM;" "q35 sin tablas: por APM"
else
  check "\_S5: en la SSDT número 1" "q35 con las tablas de SeaBIOS: \\_S5 en una SSDT"
fi

echo "== REINICIAR =="
for prueba in "pc||controlador del teclado" "q35||registro de reinicio de ACPI" \
              "pc|/SOLO:TECLADO|controlador del teclado" "pc|/SOLO:CF9|puerto CF9h" \
              "pc|/SOLO:FALLO|triple fallo" "q35|/SOLO:CF9|puerto CF9h" "q35|/NOACPI|controlador del teclado"; do
  IFS='|' read -r maq opc metodo <<< "$prueba"
  imagen
  arranca "-M $maq -no-reboot"
  orden "REINICIAR $opc"
  if termina 15; then ok "$maq: REINICIAR $opc reinicia ($(segundos) s)"; else ko "$maq: REINICIAR $opc no reinicia"; fi
  cierra
  if [ "$(grep '^  ' $T/log.txt | tail -1)" = "  $metodo" ]; then ok "$maq: REINICIAR $opc: con «$metodo»"
  else ko "$maq: REINICIAR $opc: el último método no es «$metodo» ($(grep '^  ' $T/log.txt | tail -1))"; fi
done
imagen
arranca "-M pc -no-reboot"
orden "REINICIAR /SOLO:ACPI" 4
orden 'ECHO SIGUE-VIVO'; espera '^SIGUE-VIVO' 10
cierra
check "No se ha podido reiniciar el equipo." "pc: REINICIAR /SOLO:ACPI sin RESET_REG lo dice y vuelve"
# sin -no-reboot: vuelve a arrancar
imagen
arranca "-M q35"
orden 'REINICIAR'
sleep 4
n=0; until [ "$(grep -c 'Iniciando Hispa-DOS' $T/log)" -ge 2 ] || [ $n -ge 60 ]; do sleep 0.5; n=$((n + 1)); done
cierra
n=$(grep -c 'Iniciando Hispa-DOS' $T/log.txt)
if [ "$n" -ge 2 ]; then ok "q35: tras REINICIAR, Hispa-DOS arranca otra vez"; else ko "q35: no ha vuelto a arrancar ($n)"; fi

# ---------------------------------------------------------------------------
# Hispa Ventanas: el cuadro «Salir»
RX=400; RY=300
ir() {
  local dx=$(( $1 - RX )) dy=$(( $2 - RY )) sx sy
  while [ $dx -ne 0 ] || [ $dy -ne 0 ]; do
    sx=$dx; [ $sx -gt 60 ] && sx=60; [ $sx -lt -60 ] && sx=-60
    sy=$dy; [ $sy -gt 60 ] && sy=60; [ $sy -lt -60 ] && sy=-60
    mon "mouse_move $sx $sy" 0.07
    dx=$(( dx - sx )); dy=$(( dy - sy ))
  done
  RX=$1; RY=$2; sleep 0.3
}
clic() { ir $1 $2; mon "mouse_button 1" 0.25; mon "mouse_button 0" 0.5; }
cuadro() {
  chk $T/$1.ppm marco 212 171 376 257 activa titulo 212 171 activa "Salir de Hispa Ventanas" \
    texto 238 210 0 8 negrita "¿Qué quiere hacer?" \
    texto 350 249 0 6 fina "Volver a Hispa-DOS" texto 346 293 0 6 fina "Reiniciar el equipo" \
    texto 358 337 0 6 fina "Apagar el equipo" texto 480 389 0 6 fina "Cancelar" \
    px 243 265 0 "dibujo de Hispa-DOS (pantalla negra)" px 243 300 2 "dibujo de reiniciar (flecha en redondo)" \
    px 253 332 13 "dibujo de apagar (granate)" px 300 370 7 "filete sobre Cancelar" px 285 246 4 "el foco en «Volver a Hispa-DOS»"
}
entra() {
  orden 'KEYB SP' 2
  orden 'VENTANAS' 7
  RX=400; RY=300
}

echo "== Hispa Ventanas: el cuadro «Salir», Esc, Alt+F4 y volver a Hispa-DOS =="
imagen
arranca "-M pc -m 16 -vga std -no-reboot"
entra
clic 756 13; sleep 1.5
shot v1
tecla esc; sleep 1
shot v2
mon "sendkey alt-f4" 1                      # cierra el Centro: el mismo cuadro
shot v3
tecla esc; sleep 1
clic 435 65; sleep 1                        # minimiza el Centro: no queda ventana activa
mon "sendkey alt-f4" 1                      # Alt+F4 en el escritorio
shot v4
tecla down; sleep 0.5
shot v5                                     # flecha abajo: «Reiniciar el equipo»
tecla up ret; sleep 3                       # Intro en «Volver a Hispa-DOS»
orden 'ECHO TRAS-SALIR' 1
orden 'MEM' 2
orden 'ECHO FIN-V'; espera '^FIN-V' 20
cierra
cuadro v1
chk $T/v2.ppm igual 400 332 32 32 $T/v2.ppm 560 460 0 marco 36 52 452 190 activa titulo 36 52 activa "Centro de programas"
cuadro v3
cuadro v4
chk $T/v4.ppm igual 40 64 32 32 $T/v4.ppm 520 64 0
chk $T/v5.ppm px 285 290 4 "el foco en «Reiniciar el equipo» (marco terracota)" px 285 246 6 "«Volver» ya sin el foco"
check "TRAS-SALIR" "Volver a Hispa-DOS: el indicador otra vez"
checkn "Apagando el equipo"
muestra v1 cuadro; muestra v4 altf4; muestra v5 flechas

for maq in pc q35; do
  echo "== Hispa Ventanas: «Apagar el equipo» (-M $maq) =="
  imagen
  arranca "-M $maq -m 32 -vga std"
  entra
  clic 756 13; sleep 1.5
  shot w1
  tecla down down; sleep 0.3
  shot w2
  date +%s%N > $T/t0; tecla ret
  if termina 30; then ok "$maq: Hispa Ventanas > Apagar el equipo: QEMU termina solo ($(segundos) s)"
  else ko "$maq: Hispa Ventanas > Apagar el equipo: no se ha apagado"; fi
  cierra
  cuadro w1
  check "Escribiendo en el disco lo que queda pendiente..." "$maq: APAGAR desde el lanzador"
  check "Apagando el equipo..."
  mtype -i $T/f.img ::HISPADOS/CENTRO.INI 2>/dev/null | tr -d '\r' | iconv -f CP437 -t UTF-8 > $T/ini.txt
  if grep -qx '\[Centro\]' $T/ini.txt && grep -q '^Ventana=' $T/ini.txt; then ok "$maq: CENTRO.INI guardado antes de apagar"
  else ko "$maq: CENTRO.INI no se ha guardado"; fi
  [ $maq = pc ] && muestra w2 apagar
done

echo "== Hispa Ventanas: «Reiniciar el equipo» (-M q35, con -no-reboot) =="
imagen
arranca "-M q35 -m 32 -vga std -no-reboot"
entra
clic 756 13; sleep 1.5
ir 420 301; date +%s%N > $T/t0; mon "mouse_button 1" 0.25; mon "mouse_button 0" 0.1
if termina 30; then ok "q35: Hispa Ventanas > Reiniciar el equipo: reinicia ($(segundos) s)"
else ko "q35: Hispa Ventanas > Reiniciar el equipo: no reinicia"; fi
cierra
check "Reiniciando el equipo..."
check "  registro de reinicio de ACPI" "q35: reinicia con el RESET_REG"
if mtype -i $T/f.img ::HISPADOS/CENTRO.INI 2>/dev/null | grep -q 'Centro'; then ok "q35: CENTRO.INI guardado antes de reiniciar"
else ko "q35: CENTRO.INI no se ha guardado"; fi

# ---------------------------------------------------------------------------
# [APAGAR-USB] Arrancando de una memoria USB (la BIOS la da por INT 13h: así
# arrancan los portátiles de Carlos): lo que se cambia en Configuración y lo
# que guarda Hispa Ventanas tiene que estar en la memoria después de «Apagar
# el equipo», con la FAT sana (fsck.fat). Con SeaBIOS (xHCI y EHCI) y con
# OVMF + HispaBIOS (UEFI de 64 y de 32 bits: apaga con ResetSystem).
# Y si CENTRO.INI no se puede escribir, se dice antes de apagar.
U486=${USB486:-$B/hispa486-usb.img}
U386=${USB386:-$B/hispados-usb.img}
UOFF=1048576                                # la partición empieza en el sector 2048
# uarranca "opciones de QEMU" [qemu]: la memoria USB es $T/u.img
uarranca() {
  : > $T/log; rm -f $T/in $T/mon $T/rc; mkfifo $T/in
  ( timeout 150 ${2:-qemu-system-i386} -display none -serial stdio -monitor unix:$T/mon,server,nowait \
      -parallel none -m 64 $1 < $T/in > $T/log 2>&1
    echo $? > $T/rc ) &
  QP=$!
  exec 3> $T/in
}
UXHCI="-device qemu-xhci,id=hc -drive if=none,id=u,format=raw,file=$T/u.img -device usb-storage,bus=hc.0,drive=u,bootindex=0"
UEHCI="-device usb-ehci,id=hc -drive if=none,id=u,format=raw,file=$T/u.img -device usb-storage,bus=hc.0,drive=u,bootindex=0"
uovmf() {   # 64 o 32: el firmware UEFI (OVMF) en $T
  local c=/usr/share/OVMF/OVMF_CODE_4M.fd v=/usr/share/OVMF/OVMF_VARS_4M.fd
  [ $1 = 32 ] && { c=/usr/share/OVMF/OVMF32_CODE_4M.fd; v=/usr/share/OVMF/OVMF32_VARS_4M.fd; }
  [ -f $c ] || return 1
  cp $v $T/vars.fd
  echo "-smp 2 -drive if=pflash,format=raw,readonly=on,file=$c -drive if=pflash,format=raw,file=$T/vars.fd -drive file=$T/u.img,format=raw,if=none,id=u -device qemu-xhci -device usb-storage,drive=u,bootindex=1"
}
uini() { mtype -i $T/u.img@@$UOFF "::HISPADOS/$1" 2>/dev/null | tr -d '\r' | iconv -f CP437 -t UTF-8 -c > $T/ini.txt; }
utiene() { if grep -qxF -- "$2" $T/ini.txt; then ok "$1: $3"; else ko "$1: $3 (falta «$2»)"; fi; }
ufsck() {
  dd if=$T/u.img of=$T/part.img bs=512 skip=$((UOFF / 512)) status=none
  if fsck.fat -n $T/part.img > $T/fsck.txt 2>&1; then ok "$1: la memoria USB sin errores (fsck.fat)"
  else ko "$1: fsck.fat encuentra errores en la memoria USB"; cat $T/fsck.txt; fi
  rm -f $T/part.img
}
# el escritorio de Hispa-486 (arranca solo): espera a que esté
uescritorio() { espera 'Instalado: clientes' 90 || ko "$1: no llega a Hispa Ventanas"; sleep ${2:-8}; RX=400; RY=300; }
# «Salir de Hispa Ventanas...» del menú Inicio: el cuadro, con «Apagar» primero
usalir() { tecla ctrl-esc; sleep 1; tecla up; sleep 0.4; tecla ret; sleep 2; }
uconsola() { tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 -c | grep -v '^audio:' > $T/log.txt; }

if [ -f "$U486" ]; then
  for hc in xhci ehci; do
    echo "== Desde una memoria USB ($hc, SeaBIOS): Configuración y «Apagar el equipo» en Hispa-486 =="
    cp "$U486" $T/u.img; tests/tools/sinmenu.sh $T/u.img > /dev/null
    [ $hc = xhci ] && O=$UXHCI || O=$UEHCI
    uarranca "$O"
    uescritorio $hc 12
    tecla ctrl-esc; sleep 1; tecla right; sleep 1; tecla down ret; sleep 6   # Inicio > Principal > Configuración
    clic 544 258; clic 575 317               # Pantalla: dibujo «damero», fondo granate
    ir 16 320; sleep 4                       # (se guarda solo a los 3 s)
    # el color del dibujo, granate, y enseguida Salir > Apagar (Configuración
    # no ha llegado a guardarlo sola: se lo pide Hispa Ventanas al salir,
    # con WM_ENDSESSION)
    ir 575 367; mon "mouse_button 1" 0.1; date +%s%N > $T/tc; mon "mouse_button 0" 0.1
    mon "sendkey ctrl-esc" 0.25; mon "sendkey up" 0.12; mon "sendkey ret" 0.35
    date +%s%N > $T/t0; tecla ret            # Intro: «Apagar el equipo»
    rapido=$(( ($(cat $T/t0) - $(cat $T/tc)) / 1000000 ))
    if termina 40; then ok "$hc: QEMU se apaga solo ($(segundos) s)"; else ko "$hc: no se ha apagado"; fi
    [ "$rapido" -lt 1000 ] && ok "$hc: Apagar menos de 1 s después del cambio ($rapido ms)" || ko "$hc: Apagar $rapido ms después del cambio (debía ser menos de 1 s)"
    if [ "$(cat $T/seg)" -ge 2000 ]; then ok "$hc: con la espera para que la memoria acabe (2 s o más)"
    else ko "$hc: se ha apagado sin esperar a la memoria ($(segundos) s)"; fi
    cierra; uconsola
    check "Escribiendo en el disco lo que queda pendiente..." "$hc: APAGAR vacía los búferes"
    uini VENTANAS.INI
    utiene $hc "Dibujo=damero" "VENTANAS.INI con el dibujo elegido"
    utiene $hc "ColorFondo=13" "VENTANAS.INI con el color elegido"
    utiene $hc "ColorDibujo=13" "VENTANAS.INI con el cambio hecho justo antes de apagar ($rapido ms antes: WM_ENDSESSION)"
    utiene $hc "Escritorio=486" "VENTANAS.INI conserva lo de Hispa-486"
    uini CENTRO.INI
    if grep -q '^; Hispa Ventanas - Centro de programas' $T/ini.txt && grep -qx '\[Centro\]' $T/ini.txt; then
      ok "$hc: CENTRO.INI escrito por Hispa Ventanas al salir"; else ko "$hc: CENTRO.INI no se ha escrito"; fi
    ufsck $hc
  done

  echo "== Desde una memoria USB: CENTRO.INI que no se puede escribir: se avisa antes de apagar =="
  cp "$U486" $T/u.img; tests/tools/sinmenu.sh $T/u.img > /dev/null
  mattrib -i $T/u.img@@$UOFF +r ::HISPADOS/CENTRO.INI
  uarranca "$UXHCI"
  uescritorio "solo lectura"
  usalir; tecla ret; sleep 2                 # «Apagar el equipo»: no se puede guardar
  shot u2
  tecla esc; sleep 5                         # Cancelar: se queda en Hispa Ventanas
  shot u3
  if [ -f $T/rc ]; then ko "solo lectura: con «Cancelar» se ha apagado igual"; else ok "solo lectura: con «Cancelar» sigue en Hispa Ventanas"; fi
  usalir; tecla ret; sleep 2
  date +%s%N > $T/t0; tecla ret              # «Aceptar»: apagar igualmente
  if termina 40; then ok "solo lectura: «Aceptar» apaga ($(segundos) s)"; else ko "solo lectura: «Aceptar» no apaga"; fi
  cierra; uconsola
  chk $T/u2.ppm busca 100 100 700 500 0 8 "No se ha podido guardar el Centro de programas" \
    busca 100 100 700 500 0 8 "¿Apagar igualmente?"
  chk $T/u3.ppm nobusca 100 100 700 500 0 8 "No se ha podido guardar el Centro de programas" \
    texto 26 578 8 4 negrita "Inicio"
  if [ "$(grep -c 'Escribiendo en el disco lo que queda pendiente' $T/log.txt)" = 1 ]; then
    ok "solo lectura: APAGAR una sola vez (la segunda)"; else ko "solo lectura: APAGAR no ha ido una vez"; fi
  ufsck "solo lectura"
  muestra u2 noguarda
else
  echo "  (no hay $U486: «make usb486»; se salta la memoria USB de Hispa-486)"
fi

for arq in 64 32; do
  [ -f "$U386" ] || break
  O=$(uovmf $arq) || { echo "  (sin OVMF de $arq bits: se salta)"; continue; }
  Q=qemu-system-x86_64; [ $arq = 32 ] && Q=qemu-system-i386
  echo "== Desde una memoria USB (UEFI de $arq bits, HispaBIOS): APAGAR desde DOS =="
  cp "$U386" $T/u.img; tests/tools/sinmenu.sh $T/u.img > /dev/null
  printf '@ECHO OFF\r\nCOPY C:\\HISPADOS\\EQUIPO.COM C:\\COPIA.COM\r\nECHO escrito justo antes de apagar> C:\\ANTES.TXT\r\nAPAGAR\r\n' > $T/GUARDA.BAT
  mcopy -o -i $T/u.img@@$UOFF $T/GUARDA.BAT ::GUARDA.BAT
  uarranca "$O" $Q
  espera 'C:\\>' 120 || ko "uefi$arq: no llega a C:\\>"
  sleep 1
  orden 'APAGAR /INFO' 3
  orden 'GUARDA'
  if termina 30; then ok "uefi$arq: QEMU se apaga solo ($(segundos) s)"; else ko "uefi$arq: no se ha apagado"; fi
  cierra; uconsola
  check "HispaBIOS (equipo UEFI): sí" "uefi$arq: APAGAR ve HispaBIOS"
  check "Para apagar: el firmware UEFI (ResetSystem); si no apaga, ACPI (S5)" "uefi$arq: apaga primero con el firmware"
  mcopy -o -i $T/u.img@@$UOFF ::COPIA.COM $T/copia.com 2>/dev/null
  if cmp -s $T/copia.com $B/EQUIPO.COM; then ok "uefi$arq: COPIA.COM entera en la memoria"; else ko "uefi$arq: COPIA.COM no está entera"; fi
  if [ "$(mtype -i $T/u.img@@$UOFF ::ANTES.TXT 2>/dev/null | tr -d '\r')" = "escrito justo antes de apagar" ]; then
    ok "uefi$arq: ANTES.TXT en la memoria"; else ko "uefi$arq: ANTES.TXT no está en la memoria"; fi
  ufsck uefi$arq
done

if [ -f "$U486" ] && O=$(uovmf 64); then
  echo "== Desde una memoria USB (UEFI de 64 bits, HispaBIOS): Configuración y «Apagar el equipo» en Hispa-486 =="
  cp "$U486" $T/u.img; tests/tools/sinmenu.sh $T/u.img > /dev/null
  uarranca "$O" qemu-system-x86_64
  uescritorio uefi 10
  # (con el ratón: las teclas con Ctrl o Alt no llegan por el teclado del
  # firmware de QEMU) Inicio > Configuración; Inicio > Salir > Apagar
  clic 26 586; sleep 1; clic 120 456; sleep 4
  clic 544 258; clic 575 317; ir 16 320; sleep 4
  clic 26 586; sleep 1; clic 120 552; sleep 2
  date +%s%N > $T/t0; clic 420 243
  if termina 40; then ok "uefi: Hispa Ventanas > Apagar el equipo: QEMU se apaga solo ($(segundos) s)"
  else ko "uefi: Hispa Ventanas > Apagar el equipo: no se ha apagado"; fi
  cierra; uconsola
  uini VENTANAS.INI
  utiene uefi "Dibujo=damero" "VENTANAS.INI con el dibujo elegido"
  utiene uefi "ColorFondo=13" "VENTANAS.INI con el color elegido"
  uini CENTRO.INI
  if grep -q '^; Hispa Ventanas - Centro de programas' $T/ini.txt; then ok "uefi: CENTRO.INI escrito por Hispa Ventanas"
  else ko "uefi: CENTRO.INI no se ha escrito"; fi
  ufsck uefi
fi

if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then echo "--- registro de la última sesión ---"; cat $T/log.txt; fi
rm -rf $T
exit $fail
