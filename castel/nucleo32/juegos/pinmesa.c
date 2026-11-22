/* pinmesa.c - (JUEGOS) la mesa de Hispa-Pinball, «La Feria» (pinmesa.h).
 *
 * Diseño propio, de una feria de pueblo de noche: arriba, las tres calles
 * F, E y R y las tres «tazas» (los bumpers, como las de la atracción); a la
 * izquierda, la montaña rusa (una rampa que sube y suelta la bola arriba);
 * a la derecha, el tiro al blanco (tres dianas que se tumban); en medio, la
 * tómbola (un agujero que se traga la bola, da un premio y la devuelve) y la
 * noria, con las cinco misiones en sus barquillas; abajo, las hondas, los
 * pasillos I y A, los multiplicadores y los dos flippers. Nada es de otra
 * mesa: ni la forma, ni el tema, ni los dibujos.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "pinmesa.h"

#define MAX_SEGS 200
#define MAX_CIRCS 24
#define MAX_SENSORES 24
struct f_seg mesa_segs[MAX_SEGS];
int mesa_n_segs;
struct f_circ mesa_circs[MAX_CIRCS];
int mesa_n_circs;
struct f_sensor mesa_sensores[MAX_SENSORES];
int mesa_n_sensores;
struct pieza piezas[MAX_PIEZAS];
int n_piezas;

const char *const nombre_luz[N_LUCES] = {
	"F", "E", "R", "I", "A", "rampa 1", "rampa 2", "rampa 3", "diana 1", "diana 2", "diana 3",
	"diana 1 (de pie)", "diana 2 (de pie)", "diana 3 (de pie)", "tómbola", "bola extra",
	"taza 1", "taza 2", "taza 3", "x2", "x3", "x4", "x5",
	"misión 1", "misión 2", "misión 3", "misión 4", "misión 5", "gran feria", "otra vez" };

/* los colores de la mesa */
#define C_MADERA 0x6B2E1E
#define C_ORO 0xD9A441
#define C_METAL 0xBCC4CE
#define C_METAL_ARRIBA 0xEEF1F5
#define C_GOMA 0x2B2B2E
#define C_BLANCO 0xF4F2EA

float altura_rampa(float y)
{
	float t = (RAMPA_Y0 - y) / (RAMPA_Y0 - RAMPA_Y1);

	return RAMPA_ALTO * LIMITA(t, 0.0f, 1.0f);
}

/* ------------------------------------------------------------ lo que choca */
static void seg(float ax, float ay, float bx, float by, float r, float e, int capas, int tipo, int id)
{
	struct f_seg *s;

	if (mesa_n_segs >= MAX_SEGS)
		return;
	s = &mesa_segs[mesa_n_segs++];
	s->ax = ax; s->ay = ay; s->bx = bx; s->by = by;
	s->r = r;
	s->rebote = e;
	s->capas = (unsigned char)capas;
	s->tipo = (unsigned char)tipo;
	s->activo = 1;
	s->id = (short)id;
}

static void circ(float x, float y, float r, float e, int capas, int tipo, int id)
{
	struct f_circ *c;

	if (mesa_n_circs >= MAX_CIRCS)
		return;
	c = &mesa_circs[mesa_n_circs++];
	c->x = x; c->y = y; c->r = r;
	c->rebote = e;
	c->capas = (unsigned char)capas;
	c->tipo = (unsigned char)tipo;
	c->activo = 1;
	c->id = (short)id;
}

static void sensor(float ax, float ay, float bx, float by, int capas, int id)
{
	struct f_sensor *s;

	if (mesa_n_sensores >= MAX_SENSORES)
		return;
	s = &mesa_sensores[mesa_n_sensores++];
	s->ax = ax; s->ay = ay; s->bx = bx; s->by = by;
	s->capas = (unsigned char)capas;
	s->id = (short)id;
}

/* ------------------------------------------------------------ las piezas */
static struct pieza *pieza(const float *p, int n, float z0, float z1, u32 lado, u32 arriba)
{
	struct pieza *q;
	int i;
	float cy = 0;

	if (n_piezas >= MAX_PIEZAS || n > PZ_MAX_PUNTOS)
		return 0;
	q = &piezas[n_piezas++];
	memset(q, 0, sizeof *q);
	memcpy(q->p, p, (size_t)n * 2 * sizeof(float));
	q->n = n;
	q->z0 = z0;
	q->z1 = z1;
	q->lado = lado;
	q->arriba = arriba;
	q->luz = -1;
	for (i = 0; i < n; i++)
		cy += p[2 * i + 1];
	q->orden = cy / (float)n;
	return q;
}

/* un trozo de pared: un rectángulo de grosor 2r a lo largo de a-b */
static void caja(float ax, float ay, float bx, float by, float r, float alto, u32 lado, u32 arriba)
{
	float dx = bx - ax, dy = by - ay, l = m_raiz(dx * dx + dy * dy), nx, ny, p[8];

	if (l < 0.01f)
		return;
	nx = -dy / l * r;
	ny = dx / l * r;
	p[0] = ax + nx; p[1] = ay + ny;
	p[2] = bx + nx; p[3] = by + ny;
	p[4] = bx - nx; p[5] = by - ny;
	p[6] = ax - nx; p[7] = ay - ny;
	pieza(p, 4, 0, alto, lado, arriba);
}

static struct pieza *cilindro(float x, float y, float r, float z0, float z1, u32 lado, u32 arriba)
{
	float p[2 * 16];
	int i;

	for (i = 0; i < 16; i++) {
		p[2 * i] = x + r * m_cos(i * M_PI_F / 8);
		p[2 * i + 1] = y + r * m_sen(i * M_PI_F / 8);
	}
	return pieza(p, 16, z0, z1, lado, arriba);
}

/* una pared que choca y se ve: en trozos de 40 como mucho (para el orden) */
static void pared(float ax, float ay, float bx, float by, float r, float alto, u32 lado, u32 arriba, float e, int capas, int tipo, int id)
{
	float dx = bx - ax, dy = by - ay, l = m_raiz(dx * dx + dy * dy);
	int n = (int)(l / 40.0f) + 1, i;

	seg(ax, ay, bx, by, r, e, capas, tipo, id);
	if (alto <= 0)
		return;
	for (i = 0; i < n; i++)
		caja(ax + dx * i / n, ay + dy * i / n, ax + dx * (i + 1) / n, ay + dy * (i + 1) / n, r, alto, lado, arriba);
	/* (las juntas, redondas) */
	cilindro(bx, by, r, 0, alto, lado, arriba);
}

void mesa_construye(struct f_mesa *m)
{
	int i;
	static const float postes[4] = { 167, 211, 255, 299 };

	mesa_n_segs = mesa_n_circs = mesa_n_sensores = 0;
	n_piezas = 0;
	/* el arco de arriba: centro (250, 250), la cara de dentro a 238 */
	for (i = 0; i < 24; i++) {
		float a0 = M_PI_F + M_PI_F * i / 24, a1 = M_PI_F + M_PI_F * (i + 1) / 24;

		pared(250 + 244 * m_cos(a0), 250 + 244 * m_sen(a0), 250 + 244 * m_cos(a1), 250 + 244 * m_sen(a1), 6, 40, C_MADERA, C_ORO, 0.5f, CAPA_TODAS, F_PARED, 0);
	}
	/* los lados, el carril del lanzador y su puerta (de un sentido) */
	pared(6, 250, 6, 1000, 6, 40, C_MADERA, C_ORO, 0.5f, CAPA_TODAS, F_PARED, 0);
	pared(494, 250, 494, 1000, 6, 40, C_MADERA, C_ORO, 0.5f, CAPA_TODAS, F_PARED, 0);
	pared(448, 1000, 448, 330, 4, 28, C_METAL, C_METAL_ARRIBA, 0.45f, CAPA_TODAS, F_PARED, 0);
	seg(488, 300, 448, 326, 1.5f, 0.3f, CAPA_TODAS, F_PUERTA, ID_PUERTA);
	caja(488, 300, 448, 326, 1.5f, 10, 0x8A929C, 0xD8DDE4);
	seg(452, EMBOLO_Y + 0.5f, 488, EMBOLO_Y + 0.5f, 0.5f, 0.1f, CAPA_TODAS, F_PARED, 0);
	/* las calles de arriba (F E R) */
	for (i = 0; i < 4; i++) {
		seg(postes[i], 92, postes[i], 140, 5, 0.6f, CAPA_MESA, F_POSTE, 0);
		caja(postes[i], 92, postes[i], 140, 5, 24, C_METAL, C_METAL_ARRIBA);
		cilindro(postes[i], 92, 5, 0, 24, C_METAL, C_METAL_ARRIBA);
		cilindro(postes[i], 140, 5, 0, 24, C_METAL, C_METAL_ARRIBA);
	}
	for (i = 0; i < 3; i++)
		sensor(postes[i] + 6, 116, postes[i + 1] - 6, 116, CAPA_MESA, S_CARRIL0 + i);
	/* las tazas (los bumpers) */
	{
		static const float bx[3] = { 182, 280, 231 }, by[3] = { 300, 300, 382 };

		for (i = 0; i < 3; i++) {
			struct pieza *p;

			circ(bx[i], by[i], 26, 0.7f, CAPA_MESA, F_BUMPER, ID_BUMPER0 + i);
			cilindro(bx[i], by[i], 29, 0, 6, 0xE8E8E8, 0xFFFFFF);			/* el aro de abajo */
			p = cilindro(bx[i], by[i], 26, 6, 26, 0xB0302A, 0xC0392B);		/* la taza */
			p = cilindro(bx[i], by[i], 21, 26, 30, 0x7A2A20, 0x8A3A2E);		/* la tapa (la luz) */
			if (p) {
				p->luz = (short)(LUZ_BUMPER1 + i);
				p->arriba_luz = 0xFFD04A;
				p->brillo = 1;
				p->orden = by[i] + 0.5f;
			}
		}
	}
	/* la montaña rusa: el canal (choca en las dos capas), su tapa de arriba (solo en la mesa) y sus sensores */
	pared(88, 240, 88, RAMPA_Y0, 4, 30, C_METAL, C_METAL_ARRIBA, 0.4f, CAPA_TODAS, F_PARED, 0);
	/* (inclinada, y pasando por encima de la punta de la pared del canal: una bola que caiga encima rueda a la mesa, sin quedarse en un rincón) */
	seg(12, 222, 97, 236, 4, 0.4f, CAPA_MESA, F_PARED, 0);
	sensor(12, RAMPA_Y0 - 4, 84, RAMPA_Y0 - 4, CAPA_TODAS, S_RAMPA_ENTRA);
	sensor(12, RAMPA_Y1 + 12, 84, RAMPA_Y1 + 12, CAPA_RAMPA, S_RAMPA_SALE);
	/* la rampa misma (la pintura: sube del suelo a RAMPA_ALTO, con sus barandillas) */
	for (i = 0; i < 10; i++) {
		float ya = RAMPA_Y0 - (RAMPA_Y0 - RAMPA_Y1) * i / 10, yb = RAMPA_Y0 - (RAMPA_Y0 - RAMPA_Y1) * (i + 1) / 10;
		float za = altura_rampa(ya), zb = altura_rampa(yb);
		float q[8] = { RAMPA_X0, yb, RAMPA_X1, yb, RAMPA_X1, ya, RAMPA_X0, ya };
		float r1[8] = { RAMPA_X0 - 2, yb, RAMPA_X0 + 3, yb, RAMPA_X0 + 3, ya, RAMPA_X0 - 2, ya };
		float r2[8] = { RAMPA_X1 - 3, yb, RAMPA_X1 + 2, yb, RAMPA_X1 + 2, ya, RAMPA_X1 - 3, ya };
		struct pieza *p;

		p = pieza(q, 4, 0, za, i % 2 ? 0x2C5E9A : 0x30669E, i % 2 ? 0x5E9AD8 : 0x66A2DE);
		if (p) {
			p->inclinada = 1;
			p->zt[0] = p->zt[1] = zb;
			p->zt[2] = p->zt[3] = za;
			p->brillo = 1;
			p->orden = ya - 0.2f;
		}
		p = pieza(r1, 4, 0, za, 0x9AA4B0, 0xE8ECF2);
		if (p) {
			p->inclinada = 1;
			p->zt[0] = p->zt[1] = zb + 12;
			p->zt[2] = p->zt[3] = za + 12;
			p->orden = ya - 0.1f;
		}
		p = pieza(r2, 4, 0, za, 0x9AA4B0, 0xE8ECF2);
		if (p) {
			p->inclinada = 1;
			p->zt[0] = p->zt[1] = zb + 12;
			p->zt[2] = p->zt[3] = za + 12;
			p->orden = ya;
		}
	}
	{
		/* arriba, la plataforma de donde cae la bola */
		float q[8] = { RAMPA_X0, 236, RAMPA_X1 + 14, 236, RAMPA_X1 + 14, RAMPA_Y1, RAMPA_X0, RAMPA_Y1 };
		struct pieza *p = pieza(q, 4, 0, RAMPA_ALTO, 0x2C5E9A, 0x7AB4EA);

		if (p) {
			p->brillo = 1;
			p->orden = RAMPA_Y1 - 1;
		}
	}
	/* el tiro al blanco: tres dianas que se tumban */
	for (i = 0; i < 3; i++) {
		float y = 470.0f + 40.0f * i;
		struct pieza *p;
		float q[8] = { 432, y - 15, 440, y - 15, 440, y + 15, 432, y + 15 };

		seg(436, y - 15, 436, y + 15, 4, 0.4f, CAPA_MESA, F_DIANA, ID_DIANA0 + i);
		p = pieza(q, 4, 0, 26, 0xC8641E, 0xF08A24);
		if (p) {
			p->luz = (short)(LUZ_DIANA1 + i);
			p->solo_luz = 1;
			p->arriba_luz = 0xF8A040;
		}
	}
	/* las hondas (los triángulos de goma de encima de los flippers) */
	{
		float hi[6] = { 84, 722, 84, 820, 132, 852 }, hd[6] = { 376, 722, 376, 820, 328, 852 };
		struct pieza *p;

		seg(hi[0], hi[1], hi[2], hi[3], 3, 0.5f, CAPA_MESA, F_PARED, 0);
		seg(hi[2], hi[3], hi[4], hi[5], 3, 0.5f, CAPA_MESA, F_PARED, 0);
		seg(hi[0], hi[1], hi[4], hi[5], 4, 0.6f, CAPA_MESA, F_HONDA, ID_HONDA_I);
		seg(hd[0], hd[1], hd[2], hd[3], 3, 0.5f, CAPA_MESA, F_PARED, 0);
		seg(hd[2], hd[3], hd[4], hd[5], 3, 0.5f, CAPA_MESA, F_PARED, 0);
		seg(hd[0], hd[1], hd[4], hd[5], 4, 0.6f, CAPA_MESA, F_HONDA, ID_HONDA_D);
		p = pieza(hi, 3, 0, 22, 0xD8D6CC, C_BLANCO);
		if (p)
			p->brillo = 1;
		p = pieza(hd, 3, 0, 22, 0xD8D6CC, C_BLANCO);
		if (p)
			p->brillo = 1;
		caja(hi[0] + 3, hi[1] + 2, hi[4] + 3, hi[5] - 1, 3, 20, 0x8E2420, 0xC0392B);	/* la goma */
		caja(hd[0] - 3, hd[1] + 2, hd[4] - 3, hd[5] - 1, 3, 20, 0x8E2420, 0xC0392B);
	}
	/* los pasillos: las guías, sus postes y los sensores */
	pared(46, 770, 46, 840, 4, 26, C_METAL, C_METAL_ARRIBA, 0.45f, CAPA_MESA, F_PARED, 0);
	/* (las guías acaban por encima del eje del flipper —que está un poco más abajo—: la bola cae a él, sin quedarse en un rincón) */
	pared(46, 840, 140, 897, 4, 26, C_METAL, C_METAL_ARRIBA, 0.45f, CAPA_MESA, F_PARED, 0);
	pared(412, 770, 412, 840, 4, 26, C_METAL, C_METAL_ARRIBA, 0.45f, CAPA_MESA, F_PARED, 0);
	pared(412, 840, 320, 897, 4, 26, C_METAL, C_METAL_ARRIBA, 0.45f, CAPA_MESA, F_PARED, 0);
	circ(46, 766, 6, 0.7f, CAPA_MESA, F_POSTE, 0);
	circ(412, 766, 6, 0.7f, CAPA_MESA, F_POSTE, 0);
	cilindro(46, 766, 7, 0, 26, C_GOMA, 0xF4F4F4);
	cilindro(412, 766, 7, 0, 26, C_GOMA, 0xF4F4F4);
	sensor(50, 790, 81, 790, CAPA_MESA, S_ENTRA_I);
	sensor(379, 790, 408, 790, CAPA_MESA, S_ENTRA_D);
	sensor(12, 860, 42, 860, CAPA_MESA, S_FUERA_I);
	sensor(416, 860, 444, 860, CAPA_MESA, S_FUERA_D);
	sensor(452, 420, 488, 420, CAPA_TODAS, S_LANZADOR);
	/* los ejes de los flippers (metal, que no choca: lo hace el flipper) */
	cilindro(150, FLIPPER_Y, 6, 0, 16, 0x8A929C, 0xD8DDE4);
	cilindro(310, FLIPPER_Y, 6, 0, 16, 0x8A929C, 0xD8DDE4);

	/* la física */
	memset(m, 0, sizeof *m);
	m->segs = mesa_segs;
	m->n_segs = mesa_n_segs;
	m->circs = mesa_circs;
	m->n_circs = mesa_n_circs;
	m->sensores = mesa_sensores;
	m->n_sensores = mesa_n_sensores;
	m->radio = RADIO_BOLA;
	m->g = 1150;
	m->g_rampa = 2100;
	m->roce = 0.12f;
	m->roce_choque = 0.02f;
	m->tope = 4200;
	m->patada_bumper = 300;
	m->patada_honda = 330;
	m->n_flips = 2;
	for (i = 0; i < 2; i++) {
		struct f_flip *f = &m->flips[i];

		f->px = i ? 310 : 150;
		f->py = FLIPPER_Y;
		f->largo = 66;
		f->r0 = 11;
		f->r1 = 5.5f;
		f->reposo = i ? M_PI_F - 0.50f : 0.50f;
		f->arriba = i ? M_PI_F + 0.40f : -0.40f;
		f->ang = f->reposo;
		f->vel_sube = 26;
		f->vel_baja = 14;
		f->activo = 1;
	}
	/* (las piezas, de atrás adelante: un orden por inserción, pocas) */
	for (i = 1; i < n_piezas; i++) {
		struct pieza t = piezas[i];
		int k = i - 1;

		while (k >= 0 && piezas[k].orden > t.orden) {
			piezas[k + 1] = piezas[k];
			k--;
		}
		piezas[k + 1] = t;
	}
}

/* ------------------------------------------------------------ el suelo */
static lienzo *T_OFF, *T_ON;
static u8 *T_ID;
static float KT;
#define TX(v) ((v) * KT)

/* el mapa de las luces: un círculo (sin suavizar) con el número de la luz + 1 */
static void id_circulo(float x, float y, float r, int id)
{
	int x0 = (int)TX(x - r), x1 = (int)TX(x + r) + 1, y0 = (int)TX(y - r), y1 = (int)TX(y + r) + 1, i, j;
	float rr = TX(r) * TX(r);

	for (j = MAX(0, y0); j <= MIN(T_OFF->al - 1, y1); j++)
		for (i = MAX(0, x0); i <= MIN(T_OFF->an - 1, x1); i++) {
			float dx = i + 0.5f - TX(x), dy = j + 0.5f - TX(y);

			if (dx * dx + dy * dy <= rr)
				T_ID[(size_t)j * T_OFF->an + i] = (u8)(id + 1);
		}
}

/* en los dos */
static void ambos_circulo(float x, float y, float r, u32 c) { lz_circulo_suave(T_OFF, TX(x), TX(y), TX(r), c); lz_circulo_suave(T_ON, TX(x), TX(y), TX(r), c); }
static void ambos_poligono(const float *p, int n, u32 c, int alfa)
{
	float q[2 * 40];
	int i;

	for (i = 0; i < 2 * n; i++)
		q[i] = TX(p[i]);
	lz_poligono_mezcla(T_OFF, q, n, c, alfa);
	lz_poligono_mezcla(T_ON, q, n, c, alfa);
}
static void ambos_texto(float x, float y, const char *t, u32 c, float alto)
{
	int e = MAX(1, (int)(TX(alto) / 16.0f + 0.5f));
	int a = ancho_texto(t, e);

	lz_texto_sombra(T_OFF, (int)TX(x) - a / 2, (int)TX(y), t, c, 0x000000, e);
	lz_texto_sombra(T_ON, (int)TX(x) - a / 2, (int)TX(y), t, c, 0x000000, e);
}

/* el resplandor de una luz encendida */
static void halo(float x, float y, float r, u32 c)
{
	int k;

	for (k = 8; k >= 1; k--) {
		float rr = TX(r * (1.0f + k * 0.11f));
		int x0 = (int)(TX(x) - rr), x1 = (int)(TX(x) + rr) + 1, y0 = (int)(TX(y) - rr), y1 = (int)(TX(y) + rr) + 1, i, j;

		for (j = MAX(0, y0); j <= MIN(T_ON->al - 1, y1); j++)
			for (i = MAX(0, x0); i <= MIN(T_ON->an - 1, x1); i++) {
				float dx = i + 0.5f - TX(x), dy = j + 0.5f - TX(y);

				if (dx * dx + dy * dy <= rr * rr) {
					u32 *p = T_ON->p + (size_t)j * T_ON->paso + i;

					*p = mezcla(*p, c, 14);
				}
			}
	}
}

enum { LF_CIRCULO, LF_FLECHA, LF_RECT, LF_ESTRELLA, LF_RULETA };

/* una luz pintada en el suelo: apagada (oscura, con su forma y su letra) y encendida */
static void luz(int id, int forma, float x, float y, float t, u32 color, const char *texto)
{
	u32 apagada = mezcla(color, 0x0A0A14, 165), encendida = color;
	float q[2 * 12];
	int i, k;

	for (k = 0; k < 2; k++) {
		lienzo *l = k ? T_ON : T_OFF;
		u32 c = k ? encendida : apagada;

		if (forma == LF_CIRCULO) {
			lz_circulo_suave(l, TX(x), TX(y), TX(t + 1.5f), 0x101018);
			lz_circulo_suave(l, TX(x), TX(y), TX(t), c);
			if (k)
				lz_circulo_suave(l, TX(x - t * 0.25f), TX(y - t * 0.3f), TX(t * 0.45f), aclara(c, 130));
		} else if (forma == LF_FLECHA) {
			float f[14] = { x, y - t, x + t, y, x + t * 0.45f, y, x + t * 0.45f, y + t, x - t * 0.45f, y + t, x - t * 0.45f, y, x - t, y };

			for (i = 0; i < 14; i++)
				q[i] = TX(f[i]);
			lz_poligono(l, q, 7, c);
		} else if (forma == LF_RECT) {
			lz_redondo(l, (int)TX(x - t * 1.6f), (int)TX(y - t * 0.8f), (int)TX(t * 3.2f), (int)TX(t * 1.6f), (int)TX(t * 0.5f), 0x101018);
			lz_redondo(l, (int)TX(x - t * 1.6f) + 1, (int)TX(y - t * 0.8f) + 1, (int)TX(t * 3.2f) - 2, (int)TX(t * 1.6f) - 2, (int)TX(t * 0.5f), c);
		} else if (forma == LF_ESTRELLA) {
			for (i = 0; i < 10; i++) {
				float r = (i % 2 ? 0.45f : 1.0f) * t, a = -M_PI_F / 2 + i * M_PI_F / 5;

				q[2 * i] = TX(x + r * m_cos(a));
				q[2 * i + 1] = TX(y + r * m_sen(a));
			}
			lz_poligono(l, q, 10, c);
		} else if (forma == LF_RULETA) {
			/* la rueda de la tómbola: ocho quesitos, dos colores; el agujero lo pone otro */
			for (i = 0; i < 8; i++) {
				float a0 = i * M_PI_F / 4, a1 = (i + 1) * M_PI_F / 4;
				float w[8] = { TX(x), TX(y), TX(x + t * m_cos(a0)), TX(y + t * m_sen(a0)),
					       TX(x + t * m_cos((a0 + a1) / 2) * 1.04f), TX(y + t * m_sen((a0 + a1) / 2) * 1.04f), TX(x + t * m_cos(a1)), TX(y + t * m_sen(a1)) };

				lz_poligono(l, w, 4, i % 2 ? c : (k ? 0xFFE066 : mezcla(0xFFE066, 0x0A0A14, 165)));
			}
		}
		if (texto && *texto) {
			int e = MAX(1, (int)(TX(t) / 11.0f));
			int a = ancho_texto(texto, e);

			lz_negrita(l, (int)TX(x) - a / 2, (int)TX(y) - 8 * e, texto, k ? 0x1A1208 : mezcla(0xFFFFFF, apagada, 120), e);
		}
	}
	halo(x, y, t * 1.15f, color);
	id_circulo(x, y, t * 2.1f, id);
}

void mesa_suelo(lienzo *off, lienzo *on, u8 *ids, float kt)
{
	int i, j;

	T_OFF = off;
	T_ON = on;
	T_ID = ids;
	KT = kt;
	memset(ids, 0, (size_t)off->an * off->al);
	/* el cielo de la feria, de noche: de violeta arriba a azul abajo */
	lz_degradado(off, 0, 0, off->an, off->al / 2, 0x160F38, 0x2A1C58);
	lz_degradado(off, 0, off->al / 2, off->an, off->al - off->al / 2, 0x2A1C58, 0x14305E);
	/* estrellas */
	{
		u32 s = 12345;

		for (i = 0; i < 90; i++) {
			float x, y;

			s = s * 1103515245u + 12345u;
			x = 20 + (float)((s >> 8) % 420);
			s = s * 1103515245u + 12345u;
			y = 20 + (float)((s >> 8) % 420);
			lz_circulo_suave(off, TX(x), TX(y), TX(0.8f + (i % 3) * 0.5f), mezcla(0xFFFFFF, 0x2A1C58, 60 + (i % 4) * 30));
		}
	}
	/* la carpa: rayas rojas y blancas que salen de arriba, detrás de las tazas */
	for (i = 0; i < 12; i++) {
		float a0 = 0.45f + i * (M_PI_F - 0.9f) / 12, a1 = 0.45f + (i + 1) * (M_PI_F - 0.9f) / 12;
		float p[8] = { TX(231), TX(170), TX(231 + 290 * m_cos(a0)), TX(170 + 290 * m_sen(a0)),
			       TX(231 + 290 * m_cos(a1)), TX(170 + 290 * m_sen(a1)), TX(231), TX(170) };

		lz_poligono_mezcla(off, p, 3, i % 2 ? 0xF2E6D0 : 0xB8322A, 70);
	}
	/* la orla de la carpa: banderines */
	for (i = 0; i < 14; i++) {
		float x0 = 40 + i * 27.0f, y0 = 205 + 12 * m_sen(i * 0.45f), p[6] = { x0, y0, x0 + 24, y0 + 1, x0 + 12, y0 + 18 };
		static const u32 band[5] = { 0xE8B33A, 0x3BB273, 0xC0392B, 0x2F7DD1, 0xF2E6D0 };

		for (j = 0; j < 6; j++)
			p[j] = TX(p[j]);
		lz_poligono_mezcla(off, p, 3, band[i % 5], 200);
	}
	/* las calles de arriba, más oscuras */
	lz_rect(off, (int)TX(167), (int)TX(85), (int)TX(132), (int)TX(70), 0x120C2C);
	/* la noria (detrás de las misiones): la rueda, los radios y el pie */
	{
		float cx = 231, cy = 680, r = 66;

		lz_anillo(off, (int)TX(cx), (int)TX(cy), (int)TX(r + 3), MAX(2, (int)TX(4)), 0x7A6AB8);
		lz_anillo(off, (int)TX(cx), (int)TX(cy), (int)TX(r - 12), MAX(1, (int)TX(2)), 0x5A4A98);
		for (i = 0; i < 10; i++) {
			float a = i * M_PI_F / 5;

			lz_linea_gruesa(off, (int)TX(cx), (int)TX(cy), (int)TX(cx + r * m_cos(a)), (int)TX(cy + r * m_sen(a)), MAX(1, (int)TX(2)), 0x6A5AA8);
		}
		{
			float pie[8] = { TX(cx - 6), TX(cy), TX(cx + 6), TX(cy), TX(cx + 44), TX(cy + 92), TX(cx - 44), TX(cy + 92) };

			lz_poligono_mezcla(off, pie, 4, 0x4A3A88, 160);
		}
	}
	/* las dianas: su fondo y el letrero */
	lz_redondo(off, (int)TX(404), (int)TX(445), (int)TX(40), (int)TX(130), (int)TX(8), 0x2A1A40);
	/* la tómbola: el agujero va encima de su rueda */
	/* el delantal (abajo, a los lados de los flippers) */
	{
		float iz[8] = { 12, 900, 140, 960, 140, 1000, 12, 1000 }, de[8] = { 320, 960, 444, 900, 444, 1000, 320, 1000 };

		for (i = 0; i < 8; i++) {
			iz[i] = TX(iz[i]);
			de[i] = TX(de[i]);
		}
		lz_poligono(off, iz, 4, 0x1A1030);
		lz_poligono(off, de, 4, 0x1A1030);
	}
	/* el carril del lanzador */
	lz_degradado(off, (int)TX(452), (int)TX(300), (int)TX(36), (int)TX(700), 0x1E1540, 0x2A2050);
	lz_copia(on, 0, 0, off, 0, 0, off->an, off->al);
	/* lo de los dos (encima del fondo): las letras de la mesa */
	ambos_texto(76, 950, "HISPA", 0xE8B33A, 22);
	ambos_texto(76, 972, "PINBALL", 0xE8B33A, 16);
	ambos_texto(382, 950, "LA", 0xF2E6D0, 16);
	ambos_texto(382, 968, "FERIA", 0xF2E6D0, 22);
	ambos_texto(424, 449, "TIRO", 0xFFE3A0, 12);
	ambos_texto(TOMBOLA_X, TOMBOLA_Y + 30, "TÓMBOLA", 0xFFE3A0, 14);
	ambos_texto(56, 620, "MONTAÑA", 0xA8F0C0, 11);
	ambos_texto(56, 634, "RUSA", 0xA8F0C0, 11);
	ambos_texto(231, 772, "BONUS", 0xBFD4F0, 11);
	/* las luces */
	luz(LUZ_F, LF_CIRCULO, 189, 160, 11, 0xFFD24A, "F");
	luz(LUZ_E, LF_CIRCULO, 233, 160, 11, 0xFFD24A, "E");
	luz(LUZ_R, LF_CIRCULO, 277, 160, 11, 0xFFD24A, "R");
	luz(LUZ_I, LF_CIRCULO, 66, 745, 10, 0xFFD24A, "I");
	luz(LUZ_A, LF_CIRCULO, 394, 745, 10, 0xFFD24A, "A");
	luz(LUZ_RAMPA1, LF_FLECHA, 48, 595, 11, 0x3BD27E, 0);
	luz(LUZ_RAMPA2, LF_FLECHA, 48, 565, 11, 0x3BD27E, 0);
	luz(LUZ_RAMPA3, LF_FLECHA, 48, 535, 11, 0x3BD27E, 0);
	for (i = 0; i < 3; i++)
		luz(LUZ_DIANA_L1 + i, LF_RECT, 416, 470.0f + 40 * i, 5, 0xF08A24, 0);
	luz(LUZ_TOMBOLA, LF_RULETA, TOMBOLA_X, TOMBOLA_Y, 24, 0xE0408A, 0);
	luz(LUZ_EXTRA, LF_RECT, TOMBOLA_X, TOMBOLA_Y + 62, 10, 0x40C0F0, "EXTRA");
	luz(LUZ_X2, LF_RECT, 183, 795, 8, 0x8AB8FF, "x2");
	luz(LUZ_X3, LF_RECT, 215, 795, 8, 0x8AB8FF, "x3");
	luz(LUZ_X4, LF_RECT, 247, 795, 8, 0x8AB8FF, "x4");
	luz(LUZ_X5, LF_RECT, 279, 795, 8, 0x8AB8FF, "x5");
	{
		/* las misiones, en las barquillas de la noria; en el eje, la gran feria */
		static const char *const letra[5] = { "F", "M", "T", "Z", "B" };
		static const u32 col[5] = { 0xFFD24A, 0x3BD27E, 0xF08A24, 0xE85A4A, 0xE0408A };

		for (i = 0; i < 5; i++) {
			float a = -M_PI_F / 2 + i * 2 * M_PI_F / 5;

			luz(LUZ_MISION1 + i, LF_CIRCULO, 231 + 60 * m_cos(a), 680 + 60 * m_sen(a), 12, col[i], letra[i]);
		}
		luz(LUZ_GRAN, LF_ESTRELLA, 231, 680, 20, 0xFFF2A0, 0);
	}
	luz(LUZ_OTRA_VEZ, LF_RECT, 230, 978, 8, 0x60E0A0, "OTRA");
	/* el agujero de la tómbola, encima de su rueda (no es luz: en los dos) */
	ambos_circulo(TOMBOLA_X, TOMBOLA_Y, 15, 0x9AA2AE);
	ambos_circulo(TOMBOLA_X, TOMBOLA_Y, 12.5f, 0x08060E);
	{
		/* (las flechas pintadas que no son luces: hacia la rampa y las dianas) */
		float f1[6] = { 120, 540, 104, 520, 126, 516 };

		ambos_poligono(f1, 3, 0xFFFFFF, 40);
	}
}
