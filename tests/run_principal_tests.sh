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
#  3.ª sesión, sin CENTRO.INI: lo de fábrica ya no trae esos tres, y trae el
#     Administrador de tareas en «Principal» (y no en «Accesorios»).
#  4.ª sesión, con un CENTRO.INI de la 0.9.22 (el Administrador de tareas en
#     «Accesorios»): pasa a «Principal» y queda apuntado TareasPrincipal=1.
#  5.ª sesión: si se vuelve a llevar a «Accesorios» a mano, allí se queda.
#  6.ª sesión, de fábrica: el grupo «Principal» enseña el icono del
#     Administrador de tareas, y el botón derecho en el fondo del escritorio
#     saca el menú con «Administrador de tareas...», que lo abre (y «Centro de
#     programas», que trae el Centro delante, y «Salir de Hispa Ventanas...»,
#     que saca el cuadro de salir).
#  (GUARDAR=carpeta deja las capturas.)
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
derecho() { ir $1 $2; mon "mouse_button 2" 0.25; mon "mouse_button 0" 0.5; }
chk() { python3 -I tests/tools/ventanas_check.py "$@" || fail=1; }
run() {
  rm -f $T/mon
  timeout 90 qemu-system-i386 ${QEMU_PANTALLA--vga std -global VGA.xres=800 -global VGA.yres=600} -display none -serial stdio -monitor unix:$T/mon,server,nowait \
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
  "[Grupo] Nombre=Principal Nombre=Hispa-DOS Nombre=Configuración Nombre=Administrador de tareas Nombre=Administrador de archivos [Grupo] Nombre=Accesorios "*) ok "3: de fábrica, Principal = Hispa-DOS, Configuración y los dos administradores (tareas y archivos)";;
  *) ko "3: de fábrica: $(grupos)";;
esac
if grep -qx 'SinRepetidos=1' $T/ini.txt; then ok "3: CENTRO.INI de fábrica con SinRepetidos=1"; else ko "3: sin SinRepetidos=1"; fi
if grep -qx 'TareasPrincipal=1' $T/ini.txt; then ok "3: CENTRO.INI de fábrica con TareasPrincipal=1"; else ko "3: sin TareasPrincipal=1"; fi
n=$(grep -c '^Nombre=Administrador de tareas$' $T/ini.txt)
if [ "$n" = 1 ]; then ok "3: el Administrador de tareas sale una sola vez (no se repite en Accesorios)"; else ko "3: el Administrador de tareas sale $n veces"; fi

# 4.ª: un CENTRO.INI de la 0.9.22: el Administrador de tareas estaba en
# Accesorios (se lleva allí el acceso y se quita la clave, como lo dejó la 0.9.22)
de_grupo() {   # $1 = grupo: los nombres de sus accesos, en una línea
  awk -v g="Nombre=$1" '/^\[Grupo\]/ { getline; en = ($0 == g); next } en && /^Nombre=/ { sub(/^Nombre=/, ""); printf "%s|", $0 }' $T/ini.txt
}
mover_tareas() {   # $1 = grupo de destino; $2 = 1: sin la clave TareasPrincipal
  python3 -I tests/tools/mover_acceso.py $T/ini.txt "Administrador de tareas" "$1" "$2" | iconv -f UTF-8 -t CP437 > $T/CENTRO.INI
  MTOOLS_SKIP_CHECK=1 mcopy -o -i $T/f.img $T/CENTRO.INI ::HISPADOS/CENTRO.INI
}
mover_tareas Accesorios 1
centro
case "$(de_grupo Accesorios)" in *"Administrador de tareas|"*) ;; *) ko "4: la prueba no ha dejado el acceso en Accesorios: $(de_grupo Accesorios)";; esac
if grep -q 'TareasPrincipal' $T/ini.txt; then ko "4: la prueba no ha quitado TareasPrincipal"; fi
sesion
centro
case "$(de_grupo Principal)" in
  *"Administrador de tareas|"*) ok "4: con un CENTRO.INI de la 0.9.22, el Administrador de tareas pasa a Principal";;
  *) ko "4: Principal: $(de_grupo Principal)";;
esac
case "$(de_grupo Accesorios)" in
  *"Administrador de tareas|"*) ko "4: sigue en Accesorios: $(de_grupo Accesorios)";;
  "Bloc de notas|Calculadora|Reloj|Sistema|"*) ok "4: ya no está en Accesorios, y lo demás sigue en su sitio";;
  *) ko "4: Accesorios: $(de_grupo Accesorios)";;
esac
if grep -qx 'TareasPrincipal=1' $T/ini.txt; then ok "4: CENTRO.INI: TareasPrincipal=1"; else ko "4: sin TareasPrincipal=1"; fi

# 5.ª: llevado otra vez a Accesorios a mano: se queda
mover_tareas Accesorios 0
sesion
centro
case "$(de_grupo Accesorios)" in
  *"Administrador de tareas|"*) ok "5: llevado otra vez a Accesorios a mano, se queda";;
  *) ko "5: Accesorios: $(de_grupo Accesorios)";;
esac

# 6.ª: de fábrica, lo que se ve: el icono en «Principal» y el menú del escritorio
MTOOLS_SKIP_CHECK=1 mdel -i $T/f.img ::HISPADOS/CENTRO.INI
RX=400; RY=300
{ sleep 6; send 'VENTANAS' 9
  tecla ret; sleep 2; ir 16 590; shot g1          # el grupo «Principal»
  derecho 620 430; sleep 1; shot g2               # el fondo del escritorio: su menú
  tecla t; sleep 4; ir 16 590; shot g3            # «Administrador de tareas...»
  tecla alt-f4; sleep 1.5
  derecho 620 430; sleep 1; tecla p; sleep 1.5    # «Centro de programas»: delante
  ir 16 590; shot g4
  derecho 620 430; sleep 1; tecla s; sleep 1.5    # «Salir de Hispa Ventanas...»: el cuadro
  ir 16 590; shot g5
  tecla esc; sleep 1; shot g6                     # Esc: no se sale
  mon quit; } | run
chk $T/g1.ppm titulo 200 140 activa "Principal" busca 400 230 520 275 0 8 "de tareas" busca 520 230 640 275 0 8 "de archivos"
chk $T/g2.ppm busca 350 200 560 430 0 8 "onfiguración..." busca 350 200 560 430 0 8 "Centro de " \
  busca 350 200 560 430 0 8 "Administrador de a" busca 350 200 560 430 0 8 "areas..." \
  busca 350 200 560 430 0 8 "Ventanas en c" busca 350 200 560 430 0 8 "alir de Hispa Ventanas..."
chk $T/g3.ppm titulo 170 80 activa "Administrador de tareas" titulo 36 52 no "Centro de programas" \
  busca 176 180 640 200 8 2 "Hispa Ventanas (el sistema)" busca 176 200 640 260 0 8 "Administrador de tareas"
chk $T/g4.ppm titulo 36 52 activa "Centro de programas" texto 66 155 9 12 fina "Principal" nobusca 150 60 700 300 0 8 "Administrador de tareas"
chk $T/g5.ppm titulo 212 171 activa "Salir de Hispa Ventanas"
chk $T/g6.ppm titulo 36 52 activa "Centro de programas"

[ -n "${GUARDAR:-}" ] && cp $T/info1.ppm $T/cuadro.ppm $T/g[1-6].ppm "$GUARDAR"/ 2>/dev/null
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/VENTANAS/,$p' $T/log.txt | tail -30; cat $T/ini.txt; fi
rm -rf $T
exit $fail
