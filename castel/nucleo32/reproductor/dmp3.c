/* dmp3.c - el Reproductor: los MP3 (y MP2 y MP1), con minimp3 (terceros/minimp3,
 * CC0) trama a trama y leyendo el archivo a trozos (no entero: un MP3 de una
 * hora son 100 MB). Lo de alrededor es nuestro:
 *   - el ID3v2 de delante y el ID3v1 de detrás, saltados;
 *   - la cabecera Xing/Info (o la VBRI) de la primera trama: cuántas tramas
 *     son (la duración de los de tasa variable) y su tabla para buscar;
 *   - la etiqueta LAME: el retraso y el relleno del codificador, para que
 *     no haya hueco entre canciones (sin huecos, «gapless»): se tiran las
 *     primeras «retraso + 529» muestras y se acaba donde dice;
 *   - buscar: con la tabla de Xing, o en proporción (los de tasa fija), y
 *     tirando la trama de después (por la reserva de bits).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "decod.h"
#include "util.h"

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_FLOAT_OUTPUT
#define MINIMP3_NO_SIMD
#include "minimp3.h"

#define ENTRADA (32u * 1024)
#define RETRASO_DECOD 529
#define TIRA_TRAS_BUSCAR 2	/* las tramas que se tiran tras buscar: la reserva de bits y el solape del IMDCT */	/* el de los decodificadores (lo que se suma al de LAME) */

typedef struct {
	mp3dec_t m;
	u8 b[ENTRADA];
	u32 en_b, pos_b;
	float pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
	int en_pcm, pos_pcm, canales_pcm;
	u64 inicio, fin;		/* los octetos del sonido (sin las etiquetas) */
	u32 spf;			/* las muestras de cada trama */
	u32 tramas_xing;		/* (0: no se sabe) */
	u8 toc[100];
	int hay_toc;
	int cbr;			/* de tasa fija («Info» de LAME, o sin cabecera de Xing) */
	u32 retraso, relleno;		/* (LAME) */
	u64 tirar;			/* lo que aún hay que tirar del principio */
	u64 dadas;			/* las muestras dadas desde el principio (para el final de LAME) */
	int fase;			/* 0: el ID3; 1: la primera trama; 2: listo */
	int tirar_trama;		/* tras buscar, la primera (sin su reserva de bits) */
	u64 objetivo;			/* (tras buscar) la muestra que se quería, contando el retraso */
	u64 n_tramas;
	int acabado_archivo;
} mp3;

static int llena(dec *d, mp3 *m)
{
	int k;
	u64 p;

	if (m->pos_b > 0) {
		memmove(m->b, m->b + m->pos_b, m->en_b - m->pos_b);
		m->en_b -= m->pos_b;
		m->pos_b = 0;
	}
	if (m->acabado_archivo || m->en_b >= ENTRADA)
		return 0;
	p = so_donde(d->a) - d->o.desde;
	if (p >= m->fin) {
		m->acabado_archivo = 1;
		return 0;
	}
	k = org_lee(d, m->b + m->en_b, (u32)((ENTRADA - m->en_b) < m->fin - p ? ENTRADA - m->en_b : m->fin - p));
	if (k <= 0) {
		m->acabado_archivo = 1;
		return 0;
	}
	m->en_b += (u32)k;
	return k;
}

/* la cabecera Xing/Info o VBRI de una trama (y la etiqueta LAME) */
static void mira_xing(mp3 *m, const u8 *t, u32 n, int lsf, int mono)
{
	u32 o = lsf ? (mono ? 13 : 21) : (mono ? 21 : 36), f;
	const u8 *x;

	m->cbr = 1;
	if (o + 120 <= n && (!memcmp(t + o, "Xing", 4) || !memcmp(t + o, "Info", 4))) {
		m->cbr = !memcmp(t + o, "Info", 4);
		x = t + o + 4;
		f = be32(x);
		x += 4;
		if (f & 1) {
			m->tramas_xing = be32(x);
			x += 4;
		}
		if (f & 2)
			x += 4;
		if (f & 4) {
			memcpy(m->toc, x, 100);
			m->hay_toc = 1;
			x += 100;
		}
		if (f & 8)
			x += 4;
		/* la etiqueta LAME (o la de otros codificadores, con la misma forma) */
		if (x + 24 <= t + n && (x[0] >= 'A' && x[0] <= 'Z')) {
			const u8 *r = x + 21;

			m->retraso = (u32)r[0] << 4 | r[1] >> 4;
			m->relleno = (u32)(r[1] & 15) << 8 | r[2];
			if (m->retraso > 3000 || m->relleno > 6000)
				m->retraso = m->relleno = 0;
		}
	} else if (36 + 26 <= n && !memcmp(t + 36, "VBRI", 4)) {
		m->tramas_xing = be32(t + 36 + 14);
		m->cbr = 0;
	}
}

static int mp3_paso(dec *d)
{
	mp3 *m = d->priv;
	mp3dec_frame_info_t inf;
	u8 c[10];
	int i;

	if (m->fase == 0) {
		u64 largo = org_largo(d);

		m->inicio = 0;
		org_salta(d, 0);
		/* (puede haber varios ID3v2 seguidos) */
		for (i = 0; i < 4; i++) {
			if (org_lee(d, c, 10) != 10 || memcmp(c, "ID3", 3))
				break;
			m->inicio += 10 + ((u32)(c[6] & 127) << 21 | (u32)(c[7] & 127) << 14 | (u32)(c[8] & 127) << 7 | (c[9] & 127)) +
				     ((c[5] & 0x10) ? 10 : 0);
			org_salta(d, m->inicio);
		}
		m->fin = largo;
		if (largo >= 128 + m->inicio) {
			org_salta(d, largo - 128);
			if (org_lee(d, c, 3) == 3 && !memcmp(c, "TAG", 3))
				m->fin = largo - 128;
		}
		org_salta(d, m->inicio);
		m->en_b = m->pos_b = 0;
		m->fase = 1;
		return 0;
	}
	if (m->fase == 1) {
		/* la primera trama: lo que es, y si lleva Xing */
		int muestras, vueltas = 0;

		while (vueltas++ < 8) {
			llena(d, m);
			if (m->en_b - m->pos_b < 4)
				break;
			muestras = mp3dec_decode_frame(&m->m, m->b + m->pos_b, (int)(m->en_b - m->pos_b), NULL, &inf);
			if (inf.frame_bytes == 0) {
				/* nada en lo que hay: más (o se acabó) */
				if (m->acabado_archivo || m->en_b - m->pos_b < ENTRADA / 2) {
					if (m->acabado_archivo)
						break;
					continue;
				}
				m->pos_b = m->en_b - 4;
				continue;
			}
			{
				u32 donde = m->pos_b + (u32)inf.frame_offset;
				const u8 *t = m->b + donde;
				int lsf = !(t[1] & 8), mono = (t[3] >> 6) == 3;

				d->hz = (u32)inf.hz;
				d->canales = inf.channels;
				m->spf = inf.layer == 1 ? 384 : (inf.layer == 2 || !lsf) ? 1152 : 576;
				d->kbps = (u32)inf.bitrate_kbps;
				m->inicio = (u64)(so_donde(d->a) - d->o.desde) - m->en_b + donde;
				mira_xing(m, t, (u32)inf.frame_bytes, lsf, mono);
				if (inf.layer == 3 && m->tramas_xing) {
					/* la trama de Xing no lleva sonido: se salta */
					m->pos_b = donde + (u32)inf.frame_bytes;
					m->inicio += (u64)inf.frame_bytes;
				} else
					m->pos_b = donde;
				(void)muestras;
				copia(d->nombre_formato, sizeof d->nombre_formato, inf.layer == 3 ? "MP3" : inf.layer == 2 ? "MP2 (capa II)" : "MP1 (capa I)");
				break;
			}
		}
		if (!d->hz) {
			copia(d->error, sizeof d->error, "no hay sonido MP3 en el archivo");
			return -1;
		}
		mp3dec_init(&m->m);
		if (m->tramas_xing) {
			d->total = (u64)m->tramas_xing * m->spf;
			if (m->retraso || m->relleno) {
				u64 quita = (u64)m->retraso + m->relleno;

				d->total = d->total > quita ? d->total - quita : 0;
			}
			if (d->total && m->fin > m->inicio)
				d->kbps = (u32)((m->fin - m->inicio) * 8 * d->hz / d->total / 1000);
		} else if (d->kbps) {
			/* tasa fija: por lo que mide */
			d->total = (m->fin - m->inicio) * 8 * d->hz / ((u64)d->kbps * 1000);
		}
		m->tirar = m->retraso || m->relleno ? (u64)m->retraso + RETRASO_DECOD : 0;
		m->dadas = 0;
		d->buscable = 1;
		m->fase = 2;
		return 1;
	}
	return 1;
}

/* la trama siguiente a m->pcm; 0 si se acabó */
static int otra_trama(dec *d, mp3 *m)
{
	mp3dec_frame_info_t inf;
	int muestras, vueltas = 0;

	for (;;) {
		inf.hz = 0;
		if (m->en_b - m->pos_b < 2048 && !m->acabado_archivo)
			llena(d, m);
		if (m->en_b - m->pos_b < 4)
			return 0;
		muestras = mp3dec_decode_frame(&m->m, m->b + m->pos_b, (int)(m->en_b - m->pos_b), m->pcm, &inf);
		if (!inf.frame_bytes) {
			if (m->acabado_archivo)
				return 0;
			if (llena(d, m) <= 0 || ++vueltas > 64) {
				/* (basura sin tramas: se tira y se sigue) */
				if (m->en_b - m->pos_b > 4)
					m->pos_b = m->en_b - 4;
				if (m->acabado_archivo || vueltas > 64)
					return 0;
			}
			continue;
		}
		if (!inf.hz) {
			/* (lo que ha saltado minimp3 hasta encontrar una trama: no es una) */
			m->pos_b += (u32)inf.frame_bytes;
			continue;
		}
		if (m->tirar_trama) {
			/* la primera tras buscar: dónde ha caído de verdad (en los de
			 * tasa fija se sabe por el octeto), y lo que hay que tirar */
			u64 off = (u64)(so_donde(d->a) - d->o.desde) - m->en_b + m->pos_b + (u64)inf.frame_offset;

			m->pos_b += (u32)inf.frame_bytes;
			if (m->tirar_trama == TIRA_TRAS_BUSCAR && m->cbr && m->n_tramas && m->fin > m->inicio) {
				double media = (double)(m->fin - m->inicio) / (double)m->n_tramas;
				u64 g = (u64)((double)(off - m->inicio) / media + 0.5);

				m->tirar = m->objetivo > (g + TIRA_TRAS_BUSCAR) * m->spf ? m->objetivo - (g + TIRA_TRAS_BUSCAR) * m->spf : 0;
			}
			m->tirar_trama--;
			continue;
		}
		m->pos_b += (u32)inf.frame_bytes;
		if (muestras <= 0)
			continue;	/* (una trama sin sonido: la de Xing de otro, o sin reserva) */
		m->en_pcm = muestras;
		m->pos_pcm = 0;
		m->canales_pcm = inf.channels;
		return 1;
	}
}

static int mp3_lee(dec *d, float *s, int tramas)
{
	mp3 *m = d->priv;
	int hechas = 0;

	while (hechas < tramas) {
		int k;

		if (m->pos_pcm >= m->en_pcm && !otra_trama(d, m))
			break;
		k = m->en_pcm - m->pos_pcm;
		if (m->tirar) {
			int t = (u64)k < m->tirar ? k : (int)m->tirar;

			m->pos_pcm += t;
			m->tirar -= (u64)t;
			continue;
		}
		if (k > tramas - hechas)
			k = tramas - hechas;
		/* el final que dice LAME (sin el relleno) */
		if (d->total && (m->retraso || m->relleno) && m->dadas + (u64)k > d->total) {
			k = (int)(d->total > m->dadas ? d->total - m->dadas : 0);
			if (k <= 0)
				break;
		}
		a_estereo(m->pcm + m->pos_pcm * m->canales_pcm, m->canales_pcm, s + 2 * hechas, k);
		m->pos_pcm += k;
		m->dadas += (u64)k;
		hechas += k;
	}
	return hechas;
}

/* buscar: a la trama de MP3 de antes de la que lleva esa muestra (la de
 * antes se tira: le falta su reserva de bits), y de la buena se tira lo que
 * sobra hasta la muestra. Con la tabla de Xing (los de tasa variable) o en
 * proporción (los de tasa fija, que es exacto) */
static int mp3_busca(dec *d, u64 trama)
{
	mp3 *m = d->priv;
	u64 b, largo = m->fin - m->inicio, D = (m->retraso || m->relleno) ? (u64)m->retraso + RETRASO_DECOD : 0, f, n_tramas;

	f = (trama + D) / m->spf;
	if (f < TIRA_TRAS_BUSCAR)
		f = 0;
	n_tramas = m->tramas_xing ? m->tramas_xing : (d->total + D + m->spf - 1) / m->spf;
	if (!n_tramas)
		return -1;
	if (trama == 0 || f == 0) {
		b = 0;
		f = 0;
	} else if (m->hay_toc && !m->cbr) {
		/* la tabla de Xing: el tanto por ciento, entre dos de sus puntos */
		double pc = (double)(f - TIRA_TRAS_BUSCAR) * 100.0 / (double)n_tramas, fa, fb;
		int i = (int)pc;

		if (i > 99)
			i = 99;
		fa = m->toc[i];
		fb = i < 99 ? m->toc[i + 1] : 256.0;
		b = (u64)((fa + (fb - fa) * (pc - i)) / 256.0 * (double)largo);
	} else
		b = (u64)((double)(f - TIRA_TRAS_BUSCAR) / (double)n_tramas * (double)largo);
	if (b >= largo)
		b = largo ? largo - 1 : 0;
	mp3dec_init(&m->m);
	m->en_b = m->pos_b = 0;
	m->en_pcm = m->pos_pcm = 0;
	m->acabado_archivo = 0;
	if (org_salta(d, m->inicio + b))
		return -1;
	m->tirar = (trama + D) - f * m->spf;
	m->tirar_trama = f ? TIRA_TRAS_BUSCAR : 0;
	m->objetivo = trama + D;
	m->n_tramas = n_tramas;
	m->dadas = trama;
	return 0;
}

static void mp3_cierra(dec *d)
{
	so_suelta(d->priv);
	d->priv = NULL;
}

int dmp3_abre(dec *d)
{
	mp3 *m = so_pide(sizeof *m);

	if (!m)
		return -1;
	memset(m, 0, sizeof *m);
	mp3dec_init(&m->m);
	d->priv = m;
	d->formato = F_MP3;
	copia(d->nombre_formato, sizeof d->nombre_formato, "MP3");
	d->paso = mp3_paso;
	d->lee = mp3_lee;
	d->busca = mp3_busca;
	d->cierra = mp3_cierra;
	return 0;
}
