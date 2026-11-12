#!/usr/bin/env python3
"""Hace un programa de Hispa Ventanas (.HVP) a partir de su fuente en NASM.

uso: mkhvp.py FUENTE.asm SALIDA.HVP

El fuente usa ventanas/hvp.inc (docs/VENTANAS.md, «Programas aparte»). Pasos:
 1. pasa el fuente y los .inc de ventanas/ que usa a la página 437 en
    build/gen/hvp/ y empaqueta el icono (tools/icopack.py);
 2. lo ensambla dos veces, con dos direcciones de carga distintas: los
    cuatro octetos que cambian de una a otra son los punteros que hay que
    reubicar al cargar;
 3. escribe la imagen (ensamblada para la dirección 0), detrás la tabla de
    reubicaciones (un dd por puntero: su sitio en la imagen) y rellena la
    cabecera: tamaño del archivo, memoria, tamaño de la imagen y cuántas
    reubicaciones hay.
Builds an Hispa Ventanas program module (flat 32-bit image + relocations).
"""
import os, struct, subprocess, sys

RAIZ = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
INCS = ('hvp.inc', 'hvplib.inc', 'defs.inc', 'api.inc')
DELTA = 0x00100000
CAB = 64                                # la cabecera no se reubica


def main():
    fuente, salida = sys.argv[1], sys.argv[2]
    nasm = os.environ.get('NASM', 'nasm')
    nombre = os.path.splitext(os.path.basename(salida))[0].lower()
    gen = os.path.join(os.path.dirname(salida) or '.', 'gen', 'hvp', nombre)
    os.makedirs(gen, exist_ok=True)
    cp437 = os.path.join(RAIZ, 'tools', 'cp437.py')
    asm = os.path.join(gen, os.path.basename(fuente))
    subprocess.run([sys.executable, cp437, fuente, asm], check=True)
    for inc in INCS:
        subprocess.run([sys.executable, cp437, os.path.join(RAIZ, 'ventanas', inc),
                        os.path.join(gen, inc)], check=True)
    subprocess.run([sys.executable, os.path.join(RAIZ, 'tools', 'icopack.py'), asm], check=True)
    img = []
    for org in (0, DELTA):
        binario = os.path.join(gen, '%s.%d' % (nombre, len(img)))
        subprocess.run([nasm, '-f', 'bin', '-w+error=number-overflow', '-DHVP_ORG=%d' % org,
                        '-i', gen + os.sep, '-o', binario, asm], check=True)
        img.append(bytearray(open(binario, 'rb').read()))
    a, b = img
    if len(a) != len(b) or a[:4] != b'HVP1':
        sys.exit('%s: no es un módulo (falta HVP_CABECERA) o no se ensambla igual dos veces' % fuente)
    while len(a) % 4:
        a.append(0)
        b.append(0)
    reub, i = [], CAB
    while i < len(a):
        if a[i] == b[i]:
            i += 1
            continue
        o = i - 2                       # (DELTA cambia los dos octetos altos)
        if o < CAB or o + 4 > len(a) or \
           (struct.unpack_from('<I', b, o)[0] - struct.unpack_from('<I', a, o)[0]) & 0xFFFFFFFF != DELTA:
            sys.exit('%s: en el octeto %d hay algo que depende de la dirección de carga y no es '
                     'un puntero de 32 bits' % (fuente, i))
        reub.append(o)
        i = o + 4
    fin_mem, entrada = struct.unpack_from('<II', a, 12)
    if not CAB + 512 <= entrada < len(a):
        sys.exit('%s: la entrada no está dentro del código' % fuente)
    imagen = len(a)
    archivo = imagen + 4 * len(reub)
    memoria = (max(fin_mem, archivo) + 15) & ~15
    struct.pack_into('<II', a, 8, archivo, memoria)
    struct.pack_into('<II', a, 20, imagen, len(reub))
    with open(salida, 'wb') as f:
        f.write(a)
        f.write(b''.join(struct.pack('<I', o) for o in reub))
    print('%s: %d octetos (%d reubicaciones, %d de memoria)' % (salida, archivo, len(reub), memoria))


if __name__ == '__main__':
    main()
