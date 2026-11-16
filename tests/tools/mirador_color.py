#!/usr/bin/env python3
"""Prueba de programas/comun/apaleta.inc (de color verdadero a la paleta de Hispa
Ventanas) y de programas/comun/reduce.inc (cambiar de tamaño suavizando), fuera
de Hispa Ventanas.

uso: mirador_color.py [CUANTAS_AL_AZAR]

Ensambla las dos piezas TAL CUAL dentro de un programa de Linux de 32 bits y
compara lo que sale con el mismo cálculo hecho aquí, punto a punto: el color
más cercano, el tramado ordenado y la difusión de error (Floyd-Steinberg) en
imágenes al azar, y reducciones y ampliaciones a cualquier tamaño. Además mira
que sirvan para lo que son: que los colores que ya están en la paleta salgan
tal cual, y que una foto (un degradado) con difusión se parezca más al
original, de lejos, que redondeando cada punto por su cuenta.
"""
import os, random, struct, subprocess, sys, tempfile

RAIZ = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
MAXPX = 400 * 300

ARNES = r'''
        bits 32
        cpu 386
MAXPX   equ %d
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
; la fila EAX del origen, a EDI (para reduce.inc); la última no existe si [cab+24]
da_fila:
        mov ecx, [cab+12]
        dec ecx
        cmp eax, ecx
        jb .si
        cmp dword [cab+24], 0
        jne .no
.si:
        mov ecx, [cab+8]
        lea ecx, [ecx+ecx*2]
        imul eax, ecx
        lea esi, [origen+eax]
        rep movsb
        clc
        ret
.no:
        stc
        ret
_start:
        cld
        mov ecx, paleta
        mov edx, 1024
        call lee
        mov esi, paleta
        call ap_inicia
.caso:
        mov ecx, cab                ; operación, modo, ancho, alto, ancho y alto nuevos, sin la última
        mov edx, 28
        call lee
        mov eax, [cab+8]
        imul eax, [cab+12]
        lea edx, [eax+eax*2]
        mov ecx, origen
        call lee
        cmp dword [cab], 2
        je .reduce
        ; ---- a la paleta ----
        mov eax, [cab+4]
        mov [ap_modo], eax
        mov ecx, [cab+8]
        mov edi, errores
        call ap_empieza
        xor ebx, ebx
        mov esi, origen
        mov edi, salida
.f:
        cmp ebx, [cab+12]
        jae .f9
        mov eax, ebx
        mov ecx, [cab+8]
        mov ebp, 0x1234567
        call ap_fila
        cmp ebp, 0x1234567
        jne fin
        mov eax, [cab+8]
        add edi, eax
        lea eax, [eax+eax*2]
        add esi, eax
        inc ebx
        jmp .f
.f9:
        mov ecx, salida
        mov edx, [cab+8]
        imul edx, [cab+12]
        call escribe
        jmp .caso
.reduce:
        mov eax, [cab+8]
        mov ebx, [cab+12]
        mov ecx, [cab+16]
        mov edx, [cab+20]
        mov edi, trabajo
        mov esi, da_fila
        call red_prepara
        xor ebx, ebx
        mov edi, salida
.r:
        cmp ebx, [cab+20]
        jae .r9
        mov eax, ebx
        call red_fila
        jc .r9
        mov eax, [cab+16]
        lea eax, [eax+eax*2]
        add edi, eax
        inc ebx
        jmp .r
.r9:
        mov [cab], ebx
        mov ecx, cab
        mov edx, 4
        call escribe
        mov edx, [cab]
        imul edx, [cab+16]
        lea edx, [edx+edx*2]
        mov ecx, salida
        call escribe
        jmp .caso

%%include "apaleta.inc"
%%include "reduce.inc"

        section .bss
cab     resd 7
paleta  resb 1024
        APALETA_BSS
        REDUCE_BSS
errores resb AP_MEM(2048)
trabajo resb RED_MEM(2048, 2048)
origen  resb MAXPX * 3 + 64
salida  resb MAXPX * 3 + 64
'''

PAL = None
fallos = 0
total = 0


def res(ok, texto):
    global fallos, total
    total += 1
    print(('  OK   ' if ok else '  FAIL ') + texto)
    if not ok:
        fallos += 1


def paleta():
    import re
    cols, dentro = [], False
    for l in open(os.path.join(RAIZ, 'ventanas', 'datos.inc'), encoding='utf-8'):
        if l.startswith('paleta16:'):
            dentro = True
            continue
        if dentro:
            m = re.match(r'\s+db\s+0x(\w+),\s*0x(\w+),\s*0x(\w+)', l)
            if m:
                cols.append(tuple(int(x, 16) for x in m.groups()))
            elif cols:
                break
    assert len(cols) == 16
    cols += [(r * 51, g * 51, b * 51) for r in range(6) for g in range(6) for b in range(6)]
    cols += [(i * 11,) * 3 for i in range(24)]
    return cols


def monta(tmp):
    for f in ('apaleta', 'reduce'):
        s = open(os.path.join(RAIZ, 'programas', 'comun', f + '.inc'), encoding='utf-8').read()
        open(os.path.join(tmp, f + '.inc'), 'w', encoding='cp437', errors='replace').write(s)
    asm = os.path.join(tmp, 'arnes.asm')
    open(asm, 'w').write(ARNES % MAXPX)
    subprocess.run([os.environ.get('NASM', 'nasm'), '-f', 'elf32', '-i', tmp + os.sep, '-o', asm + '.o', asm], check=True)
    subprocess.run(['ld', '-m', 'elf_i386', '-o', os.path.join(tmp, 'arnes'), asm + '.o'], check=True)
    return os.path.join(tmp, 'arnes')


# ---- el mismo cálculo, aquí ----
def niv(v):
    return (min(255, max(0, v)) + 25) // 51


_cache = {}


def exacto(r, g, b):
    mejor, md = None, 1 << 30
    for i in [16 + 36 * niv(r) + 6 * niv(g) + niv(b)] + list(range(16)) + list(range(232, 256)):
        p = PAL[i]
        d = (p[0] - r) ** 2 + (p[1] - g) ** 2 + (p[2] - b) ** 2
        if d < md:
            mejor, md = i, d
    return mejor


def cercano(r, g, b):
    k = (r >> 3, g >> 3, b >> 3)
    if k not in _cache:
        _cache[k] = exacto(*((0 if c == 0 else 255 if c == 31 else c * 8 + 4) for c in k))
    return _cache[k]


BAYER = (0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5)


def a_paleta(px, an, al, modo):
    out = bytearray()
    if modo == 0:
        return bytes(cercano(*p) for p in px)
    if modo == 1:
        for y in range(al):
            for x in range(an):
                u = (BAYER[(y & 3) * 4 + (x & 3)] * 51 >> 4) - 24
                r, g, b = px[y * an + x]
                out.append(16 + 36 * niv(r + u) + 6 * niv(g + u) + niv(b + u))
        return bytes(out)
    e0 = [[0, 0, 0] for _ in range(an + 2)]
    for y in range(al):
        e1 = [[0, 0, 0] for _ in range(an + 2)]
        for x in range(an):
            v = [min(255, max(0, c + ((e0[x + 1][k] + 8) >> 4))) for k, c in enumerate(px[y * an + x])]
            i = cercano(*v)
            out.append(i)
            for k in range(3):
                e = v[k] - PAL[i][k]
                e0[x + 2][k] += 7 * e
                e1[x][k] += 3 * e
                e1[x + 1][k] += 5 * e
                e1[x + 2][k] += e
        e0 = e1
    return bytes(out)


def reduce(px, an, al, dan, dal, sin_ultima=False):
    out = []
    xb = [i * an // dan for i in range(dan + 1)]
    for dy in range(dal):
        y0 = dy * al // dal
        y1 = max(y0 + 1, (dy + 1) * al // dal)
        filas = [y for y in range(y0, y1) if not (sin_ultima and y == al - 1)]
        if sin_ultima and (y0 == al - 1 or not filas):
            break
        if sin_ultima and len(filas) < y1 - y0:
            pass                        # (cortada: con las que haya)
        for dx in range(dan):
            x0 = xb[dx]
            x1 = max(x0 + 1, xb[dx + 1])
            n = (x1 - x0) * len(filas)
            for k in range(3):
                s = sum(px[y * an + x][k] for y in filas for x in range(x0, x1))
                out.append((s + n // 2) // n)
    return bytes(out)


def corre(prog, casos):
    ent = bytes(c for p in PAL for c in p + (0,))
    for op, modo, an, al, dan, dal, corta, px in casos:
        ent += struct.pack('<7I', op, modo, an, al, dan, dal, corta) + bytes(c for p in px for c in p)
    sal = subprocess.run([prog], input=ent, stdout=subprocess.PIPE, timeout=600).stdout
    out, p = [], 0
    for op, modo, an, al, dan, dal, corta, px in casos:
        if op == 1:
            out.append(sal[p:p + an * al])
            p += an * al
        else:
            if p + 4 > len(sal):
                out.append(None)
                continue
            n = struct.unpack_from('<I', sal, p)[0]
            out.append(sal[p + 4:p + 4 + n * dan * 3])
            p += 4 + n * dan * 3
    return out


def lejos(px, idx, an, al):
    """el error medio de la imagen vista de lejos (medias de 4x4) respecto al original"""
    t = n = 0
    for y in range(0, al - 3, 4):
        for x in range(0, an - 3, 4):
            for k in range(3):
                a = sum(px[(y + j) * an + x + i][k] for j in range(4) for i in range(4))
                b = sum(PAL[idx[(y + j) * an + x + i]][k] for j in range(4) for i in range(4))
                t += abs(a - b) / 16
                n += 1
    return t / n


def main():
    global PAL
    cuantas = int(sys.argv[1]) if len(sys.argv) > 1 else 150
    PAL = paleta()
    rnd = random.Random(20261116)
    with tempfile.TemporaryDirectory() as tmp:
        prog = monta(tmp)
        # ---- a la paleta: igual que el cálculo de aquí ----
        casos = []
        for i in range(cuantas):
            an, al = rnd.randint(1, 60), rnd.randint(1, 30)
            clase = rnd.randrange(4)
            if clase == 0:
                px = [(rnd.getrandbits(8), rnd.getrandbits(8), rnd.getrandbits(8)) for _ in range(an * al)]
            elif clase == 1:
                px = [((x * 255) // an, (y * 255) // al, (x * 3 + y * 5) & 255) for y in range(al) for x in range(an)]
            elif clase == 2:
                px = [((x * 255) // an,) * 3 for y in range(al) for x in range(an)]
            else:
                px = [rnd.choice(PAL) for _ in range(an * al)]
            casos.append((1, i % 3, an, al, 0, 0, 0, px))
        R = corre(prog, casos)
        for modo, nombre in ((0, 'el color más cercano'), (1, 'el tramado ordenado'), (2, 'la difusión de error')):
            mal = sum(1 for c, r in zip(casos, R) if c[1] == modo and r != a_paleta(c[7], c[2], c[3], modo))
            n = sum(1 for c in casos if c[1] == modo)
            res(not mal, '%s: %d imágenes al azar, punto a punto con el cálculo de la prueba%s' % (nombre, n, '' if not mal else ' (%d distintas)' % mal))
        # ---- los colores de la paleta salen tal cual (los grises que casi coinciden
        #      con uno del cubo, a tres niveles como mucho: no se distinguen) ----
        px = [PAL[i] for i in range(256)]
        R = corre(prog, [(1, 0, 16, 16, 0, 0, 0, px)])
        dif = max(abs(a - b) for i in range(256) for a, b in zip(PAL[R[0][i]], PAL[i]))
        iguales = sum(1 for i in range(256) if PAL[R[0][i]] == PAL[i])
        res(dif <= 4 and iguales >= 248, 'los colores que ya son de la paleta salen tal cual: %d iguales y los demás a %d niveles' % (iguales, dif))
        # una mancha de cada uno de los 16 colores con nombre: lisa y de ese color, con los tres modos
        casos = [(1, m, 24, 8, 0, 0, 0, [PAL[i]] * (24 * 8)) for i in range(16) for m in (0, 2)]
        R = corre(prog, casos)
        mal = [c[7][0] for c, r in zip(casos, R) if any(PAL[v] != c[7][0] for v in r)]
        res(not mal, 'una mancha de cada color con nombre de Hispa Ventanas queda lisa y de su color%s' % ('' if not mal else ': %s' % mal[:6]))
        # ---- una foto: de lejos, la difusión se parece más que el más cercano ----
        an, al = 160, 96
        px = [(40 + (x * 150) // an, 60 + (y * 120) // al, 200 - (x * 90) // an - (y * 60) // al) for y in range(al) for x in range(an)]
        R = corre(prog, [(1, m, an, al, 0, 0, 0, px) for m in (0, 1, 2)])
        e = [lejos(px, r, an, al) for r in R]
        res(e[2] < e[0] / 3 and e[1] < e[0] / 2 and e[2] < 4,
            'un degradado visto de lejos: error medio %.1f al más cercano, %.1f ordenado, %.1f con difusión' % tuple(e))
        gr = [((x * 255) // 255,) * 3 for y in range(32) for x in range(256)]
        R = corre(prog, [(1, 2, 256, 32, 0, 0, 0, gr)])
        usados = set(R[0])
        res(all(PAL[i][0] == PAL[i][1] == PAL[i][2] for i in usados) and len(usados) >= 24,
            'una escala de grises usa solo grises, y %d distintos' % len(usados))
        # ---- cambiar de tamaño ----
        casos = []
        for i in range(cuantas):
            an, al = rnd.randint(1, 70), rnd.randint(1, 40)
            clase = rnd.randrange(4)
            if clase == 0:
                dan, dal = max(1, an // rnd.choice((2, 4, 8))), max(1, al // rnd.choice((2, 4, 8)))
            elif clase == 1:
                dan, dal = rnd.randint(1, an), rnd.randint(1, al)
            elif clase == 2:
                dan, dal = rnd.randint(an, an * 3), rnd.randint(al, al * 3)
            else:
                dan, dal = an, al
            px = [(rnd.getrandbits(8), rnd.getrandbits(8), rnd.getrandbits(8)) for _ in range(an * al)]
            casos.append((2, 0, an, al, dan, dal, 1 if i % 7 == 0 else 0, px))
        R = corre(prog, casos)
        mal = 0
        for c, r in zip(casos, R):
            if r != reduce(c[7], c[2], c[3], c[4], c[5], c[6]):
                mal += 1
                if mal < 5:
                    print('       %dx%d -> %dx%d (cortada: %d): %d octetos, se esperaban %d' % (
                        c[2], c[3], c[4], c[5], c[6], len(r or b''), len(reduce(c[7], c[2], c[3], c[4], c[5], c[6]))))
        res(not mal, 'cambiar de tamaño: %d imágenes (a la mitad, a un cuarto, a cualquier medida, ampliar y cortadas), punto a punto%s' % (
            len(casos), '' if not mal else ' (%d distintas)' % mal))
        # un tablero de ajedrez de puntos, a la mitad: gris liso
        px = [((255, 255, 255) if (x + y) & 1 else (0, 0, 0)) for y in range(40) for x in range(40)]
        R = corre(prog, [(2, 0, 40, 40, 20, 20, 0, px)])
        res(set(R[0]) <= {127, 128}, 'un ajedrezado de puntos reducido a la mitad queda gris liso (suavizado)')
    print('  %s colores y tamaños: %d comprobaciones, %d fallos' % ('OK  ' if not fallos else 'FAIL', total, fallos))
    return 1 if fallos else 0


if __name__ == '__main__':
    sys.exit(main())
