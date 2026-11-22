/* decod.c - el Reproductor: lo común de los decodificadores (de dónde salen
 * los octetos, qué formato es, abrir de a poco) y los dos más sencillos: el
 * WAV (PCM de 8, 16, 24 y 32 bits, coma flotante, IMA-ADPCM y MS-ADPCM) y el
 * sonido de los CD (las pistas de un CD de verdad, o de una imagen .BIN con
 * su hoja .CUE: PCM de 16 bits, estéreo, a 44.100 Hz).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "decod.h"
#include "util.h"

/* ------------------------------------------------------------ el origen */
int org_lee(dec *d, void *b, u32 n)
{
	if (d->o.largo) {
		u64 p = so_donde(d->a);

		if (p >= d->o.desde + d->o.largo)
			return 0;
		if (p + n > d->o.desde + d->o.largo)
			n = (u32)(d->o.desde + d->o.largo - p);
	}
	return so_lee(d->a, b, n);
}

int org_salta(dec *d, u64 pos)
{
	return so_salta_a(d->a, d->o.desde + pos);
}

u64 org_largo(dec *d)
{
	return d->o.largo ? d->o.largo : so_largo(d->a);
}

/* lee un trozo más del archivo a la memoria; 1 cuando está entero */
int org_carga_paso(dec *d, u32 max)
{
	int k;

	if (!d->mem) {
		u64 n = org_largo(d);

		if (n > max) {
			copia(d->error, sizeof d->error, "el archivo es demasiado grande");
			return -1;
		}
		d->mem_largo = (u32)n;
		d->mem_leido = 0;
		d->mem = so_pide(d->mem_largo + 16);
		if (!d->mem) {
			copia(d->error, sizeof d->error, "no hay memoria para el archivo");
			return -1;
		}
		org_salta(d, 0);
	}
	if (d->mem_leido >= d->mem_largo)
		return 1;
	k = org_lee(d, d->mem + d->mem_leido, d->mem_largo - d->mem_leido > TROZO_CARGA ? TROZO_CARGA : d->mem_largo - d->mem_leido);
	if (k <= 0) {
		copia(d->error, sizeof d->error, "no se puede leer el archivo");
		return -1;
	}
	d->mem_leido += (u32)k;
	return d->mem_leido >= d->mem_largo;
}

/* ------------------------------------------------------------ el formato */
const char *dec_ext_de(const char *ruta)
{
	static char e[8];
	const char *p = ruta + strlen(ruta);
	int i;

	while (p > ruta && p[-1] != '.' && p[-1] != '\\' && p[-1] != '/')
		p--;
	e[0] = 0;
	if (p == ruta || p[-1] != '.')
		return e;
	for (i = 0; i < 7 && p[i]; i++)
		e[i] = (char)(p[i] >= 'a' && p[i] <= 'z' ? p[i] - 32 : p[i]);
	e[i] = 0;
	return e;
}

int dec_ext_vale(const char *ruta)
{
	static const char *vale[] = { "MP3", "MP2", "MP1", "OGG", "OGA", "FLA", "FLAC", "WAV", "MOD", "XM", "S3M", "IT",
		"MPG", "CUE", "OPUS", 0 };
	const char *e = dec_ext_de(ruta);
	int i;

	for (i = 0; vale[i]; i++)
		if (!strcmp(e, vale[i]))
			return 1;
	return 0;
}

static int es_mod(const u8 *c, u32 n)
{
	static const char *marcas[] = { "M.K.", "M!K!", "FLT4", "FLT8", "4CHN", "6CHN", "8CHN", "CD81", "OKTA", "OCTA", 0 };
	int i;

	if (n >= 17 && !memcmp(c, "Extended Module: ", 17))
		return 1;
	if (n >= 48 && !memcmp(c + 44, "SCRM", 4))
		return 1;
	if (n >= 4 && !memcmp(c, "IMPM", 4))
		return 1;
	if (n >= 1084) {
		for (i = 0; marcas[i]; i++)
			if (!memcmp(c + 1080, marcas[i], 4))
				return 1;
		if (c[1082] == 'C' && c[1083] == 'H' && c[1080] >= '1' && c[1080] <= '9')
			return 1;	/* «16CH», «32CH»... */
	}
	return 0;
}

/* ¿hay una trama de MP3 buena aquí? (y otra justo detrás) */
static int trama_mp3(const u8 *c, u32 n, u32 i)
{
	static const u16 kbps1[16] = { 0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0 };
	u32 b, hz, l;

	if (i + 4 > n || c[i] != 0xFF || (c[i + 1] & 0xE0) != 0xE0)
		return 0;
	if (((c[i + 1] >> 3) & 3) == 1 || ((c[i + 1] >> 1) & 3) == 0 || (c[i + 2] >> 4) == 15 || ((c[i + 2] >> 2) & 3) == 3)
		return 0;
	if (((c[i + 1] >> 3) & 3) != 3 || ((c[i + 1] >> 1) & 3) != 1)
		return 1;	/* (de las otras, con la cabecera basta) */
	b = kbps1[c[i + 2] >> 4];
	hz = ((c[i + 2] >> 2) & 3) == 0 ? 44100 : ((c[i + 2] >> 2) & 3) == 1 ? 48000 : 32000;
	if (!b)
		return 1;
	l = 144000 * b / hz + ((c[i + 2] >> 1) & 1);
	if (i + l + 2 > n)
		return 1;
	return c[i + l] == 0xFF && (c[i + l + 1] & 0xE0) == 0xE0;
}

int dec_formato(const u8 *c, u32 n, const char *ruta)
{
	const char *e = dec_ext_de(ruta);
	u32 i, k;

	if (n >= 12 && !memcmp(c, "RIFF", 4) && !memcmp(c + 8, "WAVE", 4))
		return F_WAV;
	if (n >= 4 && !memcmp(c, "fLaC", 4))
		return F_FLAC;
	if (n >= 36 && !memcmp(c, "OggS", 4)) {
		k = 27 + c[26];
		if (k + 8 <= n && !memcmp(c + k, "OpusHead", 8))
			return F_OPUS;
		if (k + 5 <= n && !memcmp(c + k, "\x7F" "FLAC", 5))
			return F_FLAC;
		return F_VORBIS;
	}
	if (es_mod(c, n))
		return F_MOD;
	/* un ID3v2 delante: lo de detrás (un MP3 casi siempre; un FLAC a veces) */
	if (n >= 10 && !memcmp(c, "ID3", 3)) {
		k = ((u32)(c[6] & 127) << 21 | (u32)(c[7] & 127) << 14 | (u32)(c[8] & 127) << 7 | (c[9] & 127)) + 10;
		if (c[5] & 0x10)
			k += 10;
		if (k + 4 <= n && !memcmp(c + k, "fLaC", 4))
			return F_FLAC;
		return F_MP3;
	}
	if (!strcmp(e, "MOD") || !strcmp(e, "XM") || !strcmp(e, "S3M") || !strcmp(e, "IT"))
		return F_MOD;	/* (los MOD de 15 instrumentos no llevan marca) */
	for (i = 0; i + 4 <= n && i < 8192; i++)
		if (trama_mp3(c, n, i))
			return F_MP3;
	return F_NADA;
}

/* ------------------------------------------------------------ abrir */
int dec_abre(dec *d, const origen *o)
{
	u8 cab[4096];
	int n;

	memset(d, 0, sizeof *d);
	d->o = *o;
	if (o->lector)
		return dcdda_abre(d);
	d->a = so_abre(o->ruta);
	if (!d->a) {
		copia(d->error, sizeof d->error, "no se puede abrir el archivo");
		return -1;
	}
	if (o->largo) {
		/* (una pista de una imagen .BIN: PCM en crudo) */
		org_salta(d, 0);
		return dcdda_abre(d);
	}
	n = org_lee(d, cab, sizeof cab);
	org_salta(d, 0);
	d->formato = dec_formato(cab, n < 0 ? 0 : (u32)n, o->ruta);
	switch (d->formato) {
	case F_MP3: return dmp3_abre(d);
	case F_VORBIS: return dvorbis_abre(d);
	case F_FLAC: return dflac_abre(d);
	case F_WAV: return dwav_abre(d);
	case F_MOD: return dmod_abre(d);
	case F_OPUS:
		copia(d->error, sizeof d->error, "Opus aún no (en la fase siguiente)");
		break;
	default:
		copia(d->error, sizeof d->error, "no es un formato que se sepa tocar");
	}
	return -1;
}

int dec_paso(dec *d)
{
	int r;

	if (d->listo)
		return 1;
	if (!d->paso)
		return -1;
	r = d->paso(d);
	if (r > 0)
		d->listo = 1;
	return r;
}

int dec_lee(dec *d, float *s, int tramas)
{
	int n;

	if (!d->listo || d->acabado || !d->lee)
		return 0;
	n = d->lee(d, s, tramas);
	if (n <= 0) {
		d->acabado = 1;
		return 0;
	}
	d->pos += (u64)n;
	return n;
}

int dec_busca(dec *d, u64 trama)
{
	if (!d->listo || !d->busca || !d->buscable)
		return -1;
	if (d->total && trama > d->total)
		trama = d->total;
	if (d->busca(d, trama))
		return -1;
	d->pos = trama;
	d->acabado = 0;
	return 0;
}

void dec_cierra(dec *d)
{
	if (d->cierra)
		d->cierra(d);
	if (d->a)
		so_cierra(d->a);
	so_suelta(d->mem);
	memset(d, 0, sizeof *d);
}

/* lo de más de dos canales, a dos (los de la izquierda a la izquierda...) */
void a_estereo(const float *e, int canales, float *s, int tramas)
{
	int i, c;

	if (canales == 2) {
		if (e != s)
			memmove(s, e, (size_t)tramas * 2 * sizeof(float));
		return;
	}
	if (canales == 1) {
		for (i = tramas - 1; i >= 0; i--)
			s[2 * i] = s[2 * i + 1] = e[i];
		return;
	}
	for (i = 0; i < tramas; i++) {
		float l = 0, r = 0;
		const float *f = e + (size_t)i * canales;

		/* (el orden de siempre: L R C LFE SL SR...) */
		l = f[0];
		r = f[1];
		if (canales >= 3) {
			l += f[2] * 0.7071f;
			r += f[2] * 0.7071f;
		}
		for (c = 4; c < canales; c++) {
			if (c & 1)
				r += f[c] * 0.7071f;
			else
				l += f[c] * 0.7071f;
		}
		s[2 * i] = l * 0.5f;
		s[2 * i + 1] = r * 0.5f;
	}
}

/* =================================================================== WAV */
typedef struct {
	u32 fmt, bloque, datos, largo_datos, por_bloque;
	int bits, flotante;
	/* ADPCM */
	u8 *b;
	i16 *pcm;
	int en_pcm, pos_pcm;
	i16 coef[32][2];
	int n_coef;
} wav;

static const int ima_paso[89] = { 7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60,
	66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598,
	658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660,
	4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500,
	20350, 22385, 24623, 27086, 29794, 32767 };
static const int ima_indice[16] = { -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8 };
static const int ms_adapta[16] = { 230, 230, 230, 230, 307, 409, 512, 614, 768, 614, 512, 409, 307, 230, 230, 230 };

static i16 sat16(int v)
{
	return (i16)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
}

/* un bloque de IMA-ADPCM a w->pcm (entrelazado); da las tramas */
static int ima_bloque(wav *w, int ch, const u8 *b, u32 n)
{
	int pred[8], ind[8], c, i, k, t = 0;
	u32 p = 4u * ch;

	if (n < p)
		return 0;
	for (c = 0; c < ch; c++) {
		pred[c] = (i16)(b[4 * c] | b[4 * c + 1] << 8);
		ind[c] = b[4 * c + 2] > 88 ? 88 : b[4 * c + 2];
		w->pcm[c] = (i16)pred[c];
	}
	t = 1;
	/* luego, de 4 en 4 octetos por canal (8 muestras) */
	while (p + 4u * ch <= n) {
		for (c = 0; c < ch; c++) {
			for (i = 0; i < 8; i++) {
				int nib = (b[p + 4 * c + i / 2] >> ((i & 1) * 4)) & 15, paso = ima_paso[ind[c]], dif = paso >> 3;

				if (nib & 4) dif += paso;
				if (nib & 2) dif += paso >> 1;
				if (nib & 1) dif += paso >> 2;
				pred[c] = nib & 8 ? pred[c] - dif : pred[c] + dif;
				pred[c] = sat16(pred[c]);
				ind[c] += ima_indice[nib];
				ind[c] = ind[c] < 0 ? 0 : ind[c] > 88 ? 88 : ind[c];
				k = t + i;
				w->pcm[k * ch + c] = (i16)pred[c];
			}
		}
		t += 8;
		p += 4u * ch;
	}
	return t;
}

static int ms_bloque(wav *w, int ch, const u8 *b, u32 n)
{
	int pred[2], delta[2], s1[2], s2[2], c1[2], c2[2], c, t, i;
	u32 p = 0;

	if (ch > 2 || n < 7u * ch)
		return 0;
	for (c = 0; c < ch; c++) {
		pred[c] = b[p++];
		if (pred[c] >= w->n_coef)
			pred[c] = 0;
		c1[c] = w->coef[pred[c]][0];
		c2[c] = w->coef[pred[c]][1];
	}
	for (c = 0; c < ch; c++, p += 2) {
		delta[c] = (i16)(b[p] | b[p + 1] << 8);
		if (delta[c] < 16)
			delta[c] = 16;
	}
	for (c = 0; c < ch; c++, p += 2)
		s1[c] = (i16)(b[p] | b[p + 1] << 8);
	for (c = 0; c < ch; c++, p += 2)
		s2[c] = (i16)(b[p] | b[p + 1] << 8);
	for (c = 0; c < ch; c++) {
		w->pcm[c] = (i16)s2[c];
		w->pcm[ch + c] = (i16)s1[c];
	}
	t = 2;
	i = 0;
	while (p < n) {
		int nib, c0, prd;

		nib = i & 1 ? b[p++] & 15 : b[p] >> 4;
		c0 = ch == 2 ? (i & 1) : 0;
		prd = (s1[c0] * c1[c0] + s2[c0] * c2[c0]) / 256;	/* (s1, s2: 16 bits; c1, c2: 16 bits) */
		prd += (nib & 8 ? nib - 16 : nib) * delta[c0];
		prd = sat16(prd);
		s2[c0] = s1[c0];
		s1[c0] = prd;
		delta[c0] = ms_adapta[nib] * delta[c0] / 256;
		if (delta[c0] < 16)
			delta[c0] = 16;
		if (delta[c0] > (1 << 20))
			delta[c0] = 1 << 20;	/* (un archivo malo: que no desborde) */
		w->pcm[t * ch + c0] = (i16)prd;
		i++;
		if (ch == 1 || !(i & 1))
			t++;
		if (t >= (int)w->por_bloque)
			break;
	}
	return t;
}

static int wav_lee(dec *d, float *s, int tramas)
{
	wav *w = d->priv;
	int ch = d->canales, hechas = 0, i, c;
	u8 b[8192];

	if (w->fmt == 0x11 || w->fmt == 2) {
		while (hechas < tramas) {
			if (w->pos_pcm >= w->en_pcm) {
				int k = org_lee(d, w->b, w->bloque);

				if (k <= 0)
					break;
				w->en_pcm = w->fmt == 0x11 ? ima_bloque(w, ch, w->b, (u32)k) : ms_bloque(w, ch, w->b, (u32)k);
				w->pos_pcm = 0;
				if (w->en_pcm <= 0)
					break;
			}
			while (hechas < tramas && w->pos_pcm < w->en_pcm) {
				float t[8];

				for (c = 0; c < ch; c++)
					t[c] = w->pcm[w->pos_pcm * ch + c] * (1.0f / 32768);
				a_estereo(t, ch, s + 2 * hechas, 1);
				hechas++;
				w->pos_pcm++;
			}
		}
		return hechas;
	}
	while (hechas < tramas) {
		u32 por = w->bloque, quiero = (u32)(tramas - hechas), k;
		int n;
		float t[8];

		if (quiero * por > sizeof b)
			quiero = sizeof b / por;
		n = org_lee(d, b, quiero * por);
		if (n <= 0)
			break;
		k = (u32)n / por;
		if (!k)
			break;
		for (i = 0; i < (int)k; i++) {
			const u8 *p = b + i * por;

			for (c = 0; c < ch && c < 8; c++) {
				switch (w->bits) {
				case 8: t[c] = (p[c] - 128) * (1.0f / 128); break;
				case 16: t[c] = (i16)(p[2 * c] | p[2 * c + 1] << 8) * (1.0f / 32768); break;
				case 24: t[c] = (i32)((u32)p[3 * c] << 8 | (u32)p[3 * c + 1] << 16 | (u32)p[3 * c + 2] << 24) * (1.0f / 2147483648.0f); break;
				case 32:
					if (w->flotante) {
						union { u32 u; float f; } v;

						v.u = (u32)p[4 * c] | (u32)p[4 * c + 1] << 8 | (u32)p[4 * c + 2] << 16 | (u32)p[4 * c + 3] << 24;
						t[c] = v.f;
					} else
						t[c] = (i32)((u32)p[4 * c] | (u32)p[4 * c + 1] << 8 | (u32)p[4 * c + 2] << 16 | (u32)p[4 * c + 3] << 24) * (1.0f / 2147483648.0f);
					break;
				case 64: {
					union { u64 u; double f; } v;
					int j;

					v.u = 0;
					for (j = 7; j >= 0; j--)
						v.u = v.u << 8 | p[8 * c + j];
					t[c] = (float)v.f;
					break;
				}
				}
			}
			a_estereo(t, ch > 8 ? 8 : ch, s + 2 * (hechas + i), 1);
		}
		hechas += (int)k;
		if ((u32)n < quiero * por)
			break;
	}
	return hechas;
}

static int wav_busca(dec *d, u64 trama)
{
	wav *w = d->priv;

	if (w->fmt == 0x11 || w->fmt == 2) {
		u64 b = trama / w->por_bloque;

		if (org_salta(d, w->datos + b * w->bloque))
			return -1;
		w->en_pcm = w->pos_pcm = 0;
		/* (lo que sobra del bloque, leyéndolo) */
		if (trama % w->por_bloque) {
			int k = org_lee(d, w->b, w->bloque);

			if (k > 0) {
				w->en_pcm = w->fmt == 0x11 ? ima_bloque(w, d->canales, w->b, (u32)k) : ms_bloque(w, d->canales, w->b, (u32)k);
				w->pos_pcm = (int)(trama % w->por_bloque);
			}
		}
		return 0;
	}
	return org_salta(d, w->datos + trama * w->bloque);
}

static void wav_cierra(dec *d)
{
	wav *w = d->priv;

	if (w) {
		so_suelta(w->b);
		so_suelta(w->pcm);
		so_suelta(w);
	}
	d->priv = NULL;
}

static int wav_paso(dec *d)
{
	(void)d;
	return 1;
}

int dwav_abre(dec *d)
{
	u8 c[64];
	u32 pos = 12, tam, fin;
	int vista_fmt = 0;
	wav *w = so_pide(sizeof *w);

	if (!w)
		return -1;
	memset(w, 0, sizeof *w);
	d->priv = w;
	d->cierra = wav_cierra;
	fin = (u32)(org_largo(d) > 0xFFFFFFF0u ? 0xFFFFFFF0u : org_largo(d));
	for (;;) {
		if (pos + 8 > fin || org_salta(d, pos) || org_lee(d, c, 8) != 8) {
			copia(d->error, sizeof d->error, "WAV sin datos");
			return -1;
		}
		tam = le32(c + 4);
		if (!memcmp(c, "fmt ", 4)) {
			u32 n = tam > 64 ? 64 : tam;
			u8 f[64];

			memset(f, 0, sizeof f);
			if (org_lee(d, f, n) != (int)n || n < 16)
				break;
			w->fmt = le16(f);
			d->canales = le16(f + 2);
			d->hz = le32(f + 4);
			w->bloque = le16(f + 12);
			w->bits = le16(f + 14);
			if (w->fmt == 0xFFFE && n >= 26)
				w->fmt = le16(f + 24);	/* (WAVE_FORMAT_EXTENSIBLE: el subformato) */
			if (w->fmt == 2) {
				/* MS-ADPCM: las muestras por bloque y los coeficientes */
				u8 x[160];
				int i;

				org_salta(d, pos + 8);
				n = tam > sizeof x ? sizeof x : tam;
				org_lee(d, x, n);
				w->por_bloque = n >= 22 ? le16(x + 18) : 0;
				w->n_coef = n >= 22 ? le16(x + 20) : 0;
				if (w->n_coef > 32)
					w->n_coef = 32;
				for (i = 0; i < w->n_coef && 22u + 4 * i + 4 <= n; i++) {
					w->coef[i][0] = (i16)le16(x + 22 + 4 * i);
					w->coef[i][1] = (i16)le16(x + 24 + 4 * i);
				}
			}
			vista_fmt = 1;
		} else if (!memcmp(c, "data", 4)) {
			w->datos = pos + 8;
			w->largo_datos = tam > fin - w->datos ? fin - w->datos : tam;
			break;
		}
		pos += 8 + tam + (tam & 1);
	}
	if (!vista_fmt || !w->datos || d->canales < 1 || d->canales > 8 || d->hz < 1000 || d->hz > 384000 || !w->bloque) {
		copia(d->error, sizeof d->error, "WAV que no se entiende");
		return -1;
	}
	d->bits = w->bits;
	switch (w->fmt) {
	case 1:
		if (w->bits != 8 && w->bits != 16 && w->bits != 24 && w->bits != 32)
			goto no;
		w->bloque = (u32)(w->bits / 8) * d->canales;
		copia(d->nombre_formato, sizeof d->nombre_formato, "WAV (PCM)");
		break;
	case 3:
		if (w->bits != 32 && w->bits != 64)
			goto no;
		w->flotante = 1;
		w->bloque = (u32)(w->bits / 8) * d->canales;
		copia(d->nombre_formato, sizeof d->nombre_formato, "WAV (coma flotante)");
		break;
	case 0x11:
		if (w->bits != 4 || w->bloque < 4u * d->canales || d->canales > 8)
			goto no;
		w->por_bloque = (w->bloque - 4u * d->canales) * 2 / d->canales + 1;
		copia(d->nombre_formato, sizeof d->nombre_formato, "WAV (IMA-ADPCM)");
		break;
	case 2:
		if (w->bits != 4 || d->canales > 2 || w->por_bloque < 2 || !w->n_coef)
			goto no;
		copia(d->nombre_formato, sizeof d->nombre_formato, "WAV (MS-ADPCM)");
		break;
	default:
		goto no;
	}
	if (w->fmt == 0x11 || w->fmt == 2) {
		w->b = so_pide(w->bloque);
		w->pcm = so_pide((size_t)(w->por_bloque + 8) * d->canales * 2);
		if (!w->b || !w->pcm)
			return -1;
		d->total = (u64)(w->largo_datos / w->bloque) * w->por_bloque;
		if (w->largo_datos % w->bloque)
			d->total += w->por_bloque;	/* (el último, a medias: lo que dé) */
	} else
		d->total = w->largo_datos / w->bloque;
	d->kbps = (u32)((u64)w->bloque * d->hz * 8 / 1000);
	if (w->fmt == 0x11 || w->fmt == 2)
		d->kbps = (u32)((u64)w->bloque * 8 * d->hz / w->por_bloque / 1000);
	d->buscable = 1;
	d->paso = wav_paso;
	d->lee = wav_lee;
	d->busca = wav_busca;
	org_salta(d, w->datos);
	if (w->largo_datos && d->o.largo == 0) {
		/* (que no lea lo de detrás de los datos: un LIST, un id3) */
		d->o.desde = 0;
		d->o.largo = w->datos + w->largo_datos;
	}
	return 0;
no:
	copia(d->error, sizeof d->error, "WAV de una clase que no se sabe tocar");
	return -1;
}

/* ============================================================= el CD */
#define SECTOR_CD 2352
#define SECTORES_VEZ 13		/* lo que cabe en una orden por IDE o SATA (32 K) */
typedef struct {
	u8 b[SECTORES_VEZ * SECTOR_CD];
	u32 en_b, pos_b;
	u32 lba;		/* (un CD de verdad) el siguiente que se lee */
	int fallos;
	int motorola;		/* (una imagen) los octetos al revés */
} cdda;

static int cdda_lee(dec *d, float *s, int tramas)
{
	cdda *c = d->priv;
	int hechas = 0;

	while (hechas < tramas) {
		if (c->pos_b >= c->en_b) {
			c->en_b = c->pos_b = 0;
			if (d->o.lector) {
				u32 n = SECTORES_VEZ, fin = d->o.lba + d->o.sectores;

				if (c->lba >= fin)
					break;
				if (n > fin - c->lba)
					n = fin - c->lba;
				if (so_cd_lee(d->o.lector, c->lba, n, c->b)) {
					/* un sector que no se lee (un rayón): silencio, y seguir */
					memset(c->b, 0, n * SECTOR_CD);
					if (++c->fallos > 40)
						break;
				}
				c->lba += n;
				c->en_b = n * SECTOR_CD;
			} else {
				int k = org_lee(d, c->b, sizeof c->b);

				if (k <= 0)
					break;
				c->en_b = (u32)k & ~3u;
			}
		}
		while (hechas < tramas && c->pos_b + 4 <= c->en_b) {
			const u8 *p = c->b + c->pos_b;

			if (c->motorola) {
				s[2 * hechas] = (i16)(p[1] | p[0] << 8) * (1.0f / 32768);
				s[2 * hechas + 1] = (i16)(p[3] | p[2] << 8) * (1.0f / 32768);
			} else {
				s[2 * hechas] = (i16)(p[0] | p[1] << 8) * (1.0f / 32768);
				s[2 * hechas + 1] = (i16)(p[2] | p[3] << 8) * (1.0f / 32768);
			}
			hechas++;
			c->pos_b += 4;
		}
	}
	return hechas;
}

static int cdda_busca(dec *d, u64 trama)
{
	cdda *c = d->priv;
	u64 oct = trama * 4;

	c->en_b = c->pos_b = 0;
	if (d->o.lector) {
		c->lba = d->o.lba + (u32)(oct / SECTOR_CD);
		/* (lo que sobra del sector, se tira al leer) */
		if (oct % SECTOR_CD) {
			float tira[2 * 588];

			cdda_lee(d, tira, (int)((oct % SECTOR_CD) / 4));
		}
		return 0;
	}
	return org_salta(d, oct);
}

static void cdda_cierra(dec *d)
{
	so_suelta(d->priv);
	d->priv = NULL;
}

int dcdda_abre(dec *d)
{
	cdda *c = so_pide(sizeof *c);

	if (!c)
		return -1;
	memset(c, 0, sizeof *c);
	d->priv = c;
	d->cierra = cdda_cierra;
	d->formato = F_CDDA;
	d->hz = 44100;
	d->canales = 2;
	d->bits = 16;
	d->kbps = 1411;
	d->buscable = 1;
	d->paso = wav_paso;
	d->lee = cdda_lee;
	d->busca = cdda_busca;
	if (d->o.lector) {
		c->lba = d->o.lba;
		d->total = (u64)d->o.sectores * (SECTOR_CD / 4);
		copia(d->nombre_formato, sizeof d->nombre_formato, "CD de música");
	} else {
		/* (una pista de una imagen: «desde» y «largo» en octetos; el bit
		 * alto de «lba», los octetos al revés: MOTOROLA en la hoja) */
		c->motorola = (d->o.lba & 0x80000000u) != 0;
		d->total = d->o.largo / 4;
		copia(d->nombre_formato, sizeof d->nombre_formato, "CD de música (imagen)");
	}
	return 0;
}
