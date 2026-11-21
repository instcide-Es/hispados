#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# castel/nucleo32/escudo/prueba/hazprueba.py SALIDA
# Hace la base de firmas pequeña de prueba de Escudo (en el formato de
# ClamAV: SALIDA/FIRMAS/PRUEBA.HDB, .HSB, .MDB, .NDB, .LDB, .FP, .IGN2) y los
# archivos que la prueban (SALIDA/ARCHIVOS: inofensivos, hechos aquí), con
# lo que tiene que decir el motor de cada uno (SALIDA/ESPERADO.TXT: «ARCHIVO
# firma» o «ARCHIVO limpio»). Cada firma mira una cosa del formato: los
# comodines, los desplazamientos, los PE (punto de entrada y secciones), las
# lógicas con sus cuentas y modificadores, ZIP dentro de ZIP, las limpias y
# las que no se dicen (.ign2). Lo usan castel/pruebas.sh (parte 20) y
# nucleo32/escudo/hacer.sh (las pruebas del motor en Linux).
import hashlib
import os
import struct
import sys
import zipfile

SAL = sys.argv[1]
F = os.path.join(SAL, 'FIRMAS')
A = os.path.join(SAL, 'ARCHIVOS')
os.makedirs(F, exist_ok=True)
os.makedirs(A, exist_ok=True)
esperado = []
firmas = {k: [] for k in ('HDB', 'HSB', 'MDB', 'NDB', 'LDB', 'FP', 'IGN2')}


def archivo(nombre, datos, firma):
    with open(os.path.join(A, nombre), 'wb') as f:
        f.write(datos)
    esperado.append('%s %s' % (nombre, firma or 'limpio'))
    return datos


def relleno(n, semilla):
    # octetos «de relleno» que no casan con nada (un generador lineal)
    x, r = semilla, bytearray()
    for _ in range(n):
        x = (x * 1103515245 + 12345) & 0x7FFFFFFF
        r.append(0x61 + (x >> 16) % 26)
    return bytes(r)


# ---- las sumas del archivo entero ----
d = archivo('SUMAMD5.BIN', b'Escudo: archivo de prueba por suma MD5\n' + relleno(300, 1), 'Hispa.Prueba.SumaMD5-1')
firmas['HDB'].append('%s:%d:Hispa.Prueba.SumaMD5-1' % (hashlib.md5(d).hexdigest(), len(d)))
d = archivo('SUMA256.BIN', b'Escudo: archivo de prueba por SHA-256\n' + relleno(700, 2), 'Hispa.Prueba.SumaSHA256-1')
firmas['HSB'].append('%s:%d:Hispa.Prueba.SumaSHA256-1' % (hashlib.sha256(d).hexdigest(), len(d)))
d = archivo('SUMA1.BIN', b'Escudo: archivo de prueba por SHA-1 de cualquier tamano\n' + relleno(123, 3), 'Hispa.Prueba.SumaSHA1-1')
firmas['HSB'].append('%s:*:Hispa.Prueba.SumaSHA1-1:73' % hashlib.sha1(d).hexdigest())
# el mismo tamaño que SUMAMD5 pero otro contenido: limpio
archivo('SUMAOTRA.BIN', b'Escudo: archivo de prueba por suma MD5\n' + relleno(300, 99), None)

# ---- los patrones del cuerpo (.ndb) ----
firmas['NDB'].append('Hispa.Prueba.Cuerpo-1:0:*:45736375646f2d50415452c34e')          # «Escudo-PATRÓN» (ó en UTF-8 = C3 B3; aquí C3 4E: no)
archivo('CUERPO.TXT', relleno(5000, 4) + b'Escudo-PATR\xc3N' + relleno(100, 5), 'Hispa.Prueba.Cuerpo-1')
# comodines: ??, nibbles, {n}, {n-m}, *, (aa|bb), !(aa|bb), (aaaa|bbbb)
firmas['NDB'].append('Hispa.Prueba.Comodines-1:0:*:434f4d??44{2}4e3?2d(41|42|43)!(00|01)(4d4d|4e4e){1-4}2a2a*46494e')
archivo('COMODIN.BIN', relleno(999, 6) + b'COMxD12N5-B\x02NN..**' + relleno(2000, 7) + b'FIN', 'Hispa.Prueba.Comodines-1')
archivo('COMODNO.BIN', relleno(999, 6) + b'COMxD12N5-B\x01NN..**' + relleno(2000, 7) + b'FIN', None)  # !(00|01) no deja
# alternativas de largo variable (se desdoblan) y [n-m]
firmas['NDB'].append('Hispa.Prueba.Variantes-1:0:*:5641524941(4e5445|42)[1-3]2121')
archivo('VARIANT.BIN', relleno(300, 8) + b'VARIAB??!!' + relleno(30, 9), 'Hispa.Prueba.Variantes-1')
# negación de un octeto (~HH, del nivel 240: el motor dice 230, así que se deja)
firmas['NDB'].append('Hispa.Prueba.Nivel240-1:0:*:41~0042:240')
# desplazamientos: absoluto, con rango, desde el final
firmas['NDB'].append('Hispa.Prueba.Absoluto-1:0:16,4:4142534f4c55544f')
archivo('ABSOLUTO.BIN', relleno(18, 10) + b'ABSOLUTO' + relleno(100, 11), 'Hispa.Prueba.Absoluto-1')
archivo('ABSNO.BIN', relleno(40, 10) + b'ABSOLUTO' + relleno(100, 11), None)        # fuera del rango
firmas['NDB'].append('Hispa.Prueba.Final-1:0:EOF-12:46494e414c2d4445')
archivo('FINAL.BIN', relleno(4000, 12) + b'FINAL-DE' + relleno(4, 13), 'Hispa.Prueba.Final-1')
# una que el motor no sabe (objetivo 3, HTML normalizado): no se carga
firmas['NDB'].append('Hispa.Prueba.HTML-1:3:*:3c736372697074')


# ---- un PE pequeño (no hace nada: «ret» en la entrada) ----
def pe(texto, datos):
    sec_al, arch_al = 0x1000, 0x200
    cab = bytearray(0x400)
    cab[0:2] = b'MZ'
    struct.pack_into('<I', cab, 0x3C, 0x80)
    cab[0x80:0x84] = b'PE\0\0'
    struct.pack_into('<HHIIIHH', cab, 0x84, 0x14C, 2, 0, 0, 0, 0xE0, 0x102)
    o = 0x98
    struct.pack_into('<HBBIIIIIII', cab, o, 0x10B, 1, 0, 0x200, 0x200, 0, 0x1000, 0x1000, 0x2000, 0x400000)
    struct.pack_into('<IIHHHHHHIIIIHHIIIIII', cab, o + 32, sec_al, arch_al, 4, 0, 0, 0, 4, 0, 0, 0x3000, 0x400, 0, 3, 0,
                     0x100000, 0x1000, 0x100000, 0x1000, 0, 16)
    s = o + 0xE0
    t = texto.ljust(0x200, b'\0')
    dd = datos.ljust(0x200, b'\0')
    struct.pack_into('<8sIIIIIIHHI', cab, s, b'.text', len(texto), 0x1000, 0x200, 0x400, 0, 0, 0, 0, 0x60000020)
    struct.pack_into('<8sIIIIIIHHI', cab, s + 40, b'.data', len(datos), 0x2000, 0x200, 0x600, 0, 0, 0, 0, 0xC0000040)
    return bytes(cab) + t + dd, dd


texto = b'\xc3' + b'\x90' * 7 + b'ENTRADA-HISPA'
d, _ = pe(texto, b'mas datos de la entrada ' + relleno(40, 30))
archivo('PRUEBA.EXE', d, 'Hispa.Prueba.PuntoEntrada-1')
firmas['NDB'].append('Hispa.Prueba.PuntoEntrada-1:1:EP+8:454e54524144412d4849535041')
# la sección .data, por su suma (.mdb): otro PE con la misma .data y otra entrada
d2, sdatos = pe(b'\xc3' + b'\x90' * 20, b'datos de la seccion de prueba ' + relleno(40, 14))
archivo('SECCION.EXE', d2, 'Hispa.Prueba.Seccion-1')
firmas['MDB'].append('%d:%s:Hispa.Prueba.Seccion-1' % (len(sdatos), hashlib.md5(sdatos).hexdigest()))
# Sx+n: el principio de la sección 1
firmas['NDB'].append('Hispa.Prueba.SeccionX-1:1:S1+0:6461746f73206465206c61')
d4, _ = pe(b'\xc3' + b'\x90' * 25, b'datos de la otra seccion ' + relleno(40, 31))
archivo('SECCX.EXE', d4, 'Hispa.Prueba.SeccionX-1')
# un PE que solo tiene el texto «ENTRADA-HISPA» fuera de la entrada: limpio
d3, _ = pe(b'\xc3' + b'\x90' * 30 + b'ENTRADA-HISPA', b'otros datos ' + relleno(40, 15))
archivo('NOENTRA.EXE', d3, None)
# el mismo patrón que la del PE, pero para «cualquier archivo»: en un .TXT (no es un PE) no casa la de PE
archivo('NOESPE.TXT', relleno(100, 16) + texto + relleno(100, 17), None)

# ---- las lógicas (.ldb) ----
# (0&1)|2 con ::i en la 1; cuentas: 3>2 (más de dos veces)
firmas['LDB'].append('Hispa.Prueba.Logica-1;Engine:51-255,Target:0;(0&1)|(2>2);4c4f4749434130;%s::i;' % b'mayusculas'.hex() + '5a5a5a5a5a')
archivo('LOGICA1.TXT', relleno(50, 18) + b'LOGICA0' + relleno(500, 19) + b'mAyUsCuLaS' + relleno(50, 20), 'Hispa.Prueba.Logica-1')
archivo('LOGICA2.TXT', relleno(50, 18) + b'ZZZZZ' + relleno(10, 1) + b'ZZZZZ' + relleno(10, 2) + b'ZZZZZ', 'Hispa.Prueba.Logica-1')
archivo('LOGNO.TXT', relleno(50, 18) + b'LOGICA0' + relleno(10, 1) + b'ZZZZZ' + relleno(10, 2) + b'ZZZZZ', None)
# ::w (ancho: UTF-16) y ::a (también en ASCII); ::f (palabra entera); con FileSize
firmas['LDB'].append('Hispa.Prueba.Ancho-1;Target:0,FileSize:100-5000;0;50414c414252413532::wa')
archivo('ANCHO.TXT', relleno(60, 21) + 'PALABRA52'.encode('utf-16-le') + relleno(60, 22), 'Hispa.Prueba.Ancho-1')
archivo('ANCHOA.TXT', relleno(60, 21) + b'PALABRA52' + relleno(60, 22), 'Hispa.Prueba.Ancho-1')
archivo('ANCHOGR.TXT', relleno(6000, 21) + b'PALABRA52' + relleno(60, 22), None)        # FileSize
firmas['LDB'].append('Hispa.Prueba.Palabra-1;Target:0;0;656e746572616d656e7465::f')
archivo('PALABRA.TXT', b'una prueba enteramente; nada mas\n', 'Hispa.Prueba.Palabra-1')
archivo('PALNO.TXT', b'una prueba enteramentes; nada mas\n', None)
# una con PCRE: no se lleva (se cuenta como no soportada)
firmas['LDB'].append('Hispa.Prueba.PCRE-1;Target:0;0&1;414243;0/abc[0-9]+/i')
# Container:CL_TYPE_ZIP: solo dentro de un ZIP
firmas['LDB'].append('Hispa.Prueba.EnZip-1;Target:0,Container:CL_TYPE_ZIP;0;534f4c4f2d454e2d5a4950')
archivo('ENZIPNO.TXT', relleno(80, 23) + b'SOLO-EN-ZIP' + relleno(80, 24), None)

# ---- las limpias (.fp) y las que no se dicen (.ign2) ----
d = archivo('FALSO.TXT', relleno(200, 25) + b'Escudo-PATR\xc3N' + b' (esto no es nada)\n', None)
firmas['FP'].append('%s:%d:Hispa.Prueba.Falso-1' % (hashlib.md5(d).hexdigest(), len(d)))
firmas['NDB'].append('Hispa.Prueba.Ignorada-1:0:*:49474e4f5241444f2d5345')
archivo('IGNORA.TXT', relleno(30, 26) + b'IGNORADO-SE' + relleno(30, 27), None)
firmas['IGN2'].append('Hispa.Prueba.Ignorada-1')

# ---- EICAR (el texto de prueba de siempre de los antivirus) y los ZIP ----
EICAR = b'X5O!P%@AP[4\\PZX54(P^)7CC)7}$EICAR-STANDARD-ANTIVIRUS-TEST-FILE!$H+H*'
archivo('EICAR.COM', EICAR, 'Eicar-Test-Signature')
archivo('EICARNO.TXT', b'esto lleva EICAR en medio: ' + EICAR + b' y no es el archivo de prueba', None)


def zipea(nombre, miembros, metodo=zipfile.ZIP_DEFLATED):
    ruta = os.path.join(A, nombre)
    with zipfile.ZipFile(ruta, 'w', metodo) as z:
        for n, datos in miembros:
            z.writestr(n, datos)
    return open(ruta, 'rb').read()


z = zipea('EICAR.ZIP', [('leeme.txt', b'nada\n' * 50), ('prueba/eicar.com', EICAR)])
esperado.append('EICAR.ZIP Eicar-Test-Signature')
zipea('ZIPZIP.ZIP', [('otro.txt', relleno(4000, 28)), ('dentro.zip', z)], zipfile.ZIP_STORED)
esperado.append('ZIPZIP.ZIP Eicar-Test-Signature')
zipea('ENZIP.ZIP', [('a.txt', relleno(80, 23) + b'SOLO-EN-ZIP' + relleno(80, 24))])
esperado.append('ENZIP.ZIP Hispa.Prueba.EnZip-1')
zipea('SUMAZIP.ZIP', [('suma.bin', open(os.path.join(A, 'SUMAMD5.BIN'), 'rb').read())])
esperado.append('SUMAZIP.ZIP Hispa.Prueba.SumaMD5-1')
zipea('LIMPIO.ZIP', [('a.txt', relleno(30000, 29)), ('b.txt', b'hola ' * 3000)])
esperado.append('LIMPIO.ZIP limpio')
archivo('LIMPIO.TXT', 'Hola, esto es un texto normal y corriente.\r\n'.encode() * 40, None)

for k, v in firmas.items():
    with open(os.path.join(F, 'PRUEBA.' + k), 'w') as f:
        f.write('# Escudo: firmas de prueba (formato de ClamAV), de hazprueba.py\n')
        f.write('\n'.join(v) + '\n')
with open(os.path.join(SAL, 'ESPERADO.TXT'), 'w') as f:
    f.write('\n'.join(esperado) + '\n')
print('hazprueba.py: %d archivos y %d firmas en %s' % (len(esperado), sum(len(v) for v in firmas.values()), SAL))
