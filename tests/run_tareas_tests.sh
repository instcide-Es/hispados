#!/bin/bash
# Prueba del Administrador de tareas (TAREAS.HVP y ventanas/tareas_api.inc).
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
#
# Con un módulo que se cuelga a propósito (programas/tareas/cuelga.asm), en
# cuatro formas: un bucle sin fin dentro de un mensaje, dentro del
# temporizador, después de pedir memoria, abrir un archivo y capturar el
# ratón, y uno que se niega a cerrar. En cada caso: Ctrl+Alt+Supr abre el
# Administrador aunque el programa no devuelva el control, sale «No
# responde», «Terminar» lo quita, la pantalla queda limpia y la memoria libre
# vuelve a ser la de antes (se compara la zona de la memoria con la de un
# arranque sin el programa colgado). Y Ctrl+Alt+Supr con el Administrador
# delante pregunta si se reinicia.
#
# Necesita qemu-system-i386, mtools, socat e ImageMagick (como las demás).
set -u
IMG=${1:-build/hispados.img}
. tests/tools/hvp_arnes.sh
for m in TAREAS CUELGA; do
  f=programas/tareas.asm; [ $m = CUELGA ] && f=programas/tareas/cuelga.asm
  NASM=${NASM:-nasm} python3 tools/mkhvp.py $f $B/$m.HVP > /dev/null || { ko "$m.HVP no se ensambla"; exit 1; }
done
G=${GUARDA:-}
# las capturas que se quieran ver: GUARDA=carpeta
muestra_png() { [ -n "$G" ] && mkdir -p $G && [ -f $T/$1.ppm ] && convert $T/$1.ppm $G/$2.png; }
MEMZ="180 418 450 54"      # la zona de la memoria en la ventana del Administrador

echo "== Administrador de tareas: lo que hay en marcha =="
prepara $B/TAREAS.HVP
{ arranca
  sleep 2; shot a0
  ir 215 139; sleep 1; shot a1          # el ratón sobre «Pasar a»: su nombre en el pie
  tecla down; sleep 1; tecla delete; sleep 1.5; shot a2; tecla ret; sleep 1   # el Centro no se termina
  sal; } | run 120 -vga std
chk $T/a0.ppm titulo 170 80 activa "Administrador de tareas"
chk $T/a0.ppm busca 176 180 640 200 8 2 "Hispa Ventanas (el sistema)"
chk $T/a0.ppm busca 176 214 640 234 0 8 "Configuración"
chk $T/a0.ppm busca 176 232 640 252 0 8 "Administrador de tareas"
chk $T/a0.ppm busca 176 480 640 498 0 6 "4 programas en marcha"
chk $T/a0.ppm busca 176 418 640 436 0 8 "Libre:"
chk $T/a1.ppm busca 176 480 640 498 0 6 "Pasar al programa elegido (Intro)"
chk $T/a2.ppm busca 150 250 650 330 0 8 "Es parte del sistema: no se puede terminar."
cp $T/a0.ppm $T/base.ppm
muestra_png a0 tareas_lista

# caso: la tecla que lo cuelga; luego Ctrl+Alt+Supr, elegir Cuelga y terminar
colgar() {   # $1 = tecla de CUELGA, $2 = prefijo de las capturas
  prepara $B/CUELGA.HVP
  mcopy -o -i $T/f.img $B/TAREAS.HVP ::HISPADOS/PROGS/TAREAS.HVP
  { arranca
    sleep 1; tecla $1; sleep 2
    tecla ctrl-alt-delete; sleep 5; shot ${2}1           # el Administrador, aunque no responda
    tecla down down down; sleep 1; tecla delete; sleep 1.5; shot ${2}2; tecla ret; sleep 4
    aparca; shot ${2}3
    tecla ctrl-alt-delete; sleep 2; shot ${2}4; tecla esc; sleep 1   # delante: ¿reiniciar?
    sal; } | run 150 -vga std
  chk $T/${2}1.ppm titulo 170 80 activa "Administrador de tareas"
  chk $T/${2}1.ppm busca 176 232 640 252 13 8 "No responde"
  chk $T/${2}2.ppm busca 150 250 650 330 0 8 "¿Terminar «Cuelga»?"
  chk $T/${2}3.ppm busca 176 480 640 498 0 6 "4 programas en marcha"
  chk $T/${2}3.ppm nobusca 176 180 640 300 0 8 "Cuelga"
  chk $T/${2}3.ppm nobusca 0 330 170 360 0 6 "Cuelga"         # su ventana, quitada
  chk $T/${2}3.ppm igual $MEMZ $T/base.ppm ${MEMZ% * *} 0     # la memoria, como sin él
  chk $T/${2}4.ppm busca 90 260 650 340 0 8 "¿Reiniciar el equipo?"
}

echo "== Administrador de tareas: un programa colgado en un mensaje =="
colgar 1 m
muestra_png m1 tareas_no_responde; muestra_png m2 tareas_terminar; muestra_png m3 tareas_terminado; muestra_png m4 tareas_reiniciar

echo "== Administrador de tareas: colgado en el temporizador =="
colgar 2 t

echo "== Administrador de tareas: colgado con memoria, un archivo abierto y el ratón capturado =="
colgar 3 r

echo "== Administrador de tareas: un programa que no se deja cerrar =="
prepara $B/CUELGA.HVP
mcopy -o -i $T/f.img $B/TAREAS.HVP ::HISPADOS/PROGS/TAREAS.HVP
{ arranca
  sleep 1; tecla 4; sleep 1
  tecla ctrl-alt-delete; sleep 5
  tecla down down down; sleep 1; tecla delete; sleep 1.5; tecla ret   # por las buenas...
  sleep 6; shot n1                                                    # ...no cierra: ¿a la fuerza?
  tecla ret; sleep 4; aparca; shot n2
  sal; } | run 150 -vga std
chk $T/n1.ppm busca 90 250 650 340 0 8 "«Cuelga» no se cierra."
chk $T/n2.ppm busca 176 480 640 498 0 6 "4 programas en marcha"
chk $T/n2.ppm igual $MEMZ $T/base.ppm ${MEMZ% * *} 0
muestra_png n1 tareas_no_cierra

rm -rf $T
exit $fail
