/* vista.c - el Reproductor: la ventana. Arriba, la «pantalla» (la carátula,
 * el título, el artista, el formato y la visualización: el espectro por
 * barras o la onda); la barra de la posición (se arrastra); los botones
 * grandes, con su símbolo y su texto (Anterior, Reproducir o Pausa, Parar,
 * Siguiente), el volumen (el de este programa en el mezclador de SONIDO.BIN),
 * Aleatorio, Repetir, la velocidad y el modo pequeño; y cuatro pestañas: la
 * Lista, el Ecualizador (10 bandas y el preamplificador, con sus
 * preajustes y los propios), Añadir (las unidades y las carpetas, para
 * añadir archivos, carpetas enteras, CD y listas, y guardar la lista) y
 * Opciones (sin huecos, el fundido, ReplayGain, la visualización). Abajo, la
 * barra de estado. Nada depende solo del color: lo que suena lleva su
 * flecha, lo elegido su franja, lo activado su marca y su palabra.
 *
 * El modo pequeño: una tira con el título que se desplaza, el tiempo y los
 * botones. Minimizada sigue sonando, con el mando en la bandeja. Todo se
 * pinta en un lienzo propio (dibujo.c) y se manda a la ventana a trozos (lo
 * que ha cambiado). Portable: lo del sistema, en so.h.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include <math.h>
#include "vista.h"
#include "motor.h"
#include "dibujo.h"
#include "imagen.h"
#include "util.h"

#define VERSION_REPRO "1.0"
#include <stdlib.h>
static int getenv_depura(void) { return getenv("REPRO_DEPURA") != NULL; }

/* ---- los colores (los de la casa: azules y verdes) ---- */
#define C_FONDO 0xF1F5FB
#define C_TINTA 0x14294A
#define C_SUAVE 0x5A7396
#define C_BLANCO 0xFFFFFF
#define C_PANTALLA_A 0x15304F
#define C_PANTALLA_B 0x22507E
#define C_LUZ 0xDDEBFF
#define C_LUZ2 0x9FC2EC
#define C_AZUL1 0x5AA6F4
#define C_AZUL2 0x2468C8
#define C_VERDE1 0x5FCB6B
#define C_VERDE2 0x2E9E40
#define C_BORDE 0x7E97BA
#define C_ELEGIDO 0xD5E6FB
#define C_FRANJA 0x2468C8
#define C_AVISO 0xFFF1D6

/* ---- lo que hay ---- */
static motor M;
static lista L;
static u32 V;
static lienzo LZ;
static int AN, AL;
static int mini;
enum { P_LISTA, P_EQ, P_ANADIR, P_OPCIONES };
static int pestana;
static int vis_modo = 1;		/* 0 apagada, 1 espectro, 2 onda */
static int ver_restante;
static int salir;
static char carpeta_propia[MAX_RUTA];
static int cambios_ajustes;
static u32 guardar_en;

/* la carátula de lo que suena */
#define LADO_CAR 128
static u32 *car;
static int car_de = -2;
static etiquetas et_oida;
static int et_de = -2;

/* ---- los sitios (se calculan al pintar todo) ---- */
static rect r_pantalla, r_car, r_vis, r_pos, r_botones, r_fila2, r_pestanas, r_cont, r_estado;

/* ---- las zonas: qué hay en cada sitio ---- */
enum {
	Z_NADA, Z_ANTERIOR, Z_TOCAR, Z_PARAR, Z_SIGUIENTE, Z_POS, Z_VOLUMEN, Z_ALEATORIO, Z_REPETIR, Z_VELOCIDAD, Z_MINI,
	Z_PESTANA, Z_FILA, Z_BARRA_LISTA, Z_VIS, Z_TIEMPO, Z_L_ANADIR, Z_L_CARPETA, Z_L_CD, Z_L_ABRIR, Z_L_GUARDAR,
	Z_L_QUITAR, Z_L_VACIAR, Z_L_ORDENAR, Z_EQ_ACTIVO, Z_EQ_AUTO, Z_EQ_BANDA, Z_EQ_PREAJUSTE, Z_EQ_GUARDAR, Z_EQ_BORRAR,
	Z_EQ_PLANO, Z_X_UNIDAD, Z_X_SUBIR, Z_X_FILA, Z_X_ANADIR, Z_X_CARPETA, Z_X_TOCAR, Z_X_GUARDAR, Z_X_BARRA,
	Z_O_HUECOS, Z_O_FUNDIDO, Z_O_RG, Z_O_VIS, Z_CAMPO, Z_COMPLETO, Z_X_CAMPO, Z_O_VELOCIDAD
};
#define MAX_ZONAS 160
static struct { rect r; int id, dato; } zonas[MAX_ZONAS];
static int n_zonas;

static void zona(int x, int y, int an, int al, int id, int dato)
{
	if (n_zonas >= MAX_ZONAS)
		return;
	zonas[n_zonas].r.x = x;
	zonas[n_zonas].r.y = y;
	zonas[n_zonas].r.an = an;
	zonas[n_zonas].r.al = al;
	zonas[n_zonas].id = id;
	zonas[n_zonas].dato = dato;
	n_zonas++;
}

static int zona_en(int x, int y, int *dato)
{
	int i;

	for (i = n_zonas - 1; i >= 0; i--)
		if (x >= zonas[i].r.x && x < zonas[i].r.x + zonas[i].r.an && y >= zonas[i].r.y && y < zonas[i].r.y + zonas[i].r.al) {
			if (dato)
				*dato = zonas[i].dato;
			return zonas[i].id;
		}
	return Z_NADA;
}

static rect *zona_de(int id, int dato)
{
	int i;

	for (i = 0; i < n_zonas; i++)
		if (zonas[i].id == id && zonas[i].dato == dato)
			return &zonas[i].r;
	return NULL;
}

/* ---- lo que hay que volver a mandar a la ventana ---- */
#define MAX_SUCIOS 16
static rect sucios[MAX_SUCIOS];
static int n_sucios, todo_sucio;

static void ensucia(int x, int y, int an, int al)
{
	int i;

	if (todo_sucio || an <= 0 || al <= 0)
		return;
	for (i = 0; i < n_sucios; i++) {
		rect *r = &sucios[i];

		if (x >= r->x && y >= r->y && x + an <= r->x + r->an && y + al <= r->y + r->al)
			return;
	}
	if (n_sucios >= MAX_SUCIOS) {
		todo_sucio = 1;
		return;
	}
	sucios[n_sucios].x = x;
	sucios[n_sucios].y = y;
	sucios[n_sucios].an = an;
	sucios[n_sucios].al = al;
	n_sucios++;
}

static void vuelca(void)
{
	int i;

	if (todo_sucio) {
		so_vuelca(V, &LZ, 0, 0, AN, AL);
	} else
		for (i = 0; i < n_sucios; i++) {
			rect r = sucios[i];

			if (r.x < 0) r.an += r.x, r.x = 0;
			if (r.y < 0) r.al += r.y, r.y = 0;
			if (r.x + r.an > AN) r.an = AN - r.x;
			if (r.y + r.al > AL) r.al = AL - r.y;
			if (r.an > 0 && r.al > 0)
				so_vuelca(V, &LZ, r.x, r.y, r.an, r.al);
		}
	n_sucios = 0;
	todo_sucio = 0;
}

/* ---- la barra de estado: un aviso pasajero ---- */
static char aviso[200];
static u32 aviso_hasta;

static void avisa(const char *t)
{
	copia(aviso, sizeof aviso, t);
	aviso_hasta = so_ms() + 4000;
	reg("%s", t);
}

/* ===================================================================== los símbolos */
enum { S_NADA, S_ANTERIOR, S_TOCAR, S_PAUSA, S_PARAR, S_SIGUIENTE, S_ALEATORIO, S_REPETIR, S_REPETIR1, S_MAS, S_CARPETA,
	S_DISCO, S_ABRIR, S_GUARDAR, S_QUITAR, S_VACIAR, S_SUBIR, S_MINI, S_COMPLETO, S_NOTA, S_VELOCIDAD, S_ORDENAR, S_ALTAVOZ };

static void simbolo(int s, int cx, int cy, int t, u32 c)
{
	/* t: el tamaño (la mitad del lado) */
	switch (s) {
	case S_TOCAR:
		dib_triangulo(cx - t * 2 / 3, cy - t, cx - t * 2 / 3, cy + t, cx + t, cy, c);
		break;
	case S_PAUSA:
		dib_rect(cx - t * 3 / 4, cy - t, t / 2 + 1, 2 * t, c);
		dib_rect(cx + t / 4, cy - t, t / 2 + 1, 2 * t, c);
		break;
	case S_PARAR:
		dib_rect(cx - t * 3 / 4, cy - t * 3 / 4, t * 3 / 2, t * 3 / 2, c);
		break;
	case S_ANTERIOR:
		dib_rect(cx - t, cy - t, t / 3 + 1, 2 * t, c);
		dib_triangulo(cx + t, cy - t, cx + t, cy + t, cx - t / 2, cy, c);
		break;
	case S_SIGUIENTE:
		dib_rect(cx + t - t / 3, cy - t, t / 3 + 1, 2 * t, c);
		dib_triangulo(cx - t, cy - t, cx - t, cy + t, cx + t / 2, cy, c);
		break;
	case S_ALEATORIO:
		dib_linea(cx - t, cy - t / 2, cx + t / 2, cy + t / 2, c);
		dib_linea(cx - t, cy - t / 2 + 1, cx + t / 2, cy + t / 2 + 1, c);
		dib_linea(cx - t, cy + t / 2, cx + t / 2, cy - t / 2, c);
		dib_linea(cx - t, cy + t / 2 + 1, cx + t / 2, cy - t / 2 + 1, c);
		dib_triangulo(cx + t / 2, cy - t / 2 - 3, cx + t / 2, cy - t / 2 + 4, cx + t, cy - t / 2, c);
		dib_triangulo(cx + t / 2, cy + t / 2 - 3, cx + t / 2, cy + t / 2 + 4, cx + t, cy + t / 2, c);
		break;
	case S_REPETIR:
	case S_REPETIR1:
		dib_rect(cx - t, cy - t / 2, 2 * t - 3, 2, c);
		dib_rect(cx - t, cy - t / 2, 2, t, c);
		dib_rect(cx - t + 3, cy + t / 2, 2 * t - 3, 2, c);
		dib_rect(cx + t - 2, cy - t / 2 + 2, 2, t, c);
		dib_triangulo(cx + t - 4, cy - t / 2 - 4, cx + t - 4, cy - t / 2 + 5, cx + t + 1, cy - t / 2 + 1, c);
		if (s == S_REPETIR1)
			dib_texto(&f_negrita12, cx - 3, cy - 8, "1", c);
		break;
	case S_MAS:
		dib_rect(cx - t, cy - 1, 2 * t, 3, c);
		dib_rect(cx - 1, cy - t, 3, 2 * t, c);
		break;
	case S_QUITAR:
		dib_rect(cx - t, cy - 1, 2 * t, 3, c);
		break;
	case S_VACIAR:
		dib_linea(cx - t, cy - t, cx + t, cy + t, c);
		dib_linea(cx - t + 1, cy - t, cx + t + 1, cy + t, c);
		dib_linea(cx + t, cy - t, cx - t, cy + t, c);
		dib_linea(cx + t + 1, cy - t, cx - t + 1, cy + t, c);
		break;
	case S_CARPETA:
		dib_rect(cx - t, cy - t / 2, t, 3, c);
		dib_rect(cx - t, cy - t / 2 + 2, 2 * t, t + t / 2, c);
		break;
	case S_DISCO:
		dib_circulo(cx, cy, t, c);
		dib_circulo(cx, cy, t / 4 + 1, 0x808080);
		break;
	case S_ABRIR:
		dib_rect(cx - t, cy - t / 2, 2 * t, t + t / 2, c);
		dib_triangulo(cx - t / 2, cy - t / 2, cx + t / 2, cy - t / 2, cx, cy - t - 2, c);
		break;
	case S_GUARDAR:
		dib_rect(cx - t, cy - t, 2 * t, 2 * t, c);
		dib_rect(cx - t / 2, cy - t + 2, t, t / 2 + 1, 0x808080);
		break;
	case S_SUBIR:
		dib_triangulo(cx - t, cy, cx + t, cy, cx, cy - t, c);
		dib_rect(cx - t / 3, cy, t * 2 / 3 + 1, t, c);
		break;
	case S_MINI:
		dib_borde(cx - t, cy - t, 2 * t, 2 * t, c);
		dib_rect(cx - t, cy + t / 3, 2 * t, t * 2 / 3, c);
		break;
	case S_COMPLETO:
		dib_borde(cx - t, cy - t, 2 * t, 2 * t, c);
		dib_rect(cx - t, cy - t, 2 * t, t / 2, c);
		break;
	case S_NOTA:
		dib_circulo(cx - t / 3, cy + t / 2, t / 2, c);
		dib_rect(cx + t / 6 - 1, cy - t, 2, t + t / 2, c);
		dib_triangulo(cx + t / 6, cy - t, cx + t * 5 / 6, cy - t / 2, cx + t / 6, cy - t / 4, c);
		break;
	case S_VELOCIDAD:
		dib_triangulo(cx - t, cy - t, cx - t, cy + t, cx, cy, c);
		dib_triangulo(cx, cy - t, cx, cy + t, cx + t, cy, c);
		break;
	case S_ORDENAR:
		dib_rect(cx - t, cy - t, 2 * t, 2, c);
		dib_rect(cx - t, cy - 1, t * 3 / 2, 2, c);
		dib_rect(cx - t, cy + t - 2, t, 2, c);
		break;
	case S_ALTAVOZ:
		dib_rect(cx - t, cy - t / 3, t / 2, t * 2 / 3 + 1, c);
		dib_triangulo(cx - t / 2, cy - t / 3, cx - t / 2, cy + t / 3, cx + t / 3, cy - t, c);
		dib_triangulo(cx - t / 2, cy + t / 3, cx + t / 3, cy + t, cx + t / 3, cy - t, c);
		break;
	}
}

/* un botón: redondeado, con su símbolo y su texto. «modo»: 0 normal (azul),
 * 1 principal (verde), 2 apagado (gris), 3 encendido (azul oscuro con marca) */
static void boton(int x, int y, int an, int al, int s, const char *t, int id, int dato, int modo)
{
	u32 a = modo == 1 ? C_VERDE1 : modo == 2 ? 0xC8D0DC : modo == 3 ? 0x3D7FD8 : C_AZUL1;
	u32 b = modo == 1 ? C_VERDE2 : modo == 2 ? 0x98A4B6 : modo == 3 ? 0x16428A : C_AZUL2;
	u32 borde = modo == 1 ? 0x1D6E2A : modo == 3 ? 0x0B2556 : 0x184A90;
	int ts = al >= 36 ? 9 : 6, w = t ? dib_ancho(al >= 36 ? &f_negrita14 : &f_negrita12, t) : 0, sx, tx;
	const fuente *f = al >= 36 ? &f_negrita14 : &f_negrita12;

	dib_redondo(x, y, an, al, 7, borde);
	dib_redondo_degradado(x + 1, y + 1, an - 2, al - 2, 6, a, b);
	if (modo == 3) {
		/* encendido: un borde claro por dentro (además del color) */
		dib_redondo_borde(x + 3, y + 3, an - 6, al - 6, 4, 0xBFE0FF);
	}
	if (s && t) {
		int total = ts * 2 + 6 + w;

		sx = x + (an - total) / 2 + ts;
		tx = sx + ts + 6;
		if (total > an - 6) {
			sx = x + 6 + ts;
			tx = sx + ts + 5;
		}
	} else {
		sx = x + an / 2;
		tx = x + (an - w) / 2;
	}
	if (s)
		simbolo(s, sx, y + al / 2, ts, C_BLANCO);
	if (t) {
		int ty = y + (al - f->linea) / 2;

		dib_texto_corto(f, tx + 1, ty + 1, x + an - tx - 4, t, 0x10305C);
		dib_texto_corto(f, tx, ty, x + an - tx - 4, t, C_BLANCO);
	}
	zona(x, y, an, al, id, dato);
}

static void casilla(int x, int y, const char *t, int marcada, int id, int dato)
{
	dib_rect(x, y + 1, 16, 16, 0x3A6EA8);
	dib_rect(x + 1, y + 2, 14, 14, C_BLANCO);
	if (marcada) {
		dib_linea(x + 3, y + 9, x + 6, y + 13, 0x1C7A30);
		dib_linea(x + 3, y + 10, x + 6, y + 14, 0x1C7A30);
		dib_linea(x + 6, y + 13, x + 13, y + 4, 0x1C7A30);
		dib_linea(x + 6, y + 14, x + 13, y + 5, 0x1C7A30);
	}
	dib_texto(&f_normal14, x + 24, y, t, C_TINTA);
	zona(x, y, 24 + dib_ancho(&f_normal14, t), 18, id, dato);
}

/* una de varias (las opciones): un círculo, lleno si es la elegida */
static int opcion(int x, int y, const char *t, int elegida, int id, int dato)
{
	int w = dib_ancho(&f_normal14, t);

	dib_circulo(x + 8, y + 9, 8, 0x3A6EA8);
	dib_circulo(x + 8, y + 9, 7, C_BLANCO);
	if (elegida)
		dib_circulo(x + 8, y + 9, 4, C_AZUL2);
	(elegida ? dib_texto(&f_negrita14, x + 22, y, t, C_TINTA) : dib_texto(&f_normal14, x + 22, y, t, C_TINTA));
	zona(x, y, 26 + w, 18, id, dato);
	return x + 34 + w;
}

/* ===================================================================== lo que suena */
static cancion *oida(void)
{
	return M.oida >= 0 && M.oida < L.n ? &L.c[M.oida] : NULL;
}

/* las etiquetas y la carátula de lo que suena (cuando cambia) */
static void mira_oida(void)
{
	cancion *c = oida();

	if (M.oida == et_de)
		return;
	et_de = M.oida;
	etiquetas_suelta(&et_oida);
	memset(&et_oida, 0, sizeof et_oida);
	so_suelta(car);
	car = NULL;
	car_de = M.oida;
	if (!c)
		return;
	if (!c->o.lector && !c->o.largo) {
		etiquetas_lee(c->o.ruta, &et_oida, 1);
		if (et_oida.caratula) {
			car = imagen_a_medida(et_oida.caratula, et_oida.caratula_largo, LADO_CAR);
			etiquetas_suelta(&et_oida);
		}
		/* (sin imagen dentro: una FOLDER.JPG o COVER.JPG en su carpeta) */
		if (!car) {
			static const char *nombres[] = { "FOLDER.JPG", "COVER.JPG", "CARATULA.JPG", "FRONT.JPG", "FOLDER.PNG", "COVER.PNG", 0 };
			char r[MAX_RUTA];
			int i;

			for (i = 0; nombres[i] && !car; i++) {
				u32 n;
				u8 *d;

				carpeta_de(c->o.ruta, r, sizeof r);
				anade(r, sizeof r, nombres[i]);
				d = so_lee_todo(r, &n, MAX_CARATULA);
				if (d) {
					car = imagen_a_medida(d, n, LADO_CAR);
					so_suelta(d);
				}
			}
		}
	}
	{
		char t[200];

		lista_nombre(c, t, sizeof t);
		{
			char tt[240];

			fmt(tt, sizeof tt, "Reproductor - %s", t);
			so_titulo(V, tt);
		}
	}
}

/* el ecualizador automático: por el género de la canción */
static int eq_auto;
static int eq_preajuste = 0;	/* el que está puesto (-1: a mano; 1000 + n: uno propio) */
#define MAX_PROPIOS 16
static struct { char nombre[40]; signed char v[BANDAS + 1]; } propios[MAX_PROPIOS];
static int n_propios;

static void pon_eq(const signed char *v)
{
	int b;

	M.eq.preamp = v[0] * 10;
	for (b = 0; b < BANDAS; b++)
		M.eq.banda[b] = v[b + 1] * 10;
	eq_cambia(&M.eq);
	cambios_ajustes++;
}

static void eq_por_genero(void)
{
	static const struct { const char *g, *p; } tabla[] = { { "rock", "Rock" }, { "metal", "Rock" }, { "punk", "Rock" },
		{ "pop", "Pop" }, { "classical", "Clásica" }, { "clásica", "Clásica" }, { "clasica", "Clásica" },
		{ "jazz", "Jazz" }, { "blues", "Jazz" }, { "dance", "Baile" }, { "house", "Baile" }, { "disco", "Baile" },
		{ "techno", "Techno" }, { "electronic", "Techno" }, { "trance", "Techno" }, { "reggae", "Reggae" }, { "ska", "Ska" },
		{ "speech", "Voz" }, { "podcast", "Voz" }, { "audiobook", "Voz" }, { "spoken", "Voz" }, { "vocal", "Voz" },
		{ "hip-hop", "Graves" }, { "rap", "Graves" }, { "live", "Directo" }, { 0, 0 } };
	voz *v = motor_voz_oida(&M);
	const char *g;
	int i, k;

	if (!eq_auto || !v)
		return;
	g = v->et.genero;
	if (!g[0])
		return;
	for (i = 0; tabla[i].g; i++)
		if (contiene_llano(g, tabla[i].g)) {
			for (k = 0; k < n_preajustes; k++)
				if (!strcmp(preajustes[k].nombre, tabla[i].p) && eq_preajuste != k) {
					char t[100];

					eq_preajuste = k;
					pon_eq(preajustes[k].v);
					fmt(t, sizeof t, "Ecualizador automático: %s (el género es «%s»)", preajustes[k].nombre, g);
					avisa(t);
				}
			return;
		}
}

/* ===================================================================== pintar */
static void disponer(void)
{
	int y;

	r_pantalla.x = 0;
	r_pantalla.y = 0;
	r_pantalla.an = AN;
	r_pantalla.al = 152;
	r_car.x = 12;
	r_car.y = 12;
	r_car.an = r_car.al = LADO_CAR;
	r_vis.an = AN >= 640 ? 220 : 0;
	r_vis.al = 96;
	r_vis.x = AN - r_vis.an - 12;
	r_vis.y = 14;
	if (!r_vis.an) {
		r_vis.x = 152;
		r_vis.y = 108;
		r_vis.an = AN - 164;
		r_vis.al = 32;
	}
	y = r_pantalla.al + 6;
	r_pos.x = 10;
	r_pos.y = y;
	r_pos.an = AN - 20;
	r_pos.al = 26;
	y += r_pos.al + 6;
	r_botones.x = 10;
	r_botones.y = y;
	r_botones.an = AN - 20;
	r_botones.al = 42;
	y += r_botones.al + 6;
	r_fila2.x = 10;
	r_fila2.y = y;
	r_fila2.an = AN - 20;
	r_fila2.al = 32;
	y += r_fila2.al + 8;
	r_pestanas.x = 0;
	r_pestanas.y = y;
	r_pestanas.an = AN;
	r_pestanas.al = 30;
	y += r_pestanas.al;
	r_estado.x = 0;
	r_estado.al = 24;
	r_estado.y = AL - r_estado.al;
	r_estado.an = AN;
	r_cont.x = 0;
	r_cont.y = y;
	r_cont.an = AN;
	r_cont.al = r_estado.y - y;
}

/* ---- la visualización ---- */
#define BARRAS 24
static float barra[BARRAS], pico[BARRAS];


static void pinta_vis(void)
{
	rect r = r_vis;
	static float m[FFT_N], mag[FFT_N / 2];
	int i;

	dib_recorte(r.x, r.y, r.an, r.al);
	dib_rect(r.x, r.y, r.an, r.al, 0x0E2238);
	dib_borde(r.x, r.y, r.an, r.al, 0x3B6EA5);
	if (vis_modo == 0) {
		dib_texto_centrado(&f_normal12, r.x, r.y + r.al / 2 - 8, r.an, "Visualización apagada (clic: encender)", C_LUZ2);
	} else {
		motor_lo_oido(&M, m, FFT_N);
		if (vis_modo == 1) {
			int ancho = (r.an - 8) / BARRAS, alto = r.al - 22;

			espectro(m, mag);
			for (i = 0; i < BARRAS; i++) {
				double f0 = 50.0 * pow(16000.0 / 50.0, (double)i / BARRAS), f1 = 50.0 * pow(16000.0 / 50.0, (double)(i + 1) / BARRAS);
				int k0 = (int)(f0 * FFT_N / M.hz), k1 = (int)(f1 * FFT_N / M.hz), k, y, h, seg;
				float v = 0, db;

				if (k1 <= k0)
					k1 = k0 + 1;
				for (k = k0; k < k1 && k < FFT_N / 2; k++)
					if (mag[k] > v)
						v = mag[k];
				db = v > 1e-6f ? (float)(20.0 * log10(v)) : -120.0f;
				v = (db + 60.0f) / 60.0f;
				if (v < 0)
					v = 0;
				if (v > 1)
					v = 1;
				/* que caigan despacio (y el pico, más despacio) */
				barra[i] = v > barra[i] ? v : barra[i] - 0.06f;
				if (barra[i] < 0)
					barra[i] = 0;
				pico[i] = barra[i] > pico[i] ? barra[i] : pico[i] - 0.015f;
				h = (int)(barra[i] * alto);
				/* a trozos de 3 puntos (la forma dice la altura, no solo el color) */
				for (seg = 0; seg * 4 < h; seg++) {
					float t = (float)(seg * 4) / alto;
					u32 c = t < 0.6f ? 0x58D36A : t < 0.85f ? 0xE8D44A : 0xF08A3C;

					y = r.y + 4 + alto - seg * 4 - 3;
					dib_rect(r.x + 4 + i * ancho + 1, y, ancho - 2, 3, c);
				}
				if (pico[i] > 0.02f)
					dib_rect(r.x + 4 + i * ancho + 1, r.y + 4 + alto - (int)(pico[i] * alto) - 1, ancho - 2, 2, C_BLANCO);
			}
			dib_texto(&f_normal12, r.x + 6, r.y + r.al - 17, "Espectro", C_LUZ2);
		} else {
			int cy = r.y + (r.al - 16) / 2, x0 = r.x + 2, ant = cy;

			dib_rect(r.x + 2, cy, r.an - 4, 1, 0x2C4E74);
			for (i = 0; i < r.an - 4; i++) {
				float s = m[i * FFT_N / (r.an - 4)];
				int y = cy - (int)(s * (r.al - 20) * 0.9f);

				if (i)
					dib_linea(x0 + i - 1, ant, x0 + i, y, 0x8FF0A0);
				ant = y;
			}
			dib_texto(&f_normal12, r.x + 6, r.y + r.al - 17, "Onda", C_LUZ2);
		}
		dib_texto(&f_normal12, r.x + r.an - dib_ancho(&f_normal12, "clic: cambiar") - 6, r.y + r.al - 17, "clic: cambiar", 0x6F92BC);
	}
	dib_recorte(0, 0, 0, 0);
	ensucia(r.x, r.y, r.an, r.al);
}

static void formato_de(char *t, int cabe)
{
	voz *v = motor_voz_oida(&M);
	char hz[24];

	t[0] = 0;
	if (!v)
		return;
	if (v->d.hz % 1000)
		fmt(hz, sizeof hz, "%u,%u kHz", v->d.hz / 1000, v->d.hz % 1000 / 100);
	else
		fmt(hz, sizeof hz, "%u kHz", v->d.hz / 1000);
	fmt(t, cabe, "%s · %s · %s", v->d.nombre_formato, hz, v->d.canales == 1 ? "mono" : v->d.canales == 2 ? "estéreo" : "varios canales (a estéreo)");
	if (v->d.kbps) {
		char k[24];

		fmt(k, sizeof k, " · %u kbit/s", v->d.kbps);
		anade(t, cabe, k);
	}
	if (v->ganancia != 1.0f && M.replaygain) {
		char k[48];
		int db = (int)(200.0 * log10(v->ganancia) + (v->ganancia < 1.0f ? -0.5 : 0.5));

		fmt(k, sizeof k, " · ReplayGain %c%d,%d dB", db < 0 ? '-' : '+', (db < 0 ? -db : db) / 10, (db < 0 ? -db : db) % 10);
		anade(t, cabe, k);
	}
}

static void pinta_pantalla(void)
{
	rect r = r_pantalla;
	cancion *c = oida();
	int x = r_car.x + r_car.an + 16, an = (r_vis.y < 100 ? r_vis.x - 16 : AN - 12) - x;
	char t[300];

	dib_degradado(r.x, r.y, r.an, r.al, C_PANTALLA_A, C_PANTALLA_B);
	/* la carátula (o un disco con su nota) */
	if (car)
		dib_imagen(r_car.x, r_car.y, r_car.an, r_car.al, car, LADO_CAR, LADO_CAR);
	else {
		dib_redondo(r_car.x, r_car.y, r_car.an, r_car.al, 10, 0x2B5C92);
		dib_circulo(r_car.x + 64, r_car.y + 64, 50, 0x16385E);
		dib_circulo(r_car.x + 64, r_car.y + 64, 16, 0x2B5C92);
		simbolo(S_NOTA, r_car.x + 64, r_car.y + 64, 22, C_LUZ);
	}
	dib_borde(r_car.x - 1, r_car.y - 1, r_car.an + 2, r_car.al + 2, 0x0B1E33);
	if (c) {
		dib_texto_corto(&f_negrita20, x, 16, an, c->titulo, C_BLANCO);
		if (c->artista[0] || c->album[0]) {
			if (c->artista[0] && c->album[0])
				fmt(t, sizeof t, "%s — %s", c->artista, c->album);
			else
				copia(t, sizeof t, c->artista[0] ? c->artista : c->album);
			if (et_oida.anyo[0]) {
				anade(t, sizeof t, " (");
				anade(t, sizeof t, et_oida.anyo);
				anade(t, sizeof t, ")");
			}
			dib_texto_corto(&f_normal14, x, 46, an, t, C_LUZ);
		}
		formato_de(t, sizeof t);
		dib_texto_corto(&f_normal12, x, 70, an, t, C_LUZ2);
		{
			const char *est = M.estado == M_SONANDO ? "Sonando" : M.estado == M_PAUSA ? "En pausa" : "Parado";
			int sx = x;

			simbolo(M.estado == M_SONANDO ? S_TOCAR : M.estado == M_PAUSA ? S_PAUSA : S_PARAR, sx + 7, 100, 6, C_LUZ);
			fmt(t, sizeof t, "%s · canción %d de %d", est, M.oida + 1, L.n);
			dib_texto_corto(&f_negrita12, sx + 20, 93, an - 20, t, C_LUZ);
		}
	} else {
		dib_texto(&f_negrita20, x, 16, "Reproductor", C_BLANCO);
		dib_texto_corto(&f_normal14, x, 46, an, L.n ? "Elija una canción de la lista (dos clics) o pulse Reproducir." :
			"Añada canciones: pestaña «Añadir», o suéltelas desde Mi equipo.", C_LUZ);
		dib_texto_corto(&f_normal12, x, 70, an, "MP3 · OGG · FLAC · WAV · MOD · XM · S3M · IT · CD de música", C_LUZ2);
	}
	zona(r_vis.x, r_vis.y, r_vis.an, r_vis.al, Z_VIS, 0);
	pinta_vis();
	ensucia(r.x, r.y, r.an, r.al);
}

/* ---- la posición ---- */
static int arrastre;		/* qué se arrastra (Z_POS, Z_VOLUMEN, Z_EQ_BANDA...) */
static int arrastre_dato;
static u32 pos_arrastre;	/* (la posición que se enseña mientras se arrastra) */

static void pinta_posicion(void)
{
	rect r = r_pos;
	u32 p = arrastre == Z_POS ? pos_arrastre : motor_pos_ms(&M), tot = motor_total_ms(&M);
	char a[24], b[24];
	int bx, bw, k;

	dib_rect(r.x - 4, r.y - 2, r.an + 8, r.al + 4, C_FONDO);
	a_tiempo(p / 1000, a, sizeof a);
	if (ver_restante && tot) {
		b[0] = '-';
		a_tiempo(tot > p ? (tot - p) / 1000 : 0, b + 1, sizeof b - 1);
	} else if (tot)
		a_tiempo(tot / 1000, b, sizeof b);
	else
		copia(b, sizeof b, "--:--");
	dib_texto(&f_negrita14, r.x, r.y + 4, a, C_TINTA);
	zona(r.x, r.y, 60, r.al, Z_TIEMPO, 0);
	bx = r.x + 62;
	bw = r.an - 124;
	dib_texto(&f_negrita14, r.x + r.an - dib_ancho(&f_negrita14, b), r.y + 4, b, C_TINTA);
	zona(r.x + r.an - 60, r.y, 60, r.al, Z_TIEMPO, 0);
	/* la barra: el carril, lo tocado (relleno y con rayas) y el tirador */
	dib_redondo(bx, r.y + 9, bw, 9, 4, C_BORDE);
	dib_redondo(bx + 1, r.y + 10, bw - 2, 7, 3, 0xE8EEF6);
	k = tot ? (int)((u64)(p > tot ? tot : p) * (u64)(bw - 2) / tot) : 0;
	if (k > 2) {
		int i;

		dib_redondo(bx + 1, r.y + 10, k, 7, 3, 0x3D8FE8);
		for (i = 6; i < k - 2; i += 6)
			dib_rect(bx + 1 + i, r.y + 11, 1, 5, 0x2A6FC4);
	}
	if (tot) {
		dib_redondo(bx + k - 6, r.y + 3, 13, 20, 5, 0x184A90);
		dib_redondo_degradado(bx + k - 5, r.y + 4, 11, 18, 4, 0x6DB4F8, 0x2468C8);
		dib_rect(bx + k - 2, r.y + 9, 1, 8, C_BLANCO);
		dib_rect(bx + k + 1, r.y + 9, 1, 8, C_BLANCO);
	}
	zona(bx - 6, r.y, bw + 12, r.al, Z_POS, bx << 16 | bw);
	ensucia(r.x - 4, r.y - 2, r.an + 8, r.al + 4);
}

/* ---- el volumen (un deslizador) ---- */
static rect r_vol;

static void pinta_volumen(void)
{
	rect r = r_vol;
	int k = M.volumen * (r.an - 1) / 100;
	char t[16];

	dib_rect(r.x - 30, r.y - 4, r.an + 30 + 52, r.al + 8, C_FONDO);
	simbolo(S_ALTAVOZ, r.x - 16, r.y + r.al / 2, 9, C_TINTA);
	/* (las ondas: tantas como volumen) */
	if (M.volumen > 33)
		dib_rect(r.x - 6, r.y + r.al / 2 - 3, 2, 7, C_TINTA);
	if (M.volumen > 66)
		dib_rect(r.x - 3, r.y + r.al / 2 - 6, 2, 13, C_TINTA);
	dib_triangulo(r.x, r.y + r.al - 6, r.x + r.an, r.y + r.al - 6, r.x + r.an, r.y + 4, 0xC9D6E8);
	dib_triangulo(r.x, r.y + r.al - 6, r.x + k, r.y + r.al - 6, r.x + k, r.y + r.al - 6 - (r.al - 10) * k / (r.an ? r.an : 1), 0x3D8FE8);
	dib_redondo(r.x + k - 5, r.y + 2, 11, r.al - 4, 4, 0x184A90);
	dib_redondo_degradado(r.x + k - 4, r.y + 3, 9, r.al - 6, 3, 0x6DB4F8, 0x2468C8);
	fmt(t, sizeof t, "%d %%", M.volumen);
	dib_texto(&f_negrita14, r.x + r.an + 10, r.y + (r.al - 16) / 2, t, C_TINTA);
	zona(r.x - 30, r.y - 4, r.an + 30 + 52, r.al + 8, Z_VOLUMEN, r.x << 16 | r.an);
	ensucia(r.x - 30, r.y - 4, r.an + 30 + 52, r.al + 8);
}

static const char *nombre_velocidad(char *t, int cabe)
{
	int c = (int)(M.velocidad * 100 + 0.5f);

	if (c % 100 == 0)
		fmt(t, cabe, "Velocidad %d×", c / 100);
	else if (c % 10 == 0)
		fmt(t, cabe, "Velocidad %d,%d×", c / 100, c / 10 % 10);
	else
		fmt(t, cabe, "Velocidad %d,%02d×", c / 100, c % 100);
	return t;
}

static void pinta_botones(void)
{
	rect r = r_botones;
	int w = (r.an - 280) / 4, x = r.x;
	char t[40];

	if (w < 70)
		w = 70;
	if (w > 140)
		w = 140;
	dib_rect(r.x, r.y, r.an, r.al, C_FONDO);
	boton(x, r.y, w, r.al, S_ANTERIOR, "Anterior", Z_ANTERIOR, 0, 0);
	x += w + 6;
	boton(x, r.y, w + 20, r.al, M.estado == M_SONANDO ? S_PAUSA : S_TOCAR, M.estado == M_SONANDO ? "Pausa" : "Reproducir", Z_TOCAR, 0, 1);
	x += w + 26;
	boton(x, r.y, w, r.al, S_PARAR, "Parar", Z_PARAR, 0, M.estado == M_PARADO ? 2 : 0);
	x += w + 6;
	boton(x, r.y, w, r.al, S_SIGUIENTE, "Siguiente", Z_SIGUIENTE, 0, 0);
	x += w + 30;
	r_vol.x = x + 28;
	r_vol.y = r.y + 6;
	r_vol.an = r.x + r.an - r_vol.x - 58;
	if (r_vol.an > 160)
		r_vol.an = 160;
	r_vol.al = r.al - 12;
	pinta_volumen();
	/* la segunda fila */
	r = r_fila2;
	dib_rect(r.x, r.y, r.an, r.al, C_FONDO);
	x = r.x;
	boton(x, r.y, 136, r.al, S_ALEATORIO, L.aleatorio ? "Aleatorio: sí" : "Aleatorio: no", Z_ALEATORIO, 0, L.aleatorio ? 3 : 0);
	x += 142;
	boton(x, r.y, 156, r.al, L.repetir == REPITE_UNA ? S_REPETIR1 : S_REPETIR,
	      L.repetir == REPITE_NO ? "Repetir: no" : L.repetir == REPITE_TODO ? "Repetir: todo" : "Repetir: una", Z_REPETIR, 0, L.repetir ? 3 : 0);
	x += 162;
	boton(x, r.y, 150, r.al, S_VELOCIDAD, nombre_velocidad(t, sizeof t), Z_VELOCIDAD, 0, M.velocidad != 1.0f ? 3 : 0);
	x += 156;
	if (x + 110 <= r.x + r.an)
		boton(r.x + r.an - 110, r.y, 110, r.al, S_MINI, "Pequeño", Z_MINI, 0, 0);
	ensucia(r_botones.x, r_botones.y, r_botones.an, r_fila2.y + r_fila2.al - r_botones.y);
}

static void pinta_pestanas(void)
{
	static const char *nom[] = { "Lista", "Ecualizador", "Añadir", "Opciones" };
	rect r = r_pestanas;
	int i, x = 8;
	char t[40];

	dib_rect(r.x, r.y, r.an, r.al, C_FONDO);
	dib_rect(r.x, r.y + r.al - 1, r.an, 1, C_BORDE);
	for (i = 0; i < 4; i++) {
		int w;

		if (i == 0)
			fmt(t, sizeof t, "Lista (%d)", L.n);
		else if (i == 1)
			fmt(t, sizeof t, "Ecualizador: %s", M.eq.activo ? "sí" : "no");
		else
			copia(t, sizeof t, nom[i]);
		w = dib_ancho(&f_negrita14, t) + 28;
		if (i == pestana) {
			dib_redondo(x, r.y + 2, w, r.al + 6, 6, C_BORDE);
			dib_redondo(x + 1, r.y + 3, w - 2, r.al + 6, 5, C_BLANCO);
			dib_rect(x + 6, r.y + 4, w - 12, 3, C_AZUL2);	/* (la elegida: una raya arriba, y en negrita) */
			dib_texto(&f_negrita14, x + 14, r.y + 8, t, C_TINTA);
		} else {
			dib_redondo(x, r.y + 5, w, r.al + 4, 6, C_BORDE);
			dib_redondo(x + 1, r.y + 6, w - 2, r.al + 4, 5, 0xDCE6F3);
			dib_texto(&f_normal14, x + 14, r.y + 10, t, C_SUAVE);
		}
		zona(x, r.y, w, r.al, Z_PESTANA, i);
		x += w + 4;
	}
	ensucia(r.x, r.y, r.an, r.al);
}

/* ---- la lista ---- */
#define ALTO_FILA 22
static int lista_arriba, lista_cursor, lista_ancla = -1;
static int fila_arrastrada = -1, fila_destino = -1;
static rect r_filas;

static int filas_visibles(void)
{
	return r_filas.al / ALTO_FILA;
}

static void asegura_visible(int i)
{
	int n = filas_visibles();

	if (i < lista_arriba)
		lista_arriba = i;
	if (n > 0 && i >= lista_arriba + n)
		lista_arriba = i - n + 1;
	if (lista_arriba > L.n - n)
		lista_arriba = L.n - n;
	if (lista_arriba < 0)
		lista_arriba = 0;
}

/* una barra de desplazamiento: el carril y el tirador (con sus rayas) */
static void barra_vertical(int x, int y, int al, int total, int visibles, int arriba, int id)
{
	int th, ty;

	dib_rect(x, y, 14, al, 0xE2E9F3);
	dib_rect(x, y, 1, al, C_BORDE);
	if (total <= visibles || total <= 0)
		return;
	th = al * visibles / total;
	if (th < 24)
		th = 24;
	ty = y + (al - th) * arriba / (total - visibles);
	dib_redondo(x + 2, ty, 11, th, 4, 0x8EA8CB);
	dib_rect(x + 5, ty + th / 2 - 4, 5, 1, C_BLANCO);
	dib_rect(x + 5, ty + th / 2, 5, 1, C_BLANCO);
	dib_rect(x + 5, ty + th / 2 + 4, 5, 1, C_BLANCO);
	zona(x, y, 14, al, id, y << 16 | al);
}

static void pinta_lista(void)
{
	rect r = r_cont;
	int y = r.y + 6, x = 8, i, n, col_dur, col_art, ancho;
	char t[300];

	/* la barra de herramientas */
	boton(x, y, 104, 28, S_MAS, "Añadir...", Z_L_ANADIR, 0, 0);
	x += 108;
	boton(x, y, 98, 28, S_DISCO, "CD", Z_L_CD, 0, 0);
	x += 102;
	boton(x, y, 116, 28, S_ABRIR, "Abrir lista", Z_L_ABRIR, 0, 0);
	x += 120;
	boton(x, y, 128, 28, S_GUARDAR, "Guardar lista", Z_L_GUARDAR, 0, L.n ? 0 : 2);
	x += 132;
	if (x + 100 < AN) {
		boton(x, y, 96, 28, S_QUITAR, "Quitar", Z_L_QUITAR, 0, L.n ? 0 : 2);
		x += 100;
	}
	if (x + 100 < AN) {
		boton(x, y, 96, 28, S_VACIAR, "Vaciar", Z_L_VACIAR, 0, L.n ? 0 : 2);
		x += 100;
	}
	if (x + 104 < AN)
		boton(x, y, 100, 28, S_ORDENAR, "Ordenar", Z_L_ORDENAR, 0, L.n ? 0 : 2);
	y += 36;
	/* los rótulos */
	ancho = AN - 16 - 16;
	col_dur = 8 + ancho - 64;
	col_art = 8 + ancho * 62 / 100;
	dib_rect(8, y, ancho, 22, 0xDCE6F3);
	dib_texto(&f_negrita12, 14, y + 4, "#", C_TINTA);
	dib_texto(&f_negrita12, 50, y + 4, "Título", C_TINTA);
	dib_texto(&f_negrita12, col_art, y + 4, "Artista", C_TINTA);
	dib_texto(&f_negrita12, col_dur, y + 4, "Duración", C_TINTA);
	y += 22;
	r_filas.x = 8;
	r_filas.y = y;
	r_filas.an = ancho;
	r_filas.al = r.y + r.al - y - 4;
	n = filas_visibles();
	if (lista_arriba > L.n - n)
		lista_arriba = L.n - n;
	if (lista_arriba < 0)
		lista_arriba = 0;
	dib_rect(r_filas.x, r_filas.y, r_filas.an, r_filas.al, C_BLANCO);
	if (!L.n) {
		dib_texto_centrado(&f_normal14, r_filas.x, r_filas.y + 20, r_filas.an, "La lista está vacía.", C_SUAVE);
		dib_texto_centrado(&f_normal12, r_filas.x, r_filas.y + 44, r_filas.an,
				   "«Añadir...» (Ctrl+O), «CD», «Abrir lista», o arrastre canciones y carpetas desde Mi equipo.", C_SUAVE);
	}
	for (i = 0; i < n && lista_arriba + i < L.n; i++) {
		int k = lista_arriba + i, fy = r_filas.y + i * ALTO_FILA;
		cancion *c = &L.c[k];
		int suena = k == M.oida && M.estado != M_PARADO;
		const fuente *f = suena ? &f_negrita12 : &f_normal12;
		u32 tinta = c->fallo ? 0x9A3A2A : C_TINTA;

		if (c->marcada) {
			dib_rect(r_filas.x, fy, r_filas.an, ALTO_FILA, C_ELEGIDO);
			dib_rect(r_filas.x, fy, 4, ALTO_FILA, C_FRANJA);	/* (la franja: no solo el color) */
		} else if (k & 1)
			dib_rect(r_filas.x, fy, r_filas.an, ALTO_FILA, 0xF6F8FC);
		if (k == lista_cursor && pestana == P_LISTA)
			dib_borde(r_filas.x + 1, fy, r_filas.an - 2, ALTO_FILA, 0x5A86C8);
		if (suena)
			simbolo(M.estado == M_PAUSA ? S_PAUSA : S_TOCAR, r_filas.x + 12, fy + ALTO_FILA / 2, 5, C_VERDE2);
		fmt(t, sizeof t, "%d", k + 1);
		dib_texto(f, r_filas.x + 22, fy + 3, t, tinta);
		if (c->fallo) {
			char tt[200];

			fmt(tt, sizeof tt, "%s (no se puede tocar)", c->titulo);
			dib_texto_corto(f, 50, fy + 3, col_art - 56, tt, tinta);
		} else
			dib_texto_corto(f, 50, fy + 3, col_art - 56, c->titulo, tinta);
		dib_texto_corto(f, col_art, fy + 3, col_dur - col_art - 8, c->artista, tinta);
		if (c->duracion)
			a_tiempo(c->duracion, t, sizeof t);
		else
			copia(t, sizeof t, "");
		dib_texto(f, col_dur, fy + 3, t, tinta);
		zona(r_filas.x, fy, r_filas.an, ALTO_FILA, Z_FILA, k);
	}
	/* (arrastrando una fila: la raya de donde irá) */
	if (fila_arrastrada >= 0 && fila_destino >= lista_arriba && fila_destino <= lista_arriba + n) {
		int fy = r_filas.y + (fila_destino - lista_arriba) * ALTO_FILA;

		dib_rect(r_filas.x, fy - 1, r_filas.an, 3, C_AZUL2);
		dib_triangulo(r_filas.x, fy - 5, r_filas.x, fy + 5, r_filas.x + 7, fy, C_AZUL2);
	}
	dib_borde(r_filas.x - 1, r_filas.y - 23, r_filas.an + 2, r_filas.al + 24, C_BORDE);
	barra_vertical(r_filas.x + r_filas.an, r_filas.y, r_filas.al, L.n, n, lista_arriba, Z_BARRA_LISTA);
}

/* ---- el ecualizador ---- */
static rect r_bandas[BANDAS + 1];
static int eq_lista_arriba;
static int campo_activo;	/* 0 nada; 1 el nombre de un preajuste; 2 el nombre de una lista */
static char campo_texto[64];

static int valor_banda(int b)
{
	return b == 0 ? M.eq.preamp : M.eq.banda[b - 1];
}

static void pon_banda(int b, int v)
{
	if (v < -120)
		v = -120;
	if (v > 120)
		v = 120;
	if (b == 0)
		M.eq.preamp = v;
	else
		M.eq.banda[b - 1] = v;
	eq_cambia(&M.eq);
	eq_preajuste = -1;
	cambios_ajustes++;
}

static void pinta_deslizador_eq(int b)
{
	rect r = r_bandas[b];
	int v = valor_banda(b), cy, ky;
	char t[16];
	static const char *et[] = { "Pre.", "60", "170", "310", "600", "1 k", "3 k", "6 k", "12 k", "14 k", "16 k" };

	dib_rect(r.x, r.y, r.an, r.al, C_FONDO);
	fmt(t, sizeof t, "%c%d", v > 0 ? '+' : v < 0 ? '-' : ' ', (v < 0 ? -v : v) / 10);
	if ((v < 0 ? -v : v) % 10) {
		char d[4];

		fmt(d, sizeof d, ",%d", (v < 0 ? -v : v) % 10);
		anade(t, sizeof t, d);
	}
	dib_texto_centrado(&f_negrita12, r.x, r.y, r.an, t, C_TINTA);
	/* el carril, con sus rayas de 0 dB y ±6 y ±12 */
	cy = r.y + 20 + (r.al - 44) / 2;
	dib_rect(r.x + r.an / 2 - 2, r.y + 20, 5, r.al - 44, C_BORDE);
	dib_rect(r.x + r.an / 2 - 1, r.y + 21, 3, r.al - 46, 0xE8EEF6);
	dib_rect(r.x + 4, cy, r.an - 8, 1, C_SUAVE);
	dib_rect(r.x + 8, r.y + 20, r.an - 16, 1, 0xB4C3D8);
	dib_rect(r.x + 8, r.y + r.al - 25, r.an - 16, 1, 0xB4C3D8);
	ky = cy - v * ((r.al - 44) / 2) / 120;
	/* (relleno desde el 0: hacia arriba o hacia abajo) */
	if (ky < cy)
		dib_rect(r.x + r.an / 2 - 1, ky, 3, cy - ky, 0x3D8FE8);
	else if (ky > cy)
		dib_rect(r.x + r.an / 2 - 1, cy, 3, ky - cy, 0xE0843C);
	dib_redondo(r.x + 2, ky - 6, r.an - 4, 13, 4, 0x184A90);
	dib_redondo_degradado(r.x + 3, ky - 5, r.an - 6, 11, 3, b == 0 ? 0x7CD887 : 0x6DB4F8, b == 0 ? 0x2E9E40 : 0x2468C8);
	dib_rect(r.x + 6, ky - 1, r.an - 12, 1, C_BLANCO);
	dib_rect(r.x + 6, ky + 1, r.an - 12, 1, C_BLANCO);
	dib_texto_centrado(b == 0 ? &f_negrita12 : &f_normal12, r.x - 4, r.y + r.al - 18, r.an + 8, et[b], C_TINTA);
	zona(r.x, r.y, r.an, r.al, Z_EQ_BANDA, b);
	ensucia(r.x, r.y, r.an, r.al);
}

static const char *nombre_preajuste(int i)
{
	return i < n_preajustes ? preajustes[i].nombre : propios[i - n_preajustes].nombre;
}

static void pinta_eq(void)
{
	rect r = r_cont;
	int y = r.y + 8, x, i, n, lx = 8, lw = 170, sx, sw, ancho;
	char t[80];

	casilla(lx, y, "Ecualizador activado", M.eq.activo, Z_EQ_ACTIVO, 0);
	casilla(lx + 210, y, "Automático (por el género)", eq_auto, Z_EQ_AUTO, 0);
	if (lx + 470 + 130 < AN)
		boton(AN - 140, y - 4, 128, 26, S_NADA, "Todo a 0 dB", Z_EQ_PLANO, 0, 0);
	y += 28;
	/* los preajustes: una lista (los de siempre y los propios) */
	dib_texto(&f_negrita12, lx, y, "Preajustes", C_TINTA);
	y += 18;
	{
		int alto = r.y + r.al - y - 40, filas = alto / 20, total = n_preajustes + n_propios;

		dib_rect(lx, y, lw, filas * 20, C_BLANCO);
		dib_borde(lx - 1, y - 1, lw + 2, filas * 20 + 2, C_BORDE);
		if (eq_lista_arriba > total - filas)
			eq_lista_arriba = total - filas;
		if (eq_lista_arriba < 0)
			eq_lista_arriba = 0;
		for (i = 0; i < filas && eq_lista_arriba + i < total; i++) {
			int k = eq_lista_arriba + i, fy = y + i * 20, el = (k < n_preajustes && k == eq_preajuste) || (k >= n_preajustes && eq_preajuste == 1000 + k - n_preajustes);

			if (el) {
				dib_rect(lx, fy, lw, 20, C_ELEGIDO);
				dib_rect(lx, fy, 4, 20, C_FRANJA);
				simbolo(S_TOCAR, lx + 12, fy + 10, 4, C_AZUL2);
			}
			if (k >= n_preajustes)
				fmt(t, sizeof t, "%s (propio)", nombre_preajuste(k));
			else
				copia(t, sizeof t, nombre_preajuste(k));
			(el ? dib_texto_corto(&f_negrita12, lx + 22, fy + 3, lw - 26, t, C_TINTA) : dib_texto_corto(&f_normal12, lx + 22, fy + 3, lw - 26, t, C_TINTA));
			zona(lx, fy, lw, 20, Z_EQ_PREAJUSTE, k);
		}
		barra_vertical(lx + lw + 1, y, filas * 20, total, filas, eq_lista_arriba, Z_NADA);
		y += filas * 20 + 6;
	}
	/* guardar el de ahora con un nombre; borrar uno propio */
	if (campo_activo == 1) {
		dib_rect(lx, y, lw, 24, C_BLANCO);
		dib_borde(lx, y, lw, 24, C_AZUL2);
		dib_borde(lx + 1, y + 1, lw - 2, 22, C_AZUL2);
		fmt(t, sizeof t, "%s_", campo_texto);
		dib_texto_corto(&f_normal12, lx + 5, y + 4, lw - 10, t, C_TINTA);
		zona(lx, y, lw, 24, Z_CAMPO, 1);
	} else {
		boton(lx, y, 110, 26, S_GUARDAR, "Guardar...", Z_EQ_GUARDAR, 0, 0);
		boton(lx + 114, y, 74, 26, S_QUITAR, "Borrar", Z_EQ_BORRAR, 0, eq_preajuste >= 1000 ? 0 : 2);
	}
	/* los deslizadores: el preamplificador y las diez bandas */
	sx = lx + lw + 56;
	sw = AN - sx - 12;
	ancho = sw / 12;
	if (ancho > 44)
		ancho = 44;
	n = r.y + r.al - (r.y + 40) - 4;
	for (i = 0; i <= BANDAS; i++) {
		x = sx + i * ancho + (i ? ancho / 2 : 0);
		r_bandas[i].x = x;
		r_bandas[i].y = r.y + 40;
		r_bandas[i].an = ancho - 4;
		r_bandas[i].al = n;
		pinta_deslizador_eq(i);
	}
	/* (las rayas de los dB, a la izquierda) */
	{
		int cy = r_bandas[0].y + 20 + (n - 44) / 2, h = (n - 44) / 2;

		dib_texto(&f_normal12, sx - 30, cy - h - 7, "+12", C_SUAVE);
		dib_texto(&f_normal12, sx - 16, cy - 7, "0", C_SUAVE);
		dib_texto(&f_normal12, sx - 30, cy + h - 7, "-12", C_SUAVE);
		dib_texto(&f_normal12, sx - 30, r_bandas[0].y - 2, "dB", C_SUAVE);
		dib_texto(&f_normal12, sx - 30, r_bandas[0].y + n - 18, "Hz", C_SUAVE);
	}
}

/* ---- añadir: las unidades y las carpetas ---- */
static char exp_ruta[MAX_RUTA];
static so_entrada *exp_e;
static int exp_n, exp_cabe, exp_arriba, exp_cursor;
static u8 *exp_marca;
static int modo_guardar;	/* 1: se está eligiendo dónde guardar la lista */
static rect r_exp;

static int es_lista_o_sonido(const char *n)
{
	const char *e = dec_ext_de(n);

	return dec_ext_vale(n) || !strcmp(e, "M3U") || !strcmp(e, "M3U8") || !strcmp(e, "PLS");
}

static void exp_lee(void)
{
	void *c;
	so_entrada e;
	int i, j;

	exp_n = 0;
	exp_arriba = exp_cursor = 0;
	if (!exp_ruta[0])
		so_carpeta_musica(exp_ruta, sizeof exp_ruta);
	c = so_abre_carpeta(exp_ruta);
	if (!c)
		return;
	while (so_siguiente(c, &e)) {
		if (!e.carpeta && !es_lista_o_sonido(e.nombre))
			continue;
		if (exp_n >= exp_cabe) {
			int cabe = exp_cabe ? exp_cabe * 2 : 64;
			so_entrada *n = so_repide(exp_e, (size_t)cabe * sizeof(so_entrada));
			u8 *m = so_repide(exp_marca, (size_t)cabe);

			if (!n || !m)
				break;
			exp_e = n;
			exp_marca = m;
			exp_cabe = cabe;
		}
		exp_e[exp_n] = e;
		exp_marca[exp_n] = 0;
		exp_n++;
	}
	so_cierra_carpeta(c);
	/* las carpetas primero, y por nombre */
	for (i = 1; i < exp_n; i++) {
		so_entrada t = exp_e[i];

		for (j = i; j > 0 && (exp_e[j - 1].carpeta < t.carpeta || (exp_e[j - 1].carpeta == t.carpeta && compara_llano(exp_e[j - 1].visto, t.visto) > 0)); j--)
			exp_e[j] = exp_e[j - 1];
		exp_e[j] = t;
	}
}

static void exp_ruta_de(int i, char *d, int cabe)
{
	int n;

	copia(d, cabe, exp_ruta);
	n = (int)strlen(d);
	if (n && d[n - 1] != so_separador() && n < cabe - 1)
		d[n] = so_separador(), d[n + 1] = 0;
	anade(d, cabe, exp_e[i].nombre);
}

static void pinta_anadir(void)
{
	rect r = r_cont;
	int y = r.y + 6, x = 8, i, n;
	char t[MAX_RUTA + 40], letras[26], que[26];
	int nu = so_unidades(letras, que, 26);

	/* las unidades */
	for (i = 0; i < nu && x + 50 < AN; i++) {
		char l[8];

		fmt(l, sizeof l, "%c:", letras[i]);
		boton(x, y, 48, 26, que[i] == 'D' ? S_DISCO : S_NADA, l, Z_X_UNIDAD, (u8)letras[i],
		      exp_ruta[0] == letras[i] ? 3 : 0);
		x += 52;
	}
	if (x + 90 < AN)
		boton(x, y, 86, 26, S_SUBIR, "Subir", Z_X_SUBIR, 0, 0);
	y += 32;
	so_a_utf8(exp_ruta, t, sizeof t);
	{
		char tt[MAX_RUTA + 60];

		fmt(tt, sizeof tt, "%s%s", modo_guardar ? "Guardar la lista en: " : "", t);
		dib_texto_corto(&f_negrita12, 10, y, AN - 20, tt, C_TINTA);
	}
	y += 20;
	r_exp.x = 8;
	r_exp.y = y;
	r_exp.an = AN - 16 - 16;
	r_exp.al = r.y + r.al - y - 40;
	n = r_exp.al / ALTO_FILA;
	if (exp_arriba > exp_n - n)
		exp_arriba = exp_n - n;
	if (exp_arriba < 0)
		exp_arriba = 0;
	dib_rect(r_exp.x, r_exp.y, r_exp.an, r_exp.al, C_BLANCO);
	dib_borde(r_exp.x - 1, r_exp.y - 1, r_exp.an + 2, r_exp.al + 2, C_BORDE);
	if (!exp_n)
		dib_texto_centrado(&f_normal14, r_exp.x, r_exp.y + 20, r_exp.an, "No hay canciones ni carpetas aquí.", C_SUAVE);
	for (i = 0; i < n && exp_arriba + i < exp_n; i++) {
		int k = exp_arriba + i, fy = r_exp.y + i * ALTO_FILA;
		so_entrada *e = &exp_e[k];

		if (exp_marca[k]) {
			dib_rect(r_exp.x, fy, r_exp.an, ALTO_FILA, C_ELEGIDO);
			dib_rect(r_exp.x, fy, 4, ALTO_FILA, C_FRANJA);
		}
		if (k == exp_cursor)
			dib_borde(r_exp.x + 1, fy, r_exp.an - 2, ALTO_FILA, 0x5A86C8);
		/* una carpeta (amarilla, con su pestaña) o una nota */
		if (e->carpeta) {
			dib_rect(r_exp.x + 10, fy + 5, 8, 3, 0xC9962A);
			dib_rect(r_exp.x + 10, fy + 7, 18, 11, 0xE8B84A);
		} else
			simbolo(S_NOTA, r_exp.x + 19, fy + 11, 7, C_AZUL2);
		if (e->carpeta)
			fmt(t, sizeof t, "%s  (carpeta)", e->visto);
		else
			copia(t, sizeof t, e->visto);
		dib_texto_corto(e->carpeta ? &f_negrita12 : &f_normal12, r_exp.x + 36, fy + 3, r_exp.an - 120, t, C_TINTA);
		if (!e->carpeta) {
			if (e->tam >= 1024 * 1024)
				fmt(t, sizeof t, "%u,%u MB", (u32)(e->tam / 1048576), (u32)(e->tam % 1048576 * 10 / 1048576));
			else
				fmt(t, sizeof t, "%u K", (u32)((e->tam + 1023) / 1024));
			dib_texto(&f_normal12, r_exp.x + r_exp.an - 70, fy + 3, t, C_SUAVE);
		}
		zona(r_exp.x, fy, r_exp.an, ALTO_FILA, Z_X_FILA, k);
	}
	barra_vertical(r_exp.x + r_exp.an + 1, r_exp.y, r_exp.al, exp_n, n, exp_arriba, Z_X_BARRA);
	y = r_exp.y + r_exp.al + 8;
	if (modo_guardar) {
		int w = 220;

		dib_texto(&f_negrita12, 8, y + 5, "Nombre:", C_TINTA);
		dib_rect(70, y, w, 26, C_BLANCO);
		dib_borde(70, y, w, 26, C_AZUL2);
		dib_borde(71, y + 1, w - 2, 24, C_AZUL2);
		fmt(t, sizeof t, "%s_", campo_texto);
		dib_texto_corto(&f_normal12, 76, y + 5, w - 10, t, C_TINTA);
		zona(70, y, w, 26, Z_X_CAMPO, 0);
		boton(70 + w + 8, y, 150, 28, S_GUARDAR, "Guardar aquí", Z_X_GUARDAR, 0, 1);
		boton(70 + w + 164, y, 100, 28, S_NADA, "Cancelar", Z_X_GUARDAR, 1, 0);
	} else {
		boton(8, y, 170, 28, S_MAS, "Añadir lo elegido", Z_X_ANADIR, 0, 0);
		boton(184, y, 190, 28, S_CARPETA, "Añadir la carpeta", Z_X_CARPETA, 0, 0);
		if (380 + 150 < AN)
			boton(380, y, 150, 28, S_TOCAR, "Tocar ahora", Z_X_TOCAR, 0, 1);
	}
}

/* ---- las opciones ---- */
static void pinta_opciones(void)
{
	rect r = r_cont;
	int y = r.y + 10, x, y2, cx = AN / 2 + 10;
	char t[200];
	static const int fundidos[] = { 0, 2, 4, 6, 8, 10 };
	static const int vel[] = { 50, 75, 100, 125, 150, 175, 200 };
	int i;

	/* a la izquierda: entre canciones y el volumen igualado */
	dib_texto(&f_negrita14, 12, y, "Entre canciones", C_TINTA);
	y += 22;
	casilla(24, y, "Sin huecos (los discos seguidos)", M.sin_huecos, Z_O_HUECOS, 0);
	y += 24;
	dib_texto(&f_normal14, 24, y, "Fundido:", C_TINTA);
	y += 20;
	x = 24;
	for (i = 0; i < 6; i++) {
		if (fundidos[i])
			fmt(t, sizeof t, "%d s", fundidos[i]);
		else
			copia(t, sizeof t, "No");
		x = opcion(x, y, t, M.fundido == fundidos[i], Z_O_FUNDIDO, fundidos[i]) - 6;
	}
	y += 30;
	dib_texto(&f_negrita14, 12, y, "Volumen igualado (ReplayGain)", C_TINTA);
	y += 22;
	x = opcion(24, y, "No", M.replaygain == RG_NO, Z_O_RG, RG_NO);
	x = opcion(x, y, "Por canción", M.replaygain == RG_PISTA, Z_O_RG, RG_PISTA);
	opcion(x, y, "Por disco", M.replaygain == RG_DISCO, Z_O_RG, RG_DISCO);
	y += 26;
	/* a la derecha: la visualización y la velocidad */
	y2 = r.y + 10;
	dib_texto(&f_negrita14, cx, y2, "Visualización (o un clic en ella)", C_TINTA);
	y2 += 22;
	opcion(cx + 12, y2, "Espectro (barras)", vis_modo == 1, Z_O_VIS, 1);
	y2 += 22;
	opcion(cx + 12, y2, "Onda (osciloscopio)", vis_modo == 2, Z_O_VIS, 2);
	y2 += 22;
	opcion(cx + 12, y2, "Apagada (gasta menos)", vis_modo == 0, Z_O_VIS, 0);
	y2 += 30;
	dib_texto(&f_negrita14, cx, y2, "Velocidad (sin cambiar el tono)", C_TINTA);
	y2 += 22;
	x = cx + 12;
	for (i = 0; i < 7; i++) {
		if (vel[i] % 100 == 0)
			fmt(t, sizeof t, "%d×", vel[i] / 100);
		else if (vel[i] % 10 == 0)
			fmt(t, sizeof t, "%d,%d×", vel[i] / 100, vel[i] / 10 % 10);
		else
			fmt(t, sizeof t, "%d,%d×", vel[i] / 100, vel[i] % 100);
		if (i == 4) {
			x = cx + 12;
			y2 += 22;
		}
		x = opcion(x, y2, t, (int)(M.velocidad * 100 + 0.5f) == vel[i], Z_O_VELOCIDAD, vel[i]);
	}
	y2 += 26;
	if (y2 > y)
		y = y2;
	/* abajo, las teclas y de dónde viene */
	if (y + 36 < r.y + r.al) {
		dib_texto_corto(&f_normal12, 12, y, AN - 20, "Teclas: Espacio, tocar o pausa · N y P, siguiente y anterior · S, parar · flechas, 5 s (con Mayús, 30 s)", C_TINTA);
		y += 16;
		dib_texto_corto(&f_normal12, 12, y, AN - 20, "+ y - (o Ctrl+flechas), volumen · Ctrl+O, añadir · Ctrl+S, guardar la lista · Ctrl+M, pequeño · Supr, quitar · Tab, otra pestaña", C_TINTA);
		y += 20;
	}
	fmt(t, sizeof t, "Reproductor %s de Hispa-Pentium · GPL-2.0 o posterior · con minimp3, stb_vorbis, dr_flac, libxmp-lite y stb_image (libres)", VERSION_REPRO);
	if (y + 16 < r.y + r.al)
		dib_texto_corto(&f_normal12, 12, y, AN - 20, t, C_SUAVE);
}

static void pinta_estado(void)
{
	rect r = r_estado;
	char t[300], d[24];
	int sin = 0;
	u32 tot = lista_duracion(&L, &sin);

	dib_degradado(r.x, r.y, r.an, r.al, 0xE4ECF6, 0xCFDCEC);
	dib_rect(r.x, r.y, r.an, 1, C_BORDE);
	if (aviso[0] && (int)(aviso_hasta - so_ms()) > 0) {
		dib_rect(r.x + 1, r.y + 1, r.an - 2, r.al - 1, C_AVISO);
		dib_rect(r.x + 1, r.y + 1, 4, r.al - 1, 0xE0843C);
		dib_texto_corto(&f_normal12, 10, r.y + 4, AN - 20, aviso, C_TINTA);
	} else {
		a_tiempo(tot, d, sizeof d);
		fmt(t, sizeof t, "%d canciones · %s%s", L.n, d, sin ? " (y lo que no se sabe aún)" : "");
		if (L.n_pendientes)
			anade(t, sizeof t, " · mirando las carpetas...");
		dib_texto(&f_normal12, 10, r.y + 4, t, C_TINTA);
		if (M.eq.activo) {
			fmt(t, sizeof t, "Ecualizador: %s", eq_preajuste >= 1000 ? propios[eq_preajuste - 1000].nombre : eq_preajuste >= 0 ? preajustes[eq_preajuste].nombre : "a mano");
			dib_texto(&f_normal12, AN - dib_ancho(&f_normal12, t) - 10, r.y + 4, t, C_TINTA);
		}
	}
	ensucia(r.x, r.y, r.an, r.al);
}

/* ---- el modo pequeño ---- */
#define MINI_AN 520
#define MINI_AL 76
static int desplaza_titulo;

static void pinta_mini_titulo(void)
{
	char t[300], tt[340];
	cancion *c = oida();
	int w, x0 = 8, an = AN - 8 - 118;

	dib_rect(0, 0, AN, 30, C_PANTALLA_A);
	dib_degradado(0, 0, AN, 30, C_PANTALLA_A, C_PANTALLA_B);
	if (c)
		lista_nombre(c, t, sizeof t);
	else
		copia(t, sizeof t, "Reproductor");
	w = dib_ancho(&f_negrita14, t);
	dib_recorte(x0, 0, an, 30);
	if (w <= an)
		dib_texto(&f_negrita14, x0, 6, t, C_BLANCO);
	else {
		/* se desplaza (y vuelve a empezar tras un hueco) */
		int p = desplaza_titulo % (w + 60);

		fmt(tt, sizeof tt, "%s", t);
		dib_texto(&f_negrita14, x0 - p, 6, tt, C_BLANCO);
		dib_texto(&f_negrita14, x0 - p + w + 60, 6, tt, C_BLANCO);
	}
	dib_recorte(0, 0, 0, 0);
	{
		char a[16], b[16];
		u32 tot = motor_total_ms(&M);

		a_tiempo(motor_pos_ms(&M) / 1000, a, sizeof a);
		if (tot)
			a_tiempo(tot / 1000, b, sizeof b);
		else
			copia(b, sizeof b, "--:--");
		fmt(t, sizeof t, "%s / %s", a, b);
		dib_texto(&f_negrita12, AN - 112, 8, t, C_LUZ);
	}
	/* la posición: una raya fina debajo */
	{
		u32 p = motor_pos_ms(&M), tot = motor_total_ms(&M);
		int k = tot ? (int)((u64)(p > tot ? tot : p) * (u64)AN / tot) : 0;

		dib_rect(0, 30, AN, 4, 0xC9D6E8);
		dib_rect(0, 30, k, 4, 0x3D8FE8);
		zona(0, 28, AN, 8, Z_POS, 0 << 16 | AN);
	}
	ensucia(0, 0, AN, 34);
}

static void pinta_mini(void)
{
	int x = 8, y = 38;

	n_zonas = 0;
	dib_rect(0, 0, AN, AL, C_FONDO);
	pinta_mini_titulo();
	boton(x, y, 40, 32, S_ANTERIOR, NULL, Z_ANTERIOR, 0, 0);
	x += 44;
	boton(x, y, 48, 32, M.estado == M_SONANDO ? S_PAUSA : S_TOCAR, NULL, Z_TOCAR, 0, 1);
	x += 52;
	boton(x, y, 40, 32, S_PARAR, NULL, Z_PARAR, 0, M.estado == M_PARADO ? 2 : 0);
	x += 44;
	boton(x, y, 40, 32, S_SIGUIENTE, NULL, Z_SIGUIENTE, 0, 0);
	x += 52;
	r_vol.x = x + 28;
	r_vol.y = y + 2;
	r_vol.an = AN - r_vol.x - 58 - 96;
	if (r_vol.an > 140)
		r_vol.an = 140;
	r_vol.al = 28;
	pinta_volumen();
	boton(AN - 92, y, 84, 32, S_COMPLETO, "Grande", Z_COMPLETO, 0, 0);
	todo_sucio = 1;
}

static void pinta_todo(void)
{
	dib_lienzo(&LZ);
	if (mini) {
		pinta_mini();
		return;
	}
	n_zonas = 0;
	disponer();
	dib_rect(0, 0, AN, AL, C_FONDO);
	pinta_pantalla();
	pinta_posicion();
	pinta_botones();
	pinta_pestanas();
	dib_rect(r_cont.x, r_cont.y, r_cont.an, r_cont.al, C_FONDO);
	switch (pestana) {
	case P_LISTA: pinta_lista(); break;
	case P_EQ: pinta_eq(); break;
	case P_ANADIR: pinta_anadir(); break;
	case P_OPCIONES: pinta_opciones(); break;
	}
	pinta_estado();
	todo_sucio = 1;
}

/* ===================================================================== lo guardado */
static void ruta_propia(char *d, int cabe, const char *nombre)
{
	copia(d, cabe, carpeta_propia);
	anade(d, cabe, nombre);
}

static void guarda_ajustes(void)
{
	char r[MAX_RUTA], t[2048], k[200];
	so_archivo *a;
	int i, b;

	if (!so_existe(carpeta_propia, NULL))
		so_haz_carpeta(carpeta_propia);
	ruta_propia(r, sizeof r, "REPRODUC.INI");
	a = so_crea(r);
	if (!a)
		return;
	fmt(t, sizeof t, "; el Reproductor de Hispa-Pentium: lo último que se usó\r\nvolumen=%d\r\naleatorio=%d\r\nrepetir=%d\r\n"
	    "ecualizador=%d\r\nautomatico=%d\r\npreajuste=%d\r\npreamp=%d\r\n", M.volumen, L.aleatorio, L.repetir, M.eq.activo, eq_auto, eq_preajuste, M.eq.preamp);
	for (b = 0; b < BANDAS; b++) {
		fmt(k, sizeof k, "banda%d=%d\r\n", b + 1, M.eq.banda[b]);
		anade(t, sizeof t, k);
	}
	fmt(k, sizeof k, "velocidad=%d\r\nfundido=%d\r\nsin_huecos=%d\r\nreplaygain=%d\r\nvisual=%d\r\npequeno=%d\r\npestana=%d\r\nancho=%d\r\nalto=%d\r\n",
	    (int)(M.velocidad * 100 + 0.5f), M.fundido, M.sin_huecos, M.replaygain, vis_modo, mini, pestana, mini ? 0 : AN, mini ? 0 : AL);
	anade(t, sizeof t, k);
	fmt(k, sizeof k, "cancion=%d\r\nposicion=%u\r\n", M.oida >= 0 ? M.oida : L.actual, M.estado != M_PARADO ? motor_pos_ms(&M) / 1000 : 0);
	anade(t, sizeof t, k);
	so_escribe(a, t, (u32)strlen(t));
	so_cierra(a);
	/* los preajustes propios */
	ruta_propia(r, sizeof r, "ECUALIZA.TXT");
	a = so_crea(r);
	if (a) {
		for (i = 0; i < n_propios; i++) {
			char n2[60];

			so_de_utf8(propios[i].nombre, n2, sizeof n2);
			fmt(t, sizeof t, "%s=%d", n2, propios[i].v[0]);
			for (b = 1; b <= BANDAS; b++) {
				fmt(k, sizeof k, ",%d", propios[i].v[b]);
				anade(t, sizeof t, k);
			}
			anade(t, sizeof t, "\r\n");
			so_escribe(a, t, (u32)strlen(t));
		}
		so_cierra(a);
	}
	/* la lista (con las pistas de los CD no: el disco puede no estar) */
	ruta_propia(r, sizeof r, "LISTA.M3U");
	lista_guarda(&L, r);
	cambios_ajustes = 0;
}

static int ini_entero(const char *t, const char *clave, int si_no)
{
	const char *p = t;
	int n = (int)strlen(clave);

	while (p && *p) {
		if (!strncmp(p, clave, (size_t)n) && p[n] == '=')
			return a_entero(p + n + 1);
		p = strchr(p, '\n');
		if (p)
			p++;
	}
	return si_no;
}

static int cancion_guardada, posicion_guardada;

static void lee_ajustes(void)
{
	char r[MAX_RUTA];
	u32 n;
	u8 *t;
	int b;

	ruta_propia(r, sizeof r, "REPRODUC.INI");
	t = so_lee_todo(r, &n, 16384);
	if (t) {
		const char *s = (const char *)t;

		motor_volumen(&M, ini_entero(s, "volumen", 80));
		L.aleatorio = ini_entero(s, "aleatorio", 0) != 0;
		L.repetir = ini_entero(s, "repetir", 0) % 3;
		M.eq.activo = ini_entero(s, "ecualizador", 0) != 0;
		eq_auto = ini_entero(s, "automatico", 0) != 0;
		eq_preajuste = ini_entero(s, "preajuste", 0);
		M.eq.preamp = ini_entero(s, "preamp", 0);
		for (b = 0; b < BANDAS; b++) {
			char k[16];

			fmt(k, sizeof k, "banda%d", b + 1);
			M.eq.banda[b] = ini_entero(s, k, 0);
			if (M.eq.banda[b] < -120 || M.eq.banda[b] > 120)
				M.eq.banda[b] = 0;
		}
		eq_cambia(&M.eq);
		b = ini_entero(s, "velocidad", 100);
		if (b >= 50 && b <= 200 && b != 100)
			motor_velocidad(&M, (float)b / 100.0f);
		M.fundido = ini_entero(s, "fundido", 0);
		if (M.fundido < 0 || M.fundido > 10)
			M.fundido = 0;
		M.sin_huecos = ini_entero(s, "sin_huecos", 1) != 0;
		M.replaygain = ini_entero(s, "replaygain", RG_PISTA) % 3;
		vis_modo = ini_entero(s, "visual", 1) % 3;
		mini = ini_entero(s, "pequeno", 0) != 0;
		pestana = ini_entero(s, "pestana", 0) & 3;
		AN = ini_entero(s, "ancho", 0);
		AL = ini_entero(s, "alto", 0);
		cancion_guardada = ini_entero(s, "cancion", -1);
		posicion_guardada = ini_entero(s, "posicion", 0);
		so_suelta(t);
	}
	ruta_propia(r, sizeof r, "ECUALIZA.TXT");
	t = so_lee_todo(r, &n, 16384);
	if (t) {
		char *p = (char *)t;

		while (*p && n_propios < MAX_PROPIOS) {
			char *fin = strchr(p, '\n'), *ig;

			if (fin)
				*fin = 0;
			ig = strchr(p, '=');
			if (ig && ig > p) {
				char *q = ig + 1;
				int k;

				*ig = 0;
				so_a_utf8(p, propios[n_propios].nombre, sizeof propios[0].nombre);
				for (k = 0; k <= BANDAS; k++) {
					int v = a_entero(q);

					propios[n_propios].v[k] = (signed char)(v < -12 ? -12 : v > 12 ? 12 : v);
					q = strchr(q, ',');
					if (!q)
						break;
					q++;
				}
				n_propios++;
			}
			if (!fin)
				break;
			p = fin + 1;
		}
		so_suelta(t);
	}
}

/* ===================================================================== hacer */
static void cambia_modo(int pequeno)
{
	static int an_grande = 760, al_grande = 560;

	if (pequeno == mini)
		return;
	if (pequeno) {
		an_grande = AN;
		al_grande = AL;
	}
	mini = pequeno;
	so_cambia_tamano(V, mini ? MINI_AN : an_grande, mini ? MINI_AL : al_grande);
	cambios_ajustes++;
}

static void nuevo_lienzo(int an, int al)
{
	if (an < 200)
		an = 200;
	if (al < 60)
		al = 60;
	if (an == LZ.an && al == LZ.al)
		return;
	so_suelta(LZ.p);
	LZ.an = AN = an;
	LZ.al = AL = al;
	LZ.p = so_pide((size_t)an * al * 4);
	if (!LZ.p) {
		/* (sin memoria para el lienzo: uno pequeño) */
		LZ.an = AN = 320;
		LZ.al = AL = 200;
		LZ.p = so_pide((size_t)AN * AL * 4);
	}
}

static void tocar_cancion(int k)
{
	if (k < 0 || k >= L.n)
		return;
	motor_toca(&M, k);
	lista_cursor = k;
	asegura_visible(k);
}

static void anade_cd(void)
{
	char l[8];
	int n = so_lectores_cd(l, 8), i, hechas = 0;

	for (i = 0; i < n; i++) {
		so_toc t;

		if (!so_cd_toc(l[i], &t) && t.pistas) {
			int k = lista_anade_cd(&L, l[i], &t);
			char a[100];

			fmt(a, sizeof a, "El CD de música de %c: (%d pistas), a la lista.", l[i], k);
			avisa(a);
			hechas += k;
		}
	}
	if (!n)
		avisa("No hay ningún lector de CD.");
	else if (!hechas)
		avisa("No hay ningún CD de música en el lector (o no se puede leer).");
}

static void quitar_elegidas(void)
{
	int i, hay = 0;

	for (i = 0; i < L.n; i++)
		hay |= L.c[i].marcada;
	if (!hay && lista_cursor >= 0 && lista_cursor < L.n)
		L.c[lista_cursor].marcada = 1;
	/* (la que suena no se para: sigue hasta acabar) */
	for (i = L.n - 1; i >= 0; i--)
		if (L.c[i].marcada) {
			if (i == M.oida) {
				M.oida = -1;
			} else if (M.oida > i)
				M.oida--;
			if (M.v[0].cancion == i) M.v[0].cancion = -3; else if (M.v[0].cancion > i) M.v[0].cancion--;
			if (M.v[1].cancion == i) M.v[1].cancion = -3; else if (M.v[1].cancion > i) M.v[1].cancion--;
			lista_quita(&L, i);
		}
	if (lista_cursor >= L.n)
		lista_cursor = L.n - 1;
}

static void marca_fila(int k, int mods)
{
	int i;

	if (mods & CON_MAYUS && lista_ancla >= 0) {
		int a = lista_ancla < k ? lista_ancla : k, b = lista_ancla < k ? k : lista_ancla;

		for (i = 0; i < L.n; i++)
			L.c[i].marcada = i >= a && i <= b;
	} else if (mods & CON_CONTROL) {
		L.c[k].marcada = !L.c[k].marcada;
		lista_ancla = k;
	} else {
		for (i = 0; i < L.n; i++)
			L.c[i].marcada = i == k;
		lista_ancla = k;
	}
	lista_cursor = k;
}

static void anade_y_toca(const char *ruta, int tocar)
{
	int antes = L.n;

	anade_argumento(&L, ruta);
	while (lista_paso(&L) && L.n - antes < 2)
		;	/* (lo primero, ya; lo demás, de a poco) */
	if (tocar && L.n > antes)
		tocar_cancion(antes);
}

static void accion(int id, int dato, int x, int y, int veces, int mods);

static void velocidad_siguiente(void)
{
	static const int vel[] = { 50, 75, 100, 125, 150, 175, 200 };
	int c = (int)(M.velocidad * 100 + 0.5f), i;

	for (i = 0; i < 7; i++)
		if (vel[i] > c)
			break;
	motor_velocidad(&M, (float)vel[i < 7 ? i : 0] / 100.0f);
	cambios_ajustes++;
}

static void guarda_lista_en(const char *nombre)
{
	char r[MAX_RUTA], n[80];
	int k;

	so_de_utf8(nombre, n, sizeof n);
	if (!n[0])
		return;
	if (!strchr(n, '.'))
		anade(n, sizeof n, ".M3U");
	copia(r, sizeof r, exp_ruta);
	k = (int)strlen(r);
	if (k && r[k - 1] != so_separador() && k < (int)sizeof r - 1)
		r[k] = so_separador(), r[k + 1] = 0;
	anade(r, sizeof r, n);
	if (lista_guarda(&L, r))
		avisa("No se puede guardar ahí (¿de solo lectura, o el nombre no vale? 8 letras y la extensión).");
	else {
		char t[MAX_RUTA + 40], u[MAX_RUTA];

		so_a_utf8(r, u, sizeof u);
		fmt(t, sizeof t, "Lista guardada: %s", u);
		avisa(t);
		modo_guardar = 0;
		exp_lee();
	}
}

static void accion(int id, int dato, int x, int y, int veces, int mods)
{
	int i;
	char r[MAX_RUTA];

	(void)y;
	switch (id) {
	case Z_ANTERIOR: motor_anterior(&M); break;
	case Z_SIGUIENTE: motor_siguiente(&M); break;
	case Z_TOCAR:
		if (M.estado == M_PARADO && lista_cursor >= 0 && lista_cursor < L.n && L.c[lista_cursor].marcada)
			tocar_cancion(lista_cursor);
		else
			motor_pausa(&M);
		break;
	case Z_PARAR: motor_para(&M); break;
	case Z_ALEATORIO:
		L.aleatorio = !L.aleatorio;
		so_suelta(L.orden);
		L.orden = NULL;
		cambios_ajustes++;
		break;
	case Z_REPETIR:
		L.repetir = (L.repetir + 1) % 3;
		cambios_ajustes++;
		break;
	case Z_VELOCIDAD: velocidad_siguiente(); break;
	case Z_MINI: cambia_modo(1); break;
	case Z_COMPLETO: cambia_modo(0); break;
	case Z_PESTANA:
		pestana = dato;
		campo_activo = 0;
		if (pestana == P_ANADIR && !exp_n)
			exp_lee();
		cambios_ajustes++;
		break;
	case Z_VIS:
		vis_modo = (vis_modo + 1) % 3;
		cambios_ajustes++;
		break;
	case Z_TIEMPO:
		ver_restante = !ver_restante;
		break;
	case Z_FILA:
		if (veces >= 2)
			tocar_cancion(dato);
		else {
			marca_fila(dato, mods);
			/* (y puede ser el principio de arrastrarla a otro sitio) */
			fila_arrastrada = dato;
			fila_destino = -1;
		}
		break;
	case Z_L_ANADIR:
		pestana = P_ANADIR;
		modo_guardar = 0;
		if (!exp_n)
			exp_lee();
		break;
	case Z_L_CD: anade_cd(); break;
	case Z_L_ABRIR:
		pestana = P_ANADIR;
		modo_guardar = 0;
		exp_lee();
		avisa("Elija una lista (.M3U o .PLS) y «Añadir lo elegido» (o dos clics).");
		break;
	case Z_L_GUARDAR:
		if (!L.n)
			break;
		pestana = P_ANADIR;
		modo_guardar = 1;
		copia(campo_texto, sizeof campo_texto, "LISTA.M3U");
		campo_activo = 2;
		exp_lee();
		break;
	case Z_L_QUITAR: quitar_elegidas(); break;
	case Z_L_VACIAR:
		motor_para(&M);
		M.oida = -1;
		lista_vacia(&L);
		lista_cursor = 0;
		lista_arriba = 0;
		break;
	case Z_L_ORDENAR:
		so_menu(V, x, r_cont.y + 36, "Por título|Por artista|Por carpeta y nombre", 0, 1);
		break;
	case Z_EQ_ACTIVO:
		M.eq.activo = !M.eq.activo;
		eq_vacia(&M.eq);
		cambios_ajustes++;
		break;
	case Z_EQ_AUTO:
		eq_auto = !eq_auto;
		if (eq_auto)
			eq_por_genero();
		cambios_ajustes++;
		break;
	case Z_EQ_PLANO:
		pon_eq(preajustes[0].v);
		eq_preajuste = 0;
		break;
	case Z_EQ_PREAJUSTE:
		if (dato < n_preajustes) {
			pon_eq(preajustes[dato].v);
			eq_preajuste = dato;
		} else {
			pon_eq(propios[dato - n_preajustes].v);
			eq_preajuste = 1000 + dato - n_preajustes;
		}
		if (!M.eq.activo) {
			M.eq.activo = 1;
			eq_vacia(&M.eq);
		}
		break;
	case Z_EQ_GUARDAR:
		campo_activo = 1;
		campo_texto[0] = 0;
		break;
	case Z_EQ_BORRAR:
		if (eq_preajuste >= 1000 && eq_preajuste - 1000 < n_propios) {
			i = eq_preajuste - 1000;
			memmove(&propios[i], &propios[i + 1], (size_t)(n_propios - i - 1) * sizeof propios[0]);
			n_propios--;
			eq_preajuste = -1;
			cambios_ajustes++;
		}
		break;
	case Z_EQ_BANDA:
		if (veces >= 2)
			pon_banda(dato, 0);
		else {
			arrastre = Z_EQ_BANDA;
			arrastre_dato = dato;
		}
		break;
	case Z_X_UNIDAD:
		fmt(exp_ruta, sizeof exp_ruta, "%c:%c", dato, so_separador());
		exp_lee();
		break;
	case Z_X_SUBIR: {
		int n = (int)strlen(exp_ruta);

		if (n > 3) {
			if (exp_ruta[n - 1] == so_separador())
				exp_ruta[--n] = 0;
			while (n > 0 && exp_ruta[n - 1] != so_separador())
				n--;
			if (n > 3)
				n--;
			exp_ruta[n] = 0;
			exp_lee();
		}
		break;
	}
	case Z_X_FILA:
		if (dato < 0 || dato >= exp_n)
			break;
		if (veces >= 2) {
			exp_ruta_de(dato, r, sizeof r);
			if (exp_e[dato].carpeta) {
				copia(exp_ruta, sizeof exp_ruta, r);
				exp_lee();
			} else if (!modo_guardar)
				anade_y_toca(r, M.estado == M_PARADO);
			else
				so_a_utf8(exp_e[dato].nombre, campo_texto, sizeof campo_texto);
		} else {
			if (!(mods & CON_CONTROL))
				for (i = 0; i < exp_n; i++)
					exp_marca[i] = 0;
			exp_marca[dato] = !exp_marca[dato] || !(mods & CON_CONTROL);
			exp_cursor = dato;
			if (modo_guardar && !exp_e[dato].carpeta)
				so_a_utf8(exp_e[dato].nombre, campo_texto, sizeof campo_texto);
		}
		break;
	case Z_X_ANADIR:
	case Z_X_TOCAR: {
		int antes = L.n, n = 0;

		for (i = 0; i < exp_n; i++)
			if (exp_marca[i]) {
				exp_ruta_de(i, r, sizeof r);
				anade_argumento(&L, r);
				n++;
			}
		if (!n && exp_cursor >= 0 && exp_cursor < exp_n) {
			exp_ruta_de(exp_cursor, r, sizeof r);
			anade_argumento(&L, r);
			n++;
		}
		for (i = 0; i < exp_n; i++)
			exp_marca[i] = 0;
		if (id == Z_X_TOCAR) {
			while (L.n == antes && lista_paso(&L))
				;
			if (L.n > antes)
				tocar_cancion(antes);
		}
		{
			char t[80];

			fmt(t, sizeof t, "Añadido a la lista (%d).", n);
			avisa(t);
		}
		break;
	}
	case Z_X_CARPETA:
		lista_anade_carpeta(&L, exp_ruta);
		avisa("Se añade la carpeta (con lo de dentro): mire la barra de estado.");
		break;
	case Z_X_GUARDAR:
		if (dato == 1) {
			modo_guardar = 0;
			campo_activo = 0;
		} else
			guarda_lista_en(campo_texto);
		break;
	case Z_X_CAMPO:
		campo_activo = 2;
		break;
	case Z_O_HUECOS:
		M.sin_huecos = !M.sin_huecos;
		cambios_ajustes++;
		break;
	case Z_O_FUNDIDO:
		M.fundido = dato;
		cambios_ajustes++;
		break;
	case Z_O_RG:
		M.replaygain = dato;
		cambios_ajustes++;
		avisa("ReplayGain: desde la próxima canción.");
		break;
	case Z_O_VIS:
		vis_modo = dato;
		cambios_ajustes++;
		break;
	case Z_O_VELOCIDAD:
		motor_velocidad(&M, (float)dato / 100.0f);
		cambios_ajustes++;
		break;
	}
}

/* el valor de un deslizador por la x (o la y) del ratón */
static void arrastra(int x, int y)
{
	if (arrastre == Z_POS) {
		int bx, bw, i;

		for (i = 0; i < n_zonas; i++)
			if (zonas[i].id == Z_POS)
				break;
		if (i >= n_zonas)
			return;
		bx = zonas[i].dato >> 16;
		bw = zonas[i].dato & 0xFFFF;
		if (bw <= 0)
			return;
		{
			int k = x - bx;
			u32 tot = motor_total_ms(&M);

			if (k < 0)
				k = 0;
			if (k > bw)
				k = bw;
			pos_arrastre = (u32)((u64)tot * (u64)k / (u64)bw);
		}
		if (mini)
			pinta_mini_titulo();
		else
			pinta_posicion();
	} else if (arrastre == Z_VOLUMEN) {
		int k = (x - r_vol.x) * 100 / (r_vol.an ? r_vol.an : 1);

		motor_volumen(&M, k);
		pinta_volumen();
		cambios_ajustes++;
	} else if (arrastre == Z_EQ_BANDA) {
		rect r = r_bandas[arrastre_dato];
		int cy = r.y + 20 + (r.al - 44) / 2, h = (r.al - 44) / 2;

		if (h > 0) {
			int v = (cy - y) * 120 / h;

			/* (de medio en medio dB) */
			v = (v >= 0 ? v + 2 : v - 2) / 5 * 5;
			pon_banda(arrastre_dato, v);
			pinta_deslizador_eq(arrastre_dato);
			pinta_estado();
		}
	} else if (arrastre == Z_BARRA_LISTA || arrastre == Z_X_BARRA) {
		int y0 = arrastre_dato >> 16, al = arrastre_dato & 0xFFFF;
		int total = arrastre == Z_BARRA_LISTA ? L.n : exp_n, vis = arrastre == Z_BARRA_LISTA ? filas_visibles() : r_exp.al / ALTO_FILA;

		if (al > 0 && total > vis) {
			int a = (y - y0) * (total - vis) / al;

			if (a < 0)
				a = 0;
			if (arrastre == Z_BARRA_LISTA)
				lista_arriba = a;
			else
				exp_arriba = a;
			pinta_todo();
		}
	}
}

static void suelta_arrastre(int x, int y)
{
	(void)y;
	if (arrastre == Z_POS) {
		motor_busca_ms(&M, pos_arrastre);
	}
	(void)x;
	arrastre = 0;
}

static void tecla(u32 k, u32 mods)
{
	int ctrl = (mods & 2) != 0 || (mods & CON_CONTROL) != 0, mayus = (mods & 1) != 0 || (mods & CON_MAYUS) != 0;

	/* un campo de texto: las letras van a él */
	if (campo_activo) {
		int n = (int)strlen(campo_texto);

		if (k == 27) {
			campo_activo = 0;
			modo_guardar = 0;
		} else if (k == '\r' || k == '\n') {
			if (campo_activo == 1 && campo_texto[0] && n_propios < MAX_PROPIOS) {
				int b;

				copia(propios[n_propios].nombre, sizeof propios[0].nombre, campo_texto);
				propios[n_propios].v[0] = (signed char)(M.eq.preamp / 10);
				for (b = 0; b < BANDAS; b++)
					propios[n_propios].v[b + 1] = (signed char)(M.eq.banda[b] / 10);
				eq_preajuste = 1000 + n_propios;
				n_propios++;
				cambios_ajustes++;
				avisa("Ajuste del ecualizador guardado.");
			} else if (campo_activo == 2) {
				guarda_lista_en(campo_texto);
				return;
			}
			campo_activo = 0;
		} else if (k == 8) {
			while (n > 0 && ((u8)campo_texto[n - 1] & 0xC0) == 0x80)
				n--;
			if (n > 0)
				campo_texto[n - 1] = 0;
		} else if (k >= 32 && k < 256 && n < (int)sizeof campo_texto - 4) {
			char c[2] = { (char)k, 0 }, u[8];

			so_a_utf8(c, u, sizeof u);
			anade(campo_texto, sizeof campo_texto, u);
		}
		return;
	}
	if (ctrl && (k == 'o' || k == 'O' || k == 15)) {
		accion(Z_L_ANADIR, 0, 0, 0, 1, 0);
		return;
	}
	if (ctrl && (k == 's' || k == 'S' || k == 19)) {
		accion(Z_L_GUARDAR, 0, 0, 0, 1, 0);
		return;
	}
	if (ctrl && (k == 'm' || k == 'M' || k == 13)) {
		cambia_modo(!mini);
		return;
	}
	if (ctrl && (k == 'l' || k == 'L' || k == 12)) {
		pestana = P_LISTA;
		return;
	}
	if (ctrl && (k == 'e' || k == 'E' || k == 5)) {
		pestana = P_EQ;
		return;
	}
	if (ctrl && (k == 'a' || k == 'A' || k == 1) && pestana == P_LISTA) {
		int i;

		for (i = 0; i < L.n; i++)
			L.c[i].marcada = 1;
		return;
	}
	if (ctrl && k == T_ARRIBA) {
		motor_volumen(&M, M.volumen + 5);
		cambios_ajustes++;
		return;
	}
	if (ctrl && k == T_ABAJO) {
		motor_volumen(&M, M.volumen - 5);
		cambios_ajustes++;
		return;
	}
	switch (k) {
	case ' ': accion(Z_TOCAR, 0, 0, 0, 1, 0); break;
	case 'n': case 'N': motor_siguiente(&M); break;
	case 'p': case 'P': motor_anterior(&M); break;
	case 's': case 'S': motor_para(&M); break;
	case '+': motor_volumen(&M, M.volumen + 5); cambios_ajustes++; break;
	case '-': motor_volumen(&M, M.volumen - 5); cambios_ajustes++; break;
	case T_DERECHA: motor_busca_ms(&M, motor_pos_ms(&M) + (mayus ? 30000 : 5000)); break;
	case T_IZQUIERDA: {
		u32 p = motor_pos_ms(&M), d = mayus ? 30000 : 5000;

		motor_busca_ms(&M, p > d ? p - d : 0);
		break;
	}
	case T_ARRIBA:
	case T_ABAJO:
	case T_RE_PAG:
	case T_AV_PAG:
	case T_INICIO:
	case T_FIN:
		if (pestana == P_LISTA && L.n) {
			int n = filas_visibles(), c = lista_cursor;

			c += k == T_ARRIBA ? -1 : k == T_ABAJO ? 1 : k == T_RE_PAG ? -n : k == T_AV_PAG ? n : k == T_INICIO ? -L.n : L.n;
			if (c < 0)
				c = 0;
			if (c >= L.n)
				c = L.n - 1;
			marca_fila(c, mayus ? CON_MAYUS : 0);
			asegura_visible(c);
		} else if (pestana == P_ANADIR && exp_n) {
			int n = r_exp.al / ALTO_FILA, c = exp_cursor, i;

			c += k == T_ARRIBA ? -1 : k == T_ABAJO ? 1 : k == T_RE_PAG ? -n : k == T_AV_PAG ? n : k == T_INICIO ? -exp_n : exp_n;
			if (c < 0)
				c = 0;
			if (c >= exp_n)
				c = exp_n - 1;
			exp_cursor = c;
			for (i = 0; i < exp_n; i++)
				exp_marca[i] = i == c;
			if (c < exp_arriba)
				exp_arriba = c;
			if (n > 0 && c >= exp_arriba + n)
				exp_arriba = c - n + 1;
		}
		break;
	case '\r':
	case '\n':
		if (pestana == P_LISTA)
			tocar_cancion(lista_cursor);
		else if (pestana == P_ANADIR && exp_cursor < exp_n)
			accion(Z_X_FILA, exp_cursor, 0, 0, 2, 0);
		break;
	case 8:
		if (pestana == P_ANADIR)
			accion(Z_X_SUBIR, 0, 0, 0, 1, 0);
		break;
	case T_SUPR:
		if (pestana == P_LISTA)
			quitar_elegidas();
		break;
	case '\t':
		pestana = (pestana + 1) % 4;
		if (pestana == P_ANADIR && !exp_n)
			exp_lee();
		break;
	case T_F1:
		pestana = P_OPCIONES;
		avisa("Las teclas están en Opciones.");
		break;
	}
}

static void rueda(int x, int y, int pasos, u32 mods)
{
	int dato = 0, id = zona_en(x, y, &dato);

	if (id == Z_EQ_BANDA) {
		pon_banda(dato, valor_banda(dato) + pasos * 10);
		pinta_deslizador_eq(dato);
		return;
	}
	if (id == Z_POS) {
		u32 p = motor_pos_ms(&M);

		motor_busca_ms(&M, pasos > 0 ? p + 5000 * (u32)pasos : (p > 5000u * (u32)-pasos ? p - 5000u * (u32)-pasos : 0));
		return;
	}
	if (!mini && pestana == P_LISTA && y >= r_filas.y - 24 && y < r_filas.y + r_filas.al && !(mods & CON_CONTROL)) {
		lista_arriba -= pasos * 3;
		pinta_todo();
		return;
	}
	if (!mini && pestana == P_ANADIR && y >= r_exp.y && y < r_exp.y + r_exp.al) {
		exp_arriba -= pasos * 3;
		pinta_todo();
		return;
	}
	if (!mini && pestana == P_EQ && id == Z_EQ_PREAJUSTE) {
		eq_lista_arriba -= pasos * 2;
		pinta_todo();
		return;
	}
	/* en lo demás, el volumen */
	motor_volumen(&M, M.volumen + pasos * 5);
	cambios_ajustes++;
	pinta_volumen();
}

/* ===================================================================== la bandeja */
static u32 icono_bandeja[256];

static void haz_icono_bandeja(void)
{
	int x, y;

	/* una nota blanca sobre un círculo azul (16 x 16), con su borde */
	for (y = 0; y < 16; y++)
		for (x = 0; x < 16; x++) {
			int dx = x - 8, dy = y - 8, d = dx * dx + dy * dy;
			u32 c = 0;

			if (d <= 56)
				c = 0xFF2468C8;
			if (d > 42 && d <= 56)
				c = 0xFF14336A;
			/* la nota: la cabeza y el palo */
			if (((x - 6) * (x - 6) + (y - 11) * (y - 11) <= 5) || (x >= 8 && x <= 8 && y >= 3 && y <= 11) || (x >= 8 && x <= 11 && y >= 3 && y <= 4))
				c = 0xFFFFFFFF;
			icono_bandeja[y * 16 + x] = c;
		}
}

static void pon_bandeja(void)
{
	char ayuda[64];
	cancion *c = oida();

	if (c)
		fmt(ayuda, sizeof ayuda, "Reproductor: %s", c->titulo);
	else
		copia(ayuda, sizeof ayuda, "Reproductor");
	so_bandeja(V, icono_bandeja, ayuda, M.estado == M_SONANDO ? "Pausa|Siguiente|Anterior|Parar|-|Mostrar la ventana|Salir" :
		   "Reproducir|Siguiente|Anterior|Parar|-|Mostrar la ventana|Salir");
}

static void opcion_bandeja(u32 a)
{
	switch (a) {
	case 0: accion(Z_TOCAR, 0, 0, 0, 1, 0); break;
	case 1: motor_siguiente(&M); break;
	case 2: motor_anterior(&M); break;
	case 3: motor_para(&M); break;
	case 5: so_trae(V); break;
	case 6: salir = 1; break;
	case SIN_ELEGIR: so_trae(V); break;	/* (un clic: la ventana) */
	}
}

/* ===================================================================== el bucle */
static void sucesos(void)
{
	so_suceso s;
	int vueltas = 0;

	while (vueltas++ < 64 && so_suceso_de(V, &s)) {
		int x = (int)s.a, y = (int)s.b, dato = 0, id;

		switch (s.clase) {
		case EV_CERRAR:
			salir = 1;
			break;
		case EV_TAMANO:
			if ((int)s.a > 0 && (int)s.b > 0) {
				nuevo_lienzo((int)s.a, (int)s.b);
				if (!mini && AL < 300)
					mini = 1;	/* (tan baja que no cabe: la pequeña) */
				else if (mini && AL >= 160)
					mini = 0;
			}
			pinta_todo();
			break;
		case EV_TECLA:
			tecla(s.a, s.b | (s.c & 0x300));
			pinta_todo();
			break;
		case EV_BAJA:
			id = zona_en(x, y, &dato);
			if (s.c & B_DERECHO) {
				if (id == Z_FILA) {
					marca_fila(dato, 0);
					so_menu(V, x, y, "Tocar|Quitar de la lista|Subir|Bajar", 0, 2);
				}
				break;
			}
			if (id != Z_CAMPO && id != Z_X_CAMPO && campo_activo == 1)
				campo_activo = 0;
			if (id == Z_POS) {
				arrastre = Z_POS;
				arrastra(x, y);
				break;
			}
			if (id == Z_VOLUMEN) {
				arrastre = Z_VOLUMEN;
				arrastra(x, y);
				break;
			}
			if (id == Z_BARRA_LISTA || id == Z_X_BARRA) {
				arrastre = id;
				arrastre_dato = dato;
				arrastra(x, y);
				break;
			}
			accion(id, dato, x, y, VECES(s.c) ? (int)VECES(s.c) : 1, (int)(s.c & 0x300));
			if (arrastre == Z_EQ_BANDA)
				arrastra(x, y);
			pinta_todo();
			break;
		case EV_MUEVE:
			if (arrastre)
				arrastra(x, y);
			else if (fila_arrastrada >= 0 && (s.c & B_IZQUIERDO) && pestana == P_LISTA && !mini) {
				int d = (y - r_filas.y + ALTO_FILA / 2) / ALTO_FILA + lista_arriba;

				if (d < 0)
					d = 0;
				if (d > L.n)
					d = L.n;
				if (d != fila_destino) {
					fila_destino = d;
					pinta_todo();
				}
			}
			break;
		case EV_SUBE:
			if (arrastre)
				suelta_arrastre(x, y);
			if (fila_arrastrada >= 0 && fila_destino >= 0 && fila_destino != fila_arrastrada && fila_destino != fila_arrastrada + 1) {
				int a = fila_destino > fila_arrastrada ? fila_destino - 1 : fila_destino;

				lista_mueve(&L, fila_arrastrada, a);
				if (M.oida == fila_arrastrada)
					M.oida = a;
				else if (fila_arrastrada < a && M.oida > fila_arrastrada && M.oida <= a)
					M.oida--;
				else if (fila_arrastrada > a && M.oida >= a && M.oida < fila_arrastrada)
					M.oida++;
				lista_cursor = a;
			}
			fila_arrastrada = fila_destino = -1;
			pinta_todo();
			break;
		case EV_RUEDA: {
			int p = (int)(s.c & 0xFF);

			if (p >= 128)
				p -= 256;
			rueda(x, y, p, s.c);
			pinta_todo();
			break;
		}
		case EV_MENU:
			if (s.b == 1 && s.a != SIN_ELEGIR)
				lista_ordena(&L, (int)s.a);
			else if (s.b == 2 && s.a != SIN_ELEGIR) {
				if (s.a == 0)
					tocar_cancion(lista_cursor);
				else if (s.a == 1)
					quitar_elegidas();
				else if (s.a == 2 && lista_cursor > 0) {
					lista_mueve(&L, lista_cursor, lista_cursor - 1);
					lista_cursor--;
				} else if (s.a == 3 && lista_cursor + 1 < L.n) {
					lista_mueve(&L, lista_cursor, lista_cursor + 1);
					lista_cursor++;
				}
			}
			pinta_todo();
			break;
		case EV_SOLTADO: {
			char r[MAX_RUTA];

			if (so_soltado(r, sizeof r)) {
				char t[MAX_RUTA + 40], u[MAX_RUTA];
				int antes = L.n;

				anade_argumento(&L, r);
				so_a_utf8(r, u, sizeof u);
				fmt(t, sizeof t, "Soltado en la lista: %s", u);
				avisa(t);
				if (M.estado == M_PARADO) {
					while (L.n == antes && lista_paso(&L))
						;
					if (L.n > antes)
						tocar_cancion(antes);
				}
			}
			pinta_todo();
			break;
		}
		case EV_BANDEJA:
			opcion_bandeja(s.a);
			pinta_todo();
			break;
		case EV_UNIDADES:
			if (pestana == P_ANADIR)
				pinta_todo();
			break;
		}
	}
}

int vista_principal(const char *args)
{
	const char *p;
	char a[MAX_RUTA];
	u32 t_pos = 0, t_vis = 0, t_buzon = 0, t_mini = 0, t_bandeja = 0;
	int estado_antes = -1, cambio_antes = -1, cambios_lista = -1, i;
	int hay_args = 0;

	so_carpeta_propia(carpeta_propia, sizeof carpeta_propia);
	if (so_otro_en_marcha(args))
		return 0;	/* (el que ya estaba lo toca) */
	lista_nueva(&L);
	if (motor_prepara(&M, &L))
		return 1;
	lee_ajustes();
	if (AN < 480 || AN > 1600 || AL < 200 || AL > 1200) {
		AN = 760;
		AL = 560;
	}
	if (mini) {
		AN = MINI_AN;
		AL = MINI_AL;
	}
	V = so_ventana("Reproductor", AN, AL);
	if (!V)
		return 1;
	LZ.an = LZ.al = 0;
	LZ.p = NULL;
	{
		int an = AN, al = AL;

		AN = AL = 0;
		nuevo_lienzo(an, al);
	}
	haz_icono_bandeja();
	/* lo que se ha pedido (archivos, carpetas, listas, un CD); si no, la de la última vez */
	for (p = args;;) {
		int k = 0;

		while (*p == ' ')
			p++;
		if (*p == '"') {
			p++;
			while (*p && *p != '"' && k < MAX_RUTA - 1)
				a[k++] = *p++;
			if (*p)
				p++;
		} else
			while (*p && *p != ' ' && k < MAX_RUTA - 1)
				a[k++] = *p++;
		a[k] = 0;
		if (!a[0])
			break;
		if (es_opcion(a))
			continue;
		anade_argumento(&L, a);
		hay_args = 1;
	}
	if (!hay_args) {
		char r[MAX_RUTA];

		ruta_propia(r, sizeof r, "LISTA.M3U");
		if (so_existe(r, NULL))
			lista_lee(&L, r);
	}
	while (L.n_pendientes && L.n < 1 && lista_paso(&L))
		;
	pinta_todo();
	vuelca();
	if (hay_args && L.n)
		tocar_cancion(0);
	else if (cancion_guardada >= 0 && cancion_guardada < L.n) {
		lista_cursor = cancion_guardada;
		L.actual = cancion_guardada;
		M.oida = cancion_guardada;
		asegura_visible(cancion_guardada);
	}
	pon_bandeja();
	reg("ventana %ux%u; %d canciones; la tarjeta a %u Hz%s", AN, AL, L.n, M.hz, M.sin_tarjeta ? " (no hay)" : "");
	while (!salir) {
		u32 ahora = so_ms();

		sucesos();
		/* el motor: lo primero (que no se corte) */
		motor_ciclo(&M, 30);
		/* las carpetas que se están añadiendo, de a poco (con la cola llena) */
		if (L.n_pendientes && motor_cola_ms(&M) > 150)
			lista_paso(&L);
		/* las duraciones de la lista, de una en una (con la cola llena) */
		if (motor_cola_ms(&M) > 180 || M.estado != M_SONANDO) {
			static int k;

			for (i = 0; i < 4 && L.n; i++) {
				cancion *c;

				k = (k + 1) % L.n;
				c = &L.c[k];
				if (!c->duracion && !c->leida && !c->fallo && !c->o.lector) {
					etiquetas e;

					if (!c->o.largo && etiquetas_lee(c->o.ruta, &e, 0) == 0) {
						if (e.titulo[0])
							copia(c->titulo, sizeof c->titulo, e.titulo);
						if (e.artista[0])
							copia(c->artista, sizeof c->artista, e.artista);
						copia(c->album, sizeof c->album, e.album);
					}
					c->duracion = duracion_rapida(&c->o);
					c->leida = 1;
					L.cambios++;
					break;
				}
			}
		}
		/* lo que se oye ha cambiado: sus etiquetas y su carátula */
		if (M.cambio != cambio_antes || M.estado != estado_antes) {
			if (M.oida != et_de && motor_cola_ms(&M) > 120)
				mira_oida();
			if (M.oida == et_de) {
				cambio_antes = M.cambio;
				estado_antes = M.estado;
				eq_por_genero();
				pinta_todo();
				pon_bandeja();
			}
		}
		if (L.cambios != cambios_lista) {
			cambios_lista = L.cambios;
			pinta_todo();
		}
		/* el tiempo y la barra, cuatro veces por segundo */
		if (ahora - t_pos >= 250) {
			t_pos = ahora;
			if (getenv_depura())
				reg("pos=%u oida=%d et_de=%d estado=%d cola=%d salida=%u marcas=%d", motor_pos_ms(&M), M.oida, et_de, M.estado, motor_cola_ms(&M), (u32)M.salida, M.n_marcas);
			if (mini)
				pinta_mini_titulo();
			else {
				pinta_posicion();
				pinta_estado();
			}
		}
		if (mini && ahora - t_mini >= 120) {
			t_mini = ahora;
			desplaza_titulo += 3;
			pinta_mini_titulo();
		}
		/* la visualización, a 20 cuadros por segundo (si suena y se ve) */
		if (!mini && vis_modo && M.estado == M_SONANDO && ahora - t_vis >= 50) {
			t_vis = ahora;
			pinta_vis();
		}
		/* lo que pide otro (dos clics en otra canción desde Mi equipo) */
		if (ahora - t_buzon >= 500) {
			char pedido[1100];

			t_buzon = ahora;
			if (so_pedido_nuevo(pedido, sizeof pedido)) {
				int antes = L.n;
				const char *q = pedido;

				for (;;) {
					int k = 0;

					while (*q == ' ')
						q++;
					if (*q == '"') {
						q++;
						while (*q && *q != '"' && k < MAX_RUTA - 1)
							a[k++] = *q++;
						if (*q)
							q++;
					} else
						while (*q && *q != ' ' && k < MAX_RUTA - 1)
							a[k++] = *q++;
					a[k] = 0;
					if (!a[0])
						break;
					if (!es_opcion(a))
						anade_argumento(&L, a);
				}
				while (L.n == antes && lista_paso(&L))
					;
				if (L.n > antes)
					tocar_cancion(antes);
				so_trae(V);
			}
		}
		/* lo guardado, a los dos segundos de un cambio */
		if (cambios_ajustes && !guardar_en)
			guardar_en = ahora + 2000;
		if (guardar_en && (int)(ahora - guardar_en) >= 0 && motor_cola_ms(&M) > 150) {
			guarda_ajustes();
			guardar_en = 0;
		}
		if (ahora - t_bandeja >= 5000) {
			t_bandeja = ahora;
			pon_bandeja();
		}
		vuelca();
		/* a dormir lo que se pueda (la cola aguanta) */
		so_duerme(M.estado == M_SONANDO && motor_cola_ms(&M) < 150 ? 1 : 10);
	}
	guarda_ajustes();
	so_bandeja(V, NULL, NULL, NULL);
	motor_cierra(&M);
	return 0;
}
