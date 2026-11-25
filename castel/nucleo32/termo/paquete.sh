#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/termo/paquete.sh SALIDA
# (TERMO) El paquete «termo», el Termómetro 1.2 como programa normal de
# Hispa-Pentium (tools/hpq.py; de tipo «herramientas»): TERMO.BIN en \HISPA,
# su ayuda (TERMO.TXT) y su acceso en el menú Inicio, con su icono
# (TERMO.MNU y TERMO.ICP, de termo32.png). Deja en SALIDA TERMO.HPQ y su
# línea del catálogo, CATALOGO.LIN. Pide la base 0.8.12 (la carga fuerte; las llamadas 220 a
# 224 del núcleo, y desde la 1.1 la 220 con EBX = 5, 6 y 7, SMI_COUNT y
# Machine Check); con una de antes, TERMO.BIN lo dice y no hace nada más.
# (1.1.1, 25/11/2026: varias pruebas en el mismo arranque, cada una con su
# resultado en pantalla, la lista de todas y su sección en INFORME.TXT)
# (1.2, 25/11/2026, rama termo-fuerte: la carga fuerte, SSE2, que pide el SSE
# del núcleo de la base 0.8.12 —con una de antes, la ligera de siempre—; la
# alimentación con todas las lecturas; «Enviar a Carlos» que dice cómo ha ido)
set -e
AQUI=$(dirname "$(readlink -f "$0")")
RAIZ=$(readlink -f "$AQUI/../../..")
SAL=$(mkdir -p "${1:?uso: paquete.sh SALIDA}" && readlink -f "$1")
HPQ=${HPQ:-$RAIZ/tools/hpq.py}
W=$(mktemp -d)
trap 'rm -rf $W' EXIT
bash $AQUI/hacer.sh $W/TERMO.BIN
iconv -f UTF-8 -t CP437 $AQUI/AYUDA.txt | sed 's/$/\r/' > $W/TERMO.TXT
python3 -I $RAIZ/tools/hpicono.py $AQUI/termo32.png $W/TERMO.ICP
printf 'nombre=Termómetro\r\nprograma=\\HISPA\\TERMO.BIN\r\nargumentos=\r\nicono=\\HISPA\\MENU\\TERMO.ICP\r\nescritorio=no\r\ntipo=Herramientas\r\n' | iconv -f UTF-8 -t CP437 > $W/TERMO.MNU
python3 -I "$HPQ" crea "$SAL/TERMO.HPQ" --nombre termo --version 1.2 \
  --descripcion "Termómetro: la temperatura del procesador y de las zonas de la BIOS, la prueba de esfuerzo y si hay que limpiar el ventilador" \
  --necesita "base >= 0.8.12" --tipo herramientas \
  $W/TERMO.BIN='\HISPA\TERMO.BIN' $W/TERMO.TXT='\HISPA\TERMO.TXT' $W/TERMO.MNU='\HISPA\MENU\TERMO.MNU' $W/TERMO.ICP='\HISPA\MENU\TERMO.ICP' > /dev/null
python3 -I "$HPQ" linea "$SAL/TERMO.HPQ" > "$SAL/CATALOGO.LIN"
echo "paquete.sh: $SAL/TERMO.HPQ ($(stat -c %s "$SAL/TERMO.HPQ") octetos) y CATALOGO.LIN"
