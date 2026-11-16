#!/bin/bash
# Prueba del descodificador de JPEG (programas/comun/jpeg.inc), fuera de
# Hispa Ventanas: tests/tools/jpeg_prueba.py lo ensambla tal cual en un
# programa de Linux y lo compara punto a punto con libjpeg-turbo (por Pillow y
# por simplejpeg) y con un modelo propio; le da JPEG cortados y estropeados a
# miles, con los datos y la memoria pegados a páginas prohibidas; y los JPEG de
# verdad que haya en la máquina (solo los lee).
#
# Necesita nasm, ld y un Python con Pillow, numpy y simplejpeg. Si el python3
# del sistema no los tiene, se hace un entorno virtual aparte (no se instala
# nada en el sistema): en $JPEG_VENV o, si no se dice, en
# ${TMPDIR:-/tmp}/hispados-jpeg-venv. Con JPEG_PY=/ruta/a/python se usa ese.
#
#   JPEG_AZAR=400     cuántos JPEG al azar por parte
#   JPEG_ROTOS=4000   cuántos estropeados
#   JPEG_VERDAD=80    cuántos de los de la máquina, a lo más
#   JPEG_SOLO=ABCDEFG qué partes (ver jpeg_prueba.py)
set -u
cd "$(dirname "$0")/.."
PY=${JPEG_PY:-}
tiene() { "$1" -c 'import PIL, numpy, simplejpeg' 2>/dev/null; }
if [[ -z $PY ]]; then
  if tiene python3; then PY=python3
  else
    V=${JPEG_VENV:-${TMPDIR:-/tmp}/hispados-jpeg-venv}
    if ! tiene "$V/bin/python"; then
      echo "== (se prepara un entorno de Python con Pillow, numpy y simplejpeg en $V) =="
      python3 -m venv "$V" && "$V/bin/pip" install -q pillow numpy simplejpeg
    fi
    PY=$V/bin/python
  fi
fi
if ! tiene "$PY"; then
  echo "  FAIL no hay un Python con Pillow, numpy y simplejpeg (JPEG_PY o JPEG_VENV)"
  echo "0 OK / 1 FAIL"
  exit 1
fi
echo "== jpeg.inc: el descodificador de JPEG, fuera de Hispa Ventanas =="
"$PY" tests/tools/jpeg_prueba.py --azar ${JPEG_AZAR:-400} --rotos ${JPEG_ROTOS:-4000} --de-verdad ${JPEG_VERDAD:-80} \
      --solo ${JPEG_SOLO:-ABCDEFG} | tee /tmp/jpeg_prueba.$$.txt
r=${PIPESTATUS[0]}
grep -o '[0-9]* OK / [0-9]* FAIL' /tmp/jpeg_prueba.$$.txt | tail -1
rm -f /tmp/jpeg_prueba.$$.txt
exit $r
