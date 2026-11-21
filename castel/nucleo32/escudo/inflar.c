/* inflar.c - ESCUDO.BIN: descomprimir deflate (RFC 1951) y gzip (RFC 1952),
 * para los .cvd de ClamAV (un .tar.gz) y lo de dentro de los ZIP. Escrito
 * aquí: los códigos de Huffman canónicos con una tabla de 10 bits para los
 * cortos (los largos, contando, como el «puff» de zlib, su referencia de
 * siempre), y la ventana de 32 K como anillo que se va sacando entero.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "escudo.h"

#define VENTANA 32768u
#define TABLA_BITS 10

typedef struct huff {
	u16 cuenta[16];			/* cuántos códigos de cada largo */
	u16 simbolo[320];		/* los símbolos, por orden de código */
	u16 tabla[1 << TABLA_BITS];	/* (símbolo << 4) | largo; 0: no está (más largo) */
} huff;

static int carga(inflar *z)
{
	int n;

	if (z->ent_fin)
		return 0;
	n = z->lee(z->yo, z->ent, sizeof z->ent);
	if (n <= 0) {
		z->ent_fin = 1;
		return 0;
	}
	z->ent_n = (u32)n;
	z->ent_i = 0;
	z->leidos += (u32)n;
	return 1;
}

/* asegura «n» bits (hasta 25) en el depósito: 0 si se acaba la entrada */
static int pide(inflar *z, u32 n)
{
	while (z->nbits < n) {
		if (z->ent_i >= z->ent_n && !carga(z))
			return 0;
		z->bits |= (u32)z->ent[z->ent_i++] << z->nbits;
		z->nbits += 8;
	}
	return 1;
}

static int bits(inflar *z, u32 n, u32 *v)
{
	if (!pide(z, n))
		return 0;
	*v = z->bits & ((1u << n) - 1);
	z->bits >>= n;
	z->nbits -= n;
	return 1;
}

/* hace la tabla de un código con los largos de «largo» (n símbolos) */
static int hace(huff *h, const u8 *largo, int n)
{
	u16 sitio[16];
	int i, l, code, k;

	memset(h->cuenta, 0, sizeof h->cuenta);
	for (i = 0; i < n; i++)
		h->cuenta[largo[i]]++;
	h->cuenta[0] = 0;
	sitio[1] = 0;
	for (l = 1; l < 15; l++)
		sitio[l + 1] = sitio[l] + h->cuenta[l];
	for (i = 0; i < n; i++)
		if (largo[i])
			h->simbolo[sitio[largo[i]]++] = (u16)i;
	/* la tabla rápida: cada código de hasta TABLA_BITS, al revés (deflate saca
	 * los códigos con el bit alto primero, y el depósito va del bajo) */
	memset(h->tabla, 0, sizeof h->tabla);
	code = 0;
	k = 0;
	for (l = 1; l <= TABLA_BITS; l++) {
		int c;
		for (c = 0; c < h->cuenta[l]; c++, code++, k++) {
			u32 r = 0, x = (u32)code, j;
			for (j = 0; j < (u32)l; j++)
				r = (r << 1) | ((x >> j) & 1);
			for (j = r; j < (1u << TABLA_BITS); j += 1u << l)
				h->tabla[j] = (u16)(h->simbolo[k] << 4 | l);
		}
		code <<= 1;
	}
	return 0;
}

/* un símbolo: por la tabla si es corto; si no, contando bit a bit */
static int simbolo(inflar *z, const huff *h)
{
	int code = 0, primero = 0, indice = 0, l;

	pide(z, TABLA_BITS);
	if (z->nbits >= TABLA_BITS || z->nbits > 0) {
		u32 e = h->tabla[z->bits & ((1u << TABLA_BITS) - 1)];
		if (e && (e & 15) <= z->nbits) {
			z->bits >>= e & 15;
			z->nbits -= e & 15;
			return e >> 4;
		}
	}
	for (l = 1; l < 16; l++) {
		u32 b;
		int c;
		if (!bits(z, 1, &b))
			return -1;
		code |= (int)b;
		c = h->cuenta[l];
		if (code - c < primero)
			return h->simbolo[indice + (code - primero)];
		indice += c;
		primero += c;
		primero <<= 1;
		code <<= 1;
	}
	return -1;
}

static int saca_ventana(inflar *z, u32 desde, u32 hasta)
{
	if (hasta > desde && z->saca(z->yo, z->ventana + desde, hasta - desde))
		return INF_PARADO;
	return INF_BIEN;
}

static int pon(inflar *z, u8 c)
{
	z->ventana[z->vpos++] = c;
	z->total++;
	if (z->vpos == VENTANA) {
		z->vpos = 0;
		return saca_ventana(z, 0, VENTANA);
	}
	return INF_BIEN;
}

static const u16 base_largo[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
				    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
static const u8 extra_largo[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
static const u16 base_dist[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
				   257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
static const u8 extra_dist[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };

static int bloque(inflar *z, const huff *hl, const huff *hd)
{
	int s, r;

	for (;;) {
		u32 e, largo, dist, desde;
		s = simbolo(z, hl);
		if (s < 0)
			return INF_MAL;
		if (s < 256) {
			if ((r = pon(z, (u8)s)) != INF_BIEN)
				return r;
			continue;
		}
		if (s == 256)
			return INF_BIEN;
		s -= 257;
		if (s >= 29)
			return INF_MAL;
		if (!bits(z, extra_largo[s], &e))
			return INF_MAL;
		largo = base_largo[s] + e;
		s = simbolo(z, hd);
		if (s < 0 || s >= 30)
			return INF_MAL;
		if (!bits(z, extra_dist[s], &e))
			return INF_MAL;
		dist = base_dist[s] + e;
		if (dist > z->total || dist > VENTANA)
			return INF_MAL;
		if (z->tope && z->total + largo > z->tope)
			return INF_TOPE;
		desde = (z->vpos + VENTANA - dist) & (VENTANA - 1);
		while (largo--) {
			u8 c = z->ventana[desde];
			desde = (desde + 1) & (VENTANA - 1);
			if ((r = pon(z, c)) != INF_BIEN)
				return r;
		}
	}
}

static int bloque_fijo(inflar *z)
{
	static huff hl, hd;
	static int hecho;

	if (!hecho) {
		u8 l[288];
		int i;
		for (i = 0; i < 144; i++)
			l[i] = 8;
		for (; i < 256; i++)
			l[i] = 9;
		for (; i < 280; i++)
			l[i] = 7;
		for (; i < 288; i++)
			l[i] = 8;
		hace(&hl, l, 288);
		for (i = 0; i < 30; i++)
			l[i] = 5;
		hace(&hd, l, 30);
		hecho = 1;
	}
	return bloque(z, &hl, &hd);
}

static int bloque_dinamico(inflar *z)
{
	static const u8 orden[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
	static huff hl, hd, hc;		/* (una sola tarea descomprime a la vez) */
	u8 largos[320];
	u32 nl, nd, nc, v;
	int i, s;

	if (!bits(z, 5, &nl) || !bits(z, 5, &nd) || !bits(z, 4, &nc))
		return INF_MAL;
	nl += 257;
	nd += 1;
	nc += 4;
	if (nl > 286 || nd > 30)
		return INF_MAL;
	memset(largos, 0, sizeof largos);
	for (i = 0; i < (int)nc; i++) {
		if (!bits(z, 3, &v))
			return INF_MAL;
		largos[orden[i]] = (u8)v;
	}
	hace(&hc, largos, 19);
	i = 0;
	while (i < (int)(nl + nd)) {
		u32 rep;
		u8 l = 0;
		s = simbolo(z, &hc);
		if (s < 0)
			return INF_MAL;
		if (s < 16) {
			largos[i++] = (u8)s;
			continue;
		}
		if (s == 16) {
			if (i == 0 || !bits(z, 2, &rep))
				return INF_MAL;
			l = largos[i - 1];
			rep += 3;
		} else if (s == 17) {
			if (!bits(z, 3, &rep))
				return INF_MAL;
			rep += 3;
		} else {
			if (!bits(z, 7, &rep))
				return INF_MAL;
			rep += 11;
		}
		if (i + (int)rep > (int)(nl + nd))
			return INF_MAL;
		while (rep--)
			largos[i++] = l;
	}
	if (!largos[256])
		return INF_MAL;
	hace(&hl, largos, (int)nl);
	hace(&hd, largos + nl, (int)nd);
	return bloque(z, &hl, &hd);
}

static int guardado(inflar *z)
{
	u32 n, c, r;

	z->bits >>= z->nbits & 7;	/* al octeto siguiente */
	z->nbits -= z->nbits & 7;
	if (!bits(z, 16, &n) || !bits(z, 16, &c))
		return INF_MAL;
	if ((n ^ 0xFFFF) != c)
		return INF_MAL;
	if (z->tope && z->total + n > z->tope)
		return INF_TOPE;
	while (n--) {
		u32 b;
		if (!bits(z, 8, &b))
			return INF_MAL;
		if ((r = (u32)pon(z, (u8)b)) != INF_BIEN)
			return (int)r;
	}
	return INF_BIEN;
}

int inflar_todo(inflar *z)
{
	u32 ultimo, tipo;
	int r = INF_BIEN;

	z->ventana = pl_reserva(VENTANA);
	if (!z->ventana)
		return INF_SIN_MEMORIA;
	z->vpos = 0;
	z->total = 0;
	do {
		if (!bits(z, 1, &ultimo) || !bits(z, 2, &tipo)) {
			r = INF_MAL;
			break;
		}
		if (tipo == 0)
			r = guardado(z);
		else if (tipo == 1)
			r = bloque_fijo(z);
		else if (tipo == 2)
			r = bloque_dinamico(z);
		else
			r = INF_MAL;
		if (r == INF_BIEN && z->tope && z->total > z->tope)
			r = INF_TOPE;
	} while (r == INF_BIEN && !ultimo);
	if (r == INF_BIEN)
		r = saca_ventana(z, 0, z->vpos);
	pl_libera(z->ventana);
	z->ventana = 0;
	return r;
}

/* gzip: ID1 ID2 CM FLG MTIME(4) XFL OS [FEXTRA] [FNAME] [FCOMMENT] [FHCRC] */
int inflar_gzip(inflar *z)
{
	u32 b[10], i, fl, v;

	for (i = 0; i < 10; i++)
		if (!bits(z, 8, &b[i]))
			return INF_MAL;
	if (b[0] != 0x1F || b[1] != 0x8B || b[2] != 8)
		return INF_MAL;
	fl = b[3];
	if (fl & 4) {
		u32 n, h;
		if (!bits(z, 8, &n) || !bits(z, 8, &h))
			return INF_MAL;
		n |= h << 8;
		while (n--)
			if (!bits(z, 8, &v))
				return INF_MAL;
	}
	if (fl & 8)
		do
			if (!bits(z, 8, &v))
				return INF_MAL;
		while (v);
	if (fl & 16)
		do
			if (!bits(z, 8, &v))
				return INF_MAL;
		while (v);
	if (fl & 2)
		if (!bits(z, 16, &v))
			return INF_MAL;
	return inflar_todo(z);
}
