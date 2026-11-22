/* xml.c - la oficina de 32 bits: el XML (oficina/comun/xml.h).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "xml.h"

/* la memoria del árbol: trozos de 64 K, de usar y tirar */
typedef struct trozo_xml {
	struct trozo_xml *sig;
	u32 usado, cabe;
} trozo_xml;

struct xml_doc {
	trozo_xml *trozos;
	xml_nodo *raiz;
	u32 nodos;
	int mal;
};

static void *mem(xml_doc *x, u32 n)
{
	trozo_xml *t = x->trozos;
	u8 *r;

	n = (n + 7) & ~7u;
	if (!t || t->usado + n > t->cabe) {
		u32 c = n > 65536 - 32 ? n : 65536 - 32;

		t = so_pide(sizeof(trozo_xml) + c);
		if (!t) {
			x->mal = 1;
			return NULL;
		}
		t->usado = 0;
		t->cabe = c;
		t->sig = x->trozos;
		x->trozos = t;
	}
	r = (u8 *)(t + 1) + t->usado;
	t->usado += n;
	return r;
}

void xml_suelta(xml_doc *x)
{
	if (!x)
		return;
	while (x->trozos) {
		trozo_xml *s = x->trozos->sig;

		so_suelta(x->trozos);
		x->trozos = s;
	}
	so_suelta(x);
}

static int es_blanco(char c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static int letra_nombre(char c)
{
	return !es_blanco(c) && c != '>' && c != '/' && c != '=' && c != '<' && c != '"' && c != '\'' && c != 0;
}

/* copia s[0..n) quitando las entidades; da el texto (acabado en 0) y su largo */
static char *sin_entidades(xml_doc *x, const char *s, u32 n, u32 *largo_)
{
	char *d = mem(x, n + 1);
	u32 i = 0, k = 0;

	if (!d)
		return NULL;
	while (i < n) {
		if (s[i] == '&') {
			u32 j = i + 1, c = 0;
			int bien = 1;

			while (j < n && j < i + 12 && s[j] != ';')
				j++;
			if (j >= n || s[j] != ';')
				bien = 0;
			else if (j - i == 4 && !memcmp(s + i + 1, "amp", 3))
				c = '&';
			else if (j - i == 3 && !memcmp(s + i + 1, "lt", 2))
				c = '<';
			else if (j - i == 3 && !memcmp(s + i + 1, "gt", 2))
				c = '>';
			else if (j - i == 5 && !memcmp(s + i + 1, "quot", 4))
				c = '"';
			else if (j - i == 5 && !memcmp(s + i + 1, "apos", 4))
				c = '\'';
			else if (s[i + 1] == '#') {
				u32 q = i + 2, v = 0, base = 10;

				if (q < j && (s[q] == 'x' || s[q] == 'X')) {
					base = 16;
					q++;
				}
				if (q >= j)
					bien = 0;
				for (; q < j && bien; q++) {
					char h = s[q];
					u32 dg = h >= '0' && h <= '9' ? (u32)(h - '0') : base == 16 && h >= 'a' && h <= 'f' ? (u32)(h - 'a' + 10) :
						 base == 16 && h >= 'A' && h <= 'F' ? (u32)(h - 'A' + 10) : 99;

					if (dg >= base || v > 0x10FFFF)
						bien = 0;
					else
						v = v * base + dg;
				}
				if (!v || v > 0x10FFFF || (v >= 0xD800 && v < 0xE000))
					v = 0xFFFD;
				c = v;
			} else
				bien = 0;
			if (bien) {
				/* (lo que ocupa en UTF-8 nunca pasa de lo que ocupaba la entidad) */
				k += (u32)utf8_pon(c, d + k);
				i = j + 1;
				continue;
			}
		}
		d[k++] = s[i++];
	}
	d[k] = 0;
	*largo_ = k;
	return d;
}

static xml_nodo *nodo(xml_doc *x, xml_nodo *padre, xml_nodo **ultimo)
{
	xml_nodo *n;

	if (x->nodos >= XML_MAX_NODOS) {
		x->mal = 1;
		return NULL;
	}
	n = mem(x, sizeof *n);
	if (!n)
		return NULL;
	memset(n, 0, sizeof *n);
	n->padre = padre;
	if (*ultimo)
		(*ultimo)->sig = n;
	else if (padre)
		padre->hijo = n;
	*ultimo = n;
	x->nodos++;
	return n;
}

static const char *busca_txt(const char *s, const char *fin, const char *t)
{
	u32 m = largo(t);

	for (; s + m <= fin; s++)
		if (!memcmp(s, t, m))
			return s;
	return NULL;
}

xml_doc *xml_lee(const char *d, u32 n)
{
	xml_doc *x = so_pide(sizeof *x);
	const char *s = d, *fin = d + n;
	xml_nodo *padre = NULL, *ultimo = NULL, *pila_ult[XML_HONDO + 1];
	int hondo = 0, saltando = 0;	/* (más hondo que el tope: se salta) */

	if (!x)
		return NULL;
	memset(x, 0, sizeof *x);
	/* (la marca de UTF-8 del principio) */
	if (n >= 3 && (u8)s[0] == 0xEF && (u8)s[1] == 0xBB && (u8)s[2] == 0xBF)
		s += 3;
	while (s < fin && !x->mal) {
		if (*s != '<') {
			const char *q = s;

			while (q < fin && *q != '<')
				q++;
			if (padre && !saltando) {
				xml_nodo *t = nodo(x, padre, &ultimo);

				if (t)
					t->texto = sin_entidades(x, s, (u32)(q - s), &t->largo);
			}
			s = q;
			continue;
		}
		if (s + 1 >= fin)
			break;
		if (s[1] == '?') {
			const char *q = busca_txt(s, fin, "?>");

			s = q ? q + 2 : fin;
			continue;
		}
		if (s[1] == '!') {
			if (fin - s >= 4 && !memcmp(s, "<!--", 4)) {
				const char *q = busca_txt(s + 4, fin, "-->");

				s = q ? q + 3 : fin;
			} else if (fin - s >= 9 && !memcmp(s, "<![CDATA[", 9)) {
				const char *q = busca_txt(s + 9, fin, "]]>");

				if (!q)
					q = fin;
				if (padre && !saltando) {
					xml_nodo *t = nodo(x, padre, &ultimo);

					if (t) {
						char *c = mem(x, (u32)(q - s - 9) + 1);

						if (c) {
							memcpy(c, s + 9, (size_t)(q - s - 9));
							c[q - s - 9] = 0;
							t->texto = c;
							t->largo = (u32)(q - s - 9);
						}
					}
				}
				s = q < fin ? q + 3 : fin;
			} else {
				/* <!DOCTYPE ...> (con su [ ... ]): se salta entera */
				int corchetes = 0;

				for (s += 2; s < fin; s++) {
					if (*s == '[')
						corchetes++;
					else if (*s == ']' && corchetes)
						corchetes--;
					else if (*s == '>' && !corchetes)
						break;
				}
				if (s < fin)
					s++;
			}
			continue;
		}
		if (s[1] == '/') {
			/* el cierre: se vuelve al padre */
			const char *q = s + 2;

			while (q < fin && *q != '>')
				q++;
			s = q < fin ? q + 1 : fin;
			if (saltando) {
				saltando--;
				continue;
			}
			if (padre) {
				ultimo = pila_ult[hondo];
				padre = padre->padre;
				hondo--;
				if (!padre && !hondo) {
					/* (fuera del elemento de arriba: lo que siga, nada) */
				}
			}
			continue;
		}
		/* una etiqueta que se abre */
		{
			const char *q = s + 1, *nom0, *nom1;
			const char *atr_t[256];
			u32 atr_l[256], na = 0;
			const char *val_t[128];
			u32 val_l[128];
			int vacio = 0;
			xml_nodo *e = NULL;

			nom0 = q;
			while (q < fin && letra_nombre(*q))
				q++;
			nom1 = q;
			/* los atributos */
			for (;;) {
				while (q < fin && es_blanco(*q))
					q++;
				if (q >= fin)
					break;
				if (*q == '>') {
					q++;
					break;
				}
				if (*q == '/') {
					vacio = 1;
					q++;
					continue;
				}
				{
					const char *a0 = q, *a1, *v0 = NULL, *v1 = NULL;

					while (q < fin && letra_nombre(*q))
						q++;
					a1 = q;
					if (a1 == a0) {
						q++;	/* (algo que no vale: se salta) */
						continue;
					}
					while (q < fin && es_blanco(*q))
						q++;
					if (q < fin && *q == '=') {
						q++;
						while (q < fin && es_blanco(*q))
							q++;
						if (q < fin && (*q == '"' || *q == '\'')) {
							char cc = *q++;

							v0 = q;
							while (q < fin && *q != cc)
								q++;
							v1 = q;
							if (q < fin)
								q++;
						} else {
							v0 = q;
							while (q < fin && letra_nombre(*q))
								q++;
							v1 = q;
						}
					}
					if (na < 128) {
						atr_t[na] = a0;
						atr_l[na] = (u32)(a1 - a0);
						val_t[na] = v0;
						val_l[na] = v0 ? (u32)(v1 - v0) : 0;
						na++;
					}
				}
			}
			s = q;
			if (nom1 == nom0)
				continue;
			if (saltando || hondo >= XML_HONDO || (!padre && x->raiz)) {
				if (!vacio && (saltando || hondo >= XML_HONDO))
					saltando++;
				continue;
			}
			e = nodo(x, padre, &ultimo);
			if (!e)
				break;
			{
				char *nm = mem(x, (u32)(nom1 - nom0) + 1);

				if (!nm)
					break;
				memcpy(nm, nom0, (size_t)(nom1 - nom0));
				nm[nom1 - nom0] = 0;
				e->nombre = nm;
			}
			if (na) {
				u32 k;

				e->atr = mem(x, na * 2 * sizeof(char *));
				if (!e->atr)
					break;
				for (k = 0; k < na; k++) {
					char *an = mem(x, atr_l[k] + 1);
					u32 l;

					if (!an)
						break;
					memcpy(an, atr_t[k], atr_l[k]);
					an[atr_l[k]] = 0;
					e->atr[2 * k] = an;
					e->atr[2 * k + 1] = val_t[k] ? sin_entidades(x, val_t[k], val_l[k], &l) : "";
					if (!e->atr[2 * k + 1])
						break;
				}
				e->n_atr = na;
			}
			if (!x->raiz)
				x->raiz = e;
			if (!vacio) {
				pila_ult[++hondo] = e;
				padre = e;
				ultimo = NULL;
			}
			(void)atr_t;
		}
	}
	if (!x->raiz || x->mal) {
		xml_suelta(x);
		return NULL;
	}
	return x;
}

xml_nodo *xml_raiz(xml_doc *x)
{
	return x ? x->raiz : NULL;
}

const char *xml_local(const char *nombre)
{
	const char *p = nombre;

	if (!nombre)
		return "";
	for (; *nombre; nombre++)
		if (*nombre == ':')
			p = nombre + 1;
	return p;
}

int xml_es(const xml_nodo *n, const char *nombre)
{
	if (!n || !n->nombre)
		return 0;
	if (strchr(nombre, ':'))
		return iguales(n->nombre, nombre);
	return iguales(xml_local(n->nombre), nombre);
}

xml_nodo *xml_primero(const xml_nodo *n)
{
	xml_nodo *h;

	if (!n)
		return NULL;
	for (h = n->hijo; h; h = h->sig)
		if (h->nombre)
			return h;
	return NULL;
}

xml_nodo *xml_hijo(const xml_nodo *n, const char *nombre)
{
	xml_nodo *h;

	if (!n)
		return NULL;
	for (h = n->hijo; h; h = h->sig)
		if (xml_es(h, nombre))
			return h;
	return NULL;
}

xml_nodo *xml_sig(const xml_nodo *n, const char *nombre)
{
	xml_nodo *h;

	if (!n)
		return NULL;
	for (h = n->sig; h; h = h->sig)
		if (h->nombre && (!nombre || xml_es(h, nombre)))
			return h;
	return NULL;
}

xml_nodo *xml_camino(const xml_nodo *n, const char *camino)
{
	char t[64];

	while (n && *camino) {
		u32 k = 0;

		while (*camino && *camino != '/') {
			if (k + 1 < sizeof t)
				t[k++] = *camino;
			camino++;
		}
		t[k] = 0;
		if (*camino == '/')
			camino++;
		n = xml_hijo(n, t);
	}
	return (xml_nodo *)n;
}

const char *xml_atr(const xml_nodo *n, const char *nombre)
{
	u32 k;
	int con = strchr(nombre, ':') != NULL;

	if (!n)
		return NULL;
	for (k = 0; k < n->n_atr; k++)
		if (con ? iguales(n->atr[2 * k], nombre) : iguales(xml_local(n->atr[2 * k]), nombre))
			return n->atr[2 * k + 1];
	return NULL;
}

void xml_texto(const xml_nodo *n, bufer *b)
{
	const xml_nodo *h;
	int hondo = 0;

	if (!n)
		return;
	/* (sin recursión: bajando y subiendo por los punteros) */
	h = n->hijo;
	while (h) {
		if (!h->nombre)
			buf_pon(b, h->texto, h->largo);
		if (h->hijo && hondo < XML_HONDO) {
			h = h->hijo;
			hondo++;
			continue;
		}
		while (h && !h->sig && h != n) {
			h = h->padre;
			hondo--;
			if (h == n)
				return;
		}
		if (!h || h == n)
			return;
		h = h->sig;
	}
}

void xml_pon_atr(bufer *b, const char *nombre, const char *valor)
{
	buf_c(b, ' ');
	buf_txt(b, nombre);
	buf_txt(b, "=\"");
	buf_xml(b, valor, largo(valor));
	buf_c(b, '"');
}

void xml_pon_atr_num(bufer *b, const char *nombre, long v)
{
	buf_fmt(b, " %s=\"%d\"", nombre, (int)v);
}

void xml_pon_elem(bufer *b, const char *nombre, const char *texto)
{
	buf_fmt(b, "<%s>", nombre);
	buf_xml(b, texto, largo(texto));
	buf_fmt(b, "</%s>", nombre);
}
