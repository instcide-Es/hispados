/* paginas.c - Hispa-PDF: hacer un documento con unas páginas (de uno o de
 * varios PDF, en el orden que se quiera, giradas o no): unir, separar,
 * sacar, quitar, ordenar y girar son todo esto. Cada página nueva lleva lo
 * que heredaba (Resources, MediaBox, CropBox, Rotate); el árbol nuevo va
 * en grupos de 32. Cuando es un documento entero en su orden, se queda con
 * todo su catálogo (marcadores, etiquetas, formularios); si no, con los
 * marcadores y el formulario del primero (lo que apunte a páginas que no
 * van, a null).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "hpdf.h"

/* un objeto del documento nuevo, siguiendo su referencia */
static pobj *de_nuevo(pnuevo *s, pobj *o)
{
	int k;

	for (k = 0; o && o->t == O_REF && k < 8; k++)
		o = o->u.ref.num < s->n ? s->v[o->u.ref.num].o : NULL;
	return o && o->t != O_REF ? o : s->d->nulo;
}

static const char *heredables[] = {"Resources", "MediaBox", "CropBox", "Rotate"};

/* el árbol de las páginas: grupos de 32 hasta que quepan en uno */
u32 arbol_paginas(pnuevo *s, u32 *hojas, u32 n)
{
	pdoc *d = s->d;
	u32 *inicial = hojas;

	while (n > 32) {
		u32 grupos = (n + 31) / 32, g, k;
		u32 *nodos = pide(grupos * sizeof(u32));

		for (g = 0; g < grupos; g++) {
			pobj *nodo = o_dicc(d), *kids = o_arreglo(d);
			u32 num = nuevo_obj(s, nodo, NULL), cuenta = 0;

			o_pon(d, nodo, "Type", o_nombre(d, "Pages"));
			for (k = g * 32; k < n && k < g * 32 + 32; k++) {
				pobj *hijo = s->v[hojas[k]].o;

				o_mete(d, kids, o_ref(d, hojas[k], 0));
				o_pon(d, hijo, "Parent", o_ref(d, num, 0));
				cuenta += o_es_nombre(o_dic_clave(hijo, "Type"), "Pages") ? (u32)o_num(o_dic_clave(hijo, "Count"), 0) : 1;
			}
			o_pon(d, nodo, "Kids", kids);
			o_pon(d, nodo, "Count", o_ent(d, (i32)cuenta));
			nodos[g] = num;
		}
		if (hojas != inicial)
			so_suelta(hojas);
		hojas = nodos;
		n = grupos;
	}
	{
		pobj *raiz = o_dicc(d), *kids = o_arreglo(d);
		u32 num = nuevo_obj(s, raiz, NULL), k, cuenta = 0;

		o_pon(d, raiz, "Type", o_nombre(d, "Pages"));
		for (k = 0; k < n; k++) {
			pobj *hijo = s->v[hojas[k]].o;

			o_mete(d, kids, o_ref(d, hojas[k], 0));
			o_pon(d, hijo, "Parent", o_ref(d, num, 0));
			cuenta += o_es_nombre(o_dic_clave(hijo, "Type"), "Pages") ? (u32)o_num(o_dic_clave(hijo, "Count"), 0) : 1;
		}
		o_pon(d, raiz, "Kids", kids);
		o_pon(d, raiz, "Count", o_ent(d, (i32)cuenta));
		if (hojas != inicial)
			so_suelta(hojas);
		return num;
	}
}

pnuevo *compone(ppag *p, u32 n, int con_marcadores)
{
	pnuevo *s = nuevo_doc();
	pdoc *d = s->d, *primero = n ? p[0].d : NULL;
	u32 i, *hojas, num_raiz, num_arbol;
	int entero = n > 0, k;
	pobj *cat;

	if (n == 0)
		hpdf_falla("no hay ninguna página");
	/* ¿es un documento entero, en su orden? */
	for (i = 0; i < n; i++)
		if (p[i].d != primero || p[i].pagina != i)
			entero = 0;
	if (entero && n != primero->n_paginas)
		entero = 0;
	for (i = 0; i < n; i++)
		if (p[i].d->version > s->version)
			s->version = p[i].d->version;
	/* 1: los números de las páginas (la primera vez que sale cada una) */
	hojas = pide(n * sizeof(u32));
	for (i = 0; i < n; i++) {
		u32 *m = mapa_num(s, p[i].d, p[i].d->num_pagina[p[i].pagina]);

		if (m && *m == 0) {
			*m = nuevo_obj(s, NULL, NULL);
			hojas[i] = *m;
		} else
			hojas[i] = nuevo_obj(s, NULL, NULL);	/* (repetida: otra copia) */
	}
	/* 2: cada página, con lo que hereda y su giro */
	for (i = 0; i < n; i++) {
		pdoc *de = p[i].d;
		pobj *orig = de->paginas[p[i].pagina], *pg = o_dicc(d);
		u32 j;
		int giro;

		for (j = 0; j < orig->n; j++) {
			const char *clave = (const char *)orig->u.v[2 * j]->u.s;

			if (!strcmp(clave, "Parent") || (!strcmp(clave, "StructParents") && !entero))
				continue;
			o_pon(d, pg, clave, copia_de(s, de, orig->u.v[2 * j + 1]));
		}
		for (k = 0; k < 4; k++)
			if (!o_dic_clave(orig, heredables[k])) {
				pobj *h = pagina_heredada(de, p[i].pagina, heredables[k]);

				if (h)
					o_pon(d, pg, heredables[k], copia_de(s, de, h));
			}
		if (!o_dic_clave(pg, "MediaBox")) {
			pobj *mb = o_arreglo(d);

			o_mete(d, mb, o_ent(d, 0));
			o_mete(d, mb, o_ent(d, 0));
			o_mete(d, mb, o_ent(d, 612));
			o_mete(d, mb, o_ent(d, 792));
			o_pon(d, pg, "MediaBox", mb);
		}
		if (!o_dic_clave(pg, "Resources"))
			o_pon(d, pg, "Resources", o_dicc(d));
		o_pon(d, pg, "Type", o_nombre(d, "Page"));
		giro = (pagina_giro(de, p[i].pagina) + p[i].giro) % 360;
		if (giro < 0)
			giro += 360;
		giro = giro / 90 * 90;
		o_pon(d, pg, "Rotate", giro ? o_ent(d, giro) : NULL);
		s->v[hojas[i]].o = pg;
	}
	copia_pendientes(s);
	/* los enlaces a páginas que no van, fuera */
	for (i = 0; i < n; i++) {
		pobj *pg = s->v[hojas[i]].o, *an = de_nuevo(s, o_dic_clave(pg, "Annots"));
		u32 j, k = 0;

		if (an->t != O_ARREGLO)
			continue;
		for (j = 0; j < an->n; j++) {
			pobj *a = de_nuevo(s, an->u.v[j]), *dest, *acc;
			int vale = a->t == O_DICC;

			if (vale && o_es_nombre(o_dic_clave(a, "Subtype"), "Link")) {
				dest = de_nuevo(s, o_dic_clave(a, "Dest"));
				acc = de_nuevo(s, o_dic_clave(a, "A"));
				if (dest->t == O_ARREGLO && (dest->n == 0 || de_nuevo(s, dest->u.v[0])->t == O_NULO))
					vale = 0;
				if (acc->t == O_DICC && o_es_nombre(o_dic_clave(acc, "S"), "GoTo")) {
					pobj *dd = de_nuevo(s, o_dic_clave(acc, "D"));

					if (dd->t == O_ARREGLO && (dd->n == 0 || de_nuevo(s, dd->u.v[0])->t == O_NULO))
						vale = 0;
				}
			}
			if (vale)
				an->u.v[k++] = an->u.v[j];
		}
		an->n = k;
	}
	num_arbol = arbol_paginas(s, hojas, n);
	so_suelta(hojas);
	/* el catálogo */
	cat = o_dicc(d);
	if (entero) {
		pobj *orig = primero->raiz;
		u32 j;

		for (j = 0; j < orig->n; j++) {
			const char *clave = (const char *)orig->u.v[2 * j]->u.s;

			if (!strcmp(clave, "Pages") || !strcmp(clave, "Version") || !strcmp(clave, "Perms"))
				continue;
			o_pon(d, cat, clave, copia_de(s, primero, orig->u.v[2 * j + 1]));
		}
	} else if (con_marcadores) {
		static const char *claves[] = {"Outlines", "Names", "Dests", "PageMode", "PageLayout", "ViewerPreferences",
					       "Lang", "OCProperties", "AcroForm", "Metadata", "MarkInfo"};
		u32 j;

		for (j = 0; j < sizeof claves / sizeof claves[0]; j++) {
			pobj *v = o_dic_clave(primero->raiz, claves[j]);

			if (v)
				o_pon(d, cat, claves[j], copia_de(s, primero, v));
		}
		/* los campos de los formularios de los demás, con los del primero */
		{
			pobj *af = resuelve(d, o_dic_clave(cat, "AcroForm"));

			for (i = 1; i < n; i++) {
				pdoc *de = p[i].d;
				pobj *af2, *campos;
				u32 q;
				int ya = 0;

				for (q = 0; q < i; q++)
					if (p[q].d == de)
						ya = 1;
				if (ya || de == primero)
					continue;
				af2 = dic_r(de, de->raiz, "AcroForm");
				campos = af2 ? dic_r(de, af2, "Fields") : NULL;
				if (!campos || campos->t != O_ARREGLO)
					continue;
				if (af->t != O_DICC) {
					af = copia_de(s, de, af2);
					if (af->t == O_DICC)
						o_pon(d, cat, "AcroForm", af);
					continue;
				}
				{
					pobj *mios = resuelve(d, o_dic_clave(af, "Fields")), *todos = o_arreglo(d);

					if (mios->t == O_ARREGLO)
						for (q = 0; q < mios->n; q++)
							o_mete(d, todos, mios->u.v[q]);
					for (q = 0; q < campos->n; q++)
						o_mete(d, todos, copia_de(s, de, campos->u.v[q]));
					o_pon(d, af, "Fields", todos);
				}
			}
		}
		copia_pendientes(s);
	}
	copia_pendientes(s);
	o_pon(d, cat, "Type", o_nombre(d, "Catalog"));
	o_pon(d, cat, "Pages", o_ref(d, num_arbol, 0));
	num_raiz = nuevo_obj(s, cat, NULL);
	s->raiz = num_raiz;
	/* la información del primero, con el programa que lo ha hecho */
	{
		pobj *info = dic_r(primero, primero->trailer, "Info"), *ni = o_dicc(d);
		static const u8 prod[] = "Hispa-PDF 1.0 (Hispa-Pentium)";

		if (info && info->t == O_DICC) {
			u32 j;

			for (j = 0; j < info->n; j++) {
				pobj *v = resuelve(primero, info->u.v[2 * j + 1]);

				if (v->t == O_CADENA || v->t == O_NOMBRE)
					o_pon(d, ni, (const char *)info->u.v[2 * j]->u.s, v);
			}
		}
		o_pon(d, ni, "Producer", o_cadena(d, prod, sizeof prod - 1));
		s->info = nuevo_obj(s, ni, NULL);
	}
	return s;
}

/* «1-3,5,8-», «z» o «fin» para la última, «-3» desde la primera;
 * «3-1», al revés. Da cuántas (o -1 si no vale) */
static int num_de(const char **t, u32 total, u32 *v)
{
	const char *p = *t;
	u32 n = 0;

	if (*p == 'z' || *p == 'Z') {
		*v = total;
		*t = p + 1;
		return 1;
	}
	if (empieza_sin_caja(p, "fin")) {
		*v = total;
		*t = p + 3;
		return 1;
	}
	if (*p < '0' || *p > '9')
		return 0;
	while (*p >= '0' && *p <= '9') {
		n = n * 10 + (u32)(*p - '0');
		if (n > 10000000)
			return 0;
		p++;
	}
	*v = n;
	*t = p;
	return 1;
}

int rangos(const char *t, u32 total, u32 *sal, u32 cabe)
{
	u32 n = 0;

	if (!t || !*t)
		return -1;
	while (*t) {
		u32 a, b;

		while (*t == ' ' || *t == ',')
			t++;
		if (!*t)
			break;
		if (*t == '-') {
			a = 1;
		} else if (!num_de(&t, total, &a))
			return -1;
		if (*t == '-') {
			t++;
			if (!*t || *t == ',')
				b = total;
			else if (!num_de(&t, total, &b))
				return -1;
		} else
			b = a;
		if (a < 1 || b < 1 || a > total || b > total)
			return -1;
		if (a <= b) {
			for (; a <= b; a++) {
				if (n >= cabe)
					return -1;
				sal[n++] = a - 1;
			}
		} else {
			for (; a >= b; a--) {
				if (n >= cabe)
					return -1;
				sal[n++] = a - 1;
			}
		}
		if (*t && *t != ',' && *t != ' ')
			return -1;
	}
	return (int)n;
}
