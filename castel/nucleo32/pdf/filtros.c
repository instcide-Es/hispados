/* filtros.c - Hispa-PDF: los filtros de los flujos. Flate (con miniz:
 * terceros/miniz, MIT), LZW, ASCIIHex, ASCII85, RunLength y los
 * predictores (PNG y TIFF). Lo descomprimido nunca pasa de MAX_FLUJO; lo
 * estropeado se da hasta donde se ha podido leer (como los lectores de
 * siempre).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "hpdf.h"
#include "miniz_cfg.h"

/* un búfer que crece */
typedef struct {
	u8 *d;
	u32 n, cap, max;
	int lleno;
} pbuf;

static int buf_mete(pbuf *b, const void *d, u32 n)
{
	if (b->n + n > b->max || b->n + n < b->n) {
		b->lleno = 1;
		n = b->max - b->n;
	}
	if (b->n + n > b->cap) {
		u32 c = b->cap ? b->cap : 4096;

		while (c < b->n + n)
			c = c > 0x40000000u ? b->n + n : c * 2;
		b->d = repide(b->d, c);
		b->cap = c;
	}
	memcpy(b->d + b->n, d, n);
	b->n += n;
	return !b->lleno;
}

/* ---- Flate ---- */
static int al_buf(const void *d, int n, void *u)
{
	return buf_mete((pbuf *)u, d, (u32)n);
}

u8 *flate_dec(const u8 *d, u32 n, u32 *sal, u32 max)
{
	pbuf b;
	size_t entra = n;
	int zlib = n >= 2 && (d[0] & 0x0F) == 8 && ((d[0] << 8) | d[1]) % 31 == 0;

	memset(&b, 0, sizeof b);
	b.max = max;
	tinfl_decompress_mem_to_callback(d, &entra, al_buf, &b, zlib ? TINFL_FLAG_PARSE_ZLIB_HEADER : 0);
	if (!b.d)
		b.d = pide(1);
	*sal = b.n;
	return b.d;
}

u8 *flate_cod(const u8 *d, u32 n, u32 *sal, int nivel)
{
	size_t largo = 0;
	int flags = (int)tdefl_create_comp_flags_from_zip_params(nivel, 15, MZ_DEFAULT_STRATEGY);
	void *r = tdefl_compress_mem_to_heap(d, n, &largo, flags);

	u8 *c;

	if (!r)
		hpdf_falla("no queda memoria para comprimir");
	/* (miniz pide con malloc: a la memoria de Hispa-PDF) */
	c = pide(largo + 1);
	memcpy(c, r, largo);
	mz_free(r);
	*sal = (u32)largo;
	return c;
}

/* ---- LZW ---- */
static u8 *lzw(const u8 *d, u32 n, u32 *sal, int temprano)
{
	/* el diccionario: cada código, su anterior y su última letra */
	static u16 prev[4096];
	static u8 letra[4096];
	static u16 largo[4096];
	u8 pila[4096];
	pbuf b;
	u32 bits = 0, nb = 0, i = 0;
	int ancho = 9, siguiente = 258, anterior = -1;

	memset(&b, 0, sizeof b);
	b.max = MAX_FLUJO;
	for (;;) {
		int c, k, l;

		while (nb < (u32)ancho && i < n) {
			bits = bits << 8 | d[i++];
			nb += 8;
		}
		if (nb < (u32)ancho)
			break;
		c = (int)((bits >> (nb - ancho)) & ((1u << ancho) - 1));
		nb -= ancho;
		if (c == 256) {
			ancho = 9;
			siguiente = 258;
			anterior = -1;
			continue;
		}
		if (c == 257)
			break;
		if (c < 256) {
			u8 x = (u8)c;

			if (!buf_mete(&b, &x, 1))
				break;
			if (anterior >= 0 && siguiente < 4096) {
				prev[siguiente] = (u16)anterior;
				letra[siguiente] = x;
				largo[siguiente] = (u16)(largo[anterior] + 1);
				siguiente++;
			}
		} else {
			int cod = c, primera;

			if (c > siguiente || anterior < 0 || (c == siguiente && siguiente >= 4096))
				break;
			if (c == siguiente) {
				/* el caso KwKwK: el anterior más su primera letra */
				cod = anterior;
			}
			l = 0;
			k = cod;
			while (k >= 256 && l < 4095) {
				pila[l++] = letra[k];
				k = prev[k];
			}
			pila[l++] = (u8)k;
			primera = k;
			{
				u8 tmp[4097];
				int j;

				for (j = 0; j < l; j++)
					tmp[j] = pila[l - 1 - j];
				if (c == siguiente)
					tmp[l++] = (u8)primera;
				if (!buf_mete(&b, tmp, (u32)l))
					break;
			}
			if (siguiente < 4096) {
				prev[siguiente] = (u16)anterior;
				letra[siguiente] = (u8)primera;
				largo[siguiente] = (u16)(largo[anterior] + 1);
				siguiente++;
			}
		}
		anterior = c;
		if (siguiente + temprano >= (1 << ancho) && ancho < 12)
			ancho++;
	}
	if (!b.d)
		b.d = pide(1);
	*sal = b.n;
	return b.d;
}

/* ---- ASCIIHex, ASCII85, RunLength ---- */
static u8 *ahex(const u8 *d, u32 n, u32 *sal)
{
	u8 *r = pide(n / 2 + 2);
	const u8 *tras;

	*sal = cadena_hex(d, d + n, r, &tras);
	return r;
}

static u8 *a85(const u8 *d, u32 n, u32 *sal)
{
	pbuf b;
	u32 i = 0, v = 0;
	int c = 0;
	u8 t[4];

	memset(&b, 0, sizeof b);
	b.max = MAX_FLUJO;
	if (n >= 2 && d[0] == '<' && d[1] == '~')
		i = 2;
	for (; i < n; i++) {
		u8 x = d[i];

		if (x == '~')
			break;
		if (es_blanco(x))
			continue;
		if (x == 'z' && c == 0) {
			memset(t, 0, 4);
			if (!buf_mete(&b, t, 4))
				break;
			continue;
		}
		if (x < '!' || x > 'u')
			continue;
		v = v * 85 + (u32)(x - '!');
		if (++c == 5) {
			t[0] = (u8)(v >> 24);
			t[1] = (u8)(v >> 16);
			t[2] = (u8)(v >> 8);
			t[3] = (u8)v;
			if (!buf_mete(&b, t, 4))
				break;
			c = 0;
			v = 0;
		}
	}
	if (c > 1 && !b.lleno) {
		int j;

		for (j = c; j < 5; j++)
			v = v * 85 + 84;
		for (j = 0; j < c - 1; j++)
			t[j] = (u8)(v >> (24 - 8 * j));
		buf_mete(&b, t, (u32)(c - 1));
	}
	if (!b.d)
		b.d = pide(1);
	*sal = b.n;
	return b.d;
}

static u8 *rle(const u8 *d, u32 n, u32 *sal)
{
	pbuf b;
	u32 i = 0;

	memset(&b, 0, sizeof b);
	b.max = MAX_FLUJO;
	while (i < n) {
		u8 l = d[i++];

		if (l == 128)
			break;
		if (l < 128) {
			u32 m = (u32)l + 1;

			if (i + m > n)
				m = n - i;
			if (!buf_mete(&b, d + i, m))
				break;
			i += m;
		} else {
			u8 t[128];

			if (i >= n)
				break;
			memset(t, d[i++], sizeof t);
			if (!buf_mete(&b, t, 257u - l))
				break;
		}
	}
	if (!b.d)
		b.d = pide(1);
	*sal = b.n;
	return b.d;
}

/* ---- los predictores ---- */
static u8 *predictor(pdoc *doc, pobj *parm, u8 *d, u32 n, u32 *sal)
{
	int pred = (int)o_num(dic_r(doc, parm, "Predictor"), 1);
	int colores = (int)o_num(dic_r(doc, parm, "Colors"), 1);
	int bpc = (int)o_num(dic_r(doc, parm, "BitsPerComponent"), 8);
	int cols = (int)o_num(dic_r(doc, parm, "Columns"), 1);
	u32 bpp, fila, filas, f, i;
	u8 *r, *ant;

	*sal = n;
	if (pred < 2)
		return d;
	if (colores < 1 || colores > 32 || cols < 1 || cols > 1000000 || (bpc != 1 && bpc != 2 && bpc != 4 && bpc != 8 && bpc != 16))
		return d;
	bpp = ((u32)colores * bpc + 7) / 8;
	fila = ((u32)colores * bpc * cols + 7) / 8;
	if (pred == 2) {
		/* TIFF: cada muestra, más la de su izquierda (8 y 16 bits) */
		if (bpc == 8) {
			for (f = 0; f + fila <= n; f += fila)
				for (i = bpp; i < fila; i++)
					d[f + i] = (u8)(d[f + i] + d[f + i - bpp]);
		} else if (bpc == 16) {
			for (f = 0; f + fila <= n; f += fila)
				for (i = bpp; i + 1 < fila; i += 2) {
					u32 v = ((u32)d[f + i] << 8 | d[f + i + 1]) + ((u32)d[f + i - bpp] << 8 | d[f + i - bpp + 1]);
					d[f + i] = (u8)(v >> 8);
					d[f + i + 1] = (u8)v;
				}
		}
		return d;
	}
	/* PNG: cada fila con su octeto de filtro delante */
	filas = n / (fila + 1);
	r = pide((size_t)filas * fila + 1);
	ant = pide(fila);
	memset(ant, 0, fila);
	for (f = 0; f < filas; f++) {
		const u8 *e = d + f * (fila + 1);
		u8 *s = r + f * fila, tipo = e[0];

		e++;
		for (i = 0; i < fila; i++) {
			u32 a = i >= bpp ? s[i - bpp] : 0, b = ant[i], c = i >= bpp ? ant[i - bpp] : 0, x = e[i];

			switch (tipo) {
			case 1: x += a; break;
			case 2: x += b; break;
			case 3: x += (a + b) / 2; break;
			case 4: {
				int p = (int)a + (int)b - (int)c, pa = p - (int)a, pb = p - (int)b, pc = p - (int)c;

				if (pa < 0) pa = -pa;
				if (pb < 0) pb = -pb;
				if (pc < 0) pc = -pc;
				x += (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
				break;
			}
			default: break;
			}
			s[i] = (u8)x;
		}
		memcpy(ant, s, fila);
	}
	so_suelta(ant);
	so_suelta(d);
	*sal = filas * fila;
	return r;
}

/* un filtro: da lo filtrado (de pide) o NULL si no se conoce */
u8 *filtro(pdoc *doc, const char *nombre, pobj *parm, const u8 *d, u32 n, u32 *sal)
{
	u8 *r;
	u32 m;

	if (!strcmp(nombre, "FlateDecode") || !strcmp(nombre, "Fl")) {
		r = flate_dec(d, n, &m, MAX_FLUJO);
		return predictor(doc, parm, r, m, sal);
	}
	if (!strcmp(nombre, "LZWDecode") || !strcmp(nombre, "LZW")) {
		int temprano = (int)o_num(dic_r(doc, parm, "EarlyChange"), 1);

		r = lzw(d, n, &m, temprano ? 1 : 0);
		return predictor(doc, parm, r, m, sal);
	}
	if (!strcmp(nombre, "ASCIIHexDecode") || !strcmp(nombre, "AHx"))
		return ahex(d, n, sal);
	if (!strcmp(nombre, "ASCII85Decode") || !strcmp(nombre, "A85"))
		return a85(d, n, sal);
	if (!strcmp(nombre, "RunLengthDecode") || !strcmp(nombre, "RL"))
		return rle(d, n, sal);
	return NULL;
}
