/* prueba_dibujo.c - Cuentas 2: la prueba del dibujo (pintor.c, celdas.c y
 * grafico.c) en el anfitrión: pinta en un lienzo de memoria celdas con
 * tramas, bordes, combinadas, lo que se sale, el formato condicional y un
 * comentario, y los cinco gráficos; guarda las capturas (.ppm) en la carpeta
 * que se le diga y comprueba cosas sencillas. Se enlaza con los objetos de
 * Cuentas (sin principal.c): pruebas/pdf.sh lo hace.
 *   prueba_dibujo CARPETA
 * Acaba con «== N bien, M mal».
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdio.h>
#include <string.h>
#include "libro.h"
#include "operar.h"
#include "calculo.h"
#include "celdas.h"
#include "grafico.h"
#include "letras.h"
#include "f_pdf.h"
#include "deshacer.h"

const char ofi_programa[] = "PRUEBA.BIN";
static int bien, mal;
static char carpeta[400];

static void mira(int si, const char *que)
{
	if (si)
		bien++;
	else {
		mal++;
		dice("mal: %s", que);
	}
}

static void guarda(const lienzo *l, const char *nombre)
{
	char r[600];
	FILE *f;
	int x, y;

	snprintf(r, sizeof r, "%s/%s.ppm", carpeta, nombre);
	f = fopen(r, "wb");
	if (!f)
		return;
	fprintf(f, "P6\n%d %d\n255\n", l->an, l->al);
	for (y = 0; y < l->al; y++)
		for (x = 0; x < l->an; x++) {
			u32 c = l->p[y * l->paso + x];
			unsigned char b[3] = { (unsigned char)(c >> 16), (unsigned char)(c >> 8), (unsigned char)c };

			fwrite(b, 1, 3, f);
		}
	fclose(f);
}

/* cuántos puntos no blancos hay en un trozo */
static int tinta(const lienzo *l, int x0, int y0, int an, int al)
{
	int x, y, n = 0;

	for (y = y0; y < y0 + al && y < l->al; y++)
		for (x = x0; x < x0 + an && x < l->an; x++)
			if (x >= 0 && y >= 0 && (l->p[y * l->paso + x] & 0xFFFFFF) != 0xFFFFFF)
				n++;
	return n;
}

static void pon(libro *l, hoja *h, const char *ref, const char *t)
{
	u32 f, c;
	char e[200];

	if (cu_lee_ref(ref, &f, &c, NULL))
		op_entrada(l, h, f, c, t, e, sizeof e);
}

static void estilo_de(libro *l, hoja *h, const char *ref, void (*cambia)(estilo *))
{
	rango r;
	u32 f, c;

	if (!cu_lee_rango(ref, &r, NULL))
		return;
	for (f = r.f0; f <= r.f1; f++)
		for (c = r.c0; c <= r.c1; c++) {
			estilo e = *lib_estilo(l, hj_estilo(h, f, c));

			cambia(&e);
			lib_pon_estilo(l, h, f, c, lib_estilo_id(l, &e));
		}
}

static void s_titulo(estilo *e) { e->negrita = 1; e->tam = 28; }
static void s_cabecera(estilo *e) { e->negrita = 1; e->trama = TR_DIAG_ARRIBA; e->fondo = 0xDCE6F5; e->color_trama = 0x5A7DB0; e->borde[BO_ABAJO] = BR_GRUESO; }
static void s_bordes(estilo *e) { int i; for (i = 0; i < 4; i++) e->borde[i] = BR_FINO; }
static void s_doble(estilo *e) { e->borde[BO_ARRIBA] = BR_DOBLE; e->negrita = 1; }
static void s_tramas(estilo *e) { e->trama = TR_CUADRICULA; e->fondo = 0xFFF4D6; e->color_trama = 0x9E7B2B; }
static void s_puntos(estilo *e) { e->borde[BO_ABAJO] = BR_PUNTOS; e->borde[BO_DER] = BR_GUIONES; e->cursiva = 1; e->subrayado = 1; }
static void s_centro(estilo *e) { e->alin_h = AH_CENTRO; e->alin_v = AV_CENTRO; e->ajustar = 1; }

int ofi_principal(const char *args)
{
	libro *l;
	hoja *h;
	lienzo lz;
	pintor p;
	trozo_hoja t;
	int i, j;

	copia(carpeta, args && args[0] ? args : "/tmp/cuentas-b/capturas", sizeof carpeta);
	letras_inicia();
	l = lib_nuevo(1);
	h = l->h[0];
	/* ---- las celdas ---- */
	pon(l, h, "A1", "Cuentas 2: la hoja de cálculo");
	estilo_de(l, h, "A1", s_titulo);
	pon(l, h, "A3", "Mes");
	pon(l, h, "B3", "Norte");
	pon(l, h, "C3", "Sur");
	pon(l, h, "D3", "Este");
	estilo_de(l, h, "A3:D3", s_cabecera);
	{
		static const char *const m[] = { "Enero", "Febrero", "Marzo", "Abril", "Mayo", "Junio" };

		for (i = 0; i < 6; i++) {
			char r[8], v[16];

			snprintf(r, sizeof r, "A%d", i + 4);
			pon(l, h, r, m[i]);
			for (j = 0; j < 3; j++) {
				snprintf(r, sizeof r, "%c%d", 'B' + j, i + 4);
				snprintf(v, sizeof v, "%d", 800 + (i * 137 + j * 211) % 900);
				pon(l, h, r, v);
			}
		}
	}
	pon(l, h, "A10", "Total");
	pon(l, h, "B10", "=SUMA(B4:B9)");
	pon(l, h, "C10", "=SUMA(C4:C9)");
	pon(l, h, "D10", "=SUMA(D4:D9)");
	estilo_de(l, h, "A4:D9", s_bordes);
	estilo_de(l, h, "A10:D10", s_doble);
	pon(l, h, "F3", "Un texto largo que se sale por la derecha sobre las celdas vacías");
	pon(l, h, "F5", "123456789012345");
	op_ancho(l, h, 5, 5, 40);
	pon(l, h, "F7", "Trama");
	estilo_de(l, h, "F7:G8", s_tramas);
	pon(l, h, "F10", "Subrayado");
	estilo_de(l, h, "F10", s_puntos);
	pon(l, h, "B12", "Combinadas, centradas y en dos líneas");
	op_combina(l, h, cu_rango(11, 1, 12, 3), 1);
	estilo_de(l, h, "B12", s_centro);
	op_comentario(l, h, 9, 0, "La suma");
	{
		condicional k;

		memset(&k, 0, sizeof k);
		k.r = cu_rango(3, 1, 8, 3);
		k.cond = FC_MAYOR;
		k.v1 = 1500;
		k.simbolo = SIM_ARRIBA;
		k.negrita = 1;
		k.color = 0x9E2B2B;
		k.fondo = 0xFFE0E0;
		k.trama = TR_SOLIDO;
		op_condicional(l, h, &k);
		k.cond = FC_MENOR;
		k.v1 = 900;
		k.simbolo = SIM_ABAJO;
		k.fondo = CU_AUTO;
		k.color = 0x1F4E9E;
		op_condicional(l, h, &k);
	}
	calc_recalcula(l);
	/* ---- pintar ---- */
	if (!lz_nuevo(&lz, 900, 330))
		return 1;
	pt_de_lienzo(&p, &lz);
	memset(&t, 0, sizeof t);
	t.l = l;
	t.h = h;
	t.f0 = 0;
	t.f1 = 15;
	t.c0 = 0;
	t.c1 = 9;
	t.x = 0;
	t.y = 0;
	t.esc = 1;
	t.rejilla = 1;
	t.comentarios = 1;
	cel_pinta(&p, &t);
	guarda(&lz, "celdas");
	/* lo que se sale: F3 sigue por G, H... (F mide 40) */
	{
		int xg = 88 * 5 + 40;	/* donde empieza G */

		mira(tinta(&lz, xg + 2, 2 * CU_ALTO + 4, 80, CU_ALTO - 8) > 30, "el texto de F3 no se sale a G3");
		/* un número que no cabe: almohadillas, no cortado (F5) */
		mira(tinta(&lz, 88 * 5 + 2, 4 * CU_ALTO + 3, 36, CU_ALTO - 6) > 10, "F5 no pinta nada");
		mira(tinta(&lz, xg + 3, 4 * CU_ALTO + 5, 70, CU_ALTO - 10) < 5, "el número de F5 se sale a G5");
		/* la trama de F7: hay de los dos colores */
		{
			int x, y, fondo = 0, ink = 0;

			for (y = 6 * CU_ALTO + 2; y < 6 * CU_ALTO + 14; y++)
				for (x = 88 * 5 + 30; x < 88 * 5 + 38; x++) {
					u32 c = lz.p[y * lz.paso + x];

					if (c == 0xFFF4D6)
						fondo++;
					if (c == 0x9E7B2B)
						ink++;
				}
			mira(fondo > 10 && ink > 10, "la trama de F7 no tiene fondo y rayas");
		}
		/* el borde grueso de debajo de la cabecera (fila 3) */
		mira((lz.p[(3 * CU_ALTO - 1) * lz.paso + 100] & 0xFFFFFF) == 0, "no está el borde grueso de B3");
		/* el comentario de A10: el triángulo rojo arriba a la derecha */
		{
			u32 c = lz.p[(9 * CU_ALTO + 2) * lz.paso + 88 - 4];

			mira(((c >> 16) & 255) > 150 && (c & 255) < 120, "no está la marca del comentario de A10");
		}
	}
	/* ---- cada serie, su trama (distinta de las demás) ---- */
	{
		u32 dib[8][64];

		for (i = 0; i < 8; i++) {
			lienzo q;
			pintor pq;
			int x, y;

			lz_nuevo(&q, 8, 8);
			pt_de_lienzo(&pq, &q);
			pt_trama(&pq, 0, 0, 8, 8, gr_trama_serie(i), 0xFFFFFF, 0);
			for (y = 0; y < 8; y++)
				for (x = 0; x < 8; x++)
					dib[i][y * 8 + x] = q.p[y * q.paso + x];
			lz_suelta(&q);
		}
		for (i = 0; i < 8; i++)
			for (j = i + 1; j < 8; j++)
				mira(memcmp(dib[i], dib[j], sizeof dib[i]) != 0, "dos series con la misma trama");
		for (i = 0; i < 8; i++)
			for (j = i + 1; j < 8; j++)
				mira(gr_marcador_serie(i) != gr_marcador_serie(j), "dos series con el mismo marcador");
	}
	/* ---- los cinco gráficos ---- */
	{
		static const char *const nom[GR_N] = { "g_columnas", "g_barras", "g_lineas", "g_sectores", "g_dispersion" };

		for (i = 0; i < GR_N; i++) {
			grafico g;
			lienzo q;
			pintor pq;

			gr_nuevo(&g, h, cu_rango(2, 0, 8, 3), i);
			g.en_filas = 0;
			if (i == GR_DISPERSION)
				g.datos = cu_rango(2, 1, 8, 3);
			if (i == GR_COLUMNAS)
				g.apiladas = 0;
			fmt(g.titulo, sizeof g.titulo, "%s: ventas por mes", gr_nombre_tipo(i));
			if (i == GR_LINEAS) {
				copia(g.eje_x, "Mes", sizeof g.eje_x);
				copia(g.eje_y, "Euros", sizeof g.eje_y);
				g.leyenda = LY_ABAJO;
			}
			lz_nuevo(&q, 480, 300);
			pt_de_lienzo(&pq, &q);
			gr_pinta(&pq, l, &g, 0, 0, 480, 300, 1);
			guarda(&q, nom[i]);
			mira(tinta(&q, 60, 60, 300, 180) > 2000, nom[i]);
			/* en las de columnas, la segunda serie (trama de rayas): hay tinta oscura de la trama */
			if (i == GR_COLUMNAS || i == GR_SECTORES) {
				int x, y, osc = 0;

				for (y = 0; y < 300; y++)
					for (x = 0; x < 480; x++)
						if (q.p[y * q.paso + x] == 0x1E2633)
							osc++;
				mira(osc > 200, "las tramas de las series no se ven");
			}
			lz_suelta(&q);
		}
		/* apiladas */
		{
			grafico g;
			lienzo q;
			pintor pq;

			gr_nuevo(&g, h, cu_rango(2, 0, 8, 3), GR_COLUMNAS);
			g.apiladas = 1;
			copia(g.titulo, "Columnas apiladas", sizeof g.titulo);
			lz_nuevo(&q, 480, 300);
			pt_de_lienzo(&pq, &q);
			gr_pinta(&pq, l, &g, 0, 0, 480, 300, 1);
			guarda(&q, "g_apiladas");
			mira(tinta(&q, 60, 60, 300, 180) > 2000, "apiladas");
			lz_suelta(&q);
		}
		/* sin datos: el aviso */
		{
			grafico g;
			lienzo q;
			pintor pq;

			gr_nuevo(&g, h, cu_rango(30, 0, 33, 2), GR_LINEAS);
			lz_nuevo(&q, 300, 200);
			pt_de_lienzo(&pq, &q);
			gr_pinta(&pq, l, &g, 0, 0, 300, 200, 1);
			mira(tinta(&q, 20, 80, 260, 40) > 50, "sin datos: no sale el aviso");
			lz_suelta(&q);
		}
	}
	/* ---- la vista preliminar: la página 1 en un lienzo ---- */
	{
		pdf_plan pl;
		lienzo q;
		pintor pq;

		if (pdf_plan_hoja(l, h, &pl) == 0) {
			lz_nuevo(&q, 620, 860);
			lz_rect(&q, 0, 0, 620, 860, 0x808080);
			pt_de_lienzo(&pq, &q);
			pdf_pinta_pagina(&pq, l, h, &pl, 0, 10, 10, 1, "PRUEBA.HOJ", 0, pl.n);
			guarda(&q, "preliminar");
			mira(pl.n >= 1 && tinta(&q, 60, 60, 300, 200) > 500, "la vista preliminar");
			lz_suelta(&q);
			pdf_plan_suelta(&pl);
		} else
			mira(0, "pdf_plan_hoja");
	}
	/* ---- lo que tarda pintar una pantalla (40 filas x 12 columnas) ---- */
	{
		u32 t0, k, n = 30;
		lienzo q;
		pintor pq;

		for (i = 0; i < 40; i++)
			for (j = 0; j < 12; j++) {
				char r[8], v[24];

				snprintf(r, sizeof r, "%c%d", 'A' + j, i + 100);
				snprintf(v, sizeof v, j % 3 ? "%d,%d" : "Fila %d.%d", i * 7, j);
				pon(l, h, r, v);
			}
		lz_nuevo(&q, 1100, 820);
		pt_de_lienzo(&pq, &q);
		t.f0 = 99;
		t.f1 = 139;
		t.c0 = 0;
		t.c1 = 11;
		t0 = so_tics();
		for (k = 0; k < n; k++)
			cel_pinta(&pq, &t);
		dice("prueba_dibujo: pintar 40 x 12 celdas: %u ms (en el anfitrión)", (so_tics() - t0) * 1000 / so_hz() / n);
		guarda(&q, "pantalla");
		lz_suelta(&q);
	}
	lz_suelta(&lz);
	dh_suelta(l);
	lib_suelta(l);
	dice("prueba_dibujo == %d bien, %d mal", bien, mal);
	return mal ? 1 : 0;
}
