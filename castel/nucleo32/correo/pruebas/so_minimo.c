/* so_minimo.c - Correo: lo mínimo de so.h para las pruebas del núcleo (mime,
 * html, vcard) en Linux: la memoria con malloc, so_apunta a la salida de
 * error, so_azar, la hora fija (22/11/2026 10:05 UTC), la zona +60 y leer
 * un archivo. Solo para pruebas/: el programa de verdad enlaza so_anfitrion.c.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../so.h"

static u32 usada;

void *so_pide(size_t n)
{
	return malloc(n ? n : 1);
}

void *so_repide(void *p, size_t n)
{
	return realloc(p, n ? n : 1);
}

void so_suelta(void *p)
{
	free(p);
}

u32 so_memoria_usada(void)
{
	return usada;
}

void so_apunta(const char *f, ...)
{
	va_list va;

	va_start(va, f);
	fputs("correo: ", stderr);
	vfprintf(stderr, f, va);
	fputc('\n', stderr);
	va_end(va);
}

static u32 semilla = 12345;

void so_azar(void *d, u32 n)
{
	u8 *p = d;
	u32 i;

	for (i = 0; i < n; i++) {
		semilla = semilla * 1103515245u + 12345u;
		p[i] = (u8)(semilla >> 16);
	}
}

i64 so_ahora(void)
{
	return 1795341900;	/* 2026-11-22 10:05:00 UTC */
}

int so_zona(i64 cuando)
{
	(void)cuando;
	return 60;
}

u8 *so_lee_archivo(const char *ruta, u32 *largo, u32 max)
{
	FILE *f = fopen(ruta, "rb");
	long l;
	u8 *d;

	if (!f)
		return NULL;
	if (fseek(f, 0, SEEK_END) || (l = ftell(f)) < 0 || (u32)l > max || fseek(f, 0, SEEK_SET)) {
		fclose(f);
		return NULL;
	}
	d = malloc((size_t)l + 1);
	if (!d || fread(d, 1, (size_t)l, f) != (size_t)l) {
		free(d);
		fclose(f);
		return NULL;
	}
	fclose(f);
	d[l] = 0;
	*largo = (u32)l;
	return d;
}
