/* f_xmlcomun.c - Pluma 2: lo común de ODT y DOCX (pluma/f_xmlcomun.h).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "f_xmlcomun.h"

void fx_junta(fx_props *d, const fx_props *s)
{
	fx_aplica(s, &d->l, &d->p);
	d->ml |= s->ml;
	d->mp |= s->mp;
}

void fx_aplica(const fx_props *s, fmt_letra *l, fmt_parrafo *p)
{
	u32 m = s->ml;

	if (l) {
		if (m & FL_FAM)
			copia(l->familia, s->l.familia, sizeof l->familia);
		if (m & FL_TAM)
			l->tam = s->l.tam;
		if (m & FL_NEG)
			l->negrita = s->l.negrita;
		if (m & FL_CUR)
			l->cursiva = s->l.cursiva;
		if (m & FL_SUB)
			l->subrayado = s->l.subrayado;
		if (m & FL_TACH)
			l->tachado = s->l.tachado;
		if (m & FL_COLOR)
			l->color = s->l.color;
		if (m & FL_RES)
			l->resalte = s->l.resalte;
		if (m & FL_VERT)
			l->vert = s->l.vert;
		if (m & FL_VERS)
			l->versalitas = s->l.versalitas;
	}
	m = s->mp;
	if (p) {
		if (m & FP_ALIN)
			p->alin = s->p.alin;
		if (m & FP_SI)
			p->sang_izq = s->p.sang_izq;
		if (m & FP_SD)
			p->sang_der = s->p.sang_der;
		if (m & FP_SP)
			p->sang_primera = s->p.sang_primera;
		if (m & FP_ANT)
			p->antes = s->p.antes;
		if (m & FP_DES)
			p->despues = s->p.despues;
		if (m & FP_INTER) {
			p->inter_tipo = s->p.inter_tipo;
			p->inter = s->p.inter;
		}
		if (m & FP_TABS) {
			p->n_tabs = s->p.n_tabs;
			memcpy(p->tabs, s->p.tabs, sizeof p->tabs);
		}
		if (m & FP_SALTO)
			p->salto_antes = s->p.salto_antes;
		if (m & FP_JUNTOS)
			p->juntos = s->p.juntos;
		if (m & FP_FONDO)
			p->fondo = s->p.fondo;
		if (m & FP_NIVTIT)
			p->nivel_titulo = s->p.nivel_titulo;
	}
}

static const struct {
	int est;
	const char *n;
} nombres[] = {
	{ EST_NORMAL, "Normal" }, { EST_NORMAL, "Standard" }, { EST_NORMAL, "Default Paragraph Style" }, { EST_NORMAL, "Estilo de párrafo predeterminado" },
	{ EST_TITULO, "Título" }, { EST_TITULO, "Title" },
	{ EST_T1, "Título 1" }, { EST_T1, "Heading 1" }, { EST_T1, "heading 1" }, { EST_T1, "Encabezado 1" },
	{ EST_T2, "Título 2" }, { EST_T2, "Heading 2" }, { EST_T2, "Encabezado 2" },
	{ EST_T3, "Título 3" }, { EST_T3, "Heading 3" }, { EST_T3, "Encabezado 3" },
	{ EST_SUBTITULO, "Subtítulo" }, { EST_SUBTITULO, "Subtitle" },
	{ EST_CITA, "Cita" }, { EST_CITA, "Quote" }, { EST_CITA, "Quotations" }, { EST_CITA, "Citas" },
	{ EST_LISTA, "Párrafo de lista" }, { EST_LISTA, "List Paragraph" },
	{ EST_ENC, "Encabezado" }, { EST_ENC, "Header" }, { EST_ENC, "header" }, { EST_ENC, "Cabecera" },
	{ EST_PIE, "Pie de página" }, { EST_PIE, "Footer" }, { EST_PIE, "footer" },
	{ EST_NOTA, "Nota al pie" }, { EST_NOTA, "Footnote" }, { EST_NOTA, "Footnote Text" }, { EST_NOTA, "footnote text" }, { EST_NOTA, "Texto nota pie" },
	{ EST_SIN_ESPACIO, "Sin espaciado" }, { EST_SIN_ESPACIO, "No Spacing" },
};

int fx_estilo_serie(const char *nombre)
{
	u32 i;

	for (i = 0; i < sizeof nombres / sizeof nombres[0]; i++)
		if (iguales_sin_caja(nombre, nombres[i].n))
			return nombres[i].est;
	return -1;
}

const char *fx_nombre_odt(int est)
{
	static const char *const t[EST_N] = { "Standard", "Title", "Heading 1", "Heading 2", "Heading 3", "Subtitle", "Quotations",
		"List Paragraph", "Header", "Footer", "Footnote", "No Spacing" };

	return est >= 0 && est < EST_N ? t[est] : "Standard";
}

const char *fx_nombre_word(int est)
{
	static const char *const t[EST_N] = { "Normal", "Title", "heading 1", "heading 2", "heading 3", "Subtitle", "Quote",
		"List Paragraph", "header", "footer", "footnote text", "No Spacing" };

	return est >= 0 && est < EST_N ? t[est] : "Normal";
}

i32 fx_medida(const char *s)
{
	const char *f;
	double v;

	if (!s)
		return 0;
	v = numero_real(s, &f);
	while (*f == ' ')
		f++;
	if (v > 1e6 || v < -1e6)
		return 0;
	if (empieza(f, "cm"))
		return (i32)(v * 566.929 + (v < 0 ? -0.5 : 0.5));
	if (empieza(f, "mm"))
		return (i32)(v * 56.6929 + (v < 0 ? -0.5 : 0.5));
	if (empieza(f, "in"))
		return (i32)(v * 1440 + (v < 0 ? -0.5 : 0.5));
	if (empieza(f, "pt"))
		return (i32)(v * 20 + (v < 0 ? -0.5 : 0.5));
	if (empieza(f, "pc"))
		return (i32)(v * 240 + (v < 0 ? -0.5 : 0.5));
	if (empieza(f, "px"))
		return (i32)(v * 15 + (v < 0 ? -0.5 : 0.5));
	return (i32)v;
}

u32 fx_color(const char *s)
{
	u32 v = 0, k;

	if (!s)
		return SIN_COLOR;
	if (*s == '#')
		s++;
	for (k = 0; k < 6; k++) {
		char c = s[k];
		u32 d = c >= '0' && c <= '9' ? (u32)(c - '0') : c >= 'a' && c <= 'f' ? (u32)(c - 'a' + 10) : c >= 'A' && c <= 'F' ? (u32)(c - 'A' + 10) : 99;

		if (d > 15)
			return SIN_COLOR;
		v = v << 4 | d;
	}
	return s[6] ? SIN_COLOR : v;
}

void fx_cm(char *t, u32 cabe, i32 tw)
{
	i32 m = (i32)(((i64)tw * 100000 / 56693 + (tw < 0 ? -0 : 0)));	/* milésimas de cm */

	fmt(t, cabe, "%s%d.%03dcm", m < 0 ? "-" : "", (m < 0 ? -m : m) / 1000, (m < 0 ? -m : m) % 1000);
}

void fx_repasa_tablas(documento *d)
{
	u32 i = 0;

	while (i < d->n_par) {
		u32 t = d->par[i].p.tabla, n, k, cols, bien = 1;

		if (!t || t > d->n_tab) {
			d->par[i].p.tabla = 0;
			i++;
			continue;
		}
		for (n = 0; i + n < d->n_par && d->par[i + n].p.tabla == t; n++)
			;
		cols = d->tab[t - 1].cols;
		if (!cols || n % cols)
			bien = 0;
		for (k = 0; k < n && bien; k++)
			if (d->par[i + k].p.fila != k / cols || d->par[i + k].p.col != k % cols)
				bien = 0;
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

void fx_pon_letra(bufer *b, u32 c)
{
	switch (c) {
	case '&': buf_txt(b, "&amp;"); return;
	case '<': buf_txt(b, "&lt;"); return;
	case '>': buf_txt(b, "&gt;"); return;
	case '"': buf_txt(b, "&quot;"); return;
	}
	if ((c < 0x20 && c != '\t' && c != '\n') || c == 0xFFFE || c == 0xFFFF || (c >= 0xD800 && c < 0xE000) || c > 0x10FFFF)
		return;
	buf_u32(b, c);
}

void fx_nombre_odf(const char *s, char *d, u32 cabe)
{
	u32 k = 0;

	while (*s && k + 1 < cabe) {
		if (s[0] == '_' && s[1] && s[2] && s[3] == '_') {
			u32 v = 0, j, bien = 1;

			for (j = 1; j <= 2; j++) {
				char c = s[j];

				if (c >= '0' && c <= '9')
					v = v * 16 + (u32)(c - '0');
				else if (c >= 'a' && c <= 'f')
					v = v * 16 + (u32)(c - 'a' + 10);
				else if (c >= 'A' && c <= 'F')
					v = v * 16 + (u32)(c - 'A' + 10);
				else
					bien = 0;
			}
			if (bien && v >= 0x20) {
				d[k++] = (char)v;
				s += 4;
				continue;
			}
		}
		d[k++] = *s++;
	}
	d[k] = 0;
}

void fx_letras(documento *d, parrafo *p, const u32 *t, u32 n, u16 f)
{
	u32 i;

	if (!n)
		return;
	if (p->n + n > 4u * 1024 * 1024 || !par_cabe(p, p->n + n)) {
		d->sin_memoria = 1;
		return;
	}
	for (i = 0; i < n; i++) {
		p->t[p->n + i] = t[i];
		p->f[p->n + i] = f;
	}
	p->n += n;
	p->ancho_maq = -1;
}

int fx_cierto(const char *v, int sin_valor)
{
	if (!v)
		return sin_valor;
	return iguales(v, "true") || iguales(v, "1") || iguales(v, "on");
}
