#!/usr/bin/env python3
"""Hace el guion de botones que lee nesnativo (un octeto por cuadro).

uso: hispanes_guion.py SALIDA PASO...
Cada paso es «BOTONES:CUADROS» (o solo «CUADROS», sin botones). Los botones
van juntos, con estas letras: A, B, S (Select), T (Start), U (arriba),
D (abajo), L (izquierda), R (derecha). Ejemplo: 60 T:8 20 R:30 RA:4
Escribe por la salida normal cuántos cuadros son.
"""
import sys

BOT = {'A': 1, 'B': 2, 'S': 4, 'T': 8, 'U': 16, 'D': 32, 'L': 64, 'R': 128}


def guion(pasos):
    d = bytearray()
    for p in pasos:
        b, _, n = p.rpartition(':')
        d += bytes([sum(BOT[c] for c in b)]) * int(n)
    return d


if __name__ == '__main__':
    d = guion(sys.argv[2:])
    open(sys.argv[1], 'wb').write(d)
    print(len(d))
