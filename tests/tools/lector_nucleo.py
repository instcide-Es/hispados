#!/usr/bin/env python3
"""El núcleo de Lector (leer un PDF) fuera de Hispa Ventanas.

uso: lector_nucleo.py monta DIR            ensambla el programa de Linux en DIR
     lector_nucleo.py vuelca ARCHIVO.pdf [BUSCAR]   lo que Lector saca del PDF

Toma TAL CUAL programas/comun/desinfla.inc y programas/lector/pdf.inc,
letras.inc, nombres.inc, contenido.inc y texto.inc (todo menos lo que pinta),
los ensambla con NASM dentro de un programa de Linux de 32 bits que abre un
PDF y escribe lo que ha entendido: páginas, tamaño, avisos, la lista de lo que
hay que pintar de cada página (textos con su sitio, rayas, rectángulos...) y
el texto en orden de lectura. tests/run_lector_tests.sh lo usa (módulo
«Nucleo» de aquí) para comparar con lo que tests/tools/hazpdf.py metió.
"""
import os, subprocess, sys, tempfile

RAIZ = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
INCS = [('programas/comun', 'desinfla.inc'), ('programas/lector', 'pdf.inc'), ('programas/lector', 'letras.inc'),
        ('programas/lector', 'nombres.inc'), ('programas/lector', 'contenido.inc'), ('programas/lector', 'texto.inc')]

ARNES = r'''
        bits 32
        cpu 386
ARCH_MAX equ 12 * 1024 * 1024
ALM_TAM  equ 6 * 1024 * 1024
        section .text
        global _start
_start:
        cld
        mov eax, [esp]              ; argc
        cmp eax, 2
        jb fin
        cmp eax, 3
        jb .sin_buscar
        mov eax, [esp+12]
        mov [buscar], eax
.sin_buscar:
        mov ebx, [esp+8]            ; el archivo
        mov eax, 5
        xor ecx, ecx
        int 0x80
        test eax, eax
        js fin
        mov ebx, eax
        mov edi, arch
.lee:
        mov eax, 3
        mov ecx, edi
        mov edx, arch + ARCH_MAX
        sub edx, edi
        jz .leido
        int 0x80
        cmp eax, 0
        jle .leido
        add edi, eax
        jmp .lee
.leido:
        sub edi, arch
        mov [pdf_tam], edi
        mov dword [pdf_dat], arch
        mov dword [ar_base], almacen
        mov eax, [alm_tam]
        add eax, almacen
        mov [ar_fin], eax
        call pdf_abrir
        mov esi, t_abre
        call sal_cad
        call sal_num
        mov eax, [pdf_npag]
        call sal_num
        mov eax, [pdf_ver]
        call sal_num
        mov eax, [pdf_reparado]
        call sal_num
        mov eax, [pdf_muchos]
        call sal_num
        call sal_nl
        cmp dword [pdf_npag], 0
        je fin
        mov edi, k_title
        mov esi, t_titulo
        call dato
        mov edi, k_author
        mov esi, t_autor
        call dato
        xor ebp, ebp                ; EBP=la página
.pag:
        mov eax, ebp
        call pdf_pagina
        mov esi, t_pag
        call sal_cad
        mov eax, ebp
        call sal_num
        mov eax, [pg_an]
        call sal_num
        mov eax, [pg_al]
        call sal_num
        mov eax, [pg_avisos]
        call sal_num
        mov eax, [it_n]
        call sal_num
        call sal_nl
        mov ebx, it_tab
        mov ecx, [it_n]
.it:
        test ecx, ecx
        jz .texto
        push ecx
        movzx eax, byte [ebx+IT_TIPO]
        mov al, [letras_it+eax]
        call sal_car
        mov eax, [ebx+IT_X]
        call sal_num
        mov eax, [ebx+IT_Y]
        call sal_num
        mov eax, [ebx+IT_AN]
        call sal_num
        mov eax, [ebx+IT_AL]
        call sal_num
        movzx eax, byte [ebx+IT_COLOR]
        call sal_num
        movzx eax, byte [ebx+IT_LETRA]
        call sal_num
        movzx eax, byte [ebx+IT_BAND]
        call sal_num
        cmp byte [ebx+IT_TIPO], ITT_LINEA
        jne .i1
        mov eax, [ebx+IT_TXT]
        call sal_num
.i1:
        cmp byte [ebx+IT_TIPO], ITT_POLI
        jne .i2
        movzx ecx, word [ebx+IT_N]  ; cada trozo: cuántos puntos y los puntos
        mov eax, ecx
        call sal_num
        mov esi, [ebx+IT_TXT]
.g:
        lodsd
        mov edx, eax
        call sal_num
        shl edx, 1
.g1:
        lodsd
        call sal_num
        dec edx
        jnz .g1
        dec ecx
        jnz .g
.i2:
        cmp byte [ebx+IT_TIPO], ITT_TEXTO
        jne .i3
        movzx eax, word [ebx+IT_FU]
        call sal_num
        movzx ecx, word [ebx+IT_N]
        mov eax, ecx
        call sal_num
        call sal_nl
        mov esi, [ebx+IT_TXT]
        call sal_n
.i3:
        call sal_nl
        pop ecx
        add ebx, IT_TAM
        dec ecx
        jmp .it
.texto:
        call tx_extrae
        mov esi, t_texto
        call sal_cad
        call sal_num
        call sal_nl
        mov esi, tx_buf
        mov ecx, eax
        call sal_n
        call sal_nl
        cmp dword [buscar], 0
        je .sig
        xor eax, eax
.busca:
        mov esi, [buscar]
        call tx_busca
        cmp eax, -1
        je .sig
        push eax
        mov esi, t_halla
        call sal_cad
        call sal_num
        mov eax, ecx
        call sal_num
        pop eax
        push eax
        movzx eax, word [tx_mapa+eax*2]
        call sal_num
        call sal_nl
        pop eax
        inc eax
        jmp .busca
.sig:
        inc ebp
        cmp ebp, [pdf_npag]
        jb .pag
fin:
        call sal_vacia
        mov eax, 1
        xor ebx, ebx
        int 0x80

dato:
        push esi
        mov ebx, cad
        mov ecx, 200
        call pdf_dato
        pop esi
        test eax, eax
        jz .x
        push eax
        call sal_cad
        pop ecx
        mov esi, cad
        call sal_n
        call sal_nl
.x:
        ret

sal_car:
        push edi
        mov edi, [sal_p]
        mov [sal_buf+edi], al
        inc edi
        mov [sal_p], edi
        pop edi
        cmp dword [sal_p], 60000
        jb .x
        call sal_vacia
.x:
        ret
sal_vacia:
        pushad
        mov eax, 4
        mov ebx, 1
        mov ecx, sal_buf
        mov edx, [sal_p]
        int 0x80
        mov dword [sal_p], 0
        popad
        ret
sal_nl:
        push eax
        mov al, 10
        call sal_car
        pop eax
        ret
sal_cad:
        push eax
.b:
        lodsb
        test al, al
        jz .x
        call sal_car
        jmp .b
.x:
        pop eax
        ret
; ESI=datos ECX=cuántos
sal_n:
        push eax
.b:
        test ecx, ecx
        jz .x
        lodsb
        call sal_car
        dec ecx
        jmp .b
.x:
        pop eax
        ret
; un espacio y EAX en decimal, con signo
sal_num:
        pushad
        push eax
        mov al, ' '
        call sal_car
        pop eax
        test eax, eax
        jns .p
        push eax
        mov al, '-'
        call sal_car
        pop eax
        neg eax
.p:
        xor ecx, ecx
        mov ebx, 10
.d:
        xor edx, edx
        div ebx
        push edx
        inc ecx
        test eax, eax
        jnz .d
.e:
        pop eax
        add al, '0'
        call sal_car
        dec ecx
        jnz .e
        popad
        ret

t_abre      db "ABRE", 0
t_pag       db "PAG", 0
t_texto     db "TEXTO", 0
t_halla     db "HALLA", 0
t_titulo    db "TITULO ", 0
t_autor     db "AUTOR ", 0
letras_it   db "?TRLIXG"
alm_tam     dd ALM_TAM

%include "desinfla.inc"
%include "pdf.inc"
%include "letras.inc"
%include "nombres.inc"
%include "contenido.inc"
%include "texto.inc"

        section .bss
buscar  resd 1
sal_p   resd 1
cad     resb 256
sal_buf resb 65536
        DESINFLA_BSS
        PDF_BSS
        LETRAS_BSS
        CONTENIDO_BSS
        TEXTO_BSS
arch    resb ARCH_MAX
almacen resb ALM_TAM
'''


def monta(tmp):
    """Ensambla el programa en la carpeta tmp y devuelve su ruta."""
    for carpeta, inc in INCS:
        t = open(os.path.join(RAIZ, carpeta, inc), encoding='utf-8').read()
        open(os.path.join(tmp, inc), 'w', encoding='cp437', errors='replace').write(t)
    asm = os.path.join(tmp, 'nucleo.asm')
    open(asm, 'w').write(ARNES)
    subprocess.run([os.environ.get('NASM', 'nasm'), '-f', 'elf32', '-i', tmp + os.sep, '-o', asm + '.o', asm], check=True)
    prog = os.path.join(tmp, 'nucleo')
    subprocess.run(['ld', '-m', 'elf_i386', '-o', prog, asm + '.o'], check=True)
    return prog


class Pagina:
    def __init__(self, n, an, al, avisos):
        self.n, self.an, self.al, self.avisos = n, an, al, avisos
        self.cosas = []         # (clase, x, y, an, al, color, letra, band, ...)
        self.texto = ''
        self.halla = []

    def textos(self):
        return [c for c in self.cosas if c[0] == 'T']

    def de(self, clase):
        return [c for c in self.cosas if c[0] == clase]


class Nucleo:
    """Lo que dice el programa de un PDF. Los números de sitio, en puntos."""

    def __init__(self, prog, pdf, buscar=None, tiempo=60):
        self.err, self.npag, self.ver, self.reparado, self.muchos = -1, 0, 0, 0, 0
        self.titulo = self.autor = ''
        self.pags = []
        self.colgado = False
        arg = [prog, pdf] + ([buscar.encode('cp437')] if buscar else [])
        try:
            r = subprocess.run(arg, stdout=subprocess.PIPE, timeout=tiempo)
        except subprocess.TimeoutExpired:
            self.colgado = True
            self.codigo = -999
            return
        self.codigo = r.returncode
        d, p = r.stdout, 0

        def linea():
            nonlocal p
            f = d.index(b'\n', p)
            l = d[p:f]
            p = f + 1
            return l
        try:
            while p < len(d):
                l = linea()
                c = l.split()
                if not c:
                    continue
                if c[0] == b'ABRE':
                    self.err, self.npag, self.ver, self.reparado, self.muchos = (int(v) for v in c[1:6])
                elif c[0] == b'TITULO':
                    self.titulo = l[7:].decode('cp437')
                elif c[0] == b'AUTOR':
                    self.autor = l[6:].decode('cp437')
                elif c[0] == b'PAG':
                    self.pags.append(Pagina(int(c[1]), int(c[2]) / 65536, int(c[3]) / 65536, int(c[4])))
                elif c[0] == b'TEXTO':
                    n = int(c[1])
                    self.pags[-1].texto = d[p:p + n].decode('cp437')
                    p += n + 1
                elif c[0] == b'HALLA':
                    self.pags[-1].halla.append((int(c[1]), int(c[2]), int(c[3])))
                elif c[0] in (b'T', b'R', b'L', b'I', b'X', b'G'):
                    v = [int(x) for x in c[1:]]
                    cosa = [c[0].decode()] + [x / 65536 for x in v[:4]] + v[4:7]
                    if c[0] == b'L':
                        cosa.append(v[7] / 65536)
                    elif c[0] == b'G':
                        trozos, k = [], 8           # cada trozo: sus vértices (x, y)
                        for _ in range(v[7]):
                            np = v[k]
                            trozos.append([(v[k + 1 + 2 * j] / 65536, v[k + 2 + 2 * j] / 65536) for j in range(np)])
                            k += 1 + 2 * np
                        cosa.append(trozos)
                    elif c[0] == b'T':
                        n = v[8]
                        cosa.append(d[p:p + n].decode('cp437'))
                        cosa.append(v[7])
                        p += n + 1
                    self.pags[-1].cosas.append(tuple(cosa))
        except (ValueError, IndexError):
            self.codigo = -998          # la salida se corta: el programa ha caído


def main():
    if len(sys.argv) >= 3 and sys.argv[1] == 'monta':
        os.makedirs(sys.argv[2], exist_ok=True)
        print(monta(sys.argv[2]))
        return
    if len(sys.argv) >= 3 and sys.argv[1] == 'vuelca':
        with tempfile.TemporaryDirectory() as tmp:
            n = Nucleo(monta(tmp), sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else None)
        print('error %d, %d páginas, versión %d.%d, reparado %d, título «%s», autor «%s», salida %d' % (
            n.err, n.npag, n.ver >> 4, n.ver & 15, n.reparado, n.titulo, n.autor, n.codigo))
        for pg in n.pags:
            print('--- página %d: %.1f x %.1f, avisos %d' % (pg.n + 1, pg.an, pg.al, pg.avisos))
            for c in pg.cosas:
                print('   ', c)
            print(pg.texto)
            for h in pg.halla:
                print('    hallado en', h)
        return
    sys.exit(__doc__)


if __name__ == '__main__':
    main()
