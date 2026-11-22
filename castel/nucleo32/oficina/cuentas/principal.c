/* principal.c - Cuentas 2: el principio. Sin nada, o con un libro, la
 * ventana. Con órdenes (para las pruebas y para quien las quiera en la
 * Consola):
 *   CUENTAS --convierte ENTRADA SALIDA   (el formato de SALIDA, por su extensión:
 *                                        .HOJ, .XLSX, .ODS, .CSV o .PDF)
 *   CUENTAS --info ENTRADA               las hojas, las celdas, las fórmulas
 *   CUENTAS --valores ENTRADA            lo que se ve en cada celda, por el puerto serie
 *   CUENTAS --guion GUION                un guion de pruebas (prueba.c)
 *   CUENTAS --mide FORMULAS              lo que tarda recalcular y desplazarse
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "archivo.h"
#include "calculo.h"
#include "letras.h"
#include "cuentas.h"

const char ofi_programa[] = "CUENTAS.BIN";

const char *cu_arg(const char *s, char *d, u32 cabe)
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

static int convierte(const char *de, const char *a)
{
	char e[300], real[600];
	int f;
	libro *l = arch_abre(de, &f, e, sizeof e);

	if (!l) {
		dice("cuentas: %s", e);
		return 1;
	}
	if (arch_guarda(l, a, arch_formato_ext(a) ? arch_formato_ext(a) : FMT_HOJ, real, sizeof real, e, sizeof e)) {
		dice("cuentas: %s", e);
		lib_suelta(l);
		return 1;
	}
	dice("cuentas: %s (%s, %u hojas) -> %s", sin_carpeta(de), arch_nombre_formato(f), l->n_h, sin_carpeta(real));
	lib_suelta(l);
	return 0;
}

static int info(const char *de)
{
	char e[300];
	int f;
	u32 i, celdas = 0, formulas = 0, graficos = 0, comb = 0, com = 0, cond = 0;
	libro *l = arch_abre(de, &f, e, sizeof e);

	if (!l) {
		dice("cuentas: %s", e);
		return 1;
	}
	for (i = 0; i < l->n_h; i++) {
		hoja *h = l->h[i];
		u32 j;

		celdas += h->n_cel;
		for (j = 0; j < h->n_cel; j++)
			if (h->cel[j].f)
				formulas++;
		graficos += h->n_gr;
		comb += h->n_comb;
		com += h->n_com;
		cond += h->n_cond;
	}
	dice("cuentas: %s: %s; %u hojas, %u celdas, %u fórmulas, %u gráficos, %u combinadas, %u comentarios, %u condicionales, %u nombres, %u estilos",
	     sin_carpeta(de), arch_nombre_formato(f), l->n_h, celdas, formulas, graficos, comb, com, cond, l->n_nom, l->n_est);
	lib_suelta(l);
	return 0;
}

static int valores(const char *de)
{
	char e[300];
	int f;
	u32 i;
	libro *l = arch_abre(de, &f, e, sizeof e);
	bufer b = { 0 };

	if (!l) {
		dice("cuentas: %s", e);
		return 1;
	}
	for (i = 0; i < l->n_h; i++) {
		hoja *h = l->h[i];
		u32 n, j, *k = hj_claves_ordenadas(h, &n);

		buf_fmt(&b, "[%s]\n", h->nombre);
		for (j = 0; k && j < n; j++) {
			celda *x = hj_celda_k(h, k[j]);
			char r[16], t[400];

			if (!x || (x->v.t == V_VACIO && !x->f))
				continue;
			cu_ref_texto(CU_FILA(k[j]), CU_COL(k[j]), r);
			lib_texto_visible(l, x, t, sizeof t, NULL);
			buf_fmt(&b, "%s=%s\n", r, t);
		}
		so_suelta(k);
	}
	buf_c(&b, 0);
	if (b.d)
		so_dice((char *)b.d);
	buf_suelta(&b);
	lib_suelta(l);
	return 0;
}

int ofi_principal(const char *args)
{
	char a1[600], a2[600], a3[600];
	const char *s = args;

	letras_inicia();
	s = cu_arg(s, a1, sizeof a1);
	s = cu_arg(s, a2, sizeof a2);
	s = cu_arg(s, a3, sizeof a3);
	if (iguales_sin_caja(a1, "--convierte") || iguales_sin_caja(a1, "/CONVIERTE"))
		return convierte(a2, a3);
	if (iguales_sin_caja(a1, "--info") || iguales_sin_caja(a1, "/INFO"))
		return info(a2);
	if (iguales_sin_caja(a1, "--valores") || iguales_sin_caja(a1, "/VALORES"))
		return valores(a2);
	if (iguales_sin_caja(a1, "--guion") || iguales_sin_caja(a1, "/GUION"))
		return cuentas_guion(a2);
	if (iguales_sin_caja(a1, "--mide") || iguales_sin_caja(a1, "/MIDE"))
		return cuentas_mide(a2[0] ? (int)numero(a2, NULL) : 10000);
	return cuentas_ventana(a1[0] ? a1 : NULL);
}
