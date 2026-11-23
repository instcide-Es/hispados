#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
#
# tpsintetico.py - (TOUCHPAD) paquetes de touchpad Synaptics hechos a mano
# (los de seis octetos del modo absoluto con W) para probar touchpad.cas sin
# un touchpad: la parte 50 de castel/pruebas.sh (pruebas_touchpad.sh).
#
#   tpsintetico.py prueba RATON.BIN CARPETA
#       hace cada caso (CARPETA/<caso>.tpd), lo pasa por «RATON /ANALIZA» con
#       corre32 y dice «bien: ...» o «mal: ...» por cada uno
#   tpsintetico.py inyecta CARPETA
#       deja los archivos para RATON /INYECTA en QEMU (con «tp_simulado»):
#       BORDE.TPD (el borde derecho, de arriba abajo) y DOSDEDOS.TPD
#
# Los archivos son los que lee RATON (uraton.cas): «@n», «+n», «o HH ...»,
# «tecla», «aj nombre valor» y «?».
import os
import re
import subprocess
import sys

XMIN, XMAX, YMIN, YMAX = 1472, 5472, 1408, 4448


def paquete(x, y, z, w=4, izq=0, der=0, ext=0):
    """Los seis octetos de un paquete (W: 4 a 15, un dedo; 0, dos; 1, tres)."""
    b0 = 0x80 | ((w & 0x0C) << 2) | ((w & 0x02) << 1) | izq | (der << 1)
    b1 = (((y >> 8) & 0x0F) << 4) | ((x >> 8) & 0x0F)
    b3 = 0xC0 | (((y >> 12) & 1) << 5) | (((x >> 12) & 1) << 4) | ((w & 1) << 2) | ((izq | (der << 1)) ^ ext)
    return [b0, b1, z & 0xFF, b3, x & 0xFF, y & 0xFF]


class Guion:
    def __init__(self):
        self.l = []

    def aj(self, n, v):
        self.l.append('aj %s %d' % (n, v))
        return self

    def o(self, octs):
        self.l.append('o ' + ' '.join('%02X' % b for b in octs))
        return self

    def mas(self, n):
        self.l.append('+%d' % n)
        return self

    def p(self, x, y, z=60, w=4, izq=0, der=0, ext=0, tics=1):
        self.o(paquete(x, y, z, w, izq, der, ext))
        if tics:
            self.mas(tics)
        return self

    def suelta(self, x=0, y=0, n=3):
        for _ in range(n):
            self.p(x, y, 0)
        return self

    def traza(self, x0, y0, x1, y1, n, z=60, w=4, izq=0):
        """El dedo de (x0, y0) a (x1, y1) en n paquetes (uno por tic)."""
        for i in range(n + 1):
            self.p(x0 + (x1 - x0) * i // n, y0 + (y1 - y0) * i // n, z, w, izq)
        return self

    def toque(self, x=3500, y=3000, n=4, w=4):
        for _ in range(n):
            self.p(x, y, 60, w)
        self.suelta(x, y, 1)
        return self

    def tecla(self):
        self.l.append('tecla')
        return self

    def q(self):
        self.l.append('?')
        return self

    def texto(self):
        return '# hecho por tpsintetico.py\r\n' + '\r\n'.join(self.l) + '\r\n'


def casos():
    """(nombre, qué es, guion, comprobación(resumen, botones))."""
    c = []

    def caso(n, que, g, mira):
        c.append((n, que, g, mira))

    g = Guion().traza(3000, 3000, 4000, 3000, 25).suelta().mas(40)
    caso('mover', 'mover a la derecha: la flecha va a la derecha (y nada más)', g,
         lambda r, b: r['dx'] > 40 and abs(r['dy']) <= 2 and r['rueda'] == 0 and not b and r['toques'] == 0)

    g = Guion().traza(3000, 2000, 3000, 3500, 25).suelta().mas(40)
    caso('arriba', 'mover hacia arriba (Y del touchpad crece): la flecha sube', g,
         lambda r, b: r['dy'] < -40 and abs(r['dx']) <= 2 and not b)

    lento = Guion().traza(3000, 3000, 3400, 3000, 100).suelta().mas(40)
    rapido = Guion().traza(3000, 3000, 3400, 3000, 5).suelta().mas(40)
    c.append(('acelera', 'la aceleración: lo mismo recorrido deprisa mueve más la flecha que despacio', (lento, rapido), None))

    g = Guion().aj('vel', 10).traza(3000, 3000, 4000, 3000, 25).suelta().mas(40)
    caso('velocidad', 'la velocidad del touchpad a 10: más que a 5', g, lambda r, b: r['dx'] > 100)

    g = Guion().toque().mas(40)
    caso('toque', 'tocar: un clic (el botón se pulsa y, a los 180 ms sin otro toque, se suelta)', g,
         lambda r, b: [v for _, v in b] == [1, 0] and r['toques'] == 1 and r['botones'] == 0 and 15 <= b[1][0] - b[0][0] <= 20)

    g = Guion().aj('toque', 0).toque().mas(40)
    caso('sintoque', 'tocar con «tocar para hacer clic» quitado: nada', g, lambda r, b: not b and r['toques'] == 0)

    g = Guion().toque().mas(6).toque().mas(40)
    caso('doble', 'dos toques seguidos: doble clic (pulsa, suelta, pulsa, suelta)', g,
         lambda r, b: [v for _, v in b] == [1, 0, 1, 0] and r['toques'] == 2)

    g = Guion().toque().mas(6).traza(3000, 3000, 3600, 3000, 20).suelta().mas(40)
    caso('arrastra', 'tocar y, enseguida, tocar y mover: arrastrar (el botón, pulsado mientras se mueve; se suelta al levantar)', g,
         lambda r, b: [v for _, v in b] == [1, 0] and r['dx'] > 20 and b[1][0] - b[0][0] >= 20)

    g = Guion().toque(n=30).mas(40)
    caso('largo', 'un dedo quieto mucho rato (no es un toque): nada', g, lambda r, b: not b)

    g = Guion().aj('dedos', 1).toque(w=0).mas(40)
    caso('toque2', 'tocar con dos dedos: el botón derecho', g, lambda r, b: [v for _, v in b] == [2, 0])

    g = Guion().aj('dedos', 1).toque(w=1).mas(40)
    caso('toque3', 'tocar con tres dedos: el del medio', g, lambda r, b: [v for _, v in b] == [4, 0])

    zona = XMAX - (XMAX - XMIN) * 8 // 100
    g = Guion().traza(5400, 4200, 5400, 1700, 50).suelta().mas(40).q()
    caso('borde', 'el borde derecho, de arriba abajo: la rueda hacia abajo, tantos pasos como lo recorrido (un paso, 1/20 del alto), sin mover la flecha', g,
         lambda r, b: r['rueda'] == -((4200 - 1700) // ((YMAX - YMIN) // 20)) and r['dx'] == 0 and r['dy'] == 0 and not b)

    g = Guion().traza(5400, 1700, 5400, 2700, 30).suelta().mas(40)
    caso('borde_sube', 'el borde derecho, hacia arriba: la rueda hacia arriba', g,
         lambda r, b: r['rueda'] == 1000 // ((YMAX - YMIN) // 20) and r['dx'] == 0 and r['dy'] == 0)

    g = Guion().traza(zona - 60, 4200, zona - 60, 1700, 50).suelta().mas(40)
    caso('fuera_borde', 'justo antes de la zona del borde (el 8 % derecho): mueve la flecha', g,
         lambda r, b: r['rueda'] == 0 and r['dy'] > 40)

    g = Guion().aj('zona', 15).traza(zona - 60, 4200, zona - 60, 1700, 50).suelta().mas(40)
    caso('zona', 'con la zona al 15 %: ahí ya es el borde', g, lambda r, b: r['rueda'] < -10 and r['dy'] == 0)

    g = Guion().aj('borde_v', 0).traza(5400, 4200, 5400, 1700, 50).suelta().mas(40)
    caso('sinborde', 'con el borde quitado: el borde derecho mueve la flecha', g, lambda r, b: r['rueda'] == 0 and r['dy'] > 40)

    g = Guion().traza(5400, 4200, 5400, 4150, 3).suelta(5400, 4150, 1).mas(40)
    caso('borde_toque', 'un toque en el borde: un clic (no la rueda)', g, lambda r, b: [v for _, v in b] == [1, 0] and r['rueda'] == 0)

    g = Guion().aj('borde_h', 1).traza(2000, 1500, 4500, 1500, 40).suelta().mas(40)
    caso('abajo', 'el borde de abajo (si se pone): a los lados, a la derecha', g,
         lambda r, b: r['lados'] == (4500 - 2000) // ((XMAX - XMIN) // 20) and r['dx'] == 0 and r['rueda'] == 0)

    g = Guion().traza(2000, 1500, 4500, 1500, 40).suelta().mas(40)
    caso('sinabajo', 'el borde de abajo, como sale (quitado): mueve la flecha', g, lambda r, b: r['lados'] == 0 and r['dx'] > 40)

    g = Guion().aj('dedos', 1).traza(3000, 4000, 3000, 2000, 40, w=0).suelta().mas(40)
    caso('dos', 'dos dedos hacia abajo: la rueda hacia abajo, sin mover la flecha', g,
         lambda r, b: r['rueda'] == -(2000 // ((YMAX - YMIN) // 20)) and r['dx'] == 0 and r['dy'] == 0 and not b)

    g = Guion().aj('dedos', 1).traza(2000, 3000, 4000, 3100, 40, w=0).suelta().mas(40)
    caso('dos_lados', 'dos dedos a la derecha (y algo hacia arriba): a los lados y nada más (el primero que sale manda)', g,
         lambda r, b: r['lados'] == 2000 // ((XMAX - XMIN) // 20) and r['rueda'] == 0)

    g = Guion().aj('dedos', 1).traza(3000, 3000, 3500, 3000, 10).traza(3500, 3000, 3500, 2000, 30, w=0).suelta().mas(40)
    caso('uno_dos', 'un dedo que mueve y luego otro: de mover a desplazar (sin saltos de la flecha)', g,
         lambda r, b: 10 < r['dx'] < 400 and r['rueda'] < -3 and abs(r['dy']) <= 2)

    g = Guion().aj('dedos', 0).traza(3000, 4000, 3000, 2000, 40, w=0).suelta().mas(40)
    caso('dos_sin', 'W = 0 sin «varios dedos» (el touchpad no lo dice): es un dedo', g, lambda r, b: r['rueda'] == 0 and r['dy'] > 40)

    g = Guion().aj('dedos', 1).aj('dos', 0).traza(3000, 4000, 3000, 2000, 40, w=0).suelta().mas(40)
    caso('dos_quitado', 'dos dedos con «dos dedos» quitado: mueve la flecha', g, lambda r, b: r['rueda'] == 0 and r['dy'] > 40)

    g = Guion().toque(w=14).mas(40).traza(3000, 3000, 4000, 3000, 25, w=14).suelta().mas(40)
    caso('palma', 'la palma (W = 14): ni clic ni mueve', g, lambda r, b: not b and r['dx'] == 0 and r['palmas'] == 2)

    g = Guion().traza(3000, 3000, 4000, 3000, 25, z=230).suelta().mas(40)
    caso('presion', 'mucha presión (Z = 230, la palma): no mueve', g, lambda r, b: r['dx'] == 0 and r['palmas'] == 1)

    g = Guion().tecla().mas(10).toque().mas(40).tecla().mas(60).toque().mas(40)
    caso('escribir', 'tocar justo después de una tecla: nada (se escribe); al segundo: el clic', g,
         lambda r, b: [v for _, v in b] == [1, 0] and r['ignorados'] == 1)

    g = Guion().aj('pausa', 0).tecla().mas(10).toque().mas(40)
    caso('escrino', 'con «al escribir» quitado: el toque vale enseguida', g, lambda r, b: [v for _, v in b] == [1, 0])

    g = Guion().p(0, 0, 0, izq=1).mas(5).p(0, 0, 0).mas(5).p(0, 0, 0, der=1).mas(3).p(0, 0, 0).mas(20)
    caso('botones', 'los botones de verdad: izquierdo y derecho', g, lambda r, b: [v for _, v in b] == [1, 0, 2, 0])

    g = Guion()
    for _ in range(4):
        g.p(3500, 3000, 60, izq=1)
    g.p(3500, 3000, 0).mas(40)
    caso('boton_dedo', 'el botón de verdad pulsado mientras se toca: no hay clic de toque', g,
         lambda r, b: [v for _, v in b] == [1, 0] and r['toques'] == 0)

    g = Guion().aj('clickpad', 1).p(3000, 3000, 60).p(3000, 3000, 60, ext=1).p(3000, 3000, 60, ext=1).p(3000, 3000, 60).p(3000, 3000, 0).mas(40)
    caso('clickpad', 'un clickpad (todo el touchpad es el botón): el izquierdo', g, lambda r, b: [v for _, v in b] == [1, 0])

    g = Guion().aj('clickpad', 1).p(5000, 1600, 60).p(5000, 1600, 60, ext=1).p(5000, 1600, 60).p(5000, 1600, 0).mas(40)
    caso('clickder', 'el clickpad, abajo a la derecha: el derecho', g, lambda r, b: [v for _, v in b] == [2, 0])

    g = Guion().o([0x12, 0x34]).traza(3000, 3000, 4000, 3000, 25).suelta().mas(40)
    caso('basura', 'dos octetos de más al principio (descolocado): se tiran y lo demás va', g,
         lambda r, b: r['malos'] == 2 and r['dx'] > 40 and r['paquetes'] >= 26)

    g = Guion().traza(3000, 3000, 3500, 3000, 10)
    pk = paquete(3500, 3000, 60)
    g.o(pk[:3] + [0x12] + pk)            # el cuarto octeto, mal: se busca otro principio
    g.traza(3500, 3000, 4000, 3000, 10).suelta().mas(40)
    caso('cuarto', 'un cuarto octeto malo en medio: se resincroniza y sigue', g,
         lambda r, b: r['resincros'] >= 1 and r['dx'] > 40 and not r['perdido'])

    g = Guion().traza(3000, 3000, 3500, 3000, 10)
    g.o(paquete(3500, 3000, 60)[:3]).mas(8)   # medio paquete y un hueco de 80 ms
    g.traza(3500, 3000, 4000, 3000, 10).suelta().mas(40)
    caso('hueco', 'medio paquete y luego nada (80 ms): se tira y sigue', g,
         lambda r, b: r['resincros'] >= 1 and r['dx'] > 40)

    g = Guion()
    for i in range(30):
        g.o([0x08, 0x10, 0x20])           # lo de un ratón de siempre: no es el modo absoluto
    g.mas(5)
    caso('perdido', 'muchos octetos malos seguidos (90): perdido (raton.cas vuelve al modo relativo)', g,
         lambda r, b: r['perdido'] == 1)

    g = Guion().traza(6000, 3000, 6000, 2900, 3).suelta().mas(40).traza(5300, 4200, 5300, 1700, 50).suelta().mas(40)
    caso('aprende', 'las medidas se aprenden: tras ver X = 6000, X = 5300 ya no es el borde (mueve la flecha)', g,
         lambda r, b: r['rueda'] == 0 and r['dy'] > 40)

    g = Guion().traza(3000, 3000, 3600, 3000, 10)
    g.mas(30)                                 # sin paquetes (se perdió el final)
    g.traza(3600, 3000, 3600, 3000, 25).suelta().mas(40)
    caso('sinfinal', 'el dedo «se va» sin el paquete de Z = 0: el toque siguiente empieza de nuevo (sin salto ni botón pulsado)', g,
         lambda r, b: not b and 0 < r['dx'] < 400)
    return c


def corre(raton, carpeta, nombre, guion):
    ruta = os.path.join(carpeta, nombre.upper()[:8] + '.TPD')
    with open(ruta, 'w', newline='') as f:
        f.write(guion.texto())
    corre32 = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'corre32', 'corre32.sh')
    r = subprocess.run([corre32, '-C', carpeta, raton, '/ANALIZA', os.path.basename(ruta)],
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=120)
    sal = r.stdout.decode('cp437', 'replace')
    with open(ruta[:-4] + '.SAL', 'w') as f:
        f.write(sal)
    fin = None
    botones = []
    for l in sal.splitlines():
        m = re.match(r't=(\d+) botones=(\d+)', l)
        if m:
            botones.append((int(m.group(1)), int(m.group(2))))
        if l.startswith('fin:'):
            fin = {k: int(v) for k, v in re.findall(r'(\w+)=(-?\d+)', l)}
    if fin is None:
        return None, botones, sal
    # (el resumen lleva lo de desde el último «?»: se suma lo de antes)
    total = dict(fin)
    for l in sal.splitlines():
        if l.startswith('? '):
            d = {k: int(v) for k, v in re.findall(r'(\w+)=(-?\d+)', l)}
            for k in ('dx', 'dy', 'rueda', 'lados'):
                total[k] += d[k]
    return total, botones, sal


def prueba(raton, carpeta):
    os.makedirs(carpeta, exist_ok=True)
    malos = 0
    for nombre, que, g, mira in casos():
        if nombre == 'acelera':
            r1, _, s1 = corre(raton, carpeta, 'lento', g[0])
            r2, _, s2 = corre(raton, carpeta, 'rapido', g[1])
            if r1 and r2 and r2['dx'] > r1['dx'] * 3 // 2 and r1['dx'] > 10:
                print('bien: %s (despacio %d puntos, deprisa %d)' % (que, r1['dx'], r2['dx']))
            else:
                print('mal: %s: %s / %s' % (que, r1, r2))
                malos += 1
            continue
        r, b, sal = corre(raton, carpeta, nombre, g)
        if r is not None and mira(r, b):
            print('bien: %s' % que)
        else:
            malos += 1
            print('mal: %s (%s): %s; botones %s; %s' % (que, nombre, r, b, sal.strip().splitlines()[-1:] if sal.strip() else 'sin salida'))
    return malos


def inyecta(carpeta):
    os.makedirs(carpeta, exist_ok=True)
    # el borde derecho, de arriba abajo, despacio (en dos veces): 16 pasos de rueda
    g = Guion()
    g.traza(5400, 4300, 5400, 1800, 50).suelta().mas(30)
    with open(os.path.join(carpeta, 'BORDE.TPD'), 'w', newline='') as f:
        f.write(g.texto())
    # dos dedos hacia arriba (el simulado dice «varios dedos»)
    g = Guion()
    g.traza(3000, 2000, 3000, 3500, 40, w=0).suelta().mas(30)
    with open(os.path.join(carpeta, 'DOSDEDOS.TPD'), 'w', newline='') as f:
        f.write(g.texto())
    # un toque (un clic) donde esté la flecha
    g = Guion().toque().mas(40)
    with open(os.path.join(carpeta, 'TOQUE.TPD'), 'w', newline='') as f:
        f.write(g.texto())
    # todo seguido, con 6 s entre medias (para las capturas): el borde, dos
    # dedos y un toque
    g = Guion()
    g.traza(5400, 4300, 5400, 1800, 50).suelta().mas(600)
    g.traza(3000, 2000, 3000, 3500, 40, w=0).suelta().mas(600)
    g.toque().mas(40)
    with open(os.path.join(carpeta, 'TODO.TPD'), 'w', newline='') as f:
        f.write(g.texto())


if __name__ == '__main__':
    if len(sys.argv) == 4 and sys.argv[1] == 'prueba':
        sys.exit(1 if prueba(sys.argv[2], sys.argv[3]) else 0)
    if len(sys.argv) == 3 and sys.argv[1] == 'inyecta':
        inyecta(sys.argv[2])
        sys.exit(0)
    print(__doc__ if __doc__ else 'tpsintetico.py prueba RATON.BIN CARPETA | inyecta CARPETA')
    sys.exit(2)
