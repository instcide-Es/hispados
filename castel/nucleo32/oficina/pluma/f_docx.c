/* f_docx.c - Pluma 2: el documento de Word (.DOCX, Office Open XML). Un
 * ZIP (oficina/comun/zip.c) con [Content_Types].xml, _rels/.rels,
 * word/document.xml (el texto), word/styles.xml (los estilos),
 * word/numbering.xml (las listas), word/_rels/document.xml.rels (dónde
 * está cada cosa), word/header1.xml y footer1.xml, word/footnotes.xml,
 * word/media/ (las imágenes, tal cual) y docProps/core.xml (título y
 * autor). Se lee lo esencial (texto, estilos, formato, listas, tablas con
 * celdas unidas, imágenes, saltos, página, encabezado y pie con sus campos,
 * notas al pie) y lo que no se entiende se salta.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "archivo.h"
#include "f_xmlcomun.h"
#include "zip.h"
#include "xml.h"
#include "letras.h"

#define EMU_TW 635			/* 914400 EMU por pulgada, 1440 twips */

typedef struct {
	char id[64];
	char nombre[64];
	char basado[64];
	int tipo;			/* 1 párrafo, 2 carácter, 3 tabla */
	int defecto;
	fx_props pr;
	int num_id, num_nivel;		/* (párrafo) su lista */
	int bordes;			/* (tabla) */
	int est_doc;
	int resolviendo;
} dstilo;

typedef struct {
	int id;
	u8 tipo[9];
	u32 inicio[9];
	i32 sang[9], primera[9];
	int hay_sang[9];
} dabs;

typedef struct {
	int id, abs;
	u32 inicio[9];			/* (startOverride) 0: el del abstracto */
	int usado;
} dnum;

typedef struct {
	char id[32];
	char destino[160];
} drel;

enum { D_CUERPO, D_NOTA, D_ENC, D_PIE };

typedef struct {
	documento *d;
	zip_lector *z;
	dstilo *est;
	u32 n_est, cabe_est;
	dabs *abs;
	u32 n_abs, cabe_abs;
	dnum *num;
	u32 n_num, cabe_num;
	drel *rel;
	u32 n_rel, cabe_rel;
	fx_props def;
	char letra_menor[48], letra_mayor[48];
	xml_doc *notas;
	int destino, nota;
	int salto_pendiente;
	/* los campos: 0 nada; 1 en la orden; 2 en el resultado (de uno que es nuestro); 3 en el resultado de otro */
	int campo;
	char orden[64];
	u32 imagenes;
	int hondo;
} lector_docx;

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

static const char *val(const xml_nodo *n)
{
	return xml_atr(n, "w:val");
}

static int vale(const xml_nodo *n)
{
	const char *v = val(n);

	if (!v)
		return 1;
	return !(iguales(v, "0") || iguales(v, "false") || iguales(v, "off") || iguales(v, "none"));
}

/* el primer descendiente de ese nombre (sin recursión, con tope) */
static xml_nodo *dentro(const xml_nodo *n, const char *nombre)
{
	const xml_nodo *h;
	int hondo = 0;

	if (!n)
		return NULL;
	h = n->hijo;
	while (h) {
		if (h->nombre && xml_es(h, nombre))
			return (xml_nodo *)h;
		if (h->hijo && hondo < 40) {
			h = h->hijo;
			hondo++;
			continue;
		}
		while (h && !h->sig) {
			h = h->padre;
			hondo--;
			if (!h || h == n)
				return NULL;
		}
		if (!h)
			return NULL;
		h = h->sig;
	}
	return NULL;
}

static parrafo *destino(lector_docx *c)
{
	documento *d = c->d;

	switch (c->destino) {
	case D_NOTA: return c->nota >= 0 && (u32)c->nota < d->n_notas ? &d->notas[c->nota] : NULL;
	case D_ENC: return &d->enc;
	case D_PIE: return &d->pie;
	}
	return d->n_par ? &d->par[d->n_par - 1] : NULL;
}

static void letras(lector_docx *c, const u32 *t, u32 n, u16 f)
{
	parrafo *p = destino(c);

	if (p)
		fx_letras(c->d, p, t, n, f);
}

static const char *rel_de(lector_docx *c, const char *id)
{
	u32 i;

	if (!id)
		return NULL;
	for (i = 0; i < c->n_rel; i++)
		if (iguales(c->rel[i].id, id))
			return c->rel[i].destino;
	return NULL;
}

/* lo de dentro del ZIP al que apunta una relación de word/ */
static u8 *saca_rel(lector_docx *c, const char *id, u32 *n)
{
	const char *t = rel_de(c, id);
	char r[200];

	*n = 0;
	if (!t || busca(t, "://"))
		return NULL;
	if (t[0] == '/')
		copia(r, t + 1, sizeof r);
	else if (empieza(t, "../"))
		copia(r, t + 3, sizeof r);
	else
		fmt(r, sizeof r, "word/%s", t);
	return zip_saca(c->z, r, n);
}

static void lee_rels(lector_docx *c, const char *ruta)
{
	u32 n;
	u8 *d = zip_saca(c->z, ruta, &n);
	xml_doc *x;
	xml_nodo *h;

	c->n_rel = 0;
	if (!d)
		return;
	x = xml_lee((char *)d, n);
	so_suelta(d);
	if (!x)
		return;
	for (h = xml_hijo(xml_raiz(x), "Relationship"); h; h = xml_sig(h, "Relationship")) {
		const char *id = xml_atr(h, "Id"), *t = xml_atr(h, "Target");
		drel *r;

		if (!id || !t || !(c->rel = crece(c->rel, &c->cabe_rel, c->n_rel + 1, sizeof(drel))))
			continue;
		r = &c->rel[c->n_rel++];
		copia(r->id, id, sizeof r->id);
		copia(r->destino, t, sizeof r->destino);
	}
	xml_suelta(x);
}

/* ---- las propiedades ---- */
static u32 color_resalte(const char *v)
{
	static const struct { const char *n; u32 c; } t[] = {
		{ "yellow", 0xFFFF00 }, { "green", 0x00FF00 }, { "cyan", 0x00FFFF }, { "magenta", 0xFF00FF }, { "blue", 0x0000FF },
		{ "red", 0xFF0000 }, { "darkBlue", 0x000080 }, { "darkCyan", 0x008080 }, { "darkGreen", 0x008000 },
		{ "darkMagenta", 0x800080 }, { "darkRed", 0x800000 }, { "darkYellow", 0x808000 }, { "darkGray", 0x808080 },
		{ "lightGray", 0xC0C0C0 }, { "black", 0x000000 }, { "white", 0xFFFFFF },
	};
	u32 i;

	if (!v)
		return SIN_COLOR;
	for (i = 0; i < sizeof t / sizeof t[0]; i++)
		if (iguales(v, t[i].n))
			return t[i].c;
	return SIN_COLOR;
}

static void props_run(lector_docx *c, const xml_nodo *r, fx_props *p)
{
	xml_nodo *h;

	if (!r)
		return;
	for (h = xml_primero(r); h; h = xml_sig(h, NULL)) {
		if (xml_es(h, "w:rFonts")) {
			const char *f = xml_atr(h, "w:ascii");

			if (!f)
				f = xml_atr(h, "w:hAnsi");
			if (!f) {
				const char *th = xml_atr(h, "w:asciiTheme");

				if (th)
					f = busca(th, "major") ? c->letra_mayor : c->letra_menor;
			}
			if (f && f[0]) {
				copia(p->l.familia, f, sizeof p->l.familia);
				p->ml |= FL_FAM;
			}
		} else if (xml_es(h, "w:sz")) {
			long v = val(h) ? numero(val(h), NULL) : 0;

			if (v >= 2 && v <= 3200) {
				p->l.tam = (u16)v;
				p->ml |= FL_TAM;
			}
		} else if (xml_es(h, "w:b")) {
			p->l.negrita = (u8)vale(h);
			p->ml |= FL_NEG;
		} else if (xml_es(h, "w:i")) {
			p->l.cursiva = (u8)vale(h);
			p->ml |= FL_CUR;
		} else if (xml_es(h, "w:u")) {
			p->l.subrayado = (u8)vale(h);
			p->ml |= FL_SUB;
		} else if (xml_es(h, "w:strike") || xml_es(h, "w:dstrike")) {
			p->l.tachado = (u8)vale(h);
			p->ml |= FL_TACH;
		} else if (xml_es(h, "w:color")) {
			p->l.color = val(h) && !iguales(val(h), "auto") ? fx_color(val(h)) : SIN_COLOR;
			p->ml |= FL_COLOR;
		} else if (xml_es(h, "w:highlight")) {
			p->l.resalte = color_resalte(val(h));
			p->ml |= FL_RES;
		} else if (xml_es(h, "w:shd")) {
			const char *f = xml_atr(h, "w:fill");

			if (f && !iguales(f, "auto")) {
				p->l.resalte = fx_color(f);
				p->ml |= FL_RES;
			}
		} else if (xml_es(h, "w:vertAlign")) {
			const char *v = val(h);

			p->l.vert = v && iguales(v, "superscript") ? 1 : v && iguales(v, "subscript") ? 2 : 0;
			p->ml |= FL_VERT;
		} else if (xml_es(h, "w:smallCaps")) {
			p->l.versalitas = (u8)vale(h);
			p->ml |= FL_VERS;
		}
	}
}

static void props_par(const xml_nodo *pp, fx_props *p, int *num_id, int *num_nivel)
{
	xml_nodo *h;

	if (!pp)
		return;
	for (h = xml_primero(pp); h; h = xml_sig(h, NULL)) {
		const char *v;

		if (xml_es(h, "w:jc")) {
			v = val(h);
			p->p.alin = !v ? AL_IZQ : iguales(v, "center") ? AL_CENTRO : (iguales(v, "right") || iguales(v, "end")) ? AL_DER :
				    (iguales(v, "both") || iguales(v, "distribute") || iguales(v, "justify")) ? AL_JUST : AL_IZQ;
			p->mp |= FP_ALIN;
		} else if (xml_es(h, "w:ind")) {
			if ((v = xml_atr(h, "w:left")) || (v = xml_atr(h, "w:start"))) {
				p->p.sang_izq = (i32)numero(v, NULL);
				p->mp |= FP_SI;
			}
			if ((v = xml_atr(h, "w:right")) || (v = xml_atr(h, "w:end"))) {
				p->p.sang_der = (i32)numero(v, NULL);
				p->mp |= FP_SD;
			}
			if ((v = xml_atr(h, "w:hanging"))) {
				p->p.sang_primera = -(i32)numero(v, NULL);
				p->mp |= FP_SP;
			} else if ((v = xml_atr(h, "w:firstLine"))) {
				p->p.sang_primera = (i32)numero(v, NULL);
				p->mp |= FP_SP;
			}
		} else if (xml_es(h, "w:spacing")) {
			if ((v = xml_atr(h, "w:before"))) {
				p->p.antes = (i32)numero(v, NULL);
				p->mp |= FP_ANT;
			}
			if ((v = xml_atr(h, "w:after"))) {
				p->p.despues = (i32)numero(v, NULL);
				p->mp |= FP_DES;
			}
			if ((v = xml_atr(h, "w:line"))) {
				const char *r = xml_atr(h, "w:lineRule");
				long l = numero(v, NULL);

				if (!r || iguales(r, "auto")) {
					p->p.inter_tipo = INTER_MULTIPLE;
					p->p.inter = (i32)(l * 100 / 240);
				} else {
					p->p.inter_tipo = iguales(r, "exact") ? INTER_EXACTO : INTER_MINIMO;
					p->p.inter = (i32)l;
				}
				p->mp |= FP_INTER;
			}
		} else if (xml_es(h, "w:pageBreakBefore")) {
			p->p.salto_antes = (u8)vale(h);
			p->mp |= FP_SALTO;
		} else if (xml_es(h, "w:keepNext")) {
			p->p.juntos = (u8)vale(h);
			p->mp |= FP_JUNTOS;
		} else if (xml_es(h, "w:shd")) {
			const char *f = xml_atr(h, "w:fill");

			p->p.fondo = f && !iguales(f, "auto") ? fx_color(f) : SIN_COLOR;
			p->mp |= FP_FONDO;
		} else if (xml_es(h, "w:outlineLvl")) {
			long l = val(h) ? numero(val(h), NULL) : 9;

			p->p.nivel_titulo = (u8)(l >= 0 && l < 9 ? l + 1 : 0);
			p->mp |= FP_NIVTIT;
		} else if (xml_es(h, "w:numPr") && num_id) {
			xml_nodo *a = xml_hijo(h, "w:numId"), *b = xml_hijo(h, "w:ilvl");

			if (a && val(a))
				*num_id = (int)numero(val(a), NULL);
			if (b && val(b))
				*num_nivel = (int)numero(val(b), NULL);
		} else if (xml_es(h, "w:tabs")) {
			xml_nodo *t;
			u32 k = 0;

			for (t = xml_hijo(h, "w:tab"); t && k < MAX_TABS; t = xml_sig(t, "w:tab")) {
				const char *ty = val(t), *le = xml_atr(t, "w:leader");

				if (ty && iguales(ty, "clear"))
					continue;
				p->p.tabs[k].pos = (i32)numero(xml_atr(t, "w:pos") ? xml_atr(t, "w:pos") : "0", NULL);
				if (p->p.tabs[k].pos < 0)
					p->p.tabs[k].pos = 0;
				p->p.tabs[k].tipo = !ty ? TAB_IZQ : iguales(ty, "center") ? TAB_CENTRO : (iguales(ty, "right") || iguales(ty, "end")) ? TAB_DER :
						    iguales(ty, "decimal") ? TAB_DECIMAL : TAB_IZQ;
				p->p.tabs[k].relleno = !le ? 0 : iguales(le, "dot") ? '.' : iguales(le, "hyphen") ? '-' : (iguales(le, "underscore") || iguales(le, "heavy")) ? '_' : 0;
				k++;
			}
			p->p.n_tabs = (u8)k;
			p->mp |= FP_TABS;
		}
	}
	if ((p->mp & FP_SI) && p->p.sang_izq < -31680)
		p->p.sang_izq = 0;
	if (p->p.sang_izq > 31680)
		p->p.sang_izq = 31680;
	if (p->p.sang_der > 31680 || p->p.sang_der < -31680)
		p->p.sang_der = 0;
	if (p->p.sang_primera > 31680 || p->p.sang_primera < -31680)
		p->p.sang_primera = 0;
	if (p->p.antes < 0 || p->p.antes > 31680)
		p->p.antes = 0;
	if (p->p.despues < 0 || p->p.despues > 31680)
		p->p.despues = 0;
	if (p->p.inter < 0 || p->p.inter > 31680)
		p->p.inter = 100;
}

/* ---- los estilos, las listas y el tema ---- */
static void lee_tema(lector_docx *c)
{
	u32 n;
	u8 *d = zip_saca(c->z, "word/theme/theme1.xml", &n);
	xml_doc *x;
	xml_nodo *fs, *h;

	copia(c->letra_menor, "Calibri", sizeof c->letra_menor);
	copia(c->letra_mayor, "Calibri Light", sizeof c->letra_mayor);
	if (!d)
		return;
	x = xml_lee((char *)d, n);
	so_suelta(d);
	if (!x)
		return;
	fs = dentro(xml_raiz(x), "fontScheme");
	if ((h = xml_camino(fs, "minorFont/latin")) && xml_atr(h, "typeface") && xml_atr(h, "typeface")[0])
		copia(c->letra_menor, xml_atr(h, "typeface"), sizeof c->letra_menor);
	if ((h = xml_camino(fs, "majorFont/latin")) && xml_atr(h, "typeface") && xml_atr(h, "typeface")[0])
		copia(c->letra_mayor, xml_atr(h, "typeface"), sizeof c->letra_mayor);
	xml_suelta(x);
}

static void lee_estilos(lector_docx *c)
{
	u32 n;
	u8 *d = zip_saca(c->z, "word/styles.xml", &n);
	xml_doc *x;
	xml_nodo *r, *h;

	if (!d)
		return;
	x = xml_lee((char *)d, n);
	so_suelta(d);
	if (!x)
		return;
	r = xml_raiz(x);
	if ((h = xml_hijo(r, "w:docDefaults"))) {
		props_run(c, xml_camino(h, "w:rPrDefault/w:rPr"), &c->def);
		props_par(xml_camino(h, "w:pPrDefault/w:pPr"), &c->def, NULL, NULL);
	}
	for (h = xml_hijo(r, "w:style"); h; h = xml_sig(h, "w:style")) {
		const char *ty = xml_atr(h, "w:type"), *v;
		dstilo *s;
		xml_nodo *q;

		if (!ty || !(c->est = crece(c->est, &c->cabe_est, c->n_est + 1, sizeof(dstilo))))
			continue;
		s = &c->est[c->n_est++];
		memset(s, 0, sizeof *s);
		s->est_doc = -1;
		s->tipo = iguales(ty, "paragraph") ? 1 : iguales(ty, "character") ? 2 : iguales(ty, "table") ? 3 : 0;
		s->defecto = fx_cierto(xml_atr(h, "w:default"), 0);
		s->pr.l = fmt_letra_normal();
		s->pr.p = fmt_parrafo_normal();
		copia(s->id, (v = xml_atr(h, "w:styleId")) ? v : "", sizeof s->id);
		copia(s->nombre, (q = xml_hijo(h, "w:name")) && val(q) ? val(q) : s->id, sizeof s->nombre);
		copia(s->basado, (q = xml_hijo(h, "w:basedOn")) && val(q) ? val(q) : "", sizeof s->basado);
		s->num_id = -1;
		props_par(xml_hijo(h, "w:pPr"), &s->pr, &s->num_id, &s->num_nivel);
		props_run(c, xml_hijo(h, "w:rPr"), &s->pr);
		if (s->tipo == 3) {
			xml_nodo *b = xml_camino(h, "w:tblPr/w:tblBorders");

			if (b) {
				xml_nodo *k;

				for (k = xml_primero(b); k; k = xml_sig(k, NULL))
					if (val(k) && !iguales(val(k), "nil") && !iguales(val(k), "none"))
						s->bordes = 1;
			}
		}
	}
	xml_suelta(x);
}

static dstilo *busca_estilo(lector_docx *c, const char *id, int tipo)
{
	u32 i;

	if (!id || !id[0])
		return NULL;
	for (i = 0; i < c->n_est; i++)
		if (c->est[i].tipo == tipo && iguales(c->est[i].id, id))
			return &c->est[i];
	return NULL;
}

static dstilo *estilo_defecto(lector_docx *c, int tipo)
{
	u32 i;

	for (i = 0; i < c->n_est; i++)
		if (c->est[i].tipo == tipo && c->est[i].defecto)
			return &c->est[i];
	return NULL;
}

/* lo que dice el estilo con sus padres (sin lo de debajo de todo) */
static void cadena(lector_docx *c, dstilo *s, fx_props *r, int hondo)
{
	if (!s || hondo > 24 || s->resolviendo)
		return;
	s->resolviendo = 1;
	cadena(c, busca_estilo(c, s->basado, s->tipo), r, hondo + 1);
	s->resolviendo = 0;
	fx_junta(r, &s->pr);
}

static void lista_de_estilo(lector_docx *c, dstilo *s, int *num_id, int *nivel, int hondo)
{
	if (!s || hondo > 24)
		return;
	if (s->num_id >= 0) {
		*num_id = s->num_id;
		*nivel = s->num_nivel;
		return;
	}
	lista_de_estilo(c, busca_estilo(c, s->basado, s->tipo), num_id, nivel, hondo + 1);
}

static int estilo_doc(lector_docx *c, dstilo *s)
{
	documento *d = c->d;
	fx_props ef = c->def;
	estilo e;
	int k;

	if (!s)
		return EST_NORMAL;
	if (s->est_doc >= 0)
		return s->est_doc;
	cadena(c, s, &ef, 0);
	k = fx_estilo_serie(s->nombre);
	if (k < 0)
		k = fx_estilo_serie(s->id);
	if (k < 0)
		k = doc_estilo(d, s->nombre);
	memset(&e, 0, sizeof e);
	e.letra = fmt_letra_normal();
	e.p = fmt_parrafo_normal();
	fx_aplica(&ef, &e.letra, &e.p);
	if (k >= 0) {
		d->est[k].letra = e.letra;
		e.p.estilo = (u16)k;
		e.p.nivel_titulo = (ef.mp & FP_NIVTIT) ? ef.p.nivel_titulo : d->est[k].p.nivel_titulo;
		d->est[k].p = e.p;
	} else {
		copia(e.nombre, s->nombre, sizeof e.nombre);
		k = doc_nuevo_estilo(d, &e);
		d->est[k].p.estilo = (u16)k;
		d->est[k].siguiente = (u16)k;
	}
	s->est_doc = k;
	return k;
}

static void lee_listas(lector_docx *c)
{
	u32 n;
	u8 *d = zip_saca(c->z, "word/numbering.xml", &n);
	xml_doc *x;
	xml_nodo *r, *h;

	if (!d)
		return;
	x = xml_lee((char *)d, n);
	so_suelta(d);
	if (!x)
		return;
	r = xml_raiz(x);
	for (h = xml_hijo(r, "w:abstractNum"); h; h = xml_sig(h, "w:abstractNum")) {
		dabs *a;
		xml_nodo *l;
		int k;

		if (!(c->abs = crece(c->abs, &c->cabe_abs, c->n_abs + 1, sizeof(dabs))))
			break;
		a = &c->abs[c->n_abs++];
		memset(a, 0, sizeof *a);
		a->id = (int)numero(xml_atr(h, "w:abstractNumId") ? xml_atr(h, "w:abstractNumId") : "-1", NULL);
		for (k = 0; k < 9; k++) {
			a->tipo[k] = LISTA_VINETAS;
			a->sang[k] = 720 + 360 * k;
			a->primera[k] = -360;
		}
		for (l = xml_hijo(h, "w:lvl"); l; l = xml_sig(l, "w:lvl")) {
			long il = numero(xml_atr(l, "w:ilvl") ? xml_atr(l, "w:ilvl") : "0", NULL);
			xml_nodo *q;
			fx_props pp;

			if (il < 0 || il > 8)
				continue;
			if ((q = xml_hijo(l, "w:numFmt"))) {
				const char *f = val(q);

				a->tipo[il] = !f || iguales(f, "bullet") || iguales(f, "none") ? LISTA_VINETAS : (iguales(f, "lowerLetter") || iguales(f, "upperLetter")) ? LISTA_LETRAS :
					      (iguales(f, "lowerRoman") || iguales(f, "upperRoman")) ? LISTA_ROMANOS : LISTA_NUMEROS;
			}
			if ((q = xml_hijo(l, "w:start")) && val(q))
				a->inicio[il] = (u32)numero(val(q), NULL) % 100000;
			memset(&pp, 0, sizeof pp);
			props_par(xml_hijo(l, "w:pPr"), &pp, NULL, NULL);
			if (pp.mp & FP_SI) {
				a->sang[il] = pp.p.sang_izq;
				a->primera[il] = (pp.mp & FP_SP) ? pp.p.sang_primera : 0;
				a->hay_sang[il] = 1;
			}
		}
	}
	for (h = xml_hijo(r, "w:num"); h; h = xml_sig(h, "w:num")) {
		dnum *m;
		xml_nodo *a = xml_hijo(h, "w:abstractNumId"), *o;

		if (!(c->num = crece(c->num, &c->cabe_num, c->n_num + 1, sizeof(dnum))))
			break;
		m = &c->num[c->n_num++];
		memset(m, 0, sizeof *m);
		m->id = (int)numero(xml_atr(h, "w:numId") ? xml_atr(h, "w:numId") : "-1", NULL);
		m->abs = a && val(a) ? (int)numero(val(a), NULL) : -1;
		for (o = xml_hijo(h, "w:lvlOverride"); o; o = xml_sig(o, "w:lvlOverride")) {
			long il = numero(xml_atr(o, "w:ilvl") ? xml_atr(o, "w:ilvl") : "0", NULL);
			xml_nodo *s = xml_hijo(o, "w:startOverride");

			if (il >= 0 && il < 9 && s && val(s))
				m->inicio[il] = (u32)numero(val(s), NULL) % 100000;
		}
	}
	xml_suelta(x);
}

static dabs *abs_de(lector_docx *c, int num_id, dnum **m)
{
	u32 i, k;

	*m = NULL;
	for (i = 0; i < c->n_num; i++)
		if (c->num[i].id == num_id) {
			*m = &c->num[i];
			for (k = 0; k < c->n_abs; k++)
				if (c->abs[k].id == c->num[i].abs)
					return &c->abs[k];
			return NULL;
		}
	return NULL;
}

/* ---- el texto ---- */
static void contenido(lector_docx *c, const xml_nodo *n, const fmt_letra *fl_par, const fx_props *ef_par);

static void pon_imagen(lector_docx *c, const xml_nodo *dr, const fmt_letra *fl)
{
	xml_nodo *cont = xml_hijo(dr, "wp:inline"), *ext, *blip;
	int flota = 0, k;
	u8 *datos;
	u32 n, ch;

	if (!cont) {
		cont = xml_hijo(dr, "wp:anchor");
		flota = 1;
	}
	if (!cont)
		return;
	blip = dentro(cont, "a:blip");
	if (!blip || c->imagenes > 2000)
		return;
	datos = saca_rel(c, xml_atr(blip, "r:embed"), &n);
	if (!datos)
		return;
	k = doc_nueva_imagen(c->d, datos, n);
	if (k < 0) {
		so_suelta(datos);
		return;
	}
	c->imagenes++;
	if ((ext = xml_hijo(cont, "wp:extent"))) {
		i64 cx = numero(xml_atr(ext, "cx") ? xml_atr(ext, "cx") : "0", NULL), cy = numero(xml_atr(ext, "cy") ? xml_atr(ext, "cy") : "0", NULL);

		/* (numero() se para en 10^8: hasta unos 16 cm; los más grandes se leen a mano) */
		{
			const char *s;
			i64 v = 0;

			for (s = xml_atr(ext, "cx"); s && *s >= '0' && *s <= '9' && v < 100000000000LL; s++)
				v = v * 10 + (*s - '0');
			cx = v;
			v = 0;
			for (s = xml_atr(ext, "cy"); s && *s >= '0' && *s <= '9' && v < 100000000000LL; s++)
				v = v * 10 + (*s - '0');
			cy = v;
		}
		if (cx > 20 * EMU_TW && cy > 20 * EMU_TW) {
			c->d->img[k].ancho = (i32)(cx / EMU_TW > 31680 ? 31680 : cx / EMU_TW);
			c->d->img[k].alto = (i32)(cy / EMU_TW > 31680 ? 31680 : cy / EMU_TW);
		}
	}
	if (flota && (xml_hijo(cont, "wp:wrapSquare") || xml_hijo(cont, "wp:wrapTight") || xml_hijo(cont, "wp:wrapThrough"))) {
		xml_nodo *al = xml_camino(cont, "wp:positionH/wp:align");
		bufer b = { 0 };

		xml_texto(al, &b);
		buf_c(&b, 0);
		c->d->img[k].ajuste = !b.mal && b.d && iguales((char *)b.d, "right") ? AJUSTE_DER : AJUSTE_IZQ;
		buf_suelta(&b);
	}
	ch = IMAGEN_0 + (u32)k;
	letras(c, &ch, 1, doc_fmt(c->d, fl));
}

static void pon_nota(lector_docx *c, const char *id, const fmt_letra *fl)
{
	xml_nodo *h, *nota = NULL;
	int k, dest, nt, primero = 1;
	u32 ch;
	fx_props ef;

	if (c->destino != D_CUERPO || !c->notas || !id)
		return;
	for (h = xml_hijo(xml_raiz(c->notas), "w:footnote"); h; h = xml_sig(h, "w:footnote"))
		if (xml_atr(h, "w:id") && iguales(xml_atr(h, "w:id"), id) && !xml_atr(h, "w:type"))
			nota = h;
	if (!nota)
		return;
	k = doc_nueva_nota(c->d);
	if (k < 0)
		return;
	ch = NOTA_0 + (u32)k;
	letras(c, &ch, 1, doc_fmt(c->d, fl));
	dest = c->destino;
	nt = c->nota;
	c->destino = D_NOTA;
	c->nota = k;
	memset(&ef, 0, sizeof ef);
	for (h = xml_hijo(nota, "w:p"); h; h = xml_sig(h, "w:p")) {
		fmt_letra f = c->d->est[EST_NOTA].letra;

		if (!primero) {
			u32 s = SALTO_LINEA;

			letras(c, &s, 1, doc_fmt(c->d, &f));
		}
		primero = 0;
		contenido(c, h, &f, &ef);
	}
	{
		parrafo *p = &c->d->notas[k];
		u32 q = 0;

		while (q < p->n && p->t[q] == ' ')
			q++;
		par_quita(p, 0, q);
	}
	c->destino = dest;
	c->nota = nt;
}

/* el formato de letra de una w:r */
static void letra_run(lector_docx *c, const xml_nodo *r, const fmt_letra *fl_par, fmt_letra *fl)
{
	xml_nodo *rp = xml_hijo(r, "w:rPr"), *rs;
	fx_props pr;

	*fl = *fl_par;
	if (!rp)
		return;
	memset(&pr, 0, sizeof pr);
	if ((rs = xml_hijo(rp, "w:rStyle")))
		cadena(c, busca_estilo(c, val(rs), 2), &pr, 0);
	props_run(c, rp, &pr);
	fx_aplica(&pr, fl, NULL);
}

/* un salto de página dentro del párrafo: lo de después, en un párrafo nuevo */
static void parte_por_salto(lector_docx *c)
{
	parrafo *p, *q;

	if (c->destino != D_CUERPO || !c->d->n_par || c->d->par[c->d->n_par - 1].p.tabla)
		return;
	p = &c->d->par[c->d->n_par - 1];
	{
		fmt_parrafo fp = p->p;

		q = arch_parrafo(c->d);
		if (!q)
			return;
		q->p = fp;
		q->p.salto_antes = 1;
	}
}

static void run(lector_docx *c, const xml_nodo *r, const fmt_letra *fl_par)
{
	fmt_letra fl;
	xml_nodo *h;
	u16 f;

	letra_run(c, r, fl_par, &fl);
	f = doc_fmt(c->d, &fl);
	for (h = xml_primero(r); h && !c->d->sin_memoria; h = xml_sig(h, NULL)) {
		if (xml_es(h, "w:fldChar")) {
			const char *t = xml_atr(h, "w:fldCharType");

			if (!t)
				continue;
			if (iguales(t, "begin")) {
				c->campo = 1;
				c->orden[0] = 0;
			} else if (iguales(t, "separate") && c->campo == 1) {
				const char *o = c->orden;

				while (*o == ' ')
					o++;
				if (empieza(o, "PAGE") && (o[4] == ' ' || !o[4] || o[4] == '\\')) {
					u32 ch = CAMPO_PAGINA;

					letras(c, &ch, 1, f);
					c->campo = 2;
				} else if (empieza(o, "NUMPAGES") || empieza(o, "SECTIONPAGES")) {
					u32 ch = CAMPO_PAGINAS;

					letras(c, &ch, 1, f);
					c->campo = 2;
				} else
					c->campo = 3;
			} else if (iguales(t, "end")) {
				if (c->campo == 1) {
					/* (sin resultado: si es nuestro, se pone) */
					const char *o = c->orden;

					while (*o == ' ')
						o++;
					if (empieza(o, "PAGE") && (o[4] == ' ' || !o[4] || o[4] == '\\')) {
						u32 ch = CAMPO_PAGINA;

						letras(c, &ch, 1, f);
					} else if (empieza(o, "NUMPAGES")) {
						u32 ch = CAMPO_PAGINAS;

						letras(c, &ch, 1, f);
					}
				}
				c->campo = 0;
			}
			continue;
		}
		if (xml_es(h, "w:instrText")) {
			if (c->campo == 1) {
				bufer b = { 0 };

				xml_texto(h, &b);
				buf_c(&b, 0);
				if (!b.mal)
					anade(c->orden, (char *)b.d, sizeof c->orden);
				buf_suelta(&b);
			}
			continue;
		}
		if (c->campo == 1 || c->campo == 2)
			continue;	/* (la orden, o el resultado de un campo que ya se ha puesto) */
		if (xml_es(h, "w:t")) {
			const xml_nodo *t;

			for (t = h->hijo; t; t = t->sig) {
				u32 i = 0, k = 0, ch, b[256];

				if (t->nombre)
					continue;
				while (i < t->largo) {
					i += (u32)utf8_lee((const u8 *)t->texto + i, t->largo - i, &ch);
					if (ch == '\n' || ch == '\r')
						ch = ' ';
					if (ch < 0x20 && ch != '\t')
						continue;
					b[k++] = ch;
					if (k == 256) {
						letras(c, b, k, f);
						k = 0;
					}
				}
				letras(c, b, k, f);
			}
		} else if (xml_es(h, "w:tab") || xml_es(h, "w:ptab")) {
			u32 ch = '\t';

			letras(c, &ch, 1, f);
		} else if (xml_es(h, "w:br")) {
			const char *t = xml_atr(h, "w:type");

			if (t && iguales(t, "page"))
				parte_por_salto(c);
			else {
				u32 ch = SALTO_LINEA;

				letras(c, &ch, 1, f);
			}
		} else if (xml_es(h, "w:cr")) {
			u32 ch = SALTO_LINEA;

			letras(c, &ch, 1, f);
		} else if (xml_es(h, "w:noBreakHyphen")) {
			u32 ch = '-';

			letras(c, &ch, 1, f);
		} else if (xml_es(h, "w:sym")) {
			const char *s = xml_atr(h, "w:char");
			u32 v = 0;

			while (s && *s) {
				char x = *s++;
				u32 dg = x >= '0' && x <= '9' ? (u32)(x - '0') : x >= 'a' && x <= 'f' ? (u32)(x - 'a' + 10) : x >= 'A' && x <= 'F' ? (u32)(x - 'A' + 10) : 0;

				v = (v << 4 | dg) & 0xFFFFF;
			}
			if (v >= 0xF000 && v <= 0xF0FF)
				v -= 0xF000;	/* (los de los tipos de símbolos) */
			if (v >= 0x20 && v < 0xD800)
				letras(c, &v, 1, f);
		} else if (xml_es(h, "w:drawing")) {
			pon_imagen(c, h, &fl);
		} else if (xml_es(h, "w:footnoteReference")) {
			fmt_letra fn = fl;

			fn.vert = 0;	/* (la llamada ya se pinta arriba) */
			pon_nota(c, xml_atr(h, "w:id"), &fn);
		} else if (xml_es(h, "mc:AlternateContent")) {
			xml_nodo *ch = xml_hijo(h, "mc:Choice");
			xml_nodo *dr = ch ? xml_hijo(ch, "w:drawing") : NULL;

			if (dr)
				pon_imagen(c, dr, &fl);
		}
	}
}

static void contenido(lector_docx *c, const xml_nodo *n, const fmt_letra *fl_par, const fx_props *ef_par)
{
	xml_nodo *h;

	if (++c->hondo > 64) {
		c->hondo--;
		return;
	}
	for (h = xml_primero(n); h && !c->d->sin_memoria; h = xml_sig(h, NULL)) {
		if (xml_es(h, "w:r"))
			run(c, h, fl_par);
		else if (xml_es(h, "w:fldSimple")) {
			const char *o = xml_atr(h, "w:instr");

			while (o && *o == ' ')
				o++;
			if (o && empieza(o, "PAGE") && (o[4] == ' ' || !o[4] || o[4] == '\\')) {
				u32 ch = CAMPO_PAGINA;

				letras(c, &ch, 1, doc_fmt(c->d, fl_par));
			} else if (o && (empieza(o, "NUMPAGES") || empieza(o, "SECTIONPAGES"))) {
				u32 ch = CAMPO_PAGINAS;

				letras(c, &ch, 1, doc_fmt(c->d, fl_par));
			} else
				contenido(c, h, fl_par, ef_par);
		} else if (xml_es(h, "w:del") || xml_es(h, "w:moveFrom") || xml_es(h, "w:pPr") || xml_es(h, "w:proofErr") ||
			   xml_es(h, "w:bookmarkStart") || xml_es(h, "w:bookmarkEnd") || xml_es(h, "w:commentRangeStart") || xml_es(h, "w:commentRangeEnd"))
			continue;
		else if (xml_es(h, "w:sdtPr") || xml_es(h, "w:sdtEndPr"))
			continue;
		else
			contenido(c, h, fl_par, ef_par);	/* w:hyperlink, w:ins, w:smartTag, w:sdt, w:sdtContent... */
	}
	c->hondo--;
}

/* el formato de un párrafo (su estilo y lo suyo); da su lista */
static void formato_par(lector_docx *c, const xml_nodo *n, fmt_parrafo *fp, fmt_letra *fl, int *num_id, int *nivel)
{
	xml_nodo *pp = xml_hijo(n, "w:pPr"), *ps = pp ? xml_hijo(pp, "w:pStyle") : NULL, *rp;
	dstilo *s = ps ? busca_estilo(c, val(ps), 1) : NULL;
	fx_props ef = c->def, propio;

	if (!s)
		s = estilo_defecto(c, 1);
	cadena(c, s, &ef, 0);
	*num_id = -1;
	*nivel = 0;
	lista_de_estilo(c, s, num_id, nivel, 0);
	memset(&propio, 0, sizeof propio);
	props_par(pp, &propio, num_id, nivel);
	fx_junta(&ef, &propio);
	/* (la marca del párrafo: su formato de letra vale para un párrafo vacío) */
	*fp = fmt_parrafo_normal();
	*fl = fmt_letra_normal();
	fx_aplica(&ef, fl, fp);
	fp->estilo = (u16)estilo_doc(c, s);
	rp = pp ? xml_hijo(pp, "w:rPr") : NULL;
	(void)rp;
}

static void parrafo_docx(lector_docx *c, const xml_nodo *n)
{
	fmt_parrafo fp;
	fmt_letra fl;
	int num_id, nivel;
	parrafo *p;
	fx_props vacio;
	xml_nodo *pp = xml_hijo(n, "w:pPr");

	formato_par(c, n, &fp, &fl, &num_id, &nivel);
	if (num_id > 0) {
		dnum *m;
		dabs *a = abs_de(c, num_id, &m);

		if (nivel < 0 || nivel > 8)
			nivel = 0;
		fp.lista = a ? a->tipo[nivel] : LISTA_NUMEROS;
		fp.nivel = (u8)nivel;
		fp.lista_id = (u16)num_id;
		if (m && !m->usado) {
			u32 ini = m->inicio[nivel] ? m->inicio[nivel] : a ? a->inicio[nivel] : 0;

			if (ini > 1)
				fp.inicio_lista = ini;
			m->usado = 1;
		}
		if (a && a->hay_sang[nivel]) {
			xml_nodo *ind = pp ? xml_hijo(pp, "w:ind") : NULL;

			if (!ind) {
				fp.sang_izq = a->sang[nivel];
				fp.sang_primera = a->primera[nivel];
			}
		} else if (!(pp && xml_hijo(pp, "w:ind"))) {
			fp.sang_izq = 720 + 360 * nivel;
			fp.sang_primera = -360;
		}
	}
	if (c->salto_pendiente) {
		fp.salto_antes = 1;
		c->salto_pendiente = 0;
	}
	p = arch_parrafo(c->d);
	if (!p)
		return;
	p->p = fp;
	c->destino = D_CUERPO;
	c->campo = 0;
	memset(&vacio, 0, sizeof vacio);
	contenido(c, n, &fl, &vacio);
	/* un salto de sección en el párrafo: lo de después, en otra página */
	if (pp && xml_hijo(pp, "w:sectPr")) {
		xml_nodo *t = xml_camino(pp, "w:sectPr/w:type");

		if (!t || !val(t) || !iguales(val(t), "continuous"))
			c->salto_pendiente = 1;
	}
}

/* ---- las tablas ---- */
#define MAX_FILAS_DOCX 4000

typedef struct {
	const xml_nodo *tc;
	int cubierta;
	int cols, filas;
	u32 fondo;
} dcelda;

static void celda_docx(lector_docx *c, const xml_nodo *tc, int *primero)
{
	xml_nodo *h;

	if (c->hondo > 64)
		return;
	c->hondo++;
	for (h = xml_primero(tc); h; h = xml_sig(h, NULL)) {
		if (xml_es(h, "w:p")) {
			fmt_parrafo fp;
			fmt_letra fl;
			int num, niv;
			fx_props vacio;

			formato_par(c, h, &fp, &fl, &num, &niv);
			if (!*primero) {
				u32 s = SALTO_LINEA;

				letras(c, &s, 1, doc_fmt(c->d, &fl));
			} else {
				parrafo *p = destino(c);

				if (p) {
					fmt_parrafo v = p->p;

					p->p = fp;
					p->p.tabla = v.tabla;
					p->p.fila = v.fila;
					p->p.col = v.col;
					p->p.abarca_cols = v.abarca_cols;
					p->p.abarca_filas = v.abarca_filas;
					p->p.cubierta = 0;
					p->p.fondo = v.fondo;
					p->p.salto_antes = 0;
					p->p.lista = LISTA_NO;
				}
			}
			*primero = 0;
			memset(&vacio, 0, sizeof vacio);
			contenido(c, h, &fl, &vacio);
		} else if (xml_es(h, "w:tbl") || xml_es(h, "w:tr") || xml_es(h, "w:tc") || xml_es(h, "w:sdt") || xml_es(h, "w:sdtContent") || xml_es(h, "w:customXml"))
			celda_docx(c, h, primero);
	}
	c->hondo--;
}

static void filas_docx(const xml_nodo *n, const xml_nodo **f, u32 *nf, int hondo)
{
	xml_nodo *h;

	for (h = xml_primero(n); h && *nf < MAX_FILAS_DOCX; h = xml_sig(h, NULL)) {
		if (xml_es(h, "w:tr"))
			f[(*nf)++] = h;
		else if ((xml_es(h, "w:sdt") || xml_es(h, "w:sdtContent") || xml_es(h, "w:customXml")) && hondo < 8)
			filas_docx(h, f, nf, hondo + 1);
	}
}

static void celdas_de(const xml_nodo *tr, const xml_nodo **t, u32 *n, int hondo)
{
	xml_nodo *h;

	for (h = xml_primero(tr); h && *n < 256; h = xml_sig(h, NULL)) {
		if (xml_es(h, "w:tc"))
			t[(*n)++] = h;
		else if ((xml_es(h, "w:sdt") || xml_es(h, "w:sdtContent") || xml_es(h, "w:customXml")) && hondo < 8)
			celdas_de(h, t, n, hondo + 1);
	}
}

static void tabla_docx(lector_docx *c, const xml_nodo *n)
{
	documento *d = c->d;
	const xml_nodo **filas;
	u32 nf = 0, nc = 0, f, k;
	i32 ancho[MAX_COLS];
	dcelda *rej;
	int t, bordes = 0;
	xml_nodo *g, *tp = xml_hijo(n, "w:tblPr");

	filas = so_pide(MAX_FILAS_DOCX * sizeof *filas);
	if (!filas) {
		d->sin_memoria = 1;
		return;
	}
	for (g = xml_hijo(xml_hijo(n, "w:tblGrid"), "w:gridCol"); g && nc < MAX_COLS; g = xml_sig(g, "w:gridCol")) {
		const char *v = xml_atr(g, "w:w");

		ancho[nc++] = v ? (i32)numero(v, NULL) : 0;
	}
	filas_docx(n, filas, &nf, 0);
	/* (las columnas que de verdad hay: por si la rejilla miente) */
	for (f = 0; f < nf; f++) {
		const xml_nodo *tcs[256];
		u32 m = 0, j, cols = 0;
		xml_nodo *tr = xml_hijo(filas[f], "w:trPr");

		celdas_de(filas[f], tcs, &m, 0);
		if (tr && xml_hijo(tr, "w:gridBefore") && val(xml_hijo(tr, "w:gridBefore")))
			cols += (u32)numero(val(xml_hijo(tr, "w:gridBefore")), NULL) % 64;
		for (j = 0; j < m; j++) {
			xml_nodo *gs = xml_camino(tcs[j], "w:tcPr/w:gridSpan");
			long s = gs && val(gs) ? numero(val(gs), NULL) : 1;

			cols += s < 1 ? 1 : s > 63 ? 63 : (u32)s;
		}
		if (cols > nc)
			nc = cols > MAX_COLS ? MAX_COLS : cols;
	}
	for (k = 0; k < nc; k++)
		if (k >= MAX_COLS)
			break;
	if (!nc || !nf || (u64)nf * nc > 200000) {
		so_suelta(filas);
		return;
	}
	rej = so_pide((size_t)nf * nc * sizeof(dcelda));
	if (!rej) {
		so_suelta(filas);
		d->sin_memoria = 1;
		return;
	}
	memset(rej, 0, (size_t)nf * nc * sizeof(dcelda));
	for (f = 0; f < nf * nc; f++)
		rej[f].fondo = SIN_COLOR;
	for (f = 0; f < nf; f++) {
		const xml_nodo *tcs[256];
		u32 m = 0, j, col = 0;
		xml_nodo *tr = xml_hijo(filas[f], "w:trPr");

		celdas_de(filas[f], tcs, &m, 0);
		if (tr && xml_hijo(tr, "w:gridBefore") && val(xml_hijo(tr, "w:gridBefore")))
			col += (u32)numero(val(xml_hijo(tr, "w:gridBefore")), NULL) % 64;
		for (j = 0; j < m && col < nc; j++) {
			xml_nodo *pr = xml_hijo(tcs[j], "w:tcPr"), *gs = xml_hijo(pr, "w:gridSpan"), *vm = xml_hijo(pr, "w:vMerge"), *sh = xml_hijo(pr, "w:shd");
			long s = gs && val(gs) ? numero(val(gs), NULL) : 1;
			dcelda *o = &rej[f * nc + col];
			u32 q;

			if (s < 1)
				s = 1;
			if (col + (u32)s > nc)
				s = (long)(nc - col);
			if (vm && (!val(vm) || !iguales(val(vm), "restart")) && f > 0) {
				/* sigue la de arriba: se busca la que manda en esta columna */
				u32 a = f;

				while (a > 0 && rej[(a - 1) * nc + col].cubierta && !rej[(a - 1) * nc + col].tc)
					a--;
				if (a > 0 && rej[(a - 1) * nc + col].tc) {
					dcelda *arriba = &rej[(a - 1) * nc + col];

					arriba->filas = (int)(f - (a - 1) + 1);
					for (q = 0; q < (u32)s; q++)
						rej[f * nc + col + q].cubierta = 1;
					col += (u32)s;
					continue;
				}
			}
			o->tc = tcs[j];
			o->cols = (int)s;
			o->filas = 1;
			if (sh && xml_atr(sh, "w:fill") && !iguales(xml_atr(sh, "w:fill"), "auto"))
				o->fondo = fx_color(xml_atr(sh, "w:fill"));
			if (xml_hijo(pr, "w:tcBorders")) {
				xml_nodo *b;

				for (b = xml_primero(xml_hijo(pr, "w:tcBorders")); b; b = xml_sig(b, NULL))
					if (val(b) && !iguales(val(b), "nil") && !iguales(val(b), "none"))
						bordes = 1;
			}
			for (q = 1; q < (u32)s; q++)
				rej[f * nc + col + q].cubierta = 1;
			col += (u32)s;
		}
	}
	/* las uniones hacia abajo: tapar lo de debajo (de todas las columnas que abarca) */
	for (f = 0; f < nf; f++)
		for (k = 0; k < nc; k++) {
			dcelda *o = &rej[f * nc + k];
			u32 a, b;

			if (!o->tc || o->filas <= 1)
				continue;
			for (a = f + 1; a < f + (u32)o->filas && a < nf; a++)
				for (b = k; b < k + (u32)o->cols && b < nc; b++) {
					rej[a * nc + b].cubierta = 1;
					rej[a * nc + b].tc = NULL;
				}
		}
	t = doc_nueva_tabla(d, (u16)nf, (u16)nc, doc_ancho_texto(d));
	if (t < 0) {
		so_suelta(rej);
		so_suelta(filas);
		return;
	}
	{
		i32 tot = 0;

		for (k = 0; k < nc; k++)
			tot += k < MAX_COLS && ancho[k] > 0 ? 0 : 1;
		for (k = 0; k < nc; k++)
			d->tab[t].ancho[k] = ancho[k] > 100 && ancho[k] < 31680 && k < MAX_COLS ? ancho[k] : doc_ancho_texto(d) / (i32)nc;
		(void)tot;
	}
	if (tp) {
		xml_nodo *b = xml_hijo(tp, "w:tblBorders"), *jc = xml_hijo(tp, "w:jc"), *ts = xml_hijo(tp, "w:tblStyle");

		if (b) {
			xml_nodo *q;

			for (q = xml_primero(b); q; q = xml_sig(q, NULL))
				if (val(q) && !iguales(val(q), "nil") && !iguales(val(q), "none"))
					bordes = 1;
		} else if (ts) {
			dstilo *s = busca_estilo(c, val(ts), 3);

			if ((s && s->bordes) || (val(ts) && (busca(val(ts), "Grid") || busca(val(ts), "Cuadr"))))
				bordes = 1;
		}
		if (jc && val(jc))
			d->tab[t].alin = (u8)(iguales(val(jc), "center") ? AL_CENTRO : (iguales(val(jc), "right") || iguales(val(jc), "end")) ? AL_DER : AL_IZQ);
	}
	d->tab[t].bordes = bordes ? BORDE_TODOS : BORDE_NO;
	for (f = 0; f < nf && !d->sin_memoria; f++)
		for (k = 0; k < nc; k++) {
			dcelda *o = &rej[f * nc + k];
			parrafo *p = arch_parrafo(d);
			int primero = 1;

			if (!p)
				break;
			p->p = d->est[EST_SIN_ESPACIO].p;
			p->p.estilo = EST_SIN_ESPACIO;
			p->p.tabla = (u16)(t + 1);
			p->p.fila = (u16)f;
			p->p.col = (u16)k;
			if (o->cubierta) {
				p->p.cubierta = 1;
				continue;
			}
			p->p.fondo = o->fondo;
			if (o->cols > 1)
				p->p.abarca_cols = (u8)o->cols;
			if (o->filas > 1)
				p->p.abarca_filas = (u8)(o->filas > 255 ? 255 : o->filas);
			if (o->tc) {
				c->destino = D_CUERPO;
				celda_docx(c, o->tc, &primero);
			}
		}
	if (c->salto_pendiente && t >= 0) {
		u32 i0 = d->n_par - nf * nc;

		if (i0 < d->n_par)
			d->par[i0].p.salto_antes = 1;
		c->salto_pendiente = 0;
	}
	so_suelta(rej);
	so_suelta(filas);
}

static void bloque(lector_docx *c, const xml_nodo *n)
{
	xml_nodo *h;

	if (c->d->sin_memoria)
		return;
	if (xml_es(n, "w:p"))
		parrafo_docx(c, n);
	else if (xml_es(n, "w:tbl"))
		tabla_docx(c, n);
	else if ((xml_es(n, "w:sdt") || xml_es(n, "w:sdtContent") || xml_es(n, "w:customXml") || xml_es(n, "w:ins")) && c->hondo < 64) {
		c->hondo++;
		for (h = xml_primero(n); h; h = xml_sig(h, NULL))
			bloque(c, h);
		c->hondo--;
	}
}

static void enc_pie(lector_docx *c, const char *rid, int dest)
{
	u32 n, viejos = c->n_rel;
	u8 *dd;
	xml_doc *x;
	xml_nodo *h;
	int primero = 1;
	parrafo *p = dest == D_ENC ? &c->d->enc : &c->d->pie;
	drel *guarda = NULL;

	dd = saca_rel(c, rid, &n);
	if (!dd)
		return;
	x = xml_lee((char *)dd, n);
	so_suelta(dd);
	if (!x)
		return;
	/* (las imágenes del encabezado van por sus relaciones: se dejan) */
	(void)viejos;
	(void)guarda;
	c->destino = dest;
	for (h = xml_hijo(xml_raiz(x), "w:p"); h; h = xml_sig(h, "w:p")) {
		fmt_parrafo fp;
		fmt_letra fl;
		int num, niv;
		fx_props vacio;

		formato_par(c, h, &fp, &fl, &num, &niv);
		if (primero) {
			p->p = fp;
			p->p.salto_antes = 0;
		} else {
			u32 s = SALTO_LINEA;

			letras(c, &s, 1, doc_fmt(c->d, &fl));
		}
		primero = 0;
		memset(&vacio, 0, sizeof vacio);
		c->campo = 0;
		contenido(c, h, &fl, &vacio);
	}
	/* (sin las imágenes: no son de la página) */
	{
		u32 k = 0, j;

		for (j = 0; j < p->n; j++)
			if (!es_imagen(p->t[j]) && !es_nota(p->t[j])) {
				p->t[k] = p->t[j];
				p->f[k++] = p->f[j];
			}
		p->n = k;
	}
	if (dest == D_ENC)
		c->d->hay_enc = 1;
	else
		c->d->hay_pie = 1;
	c->destino = D_CUERPO;
	xml_suelta(x);
}

static void pagina_docx(lector_docx *c, const xml_nodo *sp)
{
	documento *d = c->d;
	xml_nodo *h;
	const char *v;

	if (!sp)
		return;
	if ((h = xml_hijo(sp, "w:pgSz"))) {
		long w = (v = xml_atr(h, "w:w")) ? numero(v, NULL) : 0, a = (v = xml_atr(h, "w:h")) ? numero(v, NULL) : 0;

		if (w >= 2880 && w <= 31680)
			d->pag_an = (i32)w;
		if (a >= 2880 && a <= 31680)
			d->pag_al = (i32)a;
	}
	if ((h = xml_hijo(sp, "w:pgMar"))) {
		long m;

		if ((v = xml_atr(h, "w:top")) && (m = numero(v, NULL)) >= 0 && m < 8000)
			d->marg_sup = (i32)m;
		if ((v = xml_atr(h, "w:bottom")) && (m = numero(v, NULL)) >= 0 && m < 8000)
			d->marg_inf = (i32)m;
		if (((v = xml_atr(h, "w:left")) || (v = xml_atr(h, "w:start"))) && (m = numero(v, NULL)) >= 0 && m < 8000)
			d->marg_izq = (i32)m;
		if (((v = xml_atr(h, "w:right")) || (v = xml_atr(h, "w:end"))) && (m = numero(v, NULL)) >= 0 && m < 8000)
			d->marg_der = (i32)m;
		if ((v = xml_atr(h, "w:header")) && (m = numero(v, NULL)) >= 0 && m < 8000)
			d->dist_enc = (i32)m;
		if ((v = xml_atr(h, "w:footer")) && (m = numero(v, NULL)) >= 0 && m < 8000)
			d->dist_pie = (i32)m;
	}
	if ((h = xml_hijo(sp, "w:pgNumType")) && (v = xml_atr(h, "w:start")) && numero(v, NULL) > 0)
		d->num_inicio = (u32)numero(v, NULL) % 100000;
	{
		const char *enc = NULL, *pie = NULL;

		for (h = xml_primero(sp); h; h = xml_sig(h, NULL)) {
			const char *ty = xml_atr(h, "w:type");

			if (xml_es(h, "w:headerReference") && (!enc || (ty && iguales(ty, "default"))))
				enc = xml_atr(h, "r:id");
			if (xml_es(h, "w:footerReference") && (!pie || (ty && iguales(ty, "default"))))
				pie = xml_atr(h, "r:id");
		}
		if (enc)
			enc_pie(c, enc, D_ENC);
		if (pie)
			enc_pie(c, pie, D_PIE);
	}
}

static void meta_docx(lector_docx *c)
{
	u32 n;
	u8 *m = zip_saca(c->z, "docProps/core.xml", &n);
	xml_doc *x;
	bufer b = { 0 };
	const char *que[3] = { "dc:title", "dc:creator", "dc:subject" };
	char *a[3];
	u32 cabe[3];
	int k;

	if (!m)
		return;
	x = xml_lee((char *)m, n);
	so_suelta(m);
	if (!x)
		return;
	a[0] = c->d->titulo;
	a[1] = c->d->autor;
	a[2] = c->d->asunto;
	cabe[0] = sizeof c->d->titulo;
	cabe[1] = sizeof c->d->autor;
	cabe[2] = sizeof c->d->asunto;
	for (k = 0; k < 3; k++) {
		xml_nodo *h = xml_hijo(xml_raiz(x), que[k]);

		if (!h)
			continue;
		b.n = 0;
		xml_texto(h, &b);
		buf_c(&b, 0);
		if (!b.mal)
			copia(a[k], (char *)b.d, cabe[k]);
	}
	buf_suelta(&b);
	xml_suelta(x);
}

documento *docx_lee(const u8 *datos, u32 n, char *error, u32 cabe)
{
	lector_docx c;
	u32 nd;
	u8 *dd = NULL;
	xml_doc *x = NULL;
	xml_nodo *cuerpo;

	memset(&c, 0, sizeof c);
	c.z = zip_abre(datos, n);
	if (!c.z) {
		copia(error, "No es un documento DOCX (no es un ZIP que se pueda leer).", cabe);
		return NULL;
	}
	dd = zip_saca(c.z, "word/document.xml", &nd);
	if (!dd || !(x = xml_lee((char *)dd, nd))) {
		copia(error, "El documento DOCX no trae su texto (word/document.xml) o está estropeado.", cabe);
		goto mal;
	}
	so_suelta(dd);
	dd = NULL;
	c.d = doc_nuevo();
	if (!c.d)
		goto mal;
	doc_quita_parrafos(c.d, 0, c.d->n_par);
	c.nota = -1;
	lee_tema(&c);
	/* lo de Word sin decir nada: Times New Roman de 10, sin espacios */
	c.def.l = fmt_letra_normal();
	copia(c.def.l.familia, "Times New Roman", sizeof c.def.l.familia);
	c.def.l.tam = 20;
	c.def.p = fmt_parrafo_normal();
	c.def.p.despues = 0;
	c.def.p.inter = 100;
	c.def.ml = FL_FAM | FL_TAM;
	c.def.mp = FP_DES | FP_INTER | FP_ALIN | FP_SI | FP_SD | FP_SP | FP_ANT;
	lee_rels(&c, "word/_rels/document.xml.rels");
	lee_estilos(&c);
	lee_listas(&c);
	{
		u32 nn;
		u8 *fn = zip_saca(c.z, "word/footnotes.xml", &nn);

		if (fn) {
			c.notas = xml_lee((char *)fn, nn);
			so_suelta(fn);
		}
	}
	cuerpo = xml_hijo(xml_raiz(x), "w:body");
	if (cuerpo) {
		xml_nodo *h;

		for (h = xml_primero(cuerpo); h && !c.d->sin_memoria; h = xml_sig(h, NULL))
			bloque(&c, h);
		pagina_docx(&c, xml_hijo(cuerpo, "w:sectPr"));
	}
	meta_docx(&c);
	if (c.d->sin_memoria) {
		copia(error, "No hay memoria para abrirlo.", cabe);
		goto mal;
	}
	if (!c.d->n_par)
		arch_parrafo(c.d);
	fx_repasa_tablas(c.d);
	xml_suelta(x);
	xml_suelta(c.notas);
	zip_cierra(c.z);
	so_suelta(c.est);
	so_suelta(c.abs);
	so_suelta(c.num);
	so_suelta(c.rel);
	return c.d;
mal:
	so_suelta(dd);
	xml_suelta(x);
	xml_suelta(c.notas);
	zip_cierra(c.z);
	so_suelta(c.est);
	so_suelta(c.abs);
	so_suelta(c.num);
	so_suelta(c.rel);
	doc_suelta(c.d);
	return NULL;
}

/* ======================================================================
 *  Escribir
 * ====================================================================== */
#define NS_W "xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" " \
	"xmlns:wp=\"http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing\" xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" " \
	"xmlns:pic=\"http://schemas.openxmlformats.org/drawingml/2006/picture\""

static void id_estilo(const documento *d, u32 i, char *t, u32 cabe)
{
	static const char *const serie[EST_N] = { "Normal", "Title", "Heading1", "Heading2", "Heading3", "Subtitle", "Quote",
		"ListParagraph", "Header", "Footer", "FootnoteText", "NoSpacing" };

	if (i < EST_N && d->est[i].de_serie)
		copia(t, serie[i], cabe);
	else
		fmt(t, cabe, "Estilo%u", i);
}

static void pon_rpr(bufer *b, const fmt_letra *f, int con_fuente)
{
	buf_txt(b, "<w:rPr>");
	if (con_fuente && f->familia[0]) {
		buf_txt(b, "<w:rFonts");
		xml_pon_atr(b, "w:ascii", f->familia);
		xml_pon_atr(b, "w:hAnsi", f->familia);
		xml_pon_atr(b, "w:cs", f->familia);
		xml_pon_atr(b, "w:eastAsia", f->familia);
		buf_txt(b, "/>");
	}
	buf_txt(b, f->negrita ? "<w:b/><w:bCs/>" : "<w:b w:val=\"0\"/>");
	buf_txt(b, f->cursiva ? "<w:i/><w:iCs/>" : "<w:i w:val=\"0\"/>");
	if (f->versalitas)
		buf_txt(b, "<w:smallCaps/>");
	buf_txt(b, f->tachado ? "<w:strike/>" : "<w:strike w:val=\"0\"/>");
	if (f->color != SIN_COLOR)
		buf_fmt(b, "<w:color w:val=\"%06X\"/>", f->color);
	else
		buf_txt(b, "<w:color w:val=\"auto\"/>");
	buf_fmt(b, "<w:sz w:val=\"%u\"/><w:szCs w:val=\"%u\"/>", f->tam, f->tam);
	buf_txt(b, f->subrayado ? "<w:u w:val=\"single\"/>" : "<w:u w:val=\"none\"/>");
	if (f->resalte != SIN_COLOR)
		buf_fmt(b, "<w:shd w:val=\"clear\" w:color=\"auto\" w:fill=\"%06X\"/>", f->resalte);
	if (f->vert)
		buf_txt(b, f->vert == 1 ? "<w:vertAlign w:val=\"superscript\"/>" : "<w:vertAlign w:val=\"subscript\"/>");
	buf_txt(b, "</w:rPr>");
}

static void pon_ppr_props(bufer *b, const fmt_parrafo *p, int salto)
{
	u32 k;

	if (salto && p->salto_antes)
		buf_txt(b, "<w:pageBreakBefore/>");
	if (p->juntos)
		buf_txt(b, "<w:keepNext/>");
	if (p->n_tabs) {
		buf_txt(b, "<w:tabs>");
		for (k = 0; k < p->n_tabs && k < MAX_TABS; k++)
			buf_fmt(b, "<w:tab w:val=\"%s\" w:leader=\"%s\" w:pos=\"%d\"/>",
				p->tabs[k].tipo == TAB_CENTRO ? "center" : p->tabs[k].tipo == TAB_DER ? "right" : p->tabs[k].tipo == TAB_DECIMAL ? "decimal" : "left",
				p->tabs[k].relleno == '.' ? "dot" : p->tabs[k].relleno == '-' ? "hyphen" : p->tabs[k].relleno == '_' ? "underscore" : "none", p->tabs[k].pos);
		buf_txt(b, "</w:tabs>");
	}
	if (p->fondo != SIN_COLOR && !p->tabla)
		buf_fmt(b, "<w:shd w:val=\"clear\" w:color=\"auto\" w:fill=\"%06X\"/>", p->fondo);
	if (p->inter_tipo == INTER_MULTIPLE)
		buf_fmt(b, "<w:spacing w:before=\"%d\" w:after=\"%d\" w:line=\"%d\" w:lineRule=\"auto\"/>", p->antes, p->despues, (p->inter > 0 ? p->inter : 100) * 240 / 100);
	else
		buf_fmt(b, "<w:spacing w:before=\"%d\" w:after=\"%d\" w:line=\"%d\" w:lineRule=\"%s\"/>", p->antes, p->despues, p->inter,
			p->inter_tipo == INTER_EXACTO ? "exact" : "atLeast");
	if (p->sang_primera < 0)
		buf_fmt(b, "<w:ind w:left=\"%d\" w:right=\"%d\" w:hanging=\"%d\"/>", p->sang_izq, p->sang_der, -p->sang_primera);
	else
		buf_fmt(b, "<w:ind w:left=\"%d\" w:right=\"%d\" w:firstLine=\"%d\"/>", p->sang_izq, p->sang_der, p->sang_primera);
	buf_fmt(b, "<w:jc w:val=\"%s\"/>", p->alin == AL_CENTRO ? "center" : p->alin == AL_DER ? "right" : p->alin == AL_JUST ? "both" : "left");
	if (p->nivel_titulo)
		buf_fmt(b, "<w:outlineLvl w:val=\"%u\"/>", p->nivel_titulo - 1);
}

typedef struct {
	documento *d;
	bufer rels;			/* las relaciones de las imágenes (y lo demás) */
	u32 n_rels;
	u32 rid_img[IMAGEN_MAX > 4096 ? 4096 : IMAGEN_MAX];
	u32 n_dibujos;
	u16 *num_de_lista;		/* lista_id -> w:numId */
	int *tipo_num;			/* w:numId - 1 -> tipo; inicio */
	u32 *inicio_num;
	u32 n_nums;
	bufer notas;
} escritor_docx;

static void pon_texto_run(bufer *b, const u32 *t, u32 n)
{
	u32 k;

	if (!n)
		return;
	buf_txt(b, "<w:t xml:space=\"preserve\">");
	for (k = 0; k < n; k++)
		fx_pon_letra(b, t[k]);
	buf_txt(b, "</w:t>");
}

static void pon_imagen_docx(escritor_docx *w, bufer *b, u32 n)
{
	documento *d = w->d;
	const imagen *m = &d->img[n];
	i64 cx = (i64)m->ancho * EMU_TW, cy = (i64)m->alto * EMU_TW;
	u32 rid = n < 4096 ? w->rid_img[n] : 0;
	char cxs[24], cys[24];

	if (!rid)
		return;
	w->n_dibujos++;
	fmt(cxs, sizeof cxs, "%u", (u32)(cx > 0xFFFFFFFFLL ? 0xFFFFFFFFu : cx));
	fmt(cys, sizeof cys, "%u", (u32)(cy > 0xFFFFFFFFLL ? 0xFFFFFFFFu : cy));
	buf_txt(b, "<w:drawing>");
	if (m->ajuste == AJUSTE_LINEA)
		buf_txt(b, "<wp:inline distT=\"0\" distB=\"0\" distL=\"0\" distR=\"0\">");
	else
		buf_fmt(b, "<wp:anchor distT=\"0\" distB=\"0\" distL=\"114300\" distR=\"114300\" simplePos=\"0\" relativeHeight=\"%u\" behindDoc=\"0\" locked=\"0\" layoutInCell=\"1\" allowOverlap=\"1\">"
			   "<wp:simplePos x=\"0\" y=\"0\"/><wp:positionH relativeFrom=\"column\"><wp:align>%s</wp:align></wp:positionH>"
			   "<wp:positionV relativeFrom=\"paragraph\"><wp:posOffset>0</wp:posOffset></wp:positionV>",
			w->n_dibujos, m->ajuste == AJUSTE_DER ? "right" : "left");
	buf_fmt(b, "<wp:extent cx=\"%s\" cy=\"%s\"/>", cxs, cys);
	if (m->ajuste != AJUSTE_LINEA)
		buf_txt(b, "<wp:effectExtent l=\"0\" t=\"0\" r=\"0\" b=\"0\"/><wp:wrapSquare wrapText=\"bothSides\"/>");
	buf_fmt(b, "<wp:docPr id=\"%u\" name=\"Imagen %u\"/><a:graphic><a:graphicData uri=\"http://schemas.openxmlformats.org/drawingml/2006/picture\">"
		   "<pic:pic><pic:nvPicPr><pic:cNvPr id=\"%u\" name=\"%s\"/><pic:cNvPicPr/></pic:nvPicPr>"
		   "<pic:blipFill><a:blip r:embed=\"rId%u\"/><a:stretch><a:fillRect/></a:stretch></pic:blipFill>"
		   "<pic:spPr><a:xfrm><a:off x=\"0\" y=\"0\"/><a:ext cx=\"%s\" cy=\"%s\"/></a:xfrm><a:prstGeom prst=\"rect\"><a:avLst/></a:prstGeom></pic:spPr></pic:pic>"
		   "</a:graphicData></a:graphic>",
		w->n_dibujos, w->n_dibujos, w->n_dibujos, m->nombre, rid, cxs, cys);
	buf_txt(b, m->ajuste == AJUSTE_LINEA ? "</wp:inline>" : "</wp:anchor>");
	buf_txt(b, "</w:drawing>");
}

/* las letras de un párrafo (o de un trozo de él) en sus w:r */
static void pon_runs(escritor_docx *w, bufer *b, const parrafo *p, u32 a, u32 z)
{
	documento *d = w->d;
	u32 k = a;

	while (k < z) {
		u16 f = p->f[k];
		u32 j = k, t[256], n = 0;

		buf_txt(b, "<w:r>");
		pon_rpr(b, &d->fl[f], 1);
		while (j < z && p->f[j] == f) {
			u32 c = p->t[j];

			if (c == '\t' || c == SALTO_LINEA || es_objeto(c) || n == 256) {
				pon_texto_run(b, t, n);
				n = 0;
				if (c == '\t')
					buf_txt(b, "<w:tab/>");
				else if (c == SALTO_LINEA)
					buf_txt(b, "<w:br/>");
				else if (c == CAMPO_PAGINA || c == CAMPO_PAGINAS) {
					buf_fmt(b, "</w:r><w:r>");
					pon_rpr(b, &d->fl[f], 1);
					buf_fmt(b, "<w:fldChar w:fldCharType=\"begin\"/></w:r><w:r>");
					pon_rpr(b, &d->fl[f], 1);
					buf_fmt(b, "<w:instrText xml:space=\"preserve\"> %s </w:instrText></w:r><w:r>", c == CAMPO_PAGINA ? "PAGE" : "NUMPAGES");
					pon_rpr(b, &d->fl[f], 1);
					buf_txt(b, "<w:fldChar w:fldCharType=\"separate\"/></w:r><w:r>");
					pon_rpr(b, &d->fl[f], 1);
					buf_txt(b, "<w:t>1</w:t></w:r><w:r>");
					pon_rpr(b, &d->fl[f], 1);
					buf_txt(b, "<w:fldChar w:fldCharType=\"end\"/>");
				} else if (es_imagen(c) && c - IMAGEN_0 < d->n_img)
					pon_imagen_docx(w, b, c - IMAGEN_0);
				else if (es_nota(c) && c - NOTA_0 < d->n_notas) {
					fmt_letra fr = d->fl[f];

					fr.vert = 1;
					buf_txt(b, "</w:r><w:r>");
					pon_rpr(b, &fr, 1);
					buf_fmt(b, "<w:footnoteReference w:id=\"%u\"/></w:r><w:r>", c - NOTA_0 + 1);
					pon_rpr(b, &d->fl[f], 1);
				} else if (n < 256 && !es_objeto(c))
					t[n++] = c;
				j++;
				continue;
			}
			t[n++] = c;
			j++;
		}
		pon_texto_run(b, t, n);
		buf_txt(b, "</w:r>");
		k = j;
	}
}

static void pon_ppr(escritor_docx *w, bufer *b, const fmt_parrafo *p, int salto)
{
	char id[32];

	id_estilo(w->d, p->estilo < w->d->n_est ? p->estilo : 0, id, sizeof id);
	buf_fmt(b, "<w:pPr><w:pStyle w:val=\"%s\"/>", id);
	if (p->lista && p->lista_id < 0xFFFF && w->num_de_lista && w->num_de_lista[p->lista_id])
		buf_fmt(b, "<w:numPr><w:ilvl w:val=\"%u\"/><w:numId w:val=\"%u\"/></w:numPr>", p->nivel, w->num_de_lista[p->lista_id]);
	pon_ppr_props(b, p, salto);
	buf_txt(b, "</w:pPr>");
}

static void pon_parrafo_docx(escritor_docx *w, bufer *b, const parrafo *p)
{
	buf_txt(b, "<w:p>");
	pon_ppr(w, b, &p->p, 1);
	pon_runs(w, b, p, 0, p->n);
	buf_txt(b, "</w:p>");
}

/* la celda que tapa (f, c): su párrafo; -1 si no hay */
static int ancla_de(const documento *d, u32 ini, u32 n, u32 cols, u32 f, u32 c)
{
	u32 k;

	for (k = 0; k < n; k++) {
		const parrafo *p = &d->par[ini + k];
		u32 ac = p->p.abarca_cols ? p->p.abarca_cols : 1, af = p->p.abarca_filas ? p->p.abarca_filas : 1;

		if (p->p.cubierta)
			continue;
		if (f >= p->p.fila && f < p->p.fila + af && c >= p->p.col && c < p->p.col + ac && !(f == p->p.fila && c == p->p.col))
			return (int)(ini + k);
	}
	(void)cols;
	return -1;
}

static u32 pon_tabla_docx(escritor_docx *w, bufer *b, u32 i)
{
	documento *d = w->d;
	u32 t = d->par[i].p.tabla, n, k, f;
	const tabla *tb = &d->tab[t - 1];
	i32 tot = 0;
	const char *borde = tb->bordes == BORDE_NO ? "nil" : "single";

	for (n = 0; i + n < d->n_par && d->par[i + n].p.tabla == t; n++)
		;
	for (k = 0; k < tb->cols; k++)
		tot += tb->ancho[k];
	if (d->par[i].p.salto_antes)
		buf_txt(b, "<w:p><w:pPr><w:pageBreakBefore/></w:pPr></w:p>");
	buf_fmt(b, "<w:tbl><w:tblPr><w:tblW w:w=\"%d\" w:type=\"dxa\"/><w:jc w:val=\"%s\"/>", tot,
		tb->alin == AL_CENTRO ? "center" : tb->alin == AL_DER ? "right" : "left");
	buf_fmt(b, "<w:tblBorders><w:top w:val=\"%s\" w:sz=\"4\" w:space=\"0\" w:color=\"%06X\"/><w:left w:val=\"%s\" w:sz=\"4\" w:space=\"0\" w:color=\"%06X\"/>"
		   "<w:bottom w:val=\"%s\" w:sz=\"4\" w:space=\"0\" w:color=\"%06X\"/><w:right w:val=\"%s\" w:sz=\"4\" w:space=\"0\" w:color=\"%06X\"/>"
		   "<w:insideH w:val=\"%s\" w:sz=\"4\" w:space=\"0\" w:color=\"%06X\"/><w:insideV w:val=\"%s\" w:sz=\"4\" w:space=\"0\" w:color=\"%06X\"/></w:tblBorders>",
		borde, tb->color, borde, tb->color, borde, tb->color, borde, tb->color,
		tb->bordes == BORDE_TODOS ? "single" : "nil", tb->color, tb->bordes == BORDE_TODOS ? "single" : "nil", tb->color);
	buf_fmt(b, "<w:tblLayout w:type=\"fixed\"/><w:tblCellMar><w:left w:w=\"%d\" w:type=\"dxa\"/><w:right w:w=\"%d\" w:type=\"dxa\"/></w:tblCellMar></w:tblPr><w:tblGrid>",
		tb->margen, tb->margen);
	for (k = 0; k < tb->cols; k++)
		buf_fmt(b, "<w:gridCol w:w=\"%d\"/>", tb->ancho[k]);
	buf_txt(b, "</w:tblGrid>");
	for (f = 0; f < tb->filas; f++) {
		buf_txt(b, "<w:tr>");
		for (k = 0; k < tb->cols; k++) {
			u32 j = i + f * tb->cols + k;
			const parrafo *p;
			u32 ac, wcel = 0, q;
			int an;

			if (j >= i + n)
				break;
			p = &d->par[j];
			if (p->p.cubierta) {
				an = ancla_de(d, i, n, tb->cols, f, k);
				if (an >= 0 && d->par[an].p.col == k && d->par[an].p.fila < f) {
					/* debajo de una unida hacia abajo: una celda que sigue a la de arriba */
					const parrafo *a = &d->par[an];
					u32 sac = a->p.abarca_cols ? a->p.abarca_cols : 1;

					for (q = k; q < k + sac && q < tb->cols; q++)
						wcel += (u32)tb->ancho[q];
					buf_fmt(b, "<w:tc><w:tcPr><w:tcW w:w=\"%u\" w:type=\"dxa\"/>", wcel);
					if (sac > 1)
						buf_fmt(b, "<w:gridSpan w:val=\"%u\"/>", sac);
					buf_txt(b, "<w:vMerge/></w:tcPr><w:p/></w:tc>");
				} else if (an < 0) {
					buf_fmt(b, "<w:tc><w:tcPr><w:tcW w:w=\"%d\" w:type=\"dxa\"/></w:tcPr><w:p/></w:tc>", tb->ancho[k]);
				}
				continue;
			}
			ac = p->p.abarca_cols ? p->p.abarca_cols : 1;
			for (q = k; q < k + ac && q < tb->cols; q++)
				wcel += (u32)tb->ancho[q];
			buf_fmt(b, "<w:tc><w:tcPr><w:tcW w:w=\"%u\" w:type=\"dxa\"/>", wcel);
			if (ac > 1)
				buf_fmt(b, "<w:gridSpan w:val=\"%u\"/>", ac);
			if (p->p.abarca_filas > 1)
				buf_txt(b, "<w:vMerge w:val=\"restart\"/>");
			if (p->p.fondo != SIN_COLOR)
				buf_fmt(b, "<w:shd w:val=\"clear\" w:color=\"auto\" w:fill=\"%06X\"/>", p->p.fondo);
			else if (tb->cabecera && f == 0)
				buf_txt(b, "<w:shd w:val=\"clear\" w:color=\"auto\" w:fill=\"E7ECF4\"/>");
			buf_txt(b, "</w:tcPr>");
			{
				/* un w:p por cada trozo entre saltos de línea */
				u32 a = 0, x;
				fmt_parrafo fp = p->p;

				fp.salto_antes = 0;
				fp.lista = LISTA_NO;
				for (x = 0; x <= p->n; x++)
					if (x == p->n || p->t[x] == SALTO_LINEA) {
						buf_txt(b, "<w:p>");
						pon_ppr(w, b, &fp, 0);
						pon_runs(w, b, p, a, x);
						buf_txt(b, "</w:p>");
						a = x + 1;
					}
			}
			buf_txt(b, "</w:tc>");
		}
		buf_txt(b, "</w:tr>");
	}
	buf_txt(b, "</w:tbl>");
	return i + n;
}

static void pon_hdr(escritor_docx *w, bufer *b, const parrafo *p, const char *etq)
{
	u32 a = 0, x;
	fmt_parrafo fp = p->p;

	fp.salto_antes = 0;
	fp.tabla = 0;
	fp.lista = LISTA_NO;
	buf_fmt(b, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n<%s " NS_W ">", etq);
	for (x = 0; x <= p->n; x++)
		if (x == p->n || p->t[x] == SALTO_LINEA) {
			parrafo q = *p;
			u32 k, m = 0;

			/* (sin imágenes ni notas: no tienen sus relaciones aquí) */
			buf_txt(b, "<w:p>");
			pon_ppr(w, b, &fp, 0);
			q.t = so_pide((x - a + 1) * 4);
			q.f = so_pide((x - a + 1) * 2);
			if (q.t && q.f) {
				for (k = a; k < x; k++)
					if (!es_imagen(p->t[k]) && !es_nota(p->t[k])) {
						q.t[m] = p->t[k];
						q.f[m++] = p->f[k];
					}
				q.n = m;
				pon_runs(w, b, &q, 0, m);
			}
			so_suelta(q.t);
			so_suelta(q.f);
			buf_txt(b, "</w:p>");
			a = x + 1;
		}
	buf_fmt(b, "</%s>", etq);
}

static const char *ext_de(int tipo)
{
	switch (tipo) {
	case IMG_PNG: return "png";
	case IMG_JPG: return "jpeg";
	case IMG_BMP: return "bmp";
	}
	return "gif";
}

int docx_escribe(documento *d, bufer *sal)
{
	escritor_docx w;
	bufer doc = { 0 }, est = { 0 }, num = { 0 }, ct = { 0 }, rr = { 0 }, core = { 0 }, enc = { 0 }, pie = { 0 }, notas = { 0 };
	zip_escritor *z = NULL;
	u32 i, k, max_id = 0;
	int r = -1;

	memset(&w, 0, sizeof w);
	w.d = d;
	/* las listas: un w:num por cada lista_id que se usa */
	for (i = 0; i < d->n_par; i++)
		if (d->par[i].p.lista && d->par[i].p.lista_id + 1u > max_id)
			max_id = d->par[i].p.lista_id + 1u;
	if (max_id) {
		w.num_de_lista = so_pide(max_id * sizeof(u16));
		w.tipo_num = so_pide(max_id * sizeof(int));
		w.inicio_num = so_pide(max_id * sizeof(u32));
		if (!w.num_de_lista || !w.tipo_num || !w.inicio_num)
			goto fin;
		memset(w.num_de_lista, 0, max_id * sizeof(u16));
		for (i = 0; i < d->n_par; i++) {
			const fmt_parrafo *p = &d->par[i].p;

			if (p->lista && !w.num_de_lista[p->lista_id]) {
				w.tipo_num[w.n_nums] = p->lista;
				w.inicio_num[w.n_nums] = p->inicio_lista;
				w.num_de_lista[p->lista_id] = (u16)(++w.n_nums);
			}
		}
	}
	/* las relaciones: 1 estilos, 2 listas, 3 notas, 4 encabezado, 5 pie, 6 ajustes; luego las imágenes */
	w.n_rels = 6;
	for (k = 0; k < d->n_img && k < 4096; k++)
		w.rid_img[k] = ++w.n_rels;
	/* document.xml */
	buf_txt(&doc, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n<w:document " NS_W "><w:body>");
	for (i = 0; i < d->n_par;) {
		if (d->par[i].p.tabla && d->par[i].p.tabla <= d->n_tab) {
			i = pon_tabla_docx(&w, &doc, i);
			/* (dos tablas seguidas se juntarían en Word: un párrafo entre ellas) */
			if (i < d->n_par && d->par[i].p.tabla)
				buf_txt(&doc, "<w:p/>");
			continue;
		}
		pon_parrafo_docx(&w, &doc, &d->par[i]);
		i++;
	}
	if (d->n_par && d->par[d->n_par - 1].p.tabla)
		buf_txt(&doc, "<w:p/>");
	buf_txt(&doc, "<w:sectPr>");
	if (d->hay_enc)
		buf_txt(&doc, "<w:headerReference w:type=\"default\" r:id=\"rId4\"/>");
	if (d->hay_pie)
		buf_txt(&doc, "<w:footerReference w:type=\"default\" r:id=\"rId5\"/>");
	buf_fmt(&doc, "<w:pgSz w:w=\"%d\" w:h=\"%d\"%s/><w:pgMar w:top=\"%d\" w:right=\"%d\" w:bottom=\"%d\" w:left=\"%d\" w:header=\"%d\" w:footer=\"%d\" w:gutter=\"0\"/>",
		d->pag_an, d->pag_al, d->pag_an > d->pag_al ? " w:orient=\"landscape\"" : "", d->marg_sup, d->marg_der, d->marg_inf, d->marg_izq, d->dist_enc, d->dist_pie);
	if (d->num_inicio > 1)
		buf_fmt(&doc, "<w:pgNumType w:start=\"%u\"/>", d->num_inicio);
	if (d->enc_no_primera)
		buf_txt(&doc, "<w:titlePg/>");
	buf_txt(&doc, "</w:sectPr></w:body></w:document>\n");
	/* styles.xml */
	{
		fmt_letra fl = fmt_letra_normal();

		buf_txt(&est, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n<w:styles " NS_W "><w:docDefaults><w:rPrDefault>");
		pon_rpr(&est, &fl, 1);
		buf_txt(&est, "</w:rPrDefault><w:pPrDefault><w:pPr><w:spacing w:after=\"0\" w:line=\"240\" w:lineRule=\"auto\"/></w:pPr></w:pPrDefault></w:docDefaults>");
	}
	for (k = 0; k < d->n_est; k++) {
		const estilo *e = &d->est[k];
		char id[32], sig[32];

		id_estilo(d, k, id, sizeof id);
		id_estilo(d, e->siguiente < d->n_est ? e->siguiente : k, sig, sizeof sig);
		buf_fmt(&est, "<w:style w:type=\"paragraph\"%s w:styleId=\"%s\"><w:name", k == 0 ? " w:default=\"1\"" : "", id);
		xml_pon_atr(&est, "w:val", k < EST_N && e->de_serie ? fx_nombre_word((int)k) : e->nombre);
		buf_txt(&est, "/>");
		if (k)
			buf_txt(&est, "<w:basedOn w:val=\"Normal\"/>");
		buf_fmt(&est, "<w:next w:val=\"%s\"/><w:qFormat/><w:pPr>", sig);
		pon_ppr_props(&est, &e->p, 0);
		buf_txt(&est, "</w:pPr>");
		pon_rpr(&est, &e->letra, 1);
		buf_txt(&est, "</w:style>");
	}
	buf_txt(&est, "<w:style w:type=\"table\" w:default=\"1\" w:styleId=\"TableNormal\"><w:name w:val=\"Normal Table\"/><w:tblPr><w:tblInd w:w=\"0\" w:type=\"dxa\"/>"
		      "<w:tblCellMar><w:top w:w=\"0\" w:type=\"dxa\"/><w:left w:w=\"108\" w:type=\"dxa\"/><w:bottom w:w=\"0\" w:type=\"dxa\"/><w:right w:w=\"108\" w:type=\"dxa\"/></w:tblCellMar></w:tblPr></w:style>");
	buf_txt(&est, "</w:styles>\n");
	/* numbering.xml */
	buf_txt(&num, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n<w:numbering " NS_W ">");
	{
		int t;
		static const char *const vin[3] = { "\xe2\x80\xa2", "\xe2\x80\x93", "\xe2\x96\xaa" };

		for (t = LISTA_VINETAS; t <= LISTA_ROMANOS; t++) {
			int l;

			buf_fmt(&num, "<w:abstractNum w:abstractNumId=\"%d\"><w:multiLevelType w:val=\"hybridMultilevel\"/>", t - 1);
			for (l = 0; l < 9; l++) {
				const char *f = t == LISTA_VINETAS ? "bullet" : t == LISTA_LETRAS ? "lowerLetter" : t == LISTA_ROMANOS ? "lowerRoman" : "decimal";
				char txt[16];

				if (t == LISTA_VINETAS)
					copia(txt, vin[l % 3], sizeof txt);
				else
					fmt(txt, sizeof txt, "%%%d%s", l + 1, t == LISTA_LETRAS ? ")" : ".");
				buf_fmt(&num, "<w:lvl w:ilvl=\"%d\"><w:start w:val=\"1\"/><w:numFmt w:val=\"%s\"/><w:lvlText w:val=\"%s\"/><w:lvlJc w:val=\"left\"/>"
					      "<w:pPr><w:ind w:left=\"%d\" w:hanging=\"360\"/></w:pPr>%s</w:lvl>",
					l, f, txt, 720 + 360 * l, t == LISTA_VINETAS ? "<w:rPr><w:rFonts w:ascii=\"DejaVu Sans\" w:hAnsi=\"DejaVu Sans\"/></w:rPr>" : "");
			}
			buf_txt(&num, "</w:abstractNum>");
		}
		for (k = 0; k < w.n_nums; k++) {
			buf_fmt(&num, "<w:num w:numId=\"%u\"><w:abstractNumId w:val=\"%d\"/>", k + 1, w.tipo_num[k] - 1);
			buf_fmt(&num, "<w:lvlOverride w:ilvl=\"0\"><w:startOverride w:val=\"%u\"/></w:lvlOverride>", w.inicio_num[k] ? w.inicio_num[k] : 1);
			buf_txt(&num, "</w:num>");
		}
	}
	buf_txt(&num, "</w:numbering>\n");
	/* footnotes.xml */
	buf_txt(&notas, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n<w:footnotes " NS_W ">"
			"<w:footnote w:type=\"separator\" w:id=\"-1\"><w:p><w:r><w:separator/></w:r></w:p></w:footnote>"
			"<w:footnote w:type=\"continuationSeparator\" w:id=\"0\"><w:p><w:r><w:continuationSeparator/></w:r></w:p></w:footnote>");
	for (k = 0; k < d->n_notas; k++) {
		const parrafo *p = &d->notas[k];
		u32 a = 0, x;
		fmt_parrafo fp = p->p;

		fp.estilo = EST_NOTA;
		fp.tabla = 0;
		fp.lista = LISTA_NO;
		fp.salto_antes = 0;
		buf_fmt(&notas, "<w:footnote w:id=\"%u\">", k + 1);
		for (x = 0; x <= p->n; x++)
			if (x == p->n || p->t[x] == SALTO_LINEA) {
				buf_txt(&notas, "<w:p>");
				pon_ppr(&w, &notas, &fp, 0);
				if (a == 0) {
					fmt_letra fr = d->est[EST_NOTA].letra;

					fr.vert = 1;
					buf_txt(&notas, "<w:r>");
					pon_rpr(&notas, &fr, 1);
					buf_txt(&notas, "<w:footnoteRef/></w:r><w:r><w:t xml:space=\"preserve\"> </w:t></w:r>");
				}
				{
					/* (sin objetos dentro de la nota) */
					u32 j;

					for (j = a; j < x; j++)
						if (!es_objeto(p->t[j])) {
							u32 b0 = j;

							while (j < x && !es_objeto(p->t[j]))
								j++;
							pon_runs(&w, &notas, p, b0, j);
							j--;
						}
				}
				buf_txt(&notas, "</w:p>");
				a = x + 1;
			}
		buf_txt(&notas, "</w:footnote>");
	}
	buf_txt(&notas, "</w:footnotes>\n");
	if (d->hay_enc)
		pon_hdr(&w, &enc, &d->enc, "w:hdr");
	if (d->hay_pie)
		pon_hdr(&w, &pie, &d->pie, "w:ftr");
	/* las relaciones del documento */
	buf_txt(&rr, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
		     "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/>"
		     "<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/numbering\" Target=\"numbering.xml\"/>"
		     "<Relationship Id=\"rId3\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/footnotes\" Target=\"footnotes.xml\"/>"
		     "<Relationship Id=\"rId6\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/settings\" Target=\"settings.xml\"/>");
	if (d->hay_enc)
		buf_txt(&rr, "<Relationship Id=\"rId4\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/header\" Target=\"header1.xml\"/>");
	if (d->hay_pie)
		buf_txt(&rr, "<Relationship Id=\"rId5\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/footer\" Target=\"footer1.xml\"/>");
	for (k = 0; k < d->n_img && k < 4096; k++)
		buf_fmt(&rr, "<Relationship Id=\"rId%u\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/image\" Target=\"media/imagen%u.%s\"/>",
			w.rid_img[k], k + 1, ext_de(d->img[k].tipo));
	buf_txt(&rr, "</Relationships>\n");
	/* [Content_Types].xml */
	buf_txt(&ct, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
		     "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
		     "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
		     "<Default Extension=\"png\" ContentType=\"image/png\"/><Default Extension=\"jpeg\" ContentType=\"image/jpeg\"/>"
		     "<Default Extension=\"bmp\" ContentType=\"image/bmp\"/><Default Extension=\"gif\" ContentType=\"image/gif\"/>"
		     "<Override PartName=\"/word/document.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"/>"
		     "<Override PartName=\"/word/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.styles+xml\"/>"
		     "<Override PartName=\"/word/numbering.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.numbering+xml\"/>"
		     "<Override PartName=\"/word/footnotes.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.footnotes+xml\"/>"
		     "<Override PartName=\"/word/settings.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.settings+xml\"/>"
		     "<Override PartName=\"/docProps/core.xml\" ContentType=\"application/vnd.openxmlformats-package.core-properties+xml\"/>");
	if (d->hay_enc)
		buf_txt(&ct, "<Override PartName=\"/word/header1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.header+xml\"/>");
	if (d->hay_pie)
		buf_txt(&ct, "<Override PartName=\"/word/footer1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.footer+xml\"/>");
	buf_txt(&ct, "</Types>\n");
	/* docProps/core.xml */
	buf_txt(&core, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n<cp:coreProperties xmlns:cp=\"http://schemas.openxmlformats.org/package/2006/metadata/core-properties\" "
		       "xmlns:dc=\"http://purl.org/dc/elements/1.1/\" xmlns:dcterms=\"http://purl.org/dc/terms/\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">");
	if (d->titulo[0])
		xml_pon_elem(&core, "dc:title", d->titulo);
	if (d->asunto[0])
		xml_pon_elem(&core, "dc:subject", d->asunto);
	if (d->autor[0])
		xml_pon_elem(&core, "dc:creator", d->autor);
	buf_txt(&core, "</cp:coreProperties>\n");
	if (doc.mal || est.mal || num.mal || ct.mal || rr.mal || core.mal || enc.mal || pie.mal || notas.mal)
		goto fin;
	z = zip_nuevo();
	if (!z)
		goto fin;
	{
		static const char rels[] = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
					   "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"word/document.xml\"/>"
					   "<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties\" Target=\"docProps/core.xml\"/>"
					   "</Relationships>\n";
		static const char ajustes[] = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n<w:settings xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
					      "<w:defaultTabStop w:val=\"709\"/><w:compat><w:compatSetting w:name=\"compatibilityMode\" w:uri=\"http://schemas.microsoft.com/office/word\" w:val=\"15\"/></w:compat></w:settings>\n";

		if (zip_mete(z, "[Content_Types].xml", ct.d, ct.n, 0) || zip_mete(z, "_rels/.rels", rels, sizeof rels - 1, 0) ||
		    zip_mete(z, "word/document.xml", doc.d, doc.n, 0) || zip_mete(z, "word/styles.xml", est.d, est.n, 0) ||
		    zip_mete(z, "word/numbering.xml", num.d, num.n, 0) || zip_mete(z, "word/footnotes.xml", notas.d, notas.n, 0) ||
		    zip_mete(z, "word/settings.xml", ajustes, sizeof ajustes - 1, 0) ||
		    zip_mete(z, "word/_rels/document.xml.rels", rr.d, rr.n, 0) || zip_mete(z, "docProps/core.xml", core.d, core.n, 0))
			goto fin;
	}
	if (d->hay_enc && zip_mete(z, "word/header1.xml", enc.d, enc.n, 0))
		goto fin;
	if (d->hay_pie && zip_mete(z, "word/footer1.xml", pie.d, pie.n, 0))
		goto fin;
	for (k = 0; k < d->n_img && k < 4096; k++) {
		char nm[64];

		fmt(nm, sizeof nm, "word/media/imagen%u.%s", k + 1, ext_de(d->img[k].tipo));
		if (zip_mete(z, nm, d->img[k].datos, d->img[k].n, d->img[k].tipo == IMG_JPG || d->img[k].tipo == IMG_PNG))
			goto fin;
	}
	r = zip_acaba(z, sal);
	z = NULL;
fin:
	zip_deja(z);
	buf_suelta(&doc);
	buf_suelta(&est);
	buf_suelta(&num);
	buf_suelta(&ct);
	buf_suelta(&rr);
	buf_suelta(&core);
	buf_suelta(&enc);
	buf_suelta(&pie);
	buf_suelta(&notas);
	buf_suelta(&w.rels);
	buf_suelta(&w.notas);
	so_suelta(w.num_de_lista);
	so_suelta(w.tipo_num);
	so_suelta(w.inicio_num);
	return r;
}
