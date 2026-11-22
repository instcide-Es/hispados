/* solitar.c - (JUEGOS) Solitario, el de Hispa-386 (juegos/solitario.asm) en
 * Hispa-Pentium: el solitario de columnas, con la baraja francesa (52
 * cartas, siete columnas) o la española (40, seis columnas), a elegir en
 * Juego. Las cartas son las de Hispa-386, dibujadas allí (las pintas, los
 * índices y las figuras: las francesas de dos cabezas, las españolas de
 * cuerpo entero con su palo; los saca datos386.py del fuente), ahora a la
 * escala que pida la ventana. El mazo se pasa de una en una y se le da la
 * vuelta las veces que se quiera. En las columnas se baja de una en una,
 * del rey al as, alternando los colores (rojo y negro; en la española,
 * claros —oros y copas— y oscuros —espadas y bastos—); a un hueco solo va
 * un rey. Arriba, cuatro montones de salida, uno por palo, del as al rey.
 * Ratón: arrastrar una carta o una escalera (si no vale, vuelve), clic en
 * el mazo, dos clics o el botón derecho suben una carta. Teclado: F2 nueva,
 * Retroceso o Ctrl+Z deshacer (512 pasos), Espacio sacar carta, S subir las
 * que se puedan. Movimientos, tiempo (desde el primer movimiento) y cartas
 * del mazo, a la vista. Lo de cada baraja (jugadas, ganadas, el mejor
 * tiempo y los menos movimientos) y la baraja elegida van a la carpeta del
 * usuario. Para las pruebas: /S=n, /CASI (todo subido menos los reyes, en
 * las columnas) y F12 (el estado).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "juego.h"
#include "datos386.h"

#define MAX_CARTAS 52
#define MAX_COLS 7
/* los montones: 0 el mazo, 1 el descarte, 2-5 las salidas, 6-12 las columnas */
#define P_MAZO 0
#define P_DESCARTE 1
#define P_SALIDA 2
#define P_COL 6
#define N_PILAS 13
#define MAX_DESHACER 512
enum { E_JUEGA, E_GANADA };

struct pila { u8 n; u8 c[MAX_CARTAS]; u8 arriba[MAX_CARTAS]; };	/* las cartas (palo * 13 + valor - 1) y si están destapadas */
struct jugada { struct pila p[N_PILAS]; int movimientos; };
static struct pila P[N_PILAS];
static struct jugada *historia;
static int n_hist, espanola, ncols = 7, por_palo = 13, estado, movimientos, empezado, segundos, ganadas_esta;
static u32 t0;
static int casi;

/* arrastrar */
static int arrastra = -1, arr_pila, arr_desde, arr_dx, arr_dy, arr_x, arr_y, arr_ox, arr_oy, movido;
static lienzo capa;
static int ant_x = -1, ant_y, ant_an, ant_al;

/* la pantalla */
static int CA = 72, CL = 100, sep, ox, oy, fila2, arriba, pie_al = 22, paso_tapada, paso_vista;
static lienzo cara[MAX_CARTAS], dorso;
static int hechas;		/* el tamaño de las cartas pintadas */
static int esquina[16], esquina_r;	/* lo que se come cada fila de las esquinas redondas */

static const struct efecto s_carta = { ON_RUIDO, 2500, 1200, 30, 16, 1, 25 };
static const struct efecto s_sube = { ON_CAMPANA, 1320, 1320, 160, 20, 1, 150 };

static void pinta_todo(void);

/* ---------------------------------------------------------------- las cartas */
static int palo_de(int c) { return c / 13; }
static int valor_de(int c) { return c % 13 + 1; }	/* 1-13 (en la española: 1-7, 10, 11, 12) */
static int rojo(int c) { return palo_de(c) < 2; }	/* francesa: corazones y diamantes; española: oros y copas (claros) */
static int ultimo_valor(void) { return espanola ? 12 : 13; }

/* el siguiente valor hacia arriba (la española salta del 7 al 10) */
static int sube(int v) { return espanola && v == 7 ? 10 : v + 1; }

static int puede_salida(int c, int s)
{
	struct pila *p = &P[P_SALIDA + s];

	if (!p->n)
		return valor_de(c) == 1;
	return palo_de(p->c[p->n - 1]) == palo_de(c) && sube(valor_de(p->c[p->n - 1])) == valor_de(c);
}

static int puede_columna(int c, int k)
{
	struct pila *p = &P[P_COL + k];
	int t;

	if (!p->n)
		return valor_de(c) == ultimo_valor();
	t = p->c[p->n - 1];
	return p->arriba[p->n - 1] && rojo(t) != rojo(c) && sube(valor_de(c)) == valor_de(t);
}

static void baraja_nueva(void)
{
	int mazo[MAX_CARTAS], n = 0, i, k, v;

	espanola = espanola ? 1 : 0;
	ncols = espanola ? 6 : 7;
	memset(P, 0, sizeof P);
	for (i = 0; i < 4; i++)
		for (v = 1; v <= 13; v++)
			if (!espanola || v <= 7 || (v >= 10 && v <= 12))
				mazo[n++] = i * 13 + v - 1;
	por_palo = n / 4;
	for (i = n - 1; i > 0; i--) {
		int j = azar_n(i + 1), t = mazo[i];

		mazo[i] = mazo[j];
		mazo[j] = t;
	}
	if (casi) {
		/* (las pruebas) todo subido menos los reyes, cada uno en una columna */
		for (i = 0; i < 4; i++)
			for (v = 1; v < ultimo_valor(); v++)
				if (!espanola || v <= 7 || v >= 10) {
					struct pila *s = &P[P_SALIDA + i];

					s->c[s->n] = (u8)(i * 13 + v - 1);
					s->arriba[s->n++] = 1;
				}
		for (i = 0; i < 4; i++) {
			P[P_COL + i].c[0] = (u8)(i * 13 + ultimo_valor() - 1);
			P[P_COL + i].arriba[0] = 1;
			P[P_COL + i].n = 1;
		}
	} else {
		k = 0;
		for (i = 0; i < ncols; i++)
			for (v = 0; v <= i; v++) {
				struct pila *p = &P[P_COL + i];

				p->c[p->n] = (u8)mazo[k++];
				p->arriba[p->n] = v == i;
				p->n++;
			}
		for (; k < n; k++) {
			P[P_MAZO].c[P[P_MAZO].n] = (u8)mazo[k];
			P[P_MAZO].arriba[P[P_MAZO].n++] = 0;
		}
	}
	movimientos = 0;
	empezado = 0;
	segundos = 0;
	n_hist = 0;
	estado = E_JUEGA;
	ganadas_esta = 0;
	arrastra = -1;
	serie("solitario: partida nueva (baraja %s): %d cartas, %d en el mazo\n", espanola ? "española" : "francesa", n, P[P_MAZO].n);
}

static const char *clave(const char *k)
{
	static char t[40];

	if (espanola)
		snprintf(t, sizeof t, "%s", k);
	else
		snprintf(t, sizeof t, "Francesa %c%s", k[0] + 32, k + 1);
	return t;
}

static void apunta_jugada(void)
{
	if (!historia)
		return;
	if (n_hist == MAX_DESHACER) {
		memmove(historia, historia + 1, (MAX_DESHACER - 1) * sizeof *historia);
		n_hist--;
	}
	memcpy(historia[n_hist].p, P, sizeof P);
	historia[n_hist].movimientos = movimientos;
	n_hist++;
}

static void empieza_reloj(void)
{
	if (!empezado) {
		empezado = 1;
		t0 = jg_tics();
		rec_poner_entero(clave("Jugadas"), rec_entero(clave("Jugadas"), 0) + 1);
		rec_guardar();
	}
}

static void destapa(void)
{
	int k;

	for (k = 0; k < ncols; k++) {
		struct pila *p = &P[P_COL + k];

		if (p->n && !p->arriba[p->n - 1])
			p->arriba[p->n - 1] = 1;
	}
}

static void mira_si_gana(void)
{
	int s, n = 0;

	for (s = 0; s < 4; s++)
		n += P[P_SALIDA + s].n;
	if (n < 4 * por_palo || estado == E_GANADA)
		return;
	estado = E_GANADA;
	if (!ganadas_esta) {
		int t = rec_entero(clave("Tiempo"), 0), m = rec_entero(clave("Movimientos"), 0);

		ganadas_esta = 1;
		rec_poner_entero(clave("Ganadas"), rec_entero(clave("Ganadas"), 0) + 1);
		if (!t || segundos < t)
			rec_poner_entero(clave("Tiempo"), MAX(1, segundos));
		if (!m || movimientos < m)
			rec_poner_entero(clave("Movimientos"), movimientos);
		rec_guardar();
	}
	{
		static const int nt[6] = { 72, 76, 79, 84, 79, 84 };
		int i;

		for (i = 0; i < 6; i++)
			snd_nota_tras(ON_CAMPANA, nota_hz(nt[i]), 300, 24, i * 120);
	}
	serie("solitario: ¡ganada! %d movimientos en %d s (baraja %s)\n", movimientos, segundos, espanola ? "española" : "francesa");
}

/* mueve n cartas de lo alto de la pila a a la b */
static void mueve(int a, int n, int b)
{
	struct pila *pa = &P[a], *pb = &P[b];
	int i;

	apunta_jugada();
	empieza_reloj();
	for (i = 0; i < n; i++) {
		pb->c[pb->n] = pa->c[pa->n - n + i];
		pb->arriba[pb->n] = 1;
		pb->n++;
	}
	pa->n = (u8)(pa->n - n);
	movimientos++;
	destapa();
	snd_efecto(b >= P_SALIDA && b < P_COL ? &s_sube : &s_carta);
	mira_si_gana();
}

static void saca(void)
{
	struct pila *m = &P[P_MAZO], *d = &P[P_DESCARTE];

	if (estado != E_JUEGA)
		return;
	apunta_jugada();
	empieza_reloj();
	if (m->n) {
		d->c[d->n] = m->c[m->n - 1];
		d->arriba[d->n++] = 1;
		m->n--;
	} else {
		/* la vuelta al mazo */
		while (d->n) {
			m->c[m->n] = d->c[d->n - 1];
			m->arriba[m->n++] = 0;
			d->n--;
		}
	}
	movimientos++;
	snd_efecto(&s_carta);
}

/* sube la carta de arriba de la pila a si puede: 1 si la sube */
static int a_salida(int a)
{
	struct pila *p = &P[a];
	int s;

	if (!p->n || !p->arriba[p->n - 1] || (a >= P_SALIDA && a < P_COL))
		return 0;
	for (s = 0; s < 4; s++)
		if (puede_salida(p->c[p->n - 1], s)) {
			mueve(a, 1, P_SALIDA + s);
			return 1;
		}
	return 0;
}

static void sube_todas(void)
{
	int hay = 1;

	while (hay && estado == E_JUEGA) {
		int k;

		hay = a_salida(P_DESCARTE);
		for (k = 0; k < ncols; k++)
			hay |= a_salida(P_COL + k);
	}
}

static void deshace(void)
{
	if (!n_hist)
		return;
	n_hist--;
	memcpy(P, historia[n_hist].p, sizeof P);
	movimientos = historia[n_hist].movimientos;
	estado = E_JUEGA;
}

/* ---------------------------------------------------------------- pintar las cartas */
static u32 color_art(char ch, int palo)
{
	static const u32 pal[16] = { 0x1A1410, 0x3B2A20, 0x1F4E5A, 0x2C6675, 0xC2542D, 0xE07B4F, 0xC9B79C, 0x8F7F68,
				     0xFBF3E0, 0xFFFFFF, 0xE8B33A, 0x6B8E3D, 0x2F4B8F, 0x9E2B2B, 0x7FC6BF, 0xD8D2C4 };
	const unsigned char *fc = espanola ? sol_cole : sol_colf;

	if (ch >= '0' && ch <= '9')
		return pal[ch - '0'];
	if (ch >= 'a' && ch <= 'f')
		return pal[ch - 'a' + 10];
	if (ch == 'r')
		return 0xD01818;
	if (ch == 'n')
		return 0x101010;
	if (ch == 'p')
		return 0xFFCC99;
	if (ch == 'x' || ch == 'y' || ch == 'z') {
		int v = fc[palo * 3 + (ch - 'x')];

		if (!espanola && ch == 'x' && palo < 2)
			return 0xD01818;
		if (!espanola && ch == 'x' && palo >= 2)
			return 0x101010;
		return v == 16 ? 0xD01818 : v == 17 ? 0x101010 : pal[v & 15];
	}
	return 0xFF00FF;
}

/* un dibujo de Hispa-386 (an x al), escalado a w x h (el punto más cercano), derecho o del revés */
static void dibujo(lienzo *l, int x, int y, const char *d, int an, int al, int w, int h, int palo, int reves)
{
	int i, j;

	for (j = 0; j < h; j++)
		for (i = 0; i < w; i++) {
			int sx = i * an / w, sy = j * al / h;
			char ch;

			if (reves) {
				sx = an - 1 - sx;
				sy = al - 1 - sy;
			}
			ch = d[sy * an + sx];
			if (ch == '.')
				continue;
			if ((unsigned)(x + i) < (unsigned)l->an && (unsigned)(y + j) < (unsigned)l->al)
				l->p[(size_t)(y + j) * l->paso + x + i] = color_art(ch, palo);
		}
}

static void texto_reves(lienzo *l, int x, int y, const char *t, u32 c, int e)
{
	/* (del revés: se pinta en un lienzo aparte y se copia girado) */
	lienzo tmp;
	int w = ancho_texto(t, e) + 2, h = 16 * e + 2, i, j;

	lz_nuevo(&tmp, w, h);
	if (!tmp.p)
		return;
	lz_rect(&tmp, 0, 0, w, h, 0xFF00FF);
	lz_negrita(&tmp, 1, 1, t, c, e);
	for (j = 0; j < h; j++)
		for (i = 0; i < w; i++) {
			u32 v = tmp.p[(size_t)(h - 1 - j) * w + (w - 1 - i)];

			if (v != 0xFF00FF && (unsigned)(x + i) < (unsigned)l->an && (unsigned)(y + j) < (unsigned)l->al)
				l->p[(size_t)(y + j) * l->paso + x + i] = v;
		}
	lz_libre(&tmp);
}

static const char *const pip_f = sol_pipf, *const pip_e = sol_pipe;

static void pinta_cara(lienzo *l, int c)
{
	static const float pos[11][10][2] = {
		{{0}}, {{0.5f, 0.5f}},
		{{0.5f, 0.18f}, {0.5f, 0.82f}},
		{{0.5f, 0.18f}, {0.5f, 0.5f}, {0.5f, 0.82f}},
		{{0.28f, 0.18f}, {0.72f, 0.18f}, {0.28f, 0.82f}, {0.72f, 0.82f}},
		{{0.28f, 0.18f}, {0.72f, 0.18f}, {0.5f, 0.5f}, {0.28f, 0.82f}, {0.72f, 0.82f}},
		{{0.28f, 0.18f}, {0.72f, 0.18f}, {0.28f, 0.5f}, {0.72f, 0.5f}, {0.28f, 0.82f}, {0.72f, 0.82f}},
		{{0.28f, 0.18f}, {0.72f, 0.18f}, {0.5f, 0.34f}, {0.28f, 0.5f}, {0.72f, 0.5f}, {0.28f, 0.82f}, {0.72f, 0.82f}},
		{{0.28f, 0.18f}, {0.72f, 0.18f}, {0.5f, 0.34f}, {0.28f, 0.5f}, {0.72f, 0.5f}, {0.5f, 0.66f}, {0.28f, 0.82f}, {0.72f, 0.82f}},
		{{0.28f, 0.16f}, {0.72f, 0.16f}, {0.28f, 0.39f}, {0.72f, 0.39f}, {0.5f, 0.5f}, {0.28f, 0.61f}, {0.72f, 0.61f}, {0.28f, 0.84f}, {0.72f, 0.84f}},
		{{0.28f, 0.16f}, {0.72f, 0.16f}, {0.5f, 0.28f}, {0.28f, 0.39f}, {0.72f, 0.39f}, {0.28f, 0.61f}, {0.72f, 0.61f}, {0.5f, 0.72f}, {0.28f, 0.84f}, {0.72f, 0.84f}} };
	static const char *const letras_f[14] = { "", "A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K" };
	int palo = palo_de(c), v = valor_de(c), w = l->an, h = l->al, r = MAX(3, w / 10);
	int ip = MAX(7, w * 9 / 72), pp = MAX(8, w * 13 / 72);
	const char *pip = espanola ? pip_e : pip_f;
	const char *peq = espanola ? (w >= 60 ? sol_peqe : sol_p7e) : (w >= 60 ? sol_peqf : sol_p7f);
	int peq_l = w >= 60 ? 9 : 7;
	u32 tinta = espanola ? (palo < 2 ? 0xC2542D : 0x1A1410) : (palo < 2 ? 0xD01818 : 0x101010);
	char t[4];
	int e = w >= 90 ? 2 : 1, k;

	lz_rect(l, 0, 0, w, h, 0x1F4E5A);	/* (lo de fuera de las esquinas no se copia: carta()) */
	lz_redondo(l, 0, 0, w, h, r, 0x8A8A8A);
	lz_redondo(l, 1, 1, w - 2, h - 2, r - 1, 0xFFFFFF);
	/* el índice: arriba a la izquierda, y girado abajo a la derecha */
	if (espanola)
		snprintf(t, sizeof t, "%d", v);
	else
		snprintf(t, sizeof t, "%s", letras_f[v]);
	lz_negrita(l, 3, 3, t, tinta, e);
	dibujo(l, 3 + (int)strlen(t) * 4 * e - ip / 2 + 1, 4 + 16 * e, peq + palo * peq_l * peq_l, peq_l, peq_l, ip, ip, palo, 0);
	texto_reves(l, w - 4 - ancho_texto(t, e) - 2, h - 4 - 16 * e - 2, t, tinta, e);
	dibujo(l, w - 4 - (int)strlen(t) * 4 * e - ip / 2 - 1, h - 5 - 16 * e - ip, peq + palo * peq_l * peq_l, peq_l, peq_l, ip, ip, palo, 1);
	if (v <= 10 && !(espanola && v == 10)) {
		/* los números: las pintas en su sitio (las de abajo, del revés); el as, una grande */
		int n = v, x0 = w / 6, y0 = h / 8, aw = w - 2 * x0, ah = h - 2 * y0;

		if (v == 1) {
			int g = w / 2;

			dibujo(l, (w - g) / 2, (h - g) / 2, pip + palo * 169, 13, 13, g, g, palo, 0);
			return;
		}
		for (k = 0; k < n; k++) {
			float px = pos[n][k][0], py = pos[n][k][1];

			dibujo(l, x0 + (int)(px * aw) - pp / 2, y0 + (int)(py * ah) - pp / 2, pip + palo * 169, 13, 13, pp, pp, palo, py > 0.55f);
		}
		return;
	}
	/* las figuras: en un recuadro */
	{
		int fx = w / 5, fy = h / 7, fw = w - 2 * fx, fh = h - 2 * fy, f = espanola ? (v - 10) : (v - 11);

		lz_rect(l, fx - 1, fy - 1, fw + 2, fh + 2, 0xB8B8B8);
		lz_rect(l, fx, fy, fw, fh, 0xFBF3E0);
		if (!espanola) {
			/* de dos cabezas: la mitad de arriba y, del revés, la de abajo */
			dibujo(l, fx, fy, sol_figf + f * 19 * 20, 19, 20, fw, fh / 2, palo, 0);
			dibujo(l, fx, fy + fh / 2, sol_figf + f * 19 * 20, 19, 20, fw, fh - fh / 2, palo, 1);
		} else {
			/* de cuerpo entero, con su palo debajo */
			int ph = fh * 5 / 26;

			dibujo(l, fx, fy, sol_fige + f * 19 * 26, 19, 26, fw, fh - ph, palo, 0);
			dibujo(l, fx + (fw - ph) / 2, fy + fh - ph, pip + palo * 169, 13, 13, ph, ph, palo, 0);
		}
	}
}

static void pinta_dorso(lienzo *l)
{
	int w = l->an, h = l->al, r = MAX(3, w / 10), i;

	lz_rect(l, 0, 0, w, h, 0x1F4E5A);
	lz_redondo(l, 0, 0, w, h, r, 0x8A8A8A);
	lz_redondo(l, 1, 1, w - 2, h - 2, r - 1, 0xFFFFFF);
	lz_redondo(l, 4, 4, w - 8, h - 8, MAX(2, r - 3), 0x2F4B8F);
	/* la celosía */
	for (i = -h; i < w; i += MAX(4, w / 9)) {
		lienzo t;

		lz_trozo(&t, l, 5, 5, w - 10, h - 10);
		lz_linea(&t, i, 0, i + h, h, 0x4A66A8);
		lz_linea(&t, i + h, 0, i, h, 0x4A66A8);
	}
	{
		float q[8] = { w / 2.0f, h * 0.3f, w * 0.72f, h / 2.0f, w / 2.0f, h * 0.7f, w * 0.28f, h / 2.0f };

		lz_poligono(l, q, 4, 0xE8B33A);
	}
}

static void hace_cartas(void)
{
	int c;

	if (hechas == CA * 1000 + CL + espanola * 1000000)
		return;
	hechas = CA * 1000 + CL + espanola * 1000000;
	for (c = 0; c < MAX_CARTAS; c++) {
		lz_libre(&cara[c]);
		lz_nuevo(&cara[c], CA, CL);
		if (cara[c].p && (!espanola || valor_de(c) <= 7 || (valor_de(c) >= 10 && valor_de(c) <= 12)))
			pinta_cara(&cara[c], c);
	}
	lz_libre(&dorso);
	lz_nuevo(&dorso, CA, CL);
	if (dorso.p)
		pinta_dorso(&dorso);
}

/* ---------------------------------------------------------------- dónde está cada cosa */
static void sitio_pila(int p, int *x, int *y)
{
	if (p == P_MAZO) { *x = ox; *y = oy; return; }
	if (p == P_DESCARTE) { *x = ox + CA + sep; *y = oy; return; }
	if (p < P_COL) { *x = ox + (ncols - 4 + (p - P_SALIDA)) * (CA + sep); *y = oy; return; }
	*x = ox + (p - P_COL) * (CA + sep);
	*y = fila2;
}

/* lo que baja cada carta en una columna (si no caben, se juntan) */
static void pasos_columna(int k, int *pt, int *pv)
{
	struct pila *p = &P[P_COL + k];
	int i, t = 0, v = 0, alto = LZ.al - pie_al - fila2 - CL - 4;

	for (i = 0; i + 1 < p->n; i++)
		if (p->arriba[i]) v++; else t++;
	*pt = paso_tapada;
	*pv = paso_vista;
	while (t * *pt + v * *pv > alto && *pv > 8) {
		if (*pt > 4) (*pt)--;
		(*pv)--;
	}
}

static int y_carta(int k, int i)
{
	struct pila *p = &P[P_COL + k];
	int pt, pv, j, y = fila2;

	pasos_columna(k, &pt, &pv);
	for (j = 0; j < i; j++)
		y += p->arriba[j] ? pv : pt;
	return y;
}

static void pinta_hueco(lienzo *l, int x, int y, const char *t)
{
	lz_redondo(l, x, y, CA, CL, MAX(3, CA / 10), 0x2C6675);
	lz_redondo(l, x + 2, y + 2, CA - 4, CL - 4, MAX(3, CA / 10) - 1, 0x1F4E5A);
	if (t)
		lz_texto_centro(l, x + CA / 2, y + CL / 2 - 8, t, 0x7FC6BF, 1, 1);
}

/* una carta: sin lo de fuera de sus esquinas redondas */
static void carta(lienzo *l, int x, int y, int c, int arriba_)
{
	const lienzo *o = arriba_ ? &cara[c] : &dorso;
	int r = MIN(MAX(3, CA / 10), 16), f;

	if (!o->p)
		return;
	if (esquina_r != r) {
		esquina_r = r;
		for (f = 0; f < 16; f++) {
			float dy = (float)(r - f) - 0.5f;

			esquina[f] = f < r ? r - (int)(m_raiz((float)(r * r) - dy * dy) + 0.5f) : 0;
		}
	}
	for (f = 0; f < CL; f++) {
		int e = f < r ? esquina[f] : f >= CL - r ? esquina[CL - 1 - f] : 0, yy = y + f, x0 = x + e, x1 = x + CA - e;

		if (yy < 0 || yy >= l->al)
			continue;
		if (x0 < 0)
			x0 = 0;
		if (x1 > l->an)
			x1 = l->an;
		if (x1 > x0)
			memcpy(l->p + (size_t)yy * l->paso + x0, o->p + (size_t)f * o->paso + (x0 - x), (size_t)(x1 - x0) * 4);
	}
}

/* todo menos lo que se arrastra */
static void pinta_mesa(lienzo *l)
{
	int i, k, x, y;
	char t[120];

	lz_rect(l, 0, arriba, l->an, l->al - arriba, 0x1F4E5A);
	/* el tapete: unos rombos muy suaves */
	for (y = arriba; y < l->al; y += 48)
		for (x = 0; x < l->an; x += 48) {
			float q[8] = { x + 24.0f, y + 6.0f, x + 42.0f, y + 24.0f, x + 24.0f, y + 42.0f, x + 6.0f, y + 24.0f };

			lz_poligono_mezcla(l, q, 4, 0x2C6675, 90);
		}
	/* el mazo y el descarte */
	sitio_pila(P_MAZO, &x, &y);
	if (P[P_MAZO].n)
		carta(l, x, y, P[P_MAZO].c[P[P_MAZO].n - 1], 0);
	else
		pinta_hueco(l, x, y, "otra vez");
	sitio_pila(P_DESCARTE, &x, &y);
	if (P[P_DESCARTE].n > (arrastra >= 0 && arr_pila == P_DESCARTE ? 1 : 0)) {
		int n = P[P_DESCARTE].n - (arrastra >= 0 && arr_pila == P_DESCARTE ? 1 : 0);

		carta(l, x, y, P[P_DESCARTE].c[n - 1], 1);
	} else
		pinta_hueco(l, x, y, 0);
	for (i = 0; i < 4; i++) {
		struct pila *p = &P[P_SALIDA + i];
		int n = p->n - (arrastra >= 0 && arr_pila == P_SALIDA + i ? 1 : 0);

		sitio_pila(P_SALIDA + i, &x, &y);
		if (n > 0)
			carta(l, x, y, p->c[n - 1], 1);
		else
			pinta_hueco(l, x, y, "A");
	}
	for (k = 0; k < ncols; k++) {
		struct pila *p = &P[P_COL + k];
		int n = p->n - (arrastra >= 0 && arr_pila == P_COL + k ? p->n - arr_desde : 0);

		sitio_pila(P_COL + k, &x, &y);
		if (!n)
			pinta_hueco(l, x, y, 0);
		for (i = 0; i < n; i++)
			carta(l, x, y_carta(k, i), p->c[i], p->arriba[i]);
	}
	/* abajo: los movimientos, el tiempo y el mazo */
	lz_rect(l, 0, l->al - pie_al, l->an, pie_al, 0x163A44);
	snprintf(t, sizeof t, "Movimientos: %d   Tiempo: %d:%02d   Mazo: %d   Baraja %s", movimientos, segundos / 60, segundos % 60, P[P_MAZO].n, espanola ? "española" : "francesa");
	lz_texto(l, 8, l->al - pie_al + 3, t, 0xD8ECEA, 1);
}

static void pinta_arrastre(void)
{
	struct pila *p = &P[arr_pila];
	int i, n = p->n - arr_desde, pt, pv, y = arr_y;

	if (ant_x >= 0) {
		lz_copia(&LZ, ant_x, ant_y, &capa, ant_x, ant_y, ant_an, ant_al);
		jg_sucio(ant_x, ant_y, ant_an, ant_al);
	}
	pv = paso_vista;
	if (arr_pila >= P_COL)
		pasos_columna(arr_pila - P_COL, &pt, &pv);
	for (i = 0; i < n; i++) {
		carta(&LZ, arr_x, y, p->c[arr_desde + i], 1);
		y += pv;
	}
	ant_x = arr_x - 2;
	ant_y = arr_y - 2;
	ant_an = CA + 4;
	ant_al = (n - 1) * pv + CL + 4;
	jg_sucio(ant_x, ant_y, ant_an, ant_al);
}

static void pinta_todo(void)
{
	pinta_mesa(&LZ);
	if (!jg_completa)
		jg_pinta_barra();
	if (estado == E_GANADA) {
		char t[160];

		snprintf(t, sizeof t, "%d movimientos en %d:%02d.\nF2: otra partida.", movimientos, segundos / 60, segundos % 60);
		jg_cartel(&LZ, LZ.an / 2 - 170, fila2, 340, LZ.al - fila2 - pie_al, "¡Ganada!", t);
	}
	jg_todo_sucio();
}

static void pinta(void)
{
	static int s_antes = -1;

	if (arrastra >= 0) {
		pinta_arrastre();
		return;
	}
	if (s_antes != segundos) {
		s_antes = segundos;
		pinta_todo();
	}
}

static void tamano(int an, int al)
{
	int w;

	arriba = jg_completa ? 0 : ALTO_BARRA;
	/* la escala mayor con la que caben las columnas a lo ancho (y una columna larga a lo alto) */
	w = (an - 16) / (ncols + (ncols - 1) * 0.12f + 0.3f);
	CA = LIMITA(w, 40, 110);
	CL = CA * 100 / 72;
	if (arriba + 10 + CL + 12 + 6 * (CL / 9) + 12 * (CL / 4) + CL + pie_al > al) {
		int h = (al - arriba - pie_al - 24) * 100 / 330;

		CL = LIMITA(MIN(CL, h), 55, 152);
		CA = CL * 72 / 100;
	}
	sep = CA / 8;
	ox = (an - (ncols * CA + (ncols - 1) * sep)) / 2;
	oy = arriba + 10;
	fila2 = oy + CL + 14;
	paso_tapada = MAX(5, CL / 9);
	paso_vista = MAX(12, CL / 4);
	hace_cartas();
	pinta_todo();
}

/* ---------------------------------------------------------------- el ratón */
/* qué hay en (x, y): la pila y la carta (o -1) */
static int en(int x, int y, int *pila, int *idx)
{
	int p, px, py, i;

	for (p = 0; p < P_COL; p++) {
		if (p >= 2 && p < P_SALIDA)
			continue;
		sitio_pila(p, &px, &py);
		if (x >= px && x < px + CA && y >= py && y < py + CL) {
			*pila = p;
			*idx = P[p].n - 1;
			return 1;
		}
	}
	for (p = 0; p < ncols; p++) {
		struct pila *q = &P[P_COL + p];

		sitio_pila(P_COL + p, &px, &py);
		if (x < px || x >= px + CA)
			continue;
		for (i = q->n - 1; i >= 0; i--) {
			int cy = y_carta(p, i);

			if (y >= cy && y < cy + CL) {
				*pila = P_COL + p;
				*idx = i;
				return 1;
			}
		}
		if (!q->n && y >= py && y < py + CL) {
			*pila = P_COL + p;
			*idx = -1;
			return 1;
		}
	}
	return 0;
}

static int escalera_valida(int pila, int desde)
{
	struct pila *p = &P[pila];
	int i;

	if (!p->arriba[desde])
		return 0;
	for (i = desde; i + 1 < p->n; i++)
		if (rojo(p->c[i]) == rojo(p->c[i + 1]) || sube(valor_de(p->c[i + 1])) != valor_de(p->c[i]))
			return 0;
	return 1;
}

static void suelta(int x, int y)
{
	struct pila *p = &P[arr_pila];
	int n = p->n - arr_desde, c = p->c[arr_desde], dest = -1, k, mejor = 1 << 30;
	int cx = arr_x + CA / 2, cy = arr_y + CL / 2;

	(void)x;
	(void)y;
	/* el montón donde caiga más cerca (el centro de la carta) */
	for (k = 0; k < 4; k++) {
		int px, py, d;

		sitio_pila(P_SALIDA + k, &px, &py);
		d = abs(px + CA / 2 - cx) + abs(py + CL / 2 - cy);
		if (n == 1 && d < CA && d < mejor && puede_salida(c, k)) {
			mejor = d;
			dest = P_SALIDA + k;
		}
	}
	for (k = 0; k < ncols; k++) {
		int px, py, d, top = P[P_COL + k].n ? y_carta(k, P[P_COL + k].n - 1) : fila2;

		if (P_COL + k == arr_pila)
			continue;
		sitio_pila(P_COL + k, &px, &py);
		d = abs(px + CA / 2 - cx) + abs(top + CL / 2 - cy);
		if (d < CA + CL / 2 && d < mejor && puede_columna(c, k)) {
			mejor = d;
			dest = P_COL + k;
		}
	}
	arrastra = -1;
	ant_x = -1;
	if (dest >= 0)
		mueve(arr_pila, n, dest);
	pinta_todo();
}

/* ---------------------------------------------------------------- los menús y los sucesos */
static char op_juego[300];
static const char *menus_op[2];

static void pon_menus(void)
{
	snprintf(op_juego, sizeof op_juego, "Partida nueva   F2|Deshacer   Retroceso|Subir las que se puedan   S|-|%sBaraja francesa|%sBaraja española|-|%sSonidos|-|Estadísticas...|-|Salir",
		 espanola ? "" : "» ", espanola ? "» " : "", snd_mudo ? "" : "» ");
	menus_op[0] = op_juego;
	menus_op[1] = "Cómo se juega...   F1|-|Acerca de Solitario...";
	jg_menus("Juego|Ayuda", menus_op);
}

static void estadisticas(void)
{
	char t[400];
	int j = rec_entero(clave("Jugadas"), 0), g = rec_entero(clave("Ganadas"), 0), ti = rec_entero(clave("Tiempo"), 0), m = rec_entero(clave("Movimientos"), 0);

	snprintf(t, sizeof t, "Baraja %s\n\nPartidas jugadas: %d\nGanadas: %d (%d %%)\nEl mejor tiempo: %d:%02d\nLos menos movimientos: %d",
		 espanola ? "española" : "francesa", j, g, j ? g * 100 / j : 0, ti / 60, ti % 60, m);
	jg_aviso("Estadísticas", t);
}

static void ayuda(void)
{
	jg_aviso("Cómo se juega", "Suba todas las cartas a los cuatro montones de arriba a la derecha, uno por palo, del as al rey.\n\n"
		 "En las columnas, las cartas bajan de una en una alternando los colores (rojo y negro; en la española, oros y copas con espadas y bastos). "
		 "A un hueco solo va un rey. Un clic en el mazo saca una carta; vacío, le da la vuelta.\n\n"
		 "Ratón: arrastrar una carta (o una escalera); dos clics o el botón derecho la suben. Teclado: Espacio saca, S sube las que se puedan, "
		 "Retroceso o Ctrl+Z deshacen, F2 otra partida.");
}

static void pon_baraja(int esp)
{
	espanola = esp;
	rec_poner("Baraja", esp ? "española" : "francesa");
	rec_guardar();
	baraja_nueva();
	pon_menus();
	hechas = 0;
	tamano(LZ.an, LZ.al);
}

static void pulso(void)
{
	if (estado == E_JUEGA && empezado) {
		int s = (int)((jg_tics() - t0) / jg_hz);

		if (s != segundos) {
			segundos = s;
			jg_cambio = 1;
		}
	}
	if (arrastra >= 0)
		jg_cambio = 1;
}

static void suceso(const struct suceso *s)
{
	int m, o;

	switch (s->clase) {
	case EV_TECLA:
		switch (s->a) {
		case 0x158: {
			int k, n = 0;

			for (k = 0; k < 4; k++)
				n += P[P_SALIDA + k].n;
			serie("solitario: estado=%d baraja=%s movimientos=%d mazo=%d descarte=%d salidas=%d segundos=%d sonido=%d\n", estado, espanola ? "española" : "francesa",
			      movimientos, P[P_MAZO].n, P[P_DESCARTE].n, n, segundos, snd_hay);
			return;
		}
		case T_F2: baraja_nueva(); pinta_todo(); return;
		case T_BORRAR: case 26: deshace(); pinta_todo(); return;
		case ' ': saca(); pinta_todo(); return;
		case 's': case 'S': sube_todas(); pinta_todo(); return;
		case 0x13B: ayuda(); return;
		}
		return;
	case EV_RATON_BAJA: {
		int x = (int)(int32_t)s->a, y = (int)(int32_t)s->b, p, i, veces = (int)(s->c >> 16);

		if (jg_clic_barra(x, y) || estado != E_JUEGA)
			return;
		if (!en(x, y, &p, &i))
			return;
		if (p == P_MAZO) {
			saca();
			pinta_todo();
			return;
		}
		if ((s->c & BOTON_DERECHO) || veces >= 2) {
			if (i >= 0 && i == P[p].n - 1 && a_salida(p))
				pinta_todo();
			return;
		}
		if (i < 0 || (p >= P_COL && !escalera_valida(p, i)) || (p < P_COL && i != P[p].n - 1))
			return;
		/* empieza a arrastrar: la mesa sin lo que se lleva, a la capa */
		arrastra = 1;
		arr_pila = p;
		arr_desde = i;
		{
			int px, py;

			sitio_pila(p, &px, &py);
			if (p >= P_COL)
				py = y_carta(p - P_COL, i);
			arr_dx = x - px;
			arr_dy = y - py;
			arr_x = arr_ox = px;
			arr_y = arr_oy = py;
		}
		movido = 0;
		lz_libre(&capa);
		lz_nuevo(&capa, LZ.an, LZ.al);
		if (!capa.p) {
			arrastra = -1;
			return;
		}
		pinta_mesa(&capa);
		lz_copia(&LZ, 0, arriba, &capa, 0, arriba, LZ.an, LZ.al - arriba);
		jg_todo_sucio();
		ant_x = -1;
		pinta_arrastre();
		return;
	}
	case EV_RATON_MUEVE:
		if (arrastra >= 0) {
			arr_x = (int)(int32_t)s->a - arr_dx;
			arr_y = (int)(int32_t)s->b - arr_dy;
			movido = 1;
			jg_cambio = 1;
		}
		return;
	case EV_RATON_SUBE:
		if (arrastra >= 0) {
			if (!movido) {
				/* un clic sin mover: si la carta puede subir, sube */
				int p = arr_pila;

				arrastra = -1;
				ant_x = -1;
				(void)p;
				pinta_todo();
			} else
				suelta((int)(int32_t)s->a, (int)(int32_t)s->b);
		}
		return;
	case EV_MENU:
		o = jg_menu_elegido(s, &m);
		if (o < 0)
			return;
		if (m == 1) {
			if (o == 0)
				ayuda();
			else
				jg_aviso("Acerca de Solitario", "Solitario 1.0, para Hispa-Pentium (el de Hispa-386, con sus dos barajas dibujadas allí).\n\nCopyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.");
			return;
		}
		switch (o) {
		case 0: baraja_nueva(); pinta_todo(); break;
		case 1: deshace(); pinta_todo(); break;
		case 2: sube_todas(); pinta_todo(); break;
		case 4: case 5: if (espanola != (o == 5)) pon_baraja(o == 5); break;
		case 7: snd_mudo = !snd_mudo; rec_poner_entero("Sonidos", !snd_mudo); rec_guardar(); pon_menus(); break;
		case 9: estadisticas(); break;
		case 11: jg_salir = 1; break;
		}
		return;
	}
}

int principal(void)
{
	static const struct juego j = { pulso, pinta, suceso, tamano, 0, 60 };
	int an, al;
	u32 n, i;
	const char *b;

	n = sc(SC_ARGUMENTOS, (u32)jg_args, sizeof jg_args - 1, 0);
	if (ES_ERROR(n))
		n = 0;
	jg_args[MIN(n, sizeof jg_args - 1)] = 0;
	for (i = 0; jg_args[i]; i++)
		if (jg_args[i] >= 'a' && jg_args[i] <= 'z')
			jg_args[i] = (char)(jg_args[i] - 32);
	jg_tamano_maximo(&an, &al);
	if (jg_abrir("Solitario", MIN(an, 640), MIN(al, 580), 0))
		return 1;
	azar_semilla((u32)arg_entero("/S=", (int)(jg_tics() * 2654435761u ^ (u32)rdtsc())));
	casi = arg_hay("/CASI");
	rec_abrir("SOLITARI.REC");
	b = rec_texto("Baraja");
	espanola = b && b[0] == 'e';
	snd_mudo = !rec_entero("Sonidos", 1);
	historia = (struct jugada *)malloc(MAX_DESHACER * sizeof *historia);
	snd_abrir();
	pon_menus();
	baraja_nueva();
	tamano(LZ.an, LZ.al);
	serie("solitario: ventana %d x %d (cartas de %d x %d); lo guardado en %s\n", LZ.an, LZ.al, CA, CL, rec_ruta());
	return jg_correr(&j);
}
