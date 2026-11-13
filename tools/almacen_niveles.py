#!/usr/bin/env python3
"""Comprueba los niveles de Almacén (juegos/almacen.asm) y los resuelve.

uso: almacen_niveles.py [FUENTE.asm] [-n NIVEL] [-s] [--lejos NIVEL]

Lee los niveles del propio fuente (entre «niveles:» y «niveles_fin:»; cada
fila es un «db "...", 0» y un «db 0» suelto acaba el nivel) y, de cada uno:
 - comprueba que está bien formado: letras conocidas, un solo mozo, tantas
   cajas como marcas (y alguna fuera de su marca), cabe en el tablero y está
   cerrado (el mozo y las cajas no pueden salir del muro);
 - busca una solución con el menor número de empujones (búsqueda en anchura
   sobre el estado «cajas + zona del mozo», sin las casillas muertas: las
   que no dejan llevar una caja a ninguna marca) y escribe cuántos empujones
   y movimientos tiene. Con -s escribe también la solución (a d w s:
   izquierda, derecha, arriba, abajo).
Sale con 1 si algún nivel está mal o no tiene solución.

--lejos N: ayuda para diseñar. Con los muros y las marcas del nivel N, busca
hacia atrás (tirando de las cajas desde las marcas) la colocación de cajas y
mozo que más empujones necesita, y la escribe.

Las letras:  H muro   . suelo   x marca   c caja   C caja en su marca
             a mozo   A mozo sobre una marca      (espacio) fuera
Checks and solves the levels of the Almacén game.
"""
import os, re, sys
from collections import deque

RAIZ = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
MAXC, MAXF = 16, 11                    # como en almacen.asm
TOPE = 3000000                         # estados, como mucho
DIRS = ((-1, 0, 'a'), (1, 0, 'd'), (0, -1, 'w'), (0, 1, 's'))


def leer(fuente):
    niveles, filas, dentro = [], [], False
    for l in open(fuente, encoding='utf-8'):
        s = l.strip()
        if s.startswith('niveles:'):
            dentro = True
            continue
        if s.startswith('niveles_fin:'):
            break
        if not dentro:
            continue
        m = re.match(r'db\s+"([^"]*)"\s*,\s*0', s)
        if m:
            filas.append(m.group(1))
        elif re.match(r'db\s+0\b', s):
            niveles.append(filas)
            filas = []
    return niveles


class Nivel:
    def __init__(self, filas):
        self.filas = filas
        self.h = len(filas)
        self.w = max(len(f) for f in filas) if filas else 0
        self.muros, self.suelo, self.marcas, self.cajas = set(), set(), set(), set()
        self.mozos = []
        self.malas = []
        for y, f in enumerate(filas):
            for x, c in enumerate(f):
                p = (x, y)
                if c == 'H':
                    self.muros.add(p)
                elif c == ' ':
                    pass
                elif c in '.xcCaA':
                    self.suelo.add(p)
                    if c in 'xCA':
                        self.marcas.add(p)
                    if c in 'cC':
                        self.cajas.add(p)
                    if c in 'aA':
                        self.mozos.append(p)
                else:
                    self.malas.append(c)

    def errores(self):
        e = []
        if self.malas:
            e.append('letras desconocidas: ' + ''.join(sorted(set(self.malas))))
        if self.w > MAXC or self.h > MAXF:
            e.append('no cabe (%dx%d, máximo %dx%d)' % (self.w, self.h, MAXC, MAXF))
        if len(self.mozos) != 1:
            e.append('tiene que haber un mozo (hay %d)' % len(self.mozos))
        if len(self.cajas) != len(self.marcas):
            e.append('%d cajas y %d marcas' % (len(self.cajas), len(self.marcas)))
        if not self.cajas:
            e.append('sin cajas')
        if self.cajas == self.marcas:
            e.append('ya está resuelto')
        for (x, y) in self.suelo:               # cerrado: el suelo solo toca suelo o muro
            for dx, dy, _ in DIRS:
                q = (x + dx, y + dy)
                if q not in self.suelo and q not in self.muros:
                    e.append('abierto en %d,%d' % q)
        return e

    def zona(self, mozo, cajas):
        """Las casillas a las que llega el mozo sin empujar."""
        vis = {mozo}
        cola = [mozo]
        while cola:
            x, y = cola.pop()
            for dx, dy, _ in DIRS:
                q = (x + dx, y + dy)
                if q in self.suelo and q not in cajas and q not in vis:
                    vis.add(q)
                    cola.append(q)
        return vis

    def vivas(self):
        """Casillas desde las que una caja sola puede llegar a alguna marca."""
        vis = set(self.marcas)
        cola = list(self.marcas)
        while cola:
            x, y = cola.pop()
            for dx, dy, _ in DIRS:              # tirar: la caja va a q y el mozo a r
                q = (x + dx, y + dy)
                r = (x + 2 * dx, y + 2 * dy)
                if q in self.suelo and r in self.suelo and q not in vis:
                    vis.add(q)
                    cola.append(q)
        return vis

    def camino(self, a, b, cajas):
        """Los pasos del mozo de a a b sin empujar (el más corto)."""
        if a == b:
            return ''
        ant = {a: None}
        cola = deque([a])
        while cola:
            p = cola.popleft()
            for dx, dy, l in DIRS:
                q = (p[0] + dx, p[1] + dy)
                if q in self.suelo and q not in cajas and q not in ant:
                    ant[q] = (p, l)
                    if q == b:
                        s = ''
                        while ant[q]:
                            q, l2 = ant[q]
                            s = l2 + s
                        return s
                    cola.append(q)
        raise AssertionError('sin camino')

    def resolver(self):
        """-> (empujones, solución) con el menor número de empujones, o None."""
        vivas = self.vivas()
        cajas = frozenset(self.cajas)
        if not cajas <= vivas:
            return None
        meta = frozenset(self.marcas)
        z = self.zona(self.mozos[0], cajas)
        ini = (cajas, min(z))
        ant = {ini: None}
        cola = deque([(cajas, z, ini)])
        fin = None
        while cola and fin is None:
            cajas, z, clave = cola.popleft()
            for b in cajas:
                for dx, dy, l in DIRS:
                    p = (b[0] - dx, b[1] - dy)          # donde se pone el mozo
                    q = (b[0] + dx, b[1] + dy)          # a donde va la caja
                    if p not in z or q not in vivas or q in cajas:
                        continue
                    nc = frozenset(cajas - {b} | {q})
                    nz = self.zona(b, nc)
                    nclave = (nc, min(nz))
                    if nclave in ant:
                        continue
                    ant[nclave] = (clave, b, l)
                    if nc == meta:
                        fin = nclave
                        break
                    if len(ant) > TOPE:
                        return None
                    cola.append((nc, nz, nclave))
                if fin:
                    break
        if fin is None:
            return None
        emp = []                                        # los empujones, del primero al último
        k = fin
        while ant[k]:
            k, b, l = ant[k]
            emp.append((k[0], b, l))
        emp.reverse()
        sol, mozo = '', self.mozos[0]
        for cajas, b, l in emp:
            dx, dy = next((d[0], d[1]) for d in DIRS if d[2] == l)
            sol += self.camino(mozo, (b[0] - dx, b[1] - dy), cajas) + l
            mozo = b
        return len(emp), sol

    def lejos(self):
        """La colocación (cajas, mozo) más lejana tirando desde las marcas."""
        meta = frozenset(self.marcas)
        libres = self.suelo - meta
        vis, cola = {}, deque()
        for p in libres:                                # el mozo, en cada zona
            z = self.zona(p, meta)
            k = (meta, min(z))
            if k not in vis:
                vis[k] = 0
                cola.append((meta, z, k))
        ult = None
        while cola:
            cajas, z, k = cola.popleft()
            ult = (cajas, z, k)
            for b in cajas:
                for dx, dy, _ in DIRS:
                    p = (b[0] + dx, b[1] + dy)          # el mozo, al lado
                    r = (b[0] + 2 * dx, b[1] + 2 * dy)  # y a donde retrocede
                    if p not in z or r not in self.suelo or r in cajas:
                        continue
                    nc = frozenset(cajas - {b} | {p})
                    nz = self.zona(r, nc)
                    nk = (nc, min(nz))
                    if nk in vis:
                        continue
                    vis[nk] = vis[k] + 1
                    if len(vis) > TOPE:
                        return ult, vis[ult[2]]
                    cola.append((nc, nz, nk))
        return ult, vis[ult[2]]

    def pintar(self, cajas, mozo):
        for y in range(self.h):
            s = ''
            for x in range(len(self.filas[y])):
                p = (x, y)
                if p in self.muros:
                    s += 'H'
                elif p not in self.suelo:
                    s += ' '
                elif p in cajas:
                    s += 'C' if p in self.marcas else 'c'
                elif p == mozo:
                    s += 'A' if p in self.marcas else 'a'
                else:
                    s += 'x' if p in self.marcas else '.'
            print('        db "%s", 0' % s)
        print('        db 0')


def main():
    a = sys.argv[1:]
    fuente = os.path.join(RAIZ, 'juegos', 'almacen.asm')
    solo, ver, lejos = None, False, None
    i = 0
    while i < len(a):
        if a[i] == '-n':
            solo = int(a[i + 1])
            i += 2
        elif a[i] == '-s':
            ver = True
            i += 1
        elif a[i] == '--lejos':
            lejos = int(a[i + 1])
            i += 2
        else:
            fuente = a[i]
            i += 1
    niveles = [Nivel(f) for f in leer(fuente)]
    if lejos:
        n = niveles[lejos - 1]
        (cajas, z, k), d = n.lejos()
        print('; nivel %d: a %d empujones' % (lejos, d))
        mozo = max(z, key=lambda p: min(abs(p[0] - b[0]) + abs(p[1] - b[1]) for b in cajas))
        n.pintar(cajas, mozo)
        return 0
    mal = 0
    for i, n in enumerate(niveles, 1):
        if solo and i != solo:
            continue
        e = n.errores()
        if e:
            print('nivel %2d: MAL: %s' % (i, '; '.join(e)))
            mal += 1
            continue
        r = n.resolver()
        if not r:
            print('nivel %2d: %2dx%-2d %d cajas: SIN SOLUCIÓN (o demasiado grande)' % (i, n.w, n.h, len(n.cajas)))
            mal += 1
            continue
        print('nivel %2d: %2dx%-2d %d cajas: %3d empujones, %3d movimientos%s' % (
            i, n.w, n.h, len(n.cajas), r[0], len(r[1]), '  ' + r[1] if ver else ''))
    print('%d niveles%s' % (len(niveles), ', %d mal' % mal if mal else ', todos con solución'))
    return 1 if mal else 0


if __name__ == '__main__':
    sys.exit(main())
