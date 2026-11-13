#!/usr/bin/env python3
# Hispa-DOS - comprobar que la parte residente de un driver (sección .text)
# no usa nada de su inicialización (sección .init), que se descarta al
# arrancar. Lee el listado de NASM (-l) y falla si .text nombra una etiqueta
# de .init que no esté en la lista de permitidas (las que solo se usan
# durante el arranque o con una comprobación delante: ver drivers/hspusb.asm).
#
#   tools/initcheck.py listado.lst [permitida,...] [prefijo*,...]
#
# Checks that resident code (.text) never references init-only labels (.init).
import re
import sys


def source(line):
    m = re.match(r'^\s*\d+ (?:[0-9A-F]{8} \S*\s*)?\s*<\d+> ?(.*)$', line)
    if m:
        return m.group(1)
    return line[34:] if len(line) > 34 else ''


def main():
    lst = sys.argv[1]
    allow = set()
    prefixes = []
    for a in sys.argv[2:]:
        for w in a.split(','):
            if w.endswith('*'):
                prefixes.append(w[:-1])
            elif w:
                allow.add(w)
    rows = []
    sec = '.text'
    for line in open(lst, encoding='latin-1'):
        s = source(line.rstrip('\n')).split(';')[0]
        m = re.match(r'^\s*section\s+(\S+)', s)
        if m:
            sec = m.group(1)
            continue
        rows.append((sec, s))
    lab = re.compile(r'^\s*([A-Za-z_]\w*)\s*:|^\s*([A-Za-z_]\w*)\s+(?:db|dw|dd|dq|times|resb|resw|resd)\b')
    where = {}
    for sec, s in rows:
        m = lab.match(s)
        if m:
            where.setdefault(m.group(1) or m.group(2), sec)
    init = {k for k, v in where.items() if v == '.init'}
    bad = {}
    cur = '?'
    for sec, s in rows:
        m = lab.match(s)
        if m and m.group(1):
            cur = m.group(1)
        if sec != '.text' or re.match(r'^\s*\w+\s+equ\b', s):
            continue
        x = re.sub(r'^\s*[A-Za-z_.]\w*\s*:', '', s)
        x = re.sub(r'"[^"]*"', '', x)
        for w in re.findall(r'(?<![\w.$])([A-Za-z_]\w*)', x):
            if w in init and w not in allow and not any(w.startswith(p) for p in prefixes):
                bad.setdefault(w, set()).add(cur)
    for w, c in sorted(bad.items()):
        print('initcheck: %s (de .init) se usa en lo residente: %s' % (w, ', '.join(sorted(c))))
    sys.exit(1 if bad else 0)


main()
