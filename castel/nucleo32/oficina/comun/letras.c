/* letras.c - la oficina de 32 bits: las letras (oficina/comun/letras.h).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include <stdlib.h>
#include "letras.h"
#include "texto.h"

#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_assert(x) ((void)0)
#define STBTT_malloc(x, u) ((void)(u), so_pide(x))
#define STBTT_free(x, u) ((void)(u), so_suelta(x))
#define STBTT_ifloor(x) ((int)so_suelo(x))
#define STBTT_iceil(x) ((int)so_techo(x))
#define STBTT_sqrt(x) so_raiz(x)
#define STBTT_fabs(x) so_abs(x)
#define STBTT_pow(x, y) pow(x, y)
#define STBTT_fmod(x, y) fmod(x, y)
#define STBTT_cos(x) cos(x)
#define STBTT_acos(x) acos(x)
#define STBTT_strlen(x) largo(x)
#define STBTT_memcpy memcpy
#define STBTT_memset memset
#include <math.h>
#include "stb_truetype.h"

typedef struct {
	ttf t;
	stbtt_fontinfo f;
	int bien;
	int propia;		/* del usuario (no de dentro) */
	u8 *mia;		/* (los datos, si son nuestros) */
	short avance[0x180];	/* las de Europa, en milésimas (-1: sin mirar) */
} cara_t;

static cara_t caras[LETRAS_MAX];
static int n_caras;
static int dv_sans = -1, dv_serif = -1, dv_mono = -1, dv_sans_n = -1, dv_serif_n = -1, dv_mono_n = -1;

void letras_dejavu(void);	/* (de letras_dejavu.c, que hace haz_letras.py) */

int letras_mete(const u8 *d, u32 n, int propia)
{
	cara_t *c;
	int i;

	if (n_caras >= LETRAS_MAX)
		return -1;
	c = &caras[n_caras];
	memset(c, 0, sizeof *c);
	if (!ttf_abre(&c->t, d, n))
		return -1;
	if (!stbtt_InitFont(&c->f, d, 0)) {
		ttf_cierra(&c->t);
		return -1;
	}
	/* (la misma familia y estilo dos veces: la primera) */
	for (i = 0; i < n_caras; i++)
		if (iguales_sin_caja(caras[i].t.familia, c->t.familia) && caras[i].t.negrita == c->t.negrita && caras[i].t.cursiva == c->t.cursiva) {
			ttf_cierra(&c->t);
			return -1;
		}
	c->bien = 1;
	c->propia = propia;
	for (i = 0; i < 0x180; i++)
		c->avance[i] = -1;
	return n_caras++;
}

void letras_inicia(void)
{
	int i;

	if (n_caras)
		return;
	letras_dejavu();
	for (i = 0; i < n_caras; i++) {
		const ttf *t = &caras[i].t;

		if (iguales(t->familia, FAM_SANS))
			*(t->negrita ? &dv_sans_n : &dv_sans) = i;
		else if (iguales(t->familia, FAM_SERIF))
			*(t->negrita ? &dv_serif_n : &dv_serif) = i;
		else if (iguales(t->familia, FAM_MONO))
			*(t->negrita ? &dv_mono_n : &dv_mono) = i;
	}
	if (dv_sans < 0)
		dv_sans = 0;
	if (dv_serif < 0)
		dv_serif = dv_sans;
	if (dv_mono < 0)
		dv_mono = dv_sans;
	if (dv_sans_n < 0)
		dv_sans_n = dv_sans;
	if (dv_serif_n < 0)
		dv_serif_n = dv_serif;
	if (dv_mono_n < 0)
		dv_mono_n = dv_mono;
}

int letras_carpeta(const char *ruta)
{
	void *c = so_abre_carpeta(ruta);
	so_entrada e;
	int k = 0;

	if (!c)
		return 0;
	while (so_lee_carpeta(c, &e) && n_caras < LETRAS_MAX) {
		char ext[8], r[600];
		u8 *d;
		u32 n;

		extension(e.nombre, ext);
		if (e.carpeta || !iguales(ext, "TTF") || e.tamano > 24u * 1024 * 1024)
			continue;
		fmt(r, sizeof r, "%s\\%s", ruta, e.nombre);
		d = so_lee_archivo(r, &n, 24u * 1024 * 1024, NULL);
		if (!d)
			continue;
		if (letras_mete(d, n, 1) >= 0) {
			caras[n_caras - 1].mia = d;
			k++;
		} else
			so_suelta(d);
	}
	so_cierra_carpeta(c);
	return k;
}

int letras_familias(const char *lista[], int max)
{
	int i, j, n = 0;

	for (i = 0; i < n_caras; i++) {
		const char *f = caras[i].t.familia;

		for (j = 0; j < n; j++)
			if (iguales_sin_caja(lista[j], f))
				break;
		if (j < n || n >= max)
			continue;
		/* por orden (sin mirar mayúsculas) */
		for (j = n; j > 0; j--) {
			const char *a = lista[j - 1], *b = f;

			while (*a && (*a | 32) == (*b | 32))
				a++, b++;
			if ((u8)(*a | 32) <= (u8)(*b | 32))
				break;
			lista[j] = lista[j - 1];
		}
		lista[j] = f;
		n++;
	}
	return n;
}

int letra_hay_familia(const char *familia)
{
	int i;

	for (i = 0; i < n_caras; i++)
		if (iguales_sin_caja(caras[i].t.familia, familia))
			return 1;
	return 0;
}

/* ¿lleva p dentro (sin mirar mayúsculas)? */
static int lleva(const char *s, const char *p)
{
	u32 n = largo(p), i;

	for (; *s; s++) {
		for (i = 0; i < n; i++)
			if ((s[i] | 32) != (p[i] | 32))
				break;
		if (i == n)
			return 1;
	}
	return 0;
}

/* la DejaVu que más se parece a una familia que no está: 0 sans, 1 serif, 2 mono */
static int parecida(const char *f)
{
	static const char *const mono[] = { "mono", "courier", "consol", "code", "typewriter", "máquina", "maquina", "hispa", "lucida console", "fixed", 0 };
	static const char *const serif[] = { "serif", "times", "roman", "georgia", "garamond", "cambria", "book", "palatino", "baskerville",
		"didot", "bodoni", "charter", "century", "libertine", "roble", "imprenta", "carta", "clásica", "clasica", "elegante",
		"pergamino", "losa", "slab", "minion", "caslon", "tinos", "liberation serif", "noto serif", "constantia", 0 };
	int i;

	if (lleva(f, "sans"))
		return lleva(f, "mono") ? 2 : 0;
	for (i = 0; mono[i]; i++)
		if (lleva(f, mono[i]))
			return 2;
	for (i = 0; serif[i]; i++)
		if (lleva(f, serif[i]))
			return 1;
	return 0;
}

int letra_cara(const char *familia, int negrita, int cursiva, int *falsas)
{
	int i, mejor = -1, nota = -1;

	if (!n_caras)
		letras_inicia();
	for (i = 0; i < n_caras; i++) {
		const ttf *t = &caras[i].t;
		int v;

		if (!iguales_sin_caja(t->familia, familia))
			continue;
		v = (t->negrita == negrita ? 2 : 0) + (t->cursiva == cursiva ? 1 : 0);
		/* (una negrita no vale de normal) */
		if (t->negrita && !negrita)
			v -= 4;
		if (t->cursiva && !cursiva)
			v -= 4;
		if (v > nota) {
			nota = v;
			mejor = i;
		}
	}
	if (mejor < 0) {
		int p = parecida(familia);

		mejor = p == 1 ? (negrita ? dv_serif_n : dv_serif) : p == 2 ? (negrita ? dv_mono_n : dv_mono) : (negrita ? dv_sans_n : dv_sans);
	}
	if (falsas) {
		*falsas = 0;
		if (negrita && !caras[mejor].t.negrita)
			*falsas |= FALSA_NEGRITA;
		if (cursiva && !caras[mejor].t.cursiva)
			*falsas |= FALSA_CURSIVA;
	}
	return mejor;
}

const char *letra_familia(int cara)
{
	return cara >= 0 && cara < n_caras ? caras[cara].t.familia : FAM_SANS;
}

ttf *letra_ttf(int cara)
{
	return &caras[cara].t;
}

u32 letra_em(int cara)
{
	return caras[cara].t.em;
}

u32 letra_glifo(int cara, u32 c, int *cara_real)
{
	u32 g;
	int r;

	if (cara < 0 || cara >= n_caras)
		cara = dv_sans;
	g = ttf_glifo(&caras[cara].t, c);
	if (g && ttf_glifo_vale(&caras[cara].t, g)) {
		if (cara_real)
			*cara_real = cara;
		return g;
	}
	/* el respaldo: la DejaVu del mismo estilo (y, si no, la Sans) */
	r = caras[cara].t.negrita ? dv_sans_n : dv_sans;
	if (lleva(caras[cara].t.familia, "serif") || parecida(caras[cara].t.familia) == 1)
		r = caras[cara].t.negrita ? dv_serif_n : dv_serif;
	else if (parecida(caras[cara].t.familia) == 2)
		r = caras[cara].t.negrita ? dv_mono_n : dv_mono;
	g = ttf_glifo(&caras[r].t, c);
	if (!g) {
		r = dv_sans;
		g = ttf_glifo(&caras[r].t, c);
	}
	if (cara_real)
		*cara_real = r;
	return g;
}

int letra_avance(int cara, u32 glifo)
{
	int a, l;

	stbtt_GetGlyphHMetrics(&caras[cara].f, (int)glifo, &a, &l);
	return a;
}

int letra_milesimas(int cara, u32 c)
{
	int r;
	u32 g;

	if (cara < 0 || cara >= n_caras)
		cara = dv_sans;
	if (c < 0x180 && caras[cara].avance[c] >= 0)
		return caras[cara].avance[c];
	g = letra_glifo(cara, c, &r);
	{
		int v = (int)(((i64)letra_avance(r, g) * 1000 + caras[r].t.em / 2) / caras[r].t.em);

		if (c < 0x180)
			caras[cara].avance[c] = (short)v;
		return v;
	}
}

/* ---- los dibujos guardados ---- */
typedef struct guardado {
	struct guardado *sig;
	u32 clave1, clave2;
	glifo_px g;
} guardado;
#define CUBOS 4096
static guardado *cubo[CUBOS];
static u32 gastado;
static u8 gamma_t[256];

u32 letras_memoria(void)
{
	return gastado;
}

void letras_olvida(void)
{
	int i;

	for (i = 0; i < CUBOS; i++)
		while (cubo[i]) {
			guardado *s = cubo[i]->sig;

			so_suelta(cubo[i]);
			cubo[i] = s;
		}
	gastado = 0;
}

const glifo_px *letra_dibujo(int cara, u32 glifo, int tam, int sub, int falsas)
{
	u32 k1 = (u32)cara << 24 | (glifo & 0xFFFFFF), k2 = (u32)tam << 8 | (u32)sub << 4 | (u32)falsas, h;
	guardado *s;
	cara_t *c;
	float esc;
	int x0, y0, x1, y1, an, al, extra = 0, inclina = 0, i, j;
	u8 *tmp;

	if (cara < 0 || cara >= n_caras || tam < 4 || tam > 4 * 600)
		return NULL;
	h = (k1 * 2654435761u ^ k2 * 40503u) % CUBOS;
	for (s = cubo[h]; s; s = s->sig)
		if (s->clave1 == k1 && s->clave2 == k2)
			return &s->g;
	if (!gamma_t[255]) {
		/* (un poco más oscuras: la mezcla es lineal y las letras finas se pierden) */
		for (i = 0; i < 256; i++) {
			double v = i / 255.0;

			gamma_t[i] = (u8)(pow(v, 0.78) * 255 + 0.5);
		}
	}
	if (gastado > 6u * 1024 * 1024)
		letras_olvida();
	c = &caras[cara];
	if (!ttf_glifo_vale(&c->t, glifo))
		glifo = 0;
	esc = stbtt_ScaleForMappingEmToPixels(&c->f, tam / 4.0f);
	stbtt_GetGlyphBitmapBoxSubpixel(&c->f, (int)glifo, esc, esc, sub / 3.0f, 0, &x0, &y0, &x1, &y1);
	an = x1 - x0;
	al = y1 - y0;
	if (an < 0 || al < 0 || an > 2000 || al > 2000)
		an = al = 0;
	if (falsas & FALSA_NEGRITA)
		extra = tam / 4 / 24 + 1;
	if (falsas & FALSA_CURSIVA)
		inclina = (al * 21 + 50) / 100 + 1;	/* (unos 12 grados) */
	s = so_pide(sizeof(guardado) + (size_t)(an + extra + inclina) * (al ? al : 1));
	if (!s)
		return NULL;
	s->clave1 = k1;
	s->clave2 = k2;
	s->g.gris = (u8 *)(s + 1);
	s->g.an = (short)(an + extra + inclina);
	s->g.al = (short)al;
	s->g.x0 = (short)x0;
	s->g.y0 = (short)y0;
	memset(s->g.gris, 0, (size_t)s->g.an * al);
	if (an && al) {
		tmp = so_pide((size_t)an * al);
		if (tmp) {
			stbtt_MakeGlyphBitmapSubpixel(&c->f, tmp, an, al, an, esc, esc, sub / 3.0f, 0, (int)glifo);
			for (j = 0; j < al; j++) {
				/* la cursiva hecha a mano: cada fila, corrida a la derecha lo que
				 * le toca por su altura (con la parte de punto repartida) */
				int fx = inclina ? (int)(((i64)(y1 - (y0 + j)) * 21 * 256) / 100) : 0;
				int ent = fx >> 8, fr = fx & 255;

				for (i = 0; i < an; i++) {
					int v = tmp[j * an + i], e;

					if (!v)
						continue;
					for (e = 0; e <= extra; e++) {
						int x = i + e + ent;
						u8 *p = s->g.gris + j * s->g.an;
						int a = v * (256 - fr) >> 8, b = v * fr >> 8;

						if (x < s->g.an)
							p[x] = (u8)(p[x] + a > 255 ? 255 : p[x] + a);
						if (fr && x + 1 < s->g.an)
							p[x + 1] = (u8)(p[x + 1] + b > 255 ? 255 : p[x + 1] + b);
					}
				}
				for (i = 0; i < s->g.an; i++)
					s->g.gris[j * s->g.an + i] = gamma_t[s->g.gris[j * s->g.an + i]];
			}
			so_suelta(tmp);
		}
	}
	if (inclina)
		s->g.x0 = (short)(s->g.x0 - (int)(((i64)(y1) * 21) / 100) + 0);
	s->sig = cubo[h];
	cubo[h] = s;
	gastado += (u32)(sizeof(guardado) + (u32)s->g.an * (u32)al);
	return &s->g;
}
