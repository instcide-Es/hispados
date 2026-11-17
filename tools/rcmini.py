#!/usr/bin/env python3
# rcmini.py - un compilador de recursos pequeño para los programas Win32 de
# prueba de Hispa-486 (docs/486/HISPA-486.md): de un texto sencillo, parecido
# a un .rc, a la sección .rsrc de un PE. Lo usa tools/elf2pe.py (--res).
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
#
#   STRING número "texto"
#   ICON número [archivo.ico]            (sin archivo: uno de relleno, 16x16)
#   MENU número|"NOMBRE"
#     POPUP "&Archivo" [GRAYED]
#       ITEM "&Abrir\tCtrl+A" número [CHECKED] [GRAYED]
#       SEPARATOR
#     END
#     ITEM "A&yuda" número               (una orden en la misma barra)
#   END
#   DIALOG número|"NOMBRE" x y ancho alto "título" [STYLE número]
#     LTEXT|RTEXT|CTEXT "texto" número x y ancho alto
#     EDITTEXT número x y ancho alto [MULTILINE]
#     PUSHBUTTON|DEFPUSHBUTTON|CHECKBOX|RADIOBUTTON|GROUPBOX "texto" número x y ancho alto
#     LISTBOX|COMBOBOX número x y ancho alto
#     CONTROL "texto" número "CLASE" estilo x y ancho alto
#   END
# Las medidas de los diálogos, en unidades de diálogo (como en un .rc); los
# números, en decimal o 0x...; «#» empieza un comentario; el texto, en UTF-8
# (en el PE va en UTF-16, como manda el formato). Hecho a partir de la
# documentación pública del formato PE (.rsrc) y de las plantillas de menú y
# de diálogo de Win32.
import os, struct, sys

RT_ICON, RT_MENU, RT_DIALOG, RT_STRING, RT_GROUP_ICON = 3, 4, 5, 6, 14
IDIOMA = 0x0C0A                     # español
ATOMOS = {'BUTTON': 0x80, 'EDIT': 0x81, 'STATIC': 0x82, 'LISTBOX': 0x83, 'COMBOBOX': 0x85}
# los controles con nombre propio: clase y estilo (con WS_CHILD | WS_VISIBLE)
CTL_TEXTO = {
    'LTEXT': ('STATIC', 0x50020000), 'RTEXT': ('STATIC', 0x50020002), 'CTEXT': ('STATIC', 0x50020001),
    'PUSHBUTTON': ('BUTTON', 0x50010000), 'DEFPUSHBUTTON': ('BUTTON', 0x50010001),
    'CHECKBOX': ('BUTTON', 0x50010003), 'RADIOBUTTON': ('BUTTON', 0x50010009),
    'GROUPBOX': ('BUTTON', 0x50000007),
}
CTL_SOLO = {'EDITTEXT': ('EDIT', 0x50810080), 'LISTBOX': ('LISTBOX', 0x50A10001), 'COMBOBOX': ('COMBOBOX', 0x50210003)}


def trozos(linea, n):
    """parte una línea en palabras y "textos" (con \\n, \\t, \\" y \\\\)"""
    sal, i = [], 0
    while i < len(linea):
        c = linea[i]
        if c in ' \t,':
            i += 1
        elif c == '#':
            break
        elif c == '"':
            t, i = '', i + 1
            while i < len(linea) and linea[i] != '"':
                if linea[i] == '\\' and i + 1 < len(linea):
                    i += 1
                    t += {'n': '\n', 't': '\t', 'r': '\r'}.get(linea[i], linea[i])
                else:
                    t += linea[i]
                i += 1
            if i >= len(linea):
                sys.exit(f'recursos, línea {n}: falta cerrar las comillas')
            i += 1
            sal.append(('t', t))
        else:
            j = i
            while j < len(linea) and linea[j] not in ' \t,"#':
                j += 1
            sal.append(('p', linea[i:j]))
            i = j
    return sal


def u16(t):
    return t.encode('utf-16-le') + b'\0\0'


class Lector:
    def __init__(self, texto):
        self.lineas = [(n + 1, trozos(l, n + 1)) for n, l in enumerate(texto.splitlines())]
        self.lineas = [x for x in self.lineas if x[1]]
        self.i = 0

    def sig(self):
        if self.i >= len(self.lineas):
            return None, None
        self.i += 1
        return self.lineas[self.i - 1]


def num(tr, n):
    if tr[0] != 'p':
        sys.exit(f'recursos, línea {n}: se esperaba un número y hay «{tr[1]}»')
    try:
        return int(tr[1], 0)
    except ValueError:
        sys.exit(f'recursos, línea {n}: «{tr[1]}» no es un número')


def nombre(tr, n):
    return tr[1].upper() if tr[0] == 't' else num(tr, n)


def lee_menu(lec, n0):
    """las órdenes hasta END -> la plantilla (sin la cabecera)"""
    ordenes = []
    while True:
        n, tr = lec.sig()
        if tr is None:
            sys.exit(f'recursos: al menú de la línea {n0} le falta END')
        p = tr[0][1].upper()
        if p == 'END':
            break
        resto = [x[1].upper() for x in tr[1:] if x[0] == 'p']
        fl = (1 if 'GRAYED' in resto else 0) | (8 if 'CHECKED' in resto else 0)
        if p == 'SEPARATOR':
            ordenes.append((0, struct.pack('<H', 0) + u16('')))
        elif p == 'ITEM':
            ordenes.append((fl, struct.pack('<H', num(tr[2], n)) + u16(tr[1][1])))
        elif p == 'POPUP':
            ordenes.append((fl | 0x10, u16(tr[1][1]) + lee_menu(lec, n)))
        else:
            sys.exit(f'recursos, línea {n}: en un menú no vale «{tr[0][1]}»')
    if not ordenes:
        sys.exit(f'recursos: el menú de la línea {n0} está vacío')
    sal = b''
    for k, (fl, cuerpo) in enumerate(ordenes):
        if k == len(ordenes) - 1:
            fl |= 0x80              # MF_END
        sal += struct.pack('<H', fl) + cuerpo
    return sal


def lee_dialogo(lec, tr, n0):
    x, y, an, al = (num(t, n0) for t in tr[2:6])
    titulo = tr[6][1] if len(tr) > 6 and tr[6][0] == 't' else ''
    estilo = 0x80C80080             # DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU
    for k, t in enumerate(tr):
        if t == ('p', 'STYLE'):
            estilo = num(tr[k + 1], n0)
    ctls = []
    while True:
        n, c = lec.sig()
        if c is None:
            sys.exit(f'recursos: al diálogo de la línea {n0} le falta END')
        p = c[0][1].upper()
        if p == 'END':
            break
        if p in CTL_TEXTO:
            clase, est = CTL_TEXTO[p]
            texto, ident, geo = c[1][1], num(c[2], n), c[3:7]
        elif p in CTL_SOLO:
            clase, est = CTL_SOLO[p]
            texto, ident, geo = '', num(c[1], n), c[2:6]
            if ('p', 'MULTILINE') in [(a, b.upper()) for a, b in c[6:]]:
                est = (est & ~0x80) | 0x00201004    # ES_MULTILINE | ES_WANTRETURN | WS_VSCROLL
        elif p == 'CONTROL':
            texto, ident, clase, est, geo = c[1][1], num(c[2], n), c[3][1].upper(), num(c[4], n), c[5:9]
        else:
            sys.exit(f'recursos, línea {n}: en un diálogo no vale «{c[0][1]}»')
        if len(geo) != 4:
            sys.exit(f'recursos, línea {n}: faltan medidas (x y ancho alto)')
        ctls.append((est, [num(g, n) for g in geo], ident, clase, texto))
    sal = struct.pack('<IIHhhhh', estilo, 0, len(ctls), x, y, an, al) + b'\0\0' + b'\0\0' + u16(titulo)
    for est, geo, ident, clase, texto in ctls:
        sal += b'\0' * (-len(sal) % 4)
        sal += struct.pack('<IIhhhhH', est, 0, geo[0], geo[1], geo[2], geo[3], ident & 0xFFFF)
        sal += struct.pack('<HH', 0xFFFF, ATOMOS[clase]) if clase in ATOMOS else u16(clase)
        sal += u16(texto) + b'\0\0'
    return sal


def icono(archivo, ident, rec, n):
    """un .ico -> RT_ICON (uno por imagen) y RT_GROUP_ICON"""
    if archivo:
        d = open(archivo, 'rb').read()
        _, tipo, cuantos = struct.unpack_from('<HHH', d, 0)
        if tipo != 1:
            sys.exit(f'recursos, línea {n}: {archivo} no es un .ico')
        imgs = []
        for k in range(cuantos):
            an, al, col, _, pl, bpp, tam, pos = struct.unpack_from('<BBBBHHII', d, 6 + 16 * k)
            imgs.append((an, al, col, pl, bpp, d[pos:pos + tam]))
    else:                           # de relleno: 16x16, dos colores, un marco
        filas = [0xFFFF] + [0x8001] * 14 + [0xFFFF]
        xor = b''.join(struct.pack('>HH', f, 0) for f in reversed(filas))
        img = struct.pack('<IiiHHIIiiII', 40, 16, 32, 1, 1, 0, 0, 0, 0, 0, 0) + bytes([0, 0, 0, 0, 255, 255, 255, 0])
        imgs = [(16, 16, 2, 1, 1, img + xor + bytes(64))]
    grupo = struct.pack('<HHH', 0, 1, len(imgs))
    for an, al, col, pl, bpp, datos in imgs:
        k = len(rec.setdefault(RT_ICON, {})) + 1
        rec[RT_ICON][k] = datos
        grupo += struct.pack('<BBBBHHIH', an, al, col, 0, pl, bpp, len(datos), k)
    rec.setdefault(RT_GROUP_ICON, {})[ident] = grupo


def compila(texto, carpeta, rva):
    """el texto de los recursos -> los octetos de la sección .rsrc, que irá en esa RVA"""
    rec, textos = {}, {}
    lec = Lector(texto)
    while True:
        n, tr = lec.sig()
        if tr is None:
            break
        p = tr[0][1].upper()
        if p == 'STRING':
            textos[num(tr[1], n)] = tr[2][1]
        elif p == 'ICON':
            icono(os.path.join(carpeta, tr[2][1]) if len(tr) > 2 else None, nombre(tr[1], n), rec, n)
        elif p == 'MENU':
            rec.setdefault(RT_MENU, {})[nombre(tr[1], n)] = struct.pack('<HH', 0, 0) + lee_menu(lec, n)
        elif p == 'DIALOG':
            if len(tr) < 6:
                sys.exit(f'recursos, línea {n}: DIALOG nombre x y ancho alto "título"')
            rec.setdefault(RT_DIALOG, {})[nombre(tr[1], n)] = lee_dialogo(lec, tr, n)
        else:
            sys.exit(f'recursos, línea {n}: no se entiende «{tr[0][1]}»')
    for bloque in sorted({k >> 4 for k in textos}):     # los textos, de 16 en 16
        d = b''
        for k in range(16):
            t = textos.get(bloque * 16 + k, '').encode('utf-16-le')
            d += struct.pack('<H', len(t) // 2) + t
        rec.setdefault(RT_STRING, {})[bloque + 1] = d
    if not rec:
        return b''

    def orden(claves):              # primero los nombres; luego los números
        return sorted((k for k in claves if isinstance(k, str))) + sorted(k for k in claves if isinstance(k, int))
    tipos = orden(rec)
    # dónde va cada cosa: la raíz, un directorio por tipo, uno por nombre (con
    # su idioma), las fichas de los datos, los nombres y los datos
    pos = 16 + 8 * len(tipos)
    pos_tipo, pos_nom = {}, {}
    for t in tipos:
        pos_tipo[t] = pos
        pos += 16 + 8 * len(rec[t])
    for t in tipos:
        for k in orden(rec[t]):
            pos_nom[(t, k)] = pos
            pos += 24
    pos_ficha = {}
    for t in tipos:
        for k in orden(rec[t]):
            pos_ficha[(t, k)] = pos
            pos += 16
    pos_txt, nombres = {}, b''
    for k in [t for t in tipos if isinstance(t, str)] + [k for t in tipos for k in orden(rec[t]) if isinstance(k, str)]:
        if k not in pos_txt:
            pos_txt[k] = pos + len(nombres)
            nombres += struct.pack('<H', len(k)) + k.encode('utf-16-le')
    pos += len(nombres)
    pos += -pos % 4
    pos_dato, datos = {}, b''
    for t in tipos:
        for k in orden(rec[t]):
            pos_dato[(t, k)] = pos + len(datos)
            datos += rec[t][k] + b'\0' * (-len(rec[t][k]) % 4)

    def directorio(claves, donde):
        nn = sum(1 for k in claves if isinstance(k, str))
        d = struct.pack('<IIHHHH', 0, 0, 0, 0, nn, len(claves) - nn)
        for k in claves:
            d += struct.pack('<II', (0x80000000 | pos_txt[k]) if isinstance(k, str) else k, 0x80000000 | donde(k))
        return d
    sal = directorio(tipos, lambda t: pos_tipo[t])
    for t in tipos:
        sal += directorio(orden(rec[t]), lambda k: pos_nom[(t, k)])
    for t in tipos:
        for k in orden(rec[t]):
            sal += struct.pack('<IIHHHHII', 0, 0, 0, 0, 0, 1, IDIOMA, pos_ficha[(t, k)])
    for t in tipos:
        for k in orden(rec[t]):
            sal += struct.pack('<IIII', rva + pos_dato[(t, k)], len(rec[t][k]), 0, 0)
    sal += nombres
    sal += b'\0' * (-len(sal) % 4)
    return sal + datos


if __name__ == '__main__':
    d = compila(open(sys.argv[1], encoding='utf-8').read(), os.path.dirname(sys.argv[1]), 0x1000)
    sys.stdout.write(f'{len(d)} octetos de recursos\n')
