/* pinfis.c - (JUEGOS) la física del pinball (pinfis.h).
 *
 * Cada paso (f_paso, 1/400 s en el juego) se parte en subpasos: los que
 * hagan falta para que ni la bola ni la punta de un flipper se muevan más
 * de un cuarto del radio de la bola en uno (hasta 64). En cada subpaso: los
 * flippers giran lo suyo; la bola cae con la gravedad de la mesa (más en la
 * rampa), frena un poco al rodar y avanza; luego, con cada cosa de su capa
 * que toque, sale fuera por la normal y rebota: la parte de la velocidad
 * hacia la cosa se da la vuelta, por su rebote (y, en los flippers, con lo
 * que se mueve la goma en ese punto: así golpean); las gomas de las hondas y
 * los bumpers dan además su patada. Como en un subpaso la bola avanza menos
 * de un cuarto de su radio, su centro no puede pasar al otro lado de una
 * pared (para eso tendría que estar antes a menos de su radio, y ya habría
 * chocado): no atraviesa nada a ninguna velocidad.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "juego.h"
#include "pinfis.h"

void f_punta(const struct f_flip *f, float *x, float *y)
{
	*x = f->px + f->largo * m_cos(f->ang);
	*y = f->py + f->largo * m_sen(f->ang);
}

void f_empuja(struct f_mesa *m, float vx, float vy)
{
	int i;

	for (i = 0; i < F_MAX_BOLAS; i++)
		if (m->bolas[i].viva) {
			m->bolas[i].vx += vx;
			m->bolas[i].vy += vy;
		}
}

/* rebota la bola b contra una superficie de normal (nx, ny) que se mueve a
 * (sx, sy): da la velocidad con que se acercaban (positiva) o 0 */
static float rebota(struct f_bola *b, float nx, float ny, float sx, float sy, float e, float roce)
{
	float rx = b->vx - sx, ry = b->vy - sy, vn = rx * nx + ry * ny, tx, ty, vt;

	if (vn >= 0)
		return 0;
	/* lo normal se da la vuelta (por el rebote); lo de lado frena un poco */
	rx -= (1.0f + e) * vn * nx;
	ry -= (1.0f + e) * vn * ny;
	tx = -ny;
	ty = nx;
	vt = rx * tx + ry * ty;
	rx -= roce * vt * tx;
	ry -= roce * vt * ty;
	b->vx = rx + sx;
	b->vy = ry + sy;
	return -vn;
}

static void choca_seg(struct f_mesa *m, int ib, struct f_seg *s)
{
	struct f_bola *b = &m->bolas[ib];
	float dx = s->bx - s->ax, dy = s->by - s->ay, l2 = dx * dx + dy * dy, t, qx, qy, ex, ey, d2, d, nx, ny, f;
	float R = m->radio + s->r;

	/* (lo primero, lo que más descarta: ¿está la bola cerca de su caja?) */
	if (b->x + m->radio < s->x0 || b->x - m->radio > s->x1 || b->y + m->radio < s->y0 || b->y - m->radio > s->y1)
		return;
	if (!s->activo || !(s->capas & b->capa))
		return;
	t = l2 > 0 ? ((b->x - s->ax) * dx + (b->y - s->ay) * dy) / l2 : 0;
	t = LIMITA(t, 0.0f, 1.0f);
	qx = s->ax + t * dx;
	qy = s->ay + t * dy;
	ex = b->x - qx;
	ey = b->y - qy;
	d2 = ex * ex + ey * ey;
	if (d2 >= R * R)
		return;
	if (s->tipo == F_PUERTA && (b->x - s->ax) * (-dy) + (b->y - s->ay) * dx < 0)
		return;		/* (la puerta de un sentido: desde atrás, se pasa) */
	d = m_raiz(d2);
	if (d < 1e-4f) {
		/* justo encima de la raya: la normal de la raya */
		float l = m_raiz(l2);

		nx = -dy / l;
		ny = dx / l;
	} else {
		nx = ex / d;
		ny = ey / d;
	}
	b->x = qx + nx * (R + 0.01f);
	b->y = qy + ny * (R + 0.01f);
	f = rebota(b, nx, ny, 0, 0, s->rebote, m->roce_choque);
	if (f > 0) {
		m->choques++;
		if (s->tipo == F_HONDA && f > 60) {
			b->vx += nx * m->patada_honda;
			b->vy += ny * m->patada_honda;
		}
		if (m->choque && (f > 40 || s->tipo == F_DIANA))
			m->choque(s->tipo, s->id, f, ib);
	}
}

static void choca_circ(struct f_mesa *m, int ib, struct f_circ *c)
{
	struct f_bola *b = &m->bolas[ib];
	float ex = b->x - c->x, ey = b->y - c->y, d2 = ex * ex + ey * ey, rr = m->radio + c->r, d, nx, ny, f;

	if (!c->activo || !(c->capas & b->capa) || d2 >= rr * rr)
		return;
	d = m_raiz(d2);
	if (d < 1e-4f) {
		nx = 0;
		ny = -1;
	} else {
		nx = ex / d;
		ny = ey / d;
	}
	b->x = c->x + nx * (rr + 0.01f);
	b->y = c->y + ny * (rr + 0.01f);
	f = rebota(b, nx, ny, 0, 0, c->rebote, m->roce_choque);
	if (c->tipo == F_BUMPER) {
		/* el bumper patea siempre que se le toca (aunque la bola llegue despacio) */
		b->vx += nx * m->patada_bumper;
		b->vy += ny * m->patada_bumper;
		if (m->choque)
			m->choque(c->tipo, c->id, f + m->patada_bumper, ib);
		m->choques++;
	} else if (f > 0) {
		m->choques++;
		if (m->choque && f > 40)
			m->choque(c->tipo, c->id, f, ib);
	}
}

static void choca_flip(struct f_mesa *m, int ib, int k)
{
	struct f_flip *fl = &m->flips[k];
	struct f_bola *b = &m->bolas[ib];
	float tx, ty, dx, dy, l2, t, qx, qy, ex, ey, d2, rr, d, nx, ny, cx, cy, sx, sy, f;

	if (!(b->capa & CAPA_MESA))
		return;
	f_punta(fl, &tx, &ty);
	dx = tx - fl->px;
	dy = ty - fl->py;
	l2 = dx * dx + dy * dy;
	t = ((b->x - fl->px) * dx + (b->y - fl->py) * dy) / l2;
	t = LIMITA(t, 0.0f, 1.0f);
	qx = fl->px + t * dx;
	qy = fl->py + t * dy;
	rr = fl->r0 + (fl->r1 - fl->r0) * t;
	ex = b->x - qx;
	ey = b->y - qy;
	d2 = ex * ex + ey * ey;
	if (d2 >= (m->radio + rr) * (m->radio + rr))
		return;
	d = m_raiz(d2);
	if (d < 1e-4f) {
		float l = m_raiz(l2);

		nx = dy / l;
		ny = -dx / l;
	} else {
		nx = ex / d;
		ny = ey / d;
	}
	b->x = qx + nx * (m->radio + rr + 0.01f);
	b->y = qy + ny * (m->radio + rr + 0.01f);
	/* lo que se mueve la goma donde toca: w x (punto - eje) */
	cx = qx + nx * rr - fl->px;
	cy = qy + ny * rr - fl->py;
	sx = -fl->w * cy;
	sy = fl->w * cx;
	f = rebota(b, nx, ny, sx, sy, 0.35f, 2 * m->roce_choque);
	if (f > 0) {
		m->choques++;
		if (m->choque && f > 120)
			m->choque(F_FLIPPER, k, f, ib);
	}
}

static void choca_bolas(struct f_mesa *m, int i, int j)
{
	struct f_bola *a = &m->bolas[i], *b = &m->bolas[j];
	float ex = b->x - a->x, ey = b->y - a->y, d2 = ex * ex + ey * ey, rr = 2 * m->radio, d, nx, ny, va, vb, p;

	if (a->capa != b->capa || d2 >= rr * rr)
		return;
	d = m_raiz(d2);
	if (d < 1e-4f) {
		nx = 1;
		ny = 0;
		d = 0;
	} else {
		nx = ex / d;
		ny = ey / d;
	}
	p = (rr - d) / 2 + 0.01f;
	a->x -= nx * p;
	a->y -= ny * p;
	b->x += nx * p;
	b->y += ny * p;
	va = a->vx * nx + a->vy * ny;
	vb = b->vx * nx + b->vy * ny;
	if (va - vb > 0) {
		/* (las dos pesan lo mismo: se cambian lo normal, con un poco de pérdida) */
		float q = (va - vb) * 0.95f;

		a->vx -= q * nx;
		a->vy -= q * ny;
		b->vx += q * nx;
		b->vy += q * ny;
		if (m->choque && va - vb > 60)
			m->choque(F_BOLA, j, va - vb, i);
	}
}

static void mira_sensores(struct f_mesa *m, int ib, float x0, float y0)
{
	struct f_bola *b = &m->bolas[ib];
	int i;

	for (i = 0; i < m->n_sensores; i++) {
		struct f_sensor *s = &m->sensores[i];
		float dx = s->bx - s->ax, dy = s->by - s->ay;
		float l0 = (x0 - s->ax) * (-dy) + (y0 - s->ay) * dx, l1 = (b->x - s->ax) * (-dy) + (b->y - s->ay) * dx, t, l2;

		if (!(s->capas & b->capa) || (l0 < 0) == (l1 < 0))
			continue;
		/* ¿por dentro de la raya? */
		l2 = dx * dx + dy * dy;
		t = ((b->x - s->ax) * dx + (b->y - s->ay) * dy) / l2;
		if (t < 0 || t > 1)
			continue;
		if (m->cruce)
			m->cruce(s->id, ib, l0 >= 0 ? 1 : -1);
	}
}

void f_paso(struct f_mesa *m, float dt)
{
	float vmax = 0, dts;
	int n, k, i, j;

	if (!m->preparada) {
		for (i = 0; i < m->n_segs; i++) {
			struct f_seg *s = &m->segs[i];

			s->x0 = MIN(s->ax, s->bx) - s->r;
			s->x1 = MAX(s->ax, s->bx) + s->r;
			s->y0 = MIN(s->ay, s->by) - s->r;
			s->y1 = MAX(s->ay, s->by) + s->r;
		}
		m->preparada = 1;
	}
	/* cuántos subpasos: lo más rápido que se mueve algo */
	for (i = 0; i < F_MAX_BOLAS; i++)
		if (m->bolas[i].viva) {
			float v = m_abs(m->bolas[i].vx) + m_abs(m->bolas[i].vy);

			if (v > vmax)
				vmax = v;
		}
	for (k = 0; k < m->n_flips; k++) {
		/* (un flipper que se mueve: lo que anda su punta, además) */
		struct f_flip *f = &m->flips[k];
		int sube = f->pulsado && f->activo;

		if (f->ang != (sube ? f->arriba : f->reposo))
			vmax += (sube ? f->vel_sube : f->vel_baja) * f->largo;
	}
	vmax += m->g * dt;
	n = (int)(vmax * dt / (m->radio * 0.25f)) + 1;
	if (n > 64)
		n = 64;
	dts = dt / (float)n;
	for (k = 0; k < n; k++) {
		/* los flippers: hacia arriba o hacia abajo, hasta su tope */
		for (j = 0; j < m->n_flips; j++) {
			struct f_flip *f = &m->flips[j];
			float meta = (f->pulsado && f->activo) ? f->arriba : f->reposo, v = (f->pulsado && f->activo) ? f->vel_sube : f->vel_baja;
			float paso = v * dts, a = f->ang;

			if (meta > a)
				a = a + paso > meta ? meta : a + paso;
			else
				a = a - paso < meta ? meta : a - paso;
			f->w = (a - f->ang) / dts;
			f->ang = a;
		}
		for (i = 0; i < F_MAX_BOLAS; i++) {
			struct f_bola *b = &m->bolas[i];
			float x0, y0, v2;

			if (!b->viva)
				continue;
			b->vy += (b->capa == CAPA_RAMPA ? m->g_rampa : m->g) * dts;
			b->vx -= b->vx * m->roce * dts;
			b->vy -= b->vy * m->roce * dts;
			v2 = b->vx * b->vx + b->vy * b->vy;
			if (v2 > m->tope * m->tope) {
				float v = m_raiz(v2);

				b->vx = b->vx / v * m->tope;
				b->vy = b->vy / v * m->tope;
			}
			x0 = b->x;
			y0 = b->y;
			b->x += b->vx * dts;
			b->y += b->vy * dts;
			/* dos vueltas: en un rincón, lo que empuja una cosa lo deshace la otra */
			for (j = 0; j < 2; j++) {
				int q;

				for (q = 0; q < m->n_segs; q++)
					choca_seg(m, i, &m->segs[q]);
				for (q = 0; q < m->n_circs; q++)
					choca_circ(m, i, &m->circs[q]);
				for (q = 0; q < m->n_flips; q++)
					choca_flip(m, i, q);
			}
			mira_sensores(m, i, x0, y0);
		}
		for (i = 0; i < F_MAX_BOLAS; i++)
			for (j = i + 1; j < F_MAX_BOLAS; j++)
				if (m->bolas[i].viva && m->bolas[j].viva)
					choca_bolas(m, i, j);
		m->subpasos++;
	}
	for (j = 0; j < m->n_flips; j++)
		if (m->flips[j].ang == ((m->flips[j].pulsado && m->flips[j].activo) ? m->flips[j].arriba : m->flips[j].reposo))
			m->flips[j].w = 0;
}
