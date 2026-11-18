#!/bin/bash
# mkanclas.sh - rehace w32/tls/anclas.h (las autoridades raíz de HSPTLS.DLL de
# Hispa-486) a partir de los certificados PEM de w32/tls/raices, con la
# herramienta «brssl» de BearSSL (se compila del original: bearssl-0.6.tar.gz).
#   tools/mkanclas.sh /ruta/a/brssl
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
set -e
cd "$(dirname "$0")/.."
BRSSL=${1:?uso: tools/mkanclas.sh /ruta/a/brssl}
cat w32/tls/raices/*.pem > /tmp/anclas_$$.pem
"$BRSSL" ta /tmp/anclas_$$.pem > w32/tls/anclas.h
rm -f /tmp/anclas_$$.pem
grep TAs_NUM w32/tls/anclas.h
