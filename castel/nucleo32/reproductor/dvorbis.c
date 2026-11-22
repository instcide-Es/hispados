/* dvorbis.c - el Reproductor: los OGG Vorbis, con stb_vorbis
 * (terceros/stb_vorbis, dominio público o MIT). El archivo va entero a la
 * memoria, de 64 K en 64 K (dec_paso), mientras suena lo de antes.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "decod.h"
#include "util.h"

#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#define STB_VORBIS_MAX_CHANNELS 8
#include "stb_vorbis.c"

#define MAX_OGG (160u * 1024 * 1024)
#define ARENA_OGG (3u * 1024 * 1024)	/* la memoria de stb_vorbis, toda de aquí (sin malloc ni free) */

typedef struct {
	stb_vorbis *v;
	float *t;
	int cabe_t;
	char *arena;
} vorbis;

/* ¿está bien hecha la cabecera de los comentarios (el segundo paquete)?
 * stb_vorbis se fía de sus largos (un largo enorme le hace pedir memoria
 * «negativa» y escribir fuera: lo encontró el fuzz), así que se miran antes */
static int comentarios_bien(const u8 *d, u32 n)
{
	u32 p = 0, paquete = 0, serie = 0, en = 0, cuantos, i, l, q;
	int primera = 1;
	static u8 b[65536];
	int cabe_entero = 1;

	while (p + 27 <= n && paquete < 2) {
		u32 segs, k, datos;

		if (memcmp(d + p, "OggS", 4))
			return 0;
		if (primera)
			serie = le32(d + p + 14), primera = 0;
		segs = d[p + 26];
		if (p + 27 + segs > n)
			return 0;
		datos = p + 27 + segs;
		for (k = 0; k < segs; k++) {
			u32 s = d[p + 27 + k];

			if (datos + s > n)
				return 0;
			if (le32(d + p + 14) == serie && paquete == 1) {
				if (en + s <= sizeof b)
					memcpy(b + en, d + datos, s);
				else
					cabe_entero = 0;
				en += s;
			}
			datos += s;
			if (s < 255 && le32(d + p + 14) == serie)
				paquete++;
		}
		p = datos;
	}
	if (paquete < 2 || en < 7 + 8 || memcmp(b, "\x03vorbis", 7))
		return 0;
	if (!cabe_entero)
		return 1;	/* (uno enorme, de imágenes: se deja a stb_vorbis) */
	q = 7;
	l = le32(b + q);
	if (l > en - q - 4)
		return 0;
	q += 4 + l;
	if (q + 4 > en)
		return 0;
	cuantos = le32(b + q);
	q += 4;
	if (cuantos > (en - q) / 4)
		return 0;
	for (i = 0; i < cuantos; i++) {
		if (q + 4 > en)
			return 0;
		l = le32(b + q);
		if (l > en - q - 4)
			return 0;
		q += 4 + l;
	}
	return 1;
}

static int vorbis_paso(dec *d)
{
	vorbis *o = d->priv;
	stb_vorbis_info inf;
	int r = org_carga_paso(d, MAX_OGG), e = 0;

	if (r <= 0)
		return r;
	if (!comentarios_bien(d->mem, d->mem_largo)) {
		copia(d->error, sizeof d->error, "OGG Vorbis mal hecho (sus comentarios)");
		return -1;
	}
	/* (con su propia memoria: así un archivo malo no le hace soltar lo que no
	 * pidió —el fuzz encontró uno así—, y lo que pide se ve de una vez) */
	{
		stb_vorbis_alloc a;

		if (!o->arena)
			o->arena = so_pide(ARENA_OGG);
		if (!o->arena) {
			copia(d->error, sizeof d->error, "no hay memoria para el OGG");
			return -1;
		}
		a.alloc_buffer = o->arena;
		a.alloc_buffer_length_in_bytes = (int)ARENA_OGG;
		o->v = stb_vorbis_open_memory(d->mem, (int)d->mem_largo, &e, &a);
	}
	if (!o->v) {
		fmt(d->error, sizeof d->error, "OGG Vorbis que no se puede leer (%d)", e);
		return -1;
	}
	inf = stb_vorbis_get_info(o->v);
	d->hz = inf.sample_rate;
	d->canales = inf.channels;
	d->total = stb_vorbis_stream_length_in_samples(o->v);
	d->kbps = 0;
	if (d->total && d->hz)
		d->kbps = (u32)((u64)d->mem_largo * 8 * d->hz / d->total / 1000);
	d->buscable = 1;
	if (d->hz < 1000 || d->canales < 1 || d->canales > 8) {
		copia(d->error, sizeof d->error, "OGG Vorbis que no se puede tocar");
		return -1;
	}
	return 1;
}

static int vorbis_lee(dec *d, float *s, int tramas)
{
	vorbis *o = d->priv;
	int n;

	if (d->canales <= 2) {
		n = stb_vorbis_get_samples_float_interleaved(o->v, d->canales, s, tramas * d->canales);
		a_estereo(s, d->canales, s, n);
		return n;
	}
	if (o->cabe_t < tramas) {
		so_suelta(o->t);
		o->t = so_pide((size_t)tramas * d->canales * sizeof(float));
		o->cabe_t = o->t ? tramas : 0;
		if (!o->t)
			return 0;
	}
	n = stb_vorbis_get_samples_float_interleaved(o->v, d->canales, o->t, tramas * d->canales);
	a_estereo(o->t, d->canales, s, n);
	return n;
}

static int vorbis_busca(dec *d, u64 t)
{
	vorbis *o = d->priv;

	return stb_vorbis_seek(o->v, (unsigned)t) ? 0 : -1;
}

static void vorbis_cierra(dec *d)
{
	vorbis *o = d->priv;

	if (o) {
		if (o->v)
			stb_vorbis_close(o->v);
		so_suelta(o->t);
		so_suelta(o->arena);
		so_suelta(o);
	}
	d->priv = NULL;
}

int dvorbis_abre(dec *d)
{
	vorbis *o = so_pide(sizeof *o);

	if (!o)
		return -1;
	memset(o, 0, sizeof *o);
	d->priv = o;
	d->formato = F_VORBIS;
	copia(d->nombre_formato, sizeof d->nombre_formato, "OGG Vorbis");
	d->paso = vorbis_paso;
	d->lee = vorbis_lee;
	d->busca = vorbis_busca;
	d->cierra = vorbis_cierra;
	return 0;
}
