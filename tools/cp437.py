#!/usr/bin/env python3
"""Convierte un fuente UTF-8 a la página de códigos 437 antes de ensamblarlo,
para que los mensajes con acentos y eñes salgan bien en la pantalla de DOS.
Los caracteres que no existen en CP437 (solo pueden estar en comentarios)
se cambian por '?'.

Con --ventanas (Hispa Ventanas y sus programas .HVP), las letras que la 437
no tiene pero la letra de Ventanas sí (Á, Ó, €...: tools/hispa437.py) van con
su código de Ventanas: «Última» sale bien en una ventana (en modo texto, no).

uso: cp437.py [--ventanas] entrada.asm salida.asm
"""
import os, sys

args = sys.argv[1:]
ventanas = '--ventanas' in args
src, dst = [a for a in args if a != '--ventanas']
os.makedirs(os.path.dirname(dst) or '.', exist_ok=True)
text = open(src, encoding='utf-8').read()
if ventanas:
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from hispa437 import DE_LETRA
    text = ''.join(chr(0xE000 + DE_LETRA[c]) if c in DE_LETRA else c for c in text)
    out = bytearray()
    for c in text:
        o = ord(c)
        if 0xE000 <= o <= 0xE0FF:
            out.append(o - 0xE000)
        else:
            out += c.encode('cp437', errors='replace')
    data = bytes(out)
else:
    data = text.encode('cp437', errors='replace')
# Si ya está igual, no se toca. Si no, se escribe con otro nombre y se cambia
# al final: así, con «make -j», nadie lee un archivo a medias (lo que dejaba
# programas de 0 octetos en las imágenes: auditoría del 25/11/2026).
try:
    with open(dst, 'rb') as f:
        if f.read() == data:
            sys.exit(0)
except OSError:
    pass
tmp = '%s.%d.tmp' % (dst, os.getpid())
with open(tmp, 'wb') as f:
    f.write(data)
os.replace(tmp, dst)
