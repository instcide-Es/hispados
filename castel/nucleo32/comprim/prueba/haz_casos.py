#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""prueba/haz_casos.py CARPETA

Los ZIP raros para las pruebas de Comprimidos (prueba.sh), hechos a mano o con
zipfile: el AES de WinZip (AE-1 y AE-2; 128 y 256; deflate y guardado: con
«cryptography», que da el AES; el CTR de WinZip, con el contador en
little-endian, se hace aquí), nombres en la página 437 sin la marca de UTF-8,
ZIP64 a la fuerza, y los malos: rutas con «..», absolutas y con letra de
unidad, demasiado hondas, una bomba «solapada» (dos entradas sobre los mismos
datos), uno que dice medir menos de lo que da, uno con el CRC cambiado y uno
con los datos AES tocados.
"""
import hashlib, hmac, os, struct, sys, zipfile, zlib

S = sys.argv[1]
os.makedirs(S, exist_ok=True)
TEXTO = (b'En un lugar de la Mancha, de cuyo nombre no quiero acordarme.\n' * 400)


def crudo(nombre_bytes, datos, metodo=8, marcas=0, crc=None, tam=None, extra=b'', version=20, hecho_por=20):
    """(local, central sin el desplazamiento) de un miembro"""
    if metodo == 8:
        c = zlib.compressobj(6, zlib.DEFLATED, -15)
        comp = c.compress(datos) + c.flush()
    else:
        comp = datos
    crc = zlib.crc32(datos) if crc is None else crc
    tam = len(datos) if tam is None else tam
    return nombre_bytes, comp, crc, tam, metodo, marcas, extra, version, hecho_por


def junta(miembros, ruta, cd_extra_de=None):
    out = b''
    cd = b''
    for (nb, comp, crc, tam, metodo, marcas, extra, version, hecho_por) in miembros:
        off = len(out)
        out += struct.pack('<IHHHHHIIIHH', 0x04034B50, version, marcas, metodo, 0x6000, 0x5B75, crc, len(comp), tam, len(nb), len(extra)) + nb + extra + comp
        cd += struct.pack('<IHHHHHHIIIHHHHHII', 0x02014B50, hecho_por, version, marcas, metodo, 0x6000, 0x5B75, crc, len(comp), tam,
                          len(nb), len(extra), 0, 0, 0, 0x20, off) + nb + extra
    out += cd + struct.pack('<IHHHHIIH', 0x06054B50, 0, 0, len(miembros), len(miembros), len(cd), len(out), 0)
    open(os.path.join(S, ruta), 'wb').write(out)


# ---- el AES de WinZip
def aes_zip(ruta, clave, fuerza, version, metodo, tocado=False):
    from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
    lk = {1: 16, 2: 24, 3: 32}[fuerza]
    sal = os.urandom(lk // 2)
    d = hashlib.pbkdf2_hmac('sha1', clave, sal, 1000, 2 * lk + 2)
    k_aes, k_mac, verif = d[:lk], d[lk:2 * lk], d[2 * lk:]
    if metodo == 8:
        c = zlib.compressobj(6, zlib.DEFLATED, -15)
        claro = c.compress(TEXTO) + c.flush()
    else:
        claro = TEXTO
    enc = Cipher(algorithms.AES(k_aes), modes.ECB()).encryptor()
    flujo = b''
    n = 1
    while len(flujo) < len(claro):
        flujo += enc.update(n.to_bytes(16, 'little'))
        n += 1
    cif = bytes(a ^ b for a, b in zip(claro, flujo))
    mac = hmac.new(k_mac, cif, hashlib.sha1).digest()[:10]
    if tocado:
        cif = cif[:100] + bytes([cif[100] ^ 1]) + cif[101:]
    datos = sal + verif + cif + mac
    extra = struct.pack('<HHH2sBH', 0x9901, 7, version, b'AE', fuerza, metodo)
    crc = zlib.crc32(TEXTO) if version == 1 else 0
    m = (b'quijote.txt', datos, crc, len(TEXTO), 99, 1, extra, 51, 20)
    junta([m], ruta)


aes_zip('aes256.zip', b'clave buena', 3, 2, 8)
aes_zip('aes128.zip', b'clave buena', 1, 1, 8)
aes_zip('aes192g.zip', b'clave buena', 2, 2, 0)
aes_zip('aestocado.zip', b'clave buena', 3, 2, 8, tocado=True)

# ---- nombres en la página 437 (sin la marca de UTF-8), hecho en MS-DOS
junta([crudo('CAÑON/NIÑO Ü.TXT'.encode('cp437'), b'hola\n', metodo=0)], 'cp437.zip')
# y en UTF-8 con la marca (zipfile la pone sola)
with zipfile.ZipFile(os.path.join(S, 'utf8.zip'), 'w', zipfile.ZIP_DEFLATED) as z:
    z.writestr('Canción de cuna/Año 2026 €.txt', TEXTO)

# ---- ZIP64 a la fuerza (el de zipfile)
with zipfile.ZipFile(os.path.join(S, 'zip64.zip'), 'w', zipfile.ZIP_DEFLATED, allowZip64=True) as z:
    with z.open('grande.txt', 'w', force_zip64=True) as f:
        f.write(TEXTO)
    z.writestr('otro.txt', b'corto\n')

# ---- los malos
junta([crudo(b'../fuera.txt', b'mal\n'), crudo(b'bueno.txt', b'bien\n')], 'subir.zip')
junta([crudo(b'/etc/fuera.txt', b'mal\n'), crudo(b'C:/fuera.txt', b'mal\n'), crudo(b'a\\..\\..\\fuera.txt', b'mal\n'), crudo(b'bueno.txt', b'bien\n')], 'absoluto.zip')
junta([crudo(('/'.join(['h'] * 40) + '/x.txt').encode(), b'hondo\n'), crudo(b'bueno.txt', b'bien\n')], 'hondo.zip')
junta([crudo(b'corto.txt', b'\0' * 100000, tam=10)], 'miente.zip')
junta([crudo(b'crc.txt', TEXTO, crc=0x12345678)], 'crc.zip')
# la bomba solapada: dos entradas del directorio sobre la misma cabecera
nb, comp, crc, tam = b'cero.bin', zlib.compress(b'\0' * 1000000)[2:-4], zlib.crc32(b'\0' * 1000000), 1000000
loc = struct.pack('<IHHHHHIIIHH', 0x04034B50, 20, 0, 8, 0, 0x5B75, crc, len(comp), tam, len(nb), 0) + nb + comp
cd = b''
for nombre in (b'cero1.bin', b'cero2.bin'):
    cd += struct.pack('<IHHHHHHIIIHHHHHII', 0x02014B50, 20, 20, 0, 8, 0, 0x5B75, crc, len(comp), tam, len(nombre), 0, 0, 0, 0, 0x20, 0) + nombre
open(os.path.join(S, 'solapado.zip'), 'wb').write(loc + cd + struct.pack('<IHHHHIIH', 0x06054B50, 0, 0, 2, 2, len(cd), len(loc), 0))
# una de ceros (8 MB que ocupan unos 8 KB): con el tope de 1 MB (COMPRIM_MAX_TOTAL) no se saca
junta([crudo(b'ceros.bin', b'\0' * (8 << 20))], 'ceros.zip')
print('casos hechos en', S)
