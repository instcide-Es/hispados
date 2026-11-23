/* informe.c - (COMPAT) leer lo que escribe «WIN32 COMPRUEBA» (castel/nucleo32/
 * win32/diagnostico.c). Dos formas:
 *  - la de las personas (la de hoy), por renglones:
 *      HL.EXE: programa de ventanas
 *        FALTA LA DLL A3D.DLL (12 funciones)[: está, pero no es de 32 bits]
 *        falta WINMM.DLL!mixerOpen
 *        -> 9 DLL, 412 funciones: faltan 3.      (o «: está todo.»)
 *      C:\X\Y.EXE: no se encuentra.
 *      Y.DLL: no es un programa de Windows de 32 bits (PE de 386).
 *      En total: 12 archivos; faltan 142 funciones.
 *  - la de las máquinas (WIN32 COMPRUEBA ... /MAQUINA, win32 1.3:
 *    docs/pentium/WIN32.md), una línea por hecho, con tabuladores:
 *      PROGRAMA <carpeta del programa>
 *      ARCHIVO <ruta> <ventanas|consola|dll|otro|no-pe> <tamaño> <sha256>
 *      NOHAY <ruta>
 *      USA <archivo> <dll> <de dónde>
 *      FALTA <archivo> <dll> <función o #número>
 *      FALTADLL <archivo> <dll> <funciones>
 *      RESUMEN <archivo> <dll> <funciones> <faltan>
 *      TOTAL <archivos> <faltan> <dll que faltan>
 * Lo que no se entiende se salta (una versión de WIN32 con algo más no lo
 * rompe).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "compat.h"
#include "texto.h"

void inf_vacia(c_informe *i) { memset(i, 0, sizeof *i); }

void inf_suelta(c_informe *i)
{
	so_suelta(i->a);
	so_suelta(i->f);
	memset(i, 0, sizeof *i);
}

static c_archivo *nuevo_archivo(c_informe *i)
{
	if (i->n_a == i->cabe_a) {
		int c = i->cabe_a ? i->cabe_a * 2 : 16;
		c_archivo *n = so_repide(i->a, sizeof *n * (u32)c);

		if (!n)
			return NULL;
		i->a = n;
		i->cabe_a = c;
	}
	memset(&i->a[i->n_a], 0, sizeof i->a[0]);
	return &i->a[i->n_a++];
}

static c_falta *nueva_falta(c_informe *i)
{
	if (!i->n_a)
		return NULL;
	if (i->n_f == i->cabe_f) {
		int c = i->cabe_f ? i->cabe_f * 2 : 64;
		c_falta *n = so_repide(i->f, sizeof *n * (u32)c);

		if (!n)
			return NULL;
		i->f = n;
		i->cabe_f = c;
	}
	memset(&i->f[i->n_f], 0, sizeof i->f[0]);
	i->f[i->n_f].archivo = i->n_a - 1;
	return &i->f[i->n_f++];
}

static u32 num(const char *s, const char **fin)
{
	u32 v = 0;

	while (*s == ' ')
		s++;
	while (*s >= '0' && *s <= '9')
		v = v * 10 + (u32)(*s++ - '0');
	if (fin)
		*fin = s;
	return v;
}

/* el trozo k (desde 0) de una línea con tabuladores */
static void campo(const char *l, int k, char *d, u32 cabe)
{
	u32 j = 0;

	while (k > 0 && *l) {
		if (*l++ == '\t')
			k--;
	}
	while (*l && *l != '\t' && j + 1 < cabe)
		d[j++] = *l++;
	d[j] = 0;
}

static void base_de(const char *r, char *d, u32 cabe)
{
	const char *b = r, *p;

	for (p = r; *p; p++)
		if (*p == '\\' || *p == '/' || *p == ':')
			b = p + 1;
	copia(d, b, cabe);
}

/* el archivo de esa ruta (el último con ella: el que se está mirando), o -1 */
static int archivo_de(c_informe *i, const char *ruta)
{
	int k;

	for (k = i->n_a - 1; k >= 0; k--)
		if (iguales_sin_caja(i->a[k].ruta, ruta))
			return k;
	return -1;
}

static int es_hex64(const char *s)
{
	int k;

	for (k = 0; k < 64; k++)
		if (!((s[k] >= '0' && s[k] <= '9') || (s[k] >= 'a' && s[k] <= 'f') || (s[k] >= 'A' && s[k] <= 'F')))
			return 0;
	return !s[64];
}

static void linea_maquina(c_informe *i, const char *l)
{
	char t[RUTA_MAX], r[RUTA_MAX];

	campo(l, 0, t, sizeof t);
	if (iguales(t, "ARCHIVO") || iguales(t, "NOHAY")) {
		c_archivo *a = nuevo_archivo(i);

		if (!a)
			return;
		campo(l, 1, a->ruta, sizeof a->ruta);
		base_de(a->ruta, a->nombre, sizeof a->nombre);
		if (iguales(t, "NOHAY")) {
			a->clase = A_NO_ESTA;
			return;
		}
		campo(l, 2, t, sizeof t);
		a->clase = iguales(t, "ventanas") ? A_VENTANAS : iguales(t, "consola") ? A_CONSOLA : iguales(t, "dll") ? A_DLL :
			   iguales(t, "no-pe") ? A_NO_PE : A_PROGRAMA;
		campo(l, 3, t, sizeof t);
		a->tam = num(t, NULL);
		campo(l, 4, t, sizeof t);
		if (es_hex64(t)) {
			u32 k;

			for (k = 0; k < 64; k++)
				a->sha[k] = (char)minuscula((u8)t[k]);
			a->sha[64] = 0;
		}
	} else if (iguales(t, "FALTA") || iguales(t, "FALTADLL")) {
		int k;
		c_falta *f;

		campo(l, 1, r, sizeof r);
		k = archivo_de(i, r);
		if (k < 0)
			return;
		f = nueva_falta(i);
		if (!f)
			return;
		f->archivo = k;
		campo(l, 2, f->dll, sizeof f->dll);
		a_mayusculas(f->dll);
		campo(l, 3, t, sizeof t);
		if (!empieza(l, "FALTADLL")) {
			copia(f->fn, t[0] ? t : "?", sizeof f->fn);
		} else
			f->n = num(t, NULL);
	} else if (iguales(t, "RESUMEN")) {
		int k;

		campo(l, 1, r, sizeof r);
		k = archivo_de(i, r);
		if (k < 0)
			return;
		campo(l, 2, t, sizeof t);
		i->a[k].n_dll = num(t, NULL);
		campo(l, 3, t, sizeof t);
		i->a[k].n_fun = num(t, NULL);
		campo(l, 4, t, sizeof t);
		i->a[k].n_faltan = num(t, NULL);
	} else if (iguales(t, "TOTAL")) {
		campo(l, 2, t, sizeof t);
		i->total_faltan = num(t, NULL);
		i->leido_total = 1;
	}
	/* (PROGRAMA y USA, y lo que venga después, no hacen falta aquí) */
}

/* ¿es una línea de /MAQUINA? (una palabra suya y un tabulador) */
static int de_maquina(const char *l)
{
	static const char *const P[] = { "PROGRAMA\t", "ARCHIVO\t", "NOHAY\t", "USA\t", "FALTA\t", "FALTADLL\t", "RESUMEN\t", "TOTAL\t" };
	u32 k;

	for (k = 0; k < sizeof P / sizeof P[0]; k++)
		if (empieza(l, P[k]))
			return 1;
	return 0;
}

static void linea_persona(c_informe *i, const char *l)
{
	const char *p;

	if (empieza(l, "WIN32 COMPRUEBA") || l[0] == '(' || empieza(l, "No hay nada"))
		return;
	if (empieza(l, "En total:")) {
		i->leido_total = 1;
		p = busca(l, "falta");
		if (p) {
			while (*p && (*p < '0' || *p > '9'))
				p++;
			i->total_faltan = num(p, NULL);
		}
		return;
	}
	if (empieza(l, "  FALTA LA DLL ")) {
		c_falta *f = nueva_falta(i);
		u32 j = 0;

		if (!f)
			return;
		p = l + 15;
		while (*p && *p != ' ' && j + 1 < sizeof f->dll)
			f->dll[j++] = *p++;
		f->dll[j] = 0;
		if (*p == ' ' && p[1] == '(')
			f->n = num(p + 2, NULL);
		f->no32 = busca(p, "no es de 32 bits") != NULL;
		return;
	}
	if (empieza(l, "  falta ")) {
		c_falta *f = nueva_falta(i);
		const char *x;
		u32 j = 0;

		if (!f)
			return;
		p = l + 8;
		x = strchr(p, '!');
		if (!x) {
			i->n_f--;
			return;
		}
		while (p < x && j + 1 < sizeof f->dll)
			f->dll[j++] = *p++;
		f->dll[j] = 0;
		copia(f->fn, x + 1, sizeof f->fn);
		return;
	}
	if (empieza(l, "  -> ")) {
		c_archivo *a;

		if (!i->n_a)
			return;
		a = &i->a[i->n_a - 1];
		a->n_dll = num(l + 5, &p);
		p = busca(p, ", ");
		if (p)
			a->n_fun = num(p + 2, &p);
		a->n_faltan = 0;
		if (p && (p = busca(p, ": falta")) != NULL) {
			while (*p && (*p < '0' || *p > '9'))
				p++;
			a->n_faltan = num(p, NULL);
		}
		return;
	}
	if (l[0] != ' ' && (p = busca(l, ": ")) != NULL) {
		c_archivo *a = nuevo_archivo(i);
		char r[RUTA_MAX];
		u32 n = (u32)(p - l);

		if (!a)
			return;
		if (n >= sizeof r)
			n = sizeof r - 1;
		memcpy(r, l, n);
		r[n] = 0;
		/* («C:\X\Y.EXE: no se encuentra.»: el «C:» no es el final del nombre) */
		if (n == 1 && l[1] == ':' && l[2] == '\\') {
			const char *q = busca(l + 2, ": ");

			if (q) {
				n = (u32)(q - l);
				if (n >= sizeof r)
					n = sizeof r - 1;
				memcpy(r, l, n);
				r[n] = 0;
				p = q;
			}
		}
		base_de(r, a->nombre, sizeof a->nombre);
		if (strchr(r, '\\'))
			copia(a->ruta, r, sizeof a->ruta);
		p += 2;
		a->clase = empieza(p, "DLL") ? A_DLL : empieza(p, "programa de ventanas") ? A_VENTANAS :
			   empieza(p, "programa de consola") ? A_CONSOLA : empieza(p, "no se encuentra") ? A_NO_ESTA :
			   empieza(p, "no es un programa") ? A_NO_PE : A_PROGRAMA;
	}
}

int inf_lee(c_informe *i, const char *t, u32 n)
{
	u32 k = 0;
	char l[600];

	while (k < n) {
		u32 j = 0;

		while (k < n && t[k] != '\n' && t[k] != '\r') {
			if (j + 1 < sizeof l)
				l[j++] = t[k];
			k++;
		}
		l[j] = 0;
		while (k < n && (t[k] == '\n' || t[k] == '\r'))
			k++;
		if (!j)
			continue;
		if (!i->maquina && de_maquina(l))
			i->maquina = 1;
		if (i->maquina)
			linea_maquina(i, l);
		else
			linea_persona(i, l);
	}
	/* (lo que dice cada archivo de sus faltas, si no lo ha dicho) */
	{
		int a;

		for (a = 0; a < i->n_a; a++)
			if (!i->a[a].n_faltan) {
				int k2;
				u32 c = 0;

				for (k2 = 0; k2 < i->n_f; k2++)
					if (i->f[k2].archivo == a)
						c += i->f[k2].fn[0] ? 1 : i->f[k2].n;
				i->a[a].n_faltan = c;
			}
	}
	return i->n_a ? 0 : -1;
}

void inf_clave(const c_falta *f, char *d, u32 cabe)
{
	if (f->fn[0])
		fmt(d, cabe, "%s!%s", f->dll, f->fn);
	else
		fmt(d, cabe, "%s", f->dll);
}

/* ¿no cuenta, si solo se cuentan las importantes? */
static int fuera(const c_informe *i, const c_falta *f, int solo_importantes)
{
	const c_saber *s;

	if (!solo_importantes)
		return 0;
	if (i->a[f->archivo].sin_importancia)
		return 1;
	s = saber_dll(f->dll);
	return s && s->sin_importancia;
}

u32 inf_faltan(const c_informe *i, int solo_importantes)
{
	u32 n = 0;
	int k, j;

	for (k = 0; k < i->n_f; k++) {
		const c_falta *f = &i->f[k];
		int repe = 0;
		u32 max = f->n ? f->n : 1;

		if (fuera(i, f, solo_importantes))
			continue;
		for (j = 0; j < k && !repe; j++)
			if (!fuera(i, &i->f[j], solo_importantes) && iguales_sin_caja(i->f[j].dll, f->dll) && iguales(i->f[j].fn, f->fn))
				repe = 1;
		if (repe)
			continue;
		if (f->fn[0]) {
			n++;
			continue;
		}
		/* (la DLL entera: lo más que le pide uno) */
		for (j = k + 1; j < i->n_f; j++)
			if (!fuera(i, &i->f[j], solo_importantes) && !i->f[j].fn[0] && iguales_sin_caja(i->f[j].dll, f->dll) && i->f[j].n > max)
				max = i->f[j].n;
		n += max;
	}
	return n;
}
