/* f_plu.c - Pluma 2: el .PLU. Delante, lo de Pluma de Hispa-386
 * (programas/pluma.asm): «PLUMA1», 26, 0; dd cuántas letras; el texto (437;
 * el 10 acaba el párrafo, el 9 separa las celdas, el 12 es un salto de
 * página); el formato de cada letra (o, en la marca de párrafo, el del
 * párrafo); su tipo y tamaño; y «PL22». Aquel Pluma lee eso y no mira lo de
 * detrás. Detrás, lo de Pluma 2 entero («PLU2»): así nada se pierde al ir y
 * volver, y un .PLU de Hispa-386 se abre como era.
 *
 * Lo de Pluma 2: «PLU2», dd versión (1), dd lo que mide; y registros: db
 * clase, dd lo que mide, y lo suyo (todo en little-endian). Las clases: 1 la
 * página, 2 un formato de letra, 3 un estilo, 4 una tabla, 5 una imagen,
 * 6 un párrafo, 7 el encabezado, 8 el pie, 9 una nota, 10 el resumen.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "archivo.h"
#include "letras.h"

/* los tipos de letra de la familia de Hispa-386 (docs/TIPOS-DE-LETRA.md) */
static const char *const tipos386[20] = {
	"Hispa", "Roble", "Roble negra", "Imprenta", "Carta", "Clásica", "Elegante", "Pergamino", "Losa", "Llano",
	"Llano negra", "Claro", "Pantalla", "Técnica", "Redonda", "Rótulo", "Estrecha", "Máquina", "Fiesta", "Manuscrita"
};
static const u8 tam386[6] = { 24, 18, 36, 48, 72, 96 };	/* medios puntos de los tamaños 0-5 */
static const u32 color386[4] = { SIN_COLOR, 0x8B1A1A, 0x1F3F8F, 0x4F6F1F };	/* negro, granate, añil, oliva */

#define P386_VIN 0x04
#define P386_TABLA 0x08
#define P386_NUM 0x80
#define MAX386 29999

/* ---- escribir ---- */
static void pon_fmt_letra(bufer *b, const fmt_letra *f)
{
	u32 n = largo(f->familia);

	buf_c(b, (u8)n);
	buf_pon(b, f->familia, n);
	buf_le16(b, f->tam);
	buf_c(b, (u8)(f->negrita | f->cursiva << 1 | f->subrayado << 2 | f->tachado << 3 | f->versalitas << 4));
	buf_c(b, f->vert);
	buf_le32(b, f->color);
	buf_le32(b, f->resalte);
}

static void pon_fmt_par(bufer *b, const fmt_parrafo *p)
{
	u32 k;

	buf_c(b, p->alin);
	buf_c(b, p->inter_tipo);
	buf_c(b, p->lista);
	buf_c(b, p->nivel);
	buf_c(b, (u8)(p->salto_antes | p->juntos << 1 | p->cubierta << 2));
	buf_c(b, p->nivel_titulo);
	buf_le32(b, (u32)p->sang_izq);
	buf_le32(b, (u32)p->sang_der);
	buf_le32(b, (u32)p->sang_primera);
	buf_le32(b, (u32)p->antes);
	buf_le32(b, (u32)p->despues);
	buf_le32(b, (u32)p->inter);
	buf_le16(b, p->estilo);
	buf_le16(b, p->lista_id);
	buf_le32(b, p->inicio_lista);
	buf_le16(b, p->tabla);
	buf_le16(b, p->fila);
	buf_le16(b, p->col);
	buf_c(b, p->abarca_cols);
	buf_c(b, p->abarca_filas);
	buf_le32(b, p->fondo);
	buf_c(b, p->n_tabs);
	for (k = 0; k < p->n_tabs && k < MAX_TABS; k++) {
		buf_le32(b, (u32)p->tabs[k].pos);
		buf_c(b, p->tabs[k].tipo);
		buf_c(b, p->tabs[k].relleno);
	}
}

static void pon_parrafo(bufer *b, int clase, const parrafo *p)
{
	bufer r = { 0 };
	u32 k;

	pon_fmt_par(&r, &p->p);
	buf_le32(&r, p->n);
	for (k = 0; k < p->n; k++)
		buf_le32(&r, p->t[k]);
	for (k = 0; k < p->n; k++)
		buf_le16(&r, p->f[k]);
	buf_c(b, (u8)clase);
	buf_le32(b, r.n);
	buf_pon(b, r.d, r.n);
	if (r.mal)
		b->mal = 1;
	buf_suelta(&r);
}

static void registro(bufer *b, int clase, bufer *r)
{
	buf_c(b, (u8)clase);
	buf_le32(b, r->n);
	buf_pon(b, r->d, r->n);
	if (r->mal)
		b->mal = 1;
	buf_suelta(r);
}

static void pon_texto(bufer *b, const char *s)
{
	u32 n = largo(s);

	if (n > 255)
		n = 255;
	buf_c(b, (u8)n);
	buf_pon(b, s, n);
}

/* el tipo de 386 de una familia (lo más parecido) */
static int tipo_de(const char *fam, int negrita)
{
	int i;

	for (i = 0; i < 20; i++)
		if (iguales_sin_caja(fam, tipos386[i]))
			return i;
	if (iguales_sin_caja(fam, FAM_MONO) || busca(fam, "Mono") || busca(fam, "Courier"))
		return 17;
	if (iguales_sin_caja(fam, FAM_SANS) || busca(fam, "Sans") || busca(fam, "Arial") || busca(fam, "Helvetica") || busca(fam, "Calibri"))
		return negrita ? 10 : 9;
	return negrita ? 2 : 1;
}

static int tam_de(u16 t)
{
	/* el de 386 más cerca */
	int i, mejor = 0, dif = 1000;

	for (i = 0; i < 6; i++) {
		int d = t > tam386[i] ? t - tam386[i] : tam386[i] - t;

		if (d < dif) {
			dif = d;
			mejor = i;
		}
	}
	return mejor;
}

static int color_de(u32 c)
{
	u32 r = c >> 16 & 255, g = c >> 8 & 255, b = c & 255;

	if (c == SIN_COLOR || (r < 80 && g < 80 && b < 80))
		return 0;
	if (r > g && r > b)
		return 1;
	if (b > r && b >= g)
		return 2;
	return 3;
}

int plu_escribe(documento *d, bufer *b)
{
	u32 i, k, n = 0, total;
	bufer txt = { 0 }, atr = { 0 }, tip = { 0 }, ext = { 0 }, r = { 0 };

	/* lo de 386: el texto en la 437 */
	for (i = 0; i < d->n_par && n < MAX386; i++) {
		const parrafo *p = &d->par[i];
		u8 pa;

		if (p->p.tabla && p->p.col > 0 && !(i > 0 && d->par[i - 1].p.tabla != p->p.tabla))
			{ /* (las celdas siguientes van en el mismo párrafo) */ }
		if (p->p.salto_antes && i > 0 && n + 2 < MAX386) {
			buf_c(&txt, 12);
			buf_c(&atr, 0);
			buf_c(&tip, 0);
			buf_c(&txt, 10);
			buf_c(&atr, 0);
			buf_c(&tip, 0);
			n += 2;
		}
		for (k = 0; k < p->n && n < MAX386 - 1; k++) {
			u32 c = p->t[k];
			const fmt_letra *f = &d->fl[p->f[k]];
			int tp = tipo_de(f->familia, f->negrita);

			if (es_objeto(c))
				continue;
			if (c == SALTO_LINEA || c == '\t')
				c = ' ';
			buf_c(&txt, a437(c));
			buf_c(&atr, (u8)((f->negrita && tp != 2 && tp != 10 ? 1 : 0) | (f->cursiva ? 2 : 0) | (f->subrayado ? 4 : 0) | color_de(f->color) << 4));
			buf_c(&tip, (u8)(tp | tam_de(f->tam) << 5));
			n++;
		}
		/* una fila de tabla: sus celdas separadas por el 9, en un párrafo */
		if (p->p.tabla && i + 1 < d->n_par && d->par[i + 1].p.tabla == p->p.tabla && d->par[i + 1].p.fila == p->p.fila) {
			buf_c(&txt, 9);
			buf_c(&atr, 0);
			buf_c(&tip, 0);
			n++;
			continue;
		}
		pa = (u8)(p->p.alin & 3);
		if (p->p.lista == LISTA_VINETAS)
			pa |= P386_VIN;
		else if (p->p.lista)
			pa |= P386_NUM;
		if (p->p.tabla)
			pa |= P386_TABLA;
		{
			i32 s = p->p.sang_izq / 709;	/* (un paso de 386: cuatro letras, 1,25 cm) */

			if (p->p.lista && s > 0)
				s--;
			if (s > 7)
				s = 7;
			if (s < 0)
				s = 0;
			pa |= (u8)(s << 4);
		}
		buf_c(&txt, 10);
		buf_c(&atr, pa);
		buf_c(&tip, 0);
		n++;
	}
	/* (el último octeto es siempre un 10) */
	if (!n || txt.d[txt.n - 1] != 10) {
		buf_c(&txt, 10);
		buf_c(&atr, 0);
		buf_c(&tip, 0);
		n++;
	}
	buf_pon(b, "PLUMA1\x1a\0", 8);
	buf_le32(b, n);
	buf_pon(b, txt.d, txt.n);
	buf_pon(b, atr.d, atr.n);
	buf_pon(b, tip.d, tip.n);
	buf_txt(b, "PL22");
	if (txt.mal || atr.mal || tip.mal)
		b->mal = 1;
	buf_suelta(&txt);
	buf_suelta(&atr);
	buf_suelta(&tip);

	/* lo de Pluma 2 */
	buf_le32(&r, (u32)d->pag_an);
	buf_le32(&r, (u32)d->pag_al);
	buf_le32(&r, (u32)d->marg_izq);
	buf_le32(&r, (u32)d->marg_der);
	buf_le32(&r, (u32)d->marg_sup);
	buf_le32(&r, (u32)d->marg_inf);
	buf_le32(&r, (u32)d->dist_enc);
	buf_le32(&r, (u32)d->dist_pie);
	buf_c(&r, (u8)(d->hay_enc | d->hay_pie << 1 | d->enc_no_primera << 2));
	buf_le32(&r, d->num_inicio);
	registro(&ext, 1, &r);
	for (i = 0; i < d->n_fl; i++) {
		pon_fmt_letra(&r, &d->fl[i]);
		registro(&ext, 2, &r);
	}
	for (i = 0; i < d->n_est; i++) {
		const estilo *e = &d->est[i];

		pon_texto(&r, e->nombre);
		pon_fmt_letra(&r, &e->letra);
		pon_fmt_par(&r, &e->p);
		buf_le16(&r, e->siguiente);
		buf_c(&r, e->de_serie);
		registro(&ext, 3, &r);
	}
	for (i = 0; i < d->n_tab; i++) {
		const tabla *t = &d->tab[i];

		buf_le16(&r, t->filas);
		buf_le16(&r, t->cols);
		for (k = 0; k < t->cols && k < MAX_COLS; k++)
			buf_le32(&r, (u32)t->ancho[k]);
		buf_c(&r, t->bordes);
		buf_c(&r, t->cabecera);
		buf_le16(&r, t->grosor);
		buf_le32(&r, t->color);
		buf_le32(&r, (u32)t->margen);
		buf_c(&r, t->alin);
		buf_c(&r, t->viva);
		registro(&ext, 4, &r);
	}
	for (i = 0; i < d->n_img; i++) {
		const imagen *m = &d->img[i];

		buf_c(&r, m->tipo);
		buf_c(&r, m->ajuste);
		buf_le32(&r, (u32)m->ancho);
		buf_le32(&r, (u32)m->alto);
		pon_texto(&r, m->nombre);
		buf_le32(&r, m->n);
		buf_pon(&r, m->datos, m->n);
		registro(&ext, 5, &r);
	}
	for (i = 0; i < d->n_par; i++)
		pon_parrafo(&ext, 6, &d->par[i]);
	pon_parrafo(&ext, 7, &d->enc);
	pon_parrafo(&ext, 8, &d->pie);
	for (i = 0; i < d->n_notas; i++)
		pon_parrafo(&ext, 9, &d->notas[i]);
	pon_texto(&r, d->titulo);
	pon_texto(&r, d->autor);
	pon_texto(&r, d->asunto);
	registro(&ext, 10, &r);
	total = ext.n;
	buf_txt(b, "PLU2");
	buf_le32(b, 1);
	buf_le32(b, total);
	buf_pon(b, ext.d, ext.n);
	if (ext.mal)
		b->mal = 1;
	buf_suelta(&ext);
	return b->mal ? -1 : 0;
}

/* ---- leer (sin fiarse) ---- */
typedef struct {
	const u8 *d;
	u32 n, i;
	int mal;
} lector;

static u32 l8(lector *l)
{
	if (l->i + 1 > l->n) {
		l->mal = 1;
		return 0;
	}
	return l->d[l->i++];
}

static u32 l16(lector *l)
{
	u32 v;

	if (l->i + 2 > l->n) {
		l->mal = 1;
		return 0;
	}
	v = l->d[l->i] | (u32)l->d[l->i + 1] << 8;
	l->i += 2;
	return v;
}

static u32 l32(lector *l)
{
	u32 v;

	if (l->i + 4 > l->n || l->i + 4 < l->i) {
		l->mal = 1;
		return 0;
	}
	v = l->d[l->i] | (u32)l->d[l->i + 1] << 8 | (u32)l->d[l->i + 2] << 16 | (u32)l->d[l->i + 3] << 24;
	l->i += 4;
	return v;
}

static void ltexto(lector *l, char *d, u32 cabe)
{
	u32 n = l8(l), k;

	if (l->i + n > l->n) {
		l->mal = 1;
		d[0] = 0;
		return;
	}
	for (k = 0; k < n && k + 1 < cabe; k++)
		d[k] = (char)l->d[l->i + k];
	d[k] = 0;
	l->i += n;
}

static i32 limita(i32 v, i32 a, i32 b)
{
	return v < a ? a : v > b ? b : v;
}

static void lee_fmt_letra(lector *l, fmt_letra *f)
{
	u32 b;

	memset(f, 0, sizeof *f);
	ltexto(l, f->familia, sizeof f->familia);
	if (!f->familia[0])
		copia(f->familia, FAM_SERIF, sizeof f->familia);
	f->tam = (u16)limita((i32)l16(l), 2, 3200);
	b = l8(l);
	f->negrita = b & 1;
	f->cursiva = b >> 1 & 1;
	f->subrayado = b >> 2 & 1;
	f->tachado = b >> 3 & 1;
	f->versalitas = b >> 4 & 1;
	f->vert = (u8)(l8(l) % 3);
	f->color = l32(l);
	f->resalte = l32(l);
	if (f->color != SIN_COLOR)
		f->color &= 0xFFFFFF;
	if (f->resalte != SIN_COLOR)
		f->resalte &= 0xFFFFFF;
}

static void lee_fmt_par(lector *l, fmt_parrafo *p)
{
	u32 k, b;
	const i32 M = 31680;	/* (55,9 cm: más que cualquier papel) */

	memset(p, 0, sizeof *p);
	p->alin = (u8)(l8(l) & 3);
	p->inter_tipo = (u8)(l8(l) % 3);
	p->lista = (u8)(l8(l) % 5);
	p->nivel = (u8)(l8(l) % 9);
	b = l8(l);
	p->salto_antes = b & 1;
	p->juntos = b >> 1 & 1;
	p->cubierta = b >> 2 & 1;
	p->nivel_titulo = (u8)(l8(l) % 10);
	p->sang_izq = limita((i32)l32(l), -M, M);
	p->sang_der = limita((i32)l32(l), -M, M);
	p->sang_primera = limita((i32)l32(l), -M, M);
	p->antes = limita((i32)l32(l), 0, M);
	p->despues = limita((i32)l32(l), 0, M);
	p->inter = limita((i32)l32(l), 0, M);
	p->estilo = (u16)l16(l);
	p->lista_id = (u16)l16(l);
	p->inicio_lista = l32(l) % 100000;
	p->tabla = (u16)l16(l);
	p->fila = (u16)l16(l);
	p->col = (u16)l16(l);
	p->abarca_cols = (u8)l8(l);
	p->abarca_filas = (u8)l8(l);
	p->fondo = l32(l);
	if (p->fondo != SIN_COLOR)
		p->fondo &= 0xFFFFFF;
	p->n_tabs = (u8)l8(l);
	if (p->n_tabs > MAX_TABS)
		l->mal = 1;
	for (k = 0; k < p->n_tabs && !l->mal; k++) {
		p->tabs[k].pos = limita((i32)l32(l), 0, M);
		p->tabs[k].tipo = (u8)(l8(l) & 3);
		p->tabs[k].relleno = (u8)l8(l);
	}
	if (l->mal)
		p->n_tabs = 0;
}

static int lee_parrafo(lector *l, documento *d, parrafo *p)
{
	u32 n, k;

	memset(p, 0, sizeof *p);
	p->ancho_maq = -1;
	lee_fmt_par(l, &p->p);
	n = l32(l);
	if (l->mal || n > (l->n - l->i) / 6)
		return 0;
	if (n && !par_cabe(p, n)) {
		d->sin_memoria = 1;
		return 0;
	}
	for (k = 0; k < n; k++) {
		u32 c = l32(l);

		/* (lo que no es Unicode ni de los nuestros, fuera) */
		if ((c >= 0xD800 && c < 0xE000) || (c > 0x10FFFF && !es_imagen(c) && c != CAMPO_PAGINA && c != CAMPO_PAGINAS && !es_nota(c)) ||
		    (c < 0x20 && c != '\t'))
			c = 0xFFFD;
		p->t[k] = c;
	}
	for (k = 0; k < n; k++)
		p->f[k] = (u16)l16(l);
	p->n = n;
	return !l->mal;
}

/* lo de Pluma 2, detrás; 1 si estaba y se ha leído bien */
static int lee_ext(documento *d, const u8 *datos, u32 n)
{
	lector l = { datos, n, 0, 0 };
	u32 i;
	int hay_par = 0;

	if (l32(&l) != 1)
		return 0;
	l.n = l32(&l);
	if (l.n > n - 8)
		return 0;
	l.d = datos + 8;
	l.i = 0;
	d->n_fl = 0;
	d->n_est = 0;
	doc_quita_parrafos(d, 0, d->n_par);
	while (l.i < l.n && !l.mal && !d->sin_memoria) {
		u32 clase = l8(&l), largo_ = l32(&l);
		lector r;

		if (l.mal || largo_ > l.n - l.i)
			return 0;
		r.d = l.d + l.i;
		r.n = largo_;
		r.i = 0;
		r.mal = 0;
		l.i += largo_;
		switch (clase) {
		case 1:
			d->pag_an = limita((i32)l32(&r), 2880, 31680);
			d->pag_al = limita((i32)l32(&r), 2880, 31680);
			d->marg_izq = limita((i32)l32(&r), 0, 8000);
			d->marg_der = limita((i32)l32(&r), 0, 8000);
			d->marg_sup = limita((i32)l32(&r), 0, 8000);
			d->marg_inf = limita((i32)l32(&r), 0, 8000);
			d->dist_enc = limita((i32)l32(&r), 0, 8000);
			d->dist_pie = limita((i32)l32(&r), 0, 8000);
			i = l8(&r);
			d->hay_enc = i & 1;
			d->hay_pie = i >> 1 & 1;
			d->enc_no_primera = i >> 2 & 1;
			d->num_inicio = l32(&r) % 100000;
			break;
		case 2: {
			fmt_letra f;

			lee_fmt_letra(&r, &f);
			if (r.mal)
				return 0;
			/* (sin juntar los iguales: los números de los párrafos son estos) */
			if (d->n_fl >= d->cabe_fl) {
				u32 c = d->cabe_fl ? d->cabe_fl * 2 : 64;
				fmt_letra *nf = so_repide(d->fl, c * sizeof(fmt_letra));

				if (!nf || d->n_fl >= 0xFFF0)
					return 0;
				d->fl = nf;
				d->cabe_fl = c;
			}
			d->fl[d->n_fl++] = f;
			break;
		}
		case 3: {
			estilo e;

			memset(&e, 0, sizeof e);
			ltexto(&r, e.nombre, sizeof e.nombre);
			lee_fmt_letra(&r, &e.letra);
			lee_fmt_par(&r, &e.p);
			e.siguiente = (u16)l16(&r);
			e.de_serie = (u8)(l8(&r) & 1);
			if (r.mal)
				return 0;
			e.p.tabla = 0;
			doc_nuevo_estilo(d, &e);
			break;
		}
		case 4: {
			int t = doc_nueva_tabla(d, 1, 1, 1440);
			tabla *tb;
			u32 k, cols;

			if (t < 0)
				return 0;
			tb = &d->tab[t];
			tb->filas = (u16)l16(&r);
			cols = l16(&r);
			if (cols < 1 || cols > MAX_COLS)
				return 0;
			tb->cols = (u16)cols;
			for (k = 0; k < cols; k++)
				tb->ancho[k] = limita((i32)l32(&r), 100, 31680);
			tb->bordes = (u8)(l8(&r) % 4);
			tb->cabecera = (u8)(l8(&r) & 1);
			tb->grosor = (u16)limita((i32)l16(&r), 0, 200);
			tb->color = l32(&r) & 0xFFFFFF;
			tb->margen = limita((i32)l32(&r), 0, 1440);
			tb->alin = (u8)(l8(&r) % 3);
			tb->viva = (u8)(l8(&r) & 1);
			if (r.mal)
				return 0;
			break;
		}
		case 5: {
			u8 tipo = (u8)l8(&r), aj = (u8)(l8(&r) % 3);
			i32 an = limita((i32)l32(&r), 20, 31680), al = limita((i32)l32(&r), 20, 31680);
			char nom[40];
			u32 m;
			u8 *datos2;
			int k;

			(void)tipo;
			ltexto(&r, nom, sizeof nom);
			m = l32(&r);
			if (r.mal || m > r.n - r.i)
				return 0;
			datos2 = so_pide(m + 1);
			if (!datos2)
				return 0;
			memcpy(datos2, r.d + r.i, m);
			k = doc_nueva_imagen(d, datos2, m);
			if (k < 0) {
				/* (una imagen que no se sabe leer: un hueco, para que los números sigan) */
				static const u8 gif1[] = "GIF89a\x01\x00\x01\x00\x80\x00\x00\xff\xff\xff\x00\x00\x00!\xf9\x04\x01\x00\x00\x00\x00,\x00\x00\x00\x00\x01\x00\x01\x00\x00\x02\x02D\x01\x00;";

				so_suelta(datos2);
				datos2 = so_pide(sizeof gif1);
				if (!datos2)
					return 0;
				memcpy(datos2, gif1, sizeof gif1);
				k = doc_nueva_imagen(d, datos2, sizeof gif1);
				if (k < 0) {
					so_suelta(datos2);
					return 0;
				}
			}
			d->img[k].ajuste = aj;
			d->img[k].ancho = an;
			d->img[k].alto = al;
			if (nom[0])
				copia(d->img[k].nombre, nom, sizeof d->img[k].nombre);
			break;
		}
		case 6: {
			parrafo *p = arch_parrafo(d);

			if (!p || !lee_parrafo(&r, d, p))
				return 0;
			hay_par = 1;
			break;
		}
		case 7:
			par_suelta(&d->enc);
			if (!lee_parrafo(&r, d, &d->enc))
				return 0;
			break;
		case 8:
			par_suelta(&d->pie);
			if (!lee_parrafo(&r, d, &d->pie))
				return 0;
			break;
		case 9: {
			int k = doc_nueva_nota(d);

			if (k < 0 || !lee_parrafo(&r, d, &d->notas[k]))
				return 0;
			break;
		}
		case 10:
			ltexto(&r, d->titulo, sizeof d->titulo);
			ltexto(&r, d->autor, sizeof d->autor);
			ltexto(&r, d->asunto, sizeof d->asunto);
			break;
		default:
			break;	/* (de un Pluma más nuevo: se salta) */
		}
	}
	if (l.mal || !hay_par || !d->n_fl || !d->n_est)
		return 0;
	/* que todo apunte a lo que hay */
	for (i = 0; i < d->n_par; i++) {
		parrafo *p = &d->par[i];
		u32 k;

		for (k = 0; k < p->n; k++) {
			if (p->f[k] >= d->n_fl)
				p->f[k] = 0;
			if (es_imagen(p->t[k]) && p->t[k] - IMAGEN_0 >= d->n_img)
				p->t[k] = 0xFFFD;
			if (es_nota(p->t[k]) && p->t[k] - NOTA_0 >= d->n_notas)
				p->t[k] = 0xFFFD;
		}
		if (p->p.estilo >= d->n_est)
			p->p.estilo = 0;
		if (p->p.tabla > d->n_tab || (p->p.tabla && (p->p.col >= d->tab[p->p.tabla - 1].cols)))
			p->p.tabla = 0;
	}
	{
		parrafo *ps[2] = { &d->enc, &d->pie };
		int j;

		for (j = 0; j < 2; j++) {
			u32 k;

			for (k = 0; k < ps[j]->n; k++) {
				if (ps[j]->f[k] >= d->n_fl)
					ps[j]->f[k] = 0;
				if (es_objeto(ps[j]->t[k]) && ps[j]->t[k] != CAMPO_PAGINA && ps[j]->t[k] != CAMPO_PAGINAS)
					ps[j]->t[k] = 0xFFFD;
			}
			ps[j]->p.tabla = 0;
			if (ps[j]->p.estilo >= d->n_est)
				ps[j]->p.estilo = 0;
		}
		for (i = 0; i < d->n_notas; i++) {
			u32 k;

			for (k = 0; k < d->notas[i].n; k++) {
				if (d->notas[i].f[k] >= d->n_fl)
					d->notas[i].f[k] = 0;
				if (es_objeto(d->notas[i].t[k]))
					d->notas[i].t[k] = 0xFFFD;
			}
			d->notas[i].p.tabla = 0;
		}
	}
	for (i = 0; i < d->n_est; i++)
		if (d->est[i].siguiente >= d->n_est)
			d->est[i].siguiente = (u16)i;
	return 1;
}

/* arregla las tablas que no casan (de un archivo estropeado): cada tabla,
 * sus filas completas y en orden */
static void repasa_tablas(documento *d)
{
	u32 i = 0;

	while (i < d->n_par) {
		u32 t = d->par[i].p.tabla, n, k, cols, bien = 1;

		if (!t) {
			i++;
			continue;
		}
		for (n = 0; i + n < d->n_par && d->par[i + n].p.tabla == t; n++)
			;
		cols = d->tab[t - 1].cols;
		if (n % cols)
			bien = 0;
		for (k = 0; k < n && bien; k++)
			if (d->par[i + k].p.fila != k / cols || d->par[i + k].p.col != k % cols)
				bien = 0;
		/* (la misma tabla dos veces, separada: la segunda no vale) */
		for (k = i + n; k < d->n_par && bien; k++)
			if (d->par[k].p.tabla == t)
				bien = 0;
		if (!bien)
			for (k = 0; k < n; k++)
				d->par[i + k].p.tabla = 0;
		else {
			d->tab[t - 1].filas = (u16)(n / cols);
			for (k = 0; k < n; k++) {
				parrafo *p = &d->par[i + k];

				if (p->p.col + (p->p.abarca_cols ? p->p.abarca_cols : 1) > cols)
					p->p.abarca_cols = (u8)(cols - p->p.col);
				if (p->p.fila + (p->p.abarca_filas ? p->p.abarca_filas : 1) > n / cols)
					p->p.abarca_filas = (u8)(n / cols - p->p.fila);
			}
		}
		i += n;
	}
	for (i = 0; i < d->n_par; i++)
		if (!d->par[i].p.tabla) {
			d->par[i].p.cubierta = 0;
			d->par[i].p.abarca_cols = d->par[i].p.abarca_filas = 0;
		}
}

documento *plu_lee(const u8 *datos, u32 n, char *error, u32 cabe)
{
	documento *d;
	u32 letras, i, cols_fila = 0;
	const u8 *txt, *atr, *tip = NULL;
	int nuevo = 0;
	u16 fmts[256];
	u32 hay_fmt[256];

	if (n < 12 || memcmp(datos, "PLUMA1\x1a", 7)) {
		copia(error, "No es un documento de Pluma.", cabe);
		return NULL;
	}
	letras = datos[8] | (u32)datos[9] << 8 | (u32)datos[10] << 16 | (u32)datos[11] << 24;
	if (letras > (n - 12) / 2) {
		copia(error, "El documento de Pluma está cortado.", cabe);
		return NULL;
	}
	txt = datos + 12;
	atr = txt + letras;
	if (12 + 3 * (u64)letras <= n) {
		tip = atr + letras;
		if (12 + 3 * (u64)letras + 4 <= n && !memcmp(tip + letras, "PL22", 4))
			nuevo = 1;
	}
	d = doc_nuevo();
	if (!d)
		return NULL;
	/* ¿lleva lo de Pluma 2 detrás? */
	if (nuevo && 12 + 3 * (u64)letras + 4 + 12 <= n && !memcmp(tip + letras + 4, "PLU2", 4)) {
		u32 o = 12 + 3 * letras + 8;

		if (lee_ext(d, datos + o, n - o) && !d->sin_memoria) {
			repasa_tablas(d);
			if (d->n_par)
				return d;
		}
		/* (estropeado: lo de 386) */
		doc_suelta(d);
		d = doc_nuevo();
		if (!d)
			return NULL;
	}
	/* lo de Pluma de Hispa-386 */
	memset(hay_fmt, 0, sizeof hay_fmt);
	doc_quita_parrafos(d, 0, d->n_par);
	arch_parrafo(d);
	for (i = 0; i < letras && !d->sin_memoria; i++) {
		u8 c = txt[i], a = atr[i], t = tip ? tip[i] : 0;

		if (c == 10) {
			parrafo *p = &d->par[d->n_par - 1];

			p->p.alin = a & 3;
			if (a & P386_VIN)
				p->p.lista = LISTA_VINETAS;
			if (a & P386_NUM)
				p->p.lista = LISTA_NUMEROS;
			if (p->p.lista) {
				p->p.lista_id = 1;
				p->p.sang_izq = 720 + 709 * ((a >> 4) & 7);
				p->p.sang_primera = -360;
			} else
				p->p.sang_izq = 709 * ((a >> 4) & 7);
			if (a & P386_TABLA) {
				/* una fila: sus celdas, separadas por el 9 (ya partidas) */
				u32 k, cols = cols_fila + 1;
				int tb;

				/* ¿sigue la tabla de antes (mismas columnas)? */
				k = d->n_par - cols;
				if (k > 0 && d->par[k - 1].p.tabla && d->tab[d->par[k - 1].p.tabla - 1].cols == cols)
					tb = d->par[k - 1].p.tabla - 1;
				else
					tb = doc_nueva_tabla(d, 0, (u16)cols, doc_ancho_texto(d));
				if (tb >= 0) {
					tabla *T = &d->tab[tb];

					for (k = 0; k < cols; k++) {
						parrafo *q = &d->par[d->n_par - cols + k];

						q->p.tabla = (u16)(tb + 1);
						q->p.fila = T->filas;
						q->p.col = (u16)k;
						q->p.alin = p->p.alin;
						q->p.lista = LISTA_NO;
						q->p.sang_izq = 0;
						q->p.antes = q->p.despues = 0;
					}
					T->filas++;
				}
			}
			cols_fila = 0;
			if (i + 1 < letras)
				arch_parrafo(d);
			continue;
		}
		if (c == 9) {
			/* otra celda de la fila */
			cols_fila++;
			arch_parrafo(d);
			continue;
		}
		if (c == 12) {
			/* el salto de página va solo en su párrafo: el de después empieza en otra */
			if (i + 1 < letras && txt[i + 1] == 10) {
				i++;
				if (i + 1 < letras) {
					parrafo *p = arch_parrafo(d);

					if (p)
						p->p.salto_antes = 1;
				}
			}
			continue;
		}
		{
			u32 u = de437(c);
			u8 clave = (u8)(a & 0x37);
			u32 k = (u32)(t ^ clave * 37) & 255;

			if (c < 32)
				u = ' ';
			/* el formato: en una tabla pequeña de los ya vistos (tipo, tamaño y atributos) */
			int vueltas = 0;

			while (hay_fmt[k] && (hay_fmt[k] >> 16) != ((u32)t << 8 | clave) && ++vueltas < 256)
				k = (k + 1) & 255;
			if (vueltas >= 256) {
				/* (la tabla llena: se olvida) */
				memset(hay_fmt, 0, sizeof hay_fmt);
			}
			if (!hay_fmt[k]) {
				fmt_letra f = fmt_letra_normal();
				int tp = (t & 0x1F) < 20 ? (t & 0x1F) : 0, tm = (t >> 5) < 6 ? (t >> 5) : 0;

				if (!tip || !nuevo) {
					/* (los de antes: el bit 3 era «grande») */
					tp = 1;
					tm = (a & 8) ? 3 : 0;
				}
				copia(f.familia, tipos386[tp], sizeof f.familia);
				if (tp == 0)
					copia(f.familia, FAM_MONO, sizeof f.familia);
				f.tam = tam386[tm];
				f.negrita = (a & 1) || tp == 2 || tp == 10;
				f.cursiva = (a >> 1) & 1;
				f.subrayado = (a >> 2) & 1;
				f.color = color386[(a >> 4) & 3];
				fmts[k] = doc_fmt(d, &f);
				hay_fmt[k] = (((u32)t << 8 | clave) << 16) | 1;
			}
			arch_letras(d, &u, 1, fmts[k]);
		}
	}
	if (d->sin_memoria) {
		doc_suelta(d);
		copia(error, "No hay memoria para abrirlo.", cabe);
		return NULL;
	}
	if (!d->n_par)
		arch_parrafo(d);
	repasa_tablas(d);
	return d;
}
