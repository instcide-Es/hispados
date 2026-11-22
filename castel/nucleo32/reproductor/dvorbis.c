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

typedef struct {
	stb_vorbis *v;
	float *t;
	int cabe_t;
} vorbis;

static int vorbis_paso(dec *d)
{
	vorbis *o = d->priv;
	stb_vorbis_info inf;
	int r = org_carga_paso(d, MAX_OGG), e = 0;

	if (r <= 0)
		return r;
	o->v = stb_vorbis_open_memory(d->mem, (int)d->mem_largo, &e, NULL);
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
