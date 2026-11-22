/* so_anfitrion.c - Hispa-PDF en Linux (para las pruebas, con ASan; y como
 * modelo para Hispa-64): lo de so.h con la biblioteca de C de siempre.
 * Con HPDF_PAGINA=437 en el entorno, hace como Hispa-Pentium (8+3 y la
 * página 437). HPDF_MEMORIA=n (MB): lo más que se deja pedir.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "hpdf.h"

static size_t tope, usado;

void *so_pide(size_t n)
{
	size_t *p;

	if (tope && usado + n > tope)
		return NULL;
	p = malloc(n + 16);
	if (!p)
		return NULL;
	p[0] = n;
	usado += n;
	return (u8 *)p + 16;
}

void so_suelta(void *q)
{
	size_t *p;

	if (!q)
		return;
	p = (size_t *)((u8 *)q - 16);
	usado -= p[0];
	free(p);
}

void *so_repide(void *q, size_t n)
{
	size_t *p, antes;

	if (!q)
		return so_pide(n);
	p = (size_t *)((u8 *)q - 16);
	antes = p[0];
	if (tope && usado - antes + n > tope)
		return NULL;
	p = realloc(p, n + 16);
	if (!p)
		return NULL;
	usado = usado - antes + n;
	p[0] = n;
	return (u8 *)p + 16;
}

u8 *so_lee_archivo(const char *ruta, u32 *largo, u32 max, const char **error)
{
	FILE *f = fopen(ruta, "rb");
	long n;
	u8 *d;

	if (!f) {
		if (error)
			*error = "no está (o no se puede abrir)";
		return NULL;
	}
	fseek(f, 0, SEEK_END);
	n = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (n < 0 || (unsigned long)n > max) {
		fclose(f);
		if (error)
			*error = "es demasiado grande";
		return NULL;
	}
	d = so_pide((size_t)n + 1);
	if (!d || fread(d, 1, (size_t)n, f) != (size_t)n) {
		fclose(f);
		so_suelta(d);
		if (error)
			*error = d ? "no se puede leer" : "no hay memoria para leerlo";
		return NULL;
	}
	fclose(f);
	*largo = (u32)n;
	return d;
}

void *so_crea(const char *ruta, const char **error)
{
	FILE *f = fopen(ruta, "wb");

	if (!f && error)
		*error = "no se puede crear";
	return f;
}

int so_escribe(void *f, const void *d, u32 n)
{
	return fwrite(d, 1, n, f) == n ? 0 : -1;
}

int so_cierra(void *f)
{
	return fclose(f) ? -1 : 0;
}

int so_borra(const char *ruta)
{
	return remove(ruta);
}

void so_dice(const char *t)
{
	fputs(t, stdout);
	fflush(stdout);
}

int so_pagina(void)
{
	const char *p = getenv("HPDF_PAGINA");

	return p && !strcmp(p, "437") ? 437 : 0;
}

static char *args_linea;

int so_argumentos(char *d, int cabe)
{
	int n = (int)strlen(args_linea);

	if (n >= cabe)
		n = cabe - 1;
	memcpy(d, args_linea, (size_t)n);
	d[n] = 0;
	return n;
}

u32 so_tics(void)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	return (u32)(t.tv_sec * 100 + t.tv_nsec / 10000000);
}

u32 so_hz(void)
{
	return 100;
}

void so_azar(void *d, u32 n)
{
	FILE *f = fopen("/dev/urandom", "rb");

	if (!f || fread(d, 1, n, f) != n)
		memset(d, 0x5A, n);
	if (f)
		fclose(f);
}

double so_raiz(double x) { return sqrt(x); }
double so_suelo(double x) { return floor(x); }
double so_techo(double x) { return ceil(x); }
double so_abs(double x) { return fabs(x); }
double so_pot(double x, double y) { return pow(x, y); }
double so_resto(double x, double y) { return fmod(x, y); }
double so_cos(double x) { return cos(x); }
double so_sen(double x) { return sin(x); }
double so_acos(double x) { return acos(x); }
double so_atan2(double y, double x) { return atan2(y, x); }
double so_ln(double x) { return log(x); }

int hpdf_orden(const char *linea);
void hpdf_fuentes(void);

int main(int argc, char **argv)
{
	size_t n = 1;
	int i, r;
	const char *m = getenv("HPDF_MEMORIA");

	if (m)
		tope = (size_t)atoi(m) * 1024 * 1024;
	for (i = 1; i < argc; i++)
		n += strlen(argv[i]) + 3;
	args_linea = malloc(n);
	args_linea[0] = 0;
	for (i = 1; i < argc; i++) {
		int esp = strchr(argv[i], ' ') != NULL;

		if (i > 1)
			strcat(args_linea, " ");
		if (esp)
			strcat(args_linea, "\"");
		strcat(args_linea, argv[i]);
		if (esp)
			strcat(args_linea, "\"");
	}
	hpdf_fuentes();
	r = hpdf_orden(args_linea);
	free(args_linea);
	return r;
}
