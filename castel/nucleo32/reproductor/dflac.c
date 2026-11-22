/* dflac.c - el Reproductor: los FLAC (y los OGG FLAC), con dr_flac
 * (terceros/dr_flac, dominio público o MIT-0), leyendo el archivo a trozos.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "decod.h"
#include "util.h"

#define DR_FLAC_IMPLEMENTATION
#define DR_FLAC_NO_STDIO
#define DR_FLAC_NO_WCHAR
#define DR_FLAC_NO_SIMD
#define DR_FLAC_NO_OGG_CONTAINER_UNUSED
#define DRFLAC_ASSERT(x) ((void)0)
#define DRFLAC_MALLOC(n) so_pide(n)
#define DRFLAC_REALLOC(p, n) so_repide(p, n)
#define DRFLAC_FREE(p) so_suelta(p)
#define DRFLAC_COPY_MEMORY(d, o, n) memcpy(d, o, n)
#define DRFLAC_ZERO_MEMORY(p, n) memset(p, 0, n)
#include "dr_flac.h"

typedef struct {
	drflac *f;
	float *t;		/* (más de dos canales: antes de mezclarlos) */
	int cabe_t;
	int fase;
} flac;

static size_t f_lee(void *u, void *b, size_t n)
{
	int k = org_lee(u, b, (u32)n);

	return k < 0 ? 0 : (size_t)k;
}

static drflac_bool32 f_salta(void *u, int off, drflac_seek_origin o)
{
	dec *d = u;
	i64 p = off;

	if (o == DRFLAC_SEEK_CUR)
		p += (i64)(so_donde(d->a) - d->o.desde);
	else if (o == DRFLAC_SEEK_END)
		p += (i64)org_largo(d);
	if (p < 0 || (u64)p > org_largo(d))
		return DRFLAC_FALSE;
	return org_salta(d, (u64)p) == 0;
}

static drflac_bool32 f_donde(void *u, drflac_int64 *c)
{
	dec *d = u;

	*c = (drflac_int64)(so_donde(d->a) - d->o.desde);
	return DRFLAC_TRUE;
}

static int flac_paso(dec *d)
{
	flac *f = d->priv;

	org_salta(d, 0);
	f->f = drflac_open(f_lee, f_salta, f_donde, d, NULL);
	if (!f->f) {
		copia(d->error, sizeof d->error, "FLAC que no se puede leer");
		return -1;
	}
	d->hz = f->f->sampleRate;
	d->canales = f->f->channels;
	d->bits = f->f->bitsPerSample;
	d->total = f->f->totalPCMFrameCount;
	d->buscable = 1;
	if (d->total && d->hz)
		d->kbps = (u32)(org_largo(d) * 8 * d->hz / d->total / 1000);
	if (f->f->container == drflac_container_ogg)
		copia(d->nombre_formato, sizeof d->nombre_formato, "OGG FLAC");
	if (d->hz < 1000 || d->canales < 1 || d->canales > 8) {
		copia(d->error, sizeof d->error, "FLAC que no se puede tocar");
		return -1;
	}
	return 1;
}

static int flac_lee(dec *d, float *s, int tramas)
{
	flac *f = d->priv;
	drflac_uint64 n;

	if (d->canales <= 2) {
		n = drflac_read_pcm_frames_f32(f->f, (drflac_uint64)tramas, s);
		a_estereo(s, d->canales, s, (int)n);
		return (int)n;
	}
	if (f->cabe_t < tramas) {
		so_suelta(f->t);
		f->t = so_pide((size_t)tramas * d->canales * sizeof(float));
		f->cabe_t = f->t ? tramas : 0;
		if (!f->t)
			return 0;
	}
	n = drflac_read_pcm_frames_f32(f->f, (drflac_uint64)tramas, f->t);
	a_estereo(f->t, d->canales, s, (int)n);
	return (int)n;
}

static int flac_busca(dec *d, u64 t)
{
	flac *f = d->priv;

	return drflac_seek_to_pcm_frame(f->f, t) ? 0 : -1;
}

static void flac_cierra(dec *d)
{
	flac *f = d->priv;

	if (f) {
		if (f->f)
			drflac_close(f->f);
		so_suelta(f->t);
		so_suelta(f);
	}
	d->priv = NULL;
}

int dflac_abre(dec *d)
{
	flac *f = so_pide(sizeof *f);

	if (!f)
		return -1;
	memset(f, 0, sizeof *f);
	d->priv = f;
	d->formato = F_FLAC;
	copia(d->nombre_formato, sizeof d->nombre_formato, "FLAC");
	d->paso = flac_paso;
	d->lee = flac_lee;
	d->busca = flac_busca;
	d->cierra = flac_cierra;
	return 0;
}
