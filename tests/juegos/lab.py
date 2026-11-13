#!/usr/bin/env python3
"""Laboratorio de compatibilidad de juegos de Hispa-DOS.

Arranca Hispa-DOS (el disquete de uso de `make dist`) en QEMU con un ISO de
juegos como CD (HSPCD), Sound Blaster 16 + AdLib, teclado y ratón PS/2, y por
cada juego: lo copia a un disco duro de trabajo (C:), lo lanza, pulsa las
teclas típicas (Intro, Espacio...), saca capturas por el monitor de QEMU y
decide si hay imagen, pantalla negra, mensaje de error o vuelta a DOS.

NO forma parte de `make test`: depende de un ISO privado (los juegos no se
distribuyen con Hispa-DOS). La ruta va en la variable JUEGOS_ISO, y la lista
de qué se lanza en cada carpeta (también privada: describe esa colección) en
JUEGOS_TXT; el formato está en tests/juegos/juegos.ejemplo.txt.

  JUEGOS_ISO=/ruta/Juegos.ISO JUEGOS_TXT=/ruta/juegos.txt tests/juegos/lab.sh [opciones] [JUEGO ...]

Opciones (ver --help): --vga std|cirrus, --mem MB, --dpmi, --modo hdd|cd|xcopiar,
--config (líneas extra de CONFIG.SYS), --sal DIR, -j N, --lenta (CPU frenada),
--tarjeta (sonido de la máquina), --sin-sonido/--sin-raton (sin HSPSND/HSPRATON),
--emm (HSPEMM: EMS, VCPI y DOS=UMB) y --lh (residentes en la memoria superior),
--ref IMG (la misma prueba sobre una imagen de disco duro con otro DOS, p. ej.
MS-DOS 6.22, para comparar: si allí falla igual, no es cosa de Hispa-DOS).
Para depurar: --disquete (p. ej. el de `make TRACE=1 dist`: traza de INT 21h en
el .txt), --volcar, --monitor, --gdb, --conserva.

Compatibility lab: boots Hispa-DOS in QEMU with a (private) ISO of DOS games,
launches each one, takes screenshots and classifies the result.
"""
import argparse, os, re, shutil, socket, struct, subprocess, sys, tempfile, time
from concurrent.futures import ThreadPoolExecutor

AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ = os.path.dirname(os.path.dirname(AQUI))

# ---------------------------------------------------------------- ISO 9660
class Iso:
    """Lector mínimo de ISO 9660 (descriptor primario, nombres 8.3)."""
    def __init__(self, ruta):
        self.f = open(ruta, 'rb')
        self.f.seek(16 * 2048)
        pvd = self.f.read(2048)
        if pvd[1:6] != b'CD001':
            raise SystemExit('no es un ISO 9660: ' + ruta)
        self.root = self._rec(pvd[156:190])

    @staticmethod
    def _rec(r):
        lba, size = struct.unpack_from('<I', r, 2)[0], struct.unpack_from('<I', r, 10)[0]
        nl = r[32]
        name = r[33:33 + nl].decode('latin-1').split(';')[0].rstrip('.')
        return {'lba': lba, 'size': size, 'dir': bool(r[25] & 2), 'name': name.upper()}

    def ls(self, d):
        self.f.seek(d['lba'] * 2048)
        data = self.f.read(d['size'])
        out, p = [], 0
        while p < len(data):
            n = data[p]
            if n == 0:                      # relleno hasta el sector siguiente
                p = (p // 2048 + 1) * 2048
                continue
            r = data[p:p + n]
            p += n
            if r[32] == 1 and r[33] in (0, 1):
                continue
            out.append(self._rec(r))
        return out

    def find(self, ruta):
        d = self.root
        for parte in [x for x in ruta.upper().split('/') if x]:
            for e in self.ls(d):
                if e['name'] == parte:
                    d = e
                    break
            else:
                return None
        return d

    def extrae(self, d, destino):
        """Copia el directorio d (recursivo) a destino. Devuelve octetos."""
        os.makedirs(destino, exist_ok=True)
        total = 0
        for e in self.ls(d):
            nom = re.sub(r'[^A-Z0-9_!#$%&\'()@^`{}~.-]', '_', e['name'])
            if e['dir']:
                total += self.extrae(e, os.path.join(destino, nom))
            else:
                self.f.seek(e['lba'] * 2048)
                with open(os.path.join(destino, nom), 'wb') as o:
                    falta = e['size']
                    while falta:
                        b = self.f.read(min(falta, 1 << 20))
                        if not b:
                            break
                        o.write(b)
                        falta -= len(b)
                total += e['size']
        return total

# ------------------------------------------------- qué se lanza en cada juego
NO_LANZAR = re.compile(r'^(INSTAL|SETUP|SETSND|SETSOUND|SNDSETUP|CONFIG|IPXSETUP|SERSETUP|'
                       r'DOS4GW|CRACK|CHEAT|HELP|README|READTHIS|CATALOG|DEALERS|COMMIT|'
                       r'DOSBOX|AUTOEXEC|TRANS|JOY)', re.I)

def lee_mapa(ruta):
    """La lista de la colección: CARPETA[/SUB] = orden que lo lanza [| teclas]
    (teclas propias del juego, p. ej. la del menú de tarjeta gráfica, que se
    pulsan antes de las de siempre). Describe una colección concreta, así que
    NO va en el proyecto: se da con JUEGOS_TXT o --lista (por omisión, el
    fichero .txt que haya junto al ISO con su mismo nombre). El formato está
    en tests/juegos/juegos.ejemplo.txt. Sin lista, todo se adivina."""
    mapa = {}
    if not ruta or not os.path.exists(ruta):
        return mapa
    for l in open(ruta, encoding='utf-8'):
        l = l.split('#')[0].strip()
        if '=' in l:
            k, v = l.split('=', 1)
            mapa[k.strip().upper()] = v.strip()
    return mapa

def adivina(iso, carpeta):
    """Sin entrada en juegos.txt: .BAT que no sea de instalar, luego el
    ejecutable que se llama como la carpeta, luego el más grande."""
    d = iso.find(carpeta)
    cand = [e for e in iso.ls(d) if not e['dir'] and re.search(r'\.(EXE|COM|BAT)$', e['name'])
            and not NO_LANZAR.match(e['name'])]
    if not cand:
        return None
    base = carpeta.split('/')[-1].upper()
    for e in cand:
        if e['name'].rsplit('.', 1)[0] == base[:8]:
            return e['name'].rsplit('.', 1)[0]
    bats = [e for e in cand if e['name'].endswith('.BAT')]
    if bats:
        return bats[0]['name'].rsplit('.', 1)[0]
    return max(cand, key=lambda e: e['size'])['name'].rsplit('.', 1)[0]

# ------------------------------------------------------------- disco e imagen
HEADS, SPT, START = 16, 63, 63

def mt(*args):
    env = dict(os.environ, MTOOLS_SKIP_CHECK='1', MTOOLS_NO_VFAT='1')
    subprocess.run(args, check=True, env=env, stdout=subprocess.DEVNULL)

def haz_disco(img, origen, nombre, octetos):
    """Disco duro FAT16 con una partición y la carpeta del juego en \\NOMBRE."""
    mb = 64 if octetos < 40 << 20 else (128 if octetos < 100 << 20 else 504)
    total = mb * 2048
    psize = total - START
    def chs(lba):
        c, rem = divmod(lba, HEADS * SPT)
        h, s = divmod(rem, SPT)
        if c > 1023:
            c, h, s = 1023, HEADS - 1, SPT - 1
        return bytes([h, ((s + 1) & 0x3F) | ((c >> 2) & 0xC0), c & 0xFF])
    with open(img, 'wb') as f:
        f.truncate(total * 512)
        f.seek(446)
        f.write(bytes([0x80]) + chs(START) + bytes([0x06]) + chs(total - 1)
                + struct.pack('<II', START, psize) + bytes(48) + b'\x55\xAA')
    p = '%s@@%d' % (img, START * 512)
    mt('mformat', '-i', p, '-T', str(psize), '-h', str(HEADS), '-s', str(SPT),
       '-H', str(START), '-c', {64: '4', 128: '4', 504: '16'}[mb], '-v', 'JUEGOS', '::')
    if origen:
        mt('mcopy', '-s', '-i', p, origen, '::' + nombre)

def texto_bat(lineas):
    return ''.join(l + '\r\n' for l in lineas).encode('cp437')

# ------------------------------------------------------------------ QEMU
class Qemu:
    def __init__(self, args, tmp):
        self.sock = os.path.join(tmp, 'mon')
        self.p = subprocess.Popen(args + ['-monitor', 'unix:%s,server,nowait' % self.sock],
                                  stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        for _ in range(50):
            if os.path.exists(self.sock):
                break
            time.sleep(0.1)
        self.s = socket.socket(socket.AF_UNIX)
        self.s.connect(self.sock)
        self.s.settimeout(3)
        self._lee()

    def _lee(self):
        buf = b''
        try:
            while not buf.rstrip().endswith(b'(qemu)'):
                b = self.s.recv(65536)
                if not b:
                    break
                buf += b
        except socket.timeout:
            pass
        return buf.decode('latin-1')

    def mon(self, orden):
        self.s.sendall(orden.encode() + b'\n')
        return self._lee()

    def tecla(self, t):
        self.mon('sendkey ' + t)
        time.sleep(0.15)

    def escribe(self, txt, es=False):
        m = {' ': 'spc', '\n': 'ret', '.': 'dot', '\\': 'backslash', ':': 'shift-semicolon',
             '-': 'minus', '/': 'slash', '_': 'shift-minus', '=': 'equal'}
        if es:                              # con KEYB SP (teclado español)
            m.update({':': 'shift-dot', '-': 'slash', '/': 'shift-7', '_': 'shift-slash',
                      '=': 'shift-0', '\\': 'alt_r-grave_accent'})
        for c in txt:
            self.tecla(m.get(c, c.lower() if not c.isupper() else 'shift-' + c.lower()))

    def mem(self, dir_, n, fich):
        self.mon('pmemsave %d %d "%s"' % (dir_, n, fich))
        for _ in range(20):
            if os.path.exists(fich) and os.path.getsize(fich) == n:
                break
            time.sleep(0.05)
        return open(fich, 'rb').read()

    def fin(self):
        try:
            self.mon('quit')
        except Exception:
            pass
        try:
            self.p.wait(timeout=5)
        except Exception:
            self.p.kill()

# -------------------------------------------------------------- las capturas
def lee_ppm(f):
    d = open(f, 'rb').read()
    m = re.match(rb'P6\s+(\d+)\s+(\d+)\s+(\d+)\s', d)
    w, h = int(m.group(1)), int(m.group(2))
    return w, h, d[m.end():]

def analiza_ppm(f):
    """(ancho, alto, nº de colores, fracción de puntos que no son del color
    más frecuente). Pantalla negra = un solo color (o casi)."""
    w, h, px = lee_ppm(f)
    cuenta = {}
    paso = 3 * 7                           # uno de cada 7 puntos: basta
    for i in range(0, len(px) - 2, paso):
        c = px[i:i + 3]
        cuenta[c] = cuenta.get(c, 0) + 1
    n = sum(cuenta.values())
    mayor = max(cuenta.values())
    return w, h, len(cuenta), 1 - mayor / n

def pantalla_texto(b8, cols=80, filas=25):
    lineas = []
    for y in range(filas):
        fila = b8[y * cols * 2:(y + 1) * cols * 2:2]
        lineas.append(bytes(c if c >= 32 else 32 for c in fila).decode('cp437').rstrip())
    while lineas and not lineas[-1]:
        lineas.pop()
    return lineas

ERRORES = re.compile(
    r'(runtime error[^\n]*|not enough memory[^\n]*|insufficient memory[^\n]*|out of memory[^\n]*|'
    r'memoria insuficiente[^\n]*|no hay (suficiente )?memoria[^\n]*|abnormal program termination[^\n]*|'
    r'[^\n]*EMS[^\n]*|[^\n]*expanded memory[^\n]*|[^\n]*DPMI[^\n]*|[^\n]*DOS/4GW? (error|fatal)[^\n]*|'
    r'[^\n]*(error|fallo|cannot|can\'t|unable|not found|no se (puede|encuentra)|failed|'
    r'missing|invalid|incorrect|requires|necesita|excepci[oó]n|exception|divide|overflow|'
    r'no v[aá]lid|desconocid|protecci[oó]n|page fault|general protection)[^\n]*)', re.I)

def mide_wav(f, umbral=400):
    """[SONIDO-JUEGOS] (segundos con sonido, pico) de una grabación de QEMU
    (-audiodev wav). Con sonido = ventanas de 10 ms cuyo pico pasa del umbral
    (sobre 32767) una vez quitada la componente continua de la ventana (hay
    tarjetas que en reposo dejan un nivel fijo, que no suena)."""
    try:
        d = open(f, 'rb').read()
    except OSError:
        return 0.0, 0
    if len(d) < 46 or d[:4] != b'RIFF':
        return 0.0, 0
    ch, rate = struct.unpack_from('<HI', d, 22)
    bits = struct.unpack_from('<H', d, 34)[0]
    if bits != 16 or not ch or not rate:
        return 0.0, 0
    cuerpo = d[44:len(d) - (len(d) - 44) % (2 * ch)]
    v = struct.unpack('<%dh' % (len(cuerpo) // 2), cuerpo)[::ch]
    win = max(rate // 100, 1)
    con, pico = 0, 0
    for i in range(0, len(v) - win + 1, win):
        seg = v[i:i + win]
        m = sum(seg) // win
        p = max(max(seg) - m, m - min(seg))
        if p > umbral:
            con += 1
        pico = max(pico, p)
    return con / 100.0, pico

def prueba(juego, orden, a, iso, sal):
    """Una pasada con un juego. Devuelve un dict con el resultado."""
    nombre = juego.replace('/', '_')
    tmp = tempfile.mkdtemp(prefix='lab_%s_' % nombre, dir=a.tmp)
    res = {'juego': juego, 'orden': orden.split('|')[0].strip(), 'estado': '?', 'detalle': '', 'capturas': []}
    q = None
    try:
        d = iso.find(juego)
        if d is None:
            res['estado'] = 'no está en el ISO'
            return res
        carpeta = juego.split('/')[-1].upper()[:8]
        hdd = os.path.join(tmp, 'c.img')
        if a.modo == 'hdd':
            n = iso.extrae(d, os.path.join(tmp, 'x', carpeta))
            haz_disco(hdd, os.path.join(tmp, 'x', carpeta), carpeta, n)
            shutil.rmtree(os.path.join(tmp, 'x'))
        else:
            haz_disco(hdd, None, None, 400 << 20 if a.modo == 'xcopiar' else 0)
        fl = os.path.join(tmp, 'a.img')
        shutil.copy(a.disquete, fl)
        # CONFIG.SYS: el del disquete de uso + líneas pedidas
        cfg = subprocess.run(['mtype', '-i', fl, '::CONFIG.SYS'], capture_output=True).stdout
        if a.sin_xms:
            cfg = b'\r\n'.join(l for l in cfg.split(b'\r\n') if b'HSPXMS' not in l)
        if a.emm:
            # HSPEMM (EMS, VCPI) y memoria superior: las dos líneas que el
            # disquete de uso trae con REM
            cfg = cfg.replace(b'REM DEVICE=A:\\HISPADOS\\HSPEMM.SYS', b'DEVICE=A:\\HISPADOS\\HSPEMM.SYS')
            cfg = cfg.replace(b'REM DOS=UMB', b'DOS=UMB')
            if b'\nDEVICE=A:\\HISPADOS\\HSPEMM.SYS' not in cfg:
                raise RuntimeError('el CONFIG.SYS del disquete no trae HSPEMM')
        for l in a.config or []:
            cfg += l.encode('cp437') + b'\r\n'
        open(os.path.join(tmp, 'CONFIG.SYS'), 'wb').write(cfg)
        mt('mcopy', '-o', '-i', fl, os.path.join(tmp, 'CONFIG.SYS'), '::CONFIG.SYS')
        cd = 'D:\\' + juego.upper().replace('/', '\\')
        bat = ['@ECHO OFF', 'PROMPT $P$G', 'PATH A:\\HISPADOS', 'TECLADO SP']
        if not a.sin_raton:
            bat.append(('LH ' if a.lh else '') + 'A:\\HISPADOS\\HSPRATON')
        if a.joy:                           # [SONIDO-JUEGOS] como el disquete de uso
            bat.append('A:\\HISPADOS\\HSPJOY')
        if not a.sin_sonido:
            bat.append(('LH ' if a.lh else '') + 'A:\\HISPADOS\\HSPSND /VOL:80' + (' /SB' if a.sb else ''))
        if a.dpmi:
            bat.append('A:\\HISPADOS\\HSPDPMI')
        bat += a.antes or []
        if a.modo == 'hdd':
            bat += ['C:', 'CD \\' + carpeta]
        elif a.modo == 'xcopiar':
            bat += ['XCOPIAR %s C:\\%s /S /E /I' % (cd, carpeta), 'C:', 'CD \\' + carpeta]
        else:
            bat += ['D:', 'CD ' + cd[2:]]
        bat += ['MEM', 'ECHO LAB-INICIO', orden.split('|')[0].strip(), 'ECHO LAB-FIN nivel de error:', 'IF ERRORLEVEL 1 ECHO LAB-ERRORLEVEL']
        open(os.path.join(tmp, 'AUTOEXEC.BAT'), 'wb').write(texto_bat(bat))
        mt('mcopy', '-o', '-i', fl, os.path.join(tmp, 'AUTOEXEC.BAT'), '::AUTOEXEC.BAT')
        log = os.path.join(tmp, 'serie.log')
        vga = a.vga if a.vga.startswith('std') else a.vga.split(',')[0]
        args = [a.qemu, '-display', 'none', '-M', 'pc', '-m', str(a.mem), '-vga', vga,
                '-rtc', 'base=localtime', '-parallel', 'none', '-no-reboot',
                '-audiodev', ('wav,id=snd,path=' + a.wav) if a.wav else 'none,id=snd',
                '-machine', 'pcspk-audiodev=snd',
                '-serial', 'file:' + log]
        # [SONIDO-JUEGOS] --audio: lo que sale por la Sound Blaster (voz
        # digital) y por la AdLib (música FM) se graba aparte y se mide
        wavs = {}
        def voz(nombre):
            if not a.audio:
                return 'snd'
            wavs[nombre] = os.path.join(tmp, nombre + '.wav')
            args.extend(['-audiodev', 'wav,id=w%s,path=%s' % (nombre, wavs[nombre])])
            return 'w' + nombre
        if 'sb16' in a.tarjeta:
            args += ['-device', 'sb16,audiodev=' + voz('sb')]
        if 'adlib' in a.tarjeta:
            args += ['-device', 'adlib,audiodev=' + voz('fm')]
        if 'ac97' in a.tarjeta:
            args += ['-device', 'AC97,audiodev=snd']
        if 'hda' in a.tarjeta:
            args += ['-device', 'intel-hda', '-device', 'hda-duplex,audiodev=snd']
        if a.ref:
            # otro DOS (p. ej. MS-DOS 6.22) instalado en una imagen de disco
            # duro, que no se toca (-snapshot); el juego va en D:
            if os.path.getsize(a.ref) <= 2880 * 1024:
                # un disquete de arranque (el juego queda en C:)
                args += ['-snapshot', '-drive', 'file=%s,format=raw,if=floppy,index=0' % a.ref,
                         '-drive', 'file=%s,format=raw,if=ide,index=0' % hdd,
                         '-drive', 'file=%s,media=cdrom,if=ide,index=2,readonly=on' % a.iso, '-boot', 'a']
            else:
                args += ['-snapshot', '-drive', 'file=%s,format=raw,if=ide,index=0' % a.ref,
                         '-drive', 'file=%s,format=raw,if=ide,index=1' % hdd,
                         '-drive', 'file=%s,media=cdrom,if=ide,index=2,readonly=on' % a.iso, '-boot', 'c']
        else:
            args += ['-drive', 'file=%s,format=raw,if=floppy,index=0' % fl,
                     '-drive', 'file=%s,format=raw,if=ide,index=0' % hdd,
                     '-drive', 'file=%s,media=cdrom,if=ide,index=2,readonly=on' % a.iso, '-boot', 'a']
        if a.kvm:
            args += ['-enable-kvm']
        if a.gdb:
            args += ['-gdb', 'tcp:127.0.0.1:%d' % a.gdb]
        if a.lenta:
            args += ['-icount', 'shift=6']
        args += a.qemu_extra or []
        q = Qemu(args, tmp)

        def serie():
            try:
                return open(log, 'rb').read().decode('cp437', 'replace').replace('\r', '')
            except OSError:
                return ''
        def marcas():
            # con un núcleo de traza (make TRACE=1) la salida va mezclada con
            # «[02xx» y «=02xx]»: se quitan para buscar las marcas
            return re.sub(r'\[[0-9A-F]{4}|=[0-9A-F]{4}C?\]', '', serie())
        def en_indicador():
            # (referencia) ¿pantalla de texto con el indicador D:\...> al final?
            b = q.mem(0xB8000, 4000, os.path.join(tmp, 'b8p'))
            if q.mem(0x449, 1, os.path.join(tmp, 'modop'))[0] != 3:
                return False
            l = [x for x in pantalla_texto(b) if x.strip()]
            return bool(l and re.match(r'^[A-Z]:\\.*>$', l[-1].strip()))
        # esperar a que el AUTOEXEC llegue al juego
        t0 = time.time()
        limite = a.espera_arranque + (600 if a.modo == 'xcopiar' else 0)
        if a.ref:
            time.sleep(a.espera_ref)
            for t in (a.ref_teclas or '').split(','):
                if t:
                    q.tecla(t)
                    time.sleep(1.5)
            unidad = 'c:' if os.path.getsize(a.ref) <= 2880 * 1024 else 'd:'
            entra = ['cd ' + carpeta]
            if a.modo == 'cd':              # desde el CD (con MSCDEX): la unidad siguiente
                unidad = chr(ord(unidad[0]) + 1) + ':'
                entra = ['cd ' + x for x in juego.upper().split('/')]
            for l in [unidad] + entra + (a.antes or []):
                q.escribe(l + '\n', es=not a.ref_us)
                time.sleep(0.8)
            q.escribe(orden.split('|')[0].strip().lower() + '\n', es=not a.ref_us)
        while not a.ref and 'LAB-INICIO' not in marcas() and time.time() - t0 < limite:
            time.sleep(0.5)
        if not a.ref and 'LAB-INICIO' not in marcas():
            res['estado'] = 'no arranca Hispa-DOS'
            res['detalle'] = serie()[-300:]
            return res
        obs = []
        propias = [t.strip() for t in orden.split('|', 1)[1].split(',')] if '|' in orden else []
        pasos = [(a.espera, None)] + [(a.pausa, t) for t in propias + a.teclas.split(',') if t]
        for i, (esp, tecla) in enumerate(pasos):
            if tecla:
                q.tecla(tecla)
            time.sleep(esp)
            ppm = os.path.join(tmp, 's%d.ppm' % i)
            q.mon('screendump "%s"' % ppm)
            for _ in range(30):
                if os.path.exists(ppm) and os.path.getsize(ppm) > 100:
                    break
                time.sleep(0.1)
            time.sleep(0.15)
            bda = q.mem(0x400, 0x100, os.path.join(tmp, 'bda%d' % i))
            b8 = q.mem(0xB8000, 4000, os.path.join(tmp, 'b8_%d' % i))
            reg = q.mon('info registers')
            m = re.search(r'EIP=(\w+).*?HLT=(\d).*?CS =(\w+)', reg, re.S)
            cr0 = re.search(r'CR0=(\w+)', reg)
            try:
                w, h, ncol, frac = analiza_ppm(ppm)
            except Exception:
                w = h = ncol = 0
                frac = 0.0
            modo = bda[0x49]
            o = {'i': i, 'tecla': tecla or '-', 'modo': modo, 'w': w, 'h': h, 'ncol': ncol, 'frac': frac,
                 'eip': (m.group(3) + ':' + m.group(1)) if m else '?', 'hlt': m.group(2) if m else '?',
                 'pm': bool(cr0 and int(cr0.group(1), 16) & 1),
                 'texto': pantalla_texto(b8, bda[0x4A] or 80) if modo in (0, 1, 2, 3, 7) else [],
                 'fin': en_indicador() if a.ref else 'LAB-FIN' in marcas()}
            obs.append(o)
            png = os.path.join(sal, '%s_%d.png' % (nombre, i))
            subprocess.run(['convert', ppm, png], stderr=subprocess.DEVNULL)
            res['capturas'].append(png)
            if o['fin'] and i >= 1:
                break
        res['obs'] = obs
        # para diagnosticar: vectores de interrupción al final y, si se pide
        # (--volcar DIR:N, en hexadecimal), trozos de memoria
        ivt = q.mem(0, 0x400, os.path.join(tmp, 'ivt'))
        vect = ' '.join('%02X=%04X:%04X' % (n, struct.unpack_from('<H', ivt, n * 4 + 2)[0],
                                             struct.unpack_from('<H', ivt, n * 4)[0])
                        for n in (0x08, 0x09, 0x0D, 0x0F, 0x10, 0x13, 0x15, 0x16, 0x1C, 0x21, 0x2F, 0x33, 0x67))
        volc = []
        for v in a.volcar or []:
            d, n = [int(x, 16) for x in v.split(':')]
            b = q.mem(d, n, os.path.join(tmp, 'volc%x' % d))
            volc.append('%06X: %s' % (d, b.hex(' ')))
        if a.monitor:
            for m in a.monitor:
                volc.append('(qemu) %s\n%s' % (m, q.mon(m)))
        s = serie()
        if 'LAB-INICIO' not in s:
            s = marcas() + '\n--- traza ---\n' + s
        tras = s.split('LAB-INICIO', 1)[1] if 'LAB-INICIO' in s else ''
        if a.ref:
            tras = '\n'.join(obs[-1]['texto'])
        res['serie'] = tras
        if a.audio:
            # [SONIDO-JUEGOS] QEMU cierra los .wav (y pone su cabecera) al salir
            q.fin()
            q = None
            res['audio'] = {k: mide_wav(f) for k, f in wavs.items()}
            if a.conserva_wav:
                for k, f in wavs.items():
                    if os.path.exists(f):
                        shutil.copy(f, os.path.join(sal, '%s_%s.wav' % (nombre, k)))
        m = re.search(r'Memoria convencional[^\n]*|[^\n]*libres?[^\n]*', s)
        clasifica(res, obs, tras)
        with open(os.path.join(sal, nombre + '.txt'), 'w', encoding='utf-8') as f:
            f.write('juego: %s\norden: %s\nestado: %s\ndetalle: %s\n\n' %
                    (juego, orden, res['estado'], res['detalle']))
            for o in obs:
                f.write('captura %d tecla=%s modo=%02Xh %dx%d colores=%d frac=%.3f eip=%s hlt=%s pm=%s fin=%s\n'
                        % (o['i'], o['tecla'], o['modo'], o['w'], o['h'], o['ncol'], o['frac'],
                           o['eip'], o['hlt'], o['pm'], o['fin']))
                for l in o['texto']:
                    f.write('    | ' + l + '\n')
            for k, (seg, pico) in sorted(res.get('audio', {}).items()):
                f.write('audio %s: %.2f s con sonido, pico %d\n' % (k, seg, pico))
            f.write('\nvectores: %s\n%s\n' % (vect, '\n'.join(volc)))
            f.write('\n--- puerto serie (todo) ---\n' + s)
        return res
    except Exception as e:                  # que un juego no pare a los demás
        res['estado'] = 'fallo del laboratorio'
        res['detalle'] = repr(e)
        return res
    finally:
        if q:
            q.fin()
        if not a.conserva:
            shutil.rmtree(tmp, ignore_errors=True)

def clasifica(res, obs, tras):
    ult = obs[-1]
    fin = ult['fin']
    graf = [o for o in obs if o['modo'] not in (0, 1, 2, 3, 7)]
    con_imagen = [o for o in obs if o['frac'] > 0.01 and o['ncol'] > 2]
    cambia = len({(o['w'], o['h'], o['ncol'], round(o['frac'], 3)) for o in obs}) > 1
    # lo que ha escrito el juego (por DOS: sale en el puerto serie; por BIOS
    # o directo a la memoria de vídeo: en la pantalla de texto)
    txt = tras.split('LAB-FIN')[0]
    pant = '\n'.join(ult['texto'])
    err = ERRORES.search(txt) or (ERRORES.search(pant) if not graf or fin else None)
    msg = err.group(0).strip()[:110] if err else ''
    if fin:
        ultimas = [l for l in txt.strip().split('\n') if l.strip()][-3:]
        det = msg or ' / '.join(l.strip()[:70] for l in ultimas) or '(sin mensaje)'
        if graf and len(con_imagen) >= 2 and not msg:
            res['estado'] = 'arranca y sale'
        elif graf and not msg:
            res['estado'] = 'vuelve a DOS'
        else:
            res['estado'] = 'error' if msg else 'vuelve a DOS'
        res['detalle'] = det
    elif graf:
        # negro: un solo color, o el aviso de QEMU «Guest has not initialized
        # the display» (dos colores y casi nada escrito: la tarjeta está apagada)
        if not [o for o in graf if o['frac'] > 0.002 and not (o['ncol'] <= 2 and o['frac'] < 0.01)]:
            res['estado'] = 'pantalla negra'
            res['detalle'] = 'modo %02Xh, sin imagen en %d capturas; %s' % (
                ult['modo'], len(graf), 'cs:eip ' + ult['eip'])
        else:
            res['estado'] = 'arranca'
            res['detalle'] = 'modo %02Xh, %d colores%s' % (ult['modo'], max(o['ncol'] for o in graf),
                                                         '' if cambia else ' (imagen fija)')
    else:
        # sigue en modo texto y no ha vuelto
        if msg:
            res['estado'] = 'error'
            res['detalle'] = msg
        elif cambia or ult['ncol'] > 2 and ult['frac'] > 0.01:
            res['estado'] = 'texto'
            res['detalle'] = (' / '.join(l.strip()[:60] for l in ult['texto'] if l.strip())[-150:]
                              or 'pantalla de texto vacía')
            if not [l for l in ult['texto'] if l.strip()]:
                res['estado'] = 'pantalla negra'
                res['detalle'] = 'modo texto, pantalla vacía; cs:eip ' + ult['eip']
        else:
            res['estado'] = 'pantalla negra'
            res['detalle'] = 'modo texto, sin cambios; cs:eip ' + ult['eip']

def main():
    ap = argparse.ArgumentParser(description='Laboratorio de juegos de Hispa-DOS')
    ap.add_argument('juegos', nargs='*', help='carpetas del ISO (todas si no se dice)')
    ap.add_argument('--iso', default=os.environ.get('JUEGOS_ISO'))
    ap.add_argument('--lista', default=os.environ.get('JUEGOS_TXT'),
                    help='qué se lanza en cada carpeta (ver juegos.ejemplo.txt); va fuera del proyecto')
    ap.add_argument('--disquete', default=os.path.join(RAIZ, 'build', 'hispados-disquete.img'))
    ap.add_argument('--vga', default='std,retrace=precise',
                    help='tarjeta gráfica de QEMU (std, cirrus...). «retrace=precise» hace que el\n'
                         'retrazado vertical del puerto 3DAh sea de verdad: sin él Wolf3D y otros\n'
                         'se quedan en negro esperándolo (también sobre MS-DOS): es cosa de QEMU')
    ap.add_argument('--lenta', action='store_true',
                    help='CPU frenada (-icount shift=6, unos 15 MIPS, como un 486): hay juegos que\n'
                         'en una CPU rápida dan «divide by 0» (R6003) o se quedan en negro')
    ap.add_argument('--mem', type=int, default=32)
    ap.add_argument('--modo', choices=('hdd', 'cd', 'xcopiar'), default='hdd',
                    help='hdd: el laboratorio copia el juego a C: (rápido); cd: se lanza desde '
                         'el CD; xcopiar: lo copia Hispa-DOS con XCOPIAR')
    ap.add_argument('--dpmi', action='store_true', help='cargar HSPDPMI antes del juego')
    ap.add_argument('--sin-xms', action='store_true')
    ap.add_argument('--emm', action='store_true',
                    help='cargar HSPEMM (EMS, VCPI) y DOS=UMB: quita el REM de esas líneas del disquete')
    ap.add_argument('--lh', action='store_true', help='con --emm: HSPRATON y HSPSND con LH')
    ap.add_argument('--sin-sonido', action='store_true', help='no cargar HSPSND')
    ap.add_argument('--tarjeta', default='sb16,adlib',
                    help='tarjetas de sonido de la máquina: sb16, adlib, ac97, hda (o «no»)')
    ap.add_argument('--sb', action='store_true',
                    help='HSPSND /SB: Sound Blaster emulada sobre HD Audio o AC\'97 (con --emm y\n'
                         '--tarjeta hda o ac97, sin sb16)')
    ap.add_argument('--wav', help='grabar en este .wav lo que suena (con un solo juego)')
    ap.add_argument('--sin-raton', action='store_true')
    ap.add_argument('--joy', action='store_true', help='cargar también HSPJOY (como el disquete de uso)')
    ap.add_argument('--audio', action='store_true',
                    help='grabar (-audiodev wav) lo que sale por la Sound Blaster y por la AdLib y\n'
                         'medirlo: columnas «sb» y «fm» = segundos con sonido y pico')
    ap.add_argument('--conserva-wav', action='store_true', help='con --audio: dejar los .wav en --sal')
    ap.add_argument('--config', action='append', help='línea extra de CONFIG.SYS')
    ap.add_argument('--antes', action='append', help='orden extra en AUTOEXEC antes del juego')
    ap.add_argument('--orden', help='orden a lanzar (con un solo juego)')
    ap.add_argument('--teclas', default='ret,spc,ret,ret,spc',
                    help='teclas (nombres de sendkey) que se pulsan, una por captura')
    ap.add_argument('--espera', type=float, default=10, help='segundos antes de la 1.ª captura')
    ap.add_argument('--pausa', type=float, default=4, help='segundos tras cada tecla')
    ap.add_argument('--espera-arranque', type=float, default=90)
    ap.add_argument('--sal', default=None, help='carpeta de resultados')
    ap.add_argument('--tmp', default=os.environ.get('LAB_TMP'))
    ap.add_argument('-j', type=int, default=2)
    ap.add_argument('--kvm', action='store_true')
    ap.add_argument('--gdb', type=int, default=0, help='puerto para gdb (con un solo juego)')
    ap.add_argument('--ref', help='imagen de disco duro con otro DOS (MS-DOS 6.22...) para '
                                  'comparar: arranca de ella (sin tocarla), el juego en D:, '
                                  'y las órdenes se teclean')
    ap.add_argument('--ref-us', action='store_true', help='la referencia tiene teclado de EE. UU.')
    ap.add_argument('--ref-teclas', help='teclas que hay que pulsar al arrancar la referencia '
                                         '(p. ej. f3,f3 para salir de un programa de instalación)')
    ap.add_argument('--espera-ref', type=float, default=25, help='segundos que tarda en arrancar')
    ap.add_argument('--volcar', action='append', help='DIR:N (hex): volcar memoria al final')
    ap.add_argument('--monitor', action='append', help='orden del monitor de QEMU al final')
    ap.add_argument('--conserva', action='store_true', help='no borrar los discos de trabajo')
    ap.add_argument('--qemu', default=os.environ.get('QEMU', 'qemu-system-i386'))
    ap.add_argument('--qemu-extra', action='append')
    a = ap.parse_args()
    if not a.iso or not os.path.exists(a.iso):
        raise SystemExit('Diga dónde está el ISO de juegos: JUEGOS_ISO=/ruta/Juegos.ISO (o --iso)')
    if not os.path.exists(a.disquete):
        raise SystemExit('falta %s (make dist)' % a.disquete)
    propio = a.tmp is None
    if propio:
        a.tmp = tempfile.mkdtemp(prefix='hispalab_')
    os.makedirs(a.tmp, exist_ok=True)
    sal = a.sal or os.path.join(a.tmp, 'resultados')
    os.makedirs(sal, exist_ok=True)
    iso = Iso(a.iso)
    lista = a.lista or os.path.splitext(a.iso)[0] + '.txt'
    if a.lista and not os.path.exists(a.lista):
        raise SystemExit('no encuentro la lista de juegos: ' + a.lista)
    mapa = lee_mapa(lista)
    if a.juegos:
        juegos = [j.upper() for j in a.juegos]
    else:
        juegos = []
        for e in iso.ls(iso.root):
            if not e['dir']:
                continue
            subs = [k for k in mapa if k.startswith(e['name'] + '/')]
            juegos += subs or [e['name']]
    tareas = []
    for j in juegos:
        orden = a.orden or mapa.get(j)
        if orden == '-':
            continue
        if not orden:
            orden = adivina(Iso(a.iso), j) if iso.find(j) else None
        if not orden:
            print('%-16s sin ejecutable que lanzar' % j)
            continue
        tareas.append((j, orden))

    def una(t):
        r = prueba(t[0], t[1], a, Iso(a.iso), sal)
        au = ''.join(' %s=%.1fs/%d' % (k, v[0], v[1]) for k, v in sorted(r.get('audio', {}).items()))
        print('%-16s %-10s %-16s%s %s' % (r['juego'], r['orden'][:10], r['estado'], au, r['detalle'][:120]), flush=True)
        return r
    with ThreadPoolExecutor(max_workers=a.j) as ex:
        resultados = list(ex.map(una, tareas))
    with open(os.path.join(sal, 'tabla.tsv'), 'w', encoding='utf-8') as f:
        f.write('juego\torden\testado\tdetalle%s\n' % ('\tsb_s\tsb_pico\tfm_s\tfm_pico' if a.audio else ''))
        for r in resultados:
            au = ''
            if a.audio:
                x = r.get('audio', {})
                au = '\t%.2f\t%d\t%.2f\t%d' % (x.get('sb', (0, 0)) + x.get('fm', (0, 0)))
            f.write('%s\t%s\t%s\t%s%s\n' % (r['juego'], r['orden'], r['estado'], r['detalle'].replace('\t', ' ').replace('\n', ' '), au))
    cuenta = {}
    for r in resultados:
        cuenta[r['estado']] = cuenta.get(r['estado'], 0) + 1
    print('--', ', '.join('%s: %d' % kv for kv in sorted(cuenta.items())), '-- resultados en', sal)
    if propio and a.sal:
        shutil.rmtree(a.tmp, ignore_errors=True)

if __name__ == '__main__':
    main()
