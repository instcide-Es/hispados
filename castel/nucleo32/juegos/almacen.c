/* almacen.c - (JUEGOS) Almacén, el de Hispa-386 (juegos/almacen.asm) en
 * Hispa-Pentium: el mozo empuja las cajas (de una en una; no puede tirar de
 * ellas) hasta las marcas del suelo, entre estanterías. Los mismos 24
 * niveles propios (de hasta 16 x 11 casillas, de 1 a 6 cajas; los lee
 * datos386.py del fuente de Hispa-386 y tools/almacen_niveles.py comprueba
 * que todos tienen solución). Se puede ir al primero, a los ya resueltos y
 * al siguiente de uno resuelto. Arriba, el nivel, los movimientos, los
 * empujones y la mejor marca. Ahora la ventana se estira (las casillas
 * crecen), suena, y lo de cada uno (el nivel en que se quedó y la mejor
 * marca de cada nivel) va a la carpeta del usuario.
 * Flechas; Retroceso, U o Ctrl+Z deshacen (todo el nivel); R o F2 empiezan
 * el nivel otra vez; Av Pág y Re Pág cambian de nivel; un clic en la fila o
 * la columna del mozo da un paso hacia allí. Para las pruebas: /N=n (el
 * nivel, aunque no esté resuelto el anterior) y F12 (el estado).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "juego.h"
#include "datos386.h"

#define MAXC 16
#define MAXF 11
#define MAX_DESHACER 4000
enum { E_JUEGA, E_HECHO };

static u8 muro[MAXF][MAXC], marca[MAXF][MAXC], caja[MAXF][MAXC], dentro[MAXF][MAXC];
static int nc, nf, mx, my, nivel, movimientos, empujones, estado, libre_eleccion, dir_mozo = 3;
static u8 hist[MAX_DESHACER];		/* cada paso: la dirección (y 4 si empujó) */
static int n_hist;
static int cel = 32, ox, oy, arriba, cab_al = 30;

static const struct efecto s_paso = { ON_CUADRADA, 300, 300, 12, 8, 1, 10 };
static const struct efecto s_empuja = { ON_RUIDO, 400, 200, 70, 20, 3, 60 };
static const struct efecto s_marca = { ON_CAMPANA, 1046, 1046, 300, 28, 1, 280 };

static void pinta_todo(void);

static int resuelto(int n)
{
	char c[20];

	snprintf(c, sizeof c, "Mejor%d", n + 1);
	return rec_entero(c, 0) > 0;
}

static int puede_ir(int n)
{
	return libre_eleccion || n == 0 || resuelto(n) || (n > 0 && resuelto(n - 1));
}

static void carga(int n)
{
	const char *d = alm_nivel[n];
	int x = 0, y = 0;

	memset(muro, 0, sizeof muro);
	memset(marca, 0, sizeof marca);
	memset(caja, 0, sizeof caja);
	nc = nf = 0;
	for (; *d; d++) {
		if (*d == '\n') {
			y++;
			x = 0;
			continue;
		}
		if (x < MAXC && y < MAXF) {
			switch (*d) {
			case 'H': muro[y][x] = 1; break;
			case 'x': marca[y][x] = 1; break;
			case 'c': caja[y][x] = 1; break;
			case 'C': caja[y][x] = marca[y][x] = 1; break;
			case 'a': mx = x; my = y; break;
			case 'A': mx = x; my = y; marca[y][x] = 1; break;
			}
			if (x + 1 > nc)
				nc = x + 1;
			if (y + 1 > nf)
				nf = y + 1;
		}
		x++;
	}
	/* lo de dentro del almacén (lo que se pinta de suelo): lo que se alcanza desde el mozo sin pasar muros */
	{
		int pila[MAXC * MAXF][2], k = 0;

		memset(dentro, 0, sizeof dentro);
		pila[k][0] = mx;
		pila[k][1] = my;
		k++;
		dentro[my][mx] = 1;
		while (k) {
			int cx, cy, i;
			static const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };

			k--;
			cx = pila[k][0];
			cy = pila[k][1];
			for (i = 0; i < 4; i++) {
				int x2 = cx + dx[i], y2 = cy + dy[i];

				if (x2 < 0 || y2 < 0 || x2 >= nc || y2 >= nf || dentro[y2][x2] || muro[y2][x2])
					continue;
				dentro[y2][x2] = 1;
				pila[k][0] = x2;
				pila[k][1] = y2;
				k++;
			}
		}
	}
	nivel = n;
	movimientos = empujones = n_hist = 0;
	estado = E_JUEGA;
	dir_mozo = 3;
	rec_poner_entero("Nivel", n + 1);
	rec_guardar();
	serie("almacen: nivel %d (%d x %d)\n", n + 1, nc, nf);
}

static int cajas_en_marca(int *total)
{
	int x, y, n = 0, t = 0;

	for (y = 0; y < nf; y++)
		for (x = 0; x < nc; x++)
			if (caja[y][x]) {
				t++;
				n += marca[y][x];
			}
	if (total)
		*total = t;
	return n;
}

static void comprueba(void)
{
	int t, n = cajas_en_marca(&t), i;
	char c[20];

	if (n < t)
		return;
	estado = E_HECHO;
	snprintf(c, sizeof c, "Mejor%d", nivel + 1);
	if (!rec_entero(c, 0) || movimientos < rec_entero(c, 0)) {
		rec_poner_entero(c, movimientos);
		snprintf(c, sizeof c, "Empujones%d", nivel + 1);
		rec_poner_entero(c, empujones);
	}
	rec_guardar();
	{
		static const int nt[5] = { 67, 72, 76, 79, 84 };

		for (i = 0; i < 5; i++)
			snd_nota_tras(ON_TRIANGULO, nota_hz(nt[i]), 140, 24, i * 110);
	}
	serie("almacen: nivel %d resuelto: %d movimientos, %d empujones\n", nivel + 1, movimientos, empujones);
}

static void mueve(int d)
{
	static const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
	int nx = mx + dx[d], ny = my + dy[d], empuja = 0;

	if (estado != E_JUEGA)
		return;
	dir_mozo = d;
	if (nx < 0 || ny < 0 || nx >= nc || ny >= nf || muro[ny][nx])
		return;
	if (caja[ny][nx]) {
		int bx = nx + dx[d], by = ny + dy[d];

		if (bx < 0 || by < 0 || bx >= nc || by >= nf || muro[by][bx] || caja[by][bx])
			return;
		caja[ny][nx] = 0;
		caja[by][bx] = 1;
		empuja = 1;
		empujones++;
		snd_efecto(marca[by][bx] ? &s_marca : &s_empuja);
	} else
		snd_efecto(&s_paso);
	mx = nx;
	my = ny;
	movimientos++;
	if (n_hist < MAX_DESHACER)
		hist[n_hist++] = (u8)(d | (empuja ? 4 : 0));
	comprueba();
	jg_todo_sucio();
	pinta_todo();
}

static void deshace(void)
{
	static const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
	int h, d;

	if (!n_hist)
		return;
	h = hist[--n_hist];
	d = h & 3;
	if (h & 4) {
		caja[my + dy[d]][mx + dx[d]] = 0;
		caja[my][mx] = 1;
		empujones--;
	}
	mx -= dx[d];
	my -= dy[d];
	movimientos--;
	estado = E_JUEGA;
	dir_mozo = d;
	pinta_todo();
}

/* ---- pintar ---- */
static void pinta_casilla(int x, int y)
{
	int px = ox + x * cel, py = oy + y * cel;
	float c = cel / 2.0f;

	if (muro[y][x]) {
		/* una estantería: madera, con baldas y algo encima */
		lz_rect(&LZ, px, py, cel, cel, 0x5A3A22);
		lz_rect(&LZ, px + 2, py + 2, cel - 4, cel - 4, 0x7A5232);
		lz_rect(&LZ, px + 2, py + cel / 2 - 1, cel - 4, MAX(2, cel / 12), 0x4A2E1A);
		lz_rect(&LZ, px + cel / 5, py + cel / 6, cel / 4, cel / 3 - 2, 0xB8A27A);
		lz_rect(&LZ, px + cel / 2, py + cel / 2 + cel / 8, cel / 3, cel / 3 - 2, 0x8E9AA8);
		return;
	}
	if (!dentro[y][x]) {
		lz_rect(&LZ, px, py, cel, cel, 0xE9EDF2);
		return;
	}
	/* el suelo: baldosas de piedra */
	lz_rect(&LZ, px, py, cel, cel, 0xB8B0A0);
	lz_rect(&LZ, px + 1, py + 1, cel - 2, cel - 2, (x + y) % 2 ? 0xD8D2C4 : 0xCFC8B8);
	if (marca[y][x]) {
		/* la marca: un aspa dentro de un aro (que se vea sin el color) */
		lz_anillo(&LZ, (int)(px + c), (int)(py + c), (int)(cel * 0.32f), MAX(2, cel / 12), 0xC2542D);
		lz_linea_gruesa(&LZ, (int)(px + cel * 0.32f), (int)(py + cel * 0.32f), (int)(px + cel * 0.68f), (int)(py + cel * 0.68f), MAX(2, cel / 14), 0xC2542D);
		lz_linea_gruesa(&LZ, (int)(px + cel * 0.68f), (int)(py + cel * 0.32f), (int)(px + cel * 0.32f), (int)(py + cel * 0.68f), MAX(2, cel / 14), 0xC2542D);
	}
	if (caja[y][x]) {
		int b = MAX(2, cel / 10), en = marca[y][x];
		u32 m = en ? 0x6B8E3D : 0xC08A4A, os = en ? 0x3E5A20 : 0x7A5228;

		lz_rect(&LZ, px + 2, py + 2, cel - 4, cel - 4, os);
		lz_rect(&LZ, px + 2 + b, py + 2 + b, cel - 4 - 2 * b, cel - 4 - 2 * b, m);
		lz_linea_gruesa(&LZ, px + 2 + b, py + 2 + b, px + cel - 2 - b, py + cel - 2 - b, b, os);
		lz_linea_gruesa(&LZ, px + cel - 2 - b, py + 2 + b, px + 2 + b, py + cel - 2 - b, b, os);
		if (en) {
			/* en su sitio: con una marca blanca */
			lz_circulo_suave(&LZ, px + c, py + c, cel * 0.2f, 0xFFFFFF);
			lz_linea_gruesa(&LZ, (int)(px + c - cel * 0.1f), (int)(py + c), (int)(px + c - cel * 0.02f), (int)(py + c + cel * 0.08f), 2, 0x3E5A20);
			lz_linea_gruesa(&LZ, (int)(px + c - cel * 0.02f), (int)(py + c + cel * 0.08f), (int)(px + c + cel * 0.12f), (int)(py + c - cel * 0.1f), 2, 0x3E5A20);
		}
	}
	if (x == mx && y == my) {
		/* el mozo: la gorra, la cabeza y el cuerpo, mirando a donde va */
		lz_redondo(&LZ, (int)(px + cel * 0.25f), (int)(py + cel * 0.42f), (int)(cel * 0.5f), (int)(cel * 0.48f), cel / 6, 0x2F4B8F);
		lz_circulo_suave(&LZ, px + c, py + cel * 0.34f, cel * 0.2f, 0xF0C8A0);
		lz_rect(&LZ, (int)(px + cel * 0.3f), (int)(py + cel * 0.12f), (int)(cel * 0.4f), MAX(2, cel / 9), 0xC2542D);
		{
			float ex = dir_mozo == 0 ? 0.06f : dir_mozo == 1 ? -0.06f : 0, ey = dir_mozo == 2 ? 0.03f : dir_mozo == 3 ? -0.03f : 0;

			lz_circulo_suave(&LZ, px + c - cel * 0.07f + ex * cel, py + cel * 0.34f + ey * cel, MAX(1.0f, cel * 0.03f), 0x1A1410);
			lz_circulo_suave(&LZ, px + c + cel * 0.07f + ex * cel, py + cel * 0.34f + ey * cel, MAX(1.0f, cel * 0.03f), 0x1A1410);
		}
	}
}

static void pinta_todo(void)
{
	char t[160], c[20];
	int x, y, tot, n = cajas_en_marca(&tot), mejor;

	lz_rect(&LZ, 0, 0, LZ.an, LZ.al, 0xE9EDF2);
	if (!jg_completa)
		jg_pinta_barra();
	snprintf(c, sizeof c, "Mejor%d", nivel + 1);
	mejor = rec_entero(c, 0);
	if (mejor)
		snprintf(t, sizeof t, "Nivel %d de %d   Movimientos: %d   Empujones: %d   Cajas: %d de %d   Mejor: %d", nivel + 1, ALM_NIVELES, movimientos, empujones, n, tot, mejor);
	else
		snprintf(t, sizeof t, "Nivel %d de %d   Movimientos: %d   Empujones: %d   Cajas: %d de %d", nivel + 1, ALM_NIVELES, movimientos, empujones, n, tot);
	lz_negrita(&LZ, 10, arriba + 7, t, 0x1A1410, 1);
	for (y = 0; y < nf; y++)
		for (x = 0; x < nc; x++)
			pinta_casilla(x, y);
	if (estado == E_HECHO) {
		int an = MIN(nc * cel, 360);

		snprintf(t, sizeof t, "%d movimientos y %d empujones.\n%s", movimientos, empujones, nivel + 1 < ALM_NIVELES ? "Av Pág o Intro: el siguiente." : "¡Son todos!");
		jg_cartel(&LZ, ox + (nc * cel - an) / 2, oy, an, nf * cel, "¡Resuelto!", t);
	}
	jg_todo_sucio();
}

static void tamano(int an, int al)
{
	arriba = jg_completa ? 0 : ALTO_BARRA;
	cel = MIN((an - 16) / MAX(nc, 1), (al - arriba - cab_al - 12) / MAX(nf, 1));
	cel = LIMITA(cel, 10, 80);
	ox = (an - nc * cel) / 2;
	oy = arriba + cab_al + (al - arriba - cab_al - nf * cel) / 2;
	pinta_todo();
}

static void ir_a(int n)
{
	if (n < 0 || n >= ALM_NIVELES)
		return;
	if (!puede_ir(n)) {
		jg_aviso("Almacén", "Ese nivel aún no se puede: resuelva antes el anterior.");
		return;
	}
	carga(n);
	tamano(LZ.an, LZ.al);
}

static void pinta(void) {}

/* ---- los menús y los sucesos ---- */
static char op_juego[200];
static const char *menus_op[2];

static void pon_menus(void)
{
	snprintf(op_juego, sizeof op_juego, "Empezar el nivel otra vez   R|Deshacer   Retroceso|-|Nivel anterior   Re Pág|Nivel siguiente   Av Pág|Elegir nivel...|-|%sSonidos|-|Salir",
		 snd_mudo ? "" : "» ");
	menus_op[0] = op_juego;
	menus_op[1] = "Cómo se juega...   F1|-|Acerca de Almacén...";
	jg_menus("Juego|Ayuda", menus_op);
}

static void elige_nivel(void)
{
	static char nombres[ALM_NIVELES][40];
	static const char *op[ALM_NIVELES];
	int i, k = 0, idx[ALM_NIVELES], r, ini = 0;

	/* (de 12 en 12: la lista no cabe entera en una pantalla pequeña) */
	if (nivel >= 12)
		ini = 12;
	for (i = ini; i < MIN(ALM_NIVELES, ini + 12); i++)
		if (puede_ir(i)) {
			char c[20];

			snprintf(c, sizeof c, "Mejor%d", i + 1);
			if (rec_entero(c, 0))
				snprintf(nombres[k], sizeof nombres[k], "Nivel %d (resuelto: %d)", i + 1, rec_entero(c, 0));
			else
				snprintf(nombres[k], sizeof nombres[k], "Nivel %d", i + 1);
			op[k] = nombres[k];
			idx[k] = i;
			k++;
		}
	r = jg_elige("Elegir nivel", op, k, 0);
	if (r >= 0)
		ir_a(idx[r]);
}

static void ayuda(void)
{
	jg_aviso("Cómo se juega", "Lleve todas las cajas a las marcas del suelo (un aro con un aspa). El mozo empuja las cajas de una en una; no puede tirar de ellas.\n\n"
		 "Flechas: andar. Retroceso, U o Ctrl+Z: deshacer. R o F2: empezar el nivel otra vez. Av Pág y Re Pág: otro nivel. "
		 "Un clic en la fila o la columna del mozo: un paso hacia allí.\n\nLas cajas en su sitio se ven verdes y con una marca blanca.");
}

static void suceso(const struct suceso *s)
{
	int m, o;

	switch (s->clase) {
	case EV_TECLA:
		switch (s->a) {
		case 0x158: {
			int t, n = cajas_en_marca(&t);

			serie("almacen: estado=%d nivel=%d movimientos=%d empujones=%d cajas=%d/%d mozo=%d,%d sonido=%d\n", estado, nivel + 1, movimientos, empujones, n, t, mx, my, snd_hay);
			return;
		}
		case T_DERECHA: mueve(0); return;
		case T_IZQUIERDA: mueve(1); return;
		case T_ABAJO: mueve(2); return;
		case T_ARRIBA: mueve(3); return;
		case T_BORRAR: case 'u': case 'U': case 26: deshace(); return;
		case 'r': case 'R': case T_F2: carga(nivel); tamano(LZ.an, LZ.al); return;
		case T_AV_PAG: ir_a(nivel + 1); return;
		case T_RE_PAG: ir_a(nivel - 1); return;
		case 0x13B: ayuda(); return;
		case 10: case 13: if (estado == E_HECHO) ir_a(nivel + 1); return;
		}
		return;
	case EV_RATON_BAJA: {
		int x = (int)(int32_t)s->a, y = (int)(int32_t)s->b, cx, cy;

		if (jg_clic_barra(x, y) || x < ox || y < oy)
			return;
		cx = (x - ox) / cel;
		cy = (y - oy) / cel;
		if (cy == my && cx != mx)
			mueve(cx > mx ? 0 : 1);
		else if (cx == mx && cy != my)
			mueve(cy > my ? 2 : 3);
		return;
	}
	case EV_MENU:
		o = jg_menu_elegido(s, &m);
		if (o < 0)
			return;
		if (m == 1) {
			if (o == 0)
				ayuda();
			else
				jg_aviso("Acerca de Almacén", "Almacén 1.0, para Hispa-Pentium (el de Hispa-386, con sus 24 niveles).\n\nCopyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.");
			return;
		}
		switch (o) {
		case 0: carga(nivel); tamano(LZ.an, LZ.al); break;
		case 1: deshace(); break;
		case 3: ir_a(nivel - 1); break;
		case 4: ir_a(nivel + 1); break;
		case 5: elige_nivel(); break;
		case 7: snd_mudo = !snd_mudo; rec_poner_entero("Sonidos", !snd_mudo); rec_guardar(); pon_menus(); break;
		case 9: jg_salir = 1; break;
		}
		return;
	}
}

int principal(void)
{
	static const struct juego j = { 0, pinta, suceso, tamano, 0, 30 };
	int an, al, n0;
	u32 n, i;

	n = sc(SC_ARGUMENTOS, (u32)jg_args, sizeof jg_args - 1, 0);
	if (ES_ERROR(n))
		n = 0;
	jg_args[MIN(n, sizeof jg_args - 1)] = 0;
	for (i = 0; jg_args[i]; i++)
		if (jg_args[i] >= 'a' && jg_args[i] <= 'z')
			jg_args[i] = (char)(jg_args[i] - 32);
	jg_tamano_maximo(&an, &al);
	if (jg_abrir("Almacén", MIN(an, 16 * 34 + 24), MIN(al, 11 * 34 + 30 + 40), 0))
		return 1;
	rec_abrir("ALMACEN.REC");
	snd_mudo = !rec_entero("Sonidos", 1);
	libre_eleccion = arg_hay("/N");
	n0 = libre_eleccion ? arg_entero("/N=", 1) : rec_entero("Nivel", 1);
	n0 = LIMITA(n0, 1, ALM_NIVELES) - 1;
	if (!puede_ir(n0))
		n0 = 0;
	snd_abrir();
	pon_menus();
	carga(n0);
	tamano(LZ.an, LZ.al);
	serie("almacen: ventana %d x %d (casillas de %d); lo guardado en %s\n", LZ.an, LZ.al, cel, rec_ruta());
	return jg_correr(&j);
}
