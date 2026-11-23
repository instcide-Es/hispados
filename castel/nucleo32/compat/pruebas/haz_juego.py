#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""compat/pruebas/haz_juego.py CARPETA

(COMPAT) El «juego de mentira» de las pruebas del Asistente de
compatibilidad (la parte 49): unos PE de 386 hechos aquí, que no hacen nada
(su código es un «ret»), con las importaciones y exportaciones que hacen
falta para probar WIN32 COMPRUEBA y el Asistente. Nada de ningún juego de
verdad. Deja en CARPETA:

  JUEGOMEN/JUEGO.EXE    el programa: KERNEL32, USER32, su MOTOR.DLL y
                        PRUEBAC.DLL (que no hay: la trae el win32 «nuevo» de
                        la prueba, con 3 de sus 5 funciones) y A3D.DLL (opcional)
  JUEGOMEN/MOTOR.DLL    lo suyo: KERNEL32, WINMM y GLIDE2X.DLL (Glide: no importa)
  JUEGOMEN/SETUP.EXE    el instalador (FANTASMA.DLL: no importa)
  JUEGOMEN/HLDS.EXE     el servidor dedicado (FANTASMA.DLL: no importa)
  JUEGOMEN/LEEME.TXT
  TODOBIEN.EXE          uno suelto que solo pide PRUEBAC.DLL!PcUno
  NUEVO/PRUEBAC.DLL     la DLL del win32 «nuevo» (PcUno, PcDos y PcTres)
  W32/WINMM.DLL         (para Linux) lo que hace de \\WIN32\\WINMM.DLL
  REDIST/PRUEBA.EXE     un «redistribuible de Microsoft» de mentira: un
                        ejecutable con un CAB (MSZIP) dentro con LICENCIA.TXT
                        y otro CAB, INTERIOR.CAB, con PRUEBA1.DLL y PRUEBA2.DLL
                        con nombres de Windows Installer
"""
import os
import struct
import sys
import zlib

ALIN = 0x200
SEC = 0x1000


def alinea(n, a):
    return (n + a - 1) // a * a


def haz_pe(importa, exporta=(), nombre_dll=None, dll=False, gui=True):
    """Un PE32 de 386: .text (un «ret»), .idata (las importaciones) y, si
    exporta, .edata. importa: [(DLL, [funciones])]."""
    base_rva = SEC
    secciones = []
    # .text
    texto = b'\x31\xC0\xC3' + b'\xCC' * 13
    secciones.append(['.text', texto, 0x60000020])
    # .idata
    rva_idata = base_rva + SEC
    n = len(importa)
    desc_tam = (n + 1) * 20
    ilts = []
    nombres = b''
    nombres_off = {}
    cuerpo = bytearray()
    # (primero los descriptores; luego las ILT y las IAT; luego los nombres)
    pos = desc_tam
    ilt_off = []
    for dll_, funcs in importa:
        ilt_off.append(pos)
        pos += (len(funcs) + 1) * 4
    iat_off = []
    for dll_, funcs in importa:
        iat_off.append(pos)
        pos += (len(funcs) + 1) * 4
    nom_base = pos
    tabla = bytearray()
    hint = {}
    for dll_, funcs in importa:
        for f in funcs:
            if f not in hint:
                hint[f] = nom_base + len(tabla)
                tabla += b'\0\0' + f.encode() + b'\0'
                if len(tabla) % 2:
                    tabla += b'\0'
    dlln = {}
    for dll_, funcs in importa:
        dlln[dll_] = nom_base + len(tabla)
        tabla += dll_.encode() + b'\0'
    cuerpo = bytearray(nom_base + len(tabla))
    for i, (dll_, funcs) in enumerate(importa):
        struct.pack_into('<IIIII', cuerpo, i * 20, rva_idata + ilt_off[i], 0, 0, rva_idata + dlln[dll_], rva_idata + iat_off[i])
        for k, f in enumerate(funcs):
            struct.pack_into('<I', cuerpo, ilt_off[i] + k * 4, rva_idata + hint[f])
            struct.pack_into('<I', cuerpo, iat_off[i] + k * 4, rva_idata + hint[f])
    cuerpo[nom_base:] = tabla
    secciones.append(['.idata', bytes(cuerpo), 0xC0000040])
    rva_edata = None
    if exporta:
        rva_edata = rva_idata + alinea(len(cuerpo), SEC)
        exp = sorted(exporta)
        ne = len(exp)
        nombre = (nombre_dll or 'X.DLL').encode() + b'\0'
        off_fun = 40
        off_nom = off_fun + ne * 4
        off_ord = off_nom + ne * 4
        off_txt = off_ord + ne * 2
        e = bytearray(off_txt)
        txt = bytearray(nombre)
        punteros = []
        for f in exp:
            punteros.append(off_txt + len(txt))
            txt += f.encode() + b'\0'
        e += txt
        struct.pack_into('<IIHHIIIIIII', e, 0, 0, 0, 0, 0, rva_edata + off_txt, 1, ne, ne, rva_edata + off_fun,
                         rva_edata + off_nom, rva_edata + off_ord)
        for k in range(ne):
            struct.pack_into('<I', e, off_fun + k * 4, base_rva)          # (todas al «ret»)
            struct.pack_into('<I', e, off_nom + k * 4, rva_edata + punteros[k])
            struct.pack_into('<H', e, off_ord + k * 2, k)
        secciones.append(['.edata', bytes(e), 0x40000040])
    # las cabeceras
    nsec = len(secciones)
    cab_tam = alinea(0x80 + 4 + 20 + 224 + nsec * 40, ALIN)
    datos = bytearray()
    rva = base_rva
    filas = []
    off = cab_tam
    for nom, d, car in secciones:
        tam_raw = alinea(len(d), ALIN)
        filas.append((nom, len(d), rva, tam_raw, off, car))
        datos += d + b'\0' * (tam_raw - len(d))
        off += tam_raw
        rva += alinea(len(d), SEC)
    imagen = rva
    mz = bytearray(0x80)
    mz[0:2] = b'MZ'
    struct.pack_into('<I', mz, 0x3C, 0x80)
    coff = struct.pack('<HHIIIHH', 0x14C, nsec, 0x5E000000, 0, 0, 224, 0x0102 | (0x2000 if dll else 0))
    dirs = [(0, 0)] * 16
    if rva_edata:
        dirs[0] = (rva_edata, len(secciones[2][1]))
    dirs[1] = (rva_idata, (len(importa) + 1) * 20)
    opt = struct.pack('<HBBIIIIIIIIIHHHHHHIIIIHHIIIIII', 0x10B, 6, 0, alinea(len(texto), ALIN), 0, 0, base_rva, base_rva, rva_idata,
                      0x10000000 if dll else 0x400000, SEC, ALIN, 4, 0, 0, 0, 4, 0, 0, imagen, cab_tam, 0,
                      2 if gui else 3, 0, 0x100000, 0x1000, 0x100000, 0x1000, 0, 16)
    opt += b''.join(struct.pack('<II', a, b) for a, b in dirs)
    cab = bytearray(mz + b'PE\0\0' + coff + opt)
    for nom, vt, va, tr, pr, car in filas:
        cab += struct.pack('<8sIIIIIIHHI', nom.encode(), vt, va, tr, pr, 0, 0, 0, 0, car)
    cab += b'\0' * (cab_tam - len(cab))
    return bytes(cab + datos)


def haz_cab(archivos):
    """Un CAB (MSCF 1.3) de una carpeta MSZIP, con los bloques de 32 K y lo
    de antes como diccionario (como los hace Microsoft): [(nombre, datos)]."""
    todo = b''.join(d for _, d in archivos)
    bloques = []
    previo = b''
    for i in range(0, max(len(todo), 1), 32768):
        trozo = todo[i:i + 32768]
        c = zlib.compressobj(9, zlib.DEFLATED, -15, zdict=previo[-32768:]) if previo else zlib.compressobj(9, zlib.DEFLATED, -15)
        z = c.compress(trozo) + c.flush(zlib.Z_FINISH)
        bloques.append((b'CK' + z, len(trozo)))
        previo += trozo
    nombres = b''.join(n.encode('latin1') + b'\0' for n, _ in archivos)
    cab_tam = 36
    carpeta_tam = 8
    archivos_tam = sum(16 + len(n) + 1 for n, _ in archivos)
    coff_archivos = cab_tam + carpeta_tam
    coff_datos = coff_archivos + archivos_tam
    datos = b''.join(struct.pack('<IHH', 0, len(z), n) + z for z, n in bloques)
    total = coff_datos + len(datos)
    h = struct.pack('<4sIIIIIBBHHHHH', b'MSCF', 0, total, 0, coff_archivos, 0, 3, 1, 1, len(archivos), 0, 0x1234, 0)
    f = struct.pack('<IHH', coff_datos, len(bloques), 1)
    fs = b''
    desde = 0
    for n, d in archivos:
        fs += struct.pack('<IIHHHH', len(d), desde, 0, 0x5B76, 0x6000, 0x20) + n.encode('latin1') + b'\0'
        desde += len(d)
    return h + f + fs + datos


def escribe(r, d):
    os.makedirs(os.path.dirname(r), exist_ok=True)
    with open(r, 'wb') as f:
        f.write(d)


def main():
    c = sys.argv[1]
    K = ('KERNEL32.DLL', ['GetTickCount', 'ExitProcess', 'Sleep'])
    escribe(os.path.join(c, 'JUEGOMEN', 'JUEGO.EXE'), haz_pe([
        K, ('USER32.DLL', ['MessageBoxA']), ('MOTOR.DLL', ['MotorEmpieza', 'MotorPinta']),
        ('PRUEBAC.DLL', ['PcUno', 'PcDos', 'PcTres', 'PcCuatro', 'PcCinco']), ('A3D.DLL', ['A3dCrea'])]))
    escribe(os.path.join(c, 'JUEGOMEN', 'MOTOR.DLL'), haz_pe([
        ('KERNEL32.DLL', ['GetTickCount']), ('WINMM.DLL', ['timeGetTime']), ('GLIDE2X.DLL', ['grGlideInit', 'grSstWinOpen'])],
        exporta=['MotorEmpieza', 'MotorPinta', 'MotorAcaba'], nombre_dll='MOTOR.DLL', dll=True))
    escribe(os.path.join(c, 'JUEGOMEN', 'SETUP.EXE'), haz_pe([K, ('FANTASMA.DLL', ['Instala', 'Desinstala'])]))
    escribe(os.path.join(c, 'JUEGOMEN', 'HLDS.EXE'), haz_pe([K, ('WSOCK32.DLL', ['socket']), ('FANTASMA.DLL', ['Sirve'])], gui=False))
    escribe(os.path.join(c, 'JUEGOMEN', 'LEEME.TXT'), 'Un juego de mentira para las pruebas del Asistente.\r\n'.encode())
    escribe(os.path.join(c, 'TODOBIEN.EXE'), haz_pe([K, ('PRUEBAC.DLL', ['PcUno'])]))
    escribe(os.path.join(c, 'NUEVO', 'PRUEBAC.DLL'), haz_pe([K], exporta=['PcUno', 'PcDos', 'PcTres'], nombre_dll='PRUEBAC.DLL', dll=True))
    # (para Linux: lo que hace de \\WIN32, con un WINMM.DLL que tiene timeGetTime)
    escribe(os.path.join(c, 'W32', 'WINMM.DLL'), haz_pe([K], exporta=['timeGetTime', 'waveOutOpen'], nombre_dll='WINMM.DLL', dll=True))
    # el «redistribuible» de mentira: un .EXE con un CAB dentro, que lleva
    # la licencia y otro CAB con las DLL (con nombres de Windows Installer)
    d1 = haz_pe([K], exporta=['Prueba1Hola'], nombre_dll='PRUEBA1.DLL', dll=True)
    d2 = haz_pe([K], exporta=['Prueba2Hola'], nombre_dll='PRUEBA2.DLL', dll=True)
    relleno = bytes((i * 7 + 3) & 0xFF for i in range(70000))     # (para que haya más de un bloque de 32 K)
    interior = haz_cab([('F_CENTRAL_prueba1_x86', d1 + relleno), ('F_CENTRAL_prueba2_x86', d2), ('FL_otra_cosa_x86', b'no es una DLL')])
    licencia = ('LICENCIA DE PRUEBA DE HISPA-DOS\r\nEsto no es de Microsoft: es la licencia de mentira del '
                'redistribuible de las pruebas del Asistente.\r\nSe acepta o no se acepta.\r\n').encode()
    exterior = haz_cab([('.\\LICENCIA.TXT', licencia), ('.\\INTERIOR.CAB', interior)])
    sfx = haz_pe([K]) + b'\0' * 333 + exterior + b'(fin del autoextraible de mentira)'
    escribe(os.path.join(c, 'REDIST', 'PRUEBA.EXE'), sfx)


if __name__ == '__main__':
    main()
