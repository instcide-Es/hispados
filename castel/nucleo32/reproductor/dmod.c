/* dmod.c - el Reproductor: los módulos de los «trackers» (MOD, XM, S3M e IT),
 * con libxmp-lite (terceros/libxmp-lite, MIT). El módulo va entero a la
 * memoria (son pequeños) y se toca una vez (sin dar vueltas para siempre),
 * a la frecuencia de la tarjeta: así no hay que remuestrear.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "decod.h"
#include "util.h"
#include "xmp.h"

#define MAX_MOD (32u * 1024 * 1024)
#define TROZO_MOD 1024

typedef struct {
	xmp_context x;
	int empezado;
	i16 b[TROZO_MOD * 2];
	int en_b, pos_b;
	int fin;
	char titulo[80], tipo[48];
} modulo;

u32 dmod_hz = 48000;		/* (lo pone el motor: la de la tarjeta) */

static int mod_paso(dec *d)
{
	modulo *m = d->priv;
	struct xmp_module_info mi;
	struct xmp_frame_info fi;
	int r = org_carga_paso(d, MAX_MOD);

	if (r <= 0)
		return r;
	m->x = xmp_create_context();
	if (!m->x || xmp_load_module_from_memory(m->x, d->mem, (long)d->mem_largo) < 0) {
		copia(d->error, sizeof d->error, "módulo que no se puede leer");
		return -1;
	}
	so_suelta(d->mem);	/* (libxmp ya tiene lo suyo) */
	d->mem = NULL;
	if (xmp_start_player(m->x, (int)dmod_hz, 0) < 0) {
		copia(d->error, sizeof d->error, "módulo que no se puede tocar");
		return -1;
	}
	m->empezado = 1;
	xmp_get_module_info(m->x, &mi);
	xmp_get_frame_info(m->x, &fi);
	if (mi.mod) {
		copia(m->titulo, sizeof m->titulo, mi.mod->name);
		copia(m->tipo, sizeof m->tipo, mi.mod->type);
		fmt(d->nombre_formato, sizeof d->nombre_formato, "Módulo %s (%d canales)", dec_ext_de(d->o.ruta), mi.mod->chn);
	}
	d->hz = dmod_hz;
	d->canales = 2;
	d->total = fi.total_time > 0 ? (u64)fi.total_time * dmod_hz / 1000 : 0;
	d->buscable = 1;
	return 1;
}

static int mod_lee(dec *d, float *s, int tramas)
{
	modulo *m = d->priv;
	int hechas = 0, i;

	while (hechas < tramas) {
		if (m->pos_b >= m->en_b) {
			if (m->fin || xmp_play_buffer(m->x, m->b, (int)sizeof m->b, 1) != 0) {
				m->fin = 1;
				break;
			}
			m->en_b = TROZO_MOD;
			m->pos_b = 0;
		}
		for (i = 0; hechas < tramas && m->pos_b < m->en_b; i++, hechas++, m->pos_b++) {
			s[2 * hechas] = m->b[2 * m->pos_b] * (1.0f / 32768);
			s[2 * hechas + 1] = m->b[2 * m->pos_b + 1] * (1.0f / 32768);
		}
	}
	return hechas;
}

static int mod_busca(dec *d, u64 t)
{
	modulo *m = d->priv;

	m->en_b = m->pos_b = 0;
	m->fin = 0;
	return xmp_seek_time(m->x, (int)(t * 1000 / d->hz)) < 0 ? -1 : 0;
}

static void mod_cierra(dec *d)
{
	modulo *m = d->priv;

	if (m) {
		if (m->x) {
			if (m->empezado)
				xmp_end_player(m->x);
			xmp_release_module(m->x);
			xmp_free_context(m->x);
		}
		so_suelta(m);
	}
	d->priv = NULL;
}

/* el nombre que lleva el módulo dentro (para la etiqueta) */
const char *dmod_titulo(dec *d)
{
	modulo *m = d->formato == F_MOD ? d->priv : NULL;

	return m ? m->titulo : "";
}

int dmod_abre(dec *d)
{
	modulo *m = so_pide(sizeof *m);

	if (!m)
		return -1;
	memset(m, 0, sizeof *m);
	d->priv = m;
	d->formato = F_MOD;
	copia(d->nombre_formato, sizeof d->nombre_formato, "Módulo");
	d->paso = mod_paso;
	d->lee = mod_lee;
	d->busca = mod_busca;
	d->cierra = mod_cierra;
	return 0;
}
