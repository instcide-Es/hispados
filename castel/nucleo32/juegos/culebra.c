/* culebra.c - (JUEGOS) Culebra, la de Hispa-386 (juegos/culebra.asm) en
 * Hispa-Pentium: la culebra anda sola y se lleva con las flechas hasta la
 * fruta; cada fruta la alarga dos casillas y cada cinco va más deprisa (un
 * nivel). Acaba al tocar el borde, un muro o su cuerpo. Las mismas reglas,
 * el mismo tablero (28 x 18), los mismos muros y el mismo dibujo (damero
 * crema y piedra, culebra verde oliva con collar ocre y cabeza azul
 * petróleo, frutas en rombo); ahora la ventana se estira (las casillas
 * crecen), suena y los récords (los diez mejores sin muros y con muros) van
 * a la carpeta del usuario.
 * Teclas: flechas (o W A S D), P o Espacio pausa, Intro otra partida, F2,
 * F11. Para las pruebas: /S=n, /P (paso a paso: una casilla por flecha) y
 * F12 (el estado).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "juego.h"

#define COLS 28
#define FILAS 18
#define MAX_CUERPO 512
enum { E_LISTO, E_JUEGA, E_PAUSA, E_FIN };
enum { M_VACIO, M_CUERPO, M_MURO, M_FRUTA };
#define C_TINTA 0x1A1410
#define C_MARRON 0x3B2A20
#define C_PETROLEO 0x1F4E5A
#define C_TERRACOTA 0xC2542D
#define C_TERRACOTA_CL 0xE07B4F
#define C_CREMA 0xFBF3E0
#define C_OCRE 0xE8B33A
#define C_OLIVA 0x6B8E3D
#define C_PIEDRA 0xD8D2C4

static const u8 muros[][4] = { {7, 4, 5, 0}, {16, 4, 5, 0}, {7, 13, 5, 0}, {16, 13, 5, 0}, {3, 7, 4, 1}, {24, 7, 4, 1} };
static const int pasos_vel[3] = { 275, 220, 165 };	/* ms por paso (lenta, normal, rápida): los de Hispa-386 */

static u8 mapa[FILAS][COLS];
static u8 cx[MAX_CUERPO], cy[MAX_CUERPO];
static int cab, cola, largo, crecer, dir, dir_pedida[3], n_pedidas, estado = E_LISTO, nivel, comidas, con_muros, velocidad = 1, paso_a_paso;
static long puntos;
static int t_paso, record_pos = -1;
static int cel = 16, ox, oy, arriba;

static const struct efecto s_come = { ON_CAMPANA, 880, 880, 220, 30, 1, 200 };
static const struct efecto s_choca = { ON_TRIANGULO, 400, 60, 700, 36, 2, 600 };

static void pinta_todo(void);

static int ms_paso(void)
{
	int ms = pasos_vel[velocidad] - (nivel - 1) * 55;

	return ms < 55 ? 55 : ms;
}

static void fruta_nueva(void)
{
	int n = 0, k, x, y;

	for (y = 0; y < FILAS; y++)
		for (x = 0; x < COLS; x++)
			if (mapa[y][x] == M_VACIO)
				n++;
	if (!n)
		return;
	k = azar_n(n);
	for (y = 0; y < FILAS; y++)
		for (x = 0; x < COLS; x++)
			if (mapa[y][x] == M_VACIO && k-- == 0)
				mapa[y][x] = M_FRUTA;
}

static void nueva_partida(void)
{
	int i, k;

	memset(mapa, 0, sizeof mapa);
	if (con_muros)
		for (i = 0; i < (int)(sizeof muros / sizeof muros[0]); i++)
			for (k = 0; k < muros[i][2]; k++)
				mapa[muros[i][1] + (muros[i][3] ? k : 0)][muros[i][0] + (muros[i][3] ? 0 : k)] = M_MURO;
	cola = 0;
	cab = 3;
	for (i = 0; i <= 3; i++) {
		cx[i] = (u8)(10 + i);
		cy[i] = 9;
		mapa[9][10 + i] = M_CUERPO;
	}
	largo = 4;
	crecer = 0;
	dir = 0;
	n_pedidas = 0;
	nivel = 1;
	comidas = 0;
	puntos = 0;
	record_pos = -1;
	t_paso = 0;
	fruta_nueva();
	estado = E_JUEGA;
	serie("culebra: partida nueva (%s, velocidad %d)\n", con_muros ? "con muros" : "sin muros", velocidad);
	pinta_todo();
}

static void fin(void)
{
	estado = E_FIN;
	snd_efecto(&s_choca);
	record_pos = rec_meter(con_muros ? "Con" : "Sin", REC_N, (int)puntos, 0);
	serie("culebra: fin de la partida: %ld puntos, %d frutas, nivel %d; récord %d\n", puntos, comidas, nivel, record_pos + 1);
	jg_todo_sucio();
}

static void pinta_celda(int x, int y);

static void paso(void)
{
	static const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, -1, 1 };
	int nx, ny, que;

	if (n_pedidas) {
		int d = dir_pedida[0];

		memmove(dir_pedida, dir_pedida + 1, sizeof dir_pedida - sizeof dir_pedida[0]);
		n_pedidas--;
		/* (no se puede dar la vuelta sobre sí misma) */
		if ((d ^ 1) != dir)
			dir = d;
	}
	nx = cx[cab] + dx[dir];
	ny = cy[cab] + dy[dir];
	if (nx < 0 || ny < 0 || nx >= COLS || ny >= FILAS) {
		fin();
		return;
	}
	que = mapa[ny][nx];
	/* a la casilla de la cola sí se puede ir (se va a la vez), salvo si está creciendo */
	if (que == M_MURO || (que == M_CUERPO && !(nx == cx[cola] && ny == cy[cola] && !crecer))) {
		fin();
		return;
	}
	if (!crecer) {
		mapa[cy[cola]][cx[cola]] = M_VACIO;
		pinta_celda(cx[cola], cy[cola]);
		cola = (cola + 1) % MAX_CUERPO;
	} else {
		crecer--;
		largo++;
	}
	cab = (cab + 1) % MAX_CUERPO;
	cx[cab] = (u8)nx;
	cy[cab] = (u8)ny;
	mapa[ny][nx] = M_CUERPO;
	/* la cabeza nueva, el collar (la de antes) y el cuerpo (el collar de antes) */
	pinta_celda(nx, ny);
	pinta_celda(cx[(cab + MAX_CUERPO - 1) % MAX_CUERPO], cy[(cab + MAX_CUERPO - 1) % MAX_CUERPO]);
	if (largo > 2)
		pinta_celda(cx[(cab + MAX_CUERPO - 2) % MAX_CUERPO], cy[(cab + MAX_CUERPO - 2) % MAX_CUERPO]);
	if (que == M_FRUTA) {
		crecer += 2;
		puntos += 5 * (velocidad + nivel + con_muros);
		comidas++;
		snd_efecto(&s_come);
		if (comidas % 5 == 0 && ms_paso() > 55) {
			nivel++;
			snd_nota_tras(ON_TRIANGULO, nota_hz(76), 90, 26, 120);
			snd_nota_tras(ON_TRIANGULO, nota_hz(83), 140, 26, 210);
		}
		fruta_nueva();
		jg_todo_sucio();
	}
	jg_cambio = 1;
}

static void pide(int d)
{
	if (estado == E_LISTO || estado == E_FIN)
		return;
	if (estado == E_PAUSA)
		estado = E_JUEGA;
	if (n_pedidas < 3)
		dir_pedida[n_pedidas++] = d;
	if (paso_a_paso)
		paso();
}

static void pulso(void)
{
	if (estado != E_JUEGA || paso_a_paso)
		return;
	t_paso += 10;
	if (t_paso >= ms_paso()) {
		t_paso -= ms_paso();
		paso();
	}
}

/* ---- pintar ---- */
static void pinta_celda(int x, int y)
{
	int px = ox + x * cel, py = oy + y * cel, m = mapa[y][x], k;
	u32 fondo = (x + y) % 2 ? C_PIEDRA : C_CREMA;

	lz_rect(&LZ, px, py, cel, cel, fondo);
	if (m == M_MURO) {
		lz_rect(&LZ, px, py, cel, cel, C_MARRON);
		for (k = 0; k < 2; k++) {
			int yy = py + k * cel / 2;

			lz_rect(&LZ, px, yy, cel, 1, 0x5A4232);
			lz_rect(&LZ, px + (k ? cel / 2 : 0), yy, 1, cel / 2, 0x5A4232);
		}
	} else if (m == M_FRUTA) {
		float c = cel / 2.0f, q[8] = { px + c, py + 1.5f, px + cel - 1.5f, py + c, px + c, py + cel - 1.5f, px + 1.5f, py + c };
		float r[8] = { px + c, py + cel * 0.3f, px + cel * 0.7f, py + c, px + c, py + cel * 0.7f, px + cel * 0.3f, py + c };

		lz_poligono(&LZ, q, 4, C_TERRACOTA);
		lz_poligono(&LZ, r, 4, C_TERRACOTA_CL);
	} else if (m == M_CUERPO) {
		int es_cab = x == cx[cab] && y == cy[cab];
		int collar = !es_cab && x == cx[(cab + MAX_CUERPO - 1) % MAX_CUERPO] && y == cy[(cab + MAX_CUERPO - 1) % MAX_CUERPO];

		lz_redondo(&LZ, px + 1, py + 1, cel - 2, cel - 2, cel / 4, es_cab ? C_PETROLEO : collar ? C_OCRE : C_OLIVA);
		if (!es_cab && !collar)
			lz_rect(&LZ, px + cel / 3, py + cel / 3, cel / 3, cel / 3, 0x87A85A);
		if (es_cab) {
			/* los ojos, según a dónde mira */
			static const int ojos[4][4] = { {9, 2, 9, 9}, {2, 2, 2, 9}, {2, 2, 9, 2}, {2, 9, 9, 9} };
			int s = MAX(2, cel / 6);

			for (k = 0; k < 2; k++) {
				int ex = px + ojos[dir][2 * k] * cel / 12, ey = py + ojos[dir][2 * k + 1] * cel / 12;

				lz_rect(&LZ, ex, ey, s, s, 0xFFFFFF);
				lz_rect(&LZ, ex + s / 2, ey + s / 2, MAX(1, s / 2), MAX(1, s / 2), C_TINTA);
			}
		}
	}
	jg_sucio(px, py, cel, cel);
}

static void pinta_cabecera(void)
{
	char t[120];
	struct record r[1];
	int y = arriba + 6, rec = rec_tabla(con_muros ? "Con" : "Sin", r, 1) ? r[0].puntos : 0;

	lz_rect(&LZ, 0, arriba, LZ.an, oy - arriba - 2, 0xE9EDF2);
	snprintf(t, sizeof t, "Puntos: %ld    Nivel: %d    Récord: %ld    %s", puntos, nivel, MAX((long)rec, puntos), con_muros ? "Con muros" : "Sin muros");
	lz_negrita(&LZ, ox, y, t, C_TINTA, 1);
	jg_sucio(0, arriba, LZ.an, oy - arriba - 2);
}

static void pinta_todo(void)
{
	int x, y;

	lz_rect(&LZ, 0, 0, LZ.an, LZ.al, 0xE9EDF2);
	if (!jg_completa)
		jg_pinta_barra();
	lz_rect(&LZ, ox - 3, oy - 3, COLS * cel + 6, FILAS * cel + 6, C_TINTA);
	for (y = 0; y < FILAS; y++)
		for (x = 0; x < COLS; x++)
			pinta_celda(x, y);
	pinta_cabecera();
	if (estado == E_LISTO)
		jg_cartel(&LZ, ox + COLS * cel / 2 - 170, oy, 340, FILAS * cel, "Culebra", "Lleve la culebra a la fruta con las flechas.\nIntro: empezar.");
	else if (estado == E_PAUSA)
		jg_cartel(&LZ, ox + COLS * cel / 2 - 150, oy, 300, FILAS * cel, "Pausa", "Una flecha o P para seguir.");
	else if (estado == E_FIN) {
		char t[160];

		if (record_pos >= 0)
			snprintf(t, sizeof t, "%ld puntos. ¡Récord: el %d.º!\nIntro: otra partida.", puntos, record_pos + 1);
		else
			snprintf(t, sizeof t, "%ld puntos.\nIntro: otra partida.", puntos);
		jg_cartel(&LZ, ox + COLS * cel / 2 - 170, oy, 340, FILAS * cel, "Se acabó", t);
	}
	jg_todo_sucio();
}

static void pinta(void)
{
	static long p_antes = -1;
	static int e_antes = -1;

	if (e_antes != estado) {
		e_antes = estado;
		pinta_todo();
	} else if (p_antes != puntos)
		pinta_cabecera();
	p_antes = puntos;
}

static void tamano(int an, int al)
{
	arriba = jg_completa ? 0 : ALTO_BARRA;
	cel = MIN((an - 16) / COLS, (al - arriba - 34) / FILAS);
	cel = LIMITA(cel, 6, 64);
	ox = (an - COLS * cel) / 2;
	oy = arriba + 28 + (al - arriba - 28 - FILAS * cel) / 2;
	pinta_todo();
}

/* ---- los menús ---- */
static char op_juego[300];
static const char *menus_op[2];

static void pon_menus(void)
{
	snprintf(op_juego, sizeof op_juego, "Nueva partida   F2|Pausa   P|-|%sCon muros|-|%sVelocidad lenta|%sVelocidad normal|%sVelocidad rápida|-|%sSonidos|-|Mejores puntuaciones...|-|Salir",
		 con_muros ? "» " : "", velocidad == 0 ? "» " : "", velocidad == 1 ? "» " : "", velocidad == 2 ? "» " : "", snd_mudo ? "" : "» ");
	menus_op[0] = op_juego;
	menus_op[1] = "Cómo se juega...   F1|-|Acerca de Culebra...";
	jg_menus("Juego|Ayuda", menus_op);
}

static void guarda(void)
{
	rec_poner_entero("Muros", con_muros);
	rec_poner_entero("Velocidad", velocidad);
	rec_poner_entero("Sonidos", !snd_mudo);
	rec_guardar();
	pon_menus();
}

static void ayuda(void)
{
	jg_aviso("Cómo se juega", "Lleve la culebra con las flechas hasta la fruta: cada fruta la alarga dos casillas y, cada cinco, va más deprisa.\n\n"
		 "No toque el borde, los muros ni su propio cuerpo.\n\nP o Espacio: pausa. Intro: otra partida. F11: pantalla completa.");
}

static void elegido(int m, int o)
{
	if (m == 1) {
		if (o == 0)
			ayuda();
		else
			jg_aviso("Acerca de Culebra", "Culebra 1.0, para Hispa-Pentium (la de Hispa-386, con su dibujo).\n\nCopyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.");
		return;
	}
	switch (o) {
	case 0: nueva_partida(); break;
	case 1: if (estado == E_JUEGA) estado = E_PAUSA; else if (estado == E_PAUSA) estado = E_JUEGA; break;
	case 3: con_muros = !con_muros; guarda(); estado = E_LISTO; memset(mapa, 0, sizeof mapa); break;
	case 5: case 6: case 7: velocidad = o - 5; guarda(); break;
	case 9: snd_mudo = !snd_mudo; guarda(); break;
	case 11: {
		/* las dos tablas, una tras otra */
		rec_ver("Mejores puntuaciones: sin muros", "Sin", REC_N, "puntos", 0, con_muros ? -1 : record_pos);
		rec_ver("Mejores puntuaciones: con muros", "Con", REC_N, "puntos", 0, con_muros ? record_pos : -1);
		break;
	}
	case 13: jg_salir = 1; break;
	}
	pinta_todo();
}

static void suceso(const struct suceso *s)
{
	int m, o;

	switch (s->clase) {
	case EV_TECLA:
		switch (s->a) {
		case 0x158: {
			int fx = -1, fy = -1, x, y;

			for (y = 0; y < FILAS; y++)
				for (x = 0; x < COLS; x++)
					if (mapa[y][x] == M_FRUTA) {
						fx = x;
						fy = y;
					}
			serie("culebra: estado=%d puntos=%ld largo=%d frutas=%d nivel=%d cabeza=%d,%d fruta=%d,%d sonido=%d\n", estado, puntos, largo, comidas, nivel, cx[cab], cy[cab], fx, fy, snd_hay);
			return;
		}
		case T_DERECHA: case 'd': case 'D': pide(0); return;
		case T_IZQUIERDA: case 'a': case 'A': pide(1); return;
		case T_ARRIBA: case 'w': case 'W': pide(2); return;
		case T_ABAJO: case 's': case 'S': pide(3); return;
		case 'p': case 'P': case ' ':
			if (estado == E_JUEGA) estado = E_PAUSA;
			else if (estado == E_PAUSA) estado = E_JUEGA;
			jg_cambio = 1;
			return;
		case T_F2: nueva_partida(); return;
		case 0x13B: ayuda(); return;
		case 10: case 13:
			if (estado == E_LISTO || estado == E_FIN)
				nueva_partida();
			return;
		}
		return;
	case EV_FOCO:
		if (!s->a && estado == E_JUEGA) {
			estado = E_PAUSA;
			jg_cambio = 1;
		}
		return;
	case EV_RATON_BAJA:
		jg_clic_barra((int)(int32_t)s->a, (int)(int32_t)s->b);
		return;
	case EV_MENU:
		o = jg_menu_elegido(s, &m);
		if (o >= 0)
			elegido(m, o);
		return;
	}
}

int principal(void)
{
	static const struct juego j = { pulso, pinta, suceso, tamano, 0, 60 };
	int an, al;
	u32 n, i;

	jg_tamano_maximo(&an, &al);
	if (jg_abrir("Culebra", MIN(an, 28 * 20 + 24), MIN(al, 18 * 20 + 70), 0))
		return 1;
	n = sc(SC_ARGUMENTOS, (u32)jg_args, sizeof jg_args - 1, 0);
	if (ES_ERROR(n))
		n = 0;
	jg_args[MIN(n, sizeof jg_args - 1)] = 0;
	for (i = 0; jg_args[i]; i++)
		if (jg_args[i] >= 'a' && jg_args[i] <= 'z')
			jg_args[i] = (char)(jg_args[i] - 32);
	azar_semilla((u32)arg_entero("/S=", (int)(jg_tics() * 2654435761u ^ (u32)rdtsc())));
	paso_a_paso = arg_hay("/P");
	rec_abrir("CULEBRA.REC");
	con_muros = rec_entero("Muros", 0);
	velocidad = LIMITA(rec_entero("Velocidad", 1), 0, 2);
	snd_mudo = !rec_entero("Sonidos", 1);
	snd_abrir();
	pon_menus();
	tamano(LZ.an, LZ.al);
	serie("culebra: ventana %d x %d (casillas de %d); récords en %s\n", LZ.an, LZ.al, cel, rec_ruta());
	return jg_correr(&j);
}
