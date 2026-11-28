#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo64/programas/hazventanas.sh SALIDA - Hispa-64, etapa 8
# (docs/64/HISPA-64.md, sección 14): los programas gráficos, con musl
# (estáticos, con musl-gcc: hazmusl.sh lo hace antes) y la biblioteca común
# de la oficina de Hispa-Pentium (castel/nucleo32/oficina/comun, con
# so_hispa64.c en vez de so_pentium.c):
#   VENTANAS   el servidor de ventanas y el escritorio (ventanas/ventanas.c)
#   NOTAS      el bloc de notas (ventanas/notas.c)
#   PLUMA      Pluma 2, el procesador de textos (oficina/pluma, tal cual)
#   CUENTAS    Cuentas 2, la hoja de cálculo (oficina/cuentas, tal cual)
#   PDF        [etapa 11 a] Hispa-PDF (castel/nucleo32/pdf, tal cual): la orden PDF
#   IMPRIMIR   [etapa 11 a] imprimir por IPP, sin CUPS (imprimir/imprimir.c)
# Los objetos se guardan en build/ventanas-obj (solo se rehacen los que han
# cambiado). Con SIN_OFICINA=1, sin Pluma ni Cuentas. Sin «make -j».
set -e
AQUI=$(cd "$(dirname "$0")" && pwd)
R=$(cd "$AQUI/../../.." && pwd)          # la raíz del repositorio
S=$1
[ -n "$S" ] || { echo "uso: hazventanas.sh SALIDA" >&2; exit 2; }
M=$R/build/musl/bin/musl-gcc
[ -x "$M" ] || { echo "hechos: (sin musl)"; exit 0; }
OFI=$R/castel/nucleo32/oficina
V=$AQUI/ventanas
O=$R/build/ventanas-obj
mkdir -p "$O" "$S"
CF="-std=gnu11 -O2 -g0 -fno-stack-protector -DOFI_HISPA64"
INC="-I$OFI/comun -I$V -I$R/terceros/miniz -I$R/terceros/stb"
W="-Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-unused-function -Wno-missing-field-initializers -Wno-unused-result"
[ -f "$O/letras_dejavu.c" ] && [ "$O/letras_dejavu.c" -nt "$OFI/comun/haz_letras.py" ] || python3 -I "$OFI/comun/haz_letras.py" "$R/terceros/dejavu" "$O/letras_dejavu.c"
OBJS=""
# compila FUENTE.c OBJETO [-w]
compila() {
  local c=$1 o=$2 w=$W
  [ "${3:-}" = "-w" ] && w="-w"
  if [ ! -f "$o" ] || [ "$c" -nt "$o" ] || [ -n "$(find "$OFI/comun" "$V" -name '*.h' -newer "$o" 2> /dev/null | head -1)" ]; then
    # shellcheck disable=SC2086
    "$M" $CF $w $INC -c "$c" -o "$o"
  fi
}
# lo común: lo de la oficina que usan todos
COMUN=""
for c in texto ttf letras lienzo; do
  f=""; [ $c = letras ] && f="-w"
  compila "$OFI/comun/$c.c" "$O/comun_$c.o" $f; COMUN="$COMUN $O/comun_$c.o"
done
compila "$O/letras_dejavu.c" "$O/letras_dejavu.o" -w
compila "$OFI/comun/so_hispa64.c" "$O/so_hispa64.o"
COMUN="$COMUN $O/letras_dejavu.o $O/so_hispa64.o"
compila "$V/ventanas.c" "$O/ventanas.o"
compila "$V/notas.c" "$O/notas.o"
# shellcheck disable=SC2086
"$M" -static -o "$S/VENTANAS" "$O/ventanas.o" $COMUN -lm
# shellcheck disable=SC2086
"$M" -static -o "$S/NOTAS" "$O/notas.o" $COMUN -lm
HECHOS="VENTANAS NOTAS"
# la oficina: sus fuentes.lst, tal cual
oficina() {   # PROGRAMA NOMBRE
  local p=$1 n=$2 objs="" c w o
  while read -r c w; do
    case "$c" in ""|\#*) continue;; esac
    o=$O/$(echo "$c" | tr / _).o
    compila "$OFI/$c.c" "$o" "$w"
    objs="$objs $o"
  done < "$OFI/$p/fuentes.lst"
  # shellcheck disable=SC2086
  "$M" -static -o "$S/$n" $objs "$O/letras_dejavu.o" "$O/so_hispa64.o" -lm
  HECHOS="$HECHOS $n"
}
if [ "${SIN_OFICINA:-0}" != 1 ]; then
  oficina pluma PLUMA
  oficina cuentas CUENTAS
fi
# [etapa 11 a] imprimir (docs/64/HISPA-64.md, sección 27): Hispa-PDF
# (castel/nucleo32/pdf, tal cual, con su so_anfitrion.c, que es POSIX) como
# la orden PDF (unir, separar, pasar a imágenes...), y su pintor dentro de
# IMPRIMIR (imprimir/imprimir.c: IPP sin CUPS; las páginas a PWG raster)
if [ "${SIN_OFICINA:-0}" != 1 ]; then
  HP=$R/castel/nucleo32/pdf
  [ -f "$O/hpdf_fuentes.c" ] && [ "$O/hpdf_fuentes.c" -nt "$HP/haz_fuentes.py" ] || python3 -I "$HP/haz_fuentes.py" "$R/terceros/dejavu" "$O/hpdf_fuentes.c"
  HPO=""
  for c in objetos documento filtros cifra escribe paginas util orden imagen fuente pinta texto miniz_hpdf; do
    o=$O/hpdf_$c.o
    if [ ! -f "$o" ] || [ "$HP/$c.c" -nt "$o" ] || [ -n "$(find "$HP" -name '*.h' -newer "$o" 2> /dev/null | head -1)" ]; then
      "$M" -std=gnu11 -O2 -g0 -fno-stack-protector -w -I"$HP" -I"$R/terceros/miniz" -I"$R/terceros/stb" -c "$HP/$c.c" -o "$o"
    fi
    HPO="$HPO $o"
  done
  [ -f "$O/hpdf_fuentesc.o" ] && [ "$O/hpdf_fuentesc.o" -nt "$O/hpdf_fuentes.c" ] || "$M" -std=gnu11 -O2 -w -I"$HP" -c "$O/hpdf_fuentes.c" -o "$O/hpdf_fuentesc.o"
  for v in so so_sinmain; do
    o=$O/hpdf_$v.o; d=""; [ $v = so_sinmain ] && d="-Dmain=hpdf_main_anfitrion"
    # shellcheck disable=SC2086
    [ -f "$o" ] && [ "$o" -nt "$HP/so_anfitrion.c" ] || "$M" -std=gnu11 -O2 -g0 -fno-stack-protector -w $d -I"$HP" -c "$HP/so_anfitrion.c" -o "$o"
  done
  # shellcheck disable=SC2086
  "$M" -static -o "$S/PDF" $HPO "$O/hpdf_fuentesc.o" "$O/hpdf_so.o" -lm
  # (ipps://, con la BearSSL de hazred.sh, build/bearssl-obj; sin ella, solo ipp://)
  BS=$R/build/bearssl-obj/libbearssl.a; TLS=""
  if [ -f "$BS" ]; then TLS="$BS"; else TLS=""; fi
  if [ -n "$TLS" ]; then
    "$M" $CF $W -I"$R/terceros/bearssl/inc" -c "$AQUI/imprimir/imprimir.c" -o "$O/imprimir.o"
  else
    "$M" $CF $W -DSIN_TLS -c "$AQUI/imprimir/imprimir.c" -o "$O/imprimir.o"
  fi
  # shellcheck disable=SC2086
  "$M" -static -o "$S/IMPRIMIR" "$O/imprimir.o" $HPO "$O/hpdf_fuentesc.o" "$O/hpdf_so_sinmain.o" $TLS -lm
  HECHOS="$HECHOS PDF IMPRIMIR"
fi
for f in $HECHOS; do strip --strip-unneeded "$S/$f"; done
echo "hechos: $HECHOS"
