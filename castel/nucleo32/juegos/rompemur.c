/* rompemur.c - (JUEGOS) Rompemuros, para Hispa-Pentium.
 *
 * Una pala abajo, una bola y un muro de ladrillos arriba: se devuelve la
 * bola con la pala hasta que no queda ladrillo que se pueda romper. Diseño,
 * niveles, dibujos y sonidos propios.
 *
 * - Ladrillos: los normales (un golpe; cada color con su dibujo), los duros
 *   (dos golpes: con remaches; con el primero se rajan), los muy duros
 *   (tres: doble marco), los de metal (no se rompen: tornillos y brillo),
 *   los explosivos (un aspa: se llevan los de alrededor) y los de premio
 *   (una estrella: siempre sueltan una cápsula).
 * - Premios (cápsulas que caen; cada una con su letra, su dibujo y su color):
 *   L pala larga, C pala corta (¡cuidado!), M tres bolas, P pala pegajosa
 *   (la bola se queda en la pala hasta soltarla), R bola lenta, D disparos
 *   (dos cañones en la pala) y V una vida más.
 * - La bola sale de la pala con un ángulo que depende de dónde la toca, y
 *   va algo más deprisa con cada golpe (hasta un tope); cada nivel, un poco
 *   más.
 * - 24 niveles propios y el editor (Juego > Editor de niveles...): un clic
 *   pone el ladrillo elegido, el botón derecho lo quita; se guardan, hasta
 *   diez, en ROMPEMUR.NIV de la carpeta de ajustes del usuario, y se juegan
 *   con «Jugar mis niveles».
 * - Récords: los diez mejores, en la carpeta de ajustes (juego.c).
 *
 * Ratón: la pala sigue a la flecha; un clic suelta la bola (o dispara).
 * Teclado: ← → (o A D), Espacio suelta o dispara, P pausa, F2 otra partida,
 * F11 pantalla completa. La rueda también mueve la pala.
 * Para las pruebas: /S=n, /NIVEL=n, /SOLA (la pala sigue a la bola sola),
 * /MIDE y F12 (el estado, por el puerto serie).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "juego.h"

#define COLS 13
#define FILAS 18
#define LAD_AN 32		/* un ladrillo, en unidades del campo */
#define LAD_AL 14
#define CAMPO_AN (COLS * LAD_AN)
#define CAMPO_AL 460
#define LAD_Y0 36
#define PALA_Y 430
#define PALA_AL 10
#define RADIO 5.0f
#define MAX_BOLAS 8
#define MAX_CAPS 6
#define MAX_TIROS 12
#define MAX_TROZOS 48
#define PASOS 4			/* pasos de la física por pulso (400 por segundo) */
#define N_NIVELES 24
#define MAX_PROPIOS 10

enum { E_LISTO, E_JUEGA, E_PAUSA, E_PERDIDA, E_NIVEL, E_FIN, E_EDITOR };
/* los ladrillos (lo que hay en cada casilla) */
enum { L_NADA, L_ROJO, L_OCRE, L_AMARILLO, L_VERDE, L_TURQUESA, L_AZUL, L_MORADO, L_CREMA, L_DURO, L_MUY_DURO, L_METAL, L_EXPLOSIVO, L_PREMIO, N_LAD };
static const char letras_lad[N_LAD + 1] = ".rnavtzmb23MXP";
static const u32 color_lad[N_LAD] = { 0, 0xC0573E, 0xD98A3A, 0xD9B83A, 0x6E8B3D, 0x2E8C83, 0x2F5D9E, 0x8A4F7D, 0xE8DCC0, 0x8E9AA8, 0x6E7C8C, 0xB8C2CC, 0xB03A2E, 0xE0B040 };
static const char *const nombre_lad[N_LAD] = { "nada", "rojo (rayas)", "naranja (puntos)", "amarillo (rombo)", "verde (diagonales)", "turquesa (cuadros)",
	"azul (rayas de pie)", "morado (aspas)", "crema (liso)", "duro: dos golpes", "muy duro: tres golpes", "metal: no se rompe", "explosivo", "premio: suelta una cápsula" };
/* los premios */
enum { P_LARGA, P_CORTA, P_MULTI, P_PEGA, P_LENTA, P_DISPARO, P_VIDA, N_PREMIOS };
static const char letra_premio[N_PREMIOS] = { 'L', 'C', 'M', 'P', 'R', 'D', 'V' };
static const u32 color_premio[N_PREMIOS] = { 0x3E8E4E, 0x9E2B2B, 0x2F5D9E, 0xC98A2A, 0x2E8C83, 0x7B4F9D, 0xC0407A };
static const char *const nombre_premio[N_PREMIOS] = { "pala larga", "pala corta", "tres bolas", "pala pegajosa", "bola lenta", "disparos", "una vida más" };

/* los niveles: 13 columnas, hasta 18 filas («.» nada; r n a v t z m b colores; 2 3 duros; M metal; X explosivo; P premio) */
static const char *const niveles[N_NIVELES] = {
	/* 1: Primeros ladrillos */
	".............|rrrrrrrrrrrrr|nnnnnnnnnnnnn|aaaaaaaaaaaaa|vvvvvvvvvvvvv|ttttttttttttt",
	/* 2: Pirámide */
	"......P......|.....rrr.....|....nnnnn....|...aaaaaaa...|..vvvvvvvvv..|.ttttttttttt.|zzzzzzzzzzzzz",
	/* 3: Ajedrez */
	"r.r.r.r.r.r.r|.n.n.n.n.n.n.|a.a.a.a.a.a.a|.v.v.v.v.v.v.|t.t.t.t.t.t.t|.z.z.z.z.z.z.|m.m.m.m.m.m.m",
	/* 4: Corazón */
	"..rr.....rr..|.rrrr...rrrr.|rrrrrr.rrrrrr|rrrrrrrrrrrrr|.rrrrrPrrrrr.|..rrrrrrrrr..|...rrrrrrr...|....rrrrr....|.....rrr.....|......r......",
	/* 5: Franjas duras */
	"2222222222222|r.r.r.r.r.r.r|nnnnnnnnnnnnn|.............|2222222222222|ttttttttttttt|.............|zzzzPzzzPzzzz",
	/* 6: El disquete */
	"zzzzzzzzzzz..|zbbbbbbbbbzz.|zbbbbbbbbbzzz|zbbbbbbbbbzzz|zzzzzzzzzzzzz|zzz2222222zzz|zzz22zz2222zz|zzz22zz2222zz|zzz2222222zzz|zzzzzzzzzzzzz",
	/* 7: Almenas */
	"M.M.M...M.M.M|MMMMM...MMMMM|M222M...M222M|MaaaMrrrMaaaM|MaaaMrPrMaaaM|MaaaMrrrMaaaM|.............|vvvvvvvvvvvvv",
	/* 8: Rombos */
	"......a......|.....aPa.....|....a.a.a....|...a.....a...|..t.......t..|...t.....t...|....t.t.t....|.....tPt.....|......t......",
	/* 9: Barriles */
	"rrrrrrrrrrrrr|rXrrrrXrrrrXr|rrrrrrrrrrrrr|.............|nnnnnnnnnnnnn|nnXnnnnnnnXnn|nnnnnnnnnnnnn",
	/* 10: Escalera */
	"r............|rn...........|rna..........|rnav.........|rnavt........|rnavtz.......|rnavtzm......|rnavtzmb.....|2222222222222",
	/* 11: La flor */
	".....mmm.....|....mmmmm....|...mmaaamm...|...mmaPamm...|...mmaaamm...|....mmmmm....|.....mmm.....|......v......|....vvvvv....|......v......|......v......",
	/* 12: Pasillos de metal */
	"M.....M.....M|Mrrrr.M.rrrrM|Mnnnn.M.nnnnM|Maaaa.M.aaaaM|Mvvvv...vvvvM|Mtttt.M.ttttM|MPzzz.M.zzzPM|M.....M.....M",
	/* 13: La ola */
	"tt...........|ttt.........t|.ttt.......tt|..ttt.....tt.|...tttt.ttt..|....ttttttt..|zzzzzzzzzzzzz|2222222222222",
	/* 14: El barco */
	"......b......|.....bbb.....|....bbbbb....|...bbbbbbb...|......M......|rrrrrrrrrrrrr|.rrrrrrrrrrr.|..rrPrrrPrr..|.............|zzz.zzz.zzz.z",
	/* 15: Muro doble */
	"3333333333333|2222222222222|aaaaaaaaaaaaa|aaaaaaaaaaaaa|2222222222222|3333333333333",
	/* 16: El sol */
	"a.....a.....a|.a....a....a.|..a.nnnnn.a..|...nnnnnnn...|aaannnPnnnaaa|...nnnnnnn...|..a.nnnnn.a..|.a....a....a.|a.....a.....a",
	/* 17: La llave */
	"...aaaa......|..aa..aa.....|..aa..aa.....|...aaaa......|....aa.......|....aa.......|....aaaa.....|....aa.......|....aaaa.....|.............|2222222222222",
	/* 18: Explosiones en cadena */
	"XXXXXXXXXXXXX|r.r.r.r.r.r.r|.............|nnnnnnnnnnnnn|.X.X.X.X.X.X.|aaaaaaaaaaaaa",
	/* 19: El cactus */
	"......v......|.....vvv.....|..v..vvv..v..|..vv.vvv.vv..|...vvvvvvv...|.....vvv.....|.....vvv.....|.....vvv.....|nnnnnnnnnnnnn|nnnnnPnPnnnnn",
	/* 20: Laberinto */
	"MMMMMM.MMMMMM|M...........M|M.MMMM.MMMM.M|M.M2222222M.M|M.M2PaaaP2M.M|M.M2222222M.M|M.MMM...MMM.M|M...........M",
	/* 21: Arcoíris */
	"rrrrrrrrrrrrr|nnnnnnnnnnnnn|aaaaaaaaaaaaa|vvvvvvvvvvvvv|ttttttttttttt|zzzzzzzzzzzzz|mmmmmmmmmmmmm|2222222222222|3333333333333",
	/* 22: La casa */
	"......r......|.....rrr.....|....rrrrr....|...rrrrrrr...|..rrrrrrrrr..|..bbbbbbbbb..|..bzzbbbzzb..|..bzzbbbzzb..|..bbbb2bbbb..|..bbbb2bbbb..|MMMMMMMMMMMMM",
	/* 23: Fortaleza */
	"M3M3M3M3M3M3M|3.3.3.3.3.3.3|M2M2M2M2M2M2M|.P.........P.|2222222222222|XaXaXaXaXaXaX|mmmmmmmmmmmmm",
	/* 24: Gran final */
	"3333333333333|3MMMMMMMMMMM3|3M222222222M3|3M2XXXXXXX2M3|3M2XPPPPPX2M3|3M2XXXXXXX2M3|3M222222222M3|3MMMMM.MMMMM3|3333333333333",
};
static const char *const nombre_nivel[N_NIVELES] = {
	"Primeros ladrillos", "Pirámide", "Ajedrez", "Corazón", "Franjas duras", "El disquete", "Almenas", "Rombos",
	"Barriles", "Escalera", "La flor", "Pasillos de metal", "La ola", "El barco", "Muro doble", "El sol",
	"La llave", "Explosiones en cadena", "El cactus", "Laberinto", "Arcoíris", "La casa", "Fortaleza", "Gran final" };

/* ---- el estado ---- */
struct bola { float x, y, vx, vy; int viva, pegada; float pega_dx; };
struct capsula { float x, y; int tipo, viva; };
struct tiro { float x, y; int vivo; };
struct trozo { float x, y, vx, vy; u32 c; int vida; };
static u8 muro[FILAS][COLS], golpes[FILAS][COLS];
static struct bola bolas[MAX_BOLAS];
static struct capsula caps[MAX_CAPS];
static struct tiro tiros[MAX_TIROS];
static struct trozo trozos[MAX_TROZOS];
static int estado = E_LISTO, nivel = 1, vidas = 3, sola, propios, n_propios, record_pos = -1;
static long puntos;
static float pala_x = CAMPO_AN / 2, pala_an = 64, velocidad = 300;
static int pega, disparo, t_larga, t_lenta, t_disparo, t_pega, t_estado, recarga;
static int raton_x = -1, raton_activo;
static char propio[MAX_PROPIOS][FILAS * (COLS + 1) + 1];
/* el editor */
static int ed_nivel, ed_elegido = L_ROJO;
static u8 ed_muro[FILAS][COLS];

/* ---- la pantalla ---- */
static float esc = 1.0f;
static int cx0, cy0, arriba, hud_al = 38;
static lienzo fondo, capa;	/* el fondo; y el fondo con los ladrillos (lo que no se mueve) */
static lienzo lad[N_LAD][3];	/* cada ladrillo pintado, con 0, 1 o 2 golpes */
static int lad_an, lad_al;
static struct { int x, y, an, al; } antes[64], ahora[64];
static int n_antes, n_ahora;

static void pinta_todo(void);
static void empieza_nivel(void);

/* -------------------------------------------------------------- el sonido */
static const struct efecto s_pared = { ON_CUADRADA, 520, 520, 18, 14, 1, 14 };
static const struct efecto s_pala = { ON_TRIANGULO, 300, 220, 60, 40, 1, 50 };
static const struct efecto s_metal = { ON_CAMPANA, 1800, 1800, 220, 22, 1, 200 };
static const struct efecto s_duro = { ON_CUADRADA, 260, 180, 50, 26, 1, 40 };
static const struct efecto s_boom = { ON_RUIDO, 900, 120, 420, 44, 2, 380 };
static const struct efecto s_pierde = { ON_TRIANGULO, 600, 90, 700, 40, 5, 500 };
static const struct efecto s_tiro = { ON_SIERRA, 1400, 500, 70, 18, 1, 60 };

static void suena_ladrillo(int fila)
{
	static const int escala[8] = { 84, 81, 79, 76, 74, 72, 69, 67 };

	snd_nota(ON_CAMPANA, nota_hz(escala[fila % 8]), 200, 26);
}

static void fanfarria(void)
{
	static const int n[6] = { 67, 72, 76, 79, 76, 84 };
	int i;

	for (i = 0; i < 6; i++)
		snd_nota_tras(ON_CUADRADA, nota_hz(n[i]), i == 5 ? 500 : 120, 20, i * 120);
}

/* -------------------------------------------------------------- el muro */
static int lad_de_letra(char c)
{
	const char *p = strchr(letras_lad, c);

	return p && c ? (int)(p - letras_lad) : L_NADA;
}

static int golpes_de(int t) { return t == L_DURO ? 2 : t == L_MUY_DURO ? 3 : 1; }
static int rompible(int t) { return t != L_NADA && t != L_METAL; }

static void lee_nivel(const char *d, u8 m[FILAS][COLS])
{
	int f = 0, c = 0;

	memset(m, 0, FILAS * COLS);
	for (; *d && f < FILAS; d++) {
		if (*d == '|' || *d == '\n') {
			f++;
			c = 0;
			continue;
		}
		if (c < COLS)
			m[f][c++] = (u8)lad_de_letra(*d);
	}
}

static int quedan(void)
{
	int f, c, n = 0;

	for (f = 0; f < FILAS; f++)
		for (c = 0; c < COLS; c++)
			if (rompible(muro[f][c]))
				n++;
	return n;
}

/* -------------------------------------------------------------- pintar (los ladrillos) */
static void dibujo_ladrillo(lienzo *l, int x, int y, int an, int al, int t, int roto)
{
	u32 c = color_lad[t];
	int b = MAX(1, al / 7), i;

	if (t == L_METAL) {
		lz_degradado(l, x, y, an, al, 0xE8EEF4, 0x7C8896);
		lz_rect(l, x, y + al / 3, an, MAX(1, al / 6), 0xF8FBFF);
		lz_marco(l, x, y, an, al, 0x4E5866);
		lz_circulo_suave(l, x + al / 3.0f, y + al / 2.0f, al / 7.0f + 0.5f, 0x4E5866);
		lz_circulo_suave(l, x + an - al / 3.0f, y + al / 2.0f, al / 7.0f + 0.5f, 0x4E5866);
		return;
	}
	lz_degradado(l, x, y, an, al, aclara(c, 50), aclara(c, -40));
	lz_rect(l, x, y, an, b, aclara(c, 130));
	lz_rect(l, x, y, b, al, aclara(c, 90));
	lz_rect(l, x, y + al - b, an, b, aclara(c, -120));
	lz_rect(l, x + an - b, y, b, al, aclara(c, -100));
	switch (t) {
	case L_ROJO:
		for (i = 1; i <= 2; i++)
			lz_rect(l, x + an / 8, y + i * al / 3, an - an / 4, MAX(1, al / 10), aclara(c, -90));
		break;
	case L_OCRE:
		for (i = 1; i <= 4; i++)
			lz_circulo_suave(l, x + i * an / 5.0f, y + al / 2.0f, al / 9.0f + 0.5f, aclara(c, 140));
		break;
	case L_AMARILLO: {
		float q[8] = { x + an / 2.0f, y + al * 0.2f, x + an / 2.0f + al * 0.5f, y + al / 2.0f, x + an / 2.0f, y + al * 0.8f, x + an / 2.0f - al * 0.5f, y + al / 2.0f };

		lz_poligono(l, q, 4, aclara(c, -100));
		break;
	}
	case L_VERDE:
		for (i = 0; i < 5; i++)
			lz_linea_gruesa(l, x + i * an / 5 + 2, y + al - b - 1, x + i * an / 5 + al / 2 + 2, y + b + 1, MAX(1, al / 9), aclara(c, -100));
		break;
	case L_TURQUESA:
		for (i = 0; i < 4; i++)
			lz_rect(l, x + an / 8 + i * an / 5, y + al / 4 + (i % 2) * al / 4, an / 8, al / 4, aclara(c, 120));
		break;
	case L_AZUL:
		for (i = 1; i <= 5; i++)
			lz_rect(l, x + i * an / 6, y + al / 5, MAX(1, an / 24), al - 2 * (al / 5), aclara(c, 110));
		break;
	case L_MORADO:
		for (i = 0; i < 3; i++) {
			int cx = x + an / 6 + i * an / 3;

			lz_linea_gruesa(l, cx - al / 4, y + al / 4, cx + al / 4, y + al - al / 4, MAX(1, al / 8), aclara(c, 130));
			lz_linea_gruesa(l, cx + al / 4, y + al / 4, cx - al / 4, y + al - al / 4, MAX(1, al / 8), aclara(c, 130));
		}
		break;
	case L_DURO:
	case L_MUY_DURO:
		lz_marco(l, x + b + 1, y + b + 1, an - 2 * b - 2, al - 2 * b - 2, 0x3A4450);
		if (t == L_MUY_DURO)
			lz_marco(l, x + b + 3, y + b + 3, an - 2 * b - 6, al - 2 * b - 6, 0x3A4450);
		for (i = 0; i < 2; i++)
			lz_circulo_suave(l, i ? x + an - al / 3.0f : x + al / 3.0f, y + al / 2.0f, al / 8.0f + 0.4f, 0x2A323C);
		if (roto) {
			/* la raja */
			lz_linea(l, x + an / 3, y + 1, x + an / 2, y + al / 2, 0x1A1E24);
			lz_linea(l, x + an / 2, y + al / 2, x + an / 2 - 2, y + al - 2, 0x1A1E24);
			lz_linea(l, x + an / 2, y + al / 2, x + 2 * an / 3, y + al / 3, 0x1A1E24);
			if (roto > 1) {
				lz_linea(l, x + 2 * an / 3, y + 1, x + 3 * an / 4, y + al - 1, 0x1A1E24);
				lz_linea(l, x + an / 5, y + al - 1, x + an / 3, y + al / 3, 0x1A1E24);
			}
		}
		break;
	case L_EXPLOSIVO:
		lz_linea_gruesa(l, x + an / 2 - al / 3, y + al / 5, x + an / 2 + al / 3, y + al - al / 5, MAX(2, al / 5), 0xFFE08A);
		lz_linea_gruesa(l, x + an / 2 + al / 3, y + al / 5, x + an / 2 - al / 3, y + al - al / 5, MAX(2, al / 5), 0xFFE08A);
		break;
	case L_PREMIO: {
		float q[20];
		int k;

		for (k = 0; k < 10; k++) {
			float r = (k % 2 ? 0.2f : 0.45f) * al, a = -M_PI_F / 2 + k * M_PI_F / 5;

			q[2 * k] = x + an / 2.0f + r * m_cos(a) * 1.2f;
			q[2 * k + 1] = y + al / 2.0f + r * m_sen(a);
		}
		lz_poligono(l, q, 10, 0x7A4A10);
		break;
	}
	}
}

static void hace_ladrillos(void)
{
	int t, r;

	lad_an = (int)(LAD_AN * esc + 0.5f) - 1;
	lad_al = (int)(LAD_AL * esc + 0.5f) - 1;
	for (t = 1; t < N_LAD; t++)
		for (r = 0; r < 3; r++) {
			lz_libre(&lad[t][r]);
			lz_nuevo(&lad[t][r], lad_an, lad_al);
			if (lad[t][r].p)
				dibujo_ladrillo(&lad[t][r], 0, 0, lad_an, lad_al, t, r);
		}
}

static int px_x(float x) { return cx0 + (int)(x * esc); }
static int px_y(float y) { return cy0 + (int)(y * esc); }

/* el fondo del campo: cada nivel, su color y su dibujo */
static void pinta_fondo(void)
{
	static const u32 fondos[6][2] = { {0x14213A, 0x0B1424}, {0x2A1636, 0x140A1E}, {0x10302C, 0x081A18},
		{0x332014, 0x1A100A}, {0x1A2440, 0x0E1428}, {0x2C1A1A, 0x160C0C} };
	int k = (nivel - 1) % 6, i, j, l = (int)(32 * esc);
	lienzo t;

	lz_degradado(&fondo, cx0, cy0, (int)(CAMPO_AN * esc), (int)(CAMPO_AL * esc), fondos[k][0], fondos[k][1]);
	lz_trozo(&t, &fondo, cx0, cy0, (int)(CAMPO_AN * esc), (int)(CAMPO_AL * esc));
	for (j = 0; j * l < t.al; j++)
		for (i = 0; i * l < t.an; i++) {
			int px = i * l, py = j * l;

			if (nivel % 2) {
				float q[8] = { px + l / 2.0f, py + 3.0f, px + l - 3.0f, py + l / 2.0f, px + l / 2.0f, py + l - 3.0f, px + 3.0f, py + l / 2.0f };

				lz_poligono_mezcla(&t, q, 4, 0xFFFFFF, 10);
			} else
				lz_anillo(&t, px + l / 2, py + l / 2, l / 3, 1, aclara(fondos[k][0], 20));
		}
}

static void pinta_ladrillo(int f, int c)
{
	int x = px_x((float)(c * LAD_AN)), y = px_y((float)(LAD_Y0 + f * LAD_AL)), t = muro[f][c];
	int an = px_x((float)((c + 1) * LAD_AN)) - x, al = px_y((float)(LAD_Y0 + (f + 1) * LAD_AL)) - y;

	/* lo de debajo: el fondo otra vez (solo esa casilla) */
	lz_copia(&capa, x, y, &fondo, x, y, an, al);
	if (t) {
		lz_mezcla(&capa, x, y, an, al, 0, 110);	/* el hueco, oscuro */
		/* (los duros, rajados con cada golpe que llevan) */
		lz_copia(&capa, x, y, &lad[t][MIN((int)golpes[f][c], 2)], 0, 0, MIN(lad_an, an - 1), MIN(lad_al, al - 1));
	}
}

static void pinta_capa(void)
{
	int f, c;

	lz_libre(&fondo);
	lz_nuevo(&fondo, LZ.an, LZ.al);
	lz_libre(&capa);
	lz_nuevo(&capa, LZ.an, LZ.al);
	if (!capa.p || !fondo.p)
		return;
	lz_degradado(&fondo, 0, 0, fondo.an, fondo.al, 0x2A3346, 0x161C28);
	/* el marco del campo, de metal */
	lz_rect(&fondo, cx0 - 8, cy0 - 8, (int)(CAMPO_AN * esc) + 16, (int)(CAMPO_AL * esc) + 8, 0x5C6878);
	lz_rect(&fondo, cx0 - 6, cy0 - 6, (int)(CAMPO_AN * esc) + 12, (int)(CAMPO_AL * esc) + 6, 0xA8B4C2);
	lz_rect(&fondo, cx0 - 3, cy0 - 3, (int)(CAMPO_AN * esc) + 6, (int)(CAMPO_AL * esc) + 3, 0x3C4654);
	pinta_fondo();
	lz_copia(&capa, 0, 0, &fondo, 0, 0, LZ.an, LZ.al);
	for (f = 0; f < FILAS; f++)
		for (c = 0; c < COLS; c++)
			if (muro[f][c])
				pinta_ladrillo(f, c);
}

static void ladrillo_cambia(int f, int c)
{
	int x = px_x((float)(c * LAD_AN)), y = px_y((float)(LAD_Y0 + f * LAD_AL));
	int an = px_x((float)((c + 1) * LAD_AN)) - x, al = px_y((float)(LAD_Y0 + (f + 1) * LAD_AL)) - y;

	pinta_ladrillo(f, c);
	lz_copia(&LZ, x, y, &capa, x, y, an, al);
	jg_sucio(x, y, an, al);
}

/* -------------------------------------------------------------- jugar */
static void trozos_de(int f, int c)
{
	int i, k = 0;
	u32 col = color_lad[muro[f][c]];

	for (i = 0; i < MAX_TROZOS && k < 8; i++)
		if (!trozos[i].vida) {
			trozos[i].x = c * LAD_AN + 4 + azar_n(LAD_AN - 8);
			trozos[i].y = LAD_Y0 + f * LAD_AL + 2 + azar_n(LAD_AL - 4);
			trozos[i].vx = (float)(azar_n(160) - 80);
			trozos[i].vy = (float)(-azar_n(120));
			trozos[i].c = aclara(col, azar_n(120) - 60);
			trozos[i].vida = 50 + azar_n(30);
			k++;
		}
}

static void suelta_capsula(float x, float y, int forzada)
{
	int i, t;

	if (!forzada && azar_n(100) >= 16)
		return;
	t = azar_n(100);
	/* lo bueno sale más que lo malo; la vida, poco */
	t = t < 18 ? P_LARGA : t < 28 ? P_CORTA : t < 46 ? P_MULTI : t < 60 ? P_PEGA : t < 76 ? P_LENTA : t < 92 ? P_DISPARO : P_VIDA;
	for (i = 0; i < MAX_CAPS; i++)
		if (!caps[i].viva) {
			caps[i].viva = 1;
			caps[i].x = x;
			caps[i].y = y;
			caps[i].tipo = t;
			return;
		}
}

static void rompe(int f, int c, int fuerza);

static void explota(int f, int c)
{
	int df, dc;

	snd_efecto(&s_boom);
	for (df = -1; df <= 1; df++)
		for (dc = -1; dc <= 1; dc++) {
			int ff = f + df, cc = c + dc;

			if ((df || dc) && ff >= 0 && ff < FILAS && cc >= 0 && cc < COLS && rompible(muro[ff][cc]))
				rompe(ff, cc, 9);
		}
}

static void rompe(int f, int c, int fuerza)
{
	int t = muro[f][c];

	if (!t)
		return;
	if (t == L_METAL) {
		snd_efecto(&s_metal);
		return;
	}
	golpes[f][c] = (u8)(golpes[f][c] + fuerza);
	if (golpes[f][c] < golpes_de(t)) {
		snd_efecto(&s_duro);
		puntos += 20;
		ladrillo_cambia(f, c);
		return;
	}
	puntos += t == L_DURO ? 100 : t == L_MUY_DURO ? 150 : t == L_EXPLOSIVO ? 120 : 50 + 10 * (FILAS - f) / 3;
	trozos_de(f, c);
	muro[f][c] = L_NADA;
	ladrillo_cambia(f, c);
	suena_ladrillo(f);
	suelta_capsula(c * LAD_AN + LAD_AN / 2.0f, LAD_Y0 + f * LAD_AL + LAD_AL / 2.0f, t == L_PREMIO);
	if (t == L_EXPLOSIVO)
		explota(f, c);
}

static float rapidez(void)
{
	return velocidad * (t_lenta > 0 ? 0.7f : 1.0f);
}

static void ajusta_rapidez(struct bola *b)
{
	float v = m_raiz(b->vx * b->vx + b->vy * b->vy), r = rapidez();

	if (v > 0.01f) {
		b->vx = b->vx / v * r;
		b->vy = b->vy / v * r;
	}
	/* nunca casi de lado (no acabaría nunca de subir) */
	if (m_abs(b->vy) < r * 0.28f) {
		b->vy = (b->vy < 0 ? -1 : 1) * r * 0.28f;
		b->vx = (b->vx < 0 ? -1 : 1) * m_raiz(r * r - b->vy * b->vy);
	}
}

static void bola_nueva(void)
{
	memset(bolas, 0, sizeof bolas);
	bolas[0].viva = 1;
	bolas[0].pegada = 1;
	bolas[0].pega_dx = 6;
	bolas[0].x = pala_x + 6;
	bolas[0].y = PALA_Y - RADIO - 0.5f;
}

static void lanza(void)
{
	int i;

	for (i = 0; i < MAX_BOLAS; i++)
		if (bolas[i].viva && bolas[i].pegada) {
			float a = (bolas[i].pega_dx / (pala_an / 2)) * 1.0f;

			bolas[i].pegada = 0;
			bolas[i].vx = rapidez() * a * 0.8f;
			bolas[i].vy = -rapidez();
			ajusta_rapidez(&bolas[i]);
		}
}

static void dispara(void)
{
	int i, k = 0;

	if (!disparo || recarga > 0)
		return;
	for (i = 0; i < MAX_TIROS && k < 2; i++)
		if (!tiros[i].vivo) {
			tiros[i].vivo = 1;
			tiros[i].x = pala_x + (k ? pala_an / 2 - 4 : -pala_an / 2 + 4);
			tiros[i].y = PALA_Y - 4;
			k++;
		}
	recarga = 25;
	snd_efecto(&s_tiro);
}

static void premio(int t)
{
	int i;

	snd_nota(ON_TRIANGULO, nota_hz(76), 80, 30);
	snd_nota_tras(ON_TRIANGULO, nota_hz(83), 120, 30, 80);
	puntos += 100;
	switch (t) {
	case P_LARGA: pala_an = 104; t_larga = 2000; break;
	case P_CORTA: pala_an = 40; t_larga = 1200; break;
	case P_MULTI: {
		int n = 0, j;
		struct bola b = bolas[0];

		for (i = 0; i < MAX_BOLAS; i++)
			if (bolas[i].viva && !bolas[i].pegada) {
				b = bolas[i];
				break;
			}
		if (b.pegada) {
			lanza();
			b = bolas[0];
		}
		for (j = 0; j < MAX_BOLAS && n < 2; j++)
			if (!bolas[j].viva) {
				float a = n ? 0.5f : -0.5f;

				bolas[j] = b;
				bolas[j].viva = 1;
				bolas[j].pegada = 0;
				bolas[j].vx = b.vx * m_cos(a) - b.vy * m_sen(a);
				bolas[j].vy = b.vx * m_sen(a) + b.vy * m_cos(a);
				ajusta_rapidez(&bolas[j]);
				n++;
			}
		break;
	}
	case P_PEGA: pega = 1; t_pega = 3000; break;
	case P_LENTA: t_lenta = 1500; for (i = 0; i < MAX_BOLAS; i++) if (bolas[i].viva) ajusta_rapidez(&bolas[i]); break;
	case P_DISPARO: disparo = 1; t_disparo = 1500; break;
	case P_VIDA: vidas++; break;
	}
	serie("rompemuros: premio: %s\n", nombre_premio[t]);
}

/* la bola contra los ladrillos: lo que toca, después de moverse en un eje */
static int choca_muro(struct bola *b, int eje)
{
	int f0 = (int)((b->y - RADIO - LAD_Y0) / LAD_AL), f1 = (int)((b->y + RADIO - LAD_Y0) / LAD_AL);
	int c0 = (int)((b->x - RADIO) / LAD_AN), c1 = (int)((b->x + RADIO) / LAD_AN), f, c, hay = 0;

	if (b->y + RADIO < LAD_Y0 || b->y - RADIO >= LAD_Y0 + FILAS * LAD_AL)
		return 0;
	f0 = LIMITA(f0, 0, FILAS - 1);
	f1 = LIMITA(f1, 0, FILAS - 1);
	c0 = LIMITA(c0, 0, COLS - 1);
	c1 = LIMITA(c1, 0, COLS - 1);
	for (f = f0; f <= f1; f++)
		for (c = c0; c <= c1; c++) {
			float x0 = (float)(c * LAD_AN), y0 = (float)(LAD_Y0 + f * LAD_AL), x1 = x0 + LAD_AN, y1 = y0 + LAD_AL;
			float qx = LIMITA(b->x, x0, x1), qy = LIMITA(b->y, y0, y1), dx = b->x - qx, dy = b->y - qy;

			if (!muro[f][c] || dx * dx + dy * dy >= RADIO * RADIO)
				continue;
			if (!hay) {
				/* rebota en ese eje, y sale */
				if (eje == 0) {
					b->x = b->vx > 0 ? x0 - RADIO - 0.01f : x1 + RADIO + 0.01f;
					b->vx = -b->vx;
				} else {
					b->y = b->vy > 0 ? y0 - RADIO - 0.01f : y1 + RADIO + 0.01f;
					b->vy = -b->vy;
				}
			}
			hay = 1;
			rompe(f, c, 1);
		}
	return hay;
}

static void pierde_vida(void)
{
	snd_efecto(&s_pierde);
	vidas--;
	serie("rompemuros: bola perdida; quedan %d vidas\n", vidas);
	if (vidas <= 0) {
		estado = E_FIN;
		record_pos = rec_meter("Puntos", REC_N, (int)puntos, 0);
		serie("rompemuros: fin de la partida: %ld puntos, nivel %d; récord %d\n", puntos, nivel, record_pos + 1);
		jg_cambio = 1;
		return;
	}
	estado = E_PERDIDA;
	t_estado = 0;
	pala_an = 64;
	pega = disparo = 0;
	t_larga = t_lenta = t_disparo = t_pega = 0;
}

static void paso(float dt)
{
	int i, vivas = 0;

	for (i = 0; i < MAX_BOLAS; i++) {
		struct bola *b = &bolas[i];

		if (!b->viva)
			continue;
		if (b->pegada) {
			b->x = pala_x + b->pega_dx;
			b->y = PALA_Y - RADIO - 0.5f;
			vivas++;
			continue;
		}
		/* x, y luego y: así se sabe en qué cara ha dado */
		b->x += b->vx * dt;
		if (b->x < RADIO) { b->x = RADIO; b->vx = m_abs(b->vx); snd_efecto(&s_pared); }
		if (b->x > CAMPO_AN - RADIO) { b->x = CAMPO_AN - RADIO; b->vx = -m_abs(b->vx); snd_efecto(&s_pared); }
		choca_muro(b, 0);
		b->y += b->vy * dt;
		if (b->y < RADIO) { b->y = RADIO; b->vy = m_abs(b->vy); snd_efecto(&s_pared); }
		choca_muro(b, 1);
		/* la pala */
		if (b->vy > 0 && b->y + RADIO >= PALA_Y && b->y - RADIO <= PALA_Y + PALA_AL && b->x >= pala_x - pala_an / 2 - RADIO && b->x <= pala_x + pala_an / 2 + RADIO) {
			float d = (b->x - pala_x) / (pala_an / 2), a = LIMITA(d, -1.0f, 1.0f) * 1.05f;	/* hasta 60 grados */

			if (velocidad < 560)
				velocidad += 4;
			b->vx = rapidez() * m_sen(a);
			b->vy = -rapidez() * m_cos(a);
			b->y = PALA_Y - RADIO - 0.01f;
			snd_efecto(&s_pala);
			if (pega) {
				b->pegada = 1;
				b->pega_dx = b->x - pala_x;
			}
		}
		if (b->y > CAMPO_AL + RADIO * 2) {
			b->viva = 0;
			continue;
		}
		vivas++;
	}
	if (!vivas)
		pierde_vida();
	/* las cápsulas */
	for (i = 0; i < MAX_CAPS; i++) {
		struct capsula *k = &caps[i];

		if (!k->viva)
			continue;
		k->y += 110 * dt;
		if (k->y + 6 >= PALA_Y && k->y - 6 <= PALA_Y + PALA_AL && m_abs(k->x - pala_x) <= pala_an / 2 + 12) {
			k->viva = 0;
			premio(k->tipo);
		} else if (k->y > CAMPO_AL + 10)
			k->viva = 0;
	}
	/* los disparos */
	for (i = 0; i < MAX_TIROS; i++) {
		struct tiro *t = &tiros[i];
		int f, c;

		if (!t->vivo)
			continue;
		t->y -= 520 * dt;
		f = (int)((t->y - LAD_Y0) / LAD_AL);
		c = (int)(t->x / LAD_AN);
		if (t->y < 0)
			t->vivo = 0;
		else if (f >= 0 && f < FILAS && c >= 0 && c < COLS && muro[f][c]) {
			t->vivo = 0;
			rompe(f, c, 1);
		}
	}
}

static void pulso(void)
{
	int i;
	float objetivo = pala_x, v = 0;

	if (estado == E_EDITOR || estado == E_LISTO || estado == E_FIN || estado == E_PAUSA)
		return;
	jg_cambio = 1;
	/* los trozos caen aunque la bola se haya perdido */
	for (i = 0; i < MAX_TROZOS; i++)
		if (trozos[i].vida) {
			trozos[i].vida--;
			trozos[i].x += trozos[i].vx * 0.01f;
			trozos[i].y += trozos[i].vy * 0.01f;
			trozos[i].vy += 6;
		}
	if (estado == E_PERDIDA || estado == E_NIVEL) {
		if (++t_estado >= 150) {
			if (estado == E_NIVEL) {
				nivel++;
				if (!propios && nivel > N_NIVELES)
					nivel = 1;
				if (propios && nivel > n_propios)
					nivel = 1;
				empieza_nivel();
			} else
				bola_nueva();
			estado = E_JUEGA;
			pinta_todo();
		}
		return;
	}
	/* la pala: la flecha del ratón, o las teclas */
	if (tecla(K_IZQ) || tecla(K_A) || tecla(K_NUM_IZQ))
		v -= 1;
	if (tecla(K_DER) || tecla(K_D) || tecla(K_NUM_DER))
		v += 1;
	if (v != 0) {
		raton_activo = 0;
		objetivo = pala_x + v * (tecla(K_MAYUS) ? 9.0f : 5.5f);
	} else if (raton_activo && raton_x >= 0)
		objetivo = ((float)raton_x - cx0) / esc;
	if (sola) {
		/* (las pruebas: la pala va sola debajo de la bola más baja) */
		float y = -1;

		for (i = 0; i < MAX_BOLAS; i++)
			if (bolas[i].viva && bolas[i].y > y) {
				y = bolas[i].y;
				objetivo = bolas[i].x + (azar_n(9) - 4);
			}
	}
	objetivo = LIMITA(objetivo, pala_an / 2, CAMPO_AN - pala_an / 2);
	pala_x = objetivo;
	if (tecla_vino(K_ESPACIO) || (sola && azar_n(60) == 0)) {
		lanza();
		dispara();
	}
	if (recarga > 0)
		recarga--;
	for (i = 0; i < PASOS; i++)
		paso(0.01f / PASOS);
	if (t_larga > 0 && --t_larga == 0)
		pala_an = 64;
	if (t_lenta > 0 && --t_lenta == 0)
		for (i = 0; i < MAX_BOLAS; i++)
			if (bolas[i].viva)
				ajusta_rapidez(&bolas[i]);
	if (t_disparo > 0 && --t_disparo == 0)
		disparo = 0;
	if (t_pega > 0 && --t_pega == 0) {
		pega = 0;
		lanza();
	}
	if (estado == E_JUEGA && !quedan()) {
		estado = E_NIVEL;
		t_estado = 0;
		puntos += 1000;
		fanfarria();
		serie("rompemuros: nivel %d hecho; %ld puntos\n", nivel, puntos);
	}
}

/* -------------------------------------------------------------- pintar lo que se mueve */
static void apunta(int x, int y, int an, int al)
{
	if (n_ahora < 64) {
		ahora[n_ahora].x = x;
		ahora[n_ahora].y = y;
		ahora[n_ahora].an = an;
		ahora[n_ahora].al = al;
		n_ahora++;
	}
}

static void pinta_pala(void)
{
	int x = px_x(pala_x - pala_an / 2), y = px_y(PALA_Y), an = (int)(pala_an * esc), al = (int)(PALA_AL * esc);
	int e = MAX(3, an / 8);

	lz_mezcla(&LZ, x + 3, y + 4, an, al, 0, 90);
	lz_redondo(&LZ, x, y, an, al, al / 2, 0x46505E);
	lz_degradado(&LZ, x + e, y + 1, an - 2 * e, al - 2, 0xE8EEF6, 0x8C98A8);
	lz_redondo(&LZ, x, y, e + al / 2, al, al / 2, pega ? 0x3E8E4E : 0xC0573E);
	lz_redondo(&LZ, x + an - e - al / 2, y, e + al / 2, al, al / 2, pega ? 0x3E8E4E : 0xC0573E);
	if (disparo) {
		lz_rect(&LZ, x + e / 2, y - (int)(5 * esc), MAX(2, (int)(3 * esc)), (int)(6 * esc), 0x3C4654);
		lz_rect(&LZ, x + an - e / 2 - MAX(2, (int)(3 * esc)), y - (int)(5 * esc), MAX(2, (int)(3 * esc)), (int)(6 * esc), 0x3C4654);
	}
	apunta(x - 2, y - (int)(6 * esc) - 2, an + 8, al + (int)(6 * esc) + 8);
}

static void pinta_movil(void)
{
	int i;
	float r = RADIO * esc;

	/* borrar lo de antes (de la capa) */
	for (i = 0; i < n_antes; i++) {
		lz_copia(&LZ, antes[i].x, antes[i].y, &capa, antes[i].x, antes[i].y, antes[i].an, antes[i].al);
		jg_sucio(antes[i].x, antes[i].y, antes[i].an, antes[i].al);
	}
	n_ahora = 0;
	for (i = 0; i < MAX_TROZOS; i++)
		if (trozos[i].vida && trozos[i].y < CAMPO_AL) {
			int x = px_x(trozos[i].x), y = px_y(trozos[i].y), s = MAX(2, (int)(3 * esc));

			lz_rect(&LZ, x, y, s, s, trozos[i].c);
			apunta(x, y, s, s);
		}
	for (i = 0; i < MAX_CAPS; i++)
		if (caps[i].viva) {
			int x = px_x(caps[i].x - 13), y = px_y(caps[i].y - 6), an = (int)(26 * esc), al = (int)(12 * esc);
			char t[2] = { letra_premio[caps[i].tipo], 0 };
			u32 c = color_premio[caps[i].tipo];

			lz_redondo(&LZ, x, y, an, al, al / 2, aclara(c, -60));
			lz_redondo(&LZ, x + 1, y + 1, an - 2, al - 2, al / 2, c);
			lz_rect(&LZ, x + al / 2, y + 2, an - al, MAX(1, al / 5), aclara(c, 110));
			lz_negrita(&LZ, x + an / 2 - 4, y + (al - 16) / 2, t, 0xFFFFFF, 1);
			apunta(x, y, an + 2, al + 1);
		}
	for (i = 0; i < MAX_TIROS; i++)
		if (tiros[i].vivo) {
			int x = px_x(tiros[i].x) - 1, y = px_y(tiros[i].y - 8);

			lz_rect(&LZ, x, y, MAX(2, (int)(2 * esc)), (int)(8 * esc), 0xFFE08A);
			apunta(x, y, MAX(2, (int)(2 * esc)) + 1, (int)(8 * esc) + 1);
		}
	if (estado != E_FIN)
		pinta_pala();
	for (i = 0; i < MAX_BOLAS; i++)
		if (bolas[i].viva) {
			float x = cx0 + bolas[i].x * esc, y = cy0 + bolas[i].y * esc;

			lz_circulo_suave(&LZ, x + 2, y + 3, r, 0x05080E);
			lz_circulo_suave(&LZ, x, y, r, 0xC8D0DA);
			lz_circulo_suave(&LZ, x - r * 0.15f, y - r * 0.15f, r * 0.75f, 0xF4F6FA);
			lz_circulo_suave(&LZ, x - r * 0.35f, y - r * 0.35f, r * 0.3f, 0xFFFFFF);
			apunta((int)(x - r) - 2, (int)(y - r) - 2, (int)(2 * r) + 7, (int)(2 * r) + 8);
		}
	for (i = 0; i < n_ahora; i++)
		jg_sucio(ahora[i].x, ahora[i].y, ahora[i].an, ahora[i].al);
	memcpy(antes, ahora, sizeof antes);
	n_antes = n_ahora;
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

static void pinta_hud(void)
{
	char t[64], p[20];
	int y = arriba + 3, x = cx0, an = (int)(CAMPO_AN * esc), i;
	struct record r[1];

	lz_copia(&LZ, 0, arriba, &capa, 0, arriba, LZ.an, hud_al);
	con_puntos(p, puntos);
	snprintf(t, sizeof t, "Puntos: %s", p);
	lz_negrita(&LZ, x, y, t, 0xFFFFFF, 1);
	if (estado == E_EDITOR)
		snprintf(t, sizeof t, "Editor: mi nivel %d", ed_nivel + 1);
	else
		snprintf(t, sizeof t, "Nivel %d: %s", nivel, propios ? "propio" : nombre_nivel[(nivel - 1) % N_NIVELES]);
	lz_texto(&LZ, x, y + 17, t, 0xFFE3A0, 1);
	/* las vidas que quedan (además de la que se juega): palas pequeñas */
	for (i = 0; i < MIN(vidas - 1, 8); i++) {
		int vx = x + an - 18 - i * 22;

		lz_redondo(&LZ, vx, y + 5, 18, 6, 3, 0xA8B4C2);
		lz_rect(&LZ, vx, y + 5, 3, 6, 0xC0573E);
		lz_rect(&LZ, vx + 15, y + 5, 3, 6, 0xC0573E);
	}
	if (rec_tabla("Puntos", r, 1)) {
		con_puntos(p, MAX((long)r[0].puntos, puntos));
		snprintf(t, sizeof t, "Récord: %s", p);
		lz_texto(&LZ, x + an - ancho_texto(t, 1), y + 17, t, 0x9FB2CC, 1);
	}
	jg_sucio(0, arriba, LZ.an, hud_al);
}

static void pinta_carteles(void)
{
	int an = (int)(CAMPO_AN * esc), al = (int)(CAMPO_AL * esc), w = MIN(an - 40, 380);
	char t[240], p[20];

	if (estado == E_LISTO)
		jg_cartel(&LZ, cx0 + (an - w) / 2, cy0 + al / 3, w, al / 3, "Rompemuros",
			  "Devuelva la bola con la pala y rompa todos los ladrillos.\n\nUn clic o Espacio: empezar. F1: cómo se juega.");
	else if (estado == E_PAUSA)
		jg_cartel(&LZ, cx0 + (an - w) / 2, cy0 + al / 3, w, al / 3, "Pausa", "P o un clic para seguir.");
	else if (estado == E_NIVEL) {
		snprintf(t, sizeof t, "¡Nivel %d hecho! 1.000 puntos más.", nivel);
		jg_cartel(&LZ, cx0 + (an - w) / 2, cy0 + al / 3, w, al / 3, "¡Muy bien!", t);
	} else if (estado == E_FIN) {
		con_puntos(p, puntos);
		if (record_pos >= 0)
			snprintf(t, sizeof t, "%s puntos (nivel %d).\n¡Récord: el %d.º de la lista!\n\nUn clic o Intro: otra partida.", p, nivel, record_pos + 1);
		else
			snprintf(t, sizeof t, "%s puntos (nivel %d).\n\nUn clic o Intro: otra partida.", p, nivel);
		jg_cartel(&LZ, cx0 + (an - w) / 2, cy0 + al / 3, w, al / 3, "Fin de la partida", t);
	} else
		return;
	jg_sucio(cx0, cy0, an, al);
}

static void pinta_editor(void)
{
	int an = (int)(CAMPO_AN * esc), al = (int)(CAMPO_AL * esc), i, y0 = px_y(PALA_Y - 30);

	lz_copia(&LZ, cx0, cy0, &capa, cx0, cy0, an, al);
	for (i = 0; i <= COLS; i++)
		lz_rect(&LZ, px_x((float)(i * LAD_AN)), px_y(LAD_Y0), 1, (int)(FILAS * LAD_AL * esc), 0x34465F);
	for (i = 0; i <= FILAS; i++)
		lz_rect(&LZ, cx0, px_y((float)(LAD_Y0 + i * LAD_AL)), an, 1, 0x34465F);
	/* la paleta: un ladrillo de cada, y el elegido marcado */
	for (i = 1; i < N_LAD; i++) {
		int x = cx0 + 6 + ((i - 1) % 7) * (lad_an + 8), y = y0 + ((i - 1) / 7) * (lad_al + 8);

		lz_copia(&LZ, x, y, &lad[i][0], 0, 0, lad_an, lad_al);
		if (i == ed_elegido) {
			lz_marco(&LZ, x - 3, y - 3, lad_an + 6, lad_al + 6, 0xFFE3A0);
			lz_marco(&LZ, x - 2, y - 2, lad_an + 4, lad_al + 4, 0xFFE3A0);
		}
	}
	lz_texto(&LZ, cx0 + 6, y0 + 2 * (lad_al + 8) + 2, nombre_lad[ed_elegido], 0xFFFFFF, 1);
	lz_texto(&LZ, cx0 + 6, y0 + 2 * (lad_al + 8) + 20, "Clic: poner. Botón derecho: quitar. G: guardar. J: jugarlo. Esc: salir.", 0x9FB2CC, 1);
	jg_sucio(cx0, cy0, an, al);
}

static void pinta_todo(void)
{
	if (!capa.p)
		return;
	if (estado == E_EDITOR) {
		int f, c;

		memcpy(muro, ed_muro, sizeof muro);
		memset(golpes, 0, sizeof golpes);
		for (f = 0; f < FILAS; f++)
			for (c = 0; c < COLS; c++)
				pinta_ladrillo(f, c);
	}
	lz_copia(&LZ, 0, 0, &capa, 0, 0, LZ.an, LZ.al);
	if (!jg_completa)
		jg_pinta_barra();
	n_antes = 0;
	pinta_hud();
	if (estado == E_EDITOR)
		pinta_editor();
	else {
		pinta_movil();
		pinta_carteles();
	}
	jg_todo_sucio();
}

static void tamano(int an, int al)
{
	float e1, e2;

	arriba = jg_completa ? 0 : ALTO_BARRA;
	e1 = (float)(an - 24) / CAMPO_AN;
	e2 = (float)(al - arriba - hud_al - 16) / CAMPO_AL;
	esc = MIN(e1, e2);
	if (esc < 0.5f)
		esc = 0.5f;
	cx0 = (an - (int)(CAMPO_AN * esc)) / 2;
	cy0 = arriba + hud_al + 8;
	hace_ladrillos();
	pinta_capa();
	pinta_todo();
}

static void pinta(void)
{
	static long p_antes = -1;
	static int v_antes = -1, e_antes = -1;

	if (estado == E_EDITOR)
		return;
	if (e_antes != estado) {
		e_antes = estado;
		pinta_todo();
		return;
	}
	pinta_movil();
	if (p_antes != puntos || v_antes != vidas) {
		p_antes = puntos;
		v_antes = vidas;
		pinta_hud();
	}
	pinta_carteles();
}

/* -------------------------------------------------------------- las partidas */
static void empieza_nivel(void)
{
	int f, c;

	if (propios)
		lee_nivel(propio[(nivel - 1) % MAX(1, n_propios)], muro);
	else
		lee_nivel(niveles[(nivel - 1) % N_NIVELES], muro);
	for (f = 0; f < FILAS; f++)
		for (c = 0; c < COLS; c++)
			golpes[f][c] = 0;
	memset(caps, 0, sizeof caps);
	memset(tiros, 0, sizeof tiros);
	memset(trozos, 0, sizeof trozos);
	pala_an = 64;
	pega = disparo = 0;
	t_larga = t_lenta = t_disparo = t_pega = 0;
	velocidad = 290 + 8 * MIN(nivel, 20);
	pala_x = CAMPO_AN / 2;
	bola_nueva();
	if (capa.p)
		pinta_capa();
	serie("rompemuros: nivel %d (%s): %d ladrillos\n", nivel, propios ? "propio" : nombre_nivel[(nivel - 1) % N_NIVELES], quedan());
}

static void nueva_partida(void)
{
	puntos = 0;
	vidas = 3;
	record_pos = -1;
	nivel = MAX(1, arg_entero("/NIVEL=", 1));
	empieza_nivel();
	estado = E_JUEGA;
	teclas_olvidar();
	pinta_todo();
}

/* -------------------------------------------------------------- los niveles propios */
static void lee_propios(void)
{
	char ruta[200], *b, *p;
	u32 m, n, largo;
	const char *r = rec_ruta();
	char *barra;

	n_propios = 0;
	snprintf(ruta, sizeof ruta, "%s", r);
	barra = strrchr(ruta, '\\');
	if (!barra)
		return;
	strcpy(barra + 1, "ROMPEMUR.NIV");
	m = sc(SC_ABRIR, (u32)ruta, PARA_LEER, 0);
	if (ES_ERROR(m))
		return;
	largo = sc(SC_LARGO, m, 0, 0);
	if (ES_ERROR(largo) || largo > 8192) {
		sc(SC_CERRAR, m, 0, 0);
		return;
	}
	b = (char *)malloc(largo + 1);
	if (!b) {
		sc(SC_CERRAR, m, 0, 0);
		return;
	}
	n = sc(SC_LEER, m, (u32)b, largo);
	sc(SC_CERRAR, m, 0, 0);
	b[ES_ERROR(n) ? 0 : n] = 0;
	/* un nivel por renglón, con sus filas separadas por «|» */
	p = b;
	while (*p && n_propios < MAX_PROPIOS) {
		char *fin = p;
		int k;

		while (*fin && *fin != '\r' && *fin != '\n')
			fin++;
		k = (int)(fin - p);
		if (k > 0 && *p != ';' && k < (int)sizeof propio[0]) {
			memcpy(propio[n_propios], p, (size_t)k);
			propio[n_propios][k] = 0;
			n_propios++;
		}
		p = fin;
		while (*p == '\r' || *p == '\n')
			p++;
	}
	free(b);
}

static void guarda_propio(void)
{
	char ruta[200], *barra, t[FILAS * (COLS + 1) + 1];
	static char b[MAX_PROPIOS * (FILAS * (COLS + 1) + 3) + 100];
	int f, c, k = 0, i, n = 0, ultima = 0;
	u32 m;

	for (f = 0; f < FILAS; f++)
		for (c = 0; c < COLS; c++)
			if (ed_muro[f][c])
				ultima = f + 1;
	for (f = 0; f < MAX(1, ultima); f++) {
		for (c = 0; c < COLS; c++)
			t[k++] = letras_lad[ed_muro[f][c]];
		if (f + 1 < MAX(1, ultima))
			t[k++] = '|';
	}
	t[k] = 0;
	if (ed_nivel >= n_propios)
		ed_nivel = n_propios < MAX_PROPIOS ? n_propios++ : MAX_PROPIOS - 1;
	strcpy(propio[ed_nivel], t);
	n += snprintf(b + n, sizeof b - (size_t)n, "; Rompemuros: los niveles propios (uno por renglón; las filas, separadas por |)\r\n");
	for (i = 0; i < n_propios; i++)
		n += snprintf(b + n, sizeof b - (size_t)n, "%s\r\n", propio[i]);
	snprintf(ruta, sizeof ruta, "%s", rec_ruta());
	barra = strrchr(ruta, '\\');
	if (!barra)
		return;
	strcpy(barra + 1, "ROMPEMUR.NIV");
	m = sc(SC_ABRIR, (u32)ruta, PARA_CREAR, 0);
	if (ES_ERROR(m)) {
		jg_aviso("Editor", "No se ha podido guardar el nivel.");
		return;
	}
	sc(SC_ESCRIBIR_ARCHIVO, m, (u32)b, (u32)n);
	sc(SC_CERRAR, m, 0, 0);
	serie("rompemuros: nivel propio %d guardado en %s\n", ed_nivel + 1, ruta);
}

static void abre_editor(void)
{
	static const char *op[MAX_PROPIOS + 2];
	static char nombres[MAX_PROPIOS + 1][40];
	int i, r;

	for (i = 0; i < n_propios; i++) {
		snprintf(nombres[i], sizeof nombres[i], "Mi nivel %d", i + 1);
		op[i] = nombres[i];
	}
	if (n_propios < MAX_PROPIOS)
		op[n_propios] = "Uno nuevo (vacío)";
	op[n_propios + (n_propios < MAX_PROPIOS)] = "Uno nuevo, desde el nivel actual";
	r = jg_elige("Editor de niveles", op, n_propios + 1 + (n_propios < MAX_PROPIOS), 0);
	if (r < 0)
		return;
	if (r < n_propios) {
		ed_nivel = r;
		lee_nivel(propio[r], ed_muro);
	} else {
		ed_nivel = n_propios;
		if (r == n_propios && n_propios < MAX_PROPIOS)
			memset(ed_muro, 0, sizeof ed_muro);
		else
			lee_nivel(niveles[(nivel - 1) % N_NIVELES], ed_muro);
	}
	estado = E_EDITOR;
	pinta_capa();
	pinta_todo();
}

static void editor_clic(int x, int y, int derecho)
{
	float fx = ((float)x - cx0) / esc, fy = ((float)y - cy0) / esc;
	int c = (int)(fx / LAD_AN), f = (int)((fy - LAD_Y0) / LAD_AL), i;

	if (fy >= LAD_Y0 && f >= 0 && f < FILAS && c >= 0 && c < COLS && fy < LAD_Y0 + FILAS * LAD_AL) {
		ed_muro[f][c] = derecho ? L_NADA : (u8)ed_elegido;
		muro[f][c] = ed_muro[f][c];
		pinta_ladrillo(f, c);
		if (c + 1 < COLS)
			pinta_ladrillo(f, c + 1);
		if (f + 1 < FILAS)
			pinta_ladrillo(f + 1, c);
		pinta_editor();
		return;
	}
	for (i = 1; i < N_LAD; i++) {
		int bx = cx0 + 6 + ((i - 1) % 7) * (lad_an + 8), by = px_y(PALA_Y - 30) + ((i - 1) / 7) * (lad_al + 8);

		if (x >= bx && x < bx + lad_an && y >= by && y < by + lad_al) {
			ed_elegido = i;
			pinta_editor();
		}
	}
}

/* -------------------------------------------------------------- los menús */
static char op_juego[400];
static const char *menus_op[2];

static void pon_menus(void)
{
	snprintf(op_juego, sizeof op_juego, "Partida nueva   F2|Pausa   P|-|%sLos 24 niveles|%sJugar mis niveles|Editor de niveles...|-|%sSonidos|-|Mejores puntuaciones...|-|Salir",
		 propios ? "" : "» ", propios ? "» " : "", snd_mudo ? "" : "» ");
	menus_op[0] = op_juego;
	menus_op[1] = "Cómo se juega...   F1|Los premios y los ladrillos...|-|Acerca de Rompemuros...";
	jg_menus("Juego|Ayuda", menus_op);
	jg_menu_grises(0, n_propios ? 0 : (1u << 4));
}

static void ayuda(void)
{
	jg_aviso("Cómo se juega",
		 "Devuelva la bola con la pala y rompa todos los ladrillos (los de metal no se rompen: no hacen falta). Si la bola cae por abajo, se pierde una vida.\n\n"
		 "La pala sigue a la flecha del ratón; también con izquierda y derecha (con Mayús, más deprisa) o la rueda. "
		 "Un clic o Espacio suelta la bola o dispara. P: pausa. F2: otra partida. F11: pantalla completa.\n\n"
		 "La bola sale más hacia el lado cuanto más lejos del centro de la pala la toca.");
}

static void ayuda_premios(void)
{
	char t[1200];
	int i, n = 0;

	n += snprintf(t + n, sizeof t - (size_t)n, "Los premios (caen al romper un ladrillo: cójalos con la pala):\n");
	for (i = 0; i < N_PREMIOS; i++)
		n += snprintf(t + n, sizeof t - (size_t)n, "  %c: %s\n", letra_premio[i], nombre_premio[i]);
	n += snprintf(t + n, sizeof t - (size_t)n, "\nLos ladrillos:\n");
	for (i = 1; i < N_LAD; i++)
		n += snprintf(t + n, sizeof t - (size_t)n, "  %s\n", nombre_lad[i]);
	jg_aviso("Los premios y los ladrillos", t);
}

static void pausa(int si)
{
	if (si && estado == E_JUEGA)
		estado = E_PAUSA;
	else if (!si && estado == E_PAUSA) {
		estado = E_JUEGA;
		teclas_olvidar();
	}
	jg_cambio = 1;
}

static void elegido(int m, int o)
{
	if (m == 1) {
		if (o == 0)
			ayuda();
		else if (o == 1)
			ayuda_premios();
		else if (o == 3)
			jg_aviso("Acerca de Rompemuros", "Rompemuros 1.0, para Hispa-Pentium.\nDe la familia de Hispa-DOS: diseño, niveles, dibujos y sonidos propios.\n\n"
				 "Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.");
		return;
	}
	switch (o) {
	case 0: nueva_partida(); break;
	case 1: pausa(estado == E_JUEGA); break;
	case 3: case 4:
		propios = o == 4 && n_propios;
		pon_menus();
		estado = E_LISTO;
		nivel = 1;
		empieza_nivel();
		pinta_todo();
		break;
	case 5: abre_editor(); break;
	case 7: snd_mudo = !snd_mudo; rec_poner_entero("Sonidos", !snd_mudo); rec_guardar(); pon_menus(); break;
	case 9: rec_ver("Mejores puntuaciones", "Puntos", REC_N, "puntos", 0, record_pos); break;
	case 11: jg_salir = 1; break;
	}
}

static void estado_serie(void)
{
	static const char *const e[] = { "listo", "juega", "pausa", "perdida", "nivel", "fin", "editor" };
	int i, b = 0;

	for (i = 0; i < MAX_BOLAS; i++)
		b += bolas[i].viva;
	serie("rompemuros: estado=%s puntos=%ld nivel=%d vidas=%d bolas=%d quedan=%d pala=%d sonido=%d\n", e[estado], puntos, nivel, vidas, b, quedan(),
	      (int)pala_x, snd_hay);
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
		if (estado == E_EDITOR) {
			if (s->a == T_ESC) {
				estado = E_LISTO;
				empieza_nivel();
				pinta_capa();
				pinta_todo();
			} else if (s->a == 'g' || s->a == 'G') {
				guarda_propio();
				pon_menus();
			} else if (s->a == 'j' || s->a == 'J') {
				guarda_propio();
				pon_menus();
				propios = 1;
				nivel = ed_nivel + 1;
				puntos = 0;
				vidas = 3;
				empieza_nivel();
				estado = E_JUEGA;
				pinta_todo();
			} else if (s->a >= '1' && s->a <= '9') {
				ed_elegido = (int)(s->a - '0');
				pinta_editor();
			}
			return;
		}
		switch (s->a) {
		case T_F2: nueva_partida(); return;
		case 0x13B: ayuda(); return;
		case 'p': case 'P': case T_ESC: pausa(estado == E_JUEGA); return;
		case 10: case 13: case ' ':
			if (estado == E_LISTO || estado == E_FIN)
				nueva_partida();
			else if (estado == E_PAUSA)
				pausa(0);
			return;
		}
		return;
	case EV_FOCO:
		if (!s->a)
			pausa(1);
		return;
	case EV_RATON_MUEVE:
		raton_x = (int)(int32_t)s->a;
		raton_activo = 1;
		return;
	case EV_RATON_BAJA: {
		int x = (int)(int32_t)s->a, y = (int)(int32_t)s->b;

		if (jg_clic_barra(x, y))
			return;
		if (estado == E_EDITOR) {
			editor_clic(x, y, (s->c & BOTON_DERECHO) != 0);
			return;
		}
		raton_x = x;
		raton_activo = 1;
		if (estado == E_LISTO || estado == E_FIN)
			nueva_partida();
		else if (estado == E_PAUSA)
			pausa(0);
		else if (estado == E_JUEGA) {
			lanza();
			dispara();
		}
		return;
	}
	case EV_RUEDA:
		raton_activo = 0;
		pala_x = LIMITA(pala_x + ((int8_t)(s->c & 0xFF) > 0 ? -24.0f : 24.0f), pala_an / 2, CAMPO_AN - pala_an / 2);
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
	if (jg_abrir("Rompemuros", MIN(an, 560), MIN(al, 600), 0)) {
		static const char m[] = "rompemuros: no hay escritorio (o no hay memoria para la ventana)\r\n";

		sc(SC_ESCRIBIR, (u32)m, sizeof m - 1, 0);
		return 1;
	}
	n = sc(SC_ARGUMENTOS, (u32)jg_args, sizeof jg_args - 1, 0);
	if (ES_ERROR(n))
		n = 0;
	jg_args[MIN(n, sizeof jg_args - 1)] = 0;
	for (i = 0; jg_args[i]; i++)
		if (jg_args[i] >= 'a' && jg_args[i] <= 'z')
			jg_args[i] = (char)(jg_args[i] - 32);
	azar_semilla((u32)arg_entero("/S=", (int)(jg_tics() * 2654435761u ^ (u32)rdtsc())));
	sola = arg_hay("/SOLA");
	rec_abrir("ROMPEMUR.REC");
	snd_mudo = !rec_entero("Sonidos", 1);
	lee_propios();
	snd_abrir();
	pon_menus();
	nivel = MAX(1, arg_entero("/NIVEL=", 1));
	empieza_nivel();
	tamano(LZ.an, LZ.al);
	serie("rompemuros: ventana %d x %d (escala %d %%); récords en %s\n", LZ.an, LZ.al, (int)(esc * 100), rec_ruta());
	if (sola)
		nueva_partida();
	return jg_correr(&j);
}
