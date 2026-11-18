#!/bin/bash
# [JS] compila tests/navjs/anfitrion (el JavaScript del Navegador en Linux,
# gcc -m32 con su biblioteca) en build/navjs/. Sale con 1 si no se puede.
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
set -e
cd "$(dirname "$0")/../.."
B=build/navjs
mkdir -p $B
echo 'int main(void){return 0;}' > $B/t.c
gcc -m32 $B/t.c -o $B/t 2> /dev/null || exit 1
Q=terceros/quickjs
QF="-m32 -O1 -g -D_GNU_SOURCE -DCONFIG_VERSION=\"$(cat $Q/VERSION)\" -DCONFIG_SIN_HILOS -fwrapv -funsigned-char -msse2 -mfpmath=sse -w"
for f in quickjs libregexp libunicode cutils dtoa; do
  [ $B/$f.o -nt $Q/$f.c ] || gcc $QF -c $Q/$f.c -o $B/$f.o
done
python3 -c "import sys; d = open(sys.argv[1], 'rb').read(); print('static const char dom_js[] = {' + ','.join(str(b) for b in d) + ',0};')" w32/navega/dom.js > $B/nav_domjs.h
NF="-m32 -O0 -g -Wall -Wno-parentheses -Wno-sign-compare -Wno-unused-parameter -Wno-builtin-declaration-mismatch -Wno-incompatible-pointer-types -Iw32/navega -I$B -I$Q"
W="-Dsocket=w_socket -Dconnect=w_connect -Dsend=w_send -Drecv=w_recv -Dclosesocket=w_closesocket -Dioctlsocket=w_ioctlsocket -Dselect=w_select -Dgethostbyname=w_gethostbyname -Dhtons=w_htons"
gcc $NF -c w32/navega/html.c -o $B/html.o
gcc $NF -c w32/navega/pagina.c -o $B/pagina.o
gcc $NF $W -c w32/navega/http.c -o $B/http.o
gcc $NF -c w32/navega/dom.c -o $B/dom.o
gcc $NF -c w32/navega/script.c -o $B/script.o
gcc $NF -c tests/navjs/anfitrion.c -o $B/anfitrion.o
gcc -m32 -o $B/anfitrion $B/anfitrion.o $B/html.o $B/pagina.o $B/http.o $B/dom.o $B/script.o $B/quickjs.o $B/libregexp.o $B/libunicode.o $B/cutils.o $B/dtoa.o -lm
