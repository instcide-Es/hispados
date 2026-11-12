#!/usr/bin/env python3
"""Ayudante de las pruebas de REVISAR: lee una imagen FAT12/FAT16 (disquete
o partición, IMG[@desplazamiento]), calcula el informe que debería salir y
la estropea a propósito.

uso: fatimg.py IMG[@OFF] orden [args]
  info                      números del informe (una línea por dato)
  lost N TEXTO              cadena perdida de N clústeres con TEXTO repetido
  setsize RUTA BYTES        cambia el tamaño en la entrada del directorio
  crosslink RUTA1 RUTA2     el primer clúster de RUTA2 apunta al último de RUTA1
  dotdot RUTA CLUSTER       pone CLUSTER en la entrada «..» de la carpeta RUTA
  fragment RUTA             mueve el 2.º clúster del archivo al final del disco
  chain RUTA                imprime la cadena de clústeres
"""
import struct, sys


class Fat:
    def __init__(self, spec):
        self.path, _, off = spec.partition('@')
        self.off = int(off or 0)
        self.f = open(self.path, 'r+b')
        b = self.rd(0, 512)
        (self.bps, self.spc, self.res, self.nfats, self.rootent, tot16, self.media,
         self.spf) = struct.unpack_from('<HBHBHHBH', b, 11)
        self.tot = tot16 or struct.unpack_from('<I', b, 32)[0]
        self.root = self.res + self.nfats * self.spf
        self.rootsecs = (self.rootent * 32 + 511) // 512
        self.data = self.root + self.rootsecs
        self.nclus = (self.tot - self.data) // self.spc
        self.fat16 = self.nclus >= 4085
        self.bpc = self.spc * 512
        self.fat = bytearray(self.rd(self.res * 512, self.spf * 512))

    def rd(self, pos, n):
        self.f.seek(self.off + pos)
        return self.f.read(n)

    def wr(self, pos, data):
        self.f.seek(self.off + pos)
        self.f.write(data)

    def get(self, c):
        if self.fat16:
            return struct.unpack_from('<H', self.fat, c * 2)[0]
        v = struct.unpack_from('<H', self.fat, c * 3 // 2)[0]
        v = v >> 4 if c & 1 else v & 0xFFF
        return v | 0xF000 if v >= 0xFF0 else v

    def set(self, c, v):
        if self.fat16:
            struct.pack_into('<H', self.fat, c * 2, v & 0xFFFF)
        else:
            o = c * 3 // 2
            w = struct.unpack_from('<H', self.fat, o)[0]
            v &= 0xFFF
            w = (w & 0x000F) | (v << 4) if c & 1 else (w & 0xF000) | v
            struct.pack_into('<H', self.fat, o, w)

    def save_fat(self):
        for i in range(self.nfats):
            self.wr((self.res + i * self.spf) * 512, self.fat)

    def clus_lba(self, c):
        return self.data + (c - 2) * self.spc

    def chain(self, c):
        out = []
        while 2 <= c <= self.nclus + 1 and c not in out:
            out.append(c)
            c = self.get(c)
            if c >= 0xFFF8:
                break
        return out

    def dir_sectors(self, first):
        if first == 0:
            return list(range(self.root, self.root + self.rootsecs))
        return [self.clus_lba(c) + i for c in self.chain(first) for i in range(self.spc)]

    def entries(self, first):
        """(sector, desplazamiento, entrada de 32 bytes) de una carpeta"""
        for s in self.dir_sectors(first):
            b = self.rd(s * 512, 512)
            for o in range(0, 512, 32):
                e = b[o:o + 32]
                if e[0] == 0:
                    return
                yield s, o, e

    @staticmethod
    def name(e):
        n = e[0:8].decode('cp437').rstrip()
        x = e[8:11].decode('cp437').rstrip()
        return n + ('.' + x if x else '')

    def find(self, path):
        first, hit = 0, None
        for part in path.strip('\\').upper().split('\\'):
            hit = None
            for s, o, e in self.entries(first):
                if e[0] != 0xE5 and e[11] != 0x0F and not e[11] & 8 and self.name(e) == part:
                    hit = (s, o, e)
                    break
            if hit is None:
                raise SystemExit('no existe: ' + path)
            first = struct.unpack_from('<H', hit[2], 26)[0]
        return hit

    def walk(self, first=0):
        for s, o, e in self.entries(first):
            if e[0] == 0xE5 or e[11] == 0x0F or e[11] & 8 or e[0] == ord('.'):
                continue
            yield e
            if e[11] & 0x10:
                yield from self.walk(struct.unpack_from('<H', e, 26)[0])

    def info(self):
        hid = dirs = usr = 0
        bh = bd = bu = 0
        for e in self.walk():
            n = len(self.chain(struct.unpack_from('<H', e, 26)[0])) * self.bpc
            if e[11] & 0x10:
                dirs += 1; bd += n
            elif e[11] & 2:
                hid += 1; bh += n
            else:
                usr += 1; bu += n
        free = sum(1 for c in range(2, self.nclus + 2) if self.get(c) == 0)
        bad = sum(1 for c in range(2, self.nclus + 2) if self.get(c) == 0xFFF7)
        print('total', self.nclus * self.bpc)
        print('hidden', hid, bh)
        print('dirs', dirs, bd)
        print('user', usr, bu)
        print('bad', bad * self.bpc)
        print('avail', free * self.bpc)
        print('bpc', self.bpc)
        print('units', self.nclus)
        print('freeunits', free)


def main():
    fs = Fat(sys.argv[1])
    cmd, a = sys.argv[2], sys.argv[3:]
    if cmd == 'info':
        fs.info()
    elif cmd == 'lost':
        n, text = int(a[0]), a[1].encode()
        free = [c for c in range(2, fs.nclus + 2) if fs.get(c) == 0]
        cl = free[-n:]                  # del final del disco
        for i, c in enumerate(cl):
            fs.set(c, cl[i + 1] if i + 1 < n else 0xFFFF)
            data = (text * (fs.bpc // len(text) + 1))[:fs.bpc]
            fs.wr(fs.clus_lba(c) * 512, data)
        fs.save_fat()
        print(' '.join(map(str, cl)))
    elif cmd == 'setsize':
        s, o, e = fs.find(a[0])
        fs.wr(s * 512 + o + 28, struct.pack('<I', int(a[1])))
    elif cmd == 'crosslink':
        c1 = fs.chain(struct.unpack_from('<H', fs.find(a[0])[2], 26)[0])
        c2 = fs.chain(struct.unpack_from('<H', fs.find(a[1])[2], 26)[0])
        fs.set(c2[0], c1[-1])
        fs.save_fat()
        print(c1[-1])
    elif cmd == 'dotdot':
        s, o, e = fs.find(a[0])
        first = struct.unpack_from('<H', e, 26)[0]
        sec = fs.clus_lba(first)
        fs.wr(sec * 512 + 32 + 26, struct.pack('<H', int(a[1])))
    elif cmd == 'fragment':
        # el segundo clúster del archivo, a un hueco libre del final
        s, o, e = fs.find(a[0])
        ch = fs.chain(struct.unpack_from('<H', e, 26)[0])
        new = [c for c in range(2, fs.nclus + 2) if fs.get(c) == 0][-1]
        old = ch[1]
        fs.wr(fs.clus_lba(new) * 512, fs.rd(fs.clus_lba(old) * 512, fs.bpc))
        fs.set(new, fs.get(old))
        fs.set(ch[0], new)
        fs.set(old, 0)
        fs.save_fat()
    elif cmd == 'chain':
        print(' '.join(map(str, fs.chain(struct.unpack_from('<H', fs.find(a[0])[2], 26)[0]))))
    else:
        raise SystemExit(__doc__)


if __name__ == '__main__':
    main()
