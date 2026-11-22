#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/agenda/paquete.sh SALIDA
# (AGENDA) El paquete «agenda», la Agenda de Hispa-Pentium, para el
# actualizador (tools/hpq.py, OPCIONALES): hace AGENDA.BIN (uagenda.cas), su
# acceso del menú (tipo=Oficina) con su icono (icono.py) y su LEEME, y deja
# en SALIDA AGENDA.HPQ y su línea del catálogo, CATALOGO.LIN.
# Con AGENDA_CARPETA=carpeta, deja también ahí lo que lleva (las pruebas).
set -e
AQUI=$(dirname "$(readlink -f "$0")")
RAIZ=$(readlink -f "$AQUI/../../..")
SAL=$(readlink -f "${1:?uso: paquete.sh SALIDA}")
W=$(mktemp -d)
trap 'rm -rf $W' EXIT
mkdir -p "$SAL" $W/c/AGENDA $W/c/MENU
python3 -I $AQUI/../../ccastel.py $AQUI/../uagenda.cas -o $W/uagenda.asm > /dev/null
nasm -f bin $W/uagenda.asm -o $W/c/AGENDA.BIN
iconv -f UTF-8 -t CP437 $AQUI/LEEME.txt | sed 's/$/\r/' > $W/c/AGENDA/LEEME.TXT
python3 -I $AQUI/icono.py $W/c/MENU/AGENDA.ICP
printf 'nombre=Agenda\r\nprograma=%s\r\nargumentos=\r\nicono=%s\r\nescritorio=no\r\ntipo=Oficina\r\n' \
  "${AGENDA_PROGRAMA:-\\HISPA\\AGENDA.BIN}" "${AGENDA_ICONO:-\\HISPA\\MENU\\AGENDA.ICP}" > $W/c/MENU/AGENDA.MNU
[ -n "${AGENDA_CARPETA:-}" ] && cp -r $W/c/. "$AGENDA_CARPETA"/
python3 -I $RAIZ/tools/hpq.py opcionales "$SAL" $W/c agenda > /dev/null
python3 -I $RAIZ/tools/hpq.py linea "$SAL/AGENDA.HPQ" > "$SAL/CATALOGO.LIN"
echo "paquete.sh: $SAL/AGENDA.HPQ ($(stat -c %s "$SAL/AGENDA.HPQ") octetos) y CATALOGO.LIN"
