#!/usr/bin/env python3
"""La página de códigos de Hispa Ventanas: la 437 con letras de más.

  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.

La letra de Hispa Ventanas (ventanas/fuente.txt) y los tipos de los programas
de oficina (TIPOS*.DAT) son de la página 437, que no tiene Á, Í, Ó, Ú, À, È,
Ò, ni el euro. Esas letras se ponen en códigos de la 437 que en Ventanas no se
pintan nunca: los recuadros que mezclan línea sencilla y doble (B5h-D8h), dos
medios bloques (DDh, DEh) y algunos signos griegos y de fórmulas que la letra
de Ventanas no tenía (E2h, E5h, E7h, E8h, F4h, F5h). Los recuadros de siempre
(sencillos y dobles enteros), las tramas y los bloques no se tocan.

SOLO EN VENTANAS: el modo texto (la VGA y la letra de HispaBIOS) sigue siendo
la 437 de siempre. Un texto escrito en Ventanas con «Á» guarda B6h, que en un
programa de modo texto se ve como un recuadro (╢); al revés, un .TXT de DOS
con esos recuadros se ve en Ventanas con estas letras. Ver docs/VENTANAS.md.

Lo usan tools/mkfuente.py, tools/mk1252.py, tools/mkfuentebios.py,
tools/hazfuentes.py y programas/lector/haznombres.py.
"""

# código -> letra (Unicode). Los códigos NO se cambian: hay textos guardados.
EXTRA = {
    0xB5: 'À', 0xB6: 'Á', 0xB7: 'Â', 0xB8: 'Ã',
    0xBD: 'È', 0xBE: 'Ê', 0xC6: 'Ë',
    0xC7: 'Ì', 0xCF: 'Í', 0xD0: 'Î', 0xD1: 'Ï',
    0xD2: 'Ò', 0xD3: 'Ó', 0xD4: 'Ô', 0xD5: 'Õ',
    0xD6: 'Ù', 0xD7: 'Ú', 0xD8: 'Û',
    0xDD: 'ã', 0xDE: 'õ',
    0xE2: 'Œ', 0xE5: 'œ', 0xE7: '©', 0xE8: '®',
    0xF4: '€', 0xF5: '…',
}
DE_LETRA = {v: k for k, v in EXTRA.items()}


def letra(c):
    """el carácter (Unicode) del código c en Ventanas"""
    if c in EXTRA:
        return EXTRA[c]
    return bytes([c]).decode('cp437')


def codigo(ch):
    """el código de Ventanas de un carácter; None si no lo hay (ni lo que
    la 437 tenía en los códigos reutilizados: esos ya no se pintan así)"""
    if ch in DE_LETRA:
        return DE_LETRA[ch]
    try:
        b = ch.encode('cp437')
    except UnicodeEncodeError:
        return None
    if len(b) != 1 or b[0] in EXTRA:
        return None
    return b[0]


if __name__ == '__main__':
    for c, ch in sorted(EXTRA.items()):
        print('%02Xh %s  (en la 437: %s)' % (c, ch, bytes([c]).decode('cp437')))
