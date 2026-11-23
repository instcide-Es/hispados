/* f_hoj.c - Cuentas 2: el formato .HOJ, el de Cuentas de Hispa-386
 * (programas/cuentas/archivo.inc y libro.inc): texto en la página 437, una
 * línea por cosa. Se lee todo lo que escribe Cuentas de 386 (las celdas tal
 * como se teclearon, sus formatos de bits F_*, los anchos, los altos, los
 * paneles, el gráfico, las hojas del libro y las «plegadas»).
 *
 * Al guardar va, delante, lo que entiende Cuentas de 386 (que se salta lo
 * que no conoce), y en cada hoja, detrás, en líneas «2x» (la segunda letra
 * no es un espacio: 386 no las mira) y en UTF-8, todo lo de Cuentas 2:
 *   2H	2.0                     esta hoja trae lo de Cuentas 2: lo de 386 se salta
 *   2O	fecha1904	moneda	autor   (en la primera) lo del libro
 *   2N	nombre	fórmula          (en la primera) los nombres definidos
 *   2E	n	...                  un estilo (todas sus cosas; n es el de la hoja)
 *   2C	A1	estilo	tipo	dato   una celda: tipo v (solo estilo), n (número
 *                               exacto), s (texto), b (lógico), e (error), f (fórmula)
 *   2D ancho de siempre, 2W anchos, 2Y columnas ocultas, 2Q estilos de columna,
 *   2R altos, 2Z filas ocultas, 2F estilos de fila, 2M combinadas, 2K comentarios,
 *   2J condicionales, 2G gráficos, 2A autofiltro y 2a sus criterios, 2P área de
 *   impresión, 2B saltos, 2X página, 2I inmovilizar, 2V la vista
 * Los campos van separados por tabuladores; en los textos, \\ \t \n \r.
 * Las fórmulas, en inglés y con punto decimal (como en los .XLSX).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "archivo.h"
#include "formula.h"
#include "numeros.h"
#include "grafico.h"
#include "calculo.h"

/* ---- lo de 386 ---- */
#define F_NEG 0x0001
#define F_CUR 0x0002
#define F_ALIN 0x000C
#define F_COLOR 0x0070
#define F_NUM 0x0380
#define F_DEC 0x3C00
#define F_BABA 0x4000
#define F_BDER 0x8000
#define F_FONDO 0x000F0000
#define F_COMB 0x03F00000
enum { FN_GENERAL, FN_FIJO, FN_MIL, FN_MONEDA, FN_PORC, FN_FECHA };
enum { GT_BARRAS, GT_LINEAS, GT_SECTORES, GT_APILADAS, GT_XY };
/* los índices de la paleta (ventanas/defs.inc) */
static const u8 COLORES386[8] = { 0, 13, 12, 11, 4, 2, 1, 15 };	/* tinta, granate, añil, oliva, terracota, petróleo, marrón, piedra */
static const u8 FONDOS386[9] = { 0, 8, 10, 14, 6, 15, 5, 11, 2 };	/* (ninguno), crema, ocre, turquesa, arena, piedra, terracota claro, oliva, petróleo */
#define AN386 88
#define AL386 18

/* ---- escribir ---- */
static void esc(bufer *b, const char *s)
{
	for (; s && *s; s++) {
		switch (*s) {
		case '\\': buf_txt(b, "\\\\"); break;
		case '\t': buf_txt(b, "\\t"); break;
		case '\n': buf_txt(b, "\\n"); break;
		case '\r': buf_txt(b, "\\r"); break;
		default: buf_c(b, (u8)*s);
		}
	}
}

static void txt437(bufer *b, const char *s, u32 max)
{
	char t[600];
	u32 n = utf8_a_437(s, t, sizeof t), i;

	for (i = 0; i < n && i < max; i++)
		buf_c(b, (u8)(t[i] == '\r' || t[i] == '\n' ? ' ' : t[i]));
}

static u32 dist(u32 a, u32 b)
{
	int r = (int)(a >> 16 & 255) - (int)(b >> 16 & 255), g = (int)(a >> 8 & 255) - (int)(b >> 8 & 255), z = (int)(a & 255) - (int)(b & 255);

	return (u32)(r * r * 3 + g * g * 4 + z * z * 2);
}

static u32 bits386(libro *l, hoja *h, const celda *x, u32 f, u32 c)
{
	const estilo *e = lib_estilo(l, x ? x->estilo : hj_estilo(h, f, c));
	u32 b = 0, i, mejor;
	int cat;

	if (e->negrita)
		b |= F_NEG;
	if (e->cursiva)
		b |= F_CUR;
	if (e->alin_h == AH_IZQ)
		b |= 1 << 2;
	else if (e->alin_h == AH_CENTRO || e->alin_h == AH_CENTRO_SEL)
		b |= 2 << 2;
	else if (e->alin_h == AH_DER)
		b |= 3 << 2;
	if (e->color != CU_AUTO && e->color != 0) {
		mejor = 0;
		for (i = 1; i < 8; i++)
			if (dist(e->color, PALETA386[COLORES386[i]]) < dist(e->color, PALETA386[COLORES386[mejor]]))
				mejor = i;
		b |= mejor << 4;
	}
	cat = fmt_categoria(lib_fmt(l, e->numfmt));
	{
		int fn = FN_GENERAL, d = fmt_decimales(lib_fmt(l, e->numfmt));

		if (cat == CAT_NUMERO)
			fn = busca(lib_fmt(l, e->numfmt), ",") ? FN_MIL : FN_FIJO;
		else if (cat == CAT_MONEDA)
			fn = FN_MONEDA;
		else if (cat == CAT_PORCENTAJE)
			fn = FN_PORC;
		else if (cat == CAT_FECHA)
			fn = FN_FECHA;
		if (d > 8)
			d = 8;
		b |= (u32)fn << 7 | (u32)d << 10;
	}
	if (e->borde[BO_ABAJO])
		b |= F_BABA;
	if (e->borde[BO_DER])
		b |= F_BDER;
	if (e->trama != TR_NADA && e->fondo != 0xFFFFFF) {
		mejor = 1;
		for (i = 2; i < 9; i++)
			if (dist(e->fondo, PALETA386[FONDOS386[i]]) < dist(e->fondo, PALETA386[FONDOS386[mejor]]))
				mejor = i;
		b |= mejor << 16;
	}
	{
		int k = hj_combinada(h, f, c);

		if (k >= 0 && h->comb[k].f0 == f && h->comb[k].c0 == c && h->comb[k].f0 == h->comb[k].f1) {
			u32 n = (u32)h->comb[k].c1 - c;

			b |= (n > 63 ? 63 : n) << 20;
		}
	}
	return b;
}

/* lo de 386 de una hoja */
static void escribe_386(libro *l, hoja *h, bufer *b, int activa)
{
	u32 c, f, n, i, *k;
	cu_local antes = l->loc;

	for (c = 0; c < 64; c++)
		if (hj_ancho(h, c) != AN386 && hj_ancho(h, c) >= 8 && hj_ancho(h, c) <= 480) {
			char r[8];

			cu_col_texto(c, r);
			buf_fmt(b, "A %s1 %u\r\n", r, hj_ancho(h, c));
		}
	if (h->alto)
		for (f = 0; f < 1000; f++)
			if (h->alto[f] && h->alto[f] >= 4 && h->alto[f] <= 120 && h->alto[f] != AL386)
				buf_fmt(b, "H %u %u\r\n", f + 1, h->alto[f]);
	if ((h->inm_c || h->inm_f) && h->inm_c <= 20 && h->inm_f <= 20)
		buf_fmt(b, "P %u %u\r\n", h->inm_c, h->inm_f);
	for (i = 0; i < h->n_gr; i++) {
		const grafico *g = &h->gr[i];
		static const u8 a386[GR_N] = { GT_BARRAS, GT_BARRAS, GT_LINEAS, GT_SECTORES, GT_XY };
		char r[24];

		if (g->hoja != h->id || g->datos.f1 >= 1000 || g->datos.c1 >= 64)
			continue;
		cu_rango_texto(cu_rango(g->datos.f0, g->datos.c0, g->datos.f1 + (g->datos.f0 == g->datos.f1), g->datos.c1), r);
		buf_fmt(b, "G %u %s %u\r\n", (g->apiladas && g->tipo <= GR_BARRAS) ? GT_APILADAS : a386[g->tipo < GR_N ? g->tipo : 0], r, g->en_filas ? 1 : 0);
		if (g->titulo[0]) {
			buf_txt(b, "T ");
			txt437(b, g->titulo, 39);
			buf_txt(b, "\r\n");
		}
		if (g->eje_x[0]) {
			buf_txt(b, "X ");
			txt437(b, g->eje_x, 23);
			buf_txt(b, "\r\n");
		}
		if (g->eje_y[0]) {
			buf_txt(b, "Y ");
			txt437(b, g->eje_y, 23);
			buf_txt(b, "\r\n");
		}
		break;
	}
	if (!activa)
		buf_fmt(b, "V %u %u %u %u\r\n", h->cur_c < 64 ? h->cur_c : 0, h->cur_f < 1000 ? h->cur_f : 0,
			h->vista_c < 64 ? h->vista_c : 0, h->vista_f < 1000 ? h->vista_f : 0);
	l->loc = CU_ES;
	k = hj_claves_ordenadas(h, &n);
	for (i = 0; k && i < n; i++) {
		celda *x = hj_celda_k(h, k[i]);
		char t[1200], r[16];
		u32 bits;

		f = CU_FILA(k[i]);
		c = CU_COL(k[i]);
		if (!x || f >= 1000 || c >= 64)
			continue;
		bits = bits386(l, h, x, f, c);
		t[0] = 0;
		if (x->f || x->v.t != V_NUM)
			lib_texto_edicion(l, h, f, c, t, sizeof t);
		else if (fmt_categoria(lib_fmt(l, lib_estilo(l, x->estilo)->numfmt)) == CAT_FECHA && x->v.n == so_suelo(x->v.n))
			num_formatea(x->v.n, "dd/mm/yyyy", &CU_ES, t, sizeof t, NULL);
		else
			num_edicion(x->v.n, "General", &CU_ES, t, sizeof t);
		if (!t[0] && !bits)
			continue;
		cu_ref_texto(f, c, r);
		buf_fmt(b, "C %s %u ", r, bits);
		txt437(b, t, 127);
		buf_txt(b, "\r\n");
	}
	so_suelta(k);
	l->loc = antes;
}

/* ---- lo de Cuentas 2 ---- */
static void lin(bufer *b, const char *que)
{
	buf_txt(b, "2");
	buf_txt(b, que);
}

static void pon_rango(bufer *b, rango r)
{
	char t[32], u[16];

	cu_ref_texto(r.f0, r.c0, t);
	cu_ref_texto(r.f1, r.c1, u);
	buf_fmt(b, "\t%s:%s", t, u);
}

static void escribe_2(libro *l, hoja *h, bufer *b, int primera)
{
	u32 i, n, *k, f, c;
	u16 *mapa;		/* el estilo del libro -> el de la hoja (+1) */
	u32 nh = 0;

	lin(b, "H\t2.0\r\n");
	if (primera) {
		lin(b, "O\t");
		buf_fmt(b, "%d\t", l->fecha1904);
		esc(b, l->moneda);
		buf_c(b, '\t');
		esc(b, l->autor);
		buf_txt(b, "\r\n");
		for (i = 0; i < l->n_nom; i++) {
			char t[2000];

			fm_texto(l, l->nom[i].f, NULL, &CU_ARCHIVO, t, sizeof t);
			lin(b, "N\t");
			esc(b, l->nom[i].nombre);
			buf_c(b, '\t');
			esc(b, t);
			buf_txt(b, "\r\n");
		}
	}
	mapa = so_pide(l->n_est * sizeof(u16));
	if (!mapa) {
		b->mal = 1;
		return;
	}
	memset(mapa, 0, l->n_est * sizeof(u16));
	/* los estilos que usa */
#define USA(e) do { u32 e_ = (e); if (e_ < l->n_est && !mapa[e_]) { const estilo *s = lib_estilo(l, e_); int j; \
	mapa[e_] = (u16)++nh; buf_fmt(b, "2E\t%u\t", nh); esc(b, s->familia); \
	buf_fmt(b, "\t%u\t%u\t%u\t%u\t%u\t%X\t%u\t%u\t%u\t%u\t", s->tam, s->negrita, s->cursiva, s->subrayado, s->tachado, s->color, s->alin_h, s->alin_v, s->ajustar, s->sangria); \
	esc(b, lib_fmt(l, s->numfmt)); buf_fmt(b, "\t%u\t%u\t%X\t%X", s->trama, s->girado, s->fondo, s->color_trama); \
	for (j = 0; j < 4; j++) { buf_fmt(b, "\t%u\t%X", s->borde[j], s->color_borde[j]); } buf_txt(b, "\r\n"); } } while (0)
	k = hj_claves_ordenadas(h, &n);
	for (i = 0; k && i < n; i++)
		USA(hj_celda_k(h, k[i])->estilo);
	for (c = 0; c < CU_COLS; c++)
		if (h->estilo_col[c])
			USA(h->estilo_col[c]);
	if (h->estilo_fila)
		for (f = 0; f < CU_FILAS; f++)
			if (h->estilo_fila[f])
				USA(h->estilo_fila[f]);
	/* las celdas */
	for (i = 0; k && i < n; i++) {
		celda *x = hj_celda_k(h, k[i]);
		char r[16], t[64];

		if (!x)
			continue;
		cu_ref_texto(CU_FILA(k[i]), CU_COL(k[i]), r);
		buf_fmt(b, "2C\t%s\t%u\t", r, mapa[x->estilo]);
		if (x->f) {
			bufer w = { 0 };

			fm_escribe(l, x->f, h, &CU_ARCHIVO, &w);
			buf_c(&w, 0);
			buf_txt(b, "f\t");
			esc(b, w.d ? (char *)w.d : "");
			buf_suelta(&w);
		} else
			switch (x->v.t) {
			case V_NUM:
				num_a_texto_c(x->v.n, t, sizeof t);
				buf_fmt(b, "n\t%s", t);
				break;
			case V_TXT:
				buf_txt(b, "s\t");
				esc(b, x->v.s);
				break;
			case V_LOG:
				buf_fmt(b, "b\t%u", x->v.n != 0);
				break;
			case V_ERR:
				buf_fmt(b, "e\t%u", x->v.e);
				break;
			default:
				buf_txt(b, "v\t");
			}
		buf_txt(b, "\r\n");
	}
	so_suelta(k);
	/* lo de la hoja */
	buf_fmt(b, "2D\t%u\r\n", h->ancho_def);
	for (c = 0; c < CU_COLS; c++) {
		if (h->ancho[c] != h->ancho_def)
			buf_fmt(b, "2W\t%u\t%u\r\n", c, h->ancho[c]);
		if (h->oculta_c[c])
			buf_fmt(b, "2Y\t%u\r\n", c);
		if (h->estilo_col[c])
			buf_fmt(b, "2Q\t%u\t%u\r\n", c, mapa[h->estilo_col[c]]);
	}
	for (f = 0; f < CU_FILAS; f++) {
		if (h->alto && h->alto[f])
			buf_fmt(b, "2R\t%u\t%u\r\n", f, h->alto[f]);
		if (hj_fila_oculta(h, f))
			buf_fmt(b, "2Z\t%u\r\n", f);
		if (h->estilo_fila && h->estilo_fila[f])
			buf_fmt(b, "2F\t%u\t%u\r\n", f, mapa[h->estilo_fila[f]]);
	}
	so_suelta(mapa);
	for (i = 0; i < h->n_comb; i++) {
		lin(b, "M");
		pon_rango(b, h->comb[i]);
		buf_txt(b, "\r\n");
	}
	for (i = 0; i < h->n_com; i++) {
		char r[16];

		cu_ref_texto(h->com[i].f, h->com[i].c, r);
		buf_fmt(b, "2K\t%s\t", r);
		esc(b, h->com[i].autor);
		buf_c(b, '\t');
		esc(b, h->com[i].texto);
		buf_txt(b, "\r\n");
	}
	for (i = 0; i < h->n_cond; i++) {
		const condicional *d = &h->cond[i];
		char v1[40], v2[40];

		num_a_texto_c(d->v1, v1, sizeof v1);
		num_a_texto_c(d->v2, v2, sizeof v2);
		lin(b, "J");
		pon_rango(b, d->r);
		buf_fmt(b, "\t%u\t%u\t%u\t%u\t%s\t%s\t%X\t%X\t%u\t", d->cond, d->simbolo, d->negrita, d->cursiva, v1, v2, d->color, d->fondo, d->trama);
		esc(b, d->texto);
		buf_txt(b, "\r\n");
	}
	for (i = 0; i < h->n_gr; i++) {
		const grafico *g = &h->gr[i];
		hoja *hd = lib_hoja_id(l, g->hoja);

		buf_fmt(b, "2G\t%u\t%u\t%u\t%u\t%u\t%u\t", g->tipo, g->en_filas, g->rotulos, g->leyenda, g->apiladas, g->rejilla);
		esc(b, hd ? hd->nombre : "");
		pon_rango(b, g->datos);
		buf_fmt(b, "\t%u\t%u\t%d\t%d\t%d\t%d\t", g->f, g->c, g->dx, g->dy, g->an, g->al);
		esc(b, g->titulo);
		buf_c(b, '\t');
		esc(b, g->eje_x);
		buf_c(b, '\t');
		esc(b, g->eje_y);
		buf_txt(b, "\r\n");
	}
	if (h->hay_filtro) {
		lin(b, "A");
		pon_rango(b, h->filtro);
		buf_txt(b, "\r\n");
		for (i = 0; i < h->n_fcol; i++) {
			char v[40];

			num_a_texto_c(h->fcol[i].v, v, sizeof v);
			buf_fmt(b, "2a\t%u\t%u\t%s\t", h->fcol[i].c, h->fcol[i].tipo, v);
			esc(b, h->fcol[i].texto);
			buf_txt(b, "\r\n");
		}
	}
	if (h->hay_area) {
		lin(b, "P");
		pon_rango(b, h->area);
		buf_txt(b, "\r\n");
	}
	for (i = 0; i < h->n_salto_f; i++)
		buf_fmt(b, "2B\tf\t%u\r\n", h->salto_f[i]);
	for (i = 0; i < h->n_salto_c; i++)
		buf_fmt(b, "2B\tc\t%u\r\n", h->salto_c[i]);
	{
		const pagina *p = &h->pag;
		char m[4][40];
		int j;

		for (j = 0; j < 4; j++)
			num_a_texto_c(p->marg[j], m[j], sizeof m[j]);
		buf_fmt(b, "2X\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%s\t%s\t%s\t%s\t", p->horizontal, p->papel, p->rejilla, p->titulos,
			p->centrar_h, p->centrar_v, p->escala, p->encajar_an, p->encajar_al, m[0], m[1], m[2], m[3]);
		esc(b, p->encabezado);
		buf_c(b, '\t');
		esc(b, p->pie);
		buf_txt(b, "\r\n");
	}
	buf_fmt(b, "2I\t%u\t%u\r\n", h->inm_f, h->inm_c);
	buf_fmt(b, "2V\t%u\t%u\t%u\t%u\t%u\t%u\t%u", h->cur_f, h->cur_c, h->vista_f, h->vista_c, h->zoom, h->sin_rejilla, h->oculta);
	pon_rango(b, h->sel);
	buf_txt(b, "\r\n");
#undef USA
}

int hoj_escribe(libro *l, bufer *b, char *err, u32 cabe)
{
	u32 i;

	buf_txt(b, "HOJA 1\r\nM ");
	txt437(b, l->moneda[0] ? l->moneda : "EUR", 7);
	buf_txt(b, "\r\n");
	for (i = 0; i < l->n_h; i++)
		if (l->h[i]->pag.rejilla) {
			buf_txt(b, "I 1\r\n");
			break;
		}
	buf_fmt(b, "L %u\r\n", l->activa + 1);
	for (i = 0; i < l->n_h; i++) {
		hoja *h = l->h[i];

		buf_txt(b, "S ");
		txt437(b, h->nombre, 31);
		buf_txt(b, "\r\n");
		escribe_386(l, h, b, i == l->activa);
		escribe_2(l, h, b, i == 0);
	}
	if (b->mal) {
		copia(err, "No hay memoria para guardar", cabe);
		return 1;
	}
	return 0;
}

/* ===================================================================== */
/*  Leer                                                                 */
/* ===================================================================== */
typedef struct {
	const char *s;		/* la línea (sin el fin) */
	u32 n;
} linea;

/* los campos de una línea 2x (separados por tabuladores; sin los escapes) */
#define MAX_CAMPOS 40
typedef struct {
	char *c[MAX_CAMPOS];
	int n;
	char buf[8192];
} campos;

static void parte(const linea *ln, campos *k)
{
	u32 i = 0, o = 0;

	k->n = 0;
	/* (lo de delante: «2X») */
	while (i < ln->n && ln->s[i] != '\t')
		i++;
	while (i < ln->n && k->n < MAX_CAMPOS && o + 2 < sizeof k->buf) {
		i++;	/* el tabulador */
		k->c[k->n++] = k->buf + o;
		while (i < ln->n && ln->s[i] != '\t' && o + 2 < sizeof k->buf) {
			char c = ln->s[i++];

			if (c == '\\' && i < ln->n) {
				char d = ln->s[i++];

				c = d == 't' ? '\t' : d == 'n' ? '\n' : d == 'r' ? '\r' : d;
			}
			k->buf[o++] = c;
		}
		k->buf[o++] = 0;
	}
}

static u32 hex(const char *s)
{
	u32 v = 0;

	for (; *s; s++) {
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

static u32 num(const char *s)
{
	long v = numero(s, NULL);

	return v < 0 ? 0 : (u32)v;
}

static const char *campo(const campos *k, int i)
{
	return i < k->n ? k->c[i] : "";
}

static int lee_rango_txt(const char *s, rango *r)
{
	const char *fin;

	return cu_lee_rango(s, r, &fin);
}

/* una hoja con lo de Cuentas 2 */
typedef struct {
	u16 *est;		/* el de la hoja (1...) -> el del libro */
	u32 n_est;
} mapa_est;

static u16 est_de(const mapa_est *m, const char *s)
{
	u32 i = num(s);

	return i && i <= m->n_est ? m->est[i - 1] : 0;
}

static void lee_2(libro *l, hoja *h, const linea *ls, u32 nl)
{
	mapa_est m = { NULL, 0 };
	campos *k = so_pide(sizeof(campos));
	u32 i;

	if (!k)
		return;
	for (i = 0; i < nl; i++) {
		const linea *ln = &ls[i];
		char t;
		u32 f, c;
		const char *fin;

		if (ln->n < 2 || ln->s[0] != '2')
			continue;
		t = ln->s[1];
		parte(ln, k);
		switch (t) {
		case 'O':
			l->fecha1904 = (int)num(campo(k, 0));
			copia(l->moneda, campo(k, 1)[0] ? campo(k, 1) : "€", sizeof l->moneda);
			copia(l->autor, campo(k, 2), sizeof l->autor);
			break;
		case 'N': {
			char e[200];
			u32 d;
			formula *fm = fm_lee(l, h, campo(k, 1), largo(campo(k, 1)), &CU_ARCHIVO, e, sizeof e, &d);

			if (fm)
				lib_pon_nombre(l, campo(k, 0), fm);
			break;
		}
		case 'E': {
			estilo e;
			int j;
			u16 *n2;

			if (k->n < 25)
				break;
			estilo_base(&e);
			copia(e.familia, campo(k, 1), sizeof e.familia);
			e.tam = (u16)num(campo(k, 2));
			if (e.tam < 2 || e.tam > 800)
				e.tam = CU_TAM;
			e.negrita = (u8)num(campo(k, 3));
			e.cursiva = (u8)num(campo(k, 4));
			e.subrayado = (u8)num(campo(k, 5));
			e.tachado = (u8)num(campo(k, 6));
			e.color = hex(campo(k, 7));
			e.alin_h = (u8)(num(campo(k, 8)) % 7);
			e.alin_v = (u8)(num(campo(k, 9)) % 3);
			e.ajustar = (u8)num(campo(k, 10));
			e.sangria = (u8)num(campo(k, 11));
			e.numfmt = lib_fmt_id(l, campo(k, 12));
			e.trama = (u8)(num(campo(k, 13)) % TR_N);
			e.girado = (u8)num(campo(k, 14));
			e.fondo = hex(campo(k, 15)) & 0xFFFFFF;
			e.color_trama = hex(campo(k, 16)) & 0xFFFFFF;
			for (j = 0; j < 4; j++) {
				e.borde[j] = (u8)(num(campo(k, 17 + 2 * j)) % BR_N);
				e.color_borde[j] = hex(campo(k, 18 + 2 * j)) & 0xFFFFFF;
			}
			if (e.color != CU_AUTO)
				e.color &= 0xFFFFFF;
			n2 = so_repide(m.est, (m.n_est + 1) * sizeof(u16));
			if (n2) {
				m.est = n2;
				m.est[m.n_est++] = lib_estilo_id(l, &e);
			}
			break;
		}
		case 'C': {
			const char *tp = campo(k, 2), *dato = campo(k, 3);

			if (!cu_lee_ref(campo(k, 0), &f, &c, &fin))
				break;
			if (tp[0] == 'f') {
				char e[200];
				u32 d;
				formula *fm = fm_lee(l, h, dato, largo(dato), &CU_ARCHIVO, e, sizeof e, &d);

				if (fm)
					lib_pon_formula(l, h, f, c, fm);
				else
					lib_pon_texto(l, h, f, c, dato);
			} else if (tp[0] == 'n')
				lib_pon_numero(l, h, f, c, num_de_texto_c(dato));
			else if (tp[0] == 's')
				lib_pon_texto(l, h, f, c, dato);
			else if (tp[0] == 'b' || tp[0] == 'e') {
				valor v = { V_LOG, 0, 0, NULL };

				if (tp[0] == 'b')
					v.n = num(dato) != 0;
				else {
					v.t = V_ERR;
					v.e = (u8)(num(dato) % ER_N);
					if (!v.e)
						v.e = ER_VALOR;
				}
				lib_pon_valor(l, h, f, c, &v);
			}
			lib_pon_estilo(l, h, f, c, est_de(&m, campo(k, 1)));
			break;
		}
		case 'D':
			h->ancho_def = num(campo(k, 0));
			if (h->ancho_def < 4 || h->ancho_def > 2000)
				h->ancho_def = CU_ANCHO;
			for (c = 0; c < CU_COLS; c++)
				h->ancho[c] = (u16)h->ancho_def;
			break;
		case 'W':
			c = num(campo(k, 0));
			if (c < CU_COLS && num(campo(k, 1)))
				hj_pon_ancho(h, c, num(campo(k, 1)));
			break;
		case 'Y':
			c = num(campo(k, 0));
			if (c < CU_COLS)
				h->oculta_c[c] = 1;
			break;
		case 'Q':
			c = num(campo(k, 0));
			if (c < CU_COLS)
				h->estilo_col[c] = est_de(&m, campo(k, 1));
			break;
		case 'R':
			hj_pon_alto(h, num(campo(k, 0)), num(campo(k, 1)));
			break;
		case 'Z':
			hj_oculta_fila(h, num(campo(k, 0)), 1);
			break;
		case 'F':
			f = num(campo(k, 0));
			if (f < CU_FILAS) {
				if (!h->estilo_fila) {
					h->estilo_fila = so_pide(CU_FILAS * sizeof(u16));
					if (h->estilo_fila)
						memset(h->estilo_fila, 0, CU_FILAS * sizeof(u16));
				}
				if (h->estilo_fila)
					h->estilo_fila[f] = est_de(&m, campo(k, 1));
			}
			break;
		case 'M': {
			rango r;

			if (lee_rango_txt(campo(k, 0), &r))
				hj_combina(h, r);
			break;
		}
		case 'K':
			if (cu_lee_ref(campo(k, 0), &f, &c, &fin))
				hj_pon_comentario(h, f, c, campo(k, 2), campo(k, 1));
			break;
		case 'J': {
			condicional d;
			condicional *n2;

			memset(&d, 0, sizeof d);
			if (!lee_rango_txt(campo(k, 0), &d.r))
				break;
			d.cond = (u8)(num(campo(k, 1)) % FC_N);
			d.simbolo = (u8)(num(campo(k, 2)) % SIM_N);
			d.negrita = (u8)num(campo(k, 3));
			d.cursiva = (u8)num(campo(k, 4));
			d.v1 = num_de_texto_c(campo(k, 5));
			d.v2 = num_de_texto_c(campo(k, 6));
			d.color = hex(campo(k, 7));
			d.fondo = hex(campo(k, 8));
			d.trama = (u8)(num(campo(k, 9)) % TR_N);
			copia(d.texto, campo(k, 10), sizeof d.texto);
			n2 = so_repide(h->cond, (h->n_cond + 1) * sizeof(condicional));
			if (n2) {
				h->cond = n2;
				h->cond[h->n_cond++] = d;
			}
			break;
		}
		case 'G': {
			grafico g;
			grafico *n2;
			hoja *hd;

			if (k->n < 16)
				break;
			memset(&g, 0, sizeof g);
			g.tipo = (u8)(num(campo(k, 0)) % GR_N);
			g.en_filas = (u8)num(campo(k, 1));
			g.rotulos = (u8)num(campo(k, 2));
			g.leyenda = (u8)(num(campo(k, 3)) % 3);
			g.apiladas = (u8)num(campo(k, 4));
			g.rejilla = (u8)num(campo(k, 5));
			hd = lib_hoja_nombre(l, campo(k, 6), largo(campo(k, 6)));
			g.hoja = hd ? hd->id : h->id;
			lee_rango_txt(campo(k, 7), &g.datos);
			g.f = num(campo(k, 8)) < CU_FILAS ? num(campo(k, 8)) : 0;
			g.c = (u16)(num(campo(k, 9)) < CU_COLS ? num(campo(k, 9)) : 0);
			g.dx = (i32)numero(campo(k, 10), NULL);
			g.dy = (i32)numero(campo(k, 11), NULL);
			g.an = (i32)numero(campo(k, 12), NULL);
			g.al = (i32)numero(campo(k, 13), NULL);
			if (g.an < 20 || g.an > 4000)
				g.an = 480;
			if (g.al < 20 || g.al > 4000)
				g.al = 300;
			copia(g.titulo, campo(k, 14), sizeof g.titulo);
			copia(g.eje_x, campo(k, 15), sizeof g.eje_x);
			copia(g.eje_y, campo(k, 16), sizeof g.eje_y);
			n2 = so_repide(h->gr, (h->n_gr + 1) * sizeof(grafico));
			if (n2) {
				h->gr = n2;
				h->gr[h->n_gr++] = g;
			}
			break;
		}
		case 'A':
			if (lee_rango_txt(campo(k, 0), &h->filtro))
				h->hay_filtro = 1;
			break;
		case 'a': {
			filtro_col fc;
			filtro_col *n2;

			memset(&fc, 0, sizeof fc);
			fc.c = (u16)(num(campo(k, 0)) % CU_COLS);
			fc.tipo = (u8)(num(campo(k, 1)) % 9);
			fc.v = num_de_texto_c(campo(k, 2));
			copia(fc.texto, campo(k, 3), sizeof fc.texto);
			n2 = so_repide(h->fcol, (h->n_fcol + 1) * sizeof(filtro_col));
			if (n2) {
				h->fcol = n2;
				h->fcol[h->n_fcol++] = fc;
			}
			break;
		}
		case 'P':
			if (lee_rango_txt(campo(k, 0), &h->area))
				h->hay_area = 1;
			break;
		case 'B': {
			u32 **s = campo(k, 0)[0] == 'c' ? &h->salto_c : &h->salto_f, *ns = campo(k, 0)[0] == 'c' ? &h->n_salto_c : &h->n_salto_f;
			u32 *n2 = so_repide(*s, (*ns + 1) * sizeof(u32));

			if (n2) {
				*s = n2;
				n2[(*ns)++] = num(campo(k, 1));
			}
			break;
		}
		case 'X': {
			pagina *p = &h->pag;
			int j;

			p->horizontal = (u8)num(campo(k, 0));
			p->papel = (u8)(num(campo(k, 1)) % 5);
			p->rejilla = (u8)num(campo(k, 2));
			p->titulos = (u8)num(campo(k, 3));
			p->centrar_h = (u8)num(campo(k, 4));
			p->centrar_v = (u8)num(campo(k, 5));
			p->escala = (u16)num(campo(k, 6));
			if (p->escala < 10 || p->escala > 400)
				p->escala = 100;
			p->encajar_an = (u16)num(campo(k, 7));
			p->encajar_al = (u16)num(campo(k, 8));
			for (j = 0; j < 4; j++) {
				p->marg[j] = num_de_texto_c(campo(k, 9 + j));
				if (!(p->marg[j] >= 0 && p->marg[j] < 20))
					p->marg[j] = 1.8;
			}
			copia(p->encabezado, campo(k, 13), sizeof p->encabezado);
			copia(p->pie, campo(k, 14), sizeof p->pie);
			break;
		}
		case 'I':
			h->inm_f = num(campo(k, 0)) < CU_FILAS ? num(campo(k, 0)) : 0;
			h->inm_c = num(campo(k, 1)) < CU_COLS ? num(campo(k, 1)) : 0;
			break;
		case 'V':
			h->cur_f = num(campo(k, 0)) < CU_FILAS ? num(campo(k, 0)) : 0;
			h->cur_c = num(campo(k, 1)) < CU_COLS ? num(campo(k, 1)) : 0;
			h->vista_f = num(campo(k, 2)) < CU_FILAS ? num(campo(k, 2)) : 0;
			h->vista_c = num(campo(k, 3)) < CU_COLS ? num(campo(k, 3)) : 0;
			h->zoom = (u16)num(campo(k, 4));
			if (h->zoom < 10 || h->zoom > 400)
				h->zoom = 100;
			h->sin_rejilla = (u8)num(campo(k, 5));
			h->oculta = (u8)num(campo(k, 6));
			if (!lee_rango_txt(campo(k, 7), &h->sel))
				h->sel = cu_rango(h->cur_f, h->cur_c, h->cur_f, h->cur_c);
			break;
		}
	}
	so_suelta(m.est);
	so_suelta(k);
}

/* una hoja de 386 */
static void lee_386(libro *l, hoja *h, const linea *ls, u32 nl, const char *moneda, int *rejilla)
{
	u32 i;
	cu_local antes = l->loc;
	grafico g;
	int hay_g = 0;

	memset(&g, 0, sizeof g);
	l->loc = CU_ES;
	for (i = 0; i < nl; i++) {
		const linea *ln = &ls[i];
		char t[600];
		const char *p, *fin;
		u32 f, c;

		if (ln->n < 2 || ln->s[1] != ' ')
			continue;
		/* (la línea en UTF-8: de la 437) */
		de437_a_utf8((const u8 *)ln->s + 2, ln->n - 2, t, sizeof t);
		p = t;
		switch (ln->s[0]) {
		case 'I':
			*rejilla = numero(p, NULL) & 1;
			break;
		case 'A':
			if (cu_lee_ref(p, &f, &c, &fin) && c < 64) {
				long v = numero(fin, NULL);

				if (v >= 8 && v <= 480)
					hj_pon_ancho(h, c, (u32)v);
			}
			break;
		case 'H': {
			const char *q;
			long fi = numero(p, &q), v = numero(q, NULL);

			if (fi >= 1 && fi <= 1000 && v >= 4 && v <= 120 && v != AL386)
				hj_pon_alto(h, (u32)fi - 1, (u32)v);
			break;
		}
		case 'P': {
			const char *q;
			long co = numero(p, &q), fi = numero(q, NULL);

			if (co >= 0 && co <= 20 && fi >= 0 && fi <= 20) {
				h->inm_c = (u32)co;
				h->inm_f = (u32)fi;
			}
			break;
		}
		case 'V': {
			const char *q = p;
			long v[4];
			int j;

			for (j = 0; j < 4; j++)
				v[j] = numero(q, &q);
			if (v[0] >= 0 && v[0] < 64 && v[1] >= 0 && v[1] < 1000) {
				h->cur_c = (u32)v[0];
				h->cur_f = (u32)v[1];
				h->sel = cu_rango(h->cur_f, h->cur_c, h->cur_f, h->cur_c);
			}
			if (v[2] >= 0 && v[2] < 64 && v[3] >= 0 && v[3] < 1000) {
				h->vista_c = (u32)v[2];
				h->vista_f = (u32)v[3];
			}
			break;
		}
		case 'G': {
			const char *q;
			long tp = numero(p, &q);
			rango r;

			while (*q == ' ')
				q++;
			if (tp < 0 || tp > GT_XY || !cu_lee_rango(q, &r, &fin))
				break;
			gr_nuevo(&g, h, r, tp == GT_LINEAS ? GR_LINEAS : tp == GT_SECTORES ? GR_SECTORES : tp == GT_XY ? GR_DISPERSION : GR_COLUMNAS);
			g.apiladas = tp == GT_APILADAS;
			g.en_filas = (u8)(numero(fin, NULL) & 1);
			hay_g = 1;
			break;
		}
		case 'T':
			copia(g.titulo, p, sizeof g.titulo);
			break;
		case 'X':
			copia(g.eje_x, p, sizeof g.eje_x);
			break;
		case 'Y':
			copia(g.eje_y, p, sizeof g.eje_y);
			break;
		case 'C': {
			const char *q;
			long bits;
			char err[200];

			if (!cu_lee_ref(p, &f, &c, &fin) || c >= 64 || f >= 1000)
				break;
			bits = numero(fin, &q);
			if (*q == ' ')
				q++;
			if (*q && lib_pon_entrada(l, h, f, c, q, err, sizeof err))
				lib_pon_texto(l, h, f, c, q);
			{
				/* los bits, al estilo */
				const celda *x = hj_celda(h, f, c);
				estilo e = *lib_estilo(l, x ? x->estilo : 0);
				u32 b = (u32)bits, k2;
				int fn = (int)(b & F_NUM) >> 7, d = (int)(b & F_DEC) >> 10;

				e.negrita = (b & F_NEG) != 0;
				e.cursiva = (b & F_CUR) != 0;
				k2 = (b & F_ALIN) >> 2;
				e.alin_h = k2 == 1 ? AH_IZQ : k2 == 2 ? AH_CENTRO : k2 == 3 ? AH_DER : AH_GENERAL;
				k2 = (b & F_COLOR) >> 4;
				e.color = k2 ? PALETA386[COLORES386[k2]] : CU_AUTO;
				k2 = (b & F_FONDO) >> 16;
				if (k2 >= 1 && k2 <= 8) {
					e.trama = TR_SOLIDO;
					e.fondo = PALETA386[FONDOS386[k2]];
				}
				if (b & F_BABA) {
					e.borde[BO_ABAJO] = BR_FINO;
					e.color_borde[BO_ABAJO] = PALETA386[0];
				}
				if (b & F_BDER) {
					e.borde[BO_DER] = BR_FINO;
					e.color_borde[BO_DER] = PALETA386[0];
				}
				if (d > 8)
					d = 8;
				if (fn != FN_GENERAL) {
					char cod[64], dec[16];
					int j;

					dec[0] = 0;
					if (d) {
						dec[0] = '.';
						for (j = 0; j < d; j++)
							dec[1 + j] = '0';
						dec[1 + d] = 0;
					}
					if (fn == FN_FIJO)
						fmt(cod, sizeof cod, "0%s", dec);
					else if (fn == FN_MIL)
						fmt(cod, sizeof cod, "#,##0%s", dec);
					else if (fn == FN_MONEDA)
						fmt(cod, sizeof cod, "#,##0%s \"%s\"", dec, moneda);
					else if (fn == FN_PORC)
						fmt(cod, sizeof cod, "0%s%%", dec);
					else
						copia(cod, "dd/mm/yyyy", sizeof cod);
					e.numfmt = lib_fmt_id(l, cod);
				}
				lib_pon_estilo(l, h, f, c, lib_estilo_id(l, &e));
				k2 = (b & F_COMB) >> 20;
				if (k2 && c + k2 < 64)
					hj_combina(h, cu_rango(f, c, f, c + k2));
			}
			break;
		}
		}
	}
	if (hay_g) {
		grafico *n2 = so_repide(h->gr, (h->n_gr + 1) * sizeof(grafico));

		if (n2) {
			h->gr = n2;
			h->gr[h->n_gr++] = g;
		}
	}
	l->loc = antes;
}

libro *hoj_lee(const u8 *d, u32 n, char *err, u32 cabe)
{
	linea *ls;
	u32 nl = 0, i = 0, j;
	libro *l;
	u32 *ini = NULL, nh = 0, activa = 0;	/* dónde empieza cada hoja (en líneas) */
	char moneda[32];
	int rejilla = 0, hay_2 = 0;

	if (n < 4 || memcmp(d, "HOJA", 4) != 0) {
		copia(err, "No es un libro de Cuentas (.HOJ)", cabe);
		return NULL;
	}
	/* las líneas */
	for (j = 0; j < n; j++)
		if (d[j] == '\n')
			nl++;
	ls = so_pide((nl + 2) * sizeof(linea));
	ini = so_pide((nl + 2) * sizeof(u32));
	if (!ls || !ini) {
		so_suelta(ls);
		so_suelta(ini);
		copia(err, "No hay memoria", cabe);
		return NULL;
	}
	nl = 0;
	while (i < n) {
		u32 a = i;

		while (i < n && d[i] != '\n' && d[i])
			i++;
		ls[nl].s = (const char *)d + a;
		ls[nl].n = i - a;
		while (ls[nl].n && (ls[nl].s[ls[nl].n - 1] == '\r'))
			ls[nl].n--;
		nl++;
		if (i < n && !d[i])
			break;
		i++;
	}
	copia(moneda, "EUR", sizeof moneda);
	for (j = 0; j < nl; j++) {
		const linea *ln = &ls[j];

		if (ln->n >= 2 && ln->s[0] == 'S' && ln->s[1] == ' ')
			ini[nh++] = j;
		else if (ln->n >= 2 && ln->s[0] == 'M' && ln->s[1] == ' ' && !nh) {
			char t[64];

			de437_a_utf8((const u8 *)ln->s + 2, ln->n - 2 < 16 ? ln->n - 2 : 16, t, sizeof t);
			if (t[0])
				copia(moneda, t, sizeof moneda);
		} else if (ln->n >= 2 && ln->s[0] == 'L' && ln->s[1] == ' ' && !nh) {
			long v = numero(ln->s + 2, NULL);

			activa = v > 0 ? (u32)v - 1 : 0;
		} else if (ln->n >= 2 && ln->s[0] == '2' && ln->s[1] == 'H')
			hay_2 = 1;
	}
	l = lib_nuevo(0);
	if (!l) {
		so_suelta(ls);
		so_suelta(ini);
		copia(err, "No hay memoria", cabe);
		return NULL;
	}
	copia(l->moneda, moneda, sizeof l->moneda);
	if (iguales(moneda, "EUR") && hay_2)
		copia(l->moneda, "€", sizeof l->moneda);
	/* primero todas las hojas (las fórmulas piden otras por su nombre) */
	if (!nh) {
		lib_hoja_nueva(l, "Hoja1", -1);
	} else
		for (j = 0; j < nh && j < CU_MAX_HOJAS; j++) {
			char t[200];
			u32 k;
			const linea *ln = &ls[ini[j]];

			de437_a_utf8((const u8 *)ln->s + 2, ln->n - 2, t, sizeof t);
			/* (la de 386 corta el nombre en el primer blanco) */
			for (k = 0; t[k]; k++)
				if ((u8)t[k] < 32)
					t[k] = 0;
			if (!t[0] || lib_hoja_nombre(l, t, largo(t)))
				lib_nombre_libre(l, t, sizeof t);
			if (!lib_hoja_nueva(l, t, -1))
				break;
		}
	if (!l->n_h) {
		lib_suelta(l);
		so_suelta(ls);
		so_suelta(ini);
		copia(err, "No hay memoria", cabe);
		return NULL;
	}
	for (j = 0; j < l->n_h; j++) {
		u32 a = nh ? ini[j] + 1 : 0, b = (nh && j + 1 < nh) ? ini[j + 1] : nl, k;
		int con2 = 0;
		hoja *h = l->h[j];

		l->activa = j;
		for (k = a; k < b; k++)
			if (ls[k].n >= 2 && ls[k].s[0] == '2' && ls[k].s[1] == 'H')
				con2 = 1;
		/* (la primera hoja también lleva lo de delante: M, I, L; y en un libro de una hoja sin S, todo) */
		if (con2)
			lee_2(l, h, ls + a, b - a);
		else
			lee_386(l, h, ls + (j == 0 && nh ? 0 : a), b - (j == 0 && nh ? 0 : a), moneda, &rejilla);
	}
	if (rejilla)
		for (j = 0; j < l->n_h; j++)
			l->h[j]->pag.rejilla = 1;
	l->activa = activa < l->n_h ? activa : 0;
	so_suelta(ls);
	so_suelta(ini);
	return l;
}
