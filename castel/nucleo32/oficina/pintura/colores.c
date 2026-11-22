/* colores.c - Pintura 2: los colores con su nombre (colores.h).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "colores.h"
#include "texto.h"
#include "lamina.h"

typedef struct {
	u32 c;
	const char *nombre;
} con_nombre;

/* la paleta de fábrica: arriba los oscuros y fuertes, abajo los claros */
static const con_nombre fabrica[PALETA_N] = {
	{ 0x000000, "Negro" }, { 0x404040, "Gris oscuro" }, { 0x808080, "Gris" }, { 0x800000, "Granate" },
	{ 0xE02020, "Rojo" }, { 0xF07800, "Naranja" }, { 0xFFD800, "Amarillo" }, { 0x808000, "Verde oliva" },
	{ 0x20A020, "Verde" }, { 0x008080, "Verde azulado" }, { 0x2050E0, "Azul" }, { 0x000080, "Azul marino" },
	{ 0x8020A0, "Morado" }, { 0x8B4513, "Marrón" },
	{ 0xFFFFFF, "Blanco" }, { 0xC0C0C0, "Gris claro" }, { 0xE6E6E6, "Gris perla" }, { 0xFF9EC4, "Rosa" },
	{ 0xFA8072, "Salmón" }, { 0xFFC890, "Melocotón" }, { 0xFFF59D, "Amarillo claro" }, { 0xA0E040, "Lima" },
	{ 0x90EE90, "Verde claro" }, { 0x40E0D0, "Turquesa" }, { 0x87CEEB, "Celeste" }, { 0x80A0FF, "Azul claro" },
	{ 0xC8A2E8, "Lavanda" }, { 0xF5DEB3, "Beis" },
};

/* más nombres, para lo que no es de la paleta */
static const con_nombre otros[] = {
	{ 0x202020, "Casi negro" }, { 0x36454F, "Gris carbón" }, { 0x708090, "Gris pizarra" }, { 0xA9A9A9, "Gris medio" },
	{ 0xF5F5F5, "Blanco humo" }, { 0xFFFFF0, "Marfil" }, { 0xFFFDD0, "Crema" }, { 0xF9F6EE, "Hueso" },
	{ 0xDC143C, "Carmesí" }, { 0xFF0000, "Rojo puro" }, { 0xB22222, "Rojo ladrillo" }, { 0x8B0000, "Rojo oscuro" },
	{ 0x800020, "Burdeos" }, { 0xFF6347, "Tomate" }, { 0xFF7F50, "Coral" }, { 0xFF4500, "Rojo anaranjado" },
	{ 0xFFA500, "Naranja claro" }, { 0xCC5500, "Naranja quemado" }, { 0xFFD700, "Oro" }, { 0xDAA520, "Dorado oscuro" },
	{ 0xFFFF00, "Amarillo puro" }, { 0xFFF700, "Limón" }, { 0xF0E68C, "Caqui" }, { 0xBDB76B, "Caqui oscuro" },
	{ 0xCC7722, "Ocre" }, { 0xD2691E, "Chocolate" }, { 0xA0522D, "Siena" }, { 0x5C3317, "Marrón oscuro" },
	{ 0xC19A6B, "Camel" }, { 0xD2B48C, "Tostado" }, { 0xC2B280, "Arena" }, { 0xE97451, "Terracota" },
	{ 0x00FF00, "Verde puro" }, { 0x7FFF00, "Verde chillón" }, { 0x228B22, "Verde bosque" }, { 0x006400, "Verde oscuro" },
	{ 0x2E8B57, "Verde mar" }, { 0x50C878, "Esmeralda" }, { 0x98FF98, "Menta" }, { 0x6B8E23, "Verde musgo" },
	{ 0x556B2F, "Verde oliva oscuro" }, { 0x9ACD32, "Verde amarillento" }, { 0x00FFFF, "Cian" }, { 0x7FFFD4, "Aguamarina" },
	{ 0x20B2AA, "Verde mar claro" }, { 0x008B8B, "Cian oscuro" }, { 0x4682B4, "Azul acero" }, { 0x6495ED, "Azul aciano" },
	{ 0x0000FF, "Azul puro" }, { 0x4169E1, "Azul real" }, { 0x1E90FF, "Azul brillante" }, { 0x00BFFF, "Azul cielo intenso" },
	{ 0xADD8E6, "Azul pálido" }, { 0x191970, "Azul medianoche" }, { 0x003153, "Azul de Prusia" }, { 0x4B0082, "Añil" },
	{ 0xEE82EE, "Violeta" }, { 0x9400D3, "Violeta oscuro" }, { 0xFF00FF, "Magenta" }, { 0xFF0090, "Fucsia" },
	{ 0xDA70D6, "Orquídea" }, { 0xDDA0DD, "Ciruela" }, { 0x800080, "Púrpura" }, { 0x660033, "Vino" },
	{ 0xFFC0CB, "Rosa claro" }, { 0xFF69B4, "Rosa intenso" }, { 0xC71585, "Rosa oscuro" }, { 0xE6E6FA, "Lila pálido" },
	{ 0xB57EDC, "Lila" }, { 0xFFDAB9, "Durazno" }, { 0xFFE4C4, "Galleta" }, { 0xFAEBD7, "Blanco antiguo" },
	{ 0xBC8F8F, "Rosa viejo" }, { 0xCD853F, "Madera" }, { 0x8B8000, "Mostaza oscuro" }, { 0xE1AD01, "Mostaza" },
	{ 0x3B74D6, "Azul medio" }, { 0x5F9EA0, "Azul cadete" }, { 0xB0C4DE, "Azul acero claro" }, { 0x2F4F4F, "Gris pizarra oscuro" },
};
#define N_OTROS ((int)(sizeof otros / sizeof otros[0]))

u32 paleta[PALETA_N];

void paleta_de_fabrica(void)
{
	int i;

	for (i = 0; i < PALETA_N; i++)
		paleta[i] = OPACO(fabrica[i].c);
}

/* lo lejos que están dos colores (la «media roja»: pesa cada canal según lo
 * rojo que sean; sin raíz, es para comparar) */
static u32 distancia(u32 a, u32 b)
{
	int r1 = (a >> 16) & 255, g1 = (a >> 8) & 255, b1 = a & 255;
	int r2 = (b >> 16) & 255, g2 = (b >> 8) & 255, b2 = b & 255;
	int rm = (r1 + r2) / 2, dr = r1 - r2, dg = g1 - g2, db = b1 - b2;

	return (u32)(((512 + rm) * dr * dr >> 8) + 4 * dg * dg + ((767 - rm) * db * db >> 8));
}

const char *color_nombre(u32 c, int *exacto)
{
	u32 mejor = 0xFFFFFFFFu, d;
	const char *n = "Negro";
	int i;

	if (exacto)
		*exacto = 0;
	if (ALFA(c) == 0) {
		if (exacto)
			*exacto = 1;
		return "Transparente";
	}
	c &= 0xFFFFFF;
	for (i = 0; i < PALETA_N; i++) {
		d = distancia(c, fabrica[i].c);
		if (d < mejor) {
			mejor = d;
			n = fabrica[i].nombre;
		}
	}
	for (i = 0; i < N_OTROS; i++) {
		d = distancia(c, otros[i].c);
		if (d < mejor) {
			mejor = d;
			n = otros[i].nombre;
		}
	}
	if (exacto)
		*exacto = mejor == 0;
	return n;
}

const char *paleta_nombre(int i)
{
	if (i < 0 || i >= PALETA_N)
		return "";
	if ((paleta[i] & 0xFFFFFF) == fabrica[i].c && ALFA(paleta[i]) == 255)
		return fabrica[i].nombre;
	return color_nombre(paleta[i], NULL);
}

void color_codigo(u32 c, char *d, u32 cabe)
{
	if (ALFA(c) == 255)
		fmt(d, cabe, "#%06X", c & 0xFFFFFF);
	else
		fmt(d, cabe, "#%06X%02X", c & 0xFFFFFF, ALFA(c));
}

void color_describe(u32 c, char *d, u32 cabe)
{
	char cod[16];
	int ex;
	const char *n = color_nombre(c, &ex);

	color_codigo(c, cod, sizeof cod);
	if (ALFA(c) == 0)
		fmt(d, cabe, "Transparente");
	else if (ALFA(c) < 255)
		fmt(d, cabe, "%s%s translúcido al %u %% (%s)", n, ex ? "" : ", casi", ALFA(c) * 100 / 255, cod);
	else
		fmt(d, cabe, "%s%s (%s)", n, ex ? "" : ", casi", cod);
}

static int hex(int c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

int color_de_codigo(const char *s, u32 *c)
{
	u32 v = 0, n = 0;
	int k;

	while (*s == ' ')
		s++;
	if (*s == '#')
		s++;
	while (s[n] && s[n] != ' ')
		n++;
	if (n != 3 && n != 6 && n != 8)
		return 0;
	for (k = 0; k < (int)n; k++) {
		int h = hex((u8)s[k]);

		if (h < 0)
			return 0;
		v = v << 4 | (u32)h;
		if (n == 3)
			v = v << 4 | (u32)h;
	}
	if (n == 8)
		*c = (v & 0xFF) << 24 | v >> 8;
	else
		*c = 0xFF000000u | (v & 0xFFFFFF);
	return 1;
}

void color_a_hsv(u32 c, int *h, int *s, int *v)
{
	int r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
	int mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
	int mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
	int d = mx - mn, hh;

	*v = (mx * 100 + 127) / 255;
	*s = mx ? (d * 100 + mx / 2) / mx : 0;
	if (!d) {
		*h = 0;
		return;
	}
	if (mx == r)
		hh = 60 * (g - b) / d;
	else if (mx == g)
		hh = 120 + 60 * (b - r) / d;
	else
		hh = 240 + 60 * (r - g) / d;
	if (hh < 0)
		hh += 360;
	*h = hh % 360;
}

u32 color_de_hsv(int h, int s, int v)
{
	int i, f, p, q, t, V, r, g, b;

	h = ((h % 360) + 360) % 360;
	s = s < 0 ? 0 : s > 100 ? 100 : s;
	v = v < 0 ? 0 : v > 100 ? 100 : v;
	V = (v * 255 + 50) / 100;
	if (!s)
		return 0xFF000000u | (u32)V << 16 | (u32)V << 8 | (u32)V;
	i = h / 60;
	f = (h % 60) * 255 / 60;		/* (0-255) */
	p = V * (100 - s) / 100;
	q = V * (100 * 255 - s * f) / (100 * 255);
	t = V * (100 * 255 - s * (255 - f)) / (100 * 255);
	switch (i) {
	case 0: r = V; g = t; b = p; break;
	case 1: r = q; g = V; b = p; break;
	case 2: r = p; g = V; b = t; break;
	case 3: r = p; g = q; b = V; break;
	case 4: r = t; g = p; b = V; break;
	default: r = V; g = p; b = q; break;
	}
	return 0xFF000000u | (u32)r << 16 | (u32)g << 8 | (u32)b;
}
