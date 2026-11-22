/* comun.c - (COMPAT) lo pequeño que usan todos: leer y escribir un texto
 * entero, juntar rutas, mayúsculas.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "compat.h"
#include "texto.h"

char *lee_texto(const char *ruta, u32 *n, u32 max)
{
	const char *e;
	u32 l = 0;
	u8 *d = so_lee_archivo(ruta, &l, max, &e);

	if (n)
		*n = d ? l : 0;
	return (char *)d;
}

int escribe_texto(const char *ruta, const char *t, u32 n)
{
	const char *e;
	void *f = so_crea(ruta, &e);

	if (!f)
		return -1;
	if (n && so_escribe(f, t, n)) {
		so_cierra(f);
		return -1;
	}
	return so_cierra(f);
}

void une_ruta(char *d, u32 cabe, const char *a, const char *b)
{
	u32 n;

	copia(d, a, cabe);
	n = largo(d);
	if (n && d[n - 1] != '\\' && d[n - 1] != '/' && b[0])
		anade(d, "\\", cabe);
	anade(d, b, cabe);
}

void a_mayusculas(char *s)
{
	for (; *s; s++)
		if (*s >= 'a' && *s <= 'z')
			*s = (char)(*s - 32);
}
