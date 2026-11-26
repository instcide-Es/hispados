#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Hispa-DOS - tools/sin_vacios.py: que no vaya a una imagen un programa de 0
# octetos. (Con «make -j», un NASM que moría a mitad dejaba su .COM vacío y
# la siguiente vez se daba por hecho: PARTIR, APAGAR y DESCARGA de 0 octetos
# dentro de las imágenes; auditoría del 25/11/2026.) Mira los .COM, .SYS,
# .EXE, .HVP, .BIN y .DLL de los argumentos; los demás no (puede haber datos
# vacíos a propósito). Sale con 1 si falta alguno o está vacío.
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
import os, sys

EXT = ('.COM', '.SYS', '.EXE', '.HVP', '.BIN', '.DLL')
mal = []
for r in sys.argv[1:]:
    if not r.upper().endswith(EXT):
        continue
    if not os.path.isfile(r):
        mal.append(r + ' (no está)')
    elif os.path.getsize(r) == 0:
        mal.append(r + ' (0 octetos)')
if mal:
    sys.stderr.write('sin_vacios: no se hace la imagen; vuelva a compilarlos (rm y make):\n  ' + '\n  '.join(mal) + '\n')
    sys.exit(1)
