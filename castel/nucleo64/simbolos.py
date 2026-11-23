#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo64/simbolos.py - Hispa-64 (docs/64/HISPA-64.md, etapa 1): los nombres
# de las funciones del núcleo, para nombre64.cas. Lee el mapa de NASM («[map
# symbols ...]») y escribe HISPA64.SIM: una línea por función, por orden,
# «FFFFFFFF80101234 Módulo.función», en la página 437 (como escribe el núcleo).
#   simbolos.py MAPA_DE_NASM HISPA64.SIM [MÓDULO_PRINCIPAL]
# F_Módulo__función -> Módulo.función; F_función (del módulo del programa) ->
# MÓDULO_PRINCIPAL.función; y las rutinas de apoyo (castel_..., escribir_...).
import re
import sys

mapa, salida = sys.argv[1], sys.argv[2]
principal = sys.argv[3] if len(sys.argv) > 3 else 'Hispa64'
vistos = {}
with open(mapa, encoding='utf-8', errors='replace') as f:
    for ln in f:
        m = re.match(r'^([0-9A-F]{16})\s+([0-9A-F]{16})\s+(\S+)$', ln.strip())
        if not m:
            continue
        nombre = m.group(3)
        if nombre.startswith('F_'):
            n = nombre[2:]
            n = n.replace('__', '.', 1) if '__' in n else principal + '.' + n
        elif re.match(r'^(castel_\w+|escribir_\w+)$', nombre):
            n = nombre
        elif nombre == 'C_alto':
            n = 'el arranque'
        else:
            continue
        vistos.setdefault(int(m.group(2), 16), n)
with open(salida, 'wb') as f:
    for d in sorted(vistos):
        f.write(('%016X %s\n' % (d, vistos[d])).encode('cp437', 'replace'))
