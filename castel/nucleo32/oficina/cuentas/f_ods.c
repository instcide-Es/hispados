/* f_ods.c - Cuentas 2: el formato .ODS de LibreOffice (OpenDocument: un ZIP
 * con «mimetype» delante, sin comprimir, y XML). Se lee y se escribe: las
 * hojas con su nombre, los valores (números, porcentajes, moneda, fechas,
 * horas, lógicos, textos de varias líneas), las fórmulas (OpenFormula:
 * «of:=SUM([.A1:.A3])»), los estilos de celda (letra, tamaño, negrita,
 * cursiva, subrayado, tachado, color, fondo, bordes, alineación, partir en
 * líneas) con sus formatos de número (number:number-style, currency-style,
 * percentage-style, date-style, time-style, text-style), los anchos de
 * columna y altos de fila, las filas y columnas ocultas, las combinadas,
 * los comentarios (office:annotation), los nombres definidos, el área de
 * impresión, el rango del autofiltro y los paneles inmovilizados (en
 * settings.xml). Las filas y columnas «repetidas» se leen sin recorrerlas
 * una a una (LibreOffice escribe un millón de filas vacías en una).
 * Lo que se pierde: las tramas (en ODS solo hay color de fondo), el
 * formato condicional, los gráficos, la página y lo que no sea la primera
 * sección de un formato de número ([Rojo] para los negativos...).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "archivo.h"
#include "formula.h"
#include "numeros.h"
#include "zip.h"
#include "xml.h"

#define PX_CM (96.0 / 2.54)

/* ---- medidas y colores ---- */
static double medida_cm(const char *s)
{
	double v;
	const char *fin;

	if (!s || !num_lee_c(s, &v, &fin))
		return -1;
	if (empieza(fin, "cm"))
		return v;
	if (empieza(fin, "mm"))
		return v / 10;
	if (empieza(fin, "in"))
		return v * 2.54;
	if (empieza(fin, "pt"))
		return v * 2.54 / 72;
	if (empieza(fin, "pc"))
		return v * 2.54 / 6;
	if (empieza(fin, "px"))
		return v * 2.54 / 96;
	return v;
}

static double medida_pt(const char *s)
{
	double c = medida_cm(s);

	return c < 0 ? -1 : c * 72 / 2.54;
}

static u32 color(const char *s, u32 def)
{
	u32 v = 0;
	int i;

	if (!s || s[0] != '#')
		return def;
	for (i = 1; i <= 6; i++) {
		char c = s[i];

		if (c >= '0' && c <= '9')
			v = v * 16 + (u32)(c - '0');
		else if ((c | 32) >= 'a' && (c | 32) <= 'f')
			v = v * 16 + (u32)((c | 32) - 'a' + 10);
		else
			return def;
	}
	return v;
}

static int cierto(const char *s)
{
	return s && (iguales(s, "true") || iguales(s, "1"));
}

static double num_atr(const xml_nodo *n, const char *a, double def)
{
	const char *v = xml_atr(n, a);
	double x;

	if (!v || !num_lee_c(v, &x, NULL))
		return def;
	return x;
}

/* el texto de un text:p (con text:s, text:tab, text:line-break, text:span...) */
static void texto_p(const xml_nodo *n, bufer *b, int hondo)
{
	const xml_nodo *h;

	if (!n || hondo > 24)
		return;
	for (h = n->hijo; h; h = h->sig) {
		if (!h->nombre) {
			buf_pon(b, h->texto, h->largo);
			continue;
		}
		if (xml_es(h, "text:s")) {
			u32 k = (u32)num_atr(h, "text:c", 1);

			while (k-- > 0 && k < 1000)
				buf_c(b, ' ');
		} else if (xml_es(h, "text:tab"))
			buf_c(b, '\t');
		else if (xml_es(h, "text:line-break"))
			buf_c(b, '\n');
		else if (xml_es(h, "office:annotation"))
			continue;
		else
			texto_p(h, b, hondo + 1);
	}
}

/* el texto de una celda o anotación: sus text:p, separados por saltos de línea */
static char *texto_celda(const xml_nodo *n)
{
	bufer b = { 0 };
	const xml_nodo *p;
	int primero = 1;

	for (p = xml_hijo(n, "text:p"); p; p = xml_sig(p, "text:p")) {
		if (!primero)
			buf_c(&b, '\n');
		primero = 0;
		texto_p(p, &b, 0);
	}
	buf_c(&b, 0);
	if (b.mal || !b.d) {
		buf_suelta(&b);
		return duplica("");
	}
	return (char *)b.d;
}

/* ===================================================================== */
/*  Leer                                                                 */
/* ===================================================================== */
typedef struct {
	char nombre[96];
	char codigo[160];
} estilo_num;

typedef struct {
	char nombre[96];
	char padre[96];
	char datos[96];		/* el estilo de número */
	estilo e;
	u32 que;		/* lo que dice (bits) */
	int familia;		/* 0 celda, 1 columna, 2 fila */
	double ancho_cm, alto_cm;
	int alto_optimo;
	u16 id;			/* (el de Cuentas, ya hecho) */
	int hecho;
} estilo_ods;

enum { EQ_FAM = 1, EQ_TAM = 2, EQ_NEG = 4, EQ_CUR = 8, EQ_SUB = 16, EQ_TACH = 32, EQ_COLOR = 64, EQ_FONDO = 128, EQ_BORDE = 256,
       EQ_AH = 512, EQ_AV = 1024, EQ_AJ = 2048, EQ_SANG = 4096 };

typedef struct {
	libro *l;
	estilo_num *nu;
	u32 n_nu;
	estilo_ods *es;
	u32 n_es;
} lector;

static void lee_estilo_num(lector *L, const xml_nodo *s)
{
	bufer b = { 0 };
	const xml_nodo *k;
	const char *nom = xml_atr(s, "style:name");
	int fecha = xml_es(s, "number:date-style") || xml_es(s, "number:time-style");
	estilo_num *n;

	if (!nom)
		return;
	if (xml_es(s, "number:boolean-style")) {
		buf_txt(&b, "General");
	} else
		for (k = xml_primero(s); k; k = xml_sig(k, NULL)) {
			const char *v;

			if (xml_es(k, "number:number") || xml_es(k, "number:scientific-number")) {
				int d = (int)num_atr(k, "number:decimal-places", 0), mi = (int)num_atr(k, "number:min-integer-digits", 1), j;
				int gr = cierto(xml_atr(k, "number:grouping"));

				if (d < 0 || d > 30)
					d = 0;
				if (mi < 0 || mi > 30)
					mi = 1;
				if (gr) {
					char g[6] = "#,###";

					for (j = 0; j < mi && j < 3; j++)
						g[4 - j] = '0';
					for (j = 3; j < mi; j++)
						buf_c(&b, '0');
					buf_txt(&b, g);
				} else
					for (j = 0; j < mi; j++)
						buf_c(&b, '0');
				if (d) {
					buf_c(&b, '.');
					for (j = 0; j < d; j++)
						buf_c(&b, '0');
				}
				if (xml_es(k, "number:scientific-number")) {
					int me = (int)num_atr(k, "number:min-exponent-digits", 2);

					buf_txt(&b, "E+");
					for (j = 0; j < me && j < 4; j++)
						buf_c(&b, '0');
				}
			} else if (xml_es(k, "number:text") || xml_es(k, "number:currency-symbol")) {
				bufer t = { 0 };
				int raras = xml_es(k, "number:currency-symbol");

				xml_texto(k, &t);
				if (t.n == 1 && (t.d[0] == '%' || t.d[0] == ' ' || t.d[0] == '-' || t.d[0] == '/' || t.d[0] == ':' || t.d[0] == '.' ||
						 t.d[0] == ',' || t.d[0] == '(' || t.d[0] == ')' || t.d[0] == '$'))
					buf_c(&b, t.d[0]);
				else if (t.n && raras)
					buf_pon(&b, t.d, t.n);
				else if (t.n) {
					buf_c(&b, '"');
					buf_pon(&b, t.d, t.n);
					buf_c(&b, '"');
				}
				buf_suelta(&t);
			} else if (xml_es(k, "number:text-content")) {
				buf_c(&b, '@');
			} else if (fecha) {
				int largo_ = (v = xml_atr(k, "number:style")) && iguales(v, "long");

				if (xml_es(k, "number:day"))
					buf_txt(&b, largo_ ? "dd" : "d");
				else if (xml_es(k, "number:month"))
					buf_txt(&b, cierto(xml_atr(k, "number:textual")) ? (largo_ ? "mmmm" : "mmm") : (largo_ ? "mm" : "m"));
				else if (xml_es(k, "number:year"))
					buf_txt(&b, largo_ ? "yyyy" : "yy");
				else if (xml_es(k, "number:day-of-week"))
					buf_txt(&b, largo_ ? "dddd" : "ddd");
				else if (xml_es(k, "number:hours"))
					buf_txt(&b, xml_es(s, "number:time-style") && !cierto(xml_atr(s, "number:truncate-on-overflow")) && xml_atr(s, "number:truncate-on-overflow") ? (largo_ ? "[hh]" : "[h]") : (largo_ ? "hh" : "h"));
				else if (xml_es(k, "number:minutes"))
					buf_txt(&b, largo_ ? "mm" : "m");
				else if (xml_es(k, "number:seconds")) {
					int d = (int)num_atr(k, "number:decimal-places", 0), j;

					buf_txt(&b, largo_ ? "ss" : "s");
					if (d > 0 && d < 6) {
						buf_c(&b, '.');
						for (j = 0; j < d; j++)
							buf_c(&b, '0');
					}
				} else if (xml_es(k, "number:am-pm"))
					buf_txt(&b, "AM/PM");
			}
		}
	buf_c(&b, 0);
	n = so_repide(L->nu, (L->n_nu + 1) * sizeof(estilo_num));
	if (n) {
		L->nu = n;
		copia(n[L->n_nu].nombre, nom, sizeof n[0].nombre);
		copia(n[L->n_nu].codigo, b.d && b.d[0] ? (char *)b.d : "General", sizeof n[0].codigo);
		/* (un porcentaje sin el «%» escrito: se le pone) */
		if (xml_es(s, "number:percentage-style") && !strchr(n[L->n_nu].codigo, '%'))
			anade(n[L->n_nu].codigo, "%", sizeof n[0].codigo);
		L->n_nu++;
	}
	buf_suelta(&b);
}

static int borde(const char *s, u8 *b, u32 *c)
{
	double w;

	if (!s || iguales(s, "none") || iguales(s, "hidden")) {
		*b = BR_NADA;
		return 1;
	}
	w = medida_pt(s);
	*b = busca(s, "double") ? BR_DOBLE : busca(s, "dotted") ? BR_PUNTOS : (busca(s, "dash") ? BR_GUIONES : w > 0 && w < 1.2 ? BR_FINO : w < 2.6 ? BR_MEDIO : BR_GRUESO);
	{
		const char *h = strchr(s, '#');

		*c = h ? color(h, 0) : 0;
	}
	return 1;
}

static void lee_estilo(lector *L, const xml_nodo *s)
{
	const char *nom = xml_atr(s, "style:name"), *fam = xml_atr(s, "style:family"), *v;
	estilo_ods *e, *n;
	const xml_nodo *p;

	if (!nom || !fam)
		return;
	n = so_repide(L->es, (L->n_es + 1) * sizeof(estilo_ods));
	if (!n)
		return;
	L->es = n;
	e = &n[L->n_es++];
	memset(e, 0, sizeof *e);
	estilo_base(&e->e);
	copia(e->nombre, nom, sizeof e->nombre);
	copia(e->padre, xml_atr(s, "style:parent-style-name") ? xml_atr(s, "style:parent-style-name") : "", sizeof e->padre);
	copia(e->datos, xml_atr(s, "style:data-style-name") ? xml_atr(s, "style:data-style-name") : "", sizeof e->datos);
	e->familia = iguales(fam, "table-column") ? 1 : iguales(fam, "table-row") ? 2 : iguales(fam, "table-cell") ? 0 : 3;
	e->ancho_cm = e->alto_cm = -1;
	if ((p = xml_hijo(s, "style:table-column-properties")))
		e->ancho_cm = medida_cm(xml_atr(p, "style:column-width"));
	if ((p = xml_hijo(s, "style:table-row-properties"))) {
		e->alto_cm = medida_cm(xml_atr(p, "style:row-height"));
		e->alto_optimo = cierto(xml_atr(p, "style:use-optimal-row-height"));
	}
	if ((p = xml_hijo(s, "style:text-properties"))) {
		if ((v = xml_atr(p, "style:font-name")) || (v = xml_atr(p, "fo:font-family"))) {
			char t[64];
			u32 k = 0;

			/* (sin comillas) */
			for (; *v && k + 1 < sizeof t; v++)
				if (*v != '\'' && *v != '"')
					t[k++] = *v;
			t[k] = 0;
			copia(e->e.familia, t, sizeof e->e.familia);
			e->que |= EQ_FAM;
		}
		if ((v = xml_atr(p, "fo:font-size")) && medida_pt(v) > 0) {
			e->e.tam = (u16)(medida_pt(v) * 2 + 0.5);
			if (e->e.tam < 2 || e->e.tam > 800)
				e->e.tam = CU_TAM;
			e->que |= EQ_TAM;
		}
		if ((v = xml_atr(p, "fo:font-weight"))) {
			e->e.negrita = iguales(v, "bold") || (v[0] >= '6' && v[0] <= '9');
			e->que |= EQ_NEG;
		}
		if ((v = xml_atr(p, "fo:font-style"))) {
			e->e.cursiva = iguales(v, "italic") || iguales(v, "oblique");
			e->que |= EQ_CUR;
		}
		if ((v = xml_atr(p, "style:text-underline-style"))) {
			e->e.subrayado = !iguales(v, "none");
			e->que |= EQ_SUB;
		}
		if ((v = xml_atr(p, "style:text-line-through-style"))) {
			e->e.tachado = !iguales(v, "none");
			e->que |= EQ_TACH;
		}
		if ((v = xml_atr(p, "fo:color"))) {
			e->e.color = color(v, CU_AUTO);
			e->que |= EQ_COLOR;
		}
	}
	if ((p = xml_hijo(s, "style:table-cell-properties"))) {
		static const char *const lados[4] = { "fo:border-left", "fo:border-top", "fo:border-right", "fo:border-bottom" };
		int j;

		if ((v = xml_atr(p, "fo:background-color"))) {
			if (iguales(v, "transparent"))
				e->e.trama = TR_NADA;
			else {
				e->e.trama = TR_SOLIDO;
				e->e.fondo = color(v, 0xFFFFFF);
			}
			e->que |= EQ_FONDO;
		}
		if ((v = xml_atr(p, "fo:border"))) {
			for (j = 0; j < 4; j++)
				borde(v, &e->e.borde[j], &e->e.color_borde[j]);
			e->que |= EQ_BORDE;
		}
		for (j = 0; j < 4; j++)
			if ((v = xml_atr(p, lados[j]))) {
				borde(v, &e->e.borde[j], &e->e.color_borde[j]);
				e->que |= EQ_BORDE;
			}
		if ((v = xml_atr(p, "style:vertical-align"))) {
			e->e.alin_v = iguales(v, "top") ? AV_ARRIBA : iguales(v, "middle") ? AV_CENTRO : AV_ABAJO;
			e->que |= EQ_AV;
		}
		if ((v = xml_atr(p, "fo:wrap-option"))) {
			e->e.ajustar = iguales(v, "wrap");
			e->que |= EQ_AJ;
		}
		if ((v = xml_atr(p, "style:rotation-angle"))) {
			e->e.girado = (u8)((u32)numero(v, NULL) % 256);
		}
	}
	if ((p = xml_hijo(s, "style:paragraph-properties"))) {
		if ((v = xml_atr(p, "fo:text-align"))) {
			e->e.alin_h = (iguales(v, "start") || iguales(v, "left")) ? AH_IZQ : iguales(v, "center") ? AH_CENTRO :
				      (iguales(v, "end") || iguales(v, "right")) ? AH_DER : iguales(v, "justify") ? AH_JUSTIFICAR : AH_GENERAL;
			e->que |= EQ_AH;
		}
		if ((v = xml_atr(p, "fo:margin-left")) && medida_pt(v) > 0) {
			e->e.sangria = (u8)(medida_pt(v) / 10 + 0.5);
			e->que |= EQ_SANG;
		}
	}
}

static estilo_ods *busca_estilo(lector *L, const char *nom, int familia)
{
	u32 i;

	if (!nom)
		return NULL;
	for (i = L->n_es; i-- > 0;)
		if (L->es[i].familia == familia && iguales(L->es[i].nombre, nom))
			return &L->es[i];
	return NULL;
}

/* el estilo de Cuentas de un estilo de celda (con lo de su padre) */
static u16 estilo_de(lector *L, const char *nom)
{
	estilo_ods *e = busca_estilo(L, nom, 0), *cad[8];
	int n = 0, j;
	estilo r;
	const char *datos = NULL;

	if (!e)
		return 0;
	if (e->hecho)
		return e->id;
	/* la cadena de padres (hasta 8) */
	while (e && n < 8) {
		cad[n++] = e;
		e = e->padre[0] ? busca_estilo(L, e->padre, 0) : NULL;
		{
			int k;

			for (k = 0; k < n && e; k++)
				if (cad[k] == e)
					e = NULL;
		}
	}
	estilo_base(&r);
	for (j = n - 1; j >= 0; j--) {
		const estilo *s = &cad[j]->e;
		u32 q = cad[j]->que;

		if (q & EQ_FAM)
			copia(r.familia, s->familia, sizeof r.familia);
		if (q & EQ_TAM)
			r.tam = s->tam;
		if (q & EQ_NEG)
			r.negrita = s->negrita;
		if (q & EQ_CUR)
			r.cursiva = s->cursiva;
		if (q & EQ_SUB)
			r.subrayado = s->subrayado;
		if (q & EQ_TACH)
			r.tachado = s->tachado;
		if (q & EQ_COLOR)
			r.color = s->color;
		if (q & EQ_FONDO) {
			r.trama = s->trama;
			r.fondo = s->fondo;
		}
		if (q & EQ_BORDE) {
			memcpy(r.borde, s->borde, sizeof r.borde);
			memcpy(r.color_borde, s->color_borde, sizeof r.color_borde);
		}
		if (q & EQ_AH)
			r.alin_h = s->alin_h;
		if (q & EQ_AV)
			r.alin_v = s->alin_v;
		if (q & EQ_AJ)
			r.ajustar = s->ajustar;
		if (q & EQ_SANG)
			r.sangria = s->sangria;
		r.girado = s->girado ? s->girado : r.girado;
		if (cad[j]->datos[0])
			datos = cad[j]->datos;
	}
	if (datos) {
		u32 i;

		for (i = 0; i < L->n_nu; i++)
			if (iguales(L->nu[i].nombre, datos))
				r.numfmt = lib_fmt_id(L->l, L->nu[i].codigo);
	}
	e = cad[0];
	e->id = lib_estilo_id(L->l, &r);
	e->hecho = 1;
	return e->id;
}

static void lee_estilos_de(lector *L, const xml_nodo *cont)
{
	const xml_nodo *k;

	for (k = xml_primero(cont); k; k = xml_sig(k, NULL)) {
		if (xml_es(k, "style:style"))
			lee_estilo(L, k);
		else if (xml_es(k, "number:number-style") || xml_es(k, "number:currency-style") || xml_es(k, "number:percentage-style") ||
			 xml_es(k, "number:date-style") || xml_es(k, "number:time-style") || xml_es(k, "number:text-style") ||
			 xml_es(k, "number:boolean-style"))
			lee_estilo_num(L, k);
	}
}

/* una fecha (2026-11-22T14:30:00) o una hora (PT14H30M00S) */
static double fecha_ods(const char *s)
{
	int a = 0, m = 0, d = 0, h = 0, mi = 0;
	double se = 0, r;
	const char *p = s;

	a = (int)numero(p, &p);
	if (*p == '-')
		p++;
	m = (int)numero(p, &p);
	if (*p == '-')
		p++;
	d = (int)numero(p, &p);
	r = fecha_serie(a, m < 1 ? 1 : m, d < 1 ? 1 : d);
	if (*p == 'T') {
		p++;
		h = (int)numero(p, &p);
		if (*p == ':')
			p++;
		mi = (int)numero(p, &p);
		if (*p == ':') {
			p++;
			num_lee_c(p, &se, &p);
		}
		r += (h * 3600.0 + mi * 60.0 + se) / 86400.0;
	}
	return r;
}

static double hora_ods(const char *s)
{
	double t = 0, v;
	const char *p = s;

	if (*p == '-')
		p++;
	if (*p == 'P')
		p++;
	while (*p) {
		if (*p == 'T') {
			p++;
			continue;
		}
		if (!num_lee_c(p, &v, &p))
			break;
		if (*p == 'D')
			t += v;
		else if (*p == 'H')
			t += v / 24;
		else if (*p == 'M')
			t += v / 1440;
		else if (*p == 'S')
			t += v / 86400;
		else
			break;
		p++;
	}
	return t;
}

static void lee_tabla(lector *L, hoja *h, const xml_nodo *t)
{
	const xml_nodo *pila[16];
	const xml_nodo *k;
	int np = 0;
	u32 f = 0, c = 0;
	libro *l = L->l;

	/* las columnas (también las de dentro de table:table-header-columns / table:table-columns) */
	k = xml_primero(t);
	while (k || np) {
		if (!k) {
			k = xml_sig(pila[--np], NULL);
			continue;
		}
		if ((xml_es(k, "table:table-header-columns") || xml_es(k, "table:table-columns") || xml_es(k, "table:table-column-group")) && np < 16) {
			pila[np++] = k;
			k = xml_primero(k);
			continue;
		}
		if (xml_es(k, "table:table-column")) {
			double rep = num_atr(k, "table:number-columns-repeated", 1);
			u32 n = rep < 1 ? 1 : rep > CU_COLS ? CU_COLS : (u32)rep, i;
			estilo_ods *e = busca_estilo(L, xml_atr(k, "table:style-name"), 1);
			int oc = xml_atr(k, "table:visibility") && !iguales(xml_atr(k, "table:visibility"), "visible");
			u16 de = xml_atr(k, "table:default-cell-style-name") ? estilo_de(L, xml_atr(k, "table:default-cell-style-name")) : 0;

			/* (las de un grupo grande de repetidas, el ancho de siempre de la hoja) */
			if (e && e->ancho_cm > 0 && n >= 64) {
				u32 a = (u32)(e->ancho_cm * PX_CM + 0.5), j;

				if (a >= 4 && a <= 2000) {
					h->ancho_def = a;
					for (j = c; j < CU_COLS; j++)
						h->ancho[j] = (u16)a;
				}
			}
			for (i = 0; i < n && c < CU_COLS; i++, c++) {
				if (e && e->ancho_cm > 0)
					hj_pon_ancho(h, c, (u32)(e->ancho_cm * PX_CM + 0.5));
				if (oc)
					h->oculta_c[c] = 1;
				if (de)
					h->estilo_col[c] = de;
			}
		}
		k = xml_sig(k, NULL);
	}
	/* las filas */
	np = 0;
	k = xml_primero(t);
	while ((k || np) && f < CU_FILAS) {
		double rep;
		u32 n, i;
		const xml_nodo *cel;
		int hay = 0;

		if (!k) {
			k = xml_sig(pila[--np], NULL);
			continue;
		}
		if ((xml_es(k, "table:table-header-rows") || xml_es(k, "table:table-rows") || xml_es(k, "table:table-row-group")) && np < 16) {
			pila[np++] = k;
			k = xml_primero(k);
			continue;
		}
		if (!xml_es(k, "table:table-row")) {
			k = xml_sig(k, NULL);
			continue;
		}
		rep = num_atr(k, "table:number-rows-repeated", 1);
		n = rep < 1 ? 1 : rep > CU_FILAS ? CU_FILAS : (u32)rep;
		/* ¿tiene algo (no solo estilos)? */
		for (cel = xml_primero(k); cel; cel = xml_sig(cel, NULL))
			if (cel->hijo || xml_atr(cel, "office:value-type") || xml_atr(cel, "table:formula") || xml_atr(cel, "table:number-columns-spanned"))
				hay = 1;
		{
			estilo_ods *e = busca_estilo(L, xml_atr(k, "table:style-name"), 2);
			int oc = xml_atr(k, "table:visibility") && !iguales(xml_atr(k, "table:visibility"), "visible");
			u32 cuantas = (hay || n <= 1024) ? n : 0;	/* (las de un millón vacías, sin alto ni nada) */

			for (i = 0; i < cuantas && f + i < CU_FILAS; i++) {
				if (e && e->alto_cm > 0 && !e->alto_optimo) {
					u32 al = (u32)(e->alto_cm * PX_CM + 0.5);

					if (al != CU_ALTO)
						hj_pon_alto(h, f + i, al);
				}
				if (oc)
					hj_oculta_fila(h, f + i, 1);
			}
		}
		if (!hay) {
			f += n;
			k = xml_sig(k, NULL);
			continue;
		}
		/* las celdas (la fila repetida, lo que diga, hasta 1024 veces) */
		for (i = 0; i < n && i < 1024 && f < CU_FILAS; i++, f++) {
			c = 0;
			for (cel = xml_primero(k); cel && c < CU_COLS; cel = xml_sig(cel, NULL)) {
				double rc = num_atr(cel, "table:number-columns-repeated", 1);
				u32 nc = rc < 1 ? 1 : rc > CU_COLS ? CU_COLS : (u32)rc, j;
				int cubierta = xml_es(cel, "table:covered-table-cell");
				const char *ty = xml_atr(cel, "office:value-type"), *fo = xml_atr(cel, "table:formula");
				const char *sn = xml_atr(cel, "table:style-name");
				u16 est = sn ? estilo_de(L, sn) : 0;
				int con = !cubierta && (ty || fo || xml_hijo(cel, "text:p"));
				xml_nodo *an = xml_hijo(cel, "office:annotation");

				if (!xml_es(cel, "table:table-cell") && !cubierta)
					continue;
				if (!con && !est && !an && !xml_atr(cel, "table:number-columns-spanned")) {
					c += nc;
					continue;
				}
				for (j = 0; j < nc && c < CU_COLS; j++, c++) {
					if (con) {
						formula *fm = NULL;

						if (fo) {
							char e[200];
							u32 d;

							fm = fm_lee(l, h, fo, largo(fo), &CU_ODS, e, sizeof e, &d);
						}
						if (fm && fm->n == 1 && fm->t[0].t == T_ERR) {
							/* (un error escrito: «of:=#DIV/0!») */
							valor v = { V_ERR, 0, 0, NULL };

							v.e = (u8)fm->t[0].u.x.i;
							fm_suelta(fm);
							lib_pon_valor(l, h, f, c, &v);
						} else if (fm)
							lib_pon_formula(l, h, f, c, fm);
						else if (ty && (iguales(ty, "float") || iguales(ty, "percentage") || iguales(ty, "currency")))
							lib_pon_numero(l, h, f, c, num_atr(cel, "office:value", 0));
						else if (ty && iguales(ty, "date") && xml_atr(cel, "office:date-value"))
							lib_pon_numero(l, h, f, c, fecha_ods(xml_atr(cel, "office:date-value")));
						else if (ty && iguales(ty, "time") && xml_atr(cel, "office:time-value"))
							lib_pon_numero(l, h, f, c, hora_ods(xml_atr(cel, "office:time-value")));
						else if (ty && iguales(ty, "boolean")) {
							valor v = { V_LOG, 0, 0, NULL };

							v.n = cierto(xml_atr(cel, "office:boolean-value"));
							lib_pon_valor(l, h, f, c, &v);
						} else {
							char *s = xml_atr(cel, "office:string-value") ? duplica(xml_atr(cel, "office:string-value")) : texto_celda(cel);

							if (s && s[0])
								lib_pon_texto(l, h, f, c, s);
							so_suelta(s);
						}
					}
					if (est)
						lib_pon_estilo(l, h, f, c, est);
					if (an && j == 0) {
						char *s = texto_celda(an);
						bufer a = { 0 };

						xml_texto(xml_hijo(an, "dc:creator"), &a);
						buf_c(&a, 0);
						hj_pon_comentario(h, f, c, s, a.d ? (char *)a.d : "");
						buf_suelta(&a);
						so_suelta(s);
					}
					if (j == 0 && !cubierta) {
						double cs = num_atr(cel, "table:number-columns-spanned", 1), rs = num_atr(cel, "table:number-rows-spanned", 1);

						if (cs > 1 || rs > 1) {
							u32 c1 = c + (u32)(cs > CU_COLS ? CU_COLS : cs) - 1, f1 = f + (u32)(rs > CU_FILAS ? CU_FILAS : rs) - 1;

							hj_combina(h, cu_rango(f, c, f1 < CU_FILAS ? f1 : CU_FILAS - 1, c1 < CU_COLS ? c1 : CU_COLS - 1));
						}
					}
				}
			}
		}
		if (n > 1024)
			f += n - 1024;
		k = xml_sig(k, NULL);
	}
	/* el área de impresión: «Hoja1.A1:Hoja1.C6» */
	{
		const char *pr = xml_atr(t, "table:print-ranges");

		if (pr) {
			char b[200];
			u32 i, o = 0;
			int dentro = 0;
			const char *fin;

			/* sin la hoja: lo de detrás de cada punto */
			for (i = 0; pr[i] && !(pr[i] == ' ' && !dentro) && o + 1 < sizeof b; i++) {
				if (pr[i] == '\'') {
					dentro = !dentro;
					continue;
				}
				if (dentro)
					continue;
				if (pr[i] == '.') {
					/* quita lo que había de la hoja */
					while (o && b[o - 1] != ':')
						o--;
					continue;
				}
				if (pr[i] != '$')
					b[o++] = pr[i];
			}
			b[o] = 0;
			if (cu_lee_rango(b, &h->area, &fin))
				h->hay_area = 1;
		}
	}
}

/* lo de settings.xml: los paneles, la celda activa y la hoja activa */
static void lee_ajustes(lector *L, zip_lector *z)
{
	u32 n;
	u8 *d = zip_saca(z, "settings.xml", &n);
	xml_doc *x;
	const xml_nodo *k;
	const xml_nodo *pila[24];
	int np = 0;

	if (!d)
		return;
	x = xml_lee((const char *)d, n);
	so_suelta(d);
	if (!x)
		return;
	/* (se recorre todo buscando los config-item-map-named «Tables» y el «ActiveTable») */
	k = xml_raiz(x);
	while (k) {
		if (xml_es(k, "config:config-item-map-named") && xml_atr(k, "config:name") && iguales(xml_atr(k, "config:name"), "Tables")) {
			const xml_nodo *e;

			for (e = xml_hijo(k, "config:config-item-map-entry"); e; e = xml_sig(e, "config:config-item-map-entry")) {
				const char *nom = xml_atr(e, "config:name");
				hoja *h = nom ? lib_hoja_nombre(L->l, nom, largo(nom)) : NULL;
				const xml_nodo *it;
				long hs = 0, vs = 0, hp = 0, vp = 0, cx = -1, cy = -1;

				if (!h)
					continue;
				for (it = xml_hijo(e, "config:config-item"); it; it = xml_sig(it, "config:config-item")) {
					const char *in = xml_atr(it, "config:name");
					bufer b = { 0 };
					long v;

					if (!in)
						continue;
					xml_texto(it, &b);
					buf_c(&b, 0);
					v = b.d ? numero((char *)b.d, NULL) : 0;
					buf_suelta(&b);
					if (iguales(in, "HorizontalSplitMode"))
						hs = v;
					else if (iguales(in, "VerticalSplitMode"))
						vs = v;
					else if (iguales(in, "HorizontalSplitPosition"))
						hp = v;
					else if (iguales(in, "VerticalSplitPosition"))
						vp = v;
					else if (iguales(in, "CursorPositionX"))
						cx = v;
					else if (iguales(in, "CursorPositionY"))
						cy = v;
				}
				if (hs == 2 && hp > 0 && hp < (long)CU_COLS)
					h->inm_c = (u32)hp;
				if (vs == 2 && vp > 0 && vp < (long)CU_FILAS)
					h->inm_f = (u32)vp;
				if (cx >= 0 && cx < (long)CU_COLS && cy >= 0 && cy < (long)CU_FILAS) {
					h->cur_c = (u32)cx;
					h->cur_f = (u32)cy;
					h->sel = cu_rango(h->cur_f, h->cur_c, h->cur_f, h->cur_c);
				}
			}
		} else if (xml_es(k, "config:config-item") && xml_atr(k, "config:name") && iguales(xml_atr(k, "config:name"), "ActiveTable")) {
			bufer b = { 0 };
			hoja *h;

			xml_texto(k, &b);
			h = b.d ? lib_hoja_nombre(L->l, (char *)b.d, b.n) : NULL;
			if (h)
				L->l->activa = (u32)lib_hoja_indice(L->l, h->id);
			buf_suelta(&b);
		}
		/* el siguiente (en profundidad, sin recursión) */
		if (k->hijo && xml_primero(k) && np < 24) {
			pila[np++] = k;
			k = xml_primero(k);
			continue;
		}
		while (k && !xml_sig(k, NULL)) {
			k = np ? pila[--np] : NULL;
		}
		if (k)
			k = xml_sig(k, NULL);
	}
	xml_suelta(x);
}

libro *ods_lee(const u8 *d, u32 n, char *err, u32 cabe)
{
	zip_lector *z = zip_abre(d, n);
	lector L;
	xml_doc *x, *st = NULL;
	xml_nodo *r, *cuerpo, *t;
	u32 i;

	memset(&L, 0, sizeof L);
	if (!z) {
		copia(err, "No es un .ODS (no es un ZIP que se pueda leer)", cabe);
		return NULL;
	}
	{
		u32 m;
		u8 *c = zip_saca(z, "content.xml", &m);

		if (!c) {
			zip_cierra(z);
			copia(err, "El .ODS no tiene content.xml", cabe);
			return NULL;
		}
		x = xml_lee((const char *)c, m);
		so_suelta(c);
		c = zip_saca(z, "styles.xml", &m);
		if (c) {
			st = xml_lee((const char *)c, m);
			so_suelta(c);
		}
	}
	if (!x) {
		zip_cierra(z);
		xml_suelta(st);
		copia(err, "El content.xml del .ODS está mal", cabe);
		return NULL;
	}
	L.l = lib_nuevo(0);
	if (!L.l) {
		xml_suelta(x);
		xml_suelta(st);
		zip_cierra(z);
		copia(err, "No hay memoria", cabe);
		return NULL;
	}
	r = xml_raiz(x);
	if (st) {
		lee_estilos_de(&L, xml_hijo(xml_raiz(st), "office:styles"));
		lee_estilos_de(&L, xml_hijo(xml_raiz(st), "office:automatic-styles"));
	}
	lee_estilos_de(&L, xml_hijo(r, "office:automatic-styles"));
	cuerpo = xml_camino(r, "office:body/office:spreadsheet");
	/* las hojas, primero todas */
	for (t = xml_hijo(cuerpo, "table:table"); t; t = xml_sig(t, "table:table")) {
		const char *nom = xml_atr(t, "table:name");
		char nm[96];

		copia(nm, nom ? nom : "", sizeof nm);
		if (!nm[0] || lib_hoja_nombre(L.l, nm, largo(nm)))
			lib_nombre_libre(L.l, nm, sizeof nm);
		if (!lib_hoja_nueva(L.l, nm, -1))
			break;
	}
	if (!L.l->n_h)
		lib_hoja_nueva(L.l, "Hoja1", -1);
	i = 0;
	for (t = xml_hijo(cuerpo, "table:table"); t && i < L.l->n_h; t = xml_sig(t, "table:table"), i++)
		lee_tabla(&L, L.l->h[i], t);
	/* los nombres */
	{
		xml_nodo *ne = xml_hijo(cuerpo, "table:named-expressions"), *k;

		for (k = xml_primero(ne); k; k = xml_sig(k, NULL)) {
			const char *nom = xml_atr(k, "table:name");
			char txt[600], e[200];
			u32 dd;
			formula *fm = NULL;

			if (!nom)
				continue;
			if (xml_es(k, "table:named-range") && xml_atr(k, "table:cell-range-address")) {
				fmt(txt, sizeof txt, "[%s]", xml_atr(k, "table:cell-range-address"));
				fm = fm_lee(L.l, L.l->h[0], txt, largo(txt), &CU_ODS, e, sizeof e, &dd);
			} else if (xml_es(k, "table:named-expression") && xml_atr(k, "table:expression"))
				fm = fm_lee(L.l, L.l->h[0], xml_atr(k, "table:expression"), largo(xml_atr(k, "table:expression")), &CU_ODS, e, sizeof e, &dd);
			if (fm) {
				u32 j;

				for (j = 0; j < fm->n; j++)
					if (fm->t[j].t == T_REF || fm->t[j].t == T_RANGO)
						fm->t[j].a |= RA_HOJA;
				lib_pon_nombre(L.l, nom, fm);
			}
		}
	}
	/* el autofiltro (el rango de la base de datos que enseña los botones) */
	{
		xml_nodo *db = xml_hijo(cuerpo, "table:database-ranges"), *k;

		for (k = xml_hijo(db, "table:database-range"); k; k = xml_sig(k, "table:database-range")) {
			const char *tr = xml_atr(k, "table:target-range-address");
			char txt[600], e[200];
			u32 dd;
			formula *fm;

			if (!tr || !cierto(xml_atr(k, "table:display-filter-buttons")))
				continue;
			fmt(txt, sizeof txt, "[%s]", tr);
			fm = fm_lee(L.l, L.l->h[0], txt, largo(txt), &CU_ODS, e, sizeof e, &dd);
			if (fm && fm->n == 1 && (fm->t[0].t == T_RANGO || fm->t[0].t == T_REF)) {
				hoja *h = lib_hoja_id(L.l, fm->t[0].h);

				if (h) {
					h->hay_filtro = 1;
					h->filtro = cu_rango((u32)fm->t[0].u.r.f0, fm->t[0].u.r.c0, (u32)fm->t[0].u.r.f1, fm->t[0].u.r.c1);
				}
			}
			fm_suelta(fm);
		}
	}
	lee_ajustes(&L, z);
	xml_suelta(x);
	xml_suelta(st);
	zip_cierra(z);
	so_suelta(L.nu);
	so_suelta(L.es);
	return L.l;
}

/* ===================================================================== */
/*  Escribir                                                             */
/* ===================================================================== */
static void pon_num(bufer *b, double x)
{
	char t[48];

	num_a_texto_c(x, t, sizeof t);
	buf_txt(b, t);
}

static void pon_texto_p(bufer *b, const char *s)
{
	/* cada línea, un text:p; los espacios seguidos, text:s; el tabulador, text:tab */
	const char *p = s;

	buf_txt(b, "<text:p>");
	while (*p) {
		if (*p == '\n') {
			buf_txt(b, "</text:p><text:p>");
			p++;
			continue;
		}
		if (*p == '\t') {
			buf_txt(b, "<text:tab/>");
			p++;
			continue;
		}
		if (*p == ' ' && (p == s || p[-1] == ' ' || p[-1] == '\n' || p[1] == 0 || p[1] == ' ')) {
			u32 k = 0;

			while (p[k] == ' ')
				k++;
			if (k == 1 && p != s && p[-1] != '\n' && p[1] != 0) {
				buf_c(b, ' ');
				p++;
				continue;
			}
			buf_fmt(b, "<text:s text:c=\"%u\"/>", k);
			p += k;
			continue;
		}
		{
			const char *q = p;

			while (*q && *q != '\n' && *q != '\t' && *q != ' ')
				q++;
			if (q == p)
				q++;
			buf_xml(b, p, (u32)(q - p));
			p = q;
		}
	}
	buf_txt(b, "</text:p>");
}

/* un estilo de número de ODS de un código (lo de la primera sección) */
static void pon_estilo_num(bufer *b, const char *nombre, const char *cod)
{
	u32 i = 0, n = largo(cod);
	int cat = fmt_categoria(cod), fecha = fmt_es_fecha(cod), hay_h = 0;
	const char *el;
	bufer lit = { 0 };

	if (cat == CAT_TEXTO || iguales(cod, "@")) {
		buf_fmt(b, "<number:text-style style:name=\"%s\"><number:text-content/></number:text-style>", nombre);
		return;
	}
	el = fecha ? (cat == CAT_HORA ? "number:time-style" : "number:date-style") : cat == CAT_PORCENTAJE ? "number:percentage-style" :
	     cat == CAT_MONEDA ? "number:currency-style" : "number:number-style";
	buf_fmt(b, "<%s style:name=\"%s\"", el, nombre);
	if (fecha && (busca(cod, "[h") || busca(cod, "[H")))
		buf_txt(b, " number:truncate-on-overflow=\"false\"");
	buf_txt(b, ">");
#define SUELTA_LIT() do { if (lit.n) { buf_txt(b, "<number:text>"); buf_xml(b, (char *)lit.d, lit.n); buf_txt(b, "</number:text>"); lit.n = 0; } } while (0)
	while (i < n && cod[i] != ';') {
		char c = cod[i];
		u32 k = 1;

		while (i + k < n && (cod[i + k] | 32) == (c | 32) && ((c | 32) >= 'a' && (c | 32) <= 'z'))
			k++;
		if (c == '"') {
			u32 j = i + 1;

			while (j < n && cod[j] != '"')
				j++;
			/* (lo de entre comillas, un texto: al leerlo vuelve entre comillas) */
			SUELTA_LIT();
			buf_pon(&lit, cod + i + 1, j - i - 1);
			SUELTA_LIT();
			i = j + 1;
			continue;
		}
		if (c == '\\' && i + 1 < n) {
			buf_c(&lit, (u8)cod[i + 1]);
			i += 2;
			continue;
		}
		if (c == '[') {
			u32 j = i;

			while (j < n && cod[j] != ']')
				j++;
			if (fecha && (cod[i + 1] | 32) == 'h') {
				SUELTA_LIT();
				buf_txt(b, "<number:hours number:style=\"long\"/>");
				hay_h = 1;
			}
			i = j + 1;
			continue;
		}
		if (c == '_' || c == '*') {
			if (c == '_')
				buf_c(&lit, ' ');
			i += 2;
			continue;
		}
		if (!fecha && (c == '0' || c == '#' || c == '?' || c == ',' || (c == '.' && i + 1 < n && (cod[i + 1] == '0' || cod[i + 1] == '#')))) {
			/* el número */
			u32 j = i;
			int ent = 0, dec = 0, gr = 0, pt = 0, ex = 0, exd = 0;

			while (j < n && (cod[j] == '0' || cod[j] == '#' || cod[j] == '?' || cod[j] == ',' || cod[j] == '.' ||
					 ((cod[j] == 'E' || cod[j] == 'e') && j + 1 < n && (cod[j + 1] == '+' || cod[j + 1] == '-')))) {
				if (cod[j] == '.')
					pt = 1;
				else if (cod[j] == ',')
					gr = 1;
				else if (cod[j] == 'E' || cod[j] == 'e') {
					ex = 1;
					j += 2;
					while (j < n && (cod[j] == '0' || cod[j] == '#')) {
						exd++;
						j++;
					}
					break;
				} else if (cod[j] == '0') {
					if (pt)
						dec++;
					else
						ent++;
				} else if (pt)
					dec++;
				j++;
			}
			SUELTA_LIT();
			if (ex)
				buf_fmt(b, "<number:scientific-number number:decimal-places=\"%d\" number:min-integer-digits=\"%d\" number:min-exponent-digits=\"%d\"/>", dec, ent, exd);
			else
				buf_fmt(b, "<number:number number:decimal-places=\"%d\" number:min-integer-digits=\"%d\"%s/>", dec, ent, gr ? " number:grouping=\"true\"" : "");
			i = j;
			continue;
		}
		if (fecha) {
			int lc = c | 32;

			if (lc == 'd' || lc == 'y' || (lc == 'a' && k >= 2) || lc == 'h' || lc == 's' || lc == 'm') {
				SUELTA_LIT();
				if (lc == 'd')
					buf_txt(b, k >= 4 ? "<number:day-of-week number:style=\"long\"/>" : k == 3 ? "<number:day-of-week/>" :
						   k == 2 ? "<number:day number:style=\"long\"/>" : "<number:day/>");
				else if (lc == 'y' || lc == 'a')
					buf_txt(b, k >= 3 ? "<number:year number:style=\"long\"/>" : "<number:year/>");
				else if (lc == 'h') {
					buf_txt(b, k >= 2 ? "<number:hours number:style=\"long\"/>" : "<number:hours/>");
					hay_h = 1;
				} else if (lc == 's') {
					buf_txt(b, k >= 2 ? "<number:seconds number:style=\"long\"/>" : "<number:seconds/>");
				} else {
					/* m: minutos detrás de la hora o delante de los segundos */
					int min = 0;
					u32 j;

					if (hay_h && k <= 2)
						min = 1;
					for (j = i + k; j < n && !min && k <= 2; j++) {
						if ((cod[j] | 32) == 's')
							min = 1;
						if ((cod[j] | 32) == 'd' || (cod[j] | 32) == 'y' || (cod[j] | 32) == 'm')
							break;
					}
					if (min)
						buf_txt(b, k >= 2 ? "<number:minutes number:style=\"long\"/>" : "<number:minutes/>");
					else
						buf_txt(b, k >= 4 ? "<number:month number:style=\"long\" number:textual=\"true\"/>" : k == 3 ? "<number:month number:textual=\"true\"/>" :
							   k == 2 ? "<number:month number:style=\"long\"/>" : "<number:month/>");
					hay_h = 0;
				}
				i += k;
				continue;
			}
			if (lc == 'a' && i + 4 < n + 1 && ((cod[i + 1] | 32) == 'm')) {
				SUELTA_LIT();
				buf_txt(b, "<number:am-pm/>");
				i += 5;
				continue;
			}
		}
		if ((u8)c >= 0x80) {
			/* (el € sin comillas) */
			u32 j = i;

			while (j < n && (u8)cod[j] >= 0x80)
				j++;
			SUELTA_LIT();
			buf_txt(b, "<number:currency-symbol>");
			buf_xml(b, cod + i, j - i);
			buf_txt(b, "</number:currency-symbol>");
			i = j;
			continue;
		}
		if (c == '%' && cat == CAT_PORCENTAJE) {
			SUELTA_LIT();
			buf_txt(b, "<number:text>%</number:text>");
			i++;
			continue;
		}
		buf_c(&lit, (u8)c);
		i++;
	}
	SUELTA_LIT();
#undef SUELTA_LIT
	buf_fmt(b, "</%s>", el);
	buf_suelta(&lit);
}

static void pon_borde(bufer *b, const char *at, u8 br, u32 c)
{
	static const char *const anchos[BR_N] = { "", "0.06pt solid", "1.75pt solid", "2.5pt solid", "1.1pt double", "0.06pt dotted", "0.06pt dashed" };

	buf_fmt(b, " %s=\"", at);
	if (!br || br >= BR_N)
		buf_txt(b, "none");
	else
		buf_fmt(b, "%s #%06X", anchos[br], c & 0xFFFFFF);
	buf_txt(b, "\"");
}

static void pon_estilo_celda(bufer *b, libro *l, u32 i, int con_num)
{
	const estilo *e = lib_estilo(l, i);
	int j;
	static const char *const lados[4] = { "fo:border-left", "fo:border-top", "fo:border-right", "fo:border-bottom" };

	buf_fmt(b, "<style:style style:name=\"ce%u\" style:family=\"table-cell\" style:parent-style-name=\"Default\"", i);
	if (con_num)
		buf_fmt(b, " style:data-style-name=\"N%u\"", e->numfmt);
	buf_txt(b, "><style:table-cell-properties");
	buf_txt(b, e->trama != TR_NADA ? "" : " fo:background-color=\"transparent\"");
	if (e->trama != TR_NADA)
		buf_fmt(b, " fo:background-color=\"#%06X\"", e->fondo & 0xFFFFFF);
	for (j = 0; j < 4; j++)
		pon_borde(b, lados[j], e->borde[j], e->color_borde[j]);
	if (e->ajustar)
		buf_txt(b, " fo:wrap-option=\"wrap\"");
	if (e->alin_v)
		buf_fmt(b, " style:vertical-align=\"%s\"", e->alin_v == AV_ARRIBA ? "top" : "middle");
	if (e->girado)
		buf_fmt(b, " style:rotation-angle=\"%u\"", e->girado);
	buf_txt(b, "/>");
	if (e->alin_h || e->sangria) {
		buf_txt(b, "<style:paragraph-properties");
		if (e->alin_h)
			buf_fmt(b, " fo:text-align=\"%s\"", e->alin_h == AH_IZQ ? "start" : e->alin_h == AH_DER ? "end" :
				e->alin_h == AH_JUSTIFICAR ? "justify" : "center");
		if (e->sangria)
			buf_fmt(b, " fo:margin-left=\"%upt\"", e->sangria * 10);
		buf_txt(b, "/>");
	}
	buf_txt(b, "<style:text-properties");
	xml_pon_atr(b, "fo:font-family", e->familia);
	buf_txt(b, " fo:font-size=\"");
	pon_num(b, e->tam / 2.0);
	buf_txt(b, "pt\"");
	buf_fmt(b, " fo:font-weight=\"%s\" fo:font-style=\"%s\" style:text-underline-style=\"%s\" style:text-line-through-style=\"%s\"",
		e->negrita ? "bold" : "normal", e->cursiva ? "italic" : "normal", e->subrayado ? "solid" : "none", e->tachado ? "solid" : "none");
	if (e->subrayado)
		buf_txt(b, " style:text-underline-width=\"auto\" style:text-underline-color=\"font-color\"");
	if (e->color != CU_AUTO)
		buf_fmt(b, " fo:color=\"#%06X\"", e->color & 0xFFFFFF);
	buf_txt(b, "/></style:style>");
}

static void pon_dir(bufer *b, const hoja *h, rango r)
{
	char t[16], u[16];
	const char *p;
	int com = 0;

	for (p = h->nombre; *p; p++)
		if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_' || (u8)*p >= 0x80))
			com = 1;
	cu_ref_texto(r.f0, r.c0, t);
	cu_ref_texto(r.f1, r.c1, u);
	{
		bufer n = { 0 };

		if (com)
			buf_c(&n, '\'');
		for (p = h->nombre; *p; p++) {
			if (*p == '\'')
				buf_c(&n, '\'');
			buf_c(&n, (u8)*p);
		}
		if (com)
			buf_c(&n, '\'');
		buf_c(&n, 0);
		buf_fmt(b, "$%s.$", (char *)n.d);
		{
			u32 k = 0;

			while (t[k] >= 'A')
				buf_c(b, (u8)t[k++]);
			buf_fmt(b, "$%s:$%s.$", t + k, (char *)n.d);
			k = 0;
			while (u[k] >= 'A')
				buf_c(b, (u8)u[k++]);
			buf_fmt(b, "$%s", u + k);
		}
		buf_suelta(&n);
	}
}

/* el valor de una celda en sus atributos */
static void pon_valor(bufer *b, libro *l, const celda *x)
{
	const char *cod = lib_fmt(l, lib_estilo(l, x->estilo)->numfmt);
	int cat = fmt_categoria(cod);

	switch (x->v.t) {
	case V_NUM:
		if (cat == CAT_FECHA && x->v.n >= 0) {
			int a, m, d, h, mi, s;

			serie_fecha(x->v.n, &a, &m, &d);
			serie_hora(x->v.n, &h, &mi, &s);
			if (h || mi || s || x->v.n != so_suelo(x->v.n))
				buf_fmt(b, " office:value-type=\"date\" office:date-value=\"%04d-%02d-%02dT%02d:%02d:%02d\"", a, m, d, h, mi, s);
			else
				buf_fmt(b, " office:value-type=\"date\" office:date-value=\"%04d-%02d-%02d\"", a, m, d);
		} else if (cat == CAT_HORA && x->v.n >= 0) {
			double v = x->v.n * 24;
			u32 h = (u32)v;
			double r = (v - h) * 60;
			u32 mi = (u32)r;
			double s = (r - mi) * 60;

			if (s > 59.9995) {
				s = 0;
				mi++;
			}
			if (mi >= 60) {
				mi -= 60;
				h++;
			}
			buf_fmt(b, " office:value-type=\"time\" office:time-value=\"PT%02uH%02uM", h, mi);
			{
				char t[40];

				num_a_texto_c(num_redondea(s, 3), t, sizeof t);
				buf_fmt(b, "%sS\"", t);
			}
		} else {
			buf_fmt(b, " office:value-type=\"%s\" office:value=\"", cat == CAT_PORCENTAJE ? "percentage" : cat == CAT_MONEDA ? "currency" : "float");
			pon_num(b, x->v.n);
			buf_txt(b, "\"");
			if (cat == CAT_MONEDA)
				buf_txt(b, " office:currency=\"EUR\"");
		}
		break;
	case V_LOG:
		buf_fmt(b, " office:value-type=\"boolean\" office:boolean-value=\"%s\"", x->v.n != 0 ? "true" : "false");
		break;
	case V_TXT:
		if (x->f) {
			buf_txt(b, " office:value-type=\"string\"");
			xml_pon_atr(b, "office:string-value", x->v.s ? x->v.s : "");
		} else
			buf_txt(b, " office:value-type=\"string\"");
		break;
	case V_ERR:
		buf_txt(b, " office:value-type=\"string\"");
		break;
	}
}

static void escribe_tabla(libro *l, hoja *h, bufer *b, u32 *n_co, u32 *n_ro, bufer *est, int *col_usados)
{
	u32 uf, uc, f, c;

	(void)col_usados;
	hj_usado(h, &uf, &uc);
	buf_txt(b, "<table:table");
	xml_pon_atr(b, "table:name", h->nombre);
	if (h->hay_area) {
		buf_txt(b, " table:print-ranges=\"");
		{
			bufer t = { 0 };

			pon_dir(&t, h, h->area);
			buf_c(&t, 0);
			buf_xml(b, (char *)t.d, t.n - 1);
			buf_suelta(&t);
		}
		buf_txt(b, "\"");
	}
	buf_txt(b, ">");
	/* las columnas: seguidas las iguales */
	c = 0;
	while (c < CU_COLS) {
		u32 d = c;

		while (d + 1 < CU_COLS && h->ancho[d + 1] == h->ancho[c] && h->oculta_c[d + 1] == h->oculta_c[c] && h->estilo_col[d + 1] == h->estilo_col[c])
			d++;
		(*n_co)++;
		buf_fmt(est, "<style:style style:name=\"co%u\" style:family=\"table-column\"><style:table-column-properties fo:break-before=\"auto\" style:column-width=\"", *n_co);
		pon_num(est, num_redondea(h->ancho[c] / PX_CM, 5));
		buf_txt(est, "cm\"/></style:style>");
		buf_fmt(b, "<table:table-column table:style-name=\"co%u\"", *n_co);
		if (d > c)
			buf_fmt(b, " table:number-columns-repeated=\"%u\"", d - c + 1);
		if (h->oculta_c[c])
			buf_txt(b, " table:visibility=\"collapse\"");
		if (h->estilo_col[c])
			buf_fmt(b, " table:default-cell-style-name=\"ce%u\"/>", h->estilo_col[c]);
		else
			buf_txt(b, " table:default-cell-style-name=\"Default\"/>");
		c = d + 1;
	}
	/* las filas */
	if (uf == CU_SIN_FILA)
		uf = 0;
	/* (también las de alto, ocultas o con estilo de detrás de la última celda) */
	for (f = uf + 1; f < CU_FILAS; f++)
		if ((h->alto && h->alto[f]) || hj_fila_oculta(h, f) || (h->estilo_fila && h->estilo_fila[f]))
			uf = f;
	f = 0;
	while (f <= uf) {
		u32 g = f, k;
		int vacia = 1;

		/* las filas vacías seguidas, iguales: en una */
		for (c = 0; c <= uc && vacia; c++)
			if (hj_celda(h, f, c) || hj_combinada(h, f, c) >= 0 || hj_comentario(h, f, c))
				vacia = 0;
		if (vacia)
			while (g + 1 <= uf && (h->alto ? h->alto[g + 1] : 0) == (h->alto ? h->alto[f] : 0) && hj_fila_oculta(h, g + 1) == hj_fila_oculta(h, f) &&
			       (h->estilo_fila ? h->estilo_fila[g + 1] : 0) == (h->estilo_fila ? h->estilo_fila[f] : 0)) {
				int otra = 1;

				for (c = 0; c <= uc && otra; c++)
					if (hj_celda(h, g + 1, c) || hj_combinada(h, g + 1, c) >= 0 || hj_comentario(h, g + 1, c))
						otra = 0;
				if (!otra)
					break;
				g++;
			}
		buf_txt(b, "<table:table-row");
		if (h->alto && h->alto[f]) {
			(*n_ro)++;
			buf_fmt(est, "<style:style style:name=\"ro%u\" style:family=\"table-row\"><style:table-row-properties style:row-height=\"", *n_ro);
			pon_num(est, num_redondea(h->alto[f] / PX_CM, 5));
			buf_txt(est, "cm\" style:use-optimal-row-height=\"false\" fo:break-before=\"auto\"/></style:style>");
			buf_fmt(b, " table:style-name=\"ro%u\"", *n_ro);
		}
		if (g > f)
			buf_fmt(b, " table:number-rows-repeated=\"%u\"", g - f + 1);
		if (hj_fila_oculta(h, f))
			buf_txt(b, " table:visibility=\"collapse\"");
		if (h->estilo_fila && h->estilo_fila[f])
			buf_fmt(b, " table:default-cell-style-name=\"ce%u\"", h->estilo_fila[f]);
		buf_txt(b, ">");
		if (vacia) {
			buf_fmt(b, "<table:table-cell table:number-columns-repeated=\"%u\"/>", CU_COLS);
		} else {
			c = 0;
			while (c <= uc) {
				const celda *x = hj_celda(h, f, c);
				int m = hj_combinada(h, f, c);
				const comentario *cm = hj_comentario(h, f, c);
				int cubierta = m >= 0 && !(h->comb[m].f0 == f && h->comb[m].c0 == c);

				if (!x && !cm && m < 0) {
					/* vacías seguidas */
					u32 d = c;

					while (d + 1 <= uc && !hj_celda(h, f, d + 1) && hj_combinada(h, f, d + 1) < 0 && !hj_comentario(h, f, d + 1))
						d++;
					buf_txt(b, "<table:table-cell");
					if (d > c)
						buf_fmt(b, " table:number-columns-repeated=\"%u\"", d - c + 1);
					buf_txt(b, "/>");
					c = d + 1;
					continue;
				}
				buf_txt(b, cubierta ? "<table:covered-table-cell" : "<table:table-cell");
				if (x && x->estilo)
					buf_fmt(b, " table:style-name=\"ce%u\"", x->estilo);
				if (!cubierta && m >= 0)
					buf_fmt(b, " table:number-columns-spanned=\"%u\" table:number-rows-spanned=\"%u\"", (u32)h->comb[m].c1 - h->comb[m].c0 + 1, h->comb[m].f1 - h->comb[m].f0 + 1);
				if (x && x->f) {
					bufer w = { 0 };

					fm_escribe(l, x->f, h, &CU_ODS, &w);
					buf_c(&w, 0);
					buf_txt(b, " table:formula=\"of:=");
					buf_xml(b, w.d ? (char *)w.d : "", w.n ? w.n - 1 : 0);
					buf_txt(b, "\"");
					buf_suelta(&w);
				} else if (x && x->v.t == V_ERR) {
					buf_fmt(b, " table:formula=\"of:=%s\"", cu_error_texto(x->v.e == ER_CIRC ? ER_VALOR : x->v.e, 1));
				}
				if (x)
					pon_valor(b, l, x);
				buf_txt(b, ">");
				if (cm) {
					buf_txt(b, "<office:annotation><dc:creator>");
					buf_xml(b, cm->autor, largo(cm->autor));
					buf_txt(b, "</dc:creator>");
					pon_texto_p(b, cm->texto ? cm->texto : "");
					buf_txt(b, "</office:annotation>");
				}
				if (x && x->v.t != V_VACIO) {
					char t[2000];

					if (x->v.t == V_TXT && !x->f)
						pon_texto_p(b, x->v.s ? x->v.s : "");
					else {
						lib_texto_visible(l, x, t, sizeof t, NULL);
						pon_texto_p(b, t);
					}
				}
				buf_txt(b, cubierta ? "</table:covered-table-cell>" : "</table:table-cell>");
				c++;
			}
			if (uc + 1 < CU_COLS)
				buf_fmt(b, "<table:table-cell table:number-columns-repeated=\"%u\"/>", CU_COLS - uc - 1);
		}
		buf_txt(b, "</table:table-row>");
		(void)k;
		f = g + 1;
	}
	if (uf + 1 < CU_FILAS)
		buf_fmt(b, "<table:table-row table:number-rows-repeated=\"%u\"><table:table-cell table:number-columns-repeated=\"%u\"/></table:table-row>", CU_FILAS - uf - 1, CU_COLS);
	buf_txt(b, "</table:table>");
}

#define NS_DOC "xmlns:office=\"urn:oasis:names:tc:opendocument:xmlns:office:1.0\" xmlns:style=\"urn:oasis:names:tc:opendocument:xmlns:style:1.0\" " \
	"xmlns:text=\"urn:oasis:names:tc:opendocument:xmlns:text:1.0\" xmlns:table=\"urn:oasis:names:tc:opendocument:xmlns:table:1.0\" " \
	"xmlns:fo=\"urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0\" xmlns:dc=\"http://purl.org/dc/elements/1.1/\" " \
	"xmlns:meta=\"urn:oasis:names:tc:opendocument:xmlns:meta:1.0\" xmlns:number=\"urn:oasis:names:tc:opendocument:xmlns:datastyle:1.0\" " \
	"xmlns:svg=\"urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0\" xmlns:of=\"urn:oasis:names:tc:opendocument:xmlns:of:1.2\" " \
	"office:version=\"1.2\""

int ods_escribe(libro *l, bufer *out, char *err, u32 cabe)
{
	zip_escritor *z = zip_nuevo();
	bufer cont = { 0 }, est = { 0 }, b = { 0 };
	u32 i, n_co = 0, n_ro = 0;
	u8 *usa_num;
	int mal = 0;

	if (!z) {
		copia(err, "No hay memoria", cabe);
		return 1;
	}
	usa_num = so_pide(l->n_fmt + 1);
	if (!usa_num) {
		zip_deja(z);
		copia(err, "No hay memoria", cabe);
		return 1;
	}
	memset(usa_num, 0, l->n_fmt + 1);
	/* las hojas (los estilos de columna y fila van saliendo en est) */
	for (i = 0; i < l->n_h; i++)
		escribe_tabla(l, l->h[i], &cont, &n_co, &n_ro, &est, NULL);
	/* los nombres y el autofiltro */
	if (l->n_nom) {
		buf_txt(&cont, "<table:named-expressions>");
		for (i = 0; i < l->n_nom; i++) {
			bufer w = { 0 };

			fm_escribe(l, l->nom[i].f, NULL, &CU_ODS, &w);
			buf_c(&w, 0);
			buf_txt(&cont, "<table:named-expression");
			xml_pon_atr(&cont, "table:name", l->nom[i].nombre);
			buf_txt(&cont, " table:base-cell-address=\"$");
			{
				bufer t = { 0 };

				pon_dir(&t, l->h[0], cu_rango(0, 0, 0, 0));
				buf_c(&t, 0);
				/* (solo la primera celda: «$Hoja1.$A$1») */
				{
					char *dos = strchr((char *)t.d, ':');

					if (dos)
						*dos = 0;
				}
				buf_xml(&cont, (char *)t.d + 1, largo((char *)t.d + 1));
				buf_suelta(&t);
			}
			buf_txt(&cont, "\" table:expression=\"of:=");
			buf_xml(&cont, w.d ? (char *)w.d : "", w.n ? w.n - 1 : 0);
			buf_txt(&cont, "\"/>");
			buf_suelta(&w);
		}
		buf_txt(&cont, "</table:named-expressions>");
	}
	{
		int hay = 0;

		for (i = 0; i < l->n_h; i++)
			if (l->h[i]->hay_filtro) {
				if (!hay)
					buf_txt(&cont, "<table:database-ranges>");
				hay = 1;
				buf_fmt(&cont, "<table:database-range table:name=\"__Anonymous_Sheet_DB__%u\" table:target-range-address=\"", i);
				{
					bufer t = { 0 };

					pon_dir(&t, l->h[i], l->h[i]->filtro);
					buf_c(&t, 0);
					buf_xml(&cont, (char *)t.d, t.n - 1);
					buf_suelta(&t);
				}
				buf_txt(&cont, "\" table:display-filter-buttons=\"true\"/>");
			}
		if (hay)
			buf_txt(&cont, "</table:database-ranges>");
	}
	/* los estilos de celda y de número que se usan */
	for (i = 0; i < l->n_est; i++)
		if (lib_estilo(l, i)->numfmt < l->n_fmt)
			usa_num[lib_estilo(l, i)->numfmt] = 1;
	for (i = 1; i < l->n_fmt; i++)
		if (usa_num[i]) {
			char nom[16];

			fmt(nom, sizeof nom, "N%u", i);
			pon_estilo_num(&est, nom, lib_fmt(l, i));
		}
	for (i = 1; i < l->n_est; i++)
		pon_estilo_celda(&est, l, i, lib_estilo(l, i)->numfmt != 0);
	/* content.xml */
	buf_txt(&b, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<office:document-content " NS_DOC ">"
		    "<office:automatic-styles>");
	buf_pon(&b, est.d, est.n);
	buf_txt(&b, "</office:automatic-styles><office:body><office:spreadsheet>");
	buf_pon(&b, cont.d, cont.n);
	buf_txt(&b, "</office:spreadsheet></office:body></office:document-content>");
	buf_suelta(&cont);
	buf_suelta(&est);
	if (b.mal || zip_mete(z, "mimetype", "application/vnd.oasis.opendocument.spreadsheet", 46, 1) || zip_mete(z, "content.xml", b.d, b.n, 0))
		mal = 1;
	b.n = 0;
	/* styles.xml: el estilo de siempre (el de la celda 0) */
	{
		const estilo *e = lib_estilo(l, 0);

		buf_txt(&b, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<office:document-styles " NS_DOC "><office:styles>"
			    "<style:default-style style:family=\"table-cell\"><style:text-properties");
		xml_pon_atr(&b, "fo:font-family", e->familia);
		buf_txt(&b, " fo:font-size=\"");
		pon_num(&b, e->tam / 2.0);
		buf_txt(&b, "pt\"/></style:default-style><style:style style:name=\"Default\" style:family=\"table-cell\"/>"
			    "</office:styles></office:document-styles>");
		if (b.mal || zip_mete(z, "styles.xml", b.d, b.n, 0))
			mal = 1;
		b.n = 0;
	}
	buf_txt(&b, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<office:document-meta " NS_DOC "><office:meta>"
		    "<meta:generator>Cuentas 2 (Hispa-Pentium)</meta:generator>");
	if (l->autor[0]) {
		buf_txt(&b, "<dc:creator>");
		buf_xml(&b, l->autor, largo(l->autor));
		buf_txt(&b, "</dc:creator>");
	}
	buf_txt(&b, "</office:meta></office:document-meta>");
	if (b.mal || zip_mete(z, "meta.xml", b.d, b.n, 0))
		mal = 1;
	b.n = 0;
	/* settings.xml: la hoja activa, el cursor y los paneles */
	buf_txt(&b, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<office:document-settings xmlns:office=\"urn:oasis:names:tc:opendocument:xmlns:office:1.0\" "
		    "xmlns:config=\"urn:oasis:names:tc:opendocument:xmlns:config:1.0\" office:version=\"1.2\"><office:settings>"
		    "<config:config-item-set config:name=\"ooo:view-settings\"><config:config-item-map-indexed config:name=\"Views\">"
		    "<config:config-item-map-entry><config:config-item config:name=\"ViewId\" config:type=\"string\">view1</config:config-item>"
		    "<config:config-item-map-named config:name=\"Tables\">");
	for (i = 0; i < l->n_h; i++) {
		hoja *h = l->h[i];

		buf_txt(&b, "<config:config-item-map-entry");
		xml_pon_atr(&b, "config:name", h->nombre);
		buf_fmt(&b, "><config:config-item config:name=\"CursorPositionX\" config:type=\"int\">%u</config:config-item>"
			    "<config:config-item config:name=\"CursorPositionY\" config:type=\"int\">%u</config:config-item>", h->cur_c, h->cur_f);
		buf_fmt(&b, "<config:config-item config:name=\"HorizontalSplitMode\" config:type=\"short\">%u</config:config-item>"
			    "<config:config-item config:name=\"VerticalSplitMode\" config:type=\"short\">%u</config:config-item>"
			    "<config:config-item config:name=\"HorizontalSplitPosition\" config:type=\"int\">%u</config:config-item>"
			    "<config:config-item config:name=\"VerticalSplitPosition\" config:type=\"int\">%u</config:config-item>"
			    "<config:config-item config:name=\"ActiveSplitRange\" config:type=\"short\">2</config:config-item>"
			    "<config:config-item config:name=\"PositionLeft\" config:type=\"int\">0</config:config-item>"
			    "<config:config-item config:name=\"PositionRight\" config:type=\"int\">%u</config:config-item>"
			    "<config:config-item config:name=\"PositionTop\" config:type=\"int\">0</config:config-item>"
			    "<config:config-item config:name=\"PositionBottom\" config:type=\"int\">%u</config:config-item>"
			    "<config:config-item config:name=\"ShowGrid\" config:type=\"boolean\">%s</config:config-item>"
			    "</config:config-item-map-entry>",
			h->inm_c ? 2 : 0, h->inm_f ? 2 : 0, h->inm_c, h->inm_f, h->inm_c, h->inm_f, h->sin_rejilla ? "false" : "true");
	}
	buf_txt(&b, "</config:config-item-map-named><config:config-item config:name=\"ActiveTable\" config:type=\"string\">");
	buf_xml(&b, l->h[l->activa < l->n_h ? l->activa : 0]->nombre, largo(l->h[l->activa < l->n_h ? l->activa : 0]->nombre));
	buf_txt(&b, "</config:config-item></config:config-item-map-entry></config:config-item-map-indexed></config:config-item-set>"
		    "</office:settings></office:document-settings>");
	if (b.mal || zip_mete(z, "settings.xml", b.d, b.n, 0))
		mal = 1;
	b.n = 0;
	buf_txt(&b, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<manifest:manifest xmlns:manifest=\"urn:oasis:names:tc:opendocument:xmlns:manifest:1.0\" manifest:version=\"1.2\">"
		    "<manifest:file-entry manifest:full-path=\"/\" manifest:version=\"1.2\" manifest:media-type=\"application/vnd.oasis.opendocument.spreadsheet\"/>"
		    "<manifest:file-entry manifest:full-path=\"content.xml\" manifest:media-type=\"text/xml\"/>"
		    "<manifest:file-entry manifest:full-path=\"styles.xml\" manifest:media-type=\"text/xml\"/>"
		    "<manifest:file-entry manifest:full-path=\"meta.xml\" manifest:media-type=\"text/xml\"/>"
		    "<manifest:file-entry manifest:full-path=\"settings.xml\" manifest:media-type=\"text/xml\"/>"
		    "</manifest:manifest>");
	if (b.mal || zip_mete(z, "META-INF/manifest.xml", b.d, b.n, 0))
		mal = 1;
	buf_suelta(&b);
	so_suelta(usa_num);
	if (mal) {
		zip_deja(z);
		copia(err, "No hay memoria para guardar el .ODS", cabe);
		return 1;
	}
	if (zip_acaba(z, out)) {
		copia(err, "No se ha podido hacer el ZIP del .ODS", cabe);
		return 1;
	}
	return 0;
}
