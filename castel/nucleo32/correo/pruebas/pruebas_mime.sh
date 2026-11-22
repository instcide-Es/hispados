#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pruebas/pruebas_mime.sh [nuevos]: las pruebas del núcleo de Correo (mime.c,
# juegos.c, construye.c, html.c, vcard.c) en Linux, con ASan y UBSan: los
# casos de casos/ (NOMBRE.ORDEN.ext, con lo esperado en NOMBRE.esperado; la
# orden juego lleva el juego en el nombre: NOMBRE@JUEGO;
# «nuevos» escribe los que falten), la ida y vuelta de construye, la
# comparación con el módulo email de Python, lo grande en tiempo lineal y
# que compile para Hispa-Pentium sin avisos. Una línea por prueba
# («  OK   ...» o «  FAIL ...»); sale con 0 si todo va.
set -u
AQUI=$(dirname "$(readlink -f "$0")")
C=$(readlink -f "$AQUI/..")
RAIZ=$(readlink -f "$C/../../..")
O=${CORREO_PRUEBAS:-$RAIZ/build/correo-pruebas}
P=$O/prueba_mime
NUEVOS=${1:-}
mkdir -p "$O"
fallos=0
ok() { echo "  OK   $*"; }
mal() { echo "  FAIL $*"; fallos=$((fallos + 1)); }

W="-Wall -Wextra -Wno-unused-parameter -Wno-sign-compare"
if gcc -std=gnu11 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer $W -Werror \
	-I"$C" "$C"/juegos.c "$C"/mime.c "$C"/construye.c "$C"/html.c "$C"/vcard.c "$AQUI"/so_minimo.c "$AQUI"/prueba_mime.c \
	-o "$P" 2> "$O/avisos.txt"; then
	ok "se compila con ASan y UBSan, sin avisos"
else
	mal "no se compila con ASan y UBSan:"; cat "$O/avisos.txt"; exit 1
fi
export ASAN_OPTIONS=detect_leaks=1:abort_on_error=0
export UBSAN_OPTIONS=print_stacktrace=1

# ---- los casos ----
for f in "$AQUI"/casos/*.*.*; do
	b=$(basename "$f")
	case $b in *.esperado) continue;; esac
	nombre=${b%%.*}
	orden=$(echo "$b" | cut -d. -f2)
	esp="$AQUI/casos/$nombre.esperado"
	juego=""
	case $nombre in *@*) juego=${nombre#*@};; esac	# (juego: NOMBRE@JUEGO)
	salida=$(timeout 30 "$P" "$orden" "$f" $juego 2> "$O/err.txt")
	r=$?
	if [ $r -ne 0 ] || grep -q -e Sanitizer -e 'runtime error' "$O/err.txt"; then
		mal "$b (sale con $r)"; head -20 "$O/err.txt"
		continue
	fi
	if [ ! -f "$esp" ]; then
		if [ "$NUEVOS" = nuevos ]; then
			printf '%s\n' "$salida" > "$esp"
			ok "$b (esperado nuevo)"
		else
			mal "$b (no hay $nombre.esperado)"
		fi
		continue
	fi
	if [ "$salida" = "$(cat "$esp")" ]; then
		ok "$b"
	else
		mal "$b (no es lo esperado)"
		diff <(printf '%s\n' "$salida") "$esp" | head -20
	fi
done

# ---- hacer un mensaje y volverlo a leer ----
salida=$("$P" construye "$AQUI/casos/adjunto.bin" 2> "$O/err.txt")
r=$?
if [ $r -eq 0 ] && ! grep -q -e Sanitizer -e 'runtime error' "$O/err.txt"; then
	ok "construye: $(printf '%s\n' "$salida" | grep -c '^bien:') comprobaciones"
else
	mal "construye (sale con $r)"; printf '%s\n' "$salida" | grep '^MAL:'; head -20 "$O/err.txt"
fi
printf '%s\n' "$salida" | sed '/^--- fin del mensaje ---$/,$d' > "$O/construido.eml"
for f in "$AQUI"/casos/*.eml "$AQUI"/casos/*.html "$AQUI"/casos/adjunto.bin; do
	if "$P" b64 "$f" > /dev/null 2>&1; then :; else mal "base64 ida y vuelta de $(basename "$f")"; fi
done
ok "base64 ida y vuelta"

# ---- con el email de Python ----
if python3 -c 'import email.parser' 2> /dev/null; then
	python3 -I "$AQUI/compara_email.py" "$P" "$AQUI"/casos/*.eml "$O/construido.eml" ${CORREO_CORPUS:+"$CORREO_CORPUS"/*.eml} | while IFS= read -r l; do echo "$l"; done > "$O/py.txt"
	cat "$O/py.txt"
	n=$(grep -c '^  FAIL' "$O/py.txt")
	fallos=$((fallos + n))
else
	ok "(sin el módulo email de Python: no se compara)"
fi

# ---- lo grande, en tiempo lineal ----
python3 -I - "$O" <<'PY'
import sys, os
o = sys.argv[1]
ad = os.urandom(1400000)
import base64
b = base64.encodebytes(ad).decode()
m = ('From: a@b.es\r\nSubject: grande\r\nContent-Type: multipart/mixed; boundary="f"\r\n\r\n--f\r\n'
     'Content-Type: text/plain; charset=utf-8\r\nContent-Transfer-Encoding: quoted-printable\r\n\r\n' +
     ('Una l=C3=ADnea de texto con =\r\nsalto blando y --algo que no es frontera\r\n' * 4000) +
     '--f\r\nContent-Type: application/octet-stream; name=a.bin\r\nContent-Transfer-Encoding: base64\r\n\r\n' + b + '--f--\r\n')
open(o + '/grande.eml', 'w').write(m)
h = ('<table><tr><td><p>Hola <b>mundo</b> &amp; &euro; <a href="https://x.es/a">www.x.es</a> <img src="https://x.es/i.png" width=1 height=1></p></td></tr></table>' * 6000)
open(o + '/grande.html', 'w').write(h[:1 << 20])
open(o + '/hondo.html', 'w').write('<div><b><a href=x>' * 100000)
open(o + '/cabecera.eml', 'w').write('Subject: ' + '=?utf-8?q?a' * 200000 + '\r\nFrom: ' + '"a" <b@c.es>, ' * 100000 + '\r\n\r\nx')
PY
for c in "mime grande.eml" "html grande.html" "html hondo.html" "mime cabecera.eml" "dirs cabecera.eml" "2047 cabecera.eml"; do
	set -- $c
	t0=$(date +%s%N)
	if timeout 60 "$P" "$1" "$O/$2" > /dev/null 2> "$O/err.txt" && ! grep -q -e Sanitizer -e 'runtime error' "$O/err.txt"; then
		ms=$((($(date +%s%N) - t0) / 1000000))
		if [ $ms -lt 15000 ]; then ok "$1 de $2 en $ms ms"; else mal "$1 de $2: $ms ms (¿cuadrático?)"; fi
	else
		mal "$1 de $2"; head -10 "$O/err.txt"
	fi
done

# ---- para Hispa-Pentium ----
if echo 'int x;' | gcc -m32 -x c -c - -o /dev/null 2> /dev/null; then
	bien=1
	for c in juegos mime construye html vcard; do
		if ! gcc -m32 -std=gnu11 -march=i586 -O2 -ffreestanding -fno-pic -fno-stack-protector -fno-builtin -DCORREO_PENTIUM \
			$W -Werror -I"$C/inc" -I"$C" -c "$C/$c.c" -o "$O/$c.p.o" 2> "$O/avisos.txt"; then
			bien=0; cat "$O/avisos.txt"
		fi
		# sin las divisiones de 64 bits de libgcc ni la coma flotante
		if nm "$O/$c.p.o" | grep -qE ' U (__(u)?(div|mod)di3|__.*df|__.*sf|malloc|free|realloc)'; then
			bien=0; echo "$c.c pide: $(nm "$O/$c.p.o" | grep -E ' U (__|malloc|free|realloc)' | tr '\n' ' ')"
		fi
	done
	if [ $bien = 1 ]; then ok "se compila para Hispa-Pentium (-m32 -march=i586 -ffreestanding) sin avisos"; else mal "para Hispa-Pentium"; fi
else
	ok "(sin gcc -m32: no se mira lo de Hispa-Pentium)"
fi

if [ $fallos -eq 0 ]; then echo "pruebas_mime.sh: todo bien"; exit 0; fi
echo "pruebas_mime.sh: $fallos fallos"
exit 1
