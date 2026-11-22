/* f_xlsx.c - Cuentas 2: el formato .XLSX de Excel (OOXML: un ZIP con XML;
 * oficina/comun/zip.h y xml.h). Se lee y se escribe: las hojas con su
 * nombre, los valores (números, textos —los compartidos y los de dentro—,
 * lógicos, errores), las fórmulas (en inglés, también las compartidas, que
 * se sacan corriendo la primera), los estilos (formatos de número, letras,
 * rellenos con su trama, bordes, alineación), los anchos, los altos, las
 * filas y columnas ocultas, las combinadas, los paneles inmovilizados, los
 * nombres definidos (y el área de impresión), el autofiltro, los
 * comentarios (con su dibujo VML, para que Excel los enseñe), el formato
 * condicional sencillo (cellIs y containsText con su dxf) y la página.
 * Lo que se pierde: los gráficos (aún no se escriben ni se leen en .XLSX),
 * el símbolo del formato condicional (no lo hay en .XLSX), los criterios
 * del autofiltro (solo su rango) y los estilos de las filas y columnas de
 * más de lo que diga cada celda.
 * Los anchos: los «caracteres» de Excel se pasan a puntos de Cuentas con la
 * misma proporción que la columna de siempre (8,43 de Excel son los 88 de
 * Cuentas), y de vuelta igual: un libro va y vuelve con los mismos anchos.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "archivo.h"
#include "formula.h"
#include "numeros.h"
#include "zip.h"
#include "xml.h"

#define MAX_HOJAS_X 255

/* ---- medidas ---- */
static u32 px_de_ancho(double w)
{
	double px = (w * 7 + 5) * CU_ANCHO / 64.0;

	if (px < 1)
		px = 1;
	if (px > 2000)
		px = 2000;
	return (u32)(px + 0.5);
}

static double ancho_de_px(u32 px)
{
	return (px * 64.0 / CU_ANCHO - 5) / 7;
}

/* ---- colores ---- */
static const u32 INDEXADOS[66] = {
	0x000000, 0xFFFFFF, 0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00, 0xFF00FF, 0x00FFFF,
	0x000000, 0xFFFFFF, 0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00, 0xFF00FF, 0x00FFFF,
	0x800000, 0x008000, 0x000080, 0x808000, 0x800080, 0x008080, 0xC0C0C0, 0x808080,
	0x9999FF, 0x993366, 0xFFFFCC, 0xCCFFFF, 0x660066, 0xFF8080, 0x0066CC, 0xCCCCFF,
	0x000080, 0xFF00FF, 0xFFFF00, 0x00FFFF, 0x800080, 0x800000, 0x008080, 0x0000FF,
	0x00CCFF, 0xCCFFFF, 0xCCFFCC, 0xFFFF99, 0x99CCFF, 0xFF99CC, 0xCC99FF, 0xFFCC99,
	0x3366FF, 0x33CCCC, 0x99CC00, 0xFFCC00, 0xFF9900, 0xFF6600, 0x666699, 0x969696,
	0x003366, 0x339966, 0x003300, 0x333300, 0x993300, 0x993366, 0x333399, 0x333333,
	0x000000, 0xFFFFFF
};
static const u32 TEMA[12] = { 0xFFFFFF, 0x000000, 0xE7E6E6, 0x44546A, 0x4472C4, 0xED7D31, 0xA5A5A5, 0xFFC000, 0x5B9BD5, 0x70AD47, 0x0563C1, 0x954F72 };

static u32 hex(const char *s)
{
	u32 v = 0;

	for (; s && *s; s++) {
		char c = *s;

		if (c >= '0' && c <= '9')
			v = v * 16 + (u32)(c - '0');
		else if ((c | 32) >= 'a' && (c | 32) <= 'f')
			v = v * 16 + (u32)((c | 32) - 'a' + 10);
		else
			break;
	}
	return v;
}

/* el color de un elemento (rgb, indexed, theme); def si no lo dice */
static u32 color_de(const xml_nodo *n, u32 def)
{
	const char *a;

	if (!n)
		return def;
	if ((a = xml_atr(n, "auto")) && a[0] == '1')
		return def;
	if ((a = xml_atr(n, "rgb")))
		return hex(a) & 0xFFFFFF;
	if ((a = xml_atr(n, "indexed"))) {
		long i = numero(a, NULL);

		return i >= 0 && i < 66 ? INDEXADOS[i] : def;
	}
	if ((a = xml_atr(n, "theme"))) {
		long i = numero(a, NULL);

		return i >= 0 && i < 12 ? TEMA[i] : def;
	}
	return def;
}

static int cierto(const xml_nodo *n, const char *atr, int sin)
{
	const char *v = xml_atr(n, atr);

	if (!v)
		return sin;
	return v[0] == '1' || v[0] == 't' || v[0] == 'T';
}

static double num_atr(const xml_nodo *n, const char *a, double def)
{
	const char *v = xml_atr(n, a);
	double x;

	if (!v || !num_lee_c(v, &x, NULL))
		return def;
	return x;
}

/* ---- los formatos de número de serie (los de Excel en español) ---- */
static const char *fmt_serie(int id)
{
	switch (id) {
	case 0: return "General";
	case 1: return "0";
	case 2: return "0.00";
	case 3: return "#,##0";
	case 4: return "#,##0.00";
	case 5: return "#,##0 \"€\";-#,##0 \"€\"";
	case 6: return "#,##0 \"€\";[Red]-#,##0 \"€\"";
	case 7: return "#,##0.00 \"€\";-#,##0.00 \"€\"";
	case 8: return "#,##0.00 \"€\";[Red]-#,##0.00 \"€\"";
	case 9: return "0%";
	case 10: return "0.00%";
	case 11: return "0.00E+00";
	case 12: return "# ?/?";
	case 13: return "# ?\?/?\?";
	case 14: return "dd/mm/yyyy";
	case 15: return "d-mmm-yy";
	case 16: return "d-mmm";
	case 17: return "mmm-yy";
	case 18: return "h:mm AM/PM";
	case 19: return "h:mm:ss AM/PM";
	case 20: return "h:mm";
	case 21: return "h:mm:ss";
	case 22: return "dd/mm/yyyy h:mm";
	case 37: return "#,##0 ;(#,##0)";
	case 38: return "#,##0 ;[Red](#,##0)";
	case 39: return "#,##0.00;(#,##0.00)";
	case 40: return "#,##0.00;[Red](#,##0.00)";
	case 45: return "mm:ss";
	case 46: return "[h]:mm:ss";
	case 47: return "mm:ss.0";
	case 48: return "##0.0E+0";
	case 49: return "@";
	}
	return "General";
}

/* ===================================================================== */
/*  Leer                                                                 */
/* ===================================================================== */
typedef struct {
	char familia[32];
	u16 tam;
	u8 neg, cur, sub, tach;
	u32 color;
} x_letra;
typedef struct {
	u8 trama;
	u32 fondo, color;
} x_relleno;
typedef struct {
	u8 b[4];
	u32 c[4];
} x_borde;

typedef struct {
	libro *l;
	zip_lector *z;
	char **ss;		/* los textos compartidos */
	u32 n_ss;
	char **nf;		/* numFmtId -> código (los propios) */
	u32 *nf_id;
	u32 n_nf;
	x_letra *le;
	u32 n_le;
	x_relleno *re;
	u32 n_re;
	x_borde *bo;
	u32 n_bo;
	u16 *xf;		/* cellXfs -> estilo de Cuentas */
	u32 n_xf;
	condicional *dxf;	/* (los dxf, como lo que cambia un condicional) */
	u32 n_dxf;
	int fecha1904;
} lector;

static char *texto_de(const xml_nodo *n)
{
	bufer b = { 0 };
	char *s;

	/* los <t> (de <si> o de <is>, también dentro de <r>); los <rPh> (fonética) no */
	if (n) {
		const xml_nodo *h;

		for (h = xml_primero(n); h; h = xml_sig(h, NULL)) {
			if (xml_es(h, "t"))
				xml_texto(h, &b);
			else if (xml_es(h, "r"))
				xml_texto(xml_hijo(h, "t"), &b);
		}
	}
	buf_c(&b, 0);
	s = b.mal ? NULL : (char *)b.d;
	if (!s)
		buf_suelta(&b);
	return s ? s : duplica("");
}

static xml_doc *saca_xml(zip_lector *z, const char *nombre)
{
	u32 n;
	u8 *d = zip_saca(z, nombre, &n);
	xml_doc *x;

	if (!d)
		return NULL;
	x = xml_lee((const char *)d, n);
	so_suelta(d);
	return x;
}

/* la ruta de dentro del ZIP de lo que dice una relación (con la carpeta de quien la tiene) */
static void ruta_rel(const char *carpeta, const char *destino, char *d, u32 cabe)
{
	char t[600];
	u32 k;

	if (destino[0] == '/') {
		copia(d, destino + 1, cabe);
		return;
	}
	copia(t, carpeta, sizeof t);
	while (empieza(destino, "../")) {
		/* una carpeta menos */
		k = largo(t);
		if (k && t[k - 1] == '/')
			t[--k] = 0;
		while (k && t[k - 1] != '/')
			t[--k] = 0;
		destino += 3;
	}
	anade(t, destino, sizeof t);
	copia(d, t, cabe);
}

/* el destino de la relación rid en el .rels de una parte (NULL si no) */
static int rel_destino(zip_lector *z, const char *parte, const char *rid, const char *tipo, char *d, u32 cabe)
{
	char rels[600], carpeta[600];
	const char *bar = strrchr(parte, '/');
	xml_doc *x;
	xml_nodo *r;
	int hay = 0;

	if (bar) {
		copia(carpeta, parte, (u32)(bar - parte) + 2 < sizeof carpeta ? (u32)(bar - parte) + 2 : sizeof carpeta);
		fmt(rels, sizeof rels, "%s_rels/%s.rels", carpeta, bar + 1);
	} else {
		carpeta[0] = 0;
		fmt(rels, sizeof rels, "_rels/%s.rels", parte);
	}
	x = saca_xml(z, rels);
	if (!x)
		return 0;
	for (r = xml_hijo(xml_raiz(x), "Relationship"); r; r = xml_sig(r, "Relationship")) {
		const char *id = xml_atr(r, "Id"), *t = xml_atr(r, "Type"), *ds = xml_atr(r, "Target");

		if (!ds)
			continue;
		if ((rid && id && iguales(id, rid)) || (!rid && tipo && t && busca(t, tipo))) {
			ruta_rel(carpeta, ds, d, cabe);
			hay = 1;
			break;
		}
	}
	xml_suelta(x);
	return hay;
}

/* «\€» -> «€» (al escribir, lo que no es ASCII fuera de comillas va con «\» delante) */
static char *sin_comillas_de_mas(const char *s)
{
	bufer b = { 0 };
	const char *p = s;
	int com = 0;

	while (*p) {
		if (*p == '"')
			com = !com;
		if (!com && *p == '\\' && (u8)p[1] >= 0x80) {
			p++;
			continue;
		}
		buf_c(&b, (u8)*p++);
	}
	buf_c(&b, 0);
	if (b.mal || !b.d) {
		buf_suelta(&b);
		return duplica(s);
	}
	return (char *)b.d;
}

static void lee_estilos(lector *L)
{
	xml_doc *x = saca_xml(L->z, "xl/styles.xml");
	xml_nodo *r, *n, *k;
	u32 i;

	if (!x)
		x = saca_xml(L->z, "xl/Styles.xml");
	if (!x)
		return;
	r = xml_raiz(x);
	/* los formatos de número propios */
	for (n = xml_hijo(xml_hijo(r, "numFmts"), "numFmt"); n; n = xml_sig(n, "numFmt")) {
		char **a = so_repide(L->nf, (L->n_nf + 1) * sizeof(char *));
		u32 *b = so_repide(L->nf_id, (L->n_nf + 1) * sizeof(u32));

		if (a)
			L->nf = a;
		if (b)
			L->nf_id = b;
		if (!a || !b)
			break;
		L->nf[L->n_nf] = sin_comillas_de_mas(xml_atr(n, "formatCode") ? xml_atr(n, "formatCode") : "General");
		L->nf_id[L->n_nf] = (u32)num_atr(n, "numFmtId", 0);
		L->n_nf++;
	}
	/* las letras */
	for (n = xml_hijo(xml_hijo(r, "fonts"), "font"); n; n = xml_sig(n, "font")) {
		x_letra *a = so_repide(L->le, (L->n_le + 1) * sizeof(x_letra)), *f;

		if (!a)
			break;
		L->le = a;
		f = &a[L->n_le++];
		memset(f, 0, sizeof *f);
		copia(f->familia, xml_atr(xml_hijo(n, "name"), "val") ? xml_atr(xml_hijo(n, "name"), "val") : CU_FAMILIA, sizeof f->familia);
		f->tam = (u16)(num_atr(xml_hijo(n, "sz"), "val", 10) * 2 + 0.5);
		if (f->tam < 2 || f->tam > 800)
			f->tam = CU_TAM;
		f->neg = xml_hijo(n, "b") && cierto(xml_hijo(n, "b"), "val", 1);
		f->cur = xml_hijo(n, "i") && cierto(xml_hijo(n, "i"), "val", 1);
		f->sub = xml_hijo(n, "u") && !(xml_atr(xml_hijo(n, "u"), "val") && iguales(xml_atr(xml_hijo(n, "u"), "val"), "none"));
		f->tach = xml_hijo(n, "strike") && cierto(xml_hijo(n, "strike"), "val", 1);
		f->color = xml_hijo(n, "color") ? color_de(xml_hijo(n, "color"), CU_AUTO) : CU_AUTO;
		if (f->color == 0x000000 && xml_atr(xml_hijo(n, "color"), "theme"))
			f->color = CU_AUTO;
	}
	/* los rellenos */
	for (n = xml_hijo(xml_hijo(r, "fills"), "fill"); n; n = xml_sig(n, "fill")) {
		x_relleno *a = so_repide(L->re, (L->n_re + 1) * sizeof(x_relleno)), *f;
		xml_nodo *p = xml_hijo(n, "patternFill");
		const char *t = xml_atr(p, "patternType");
		static const struct {
			const char *n;
			u8 t;
		} tr[] = { { "solid", TR_SOLIDO }, { "darkGray", TR_GRIS75 }, { "mediumGray", TR_GRIS50 }, { "lightGray", TR_GRIS25 },
			   { "gray125", TR_GRIS12 }, { "gray0625", TR_GRIS12 }, { "darkHorizontal", TR_HORIZ }, { "lightHorizontal", TR_HORIZ },
			   { "darkVertical", TR_VERT }, { "lightVertical", TR_VERT }, { "darkDown", TR_DIAG_ABAJO }, { "lightDown", TR_DIAG_ABAJO },
			   { "darkUp", TR_DIAG_ARRIBA }, { "lightUp", TR_DIAG_ARRIBA }, { "darkGrid", TR_CUADRICULA }, { "lightGrid", TR_CUADRICULA },
			   { "darkTrellis", TR_ENREJADO }, { "lightTrellis", TR_ENREJADO } };

		if (!a)
			break;
		L->re = a;
		f = &a[L->n_re++];
		f->trama = TR_NADA;
		f->fondo = 0xFFFFFF;
		f->color = 0;
		if (!t && p && xml_hijo(p, "fgColor"))
			t = "solid";
		for (i = 0; t && i < sizeof tr / sizeof tr[0]; i++)
			if (iguales(t, tr[i].n))
				f->trama = tr[i].t;
		if (f->trama == TR_SOLIDO)
			f->fondo = color_de(xml_hijo(p, "fgColor"), 0xFFFFFF);
		else if (f->trama != TR_NADA) {
			f->color = color_de(xml_hijo(p, "fgColor"), 0x000000);
			f->fondo = color_de(xml_hijo(p, "bgColor"), 0xFFFFFF);
		}
		if (f->fondo == CU_AUTO)
			f->fondo = 0xFFFFFF;
	}
	/* (los dos primeros rellenos de Excel son «none» y «gray125» de mentira: el segundo no se pinta) */
	/* los bordes */
	for (n = xml_hijo(xml_hijo(r, "borders"), "border"); n; n = xml_sig(n, "border")) {
		x_borde *a = so_repide(L->bo, (L->n_bo + 1) * sizeof(x_borde)), *f;
		static const char *const lados[4] = { "left", "top", "right", "bottom" };
		int j;

		if (!a)
			break;
		L->bo = a;
		f = &a[L->n_bo++];
		memset(f, 0, sizeof *f);
		for (j = 0; j < 4; j++) {
			xml_nodo *s = xml_hijo(n, lados[j]);
			const char *st = xml_atr(s, "style");

			if (!s && j == 0)
				s = xml_hijo(n, "start");
			if (!s && j == 2)
				s = xml_hijo(n, "end");
			st = xml_atr(s, "style");
			if (!st || iguales(st, "none"))
				continue;
			f->b[j] = iguales(st, "thin") ? BR_FINO : iguales(st, "medium") ? BR_MEDIO : iguales(st, "thick") ? BR_GRUESO :
				  iguales(st, "double") ? BR_DOBLE : (iguales(st, "dotted") || iguales(st, "hair")) ? BR_PUNTOS :
				  busca(st, "ash") ? (busca(st, "medium") ? BR_GUIONES : BR_GUIONES) : BR_FINO;
			f->c[j] = color_de(xml_hijo(s, "color"), 0x000000);
			if (f->c[j] == CU_AUTO)
				f->c[j] = 0;
		}
	}
	/* los cellXfs */
	for (n = xml_hijo(xml_hijo(r, "cellXfs"), "xf"); n; n = xml_sig(n, "xf")) {
		u16 *a = so_repide(L->xf, (L->n_xf + 1) * sizeof(u16));
		estilo e;
		u32 fi = (u32)num_atr(n, "fontId", 0), ri = (u32)num_atr(n, "fillId", 0), bi = (u32)num_atr(n, "borderId", 0), ni = (u32)num_atr(n, "numFmtId", 0);
		const char *cod = NULL;
		int j;

		if (!a)
			break;
		L->xf = a;
		estilo_base(&e);
		if (fi < L->n_le) {
			copia(e.familia, L->le[fi].familia, sizeof e.familia);
			e.tam = L->le[fi].tam;
			e.negrita = L->le[fi].neg;
			e.cursiva = L->le[fi].cur;
			e.subrayado = L->le[fi].sub;
			e.tachado = L->le[fi].tach;
			e.color = L->le[fi].color;
		}
		if (ri < L->n_re && !(ri == 1 && L->re[ri].trama == TR_GRIS12 && L->re[ri].color == 0 && 0)) {
			e.trama = L->re[ri].trama;
			e.fondo = L->re[ri].fondo;
			e.color_trama = L->re[ri].color;
		}
		if (bi < L->n_bo)
			for (j = 0; j < 4; j++) {
				e.borde[j] = L->bo[bi].b[j];
				e.color_borde[j] = L->bo[bi].c[j];
			}
		for (i = 0; i < L->n_nf; i++)
			if (L->nf_id[i] == ni)
				cod = L->nf[i];
		if (!cod)
			cod = fmt_serie((int)ni);
		e.numfmt = lib_fmt_id(L->l, cod);
		if ((k = xml_hijo(n, "alignment"))) {
			const char *h = xml_atr(k, "horizontal"), *v = xml_atr(k, "vertical");

			if (h)
				e.alin_h = iguales(h, "left") ? AH_IZQ : iguales(h, "center") ? AH_CENTRO : iguales(h, "right") ? AH_DER :
					   iguales(h, "fill") ? AH_RELLENAR : iguales(h, "justify") ? AH_JUSTIFICAR :
					   iguales(h, "centerContinuous") ? AH_CENTRO_SEL : AH_GENERAL;
			if (v)
				e.alin_v = iguales(v, "top") ? AV_ARRIBA : iguales(v, "center") ? AV_CENTRO : AV_ABAJO;
			e.ajustar = (u8)cierto(k, "wrapText", 0);
			e.sangria = (u8)num_atr(k, "indent", 0);
			e.girado = (u8)num_atr(k, "textRotation", 0);
		}
		L->xf[L->n_xf++] = lib_estilo_id(L->l, &e);
	}
	/* los dxf (de los condicionales) */
	for (n = xml_hijo(xml_hijo(r, "dxfs"), "dxf"); n; n = xml_sig(n, "dxf")) {
		condicional *a = so_repide(L->dxf, (L->n_dxf + 1) * sizeof(condicional)), *d;
		xml_nodo *f = xml_hijo(n, "font"), *p = xml_hijo(xml_hijo(n, "fill"), "patternFill");

		if (!a)
			break;
		L->dxf = a;
		d = &a[L->n_dxf++];
		memset(d, 0, sizeof *d);
		d->color = CU_AUTO;
		d->fondo = CU_AUTO;
		d->trama = TR_SOLIDO;
		if (f) {
			d->negrita = xml_hijo(f, "b") && cierto(xml_hijo(f, "b"), "val", 1);
			d->cursiva = xml_hijo(f, "i") && cierto(xml_hijo(f, "i"), "val", 1);
			if (xml_hijo(f, "color"))
				d->color = color_de(xml_hijo(f, "color"), CU_AUTO);
		}
		if (p) {
			if (xml_hijo(p, "bgColor"))
				d->fondo = color_de(xml_hijo(p, "bgColor"), CU_AUTO);
			else if (xml_hijo(p, "fgColor"))
				d->fondo = color_de(xml_hijo(p, "fgColor"), CU_AUTO);
		}
	}
	xml_suelta(x);
}

static void lee_compartidos(lector *L)
{
	xml_doc *x = saca_xml(L->z, "xl/sharedStrings.xml");
	xml_nodo *n;
	u32 cuantos = 0, i = 0;

	if (!x)
		return;
	for (n = xml_hijo(xml_raiz(x), "si"); n; n = xml_sig(n, "si"))
		cuantos++;
	L->ss = so_pide((cuantos ? cuantos : 1) * sizeof(char *));
	if (L->ss)
		for (n = xml_hijo(xml_raiz(x), "si"); n && i < cuantos; n = xml_sig(n, "si"))
			L->ss[i++] = texto_de(n);
	L->n_ss = L->ss ? i : 0;
	xml_suelta(x);
}

/* las compartidas: la primera de cada «si», y dónde está */
typedef struct {
	formula *f;
	u32 f0, c0;
} compartida;

static void lee_comentarios(lector *L, hoja *h, const char *parte)
{
	xml_doc *x = saca_xml(L->z, parte);
	xml_nodo *n, *r;
	const char *autores[64];
	u32 na = 0;

	if (!x)
		return;
	r = xml_raiz(x);
	for (n = xml_hijo(xml_hijo(r, "authors"), "author"); n && na < 64; n = xml_sig(n, "author")) {
		bufer b = { 0 };

		xml_texto(n, &b);
		buf_c(&b, 0);
		autores[na++] = b.d ? (char *)b.d : NULL;
	}
	for (n = xml_hijo(xml_hijo(r, "commentList"), "comment"); n; n = xml_sig(n, "comment")) {
		u32 f, c;
		const char *fin;
		char *t;
		u32 ai = (u32)num_atr(n, "authorId", 0);

		if (!xml_atr(n, "ref") || !cu_lee_ref(xml_atr(n, "ref"), &f, &c, &fin))
			continue;
		t = texto_de(xml_hijo(n, "text"));
		hj_pon_comentario(h, f, c, t, ai < na && autores[ai] ? autores[ai] : "");
		so_suelta(t);
	}
	while (na)
		so_suelta((void *)autores[--na]);
	xml_suelta(x);
}

static void lee_hoja(lector *L, hoja *h, const char *parte)
{
	xml_doc *x = saca_xml(L->z, parte);
	xml_nodo *r, *n, *fila, *k;
	compartida *cp = NULL;
	u32 n_cp = 0, i;
	libro *l = L->l;

	if (!x)
		return;
	r = xml_raiz(x);
	/* el ancho de siempre */
	if ((n = xml_hijo(r, "sheetFormatPr")) && xml_atr(n, "defaultColWidth")) {
		u32 a = px_de_ancho(num_atr(n, "defaultColWidth", 8.43) - 0.71);

		h->ancho_def = a;
		for (i = 0; i < CU_COLS; i++)
			h->ancho[i] = (u16)a;
	}
	/* las columnas */
	for (n = xml_hijo(xml_hijo(r, "cols"), "col"); n; n = xml_sig(n, "col")) {
		u32 a = (u32)num_atr(n, "min", 1), b = (u32)num_atr(n, "max", a), c;
		int con = xml_atr(n, "width") != NULL;
		u32 an = px_de_ancho(num_atr(n, "width", 8.43));
		int oc = cierto(n, "hidden", 0);
		u32 s = (u32)num_atr(n, "style", 0);

		if (a < 1)
			a = 1;
		if (b > CU_COLS)
			b = CU_COLS;
		for (c = a - 1; c < b; c++) {
			if (con)
				h->ancho[c] = (u16)an;
			if (oc)
				h->oculta_c[c] = 1;
			if (s && s < L->n_xf)
				h->estilo_col[c] = L->xf[s];
		}
	}
	/* las celdas */
	for (fila = xml_hijo(xml_hijo(r, "sheetData"), "row"); fila; fila = xml_sig(fila, "row")) {
		static u32 ultima;
		u32 f = xml_atr(fila, "r") ? (u32)num_atr(fila, "r", 1) - 1 : ultima + 1, col = 0;

		if (fila == xml_hijo(xml_hijo(r, "sheetData"), "row") && !xml_atr(fila, "r"))
			f = 0;
		ultima = f;
		if (f >= CU_FILAS)
			continue;
		if (xml_atr(fila, "ht") && (cierto(fila, "customHeight", 0) || 1)) {
			u32 al = (u32)(num_atr(fila, "ht", 15) * 4 / 3 + 0.5);

			if (al != CU_ALTO)
				hj_pon_alto(h, f, al ? al : 1);
		}
		if (cierto(fila, "hidden", 0))
			hj_oculta_fila(h, f, 1);
		if (cierto(fila, "customFormat", 0) && xml_atr(fila, "s")) {
			u32 s = (u32)num_atr(fila, "s", 0);

			if (s < L->n_xf && L->xf[s]) {
				if (!h->estilo_fila) {
					h->estilo_fila = so_pide(CU_FILAS * sizeof(u16));
					if (h->estilo_fila)
						memset(h->estilo_fila, 0, CU_FILAS * sizeof(u16));
				}
				if (h->estilo_fila)
					h->estilo_fila[f] = L->xf[s];
			}
		}
		for (k = xml_hijo(fila, "c"); k; k = xml_sig(k, "c")) {
			u32 ff = f, c = col;
			const char *ref = xml_atr(k, "r"), *t = xml_atr(k, "t"), *fin;
			xml_nodo *v = xml_hijo(k, "v"), *fo = xml_hijo(k, "f");
			u32 s = (u32)num_atr(k, "s", 0);
			bufer vb = { 0 };
			int puesto = 0;

			if (ref && !cu_lee_ref(ref, &ff, &c, &fin))
				continue;
			col = c + 1;
			if (ff >= CU_FILAS || c >= CU_COLS)
				continue;
			if (v)
				xml_texto(v, &vb);
			buf_c(&vb, 0);
			/* la fórmula */
			if (fo) {
				bufer fb = { 0 };
				const char *ft = xml_atr(fo, "t");
				formula *fm = NULL;
				char e[200];
				u32 d;

				xml_texto(fo, &fb);
				buf_c(&fb, 0);
				if (ft && iguales(ft, "shared") && xml_atr(fo, "si")) {
					u32 si = (u32)num_atr(fo, "si", 0);

					if (fb.n > 1) {
						fm = fm_lee(l, h, (char *)fb.d, fb.n - 1, &CU_ARCHIVO, e, sizeof e, &d);
						if (fm && si < 100000) {
							if (si >= n_cp) {
								compartida *nc = so_repide(cp, (si + 1) * sizeof(compartida));

								if (nc) {
									memset(nc + n_cp, 0, (si + 1 - n_cp) * sizeof(compartida));
									cp = nc;
									n_cp = si + 1;
								}
							}
							if (si < n_cp) {
								fm_suelta(cp[si].f);
								cp[si].f = fm_copia(fm);
								cp[si].f0 = ff;
								cp[si].c0 = c;
							}
						}
					} else if (si < n_cp && cp[si].f)
						fm = fm_copia_desplaza(cp[si].f, (int)ff - (int)cp[si].f0, (int)c - (int)cp[si].c0);
				} else if (fb.n > 1)
					fm = fm_lee(l, h, (char *)fb.d, fb.n - 1, &CU_ARCHIVO, e, sizeof e, &d);
				buf_suelta(&fb);
				if (fm) {
					lib_pon_formula(l, h, ff, c, fm);
					puesto = 1;
				}
			}
			if (!puesto) {
				const char *val = vb.d ? (const char *)vb.d : "";

				if (t && iguales(t, "s")) {
					u32 i2 = (u32)numero(val, NULL);

					if (i2 < L->n_ss)
						lib_pon_texto(l, h, ff, c, L->ss[i2]);
				} else if (t && iguales(t, "inlineStr")) {
					char *z = texto_de(xml_hijo(k, "is"));

					lib_pon_texto(l, h, ff, c, z);
					so_suelta(z);
				} else if (t && (iguales(t, "str") || iguales(t, "e"))) {
					if (iguales(t, "e")) {
						u32 lg;
						int e2 = cu_error_de(val, &lg);
						valor w = { V_ERR, 0, 0, NULL };

						w.e = (u8)(e2 ? e2 : ER_VALOR);
						lib_pon_valor(l, h, ff, c, &w);
					} else if (val[0])
						lib_pon_texto(l, h, ff, c, val);
				} else if (t && iguales(t, "b")) {
					valor w = { V_LOG, 0, 0, NULL };

					w.n = val[0] == '1' || val[0] == 't';
					if (v)
						lib_pon_valor(l, h, ff, c, &w);
				} else if (v && val[0]) {
					double z;

					if (num_lee_c(val, &z, NULL))
						lib_pon_numero(l, h, ff, c, z);
				}
			}
			buf_suelta(&vb);
			if (s < L->n_xf)
				lib_pon_estilo(l, h, ff, c, L->xf[s]);
		}
	}
	for (i = 0; i < n_cp; i++)
		fm_suelta(cp[i].f);
	so_suelta(cp);
	/* las combinadas */
	for (n = xml_hijo(xml_hijo(r, "mergeCells"), "mergeCell"); n; n = xml_sig(n, "mergeCell")) {
		rango g;
		const char *fin;

		if (xml_atr(n, "ref") && cu_lee_rango(xml_atr(n, "ref"), &g, &fin))
			hj_combina(h, g);
	}
	/* la vista y los paneles */
	if ((n = xml_hijo(xml_hijo(r, "sheetViews"), "sheetView"))) {
		xml_nodo *p = xml_hijo(n, "pane"), *s = xml_hijo(n, "selection");
		const char *z;

		if (cierto(n, "showGridLines", 1) == 0)
			h->sin_rejilla = 1;
		if (xml_atr(n, "zoomScale")) {
			u32 zo = (u32)num_atr(n, "zoomScale", 100);

			h->zoom = (u16)(zo >= 10 && zo <= 400 ? zo : 100);
		}
		if (p && (z = xml_atr(p, "state")) && (iguales(z, "frozen") || iguales(z, "frozenSplit"))) {
			double xs = num_atr(p, "xSplit", 0), ys = num_atr(p, "ySplit", 0);

			if (xs >= 0 && xs < CU_COLS)
				h->inm_c = (u32)xs;
			if (ys >= 0 && ys < CU_FILAS)
				h->inm_f = (u32)ys;
		}
		while (s && xml_sig(s, "selection"))
			s = xml_sig(s, "selection");
		if (s && (z = xml_atr(s, "activeCell"))) {
			u32 f, c;
			const char *fin;

			if (cu_lee_ref(z, &f, &c, &fin)) {
				h->cur_f = f;
				h->cur_c = c;
				h->sel = cu_rango(f, c, f, c);
			}
		}
	}
	/* el autofiltro */
	if ((n = xml_hijo(r, "autoFilter")) && xml_atr(n, "ref")) {
		const char *fin;

		if (cu_lee_rango(xml_atr(n, "ref"), &h->filtro, &fin))
			h->hay_filtro = 1;
	}
	/* los condicionales */
	for (n = xml_hijo(r, "conditionalFormatting"); n; n = xml_sig(n, "conditionalFormatting")) {
		const char *sq = xml_atr(n, "sqref");
		xml_nodo *cf;
		rango g;
		const char *fin;

		if (!sq || !cu_lee_rango(sq, &g, &fin))
			continue;
		for (cf = xml_hijo(n, "cfRule"); cf; cf = xml_sig(cf, "cfRule")) {
			const char *ty = xml_atr(cf, "type"), *op = xml_atr(cf, "operator");
			condicional d;
			u32 di = (u32)num_atr(cf, "dxfId", 0xFFFF);
			xml_nodo *e1 = xml_hijo(cf, "formula"), *e2 = e1 ? xml_sig(e1, "formula") : NULL;
			condicional *nc;

			memset(&d, 0, sizeof d);
			d.color = d.fondo = CU_AUTO;
			d.trama = TR_SOLIDO;
			if (di < L->n_dxf)
				d = L->dxf[di];
			d.r = g;
			if (ty && iguales(ty, "cellIs") && op) {
				d.cond = iguales(op, "greaterThan") ? FC_MAYOR : iguales(op, "lessThan") ? FC_MENOR : iguales(op, "between") ? FC_ENTRE :
					 iguales(op, "equal") ? FC_IGUAL : iguales(op, "notEqual") ? FC_DISTINTO : iguales(op, "greaterThanOrEqual") ? FC_MAYOR_IGUAL :
					 iguales(op, "lessThanOrEqual") ? FC_MENOR_IGUAL : FC_MAYOR;
				{
					bufer b = { 0 };

					xml_texto(e1, &b);
					buf_c(&b, 0);
					d.v1 = b.d ? num_de_texto_c((char *)b.d) : 0;
					buf_suelta(&b);
					xml_texto(e2, &b);
					buf_c(&b, 0);
					d.v2 = b.d ? num_de_texto_c((char *)b.d) : 0;
					buf_suelta(&b);
				}
			} else if (ty && iguales(ty, "containsText") && xml_atr(cf, "text")) {
				d.cond = FC_TEXTO;
				copia(d.texto, xml_atr(cf, "text"), sizeof d.texto);
			} else
				continue;
			{
				/* (el símbolo de Cuentas, en su atributo propio, si lo trae) */
				const char *sm = xml_atr(cf, "cuentasSimbolo");

				if (sm)
					d.simbolo = (u8)(numero(sm, NULL) % SIM_N);
			}
			nc = so_repide(h->cond, (h->n_cond + 1) * sizeof(condicional));
			if (nc) {
				h->cond = nc;
				h->cond[h->n_cond++] = d;
			}
		}
	}
	/* la página */
	if ((n = xml_hijo(r, "printOptions")) && cierto(n, "gridLines", 0))
		h->pag.rejilla = 1;
	if ((n = xml_hijo(r, "printOptions"))) {
		h->pag.titulos = (u8)cierto(n, "headings", 0);
		h->pag.centrar_h = (u8)cierto(n, "horizontalCentered", 0);
		h->pag.centrar_v = (u8)cierto(n, "verticalCentered", 0);
	}
	if ((n = xml_hijo(r, "pageMargins"))) {
		h->pag.marg[0] = num_redondea(num_atr(n, "left", 0.7) * 2.54, 6);
		h->pag.marg[1] = num_redondea(num_atr(n, "top", 0.75) * 2.54, 6);
		h->pag.marg[2] = num_redondea(num_atr(n, "right", 0.7) * 2.54, 6);
		h->pag.marg[3] = num_redondea(num_atr(n, "bottom", 0.75) * 2.54, 6);
	}
	if ((n = xml_hijo(r, "pageSetup"))) {
		const char *o = xml_atr(n, "orientation");
		u32 ps = (u32)num_atr(n, "paperSize", 9);

		h->pag.horizontal = o && iguales(o, "landscape");
		h->pag.papel = ps == 1 ? 1 : ps == 8 ? 2 : ps == 11 ? 3 : ps == 5 ? 4 : 0;
		h->pag.escala = (u16)num_atr(n, "scale", 100);
		if (h->pag.escala < 10 || h->pag.escala > 400)
			h->pag.escala = 100;
		if (cierto(xml_hijo(xml_hijo(r, "sheetPr"), "pageSetUpPr"), "fitToPage", 0)) {
			h->pag.encajar_an = (u16)num_atr(n, "fitToWidth", 1);
			h->pag.encajar_al = (u16)num_atr(n, "fitToHeight", 1);
		}
	}
	if ((n = xml_hijo(r, "headerFooter"))) {
		xml_nodo *e[2] = { xml_hijo(n, "oddHeader"), xml_hijo(n, "oddFooter") };
		char *d[2] = { h->pag.encabezado, h->pag.pie };
		int j;

		for (j = 0; j < 2; j++)
			if (e[j]) {
				bufer b = { 0 };
				u32 a = 0, o = 0;
				char *s;

				xml_texto(e[j], &b);
				buf_c(&b, 0);
				s = b.d ? (char *)b.d : "";
				/* (sin &L &C &R ni las letras &"...") */
				while (s[a] && o + 1 < 128) {
					if (s[a] == '&' && (s[a + 1] == 'L' || s[a + 1] == 'C' || s[a + 1] == 'R')) {
						a += 2;
						if (o && d[j][o - 1] != ' ')
							d[j][o++] = ' ';
						continue;
					}
					if (s[a] == '&' && s[a + 1] == '"') {
						a += 2;
						while (s[a] && s[a] != '"')
							a++;
						if (s[a])
							a++;
						continue;
					}
					d[j][o++] = s[a++];
				}
				while (o && d[j][o - 1] == ' ')
					o--;
				d[j][o] = 0;
				buf_suelta(&b);
			}
	}
	/* los saltos */
	{
		static const char *const q[2] = { "rowBreaks", "colBreaks" };
		int j;

		for (j = 0; j < 2; j++)
			for (n = xml_hijo(xml_hijo(r, q[j]), "brk"); n; n = xml_sig(n, "brk")) {
				u32 **s = j ? &h->salto_c : &h->salto_f, *ns = j ? &h->n_salto_c : &h->n_salto_f;
				u32 *nn = so_repide(*s, (*ns + 1) * sizeof(u32));
				u32 v = (u32)num_atr(n, "id", 0);

				if (nn && v) {
					*s = nn;
					nn[(*ns)++] = v;
				}
			}
	}
	/* los comentarios: por las relaciones de la hoja */
	{
		char cm[600];

		if (rel_destino(L->z, parte, NULL, "/comments", cm, sizeof cm))
			lee_comentarios(L, h, cm);
	}
	xml_suelta(x);
}

libro *xlsx_lee(const u8 *d, u32 n, char *err, u32 cabe)
{
	lector L;
	xml_doc *wb;
	xml_nodo *r, *s;
	char partes[MAX_HOJAS_X][256];
	u32 nh = 0, i;
	char wbruta[256];

	memset(&L, 0, sizeof L);
	L.z = zip_abre(d, n);
	if (!L.z) {
		copia(err, "No es un .XLSX (no es un ZIP que se pueda leer)", cabe);
		return NULL;
	}
	/* el libro: por la relación de _rels/.rels, o donde siempre */
	if (!rel_destino(L.z, "", NULL, "/officeDocument", wbruta, sizeof wbruta) || zip_busca(L.z, wbruta) < 0)
		copia(wbruta, "xl/workbook.xml", sizeof wbruta);
	wb = saca_xml(L.z, wbruta);
	if (!wb) {
		zip_cierra(L.z);
		copia(err, "El .XLSX no tiene libro (xl/workbook.xml)", cabe);
		return NULL;
	}
	L.l = lib_nuevo(0);
	if (!L.l) {
		xml_suelta(wb);
		zip_cierra(L.z);
		copia(err, "No hay memoria", cabe);
		return NULL;
	}
	r = xml_raiz(wb);
	L.fecha1904 = cierto(xml_hijo(r, "workbookPr"), "date1904", 0);
	lee_estilos(&L);
	lee_compartidos(&L);
	/* las hojas (primero todas: las fórmulas se piden unas a otras) */
	for (s = xml_hijo(xml_hijo(r, "sheets"), "sheet"); s && nh < MAX_HOJAS_X; s = xml_sig(s, "sheet")) {
		const char *nom = xml_atr(s, "name"), *rid = xml_atr(s, "r:id");
		char nm[96];

		if (!rid)
			rid = xml_atr(s, "id");
		if (!rid || !rel_destino(L.z, wbruta, rid, NULL, partes[nh], sizeof partes[nh]))
			continue;
		copia(nm, nom && nom[0] ? nom : "", sizeof nm);
		if (!nm[0] || lib_hoja_nombre(L.l, nm, largo(nm)))
			lib_nombre_libre(L.l, nm, sizeof nm);
		if (!lib_hoja_nueva(L.l, nm, -1))
			break;
		if (xml_atr(s, "state") && !iguales(xml_atr(s, "state"), "visible"))
			L.l->h[L.l->n_h - 1]->oculta = 1;
		nh++;
	}
	if (!L.l->n_h)
		lib_hoja_nueva(L.l, "Hoja1", -1);
	/* los nombres definidos */
	for (s = xml_hijo(xml_hijo(r, "definedNames"), "definedName"); s; s = xml_sig(s, "definedName")) {
		const char *nom = xml_atr(s, "name");
		bufer b = { 0 };
		char e[200];
		u32 dd;
		formula *fm;
		long ls = xml_atr(s, "localSheetId") ? numero(xml_atr(s, "localSheetId"), NULL) : -1;

		if (!nom)
			continue;
		xml_texto(s, &b);
		buf_c(&b, 0);
		fm = b.d ? fm_lee(L.l, L.l->h[0], (char *)b.d, b.n - 1, &CU_ARCHIVO, e, sizeof e, &dd) : NULL;
		buf_suelta(&b);
		if (!fm)
			continue;
		if (iguales(nom, "_xlnm.Print_Area")) {
			if (fm->n >= 1 && (fm->t[0].t == T_RANGO || fm->t[0].t == T_REF)) {
				hoja *hh = lib_hoja_id(L.l, fm->t[0].h);

				if (ls >= 0 && (u32)ls < L.l->n_h)
					hh = L.l->h[ls];
				if (hh) {
					hh->hay_area = 1;
					hh->area = cu_rango((u32)fm->t[0].u.r.f0, fm->t[0].u.r.c0, (u32)fm->t[0].u.r.f1, fm->t[0].u.r.c1);
				}
			}
			fm_suelta(fm);
			continue;
		}
		if (empieza(nom, "_xlnm.") || ls >= 0) {
			fm_suelta(fm);
			continue;
		}
		lib_pon_nombre(L.l, nom, fm);
	}
	{
		xml_nodo *bv = xml_hijo(xml_hijo(r, "bookViews"), "workbookView");
		u32 a = bv ? (u32)num_atr(bv, "activeTab", 0) : 0;

		L.l->activa = a < L.l->n_h ? a : 0;
	}
	for (i = 0; i < nh && i < L.l->n_h; i++)
		lee_hoja(&L, L.l->h[i], partes[i]);
	/* las fechas de 1904: a las de 1900 */
	if (L.fecha1904)
		for (i = 0; i < L.l->n_h; i++) {
			hoja *h = L.l->h[i];
			u32 j;

			for (j = 0; j < h->n_cel; j++)
				if (h->cel[j].v.t == V_NUM && !h->cel[j].f && fmt_es_fecha(lib_fmt(L.l, lib_estilo(L.l, h->cel[j].estilo)->numfmt)))
					h->cel[j].v.n += 1462;
		}
	xml_suelta(wb);
	for (i = 0; i < L.n_ss; i++)
		so_suelta(L.ss[i]);
	so_suelta(L.ss);
	for (i = 0; i < L.n_nf; i++)
		so_suelta(L.nf[i]);
	so_suelta(L.nf);
	so_suelta(L.nf_id);
	so_suelta(L.le);
	so_suelta(L.re);
	so_suelta(L.bo);
	so_suelta(L.xf);
	so_suelta(L.dxf);
	zip_cierra(L.z);
	{
		hoja *h = lib_hoja(L.l, L.l->activa);

		(void)h;
	}
	return L.l;
}

/* ===================================================================== */
/*  Escribir                                                             */
/* ===================================================================== */
typedef struct {
	bufer *lista;		/* las piezas (cada una, un texto acabado en 0) */
	char **v;
	u32 n, cabe;
} tabla_txt;

/* el número de la pieza (la mete si no está) */
static u32 pieza(tabla_txt *t, const char *s)
{
	u32 i;
	char **n;

	for (i = 0; i < t->n; i++)
		if (iguales(t->v[i], s))
			return i;
	if (t->n >= t->cabe) {
		u32 c = t->cabe ? t->cabe * 2 : 16;

		n = so_repide(t->v, c * sizeof(char *));
		if (!n)
			return 0;
		t->v = n;
		t->cabe = c;
	}
	t->v[t->n] = duplica(s);
	return t->n++;
}

static void tabla_suelta(tabla_txt *t)
{
	u32 i;

	for (i = 0; i < t->n; i++)
		so_suelta(t->v[i]);
	so_suelta(t->v);
}

/* los textos compartidos: con su dispersión (puede haber muchos) */
typedef struct {
	char **v;
	u32 n, cabe;
	u32 *tab;
	u32 cabe_tab;
	u32 usos;
} compartidos;

static u32 firma(const char *s)
{
	u32 h = 2166136261u;

	for (; *s; s++)
		h = (h ^ (u8)*s) * 16777619u;
	return h;
}

static u32 comp_mete(compartidos *c, const char *s)
{
	u32 m, i;

	c->usos++;
	if ((c->n + 1) * 2 > c->cabe_tab) {
		u32 nc = c->cabe_tab ? c->cabe_tab * 2 : 1024, k;
		u32 *t = so_pide(nc * sizeof(u32));

		if (!t)
			return 0;
		memset(t, 0, nc * sizeof(u32));
		for (k = 0; k < c->n; k++) {
			for (i = firma(c->v[k]) & (nc - 1); t[i]; i = (i + 1) & (nc - 1))
				;
			t[i] = k + 1;
		}
		so_suelta(c->tab);
		c->tab = t;
		c->cabe_tab = nc;
	}
	m = c->cabe_tab - 1;
	for (i = firma(s) & m; c->tab[i]; i = (i + 1) & m)
		if (iguales(c->v[c->tab[i] - 1], s))
			return c->tab[i] - 1;
	if (c->n >= c->cabe) {
		u32 nc = c->cabe ? c->cabe * 2 : 256;
		char **n = so_repide(c->v, nc * sizeof(char *));

		if (!n)
			return 0;
		c->v = n;
		c->cabe = nc;
	}
	c->v[c->n] = duplica(s);
	c->tab[i] = c->n + 1;
	return c->n++;
}

static void pon_color(bufer *b, const char *el, u32 c)
{
	buf_fmt(b, "<%s rgb=\"FF%06X\"/>", el, c & 0xFFFFFF);
}

static void pon_num(bufer *b, double x)
{
	char t[48];

	num_a_texto_c(x, t, sizeof t);
	buf_txt(b, t);
}

static const char *nombre_trama(int t)
{
	switch (t) {
	case TR_SOLIDO: return "solid";
	case TR_GRIS75: return "darkGray";
	case TR_GRIS50: return "mediumGray";
	case TR_GRIS25: return "lightGray";
	case TR_GRIS12: return "gray125";
	case TR_HORIZ: return "darkHorizontal";
	case TR_VERT: return "darkVertical";
	case TR_DIAG_ABAJO: return "darkDown";
	case TR_DIAG_ARRIBA: return "darkUp";
	case TR_CUADRICULA: return "darkGrid";
	case TR_ENREJADO: return "darkTrellis";
	}
	return "none";
}

static const char *nombre_borde(int b)
{
	switch (b) {
	case BR_FINO: return "thin";
	case BR_MEDIO: return "medium";
	case BR_GRUESO: return "thick";
	case BR_DOBLE: return "double";
	case BR_PUNTOS: return "dotted";
	case BR_GUIONES: return "dashed";
	}
	return NULL;
}

/* los estilos: styles.xml; en *xf, el número de xf de cada estilo de Cuentas */
static void escribe_estilos(libro *l, bufer *b, u32 *xf, u32 *dxf_de, const condicional **conds, u32 n_conds)
{
	tabla_txt fuentes = { 0 }, rellenos = { 0 }, bordes = { 0 }, xfs = { 0 }, nfs = { 0 };
	u32 i, j;
	bufer t = { 0 };

	/* los dos rellenos de Excel de siempre */
	pieza(&rellenos, "<fill><patternFill patternType=\"none\"/></fill>");
	pieza(&rellenos, "<fill><patternFill patternType=\"gray125\"/></fill>");
	for (i = 0; i < l->n_est; i++) {
		const estilo *e = lib_estilo(l, i);
		u32 fi, ri, bi, ni = 0;
		const char *cod = lib_fmt(l, e->numfmt);

		/* la letra */
		t.n = 0;
		buf_txt(&t, "<font>");
		if (e->negrita)
			buf_txt(&t, "<b/>");
		if (e->cursiva)
			buf_txt(&t, "<i/>");
		if (e->tachado)
			buf_txt(&t, "<strike/>");
		if (e->subrayado)
			buf_txt(&t, "<u/>");
		buf_fmt(&t, "<sz val=\"");
		pon_num(&t, e->tam / 2.0);
		buf_txt(&t, "\"/>");
		if (e->color != CU_AUTO)
			pon_color(&t, "color", e->color);
		buf_txt(&t, "<name");
		xml_pon_atr(&t, "val", e->familia);
		buf_txt(&t, "/><family val=\"2\"/></font>");
		buf_c(&t, 0);
		fi = pieza(&fuentes, (char *)t.d);
		/* el relleno */
		t.n = 0;
		if (e->trama == TR_NADA)
			ri = 0;
		else {
			buf_fmt(&t, "<fill><patternFill patternType=\"%s\">", nombre_trama(e->trama));
			if (e->trama == TR_SOLIDO) {
				pon_color(&t, "fgColor", e->fondo);
				buf_txt(&t, "<bgColor indexed=\"64\"/>");
			} else {
				pon_color(&t, "fgColor", e->color_trama);
				pon_color(&t, "bgColor", e->fondo);
			}
			buf_txt(&t, "</patternFill></fill>");
			buf_c(&t, 0);
			ri = pieza(&rellenos, (char *)t.d);
		}
		/* los bordes */
		t.n = 0;
		buf_txt(&t, "<border>");
		{
			static const char *const lados[4] = { "left", "right", "top", "bottom" };
			static const int bo[4] = { BO_IZQ, BO_DER, BO_ARRIBA, BO_ABAJO };

			for (j = 0; j < 4; j++) {
				const char *nb = nombre_borde(e->borde[bo[j]]);

				if (nb) {
					buf_fmt(&t, "<%s style=\"%s\">", lados[j], nb);
					pon_color(&t, "color", e->color_borde[bo[j]]);
					buf_fmt(&t, "</%s>", lados[j]);
				} else
					buf_fmt(&t, "<%s/>", lados[j]);
			}
		}
		buf_txt(&t, "<diagonal/></border>");
		buf_c(&t, 0);
		bi = pieza(&bordes, (char *)t.d);
		/* el formato del número (los propios, desde el 164) */
		if (!iguales(cod, "General"))
			ni = 164 + pieza(&nfs, cod);
		/* el xf */
		t.n = 0;
		buf_fmt(&t, "<xf numFmtId=\"%u\" fontId=\"%u\" fillId=\"%u\" borderId=\"%u\" xfId=\"0\"", ni, fi, ri, bi);
		if (ni)
			buf_txt(&t, " applyNumberFormat=\"1\"");
		if (fi)
			buf_txt(&t, " applyFont=\"1\"");
		if (ri)
			buf_txt(&t, " applyFill=\"1\"");
		if (bi)
			buf_txt(&t, " applyBorder=\"1\"");
		if (e->alin_h || e->alin_v || e->ajustar || e->sangria || e->girado) {
			static const char *const ah[] = { "general", "left", "center", "right", "fill", "justify", "centerContinuous" };
			static const char *const av[] = { "bottom", "center", "top" };

			buf_txt(&t, " applyAlignment=\"1\"><alignment");
			if (e->alin_h)
				buf_fmt(&t, " horizontal=\"%s\"", ah[e->alin_h % 7]);
			if (e->alin_v)
				buf_fmt(&t, " vertical=\"%s\"", av[e->alin_v % 3]);
			if (e->ajustar)
				buf_txt(&t, " wrapText=\"1\"");
			if (e->sangria)
				buf_fmt(&t, " indent=\"%u\"", e->sangria);
			if (e->girado)
				buf_fmt(&t, " textRotation=\"%u\"", e->girado);
			buf_txt(&t, "/></xf>");
		} else
			buf_txt(&t, "/>");
		buf_c(&t, 0);
		xf[i] = pieza(&xfs, (char *)t.d);
	}
	buf_txt(b, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
		   "<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">");
	if (nfs.n) {
		buf_fmt(b, "<numFmts count=\"%u\">", nfs.n);
		for (i = 0; i < nfs.n; i++) {
			/* (lo que no es ASCII fuera de comillas, con «\\» delante: «€» -> «\\€») */
			bufer q = { 0 };
			const char *p = nfs.v[i];
			int com = 0;

			for (; *p; p++) {
				if (*p == '"')
					com = !com;
				if (!com && (u8)*p >= 0x80 && ((u8)*p & 0xC0) != 0x80)
					buf_c(&q, '\\');
				buf_c(&q, (u8)*p);
			}
			buf_c(&q, 0);
			buf_fmt(b, "<numFmt numFmtId=\"%u\"", 164 + i);
			xml_pon_atr(b, "formatCode", q.d ? (char *)q.d : "General");
			buf_suelta(&q);
			buf_txt(b, "/>");
		}
		buf_txt(b, "</numFmts>");
	}
	buf_fmt(b, "<fonts count=\"%u\">", fuentes.n);
	for (i = 0; i < fuentes.n; i++)
		buf_txt(b, fuentes.v[i]);
	buf_fmt(b, "</fonts><fills count=\"%u\">", rellenos.n);
	for (i = 0; i < rellenos.n; i++)
		buf_txt(b, rellenos.v[i]);
	buf_fmt(b, "</fills><borders count=\"%u\">", bordes.n);
	for (i = 0; i < bordes.n; i++)
		buf_txt(b, bordes.v[i]);
	buf_txt(b, "</borders><cellStyleXfs count=\"1\"><xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\"/></cellStyleXfs>");
	buf_fmt(b, "<cellXfs count=\"%u\">", xfs.n);
	for (i = 0; i < xfs.n; i++)
		buf_txt(b, xfs.v[i]);
	buf_txt(b, "</cellXfs><cellStyles count=\"1\"><cellStyle name=\"Normal\" xfId=\"0\" builtinId=\"0\"/></cellStyles>");
	/* los dxf de los condicionales */
	buf_fmt(b, "<dxfs count=\"%u\">", n_conds);
	for (i = 0; i < n_conds; i++) {
		const condicional *d = conds[i];

		buf_txt(b, "<dxf>");
		if (d->negrita || d->cursiva || d->color != CU_AUTO) {
			buf_txt(b, "<font>");
			if (d->negrita)
				buf_txt(b, "<b/>");
			if (d->cursiva)
				buf_txt(b, "<i/>");
			if (d->color != CU_AUTO)
				pon_color(b, "color", d->color);
			buf_txt(b, "</font>");
		}
		if (d->fondo != CU_AUTO) {
			buf_txt(b, "<fill><patternFill patternType=\"solid\">");
			pon_color(b, "bgColor", d->fondo);
			buf_txt(b, "</patternFill></fill>");
		}
		buf_txt(b, "</dxf>");
		dxf_de[i] = i;
	}
	buf_txt(b, "</dxfs><tableStyles count=\"0\"/></styleSheet>");
	tabla_suelta(&fuentes);
	tabla_suelta(&rellenos);
	tabla_suelta(&bordes);
	tabla_suelta(&xfs);
	tabla_suelta(&nfs);
	buf_suelta(&t);
}

static void pon_ref(bufer *b, u32 f, u32 c)
{
	char t[16];

	cu_ref_texto(f, c, t);
	buf_txt(b, t);
}

static void pon_rango_x(bufer *b, rango r)
{
	pon_ref(b, r.f0, r.c0);
	if (r.f0 != r.f1 || r.c0 != r.c1) {
		buf_c(b, ':');
		pon_ref(b, r.f1, r.c1);
	}
}

/* una fórmula para Excel (en inglés; las de Excel 2010 y después, con _xlfn.) */
static void pon_formula(libro *l, hoja *h, const formula *fm, bufer *b)
{
	bufer w = { 0 };
	u32 i;

	fm_escribe(l, fm, h, &CU_ARCHIVO, &w);
	buf_c(&w, 0);
	if (!w.d) {
		buf_suelta(&w);
		return;
	}
	{
		static const char *const nuevas[] = { "DAYS(", "IFNA(", NULL };
		const char *s = (const char *)w.d;

		for (i = 0; s[i];) {
			int k, puesta = 0;

			for (k = 0; nuevas[k]; k++)
				if (empieza(s + i, nuevas[k]) && (i == 0 || !((s[i - 1] >= 'A' && s[i - 1] <= 'Z') || s[i - 1] == '.' || s[i - 1] == '_'))) {
					buf_txt(b, "_xlfn.");
					buf_txt(b, nuevas[k]);
					i += largo(nuevas[k]);
					puesta = 1;
					break;
				}
			if (!puesta) {
				char x[2] = { s[i], 0 };

				buf_xml(b, x, 1);
				i++;
			}
		}
	}
	buf_suelta(&w);
}

static void escribe_hoja(libro *l, hoja *h, u32 indice, const u32 *xf, compartidos *ss, bufer *b, u32 *dxf_base, int con_comentarios)
{
	u32 n, i, *k, uf, uc, f;
	u32 fila_ant = CU_SIN_FILA;

	hj_usado(h, &uf, &uc);
	buf_txt(b, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
		   "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
		   "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">");
	if (h->pag.encajar_an || h->pag.encajar_al)
		buf_txt(b, "<sheetPr><pageSetUpPr fitToPage=\"1\"/></sheetPr>");
	buf_txt(b, "<dimension ref=\"");
	if (uf == CU_SIN_FILA)
		buf_txt(b, "A1");
	else
		pon_rango_x(b, cu_rango(0, 0, uf, uc));
	buf_txt(b, "\"/><sheetViews><sheetView workbookViewId=\"0\"");
	if (indice == l->activa)
		buf_txt(b, " tabSelected=\"1\"");
	if (h->sin_rejilla)
		buf_txt(b, " showGridLines=\"0\"");
	if (h->zoom && h->zoom != 100)
		buf_fmt(b, " zoomScale=\"%u\" zoomScaleNormal=\"%u\"", h->zoom, h->zoom);
	buf_txt(b, ">");
	if (h->inm_f || h->inm_c) {
		buf_txt(b, "<pane");
		if (h->inm_c)
			buf_fmt(b, " xSplit=\"%u\"", h->inm_c);
		if (h->inm_f)
			buf_fmt(b, " ySplit=\"%u\"", h->inm_f);
		buf_txt(b, " topLeftCell=\"");
		pon_ref(b, h->inm_f, h->inm_c);
		buf_fmt(b, "\" activePane=\"%s\" state=\"frozen\"/>", h->inm_f && h->inm_c ? "bottomRight" : h->inm_f ? "bottomLeft" : "topRight");
	}
	buf_txt(b, "<selection");
	if (h->inm_f || h->inm_c)
		buf_fmt(b, " pane=\"%s\"", h->inm_f && h->inm_c ? "bottomRight" : h->inm_f ? "bottomLeft" : "topRight");
	buf_txt(b, " activeCell=\"");
	pon_ref(b, h->cur_f, h->cur_c);
	buf_txt(b, "\" sqref=\"");
	pon_rango_x(b, h->sel);
	buf_txt(b, "\"/></sheetView></sheetViews><sheetFormatPr ");
	if (h->ancho_def != CU_ANCHO) {
		buf_txt(b, "defaultColWidth=\"");
		pon_num(b, ancho_de_px(h->ancho_def) + 0.71);
		buf_txt(b, "\" ");
	}
	buf_txt(b, "defaultRowHeight=\"15\"/>");
	/* las columnas (seguidas las iguales) */
	{
		int hay = 0;
		u32 c = 0;

		while (c < CU_COLS) {
			u32 d = c;

			if (h->ancho[c] == h->ancho_def && !h->oculta_c[c] && !h->estilo_col[c]) {
				c++;
				continue;
			}
			while (d + 1 < CU_COLS && h->ancho[d + 1] == h->ancho[c] && h->oculta_c[d + 1] == h->oculta_c[c] && h->estilo_col[d + 1] == h->estilo_col[c])
				d++;
			if (!hay)
				buf_txt(b, "<cols>");
			hay = 1;
			buf_fmt(b, "<col min=\"%u\" max=\"%u\" width=\"", c + 1, d + 1);
			pon_num(b, ancho_de_px(h->ancho[c]));
			buf_txt(b, "\"");
			if (h->ancho[c] != h->ancho_def)
				buf_txt(b, " customWidth=\"1\"");
			if (h->oculta_c[c])
				buf_txt(b, " hidden=\"1\"");
			if (h->estilo_col[c])
				buf_fmt(b, " style=\"%u\"", xf[h->estilo_col[c]]);
			buf_txt(b, "/>");
			c = d + 1;
		}
		if (hay)
			buf_txt(b, "</cols>");
	}
	/* las filas y sus celdas */
	buf_txt(b, "<sheetData>");
	k = hj_claves_ordenadas(h, &n);
#define FILA_ESPECIAL(x) ((h->alto && h->alto[x]) || hj_fila_oculta(h, x) || (h->estilo_fila && h->estilo_fila[x]))
#define ABRE_FILA(x) do { buf_fmt(b, "<row r=\"%u\"", (x) + 1); \
	if (h->alto && h->alto[x]) { buf_txt(b, " ht=\""); pon_num(b, h->alto[x] * 0.75); buf_txt(b, "\" customHeight=\"1\""); } \
	if (hj_fila_oculta(h, x)) { buf_txt(b, " hidden=\"1\""); } \
	if (h->estilo_fila && h->estilo_fila[x]) { buf_fmt(b, " s=\"%u\" customFormat=\"1\"", xf[h->estilo_fila[x]]); } buf_txt(b, ">"); } while (0)
	f = 0;
	for (i = 0; k && i <= n; i++) {
		u32 kf = i < n ? CU_FILA(k[i]) : CU_FILAS;

		/* las filas sin celdas pero con alto, ocultas o con estilo, antes que esta */
		if (kf != fila_ant) {
			if (fila_ant != CU_SIN_FILA)
				buf_txt(b, "</row>");
			for (f = fila_ant == CU_SIN_FILA ? 0 : fila_ant + 1; f < kf && f < CU_FILAS; f++)
				if (FILA_ESPECIAL(f)) {
					ABRE_FILA(f);
					buf_txt(b, "</row>");
				}
			if (i == n)
				break;
			ABRE_FILA(kf);
			fila_ant = kf;
		}
		{
			celda *x = hj_celda_k(h, k[i]);
			char t[48];

			if (!x)
				continue;
			buf_txt(b, "<c r=\"");
			pon_ref(b, CU_FILA(k[i]), CU_COL(k[i]));
			buf_c(b, '"');
			if (x->estilo && xf[x->estilo])
				buf_fmt(b, " s=\"%u\"", xf[x->estilo]);
			if (x->f) {
				if (x->v.t == V_TXT)
					buf_txt(b, " t=\"str\"");
				else if (x->v.t == V_LOG)
					buf_txt(b, " t=\"b\"");
				else if (x->v.t == V_ERR)
					buf_txt(b, " t=\"e\"");
				buf_txt(b, "><f>");
				pon_formula(l, h, x->f, b);
				buf_txt(b, "</f>");
				switch (x->v.t) {
				case V_NUM:
					num_a_texto_c(x->v.n, t, sizeof t);
					buf_fmt(b, "<v>%s</v>", t);
					break;
				case V_TXT:
					buf_txt(b, "<v>");
					buf_xml(b, x->v.s ? x->v.s : "", x->v.s ? largo(x->v.s) : 0);
					buf_txt(b, "</v>");
					break;
				case V_LOG:
					buf_fmt(b, "<v>%u</v>", x->v.n != 0);
					break;
				case V_ERR:
					buf_fmt(b, "<v>%s</v>", x->v.e == ER_CIRC ? "#VALUE!" : cu_error_texto(x->v.e, 1));
					break;
				}
				buf_txt(b, "</c>");
				continue;
			}
			switch (x->v.t) {
			case V_NUM:
				num_a_texto_c(x->v.n, t, sizeof t);
				buf_fmt(b, "><v>%s</v></c>", t);
				break;
			case V_TXT:
				buf_fmt(b, " t=\"s\"><v>%u</v></c>", comp_mete(ss, x->v.s ? x->v.s : ""));
				break;
			case V_LOG:
				buf_fmt(b, " t=\"b\"><v>%u</v></c>", x->v.n != 0);
				break;
			case V_ERR:
				buf_fmt(b, " t=\"e\"><v>%s</v></c>", x->v.e == ER_CIRC ? "#VALUE!" : cu_error_texto(x->v.e, 1));
				break;
			default:
				buf_txt(b, "/>");
			}
		}
	}
	if (!k || n == 0) {
		/* (sin celdas: las filas especiales) */
		if (fila_ant != CU_SIN_FILA)
			buf_txt(b, "</row>");
		if (!n)
			for (f = 0; f < CU_FILAS; f++)
				if (FILA_ESPECIAL(f)) {
					ABRE_FILA(f);
					buf_txt(b, "</row>");
				}
	} else {
		/* las que quedan detrás de la última celda */
		for (f = fila_ant + 1; f < CU_FILAS; f++)
			if (FILA_ESPECIAL(f)) {
				ABRE_FILA(f);
				buf_txt(b, "</row>");
			}
	}
	so_suelta(k);
	buf_txt(b, "</sheetData>");
#undef FILA_ESPECIAL
#undef ABRE_FILA
	if (h->hay_filtro) {
		buf_txt(b, "<autoFilter ref=\"");
		pon_rango_x(b, h->filtro);
		buf_txt(b, "\"/>");
	}
	if (h->n_comb) {
		buf_fmt(b, "<mergeCells count=\"%u\">", h->n_comb);
		for (i = 0; i < h->n_comb; i++) {
			buf_txt(b, "<mergeCell ref=\"");
			pon_rango_x(b, h->comb[i]);
			buf_txt(b, "\"/>");
		}
		buf_txt(b, "</mergeCells>");
	}
	for (i = 0; i < h->n_cond; i++) {
		const condicional *d = &h->cond[i];
		static const char *const ops[] = { "greaterThan", "lessThan", "between", "equal", "notEqual", "greaterThanOrEqual", "lessThanOrEqual" };

		buf_txt(b, "<conditionalFormatting sqref=\"");
		pon_rango_x(b, d->r);
		if (d->cond == FC_TEXTO) {
			char r0[16];

			cu_ref_texto(d->r.f0, d->r.c0, r0);
			buf_fmt(b, "\"><cfRule type=\"containsText\" dxfId=\"%u\" priority=\"%u\" operator=\"containsText\"", *dxf_base + i, i + 1);
			xml_pon_atr(b, "text", d->texto);
			buf_txt(b, "><formula>NOT(ISERROR(SEARCH(\"");
			buf_xml(b, d->texto, largo(d->texto));
			buf_fmt(b, "\",%s)))</formula></cfRule></conditionalFormatting>", r0);
		} else {
			buf_fmt(b, "\"><cfRule type=\"cellIs\" dxfId=\"%u\" priority=\"%u\" operator=\"%s\"><formula>", *dxf_base + i, i + 1, ops[d->cond < 7 ? d->cond : 0]);
			pon_num(b, d->v1);
			buf_txt(b, "</formula>");
			if (d->cond == FC_ENTRE) {
				buf_txt(b, "<formula>");
				pon_num(b, d->v2);
				buf_txt(b, "</formula>");
			}
			buf_txt(b, "</cfRule></conditionalFormatting>");
		}
	}
	*dxf_base += h->n_cond;
	if (h->pag.rejilla || h->pag.titulos || h->pag.centrar_h || h->pag.centrar_v) {
		buf_txt(b, "<printOptions");
		if (h->pag.centrar_h)
			buf_txt(b, " horizontalCentered=\"1\"");
		if (h->pag.centrar_v)
			buf_txt(b, " verticalCentered=\"1\"");
		if (h->pag.titulos)
			buf_txt(b, " headings=\"1\"");
		if (h->pag.rejilla)
			buf_txt(b, " gridLines=\"1\"");
		buf_txt(b, "/>");
	}
	{
		static const char *const lados[4] = { "left", "top", "right", "bottom" };
		int j;

		buf_txt(b, "<pageMargins");
		for (j = 0; j < 4; j++) {
			buf_fmt(b, " %s=\"", lados[j]);
			pon_num(b, h->pag.marg[j] / 2.54);
			buf_txt(b, "\"");
		}
		buf_txt(b, " header=\"0.3\" footer=\"0.3\"/>");
	}
	{
		static const u32 papeles[5] = { 9, 1, 8, 11, 5 };

		buf_fmt(b, "<pageSetup paperSize=\"%u\"", papeles[h->pag.papel % 5]);
		if (h->pag.escala != 100)
			buf_fmt(b, " scale=\"%u\"", h->pag.escala);
		if (h->pag.encajar_an || h->pag.encajar_al)
			buf_fmt(b, " fitToWidth=\"%u\" fitToHeight=\"%u\"", h->pag.encajar_an, h->pag.encajar_al);
		buf_fmt(b, " orientation=\"%s\"/>", h->pag.horizontal ? "landscape" : "portrait");
	}
	if (h->pag.encabezado[0] || h->pag.pie[0]) {
		buf_txt(b, "<headerFooter>");
		if (h->pag.encabezado[0]) {
			buf_txt(b, "<oddHeader>&amp;C");
			buf_xml(b, h->pag.encabezado, largo(h->pag.encabezado));
			buf_txt(b, "</oddHeader>");
		}
		if (h->pag.pie[0]) {
			buf_txt(b, "<oddFooter>&amp;C");
			buf_xml(b, h->pag.pie, largo(h->pag.pie));
			buf_txt(b, "</oddFooter>");
		}
		buf_txt(b, "</headerFooter>");
	}
	if (h->n_salto_f) {
		buf_fmt(b, "<rowBreaks count=\"%u\" manualBreakCount=\"%u\">", h->n_salto_f, h->n_salto_f);
		for (i = 0; i < h->n_salto_f; i++)
			buf_fmt(b, "<brk id=\"%u\" max=\"16383\" man=\"1\"/>", h->salto_f[i]);
		buf_txt(b, "</rowBreaks>");
	}
	if (h->n_salto_c) {
		buf_fmt(b, "<colBreaks count=\"%u\" manualBreakCount=\"%u\">", h->n_salto_c, h->n_salto_c);
		for (i = 0; i < h->n_salto_c; i++)
			buf_fmt(b, "<brk id=\"%u\" max=\"1048575\" man=\"1\"/>", h->salto_c[i]);
		buf_txt(b, "</colBreaks>");
	}
	if (con_comentarios)
		buf_txt(b, "<legacyDrawing r:id=\"rId2\"/>");
	buf_txt(b, "</worksheet>");
}

static void escribe_comentarios(hoja *h, bufer *c, bufer *v)
{
	u32 i;

	buf_txt(c, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
		   "<comments xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\"><authors>");
	for (i = 0; i < h->n_com; i++) {
		buf_txt(c, "<author>");
		buf_xml(c, h->com[i].autor, largo(h->com[i].autor));
		buf_txt(c, "</author>");
	}
	buf_txt(c, "</authors><commentList>");
	for (i = 0; i < h->n_com; i++) {
		buf_txt(c, "<comment ref=\"");
		pon_ref(c, h->com[i].f, h->com[i].c);
		buf_fmt(c, "\" authorId=\"%u\"><text><t xml:space=\"preserve\">", i);
		buf_xml(c, h->com[i].texto ? h->com[i].texto : "", h->com[i].texto ? largo(h->com[i].texto) : 0);
		buf_txt(c, "</t></text></comment>");
	}
	buf_txt(c, "</commentList></comments>");
	/* el dibujo VML de los cuadritos (sin él Excel no los enseña) */
	buf_txt(v, "<xml xmlns:v=\"urn:schemas-microsoft-com:vml\" xmlns:o=\"urn:schemas-microsoft-com:office:office\" "
		   "xmlns:x=\"urn:schemas-microsoft-com:office:excel\">"
		   "<o:shapelayout v:ext=\"edit\"><o:idmap v:ext=\"edit\" data=\"1\"/></o:shapelayout>"
		   "<v:shapetype id=\"_x0000_t202\" coordsize=\"21600,21600\" o:spt=\"202\" path=\"m,l,21600r21600,l21600,xe\">"
		   "<v:stroke joinstyle=\"miter\"/><v:path gradientshapeok=\"t\" o:connecttype=\"rect\"/></v:shapetype>");
	for (i = 0; i < h->n_com; i++)
		buf_fmt(v, "<v:shape id=\"_x0000_s%u\" type=\"#_x0000_t202\" style=\"position:absolute;margin-left:80pt;margin-top:2pt;"
			   "width:110pt;height:60pt;z-index:%u;visibility:hidden\" fillcolor=\"#ffffe1\" o:insetmode=\"auto\">"
			   "<v:fill color2=\"#ffffe1\"/><v:shadow on=\"t\" color=\"black\" obscured=\"t\"/><v:path o:connecttype=\"none\"/>"
			   "<v:textbox style=\"mso-direction-alt:auto\"><div style=\"text-align:left\"></div></v:textbox>"
			   "<x:ClientData ObjectType=\"Note\"><x:MoveWithCells/><x:SizeWithCells/><x:AutoFill>False</x:AutoFill>"
			   "<x:Row>%u</x:Row><x:Column>%u</x:Column></x:ClientData></v:shape>",
			1025 + i, i + 1, h->com[i].f, h->com[i].c);
	buf_txt(v, "</xml>");
}

int xlsx_escribe(libro *l, bufer *out, char *err, u32 cabe)
{
	zip_escritor *z = zip_nuevo();
	bufer b = { 0 };
	u32 *xf = NULL, i, ncond = 0, dxf_base = 0, *dxf_de = NULL;
	const condicional **conds = NULL;
	compartidos ss = { 0 };
	int mal = 0;

	if (!z) {
		copia(err, "No hay memoria", cabe);
		return 1;
	}
	xf = so_pide(l->n_est * sizeof(u32) + 4);
	for (i = 0; i < l->n_h; i++)
		ncond += l->h[i]->n_cond;
	conds = so_pide((ncond + 1) * sizeof(condicional *));
	dxf_de = so_pide((ncond + 1) * sizeof(u32));
	if (!xf || !conds || !dxf_de) {
		so_suelta(xf);
		so_suelta(conds);
		so_suelta(dxf_de);
		zip_deja(z);
		copia(err, "No hay memoria", cabe);
		return 1;
	}
	ncond = 0;
	for (i = 0; i < l->n_h; i++) {
		u32 j;

		for (j = 0; j < l->h[i]->n_cond; j++)
			conds[ncond++] = &l->h[i]->cond[j];
	}
#define METE(nombre) do { if (b.mal || zip_mete(z, nombre, b.d, b.n, 0)) mal = 1; b.n = 0; } while (0)
	/* [Content_Types].xml */
	buf_txt(&b, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
		    "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
		    "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
		    "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
		    "<Default Extension=\"vml\" ContentType=\"application/vnd.openxmlformats-officedocument.vmlDrawing\"/>"
		    "<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>");
	for (i = 0; i < l->n_h; i++) {
		buf_fmt(&b, "<Override PartName=\"/xl/worksheets/sheet%u.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>", i + 1);
		if (l->h[i]->n_com)
			buf_fmt(&b, "<Override PartName=\"/xl/comments%u.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.comments+xml\"/>", i + 1);
	}
	buf_txt(&b, "<Override PartName=\"/xl/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml\"/>"
		    "<Override PartName=\"/xl/sharedStrings.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sharedStrings+xml\"/>"
		    "<Override PartName=\"/docProps/core.xml\" ContentType=\"application/vnd.openxmlformats-package.core-properties+xml\"/>"
		    "<Override PartName=\"/docProps/app.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.extended-properties+xml\"/>"
		    "</Types>");
	METE("[Content_Types].xml");
	buf_txt(&b, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
		    "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
		    "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>"
		    "<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties\" Target=\"docProps/core.xml\"/>"
		    "<Relationship Id=\"rId3\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/extended-properties\" Target=\"docProps/app.xml\"/>"
		    "</Relationships>");
	METE("_rels/.rels");
	{
		int a, m, d, h2, mi, s;

		so_ahora(&a, &m, &d, &h2, &mi, &s);
		buf_txt(&b, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
			    "<cp:coreProperties xmlns:cp=\"http://schemas.openxmlformats.org/package/2006/metadata/core-properties\" "
			    "xmlns:dc=\"http://purl.org/dc/elements/1.1/\" xmlns:dcterms=\"http://purl.org/dc/terms/\" "
			    "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"><dc:creator>");
		buf_xml(&b, l->autor, largo(l->autor));
		buf_fmt(&b, "</dc:creator><dcterms:created xsi:type=\"dcterms:W3CDTF\">%04d-%02d-%02dT%02d:%02d:%02dZ</dcterms:created></cp:coreProperties>", a, m, d, h2, mi, s);
		METE("docProps/core.xml");
	}
	buf_txt(&b, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
		    "<Properties xmlns=\"http://schemas.openxmlformats.org/officeDocument/2006/extended-properties\">"
		    "<Application>Cuentas 2 (Hispa-Pentium)</Application></Properties>");
	METE("docProps/app.xml");
	/* el libro */
	buf_txt(&b, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
		    "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
		    "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
		    "<workbookPr/>");
	buf_fmt(&b, "<bookViews><workbookView activeTab=\"%u\"/></bookViews><sheets>", l->activa);
	for (i = 0; i < l->n_h; i++) {
		buf_txt(&b, "<sheet");
		xml_pon_atr(&b, "name", l->h[i]->nombre);
		buf_fmt(&b, " sheetId=\"%u\"%s r:id=\"rId%u\"/>", i + 1, l->h[i]->oculta ? " state=\"hidden\"" : "", i + 1);
	}
	buf_txt(&b, "</sheets>");
	{
		int hay = l->n_nom > 0;

		for (i = 0; i < l->n_h; i++)
			if (l->h[i]->hay_area)
				hay = 1;
		if (hay) {
			buf_txt(&b, "<definedNames>");
			for (i = 0; i < l->n_h; i++)
				if (l->h[i]->hay_area) {
					hoja *h = l->h[i];
					char t[16], u[16];

					cu_ref_texto(h->area.f0, h->area.c0, t);
					cu_ref_texto(h->area.f1, h->area.c1, u);
					buf_fmt(&b, "<definedName name=\"_xlnm.Print_Area\" localSheetId=\"%u\">'", i);
					{
						const char *p;

						for (p = h->nombre; *p; p++) {
							char x[2] = { *p, 0 };

							if (*p == '\'')
								buf_txt(&b, "''");
							else
								buf_xml(&b, x, 1);
						}
					}
					/* (con $) */
					{
						char t2[20], u2[20];
						u32 a = 0, c2 = 0;

						t2[c2++] = '$';
						for (a = 0; t[a] >= 'A'; a++)
							t2[c2++] = t[a];
						t2[c2++] = '$';
						copia(t2 + c2, t + a, sizeof t2 - c2);
						c2 = 0;
						u2[c2++] = '$';
						for (a = 0; u[a] >= 'A'; a++)
							u2[c2++] = u[a];
						u2[c2++] = '$';
						copia(u2 + c2, u + a, sizeof u2 - c2);
						buf_fmt(&b, "'!%s:%s</definedName>", t2, u2);
					}
				}
			for (i = 0; i < l->n_nom; i++) {
				buf_txt(&b, "<definedName");
				xml_pon_atr(&b, "name", l->nom[i].nombre);
				buf_txt(&b, ">");
				pon_formula(l, NULL, l->nom[i].f, &b);
				buf_txt(&b, "</definedName>");
			}
			buf_txt(&b, "</definedNames>");
		}
	}
	buf_txt(&b, "<calcPr calcId=\"191029\"/></workbook>");
	METE("xl/workbook.xml");
	buf_txt(&b, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
		    "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">");
	for (i = 0; i < l->n_h; i++)
		buf_fmt(&b, "<Relationship Id=\"rId%u\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet%u.xml\"/>", i + 1, i + 1);
	buf_fmt(&b, "<Relationship Id=\"rId%u\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/>", l->n_h + 1);
	buf_fmt(&b, "<Relationship Id=\"rId%u\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/sharedStrings\" Target=\"sharedStrings.xml\"/>", l->n_h + 2);
	buf_txt(&b, "</Relationships>");
	METE("xl/_rels/workbook.xml.rels");
	escribe_estilos(l, &b, xf, dxf_de, conds, ncond);
	METE("xl/styles.xml");
	for (i = 0; i < l->n_h && !mal; i++) {
		char nom[64];
		hoja *h = l->h[i];

		escribe_hoja(l, h, i, xf, &ss, &b, &dxf_base, h->n_com > 0);
		fmt(nom, sizeof nom, "xl/worksheets/sheet%u.xml", i + 1);
		METE(nom);
		if (h->n_com) {
			bufer v = { 0 };

			buf_fmt(&b, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
				    "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
				    "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/comments\" Target=\"../comments%u.xml\"/>"
				    "<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/vmlDrawing\" Target=\"../drawings/vmlDrawing%u.vml\"/>"
				    "</Relationships>", i + 1, i + 1);
			fmt(nom, sizeof nom, "xl/worksheets/_rels/sheet%u.xml.rels", i + 1);
			METE(nom);
			escribe_comentarios(h, &b, &v);
			fmt(nom, sizeof nom, "xl/comments%u.xml", i + 1);
			METE(nom);
			fmt(nom, sizeof nom, "xl/drawings/vmlDrawing%u.vml", i + 1);
			if (v.mal || zip_mete(z, nom, v.d, v.n, 0))
				mal = 1;
			buf_suelta(&v);
		}
	}
	buf_fmt(&b, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
		    "<sst xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" count=\"%u\" uniqueCount=\"%u\">", ss.usos, ss.n);
	for (i = 0; i < ss.n; i++) {
		const char *s = ss.v[i];

		buf_txt(&b, (s[0] == ' ' || (s[0] && s[largo(s) - 1] == ' ') || strchr(s, '\n')) ? "<si><t xml:space=\"preserve\">" : "<si><t>");
		buf_xml(&b, s, largo(s));
		buf_txt(&b, "</t></si>");
	}
	buf_txt(&b, "</sst>");
	METE("xl/sharedStrings.xml");
#undef METE
	buf_suelta(&b);
	for (i = 0; i < ss.n; i++)
		so_suelta(ss.v[i]);
	so_suelta(ss.v);
	so_suelta(ss.tab);
	so_suelta(xf);
	so_suelta(conds);
	so_suelta(dxf_de);
	if (mal) {
		zip_deja(z);
		copia(err, "No hay memoria para guardar el .XLSX", cabe);
		return 1;
	}
	if (zip_acaba(z, out)) {
		copia(err, "No se ha podido hacer el ZIP del .XLSX", cabe);
		return 1;
	}
	return 0;
}
