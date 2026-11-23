/* historia.c - (COMPAT) lo comprobado de cada usuario, en su carpeta
 * (\USUARIOS\<USUARIO>\COMPAT; lo suyo no lo ve nadie más):
 *   PROGRAMA.LST  un programa por línea, con tabuladores: la ruta, el
 *                 nombre, la versión de win32, cuántas faltan, cuántas
 *                 importantes, la fecha y su número (UTF-8)
 *   P<número>.TXT lo que le faltaba la última vez, una clave por línea
 *                 («WINMM.DLL!mixerOpen»; «A3D.DLL» si es la DLL entera)
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "compat.h"
#include "plat.h"
#include "texto.h"

#define MAX_PROGRAMAS 200

int hist_carpeta(char *d, u32 cabe)
{
	char u[RUTA_MAX];

	if (plat_carpeta_usuario(u, sizeof u))
		return -1;
	une_ruta(d, cabe, u, "COMPAT");
	if (so_existe(d) != 2)
		plat_haz_carpeta(d);
	return so_existe(d) == 2 ? 0 : -1;
}

void hist_id(const char *ruta, char id[9])
{
	u32 h = 2166136261u;
	char m[RUTA_MAX];
	const char *p;

	copia(m, ruta, sizeof m);
	a_mayusculas(m);
	for (p = m; *p; p++)
		h = (h ^ (u8)*p) * 16777619u;
	fmt(id, 9, "%08X", h);
}

void hist_nombre(const char *ruta, char *d, u32 cabe)
{
	char r[RUTA_MAX], ext[8];
	u32 n;

	copia(r, ruta, sizeof r);
	n = largo(r);
	while (n > 3 && (r[n - 1] == '\\' || r[n - 1] == '/'))
		r[--n] = 0;
	copia(d, sin_carpeta(r), cabe);
	extension(d, ext);
	if (iguales(ext, "EXE")) {
		n = largo(d);
		if (n > 4)
			d[n - 4] = 0;
	}
	if (!d[0])
		copia(d, r, cabe);
}

static void campo(const char **p, char *d, u32 cabe)
{
	u32 j = 0;

	while (**p && **p != '\t' && **p != '\n' && **p != '\r') {
		if (j + 1 < cabe)
			d[j++] = **p;
		(*p)++;
	}
	d[j] = 0;
	if (**p == '\t')
		(*p)++;
}

int hist_lee(c_programa **pp, int *n)
{
	char c[RUTA_MAX], r[RUTA_MAX], t[24];
	char *b;
	const char *p;
	c_programa *v;
	u32 nb;

	*pp = NULL;
	*n = 0;
	if (hist_carpeta(c, sizeof c))
		return -1;
	une_ruta(r, sizeof r, c, "PROGRAMA.LST");
	v = so_pide(sizeof *v * MAX_PROGRAMAS);
	if (!v)
		return -1;
	*pp = v;
	b = lee_texto(r, &nb, 1u << 20);
	if (!b)
		return 0;
	for (p = b; *p && *n < MAX_PROGRAMAS;) {
		c_programa *x = &v[*n];

		memset(x, 0, sizeof *x);
		campo(&p, x->ruta, sizeof x->ruta);
		campo(&p, x->nombre, sizeof x->nombre);
		campo(&p, x->win32, sizeof x->win32);
		campo(&p, t, sizeof t);
		x->faltan = (u32)numero(t, NULL);
		campo(&p, t, sizeof t);
		x->importantes = (u32)numero(t, NULL);
		campo(&p, x->fecha, sizeof x->fecha);
		campo(&p, x->id, sizeof x->id);
		while (*p && *p != '\n')
			p++;
		while (*p == '\n' || *p == '\r')
			p++;
		if (x->ruta[0] && largo(x->id) == 8)
			(*n)++;
	}
	so_suelta(b);
	return 0;
}

int hist_guarda(const c_programa *v, int n)
{
	char c[RUTA_MAX], r[RUTA_MAX];
	bufer b;
	int k, e;

	if (hist_carpeta(c, sizeof c))
		return -1;
	memset(&b, 0, sizeof b);
	for (k = 0; k < n; k++)
		buf_fmt(&b, "%s\t%s\t%s\t%u\t%u\t%s\t%s\r\n", v[k].ruta, v[k].nombre, v[k].win32, v[k].faltan, v[k].importantes, v[k].fecha, v[k].id);
	une_ruta(r, sizeof r, c, "PROGRAMA.LST");
	if (b.mal) {
		buf_suelta(&b);
		return -1;
	}
	e = escribe_texto(r, b.d ? (const char *)b.d : "", b.n);
	buf_suelta(&b);
	return e;
}

int hist_apunta(const char *ruta, const char *nombre, const char *win32, const c_informe *i)
{
	c_programa *v;
	int n, k, e;
	char c[RUTA_MAX], r[RUTA_MAX], id[9], f[100];
	int ano, mes, dia, h, mi, s;
	bufer b;

	if (hist_lee(&v, &n) || !v)
		return -1;
	hist_id(ruta, id);
	for (k = 0; k < n; k++)
		if (iguales(v[k].id, id))
			break;
	if (k == n) {
		if (n >= MAX_PROGRAMAS) {
			/* (lleno: se va el más viejo, el primero) */
			memmove(v, v + 1, sizeof *v * (u32)(n - 1));
			k = n - 1;
		} else
			n++;
		memset(&v[k], 0, sizeof v[k]);
	}
	copia(v[k].ruta, ruta, sizeof v[k].ruta);
	if (nombre && nombre[0])
		copia(v[k].nombre, nombre, sizeof v[k].nombre);
	else if (!v[k].nombre[0])
		hist_nombre(ruta, v[k].nombre, sizeof v[k].nombre);
	copia(v[k].win32, win32, sizeof v[k].win32);
	v[k].faltan = inf_faltan(i, 0);
	v[k].importantes = inf_faltan(i, 1);
	so_ahora(&ano, &mes, &dia, &h, &mi, &s);
	fmt(v[k].fecha, sizeof v[k].fecha, "%02d/%02d/%04d %02d:%02d", dia, mes, ano, h, mi);
	copia(v[k].id, id, sizeof v[k].id);
	e = hist_guarda(v, n);
	so_suelta(v);
	if (e)
		return e;
	/* lo que falta, sin repetir */
	memset(&b, 0, sizeof b);
	for (k = 0; k < i->n_f; k++) {
		int j, repe = 0;

		inf_clave(&i->f[k], f, sizeof f);
		for (j = 0; j < k && !repe; j++) {
			char g[100];

			inf_clave(&i->f[j], g, sizeof g);
			repe = iguales_sin_caja(f, g);
		}
		if (!repe)
			buf_fmt(&b, "%s\r\n", f);
	}
	if (hist_carpeta(c, sizeof c)) {
		buf_suelta(&b);
		return -1;
	}
	fmt(r, sizeof r, "%s\\P%s.TXT", c, id);
	e = b.mal ? -1 : escribe_texto(r, b.d ? (const char *)b.d : "", b.n);
	buf_suelta(&b);
	return e;
}

char *hist_faltas(const char *id)
{
	char c[RUTA_MAX], r[RUTA_MAX];

	if (hist_carpeta(c, sizeof c))
		return NULL;
	fmt(r, sizeof r, "%s\\P%s.TXT", c, id);
	return lee_texto(r, NULL, 4u << 20);
}

int hist_olvida(const char *ruta)
{
	c_programa *v;
	int n, k, e;
	char id[9], c[RUTA_MAX], r[RUTA_MAX];

	if (hist_lee(&v, &n) || !v)
		return -1;
	hist_id(ruta, id);
	for (k = 0; k < n; k++)
		if (iguales(v[k].id, id)) {
			memmove(v + k, v + k + 1, sizeof *v * (u32)(n - k - 1));
			n--;
			break;
		}
	e = hist_guarda(v, n);
	so_suelta(v);
	if (!hist_carpeta(c, sizeof c)) {
		fmt(r, sizeof r, "%s\\P%s.TXT", c, id);
		so_borra(r);
	}
	return e;
}
