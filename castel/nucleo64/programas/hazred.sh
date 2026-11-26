#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo64/programas/hazred.sh - Hispa-64, etapa 9: los programas de red
# del anillo 3 (programas/red/), en C con musl (build/musl, de hazmusl.sh),
# estáticos, -O2 y sin símbolos, en la carpeta SALIDA:
#   PING       el eco de ICMP (ping socket o SOCK_RAW)
#   IFCONFIG   las interfaces, la puerta de enlace y los DNS
#   DESCARGA   HTTP/1.1 y HTTPS (BearSSL 0.6, terceros/bearssl, licencia MIT)
#   NTP        la hora por SNTP
#   ECO        el eco por TCP (servidor y cliente)
#   MRED       la prueba de las llamadas de red (por 127.0.0.1)
#   MRED6      [etapa 10 c] la de IPv6 (por ::1; con «red NOMBRE PUERTO», con la red)
# BearSSL se compila una vez en build/bearssl-obj (un gcc por archivo, sin
# make -j; solo lo que ha cambiado). Nada se instala en el sistema.
# Uso: hazred.sh SALIDA   (dice en la última línea los que ha hecho)
set -e
AQUI=$(cd "$(dirname "$0")" && pwd)
R=$(cd "$AQUI/../../.." && pwd)          # la raíz del repositorio
S=$1
[ -n "$S" ] || { echo "uso: hazred.sh SALIDA" >&2; exit 2; }
B=$R/build
M=$B/musl/bin/musl-gcc
if [ ! -x "$M" ] || [ ! -f "$B/musl/lib/libc.a" ]; then
  echo "hazred: no está musl en $B/musl (se hace con hazmusl.sh)" >&2
  exit 1
fi
mkdir -p "$S" "$B/bearssl-obj"
O=$B/bearssl-obj
# (en terceros/bearssl/src hay solo lo del perfil de TLS.BIN: sin i62 ni
# sslio; el azar se le da a mano: BR_INT128=0 y BR_UMUL128=0 para que no pida
# br_rsa_i62_*)
BI="-I$R/terceros/bearssl/inc -I$R/terceros/bearssl/src -DBR_INT128=0 -DBR_UMUL128=0 -DBR_USE_URANDOM=0 -DBR_USE_GETENTROPY=0"
for f in "$R"/terceros/bearssl/src/*.c; do
  o=$O/$(basename "$f" .c).o
  if [ ! -f "$o" ] || [ "$f" -nt "$o" ]; then
    # shellcheck disable=SC2086
    nice "$M" -O2 -fno-stack-protector $BI -c "$f" -o "$o"
    rm -f "$O/libbearssl.a"
  fi
done
[ -f "$O/libbearssl.a" ] || ar rcs "$O/libbearssl.a" "$O"/*.o
P=$AQUI/red
CF="-O2 -Wall -Wno-unused-result -Wno-unused-function -fno-stack-protector -static"
nice "$M" $CF -o "$S/PING" "$P/ping.c"
nice "$M" $CF -o "$S/IFCONFIG" "$P/ifconfig.c"
# shellcheck disable=SC2086
nice "$M" $CF -I"$R/terceros/bearssl/inc" -I"$R/w32/tls" -o "$S/DESCARGA" "$P/descarga.c" "$O/libbearssl.a"
nice "$M" $CF -o "$S/NTP" "$P/ntp.c"
nice "$M" $CF -o "$S/ECO" "$P/eco.c"
nice "$M" $CF -pthread -o "$S/MRED" "$P/mred.c"
nice "$M" $CF -pthread -o "$S/MRED6" "$P/mred6.c"        # [etapa 10 c] IPv6
for f in PING IFCONFIG DESCARGA NTP ECO MRED MRED6; do
  strip --strip-unneeded "$S/$f"
done
echo "hechos: PING IFCONFIG DESCARGA NTP ECO MRED MRED6"
