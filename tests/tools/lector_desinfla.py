#!/usr/bin/env python3
"""Prueba del descompresor (programas/comun/desinfla.inc) fuera de Hispa Ventanas.

uso: lector_desinfla.py [CUANTOS_AL_AZAR]

Toma desinfla.inc TAL CUAL, lo ensambla con NASM dentro de un programa de Linux
de 32 bits que lee casos por la entrada (clase, datos comprimidos y lo que
cabe en el destino) y devuelve lo que sale, y lo compara con el «zlib» de
Python: bloques sin comprimir, con códigos fijos y con códigos propios, datos
grandes, todos los niveles y estrategias, el destino demasiado pequeño, datos
cortados en cualquier sitio y datos estropeados (que den error, no un cuelgue
ni una escritura fuera de sitio: detrás del destino hay un centinela).
"""
import os, random, struct, subprocess, sys, tempfile, zlib

RAIZ = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
CABE = 2 * 1024 * 1024

ARNES = r'''
        bits 32
        cpu 386
CABE    equ %d
        section .text
        global _start
; lee EDX octetos en ECX (todos); si se acaba la entrada, termina
lee:
        push ebx
.l:
        test edx, edx
        jz .x
        mov eax, 3
        xor ebx, ebx
        int 0x80
        cmp eax, 0
        jle fin
        add ecx, eax
        sub edx, eax
        jmp .l
.x:
        pop ebx
        ret
escribe:
        push ebx
.l:
        test edx, edx
        jz .x
        mov eax, 4
        mov ebx, 1
        int 0x80
        cmp eax, 0
        jle fin
        add ecx, eax
        sub edx, eax
        jmp .l
.x:
        pop ebx
        ret
fin:
        mov eax, 1
        xor ebx, ebx
        int 0x80
_start:
        cld
.caso:
        mov ecx, cab                ; clase (4), octetos de entrada (4), lo que cabe (4)
        mov edx, 12
        call lee
        mov ecx, entrada
        mov edx, [cab+4]
        call lee
        mov edi, salida             ; el destino, lleno de basura conocida
        mov ecx, CABE + 64
        mov al, 0xA5
        rep stosb
        mov esi, entrada
        mov ecx, [cab+4]
        mov edi, salida
        mov edx, [cab+8]
        mov ebx, 0x11223344         ; (que conserve los registros)
        mov ebp, 0x55667788
        cmp dword [cab], 0
        jne .zlib
        call desinfla
        jmp .hecho
.zlib:
        call desinfla_zlib
.hecho:
        setc byte [res]
        mov [res+4], eax
        xor eax, eax
        cmp ebx, 0x11223344
        jne .reg
        cmp ebp, 0x55667788
        jne .reg
        cmp esi, entrada
        jne .reg
        cmp edi, salida
        jne .reg
        inc eax
.reg:
        mov [res+1], al
        mov eax, [dsf_error]
        mov [res+2], al
        mov eax, [dsf_suma]
        mov [res+3], al
        mov eax, [dsf_usado]
        mov [res+8], eax
        ; el centinela: ¿ha escrito detrás de lo que cabía?
        mov esi, salida
        add esi, [cab+8]
        mov ecx, 64
        mov byte [res+12], 1
.cen:
        lodsb
        cmp al, 0xA5
        jne .roto
        dec ecx
        jnz .cen
        jmp .cen_ok
.roto:
        mov byte [res+12], 0
.cen_ok:
        mov ecx, res
        mov edx, 16
        call escribe
        mov ecx, salida
        mov edx, [res+4]
        cmp edx, CABE
        jbe .e
        mov edx, CABE
.e:
        call escribe
        jmp .caso

%%include "desinfla.inc"

        section .bss
cab     resd 3
res     resb 16
        DESINFLA_BSS
entrada resb CABE + 64
salida  resb CABE + 128
'''

fallos = 0
total = 0


def monta(tmp):
    inc = open(os.path.join(RAIZ, 'programas', 'comun', 'desinfla.inc'), encoding='utf-8').read()
    open(os.path.join(tmp, 'desinfla.inc'), 'w', encoding='cp437', errors='replace').write(inc)
    asm = os.path.join(tmp, 'arnes.asm')
    open(asm, 'w').write(ARNES % CABE)
    subprocess.run([os.environ.get('NASM', 'nasm'), '-f', 'elf32', '-i', tmp + os.sep, '-o', asm + '.o', asm], check=True)
    subprocess.run(['ld', '-m', 'elf_i386', '-o', os.path.join(tmp, 'arnes'), asm + '.o'], check=True)
    return os.path.join(tmp, 'arnes')


class Casos:
    def __init__(self):
        self.l = []

    def pon(self, nombre, clase, datos, cabe, espera):
        """espera: (cf, error o None, salida o None (prefijo si hay error), suma o None)"""
        assert len(datos) <= CABE and cabe <= CABE
        self.l.append((nombre, clase, datos, cabe, espera))

    def corre(self, prog):
        global fallos, total
        ent = b''.join(struct.pack('<III', c, len(d), cabe) + d for _, c, d, cabe, _ in self.l)
        sal = subprocess.run([prog], input=ent, stdout=subprocess.PIPE, timeout=600).stdout
        p = 0
        for nombre, clase, datos, cabe, (cf, err, out, suma) in self.l:
            total += 1
            if p + 16 > len(sal):
                print('  FAIL %s: el programa se ha parado (¿un cuelgue o una escritura fuera de sitio?)' % nombre)
                fallos += 1
                break
            rcf, rreg, rerr, rsuma, rn, rus, rcen = struct.unpack_from('<BBBBIIB', sal, p)
            dat = sal[p + 16:p + 16 + min(rn, CABE)]
            p += 16 + min(rn, CABE)
            mal = []
            if not rreg:
                mal.append('no conserva los registros')
            if not rcen:
                mal.append('ha escrito detrás del destino')
            if rn > cabe:
                mal.append('dice que ha escrito %d y cabían %d' % (rn, cabe))
            if cf is not None and rcf != cf:
                mal.append('CF=%d y se esperaba %d (error %d)' % (rcf, cf, rerr))
            if err is not None and rcf and rerr != err:
                mal.append('error %d y se esperaba %d' % (rerr, err))
            if rcf and rerr not in (1, 2, 3):
                mal.append('error sin clase')
            if out is not None:
                if rcf == 0 and dat != out:
                    mal.append('la salida no es la esperada (%d octetos, se esperaban %d)' % (len(dat), len(out)))
                if rcf == 1 and dat != out[:len(dat)]:
                    mal.append('lo escrito antes del error no es el principio de lo esperado')
            if suma is not None and rcf == 0 and rsuma != suma:
                mal.append('suma=%d y se esperaba %d' % (rsuma, suma))
            if rcf == 0 and rus > len(datos):
                mal.append('dice haber gastado más de lo que había')
            if mal:
                fallos += 1
                if fallos <= 30:
                    print('  FAIL %s: %s' % (nombre, '; '.join(mal)))
        self.l = []


def crudo(d, nivel=6, estrategia=zlib.Z_DEFAULT_STRATEGY, mem=8):
    c = zlib.compressobj(nivel, zlib.DEFLATED, -15, mem, estrategia)
    return c.compress(d) + c.flush()


def con_zlib(d, nivel=6, estrategia=zlib.Z_DEFAULT_STRATEGY, wbits=15):
    c = zlib.compressobj(nivel, zlib.DEFLATED, wbits, 8, estrategia)
    return c.compress(d) + c.flush()


def datos_de(rnd, clase, n):
    if clase == 0:
        return bytes(rnd.getrandbits(8) for _ in range(n))                 # ruido: no se comprime
    if clase == 1:
        return bytes(rnd.choice(b'abcdefgh \n') for _ in range(n))           # pocas letras
    if clase == 2:
        pal = [b'Hola', b'mundo', b' ', b'factura', b'BT /F1 12 Tf', b' Td ', b'(texto) Tj\n', b'0 0 1 rg', b'\xe1\xf1']
        return b''.join(rnd.choice(pal) for _ in range(n // 5 + 1))[:n]     # como un PDF
    if clase == 3:
        return bytes([rnd.getrandbits(8)]) * n                               # un solo octeto repetido
    if clase == 4:
        t = bytes(rnd.getrandbits(8) for _ in range(rnd.randint(1, 300)))    # un trozo repetido (distancias)
        return (t * (n // len(t) + 1))[:n]
    b = bytearray()                                                          # mezcla, con repeticiones lejanas
    while len(b) < n:
        if b and rnd.random() < 0.5:
            i = rnd.randrange(len(b)); l = rnd.randint(3, 400)
            b += b[i:i + l]
        else:
            b += bytes(rnd.getrandbits(8) for _ in range(rnd.randint(1, 40)))
    return bytes(b[:n])


def main():
    cuantos = int(sys.argv[1]) if len(sys.argv) > 1 else 1500
    rnd = random.Random(20261116)
    with tempfile.TemporaryDirectory() as tmp:
        prog = monta(tmp)
        c = Casos()
        # ---- casos elegidos ----
        c.pon('vacío (zlib)', 1, con_zlib(b''), 100, (0, None, b'', 0))
        c.pon('vacío (Deflate)', 0, crudo(b''), 0, (0, None, b'', None))
        c.pon('una letra', 1, con_zlib(b'a'), 1, (0, None, b'a', 0))
        hola = b'Hola, mundo. ' * 40
        for niv in range(10):
            c.pon('nivel %d' % niv, 1, con_zlib(hola, niv), len(hola), (0, None, hola, 0))
        for est, nom in ((zlib.Z_FIXED, 'códigos fijos'), (zlib.Z_HUFFMAN_ONLY, 'solo Huffman'),
                         (zlib.Z_RLE, 'RLE'), (zlib.Z_FILTERED, 'filtrado')):
            for cl in range(6):
                d = datos_de(rnd, cl, 5000)
                c.pon('%s, datos %d' % (nom, cl), 1, con_zlib(d, 9, est), len(d), (0, None, d, 0))
                c.pon('%s, datos %d (Deflate)' % (nom, cl), 0, crudo(d, 9, est), len(d) + 7, (0, None, d, None))
        # bloques sin comprimir, hechos a mano: varios, vacíos y de 65.535
        d = datos_de(rnd, 0, 70000)
        g = b''
        trozos = [d[:0], d[:1], d[1:65536], d[65536:65536], d[65536:]]
        for i, t in enumerate(trozos):
            g += bytes([1 if i == len(trozos) - 1 else 0]) + struct.pack('<HH', len(t), len(t) ^ 0xFFFF) + t
        c.pon('bloques guardados', 0, g, len(d), (0, None, d, None))
        c.pon('bloques guardados (zlib)', 1, b'\x78\x01' + g + struct.pack('>I', zlib.adler32(d)), len(d), (0, None, d, 0))
        c.pon('guardado con el complemento mal', 0, b'\x01\x05\x00\x00\x00abcde', 10, (1, 1, b'', None))
        c.pon('guardado cortado', 0, b'\x01\x05\x00\xfa\xffabc', 10, (1, 2, b'abcde', None))
        c.pon('clase de bloque 3', 0, b'\x07\x00', 10, (1, 1, b'', None))
        # mezcla de clases de bloque en un mismo flujo (Z_FULL_FLUSH cambia de bloque)
        co = zlib.compressobj(9, zlib.DEFLATED, -15)
        partes, todo = b'', b''
        for i in range(12):
            t = datos_de(rnd, i % 6, rnd.randint(1, 4000))
            todo += t
            partes += co.compress(t) + co.flush(rnd.choice([zlib.Z_SYNC_FLUSH, zlib.Z_FULL_FLUSH, zlib.Z_NO_FLUSH]))
        partes += co.flush()
        c.pon('muchos bloques', 0, partes, len(todo), (0, None, todo, None))
        # grandes
        for cl in range(6):
            d = datos_de(rnd, cl, 1500000 if cl else 900000)
            c.pon('grande, datos %d' % cl, 1, con_zlib(d, 6), len(d), (0, None, d, 0))
        d = b'\0' * CABE                                    # la mayor compresión: largos de 258 seguidos
        c.pon('dos megas de ceros', 1, con_zlib(d, 9), len(d), (0, None, d, 0))
        # ventanas pequeñas (otras cabeceras de zlib)
        for wb in range(9, 16):
            d = datos_de(rnd, 5, 20000)
            c.pon('ventana de %d bits' % wb, 1, con_zlib(d, 6, wbits=wb), len(d), (0, None, d, 0))
        # la suma y la cabecera
        z = con_zlib(hola)
        c.pon('sin suma', 1, z[:-4], len(hola), (0, None, hola, 2))
        c.pon('suma a medias', 1, z[:-2], len(hola), (0, None, hola, 2))
        c.pon('suma equivocada', 1, z[:-1] + bytes([z[-1] ^ 1]), len(hola), (0, None, hola, 1))
        c.pon('cabecera que no es de zlib', 1, b'\x1f\x8b' + z[2:], len(hola), (1, 1, b'', None))
        c.pon('cabecera con diccionario', 1, b'\x78\xbb' + z[2:], len(hola), (1, 1, b'', None))
        c.pon('un solo octeto', 1, b'\x78', 10, (1, 2, b'', None))
        c.pon('nada', 1, b'', 10, (1, 2, b'', None))
        c.pon('nada (Deflate)', 0, b'', 10, (1, 2, b'', None))
        c.pon('distancia antes del principio', 0, b'\x03\x02\x00\x00', 50, (1, 1, b'', None))
        # el destino pequeño: error 3 y lo que quepa, bien
        for cl in range(6):
            d = datos_de(rnd, cl, 3000)
            z = con_zlib(d, 9)
            for cabe in (0, 1, 2, 257, 258, 259, 1500, 2999):
                c.pon('no cabe (%d de 3000), datos %d' % (cabe, cl), 1, z, cabe, (1, 3, d, None))
            c.pon('cabe justo, datos %d' % cl, 1, z, 3000, (0, None, d, 0))
        c.corre(prog)
        # ---- cortado en cada octeto: siempre error 2 (o bien, si solo falta la suma) ----
        for cl in (1, 2, 5):
            d = datos_de(rnd, cl, 700)
            for est in (zlib.Z_DEFAULT_STRATEGY, zlib.Z_FIXED):
                z = con_zlib(d, 9, est)
                for corte in range(len(z)):
                    if corte >= len(z) - 4:
                        c.pon('cortado en %d de %d' % (corte, len(z)), 1, z[:corte], 700, (0, None, d, 2))
                    else:
                        c.pon('cortado en %d de %d' % (corte, len(z)), 1, z[:corte], 700, (1, 2, d, None))
        c.corre(prog)
        # ---- al azar: comprimir y descomprimir ----
        for i in range(cuantos):
            d = datos_de(rnd, rnd.randrange(6), rnd.choice([rnd.randint(0, 50), rnd.randint(0, 3000), rnd.randint(0, 70000)]))
            niv = rnd.randint(0, 9)
            est = rnd.choice([zlib.Z_DEFAULT_STRATEGY] * 3 + [zlib.Z_FIXED, zlib.Z_HUFFMAN_ONLY, zlib.Z_RLE, zlib.Z_FILTERED])
            if rnd.random() < 0.5:
                c.pon('azar %d (zlib)' % i, 1, con_zlib(d, niv, est), len(d) + rnd.randint(0, 9), (0, None, d, 0))
            else:
                c.pon('azar %d (Deflate)' % i, 0, crudo(d, niv, est, rnd.randint(1, 9)), len(d), (0, None, d, None))
        c.corre(prog)
        # ---- estropeados: lo que diga, pero sin colgarse ni escribir fuera; si
        #      dice que está bien, tiene que coincidir con lo que diga zlib ----
        for i in range(cuantos * 2):
            d = datos_de(rnd, rnd.choice([1, 2, 4, 5]), rnd.randint(1, 2000))
            z = bytearray(crudo(d, rnd.randint(1, 9), rnd.choice([zlib.Z_DEFAULT_STRATEGY, zlib.Z_FIXED])))
            for _ in range(rnd.randint(1, 3)):
                z[rnd.randrange(len(z))] ^= 1 << rnd.randrange(8)
            z = bytes(z)
            cabe = rnd.choice([len(d), 4000, 100000])
            try:
                do = zlib.decompressobj(-15)
                bien = do.decompress(z)
                acabado = do.eof
            except zlib.error:
                bien, acabado = None, False
            if acabado and len(bien) <= cabe:
                c.pon('estropeado %d (zlib lo da por bueno)' % i, 0, z, cabe, (0, None, bien, None))
            elif acabado:
                c.pon('estropeado %d (no cabe)' % i, 0, z, cabe, (1, 3, bien, None))
            else:
                c.pon('estropeado %d' % i, 0, z, cabe, (1, None, None, None))
        for i in range(cuantos):                            # y ruido puro
            z = bytes(rnd.getrandbits(8) for _ in range(rnd.randint(0, 300)))
            try:
                do = zlib.decompressobj(-15)
                bien = do.decompress(z, 5000)
                acabado = do.eof
            except zlib.error:
                bien, acabado = None, False
            if acabado:
                c.pon('ruido %d (zlib lo da por bueno)' % i, 0, z, 5000, (0, None, bien, None))
            else:
                c.pon('ruido %d' % i, 0, z, 5000, (1, None, None, None))
        c.corre(prog)
    print('  %s desinfla.inc contra zlib: %d casos, %d fallos' % ('OK  ' if not fallos else 'FAIL', total, fallos))
    sys.exit(1 if fallos else 0)


if __name__ == '__main__':
    main()
