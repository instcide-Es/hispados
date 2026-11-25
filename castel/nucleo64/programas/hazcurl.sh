#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo64/programas/hazcurl.sh - Hispa-64, etapa 10 b (docs/64/HISPA-64.md,
# sección 19): curl con BearSSL, en C con musl (build/musl), estáticos, en la
# carpeta SALIDA:
#   CURL    la orden curl 8.14.1 de siempre (terceros/curl, licencia de curl)
#   MCURL   la prueba de libcurl (programas/red/mcurl.c): la interfaz «multi»
#           con descargas a la vez por 127.0.0.1 (y su servidor dentro), y
#           bajar una dirección con su sha256 (la de las pruebas de red)
# libcurl se compila una vez en build/curl-obj, con la configuración que
# sacó su configure para x86_64-linux-musl (terceros/curl/lib/curl_config.h);
# BearSSL entero (terceros/bearssl/src y src64: lo que pide
# br_ssl_client_init_full, además de lo de TLS.BIN) en build/bearssl64-obj.
# Un gcc por archivo, sin make -j; solo lo que ha cambiado.
# Uso: hazcurl.sh SALIDA   (dice en la última línea los que ha hecho)
set -e
AQUI=$(cd "$(dirname "$0")" && pwd)
R=$(cd "$AQUI/../../.." && pwd)
S=$1
[ -n "$S" ] || { echo "uso: hazcurl.sh SALIDA" >&2; exit 2; }
B=$R/build
M=$B/musl/bin/musl-gcc
[ -x "$M" ] || { echo "hazcurl: no está musl en $B/musl (se hace con hazmusl.sh)" >&2; exit 1; }
BR=$R/terceros/bearssl
C=$R/terceros/curl
# ---- BearSSL entero, para curl ----
OB=$B/bearssl64-obj
mkdir -p "$OB" "$S"
BI="-I$BR/inc -I$BR/src"
for f in "$BR"/src/*.c "$BR"/src64/*.c; do
  o=$OB/$(basename "$f" .c).o
  if [ ! -f "$o" ] || [ "$f" -nt "$o" ]; then
    # shellcheck disable=SC2086
    nice "$M" -O2 -fno-stack-protector $BI -c "$f" -o "$o"
    rm -f "$OB/libbearssl.a"
  fi
done
[ -f "$OB/libbearssl.a" ] || ar rcs "$OB/libbearssl.a" "$OB"/*.o
# ---- libcurl ----
OC=$B/curl-obj
mkdir -p "$OC/lib" "$OC/src"
lista() {   # CARPETA VARIABLES...: los archivos de su Makefile.inc
  local d=$1; shift
  { echo "include Makefile.inc"; echo "all:"; printf '\t@echo'; for v in "$@"; do printf ' $(%s)' "$v"; done; echo; } > "$OC/lista.mk"
  (cd "$d" && make -s -f "$OC/lista.mk")
}
CF="-O2 -fno-stack-protector -D_GNU_SOURCE -DHAVE_CONFIG_H -DCURL_STATICLIB -I$C/include -I$C/lib $BI"
cambio=0
for f in $(lista "$C/lib" CSOURCES); do
  o=$OC/lib/$(echo "$f" | tr / _ | sed 's/\.c$/.o/')
  if [ ! -f "$o" ] || [ "$C/lib/$f" -nt "$o" ] || [ "$C/lib/curl_config.h" -nt "$o" ]; then
    # shellcheck disable=SC2086
    nice "$M" $CF -DBUILDING_LIBCURL -c "$C/lib/$f" -o "$o"
    cambio=1
  fi
done
if [ $cambio = 1 ] || [ ! -f "$OC/libcurl.a" ]; then rm -f "$OC/libcurl.a"; ar rcs "$OC/libcurl.a" "$OC"/lib/*.o; fi
# ---- la orden curl ----
# las autoridades de siempre (w32/tls/raices, las de DESCARGA y TLS.BIN),
# dentro de la orden (CURL_CA_EMBED): sin --cacert, son las suyas
CA=$OC/src/ca_embed.c
if [ ! -f "$CA" ] || [ -n "$(find "$R/w32/tls/raices" -newer "$CA")" ]; then
  cat "$R"/w32/tls/raices/*.pem | python3 -I -c 'import sys; d = sys.stdin.buffer.read() + b"\0"; print("const unsigned char curl_ca_embed[] = {"); [print(",".join(str(c) for c in d[i:i+24]) + ",") for i in range(0, len(d), 24)]; print("};")' > "$CA"
  rm -f "$OC/src/ca_embed.o"
fi
[ -f "$OC/src/ca_embed.o" ] || nice "$M" -O2 -c "$CA" -o "$OC/src/ca_embed.o"
for f in $(lista "$C/src" CURL_CFILES CURLX_CFILES); do
  o=$OC/src/$(echo "$f" | sed 's|^\.\./lib/||' | tr / _ | sed 's/\.c$/.o/')
  if [ ! -f "$o" ] || [ "$C/src/$f" -nt "$o" ] || [ "$C/lib/curl_config.h" -nt "$o" ] || [ "$AQUI/hazcurl.sh" -nt "$o" ]; then
    # shellcheck disable=SC2086
    nice "$M" $CF -DCURL_CA_EMBED -I"$C/lib/curlx" -I"$C/src" -c "$C/src/$f" -o "$o"
  fi
done
nice "$M" -static -o "$S/CURL" "$OC"/src/*.o "$OC/libcurl.a" "$OB/libbearssl.a" -pthread
# ---- la prueba de libcurl ----
# shellcheck disable=SC2086
nice "$M" -O2 -Wall -Wno-unused-result -fno-stack-protector -static -DCURL_STATICLIB -I"$C/include" $BI \
  -o "$S/MCURL" "$AQUI/red/mcurl.c" "$OC/libcurl.a" "$OB/libbearssl.a" -pthread
strip --strip-unneeded "$S/CURL" "$S/MCURL"
echo "hechos: CURL MCURL"
