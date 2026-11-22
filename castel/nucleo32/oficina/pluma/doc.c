/* doc.c - Pluma 2: el documento (pluma/doc.h).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "doc.h"
#include "texto.h"
#include "imagen.h"
#include "letras.h"

static void *crece(void *p, u32 *cabe, u32 hace_falta, u32 tam)
{
	u32 c;
	void *n;

	if (hace_falta <= *cabe)
		return p;
	c = *cabe ? *cabe : 8;
	while (c < hace_falta)
		c *= 2;
	n = so_repide(p, (size_t)c * tam);
	if (!n)
		return NULL;
	memset((u8 *)n + (size_t)*cabe * tam, 0, (size_t)(c - *cabe) * tam);
	*cabe = c;
	return n;
}

fmt_letra fmt_letra_normal(void)
{
	fmt_letra f;

	memset(&f, 0, sizeof f);
	copia(f.familia, FAM_SERIF, sizeof f.familia);
	f.tam = 24;
	f.color = SIN_COLOR;
	f.resalte = SIN_COLOR;
	return f;
}

fmt_parrafo fmt_parrafo_normal(void)
{
	fmt_parrafo p;

	memset(&p, 0, sizeof p);
	p.alin = AL_IZQ;
	p.inter_tipo = INTER_MULTIPLE;
	p.inter = 115;
	p.despues = 8 * TW_PT;
	p.fondo = SIN_COLOR;
	return p;
}

int fmt_iguales(const fmt_letra *a, const fmt_letra *b)
{
	return a->tam == b->tam && a->negrita == b->negrita && a->cursiva == b->cursiva && a->subrayado == b->subrayado &&
	       a->tachado == b->tachado && a->vert == b->vert && a->versalitas == b->versalitas && a->color == b->color &&
	       a->resalte == b->resalte && iguales(a->familia, b->familia);
}

u16 doc_fmt(documento *d, const fmt_letra *f)
{
	u32 i;
	fmt_letra *n;

	for (i = d->n_fl; i-- > 0;)
		if (fmt_iguales(&d->fl[i], f))
			return (u16)i;
	if (d->n_fl >= 0xFFF0)
		return 0;	/* (no caben más: el primero) */
	n = crece(d->fl, &d->cabe_fl, d->n_fl + 1, sizeof(fmt_letra));
	if (!n) {
		d->sin_memoria = 1;
		return 0;
	}
	d->fl = n;
	d->fl[d->n_fl] = *f;
	return (u16)d->n_fl++;
}

int doc_estilo(const documento *d, const char *nombre)
{
	u32 i;

	for (i = 0; i < d->n_est; i++)
		if (iguales_sin_caja(d->est[i].nombre, nombre))
			return (int)i;
	return -1;
}

int doc_nuevo_estilo(documento *d, const estilo *e)
{
	estilo *n = crece(d->est, &d->cabe_est, d->n_est + 1, sizeof(estilo));

	if (!n || d->n_est >= 0xFFF0) {
		d->sin_memoria = 1;
		return 0;
	}
	d->est = n;
	d->est[d->n_est] = *e;
	return (int)d->n_est++;
}

u16 doc_fmt_estilo(documento *d, int e)
{
	if (e < 0 || (u32)e >= d->n_est)
		e = 0;
	return doc_fmt(d, &d->est[e].letra);
}

void doc_estilos_de_serie(documento *d)
{
	static const struct {
		const char *nombre;
		const char *familia;
		u16 tam;
		u8 negrita, cursiva, alin, nivel;
		i32 antes, despues, sang;
		u32 color;
		u8 sig;
	} t[EST_N] = {
		{ "Normal", FAM_SERIF, 24, 0, 0, AL_IZQ, 0, 0, 8 * TW_PT, 0, SIN_COLOR, EST_NORMAL },
		{ "Título", FAM_SANS, 52, 1, 0, AL_IZQ, 0, 0, 12 * TW_PT, 0, 0x1F3864, EST_NORMAL },
		{ "Título 1", FAM_SANS, 32, 1, 0, AL_IZQ, 1, 18 * TW_PT, 6 * TW_PT, 0, 0x2F5496, EST_NORMAL },
		{ "Título 2", FAM_SANS, 26, 1, 0, AL_IZQ, 2, 12 * TW_PT, 4 * TW_PT, 0, 0x2F5496, EST_NORMAL },
		{ "Título 3", FAM_SANS, 24, 1, 1, AL_IZQ, 3, 10 * TW_PT, 4 * TW_PT, 0, 0x1F3864, EST_NORMAL },
		{ "Subtítulo", FAM_SANS, 30, 0, 0, AL_IZQ, 0, 0, 12 * TW_PT, 0, 0x595959, EST_NORMAL },
		{ "Cita", FAM_SERIF, 24, 0, 1, AL_IZQ, 0, 6 * TW_PT, 10 * TW_PT, 720, 0x404040, EST_NORMAL },
		{ "Párrafo de lista", FAM_SERIF, 24, 0, 0, AL_IZQ, 0, 0, 4 * TW_PT, 720, SIN_COLOR, EST_LISTA },
		{ "Encabezado", FAM_SANS, 18, 0, 0, AL_CENTRO, 0, 0, 0, 0, 0x595959, EST_ENC },
		{ "Pie de página", FAM_SANS, 18, 0, 0, AL_CENTRO, 0, 0, 0, 0, 0x595959, EST_PIE },
		{ "Nota al pie", FAM_SERIF, 18, 0, 0, AL_IZQ, 0, 0, 0, 0, SIN_COLOR, EST_NOTA },
		{ "Sin espaciado", FAM_SERIF, 24, 0, 0, AL_IZQ, 0, 0, 0, 0, SIN_COLOR, EST_SIN_ESPACIO },
	};
	int i;

	d->n_est = 0;
	for (i = 0; i < EST_N; i++) {
		estilo e;

		memset(&e, 0, sizeof e);
		copia(e.nombre, t[i].nombre, sizeof e.nombre);
		e.letra = fmt_letra_normal();
		copia(e.letra.familia, t[i].familia, sizeof e.letra.familia);
		e.letra.tam = t[i].tam;
		e.letra.negrita = t[i].negrita;
		e.letra.cursiva = t[i].cursiva;
		e.letra.color = t[i].color;
		e.p = fmt_parrafo_normal();
		e.p.alin = t[i].alin;
		e.p.nivel_titulo = t[i].nivel;
		e.p.antes = t[i].antes;
		e.p.despues = t[i].despues;
		e.p.sang_izq = t[i].sang;
		e.p.estilo = (u16)i;
		if (i == EST_ENC || i == EST_PIE || i == EST_NOTA || i == EST_SIN_ESPACIO || i == EST_TITULO)
			e.p.inter = 100;
		if (i >= EST_T1 && i <= EST_T3)
			e.p.juntos = 1;
		if (i == EST_CITA)
			e.p.sang_der = 720;
		e.siguiente = t[i].sig;
		e.de_serie = 1;
		doc_nuevo_estilo(d, &e);
	}
}

documento *doc_nuevo(void)
{
	documento *d = so_pide(sizeof(documento));

	if (!d)
		return NULL;
	memset(d, 0, sizeof *d);
	d->pag_an = 11906;		/* A4 */
	d->pag_al = 16838;
	d->marg_izq = d->marg_der = 1418;	/* 2,5 cm */
	d->marg_sup = d->marg_inf = 1418;
	d->dist_enc = d->dist_pie = 709;	/* 1,25 cm */
	d->num_inicio = 1;
	doc_estilos_de_serie(d);
	if (!doc_inserta_parrafos(d, 0, 1)) {
		doc_suelta(d);
		return NULL;
	}
	d->enc.p = d->est[EST_ENC].p;
	d->pie.p = d->est[EST_PIE].p;
	d->enc.ancho_maq = d->pie.ancho_maq = -1;
	return d;
}

void par_suelta(parrafo *p)
{
	so_suelta(p->t);
	so_suelta(p->f);
	so_suelta(p->lin);
	so_suelta(p->faltas);
	p->t = NULL;
	p->f = NULL;
	p->lin = NULL;
	p->faltas = NULL;
	p->n = p->cabe = p->n_lin = p->cabe_lin = 0;
}

void doc_suelta(documento *d)
{
	u32 i;

	if (!d)
		return;
	for (i = 0; i < d->n_par; i++)
		par_suelta(&d->par[i]);
	for (i = 0; i < d->n_notas; i++)
		par_suelta(&d->notas[i]);
	for (i = 0; i < d->n_img; i++) {
		so_suelta(d->img[i].datos);
		so_suelta(d->img[i].px);
	}
	par_suelta(&d->enc);
	par_suelta(&d->pie);
	so_suelta(d->par);
	so_suelta(d->notas);
	so_suelta(d->fl);
	so_suelta(d->est);
	so_suelta(d->tab);
	so_suelta(d->img);
	so_suelta(d->pag);
	so_suelta(d->trz);
	so_suelta(d);
}

parrafo *doc_inserta_parrafos(documento *d, u32 en, u32 cuantos)
{
	parrafo *n;
	u32 i;

	if (en > d->n_par)
		en = d->n_par;
	n = crece(d->par, &d->cabe_par, d->n_par + cuantos, sizeof(parrafo));
	if (!n) {
		d->sin_memoria = 1;
		return NULL;
	}
	d->par = n;
	memmove(d->par + en + cuantos, d->par + en, (d->n_par - en) * sizeof(parrafo));
	for (i = 0; i < cuantos; i++) {
		parrafo *p = &d->par[en + i];

		memset(p, 0, sizeof *p);
		p->p = d->n_est ? d->est[0].p : fmt_parrafo_normal();
		p->ancho_maq = -1;
	}
	d->n_par += cuantos;
	d->version++;
	return &d->par[en];
}

void doc_quita_parrafos(documento *d, u32 desde, u32 cuantos)
{
	u32 i;

	if (desde >= d->n_par)
		return;
	if (desde + cuantos > d->n_par)
		cuantos = d->n_par - desde;
	for (i = 0; i < cuantos; i++)
		par_suelta(&d->par[desde + i]);
	memmove(d->par + desde, d->par + desde + cuantos, (d->n_par - desde - cuantos) * sizeof(parrafo));
	d->n_par -= cuantos;
	d->version++;
}

int par_cabe(parrafo *p, u32 n)
{
	u32 c;
	u32 *t;
	u16 *f;

	if (n <= p->cabe)
		return 1;
	c = p->cabe ? p->cabe : 16;
	while (c < n)
		c *= 2;
	t = so_repide(p->t, (size_t)c * 4);
	if (!t)
		return 0;
	p->t = t;
	f = so_repide(p->f, (size_t)c * 2);
	if (!f)
		return 0;
	p->f = f;
	p->cabe = c;
	return 1;
}

int par_inserta(parrafo *p, u32 en, const u32 *t, const u16 *f, u32 n)
{
	if (en > p->n)
		en = p->n;
	if (!n)
		return 1;
	if (!par_cabe(p, p->n + n))
		return 0;
	memmove(p->t + en + n, p->t + en, (p->n - en) * 4);
	memmove(p->f + en + n, p->f + en, (p->n - en) * 2);
	memcpy(p->t + en, t, n * 4);
	if (f)
		memcpy(p->f + en, f, n * 2);
	else {
		u32 i;
		u16 v = en ? p->f[en - 1] : (en + n < p->n + n ? p->f[en + n] : 0);

		for (i = 0; i < n; i++)
			p->f[en + i] = v;
	}
	p->n += n;
	p->ancho_maq = -1;
	p->version++;
	return 1;
}

int par_inserta_uno(parrafo *p, u32 en, u32 c, u16 f)
{
	return par_inserta(p, en, &c, &f, 1);
}

void par_quita(parrafo *p, u32 desde, u32 hasta)
{
	if (hasta > p->n)
		hasta = p->n;
	if (desde >= hasta)
		return;
	memmove(p->t + desde, p->t + hasta, (p->n - hasta) * 4);
	memmove(p->f + desde, p->f + hasta, (p->n - hasta) * 2);
	p->n -= hasta - desde;
	p->ancho_maq = -1;
	p->version++;
}

int par_copia(parrafo *d, const parrafo *o)
{
	memset(d, 0, sizeof *d);
	d->p = o->p;
	d->ancho_maq = -1;
	if (o->n) {
		if (!par_cabe(d, o->n)) {
			par_suelta(d);
			return 0;
		}
		memcpy(d->t, o->t, o->n * 4);
		memcpy(d->f, o->f, o->n * 2);
		d->n = o->n;
	}
	return 1;
}

void par_cambia(documento *d, parrafo *p)
{
	p->ancho_maq = -1;
	p->version++;
	d->version++;
	d->cambiado = 1;
}

u16 par_fmt_en(documento *d, const parrafo *p, u32 o)
{
	if (p->n == 0)
		return doc_fmt_estilo(d, p->p.estilo);
	if (o > 0) {
		/* (detrás de una imagen, el formato de la letra de antes de ella) */
		u32 k = o;

		while (k > 0 && es_objeto(p->t[k - 1]))
			k--;
		return p->f[k ? k - 1 : (o - 1)];
	}
	return p->f[0];
}

/* ---- tablas ---- */
int doc_nueva_tabla(documento *d, u16 filas, u16 cols, i32 ancho_total)
{
	tabla *n;
	u32 i;

	if (cols < 1)
		cols = 1;
	if (cols > MAX_COLS)
		cols = MAX_COLS;
	n = crece(d->tab, &d->cabe_tab, d->n_tab + 1, sizeof(tabla));
	if (!n || d->n_tab >= 0xFFF0) {
		d->sin_memoria = 1;
		return -1;
	}
	d->tab = n;
	n = &d->tab[d->n_tab];
	memset(n, 0, sizeof *n);
	n->filas = filas;
	n->cols = cols;
	for (i = 0; i < cols; i++)
		n->ancho[i] = ancho_total / cols;
	n->bordes = BORDE_TODOS;
	n->grosor = 10;
	n->color = 0x000000;
	n->margen = 100;
	n->viva = 1;
	return (int)d->n_tab++;
}

int doc_celda(const documento *d, u32 t, u32 fila, u32 col)
{
	u32 i;

	for (i = 0; i < d->n_par; i++)
		if (d->par[i].p.tabla == t + 1) {
			for (; i < d->n_par && d->par[i].p.tabla == t + 1; i++)
				if (d->par[i].p.fila == fila && d->par[i].p.col == col)
					return (int)i;
			return -1;
		}
	return -1;
}

u32 doc_tabla_ini(const documento *d, u32 p, u32 *n)
{
	u32 t = d->par[p].p.tabla, a = p, b = p;

	if (!t) {
		*n = 0;
		return p;
	}
	while (a > 0 && d->par[a - 1].p.tabla == t)
		a--;
	while (b + 1 < d->n_par && d->par[b + 1].p.tabla == t)
		b++;
	*n = b - a + 1;
	return a;
}

/* ---- imágenes ---- */
int doc_nueva_imagen(documento *d, u8 *datos, u32 n)
{
	imagen *m;
	int an, al, t = imagen_tipo(datos, n);

	if (!t || !imagen_mide(datos, n, &an, &al))
		return -1;
	m = crece(d->img, &d->cabe_img, d->n_img + 1, sizeof(imagen));
	if (!m || d->n_img >= IMAGEN_MAX) {
		d->sin_memoria = 1;
		return -1;
	}
	d->img = m;
	m = &d->img[d->n_img];
	memset(m, 0, sizeof *m);
	m->datos = datos;
	m->n = n;
	m->tipo = (u8)(t == IMAGEN_PNG ? IMG_PNG : t == IMAGEN_JPG ? IMG_JPG : t == IMAGEN_BMP ? IMG_BMP : IMG_GIF);
	m->px_an = an;
	m->px_al = al;
	/* a 96 puntos por pulgada, sin pasar del ancho del texto */
	m->ancho = an * 15;
	m->alto = al * 15;
	if (m->ancho > doc_ancho_texto(d)) {
		m->alto = (i32)((i64)m->alto * doc_ancho_texto(d) / m->ancho);
		m->ancho = doc_ancho_texto(d);
	}
	fmt(m->nombre, sizeof m->nombre, "imagen%u.%s", d->n_img + 1,
	    m->tipo == IMG_PNG ? "png" : m->tipo == IMG_JPG ? "jpg" : m->tipo == IMG_BMP ? "bmp" : "gif");
	return (int)d->n_img++;
}

int doc_imagen_px(documento *d, int i)
{
	imagen *m;
	int an, al;

	if (i < 0 || (u32)i >= d->n_img)
		return 0;
	m = &d->img[i];
	if (m->px)
		return 1;
	m->px = imagen_lee(m->datos, m->n, &an, &al);
	if (!m->px)
		return 0;
	m->px_an = an;
	m->px_al = al;
	return 1;
}

int doc_nueva_nota(documento *d)
{
	parrafo *n = crece(d->notas, &d->cabe_notas, d->n_notas + 1, sizeof(parrafo));

	if (!n) {
		d->sin_memoria = 1;
		return -1;
	}
	d->notas = n;
	memset(&d->notas[d->n_notas], 0, sizeof(parrafo));
	d->notas[d->n_notas].p = d->est[EST_NOTA].p;
	d->notas[d->n_notas].p.estilo = EST_NOTA;
	d->notas[d->n_notas].ancho_maq = -1;
	return (int)d->n_notas++;
}

i32 doc_ancho_texto(const documento *d)
{
	i32 a = d->pag_an - d->marg_izq - d->marg_der;

	return a < 1440 ? 1440 : a;
}

i32 doc_alto_texto(const documento *d)
{
	i32 a = d->pag_al - d->marg_sup - d->marg_inf;

	return a < 1440 ? 1440 : a;
}

void doc_numero_lista(int tipo, u32 n, char *t, int nivel)
{
	static const char *const r1[] = { "", "i", "ii", "iii", "iv", "v", "vi", "vii", "viii", "ix" };
	static const char *const r10[] = { "", "x", "xx", "xxx", "xl", "l", "lx", "lxx", "lxxx", "xc" };

	(void)nivel;
	if (tipo == LISTA_LETRAS) {
		char a[8];
		int k = 0;
		u32 v = n;

		/* a, b, ..., z, aa, ab... */
		while (v > 0 && k < 6) {
			a[k++] = (char)('a' + (v - 1) % 26);
			v = (v - 1) / 26;
		}
		{
			int i;

			for (i = 0; i < k; i++)
				t[i] = a[k - 1 - i];
			t[k] = ')';
			t[k + 1] = 0;
		}
		return;
	}
	if (tipo == LISTA_ROMANOS && n < 400) {
		t[0] = 0;
		while (n >= 100) {
			anade(t, "c", 16);
			n -= 100;
		}
		anade(t, r10[n / 10], 16);
		anade(t, r1[n % 10], 16);
		anade(t, ".", 16);
		return;
	}
	fmt(t, 16, "%u.", n);
}
