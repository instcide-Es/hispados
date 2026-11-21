/* documento.c - Hispa-PDF: abrir un PDF. La tabla de objetos (la «xref» de
 * siempre, la que va en un flujo desde PDF 1.5 y las dos a la vez en los
 * híbridos), los objetos que van dentro de flujos de objetos, las
 * actualizaciones (/Prev), el cifrado y, si algo de eso está mal, rehacer la
 * tabla buscando los objetos por todo el archivo. Y el árbol de las páginas,
 * con lo que heredan.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "hpdf.h"

void ar_suelta(pdoc *d);
void fuentes_suelta(pdoc *d);
static void fuentes_suelta_doc(pdoc *d) { fuentes_suelta(d); }
static pobj *carga(pdoc *d, u32 num, int hondo);
static void registra_objstm(pdoc *d);
static void busca_trailer(pdoc *d, u32 ultimo_trailer);

static pdoc *doc_nuevo(void)
{
	pdoc *d = pide(sizeof(pdoc));

	memset(d, 0, sizeof *d);
	d->nulo = o_nuevo(d, O_NULO);
	return d;
}

pdoc *pdf_vacio(void)
{
	return doc_nuevo();
}

void pdf_cierra(pdoc *d)
{
	if (!d)
		return;
	fuentes_suelta_doc(d);
	ar_suelta(d);
	so_suelta(d->e);
	so_suelta(d->paginas);
	so_suelta(d->num_pagina);
	so_suelta(d->datos);
	so_suelta(d);
}

/* la tabla, con sitio para el objeto num */
static int sitio(pdoc *d, u32 num)
{
	u32 c;

	if (num >= MAX_OBJETOS)
		return 0;
	if (num < d->n)
		return 1;
	c = d->n ? d->n : 64;
	while (c <= num)
		c *= 2;
	if (c > MAX_OBJETOS)
		c = MAX_OBJETOS;
	d->e = repide(d->e, c * sizeof(pentrada));
	memset(d->e + d->n, 0, (c - d->n) * sizeof(pentrada));
	d->n = c;
	return 1;
}

/* apunta una entrada (si no la hay ya: las tablas van de la más nueva a la más vieja) */
static void apunta(pdoc *d, u32 num, int tipo, u32 pos, u32 gen, u32 indice, int pisa)
{
	pentrada *e;

	if (!sitio(d, num))
		return;
	e = &d->e[num];
	if (e->tipo != 0 && !pisa)
		return;
	if (e->tipo == 0 && e->gen == 0xFFFF && !pisa)
		return;		/* (borrado en una tabla más nueva) */
	e->tipo = (u8)tipo;
	e->pos = pos;
	e->gen = (u16)gen;
	e->indice = indice;
}

/* «startxref»: la posición de la última tabla */
static int busca_startxref(pdoc *d, u32 *pos)
{
	u32 i, desde = d->largo > 4096 ? d->largo - 4096 : 0;
	plex l;
	ptok t;

	if (d->largo < 9)
		return 0;
	for (i = d->largo - 9; i + 1 > desde; i--) {
		if (memcmp(d->datos + i, "startxref", 9) == 0) {
			l.ini = d->datos;
			l.p = d->datos + i + 9;
			l.fin = d->datos + d->largo;
			if (lex_token(&l, &t) == T_ENT && t.i > 0 && (u32)t.i < d->largo) {
				*pos = (u32)t.i;
				return 1;
			}
			return 0;
		}
		if (i == 0)
			break;
	}
	return 0;
}

/* la tabla de siempre, en pos («xref» ya visto): sus entradas; su trailer, a *tr */
static int xref_clasica(pdoc *d, plex *l, pobj **tr)
{
	ptok t, t2;

	for (;;) {
		const u8 *antes = l->p;
		u32 ini, cuantos, k;

		if (lex_token(l, &t) != T_ENT) {
			l->p = antes;
			break;
		}
		if (lex_token(l, &t2) != T_ENT || t.i < 0 || t2.i < 0)
			return 0;
		ini = (u32)t.i;
		cuantos = (u32)t2.i;
		if (cuantos > MAX_OBJETOS || ini > MAX_OBJETOS)
			return 0;
		for (k = 0; k < cuantos; k++) {
			ptok a, b, c;

			if (lex_token(l, &a) != T_ENT || lex_token(l, &b) != T_ENT || lex_token(l, &c) != T_PALABRA)
				return 0;
			if (ini + k >= MAX_OBJETOS)
				continue;
			if (c.n == 1 && c.p[0] == 'n') {
				if (a.i > 0 && (u32)a.i < d->largo)
					apunta(d, ini + k, 1, (u32)a.i, (u32)b.i, 0, 0);
			} else if (c.n == 1 && c.p[0] == 'f') {
				/* libre: que las tablas más viejas no lo vuelvan a poner */
				if (sitio(d, ini + k) && d->e[ini + k].tipo == 0)
					d->e[ini + k].gen = 0xFFFF;
			} else
				return 0;
		}
	}
	if (lex_token(l, &t) != T_PALABRA || !tok_es(&t, "trailer"))
		return 0;
	*tr = lee_objeto(d, l, 0);
	return *tr && (*tr)->t == O_DICC;
}

/* un número de «w» octetos, el alto primero */
static u32 campo(const u8 *p, int w)
{
	u32 v = 0;
	int i;

	for (i = 0; i < w; i++)
		v = v << 8 | p[i];
	return v;
}

static pobj *lee_indirecto(pdoc *d, u32 pos, u32 *num, u32 *gen, int hondo);

/* la tabla en un flujo (/Type /XRef) */
static int xref_flujo(pdoc *d, pobj *f)
{
	pobj *dic = f->u.f.dic, *w = dic_r(d, dic, "W"), *ind = dic_r(d, dic, "Index");
	int ws[3], i, paso;
	u32 n, k, tam, sec, q;
	u8 *datos;
	const u8 *p;

	if (!w || w->t != O_ARREGLO || w->n < 3)
		return 0;
	for (i = 0; i < 3; i++) {
		ws[i] = (int)o_num(resuelve(d, w->u.v[i]), -1);
		if (ws[i] < 0 || ws[i] > 4)
			return 0;
	}
	paso = ws[0] + ws[1] + ws[2];
	if (paso == 0)
		return 0;
	tam = (u32)o_num(dic_r(d, dic, "Size"), 0);
	datos = flujo_datos(d, f, &n, 0, NULL);
	if (!datos)
		return 0;
	p = datos;
	q = 0;
	for (sec = 0;; sec++) {
		u32 ini, cuantos;

		if (ind && ind->t == O_ARREGLO) {
			if (2 * sec + 1 >= ind->n)
				break;
			ini = (u32)o_num(resuelve(d, ind->u.v[2 * sec]), 0);
			cuantos = (u32)o_num(resuelve(d, ind->u.v[2 * sec + 1]), 0);
		} else {
			if (sec > 0)
				break;
			ini = 0;
			cuantos = tam;
		}
		if (ini > MAX_OBJETOS || cuantos > MAX_OBJETOS)
			break;
		for (k = 0; k < cuantos && (q + 1) * (u32)paso <= n; k++, q++) {
			const u8 *e = p + q * paso;
			u32 tipo = ws[0] ? campo(e, ws[0]) : 1, b = campo(e + ws[0], ws[1]), c = campo(e + ws[0] + ws[1], ws[2]);

			if (ini + k >= MAX_OBJETOS)
				continue;
			if (tipo == 1) {
				if (b < d->largo)
					apunta(d, ini + k, 1, b, c, 0, 0);
			} else if (tipo == 2) {
				apunta(d, ini + k, 2, b, 0, c, 0);
			} else if (tipo == 0) {
				if (sitio(d, ini + k) && d->e[ini + k].tipo == 0)
					d->e[ini + k].gen = 0xFFFF;
			}
		}
	}
	so_suelta(datos);
	return 1;
}

/* todas las tablas, desde la última (con sus /Prev y /XRefStm) */
static int lee_tablas(pdoc *d)
{
	u32 pos, vistas[MAX_XREF];
	int nv = 0, i;
	pobj *primero = NULL;

	if (!busca_startxref(d, &pos))
		return 0;
	while (nv < MAX_XREF) {
		plex l;
		ptok t;
		pobj *tr = NULL;

		for (i = 0; i < nv; i++)
			if (vistas[i] == pos)
				goto acaba;
		vistas[nv++] = pos;
		if (pos >= d->largo)
			return primero != NULL;
		l.ini = d->datos;
		l.p = d->datos + pos;
		l.fin = d->datos + d->largo;
		/* (algunos apuntan unos octetos antes o después: se mira cerca) */
		lex_blancos(&l);
		if (lex_token(&l, &t) == T_PALABRA && tok_es(&t, "xref")) {
			if (!xref_clasica(d, &l, &tr))
				return 0;
			/* el híbrido: su tabla en flujo, que va antes que la vieja */
			{
				pobj *x = o_dic_clave(tr, "XRefStm");
				u32 num, gen;

				if (o_es_num(x) && o_num(x, 0) > 0 && o_num(x, 0) < d->largo) {
					pobj *f = lee_indirecto(d, (u32)o_num(x, 0), &num, &gen, 0);

					if (f && f->t == O_FLUJO)
						xref_flujo(d, f);
				}
			}
		} else {
			u32 num, gen;
			pobj *f = lee_indirecto(d, pos, &num, &gen, 0);

			if (!f || f->t != O_FLUJO || !o_es_nombre(o_dic_clave(f, "Type"), "XRef"))
				return 0;
			if (!xref_flujo(d, f))
				return 0;
			tr = f->u.f.dic;
		}
		if (!primero)
			primero = tr;
		{
			pobj *pv = o_dic_clave(tr, "Prev");

			if (!o_es_num(pv))
				break;
			pos = (u32)o_num(pv, 0);
		}
	}
acaba:
	d->trailer = primero;
	return primero != NULL;
}

/* rehacer la tabla: cada «N G obj» del archivo (el último de cada número
 * gana), los flujos de objetos que haya y el trailer (el último; o el de
 * una tabla en flujo; o se busca el catálogo) */
static void repara(pdoc *d)
{
	u32 i, ultimo_trailer = 0;
	u8 *p = d->datos;

	d->reparado = 1;
	d->intentado_reparar = 1;
	for (i = 0; i < d->n; i++) {
		/* se queda lo ya leído; lo que no, se olvida */
		if (!d->e[i].obj)
			memset(&d->e[i], 0, sizeof(pentrada));
	}
	for (i = 0; i + 3 <= d->largo; i++) {
		if (p[i] == 'o' && p[i + 1] == 'b' && p[i + 2] == 'j' && (i + 3 == d->largo || es_blanco(p[i + 3]) || es_delim(p[i + 3]))) {
			/* hacia atrás: blancos, la generación, blancos, el número */
			u32 k = i, fg, fn;
			u32 num = 0, gen = 0, m;

			while (k > 0 && es_blanco(p[k - 1]))
				k--;
			fg = k;
			while (k > 0 && p[k - 1] >= '0' && p[k - 1] <= '9' && fg - k < 6)
				k--;
			if (k == fg)
				continue;
			for (m = k; m < fg; m++)
				gen = gen * 10 + (p[m] - '0');
			while (k > 0 && es_blanco(p[k - 1]))
				k--;
			fn = k;
			while (k > 0 && p[k - 1] >= '0' && p[k - 1] <= '9' && fn - k < 8)
				k--;
			if (k == fn || fn == fg)
				continue;
			for (m = k; m < fn; m++)
				num = num * 10 + (p[m] - '0');
			if (num == 0 || num >= MAX_OBJETOS)
				continue;
			if (sitio(d, num) && !d->e[num].obj)
				apunta(d, num, 1, k, gen, 0, 1);
		} else if (p[i] == 't' && i + 7 <= d->largo && memcmp(p + i, "trailer", 7) == 0) {
			ultimo_trailer = i + 7;
		}
	}
	if (!d->cifrado || d->c.listo)
		registra_objstm(d);
	busca_trailer(d, ultimo_trailer);
}

/* los flujos de objetos: lo que llevan, si no está suelto */
static void registra_objstm(pdoc *d)
{
	u32 i;

	for (i = 1; i < d->n; i++) {
		pobj *o;

		if (d->e[i].tipo != 1)
			continue;
		o = carga(d, i, 0);
		if (o && o->t == O_FLUJO && o_es_nombre(o_dic_clave(o, "Type"), "ObjStm")) {
			u32 n, k, cuantos = (u32)o_num(dic_r(d, o, "N"), 0), largo;
			u8 *datos;
			plex l;
			ptok a, b;

			if (cuantos > 100000)
				continue;
			datos = flujo_datos(d, o, &largo, 0, NULL);
			if (!datos)
				continue;
			l.ini = l.p = datos;
			l.fin = datos + largo;
			for (k = 0; k < cuantos; k++) {
				if (lex_token(&l, &a) != T_ENT || lex_token(&l, &b) != T_ENT)
					break;
				n = (u32)a.i;
				if (a.i > 0 && n < MAX_OBJETOS && sitio(d, n) && d->e[n].tipo == 0)
					apunta(d, n, 2, i, 0, k, 1);
			}
			so_suelta(datos);
		}
	}
}

/* el trailer: el último del archivo; o el de una tabla en flujo; o uno
 * nuevo con el catálogo que se encuentre */
static void busca_trailer(pdoc *d, u32 ultimo_trailer)
{
	u32 i;

	if (ultimo_trailer) {
		plex l;
		pobj *t;

		l.ini = d->datos;
		l.p = d->datos + ultimo_trailer;
		l.fin = d->datos + d->largo;
		t = lee_objeto(d, &l, 0);
		if (t && t->t == O_DICC && o_dic_clave(t, "Root"))
			d->trailer = t;
	}
	if (!d->trailer || !o_dic_clave(d->trailer, "Root")) {
		pobj *t = o_dicc(d);

		for (i = 1; i < d->n; i++) {
			pobj *o;

			if (!d->e[i].tipo)
				continue;
			o = carga(d, i, 0);
			if (o && o->t == O_FLUJO && o_es_nombre(o_dic_clave(o, "Type"), "XRef") && o_dic_clave(o, "Root")) {
				t = o->u.f.dic;
			} else if (o && o->t == O_DICC && o_es_nombre(o_dic_clave(o, "Type"), "Catalog") &&
				   !o_dic_clave(t, "Root")) {
				o_pon(d, t, "Root", o_ref(d, i, d->e[i].gen));
			}
		}
		if (d->trailer) {
			/* lo que tuviera el de antes (el cifrado, el ID) */
			pobj *v;

			if ((v = o_dic_clave(d->trailer, "Encrypt")) && !o_dic_clave(t, "Encrypt"))
				o_pon(d, t, "Encrypt", v);
			if ((v = o_dic_clave(d->trailer, "ID")) && !o_dic_clave(t, "ID"))
				o_pon(d, t, "ID", v);
			if ((v = o_dic_clave(d->trailer, "Info")) && !o_dic_clave(t, "Info"))
				o_pon(d, t, "Info", v);
		}
		d->trailer = t;
	}
}

/* el objeto indirecto que empieza en pos («num gen obj ...»). Un flujo, con
 * sus datos en el archivo (sin descifrar) */
static pobj *lee_indirecto(pdoc *d, u32 pos, u32 *num, u32 *gen, int hondo)
{
	plex l;
	ptok a, b, c;
	pobj *o;

	if (pos >= d->largo)
		return NULL;
	l.ini = d->datos;
	l.p = d->datos + pos;
	l.fin = d->datos + d->largo;
	if (lex_token(&l, &a) != T_ENT || lex_token(&l, &b) != T_ENT || lex_token(&l, &c) != T_PALABRA || !tok_es(&c, "obj"))
		return NULL;
	if (a.i < 0 || b.i < 0)
		return NULL;
	*num = (u32)a.i;
	*gen = (u32)b.i;
	o = lee_objeto(d, &l, 0);
	if (!o)
		return NULL;
	if (o->t == O_DICC) {
		const u8 *antes = l.p;
		ptok t;

		if (lex_token(&l, &t) == T_PALABRA && tok_es(&t, "stream")) {
			const u8 *ini = l.p, *fin = d->datos + d->largo;
			pobj *lg = o_dic_clave(o, "Length"), *f;
			i64 largo = -1;
			u32 k;

			if (ini < fin && *ini == '\r')
				ini++;
			if (ini < fin && *ini == '\n')
				ini++;
			if (lg && lg->t == O_REF && hondo < 8) {
				pentrada *e = lg->u.ref.num < d->n ? &d->e[lg->u.ref.num] : NULL;

				if (e && !e->cargando)
					largo = (i64)o_num(carga(d, lg->u.ref.num, hondo + 1), -1);
			} else
				largo = (i64)o_num(lg, -1);
			/* ¿acaba donde dice? (si no, se busca «endstream») */
			if (largo >= 0 && largo <= fin - ini) {
				const u8 *q = ini + largo;

				while (q < fin && es_blanco(*q))
					q++;
				if (fin - q < 9 || memcmp(q, "endstream", 9) != 0)
					largo = -1;
			} else
				largo = -1;
			if (largo < 0) {
				const u8 *q;

				for (q = ini; q + 9 <= fin; q++)
					if (*q == 'e' && memcmp(q, "endstream", 9) == 0)
						break;
				if (q + 9 > fin) {
					/* (sin «endstream»: hasta «endobj», o hasta el final) */
					for (q = ini; q + 6 <= fin; q++)
						if (*q == 'e' && memcmp(q, "endobj", 6) == 0)
							break;
					if (q + 6 > fin)
						q = fin;
				}
				k = (u32)(q - ini);
				/* sin el salto de línea de delante de «endstream» */
				if (k > 0 && ini[k - 1] == '\n')
					k--;
				if (k > 0 && ini[k - 1] == '\r')
					k--;
				largo = k;
			}
			f = o_nuevo(d, O_FLUJO);
			f->u.f.dic = o;
			f->u.f.datos = ini;
			f->u.f.largo = (u32)largo;
			f->u.f.num = *num;
			f->u.f.gen = *gen;
			f->cifrada = 1;
			return f;
		}
		l.p = antes;
	}
	return o;
}

/* el objeto num, del archivo o de su flujo de objetos */
static pobj *carga(pdoc *d, u32 num, int hondo)
{
	pentrada *e;
	pobj *o = NULL;

	if (num >= d->n || hondo > 16)
		return NULL;
	e = &d->e[num];
	if (e->obj)
		return e->obj;
	if (e->cargando || e->malo || e->tipo == 0)
		return NULL;
	e->cargando = 1;
	if (e->tipo == 1) {
		u32 n, g;

		o = lee_indirecto(d, e->pos, &n, &g, hondo);
		if (o && n != num)
			o = NULL;
		if (o && d->cifrado && num != d->c.num_encrypt) {
			if (o->t == O_FLUJO)
				descifra_cadenas(d, o->u.f.dic, num, g, 0);
			else
				descifra_cadenas(d, o, num, g, 0);
		}
		if (o && o->t == O_FLUJO && (!d->cifrado || num == d->c.num_encrypt))
			o->cifrada = 0;
	} else if (e->tipo == 2) {
		pobj *f = carga(d, e->pos, hondo + 1);

		if (f && f->t == O_FLUJO && e->pos != num) {
			u32 cuantos = (u32)o_num(resuelve(d, o_dic_clave(f, "N")), 0);
			u32 primero = (u32)o_num(resuelve(d, o_dic_clave(f, "First")), 0);
			u32 largo, k;
			u8 *datos = cuantos <= 1000000 ? flujo_datos(d, f, &largo, 0, NULL) : NULL;

			if (datos) {
				plex l;
				ptok a, b;
				u32 desp = 0xFFFFFFFF;

				/* los pares «número posición» del principio */
				l.ini = l.p = datos;
				l.fin = datos + largo;
				for (k = 0; k < cuantos; k++) {
					if (lex_token(&l, &a) != T_ENT || lex_token(&l, &b) != T_ENT)
						break;
					if ((u32)a.i == num && (k == e->indice || desp == 0xFFFFFFFF)) {
						desp = (u32)b.i;
						if (k == e->indice)
							break;
					}
				}
				if (desp != 0xFFFFFFFF && primero + desp < largo) {
					/* el objeto se copia: los datos del flujo se sueltan */
					u32 resto = largo - primero - desp;
					u8 *copia = ar_pide(d, resto + 1);

					memcpy(copia, datos + primero + desp, resto);
					l.ini = l.p = copia;
					l.fin = copia + resto;
					o = lee_objeto(d, &l, 0);
				}
				so_suelta(datos);
			}
		}
	}
	e->cargando = 0;
	if (!o) {
		e->malo = 1;
		return NULL;
	}
	e->obj = o;
	return o;
}

pobj *objeto(pdoc *d, u32 num)
{
	pobj *o = carga(d, num, 0);

	if (!o && !d->intentado_reparar && d->datos && num < d->n && d->e[num].tipo == 1) {
		/* la tabla miente: se rehace una vez */
		repara(d);
		o = carga(d, num, 0);
	}
	return o;
}

pobj *resuelve(pdoc *d, pobj *o)
{
	int k;

	for (k = 0; o && o->t == O_REF && k < 32; k++)
		o = objeto(d, o->u.ref.num);
	if (!o || o->t == O_REF)
		return d->nulo;
	return o;
}

pobj *dic_r(pdoc *d, pobj *dic, const char *clave)
{
	pobj *o = o_dic_clave(dic, clave);

	if (!o)
		return NULL;
	o = resuelve(d, o);
	return o->t == O_NULO ? NULL : o;
}

/* ---- los datos de los flujos ---- */
u8 *flujo_crudo(pdoc *d, pobj *f, u32 *n)
{
	u8 *r;

	if (!f || f->t != O_FLUJO)
		return NULL;
	if (f->cifrada && d && d->cifrado) {
		pobj *t = o_dic_clave(f, "Type");

		if (o_es_nombre(t, "XRef"))
			goto tal_cual;
		if (o_es_nombre(t, "Metadata") && !d->c.cifra_metadatos)
			goto tal_cual;
		/* (un /Crypt /Identity en sus filtros: tampoco) */
		{
			pobj *fl = resuelve(d, o_dic_clave(f, "Filter"));

			if (o_es_nombre(fl, "Crypt") || (fl->t == O_ARREGLO && fl->n > 0 && o_es_nombre(resuelve(d, fl->u.v[0]), "Crypt")))
				goto tal_cual;
		}
		return descifra(d, f->u.f.num, f->u.f.gen, f->u.f.datos, f->u.f.largo, n, 0);
	}
tal_cual:
	r = pide(f->u.f.largo + 1);
	if (f->u.f.largo)
		memcpy(r, f->u.f.datos, f->u.f.largo);
	*n = f->u.f.largo;
	return r;
}

static int es_filtro_imagen(pobj *o)
{
	return o_es_nombre(o, "DCTDecode") || o_es_nombre(o, "DCT") || o_es_nombre(o, "JPXDecode") ||
	       o_es_nombre(o, "JBIG2Decode") || o_es_nombre(o, "CCITTFaxDecode") || o_es_nombre(o, "CCF");
}

u8 *flujo_datos(pdoc *d, pobj *f, u32 *n, int imagen, const char **queda)
{
	pobj *fl, *pr;
	u8 *datos;
	u32 largo, k, cuantos;

	if (queda)
		*queda = NULL;
	datos = flujo_crudo(d, f, &largo);
	if (!datos)
		return NULL;
	fl = resuelve(d, o_dic_clave(f, "Filter"));
	pr = resuelve(d, o_dic_clave(f, "DecodeParms"));
	if (pr->t == O_NULO)
		pr = resuelve(d, o_dic_clave(f, "DP"));
	cuantos = fl->t == O_ARREGLO ? fl->n : fl->t == O_NOMBRE ? 1 : 0;
	if (cuantos > 8) {
		so_suelta(datos);
		return NULL;
	}
	for (k = 0; k < cuantos; k++) {
		pobj *uno = fl->t == O_ARREGLO ? resuelve(d, fl->u.v[k]) : fl;
		pobj *p = pr->t == O_ARREGLO ? (k < pr->n ? resuelve(d, pr->u.v[k]) : d->nulo) : pr;
		u8 *otro;
		u32 n2;

		if (uno->t != O_NOMBRE)
			break;
		if (es_filtro_imagen(uno)) {
			if (imagen) {
				if (queda)
					*queda = (const char *)uno->u.s;
				break;
			}
			so_suelta(datos);
			return NULL;
		}
		if (o_es_nombre(uno, "Crypt"))
			continue;
		otro = filtro(d, (const char *)uno->u.s, p, datos, largo, &n2);
		so_suelta(datos);
		if (!otro)
			return NULL;
		datos = otro;
		largo = n2;
	}
	*n = largo;
	return datos;
}

/* ---- abrir ---- */
pdoc *pdf_abre(u8 *datos, u32 largo, const char *clave, int *necesita_clave)
{
	pdoc *d = doc_nuevo();
	u32 i;
	pobj *raiz;

	if (necesita_clave)
		*necesita_clave = 0;
	d->datos = datos;
	d->largo = largo;
	/* «%PDF-1.x» en el primer KB */
	d->version = 14;
	for (i = 0; i + 8 <= largo && i < 1024; i++)
		if (memcmp(datos + i, "%PDF-", 5) == 0) {
			if (datos[i + 5] >= '1' && datos[i + 5] <= '2' && datos[i + 6] == '.' && datos[i + 7] >= '0' && datos[i + 7] <= '9')
				d->version = (datos[i + 5] - '0') * 10 + (datos[i + 7] - '0');
			break;
		}
	if (i + 8 > largo || i >= 1024) {
		fmt(d->error, sizeof d->error, "no es un PDF (no empieza por %%PDF-)");
		d->datos = NULL;
		pdf_cierra(d);
		return NULL;
	}
	if (!lee_tablas(d) || !o_dic_clave(d->trailer, "Root"))
		repara(d);
	/* el cifrado, antes de leer nada más */
	{
		pobj *enc = o_dic_clave(d->trailer, "Encrypt");

		if (enc && enc->t != O_NULO) {
			int r;

			if (enc->t == O_REF)
				d->c.num_encrypt = enc->u.ref.num;
			d->cifrado = 1;
			enc = resuelve(d, enc);
			r = cifrado_prepara(d, enc, clave);
			if (r <= 0) {
				if (necesita_clave)
					*necesita_clave = r == 0 ? 1 : 2;
				d->datos = NULL;
				pdf_cierra(d);
				return NULL;
			}
			/* lo leído antes de saber el cifrado, otra vez (ya descifrado) */
			d->c.listo = 1;
			for (i = 0; i < d->n; i++)
				if (i != d->c.num_encrypt) {
					d->e[i].obj = NULL;
					d->e[i].malo = 0;
				}
			if (d->reparado)
				registra_objstm(d);
		}
	}
	raiz = dic_r(d, d->trailer, "Root");
	if ((!raiz || raiz->t != O_DICC) && !d->reparado) {
		repara(d);
		raiz = dic_r(d, d->trailer, "Root");
	}
	if (!raiz || raiz->t != O_DICC) {
		d->datos = NULL;
		pdf_cierra(d);
		return NULL;
	}
	d->raiz = raiz;
	if (!pdf_paginas(d) && !d->reparado) {
		repara(d);
		d->raiz = dic_r(d, d->trailer, "Root");
		if (d->raiz)
			pdf_paginas(d);
	}
	/* la versión del catálogo, si es más alta */
	{
		pobj *v = dic_r(d, d->raiz, "Version");

		if (v && v->t == O_NOMBRE && v->n >= 3 && v->u.s[1] == '.')
			if ((v->u.s[0] - '0') * 10 + (v->u.s[2] - '0') > d->version)
				d->version = (v->u.s[0] - '0') * 10 + (v->u.s[2] - '0');
	}
	return d;
}

pdoc *pdf_abre_archivo(const char *ruta, const char *clave, int *necesita_clave, const char **error)
{
	u32 largo;
	u8 *datos = so_lee_archivo(ruta, &largo, MAX_ARCHIVO_PDF, error);
	pdoc *d;

	if (!datos)
		return NULL;
	d = pdf_abre(datos, largo, clave, necesita_clave);
	if (!d) {
		so_suelta(datos);
		if (error && necesita_clave && *necesita_clave == 1)
			*error = "lleva contraseña (/CLAVE=...)";
		else if (error && necesita_clave && *necesita_clave == 2)
			*error = "su cifrado no se conoce";
		else if (error)
			*error = "no es un PDF que se pueda leer";
	}
	return d;
}

/* ---- el árbol de las páginas, en orden (sin recursión: con pila) ---- */
int pdf_paginas(pdoc *d)
{
	struct { pobj *nodo; u32 k; } pila[MAX_HONDO];
	int n = 0;
	u32 cap = 0, vueltas = 0;
	pobj *raiz = dic_r(d, d->raiz, "Pages");
	u8 *visto;

	d->n_paginas = 0;
	if (!raiz || raiz->t != O_DICC)
		return 0;
	visto = pide(d->n ? d->n : 1);
	memset(visto, 0, d->n ? d->n : 1);
	{
		pobj *r = o_dic_clave(d->raiz, "Pages");

		if (r && r->t == O_REF && r->u.ref.num < d->n)
			visto[r->u.ref.num] = 1;
	}
	pila[n].nodo = raiz;
	pila[n].k = 0;
	n++;
	while (n > 0) {
		pobj *nodo = pila[n - 1].nodo, *kids = dic_r(d, nodo, "Kids");
		pobj *hijo, *ref;

		if (++vueltas > 4 * MAX_PAGINAS + 100000)
			break;
		if (!kids || kids->t != O_ARREGLO || pila[n - 1].k >= kids->n) {
			n--;
			continue;
		}
		ref = kids->u.v[pila[n - 1].k++];
		if (ref->t != O_REF || ref->u.ref.num >= d->n || visto[ref->u.ref.num])
			continue;
		visto[ref->u.ref.num] = 1;
		hijo = resuelve(d, ref);
		if (hijo->t != O_DICC)
			continue;
		if (o_es_nombre(o_dic_clave(hijo, "Type"), "Pages") || (!o_dic_clave(hijo, "Type") && dic_r(d, hijo, "Kids"))) {
			if (n < MAX_HONDO) {
				pila[n].nodo = hijo;
				pila[n].k = 0;
				n++;
			}
			continue;
		}
		/* una hoja: una página */
		if (d->n_paginas >= MAX_PAGINAS)
			break;
		if (d->n_paginas >= cap) {
			cap = cap ? cap * 2 : 64;
			d->paginas = repide(d->paginas, cap * sizeof(pobj *));
			d->num_pagina = repide(d->num_pagina, cap * sizeof(u32));
		}
		d->paginas[d->n_paginas] = hijo;
		d->num_pagina[d->n_paginas] = ref->u.ref.num;
		d->n_paginas++;
	}
	so_suelta(visto);
	return d->n_paginas > 0;
}

/* lo que la página hereda de sus padres (Resources, MediaBox, CropBox, Rotate) */
pobj *pagina_heredada(pdoc *d, u32 i, const char *clave)
{
	pobj *p = i < d->n_paginas ? d->paginas[i] : NULL;
	int k;

	for (k = 0; p && k < MAX_HONDO; k++) {
		pobj *v = dic_r(d, p, clave);

		if (v)
			return v;
		p = dic_r(d, p, "Parent");
		if (p && p->t != O_DICC)
			break;
	}
	return NULL;
}

static int caja_de(pdoc *d, pobj *a, double c[4])
{
	int k;

	if (!a || a->t != O_ARREGLO || a->n < 4)
		return 0;
	for (k = 0; k < 4; k++) {
		pobj *x = resuelve(d, a->u.v[k]);

		if (!o_es_num(x))
			return 0;
		c[k] = o_num(x, 0);
	}
	if (c[0] > c[2]) {
		double t = c[0];
		c[0] = c[2];
		c[2] = t;
	}
	if (c[1] > c[3]) {
		double t = c[1];
		c[1] = c[3];
		c[3] = t;
	}
	return c[2] - c[0] >= 1 && c[3] - c[1] >= 1;
}

void pagina_caja(pdoc *d, u32 i, double c[4])
{
	double m[4];

	if (!caja_de(d, pagina_heredada(d, i, "MediaBox"), m)) {
		m[0] = m[1] = 0;
		m[2] = 612;
		m[3] = 792;
	}
	if (caja_de(d, pagina_heredada(d, i, "CropBox"), c)) {
		/* (la CropBox, dentro de la MediaBox) */
		if (c[0] < m[0]) c[0] = m[0];
		if (c[1] < m[1]) c[1] = m[1];
		if (c[2] > m[2]) c[2] = m[2];
		if (c[3] > m[3]) c[3] = m[3];
		if (c[2] - c[0] >= 1 && c[3] - c[1] >= 1)
			return;
	}
	memcpy(c, m, sizeof m);
}

int pagina_giro(pdoc *d, u32 i)
{
	int g = (int)o_num(pagina_heredada(d, i, "Rotate"), 0);

	g %= 360;
	if (g < 0)
		g += 360;
	return (g / 90) * 90;
}
