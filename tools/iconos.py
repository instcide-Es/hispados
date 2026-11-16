#!/usr/bin/env python3
"""Dibuja los iconos de 16x16 de la barra de Pluma y los escribe como líneas
«db» de NASM (un carácter por punto: «.» transparente, 0-9 y a-f los colores de
la paleta de Hispa Ventanas)."""
def lienzo(): return [['.'] * 16 for _ in range(16)]
def pon(g, x, y, c):
    if 0 <= x < 16 and 0 <= y < 16: g[y][x] = c
def caja(g, x0, y0, x1, y1, borde='0', fondo=None):
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            if x in (x0, x1) or y in (y0, y1): pon(g, x, y, borde)
            elif fondo: pon(g, x, y, fondo)
def raya(g, x0, y0, x1, y1, c='0'):
    n = max(abs(x1 - x0), abs(y1 - y0)) or 1
    for i in range(n + 1):
        pon(g, round(x0 + (x1 - x0) * i / n), round(y0 + (y1 - y0) * i / n), c)
def lineas(g, filas, c='0'):
    for y, x0, x1 in filas:
        for x in range(x0, x1 + 1): pon(g, x, y, c)
def texto(g, filas, x0=0, y0=0):
    for j, f in enumerate(filas):
        for i, ch in enumerate(f):
            if ch != ' ': pon(g, x0 + i, y0 + j, ch)

I = {}
g = lienzo(); caja(g, 3, 1, 12, 14, '0', '9'); pon(g, 12, 1, '.'); pon(g, 11, 1, '.'); pon(g, 12, 2, '.')
raya(g, 10, 1, 12, 3); lineas(g, [(2, 10, 10), (3, 10, 12)]); I['nuevo'] = g
g = lienzo(); caja(g, 2, 3, 7, 5, '0', 'a'); caja(g, 1, 5, 14, 12, '0', 'a'); lineas(g, [(7, 2, 13)], '9'); I['abrir'] = g
g = lienzo(); caja(g, 2, 2, 13, 13, '0', 'c'); caja(g, 4, 2, 11, 6, '0', '9'); caja(g, 5, 9, 10, 13, '0', 'f'); pon(g, 8, 10, '0'); pon(g, 8, 11, '0'); I['guardar'] = g
g = lienzo(); caja(g, 4, 1, 11, 6, '0', '9'); caja(g, 1, 6, 14, 11, '0', 'f'); caja(g, 4, 10, 11, 14, '0', '9'); pon(g, 12, 8, 'b'); lineas(g, [(12, 6, 9)], '7'); I['imprimir'] = g
g = lienzo(); raya(g, 4, 1, 10, 9); raya(g, 11, 1, 5, 9)
for cx in (4, 11):
    for dx, dy in ((0, -2), (1, -2), (2, -1), (2, 0), (1, 1), (0, 1), (-1, 0), (-1, -1)): pon(g, cx + dx - (1 if cx == 4 else 0), 12 + dy, 'd')
I['cortar'] = g
g = lienzo(); caja(g, 2, 1, 9, 10, '0', '9'); caja(g, 6, 5, 13, 14, '0', '9'); lineas(g, [(8, 8, 11), (10, 8, 11), (12, 8, 11)], '7'); I['copiar'] = g
g = lienzo(); caja(g, 2, 2, 13, 14, '0', 'a'); caja(g, 6, 1, 9, 3, '0', 'f'); caja(g, 5, 6, 12, 13, '0', '9'); lineas(g, [(8, 7, 10), (10, 7, 10)], '7'); I['pegar'] = g
g = lienzo(); raya(g, 13, 1, 8, 6, '1'); raya(g, 14, 2, 9, 7, '1'); raya(g, 13, 2, 9, 6, '1')
texto(g, ["   0000 ", "  0aaaa0", " 0aaaaa0", "0aaaaa0 ", "0aaaa0  ", "0aaa0   ", " 000    "], 1, 7); I['brocha'] = g
flecha = ["     00000      ", "   00ccccc00    ", "  0cc00000cc0   ", "  0c0     0c0   ", " 0c0       0c0  ", "00c00      0c0  ", "0ccc0      0c0  ", " 0c0       0c0  ", "  0       0c0   ", "         0c0    ", "       00c0     ", "       000      "]
g = lienzo(); texto(g, flecha, 0, 2); I['deshacer'] = g
g = lienzo(); texto(g, [f[::-1] for f in flecha], 0, 2); I['rehacer'] = g
for nom, pat in (('izq', [(2, 12), (2, 8), (2, 12), (2, 6), (2, 12), (2, 9)]), ('cen', [(2, 13), (4, 11), (2, 13), (5, 10), (2, 13), (4, 11)]),
                 ('der', [(3, 13), (7, 13), (3, 13), (9, 13), (3, 13), (6, 13)]), ('just', [(2, 13)] * 6)):
    g = lienzo(); lineas(g, [(2 + 2 * i, a, b) for i, (a, b) in enumerate(pat)]); I[nom] = g
g = lienzo(); lineas(g, [(3, 6, 13), (7, 6, 13), (11, 6, 13)])
texto(g, [" c ", "cc ", " c ", "", "cc ", " cc", "cc ", "", "ccc", " cc", "ccc"], 1, 2); I['numera'] = g
g = lienzo(); lineas(g, [(3, 6, 13), (7, 6, 13), (11, 6, 13)])
for y in (3, 7, 11): caja(g, 2, y - 1, 3, y, 'c')
I['vinetas'] = g
for nom, fl in (('sang_mas', ["c   ", "cc  ", "cccc", "cc  ", "c   "]), ('sang_menos', ["   c", "  cc", "cccc", "  cc", "   c"])):
    g = lienzo(); lineas(g, [(2, 2, 13), (5, 7, 13), (8, 7, 13), (11, 7, 13), (14, 2, 13)]); texto(g, fl, 1, 6); I[nom] = g

# ---- los de las demás aplicaciones ----
g = lienzo(); raya(g, 3, 3, 12, 12, 'd'); raya(g, 4, 3, 13, 12, 'd'); raya(g, 12, 3, 3, 12, 'd'); raya(g, 13, 3, 4, 12, 'd'); I['borrar'] = g
g = lienzo(); lineas(g, [(7, 3, 12), (8, 3, 12)], 'b')
for y in range(3, 13): pon(g, 7, y, 'b'); pon(g, 8, y, 'b')
I['mas'] = g
g = lienzo(); lineas(g, [(7, 3, 12), (8, 3, 12)], 'd'); I['menos'] = g
g = lienzo(); raya(g, 2, 8, 6, 12, 'b'); raya(g, 3, 8, 7, 12, 'b'); raya(g, 6, 12, 13, 3, 'b'); raya(g, 7, 12, 14, 3, 'b'); I['visto'] = g
g = lienzo(); texto(g, ["000000000", "00     0", " 00     ", "  00    ", "   00   ", "  00    ", " 00     ", "00     0", "000000000"], 3, 3); I['sigma'] = g
g = lienzo(); raya(g, 11, 2, 4, 9, 'a'); raya(g, 12, 3, 5, 10, 'a'); raya(g, 13, 4, 6, 11, 'a'); raya(g, 12, 1, 14, 3, 'd'); raya(g, 3, 10, 5, 12); raya(g, 3, 10, 2, 13); raya(g, 2, 13, 5, 12); I['lapiz'] = g
g = lienzo(); caja(g, 3, 1, 12, 14, '0', '9'); lineas(g, [(4, 5, 10), (6, 5, 10), (8, 5, 10), (10, 5, 8)], '7'); I['texto'] = g
g = lienzo()
for dx, dy in ((2,0),(3,0),(4,0),(1,1),(5,1),(0,2),(6,2),(0,3),(6,3),(0,4),(6,4),(1,5),(5,5),(2,6),(3,6),(4,6)): pon(g, 3 + dx, 2 + dy, '0')
raya(g, 9, 9, 13, 13); raya(g, 10, 9, 14, 13); I['buscar'] = g
g = lienzo(); raya(g, 8, 3, 3, 8, 'c'); raya(g, 3, 8, 8, 13, 'c'); lineas(g, [(8, 3, 13)], 'c'); raya(g, 8, 4, 4, 8, 'c'); raya(g, 4, 8, 8, 12, 'c'); I['volver'] = g
g = lienzo(); lineas(g, [(13, 2, 13)]); 
for x, h, c in ((3, 4, 'c'), (6, 8, 'd'), (9, 6, 'b'), (12, 10, 'a')):
    for y in range(13 - h, 13): pon(g, x, y, c); pon(g, x + 1, y, c)
I['grafico'] = g

g = lienzo(); caja(g, 1, 2, 14, 13, '0', '9'); lineas(g, [(5, 2, 13), (8, 2, 13), (11, 2, 13)], '7'); lineas(g, [(3, 2, 13), (4, 2, 13)], 'b')
for y in range(3, 13): pon(g, 5, y, '7'); pon(g, 10, y, '7')
I['hoja'] = g

g = lienzo(); caja(g, 2, 1, 10, 13, '0', '9'); lineas(g, [(4, 4, 8), (6, 4, 8)], '7')
for dx, dy in ((1,0),(2,0),(0,1),(3,1),(0,2),(3,2),(1,3),(2,3)): pon(g, 8 + dx, 7 + dy, 'c')
pon(g, 9, 8, '9'); pon(g, 10, 8, '9'); pon(g, 9, 9, '9'); pon(g, 10, 9, '9'); raya(g, 12, 11, 14, 13, 'c'); raya(g, 12, 12, 13, 13, 'c')
I['prelim'] = g

if __name__ == '__main__':
    import sys
    for nom, g in I.items():
        print('ic_%s:' % nom)
        for f in g:
            print('        db "%s"' % ''.join(f))
