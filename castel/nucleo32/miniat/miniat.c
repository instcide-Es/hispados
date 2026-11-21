/* miniat.c - MINIAT.BIN, las miniaturas del explorador de archivos de
 * Hispa-Pentium (EQUIPO.BIN, uequipo.cas): un programa del anillo 3 que lee
 * una imagen (BMP, PNG, JPG, GIF, TGA, PSD, PNM: stb_image, de
 * terceros/stb), la deja en pequeño (como mucho LADO x LADO, sin cambiar su
 * forma; lo transparente, sobre blanco) y la escribe en un archivo:
 *
 *     MINIAT ENTRADA SALIDA [LADO]
 *
 * La salida: «HPMI», el ancho y el alto (palabras de 32 bits, la baja
 * primero) y sus puntos 0x00RRGGBB, fila a fila. Acaba con 0 si ha ido
 * bien; 1, si no se ha podido leer la imagen; 2, si no se ha podido escribir.
 * Las llamadas al núcleo, por «int 0x80» (docs/PENTIUM.md, sección 6).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

typedef unsigned int u32;
typedef unsigned char u8;

#define ES_ERROR(r) ((u32)(r) >= 0xFFFFFFF0u)
#define MAX_ARCHIVO (24u * 1024 * 1024)

static u32 llamada(u32 n, u32 a, u32 b, u32 c)
{
	u32 r;

	__asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "b"(a), "c"(b), "d"(c) : "memory");
	return r;
}

/* ---- la memoria: la del final del programa, que crece (la llamada 17) ----
 * Va hacia delante y no se devuelve nada (el programa dura una imagen);
 * realloc copia. Cada trozo lleva delante lo que mide. */
void *malloc(size_t n)
{
	u32 r, t = (n + 15) & ~15u;

	r = llamada(17, t + 16, 0, 0);
	if (ES_ERROR(r) || r == 0)
		return NULL;
	*(u32 *)r = t;
	return (void *)(r + 16);
}

void free(void *p)
{
	(void)p;
}

void *realloc(void *p, size_t n)
{
	void *q;
	u32 antes;

	if (!p)
		return malloc(n);
	antes = *(u32 *)((u8 *)p - 16);
	if (n <= antes)
		return p;
	q = malloc(n);
	if (q)
		memcpy(q, p, antes);
	return q;
}

int abs(int x)
{
	return x < 0 ? -x : x;
}

void *memcpy(void *d, const void *o, size_t n)
{
	u8 *a = d;
	const u8 *b = o;

	while (n--)
		*a++ = *b++;
	return d;
}

void *memmove(void *d, const void *o, size_t n)
{
	u8 *a = d;
	const u8 *b = o;

	if (a <= b || a >= b + n)
		return memcpy(d, o, n);
	while (n--)
		a[n] = b[n];
	return d;
}

void *memset(void *d, int c, size_t n)
{
	u8 *a = d;

	while (n--)
		*a++ = (u8)c;
	return d;
}

int memcmp(const void *x, const void *y, size_t n)
{
	const u8 *a = x, *b = y;

	for (; n; n--, a++, b++)
		if (*a != *b)
			return *a - *b;
	return 0;
}

size_t strlen(const char *s)
{
	size_t n = 0;

	while (s[n])
		n++;
	return n;
}

int strcmp(const char *a, const char *b)
{
	while (*a && *a == *b)
		a++, b++;
	return (u8)*a - (u8)*b;
}

int strncmp(const char *a, const char *b, size_t n)
{
	for (; n; n--, a++, b++) {
		if (*a != *b)
			return (u8)*a - (u8)*b;
		if (!*a)
			return 0;
	}
	return 0;
}

#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_SIMD
#define STBI_NO_THREAD_LOCALS
#define STBI_ASSERT(x) ((void)0)
#define STBI_MAX_DIMENSIONS 8192
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

/* el trozo número k de los argumentos (separados por espacios), a d */
static int trozo(const char *a, int k, char *d, int cabe)
{
	int n = 0;

	while (*a == ' ')
		a++;
	while (k > 0 && *a) {
		while (*a && *a != ' ')
			a++;
		while (*a == ' ')
			a++;
		k--;
	}
	while (*a && *a != ' ' && n < cabe - 1)
		d[n++] = *a++;
	d[n] = 0;
	return n;
}

int miniatura(void)
{
	static char args[400], entrada[200], salida[200], lado_t[16];
	u32 m, n, largo, i, x, y, lado = 64, an, al, cab[3];
	u8 *datos, *img;
	u32 *puntos;
	int w, h, comp;

	n = llamada(14, (u32)args, sizeof(args) - 1, 0);
	if (ES_ERROR(n))
		return 1;
	args[n < sizeof(args) ? n : sizeof(args) - 1] = 0;
	if (!trozo(args, 0, entrada, sizeof(entrada)) || !trozo(args, 1, salida, sizeof(salida)))
		return 1;
	if (trozo(args, 2, lado_t, sizeof(lado_t))) {
		lado = 0;
		for (i = 0; lado_t[i] >= '0' && lado_t[i] <= '9'; i++)
			lado = lado * 10 + (lado_t[i] - '0');
		if (lado < 8 || lado > 256)
			lado = 64;
	}
	/* el archivo, entero a la memoria */
	m = llamada(7, (u32)entrada, 0, 0);
	if (ES_ERROR(m))
		return 1;
	largo = llamada(13, m, 0, 0);
	if (ES_ERROR(largo) || largo == 0 || largo > MAX_ARCHIVO) {
		llamada(9, m, 0, 0);
		return 1;
	}
	datos = malloc(largo);
	if (!datos) {
		llamada(9, m, 0, 0);
		return 1;
	}
	for (i = 0; i < largo; i += n) {
		n = llamada(8, m, (u32)(datos + i), largo - i);
		if (ES_ERROR(n) || n == 0)
			break;
	}
	llamada(9, m, 0, 0);
	if (i < largo)
		return 1;
	img = stbi_load_from_memory(datos, (int)largo, &w, &h, &comp, 4);
	if (!img || w <= 0 || h <= 0)
		return 1;
	/* lo que medirá: lo más que quepa en lado x lado, con su forma */
	if ((u32)w >= (u32)h) {
		an = (u32)w < lado ? (u32)w : lado;
		al = (u32)h * an / (u32)w;
	} else {
		al = (u32)h < lado ? (u32)h : lado;
		an = (u32)w * al / (u32)h;
	}
	if (an == 0)
		an = 1;
	if (al == 0)
		al = 1;
	puntos = malloc(an * al * 4);
	if (!puntos)
		return 1;
	/* cada punto, la media de los que le tocan (y lo transparente, sobre blanco) */
	for (y = 0; y < al; y++) {
		u32 y0 = y * (u32)h / al, y1 = (y + 1) * (u32)h / al;

		if (y1 <= y0)
			y1 = y0 + 1;
		for (x = 0; x < an; x++) {
			u32 x0 = x * (u32)w / an, x1 = (x + 1) * (u32)w / an;
			u32 r = 0, g = 0, b = 0, cuenta = 0, yy, xx, pasos;

			if (x1 <= x0)
				x1 = x0 + 1;
			/* (en las imágenes grandes, uno de cada pocos: basta para verla) */
			pasos = (x1 - x0) > 8 ? (x1 - x0) / 8 : 1;
			for (yy = y0; yy < y1; yy += pasos)
				for (xx = x0; xx < x1; xx += pasos) {
					u8 *p = img + ((size_t)yy * (u32)w + xx) * 4;
					u32 a = p[3];

					r += (p[0] * a + 255 * (255 - a)) / 255;
					g += (p[1] * a + 255 * (255 - a)) / 255;
					b += (p[2] * a + 255 * (255 - a)) / 255;
					cuenta++;
				}
			puntos[y * an + x] = ((r / cuenta) << 16) | ((g / cuenta) << 8) | (b / cuenta);
		}
	}
	m = llamada(7, (u32)salida, 2, 0);
	if (ES_ERROR(m))
		return 2;
	cab[0] = 0x494D5048;	/* «HPMI» */
	cab[1] = an;
	cab[2] = al;
	n = llamada(18, m, (u32)cab, 12);
	if (n == 12)
		n = llamada(18, m, (u32)puntos, an * al * 4);
	llamada(9, m, 0, 0);
	return n == an * al * 4 ? 0 : 2;
}
