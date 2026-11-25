#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Hispa-DOS - tools/tam_com.py: ¿cabe un .COM, con sus datos sin valor
# inicial (.bss) y su pila, en los 64 KB de su segmento? El programa deja, en
# el desplazamiento OFS del archivo (por omisión 3: justo detrás del «jmp» del
# principio), un «dd» con la dirección de su último octeto (prog_end). NASM no
# avisa si eso pasa de 64 KB: la pila iría a parar encima del código.
# uso: tam_com.py PROGRAMA.COM [OFS]
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
import struct, sys

ruta = sys.argv[1]
ofs = int(sys.argv[2], 0) if len(sys.argv) > 2 else 3
d = open(ruta, 'rb').read()
fin = struct.unpack_from('<I', d, ofs)[0]
if fin < 0x100 + len(d) or fin > 0xFFF0:
    sys.stderr.write('tam_com: %s no cabe en 64 KB (acaba en 0x%X; como mucho 0xFFF0)\n' % (ruta, fin))
    sys.exit(1)
print('tam_com: %s acaba en 0x%04X (quedan %d octetos)' % (ruta, fin, 0xFFF0 - fin))
