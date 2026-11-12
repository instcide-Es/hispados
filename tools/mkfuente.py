#!/usr/bin/env python3
"""Genera la fuente de mapa de bits de Hispa Ventanas (8x16, CP437) a partir
del diseño propio en texto de ventanas/fuente.txt.

uso: mkfuente.py fuente.txt salida.inc [vista.ppm]

Formato del diseño:
  = X [fila]        empieza el carácter X (o :nombre para una pieza) cuya
                    primera línea va en la fila indicada (3 si no se dice);
                    siguen sus líneas, con '#' punto encendido y '.' apagado
                    (hasta 8 columnas; lo que falta queda apagado)
  + X base pieza [desplazamiento]
                    carácter compuesto: el dibujo de «base» más la pieza
                    (un acento), subida o bajada «desplazamiento» filas
  ; comentario
Los caracteres que no se diseñan salen como un recuadro.
Builds the Hispa Ventanas bitmap font from our own text design.
"""
import sys

ALTO, ANCHO = 16, 8


def main():
    src, dst = sys.argv[1], sys.argv[2]
    glifos = {}          # nombre -> lista de 16 enteros
    actual = None
    fila = 0
    for n, linea in enumerate(open(src, encoding='utf-8'), 1):
        linea = linea.rstrip('\n')
        if not linea.strip() or linea.startswith('; '):
            continue
        if linea.startswith('= '):
            p = linea.split()
            actual = p[1]
            fila = int(p[2]) if len(p) > 2 else 3
            if actual in glifos:
                sys.exit('%s:%d: «%s» repetido' % (src, n, actual))
            glifos[actual] = [0] * ALTO
            continue
        if linea.startswith('+ '):
            p = linea.split()
            nombre, base, pieza = p[1], p[2], p[3]
            d = int(p[4]) if len(p) > 4 else 0
            if base not in glifos or pieza not in glifos:
                sys.exit('%s:%d: falta «%s» o «%s»' % (src, n, base, pieza))
            g = list(glifos[base])
            for i, v in enumerate(glifos[pieza]):
                if v:
                    if not 0 <= i + d < ALTO:
                        sys.exit('%s:%d: la pieza se sale' % (src, n))
                    g[i + d] |= v
            glifos[nombre] = g
            actual = None
            continue
        if actual is None:
            sys.exit('%s:%d: línea suelta' % (src, n))
        t = linea.strip()
        if len(t) > ANCHO or set(t) - set('#.'):
            sys.exit('%s:%d: línea no válida «%s»' % (src, n, t))
        if fila >= ALTO:
            sys.exit('%s:%d: «%s» tiene demasiadas filas' % (src, n, actual))
        v = 0
        for i, c in enumerate(t):
            if c == '#':
                v |= 0x80 >> i
        glifos[actual][fila] = v
        fila += 1

    caja = [0, 0, 0, 0x7C, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x7C, 0, 0, 0, 0]
    tabla = [list(caja) for _ in range(256)]
    tabla[0] = [0] * ALTO
    tabla[32] = [0] * ALTO
    tabla[255] = [0] * ALTO
    hechos = 0
    for nombre, g in glifos.items():
        if nombre.startswith(':') and len(nombre) > 1:
            continue
        try:
            cod = nombre.encode('cp437')
        except UnicodeEncodeError:
            sys.exit('«%s» no existe en CP437' % nombre)
        if len(cod) != 1:
            sys.exit('«%s» no es un solo carácter' % nombre)
        tabla[cod[0]] = g
        hechos += 1
    with open(dst, 'w') as f:
        f.write('; Generado por tools/mkfuente.py a partir de ventanas/fuente.txt\n')
        f.write('; (%d caracteres). No editar.\n' % hechos)
        f.write('fuente8x16:\n')
        for c in range(256):
            f.write('        db ' + ','.join('0x%02X' % v for v in tabla[c]) + ' ; %02X\n' % c)
    if len(sys.argv) > 3:
        # vista previa: 16x16 caracteres, ampliados x2
        Z = 2
        w, h = 16 * (ANCHO + 2) * Z, 16 * (ALTO + 2) * Z
        px = bytearray(b'\xfb\xf3\xe0' * (w * h))
        for c in range(256):
            ox, oy = (c % 16) * (ANCHO + 2) + 1, (c // 16) * (ALTO + 2) + 1
            for y in range(ALTO):
                for x in range(ANCHO):
                    if tabla[c][y] & (0x80 >> x):
                        for yy in range(Z):
                            for xx in range(Z):
                                o = (((oy + y) * Z + yy) * w + (ox + x) * Z + xx) * 3
                                px[o:o + 3] = b'\x1a\x14\x10'
        with open(sys.argv[3], 'wb') as f:
            f.write(b'P6\n%d %d\n255\n' % (w, h) + bytes(px))


if __name__ == '__main__':
    main()
