#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# fuzz.py PROGRAMA CARPETA_SEMILLAS VECES [SEMILLA]
# El fuzz del Reproductor en el anfitrión: cada archivo de prueba (los de
# haz_pruebas.py y unos hechos aquí con etiquetas y listas raras), cambiado
# al azar (bits, octetos, trozos repetidos o quitados, cortado), pasa por
# «repro fuzz» (los decodificadores, las etiquetas, las listas y las hojas
# .CUE) hecho con ASan y UBSan. Cuenta los que revientan (lo de terceros/
# que solo avisa de UB sin romper nada se cuenta aparte) y deja el primero
# de cada clase en /tmp para verlo.
import os, random, subprocess, sys, tempfile, struct

prog, carpeta, veces = sys.argv[1], sys.argv[2], int(sys.argv[3])
random.seed(int(sys.argv[4]) if len(sys.argv) > 4 else 2026)
semillas = []
for n in sorted(os.listdir(carpeta)):
    p = os.path.join(carpeta, n)
    if os.path.isfile(p) and os.path.getsize(p) < 3_000_000 and not n.endswith('.png'):
        semillas.append(open(p, 'rb').read())

# unas semillas más: etiquetas y listas a propósito raras
def id3(marcos, v=3, flags=0):
    cuerpo = b''
    for k, d in marcos:
        n = len(d)
        tam = struct.pack('>I', n) if v == 3 else bytes([(n >> 21) & 127, (n >> 14) & 127, (n >> 7) & 127, n & 127])
        cuerpo += k + tam + b'\0\0' + d
    n = len(cuerpo)
    return b'ID3' + bytes([v, 0, flags, (n >> 21) & 127, (n >> 14) & 127, (n >> 7) & 127, n & 127]) + cuerpo

semillas.append(id3([(b'TIT2', b'\x01\xff\xfeH\x00o\x00l\x00a\x00'), (b'APIC', b'\x00image/png\x00\x03desc\x00\x89PNG' + b'x' * 100),
                     (b'TXXX', b'\x03REPLAYGAIN_TRACK_GAIN\x00-3.20 dB'), (b'COMM', b'\x00spa\x00comentario')]) + b'\xff\xfb\x90\x00' * 200)
semillas.append(id3([(b'TIT2', b'\x03t\xc3\xadtulo'), (b'TCON', b'\x00(17)')], v=4, flags=0x80))
semillas.append(b'fLaC' + bytes([0x84, 0, 0, 40]) + struct.pack('<I', 4) + b'yo\x00\x00' + struct.pack('<I', 1) + struct.pack('<I', 10) + b'TITLE=hola' + b'\0' * 12)
semillas.append(b'#EXTM3U\r\n#EXTINF:-1,\r\n../../../../x.mp3\r\nfile:///C:/M%C3%9ASICA/a.mp3\r\n' + b'a' * 2000 + b'\r\n')
semillas.append(b'FILE "a.bin" BINARY\r\nTRACK 01 AUDIO\r\nINDEX 01 99:99:99\r\nTRACK 02 AUDIO\r\nINDEX 01 00:00:01\r\n' * 30)
semillas.append(b'[playlist]\r\nFile1=\r\nTitle1=' + b'z' * 3000 + b'\r\nLength1=-5\r\n')

def cambia(d):
    d = bytearray(d)
    for _ in range(random.randint(1, 12)):
        if not d:
            break
        c = random.random()
        i = random.randrange(len(d))
        if c < 0.4:
            d[i] ^= 1 << random.randrange(8)
        elif c < 0.6:
            d[i] = random.choice([0, 0xFF, 0x7F, 0x80, random.randrange(256)])
        elif c < 0.7:
            j = min(len(d), i + random.randint(1, 64))
            d[i:i] = d[i:j] * random.randint(1, 4)
        elif c < 0.8:
            del d[i:i + random.randint(1, 256)]
        elif c < 0.9:
            d[i:i + 4] = struct.pack('<I', random.choice([0, 0xFFFFFFFF, 0x7FFFFFFF, random.randrange(1 << 32)]))
        else:
            d = d[:i]
    return bytes(d)

malos = ub = 0
tmp = tempfile.mkdtemp()
exts = ['.mp3', '.ogg', '.flac', '.wav', '.mod', '.xm', '.s3m', '.it', '.m3u', '.pls', '.cue']
env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0:abort_on_error=0', UBSAN_OPTIONS='print_stacktrace=0:halt_on_error=0')
for k in range(veces):
    s = random.choice(semillas)
    ruta = os.path.join(tmp, 'f%d%s' % (k % 4, random.choice(exts)))
    open(ruta, 'wb').write(cambia(s))
    try:
        r = subprocess.run([prog, 'fuzz', ruta], capture_output=True, timeout=60, env=env)
        err = r.stderr.decode('utf-8', 'replace')
        nuestro = [l for l in err.splitlines() if 'runtime error' in l and '/terceros/' not in l]
        if r.returncode != 0 or 'AddressSanitizer' in err or nuestro:
            malos += 1
            if malos <= 5:
                dest = '/tmp/repro-fuzz-%d%s' % (malos, os.path.splitext(ruta)[1])
                open(dest, 'wb').write(open(ruta, 'rb').read())
                print('REVIENTA (%d): %s\n%s' % (r.returncode, dest, '\n'.join(err.splitlines()[:12])))
        elif 'runtime error' in err:
            ub += 1
    except subprocess.TimeoutExpired:
        malos += 1
        print('SE CUELGA:', ruta)
print('fuzz: %d casos, %d malos, %d con avisos de UB solo en terceros/' % (veces, malos, ub))
