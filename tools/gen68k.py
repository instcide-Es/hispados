#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""Genera las instrucciones del 68000 de Hispa-Drive (programas/hispadrv/).

uso: gen68k.py SALIDA.inc

Para cada uno de los 65.536 códigos se mira qué instrucción es (con su
tamaño y sus modos de dirección); a cada forma distinta le toca una rutina
en NASM (los números de registro los saca la propia rutina del código, que
está en EBX). Sale:
  - las rutinas (m_r0, m_r1...), con sus ciclos (los del manual del 68000);
  - m_rutinas: la lista de sus direcciones;
  - m_rle: la tabla de saltos comprimida, parejas (cuántos códigos
    seguidos, número de rutina), acabada en 0; m68k_tablas la desempaqueta.
Las convenciones (registros del x86, banderas, memoria) son las de
programas/hispadrv/m68k.inc. Todo propio, desde el manual del procesador.
"""
import sys

TAM = {'b': 1, 'w': 2, 'l': 4}
REG = {'a': {'b': 'al', 'w': 'ax', 'l': 'eax'},
       'c': {'b': 'cl', 'w': 'cx', 'l': 'ecx'},
       'd': {'b': 'dl', 'w': 'dx', 'l': 'edx'}}
MEM = {'b': 'byte', 'w': 'word', 'l': 'dword'}
BITS = {'b': 8, 'w': 16, 'l': 32}
MODOS = ['Dn', 'An', 'ind', 'post', 'pre', 'd16', 'd8x', 'absw', 'absl', 'pcd16', 'pcd8x', 'imm']
# los ciclos de calcular la dirección y leer: (b/w, l)
EA_CIC = {'Dn': (0, 0), 'An': (0, 0), 'ind': (4, 8), 'post': (4, 8), 'post7': (4, 8),
          'pre': (6, 10), 'pre7': (6, 10), 'd16': (8, 12), 'd8x': (10, 14), 'absw': (8, 12),
          'absl': (12, 16), 'pcd16': (8, 12), 'pcd8x': (10, 14), 'imm': (4, 8)}
CONDS = ['t', 'f', 'hi', 'ls', 'cc', 'cs', 'ne', 'eq', 'vc', 'vs', 'pl', 'mi', 'ge', 'lt', 'gt', 'le']
# condición -> salto del x86 si se cumple (tras COND)
JCC = {'hi': 'ja', 'ls': 'jbe', 'cc': 'jnc', 'cs': 'jc', 'ne': 'jnz', 'eq': 'jz', 'vc': 'jno',
       'vs': 'jo', 'pl': 'jns', 'mi': 'js', 'ge': 'jge', 'lt': 'jl', 'gt': 'jg', 'le': 'jle'}
NEG = {'ja': 'jbe', 'jbe': 'ja', 'jnc': 'jc', 'jc': 'jnc', 'jnz': 'jz', 'jz': 'jnz', 'jno': 'jo',
       'jo': 'jno', 'jns': 'js', 'js': 'jns', 'jge': 'jl', 'jl': 'jge', 'jg': 'jle', 'jle': 'jg'}


def cic(modo, t):
    return EA_CIC[modo][1 if t == 'l' else 0]


def ea_modo(op_modo, reg, t=None):
    """el modo de dirección de los 6 bits (modo, registro); None si no vale"""
    if op_modo < 7:
        m = MODOS[op_modo]
        if m in ('post', 'pre') and reg == 7 and t == 'b':
            m += '7'
        return m
    return {0: 'absw', 1: 'absl', 2: 'pcd16', 3: 'pcd8x', 4: 'imm'}.get(reg)


DATOS = set(MODOS) - {'An'} | {'post7', 'pre7'}
MEMORIA = DATOS - {'Dn'}
ALTERABLE = set(MODOS) - {'pcd16', 'pcd8x', 'imm'} | {'post7', 'pre7'}
DAT_ALT = DATOS & ALTERABLE
MEM_ALT = MEMORIA & ALTERABLE
CONTROL = {'ind', 'd16', 'd8x', 'absw', 'absl', 'pcd16', 'pcd8x'}
CTL_ALT = CONTROL & ALTERABLE
TODOS = set(MODOS) | {'post7', 'pre7'}


_n = [0]


def nueva():
    _n[0] += 1
    return '.e%d' % _n[0]


def idx(pos):
    """EBP = el número de registro del campo (bits 0-2 o 9-11)"""
    if pos == 0:
        return ['mov ebp, ebx', 'and ebp, 7']
    return ['mov ebp, ebx', 'shr ebp, 9', 'and ebp, 7']


def ea_dir(modo, pos, t, mascara=True):
    """calcula la dirección en EDX (24 bits; con mascara=False, los 32: LEA y
    PEA); gasta EBP. (An)+ y -(An) cambian An"""
    c = ea_dir32(modo, pos, t)
    if mascara and c[-1] != 'and edx, 0xFFFFFF':
        c.append('and edx, 0xFFFFFF')
    if not mascara and c[-1] == 'and edx, 0xFFFFFF':
        c.pop()
    return c


def ea_dir32(modo, pos, t):
    paso = {'b': 1, 'w': 2, 'l': 4}[t] if t else 0
    if modo == 'ind':
        return idx(pos) + ['mov edx, [m_a+ebp*4]', 'and edx, 0xFFFFFF']
    if modo in ('post', 'post7'):
        p = 2 if modo == 'post7' else paso
        return idx(pos) + ['mov edx, [m_a+ebp*4]', 'add dword [m_a+ebp*4], %d' % p,
                           'and edx, 0xFFFFFF']
    if modo in ('pre', 'pre7'):
        p = 2 if modo == 'pre7' else paso
        return idx(pos) + ['sub dword [m_a+ebp*4], %d' % p, 'mov edx, [m_a+ebp*4]',
                           'and edx, 0xFFFFFF']
    if modo == 'd16':
        return idx(pos) + ['movsx edx, word [esi]', 'add esi, 2', 'add edx, [m_a+ebp*4]',
                           'and edx, 0xFFFFFF']
    if modo in ('d8x', 'pcd8x'):
        e1, e2 = nueva(), nueva()
        c = ['movzx ebp, word [esi]', 'mov edx, ebp', 'shr edx, 12',
             'test ebp, 0x800', 'jnz short ' + e1, 'movsx edx, word [m_d+edx*4]', 'jmp short ' + e2,
             e1 + ':', 'mov edx, [m_d+edx*4]', e2 + ':', 'shl ebp, 24', 'sar ebp, 24',
             'add edx, ebp']
        if modo == 'd8x':
            c += idx(pos) + ['add edx, [m_a+ebp*4]']
        else:
            c += ['add edx, esi', 'sub edx, [m_pcbase]']
        return c + ['add esi, 2', 'and edx, 0xFFFFFF']
    if modo == 'absw':
        return ['movsx edx, word [esi]', 'add esi, 2', 'and edx, 0xFFFFFF']
    if modo == 'absl':
        return ['mov edx, [esi]', 'rol edx, 16', 'add esi, 4', 'and edx, 0xFFFFFF']
    if modo == 'pcd16':
        return ['mov ebp, esi', 'sub ebp, [m_pcbase]', 'movsx edx, word [esi]', 'add edx, ebp',
                'add esi, 2', 'and edx, 0xFFFFFF']
    raise ValueError(modo)


def lee(t):
    return ['LEE%d' % BITS[t]]


def esc(t):
    return ['ESC%d' % BITS[t]]


def ea_lee(modo, pos, t):
    """el operando en EAX (para la memoria, EDX = su dirección)"""
    if modo == 'Dn':
        return idx(pos) + ['mov eax, [m_d+ebp*4]']
    if modo == 'An':
        return idx(pos) + ['mov eax, [m_a+ebp*4]']
    if modo == 'imm':
        if t == 'b':
            return ['movzx eax, byte [esi]', 'add esi, 2']
        if t == 'w':
            return ['movzx eax, word [esi]', 'add esi, 2']
        return ['mov eax, [esi]', 'rol eax, 16', 'add esi, 4']
    return ea_dir(modo, pos, t) + lee(t)


def ea_esc(modo, pos, t):
    """EAX -> el operando (Dn o memoria, calculando la dirección)"""
    if modo == 'Dn':
        return idx(pos) + ['mov [m_d+ebp*4], %s' % REG['a'][t]]
    return ea_dir(modo, pos, t) + esc(t)


class Rutinas:
    def __init__(self):
        self.lista = []
        self.por_clave = {}

    def una(self, clave, fn):
        if clave not in self.por_clave:
            self.por_clave[clave] = len(self.lista)
            self.lista.append((clave, fn(*clave[1:]) if callable(fn) else fn))
        return self.por_clave[clave]


def fin(n):
    return ['MFIN %d' % n]


# ---------------------------------------------------------------------------
#  Las instrucciones: cada una, una función que devuelve las líneas
# ---------------------------------------------------------------------------
def g_move(t, src, dst):
    c = ea_lee(src, 0, t)
    n = 4 + cic(src, t)
    if dst == 'Dn':
        c += idx(9) + ['mov [m_d+ebp*4], %s' % REG['a'][t], 'test %s, %s' % (REG['a'][t], REG['a'][t]), 'FL_LOG']
    else:
        c += ['mov ecx, eax'] + ea_dir(dst, 9, t)
        c += ['test %s, %s' % (REG['c'][t], REG['c'][t]), 'FL_LOG', 'mov eax, ecx'] + esc(t)
        if t == 'l':
            dm = {'ind': 8, 'post': 8, 'pre': 8, 'd16': 12, 'd8x': 14, 'absw': 12, 'absl': 16}[dst]
        else:
            dm = {'ind': 4, 'post': 4, 'post7': 4, 'pre': 4, 'pre7': 4, 'd16': 8, 'd8x': 10, 'absw': 8,
                  'absl': 12}[dst]
        n += dm
    return c + fin(n)


def g_movea(t, src):
    c = ea_lee(src, 0, t)
    if t == 'w':
        c += ['cwde']
    c += idx(9) + ['mov [m_a+ebp*4], eax']
    return c + fin(4 + cic(src, t))


def g_alu_ea_dn(op, t, src):
    """OP <ea>,Dn: add sub and or cmp"""
    c = ea_lee(src, 0, t) + idx(9)
    x86 = {'add': 'add', 'sub': 'sub', 'and': 'and', 'or': 'or', 'cmp': 'cmp'}[op]
    c += ['%s %s [m_d+ebp*4], %s' % (x86, MEM[t], REG['a'][t])]
    c += [{'add': 'FL_ARI', 'sub': 'FL_ARI', 'cmp': 'FL_CMP'}.get(op, 'FL_LOG')]
    n = 4 + cic(src, t)
    if t == 'l':
        n += 4 if src in ('Dn', 'An', 'imm') else 2
        if op == 'cmp':
            n = 6 + cic(src, t)
    return c + fin(n)


def g_alu_dn_ea(op, t, dst):
    """OP Dn,<ea> (memoria): add sub and or eor"""
    c = ea_dir(dst, 0, t) + lee(t) + idx(9)
    if op == 'sub':
        c += ['mov ecx, eax', 'sub %s, [m_d+ebp*4]' % REG['c'][t], 'FL_ARI']
    else:
        x86 = {'add': 'add', 'and': 'and', 'or': 'or', 'eor': 'xor'}[op]
        c += ['mov ecx, [m_d+ebp*4]', '%s %s, %s' % (x86, REG['c'][t], REG['a'][t])]
        c += ['FL_ARI' if op == 'add' else 'FL_LOG']
    c += ['mov eax, ecx'] + esc(t)
    return c + fin((8 if t != 'l' else 12) + cic(dst, t))


def g_eor_dn(t):
    c = idx(9) + ['mov eax, [m_d+ebp*4]'] + idx(0)
    c += ['xor %s [m_d+ebp*4], %s' % (MEM[t], REG['a'][t]), 'FL_LOG']
    return c + fin(4 if t != 'l' else 8)


def g_adda(op, t, src):
    c = ea_lee(src, 0, t)
    if t == 'w':
        c += ['cwde']
    c += idx(9)
    if op == 'cmp':
        c += ['cmp [m_a+ebp*4], eax', 'FL_CMP']
        return c + fin(6 + cic(src, t))
    c += ['%s [m_a+ebp*4], eax' % op]
    n = 8 + cic(src, t)
    if t == 'l' and src not in ('Dn', 'An', 'imm'):
        n = 6 + cic(src, t)
    return c + fin(n)


def g_imm(op, t, dst):
    """ORI ANDI SUBI ADDI EORI CMPI #,<ea>"""
    c = ea_lee('imm', 0, t) + ['mov ecx, eax']
    x86 = {'ori': 'or', 'andi': 'and', 'subi': 'sub', 'addi': 'add', 'eori': 'xor', 'cmpi': 'cmp'}[op]
    fl = {'subi': 'FL_ARI', 'addi': 'FL_ARI', 'cmpi': 'FL_CMP'}.get(op, 'FL_LOG')
    if dst == 'Dn':
        c += idx(0) + ['%s %s [m_d+ebp*4], %s' % (x86, MEM[t], REG['c'][t]), fl]
        n = 8 if t != 'l' else (14 if op == 'cmpi' else 16)
        return c + fin(n)
    c += ea_dir(dst, 0, t) + lee(t)
    c += ['%s %s, %s' % (x86, REG['a'][t], REG['c'][t])]
    if op == 'cmpi':
        c += [fl]
        return c + fin((8 if t != 'l' else 12) + cic(dst, t))
    c += ['mov ecx, eax', fl, 'mov eax, ecx'] + esc(t)
    return c + fin((12 if t != 'l' else 20) + cic(dst, t))


def g_imm_ccr(op, sr):
    """ORI/ANDI/EORI a CCR o a SR"""
    x86 = {'ori': 'or', 'andi': 'and', 'eori': 'xor'}[op]
    c = []
    if sr:
        c += ['test byte [m_srh], 0x20', 'jz near m_ex_privilegio']
    c += ['call m_sr_lee', 'movzx ecx, word [esi]', 'add esi, 2']
    if sr:
        c += ['%s ax, cx' % x86, 'call m_sr_pon']
    else:
        c += ['%s al, cl' % x86, 'call m_ccr_pon']
    return c + fin(20)


def g_q(op, t, dst):
    """ADDQ/SUBQ #n,<ea>"""
    c = ['mov ecx, ebx', 'shr ecx, 9', 'and ecx, 7', 'movzx ecx, byte [m_q8+ecx]']
    if dst == 'An':
        c += idx(0) + ['%s [m_a+ebp*4], ecx' % op]
        return c + fin(8 if t == 'w' else 6)
    if dst == 'Dn':
        c += idx(0) + ['%s %s [m_d+ebp*4], %s' % (op, MEM[t], REG['c'][t]), 'FL_ARI']
        return c + fin(4 if t != 'l' else 8)
    c += ea_dir(dst, 0, t) + lee(t)
    c += ['%s %s, %s' % (op, REG['a'][t], REG['c'][t]), 'mov ecx, eax', 'FL_ARI', 'mov eax, ecx'] + esc(t)
    return c + fin((8 if t != 'l' else 12) + cic(dst, t))


def g_moveq():
    return ['movsx eax, bl'] + idx(9) + ['mov [m_d+ebp*4], eax', 'test eax, eax', 'FL_LOG'] + fin(4)


def g_unaria(op, t, dst):
    """NEG NEGX NOT CLR TST"""
    r = REG['a'][t]
    if dst == 'Dn':
        c = idx(0)
        m = '%s [m_d+ebp*4]' % MEM[t]
        if op == 'neg':
            c += ['neg %s' % m, 'FL_ARI']
        elif op == 'not':
            c += ['not %s' % m, 'test %s, %s' % (m, {'b': '0xFF', 'w': '0xFFFF', 'l': '0xFFFFFFFF'}[t]),
                  'FL_LOG']
        elif op == 'clr':
            c += ['mov %s, 0' % m, 'mov word [m_vf], 0x4000']
        elif op == 'tst':
            c += ['cmp %s, 0' % m, 'FL_LOG']
        elif op == 'negx':
            c += ['mov ecx, [m_d+ebp*4]', 'mov al, 0', 'sub al, [m_x]', 'mov eax, 0',
                  'sbb %s, %s' % (r, REG['c'][t]), 'mov [m_d+ebp*4], %s' % r, 'call m_fl_x']
        n = 4 if t != 'l' else 6
        if op == 'tst':
            n = 4
        return c + fin(n)
    if op == 'tst':
        return ea_lee(dst, 0, t) + ['test %s, %s' % (r, r), 'FL_LOG'] + fin(4 + cic(dst, t))
    c = ea_dir(dst, 0, t)
    if op == 'clr':
        c += lee(t) + ['xor eax, eax'] + esc(t) + ['mov word [m_vf], 0x4000']
    else:
        c += lee(t)
        if op == 'neg':
            c += ['neg %s' % r, 'mov ecx, eax', 'FL_ARI', 'mov eax, ecx']
        elif op == 'not':
            c += ['not %s' % r, 'test %s, %s' % (r, r), 'mov ecx, eax', 'FL_LOG', 'mov eax, ecx']
        elif op == 'negx':
            c += ['mov ecx, eax', 'mov al, 0', 'sub al, [m_x]', 'mov eax, 0',
                  'sbb %s, %s' % (r, REG['c'][t]), 'call m_fl_x']
        c += esc(t)
    return c + fin((8 if t != 'l' else 12) + cic(dst, t))


def g_bcc(cc, largo):
    """Bcc, BRA, BSR (corto: el desplazamiento en BL; largo: en la palabra de después)"""
    if cc == 'bsr':
        c = []
        if largo:
            c += ['movsx ecx, word [esi]', 'lea eax, [esi+2]']
        else:
            c += ['movsx ecx, bl', 'mov eax, esi']
        c += ['sub eax, [m_pcbase]', 'sub dword [m_a+7*4], 4', 'mov edx, [m_a+7*4]', 'and edx, 0xFFFFFF',
              'ESC32', 'add esi, ecx']
        return c + fin(18)
    if cc == 't':
        if largo:
            return ['movsx eax, word [esi]', 'add esi, eax'] + fin(10)
        return ['movsx eax, bl', 'add esi, eax'] + fin(10)
    j = JCC[cc]
    c = ['COND', '%s short .no' % NEG[j]]
    if largo:
        c += ['movsx eax, word [esi]', 'add esi, eax'] + fin(10) + ['.no:', 'add esi, 2'] + fin(12)
    else:
        c += ['movsx eax, bl', 'add esi, eax'] + fin(10) + ['.no:'] + fin(8)
    return c


def g_dbcc(cc):
    c = []
    if cc != 'f':
        if cc == 't':
            return ['add esi, 2'] + fin(12)
        c += ['COND', '%s near .si' % JCC[cc]]
    c += idx(0) + ['sub word [m_d+ebp*4], 1', 'jc short .fin',
                   'movsx eax, word [esi]', 'add esi, eax'] + fin(10)
    c += ['.fin:', 'add esi, 2'] + fin(14)
    if cc != 'f':
        c += ['.si:', 'add esi, 2'] + fin(12)
    return c


def g_scc(cc, dst):
    c = []
    if cc == 't':
        c += ['mov eax, 0xFF']
    elif cc == 'f':
        c += ['xor eax, eax']
    else:
        c += ['COND', 'set%s al' % JCC[cc][1:], 'neg al']
    if dst == 'Dn':
        c += idx(0) + ['mov [m_d+ebp*4], al']
        # 4 si es falsa, 6 si se cumple
        return c + ['and eax, 2', 'sub edi, eax'] + fin(4)
    c += ['mov ecx, eax'] + ea_dir(dst, 0, 'b') + ['LEE8', 'mov eax, ecx', 'ESC8']
    return c + fin(8 + cic(dst, 'b'))


def g_lea(src):
    c = ea_dir(src, 0, None, False) + idx(9) + ['mov [m_a+ebp*4], edx']
    return c + fin({'ind': 4, 'd16': 8, 'd8x': 12, 'absw': 8, 'absl': 12, 'pcd16': 8, 'pcd8x': 12}[src])


def g_pea(src):
    c = ea_dir(src, 0, None, False) + ['mov eax, edx', 'sub dword [m_a+7*4], 4', 'mov edx, [m_a+7*4]',
                                'and edx, 0xFFFFFF', 'ESC32']
    return c + fin({'ind': 12, 'd16': 16, 'd8x': 20, 'absw': 16, 'absl': 20, 'pcd16': 16, 'pcd8x': 20}[src])


def g_jmp(jsr, src):
    c = ea_dir(src, 0, None)
    if jsr:
        c += ['mov ecx, edx', 'mov eax, esi', 'sub eax, [m_pcbase]', 'sub dword [m_a+7*4], 4',
              'mov edx, [m_a+7*4]', 'and edx, 0xFFFFFF', 'ESC32', 'mov edx, ecx']
    c += ['call m_salta']
    n = {'ind': 8, 'd16': 10, 'd8x': 14, 'absw': 10, 'absl': 12, 'pcd16': 10, 'pcd8x': 14}[src]
    return c + fin(n + (8 if jsr else 0))


def g_simple(nombre):
    if nombre == 'nop':
        return fin(4)
    if nombre == 'rts':
        return ['mov edx, [m_a+7*4]', 'add dword [m_a+7*4], 4', 'and edx, 0xFFFFFF', 'LEE32',
                'mov edx, eax', 'and edx, 0xFFFFFF', 'call m_salta'] + fin(16)
    if nombre == 'rtr':
        return ['mov edx, [m_a+7*4]', 'add dword [m_a+7*4], 6', 'and edx, 0xFFFFFF', 'LEE16',
                'call m_ccr_pon', 'add edx, 2', 'LEE32',
                'mov edx, eax', 'and edx, 0xFFFFFF', 'call m_salta'] + fin(20)
    if nombre == 'rte':
        return ['test byte [m_srh], 0x20', 'jz near m_ex_privilegio',
                'mov edx, [m_a+7*4]', 'add dword [m_a+7*4], 6', 'and edx, 0xFFFFFF', 'LEE16',
                'mov ecx, eax', 'add edx, 2', 'and edx, 0xFFFFFF', 'LEE32', 'push eax', 'mov eax, ecx',
                'call m_sr_pon', 'pop edx', 'and edx, 0xFFFFFF', 'call m_salta'] + fin(20)
    if nombre == 'trapv':
        return ['cmp byte [m_v], 0', 'jne near m_ex_trapv'] + fin(4)
    if nombre == 'reset':
        return ['test byte [m_srh], 0x20', 'jz near m_ex_privilegio'] + fin(132)
    if nombre == 'stop':
        return ['test byte [m_srh], 0x20', 'jz near m_ex_privilegio',
                'movzx eax, word [esi]', 'add esi, 2', 'call m_sr_pon', 'mov byte [m_parado], 1',
                'call m_pide_mirar'] + fin(4)
    if nombre == 'ilegal':
        return ['jmp m_ex_ilegal']
    if nombre == 'lineaa':
        return ['jmp m_ex_lineaa']
    if nombre == 'lineaf':
        return ['jmp m_ex_lineaf']
    raise ValueError(nombre)


def g_trap():
    return ['mov eax, ebx', 'and eax, 15', 'add eax, 32', 'call m_excepcion'] + fin(34)


def g_link():
    c = idx(0) + ['mov eax, [m_a+ebp*4]', 'sub dword [m_a+7*4], 4', 'mov edx, [m_a+7*4]',
                  'cmp ebp, 7', 'jne short .n', 'mov eax, edx', '.n:',
                  'and edx, 0xFFFFFF', 'ESC32'] + idx(0)
    c += ['mov eax, [m_a+7*4]', 'mov [m_a+ebp*4], eax', 'movsx eax, word [esi]', 'add esi, 2',
          'add [m_a+7*4], eax']
    return c + fin(16)


def g_unlk():
    c = idx(0) + ['mov edx, [m_a+ebp*4]', 'lea eax, [edx+4]', 'mov [m_a+7*4], eax', 'and edx, 0xFFFFFF',
                  'LEE32'] + idx(0) + ['mov [m_a+ebp*4], eax']
    return c + fin(12)


def g_usp(a_usp):
    c = ['test byte [m_srh], 0x20', 'jz near m_ex_privilegio'] + idx(0)
    if a_usp:
        c += ['mov eax, [m_a+ebp*4]', 'mov [m_osp], eax']
    else:
        c += ['mov eax, [m_osp]', 'mov [m_a+ebp*4], eax']
    return c + fin(4)


def g_swap():
    return idx(0) + ['mov eax, [m_d+ebp*4]', 'rol eax, 16', 'mov [m_d+ebp*4], eax', 'test eax, eax',
                     'FL_LOG'] + fin(4)


def g_ext(t):
    c = idx(0) + ['mov eax, [m_d+ebp*4]']
    if t == 'w':
        c += ['movsx ax, al', 'mov [m_d+ebp*4], ax', 'test ax, ax']
    else:
        c += ['cwde', 'mov [m_d+ebp*4], eax', 'test eax, eax']
    return c + ['FL_LOG'] + fin(4)


def g_exg(tipo):
    a = {'dd': ('m_d', 'm_d'), 'aa': ('m_a', 'm_a'), 'da': ('m_d', 'm_a')}[tipo]
    c = idx(9) + ['mov eax, [%s+ebp*4]' % a[0], 'mov edx, ebp'] + idx(0)
    c += ['xchg eax, [%s+ebp*4]' % a[1], 'mov [%s+edx*4], eax' % a[0]]
    return c + fin(6)


def g_move_sr(dir_, ea):
    """dir_: 'de' (MOVE SR,<ea>), 'a_ccr', 'a_sr'"""
    if dir_ == 'de':
        if ea == 'Dn':
            return ['call m_sr_lee', 'mov ecx, eax'] + idx(0) + ['mov [m_d+ebp*4], cx'] + fin(6)
        return ea_dir(ea, 0, 'w') + ['LEE16', 'call m_sr_lee', 'ESC16'] + fin(8 + cic(ea, 'w'))
    c = []
    if dir_ == 'a_sr':
        c += ['test byte [m_srh], 0x20', 'jz near m_ex_privilegio']
    c += ea_lee(ea, 0, 'w')
    c += ['call m_sr_pon' if dir_ == 'a_sr' else 'call m_ccr_pon']
    return c + fin(12 + cic(ea, 'w'))


def g_mul(signo, src):
    """MULU/MULS: 38 ciclos y 2 por cada 1 de la fuente (MULS: por cada
    cambio de 0 a 1 o de 1 a 0, mirando también el bit de su derecha, 0)"""
    c = ea_lee(src, 0, 'w')
    c += ['movsx eax, ax' if signo else 'movzx eax, ax']
    c += ['mov ecx, eax']
    if signo:
        c += ['add ecx, ecx', 'xor ecx, eax']
    c += ['and ecx, 0xFFFF', 'movzx edx, cl', 'movzx edx, byte [m_unos+edx]', 'sub edi, edx', 'sub edi, edx',
          'shr ecx, 8', 'movzx edx, byte [m_unos+ecx]', 'sub edi, edx', 'sub edi, edx']
    c += idx(9)
    c += ['movsx ecx, word [m_d+ebp*4]' if signo else 'movzx ecx, word [m_d+ebp*4]']
    c += ['imul eax, ecx', 'mov [m_d+ebp*4], eax', 'test eax, eax', 'FL_LOG']
    return c + fin(38 + cic(src, 'w'))


def g_div(signo, src):
    """DIVU/DIVS (los ciclos los cuenta la rutina, como el 68000)"""
    c = ea_lee(src, 0, 'w')
    c += ['movsx ecx, ax' if signo else 'movzx ecx, ax', 'test ecx, ecx', 'jz near m_ex_cero_%d' % cic(src, 'w')]
    c += idx(9) + ['mov eax, [m_d+ebp*4]', 'push ebp', 'call %s' % ('m_divs' if signo else 'm_divu'),
                   'pop ebp', 'mov [m_d+ebp*4], eax']
    return c + fin(cic(src, 'w'))


def g_chk(src):
    """CHK: si Dn<0 o Dn>límite, la excepción. Banderas (las del 68000 de
    verdad): Z=V=C=0; N=1 si Dn<0, N=0 si es mayor que el límite; si no salta,
    N queda como estaba"""
    c = ea_lee(src, 0, 'w') + idx(9) + ['mov cx, [m_d+ebp*4]']
    c += ['and byte [m_f], 0x80', 'mov byte [m_v], 0',
          'test cx, cx', 'js short .neg', 'cmp cx, ax', 'jg short .mayor']
    c += fin(10 + cic(src, 'w'))
    c += ['.neg:', 'mov byte [m_f], 0x80', 'sub edi, %d' % cic(src, 'w'), 'jmp m_ex_chk']
    c += ['.mayor:', 'mov byte [m_f], 0', 'sub edi, %d' % cic(src, 'w'), 'jmp m_ex_chk']
    return c


def g_addx(op, t, mem):
    """ADDX/SUBX: Dy,Dx o -(Ay),-(Ax)"""
    r = REG['a'][t]
    rc = REG['c'][t]
    if not mem:
        c = idx(0) + ['mov ecx, [m_d+ebp*4]'] + idx(9)
        c += ['mov al, 0', 'sub al, [m_x]']  # CF = X
        c += ['%s %s [m_d+ebp*4], %s' % ('adc' if op == 'add' else 'sbb', MEM[t], rc), 'call m_fl_x']
        return c + fin(4 if t != 'l' else 8)
    p = TAM[t]
    c = []
    for pos, reg in ((0, 'ecx'), (9, 'edx')):
        c += idx(pos)
        if t == 'b':
            c += ['cmp ebp, 7', 'jne short .p' + str(pos), 'sub dword [m_a+ebp*4], 1', '.p' + str(pos) + ':']
        c += ['sub dword [m_a+ebp*4], %d' % p]
        if pos == 0:
            c += ['mov edx, [m_a+ebp*4]', 'and edx, 0xFFFFFF'] + lee(t) + ['mov ecx, eax']
        else:
            c += ['mov edx, [m_a+ebp*4]', 'and edx, 0xFFFFFF'] + lee(t)
    c += ['push eax', 'mov al, 0', 'sub al, [m_x]', 'pop eax',
          '%s %s, %s' % ('adc' if op == 'add' else 'sbb', r, rc), 'mov ecx, eax', 'call m_fl_x',
          'mov eax, ecx'] + esc(t)
    return c + fin(18 if t != 'l' else 30)


def g_cmpm(t):
    p = TAM[t]
    c = []
    for pos in (0, 9):
        c += idx(pos) + ['mov edx, [m_a+ebp*4]']
        if t == 'b':
            c += ['cmp ebp, 7', 'jne short .p' + str(pos), 'add dword [m_a+ebp*4], 1', '.p' + str(pos) + ':']
        c += ['add dword [m_a+ebp*4], %d' % p, 'and edx, 0xFFFFFF'] + lee(t)
        if pos == 0:
            c += ['mov ecx, eax']
    c += ['cmp %s, %s' % (REG['a'][t], REG['c'][t]), 'FL_CMP']
    return c + fin(12 if t != 'l' else 20)


def g_bcd(op, mem):
    """ABCD/SBCD: Dy,Dx o -(Ay),-(Ax)"""
    rut = 'm_abcd' if op == 'abcd' else 'm_sbcd'
    if not mem:
        c = idx(0) + ['mov ecx, [m_d+ebp*4]'] + idx(9) + ['mov eax, [m_d+ebp*4]', 'push ebp', 'call ' + rut,
                                                          'pop ebp', 'mov [m_d+ebp*4], al']
        return c + fin(6)
    c = []
    for pos in (0, 9):
        c += idx(pos) + ['cmp ebp, 7', 'jne short .p' + str(pos), 'sub dword [m_a+ebp*4], 1',
                         '.p' + str(pos) + ':', 'sub dword [m_a+ebp*4], 1', 'mov edx, [m_a+ebp*4]',
                         'and edx, 0xFFFFFF', 'LEE8']
        if pos == 0:
            c += ['mov ecx, eax']
    c += ['call ' + rut, 'ESC8']
    return c + fin(18)


def g_nbcd(dst):
    if dst == 'Dn':
        return idx(0) + ['mov eax, [m_d+ebp*4]', 'push ebp', 'call m_nbcd', 'pop ebp',
                         'mov [m_d+ebp*4], al'] + fin(6)
    return ea_dir(dst, 0, 'b') + ['LEE8', 'call m_nbcd', 'ESC8'] + fin(8 + cic(dst, 'b'))


def g_tas(dst):
    if dst == 'Dn':
        return idx(0) + ['mov al, [m_d+ebp*4]', 'test al, al', 'FL_LOG', 'or byte [m_d+ebp*4], 0x80'] + fin(4)
    c = ea_dir(dst, 0, 'b') + ['LEE8', 'test al, al', 'mov ecx, eax', 'FL_LOG', 'mov eax, ecx',
                               'or al, 0x80', 'cmp byte [m_tas], 0', 'je short .no', 'ESC8', '.no:']
    return c + fin(10 + cic(dst, 'b'))


def g_bit(op, dinamico, dst):
    """BTST BCHG BCLR BSET (con el número de bit en Dn o inmediato)"""
    x86 = {'btst': 'bt', 'bchg': 'btc', 'bclr': 'btr', 'bset': 'bts'}[op]
    c = []
    if dinamico:
        c += idx(9) + ['mov ecx, [m_d+ebp*4]']
    else:
        c += ['movzx ecx, byte [esi]', 'add esi, 2']
    z = ['setnc cl', 'shl cl, 6', 'and byte [m_f], 0xBF', 'or [m_f], cl']
    if dst == 'Dn':
        c += ['and ecx, 31']
        if op != 'btst':
            c += ['cmp ecx, 16', 'cmc', 'sbb eax, eax', 'and eax, 2', 'sub edi, eax']
        c += idx(0) + ['%s dword [m_d+ebp*4], ecx' % x86] + z
        n = {'btst': 6, 'bchg': 6, 'bset': 6, 'bclr': 8}[op] + (0 if dinamico else 4)
        return c + fin(n)
    c += ['and ecx, 7']
    if dst == 'imm':
        c += ['movzx eax, byte [esi]', 'add esi, 2', 'bt eax, ecx'] + z
        return c + fin(10)
    c += ea_dir(dst, 0, 'b') + ['LEE8', '%s eax, ecx' % x86] + z
    if op != 'btst':
        c += ['ESC8']
    n = (4 if op == 'btst' else 8) + cic(dst, 'b') + (0 if dinamico else 4)
    return c + fin(n)


def g_movep(t, a_mem):
    c = ['movsx edx, word [esi]', 'add esi, 2'] + idx(0) + ['add edx, [m_a+ebp*4]', 'and edx, 0xFFFFFF']
    n = TAM[t]
    if a_mem:
        c += idx(9) + ['mov ecx, [m_d+ebp*4]']
        for i in range(n):
            c += ['mov eax, ecx', 'shr eax, %d' % (8 * (n - 1 - i)), 'ESC8', 'add edx, 2', 'and edx, 0xFFFFFF']
    else:
        c += ['xor ecx, ecx']
        for i in range(n):
            c += ['LEE8', 'shl ecx, 8', 'mov cl, al', 'add edx, 2', 'and edx, 0xFFFFFF']
        c += idx(9) + ['mov [m_d+ebp*4], %s' % REG['c'][t]]
    return c + fin(16 if t == 'w' else 24)


def g_movem(t, a_mem, ea):
    largo = 1 if t == 'l' else 0
    c = ['movzx ecx, word [esi]', 'add esi, 2']
    if a_mem:
        if ea in ('pre', 'pre7'):
            c += idx(0) + ['mov edx, [m_a+ebp*4]', 'push edx', 'and edx, 0xFFFFFF', 'push ebp',
                           'mov eax, %d' % largo, 'call m_movem_rm_pre', 'pop ebp', 'pop eax',
                           'mov ecx, eax', 'and ecx, 0xFFFFFF', 'sub edx, ecx', 'add eax, edx',
                           'mov [m_a+ebp*4], eax']
            return c + fin(8)
        c += ea_dir(ea, 0, None) + ['mov eax, %d' % largo, 'call m_movem_rm']
        return c + fin({'ind': 8, 'd16': 12, 'd8x': 14, 'absw': 12, 'absl': 16}[ea])
    if ea in ('post', 'post7'):
        c += idx(0) + ['mov edx, [m_a+ebp*4]', 'push edx', 'and edx, 0xFFFFFF', 'push ebp',
                       'mov eax, %d' % largo, 'call m_movem_mr', 'pop ebp', 'pop eax',
                       'mov ecx, eax', 'and ecx, 0xFFFFFF', 'sub edx, ecx', 'add eax, edx',
                       'mov [m_a+ebp*4], eax']
        return c + fin(12)
    c += ea_dir(ea, 0, None) + ['mov eax, %d' % largo, 'call m_movem_mr']
    return c + fin({'ind': 12, 'd16': 16, 'd8x': 18, 'absw': 16, 'absl': 20, 'pcd16': 16, 'pcd8x': 18}[ea])


# los desplazamientos y giros
DESP = ['as', 'ls', 'rox', 'ro']


def g_desp_reg(tipo, izq, t, por_reg):
    c = []
    if por_reg:
        c += idx(9) + ['mov ecx, [m_d+ebp*4]', 'and ecx, 63']
    else:
        c += ['mov ecx, ebx', 'shr ecx, 9', 'and ecx, 7', 'movzx ecx, byte [m_q8+ecx]']
    c += idx(0)
    base = 6 if t != 'l' else 8
    c += ['sub edi, ecx', 'sub edi, ecx']
    rapido = tipo in ('ls', 'as', 'ro') and not (tipo == 'as' and izq)
    m = '%s [m_d+ebp*4]' % MEM[t]
    if rapido:
        lim = BITS[t]
        if tipo == 'ro':
            c += ['test ecx, 31', 'jz near .lento']
        else:
            c += ['cmp ecx, %d' % lim, 'jae near .lento', 'test ecx, ecx', 'jz near .lento']
        if tipo == 'ro':
            c += ['%s %s, cl' % ('rol' if izq else 'ror', m), 'setc cl',
                  'test %s, %s' % (m, {'b': '0xFF', 'w': '0xFFFF', 'l': '0xFFFFFFFF'}[t]),
                  'lahf', 'or ah, cl', 'mov byte [m_v], 0', 'mov [m_f], ah']
        else:
            ins = {'ls': ('shl', 'shr'), 'as': ('sal', 'sar')}[tipo][0 if izq else 1]
            c += ['%s %s, cl' % (ins, m), 'lahf', 'setc byte [m_x]', 'mov byte [m_v], 0', 'mov [m_f], ah']
        c += fin(base)
        c += ['.lento:']
    c += ['call m_d_%s%s%s' % (tipo, 'l' if izq else 'r', t)]
    return c + fin(base)


def g_desp_mem(tipo, izq, dst):
    c = ea_dir(dst, 0, 'w') + ['LEE16', 'mov ecx, 1', 'push edx', 'call m_v_%s%sw' % (tipo, 'l' if izq else 'r'),
                               'pop edx', 'ESC16']
    return c + fin(8 + cic(dst, 'w'))


def rutinas_desp():
    """las rutinas lentas: m_d_xxx (sobre Dn: EBP=registro, ECX=cuenta 0-63) y
    m_v_xxx (sobre EAX, ECX=cuenta): bit a bit, con todos los casos"""
    out = []
    for tipo in DESP:
        for izq in (True, False):
            for t in 'bwl':
                n = '%s%s%s' % (tipo, 'l' if izq else 'r', t)
                r = REG['a'][t]
                out.append('m_d_%s:' % n)
                out += ['mov eax, [m_d+ebp*4]', 'call m_v_%s' % n, 'mov [m_d+ebp*4], %s' % r, 'ret']
                out.append('m_v_%s:' % n)
                out += ['push edx', 'xor edx, edx', 'test ecx, ecx', 'jz .cero']
                if tipo == 'rox':
                    out += ['.l:', 'bt word [m_x], 0']
                    out += ['%s %s, 1' % ('rcl' if izq else 'rcr', r), 'setc byte [m_x]', 'dec ecx', 'jnz .l',
                            'mov dh, [m_x]']
                else:
                    ins = {'as': ('sal', 'sar'), 'ls': ('shl', 'shr'), 'ro': ('rol', 'ror')}[tipo][0 if izq else 1]
                    out += ['.l:', '%s %s, 1' % (ins, r)]
                    if tipo == 'as' and izq:
                        out += ['jno .s', 'mov dl, 1', '.s:']
                    out += ['setc dh', 'dec ecx', 'jnz .l']
                    if tipo in ('as', 'ls'):
                        out += ['mov [m_x], dh']
                out += ['.f:', 'test %s, %s' % (r, r), 'push eax', 'lahf', 'and ah, 0xFE', 'or ah, dh',
                        'mov [m_f], ah', 'mov [m_v], dl', 'pop eax', 'pop edx', 'ret']
                out += ['.cero:']
                if tipo == 'rox':
                    out += ['mov dh, [m_x]']
                out += ['jmp .f']
    return out


# ---------------------------------------------------------------------------
#  La descodificación: código -> (clave de la rutina)
# ---------------------------------------------------------------------------
def tam2(bits):
    return {0: 'b', 1: 'w', 2: 'l'}.get(bits)


def descodifica(op):
    alto = op >> 12
    modo = (op >> 3) & 7
    reg = op & 7
    r9 = (op >> 9) & 7
    m6 = (op >> 6) & 7

    def ea(t=None, permitidos=TODOS):
        m = ea_modo(modo, reg, t)
        if m is None or m not in permitidos:
            return None
        return m

    if alto == 0:
        if op in (0x003C, 0x023C, 0x0A3C):
            return ('immccr', {0x003C: 'ori', 0x023C: 'andi', 0x0A3C: 'eori'}[op], False)
        if op in (0x007C, 0x027C, 0x0A7C):
            return ('immccr', {0x007C: 'ori', 0x027C: 'andi', 0x0A7C: 'eori'}[op], True)
        if op & 0x0100:
            if modo == 1:
                return ('movep', 'w' if m6 & 1 == 0 else 'l', bool(m6 & 2))
            bop = ['btst', 'bchg', 'bclr', 'bset'][(op >> 6) & 3]
            e = ea('b', DATOS if bop == 'btst' else DAT_ALT)
            if e is None:
                return None
            return ('bit', bop, True, e)
        sub = r9
        if sub == 4:
            bop = ['btst', 'bchg', 'bclr', 'bset'][(op >> 6) & 3]
            e = ea('b', (DATOS - {'imm'}) if bop == 'btst' else DAT_ALT)
            if e is None:
                return None
            return ('bit', bop, False, e)
        t = tam2((op >> 6) & 3)
        if t is None or sub == 7:
            return None
        iop = {0: 'ori', 1: 'andi', 2: 'subi', 3: 'addi', 5: 'eori', 6: 'cmpi'}.get(sub)
        if iop is None:
            return None
        e = ea(t, DAT_ALT)
        if e is None:
            return None
        return ('imm', iop, t, e)
    if alto in (1, 2, 3):
        t = {1: 'b', 2: 'l', 3: 'w'}[alto]
        src = ea(t, TODOS if t != 'b' else TODOS - {'An'})
        if src is None:
            return None
        if m6 == 1:
            if t == 'b':
                return None
            return ('movea', t, src)
        dst = ea_modo(m6, r9, t)
        if dst is None or dst not in DAT_ALT:
            return None
        return ('move', t, src, dst)
    if alto == 4:
        if op & 0x0100:
            if m6 == 6:
                e = ea('w', DATOS)
                return ('chk', e) if e else None
            if m6 == 7:
                e = ea(None, CONTROL)
                return ('lea', e) if e else None
            return None
        if op == 0x4AFC:
            return ('simple', 'ilegal')
        if (op & 0xFFF0) == 0x4E40:
            return ('trap',)
        if (op & 0xFFF8) == 0x4E50:
            return ('link',)
        if (op & 0xFFF8) == 0x4E58:
            return ('unlk',)
        if (op & 0xFFF8) == 0x4E60:
            return ('usp', True)
        if (op & 0xFFF8) == 0x4E68:
            return ('usp', False)
        simples = {0x4E70: 'reset', 0x4E71: 'nop', 0x4E72: 'stop', 0x4E73: 'rte', 0x4E75: 'rts',
                   0x4E76: 'trapv', 0x4E77: 'rtr'}
        if op in simples:
            return ('simple', simples[op])
        if (op & 0xFFC0) == 0x4E80:
            e = ea(None, CONTROL)
            return ('jmp', True, e) if e else None
        if (op & 0xFFC0) == 0x4EC0:
            e = ea(None, CONTROL)
            return ('jmp', False, e) if e else None
        if (op & 0xFFF8) == 0x4840:
            return ('swap',)
        if (op & 0xFFF8) == 0x4880:
            return ('ext', 'w')
        if (op & 0xFFF8) == 0x48C0:
            return ('ext', 'l')
        if (op & 0xFFC0) == 0x40C0:
            e = ea('w', DAT_ALT)
            return ('movesr', 'de', e) if e else None
        if (op & 0xFFC0) == 0x44C0:
            e = ea('w', DATOS)
            return ('movesr', 'a_ccr', e) if e else None
        if (op & 0xFFC0) == 0x46C0:
            e = ea('w', DATOS)
            return ('movesr', 'a_sr', e) if e else None
        if (op & 0xFFC0) == 0x4800:
            e = ea('b', DAT_ALT)
            return ('nbcd', e) if e else None
        if (op & 0xFFC0) == 0x4840:
            e = ea(None, CONTROL)
            return ('pea', e) if e else None
        if (op & 0xFB80) == 0x4880:
            t = 'l' if op & 0x40 else 'w'
            if op & 0x0400:
                e = ea(t, CONTROL | {'post', 'post7'})
                if e == 'post7':
                    e = 'post'
                return ('movem', t, False, e) if e else None
            e = ea(t, CTL_ALT | {'pre', 'pre7'})
            if e == 'pre7':
                e = 'pre'
            return ('movem', t, True, e) if e else None
        if (op & 0xFFC0) == 0x4AC0:
            e = ea('b', DAT_ALT)
            return ('tas', e) if e else None
        t = tam2((op >> 6) & 3)
        if t is None:
            return None
        un = {0x4000: 'negx', 0x4200: 'clr', 0x4400: 'neg', 0x4600: 'not', 0x4A00: 'tst'}.get(op & 0xFF00)
        if un is None:
            return None
        e = ea(t, DAT_ALT)
        return ('unaria', un, t, e) if e else None
    if alto == 5:
        if (op >> 6) & 3 == 3:
            cc = CONDS[(op >> 8) & 15]
            if modo == 1:
                return ('dbcc', cc)
            e = ea('b', DAT_ALT)
            return ('scc', cc, e) if e else None
        t = tam2((op >> 6) & 3)
        e = ea(t, ALTERABLE)
        if e is None or (e == 'An' and t == 'b'):
            return None
        return ('q', 'sub' if op & 0x100 else 'add', t, e)
    if alto == 6:
        cc = (op >> 8) & 15
        nombre = 't' if cc == 0 else ('bsr' if cc == 1 else CONDS[cc])
        return ('bcc', nombre, (op & 0xFF) == 0)
    if alto == 7:
        if op & 0x100:
            return None
        return ('moveq',)
    if alto in (8, 9, 0xB, 0xC, 0xD):
        tsz = (op >> 6) & 3
        if alto in (9, 0xB, 0xD) and tsz == 3:
            t = 'l' if op & 0x100 else 'w'
            e = ea(t, TODOS)
            if e is None:
                return None
            return ('adda', {9: 'sub', 0xB: 'cmp', 0xD: 'add'}[alto], t, e)
        if alto in (8, 0xC) and tsz == 3:
            e = ea('w', DATOS)
            if e is None:
                return None
            if alto == 8:
                return ('div', bool(op & 0x100), e)
            return ('mul', bool(op & 0x100), e)
        t = tam2(tsz)
        if not op & 0x100:
            opn = {8: 'or', 9: 'sub', 0xB: 'cmp', 0xC: 'and', 0xD: 'add'}[alto]
            perm = DATOS if opn in ('or', 'and') else TODOS
            e = ea(t, perm)
            if e is None or (e == 'An' and t == 'b'):
                return None
            return ('alu1', opn, t, e)
        # bit 8 a 1
        if alto == 0xB:
            if modo == 1:
                return ('cmpm', t)
            e = ea(t, DAT_ALT)
            if e is None:
                return None
            if e == 'Dn':
                return ('eordn', t)
            return ('alu2', 'eor', t, e)
        if modo in (0, 1):
            if alto in (8, 0xC):
                if tsz == 0:
                    return ('bcd', 'sbcd' if alto == 8 else 'abcd', modo == 1)
                if alto == 0xC and tsz == 1:
                    return ('exg', 'dd' if modo == 0 else 'aa')
                if alto == 0xC and tsz == 2 and modo == 1:
                    return ('exg', 'da')
                return None
            return ('addx', 'sub' if alto == 9 else 'add', t, modo == 1)
        e = ea(t, MEM_ALT)
        if e is None:
            return None
        return ('alu2', {8: 'or', 9: 'sub', 0xC: 'and', 0xD: 'add'}[alto], t, e)
    if alto == 0xE:
        if (op >> 6) & 3 == 3:
            if op & 0x0800:
                return None
            e = ea('w', MEM_ALT)
            if e is None:
                return None
            return ('despm', DESP[(op >> 9) & 3], bool(op & 0x100), e)
        t = tam2((op >> 6) & 3)
        return ('despr', DESP[(op >> 3) & 3], bool(op & 0x100), t, bool(op & 0x20))
    if alto == 0xA:
        return ('simple', 'lineaa')
    if alto == 0xF:
        return ('simple', 'lineaf')
    return None


GEN = {'move': g_move, 'movea': g_movea, 'alu1': g_alu_ea_dn, 'alu2': g_alu_dn_ea, 'eordn': g_eor_dn,
       'adda': g_adda, 'imm': g_imm, 'immccr': g_imm_ccr, 'q': g_q, 'moveq': g_moveq, 'unaria': g_unaria,
       'bcc': g_bcc, 'dbcc': g_dbcc, 'scc': g_scc, 'lea': g_lea, 'pea': g_pea, 'jmp': g_jmp,
       'simple': g_simple, 'trap': g_trap, 'link': g_link, 'unlk': g_unlk, 'usp': g_usp, 'swap': g_swap,
       'ext': g_ext, 'exg': g_exg, 'movesr': g_move_sr, 'mul': g_mul, 'div': g_div, 'chk': g_chk,
       'addx': g_addx, 'cmpm': g_cmpm, 'bcd': g_bcd, 'nbcd': g_nbcd, 'tas': g_tas, 'bit': g_bit,
       'movep': g_movep, 'movem': g_movem, 'despr': g_desp_reg, 'despm': g_desp_mem}


def main():
    salida = sys.argv[1]
    claves = {}
    lista = []
    tabla = []
    for op in range(65536):
        k = descodifica(op)
        if k is None:
            k = ('simple', 'ilegal')
        if k not in claves:
            claves[k] = len(lista)
            lista.append(k)
        tabla.append(claves[k])
    out = ['; generado por tools/gen68k.py: no se toca a mano', '']
    for i, k in enumerate(lista):
        lineas = GEN[k[0]](*k[1:])
        out.append('        align 4')
        out.append('m_r%d:                ; %s' % (i, ' '.join(str(x) for x in k)))
        for ln in lineas:
            if ln.endswith(':'):
                out.append(ln)
            else:
                out.append('        ' + ln)
    out.append('')
    out += [('        ' + x) if not x.endswith(':') else x for x in rutinas_desp()]
    out.append('')
    out.append('        align 4')
    out.append('m_rutinas:')
    for i in range(len(lista)):
        out.append('        dd m_r%d' % i)
    out.append('m_rle:')
    i = 0
    while i < 65536:
        j = i
        while j < 65536 and tabla[j] == tabla[i] and j - i < 65535:
            j += 1
        out.append('        dw %d, %d' % (j - i, tabla[i]))
        i = j
    out.append('        dw 0, 0')
    out.append('m_q8    db 8, 1, 2, 3, 4, 5, 6, 7')
    out.append('')
    with open(salida, 'w') as f:
        f.write('\n'.join(out))
    sys.stderr.write('gen68k: %d rutinas\n' % len(lista))


if __name__ == '__main__':
    main()
