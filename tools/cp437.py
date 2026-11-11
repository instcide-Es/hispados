#!/usr/bin/env python3
"""Convierte un fuente UTF-8 a la página de códigos 437 antes de ensamblarlo,
para que los mensajes con acentos y eñes salgan bien en la pantalla de DOS.
Los caracteres que no existen en CP437 (solo pueden estar en comentarios)
se cambian por '?'.

uso: cp437.py entrada.asm salida.asm
"""
import os, sys

src, dst = sys.argv[1], sys.argv[2]
os.makedirs(os.path.dirname(dst) or '.', exist_ok=True)
text = open(src, encoding='utf-8').read()
with open(dst, 'wb') as f:
    f.write(text.encode('cp437', errors='replace'))
