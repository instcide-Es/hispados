#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# amlver.py - (I2CHID) un desensamblador pequeño de AML (las tablas ACPI DSDT
# y SSDT), para mirar lo que hacen los métodos de un equipo sin iasl: lo
# escribe parecido a ASL. No lo sabe todo (lo raro sale como «?XX»), pero sí
# los nombres, los dispositivos, los métodos con sus llamadas, los If, los
# Store, los paquetes y los ResourceTemplate (I2cSerialBusV2, GpioInt,
# Interrupt, Memory32Fixed...), que es lo que hace falta para ver de dónde
# sale un touchpad HID sobre I2C.
#
#   amlver.py [-b NOMBRE] TABLA.BIN [MÁS.BIN ...]
#
# Con -b, solo los bloques (Device, Method, Scope...) cuyo nombre lleve NOMBRE
# (p. ej. -b TPD0). Las demás tablas se leen para saber cuántos argumentos
# lleva cada método.
import struct
import sys

UNO = {0x00: 'Zero', 0x01: 'One', 0xFF: 'Ones'}
LOCAL = {0x60 + i: 'Local%d' % i for i in range(8)}
ARG = {0x68 + i: 'Arg%d' % i for i in range(7)}
# operador: (nombre, nº de operandos, nº de destinos)
OPS = {
    0x70: ('Store', 1, 1), 0x71: ('RefOf', 1, 0), 0x72: ('Add', 2, 1), 0x73: ('Concatenate', 2, 1),
    0x74: ('Subtract', 2, 1), 0x75: ('Increment', 1, 0), 0x76: ('Decrement', 1, 0), 0x77: ('Multiply', 2, 1),
    0x78: ('Divide', 2, 2), 0x79: ('ShiftLeft', 2, 1), 0x7A: ('ShiftRight', 2, 1), 0x7B: ('And', 2, 1),
    0x7C: ('NAnd', 2, 1), 0x7D: ('Or', 2, 1), 0x7E: ('NOr', 2, 1), 0x7F: ('XOr', 2, 1), 0x80: ('Not', 1, 1),
    0x81: ('FindSetLeftBit', 1, 1), 0x82: ('FindSetRightBit', 1, 1), 0x83: ('DerefOf', 1, 0),
    0x84: ('ConcatenateResTemplate', 2, 1), 0x85: ('Mod', 2, 1), 0x86: ('Notify', 2, 0), 0x87: ('SizeOf', 1, 0),
    0x88: ('Index', 2, 1), 0x8E: ('ObjectType', 1, 0), 0x90: ('LAnd', 2, 0), 0x91: ('LOr', 2, 0),
    0x92: ('LNot', 1, 0), 0x93: ('LEqual', 2, 0), 0x94: ('LGreater', 2, 0), 0x95: ('LLess', 2, 0),
    0x96: ('ToBuffer', 1, 1), 0x97: ('ToDecimalString', 1, 1), 0x98: ('ToHexString', 1, 1),
    0x99: ('ToInteger', 1, 1), 0x9C: ('ToString', 2, 1), 0x9D: ('CopyObject', 1, 1), 0x9E: ('Mid', 3, 1),
    0xA4: ('Return', 1, 0), 0x8A: ('CreateDWordField', 3, 0), 0x8B: ('CreateWordField', 3, 0),
    0x8C: ('CreateByteField', 3, 0), 0x8D: ('CreateBitField', 3, 0), 0x8F: ('CreateQWordField', 3, 0),
    0xCC: ('BreakPoint', 0, 0), 0xA3: ('Noop', 0, 0), 0xA5: ('Break', 0, 0), 0x9F: ('Continue', 0, 0),
}
EXT = {
    0x12: ('CondRefOf', 1, 1), 0x13: ('CreateField', 4, 0), 0x21: ('Stall', 1, 0), 0x22: ('Sleep', 1, 0),
    0x23: ('Acquire', 1, 0), 0x24: ('Signal', 1, 0), 0x25: ('Wait', 2, 0), 0x26: ('Reset', 1, 0),
    0x27: ('Release', 1, 0), 0x28: ('FromBCD', 1, 1), 0x29: ('ToBCD', 1, 1), 0x30: ('Revision', 0, 0),
    0x31: ('Debug', 0, 0), 0x33: ('Timer', 0, 0),
}


class Aml:
    def __init__(self, d, metodos):
        self.d = d
        self.m = metodos

    def pkglen(self, p):
        b = self.d[p]
        n = b >> 6
        if n == 0:
            return b & 0x3F, p + 1
        v = b & 0x0F
        for i in range(n):
            v |= self.d[p + 1 + i] << (4 + 8 * i)
        return v, p + 1 + n

    def nameseg(self, p):
        return self.d[p:p + 4].decode('latin-1').rstrip('_') or '_', p + 4

    def namestring(self, p):
        s = ''
        while self.d[p] in (0x5C, 0x5E):
            s += chr(self.d[p])
            p += 1
        b = self.d[p]
        if b == 0x00:
            return s, p + 1
        if b == 0x2E:
            a, p = self.nameseg(p + 1)
            c, p = self.nameseg(p)
            return s + a + '.' + c, p
        if b == 0x2F:
            n = self.d[p + 1]
            p += 2
            segs = []
            for _ in range(n):
                a, p = self.nameseg(p)
                segs.append(a)
            return s + '.'.join(segs), p
        a, p = self.nameseg(p)
        return s + a, p

    def es_nombre(self, b):
        return b in (0x5C, 0x5E, 0x2E, 0x2F, 0x5F) or 0x41 <= b <= 0x5A

    def term(self, p):
        d = self.d
        b = d[p]
        if b in UNO:
            return UNO[b], p + 1
        if b == 0x0A:
            return '0x%02X' % d[p + 1], p + 2
        if b == 0x0B:
            return '0x%04X' % struct.unpack_from('<H', d, p + 1)[0], p + 3
        if b == 0x0C:
            return '0x%08X' % struct.unpack_from('<I', d, p + 1)[0], p + 5
        if b == 0x0E:
            return '0x%016X' % struct.unpack_from('<Q', d, p + 1)[0], p + 9
        if b == 0x0D:
            e = d.index(0, p + 1)
            return '"%s"' % d[p + 1:e].decode('latin-1'), e + 1
        if b in LOCAL:
            return LOCAL[b], p + 1
        if b in ARG:
            return ARG[b], p + 1
        if b == 0x11:
            n, q = self.pkglen(p + 1)
            fin = p + 1 + n
            t, q = self.term(q)
            datos = d[q:fin]
            return self.buffer(t, datos), fin
        if b in (0x12, 0x13):
            n, q = self.pkglen(p + 1)
            fin = p + 1 + n
            if b == 0x12:
                cuenta = d[q]
                q += 1
            else:
                cuenta, q = self.term(q)
            el = []
            while q < fin:
                t, q = self.term(q)
                el.append(t)
            return 'Package (%s) {%s}' % (cuenta, ', '.join(el)), fin
        if b in OPS:
            nom, n, t = OPS[b]
            q = p + 1
            a = []
            for _ in range(n + t):
                x, q = self.term(q)
                a.append(x)
            return '%s (%s)' % (nom, ', '.join(a)), q
        if b == 0x92 and d[p + 1] in (0x93, 0x94, 0x95):
            nom = {0x93: 'LNotEqual', 0x94: 'LLessEqual', 0x95: 'LGreaterEqual'}[d[p + 1]]
            x, q = self.term(p + 2)
            y, q = self.term(q)
            return '%s (%s, %s)' % (nom, x, y), q
        if b == 0x5B and d[p + 1] in EXT:
            nom, n, t = EXT[d[p + 1]]
            q = p + 2
            a = []
            if d[p + 1] == 0x23:
                x, q = self.term(q)
                a = [x, '0x%04X' % struct.unpack_from('<H', d, q)[0]]
                q += 2
            else:
                for _ in range(n + t):
                    x, q = self.term(q)
                    a.append(x)
            return '%s (%s)' % (nom, ', '.join(a)), q
        if b == 0x89:
            q = p + 1
            x, q = self.term(q)
            o1 = d[q]
            y, q = self.term(q + 1)
            o2 = d[q]
            z, q = self.term(q + 1)
            w, q = self.term(q)
            return 'Match (%s, %d, %s, %d, %s, %s)' % (x, o1, y, o2, z, w), q
        if self.es_nombre(b):
            n, q = self.namestring(p)
            ult = n.split('.')[-1].lstrip('\\^')
            argc = self.m.get(n.lstrip('\\^'), self.m.get(ult, 0))
            if argc:
                a = []
                for _ in range(argc):
                    x, q = self.term(q)
                    a.append(x)
                return '%s (%s)' % (n, ', '.join(a)), q
            return n, q
        return '?%02X' % b, p + 1

    def buffer(self, t, datos):
        r = recursos(datos)
        if r is not None:
            return 'ResourceTemplate () {\n' + ''.join('        ' + x + '\n' for x in r) + '    }'
        if len(datos) <= 32:
            return 'Buffer (%s) {%s}' % (t, ', '.join('0x%02X' % x for x in datos))
        return 'Buffer (%s) {%d octetos}' % (t, len(datos))

    def bloque(self, p, fin, sang, filtro, dentro):
        d = self.d
        out = []
        while p < fin:
            b = d[p]
            ini = p
            try:
                if b == 0x10 or b == 0x14 or (b == 0x5B and d[p + 1] in (0x82, 0x84, 0x85, 0x83)):
                    ext = b == 0x5B
                    q = p + 2 if ext else p + 1
                    n, q2 = self.pkglen(q)
                    fb = q + n
                    nom, q2 = self.namestring(q2)
                    if b == 0x14:
                        fl = d[q2]
                        q2 += 1
                        cab = 'Method (%s, %d%s)' % (nom, fl & 7, ', Serialized' if fl & 8 else '')
                    elif b == 0x10:
                        cab = 'Scope (%s)' % nom
                    elif d[p + 1] == 0x82:
                        cab = 'Device (%s)' % nom
                    elif d[p + 1] == 0x84:
                        q2 += 3
                        cab = 'PowerResource (%s)' % nom
                    elif d[p + 1] == 0x83:
                        q2 += 6
                        cab = 'Processor (%s)' % nom
                    else:
                        cab = 'ThermalZone (%s)' % nom
                    mio = dentro or (filtro is None) or (filtro in nom)
                    sub = self.bloque(q2, fb, sang + 1, filtro, mio)
                    if mio:
                        out.append('    ' * sang + cab + ' {')
                        out.extend(sub)
                        out.append('    ' * sang + '}')
                    else:
                        out.extend(sub)
                    p = fb
                    continue
                if b in (0xA0, 0xA1, 0xA2):
                    n, q = self.pkglen(p + 1)
                    fb = p + 1 + n
                    if b == 0xA1:
                        cab = 'Else'
                    else:
                        c, q = self.term(q)
                        cab = ('If (%s)' if b == 0xA0 else 'While (%s)') % c
                    sub = self.bloque(q, fb, sang + 1, filtro, dentro)
                    if dentro:
                        out.append('    ' * sang + cab + ' {')
                        out.extend(sub)
                        out.append('    ' * sang + '}')
                    p = fb
                    continue
                if b == 0x08:
                    nom, q = self.namestring(p + 1)
                    v, q = self.term(q)
                    if dentro:
                        out.append('    ' * sang + 'Name (%s, %s)' % (nom, v))
                    p = q
                    continue
                if b == 0x5B and d[p + 1] == 0x80:
                    nom, q = self.namestring(p + 2)
                    sp = d[q]
                    a, q = self.term(q + 1)
                    l, q = self.term(q)
                    if dentro:
                        out.append('    ' * sang + 'OperationRegion (%s, %d, %s, %s)' % (nom, sp, a, l))
                    p = q
                    continue
                if b == 0x5B and d[p + 1] in (0x81, 0x86, 0x87):
                    n, q = self.pkglen(p + 2)
                    fb = p + 2 + n
                    nom, q = self.namestring(q)
                    if d[p + 1] == 0x86:
                        n2, q = self.namestring(q)
                        nom += ', ' + n2
                    elif d[p + 1] == 0x87:
                        n2, q = self.namestring(q)
                        v, q = self.term(q)
                        nom += ', %s, %s' % (n2, v)
                    q += 1
                    campos = []
                    bit = 0
                    while q < fb:
                        c = d[q]
                        if c == 0x00:
                            n3, q = self.pkglen(q + 1)
                            bit += n3
                        elif c == 0x01:
                            q += 3
                        elif c == 0x03:
                            q += 4
                        elif c == 0x02:
                            x, q = self.term(q + 1)
                        else:
                            s, q = self.nameseg(q)
                            n3, q = self.pkglen(q)
                            campos.append('%s@%d:%d' % (s, bit, n3))
                            bit += n3
                    if dentro:
                        out.append('    ' * sang + 'Field (%s) {%s}' % (nom, ', '.join(campos)))
                    p = fb
                    continue
                if b == 0x5B and d[p + 1] == 0x01:
                    nom, q = self.namestring(p + 2)
                    if dentro:
                        out.append('    ' * sang + 'Mutex (%s)' % nom)
                    p = q + 1
                    continue
                if b == 0x5B and d[p + 1] == 0x02:
                    nom, q = self.namestring(p + 2)
                    p = q
                    continue
                if b == 0x06:
                    a, q = self.namestring(p + 1)
                    c, q = self.namestring(q)
                    p = q
                    continue
                if b == 0x15:
                    nom, q = self.namestring(p + 1)
                    p = q + 2
                    continue
                t, q = self.term(p)
                if dentro:
                    out.append('    ' * sang + t)
                p = q if q > p else p + 1
            except (IndexError, ValueError, struct.error):
                if dentro:
                    out.append('    ' * sang + '?? (en %Xh)' % ini)
                p = ini + 1
        return out


def recursos(b):
    """Un ResourceTemplate (acaba en 79h): sus descriptores; None si no lo es."""
    if len(b) < 2 or b[-2] != 0x79:
        return None
    r = []
    p = 0
    try:
        while p < len(b):
            t = b[p]
            if t & 0x80:
                n = struct.unpack_from('<H', b, p + 1)[0]
                d = b[p + 3:p + 3 + n]
                k = t & 0x7F
                if k == 0x0E:
                    tipo = d[2]
                    if tipo == 1:
                        tfl = struct.unpack_from('<H', d, 4)[0]
                        ln = struct.unpack_from('<H', d, 7)[0]
                        vel, esc = struct.unpack_from('<IH', d, 9)
                        fuente = d[9 + ln:].split(b'\0')[0].decode('latin-1')
                        r.append('I2cSerialBusV2 (0x%02X, %s, %d, %s, "%s")' % (
                            esc, 'ControllerInitiated' if not d[3] & 1 else 'DeviceInitiated', vel,
                            'AddressingMode10Bit' if tfl & 1 else 'AddressingMode7Bit', fuente))
                    else:
                        r.append('SerialBus (tipo %d)' % tipo)
                elif k == 0x0C:
                    tipo = d[1]
                    fl = struct.unpack_from('<H', d, 4)[0]
                    pins_off, = struct.unpack_from('<H', d, 11)
                    src_off, = struct.unpack_from('<H', d, 14)
                    pines = []
                    q = pins_off - 3
                    while q + 1 < src_off - 3:
                        pines.append(struct.unpack_from('<H', d, q)[0])
                        q += 2
                    fuente = d[src_off - 3:].split(b'\0')[0].decode('latin-1')
                    if tipo == 0:
                        r.append('GpioInt (%s, %s, %s, ..., "%s") {%s}' % (
                            'Edge' if fl & 1 else 'Level', ['ActiveHigh', 'ActiveLow', 'ActiveBoth'][(fl >> 1) & 3],
                            'Shared' if fl & 8 else 'Exclusive', fuente, ', '.join('0x%04X' % x for x in pines)))
                    else:
                        r.append('GpioIo (..., "%s") {%s}' % (fuente, ', '.join('0x%04X' % x for x in pines)))
                elif k == 0x09:
                    fl = d[0]
                    cnt = d[1]
                    irqs = struct.unpack_from('<%dI' % cnt, d, 2)
                    r.append('Interrupt (%s, %s, %s) {%s}' % (
                        'ResourceConsumer' if fl & 1 else 'ResourceProducer', 'Edge' if fl & 2 else 'Level',
                        'ActiveLow' if fl & 4 else 'ActiveHigh', ', '.join('0x%X' % x for x in irqs)))
                elif k == 0x06:
                    a, l = struct.unpack_from('<II', d, 1)
                    r.append('Memory32Fixed (%s, 0x%08X, 0x%08X)' % ('ReadWrite' if d[0] & 1 else 'ReadOnly', a, l))
                else:
                    r.append('(descriptor largo %02Xh, %d octetos)' % (k, n))
                p += 3 + n
            else:
                k = (t >> 3) & 0xF
                n = t & 7
                if k == 0xF:
                    break
                r.append('(descriptor corto %Xh, %d octetos)' % (k, n))
                p += 1 + n
    except (IndexError, struct.error):
        return None
    return r


def metodos(tablas):
    """Los métodos (y cuántos argumentos llevan) de todas las tablas."""
    m = {}
    for d in tablas:
        p = 36
        while True:
            p = d.find(b'\x14', p)
            if p < 0:
                break
            try:
                a = Aml(d, {})
                n, q = a.pkglen(p + 1)
                nom, q2 = a.namestring(q)
                fl = d[q2]
                if p + 1 + n <= len(d) and nom and all(c.isalnum() or c in '_.\\^' for c in nom):
                    m[nom.lstrip('\\^')] = fl & 7
                    m.setdefault(nom.split('.')[-1], fl & 7)
            except (IndexError, ValueError):
                pass
            p += 1
        # (y los External: 15h nombre tipo argumentos)
    return m


def main():
    args = sys.argv[1:]
    filtro = None
    if len(args) >= 2 and args[0] == '-b':
        filtro = args[1]
        args = args[2:]
    if not args:
        print(__doc__ or 'uso: amlver.py [-b NOMBRE] TABLA.BIN ...')
        return 2
    tablas = [open(f, 'rb').read() for f in args]
    m = metodos(tablas)
    for f, d in zip(args, tablas):
        a = Aml(d, m)
        lineas = a.bloque(36, len(d), 0, filtro, filtro is None)
        if lineas:
            print('// %s (%s, %d octetos)' % (f, d[:4].decode('latin-1'), len(d)))
            print('\n'.join(lineas))
    return 0


if __name__ == '__main__':
    sys.exit(main())
