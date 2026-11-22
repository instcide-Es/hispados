/* sistema.c - (JUEGOS) las llamadas al sistema de mentira, para probar en el
 * anfitrión lo de los juegos (pruebas.c): el reloj avanza a mano, el sonido
 * se apunta en un búfer y lo demás no hace nada.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "pentium.h"

uint32_t tics_falsos;
int16_t *sonido_grabado;
size_t sonido_n, sonido_cabe;
static uint32_t en_cola;

uint32_t sc(uint32_t n, uint32_t a, uint32_t b, uint32_t c)
{
	switch (n) {
	case SC_TICS: return tics_falsos;
	case SC_DATO: return a == DATO_HZ ? 100 : 0;
	case SC_SERIE: fwrite((const char *)(uintptr_t)a, 1, b, stdout); return 0;
	case 190: return 1;				/* el canal 1 */
	case 191:					/* las muestras: se apuntan */
		if (sonido_n + c / 2 > sonido_cabe) {
			sonido_cabe = (sonido_n + c / 2) * 2 + 4096;
			sonido_grabado = realloc(sonido_grabado, sonido_cabe * 2);
		}
		memcpy(sonido_grabado + sonido_n, (const void *)(uintptr_t)b, c);
		sonido_n += c / 2;
		en_cola += c;
		return c;
	case 192:
		if (b == 4) {
			/* en cada mirada se han tocado 10 ms (220 muestras) */
			en_cola = en_cola > 440 ? en_cola - 440 : 0;
			return en_cola;
		}
		return 0;
	}
	return 0xFFFFFFF9u;
}
