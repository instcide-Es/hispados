#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""mirador/prueba/haz_imagenes.py CARPETA: las imágenes de prueba del Mirador (pruebas_zipvisor.sh),
hechas aquí, sin bibliotecas: FLOR.PNG, GRANDE.PNG (1200 x 900, con su rejilla), CASA.BMP, PAISAJE.JPG,
BOLA.GIF (animado, cuatro cuadros, con su LZW) e ICONO.ICO (uno de 16 x 16 de 4 bits con su máscara y
otro de 32 x 32 de 32 bits con alfa)."""
import struct, zlib, sys, base64, os
g = sys.argv[1]
def png(w, h, f):
    raw = b''.join(b'\0' + b''.join(bytes(f(x, y)) for x in range(w)) for y in range(h))
    ch = lambda t, d: struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d))
    return b'\x89PNG\r\n\x1a\n' + ch(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) + ch(b'IDAT', zlib.compress(raw)) + ch(b'IEND', b'')
open(g + '/FLOR.PNG', 'wb').write(png(160, 120, lambda x, y: (255, 122, 51) if (x - 80) ** 2 + (y - 60) ** 2 < 35 ** 2 else (47, 116, 208)))
# una grande: 1200 x 900, degradado con una rejilla (para acercar y alejar)
open(g + '/GRANDE.PNG', 'wb').write(png(1200, 900, lambda x, y: (255, 255, 255) if x % 100 < 3 or y % 100 < 3 else (x * 255 // 1200, y * 255 // 900, 128)))
w, h = 64, 96
filas = b''.join(bytes((255, 255, 255) if 10 <= x < 54 and 56 <= y < 86 else (0x4A, 0x29, 0x14)) for y in range(h) for x in range(w))
open(g + '/CASA.BMP', 'wb').write(b'BM' + struct.pack('<IHHI', 54 + len(filas), 0, 0, 54) + struct.pack('<IiiHHIIiiII', 40, w, h, 1, 24, 0, len(filas), 2835, 2835, 0, 0) + filas)
open(g + '/PAISAJE.JPG', 'wb').write(base64.b64decode('/9j/4AAQSkZJRgABAQAAAAAAAAD/2wBDAAYEBQYFBAYGBQYHBwYIChAKCgkJChQODwwQFxQYGBcUFhYaHSUfGhsjHBYWICwgIyYnKSopGR8tMC0oMCUoKSj/2wBDAQcHBwoIChMKChMoGhYaKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCj/wAARCAAgADADASIAAhEBAxEB/8QAFgABAQEAAAAAAAAAAAAAAAAAAAYH/8QAFhAAAwAAAAAAAAAAAAAAAAAAABRh/8QAFwEBAQEBAAAAAAAAAAAAAAAAAAYFB//EABcRAQEBAQAAAAAAAAAAAAAAAAATFGH/2gAMAwEAAhEDEQA/ANQfofpKP0P04rgWkVW/Q/SUfofowEVW/Q/SUfofowEUo/Q/SUfofpa4ONWKrfofpKP0P0YOEVW/Q/SUfofowcIv/9k='))
# un GIF animado de 4 cuadros (64 x 64): un cuadrado que va de un lado a otro
def lzw(datos, minimo=8):
    clear, fin = 1 << minimo, (1 << minimo) + 1
    tabla = {bytes([i]): i for i in range(1 << minimo)}
    sig, tam = fin + 1, minimo + 1
    bits, n, out = 0, 0, bytearray()
    def pon(c):
        nonlocal bits, n
        bits |= c << n; n += tam
        while n >= 8:
            out.append(bits & 255); bits >>= 8; n -= 8
    pon(clear)
    w = b''
    for b in datos:
        wb = w + bytes([b])
        if wb in tabla:
            w = wb
        else:
            pon(tabla[w])
            if sig < 4096:
                tabla[wb] = sig; sig += 1
                if sig > (1 << tam) and tam < 12: tam += 1
            else:
                pon(clear); tabla = {bytes([i]): i for i in range(1 << minimo)}; sig, tam = fin + 1, minimo + 1
            w = bytes([b])
    if w: pon(tabla[w])
    pon(fin)
    if n: out.append(bits & 255)
    return bytes(out)
pal = bytes([255, 255, 255, 220, 30, 30, 30, 120, 220, 0, 0, 0]) + bytes(3 * 252)
gif = b'GIF89a' + struct.pack('<HHBBB', 64, 64, 0xF7, 0, 0) + pal + b'\x21\xFF\x0BNETSCAPE2.0\x03\x01\x00\x00\x00'
for k in range(4):
    px = bytes((1 if (16 * k <= x < 16 * k + 16 and 24 <= y < 40) else 2 if y > 56 else 0) for y in range(64) for x in range(64))
    d = lzw(px)
    gif += b'\x21\xF9\x04\x00' + struct.pack('<H', 25) + b'\x00\x00' + b'\x2C' + struct.pack('<HHHHB', 0, 0, 64, 64, 0) + b'\x08'
    for i in range(0, len(d), 255):
        gif += bytes([len(d[i:i + 255])]) + d[i:i + 255]
    gif += b'\x00'
open(g + '/BOLA.GIF', 'wb').write(gif + b'\x3B')
# un ICO con dos: 16 x 16 de 4 bits (con su máscara) y 32 x 32 de 32 bits (con alfa)
def dib32(n):
    filas = b''.join(bytes((0, 160, 0, 255) if (x - 16) ** 2 + (y - 16) ** 2 < 14 ** 2 else (0, 0, 0, 0)) for y in range(n - 1, -1, -1) for x in range(n))
    return struct.pack('<IiiHHIIiiII', 40, n, 2 * n, 1, 32, 0, 0, 0, 0, 0, 0) + filas + bytes(((n + 31) // 32) * 4 * n)
def dib4(n):
    pal = bytes([0, 0, 0, 0, 0, 0, 255, 0] + [0] * 56)
    filas = b''.join(bytes([0x11] * (n // 2)) for y in range(n))
    mascara = b''.join(bytes([0xF0, 0x0F, 0, 0]) for y in range(n))
    return struct.pack('<IiiHHIIiiII', 40, n, 2 * n, 1, 4, 0, 0, 0, 0, 16, 0) + pal + filas + mascara
a, b = dib4(16), dib32(32)
ico = struct.pack('<HHH', 0, 1, 2) + struct.pack('<BBBBHHII', 16, 16, 16, 0, 1, 4, len(a), 6 + 32) + struct.pack('<BBBBHHII', 32, 32, 0, 0, 1, 32, len(b), 6 + 32 + len(a)) + a + b
open(g + '/ICONO.ICO', 'wb').write(ico)
