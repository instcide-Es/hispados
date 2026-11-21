/* objetos.c - Hispa-PDF: la memoria del documento, los objetos del PDF y su
 * analizador (las palabras, los números, los nombres, las cadenas, los
 * arreglos y los diccionarios). Sin recursión sin tope: lo anidado, hasta
 * MAX_HONDO.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "hpdf.h"

so_salto *hpdf_salto;
char hpdf_mensaje[200];

void hpdf_falla(const char *f, ...)
{
	va_list va;

	va_start(va, f);
	vfmt(hpdf_mensaje, sizeof hpdf_mensaje, f, va);
	va_end(va);
	so_salta(*hpdf_salto, 1);
}

void *pide(size_t n)
{
	void *p = so_pide(n ? n : 1);

	if (!p)
		hpdf_falla("no queda memoria (se han pedido %u octetos)", (u32)n);
	return p;
}

void *repide(void *p, size_t n)
{
	void *q = so_repide(p, n ? n : 1);

	if (!q)
		hpdf_falla("no queda memoria (se han pedido %u octetos)", (u32)n);
	return q;
}

/* ---- la memoria del documento: bloques que se sueltan al cerrarlo ---- */
struct bloque {
	bloque *sig;
	u32 usado, cap;
	u32 relleno;
	/* y los datos */
};
#define TAM_BLOQUE (64u * 1024)

void *ar_pide(pdoc *d, u32 n)
{
	bloque *b = d->bloques;
	u8 *r;

	n = (n + 7) & ~7u;
	if (n > TAM_BLOQUE / 4) {
		/* uno suyo, detrás del primero (el de ahora sigue sirviendo) */
		bloque *g = pide(sizeof(bloque) + n);

		g->usado = g->cap = n;
		if (b) {
			g->sig = b->sig;
			b->sig = g;
		} else {
			g->sig = NULL;
			d->bloques = g;
			/* (y que el siguiente pequeño no caiga en este, que está lleno) */
		}
		r = (u8 *)(g + 1);
		memset(r, 0, n);
		return r;
	}
	if (!b || b->cap - b->usado < n) {
		b = pide(sizeof(bloque) + TAM_BLOQUE);
		b->usado = 0;
		b->cap = TAM_BLOQUE;
		b->sig = d->bloques;
		d->bloques = b;
	}
	r = (u8 *)(b + 1) + b->usado;
	b->usado += n;
	memset(r, 0, n);
	return r;
}

void ar_suelta(pdoc *d)
{
	bloque *b = d->bloques, *s;

	while (b) {
		s = b->sig;
		so_suelta(b);
		b = s;
	}
	d->bloques = NULL;
}

/* ---- los objetos ---- */
pobj *o_nuevo(pdoc *d, int t)
{
	pobj *o = ar_pide(d, sizeof(pobj));

	o->t = (u8)t;
	return o;
}

pobj *o_ent(pdoc *d, i32 v)
{
	pobj *o = o_nuevo(d, O_ENT);

	o->u.i = v;
	return o;
}

pobj *o_real(pdoc *d, double v)
{
	pobj *o;

	if (v == (double)(i32)v && v > -2147483647.0 && v < 2147483647.0)
		return o_ent(d, (i32)v);
	o = o_nuevo(d, O_REAL);
	o->u.r = v;
	return o;
}

pobj *o_nombre(pdoc *d, const char *s)
{
	pobj *o = o_nuevo(d, O_NOMBRE);
	u32 n = (u32)strlen(s);

	o->u.s = ar_pide(d, n + 1);
	memcpy(o->u.s, s, n);
	o->n = n;
	return o;
}

pobj *o_cadena(pdoc *d, const u8 *s, u32 n)
{
	pobj *o = o_nuevo(d, O_CADENA);

	o->u.s = ar_pide(d, n + 1);
	if (n)
		memcpy(o->u.s, s, n);
	o->n = n;
	return o;
}

pobj *o_ref(pdoc *d, u32 num, u32 gen)
{
	pobj *o = o_nuevo(d, O_REF);

	o->u.ref.num = num;
	o->u.ref.gen = gen;
	return o;
}

pobj *o_arreglo(pdoc *d)
{
	return o_nuevo(d, O_ARREGLO);
}

pobj *o_dicc(pdoc *d)
{
	return o_nuevo(d, O_DICC);
}

static void crece(pdoc *d, pobj *a, u32 hace_falta)
{
	u32 c;
	pobj **v;

	if (hace_falta <= a->cap)
		return;
	if (hace_falta > 2 * MAX_ELEMENTOS)
		hpdf_falla("demasiados elementos en un arreglo o un diccionario");
	c = a->cap ? a->cap * 2 : 8;
	while (c < hace_falta)
		c *= 2;
	v = ar_pide(d, c * (u32)sizeof(pobj *));
	if (a->cap)
		memcpy(v, a->u.v, a->cap * sizeof(pobj *));
	a->u.v = v;
	a->cap = c;
}

void o_mete(pdoc *d, pobj *a, pobj *x)
{
	crece(d, a, a->n + 1);
	a->u.v[a->n++] = x;
}

void o_pon(pdoc *d, pobj *dic, const char *clave, pobj *x)
{
	u32 i;

	if (dic->t == O_FLUJO)
		dic = dic->u.f.dic;
	for (i = 0; i < dic->n; i++)
		if (o_es_nombre(dic->u.v[2 * i], clave)) {
			if (x)
				dic->u.v[2 * i + 1] = x;
			else {
				memmove(&dic->u.v[2 * i], &dic->u.v[2 * i + 2], (dic->n - i - 1) * 2 * sizeof(pobj *));
				dic->n--;
			}
			return;
		}
	if (!x)
		return;
	crece(d, dic, 2 * dic->n + 2);
	dic->u.v[2 * dic->n] = o_nombre(d, clave);
	dic->u.v[2 * dic->n + 1] = x;
	dic->n++;
}

pobj *o_dic_de(pobj *o)
{
	if (!o)
		return NULL;
	if (o->t == O_FLUJO)
		return o->u.f.dic;
	if (o->t == O_DICC)
		return o;
	return NULL;
}

pobj *o_dic_clave(pobj *dic, const char *clave)
{
	u32 i;

	dic = o_dic_de(dic);
	if (!dic)
		return NULL;
	for (i = 0; i < dic->n; i++)
		if (o_es_nombre(dic->u.v[2 * i], clave))
			return dic->u.v[2 * i + 1];
	return NULL;
}

int o_es_nombre(pobj *o, const char *s)
{
	return o && o->t == O_NOMBRE && strcmp((const char *)o->u.s, s) == 0;
}

int o_es_num(pobj *o)
{
	return o && (o->t == O_ENT || o->t == O_REAL);
}

double o_num(pobj *o, double si_no)
{
	if (!o)
		return si_no;
	if (o->t == O_ENT)
		return o->u.i;
	if (o->t == O_REAL)
		return o->u.r;
	return si_no;
}

/* ---- el analizador ---- */
int es_blanco(int c)
{
	return c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '\f' || c == 0;
}

int es_delim(int c)
{
	return c == '(' || c == ')' || c == '<' || c == '>' || c == '[' || c == ']' || c == '{' || c == '}' ||
	       c == '/' || c == '%';
}

void lex_blancos(plex *l)
{
	while (l->p < l->fin) {
		if (es_blanco(*l->p))
			l->p++;
		else if (*l->p == '%') {
			while (l->p < l->fin && *l->p != '\n' && *l->p != '\r')
				l->p++;
		} else
			break;
	}
}

static int hexv(int c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

/* una cadena (...) desde p (tras el paréntesis): la deja en sal (si no es
 * NULL) y da su largo; *tras, detrás del paréntesis que la cierra */
u32 cadena_literal(const u8 *p, const u8 *fin, u8 *sal, const u8 **tras)
{
	u32 n = 0;
	int hondo = 1;

	while (p < fin) {
		int c = *p++;

		if (c == '\\') {
			if (p >= fin)
				break;
			c = *p++;
			switch (c) {
			case 'n': c = '\n'; break;
			case 'r': c = '\r'; break;
			case 't': c = '\t'; break;
			case 'b': c = '\b'; break;
			case 'f': c = '\f'; break;
			case '\r':
				if (p < fin && *p == '\n')
					p++;
				continue;
			case '\n':
				continue;
			default:
				if (c >= '0' && c <= '7') {
					int v = c - '0', k;

					for (k = 0; k < 2 && p < fin && *p >= '0' && *p <= '7'; k++)
						v = v * 8 + (*p++ - '0');
					c = v & 0xFF;
				}
			}
		} else if (c == '(') {
			hondo++;
		} else if (c == ')') {
			if (--hondo == 0)
				break;
		}
		if (sal)
			sal[n] = (u8)c;
		n++;
	}
	*tras = p;
	return n;
}

u32 cadena_hex(const u8 *p, const u8 *fin, u8 *sal, const u8 **tras)
{
	u32 n = 0;
	int alto = -1;

	while (p < fin && *p != '>') {
		int v = hexv(*p++);

		if (v < 0)
			continue;
		if (alto < 0)
			alto = v;
		else {
			if (sal)
				sal[n] = (u8)(alto * 16 + v);
			n++;
			alto = -1;
		}
	}
	if (alto >= 0) {
		if (sal)
			sal[n] = (u8)(alto * 16);
		n++;
	}
	if (p < fin)
		p++;
	*tras = p;
	return n;
}

int tok_es(ptok *t, const char *s)
{
	u32 n = (u32)strlen(s);

	return t->t == T_PALABRA && t->n == n && memcmp(t->p, s, n) == 0;
}

/* un número: entero si se puede; los raros («--5», «4.5.6») se leen como se pueda */
static void numero(plex *l, ptok *t)
{
	const u8 *p = l->p;
	int menos = 0, punto = 0, cifras = 0;
	double v = 0, esc = 1;
	i64 e = 0;

	while (p < l->fin && (*p == '+' || *p == '-')) {
		if (*p == '-')
			menos = !menos;
		p++;
	}
	while (p < l->fin && ((*p >= '0' && *p <= '9') || *p == '.')) {
		if (*p == '.') {
			if (punto)
				break;
			punto = 1;
		} else {
			cifras++;
			if (punto) {
				esc /= 10;
				v += (*p - '0') * esc;
			} else {
				v = v * 10 + (*p - '0');
				if (e < 10000000000LL)
					e = e * 10 + (*p - '0');
			}
		}
		p++;
	}
	/* lo que quede pegado (un número mal escrito) se salta */
	while (p < l->fin && !es_blanco(*p) && !es_delim(*p))
		p++;
	l->p = p;
	if (!cifras) {
		t->t = T_ENT;
		t->i = 0;
		return;
	}
	if (menos) {
		v = -v;
		e = -e;
	}
	if (!punto && e >= -2147483647LL && e <= 2147483647LL) {
		t->t = T_ENT;
		t->i = (i32)e;
	} else {
		t->t = T_REAL;
		t->r = v;
	}
}

int lex_token(plex *l, ptok *t)
{
	int c;

	lex_blancos(l);
	t->p = l->p;
	t->n = 0;
	if (l->p >= l->fin)
		return t->t = T_FIN;
	c = *l->p;
	switch (c) {
	case '[': l->p++; return t->t = T_ABRE_A;
	case ']': l->p++; return t->t = T_CIERRA_A;
	case '{': l->p++; return t->t = T_ABRE_L;
	case '}': l->p++; return t->t = T_CIERRA_L;
	case '<':
		if (l->p + 1 < l->fin && l->p[1] == '<') {
			l->p += 2;
			return t->t = T_ABRE_D;
		}
		{
			const u8 *tras;

			t->p = l->p + 1;
			t->n = cadena_hex(l->p + 1, l->fin, NULL, &tras);
			l->p = tras;
			return t->t = T_HEX;
		}
	case '>':
		if (l->p + 1 < l->fin && l->p[1] == '>') {
			l->p += 2;
			return t->t = T_CIERRA_D;
		}
		l->p++;
		return t->t = T_MALO;
	case '(': {
		const u8 *tras;

		t->p = l->p + 1;
		t->n = cadena_literal(l->p + 1, l->fin, NULL, &tras);
		l->p = tras;
		return t->t = T_CADENA;
	}
	case ')':
		l->p++;
		return t->t = T_MALO;
	case '/':
		l->p++;
		t->p = l->p;
		while (l->p < l->fin && !es_blanco(*l->p) && !es_delim(*l->p))
			l->p++;
		t->n = (u32)(l->p - t->p);
		return t->t = T_NOMBRE;
	}
	if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.') {
		numero(l, t);
		return t->t;
	}
	while (l->p < l->fin && !es_blanco(*l->p) && !es_delim(*l->p))
		l->p++;
	if (l->p == t->p)
		l->p++;
	t->n = (u32)(l->p - t->p);
	return t->t = T_PALABRA;
}

/* el nombre, con sus #xx ya cambiados */
static pobj *nombre_de_tok(pdoc *d, ptok *t)
{
	pobj *o = o_nuevo(d, O_NOMBRE);
	u32 i, n = 0;

	o->u.s = ar_pide(d, t->n + 1);
	for (i = 0; i < t->n; i++) {
		u8 c = t->p[i];

		if (c == '#' && i + 2 < t->n && hexv(t->p[i + 1]) >= 0 && hexv(t->p[i + 2]) >= 0) {
			c = (u8)(hexv(t->p[i + 1]) * 16 + hexv(t->p[i + 2]));
			i += 2;
		}
		o->u.s[n++] = c;
	}
	o->n = n;
	return o;
}

static pobj *cadena_de_tok(pdoc *d, ptok *t, const u8 *fin)
{
	pobj *o = o_nuevo(d, O_CADENA);
	const u8 *tras;

	if (t->n > MAX_CADENA)
		hpdf_falla("una cadena demasiado larga");
	o->u.s = ar_pide(d, t->n + 1);
	if (t->t == T_CADENA)
		o->n = cadena_literal(t->p, fin, o->u.s, &tras);
	else {
		o->n = cadena_hex(t->p, fin, o->u.s, &tras);
		o->hex = 1;
	}
	return o;
}

/* o, dentro del arreglo o diccionario a (con su clave pendiente) */
static void mete_en(pdoc *d, pobj *a, pobj **clave, pobj *o)
{
	if (a->t == O_ARREGLO) {
		if (a->n >= MAX_ELEMENTOS)
			hpdf_falla("un arreglo con demasiados elementos");
		o_mete(d, a, o);
	} else if (*clave) {
		if (a->n >= MAX_ELEMENTOS)
			hpdf_falla("un diccionario con demasiadas claves");
		crece(d, a, 2 * a->n + 2);
		a->u.v[2 * a->n] = *clave;
		a->u.v[2 * a->n + 1] = o;
		a->n++;
		*clave = NULL;
	} else if (o->t == O_NOMBRE) {
		*clave = o;
	}
	/* (un valor sin clave se tira) */
}

/* el objeto que empieza por el token t (ya leído). Lo anidado, con pila
 * propia (sin recursión): un arreglo o diccionario abierto por nivel. Lo que
 * se quede sin cerrar se cierra solo */
pobj *lee_desde_token(pdoc *d, plex *l, ptok *t0, int hondo0)
{
	pobj *pila[MAX_HONDO], *clave[MAX_HONDO];
	int n = 0;
	ptok tk = *t0, *t = &tk;
	pobj *o;

	(void)hondo0;
	for (;;) {
		o = NULL;
		switch (t->t) {
		case T_FIN:
			goto cierra;
		case T_ENT: {
			/* ¿«num gen R»? */
			const u8 *antes = l->p;
			ptok t2, t3;

			if (t->i >= 0 && lex_token(l, &t2) == T_ENT && t2.i >= 0 && lex_token(l, &t3) == T_PALABRA &&
			    tok_es(&t3, "R")) {
				o = o_ref(d, (u32)t->i, (u32)t2.i);
			} else {
				l->p = antes;
				o = o_ent(d, t->i);
			}
			break;
		}
		case T_REAL:
			o = o_nuevo(d, O_REAL);
			o->u.r = t->r;
			break;
		case T_NOMBRE:
			o = nombre_de_tok(d, t);
			break;
		case T_CADENA:
		case T_HEX:
			o = cadena_de_tok(d, t, l->fin);
			break;
		case T_ABRE_A:
		case T_ABRE_D:
			if (n >= MAX_HONDO)
				goto cierra;	/* (demasiado hondo: lo de dentro no se lee) */
			clave[n] = NULL;
			pila[n++] = t->t == T_ABRE_A ? o_arreglo(d) : o_dicc(d);
			lex_token(l, t);
			continue;
		case T_CIERRA_A:
		case T_CIERRA_D:
			if (n == 0)
				return NULL;
			o = pila[--n];
			break;
		case T_PALABRA:
			if (tok_es(t, "true") || tok_es(t, "false")) {
				o = o_nuevo(d, O_BOOL);
				o->u.b = tok_es(t, "true");
			} else if (tok_es(t, "null")) {
				o = o_nuevo(d, O_NULO);
			} else if (n > 0 && (tok_es(t, "endobj") || tok_es(t, "stream") || tok_es(t, "obj") ||
					     tok_es(t, "endstream") || tok_es(t, "xref") || tok_es(t, "trailer") ||
					     tok_es(t, "startxref"))) {
				/* un arreglo o diccionario sin cerrar: se cierra aquí */
				l->p = t->p;
				goto cierra;
			} else if (n == 0) {
				return NULL;
			} else {
				o = o_nuevo(d, O_NULO);		/* una palabra que no se conoce */
			}
			break;
		default:
			if (n == 0)
				return NULL;
			o = o_nuevo(d, O_NULO);
		}
		if (n == 0)
			return o;
		mete_en(d, pila[n - 1], &clave[n - 1], o);
		lex_token(l, t);
	}
cierra:
	if (n == 0)
		return NULL;
	while (n > 1) {
		o = pila[--n];
		mete_en(d, pila[n - 1], &clave[n - 1], o);
	}
	return pila[0];
}

pobj *lee_objeto(pdoc *d, plex *l, int hondo)
{
	ptok t;

	lex_token(l, &t);
	return lee_desde_token(d, l, &t, hondo);
}
