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
SIZE_MB = int(os.environ.get('USB_MB', '512'))   # Carlos 14/11: 512 MB (el .zip apenas crece)
START = 2048
HEADS, SPT = 255, 63
HVPSIS = ('CONFIG.HVP',)            # [Configuración] .HVP que van en \HISPADOS y no en JUEGOS
NESROMS = ('HISPA1.NES',)           # los juegos propios de Hispa-Nes (NESROMS del Makefile), en JUEGOS\NES
SMSROMS = ('BUZO.SMS',)             # y los de Hispa-System (SMSROMS del Makefile), en JUEGOS\SMS
SONS = ('HISPA.WAV', 'CAMPANA.WAV')  # el sonido de prueba y la campanita (SONS del Makefile)

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
    # sectores por clúster: el menor que deje FAT16 por debajo de 65.525 clústeres
    spc = 4
    while psize // spc >= 65000:
        spc *= 2
    run('mformat', '-i', img, '-T', str(psize), '-h', str(HEADS), '-s', str(SPT),
        '-H', str(START), '-c', str(spc), '-v', 'HISPADOS', '-B', os.path.join(B, 'boot16.bin'), '::')
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
                     ('KEYB.COM', 'TECLADO.COM'), ('PLAYWAV.COM', 'SONAR.COM'), ('SONIDO.COM', 'SONIDO.COM'), ('AUDICION.COM', 'AUDICION.COM'),
                     ('SYSINFO.COM', 'INFO.COM'), ('GESTOR.COM', 'GESTOR.COM'),
                     ('EQUIPO.COM', 'EQUIPO.COM'), ('DIAGNOST.BAT', 'DIAGNOST.BAT'),
                     ('INSTALAR.COM', 'INSTALAR.COM'), ('APAGAR.COM', 'APAGAR.COM'), ('REINICIAR.COM', 'REINICIA.COM'), ('MOVER.COM', 'MOVER.COM'), ('ATRIBUTO.COM', 'ATRIBUTO.COM'), ('ARBOL.COM', 'ARBOL.COM'), ('BUSCAR.COM', 'BUSCAR.COM'), ('ORDENAR.COM', 'ORDENAR.COM'), ('PAGINAR.COM', 'PAGINAR.COM'), ('ELEGIR.COM', 'ELEGIR.COM'), ('EXPANDIR.COM', 'EXPANDIR.COM'), ('ETIQUETA.COM', 'ETIQUETA.COM'), ('XCOPIAR.COM', 'XCOPIAR.COM'), ('REVISAR.COM', 'REVISAR.COM'),
                     ('VELOCIDAD.COM', 'VELOCIDA.COM'), ('LOADFIX.COM', 'LOADFIX.COM'), ('PROTECTOR.COM', 'PROTECTO.COM'),
                     ('ALIAS.INI', 'ALIAS.INI'),
                     ('HSPXMS.SYS', 'HSPXMS.SYS'), ('HSPEMM.SYS', 'HSPEMM.SYS'), ('HSPRAM.SYS', 'HSPRAM.SYS'),
                     ('HSPSND.COM', 'HSPSND.COM'), ('HSPDPMI.COM', 'HSPDPMI.COM'), ('HSPUSB.SYS', 'HSPUSB.SYS'),
                     ('HSPCD.SYS', 'HSPCD.SYS'), ('HSPRATON.COM', 'HSPRATON.COM'),
                     ('HSPJOY.COM', 'HSPJOY.COM'), ('MANDO.COM', 'MANDO.COM'), ('MANDO.INI', 'MANDO.INI'),
                     ('VENTANAS.COM', 'VENTANAS.COM'),
                     # la red (docs/RED.md)
                     ('HSPRED.COM', 'HSPRED.COM'), ('CONFRED.COM', 'CONFRED.COM'),
                     ('PING.COM', 'PING.COM'), ('DESCARGAR.COM', 'DESCARGA.COM'),
                     ('RED.INI', 'RED.INI')):
        cp(os.path.join(B, src), 'HISPADOS/' + dst)
    # los programas de Hispa Ventanas (.HVP): los juegos, en \HISPADOS\JUEGOS;
    # los accesorios (los que dicen grupo 1 o 2, «Sonido», en la cabecera), en
    # \HISPADOS\PROGS
    md('HISPADOS/JUEGOS')
    md('HISPADOS/PROGS')
    for f in sorted(os.listdir(B)):
        if f.endswith('.HVP'):
            if f in HVPSIS:
                # [Configuración] los que no son juegos ni accesorios (HVPSIS del Makefile), en \HISPADOS
                cp(os.path.join(B, f), 'HISPADOS/' + f)
            else:
                cab = open(os.path.join(B, f), 'rb').read(32)
                carpeta = 'PROGS' if cab[:4] == b'HVP1' and cab[28] in (1, 2) else 'JUEGOS'  # [MEZCLADOR] 2: «Sonido»
                cp(os.path.join(B, f), 'HISPADOS/%s/%s' % (carpeta, f))
    # los cartuchos de los emuladores, cada uno en su carpeta
    md('HISPADOS/JUEGOS/NES')
    md('HISPADOS/JUEGOS/SMS')
    for f in NESROMS:
        cp(os.path.join(B, f), 'HISPADOS/JUEGOS/NES/' + f)
    for f in SMSROMS:
        cp(os.path.join(B, f), 'HISPADOS/JUEGOS/SMS/' + f)
    for f in SONS:
        cp(os.path.join(B, f), 'HISPADOS/' + f)
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
