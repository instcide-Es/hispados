#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""Crea las imágenes de CD de Hispa-DOS (docs/CD.md):

  python3 tools/mkcd.py        Hispa-386: build/hispados-cd.iso
  python3 tools/mkcd.py 486    Hispa-486: build/hispa486-cd.iso

El CD lleva lo mismo que la imagen USB de su gama (make usb / make usb486:
se sacan sus archivos con mtools), sin INSTALL.BAT ni \\EFI, más:
  \\HISPADOS\\HISPACD.INI  la marca del CD: INSTALAR, arrancando de disquete,
                         busca la unidad que la tiene y copia de ella todo
                         el sistema (sin pedir los demás disquetes)
  \\ARRANQUE.IMG          el disquete de arranque del CD (1,44 MB), que es
                         también la imagen de arranque El Torito, en
                         emulación de disquete (lo que mejor arrancan las
                         BIOS de 1995-2000); se puede grabar en un disquete
                         de verdad para los equipos que no arrancan de CD
  \\LEEMECD.TXT           cómo se usa el CD (cd/LEEMECD.TXT)
El disquete de arranque trae el núcleo, el intérprete, los controladores de
CONFIG.SYS (HSPXMS, HSPEMM, HSPCD y HSPUSB) y el CONFIG.SYS del disquete de
siempre (floppy/CONFIG.SYS, con el primer menú: arrancar sin instalar o
instalar); su AUTOEXEC.BAT busca el CD (la unidad con HISPACD.INI) y lo usa
todo de allí (el de la memoria USB, usb/AUTOEXEC.BAT, con %CD%: en vez de la
letra). Hace falta genisoimage (o mkisofs) y mtools.

Builds the Hispa-DOS CD images (ISO 9660 level 1 + Joliet, El Torito floppy
emulation boot).
"""
import os, re, shutil, subprocess, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mkusb import autoexec486, dos   # (el AUTOEXEC.BAT de Hispa-486)

B = 'build'
# lo que va en el \HISPADOS del disquete de arranque (lo demás se usa del CD)
ARRANQUE = (('HSPXMS.SYS', 'HSPXMS.SYS'), ('HSPEMM.SYS', 'HSPEMM.SYS'),
            ('HSPCD.SYS', 'HSPCD.SYS'), ('HSPUSB.SYS', 'HSPUSB.SYS'))
DATOS = ('ALIAS.INI', 'AYUDA.HLP', 'TECLAS.INI')     # (sistema/: el intérprete los busca en A:)
LETRA = re.compile(r'(?<![A-Za-z0-9%])[A-Z]:\\')

def run(*args, **kw):
    env = dict(os.environ, MTOOLS_SKIP_CHECK='1', MTOOLS_NO_VFAT='1')
    subprocess.run(args, check=True, env=env, stdout=subprocess.DEVNULL, **kw)

def version():
    txt = open(os.path.join('lib', 'version.inc'), encoding='utf-8').read()
    return re.search(r'%define VER_STR "([^"]+)"', txt).group(1)

def autoexec_cd(edicion):
    """El AUTOEXEC.BAT del disquete de arranque del CD: el de la memoria USB
    con %CD%: (la unidad del CD) en vez de su letra, y antes, buscar el CD."""
    txt = open(os.path.join('usb', 'AUTOEXEC.BAT'), encoding='utf-8').read()
    txt = re.sub(r'(?<![A-Za-z0-9%])[A-Z]:\\HISPADOS', r'%CD%:\\HISPADOS', txt)
    txt = re.sub(r'(?m)^PATH .*$', r'PATH %CD%:\\HISPADOS;A:\\HISPADOS', txt)
    busca = ('REM [CD] La unidad del CD de Hispa-DOS: la que tiene \\HISPADOS\\HISPACD.INI.\n'
             'REM Todo se usa de ella (en el disquete solo están los controladores de\n'
             'REM CONFIG.SYS); %CD% es su letra.\n'
             'SET CD=\n'
             'FOR %%U IN (C D E F G H I J K L M N O P Q R S T U V W X Y Z) DO IF EXIST %%U:\\HISPADOS\\HISPACD.INI SET CD=%%U\n'
             'IF "%CD%"=="" GOTO SINCD\n'
             'ECHO El CD de Hispa-DOS está en la unidad %CD%:\n')
    if 'PROMPT $P$G\n' not in txt or '\n:FIN' not in txt:
        sys.exit('mkcd: usb/AUTOEXEC.BAT sin «PROMPT $P$G» o sin «:FIN»: no sé dónde buscar el CD')
    txt = txt.replace('PROMPT $P$G\n', 'PROMPT $P$G\n' + busca, 1)
    sincd = ('GOTO FIN\n'
             ':SINCD\n'
             'ECHO.\n'
             'ECHO No se encuentra el CD de Hispa-DOS. Compruebe que está puesto y que el\n'
             'ECHO lector se ha encontrado (lo dice HSPCD al arrancar) y reinicie.\n')
    i = txt.rindex('\n:FIN')
    txt = txt[:i + 1] + sincd + txt[i + 1:]
    if edicion == '486':
        txt = autoexec486(txt, '%CD%')
    return txt

def raiz_disco(txt):
    """CONFIG.SYS y AUTOEXEC.BAT de la raíz del CD (los que INSTALAR copia al
    disco duro): con C:, la letra que tendrá el disco al arrancar desde él.
    [LETRAS] Los de la imagen USB vienen sin la letra (\\HISPADOS en
    CONFIG.SYS y %ARRANQUE%\\ en AUTOEXEC.BAT, tools/mkusb.py): del CD no se
    arranca con ellos (para eso está el disquete ARRANQUE.IMG), así que aquí
    se les pone la C: del disco duro."""
    txt = txt.replace('%ARRANQUE%\\', 'C:\\')
    txt = re.sub(r'(?<![A-Za-z0-9:\\])\\HISPADOS\\', r'C:\\HISPADOS\\', txt)
    return LETRA.sub(r'C:\\', txt)

def mkarranque(edicion, out, d):
    if os.path.exists(out):
        os.remove(out)
    run('mformat', '-C', '-f', '1440', '-v', 'HISPACD', '-B', os.path.join(B, 'boot.bin'), '-i', out, '::')
    run('mcopy', '-i', out, os.path.join(B, 'KERNEL.SYS'), '::KERNEL.SYS')
    run('mattrib', '-i', out, '+h', '+s', '+r', '::KERNEL.SYS')
    run('mcopy', '-i', out, os.path.join(B, 'COMMAND.COM'), '::COMMAND.COM')
    os.makedirs(d, exist_ok=True)
    for f, txt in (('CONFIG.SYS', open(os.path.join('floppy', 'CONFIG.SYS'), encoding='utf-8').read()),
                   ('AUTOEXEC.BAT', autoexec_cd(edicion))):
        with open(os.path.join(d, f), 'wb') as o:
            o.write(dos(txt))
        run('mcopy', '-i', out, os.path.join(d, f), '::' + f)
    run('mmd', '-i', out, '::HISPADOS')
    for src, dst in ARRANQUE:
        run('mcopy', '-i', out, os.path.join(B, src), '::HISPADOS/' + dst)
    for f in DATOS:
        run('mcopy', '-i', out, os.path.join('sistema', f), '::HISPADOS/' + f)

def main(edicion='386'):
    usb = os.path.join(B, 'hispa486-usb.img' if edicion == '486' else 'hispados-usb.img')
    iso = os.path.join(B, 'hispa486-cd.iso' if edicion == '486' else 'hispados-cd.iso')
    arbol = os.path.join(B, 'cd486' if edicion == '486' else 'cd386')
    trabajo = arbol + '-arranque'
    if not os.path.exists(usb):
        sys.exit('mkcd: falta %s (make %s)' % (usb, 'usb486' if edicion == '486' else 'usb'))
    shutil.rmtree(arbol, ignore_errors=True)
    os.makedirs(arbol)
    run('mcopy', '-s', '-n', '-i', usb + '@@1M', '::/', arbol + '/')
    for f in ('INSTALL.BAT',):
        if os.path.exists(os.path.join(arbol, f)):
            os.remove(os.path.join(arbol, f))
    shutil.rmtree(os.path.join(arbol, 'EFI'), ignore_errors=True)
    for f in ('CONFIG.SYS', 'AUTOEXEC.BAT'):
        p = os.path.join(arbol, f)
        txt = open(p, 'rb').read().decode('cp437')
        open(p, 'wb').write(raiz_disco(txt).encode('cp437'))
    gama = '486' if edicion == '486' else '386'
    with open(os.path.join(arbol, 'HISPADOS', 'HISPACD.INI'), 'wb') as o:
        o.write(dos('[Hispa-DOS]\nCD=1\nGama=%s\nVersion=%s\n' % (gama, version())))
    with open(os.path.join(arbol, 'LEEMECD.TXT'), 'wb') as o:
        o.write(dos(open(os.path.join('cd', 'LEEMECD.TXT'), encoding='utf-8').read()))
    mkarranque(edicion, os.path.join(arbol, 'ARRANQUE.IMG'), trabajo)
    herr = shutil.which('genisoimage') or shutil.which('mkisofs')
    if not herr:
        sys.exit('mkcd: hace falta genisoimage o mkisofs')
    if os.path.exists(iso):
        os.remove(iso)
    # ISO 9660 nivel 1 (nombres 8.3: los lee el núcleo con HSPCD y cualquier
    # MSCDEX) + Joliet (Windows); arranque El Torito con ARRANQUE.IMG en
    # emulación de disquete de 1,44 MB (por su tamaño)
    run(herr, '-quiet', '-iso-level', '1', '-J', '-V', 'HISPA486' if edicion == '486' else 'HISPADOS',
        '-A', 'Hispa-DOS %s' % version(), '-b', 'ARRANQUE.IMG', '-c', 'ARRANQUE.CAT',
        '-hide', 'ARRANQUE.CAT', '-hide-joliet', 'ARRANQUE.CAT', '-o', iso, arbol)
    print(iso)

if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else '386')
