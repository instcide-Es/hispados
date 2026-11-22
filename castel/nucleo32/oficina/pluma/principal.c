/* principal.c - Pluma 2: el principio. Sin nada, o con un documento, la
 * ventana. Con órdenes (para las pruebas y para quien las quiera en la
 * Consola):
 *   PLUMA --convierte ENTRADA SALIDA     (el formato de SALIDA, por su extensión)
 *   PLUMA --texto ENTRADA                el texto, por el puerto serie
 *   PLUMA --info ENTRADA                 párrafos, páginas, tablas, imágenes...
 *   PLUMA --mide PAGINAS [LETRAS]        lo que tarda escribir en un documento de
 *                                        tantas páginas (maquetar y pintar)
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "archivo.h"
#include "maqueta.h"
#include "editar.h"
#include "letras.h"
#include "pluma.h"

const char ofi_programa[] = "PLUMA.BIN";

/* el argumento siguiente (con comillas, si las lleva); da dónde sigue */
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

static int convierte(const char *de, const char *a)
{
	char e[300], real[600];
	int f;
	documento *d = arch_abre(de, &f, e, sizeof e);

	if (!d) {
		dice("pluma: %s", e);
		return 1;
	}
	maq_documento(d);
	if (arch_guarda(d, a, arch_formato_ext(a) ? arch_formato_ext(a) : FMT_PLU, real, sizeof real, e, sizeof e)) {
		dice("pluma: %s", e);
		doc_suelta(d);
		return 1;
	}
	dice("pluma: %s (%s, %u páginas) -> %s", sin_carpeta(de), arch_extension(f), d->n_pag, sin_carpeta(real));
	doc_suelta(d);
	return 0;
}

static int info(const char *de)
{
	char e[300];
	int f;
	u32 pal, le, se, pa, tablas = 0, i, imgs = 0;
	documento *d = arch_abre(de, &f, e, sizeof e);

	if (!d) {
		dice("pluma: %s", e);
		return 1;
	}
	maq_documento(d);
	ed_cuenta(d, &pal, &le, &se, &pa);
	for (i = 0; i < d->n_tab; i++)
		if (d->tab[i].viva)
			tablas++;
	for (i = 0; i < d->n_par; i++) {
		u32 k;

		for (k = 0; k < d->par[i].n; k++)
			if (es_imagen(d->par[i].t[k]))
				imgs++;
	}
	dice("pluma: %s: %s; %u párrafos, %u páginas, %u palabras, %u letras, %u tablas, %u imágenes, %u estilos, %u formatos",
	     sin_carpeta(de), arch_nombre_formato(f), d->n_par, d->n_pag, pal, le, tablas, imgs, d->n_est, d->n_fl);
	doc_suelta(d);
	return 0;
}

static int texto(const char *de)
{
	char e[300];
	int f;
	bufer b = { 0 };
	documento *d = arch_abre(de, &f, e, sizeof e);

	if (!d) {
		dice("pluma: %s", e);
		return 1;
	}
	txt_escribe(d, &b, 1);
	buf_c(&b, 0);
	if (!b.mal)
		so_dice((char *)b.d);
	buf_suelta(&b);
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
	if (iguales_sin_caja(a1, "--convierte") || iguales_sin_caja(a1, "/CONVIERTE"))
		return convierte(a2, a3);
	if (iguales_sin_caja(a1, "--texto") || iguales_sin_caja(a1, "/TEXTO"))
		return texto(a2);
	if (iguales_sin_caja(a1, "--info") || iguales_sin_caja(a1, "/INFO"))
		return info(a2);
	if (iguales_sin_caja(a1, "--mide") || iguales_sin_caja(a1, "/MIDE"))
		return pluma_mide((int)numero(a2, NULL), a3[0] ? (int)numero(a3, NULL) : 200);
	return pluma_ventana(a1[0] ? a1 : NULL);
}
