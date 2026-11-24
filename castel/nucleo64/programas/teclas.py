#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo64/programas/teclas.py - para las pruebas de Hispa-64, etapa 6
# (pruebas.sh): escribe un texto en el teclado de QEMU (sendkey por el
# monitor), con el mapa español, como lo teclearía una persona: las
# mayúsculas con Mayús, AltGr para @ # | ~ [ ] { } \ €, los acentos con la
# tecla muerta (´ ` ¨ ^) y luego la vocal, la ñ y la ç en su sitio.
# Las teclas especiales, entre llaves: {intro} {retro} {supr} {izq} {der}
# {arriba} {abajo} {inicio} {fin} {tab} {esc} {ctrl-c} {ctrl-d} {ctrl-u}
# {bloqmayus} {bloqnum} {f1}... {pausa} (medio segundo sin teclas). Va igual para el teclado PS/2
# y para uno USB (QEMU manda sendkey al teclado que haya).
#   teclas.py MONITOR.sock TEXTO...    (los textos, seguidos)
#   teclas.py -n TEXTO...              dice las teclas, sin mandarlas
import socket
import sys
import time

# las teclas de QEMU (por su sitio en el teclado de EE. UU.) de cada letra en el mapa español
SIMPLES = {' ': 'spc', '\n': 'ret', '\t': 'tab', '.': 'dot', ',': 'comma', '-': 'slash', "'": 'minus',
           '<': 'less', '+': 'bracket_right', 'ñ': 'semicolon', 'ç': 'backslash', 'º': 'grave_accent',
           '¡': 'equal'}
CON_MAYÚS = {'!': '1', '"': '2', '·': '3', '$': '4', '%': '5', '&': '6', '/': '7', '(': '8', ')': '9',
             '=': '0', '?': 'minus', '¿': 'equal', '*': 'bracket_right', ';': 'comma', ':': 'dot',
             '_': 'slash', '>': 'less', 'Ñ': 'semicolon', 'Ç': 'backslash', 'ª': 'grave_accent',
             }
CON_ALTGR = {'|': '1', '@': '2', '#': '3', '~': '4', '€': 'e', '¬': '6', '[': 'bracket_left',
             ']': 'bracket_right', '{': 'apostrophe', '}': 'backslash', '\\': 'grave_accent'}
# los acentos: la tecla muerta y la vocal
MUERTAS = {'´': 'apostrophe', '`': 'bracket_left', '¨': 'shift-apostrophe', '^': 'shift-bracket_left'}
ACENTOS = {'á': '´a', 'é': '´e', 'í': '´i', 'ó': '´o', 'ú': '´u', 'Á': '´A', 'É': '´E', 'Í': '´I',
           'Ó': '´O', 'Ú': '´U', 'à': '`a', 'è': '`e', 'ì': '`i', 'ò': '`o', 'ù': '`u', 'ü': '¨u',
           'Ü': '¨U', 'ï': '¨i', 'â': '^a', 'ê': '^e', 'î': '^i', 'ô': '^o', 'û': '^u'}
ESPECIALES = {'intro': 'ret', 'retro': 'backspace', 'supr': 'delete', 'izq': 'left', 'der': 'right',
              'arriba': 'up', 'abajo': 'down', 'inicio': 'home', 'fin': 'end', 'tab': 'tab', 'esc': 'esc',
              'ctrl-c': 'ctrl-c', 'ctrl-d': 'ctrl-d', 'ctrl-u': 'ctrl-u', 'bloqmayus': 'caps_lock',
              'bloqnum': 'num_lock', 'pausa': None}
for i in range(1, 13):
    ESPECIALES[f'f{i}'] = f'f{i}'


def una(c):
    if c in MUERTAS:
        return [MUERTAS[c], 'spc']
    if c in ACENTOS:
        m, v = ACENTOS[c]
        return [MUERTAS[m]] + una(v)
    if 'a' <= c <= 'z' or '0' <= c <= '9':
        return [c]
    if 'A' <= c <= 'Z':
        return ['shift-' + c.lower()]
    if c in SIMPLES:
        return [SIMPLES[c]]
    if c in CON_MAYÚS:
        return ['shift-' + CON_MAYÚS[c]]
    if c in CON_ALTGR:
        return ['alt_r-' + CON_ALTGR[c]]
    raise SystemExit(f'teclas.py: no sé teclear «{c}»')


def teclas(texto):
    t = []
    i = 0
    while i < len(texto):
        if texto[i] == '{':
            j = texto.index('}', i)
            n = texto[i + 1:j]
            if n not in ESPECIALES:
                raise SystemExit(f'teclas.py: no sé qué tecla es «{{{n}}}»')
            t.append(ESPECIALES[n])
            i = j + 1
            continue
        t += una(texto[i])
        i += 1
    return t


def manda(sock, lista):
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(sock)
    s.settimeout(5)
    time.sleep(0.3)
    for k in lista:
        if k is None:
            time.sleep(0.5)
            continue
        s.sendall(f'sendkey {k} 30\n'.encode())
        time.sleep(0.07)
    time.sleep(0.3)
    s.close()


if __name__ == '__main__':
    if len(sys.argv) < 3:
        raise SystemExit('uso: teclas.py MONITOR.sock TEXTO...  (o -n TEXTO...)')
    lista = teclas(''.join(sys.argv[2:]))
    if sys.argv[1] == '-n':
        print(' '.join('(pausa)' if k is None else k for k in lista))
    else:
        manda(sys.argv[1], lista)
