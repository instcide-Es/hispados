/* iconos.c - la oficina de 32 bits: los iconos propios de las barras,
 * dibujados con rayas y polígonos suavizados a cualquier tamaño (así valen
 * igual con la letra normal que con la grande). Cada uno, con su forma y
 * un borde oscuro: se distinguen sin el color.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "ui.h"
#include "letras.h"

/* las coordenadas, en dieciseisavos del icono (0-16) */
static int X0, Y0, T, GRIS;
#define PX(v) (X0 * 16 + (v) * T)
#define PY(v) (Y0 * 16 + (v) * T)

static u32 tinta(u32 c)
{
	if (!GRIS)
		return c;
	{
		u32 r = c >> 16 & 255, g = c >> 8 & 255, b = c & 255, m = (r * 3 + g * 6 + b) / 10;

		m = 150 + m * 70 / 255;
		return m << 16 | m << 8 | m;
	}
}

static void pol(lienzo *l, const int *v, int n, u32 c)
{
	int xy[32], k;

	for (k = 0; k < n && k < 16; k++) {
		xy[2 * k] = PX(v[2 * k]);
		xy[2 * k + 1] = PY(v[2 * k + 1]);
	}
	lz_poligono(l, xy, n, tinta(c));
}

static void raya(lienzo *l, int x0, int y0, int x1, int y1, int g, u32 c)
{
	lz_linea(l, PX(x0), PY(y0), PX(x1), PY(y1), g * T, tinta(c));
}

static void caja(lienzo *l, int x0, int y0, int x1, int y1, u32 relleno, u32 borde)
{
	int a[8] = { x0, y0, x1, y0, x1, y1, x0, y1 };
	pol(l, a, 4, borde);
	/* lo de dentro, un punto más pequeño por cada lado */
	{
		int xy[8] = { PX(x0) + 16, PY(y0) + 16, PX(x1) - 16, PY(y0) + 16, PX(x1) - 16, PY(y1) - 16, PX(x0) + 16, PY(y1) - 16 };

		lz_poligono(l, xy, 4, tinta(relleno));
	}
}

static void letra(lienzo *l, const char *t, int negrita, int cursiva, int serif, u32 c, int esc)
{
	int cara = letra_cara(serif ? FAM_SERIF : FAM_SANS, negrita, 0, NULL);
	int tam = T * 4 * esc / 100, w = lz_ancho(cara, tam, cursiva ? FALSA_CURSIVA : 0, t);

	lz_texto(l, cara, tam, cursiva ? FALSA_CURSIVA : 0, X0 + (T - w) / 2, Y0 + T * 13 / 16, t, tinta(c));
}

static void lineas(lienzo *l, int alin, int y0)
{
	int k;

	for (k = 0; k < 4; k++) {
		int y = y0 + k * 3, a = 2, b = 14;

		if (k % 2 == 1 && alin != 3) {
			if (alin == 0)
				b = 10;
			else if (alin == 1)
				a = 4, b = 12;
			else
				a = 6;
		}
		raya(l, a, y, b, y, 2, 0x26313F);
	}
}

static void hoja(lienzo *l, int x0, int y0, int x1, int y1)
{
	int a[10] = { x0, y0, x1 - 3, y0, x1, y0 + 3, x1, y1, x0, y1 };
	int b[6] = { x1 - 3, y0, x1 - 3, y0 + 3, x1, y0 + 3 };

	pol(l, a, 5, 0x3B4656);
	{
		int c[10] = { x0 + 1, y0 + 1, x1 - 3, y0 + 1, x1 - 1, y0 + 3, x1 - 1, y1 - 1, x0 + 1, y1 - 1 };

		pol(l, c, 5, 0xFFFFFF);
	}
	pol(l, b, 3, 0xB7C2D0);
}

static void flecha_curva(lienzo *l, int derecha)
{
	/* un arco (con trozos rectos) y su punta */
	static const int ax[] = { 4, 5, 7, 9, 11, 12, 12 };
	static const int ay[] = { 7, 5, 4, 4, 5, 7, 10 };
	int k;
	u32 c = 0x2563C9;

	for (k = 0; k + 1 < 7; k++) {
		int x0 = derecha ? 16 - ax[k] : ax[k], x1 = derecha ? 16 - ax[k + 1] : ax[k + 1];

		raya(l, x0, ay[k], x1, ay[k + 1], 2, c);
	}
	{
		int bx = derecha ? 12 : 4;
		int p[6] = { bx - 3, 6, bx + 3, 6, bx, 11 };

		if (derecha) {
			p[0] = bx - 3; p[2] = bx + 3; p[4] = bx;
		}
		pol(l, p, 3, c);
	}
}

static void lupa(lienzo *l, int cx, int cy, int r)
{
	lz_circulo(l, PX(cx), PY(cy), r * T, tinta(0x26313F));
	lz_circulo(l, PX(cx), PY(cy), r * T - 2 * 16, tinta(0xEAF2FF));
	raya(l, cx + r * 7 / 10, cy + r * 7 / 10, cx + r * 7 / 10 + 4, cy + r * 7 / 10 + 4, 3, 0x26313F);
}

void ui_icono(lienzo *l, int ic, int x, int y, int t, int apagado)
{
	X0 = x;
	Y0 = y;
	T = t;
	GRIS = apagado;
	switch (ic) {
	case IC_NUEVO:
		hoja(l, 3, 1, 13, 15);
		break;
	case IC_ABRIR: {
		int a[12] = { 1, 4, 6, 4, 7, 5, 14, 5, 14, 14, 1, 14 };
		int b[8] = { 3, 8, 16, 8, 14, 14, 1, 14 };
		int c2[8] = { 4, 9, 15, 9, 13, 13, 2, 13 };

		pol(l, a, 6, 0x6B4E12);
		pol(l, b, 4, 0x6B4E12);
		pol(l, c2, 4, 0xF2C14E);
		break;
	}
	case IC_GUARDAR:
		caja(l, 1, 1, 15, 15, 0x2F5FAF, 0x12264A);
		caja(l, 4, 1, 12, 6, 0xE3E8EF, 0x12264A);
		caja(l, 9, 2, 11, 5, 0x2F5FAF, 0x2F5FAF);
		caja(l, 3, 9, 13, 15, 0xFFFFFF, 0x12264A);
		break;
	case IC_IMPRIMIR:
		caja(l, 4, 1, 12, 7, 0xFFFFFF, 0x26313F);
		caja(l, 1, 6, 15, 12, 0x8D99AA, 0x26313F);
		caja(l, 4, 10, 12, 15, 0xFFFFFF, 0x26313F);
		raya(l, 5, 12, 11, 12, 1, 0x26313F);
		lz_circulo(l, PX(12), PY(8), T, tinta(0x3BB273));
		break;
	case IC_VISTA_PREVIA:
		hoja(l, 2, 1, 11, 14);
		lupa(l, 10, 9, 4);
		break;
	case IC_ORTOGRAFIA:
		letra(l, "abc", 1, 0, 0, 0x26313F, 62);
		{
			/* (la letra, arriba; la marca, abajo) */
			raya(l, 4, 12, 7, 15, 2, 0x1E8E3E);
			raya(l, 7, 15, 13, 9, 2, 0x1E8E3E);
		}
		break;
	case IC_CORTAR:
		raya(l, 4, 1, 10, 11, 2, 0x26313F);
		raya(l, 12, 1, 6, 11, 2, 0x26313F);
		lz_circulo(l, PX(5), PY(12), 3 * T, tinta(0x26313F));
		lz_circulo(l, PX(5), PY(12), 3 * T - 2 * 16, tinta(0xFFFFFF));
		lz_circulo(l, PX(11), PY(12), 3 * T, tinta(0x26313F));
		lz_circulo(l, PX(11), PY(12), 3 * T - 2 * 16, tinta(0xFFFFFF));
		break;
	case IC_COPIAR:
		hoja(l, 1, 1, 10, 11);
		hoja(l, 6, 5, 15, 15);
		break;
	case IC_PEGAR:
		caja(l, 2, 2, 13, 15, 0xA36B2C, 0x4A2E0F);
		caja(l, 5, 1, 10, 4, 0xC9CED6, 0x26313F);
		hoja(l, 7, 6, 15, 15);
		break;
	case IC_BROCHA: {
		int a[8] = { 9, 1, 12, 1, 12, 6, 9, 6 };
		int b[8] = { 7, 6, 14, 6, 15, 10, 6, 10 };
		int c2[10] = { 6, 10, 15, 10, 14, 15, 10, 13, 6, 15 };

		pol(l, a, 4, 0x6B4E12);
		pol(l, b, 4, 0x8D99AA);
		pol(l, c2, 5, 0xF2C14E);
		raya(l, 6, 10, 15, 10, 1, 0x26313F);
		break;
	}
	case IC_DESHACER:
		flecha_curva(l, 0);
		break;
	case IC_REHACER:
		flecha_curva(l, 1);
		break;
	case IC_TABLA: {
		int k;

		caja(l, 1, 2, 15, 14, 0xFFFFFF, 0x26313F);
		caja(l, 1, 2, 15, 6, 0xC4D5EF, 0x26313F);
		for (k = 1; k < 3; k++) {
			raya(l, 1 + k * 14 / 3, 2, 1 + k * 14 / 3, 14, 1, 0x26313F);
			raya(l, 1, 2 + k * 4, 15, 2 + k * 4, 1, 0x26313F);
		}
		break;
	}
	case IC_IMAGEN: {
		int m[8] = { 2, 13, 7, 6, 10, 10, 14, 13 };

		caja(l, 1, 2, 15, 14, 0xDDEBFA, 0x26313F);
		pol(l, m, 4, 0x2F8A3E);
		lz_circulo(l, PX(11), PY(6), 2 * T, tinta(0xE7A10E));
		break;
	}
	case IC_NEGRITA:
		letra(l, "N", 1, 0, 1, 0x101820, 100);
		break;
	case IC_CURSIVA:
		letra(l, "K", 0, 1, 1, 0x101820, 100);
		break;
	case IC_SUBRAYADO:
		letra(l, "S", 0, 0, 1, 0x101820, 92);
		raya(l, 3, 15, 13, 15, 2, 0x101820);
		break;
	case IC_TACHADO:
		letra(l, "ab", 0, 0, 1, 0x101820, 80);
		raya(l, 1, 9, 15, 9, 2, 0xB3261E);
		break;
	case IC_SUPERINDICE:
		letra(l, "x", 0, 0, 1, 0x101820, 80);
		raya(l, 12, 3, 14, 1, 1, 0x2563C9);
		raya(l, 14, 1, 14, 6, 1, 0x2563C9);
		break;
	case IC_SUBINDICE:
		letra(l, "x", 0, 0, 1, 0x101820, 80);
		raya(l, 12, 12, 14, 10, 1, 0x2563C9);
		raya(l, 14, 10, 14, 15, 1, 0x2563C9);
		break;
	case IC_IZQUIERDA:
		lineas(l, 0, 3);
		break;
	case IC_CENTRO:
		lineas(l, 1, 3);
		break;
	case IC_DERECHA:
		lineas(l, 2, 3);
		break;
	case IC_JUSTIFICADO:
		lineas(l, 3, 3);
		break;
	case IC_VINETAS: {
		int k;

		for (k = 0; k < 3; k++) {
			lz_circulo(l, PX(3), PY(3 + k * 5), (3 * T) / 2, tinta(0x26313F));
			raya(l, 6, 3 + k * 5, 15, 3 + k * 5, 2, 0x26313F);
		}
		break;
	}
	case IC_NUMEROS: {
		int k, cara = letra_cara(FAM_SANS, 1, 0, NULL);

		for (k = 0; k < 3; k++) {
			char t[2] = { (char)('1' + k), 0 };

			lz_texto(l, cara, T * 4 * 38 / 100, 0, X0 + T / 16, Y0 + (5 + k * 5) * T / 16, t, tinta(0x2563C9));
			raya(l, 6, 3 + k * 5, 15, 3 + k * 5, 2, 0x26313F);
		}
		break;
	}
	case IC_SANGRIA_MENOS:
	case IC_SANGRIA_MAS: {
		int k, p[6];

		for (k = 0; k < 4; k++)
			raya(l, k == 0 || k == 3 ? 1 : 8, 2 + k * 4, 15, 2 + k * 4, 2, 0x26313F);
		if (ic == IC_SANGRIA_MAS) {
			p[0] = 1; p[1] = 5; p[2] = 1; p[3] = 11; p[4] = 6; p[5] = 8;
		} else {
			p[0] = 6; p[1] = 5; p[2] = 6; p[3] = 11; p[4] = 1; p[5] = 8;
		}
		pol(l, p, 3, 0x2563C9);
		break;
	}
	case IC_COLOR_LETRA:
		letra(l, "A", 1, 0, 0, 0x101820, 95);
		break;
	case IC_RESALTE: {
		int a[8] = { 9, 1, 14, 6, 8, 12, 3, 7 };
		int b[8] = { 3, 7, 8, 12, 5, 14, 1, 10 };

		pol(l, a, 4, 0x26313F);
		{
			int c2[8] = { 9, 2, 13, 6, 8, 11, 4, 7 };

			pol(l, c2, 4, 0xF4E04D);
		}
		pol(l, b, 4, 0x26313F);
		break;
	}
	case IC_BUSCAR:
		lupa(l, 7, 7, 5);
		break;
	case IC_ZOOM:
		lupa(l, 7, 7, 5);
		raya(l, 5, 7, 9, 7, 2, 0x26313F);
		raya(l, 7, 5, 7, 9, 2, 0x26313F);
		break;
	case IC_PDF:
		hoja(l, 3, 1, 13, 15);
		caja(l, 1, 8, 15, 14, 0xC62828, 0x7A1414);
		{
			int cara = letra_cara(FAM_SANS, 1, 0, NULL), w = lz_ancho(cara, T * 4 * 34 / 100, 0, "PDF");

			lz_texto(l, cara, T * 4 * 34 / 100, 0, X0 + (T - w) / 2, Y0 + T * 13 / 16, "PDF", tinta(0xFFFFFF));
		}
		break;
	case IC_MARCAS:
		letra(l, "\xc2\xb6", 1, 0, 0, 0x26313F, 100);
		break;
	case IC_SALTO:
		hoja(l, 3, 0, 13, 6);
		hoja(l, 3, 10, 13, 16);
		{
			int k;

			for (k = 0; k < 8; k++)
				raya(l, k * 2, 8, k * 2 + 1, 8, 1, 0x2563C9);
		}
		break;
	case IC_BORDES:
		caja(l, 1, 1, 15, 15, 0xFFFFFF, 0x26313F);
		raya(l, 8, 1, 8, 15, 1, 0x8D99AA);
		raya(l, 1, 8, 15, 8, 1, 0x8D99AA);
		break;
	/* (CUENTAS32) los de la hoja de cálculo */
	case IC_AUTOSUMA: {
		/* la sigma, con trazos gruesos */
		raya(l, 3, 2, 13, 2, 2, 0x101820);
		raya(l, 3, 2, 8, 8, 2, 0x101820);
		raya(l, 8, 8, 3, 14, 2, 0x101820);
		raya(l, 3, 14, 13, 14, 2, 0x101820);
		raya(l, 13, 2, 13, 4, 2, 0x101820);
		raya(l, 13, 14, 13, 12, 2, 0x101820);
		break;
	}
	case IC_FX:
		letra(l, "fx", 0, 1, 1, 0x101820, 78);
		break;
	case IC_ORDEN_ASC:
	case IC_ORDEN_DESC: {
		/* «A» arriba y «Z» abajo (o al revés) y la flecha hacia abajo */
		int cara = letra_cara(FAM_SANS, 1, 0, NULL), tam = T * 4 * 46 / 100;
		int fl[6] = { 10, 11, 16, 11, 13, 15 };

		lz_texto(l, cara, tam, 0, X0 + T * 1 / 16, Y0 + T * 7 / 16, ic == IC_ORDEN_ASC ? "A" : "Z", tinta(0x101820));
		lz_texto(l, cara, tam, 0, X0 + T * 1 / 16, Y0 + T * 15 / 16, ic == IC_ORDEN_ASC ? "Z" : "A", tinta(0x101820));
		raya(l, 13, 2, 13, 11, 2, 0x2563C9);
		pol(l, fl, 3, 0x2563C9);
		break;
	}
	case IC_GRAFICO: {
		/* tres columnas de alturas distintas, cada una con su relleno, sobre un eje */
		caja(l, 2, 9, 5, 14, 0x2563C9, 0x101820);
		caja(l, 6, 4, 9, 14, 0xF2C14E, 0x101820);
		caja(l, 10, 7, 13, 14, 0x3BB273, 0x101820);
		raya(l, 1, 14, 15, 14, 1, 0x101820);
		raya(l, 1, 1, 1, 14, 1, 0x101820);
		/* (la del medio, con rayas: no solo el color) */
		raya(l, 6, 7, 9, 5, 1, 0x101820);
		raya(l, 6, 10, 9, 8, 1, 0x101820);
		break;
	}
	case IC_MONEDA:
		letra(l, "\xe2\x82\xac", 1, 0, 0, 0x1F6B3A, 96);
		break;
	case IC_PORCENTAJE:
		letra(l, "%", 1, 0, 0, 0x101820, 92);
		break;
	case IC_MILES:
		letra(l, "000", 1, 0, 0, 0x101820, 52);
		raya(l, 2, 14, 14, 14, 1, 0x101820);
		break;
	case IC_DEC_MAS:
	case IC_DEC_MENOS: {
		/* «,0» con una flecha hacia «,00» (o al revés) */
		int cara = letra_cara(FAM_SANS, 1, 0, NULL), tam = T * 4 * 42 / 100;

		lz_texto(l, cara, tam, 0, X0, Y0 + T * 7 / 16, ic == IC_DEC_MAS ? ",0" : ",00", tinta(0x101820));
		lz_texto(l, cara, tam, 0, X0 + T * 5 / 16, Y0 + T * 15 / 16, ic == IC_DEC_MAS ? ",00" : ",0", tinta(0x101820));
		{
			int fl[6] = { 1, 10, 5, 10, 3, 13 };

			raya(l, 3, 7, 3, 10, 2, 0x2563C9);
			pol(l, fl, 3, 0x2563C9);
		}
		break;
	}
	case IC_COMBINAR: {
		/* dos celdas que se juntan: las flechas hacia el medio */
		int a[6] = { 2, 8, 6, 5, 6, 11 }, b[6] = { 14, 8, 10, 5, 10, 11 };

		caja(l, 0, 2, 16, 14, 0xFFFFFF, 0x26313F);
		raya(l, 8, 2, 8, 4, 1, 0x8D99AA);
		raya(l, 8, 12, 8, 14, 1, 0x8D99AA);
		pol(l, a, 3, 0x2563C9);
		pol(l, b, 3, 0x2563C9);
		break;
	}
	case IC_RELLENO: {
		/* el bote de pintura inclinado y la gota */
		int bote[8] = { 3, 7, 9, 2, 14, 7, 8, 13 };
		int dentro[8] = { 5, 7, 9, 4, 12, 7, 8, 11 };

		pol(l, bote, 4, 0x26313F);
		pol(l, dentro, 4, 0xFFFFFF);
		lz_circulo(l, PX(14), PY(12), 2 * T, tinta(0x2563C9));
		raya(l, 9, 2, 6, 0, 1, 0x26313F);
		break;
	}
	case IC_FILTRO: {
		int e[14] = { 1, 2, 15, 2, 10, 8, 10, 14, 6, 15, 6, 8, 1, 2 };

		pol(l, e, 6, 0x26313F);
		{
			int d[12] = { 3, 3, 13, 3, 9, 7, 9, 12, 7, 13, 7, 7 };

			pol(l, d, 6, 0xDDEBFA);
		}
		break;
	}
	case IC_COMENTARIO: {
		int b[10] = { 1, 2, 15, 2, 15, 11, 6, 11, 3, 15 };

		pol(l, b, 5, 0x26313F);
		{
			int c2[10] = { 2, 3, 14, 3, 14, 10, 5, 10, 4, 12 };

			pol(l, c2, 5, 0xFFF4C2);
		}
		raya(l, 4, 5, 12, 5, 1, 0x26313F);
		raya(l, 4, 8, 10, 8, 1, 0x26313F);
		break;
	}
	case IC_INMOVILIZAR:
		caja(l, 1, 1, 15, 15, 0xFFFFFF, 0x26313F);
		caja(l, 1, 1, 15, 5, 0xC4D5EF, 0x26313F);
		caja(l, 1, 1, 5, 15, 0xC4D5EF, 0x26313F);
		raya(l, 1, 5, 15, 5, 2, 0x101820);
		raya(l, 5, 1, 5, 15, 2, 0x101820);
		break;
	case IC_AYUDA:
		lz_circulo(l, PX(8), PY(8), 7 * T, tinta(0x2563C9));
		{
			int cara = letra_cara(FAM_SANS, 1, 0, NULL), w = lz_ancho(cara, T * 4 * 70 / 100, 0, "?");

			lz_texto(l, cara, T * 4 * 70 / 100, 0, X0 + (T - w) / 2, Y0 + T * 13 / 16, "?", tinta(0xFFFFFF));
		}
		break;
	}
}
