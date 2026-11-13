#!/usr/bin/env python3
"""Crea build/hispados-usb.img: imagen de disco para memorias USB.

MBR de Hispa-DOS + una partición FAT16 activa que empieza en el sector 2048
(alineada a 1 MB). La partición se formatea con mtools usando el sector de
arranque FAT16 de Hispa-DOS. Grábala con Rufus (modo DD), balenaEtcher o dd.

Creates the USB disk image (Hispa-DOS MBR + active FAT16 partition).
"""
import os, struct, subprocess, sys

B = 'build'
OUT = os.path.join(B, 'hispados-usb.img')
SIZE_MB = 64
START = 2048
HEADS, SPT = 255, 63
HVPSIS = ('CONFIG.HVP',)            # [Configuración] .HVP que van en \HISPADOS y no en JUEGOS

def chs(lba):
    c, rem = divmod(lba, HEADS * SPT)
    h, s = divmod(rem, SPT)
    if c > 1023:
        c, h, s = 1023, HEADS - 1, SPT - 1
    return bytes([h, ((s + 1) & 0x3F) | ((c >> 2) & 0xC0), c & 0xFF])

def run(*args):
    env = dict(os.environ, MTOOLS_SKIP_CHECK='1', MTOOLS_NO_VFAT='1')
    subprocess.run(args, check=True, env=env, stdout=subprocess.DEVNULL)

def main():
    total = SIZE_MB * 2048
    psize = total - START
    with open(OUT, 'wb') as f:
        f.truncate(total * 512)
    mbr = bytearray(open(os.path.join(B, 'mbr.bin'), 'rb').read()[:446])
    entry = bytes([0x80]) + chs(START) + bytes([0x06]) + chs(START + psize - 1) \
        + struct.pack('<II', START, psize)
    mbr += entry + bytes(48) + b'\x55\xAA'
    with open(OUT, 'r+b') as f:
        f.write(mbr)
    img = '%s@@%d' % (OUT, START * 512)
    run('mformat', '-i', img, '-T', str(psize), '-h', str(HEADS), '-s', str(SPT),
        '-H', str(START), '-c', '4', '-v', 'HISPADOS', '-B', os.path.join(B, 'boot16.bin'), '::')
    def cp(src, dst):
        run('mcopy', '-o', '-i', img, src, '::' + dst)
    def md(d):
        run('mmd', '-i', img, '::' + d)
    cp(os.path.join(B, 'KERNEL.SYS'), 'KERNEL.SYS')
    run('mattrib', '-i', img, '+h', '+s', '+r', '::KERNEL.SYS')
    cp(os.path.join(B, 'COMMAND.COM'), 'COMMAND.COM')
    os.makedirs(os.path.join(B, 'usb'), exist_ok=True)
    for f in ('AUTOEXEC.BAT', 'CONFIG.SYS', 'INSTALL.BAT', 'LEEME.TXT'):
        txt = open(os.path.join('usb', f), encoding='utf-8').read()
        dst = os.path.join(B, 'usb', f)
        with open(dst, 'wb') as o:
            o.write(txt.replace('\r\n', '\n').replace('\n', '\r\n').encode('cp437'))
        cp(dst, f)
    # el sistema en \HISPADOS: nombres en español (los clásicos en ALIAS.INI)
    md('HISPADOS')
    for src, dst in (('EDIT.COM', 'EDITAR.COM'), ('FDISK.COM', 'PARTIR.COM'),
                     ('FORMAT.COM', 'FORMATEA.COM'), ('SYS.COM', 'SISTEMA.COM'),
                     ('KEYB.COM', 'TECLADO.COM'), ('PLAYWAV.COM', 'SONAR.COM'), ('SONIDO.COM', 'SONIDO.COM'),
                     ('SYSINFO.COM', 'INFO.COM'), ('GESTOR.COM', 'GESTOR.COM'),
                     ('INSTALAR.COM', 'INSTALAR.COM'), ('MOVER.COM', 'MOVER.COM'), ('ATRIBUTO.COM', 'ATRIBUTO.COM'), ('ARBOL.COM', 'ARBOL.COM'), ('BUSCAR.COM', 'BUSCAR.COM'), ('ORDENAR.COM', 'ORDENAR.COM'), ('PAGINAR.COM', 'PAGINAR.COM'), ('ELEGIR.COM', 'ELEGIR.COM'), ('EXPANDIR.COM', 'EXPANDIR.COM'), ('ETIQUETA.COM', 'ETIQUETA.COM'), ('XCOPIAR.COM', 'XCOPIAR.COM'), ('REVISAR.COM', 'REVISAR.COM'),
                     ('ALIAS.INI', 'ALIAS.INI'),
                     ('HSPXMS.SYS', 'HSPXMS.SYS'), ('HSPRAM.SYS', 'HSPRAM.SYS'),
                     ('HSPSND.COM', 'HSPSND.COM'), ('HSPDPMI.COM', 'HSPDPMI.COM'), ('HSPUSB.SYS', 'HSPUSB.SYS'),
                     ('HSPCD.SYS', 'HSPCD.SYS'), ('HSPRATON.COM', 'HSPRATON.COM'),
                     ('VENTANAS.COM', 'VENTANAS.COM')):
        cp(os.path.join(B, src), 'HISPADOS/' + dst)
    # los juegos de Hispa Ventanas (.HVP), en \HISPADOS\JUEGOS
    md('HISPADOS/JUEGOS')
    for f in sorted(os.listdir(B)):
        if f.endswith('.HVP'):
            # [Configuración] los que no son juegos (HVPSIS del Makefile), en \HISPADOS
            cp(os.path.join(B, f), ('HISPADOS/' if f in HVPSIS else 'HISPADOS/JUEGOS/') + f)
    print(OUT)
    mkfloppy()

def mkfloppy():
    """Disquete para usar: el de las pruebas con CONFIG.SYS y AUTOEXEC.BAT
    de uso normal (carpeta floppy/)."""
    import shutil
    out = os.path.join(B, 'hispados-disquete.img')
    shutil.copy(os.path.join(B, 'hispados.img'), out)
    os.makedirs(os.path.join(B, 'floppy'), exist_ok=True)
    for f in ('CONFIG.SYS', 'AUTOEXEC.BAT'):
        txt = open(os.path.join('floppy', f), encoding='utf-8').read()
        dst = os.path.join(B, 'floppy', f)
        with open(dst, 'wb') as o:
            o.write(txt.replace('\r\n', '\n').replace('\n', '\r\n').encode('cp437'))
        run('mcopy', '-o', '-i', out, dst, '::' + f)
    print(out)

if __name__ == '__main__':
    main()
