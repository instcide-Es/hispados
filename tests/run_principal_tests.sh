#!/bin/bash
# Prueba del grupo «Principal» de Hispa Ventanas desde la 0.9.22 y de la
# salida a Hispa-DOS:
#  1.ª sesión, con un CENTRO.INI de antes (Principal con Editar, Gestor e
#     Información, y otro grupo con un acceso a EDITAR.COM puesto a mano):
#     al arrancar se quitan solo los tres de Principal, el del otro grupo se
#     queda, y queda apuntado SinRepetidos=1; los programas siguen en el disco;
#     «Volver a Hispa-DOS» (cerrar el Centro e Intro) dice «Hispa Ventanas ha
#     terminado.» y después enseña la información del sistema (INFO).
#  2.ª sesión: si se vuelve a poner Editar en Principal, ya no se quita.
#  3.ª sesión, sin CENTRO.INI: lo de fábrica ya no trae esos tres.
# Hispa Ventanas «Principal» group (0.9.22) and exit-to-DOS test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$1" $T/log.txt; then ok "$1"; else ko "$1"; fi; }
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.3}"; }
tecla() { local k; for k in "$@"; do mon "sendkey $k" 0.3; done; }
shot() { sleep 0.4; mon "screendump $T/$1.ppm" 0.6; }
send() { printf '%s\r' "$1"; sleep "${2:-1.5}"; }
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
run() {
  rm -f $T/mon
  timeout 90 qemu-system-i386 -display none -serial stdio -monitor unix:$T/mon,server,nowait \
    -parallel none -m 16 -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a -no-reboot > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 > $T/log.txt
}
centro() { MTOOLS_SKIP_CHECK=1 mtype -i $T/f.img ::HISPADOS/CENTRO.INI 2>/dev/null | tr -d '\r' | iconv -f CP437 -t UTF-8 > $T/ini.txt; }
grupos() { grep -E '^(Nombre|\[Grupo\])' $T/ini.txt | tr '\n' ' '; }
# Ventanas: entrar, esperar, cerrar el Centro (pregunta) e Intro = «Volver a Hispa-DOS»
sesion() {
  { sleep 6; send 'VENTANAS' 9; clic 475 65; sleep 1; shot cuadro; tecla ret; sleep 4; shot info
    send 'ECHO TRAS-SALIR' 2; } | run
}

echo "== Hispa Ventanas: «Principal» sin repetidos y salida con INFO =="
cp "$IMG" $T/f.img
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS
MTOOLS_SKIP_CHECK=1 mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
printf '%s\r\n' '[Centro]' 'Ventana=36,52,452,190' 'Activo=0' \
  '[Grupo]' 'Nombre=Principal' 'Ventana=60,80,300,200' 'Estado=1' \
  '[Acceso]' 'Nombre=Editar' 'Icono=3' 'Programa=A:\HISPADOS\EDITAR.COM' 'Volver=1' \
  '[Acceso]' 'Nombre=Gestor' 'Icono=2' 'Programa=A:\HISPADOS\GESTOR.COM' 'Volver=1' \
  '[Acceso]' 'Nombre=Información' 'Icono=4' 'Programa=a:\hispados\info.com' 'Volver=1' 'Pausa=1' \
  '[Acceso]' 'Nombre=Hispa-DOS' 'Icono=6' 'Programa=A:\COMMAND.COM' \
  '[Grupo]' 'Nombre=Otros' \
  '[Acceso]' 'Nombre=Mi editor' 'Icono=3' 'Programa=A:\HISPADOS\EDITAR.COM' \
  | iconv -f UTF-8 -t CP437 > $T/CENTRO.INI
MTOOLS_SKIP_CHECK=1 mcopy -o -i $T/f.img $T/CENTRO.INI ::HISPADOS/CENTRO.INI
sesion
centro
g=$(grupos)
case "$g" in
  *"Nombre=Principal Nombre=Hispa-DOS "*) ok "1: Principal, sin Editar, Gestor ni Información";;
  *) ko "1: Principal: $g";;
esac
case "$g" in
  *"Nombre=Otros Nombre=Mi editor"*) ok "1: el EDITAR.COM de otro grupo se queda";;
  *) ko "1: el grupo Otros: $g";;
esac
if grep -qx 'SinRepetidos=1' $T/ini.txt; then ok "1: CENTRO.INI: SinRepetidos=1"; else ko "1: CENTRO.INI sin SinRepetidos=1"; fi
for p in EDITAR GESTOR INFO; do
  if MTOOLS_SKIP_CHECK=1 mdir -i $T/f.img ::HISPADOS/$p.COM > /dev/null 2>&1; then ok "1: $p.COM sigue en el disco"; else ko "1: falta $p.COM"; fi
done
check "Hispa Ventanas ha terminado."
# INFO escribe en la pantalla: se mira la captura después de salir
if python3 -I tests/tools/ventanas_check.py $T/info.ppm tam 720 400 > /dev/null 2>&1; then ok "1: de vuelta en modo texto"; else ko "1: no ha vuelto al modo texto"; fi
if sed -n '/Hispa Ventanas ha terminado/,$p' $T/log.txt | grep -qF 'TRAS-SALIR'; then ok "1: después, el intérprete de órdenes sigue (ECHO TRAS-SALIR)"; else ko "1: sin el intérprete tras salir"; fi
n=$(sed -n '/Hispa Ventanas ha terminado/,/TRAS-SALIR/p' $T/log.txt | grep -cE 'Procesador|Memoria|Propiedades del sistema')
s=$(python3 -I - $T/info.ppm <<'PY'
import re, sys
d = open(sys.argv[1], 'rb').read()
m = re.match(rb'P6\s+(\d+)\s+(\d+)\s+(\d+)\s', d); w, h = int(m.group(1)), int(m.group(2)); d = d[m.end():]
# INFO pinta su cuadro con fondo azul: cuántos puntos azules hay en la pantalla
print(sum(1 for i in range(0, len(d), 3) if d[i+2] > 120 and d[i] < 60 and d[i+1] < 60))
PY
)
if [ "$n" -ge 1 ] || [ "${s:-0}" -gt 20000 ]; then ok "1: tras el adiós sale INFO (texto: $n, azul: $s)"; else ko "1: INFO no ha salido al volver (texto: $n, azul: $s)"; fi
cp $T/info.ppm $T/info1.ppm

# 2.ª: Editar puesto otra vez a mano en Principal: se queda
python3 -I - $T/ini.txt <<'PY' | iconv -f UTF-8 -t CP437 > $T/CENTRO.INI
import sys
s = open(sys.argv[1], encoding='utf-8').read()
a = 'Nombre=Principal\n'
i = s.index(a); j = s.index('[Acceso]', i)
s = s[:j] + '[Acceso]\nNombre=Editar\nIcono=3\nPrograma=A:\\HISPADOS\\EDITAR.COM\n' + s[j:]
sys.stdout.write(s.replace('\n', '\r\n'))
PY
MTOOLS_SKIP_CHECK=1 mcopy -o -i $T/f.img $T/CENTRO.INI ::HISPADOS/CENTRO.INI
sesion
centro
case "$(grupos)" in
  *"Nombre=Principal Nombre=Editar "*) ok "2: Editar puesto otra vez a mano se queda";;
  *) ko "2: Principal: $(grupos)";;
esac

# 3.ª: sin CENTRO.INI, lo de fábrica
MTOOLS_SKIP_CHECK=1 mdel -i $T/f.img ::HISPADOS/CENTRO.INI
sesion
centro
case "$(grupos)" in
  "[Grupo] Nombre=Principal Nombre=Hispa-DOS Nombre=Configuración Nombre=Administrador de archivos [Grupo] Nombre=Accesorios "*) ok "3: de fábrica, Principal = Hispa-DOS, Configuración y Administrador de archivos";;
  *) ko "3: de fábrica: $(grupos)";;
esac
if grep -qx 'SinRepetidos=1' $T/ini.txt; then ok "3: CENTRO.INI de fábrica con SinRepetidos=1"; else ko "3: sin SinRepetidos=1"; fi

[ -n "${GUARDAR:-}" ] && cp $T/info1.ppm $T/cuadro.ppm "$GUARDAR"/ 2>/dev/null
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/VENTANAS/,$p' $T/log.txt | tail -30; cat $T/ini.txt; fi
rm -rf $T
exit $fail
