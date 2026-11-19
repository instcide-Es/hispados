# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/pantalla.py < salida > líneas
# Lo que sale por el puerto serie de Hispa-Pentium lleva los retrocesos (el
# octeto 8) de cuando se corrige la línea de órdenes. Esto deja cada línea
# como se ve en la pantalla: un retroceso mueve el cursor un sitio atrás, y lo
# que viene después se escribe encima.
import sys

for linea in sys.stdin.read().split('\n'):
    queda = []
    cursor = 0
    for letra in linea:
        if letra == '\x08':
            cursor = max(0, cursor - 1)
        else:
            if cursor < len(queda):
                queda[cursor] = letra
            else:
                queda.append(letra)
            cursor += 1
    print(''.join(queda).rstrip())
