#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""nucleo32/mira_iconos.py SALIDA MOTIVO ANCHO ALTO [COMPROBACIÓN...]

(ICONOS) Mira dónde ha puesto el escritorio de Hispa-Pentium los iconos del
tapiz: la línea «escritorio: iconos (MOTIVO): ...» que sale por el puerto
serie (la última con ese motivo; «MOTIVO#2», la segunda). Siempre comprueba
que ninguno se sale de la pantalla de ANCHO x ALTO ni toca la barra de
tareas (34 puntos) ni el dock (62 más, si se ve), y que no hay dos uno
encima de otro (en la misma página). Más comprobaciones:

  n N                  son N iconos
  sindock              el dock no se ve (no se le deja su sitio)
  paginas N            van en N páginas
  orden A,B,C...       de arriba abajo y columna a columna, los primeros son esos
  por CLAVE DATOS      en el orden de CLAVE (nombre, tipo, tamano, fecha), con
                       DATOS un archivo «nombre|familia|ext|tamaño|fecha» por
                       icono (familia: 0 programa, 1 carpeta, 2 documento,
                       3 acceso; fecha: un número, mayor lo más nuevo; «-» si
                       no se sabe: no se mira el orden entre esos)
  en NOMBRE X Y        ese, en ese punto
  rejilla              todos en las celdas de la rejilla
  fuera NOMBRE X Y     ese, NO en ese punto

Acaba con 0 si todo está bien; si no, dice qué falla y acaba con 1.
"""
import re
import sys

BARRA = 34
DOCK = 62
CELDA_AN = 112
CELDA_AL = 62


def lee(salida, motivo):
    veces = 0
    if '#' in motivo:
        motivo, veces = motivo.split('#')
        veces = int(veces)
    lineas = []
    for l in open(salida, 'rb').read().decode('utf-8', 'replace').splitlines():
        m = re.search(r'escritorio: iconos \((.*?)\): (\d+) en la zona (-?\d+),(-?\d+)-(-?\d+),(-?\d+), celdas de (\d+) x (\d+), (\d+) x (\d+), (\d+) página\(s\), orden (\d+):(.*)$', l)
        if m and m.group(1) == motivo:
            lineas.append(m)
    if not lineas:
        return None
    m = lineas[veces - 1] if veces else lineas[-1]
    iconos = []
    for t in m.group(13).split(';'):
        t = t.strip()
        if not t:
            continue
        mm = re.match(r'(.*)@(-?\d+),(-?\d+)/(\d+)$', t)
        iconos.append((mm.group(1), int(mm.group(2)), int(mm.group(3)), int(mm.group(4))))
    zona = tuple(int(m.group(i)) for i in range(3, 7))
    return dict(n=int(m.group(2)), zona=zona, paso=(int(m.group(7)), int(m.group(8))), cr=(int(m.group(9)), int(m.group(10))),
                paginas=int(m.group(11)), orden=int(m.group(12)), iconos=iconos)


def clave_letra(c):
    return c.upper()


def main():
    salida, motivo, an, al = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
    args = sys.argv[5:]
    d = lee(salida, motivo)
    if d is None:
        print(f"no hay ninguna línea «escritorio: iconos ({motivo})»")
        return 1
    mal = []
    abajo = BARRA + DOCK
    if 'sindock' in args:
        abajo = BARRA
    pa, pl = d['paso']
    ic = d['iconos']
    if len(ic) != d['n']:
        mal.append(f"dice {d['n']} y van {len(ic)}")
    for (nom, x, y, p) in ic:
        if x < 0 or y < 0 or x + CELDA_AN > an or y + CELDA_AL > al - abajo:
            mal.append(f"«{nom}» en {x},{y}: se sale (o toca la barra o el dock: lo libre acaba en {al - abajo})")
    for i in range(len(ic)):
        for j in range(i + 1, len(ic)):
            a, b = ic[i], ic[j]
            if a[3] == b[3] and abs(a[1] - b[1]) < pa and abs(a[2] - b[2]) < CELDA_AL:
                mal.append(f"«{a[0]}» ({a[1]},{a[2]}) y «{b[0]}» ({b[1]},{b[2]}), uno encima de otro")
    k = 0
    while k < len(args):
        a = args[k]
        if a == 'n':
            if d['n'] != int(args[k + 1]):
                mal.append(f"son {d['n']} y tenían que ser {args[k + 1]}")
            k += 2
        elif a == 'sindock':
            k += 1
        elif a == 'paginas':
            if d['paginas'] != int(args[k + 1]):
                mal.append(f"van en {d['paginas']} páginas y tenían que ser {args[k + 1]}")
            k += 2
        elif a == 'rejilla':
            x0, y0 = d['zona'][0], d['zona'][1]
            for (nom, x, y, p) in ic:
                if (x - x0) % pa or (y - y0) % pl:
                    mal.append(f"«{nom}» en {x},{y}: fuera de la rejilla")
            k += 1
        elif a in ('en', 'fuera'):
            nom, x, y = args[k + 1], int(args[k + 2]), int(args[k + 3])
            hay = [t for t in ic if t[0] == nom]
            if not hay:
                mal.append(f"no está «{nom}»")
            elif a == 'en' and (hay[0][1], hay[0][2]) != (x, y):
                mal.append(f"«{nom}» está en {hay[0][1]},{hay[0][2]} y no en {x},{y}")
            elif a == 'fuera' and (hay[0][1], hay[0][2]) == (x, y):
                mal.append(f"«{nom}» sigue en {x},{y}")
            k += 4
        elif a == 'orden':
            quiere = args[k + 1].split(',')
            hay = [t[0] for t in sorted(ic, key=lambda t: (t[3], t[1], t[2]))]
            if hay[:len(quiere)] != quiere:
                mal.append(f"el orden es {', '.join(hay[:len(quiere)])} y tenía que ser {', '.join(quiere)}")
            k += 2
        elif a == 'por':
            clave, datos = args[k + 1], args[k + 2]
            info = {}
            for l in open(datos, encoding='utf-8'):
                c = l.rstrip('\n').split('|')
                if len(c) == 5:
                    info[c[0]] = (int(c[1]), c[2], int(c[3]), None if c[4] == '-' else int(c[4]))
            hay = [t[0] for t in sorted(ic, key=lambda t: (t[3], t[1], t[2]))]
            if hay and hay[0] != 'Mi equipo':
                mal.append(f"el primero es «{hay[0]}» y no Mi equipo")
            resto = [h for h in hay if h != 'Mi equipo']
            falta = [h for h in resto if h not in info]
            if falta:
                mal.append(f"sin datos de {', '.join(falta)}")
            else:
                for a1, b1 in zip(resto, resto[1:]):
                    fa, ea, ta, da = info[a1]
                    fb, eb, tb, db = info[b1]
                    na, nb = a1.upper(), b1.upper()
                    if clave == 'nombre':
                        bien = na <= nb
                    elif clave == 'tipo':
                        ka = (fa, ea if fa == 2 else '', na)
                        kb = (fb, eb if fb == 2 else '', nb)
                        bien = ka <= kb
                    elif clave == 'tamano':
                        bien = (ta, na) <= (tb, nb)
                    else:
                        if da is None and db is None:
                            bien = True
                        elif da is None:
                            bien = True
                        elif db is None:
                            bien = False
                        else:
                            bien = (-da, na) <= (-db, nb)
                    if not bien:
                        mal.append(f"por {clave}: «{a1}» va antes que «{b1}»")
            k += 3
        else:
            print(f"no sé qué es «{a}»")
            return 2
    if mal:
        for m in mal[:8]:
            print(m)
        return 1
    return 0


sys.exit(main())
