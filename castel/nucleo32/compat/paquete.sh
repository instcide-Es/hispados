#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/compat/paquete.sh SALIDA
# (COMPAT) El paquete «compat», el Asistente de compatibilidad, para el
# actualizador de Hispa-Pentium (tools/hpq.py, OPCIONALES; tipo
# «herramientas»): hace COMPAT.BIN (hacer.sh), su LEEME, su lista de
# componentes de Microsoft (REDIST.LST: solo dónde bajarlos), adónde manda
# los informes (COMPAT.INI) y su acceso del menú Inicio con su icono, y deja
# en SALIDA COMPAT.HPQ y su línea del catálogo, CATALOGO.LIN.
set -e
AQUI=$(dirname "$(readlink -f "$0")")
RAIZ=$(readlink -f "$AQUI/../../..")
SAL=$(readlink -m "${1:?uso: paquete.sh SALIDA}")
W=$(mktemp -d)
trap 'rm -rf $W' EXIT
mkdir -p "$SAL" $W/c/BIN $W/c/COMPAT
bash $AQUI/hacer.sh $W/c/BIN/COMPAT.BIN > /dev/null
# (el LEEME, en la 437, para el Bloc y TYPE; la lista y COMPAT.INI, en UTF-8: los lee el Asistente)
iconv -f UTF-8 -t CP437//TRANSLIT $AQUI/LEEME.txt | sed 's/\r$//; s/$/\r/' > $W/c/COMPAT/LEEME.TXT
for f in REDIST.LST COMPAT.INI; do sed 's/\r$//; s/$/\r/' $AQUI/$f > $W/c/COMPAT/$f; done
python3 -I $RAIZ/tools/hpicono.py $AQUI/compat32.png $W/c/COMPAT.ICP
printf 'nombre=Asistente de compatibilidad\r\nprograma=%s\r\nargumentos=\r\nicono=%s\r\nescritorio=no\r\ntipo=Herramientas\r\n' \
  "${COMPAT_PROGRAMA:-\\HISPA\\COMPAT.BIN}" "${COMPAT_ICONO:-\\HISPA\\MENU\\COMPAT.ICP}" > $W/c/COMPAT.MNU
[ -n "${COMPAT_CARPETA:-}" ] && cp -r $W/c/. "$COMPAT_CARPETA"/
python3 -I $RAIZ/tools/hpq.py opcionales "$SAL" $W/c compat > /dev/null
python3 -I $RAIZ/tools/hpq.py linea "$SAL/COMPAT.HPQ" > "$SAL/CATALOGO.LIN"
echo "paquete.sh: $SAL/COMPAT.HPQ ($(stat -c %s "$SAL/COMPAT.HPQ") octetos) y CATALOGO.LIN"
