/* f_odt.c - Pluma 2: el OpenDocument de texto (.ODT, el de LibreOffice).
 * Un ZIP (oficina/comun/zip.c) con «mimetype» (el primero, sin
 * comprimir), content.xml (el texto y los estilos automáticos), styles.xml
 * (los estilos con nombre, la página, el encabezado y el pie), meta.xml
 * (título y autor), Pictures/ (las imágenes, tal cual) y
 * META-INF/manifest.xml. Se lee lo esencial (texto, estilos, formato,
 * listas, tablas con celdas unidas, imágenes, saltos, página, encabezado y
 * pie con sus campos, notas al pie) y lo que no se entiende se salta.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "archivo.h"
#include "pluma.h"		/* (0.8.8) PLUMA_VERSION */
#include "f_xmlcomun.h"
#include "zip.h"
#include "xml.h"
#include "letras.h"

/* ---- los estilos del ODT ---- */
typedef struct {
	char nombre[64];
	char padre[64];
	char mostrar[64];
	char lista[64];			/* (párrafo) style:list-style-name */
	char familia[16];		/* paragraph, text, table-cell, graphic, table-column, table... */
	fx_props pr;
	int automatico;
	int pagina;			/* (párrafo) lleva style:master-page-name: salto */
	int ajuste;			/* (graphic) AJUSTE_* */
	int borde;			/* (table-cell) lleva borde */
	u32 fondo;			/* (table-cell) */
	i32 ancho;			/* (table-column) */
	int alin_tabla;			/* (table) */
	int est_doc;			/* el estilo de Pluma (-1: aún no) */
	int resolviendo;
} ostilo;

typedef struct {
	char nombre[64];
	u8 tipo[10];			/* LISTA_* por nivel */
	u32 inicio[10];
	i32 sang[10], primera[10];
} olista;

typedef struct {
	char nombre[64];
	char familia[48];
} ocara;

enum { D_CUERPO, D_NOTA, D_ENC, D_PIE };

typedef struct {
	documento *d;
	zip_lector *z;
	ostilo *est;
	u32 n_est, cabe_est;
	olista *lis;
	u32 n_lis, cabe_lis;
	ocara *caras;
	u32 n_caras, cabe_caras;
	fx_props def;			/* style:default-style (párrafo) */
	int destino, nota;
	int espacio;			/* lo último fue un espacio (los blancos se juntan) */
	u16 sig_lista;
	char ultima_lista[64];
	u16 ultima_lista_id;
	int primer_par;
	u32 imagenes;
	int hondo;
} lector_odt;

static void *crece(void *p, u32 *cabe, u32 hace, u32 tam)
{
	u32 c;
	void *n;

	if (hace <= *cabe)
		return p;
	c = *cabe ? *cabe * 2 : 32;
	while (c < hace)
		c *= 2;
	n = so_repide(p, (size_t)c * tam);
	if (!n)
		return NULL;
	*cabe = c;
	return n;
}

static parrafo *destino(lector_odt *c)
{
	documento *d = c->d;

	switch (c->destino) {
	case D_NOTA: return c->nota >= 0 && (u32)c->nota < d->n_notas ? &d->notas[c->nota] : NULL;
	case D_ENC: return &d->enc;
	case D_PIE: return &d->pie;
	}
	return d->n_par ? &d->par[d->n_par - 1] : NULL;
}

static void letras(lector_odt *c, const u32 *t, u32 n, u16 f)
{
	parrafo *p = destino(c);

	if (p)
		fx_letras(c->d, p, t, n, f);
}

static ostilo *busca_estilo(lector_odt *c, const char *nombre, const char *familia)
{
	u32 i;

	if (!nombre || !nombre[0])
		return NULL;
	/* (los automáticos de content.xml van después: se buscan de atrás adelante) */
	for (i = c->n_est; i-- > 0;)
		if (iguales(c->est[i].nombre, nombre) && (!familia || iguales(c->est[i].familia, familia)))
			return &c->est[i];
	return NULL;
}

static olista *busca_lista(lector_odt *c, const char *nombre)
{
	u32 i;

	if (!nombre)
		return NULL;
	for (i = c->n_lis; i-- > 0;)
		if (iguales(c->lis[i].nombre, nombre))
			return &c->lis[i];
	return NULL;
}

static const char *familia_de_cara(lector_odt *c, const char *n)
{
	u32 i;

	for (i = 0; i < c->n_caras; i++)
		if (iguales(c->caras[i].nombre, n))
			return c->caras[i].familia;
	return n;
}

/* quita las comillas de una familia («'DejaVu Serif'») */
static void familia_limpia(const char *s, char *d, u32 cabe)
{
	u32 k = 0;

	while (*s == ' ' || *s == '\'' || *s == '"')
		s++;
	while (*s && *s != '\'' && *s != '"' && *s != ',' && k + 1 < cabe)
		d[k++] = *s++;
	while (k && d[k - 1] == ' ')
		k--;
	d[k] = 0;
}

/* las propiedades de texto */
static void props_texto(lector_odt *c, const xml_nodo *n, fx_props *p)
{
	const char *v;

	if (!n)
		return;
	if ((v = xml_atr(n, "style:font-name"))) {
		familia_limpia(familia_de_cara(c, v), p->l.familia, sizeof p->l.familia);
		if (p->l.familia[0])
			p->ml |= FL_FAM;
	} else if ((v = xml_atr(n, "fo:font-family"))) {
		familia_limpia(v, p->l.familia, sizeof p->l.familia);
		if (p->l.familia[0])
			p->ml |= FL_FAM;
	}
	if ((v = xml_atr(n, "fo:font-size")) && !strchr(v, '%')) {
		i32 tw = fx_medida(v);

		if (tw >= 20 && tw <= 32000) {
			p->l.tam = (u16)((tw + 5) / 10);
			p->ml |= FL_TAM;
		}
	}
	if ((v = xml_atr(n, "fo:font-weight"))) {
		p->l.negrita = iguales(v, "bold") || numero(v, NULL) >= 600;
		p->ml |= FL_NEG;
	}
	if ((v = xml_atr(n, "fo:font-style"))) {
		p->l.cursiva = iguales(v, "italic") || iguales(v, "oblique");
		p->ml |= FL_CUR;
	}
	if ((v = xml_atr(n, "style:text-underline-style"))) {
		p->l.subrayado = !iguales(v, "none");
		p->ml |= FL_SUB;
	}
	if ((v = xml_atr(n, "style:text-line-through-style"))) {
		p->l.tachado = !iguales(v, "none");
		p->ml |= FL_TACH;
	}
	if ((v = xml_atr(n, "fo:color"))) {
		p->l.color = fx_color(v);
		p->ml |= FL_COLOR;
	}
	if ((v = xml_atr(n, "style:use-window-font-color")) && iguales(v, "true")) {
		p->l.color = SIN_COLOR;
		p->ml |= FL_COLOR;
	}
	if ((v = xml_atr(n, "fo:background-color"))) {
		p->l.resalte = fx_color(v);
		p->ml |= FL_RES;
	}
	if ((v = xml_atr(n, "style:text-position"))) {
		p->l.vert = empieza(v, "super") ? 1 : empieza(v, "sub") ? 2 : (numero(v, NULL) > 0 ? 1 : numero(v, NULL) < 0 ? 2 : 0);
		p->ml |= FL_VERT;
	}
	if ((v = xml_atr(n, "fo:font-variant"))) {
		p->l.versalitas = iguales(v, "small-caps");
		p->ml |= FL_VERS;
	}
}

static void props_parrafo(const xml_nodo *n, fx_props *p, int *borde)
{
	const char *v;
	xml_nodo *ts;

	if (!n)
		return;
	if ((v = xml_atr(n, "fo:text-align"))) {
		p->p.alin = iguales(v, "center") ? AL_CENTRO : (iguales(v, "end") || iguales(v, "right")) ? AL_DER : iguales(v, "justify") ? AL_JUST : AL_IZQ;
		p->mp |= FP_ALIN;
	}
	if ((v = xml_atr(n, "fo:margin-left"))) {
		p->p.sang_izq = fx_medida(v);
		p->mp |= FP_SI;
	}
	if ((v = xml_atr(n, "fo:margin-right"))) {
		p->p.sang_der = fx_medida(v);
		p->mp |= FP_SD;
	}
	if ((v = xml_atr(n, "fo:text-indent"))) {
		p->p.sang_primera = fx_medida(v);
		p->mp |= FP_SP;
	}
	if ((v = xml_atr(n, "fo:margin-top"))) {
		p->p.antes = fx_medida(v);
		if (p->p.antes < 0)
			p->p.antes = 0;
		p->mp |= FP_ANT;
	}
	if ((v = xml_atr(n, "fo:margin-bottom"))) {
		p->p.despues = fx_medida(v);
		if (p->p.despues < 0)
			p->p.despues = 0;
		p->mp |= FP_DES;
	}
	if ((v = xml_atr(n, "fo:line-height"))) {
		if (strchr(v, '%')) {
			p->p.inter_tipo = INTER_MULTIPLE;
			p->p.inter = (i32)numero(v, NULL);
		} else if (!iguales(v, "normal")) {
			p->p.inter_tipo = INTER_EXACTO;
			p->p.inter = fx_medida(v);
		} else {
			p->p.inter_tipo = INTER_MULTIPLE;
			p->p.inter = 100;
		}
		if (p->p.inter < 0 || p->p.inter > 31680)
			p->p.inter = 100;
		p->mp |= FP_INTER;
	} else if ((v = xml_atr(n, "style:line-height-at-least"))) {
		p->p.inter_tipo = INTER_MINIMO;
		p->p.inter = fx_medida(v);
		p->mp |= FP_INTER;
	}
	if ((v = xml_atr(n, "fo:break-before"))) {
		p->p.salto_antes = iguales(v, "page");
		p->mp |= FP_SALTO;
	}
	if ((v = xml_atr(n, "fo:keep-with-next"))) {
		p->p.juntos = iguales(v, "always");
		p->mp |= FP_JUNTOS;
	}
	if ((v = xml_atr(n, "fo:background-color"))) {
		p->p.fondo = fx_color(v);
		p->mp |= FP_FONDO;
	}
	if (borde) {
		const char *a[] = { "fo:border", "fo:border-top", "fo:border-bottom", "fo:border-left", "fo:border-right" };
		int k;

		for (k = 0; k < 5; k++)
			if ((v = xml_atr(n, a[k])) && (busca(v, "solid") || busca(v, "double") || busca(v, "dotted") || busca(v, "dashed") ||
							   busca(v, "groove") || busca(v, "ridge") || busca(v, "inset") || busca(v, "outset")))
				*borde = 1;
	}
	ts = xml_hijo(n, "style:tab-stops");
	if (ts) {
		xml_nodo *t;
		u32 k = 0;

		for (t = xml_hijo(ts, "style:tab-stop"); t && k < MAX_TABS; t = xml_sig(t, "style:tab-stop")) {
			const char *ty = xml_atr(t, "style:type"), *le = xml_atr(t, "style:leader-text");

			p->p.tabs[k].pos = fx_medida(xml_atr(t, "style:position"));
			if (p->p.tabs[k].pos < 0)
				p->p.tabs[k].pos = 0;
			p->p.tabs[k].tipo = !ty ? TAB_IZQ : iguales(ty, "center") ? TAB_CENTRO : iguales(ty, "right") ? TAB_DER : iguales(ty, "char") ? TAB_DECIMAL : TAB_IZQ;
			p->p.tabs[k].relleno = le && (le[0] == '.' || le[0] == '-' || le[0] == '_') ? (u8)le[0] : 0;
			if (!le && xml_atr(t, "style:leader-style") && !iguales(xml_atr(t, "style:leader-style"), "none"))
				p->p.tabs[k].relleno = '.';
			k++;
		}
		p->p.n_tabs = (u8)k;
		p->mp |= FP_TABS;
	}
}

static void lee_estilo(lector_odt *c, const xml_nodo *n, int automatico)
{
	ostilo *s;
	const char *v;
	xml_nodo *h;

	if (!(c->est = crece(c->est, &c->cabe_est, c->n_est + 1, sizeof(ostilo)))) {
		c->d->sin_memoria = 1;
		return;
	}
	s = &c->est[c->n_est++];
	memset(s, 0, sizeof *s);
	s->est_doc = -1;
	s->fondo = SIN_COLOR;
	s->automatico = automatico;
	s->pr.l = fmt_letra_normal();
	s->pr.p = fmt_parrafo_normal();
	copia(s->nombre, (v = xml_atr(n, "style:name")) ? v : "", sizeof s->nombre);
	copia(s->padre, (v = xml_atr(n, "style:parent-style-name")) ? v : "", sizeof s->padre);
	copia(s->familia, (v = xml_atr(n, "style:family")) ? v : "", sizeof s->familia);
	if ((v = xml_atr(n, "style:display-name")))
		copia(s->mostrar, v, sizeof s->mostrar);
	else
		fx_nombre_odf(s->nombre, s->mostrar, sizeof s->mostrar);
	copia(s->lista, (v = xml_atr(n, "style:list-style-name")) ? v : "", sizeof s->lista);
	if ((v = xml_atr(n, "style:master-page-name")) && v[0])
		s->pagina = 1;
	if ((v = xml_atr(n, "style:default-outline-level")) && numero(v, NULL) > 0) {
		s->pr.p.nivel_titulo = (u8)(numero(v, NULL) % 10);
		s->pr.mp |= FP_NIVTIT;
	}
	props_texto(c, xml_hijo(n, "style:text-properties"), &s->pr);
	props_parrafo(xml_hijo(n, "style:paragraph-properties"), &s->pr, NULL);
	if ((h = xml_hijo(n, "style:table-cell-properties"))) {
		props_parrafo(h, &s->pr, &s->borde);
		s->fondo = (s->pr.mp & FP_FONDO) ? s->pr.p.fondo : SIN_COLOR;
	}
	if ((h = xml_hijo(n, "style:table-column-properties")))
		s->ancho = fx_medida(xml_atr(h, "style:column-width"));
	if ((h = xml_hijo(n, "style:table-properties"))) {
		v = xml_atr(h, "table:align");
		s->alin_tabla = v && iguales(v, "center") ? AL_CENTRO : v && iguales(v, "right") ? AL_DER : AL_IZQ;
	}
	if ((h = xml_hijo(n, "style:graphic-properties"))) {
		const char *w = xml_atr(h, "style:wrap"), *hp = xml_atr(h, "style:horizontal-pos");

		s->ajuste = AJUSTE_LINEA;
		if (w && iguales(w, "right"))
			s->ajuste = AJUSTE_IZQ;
		else if (w && iguales(w, "left"))
			s->ajuste = AJUSTE_DER;
		else if (w && (iguales(w, "parallel") || iguales(w, "dynamic") || iguales(w, "biggest")))
			s->ajuste = hp && (iguales(hp, "right") || iguales(hp, "outside")) ? AJUSTE_DER : AJUSTE_IZQ;
	}
}

static void lee_lista(lector_odt *c, const xml_nodo *n)
{
	olista *l;
	xml_nodo *h;
	const char *v;
	int k;

	if (!(c->lis = crece(c->lis, &c->cabe_lis, c->n_lis + 1, sizeof(olista)))) {
		c->d->sin_memoria = 1;
		return;
	}
	l = &c->lis[c->n_lis++];
	memset(l, 0, sizeof *l);
	copia(l->nombre, (v = xml_atr(n, "style:name")) ? v : "", sizeof l->nombre);
	for (k = 0; k < 10; k++) {
		l->tipo[k] = LISTA_VINETAS;
		l->sang[k] = 720 + 360 * k;
		l->primera[k] = -360;
	}
	for (h = xml_primero(n); h; h = xml_sig(h, NULL)) {
		int nivel = (int)numero(xml_atr(h, "text:level") ? xml_atr(h, "text:level") : "1", NULL) - 1;
		xml_nodo *pp, *la;

		if (nivel < 0 || nivel > 9)
			continue;
		if (xml_es(h, "text:list-level-style-number")) {
			const char *f = xml_atr(h, "style:num-format");

			l->tipo[nivel] = !f || !f[0] ? LISTA_NUMEROS : (f[0] == 'a' || f[0] == 'A') ? LISTA_LETRAS : (f[0] == 'i' || f[0] == 'I') ? LISTA_ROMANOS : LISTA_NUMEROS;
			if ((v = xml_atr(h, "text:start-value")) && numero(v, NULL) > 1)
				l->inicio[nivel] = (u32)numero(v, NULL) % 100000;
		} else if (xml_es(h, "text:list-level-style-bullet") || xml_es(h, "text:list-level-style-image"))
			l->tipo[nivel] = LISTA_VINETAS;
		pp = xml_hijo(h, "style:list-level-properties");
		la = xml_hijo(pp, "style:list-level-label-alignment");
		if (la) {
			if ((v = xml_atr(la, "fo:margin-left")))
				l->sang[nivel] = fx_medida(v);
			if ((v = xml_atr(la, "fo:text-indent")))
				l->primera[nivel] = fx_medida(v);
		} else if (pp && (v = xml_atr(pp, "text:space-before"))) {
			i32 mw = fx_medida(xml_atr(pp, "text:min-label-width"));

			l->sang[nivel] = fx_medida(v) + (mw > 0 ? mw : 360);
			l->primera[nivel] = -(mw > 0 ? mw : 360);
		}
	}
}

/* los estilos de una sección (office:styles o office:automatic-styles) */
static void lee_estilos(lector_odt *c, const xml_nodo *n, int automatico)
{
	xml_nodo *h;

	for (h = xml_primero(n); h; h = xml_sig(h, NULL)) {
		if (xml_es(h, "style:style"))
			lee_estilo(c, h, automatico);
		else if (xml_es(h, "text:list-style"))
			lee_lista(c, h);
		else if (xml_es(h, "style:default-style")) {
			const char *f = xml_atr(h, "style:family");

			if (f && iguales(f, "paragraph")) {
				props_texto(c, xml_hijo(h, "style:text-properties"), &c->def);
				props_parrafo(xml_hijo(h, "style:paragraph-properties"), &c->def, NULL);
			}
		}
	}
}

static void lee_caras(lector_odt *c, const xml_nodo *n)
{
	xml_nodo *h;

	for (h = xml_hijo(n, "style:font-face"); h; h = xml_sig(h, "style:font-face")) {
		const char *nm = xml_atr(h, "style:name"), *f = xml_atr(h, "svg:font-family");
		ocara *o;

		if (!nm || !f || !(c->caras = crece(c->caras, &c->cabe_caras, c->n_caras + 1, sizeof(ocara))))
			continue;
		o = &c->caras[c->n_caras++];
		copia(o->nombre, nm, sizeof o->nombre);
		familia_limpia(f, o->familia, sizeof o->familia);
	}
}

/* lo que dice un estilo con todos sus padres (y el de serie debajo) */
static void efectivo(lector_odt *c, ostilo *s, fx_props *r, int hondo)
{
	if (!s || hondo > 24 || s->resolviendo) {
		*r = c->def;
		return;
	}
	s->resolviendo = 1;
	if (s->padre[0])
		efectivo(c, busca_estilo(c, s->padre, s->familia[0] ? s->familia : NULL), r, hondo + 1);
	else
		*r = c->def;
	s->resolviendo = 0;
	fx_junta(r, &s->pr);
}

/* el estilo de Pluma de un estilo de párrafo del ODT */
static int estilo_doc(lector_odt *c, ostilo *s)
{
	documento *d = c->d;
	int hondo = 0, k;
	ostilo *s0 = s;
	fx_props ef;
	estilo e;

	while (s && s->automatico && hondo++ < 24)
		s = busca_estilo(c, s->padre, "paragraph");
	if (!s)
		return EST_NORMAL;
	if (s->est_doc >= 0)
		return s->est_doc;
	(void)s0;
	efectivo(c, s, &ef, 0);
	k = fx_estilo_serie(s->mostrar);
	if (k < 0)
		k = fx_estilo_serie(s->nombre);
	if (k < 0)
		k = doc_estilo(d, s->mostrar);
	memset(&e, 0, sizeof e);
	e.letra = fmt_letra_normal();
	e.p = fmt_parrafo_normal();
	fx_aplica(&ef, &e.letra, &e.p);
	if (k >= 0) {
		/* el de serie, con lo que dice el documento */
		d->est[k].letra = e.letra;
		e.p.estilo = (u16)k;
		e.p.nivel_titulo = (ef.mp & FP_NIVTIT) ? ef.p.nivel_titulo : d->est[k].p.nivel_titulo;
		d->est[k].p = e.p;
	} else {
		copia(e.nombre, s->mostrar[0] ? s->mostrar : s->nombre, sizeof e.nombre);
		k = doc_nuevo_estilo(d, &e);
		d->est[k].p.estilo = (u16)k;
		d->est[k].siguiente = (u16)k;
	}
	s->est_doc = k;
	return k;
}

/* ---- el texto ---- */
static void contenido(lector_odt *c, const xml_nodo *n, const fmt_letra *base);

static void pon_imagen(lector_odt *c, const xml_nodo *fr, const fmt_letra *base)
{
	xml_nodo *im = xml_hijo(fr, "draw:image");
	const char *href, *v;
	u8 *datos;
	u32 n;
	int k;
	ostilo *g;
	const char *anc = xml_atr(fr, "text:anchor-type");
	u32 ch;

	if (!im) {
		/* (un cuadro de texto: su texto, en línea) */
		xml_nodo *tb = xml_hijo(fr, "draw:text-box");

		if (tb)
			contenido(c, tb, base);
		return;
	}
	href = xml_atr(im, "xlink:href");
	if (!href || busca(href, "://") || c->imagenes > 2000)
		return;
	datos = zip_saca(c->z, href, &n);
	if (!datos)
		return;
	k = doc_nueva_imagen(c->d, datos, n);
	if (k < 0) {
		so_suelta(datos);
		return;
	}
	c->imagenes++;
	if ((v = xml_atr(fr, "svg:width")) && fx_medida(v) > 20)
		c->d->img[k].ancho = fx_medida(v) > 31680 ? 31680 : fx_medida(v);
	if ((v = xml_atr(fr, "svg:height")) && fx_medida(v) > 20)
		c->d->img[k].alto = fx_medida(v) > 31680 ? 31680 : fx_medida(v);
	g = busca_estilo(c, xml_atr(fr, "draw:style-name"), NULL);
	if (anc && !iguales(anc, "as-char") && g)
		c->d->img[k].ajuste = (u8)g->ajuste;
	ch = IMAGEN_0 + (u32)k;
	letras(c, &ch, 1, doc_fmt(c->d, base));
	c->espacio = 0;
}

static void pon_nota(lector_odt *c, const xml_nodo *n, const fmt_letra *base)
{
	xml_nodo *cuerpo = xml_hijo(n, "text:note-body"), *h;
	int k, dest, nota, primero = 1;
	u32 ch;
	fmt_letra fn;

	if (c->destino != D_CUERPO || !cuerpo)
		return;
	k = doc_nueva_nota(c->d);
	if (k < 0)
		return;
	ch = NOTA_0 + (u32)k;
	letras(c, &ch, 1, doc_fmt(c->d, base));
	dest = c->destino;
	nota = c->nota;
	c->destino = D_NOTA;
	c->nota = k;
	fn = c->d->est[EST_NOTA].letra;
	for (h = xml_primero(cuerpo); h; h = xml_sig(h, NULL)) {
		if (!xml_es(h, "text:p") && !xml_es(h, "text:h"))
			continue;
		if (!primero) {
			u32 s = SALTO_LINEA;

			letras(c, &s, 1, doc_fmt(c->d, &fn));
		}
		primero = 0;
		c->espacio = 1;
		contenido(c, h, &fn);
	}
	{
		parrafo *p = &c->d->notas[k];
		u32 q = 0;

		while (q < p->n && p->t[q] == ' ')
			q++;
		par_quita(p, 0, q);
	}
	c->destino = dest;
	c->nota = nota;
	c->espacio = 0;
}

static void contenido(lector_odt *c, const xml_nodo *n, const fmt_letra *base)
{
	xml_nodo *h;
	u16 f = doc_fmt(c->d, base);

	if (++c->hondo > 64) {
		c->hondo--;
		return;
	}
	for (h = n->hijo; h && !c->d->sin_memoria; h = h->sig) {
		if (!h->nombre) {
			/* el texto: los blancos seguidos, un espacio */
			u32 i = 0, k = 0, ch, t[256];

			while (i < h->largo) {
				i += (u32)utf8_lee((const u8 *)h->texto + i, h->largo - i, &ch);
				if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
					if (c->espacio)
						continue;
					ch = ' ';
					c->espacio = 1;
				} else
					c->espacio = 0;
				if (ch < 0x20)
					continue;
				t[k++] = ch;
				if (k == 256) {
					letras(c, t, k, f);
					k = 0;
				}
			}
			letras(c, t, k, f);
			continue;
		}
		if (xml_es(h, "text:s")) {
			const char *v = xml_atr(h, "text:c");
			long m = v ? numero(v, NULL) : 1, j;
			u32 sp = ' ';

			if (m < 1)
				m = 1;
			if (m > 1000)
				m = 1000;
			for (j = 0; j < m; j++)
				letras(c, &sp, 1, f);
			c->espacio = 0;
		} else if (xml_es(h, "text:tab")) {
			u32 t = '\t';

			letras(c, &t, 1, f);
			c->espacio = 0;
		} else if (xml_es(h, "text:line-break")) {
			u32 t = SALTO_LINEA;

			letras(c, &t, 1, f);
			c->espacio = 1;
		} else if (xml_es(h, "text:page-number")) {
			u32 t = CAMPO_PAGINA;

			letras(c, &t, 1, f);
			c->espacio = 0;
		} else if (xml_es(h, "text:page-count")) {
			u32 t = CAMPO_PAGINAS;

			letras(c, &t, 1, f);
			c->espacio = 0;
		} else if (xml_es(h, "text:span")) {
			ostilo *s = busca_estilo(c, xml_atr(h, "text:style-name"), NULL);
			fmt_letra fl = *base;

			{
				/* el estilo y sus padres (de arriba abajo), solo lo que dicen ellos */
				ostilo *cad[24];
				int nc = 0;

				while (s && nc < 24) {
					cad[nc++] = s;
					s = s->padre[0] ? busca_estilo(c, s->padre, NULL) : NULL;
				}
				while (nc-- > 0)
					fx_aplica(&cad[nc]->pr, &fl, NULL);
			}
			contenido(c, h, &fl);
		} else if (xml_es(h, "draw:frame")) {
			pon_imagen(c, h, base);
		} else if (xml_es(h, "draw:a")) {
			contenido(c, h, base);
		} else if (xml_es(h, "text:note")) {
			pon_nota(c, h, base);
		} else if (xml_es(h, "office:annotation") || xml_es(h, "text:note-citation") || xml_es(h, "text:soft-page-break") ||
			   xml_es(h, "text:bookmark") || xml_es(h, "text:bookmark-start") || xml_es(h, "text:bookmark-end") ||
			   xml_es(h, "text:change") || xml_es(h, "text:change-start") || xml_es(h, "text:change-end") ||
			   xml_es(h, "text:tracked-changes") || xml_es(h, "office:annotation-end") || xml_es(h, "text:reference-mark-start") ||
			   xml_es(h, "text:sequence-decls") || xml_es(h, "text:variable-decls")) {
			/* (se salta) */
		} else
			contenido(c, h, base);	/* (text:a, text:meta, campos...: su texto) */
	}
	c->hondo--;
}

/* el formato de un párrafo con su estilo */
static void formato_de(lector_odt *c, const char *nombre_estilo, fmt_parrafo *fp, fmt_letra *fl, int *est, ostilo **so)
{
	ostilo *s = busca_estilo(c, nombre_estilo, "paragraph");
	fx_props ef;

	*fp = fmt_parrafo_normal();
	*fl = fmt_letra_normal();
	efectivo(c, s, &ef, 0);
	fx_aplica(&ef, fl, fp);
	*est = s ? estilo_doc(c, s) : EST_NORMAL;
	fp->estilo = (u16)*est;
	if (so)
		*so = s;
}

/* un párrafo (text:p o text:h) */
static void parrafo_odt(lector_odt *c, const xml_nodo *n, int lista, int nivel, u16 lista_id, u32 inicio, const olista *ol)
{
	parrafo *p;
	fmt_parrafo fp;
	fmt_letra fl;
	int est;
	ostilo *s;
	const char *v;

	formato_de(c, xml_atr(n, "text:style-name"), &fp, &fl, &est, &s);
	if (s && s->pagina && !c->primer_par)
		fp.salto_antes = 1;
	if (xml_es(n, "text:h") && (v = xml_atr(n, "text:outline-level")))
		fp.nivel_titulo = (u8)(numero(v, NULL) % 10);
	if (lista) {
		fp.lista = (u8)lista;
		fp.nivel = (u8)(nivel > 8 ? 8 : nivel);
		fp.lista_id = lista_id;
		fp.inicio_lista = inicio;
		if (ol && !(s && (s->pr.mp & FP_SI))) {
			fp.sang_izq = ol->sang[fp.nivel];
			fp.sang_primera = ol->primera[fp.nivel];
		}
	}
	p = arch_parrafo(c->d);
	if (!p)
		return;
	p->p = fp;
	c->primer_par = 0;
	c->destino = D_CUERPO;
	c->espacio = 1;
	contenido(c, n, &fl);
	/* (sin el espacio del final, si lo dejó un blanco del XML) */
	p = &c->d->par[c->d->n_par - 1];
	(void)p;
}

static void bloque(lector_odt *c, const xml_nodo *n);

static void lista_odt(lector_odt *c, const xml_nodo *n, const char *estilo_padre, int nivel, u16 id)
{
	const char *sn = xml_atr(n, "text:style-name");
	const olista *ol;
	xml_nodo *it;

	if (!sn)
		sn = estilo_padre;
	if (nivel > 9 || c->hondo > 64)
		return;
	ol = busca_lista(c, sn);
	if (nivel == 0) {
		const char *cn = xml_atr(n, "text:continue-numbering");

		if (((cn && iguales(cn, "true")) || xml_atr(n, "text:continue-list")) && sn && iguales(sn, c->ultima_lista))
			id = c->ultima_lista_id;
		else
			id = ++c->sig_lista;
		copia(c->ultima_lista, sn ? sn : "", sizeof c->ultima_lista);
		c->ultima_lista_id = id;
	}
	c->hondo++;
	for (it = xml_primero(n); it; it = xml_sig(it, NULL)) {
		xml_nodo *h;
		u32 inicio = 0;
		const char *v;
		int cabecera = xml_es(it, "text:list-header");

		if (!xml_es(it, "text:list-item") && !cabecera)
			continue;
		if ((v = xml_atr(it, "text:start-value")) && numero(v, NULL) > 1)
			inicio = (u32)numero(v, NULL) % 100000;
		for (h = xml_primero(it); h; h = xml_sig(h, NULL)) {
			if (xml_es(h, "text:p") || xml_es(h, "text:h")) {
				int tipo = ol ? ol->tipo[nivel] : LISTA_VINETAS;

				if (!inicio && ol && ol->inicio[nivel] > 1 && it == xml_primero(n))
					inicio = ol->inicio[nivel];
				parrafo_odt(c, h, cabecera ? LISTA_NO : tipo, nivel, id, inicio, ol);
				if (cabecera && c->d->n_par && ol) {
					c->d->par[c->d->n_par - 1].p.sang_izq = ol->sang[nivel];
					c->d->par[c->d->n_par - 1].p.sang_primera = 0;
				}
				inicio = 0;
				cabecera = 1;	/* (los demás párrafos del elemento, sin marca) */
			} else if (xml_es(h, "text:list"))
				lista_odt(c, h, sn, nivel + 1, id);
			else
				bloque(c, h);
		}
	}
	c->hondo--;
}

/* ---- las tablas ---- */
#define MAX_FILAS_ODT 4000

typedef struct {
	const xml_nodo *celda;		/* NULL: vacía */
	int cubierta;
	int cols, filas;
} ocelda;

static void filas_de(const xml_nodo *n, const xml_nodo **filas, u32 *rep, u32 *nf, u32 max)
{
	xml_nodo *h;

	for (h = xml_primero(n); h && *nf < max; h = xml_sig(h, NULL)) {
		if (xml_es(h, "table:table-row")) {
			const char *v = xml_atr(h, "table:number-rows-repeated");
			long r = v ? numero(v, NULL) : 1;

			if (r < 1)
				r = 1;
			if (r > 200)
				r = 200;	/* (las de las hojas de cálculo, sin fin) */
			filas[*nf] = h;
			rep[*nf] = (u32)r;
			(*nf)++;
		} else if (xml_es(h, "table:table-header-rows") || xml_es(h, "table:table-rows") || xml_es(h, "table:table-row-group"))
			filas_de(h, filas, rep, nf, max);
	}
}

static void columnas_de(lector_odt *c, const xml_nodo *n, i32 *ancho, u32 *nc)
{
	xml_nodo *h;

	for (h = xml_primero(n); h && *nc < MAX_COLS; h = xml_sig(h, NULL)) {
		if (xml_es(h, "table:table-column")) {
			const char *v = xml_atr(h, "table:number-columns-repeated");
			long r = v ? numero(v, NULL) : 1, k;
			ostilo *s = busca_estilo(c, xml_atr(h, "table:style-name"), NULL);

			if (r < 1)
				r = 1;
			for (k = 0; k < r && *nc < MAX_COLS; k++)
				ancho[(*nc)++] = s && s->ancho > 0 ? s->ancho : 0;
		} else if (xml_es(h, "table:table-columns") || xml_es(h, "table:table-header-columns") || xml_es(h, "table:table-column-group"))
			columnas_de(c, h, ancho, nc);
	}
}

/* el texto de una celda (sus párrafos, uno detrás de otro con saltos de línea) */
static void celda_odt(lector_odt *c, const xml_nodo *n, int *primero, fmt_letra *fl)
{
	xml_nodo *h;

	if (c->hondo > 64)
		return;
	c->hondo++;
	for (h = xml_primero(n); h; h = xml_sig(h, NULL)) {
		if (xml_es(h, "text:p") || xml_es(h, "text:h")) {
			fmt_parrafo fp;
			int est;

			formato_de(c, xml_atr(h, "text:style-name"), &fp, fl, &est, NULL);
			if (!*primero) {
				u32 s = SALTO_LINEA;

				letras(c, &s, 1, doc_fmt(c->d, fl));
			} else {
				parrafo *p = destino(c);

				if (p) {
					u16 t = p->p.tabla, fi = p->p.fila, co = p->p.col;
					u8 ac = p->p.abarca_cols, af = p->p.abarca_filas;
					u32 fo = p->p.fondo;

					p->p = fp;
					p->p.tabla = t;
					p->p.fila = fi;
					p->p.col = co;
					p->p.abarca_cols = ac;
					p->p.abarca_filas = af;
					p->p.fondo = fo;
					p->p.salto_antes = 0;
				}
			}
			*primero = 0;
			c->espacio = 1;
			contenido(c, h, fl);
		} else if (xml_es(h, "text:list") || xml_es(h, "text:list-item") || xml_es(h, "table:table") ||
			   xml_es(h, "table:table-row") || xml_es(h, "table:table-cell") || xml_es(h, "text:section") ||
			   xml_es(h, "table:table-rows") || xml_es(h, "table:table-header-rows"))
			celda_odt(c, h, primero, fl);
	}
	c->hondo--;
}

static void tabla_odt(lector_odt *c, const xml_nodo *n)
{
	documento *d = c->d;
	const xml_nodo **filas;
	u32 *rep, nf = 0, nc = 0, total = 0, f, k, col;
	i32 ancho[MAX_COLS];
	ocelda *rej;
	int t, hay_borde = 0;
	ostilo *ts = busca_estilo(c, xml_atr(n, "table:style-name"), NULL);

	filas = so_pide(MAX_FILAS_ODT * sizeof *filas);
	rep = so_pide(MAX_FILAS_ODT * sizeof *rep);
	if (!filas || !rep) {
		so_suelta(filas);
		so_suelta(rep);
		d->sin_memoria = 1;
		return;
	}
	columnas_de(c, n, ancho, &nc);
	filas_de(n, filas, rep, &nf, MAX_FILAS_ODT);
	for (f = 0; f < nf; f++)
		total += rep[f];
	if (total > MAX_FILAS_ODT)
		total = MAX_FILAS_ODT;
	/* las columnas de verdad: las de la fila más larga */
	for (f = 0; f < nf; f++) {
		xml_nodo *h;
		u32 m = 0;

		for (h = xml_primero(filas[f]); h; h = xml_sig(h, NULL))
			if (xml_es(h, "table:table-cell") || xml_es(h, "table:covered-table-cell")) {
				const char *v = xml_atr(h, "table:number-columns-repeated");
				long r = v ? numero(v, NULL) : 1;

				if (r < 1)
					r = 1;
				m += r > MAX_COLS ? MAX_COLS : (u32)r;
			}
		if (m > nc)
			nc = m > MAX_COLS ? MAX_COLS : m;
	}
	if (!nc || !total || (u64)total * nc > 200000) {
		so_suelta(filas);
		so_suelta(rep);
		return;
	}
	rej = so_pide((size_t)total * nc * sizeof(ocelda));
	if (!rej) {
		so_suelta(filas);
		so_suelta(rep);
		d->sin_memoria = 1;
		return;
	}
	memset(rej, 0, (size_t)total * nc * sizeof(ocelda));
	{
		u32 fila = 0;

		for (f = 0; f < nf && fila < total; f++) {
			u32 r;

			for (r = 0; r < rep[f] && fila < total; r++, fila++) {
				xml_nodo *h;

				col = 0;
				for (h = xml_primero(filas[f]); h && col < nc; h = xml_sig(h, NULL)) {
					int cub = xml_es(h, "table:covered-table-cell");
					const char *v;
					long rr;

					if (!cub && !xml_es(h, "table:table-cell"))
						continue;
					v = xml_atr(h, "table:number-columns-repeated");
					rr = v ? numero(v, NULL) : 1;
					if (rr < 1)
						rr = 1;
					while (rr-- > 0 && col < nc) {
						ocelda *o = &rej[fila * nc + col];

						if (!o->cubierta) {
							o->celda = h;
							o->cubierta = cub;
							if (!cub) {
								long sc = (v = xml_atr(h, "table:number-columns-spanned")) ? numero(v, NULL) : 1;
								long sr = (v = xml_atr(h, "table:number-rows-spanned")) ? numero(v, NULL) : 1;

								o->cols = sc < 1 ? 1 : sc > 255 ? 255 : (int)sc;
								o->filas = sr < 1 ? 1 : sr > 255 ? 255 : (int)sr;
							}
						}
						col++;
					}
				}
			}
		}
		/* las tapadas por las unidas (aunque el XML no lo diga) */
		for (f = 0; f < total; f++)
			for (col = 0; col < nc; col++) {
				ocelda *o = &rej[f * nc + col];
				u32 a, b;

				if (o->cubierta || (o->cols <= 1 && o->filas <= 1))
					continue;
				if (col + (u32)o->cols > nc)
					o->cols = (int)(nc - col);
				if (f + (u32)o->filas > total)
					o->filas = (int)(total - f);
				for (a = f; a < f + (u32)o->filas; a++)
					for (b = col; b < col + (u32)o->cols; b++)
						if (a != f || b != col)
							rej[a * nc + b].cubierta = 1;
			}
	}
	t = doc_nueva_tabla(d, (u16)total, (u16)nc, doc_ancho_texto(d));
	if (t < 0) {
		so_suelta(rej);
		so_suelta(filas);
		so_suelta(rep);
		return;
	}
	{
		i32 suma = 0, sin = 0;

		for (k = 0; k < nc; k++) {
			if (ancho[k] > 0 && k < MAX_COLS)
				suma += ancho[k];
			else
				sin++;
		}
		for (k = 0; k < nc; k++)
			d->tab[t].ancho[k] = ancho[k] > 0 ? (ancho[k] < 200 ? 200 : ancho[k]) :
					      (doc_ancho_texto(d) - suma > 400 * sin && sin ? (doc_ancho_texto(d) - suma) / sin : doc_ancho_texto(d) / (i32)nc);
	}
	if (ts)
		d->tab[t].alin = (u8)ts->alin_tabla;
	for (f = 0; f < total && !d->sin_memoria; f++)
		for (col = 0; col < nc; col++) {
			ocelda *o = &rej[f * nc + col];
			parrafo *p = arch_parrafo(d);
			fmt_letra fl = fmt_letra_normal();
			int primero = 1;

			if (!p)
				break;
			p->p = d->est[EST_SIN_ESPACIO].p;
			p->p.estilo = EST_SIN_ESPACIO;
			p->p.tabla = (u16)(t + 1);
			p->p.fila = (u16)f;
			p->p.col = (u16)col;
			if (o->cubierta) {
				p->p.cubierta = 1;
				continue;
			}
			if (o->cols > 1)
				p->p.abarca_cols = (u8)o->cols;
			if (o->filas > 1)
				p->p.abarca_filas = (u8)o->filas;
			if (o->celda) {
				ostilo *cs = busca_estilo(c, xml_atr(o->celda, "table:style-name"), NULL);

				if (cs) {
					if (cs->borde)
						hay_borde = 1;
					p->p.fondo = cs->fondo;
				}
				c->destino = D_CUERPO;
				celda_odt(c, o->celda, &primero, &fl);
			}
		}
	d->tab[t].bordes = hay_borde ? BORDE_TODOS : BORDE_NO;
	so_suelta(rej);
	so_suelta(filas);
	so_suelta(rep);
}

static void bloque(lector_odt *c, const xml_nodo *n)
{
	if (c->d->sin_memoria)
		return;
	if (xml_es(n, "text:p") || xml_es(n, "text:h"))
		parrafo_odt(c, n, LISTA_NO, 0, 0, 0, NULL);
	else if (xml_es(n, "text:list"))
		lista_odt(c, n, NULL, 0, 0);
	else if (xml_es(n, "table:table"))
		tabla_odt(c, n);
	else if (xml_es(n, "text:section") || xml_es(n, "text:index-body") || xml_es(n, "text:table-of-content") ||
		 xml_es(n, "text:alphabetical-index") || xml_es(n, "text:illustration-index") || xml_es(n, "text:object-index") ||
		 xml_es(n, "text:user-index") || xml_es(n, "text:bibliography") || xml_es(n, "text:change-start")) {
		xml_nodo *h;

		if (xml_es(n, "text:table-of-content") || xml_es(n, "text:alphabetical-index") || xml_es(n, "text:illustration-index") ||
		    xml_es(n, "text:object-index") || xml_es(n, "text:user-index") || xml_es(n, "text:bibliography"))
			n = xml_hijo(n, "text:index-body") ? xml_hijo(n, "text:index-body") : n;
		if (c->hondo > 64)
			return;
		c->hondo++;
		for (h = xml_primero(n); h; h = xml_sig(h, NULL))
			bloque(c, h);
		c->hondo--;
	}
}

/* el encabezado o el pie de la página */
static void enc_pie(lector_odt *c, const xml_nodo *n, int dest)
{
	xml_nodo *h;
	int primero = 1;
	parrafo *p = dest == D_ENC ? &c->d->enc : &c->d->pie;
	fmt_letra fl;

	if (!n)
		return;
	c->destino = dest;
	for (h = xml_primero(n); h; h = xml_sig(h, NULL)) {
		fmt_parrafo fp;
		int est;

		if (!xml_es(h, "text:p") && !xml_es(h, "text:h"))
			continue;
		formato_de(c, xml_atr(h, "text:style-name"), &fp, &fl, &est, NULL);
		if (primero) {
			p->p = fp;
			p->p.salto_antes = 0;
		} else {
			u32 s = SALTO_LINEA;

			letras(c, &s, 1, doc_fmt(c->d, &fl));
		}
		primero = 0;
		c->espacio = 1;
		contenido(c, h, &fl);
	}
	if (dest == D_ENC)
		c->d->hay_enc = 1;
	else
		c->d->hay_pie = 1;
	c->destino = D_CUERPO;
}

static void pagina_odt(lector_odt *c, xml_nodo *raiz_estilos)
{
	documento *d = c->d;
	xml_nodo *ms = xml_hijo(raiz_estilos, "office:master-styles"), *mp, *au, *pl = NULL;
	const char *nl;

	if (!ms)
		return;
	mp = xml_hijo(ms, "style:master-page");
	{
		xml_nodo *h;

		for (h = mp; h; h = xml_sig(h, "style:master-page"))
			if (xml_atr(h, "style:name") && iguales(xml_atr(h, "style:name"), "Standard"))
				mp = h;
	}
	if (!mp)
		return;
	nl = xml_atr(mp, "style:page-layout-name");
	au = xml_hijo(raiz_estilos, "office:automatic-styles");
	if (nl && au) {
		xml_nodo *h;

		for (h = xml_hijo(au, "style:page-layout"); h; h = xml_sig(h, "style:page-layout"))
			if (xml_atr(h, "style:name") && iguales(xml_atr(h, "style:name"), nl))
				pl = h;
	}
	if (pl) {
		xml_nodo *pp = xml_hijo(pl, "style:page-layout-properties"), *hs, *fs;
		i32 v;
		i32 enc_al = 0, pie_al = 0;

		if (pp) {
			if ((v = fx_medida(xml_atr(pp, "fo:page-width"))) >= 2880 && v <= 31680)
				d->pag_an = v;
			if ((v = fx_medida(xml_atr(pp, "fo:page-height"))) >= 2880 && v <= 31680)
				d->pag_al = v;
			if (xml_atr(pp, "fo:margin-left") && (v = fx_medida(xml_atr(pp, "fo:margin-left"))) >= 0 && v < 8000)
				d->marg_izq = v;
			if (xml_atr(pp, "fo:margin-right") && (v = fx_medida(xml_atr(pp, "fo:margin-right"))) >= 0 && v < 8000)
				d->marg_der = v;
			if (xml_atr(pp, "fo:margin-top") && (v = fx_medida(xml_atr(pp, "fo:margin-top"))) >= 0 && v < 8000)
				d->marg_sup = v;
			if (xml_atr(pp, "fo:margin-bottom") && (v = fx_medida(xml_atr(pp, "fo:margin-bottom"))) >= 0 && v < 8000)
				d->marg_inf = v;
		}
		/* el encabezado va dentro del margen de ODF: el texto empieza debajo */
		hs = xml_camino(pl, "style:header-style/style:header-footer-properties");
		fs = xml_camino(pl, "style:footer-style/style:header-footer-properties");
		if (hs && xml_hijo(mp, "style:header")) {
			enc_al = fx_medida(xml_atr(hs, "fo:min-height")) + fx_medida(xml_atr(hs, "fo:margin-bottom"));
			if (enc_al < 300)
				enc_al = 300;
			d->dist_enc = d->marg_sup;
			d->marg_sup += enc_al;
		}
		if (fs && xml_hijo(mp, "style:footer")) {
			pie_al = fx_medida(xml_atr(fs, "fo:min-height")) + fx_medida(xml_atr(fs, "fo:margin-top"));
			if (pie_al < 300)
				pie_al = 300;
			d->dist_pie = d->marg_inf;
			d->marg_inf += pie_al;
		}
		if (d->marg_sup > 8000)
			d->marg_sup = 8000;
		if (d->marg_inf > 8000)
			d->marg_inf = 8000;
	}
	enc_pie(c, xml_hijo(mp, "style:header"), D_ENC);
	enc_pie(c, xml_hijo(mp, "style:footer"), D_PIE);
}

static void meta_odt(lector_odt *c)
{
	u32 n;
	u8 *m = zip_saca(c->z, "meta.xml", &n);
	xml_doc *x;
	xml_nodo *me;
	bufer b = { 0 };

	if (!m)
		return;
	x = xml_lee((char *)m, n);
	so_suelta(m);
	if (!x)
		return;
	me = xml_camino(xml_raiz(x), "office:meta");
	if (me) {
		const char *que[3] = { "dc:title", "dc:creator", "dc:subject" };
		char *a[3] = { c->d->titulo, c->d->autor, c->d->asunto };
		u32 cabe[3] = { sizeof c->d->titulo, sizeof c->d->autor, sizeof c->d->asunto };
		int k;

		for (k = 0; k < 3; k++) {
			xml_nodo *h = xml_hijo(me, que[k]);

			if (!h && k == 1)
				h = xml_hijo(me, "meta:initial-creator");
			if (!h)
				continue;
			b.n = 0;
			xml_texto(h, &b);
			buf_c(&b, 0);
			if (!b.mal)
				copia(a[k], (char *)b.d, cabe[k]);
		}
	}
	buf_suelta(&b);
	xml_suelta(x);
}

documento *odt_lee(const u8 *datos, u32 n, char *error, u32 cabe)
{
	lector_odt c;
	u32 nc = 0, ns = 0;
	u8 *cont = NULL, *est = NULL;
	xml_doc *xc = NULL, *xs = NULL;
	xml_nodo *cuerpo;

	memset(&c, 0, sizeof c);
	c.z = zip_abre(datos, n);
	if (!c.z) {
		copia(error, "No es un documento ODT (no es un ZIP que se pueda leer).", cabe);
		return NULL;
	}
	cont = zip_saca(c.z, "content.xml", &nc);
	if (!cont || !(xc = xml_lee((char *)cont, nc))) {
		copia(error, "El documento ODT no trae su texto (content.xml) o está estropeado.", cabe);
		goto mal;
	}
	so_suelta(cont);
	cont = NULL;
	c.d = doc_nuevo();
	if (!c.d)
		goto mal;
	doc_quita_parrafos(c.d, 0, c.d->n_par);
	c.def.l = fmt_letra_normal();
	c.def.p = fmt_parrafo_normal();
	/* lo de LibreOffice sin decir nada: Liberation Serif de 12, sin espacios */
	c.def.p.despues = 0;
	c.def.p.inter = 100;
	c.def.mp = FP_DES | FP_INTER | FP_ALIN | FP_SI | FP_SD | FP_SP | FP_ANT;
	c.def.ml = FL_TAM | FL_FAM;
	c.nota = -1;
	c.primer_par = 1;
	est = zip_saca(c.z, "styles.xml", &ns);
	if (est) {
		xs = xml_lee((char *)est, ns);
		so_suelta(est);
		est = NULL;
	}
	if (xs) {
		lee_caras(&c, xml_hijo(xml_raiz(xs), "office:font-face-decls"));
		lee_estilos(&c, xml_hijo(xml_raiz(xs), "office:styles"), 0);
		lee_estilos(&c, xml_hijo(xml_raiz(xs), "office:automatic-styles"), 1);
	}
	lee_caras(&c, xml_hijo(xml_raiz(xc), "office:font-face-decls"));
	lee_estilos(&c, xml_hijo(xml_raiz(xc), "office:automatic-styles"), 1);
	if (xs)
		pagina_odt(&c, xml_raiz(xs));
	cuerpo = xml_camino(xml_raiz(xc), "office:body/office:text");
	if (cuerpo) {
		xml_nodo *h;

		for (h = xml_primero(cuerpo); h && !c.d->sin_memoria; h = xml_sig(h, NULL))
			bloque(&c, h);
	}
	meta_odt(&c);
	if (c.d->sin_memoria) {
		copia(error, "No hay memoria para abrirlo.", cabe);
		goto mal;
	}
	if (!c.d->n_par)
		arch_parrafo(c.d);
	fx_repasa_tablas(c.d);
	xml_suelta(xc);
	xml_suelta(xs);
	zip_cierra(c.z);
	so_suelta(c.est);
	so_suelta(c.lis);
	so_suelta(c.caras);
	return c.d;
mal:
	so_suelta(cont);
	so_suelta(est);
	xml_suelta(xc);
	xml_suelta(xs);
	zip_cierra(c.z);
	so_suelta(c.est);
	so_suelta(c.lis);
	so_suelta(c.caras);
	doc_suelta(c.d);
	return NULL;
}

/* ======================================================================
 *  Escribir
 * ====================================================================== */
static void nombre_interno(const char *s, char *d, u32 cabe)
{
	u32 k = 0;

	for (; *s && k + 5 < cabe; s++) {
		u8 ch = (u8)*s;

		if (ch == ' ') {
			memcpy(d + k, "_20_", 4);
			k += 4;
		} else if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '-' || ch >= 0x80)
			d[k++] = (char)ch;
		else {
			fmt(d + k, cabe - k, "_%02x_", ch);
			k += 4;
		}
	}
	d[k] = 0;
	if (!k)
		copia(d, "Estilo", cabe);
}

static void nombre_estilo(const documento *d, u32 i, char *interno, char *visible, u32 cabe)
{
	const char *v = i < EST_N && d->est[i].de_serie ? fx_nombre_odt((int)i) : d->est[i].nombre;

	copia(visible, v, cabe);
	nombre_interno(v, interno, cabe);
}

static void pon_txt_props(bufer *b, const fmt_letra *f)
{
	char t[64];

	buf_txt(b, "<style:text-properties");
	if (f->familia[0]) {
		fmt(t, sizeof t, "'%s'", f->familia);
		xml_pon_atr(b, "fo:font-family", t);
		xml_pon_atr(b, "style:font-family-asian", t);
		xml_pon_atr(b, "style:font-family-complex", t);
	}
	fmt(t, sizeof t, "%u.%upt", f->tam / 2, (f->tam & 1) ? 5 : 0);
	xml_pon_atr(b, "fo:font-size", t);
	xml_pon_atr(b, "fo:font-weight", f->negrita ? "bold" : "normal");
	xml_pon_atr(b, "fo:font-style", f->cursiva ? "italic" : "normal");
	if (f->subrayado) {
		buf_txt(b, " style:text-underline-style=\"solid\" style:text-underline-width=\"auto\" style:text-underline-color=\"font-color\"");
	} else
		buf_txt(b, " style:text-underline-style=\"none\"");
	buf_txt(b, f->tachado ? " style:text-line-through-style=\"solid\"" : " style:text-line-through-style=\"none\"");
	if (f->color != SIN_COLOR)
		buf_fmt(b, " fo:color=\"#%06x\"", f->color);
	else
		buf_txt(b, " style:use-window-font-color=\"true\"");
	if (f->resalte != SIN_COLOR)
		buf_fmt(b, " fo:background-color=\"#%06x\"", f->resalte);
	else
		buf_txt(b, " fo:background-color=\"transparent\"");
	buf_txt(b, f->vert == 1 ? " style:text-position=\"super 58%\"" : f->vert == 2 ? " style:text-position=\"sub 58%\"" : " style:text-position=\"0% 100%\"");
	buf_txt(b, f->versalitas ? " fo:font-variant=\"small-caps\"" : " fo:font-variant=\"normal\"");
	buf_txt(b, "/>");
}

static void pon_par_props(bufer *b, const fmt_parrafo *p, int con_salto)
{
	char t[32];
	u32 k;

	buf_txt(b, "<style:paragraph-properties");
	xml_pon_atr(b, "fo:text-align", p->alin == AL_CENTRO ? "center" : p->alin == AL_DER ? "end" : p->alin == AL_JUST ? "justify" : "start");
	fx_cm(t, sizeof t, p->sang_izq);
	xml_pon_atr(b, "fo:margin-left", t);
	fx_cm(t, sizeof t, p->sang_der);
	xml_pon_atr(b, "fo:margin-right", t);
	fx_cm(t, sizeof t, p->sang_primera);
	xml_pon_atr(b, "fo:text-indent", t);
	fx_cm(t, sizeof t, p->antes);
	xml_pon_atr(b, "fo:margin-top", t);
	fx_cm(t, sizeof t, p->despues);
	xml_pon_atr(b, "fo:margin-bottom", t);
	if (p->inter_tipo == INTER_MULTIPLE)
		buf_fmt(b, " fo:line-height=\"%d%%\"", p->inter > 0 ? p->inter : 100);
	else if (p->inter_tipo == INTER_EXACTO) {
		fx_cm(t, sizeof t, p->inter);
		xml_pon_atr(b, "fo:line-height", t);
	} else {
		fx_cm(t, sizeof t, p->inter);
		xml_pon_atr(b, "style:line-height-at-least", t);
	}
	if (con_salto && p->salto_antes)
		buf_txt(b, " fo:break-before=\"page\"");
	if (p->juntos)
		buf_txt(b, " fo:keep-with-next=\"always\"");
	if (p->fondo != SIN_COLOR && !p->tabla)
		buf_fmt(b, " fo:background-color=\"#%06x\"", p->fondo);
	if (!p->n_tabs) {
		buf_txt(b, "/>");
		return;
	}
	buf_txt(b, "><style:tab-stops>");
	for (k = 0; k < p->n_tabs && k < MAX_TABS; k++) {
		fx_cm(t, sizeof t, p->tabs[k].pos);
		buf_txt(b, "<style:tab-stop");
		xml_pon_atr(b, "style:position", t);
		if (p->tabs[k].tipo != TAB_IZQ)
			xml_pon_atr(b, "style:type", p->tabs[k].tipo == TAB_CENTRO ? "center" : p->tabs[k].tipo == TAB_DER ? "right" : "char");
		if (p->tabs[k].tipo == TAB_DECIMAL)
			xml_pon_atr(b, "style:char", ",");
		if (p->tabs[k].relleno) {
			char r[2] = { (char)p->tabs[k].relleno, 0 };

			xml_pon_atr(b, "style:leader-style", p->tabs[k].relleno == '_' ? "solid" : p->tabs[k].relleno == '-' ? "dash" : "dotted");
			xml_pon_atr(b, "style:leader-text", r);
		}
		buf_txt(b, "/>");
	}
	buf_txt(b, "</style:tab-stops></style:paragraph-properties>");
}

/* los estilos automáticos de los párrafos: cada combinación distinta, una vez */
typedef struct {
	bufer b;			/* lo de los estilos */
	char **claves;
	u32 n, cabe;
} autos;

static int auto_estilo(autos *a, const char *clave)
{
	u32 i;

	for (i = 0; i < a->n; i++)
		if (iguales(a->claves[i], clave))
			return (int)i;
	if (a->n >= a->cabe) {
		u32 c = a->cabe ? a->cabe * 2 : 64;
		char **n = so_repide(a->claves, c * sizeof(char *));

		if (!n)
			return -1;
		a->claves = n;
		a->cabe = c;
	}
	a->claves[a->n] = duplica(clave);
	if (!a->claves[a->n])
		return -1;
	return (int)a->n++;
}

static void suelta_autos(autos *a)
{
	u32 i;

	for (i = 0; i < a->n; i++)
		so_suelta(a->claves[i]);
	so_suelta(a->claves);
	buf_suelta(&a->b);
}

/* el estilo P<n> de un párrafo: su clave es lo que lleva escrito */
static int estilo_parrafo(documento *d, autos *a, const fmt_parrafo *p, const char *lista)
{
	bufer t = { 0 };
	char in[200], vi[100];
	int k;
	u32 e = p->estilo < d->n_est ? p->estilo : 0;

	nombre_estilo(d, e, in, vi, sizeof in);
	buf_txt(&t, "<style:style style:family=\"paragraph\"");
	xml_pon_atr(&t, "style:parent-style-name", in);
	if (lista)
		xml_pon_atr(&t, "style:list-style-name", lista);
	buf_c(&t, '>');
	pon_par_props(&t, p, 1);
	buf_txt(&t, "</style:style>");
	buf_c(&t, 0);
	if (t.mal) {
		buf_suelta(&t);
		return 0;
	}
	k = auto_estilo(a, (char *)t.d);
	if (k >= 0 && (u32)k + 1 == a->n) {
		/* nuevo: con su nombre */
		char nm[16];

		fmt(nm, sizeof nm, "P%d", k + 1);
		buf_fmt(&a->b, "<style:style style:name=\"%s\"%s", nm, (char *)t.d + largo("<style:style"));
	}
	buf_suelta(&t);
	return k < 0 ? 0 : k + 1;
}

static const char *nombre_lista(int tipo)
{
	switch (tipo) {
	case LISTA_NUMEROS: return "L2";
	case LISTA_LETRAS: return "L3";
	case LISTA_ROMANOS: return "L4";
	}
	return "L1";
}

static void pon_listas(bufer *b)
{
	int t, k;
	static const char *const vin[3] = { "\xe2\x80\xa2", "\xe2\x80\x93", "\xe2\x96\xaa" };

	for (t = LISTA_VINETAS; t <= LISTA_ROMANOS; t++) {
		buf_fmt(b, "<text:list-style style:name=\"%s\">", nombre_lista(t));
		for (k = 0; k < 10; k++) {
			char m[32], s[32];

			fx_cm(m, sizeof m, 720 + 360 * k);
			fx_cm(s, sizeof s, -360);
			if (t == LISTA_VINETAS)
				buf_fmt(b, "<text:list-level-style-bullet text:level=\"%d\" text:bullet-char=\"%s\">", k + 1, vin[k % 3]);
			else
				buf_fmt(b, "<text:list-level-style-number text:level=\"%d\" style:num-suffix=\"%s\" style:num-format=\"%s\">", k + 1,
					t == LISTA_LETRAS ? ")" : ".", t == LISTA_LETRAS ? "a" : t == LISTA_ROMANOS ? "i" : "1");
			buf_fmt(b, "<style:list-level-properties text:list-level-position-and-space-mode=\"label-alignment\">"
				   "<style:list-level-label-alignment text:label-followed-by=\"listtab\" text:list-tab-stop-position=\"%s\" fo:text-indent=\"%s\" fo:margin-left=\"%s\"/>"
				   "</style:list-level-properties>", m, s, m);
			buf_txt(b, t == LISTA_VINETAS ? "</text:list-level-style-bullet>" : "</text:list-level-style-number>");
		}
		buf_txt(b, "</text:list-style>");
	}
}

typedef struct {
	documento *d;
	autos ap;			/* los P */
	bufer *cuerpo;
	zip_escritor *z;
	int hay_lista[5];
	u32 n_frames;
	u16 lista_antes;
} escritor_odt;

/* el texto de un párrafo (con sus T) */
static void pon_letras_odt(escritor_odt *w, bufer *b, const parrafo *p, int en_tabla)
{
	documento *d = w->d;
	u32 k;
	u16 f = 0xFFFF;
	int abierto = 0;

	for (k = 0; k < p->n; k++) {
		u32 c = p->t[k];

		if (p->f[k] != f && !es_imagen(c)) {
			if (abierto)
				buf_txt(b, "</text:span>");
			f = p->f[k];
			buf_fmt(b, "<text:span text:style-name=\"T%u\">", (u32)f + 1);
			abierto = 1;
		}
		if (c == ' ') {
			/* los espacios seguidos (y el primero), con text:s */
			u32 j = k;

			while (j < p->n && p->t[j] == ' ' && p->f[j] == f)
				j++;
			if (k == 0 || j - k > 1 || j == p->n) {
				if (k > 0 && !(j == p->n)) {
					buf_c(b, ' ');
					if (j - k > 1)
						buf_fmt(b, "<text:s text:c=\"%u\"/>", j - k - 1);
				} else
					buf_fmt(b, "<text:s text:c=\"%u\"/>", j - k);
			} else
				buf_c(b, ' ');
			k = j - 1;
			continue;
		}
		if (c == '\t')
			buf_txt(b, "<text:tab/>");
		else if (c == SALTO_LINEA) {
			if (en_tabla)
				buf_txt(b, "<text:line-break/>");
			else
				buf_txt(b, "<text:line-break/>");
		} else if (c == CAMPO_PAGINA)
			buf_txt(b, "<text:page-number text:select-page=\"current\">1</text:page-number>");
		else if (c == CAMPO_PAGINAS)
			buf_txt(b, "<text:page-count>1</text:page-count>");
		else if (es_nota(c)) {
			u32 n = c - NOTA_0;

			if (n < d->n_notas) {
				buf_fmt(b, "<text:note text:id=\"ftn%u\" text:note-class=\"footnote\"><text:note-citation>%u</text:note-citation><text:note-body><text:p text:style-name=\"Footnote\">",
					n + 1, n + 1);
				pon_letras_odt(w, b, &d->notas[n], 1);
				buf_txt(b, "</text:p></text:note-body></text:note>");
			}
		} else if (es_imagen(c)) {
			u32 n = c - IMAGEN_0;
			char an[32], al[32];

			if (n >= d->n_img)
				continue;
			fx_cm(an, sizeof an, d->img[n].ancho);
			fx_cm(al, sizeof al, d->img[n].alto);
			w->n_frames++;
			if (abierto) {
				buf_txt(b, "</text:span>");
				abierto = 0;
				f = 0xFFFF;
			}
			buf_fmt(b, "<draw:frame draw:style-name=\"fr%d\" draw:name=\"Imagen%u\" text:anchor-type=\"%s\" svg:width=\"%s\" svg:height=\"%s\" draw:z-index=\"%u\">",
				d->img[n].ajuste == AJUSTE_IZQ ? 2 : d->img[n].ajuste == AJUSTE_DER ? 3 : 1, w->n_frames,
				d->img[n].ajuste == AJUSTE_LINEA ? "as-char" : "char", an, al, w->n_frames);
			buf_txt(b, "<draw:image xlink:type=\"simple\" xlink:show=\"embed\" xlink:actuate=\"onLoad\"");
			{
				char r[64];

				fmt(r, sizeof r, "Pictures/%s", d->img[n].nombre);
				xml_pon_atr(b, "xlink:href", r);
			}
			buf_txt(b, "/></draw:frame>");
		} else
			fx_pon_letra(b, c);
	}
	if (abierto)
		buf_txt(b, "</text:span>");
}

static void pon_parrafo_odt(escritor_odt *w, const parrafo *p)
{
	documento *d = w->d;
	bufer *b = w->cuerpo;
	int P, k;
	const char *etq = p->p.nivel_titulo ? "text:h" : "text:p";

	P = estilo_parrafo(d, &w->ap, &p->p, NULL);
	if (p->p.lista) {
		int primero = p->p.lista_id != w->lista_antes;

		w->hay_lista[p->p.lista] = 1;
		buf_fmt(b, "<text:list text:style-name=\"%s\"%s>", nombre_lista(p->p.lista), primero ? "" : " text:continue-numbering=\"true\"");
		for (k = 0; k < p->p.nivel; k++)
			buf_txt(b, "<text:list-item><text:list>");
		if (primero && p->p.inicio_lista)
			buf_fmt(b, "<text:list-item text:start-value=\"%u\">", p->p.inicio_lista);
		else if (primero)
			buf_txt(b, "<text:list-item text:start-value=\"1\">");
		else
			buf_txt(b, "<text:list-item>");
		w->lista_antes = p->p.lista_id;
	} else
		w->lista_antes = 0xFFFF;
	buf_fmt(b, "<%s text:style-name=\"P%d\"", etq, P);
	if (p->p.nivel_titulo)
		buf_fmt(b, " text:outline-level=\"%u\"", p->p.nivel_titulo);
	buf_c(b, '>');
	pon_letras_odt(w, b, p, 0);
	buf_fmt(b, "</%s>", etq);
	if (p->p.lista) {
		buf_txt(b, "</text:list-item>");
		for (k = 0; k < p->p.nivel; k++)
			buf_txt(b, "</text:list></text:list-item>");
		buf_txt(b, "</text:list>");
	}
}

/* una tabla (desde el párrafo i); da el de después */
static u32 pon_tabla_odt(escritor_odt *w, u32 i, autos *celdas, int numero)
{
	documento *d = w->d;
	bufer *b = w->cuerpo;
	u32 t = d->par[i].p.tabla, k, n;
	const tabla *tb = &d->tab[t - 1];
	u32 fila = 0xFFFF;

	for (n = 0; i + n < d->n_par && d->par[i + n].p.tabla == t; n++)
		;
	buf_fmt(b, "<table:table table:name=\"Tabla%d\" table:style-name=\"Tabla%d\">", numero, numero);
	for (k = 0; k < tb->cols; k++)
		buf_fmt(b, "<table:table-column table:style-name=\"Tabla%d.C%u\"/>", numero, k + 1);
	for (k = 0; k < n; k++) {
		const parrafo *p = &d->par[i + k];

		if (p->p.fila != fila) {
			if (fila != 0xFFFF)
				buf_txt(b, "</table:table-row>");
			buf_txt(b, "<table:table-row>");
			fila = p->p.fila;
		}
		if (p->p.cubierta) {
			buf_txt(b, "<table:covered-table-cell/>");
			continue;
		}
		{
			char clave[64];
			int ce;

			fmt(clave, sizeof clave, "%d %x", tb->bordes == BORDE_TODOS || (tb->bordes == BORDE_CABECERA && p->p.fila == 0) ? 1 : 0, p->p.fondo);
			ce = auto_estilo(celdas, clave);
			buf_fmt(b, "<table:table-cell table:style-name=\"Ce%d\" office:value-type=\"string\"", ce + 1);
		}
		if (p->p.abarca_cols > 1)
			buf_fmt(b, " table:number-columns-spanned=\"%u\"", p->p.abarca_cols);
		if (p->p.abarca_filas > 1)
			buf_fmt(b, " table:number-rows-spanned=\"%u\"", p->p.abarca_filas);
		buf_c(b, '>');
		{
			/* la celda: un text:p por cada trozo entre saltos de línea */
			parrafo q = *p;
			u32 a = 0, j;
			int P;
			fmt_parrafo fp = p->p;

			fp.salto_antes = 0;
			P = estilo_parrafo(d, &w->ap, &fp, NULL);
			for (j = 0; j <= p->n; j++)
				if (j == p->n || p->t[j] == SALTO_LINEA) {
					q.t = p->t + a;
					q.f = p->f + a;
					q.n = j - a;
					buf_fmt(b, "<text:p text:style-name=\"P%d\">", P);
					pon_letras_odt(w, b, &q, 1);
					buf_txt(b, "</text:p>");
					a = j + 1;
				}
		}
		buf_txt(b, "</table:table-cell>");
	}
	if (fila != 0xFFFF)
		buf_txt(b, "</table:table-row>");
	buf_txt(b, "</table:table>");
	return i + n;
}

static const char *mime_de(int tipo)
{
	switch (tipo) {
	case IMG_PNG: return "image/png";
	case IMG_JPG: return "image/jpeg";
	case IMG_BMP: return "image/bmp";
	}
	return "image/gif";
}

static void pon_enc_pie(escritor_odt *w, bufer *b, const parrafo *p, const char *etq)
{
	parrafo q = *p;
	u32 a = 0, j;
	int P;
	fmt_parrafo fp = p->p;

	fp.salto_antes = 0;
	fp.tabla = 0;
	P = estilo_parrafo(w->d, &w->ap, &fp, NULL);
	buf_fmt(b, "<%s>", etq);
	for (j = 0; j <= p->n; j++)
		if (j == p->n || p->t[j] == SALTO_LINEA) {
			q.t = p->t + a;
			q.f = p->f + a;
			q.n = j - a;
			buf_fmt(b, "<text:p text:style-name=\"P%d\">", P);
			pon_letras_odt(w, b, &q, 1);
			buf_txt(b, "</text:p>");
			a = j + 1;
		}
	buf_fmt(b, "</%s>", etq);
}

#define NS_OFFICE "xmlns:office=\"urn:oasis:names:tc:opendocument:xmlns:office:1.0\" xmlns:style=\"urn:oasis:names:tc:opendocument:xmlns:style:1.0\" " \
	"xmlns:text=\"urn:oasis:names:tc:opendocument:xmlns:text:1.0\" xmlns:table=\"urn:oasis:names:tc:opendocument:xmlns:table:1.0\" " \
	"xmlns:draw=\"urn:oasis:names:tc:opendocument:xmlns:drawing:1.0\" xmlns:fo=\"urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0\" " \
	"xmlns:xlink=\"http://www.w3.org/1999/xlink\" xmlns:dc=\"http://purl.org/dc/elements/1.1/\" xmlns:meta=\"urn:oasis:names:tc:opendocument:xmlns:meta:1.0\" " \
	"xmlns:svg=\"urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0\" office:version=\"1.3\""

int odt_escribe(documento *d, bufer *sal)
{
	escritor_odt w;
	bufer cuerpo = { 0 }, cont = { 0 }, est = { 0 }, man = { 0 }, meta = { 0 }, encpie = { 0 };
	autos celdas;
	u32 i, k;
	int r = -1, ntabla = 0;

	memset(&w, 0, sizeof w);
	memset(&celdas, 0, sizeof celdas);
	w.d = d;
	w.cuerpo = &cuerpo;
	w.lista_antes = 0xFFFF;
	w.z = zip_nuevo();
	if (!w.z)
		return -1;
	/* el cuerpo (primero: así se saben los estilos que hacen falta) */
	for (i = 0; i < d->n_par;) {
		if (d->par[i].p.tabla && d->par[i].p.tabla <= d->n_tab) {
			if (d->par[i].p.salto_antes) {
				/* (un salto antes de la tabla: un párrafo vacío con el salto) */
				fmt_parrafo fp = d->par[i].p;
				int P;

				fp.tabla = 0;
				P = estilo_parrafo(d, &w.ap, &fp, NULL);
				buf_fmt(&cuerpo, "<text:p text:style-name=\"P%d\"/>", P);
			}
			i = pon_tabla_odt(&w, i, &celdas, ++ntabla);
			w.lista_antes = 0xFFFF;
			continue;
		}
		pon_parrafo_odt(&w, &d->par[i]);
		i++;
	}
	if (d->hay_enc)
		pon_enc_pie(&w, &encpie, &d->enc, "style:header");
	if (d->hay_pie)
		pon_enc_pie(&w, &encpie, &d->pie, "style:footer");
	/* content.xml */
	buf_txt(&cont, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<office:document-content " NS_OFFICE "><office:automatic-styles>");
	for (k = 0; k < d->n_fl; k++) {
		buf_fmt(&cont, "<style:style style:name=\"T%u\" style:family=\"text\">", k + 1);
		pon_txt_props(&cont, &d->fl[k]);
		buf_txt(&cont, "</style:style>");
	}
	buf_pon(&cont, w.ap.b.d, w.ap.b.n);
	/* las tablas: su ancho y sus columnas */
	{
		int nt = 0;

		for (i = 0; i < d->n_par;) {
			u32 t = d->par[i].p.tabla, n;
			const tabla *tb;
			i32 tot = 0;
			char m[32];

			if (!t || t > d->n_tab) {
				i++;
				continue;
			}
			tb = &d->tab[t - 1];
			nt++;
			for (k = 0; k < tb->cols; k++)
				tot += tb->ancho[k];
			fx_cm(m, sizeof m, tot);
			buf_fmt(&cont, "<style:style style:name=\"Tabla%d\" style:family=\"table\"><style:table-properties style:width=\"%s\" table:align=\"%s\"/></style:style>",
				nt, m, tb->alin == AL_CENTRO ? "center" : tb->alin == AL_DER ? "right" : "left");
			for (k = 0; k < tb->cols; k++) {
				fx_cm(m, sizeof m, tb->ancho[k]);
				buf_fmt(&cont, "<style:style style:name=\"Tabla%d.C%u\" style:family=\"table-column\"><style:table-column-properties style:column-width=\"%s\"/></style:style>",
					nt, k + 1, m);
			}
			for (n = 0; i + n < d->n_par && d->par[i + n].p.tabla == t; n++)
				;
			i += n;
		}
	}
	for (k = 0; k < celdas.n; k++) {
		const char *c = celdas.claves[k];
		u32 fondo = 0;
		const char *q = c + 2;

		while (*q) {
			char h = *q++;

			fondo = fondo * 16 + (u32)(h >= 'a' ? h - 'a' + 10 : h - '0');
		}
		buf_fmt(&cont, "<style:style style:name=\"Ce%u\" style:family=\"table-cell\"><style:table-cell-properties fo:padding=\"0.097cm\" fo:border=\"%s\"",
			k + 1, c[0] == '1' ? "0.5pt solid #000000" : "none");
		if (fondo != SIN_COLOR)
			buf_fmt(&cont, " fo:background-color=\"#%06x\"", fondo & 0xFFFFFF);
		buf_txt(&cont, "/></style:style>");
	}
	buf_txt(&cont, "<style:style style:name=\"fr1\" style:family=\"graphic\"><style:graphic-properties style:vertical-pos=\"top\" style:vertical-rel=\"baseline\"/></style:style>");
	buf_txt(&cont, "<style:style style:name=\"fr2\" style:family=\"graphic\"><style:graphic-properties style:wrap=\"right\" style:horizontal-pos=\"left\" style:horizontal-rel=\"paragraph\" style:vertical-pos=\"top\" style:vertical-rel=\"paragraph\" fo:margin-right=\"0.3cm\"/></style:style>");
	buf_txt(&cont, "<style:style style:name=\"fr3\" style:family=\"graphic\"><style:graphic-properties style:wrap=\"left\" style:horizontal-pos=\"right\" style:horizontal-rel=\"paragraph\" style:vertical-pos=\"top\" style:vertical-rel=\"paragraph\" fo:margin-left=\"0.3cm\"/></style:style>");
	pon_listas(&cont);
	buf_txt(&cont, "</office:automatic-styles><office:body><office:text>");
	buf_pon(&cont, cuerpo.d, cuerpo.n);
	buf_txt(&cont, "</office:text></office:body></office:document-content>\n");
	/* styles.xml */
	buf_txt(&est, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<office:document-styles " NS_OFFICE "><office:styles>");
	buf_txt(&est, "<style:default-style style:family=\"paragraph\">");
	{
		fmt_letra fl = fmt_letra_normal();
		fmt_parrafo fp = fmt_parrafo_normal();

		fp.despues = 0;
		fp.inter = 100;
		pon_par_props(&est, &fp, 0);
		pon_txt_props(&est, &fl);
	}
	buf_txt(&est, "</style:default-style>");
	for (k = 0; k < d->n_est; k++) {
		char in[200], vi[100];
		u32 sg = d->est[k].siguiente < d->n_est ? d->est[k].siguiente : k;

		nombre_estilo(d, k, in, vi, sizeof in);
		buf_txt(&est, "<style:style style:family=\"paragraph\"");
		xml_pon_atr(&est, "style:name", in);
		xml_pon_atr(&est, "style:display-name", vi);
		{
			char si[200], sv[100];

			nombre_estilo(d, sg, si, sv, sizeof si);
			xml_pon_atr(&est, "style:next-style-name", si);
		}
		if (d->est[k].p.nivel_titulo)
			buf_fmt(&est, " style:default-outline-level=\"%u\"", d->est[k].p.nivel_titulo);
		buf_txt(&est, " style:class=\"text\">");
		pon_par_props(&est, &d->est[k].p, 0);
		pon_txt_props(&est, &d->est[k].letra);
		buf_txt(&est, "</style:style>");
	}
	buf_txt(&est, "</office:styles><office:automatic-styles>");
	{
		char an[32], al[32], mi[32], md[32], ms[32], mf[32], ea[32], pa[32];
		i32 sup = d->marg_sup, inf = d->marg_inf, ealto = 0, palto = 0;

		if (d->hay_enc) {
			ealto = d->marg_sup - d->dist_enc;
			if (ealto < 300)
				ealto = 300;
			sup = d->dist_enc;
		}
		if (d->hay_pie) {
			palto = d->marg_inf - d->dist_pie;
			if (palto < 300)
				palto = 300;
			inf = d->dist_pie;
		}
		fx_cm(an, sizeof an, d->pag_an);
		fx_cm(al, sizeof al, d->pag_al);
		fx_cm(mi, sizeof mi, d->marg_izq);
		fx_cm(md, sizeof md, d->marg_der);
		fx_cm(ms, sizeof ms, sup);
		fx_cm(mf, sizeof mf, inf);
		buf_fmt(&est, "<style:page-layout style:name=\"pm1\"><style:page-layout-properties fo:page-width=\"%s\" fo:page-height=\"%s\" style:print-orientation=\"%s\" fo:margin-top=\"%s\" fo:margin-bottom=\"%s\" fo:margin-left=\"%s\" fo:margin-right=\"%s\"/>",
			an, al, d->pag_an > d->pag_al ? "landscape" : "portrait", ms, mf, mi, md);
		fx_cm(ea, sizeof ea, ealto - 200);
		fx_cm(pa, sizeof pa, palto - 200);
		buf_fmt(&est, "<style:header-style>%s%s%s</style:header-style>", d->hay_enc ? "<style:header-footer-properties fo:min-height=\"" : "",
			d->hay_enc ? ea : "", d->hay_enc ? "\" fo:margin-bottom=\"0.353cm\"/>" : "");
		buf_fmt(&est, "<style:footer-style>%s%s%s</style:footer-style>", d->hay_pie ? "<style:header-footer-properties fo:min-height=\"" : "",
			d->hay_pie ? pa : "", d->hay_pie ? "\" fo:margin-top=\"0.353cm\"/>" : "");
		buf_txt(&est, "</style:page-layout>");
	}
	buf_txt(&est, "</office:automatic-styles><office:master-styles><style:master-page style:name=\"Standard\" style:page-layout-name=\"pm1\">");
	buf_pon(&est, encpie.d, encpie.n);
	buf_txt(&est, "</style:master-page></office:master-styles></office:document-styles>\n");
	/* (los P del encabezado y el pie van en content.xml, pero los usa
	 * styles.xml: se repiten en styles.xml como automáticos) */
	if (d->hay_enc || d->hay_pie) {
		bufer e2 = { 0 };
		const char *marca = "</office:automatic-styles>";
		u32 pos = 0;

		/* busca la marca en est y mete los P delante */
		for (pos = 0; pos + largo(marca) <= est.n; pos++)
			if (!memcmp(est.d + pos, marca, largo(marca)))
				break;
		buf_pon(&e2, est.d, pos);
		buf_pon(&e2, w.ap.b.d, w.ap.b.n);
		for (k = 0; k < d->n_fl; k++) {
			buf_fmt(&e2, "<style:style style:name=\"T%u\" style:family=\"text\">", k + 1);
			pon_txt_props(&e2, &d->fl[k]);
			buf_txt(&e2, "</style:style>");
		}
		buf_pon(&e2, est.d + pos, est.n - pos);
		buf_suelta(&est);
		est = e2;
	}
	/* meta.xml */
	buf_txt(&meta, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<office:document-meta " NS_OFFICE "><office:meta><meta:generator>Pluma/" PLUMA_VERSION " (Hispa-Pentium)</meta:generator>");
	if (d->titulo[0])
		xml_pon_elem(&meta, "dc:title", d->titulo);
	if (d->autor[0]) {
		xml_pon_elem(&meta, "dc:creator", d->autor);
		xml_pon_elem(&meta, "meta:initial-creator", d->autor);
	}
	if (d->asunto[0])
		xml_pon_elem(&meta, "dc:subject", d->asunto);
	buf_txt(&meta, "</office:meta></office:document-meta>\n");
	/* manifest.xml */
	buf_txt(&man, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<manifest:manifest xmlns:manifest=\"urn:oasis:names:tc:opendocument:xmlns:manifest:1.0\" manifest:version=\"1.3\">"
		      "<manifest:file-entry manifest:full-path=\"/\" manifest:version=\"1.3\" manifest:media-type=\"application/vnd.oasis.opendocument.text\"/>"
		      "<manifest:file-entry manifest:full-path=\"content.xml\" manifest:media-type=\"text/xml\"/>"
		      "<manifest:file-entry manifest:full-path=\"styles.xml\" manifest:media-type=\"text/xml\"/>"
		      "<manifest:file-entry manifest:full-path=\"meta.xml\" manifest:media-type=\"text/xml\"/>");
	for (k = 0; k < d->n_img; k++) {
		char r[64];

		fmt(r, sizeof r, "Pictures/%s", d->img[k].nombre);
		buf_txt(&man, "<manifest:file-entry");
		xml_pon_atr(&man, "manifest:full-path", r);
		xml_pon_atr(&man, "manifest:media-type", mime_de(d->img[k].tipo));
		buf_txt(&man, "/>");
	}
	buf_txt(&man, "</manifest:manifest>\n");
	if (cuerpo.mal || cont.mal || est.mal || man.mal || meta.mal || w.ap.b.mal || encpie.mal)
		goto fin;
	if (zip_mete(w.z, "mimetype", "application/vnd.oasis.opendocument.text", 39, 1) ||
	    zip_mete(w.z, "content.xml", cont.d, cont.n, 0) || zip_mete(w.z, "styles.xml", est.d, est.n, 0) ||
	    zip_mete(w.z, "meta.xml", meta.d, meta.n, 0))
		goto fin;
	for (k = 0; k < d->n_img; k++) {
		char r[64];

		fmt(r, sizeof r, "Pictures/%s", d->img[k].nombre);
		if (zip_mete(w.z, r, d->img[k].datos, d->img[k].n, d->img[k].tipo == IMG_JPG || d->img[k].tipo == IMG_PNG))
			goto fin;
	}
	if (zip_mete(w.z, "META-INF/manifest.xml", man.d, man.n, 0))
		goto fin;
	r = zip_acaba(w.z, sal);
	w.z = NULL;
fin:
	zip_deja(w.z);
	buf_suelta(&cuerpo);
	buf_suelta(&cont);
	buf_suelta(&est);
	buf_suelta(&man);
	buf_suelta(&meta);
	buf_suelta(&encpie);
	suelta_autos(&w.ap);
	suelta_autos(&celdas);
	return r;
}
