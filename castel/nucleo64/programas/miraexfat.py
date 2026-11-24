#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo64/programas/miraexfat.py - Hispa-64, etapa 6 (docs/64/HISPA-64.md,
# sección 12): comprueba un volumen exFAT desde el anfitrión (en el servidor
# no hay fsck.exfat y no se instala nada), según «exFAT file system
# specification» (Microsoft, 2019):
#   - el sector de arranque: la firma, la suma de las 11 primeras regiones
#     (y la de la copia), la copia igual al original (menos VolumeFlags y
#     PercentInUse), VolumeDirty a cero (se ha vaciado al apagar) y
#     PercentInUse, el que toca;
#   - la tabla de mayúsculas (con su suma) y el mapa de bits;
#   - cada grupo de entradas de cada carpeta (desde la raíz): la 0x85 con su
#     número de secundarias, la 0xC0 y las 0xC1, SetChecksum, NameLength y
#     NameHash (con la tabla del volumen), ValidDataLength <= DataLength, las
#     carpetas con DataLength = ValidDataLength en cúmulos enteros, nada
#     detrás de la entrada de fin (0x00), los 10 ms (< 200);
#   - los cúmulos de cada archivo y carpeta (seguidos con NoFatChain, o la
#     cadena de la FAT, del largo que dice DataLength): ninguno fuera del
#     volumen ni en dos sitios a la vez (cruzados), y el mapa de bits igual a
#     lo usado (ni perdidos ni sin marcar).
# Uso:
#   miraexfat.py IMAGEN                 comprueba (y lo dice: 0 si está bien)
#   miraexfat.py IMAGEN tipo RUTA       escribe el archivo (como mtype)
#   miraexfat.py IMAGEN dir RUTA        lista la carpeta: «nombre tamaño» o «nombre/»
# (las rutas, con «/», sin mirar mayúsculas: con la tabla del volumen)
import struct
import sys


class Mal(Exception):
    pass


def suma32(datos, saltar=()):
    s = 0
    for i, b in enumerate(datos):
        if i in saltar:
            continue
        s = ((((s & 1) << 31) | (s >> 1)) + b) & 0xFFFFFFFF
    return s


def suma16(datos, saltar=()):
    s = 0
    for i, b in enumerate(datos):
        if i in saltar:
            continue
        s = ((((s & 1) << 15) | (s >> 1)) + b) & 0xFFFF
    return s


class Volumen:
    def __init__(self, ruta):
        with open(ruta, 'rb') as f:
            self.img = f.read()
        self.quejas = []
        b = self.img[:512]
        if b[3:11] != b'EXFAT   ' or b[510:512] != b'\x55\xAA':
            raise Mal('no es un volumen exFAT')
        (self.despl, self.sectores, self.fat_ini, self.fat_largo, self.heap, self.cúmulos, self.raíz, self.serie,
         self.revisión, self.flags, bps, spc, self.n_fats, _unidad) = struct.unpack_from('<QQIIIIIIHHBBBB', b, 64)
        self.pct = b[112]
        self.bps = 1 << bps
        self.oc = self.bps << spc
        self.fat = struct.unpack_from('<%dI' % (self.cúmulos + 2), self.img, self.fat_ini * self.bps)
        self.mayús = list(range(0x10000))
        self.usados = {}             # cúmulo -> de quién es
        self.archivos = 0
        self.carpetas = 0

    def queja(self, t):
        self.quejas.append(t)

    def pos(self, c):
        return self.heap * self.bps + (c - 2) * self.oc

    # ---- el arranque ----
    def mira_arranque(self):
        bps = self.bps
        for base, qué in ((0, 'el sector de arranque'), (12, 'la copia del arranque')):
            región = self.img[base * bps:(base + 11) * bps]
            s = suma32(región, (106, 107, 112))
            sumas = struct.unpack_from('<%dI' % (bps // 4), self.img, (base + 11) * bps)
            if any(x != s for x in sumas):
                self.queja('%s: la suma no cuadra' % qué)
        a = bytearray(self.img[0:11 * bps])
        c = bytearray(self.img[12 * bps:23 * bps])
        for x in (a, c):
            x[106:108] = b'\0\0'
            x[112] = 0
        if a != c:
            self.queja('la copia del arranque no es igual al original')
        if self.flags & 2:
            self.queja('VolumeDirty puesto (no se ha vaciado)')
        if self.flags & 1:
            self.queja('ActiveFat = 1 (TexFAT)')
        if self.revisión >> 8 != 1:
            self.queja('FileSystemRevision %x' % self.revisión)
        if self.n_fats != 1:
            self.queja('NumberOfFats = %d' % self.n_fats)

    # ---- los cúmulos ----
    def cadena(self, primero, largo, seguido, dueño, carpeta=False):
        """los cúmulos de un archivo: comprobados y apuntados a su dueño"""
        n = (largo + self.oc - 1) // self.oc
        if primero == 0:
            if largo != 0:
                self.queja('%s: DataLength %d sin cúmulos' % (dueño, largo))
            return []
        if n == 0:
            self.queja('%s: FirstCluster %d con DataLength 0' % (dueño, primero))
            return []
        cs = []
        if seguido:
            cs = list(range(primero, primero + n))
        else:
            c = primero
            while True:
                if c < 2 or c >= self.cúmulos + 2:
                    self.queja('%s: la cadena se sale del volumen (%#x)' % (dueño, c))
                    break
                cs.append(c)
                if len(cs) > self.cúmulos:
                    self.queja('%s: la cadena da vueltas' % dueño)
                    break
                s = self.fat[c]
                if s >= 0xFFFFFFF8:
                    break
                if s == 0xFFFFFFF7 or s < 2:
                    self.queja('%s: la cadena se rompe en %d (%#x)' % (dueño, c, s))
                    break
                c = s
            if len(cs) != n:
                self.queja('%s: %d cúmulos en la cadena y DataLength dice %d' % (dueño, len(cs), n))
        for c in cs:
            if c < 2 or c >= self.cúmulos + 2:
                self.queja('%s: el cúmulo %d se sale del volumen' % (dueño, c))
            elif c in self.usados:
                self.queja('cruzados: el cúmulo %d es de %s y de %s' % (c, self.usados[c], dueño))
            else:
                self.usados[c] = dueño
        return cs

    def lee(self, cs, largo):
        d = b''.join(self.img[self.pos(c):self.pos(c) + self.oc] for c in cs)
        return d[:largo]

    # ---- las carpetas ----
    def entradas(self, datos):
        return [datos[i:i + 32] for i in range(0, len(datos), 32)]

    def hash_nombre(self, u):
        h = 0
        for x in u:
            x = self.mayús[x]
            h = ((((h & 1) << 15) | (h >> 1)) + (x & 0xFF)) & 0xFFFF
            h = ((((h & 1) << 15) | (h >> 1)) + (x >> 8)) & 0xFFFF
        return h

    def carpeta(self, datos, ruta, raíz=False):
        """los archivos de una carpeta: [(nombre, atrib, primero, válido, largo, seguido)]"""
        es = self.entradas(datos)
        hijos = []
        i = 0
        fin = False
        while i < len(es):
            e = es[i]
            t = e[0]
            if fin:
                if t != 0:
                    self.queja('%s: una entrada (%#x) detrás de la de fin' % (ruta, t))
                i += 1
                continue
            if t == 0:
                fin = True
                i += 1
                continue
            if t < 0x80:
                i += 1
                continue
            if t in (0x81, 0x82, 0x83, 0xA0, 0xA1, 0xA2):
                if not raíz:
                    self.queja('%s: una entrada %#x fuera de la raíz' % (ruta, t))
                i += 1
                continue
            if t != 0x85:
                self.queja('%s: una entrada %#x suelta (sin su 0x85)' % (ruta, t))
                i += 1
                continue
            sc = e[1]
            if sc < 2 or sc > 18 or i + sc >= len(es):
                self.queja('%s: un grupo con %d secundarias' % (ruta, sc))
                i += 1
                continue
            g = b''.join(es[i:i + 1 + sc])
            if suma16(g, (2, 3)) != struct.unpack_from('<H', g, 2)[0]:
                self.queja('%s: SetChecksum mal en la entrada %d' % (ruta, i))
            atrib = struct.unpack_from('<H', g, 4)[0]
            if g[20] >= 200 or g[21] >= 200:
                self.queja('%s: los 10 ms pasan de 199 en la entrada %d' % (ruta, i))
            s = g[32:64]
            if s[0] != 0xC0:
                self.queja('%s: la 0x85 de la entrada %d no lleva detrás su 0xC0' % (ruta, i))
                i += 1 + sc
                continue
            flags, nl, h = s[1], s[3], struct.unpack_from('<H', s, 4)[0]
            válido, = struct.unpack_from('<Q', s, 8)
            primero, largo = struct.unpack_from('<IQ', s, 20)
            u = []
            for k in range(2, 1 + sc):
                f = g[k * 32:(k + 1) * 32]
                if f[0] != 0xC1:
                    self.queja('%s: una secundaria %#x donde va el nombre' % (ruta, f[0]))
                    continue
                u.extend(struct.unpack_from('<15H', f, 2))
            if nl == 0 or nl > len(u) or (nl + 14) // 15 != sc - 1:
                self.queja('%s: NameLength %d con %d entradas de nombre' % (ruta, nl, sc - 1))
            u = u[:nl]
            nombre = ''.join(chr(x) for x in u)
            if self.hash_nombre(u) != h:
                self.queja('%s/%s: NameHash mal' % (ruta, nombre))
            if válido > largo:
                self.queja('%s/%s: ValidDataLength > DataLength' % (ruta, nombre))
            if not flags & 1:
                self.queja('%s/%s: sin AllocationPossible' % (ruta, nombre))
            if atrib & 0x10 and (largo % self.oc or válido != largo or largo == 0):
                self.queja('%s/%s: una carpeta con DataLength %d y ValidDataLength %d' % (ruta, nombre, largo, válido))
            hijos.append((nombre, atrib, primero, válido, largo, bool(flags & 2)))
            i += 1 + sc
        mayús = [n.upper() for n, *_ in hijos]
        if len(set(mayús)) != len(mayús):
            self.queja('%s: dos nombres iguales' % ruta)
        return hijos

    def recorre(self, datos, ruta, raíz=False, archivos=None):
        for nombre, atrib, primero, válido, largo, seguido in self.carpeta(datos, ruta, raíz):
            r = ruta + '/' + nombre
            cs = self.cadena(primero, largo, seguido, r)
            if archivos is not None:
                archivos[r] = (r, atrib, cs, válido, largo)
            if atrib & 0x10:
                self.carpetas += 1
                self.recorre(self.lee(cs, largo), r, archivos=archivos)
            else:
                self.archivos += 1

    def mira(self, archivos=None):
        self.mira_arranque()
        # la raíz
        # (su largo no lo dice nadie: la cadena, hasta el final)
        raíz_cs = []
        c = self.raíz
        while 2 <= c < self.cúmulos + 2 and len(raíz_cs) <= self.cúmulos:
            raíz_cs.append(c)
            s = self.fat[c]
            if s >= 0xFFFFFFF8:
                break
            if s < 2 or s >= self.cúmulos + 2:
                self.queja('la raíz: la cadena se rompe en %d' % c)
                break
            c = s
        for c in raíz_cs:
            self.usados[c] = '(la raíz)'
        datos = self.lee(raíz_cs, len(raíz_cs) * self.oc)
        mapa = tabla = None
        for e in self.entradas(datos):
            if e[0] == 0:
                break
            if e[0] == 0x81 and mapa is None:
                mapa = struct.unpack_from('<IQ', e, 20)
            if e[0] == 0x82 and tabla is None:
                tabla = (struct.unpack_from('<I', e, 4)[0],) + struct.unpack_from('<IQ', e, 20)
        if mapa is None or tabla is None:
            raise Mal('sin el mapa de bits o la tabla de mayúsculas')
        cs_mapa = self.cadena(mapa[0], mapa[1], False, '(el mapa de bits)')
        bits = self.lee(cs_mapa, mapa[1])
        if len(bits) * 8 < self.cúmulos:
            self.queja('el mapa de bits es corto')
        cs_tabla = self.cadena(tabla[1], tabla[2], False, '(la tabla de mayúsculas)')
        t = self.lee(cs_tabla, tabla[2])
        if suma32(t) != tabla[0]:
            self.queja('la suma de la tabla de mayúsculas no cuadra')
        w = struct.unpack('<%dH' % (len(t) // 2), t)
        i = k = 0
        while i < len(w) and k < 0x10000:
            if w[i] == 0xFFFF and i + 1 < len(w):
                k += w[i + 1]
                i += 2
            else:
                self.mayús[k] = w[i]
                k += 1
                i += 1
        self.recorre(datos, '', raíz=True, archivos=archivos)
        # el mapa de bits frente a lo usado
        perdidos = sin_marcar = 0
        for c in range(2, self.cúmulos + 2):
            b = (bits[(c - 2) // 8] >> ((c - 2) % 8)) & 1 if (c - 2) // 8 < len(bits) else 0
            if b and c not in self.usados:
                perdidos += 1
            if not b and c in self.usados:
                sin_marcar += 1
                if sin_marcar < 4:
                    self.queja('el cúmulo %d es de %s y en el mapa está libre' % (c, self.usados[c]))
        if perdidos:
            self.queja('%d cúmulos en uso en el mapa y sin dueño (perdidos)' % perdidos)
        if sin_marcar:
            self.queja('%d cúmulos usados y libres en el mapa' % sin_marcar)
        pct = len(self.usados) * 100 // self.cúmulos
        if self.pct != pct and self.pct != 0xFF:
            self.queja('PercentInUse %d y está en uso el %d %%' % (self.pct, pct))


def busca(v, ruta):
    archivos = {}
    v.mira(archivos)

    def igual(t):
        return ''.join(chr(v.mayús[ord(ch)]) if ord(ch) < 0x10000 else ch for ch in t)
    clave = igual('/' + ruta.strip('/'))
    for k, val in archivos.items():
        if igual(k) == clave:
            return archivos, val
    return archivos, None


def main():
    if len(sys.argv) < 2:
        print('uso: miraexfat.py IMAGEN [tipo RUTA | dir RUTA]', file=sys.stderr)
        sys.exit(2)
    try:
        v = Volumen(sys.argv[1])
        if len(sys.argv) >= 4 and sys.argv[2] == 'tipo':
            _, a = busca(v, sys.argv[3])
            if a is None or a[1] & 0x10:
                print('miraexfat: no está %s' % sys.argv[3], file=sys.stderr)
                sys.exit(1)
            datos = v.lee(a[2], a[4])
            datos = datos[:a[3]] + bytes(a[4] - a[3])
            sys.stdout.buffer.write(datos)
            sys.exit(0)
        if len(sys.argv) >= 4 and sys.argv[2] == 'dir':
            archivos, a = busca(v, sys.argv[3])
            if sys.argv[3].strip('/') == '':
                pre = ''
            elif a is None or not a[1] & 0x10:
                print('miraexfat: no está la carpeta %s' % sys.argv[3], file=sys.stderr)
                sys.exit(1)
            else:
                pre = a[0]
            for r, atrib, cs, válido, largo in archivos.values():
                padre, _, nombre = r.rpartition('/')
                if padre == pre:
                    print(nombre + '/' if atrib & 0x10 else '%s %d' % (nombre, largo))
            sys.exit(0)
        v.mira()
    except Mal as e:
        print('exFAT MAL: %s' % e)
        sys.exit(1)
    if v.quejas:
        for q in v.quejas[:20]:
            print('exFAT MAL: ' + q)
        if len(v.quejas) > 20:
            print('exFAT MAL: ... y %d más' % (len(v.quejas) - 20))
        sys.exit(1)
    print('exFAT bien: %d archivos y %d carpetas; %d de %d cúmulos en uso (el %d %%); sumas, NameHash, cadenas y mapa de bits, bien'
          % (v.archivos, v.carpetas, len(v.usados), v.cúmulos, v.pct))


if __name__ == '__main__':
    main()
