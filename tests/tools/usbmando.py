#!/usr/bin/env python3
"""Un mando USB de mentira para las pruebas: QEMU no trae ningún mando de
juegos USB, pero sí el dispositivo «usb-redir», que pasa a un programa de
fuera todo lo que el sistema invitado le pide a un dispositivo USB (protocolo
usbredir, público: usb-redirection-protocol.txt). Este guion hace de ese
programa: se presenta como un mando con los descriptores de uno real y manda
los informes que se le digan. Así HSPUSB lo enumera por el xHCI de QEMU como
haría con el de verdad (descriptores, descriptor de informe HID, endpoint de
interrupción, informes).

uso: usbmando.py SOCKET PERFIL orden...
  PERFIL: ds4, dsense, luna, stadia, swpro (HID, con el descriptor de informe
          real de tests/mandos/descriptores.json) o x360 (tipo Xbox 360)
  rep HEX        manda un informe (bytes en hexadecimal)
  wait SEGUNDOS
  espera TEXTO   espera a que TEXTO salga (otra vez) en el registro de la
                 consola del invitado: el fichero de la variable USBMANDO_VE
                 (así los informes salen cuando el programa de DOS que los
                 mira ya está en marcha, tarde lo que tarde en arrancar)
  fin            desconecta el mando
QEMU:  -chardev socket,id=rd,path=SOCKET,server=on,wait=off
       -device usb-redir,chardev=rd,bus=xhci.0
Al acabar las órdenes sigue atendiendo a QEMU hasta que este cierra.

A fake USB gamepad for the tests (usbredir host side for QEMU's usb-redir).
"""
import json, os, select, socket, struct, sys, time

HELLO, DEV_CONNECT, DEV_DISCONNECT, RESET, IF_INFO, EP_INFO = 0, 1, 2, 3, 4, 5
SET_CONF, GET_CONF, CONF_STATUS, SET_ALT, GET_ALT, ALT_STATUS = 6, 7, 8, 9, 10, 11
START_INT, STOP_INT, INT_STATUS = 15, 16, 17
CONTROL, INTERRUPT = 100, 103
CAP_CONNECT_VERSION, CAP_EP_MAXPKT, CAP_ID64, CAP_BULK32 = 1, 4, 5, 6
OK, STALL = 0, 4
T_CONTROL, T_INTERRUPT, T_INVALID = 0, 3, 255

AQUI = os.path.dirname(os.path.abspath(__file__))


def perfil(nombre):
    """-> dict con los descriptores del mando"""
    if nombre == 'x360':
        # Mando de Xbox 360 con cable (045E:028E): la configuración entera,
        # como la da el mando (4 interfaces; la del mando es la 0, FF/5D/01)
        cfg = bytes.fromhex(
            '09029900040100a0fa'
            '0904000002ff5d0100' '1121000101258114000000001301080000'
            '0705810320000407050103200008'
            '0904010004ff5d0300' '1b2100010101824001022016830000000000001603000000000000'
            '0705820320000207050203200004' '0705830320004007050303200010'
            '0904020001ff5d0200' '092100010122840700' '07058403200010'
            '0904030000fffd1304' '064100010103')
        assert len(cfg) == 0x99, len(cfg)
        dev = struct.pack('<BBHBBBBHHHBBBB', 18, 1, 0x200, 0xFF, 0xFF, 0xFF, 8,
                          0x045E, 0x028E, 0x0114, 1, 2, 3, 1)
        return {'dev': dev, 'cfg': cfg, 'informe': b'', 'ep': 0x81,
                'ifs': [(0, 0xFF, 0x5D, 1), (1, 0xFF, 0x5D, 3), (2, 0xFF, 0x5D, 2), (3, 0xFF, 0xFD, 0x13)],
                'eps': [(0x81, 4, 0, 32), (0x01, 8, 0, 32), (0x82, 2, 1, 32), (0x02, 4, 1, 32),
                        (0x83, 64, 1, 32), (0x03, 16, 1, 32), (0x84, 16, 2, 32)]}
    datos = json.load(open(os.path.join(AQUI, '..', 'mandos', 'descriptores.json')))
    rd = bytes.fromhex(datos[nombre]['informe'])
    ids = {'ds4': (0x054C, 0x09CC), 'dsense': (0x054C, 0x0CE6), 'swpro': (0x057E, 0x2009),
           'luna': (0x1949, 0x0419), 'stadia': (0x18D1, 0x9400)}[nombre]
    # la interfaz HID y su endpoint, con los números del mando de verdad en
    # el DualShock 4 (interfaz 3, endpoint 84h, 64 bytes cada 5 ms)
    ifn, ep = (3, 0x84) if nombre == 'ds4' else (0, 0x81)
    cfg = struct.pack('<BBHBBBBB', 9, 2, 9 + 9 + 9 + 7, 1, 1, 0, 0x80, 250)
    cfg += struct.pack('<BBBBBBBBB', 9, 4, ifn, 0, 1, 3, 0, 0, 0)
    cfg += struct.pack('<BBHBBBH', 9, 0x21, 0x111, 0, 1, 0x22, len(rd))
    cfg += struct.pack('<BBBBHB', 7, 5, ep, 3, 64, 5)
    dev = struct.pack('<BBHBBBBHHHBBBB', 18, 1, 0x200, 0, 0, 0, 64, ids[0], ids[1], 0x100, 0, 0, 0, 1)
    return {'dev': dev, 'cfg': cfg, 'informe': rd, 'ep': ep,
            'ifs': [(ifn, 3, 0, 0)], 'eps': [(ep, 5, ifn, 64)]}


class Mando:
    def __init__(self, path, p):
        self.p = p
        for _ in range(100):
            try:
                self.s = socket.socket(socket.AF_UNIX)
                self.s.connect(path)
                break
            except OSError:
                time.sleep(0.1)
        else:
            raise SystemExit('no se puede conectar con ' + path)
        self.buf = b''
        self.hello = False
        self.recibiendo = False
        self.pend = None
        self.nid = 1
        self.log = []
        # (el xHCI de QEMU exige los identificadores de 64 bits y las longitudes
        # de 32 en los paquetes bulk, aunque aquí no haya ninguno)
        caps = (1 << CAP_CONNECT_VERSION) | (1 << CAP_EP_MAXPKT) | (1 << CAP_ID64) | (1 << CAP_BULK32)
        self.send(HELLO, 0, b'usbmando (pruebas de Hispa-DOS)'.ljust(64, b'\0') + struct.pack('<I', caps))

    def send(self, tipo, ident, datos):
        # la cabecera lleva el identificador en 32 bits hasta que los dos
        # lados han dicho (en su saludo) que entienden los de 64
        fmt = '<IIQ' if self.hello else '<III'
        self.s.sendall(struct.pack(fmt, tipo, len(datos), ident) + datos)

    def info(self):
        ifs = self.p['ifs']
        d = struct.pack('<I', len(ifs))
        for k in range(4):
            d += bytes(x[k] for x in ifs).ljust(32, b'\0')
        self.send(IF_INFO, 0, d)
        tipo = [T_INVALID] * 32
        ival = [0] * 32
        ifn = [0] * 32
        mps = [0] * 32
        tipo[0] = tipo[16] = T_CONTROL
        for ep, iv, i, m in self.p['eps']:
            k = (ep & 15) + (16 if ep & 0x80 else 0)
            tipo[k], ival[k], ifn[k], mps[k] = T_INTERRUPT, iv, i, m
        self.send(EP_INFO, 0, bytes(tipo) + bytes(ival) + bytes(ifn) + struct.pack('<32H', *mps))

    def conectar(self):
        self.info()
        dev = self.p['dev']
        vid, pid, ver = struct.unpack_from('<HHH', dev, 8)
        self.send(DEV_CONNECT, 0, struct.pack('<BBBBHHH', 1, dev[4], dev[5], dev[6], vid, pid, ver))

    def control(self, ident, d):
        ep, req, rt, st, val, idx, ln = struct.unpack_from('<BBBBHHH', d)
        out = None
        if rt == 0x80 and req == 6:                 # GET_DESCRIPTOR
            t = val >> 8
            if t == 1:
                out = self.p['dev']
            elif t == 2:
                out = self.p['cfg']
        elif rt == 0x81 and req == 6:               # descriptores de la interfaz HID
            if val >> 8 == 0x22 and self.p['informe']:
                out = self.p['informe']
        elif rt in (0x80, 0x81, 0x82) and req == 0:   # GET_STATUS
            out = b'\0\0'
        elif rt == 0x21 and req in (0x09, 0x0A, 0x0B):   # SET_REPORT, SET_IDLE, SET_PROTOCOL
            out = b''
        self.log.append('control %02x %02x %04x %04x %d -> %s' % (rt, req, val, idx, ln,
                        'STALL' if out is None else len(out[:ln])))
        if out is None:
            self.send(CONTROL, ident, struct.pack('<BBBBHHH', ep, req, rt, STALL, val, idx, 0))
        else:
            out = out[:ln] if rt & 0x80 else b''
            self.send(CONTROL, ident, struct.pack('<BBBBHHH', ep, req, rt, OK, val, idx, len(out)) + out)

    def paquete(self, tipo, ident, d):
        if tipo == HELLO:
            self.hello = True
            self.conectar()
        elif tipo == CONTROL:
            self.control(ident, d)
        elif tipo == SET_CONF:
            self.info()
            self.send(CONF_STATUS, ident, struct.pack('<BB', OK, d[0]))
            self.log.append('set_configuration %d' % d[0])
        elif tipo == GET_CONF:
            self.send(CONF_STATUS, ident, struct.pack('<BB', OK, 1))
        elif tipo == SET_ALT:
            self.send(ALT_STATUS, ident, struct.pack('<BBB', OK, d[0], d[1]))
        elif tipo == GET_ALT:
            self.send(ALT_STATUS, ident, struct.pack('<BBB', OK, d[0], 0))
        elif tipo == START_INT:
            self.send(INT_STATUS, ident, struct.pack('<BB', OK, d[0]))
            self.log.append('start_interrupt_receiving %02x' % d[0])
            if d[0] == self.p['ep']:
                self.recibiendo = True
                if self.pend is not None:
                    self.informe(self.pend)
        elif tipo == STOP_INT:
            self.send(INT_STATUS, ident, struct.pack('<BB', OK, d[0]))
            if d[0] == self.p['ep']:
                self.recibiendo = False
        # lo demás (reset, cancelar, filtros) no pide respuesta

    def informe(self, datos):
        if not self.recibiendo:
            self.pend = datos           # aún no lo ha pedido nadie: el último, al empezar
            return
        self.pend = None
        self.nid += 1
        self.send(INTERRUPT, self.nid, struct.pack('<BBH', self.p['ep'], OK, len(datos)) + datos)

    def atender(self, segundos):
        """atiende a QEMU durante ese tiempo; False si ha cerrado"""
        fin = time.time() + segundos
        while True:
            falta = fin - time.time()
            if falta <= 0:
                return True
            r, _, _ = select.select([self.s], [], [], falta)
            if not r:
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


def main():
    m = Mando(sys.argv[1], perfil(sys.argv[2]))
    a = sys.argv[3:]
    i = 0
    vivo = True
    visto = 0
    while i < len(a) and vivo:
        if a[i] == 'rep':
            m.informe(bytes.fromhex(a[i + 1]))
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
                    d = open(os.environ['USBMANDO_VE'], 'rb').read()
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
            vivo = m.atender(0.2)
            i += 1
        else:
            raise SystemExit('orden desconocida: ' + a[i])
    while vivo:
        vivo = m.atender(1)
    if os.environ.get('USBMANDO_LOG'):
        open(os.environ['USBMANDO_LOG'], 'w').write('\n'.join(m.log) + '\n')


if __name__ == '__main__':
    main()
