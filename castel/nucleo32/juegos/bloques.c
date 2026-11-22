/* bloques.c - (JUEGOS) Bloques, el de las piezas que caen, para Hispa-Pentium.
 *
 * Por un pozo de 10 x 20 casillas caen piezas de azulejos que se mueven y se
 * giran para encajarlas; cada fila completa desaparece. Es el Bloques de
 * Hispa-386 (juegos/bloques.asm) hecho otra vez, con lo que se espera hoy
 * de un juego así y con aspecto propio: las siete piezas de cuatro casillas
 * con sus nombres de siempre en Hispa-DOS (Viga, Losa, Pico, Peldaño,
 * Escalón, Gancho y Garfio) o, con «Piezas variadas», las once de Hispa-386
 * (Barrita, Codo, Cruz y Cazo también, en un pozo de 11 y con las filas de
 * piedra); cada pieza con su color **y su trama** (rayas, puntos, rombo,
 * diagonales, cuadros...), para que se distingan sin mirar el color.
 *
 * - Girar con apoyos: si la pieza girada no cabe, se prueba un poco a un
 *   lado, al otro y un poco más arriba (hasta dos casillas a los lados para
 *   la Viga), y en los dos sentidos.
 * - Las tres siguientes a la vista, la reserva (C o Mayús: guardar la pieza
 *   y sacar la guardada, una vez por pieza), la sombra (dónde caería), bajar
 *   deprisa (abajo) y de golpe (Espacio).
 * - Al tocar fondo, medio segundo para moverla aún (15 veces como mucho).
 * - Las piezas salen de una «bolsa»: las siete (o las once) en un orden al
 *   azar, y otra bolsa: no hay rachas largas sin una pieza.
 * - Niveles: cada 10 filas, uno más (hasta el 15), y cae más deprisa.
 *   Puntos: 100, 300, 500 u 800 por una, dos, tres o cuatro filas a la vez
 *   (por el nivel), 50 más por cada vez seguida que se quitan filas, 1 por
 *   casilla bajada deprisa y 2 de golpe, y 1000 por el nivel si el pozo queda
 *   vacío («¡Limpio!»).
 * - Música propia (sintes.c: una melodía en la menor, en dos voces, escrita
 *   aquí) y sonidos sintetizados; los récords (los diez mejores de cada
 *   forma de jugar) en la carpeta del usuario (juego.c).
 *
 * Teclas: ← → mover (también A D y el teclado numérico), ↑ o X girar a la
 * derecha, Z o Ctrl girar a la izquierda, ↓ bajar, Espacio soltar, C o Mayús
 * reserva, P o Esc pausa, M música, F2 otra partida, F11 pantalla completa.
 * Ratón: clic gira, el botón derecho suelta; la rueda gira.
 * Para las pruebas: /S=n (la semilla), /PRUEBA (cuatro filas casi llenas y
 * la primera pieza, una Viga), /MIDE (lo que gasta) y F12 (el estado, por el
 * puerto serie).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "juego.h"

#define VISIBLES 20
#define OCULTAS 2
#define FILAS (VISIBLES + OCULTAS)
#define MAX_COLS 11
#define N_FORMAS 11
#define PIEDRA 11		/* (el tipo de las filas de piedra) */
#define NIVEL_MAX 15

enum { E_LISTO, E_JUEGA, E_PAUSA, E_QUITA, E_FIN };

struct forma { const char *nombre; int lado, n; signed char c[5][2]; };
static const struct forma formas[N_FORMAS] = {
	{ "Barrita", 3, 3, { {0,1}, {1,1}, {2,1} } },
	{ "Codo", 2, 3, { {0,0}, {0,1}, {1,1} } },
	{ "Losa", 2, 4, { {0,0}, {1,0}, {0,1}, {1,1} } },
	{ "Viga", 4, 4, { {0,1}, {1,1}, {2,1}, {3,1} } },
	{ "Gancho", 3, 4, { {0,1}, {1,1}, {2,1}, {2,0} } },
	{ "Garfio", 3, 4, { {0,0}, {0,1}, {1,1}, {2,1} } },
	{ "Peldaño", 3, 4, { {1,0}, {2,0}, {0,1}, {1,1} } },
	{ "Escalón", 3, 4, { {0,0}, {1,0}, {1,1}, {2,1} } },
	{ "Pico", 3, 4, { {1,0}, {0,1}, {1,1}, {2,1} } },
	{ "Cruz", 3, 5, { {1,0}, {0,1}, {1,1}, {2,1}, {1,2} } },
	{ "Cazo", 3, 5, { {0,0}, {2,0}, {0,1}, {1,1}, {2,1} } },
};
/* los colores (propios: ni los de otro juego ni los de nadie) y las tramas */
enum { T_RAYAS_H, T_PUNTOS, T_ROMBO, T_DIAG_SUBE, T_DIAG_BAJA, T_RAYAS_V, T_CUADROS, T_CRUZ, T_ARO, T_ASPA, T_MARCO, T_MOTAS };
static const u32 color_de[N_FORMAS + 1] = {
	0x4E7FA8, 0xB0703C, 0x2F5D9E, 0xC0573E, 0x56646E, 0x2E8C83,
	0x8A4F7D, 0x6E8B3D, 0xC9962A, 0xA2465A, 0x7B6A3A, 0x8C8778 };
static const u8 trama_de[N_FORMAS + 1] = {
	T_CRUZ, T_ARO, T_PUNTOS, T_RAYAS_H, T_RAYAS_V, T_CUADROS,
	T_DIAG_SUBE, T_DIAG_BAJA, T_ROMBO, T_ASPA, T_MARCO, T_MOTAS };
static const char *const nombre_trama[N_FORMAS] = {
	"una cruz", "un aro", "cuatro puntos", "rayas tumbadas", "rayas de pie", "cuadros",
	"rayas que suben", "rayas que bajan", "un rombo", "un aspa", "un marco" };

/* ---- el estado ---- */
static u8 pozo[FILAS][MAX_COLS];
static int cols = 10, variadas, estado = E_LISTO, antes_pausa;
static int tipo, giro, px, py, reserva = -1, ya_reservo;
static int cola[3 * N_FORMAS], n_cola;	/* las que vienen */
static long puntos;
static int filas_hechas, nivel = 1, nivel_ini = 1, piezas, seguidas, record_pos = -1;
static int quitando[FILAS], t_quita, t_caida, t_suelo, movidas_suelo, en_suelo;
static int das_dir, das_t, abajo_t, primera = 1;
static int musica = 1, sombra = 1, prueba;
static const char *aviso;		/* «¡Cuádruple!»... */
static int t_aviso;

/* ---- la pantalla ---- */
static int cel = 24, ox, oy, arriba, pan_iz, pan_de, pan_an, escala_t = 1;
static lienzo fondo, azul[N_FORMAS + 2];	/* + la sombra */

static void pinta_todo(void);
static void nueva_partida(void);

/* ------------------------------------------------------------- las piezas */
static void casillas(int t, int g, int *cx, int *cy)
{
	int i, n = formas[t].n, l = formas[t].lado;

	for (i = 0; i < n; i++) {
		int x = formas[t].c[i][0], y = formas[t].c[i][1], k;

		for (k = 0; k < (g & 3); k++) {
			int nx = l - 1 - y;

			y = x;
			x = nx;
		}
		cx[i] = x;
		cy[i] = y;
	}
}

static int cabe(int t, int g, int x, int y)
{
	int cx[5], cy[5], i;

	casillas(t, g, cx, cy);
	for (i = 0; i < formas[t].n; i++) {
		int c = x + cx[i], f = y + cy[i];

		if (c < 0 || c >= cols || f >= FILAS)
			return 0;
		if (f >= 0 && pozo[f][c])
			return 0;
	}
	return 1;
}

static int fila_sombra(void)
{
	int y = py;

	while (cabe(tipo, giro, px, y + 1))
		y++;
	return y;
}

static void llena_bolsa(void)
{
	int b[N_FORMAS], n = 0, i;

	if (variadas)
		for (i = 0; i < N_FORMAS; i++)
			b[n++] = i;
	else
		for (i = 2; i <= 8; i++)
			b[n++] = i;
	for (i = n - 1; i > 0; i--) {
		int k = azar_n(i + 1), t = b[i];

		b[i] = b[k];
		b[k] = t;
	}
	for (i = 0; i < n; i++)
		cola[n_cola++] = b[i];
}

static int saca_de_la_cola(void)
{
	int t = cola[0];

	memmove(cola, cola + 1, (size_t)(n_cola - 1) * sizeof cola[0]);
	n_cola--;
	if (n_cola < 4)
		llena_bolsa();
	return t;
}

/* ------------------------------------------------------------- el sonido */
static const struct efecto s_mover = { ON_CUADRADA, 900, 900, 12, 10, 1, 8 };
static const struct efecto s_girar = { ON_TRIANGULO, 520, 780, 45, 30, 2, 30 };
static const struct efecto s_encaja = { ON_RUIDO, 1800, 600, 60, 28, 1, 50 };
static const struct efecto s_golpe = { ON_SENO, 160, 70, 90, 55, 1, 80 };
static const struct efecto s_reserva = { ON_TRIANGULO, 660, 440, 70, 30, 3, 50 };
static const struct efecto s_fin = { ON_CUADRADA, 440, 110, 900, 30, 5, 600 };

static const char musica1[] =
	"T132 O5 L8 V34 @1 e4 a b > c4 < b a | g4 e f g4 a b | > c4 < b a g4 f e | f2 e2 |"
	"e4 a b > c4 d e | d4 c < b > c4 < b a | g4 e f g4 f e | a2. r4 |"
	"> c4 < g a b4 g e | f4 a > c < b4 a g | a4 e f g4 e c | d2 e2 |"
	"> c4 < g a b4 > c d | e4 d c < b4 a g+ | a4 e c d4 < b > d | < a2. > r4 |";
static const char musica2[] =
	"T132 L4 V52 @1 o2 a o3 e o2 a o3 e | o2 g o3 d o2 g o3 d | o2 a o3 e o2 a o3 e | o2 d a o2 e b |"
	"o2 a o3 e o2 a o3 e | o2 f o3 c o2 g o3 d | o2 e b o2 e g+ | o2 a o3 e o2 a2 |"
	"o2 c g o3 c o2 g | o2 f o3 c o2 f o3 c | o2 a o3 e o2 c g | o2 d a o2 e b |"
	"o2 c g o3 c o2 g | o2 e b o2 e b | o2 a o3 e o2 d a | o2 a o3 e o2 a2 |";

static void pon_musica(void)
{
	if (musica && estado == E_JUEGA)
		snd_musica(musica1, musica2, 1);
	else
		snd_musica_parar();
}

static void suena_filas(int n)
{
	static const int arpegio[5][5] = { {72, 76, 79, 0, 0}, {72, 76, 79, 84, 0}, {72, 76, 79, 84, 88},
		{67, 72, 76, 79, 84}, {64, 69, 72, 76, 81} };
	int i;

	for (i = 0; i < 5; i++)
		if (arpegio[MIN(n, 5) - 1][i])
			snd_nota_tras(ON_CAMPANA, nota_hz(arpegio[MIN(n, 5) - 1][i] + (n >= 4 ? 0 : -12)), 380, 34, i * 55);
	if (n >= 4)
		for (i = 0; i < 4; i++)
			snd_nota_tras(ON_CUADRADA, nota_hz(60 + i * 4), 120, 14, 300 + i * 70);
}

/* ------------------------------------------------------------- jugar */
static void avisa(const char *t)
{
	aviso = t;
	t_aviso = 150;
}

static void pieza_nueva(void)
{
	int cy[5], cx[5], i, m = 9;

	tipo = saca_de_la_cola();
	giro = 0;
	casillas(tipo, 0, cx, cy);
	for (i = 0; i < formas[tipo].n; i++)
		if (cy[i] < m)
			m = cy[i];
	px = (cols - formas[tipo].lado) / 2;
	py = 1 - m;		/* lo de arriba de la pieza, en la última fila escondida */
	ya_reservo = 0;
	t_caida = 0;
	t_suelo = 0;
	movidas_suelo = 0;
	en_suelo = 0;
	piezas++;
	if (!cabe(tipo, giro, px, py)) {
		/* no cabe: se acabó */
		estado = E_FIN;
		snd_musica_parar();
		snd_efecto(&s_fin);
		record_pos = rec_meter(variadas ? "Variadas" : "Puntos", REC_N, (int)puntos, 0);
		rec_guardar();
		serie("bloques: fin de la partida: %ld puntos, %d filas, nivel %d, %d piezas; récord %d\n", puntos, filas_hechas, nivel, piezas,
		      record_pos + 1);
	}
}

static int velocidad_ms(void)
{
	static const int ms[NIVEL_MAX + 1] = { 0, 800, 700, 610, 530, 450, 380, 310, 250, 200, 160, 125, 100, 80, 65, 50 };

	return ms[LIMITA(nivel, 1, NIVEL_MAX)];
}

static void sube_piedra(void)
{
	int f, c, hueco = azar_n(cols);

	for (f = 0; f < FILAS - 1; f++)
		memcpy(pozo[f], pozo[f + 1], sizeof pozo[0]);
	for (c = 0; c < cols; c++)
		pozo[FILAS - 1][c] = c == hueco ? 0 : PIEDRA + 1;
	if (!cabe(tipo, giro, px, py) && py > 0)
		py--;
}

static void encaja(void)
{
	int cx[5], cy[5], i, n = 0, f, arriba_del_todo = 1;

	casillas(tipo, giro, cx, cy);
	for (i = 0; i < formas[tipo].n; i++) {
		int fy = py + cy[i];

		if (fy >= 0)
			pozo[fy][px + cx[i]] = (u8)(tipo + 1);
		if (fy >= OCULTAS)
			arriba_del_todo = 0;
	}
	snd_efecto(&s_encaja);
	for (f = 0; f < FILAS; f++) {
		int c, llena = 1;

		for (c = 0; c < cols; c++)
			if (!pozo[f][c])
				llena = 0;
		quitando[f] = llena;
		n += llena;
	}
	if (n) {
		static const int premio[6] = { 0, 100, 300, 500, 800, 1200 };

		puntos += (long)premio[MIN(n, 5)] * nivel + (long)(seguidas > 0 ? 50 * seguidas * nivel : 0);
		seguidas++;
		filas_hechas += n;
		estado = E_QUITA;
		t_quita = 0;
		suena_filas(n);
		avisa(n == 1 ? 0 : n == 2 ? "¡Doble!" : n == 3 ? "¡Triple!" : n == 4 ? "¡Cuádruple!" : "¡Quíntuple!");
		if (seguidas > 1 && n < 2)
			avisa("¡Seguidas!");
		serie("bloques: %d filas a la vez (llevo %d); %ld puntos\n", n, filas_hechas, puntos);
	} else {
		seguidas = 0;
		if (arriba_del_todo) {
			tipo = cola[0];
			estado = E_FIN;
			snd_musica_parar();
			snd_efecto(&s_fin);
			record_pos = rec_meter(variadas ? "Variadas" : "Puntos", REC_N, (int)puntos, 0);
			serie("bloques: fin de la partida: %ld puntos, %d filas, nivel %d, %d piezas; récord %d\n", puntos, filas_hechas, nivel,
			      piezas, record_pos + 1);
			return;
		}
		if (variadas && nivel >= 5 && piezas % 10 == 0)
			sube_piedra();
		pieza_nueva();
	}
}

static void acaba_de_quitar(void)
{
	int f, k, vacio = 1, nv;

	for (f = 0; f < FILAS; f++)
		if (quitando[f]) {
			for (k = f; k > 0; k--)
				memcpy(pozo[k], pozo[k - 1], sizeof pozo[0]);
			memset(pozo[0], 0, sizeof pozo[0]);
			quitando[f] = 0;
		}
	for (f = 0; f < FILAS; f++)
		for (k = 0; k < cols; k++)
			if (pozo[f][k])
				vacio = 0;
	if (vacio) {
		puntos += 1000L * nivel;
		avisa("¡Limpio!");
		snd_nota_tras(ON_CAMPANA, nota_hz(96), 600, 30, 0);
	}
	nv = MIN(NIVEL_MAX, nivel_ini + filas_hechas / 10);
	if (nv > nivel) {
		int i;

		nivel = nv;
		avisa("¡Nivel nuevo!");
		for (i = 0; i < 6; i++)
			snd_nota_tras(ON_TRIANGULO, nota_hz(69 + i * 2), 90, 26, 260 + i * 60);
	}
	estado = E_JUEGA;
	if (variadas && nivel >= 5 && piezas % 10 == 0)
		sube_piedra();
	pieza_nueva();
}

static int mueve(int dx, int dy)
{
	if (!cabe(tipo, giro, px + dx, py + dy))
		return 0;
	px += dx;
	py += dy;
	if (en_suelo && movidas_suelo < 15 && dx) {
		t_suelo = 0;
		movidas_suelo++;
	}
	return 1;
}

static void gira(int sentido)
{
	static const signed char apoyos[][2] = { {0,0}, {-1,0}, {1,0}, {0,-1}, {-1,-1}, {1,-1}, {-2,0}, {2,0}, {0,1} };
	int g = (giro + (sentido > 0 ? 1 : 3)) & 3, i, n = (int)(sizeof apoyos / sizeof apoyos[0]);

	if (tipo == 2)		/* la Losa no gira (es igual) */
		return;
	for (i = 0; i < n; i++) {
		int dx = apoyos[i][0], dy = apoyos[i][1];

		if ((dx == 2 || dx == -2) && formas[tipo].lado < 4)
			continue;
		if (cabe(tipo, g, px + dx, py + dy)) {
			px += dx;
			py += dy;
			giro = g;
			snd_efecto(&s_girar);
			if (en_suelo && movidas_suelo < 15) {
				t_suelo = 0;
				movidas_suelo++;
			}
			return;
		}
	}
}

static void suelta(void)
{
	int n = 0;

	while (cabe(tipo, giro, px, py + 1)) {
		py++;
		n++;
	}
	puntos += 2 * n;
	snd_efecto(&s_golpe);
	encaja();
}

static void guarda_reserva(void)
{
	int t;

	if (ya_reservo)
		return;
	snd_efecto(&s_reserva);
	if (reserva < 0) {
		reserva = tipo;
		pieza_nueva();
	} else {
		t = reserva;
		reserva = tipo;
		cola[n_cola] = 0;
		memmove(cola + 1, cola, (size_t)n_cola * sizeof cola[0]);
		cola[0] = t;
		n_cola++;
		piezas--;
		pieza_nueva();
	}
	ya_reservo = 1;
}

static void nueva_partida(void)
{
	memset(pozo, 0, sizeof pozo);
	cols = variadas ? 11 : 10;
	n_cola = 0;
	llena_bolsa();
	puntos = 0;
	filas_hechas = 0;
	nivel = nivel_ini;
	piezas = 0;
	seguidas = 0;
	reserva = -1;
	record_pos = -1;
	aviso = 0;
	memset(quitando, 0, sizeof quitando);
	if (prueba) {
		/* cuatro filas llenas menos la columna de la derecha, y una Viga */
		int f, c;

		for (f = FILAS - 4; f < FILAS; f++)
			for (c = 0; c < cols - 1; c++)
				pozo[f][c] = (u8)(1 + ((c + f) % 7) + 2);
		cola[0] = 3;
	}
	estado = E_JUEGA;
	das_dir = 0;
	pieza_nueva();
	pon_musica();
	if (!cel)
		return;
	serie("bloques: partida nueva (%s, nivel %d)\n", variadas ? "piezas variadas" : "las siete piezas", nivel);
	pinta_todo();
}

static void pausa(int si)
{
	if (si && estado == E_JUEGA) {
		antes_pausa = estado;
		estado = E_PAUSA;
		snd_musica_parar();
	} else if (!si && estado == E_PAUSA) {
		estado = E_JUEGA;
		pon_musica();
	}
	jg_cambio = 1;
}

static void pulso(void)
{
	int izq, der;

	if (t_aviso > 0 && --t_aviso == 0)
		aviso = 0;
	if (estado == E_QUITA) {
		if (++t_quita >= 28)
			acaba_de_quitar();
		jg_cambio = 1;
		return;
	}
	if (estado != E_JUEGA)
		return;
	if (primera) {
		/* (lo pulsado para empezar no cuenta) */
		primera = 0;
		teclas_olvidar();
		return;
	}
	/* girar y la reserva: al pulsar */
	if (tecla_vino(K_ARRIBA) || tecla_vino(K_X) || tecla_vino(K_W) || tecla_vino(K_NUM_ARRIBA))
		gira(1);
	if (tecla_vino(K_Z) || tecla_vino(K_CTRL) || tecla_vino(K_CTRL_D))
		gira(-1);
	if (tecla_vino(K_C) || tecla_vino(K_MAYUS) || tecla_vino(K_MAYUS_D)) {
		guarda_reserva();
		if (estado != E_JUEGA)
			return;
	}
	if (tecla_vino(K_ESPACIO)) {
		suelta();
		jg_cambio = 1;
		return;
	}
	/* mover: al pulsar, y si se deja pulsado, tras 160 ms, cada 45 */
	izq = tecla(K_IZQ) || tecla(K_A) || tecla(K_NUM_IZQ);
	der = tecla(K_DER) || tecla(K_D) || tecla(K_NUM_DER);
	if (tecla_vino(K_IZQ) || tecla_vino(K_A) || tecla_vino(K_NUM_IZQ)) {
		if (mueve(-1, 0))
			snd_efecto(&s_mover);
		das_dir = -1;
		das_t = 0;
	} else if (tecla_vino(K_DER) || tecla_vino(K_D) || tecla_vino(K_NUM_DER)) {
		if (mueve(1, 0))
			snd_efecto(&s_mover);
		das_dir = 1;
		das_t = 0;
	} else if ((das_dir < 0 && izq) || (das_dir > 0 && der)) {
		das_t += 10;
		if (das_t >= 160) {
			das_t -= 45;
			if (mueve(das_dir, 0))
				snd_efecto(&s_mover);
		}
	} else
		das_dir = izq ? -1 : der ? 1 : 0;
	/* caer: lo del nivel; o, con la flecha abajo, una casilla cada 30 ms */
	if (tecla(K_ABAJO) || tecla(K_S) || tecla(K_NUM_ABAJO)) {
		abajo_t += 10;
		if (abajo_t >= 30 || tecla_vino(K_ABAJO) || tecla_vino(K_S) || tecla_vino(K_NUM_ABAJO)) {
			abajo_t = 0;
			if (mueve(0, 1)) {
				puntos++;
				t_caida = 0;
			}
		}
	} else
		abajo_t = 30;
	t_caida += 10;
	if (t_caida >= velocidad_ms()) {
		t_caida = 0;
		mueve(0, 1);
	}
	en_suelo = !cabe(tipo, giro, px, py + 1);
	if (en_suelo) {
		t_suelo += 10;
		if (t_suelo >= 500 || movidas_suelo >= 15)
			encaja();
	} else
		t_suelo = 0;
	jg_cambio = 1;
}

/* ------------------------------------------------------------- pintar */
static void trama(lienzo *l, int x, int y, int s, int t, u32 c)
{
	u32 cl = aclara(c, 150), os = aclara(c, -120);
	int g = MAX(1, s / 12), i, m = s / 2, k;

	switch (t) {
	case T_RAYAS_H:
		for (i = 1; i <= 3; i++)
			lz_rect(l, x + s / 5, y + i * s / 4 - g / 2, s - 2 * (s / 5), g, os);
		break;
	case T_RAYAS_V:
		for (i = 1; i <= 3; i++)
			lz_rect(l, x + i * s / 4 - g / 2, y + s / 5, g, s - 2 * (s / 5), os);
		break;
	case T_PUNTOS:
		for (i = 0; i < 2; i++)
			for (k = 0; k < 2; k++)
				lz_circulo_suave(l, (float)x + s * (0.32f + 0.36f * i), (float)y + s * (0.32f + 0.36f * k), s * 0.09f + 0.6f, cl);
		break;
	case T_ROMBO: {
		float q[8] = { (float)x + m, (float)y + s * 0.2f, (float)x + s * 0.8f, (float)y + m,
			       (float)x + m, (float)y + s * 0.8f, (float)x + s * 0.2f, (float)y + m };
		float r[8] = { (float)x + m, (float)y + s * 0.33f, (float)x + s * 0.67f, (float)y + m,
			       (float)x + m, (float)y + s * 0.67f, (float)x + s * 0.33f, (float)y + m };
		lz_poligono(l, q, 4, os);
		lz_poligono(l, r, 4, cl);
		break;
	}
	case T_DIAG_SUBE:
	case T_DIAG_BAJA:
		for (i = -1; i <= 1; i++) {
			float a = (float)s * 0.3f * (float)i, w = (float)g * 0.8f + 0.4f;
			float q[8];

			if (t == T_DIAG_SUBE) {
				float z[8] = { x + s * 0.18f + a - w, y + s * 0.82f, x + s * 0.18f + a + w, y + s * 0.82f,
					       x + s * 0.82f + a + w, y + s * 0.18f, x + s * 0.82f + a - w, y + s * 0.18f };
				memcpy(q, z, sizeof q);
			} else {
				float z[8] = { x + s * 0.18f + a - w, y + s * 0.18f, x + s * 0.18f + a + w, y + s * 0.18f,
					       x + s * 0.82f + a + w, y + s * 0.82f, x + s * 0.82f + a - w, y + s * 0.82f };
				memcpy(q, z, sizeof q);
			}
			/* (recortado al azulejo: lo de fuera no se pinta) */
			{
				lienzo tz;

				lz_trozo(&tz, l, x + s / 6, y + s / 6, s - 2 * (s / 6), s - 2 * (s / 6));
				for (k = 0; k < 4; k++) {
					q[2 * k] -= (float)(x + s / 6);
					q[2 * k + 1] -= (float)(y + s / 6);
				}
				lz_poligono(&tz, q, 4, os);
			}
		}
		break;
	case T_CUADROS:
		for (i = 0; i < 3; i++)
			for (k = 0; k < 3; k++)
				if ((i + k) % 2 == 0)
					lz_rect(l, x + s / 6 + i * (s - s / 3) / 3, y + s / 6 + k * (s - s / 3) / 3, (s - s / 3) / 3, (s - s / 3) / 3, os);
		break;
	case T_CRUZ:
		lz_rect(l, x + m - g, y + s / 5, 2 * g, s - 2 * (s / 5), cl);
		lz_rect(l, x + s / 5, y + m - g, s - 2 * (s / 5), 2 * g, cl);
		break;
	case T_ARO:
		lz_circulo_suave(l, (float)x + m, (float)y + m, s * 0.3f, cl);
		lz_circulo_suave(l, (float)x + m, (float)y + m, s * 0.3f - 1.0f - g, c);
		break;
	case T_ASPA:
		lz_linea_gruesa(l, x + s / 4, y + s / 4, x + s - s / 4, y + s - s / 4, 2 * g, cl);
		lz_linea_gruesa(l, x + s - s / 4, y + s / 4, x + s / 4, y + s - s / 4, 2 * g, cl);
		break;
	case T_MARCO:
		lz_marco(l, x + s / 4, y + s / 4, s - 2 * (s / 4), s - 2 * (s / 4), cl);
		lz_marco(l, x + s / 4 + 1, y + s / 4 + 1, s - 2 * (s / 4) - 2, s - 2 * (s / 4) - 2, cl);
		break;
	case T_MOTAS: {
		static const u8 mt[8][2] = { {3,4}, {9,2}, {6,8}, {12,6}, {4,12}, {10,11}, {13,13}, {7,14} };

		for (i = 0; i < 8; i++)
			lz_rect(l, x + mt[i][0] * s / 16, y + mt[i][1] * s / 16, MAX(1, s / 10), MAX(1, s / 10), aclara(c, i % 2 ? 90 : -90));
		break;
	}
	}
}

static void azulejo(lienzo *l, int x, int y, int s, int t)
{
	u32 c = color_de[t];
	int b = MAX(1, s / 10);

	lz_degradado(l, x, y, s, s, aclara(c, 40), aclara(c, -30));
	lz_rect(l, x, y, s, b, aclara(c, 120));
	lz_rect(l, x, y, b, s, aclara(c, 90));
	lz_rect(l, x, y + s - b, s, b, aclara(c, -110));
	lz_rect(l, x + s - b, y, b, s, aclara(c, -80));
	trama(l, x, y, s, trama_de[t], c);
}

static void hace_azulejos(void)
{
	int t;

	for (t = 0; t < N_FORMAS + 2; t++) {
		lz_libre(&azul[t]);
		lz_nuevo(&azul[t], cel, cel);
		if (!azul[t].p)
			continue;
		if (t <= N_FORMAS)
			azulejo(&azul[t], 0, 0, cel, t);
	}
}

/* el fondo: azulejos de suelo en azul muy suave y, en medio, el pozo */
static void hace_fondo(void)
{
	int x, y, l = 40;

	lz_libre(&fondo);
	lz_nuevo(&fondo, LZ.an, LZ.al);
	if (!fondo.p)
		return;
	lz_degradado(&fondo, 0, 0, fondo.an, fondo.al, 0x1B2A44, 0x0F1A2C);
	for (y = arriba; y < fondo.al; y += l)
		for (x = 0; x < fondo.an; x += l) {
			float q[8] = { (float)x + l / 2, (float)y + 4, (float)x + l - 4, (float)y + l / 2,
				       (float)x + l / 2, (float)y + l - 4, (float)x + 4, (float)y + l / 2 };

			lz_marco(&fondo, x, y, l, l, 0x22344F);
			lz_poligono_mezcla(&fondo, q, 4, 0x2C4366, 110);
		}
	/* el pozo, hundido */
	lz_rect(&fondo, ox - 6, oy - 6, cols * cel + 12, VISIBLES * cel + 12, 0x0A1220);
	lz_rect(&fondo, ox - 4, oy - 4, cols * cel + 8, VISIBLES * cel + 8, 0x8FA3B8);
	lz_rect(&fondo, ox - 2, oy - 2, cols * cel + 4, VISIBLES * cel + 4, 0x3A4C66);
	lz_rect(&fondo, ox, oy, cols * cel, VISIBLES * cel, 0x111C2E);
	for (y = 0; y < VISIBLES; y++)
		for (x = 0; x < cols; x++)
			lz_rect(&fondo, ox + x * cel + cel / 2, oy + y * cel + cel / 2, 1, 1, 0x34465F);
	/* los paneles */
	lz_redondo(&fondo, pan_iz, oy - 4, pan_an, VISIBLES * cel + 8, 10, 0x22344F);
	lz_redondo(&fondo, pan_de, oy - 4, pan_an, VISIBLES * cel + 8, 10, 0x22344F);
}

static void tamano(int an, int al)
{
	int alto, w;

	arriba = jg_completa ? 0 : ALTO_BARRA;
	alto = al - arriba - 24;
	cel = alto / VISIBLES;
	w = (an - 48) / (cols + 12);
	if (w < cel)
		cel = w;
	cel = LIMITA(cel, 8, 64);
	pan_an = cel * 5 + 8;
	w = cols * cel + 2 * pan_an + 32;
	ox = (an - w) / 2 + pan_an + 16;
	oy = arriba + (al - arriba - VISIBLES * cel) / 2;
	pan_iz = ox - 16 - pan_an;
	pan_de = ox + cols * cel + 16;
	escala_t = cel >= 30 ? 2 : 1;
	hace_azulejos();
	hace_fondo();
	pinta_todo();
}

/* una pieza pequeña (en una caja de ancho «an»), con su nombre debajo */
static void pinta_mini(int x, int y, int an, int t, int s, int nombre, int gris)
{
	int cx[5], cy[5], i, mx = 9, Mx = -9, my = 9, My = -9, w, h;

	if (t < 0)
		return;
	casillas(t, 0, cx, cy);
	for (i = 0; i < formas[t].n; i++) {
		mx = MIN(mx, cx[i]); Mx = MAX(Mx, cx[i]);
		my = MIN(my, cy[i]); My = MAX(My, cy[i]);
	}
	w = (Mx - mx + 1) * s;
	h = (My - my + 1) * s;
	for (i = 0; i < formas[t].n; i++) {
		int xx = x + (an - w) / 2 + (cx[i] - mx) * s, yy = y + (2 * s - h) / 2 + (cy[i] - my) * s;

		azulejo(&LZ, xx, yy, s, t);
		if (gris)
			lz_mezcla(&LZ, xx, yy, s, s, 0x22344F, 150);
	}
	if (nombre)
		lz_texto_centro(&LZ, x + an / 2, y + 2 * s + 6, formas[t].nombre, 0xC8D4E4, 1, 0);
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

static void pinta_paneles(void)
{
	char t[32];
	int s = MAX(6, cel * 3 / 4), y, x = pan_iz + 8, a = pan_an - 16, h = 16 * escala_t + 4;
	struct record r[1];

	lz_copia(&LZ, pan_iz, oy - 4, &fondo, pan_iz, oy - 4, pan_an, VISIBLES * cel + 8);
	lz_copia(&LZ, pan_de, oy - 4, &fondo, pan_de, oy - 4, pan_an, VISIBLES * cel + 8);
	/* la izquierda: la reserva y la cuenta */
	y = oy + 6;
	lz_texto_centro(&LZ, pan_iz + pan_an / 2, y, "Reserva", 0xFFE3A0, escala_t, 1);
	y += h + 4;
	lz_redondo(&LZ, x, y, a, 2 * s + 26, 6, 0x15223A);
	pinta_mini(x, y + 4, a, reserva, s, 1, ya_reservo);
	y += 2 * s + 26 + 12;
#define DATO(rotulo, valor) do { lz_texto(&LZ, x, y, rotulo, 0x9FB2CC, 1); y += 17; \
		lz_negrita(&LZ, x, y, valor, 0xFFFFFF, escala_t); y += h + 8; } while (0)
	con_puntos(t, puntos);
	DATO("Puntos", t);
	snprintf(t, sizeof t, "%d", filas_hechas);
	DATO("Filas", t);
	snprintf(t, sizeof t, "%d", nivel);
	DATO("Nivel", t);
	if (rec_tabla(variadas ? "Variadas" : "Puntos", r, 1)) {
		con_puntos(t, MAX((long)r[0].puntos, puntos));
		DATO("Récord", t);
	}
	/* la derecha: las tres siguientes */
	x = pan_de + 8;
	y = oy + 6;
	lz_texto_centro(&LZ, pan_de + pan_an / 2, y, "Siguientes", 0xFFE3A0, escala_t, 1);
	y += h + 4;
	lz_redondo(&LZ, x, y, a, 2 * s + 26, 6, 0x15223A);
	if (estado != E_LISTO)
		pinta_mini(x, y + 4, a, cola[0], s, 1, 0);
	y += 2 * s + 26 + 10;
	{
		int s2 = MAX(5, s * 2 / 3), k;

		for (k = 1; k <= 2; k++) {
			lz_redondo(&LZ, x, y, a, 2 * s2 + 24, 6, 0x15223A);
			if (estado != E_LISTO)
				pinta_mini(x, y + 4, a, cola[k], s2, 1, 0);
			y += 2 * s2 + 24 + 8;
		}
	}
	y = oy + VISIBLES * cel - 5 * 17;
	lz_texto(&LZ, x, y, musica ? "M: música (sí)" : "M: música (no)", 0x7F93AF, 1);
	lz_texto(&LZ, x, y + 17, "P: pausa", 0x7F93AF, 1);
	lz_texto(&LZ, x, y + 34, "F2: otra partida", 0x7F93AF, 1);
	lz_texto(&LZ, x, y + 51, variadas ? "Piezas variadas" : "Siete piezas", 0x7F93AF, 1);
	jg_sucio(pan_iz, oy - 4, pan_an, VISIBLES * cel + 8);
	jg_sucio(pan_de, oy - 4, pan_an, VISIBLES * cel + 8);
}

static void pinta_pozo(void)
{
	int f, c, cx[5], cy[5], i, bw = cols * cel, bh = VISIBLES * cel;

	lz_copia(&LZ, ox, oy, &fondo, ox, oy, bw, bh);
	for (f = OCULTAS; f < FILAS; f++)
		for (c = 0; c < cols; c++)
			if (pozo[f][c]) {
				int x = ox + c * cel, y = oy + (f - OCULTAS) * cel;

				lz_copia(&LZ, x, y, &azul[pozo[f][c] - 1], 0, 0, cel, cel);
				if (estado == E_QUITA && quitando[f] && (t_quita / 4) % 2 == 0)
					lz_mezcla(&LZ, x, y, cel, cel, 0xFFFFFF, 200);
			}
	if (estado == E_JUEGA || estado == E_PAUSA) {
		casillas(tipo, giro, cx, cy);
		if (sombra && estado == E_JUEGA) {
			int sy = fila_sombra();

			for (i = 0; i < formas[tipo].n; i++) {
				int f2 = sy + cy[i];
				int x = ox + (px + cx[i]) * cel, y = oy + (f2 - OCULTAS) * cel;

				if (f2 < OCULTAS)
					continue;
				lz_mezcla(&LZ, x + 1, y + 1, cel - 2, cel - 2, color_de[tipo], 70);
				lz_marco(&LZ, x + 1, y + 1, cel - 2, cel - 2, aclara(color_de[tipo], 60));
				lz_marco(&LZ, x + 2, y + 2, cel - 4, cel - 4, aclara(color_de[tipo], 20));
			}
		}
		for (i = 0; i < formas[tipo].n; i++) {
			int f2 = py + cy[i];

			if (f2 < OCULTAS)
				continue;
			lz_copia(&LZ, ox + (px + cx[i]) * cel, oy + (f2 - OCULTAS) * cel, &azul[tipo], 0, 0, cel, cel);
		}
	}
	if (aviso && estado == E_QUITA) {
		lz_texto_sombra(&LZ, ox + (bw - ancho_texto(aviso, 2)) / 2, oy + bh / 3, aviso, 0xFFE3A0, 0x101010, 2);
	} else if (aviso && estado == E_JUEGA && t_aviso > 60)
		lz_texto_sombra(&LZ, ox + (bw - ancho_texto(aviso, 2)) / 2, oy + bh / 3, aviso, 0xFFE3A0, 0x101010, 2);
	if (estado == E_LISTO)
		jg_cartel(&LZ, ox + 8, oy, bw - 16, bh, "Bloques",
			  "Encaje las piezas que caen: cada fila completa desaparece.\n\nIntro o Espacio: empezar.\nF1: cómo se juega.");
	else if (estado == E_PAUSA)
		jg_cartel(&LZ, ox + 8, oy, bw - 16, bh, "Pausa", "P o Intro para seguir.");
	else if (estado == E_FIN) {
		char t[200], p[20];

		con_puntos(p, puntos);
		if (record_pos >= 0)
			snprintf(t, sizeof t, "%s puntos y %d filas.\n¡Récord: el %d.º de la lista!\n\nIntro: otra partida.", p, filas_hechas, record_pos + 1);
		else
			snprintf(t, sizeof t, "%s puntos y %d filas.\n\nIntro: otra partida.", p, filas_hechas);
		jg_cartel(&LZ, ox + 8, oy, bw - 16, bh, "Fin de la partida", t);
	}
	jg_sucio(ox, oy, bw, bh);
}

static void pinta_todo(void)
{
	if (!fondo.p)
		return;
	lz_copia(&LZ, 0, 0, &fondo, 0, 0, LZ.an, LZ.al);
	if (!jg_completa)
		jg_pinta_barra();
	pinta_pozo();
	pinta_paneles();
	jg_todo_sucio();
}

static void pinta(void)
{
	static long p_antes = -1;
	static int r_antes = -2, c0 = -1, e_antes = -1, n_antes = -1;

	pinta_pozo();
	if (p_antes != puntos || r_antes != reserva + 16 * ya_reservo || c0 != cola[0] || e_antes != estado || n_antes != nivel) {
		p_antes = puntos;
		r_antes = reserva + 16 * ya_reservo;
		c0 = cola[0];
		e_antes = estado;
		n_antes = nivel;
		pinta_paneles();
	}
}

/* ------------------------------------------------------------- los menús */
static char op_juego[400];
static const char *menus_op[2];

static void pon_menus(void)
{
	snprintf(op_juego, sizeof op_juego,
		 "Partida nueva   F2|Pausa   P|-|Empezar en el nivel...|%sLas siete piezas|%sPiezas variadas (de Hispa-386)|-|%sMúsica   M|%sSonidos|%sSombra de la pieza|-|Mejores puntuaciones...|-|Salir",
		 variadas ? "" : "» ", variadas ? "» " : "", musica ? "» " : "", snd_mudo ? "" : "» ", sombra ? "» " : "");
	menus_op[0] = op_juego;
	menus_op[1] = "Cómo se juega...   F1|Las piezas...|-|Acerca de Bloques...";
	jg_menus("Juego|Ayuda", menus_op);
}

static void guarda_ajustes(void)
{
	rec_poner_entero("Nivel", nivel_ini);
	rec_poner("Piezas", variadas ? "variadas" : "siete");
	rec_poner_entero("Musica", musica);
	rec_poner_entero("Sonidos", !snd_mudo);
	rec_poner_entero("Sombra", sombra);
	rec_guardar();
	pon_menus();
}

static void ayuda(void)
{
	jg_aviso("Cómo se juega",
		 "Encaje las piezas que caen: cada fila completa desaparece y lo de encima baja. Se acaba cuando ya no cabe una pieza nueva.\n\n"
		 "Izquierda y derecha: mover. Arriba o X: girar (Z o Control, al otro lado). Abajo: bajar deprisa. Espacio: soltar de golpe. "
		 "C o Mayús: guardar la pieza en la reserva (y sacar la guardada). P: pausa. M: música. F11: pantalla completa.\n\n"
		 "Ratón: un clic gira, el botón derecho suelta, la rueda gira.\n\n"
		 "Puntos: 100, 300, 500 u 800 por una, dos, tres o cuatro filas a la vez (por el nivel), y más si se quitan seguidas. "
		 "Cada 10 filas, un nivel más: cae más deprisa.");
}

static void piezas_ayuda(void)
{
	char t[1200];
	int i, n = 0;

	n += snprintf(t + n, sizeof t - (size_t)n, "Cada pieza lleva su color y su dibujo:\n\n");
	for (i = variadas ? 0 : 2; i <= (variadas ? N_FORMAS - 1 : 8); i++)
		n += snprintf(t + n, sizeof t - (size_t)n, "%s: %s\n", formas[i].nombre, nombre_trama[i]);
	if (variadas)
		n += snprintf(t + n, sizeof t - (size_t)n, "Piedra: motas (desde el nivel 5 sube una fila con un hueco cada 10 piezas)\n");
	jg_aviso("Las piezas", t);
}

static void elegido(int m, int o)
{
	if (m == 1) {
		if (o == 0)
			ayuda();
		else if (o == 1)
			piezas_ayuda();
		else if (o == 3)
			jg_aviso("Acerca de Bloques", "Bloques 1.0, para Hispa-Pentium.\nDe la familia de Hispa-DOS: diseño, piezas, dibujos, música y sonidos propios.\n\n"
				 "Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.");
		jg_cambio = 1;
		return;
	}
	switch (o) {
	case 0: nueva_partida(); break;
	case 1: pausa(estado == E_JUEGA); break;
	case 3: {
		static const char *const nv[] = { "Nivel 1", "Nivel 3", "Nivel 5", "Nivel 7", "Nivel 10" };
		static const int v[] = { 1, 3, 5, 7, 10 };
		int k, a = 0, r;

		for (k = 0; k < 5; k++)
			if (v[k] == nivel_ini)
				a = k;
		r = jg_elige("Empezar en el nivel", nv, 5, a);
		if (r >= 0) {
			nivel_ini = v[r];
			guarda_ajustes();
		}
		break;
	}
	case 4: case 5:
		if (variadas != (o == 5)) {
			variadas = o == 5;
			guarda_ajustes();
			cols = variadas ? 11 : 10;
			tamano(LZ.an, LZ.al);
			estado = E_LISTO;
		}
		break;
	case 7: musica = !musica; pon_musica(); guarda_ajustes(); break;
	case 8: snd_mudo = !snd_mudo; guarda_ajustes(); break;
	case 9: sombra = !sombra; guarda_ajustes(); break;
	case 11: rec_ver(variadas ? "Mejores puntuaciones (piezas variadas)" : "Mejores puntuaciones", variadas ? "Variadas" : "Puntos", REC_N, "puntos", 0, record_pos); break;
	case 13: jg_salir = 1; break;
	}
	pinta_todo();
}

static void estado_serie(void)
{
	static const char *const e[] = { "listo", "juega", "pausa", "quita", "fin" };

	serie("bloques: estado=%s puntos=%ld filas=%d nivel=%d piezas=%d pieza=%s reserva=%s siguiente=%s sonido=%d\n", e[estado], puntos,
	      filas_hechas, nivel, piezas, formas[tipo].nombre, reserva >= 0 ? formas[reserva].nombre : "-", formas[cola[0]].nombre, snd_hay);
}

static void suceso(const struct suceso *s)
{
	int m, o;

	switch (s->clase) {
	case EV_TECLA:
		switch (s->a) {
		case 0x158: estado_serie(); return;
		case T_F2: nueva_partida(); return;
		case 0x13B: ayuda(); return;	/* F1 */
		case 'p': case 'P': case T_ESC: pausa(estado == E_JUEGA); return;
		case 'm': case 'M': musica = !musica; pon_musica(); guarda_ajustes(); jg_cambio = 1; pinta_paneles(); return;
		case 10: case 13: case ' ':
			if (estado == E_LISTO || estado == E_FIN) {
				primera = 1;
				nueva_partida();
			} else if (estado == E_PAUSA)
				pausa(0);
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
		if (estado == E_JUEGA) {
			if (s->c & BOTON_DERECHO)
				suelta();
			else
				gira(1);
			jg_cambio = 1;
		} else if (estado == E_LISTO || estado == E_FIN) {
			primera = 1;
			nueva_partida();
		} else if (estado == E_PAUSA)
			pausa(0);
		return;
	case EV_RUEDA:
		if (estado == E_JUEGA) {
			gira((int8_t)(s->c & 0xFF) > 0 ? 1 : -1);
			jg_cambio = 1;
		}
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
	const char *p;

	jg_tamano_maximo(&an, &al);
	if (jg_abrir("Bloques", MIN(an, 600), MIN(al, 560), 0)) {
		static const char m[] = "bloques: no hay escritorio (o no hay memoria para la ventana)\r\n";

		sc(SC_ESCRIBIR, (u32)m, sizeof m - 1, 0);
		return 1;
	}
	{
		u32 n = sc(SC_ARGUMENTOS, (u32)jg_args, sizeof jg_args - 1, 0), i;

		if (ES_ERROR(n))
			n = 0;
		jg_args[MIN(n, sizeof jg_args - 1)] = 0;
		for (i = 0; jg_args[i]; i++)
			if (jg_args[i] >= 'a' && jg_args[i] <= 'z')
				jg_args[i] = (char)(jg_args[i] - 32);
	}
	azar_semilla((u32)arg_entero("/S=", (int)(jg_tics() * 2654435761u ^ (u32)rdtsc())));
	prueba = arg_hay("/PRUEBA");
	rec_abrir("BLOQUES.REC");
	nivel_ini = LIMITA(rec_entero("Nivel", 1), 1, 10);
	p = rec_texto("Piezas");
	variadas = p && !strcmp(p, "variadas");
	musica = rec_entero("Musica", 1);
	snd_mudo = !rec_entero("Sonidos", 1);
	sombra = rec_entero("Sombra", 1);
	cols = variadas ? 11 : 10;
	snd_abrir();
	pon_menus();
	n_cola = 0;
	llena_bolsa();
	tamano(LZ.an, LZ.al);
	serie("bloques: ventana %d x %d, casillas de %d; récords en %s\n", LZ.an, LZ.al, cel, rec_ruta());
	return jg_correr(&j);
}
