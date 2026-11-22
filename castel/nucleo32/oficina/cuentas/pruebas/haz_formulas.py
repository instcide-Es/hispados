#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# haz_formulas.py SALIDA [CASOS]: la otra mitad de la batería de fórmulas de
# Cuentas 2 (la de formulas.txt es a mano): un guion de CUENTAS --guion con
# los resultados calculados aquí, con Python (las cuentas de dinero de
# siempre, la estadística, las fechas con datetime, los redondeos con
# decimal, los textos), de casos al azar con semilla fija.
import sys, random, math, datetime, statistics
from decimal import Decimal, ROUND_HALF_UP

sal = sys.argv[1]
casos = int(sys.argv[2]) if len(sys.argv) > 2 else 60
R = random.Random(20261122)
L = ['# hecho por haz_formulas.py: no se toca a mano']
fila = [0]


def es(x):
    """un número como se escribe en una fórmula en español"""
    t = repr(float(x)) if not isinstance(x, int) else str(x)
    if 'e' in t or 'E' in t:
        t = '%.15g' % x
    return t.replace('.', ',')


def celda(col):
    fila[0] += 1
    return '%s%d' % (col, fila[0])


def num(formula, esperado, col='A'):
    c = celda(col)
    L.append('pon %s =%s' % (c, formula))
    L.append('num %s %r' % (c, float(esperado)))


def txt(formula, esperado, col='A'):
    c = celda(col)
    L.append('pon %s =%s' % (c, formula))
    L.append('txt %s %s' % (c, esperado))


def err(formula, e, col='A'):
    c = celda(col)
    L.append('pon %s =%s' % (c, formula))
    L.append('err %s %s' % (c, e))


# ---- el dinero ----
def pmt(r, n, pv, fv=0, t=0):
    if r == 0:
        return -(pv + fv) / n
    k = (1 + r) ** n
    return -(r * (fv + pv * k)) / ((1 + r * t) * (k - 1))


def fv(r, n, p, pv=0, t=0):
    if r == 0:
        return -(pv + p * n)
    k = (1 + r) ** n
    return -(pv * k + p * (1 + r * t) * (k - 1) / r)


def pv(r, n, p, fv_=0, t=0):
    if r == 0:
        return -(fv_ + p * n)
    k = (1 + r) ** n
    return -(fv_ + p * (1 + r * t) * (k - 1) / r) / k


def nper(r, p, pv_, fv_=0, t=0):
    if r == 0:
        return -(pv_ + fv_) / p
    q = p * (1 + r * t)
    return math.log((q - fv_ * r) / (q + pv_ * r)) / math.log(1 + r)


def ipmt(r, per, n, pv_, fv_=0, t=0):
    p = pmt(r, n, pv_, fv_, t)
    if per == 1:
        return 0 if t else -pv_ * r
    if t:
        i = fv(r, per - 2, p, pv_, 1) - p
    else:
        i = fv(r, per - 1, p, pv_, 0)
    return i * r


L.append('hoja Dinero')
fila[0] = 0
for _ in range(casos):
    r = R.choice([0.01, 0.025, 0.035 / 12, 0.05 / 12, 0.07, 0.1, 0.004])
    n = R.choice([6, 12, 24, 36, 60, 120, 360])
    capital = R.choice([1000, 2500.5, 15000, 120000, 300000])
    t = R.choice([0, 1])
    vf_ = R.choice([0, 0, 500, -1000])
    num('PAGO(%s;%d;%s;%s;%d)' % (es(r), n, es(-capital), es(vf_), t), pmt(r, n, -capital, vf_, t))
    p = pmt(r, n, -capital, vf_, t)
    num('VA(%s;%d;%s;%s;%d)' % (es(r), n, es(p), es(vf_), t), pv(r, n, p, vf_, t), 'B')
    num('VF(%s;%d;%s;%s;%d)' % (es(r), n, es(-100), es(-capital), t), fv(r, n, -100, -capital, t), 'C')
    num('NPER(%s;%s;%s;%s;%d)' % (es(r), es(p), es(-capital), es(vf_), t), nper(r, p, -capital, vf_, t), 'D')
    per = R.randint(1, n)
    num('PAGOINT(%s;%d;%d;%s;%s;%d)' % (es(r), per, n, es(capital), es(vf_), t), ipmt(r, per, n, capital, vf_, t), 'E')
    num('PAGOPRIN(%s;%d;%d;%s;%s;%d)' % (es(r), per, n, es(capital), es(vf_), t),
        pmt(r, n, capital, vf_, t) - ipmt(r, per, n, capital, vf_, t), 'F')
    # TASA: con la cuota de antes sale la tasa de antes
    num('TASA(%d;%s;%s;%s;%d)' % (n, es(p), es(-capital), es(vf_), t), r, 'G')
# VNA y TIR con flujos de una columna
flujos = [-10000, 3000, 4200, 6800, 1200]
L.append('pon H1 %s' % es(flujos[0]))
for i, v in enumerate(flujos):
    L.append('pon H%d %s' % (i + 1, es(v)))
for tasa in (0.05, 0.08, 0.12):
    num('VNA(%s;H2:H5)+H1' % es(tasa), sum(v / (1 + tasa) ** i for i, v in enumerate(flujos)), 'I')


def irr(c):
    lo, hi = -0.99, 1.0
    f = lambda x: sum(v / (1 + x) ** i for i, v in enumerate(c))
    for _ in range(300):
        mid = (lo + hi) / 2
        if f(lo) * f(mid) <= 0:
            hi = mid
        else:
            lo = mid
    return (lo + hi) / 2


num('TIR(H1:H5)', irr(flujos), 'I')

# ---- estadística ----
L.append('hoja Estadistica')
fila[0] = 0
for k in range(casos // 3):
    datos = [round(R.uniform(-500, 500), R.choice([0, 1, 2])) for _ in range(R.randint(3, 25))]
    col = 'A'
    base = k * 30 + 1
    for i, v in enumerate(datos):
        L.append('pon %s%d %s' % ('J', base + i, es(v)))
    rg = 'J%d:J%d' % (base, base + len(datos) - 1)
    num('SUMA(%s)' % rg, sum(datos))
    num('PROMEDIO(%s)' % rg, statistics.fmean(datos), 'B')
    num('MEDIANA(%s)' % rg, statistics.median(datos), 'C')
    num('DESVEST(%s)' % rg, statistics.stdev(datos), 'D')
    num('DESVESTP(%s)' % rg, statistics.pstdev(datos), 'E')
    num('VAR(%s)' % rg, statistics.variance(datos), 'F')
    num('MAX(%s)-MIN(%s)' % (rg, rg), max(datos) - min(datos), 'G')
    s = sorted(datos)
    num('K.ESIMO.MAYOR(%s;2)' % rg, s[-2], 'H')
    num('K.ESIMO.MENOR(%s;2)' % rg, s[1], 'I')
    umbral = round(R.uniform(-200, 200))
    num('CONTAR.SI(%s;">%s")' % (rg, es(umbral)), sum(1 for v in datos if v > umbral), 'K')
    num('SUMAR.SI(%s;"<=%s")' % (rg, es(umbral)), sum(v for v in datos if v <= umbral), 'L')
    num('SUMAPRODUCTO(%s;%s)' % (rg, rg), sum(v * v for v in datos), 'M')
    fila[0] = base + 28

# ---- redondeos (con decimal: la mitad, lejos de cero) ----
L.append('hoja Redondeos')
fila[0] = 0
for _ in range(casos):
    x = round(R.uniform(-10000, 10000), R.choice([2, 3, 4, 5]))
    d = R.choice([0, 1, 2, 3, -1, -2])
    q = Decimal(repr(x)).quantize(Decimal(1).scaleb(-d), rounding=ROUND_HALF_UP)
    num('REDONDEAR(%s;%d)' % (es(x), d), float(q))
    num('ENTERO(%s)' % es(x), math.floor(x), 'B')
    num('TRUNCAR(%s;%d)' % (es(x), max(d, 0)), float(Decimal(repr(x)).quantize(Decimal(1).scaleb(-max(d, 0)), rounding='ROUND_DOWN')), 'C')
    m = R.choice([3, 7, -4, 2.5])
    num('RESIDUO(%s;%s)' % (es(x), es(m)), x - m * math.floor(x / m), 'D')
    num('ABS(%s)*SIGNO(%s)' % (es(x), es(x)), x, 'E')
    y = R.uniform(0.1, 50)
    num('RAIZ(%s)' % es(round(y, 4)), math.sqrt(round(y, 4)), 'F')
    num('LN(%s)' % es(round(y, 4)), math.log(round(y, 4)), 'G')
    b, e_ = round(y, 2), R.randint(-3, 4)
    num('POTENCIA(%s;%d)' % (es(b), e_), b ** e_, 'H')

# ---- fechas ----
L.append('hoja Fechas')
fila[0] = 0
base = datetime.date(1899, 12, 30)
for _ in range(casos):
    f = datetime.date(1950, 1, 1) + datetime.timedelta(days=R.randint(0, 40000))
    s = (f - base).days
    num('FECHA(%d;%d;%d)' % (f.year, f.month, f.day), s)
    num('AÑO(%d)+MES(%d)+DIA(%d)' % (s, s, s), f.year + f.month + f.day, 'B')
    num('DIASEM(%d)' % s, (f.isoweekday() % 7) + 1, 'C')
    num('DIASEM(%d;2)' % s, f.isoweekday(), 'D')
    m = R.randint(-30, 30)
    y, mo = f.year + (f.month - 1 + m) // 12, (f.month - 1 + m) % 12 + 1
    import calendar
    ult = calendar.monthrange(y, mo)[1]
    num('FIN.MES(%d;%d)' % (s, m), (datetime.date(y, mo, ult) - base).days, 'E')
    num('FECHA.MES(%d;%d)' % (s, m), (datetime.date(y, mo, min(f.day, ult)) - base).days, 'F')
    g = f + datetime.timedelta(days=R.randint(0, 400))
    lab = sum(1 for i in range((g - f).days + 1) if (f + datetime.timedelta(days=i)).weekday() < 5)
    num('DIAS.LAB(%d;%d)' % (s, (g - base).days), lab, 'G')
    txt('TEXTO(%d;"dd/mm/aaaa")' % s, f.strftime('%d/%m/%Y'), 'H')
    h, mi, se = R.randint(0, 23), R.randint(0, 59), R.randint(0, 59)
    num('NSHORA(%d;%d;%d)' % (h, mi, se), (h * 3600 + mi * 60 + se) / 86400, 'I')
    num('HORA(NSHORA(%d;%d;%d))*100+MINUTO(NSHORA(%d;%d;%d))' % (h, mi, se, h, mi, se), h * 100 + mi, 'K')

# ---- textos ----
L.append('hoja Textos')
fila[0] = 0
palabras = ['cuenta', 'Árbol', 'ñandú', 'Hispa', 'PENTIUM', 'caña', 'pingüino', 'hoja de cálculo', 'año', 'x']
for _ in range(casos):
    a, b = R.choice(palabras), R.choice(palabras)
    t = a + ' ' + b
    n = R.randint(0, len(t))
    txt('IZQUIERDA("%s";%d)' % (t, n), t[:n])
    txt('DERECHA("%s";%d)' % (t, n), t[len(t) - n:] if n else '', 'B')
    i = R.randint(1, len(t))
    txt('EXTRAE("%s";%d;%d)' % (t, i, n), t[i - 1:i - 1 + n], 'C')
    num('LARGO("%s")' % t, len(t), 'D')
    txt('MAYUSC("%s")' % t, t.upper(), 'E')
    txt('MINUSC("%s")' % t, t.lower(), 'F')
    txt('SUSTITUIR("%s";"a";"@")' % t, t.replace('a', '@'), 'G')
    txt('REPETIR("%s";%d)' % (a, n % 4), a * (n % 4), 'H')
    pos = t.find(b[0]) + 1
    num('ENCONTRAR("%s";"%s")' % (b[0], t), pos, 'I')

with open(sal, 'w', encoding='utf-8') as f:
    f.write('\n'.join(L) + '\n')
print('haz_formulas.py: %s (%d líneas)' % (sal, len(L)))
