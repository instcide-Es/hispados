#!/usr/bin/env python3
"""Crea build/hispados-usb.img: imagen de disco para memorias USB.

MBR de Hispa-DOS + una partición FAT16 activa que empieza en el sector 2048
(alineada a 1 MB). La partición se formatea con mtools usando el sector de
arranque FAT16 de Hispa-DOS. Grábala con Rufus (modo DD), balenaEtcher o dd.

  python3 tools/mkusb.py        Hispa-386: build/hispados-usb.img y los
                                disquetes (de arranque, «Instalación 2» y
                                «Oficina»)
  python3 tools/mkusb.py 486    Hispa-486: build/hispa486-usb.img (lo mismo
                                más el escritorio de Hispa-486 y los programas
                                Hispa32) y el disquete «Hispa-486»
                                (build/hispa486-disquete.img). Ver
                                docs/486/HISPA-486.md, «La imagen».

Creates the USB disk image (Hispa-DOS MBR + active FAT16 partition).
"""
import os, re, struct, subprocess, sys

B = 'build'
OUT = os.path.join(B, 'hispados-usb.img')
OUT486 = os.path.join(B, 'hispa486-usb.img')
# [HISPA-486] los programas de 32 bits de ejemplo, en C:\HISPA32: el nombre en
# build/ (el de sus reglas y sus pruebas) y el que llevan en la imagen
HISPA32 = (('WINCALC.EXE', 'CALC32.EXE'), ('WINHOLA.EXE', 'HOLA32.EXE'),
           ('WINCTL.EXE', 'CTL32.EXE'), ('WINMENU.EXE', 'MENU32.EXE'),
           ('WINGUI.EXE', 'AVISO32.EXE'))
# y lo que la edición pone en \HISPADOS (en sistema/486/, en UTF-8)
SIS486 = ('EDICION.INI', 'VENTANAS.INI', 'CENTRO.INI')
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

def dos(txt):
    """Texto del proyecto (UTF-8, LF) a texto de DOS (página 437, CR+LF)."""
    return txt.replace('\r\n', '\n').replace('\n', '\r\n').encode('cp437')

def autoexec486(txt, letra):
    """[HISPA-486] El AUTOEXEC.BAT de Hispa-486: el de Hispa-386 y, en el
    arranque normal, el escritorio (Hispa Ventanas, que con la clave de
    VENTANAS.INI sale con la barra de tareas y el menú Inicio)."""
    ancla = 'REM %s:\\HISPADOS\\HSPRED /AUTO\n' % letra
    if txt.count(ancla) != 1:
        sys.exit('mkusb: el AUTOEXEC.BAT no tiene la línea «%s»: no sé dónde '
                 'poner Hispa Ventanas' % ancla.strip())
    return txt.replace(ancla, ancla +
        'REM [Hispa-486] El escritorio, con la barra de tareas y el menú Inicio\n'
        '%s:\\HISPADOS\\VENTANAS\n' % letra)

def centro486(en_imagen):
    """[HISPA-486] sistema/486/CENTRO.INI para una imagen que lleva los
    archivos en_imagen (rutas «C:\\...» en mayúsculas): sin los accesos a
    programas que no estén y, si a algún .HVP de JUEGOS o PROGS le falta su
    acceso, con Modulos=0 (Hispa Ventanas lo añade solo al arrancar)."""
    txt = open(os.path.join('sistema', '486', 'CENTRO.INI'), encoding='utf-8').read()
    bloques = re.split(r'(?m)^(?=\[)', txt)
    fuera, progs = [], set()
    for b in bloques:
        m = re.search(r'(?m)^Programa=(.*)$', b)
        if b.startswith('[Acceso]') and m:
            prog = m.group(1).strip().upper()
            if prog.startswith('C:\\') and prog not in en_imagen:
                print('mkusb: CENTRO.INI de Hispa-486: «%s» no está en la imagen: '
                      'se quita su acceso' % prog)
                continue
            progs.add(prog)
        fuera.append(b)
    txt = ''.join(fuera)
    modulos = [f for f in en_imagen if re.match(r'C:\\HISPADOS\\(JUEGOS|PROGS)\\[^\\]+\.HVP$', f)]
    if all(f in progs for f in modulos):
        txt = re.sub(r'(?m)^Modulos=.*$', 'Modulos=%d' % len(modulos), txt)
    else:
        txt = re.sub(r'(?m)^Modulos=.*$', 'Modulos=0', txt)
    return txt

def main(edicion='386'):
    out = OUT486 if edicion == '486' else OUT
    total = SIZE_MB * 2048
    psize = total - START
    with open(out, 'wb') as f:
        f.truncate(total * 512)
    mbr = bytearray(open(os.path.join(B, 'mbr.bin'), 'rb').read()[:446])
    entry = bytes([0x80]) + chs(START) + bytes([0x06]) + chs(START + psize - 1) \
        + struct.pack('<II', START, psize)
    mbr += entry + bytes(48) + b'\x55\xAA'
    with open(out, 'r+b') as f:
        f.write(mbr)
    img = '%s@@%d' % (out, START * 512)
    # sectores por clúster: el menor que deje FAT16 por debajo de 65.525 clústeres
    spc = 4
    while psize // spc >= 65000:
        spc *= 2
    run('mformat', '-i', img, '-T', str(psize), '-h', str(HEADS), '-s', str(SPT),
        '-H', str(START), '-c', str(spc), '-v', 'HISPADOS', '-B', os.path.join(B, 'boot16.bin'), '::')
    en_imagen = set()                # lo copiado, como «C:\\HISPADOS\\...»
    def cp(src, dst):
        run('mcopy', '-o', '-i', img, src, '::' + dst)
        en_imagen.add('C:\\' + dst.replace('/', '\\').upper())
    def md(d):
        run('mmd', '-i', img, '::' + d)
    cp(os.path.join(B, 'KERNEL.SYS'), 'KERNEL.SYS')
    run('mattrib', '-i', img, '+h', '+s', '+r', '::KERNEL.SYS')
    cp(os.path.join(B, 'COMMAND.COM'), 'COMMAND.COM')
    # HispaBIOS: para los equipos que solo arrancan por UEFI (sin CSM); en los
    # que tienen BIOS la memoria arranca igual que siempre por el MBR
    if os.path.exists(os.path.join(B, 'BOOTX64.EFI')):
        md('EFI')
        md('EFI/BOOT')
        for f in ('BOOTX64.EFI', 'BOOTIA32.EFI'):
            cp(os.path.join(B, f), 'EFI/BOOT/' + f)
    dusb = os.path.join(B, 'usb486' if edicion == '486' else 'usb')
    os.makedirs(dusb, exist_ok=True)
    for f in ('AUTOEXEC.BAT', 'CONFIG.SYS', 'INSTALL.BAT', 'LEEME.TXT'):
        txt = open(os.path.join('usb', f), encoding='utf-8').read()
        if edicion == '486' and f == 'AUTOEXEC.BAT':
            txt = autoexec486(txt, 'C')
        dst = os.path.join(dusb, f)
        with open(dst, 'wb') as o:
            o.write(dos(txt))
        cp(dst, f)
    # el sistema en \HISPADOS: nombres en español (los clásicos en ALIAS.INI)
    md('HISPADOS')
    for src, dst in (('EDIT.COM', 'EDITAR.COM'), ('FDISK.COM', 'PARTIR.COM'),
                     ('FORMAT.COM', 'FORMATEA.COM'), ('SYS.COM', 'SISTEMA.COM'),
                     ('KEYB.COM', 'TECLADO.COM'), ('PLAYWAV.COM', 'SONAR.COM'), ('SONIDO.COM', 'SONIDO.COM'), ('AUDICION.COM', 'AUDICION.COM'),
                     ('SYSINFO.COM', 'INFO.COM'), ('GESTOR.COM', 'GESTOR.COM'),
                     ('EQUIPO.COM', 'EQUIPO.COM'), ('DIAGNOST.BAT', 'DIAGNOST.BAT'),
                     ('INSTALAR.COM', 'INSTALAR.COM'), ('APAGAR.COM', 'APAGAR.COM'), ('REINICIAR.COM', 'REINICIA.COM'), ('MOVER.COM', 'MOVER.COM'), ('ATRIBUTO.COM', 'ATRIBUTO.COM'), ('ARBOL.COM', 'ARBOL.COM'), ('BUSCAR.COM', 'BUSCAR.COM'), ('ORDENAR.COM', 'ORDENAR.COM'), ('PAGINAR.COM', 'PAGINAR.COM'), ('ELEGIR.COM', 'ELEGIR.COM'), ('EXPANDIR.COM', 'EXPANDIR.COM'), ('ETIQUETA.COM', 'ETIQUETA.COM'), ('TECLA.COM', 'TECLA.COM'), ('XCOPIAR.COM', 'XCOPIAR.COM'), ('REVISAR.COM', 'REVISAR.COM'),
                     ('VELOCIDAD.COM', 'VELOCIDA.COM'), ('LOADFIX.COM', 'LOADFIX.COM'), ('PROTECTOR.COM', 'PROTECTO.COM'),
                     ('ALIAS.INI', 'ALIAS.INI'), ('AYUDA.HLP', 'AYUDA.HLP'), ('TECLAS.INI', 'TECLAS.INI'),
                     ('HSPXMS.SYS', 'HSPXMS.SYS'), ('HSPEMM.SYS', 'HSPEMM.SYS'), ('HSPRAM.SYS', 'HSPRAM.SYS'),
                     ('HSPSND.COM', 'HSPSND.COM'), ('HSPDPMI.COM', 'HSPDPMI.COM'), ('HSPUSB.SYS', 'HSPUSB.SYS'),
                     ('HSPCD.SYS', 'HSPCD.SYS'), ('HSPRATON.COM', 'HSPRATON.COM'),
                     ('HSPJOY.COM', 'HSPJOY.COM'), ('MANDO.COM', 'MANDO.COM'), ('MANDO.INI', 'MANDO.INI'),
                     ('VENTANAS.COM', 'VENTANAS.COM'),
                     # la red (docs/RED.md)
                     ('HSPRED.COM', 'HSPRED.COM'), ('CONFRED.COM', 'CONFRED.COM'),
                     ('PING.COM', 'PING.COM'), ('DESCARGAR.COM', 'DESCARGA.COM'),
                     ('CONSOLA.COM', 'CONSOLA.COM'), ('ACTUALIZAR.COM', 'ACTUALIZ.COM'),
                     ('RED.INI', 'RED.INI'),
                     # [HISPA-486] el Wi-Fi (docs/WIFI.md): el residente, su
                     # núcleo de 32 bits, la orden WIFI y el microcódigo de
                     # Broadcom con su licencia (archivos aparte, sin tocar)
                     ('HSPWIFI.COM', 'HSPWIFI.COM'), ('HSPWIFI.BIN', 'HSPWIFI.BIN'),
                     ('WIFI.COM', 'WIFI.COM'), ('BCM43XX.FW', 'BCM43XX.FW'),
                     ('BCM43XXH.FW', 'BCM43XXH.FW'), ('BCM43XX.TXT', 'BCM43XX.TXT'),
                     # [HISPA-486] los programas de Windows de consola
                     # (docs/486/HISPA-486.md): HW32 y su MSVCRT.DLL, juntos
                     # (HW32 busca las DLL en su carpeta)
                     ('HW32.COM', 'HW32.COM'), ('MSVCRT.DLL', 'MSVCRT.DLL')):
        cp(os.path.join(B, src), 'HISPADOS/' + dst)
    # los programas de Hispa Ventanas (.HVP): los juegos (grupo 0 en la
    # cabecera), en \HISPADOS\JUEGOS; los demás (1 Accesorios, 2 Sonido,
    # 3 Principal, 4 Productividad...), en \HISPADOS\PROGS.
    # Los de PROGS van EXACTAMENTE los de HVPA del Makefile y EN SU ORDEN (lo
    # pasa el Makefile en la variable HVPA): los grupos que no existen se crean
    # al encontrar su primer módulo, así que el orden decide dónde sale cada
    # grupo. Un accesorio viejo que siga en build/ pero ya no esté en HVPA no se
    # copia (antes se colaba cualquier .HVP que hubiera en build/).
    md('HISPADOS/JUEGOS')
    md('HISPADOS/PROGS')
    hvpa = os.environ.get('HVPA', '').split()
    for f in hvpa:
        cp(os.path.join(B, f), 'HISPADOS/PROGS/' + f)
    # los datos comunes de esos programas (PROGSDAT del Makefile: TIPOS.DAT, los
    # tipos de letra de Productividad), al lado de los .HVP
    for f in os.environ.get('PROGSDAT', '').split():
        cp(f, 'HISPADOS/PROGS/' + os.path.basename(f))
    for f in sorted(os.listdir(B)):
        if f.endswith('.HVP') and f not in hvpa:
            if f in HVPSIS:
                # [Configuración] los que no son juegos ni accesorios (HVPSIS del Makefile), en \HISPADOS
                cp(os.path.join(B, f), 'HISPADOS/' + f)
            else:
                cab = open(os.path.join(B, f), 'rb').read(32)
                if cab[:4] == b'HVP1' and cab[28] == 0:
                    cp(os.path.join(B, f), 'HISPADOS/JUEGOS/' + f)
                elif not hvpa:
                    # (sin HVPA, como antes: por la cabecera)
                    cp(os.path.join(B, f), 'HISPADOS/PROGS/' + f)
                else:
                    print('mkusb: %s no está en HVPA: no se copia' % f)
    # los cartuchos de los emuladores, cada uno en su carpeta
    md('HISPADOS/JUEGOS/NES')
    md('HISPADOS/JUEGOS/SMS')
    for f in NESROMS:
        cp(os.path.join(B, f), 'HISPADOS/JUEGOS/NES/' + f)
    for f in SMSROMS:
        cp(os.path.join(B, f), 'HISPADOS/JUEGOS/SMS/' + f)
    for f in SONS:
        cp(os.path.join(B, f), 'HISPADOS/' + f)
    if edicion == '486':
        # [HISPA-486] los programas Hispa32 y, en \HISPADOS, la edición, la
        # clave del escritorio y el Centro de programas con el grupo «Hispa32»
        md('HISPA32')
        for src, dst in HISPA32:
            cp(os.path.join(B, src), 'HISPA32/' + dst)
        for f, txt in sis486(en_imagen):
            dst = os.path.join(dusb, f)
            with open(dst, 'wb') as o:
                o.write(dos(txt))
            cp(dst, 'HISPADOS/' + f)
        print(out)
        mkdisq486(en_imagen)
        return
    print(out)
    mkfloppy()

def sis486(en_imagen):
    r"""[HISPA-486] (nombre, texto) de lo que la edición pone en \HISPADOS."""
    for f in SIS486:
        if f == 'CENTRO.INI':
            yield f, centro486(en_imagen)
        else:
            yield f, open(os.path.join('sistema', '486', f), encoding='utf-8').read()

def mkdisq486(en_imagen):
    r"""[HISPA-486] El disquete «Hispa-486»: lo que la edición añade a la
    instalación desde disquetes (los de Hispa-386: el de arranque,
    «Instalación 2» y «Oficina»). INSTALAR lo pide el último; trae \HISPA32,
    lo de \HISPADOS de la edición (EDICION.INI, VENTANAS.INI, CENTRO.INI) y el
    AUTOEXEC.BAT que arranca el escritorio. Para el CENTRO.INI vale la lista
    de la imagen USB: con los tres disquetes, el disco duro queda igual."""
    out = os.path.join(B, 'hispa486-disquete.img')
    d = os.path.join(B, 'disq486')
    os.makedirs(d, exist_ok=True)
    if os.path.exists(out):
        os.remove(out)
    run('mformat', '-C', '-f', '1440', '-v', 'HISPA486', '-i', out, '::')
    run('mmd', '-i', out, '::HISPADOS', '::HISPA32')
    txt = autoexec486(open(os.path.join('floppy', 'AUTOEXEC.BAT'), encoding='utf-8').read(), 'A')
    pon = [('AUTOEXEC.BAT', txt, 'AUTOEXEC.BAT')]
    pon += [(f, t, 'HISPADOS/' + f) for f, t in sis486(en_imagen)]
    for f, t, dst in pon:
        with open(os.path.join(d, f), 'wb') as o:
            o.write(dos(t))
        run('mcopy', '-i', out, os.path.join(d, f), '::' + dst)
    for src, dst in HISPA32:
        run('mcopy', '-i', out, os.path.join(B, src), '::HISPA32/' + dst)
    print(out)

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
    mkoficina()
    mkinstal2()

def mkinstal2():
    """[0.9.24] El disquete «Instalación 2»: lo que ya no cabe en el de
    arranque (los programas de Windows de consola de Hispa-486: HW32 y
    MSVCRT.DLL), en \\HISPADOS, como queda en el disco duro. INSTALAR lo pide
    antes que el de Oficina; también se puede copiar a mano con
    XCOPIAR A:\\HISPADOS C:\\HISPADOS."""
    out = os.path.join(B, 'hispados-instal2.img')
    if os.path.exists(out):
        os.remove(out)
    run('mformat', '-C', '-f', '1440', '-v', 'INSTAL2', '-i', out, '::')
    run('mmd', '-i', out, '::HISPADOS')
    for f in ('HW32.COM', 'MSVCRT.DLL'):
        run('mcopy', '-i', out, os.path.join(B, f), '::HISPADOS/' + f)
    print(out)

def mkoficina():
    """[0.9.22] El disquete «Oficina»: la familia de oficina (HVPO del Makefile)
    y sus datos (PROGSDAT), en \\HISPADOS\\PROGS, como quedan en el disco duro.
    INSTALAR lo pide al acabar; también se puede copiar a mano con
    XCOPIAR A:\\HISPADOS C:\\HISPADOS /S."""
    hvpo = os.environ.get('HVPO', '').split()
    if not hvpo:
        return
    out = os.path.join(B, 'hispados-oficina.img')
    if os.path.exists(out):
        os.remove(out)
    run('mformat', '-C', '-f', '1440', '-v', 'OFICINA', '-i', out, '::')
    run('mmd', '-i', out, '::HISPADOS', '::HISPADOS/PROGS')
    for f in hvpo:
        run('mcopy', '-i', out, os.path.join(B, f), '::HISPADOS/PROGS/' + f)
    for f in os.environ.get('PROGSDAT', '').split():
        run('mcopy', '-i', out, f, '::HISPADOS/PROGS/' + os.path.basename(f))
    print(out)

if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else '386')
