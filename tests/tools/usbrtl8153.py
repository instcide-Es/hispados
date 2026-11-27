#!/usr/bin/env python3
"""Un adaptador de red USB Realtek RTL8153 (0BDA:8153) de mentira para las
pruebas de Hispa-Pentium: QEMU no lo trae (su usb-net es RNDIS + CDC-ECM, y
de velocidad plena), pero sí «usb-redir», que pasa a un programa de fuera
todo lo que el sistema invitado le pide a un aparato USB (protocolo usbredir,
público: usb-redirection-protocol.txt). Este guion hace de ese programa, como
tests/tools/usbmando.py, con los descriptores del RTL8153 de verdad:

  - dos configuraciones: la 1, la propia de Realtek (una interfaz FF/FF/00,
    la de su controlador r8152), y la 2, CDC-ECM (control 02/06/00 con su
    descriptor de Ethernet e iMACAddress; datos 0A/00/00 con la variante 0
    sin extremos y la 1 con los dos a granel);
  - de alta velocidad (USBRTL_VEL=2, lo de siempre) o súper (3);
  - las tramas que el invitado manda a granel (extremo 02h) van a QEMU por
    UDP (-netdev socket,udp=..., unido a un -netdev user por un hubport): así
    detrás está el DHCP, la puerta y el DNS de QEMU; lo que llega de QEMU,
    a los pedidos a granel de entrada (81h), una trama por transferencia;
  - los avisos de CDC por el extremo de interrupción (83h):
    NETWORK_CONNECTION y CONNECTION_SPEED_CHANGE (1 Gbit/s).
Y apunta lo mal hecho («modelo: FALLO ...»): datos a granel sin la
configuración de ECM o sin la variante 1, una configuración que no existe...

uso: usbrtl8153.py SOCKET PUERTO_QEMU PUERTO_MIO orden...
  enlace 0|1     manda NETWORK_CONNECTION (sin cable / con cable)
  wait SEGUNDOS
  espera TEXTO   espera a que TEXTO salga (otra vez) en el registro de la
                 consola del invitado (el fichero de USBRTL_VE)
  fin            desconecta el adaptador
QEMU:  -chardev socket,id=rd,path=SOCKET,server=on,wait=off
       -device usb-redir,chardev=rd,bus=xhci.0
       -netdev user,id=n0 -netdev socket,id=s0,udp=127.0.0.1:PUERTO_MIO,localaddr=127.0.0.1:PUERTO_QEMU
       -netdev hubport,id=h0,hubid=1,netdev=n0 -netdev hubport,id=h1,hubid=1,netdev=s0
Al acabar las órdenes sigue atendiendo a QEMU hasta que este cierra; en
USBRTL_LOG deja lo que ha visto (una línea por cosa y, al final, «resumen»).

Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""
import os, select, socket, struct, sys, time

HELLO, DEV_CONNECT, DEV_DISCONNECT, RESET, IF_INFO, EP_INFO = 0, 1, 2, 3, 4, 5
SET_CONF, GET_CONF, CONF_STATUS, SET_ALT, GET_ALT, ALT_STATUS = 6, 7, 8, 9, 10, 11
START_ISO, STOP_ISO, ISO_STATUS = 12, 13, 14
START_INT, STOP_INT, INT_STATUS = 15, 16, 17
ALLOC_STREAMS, FREE_STREAMS, STREAMS_STATUS, CANCEL = 18, 19, 20, 21
CONTROL, BULK, ISO, INTERRUPT = 100, 101, 102, 103
CAP_CONNECT_VERSION, CAP_EP_MAXPKT, CAP_ID64, CAP_BULK32 = 1, 4, 5, 6
OK, CANCELLED, INVAL, IOERROR, STALL = 0, 1, 2, 3, 4
T_CONTROL, T_ISO, T_BULK, T_INTERRUPT, T_INVALID = 0, 1, 2, 3, 255

MAC = os.environ.get('USBRTL_MAC', '00E04C680153')
VEL = int(os.environ.get('USBRTL_VEL', '2'))
MPS = 1024 if VEL == 3 else 512


def descriptores():
    dev = struct.pack('<BBHBBBBHHHBBBB', 18, 1, 0x0300 if VEL == 3 else 0x0210, 0, 0, 0,
                      9 if VEL == 3 else 64, 0x0BDA, 0x8153, 0x3000, 1, 2, 6, 2)

    def ep(n, tipo, mps, iv):
        d = struct.pack('<BBBBHB', 7, 5, n, tipo, mps, iv)
        if VEL == 3:
            d += struct.pack('<BBBBH', 6, 0x30, 0, 0, 0)       # (súper: el compañero)
        return d
    # la 1: la de Realtek (r8152)
    c1 = struct.pack('<BBBBBBBBB', 9, 4, 0, 0, 3, 0xFF, 0xFF, 0, 0)
    c1 += ep(0x81, 2, MPS, 0) + ep(0x02, 2, MPS, 0) + ep(0x83, 3, 2, 8)
    c1 = struct.pack('<BBHBBBBB', 9, 2, 9 + len(c1), 1, 1, 0, 0xA0, 50) + c1
    # la 2: CDC-ECM
    c2 = struct.pack('<BBBBBBBBB', 9, 4, 0, 0, 1, 2, 6, 0, 5)
    c2 += bytes([5, 0x24, 0, 0x10, 0x01])                       # cabecera CDC 1.10
    c2 += bytes([5, 0x24, 6, 0, 1])                             # unión: 0 manda en 1
    c2 += struct.pack('<BBBBIHHB', 13, 0x24, 0x0F, 3, 0, 1514, 0, 0)
    c2 += ep(0x83, 3, 16, 8)
    c2 += struct.pack('<BBBBBBBBB', 9, 4, 1, 0, 0, 0x0A, 0, 0, 0)
    c2 += struct.pack('<BBBBBBBBB', 9, 4, 1, 1, 2, 0x0A, 0, 0, 0)
    c2 += ep(0x81, 2, MPS, 0) + ep(0x02, 2, MPS, 0)
    c2 = struct.pack('<BBHBBBBB', 9, 2, 9 + len(c2), 2, 2, 0, 0xA0, 50) + c2
    return dev, [c1, c2]


def cadena(t):
    u = t.encode('utf-16-le')
    return bytes([2 + len(u), 3]) + u


CADENAS = {0: bytes([4, 3, 9, 4]), 1: cadena('Realtek'), 2: cadena('USB 10/100/1000 LAN'),
           3: cadena(MAC), 5: cadena('CDC Communications Control'), 6: cadena('000001')}


class Rtl:
    def __init__(self, path, p_qemu, p_mio):
        for _ in range(100):
            try:
                self.s = socket.socket(socket.AF_UNIX)
                self.s.connect(path)
                break
            except OSError:
                time.sleep(0.1)
        else:
            raise SystemExit('no se puede conectar con ' + path)
        self.u = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.u.bind(('127.0.0.1', p_mio))
        self.qemu = ('127.0.0.1', p_qemu)
        self.dev, self.cfgs = descriptores()
        self.buf = b''
        self.hello = False
        self.conf = 0
        self.alt = 0
        self.filtro = None
        self.int_on = False
        self.enlace = 1
        self.pend_int = []
        self.pedidos = []          # los pedidos a granel de entrada (id, largo)
        self.tramas = []           # lo que ha llegado de QEMU y aún no se ha pedido
        self.nid = 1
        self.log = []
        self.n_sale = self.n_entra = self.n_tiradas = 0
        self.fallos = 0
        caps = (1 << CAP_CONNECT_VERSION) | (1 << CAP_EP_MAXPKT) | (1 << CAP_ID64) | (1 << CAP_BULK32)
        self.send(HELLO, 0, b'usbrtl8153 (pruebas de Hispa-Pentium)'.ljust(64, b'\0') + struct.pack('<I', caps))

    def fallo(self, t):
        self.fallos += 1
        self.log.append('modelo: FALLO ' + t)

    def send(self, tipo, ident, datos):
        fmt = '<IIQ' if self.hello else '<III'
        self.s.sendall(struct.pack(fmt, tipo, len(datos), ident) + datos)

    def info(self):
        """IF_INFO y EP_INFO de la configuración y la variante de ahora"""
        if self.conf == 2:
            ifs = [(0, 2, 6, 0), (1, 0x0A, 0, 0)]
            eps = [(0x83, T_INTERRUPT, 8, 0, 16)]
            if self.alt == 1:
                eps += [(0x81, T_BULK, 0, 1, MPS), (0x02, T_BULK, 0, 1, MPS)]
        elif self.conf == 1:
            ifs = [(0, 0xFF, 0xFF, 0)]
            eps = [(0x81, T_BULK, 0, 0, MPS), (0x02, T_BULK, 0, 0, MPS), (0x83, T_INTERRUPT, 8, 0, 2)]
        else:
            ifs, eps = [], []
        d = struct.pack('<I', len(ifs))
        for k in range(4):
            d += bytes(x[k] for x in ifs).ljust(32, b'\0')
        self.send(IF_INFO, 0, d)
        tipo = [T_INVALID] * 32
        ival = [0] * 32
        ifn = [0] * 32
        mps = [0] * 32
        tipo[0] = tipo[16] = T_CONTROL
        mps[0] = mps[16] = 64
        for e, t, iv, i, m in eps:
            k = (e & 15) + (16 if e & 0x80 else 0)
            tipo[k], ival[k], ifn[k], mps[k] = t, iv, i, m
        self.send(EP_INFO, 0, bytes(tipo) + bytes(ival) + bytes(ifn) + struct.pack('<32H', *mps))

    def conectar(self):
        self.info()
        vid, pid, ver = struct.unpack_from('<HHH', self.dev, 8)
        self.send(DEV_CONNECT, 0, struct.pack('<BBBBHHH', VEL, 0, 0, 0, vid, pid, ver))

    def control(self, ident, d):
        ep, req, rt, st, val, idx, ln = struct.unpack_from('<BBBBHHH', d)
        datos = d[10:]
        out = None
        if rt == 0x80 and req == 6:
            t, i = val >> 8, val & 0xFF
            if t == 1:
                out = self.dev
            elif t == 2 and i < len(self.cfgs):
                out = self.cfgs[i]
            elif t == 3 and i in CADENAS:
                out = CADENAS[i]
            elif t == 0x0F:
                out = bytes([5, 0x0F, 5, 0, 0])                 # BOS, vacío
        elif rt in (0x80, 0x81, 0x82) and req == 0:
            out = b'\0\0'
        elif rt == 0x21 and req == 0x43:                        # SET_ETHERNET_PACKET_FILTER
            if self.conf != 2:
                self.fallo('SET_ETHERNET_PACKET_FILTER sin la configuración de ECM')
            self.filtro = val
            self.log.append('filtro %04x' % val)
            out = b''
        elif rt == 0x21 and req in (0x40, 0x41, 0x42, 0x44):    # multidifusión, PM: vale
            out = b''
        elif rt & 0x60 == 0x40:                                 # del fabricante (lo de r8152)
            self.log.append('del fabricante %02x %02x %04x %04x %d' % (rt, req, val, idx, ln))
            out = bytes(ln) if rt & 0x80 else b''
        elif rt == 0x02 and req == 1:                           # CLEAR_FEATURE(ENDPOINT_HALT)
            out = b''
        self.log.append('control %02x %02x %04x %04x %d -> %s' % (rt, req, val, idx, ln,
                        'STALL' if out is None else len(out[:ln])))
        if out is None:
            self.send(CONTROL, ident, struct.pack('<BBBBHHH', ep, req, rt, STALL, val, idx, 0))
        else:
            out = out[:ln] if rt & 0x80 else b''
            self.send(CONTROL, ident, struct.pack('<BBBBHHH', ep, req, rt, OK, val, idx, len(out)) + out)

    def aviso(self, d):
        if not self.int_on:
            self.pend_int.append(d)
            return
        self.nid += 1
        self.send(INTERRUPT, self.nid, struct.pack('<BBH', 0x83, OK, len(d)) + d)

    def avisos_enlace(self):
        self.aviso(struct.pack('<BBHHH', 0xA1, 0x00, self.enlace, 0, 0))
        if self.enlace:
            self.aviso(struct.pack('<BBHHHII', 0xA1, 0x2A, 0, 0, 8, 1000000000, 1000000000))

    def bulk(self, ident, d):
        e, st, ln, sid, lnh = struct.unpack_from('<BBHIH', d)
        ln |= lnh << 16
        cuerpo = d[10:]
        bien = self.conf == 2 and self.alt == 1
        if e == 0x02:
            if not bien:
                self.fallo('datos a granel (sale) sin ECM o sin la variante 1 (conf %d, var %d)' % (self.conf, self.alt))
            elif len(cuerpo) < 14 or len(cuerpo) > 1515:
                self.fallo('una trama de %d octetos' % len(cuerpo))
            else:
                t = cuerpo
                # (el octeto de más de usbnet: una trama que no acaba en paquete corto)
                if len(t) % MPS == 1 and len(t) > 60:
                    t = t[:-1]
                self.n_sale += 1
                self.u.sendto(t, self.qemu)
            self.send(BULK, ident, struct.pack('<BBHIH', e, OK, ln & 0xFFFF, sid, ln >> 16))
        elif e == 0x81:
            if not bien:
                self.fallo('pide a granel (entra) sin ECM o sin la variante 1 (conf %d, var %d)' % (self.conf, self.alt))
            self.pedidos.append((ident, ln, sid))
            self.sirve()
        else:
            self.fallo('a granel por el extremo %02x' % e)
            self.send(BULK, ident, struct.pack('<BBHIH', e, STALL, 0, sid, 0))

    def sirve(self):
        while self.pedidos and self.tramas:
            ident, ln, sid = self.pedidos.pop(0)
            t = self.tramas.pop(0)[:ln]
            self.n_entra += 1
            self.send(BULK, ident, struct.pack('<BBHIH', 0x81, OK, len(t) & 0xFFFF, sid, len(t) >> 16) + t)

    def paquete(self, tipo, ident, d):
        if tipo == HELLO:
            self.hello = True
            self.conectar()
        elif tipo == CONTROL:
            self.control(ident, d)
        elif tipo == SET_CONF:
            if d[0] not in (0, 1, 2):
                self.fallo('SET_CONFIGURATION %d (no existe)' % d[0])
            self.conf, self.alt = d[0], 0
            self.int_on = False
            self.pedidos = []
            self.info()
            self.send(CONF_STATUS, ident, struct.pack('<BB', OK, d[0]))
            self.log.append('set_configuration %d' % d[0])
        elif tipo == GET_CONF:
            self.send(CONF_STATUS, ident, struct.pack('<BB', OK, self.conf))
        elif tipo == SET_ALT:
            ifz, alt = d[0], d[1]
            ok = self.conf == 2 and ifz == 1 and alt in (0, 1) or alt == 0
            if not ok:
                self.fallo('SET_INTERFACE %d variante %d (conf %d)' % (ifz, alt, self.conf))
            if self.conf == 2 and ifz == 1:
                self.alt = alt
                if alt == 0:
                    self.pedidos = []
            self.info()
            self.send(ALT_STATUS, ident, struct.pack('<BBB', OK if ok else STALL, ifz, alt))
            self.log.append('set_interface %d %d' % (ifz, alt))
        elif tipo == GET_ALT:
            self.send(ALT_STATUS, ident, struct.pack('<BBB', OK, d[0], self.alt if d[0] == 1 else 0))
        elif tipo == START_INT:
            self.send(INT_STATUS, ident, struct.pack('<BB', OK, d[0]))
            self.log.append('start_interrupt_receiving %02x' % d[0])
            if d[0] == 0x83 and self.conf == 2:
                self.int_on = True
                if not self.pend_int:
                    self.avisos_enlace()
                p, self.pend_int = self.pend_int, []
                for x in p:
                    self.aviso(x)
        elif tipo == STOP_INT:
            self.send(INT_STATUS, ident, struct.pack('<BB', OK, d[0]))
            if d[0] == 0x83:
                self.int_on = False
        elif tipo == BULK:
            self.bulk(ident, d)
        elif tipo == CANCEL:
            for k, x in enumerate(self.pedidos):
                if x[0] == ident:
                    self.pedidos.pop(k)
                    self.send(BULK, ident, struct.pack('<BBHIH', 0x81, CANCELLED, 0, x[2], 0))
                    break
        elif tipo == RESET:
            self.conf, self.alt, self.int_on, self.pedidos = 0, 0, False, []
        elif tipo == ALLOC_STREAMS or tipo == FREE_STREAMS:
            self.send(STREAMS_STATUS, ident, struct.pack('<BIB', OK, 0, 0))

    def atender(self, segundos):
        fin = time.time() + segundos
        while True:
            falta = fin - time.time()
            if falta <= 0:
                return True
            r, _, _ = select.select([self.s, self.u], [], [], falta)
            if self.u in r:
                t, _ = self.u.recvfrom(4096)
                if self.conf == 2 and self.alt == 1 and len(self.tramas) < 256:
                    self.tramas.append(t)
                    self.sirve()
                else:
                    self.n_tiradas += 1
            if self.s not in r:
                continue
            try:
                d = self.s.recv(65536)
            except OSError:
                return False
            if not d:
                return False
            self.buf += d
            while True:
                fmt = '<IIQ' if self.hello else '<III'
                cab = struct.calcsize(fmt)
                if len(self.buf) < cab:
                    break
                tipo, ln, ident = struct.unpack_from(fmt, self.buf)
                if len(self.buf) < cab + ln:
                    break
                cuerpo = self.buf[cab:cab + ln]
                self.buf = self.buf[cab + ln:]
                self.paquete(tipo, ident, cuerpo)

    def resumen(self):
        return ('resumen: configuración %d, variante %d, filtro %s, %d tramas salen, %d entran, '
                '%d tiradas, %d fallos' % (self.conf, self.alt,
                                          'ninguno' if self.filtro is None else '%04x' % self.filtro,
                                          self.n_sale, self.n_entra, self.n_tiradas, self.fallos))


def main():
    m = Rtl(sys.argv[1], int(sys.argv[2]), int(sys.argv[3]))
    a = sys.argv[4:]
    i = 0
    vivo = True
    visto = 0
    while i < len(a) and vivo:
        if a[i] == 'enlace':
            m.enlace = int(a[i + 1])
            m.log.append('enlace %d' % m.enlace)
            m.avisos_enlace()
            vivo = m.atender(0.02)
            i += 2
        elif a[i] == 'wait':
            vivo = m.atender(float(a[i + 1]))
            i += 2
        elif a[i] == 'espera':
            texto = a[i + 1].encode()
            fin = time.time() + 120
            while vivo and time.time() < fin:
                try:
                    d = open(os.environ['USBRTL_VE'], 'rb').read()
                except OSError:
                    d = b''
                k = d.find(texto, visto)
                if k >= 0:
                    visto = k + len(texto)
                    break
                vivo = m.atender(0.05)
            i += 2
        elif a[i] == 'fin':
            m.send(DEV_DISCONNECT, 0, b'')
            m.log.append('desconectado')
            vivo = m.atender(0.2)
            i += 1
        else:
            raise SystemExit('orden desconocida: ' + a[i])
    while vivo:
        vivo = m.atender(1)
    m.log.append(m.resumen())
    if os.environ.get('USBRTL_LOG'):
        open(os.environ['USBRTL_LOG'], 'w').write('\n'.join(m.log) + '\n')


if __name__ == '__main__':
    main()
