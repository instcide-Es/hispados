# genera.py - hace los datos fijos de tests/woff (letra.ttf, .woff, .woff2) con
# fontTools, de un recorte de terceros/dejavu/SANS.TTF. No hace falta para la
# prueba (los datos ya están); solo si se quieren rehacer:
#   python3 -m venv /tmp/v && /tmp/v/bin/pip install fonttools brotli
#   /tmp/v/bin/python -I tests/woff/genera.py terceros/dejavu/SANS.TTF tests/woff
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
import sys
from fontTools import subset
from fontTools.ttLib import TTFont
from fontTools.ttLib import woff2
src, dst = sys.argv[1], sys.argv[2]
opt = subset.Options(); opt.hinting = True; opt.layout_features = []; opt.notdef_outline = True
opt.name_IDs = [0, 1, 2, 3, 4, 5, 6]   # (con el copyright: palabras del diccionario de Brotli); opt.drop_tables += ['GSUB', 'GPOS', 'GDEF', 'kern']
f = TTFont(src)
s = subset.Subsetter(opt)
s.populate(text="AaBbÁáÑñ¿?0123 Hola!éÉ@&%ğŁ")
s.subset(f)
# un glifo con OVERLAP_SIMPLE, para el overlapSimpleBitmap de WOFF2
g = f['glyf'][f.getBestCmap()[ord('A')]]
g.flags[0] |= 0x40
# instrucciones (de mentira: no se ejecutan) en un glifo simple y en uno compuesto
from fontTools.ttLib.tables import ttProgram
for nom, bc in (('a', bytes([0xB0, 1, 0x10, 0xB1, 2, 3, 0x2F])), ('Aacute', bytes(range(0xB0, 0xB0 + 1)) + bytes([7, 0x10]))):
    pr = ttProgram.Program(); pr.fromBytecode(bc); f['glyf'][nom].program = pr
# una descripción en inglés (nameID 10, Mac: en ASCII), para que Brotli use su diccionario
f['name'].setName('Information about the development of international standards, available for '
                  'everyone: experience, environment, government, community, technology, performance', 10, 1, 0, 0)
f.save(dst + '/letra.ttf')
f = TTFont(dst + '/letra.ttf'); f.flavor = 'woff'; f.save(dst + '/letra.woff')
f = TTFont(dst + '/letra.ttf'); f.flavor = 'woff2'; f.save(dst + '/letra.woff2')
woff2.compress(dst + '/letra.ttf', dst + '/letra_hmtx.woff2', transform_tables={'glyf', 'loca', 'hmtx'})
# sin transformar glyf (versión 3, «nula»)
woff2.compress(dst + '/letra.ttf', dst + '/letra_nula.woff2', transform_tables=set())
