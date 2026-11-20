#!/usr/bin/env python3
# brotli_dic.py - hace w32/surf/recursos/BROTLI.DIC: el diccionario de Brotli
# (RFC 7932, apéndice A: 122.784 bytes, sha256 20e42eb1...35c70) sacado de
# terceros/brotli/c/common/dictionary.c y comprimido con zlib (58 KB en vez de
# 120: el disquete «Internet 2» va justo). Lo lee w32/surf/woff.c.
#   uso: tools/brotli_dic.py [salida]
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
import hashlib, re, sys, zlib
src = open('terceros/brotli/c/common/dictionary.c').read()
bloque = src.split('/* GENERATED CODE START */')[1].split('/* GENERATED CODE END */')[0]
d = bytes(int(x) for x in re.findall(r'\d+', bloque))
assert len(d) == 122784, len(d)
assert hashlib.sha256(d).hexdigest() == '20e42eb1b511c21806d4d227d07e5dd06877d8ce7b3a817f378f313653f35c70'
open(sys.argv[1] if len(sys.argv) > 1 else 'w32/surf/recursos/BROTLI.DIC', 'wb').write(zlib.compress(d, 9))
