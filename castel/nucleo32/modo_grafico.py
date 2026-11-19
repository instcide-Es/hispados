#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""nucleo32/modo_grafico.py NUCLEO.BIN SALIDA.BIN [ANCHO ALTO [BITS]]

Hace una copia del núcleo de Hispa-Pentium que le pide al cargador (GRUB) un
modo gráfico: en su cabecera multiboot pone la bandera de vídeo (el bit 2) y,
en el sitio que deja el compilador detrás (cuatro palabras a cero), la clase
de modo (0: gráfico, con la memoria de la pantalla seguida), el ancho, el alto
y los bits por punto (800 x 600 x 32 si no se dice). GRUB pone ese modo (o el
que más se le parezca) antes de entrar en el núcleo y le dice dónde está la
memoria de la pantalla; el núcleo arranca entonces su escritorio en él
(video.cas). Hace falta en los PC de verdad, donde no hay la tarjeta de
QEMU/Bochs que se programa por sus puertos.

El núcleo sin tocar pide lo de siempre (modo texto): con QEMU y -kernel, y
para la consola de texto con GRUB, se usa ese.
"""
import struct
import sys


def mira(a, b, ancho, alto, bits):
    """¿es «b» el núcleo «a» con el modo pedido en su cabecera, y nada más cambiado?"""
    a, b = open(a, 'rb').read(), open(b, 'rb').read()
    o = a.index(struct.pack('<I', 0x1BADB002))
    ma, fa, sa = struct.unpack_from('<III', a, o)
    mb, fb, sb = struct.unpack_from('<III', b, o)
    bien = fa & 4 == 0 and fb == fa | 4 and (mb + fb + sb) & 0xFFFFFFFF == 0 and a[o + 32:o + 48] == bytes(16)
    bien = bien and struct.unpack_from('<IIII', b, o + 32) == (0, ancho, alto, bits)
    return bien and a[:o + 4] == b[:o + 4] and a[o + 12:o + 32] == b[o + 12:o + 32] and a[o + 48:] == b[o + 48:]


def main():
    if len(sys.argv) > 6 and sys.argv[1] == '--mira':      # (para las pruebas)
        bien = mira(sys.argv[2], sys.argv[3], int(sys.argv[4]), int(sys.argv[5]), int(sys.argv[6]))
        print('modo_grafico.py: la cabecera %s' % ('está bien' if bien else 'NO está bien'))
        sys.exit(0 if bien else 1)
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    d = bytearray(open(sys.argv[1], 'rb').read())
    ancho = int(sys.argv[3]) if len(sys.argv) > 3 else 800
    alto = int(sys.argv[4]) if len(sys.argv) > 4 else 600
    bits = int(sys.argv[5]) if len(sys.argv) > 5 else 32
    for o in range(0, min(8192, len(d) - 48), 4):
        magia, banderas, suma = struct.unpack_from('<III', d, o)
        if magia == 0x1BADB002 and (magia + banderas + suma) & 0xFFFFFFFF == 0:
            break
    else:
        sys.exit('modo_grafico.py: %s no tiene cabecera multiboot' % sys.argv[1])
    if banderas & 4 == 0 and bytes(d[o + 32:o + 48]) != bytes(16):
        sys.exit('modo_grafico.py: la cabecera no deja sitio para el modo gráfico (¿un núcleo de antes de la 0.4?)')
    banderas |= 4
    struct.pack_into('<III', d, o, magia, banderas, (-(magia + banderas)) & 0xFFFFFFFF)
    struct.pack_into('<IIII', d, o + 32, 0, ancho, alto, bits)
    open(sys.argv[2], 'wb').write(bytes(d))
    print('modo_grafico.py: %s pide %d x %d x %d' % (sys.argv[2], ancho, alto, bits))


if __name__ == '__main__':
    main()
