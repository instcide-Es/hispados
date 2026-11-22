#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# sesion_ssh.sh RESULTADO PRUEBAS - (TERMO) lo que hace el sshd de la prueba
# del botón «Terminal» (su ForceCommand: no hay shell): apunta en RESULTADO
# lo que ve (TERM, COLORTERM, los colores, el tamaño), pinta colores.sh y pone
# a pantalla completa htop, vim y less (cada fase, apuntada, para que
# conduce.py saque la pantalla); luego lee las teclas en crudo (en hex) y el
# tamaño que llega con SIGWINCH (window-change) cuando se encoge la ventana.
R=${1:?}; P=${2:?}
fase() { echo "FASE $1" >> "$R"; }
: > "$R"
echo "TERM=$TERM COLORTERM=${COLORTERM:-} LANG=${LANG:-} colores=$(tput colors 2> /dev/null)" >> "$R"
echo "tamano=$(stty size)" >> "$R"
fase colores; COLORES_RESP="$R" bash "$P/colores.sh"; sleep 4
fase htop; timeout --foreground 7 htop -d 10; sleep 1
fase vim; timeout --foreground 7 vim -n -u NONE -N -c 'syntax on' -c 'set number' -c 'set nomodified' "$P/../vt.c"; sleep 1
fase less; timeout --foreground 6 less "$P/../vt.h"; sleep 1
clear
fase teclas
echo "Pulse teclas (8 s)..."
stty raw -echo
timeout --foreground 8 dd bs=1 count=64 2> /dev/null | od -An -tx1 | tr -s ' \n' ' ' > "$R.teclas"
stty sane
echo "teclas=$(cat "$R.teclas")" >> "$R"
trap 'echo "winch=$(stty size)" >> "$R"' WINCH
fase encoge
echo "Encoja la ventana (12 s)..."
for _ in $(seq 24); do sleep 0.5; done
echo "tamano_final=$(stty size)" >> "$R"
fase fin
