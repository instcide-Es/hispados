/* tragabol.c - (JUEGOS) Tragabolas, el de Hispa-386 (juegos/tragabolas.asm)
 * en Hispa-Pentium: el Tragón (una bola azul petróleo con ojos) recoge las
 * bolitas de un jardín de setos (25 x 17 casillas) esquivando a tres erizos:
 * el granate va a por él (una de cada seis veces, a cualquier sitio), el añil
 * anda al azar y el marrón le sale al paso (cuatro casillas por delante).
 * Tres jardines propios (los de Hispa-386) que se turnan; cada vuelta va más
 * deprisa (220, 165 y 110 ms por paso) y el sueño dura menos (32, 24 y 16
 * pasos). Cuatro campanas por jardín: al coger una, los erizos se duermen
 * (grises, con los ojos cerrados) y no pinchan. Tres vidas. Puntos: 10 por
 * bolita, 50 por campana y 100 por el número del jardín al dejarlo limpio.
 * Ahora todo se mueve suave de casilla a casilla, la ventana se estira,
 * suena y los récords (los diez mejores) van a la carpeta del usuario.
 * Flechas (la pedida se recuerda hasta que se puede girar), P o Espacio
 * pausa, F2 partida nueva, F11. Para las pruebas: /S=n, /N=n (el jardín) y
 * F12 (el estado).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "juego.h"
#include "datos386.h"

#define COLS 25
#define FILAS 17
enum { E_LISTO, E_JUEGA, E_PAUSA, E_PINCHAZO, E_LIMPIO, E_FIN };
enum { M_SUELO, M_SETO, M_BOLA, M_CAMPANA };
static const int dxs[4] = { 1, -1, 0, 0 }, dys[4] = { 0, 0, 1, -1 };
static const u32 color_erizo[3] = { 0x9E2B2B, 0x2F4B8F, 0x5A3A20 };

struct actor { int x, y, d, nd, ax, ay; };	/* la casilla, la dirección (0-3, -1 quieto), la pedida y la casilla de antes */
static u8 mapa[FILAS][COLS];
static struct actor tragon, erizos[3];
static int estado = E_LISTO, jardin, vueltas, vidas = 3, dormidos, bolitas, record_pos = -1, t_estado, jardin_ini;
static long puntos;
static int t_paso, ms_paso = 220;	/* el paso: todos a la vez */
static int cel = 22, ox, oy, arriba, cab_al = 30;
static lienzo capa;
static struct { int x, y, an, al; } s_antes[8], s_ahora[8];
static int n_s_antes, n_s_ahora;

static const struct efecto s_bolita = { ON_CUADRADA, 990, 990, 18, 10, 1, 12 };
static const struct efecto s_campana = { ON_CAMPANA, 1320, 1320, 500, 30, 1, 450 };
static const struct efecto s_pincha = { ON_TRIANGULO, 700, 90, 800, 36, 2, 700 };

static void pinta_todo(void);

static int vuelta(void) { return MIN(vueltas, 2); }

static void empieza_jardin(void)
{
	const char *d = trg_jardin[jardin % 3];
	int x, y, i;

	bolitas = 0;
	for (y = 0; y < FILAS; y++)
		for (x = 0; x < COLS; x++) {
			char c = d[y * COLS + x];

			mapa[y][x] = c == '#' ? M_SETO : c == '.' ? M_BOLA : c == 'o' ? M_CAMPANA : M_SUELO;
			if (c == '.' || c == 'o')
				bolitas++;
		}
	tragon.x = 12;
	tragon.y = 14;
	tragon.d = -1;
	tragon.nd = -1;
	for (i = 0; i < 3; i++) {
		erizos[i].x = 10 + 2 * i;
		erizos[i].y = 8;
		erizos[i].d = 3;
	}
	if (mapa[14][12] == M_BOLA) {
		mapa[14][12] = M_SUELO;
		bolitas--;
	}
	dormidos = 0;
	t_paso = 0;
	ms_paso = 220 - 55 * vuelta();
	tragon.ax = tragon.x;
	tragon.ay = tragon.y;
	for (i = 0; i < 3; i++) {
		erizos[i].ax = erizos[i].x;
		erizos[i].ay = erizos[i].y;
	}
	serie("tragabolas: jardín %d (vuelta %d): %d bolitas\n", jardin % 3 + 1, vueltas + 1, bolitas);
}

static void nueva_partida(void)
{
	puntos = 0;
	vidas = 3;
	jardin = jardin_ini;
	vueltas = 0;
	record_pos = -1;
	empieza_jardin();
	estado = E_JUEGA;
	pinta_todo();
}

static int seto(int x, int y)
{
	if (x < 0 || y < 0 || x >= COLS || y >= FILAS)
		return 1;
	return mapa[y][x] == M_SETO;
}

static void mueve_erizo(int i)
{
	struct actor *e = &erizos[i];
	int ox_ = tragon.x, oy_ = tragon.y, al_azar = i == 1 || (i == 0 && azar_n(6) == 0), mejor = -1, mejor_p = 1 << 30, d;

	if (i == 2 && tragon.d >= 0) {
		ox_ += 4 * dxs[tragon.d];
		oy_ += 4 * dys[tragon.d];
	}
	for (d = 0; d < 4; d++) {
		int p;

		if (e->d >= 0 && (d ^ 1) == e->d)
			continue;	/* media vuelta, no */
		if (seto(e->x + dxs[d], e->y + dys[d]))
			continue;
		p = al_azar ? azar_n(1000) : abs(e->x + dxs[d] - ox_) + abs(e->y + dys[d] - oy_);
		if (p < mejor_p) {
			mejor_p = p;
			mejor = d;
		}
	}
	if (mejor < 0 && e->d >= 0 && !seto(e->x + dxs[e->d ^ 1], e->y + dys[e->d ^ 1]))
		mejor = e->d ^ 1;	/* (un callejón: media vuelta) */
	e->d = mejor;
	if (mejor >= 0) {
		e->x += dxs[mejor];
		e->y += dys[mejor];
	}
}

static int pinchado(void)
{
	int i;

	if (dormidos)
		return 0;
	for (i = 0; i < 3; i++)
		if (erizos[i].x == tragon.x && erizos[i].y == tragon.y)
			return 1;
	return 0;
}

static void pinta_celda_capa(int x, int y);

static void paso(void)
{
	int i, ax = tragon.x, ay = tragon.y;

	tragon.ax = tragon.x;
	tragon.ay = tragon.y;
	for (i = 0; i < 3; i++) {
		erizos[i].ax = erizos[i].x;
		erizos[i].ay = erizos[i].y;
	}
	/* el Tragón: gira si puede a donde se pidió; si no, sigue */
	if (tragon.nd >= 0 && !seto(tragon.x + dxs[tragon.nd], tragon.y + dys[tragon.nd]))
		tragon.d = tragon.nd;
	if (tragon.d >= 0 && !seto(tragon.x + dxs[tragon.d], tragon.y + dys[tragon.d])) {
		tragon.x += dxs[tragon.d];
		tragon.y += dys[tragon.d];
	}
	/* ¿se cruza con un erizo que viene de frente? */
	for (i = 0; i < 3; i++) {
		int ex = erizos[i].x, ey = erizos[i].y;

		mueve_erizo(i);
		if (!dormidos && ex == tragon.x && ey == tragon.y && erizos[i].x == ax && erizos[i].y == ay) {
			erizos[i].x = tragon.x;
			erizos[i].y = tragon.y;
		}
	}
	if (dormidos)
		dormidos--;
	if (mapa[tragon.y][tragon.x] == M_BOLA || mapa[tragon.y][tragon.x] == M_CAMPANA) {
		if (mapa[tragon.y][tragon.x] == M_BOLA) {
			puntos += 10;
			snd_efecto(&s_bolita);
		} else {
			puntos += 50;
			dormidos = 32 - 8 * vuelta();
			snd_efecto(&s_campana);
			serie("tragabolas: campana: los erizos duermen %d pasos\n", dormidos);
		}
		mapa[tragon.y][tragon.x] = M_SUELO;
		pinta_celda_capa(tragon.x, tragon.y);
		if (--bolitas == 0) {
			puntos += 100 * (jardin % 3 + 1);
			estado = E_LIMPIO;
			t_estado = 0;
			{
				static const int n[5] = { 67, 72, 76, 79, 84 };

				for (i = 0; i < 5; i++)
					snd_nota_tras(ON_CUADRADA, nota_hz(n[i]), 140, 20, i * 130);
			}
			serie("tragabolas: jardín %d limpio; %ld puntos\n", jardin % 3 + 1, puntos);
		}
	}
	if (estado == E_JUEGA && pinchado()) {
		vidas--;
		snd_efecto(&s_pincha);
		serie("tragabolas: ¡pinchazo!; quedan %d vidas\n", vidas);
		if (vidas <= 0) {
			estado = E_FIN;
			record_pos = rec_meter("Puntos", REC_N, (int)puntos, 0);
			serie("tragabolas: fin de la partida: %ld puntos; récord %d\n", puntos, record_pos + 1);
		} else {
			estado = E_PINCHAZO;
			t_estado = 0;
		}
	}
	jg_cambio = 1;
}

static void pulso(void)
{
	if (estado == E_PINCHAZO || estado == E_LIMPIO) {
		if (++t_estado >= 150) {
			if (estado == E_LIMPIO) {
				jardin++;
				if (jardin % 3 == jardin_ini % 3)
					vueltas++;
				empieza_jardin();
				pinta_todo();
			} else {
				int i;

				tragon.x = tragon.ax = 12;
				tragon.y = tragon.ay = 14;
				tragon.d = tragon.nd = -1;
				for (i = 0; i < 3; i++) {
					erizos[i].x = erizos[i].ax = 10 + 2 * i;
					erizos[i].y = erizos[i].ay = 8;
					erizos[i].d = 3;
				}
				dormidos = 0;
			}
			estado = E_JUEGA;
		}
		jg_cambio = 1;
		return;
	}
	if (estado != E_JUEGA)
		return;
	t_paso += 10;
	if (t_paso >= ms_paso) {
		t_paso -= ms_paso;
		paso();
	}
	jg_cambio = 1;
}

/* ---- pintar ---- */
static void pinta_celda_capa(int x, int y)
{
	int px = ox + x * cel, py = oy + y * cel, m = mapa[y][x];

	lz_rect(&capa, px, py, cel, cel, 0xE4DDC8);
	if (m == M_SETO) {
		lz_rect(&capa, px, py, cel, cel, 0x3E6B2E);
		lz_circulo_suave(&capa, px + cel * 0.3f, py + cel * 0.35f, cel * 0.22f, 0x5A8A3E);
		lz_circulo_suave(&capa, px + cel * 0.7f, py + cel * 0.65f, cel * 0.2f, 0x4E7E36);
		lz_circulo_suave(&capa, px + cel * 0.65f, py + cel * 0.25f, cel * 0.12f, 0x6E9E4A);
	} else if (m == M_BOLA)
		lz_circulo_suave(&capa, px + cel / 2.0f, py + cel / 2.0f, MAX(1.5f, cel * 0.1f), 0xC2542D);
	else if (m == M_CAMPANA) {
		/* una campana: la copa, el borde y el badajo */
		float q[8] = { px + cel * 0.32f, py + cel * 0.25f, px + cel * 0.68f, py + cel * 0.25f, px + cel * 0.82f, py + cel * 0.72f, px + cel * 0.18f, py + cel * 0.72f };

		lz_poligono(&capa, q, 4, 0xE8B33A);
		lz_circulo_suave(&capa, px + cel / 2.0f, py + cel * 0.27f, cel * 0.18f, 0xE8B33A);
		lz_rect(&capa, px + (int)(cel * 0.14f), py + (int)(cel * 0.70f), (int)(cel * 0.72f), MAX(2, cel / 8), 0xB8862A);
		lz_circulo_suave(&capa, px + cel / 2.0f, py + cel * 0.84f, cel * 0.09f, 0x7A5A1A);
	}
	if (LZ.p && capa.p) {
		lz_copia(&LZ, px, py, &capa, px, py, cel, cel);
		jg_sucio(px, py, cel, cel);
	}
}

static void apunta(int x, int y, int an, int al)
{
	if (n_s_ahora < 8) {
		s_ahora[n_s_ahora].x = x;
		s_ahora[n_s_ahora].y = y;
		s_ahora[n_s_ahora].an = an;
		s_ahora[n_s_ahora].al = al;
		n_s_ahora++;
	}
}

/* dónde se pinta un actor: entre la casilla de antes y la de ahora, según lo que va del paso */
static void sitio(const struct actor *a, float *px, float *py)
{
	float t = estado == E_JUEGA ? (float)t_paso / (float)ms_paso : 1.0f;
	float x = (float)a->ax + ((float)a->x - (float)a->ax) * t, y = (float)a->ay + ((float)a->y - (float)a->ay) * t;

	*px = ox + x * cel + cel / 2.0f;
	*py = oy + y * cel + cel / 2.0f;
}

static void pinta_tragon(void)
{
	float x, y, r = cel * 0.42f;
	int d = tragon.d < 0 ? 0 : tragon.d, k;

	sitio(&tragon, &x, &y);
	if (estado == E_PINCHAZO && (t_estado / 10) % 2)
		r *= 0.6f;
	lz_circulo_suave(&LZ, x, y, r, 0x1F4E5A);
	lz_circulo_suave(&LZ, x - r * 0.3f, y - r * 0.3f, r * 0.35f, 0x3E7E8A);
	for (k = -1; k <= 1; k += 2) {
		float ex = x + dxs[d] * r * 0.35f + (dys[d] ? k * r * 0.35f : 0), ey = y + dys[d] * r * 0.35f + (dxs[d] ? k * r * 0.35f : -r * 0.15f);

		lz_circulo_suave(&LZ, ex, ey, r * 0.22f, 0xFFFFFF);
		lz_circulo_suave(&LZ, ex + dxs[d] * r * 0.08f, ey + dys[d] * r * 0.08f, r * 0.11f, 0x1A1410);
	}
	apunta((int)(x - r) - 2, (int)(y - r) - 2, (int)(2 * r) + 5, (int)(2 * r) + 5);
}

static void pinta_erizo(int i)
{
	float x, y, r = cel * 0.36f;
	u32 c = dormidos ? ((dormidos < 8 && (dormidos % 2)) ? color_erizo[i] : 0x8A8A8A) : color_erizo[i];
	int k;

	sitio(&erizos[i], &x, &y);
	/* las púas: un corro de triángulos */
	for (k = 0; k < 10; k++) {
		float a = k * M_PI_F / 5, q[6] = { x + r * 0.7f * m_cos(a - 0.3f), y + r * 0.7f * m_sen(a - 0.3f), x + r * 1.25f * m_cos(a), y + r * 1.25f * m_sen(a),
						     x + r * 0.7f * m_cos(a + 0.3f), y + r * 0.7f * m_sen(a + 0.3f) };

		lz_poligono(&LZ, q, 3, aclara(c, -60));
	}
	lz_circulo_suave(&LZ, x, y, r, c);
	if (dormidos) {
		lz_rect(&LZ, (int)(x - r * 0.55f), (int)(y - r * 0.1f), (int)(r * 0.4f), 2, 0xF0F0F0);
		lz_rect(&LZ, (int)(x + r * 0.15f), (int)(y - r * 0.1f), (int)(r * 0.4f), 2, 0xF0F0F0);
	} else
		for (k = -1; k <= 1; k += 2) {
			lz_circulo_suave(&LZ, x + k * r * 0.35f, y - r * 0.12f, r * 0.24f, 0xFFFFFF);
			lz_circulo_suave(&LZ, x + k * r * 0.35f, y - r * 0.05f, r * 0.12f, 0x1A1410);
		}
	apunta((int)(x - r * 1.3f) - 2, (int)(y - r * 1.3f) - 2, (int)(2.6f * r) + 5, (int)(2.6f * r) + 5);
}

static void pinta_cabecera(void)
{
	char t[120];
	struct record r[1];
	long rec = rec_tabla("Puntos", r, 1) ? r[0].puntos : 0;

	lz_rect(&LZ, 0, arriba, LZ.an, cab_al, 0xE9EDF2);
	snprintf(t, sizeof t, "Puntos: %ld   Jardín: %d   Vidas: %d   Récord: %ld%s", puntos, jardin % 3 + 1, vidas, MAX(rec, puntos), dormidos ? "   (duermen)" : "");
	lz_negrita(&LZ, ox, arriba + 7, t, 0x1A1410, 1);
	jg_sucio(0, arriba, LZ.an, cab_al);
}

static void pinta_carteles(void)
{
	int an = MIN(COLS * cel - 20, 360);
	char t[160];

	if (estado == E_LISTO)
		jg_cartel(&LZ, ox + (COLS * cel - an) / 2, oy, an, FILAS * cel, "Tragabolas", "Recoja todas las bolitas sin que le pinche un erizo.\nUna flecha o Intro: empezar.");
	else if (estado == E_PAUSA)
		jg_cartel(&LZ, ox + (COLS * cel - an) / 2, oy, an, FILAS * cel, "Pausa", "Una flecha o P para seguir.");
	else if (estado == E_LIMPIO)
		jg_cartel(&LZ, ox + (COLS * cel - an) / 2, oy, an, FILAS * cel, "¡Jardín limpio!", "Al siguiente...");
	else if (estado == E_FIN) {
		if (record_pos >= 0)
			snprintf(t, sizeof t, "%ld puntos. ¡Récord: el %d.º!\nIntro: otra partida.", puntos, record_pos + 1);
		else
			snprintf(t, sizeof t, "%ld puntos.\nIntro: otra partida.", puntos);
		jg_cartel(&LZ, ox + (COLS * cel - an) / 2, oy, an, FILAS * cel, "Fin de la partida", t);
	} else
		return;
	jg_sucio(ox, oy, COLS * cel, FILAS * cel);
}

static void pinta_todo(void)
{
	int x, y;

	lz_libre(&capa);
	lz_nuevo(&capa, LZ.an, LZ.al);
	lz_rect(&LZ, 0, 0, LZ.an, LZ.al, 0xE9EDF2);
	if (!jg_completa)
		jg_pinta_barra();
	if (capa.p) {
		lz_rect(&capa, 0, 0, capa.an, capa.al, 0xE9EDF2);
		lz_rect(&capa, ox - 3, oy - 3, COLS * cel + 6, FILAS * cel + 6, 0x2A4A20);
		for (y = 0; y < FILAS; y++)
			for (x = 0; x < COLS; x++)
				pinta_celda_capa(x, y);
		lz_copia(&LZ, 0, arriba + cab_al, &capa, 0, arriba + cab_al, LZ.an, LZ.al - arriba - cab_al);
	}
	n_s_antes = 0;
	pinta_cabecera();
	jg_todo_sucio();
	jg_cambio = 1;
}

static void pinta(void)
{
	static long p_antes = -1;
	static int v_antes = -1, d_antes = -1, e_antes = -1;
	int i;

	if (!capa.p)
		return;
	if (e_antes != estado) {
		e_antes = estado;
		lz_copia(&LZ, ox, oy, &capa, ox, oy, COLS * cel, FILAS * cel);
		jg_sucio(ox, oy, COLS * cel, FILAS * cel);
		n_s_antes = 0;
	}
	for (i = 0; i < n_s_antes; i++) {
		lz_copia(&LZ, s_antes[i].x, s_antes[i].y, &capa, s_antes[i].x, s_antes[i].y, s_antes[i].an, s_antes[i].al);
		jg_sucio(s_antes[i].x, s_antes[i].y, s_antes[i].an, s_antes[i].al);
	}
	n_s_ahora = 0;
	if (estado != E_LISTO) {
		for (i = 0; i < 3; i++)
			pinta_erizo(i);
		pinta_tragon();
	}
	for (i = 0; i < n_s_ahora; i++)
		jg_sucio(s_ahora[i].x, s_ahora[i].y, s_ahora[i].an, s_ahora[i].al);
	memcpy(s_antes, s_ahora, sizeof s_antes);
	n_s_antes = n_s_ahora;
	pinta_carteles();
	if (p_antes != puntos || v_antes != vidas || d_antes != !!dormidos) {
		p_antes = puntos;
		v_antes = vidas;
		d_antes = !!dormidos;
		pinta_cabecera();
	}
}

static void tamano(int an, int al)
{
	arriba = jg_completa ? 0 : ALTO_BARRA;
	cel = MIN((an - 16) / COLS, (al - arriba - cab_al - 12) / FILAS);
	cel = LIMITA(cel, 8, 64);
	ox = (an - COLS * cel) / 2;
	oy = arriba + cab_al + (al - arriba - cab_al - FILAS * cel) / 2;
	pinta_todo();
}

/* ---- los menús y los sucesos ---- */
static char op_juego[200];
static const char *menus_op[2];

static void pon_menus(void)
{
	snprintf(op_juego, sizeof op_juego, "Partida nueva   F2|Pausa   P|-|%sSonidos|-|Mejores puntuaciones...|-|Salir", snd_mudo ? "" : "» ");
	menus_op[0] = op_juego;
	menus_op[1] = "Cómo se juega...   F1|-|Acerca de Tragabolas...";
	jg_menus("Juego|Ayuda", menus_op);
}

static void ayuda(void)
{
	jg_aviso("Cómo se juega", "Lleve al Tragón con las flechas por el jardín y recoja todas las bolitas. Los erizos pinchan: el granate va a por usted, "
		 "el añil anda sin rumbo y el marrón le sale al paso. Con una campana, se duermen un rato y no pinchan.\n\n"
		 "Tres vidas. 10 puntos por bolita, 50 por campana y 100 por el jardín al dejarlo limpio. P: pausa. F2: otra partida.");
}

static void pausa(int si)
{
	if (si && estado == E_JUEGA)
		estado = E_PAUSA;
	else if (!si && estado == E_PAUSA)
		estado = E_JUEGA;
	jg_cambio = 1;
}

static void suceso(const struct suceso *s)
{
	int m, o, d = -1;

	switch (s->clase) {
	case EV_TECLA:
		switch (s->a) {
		case 0x158:
			serie("tragabolas: estado=%d puntos=%ld jardin=%d vidas=%d bolitas=%d tragon=%d,%d erizos=%d,%d %d,%d %d,%d dormidos=%d sonido=%d\n", estado, puntos, jardin % 3 + 1, vidas,
			      bolitas, tragon.x, tragon.y, erizos[0].x, erizos[0].y, erizos[1].x, erizos[1].y, erizos[2].x, erizos[2].y, dormidos, snd_hay);
			return;
		case T_DERECHA: d = 0; break;
		case T_IZQUIERDA: d = 1; break;
		case T_ABAJO: d = 2; break;
		case T_ARRIBA: d = 3; break;
		case 'p': case 'P': case ' ': pausa(estado == E_JUEGA); return;
		case T_F2: nueva_partida(); return;
		case 0x13B: ayuda(); return;
		case 10: case 13:
			if (estado == E_LISTO || estado == E_FIN)
				nueva_partida();
			return;
		}
		if (d >= 0) {
			if (estado == E_LISTO || estado == E_FIN)
				nueva_partida();
			pausa(0);
			tragon.nd = d;
			if (tragon.d < 0)
				tragon.d = d;
		}
		return;
	case EV_FOCO:
		if (!s->a)
			pausa(1);
		return;
	case EV_RATON_BAJA:
		jg_clic_barra((int)(int32_t)s->a, (int)(int32_t)s->b);
		return;
	case EV_MENU:
		o = jg_menu_elegido(s, &m);
		if (o < 0)
			return;
		if (m == 1) {
			if (o == 0)
				ayuda();
			else
				jg_aviso("Acerca de Tragabolas", "Tragabolas 1.0, para Hispa-Pentium (el de Hispa-386, con sus jardines).\n\nCopyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.");
			return;
		}
		switch (o) {
		case 0: nueva_partida(); break;
		case 1: pausa(estado == E_JUEGA); break;
		case 3: snd_mudo = !snd_mudo; rec_poner_entero("Sonidos", !snd_mudo); rec_guardar(); pon_menus(); break;
		case 5: rec_ver("Mejores puntuaciones", "Puntos", REC_N, "puntos", 0, record_pos); break;
		case 7: jg_salir = 1; break;
		}
		return;
	}
}

int principal(void)
{
	static const struct juego j = { pulso, pinta, suceso, tamano, 0, 60 };
	int an, al;
	u32 n, i;

	n = sc(SC_ARGUMENTOS, (u32)jg_args, sizeof jg_args - 1, 0);
	if (ES_ERROR(n))
		n = 0;
	jg_args[MIN(n, sizeof jg_args - 1)] = 0;
	for (i = 0; jg_args[i]; i++)
		if (jg_args[i] >= 'a' && jg_args[i] <= 'z')
			jg_args[i] = (char)(jg_args[i] - 32);
	jg_tamano_maximo(&an, &al);
	if (jg_abrir("Tragabolas", MIN(an, COLS * 24 + 24), MIN(al, FILAS * 24 + 30 + 40), 0))
		return 1;
	azar_semilla((u32)arg_entero("/S=", (int)(jg_tics() * 2654435761u ^ (u32)rdtsc())));
	jardin_ini = LIMITA(arg_entero("/N=", 1), 1, 3) - 1;
	jardin = jardin_ini;
	rec_abrir("TRAGABOL.REC");
	snd_mudo = !rec_entero("Sonidos", 1);
	snd_abrir();
	pon_menus();
	empieza_jardin();
	tamano(LZ.an, LZ.al);
	serie("tragabolas: ventana %d x %d (casillas de %d); récords en %s\n", LZ.an, LZ.al, cel, rec_ruta());
	return jg_correr(&j);
}
