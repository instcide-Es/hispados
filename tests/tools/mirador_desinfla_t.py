#!/usr/bin/env python3
"""Prueba del descompresor por tramos (programas/comun/desinfla_t.inc) fuera de Hispa Ventanas.

uso: mirador_desinfla_t.py [CUANTOS_AL_AZAR]

Como tests/tools/lector_desinfla.py con desinfla.inc: toma desinfla_t.inc TAL
CUAL, lo ensambla con NASM dentro de un programa de Linux de 32 bits y le
pasa casos. De cada caso, el programa descomprime dos veces: con desinfla.inc
(de una vez, de memoria a memoria) y con desinfla_t.inc, dándole la entrada a
trozos y pidiéndole la salida a trozos (de un octeto, de muchos, al azar...).
Aquí se compara lo que sale con el «zlib» de Python y, además, lo uno con lo
otro: tienen que dar lo mismo (los mismos octetos, el mismo final o el mismo
error, la misma suma y lo mismo gastado), se parta como se parta.
  - bloques sin comprimir, con códigos fijos y con códigos propios, datos
    grandes (más que la ventana de 32 K, con copias que le dan la vuelta),
    todos los niveles y estrategias, ventanas pequeñas;
  - el destino demasiado pequeño, datos cortados en cada octeto y miles de
    datos estropeados: error, nunca un cuelgue.
Cada trozo de entrada se le da pegado a una página prohibida (leer un octeto
de más es una violación de segmento) y detrás del destino y de su memoria de
trabajo hay centinelas. Escribe «  OK   ...» o «  FAIL ...».
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

; -> EAX=el tamaño del trozo siguiente según [EBX] (el paso: 0, todo; N, de N
; en N; con el bit 31, al azar de 1 a 300 con la semilla de [EBX+4])
paso:
        mov eax, [ebx]
        test eax, eax
        jz .todo
        jns .x
        mov eax, [ebx+4]
        imul eax, 1103515245
        add eax, 12345
        mov [ebx+4], eax
        shr eax, 16
        xor edx, edx
        mov ecx, 300
        div ecx
        lea eax, [edx+1]
        ret
.todo:
        mov eax, 0x7FFFFFFF
.x:
        ret

; da al descompresor el trozo siguiente de la entrada (o le dice que no hay
; más): copiado al final de la zona, pegado a la página prohibida
da_entrada:
        pushad
        mov ebx, paso_in
        call paso
        mov ecx, [cab+4]
        sub ecx, [in_pos]
        cmp eax, ecx
        jbe .a
        mov eax, ecx
.a:
        mov ecx, eax                ; ECX=octetos del trozo (0: no hay más)
        mov esi, entrada
        add esi, [in_pos]
        add [in_pos], ecx
        mov edi, [zona_fin]
        sub edi, ecx
        push edi
        push ecx
        rep movsb
        pop ecx
        pop esi
        call dst_entrada
        popad
        ret

_start:
        cld
        ; la zona de los trozos de entrada: acaba en una página prohibida
        mov eax, 192                ; mmap2
        xor ebx, ebx
        mov ecx, CABE + 8192
        mov edx, 3
        mov esi, 0x22
        mov edi, -1
        xor ebp, ebp
        int 0x80
        cmp eax, -4096
        ja fin
        add eax, CABE + 4096
        mov [zona_fin], eax
        mov ebx, eax
        mov ecx, 4096
        xor edx, edx
        mov eax, 125                ; mprotect
        int 0x80
.caso:
        mov ecx, cab                ; clase, octetos de entrada, lo que cabe, paso de entrada, paso de salida
        mov edx, 20
        call lee
        mov ecx, entrada
        mov edx, [cab+4]
        call lee
        mov edi, res
        mov ecx, 8
        xor eax, eax
        rep stosd
        ; ---- la referencia: desinfla.inc, de una vez ----
        mov edi, salida2
        mov ecx, CABE + 64
        mov al, 0xA5
        rep stosb
        mov esi, entrada
        mov ecx, [cab+4]
        mov edi, salida2
        mov edx, [cab+8]
        test dword [cab], 1
        jnz .rz
        call desinfla
        jmp .rh
.rz:
        call desinfla_zlib
.rh:
        setc byte [res+13]
        mov [res+16], eax
        mov eax, [dsf_error]
        mov [res+14], al
        mov eax, [dsf_suma]
        mov [res+15], al
        mov eax, [dsf_usado]
        mov [res+20], eax
        ; ---- desinfla_t.inc, a trozos ----
        mov edi, salida
        mov ecx, CABE + 64
        mov al, 0xA5
        rep stosb
        mov edi, trab - 64
        mov ecx, DST_MEM + 128
        mov al, 0x5A
        rep stosb
        mov eax, [cab+12]           ; los pasos (y, si son al azar, su semilla: los mismos bits)
        mov [paso_in], eax
        mov [paso_in+4], eax
        mov eax, [cab+16]
        mov [paso_out], eax
        mov [paso_out+4], eax
        mov dword [in_pos], 0
        mov dword [n_out], 0
        mov byte [res+1], 1
        mov edi, trab
        mov eax, [cab]
        and eax, 1
        mov ebx, 0x11223344
        mov ebp, 0x55667788
        call dst_empieza
        cmp ebx, 0x11223344
        jne .reg0
        cmp ebp, 0x55667788
        jne .reg0
        cmp edi, trab
        je .emp
.reg0:
        mov byte [res+1], 0
.emp:
        call da_entrada
.pide:
        ; cuánto se pide: lo que diga el paso de salida, sin pasar de lo que cabe
        mov ebx, paso_out
        call paso
        mov ecx, [cab+8]
        sub ecx, [n_out]
        cmp eax, ecx
        jbe .p1
        mov eax, ecx
.p1:
        mov ecx, eax
        mov edi, salida
        add edi, [n_out]
        test ecx, ecx
        jnz .p2
        ; ya no cabe más: ¿queda algo por salir? Se pide un octeto aparte
        mov edi, sobra
        mov ecx, 1
        mov ebx, 0x11223344
        mov ebp, 0x55667788
        mov esi, 0x99AABBCC
        call dst_sigue
        call .registros
        cmp edx, DST_HAMBRE
        jne .s1
        call da_entrada
        jmp .pide
.s1:
        cmp edx, DST_HECHO
        jne .acaba
        mov edx, 4                  ; (lo que en desinfla.inc es «no cabe»)
        jmp .acaba
.p2:
        mov ebx, 0x11223344
        mov ebp, 0x55667788
        mov esi, 0x99AABBCC
        mov [quiere], ecx
        mov [donde], edi
        call dst_sigue
        call .registros
        cmp ecx, [quiere]
        jne .reg1
        cmp edi, [donde]
        jne .reg1
        cmp eax, ecx                ; (nunca más de lo pedido; y «hecho» es justo lo pedido)
        ja .reg1
        cmp edx, DST_HECHO
        jne .p3
        cmp eax, ecx
        je .p3
.reg1:
        mov byte [res+1], 0
.p3:
        add [n_out], eax
        cmp edx, DST_HECHO
        je .pide
        cmp edx, DST_HAMBRE
        jne .acaba
        call da_entrada
        jmp .pide
.acaba:
        mov [res], dl
        mov eax, [dst_error]
        mov [res+2], al
        mov eax, [dst_suma]
        mov [res+3], al
        mov eax, [n_out]
        mov [res+4], eax
        mov eax, [dst_usado]
        mov [res+8], eax
        ; después de acabar no da más: lo mismo, y cero octetos
        cmp edx, 4
        je .cen
        mov ebp, edx
        mov edi, sobra
        mov ecx, 1
        call dst_sigue
        test eax, eax
        jnz .reg2
        cmp edx, ebp
        je .cen
.reg2:
        mov byte [res+1], 0
.cen:
        ; los centinelas: detrás del destino, y delante y detrás de la memoria de trabajo
        mov byte [res+12], 1
        mov esi, salida
        add esi, [cab+8]
        mov ecx, 64
        mov ah, 0xA5
        call .mira
        mov esi, trab - 64
        mov ecx, 64
        mov ah, 0x5A
        call .mira
        mov esi, trab + DST_MEM
        mov ecx, 64
        call .mira
        ; ¿ha salido lo mismo que con desinfla.inc?
        mov byte [res+24], 0
        mov ecx, [n_out]
        cmp ecx, [res+16]
        jne .da
        mov esi, salida
        mov edi, salida2
        repe cmpsb
        jne .da
        mov byte [res+24], 1
.da:
        mov ecx, res
        mov edx, 32
        call escribe
        mov ecx, salida
        mov edx, [n_out]
        call escribe
        jmp .caso
; (interna) ¿ha conservado EBX, EBP y ESI?
.registros:
        cmp ebx, 0x11223344
        jne .r_no
        cmp ebp, 0x55667788
        jne .r_no
        cmp esi, 0x99AABBCC
        je .r_si
.r_no:
        mov byte [res+1], 0
.r_si:
        ret
; (interna) ¿siguen los ECX octetos de ESI valiendo AH?
.mira:
        lodsb
        cmp al, ah
        jne .m_no
        dec ecx
        jnz .mira
        ret
.m_no:
        mov byte [res+12], 0
        ret

%%include "desinfla.inc"
%%include "desinfla_t.inc"

        section .bss
cab     resd 7
res     resb 32
zona_fin resd 1
in_pos  resd 1
n_out   resd 1
quiere  resd 1
donde   resd 1
paso_in resd 2
paso_out resd 2
sobra   resb 16
        DESINFLA_BSS
        DESINFLA_T_BSS
        resb 64
trab    resb DST_MEM + 64
entrada resb CABE + 64
salida  resb CABE + 128
salida2 resb CABE + 128
'''

fallos = 0
total = 0
partidos = 0


def monta(tmp):
    for f in ('desinfla', 'desinfla_t'):
        inc = open(os.path.join(RAIZ, 'programas', 'comun', f + '.inc'), encoding='utf-8').read()
        open(os.path.join(tmp, f + '.inc'), 'w', encoding='cp437', errors='replace').write(inc)
    asm = os.path.join(tmp, 'arnes.asm')
    open(asm, 'w').write(ARNES % CABE)
    subprocess.run([os.environ.get('NASM', 'nasm'), '-f', 'elf32', '-i', tmp + os.sep, '-o', asm + '.o', asm], check=True)
    subprocess.run(['ld', '-m', 'elf_i386', '-o', os.path.join(tmp, 'arnes'), asm + '.o'], check=True)
    return os.path.join(tmp, 'arnes')


# cómo se parten la entrada y la salida: (paso de entrada, paso de salida); 0,
# de una vez; N, de N en N; None, al azar (de 1 a 300 octetos cada trozo)
PARTES = ((0, 0), (1, 1), (None, None), (0, 37), (7, 0), (1, 0), (0, 1), (4096, 32768), (None, 65536))


class Casos:
    def __init__(self, rnd):
        self.l = []
        self.rnd = rnd

    def pon(self, nombre, clase, datos, cabe, espera, partes=PARTES):
        """espera: (cf, error o None, salida o None (prefijo si hay error), suma o None), como en desinfla.inc"""
        assert len(datos) <= CABE and cabe <= CABE
        for pi, po in partes:
            a = pi if pi is not None else 0x80000000
            b = po if po is not None else 0x80000000
            self.l.append(('%s [entrada %s, salida %s]' % (nombre, pi or ('azar' if pi is None else 'entera'),
                                                          po or ('azar' if po is None else 'entera')),
                           clase, datos, cabe, espera, a, b, self.rnd.getrandbits(31), self.rnd.getrandbits(31)))

    def corre(self, prog):
        global fallos, total, partidos
        ent = bytearray()
        for _, c, d, cabe, _, a, b, sa, sb in self.l:
            # (el arnés toma el paso y, si es al azar, la semilla: van en los dos mismos campos)
            ent += struct.pack('<IIIII', c, len(d), cabe, a if a != 0x80000000 else (0x80000000 | sa), b if b != 0x80000000 else (0x80000000 | sb)) + d
        sal = subprocess.run([prog], input=bytes(ent), stdout=subprocess.PIPE, timeout=1200).stdout
        p = 0
        for nombre, clase, datos, cabe, (cf, err, out, suma), a, b, sa, sb in self.l:
            total += 1
            if a or b:
                partidos += 1
            if p + 32 > len(sal):
                print('  FAIL %s: el programa se ha parado (¿un cuelgue o un acceso fuera de sitio?)' % nombre)
                fallos += 1
                break
            (est, reg, terr, tsuma, tn, tus, cen, rcf, rerr, rsuma, rn, rus, igual) = struct.unpack_from('<BBBBIIBBBBIIB', sal, p)
            dat = sal[p + 32:p + 32 + tn]
            p += 32 + tn
            mal = []
            # como lo diría desinfla.inc: CF y la clase del error (3: no cabe)
            tcf = 0 if est == 2 else 1
            terr = {2: 0, 3: terr, 4: 3}.get(est, 99)
            if est not in (2, 3, 4):
                mal.append('acaba con un estado que no existe (%d)' % est)
            if not reg:
                mal.append('no conserva los registros, escribe más de lo pedido o sigue dando tras acabar')
            if not cen:
                mal.append('ha escrito fuera del destino o de su memoria de trabajo')
            if tn > cabe:
                mal.append('dice que ha escrito %d y cabían %d' % (tn, cabe))
            if cf is not None and tcf != cf:
                mal.append('acaba %s (error %d) y se esperaba %s' % ('mal' if tcf else 'bien', terr, 'mal' if cf else 'bien'))
            if err is not None and tcf and terr != err:
                mal.append('error %d y se esperaba %d' % (terr, err))
            if tcf and terr not in (1, 2, 3):
                mal.append('error sin clase')
            if out is not None:
                if tcf == 0 and dat != out:
                    mal.append('la salida no es la esperada (%d octetos, se esperaban %d)' % (len(dat), len(out)))
                if tcf == 1 and dat != out[:len(dat)]:
                    mal.append('lo escrito antes del error no es el principio de lo esperado')
            if suma is not None and tcf == 0 and tsuma != suma:
                mal.append('suma=%d y se esperaba %d' % (tsuma, suma))
            if tcf == 0 and tus > len(datos):
                mal.append('dice haber gastado más de lo que había')
            # y lo mismo que desinfla.inc
            if tcf != rcf or (tcf and terr != rerr):
                mal.append('desinfla.inc acaba %s (error %d) y este %s (error %d)' % ('mal' if rcf else 'bien', rerr, 'mal' if tcf else 'bien', terr))
            elif not igual:
                mal.append('no sale lo mismo que con desinfla.inc (%d octetos y %d)' % (tn, rn))
            elif tcf == 0 and (tus != rus or (clase & 1 and tsuma != rsuma)):
                mal.append('desinfla.inc gasta %d (suma %d) y este %d (suma %d)' % (rus, rsuma, tus, tsuma))
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


def fijo(piezas):
    """un bloque Deflate con los códigos fijos, hecho a mano: «piezas» son
    octetos sueltos (bytes) o copias (largo, distancia). zlib nunca copia de
    más de 32.506 octetos atrás; aquí se llega a los 32.768 que admite el formato"""
    LB = [3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258]
    LE = [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0]
    DB = [1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
          8193, 12289, 16385, 24577]
    DE = [0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13]
    bits = []

    def pon(v, n):                      # n bits, el de menos peso primero
        bits.extend((v >> i) & 1 for i in range(n))

    def codigo(v, n):                   # un código de Huffman: el de más peso primero
        bits.extend((v >> (n - 1 - i)) & 1 for i in range(n))

    def simbolo(s):
        if s < 144:
            codigo(0x30 + s, 8)
        elif s < 256:
            codigo(0x190 + s - 144, 9)
        elif s < 280:
            codigo(s - 256, 7)
        else:
            codigo(0xC0 + s - 280, 8)

    pon(1, 1)
    pon(1, 2)
    for p in piezas:
        if isinstance(p, tuple):
            largo, dist = p
            i = max(k for k in range(29) if LB[k] <= largo)
            if largo == 258:
                i = 28
            simbolo(257 + i)
            pon(largo - LB[i], LE[i])
            j = max(k for k in range(30) if DB[k] <= dist)
            codigo(j, 5)
            pon(dist - DB[j], DE[j])
        else:
            for b in p:
                simbolo(b)
    simbolo(256)
    out = bytearray()
    for i in range(0, len(bits), 8):
        out.append(sum(b << k for k, b in enumerate(bits[i:i + 8])))
    return bytes(out)


def aplica(piezas):
    """lo que tiene que salir de esas piezas"""
    out = bytearray()
    for p in piezas:
        if isinstance(p, tuple):
            for _ in range(p[0]):
                out.append(out[-p[1]])
        else:
            out += p
    return bytes(out)


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
    if clase == 6:                                                           # como un PNG: filas parecidas, con su filtro
        fila = bytearray(rnd.getrandbits(8) for _ in range(rnd.randint(8, 3000)))
        b = bytearray()
        while len(b) < n:
            for _ in range(rnd.randint(0, 6)):
                fila[rnd.randrange(len(fila))] = rnd.getrandbits(8)
            b += bytes((rnd.randrange(5),)) + fila
        return bytes(b[:n])
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
    rnd = random.Random(20261117)
    UNA = ((None, None),)
    with tempfile.TemporaryDirectory() as tmp:
        prog = monta(tmp)
        c = Casos(rnd)
        # ---- casos elegidos (cada uno, partido de todas las maneras) ----
        c.pon('vacío (zlib)', 1, con_zlib(b''), 100, (0, None, b'', 0))
        c.pon('vacío (Deflate)', 0, crudo(b''), 0, (0, None, b'', None))
        c.pon('una letra', 1, con_zlib(b'a'), 1, (0, None, b'a', 0))
        hola = b'Hola, mundo. ' * 40
        for niv in range(10):
            c.pon('nivel %d' % niv, 1, con_zlib(hola, niv), len(hola), (0, None, hola, 0))
        for est, nom in ((zlib.Z_FIXED, 'códigos fijos'), (zlib.Z_HUFFMAN_ONLY, 'solo Huffman'),
                         (zlib.Z_RLE, 'RLE'), (zlib.Z_FILTERED, 'filtrado')):
            for cl in range(7):
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
            t = datos_de(rnd, i % 7, rnd.randint(1, 4000))
            todo += t
            partes += co.compress(t) + co.flush(rnd.choice([zlib.Z_SYNC_FLUSH, zlib.Z_FULL_FLUSH, zlib.Z_NO_FLUSH]))
        partes += co.flush()
        c.pon('muchos bloques', 0, partes, len(todo), (0, None, todo, None))
        # grandes: mucho más que la ventana, con copias que le dan la vuelta
        for cl in range(7):
            d = datos_de(rnd, cl, 1500000 if cl else 900000)
            c.pon('grande, datos %d' % cl, 1, con_zlib(d, 6), len(d), (0, None, d, 0))
        d = b'\0' * CABE                                    # la mayor compresión: largos de 258 seguidos
        c.pon('dos megas de ceros', 1, con_zlib(d, 9), len(d), (0, None, d, 0))
        # distancias de justo 32.768 (toda la ventana) y de una menos, por todas las posiciones
        for dist in (32768, 32767, 32766, 1):
            t = bytes(rnd.getrandbits(8) for _ in range(dist))
            d = t * 5 + t[:1234]
            z = crudo(d, 9)
            c.pon('copias a %d octetos' % dist, 0, z, len(d), (0, None, d, None))
        # y hechas a mano, hasta donde llega el formato: copias de justo 32.768 octetos
        # atrás, con la ventana en todas las posiciones, y una más (que es un error)
        for fase in (0, 1, 5, 257, 258, 259, 4000, 32767, 32768, 32769):
            pz = [bytes(rnd.getrandbits(8) for _ in range(32768 + fase))]
            for _ in range(140):
                pz.append((rnd.choice((3, 4, 100, 257, 258)), rnd.choice((32768, 32768, 32767, 32766, 32768 - 257, 1, 2))))
                if rnd.random() < 0.3:
                    pz.append(bytes(rnd.getrandbits(8) for _ in range(rnd.randint(1, 5))))
            d = aplica(pz)
            c.pon('copias de toda la ventana, fase %d' % fase, 0, fijo(pz), len(d), (0, None, d, None))
        for antes in (0, 1, 2, 100, 32766, 32767):
            ruido = bytes(rnd.getrandbits(8) for _ in range(antes))
            c.pon('distancia de %d con %d octetos fuera' % (antes + 1, antes), 0, fijo([ruido, (5, antes + 1)]), 40000, (1, 1, ruido, None))
            if antes:
                d = aplica([ruido, (300 if antes > 1 else 5, antes)])
                c.pon('distancia de %d con %d octetos fuera' % (antes, antes), 0,
                      fijo([ruido, (258, antes), (42, antes)] if antes > 1 else [ruido, (5, antes)]), 40000, (0, None, d, None))
        # ventanas pequeñas (otras cabeceras de zlib)
        for wb in range(9, 16):
            d = datos_de(rnd, 5, 20000)
            c.pon('ventana de %d bits' % wb, 1, con_zlib(d, 6, wbits=wb), len(d), (0, None, d, 0))
        # la suma y la cabecera
        z = con_zlib(hola)
        c.pon('sin suma', 1, z[:-4], len(hola), (0, None, hola, 2))
        c.pon('suma a medias', 1, z[:-2], len(hola), (0, None, hola, 2))
        c.pon('suma equivocada', 1, z[:-1] + bytes([z[-1] ^ 1]), len(hola), (0, None, hola, 1))
        c.pon('con más datos detrás de la suma', 1, z + b'sobra', len(hola), (0, None, hola, 0))
        c.pon('cabecera que no es de zlib', 1, b'\x1f\x8b' + z[2:], len(hola), (1, 1, b'', None))
        c.pon('cabecera con diccionario', 1, b'\x78\xbb' + z[2:], len(hola), (1, 1, b'', None))
        c.pon('un solo octeto', 1, b'\x78', 10, (1, 2, b'', None))
        c.pon('nada', 1, b'', 10, (1, 2, b'', None))
        c.pon('nada (Deflate)', 0, b'', 10, (1, 2, b'', None))
        c.pon('distancia antes del principio', 0, b'\x03\x02\x00\x00', 50, (1, 1, b'', None))
        # el destino pequeño: lo que quepa, bien, y se sabe que queda más
        for cl in range(7):
            d = datos_de(rnd, cl, 3000)
            z = con_zlib(d, 9)
            for cabe in (0, 1, 2, 257, 258, 259, 1500, 2999):
                c.pon('no cabe (%d de 3000), datos %d' % (cabe, cl), 1, z, cabe, (1, 3, d, None))
            c.pon('cabe justo, datos %d' % cl, 1, z, 3000, (0, None, d, 0))
        c.corre(prog)
        elegidos = total
        # ---- cortado en cada octeto: siempre error 2 (o bien, si solo falta la suma) ----
        for cl in (1, 2, 5):
            d = datos_de(rnd, cl, 700)
            for est in (zlib.Z_DEFAULT_STRATEGY, zlib.Z_FIXED):
                z = con_zlib(d, 9, est)
                for corte in range(len(z)):
                    partes_c = (((0, 0), (1, 1), (None, None))[corte % 3],)
                    if corte >= len(z) - 4:
                        c.pon('cortado en %d de %d' % (corte, len(z)), 1, z[:corte], 700, (0, None, d, 2), partes_c)
                    else:
                        c.pon('cortado en %d de %d' % (corte, len(z)), 1, z[:corte], 700, (1, 2, d, None), partes_c)
        c.corre(prog)
        # ---- al azar: comprimir y descomprimir, partido al azar ----
        for i in range(cuantos):
            d = datos_de(rnd, rnd.randrange(7), rnd.choice([rnd.randint(0, 50), rnd.randint(0, 3000), rnd.randint(0, 70000), rnd.randint(0, 200000)]))
            niv = rnd.randint(0, 9)
            est = rnd.choice([zlib.Z_DEFAULT_STRATEGY] * 3 + [zlib.Z_FIXED, zlib.Z_HUFFMAN_ONLY, zlib.Z_RLE, zlib.Z_FILTERED])
            partes_a = (rnd.choice(PARTES), (None, None))
            if rnd.random() < 0.5:
                c.pon('azar %d (zlib)' % i, 1, con_zlib(d, niv, est), len(d) + rnd.randint(0, 9), (0, None, d, 0), partes_a)
            else:
                c.pon('azar %d (Deflate)' % i, 0, crudo(d, niv, est, rnd.randint(1, 9)), len(d), (0, None, d, None), partes_a)
        c.corre(prog)
        # ---- estropeados: lo que diga, pero sin colgarse ni escribir fuera; si
        #      dice que está bien, tiene que coincidir con lo que diga zlib ----
        for i in range(cuantos * 2):
            d = datos_de(rnd, rnd.choice([1, 2, 4, 5, 6]), rnd.randint(1, 2000))
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
            partes_e = (rnd.choice(PARTES),)
            if acabado and len(bien) <= cabe:
                c.pon('estropeado %d (zlib lo da por bueno)' % i, 0, z, cabe, (0, None, bien, None), partes_e)
            elif acabado:
                c.pon('estropeado %d (no cabe)' % i, 0, z, cabe, (1, 3, bien, None), partes_e)
            else:
                c.pon('estropeado %d' % i, 0, z, cabe, (1, None, None, None), partes_e)
        for i in range(cuantos):                            # y ruido puro
            z = bytes(rnd.getrandbits(8) for _ in range(rnd.randint(0, 300)))
            try:
                do = zlib.decompressobj(-15)
                bien = do.decompress(z, 5000)
                acabado = do.eof
            except zlib.error:
                bien, acabado = None, False
            partes_e = (rnd.choice(PARTES),)
            if acabado:
                c.pon('ruido %d (zlib lo da por bueno)' % i, 0, z, 5000, (0, None, bien, None), partes_e)
            else:
                c.pon('ruido %d' % i, 0, z, 5000, (1, None, None, None), partes_e)
        c.corre(prog)
    print('  %s desinfla_t.inc contra zlib y contra desinfla.inc: %d casos (%d con la entrada o la salida a trozos), %d fallos' % (
        'OK  ' if not fallos else 'FAIL', total, partidos, fallos))
    sys.exit(1 if fallos else 0)


if __name__ == '__main__':
    main()
