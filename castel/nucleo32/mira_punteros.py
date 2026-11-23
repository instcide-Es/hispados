#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# mira_punteros.py - (PUNTEROS) mira el puntero del ratón en las capturas de
# la parte 51 de castel/pruebas.sh (pruebas_punteros.sh). El puntero es lo
# que cambia entre dos capturas de la misma pantalla: una con él en (X, Y)
# («con») y otra con él lejos («sin»). De ahí, su máscara alrededor del
# punto activo (en un cuadrado de lado 2R + 1) y sus colores. Las órdenes:
#
#   forma CON SIN X Y R [MIN]          hay puntero (al menos MIN puntos; 20)
#   color CON SIN X Y R RRGGBB         el relleno es ese color (±24) y tiene
#                                      un borde que se distingue de él
#   visible CON SIN X Y R              se distingue del fondo (luz ≥ 100)
#   invertido CON SIN X Y R            el relleno es el contrario del fondo
#   escala C1 S1 X1 Y1 C2 S2 X2 Y2 R MIN MAX
#                                      lo alto del segundo / el primero
#   espejo C1 S1 X1 Y1 C2 S2 X2 Y2 R   el segundo es el primero en espejo
#   igual C1 S1 X1 Y1 C2 S2 X2 Y2 R    los dos, iguales
#   distintas R C S X Y [C S X Y...]   todas distintas entre sí (más del 5 %)
#   nitido CON SIN X Y R suave|duro    suave: con puntos de medio tono y sin
#                                      bloques (no es uno pequeño ampliado);
#                                      duro: sin medios tonos (el «Retro»)
#   restos A B [x0 y0 x1 y1 ...]       las dos pantallas, iguales (salvo en
#                                      esos rectángulos)
#   anillos CON SIN X Y                hay anillos lejos del puntero
#
# Sale con 0 si se cumple; si no, con 1 y dice por qué.
import sys


def lee(nombre):
    with open(nombre, 'rb') as f:
        d = f.read()
    partes = []
    i = 0
    while len(partes) < 4:
        while d[i:i + 1].isspace():
            i += 1
        if d[i:i + 1] == b'#':
            while d[i:i + 1] != b'\n':
                i += 1
            continue
        j = i
        while not d[j:j + 1].isspace():
            j += 1
        partes.append(d[i:j])
        i = j
    i += 1
    an, al = int(partes[1]), int(partes[2])
    return an, al, d[i:i + an * al * 3]


cache = {}


def imagen(n):
    if n not in cache:
        cache[n] = lee(n)
    return cache[n]


def px(im, x, y):
    an, al, d = im
    if x < 0 or y < 0 or x >= an or y >= al:
        return None
    k = (y * an + x) * 3
    return (d[k], d[k + 1], d[k + 2])


def luz(c):
    return (c[0] * 77 + c[1] * 150 + c[2] * 29) >> 8


def parche(con, sin, x, y, r):
    """lo que cambia alrededor de (x, y): {(dx, dy): (color con, color sin)}"""
    a, b = imagen(con), imagen(sin)
    m = {}
    for dy in range(-r, r + 1):
        for dx in range(-r, r + 1):
            p, q = px(a, x + dx, y + dy), px(b, x + dx, y + dy)
            if p is not None and q is not None and p != q:
                m[(dx, dy)] = (p, q)
    return m


def caja(m):
    xs = [k[0] for k in m]
    ys = [k[1] for k in m]
    return min(xs), min(ys), max(xs), max(ys)


def cerca(c, d, t):
    return abs(c[0] - d[0]) <= t and abs(c[1] - d[1]) <= t and abs(c[2] - d[2]) <= t


def principales(m):
    """los dos colores del puntero que más salen (el relleno y el borde)"""
    cuenta = {}
    for p, q in m.values():
        cuenta[p] = cuenta.get(p, 0) + 1
    orden = sorted(cuenta.items(), key=lambda kv: -kv[1])
    return orden


def relleno(m):
    """el color del relleno: el que más sale en lo de dentro (lo que queda
    del puntero quitándole un borde de 2 puntos); si no queda nada, el que
    más sale en todo"""
    dentro = [k for k in m if all((k[0] + i, k[1] + j) in m for i in range(-2, 3) for j in range(-2, 3))]
    cuenta = {}
    for k in (dentro or list(m)):
        p = m[k][0]
        cuenta[p] = cuenta.get(p, 0) + 1
    return max(cuenta.items(), key=lambda kv: kv[1])[0]


def falla(t):
    print(t)
    sys.exit(1)


def bien(t):
    print(t)
    sys.exit(0)


def main(a):
    o = a[0]
    if o == 'forma':
        con, sin, x, y, r = a[1], a[2], int(a[3]), int(a[4]), int(a[5])
        mn = int(a[6]) if len(a) > 6 else 20
        m = parche(con, sin, x, y, r)
        if len(m) < mn:
            falla(f'solo {len(m)} puntos de puntero (pide {mn})')
        x0, y0, x1, y1 = caja(m)
        bien(f'{len(m)} puntos, {x1 - x0 + 1} x {y1 - y0 + 1}')
    if o == 'color':
        con, sin, x, y, r, c = a[1], a[2], int(a[3]), int(a[4]), int(a[5]), a[6]
        e = (int(c[0:2], 16), int(c[2:4], 16), int(c[4:6], 16))
        m = parche(con, sin, x, y, r)
        if len(m) < 20:
            falla(f'no hay puntero ({len(m)} puntos)')
        orden = principales(m)
        rel = [k for k, n in orden[:3] if cerca(k, e, 24)] or [k for k in [relleno(m)] if cerca(k, e, 24)]
        if not rel:
            falla('el relleno no es %s: salen %s' % (c, ', '.join('%02X%02X%02X x%d' % (k + (n,)) for k, n in orden[:4])))
        borde = [k for k, n in orden if abs(luz(k) - luz(rel[0])) >= 100 and n >= 6]
        if not borde:
            falla('no tiene un borde que se distinga del relleno')
        bien('relleno %02X%02X%02X, borde %02X%02X%02X' % (rel[0] + borde[0]))
    if o == 'visible':
        con, sin, x, y, r = a[1], a[2], int(a[3]), int(a[4]), int(a[5])
        m = parche(con, sin, x, y, r)
        d = max([abs(luz(p) - luz(q)) for p, q in m.values()] + [0])
        if d < 100:
            falla(f'no se distingue del fondo (la mayor diferencia de luz: {d})')
        bien(f'se distingue del fondo (diferencia de luz {d})')
    if o == 'invertido':
        con, sin, x, y, r = a[1], a[2], int(a[3]), int(a[4]), int(a[5])
        m = parche(con, sin, x, y, r)
        if len(m) < 20:
            falla('no hay puntero')
        fondo = luz(px(imagen(sin), x + 3, y + 6))
        rel = relleno(m)
        if fondo >= 128 and luz(rel) > 100:
            falla(f'sobre claro (luz {fondo}) el relleno no es oscuro (luz {luz(rel)})')
        if fondo < 128 and luz(rel) < 155:
            falla(f'sobre oscuro (luz {fondo}) el relleno no es claro (luz {luz(rel)})')
        bien(f'el fondo, de luz {fondo}; el relleno, de luz {luz(rel)}')
    if o in ('escala', 'espejo', 'igual'):
        m1 = parche(a[1], a[2], int(a[3]), int(a[4]), int(a[9]))
        m2 = parche(a[5], a[6], int(a[7]), int(a[8]), int(a[9]))
        if len(m1) < 20 or len(m2) < 20:
            falla(f'falta puntero ({len(m1)} y {len(m2)} puntos)')
        if o == 'escala':
            b1, b2 = caja(m1), caja(m2)
            h1, h2 = b1[3] - b1[1] + 1, b2[3] - b2[1] + 1
            q = h2 / h1
            if not float(a[10]) <= q <= float(a[11]):
                falla(f'de {h1} a {h2} puntos de alto: {q:.2f} veces (pide de {a[10]} a {a[11]})')
            bien(f'de {h1} a {h2} puntos de alto: {q:.2f} veces')
        s1 = set(m1)
        s2 = set((-dx, dy) for dx, dy in m2) if o == 'espejo' else set(m2)
        dif = len(s1 ^ s2)
        tope = max(4, (len(s1) + len(s2)) // 50)
        if dif > tope:
            falla(f'{dif} puntos distintos (de {len(s1)} y {len(s2)}; tope {tope})')
        bien(f'{len(s1)} y {len(s2)} puntos, {dif} distintos')
    if o == 'distintas':
        r = int(a[1])
        ms = []
        for i in range(2, len(a), 4):
            ms.append(set(parche(a[i], a[i + 1], int(a[i + 2]), int(a[i + 3]), r)))
        for i in range(len(ms)):
            if len(ms[i]) < 20:
                falla(f'la {i + 1}.ª no tiene puntero ({len(ms[i])} puntos)')
            for j in range(i + 1, len(ms)):
                if len(ms[i] ^ ms[j]) * 20 < len(ms[i]) + len(ms[j]):
                    falla(f'la {i + 1}.ª y la {j + 1}.ª se parecen demasiado')
        bien(f'las {len(ms)}, distintas entre sí')
    if o == 'nitido':
        con, sin, x, y, r, modo = a[1], a[2], int(a[3]), int(a[4]), int(a[5]), a[6]
        m = parche(con, sin, x, y, r)
        if len(m) < 20:
            falla('no hay puntero')
        orden = principales(m)
        c1 = orden[0][0]
        c2 = next((k for k, n in orden[1:] if abs(luz(k) - luz(c1)) >= 60), orden[min(1, len(orden) - 1)][0])
        medios = [k for k, (p, q) in m.items() if not cerca(p, c1, 12) and not cerca(p, c2, 12) and not cerca(p, q, 12)]
        if modo == 'duro':
            if len(medios) > len(m) // 50:
                falla(f'{len(medios)} puntos de medio tono (el «Retro» no suaviza)')
            bien(f'sin medios tonos ({len(medios)} de {len(m)})')
        if len(medios) < 12:
            falla(f'solo {len(medios)} puntos de medio tono: no está suavizado')
        # ¿bloques de 3 x 3 (uno pequeño ampliado)? con cualquier alineación
        pocos = None
        for ox in range(3):
            for oy in range(3):
                partidos = 0
                bloques = {}
                for (dx, dy), (p, q) in m.items():
                    k = ((dx + ox) // 3, (dy + oy) // 3)
                    bloques.setdefault(k, set()).add(p)
                for k, cs in bloques.items():
                    n = sum(1 for i in range(3) for j in range(3) if (k[0] * 3 - ox + i, k[1] * 3 - oy + j) in m)
                    if len(cs) > 1 or n not in (0, 9):
                        partidos += 1
                pocos = partidos if pocos is None else min(pocos, partidos)
        if pocos < 10:
            falla(f'parece ampliado a bloques de 3 x 3 ({pocos} bloques partidos)')
        bien(f'{len(medios)} puntos de medio tono y {pocos} bloques de 3 x 3 partidos: nítido, no ampliado')
    if o == 'restos':
        A, B = imagen(a[1]), imagen(a[2])
        fuera = [tuple(int(v) for v in a[i:i + 4]) for i in range(3, len(a) - 3, 4)]
        if A[0] != B[0] or A[1] != B[1]:
            falla('no miden lo mismo')
        an, al = A[0], A[1]
        n = 0
        ej = None
        for y in range(al):
            fila = slice(y * an * 3, (y + 1) * an * 3)
            if A[2][fila] == B[2][fila]:
                continue
            for x in range(an):
                k = (y * an + x) * 3
                if A[2][k:k + 3] != B[2][k:k + 3] and not any(x0 <= x < x1 and y0 <= y < y1 for x0, y0, x1, y1 in fuera):
                    n += 1
                    ej = ej or (x, y)
        if n:
            falla(f'{n} puntos distintos (el primero, en {ej})')
        bien('iguales punto a punto')
    if o == 'anillos':
        con, sin, x, y = a[1], a[2], int(a[3]), int(a[4])
        m = parche(con, sin, x, y, 200)
        lejos = [k for k in m if k[0] * k[0] + k[1] * k[1] > 70 * 70 or k[1] < -12 or k[0] < -12]
        if len(lejos) < 40:
            falla(f'no hay anillos ({len(lejos)} puntos lejos del puntero)')
        bien(f'{len(lejos)} puntos de los anillos')
    falla('orden desconocida: ' + o)


if __name__ == '__main__':
    main(sys.argv[1:])
