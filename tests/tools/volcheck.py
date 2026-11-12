#!/usr/bin/env python3
"""Comprueba la sesión de volumen de tests/run_sound_tests.sh.

  volcheck.py registro.txt audio.txt tarjeta volumen_bajo teclas
"""
import re, sys
log = open(sys.argv[1], encoding='utf-8', errors='replace').read()
aud = open(sys.argv[2]).read()
card, low, keys = sys.argv[3], int(sys.argv[4]), sys.argv[5]
ok = True
def check(cond, text):
    global ok
    print("  %s %s" % ("OK  " if cond else "FAIL", text))
    ok = ok and bool(cond)
def part(a, b=None):
    """El trozo del registro entre «ECHO a» y «ECHO b»."""
    i = log.find("ECHO " + a)
    j = log.find("ECHO " + b) if b else len(log)
    return log[i:j] if i >= 0 else ""
def vols(a, b=None):
    return re.findall(r'Volumen: (\d+) %(?:  \[[^\]]*\])?( \(en silencio\))?', part(a, b))
tr = {}
for a, b, h, p in re.findall(r'tramo ([\d.]+)-([\d.]+) s: (\d+) Hz, pico (\d+)', aud):
    for f in (441, 525, 735, 1225, 1575):
        if abs(int(h) - f) < f * 0.05 and float(b) - float(a) > 0.15:
            tr[f] = max(tr.get(f, 0), int(p))
t300 = [float(b) - float(a) for a, b, h, p in re.findall(r'tramo ([\d.]+)-([\d.]+) s: (\d+) Hz, pico (\d+)', aud) if abs(int(h) - 300) < 15]
check(t300 and abs(t300[0] - 0.6) < 0.15, "un WAV de 8000 Hz suena a su frecuencia y dura lo suyo (300 Hz, %s s)" % (["%.2f" % x for x in t300]))
check("Volumen: 100 %" in log and ("Volumen: %d %%" % low) in log, "HSPSND /VOL:n cambia el volumen del residente")
if card == 'sb16':
    check("maestro 60" in log, "SB16: el maestro del mezclador baja con /VOL:20 (QEMU no lo aplica al sonido)")
else:
    hi, lo = tr.get(525, 0), tr.get(735, 0)
    check(hi > 0 and lo > 0 and hi > lo * 1.4, "más amplitud con /VOL:100 que con /VOL:%d (picos %d y %d)" % (low, hi, lo))
want = {'hda': "Códec 0: 1AF4:0032 (en uso)", 'hdasoft': "Volumen por programa", 'hda2': "Códec 2: 1AF4:0022 (en uso)",
        'ac97': "Códec AC'97 8384:7600", 'sb16': "Mezclador: maestro", 'sb2': "Tarjeta: Sound Blaster en 220h"}[card]
check(want in log, "/INFO: " + want)
if card == 'hda':
    for w in ("Pin 03: altavoz, conector (00104010) -> 03>02", "AFG 01, nodos 02-05", "GPIO 0",
              "Amplificador de volumen: nodo 02 (salida), 0 dB=4A de 4A pasos de 1,00 dB"):
        check(w in log, "/INFO: " + w)
check(vols('uno', 'dos') == [(str(low), ''), ('60', ''), ('70', '')], "SONIDO, SONIDO 60 y SONIDO +: %s" % vols('uno', 'dos'))
check(vols('dos', 'tres')[:3] == [('60', ''), ('50', ''), ('50', ' (en silencio)')], "SONIDO - y SONIDO /M: %s" % vols('dos', 'tres')[:3])
check(1225 not in tr, "en silencio no suena (1225 Hz)")
check(vols('dos', 'tres')[3:4] == [('50', '')], "SONIDO /M otra vez devuelve el volumen")
check(1575 in tr, "SONAR /VOL:100 /TONE suena (pico %d)" % tr.get(1575, 0))
check(vols('tres', 'cuatro')[:1] == [('100', '')], "SONAR /VOL:100 deja el volumen puesto")
check("El volumen va de 0 a 100." in log, "SONIDO 150 se rechaza")
check("SONIDO /A     ajuste con las flechas" in log, "SONIDO /? explica la orden")
fin = 'cinco' if keys else None
check(vols('cuatro', fin)[:1] == [('90', '')], "SONIDO /A: dos veces «-» y Esc: %s" % vols('cuatro', fin)[:1])
if keys:
    check(vols('cinco', 'seis') == [('60', '')], "Ctrl+Alt+abajo tres veces: %s" % vols('cinco', 'seis'))
    check(vols('seis', 'siete') == [('70', '')], "Ctrl+Alt+arriba: %s" % vols('seis', 'siete'))
    check(vols('siete', 'ocho') == [('70', ' (en silencio)')], "Ctrl+Alt+Fin silencia: %s" % vols('siete', 'ocho'))
    check(vols('ocho', 'nueve') == [('60', '')], "Ctrl+Alt+Fin otra vez y teclas multimedia de volumen: %s" % vols('ocho', 'nueve'))
    check(vols('nueve')[:1] == [('60', ' (en silencio)')], "tecla multimedia de silencio: %s" % vols('nueve')[:1])
    check("Teclado español instalado." in log and "REM ñ" in log, "TECLADO SP sigue funcionando con las teclas de volumen")
sys.exit(0 if ok else 1)
