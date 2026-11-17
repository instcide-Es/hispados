#!/usr/bin/env python3
"""Lleva un acceso directo de un CENTRO.INI (ya en UTF-8, con saltos «\\n») a
otro grupo y escribe el resultado (con saltos de DOS) en la salida.
Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.

uso: mover_acceso.py CENTRO.txt NOMBRE_DEL_ACCESO GRUPO [1]
  El cuarto argumento, «1», quita además la clave TareasPrincipal= (para
  dejar el archivo como lo escribía la 0.9.22: tests/run_principal_tests.sh).
"""
import re
import sys

s = open(sys.argv[1], encoding='utf-8').read()
if len(sys.argv) > 4 and sys.argv[4] == '1':
    s = s.replace('TareasPrincipal=1\n', '')
m = re.search(r'\[Acceso\]\nNombre=' + re.escape(sys.argv[2]) + r'\n(?:(?!\[)[^\n]*\n)*', s)
if not m:
    sys.exit('no hay un acceso con ese nombre')
acc = m.group(0)
s = s[:m.start()] + s[m.end():]
i = s.index('Nombre=' + sys.argv[3] + '\n')
j = s.find('[Grupo]', i)
if j < 0:
    j = len(s)
s = s[:j] + acc + s[j:]
sys.stdout.write(s.replace('\n', '\r\n'))
