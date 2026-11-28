#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo64/programas/hazcasa.py - Hispa-64, etapa 11 a (docs/64/HISPA-64.md,
# sección 27): pone la marca de la casa (Casa64) en el sector 32 de una
# partición FAT32 ya hecha (mkfs.fat -F 32 -R 32832: el diario, los sectores
# 64 a 32831, dentro de los reservados).
#   hazcasa.py PARTICIÓN [INI [SECTORES]]       (la imagen de la partición sola)
#   hazcasa.py --mira PARTICIÓN                 dice el estado de la marca
#   hazcasa.py --estropea PARTICIÓN             (pruebas) la deja como un corte sin diario
import struct
import sys
import zlib


def marca(ini, n):
    b = bytearray(512)
    b[0:16] = b'HISPA64 CASA' + bytes(4)
    struct.pack_into('<IIII', b, 16, 1, ini, n, 0)
    struct.pack_into('<I', b, 508, zlib.crc32(bytes(b[:508])) & 0xFFFFFFFF)
    return bytes(b)


def estropea(p):
    # (pruebas) lo que deja un corte sin diario: 3 cúmulos perdidos (ocupados
    # y de nadie), el primer archivo de la raíz 3 cúmulos más largo que su
    # cadena, y la marca «en uso» (no se cerró bien)
    with open(p, 'r+b') as f:
        bs = f.read(512)
        bps, spc, res, nf = struct.unpack_from('<HBHB', bs, 11)
        fsz = struct.unpack_from('<I', bs, 36)[0]
        raiz = struct.unpack_from('<I', bs, 44)[0]
        fat = res * bps
        datos = (res + nf * fsz) * bps
        oc = bps * spc
        f.seek(fat)
        t = bytearray(f.read(fsz * bps))
        total = struct.unpack_from('<I', bs, 32)[0]
        n = min(len(t) // 4, (total - datos // bps) // spc + 2)
        perdidos = []
        c = n - 1
        while len(perdidos) < 3 and c > 2:
            if struct.unpack_from('<I', t, c * 4)[0] & 0x0FFFFFFF == 0:
                perdidos.append(c)
            c -= 1
        for c in perdidos:
            for k in range(nf):
                f.seek(fat + k * fsz * bps + c * 4)
                f.write(struct.pack('<I', 0x0FFFFFFF))
        f.seek(datos + (raiz - 2) * oc)
        d = f.read(oc)
        hecho = None
        for i in range(0, oc, 32):
            e = d[i:i + 32]
            if e[0] == 0:
                break
            if e[0] == 0xE5 or e[11] & 0x3F == 0x0F or e[11] & 0x18:
                continue
            tam = struct.unpack_from('<I', e, 28)[0]
            f.seek(datos + (raiz - 2) * oc + i + 28)
            f.write(struct.pack('<I', tam + 3 * oc))
            hecho = e[:11].decode('latin-1').strip()
            break
        f.seek(32 * 512)
        m = bytearray(f.read(512))
        struct.pack_into('<I', m, 28, 1)
        struct.pack_into('<I', m, 508, zlib.crc32(bytes(m[:508])) & 0xFFFFFFFF)
        f.seek(32 * 512)
        f.write(m)
    print('estropeado: %d cúmulos perdidos, %s 3 cúmulos más largo; la marca, en uso' % (len(perdidos), hecho))
    return 0


def main():
    if sys.argv[1] == '--mira':
        with open(sys.argv[2], 'rb') as f:
            f.seek(32 * 512)
            b = f.read(512)
        if b[:12] != b'HISPA64 CASA' or zlib.crc32(b[:508]) & 0xFFFFFFFF != struct.unpack_from('<I', b, 508)[0]:
            print('sin la marca de la casa')
            return 1
        v, ini, n, estado = struct.unpack_from('<IIII', b, 16)
        veces, cortes = struct.unpack_from('<QQ', b, 40)
        print('casa: versión %d, el diario en %d (+%d sectores), %s, montada %d veces, %d sin cerrar'
              % (v, ini, n, 'cerrada bien' if estado == 0 else 'EN USO (no se cerró)', veces, cortes))
        return 0
    if sys.argv[1] == '--estropea':
        return estropea(sys.argv[2])
    p = sys.argv[1]
    ini = int(sys.argv[2]) if len(sys.argv) > 2 else 64
    n = int(sys.argv[3]) if len(sys.argv) > 3 else 32768
    with open(p, 'r+b') as f:
        bs = f.read(512)
        res = struct.unpack_from('<H', bs, 14)[0]
        if bs[82:90] != b'FAT32   ' or ini + n > res:
            print('hazcasa.py: no es FAT32 o el diario no cabe en los %d sectores reservados' % res, file=sys.stderr)
            return 1
        f.seek(32 * 512)
        f.write(marca(ini, n))
        # (el diario, a cero: sin transacción)
        f.seek(ini * 512)
        f.write(bytes(16 * 4096))
    return 0


sys.exit(main())
