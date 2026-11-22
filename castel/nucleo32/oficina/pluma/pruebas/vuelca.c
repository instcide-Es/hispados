/* vuelca.c - Pluma 2, pruebas de los formatos (pruebas/formatos.sh): un
 * programa aparte (en lugar de principal.c) que abre un documento con los
 * lectores de Pluma y saca lo que lleva, en un texto fijo (para comparar la
 * ida y la vuelta), o lo pasa a otro formato:
 *   vuelca DOCUMENTO                 lo que lleva (las medidas, en puntos)
 *   vuelca --convierte ENTRADA SALIDA
 *   vuelca --lee ARCHIVO...          solo leer (para el fuzz: 0 aunque no se pueda)
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdio.h>
#include <string.h>
#include "archivo.h"
#include "letras.h"
#include "pluma.h"

const char ofi_programa[] = "VUELCA";

static int pt(i32 tw)
{
	return (tw + (tw < 0 ? -10 : 10)) / 20;
}

static void letra(const fmt_letra *f, char *t, u32 cabe)
{
	fmt(t, cabe, "%s %u%s%s%s%s%s%s c=%x r=%x", f->familia, f->tam, f->negrita ? " N" : "", f->cursiva ? " K" : "", f->subrayado ? " S" : "",
	    f->tachado ? " T" : "", f->vert == 1 ? " sup" : f->vert == 2 ? " sub" : "", f->versalitas ? " vers" : "", f->color, f->resalte);
}

static void texto(const documento *d, const parrafo *p)
{
	u32 k;
	u16 f = 0xFFFF;

	for (k = 0; k < p->n; k++) {
		u32 c = p->t[k];

		if (p->f[k] != f) {
			char t[200];

			f = p->f[k];
			letra(&d->fl[f], t, sizeof t);
			printf("{%s}", t);
		}
		if (c == '\t')
			printf("<TAB>");
		else if (c == SALTO_LINEA)
			printf("<BR>");
		else if (c == CAMPO_PAGINA)
			printf("<PAG>");
		else if (c == CAMPO_PAGINAS)
			printf("<PAGS>");
		else if (es_imagen(c)) {
			const imagen *m = &d->img[c - IMAGEN_0];

			printf("<IMG tipo=%u ajuste=%u %dx%dpt %dx%dpx %u oct>", m->tipo, m->ajuste, pt(m->ancho), pt(m->alto), m->px_an, m->px_al, m->n);
		} else if (es_nota(c)) {
			printf("<NOTA ");
			texto(d, &d->notas[c - NOTA_0]);
			printf(">");
		} else {
			char u[5];
			int m = utf8_pon(c, u);

			u[m] = 0;
			printf("%s", u);
		}
	}
}

static void par(const documento *d, const parrafo *p, const char *que)
{
	const fmt_parrafo *f = &p->p;
	u32 k;

	printf("%s [%s] al=%u si=%d sd=%d sp=%d an=%d de=%d int=%u/%d%s%s%s", que, f->estilo < d->n_est ? d->est[f->estilo].nombre : "?", f->alin,
	       pt(f->sang_izq), pt(f->sang_der), pt(f->sang_primera), pt(f->antes), pt(f->despues), f->inter_tipo,
	       f->inter_tipo == INTER_MULTIPLE ? (f->inter + 2) / 5 * 5 : pt(f->inter), f->salto_antes ? " SALTO" : "", f->juntos ? " juntos" : "",
	       f->nivel_titulo ? " tit" : "");
	if (f->lista)
		printf(" lista=%u niv=%u ini=%u", f->lista, f->nivel, f->inicio_lista);
	for (k = 0; k < f->n_tabs; k++)
		printf(" tab=%d/%u/%c", pt(f->tabs[k].pos), f->tabs[k].tipo, f->tabs[k].relleno ? f->tabs[k].relleno : '0');
	if (f->tabla)
		printf(" T(%u,%u)%s%s%s", f->fila, f->col, f->cubierta ? " cub" : "", f->abarca_cols > 1 ? " ac" : "", f->abarca_filas > 1 ? " af" : "");
	if (f->abarca_cols > 1 || f->abarca_filas > 1)
		printf("=%u,%u", f->abarca_cols ? f->abarca_cols : 1, f->abarca_filas ? f->abarca_filas : 1);
	if (f->fondo != SIN_COLOR)
		printf(" fondo=%x", f->fondo);
	printf(": ");
	texto(d, p);
	printf("\n");
}

static int vuelca(const char *r)
{
	char e[300];
	int f;
	u32 i, k;
	documento *d = arch_abre(r, &f, e, sizeof e);

	if (!d) {
		printf("ERROR: %s\n", e);
		return 1;
	}
	printf("página %dx%d márgenes %d %d %d %d enc=%d pie=%d dist %d %d num=%u\n", pt(d->pag_an), pt(d->pag_al), pt(d->marg_izq), pt(d->marg_der),
	       pt(d->marg_sup), pt(d->marg_inf), d->hay_enc, d->hay_pie, pt(d->dist_enc), pt(d->dist_pie), d->num_inicio);
	printf("título «%s» autor «%s»\n", d->titulo, d->autor);
	for (i = 0; i < d->n_tab; i++) {
		printf("tabla %u: %ux%u bordes=%u alin=%u anchos", i, d->tab[i].filas, d->tab[i].cols, d->tab[i].bordes, d->tab[i].alin);
		for (k = 0; k < d->tab[i].cols; k++)
			printf(" %d", pt(d->tab[i].ancho[k]));
		printf("\n");
	}
	if (d->hay_enc)
		par(d, &d->enc, "ENC");
	if (d->hay_pie)
		par(d, &d->pie, "PIE");
	for (i = 0; i < d->n_par; i++)
		par(d, &d->par[i], "P");
	doc_suelta(d);
	return 0;
}

int ofi_principal(const char *args)
{
	char a1[600], a2[600], a3[600];
	const char *s = args;

	letras_inicia();
	s = pluma_arg(s, a1, sizeof a1);
	s = pluma_arg(s, a2, sizeof a2);
	s = pluma_arg(s, a3, sizeof a3);
	if (iguales(a1, "--convierte")) {
		char e[300], real[600];
		int f;
		documento *d = arch_abre(a2, &f, e, sizeof e);

		if (!d) {
			printf("ERROR: %s\n", e);
			return 1;
		}
		if (arch_guarda(d, a3, arch_formato_ext(a3), real, sizeof real, e, sizeof e)) {
			printf("ERROR: %s\n", e);
			doc_suelta(d);
			return 1;
		}
		doc_suelta(d);
		return 0;
	}
	if (iguales(a1, "--lee")) {
		char e[300];
		int f;
		const char *q = a2;

		while (*q) {
			documento *d = arch_abre(q, &f, e, sizeof e);

			doc_suelta(d);
			s = pluma_arg(s, a2, sizeof a2);
			q = a2;
		}
		return 0;
	}
	return vuelca(a1);
}

/* (lo de principal.c que usa: los argumentos) */
const char *pluma_arg(const char *s, char *d, u32 cabe)
{
	u32 k = 0;

	while (*s == ' ')
		s++;
	if (*s == '"') {
		s++;
		while (*s && *s != '"') {
			if (k + 1 < cabe)
				d[k++] = *s;
			s++;
		}
		if (*s == '"')
			s++;
	} else
		while (*s && *s != ' ') {
			if (k + 1 < cabe)
				d[k++] = *s;
			s++;
		}
	d[k] = 0;
	return s;
}
