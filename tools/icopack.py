#!/usr/bin/env python3
"""Empaqueta los iconos de Hispa Ventanas a 4 bits por punto.

uso: icopack.py ARCHIVO.inc...   (lo cambia en el sitio: se usa con las copias
                                  de build/gen/ventanas, no con los fuentes)

En ventanas/iconos*.inc los iconos están en texto, para poder retocarlos a
mano: una línea db "..." de 32 caracteres por fila, «.» transparente y 0-9,
a-f los 16 colores. Aquí cada fila pasa a 16 octetos (el punto de la
izquierda en los 4 bits bajos), que es como los lee dc_icono (dc.inc). El
transparente se guarda como el color 3 (azul petróleo claro), que por eso no
se puede usar en un icono.
Packs the text-art icons to 4 bits per pixel in the generated sources.
"""
import re, sys

FILA = re.compile(r'^(\s+)db\s+"([.0-9a-f]{32})"\s*$')


def nibble(c, donde):
    if c == '.':
        return 3
    if c == '3':
        sys.exit('%s: el color 3 no se puede usar en un icono (es el transparente)' % donde)
    return int(c, 16)


def main():
    for ruta in sys.argv[1:]:
        salida = []
        for n, linea in enumerate(open(ruta, encoding='latin-1'), 1):
            m = FILA.match(linea)
            if not m:
                salida.append(linea)
                continue
            t = m.group(2)
            octetos = [nibble(t[i], '%s:%d' % (ruta, n)) | (nibble(t[i + 1], '%s:%d' % (ruta, n)) << 4)
                       for i in range(0, 32, 2)]
            salida.append('%sdb %s\n' % (m.group(1), ', '.join('0x%02x' % o for o in octetos)))
        open(ruta, 'w', encoding='latin-1').writelines(salida)


if __name__ == '__main__':
    main()
