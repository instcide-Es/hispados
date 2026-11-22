/* escribe.c - Hispa-PDF: hacer un PDF nuevo. Copiar objetos de otros
 * documentos con sus números cambiados (sin traerse lo que no hace falta:
 * las páginas que no van y los nodos del árbol de páginas se quedan
 * fuera), y escribirlo: la tabla de siempre o, al comprimir, los objetos en
 * flujos de objetos con su tabla en un flujo (PDF 1.5); los flujos, otra vez
 * con Flate si así ocupan menos; las imágenes JPEG, más pequeñas; y el
 * cifrado, si se pide.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "hpdf.h"

#define MAPA_NULO 0xFFFFFFFFu

pnuevo *nuevo_doc(void)
{
	pnuevo *s = pide(sizeof(pnuevo));

	memset(s, 0, sizeof *s);
	s->d = pdf_vacio();
	s->cap = 64;
	s->v = pide(s->cap * sizeof(psalobj));
	memset(s->v, 0, s->cap * sizeof(psalobj));
	s->n = 1;		/* (el 0 no se usa) */
	s->version = 14;
	return s;
}

void nuevo_suelta(pnuevo *s)
{
	int i;

	if (!s)
		return;
	for (i = 0; i < s->n_mapa; i++)
		so_suelta(s->mapa[i].a);
	so_suelta(s->pend_num);
	so_suelta(s->pend_de);
	so_suelta(s->v);
	pdf_cierra(s->d);
	so_suelta(s);
}

u32 nuevo_obj(pnuevo *s, pobj *o, pdoc *de)
{
	if (s->n >= MAX_OBJETOS)
		hpdf_falla("demasiados objetos en el documento nuevo");
	if (s->n >= s->cap) {
		s->v = repide(s->v, s->cap * 2 * sizeof(psalobj));
		memset(s->v + s->cap, 0, s->cap * sizeof(psalobj));
		s->cap *= 2;
	}
	s->v[s->n].o = o;
	s->v[s->n].de = de;
	return s->n++;
}

/* el mapa de los números de «de» a los nuevos */
static int mapa_de(pnuevo *s, pdoc *de)
{
	int i;

	for (i = 0; i < s->n_mapa; i++)
		if (s->mapa[i].de == de)
			return i;
	if (s->n_mapa >= 64)
		hpdf_falla("demasiados documentos a la vez (hasta 64)");
	s->mapa[i].de = de;
	s->mapa[i].n = de->n;
	s->mapa[i].a = pide((de->n ? de->n : 1) * sizeof(u32));
	memset(s->mapa[i].a, 0, (de->n ? de->n : 1) * sizeof(u32));
	s->n_mapa++;
	return i;
}

u32 *mapa_num(pnuevo *s, pdoc *de, u32 num)
{
	int m = mapa_de(s, de);

	if (num >= s->mapa[m].n)
		return NULL;
	return &s->mapa[m].a[num];
}

static void pendiente(pnuevo *s, u32 nuevo, int m)
{
	if (s->n_pend >= s->cap_pend) {
		s->cap_pend = s->cap_pend ? s->cap_pend * 2 : 256;
		s->pend_num = repide(s->pend_num, s->cap_pend * sizeof(u32));
		s->pend_de = repide(s->pend_de, s->cap_pend * sizeof(u32));
	}
	s->pend_num[s->n_pend] = nuevo;
	s->pend_de[s->n_pend] = (u32)m;
	s->n_pend++;
}

static pobj *copia_r(pnuevo *s, pdoc *de, int m, pobj *o, int hondo)
{
	pobj *c;
	u32 i;

	if (!o)
		return s->d->nulo;
	if (hondo > MAX_HONDO + 4)
		return s->d->nulo;
	switch (o->t) {
	case O_REF: {
		u32 num = o->u.ref.num, *ma;
		pobj *x;

		if (num >= s->mapa[m].n)
			return s->d->nulo;
		ma = &s->mapa[m].a[num];
		if (*ma == MAPA_NULO)
			return s->d->nulo;
		if (*ma)
			return o_ref(s->d, *ma, 0);
		x = objeto(de, num);
		if (!x || x->t == O_NULO) {
			*ma = MAPA_NULO;
			return s->d->nulo;
		}
		/* una página que no va, o un nodo del árbol de páginas: no se trae */
		{
			pobj *t = o_dic_clave(o_dic_de(x), "Type");

			if (o_es_nombre(t, "Page") || o_es_nombre(t, "Pages")) {
				*ma = MAPA_NULO;
				return s->d->nulo;
			}
		}
		*ma = nuevo_obj(s, NULL, de);
		s->v[*ma].origen = num;
		pendiente(s, *ma, m);
		return o_ref(s->d, *ma, 0);
	}
	case O_ARREGLO:
		c = o_arreglo(s->d);
		for (i = 0; i < o->n; i++)
			o_mete(s->d, c, copia_r(s, de, m, o->u.v[i], hondo + 1));
		return c;
	case O_DICC:
		c = o_dicc(s->d);
		for (i = 0; i < o->n; i++) {
			pobj *v = copia_r(s, de, m, o->u.v[2 * i + 1], hondo + 1);

			o_pon(s->d, c, (const char *)o->u.v[2 * i]->u.s, v);
		}
		return c;
	case O_FLUJO:
		c = o_nuevo(s->d, O_FLUJO);
		*c = *o;
		c->u.f.dic = copia_r(s, de, m, o->u.f.dic, hondo + 1);
		return c;
	default:
		return o;	/* lo sencillo se comparte (no cambia) */
	}
}

pobj *copia_de(pnuevo *s, pdoc *de, pobj *o)
{
	return copia_r(s, de, mapa_de(s, de), o, 0);
}

/* lo pendiente: cada objeto apuntado, copiado (y lo que apunte él) */
void copia_pendientes(pnuevo *s)
{
	while (s->n_pend > 0) {
		u32 nuevo = s->pend_num[--s->n_pend];
		int m = (int)s->pend_de[s->n_pend];
		pdoc *de = s->mapa[m].de;

		pobj *c = copia_r(s, de, m, objeto(de, s->v[nuevo].origen), 0);

		s->v[nuevo].o = c;	/* (después: copia_r puede mover s->v) */
	}
}

/* =========================================================================
 *  Escribir
 * ========================================================================= */
typedef struct {
	void *f;
	u8 b[32768];
	u32 n;
	u32 pos;		/* lo escrito hasta ahora */
	int mal;
	u8 *m;			/* (a la memoria: los objetos de un flujo de objetos) */
	u32 mn, mcap;
	int en_memoria;
} pesc;

static void sale(pesc *e, const void *d, u32 n)
{
	const u8 *p = d;

	if (e->en_memoria) {
		if (e->mn + n > e->mcap) {
			u32 c = e->mcap ? e->mcap : 256;

			while (c < e->mn + n)
				c *= 2;
			e->m = repide(e->m, c);
			e->mcap = c;
		}
		memcpy(e->m + e->mn, d, n);
		e->mn += n;
		return;
	}
	e->pos += n;
	while (n) {
		u32 t = sizeof e->b - e->n < n ? (u32)sizeof e->b - e->n : n;

		memcpy(e->b + e->n, p, t);
		e->n += t;
		p += t;
		n -= t;
		if (e->n == sizeof e->b) {
			if (so_escribe(e->f, e->b, e->n))
				e->mal = 1;
			e->n = 0;
		}
	}
}

static void sale_t(pesc *e, const char *t)
{
	sale(e, t, (u32)strlen(t));
}

static void salef(pesc *e, const char *f, ...)
{
	char t[200];
	va_list va;
	int n;

	va_start(va, f);
	n = vfmt(t, sizeof t, f, va);
	va_end(va);
	if (n > (int)sizeof t - 1)
		n = sizeof t - 1;
	sale(e, t, (u32)n);
}

/* un número real, sin ceros de más */
static void sale_real(pesc *e, double v)
{
	char t[48];
	int n;

	if (v != v)
		v = 0;
	if (v > 1e9)
		v = 1e9;
	if (v < -1e9)
		v = -1e9;
	n = fmt(t, sizeof t, "%.5f", v);
	while (n > 1 && t[n - 1] == '0')
		t[--n] = 0;
	if (n > 1 && t[n - 1] == '.')
		t[--n] = 0;
	if (!strcmp(t, "-0"))
		strcpy(t, "0");
	sale(e, t, (u32)n);
}

static void sale_nombre(pesc *e, const u8 *s, u32 n)
{
	u32 i;

	sale(e, "/", 1);
	for (i = 0; i < n; i++) {
		u8 c = s[i];

		if (c < 0x21 || c > 0x7E || c == '#' || es_delim(c)) {
			char t[4];

			fmt(t, sizeof t, "#%02x", (u32)c);
			sale(e, t, 3);
		} else
			sale(e, &c, 1);
	}
}

static void sale_cadena(pesc *e, const u8 *s, u32 n, int hex)
{
	u32 i, raros = 0;

	for (i = 0; i < n; i++)
		if (s[i] < 32 || s[i] > 126)
			raros++;
	if (hex || raros > n / 4) {
		static const char h[] = "0123456789ABCDEF";

		sale(e, "<", 1);
		for (i = 0; i < n; i++) {
			char t[2] = {h[s[i] >> 4], h[s[i] & 15]};

			sale(e, t, 2);
		}
		sale(e, ">", 1);
		return;
	}
	sale(e, "(", 1);
	for (i = 0; i < n; i++) {
		u8 c = s[i];

		if (c == '(' || c == ')' || c == '\\') {
			char t[2] = {'\\', (char)c};

			sale(e, t, 2);
		} else if (c < 32 || c > 126) {
			char t[5];

			fmt(t, sizeof t, "\\%03u", 0);
			t[1] = (char)('0' + (c >> 6));
			t[2] = (char)('0' + ((c >> 3) & 7));
			t[3] = (char)('0' + (c & 7));
			sale(e, t, 4);
		} else
			sale(e, &c, 1);
	}
	sale(e, ")", 1);
}

typedef struct {
	pesc e;
	pnuevo *s;
	popciones *op;
	pcifra_sal cif;
	int cifra;
	u32 num_encrypt;
} pctx;

static void sale_obj(pctx *c, pobj *o, u32 num, int hondo);

static void sale_valor(pctx *c, pobj *o, u32 num, int hondo)
{
	pesc *e = &c->e;
	u32 i;

	if (!o || hondo > MAX_HONDO + 8) {
		sale_t(e, "null");
		return;
	}
	switch (o->t) {
	case O_NULO: sale_t(e, "null"); break;
	case O_BOOL: sale_t(e, o->u.b ? "true" : "false"); break;
	case O_ENT: salef(e, "%d", o->u.i); break;
	case O_REAL: sale_real(e, o->u.r); break;
	case O_NOMBRE: sale_nombre(e, o->u.s, o->n); break;
	case O_CADENA:
		if (c->cifra && num && num != c->num_encrypt) {
			u32 n;
			u8 *x = cifra(&c->cif, num, 0, o->u.s, o->n, &n);

			sale_cadena(e, x, n, 1);
			so_suelta(x);
		} else
			sale_cadena(e, o->u.s, o->n, o->hex);
		break;
	case O_REF: salef(e, "%u 0 R", o->u.ref.num); break;
	case O_ARREGLO:
		sale_t(e, "[");
		for (i = 0; i < o->n; i++) {
			if (i)
				sale_t(e, " ");
			sale_valor(c, o->u.v[i], num, hondo + 1);
		}
		sale_t(e, "]");
		break;
	case O_DICC:
		sale_t(e, "<<");
		for (i = 0; i < o->n; i++) {
			pobj *v = o->u.v[2 * i + 1];

			if (!v || v->t == O_NULO)
				continue;	/* (una clave a null es como si no estuviera) */
			sale_nombre(e, o->u.v[2 * i]->u.s, o->u.v[2 * i]->n);
			sale_t(e, " ");
			sale_valor(c, v, num, hondo + 1);
		}
		sale_t(e, ">>");
		break;
	case O_FLUJO:
		sale_t(e, "null");	/* (un flujo solo va suelto) */
		break;
	}
}

/* ---- comprimir los flujos ---- */
static int filtros_simples(pdoc *d, pobj *f)
{
	pobj *fl = resuelve(d, o_dic_clave(f, "Filter"));
	u32 i;

	if (fl->t == O_NULO)
		return 1;
	if (fl->t == O_NOMBRE)
		fl = NULL;
	for (i = 0; fl ? i < fl->n : i < 1; i++) {
		pobj *x = fl ? resuelve(d, fl->u.v[i]) : resuelve(d, o_dic_clave(f, "Filter"));

		if (!(o_es_nombre(x, "FlateDecode") || o_es_nombre(x, "Fl") || o_es_nombre(x, "LZWDecode") || o_es_nombre(x, "LZW") ||
		      o_es_nombre(x, "ASCIIHexDecode") || o_es_nombre(x, "AHx") || o_es_nombre(x, "ASCII85Decode") ||
		      o_es_nombre(x, "A85") || o_es_nombre(x, "RunLengthDecode") || o_es_nombre(x, "RL")))
			return 0;
	}
	return 1;
}

static int es_solo_dct(pdoc *d, pobj *f)
{
	pobj *fl = resuelve(d, o_dic_clave(f, "Filter"));

	if (fl->t == O_ARREGLO && fl->n == 1)
		fl = resuelve(d, fl->u.v[0]);
	return o_es_nombre(fl, "DCTDecode") || o_es_nombre(fl, "DCT");
}

u8 *jpeg_menor(const u8 *d, u32 n, int calidad, int lado, int *an, int *al, u32 *sal);

/* los datos que van al archivo para el flujo f (con el diccionario d2, que
 * se puede cambiar); de pide */
static u8 *datos_flujo(pctx *c, pobj *f, pdoc *de, pobj *d2, u32 *n)
{
	u8 *crudo;
	popciones *op = c->op;

	if (!de || !f->u.f.datos) {
		crudo = pide(f->u.f.largo + 1);
		if (f->u.f.largo)
			memcpy(crudo, f->u.f.datos, f->u.f.largo);
		*n = f->u.f.largo;
	} else {
		crudo = flujo_crudo(de, f, n);
		if (!crudo) {
			*n = 0;
			return pide(1);
		}
	}
	if (op && op->calidad_img && de && o_es_nombre(o_dic_clave(d2, "Subtype"), "Image") && es_solo_dct(de, f) &&
	    *n > 20000 && !o_dic_clave(d2, "Decode")) {
		/* una foto JPEG: más pequeña y con menos calidad, si así ocupa menos */
		pobj *cs = resuelve(de, o_dic_clave(d2, "ColorSpace"));
		int an, al;
		u32 m;
		u8 *j;

		if (o_es_nombre(cs, "DeviceRGB") || o_es_nombre(cs, "DeviceGray") ||
		    (cs->t == O_ARREGLO && cs->n == 2 && o_es_nombre(resuelve(de, cs->u.v[0]), "ICCBased"))) {
			j = jpeg_menor(crudo, *n, op->calidad_img, op->lado_img, &an, &al, &m);
			if (j && m < *n * 9 / 10) {
				so_suelta(crudo);
				o_pon(c->s->d, d2, "Width", o_ent(c->s->d, an));
				o_pon(c->s->d, d2, "Height", o_ent(c->s->d, al));
				o_pon(c->s->d, d2, "DecodeParms", NULL);
				*n = m;
				return j;
			}
			so_suelta(j);
		}
	}
	if (op && op->comprime && (!de || filtros_simples(de, f))) {
		u32 m, k;
		u8 *plano, *z;

		if (de) {
			plano = flujo_datos(de, f, &m, 0, NULL);
		} else {
			plano = NULL;
			m = 0;
		}
		if (!de && !o_dic_clave(d2, "Filter")) {
			plano = pide(*n + 1);
			memcpy(plano, crudo, *n);
			m = *n;
		}
		if (plano) {
			z = flate_cod(plano, m, &k, 9);
			so_suelta(plano);
			if (k + 20 < *n) {
				so_suelta(crudo);
				o_pon(c->s->d, d2, "Filter", o_nombre(c->s->d, "FlateDecode"));
				o_pon(c->s->d, d2, "DecodeParms", NULL);
				o_pon(c->s->d, d2, "DL", NULL);
				*n = k;
				return z;
			}
			so_suelta(z);
		}
	}
	return crudo;
}

static void sale_flujo(pctx *c, u32 num, pobj *f, pdoc *de)
{
	pobj *d2 = o_dicc(c->s->d), *dic = o_dic_de(f);
	u32 n, i;
	u8 *datos;

	for (i = 0; dic && i < dic->n; i++)
		o_pon(c->s->d, d2, (const char *)dic->u.v[2 * i]->u.s, dic->u.v[2 * i + 1]);
	datos = datos_flujo(c, f, de, d2, &n);
	if (c->cifra && !o_es_nombre(o_dic_clave(d2, "Type"), "XRef")) {
		u32 m;
		u8 *x = cifra(&c->cif, num, 0, datos, n, &m);

		so_suelta(datos);
		datos = x;
		n = m;
	}
	o_pon(c->s->d, d2, "Length", o_ent(c->s->d, (i32)n));
	sale_valor(c, d2, num, 0);
	sale_t(&c->e, "\nstream\n");
	sale(&c->e, datos, n);
	sale_t(&c->e, "\nendstream");
	so_suelta(datos);
}

static void sale_obj(pctx *c, pobj *o, u32 num, int hondo)
{
	(void)hondo;
	salef(&c->e, "%u 0 obj\n", num);
	if (o && o->t == O_FLUJO)
		sale_flujo(c, num, o, c->s->v[num].de);
	else
		sale_valor(c, o, num, 0);
	sale_t(&c->e, "\nendobj\n");
}

/* un objeto entero a un texto (para los flujos de objetos; sus cadenas
 * no se cifran: va cifrado el flujo entero) */
static u8 *a_texto(pctx *c, pobj *o, u32 *n)
{
	u8 *r;

	c->e.en_memoria = 1;
	c->e.m = NULL;
	c->e.mn = c->e.mcap = 0;
	sale_valor(c, o, 0, 0);
	c->e.en_memoria = 0;
	r = c->e.m ? c->e.m : pide(1);
	*n = c->e.mn;
	c->e.m = NULL;
	return r;
}

/* que las tablas de las posiciones lleguen hasta num (a cero lo nuevo) */
static void asegura(u32 **pos, u32 **ef, u32 **ind, u32 *cap, u32 num)
{
	u32 c = *cap;

	if (num < c)
		return;
	while (c <= num)
		c *= 2;
	*pos = repide(*pos, c * sizeof(u32));
	*ef = repide(*ef, c * sizeof(u32));
	*ind = repide(*ind, c * sizeof(u32));
	memset(*pos + *cap, 0, (c - *cap) * sizeof(u32));
	memset(*ef + *cap, 0, (c - *cap) * sizeof(u32));
	memset(*ind + *cap, 0, (c - *cap) * sizeof(u32));
	*cap = c;
}

static void id_nuevo(pnuevo *s, u8 id[16])
{
	u8 semilla[64];
	u32 t = so_tics();

	so_azar(semilla, 32);
	memcpy(semilla + 32, &t, 4);
	memcpy(semilla + 36, &s->n, 4);
	md5(semilla, 40, id);
}

int escribe_pdf(pnuevo *s, const char *ruta, popciones *op, const char **error)
{
	pctx *c = pide(sizeof(pctx));
	u32 *pos = NULL, *en_flujo = NULL, *indice = NULL;
	u32 i, n_objs, xref, size, cap_pos;
	u8 id[16];
	pobj *idv, *trailer;
	int version = s->version, r = 0;
	popciones nada;

	memset(c, 0, sizeof *c);
	if (!op) {
		memset(&nada, 0, sizeof nada);
		op = &nada;
	}
	c->s = s;
	c->op = op;
	id_nuevo(s, id);
	if (op->cifrado) {
		cifrado_nuevo(s->d, &c->cif, op->cifrado, op->clave_usuario ? op->clave_usuario : "",
			      op->clave_dueno, id, op->permisos ? op->permisos : -4);
		c->num_encrypt = nuevo_obj(s, c->cif.dic, NULL);
		c->cifra = 1;
		if (op->cifrado == 2 && version < 16)
			version = 16;
		if (op->cifrado == 3 && version < 17)
			version = 17;
	}
	if (op->objetos_en_flujo && version < 15)
		version = 15;
	if (version > 17 && version != 20)
		version = 17;
	if (version < 14)
		version = 14;
	c->e.f = so_crea(ruta, error);
	if (!c->e.f) {
		so_suelta(c);
		return -1;
	}
	n_objs = s->n;
	cap_pos = n_objs + 64;
	pos = pide(cap_pos * sizeof(u32));
	en_flujo = pide(cap_pos * sizeof(u32));
	indice = pide(cap_pos * sizeof(u32));
	memset(pos, 0, cap_pos * sizeof(u32));
	memset(en_flujo, 0, cap_pos * sizeof(u32));
	memset(indice, 0, cap_pos * sizeof(u32));
	salef(&c->e, "%%PDF-%u.%u\n%%\xE2\xE3\xCF\xD3\n", (u32)version / 10, (u32)version % 10);
	if (!op->objetos_en_flujo) {
		for (i = 1; i < n_objs; i++) {
			pos[i] = c->e.pos;
			sale_obj(c, s->v[i].o, i, 0);
		}
		size = n_objs;
		xref = c->e.pos;
		salef(&c->e, "xref\n0 %u\n0000000000 65535 f \n", size);
		for (i = 1; i < size; i++)
			salef(&c->e, "%010u 00000 n \n", pos[i]);
		trailer = o_dicc(s->d);
		o_pon(s->d, trailer, "Size", o_ent(s->d, (i32)size));
	} else {
		/* los sueltos (no flujos, ni el /Encrypt), de 100 en 100 en flujos de objetos */
		u32 k = 1;

		while (k < n_objs) {
			u32 lista[100], nl = 0, j, total = 0, num_fs;
			u8 *trozos[100];
			u32 largos[100];
			char cab[2600];
			int nc = 0;
			pobj *fs;
			u8 *todo;

			while (k < n_objs && nl < 100) {
				pobj *o = s->v[k].o;

				if (o && o->t != O_FLUJO && k != c->num_encrypt)
					lista[nl++] = k;
				k++;
			}
			if (!nl)
				continue;
			for (j = 0; j < nl; j++) {
				trozos[j] = a_texto(c, s->v[lista[j]].o, &largos[j]);
				nc += fmt(cab + nc, (int)sizeof cab - nc, "%u %u ", lista[j], total);
				total += largos[j] + 1;
			}
			todo = pide((u32)nc + total + 1);
			memcpy(todo, cab, (size_t)nc);
			total = (u32)nc;
			for (j = 0; j < nl; j++) {
				memcpy(todo + total, trozos[j], largos[j]);
				total += largos[j];
				todo[total++] = '\n';
				so_suelta(trozos[j]);
			}
			fs = o_nuevo(s->d, O_FLUJO);
			fs->u.f.dic = o_dicc(s->d);
			o_pon(s->d, fs->u.f.dic, "Type", o_nombre(s->d, "ObjStm"));
			o_pon(s->d, fs->u.f.dic, "N", o_ent(s->d, (i32)nl));
			o_pon(s->d, fs->u.f.dic, "First", o_ent(s->d, nc));
			fs->u.f.datos = todo;
			fs->u.f.largo = total;
			fs->propio = 1;
			num_fs = nuevo_obj(s, fs, NULL);
			asegura(&pos, &en_flujo, &indice, &cap_pos, num_fs);
			for (j = 0; j < nl; j++) {
				en_flujo[lista[j]] = num_fs;
				indice[lista[j]] = j;
			}
			/* (y se escribe ya, comprimido) */
			{
				popciones op2 = *op;
				popciones *antes = c->op;

				op2.comprime = 1;
				op2.calidad_img = 0;
				c->op = &op2;
				pos[num_fs] = c->e.pos;
				sale_obj(c, fs, num_fs, 0);
				c->op = antes;
			}
			so_suelta(todo);
			fs->u.f.datos = NULL;
		}
		/* los flujos y el /Encrypt, sueltos */
		for (i = 1; i < n_objs; i++) {
			pobj *o = s->v[i].o;

			if (o && (o->t == O_FLUJO || i == c->num_encrypt)) {
				pos[i] = c->e.pos;
				sale_obj(c, o, i, 0);
			}
		}
		/* la tabla, en un flujo: W [1 4 2] */
		size = s->n + 1;
		{
			u32 num_x = s->n;
			u8 *t = pide(size * 7);
			pobj *fx = o_nuevo(s->d, O_FLUJO), *w;

			asegura(&pos, &en_flujo, &indice, &cap_pos, num_x + 1);
			xref = c->e.pos;
			for (i = 0; i < size; i++) {
				u8 *p = t + 7 * i;
				u32 v1, v2;
				u8 tipo;

				if (i == 0) {
					tipo = 0; v1 = 0; v2 = 65535;
				} else if (i == num_x) {
					tipo = 1; v1 = xref; v2 = 0;
				} else if (i < n_objs && en_flujo[i]) {
					tipo = 2; v1 = en_flujo[i]; v2 = indice[i];
				} else if (i < num_x && pos[i]) {
					tipo = 1; v1 = pos[i]; v2 = 0;
				} else {
					tipo = 0; v1 = 0; v2 = 0;
				}
				p[0] = tipo;
				p[1] = (u8)(v1 >> 24); p[2] = (u8)(v1 >> 16); p[3] = (u8)(v1 >> 8); p[4] = (u8)v1;
				p[5] = (u8)(v2 >> 8); p[6] = (u8)v2;
			}
			fx->u.f.dic = o_dicc(s->d);
			trailer = fx->u.f.dic;
			o_pon(s->d, trailer, "Type", o_nombre(s->d, "XRef"));
			o_pon(s->d, trailer, "Size", o_ent(s->d, (i32)size));
			w = o_arreglo(s->d);
			o_mete(s->d, w, o_ent(s->d, 1));
			o_mete(s->d, w, o_ent(s->d, 4));
			o_mete(s->d, w, o_ent(s->d, 2));
			o_pon(s->d, trailer, "W", w);
			fx->u.f.datos = t;
			fx->u.f.largo = size * 7;
			fx->propio = 1;
			nuevo_obj(s, fx, NULL);
			/* (el trailer va en el diccionario del flujo: se completa abajo) */
			o_pon(s->d, trailer, "Root", o_ref(s->d, s->raiz, 0));
			if (s->info)
				o_pon(s->d, trailer, "Info", o_ref(s->d, s->info, 0));
			idv = o_arreglo(s->d);
			o_mete(s->d, idv, o_cadena(s->d, id, 16));
			o_mete(s->d, idv, o_cadena(s->d, id, 16));
			idv->u.v[0]->hex = idv->u.v[1]->hex = 1;
			o_pon(s->d, trailer, "ID", idv);
			if (c->cifra)
				o_pon(s->d, trailer, "Encrypt", o_ref(s->d, c->num_encrypt, 0));
			{
				popciones op2 = *op;
				popciones *antes = c->op;
				int cif = c->cifra;

				op2.comprime = 1;
				op2.calidad_img = 0;
				c->op = &op2;
				c->cifra = 0;	/* (la tabla no se cifra; sus cadenas, tampoco) */
				sale_obj(c, fx, num_x, 0);
				c->cifra = cif;
				c->op = antes;
			}
			so_suelta(t);
			fx->u.f.datos = NULL;
			salef(&c->e, "startxref\n%u\n%%%%EOF\n", xref);
			goto cierra;
		}
	}
	o_pon(s->d, trailer, "Root", o_ref(s->d, s->raiz, 0));
	if (s->info)
		o_pon(s->d, trailer, "Info", o_ref(s->d, s->info, 0));
	idv = o_arreglo(s->d);
	o_mete(s->d, idv, o_cadena(s->d, id, 16));
	o_mete(s->d, idv, o_cadena(s->d, id, 16));
	idv->u.v[0]->hex = idv->u.v[1]->hex = 1;
	o_pon(s->d, trailer, "ID", idv);
	if (c->cifra)
		o_pon(s->d, trailer, "Encrypt", o_ref(s->d, c->num_encrypt, 0));
	sale_t(&c->e, "trailer\n");
	{
		int cif = c->cifra;

		c->cifra = 0;
		sale_valor(c, trailer, 0, 0);
		c->cifra = cif;
	}
	salef(&c->e, "\nstartxref\n%u\n%%%%EOF\n", xref);
cierra:
	if (c->e.n && so_escribe(c->e.f, c->e.b, c->e.n))
		c->e.mal = 1;
	if (so_cierra(c->e.f))
		c->e.mal = 1;
	if (c->e.mal) {
		if (error)
			*error = "no se ha podido escribir (¿el disco, lleno?)";
		r = -1;
	}
	so_suelta(pos);
	so_suelta(en_flujo);
	so_suelta(indice);
	so_suelta(c);
	return r;
}
