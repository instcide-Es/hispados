#!/usr/bin/env python3
# Hispa-DOS - comprobar el bloque residente de la red por USB de HSPUSB
# (sección .red de lib/usb_red.inc). Ese bloque se ensambla para la
# dirección 0 y se ejecuta donde lo copie net_setup, así que dentro de él:
#   - las llamadas y saltos a rutinas de fuera (.text) tienen que ir con
#     MCALL (que apunta la posición para corregirla) o por una tabla de
#     direcciones ([c_blk_tab+bx]); un «call rutina» directo saltaría a
#     cualquier sitio;
#   - sus propias etiquetas solo valen como desplazamiento desde la base
#     del bloque: dentro de corchetes con un registro ([di+n_st]) o en LEA
#     y ADD; un «[n_st]» a secas leería de otro sitio.
# Lee el listado de NASM (-l) y falla si encuentra algo así.
#
#   tools/redcheck.py listado.lst
#
# Checks that the relocated .red block of HSPUSB only reaches outside code
# through MCALL and its own data through a base register.
import re
import sys

SALTOS = {'call', 'jmp', 'loop', 'loope', 'loopne', 'loopz', 'loopnz', 'jcxz', 'jecxz'}
DATOS = {'db', 'dw', 'dd', 'dq', 'times', 'align', 'equ', 'resb', 'resw', 'resd'}
CON_BASE = {'lea', 'add', 'sub'}


def fuente(linea):
    m = re.match(r'^\s*\d+ (?:[0-9A-F]{8} \S*\s*)?\s*((?:<\d+> ?)*)(.*)$', linea)
    if not m:
        return 0, ''
    return m.group(1).count('<'), m.group(2)


def main():
    filas = []
    sec = '.text'
    for n, linea in enumerate(open(sys.argv[1], encoding='latin-1'), 1):
        nivel, s = fuente(linea.rstrip('\n'))
        s = re.sub(r'"[^"]*"', '', s).split(';')[0]
        m = re.match(r'^\s*section\s+(\S+)', s)
        if m:
            sec = m.group(1)
            continue
        if sec == '.red' and s.strip():
            filas.append((n, nivel, s))
    if not filas:
        print('redcheck: no hay sección .red en el listado')
        return 1
    etq = re.compile(r'^\s*([A-Za-z_]\w*)\s*:|^\s*([A-Za-z_]\w*)\s+(?:db|dw|dd|dq|times)\b')
    suyas = set()
    for _, nivel, s in filas:
        m = etq.match(s)
        if m:
            suyas.add(m.group(1) or m.group(2))
    malas = []
    for n, nivel, s in filas:
        if '%' in s or '__SECT__' in s or '[section' in s:
            continue                    # el cuerpo de MCALL, expandido
        x = re.sub(r'^\s*[A-Za-z_.]\w*\s*:', '', s).strip()
        x = re.sub(r'^([A-Za-z_]\w*)\s+(?=(?:db|dw|dd|dq|times)\b)', '', x)
        if not x or re.match(r'^\w+\s+equ\b', x):
            continue
        p = x.split(None, 1)
        mn = p[0].lower()
        op = p[1] if len(p) > 1 else ''
        if mn in DATOS or mn == 'mcall' or mn.startswith('%'):
            continue
        if mn in SALTOS or (mn.startswith('j') and len(mn) <= 4):
            d = re.sub(r'^(near|short|far|word|dword)\s+', '', op.strip(), flags=re.I)
            if d.startswith('.') or d.startswith('[') or d in suyas:
                continue
            if re.fullmatch(r'(?:e?[abcd]x|e?[sd]i|e?bp)', d, re.I):
                continue
            malas.append((n, s, 'salto o llamada a fuera del bloque sin MCALL'))
            continue
        for w in re.findall(r'(?<![\w.$])([A-Za-z_]\w*)', op):
            if w not in suyas:
                continue
            en_corchetes = re.search(r'\[[^\]]*\b(?:bx|si|di|bp)\b[^\]]*\b%s\b[^\]]*\]' % re.escape(w), op) \
                or re.search(r'\[[^\]]*\b%s\b[^\]]*\b(?:bx|si|di|bp)\b[^\]]*\]' % re.escape(w), op)
            if en_corchetes or mn in CON_BASE:
                continue
            malas.append((n, s, 'etiqueta del bloque «%s» sin el registro de la base' % w))
    for n, s, por in malas:
        print('redcheck: línea %d del listado: %s\n    %s' % (n, por, s.strip()))
    return 1 if malas else 0


if __name__ == '__main__':
    sys.exit(main())
