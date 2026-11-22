/* vista.c - (COMPAT) el resultado en renglones (vista.h).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "vista.h"
#include "texto.h"

static renglon *pon(vista *v, int clase, int sangria, const char *f, ...)
{
	va_list a;
	renglon *r;

	if (v->n == v->cabe) {
		int c = v->cabe ? v->cabe * 2 : 64;
		renglon *x = so_repide(v->r, sizeof *x * (u32)c);

		if (!x)
			return NULL;
		v->r = x;
		v->cabe = c;
	}
	r = &v->r[v->n++];
	r->clase = clase;
	r->sangria = sangria;
	va_start(a, f);
	vfmt(r->t, sizeof r->t, f, a);
	va_end(a);
	return r;
}

void vista_suelta(vista *v)
{
	so_suelta(v->r);
	memset(v, 0, sizeof *v);
}

static const char *plural(u32 n, const char *uno, const char *varios) { return n == 1 ? uno : varios; }

/* ¿es esta falta de las que no importan? (su archivo no hace falta, o su DLL es opcional) */
static int sin_importancia(const c_informe *i, const c_falta *f)
{
	const c_saber *s = saber_dll(f->dll);

	return i->a[f->archivo].sin_importancia || (s && s->sin_importancia);
}

/* los grupos por DLL: lo de una DLL, de los archivos que importan (o no) */
static void grupo(vista *v, const c_informe *i, int importan)
{
	int k, j;
	char hechas[2048];

	hechas[0] = 0;
	for (k = 0; k < i->n_f; k++) {
		const c_falta *f = &i->f[k];
		const c_saber *s;
		char marca[48], linea[240], quien[200];
		u32 cuantas = 0;
		int primero = 1;

		if ((sin_importancia(i, f) ? 0 : 1) != importan)
			continue;
		fmt(marca, sizeof marca, "|%s|", f->dll);
		if (busca(hechas, marca))
			continue;
		if (largo(hechas) + largo(marca) < sizeof hechas)
			anade(hechas, marca, sizeof hechas);
		s = saber_dll(f->dll);
		/* cuántas distintas y quién las pide */
		quien[0] = 0;
		for (j = 0; j < i->n_f; j++) {
			const c_falta *g = &i->f[j];
			int q, repe = 0;

			if (!iguales_sin_caja(g->dll, f->dll) || (sin_importancia(i, g) ? 0 : 1) != importan)
				continue;
			for (q = 0; q < j && !repe; q++)
				repe = iguales_sin_caja(i->f[q].dll, g->dll) && iguales(i->f[q].fn, g->fn) && (sin_importancia(i, &i->f[q]) ? 0 : 1) == importan;
			if (!g->fn[0]) {
				/* (la DLL entera: lo más que le pide uno) */
				if ((g->n ? g->n : 1) > cuantas)
					cuantas = g->n ? g->n : 1;
			} else if (!repe)
				cuantas++;
			if (!busca(quien, i->a[g->archivo].nombre) && largo(quien) + largo(i->a[g->archivo].nombre) + 3 < sizeof quien) {
				if (quien[0])
					anade(quien, ", ", sizeof quien);
				anade(quien, i->a[g->archivo].nombre, sizeof quien);
			}
		}
		if (!f->fn[0])
			pon(v, R_DLL, 1, "Falta la DLL entera %s%s (%u %s)", f->dll, f->no32 ? " (está, pero no es de 32 bits)" : "", cuantas,
			    plural(cuantas, "función", "funciones"));
		else
			pon(v, R_DLL, 1, "%s: %s %u", f->dll, plural(cuantas, "falta", "faltan"), cuantas);
		if (s)
			pon(v, R_NOTA, 2, "%s: %s.%s%s", s->tema, s->para, s->nota ? " " : "", s->nota ? s->nota : "");
		else
			pon(v, R_NOTA, 2, "Una DLL que no es de Windows: tiene que venir con el programa (en su carpeta).");
		/* las funciones, en renglones de unas cuantas */
		linea[0] = 0;
		for (j = 0; j < i->n_f; j++) {
			const c_falta *g = &i->f[j];
			int q, repe = 0;

			if (!g->fn[0] || !iguales_sin_caja(g->dll, f->dll) || (sin_importancia(i, g) ? 0 : 1) != importan)
				continue;
			for (q = 0; q < j && !repe; q++)
				repe = iguales_sin_caja(i->f[q].dll, g->dll) && iguales(i->f[q].fn, g->fn);
			if (repe)
				continue;
			if (largo(linea) + largo(g->fn) + 2 > 88) {
				pon(v, R_FUNC, 2, "%s", linea);
				linea[0] = 0;
			}
			if (linea[0])
				anade(linea, ", ", sizeof linea);
			anade(linea, g->fn, sizeof linea);
			primero = 0;
		}
		if (linea[0])
			pon(v, R_FUNC, 2, "%s", linea);
		(void)primero;
		pon(v, R_GRIS, 2, "Lo %s: %s", busca(quien, ",") ? "piden" : "pide", quien);
	}
}

int vista_haz(vista *v, const c_informe *i, const char *nombre)
{
	u32 todas = inf_faltan(i, 0), imp = inf_faltan(i, 1);
	int k, n_sin = 0, n_ok = 0;

	memset(v, 0, sizeof *v);
	if (!todas)
		pon(v, R_BIEN, 0, "%s: está todo. Tiene lo que pide a Windows.", nombre);
	else if (!imp)
		pon(v, R_BIEN, 0, "%s: lo que hace falta está todo (faltan %u, pero en partes que no hacen falta).", nombre, todas);
	else if (imp == todas)
		pon(v, R_MAL, 0, "%s: %s %u.", nombre, plural(imp, "falta", "faltan"), imp);
	else
		pon(v, R_MAL, 0, "%s: %s %u (y %u más en partes que no hacen falta).", nombre, plural(imp, "falta", "faltan"), imp, todas - imp);
	for (k = 0; k < i->n_a; k++) {
		if (i->a[k].sin_importancia)
			n_sin++;
		if (!i->a[k].n_faltan && i->a[k].clase != A_NO_ESTA && i->a[k].clase != A_NO_PE)
			n_ok++;
	}
	pon(v, R_GRIS, 0, "Mirados: %d %s (%d con todo lo suyo).", i->n_a, plural((u32)i->n_a, "archivo", "archivos"), n_ok);
	if (imp) {
		pon(v, R_APARTADO, 0, "Lo que falta, por DLL");
		grupo(v, i, 1);
	}
	if (todas > imp || n_sin) {
		pon(v, R_APARTADO, 0, "Lo que no hace falta");
		for (k = 0; k < i->n_a; k++)
			if (i->a[k].sin_importancia)
				pon(v, R_GRIS, 1, "%s: %s%s", i->a[k].nombre, saber_motivo(i->a[k].sin_importancia),
				    i->a[k].n_faltan ? " (aunque le falte algo, no importa)." : ".");
		grupo(v, i, 0);
	}
	for (k = 0; k < i->n_a; k++)
		if (i->a[k].clase == A_NO_PE)
			pon(v, R_GRIS, 0, "%s no es un programa de Windows de 32 bits (puede ser de 16 bits o de 64).", i->a[k].nombre);
	return 0;
}

char *vista_texto(const vista *v)
{
	bufer b;
	int k;

	memset(&b, 0, sizeof b);
	for (k = 0; k < v->n; k++) {
		const renglon *r = &v->r[k];
		int s;

		if (r->clase == R_APARTADO)
			buf_txt(&b, "\n");
		for (s = 0; s < r->sangria; s++)
			buf_txt(&b, "  ");
		if (r->clase == R_BIEN)
			buf_txt(&b, "[BIEN] ");
		else if (r->clase == R_MAL)
			buf_txt(&b, "[FALTA] ");
		else if (r->clase == R_APARTADO)
			buf_txt(&b, "== ");
		buf_txt(&b, r->t);
		buf_txt(&b, "\n");
	}
	buf_c(&b, 0);
	return (char *)b.d;
}
