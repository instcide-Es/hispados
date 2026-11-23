/* iconos_p.c - Pintura 2: los iconos de la caja de herramientas, dibujados
 * con rayas y polígonos suavizados (oficina/comun/lienzo.h) a cualquier
 * tamaño: se reconocen por la forma, no por el color.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <math.h>
#include "pintura.h"

/* (las medidas, en un icono de 24: se escalan a «t») */
static int X0, Y0, T;
#define C(v) ((int)((v) * T * 16 / 24))
#define PX(v) (X0 * 16 + C(v))
#define PY(v) (Y0 * 16 + C(v))

static void raya(lienzo *l, float x0, float y0, float x1, float y1, float g, u32 c)
{
	lz_linea(l, PX(x0), PY(y0), PX(x1), PY(y1), (int)(g * 16 * T / 24), c);
}

static void poli(lienzo *l, const float *p, int n, u32 c)
{
	int xy[32], i;

	for (i = 0; i < n && i < 16; i++) {
		xy[2 * i] = PX(p[2 * i]);
		xy[2 * i + 1] = PY(p[2 * i + 1]);
	}
	lz_poligono(l, xy, n, c);
}

static void contorno(lienzo *l, const float *p, int n, float g, u32 c, int cerrado)
{
	int i;

	for (i = 0; i + 1 < n + (cerrado ? 1 : 0); i++)
		raya(l, p[2 * i], p[2 * i + 1], p[2 * ((i + 1) % n)], p[2 * ((i + 1) % n) + 1], g, c);
}

static void punteado(lienzo *l, float x0, float y0, float x1, float y1, u32 c)
{
	int i, n = 6;

	for (i = 0; i < n; i += 2)
		raya(l, x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * i / n, x0 + (x1 - x0) * (i + 1) / n, y0 + (y1 - y0) * (i + 1) / n, 1.6f, c);
}

static void elipse(lienzo *l, float cx, float cy, float a, float b, float g, u32 c, int puntos)
{
	int i, n = 28;

	for (i = 0; i < n; i++) {
		double t0 = 6.2831853 * i / n, t1 = 6.2831853 * (i + 1) / n;

		if (puntos && i % 2)
			continue;
		raya(l, cx + a * (float)cos(t0), cy + b * (float)cos(t0 - 1.5707963), cx + a * (float)cos(t1), cy + b * (float)cos(t1 - 1.5707963), g, c);
	}
}

void pin_icono(lienzo *l, int h, int x, int y, int t, int apagado)
{
	u32 tinta = apagado ? UI_TINTA_GRIS : UI_TINTA, claro = apagado ? 0xD0D4DA : 0xFFFFFF;
	u32 acento = apagado ? UI_TINTA_GRIS : 0x2563C9, calido = apagado ? UI_TINTA_GRIS : 0xE07B30;

	X0 = x;
	Y0 = y;
	T = t;
	switch (h) {
	case H_SELECCION:
		punteado(l, 3, 4, 21, 4, tinta);
		punteado(l, 21, 4, 21, 20, tinta);
		punteado(l, 21, 20, 3, 20, tinta);
		punteado(l, 3, 20, 3, 4, tinta);
		break;
	case H_LIBRE:
		elipse(l, 12, 11, 9, 7, 1.6f, tinta, 1);
		raya(l, 8, 17, 6, 22, 1.6f, tinta);
		break;
	case H_LAPIZ: {
		static const float cuerpo[] = { 6, 15, 16, 5, 19, 8, 9, 18 };
		static const float punta[] = { 6, 15, 9, 18, 4, 20 };

		poli(l, cuerpo, 4, calido);
		contorno(l, cuerpo, 4, 1.4f, tinta, 1);
		poli(l, punta, 3, claro);
		contorno(l, punta, 3, 1.4f, tinta, 1);
		raya(l, 4, 20, 5, 19, 2, tinta);
		break;
	}
	case H_PINCEL: {
		static const float mango[] = { 11, 11, 19, 3, 21, 5, 13, 13 };
		static const float pelo[] = { 11, 11, 13, 13, 9, 19, 3, 21, 5, 15 };

		poli(l, mango, 4, calido);
		contorno(l, mango, 4, 1.3f, tinta, 1);
		poli(l, pelo, 5, tinta);
		break;
	}
	case H_AEROGRAFO: {
		static const float bote[] = { 10, 9, 17, 9, 17, 21, 10, 21 };
		int i;
		static const float gotas[] = { 3, 4, 5, 7, 2, 9, 6, 3, 4, 11, 7, 6 };

		poli(l, bote, 4, claro);
		contorno(l, bote, 4, 1.5f, tinta, 1);
		raya(l, 12, 9, 12, 6, 2.2f, tinta);
		raya(l, 12, 6, 15, 6, 2.2f, tinta);
		raya(l, 10, 13, 17, 13, 1.2f, tinta);
		for (i = 0; i < 6; i++)
			raya(l, gotas[2 * i], gotas[2 * i + 1], gotas[2 * i] + 0.3f, gotas[2 * i + 1] + 0.3f, 1.6f, tinta);
		break;
	}
	case H_BORRADOR: {
		static const float goma[] = { 3, 15, 12, 6, 21, 15, 14, 21, 9, 21 };
		static const float punta[] = { 3, 15, 7, 11, 14, 18, 11, 21, 9, 21 };

		poli(l, goma, 5, claro);
		poli(l, punta, 5, acento);
		contorno(l, goma, 5, 1.4f, tinta, 1);
		raya(l, 9, 21, 21, 21, 1.2f, tinta);
		break;
	}
	case H_RELLENO: {
		static const float cubo[] = { 4, 11, 11, 4, 18, 11, 11, 18 };

		poli(l, cubo, 4, claro);
		contorno(l, cubo, 4, 1.5f, tinta, 1);
		raya(l, 7, 8, 3, 4, 1.4f, tinta);
		/* la gota que cae */
		lz_circulo(l, PX(19.5f), PY(17), C(2.3f), acento);
		raya(l, 18, 11, 19.5f, 15, 1.6f, acento);
		break;
	}
	case H_CUENTAGOTAS: {
		static const float tubo[] = { 4, 17, 13, 8, 16, 11, 7, 20 };

		poli(l, tubo, 4, claro);
		contorno(l, tubo, 4, 1.4f, tinta, 1);
		raya(l, 4, 17, 2.5f, 21.5f, 1.6f, tinta);
		raya(l, 2.5f, 21.5f, 7, 20, 1.6f, tinta);
		lz_circulo(l, PX(17.5f), PY(6.5f), C(3.6f), tinta);
		raya(l, 12, 7, 17, 12, 2.2f, tinta);
		break;
	}
	case H_TEXTO: {
		static const float a1[] = { 4, 20, 12, 3 }, a2[] = { 12, 3, 20, 20 };

		raya(l, a1[0], a1[1], a1[2], a1[3], 2.6f, tinta);
		raya(l, a2[0], a2[1], a2[2], a2[3], 2.6f, tinta);
		raya(l, 7.5f, 14, 16.5f, 14, 2.2f, tinta);
		break;
	}
	case H_LUPA:
		elipse(l, 10, 10, 6.5f, 6.5f, 2, tinta, 0);
		raya(l, 14.8f, 14.8f, 21, 21, 3, tinta);
		raya(l, 7, 10, 13, 10, 1.4f, tinta);
		raya(l, 10, 7, 10, 13, 1.4f, tinta);
		break;
	case H_RAYA:
		raya(l, 3, 20, 21, 4, 2, tinta);
		break;
	case H_CURVA: {
		int i;
		float px = 3, py = 19;

		for (i = 1; i <= 16; i++) {
			float s = (float)i / 16, u = 1 - s;
			float qx = u * u * u * 3 + 3 * u * u * s * 6 + 3 * u * s * s * 18 + s * s * s * 21;
			float qy = u * u * u * 19 + 3 * u * u * s * -2 + 3 * u * s * s * 26 + s * s * s * 5;

			raya(l, px, py, qx, qy, 2, tinta);
			px = qx;
			py = qy;
		}
		break;
	}
	case H_RECTANGULO: {
		static const float r[] = { 3, 5, 21, 5, 21, 19, 3, 19 };

		contorno(l, r, 4, 2, tinta, 1);
		break;
	}
	case H_REDONDEADO:
		lz_marco_redondeado(l, X0 + C(3) / 16, Y0 + C(5) / 16, C(18) / 16 + 1, C(14) / 16 + 1, C(5) / 16, tinta);
		lz_marco_redondeado(l, X0 + C(3) / 16 + 1, Y0 + C(5) / 16 + 1, C(18) / 16 - 1, C(14) / 16 - 1, C(4) / 16, tinta);
		break;
	case H_ELIPSE:
		elipse(l, 12, 12, 9, 6.5f, 2, tinta, 0);
		break;
	case H_POLIGONO: {
		static const float p[] = { 4, 8, 13, 3, 21, 9, 17, 20, 7, 18 };

		contorno(l, p, 5, 2, tinta, 1);
		break;
	}
	}
}
