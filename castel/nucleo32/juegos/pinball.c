/* pinball.c - (JUEGOS) Hispa-Pinball: «La Feria», para Hispa-Pentium.
 *
 * Una mesa de pinball propia (pinmesa.c), vista en perspectiva desde donde
 * está el jugador, como una de verdad: el suelo de la mesa con su dibujo y
 * sus luces, las piezas con volumen (paredes, tazas, hondas, dianas, la
 * montaña rusa), la bola de acero con su brillo y su sombra, el marcador de
 * puntos de matriz de puntos a la derecha y, a la izquierda, el cartel de la
 * feria con su noria que gira. La física va en el plano de la mesa (2D,
 * pinfis.c, a 400 pasos por segundo y con subpasos) y se proyecta.
 *
 * - El fondo (el suelo, las piezas y cada luz apagada y encendida) se pinta
 *   una vez, al empezar y al cambiar el tamaño; en cada cuadro solo se
 *   rehace lo de la bola, los flippers y el émbolo, y cada luz que cambia se
 *   copia de su versión encendida o apagada (por tramos de filas: solo sus
 *   puntos). La bola se esconde detrás de lo que está delante (un búfer con
 *   la profundidad de cada punto del fondo).
 * - Las reglas: tres bolas; F-E-R (las calles de arriba) e I-A (los pasillos)
 *   encienden la palabra FERIA (y suben el multiplicador del bonus); los
 *   flippers cambian de sitio las letras de arriba; la montaña rusa, el tiro
 *   al blanco (tres dianas), las tazas (bumpers) y la tómbola (que da un
 *   premio). Cinco misiones, una tras otra, con su texto en el marcador; con
 *   las cinco, la «¡Gran Feria!»: dos bolas a la vez y los puntos al doble.
 *   La bola extra se enciende en la tómbola al acabar el tiro al blanco.
 *   Los primeros 10 s de cada bola, si se cuela, vuelve («¡Otra vez!»).
 *   Tiro de habilidad: al lanzar, la calle de arriba que parpadea.
 * - Empujar la mesa (X, punto, flecha arriba) mueve la bola; mucho seguido,
 *   «¡Cuidado!» y luego la FALTA: los flippers se apagan hasta que se pierde
 *   la bola, y no hay bonus.
 *
 * Teclas: flipper izquierdo Z, Mayús izquierda, Ctrl izquierdo o ←; derecho,
 * el «-» (la «/» de otros teclados), Mayús derecha, Ctrl derecho o →;
 * lanzar: mantener Espacio, ↓ o Intro y soltar; empujar: X, punto, ↑;
 * P pausa; Esc el menú; M música; F2 otra partida; F11 ventana o pantalla
 * completa. Empieza a pantalla completa (/VENTANA: en una ventana).
 * Para las pruebas: /S=n, /SOLO (juega solo: los flippers saltan cuando la
 * bola llega y lanza solo), /MIDE (lo que gasta) y F12 (el estado).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "pinmesa.h"

enum { E_ESPERA, E_LANZA, E_JUEGA, E_PERDIDA, E_FIN, E_PAUSA };
static const char *const nombre_estado[] = { "espera", "lanza", "juega", "perdida", "fin", "pausa" };

/* ================================================================ la pantalla */
static float P_S = 1, P_cx, P_A, P_B, P_k = 0.6f, P_hz = 0.78f, P_wlejos;
static int mesa_x0, mesa_x1, mesa_y0, mesa_y1;	/* lo que ocupa la mesa en la pantalla */
static int pan_x, pan_an, cart_x, cart_an, arriba;	/* el marcador (derecha) y el cartel (izquierda) */
static lienzo f_off, f_on, base;
static u16 *zb;
static u8 *idp;
struct tramo { u16 y, x0, x1; };
static struct tramo *tramos;
static int n_tramos, luz_ini[N_LUCES + 1];
static int luz_bx0[N_LUCES], luz_by0[N_LUCES], luz_bx1[N_LUCES], luz_by1[N_LUCES];
static u8 luz_pintada[N_LUCES];
static int listo;			/* el fondo está hecho */

static inline float w_de(float y) { return 1.0f / (1.0f + P_k * (MESA_LARGO - y) / MESA_LARGO); }
static inline void proy(float x, float y, float z, float *sx, float *sy)
{
	float w = w_de(y);

	*sx = P_cx + (x - MESA_AN / 2) * P_S * w;
	*sy = P_A + P_B * w - z * P_S * w * P_hz;
}

/* ================================================================ el juego */
static struct f_mesa M;
static int estado = E_ESPERA, antes_pausa, bola_num, bolas_extra, falta, aviso_temblor, solo, musica = 1;
static float temblor;
static long puntos, record_alto;
static int record_pos = -1;
static int multi = 1, letra[5], rampas, rampas_mision, dianas[3], tazas_mision, mision, ronda = 1, gran_feria;
static int extra_luz, extra_dada, t_dianas, t_tazas[3];
static int retenida = -1, t_tombola;	/* la bola que se ha tragado la tómbola */
static int salva_t, skill_t, skill_carril, ya_salvada;
static float tiro;			/* lo que se ha estirado el émbolo (0-1) */
static int en_lanzador;			/* hay una bola en el émbolo */
static char msg[96];
static int msg_t, msg_desplaza;
static int t_bonus;
static long bonus_bola;
static int letras_bola, rampas_bola, dianas_bola, tazas_bola;
static int golpes_flipper_dichos, tic;
static int pulsado_antes[2], solo_t[2];
static int lanzar_auto;			/* (multibola, otra vez): lanzar sola la que está en el émbolo */
static int quieta[F_MAX_BOLAS];		/* los pulsos que lleva cada bola sin moverse */

static const char *const texto_mision[5] = {
	"Enciende F-E-R-I-A", "Sube 3 veces a la montaña rusa", "Tumba las 3 dianas del tiro", "20 golpes en las tazas", "Mete la bola en la tómbola" };

static void mensaje(const char *t, int tics)
{
	snprintf(msg, sizeof msg, "%s", t);
	msg_t = tics;
	msg_desplaza = 0;
}

/* ---- el sonido ---- */
static const struct efecto s_flip = { ON_RUIDO, 3000, 1200, 30, 26, 1, 25 };
static const struct efecto s_flip2 = { ON_SENO, 140, 90, 40, 30, 1, 35 };
static const struct efecto s_bumper = { ON_CUADRADA, 700, 260, 90, 30, 1, 70 };
static const struct efecto s_bumper2 = { ON_RUIDO, 1500, 400, 70, 22, 1, 60 };
static const struct efecto s_honda = { ON_TRIANGULO, 420, 200, 60, 34, 1, 50 };
static const struct efecto s_diana = { ON_RUIDO, 700, 300, 120, 34, 1, 100 };
static const struct efecto s_rampa = { ON_SIERRA, 200, 900, 400, 18, 20, 200 };
static const struct efecto s_muelle = { ON_TRIANGULO, 180, 900, 160, 30, 2, 120 };
static const struct efecto s_pierde = { ON_TRIANGULO, 520, 80, 900, 34, 5, 700 };
static const struct efecto s_falta = { ON_CUADRADA, 110, 100, 900, 30, 5, 400 };
static const struct efecto s_tombola = { ON_SENO, 600, 150, 500, 30, 5, 400 };
static const struct efecto s_bola = { ON_CUADRADA, 2500, 2500, 10, 10, 1, 8 };

static void campanilla(int nota) { snd_nota(ON_CAMPANA, nota_hz(nota), 260, 26); }

static void organillo(void)
{
	static const int n[8] = { 72, 76, 79, 84, 79, 84, 88, 91 };
	int i;

	for (i = 0; i < 8; i++)
		snd_nota_tras(i % 2 ? ON_TRIANGULO : ON_CUADRADA, nota_hz(n[i]), i == 7 ? 600 : 140, 18, i * 140);
}

static const char vals1[] =
	"T168 O5 L4 V22 @1 g2 e | f2 d | e c g | c2. | a2 f | g2 e | f d g | c2. |"
	"e g > c | < b2 g | a f d | g2. | e g > c | < b a g | f e d | c2. |";
static const char vals2[] =
	"T168 L4 V30 @1 o3 c o4 e g | o3 c o4 e g | o3 c o4 e g | o3 c o4 e g | o3 f o4 a > c | o3 c o4 e g | o3 g o4 b > d | o3 c o4 e g |"
	"o3 c o4 e g | o3 g o4 b > d | o3 f o4 a > c | o3 g o4 b > d | o3 c o4 e g | o3 g o4 b > d | o3 g o4 f g | o3 c o4 e c |";

static void pon_musica(void)
{
	if (musica && (estado == E_JUEGA || estado == E_LANZA))
		snd_musica(vals1, vals2, 1);
	else
		snd_musica_parar();
}

/* ================================================================ las luces */
static u8 luz_quiero[N_LUCES];

static void luz_pinta(int l, int encendida)
{
	int i;
	lienzo *de = encendida ? &f_on : &f_off;

	if (!listo || luz_pintada[l] == encendida)
		return;
	luz_pintada[l] = (u8)encendida;
	for (i = luz_ini[l]; i < luz_ini[l + 1]; i++) {
		struct tramo *t = &tramos[i];
		size_t o = (size_t)t->y * base.paso + t->x0;

		memcpy(base.p + o, de->p + o, (size_t)(t->x1 - t->x0) * 4);
		memcpy(LZ.p + o, de->p + o, (size_t)(t->x1 - t->x0) * 4);
	}
	if (luz_ini[l + 1] > luz_ini[l])
		jg_sucio(luz_bx0[l], luz_by0[l], luz_bx1[l] - luz_bx0[l], luz_by1[l] - luz_by0[l]);
}

/* lo que tiene que estar encendido ahora (parpadeos incluidos) */
static void luces(void)
{
	int i, rapido = (tic / 12) % 2, lento = (tic / 30) % 2;

	memset(luz_quiero, 0, sizeof luz_quiero);
	if (estado == E_ESPERA || estado == E_FIN) {
		/* el reclamo: las luces van encendiéndose en corro */
		for (i = 0; i < N_LUCES; i++)
			luz_quiero[i] = ((i + tic / 6) % 7) < 3;
		for (i = 0; i < 3; i++)
			luz_quiero[LUZ_DIANA1 + i] = 1;
	} else if (!falta) {
		for (i = 0; i < 5; i++)
			luz_quiero[LUZ_F + i] = (u8)letra[i];
		if (estado == E_LANZA && skill_carril >= 0)
			luz_quiero[LUZ_F + skill_carril] = (u8)rapido;
		for (i = 0; i < 3; i++) {
			luz_quiero[LUZ_RAMPA1 + i] = mision == 1 ? (rampas_mision > i ? 1 : (rampas_mision == i ? (u8)lento : 0)) : (rampas % 3 > i);
			luz_quiero[LUZ_DIANA_L1 + i] = (u8)(dianas[i] ? 1 : (mision == 2 ? lento : 0));
			luz_quiero[LUZ_DIANA1 + i] = (u8)!dianas[i];
			luz_quiero[LUZ_BUMPER1 + i] = (u8)(t_tazas[i] > 0 || (mision == 3 && lento));
		}
		luz_quiero[LUZ_TOMBOLA] = (u8)(mision == 4 || extra_luz ? rapido : t_tombola > 0);
		luz_quiero[LUZ_EXTRA] = (u8)(extra_luz ? lento : 0);
		for (i = 0; i < 4; i++)
			luz_quiero[LUZ_X2 + i] = (u8)(multi >= i + 2);
		for (i = 0; i < 5; i++)
			luz_quiero[LUZ_MISION1 + i] = (u8)(i < mision || (i == mision && lento));
		luz_quiero[LUZ_GRAN] = (u8)(gran_feria ? rapido : 0);
		luz_quiero[LUZ_OTRA_VEZ] = (u8)(salva_t > 300 ? 1 : salva_t > 0 ? rapido : 0);
	} else {
		for (i = 0; i < 3; i++)
			luz_quiero[LUZ_DIANA1 + i] = (u8)!dianas[i];
	}
	for (i = 0; i < N_LUCES; i++)
		luz_pinta(i, luz_quiero[i]);
}

/* ================================================================ pintar el fondo */
/* los tramos de un polígono (sin suavizar), para la profundidad y las luces */
static void tramos_poligono(const float *xy, int n, u16 prof, int id, int pon_id)
{
	float ymin = 1e9f, ymax = -1e9f;
	int i, y, y0, y1;

	for (i = 0; i < n; i++) {
		if (xy[2 * i + 1] < ymin) ymin = xy[2 * i + 1];
		if (xy[2 * i + 1] > ymax) ymax = xy[2 * i + 1];
	}
	y0 = MAX(0, (int)(ymin + 0.5f));
	y1 = MIN(LZ.al - 1, (int)(ymax + 0.5f));
	for (y = y0; y <= y1; y++) {
		float yc = (float)y + 0.5f, cortes[24];
		int nc = 0, a, b;

		for (i = 0; i < n; i++) {
			float ax = xy[2 * i], ay = xy[2 * i + 1], bx = xy[2 * ((i + 1) % n)], by = xy[2 * ((i + 1) % n) + 1];

			if (((ay <= yc && by > yc) || (by <= yc && ay > yc)) && nc < 24)
				cortes[nc++] = ax + (yc - ay) * (bx - ax) / (by - ay);
		}
		for (a = 1; a < nc; a++) {
			float v = cortes[a];

			for (b = a - 1; b >= 0 && cortes[b] > v; b--)
				cortes[b + 1] = cortes[b];
			cortes[b + 1] = v;
		}
		for (a = 0; a + 1 < nc; a += 2) {
			int x0 = MAX(0, (int)(cortes[a] + 0.5f)), x1 = MIN(LZ.an, (int)(cortes[a + 1] + 0.5f)), x;
			size_t o = (size_t)y * LZ.an;

			for (x = x0; x < x1; x++) {
				zb[o + x] = prof;
				if (pon_id)
					idp[o + x] = (u8)id;
			}
		}
	}
}

/* un prisma de la mesa en perspectiva: los lados que se ven (con su luz) y lo de arriba.
 * En «l1» (y en «l2», si no es 0, con «arriba2» arriba); con «marca», la profundidad y la luz */
static void prisma(const struct pieza *pz, lienzo *l1, lienzo *l2, int marca)
{
	float sc[2 * PZ_MAX_PUNTOS], mx = 0, my = 0, q[8];
	int i, n = pz->n;

	for (i = 0; i < n; i++) {
		mx += pz->p[2 * i];
		my += pz->p[2 * i + 1];
	}
	mx /= (float)n;
	my /= (float)n;
	for (i = 0; i < n; i++) {
		float ax = pz->p[2 * i], ay = pz->p[2 * i + 1], bx = pz->p[2 * ((i + 1) % n)], by = pz->p[2 * ((i + 1) % n) + 1];
		float nx = by - ay, ny = -(bx - ax), l = m_raiz(nx * nx + ny * ny), cxm = (ax + bx) / 2, cym = (ay + by) / 2, luzf;
		float zta = pz->inclinada ? pz->zt[i] : pz->z1, ztb = pz->inclinada ? pz->zt[(i + 1) % n] : pz->z1;
		u32 c;

		if (l < 1e-3f)
			continue;
		nx /= l;
		ny /= l;
		if (nx * (cxm - mx) + ny * (cym - my) < 0) {
			nx = -nx;
			ny = -ny;
		}
		/* ¿mira hacia el jugador (la cámara)? */
		if (nx * (MESA_AN / 2 - cxm) + ny * (2600 - cym) <= 0)
			continue;
		proy(ax, ay, pz->z0, &q[0], &q[1]);
		proy(bx, by, pz->z0, &q[2], &q[3]);
		proy(bx, by, ztb, &q[4], &q[5]);
		proy(ax, ay, zta, &q[6], &q[7]);
		luzf = 0.52f + 0.48f * LIMITA(-0.33f * nx + 0.94f * ny, 0.0f, 1.0f);
		c = mezcla(0, pz->lado, (int)(luzf * 256));
		if (!pz->solo_luz)
			lz_poligono(l1, q, 4, c);
		if (l2)
			lz_poligono(l2, q, 4, c);
		if (marca)
			tramos_poligono(q, 4, (u16)(cym * 16), pz->solo_luz ? pz->luz + 1 : 0, 1);
	}
	for (i = 0; i < n; i++)
		proy(pz->p[2 * i], pz->p[2 * i + 1], pz->inclinada ? pz->zt[i] : pz->z1, &sc[2 * i], &sc[2 * i + 1]);
	if (!pz->solo_luz)
		lz_poligono(l1, sc, n, pz->arriba);
	if (l2)
		lz_poligono(l2, sc, n, pz->luz >= 0 ? pz->arriba_luz : pz->arriba);
	if (pz->brillo) {
		/* un brillo: lo de arriba, encogido hacia atrás a la izquierda */
		float b[2 * PZ_MAX_PUNTOS], sx = 0, sy = 0;

		for (i = 0; i < n; i++) {
			sx += sc[2 * i];
			sy += sc[2 * i + 1];
		}
		sx /= (float)n;
		sy /= (float)n;
		for (i = 0; i < n; i++) {
			b[2 * i] = sx + (sc[2 * i] - sx) * 0.55f - (sc[2 * i] - sx) * 0.0f - 1.5f;
			b[2 * i + 1] = sy + (sc[2 * i + 1] - sy) * 0.55f - 1.5f;
		}
		if (!pz->solo_luz)
			lz_poligono_mezcla(l1, b, n, 0xFFFFFF, 60);
		if (l2)
			lz_poligono_mezcla(l2, b, n, 0xFFFFFF, 80);
	}
	if (marca)
		tramos_poligono(sc, n, (u16)(my * 16), pz->luz >= 0 ? pz->luz + 1 : 0, 1);
}

/* el cartel de la izquierda (lo que no se mueve) y la noria (que gira) */
static float noria_ang;
static void pinta_noria(lienzo *l, int conluz)
{
	int cx = cart_x + cart_an / 2, cy = arriba + (LZ.al - arriba) * 62 / 100, r = MIN(cart_an / 2 - 14, (LZ.al - arriba) / 4), i;
	static const u32 barq[8] = { 0xE85A4A, 0xFFD24A, 0x3BD27E, 0x40A0F0, 0xE0408A, 0xF08A24, 0xA070E0, 0x7FC6BF };

	if (r < 20)
		return;
	lz_copia(l, cx - r - 16, cy - r - 16, &f_off, cx - r - 16, cy - r - 16, 2 * r + 32, 2 * r + 32);
	lz_anillo(l, cx, cy, r, 3, 0xC8B8F0);
	lz_anillo(l, cx, cy, r * 7 / 10, 2, 0x8878C8);
	for (i = 0; i < 8; i++) {
		float a = noria_ang + i * M_PI_F / 4;
		int bx = cx + (int)(r * m_cos(a)), by = cy + (int)(r * m_sen(a));

		lz_linea_gruesa(l, cx, cy, bx, by, 2, 0x9888D8);
		/* la barquilla cuelga siempre derecha */
		lz_redondo(l, bx - 9, by + 2, 18, 12, 4, barq[i]);
		lz_rect(l, bx - 1, by - 2, 2, 5, 0xE0E0F0);
		if (conluz && ((tic / 10 + i) % 4) == 0)
			lz_circulo_suave(l, (float)bx, (float)by + 8, 3, 0xFFFFE0);
	}
	lz_circulo_suave(l, (float)cx, (float)cy, 7, 0xFFD24A);
	jg_sucio(cx - r - 16, cy - r - 16, 2 * r + 32, 2 * r + 32);
}

static void pinta_cartel_fijo(lienzo *l)
{
	int i, cx = cart_x + cart_an / 2, y = arriba + 24, e = cart_an >= 230 ? 3 : 2;

	if (cart_an < 120)
		return;
	lz_degradado(l, cart_x, arriba, cart_an, LZ.al - arriba, 0x1A0F3A, 0x0C1830);
	/* las bombillas del marco */
	for (i = cart_x + 8; i < cart_x + cart_an - 8; i += 16) {
		lz_circulo_suave(l, (float)i, (float)arriba + 8, 3, 0xFFE8A0);
		lz_circulo_suave(l, (float)i, (float)LZ.al - 8, 3, 0xFFE8A0);
	}
	lz_texto_sombra(l, cx - ancho_texto("HISPA", e) / 2, y, "HISPA", 0xFFD24A, 0x5A2A00, e);
	y += 16 * e + 2;
	lz_texto_sombra(l, cx - ancho_texto("PINBALL", e) / 2, y, "PINBALL", 0xFFD24A, 0x5A2A00, e);
	y += 16 * e + 12;
	lz_redondo(l, cart_x + 12, y, cart_an - 24, 16 * e + 12, 8, 0xB8322A);
	lz_texto_sombra(l, cx - ancho_texto("La Feria", e) / 2, y + 6, "La Feria", 0xF2E6D0, 0x401010, e);
	/* el suelo de la feria, abajo */
	lz_degradado(l, cart_x, LZ.al - 60, cart_an, 52, 0x2A1A10, 0x140C08);
}

/* el marco del marcador (lo que no se mueve) */
static int dmd_x, dmd_y1, dmd_y2, dmd_d, info_y;
static void pinta_marcador_fijo(lienzo *l)
{
	int y = arriba + 10, e = pan_an >= 280 ? 2 : 1;

	lz_degradado(l, pan_x, arriba, pan_an, LZ.al - arriba, 0x161228, 0x0A0A16);
	lz_texto_centro(l, pan_x + pan_an / 2, y, "Hispa-Pinball", 0xFFD24A, e, 1);
	y += 16 * e + 10;
	dmd_d = MAX(2, (pan_an - 24) / 98);
	dmd_x = pan_x + (pan_an - 96 * dmd_d) / 2;
	dmd_y1 = y;
	dmd_y2 = y + 16 * dmd_d + 10;
	lz_redondo(l, dmd_x - 8, dmd_y1 - 8, 96 * dmd_d + 16, 2 * 16 * dmd_d + 26, 6, 0x2A2A30);
	info_y = dmd_y2 + 16 * dmd_d + 18;
}

/* una línea del marcador de puntos (96 x 16 puntos ámbar): el texto, desde el punto «desde» */
static void dmd(int y, const char *t, int desde, int centrado)
{
	char b[128];
	int n = a437(t, b, sizeof b), f, k, total = n * 8, x0 = centrado ? (96 - total) / 2 : -desde, d = dmd_d;

	if (dmd_x < 0 || dmd_x + 96 * d > LZ.an || y < 0 || y + 16 * d > LZ.al)
		return;
	lz_rect(&LZ, dmd_x - 2, y - 2, 96 * d + 4, 16 * d + 4, 0x120A04);
	for (f = 0; f < 16; f++) {
		u32 *fila = LZ.p + (size_t)(y + f * d) * LZ.paso + dmd_x;

		for (k = 0; k < 96; k++) {
			int c = k - x0, on = 0, i, j;
			u32 col;

			if (c >= 0 && c < total)
				on = (jg_letra((unsigned char)b[c / 8])[f] >> (7 - c % 8)) & 1;
			col = on ? 0xFFB030 : 0x3A1E08;
			/* (cada punto, d-1 x d-1: lo demás es la rejilla) */
			for (j = 0; j < d - 1; j++)
				for (i = 0; i < d - 1; i++)
					fila[(size_t)j * LZ.paso + k * d + i] = col;
		}
	}
	jg_sucio(dmd_x - 2, y - 2, 96 * d + 4, 16 * d + 4);
}

static void con_puntos(char *d, long n)
{
	char t[16];
	int l, i, k = 0;

	snprintf(t, sizeof t, "%d", (int)n);
	l = (int)strlen(t);
	for (i = 0; i < l; i++) {
		d[k++] = t[i];
		if ((l - i - 1) % 3 == 0 && i != l - 1)
			d[k++] = '.';
	}
	d[k] = 0;
}

static void pinta_info(void)
{
	char t[96];
	int y = info_y, i, x = pan_x + 14, a = pan_an - 28;

	lz_copia(&LZ, pan_x, info_y - 4, &base, pan_x, info_y - 4, pan_an, LZ.al - info_y + 4);
	if (estado == E_ESPERA || estado == E_FIN)
		snprintf(t, sizeof t, "Pulse Intro para jugar");
	else
		snprintf(t, sizeof t, "Bola %d de 3%s   Bonus x%d", MIN(bola_num, 3), bolas_extra ? " (+1)" : "", multi);
	lz_negrita(&LZ, x, y, t, 0xFFFFFF, 1);
	y += 22;
	{
		char p[20];

		con_puntos(p, MAX(record_alto, puntos));
		snprintf(t, sizeof t, "Récord: %s", p);
		lz_texto(&LZ, x, y, t, 0x9FB2CC, 1);
	}
	y += 26;
	lz_negrita(&LZ, x, y, gran_feria ? "¡GRAN FERIA! (puntos x2)" : ronda > 1 ? "Misiones (otra vuelta)" : "Misiones", 0xFFD24A, 1);
	y += 22;
	for (i = 0; i < 5; i++) {
		u32 c = i < mision ? 0x8FE0A0 : i == mision ? 0xFFFFFF : 0x7F8AA0;
		char pr[24] = "";

		if (i == mision && i == 1)
			snprintf(pr, sizeof pr, " (%d/3)", rampas_mision);
		else if (i == mision && i == 3)
			snprintf(pr, sizeof pr, " (%d/20)", tazas_mision);
		snprintf(t, sizeof t, "%s %d. %s%s", i < mision ? "\xe2\x88\x9a" : i == mision ? "»" : " ", i + 1, texto_mision[i], pr);
		y += lz_parrafo(&LZ, x, y, a, t, c, 1, 17) * 17 + 3;
	}
	y += 10;
	lz_texto(&LZ, x, y, "Flippers: Z y -", 0x9FB2CC, 1);
	lz_texto(&LZ, x, y + 17, "(o Mayús, Ctrl, flechas)", 0x7F8AA0, 1);
	lz_texto(&LZ, x, y + 34, "Lanzar: Espacio (mantener)", 0x9FB2CC, 1);
	lz_texto(&LZ, x, y + 51, "Empujar: X . flecha arriba", 0x9FB2CC, 1);
	lz_texto(&LZ, x, y + 68, "Esc: menú   P: pausa", 0x9FB2CC, 1);
	if (falta)
		lz_negrita(&LZ, x, y + 92, "FALTA: flippers apagados", 0xFF6050, 1);
	else if (aviso_temblor)
		lz_negrita(&LZ, x, y + 92, "¡Cuidado! No empuje tanto", 0xFFB030, 1);
	jg_sucio(pan_x, info_y - 4, pan_an, LZ.al - info_y + 4);
}

static void construye(void)
{
	lienzo t_off, t_on;
	u8 *t_id;
	float kt;
	int i, x, y, tw, th, mesa_an_px, alto;
	float hn, hf;

	listo = 0;
	arriba = jg_completa ? 0 : ALTO_BARRA;
	alto = LZ.al - arriba;
	/* el sitio de cada cosa: la mesa en medio, el marcador a la derecha y el cartel a la izquierda */
	hn = (float)(LZ.al - 4);
	hf = (float)(arriba + 34);
	P_wlejos = 1.0f / (1.0f + P_k);
	P_B = (hn - hf) / (1.0f - P_wlejos);
	P_A = hn - P_B;
	P_S = (hn - hf) * 0.96f / MESA_AN;
	pan_an = LIMITA(LZ.an / 4, 200, 320);
	mesa_an_px = (int)(MESA_AN * P_S) + 20;
	if (mesa_an_px + pan_an > LZ.an) {
		P_S = (float)(LZ.an - pan_an - 24) / MESA_AN;
		mesa_an_px = (int)(MESA_AN * P_S) + 20;
	}
	cart_an = LZ.an - mesa_an_px - pan_an;
	if (cart_an > 360)
		cart_an = 360;
	if (cart_an < 120)
		cart_an = 0;
	{
		int libre = LZ.an - mesa_an_px - pan_an - cart_an;

		cart_x = libre / 2;
		P_cx = (float)(cart_x + cart_an + mesa_an_px / 2);
		pan_x = cart_x + cart_an + mesa_an_px;
	}
	mesa_x0 = (int)(P_cx - MESA_AN / 2 * P_S) - 2;
	mesa_x1 = (int)(P_cx + MESA_AN / 2 * P_S) + 2;
	mesa_y0 = (int)hf - 40;
	mesa_y1 = LZ.al;
	(void)alto;
	/* las capas */
	lz_libre(&f_off);
	lz_libre(&f_on);
	lz_libre(&base);
	free(zb);
	free(idp);
	free(tramos);
	tramos = 0;
	lz_nuevo(&f_off, LZ.an, LZ.al);
	lz_nuevo(&f_on, LZ.an, LZ.al);
	lz_nuevo(&base, LZ.an, LZ.al);
	zb = (u16 *)calloc((size_t)LZ.an * LZ.al, 2);
	idp = (u8 *)calloc((size_t)LZ.an * LZ.al, 1);
	kt = LIMITA(P_S, 0.6f, 1.4f);
	tw = (int)(MESA_AN * kt) + 1;
	th = (int)(MESA_LARGO * kt) + 1;
	lz_nuevo(&t_off, tw, th);
	lz_nuevo(&t_on, tw, th);
	t_id = (u8 *)malloc((size_t)tw * th);
	if (!f_off.p || !f_on.p || !base.p || !zb || !idp || !t_off.p || !t_on.p || !t_id) {
		serie("pinball: no hay memoria para la mesa (%d x %d)\n", LZ.an, LZ.al);
		lz_libre(&t_off);
		lz_libre(&t_on);
		free(t_id);
		return;
	}
	mesa_suelo(&t_off, &t_on, t_id, kt);
	/* lo de fuera de la mesa: el mueble (en los dos) */
	lz_degradado(&f_off, 0, 0, LZ.an, LZ.al, 0x1A1424, 0x0A0810);
	lz_copia(&f_on, 0, 0, &f_off, 0, 0, LZ.an, LZ.al);
	/* el suelo de la mesa, en perspectiva: cada fila de la pantalla es una y de la mesa */
	for (y = MAX(0, (int)hf - 1); y < MIN(LZ.al, (int)hn + 1); y++) {
		float w = ((float)y + 0.5f - P_A) / P_B, my, sw, du, u;
		int sx0, sx1, v0, fv;
		u32 *po, *pn;
		u8 *pi;

		if (w < P_wlejos || w > 1.0f)
			continue;
		my = MESA_LARGO - (1.0f / w - 1.0f) * MESA_LARGO / P_k;
		sw = P_S * w;
		sx0 = MAX(0, (int)(P_cx - MESA_AN / 2 * sw + 0.5f));
		sx1 = MIN(LZ.an - 1, (int)(P_cx + MESA_AN / 2 * sw - 0.5f));
		{
			float vv = my * kt - 0.5f;

			v0 = (int)vv;
			if (v0 < 0) v0 = 0;
			if (v0 >= th - 1) v0 = th - 2;
			fv = (int)((vv - (float)v0) * 256);
			fv = LIMITA(fv, 0, 256);
		}
		du = kt / sw;
		u = ((float)sx0 + 0.5f - P_cx) / sw * kt + MESA_AN / 2 * kt - 0.5f;
		po = f_off.p + (size_t)y * f_off.paso;
		pn = f_on.p + (size_t)y * f_on.paso;
		pi = idp + (size_t)y * LZ.an;
		for (x = sx0; x <= sx1; x++, u += du) {
			int u0 = (int)u, fu, k;
			u32 a, b, c, d, r[2];

			if (u0 < 0) u0 = 0;
			if (u0 >= tw - 1) u0 = tw - 2;
			fu = LIMITA((int)((u - (float)u0) * 256), 0, 256);
			for (k = 0; k < 2; k++) {
				const lienzo *t = k ? &t_on : &t_off;
				const u32 *f0 = t->p + (size_t)v0 * t->paso + u0;

				a = f0[0];
				b = f0[1];
				c = f0[t->paso];
				d = f0[t->paso + 1];
				r[k] = mezcla(mezcla(a, b, fu), mezcla(c, d, fu), fv);
			}
			{
				/* lo de lejos, más oscuro; y un brillo de cristal en diagonal */
				int sombra = (int)((1.0f - w) * 150.0f);
				float s = (float)(x - mesa_x0) / (float)(mesa_x1 - mesa_x0) + (float)(y - hf) / (hn - hf) * 0.45f - 0.62f;
				int brillo = (int)(18.0f - m_abs(s) * 120.0f);

				for (k = 0; k < 2; k++) {
					r[k] = mezcla(r[k], 0, sombra);
					if (brillo > 0)
						r[k] = mezcla(r[k], 0xFFFFFF, brillo);
				}
			}
			po[x] = r[0];
			pn[x] = r[1];
			pi[x] = t_id[(size_t)LIMITA((int)(my * kt), 0, th - 1) * tw + LIMITA((int)(u + 0.5f), 0, tw - 1)];
		}
	}
	lz_libre(&t_off);
	lz_libre(&t_on);
	free(t_id);
	/* las piezas, de atrás adelante (en los dos; con la profundidad y las luces) */
	for (i = 0; i < n_piezas; i++)
		prisma(&piezas[i], &f_off, &f_on, 1);
	/* el cartel y el marcador (en los dos: no tienen luces) */
	pinta_cartel_fijo(&f_off);
	pinta_cartel_fijo(&f_on);
	pinta_marcador_fijo(&f_off);
	pinta_marcador_fijo(&f_on);
	/* los tramos de cada luz (por filas), y lo que ocupa */
	{
		int cuenta[N_LUCES + 1], l;
		size_t o;

		memset(cuenta, 0, sizeof cuenta);
		for (y = 0; y < LZ.al; y++) {
			int ant = 0;

			o = (size_t)y * LZ.an;
			for (x = 0; x < LZ.an; x++) {
				int v = idp[o + x];

				if (v && v != ant && v <= N_LUCES)
					cuenta[v - 1]++;
				ant = v;
			}
		}
		luz_ini[0] = 0;
		for (l = 0; l < N_LUCES; l++)
			luz_ini[l + 1] = luz_ini[l] + cuenta[l];
		n_tramos = luz_ini[N_LUCES];
		tramos = (struct tramo *)malloc((size_t)(n_tramos + 1) * sizeof *tramos);
		memset(cuenta, 0, sizeof cuenta);
		for (l = 0; l < N_LUCES; l++) {
			luz_bx0[l] = luz_by0[l] = 1 << 30;
			luz_bx1[l] = luz_by1[l] = -1;
		}
		for (y = 0; y < LZ.al && tramos; y++) {
			o = (size_t)y * LZ.an;
			for (x = 0; x < LZ.an;) {
				int v = idp[o + x], x0 = x;

				while (x < LZ.an && idp[o + x] == v)
					x++;
				if (v && v <= N_LUCES) {
					struct tramo *t = &tramos[luz_ini[v - 1] + cuenta[v - 1]++];

					l = v - 1;
					t->y = (u16)y;
					t->x0 = (u16)x0;
					t->x1 = (u16)x;
					luz_bx0[l] = MIN(luz_bx0[l], x0);
					luz_bx1[l] = MAX(luz_bx1[l], x);
					luz_by0[l] = MIN(luz_by0[l], y);
					luz_by1[l] = MAX(luz_by1[l], y + 1);
				}
			}
		}
	}
	free(idp);
	idp = 0;
	/* la base: todo apagado; luego, lo que toca encender */
	lz_copia(&base, 0, 0, &f_off, 0, 0, LZ.an, LZ.al);
	memset(luz_pintada, 0, sizeof luz_pintada);
	listo = 1;
	luces();
	lz_copia(&LZ, 0, 0, &base, 0, 0, LZ.an, LZ.al);
	serie("pinball: mesa de %d x %d (escala %d %%), %d piezas, %d tramos de luces\n", LZ.an, LZ.al, (int)(P_S * 100), n_piezas, n_tramos);
}

/* ================================================================ lo que se mueve */
#define MAX_SUC 24
static struct { int x, y, an, al; } s_ahora[MAX_SUC];
static int n_s_ahora;
/* lo que se mueve: cada cosa, con lo que la define (si no cambia, no se repinta) */
#define MAX_MOVILES (2 + 1 + F_MAX_BOLAS)
struct movil { int x0, y0, x1, y1, firma, hay; };
static struct movil mov_antes[MAX_MOVILES];


static void apunta(float x0, float y0, float x1, float y1)
{
	if (n_s_ahora < MAX_SUC) {
		s_ahora[n_s_ahora].x = (int)x0 - 2;
		s_ahora[n_s_ahora].y = (int)y0 - 2;
		s_ahora[n_s_ahora].an = (int)(x1 - x0) + 5;
		s_ahora[n_s_ahora].al = (int)(y1 - y0) + 5;
		n_s_ahora++;
	}
}

static void pinta_pieza_movil(struct pieza *pz)
{
	float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f, sx, sy;
	int i, k;

	prisma(pz, &LZ, 0, 0);
	for (i = 0; i < pz->n; i++)
		for (k = 0; k < 2; k++) {
			proy(pz->p[2 * i], pz->p[2 * i + 1], k ? pz->z1 : pz->z0, &sx, &sy);
			x0 = MIN(x0, sx); x1 = MAX(x1, sx);
			y0 = MIN(y0, sy); y1 = MAX(y1, sy);
		}
	apunta(x0, y0, x1, y1);
}

static void pinta_flipper(int k)
{
	struct f_flip *f = &M.flips[k];
	struct pieza pz;
	float tx, ty;
	int j;

	memset(&pz, 0, sizeof pz);
	f_punta(f, &tx, &ty);
	for (j = 0; j < 7; j++) {
		float t = -M_PI_F / 2 + j * M_PI_F / 6;

		pz.p[2 * j] = tx + f->r1 * m_cos(f->ang + t);
		pz.p[2 * j + 1] = ty + f->r1 * m_sen(f->ang + t);
		pz.p[2 * (j + 7)] = f->px + f->r0 * m_cos(f->ang + M_PI_F / 2 + t + M_PI_F / 2);
		pz.p[2 * (j + 7) + 1] = f->py + f->r0 * m_sen(f->ang + M_PI_F / 2 + t + M_PI_F / 2);
	}
	pz.n = 14;
	pz.z1 = 14;
	pz.lado = falta ? 0x5A3A3A : 0xB82A24;
	pz.arriba = falta ? 0x9A948C : 0xF4EEDC;
	pz.brillo = 1;
	pz.luz = -1;
	pinta_pieza_movil(&pz);
}

static void pinta_embolo(void)
{
	struct pieza pz;
	float y = EMBOLO_Y + 1 + tiro * 24;
	float q[8] = { 459, y, 477, y, 477, y + 9, 459, y + 9 };
	float r[8] = { 465, y + 9, 471, y + 9, 471, 1000, 465, 1000 };

	memset(&pz, 0, sizeof pz);
	pz.luz = -1;
	memcpy(pz.p, r, sizeof r);
	pz.n = 4;
	pz.z1 = 8;
	pz.lado = 0x7A828C;
	pz.arriba = 0xC8D0D8;
	pinta_pieza_movil(&pz);
	memcpy(pz.p, q, sizeof q);
	pz.z1 = 14;
	pz.lado = 0x8E2420;
	pz.arriba = 0xD8443A;
	pz.brillo = 1;
	pinta_pieza_movil(&pz);
}

static void pinta_bola(int i)
{
	struct f_bola *b = &M.bolas[i];
	float z = RADIO_BOLA + (b->capa == CAPA_RAMPA ? altura_rampa(b->y) : 0), sx, sy, rs, hx, hy, hr;
	int x0, x1, y0, y1, px, py, prueba_z = b->capa == CAPA_MESA;
	u16 yo = (u16)(LIMITA(b->y, 0, 4000) * 16 + 10);
	float hundida = (i == retenida) ? 0.75f : 1.0f;

	proy(b->x, b->y, z * hundida, &sx, &sy);
	rs = RADIO_BOLA * P_S * w_de(b->y) * hundida;
	/* la sombra, en el suelo (o en la rampa), un poco abajo a la derecha */
	proy(b->x + 6, b->y + 8, b->capa == CAPA_RAMPA ? altura_rampa(b->y + 8) : 0, &hx, &hy);
	hr = rs * 1.05f;
	x0 = MAX(0, (int)(hx - hr - 2));
	x1 = MIN(LZ.an - 1, (int)(hx + hr + 2));
	y0 = MAX(0, (int)(hy - hr * 0.6f - 2));
	y1 = MIN(LZ.al - 1, (int)(hy + hr * 0.6f + 2));
	for (py = y0; py <= y1; py++) {
		u32 *p = LZ.p + (size_t)py * LZ.paso;

		for (px = x0; px <= x1; px++) {
			float dx = (px + 0.5f - hx) / hr, dy = (py + 0.5f - hy) / (hr * 0.6f), d2 = dx * dx + dy * dy;

			if (d2 >= 1.0f || (prueba_z && zb[(size_t)py * LZ.an + px] > yo + 160))
				continue;
			p[px] = mezcla(p[px], 0, (int)((1.0f - d2) * 150));
		}
	}
	apunta((float)x0, (float)y0, (float)x1, (float)y1);
	/* la bola: acero, con la luz de arriba a la izquierda, un reflejo de la mesa abajo y su brillo */
	x0 = MAX(0, (int)(sx - rs - 2));
	x1 = MIN(LZ.an - 1, (int)(sx + rs + 2));
	y0 = MAX(0, (int)(sy - rs - 2));
	y1 = MIN(LZ.al - 1, (int)(sy + rs + 2));
	for (py = y0; py <= y1; py++) {
		u32 *p = LZ.p + (size_t)py * LZ.paso;

		for (px = x0; px <= x1; px++) {
			float dx = (px + 0.5f - sx) / rs, dy = (py + 0.5f - sy) / rs, d2 = dx * dx + dy * dy, nz, dif, esp, borde;
			u32 c;

			if (d2 >= 1.0f + 2.0f / rs)
				continue;
			if (prueba_z && zb[(size_t)py * LZ.an + px] > yo)
				continue;
			nz = m_raiz(MAX(0.0f, 1.0f - d2));
			dif = -0.42f * dx - 0.55f * dy + 0.72f * nz;
			esp = -0.25f * dx - 0.33f * dy + 0.91f * nz;
			esp = esp > 0.9f ? (esp - 0.9f) * 10.0f : 0;
			c = mezcla(0x262A32, 0xDCE2EA, (int)(LIMITA(dif * 0.8f + 0.35f, 0.0f, 1.0f) * 256));
			if (dy > 0.2f)
				c = mezcla(c, 0x4A3A8A, (int)((dy - 0.2f) * 120));	/* (lo que refleja de la mesa) */
			c = mezcla(c, 0xFFFFFF, (int)(LIMITA(esp, 0.0f, 1.0f) * 256));
			borde = (1.0f - m_raiz(d2)) * rs + 0.5f;
			p[px] = borde >= 1.0f ? c : mezcla(p[px], c, (int)(LIMITA(borde, 0.0f, 1.0f) * 256));
		}
	}
	apunta((float)x0, (float)y0, (float)x1, (float)y1);
}

static char dmd1_antes[40], dmd2_antes[100];
static int dmd2_desde_antes = -1, info_antes = -1;

static void pinta_marcador(int todo)
{
	char t1[40], t2[100];
	int desde = 0, centrado = 1, firma;

	if (estado == E_ESPERA || estado == E_FIN) {
		static const char *const r[3] = { "HISPA", "PINBALL", "LA FERIA" };

		if (estado == E_FIN && (tic / 150) % 2 == 0)
			con_puntos(t1, puntos);
		else
			snprintf(t1, sizeof t1, "%s", r[(tic / 120) % 3]);
	} else
		con_puntos(t1, puntos);
	if (msg_t > 0)
		snprintf(t2, sizeof t2, "%s", msg);
	else if (estado == E_ESPERA)
		snprintf(t2, sizeof t2, "Pulse Intro para jugar");
	else if (estado == E_FIN)
		snprintf(t2, sizeof t2, "Fin de la partida. Intro: otra");
	else if (estado == E_LANZA)
		snprintf(t2, sizeof t2, "Bola %d: lance con Espacio", MIN(bola_num, 3));
	else
		snprintf(t2, sizeof t2, "Misión %d: %s", mision + 1, texto_mision[mision]);
	{
		char b[100];

		if (a437(t2, b, sizeof b) > 12) {
			int total = ((int)strlen(b) + 4) * 8;

			centrado = 0;
			desde = ((tic / 2) % (total + 96)) - 96;	/* (de derecha a izquierda, 50 puntos por segundo) */
		}
	}
	if (todo || strcmp(t1, dmd1_antes)) {
		strcpy(dmd1_antes, t1);
		dmd(dmd_y1, t1, 0, 1);
	}
	if (todo || strcmp(t2, dmd2_antes) || desde != dmd2_desde_antes) {
		strcpy(dmd2_antes, t2);
		dmd2_desde_antes = desde;
		dmd(dmd_y2, t2, desde, centrado);
	}
	firma = (int)(estado * 7 + bola_num * 131 + multi * 17 + mision * 1031 + rampas_mision * 77 + tazas_mision * 3 + falta * 5003 + aviso_temblor * 997 +
		      bolas_extra * 4999 + gran_feria * 13 + ronda * 101 + (int)(MAX(record_alto, puntos) >> 4));
	if (todo || firma != info_antes) {
		info_antes = firma;
		pinta_info();
	}
}

static void pinta_todo(void)
{
	if (!listo)
		return;
	lz_copia(&LZ, 0, 0, &base, 0, 0, LZ.an, LZ.al);
	if (!jg_completa)
		jg_pinta_barra();
	memset(mov_antes, 0, sizeof mov_antes);
	pinta_marcador(1);
	if (cart_an)
		pinta_noria(&LZ, 1);
	jg_todo_sucio();
}


static int toca(const struct movil *a, int x0, int y0, int x1, int y1)
{
	return a->hay && a->x0 < x1 && x0 < a->x1 && a->y0 < y1 && y0 < a->y1;
}

static void pinta_movil(int k)
{
	if (k < 2)
		pinta_flipper(k);
	else if (k == 2)
		pinta_embolo();
	else
		pinta_bola(k - 3);
}

/* el orden de atrás adelante: las bolas de lejos, los flippers, las de cerca, el émbolo y las de la rampa */
static int orden_de(int k)
{
	struct f_bola *b;

	if (k < 2)
		return 10;
	if (k == 2)
		return 30;
	b = &M.bolas[k - 3];
	if (b->capa == CAPA_RAMPA)
		return 40;
	if (b->y >= 880)
		return b->x >= 452 ? 35 : 20;
	return 0;
}

static void pinta(void)
{
	static int noria_t;
	struct movil ahora[MAX_MOVILES];
	int cambia[MAX_MOVILES], i, k, o;

	if (!listo)
		return;
	/* lo que es cada cosa ahora (lo que la define y lo que ocupa) */
	for (k = 0; k < MAX_MOVILES; k++) {
		memset(&ahora[k], 0, sizeof ahora[k]);
		n_s_ahora = 0;
		if (k < 2)
			ahora[k].firma = (int)(M.flips[k].ang * 1000) * 2 + falta;
		else if (k == 2)
			ahora[k].firma = (int)(tiro * 1000);
		else {
			struct f_bola *b = &M.bolas[k - 3];

			if (!b->viva && k - 3 != retenida)
				continue;
			ahora[k].firma = ((int)(b->x * 4) * 7919) ^ ((int)(b->y * 4) * 31) ^ (b->capa << 28) ^ (k - 3 == retenida);
		}
		ahora[k].hay = 1;
	}
	/* cambia: lo que ya no es como antes (o no estaba, o se ha ido) */
	for (k = 0; k < MAX_MOVILES; k++)
		cambia[k] = ahora[k].hay != mov_antes[k].hay || ahora[k].firma != mov_antes[k].firma || estado == E_PAUSA;
	/* lo de antes de lo que cambia, se borra (de la base) */
	for (k = 0; k < MAX_MOVILES; k++)
		if (cambia[k] && mov_antes[k].hay) {
			struct movil *a = &mov_antes[k];

			lz_copia(&LZ, a->x0, a->y0, &base, a->x0, a->y0, a->x1 - a->x0, a->y1 - a->y0);
			jg_sucio(a->x0, a->y0, a->x1 - a->x0, a->y1 - a->y0);
		}
	/* lo que no cambia pero toca lo borrado, también se repinta (hasta que no quede nada) */
	for (o = 0; o < 3; o++)
		for (k = 0; k < MAX_MOVILES; k++) {
			if (cambia[k] || !mov_antes[k].hay)
				continue;
			for (i = 0; i < MAX_MOVILES; i++)
				if (cambia[i] && mov_antes[i].hay && toca(&mov_antes[k], mov_antes[i].x0, mov_antes[i].y0, mov_antes[i].x1, mov_antes[i].y1)) {
					struct movil *a = &mov_antes[k];

					cambia[k] = 1;
					lz_copia(&LZ, a->x0, a->y0, &base, a->x0, a->y0, a->x1 - a->x0, a->y1 - a->y0);
					jg_sucio(a->x0, a->y0, a->x1 - a->x0, a->y1 - a->y0);
					break;
				}
		}
	/* y se pinta lo que cambia, de atrás adelante; lo que no, se queda como estaba */
	for (o = 0; o <= 40; o += 5)
		for (k = 0; k < MAX_MOVILES; k++) {
			int j, x0 = 1 << 30, y0 = 1 << 30, x1 = -1, y1 = -1;

			if (!ahora[k].hay || !cambia[k] || orden_de(k) != o)
				continue;
			n_s_ahora = 0;
			pinta_movil(k);
			for (j = 0; j < n_s_ahora; j++) {
				x0 = MIN(x0, s_ahora[j].x);
				y0 = MIN(y0, s_ahora[j].y);
				x1 = MAX(x1, s_ahora[j].x + s_ahora[j].an);
				y1 = MAX(y1, s_ahora[j].y + s_ahora[j].al);
			}
			ahora[k].x0 = MAX(0, x0);
			ahora[k].y0 = MAX(0, y0);
			ahora[k].x1 = MIN(LZ.an, x1);
			ahora[k].y1 = MIN(LZ.al, y1);
			jg_sucio(ahora[k].x0, ahora[k].y0, ahora[k].x1 - ahora[k].x0, ahora[k].y1 - ahora[k].y0);
		}
	for (k = 0; k < MAX_MOVILES; k++) {
		if (ahora[k].hay && !cambia[k]) {
			ahora[k].x0 = mov_antes[k].x0;
			ahora[k].y0 = mov_antes[k].y0;
			ahora[k].x1 = mov_antes[k].x1;
			ahora[k].y1 = mov_antes[k].y1;
		}
		mov_antes[k] = ahora[k];
	}
	if (estado == E_PAUSA) {
		int an = MIN(mesa_x1 - mesa_x0 - 20, 380);

		jg_cartel(&LZ, (int)P_cx - an / 2, (LZ.al - 260) / 2, an, 260, "Pausa",
			  "Intro o Esc: seguir.\nF2: partida nueva.\nM: música (sí o no).\nF11: ventana o pantalla completa.\nR: mejores puntuaciones.\nS: salir del juego.");
		jg_sucio((int)P_cx - an / 2, (LZ.al - 260) / 2, an + 6, 266);
	}
	pinta_marcador(0);
	if (cart_an && ++noria_t >= 5) {
		noria_t = 0;
		pinta_noria(&LZ, 1);
	}
}

/* ================================================================ las reglas */
static void suma(long n) { puntos += gran_feria ? 2 * n : n; }

static int bolas_vivas(void)
{
	int i, n = retenida >= 0;

	for (i = 0; i < F_MAX_BOLAS; i++)
		if (M.bolas[i].viva)
			n++;
	return n;
}

static void bola_al_lanzador(int automatica)
{
	int i;

	for (i = 0; i < F_MAX_BOLAS; i++)
		if (!M.bolas[i].viva && i != retenida) {
			memset(&M.bolas[i], 0, sizeof M.bolas[i]);
			M.bolas[i].viva = 1;
			M.bolas[i].capa = CAPA_MESA;
			M.bolas[i].x = LANZA_X;
			M.bolas[i].y = LANZA_Y;
			break;
		}
	tiro = 0;
	lanzar_auto = automatica ? 30 : 0;
}

static void mision_cumplida(void)
{
	char t[96];

	suma(100000L * (mision + 1) * ronda);
	snprintf(t, sizeof t, "¡Misión cumplida! %s", texto_mision[mision]);
	mensaje(t, 300);
	organillo();
	serie("pinball: misión %d cumplida (%s); %ld puntos\n", mision + 1, texto_mision[mision], puntos);
	mision++;
	rampas_mision = 0;
	tazas_mision = 0;
	if (mision == 5) {
		/* ¡la gran feria!: otra bola y los puntos al doble mientras haya dos */
		gran_feria = 1;
		mision = 0;
		ronda++;
		mensaje("¡GRAN FERIA! ¡Dos bolas y puntos al doble!", 400);
		bola_al_lanzador(1);
		serie("pinball: ¡gran feria! (dos bolas)\n");
	}
}

static void feria_completa(void)
{
	char t[64];

	memset(letra, 0, sizeof letra);
	multi = MIN(5, multi + 1);
	suma(25000L * ronda);
	snprintf(t, sizeof t, "¡FERIA! Bonus x%d", multi);
	mensaje(t, 250);
	organillo();
	if (mision == 0)
		mision_cumplida();
}

static void enciende_letra(int k)
{
	if (letra[k]) {
		suma(100);
		return;
	}
	letra[k] = 1;
	letras_bola++;
	suma(2000);
	campanilla(76 + 2 * k);
	if (letra[0] && letra[1] && letra[2] && letra[3] && letra[4])
		feria_completa();
}

static struct f_seg *seg_diana[3];

static void choque(int tipo, int id, float f, int b)
{
	(void)b;
	switch (tipo) {
	case F_BUMPER: {
		int k = id - ID_BUMPER0;

		if (k < 0 || k > 2)
			return;
		suma(1000);
		t_tazas[k] = 12;
		tazas_bola++;
		snd_efecto(&s_bumper);
		snd_efecto(&s_bumper2);
		if (mision == 3 && ++tazas_mision >= 20)
			mision_cumplida();
		break;
	}
	case F_HONDA:
		suma(100);
		snd_efecto(&s_honda);
		break;
	case F_DIANA: {
		int k = id - ID_DIANA0;

		if (k < 0 || k > 2 || dianas[k])
			return;
		dianas[k] = 1;
		if (seg_diana[k])
			seg_diana[k]->activo = 0;
		dianas_bola++;
		suma(2500);
		snd_efecto(&s_diana);
		if (dianas[0] && dianas[1] && dianas[2]) {
			suma(25000L * ronda);
			mensaje("¡Tiro al blanco!", 200);
			t_dianas = 150;
			if (mision == 2)
				mision_cumplida();
			if (!extra_dada) {
				extra_luz = 1;
				extra_dada = 1;
				mensaje("¡La bola extra, en la tómbola!", 250);
			}
		}
		break;
	}
	case F_FLIPPER:
		if (golpes_flipper_dichos < 6 && f > 400) {
			golpes_flipper_dichos++;
			serie("pinball: el flipper %s golpea la bola (%d u/s)\n", id ? "derecho" : "izquierdo", (int)f);
		}
		break;
	case F_BOLA:
		snd_efecto(&s_bola);
		break;
	case F_GOMA: case F_POSTE:
		if (f > 300)
			snd_nota(ON_TRIANGULO, 330, 40, 18);
		break;
	}
}

static void cruce(int id, int ib, int sentido)
{
	struct f_bola *b = &M.bolas[ib];

	(void)sentido;
	if (id >= S_CARRIL0 && id <= S_CARRIL2 && b->vy > 0) {
		int k = id - S_CARRIL0;

		if (skill_t > 0 && k == skill_carril) {
			suma(50000L * ronda);
			mensaje("¡Tiro de habilidad!", 200);
			organillo();
			serie("pinball: tiro de habilidad\n");
		}
		skill_t = 0;
		enciende_letra(k);
	} else if ((id == S_ENTRA_I || id == S_ENTRA_D) && b->vy > 0)
		enciende_letra(id == S_ENTRA_I ? 3 : 4);
	else if ((id == S_FUERA_I || id == S_FUERA_D) && b->vy > 0) {
		mensaje("¡Por fuera!", 120);
		snd_nota(ON_TRIANGULO, 220, 200, 24);
	} else if (id == S_RAMPA_ENTRA) {
		if (b->capa == CAPA_MESA && b->vy < 0) {
			b->capa = CAPA_RAMPA;
			snd_efecto(&s_rampa);
		} else if (b->capa == CAPA_RAMPA && b->vy > 0)
			b->capa = CAPA_MESA;
	} else if (id == S_RAMPA_SALE && b->capa == CAPA_RAMPA && b->vy < 0) {
		/* arriba de la montaña rusa: cae a la mesa, hacia las calles */
		b->capa = CAPA_MESA;
		b->x = 118;
		b->y = 214;
		b->vx = 240;
		b->vy = 30;
		rampas++;
		rampas_bola++;
		suma(10000L * MIN(rampas, 5));
		campanilla(84);
		mensaje("¡Montaña rusa!", 150);
		serie("pinball: montaña rusa (%d)\n", rampas);
		if (mision == 1 && ++rampas_mision >= 3)
			mision_cumplida();
	} else if (id == S_LANZADOR && b->vy < 0) {
		if (!ya_salvada && salva_t == 0)
			salva_t = 1000;
		skill_t = 350;
		if (estado == E_LANZA)
			estado = E_JUEGA;
	}
}

static void premio_tombola(void)
{
	static const char *const p[5] = { "Premio de la tómbola: 25.000", "Premio de la tómbola: bonus x2", "Premio de la tómbola: una letra",
		"Premio de la tómbola: 50.000", "Premio de la tómbola: 10 s de «otra vez»" };
	int k = azar_n(5), i;

	suma(15000L * ronda);
	if (extra_luz) {
		extra_luz = 0;
		bolas_extra++;
		mensaje("¡BOLA EXTRA!", 300);
		organillo();
		serie("pinball: bola extra\n");
	} else {
		mensaje(p[k], 220);
		switch (k) {
		case 0: suma(25000); break;
		case 1: multi = MIN(5, multi + 1); break;
		case 2:
			for (i = 0; i < 5; i++)
				if (!letra[i]) {
					enciende_letra(i);
					break;
				}
			break;
		case 3: suma(50000); break;
		case 4: salva_t = 1000; ya_salvada = 0; break;
		}
	}
	serie("pinball: tómbola (%s)\n", msg);
	if (mision == 4)
		mision_cumplida();
}

static void nueva_bola(void)
{
	int k;

	falta = 0;
	temblor = 0;
	aviso_temblor = 0;
	multi = 1;
	salva_t = 0;
	ya_salvada = 0;
	letras_bola = rampas_bola = dianas_bola = tazas_bola = 0;
	for (k = 0; k < 2; k++)
		M.flips[k].activo = 1;
	skill_carril = azar_n(3);
	estado = E_LANZA;
	bola_al_lanzador(0);
	pon_musica();
	serie("pinball: bola %d al lanzador\n", bola_num);
}

static void nueva_partida(void)
{
	int i;
	struct record r[1];

	memset(M.bolas, 0, sizeof M.bolas);
	puntos = 0;
	bola_num = 1;
	bolas_extra = 0;
	record_pos = -1;
	memset(letra, 0, sizeof letra);
	rampas = rampas_mision = tazas_mision = mision = 0;
	ronda = 1;
	gran_feria = 0;
	extra_luz = extra_dada = 0;
	retenida = -1;
	t_tombola = 0;
	for (i = 0; i < 3; i++) {
		dianas[i] = 0;
		if (seg_diana[i])
			seg_diana[i]->activo = 1;
	}
	record_alto = rec_tabla("Puntos", r, 1) ? r[0].puntos : 0;
	golpes_flipper_dichos = 0;
	mensaje("¡Bienvenidos a la feria!", 200);
	serie("pinball: partida nueva\n");
	nueva_bola();
	pinta_todo();
}

static void bola_perdida(void)
{
	snd_efecto(&s_pierde);
	snd_musica_parar();
	gran_feria = 0;
	bonus_bola = falta ? 0 : ((long)letras_bola * 1000 + (long)rampas_bola * 5000 + (long)dianas_bola * 2500 + (long)tazas_bola * 200) * multi;
	puntos += bonus_bola;
	{
		char t[64], p[20];

		con_puntos(p, bonus_bola);
		snprintf(t, sizeof t, falta ? "Falta: sin bonus" : "Bonus: %s (x%d)", p, multi);
		mensaje(t, 250);
	}
	serie("pinball: bola perdida; bonus %ld; %ld puntos\n", bonus_bola, puntos);
	estado = E_PERDIDA;
	t_bonus = 260;
}

static void siguiente_bola(void)
{
	if (bolas_extra > 0) {
		bolas_extra--;
		mensaje("¡Bola extra! Otra vez la misma", 200);
	} else
		bola_num++;
	if (bola_num > 3) {
		estado = E_FIN;
		record_pos = rec_meter("Puntos", REC_N, (int)puntos, 0);
		serie("pinball: fin de la partida: %ld puntos; récord %d\n", puntos, record_pos + 1);
		mensaje(record_pos == 0 ? "¡Récord de la feria!" : record_pos > 0 ? "¡Entra en los mejores!" : "Fin de la partida", 400);
		snd_musica_parar();
		return;
	}
	nueva_bola();
}

static void empuja(float vx, float vy)
{
	f_empuja(&M, vx, vy);
	temblor += 1.0f;
	snd_nota(ON_RUIDO, 300, 60, 22);
	if (temblor > 4.2f && !falta) {
		int k;

		falta = 1;
		for (k = 0; k < 2; k++)
			M.flips[k].activo = 0;
		mensaje("FALTA", 400);
		snd_efecto(&s_falta);
		snd_musica_parar();
		serie("pinball: falta (la mesa se ha empujado demasiado)\n");
	} else if (temblor > 2.6f && !aviso_temblor) {
		aviso_temblor = 1;
		mensaje("¡Cuidado! No empuje tanto", 150);
	}
}

static int raton_izq, raton_der;

static void pulso(void)
{
	int k, i, izq, der, lanzar;

	tic++;
	if (msg_t > 0)
		msg_t--;
	noria_ang += 0.004f;
	if (!listo)
		return;
	if (estado == E_PAUSA) {
		jg_cambio = 1;
		return;
	}
	luces();
	jg_cambio = 1;
	if (estado == E_ESPERA || estado == E_FIN)
		return;
	if (estado == E_PERDIDA) {
		if (--t_bonus <= 0)
			siguiente_bola();
		return;
	}
	/* los flippers */
	izq = tecla(K_Z) || tecla(K_MAYUS) || tecla(K_IZQ) || tecla(K_CTRL) || raton_izq;
	der = tecla(K_MENOS) || tecla(K_MAYUS_D) || tecla(K_DER) || tecla(K_CTRL_D) || raton_der;
	if (solo) {
		for (k = 0; k < 2; k++) {
			if (solo_t[k] > 0)
				solo_t[k]--;
			for (i = 0; i < F_MAX_BOLAS; i++) {
				struct f_bola *b = &M.bolas[i];

				if (b->viva && b->capa == CAPA_MESA && b->y > 850 && b->y < 915 && b->vy > -60 &&
				    (k ? (b->x > 238 && b->x < 330) : (b->x > 130 && b->x < 222)) && solo_t[k] == 0)
					solo_t[k] = 16;
			}
		}
		izq |= solo_t[0] > 4;
		der |= solo_t[1] > 4;
	}
	for (k = 0; k < 2; k++) {
		int p = k ? der : izq;

		M.flips[k].pulsado = p;
		if (p && !pulsado_antes[k] && !falta) {
			snd_efecto(&s_flip);
			snd_efecto(&s_flip2);
			/* el cambio de calle: las letras de arriba (y la del tiro de habilidad) se corren */
			if (estado == E_LANZA)
				skill_carril = (skill_carril + (k ? 1 : 2)) % 3;
			else {
				int f = letra[0], e = letra[1], r = letra[2];

				if (k) {
					letra[0] = r; letra[1] = f; letra[2] = e;
				} else {
					letra[0] = e; letra[1] = r; letra[2] = f;
				}
			}
		}
		pulsado_antes[k] = p;
	}
	/* el lanzador: ¿hay una bola en el émbolo? */
	en_lanzador = -1;
	for (i = 0; i < F_MAX_BOLAS; i++)
		if (M.bolas[i].viva && M.bolas[i].x > 452 && M.bolas[i].y > 930)
			en_lanzador = i;
	lanzar = tecla(K_ESPACIO) || tecla(K_ABAJO) || tecla(K_INTRO);
	if (en_lanzador >= 0 && (lanzar_auto > 0 || (solo && estado == E_LANZA))) {
		if (lanzar_auto > 0)
			lanzar_auto--;
		tiro = MIN(1.0f, tiro + 0.025f);
		lanzar = tiro < (solo ? 0.55f + 0.02f * (tic % 10) : 0.7f);
		if (!lanzar)
			lanzar_auto = 0;
	}
	if (lanzar && en_lanzador >= 0) {
		tiro = MIN(1.0f, tiro + 0.012f);
	} else if (tiro > 0) {
		if (en_lanzador >= 0 && tiro > 0.04f) {
			M.bolas[en_lanzador].vy = -(950.0f + 1850.0f * tiro);
			snd_efecto(&s_muelle);
			serie("pinball: bola lanzada (fuerza %d %%)\n", (int)(tiro * 100));
		}
		tiro = 0;
	}
	/* empujar la mesa */
	if (estado == E_JUEGA) {
		if (tecla_vino(K_X))
			empuja(70, -25);
		if (tecla_vino(K_PUNTO))
			empuja(-70, -25);
		if (tecla_vino(K_ARRIBA) || tecla_vino(K_NUM_ARRIBA))
			empuja(0, -95);
	}
	temblor = MAX(0.0f, temblor - 0.006f);
	if (temblor < 1.0f)
		aviso_temblor = 0;
	/* la física: cuatro pasos de 1/400 s */
	for (k = 0; k < 4; k++)
		f_paso(&M, 1.0f / 400);
	/* la tómbola, las bolas perdidas */
	for (i = 0; i < F_MAX_BOLAS; i++) {
		struct f_bola *b = &M.bolas[i];
		float dx, dy;

		if (!b->viva)
			continue;
		dx = b->x - TOMBOLA_X;
		dy = b->y - TOMBOLA_Y;
		if (retenida < 0 && b->capa == CAPA_MESA && dx * dx + dy * dy < 9 * 9 && b->vx * b->vx + b->vy * b->vy < 520 * 520) {
			retenida = i;
			b->viva = 0;
			b->x = TOMBOLA_X;
			b->y = TOMBOLA_Y;
			t_tombola = 160;
			snd_efecto(&s_tombola);
			premio_tombola();
		}
		/* la búsqueda de la bola: si se queda quieta 3 s en la mesa (fuera del lanzador), un empujoncito */
		if (b->vx * b->vx + b->vy * b->vy < 25 * 25 && !(b->x > 452 && b->y > 900) && !(b->y > 840 && b->x > 110 && b->x < 350)) {
			/* (no en el lanzador, ni parada en un flipper, que es cosa del jugador) */
			if (++quieta[i] > 300) {
				quieta[i] = 0;
				b->vx = LIMITA((230.0f - b->x) * 1.5f, -300.0f, 300.0f) + (float)(azar_n(60) - 30);
				b->vy = -320;
				mensaje("Buscando la bola...", 120);
				serie("pinball: la bola estaba quieta en (%d, %d): un empujoncito\n", (int)b->x, (int)b->y);
			}
		} else
			quieta[i] = 0;
		if (b->y > MESA_LARGO + 14) {
			b->viva = 0;
			if (salva_t > 0 && !ya_salvada && !falta) {
				ya_salvada = 1;
				salva_t = 0;
				mensaje("¡Otra vez!", 150);
				campanilla(88);
				bola_al_lanzador(1);
				serie("pinball: ¡otra vez! (la bola vuelve)\n");
			} else if (bolas_vivas() == 0)
				bola_perdida();
			else if (bolas_vivas() == 1 && gran_feria) {
				gran_feria = 0;
				mensaje("Se acabó la gran feria", 150);
			}
		}
	}
	if (retenida >= 0 && --t_tombola <= 0) {
		struct f_bola *b = &M.bolas[retenida];

		b->viva = 1;
		b->capa = CAPA_MESA;
		b->x = TOMBOLA_X - 4;
		b->y = TOMBOLA_Y - 14;
		b->vx = -110.0f - (float)azar_n(80);
		b->vy = -720;
		retenida = -1;
		snd_nota(ON_RUIDO, 900, 90, 30);
	}
	if (t_dianas > 0 && --t_dianas == 0)
		for (i = 0; i < 3; i++) {
			dianas[i] = 0;
			if (seg_diana[i])
				seg_diana[i]->activo = 1;
		}
	for (i = 0; i < 3; i++)
		if (t_tazas[i] > 0)
			t_tazas[i]--;
	if (salva_t > 0)
		salva_t--;
	if (skill_t > 0)
		skill_t--;
}

/* ================================================================ los menús y los sucesos */
static char op_partida[300];
static const char *menus_op[2];

static void pon_menus(void)
{
	snprintf(op_partida, sizeof op_partida, "Partida nueva   F2|Pausa   P|-|%sMúsica   M|%sSonidos|Pantalla completa   F11|-|Mejores puntuaciones...|-|Salir",
		 musica ? "» " : "", snd_mudo ? "" : "» ");
	menus_op[0] = op_partida;
	menus_op[1] = "Cómo se juega...   F1|Las reglas de la feria...|-|Acerca de Hispa-Pinball...";
	jg_menus("Partida|Ayuda", menus_op);
}

static void ayuda(void)
{
	jg_aviso("Cómo se juega",
		 "Flipper izquierdo: Z (o Mayús izquierda, Control izquierdo, flecha izquierda o el botón izquierdo del ratón). "
		 "Flipper derecho: la tecla «-» (o Mayús derecha, Control derecho, flecha derecha o el botón derecho).\n\n"
		 "Lanzar: mantenga Espacio (o flecha abajo, o Intro) y suelte: cuanto más tiempo, más fuerte.\n\n"
		 "Empujar la mesa: X, el punto y la flecha arriba. Con cuidado: si se empuja mucho, FALTA y se apagan los flippers.\n\n"
		 "P: pausa. Esc: el menú. M: música. F2: otra partida. F11: ventana o pantalla completa.");
}

static void reglas(void)
{
	jg_aviso("Las reglas de la feria",
		 "Tres bolas. Las calles de arriba (F, E, R) y los pasillos de los flippers (I, A) encienden FERIA: con la palabra entera, el bonus sube (hasta x5). "
		 "Los flippers corren las letras de arriba.\n\n"
		 "Cinco misiones, una tras otra (se ven en el marcador): FERIA, la montaña rusa tres veces, el tiro al blanco (las tres dianas), "
		 "20 golpes en las tazas y la tómbola. Con las cinco, la ¡GRAN FERIA!: dos bolas y los puntos al doble.\n\n"
		 "La tómbola da un premio; tras el tiro al blanco, da la bola extra. Al lanzar, la calle que parpadea vale el tiro de habilidad. "
		 "Los primeros segundos de cada bola, si se cuela, vuelve («Otra vez»). Al perder la bola, el bonus: letras, rampas, dianas y tazas, por el multiplicador.");
}

static void pausa(int si)
{
	if (si && (estado == E_JUEGA || estado == E_LANZA)) {
		antes_pausa = estado;
		estado = E_PAUSA;
		snd_musica_parar();
	} else if (!si && estado == E_PAUSA) {
		estado = antes_pausa;
		teclas_olvidar();
		pon_musica();
		pinta_todo();
	}
	jg_cambio = 1;
}

static void elegido(int m, int o)
{
	if (m == 1) {
		if (o == 0)
			ayuda();
		else if (o == 1)
			reglas();
		else if (o == 3)
			jg_aviso("Acerca de Hispa-Pinball", "Hispa-Pinball 1.0: «La Feria», para Hispa-Pentium.\n"
				 "De la familia de Hispa-DOS: la mesa, el tema, los dibujos, la música y los sonidos, propios.\n\n"
				 "Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.");
		pinta_todo();
		return;
	}
	switch (o) {
	case 0: nueva_partida(); break;
	case 1: pausa(estado != E_PAUSA); break;
	case 3: musica = !musica; pon_musica(); rec_poner_entero("Musica", musica); rec_guardar(); pon_menus(); break;
	case 4: snd_mudo = !snd_mudo; rec_poner_entero("Sonidos", !snd_mudo); rec_guardar(); pon_menus(); break;
	case 5: jg_pantalla_completa(1); break;
	case 7: rec_ver("Mejores puntuaciones de la feria", "Puntos", REC_N, "puntos", 0, record_pos); pinta_todo(); break;
	case 9: jg_salir = 1; break;
	}
}

static void estado_serie(void)
{
	struct f_bola *b = &M.bolas[0];
	int i, k = -1;

	for (i = 0; i < F_MAX_BOLAS; i++)
		if (M.bolas[i].viva) {
			k = i;
			break;
		}
	if (k >= 0)
		b = &M.bolas[k];
	serie("pinball: estado=%s puntos=%ld bola=%d bolas=%d x=%d y=%d v=%d capa=%d mision=%d letras=%c%c%c%c%c multi=%d rampas=%d tazas=%d dianas=%d%d%d falta=%d sonido=%d subpasos=%ld\n",
	      nombre_estado[estado], puntos, bola_num, bolas_vivas(), k >= 0 ? (int)b->x : -1, k >= 0 ? (int)b->y : -1,
	      k >= 0 ? (int)m_raiz(b->vx * b->vx + b->vy * b->vy) : 0, k >= 0 ? b->capa : 0, mision + 1,
	      letra[0] ? 'F' : '-', letra[1] ? 'E' : '-', letra[2] ? 'R' : '-', letra[3] ? 'I' : '-', letra[4] ? 'A' : '-',
	      multi, rampas, tazas_bola, dianas[0], dianas[1], dianas[2], falta, snd_hay, M.subpasos);
}

static void suceso(const struct suceso *s)
{
	int m, o;

	switch (s->clase) {
	case EV_TECLA:
		if (s->a == 0x158) {
			estado_serie();
			return;
		}
		if (estado == E_PAUSA) {
			switch (s->a) {
			case 10: case 13: case T_ESC: case 'p': case 'P': pausa(0); return;
			case T_F2: estado = E_JUEGA; nueva_partida(); return;
			case 'm': case 'M': musica = !musica; rec_poner_entero("Musica", musica); rec_guardar(); pon_menus(); return;
			case 'r': case 'R': rec_ver("Mejores puntuaciones de la feria", "Puntos", REC_N, "puntos", 0, record_pos); pinta_todo(); return;
			case 's': case 'S': jg_salir = 1; return;
			}
			return;
		}
		switch (s->a) {
		case T_F2: nueva_partida(); return;
		case 0x13B: ayuda(); pinta_todo(); return;
		case T_ESC: case 'p': case 'P':
			if (estado == E_ESPERA || estado == E_FIN) {
				if (s->a == T_ESC && jg_completa)
					jg_pantalla_completa(0);
			} else
				pausa(1);
			return;
		case 'm': case 'M': musica = !musica; pon_musica(); rec_poner_entero("Musica", musica); rec_guardar(); pon_menus(); return;
		case 10: case 13:
			if (estado == E_ESPERA || estado == E_FIN)
				nueva_partida();
			return;
		}
		return;
	case EV_FOCO:
		if (!s->a)
			pausa(1);
		return;
	case EV_RATON_BAJA:
		if (jg_clic_barra((int)(int32_t)s->a, (int)(int32_t)s->b))
			return;
		if (estado == E_ESPERA || estado == E_FIN) {
			nueva_partida();
			return;
		}
		if (estado == E_PAUSA) {
			pausa(0);
			return;
		}
		if (s->c & BOTON_DERECHO)
			raton_der = 1;
		else
			raton_izq = 1;
		return;
	case EV_RATON_SUBE:
		if (!(s->c & BOTON_DERECHO))
			raton_der = 0;
		if (!(s->c & BOTON_IZQUIERDO))
			raton_izq = 0;
		return;
	case EV_MENU:
		o = jg_menu_elegido(s, &m);
		if (o >= 0)
			elegido(m, o);
		return;
	}
}

static void tamano(int an, int al)
{
	(void)an;
	(void)al;
	lz_rect(&LZ, 0, 0, LZ.an, LZ.al, 0x0A0810);
	lz_texto_centro(&LZ, LZ.an / 2, LZ.al / 2 - 8, "Preparando la mesa...", 0xFFD24A, 2, 1);
	jg_todo_sucio();
	jg_subir();
	{
		u64 t = rdtsc();

		construye();
		serie("pinball: la mesa, hecha en %u millones de ciclos\n", (u32)((rdtsc() - t) / 1000000));
	}
	pinta_todo();
}

int principal(void)
{
	static const struct juego j = { pulso, pinta, suceso, tamano, 0, 60 };
	int an, al, i;
	u32 n;

	n = sc(SC_ARGUMENTOS, (u32)jg_args, sizeof jg_args - 1, 0);
	if (ES_ERROR(n))
		n = 0;
	jg_args[MIN(n, sizeof jg_args - 1)] = 0;
	for (i = 0; jg_args[i]; i++)
		if (jg_args[i] >= 'a' && jg_args[i] <= 'z')
			jg_args[i] = (char)(jg_args[i] - 32);
	jg_tamano_maximo(&an, &al);
	if (jg_abrir("Hispa-Pinball", an, al, arg_hay("/VENTANA") ? 0 : JV_COMPLETA)) {
		static const char m[] = "pinball: no hay escritorio (o no hay memoria para la ventana)\r\n";

		sc(SC_ESCRIBIR, (u32)m, sizeof m - 1, 0);
		return 1;
	}
	azar_semilla((u32)arg_entero("/S=", (int)(jg_tics() * 2654435761u ^ (u32)rdtsc())));
	solo = arg_hay("/SOLO");
	rec_abrir("PINBALL.REC");
	musica = rec_entero("Musica", 1);
	snd_mudo = !rec_entero("Sonidos", 1);
	mesa_construye(&M);
	M.choque = choque;
	M.cruce = cruce;
	for (i = 0; i < M.n_segs; i++)
		if (M.segs[i].tipo == F_DIANA && M.segs[i].id >= ID_DIANA0 && M.segs[i].id <= ID_DIANA2)
			seg_diana[M.segs[i].id - ID_DIANA0] = &M.segs[i];
	snd_abrir();
	pon_menus();
	tamano(LZ.an, LZ.al);
	{
		struct record r[1];

		record_alto = rec_tabla("Puntos", r, 1) ? r[0].puntos : 0;
	}
	serie("pinball: listo (%s); récords en %s\n", jg_completa ? "pantalla completa" : "ventana", rec_ruta());
	if (solo)
		nueva_partida();
	return jg_correr(&j);
}
