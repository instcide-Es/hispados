#!/usr/bin/env python3
"""Hace las ROM de prueba de los cartuchos de Hispa-Nes (MAPTESTn.NES).

uso: mkmaptest.py CARPETA        deja en ella MAPTEST1.NES, 2, 3, 4, 7 y 66

Ensambla tests/hispanes/maptest.s para cada tipo de cartucho (el trozo de
16 KB de C000h) y le pone delante la cabecera iNES y los demás bancos, cada
uno con su número en el primer octeto, que es lo que el programa comprueba
al pedirlos. Son ROM propias: no hay nada de terceros.
"""
import os, subprocess, sys

RAIZ = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')


def banco(n, tam):
    return bytes([n]) + bytes([0xFF]) * (tam - 1)


def rom(mapa, codigo):
    chr_ = b''
    if mapa == 1:                       # 8 bancos de 16 KB; dibujos en RAM
        prg = b''.join(banco(i, 16384) for i in range(7)) + codigo
    elif mapa == 2:                     # 4 bancos de 16 KB; dibujos en RAM
        prg = b''.join(banco(i, 16384) for i in range(3)) + codigo
    elif mapa == 3:                     # 16 KB y 4 bancos de dibujos de 8 KB
        prg = codigo
        chr_ = b''.join(banco(i, 8192) for i in range(4))
    elif mapa == 4:                     # 6 bancos de 8 KB y los dos fijos; 8 KB de dibujos de 1 KB
        prg = b''.join(banco(i, 8192) for i in range(6)) + codigo
        chr_ = b''.join(banco(i, 1024) for i in range(8))
    else:                               # 7 y 66: 4 bancos de 32 KB, el programa en todos
        prg = b''.join(banco(i, 16384) + codigo for i in range(4))
        if mapa == 66:
            chr_ = b''.join(banco(i, 8192) for i in range(4))
    cab = b'NES\x1a' + bytes([len(prg) // 16384, len(chr_) // 8192, (mapa & 15) << 4, mapa & 0xF0]) + bytes(8)
    return cab + prg + chr_


def main():
    carpeta = sys.argv[1]
    for mapa in (1, 2, 3, 4, 7, 66):
        tmp = os.path.join(carpeta, 'maptest.bin')
        subprocess.run([sys.executable, os.path.join(RAIZ, 'tools', 'asm6502.py'), '-D', 'MAPA=%d' % mapa,
                        os.path.join(RAIZ, 'tests', 'hispanes', 'maptest.s'), tmp],
                       check=True, stdout=subprocess.DEVNULL)
        codigo = open(tmp, 'rb').read()
        assert len(codigo) == 16384, len(codigo)
        with open(os.path.join(carpeta, 'MAPTEST%d.NES' % mapa), 'wb') as f:
            f.write(rom(mapa, codigo))
        os.remove(tmp)


if __name__ == '__main__':
    main()
