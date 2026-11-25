#!/usr/bin/env python3
# Toca a mano la FAT12 de una imagen de disquete, para las pruebas de REVISAR:
#   fat12.py imagen pon GRUPO VALOR          la casilla del grupo (en las dos FAT)
#   fat12.py imagen primero NOMBRE.EXT G     el primer grupo de una entrada de la raíz
#   fat12.py imagen dir RUTA/DIR G           (lo mismo, de un directorio de la raíz)
#   fat12.py imagen grupo NOMBRE.EXT         escribe el primer grupo de la entrada
#   fat12.py imagen libres N                 escribe N grupos libres seguidos (el 1.º)
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
import struct, sys
img, orden = sys.argv[1], sys.argv[2]
d = bytearray(open(img, 'rb').read())
bps, spc, res, nfat, nraiz, _, _, spf = struct.unpack_from('<HBHBHHBH', d, 11)
raiz = (res + nfat * spf) * bps
def fat(g, v=None):
    for c in range(nfat):
        b = (res + c * spf) * bps + g * 3 // 2
        w = struct.unpack_from('<H', d, b)[0]
        if v is None:
            return (w >> 4) if g & 1 else (w & 0xFFF)
        w = (w & 0x000F) | (v << 4) if g & 1 else (w & 0xF000) | v
        struct.pack_into('<H', d, b, w)
def entrada(n):
    nom, _, ext = n.upper().partition('.')
    clave = nom.ljust(8).encode() + ext.ljust(3).encode()
    for i in range(nraiz):
        if d[raiz + 32 * i: raiz + 32 * i + 11] == clave:
            return raiz + 32 * i
    sys.exit('no está ' + n)
if orden == 'pon':
    fat(int(sys.argv[3]), int(sys.argv[4], 0))
elif orden in ('primero', 'dir'):
    struct.pack_into('<H', d, entrada(sys.argv[3]) + 26, int(sys.argv[4]))
elif orden == 'grupo':
    print(struct.unpack_from('<H', d, entrada(sys.argv[3]) + 26)[0]); sys.exit()
elif orden == 'libres':
    n, g, seguidos = int(sys.argv[3]), 2, 0
    total = (len(d) // bps - raiz // bps - (nraiz * 32 + bps - 1) // bps) // spc + 2
    while g < total:
        seguidos = seguidos + 1 if fat(g) == 0 else 0
        if seguidos == n:
            print(g - n + 1); sys.exit()
        g += 1
    sys.exit('no hay sitio')
open(img, 'wb').write(d)
