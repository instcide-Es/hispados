#!/bin/bash
# Baja los programas de OpenGL de otros con los que se prueba HispaGL
# (docs/486/OPENGL.md) a tests/thirdparty/dl/gl/ (no va en el repositorio):
#  - wglgears.c de los demos de Mesa (la etiqueta mesa-demos-9.0.0): los
#    engranajes de siempre, para Windows (WGL, sin GLUT). Licencia MIT
#    (Brian Paul, 1999-2001; el paso a WGL, Ben Skeggs, 2004).
# Se compila con gcc -m32, las cabeceras mínimas de tests/gl/terceros/ y
# tools/elf2pe.py (make build/WGLGEARS.EXE).
# Downloads third-party OpenGL programs used to test HispaGL.
set -e
cd "$(dirname "$0")"
D=dl/gl
mkdir -p $D
URL=https://gitlab.freedesktop.org/mesa/demos/-/raw/mesa-demos-9.0.0/src/wgl/wglgears.c
SUMA=d975939025963ec046d29fa438f4a9dc5c55190cae4f197488297bee68f8ef16
if [ ! -f $D/wglgears.c ] || ! echo "$SUMA  $D/wglgears.c" | sha256sum -c --quiet 2> /dev/null; then
  curl -sSfL -o $D/wglgears.c.nuevo "$URL"
  echo "$SUMA  $D/wglgears.c.nuevo" | sha256sum -c --quiet
  mv $D/wglgears.c.nuevo $D/wglgears.c
fi
echo "fetch_gl.sh: $D/wglgears.c (mesa-demos-9.0.0, MIT)"
