#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo64/programas/hazexfat.py - Hispa-64, etapa 5 (docs/64/HISPA-64.md,
# sección 11): hace un volumen exFAT pequeño, para las pruebas, sin
# mkfs.exfat (en el servidor no está y no se instala nada). Según «exFAT file
# system specification» (Microsoft, 2019): el sector de arranque (y su
# copia) con su suma, la FAT, el mapa de bits de los libres, la tabla de
# mayúsculas, la raíz (con la etiqueta, el mapa y la tabla) y los archivos,
# cada uno con sus entradas 0x85, 0xC0 y 0xC1 (con la suma del grupo y el
# «hash» del nombre).
#   hazexfat.py SALIDA OCTETOS DESPLAZAMIENTO ETIQUETA RUTA_EN_EL_VOLUMEN=ARCHIVO_DEL_ANFITRIÓN ...
# (DESPLAZAMIENTO: el sector de la partición en el disco, para PartitionOffset)
# Los archivos van seguidos (NoFatChain) salvo los que acaban en «.frag», que
# van a trozos, enlazados en la FAT (y se guardan sin «.frag»).
import struct, sys, time

SECTOR = 512
CÚMULO = 4096
SPC_SHIFT = 3


def mayúscula(u):
    if 0x61 <= u <= 0x7A:
        return u - 32
    if 0xE0 <= u <= 0xFE and u != 0xF7:
        return u - 32
    if u == 0xFF:
        return 0x178
    return u


def suma32(datos, saltar):
    s = 0
    for i, b in enumerate(datos):
        if i in saltar:
            continue
        s = (((s & 1) << 31) | (s >> 1)) + b
        s &= 0xFFFFFFFF
    return s


def suma16(datos, saltar=()):
    s = 0
    for i, b in enumerate(datos):
        if i in saltar:
            continue
        s = (((s & 1) << 15) | (s >> 1)) + b
        s &= 0xFFFF
    return s


def hora_dos(t):
    lt = time.gmtime(t)
    fecha = ((lt.tm_year - 1980) << 9) | (lt.tm_mon << 5) | lt.tm_mday
    hora = (lt.tm_hour << 11) | (lt.tm_min << 5) | (lt.tm_sec // 2)
    return (fecha << 16) | hora


class Volumen:
    def __init__(self, octetos, desplazamiento, etiqueta):
        self.octetos = octetos
        self.sectores = octetos // SECTOR
        self.fat_ini = 128
        cúmulos = (self.sectores - 256) // (CÚMULO // SECTOR)
        self.fat_largo = ((cúmulos + 2) * 4 + SECTOR - 1) // SECTOR
        self.heap = (self.fat_ini + self.fat_largo + 7) // 8 * 8
        self.cúmulos = (self.sectores - self.heap) // (CÚMULO // SECTOR)
        self.fat = [0] * (self.cúmulos + 2)
        self.fat[0] = 0xFFFFFFF8
        self.fat[1] = 0xFFFFFFFF
        self.usados = set()
        self.img = bytearray(octetos)
        self.desplazamiento = desplazamiento
        self.etiqueta = etiqueta
        self.próximo = 2
        self.ahora = hora_dos(time.time())

    def pide(self, n, seguidos=True):
        """n cúmulos: seguidos, o a trozos (uno sí y otro no)"""
        r = []
        c = self.próximo
        while len(r) < n:
            if c not in self.usados:
                r.append(c)
                self.usados.add(c)
                c += 1 if seguidos else 2
            else:
                c += 1
        self.próximo = max(self.usados) + 1
        return r

    def enlaza(self, cs):
        for a, b in zip(cs, cs[1:]):
            self.fat[a] = b
        self.fat[cs[-1]] = 0xFFFFFFFF

    def pos(self, c):
        return self.heap * SECTOR + (c - 2) * CÚMULO

    def escribe(self, cs, datos):
        for i, c in enumerate(cs):
            trozo = datos[i * CÚMULO:(i + 1) * CÚMULO]
            self.img[self.pos(c):self.pos(c) + len(trozo)] = trozo

    def grupo(self, nombre, atrib, primero, largo, sin_cadena):
        u = [ord(ch) for ch in nombre]
        h = 0
        for x in u:
            x = mayúscula(x)
            h = (((h & 1) << 15) | (h >> 1)) + (x & 0xFF)
            h &= 0xFFFF
            h = (((h & 1) << 15) | (h >> 1)) + (x >> 8)
            h &= 0xFFFF
        n_nombre = (len(u) + 14) // 15
        e = bytearray(32 * (2 + n_nombre))
        e[0] = 0x85
        e[1] = 1 + n_nombre
        struct.pack_into('<H', e, 4, atrib)
        struct.pack_into('<III', e, 8, self.ahora, self.ahora, self.ahora)
        s = e[32:64]
        s[0] = 0xC0
        s[1] = 0x01 | (0x02 if sin_cadena else 0)
        s[3] = len(u)
        struct.pack_into('<H', s, 4, h)
        struct.pack_into('<Q', s, 8, largo)
        struct.pack_into('<I', s, 20, primero)
        struct.pack_into('<Q', s, 24, largo)
        e[32:64] = s
        for k in range(n_nombre):
            f = bytearray(32)
            f[0] = 0xC1
            for j, x in enumerate(u[k * 15:(k + 1) * 15]):
                struct.pack_into('<H', f, 2 + j * 2, x)
            e[64 + k * 32:96 + k * 32] = f
        struct.pack_into('<H', e, 2, suma16(e, (2, 3)))
        return bytes(e)

    def haz(self, archivos):
        # el mapa de bits, la tabla de mayúsculas y la raíz
        mapa_oct = (self.cúmulos + 7) // 8
        c_mapa = self.pide((mapa_oct + CÚMULO - 1) // CÚMULO)
        self.enlaza(c_mapa)
        tabla = b''.join(struct.pack('<H', mayúscula(u)) for u in range(0x10000))
        c_tabla = self.pide(len(tabla) // CÚMULO)
        self.enlaza(c_tabla)
        self.escribe(c_tabla, tabla)
        suma_tabla = suma32(tabla, ())
        # las carpetas: ruta -> lista de entradas
        carpetas = {'': []}
        for ruta in sorted({r.rsplit('/', 1)[0] for r, _ in archivos if '/' in r}):
            partes = ruta.split('/')
            for i in range(1, len(partes) + 1):
                carpetas.setdefault('/'.join(partes[:i]), [])
        # los archivos
        for ruta, datos in archivos:
            frag = ruta.endswith('.frag')
            if frag:
                ruta = ruta[:-5]
            padre, _, nombre = ruta.rpartition('/')
            n = max(1, (len(datos) + CÚMULO - 1) // CÚMULO)
            if not datos:
                primero, sin_cadena = 0, False
            else:
                cs = self.pide(n, seguidos=not frag)
                self.escribe(cs, datos)
                if frag:
                    self.enlaza(cs)
                primero, sin_cadena = cs[0], not frag
            carpetas[padre].append(self.grupo(nombre, 0x20, primero, len(datos), sin_cadena))
        # las carpetas, de las más hondas a la raíz (un cúmulo cada una, seguidas)
        c_raíz = None
        for ruta in sorted(carpetas, key=lambda r: -r.count('/') if r else 1):
            if ruta == '':
                continue
            datos = b''.join(carpetas[ruta])
            cs = self.pide(max(1, (len(datos) + CÚMULO - 1) // CÚMULO))
            self.escribe(cs, datos)
            padre, _, nombre = ruta.rpartition('/')
            carpetas[padre].append(self.grupo(nombre, 0x10, cs[0], len(cs) * CÚMULO, True))
        etq = bytearray(32)
        etq[0] = 0x83
        etq[1] = len(self.etiqueta)
        for j, ch in enumerate(self.etiqueta):
            struct.pack_into('<H', etq, 2 + j * 2, ord(ch))
        mapa_e = bytearray(32)
        mapa_e[0] = 0x81
        struct.pack_into('<IQ', mapa_e, 20, c_mapa[0], mapa_oct)
        tabla_e = bytearray(32)
        tabla_e[0] = 0x82
        struct.pack_into('<I', tabla_e, 4, suma_tabla)
        struct.pack_into('<IQ', tabla_e, 20, c_tabla[0], len(tabla))
        raíz = bytes(etq) + bytes(mapa_e) + bytes(tabla_e) + b''.join(carpetas[''])
        c_raíz = self.pide(max(1, (len(raíz) + CÚMULO - 1) // CÚMULO))
        self.enlaza(c_raíz)
        self.escribe(c_raíz, raíz)
        # el mapa de bits
        mapa = bytearray(mapa_oct)
        for c in self.usados:
            mapa[(c - 2) // 8] |= 1 << ((c - 2) % 8)
        self.escribe(c_mapa, bytes(mapa))
        # la FAT
        f = self.fat_ini * SECTOR
        self.img[f:f + len(self.fat) * 4] = struct.pack('<%dI' % len(self.fat), *self.fat)
        # el arranque y su copia
        b = bytearray(SECTOR)
        b[0:3] = b'\xEB\x76\x90'
        b[3:11] = b'EXFAT   '
        struct.pack_into('<QQIIIIIIHHBBBB', b, 64, self.desplazamiento, self.sectores, self.fat_ini, self.fat_largo,
                         self.heap, self.cúmulos, c_raíz[0], 0x12345678, 0x0100, 0, 9, SPC_SHIFT, 1, 0x80)
        b[112] = len(self.usados) * 100 // self.cúmulos
        b[510:512] = b'\x55\xAA'
        región = bytearray(SECTOR * 11)
        región[0:SECTOR] = b
        for s in range(1, 9):
            región[s * SECTOR + 510:s * SECTOR + 512] = b'\x55\xAA'
        suma = suma32(región, (106, 107, 112))
        sector_suma = struct.pack('<I', suma) * (SECTOR // 4)
        for base in (0, 12):
            self.img[base * SECTOR:(base + 11) * SECTOR] = región
            self.img[(base + 11) * SECTOR:(base + 12) * SECTOR] = sector_suma
        return bytes(self.img)


def main():
    if len(sys.argv) < 5:
        print(__doc__ or 'uso: hazexfat.py SALIDA OCTETOS DESPLAZAMIENTO ETIQUETA RUTA=ARCHIVO...', file=sys.stderr)
        sys.exit(2)
    salida, octetos, despl, etiqueta = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4]
    archivos = []
    for a in sys.argv[5:]:
        ruta, _, origen = a.partition('=')
        with open(origen, 'rb') as f:
            archivos.append((ruta, f.read()))
    v = Volumen(octetos, despl, etiqueta)
    with open(salida, 'wb') as f:
        f.write(v.haz(archivos))
    print('exFAT: %d cúmulos de %d octetos, %d usados' % (v.cúmulos, CÚMULO, len(v.usados)))


if __name__ == '__main__':
    main()
