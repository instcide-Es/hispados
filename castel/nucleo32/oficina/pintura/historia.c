/* historia.c - Pintura 2: deshacer y rehacer por baldosas (historia.h).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "historia.h"
#include "texto.h"
#include "miniz_cfg.h"

#define TB (BALDOSA * BALDOSA)

typedef struct {
	char nombre[48];
	int entera;		/* la imagen entera (de an x al) */
	int an, al;
	u32 n;			/* (baldosas) cuántas */
	u16 *cual;		/* (baldosas) tx, ty de cada una */
	u8 *z;			/* lo guardado, comprimido (o tal cual si «crudo_tal») */
	u32 zn, crudo;
	int tal_cual;
	int zona[4];
} paso_t;

static lamina *L;
static int bx, by;
static u32 **viejo;
static u8 **cob;
static u32 *toc;		/* las baldosas tocadas en este paso */
static u32 n_toc, cabe_toc;
static int en_paso, perdido;
static paso_t *pila;
static int n_pila, cabe_pila, actual;
static u32 tope = 48u << 20, gastado;

static void suelta_paso(paso_t *p)
{
	if (p->z && !p->tal_cual)
		mz_free(p->z);
	else
		so_suelta(p->z);
	so_suelta(p->cual);
	memset(p, 0, sizeof *p);
}

static u32 lo_que_gasta(const paso_t *p)
{
	return p->zn + p->n * 4 + (u32)sizeof *p;
}

static void olvida_todo(void)
{
	int i;

	for (i = 0; i < n_pila; i++)
		suelta_paso(&pila[i]);
	n_pila = actual = 0;
	gastado = 0;
}

static void suelta_baldosas(void)
{
	u32 i;

	for (i = 0; i < n_toc; i++) {
		so_suelta(viejo[toc[i]]);
		so_suelta(cob[toc[i]]);
		viejo[toc[i]] = NULL;
		cob[toc[i]] = NULL;
	}
	n_toc = 0;
}

void his_lamina(lamina *l, int olvida)
{
	u32 n;

	if (viejo)
		suelta_baldosas();
	so_suelta(viejo);
	so_suelta(cob);
	viejo = NULL;
	cob = NULL;
	en_paso = 0;
	if (olvida)
		olvida_todo();
	L = l;
	bx = (l->an + BALDOSA - 1) / BALDOSA;
	by = (l->al + BALDOSA - 1) / BALDOSA;
	n = (u32)bx * (u32)by;
	viejo = so_pide(n * sizeof *viejo + 4);
	cob = so_pide(n * sizeof *cob + 4);
	if (viejo)
		memset(viejo, 0, n * sizeof *viejo);
	if (cob)
		memset(cob, 0, n * sizeof *cob);
}

void his_suelta(void)
{
	if (viejo)
		suelta_baldosas();
	so_suelta(viejo);
	so_suelta(cob);
	so_suelta(toc);
	viejo = NULL;
	cob = NULL;
	toc = NULL;
	cabe_toc = 0;
	olvida_todo();
	so_suelta(pila);
	pila = NULL;
	cabe_pila = 0;
}

void his_tope(u32 octetos)
{
	tope = octetos;
}

void his_empieza(void)
{
	if (en_paso)
		his_acaba("");
	en_paso = 1;
	perdido = 0;
	n_toc = 0;
}

int his_en_paso(void)
{
	return en_paso;
}

/* el trozo de una baldosa que es de la lámina */
static void medida(u32 t, int *x, int *y, int *an, int *al)
{
	*x = (int)(t % (u32)bx) * BALDOSA;
	*y = (int)(t / (u32)bx) * BALDOSA;
	*an = L->an - *x < BALDOSA ? L->an - *x : BALDOSA;
	*al = L->al - *y < BALDOSA ? L->al - *y : BALDOSA;
}

static void de_lamina(u32 *d, u32 t)
{
	int x, y, an, al, f;

	medida(t, &x, &y, &an, &al);
	for (f = 0; f < al; f++)
		memcpy(d + f * BALDOSA, L->p + (size_t)(y + f) * L->an + x, (size_t)an * 4);
}

static void a_lamina(const u32 *o, u32 t)
{
	int x, y, an, al, f;

	medida(t, &x, &y, &an, &al);
	for (f = 0; f < al; f++)
		memcpy(L->p + (size_t)(y + f) * L->an + x, o + f * BALDOSA, (size_t)an * 4);
}

static int igual_que_lamina(const u32 *o, u32 t)
{
	int x, y, an, al, f;

	medida(t, &x, &y, &an, &al);
	for (f = 0; f < al; f++)
		if (memcmp(L->p + (size_t)(y + f) * L->an + x, o + f * BALDOSA, (size_t)an * 4))
			return 0;
	return 1;
}

/* el paso más viejo, fuera (para hacer sitio) */
static int olvida_el_mas_viejo(void)
{
	if (actual < 1)
		return 0;
	gastado -= lo_que_gasta(&pila[0]);
	suelta_paso(&pila[0]);
	memmove(pila, pila + 1, (size_t)(n_pila - 1) * sizeof *pila);
	n_pila--;
	actual--;
	return 1;
}

static void *pide_o_haz_sitio(size_t n)
{
	void *p = so_pide(n);

	while (!p && olvida_el_mas_viejo())
		p = so_pide(n);
	return p;
}

int his_toca(int x0, int y0, int x1, int y1)
{
	int tx, ty, t0x, t0y, t1x, t1y;

	if (!en_paso || !viejo || !L->p)
		return 0;
	if (x0 < 0)
		x0 = 0;
	if (y0 < 0)
		y0 = 0;
	if (x1 > L->an)
		x1 = L->an;
	if (y1 > L->al)
		y1 = L->al;
	if (x1 <= x0 || y1 <= y0)
		return 1;
	t0x = x0 / BALDOSA;
	t0y = y0 / BALDOSA;
	t1x = (x1 - 1) / BALDOSA;
	t1y = (y1 - 1) / BALDOSA;
	for (ty = t0y; ty <= t1y; ty++)
		for (tx = t0x; tx <= t1x; tx++) {
			u32 t = (u32)ty * (u32)bx + (u32)tx;

			if (viejo[t])
				continue;
			if (n_toc >= cabe_toc) {
				u32 c = cabe_toc ? cabe_toc * 2 : 256;
				u32 *n = so_repide(toc, c * 4);

				if (!n) {
					perdido = 1;
					return 0;
				}
				toc = n;
				cabe_toc = c;
			}
			viejo[t] = pide_o_haz_sitio(TB * 4);
			if (!viejo[t]) {
				perdido = 1;
				return 0;
			}
			de_lamina(viejo[t], t);
			toc[n_toc++] = t;
		}
	return 1;
}

u32 his_viejo(int x, int y)
{
	u32 t = (u32)(y / BALDOSA) * (u32)bx + (u32)(x / BALDOSA);

	if (viejo && viejo[t])
		return viejo[t][(y % BALDOSA) * BALDOSA + x % BALDOSA];
	return L->p[(size_t)y * L->an + x];
}

u8 *his_cob(int x, int y)
{
	u32 t = (u32)(y / BALDOSA) * (u32)bx + (u32)(x / BALDOSA);

	if (!cob || !viejo || !viejo[t])
		return NULL;
	if (!cob[t]) {
		cob[t] = pide_o_haz_sitio(TB);
		if (!cob[t])
			return NULL;
		memset(cob[t], 0, TB);
	}
	return cob[t] + (y % BALDOSA) * BALDOSA + x % BALDOSA;
}

static void zona_de(int zona[4], u32 t)
{
	int x, y, an, al;

	medida(t, &x, &y, &an, &al);
	if (zona[2] <= zona[0]) {
		zona[0] = x;
		zona[1] = y;
		zona[2] = x + an;
		zona[3] = y + al;
		return;
	}
	if (x < zona[0])
		zona[0] = x;
	if (y < zona[1])
		zona[1] = y;
	if (x + an > zona[2])
		zona[2] = x + an;
	if (y + al > zona[3])
		zona[3] = y + al;
}

void his_restaura(int zona[4])
{
	u32 i;
	int z[4] = { 0, 0, 0, 0 };

	for (i = 0; i < n_toc; i++) {
		a_lamina(viejo[toc[i]], toc[i]);
		if (cob[toc[i]])
			memset(cob[toc[i]], 0, TB);
		zona_de(z, toc[i]);
	}
	if (zona)
		memcpy(zona, z, sizeof z);
}

void his_cancela(int zona[4])
{
	his_restaura(zona);
	suelta_baldosas();
	en_paso = 0;
}

static u8 *comprime(const void *d, u32 n, u32 *zn, int *tal_cual)
{
	size_t k = 0;
	void *z = tdefl_compress_mem_to_heap(d, n, &k, 1 | TDEFL_GREEDY_PARSING_FLAG);

	if (z && k < n) {
		*zn = (u32)k;
		*tal_cual = 0;
		return z;
	}
	if (z)
		mz_free(z);
	/* (no se comprime: tal cual) */
	z = so_pide(n ? n : 1);
	if (!z)
		return NULL;
	memcpy(z, d, n);
	*zn = n;
	*tal_cual = 1;
	return z;
}

static int descomprime(const paso_t *p, void *d)
{
	if (p->tal_cual) {
		memcpy(d, p->z, p->crudo);
		return 1;
	}
	return tinfl_decompress_mem_to_mem(d, p->crudo, p->z, p->zn, 0) == p->crudo;
}

static int mete(paso_t *p)
{
	int i;

	/* lo que se podía rehacer, fuera */
	for (i = actual; i < n_pila; i++) {
		gastado -= lo_que_gasta(&pila[i]);
		suelta_paso(&pila[i]);
	}
	n_pila = actual;
	if (n_pila >= cabe_pila) {
		int c = cabe_pila ? cabe_pila * 2 : 64;
		paso_t *n = so_repide(pila, (size_t)c * sizeof *pila);

		if (!n)
			return 0;
		pila = n;
		cabe_pila = c;
	}
	pila[n_pila++] = *p;
	actual = n_pila;
	gastado += lo_que_gasta(p);
	while (gastado > tope && actual > 1)
		olvida_el_mas_viejo();
	return 1;
}

void his_acaba(const char *nombre)
{
	paso_t p;
	u32 i, k = 0;
	u32 *buf;

	if (!en_paso)
		return;
	en_paso = 0;
	memset(&p, 0, sizeof p);
	copia(p.nombre, nombre, sizeof p.nombre);
	/* las que han cambiado de verdad */
	for (i = 0; i < n_toc; i++)
		if (!igual_que_lamina(viejo[toc[i]], toc[i]))
			toc[k++] = toc[i];
		else {
			so_suelta(viejo[toc[i]]);
			so_suelta(cob[toc[i]]);
			viejo[toc[i]] = NULL;
			cob[toc[i]] = NULL;
		}
	n_toc = k;
	if (!n_toc || perdido) {
		if (perdido && n_toc)
			dice("pintura: «%s» no se podrá deshacer (no hay memoria)", nombre);
		suelta_baldosas();
		return;
	}
	buf = so_pide((size_t)n_toc * TB * 4);
	p.cual = so_pide(n_toc * 4);
	if (!buf || !p.cual) {
		so_suelta(buf);
		so_suelta(p.cual);
		suelta_baldosas();
		dice("pintura: «%s» no se podrá deshacer (no hay memoria)", nombre);
		return;
	}
	for (i = 0; i < n_toc; i++) {
		memcpy(buf + (size_t)i * TB, viejo[toc[i]], TB * 4);
		p.cual[2 * i] = (u16)(toc[i] % (u32)bx);
		p.cual[2 * i + 1] = (u16)(toc[i] / (u32)bx);
		zona_de(p.zona, toc[i]);
	}
	p.n = n_toc;
	p.crudo = n_toc * TB * 4;
	suelta_baldosas();
	p.z = comprime(buf, p.crudo, &p.zn, &p.tal_cual);
	so_suelta(buf);
	if (!p.z || !mete(&p)) {
		suelta_paso(&p);
		return;
	}
}

int his_entera(const char *nombre, lamina *antes)
{
	paso_t p;

	if (en_paso)
		his_acaba("");
	memset(&p, 0, sizeof p);
	copia(p.nombre, nombre, sizeof p.nombre);
	p.entera = 1;
	p.an = antes->an;
	p.al = antes->al;
	p.crudo = (u32)antes->an * (u32)antes->al * 4;
	p.z = comprime(antes->p, p.crudo, &p.zn, &p.tal_cual);
	lam_suelta(antes);
	p.zona[2] = L->an;
	p.zona[3] = L->al;
	if (!p.z)
		return 0;
	if (!mete(&p)) {
		suelta_paso(&p);
		return 0;
	}
	return 1;
}

int his_se_deshace(void)
{
	return actual > 0;
}

int his_se_rehace(void)
{
	return actual < n_pila;
}

const char *his_que_deshace(void)
{
	return actual > 0 ? pila[actual - 1].nombre : "";
}

const char *his_que_rehace(void)
{
	return actual < n_pila ? pila[actual].nombre : "";
}

/* el paso, al revés: lo guardado va a la lámina y lo de la lámina se guarda */
static int da_la_vuelta(paso_t *p, lamina *l, int *tamano, int zona[4])
{
	u8 *z;
	u32 zn;
	int tal;

	*tamano = 0;
	if (p->entera) {
		lamina n;

		if (!lam_nueva(&n, p->an, p->al, 0))
			return 0;
		if (!descomprime(p, n.p)) {
			lam_suelta(&n);
			return 0;
		}
		z = comprime(l->p, (u32)l->an * (u32)l->al * 4, &zn, &tal);
		if (!z) {
			lam_suelta(&n);
			return 0;
		}
		gastado -= lo_que_gasta(p);
		if (p->tal_cual)
			so_suelta(p->z);
		else
			mz_free(p->z);
		p->z = z;
		p->zn = zn;
		p->tal_cual = tal;
		p->crudo = (u32)l->an * (u32)l->al * 4;
		*tamano = p->an != l->an || p->al != l->al;
		p->an = l->an;
		p->al = l->al;
		lam_suelta(l);
		*l = n;
		gastado += lo_que_gasta(p);
		his_lamina(l, 0);
		zona[0] = zona[1] = 0;
		zona[2] = l->an;
		zona[3] = l->al;
		return 1;
	}
	{
		u32 *buf = so_pide(p->crudo), i;

		if (!buf)
			return 0;
		if (!descomprime(p, buf)) {
			so_suelta(buf);
			return 0;
		}
		for (i = 0; i < p->n; i++) {
			u32 t = (u32)p->cual[2 * i + 1] * (u32)bx + p->cual[2 * i], tmp[64];
			int x, y, an, al, f;

			if (p->cual[2 * i] >= bx || p->cual[2 * i + 1] >= by)
				continue;
			medida(t, &x, &y, &an, &al);
			/* (cambiar la baldosa guardada por la de la lámina, fila a fila) */
			for (f = 0; f < al; f++) {
				u32 *a = L->p + (size_t)(y + f) * L->an + x, *b = buf + (size_t)i * TB + f * BALDOSA;

				memcpy(tmp, a, (size_t)an * 4);
				memcpy(a, b, (size_t)an * 4);
				memcpy(b, tmp, (size_t)an * 4);
			}
		}
		z = comprime(buf, p->crudo, &zn, &tal);
		so_suelta(buf);
		if (!z)
			return 0;
		gastado -= lo_que_gasta(p);
		if (p->tal_cual)
			so_suelta(p->z);
		else
			mz_free(p->z);
		p->z = z;
		p->zn = zn;
		p->tal_cual = tal;
		gastado += lo_que_gasta(p);
		memcpy(zona, p->zona, sizeof p->zona);
		return 1;
	}
}

int his_deshace(lamina *l, int *tamano, int zona[4])
{
	if (en_paso)
		his_acaba("");
	if (actual < 1)
		return 0;
	if (!da_la_vuelta(&pila[actual - 1], l, tamano, zona))
		return 0;
	actual--;
	return 1;
}

int his_rehace(lamina *l, int *tamano, int zona[4])
{
	if (en_paso)
		his_acaba("");
	if (actual >= n_pila)
		return 0;
	if (!da_la_vuelta(&pila[actual], l, tamano, zona))
		return 0;
	actual++;
	return 1;
}

u32 his_memoria(void)
{
	return gastado;
}

int his_pasos(void)
{
	return n_pila;
}
