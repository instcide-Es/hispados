/* html.c - Correo: de HTML (y de texto) a un documento con formato (html.h).
 * Un tokenizador tolerante como el de HTML5, sin árbol: una pila de
 * HTML_MAX_ANIDADO etiquetas con el estilo y el párrafo de cada una. Sin
 * JavaScript: <script>, <style>, los on...= y los javascript: se tiran (y se
 * cuentan); las imágenes de fuera no se cargan (se apuntan, y los píxeles de
 * rastreo se señalan); los enlaces cuyo texto dice un sitio y llevan a otro
 * se marcan como engañosos. Lo que llega es hostil: topes y tiempo lineal.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include <stdio.h>
#include "html.h"
#include "mime.h"
#include "interno.h"

#define MAX_TRAMOS (1u << 20)
#define MAX_PARRAFOS (1u << 18)
#define MAX_ENLACES 4096
#define MAX_IMAGENES 4096
#define MAX_ATRIBUTOS 24
#define MAX_VISIBLE 1024

/* ---- las etiquetas que se miran ---- */
enum {
	T_NADA, T_A, T_B, T_I, T_U, T_S, T_H1, T_H2, T_H3, T_H4, T_H5, T_H6, T_P, T_BR, T_DIV, T_CITA, T_UL, T_OL,
	T_LI, T_PRE, T_MONO, T_HR, T_TABLE, T_TR, T_TD, T_TH, T_IMG, T_CRUDO, T_BASE, T_PEQUENO, T_CENTER, T_DL, T_DT,
	T_DD, T_BLOQUE, T_SECCION_TABLA, T_SPAN, T_OTRA
};

static const struct {
	const char *n;
	u8 t;
} ETIQ[] = {
	{"a", T_A}, {"abbr", T_SPAN}, {"address", T_BLOQUE}, {"article", T_BLOQUE}, {"aside", T_BLOQUE},
	{"b", T_B}, {"base", T_BASE}, {"big", T_SPAN}, {"blockquote", T_CITA}, {"body", T_OTRA}, {"br", T_BR},
	{"caption", T_BLOQUE}, {"center", T_CENTER}, {"cite", T_I}, {"code", T_MONO}, {"dd", T_DD}, {"del", T_S},
	{"details", T_BLOQUE}, {"dfn", T_I}, {"dir", T_UL}, {"div", T_DIV}, {"dl", T_DL}, {"dt", T_DT}, {"em", T_I},
	{"fieldset", T_BLOQUE}, {"figcaption", T_BLOQUE}, {"figure", T_BLOQUE}, {"font", T_SPAN}, {"footer", T_BLOQUE},
	{"form", T_BLOQUE}, {"h1", T_H1}, {"h2", T_H2}, {"h3", T_H3}, {"h4", T_H4}, {"h5", T_H5}, {"h6", T_H6},
	{"head", T_OTRA}, {"header", T_BLOQUE}, {"hr", T_HR}, {"html", T_OTRA}, {"i", T_I}, {"img", T_IMG},
	{"ins", T_U}, {"kbd", T_MONO}, {"li", T_LI}, {"listing", T_PRE}, {"main", T_BLOQUE}, {"mark", T_SPAN},
	{"menu", T_UL}, {"nav", T_BLOQUE}, {"ol", T_OL}, {"p", T_P}, {"pre", T_PRE}, {"q", T_SPAN}, {"s", T_S},
	{"samp", T_MONO}, {"section", T_BLOQUE}, {"small", T_PEQUENO}, {"span", T_SPAN}, {"strike", T_S},
	{"strong", T_B}, {"sub", T_PEQUENO}, {"summary", T_BLOQUE}, {"sup", T_PEQUENO}, {"table", T_TABLE},
	{"tbody", T_SECCION_TABLA}, {"td", T_TD}, {"tfoot", T_SECCION_TABLA}, {"th", T_TH},
	{"thead", T_SECCION_TABLA}, {"tr", T_TR}, {"tt", T_MONO}, {"u", T_U}, {"ul", T_UL}, {"var", T_I},
	{"xmp", T_PRE},
};

/* lo que se salta hasta su cierre (no se enseña) */
static const char *const CRUDOS[] = {"script", "style", "title", "template", "iframe", "object", "applet",
				     "embed", "svg", "math", "frameset", "noembed", "textarea", "select"};

static u8 etiqueta(const char *n)
{
	int a = 0, z = (int)(sizeof ETIQ / sizeof ETIQ[0]) - 1;

	while (a <= z) {
		int m = (a + z) / 2, c = strcmp(n, ETIQ[m].n);

		if (!c)
			return ETIQ[m].t;
		if (c < 0)
			z = m - 1;
		else
			a = m + 1;
	}
	return T_NADA;
}

/* ---- las entidades con nombre (por orden; «l»: vale sin «;») ---- */
static const struct {
	const char *n;
	u16 u;
	u8 l;
} ENT[] = {
	{"AElig", 0xC6, 1}, {"Aacute", 0xC1, 1}, {"Acirc", 0xC2, 1}, {"Agrave", 0xC0, 1}, {"Alpha", 0x391, 0},
	{"Aring", 0xC5, 1}, {"Atilde", 0xC3, 1}, {"Auml", 0xC4, 1}, {"Beta", 0x392, 0}, {"Ccedil", 0xC7, 1},
	{"Dagger", 0x2021, 0}, {"Delta", 0x394, 0}, {"ETH", 0xD0, 1}, {"Eacute", 0xC9, 1}, {"Ecirc", 0xCA, 1},
	{"Egrave", 0xC8, 1}, {"Euml", 0xCB, 1}, {"Gamma", 0x393, 0}, {"Iacute", 0xCD, 1}, {"Icirc", 0xCE, 1},
	{"Igrave", 0xCC, 1}, {"Iuml", 0xCF, 1}, {"Lambda", 0x39B, 0}, {"Ntilde", 0xD1, 1}, {"OElig", 0x152, 0},
	{"Oacute", 0xD3, 1}, {"Ocirc", 0xD4, 1}, {"Ograve", 0xD2, 1}, {"Omega", 0x3A9, 0}, {"Oslash", 0xD8, 1},
	{"Otilde", 0xD5, 1}, {"Ouml", 0xD6, 1}, {"Phi", 0x3A6, 0}, {"Pi", 0x3A0, 0}, {"Prime", 0x2033, 0},
	{"Scaron", 0x160, 0}, {"Sigma", 0x3A3, 0}, {"THORN", 0xDE, 1}, {"Theta", 0x398, 0}, {"Uacute", 0xDA, 1},
	{"Ucirc", 0xDB, 1}, {"Ugrave", 0xD9, 1}, {"Uuml", 0xDC, 1}, {"Yacute", 0xDD, 1}, {"Yuml", 0x178, 0},
	{"aacute", 0xE1, 1}, {"acirc", 0xE2, 1}, {"acute", 0xB4, 1}, {"aelig", 0xE6, 1}, {"agrave", 0xE0, 1},
	{"alpha", 0x3B1, 0}, {"amp", '&', 1}, {"apos", '\'', 0}, {"aring", 0xE5, 1}, {"asymp", 0x2248, 0},
	{"atilde", 0xE3, 1}, {"auml", 0xE4, 1}, {"bdquo", 0x201E, 0}, {"beta", 0x3B2, 0}, {"brvbar", 0xA6, 1},
	{"bull", 0x2022, 0}, {"ccedil", 0xE7, 1}, {"cedil", 0xB8, 1}, {"cent", 0xA2, 1}, {"check", 0x2713, 0},
	{"circ", 0x2C6, 0}, {"clubs", 0x2663, 0}, {"copy", 0xA9, 1}, {"curren", 0xA4, 1}, {"dagger", 0x2020, 0},
	{"darr", 0x2193, 0}, {"deg", 0xB0, 1}, {"delta", 0x3B4, 0}, {"diams", 0x2666, 0}, {"divide", 0xF7, 1},
	{"eacute", 0xE9, 1}, {"ecirc", 0xEA, 1}, {"egrave", 0xE8, 1}, {"emsp", 0x2003, 0}, {"ensp", 0x2002, 0},
	{"epsilon", 0x3B5, 0}, {"equiv", 0x2261, 0}, {"eth", 0xF0, 1}, {"euml", 0xEB, 1}, {"euro", 0x20AC, 0},
	{"frac12", 0xBD, 1}, {"frac14", 0xBC, 1}, {"frac34", 0xBE, 1}, {"frasl", 0x2044, 0}, {"gamma", 0x3B3, 0},
	{"ge", 0x2265, 0}, {"gt", '>', 1}, {"harr", 0x2194, 0}, {"hearts", 0x2665, 0}, {"hellip", 0x2026, 0},
	{"iacute", 0xED, 1}, {"icirc", 0xEE, 1}, {"iexcl", 0xA1, 1}, {"igrave", 0xEC, 1}, {"infin", 0x221E, 0},
	{"iquest", 0xBF, 1}, {"iuml", 0xEF, 1}, {"lambda", 0x3BB, 0}, {"laquo", 0xAB, 1}, {"larr", 0x2190, 0},
	{"ldquo", 0x201C, 0}, {"le", 0x2264, 0}, {"lrm", 0x200E, 0}, {"lsaquo", 0x2039, 0}, {"lsquo", 0x2018, 0},
	{"lt", '<', 1}, {"macr", 0xAF, 1}, {"mdash", 0x2014, 0}, {"micro", 0xB5, 1}, {"middot", 0xB7, 1},
	{"minus", 0x2212, 0}, {"mu", 0x3BC, 0}, {"nbsp", 0xA0, 1}, {"ndash", 0x2013, 0}, {"ne", 0x2260, 0},
	{"not", 0xAC, 1}, {"ntilde", 0xF1, 1}, {"oacute", 0xF3, 1}, {"ocirc", 0xF4, 1}, {"oelig", 0x153, 0},
	{"ograve", 0xF2, 1}, {"oline", 0x203E, 0}, {"omega", 0x3C9, 0}, {"ordf", 0xAA, 1}, {"ordm", 0xBA, 1},
	{"oslash", 0xF8, 1}, {"otilde", 0xF5, 1}, {"ouml", 0xF6, 1}, {"para", 0xB6, 1}, {"permil", 0x2030, 0},
	{"phi", 0x3C6, 0}, {"pi", 0x3C0, 0}, {"plusmn", 0xB1, 1}, {"pound", 0xA3, 1}, {"prime", 0x2032, 0},
	{"quot", '"', 1}, {"raquo", 0xBB, 1}, {"rarr", 0x2192, 0}, {"rdquo", 0x201D, 0}, {"reg", 0xAE, 1},
	{"rlm", 0x200F, 0}, {"rsaquo", 0x203A, 0}, {"rsquo", 0x2019, 0}, {"sbquo", 0x201A, 0},
	{"scaron", 0x161, 0}, {"sect", 0xA7, 1}, {"shy", 0xAD, 1}, {"sigma", 0x3C3, 0}, {"spades", 0x2660, 0},
	{"sup1", 0xB9, 1}, {"sup2", 0xB2, 1}, {"sup3", 0xB3, 1}, {"szlig", 0xDF, 1}, {"tau", 0x3C4, 0},
	{"theta", 0x3B8, 0}, {"thinsp", 0x2009, 0}, {"thorn", 0xFE, 1}, {"tilde", 0x2DC, 0}, {"times", 0xD7, 1},
	{"trade", 0x2122, 0}, {"uacute", 0xFA, 1}, {"uarr", 0x2191, 0}, {"ucirc", 0xFB, 1}, {"ugrave", 0xF9, 1},
	{"uml", 0xA8, 1}, {"uuml", 0xFC, 1}, {"yacute", 0xFD, 1}, {"yen", 0xA5, 1}, {"yuml", 0xFF, 1},
	{"zwj", 0x200D, 0}, {"zwnj", 0x200C, 0},
};

static int entidad_nombre(const char *n, u32 l, int *legado)
{
	int a = 0, z = (int)(sizeof ENT / sizeof ENT[0]) - 1;
	char b[16];

	if (l >= sizeof b)
		return -1;
	memcpy(b, n, l);
	b[l] = 0;
	while (a <= z) {
		int m = (a + z) / 2, c = strcmp(b, ENT[m].n);

		if (!c) {
			*legado = ENT[m].l;
			return ENT[m].u;
		}
		if (c < 0)
			z = m - 1;
		else
			a = m + 1;
	}
	return -1;
}

/* de 0x80 a 0x9F en &#...; es la 1252 (HTML5) */
static const u16 C1_1252[32] = {
	0x20AC, 0x81, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x8D,
	0x017D, 0x8F, 0x90, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A,
	0x0153, 0x9D, 0x017E, 0x0178};

/* una entidad en s[i] == '&': la letra (o -1 si no lo es) y lo que ocupa */
static i32 entidad(const char *s, u32 n, u32 i, u32 *largo, int en_atributo)
{
	u32 j = i + 1;

	if (j < n && s[j] == '#') {
		u32 v = 0, k;
		int hex = 0, hay = 0;

		j++;
		if (j < n && (s[j] == 'x' || s[j] == 'X')) {
			hex = 1;
			j++;
		}
		for (k = j; k < n; k++) {
			int d = hex ? valor_hex((u8)s[k]) : (es_cifra((u8)s[k]) ? s[k] - '0' : -1);

			if (d < 0)
				break;
			hay = 1;
			if (v <= 0x10FFFF)
				v = v * (hex ? 16 : 10) + (u32)d;
		}
		if (!hay)
			return -1;
		if (k < n && s[k] == ';')
			k++;
		*largo = k - i;
		if (v == 0 || v > 0x10FFFF || (v >= 0xD800 && v <= 0xDFFF))
			return 0xFFFD;
		if (v >= 0x80 && v <= 0x9F)
			return C1_1252[v - 0x80];
		return (i32)v;
	}
	{
		u32 k;
		int legado = 0;
		i32 u;

		for (k = j; k < n && k - j < 12 && es_alnum((u8)s[k]); k++)
			;
		if (k == j)
			return -1;
		if (k < n && s[k] == ';') {
			u = entidad_nombre(s + j, k - j, &legado);
			if (u >= 0) {
				*largo = k + 1 - i;
				return u;
			}
		}
		/* sin «;»: solo las de antes, y en un atributo si no sigue «=» ni letra */
		{
			u32 l;

			for (l = k - j; l >= 2; l--) {
				u = entidad_nombre(s + j, l, &legado);
				if (u >= 0 && legado) {
					if (en_atributo && j + l < n && (s[j + l] == '=' || es_alnum((u8)s[j + l])))
						return -1;
					*largo = l + 1;
					return u;
				}
			}
		}
	}
	return -1;
}

/* un valor de atributo con las entidades, en memoria nueva */
static char *valor_atributo(const char *s, u32 n, u32 tope)
{
	Texto t;
	u32 i = 0;

	tx_ini(&t, tope);
	while (i < n) {
		u8 c = (u8)s[i];

		if (c == '&') {
			u32 l;
			i32 u = entidad(s, n, i, &l, 1);

			if (u >= 0) {
				tx_letra(&t, (u32)u);
				i += l;
				continue;
			}
		}
		if (c == '\t' || c == '\n' || c == '\r')
			c = ' ';
		if (c < 0x20)
			c = ' ';
		tx_c(&t, c);
		i++;
	}
	if (t.malo == 2)
		t.malo = 0;
	return tx_fin(&t, NULL);
}

/* ---- el documento que se va haciendo ---- */
typedef struct {
	u8 tag;
	u16 estilo;
	u16 enlace;
	u8 en_enlace;
	u8 clase, nivel_lista, nivel_cita, alinea;
	u8 pre, oculto, celda;
	u32 contador;	/* (ul/ol) el número que va; 0: viñetas */
	u8 es_ol;
} Marco;

typedef struct {
	Doc *d;
	Texto t;
	u32 cap_tramos, cap_parrafos, cap_enlaces, cap_imagenes;
	Marco ctx;
	Marco pila[HTML_MAX_ANIDADO];
	int np;
	int par_abierto, espacio, tab, primero, pre_inicio;
	u8 hueco;
	u32 enlace_ini;	/* dónde empieza el texto del enlace de ahora */
	char *base;
	int sin_memoria;
	u32 col;	/* la columna en <pre> (para los tabuladores) */
} H;

static int crece(void **v, u32 *cap, u32 quiere, size_t tam)
{
	u32 c;
	void *q;

	if (quiere <= *cap)
		return 1;
	c = *cap ? *cap * 2 : 32;
	while (c < quiere)
		c *= 2;
	q = so_repide(*v, (size_t)c * tam);
	if (!q)
		return 0;
	*v = q;
	*cap = c;
	return 1;
}

static int clase_base(const Marco *c)
{
	return c->nivel_cita ? P_CITA : (c->nivel_lista ? P_LISTA : P_NORMAL);
}

static void cierra_par(H *h)
{
	h->par_abierto = 0;
	h->espacio = 0;
	h->tab = 0;
	h->col = 0;
}

static void pide_hueco(H *h, int k)
{
	if (h->hueco < k)
		h->hueco = (u8)k;
}

static int abre_par(H *h, int clase)
{
	Doc *d = h->d;
	Parrafo *p;

	if (d->n_parrafos >= MAX_PARRAFOS) {
		d->cortado = 1;
		return 0;
	}
	if (!crece((void **)&d->parrafos, &h->cap_parrafos, d->n_parrafos + 1, sizeof(Parrafo))) {
		h->sin_memoria = 1;
		return 0;
	}
	p = &d->parrafos[d->n_parrafos++];
	p->ini = p->fin = d->n_tramos;
	p->clase = (u8)clase;
	p->nivel = clase == P_LISTA ? (u8)(h->ctx.nivel_lista + h->ctx.nivel_cita)
		   : clase == P_CITA ? h->ctx.nivel_cita : (u8)(h->ctx.nivel_cita + h->ctx.nivel_lista);
	p->hueco = h->primero ? 0 : (h->hueco > 2 ? 2 : h->hueco);
	p->alinea = h->ctx.alinea;
	h->primero = 0;
	h->hueco = 0;
	h->par_abierto = 1;
	h->espacio = 0;
	h->tab = 0;
	h->col = 0;
	return 1;
}

/* octetos ya buenos al texto, con su estilo */
static void pon_bytes(H *h, const char *s, u32 n, u16 estilo, u16 enlace, int solo)
{
	Doc *d = h->d;
	Tramo *u;

	if (!n || h->sin_memoria || d->cortado)
		return;
	if (h->t.n + n > HTML_MAX_TEXTO) {
		d->cortado = 1;
		return;
	}
	if (!tx_pon(&h->t, s, n)) {
		if (h->t.malo == 1)
			h->sin_memoria = 1;
		else
			d->cortado = 1;
		return;
	}
	u = d->n_tramos ? &d->tramos[d->n_tramos - 1] : NULL;
	if (!solo && u && u->estilo == estilo && u->enlace == enlace && !(estilo & E_IMAGEN) && u->ini + u->n == h->t.n - n &&
	    d->n_parrafos && d->parrafos[d->n_parrafos - 1].fin == d->n_tramos && d->parrafos[d->n_parrafos - 1].ini < d->n_tramos) {
		u->n += n;
		return;
	}
	if (d->n_tramos >= MAX_TRAMOS) {
		d->cortado = 1;
		return;
	}
	if (!crece((void **)&d->tramos, &h->cap_tramos, d->n_tramos + 1, sizeof(Tramo))) {
		h->sin_memoria = 1;
		return;
	}
	u = &d->tramos[d->n_tramos++];
	u->ini = h->t.n - n;
	u->n = n;
	u->estilo = estilo;
	u->enlace = enlace;
	if (d->n_parrafos)
		d->parrafos[d->n_parrafos - 1].fin = d->n_tramos;
}

static u16 estilo_de(const H *h)
{
	return (u16)(h->ctx.estilo | (h->ctx.en_enlace ? E_ENLACE : 0));
}

/* una letra (o un trozo de octetos sin blancos) del texto, juntando los blancos */
static void pon_trozo(H *h, const char *s, u32 n)
{
	if (h->ctx.oculto)
		return;
	if (!h->par_abierto) {
		if (!abre_par(h, h->ctx.pre ? P_PRE : h->ctx.clase))
			return;
	} else if (h->tab) {
		pon_bytes(h, "\t", 1, estilo_de(h), h->ctx.enlace, 0);
	} else if (h->espacio)
		pon_bytes(h, " ", 1, estilo_de(h), h->ctx.enlace, 0);
	h->tab = h->espacio = 0;
	pon_bytes(h, s, n, estilo_de(h), h->ctx.enlace, 0);
}

static void pon_blanco(H *h, int c)
{
	if (h->ctx.oculto)
		return;
	if (h->ctx.pre) {
		if (h->pre_inicio && c == '\n') {
			h->pre_inicio = 0;
			return;
		}
		h->pre_inicio = 0;
		if (c == '\r')
			return;
		if (c == '\n') {
			if (h->par_abierto)
				cierra_par(h);
			else
				pide_hueco(h, h->hueco + 1);
			return;
		}
		if (c == '\t') {
			u32 k = 8 - h->col % 8;

			while (k--) {
				pon_trozo(h, " ", 1);
				h->col++;
			}
			return;
		}
		pon_trozo(h, " ", 1);
		h->col++;
		return;
	}
	if (h->par_abierto)
		h->espacio = 1;
}

/* el texto entre las etiquetas */
static void pon_texto(H *h, const char *s, u32 n)
{
	u32 i = 0;

	while (i < n && !h->sin_memoria && !h->d->cortado) {
		u8 c = (u8)s[i];

		if (c == '&') {
			u32 l;
			i32 u = entidad(s, n, i, &l, 0);

			if (u >= 0) {
				char b[4];
				int k;

				i += l;
				if (u == 0xA0) {	/* &nbsp;: un blanco que no se junta */
					pon_trozo(h, " ", 1);
					continue;
				}
				if (u == 0xAD || u == 0x200B || u == 0x200C || u == 0x200D || u == 0xFEFF || u == 0x034F || u == 0x200E ||
				    u == 0x200F)
					continue;
				if (u < 0x20 || u == 0x7F) {
					if (es_espacio(u))
						pon_blanco(h, u);
					continue;
				}
				k = utf8_pon((u32)u, b);
				pon_trozo(h, b, (u32)k);
				if (h->ctx.pre)
					h->col++;
				continue;
			}
			/* no es una entidad: el «&» tal cual */
			pon_trozo(h, "&", 1);
			if (h->ctx.pre)
				h->col++;
			i++;
			continue;
		}
		if (es_espacio(c)) {
			pon_blanco(h, c);
			i++;
			continue;
		}
		if (c < 0x20 || c == 0x7F) {
			i++;
			continue;
		}
		h->pre_inicio = 0;
		if (c < 0x80) {
			u32 a = i;

			/* un trozo de ASCII sin blancos ni & de una vez */
			while (i < n && (u8)s[i] > 0x20 && (u8)s[i] < 0x7F && s[i] != '&')
				i++;
			pon_trozo(h, s + a, i - a);
			if (h->ctx.pre)
				h->col += i - a;
			continue;
		}
		{
			u32 a = i, u = utf8_letra((const u8 *)s, n, &i);
			char b[4];
			int k;

			if (u == 0xA0) {
				pon_trozo(h, " ", 1);
				continue;
			}
			if (u == 0xAD || u == 0x200B || u == 0x200C || u == 0x200D || u == 0xFEFF || u == 0x034F)
				continue;
			if (u == 0xFFFD && !(i - a == 3 && (u8)s[a] == 0xEF)) {
				k = utf8_pon(0xFFFD, b);
				pon_trozo(h, b, (u32)k);
			} else
				pon_trozo(h, s + a, i - a);
			if (h->ctx.pre)
				h->col++;
		}
	}
}

/* ---- los dominios ---- */
static const char *const SUFIJOS2[] = {
	"ac.jp", "ac.uk", "appspot.com", "azurewebsites.net", "blogspot.com", "cloudfront.net", "co.il", "co.in",
	"co.jp", "co.kr", "co.nz", "co.uk", "co.za", "com.ar", "com.au", "com.br", "com.cn", "com.co", "com.ec",
	"com.es", "com.mx", "com.pe", "com.pt", "com.tr", "com.tw", "com.uy", "com.ve", "edu.ar", "edu.au", "edu.es",
	"github.io", "gitlab.io", "gob.ar", "gob.es", "gob.mx", "gov.au", "gov.br", "gov.uk", "herokuapp.com",
	"ltd.uk", "me.uk", "ne.jp", "net.au", "net.br", "netlify.app", "nom.es", "or.jp", "org.ar", "org.au", "org.br",
	"org.es", "org.mx", "org.uk", "pages.dev", "plc.uk", "sch.uk", "vercel.app", "web.app", "workers.dev",
};

static int es_ip(const char *h)
{
	int cifras = 0;

	if (h[0] == '[')
		return 1;
	if (h[0] == '0' && (h[1] == 'x' || h[1] == 'X'))
		return 1;
	for (; *h; h++) {
		if (es_cifra((u8)*h))
			cifras++;
		else if (*h != '.')
			return 0;
	}
	return cifras > 0;
}

/* el sitio de una dirección: «https://user@www.banco.es:443/x» -> «www.banco.es»;
 * con «usuario» y «es_ip». 0 si no lleva sitio (relativa, mailto:...) */
static int sitio_de(const char *u, char *host, int cabe, int *usuario, char *usu, int cabe_usu)
{
	const char *p = u, *a, *f, *arroba = NULL;
	int k = 0;

	*usuario = 0;
	host[0] = 0;
	if (usu && cabe_usu > 0)
		usu[0] = 0;
	while (*p && (u8)*p <= 32)
		p++;
	if (p[0] == '/' && p[1] == '/')
		p += 2;
	else {
		const char *q = p;

		while (es_alnum((u8)*q) || *q == '+' || *q == '-' || *q == '.')
			q++;
		if (q == p || q[0] != ':' || q[1] != '/' || q[2] != '/')
			return 0;
		p = q + 3;
	}
	/* (los «\» los toman los navegadores como «/») */
	for (f = p; *f && *f != '/' && *f != '?' && *f != '#' && *f != '\\'; f++)
		if (*f == '@')
			arroba = f;
	a = p;
	if (arroba) {
		*usuario = 1;
		if (usu)
			cr_copia_en(usu, cabe_usu, p, (u32)(arroba - p));
		a = arroba + 1;
	}
	if (*a == '[') {
		for (; a < f && k < cabe - 1; a++) {
			host[k++] = *a;
			if (*a == ']')
				break;
		}
	} else
		for (; a < f && *a != ':' && k < cabe - 1; a++)
			host[k++] = (char)minuscula((u8)*a);
	host[k] = 0;
	while (k > 0 && host[k - 1] == '.')
		host[--k] = 0;
	return k > 0;
}

void html_dominio_base(const char *host, char *d, int cabe)
{
	char h[256];
	const char *s = host, *a;
	int k = 0, usu, n, puntos = 0, i;
	const char *etq[4] = {0};

	if (cabe <= 0)
		return;
	d[0] = 0;
	if (!host)
		return;
	if (strstr(host, "://") || (host[0] == '/' && host[1] == '/')) {
		if (!sitio_de(host, h, sizeof h, &usu, NULL, 0))
			return;
	} else {
		a = strrchr(s, '@');
		if (a)
			s = a + 1;
		for (; *s && *s != '/' && *s != ':' && *s != '?' && *s != '#' && !es_espacio((u8)*s) && k < (int)sizeof h - 1; s++)
			h[k++] = (char)minuscula((u8)*s);
		h[k] = 0;
		while (k > 0 && h[k - 1] == '.')
			h[--k] = 0;
	}
	if (es_ip(h)) {
		cr_copia_en(d, cabe, h, (u32)strlen(h));
		return;
	}
	n = (int)strlen(h);
	/* las tres últimas etiquetas */
	for (i = n - 1; i >= 0; i--)
		if (h[i] == '.') {
			if (puntos < 3)
				etq[puntos] = h + i + 1;
			puntos++;
		}
	if (puntos < 2) {
		cr_copia_en(d, cabe, h, (u32)n);
		return;
	}
	/* ¿las dos últimas son un sufijo de los de dos niveles? */
	{
		const char *dos = etq[1];
		u32 j;

		for (j = 0; j < sizeof SUFIJOS2 / sizeof SUFIJOS2[0]; j++)
			if (!strcmp(dos, SUFIJOS2[j])) {
				const char *r = puntos >= 3 ? etq[2] : h;

				cr_copia_en(d, cabe, r, (u32)strlen(r));
				return;
			}
		cr_copia_en(d, cabe, dos, (u32)strlen(dos));
	}
}

/* ¿parece un dominio? (letras, cifras, guiones y puntos; un TLD de letras) */
static int parece_dominio(const char *s, u32 n)
{
	u32 i, etq = 0, tld = 0, puntos = 0;
	int letras = 1;

	if (n < 4)
		return 0;
	for (i = 0; i < n; i++) {
		u8 c = (u8)s[i];

		if (c == '.') {
			if (!etq)
				return 0;
			puntos++;
			etq = 0;
			tld = 0;
			letras = 1;
			continue;
		}
		if (!(es_alnum(c) || c == '-' || c >= 0x80))
			return 0;
		if (!es_letra(c))
			letras = 0;
		etq++;
		tld++;
	}
	return puntos >= 1 && tld >= 2 && letras;
}

/* el porqué, sin pasarse ni partir una letra */
static void pon_motivo(Enlace *e, const char *f, const char *a, const char *b)
{
	char x[56], y[72], m[400];

	cr_copia_en(x, sizeof x, a, (u32)strlen(a));
	cr_copia_en(y, sizeof y, b ? b : "", b ? (u32)strlen(b) : 0);
	snprintf(m, sizeof m, f, x, y);
	cr_copia_en(e->motivo, sizeof e->motivo, m, (u32)strlen(m));
}

static void comprueba_enlace(Doc *d, Enlace *e)
{
	char hh[200], hv[200], b1[200], b2[200], usu[64];
	int usuario = 0, k = 0;
	const char *v = e->visible, *f;
	u32 n;

	if (!e->href || !sitio_de(e->href, hh, sizeof hh, &usuario, usu, sizeof usu))
		return;
	if (usuario) {
		e->enganoso = 1;
		pon_motivo(e, "El enlace lleva «%s@» delante del sitio: en verdad va a «%s».", usu, hh);
		d->enganosos++;
		return;
	}
	if (es_ip(hh)) {
		e->enganoso = 1;
		pon_motivo(e, "El enlace lleva a una dirección numérica («%s»)%s, no a un nombre.", hh, "");
		d->enganosos++;
		return;
	}
	if (!v)
		return;
	/* lo que se ve: sin blancos alrededor ni < > */
	while (*v && (es_espacio((u8)*v) || *v == '<' || *v == '('))
		v++;
	n = (u32)strlen(v);
	while (n && (es_espacio((u8)v[n - 1]) || v[n - 1] == '>' || v[n - 1] == ')' || v[n - 1] == '.'))
		n--;
	if (!n || memchr(v, ' ', n) || memchr(v, '\t', n) || memchr(v, '\n', n))
		return;
	{
		char vis[300];
		int us2;

		cr_copia_en(vis, sizeof vis, v, n);
		if (cr_empieza(vis, n, "http://") || cr_empieza(vis, n, "https://")) {
			if (!sitio_de(vis, hv, sizeof hv, &us2, NULL, 0))
				return;
		} else {
			const char *s = vis;

			if (cr_empieza(s, n, "mailto:"))
				return;
			for (f = s; *f && *f != '/' && *f != ':' && *f != '?' && *f != '#' && k < (int)sizeof hv - 1; f++)
				hv[k++] = (char)minuscula((u8)*f);
			hv[k] = 0;
			if (strchr(s, '@'))
				return;	/* una dirección de correo: no es un sitio */
			if (!parece_dominio(hv, (u32)k))
				return;
		}
	}
	if (!hv[0])
		return;
	html_dominio_base(hv, b1, sizeof b1);
	html_dominio_base(hh, b2, sizeof b2);
	if (strcmp(b1, b2)) {
		e->enganoso = 1;
		pon_motivo(e, "El enlace dice «%s» pero lleva a «%s».", hv, hh);
		d->enganosos++;
	}
}

/* ---- las etiquetas ---- */
typedef struct {
	char nombre[16];
	u32 vi, vn;
	int hay_valor;
} Atributo;

static const Atributo *atributo(const Atributo *a, int na, const char *n)
{
	int k;

	for (k = 0; k < na; k++)
		if (!strcmp(a[k].nombre, n))
			return &a[k];
	return NULL;
}

static int es_script_url(const char *u)
{
	char b[24];
	int k = 0;

	/* sin blancos ni controles por medio («java\tscript:») */
	for (; *u && k < (int)sizeof b - 1; u++) {
		if ((u8)*u <= 32)
			continue;
		b[k++] = (char)minuscula((u8)*u);
		if (*u == ':')
			break;
	}
	b[k] = 0;
	return !strncmp(b, "javascript:", 11) || !strncmp(b, "vbscript:", 9) || !strncmp(b, "livescript:", 11) ||
	       (!strncmp(b, "data:", 5) && cr_empieza(u + 1, 9, "text/html"));
}

/* una dirección relativa con <base> */
static char *con_base(H *h, char *u)
{
	const char *p;
	Texto t;

	if (!h->base || !u || !u[0] || u[0] == '#')
		return u;
	for (p = u; es_alnum((u8)*p) || *p == '+' || *p == '-' || *p == '.'; p++)
		;
	if (*p == ':' && p > u)
		return u;	/* ya lleva su esquema */
	if (u[0] == '/' && u[1] == '/')
		return u;
	tx_ini(&t, 0);
	if (u[0] == '/') {
		/* esquema y sitio de la base */
		const char *b = strstr(h->base, "://"), *f;

		f = b ? strchr(b + 3, '/') : NULL;
		tx_pon(&t, h->base, f ? (u32)(f - h->base) : (u32)strlen(h->base));
	} else {
		const char *f = strrchr(h->base, '/');
		const char *b = strstr(h->base, "://");

		if (f && (!b || f > b + 2))
			tx_pon(&t, h->base, (u32)(f - h->base) + 1);
		else {
			tx_s(&t, h->base);
			tx_c(&t, '/');
		}
	}
	tx_s(&t, u);
	{
		char *r = tx_fin(&t, NULL);

		if (!r)
			return u;
		so_suelta(u);
		return r;
	}
}

static void termina_enlace(H *h)
{
	Doc *d = h->d;
	Enlace *e;
	u32 a, b, k, j;
	char *v;

	if (!h->ctx.en_enlace || h->ctx.enlace >= d->n_enlaces)
		return;
	e = &d->enlaces[h->ctx.enlace];
	if (e->visible)
		return;
	a = h->enlace_ini;
	b = h->t.n;
	if (a > b)
		a = b;
	if (b - a > MAX_VISIBLE)
		b = a + MAX_VISIBLE;
	while (b > a && b < h->t.n && ((u8)h->t.d[b] & 0xC0) == 0x80)
		b--;
	v = so_pide(b - a + 1);
	if (!v) {
		h->sin_memoria = 1;
		return;
	}
	/* sin las imágenes (U+FFFC) */
	for (k = a, j = 0; k < b; k++) {
		if (k + 2 < b && (u8)h->t.d[k] == 0xEF && (u8)h->t.d[k + 1] == 0xBF && (u8)h->t.d[k + 2] == 0xBC) {
			k += 2;
			continue;
		}
		v[j++] = h->t.d[k];
	}
	v[j] = 0;
	e->visible = v;
	comprueba_enlace(d, e);
}

static int empuja(H *h, u8 tag)
{
	if (h->np >= HTML_MAX_ANIDADO)
		return 0;
	h->pila[h->np] = h->ctx;
	h->pila[h->np].tag = tag;
	h->np++;
	return 1;
}

/* quita de la pila hasta la k (incluida) */
static void quita_hasta(H *h, int k)
{
	while (h->np > k) {
		Marco *m = &h->pila[--h->np];
		u8 t = m->tag;

		if (t == T_A)
			termina_enlace(h);
		h->ctx = *m;	/* el estado de antes de abrirla */
		h->ctx.tag = h->np ? h->pila[h->np - 1].tag : T_NADA;
		switch (t) {
		case T_P: case T_H1: case T_H2: case T_H3: case T_H4: case T_H5: case T_H6: case T_CITA: case T_PRE:
		case T_TABLE: case T_DL:
			cierra_par(h);
			pide_hueco(h, 1);
			break;
		case T_UL: case T_OL:
			cierra_par(h);
			if (!h->ctx.nivel_lista)
				pide_hueco(h, 1);	/* (las de dentro de otra, sin hueco) */
			break;
		case T_DIV: case T_LI: case T_TR: case T_BLOQUE: case T_CENTER: case T_DT: case T_DD:
			cierra_par(h);
			break;
		case T_TD: case T_TH:
			break;
		}
	}
}

/* la última abierta de ese tipo, sin pasar de las «barreras» */
static int busca_abierta(const H *h, u8 tag, const u8 *barreras, int nb)
{
	int k, j;

	for (k = h->np - 1; k >= 0; k--) {
		if (h->pila[k].tag == tag)
			return k;
		for (j = 0; j < nb; j++)
			if (h->pila[k].tag == barreras[j])
				return -1;
	}
	return -1;
}

static int cierra_p(H *h)
{
	if (h->np && h->pila[h->np - 1].tag == T_P) {
		quita_hasta(h, h->np - 1);
		return 1;
	}
	return 0;
}

static int estilo_oculto(const char *st)
{
	char b[256];
	int k = 0;

	for (; *st && k < (int)sizeof b - 1; st++)
		if (!es_espacio((u8)*st))
			b[k++] = (char)minuscula((u8)*st);
	b[k] = 0;
	return strstr(b, "display:none") || strstr(b, "visibility:hidden") || strstr(b, "mso-hide:all");
}

static int numero_atributo(const char *s, u32 n, int *hay)
{
	u32 i = 0, v;

	while (i < n && es_espacio((u8)s[i]))
		i++;
	v = cr_numero(s, n, &i, hay);
	if (*hay && i < n && s[i] == '%')
		*hay = 0;	/* un porcentaje no dice el tamaño */
	return v > 100000 ? 100000 : (int)v;
}

/* el tamaño de un style («width:1px») */
static void tam_estilo(const char *st, const char *que, int *v, int *hay)
{
	const char *p = st;

	while ((p = strstr(p, que)) != NULL) {
		const char *q = p + strlen(que);
		u32 i = 0;
		int h;
		u32 x;

		if (p > st && (es_alnum((u8)p[-1]) || p[-1] == '-')) {
			p = q;
			continue;
		}
		while (*q == ' ')
			q++;
		if (*q != ':') {
			p = q;
			continue;
		}
		q++;
		while (*q == ' ')
			q++;
		x = cr_numero(q, (u32)strlen(q), &i, &h);
		if (h && (q[i] == 'p' || q[i] == ';' || q[i] == 0 || q[i] == ' ')) {
			*v = x > 100000 ? 100000 : (int)x;
			*hay = 1;
		}
		return;
	}
}

static void imagen(H *h, const char *s, const Atributo *a, int na)
{
	Doc *d = h->d;
	const Atributo *src = atributo(a, na, "src"), *alt = atributo(a, na, "alt");
	const Atributo *w = atributo(a, na, "width"), *hg = atributo(a, na, "height"), *st = atributo(a, na, "style");
	Imagen *im;
	int hw = 0, hh = 0, oculta = h->ctx.oculto != 0;
	char *estilo = NULL;

	if (d->n_imagenes >= MAX_IMAGENES || !crece((void **)&d->imagenes, &h->cap_imagenes, d->n_imagenes + 1, sizeof(Imagen))) {
		if (d->n_imagenes < MAX_IMAGENES)
			h->sin_memoria = 1;
		else if (alt && alt->vn)
			pon_texto(h, s + alt->vi, alt->vn);
		return;
	}
	im = &d->imagenes[d->n_imagenes];
	memset(im, 0, sizeof *im);
	im->src = valor_atributo(src ? s + src->vi : "", src ? src->vn : 0, 1u << 20);
	im->alt = valor_atributo(alt ? s + alt->vi : "", alt ? alt->vn : 0, 4096);
	if (!im->src || !im->alt) {
		so_suelta(im->src);
		so_suelta(im->alt);
		h->sin_memoria = 1;
		return;
	}
	{
		/* sin blancos delante */
		char *p = im->src;
		u32 l;

		while (*p == ' ')
			p++;
		if (p != im->src)
			memmove(im->src, p, strlen(p) + 1);
		l = (u32)strlen(im->src);
		while (l && im->src[l - 1] == ' ')
			im->src[--l] = 0;
	}
	im->src = con_base(h, im->src);
	if (w)
		im->ancho = numero_atributo(s + w->vi, w->vn, &hw);
	if (hg)
		im->alto = numero_atributo(s + hg->vi, hg->vn, &hh);
	if (st) {
		estilo = valor_atributo(s + st->vi, st->vn, 4096);
		if (estilo) {
			char *p;

			for (p = estilo; *p; p++)
				*p = (char)minuscula((u8)*p);
			if (estilo_oculto(estilo))
				oculta = 1;
			if (!hw)
				tam_estilo(estilo, "width", &im->ancho, &hw);
			if (!hh)
				tam_estilo(estilo, "height", &im->alto, &hh);
			so_suelta(estilo);
		}
	}
	if (!hw)
		im->ancho = 0;
	if (!hh)
		im->alto = 0;
	im->remota = cr_empieza(im->src, (u32)strlen(im->src), "http:") || cr_empieza(im->src, (u32)strlen(im->src), "https:") ||
		     cr_empieza(im->src, (u32)strlen(im->src), "ftp:") || (im->src[0] == '/' && im->src[1] == '/');
	im->rastreo = oculta || (hw && hh && im->ancho <= 1 && im->alto <= 1) || (hw && im->ancho == 0) || (hh && im->alto == 0);
	if (im->remota)
		d->remotas++;
	d->n_imagenes++;
	if (!im->rastreo && !h->ctx.oculto) {
		/* la imagen, como un tramo suyo (U+FFFC) */
		if (!h->par_abierto) {
			if (!abre_par(h, h->ctx.pre ? P_PRE : h->ctx.clase))
				return;
		} else if (h->tab)
			pon_bytes(h, "\t", 1, estilo_de(h), h->ctx.enlace, 0);
		else if (h->espacio)
			pon_bytes(h, " ", 1, estilo_de(h), h->ctx.enlace, 0);
		h->tab = h->espacio = 0;
		pon_bytes(h, "\xEF\xBF\xBC", 3, (u16)((h->ctx.estilo & ~E_ENLACE) | E_IMAGEN), (u16)(d->n_imagenes - 1), 1);
	}
}

static void abre_enlace(H *h, const char *s, const Atributo *a, int na)
{
	Doc *d = h->d;
	const Atributo *hr = atributo(a, na, "href");
	Enlace *e;
	char *u;

	if (!hr)
		return;
	u = valor_atributo(s + hr->vi, hr->vn, 1u << 20);
	if (!u) {
		h->sin_memoria = 1;
		return;
	}
	{
		char *p = u;
		u32 l;

		while (*p == ' ')
			p++;
		if (p != u)
			memmove(u, p, strlen(p) + 1);
		l = (u32)strlen(u);
		while (l && u[l - 1] == ' ')
			u[--l] = 0;
	}
	if (es_script_url(u)) {
		d->habia_scripts++;
		so_suelta(u);
		return;
	}
	if (!u[0] || d->n_enlaces >= MAX_ENLACES || !crece((void **)&d->enlaces, &h->cap_enlaces, d->n_enlaces + 1, sizeof(Enlace))) {
		so_suelta(u);
		return;
	}
	u = con_base(h, u);
	e = &d->enlaces[d->n_enlaces];
	memset(e, 0, sizeof *e);
	e->href = u;
	h->ctx.enlace = (u16)d->n_enlaces++;
	h->ctx.en_enlace = 1;
	h->enlace_ini = h->t.n;
	/* lo que se vea sale con su espacio de delante fuera del enlace */
	if (h->par_abierto && (h->espacio || h->tab)) {
		Marco c = h->ctx;

		h->ctx.en_enlace = 0;
		pon_bytes(h, h->tab ? "\t" : " ", 1, estilo_de(h), 0, 0);
		h->ctx = c;
		h->espacio = h->tab = 0;
		h->enlace_ini = h->t.n;
	}
}

/* <etiqueta ...> ya leída: lo que hace */
static void etiqueta_abre(H *h, const char *s, u8 t, const Atributo *a, int na, int sola)
{
	static const u8 bar_li[] = {T_UL, T_OL}, bar_td[] = {T_TR, T_TABLE}, bar_tr[] = {T_TABLE}, bar_dd[] = {T_DL};
	const Atributo *al = atributo(a, na, "align"), *st = atributo(a, na, "style");
	int k, oculto = atributo(a, na, "hidden") != NULL;

	if (st && st->vn) {
		char *e = valor_atributo(s + st->vi, st->vn, 4096);

		if (e) {
			if (estilo_oculto(e))
				oculto = 1;
			so_suelta(e);
		}
	}
	switch (t) {
	case T_BR:
		if (h->par_abierto)
			cierra_par(h);
		else
			pide_hueco(h, h->hueco + 1);
		return;
	case T_HR:
		cierra_p(h);
		cierra_par(h);
		if (!h->ctx.oculto && abre_par(h, P_RAYA))
			cierra_par(h);
		return;
	case T_IMG:
		imagen(h, s, a, na);
		return;
	case T_BASE: {
		const Atributo *b = atributo(a, na, "href");

		if (b && !h->base) {
			h->base = valor_atributo(s + b->vi, b->vn, 4096);
			if (h->base && (!strstr(h->base, "://") || es_script_url(h->base))) {
				so_suelta(h->base);
				h->base = NULL;
			}
		}
		return;
	}
	case T_P: case T_DIV: case T_H1: case T_H2: case T_H3: case T_H4: case T_H5: case T_H6: case T_CITA: case T_UL:
	case T_OL: case T_PRE: case T_TABLE: case T_DL: case T_BLOQUE: case T_CENTER:
		cierra_p(h);
		break;
	case T_LI:
		cierra_p(h);
		k = busca_abierta(h, T_LI, bar_li, 2);
		if (k >= 0)
			quita_hasta(h, k);
		break;
	case T_DT: case T_DD:
		cierra_p(h);
		k = busca_abierta(h, T_DT, bar_dd, 1);
		if (k < 0)
			k = busca_abierta(h, T_DD, bar_dd, 1);
		if (k >= 0)
			quita_hasta(h, k);
		break;
	case T_TD: case T_TH:
		k = busca_abierta(h, T_TD, bar_td, 2);
		if (k < 0)
			k = busca_abierta(h, T_TH, bar_td, 2);
		if (k >= 0)
			quita_hasta(h, k);
		break;
	case T_TR:
		k = busca_abierta(h, T_TR, bar_tr, 1);
		if (k >= 0)
			quita_hasta(h, k);
		break;
	case T_A:
		k = busca_abierta(h, T_A, NULL, 0);
		if (k >= 0)
			quita_hasta(h, k);
		break;
	case T_NADA: case T_OTRA: case T_SECCION_TABLA:
		return;
	}
	if (sola) {
		/* <div/> <p/>: vacías (como en XHTML) */
		if (t != T_A && t != T_B && t != T_I && t != T_U && t != T_S && t != T_MONO && t != T_PEQUENO && t != T_SPAN &&
		    t != T_TD && t != T_TH)
			cierra_par(h);
		return;
	}
	if (!empuja(h, t))
		return;
	if (oculto)
		h->ctx.oculto = 1;
	if (al && al->vn >= 4) {
		if (cr_empieza(s + al->vi, al->vn, "center"))
			h->ctx.alinea = 1;
		else if (cr_empieza(s + al->vi, al->vn, "right"))
			h->ctx.alinea = 2;
		else if (cr_empieza(s + al->vi, al->vn, "left"))
			h->ctx.alinea = 0;
	}
	switch (t) {
	case T_B:
		h->ctx.estilo |= E_NEGRITA;
		break;
	case T_I:
		h->ctx.estilo |= E_CURSIVA;
		break;
	case T_U:
		h->ctx.estilo |= E_SUBRAYADO;
		break;
	case T_S:
		h->ctx.estilo |= E_TACHADO;
		break;
	case T_MONO:
		h->ctx.estilo |= E_MONO;
		break;
	case T_PEQUENO:
		h->ctx.estilo |= E_PEQUENO;
		break;
	case T_A:
		abre_enlace(h, s, a, na);
		break;
	case T_H1: case T_H2: case T_H3:
		cierra_par(h);
		pide_hueco(h, 1);
		h->ctx.clase = P_TITULO;
		h->ctx.estilo |= E_TITULO | E_NEGRITA;
		break;
	case T_H4: case T_H5: case T_H6:
		cierra_par(h);
		pide_hueco(h, 1);
		h->ctx.clase = (u8)clase_base(&h->ctx);
		h->ctx.estilo |= E_NEGRITA;
		break;
	case T_P:
		cierra_par(h);
		pide_hueco(h, 1);
		h->ctx.clase = (u8)clase_base(&h->ctx);
		break;
	case T_DIV: case T_BLOQUE: case T_DT:
		cierra_par(h);
		h->ctx.clase = (u8)clase_base(&h->ctx);
		break;
	case T_CENTER:
		cierra_par(h);
		h->ctx.alinea = 1;
		h->ctx.clase = (u8)clase_base(&h->ctx);
		break;
	case T_DD:
		cierra_par(h);
		if (h->ctx.nivel_lista < 200)
			h->ctx.nivel_lista++;
		h->ctx.clase = (u8)clase_base(&h->ctx);
		break;
	case T_CITA:
		cierra_par(h);
		pide_hueco(h, 1);
		if (h->ctx.nivel_cita < 200)
			h->ctx.nivel_cita++;
		h->ctx.clase = P_CITA;
		break;
	case T_UL: case T_OL: {
		const Atributo *ini = atributo(a, na, "start");
		int hay = 0;

		cierra_par(h);
		if (!h->ctx.nivel_lista)
			pide_hueco(h, 1);
		if (h->ctx.nivel_lista < 200)
			h->ctx.nivel_lista++;
		h->ctx.clase = P_LISTA;
		/* el contador vive en el marco de la lista (en la pila: lo cambian
		 * sus <li>; al cerrarla no se mira) */
		h->pila[h->np - 1].es_ol = t == T_OL;
		h->pila[h->np - 1].contador = 1;
		if (t == T_OL && ini) {
			u32 v = (u32)numero_atributo(s + ini->vi, ini->vn, &hay);

			if (hay)
				h->pila[h->np - 1].contador = v;
		}
		break;
	}
	case T_LI: {
		char b[24];
		int j;

		cierra_par(h);
		h->ctx.clase = P_LISTA;
		if (!h->ctx.nivel_lista)
			h->ctx.nivel_lista = 1;
		/* el número de la lista de fuera */
		for (j = h->np - 2; j >= 0; j--)
			if (h->pila[j].tag == T_UL || h->pila[j].tag == T_OL)
				break;
		if (j >= 0 && h->pila[j].es_ol) {
			u32 *c = &h->pila[j].contador;

			snprintf(b, sizeof b, "%u. ", (unsigned)*c);
			if (*c < 0xFFFFFFF0u)
				(*c)++;
		} else
			strcpy(b, "\xE2\x80\xA2 ");
		if (!h->ctx.oculto) {
			Marco c = h->ctx;

			h->ctx.estilo = 0;
			h->ctx.en_enlace = 0;
			pon_trozo(h, b, (u32)strlen(b) - 1);
			h->ctx = c;
			h->espacio = 1;
		}
		break;
	}
	case T_PRE:
		cierra_par(h);
		pide_hueco(h, 1);
		h->ctx.clase = P_PRE;
		h->ctx.pre = 1;
		h->ctx.estilo |= E_MONO;
		h->pre_inicio = 1;
		break;
	case T_TABLE:
		cierra_par(h);
		h->ctx.clase = (u8)clase_base(&h->ctx);
		break;
	case T_TR:
		cierra_par(h);
		h->ctx.clase = P_TABLA;
		break;
	case T_TD: case T_TH:
		if (h->par_abierto)
			h->tab = 1;
		if (t == T_TH)
			h->ctx.estilo |= E_NEGRITA;
		break;
	}
}

static void etiqueta_cierra(H *h, u8 t)
{
	int k;

	if (t == T_BR) {
		etiqueta_abre(h, "", T_BR, NULL, 0, 1);
		return;
	}
	if (t == T_NADA || t == T_OTRA || t == T_IMG || t == T_HR || t == T_BASE || t == T_SECCION_TABLA)
		return;
	if (t == T_P) {
		k = busca_abierta(h, T_P, NULL, 0);
		if (k >= 0)
			quita_hasta(h, k);
		else {
			cierra_par(h);	/* </p> sin <p>: un párrafo vacío */
			pide_hueco(h, 1);
		}
		return;
	}
	k = busca_abierta(h, t, NULL, 0);
	if (k < 0 && t >= T_H1 && t <= T_H6) {
		int j;

		for (j = T_H1; j <= T_H6 && k < 0; j++)
			k = busca_abierta(h, (u8)j, NULL, 0);
	}
	if (k >= 0)
		quita_hasta(h, k);
}

static Doc *doc_nuevo(void)
{
	Doc *d = so_pide(sizeof *d);

	if (d)
		memset(d, 0, sizeof *d);
	return d;
}

static Doc *acaba(H *h)
{
	Doc *d = h->d;

	quita_hasta(h, 0);
	so_suelta(h->base);
	if (h->sin_memoria) {
		d->texto = tx_fin(&h->t, &d->n);
		doc_libera(d);
		return NULL;
	}
	d->texto = tx_fin(&h->t, &d->n);
	if (!d->texto) {
		doc_libera(d);
		return NULL;
	}
	return d;
}

static void h_ini(H *h, Doc *d)
{
	memset(h, 0, sizeof *h);
	h->d = d;
	tx_ini(&h->t, HTML_MAX_TEXTO + 16);
	h->primero = 1;
	h->ctx.tag = T_NADA;
	h->ctx.clase = P_NORMAL;
}

/* lo de «</nombre» (sin mayúsculas) desde i: su «<», o n */
static u32 busca_cierre(const char *s, u32 n, u32 i, const char *nombre)
{
	u32 l = (u32)strlen(nombre);

	while (i < n) {
		const char *p = memchr(s + i, '<', n - i);
		u32 k;

		if (!p)
			return n;
		k = (u32)(p - s);
		if (k + 2 + l <= n && s[k + 1] == '/' && cr_empieza(s + k + 2, l, nombre) &&
		    (k + 2 + l == n || !es_alnum((u8)s[k + 2 + l])))
			return k;
		i = k + 1;
	}
	return n;
}

Doc *html_a_doc(const char *s, u32 n)
{
	H h;
	Doc *d = doc_nuevo();
	u32 i = 0;

	if (!d)
		return NULL;
	if (!s)
		n = 0;
	h_ini(&h, d);
	while (i < n && !h.sin_memoria && !d->cortado) {
		const char *p = memchr(s + i, '<', n - i);
		u32 k = p ? (u32)(p - s) : n, j;

		if (k > i)
			pon_texto(&h, s + i, k - i);
		i = k;
		if (i >= n)
			break;
		j = i + 1;
		if (j >= n) {
			pon_texto(&h, "<", 1);
			break;
		}
		if (s[j] == '!') {
			if (j + 2 < n && s[j + 1] == '-' && s[j + 2] == '-') {
				/* <!-- ... --> (y <!--> <!---> vacíos) */
				u32 q = j + 3;

				if (q < n && s[q] == '>') {
					i = q + 1;
					continue;
				}
				if (q + 1 < n && s[q] == '-' && s[q + 1] == '>') {
					i = q + 2;
					continue;
				}
				for (;;) {
					const char *m = q < n ? memchr(s + q, '-', n - q) : NULL;

					if (!m) {
						i = n;
						break;
					}
					q = (u32)(m - s);
					if (q + 2 < n && s[q + 1] == '-' && s[q + 2] == '>') {
						i = q + 3;
						break;
					}
					if (q + 3 < n && s[q + 1] == '-' && s[q + 2] == '!' && s[q + 3] == '>') {
						i = q + 4;
						break;
					}
					q++;
				}
				continue;
			}
			p = memchr(s + j, '>', n - j);
			i = p ? (u32)(p - s) + 1 : n;
			continue;
		}
		if (s[j] == '?') {
			p = memchr(s + j, '>', n - j);
			i = p ? (u32)(p - s) + 1 : n;
			continue;
		}
		if (s[j] == '/') {
			char nm[16];
			int l = 0;

			j++;
			if (j < n && !es_letra((u8)s[j])) {
				/* «</ ...>»: un comentario raro */
				p = memchr(s + j, '>', n - j);
				i = p ? (u32)(p - s) + 1 : n;
				continue;
			}
			for (; j < n && es_alnum((u8)s[j]); j++)
				if (l < (int)sizeof nm - 1)
					nm[l++] = (char)minuscula((u8)s[j]);
			nm[l] = 0;
			p = memchr(s + j, '>', n - j);
			i = p ? (u32)(p - s) + 1 : n;
			etiqueta_cierra(&h, etiqueta(nm));
			continue;
		}
		if (!es_letra((u8)s[j])) {
			pon_texto(&h, "<", 1);
			i = j;
			continue;
		}
		{
			char nm[16];
			int l = 0, na = 0, sola = 0;
			Atributo at[MAX_ATRIBUTOS];
			u8 t;
			u32 q;

			for (; j < n && !es_espacio((u8)s[j]) && s[j] != '>' && s[j] != '/'; j++)
				if (l < (int)sizeof nm - 1)
					nm[l++] = (char)minuscula((u8)s[j]);
			nm[l] = 0;
			/* los atributos */
			while (j < n) {
				u32 a0, a1;
				Atributo *A = NULL;

				while (j < n && (es_espacio((u8)s[j]) || s[j] == '/')) {
					if (s[j] == '/' && j + 1 < n && s[j + 1] == '>')
						sola = 1;
					j++;
				}
				if (j >= n || s[j] == '>')
					break;
				a0 = j;
				for (j++; j < n && !es_espacio((u8)s[j]) && s[j] != '>' && s[j] != '/' && s[j] != '='; j++)
					;
				a1 = j;
				if (a1 - a0 > 2 && minuscula((u8)s[a0]) == 'o' && minuscula((u8)s[a0 + 1]) == 'n')
					d->habia_scripts++;
				if (na < MAX_ATRIBUTOS && a1 - a0 < sizeof at[0].nombre) {
					u32 x;

					A = &at[na++];
					for (x = 0; x < a1 - a0; x++)
						A->nombre[x] = (char)minuscula((u8)s[a0 + x]);
					A->nombre[a1 - a0] = 0;
					A->vi = A->vn = 0;
					A->hay_valor = 0;
				}
				while (j < n && es_espacio((u8)s[j]))
					j++;
				if (j < n && s[j] == '=') {
					j++;
					while (j < n && es_espacio((u8)s[j]))
						j++;
					if (j < n && (s[j] == '"' || s[j] == '\'')) {
						char c = s[j];
						const char *f = memchr(s + j + 1, c, n - j - 1);
						u32 e = f ? (u32)(f - s) : n;

						if (A) {
							A->vi = j + 1;
							A->vn = e - (j + 1);
						}
						j = e < n ? e + 1 : n;
					} else {
						u32 v0 = j;

						while (j < n && !es_espacio((u8)s[j]) && s[j] != '>')
							j++;
						if (A) {
							A->vi = v0;
							A->vn = j - v0;
						}
					}
					if (A)
						A->hay_valor = 1;
				}
			}
			i = j < n ? j + 1 : n;
			{
				u32 c;

				for (c = 0; c < sizeof CRUDOS / sizeof CRUDOS[0]; c++)
					if (!strcmp(nm, CRUDOS[c]))
						break;
				if (c < sizeof CRUDOS / sizeof CRUDOS[0]) {
					if (c == 0)
						d->habia_scripts++;
					if (!sola || c == 0) {
						q = busca_cierre(s, n, i, nm);
						p = q < n ? memchr(s + q, '>', n - q) : NULL;
						i = p ? (u32)(p - s) + 1 : n;
					}
					continue;
				}
			}
			t = etiqueta(nm);
			etiqueta_abre(&h, s, t, at, na, sola);
		}
	}
	return acaba(&h);
}

/* ---- de texto ---- */
static int es_letra_url(u8 c)
{
	return c > 32 && c != '<' && c != '>' && c != '"' && c != 0x7f;
}

/* un enlace del texto (url o dirección) dentro de la palabra [a, b): 1 si lo hay */
static int busca_url(const char *s, u32 a, u32 b, u32 *ia, u32 *ib, int *tipo)
{
	u32 i;

	for (i = a; i < b; i++) {
		u32 r = b - i;

		if ((i == a || !es_alnum((u8)s[i - 1])) &&
		    ((r > 8 && cr_empieza(s + i, r, "https://")) || (r > 7 && cr_empieza(s + i, r, "http://")) ||
		     (r > 6 && cr_empieza(s + i, r, "ftp://")) || (r > 7 && cr_empieza(s + i, r, "mailto:")) ||
		     (r > 5 && cr_empieza(s + i, r, "www.")))) {
			u32 f = i;
			int par = 0;

			while (f < b && es_letra_url((u8)s[f])) {
				if (s[f] == '(')
					par++;
				else if (s[f] == ')')
					par--;
				f++;
			}
			/* sin la puntuación del final */
			while (f > i && (s[f - 1] == '.' || s[f - 1] == ',' || s[f - 1] == ';' || s[f - 1] == ':' || s[f - 1] == '!' ||
					 s[f - 1] == '?' || s[f - 1] == '\'' || (s[f - 1] == ')' && par < 0) || s[f - 1] == ']')) {
				if (s[f - 1] == ')')
					par++;
				f--;
			}
			if (cr_empieza(s + i, f - i, "www.") && !memchr(s + i + 4, '.', f - i - 4))
				continue;
			*ia = i;
			*ib = f;
			*tipo = cr_empieza(s + i, f - i, "www.") ? 1 : 0;
			return 1;
		}
	}
	/* una dirección de correo */
	{
		const char *ar = memchr(s + a, '@', b - a);

		if (ar) {
			u32 m = (u32)(ar - s), x = m, y = m + 1;

			while (x > a && (es_alnum((u8)s[x - 1]) || strchr("._%+-", s[x - 1])))
				x--;
			while (y < b && (es_alnum((u8)s[y]) || s[y] == '.' || s[y] == '-'))
				y++;
			while (y > m + 1 && s[y - 1] == '.')
				y--;
			while (x < m && s[x] == '.')
				x++;
			if (x < m && y > m + 1) {
				char t[260];

				if (y - x < sizeof t) {
					memcpy(t, s + x, y - x);
					t[y - x] = 0;
					if (mime_dir_valida(t)) {
						*ia = x;
						*ib = y;
						*tipo = 2;
						return 1;
					}
				}
			}
		}
	}
	return 0;
}

static void enlace_texto(H *h, const char *s, u32 a, u32 b, int tipo)
{
	Doc *d = h->d;
	Enlace *e;
	Texto t;

	if (d->n_enlaces >= MAX_ENLACES || !crece((void **)&d->enlaces, &h->cap_enlaces, d->n_enlaces + 1, sizeof(Enlace))) {
		pon_trozo(h, s + a, b - a);
		return;
	}
	tx_ini(&t, 0);
	if (tipo == 1)
		tx_s(&t, "http://");
	else if (tipo == 2)
		tx_s(&t, "mailto:");
	tx_pon(&t, s + a, b - a);
	e = &d->enlaces[d->n_enlaces];
	memset(e, 0, sizeof *e);
	e->href = tx_fin(&t, NULL);
	e->visible = cr_copia(s + a, b - a);
	if (!e->href || !e->visible) {
		so_suelta(e->href);
		so_suelta(e->visible);
		h->sin_memoria = 1;
		return;
	}
	h->ctx.enlace = (u16)d->n_enlaces++;
	h->ctx.en_enlace = 1;
	pon_trozo(h, s + a, b - a);
	h->ctx.en_enlace = 0;
	h->ctx.enlace = 0;
}

/* un renglón del texto: las palabras, con sus enlaces */
static void renglon_texto(H *h, const char *s, u32 a, u32 b)
{
	u32 i = a;

	while (i < b && !h->sin_memoria && !h->d->cortado) {
		u32 w, ia, ib;
		int tipo;

		if (es_blanco((u8)s[i])) {
			/* los blancos del texto se guardan tal cual (sin juntar) */
			u32 k = i;

			while (k < b && es_blanco((u8)s[k]))
				k++;
			if (h->par_abierto) {
				u32 x;

				for (x = i; x < k; x++)
					pon_bytes(h, s[x] == '\t' ? "\t" : " ", 1, estilo_de(h), 0, 0);
			} else {
				if (!abre_par(h, h->ctx.clase))
					return;
				for (; i < k; i++)
					pon_bytes(h, s[i] == '\t' ? "\t" : " ", 1, estilo_de(h), 0, 0);
			}
			i = k;
			continue;
		}
		for (w = i; w < b && !es_blanco((u8)s[w]); w++)
			;
		if (busca_url(s, i, w, &ia, &ib, &tipo)) {
			if (ia > i)
				pon_trozo(h, s + i, ia - i);
			enlace_texto(h, s, ia, ib, tipo);
			if (ib < w)
				pon_trozo(h, s + ib, w - ib);
		} else
			pon_trozo(h, s + i, w - i);
		i = w;
	}
}

Doc *texto_a_doc(const char *s, u32 n)
{
	H h;
	Doc *d = doc_nuevo();
	u32 p = 0;
	int firma = 0;

	if (!d)
		return NULL;
	if (!s)
		n = 0;
	h_ini(&h, d);
	while (p < n && !h.sin_memoria && !d->cortado) {
		const char *f = memchr(s + p, '\n', n - p);
		u32 e = f ? (u32)(f - s) : n, ef = e, a = p;
		int q = 0;

		if (ef > p && s[ef - 1] == '\r')
			ef--;
		/* la cita: «>», «>>», «> >» */
		for (;;) {
			u32 k = a;

			while (k < ef && s[k] == ' ' && k - a < 3 && q)
				k++;
			if (k < ef && s[k] == '>') {
				q++;
				a = k + 1;
				continue;
			}
			break;
		}
		if (q && a < ef && s[a] == ' ')
			a++;
		if (ef - a == 3 && !memcmp(s + a, "-- ", 3) && !q)
			firma = 1;
		h.ctx.nivel_cita = (u8)(q > 200 ? 200 : q);
		h.ctx.clase = q ? P_CITA : P_NORMAL;
		h.ctx.estilo = firma ? E_PEQUENO : 0;
		cierra_par(&h);
		if (a >= ef) {
			pide_hueco(&h, h.hueco + 1);
		} else {
			/* el texto ya viene en UTF-8: los octetos malos, fuera */
			if (!utf8_valido((const u8 *)s + a, ef - a)) {
				u32 l;
				char *u = jc_a_utf8("utf-8", (const u8 *)s + a, ef - a, &l);

				if (u) {
					renglon_texto(&h, u, 0, l);
					so_suelta(u);
				}
			} else
				renglon_texto(&h, s, a, ef);
		}
		p = e + 1;
	}
	return acaba(&h);
}

void doc_libera(Doc *d)
{
	u32 k;

	if (!d)
		return;
	for (k = 0; k < d->n_enlaces; k++) {
		so_suelta(d->enlaces[k].href);
		so_suelta(d->enlaces[k].visible);
	}
	for (k = 0; k < d->n_imagenes; k++) {
		so_suelta(d->imagenes[k].src);
		so_suelta(d->imagenes[k].alt);
	}
	so_suelta(d->enlaces);
	so_suelta(d->imagenes);
	so_suelta(d->tramos);
	so_suelta(d->parrafos);
	so_suelta(d->texto);
	so_suelta(d);
}

char *doc_a_texto(const Doc *d, u32 *largo)
{
	Texto t;
	u32 p, k;

	tx_ini(&t, 0);
	if (!d)
		return tx_fin(&t, largo);
	for (p = 0; p < d->n_parrafos; p++) {
		const Parrafo *P = &d->parrafos[p];
		u32 j;

		if (p) {
			tx_c(&t, '\n');
			for (j = 0; j < P->hueco; j++) {
				if (P->clase == P_CITA && d->parrafos[p - 1].clase == P_CITA) {
					for (k = 0; k < P->nivel; k++)
						tx_c(&t, '>');
				}
				tx_c(&t, '\n');
			}
		}
		if (P->clase == P_CITA) {
			for (k = 0; k < P->nivel; k++)
				tx_c(&t, '>');
			tx_c(&t, ' ');
		} else if (P->clase == P_LISTA && P->nivel > 1) {
			for (k = 1; k < P->nivel; k++)
				tx_s(&t, "  ");
		} else if (P->clase == P_RAYA) {
			tx_s(&t, "----------------------------------------");
			continue;
		}
		for (j = P->ini; j < P->fin && j < d->n_tramos; j++) {
			const Tramo *T = &d->tramos[j];

			if (T->estilo & E_IMAGEN) {
				const Imagen *im = T->enlace < d->n_imagenes ? &d->imagenes[T->enlace] : NULL;

				tx_c(&t, '[');
				tx_s(&t, im && im->alt && im->alt[0] ? im->alt : "imagen");
				tx_c(&t, ']');
				continue;
			}
			if (T->ini + T->n <= d->n)
				tx_pon(&t, d->texto + T->ini, T->n);
			/* al acabar un enlace, adónde lleva (si no es lo que se ve) */
			if ((T->estilo & E_ENLACE) && T->enlace < d->n_enlaces &&
			    (j + 1 >= P->fin || !(d->tramos[j + 1].estilo & E_ENLACE) || d->tramos[j + 1].enlace != T->enlace)) {
				const Enlace *e = &d->enlaces[T->enlace];
				const char *h = e->href ? e->href : "";
				const char *v = e->visible ? e->visible : "";

				if (strcmp(h, v) && !(cr_empieza(h, (u32)strlen(h), "mailto:") && !strcmp(h + 7, v)) &&
				    !(cr_empieza(h, (u32)strlen(h), "http://") && !strcmp(h + 7, v))) {
					tx_s(&t, " <");
					tx_s(&t, h);
					tx_c(&t, '>');
				}
			}
		}
	}
	if (d->n_parrafos)
		tx_c(&t, '\n');
	return tx_fin(&t, largo);
}
