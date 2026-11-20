# Hace tests/surf/icono.{ttf,woff,woff2}: una letra de iconos de prueba con
# un cuadrado lleno en U+E000 y un triángulo en U+E001 (para la prueba de
# @font-face del Navegador completo: tests/run_surf_pintado_tests.sh). Se
# hace una vez con fontTools (pip install fonttools brotli); los archivos van
# en el repositorio. Copyright (C) 2026 Carlos Sanz y contribuidores.
# GPL-2.0 o posterior.
import sys, os
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen

def cuadrado():
    p = TTGlyphPen(None)
    p.moveTo((100, 0)); p.lineTo((100, 800)); p.lineTo((900, 800)); p.lineTo((900, 0)); p.closePath()
    return p.glyph()

def triangulo():
    p = TTGlyphPen(None)
    p.moveTo((100, 0)); p.lineTo((500, 800)); p.lineTo((900, 0)); p.closePath()
    return p.glyph()

def vacio():
    return TTGlyphPen(None).glyph()

fb = FontBuilder(1000, isTTF=True)
orden = [".notdef", "space", "cuadrado", "triangulo"]
fb.setupGlyphOrder(orden)
fb.setupCharacterMap({0x20: "space", 0xE000: "cuadrado", 0xE001: "triangulo"})
fb.setupGlyf({".notdef": vacio(), "space": vacio(), "cuadrado": cuadrado(), "triangulo": triangulo()})
fb.setupHorizontalMetrics({".notdef": (500, 0), "space": (250, 0), "cuadrado": (1000, 100), "triangulo": (1000, 100)})
fb.setupHorizontalHeader(ascent=1000, descent=0)
fb.setupNameTable({"familyName": "IconoPrueba", "styleName": "Regular"})
fb.setupOS2(sTypoAscender=1000, sTypoDescender=0, usWinAscent=1000, usWinDescent=0)
fb.setupPost()
d = os.path.dirname(os.path.abspath(__file__))
fb.save(os.path.join(d, "icono.ttf"))
from fontTools.ttLib import TTFont
for fl in ("woff", "woff2"):
    t = TTFont(os.path.join(d, "icono.ttf"))
    t.flavor = fl
    t.save(os.path.join(d, "icono." + fl))
