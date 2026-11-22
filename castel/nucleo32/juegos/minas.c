/* minas.c - (JUEGOS) Campo de minas, el de Hispa-386 (juegos/minas.asm) en
 * Hispa-Pentium: tableros Pequeño (9 x 9, 10 minas), Mediano (16 x 16, 40),
 * Grande (30 x 16, 99) y Personalizado (hasta 30 x 24). La primera losa
 * nunca tiene mina (ni las de alrededor, si caben). Clic: levantar; botón
 * derecho, Mayús+clic o B: banderín; dos clics en un número (o un clic, si
 * ya tiene sus banderines alrededor): levantar las de alrededor. Teclado:
 * flechas, Intro o Espacio, B y F2. Arriba, los banderines que faltan, el
 * tiempo y el rombo (añil mientras se juega, verde con una marca al ganar,
 * granate con un aspa al perder), que con un clic empieza otra partida.
 * Ahora la ventana se estira (las losas crecen), suena, y los mejores
 * tiempos de cada tablero (los diez mejores) van a la carpeta del usuario.
 * Para las pruebas: /S=n, /P (un tablero de 9 x 9 con dos minas en dos
 * esquinas, que se guarda aparte) y F12 (el estado).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "juego.h"

#define MAX_C 30
#define MAX_F 24
enum { E_LISTO, E_JUEGA, E_GANA, E_PIERDE };
enum { T_PEQUENO, T_MEDIANO, T_GRANDE, T_PERSONAL, T_PRUEBA };
static const char *const clave_t[5] = { "Pequeño", "Mediano", "Grande", "Personal", "Prueba" };
static const char *const nombre_t[5] = { "Pequeño (9 x 9, 10 minas)", "Mediano (16 x 16, 40 minas)", "Grande (30 x 16, 99 minas)", "Personalizado", "Prueba" };

static u8 mina[MAX_F][MAX_C], vista[MAX_F][MAX_C], bandera[MAX_F][MAX_C], cuenta[MAX_F][MAX_C];
static int cols = 9, filas = 9, minas = 10, tablero, estado = E_LISTO, puestas, banderas, levantadas, prueba;
static int per_c = 20, per_f = 12, per_m = 40;
static int cur_x, cur_y, ver_cursor, record_pos = -1, exploto_x = -1, exploto_y = -1;
static u32 t0;
static int segundos;
static int cel = 24, ox, oy, arriba, cab_al = 44;

static const struct efecto s_levanta = { ON_CUADRADA, 1400, 1400, 10, 12, 1, 8 };
static const struct efecto s_bandera = { ON_TRIANGULO, 660, 990, 60, 26, 2, 50 };
static const struct efecto s_boom = { ON_RUIDO, 1200, 80, 900, 50, 2, 800 };

static void pinta_todo(void);
static void pinta_losa(int x, int y);

static void pon_tablero(int t)
{
	static const int c[3] = { 9, 16, 30 }, f[3] = { 9, 16, 16 }, m[3] = { 10, 40, 99 };

	tablero = t;
	if (t < 3) {
		cols = c[t];
		filas = f[t];
		minas = m[t];
	} else if (t == T_PERSONAL) {
		cols = per_c;
		filas = per_f;
		minas = per_m;
	} else {
		cols = filas = 9;
		minas = 2;
	}
}

static void nueva_partida(void)
{
	memset(mina, 0, sizeof mina);
	memset(vista, 0, sizeof vista);
	memset(bandera, 0, sizeof bandera);
	memset(cuenta, 0, sizeof cuenta);
	puestas = 0;
	banderas = 0;
	levantadas = 0;
	segundos = 0;
	record_pos = -1;
	exploto_x = exploto_y = -1;
	estado = E_LISTO;
	cur_x = cols / 2;
	cur_y = filas / 2;
	serie("minas: partida nueva: %s, %d x %d, %d minas\n", nombre_t[tablero], cols, filas, minas);
}

/* las minas, al levantar la primera: ni en ella ni alrededor (si caben) */
static void pon_minas(int px, int py)
{
	int libres = cols * filas, cerca = 1, x, y, n;

	if (prueba) {
		mina[0][0] = mina[filas - 1][cols - 1] = 1;
		if ((px == 0 && py == 0) || (px == cols - 1 && py == filas - 1)) {
			mina[px == 0 ? 0 : filas - 1][px == 0 ? 0 : cols - 1] = 0;
			mina[0][cols - 1] = 1;
		}
	} else {
		if (libres - 9 < minas)
			cerca = 0;
		for (n = 0; n < minas;) {
			x = azar_n(cols);
			y = azar_n(filas);
			if (mina[y][x] || (x == px && y == py) || (cerca && abs(x - px) <= 1 && abs(y - py) <= 1))
				continue;
			mina[y][x] = 1;
			n++;
		}
	}
	for (y = 0; y < filas; y++)
		for (x = 0; x < cols; x++) {
			int dx, dy, k = 0;

			for (dy = -1; dy <= 1; dy++)
				for (dx = -1; dx <= 1; dx++)
					if (x + dx >= 0 && y + dy >= 0 && x + dx < cols && y + dy < filas && mina[y + dy][x + dx])
						k++;
			cuenta[y][x] = (u8)k;
		}
	puestas = 1;
	t0 = jg_tics();
	estado = E_JUEGA;
}

static void gana(void)
{
	int x, y;

	estado = E_GANA;
	for (y = 0; y < filas; y++)
		for (x = 0; x < cols; x++)
			if (mina[y][x])
				bandera[y][x] = 1;
	banderas = minas;
	record_pos = tablero == T_PERSONAL ? -1 : rec_meter(clave_t[tablero], REC_N, MAX(1, segundos), 1);
	{
		static const int n[5] = { 72, 76, 79, 84, 88 };
		int i;

		for (i = 0; i < 5; i++)
			snd_nota_tras(ON_CAMPANA, nota_hz(n[i]), 300, 26, i * 90);
	}
	serie("minas: ganada en %d s (%s); récord %d\n", segundos, nombre_t[tablero], record_pos + 1);
	pinta_todo();
}

static void levanta(int x, int y)
{
	int pila[MAX_C * MAX_F][2], n = 0;

	if (x < 0 || y < 0 || x >= cols || y >= filas || vista[y][x] || bandera[y][x])
		return;
	if (!puestas)
		pon_minas(x, y);
	if (mina[y][x]) {
		int i, k;

		estado = E_PIERDE;
		vista[y][x] = 1;
		exploto_x = x;
		exploto_y = y;
		for (i = 0; i < filas; i++)
			for (k = 0; k < cols; k++)
				if (mina[i][k])
					vista[i][k] = 1;
		snd_efecto(&s_boom);
		serie("minas: ¡mina! (%d, %d), a los %d s\n", x, y, segundos);
		pinta_todo();
		return;
	}
	pila[n][0] = x;
	pila[n][1] = y;
	n++;
	vista[y][x] = 1;
	levantadas++;
	while (n) {
		int cx, cy, dx, dy;

		n--;
		cx = pila[n][0];
		cy = pila[n][1];
		pinta_losa(cx, cy);
		if (cuenta[cy][cx])
			continue;
		for (dy = -1; dy <= 1; dy++)
			for (dx = -1; dx <= 1; dx++) {
				int nx = cx + dx, ny = cy + dy;

				if (nx < 0 || ny < 0 || nx >= cols || ny >= filas || vista[ny][nx] || bandera[ny][nx] || mina[ny][nx])
					continue;
				vista[ny][nx] = 1;
				levantadas++;
				pila[n][0] = nx;
				pila[n][1] = ny;
				n++;
			}
	}
	snd_efecto(&s_levanta);
	if (levantadas == cols * filas - minas)
		gana();
	jg_cambio = 1;
}

static void alrededor(int x, int y)
{
	int dx, dy, b = 0;

	if (!vista[y][x] || !cuenta[y][x])
		return;
	for (dy = -1; dy <= 1; dy++)
		for (dx = -1; dx <= 1; dx++)
			if (x + dx >= 0 && y + dy >= 0 && x + dx < cols && y + dy < filas && bandera[y + dy][x + dx])
				b++;
	if (b != cuenta[y][x])
		return;
	for (dy = -1; dy <= 1 && estado != E_PIERDE; dy++)
		for (dx = -1; dx <= 1 && estado != E_PIERDE; dx++)
			levanta(x + dx, y + dy);
}

static void pon_bandera(int x, int y)
{
	if (vista[y][x] || estado == E_GANA || estado == E_PIERDE)
		return;
	bandera[y][x] = !bandera[y][x];
	banderas += bandera[y][x] ? 1 : -1;
	snd_efecto(&s_bandera);
	pinta_losa(x, y);
	jg_cambio = 1;
}

/* ---- pintar ---- */
static void dibuja_mina(int px, int py, u32 fondo)
{
	float c = cel / 2.0f, r = cel * 0.26f;
	int k;

	(void)fondo;
	for (k = 0; k < 4; k++) {
		float a = k * M_PI_F / 4;

		lz_linea_gruesa(&LZ, px + (int)(c - r * 1.5f * m_cos(a)), py + (int)(c - r * 1.5f * m_sen(a)), px + (int)(c + r * 1.5f * m_cos(a)),
				py + (int)(c + r * 1.5f * m_sen(a)), MAX(2, cel / 10), 0x1A1410);
	}
	lz_circulo_suave(&LZ, px + c, py + c, r, 0x1A1410);
	lz_circulo_suave(&LZ, px + c - r * 0.35f, py + c - r * 0.35f, r * 0.3f, 0xFFFFFF);
}

static void dibuja_bandera(int px, int py)
{
	float q[6] = { px + cel * 0.38f, py + cel * 0.18f, px + cel * 0.78f, py + cel * 0.34f, px + cel * 0.38f, py + cel * 0.50f };

	lz_rect(&LZ, px + (int)(cel * 0.34f), py + (int)(cel * 0.18f), MAX(2, cel / 10), (int)(cel * 0.6f), 0x1A1410);
	lz_rect(&LZ, px + (int)(cel * 0.22f), py + (int)(cel * 0.74f), (int)(cel * 0.5f), MAX(2, cel / 9), 0x1A1410);
	lz_poligono(&LZ, q, 3, 0xC2542D);
}

static void pinta_losa(int x, int y)
{
	static const u32 col[9] = { 0, 0x2F4B8F, 0x3E7A2E, 0xB8322A, 0x4A2A7A, 0x7A2A1A, 0x1F6E7A, 0x1A1410, 0x6A6A6A };
	int px = ox + x * cel, py = oy + y * cel, b = MAX(1, cel / 10);

	if (vista[y][x]) {
		u32 f = (x == exploto_x && y == exploto_y) ? 0xE85A4A : 0xF4EEDF;

		lz_rect(&LZ, px, py, cel, cel, 0xC9BFA8);
		lz_rect(&LZ, px + 1, py + 1, cel - 1, cel - 1, f);
		if (mina[y][x])
			dibuja_mina(px, py, f);
		else if (cuenta[y][x]) {
			char t[2] = { (char)('0' + cuenta[y][x]), 0 };
			int e = cel >= 30 ? 2 : 1;

			lz_negrita(&LZ, px + (cel - 8 * e) / 2, py + (cel - 16 * e) / 2, t, col[cuenta[y][x]], e);
		}
	} else {
		lz_rect(&LZ, px, py, cel, cel, 0x9AA2B0);
		lz_degradado(&LZ, px + b, py + b, cel - 2 * b, cel - 2 * b, 0xDDE3EC, 0xC0C8D6);
		lz_rect(&LZ, px, py, cel, b, 0xF4F7FB);
		lz_rect(&LZ, px, py, b, cel, 0xF4F7FB);
		lz_rect(&LZ, px, py + cel - b, cel, b, 0x7C8494);
		lz_rect(&LZ, px + cel - b, py, b, cel, 0x7C8494);
		if (bandera[y][x]) {
			dibuja_bandera(px, py);
			if (estado == E_PIERDE && !mina[y][x]) {
				/* un banderín mal puesto: con un aspa */
				lz_linea_gruesa(&LZ, px + 3, py + 3, px + cel - 3, py + cel - 3, 2, 0xB8322A);
				lz_linea_gruesa(&LZ, px + cel - 3, py + 3, px + 3, py + cel - 3, 2, 0xB8322A);
			}
		}
	}
	if (ver_cursor && x == cur_x && y == cur_y) {
		lz_marco(&LZ, px + 1, py + 1, cel - 2, cel - 2, 0x1D4ED8);
		lz_marco(&LZ, px + 2, py + 2, cel - 4, cel - 4, 0x1D4ED8);
	}
	jg_sucio(px, py, cel, cel);
}

static int rombo_x, rombo_y, rombo_r;

static void pinta_cabecera(void)
{
	char t[40];
	int y = arriba + 6, cx = LZ.an / 2;
	float r = 15;
	u32 c = estado == E_GANA ? 0x3E8E4E : estado == E_PIERDE ? 0x9E2B2B : 0x2F4B8F;
	float q[8] = { cx, y + 1.0f, cx + r, y + 1 + r, cx, y + 1 + 2 * r, cx - r, y + 1 + r };

	lz_rect(&LZ, 0, arriba, LZ.an, cab_al, 0xE9EDF2);
	/* los banderines que faltan, a la izquierda; el tiempo, a la derecha */
	lz_redondo(&LZ, ox, y + 2, 76, 28, 4, 0x1A1410);
	snprintf(t, sizeof t, "%3d", minas - banderas);
	lz_texto(&LZ, ox + 14, y + 8, t, 0xFF7050, 2);
	lz_redondo(&LZ, ox + cols * cel - 76, y + 2, 76, 28, 4, 0x1A1410);
	snprintf(t, sizeof t, "%3d", MIN(segundos, 999));
	lz_texto(&LZ, ox + cols * cel - 62, y + 8, t, 0xFF7050, 2);
	lz_poligono(&LZ, q, 4, c);
	if (estado == E_GANA) {
		lz_linea_gruesa(&LZ, (int)(cx - 7), y + 16, (int)(cx - 2), y + 22, 3, 0xFFFFFF);
		lz_linea_gruesa(&LZ, (int)(cx - 2), y + 22, (int)(cx + 8), y + 10, 3, 0xFFFFFF);
	} else if (estado == E_PIERDE) {
		lz_linea_gruesa(&LZ, (int)(cx - 6), y + 10, (int)(cx + 6), y + 22, 3, 0xFFFFFF);
		lz_linea_gruesa(&LZ, (int)(cx + 6), y + 10, (int)(cx - 6), y + 22, 3, 0xFFFFFF);
	} else
		lz_circulo_suave(&LZ, cx, y + 1 + r, 4, 0xFFFFFF);
	rombo_x = cx;
	rombo_y = (int)(y + 1 + r);
	rombo_r = (int)r;
	jg_sucio(0, arriba, LZ.an, cab_al);
}

static void pinta_todo(void)
{
	int x, y;

	lz_rect(&LZ, 0, 0, LZ.an, LZ.al, 0xE9EDF2);
	if (!jg_completa)
		jg_pinta_barra();
	lz_rect(&LZ, ox - 3, oy - 3, cols * cel + 6, filas * cel + 6, 0x6B7280);
	for (y = 0; y < filas; y++)
		for (x = 0; x < cols; x++)
			pinta_losa(x, y);
	pinta_cabecera();
	jg_todo_sucio();
}

static void pinta(void)
{
	static int s_antes = -1, e_antes = -1, b_antes = -1;

	if (s_antes != segundos || e_antes != estado || b_antes != banderas) {
		s_antes = segundos;
		e_antes = estado;
		b_antes = banderas;
		pinta_cabecera();
	}
}

static void tamano(int an, int al)
{
	arriba = jg_completa ? 0 : ALTO_BARRA;
	cel = MIN((an - 16) / cols, (al - arriba - cab_al - 12) / filas);
	cel = LIMITA(cel, 10, 56);
	ox = (an - cols * cel) / 2;
	oy = arriba + cab_al + (al - arriba - cab_al - filas * cel) / 2;
	pinta_todo();
}

static void cambia_tablero(int t)
{
	int an, al, man, mal;

	pon_tablero(t);
	rec_poner("Tablero", clave_t[t]);
	rec_guardar();
	nueva_partida();
	/* la ventana, a la medida del tablero */
	jg_tamano_maximo(&man, &mal);
	an = MIN(man, cols * 28 + 24);
	al = MIN(mal, filas * 28 + cab_al + 40);
	(void)an;
	(void)al;
	tamano(LZ.an, LZ.al);
}

static void pulso(void)
{
	if (estado == E_JUEGA) {
		int s = (int)((jg_tics() - t0) / jg_hz);

		if (s != segundos) {
			segundos = s;
			jg_cambio = 1;
		}
	}
}

/* ---- los menús y los sucesos ---- */
static char op_juego[400];
static const char *menus_op[2];

static void pon_menus(void)
{
	snprintf(op_juego, sizeof op_juego, "Partida nueva   F2|-|%sPequeño (9 x 9, 10 minas)|%sMediano (16 x 16, 40 minas)|%sGrande (30 x 16, 99 minas)|%sPersonalizado...|-|%sSonidos|-|Mejores tiempos...|-|Salir",
		 tablero == T_PEQUENO ? "» " : "", tablero == T_MEDIANO ? "» " : "", tablero == T_GRANDE ? "» " : "", tablero == T_PERSONAL ? "» " : "", snd_mudo ? "" : "» ");
	menus_op[0] = op_juego;
	menus_op[1] = "Cómo se juega...   F1|-|Acerca de Campo de minas...";
	jg_menus("Juego|Ayuda", menus_op);
}

static void personalizado(void)
{
	static const char *const an[] = { "9 de ancho", "12 de ancho", "16 de ancho", "20 de ancho", "24 de ancho", "30 de ancho" };
	static const char *const al[] = { "9 de alto", "12 de alto", "16 de alto", "20 de alto", "24 de alto" };
	static const char *const mi[] = { "Pocas minas (10 %)", "Normal (15 %)", "Muchas (20 %)", "Muchísimas (25 %)" };
	static const int va[] = { 9, 12, 16, 20, 24, 30 }, vl[] = { 9, 12, 16, 20, 24 }, vm[] = { 10, 15, 20, 25 };
	int a = jg_elige("Personalizado: el ancho", an, 6, 3), b, c;

	if (a < 0)
		return;
	b = jg_elige("Personalizado: el alto", al, 5, 1);
	if (b < 0)
		return;
	c = jg_elige("Personalizado: las minas", mi, 4, 1);
	if (c < 0)
		return;
	per_c = va[a];
	per_f = vl[b];
	per_m = MAX(1, per_c * per_f * vm[c] / 100);
	rec_poner_entero("Ancho", per_c);
	rec_poner_entero("Alto", per_f);
	rec_poner_entero("Minas", per_m);
	cambia_tablero(T_PERSONAL);
}

static void ayuda(void)
{
	jg_aviso("Cómo se juega", "Levante todas las losas que no tienen mina. Cada número dice cuántas minas hay en las ocho de alrededor.\n\n"
		 "Clic: levantar. Botón derecho, Mayús+clic o B: poner o quitar un banderín. Dos clics en un número (o uno, si ya tiene sus banderines): levantar las de alrededor.\n\n"
		 "Teclado: las flechas, Intro o Espacio y B. F2 o un clic en el rombo: otra partida. La primera losa nunca tiene mina.");
}

static void elegido(int m, int o)
{
	if (m == 1) {
		if (o == 0)
			ayuda();
		else
			jg_aviso("Acerca de Campo de minas", "Campo de minas 1.0, para Hispa-Pentium (el de Hispa-386).\n\nCopyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.");
		return;
	}
	switch (o) {
	case 0: nueva_partida(); pinta_todo(); break;
	case 2: case 3: case 4: cambia_tablero(o - 2); break;
	case 5: personalizado(); break;
	case 7: snd_mudo = !snd_mudo; rec_poner_entero("Sonidos", !snd_mudo); rec_guardar(); break;
	case 9: {
		static const char *const t[3] = { "Pequeño", "Mediano", "Grande" };
		int r = jg_elige("Mejores tiempos", t, 3, tablero < 3 ? tablero : 0);
		char tit[60];

		if (r >= 0) {
			snprintf(tit, sizeof tit, "Mejores tiempos: %s", nombre_t[r]);
			rec_ver(tit, clave_t[r], REC_N, "s", 1, r == tablero ? record_pos : -1);
		}
		break;
	}
	case 11: jg_salir = 1; break;
	}
	pon_menus();
}

static void suceso(const struct suceso *s)
{
	int m, o;

	switch (s->clase) {
	case EV_TECLA:
		if (s->a == 0x158) {
			serie("minas: estado=%d tablero=%s %dx%d minas=%d banderas=%d levantadas=%d segundos=%d sonido=%d\n", estado, clave_t[tablero], cols, filas, minas, banderas, levantadas,
			      segundos, snd_hay);
			return;
		}
		if (s->a == T_F2) {
			nueva_partida();
			pinta_todo();
			return;
		}
		if (s->a == 0x13B) {
			ayuda();
			return;
		}
		if (estado == E_GANA || estado == E_PIERDE)
			return;
		{
			int ax = cur_x, ay = cur_y;

			ver_cursor = 1;
			switch (s->a) {
			case T_IZQUIERDA: cur_x = (cur_x + cols - 1) % cols; break;
			case T_DERECHA: cur_x = (cur_x + 1) % cols; break;
			case T_ARRIBA: cur_y = (cur_y + filas - 1) % filas; break;
			case T_ABAJO: cur_y = (cur_y + 1) % filas; break;
			case 10: case 13: case ' ':
				if (vista[cur_y][cur_x])
					alrededor(cur_x, cur_y);
				else
					levanta(cur_x, cur_y);
				break;
			case 'b': case 'B': pon_bandera(cur_x, cur_y); break;
			}
			if (estado == E_GANA || estado == E_PIERDE)
				return;
			pinta_losa(ax, ay);
			pinta_losa(cur_x, cur_y);
		}
		return;
	case EV_RATON_BAJA: {
		int x = (int)(int32_t)s->a, y = (int)(int32_t)s->b, cx, cy;

		if (jg_clic_barra(x, y))
			return;
		if (abs(x - rombo_x) + abs(y - rombo_y) <= rombo_r) {
			nueva_partida();
			pinta_todo();
			return;
		}
		if (x < ox || y < oy || estado == E_GANA || estado == E_PIERDE)
			return;
		cx = (x - ox) / cel;
		cy = (y - oy) / cel;
		if (cx >= cols || cy >= filas)
			return;
		if (ver_cursor) {
			ver_cursor = 0;
			pinta_losa(cur_x, cur_y);
		}
		if ((s->c & BOTON_DERECHO) || (s->c & CON_MAYUS))
			pon_bandera(cx, cy);
		else if (vista[cy][cx])
			alrededor(cx, cy);
		else
			levanta(cx, cy);
		return;
	}
	case EV_MENU:
		o = jg_menu_elegido(s, &m);
		if (o >= 0)
			elegido(m, o);
		return;
	}
}

int principal(void)
{
	static const struct juego j = { pulso, pinta, suceso, tamano, 0, 30 };
	int an, al, t;
	u32 n, i;
	const char *g;

	n = sc(SC_ARGUMENTOS, (u32)jg_args, sizeof jg_args - 1, 0);
	if (ES_ERROR(n))
		n = 0;
	jg_args[MIN(n, sizeof jg_args - 1)] = 0;
	for (i = 0; jg_args[i]; i++)
		if (jg_args[i] >= 'a' && jg_args[i] <= 'z')
			jg_args[i] = (char)(jg_args[i] - 32);
	jg_tamano_maximo(&an, &al);
	if (jg_abrir("Campo de minas", MIN(an, 16 * 26 + 24), MIN(al, 16 * 26 + 44 + 40), 0))
		return 1;
	azar_semilla((u32)arg_entero("/S=", (int)(jg_tics() * 2654435761u ^ (u32)rdtsc())));
	rec_abrir("MINAS.REC");
	prueba = arg_hay("/P");
	per_c = LIMITA(rec_entero("Ancho", 20), 9, MAX_C);
	per_f = LIMITA(rec_entero("Alto", 12), 9, MAX_F);
	per_m = LIMITA(rec_entero("Minas", 40), 1, per_c * per_f - 10);
	g = rec_texto("Tablero");
	t = T_PEQUENO;
	for (i = 0; i < 4; i++)
		if (g && !strcmp(g, clave_t[i]))
			t = (int)i;
	snd_mudo = !rec_entero("Sonidos", 1);
	pon_tablero(prueba ? T_PRUEBA : t);
	snd_abrir();
	pon_menus();
	nueva_partida();
	tamano(LZ.an, LZ.al);
	serie("minas: ventana %d x %d (losas de %d); récords en %s\n", LZ.an, LZ.al, cel, rec_ruta());
	return jg_correr(&j);
}
