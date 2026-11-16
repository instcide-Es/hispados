#!/usr/bin/env python3
"""Prueba del descodificador de JPEG (programas/comun/jpeg.inc) fuera de Hispa Ventanas.

uso: jpeg_prueba.py [--azar N] [--rotos N] [--de-verdad N] [--solo LETRAS] [--guarda DIR]

Toma jpeg.inc TAL CUAL y lo ensambla con NASM dentro de un programa de Linux de
32 bits (como hace lector_desinfla.py con el descompresor) que lee casos por la
entrada (opciones, memoria de trabajo y el JPEG) y devuelve las medidas, los
errores y las filas que salen. Los datos se le ponen pegados a una página de
memoria prohibida, y la memoria de trabajo también (y con un centinela
delante, y otros dos alrededor de sus variables): una lectura o una escritura
fuera de sitio para el programa en seco o rompe un centinela, y se ve.

Las partes (--solo):
  A  JPEG hechos con Pillow (libjpeg-turbo): grises, color con los tres
     submuestreos, CMYK; calidades de 5 a 100; tamaños raros; con y sin
     reinicios, con Huffman optimizado; y progresivos. Punto a punto contra
     libjpeg-turbo ampliando el color sin suavizar (simplejpeg) o contra Pillow.
  B  JPEG hechos aquí mismo (un codificador pequeño que sabe lo que mete):
     todos los factores de muestreo de 1 a 4, tablas de 16 bits, componentes en
     barridos aparte, SOF1, RGB sin girar, YCCK, rellenos, comentarios. Contra
     un modelo en coma flotante hecho con los mismos coeficientes (y contra
     Pillow cuando no suaviza).
  C  las reducciones 1/2, 1/4 y 1/8 contra las de libjpeg-turbo y contra el modelo.
  D  lo que se rechaza, cada cosa con su error; la memoria que pide.
  E  cortados: lo entregado antes del corte es igual que en el entero.
  F  estropeados al azar, a miles: ni un cuelgue ni nada fuera de su sitio.
  G  JPEG de verdad que haya en la máquina (solo se leen).
"""
import argparse, glob, hashlib, io, math, os, random, struct, subprocess, sys, tempfile

import numpy as np
from PIL import Image, ImageFile
try:
    import simplejpeg
except ImportError:                                     # (sin él no hay referencia sin suavizado)
    simplejpeg = None

ImageFile.MAXBLOCK = 1 << 22                            # (si no, Pillow no sabe guardar algunos con ruido y calidad 100)
RAIZ = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
ENT_MAX = 8 * 1024 * 1024
TRAB_MAX = 96 * 1024 * 1024
SAL_MAX = 64 * 1024 * 1024
E_NOJPEG, E_CORTO, E_DATOS, E_MEMORIA, E_PRECISION, E_ARITMETICO, E_SINPERDIDAS, E_JERARQUICO = range(1, 9)
R1, R2, R4, R8, COMP, TR_NO, TR_SI = 0, 1, 2, 3, 4, 8, 16
B_PROG, B_COEF, B_ADOBE, B_JFIF, B_GIRADO = 1, 2, 4, 8, 16

ARNES = r'''
        bits 32
        cpu 386
ENT_MAX equ %d
TRAB_MAX equ %d
SAL_MAX equ %d
MAGIA1  equ 0x11223344
MAGIA2  equ 0x55667788
MAGIA3  equ 0x0BADC0DE
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
; ECX=octetos (múltiplo de 4096) -> EAX=un trozo de memoria con una página prohibida detrás
mapea:
        push ecx
        mov eax, 192                ; mmap2
        xor ebx, ebx
        add ecx, 4096
        mov edx, 3
        mov esi, 0x22
        mov edi, -1
        xor ebp, ebp
        int 0x80
        cmp eax, 0xFFFFF000
        ja fin
        pop ecx
        push eax
        lea ebx, [eax+ecx]
        mov eax, 125                ; mprotect
        mov ecx, 4096
        xor edx, edx
        int 0x80
        test eax, eax
        jnz fin
        pop eax
        ret
_start:
        cld
        mov ecx, ENT_MAX
        call mapea
        mov [ent_base], eax
        mov ecx, TRAB_MAX
        call mapea
        mov [trab_base], eax
        mov edi, cen1
        mov ecx, 64
        mov al, 0xA5
        rep stosb
        mov edi, cen2
        mov ecx, 64
        rep stosb
.caso:
        mov ecx, cab                ; opciones, octetos, memoria de trabajo, modo
        mov edx, 16
        call lee
        mov ecx, [ent_base]
        add ecx, ENT_MAX
        sub ecx, [cab+4]
        mov [dat], ecx
        mov edx, [cab+4]
        call lee
        mov edi, res
        mov ecx, 16
        xor eax, eax
        rep stosd
        mov dword [res+36], 0xFF
        mov dword [res+48], 3
        mov dword [trab], 0
        ; ---- las medidas ----
        mov esi, [dat]
        mov ecx, [cab+4]
        mov eax, [cab]
        mov edi, MAGIA1
        mov ebp, MAGIA2
        call jpeg_medidas
        jnc .m_bien
        mov [res], eax
        cmp esi, [dat]
        jne .m_reg
        cmp edi, MAGIA1
        jne .m_reg
        cmp ebp, MAGIA2
        jne .m_reg
        cmp ebx, ebx
        jmp .hecho
.m_reg:
        and dword [res+48], ~1
        jmp .hecho
.m_bien:
        mov [res+4], eax
        mov [res+8], ebx
        mov [res+12], ecx
        mov [res+16], edx
        cmp esi, [dat]
        jne .m_reg2
        cmp edi, MAGIA1
        jne .m_reg2
        cmp ebp, MAGIA2
        je .m_reg3
.m_reg2:
        and dword [res+48], ~1
.m_reg3:
        mov eax, [jp_pide]
        mov [res+20], eax
        mov eax, [jp_sal_an]
        mov [res+24], eax
        mov eax, [jp_sal_al]
        mov [res+28], eax
        mov eax, [jp_sal_nc]
        mov [res+32], eax
        cmp dword [cab+12], 1       ; modo 1: pedir una fila sin haber abierto
        jne .abre
        call jpeg_fila
        jnc .m_reg
        mov [res+44], eax
        jmp .hecho
.abre:
        ; ---- la memoria de trabajo: la justa (o la que diga el caso), pegada
        ;      a la página prohibida, con un centinela delante ----
        mov eax, [cab+8]
        cmp eax, 0xFFFFFFFF
        jne .mem
        mov eax, [jp_pide]
.mem:
        mov [mem], eax
        mov dword [res+36], 0xFE
        cmp eax, TRAB_MAX - 4096
        ja .hecho
        mov edi, [trab_base]
        add edi, TRAB_MAX
        sub edi, eax
        mov [trab], edi
        mov ecx, eax
        mov al, 0xCC
        rep stosb
        mov edi, [trab]
        sub edi, 64
        mov ecx, 64
        mov al, 0xA5
        rep stosb
        mov esi, [dat]
        mov ecx, [cab+4]
        mov eax, [cab]
        mov edi, [trab]
        mov edx, [mem]
        mov ebx, MAGIA1
        mov ebp, MAGIA2
        call jpeg_abre
        jnc .a_bien
        mov [res+36], eax
        jmp .a_reg
.a_bien:
        mov dword [res+36], 0
        cmp eax, [cab]
        jne .a_mal
.a_reg:
        cmp esi, [dat]
        jne .a_mal
        cmp ecx, [cab+4]
        jne .a_mal
        cmp edi, [trab]
        jne .a_mal
        cmp edx, [mem]
        jne .a_mal
        cmp ebx, MAGIA1
        jne .a_mal
        cmp ebp, MAGIA2
        je .a_reg2
.a_mal:
        and dword [res+48], ~1
.a_reg2:
        cmp dword [res+36], 0
        jne .hecho
        ; ---- las filas ----
        mov eax, [jp_sal_an]
        imul eax, [jp_sal_nc]
        mov [fila_n], eax
        mov dword [pos], 0
.fila:
        mov eax, MAGIA3
        mov ebx, MAGIA1
        mov ecx, MAGIA2
        mov edx, MAGIA3
        mov edi, MAGIA1
        mov ebp, MAGIA2
        mov esi, MAGIA3
        call jpeg_fila
        jc .f_fin
        cmp eax, MAGIA3
        jne .f_mal
        cmp ebx, MAGIA1
        jne .f_mal
        cmp ecx, MAGIA2
        jne .f_mal
        cmp edx, MAGIA3
        jne .f_mal
        cmp edi, MAGIA1
        jne .f_mal
        cmp ebp, MAGIA2
        je .f_reg
.f_mal:
        and dword [res+48], ~1
.f_reg:
        inc dword [res+40]
        mov eax, [res+40]
        cmp eax, [jp_sal_al]
        jbe .f_cabe
        or dword [res+48], 4        ; más filas de las que tiene
        jmp .f_fin2
.f_cabe:
        mov ecx, [fila_n]
        mov eax, [pos]
        add eax, ecx
        cmp eax, SAL_MAX
        ja .f_lee
        mov edi, salida
        add edi, [pos]
        mov [pos], eax
        rep movsb
        jmp .fila
.f_lee:
        rep lodsb                   ; (no cabe en la salida: al menos se lee)
        jmp .fila
.f_fin:
        mov [res+44], eax
        cmp ebx, MAGIA1
        jne .f_mal2
        cmp ecx, MAGIA2
        jne .f_mal2
        cmp edx, MAGIA3
        jne .f_mal2
        cmp edi, MAGIA1
        jne .f_mal2
        cmp ebp, MAGIA2
        jne .f_mal2
        cmp esi, MAGIA3
        je .f_fin2
.f_mal2:
        and dword [res+48], ~1
.f_fin2:
        call jpeg_fila              ; después del final: que siga diciendo que no hay
        jc .hecho
        or dword [res+48], 8
.hecho:
        mov eax, [jp_error]
        mov [res+56], eax
        ; ---- los centinelas ----
        mov esi, cen1
        call mira
        mov esi, cen2
        call mira
        mov esi, [trab]
        test esi, esi
        jz .sin
        sub esi, 64
        call mira
.sin:
        mov eax, [pos]
        cmp dword [res+36], 0
        je .e
        xor eax, eax
.e:
        mov [res+52], eax
        mov ecx, res
        mov edx, 64
        call escribe
        mov ecx, salida
        mov edx, [res+52]
        call escribe
        jmp .caso
mira:
        mov ecx, 64
.m:
        lodsb
        cmp al, 0xA5
        jne .roto
        dec ecx
        jnz .m
        ret
.roto:
        and dword [res+48], ~2
        ret

%%include "jpeg.inc"

        section .bss
cab     resd 4
res     resd 16
ent_base resd 1
trab_base resd 1
dat     resd 1
trab    resd 1
mem     resd 1
fila_n  resd 1
pos     resd 1
cen1    resb 64
        JPEG_BSS
cen2    resb 64
salida  resb SAL_MAX + 64
'''

fallos = 0
total = 0
errores = {}                                            # de cada clase: [mayor, suma, cuántos] por componente


def falla(nombre, que):
    global fallos
    fallos += 1
    if fallos <= 40:
        print('  FAIL %s: %s' % (nombre, que))


def monta(tmp):
    inc = open(os.path.join(RAIZ, 'programas', 'comun', 'jpeg.inc'), encoding='utf-8').read()
    open(os.path.join(tmp, 'jpeg.inc'), 'w', encoding='cp437', errors='replace').write(inc)
    asm = os.path.join(tmp, 'arnes.asm')
    open(asm, 'w').write(ARNES % (ENT_MAX, TRAB_MAX, SAL_MAX))
    subprocess.run([os.environ.get('NASM', 'nasm'), '-f', 'elf32', '-i', tmp + os.sep, '-o', asm + '.o', asm], check=True)
    subprocess.run(['ld', '-m', 'elf_i386', '-o', os.path.join(tmp, 'arnes'), asm + '.o'], check=True)
    return os.path.join(tmp, 'arnes')


class Res:
    pass


def corre(prog, casos, espera=600):
    """casos: [(datos, opciones, memoria (None: la justa), modo)] -> [Res o None (el programa se paró ahí)]"""
    sal = []
    i = 0
    while i < len(casos):
        # por tandas: que lo que salga de cada una quepa bien en la memoria
        j, peso = i, 0
        while j < len(casos) and (j == i or peso < 80e6) and j - i < 4000:
            peso += len(casos[j][0]) * 30 + 100000
            j += 1
        ent = b''.join(struct.pack('<IIII', op, len(d), 0xFFFFFFFF if m is None else m, modo) + d
                       for d, op, m, modo in casos[i:j])
        try:
            out = subprocess.run([prog], input=ent, stdout=subprocess.PIPE, timeout=espera).stdout
        except subprocess.TimeoutExpired as e:
            out = e.stdout or b''
        p = 0
        n = 0
        for _ in range(i, j):
            if p + 64 > len(out):
                break
            r = Res()
            (r.med, r.an, r.al, r.nc, r.band, r.pide, r.san, r.sal, r.snc, r.abre, r.filas, r.final,
             r.mira, r.nsal, r.error, _) = struct.unpack_from('<16I', out, p)
            if p + 64 + r.nsal > len(out):
                break
            r.datos = out[p + 64:p + 64 + r.nsal]
            p += 64 + r.nsal
            sal.append(r)
            n += 1
        if n < j - i:
            sal.append(None)                            # se paró en este: los demás de la tanda, otra vez
            i += n + 1
        else:
            i = j
    return sal


def puntos(r):
    """lo que ha salido, como una matriz filas x ancho x octetos"""
    return np.frombuffer(r.datos, dtype=np.uint8).reshape(r.filas, r.san, r.snc)


def sano(nombre, r):
    """lo que tiene que cumplir cualquier resultado, salga lo que salga"""
    global total
    total += 1
    if r is None:
        falla(nombre, 'el programa se ha parado (una lectura o una escritura fuera de sitio, o un cuelgue)')
        return False
    mal = []
    if not r.mira & 1:
        mal.append('no conserva los registros')
    if not r.mira & 2:
        mal.append('ha roto un centinela')
    if r.mira & 4:
        mal.append('da más filas de las que tiene')
    if r.mira & 8:
        mal.append('después del final da otra fila')
    if r.med > 8 or (r.abre > 8 and r.abre < 0xFE) or r.final > 8:
        mal.append('un error sin clase (%d, %d, %d)' % (r.med, r.abre, r.final))
    if r.med == 0 and r.abre == 0 and r.filas > r.sal:
        mal.append('filas de más')
    if mal:
        falla(nombre, '; '.join(mal))
    return not mal


def apunta(clase, dif):
    """dif: |mío - referencia|, filas x ancho x componentes"""
    e = errores.setdefault(clase, [])
    while len(e) < dif.shape[2]:
        e.append([0, 0, 0])
    for c in range(dif.shape[2]):
        d = dif[:, :, c]
        e[c][0] = max(e[c][0], int(d.max()) if d.size else 0)
        e[c][1] += int(d.sum())
        e[c][2] += d.size


def compara(nombre, r, ref, tol, clase):
    """el resultado entero y sin error, contra la referencia (alto x ancho x nc)"""
    if not sano(nombre, r):
        return
    if r.med or r.abre or r.final:
        return falla(nombre, 'error %d/%d/%d (y [jp_error]=%d)' % (r.med, r.abre, r.final, r.error))
    if ref.ndim == 2:
        ref = ref[:, :, None]
    if (r.filas, r.san, r.snc) != ref.shape:
        return falla(nombre, 'mide %dx%dx%d y se esperaba %dx%dx%d' % (r.san, r.filas, r.snc, ref.shape[1], ref.shape[0], ref.shape[2]))
    dif = np.abs(puntos(r).astype(np.int16) - ref.astype(np.int16))
    apunta(clase, dif)
    m = int(dif.max()) if dif.size else 0
    if m > tol:
        y, x, c = np.unravel_index(int(dif.argmax()), dif.shape)
        falla(nombre, 'difiere en %d (se admite %d) en x=%d y=%d componente %d; media %.3f' % (m, tol, x, y, c, dif.mean()))


# ---------------------------------------------------------------------------
#  Imágenes de prueba
# ---------------------------------------------------------------------------
def dibujo(rnd, an, al, nc, clase):
    """una imagen de an x al con nc componentes: 0 foto suave, 1 ruido, 2 letras (bordes duros), 3 lisa"""
    yy, xx = np.mgrid[0:al, 0:an].astype(np.float64)
    cap = []
    for c in range(nc):
        if clase == 3:
            p = np.full((al, an), rnd.randint(0, 255), dtype=np.float64)
        elif clase == 1:
            p = np.array([rnd.getrandbits(8) for _ in range(an * al)], dtype=np.float64).reshape(al, an)
        elif clase == 2:
            p = np.full((al, an), 255.0)
            for _ in range(max(2, an * al // 60)):
                x, y = rnd.randrange(an), rnd.randrange(al)
                p[y:y + rnd.randint(1, 3), x:x + rnd.randint(1, 9)] = rnd.choice([0, 0, 0, 40, 200])
        else:
            p = 128 + 100 * np.sin(xx / rnd.uniform(3, 40) + rnd.uniform(0, 6)) * np.cos(yy / rnd.uniform(3, 40) + rnd.uniform(0, 6))
            p += (xx - an / 2) * rnd.uniform(-1, 1) + (yy - al / 2) * rnd.uniform(-1, 1)
            x, y = rnd.randrange(an), rnd.randrange(al)
            p[y:y + al // 3 + 1, x:x + an // 3 + 1] += rnd.uniform(-120, 120)
            p += np.array([rnd.gauss(0, 4) for _ in range(an * al)]).reshape(al, an)
        cap.append(np.clip(p, 0, 255).astype(np.uint8))
    return cap[0] if nc == 1 else np.dstack(cap)


def con_pillow(a, **kw):
    modo = {2: 'L', 3: {3: 'RGB', 4: 'CMYK'}.get(a.shape[2] if a.ndim == 3 else 0)}[a.ndim]
    b = io.BytesIO()
    Image.fromarray(a, modo).save(b, 'JPEG', **kw)
    return b.getvalue()


def ref_pillow(d):
    """lo que da Pillow; el CMYK, como viene en el archivo (Pillow le da la vuelta)"""
    im = Image.open(io.BytesIO(d))
    im.load()
    a = np.asarray(im)
    if im.mode == 'CMYK':
        a = 255 - a
    return a


def ref_turbo(d, red=1):
    """libjpeg-turbo sin suavizar al ampliar el color, y con su reducción"""
    al, an, cs, _ = simplejpeg.decode_jpeg_header(d)
    cs = {'Gray': 'GRAY', 'CMYK': 'CMYK', 'YCCK': 'CMYK'}.get(cs, 'RGB')
    kw = {}
    if red > 1:
        kw = dict(min_width=-(-an // red), min_height=-(-al // red), min_factor=red)
    a = simplejpeg.decode_jpeg(d, cs, fastupsample=True, **kw)
    assert a.shape[0] == -(-al // red) and a.shape[1] == -(-an // red), 'la referencia no reduce a 1/%d' % red
    return a


def a_rgb(cmyk, adobe=True):
    """de los cuatro componentes como vienen a rojo, verde y azul, con la cuenta de jpeg.inc"""
    c = cmyk.astype(np.int32)
    if not adobe:
        c = 255 - c
    t = c[:, :, :3] * c[:, :, 3:4] + 128
    return ((t + (t >> 8)) >> 8).astype(np.uint8)


# ---------------------------------------------------------------------------
#  Un codificador pequeño, que sabe lo que mete, y su modelo
# ---------------------------------------------------------------------------
ZZ = [0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5, 12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7, 14, 21, 28,
      35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63]
M8 = np.array([[(math.sqrt(0.5) if u == 0 else 1.0) / 2 * math.cos((2 * x + 1) * u * math.pi / 16) for u in range(8)] for x in range(8)])


def tablas_de_siempre():
    """las tablas de Huffman de siempre (anexo K de la norma), sacadas de un JPEG de Pillow sin optimizar"""
    b = io.BytesIO()
    Image.new('RGB', (8, 8)).save(b, 'JPEG')
    d = b.getvalue()
    t = {}
    p = 2
    while d[p + 1] != 0xDA:
        n = struct.unpack_from('>H', d, p + 2)[0]
        if d[p + 1] == 0xC4:
            q = p + 4
            while q < p + 2 + n:
                cuentas = list(d[q + 1:q + 17])
                t[d[q]] = (cuentas, bytes(d[q + 17:q + 17 + sum(cuentas)]))
                q += 17 + sum(cuentas)
        p += 2 + n
    return [t[0], t[1]], [t[0x10], t[0x11]]


H_DC, H_AC = tablas_de_siempre()


def codigos(tabla):
    cuentas, vals = tabla
    c, k, d = 0, 0, {}
    for largo in range(1, 17):
        for _ in range(cuentas[largo - 1]):
            d[vals[k]] = (c, largo)
            c += 1
            k += 1
        c <<= 1
    return d


class Bits:
    def __init__(self):
        self.b = bytearray()
        self.acc = 0
        self.n = 0

    def pon(self, v, n):
        self.acc = (self.acc << n) | (v & ((1 << n) - 1))
        self.n += n
        while self.n >= 8:
            o = (self.acc >> (self.n - 8)) & 0xFF
            self.b.append(o)
            if o == 0xFF:
                self.b.append(0)
            self.n -= 8
        self.acc &= (1 << self.n) - 1

    def cierra(self):
        if self.n:
            self.pon(0x7F, 8 - self.n)                  # se rellena con unos


def seg(marca, cuerpo):
    return bytes([0xFF, marca]) + struct.pack('>H', len(cuerpo) + 2) + bytes(cuerpo)


def codifica(rnd, an, al, fact, clase=0, calidad=60, q16=False, ri=0, aparte=False, sof=0xC0, marca='jfif',
             ids=None, tr_adobe=None, relleno=False):
    """-> (el JPEG, lo que hace falta para el modelo). fact: [(h, v)] de cada componente.
    Cada componente se inventa ya a su tamaño (no hace falta una imagen de la que salga)."""
    nc = len(fact)
    hmax, vmax = max(h for h, v in fact), max(v for h, v in fact)
    mcux, mcuy = -(-an // (8 * hmax)), -(-al // (8 * vmax))
    # cuantización: algo que crezca con la frecuencia; con q16, valores de más de 255
    qs = []
    for t in range(2):
        q = [max(1, min(60000 if q16 else 255, int((1 + (ZZ[k] // 8) + (ZZ[k] % 8)) * (100 - calidad) / 8 * (t + 1) * (40 if q16 and k > 40 else 1)) + 1)) for k in range(64)]
        qs.append(q)
    comp = []
    for i, (h, v) in enumerate(fact):
        bw, bh = mcux * h, mcuy * v
        cw, ch = -(-an * h // hmax), -(-al * v // vmax)
        p = dibujo(rnd, bw * 8, bh * 8, 1, clase).astype(np.float64) - 128
        bl = p.reshape(bh, 8, bw, 8).transpose(0, 2, 1, 3)              # bh x bw x 8 x 8
        co = M8.T @ bl @ M8                                             # la transformada directa
        q = np.array(qs[min(i, 1) if nc != 4 else 0], dtype=np.float64)
        qn = np.zeros(64)
        qn[ZZ] = q
        cq = np.rint(co / qn.reshape(8, 8)).astype(np.int32)
        cq = np.clip(cq, -1023, 1023)
        comp.append(dict(h=h, v=v, bw=bw, bh=bh, bwr=-(-cw // 8), bhr=-(-ch // 8), c=cq, q=qn.reshape(8, 8), tq=(min(i, 1) if nc != 4 else 0),
                         td=min(i, 1) if nc != 4 else 0))
    if ids is None:
        ids = [1, 2, 3, 4][:nc]
    d = bytearray(b'\xff\xd8')
    if marca == 'jfif':
        d += seg(0xE0, b'JFIF\0\1\1\0\0\1\0\1\0\0')
    elif marca == 'adobe':
        d += seg(0xEE, b'Adobe\0\x64\0\0\0\0' + bytes([tr_adobe]))
    if relleno:
        d += seg(0xFE, b'un comentario, que se salta') + b'\xff\xff\xff'
        d += seg(0xE1, b'Exif\0\0' + bytes(rnd.getrandbits(8) for _ in range(50)))
    usadas = sorted(set(c['tq'] for c in comp))
    for t in usadas:
        if q16:
            d += seg(0xDB, bytes([0x10 | t]) + b''.join(struct.pack('>H', x) for x in qs[t]))
        else:
            d += seg(0xDB, bytes([t]) + bytes(qs[t]))
    d += seg(sof, struct.pack('>BHHB', 8, al, an, nc) + b''.join(bytes([ids[i], c['h'] << 4 | c['v'], c['tq']]) for i, c in enumerate(comp)))
    for t in sorted(set(c['td'] for c in comp)):
        d += seg(0xC4, bytes([t]) + bytes(H_DC[t][0]) + bytes(H_DC[t][1]))
        d += seg(0xC4, bytes([0x10 | t]) + bytes(H_AC[t][0]) + bytes(H_AC[t][1]))
    if ri:
        d += seg(0xDD, struct.pack('>H', ri))
    cdc = [codigos(t) for t in H_DC]
    cac = [codigos(t) for t in H_AC]

    def bloque(b, c, by, bx, pred):
        z = c['c'][by, bx].reshape(64)[ZZ]
        dif = int(z[0]) - pred
        s = abs(dif).bit_length()
        b.pon(*cdc[c['td']][s])
        if s:
            b.pon(dif if dif > 0 else dif + (1 << s) - 1, s)
        r = 0
        ult = max([k for k in range(1, 64) if z[k]], default=0)
        for k in range(1, ult + 1):
            v = int(z[k])
            if v == 0:
                r += 1
                continue
            while r > 15:
                b.pon(*cac[c['td']][0xF0])
                r -= 16
            s = abs(v).bit_length()
            b.pon(*cac[c['td']][r << 4 | s])
            b.pon(v if v > 0 else v + (1 << s) - 1, s)
            r = 0
        if ult < 63:
            b.pon(*cac[c['td']][0])
        return int(z[0])

    def barrido(cuales):
        nonlocal d
        d += seg(0xDA, bytes([len(cuales)]) + b''.join(bytes([ids[i], comp[i]['td'] << 4 | comp[i]['td']]) for i in cuales) + b'\0\x3f\0')
        b = Bits()
        pred = [0] * nc
        if len(cuales) == 1:
            c = comp[cuales[0]]
            unidades = [[(cuales[0], by, bx)] for by in range(c['bhr']) for bx in range(c['bwr'])]
        else:
            unidades = [[(i, my * comp[i]['v'] + v, mx * comp[i]['h'] + h) for i in cuales for v in range(comp[i]['v']) for h in range(comp[i]['h'])]
                        for my in range(mcuy) for mx in range(mcux)]
        for n, u in enumerate(unidades):
            if ri and n and n % ri == 0:
                b.cierra()
                if relleno:
                    b.b += b'\xff'
                b.b += bytes([0xFF, 0xD0 + (n // ri - 1) % 8])
                pred = [0] * nc
            for i, by, bx in u:
                pred[i] = bloque(b, comp[i], by, bx, pred[i])
        b.cierra()
        d += b.b

    if aparte:
        orden = list(range(nc))
        if aparte == 2 and nc >= 3:                     # dos juntos y los demás aparte
            barrido([0, 1])
            orden = orden[2:]
        for i in orden:
            barrido([i])
    else:
        barrido(list(range(nc)))
    d += b'\xff\xd9'
    return bytes(d), dict(an=an, al=al, comp=comp, hmax=hmax, vmax=vmax, aparte=aparte)


def ycc_rgb(y, cb, cr):
    y, cb, cr = y.astype(np.int32), cb.astype(np.int32) - 128, cr.astype(np.int32) - 128
    r = y + ((91881 * cr + 32768) >> 16)
    g = y + ((-22554 * cb - 46802 * cr + 32768) >> 16)
    b = y + ((116130 * cb + 32768) >> 16)
    return [np.clip(v, 0, 255) for v in (r, g, b)]


def modelo(m, red=1, girado=True, comp_mode=False, adobe=False):
    """lo que tiene que dar jpeg.inc, hecho en coma flotante con los coeficientes que se metieron"""
    an, al, hmax, vmax = m['an'], m['al'], m['hmax'], m['vmax']
    n = 8 // red
    san, sal = -(-an // red), -(-al // red)
    planos = []
    yy, xx = np.mgrid[0:sal, 0:san]
    for i, c in enumerate(m['comp']):
        ni = n
        while ni < 8 and (hmax * n) % (c['h'] * ni * 2) == 0 and (vmax * n) % (c['v'] * ni * 2) == 0:
            ni *= 2
        f = 8 // ni
        mat = M8.reshape(ni, f, 8).mean(axis=1)
        co = c['c'] * c['q']
        if m['aparte'] and len(m['comp']) > 1 and ni < 8:
            # con los coeficientes guardados y reducción, solo se guardan los de
            # las ni primeras filas y columnas
            co = co.copy()
            co[:, :, ni:, :] = 0
            co[:, :, :, ni:] = 0
        bl = mat @ co @ mat.T                                           # bh x bw x ni x ni
        # los bloques que no entran en un barrido de un solo componente se quedan a cero (gris)
        if m['aparte'] and len(m['comp']) > 1 and not (m['aparte'] == 2 and i < 2):
            bl[c['bhr']:, :] = 0
            bl[:, c['bwr']:] = 0
        p = np.clip(np.floor(bl + 128.5), 0, 255).astype(np.uint8)
        p = p.transpose(0, 2, 1, 3).reshape(c['bh'] * ni, c['bw'] * ni)
        planos.append(p[yy * (c['v'] * ni) // (vmax * n), xx * (c['h'] * ni) // (hmax * n)])
    nc = len(planos)
    if nc == 1:
        return planos[0]
    if girado:
        rgb = ycc_rgb(*planos[:3])
        if nc == 4:
            rgb = [255 - v for v in rgb]
        planos = [v.astype(np.uint8) for v in rgb] + planos[3:]
    a = np.dstack(planos)
    if nc == 4 and not comp_mode:
        a = a_rgb(a, adobe)
    return a


# ---------------------------------------------------------------------------
TAMANOS = [(1, 1), (1, 2), (2, 1), (3, 5), (7, 7), (8, 8), (9, 9), (15, 17), (16, 16), (17, 15), (16, 8), (8, 16), (31, 33), (33, 31),
           (32, 32), (48, 16), (63, 65), (100, 75), (127, 129), (1, 300), (300, 1), (2, 1000), (1000, 2), (2050, 3), (3, 2050),
           (257, 255), (640, 480), (65, 7), (7, 65), (1300, 17), (24, 1100)]


def parte_a(prog, rnd, cuantos):
    print('== A: JPEG de Pillow (libjpeg-turbo), punto a punto ==')
    casos, que = [], []

    def pon(nombre, d, op, ref, tol, clase):
        casos.append((d, op, None, 0))
        que.append((nombre, ref, tol, clase))

    n = 0
    for i in range(cuantos):
        an, al = TAMANOS[i % len(TAMANOS)] if i < len(TAMANOS) * 3 else (rnd.randint(1, 400), rnd.randint(1, 400))
        cal = [5, 10, 25, 50, 75, 90, 95, 100][i % 8] if i % 3 else rnd.randint(5, 100)
        clase = rnd.choice([0, 0, 0, 1, 2, 3])
        modo = [1, 3, 3, 3, 4, 3, 1, 3][i % 8]
        kw = dict(quality=cal)
        if rnd.random() < 0.3:
            kw['optimize'] = True
        if rnd.random() < 0.3:
            if rnd.random() < 0.5:
                kw['restart_marker_blocks'] = rnd.choice([1, 2, 3, 7, 50])
            else:
                kw['restart_marker_rows'] = rnd.choice([1, 2])
        prog_ = i % 4 == 3
        if prog_:
            kw['progressive'] = True
        if modo == 3:
            kw['subsampling'] = [0, 1, 2][(i // 8) % 3]
            if kw['subsampling'] == 0 and rnd.random() < 0.2:
                kw['keep_rgb'] = True
        a = dibujo(rnd, an, al, modo, clase)
        d = con_pillow(a, **kw)
        nombre = 'A%d %dx%d %s calidad %d %s' % (i, an, al, {1: 'gris', 3: 'color', 4: 'CMYK'}[modo], cal,
                                                 ' '.join('%s=%s' % (k, v) for k, v in kw.items() if k != 'quality'))
        cl = ('progresivo ' if prog_ else '') + {1: 'gris', 3: 'color 4:4:4', 4: 'CMYK'}[modo]
        if modo == 3 and kw['subsampling']:
            cl = ('progresivo ' if prog_ else '') + 'color ' + ['', '4:2:2', '4:2:0'][kw['subsampling']]
        if modo == 4:
            ref = ref_pillow(d)
            pon(nombre + ' (componentes)', d, COMP, ref, 2, cl)
            pon(nombre + ' (a RGB)', d, 0, a_rgb(ref), 3, cl + ' a RGB')
        elif modo == 1:
            pon(nombre, d, 0, ref_pillow(d), 2, cl)
        else:
            ref = ref_turbo(d)
            if kw['subsampling'] == 0:
                assert np.array_equal(ref, ref_pillow(d))
            pon(nombre, d, 0, ref, 4, cl)
            if rnd.random() < 0.15:
                pon(nombre + ' (JPEG_COMP)', d, COMP, ref, 4, cl)
    res = corre(prog, casos)
    for (nombre, ref, tol, clase), r in zip(que, res + [None] * len(que)):
        compara(nombre, r, ref, tol, clase)
    # lo que dicen las medidas
    a = dibujo(rnd, 50, 30, 3, 0)
    for nombre, d, band in (('JFIF en color', con_pillow(a), B_JFIF | B_GIRADO),
                            ('progresivo', con_pillow(a, progressive=True), B_JFIF | B_GIRADO | B_PROG | B_COEF),
                            ('gris', con_pillow(a[:, :, 0]), B_JFIF),
                            ('RGB sin girar', con_pillow(a, keep_rgb=True, subsampling=0), B_ADOBE),
                            ('CMYK', con_pillow(dibujo(rnd, 50, 30, 4, 0)), B_ADOBE)):
        r = corre(prog, [(d, 0, None, 0)])[0]
        if sano('medidas de ' + nombre, r):
            esp_nc = {'gris': 1, 'CMYK': 4}.get(nombre, 3)
            if (r.an, r.al, r.nc, r.band) != (50, 30, esp_nc, band):
                falla('medidas de ' + nombre, 'dice %dx%d, %d componentes, banderas %d (se esperaba %d)' % (r.an, r.al, r.nc, r.band, band))


def parte_b(prog, rnd, cuantos):
    print('== B: JPEG hechos aquí, contra el modelo (y contra Pillow donde no suaviza) ==')
    casos, que = [], []

    def pon(nombre, d, op, ref, tol, clase):
        casos.append((d, op, None, 0))
        que.append((nombre, ref, tol, clase))

    def pillow_vale(fact):
        # libjpeg suaviza los componentes que van justo a la mitad (a lo ancho, a lo alto o las dos)
        hmax, vmax = max(h for h, v in fact), max(v for h, v in fact)
        if sum(h * v for h, v in fact) > 10:
            return False                                # (más de 10 bloques por MCU: la norma no deja, y libjpeg tampoco)
        for h, v in fact:
            if hmax % h or vmax % v:
                return False                            # (proporción no entera: libjpeg no sabe)
            if (hmax // h, vmax // v) in ((2, 1), (1, 2), (2, 2)):
                return False
        return True

    lista = []
    for h in range(1, 5):                               # todos los factores del primero, con los otros a 1
        for v in range(1, 5):
            lista.append([(h, v), (1, 1), (1, 1)])
    lista += [[(2, 2), (2, 1), (1, 2)], [(1, 1), (2, 2), (1, 1)], [(4, 2), (2, 2), (1, 1)], [(2, 2), (2, 2), (2, 2)], [(3, 3), (1, 1), (3, 1)],
              [(4, 4), (2, 2), (4, 4)], [(3, 2), (2, 1), (1, 2)], [(4, 1), (3, 1), (1, 1)], [(2, 4), (1, 3), (1, 1)],
              [(1, 1)], [(2, 2)], [(4, 3)],
              [(1, 1), (1, 1), (1, 1), (1, 1)], [(2, 2), (1, 1), (1, 1), (2, 2)], [(2, 1), (1, 1), (1, 1), (2, 1)], [(4, 4), (1, 1), (2, 2), (4, 4)]]
    for i in range(cuantos):
        fact = lista[i % len(lista)]
        nc = len(fact)
        an, al = (rnd.randint(1, 150), rnd.randint(1, 150)) if i % 5 else TAMANOS[rnd.randrange(len(TAMANOS) - 12)]
        kw = dict(clase=rnd.choice([0, 0, 1, 2, 3]), calidad=rnd.choice([10, 40, 60, 85, 98]))
        if i >= len(lista):
            kw['q16'] = rnd.random() < 0.25
            kw['ri'] = rnd.choice([0, 0, 1, 2, 5, 9])
            kw['aparte'] = rnd.choice([0, 0, 1, 2])
            kw['sof'] = rnd.choice([0xC0, 0xC0, 0xC1])
            kw['relleno'] = rnd.random() < 0.3
        girado, adobe, op = nc >= 3, False, 0
        if nc == 3:
            v = rnd.random()
            if v < 0.15:
                kw.update(marca='adobe', tr_adobe=0)
                girado, adobe = False, True
            elif v < 0.3:
                kw.update(marca='adobe', tr_adobe=1)
                adobe = True
            elif v < 0.4:
                kw.update(marca='nada', ids=[82, 71, 66])
                girado = False
            elif v < 0.5:
                kw.update(marca='nada', ids=[7, 77, 177])
            elif v < 0.6:
                kw.update(marca='nada', ids=[82, 71, 66])
                op = TR_SI                              # se llaman R, G y B, pero quien llama dice que van girados
            elif v < 0.7:
                op, girado = TR_NO, False
        elif nc == 4:
            v = rnd.random()
            if v < 0.4:
                kw.update(marca='adobe', tr_adobe=2)
                adobe = True
            elif v < 0.7:
                kw.update(marca='adobe', tr_adobe=0)
                girado, adobe = False, True
            elif v < 0.85:
                kw.update(marca='nada')
                girado = False
            else:
                kw.update(marca='nada')
                op = TR_SI
        d, m = codifica(rnd, an, al, fact, **kw)
        nombre = 'B%d %dx%d factores %s %s' % (i, an, al, 'x'.join('%d%d' % f for f in fact), ' '.join('%s=%s' % kv for kv in kw.items()))
        cl = 'propios, %d componente%s' % (nc, 's' if nc > 1 else '')
        pon(nombre, d, op, modelo(m, 1, girado, False, adobe), 4 if nc > 1 else 1, cl)
        if nc == 4:
            pon(nombre + ' (componentes)', d, op | COMP, modelo(m, 1, girado, True, adobe), 4, cl)
        red = rnd.choice([2, 4, 8])
        pon(nombre + ' a 1/%d' % red, d, op | {2: R2, 4: R4, 8: R8}[red], modelo(m, red, girado, False, adobe), 4 if nc > 1 else 1,
            'propios reducidos, %d componente%s' % (nc, 's' if nc > 1 else ''))
        # y contra Pillow, que no sabe nada del modelo
        if not op and pillow_vale(fact) and not kw.get('ids'):
            try:
                ref = ref_pillow(d)
            except Exception as e:
                falla(nombre, 'Pillow no lo abre: %s' % e)
                continue
            if nc == 4:
                pon(nombre + ' (contra Pillow)', d, COMP, ref, 4, cl + ', contra Pillow')
            else:
                pon(nombre + ' (contra Pillow)', d, 0, ref, 4 if nc > 1 else 2, cl + ', contra Pillow')
    res = corre(prog, casos)
    for (nombre, ref, tol, clase), r in zip(que, res + [None] * len(que)):
        compara(nombre, r, ref, tol, clase)


def parte_c(prog, rnd, cuantos):
    print('== C: las reducciones a 1/2, 1/4 y 1/8, contra las de libjpeg-turbo ==')
    casos, que = [], []
    for i in range(cuantos):
        an, al = TAMANOS[i % len(TAMANOS)] if i < len(TAMANOS) else (rnd.randint(1, 500), rnd.randint(1, 500))
        modo = [1, 3, 3, 3, 4][i % 5]
        kw = dict(quality=rnd.choice([10, 30, 60, 85, 95, 100]))
        if modo == 3:
            kw['subsampling'] = (i // 5) % 3
        if rnd.random() < 0.3:
            kw['restart_marker_blocks'] = rnd.choice([1, 3, 11])
        if i % 4 == 2:
            kw['progressive'] = True
        # (los progresivos, con imágenes suaves: reducidos solo guardan las frecuencias
        # bajas, y con ruido o letras no se puede pedir que se parezcan a la media exacta)
        d = con_pillow(dibujo(rnd, an, al, modo, 0 if kw.get('progressive') else rnd.choice([0, 0, 1, 2])), **kw)
        for red in (2, 4, 8):
            nombre = 'C%d %dx%d a 1/%d %s %s' % (i, an, al, red, {1: 'gris', 3: 'color', 4: 'CMYK'}[modo], ' '.join('%s=%s' % kv for kv in kw.items()))
            if -(-an // red) == -(-an * 2 // red) and -(-al // red) == -(-al * 2 // red):
                continue                                # (mide lo mismo que con menos reducción: la referencia se queda con esa)
            ref = ref_turbo(d, red)
            cl = 'a 1/%d, %s' % (red, {1: 'gris', 3: 'color', 4: 'CMYK'}[modo])
            # un progresivo reducido solo guarda las frecuencias bajas: no es la media exacta
            if kw.get('progressive'):
                if red == 8 and kw.get('subsampling', 0) == 0:
                    tol, cl = 4 if modo > 1 else 2, cl + ' (progresivo, solo el DC)'
                else:
                    tol, cl = 255, cl + ' (progresivo: solo frecuencias bajas)'
            else:
                tol = 2 if modo == 1 else 4
            casos.append((d, {2: R2, 4: R4, 8: R8}[red] | (COMP if modo == 4 else 0), None, 0))
            que.append((nombre, ref, tol, cl))
    res = corre(prog, casos)
    for (nombre, ref, tol, clase), r in zip(que, res + [None] * len(que)):
        compara(nombre, r, ref, tol, clase)
    # el progresivo reducido: que al menos se parezca (media pequeña)
    for clase, e in errores.items():
        if 'solo frecuencias bajas' in clase:
            for c in e:
                if c[2] and c[1] / c[2] > 4:
                    falla(clase, 'la media del error es %.2f' % (c[1] / c[2]))


def cambia(d, viejo, nuevo, cuantas=1):
    assert d.count(viejo) >= 1, viejo
    return d.replace(viejo, nuevo, cuantas)


def parte_d(prog, rnd):
    print('== D: lo que se rechaza, y la memoria ==')
    a = dibujo(rnd, 40, 24, 3, 0)
    base = con_pillow(a, quality=80)
    gris = con_pillow(a[:, :, 0], quality=80)
    sof = base.index(b'\xff\xc0')
    casos = [
        ('nada', b'', 0, None, (E_NOJPEG, None, None)),
        ('un octeto', b'\xff', 0, None, (E_NOJPEG, None, None)),
        ('no es un JPEG', b'%PDF-1.4 esto no es', 0, None, (E_NOJPEG, None, None)),
        ('un PNG', b'\x89PNG\r\n\x1a\n' + bytes(40), 0, None, (E_NOJPEG, None, None)),
        ('solo el principio', b'\xff\xd8', 0, None, (E_CORTO, None, None)),
        ('principio y final, sin imagen', b'\xff\xd8\xff\xd9', 0, None, (E_DATOS, None, None)),
        ('aritmético (SOF9)', base[:sof] + b'\xff\xc9' + base[sof + 2:], 0, None, (E_ARITMETICO, None, None)),
        ('aritmético progresivo (SOF10)', base[:sof] + b'\xff\xca' + base[sof + 2:], 0, None, (E_ARITMETICO, None, None)),
        ('sin pérdidas (SOF3)', base[:sof] + b'\xff\xc3' + base[sof + 2:], 0, None, (E_SINPERDIDAS, None, None)),
        ('sin pérdidas aritmético (SOF11)', base[:sof] + b'\xff\xcb' + base[sof + 2:], 0, None, (E_ARITMETICO, None, None)),
        ('jerárquico (SOF5)', base[:sof] + b'\xff\xc5' + base[sof + 2:], 0, None, (E_JERARQUICO, None, None)),
        ('jerárquico (SOF7)', base[:sof] + b'\xff\xc7' + base[sof + 2:], 0, None, (E_JERARQUICO, None, None)),
        ('jerárquico (SOF13)', base[:sof] + b'\xff\xcd' + base[sof + 2:], 0, None, (E_JERARQUICO, None, None)),
        ('jerárquico (DHP)', base[:sof] + b'\xff\xde' + base[sof + 2:], 0, None, (E_JERARQUICO, None, None)),
        ('12 bits', base[:sof + 4] + b'\x0c' + base[sof + 5:], 0, None, (E_PRECISION, None, None)),
        ('16 bits', base[:sof + 4] + b'\x10' + base[sof + 5:], 0, None, (E_PRECISION, None, None)),
        ('9 bits', base[:sof + 4] + b'\x09' + base[sof + 5:], 0, None, (E_DATOS, None, None)),
        ('alto 0', base[:sof + 5] + b'\0\0' + base[sof + 7:], 0, None, (E_DATOS, None, None)),
        ('ancho 0', base[:sof + 7] + b'\0\0' + base[sof + 9:], 0, None, (E_DATOS, None, None)),
        ('dos componentes', base[:sof + 9] + b'\x02' + base[sof + 10:], 0, None, (E_DATOS, None, None)),
        ('factor de muestreo 0', base[:sof + 11] + b'\x01' + base[sof + 12:], 0, None, (E_DATOS, None, None)),
        ('factor de muestreo 5', base[:sof + 11] + b'\x51' + base[sof + 12:], 0, None, (E_DATOS, None, None)),
        ('dos SOF', base[:sof] + base[sof:sof + 19] + base[sof:], 0, None, (E_DATOS, None, None)),
        ('barrido sin SOF', base[:sof] + base[sof + 19:], 0, None, (E_DATOS, None, None)),
        ('sin tablas de Huffman', base[:base.index(b'\xff\xc4')] + base[base.index(b'\xff\xda'):], 0, None, (0, E_DATOS, None)),
        ('sin tablas de cuantización', base[:base.index(b'\xff\xdb')] + base[sof:], 0, None, (0, E_DATOS, None)),
        ('segmento con largo 1', base[:sof + 2] + b'\0\1' + base[sof + 4:], 0, None, (E_DATOS, None, None)),
        ('segmento más largo que el archivo', base[:sof + 2] + b'\xff\xff' + base[sof + 4:], 0, None, (E_CORTO, None, None)),
        ('memoria: un octeto menos', base, 0, -1, (0, E_MEMORIA, None)),
        ('memoria: nada', base, 0, 0, (0, E_MEMORIA, None)),
        ('memoria: de sobra', base, 0, 500000, (0, 0, 0)),
        ('memoria: un octeto menos (gris)', gris, 0, -1, (0, E_MEMORIA, None)),
        ('memoria: un octeto menos (progresivo)', con_pillow(a, progressive=True), R2, -1, (0, E_MEMORIA, None)),
    ]
    # la que piden: sin sorpresas (una tira, y en los progresivos los coeficientes)
    pedidos = corre(prog, [(d, op, 0, 0) for _, d, op, _, _ in casos])
    lanza = []
    for (nombre, d, op, mem, esp), r0 in zip(casos, pedidos):
        if mem == -1:
            mem = r0.pide - 1
        lanza.append((d, op, mem, 0))
    for (nombre, d, op, mem, (med, abre, final)), r in zip(casos, corre(prog, lanza)):
        if not sano(nombre, r):
            continue
        if r.med != med or (abre is not None and r.abre != abre) or (final is not None and r.final != final):
            falla(nombre, 'da %d/%d/%d y se esperaba %s/%s/%s' % (r.med, r.abre, r.final, med, abre, final))
    # jpeg_fila sin nada abierto
    r = corre(prog, [(base, 0, None, 1)])[0]
    if sano('una fila sin abrir', r) and r.final != E_NOJPEG:
        falla('una fila sin abrir', 'da %d' % r.final)
    # la memoria de trabajo que se pide, para que conste
    grande = con_pillow(dibujo(rnd, 1240, 1754, 3, 3), quality=50, subsampling=2)
    grande_g = con_pillow(dibujo(rnd, 1240, 1754, 1, 3), quality=50)
    grande_p = con_pillow(dibujo(rnd, 1240, 1754, 3, 3), quality=50, subsampling=2, progressive=True)
    for nombre, d in (('A4 a 150 ppp, color 4:2:0', grande), ('A4 a 150 ppp, gris', grande_g), ('A4 a 150 ppp, color 4:2:0 progresivo', grande_p)):
        rs = corre(prog, [(d, op, 0, 0) for op in (R1, R2, R4, R8)])
        print('     memoria de trabajo, %s: %s' % (nombre, ', '.join('1/%d: %d' % (1 << i, r.pide) for i, r in enumerate(rs))))
        global total
        total += 1
        if not all(r and r.abre == E_MEMORIA for r in rs):
            falla(nombre, 'con la memoria a cero tenía que pedirla')


def parte_e(prog, rnd):
    print('== E: cortados: lo de antes del corte, igual que en el entero ==')
    global total
    for nombre, d, tira in (('gris', con_pillow(dibujo(rnd, 90, 120, 1, 0), quality=70), 8),
                            ('color 4:2:0', con_pillow(dibujo(rnd, 90, 120, 3, 0), quality=70, subsampling=2), 16),
                            ('color 4:4:4 con reinicios', con_pillow(dibujo(rnd, 90, 120, 3, 0), quality=70, subsampling=0, restart_marker_blocks=5), 8),
                            ('CMYK', con_pillow(dibujo(rnd, 60, 80, 4, 0), quality=70), 8),
                            ('progresivo', con_pillow(dibujo(rnd, 90, 120, 3, 0), quality=70, progressive=True), 0)):
        entero = corre(prog, [(d, 0, None, 0)])[0]
        if not sano(nombre + ' entero', entero) or entero.final or entero.filas != entero.sal:
            falla(nombre + ' entero', 'no sale entero')
            continue
        e = puntos(entero)
        cortes = list(range(0, len(d)))
        res = corre(prog, [(d[:c], 0, None, 0) for c in cortes])
        for c, r in zip(cortes, res + [None] * len(cortes)):
            n = '%s cortado en %d de %d' % (nombre, c, len(d))
            if not sano(n, r):
                continue
            if r.med or r.abre:
                if r.med not in (0, E_NOJPEG, E_CORTO) or r.abre not in (0xFF, 0, E_CORTO, E_DATOS):
                    falla(n, 'error %d/%d' % (r.med, r.abre))
                continue
            p = puntos(r)
            if tira:
                # tira a tira: todo menos la última tira entregada tiene que ser idéntico; y
                # si no dice error, entero e idéntico
                if r.final == 0:
                    if r.filas != entero.filas or not np.array_equal(p, e):
                        falla(n, 'dice que está bien y no es igual que el entero')
                    continue
                if r.final not in (E_CORTO, E_DATOS):
                    falla(n, 'error %d' % r.final)
                buenas = max(0, (r.filas - 1) // tira * tira) if r.filas else 0
                if not np.array_equal(p[:buenas], e[:buenas]):
                    falla(n, 'las %d filas de antes de la tira del corte no son las del entero' % buenas)
                if c < len(d) - 2 - 3 * tira and r.filas == entero.filas and np.array_equal(p, e) and c < len(d) // 2:
                    falla(n, 'sale entero con la mitad de los datos')
            else:
                # progresivo: se entrega entero (con menos detalle) y se dice el error
                if r.filas != entero.sal:
                    falla(n, 'entrega %d filas de %d' % (r.filas, entero.sal))
                if r.final not in (E_CORTO, E_DATOS):
                    falla(n, 'no dice que está cortado (%d)' % r.final)
        # el progresivo cortado a la mitad se tiene que parecer al entero (ya tiene los DC)
        if not tira:
            r = corre(prog, [(d[:len(d) * 2 // 3], 0, None, 0)])[0]
            total += 1
            dif = np.abs(puntos(r).astype(int) - e).mean()
            if dif > 12:
                falla(nombre + ' a dos tercios', 'no se parece al entero (media %.1f)' % dif)


def estropea(rnd, d):
    b = bytearray(d)
    if len(b) < 2:
        return bytes(b) + b'\xff'
    v = rnd.random()
    if v < 0.45:                                        # unos bits cambiados
        for _ in range(rnd.randint(1, 4)):
            b[rnd.randrange(len(b))] ^= 1 << rnd.randrange(8)
    elif v < 0.6:                                       # unos octetos cualesquiera
        for _ in range(rnd.randint(1, 6)):
            b[rnd.randrange(len(b))] = rnd.choice([0, 0xFF, rnd.getrandbits(8), 0xD9, 0xDA, 0xC4, 0xD0])
    elif v < 0.7:                                       # un trozo quitado
        i = rnd.randrange(len(b))
        del b[i:i + rnd.randint(1, 40)]
    elif v < 0.8:                                       # un trozo metido
        i = rnd.randrange(len(b))
        b[i:i] = bytes(rnd.getrandbits(8) for _ in range(rnd.randint(1, 30)))
    elif v < 0.9:                                       # un trozo repetido en otro sitio
        i, j = rnd.randrange(len(b)), rnd.randrange(len(b))
        b[j:j] = b[i:i + rnd.randint(1, 60)]
    else:                                               # cabeceras: un octeto de los 300 primeros
        for _ in range(rnd.randint(1, 3)):
            b[rnd.randrange(min(len(b), 300))] = rnd.getrandbits(8)
    if rnd.random() < 0.15:
        del b[rnd.randrange(len(b)):]
    return bytes(b)


def parte_f(prog, rnd, cuantos):
    print('== F: estropeados al azar ==')
    global total
    semillas = []
    for i in range(40):
        an, al = rnd.randint(1, 70), rnd.randint(1, 70)
        modo = [1, 3, 3, 4, 3][i % 5]
        kw = dict(quality=rnd.choice([20, 60, 90]))
        if modo == 3:
            kw['subsampling'] = rnd.randrange(3)
        if i % 3 == 0:
            kw['progressive'] = True
        if i % 4 == 0:
            kw['restart_marker_blocks'] = rnd.choice([1, 2, 5])
        semillas.append(con_pillow(dibujo(rnd, an, al, modo, rnd.choice([0, 1, 2])), **kw))
    for i in range(20):
        fact = rnd.choice([[(2, 2), (1, 1), (1, 1)], [(4, 1), (1, 1), (1, 1)], [(1, 1)], [(2, 1), (1, 2), (1, 1)], [(2, 2), (1, 1), (1, 1), (2, 2)], [(3, 4), (1, 3), (2, 1)]])
        semillas.append(codifica(rnd, rnd.randint(1, 60), rnd.randint(1, 60), fact, clase=rnd.choice([0, 1]), q16=rnd.random() < 0.3,
                                 ri=rnd.choice([0, 1, 3]), aparte=rnd.choice([0, 1, 2]), sof=rnd.choice([0xC0, 0xC1]))[0])
    casos = []
    for i in range(cuantos):
        d = estropea(rnd, rnd.choice(semillas))
        if rnd.random() < 0.1:
            d = estropea(rnd, d)
        casos.append((d, rnd.choice([0, 0, 0, R2, R4, R8, COMP, COMP | R2, TR_NO, TR_SI | R4]), None, 0))
    for i in range(cuantos // 10):                      # y ruido puro detrás de un principio de JPEG
        casos.append((b'\xff\xd8' + bytes(rnd.getrandbits(8) for _ in range(rnd.randint(0, 400))), 0, None, 0))
    for i in range(cuantos // 10):                      # y con memoria de menos: nunca la usa
        d = estropea(rnd, rnd.choice(semillas))
        casos.append((d, rnd.choice([0, R2, COMP]), rnd.choice([0, 1, 100, 1000, 5000]), 0))
    res = corre(prog, casos, espera=900)
    bien = mal = 0
    for i, (c, r) in enumerate(zip(casos, res + [None] * len(casos))):
        if sano('estropeado %d' % i, r):
            if r.med == 0 and r.abre == 0 and r.final == 0:
                bien += 1
            else:
                mal += 1
        elif r is None and fallos <= 3:
            p = os.path.join(tempfile.gettempdir(), 'jpeg_prueba_estropeado_%d.jpg' % i)
            open(p, 'wb').write(c[0])
            print('       (guardado en %s, opciones %d)' % (p, c[1]))
    print('     %d estropeados: %d dan error y %d salen enteros (el daño cayó donde no rompe la lectura)' % (len(casos), mal, bien))


def parte_g(prog, cuantos, donde):
    print('== G: JPEG de verdad que hay en la máquina ==')
    global total
    vistos, lista = set(), []
    nombres = []
    for raiz in donde:
        try:
            out = subprocess.run(['find', raiz, '-xdev', '-type', 'f', '(', '-iname', '*.jpg', '-o', '-iname', '*.jpeg', ')', '-size', '-400k'],
                                 stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, timeout=120).stdout.decode('utf-8', 'replace').split('\n')
        except Exception:
            out = []
        nombres += [n for n in out if n and '/proc/' not in n and 'jpeg_prueba' not in n]
    nombres.sort()
    random.Random(7).shuffle(nombres)
    # de cada carpeta, pocos: que haya variedad
    por_carpeta = {}
    for n in nombres:
        if len(lista) >= cuantos:
            break
        c = os.path.dirname(n)
        if por_carpeta.get(c, 0) >= 3:
            continue
        try:
            d = open(n, 'rb').read()
        except OSError:
            continue
        h = hashlib.md5(d).digest()
        if h in vistos or len(d) < 4 or len(d) > ENT_MAX:
            continue
        vistos.add(h)
        por_carpeta[c] = por_carpeta.get(c, 0) + 1
        lista.append((n, d))
    if not lista:
        print('     (no hay ninguno)')
        return
    res = corre(prog, [(d, COMP, None, 0) for n, d in lista] + [(d, COMP | R4, None, 0) for n, d in lista])
    res += [None] * (2 * len(lista) - len(res))
    clases = {}
    malos = []
    for k, (n, d) in enumerate(lista):
        r, r4 = res[k], res[len(lista) + k]
        try:
            ref = ref_turbo(d)
            ref4 = ref_turbo(d, 4)
        except Exception as e:
            # la referencia no lo abre: tampoco tiene por qué jpeg.inc, pero que no se rompa
            if sano(n, r):
                malos.append('%s: la referencia no lo abre (%s); jpeg.inc da %d/%d/%d' % (n, str(e)[:60], r.med, r.abre, r.final))
            continue
        if r is not None and r.med == 0:
            t = '%s%d componente%s' % ('progresivo, ' if r.band & B_PROG else '', r.nc, 's' if r.nc > 1 else '')
            clases[t] = clases.get(t, 0) + 1
        antes = fallos
        compara(n, r, ref, 4 if ref.ndim == 3 and ref.shape[2] > 1 else 2, 'de verdad')
        prog_ = r is not None and r.band & B_PROG
        compara(n + ' a 1/4', r4, ref4, 255 if prog_ else (4 if ref.ndim == 3 and ref.shape[2] > 1 else 2), 'de verdad a 1/4' + (' (progresivos)' if prog_ else ''))
        if fallos > antes:
            malos.append(n)
    print('     %d archivos: %s' % (len(lista), ', '.join('%d de %s' % (v, k) for k, v in sorted(clases.items()))))
    for m in malos:
        print('     no va: %s' % m)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--azar', type=int, default=400)
    ap.add_argument('--rotos', type=int, default=4000)
    ap.add_argument('--de-verdad', type=int, default=80)
    ap.add_argument('--donde', default='/root,/home,/usr/share,/www,/var/www,/srv,/opt')
    ap.add_argument('--solo', default='ABCDEFG')
    a = ap.parse_args()
    if simplejpeg is None:
        print('  FAIL falta simplejpeg (la referencia de libjpeg-turbo sin suavizado)')
        sys.exit(1)
    rnd = random.Random(20261116)
    with tempfile.TemporaryDirectory() as tmp:
        prog = monta(tmp)
        if 'A' in a.solo:
            parte_a(prog, rnd, a.azar)
        if 'B' in a.solo:
            parte_b(prog, rnd, a.azar)
        if 'C' in a.solo:
            parte_c(prog, rnd, max(30, a.azar // 4))
        if 'D' in a.solo:
            parte_d(prog, rnd)
        if 'E' in a.solo:
            parte_e(prog, rnd)
        if 'F' in a.solo:
            parte_f(prog, rnd, a.rotos)
        if 'G' in a.solo:
            parte_g(prog, a.de_verdad, [x for x in a.donde.split(',') if os.path.isdir(x)])
    print('  El error contra la referencia (el mayor y el medio de cada componente):')
    for clase in sorted(errores):
        e = errores[clase]
        print('     %-58s %s' % (clase, '  '.join('%d / %.3f' % (c[0], c[1] / max(1, c[2])) for c in e)))
    print('  %s jpeg.inc: %d OK / %d FAIL' % ('OK  ' if not fallos else 'FAIL', total - fallos, fallos))
    sys.exit(1 if fallos else 0)


if __name__ == '__main__':
    main()
