#!/usr/bin/env python3
# Hispa-DOS - simulador de la tarjeta Marvell Yukon 2 «Optima» (88E8059) para
# probar HSPRED sin el equipo de verdad (QEMU no emula esta tarjeta).
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
#
# HSPRED.COM corre tal cual, en modo real, dentro de una máquina virtual
# mínima hecha con KVM (/dev/kvm) desde este mismo programa: 1 MB de memoria,
# el PIC y el temporizador del núcleo, y nada más. Lo que HSPRED le pide al
# DOS, a la BIOS y a la BIOS PCI (INT 21h, 1Ah...) lo contesta este programa
# (cada vector apunta a un «out» que saca el control aquí), y los registros
# de la tarjeta (en memoria, 16 KB, o por sus 256 puertos con RAP) los
# contesta un modelo del chip escrito con lo que se sabe por sky2 de Linux:
# PHY por SMI, unidades de precarga con sus «elementos de lista», anillo de
# estado e interrupción. El modelo, además, vigila lo que hace el
# controlador (anillos alineados a 4 KB, elementos con el bit de «vale»,
# índices dentro del anillo, escrituras con el chip en reinicio...).
#
# Sirve para cazar los fallos del ensamblador (registros pisados, pila,
# desplazamientos, orden de las operaciones); NO demuestra que el chip de
# verdad se porte como el modelo: eso, solo en el HP Mini.
#
# Uso: yukon_sim.py build/HSPRED.COM   (lo llama tests/run_yukon_tests.sh)
# Marvell Yukon 2 simulator (KVM real-mode harness + chip model) for HSPRED.
import ctypes
import fcntl
import mmap
import os
import signal
import struct
import sys

KVM_CREATE_VM = 0xAE01
KVM_CREATE_VCPU = 0xAE41
KVM_GET_VCPU_MMAP_SIZE = 0xAE04
KVM_SET_USER_MEMORY_REGION = 0x4020AE46
KVM_SET_TSS_ADDR = 0xAE47
KVM_CREATE_IRQCHIP = 0xAE60
KVM_IRQ_LINE = 0x4008AE61
KVM_CREATE_PIT2 = 0x4040AE77
KVM_RUN = 0xAE80
KVM_GET_REGS = 0x8090AE81
KVM_SET_REGS = 0x4090AE82
KVM_GET_SREGS = 0x8138AE83
KVM_SET_SREGS = 0x4138AE84
EXIT_IO, EXIT_HLT, EXIT_MMIO = 2, 5, 6

MEM = 0x110000
STUB = 0xF000                   # segmento de los «out» de cada vector
MMIO_BASE = 0xFEB00000
IO_BASE = 0x3000
IRQ = 11
BDF = 0x0100
MAC = bytes.fromhex('0026553a1c9e')
REGS = ('ax', 'bx', 'cx', 'dx', 'si', 'di', 'sp', 'bp')
ROFF = dict(ax=0, bx=1, cx=2, dx=3, si=4, di=5, sp=6, bp=7, ip=16, fl=17)
SOFF = dict(cs=0, ds=1, es=2, fs=3, gs=4, ss=5)


class Fallo(Exception):
    pass


class Yukon:
    """El chip: registros, PHY, colas y anillo de estado."""

    def __init__(self, vm, rev=1, cable=True, vel=1000):
        self.vm = vm
        self.r = bytearray(0x4000)
        self.cfg = bytearray(0x200)
        self.cable = cable
        self.vel = vel
        self.avisos = []
        self.rap = 0
        self.accesos_io = 0
        self.accesos_mem = 0
        self.imsk = 0
        self.isrc = 0
        self.masked = False
        self.sw_reset = True
        self.phy = [0] * 32
        self.phy[2], self.phy[3] = 0x0141, 0x0E90
        self.phy[1] = 0x7949
        self.phy_pag = 0
        self.aneg = -1              # lecturas que faltan para el enlace
        self.phy_reset = True
        self.st = dict(on=False, addr=0, last=0, put=0)
        self.q = {0x450: dict(on=False, addr=0, last=0, get=0, nombre='recepción'),
                  0x6D0: dict(on=False, addr=0, last=0, get=0, nombre='envío')}
        self.rxbufs = []
        self.enviadas = []
        self.pendientes = []
        self.tx_hi = 0
        self.rx_sin_bufer = 0
        self.mib = {}
        self.r[0x100:0x106] = MAC
        self.r[0x119] = ord('T')    # PMD: cobre
        self.r[0x11A] = rev << 4
        self.r[0x11B] = 0xBC        # Optima
        self.r[0x11C] = 0           # sin búfer RAM
        self.r[0x11E] = 0x01
        c = self.cfg
        struct.pack_into('<HHHHBBBB', c, 0, 0x11AB, 0x4381, 0x0000, 0x0010,
                         0x11, 0x00, 0x00, 0x02)
        struct.pack_into('<II', c, 0x10, MMIO_BASE | 4, 0)
        struct.pack_into('<I', c, 0x18, IO_BASE | 1)
        struct.pack_into('<HH', c, 0x2C, 0x103C, 0x1608)
        c[0x34] = 0x48
        c[0x3C], c[0x3D] = IRQ, 1
        struct.pack_into('<BBH', c, 0x48, 0x01, 0xE0, 0xFE03)  # gestión de energía
        struct.pack_into('<H', c, 0x4C, 0x0000)
        struct.pack_into('<BBH', c, 0xE0, 0x10, 0x00, 0x0012)  # PCI Express
        struct.pack_into('<I', c, 0x40, 0x04000000 | 0x8000)   # PHY apagado
        struct.pack_into('<I', c, 0x80, 0x0000FFFF)
        struct.pack_into('<I', c, 0x84, 0xFFFFFFFF)
        struct.pack_into('<I', c, 0x88, 0xFFFFFFFF)
        struct.pack_into('<H', c, 0x164, 0x0043)               # PSM con ASPM

    def aviso(self, t):
        if t not in self.avisos:
            self.avisos.append(t)

    # ---- interrupción ----
    def irq_act(self):
        on = bool(self.isrc & self.imsk) and not self.masked
        self.vm.irq_line(IRQ, on)

    # ---- PHY ----
    def phy_estado(self):
        if self.aneg > 0:
            self.aneg -= 1
        if self.aneg == 0 and self.cable and not (self.phy[17] & 0x0400):
            v = {1000: 0x8000, 100: 0x4000, 10: 0}[self.vel]
            self.phy[17] = v | 0x2000 | 0x0800 | 0x0400
            self.phy[1] |= 0x0024
            self.phy[19] |= 0x0C00
            self.phy_int()

    def phy_int(self):
        if self.phy[19] & self.phy[18]:
            self.isrc |= 0x10
        else:
            self.isrc &= ~0x10
        self.irq_act()

    def enlace(self, arriba):
        """Enchufar o quitar el cable con todo en marcha."""
        self.cable = arriba
        if arriba:
            self.aneg = 1
            self.phy_estado()
        else:
            self.phy[17] &= ~0x0C00
            self.phy[1] &= ~0x0024
            self.phy[19] |= 0x0400
            self.phy_int()

    def phy_rd(self, reg):
        if self.phy_reset:
            self.aviso('PHY leído con el PHY en reinicio (GPHY_CTRL)')
            return 0xFFFF
        if self.phy_pag not in (0,):
            return 0
        self.phy_estado()
        v = self.phy[reg]
        if reg == 19:
            self.phy[19] = 0
            self.phy_int()
        return v

    def phy_wr(self, reg, v):
        if self.phy_reset:
            self.aviso('PHY escrito con el PHY en reinicio (GPHY_CTRL)')
            return
        if reg == 22:
            self.phy_pag = v & 0xFF
            return
        if self.phy_pag != 0:
            return
        if reg == 0:
            if v & 0x0200 or v & 0x8000:
                self.aneg = 6 if v & 0x1000 else -1
                self.phy[17] &= ~0x0C00
            v &= ~0x8200
        self.phy[reg] = v
        if reg == 18:
            self.phy_int()

    # ---- anillo de estado ----
    def estado(self, status, length, css, op):
        s = self.st
        if not s['on']:
            self.aviso('elemento de estado con la unidad de estado apagada')
            return
        self.vm.wmem(s['addr'] + s['put'] * 8,
                     struct.pack('<IHBB', status & 0xFFFFFFFF, length, css, op | 0x80))
        s['put'] = (s['put'] + 1) % (s['last'] + 1)
        struct.pack_into('<H', self.r, 0xE9C, s['put'])
        self.isrc |= 1 << 30
        self.irq_act()

    # ---- colas ----
    def put_idx(self, base, idx):
        q = self.q[base]
        if not q['on']:
            self.aviso('índice put de %s con su precarga apagada' % q['nombre'])
            return
        if idx > q['last']:
            self.aviso('índice put de %s fuera del anillo (%d)' % (q['nombre'], idx))
            return
        if q['addr'] & 0xFFF:
            self.aviso('anillo de %s sin alinear a 4 KB (%08X)' % (q['nombre'], q['addr']))
        while q['get'] != idx:
            a = q['addr'] + q['get'] * 8
            addr, length, ctrl, op = struct.unpack('<IHBB', self.vm.rmem(a, 8))
            q['get'] = (q['get'] + 1) % (q['last'] + 1)
            if not op & 0x80:
                self.aviso('elemento de %s sin el bit de «vale» (código %02X)'
                           % (q['nombre'], op))
                continue
            op &= 0x7F
            if base == 0x450:
                if op == 0x41:
                    if addr & 7:
                        self.aviso('búfer de recepción sin alinear a 8')
                    self.rxbufs.append((addr, length))
                elif op != 0x21:
                    self.aviso('código raro en recepción: %02X' % op)
            else:
                if op == 0x21:
                    self.tx_hi = addr
                elif op == 0x41:
                    if not ctrl & 0x80:
                        self.aviso('trama de envío sin fin de trama (EOP)')
                    self.pendientes.append(self.vm.rmem(addr, length))
                else:
                    self.aviso('código raro en envío: %02X' % op)
        struct.pack_into('<H', self.r, base + 0x10, q['get'])
        if base == 0x6D0:
            self.tx_sale()

    def tx_sale(self):
        gp = struct.unpack_from('<H', self.r, 0x2804)[0]
        if not gp & 0x1000 or not self.pendientes:
            return
        for f in self.pendientes:
            self.enviadas.append(f)
            k = 0x29C8 if f[:6] == b'\xff' * 6 else 0x29C0
            self.mib[k] = self.mib.get(k, 0) + 1
        self.pendientes = []
        idx = self.q[0x6D0]['get']
        struct.pack_into('<H', self.r, 0x6A4, idx)      # Q_DONE
        struct.pack_into('<H', self.r, 0xE90, idx)      # STAT_TXA1_RIDX
        self.estado(idx & 0xFFF, 0, 0, 0x68)

    def recibe(self, trama, estado=0x100, basura=False):
        """Una trama llega por el cable."""
        gp = struct.unpack_from('<H', self.r, 0x2804)[0]
        if not gp & 0x0800:
            return False
        rxc = struct.unpack_from('<H', self.r, 0x280C)[0]
        if rxc & 0x8000 and not trama[0] & 1 and trama[:6] != MAC:
            return False
        if not self.rxbufs:
            self.rx_sin_bufer += 1
            return False
        addr, blen = self.rxbufs.pop(0)
        n = len(trama)
        if n > blen:
            self.aviso('trama más larga que el búfer')
            n = blen
        self.vm.wmem(addr, trama[:n])
        k = 0x2908 if trama[:6] == b'\xff' * 6 else (0x2918 if trama[0] & 1 else 0x2900)
        self.mib[k] = self.mib.get(k, 0) + 1
        if basura:                  # elementos que el controlador no usa
            self.estado(0x12345678, 0, 0, 0x65)
        self.estado((len(trama) << 16) | estado, n, 0x02, 0x60)
        return True

    # ---- registros ----
    def rd(self, a, n):
        if a >= 0x1C00 and a < 0x1E00:
            return int.from_bytes(self.cfg[a - 0x1C00:a - 0x1C00 + n], 'little')
        if a == 0x04:
            return (0x01 if self.sw_reset else 0x02) | 0x0200
        if a == 0x08:
            return self.isrc
        if a == 0x0C:
            return self.imsk
        if a in (0x1C, 0x24):
            v = self.isrc & self.imsk
            if v:
                self.masked = True
                self.irq_act()
            return v
        if a == 0x28:
            self.masked = False
            self.irq_act()
            return 0
        if a == 0x2884 and self.pend_smi is not None:
            return self.pend_smi
        if 0x2900 <= a <= 0x2A5F:
            v = self.mib.get(a & ~7, 0)
            return (v >> 16 if a & 4 else v) & 0xFFFF
        return int.from_bytes(self.r[a:a + n], 'little')

    pend_smi = None

    def wr(self, a, n, v):
        if self.sw_reset and a not in (0x04, 0x05, 0x07, 0x1C80, 0x0E68) and a >= 0x80:
            self.aviso('escritura en %04X con el chip en reinicio' % a)
        if a >= 0x1C00 and a < 0x1E00:
            if not self.r[0x158] & 0x02 and a not in (0x1C80,):
                self.aviso('escritura en la conf. PCI (%04X) sin permiso (TST_CTRL1)' % a)
            self.cfg[a - 0x1C00:a - 0x1C00 + n] = v.to_bytes(n, 'little')
            return
        if a == 0x04:
            if n == 1:
                if v & 0x01:
                    self.sw_reset = True
                    self.imsk = 0
                    self.st['on'] = False
                    for q in self.q.values():
                        q['on'] = False
                    self.rxbufs = []
                    self.irq_act()
                if v & 0x02:
                    self.sw_reset = False
            return
        if a == 0x0C:
            self.imsk = v
            self.irq_act()
            return
        if a == 0x2C:
            self.masked = False
            self.irq_act()
            return
        if a == 0x158:
            self.r[0x158] = 0x02 if v & 0x02 else 0x00
            return
        if a == 0xF04:              # GPHY_CTRL
            if v & 0x01:
                self.phy_reset = True
            if v & 0x02:
                self.phy_reset = False
            return
        if a == 0x2880:             # GM_SMI_CTRL
            if (v >> 11) & 0x1F:
                self.aviso('SMI a un PHY que no es el 0')
            reg = (v >> 6) & 0x1F
            if v & 0x20:
                self.pend_smi = self.phy_rd(reg)
                v |= 0x10
            else:
                self.phy_wr(reg, struct.unpack_from('<H', self.r, 0x2884)[0])
                self.pend_smi = None
            v &= ~0x08
        if a == 0x2804:
            self.r[a:a + n] = v.to_bytes(n, 'little')
            self.tx_sale()
            return
        if a == 0xE80:              # STAT_CTRL
            if v & 0x01:
                self.st.update(on=False, put=0)
                struct.pack_into('<H', self.r, 0xE9C, 0)
                self.isrc &= ~(1 << 30)
            if v & 0x08:
                self.st['on'] = True
                self.st['addr'] = struct.unpack_from('<I', self.r, 0xE88)[0]
                self.st['last'] = struct.unpack_from('<H', self.r, 0xE84)[0]
                if self.st['addr'] & 0xFFF:
                    self.aviso('anillo de estado sin alinear a 4 KB')
                if struct.unpack_from('<I', self.r, 0xE8C)[0]:
                    self.aviso('anillo de estado por encima de 4 GB')
            if v & 0x10:
                self.isrc &= ~(1 << 30)
            self.irq_act()
            return
        for base in self.q:
            q = self.q[base]
            if a == base:           # PREF_UNIT_CTRL
                if v & 0x01:
                    q.update(on=False, get=0)
                    if base == 0x450:
                        self.rxbufs = []
                if v & 0x08:
                    q['on'] = True
                    q['addr'] = struct.unpack_from('<I', self.r, base + 8)[0]
                    q['last'] = struct.unpack_from('<H', self.r, base + 4)[0]
                    csr = base - 0x450 + 0x434
                    if not self.r[csr + 1] & 0x01:
                        self.aviso('precarga de %s en marcha sin arrancar su BMU'
                                   % q['nombre'])
                return
            if a == base + 0x14:
                self.r[a:a + n] = v.to_bytes(n, 'little')
                self.put_idx(base, v)
                return
        if a in (0x434, 0x6B4):     # Q_CSR: se guardan los bits puestos
            old = int.from_bytes(self.r[a:a + 4], 'little')
            v = 0 if v & 0x01 else (old | v)
            n = 4
        self.r[a:a + n] = v.to_bytes(n, 'little')

    def io(self, port, n, v=None):
        self.accesos_io += 1
        off = port - IO_BASE
        if off == 0 and n == 1:
            if v is None:
                return self.rap
            self.rap = v & 0x7F
            return None
        a = off if off < 0x80 else self.rap * 0x80 + off - 0x80
        if v is None:
            return self.rd(a, n)
        self.wr(a, n, v)
        return None


class VM:
    def __init__(self):
        self.kvm = os.open('/dev/kvm', os.O_RDWR)
        self.vmfd = fcntl.ioctl(self.kvm, KVM_CREATE_VM, 0)
        fcntl.ioctl(self.vmfd, KVM_SET_TSS_ADDR, 0xFFFBD000 - (1 << 32))
        fcntl.ioctl(self.vmfd, KVM_CREATE_IRQCHIP, 0)
        fcntl.ioctl(self.vmfd, KVM_CREATE_PIT2, bytes(64))
        self.mem = mmap.mmap(-1, MEM)
        base = ctypes.addressof(ctypes.c_char.from_buffer(self.mem))
        fcntl.ioctl(self.vmfd, KVM_SET_USER_MEMORY_REGION,
                    struct.pack('<IIQQQ', 0, 0, 0, MEM, base))
        self.vcpu = fcntl.ioctl(self.vmfd, KVM_CREATE_VCPU, 0)
        n = fcntl.ioctl(self.kvm, KVM_GET_VCPU_MMAP_SIZE, 0)
        self.run = mmap.mmap(self.vcpu, n)
        self.chip = None
        self.salida = bytearray()
        self.archivos = {}
        self.abiertos = {}
        self.lineas = {}
        self.v86 = False
        self.bloque = 0x2001        # lo que da AH=48h (sin alinear)
        self.bloque_paras = None
        self.irq_roto = False       # la interrupción de la tarjeta no llega
        self.sin_puertos = False
        # vectores: cada uno a su «push ax; mov al, n; out E6h, al; pop ax;
        # iret» (el puerto E6h no es de nadie: sale aquí, a servicio())
        for i in range(256):
            self.wmem(STUB * 16 + i * 8, bytes([0x50, 0xB0, i, 0xE6, 0xE6, 0x58, 0xCF, 0x90]))
            self.wmem(i * 4, struct.pack('<HH', i * 8, STUB))
        for i in range(0x60, 0x81):     # vectores libres (60h a 80h)
            self.wmem(i * 4, bytes(4))
        # reloj (IRQ 0): cuenta en 0:46C y EOI
        t = bytes.fromhex('1e50 31c0 8ed8 66ff066c04 b020 e620 58 1f cf')
        self.wmem(STUB * 16 + 0x900, t)
        self.wmem(8 * 4, struct.pack('<HH', 0x900, STUB))
        # las demás IRQ, sin dueño: EOI a los dos PIC
        e = bytes.fromhex('50 b020 e6a0 e620 58 cf')
        self.wmem(STUB * 16 + 0x920, e)
        for v in list(range(9, 16)) + list(range(0x70, 0x78)):
            self.wmem(v * 4, struct.pack('<HH', 0x920, STUB))
        # arranque: PIC (bases 08h y 70h; solo el reloj, el teclado y la
        # cascada sin máscara) y temporizador rápido (unos 600 tics/s)
        b = bytes.fromhex('b011 e620 e6a0 b008 e621 b070 e6a1 b004 e621 b002 e6a1'
                          'b001 e621 e6a1 b0f8 e621 b0ff e6a1'
                          'b036 e643 b0d0 e640 b007 e640 e6ef')
        self.wmem(STUB * 16 + 0xA00, b)
        # rutinas de la prueba (segmento 5000h)
        self.wmem(0x50000, bytes.fromhex('cd60 e6e4'))      # llamar al packet driver
        self.wmem(0x50100, bytes.fromhex('e6e3 cb'))        # receptor
        self.wmem(0x50200, bytes.fromhex('fb 90 e6e2 ebfb'))  # esperar
        self.wmem(0x50300, bytes.fromhex('cd2f e6e4'))      # INT 2Fh
        self.recibidas = []
        self.rx_len = None
        self.set_state(STUB, 0xA00, STUB, 0xFFF0)
        self.go({0xEF})

    # ---- memoria y registros ----
    def rmem(self, a, n):
        return bytes(self.mem[a:a + n])

    def wmem(self, a, b):
        self.mem[a:a + len(b)] = bytes(b)

    def regs(self):
        return list(struct.unpack('<18Q', fcntl.ioctl(self.vcpu, KVM_GET_REGS, bytes(144))))

    def set_regs(self, r):
        fcntl.ioctl(self.vcpu, KVM_SET_REGS, struct.pack('<18Q', *r))

    def sregs(self):
        return bytearray(fcntl.ioctl(self.vcpu, KVM_GET_SREGS, bytes(312)))

    def seg(self, s, name):
        return struct.unpack_from('<H', s, SOFF[name] * 24 + 12)[0]

    def set_seg(self, s, name, v):
        o = SOFF[name] * 24
        struct.pack_into('<QIH', s, o, v << 4, 0xFFFF, v)
        struct.pack_into('<BBBBBBBB', s, o + 14, 0x9B if name == 'cs' else 0x93,
                         1, 0, 0, 1, 0, 0, 0)
        s[o + 22] = 0

    def set_state(self, cs, ip, ss, sp, ds=None, es=None, **kw):
        s = self.sregs()
        self.set_seg(s, 'cs', cs)
        self.set_seg(s, 'ss', ss)
        self.set_seg(s, 'ds', cs if ds is None else ds)
        self.set_seg(s, 'es', cs if es is None else es)
        fcntl.ioctl(self.vcpu, KVM_SET_SREGS, bytes(s))
        r = self.regs()
        r[ROFF['ip']] = ip
        r[ROFF['sp']] = sp
        r[ROFF['fl']] = 0x202
        for k, v in kw.items():
            r[ROFF[k]] = v
        self.set_regs(r)

    def irq_line(self, irq, on):
        on = 1 if on and not self.irq_roto else 0
        if self.lineas.get(irq) != on:
            self.lineas[irq] = on
            fcntl.ioctl(self.vmfd, KVM_IRQ_LINE, struct.pack('<II', irq, on))

    # ---- servicios del DOS y de la BIOS ----
    def cadena(self, seg, off):
        a = seg * 16 + off
        return self.rmem(a, self.mem.find(b'\0', a) - a).decode('cp437')

    def servicio(self, n, r, s):
        """-> (texto para parar, o None), y los registros cambiados."""
        bx, cx, dx = (r[ROFF[k]] & 0xFFFF for k in ('bx', 'cx', 'dx'))
        ss, sp = self.seg(s, 'ss'), r[ROFF['sp']] & 0xFFFF
        pila = ss * 16 + sp         # AX guardado, IP, CS e indicadores
        ax = struct.unpack('<H', self.rmem(pila, 2))[0]
        ah, al = ax >> 8, ax & 0xFF
        fa = pila + 6
        fl = struct.unpack('<H', self.rmem(fa, 2))[0]
        cf = None
        ds = self.seg(s, 'ds')
        fin = None

        def setr(k, v, n=16):
            if k == 'ax':
                self.wmem(pila, struct.pack('<H', v & 0xFFFF))
                return
            m = (1 << n) - 1
            r[ROFF[k]] = (r[ROFF[k]] & ~m) | (v & m)

        if n in (0, 6, 12, 13):
            ip, cs = struct.unpack('<HH', self.rmem(pila + 2, 4))
            raise Fallo('excepción %d del procesador en %04X:%04X (%s)' %
                        (n, cs, ip, self.rmem(cs * 16 + ip, 8).hex()))
        if n == 0x21:
            if ah == 0x02:
                self.salida.append(dx & 0xFF)
            elif ah == 0x09:
                a = ds * 16 + dx
                self.salida += self.rmem(a, self.mem.find(b'$', a) - a)
            elif ah == 0x25:
                self.wmem(al * 4, struct.pack('<HH', dx, ds))
            elif ah == 0x35:
                o, sg = struct.unpack('<HH', self.rmem(al * 4, 4))
                setr('bx', o)
                self.set_seg(s, 'es', sg)
            elif ah == 0x30:
                setr('ax', 0x1606)
            elif ah == 0x31:
                fin = 'residente'
                self.res_paras = dx
            elif ah == 0x4C:
                fin = 'fin'
                self.codigo = al
            elif ah == 0x49:
                cf = 0
            elif ah == 0x58:
                cf = 0
                if al in (0, 2):
                    setr('ax', 0)
            elif ah == 0x48:        # (LH) un bloque en la convencional
                cf = 0
                setr('ax', self.bloque)
                self.bloque_paras = bx
            elif ah == 0x4A:
                cf = 0
                if self.seg(s, 'es') == self.bloque:
                    self.bloque_paras = bx
            elif ah in (0x3C, 0x3D):
                nombre = self.cadena(ds, dx).upper()
                if ah == 0x3D and nombre not in self.archivos:
                    cf = 1
                    setr('ax', 2)
                else:
                    if ah == 0x3C:
                        self.archivos[nombre] = bytearray()
                    h = 5 + len(self.abiertos)
                    self.abiertos[h] = nombre
                    setr('ax', h)
                    cf = 0
            elif ah == 0x42:
                cf = 0
                setr('ax', len(self.archivos[self.abiertos[bx]]) & 0xFFFF)
                setr('dx', 0)
            elif ah == 0x40:
                self.archivos[self.abiertos[bx]] += self.rmem(ds * 16 + dx, cx)
                setr('ax', cx)
                cf = 0
            elif ah == 0x3E:
                self.abiertos.pop(bx, None)
                cf = 0
            else:
                cf = 1
        elif n == 0x1A and ah == 0xB1:
            c = self.chip.cfg
            di = r[ROFF['di']] & 0xFFFF
            cf = 0
            setr('ax', al)          # AH = 0
            if al == 0x01:
                setr('dx', 0x20494350, 32)
                setr('bx', 0x0210)
                setr('cx', 1)
            elif al == 0x02:        # buscar por fabricante y modelo
                if (dx, cx) == (0x11AB, 0x4381) and (r[ROFF['si']] & 0xFFFF) == 0:
                    setr('bx', BDF)
                else:
                    setr('ax', 0x8600 | al)
                    cf = 1
            elif al == 0x03:        # buscar por clase
                setr('ax', 0x8600 | al)
                cf = 1
            elif al in (0x08, 0x09, 0x0A):
                w = {8: 1, 9: 2, 10: 4}[al]
                setr('cx', int.from_bytes(c[di:di + w], 'little'), 32 if w == 4 else 16)
            elif al in (0x0B, 0x0C, 0x0D):
                w = {0x0B: 1, 0x0C: 2, 0x0D: 4}[al]
                c[di:di + w] = (r[ROFF['cx']] & ((1 << (8 * w)) - 1)).to_bytes(w, 'little')
        if cf is not None:
            self.wmem(fa, struct.pack('<H', (fl & ~1) | cf))
        return fin

    # ---- correr hasta una marca ----
    def go(self, marcas, plazo=30):
        def tarde(*_):
            raise Fallo('la máquina no vuelve (¿colgada?)')
        signal.signal(signal.SIGALRM, tarde)
        signal.alarm(plazo)
        try:
            return self._go(marcas)
        except Fallo as e:
            signal.alarm(0)
            r = self.regs()
            raise Fallo('%s; en %04X:%04X, SP=%04X; en pantalla: %s' %
                        (e, self.seg(self.sregs(), 'cs'), r[ROFF['ip']] & 0xFFFF,
                         r[ROFF['sp']] & 0xFFFF, self.salida.decode('cp437')[-600:]))
        finally:
            signal.alarm(0)

    def _go(self, marcas):
        run = self.run
        while True:
            fcntl.ioctl(self.vcpu, KVM_RUN, 0)
            reason = struct.unpack_from('<I', run, 8)[0]
            if reason == EXIT_IO:
                d, size, port, count, off = struct.unpack_from('<BBHIQ', run, 32)
                chip = self.chip
                if chip and IO_BASE <= port < IO_BASE + 0x100 and not self.sin_puertos:
                    if d == 1:
                        chip.io(port, size, int.from_bytes(run[off:off + size], 'little'))
                    else:
                        run[off:off + size] = (chip.io(port, size) &
                                               ((1 << (8 * size)) - 1)).to_bytes(size, 'little')
                    continue
                if d == 0:
                    run[off:off + size] = b'\xff' * size
                    continue
                if port in marcas:
                    return port
                if port == 0xE3:    # el receptor de la prueba
                    r = self.regs()
                    s = self.sregs()
                    if r[0] & 0xFFFF == 0:
                        self.rx_len = r[ROFF['cx']] & 0xFFFF
                        self.set_seg(s, 'es', 0x5000)
                        fcntl.ioctl(self.vcpu, KVM_SET_SREGS, bytes(s))
                        r[ROFF['di']] = (r[ROFF['di']] & ~0xFFFF) | 0x1000
                        self.set_regs(r)
                    else:
                        a = self.seg(s, 'ds') * 16 + (r[ROFF['si']] & 0xFFFF)
                        n = r[ROFF['cx']] & 0xFFFF
                        if n != self.rx_len:
                            raise Fallo('el receptor recibe otra longitud')
                        self.recibidas.append(self.rmem(a, n))
                    continue
                r = self.regs()
                s = self.sregs()
                if port == 0xE6 and self.seg(s, 'cs') == STUB:
                    fin = self.servicio(r[0] & 0xFF, r, s)
                    self.set_regs(r)
                    fcntl.ioctl(self.vcpu, KVM_SET_SREGS, bytes(s))
                    if fin:
                        return fin
                continue
            if reason == EXIT_MMIO:
                addr, data, ln, wr = struct.unpack_from('<Q8sIB', run, 32)
                chip = self.chip
                if chip and MMIO_BASE <= addr < MMIO_BASE + 0x4000:
                    chip.accesos_mem += 1
                    if wr:
                        chip.wr(addr - MMIO_BASE, ln, int.from_bytes(data[:ln], 'little'))
                    else:
                        v = chip.rd(addr - MMIO_BASE, ln) & ((1 << (8 * ln)) - 1)
                        run[40:40 + ln] = v.to_bytes(ln, 'little')
                elif not wr:
                    run[40:40 + ln] = b'\xff' * ln
                continue
            if reason == EXIT_HLT:
                continue
            r = self.regs()
            raise Fallo('KVM sale con %d en %04X:%04X' %
                        (reason, self.seg(self.sregs(), 'cs'), r[ROFF['ip']] & 0xFFFF))

    # ---- lo que hace la prueba ----
    def programa(self, com, args, seg=0x1000):
        """Ejecuta HSPRED con esos argumentos -> ('fin' o 'residente', texto)."""
        self.salida = bytearray()
        psp = bytearray(256)
        psp[0:2] = b'\xcd\x20'
        struct.pack_into('<H', psp, 0x2C, 0x0F00)
        struct.pack_into('<H', psp, 0x02, 0x9FC0 if seg < 0x9000 else seg + 0x1000)   # final de su memoria
        t = (' ' + args).encode('cp437') if args else b''
        psp[0x80] = len(t)
        psp[0x81:0x81 + len(t) + 1] = t + b'\r'
        self.wmem(seg * 16, psp)
        self.wmem(seg * 16 + 0x100, com)
        self.wmem(seg * 16 + 0xFFFC, b'\0\0\0\0')
        self.set_state(seg, 0x100, seg, 0xFFFC, ax=0, bx=0, cx=0, dx=0, si=0, di=0, bp=0)
        if self.v86:                # (solo lo que mira HSPRED: SMSW)
            s = self.sregs()
            raise Fallo('modo V86 sin simular')
        fin = self.go(set())
        return fin, self.salida.decode('cp437')

    def pkt(self, vec=0x60, **kw):
        """Llama al packet driver -> registros, CF."""
        self.wmem(0x50001, bytes([vec]))
        ds = kw.pop('ds', 0x5000)
        es = kw.pop('es', 0x5000)
        z = dict(ax=0, bx=0, cx=0, dx=0, si=0, di=0, bp=0)
        z.update(kw)
        self.set_state(0x5000, 0x0000, 0x5000, 0xFF00, ds=ds, es=es, **z)
        self.go({0xE4})
        r = self.regs()
        return {k: r[ROFF[k]] & 0xFFFF for k in REGS}, r[ROFF['fl']] & 1

    def int2f(self, ax):
        self.set_state(0x5000, 0x0300, 0x5000, 0xFF00, ax=ax, bx=0, cx=0, dx=0)
        self.go({0xE4})
        r = self.regs()
        return r[0] & 0xFFFF

    def espera(self, vueltas=200):
        """Deja pasar el tiempo con las interrupciones abiertas."""
        self.set_state(0x5000, 0x0200, 0x5000, 0xFF00)
        t0 = struct.unpack('<I', self.rmem(0x46C, 4))[0]
        for _ in range(vueltas):
            self.go({0xE2})
        # sin IRQ (/SONDEO) HSPRED mira la tarjeta en cada tic del reloj: que
        # pasen por lo menos dos, por rápido que vaya el equipo
        n = 0
        while self.irq_roto and n < 100000 and \
                struct.unpack('<I', self.rmem(0x46C, 4))[0] - t0 < 2:
            self.go({0xE2})
            n += 1


def comprueba(cond, texto):
    global FALLOS
    print('  %s  %s' % ('ok ' if cond else 'MAL', texto))
    if not cond:
        FALLOS += 1


def trama(dst, src, tipo, n):
    return dst + src + struct.pack('>H', tipo) + bytes((i * 7 + n) & 0xFF for i in range(n - 14))


def escenario(com, modo, rev=1, cable=True, vel=1000, detalle=False, sondeo=False, lh=False):
    """Carga, envío, recepción, /INFO y descarga, con un modo de acceso."""
    print('Yukon simulada: registros por %s, revisión %d, %s%s%s' %
          (modo, rev, ('enlace a %d' % vel) if cable else 'sin cable',
           ', sin IRQ (/SONDEO)' if sondeo else '',
           ', con LH (búferes en un bloque de la convencional)' if lh else ''))
    vm = VM()
    vm.irq_roto = sondeo
    chip = vm.chip = Yukon(vm, rev=rev, cable=cable, vel=vel)
    arg = {'memoria': '', 'puertos': '/PUERTOS', 'auto': ''}[modo]
    if sondeo:
        arg += ' /SONDEO'
    reloj = vm.rmem(8 * 4, 4)
    fin, txt = vm.programa(com, ('/DEPURA:Y.LOG ' + arg).strip(), seg=0xD000 if lh else 0x1000)
    if detalle:
        print(txt)
    comprueba(fin == 'residente', 'HSPRED se queda residente (%s)' % fin)
    if fin != 'residente':
        print(txt)
        return vm
    comprueba('Marvell Yukon 88E8059 (Optima), MAC 00:26:55:3A:1C:9E, IRQ %d' %
              (0 if sondeo else IRQ) in txt, 'dice la tarjeta, la MAC del chip y el IRQ')
    cmd = struct.unpack_from('<H', chip.cfg, 4)[0]
    comprueba(cmd & 0x407 == (0x407 if sondeo else 0x007),
              'orden PCI: E/S, memoria, bus master e INTx %s' % ('tapada' if sondeo else 'permitida'))
    if sondeo:
        comprueba(vm.rmem(8 * 4, 4) != reloj, 'con /SONDEO se engancha al reloj (INT 08h)')
    if modo == 'puertos':
        comprueba(chip.accesos_io > 100, 'los registros, por los puertos')
    else:
        comprueba(chip.accesos_mem > 100 and '[Yukon] identificador del chip por memoria: 00BC' in txt
                  and 'por los puertos: 00BC' in txt,
                  'los registros, por memoria (y por los puertos también lee el chip)')
    comprueba(b'[Yukon]' in vm.archivos.get('Y.LOG', b''), '/DEPURA deja los pasos en el archivo')
    paras = vm.res_paras
    if lh:
        comprueba(paras * 16 < 8000 and vm.bloque_paras and vm.bloque_paras * 16 < 13000,
                  'residente %d octetos y búferes aparte: %d' % (paras * 16, (vm.bloque_paras or 0) * 16))
    else:
        comprueba(paras * 16 < 20000, 'residente con los búferes: %d octetos' % (paras * 16))
    comprueba(chip.st['on'] and chip.q[0x450]['on'] and chip.q[0x6D0]['on'],
              'unidad de estado y las dos colas, en marcha')
    comprueba(len(chip.rxbufs) == 4 and all(b[1] == 1520 for b in chip.rxbufs),
              'cuatro búferes de recepción de 1520 dados a la tarjeta')
    zonas = sorted([(a, a + n) for a, n in chip.rxbufs] +
                   [(chip.st['addr'], chip.st['addr'] + 8 * 64),
                    (chip.q[0x450]['addr'], chip.q[0x450]['addr'] + 8 * 16),
                    (chip.q[0x6D0]['addr'], chip.q[0x6D0]['addr'] + 8 * 16)])
    if lh:
        dentro = (vm.bloque * 16, (vm.bloque + (vm.bloque_paras or 0)) * 16)
    else:
        dentro = (0x10000 + 0x1100, 0x10000 + paras * 16)   # (lo residente, 4 KB o más)
    comprueba(all(zonas[i][1] <= zonas[i + 1][0] for i in range(len(zonas) - 1))
              and dentro[0] <= zonas[0][0] and zonas[-1][1] <= dentro[1],
              'anillos y búferes sin pisarse, en su sitio (%05X-%05X)' % (zonas[0][0], zonas[-1][1]))
    comprueba(chip.imsk == 0x40000010, 'máscara de interrupciones: estado y PHY')
    gp = struct.unpack_from('<H', chip.r, 0x2804)[0]
    if cable:
        comprueba(gp & 0x1800 == 0x1800, 'con enlace, el MAC envía y recibe (GP_CTRL %04X)' % gp)
        ipg = struct.unpack_from('<H', chip.r, 0x2818)[0] & 0x1F
        comprueba(ipg == (0x1E if vel == 1000 else 0x18), 'hueco entre tramas para %d Mbit/s' % vel)
        comprueba(vm.int2f(0xDA01) == (({10: 1, 100: 2, 1000: 3}[vel] << 8) | 1),
                  'INT 2Fh AX=DA01h: enlace y velocidad')
    else:
        comprueba(gp & 0x1800 == 0, 'sin enlace, el MAC parado')
        comprueba(vm.int2f(0xDA01) == 0, 'INT 2Fh AX=DA01h: sin enlace')
    # el packet driver
    r, cf = vm.pkt(ax=0x0201, bx=0xFFFF, cx=0, dx=0, si=0, di=0x0100, ds=0x5000, es=0x5000)
    h = r['ax']
    comprueba(cf == 0, 'access_type (todos los tipos)')
    vm.wmem(0x52000, bytes(16))
    r, cf = vm.pkt(ax=0x0600, bx=h, cx=6, di=0x2000)
    comprueba(cf == 0 and vm.rmem(0x52000, 6) == MAC, 'get_address da la MAC')
    arp = trama(b'\xff' * 6, MAC, 0x0806, 42)
    vm.wmem(0x53000, arp)
    r, cf = vm.pkt(ax=0x0400, cx=len(arp), si=0x3000)
    if not cable:
        comprueba(cf == 1 and not chip.enviadas and not chip.pendientes,
                  'sin enlace, send_pkt dice que no puede (y no deja nada a medias)')
        # llega el cable: el PHY interrumpe y el MAC arranca
        chip.enlace(True)
        vm.espera()
        gp = struct.unpack_from('<H', chip.r, 0x2804)[0]
        comprueba(gp & 0x1800 == 0x1800, 'al enchufar el cable (interrupción del PHY), el MAC arranca')
        r, cf = vm.pkt(ax=0x0400, cx=len(arp), si=0x3000)
    comprueba(cf == 0 and chip.enviadas and chip.enviadas[-1] == arp + bytes(18),
              'send_pkt: la trama sale, rellena a 60 octetos')
    for i in range(20):             # da la vuelta al anillo de envío
        t = trama(b'\x02\x00\x00\x00\x00\x01', MAC, 0x0800, 60 + i * 70)
        vm.wmem(0x53000, t)
        r, cf = vm.pkt(ax=0x0400, cx=len(t), si=0x3000)
        if cf or chip.enviadas[-1] != t:
            comprueba(False, 'envío %d' % i)
            break
    else:
        comprueba(len(chip.enviadas) == 21, '20 envíos más (el anillo de envío da la vuelta)')
    # recepción: 40 tramas de varios tamaños, de una en una y a ráfagas
    esperadas = []
    for i in range(40):
        dst = MAC if i % 3 else b'\xff' * 6
        t = trama(dst, b'\x02\x00\x00\x00\x00\x02', 0x0800, [60, 64, 590, 1514, 100][i % 5])
        if chip.recibe(t, basura=(i % 7 == 0)):
            esperadas.append(t)
        if i % 4 == 3 or i < 8:
            vm.espera(20)
    vm.espera(50)
    comprueba(len(esperadas) >= 30 and vm.recibidas == esperadas,
              'recepción: %d tramas entregadas enteras y en orden (el anillo da la vuelta)'
              % len(vm.recibidas))
    comprueba(len(chip.rxbufs) == 4, 'los búferes vuelven a la tarjeta')
    n = len(vm.recibidas)
    chip.recibe(trama(b'\x02\x00\x00\x00\x00\x09', b'\x02\x00\x00\x00\x00\x02', 0x0800, 80))
    chip.recibe(trama(MAC, b'\x02\x00\x00\x00\x00\x02', 0x0800, 80), estado=0x102)   # CRC mal
    vm.espera(30)
    comprueba(len(vm.recibidas) == n, 'una trama para otro y una con error no se entregan')
    t = trama(MAC, b'\x02\x00\x00\x00\x00\x02', 0x0806, 60)
    chip.recibe(t)
    vm.espera(30)
    comprueba(vm.recibidas[-1] == t, 'y después sigue recibiendo')
    # el cable se va y vuelve
    chip.enlace(False)
    vm.espera(30)
    gp = struct.unpack_from('<H', chip.r, 0x2804)[0]
    comprueba(gp & 0x1800 == 0 and vm.int2f(0xDA01) == 0, 'sin cable: MAC parado y enlace «no»')
    chip.enlace(True)
    vm.espera(30)
    gp = struct.unpack_from('<H', chip.r, 0x2804)[0]
    comprueba(gp & 0x1800 == 0x1800, 'vuelve el cable: MAC en marcha')
    # /INFO con el residente cargado (otra copia del programa)
    vm.salida = bytearray()
    fin, info = vm.programa(com, '/INFO /DEPURA:Y.LOG', seg=0x3000)
    if detalle:
        print(info)
    comprueba(fin == 'fin' and 'Yukon, PHY:' in info and 'irqs=' in info
              and 'Enlace:          sí, %d Mbit/s' % vel in info,
              '/INFO con el residente: registros, PHY, contadores y enlace')
    comprueba('(!)' not in info, '/INFO no ve nada raro (sin avisos)')
    comprueba(b'Yukon, PHY:' in vm.archivos['Y.LOG'], '/INFO /DEPURA lo añade al archivo')
    t = trama(MAC, b'\x02\x00\x00\x00\x00\x02', 0x0806, 77)
    chip.recibe(t)
    vm.espera(30)
    comprueba(vm.recibidas[-1] == t, 'tras /INFO sigue recibiendo')
    # descargar
    r, cf = vm.pkt(ax=0x0300, bx=h)
    fin, txt = vm.programa(com, '/U', seg=0x3000)
    comprueba('HSPRED descargado' in txt, '/U lo descarga')
    comprueba(chip.sw_reset and chip.imsk == 0 and not vm.lineas.get(IRQ),
              'la tarjeta queda en reinicio y sin interrupciones')
    comprueba(vm.rmem(8 * 4, 4) == reloj, 'el vector del reloj queda como estaba')
    t0 = struct.unpack('<I', vm.rmem(0x46C, 4))[0]
    vm.espera(3000)
    comprueba(struct.unpack('<I', vm.rmem(0x46C, 4))[0] > t0, 'y el reloj sigue andando')
    # y se puede volver a cargar
    fin, txt = vm.programa(com, arg.strip())
    t = trama(MAC, b'\x02\x00\x00\x00\x00\x02', 0x0806, 99)
    if fin == 'residente':
        r, cf = vm.pkt(ax=0x0201, bx=0xFFFF, cx=0, dx=0, si=0, di=0x0100, ds=0x5000, es=0x5000)
        chip.recibe(t)
        vm.espera(100)
    comprueba(fin == 'residente' and vm.recibidas[-1] == t, 'se vuelve a cargar y recibe')
    comprueba(not chip.avisos, 'el modelo del chip no tiene quejas' +
              (': ' + '; '.join(chip.avisos) if chip.avisos else ''))
    return vm


FALLOS = 0


def main():
    com = open(sys.argv[1], 'rb').read()
    detalle = '-v' in sys.argv
    escenario(com, 'memoria', detalle=detalle)
    escenario(com, 'puertos', vel=100)
    escenario(com, 'memoria', rev=0, cable=False)
    escenario(com, 'memoria', sondeo=True)
    escenario(com, 'puertos', sondeo=True, cable=False)
    escenario(com, 'memoria', lh=True)
    print('Yukon simulada: sin contestar por ningún camino')
    vm = VM()
    vm.chip = Yukon(vm)
    vm.chip.r[0x11B] = 0x00
    fin, txt = vm.programa(com, '')
    comprueba(fin == 'fin' and 'no contesta' in txt, 'lo dice y no se queda residente')
    print('Yukon simulada: otro chip de la familia (Yukon 2 EC Ultra, B4h)')
    vm = VM()
    vm.chip = Yukon(vm)
    vm.chip.r[0x11B] = 0xB4
    fin, txt = vm.programa(com, '')
    comprueba(fin == 'fin' and 'solo la Optima' in txt, 'lo dice y no se queda residente')
    print('FALLOS: %d' % FALLOS)
    sys.exit(1 if FALLOS else 0)


if __name__ == '__main__':
    main()
