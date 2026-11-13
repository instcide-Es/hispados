#!/bin/bash
# Laboratorio de compatibilidad de juegos (NO va en `make test`: necesita un
# ISO privado con juegos de DOS; los juegos no se distribuyen con Hispa-DOS).
#
#   JUEGOS_ISO=/ruta/Juegos.ISO JUEGOS_TXT=/ruta/juegos.txt tests/juegos/lab.sh [opciones] [CARPETA ...]
#
# Arranca el disquete de uso (make dist) en QEMU con el ISO como CD (HSPCD),
# -vga std (o --vga cirrus), Sound Blaster 16, AdLib y teclado/ratón PS/2;
# copia cada juego a un disco duro de trabajo, lo lanza, pulsa Intro/Espacio,
# saca capturas (screendump) y dice: arranca / pantalla negra / error / vuelve
# a DOS. Resultados (tabla.tsv, capturas .png y un .txt por juego) en --sal.
# Qué se lanza en cada carpeta lo dice la lista de JUEGOS_TXT (o --lista), que
# tampoco va en el proyecto; su formato, en tests/juegos/juegos.ejemplo.txt.
# Más en lab.py --help.
# Game compatibility lab (needs a private ISO; not part of `make test`).
set -eu
cd "$(dirname "$0")/../.."
[ -f build/hispados-disquete.img ] || make -s dist
exec python3 tests/juegos/lab.py "$@"
