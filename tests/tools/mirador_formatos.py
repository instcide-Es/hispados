#!/usr/bin/env python3
"""Prueba de los formatos de Mirador (programas/mirador/*.inc) fuera de Hispa Ventanas.

uso: mirador_formatos.py [--rotos N] [--semilla S] [--guardar DIR] [--sin-jpeg]

Toma el núcleo de Mirador TAL CUAL (nucleo.inc y un .inc por formato, con
programas/comun/desinfla.inc y jpeg.inc), lo ensambla con NASM dentro de un
programa de Linux de 32 bits que lee archivos de imagen por la entrada y
devuelve la imagen descodificada (rojo, verde y azul de cada punto, con lo
transparente ya mezclado con el fondo), y lo compara punto a punto con lo que
tiene que salir:
  - imágenes hechas aquí, de todos los tamaños y en todas las variantes de
    cada formato (BMP, PCX, GIF, PNG, TGA, ICO, PBM/PGM/PPM), con escritores
    propios que saben lo que han metido; lo que además sabe leer Pillow se
    compara también con Pillow (así se prueban los escritores);
  - JPEG hechos por Pillow, con un margen (cada descodificador redondea a su
    manera);
  - lo que no se admite, que lo diga; y miles de archivos estropeados al azar
    o cortados: ni un cuelgue, ni una lectura fuera del archivo, ni una
    escritura fuera de su memoria. El archivo y la memoria de trabajo se le
    dan pegados a una página prohibida: pasarse un octeto para el programa.
Necesita Pillow (si no está en el Python del sistema, el guion de la prueba
hace un entorno virtual aparte). Escribe «  OK   ...» o «  FAIL ...».
"""
import io, os, random, struct, subprocess, sys, tempfile, zlib

from PIL import Image

RAIZ = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
MAXPX = 1500000            # puntos que caben en la salida del arnés
MAXMEM = 48 * 1024 * 1024  # memoria de trabajo que se le da como mucho

ARNES = r'''
        bits 32
        cpu 386
MAXPX   equ %d
MAXMEM  equ %d
%%define MI_CON_JPEG
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
; EAX=octetos -> EAX=un bloque de esa medida que ACABA en una página prohibida
; (pasarse un octeto es una violación de segmento); EDX=su base, ECX=su tamaño
bloque:
        push ebx
        push esi
        push edi
        push ebp
        mov [bl_n], eax
        lea ecx, [eax+4095]
        and ecx, ~4095
        mov [bl_util], ecx
        add ecx, 4096
        mov [bl_tam], ecx
        mov eax, 192                ; mmap2: privado y anónimo
        xor ebx, ebx
        mov edx, 3
        mov esi, 0x22
        mov edi, -1
        xor ebp, ebp
        int 0x80
        cmp eax, -4096
        ja fin
        mov [bl_base], eax
        mov ebx, eax                ; la última página, prohibida
        add ebx, [bl_util]
        mov ecx, 4096
        xor edx, edx
        mov eax, 125                ; mprotect
        int 0x80
        mov edx, [bl_base]
        mov ecx, [bl_tam]
        mov eax, edx
        add eax, [bl_util]
        sub eax, [bl_n]
        pop ebp
        pop edi
        pop esi
        pop ebx
        ret
suelta:                             ; EDX=base ECX=tamaño
        push ebx
        mov ebx, edx
        mov eax, 91
        int 0x80
        pop ebx
        ret

_start:
        cld
.caso:
        mov ecx, cab                ; fondo (4), color liso (4), octetos (4)
        mov edx, 12
        call lee
        mov eax, [cab+8]
        call bloque
        mov [arch], eax
        mov [arch_b], edx
        mov [arch_t], ecx
        mov ecx, eax
        mov edx, [cab+8]
        call lee
        mov edi, res
        mov ecx, 12
        xor eax, eax
        rep stosd
        mov eax, [cab]
        mov [mi_fondo], eax
        mov eax, [cab+4]
        mov [mi_fondo_rgb], eax
        mov esi, [arch]
        mov ecx, [cab+8]
        mov ebx, 0x11223344
        mov ebp, 0x55667788
        call mi_abre
        cmp ebx, 0x11223344         ; (que conserve los registros)
        jne .reg
        cmp ebp, 0x55667788
        jne .reg
        cmp esi, [arch]
        je .reg_ok
.reg:
        mov dword [res+36], 1
.reg_ok:
        mov [res], eax
        test eax, eax
        jnz .da
        mov eax, [im_an]
        mul dword [im_al]
        cmp eax, MAXPX
        ja .mucho
        cmp dword [mi_mem], MAXMEM
        jbe .cabe
.mucho:
        mov dword [res], 100
        jmp .da
.cabe:
        mov eax, [mi_mem]
        call bloque
        mov [mem_b], edx
        mov [mem_t], ecx
        push eax
        mov edi, eax                ; con basura: nadie cuenta con que venga a cero
        mov ecx, [mi_mem]
        mov al, 0xA5
        rep stosb
        pop edi
        call mi_prepara
        mov [res], eax
        test eax, eax
        jnz .suelta
        mov eax, [im_an]            ; el centinela, detrás de la última fila
        mul dword [im_al]
        lea edi, [salida+eax+eax*2]
        mov ecx, 64
        mov al, 0x5A
        rep stosb
        xor ebx, ebx
        mov edi, salida
.fila:
        cmp ebx, [im_al]
        jae .filas
        mov eax, ebx
        call mi_fila
        jc .filas
        mov eax, [im_an]
        lea eax, [eax+eax*2]
        add edi, eax
        inc ebx
        jmp .fila
.filas:
        mov [res+32], ebx
        mov eax, [im_an]
        mul dword [im_al]
        lea esi, [salida+eax+eax*2]
        mov ecx, 64
.cen:
        lodsb
        cmp al, 0x5A
        jne .cen_mal
        dec ecx
        jnz .cen
        jmp .suelta
.cen_mal:
        mov dword [res+36], 2
.suelta:
        mov edx, [mem_b]
        mov ecx, [mem_t]
        call suelta
.da:
        mov eax, [im_an]
        mov [res+4], eax
        mov eax, [im_al]
        mov [res+8], eax
        mov eax, [im_clase]
        mov [res+12], eax
        mov eax, [im_bits]
        mov [res+16], eax
        mov eax, [im_ncol]
        mov [res+20], eax
        mov eax, [im_hay_alfa]
        mov [res+24], eax
        mov eax, [im_aviso]
        mov [res+28], eax
        mov eax, [im_fmt]
        test eax, eax
        jz .sin_fmt
        sub eax, mi_formatos
        xor edx, edx
        mov ecx, F_TAM
        div ecx
        inc eax
.sin_fmt:
        mov [res+40], eax
        mov eax, [im_var]
        test eax, eax
        setnz byte [res+44]
        mov ecx, res
        mov edx, 48
        call escribe
        mov eax, [res+32]
        mul dword [im_an]
        lea edx, [eax+eax*2]
        mov ecx, salida
        call escribe
        mov edx, [arch_b]
        mov ecx, [arch_t]
        call suelta
        jmp .caso

%%include "desinfla.inc"
%%include "jpeg.inc"
%%include "nucleo.inc"
%%include "bmp.inc"
%%include "ico.inc"
%%include "pcx.inc"
%%include "gif.inc"
%%include "png.inc"
%%include "tga.inc"
%%include "pnm.inc"
%%include "jpg.inc"

        section .bss
cab     resd 3
res     resd 12
arch    resd 1
arch_b  resd 1
arch_t  resd 1
mem_b   resd 1
mem_t   resd 1
bl_n    resd 1
bl_util resd 1
bl_tam  resd 1
bl_base resd 1
        DESINFLA_BSS
        JPEG_BSS
        NUCLEO_BSS
        BMP_BSS
        ICO_BSS
        PCX_BSS
        GIF_BSS
        PNG_BSS
        TGA_BSS
        PNM_BSS
        JPG_BSS
salida  resb MAXPX * 3 + 256
'''

FORMATOS = ['?', 'PNG', 'GIF', 'BMP', 'JPEG', 'PNM', 'ICO', 'PCX', 'TGA']
fallos = 0
total = 0


def res(ok, texto):
    global fallos, total
    total += 1
    if not ok:
        fallos += 1
        if fallos <= 40:
            print('  FAIL ' + texto)


def monta(tmp, con_jpeg=True):
    for d, fs in (('mirador', ['nucleo', 'bmp', 'ico', 'pcx', 'gif', 'png', 'tga', 'pnm', 'jpg']),
                  ('comun', ['desinfla', 'jpeg'])):
        for f in fs:
            s = open(os.path.join(RAIZ, 'programas', d, f + '.inc'), encoding='utf-8').read()
            open(os.path.join(tmp, f + '.inc'), 'w', encoding='cp437', errors='replace').write(s)
    asm = os.path.join(tmp, 'arnes.asm')
    src = ARNES % (MAXPX, MAXMEM)
    if not con_jpeg:
        src = src.replace('%define MI_CON_JPEG', '').replace('%include "jpeg.inc"', '').replace('        JPEG_BSS', '')
    open(asm, 'w').write(src)
    subprocess.run([os.environ.get('NASM', 'nasm'), '-f', 'elf32', '-i', tmp + os.sep, '-o', asm + '.o', asm], check=True)
    subprocess.run(['ld', '-m', 'elf_i386', '-o', os.path.join(tmp, 'arnes'), asm + '.o'], check=True)
    return os.path.join(tmp, 'arnes')


class Res:
    pass


def corre(prog, casos, espera=600):
    """casos: [(datos, fondo, liso)] -> [Res o None (el programa se ha parado ahí)]"""
    ent = b''.join(struct.pack('<III', f, l, len(d)) + d for d, f, l in casos)
    try:
        p = subprocess.run([prog], input=ent, stdout=subprocess.PIPE, timeout=espera)
        sal = p.stdout
    except subprocess.TimeoutExpired as e:
        sal = e.stdout or b''
    out, p = [], 0
    for d, f, l in casos:
        if p + 48 > len(sal):
            out.append(None)
            continue
        r = Res()
        (r.err, r.an, r.al, r.clase, r.bits, r.ncol, r.alfa, r.aviso, r.filas, r.mal, r.fmt, r.var) = struct.unpack_from('<12I', sal, p)
        r.var &= 0xFF
        n = r.filas * r.an * 3
        r.rgb = sal[p + 48:p + 48 + n]
        p += 48 + n
        if len(r.rgb) != n:
            out.append(None)
            continue
        out.append(r)
    return out


# ----------------------------------------------------------------------------
#  Imágenes de prueba: (ancho, alto, [(r, g, b, a)])
# ----------------------------------------------------------------------------
def mezcla(px, an, al, fondo, liso):
    """lo que tiene que entregar el núcleo: lo transparente, sobre el fondo"""
    out = bytearray()
    lr, lg, lb = liso & 255, (liso >> 8) & 255, (liso >> 16) & 255
    for i, (r, g, b, a) in enumerate(px):
        if a == 255:
            out += bytes((r, g, b))
            continue
        if fondo == 0:
            x, y = i % an, i // an
            v = 0xCC if ((x >> 3) ^ (y >> 3)) & 1 else 0xFF
            bg = (v, v, v)
        else:
            bg = (lr, lg, lb)
        out += bytes(((c * a + f * (255 - a) + 127) // 255) for c, f in zip((r, g, b), bg))
    return bytes(out)


def imagen(rnd, an, al, clase):
    """clase: 'foto' (degradados y ruido), 'azar', 'liso' (manchas: se comprime bien)"""
    px = []
    if clase == 'azar':
        return [(rnd.getrandbits(8), rnd.getrandbits(8), rnd.getrandbits(8), 255) for _ in range(an * al)]
    cols = [(rnd.getrandbits(8), rnd.getrandbits(8), rnd.getrandbits(8)) for _ in range(6)]
    for y in range(al):
        for x in range(an):
            if clase == 'liso':
                c = cols[((x // 5) + (y // 3) * 2) % 6]
                px.append(c + (255,))
            else:
                px.append(((x * 255 // max(1, an - 1)) & 255, (y * 255 // max(1, al - 1)) & 255,
                           (x * 7 + y * 13 + rnd.getrandbits(3)) & 255, 255))
    return px


def con_alfa(rnd, px):
    return [(r, g, b, rnd.choice((0, 255, 255, 128, rnd.getrandbits(8)))) for r, g, b, a in px]


def con_paleta(rnd, an, al, ncol, clase='liso'):
    pal = [(rnd.getrandbits(8), rnd.getrandbits(8), rnd.getrandbits(8)) for _ in range(ncol)]
    if clase == 'azar':
        idx = [rnd.randrange(ncol) for _ in range(an * al)]
    else:
        idx = [(((x // 4) + (y // 2) * 3 + (rnd.getrandbits(1) if rnd.random() < 0.1 else 0)) % ncol)
               for y in range(al) for x in range(an)]
    return pal, idx


def empaqueta(vals, bits, relleno=1):
    """una fila de valores de «bits» bits, del de más peso al de menos; rellena hasta «relleno» octetos"""
    out = bytearray()
    acc = n = 0
    for v in vals:
        acc = (acc << bits) | v
        n += bits
        while n >= 8:
            n -= 8
            out.append((acc >> n) & 255)
    if n:
        out.append((acc << (8 - n)) & 255)
    while len(out) % relleno:
        out.append(0)
    return bytes(out)


# ----------------------------------------------------------------------------
#  BMP
# ----------------------------------------------------------------------------
def rle8(fila_idx, rnd):
    out = bytearray()
    i = 0
    while i < len(fila_idx):
        j = i
        while j < len(fila_idx) and j - i < 255 and fila_idx[j] == fila_idx[i]:
            j += 1
        if j - i >= 2 or rnd.random() < 0.3:
            out += bytes((j - i, fila_idx[i]))
            i = j
        else:
            n = min(len(fila_idx) - i, rnd.randint(3, 12))
            if n < 3:
                out += bytes((1, fila_idx[i]))
                i += 1
                continue
            out += bytes((0, n)) + bytes(fila_idx[i:i + n])
            if n & 1:
                out.append(0)
            i += n
    return out


def rle4(fila_idx, rnd):
    out = bytearray()
    i = 0
    while i < len(fila_idx):
        if rnd.random() < 0.6 or len(fila_idx) - i < 3:
            # una tirada de dos colores alternados
            a = fila_idx[i]
            b = fila_idx[i + 1] if i + 1 < len(fila_idx) else 0
            j = i
            while j < len(fila_idx) and j - i < 255 and fila_idx[j] == (a if (j - i) % 2 == 0 else b):
                j += 1
            out += bytes((j - i, (a << 4) | b))
            i = j
        else:
            n = min(len(fila_idx) - i, rnd.randint(3, 13))
            trozo = fila_idx[i:i + n]
            d = empaqueta(trozo, 4, 2)
            out += bytes((0, n)) + d
            i += n
    return out


def haz_bmp(rnd, an, al, bits, comp=0, arriba=False, cab=40, m565=False, alfa=False, clase='liso'):
    """-> (datos, puntos esperados)"""
    pal = None
    if bits <= 8:
        ncol = 1 << bits
        usados = rnd.choice((ncol, ncol, max(2, ncol // 2))) if cab != 12 else ncol
        pal, idx = con_paleta(rnd, an, al, usados, clase)
        px = [pal[i] + (255,) for i in idx]
    else:
        px = imagen(rnd, an, al, rnd.choice(('foto', 'azar')))
        if alfa:
            px = con_alfa(rnd, px)
    filas = []
    paso = ((an * bits + 31) // 32) * 4
    esperado = []
    for y in range(al):
        f = px[y * an:(y + 1) * an]
        if bits <= 8:
            fi = idx[y * an:(y + 1) * an]
            if comp == 1:
                d = rle8(fi, rnd) + b'\0\0'
            elif comp == 2:
                d = rle4(fi, rnd) + b'\0\0'
            else:
                d = empaqueta(fi, bits, 4)
            esperado += f
        elif bits == 16:
            d = bytearray()
            for r, g, b, a in f:
                if m565:
                    v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
                    esperado.append(((r >> 3) * 255 // 31, (g >> 2) * 255 // 63, (b >> 3) * 255 // 31, 255))
                else:
                    v = ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3)
                    esperado.append(((r >> 3) * 255 // 31, (g >> 3) * 255 // 31, (b >> 3) * 255 // 31, 255))
                d += struct.pack('<H', v)
            d += b'\0' * (paso - len(d))
        elif bits == 24:
            d = b''.join(bytes((b, g, r)) for r, g, b, a in f)
            d += b'\0' * (paso - len(d))
            esperado += [(r, g, b, 255) for r, g, b, a in f]
        else:
            d = b''.join(bytes((b, g, r, a if alfa else rnd.getrandbits(8))) for r, g, b, a in f)
            esperado += [(r, g, b, a if alfa else 255) for r, g, b, a in f]
        filas.append(bytes(d))
    if not arriba:
        filas.reverse()
    datos = b''.join(filas)
    if comp in (1, 2):
        datos = datos[:-2] + b'\0\1'
    compr = comp
    mascaras = b''
    if bits == 16 and m565:
        compr = 3
        mascaras = struct.pack('<III', 0xF800, 0x07E0, 0x001F)
    if bits == 32 and alfa:
        compr = 3
        mascaras = struct.pack('<IIII', 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000)
        cab = max(cab, 56)
    if cab == 12:
        c = struct.pack('<IHHHH', 12, an, al, 1, bits)
        p = b''.join(bytes((b, g, r)) for r, g, b in pal) if pal else b''
    else:
        c = struct.pack('<IiiHHIIiiII', cab, an, -al if arriba else al, 1, bits, compr, len(datos), 2835, 2835,
                        len(pal) if pal and len(pal) != (1 << bits) else 0, 0)
        if cab >= 52:
            m = mascaras if mascaras else struct.pack('<III', 0, 0, 0)
            c += m + b'\0' * (cab - 40 - len(m))
            mascaras = b''
        p = b''.join(bytes((b, g, r, 0)) for r, g, b in pal) if pal else b''
    # (con RLE, sin hueco: Pillow cuenta el relleno de los trozos desde el principio del archivo)
    hueco = b'\0' * (rnd.choice((0, 0, 3)) if not comp else 0)
    desde = 14 + len(c) + len(mascaras) + len(p) + len(hueco)
    return b'BM' + struct.pack('<IHHI', desde + len(datos), 0, 0, desde) + c + mascaras + p + hueco + datos, esperado


# ----------------------------------------------------------------------------
#  PCX
# ----------------------------------------------------------------------------
def pcx_rle(d):
    out = bytearray()
    i = 0
    while i < len(d):
        j = i
        while j < len(d) and j - i < 63 and d[j] == d[i]:
            j += 1
        if j - i > 1 or d[i] >= 0xC0:
            out += bytes((0xC0 | (j - i), d[i]))
        else:
            out.append(d[i])
        i = j
    return bytes(out)


def haz_pcx(rnd, an, al, variante, rle=True):
    """variante: 'mono', 'ega' (planos de 1 bit: 2, 3 o 4), 'p2', 'p4', '256', 'gris', 'rgb'"""
    opl = (an + 7) // 8
    planos, bits = 1, 1
    pal16 = bytes(rnd.getrandbits(8) for _ in range(48))
    cola = b''
    esperado = []
    filas = []
    if variante == 'mono':
        idx = [rnd.getrandbits(1) if rnd.random() < 0.2 else (x // 3 + y) & 1 for y in range(al) for x in range(an)]
        for y in range(al):
            filas.append(empaqueta(idx[y * an:(y + 1) * an], 1))
        esperado = [((255, 255, 255, 255) if i else (0, 0, 0, 255)) for i in idx]
    elif variante.startswith('ega'):
        planos = int(variante[3])
        pal, idx = con_paleta(rnd, an, al, 1 << planos)
        pal16 = b''.join(bytes(c) for c in pal).ljust(48, b'\0')
        for y in range(al):
            f = idx[y * an:(y + 1) * an]
            filas.append(b''.join(empaqueta([(v >> p) & 1 for v in f], 1) for p in range(planos)))
        esperado = [pal[i] + (255,) for i in idx]
    elif variante in ('p2', 'p4'):
        bits = int(variante[1])
        opl = (an * bits + 7) // 8
        pal, idx = con_paleta(rnd, an, al, 1 << bits)
        pal16 = b''.join(bytes(c) for c in pal).ljust(48, b'\0')
        for y in range(al):
            filas.append(empaqueta(idx[y * an:(y + 1) * an], bits))
        esperado = [pal[i] + (255,) for i in idx]
    elif variante in ('256', 'gris'):
        bits, opl = 8, an
        pal, idx = con_paleta(rnd, an, al, 256, rnd.choice(('liso', 'azar')))
        for y in range(al):
            filas.append(bytes(idx[y * an:(y + 1) * an]))
        if variante == '256':
            cola = b'\x0c' + b''.join(bytes(c) for c in pal)
            esperado = [pal[i] + (255,) for i in idx]
        else:
            esperado = [(i, i, i, 255) for i in idx]
    else:
        bits, opl, planos = 8, an, 3
        px = imagen(rnd, an, al, rnd.choice(('foto', 'azar', 'liso')))
        esperado = px
    if opl & 1:
        opl += 1                        # (lo corriente: un número par de octetos)
    cuerpo = bytearray()
    for y in range(al):
        if variante == 'rgb':
            f = px[y * an:(y + 1) * an]
            linea = b''.join(bytes(c[k] for c in f).ljust(opl, b'\0') for k in range(3))
        elif planos > 1:
            n = (an + 7) // 8
            linea = b''.join(filas[y][p * n:(p + 1) * n].ljust(opl, b'\0') for p in range(planos))
        else:
            linea = filas[y].ljust(opl, b'\0')
        cuerpo += pcx_rle(linea) if rle else linea
    # (un PCX de 256 grises sin paleta es de antes de la versión 5)
    cab = struct.pack('<BBBBHHHHHH', 10, 3 if variante == 'gris' else 5, 1 if rle else 0, bits, 0, 0, an - 1, al - 1, 72, 72) + pal16
    cab += struct.pack('<BBHH', 0, planos, opl, 1) + b'\0' * 58
    return cab + bytes(cuerpo) + cola, esperado


# ----------------------------------------------------------------------------
#  GIF
# ----------------------------------------------------------------------------
def gif_lzw(idx, minimo, rnd, sin_comprimir=False):
    borra, fin = 1 << minimo, (1 << minimo) + 1
    out = bytearray()
    acc = n = 0

    def pon(cod, tam):
        nonlocal acc, n
        acc |= cod << n
        n += tam
        while n >= 8:
            out.append(acc & 255)
            acc >>= 8
            n -= 8

    tabla = {}
    sig = fin + 1
    tam = minimo + 1
    pon(borra, tam)
    w = ()
    cuenta = 0
    for c in idx:
        wc = w + (c,)
        if len(wc) == 1 or wc in tabla:
            w = wc
            continue
        pon(tabla[w] if len(w) > 1 else w[0], tam)
        cuenta += 1
        if sig < 4096:
            tabla[wc] = sig
            sig += 1
            if sig > (1 << tam) and tam < 12:
                tam += 1
        if sin_comprimir or sig >= 4096 and rnd.random() < 0.5:
            pon(borra, tam)
            tabla = {}
            sig = fin + 1
            tam = minimo + 1
        w = (c,)
    if w:
        pon(tabla[w] if len(w) > 1 else w[0], tam)
        # (el descodificador añade un código más al recibir este)
        if sig < 4096:
            sig += 1
            if sig > (1 << tam) and tam < 12:
                tam += 1
    pon(fin, tam)
    if n:
        out.append(acc & 255)
    return bytes(out)


def trozos(d, rnd):
    out = bytearray()
    i = 0
    while i < len(d):
        n = min(len(d) - i, rnd.choice((255, 255, 100, 1, 17)))
        out.append(n)
        out += d[i:i + n]
        i += n
    out.append(0)
    return bytes(out)


def haz_gif(rnd, an, al, bits=8, entre=False, transp=False, local=False, v87=False, cuadro=None, sin_comprimir=False,
            animado=False, clase='liso'):
    ncol = 1 << bits
    pal, idx = con_paleta(rnd, an, al, ncol, clase)
    fx, fy, fw, fh = cuadro if cuadro else (0, 0, an, al)
    t = rnd.randrange(ncol) if transp else None
    fondo = rnd.randrange(ncol)
    d = (b'GIF87a' if v87 else b'GIF89a')
    otra = [(rnd.getrandbits(8), rnd.getrandbits(8), rnd.getrandbits(8)) for _ in range(ncol)]
    d += struct.pack('<HHBBB', an, al, (0x80 | (bits - 1) | 0x70) if not local or rnd.random() < 0.5 else 0, fondo, 0)
    hay_global = d[10] & 0x80
    if hay_global:
        d += b''.join(bytes(c) for c in (otra if local else pal))
    if not v87:
        d += b'\x21\xfe' + trozos(b'hecho para la prueba de Mirador', rnd)
        if transp or animado:
            d += b'\x21\xf9\x04' + bytes((1 if transp else 0, 10, 0, t if transp else 0)) + b'\0'
    cuadro_idx = [idx[(fy + y) * an + fx + x] for y in range(fh) for x in range(fw)]
    orden = list(range(fh))
    if entre:
        orden = [y for a, b in ((0, 8), (4, 8), (2, 4), (1, 2)) for y in range(a, fh, b)]
    flujo = [cuadro_idx[y * fw + x] for y in orden for x in range(fw)]
    d += b'\x2c' + struct.pack('<HHHHB', fx, fy, fw, fh, (0x40 if entre else 0) | ((0x80 | (bits - 1)) if local else 0))
    if local:
        d += b''.join(bytes(c) for c in pal)
    minimo = max(2, bits)
    d += bytes((minimo,)) + trozos(gif_lzw(flujo, minimo, rnd, sin_comprimir), rnd)
    if animado:   # un segundo cuadro, que no se tiene que ver
        d += b'\x21\xf9\x04\x00\x0a\x00\x00\x00\x2c' + struct.pack('<HHHHB', 0, 0, an, al, 0)
        d += bytes((minimo,)) + trozos(gif_lzw([rnd.randrange(ncol) for _ in range(an * al)], minimo, rnd), rnd)
    d += b'\x3b'
    esperado = []
    for y in range(al):
        for x in range(an):
            if fx <= x < fx + fw and fy <= y < fy + fh:
                i = idx[y * an + x]
            else:
                i = t if transp else fondo
            if not local and not hay_global:
                c = (i, i, i)
            else:
                c = pal[i] if (local or fx <= x < fx + fw and fy <= y < fy + fh or True) else otra[i]
            esperado.append(c + ((0,) if transp and i == t else (255,)))
    return d, esperado


# ----------------------------------------------------------------------------
#  PNG
# ----------------------------------------------------------------------------
def png_trozo(nombre, d):
    return struct.pack('>I', len(d)) + nombre + d + struct.pack('>I', zlib.crc32(nombre + d) & 0xFFFFFFFF)


def png_filtra(tipo, fila, ant, bpp):
    out = bytearray(len(fila))
    for i, v in enumerate(fila):
        a = fila[i - bpp] if i >= bpp else 0
        b = ant[i]
        c = ant[i - bpp] if i >= bpp else 0
        if tipo == 0:
            p = 0
        elif tipo == 1:
            p = a
        elif tipo == 2:
            p = b
        elif tipo == 3:
            p = (a + b) >> 1
        else:
            pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
            p = a if pa <= pb and pa <= pc else (b if pb <= pc else c)
        out[i] = (v - p) & 255
    return bytes(out)


ADAM = ((0, 0, 8, 8), (4, 0, 8, 8), (0, 4, 4, 8), (2, 0, 4, 4), (0, 2, 2, 4), (1, 0, 2, 2), (0, 1, 1, 2))


def haz_png(rnd, an, al, color, prof, entre=False, trns=False, filtro=None, nidat=1, nivel=6, clase=None):
    """color: 0 gris, 2 RGB, 3 paleta, 4 gris+alfa, 6 RGBA. -> (datos, esperado)"""
    mx = (1 << prof) - 1
    muestras = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[color]
    extra = b''
    # los puntos, como muestras de «prof» bits
    if color == 3:
        ncol = rnd.choice((1 << prof, max(1, (1 << prof) - 1)))
        pal = [(rnd.getrandbits(8), rnd.getrandbits(8), rnd.getrandbits(8)) for _ in range(ncol)]
        m = [(rnd.randrange(ncol),) for _ in range(an * al)] if clase == 'azar' else \
            [((x // 3 + y) % ncol,) for y in range(al) for x in range(an)]
        extra = png_trozo(b'PLTE', b''.join(bytes(c) for c in pal))
        alfas = [255] * ncol
        if trns:
            n = rnd.randint(1, ncol)
            alfas[:n] = [rnd.choice((0, 255, rnd.getrandbits(8))) for _ in range(n)]
            extra += png_trozo(b'tRNS', bytes(alfas[:n]))
        esperado = [pal[i[0]] + (alfas[i[0]],) for i in m]
    else:
        if clase == 'liso':
            base = [tuple(rnd.randint(0, mx) for _ in range(muestras)) for _ in range(5)]
            m = [base[(x // 4 + y // 3) % 5] for y in range(al) for x in range(an)]
        else:
            m = [tuple(rnd.randint(0, mx) for _ in range(muestras)) for _ in range(an * al)]
        clave = None
        if trns and color in (0, 2):
            clave = m[rnd.randrange(len(m))][:muestras]
            extra = png_trozo(b'tRNS', b''.join(struct.pack('>H', v) for v in clave))

        def a8(v):
            return v >> 8 if prof == 16 else v * 255 // mx
        esperado = []
        for p in m:
            if color == 0:
                g = a8(p[0])
                esperado.append((g, g, g, 0 if clave == p else 255))
            elif color == 2:
                esperado.append((a8(p[0]), a8(p[1]), a8(p[2]), 0 if clave == p else 255))
            elif color == 4:
                g = a8(p[0])
                esperado.append((g, g, g, a8(p[1])))
            else:
                esperado.append(tuple(a8(v) for v in p))
    bpp = max(1, muestras * prof // 8)

    def fila_bytes(pts):
        if prof == 16:
            return b''.join(struct.pack('>H', v) for p in pts for v in p)
        if prof == 8:
            return bytes(v for p in pts for v in p)
        return empaqueta([p[0] for p in pts], prof)

    crudo = bytearray()
    pasadas = ADAM if entre else ((0, 0, 1, 1),)
    for x0, y0, dx, dy in pasadas:
        if x0 >= an or y0 >= al:
            continue
        ant = None
        for y in range(y0, al, dy):
            f = fila_bytes([m[y * an + x] for x in range(x0, an, dx)])
            if ant is None:
                ant = bytes(len(f))
            t = rnd.randrange(5) if filtro is None else filtro
            crudo.append(t)
            crudo += png_filtra(t, f, ant, bpp)
            ant = f
    z = zlib.compress(bytes(crudo), nivel)
    d = b'\x89PNG\r\n\x1a\n' + png_trozo(b'IHDR', struct.pack('>IIBBBBB', an, al, prof, color, 0, 0, 1 if entre else 0))
    d += png_trozo(b'gAMA', struct.pack('>I', 45455)) + extra
    n = max(1, len(z) // nidat)
    for i in range(0, len(z), n):
        d += png_trozo(b'IDAT', z[i:i + n])
    d += png_trozo(b'tEXt', b'Comment\0prueba') + png_trozo(b'IEND', b'')
    return d, esperado


# ----------------------------------------------------------------------------
#  TGA
# ----------------------------------------------------------------------------
def haz_tga(rnd, an, al, tipo, bits, rle=False, arriba=False, derecha=False, palbits=24, alfa=False, nombre=b''):
    """tipo: 1 paleta, 2 color, 3 grises"""
    pal_d = b''
    if tipo == 1:
        pal, idx = con_paleta(rnd, an, al, 256, rnd.choice(('liso', 'azar')))
        pts = [bytes((i,)) for i in idx]
        esperado = []
        for i in idx:
            r, g, b = pal[i]
            if palbits in (15, 16):
                esperado.append(((r >> 3) * 255 // 31, (g >> 3) * 255 // 31, (b >> 3) * 255 // 31, 255))
            else:
                esperado.append((r, g, b, 255))
        for r, g, b in pal:
            if palbits in (15, 16):
                pal_d += struct.pack('<H', ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3))
            elif palbits == 24:
                pal_d += bytes((b, g, r))
            else:
                pal_d += bytes((b, g, r, 255))
    elif tipo == 3:
        g = [rnd.getrandbits(8) if rnd.random() < 0.3 else (x * 3 + y) & 255 for y in range(al) for x in range(an)]
        pts = [bytes((v,)) for v in g]
        esperado = [(v, v, v, 255) for v in g]
    else:
        px = imagen(rnd, an, al, rnd.choice(('foto', 'liso', 'azar')))
        if alfa:
            px = con_alfa(rnd, px)
        pts, esperado = [], []
        for r, g, b, a in px:
            if bits in (15, 16):
                pts.append(struct.pack('<H', ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3) | (0x8000 if rnd.getrandbits(1) else 0)))
                esperado.append(((r >> 3) * 255 // 31, (g >> 3) * 255 // 31, (b >> 3) * 255 // 31, 255))
            elif bits == 24:
                pts.append(bytes((b, g, r)))
                esperado.append((r, g, b, 255))
            else:
                pts.append(bytes((b, g, r, a if alfa else rnd.getrandbits(8))))
                esperado.append((r, g, b, a if alfa else 255))
    # el orden del archivo
    filas = [pts[y * an:(y + 1) * an] for y in range(al)]
    if derecha:
        filas = [f[::-1] for f in filas]
    if not arriba:
        filas.reverse()
    orden = [p for f in filas for p in f]
    if rle:
        cuerpo = bytearray()
        i = 0
        while i < len(orden):
            j = i
            while j < len(orden) and j - i < 128 and orden[j] == orden[i]:
                j += 1
            if j - i > 1 or rnd.random() < 0.2:
                cuerpo += bytes((0x80 | (j - i - 1),)) + orden[i]
                i = j
            else:
                n = min(len(orden) - i, rnd.randint(1, 128))
                cuerpo += bytes((n - 1,)) + b''.join(orden[i:i + n])
                i += n
    else:
        cuerpo = b''.join(orden)
    desc = (0x20 if arriba else 0) | (0x10 if derecha else 0) | (8 if alfa else 0)
    cab = struct.pack('<BBBHHBHHHHBB', len(nombre), 1 if tipo == 1 else 0, tipo + (8 if rle else 0),
                      0, 256 if tipo == 1 else 0, palbits if tipo == 1 else 0, 0, 0, an, al, bits, desc)
    return cab + nombre + pal_d + bytes(cuerpo), esperado


# ----------------------------------------------------------------------------
#  PBM, PGM, PPM
# ----------------------------------------------------------------------------
def haz_pnm(rnd, an, al, tipo, maximo=255):
    def escala(v):
        return v if maximo == 255 else (min(v, maximo) * 255 + maximo // 2) // maximo
    sep = lambda: rnd.choice((b' ', b'\n', b'\t', b'  ', b'\r\n', b'\n# un comentario\n'))
    cab = b'P%d' % tipo + sep() + b'%d' % an + sep() + b'%d' % al
    if tipo in (1, 4):
        bitsv = [rnd.getrandbits(1) if rnd.random() < 0.3 else (x // 2 + y) & 1 for y in range(al) for x in range(an)]
        esperado = [((0, 0, 0, 255) if v else (255, 255, 255, 255)) for v in bitsv]
        if tipo == 1:
            return cab + b'\n' + b''.join((b'%d' % v) + rnd.choice((b'', b' ', b'\n')) for v in bitsv), esperado
        return cab + b'\n' + b''.join(empaqueta(bitsv[y * an:(y + 1) * an], 1) for y in range(al)), esperado
    cab += sep() + b'%d' % maximo
    n = 1 if tipo in (2, 5) else 3
    vals = [rnd.randint(0, maximo) for _ in range(an * al * n)]
    e = [escala(v) for v in vals]
    if n == 1:
        esperado = [(v, v, v, 255) for v in e]
    else:
        esperado = [(e[i], e[i + 1], e[i + 2], 255) for i in range(0, len(e), 3)]
    if tipo in (2, 3):
        return cab + b'\n' + b''.join((b'%d' % v) + rnd.choice((b' ', b'\n', b'  ')) for v in vals), esperado
    if maximo > 255:
        return cab + b'\n' + b''.join(struct.pack('>H', v) for v in vals), esperado
    return cab + b'\n' + bytes(vals), esperado


# ----------------------------------------------------------------------------
#  ICO
# ----------------------------------------------------------------------------
def ico_bmp(rnd, lado, bits, alto=None):
    al = alto or lado
    an = lado
    esperado, filas, mascara = [], [], []
    pal_d = b''
    if bits <= 8:
        pal, idx = con_paleta(rnd, an, al, 1 << bits)
        pal_d = b''.join(bytes((b, g, r, 0)) for r, g, b in pal)
    else:
        px = imagen(rnd, an, al, 'foto')
    for y in range(al):
        m = [1 if rnd.random() < 0.25 else 0 for _ in range(an)]
        if bits <= 8:
            f = idx[y * an:(y + 1) * an]
            filas.append(empaqueta(f, bits, 4))
            esperado += [pal[i] + ((0,) if t else (255,)) for i, t in zip(f, m)]
        elif bits == 24:
            f = px[y * an:(y + 1) * an]
            filas.append(b''.join(bytes((b, g, r)) for r, g, b, a in f).ljust(((an * 24 + 31) // 32) * 4, b'\0'))
            esperado += [(r, g, b, 0 if t else 255) for (r, g, b, a), t in zip(f, m)]
        else:
            f = [(r, g, b, rnd.choice((0, 255, 255, rnd.getrandbits(8)))) for r, g, b, a in px[y * an:(y + 1) * an]]
            filas.append(b''.join(bytes((b, g, r, a)) for r, g, b, a in f))
            esperado += f
        mascara.append(empaqueta(m, 1, 4))
    cab = struct.pack('<IiiHHIIiiII', 40, an, al * 2, 1, bits, 0, 0, 0, 0, 0, 0)
    return cab + pal_d + b''.join(reversed(filas)) + b''.join(reversed(mascara)), esperado


def haz_ico(rnd, imagenes, cursor=False):
    """imagenes: [(lado, bits o 'png')] -> la mayor es la que se tiene que ver"""
    partes, mejor = [], None
    for lado, bits in imagenes:
        if bits == 'png':
            d, e = haz_png(rnd, lado, lado, 6, 8)
            b = 32
        else:
            d, e = ico_bmp(rnd, lado, bits)
            b = bits
        partes.append((lado, b, d))
        if mejor is None or (lado * lado * 64 + b) > mejor[0]:
            mejor = (lado * lado * 64 + b, e, lado)
    d = struct.pack('<HHH', 0, 2 if cursor else 1, len(partes))
    pos = 6 + 16 * len(partes)
    for lado, b, dat in partes:
        d += struct.pack('<BBBBHHII', lado & 255, lado & 255, 0, 0, 1, b, len(dat), pos)
        pos += len(dat)
    return d + b''.join(p[2] for p in partes), mejor[1], mejor[2]


# ----------------------------------------------------------------------------
def pillow_rgba(d):
    """lo que lee Pillow, como [(r, g, b, a)]; None si no sabe"""
    try:
        im = Image.open(io.BytesIO(d))
        im.load()
        if im.mode == 'I;16':
            return None
        im = im.convert('RGBA')
        return im.size, list(im.get_flattened_data() if hasattr(im, 'get_flattened_data') else im.getdata())
    except Exception:
        return None


def casos_buenos(rnd):
    """-> [(nombre, formato, datos, ancho, alto, esperado, con_pillow)]"""
    L = []

    def tam():
        return rnd.choice((1, 2, 3, 7, 8, 9, 15, 16, 17, 31, 33, 64)), rnd.choice((1, 2, 3, 5, 8, 9, 16, 21, 40))

    def pon(nombre, fmt, an, al, de, pillow=True):
        d, e = de
        L.append((nombre, fmt, d, an, al, e, pillow))

    # ---- BMP ----
    for rep in range(3):
        for bits in (1, 4, 8, 16, 24, 32):
            for arriba in (False, True):
                for cab in ((12, 40, 64, 108, 124) if not arriba else (40, 124)):
                    if cab == 12 and bits > 8 and bits != 24:
                        continue
                    an, al = tam()
                    pon('BMP %d bits, cabecera %d%s' % (bits, cab, ', de arriba abajo' if arriba else ''), 'BMP', an, al,
                        haz_bmp(rnd, an, al, bits, 0, arriba, cab, clase=rnd.choice(('liso', 'azar'))), pillow=(cab != 64 or bits != 16))
        for comp, bits in ((1, 8), (2, 4)):
            for clase in ('liso', 'azar'):
                an, al = tam()
                pon('BMP RLE%d %s' % (bits, clase), 'BMP', an, al, haz_bmp(rnd, an, al, bits, comp, False, 40, clase=clase))
        an, al = tam()
        pon('BMP 16 bits 5-6-5', 'BMP', an, al, haz_bmp(rnd, an, al, 16, m565=True))
        an, al = tam()
        pon('BMP 16 bits 5-6-5, cabecera 124', 'BMP', an, al, haz_bmp(rnd, an, al, 16, m565=True, cab=124))
        an, al = tam()
        pon('BMP 32 bits con opacidad', 'BMP', an, al, haz_bmp(rnd, an, al, 32, alfa=True, cab=124))
    pon('BMP grande de 24 bits', 'BMP', 400, 300, haz_bmp(rnd, 400, 300, 24))
    pon('BMP grande RLE8', 'BMP', 500, 333, haz_bmp(rnd, 500, 333, 8, 1))
    # ---- PCX ----
    for rep in range(4):
        for v in ('mono', 'ega2', 'ega3', 'ega4', 'p2', 'p4', '256', 'gris', 'rgb'):
            for rle in (True, False):
                an, al = tam()
                pon('PCX %s%s' % (v, '' if rle else ' sin comprimir'), 'PCX', an, al, haz_pcx(rnd, an, al, v, rle),
                    pillow=(v in ('mono', 'ega4', '256', 'rgb') and rle))
    pon('PCX grande de 256 colores', 'PCX', 321, 200, haz_pcx(rnd, 321, 200, '256'))
    pon('PCX grande de 24 bits', 'PCX', 255, 201, haz_pcx(rnd, 255, 201, 'rgb'))
    # ---- GIF ----
    for rep in range(4):
        for bits in (1, 2, 3, 4, 5, 6, 7, 8):
            for entre in (False, True):
                an, al = tam()
                tr = rnd.random() < 0.5
                lo = rnd.random() < 0.3
                v87 = not tr and rnd.random() < 0.3
                pon('GIF %d bits%s%s%s%s' % (bits, ', entrelazado' if entre else '', ', transparente' if tr else '',
                                            ', paleta local' if lo else '', ', 87a' if v87 else ''), 'GIF', an, al,
                    haz_gif(rnd, an, al, bits, entre, tr, lo, v87, clase=rnd.choice(('liso', 'azar'))))
        pon('GIF sin comprimir', 'GIF', 33, 9, haz_gif(rnd, 33, 9, 8, sin_comprimir=True))
        pon('GIF animado: el primer cuadro', 'GIF', 20, 12, haz_gif(rnd, 20, 12, 4, animado=True))
        pon('GIF con el cuadro más pequeño que la pantalla', 'GIF', 30, 20, haz_gif(rnd, 30, 20, 4, transp=True, cuadro=(5, 3, 20, 11)),
            pillow=False)
        pon('GIF con el cuadro más pequeño, entrelazado y sin transparente', 'GIF', 30, 20,
            haz_gif(rnd, 30, 20, 4, entre=True, cuadro=(2, 4, 28, 13)), pillow=False)
    pon('GIF grande (el diccionario se llena)', 'GIF', 300, 220, haz_gif(rnd, 300, 220, 8, clase='azar'))
    pon('GIF grande entrelazado', 'GIF', 301, 221, haz_gif(rnd, 301, 221, 8, entre=True, clase='liso'))
    # ---- PNG ----
    for rep in range(3):
        for color, profs in ((0, (1, 2, 4, 8, 16)), (2, (8, 16)), (3, (1, 2, 4, 8)), (4, (8, 16)), (6, (8, 16))):
            for prof in profs:
                for entre in (False, True):
                    for trns in ((False, True) if color in (0, 2, 3) else (False,)):
                        an, al = tam()
                        pon('PNG color %d, %d bits%s%s' % (color, prof, ', Adam7' if entre else '', ', tRNS' if trns else ''),
                            'PNG', an, al, haz_png(rnd, an, al, color, prof, entre, trns, nidat=rnd.choice((1, 1, 3)),
                                                   clase=rnd.choice(('liso', 'azar'))),
                            pillow=(prof <= 8 and not (trns and color in (0, 2) and prof < 8)))
    for f in range(5):
        for color, prof in ((2, 8), (6, 16), (0, 4), (3, 2)):
            pon('PNG filtro %d, color %d, %d bits' % (f, color, prof), 'PNG', 37, 19, haz_png(rnd, 37, 19, color, prof, filtro=f),
                pillow=(prof <= 8))
    for nivel in (0, 1, 9):
        pon('PNG grande, compresión %d' % nivel, 'PNG', 300, 200, haz_png(rnd, 300, 200, 2, 8, nivel=nivel, clase='liso'))
    pon('PNG grande con Adam7 y opacidad', 'PNG', 257, 199, haz_png(rnd, 257, 199, 6, 8, entre=True, clase='liso'))
    for an, al in ((1, 1), (2, 1), (1, 2), (3, 3), (4, 5), (5, 4), (8, 8), (9, 9)):
        pon('PNG Adam7 de %dx%d' % (an, al), 'PNG', an, al, haz_png(rnd, an, al, 2, 8, entre=True))
        pon('PNG Adam7 de %dx%d, 1 bit' % (an, al), 'PNG', an, al, haz_png(rnd, an, al, 0, 1, entre=True))
    # ---- TGA ----
    for rep in range(3):
        for tipo, bits in ((1, 8), (2, 15), (2, 16), (2, 24), (2, 32), (3, 8)):
            for rle in (False, True):
                for arriba in (False, True):
                    an, al = tam()
                    der = rnd.random() < 0.25
                    alfa = bits == 32 and rnd.random() < 0.6
                    pb = rnd.choice((15, 16, 24, 32))
                    nom = rnd.choice((b'', b'una imagen'))   # (con nombre de 10 letras Pillow lo toma por un PCX)
                    pon('TGA tipo %d, %d bits%s%s%s%s' % (tipo, bits, ', RLE' if rle else '', ', de arriba abajo' if arriba else '',
                                                        ', de derecha a izquierda' if der else '', ', con opacidad' if alfa else ''),
                        'TGA', an, al, haz_tga(rnd, an, al, tipo, bits, rle, arriba, der, pb, alfa, nombre=nom),
                        pillow=(not nom and not der and not rle and not (bits == 32 and not alfa) and bits not in (15, 16) and not (tipo == 1 and pb != 24)))
    pon('TGA grande RLE de 24 bits', 'TGA', 320, 240, haz_tga(rnd, 320, 240, 2, 24, rle=True))
    # ---- PNM ----
    for rep in range(3):
        for tipo in (1, 2, 3, 4, 5, 6):
            for maximo in ((255,) if tipo in (1, 4) else (255, 1, 15, 100, 1023, 65535)):
                an, al = tam()
                pon('PNM P%d, máximo %d' % (tipo, maximo), 'PNM', an, al, haz_pnm(rnd, an, al, tipo, maximo), pillow=(maximo == 255))
    pon('PPM grande', 'PNM', 400, 250, haz_pnm(rnd, 400, 250, 6))
    # ---- ICO ----
    for rep in range(3):
        for bits in (1, 4, 8, 24, 32):
            lado = rnd.choice((16, 24, 32, 48))
            d, e, l = haz_ico(rnd, [(lado, bits)], cursor=rnd.random() < 0.2)
            L.append(('ICO de %d bits y %d de lado' % (bits, lado), 'ICO', d, l, l, e, False))
        d, e, l = haz_ico(rnd, [(16, 4), (48, 8), (32, 32), (48, 32), (24, 24)])
        L.append(('ICO con cinco imágenes: la mayor', 'ICO', d, l, l, e, False))
        d, e, l = haz_ico(rnd, [(16, 4), (64, 'png'), (32, 8)])
        L.append(('ICO con un PNG dentro', 'ICO', d, l, l, e, False))
    return L


def prueba_buenos(prog, rnd, guardar):
    L = casos_buenos(rnd)
    por_fmt = {}
    entradas, metas = [], []
    for nombre, fmt, d, an, al, e, pil in L:
        if guardar:
            ext = {'PNM': 'ppm'}.get(fmt, fmt.lower())
            open(os.path.join(guardar, '%03d_%s.%s' % (len(metas), nombre.replace(' ', '_').replace(',', '')[:40], ext)), 'wb').write(d)
        # los escritores de aquí, contra Pillow (donde Pillow sabe)
        if pil:
            pr = pillow_rgba(d)
            if pr is None:
                res(False, '%s: Pillow no lo abre (¿el escritor de la prueba está mal?)' % nombre)
            elif pr[0] != (an, al) or pr[1] != [tuple(p) for p in e]:
                dif = sum(1 for a, b in zip(pr[1], e) if tuple(a) != tuple(b)) if pr[0] == (an, al) else -1
                res(False, '%s: Pillow no ve lo mismo que dice el escritor de la prueba (%d puntos distintos)' % (nombre, dif))
        for fondo, liso in ((0, 0), (1, rnd.getrandbits(24))):
            entradas.append((d, fondo, liso))
            metas.append((nombre, fmt, an, al, mezcla(e, an, al, fondo, liso), any(p[3] != 255 for p in e)))
    R = corre(prog, entradas)
    for r, (nombre, fmt, an, al, rgb, alfa) in zip(R, metas):
        c = por_fmt.setdefault(fmt, [0, 0])
        c[0] += 1
        if r is None:
            res(False, '%s: el programa se ha parado (¿un cuelgue o un acceso fuera de sitio?)' % nombre)
            break
        mal = []
        if r.err:
            mal.append('error %d' % r.err)
        else:
            if FORMATOS[r.fmt] != fmt:
                mal.append('lo toma por %s' % FORMATOS[r.fmt])
            if (r.an, r.al) != (an, al):
                mal.append('mide %dx%d y es de %dx%d' % (r.an, r.al, an, al))
            elif r.filas != al:
                mal.append('da %d filas de %d' % (r.filas, al))
            elif r.rgb != rgb:
                n = sum(1 for i in range(0, len(rgb), 3) if rgb[i:i + 3] != r.rgb[i:i + 3])
                i = next(i for i in range(0, len(rgb), 3) if rgb[i:i + 3] != r.rgb[i:i + 3])
                mal.append('%d puntos distintos; el primero, el %d,%d: sale %s y es %s' % (
                    n, (i // 3) % an, (i // 3) // an, r.rgb[i:i + 3].hex(), rgb[i:i + 3].hex()))
            if r.aviso:
                mal.append('avisa de que está cortado, y está entero')
            if bool(r.alfa) != alfa and fmt not in ('ICO',) and not (alfa is False and r.alfa):
                mal.append('transparencia: dice %d' % r.alfa)
        if r.mal:
            mal.append('no conserva los registros' if r.mal == 1 else 'ha escrito detrás de la imagen')
        res(not mal, '%s: %s' % (nombre, '; '.join(mal)))
        if mal:
            c[1] += 1
    for fmt in ('BMP', 'PCX', 'GIF', 'PNG', 'TGA', 'ICO', 'PNM'):
        n, m = por_fmt.get(fmt, (0, 0))
        print('  %s %s: %d imágenes punto a punto%s' % ('OK  ' if not m and n else 'FAIL', fmt, n, '' if not m else ', %d mal' % m))
    return L


def prueba_jpeg(prog, rnd):
    entradas, metas = [], []
    for nombre, modo, kw in (('JPEG de color', 'RGB', {}), ('JPEG gris', 'L', {}),
                             ('JPEG progresivo', 'RGB', {'progressive': True}),
                             ('JPEG 4:4:4', 'RGB', {'subsampling': 0}), ('JPEG de 1x1', 'RGB', {})):
        an, al = (1, 1) if '1x1' in nombre else (rnd.randint(20, 90), rnd.randint(20, 70))
        im = Image.new('RGB', (an, al))
        im.putdata([((x * 255) // an, (y * 255) // al, ((x + y) * 4) & 255) for y in range(al) for x in range(an)])
        im = im.convert(modo)
        b = io.BytesIO()
        im.save(b, 'JPEG', quality=90, **kw)
        ref = Image.open(io.BytesIO(b.getvalue())).convert('RGB').tobytes()
        entradas.append((b.getvalue(), 0, 0))
        metas.append((nombre, an, al, ref))
    for r, (nombre, an, al, ref) in zip(corre(prog, entradas), metas):
        if r is None:
            res(False, nombre + ': el programa se ha parado')
            break
        mal = []
        if r.err:
            mal.append('error %d' % r.err)
        elif (r.an, r.al, r.filas) != (an, al, al):
            mal.append('mide %dx%d (%d filas) y es de %dx%d' % (r.an, r.al, r.filas, an, al))
        else:
            # (jpeg.inc repite los puntos del color submuestreado y Pillow los suaviza: margen ancho)
            dif = sorted(abs(a - b) for a, b in zip(r.rgb, ref))
            if dif[len(dif) * 9 // 10] > 12:
                mal.append('se aparta de Pillow: el 90 %% de las muestras, hasta %d niveles' % dif[len(dif) * 9 // 10])
        if FORMATOS[r.fmt] != 'JPEG':
            mal.append('lo toma por %s' % FORMATOS[r.fmt])
        res(not mal, '%s: %s' % (nombre, '; '.join(mal)))
        print('  %s %s (%dx%d)%s' % ('OK  ' if not mal else 'FAIL', nombre, an, al, '' if not mal else ': ' + '; '.join(mal)))


def prueba_no_admitidos(prog, rnd):
    """lo que tiene que decir que no, y cómo: (nombre, datos, error, con texto de variante)"""
    C = []
    bmp, _ = haz_bmp(rnd, 8, 8, 24)
    b = bytearray(bmp)
    b[30] = 4                                   # compresión JPEG
    C.append(('BMP con un JPEG dentro', bytes(b), 2, 1))
    b = bytearray(bmp)
    b[28] = 7                                   # 7 bits por punto
    C.append(('BMP de 7 bits', bytes(b), 2, 1))
    b = bytearray(bmp)
    b[18:22] = struct.pack('<i', 70000)
    C.append(('BMP de 70.000 puntos de ancho', bytes(b), 5, 0))
    C.append(('un texto', b'Esto no es una imagen, es un texto cualquiera.\r\n' * 5, 4, 0))
    C.append(('un archivo vacío', b'', 4, 0))
    C.append(('un octeto', b'B', 4, 0))
    C.append(('un PDF', b'%PDF-1.4\n1 0 obj\n<< /Type /Catalog >>\nendobj\n' + bytes(200), 4, 0))
    C.append(('ceros', bytes(4000), 4, 0))
    png, _ = haz_png(rnd, 8, 8, 2, 8)
    b = bytearray(png)
    b[24] = 3                                   # color de 3 bits por muestra: no existe
    C.append(('PNG con una profundidad que no existe', bytes(b), 1, 0))
    C.append(('PNG sin datos', png[:33] + png_trozo(b'IEND', b''), 1, 0))
    tga, _ = haz_tga(rnd, 8, 8, 3, 8)
    b = bytearray(tga)
    b[16] = 16                                  # grises con opacidad
    C.append(('TGA de grises con opacidad', bytes(b) + bytes(64), 2, 1))
    pcx, _ = haz_pcx(rnd, 8, 8, 'rgb')
    b = bytearray(pcx)
    b[65] = 2                                   # dos planos de 8 bits
    C.append(('PCX de dos planos de 8 bits', bytes(b), 2, 1))
    C.append(('GIF sin ninguna imagen', b'GIF89a' + struct.pack('<HHBBB', 4, 4, 0, 0, 0) + b'\x3b' + bytes(8), 1, 0))
    C.append(('PPM sin medidas', b'P6\n# nada\n', 1, 0))
    R = corre(prog, [(d, 0, 0) for _, d, _, _ in C])
    bien = 0
    for r, (nombre, d, err, var) in zip(R, C):
        if r is None:
            res(False, nombre + ': el programa se ha parado')
            break
        ok = r.err == err and r.var == var
        bien += ok
        res(ok, '%s: error %d (variante %d) y se esperaba %d (%d)' % (nombre, r.err, r.var, err, var))
    print('  %s lo que no se admite, dicho: %d de %d' % ('OK  ' if bien == len(C) else 'FAIL', bien, len(C)))


def prueba_cortados(prog, rnd, L):
    """cada formato, cortado por varios sitios: avisa o da menos filas, y lo que da es el principio"""
    entradas, metas = [], []
    elegidos = {}
    for c in L:
        if c[3] * c[4] >= 600 and c[1] not in ('ICO',) and len(elegidos.setdefault(c[1], [])) < 6:
            elegidos[c[1]].append(c)
    for fmt, cs in elegidos.items():
        for nombre, fmt, d, an, al, e, pil in cs:
            for corte in (len(d) * 9 // 10, len(d) * 2 // 3, len(d) // 2, len(d) - 1):
                entradas.append((d[:corte], 1, 0x112233))
                metas.append((nombre, fmt, an, al, mezcla(e, an, al, 1, 0x112233), corte, len(d)))
    R = corre(prog, entradas)
    bien = n = 0
    for r, (nombre, fmt, an, al, rgb, corte, largo) in zip(R, metas):
        n += 1
        if r is None:
            res(False, '%s cortado en %d de %d: el programa se ha parado' % (nombre, corte, largo))
            break
        mal = []
        if r.mal:
            mal.append('estropea los registros o escribe fuera')
        if r.err == 0:
            if (r.an, r.al) != (an, al):
                mal.append('otras medidas')
            elif r.filas == al and not r.aviso and r.rgb != rgb:
                mal.append('lo da por bueno y entero, y no es la imagen')
            elif r.filas > al:
                mal.append('más filas que alto')
        elif r.err != 1:
            mal.append('error %d' % r.err)
        ok = not mal
        bien += ok
        res(ok, '%s cortado en %d de %d: %s' % (nombre, corte, largo, '; '.join(mal)))
    print('  %s archivos cortados (avisan, y nada de basura dada por buena): %d de %d' % ('OK  ' if bien == n else 'FAIL', bien, n))


def prueba_rotos(prog, rnd, L, cuantos):
    """archivos estropeados al azar: ni un cuelgue ni un acceso fuera de sitio"""
    base = [c for c in L if len(c[2]) < 60000]
    hechos = 0
    por_fmt = {}
    while hechos < cuantos:
        lote, metas = [], []
        for _ in range(min(400, cuantos - hechos)):
            nombre, fmt, d, an, al, e, pil = rnd.choice(base)
            b = bytearray(d)
            modo = rnd.randrange(6)
            if modo == 0:                                   # unos octetos cambiados en cualquier sitio
                for _ in range(rnd.randint(1, 8)):
                    b[rnd.randrange(len(b))] = rnd.getrandbits(8)
            elif modo == 1:                                 # en la cabecera
                for _ in range(rnd.randint(1, 4)):
                    b[rnd.randrange(min(len(b), 64))] = rnd.choice((0, 1, 255, 0x7F, 0x80, rnd.getrandbits(8)))
            elif modo == 2:                                 # cortado
                b = b[:rnd.randrange(len(b))]
            elif modo == 3:                                 # un trozo de ruido
                p = rnd.randrange(len(b))
                n = rnd.randint(1, 64)
                b[p:p + n] = bytes(rnd.getrandbits(8) for _ in range(n))
            elif modo == 4:                                 # un trozo quitado o repetido
                p = rnd.randrange(len(b))
                n = rnd.randint(1, 40)
                if rnd.getrandbits(1):
                    del b[p:p + n]
                else:
                    b[p:p] = b[p:p + n]
            else:                                           # un bit
                p = rnd.randrange(len(b))
                b[p] ^= 1 << rnd.randrange(8)
            lote.append((bytes(b), rnd.getrandbits(1), rnd.getrandbits(24)))
            metas.append((nombre, fmt))
        R = corre(prog, lote, espera=300)
        for i, (r, (nombre, fmt)) in enumerate(zip(R, metas)):
            c = por_fmt.setdefault(fmt, [0, 0])
            c[0] += 1
            if r is None:
                c[1] += 1
                res(False, '%s estropeado: el programa se ha parado o no acaba (caso guardado en /tmp/mirador_roto.bin)' % nombre)
                open('/tmp/mirador_roto.bin', 'wb').write(lote[i][0])
                break
            ok = not r.mal and r.err in (0, 1, 2, 4, 5, 100) and (r.err or r.filas <= r.al)
            if not ok:
                c[1] += 1
            res(ok, '%s estropeado: error %d, registros o centinela %d, %d filas de %d' % (nombre, r.err, r.mal, r.filas, r.al))
        hechos += len(lote)
    for fmt in sorted(por_fmt):
        n, m = por_fmt[fmt]
        print('  %s %s: %d archivos estropeados al azar, sin cuelgues ni accesos fuera de sitio%s' % (
            'OK  ' if not m else 'FAIL', fmt, n, '' if not m else ' (%d mal)' % m))


def main():
    a = sys.argv[1:]
    rotos = int(a[a.index('--rotos') + 1]) if '--rotos' in a else 3000
    semilla = int(a[a.index('--semilla') + 1]) if '--semilla' in a else 20261116
    guardar = a[a.index('--guardar') + 1] if '--guardar' in a else None
    if guardar:
        os.makedirs(guardar, exist_ok=True)
    rnd = random.Random(semilla)
    with tempfile.TemporaryDirectory() as tmp:
        prog = monta(tmp, '--sin-jpeg' not in a)
        L = prueba_buenos(prog, rnd, guardar)
        if '--sin-jpeg' not in a:
            prueba_jpeg(prog, rnd)
        prueba_no_admitidos(prog, rnd)
        prueba_cortados(prog, rnd, L)
        if rotos:
            prueba_rotos(prog, rnd, L, rotos)
    print('  %s formatos de Mirador: %d comprobaciones, %d fallos' % ('OK  ' if not fallos else 'FAIL', total, fallos))
    return 1 if fallos else 0


if __name__ == '__main__':
    sys.exit(main())
