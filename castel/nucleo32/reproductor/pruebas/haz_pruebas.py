#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# haz_pruebas.py CARPETA - los archivos de prueba del Reproductor, todos
# hechos aquí (tonos, escalas y acordes sintetizados: nada de música de
# nadie). El WAV de origen lo hace este programa; los MP3, OGG, FLAC, Opus y
# los WAV comprimidos, el ffmpeg del anfitrión (solo como herramienta: lo que
# sale es nuestro). También: un módulo MOD propio (cuatro canales, una onda
# cuadrada y una triangular), una imagen .BIN de CD de música con su hoja
# .CUE, un FLAC entero con su .CUE, listas .M3U y .PLS, y una carátula PNG.
import math, os, struct, subprocess, sys, zlib, base64

D = sys.argv[1]
os.makedirs(D, exist_ok=True)
HZ = 44100


def wav(ruta, muestras, hz=HZ, canales=2):
    datos = b''.join(struct.pack('<h', max(-32768, min(32767, int(round(m))))) for m in muestras)
    with open(ruta, 'wb') as f:
        f.write(b'RIFF' + struct.pack('<I', 36 + len(datos)) + b'WAVEfmt ')
        f.write(struct.pack('<IHHIIHH', 16, 1, canales, hz, hz * canales * 2, canales * 2, 16))
        f.write(b'data' + struct.pack('<I', len(datos)) + datos)


def nota(n):  # n semitonos sobre el la 440
    return 440.0 * 2 ** (n / 12.0)


def escala(seg=10.0, amp=9000):
    """izquierda: una escala (do mayor, 0,5 s cada nota, con su envolvente);
    derecha: un acorde que cambia cada 2 s. Así se ve si un lado cae en el otro."""
    out = []
    notas = [-9, -7, -5, -4, -2, 0, 2, 3]
    acordes = [(-9, -5, -2), (-4, 0, 3), (-2, 2, 5), (-7, -3, 0), (-9, -5, -2)]
    for i in range(int(seg * HZ)):
        t = i / HZ
        k = int(t / 0.5)
        tt = t - k * 0.5
        env = min(1.0, tt / 0.01) * max(0.0, 1.0 - tt / 0.5) ** 0.5
        l = amp * env * math.sin(2 * math.pi * nota(notas[k % 8]) * t)
        a = acordes[int(t / 2) % 5]
        r = amp * 0.4 * sum(math.sin(2 * math.pi * nota(x) * t) for x in a)
        out += [l, r]
    return out


def seno(f, seg, amp=12000, fase=0.0, hz=HZ):
    out = []
    for i in range(int(seg * hz)):
        v = amp * math.sin(2 * math.pi * f * i / hz + fase)
        out += [v, v]
    return out


def ff(*a):
    subprocess.run(['ffmpeg', '-hide_banner', '-loglevel', 'error', '-y'] + list(a), check=True)


def png(ruta, an=64, al=64):
    """una carátula propia: un degradado azul con un disco blanco"""
    filas = b''
    for y in range(al):
        filas += b'\x00'
        for x in range(an):
            dx, dy = x - an / 2, y - al / 2
            dentro = dx * dx + dy * dy < (an / 3) ** 2 and dx * dx + dy * dy > (an / 10) ** 2
            filas += bytes([255, 255, 255]) if dentro else bytes([20, 60 + y * 2, 120 + x])
    def trozo(t, d):
        return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    with open(ruta, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n' + trozo(b'IHDR', struct.pack('>IIBBBBB', an, al, 8, 2, 0, 0, 0)) +
                trozo(b'IDAT', zlib.compress(filas)) + trozo(b'IEND', b''))


def j(n):
    return os.path.join(D, n)


# ---- el origen ----
wav(j('escala.wav'), escala())
png(j('caratula.png'))
meta = ['-metadata', 'title=Escala de prueba', '-metadata', 'artist=Hispa-DOS', '-metadata', 'album=Tonos de Pentium',
        '-metadata', 'date=2026', '-metadata', 'track=3', '-metadata', 'genre=Rock', '-metadata', 'comment=Hecho por haz_pruebas.py',
        '-metadata', 'REPLAYGAIN_TRACK_GAIN=-6.50 dB', '-metadata', 'REPLAYGAIN_TRACK_PEAK=0.400000',
        '-metadata', 'REPLAYGAIN_ALBUM_GAIN=-4.25 dB']
ff('-i', j('escala.wav'), '-c:a', 'libmp3lame', '-b:a', '192k', '-id3v2_version', '3', *meta, j('escala.mp3'))
ff('-i', j('escala.wav'), '-i', j('caratula.png'), '-map', '0', '-map', '1', '-c:a', 'libmp3lame', '-q:a', '2',
   '-c:v', 'copy', '-id3v2_version', '4', '-disposition:v', 'attached_pic', *meta, j('esc_vbr.mp3'))
ff('-i', j('escala.wav'), '-c:a', 'mp2', '-b:a', '192k', j('escala.mp2'))
ff('-i', j('escala.wav'), '-c:a', 'flac', *meta, j('escala.flac'))
ff('-i', j('escala.wav'), '-i', j('caratula.png'), '-map', '0', '-map', '1', '-c:a', 'flac', '-c:v', 'copy',
   '-disposition:v', 'attached_pic', *meta, j('esc_car.flac'))
# (la imagen en un OGG: METADATA_BLOCK_PICTURE, como hacen los programas de etiquetas)
p = open(j('caratula.png'), 'rb').read()
bloque = struct.pack('>I', 3) + struct.pack('>I', 9) + b'image/png' + struct.pack('>I', 0) + struct.pack('>IIII', 64, 64, 24, 0) + struct.pack('>I', len(p)) + p
ff('-i', j('escala.wav'), '-c:a', 'libvorbis', '-q:a', '5', *meta, '-metadata', 'METADATA_BLOCK_PICTURE=' + base64.b64encode(bloque).decode(), j('escala.ogg'))
ff('-i', j('escala.wav'), '-c:a', 'libopus', '-b:a', '96k', j('escala.opus'))
ff('-i', j('escala.wav'), '-c:a', 'adpcm_ima_wav', j('esc_ima.wav'))
ff('-i', j('escala.wav'), '-c:a', 'adpcm_ms', j('esc_ms.wav'))
ff('-i', j('escala.wav'), '-c:a', 'pcm_s24le', j('esc_24.wav'))
ff('-i', j('escala.wav'), '-c:a', 'pcm_f32le', j('esc_f32.wav'))
ff('-i', j('escala.wav'), '-c:a', 'pcm_u8', '-ac', '1', '-ar', '22050', j('esc_8.wav'))
ff('-i', j('escala.wav'), '-ar', '48000', '-c:a', 'libvorbis', '-q:a', '3', j('esc_48.ogg'))
# 5.1 (seis canales) en FLAC: que se mezcle a dos
ff('-i', j('escala.wav'), '-filter_complex', '[0:a]pan=5.1|FL=c0|FR=c1|FC=0.5*c0+0.5*c1|LFE=0*c0|BL=c0|BR=c1[s]', '-map', '[s]', '-c:a', 'flac', j('esc_51.flac'))

# ---- sin huecos: un seno de 1 kHz partido en dos (el corte, a mitad de onda) ----
s = seno(1000, 6.0)
wav(j('seno.wav'), s)
mitad = int(3.0137 * HZ)
wav(j('seno_a.wav'), s[:2 * mitad])
wav(j('seno_b.wav'), s[2 * mitad:])
for x in ('a', 'b'):
    ff('-i', j('seno_%s.wav' % x), '-c:a', 'libmp3lame', '-b:a', '256k', j('seno_%s.mp3' % x))
    ff('-i', j('seno_%s.wav' % x), '-c:a', 'flac', j('seno_%s.flac' % x))
    ff('-i', j('seno_%s.wav' % x), '-c:a', 'libvorbis', '-q:a', '6', j('seno_%s.ogg' % x))

# ---- un CD de música: imagen .BIN (16 bits, estéreo, 44,1 kHz) y su .CUE ----
pistas = [(nota(-9), 4.0), (nota(-5), 3.0), (nota(0), 5.0)]   # la, do#, la+1 (como en el CD de prueba del núcleo)
binario = b''
cue = 'REM hecho por haz_pruebas.py\r\nPERFORMER "Hispa-DOS"\r\nTITLE "Tonos en CD"\r\nFILE "TONOS.BIN" BINARY\r\n'
for k, (f, seg) in enumerate(pistas):
    sect = len(binario) // 2352
    cue += '  TRACK %02d AUDIO\r\n    TITLE "Tono %d"\r\n    INDEX 01 %02d:%02d:%02d\r\n' % (k + 1, k + 1, sect // 4500, sect // 75 % 60, sect % 75)
    d = b''.join(struct.pack('<hh', int(10000 * math.sin(2 * math.pi * f * i / HZ)), int(10000 * math.sin(2 * math.pi * f * 1.5 * i / HZ))) for i in range(int(seg * HZ)))
    d += b'\0' * ((-len(d)) % 2352)
    binario += d
open(j('TONOS.BIN'), 'wb').write(binario)
open(j('TONOS.CUE'), 'w', newline='').write(cue)
# el mismo, como un FLAC entero con su hoja
ff('-f', 's16le', '-ar', '44100', '-ac', '2', '-i', j('TONOS.BIN'), '-c:a', 'flac', j('ALBUM.FLAC'))
open(j('ALBUM.CUE'), 'w', newline='').write(cue.replace('"TONOS.BIN" BINARY', '"ALBUM.FLAC" WAVE'))

# ---- un módulo MOD propio: 4 canales, una cuadrada y una triangular ----
def mod(ruta):
    nombre = b'HISPA PRUEBA'.ljust(20, b'\0')
    largo = 32  # en palabras (64 muestras)
    cuad = bytes([0x60] * 32 + [0xA0] * 32)
    tri = bytes([(x * 8) & 0xFF if x < 16 else (256 - (x - 16) * 8) & 0xFF for x in range(32)] * 2)
    ins = b''
    for i in range(31):
        if i == 0:
            ins += b'cuadrada'.ljust(22, b'\0') + struct.pack('>HBBHH', largo, 0, 48, 0, largo)
        elif i == 1:
            ins += b'triangular'.ljust(22, b'\0') + struct.pack('>HBBHH', largo, 0, 64, 0, largo)
        else:
            ins += b'\0' * 22 + struct.pack('>HBBHH', 0, 0, 0, 0, 1)
    orden = bytes([0, 1, 0, 1] + [0] * 124)
    periodos = [428, 381, 339, 320, 285, 254, 226, 214]  # do re mi fa sol la si do
    pat = b''
    for p in range(2):
        for fila in range(64):
            for c in range(4):
                if c == 0 and fila % 4 == 0:
                    per = periodos[(fila // 4 + p * 2) % 8]
                    pat += struct.pack('>BBBB', 0x00 | (per >> 8), per & 0xFF, 0x10, 0x00)
                elif c == 1 and fila % 8 == 0:
                    per = periodos[(fila // 8) % 8] * 2
                    pat += struct.pack('>BBBB', 0x00 | (per >> 8), per & 0xFF, 0x20, 0x00)
                else:
                    pat += b'\0\0\0\0'
    datos = nombre + ins + bytes([4, 127]) + orden + b'M.K.' + pat + cuad + tri
    open(ruta, 'wb').write(datos)

mod(j('PRUEBA.MOD'))

# ---- listas ----
open(j('lista.m3u'), 'w', newline='').write('#EXTM3U\r\n#EXTINF:10,Hispa-DOS - Escala MP3\r\nescala.mp3\r\n#EXTINF:10,Escala FLAC\r\nescala.flac\r\nhttp://radio.example/flujo\r\nescala.ogg\r\n')
open(j('lista.pls'), 'w', newline='').write('[playlist]\r\nFile1=escala.wav\r\nTitle1=La escala en WAV\r\nLength1=10\r\nFile2=PRUEBA.MOD\r\nNumberOfEntries=2\r\nVersion=2\r\n')
print('haz_pruebas.py: hecho en', D)
