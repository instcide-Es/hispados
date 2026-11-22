/* prueba_pinball.c - (JUEGOS) la física de Hispa-Pinball, en el anfitrión
 * (prueba/anfitrion.sh; la parte 39 de castel/pruebas.sh):
 *
 *  1. No atraviesa: miles de bolas por la mesa de verdad (pinmesa.c), desde
 *     sitios y en direcciones al azar, a cualquier velocidad (hasta 15.000
 *     unidades por segundo, sin el tope del juego): ninguna sale por las
 *     paredes de fuera, ni cruza la del carril del lanzador, ni se mete en
 *     una taza o un poste.
 *  2. Conserva lo razonable: en una caja sin roce y con rebote 1, la
 *     rapidez sigue igual (±2 %) tras 30 s y más de 120 choques; con la gravedad,
 *     la energía (la del movimiento más la de la altura) no se va más de un
 *     3 %; con rebote 0,5, cada choque pierde lo que tiene que perder.
 *  3. Ninguna se queda atascada: 2.000 bolas soltadas por la mesa (también
 *     encima del canal de la rampa), con los flippers abajo, acaban todas
 *     cayendo por abajo (o en el lanzador) antes de 15 s: no hay rincones ni
 *     repisas donde se queden quietas.
 *  4. El flipper golpea: una bola quieta en el flipper sale disparada hacia
 *     arriba al pulsarlo (a más de 1.000 por segundo); y una bola que llega
 *     rapidísima mientras el flipper sube no lo atraviesa.
 * Sale con 1 si algo falla. Lo que dice, una línea por prueba («OK» o «MAL»).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdio.h>
#include "pinmesa.h"

char jg_args[256];
static unsigned s_azar = 12345;
static float al_azar(float a, float b)
{
	s_azar = s_azar * 1103515245u + 12345u;
	return a + (b - a) * (float)((s_azar >> 8) & 0xFFFF) / 65535.0f;
}

static int fallos;
static void dice(int bien, const char *que)
{
	printf("%s %s\n", bien ? "OK " : "MAL", que);
	if (!bien)
		fallos++;
}

static float dist_seg(float px, float py, const struct f_seg *s)
{
	float dx = s->bx - s->ax, dy = s->by - s->ay, l2 = dx * dx + dy * dy, t = l2 > 0 ? ((px - s->ax) * dx + (py - s->ay) * dy) / l2 : 0, qx, qy;

	t = t < 0 ? 0 : t > 1 ? 1 : t;
	qx = s->ax + t * dx - px;
	qy = s->ay + t * dy - py;
	return m_raiz(qx * qx + qy * qy);
}

/* ¿se puede poner ahí una bola? (sin tocar nada de la mesa) */
static int libre(struct f_mesa *m, float x, float y)
{
	int i;

	if (x > 440 || x < 20 || y < 60)
		return 0;
	if (y < 250 && (x - 250) * (x - 250) + (y - 250) * (y - 250) > (238 - 15) * (238 - 15))
		return 0;
	if (x < 92 && y > 236 && y < 474)
		return 0;	/* (el canal de la rampa) */
	for (i = 0; i < m->n_segs; i++)
		if (dist_seg(x, y, &m->segs[i]) < m->radio + m->segs[i].r + 2)
			return 0;
	for (i = 0; i < m->n_circs; i++) {
		float dx = x - m->circs[i].x, dy = y - m->circs[i].y;

		if (dx * dx + dy * dy < (m->radio + m->circs[i].r + 2) * (m->radio + m->circs[i].r + 2))
			return 0;
	}
	for (i = 0; i < m->n_flips; i++) {
		float tx, ty;

		f_punta(&m->flips[i], &tx, &ty);
		if (y > 860 && x > 120 && x < 340)
			return 0;
	}
	return 1;
}

static int en_rampa_ahora;
static void cruce_prueba(int id, int b, int sentido)
{
	(void)b;
	(void)sentido;
	if (id == S_RAMPA_ENTRA)
		en_rampa_ahora = 1;
}

static void no_atraviesa(void)
{
	struct f_mesa m;
	int intento, malas = 0, i, pasos_tot = 0, choques = 0;
	float vmax_vista = 0;
	char t[200];

	mesa_construye(&m);
	m.tope = 20000;
	m.cruce = cruce_prueba;
	for (intento = 0; intento < 4000; intento++) {
		float x, y, a, v;
		int k, salio = 0;

		do {
			x = al_azar(20, 440);
			y = al_azar(60, 880);
		} while (!libre(&m, x, y));
		a = al_azar(0, 2 * M_PI_F);
		v = intento < 1000 ? al_azar(100, 2000) : intento < 3000 ? al_azar(2000, 8000) : al_azar(8000, 15000);
		memset(m.bolas, 0, sizeof m.bolas);
		m.bolas[0].viva = 1;
		m.bolas[0].capa = CAPA_MESA;
		m.bolas[0].x = x;
		m.bolas[0].y = y;
		m.bolas[0].vx = v * m_cos(a);
		m.bolas[0].vy = v * m_sen(a);
		m.flips[0].pulsado = m.flips[1].pulsado = intento % 3 == 0;
		en_rampa_ahora = 0;
		for (k = 0; k < 400 && !salio; k++) {
			struct f_bola *b = &m.bolas[0];
			float vb = m_raiz(b->vx * b->vx + b->vy * b->vy);

			if (k % 50 == 25)
				m.flips[0].pulsado = !m.flips[0].pulsado;
			f_paso(&m, 1.0f / 400);
			pasos_tot++;
			if (vb > vmax_vista)
				vmax_vista = vb;
			if (b->y > 1005) {
				salio = 1;	/* (se ha ido por abajo: la bola perdida, que vale) */
				break;
			}
			/* las paredes de fuera y la del carril */
			if (b->x < 12 + RADIO_BOLA - 1.0f || b->x > 488 - RADIO_BOLA + 1.0f ||
			    (b->y < 250 && (b->x - 250) * (b->x - 250) + (b->y - 250) * (b->y - 250) > (238 - RADIO_BOLA + 1) * (238 - RADIO_BOLA + 1)) ||
			    (b->x > 444 - RADIO_BOLA + 1.0f && b->y > 336)) {
				if (malas < 5)
					printf("    sale: intento %d paso %d: (%.1f, %.1f) a %.0f\n", intento, k, (double)b->x, (double)b->y, (double)v);
				malas++;
				break;
			}
			for (i = 0; i < m.n_circs; i++) {
				float dx = b->x - m.circs[i].x, dy = b->y - m.circs[i].y, rr = m.circs[i].r + RADIO_BOLA - 1.0f;

				if (dx * dx + dy * dy < rr * rr) {
					if (malas < 5)
						printf("    dentro de un poste: intento %d (%.1f, %.1f)\n", intento, (double)b->x, (double)b->y);
					malas++;
					salio = 1;
					break;
				}
			}
			/* (y de ninguna pared de su capa) */
			for (i = 0; i < m.n_segs && !salio; i++) {
				const struct f_seg *s = &m.segs[i];

				if (!(s->capas & b->capa) || !s->activo || s->tipo == F_PUERTA)
					continue;
				if (dist_seg(b->x, b->y, s) < RADIO_BOLA + s->r - 1.0f) {
					if (malas < 5)
						printf("    dentro de una pared: intento %d (%.1f, %.1f) seg %d\n", intento, (double)b->x, (double)b->y, i);
					malas++;
					salio = 1;
				}
			}
		}
		choques += (int)m.choques;
		m.choques = 0;
	}
	snprintf(t, sizeof t, "no atraviesa: 4000 bolas al azar (hasta 15000 u/s; la más rápida, %.0f), %d pasos, %ld subpasos: %d fuera de sitio",
		 (double)vmax_vista, pasos_tot, m.subpasos, malas);
	dice(malas == 0, t);
}

static struct f_seg caja_segs[4];
static void caja(struct f_mesa *m, float e)
{
	int i;
	static const float p[5][2] = { {0, 0}, {400, 0}, {400, 400}, {0, 400}, {0, 0} };

	memset(m, 0, sizeof *m);
	for (i = 0; i < 4; i++) {
		caja_segs[i].ax = p[i][0];
		caja_segs[i].ay = p[i][1];
		caja_segs[i].bx = p[i + 1][0];
		caja_segs[i].by = p[i + 1][1];
		caja_segs[i].rebote = e;
		caja_segs[i].capas = CAPA_TODAS;
		caja_segs[i].activo = 1;
	}
	m->segs = caja_segs;
	m->n_segs = 4;
	m->radio = RADIO_BOLA;
	m->tope = 1e9f;
	m->bolas[0].viva = 1;
	m->bolas[0].capa = CAPA_MESA;
}

static void conserva(void)
{
	struct f_mesa m;
	int k;
	float v0, v1, e0, e1, emax = 0, emin = 1e30f;
	char t[200];

	caja(&m, 1.0f);
	m.bolas[0].x = 123;
	m.bolas[0].y = 211;
	m.bolas[0].vx = 1200;
	m.bolas[0].vy = 870;
	v0 = m_raiz(1200.0f * 1200 + 870.0f * 870);
	for (k = 0; k < 12000; k++)
		f_paso(&m, 1.0f / 400);
	v1 = m_raiz(m.bolas[0].vx * m.bolas[0].vx + m.bolas[0].vy * m.bolas[0].vy);
	snprintf(t, sizeof t, "conserva: sin roce y rebote 1, tras 30 s y %ld choques, la rapidez es el %.2f %% de la de antes", m.choques, (double)(100 * v1 / v0));
	dice(v1 > 0.98f * v0 && v1 < 1.02f * v0 && m.choques > 120, t);
	/* con la gravedad: la energía (por unidad de masa) */
	caja(&m, 1.0f);
	m.g = 1150;
	m.bolas[0].x = 200;
	m.bolas[0].y = 60;
	m.bolas[0].vx = 300;
	e0 = 0.5f * 300 * 300 + m.g * (400 - 60);
	for (k = 0; k < 2000; k++) {
		f_paso(&m, 1.0f / 400);
		e1 = 0.5f * (m.bolas[0].vx * m.bolas[0].vx + m.bolas[0].vy * m.bolas[0].vy) + m.g * (400 - m.bolas[0].y);
		if (e1 > emax) emax = e1;
		if (e1 < emin) emin = e1;
	}
	snprintf(t, sizeof t, "conserva: con la gravedad, en 5 s botando, la energía va del %.1f al %.1f %% de la de antes", (double)(100 * emin / e0), (double)(100 * emax / e0));
	dice(emin > 0.97f * e0 && emax < 1.03f * e0, t);
	/* con rebote 0,5: de frente contra la pared, sale a la mitad */
	caja(&m, 0.5f);
	m.bolas[0].x = 200;
	m.bolas[0].y = 200;
	m.bolas[0].vx = 1000;
	for (k = 0; k < 120; k++)
		f_paso(&m, 1.0f / 400);
	snprintf(t, sizeof t, "conserva: con rebote 0,5, de frente contra la pared a 1000 sale a %.0f", (double)-m.bolas[0].vx);
	dice(m.bolas[0].vx < -470 && m.bolas[0].vx > -530, t);
}

static int golpes_flipper;
static float fuerza_flipper;
static void choque_prueba(int tipo, int id, float f, int b)
{
	(void)id;
	(void)b;
	if (tipo == F_FLIPPER) {
		golpes_flipper++;
		if (f > fuerza_flipper)
			fuerza_flipper = f;
	}
}

static void flipper(void)
{
	struct f_mesa m;
	int k, i, malas = 0;
	float vmax = 0, vy_fin;
	char t[200];

	mesa_construye(&m);
	m.choque = choque_prueba;
	/* una bola quieta en el flipper izquierdo (abajo): se deja asentar y se pulsa */
	memset(m.bolas, 0, sizeof m.bolas);
	m.bolas[0].viva = 1;
	m.bolas[0].capa = CAPA_MESA;
	m.bolas[0].x = 180;
	m.bolas[0].y = 900;
	for (k = 0; k < 8; k++)
		f_paso(&m, 1.0f / 400);
	m.flips[0].pulsado = 1;
	for (k = 0; k < 80; k++) {
		float v;

		f_paso(&m, 1.0f / 400);
		v = m_raiz(m.bolas[0].vx * m.bolas[0].vx + m.bolas[0].vy * m.bolas[0].vy);
		if (v > vmax)
			vmax = v;
	}
	vy_fin = m.bolas[0].vy;
	snprintf(t, sizeof t, "flipper: la bola quieta en el flipper izquierdo sale a %.0f u/s hacia arriba (golpe de %.0f)", (double)vmax, (double)fuerza_flipper);
	dice(vmax > 1000 && golpes_flipper > 0 && (vy_fin < 0 || m.bolas[0].y < 700), t);
	/* el derecho, igual */
	mesa_construye(&m);
	memset(m.bolas, 0, sizeof m.bolas);
	m.bolas[0].viva = 1;
	m.bolas[0].capa = CAPA_MESA;
	m.bolas[0].x = 280;
	m.bolas[0].y = 900;
	for (k = 0; k < 8; k++)
		f_paso(&m, 1.0f / 400);
	m.flips[1].pulsado = 1;
	vmax = 0;
	for (k = 0; k < 80; k++) {
		float v;

		f_paso(&m, 1.0f / 400);
		v = m_raiz(m.bolas[0].vx * m.bolas[0].vx + m.bolas[0].vy * m.bolas[0].vy);
		if (v > vmax)
			vmax = v;
	}
	snprintf(t, sizeof t, "flipper: y en el derecho, a %.0f u/s", (double)vmax);
	dice(vmax > 1000, t);
	/* rapidísima contra el flipper que sube: no lo atraviesa */
	for (i = 0; i < 300; i++) {
		float x = al_azar(160, 205), v = al_azar(1500, 4200), tx, ty, ay;

		mesa_construye(&m);
		memset(m.bolas, 0, sizeof m.bolas);
		m.bolas[0].viva = 1;
		m.bolas[0].capa = CAPA_MESA;
		m.bolas[0].x = x;
		m.bolas[0].y = 820;
		m.bolas[0].vy = v;
		m.flips[0].pulsado = 1;
		m.flips[0].ang = m.flips[0].reposo - al_azar(0, 0.6f);
		for (k = 0; k < 40; k++) {
			f_paso(&m, 1.0f / 400);
			/* ¿debajo del flipper, entre el eje y la punta? */
			f_punta(&m.flips[0], &tx, &ty);
			if (m.bolas[0].x > m.flips[0].px && m.bolas[0].x < tx) {
				ay = m.flips[0].py + (ty - m.flips[0].py) * (m.bolas[0].x - m.flips[0].px) / (tx - m.flips[0].px);
				if (m.bolas[0].y > ay + 2) {
					malas++;
					break;
				}
			}
		}
	}
	snprintf(t, sizeof t, "flipper: 300 bolas a 1500-4200 u/s contra el flipper que sube: %d lo atraviesan", malas);
	dice(malas == 0, t);
}

static void atascadas(void)
{
	struct f_mesa m;
	int intento, malas = 0, k;
	char t[200];

	mesa_construye(&m);
	for (intento = 0; intento < 2000; intento++) {
		float x, y;

		do {
			x = al_azar(20, 440);
			y = intento % 10 == 0 ? al_azar(150, 236) : al_azar(60, 880);
		} while (!libre(&m, x, y) && !(intento % 10 == 0 && x < 100 && x > 25));
		memset(m.bolas, 0, sizeof m.bolas);
		m.bolas[0].viva = 1;
		m.bolas[0].capa = CAPA_MESA;
		m.bolas[0].x = x;
		m.bolas[0].y = y;
		m.bolas[0].vx = al_azar(-60, 60);
		for (k = 0; k < 6000 && m.bolas[0].y < 1005; k++)
			f_paso(&m, 1.0f / 400);
		if (m.bolas[0].y < 1005 && !(m.bolas[0].x > 452 && m.bolas[0].y > 930)) {
			if (malas < 5)
				printf("    se queda en (%.1f, %.1f), soltada en (%.1f, %.1f)\n", (double)m.bolas[0].x, (double)m.bolas[0].y, (double)x, (double)y);
			malas++;
		}
	}
	snprintf(t, sizeof t, "atascadas: 2000 bolas soltadas por la mesa (una de cada diez, encima del canal de la rampa): %d se quedan quietas", malas);
	dice(malas == 0, t);
}

int main(void)
{
	no_atraviesa();
	atascadas();
	conserva();
	flipper();
	printf("%s\n", fallos ? "== hay fallos ==" : "== todo bien ==");
	return fallos != 0;
}
