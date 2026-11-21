/* ventana.c - (SONIDO) las ventanas de la orden SONIDO:
 *   SONIDO /VENTANA  Configuración > Sonido: la tarjeta, el códec y la
 *                    salida (altavoces o auriculares), el volumen general
 *                    (con el ratón, arrastrando o con la rueda, y con las
 *                    flechas), el silencio, probar el altavoz izquierdo y el
 *                    derecho, los sonidos del sistema, y el volumen de cada
 *                    programa que suena;
 *   SONIDO /BANDEJA  la ventanita del icono del altavoz de la bandeja: el
 *                    volumen, el silencio y «Configuración de sonido...»;
 *                    se va al pinchar fuera.
 * Lo que se cambia va al núcleo (la llamada 193): la bandeja lo ve al
 * momento y el servicio lo guarda en \HISPA\SONIDO.INI.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "sonido.h"

#define EV_RATON_SUBE 3
#define EV_RATON_MUEVE 4
#define EV_RUEDA 13
#define TINTA 0x14294A
#define SUAVE 0x5A7396
#define BLANCO 0xFFFFFF
#define AZUL1 0x5AA6F4
#define AZUL2 0x2468C8

struct orden { u32 op; int x, y, ancho, alto; u32 tinta, tinta2; const void *datos; int n; };
static u32 vn;

static void dib(u32 op, int x, int y, int an, int al, u32 t1, u32 t2, const void *d, int n)
{
	struct orden o = { op, x, y, an, al, t1, t2, d, n };

	sc(SC_DIBUJAR, vn, (u32)&o, 0);
}
static void rect(int x, int y, int an, int al, u32 c) { dib(1, x, y, an, al, c, 0, 0, 0); }
static void degr(int x, int y, int an, int al, u32 a, u32 b) { dib(2, x, y, an, al, a, b, 0, 0); }
static void borde(int x, int y, int an, int al, u32 c) { dib(3, x, y, an, al, c, 0, 0, 0); }
static void redondo(int x, int y, int an, int al, int r, u32 a, u32 b) { dib(9, x, y, an, al, a, b, 0, r); }
static void texto(int x, int y, const char *t, u32 c) { dib(5, x, y, 0, 0, c, 0, t, strlen(t)); }
static void negrita(int x, int y, const char *t, u32 c) { dib(10, x, y, 0, 0, c, 0, t, strlen(t)); }

/* ---- los controles: dónde está cada uno, para saber qué se ha pulsado ---- */
enum { Z_NADA, Z_VOLUMEN, Z_SILENCIO, Z_IZQ, Z_DER, Z_SISTEMA, Z_CERRAR, Z_CONFIG, Z_CANAL };
#define MAX_ZONAS 24
static struct { int x, y, an, al, que, dato; } zonas[MAX_ZONAS];
static int n_zonas;

static void zona(int x, int y, int an, int al, int que, int dato)
{
	if (n_zonas < MAX_ZONAS) {
		zonas[n_zonas].x = x, zonas[n_zonas].y = y, zonas[n_zonas].an = an, zonas[n_zonas].al = al;
		zonas[n_zonas].que = que, zonas[n_zonas].dato = dato;
		n_zonas++;
	}
}

static int zona_en(int x, int y, int *dato)
{
	int i;

	for (i = 0; i < n_zonas; i++)
		if (x >= zonas[i].x && x < zonas[i].x + zonas[i].an && y >= zonas[i].y && y < zonas[i].y + zonas[i].al) {
			*dato = zonas[i].dato;
			return i;
		}
	return -1;
}

static void boton(int x, int y, int an, int al, const char *t, int que)
{
	int tx = x + (an - (int)strlen(t) * 8) / 2;

	redondo(x, y, an, al, 6, 0x184A90, 0x184A90);
	redondo(x + 1, y + 1, an - 2, al - 2, 5, AZUL1, AZUL2);
	negrita(tx + 1, y + (al - 16) / 2 + 1, t, 0x10305C);
	negrita(tx, y + (al - 16) / 2, t, BLANCO);
	zona(x, y, an, al, que, 0);
}

static void casilla(int x, int y, const char *t, int marcada, int que)
{
	rect(x, y + 1, 14, 14, 0x3A6EA8);
	rect(x + 1, y + 2, 12, 12, BLANCO);
	if (marcada) {
		rect(x + 3, y + 7, 2, 4, 0x1C7A30);
		rect(x + 5, y + 9, 2, 3, 0x1C7A30);
		rect(x + 7, y + 6, 2, 4, 0x1C7A30);
		rect(x + 9, y + 4, 2, 3, 0x1C7A30);
	}
	texto(x + 22, y, t, TINTA);
	zona(x, y, 22 + strlen(t) * 8, 16, que, 0);
}

/* el deslizador: «an» puntos para el 0..100 */
static void deslizador(int x, int y, int an, u32 v, int apagado, int que, int dato)
{
	int p = x + (int)v * (an - 1) / 100;
	char t[8];

	rect(x, y + 6, an, 6, 0x7E97BA);
	rect(x + 1, y + 7, an - 2, 4, 0xE8EEF6);
	if (p > x + 1)
		rect(x + 1, y + 7, p - x - 1, 4, apagado ? 0xA9B6C8 : 0x3D8FE8);
	redondo(p - 5, y, 11, 18, 4, apagado ? 0xC8D0DC : 0x6DB4F8, apagado ? 0x8A98AE : 0x2468C8);
	formatear(t, sizeof t, "%u %%", v);
	rect(x + an + 8, y + 1, 48, 16, 0xE6EEF9);
	texto(x + an + 10, y + 1, t, TINTA);
	zona(x - 6, y - 2, an + 12, 22, que, dato);
}

static u32 valor_en(int x, int x0, int an)
{
	int v = (x - x0) * 100 / (an - 1);

	return v < 0 ? 0 : v > 100 ? 100 : (u32)v;
}

/* un altavoz (32 x 32), con sus ondas o con la cruz del silencio */
static void altavoz(int x, int y, int mudo, u32 vol)
{
	int i;

	rect(x, y + 11, 8, 10, 0x2E5C94);
	for (i = 0; i < 10; i++)
		rect(x + 8 + i, y + 11 - i, 1, 10 + 2 * i, 0x2E5C94);
	if (mudo) {
		for (i = 0; i < 9; i++) {
			rect(x + 22 + i, y + 11 + i, 2, 2, 0xD8362E);
			rect(x + 30 - i, y + 11 + i, 2, 2, 0xD8362E);
		}
		return;
	}
	for (i = 0; i < 3 && vol > (u32)i * 33; i++) {
		int r = 4 + i * 4;

		rect(x + 20 + r, y + 16 - r, 2, 2 * r, 0x3D8FE8);
	}
}

/* ---- Configuración > Sonido ---- */
#define AN 460
#define AL 400
static struct estado_sonido e;
static struct info_canal can[MAX_CANALES];
static int hay;

static void pinta_ajustes(void)
{
	char t[100];
	int y, i, n = 0;

	n_zonas = 0;
	degr(0, 0, AN, AL, 0xF4F8FE, 0xD6E6FA);
	altavoz(16, 14, e.silencio, e.volumen);
	negrita(70, 14, "Sonido", TINTA);
	texto(70, 34, hay ? "La tarjeta, el volumen y los sonidos" : "Sin tarjeta de sonido", SUAVE);
	rect(14, 62, AN - 28, 1, 0xA9C6EC);
	if (!hay) {
		texto(20, 80, "No hay tarjeta de sonido que Hispa-Pentium", TINTA);
		texto(20, 98, "sepa llevar (HD Audio, AC'97 o Sound Blaster 16).", TINTA);
		boton(AN - 104, AL - 42, 90, 28, "Cerrar", Z_CERRAR);
		return;
	}
	texto(20, 74, "Tarjeta:", SUAVE);
	texto(110, 74, e.nombre, TINTA);
	texto(20, 94, "Códec:", SUAVE);
	texto(110, 94, e.codec, TINTA);
	texto(20, 114, "Salida:", SUAVE);
	formatear(t, sizeof t, "%s%s", e.salida == SALIDA_AURICULARES ? "Auriculares" : e.salida == SALIDA_LINEA ? "Línea" : "Altavoces",
		  e.detecta ? (e.auriculares ? " (auriculares enchufados)" : " (sin auriculares)") : "");
	texto(110, 114, t, TINTA);
	if (e.detalle[0])
		texto(110, 132, e.detalle, SUAVE);
	negrita(20, 160, "Volumen:", TINTA);
	deslizador(110, 159, 260, e.volumen, e.silencio, Z_VOLUMEN, 0);
	casilla(110, 186, "Silencio", e.silencio, Z_SILENCIO);
	texto(20, 220, "Probar:", SUAVE);
	boton(110, 214, 150, 28, "Izquierdo", Z_IZQ);
	boton(270, 214, 150, 28, "Derecho", Z_DER);
	casilla(20, 256, "Sonidos del sistema (inicio, apagar, avisos)", e.sistema, Z_SISTEMA);
	negrita(20, 288, "Programas que suenan:", TINTA);
	y = 310;
	for (i = 0; i < MAX_CANALES && n < 2; i++)
		if (can[i].dueno) {
			char nom[17];

			memcpy(nom, can[i].nombre, 16);
			nom[16] = 0;
			if (strlen(nom) > 12)
				nom[12] = 0;
			texto(20, y, nom, TINTA);
			deslizador(130, y - 1, 240, can[i].volumen, e.silencio, Z_CANAL, i + 1);
			y += 26;
			n++;
		}
	if (!n)
		texto(20, y, "(ninguno)", SUAVE);
	boton(AN - 104, AL - 40, 90, 28, "Cerrar", Z_CERRAR);
}

static void lee(void)
{
	hay = sc(SC_SONIDO, G_ESTADO, (u32)&e, 0) == 1;
	if (sc(SC_SONIDO, G_CANALES, (u32)can, 0) != MAX_CANALES)
		memset(can, 0, sizeof can);
}

static int distinto(struct estado_sonido *a, struct info_canal *c)
{
	return a->cambios != e.cambios || a->volumen != e.volumen || a->silencio != e.silencio || a->salida != e.salida ||
	       a->auriculares != e.auriculares || a->sistema != e.sistema || memcmp(c, can, sizeof can);
}

int ventana_ajustes(void)
{
	struct suceso s;
	int arrastra = 0, dato = 0, k;
	u32 ult = h_tics();

	if (!h_hay_escritorio()) {
		h_escribe("SONIDO /VENTANA: solo con el escritorio.\n");
		return 1;
	}
	{
		/* (si no hay servicio, se pone en marcha: abrir un canal lo hace) */
		struct estado_sonido x;

		if (sc(SC_SONIDO, G_ESTADO, (u32)&x, 0) != 1) {
			u32 c = sc(SC_ABRIR_CANAL, 48000, 2 | 16 << 8, 0);

			if (!ES_ERROR(c))
				sc(SC_CANAL, c, C_CERRAR, 0);
		}
	}
	vn = h_ventana("Sonido", AN, AL);
	if (ES_ERROR(vn))
		return 1;
	sc(80, vn, 0xFFFFFFFF, 0);		/* en el centro (como un cuadro de Configuración) */
	lee();
	pinta_ajustes();
	for (;;) {
		if (!h_suceso(vn, &s, 0)) {
			if (h_tics() - ult > h_hz / 2) {
				struct estado_sonido a = e;
				struct info_canal c[MAX_CANALES];

				memcpy(c, can, sizeof can);
				ult = h_tics();
				lee();
				if (distinto(&a, c))
					pinta_ajustes();
			}
			h_dormir(2);
			continue;
		}
		if (s.clase == EV_CERRAR)
			break;
		if (s.clase == EV_TECLA) {
			if (s.a == 27)
				break;
			if (s.a == T_DERECHA || s.a == '+' || s.a == T_IZQUIERDA || s.a == '-') {
				sc(SC_SONIDO, G_SUBIR, 0, (s.a == T_DERECHA || s.a == '+') ? 5 : (u32)-5);
				lee();
				pinta_ajustes();
			}
			continue;
		}
		if (s.clase == EV_RUEDA) {
			int pasos = (i32)(signed char)(s.c & 0xFF);

			k = zona_en(s.a, s.b, &dato);
			if (k >= 0 && zonas[k].que == Z_CANAL) {
				int v = (int)can[dato - 1].volumen + pasos * 5;

				sc(SC_SONIDO, G_VOLUMEN_CANAL, dato, v < 0 ? 0 : v > 100 ? 100 : v);
			} else
				sc(SC_SONIDO, G_SUBIR, 0, (u32)(pasos * 5));
			lee();
			pinta_ajustes();
			continue;
		}
		if (s.clase == EV_RATON_SUBE) {
			if (arrastra)
				sc(SC_SONIDO, G_SISTEMA, 0, SS_VOLUMEN);
			arrastra = 0;
			continue;
		}
		if (s.clase == EV_RATON_MUEVE && arrastra) {
			if (arrastra == Z_VOLUMEN)
				sc(SC_SONIDO, G_VOLUMEN, 0, valor_en(s.a, 110, 260));
			else
				sc(SC_SONIDO, G_VOLUMEN_CANAL, dato, valor_en(s.a, 130, 240));
			lee();
			pinta_ajustes();
			continue;
		}
		if (s.clase != EV_RATON_BAJA)
			continue;
		k = zona_en(s.a, s.b, &dato);
		if (k < 0)
			continue;
		switch (zonas[k].que) {
		case Z_VOLUMEN:
			sc(SC_SONIDO, G_VOLUMEN, 0, valor_en(s.a, 110, 260));
			arrastra = Z_VOLUMEN;
			break;
		case Z_CANAL:
			sc(SC_SONIDO, G_VOLUMEN_CANAL, dato, valor_en(s.a, 130, 240));
			arrastra = Z_CANAL;
			break;
		case Z_SILENCIO:
			sc(SC_SONIDO, G_SILENCIO, 0, 2);
			break;
		case Z_SISTEMA:
			sc(SC_SONIDO, G_SONIDOS, 0, e.sistema ? 0 : 1);
			break;
		case Z_IZQ:
			sc(SC_SONIDO, G_PROBAR, 0, 1);
			break;
		case Z_DER:
			sc(SC_SONIDO, G_PROBAR, 0, 2);
			break;
		case Z_CERRAR:
			sc(SC_CERRAR_VENTANA, vn, 0, 0);
			return 0;
		}
		lee();
		pinta_ajustes();
	}
	sc(SC_CERRAR_VENTANA, vn, 0, 0);
	return 0;
}

/* ---- la ventanita de la bandeja ---- */
#define BAN 240
#define BAL 104

static void pinta_bandeja(void)
{
	n_zonas = 0;
	degr(0, 0, BAN, BAL, 0xF4F8FE, 0xD6E6FA);
	borde(0, 0, BAN, BAL, 0x7E97BA);
	altavoz(10, 6, e.silencio, e.volumen);
	negrita(52, 14, "Volumen", TINTA);
	deslizador(14, 42, 150, e.volumen, e.silencio, Z_VOLUMEN, 0);
	casilla(14, 70, "Silencio", e.silencio, Z_SILENCIO);
	texto(126, 70, "Ajustes...", 0x1F5FB8);
	rect(126, 86, 80, 1, 0x1F5FB8);
	zona(126, 68, 84, 20, Z_CONFIG, 0);
}

int ventana_bandeja(void)
{
	struct suceso s;
	int arrastra = 0, dato, k, foco = 0;
	u32 ult = h_tics(), empezo = h_tics();
	int an = sc(SC_DATO, 11, 0, 0), al = sc(SC_DATO, 14, 0, 0);

	if (!h_hay_escritorio())
		return 1;
	vn = h_ventana("Volumen", BAN, BAL);
	if (ES_ERROR(vn))
		return 1;
	if (an > 0 && al > 0 && !ES_ERROR((u32)an) && !ES_ERROR((u32)al))
		sc(80, vn, an - BAN - 16, al - BAL - 40);
	lee();
	pinta_bandeja();
	for (;;) {
		if (!h_suceso(vn, &s, 0)) {
			if (h_tics() - ult > h_hz / 2) {
				struct estado_sonido a = e;

				ult = h_tics();
				lee();
				if (a.volumen != e.volumen || a.silencio != e.silencio)
					pinta_bandeja();
			}
			h_dormir(2);
			continue;
		}
		if (s.clase == EV_CERRAR || (s.clase == EV_TECLA && s.a == 27))
			break;
		if (s.clase == EV_FOCO) {
			if (s.a)
				foco = 1;
			else if (foco || h_tics() - empezo > h_hz)
				break;		/* (se ha pinchado fuera) */
			continue;
		}
		if (s.clase == EV_TECLA && (s.a == T_DERECHA || s.a == T_ARRIBA || s.a == T_IZQUIERDA || s.a == T_ABAJO)) {
			sc(SC_SONIDO, G_SUBIR, 0, (s.a == T_DERECHA || s.a == T_ARRIBA) ? 5 : (u32)-5);
			lee();
			pinta_bandeja();
			continue;
		}
		if (s.clase == EV_RUEDA) {
			sc(SC_SONIDO, G_SUBIR, 0, (u32)((i32)(signed char)(s.c & 0xFF) * 5));
			lee();
			pinta_bandeja();
			continue;
		}
		if (s.clase == EV_RATON_SUBE) {
			if (arrastra)
				sc(SC_SONIDO, G_SISTEMA, 0, SS_VOLUMEN);
			arrastra = 0;
			continue;
		}
		if (s.clase == EV_RATON_MUEVE && arrastra) {
			sc(SC_SONIDO, G_VOLUMEN, 0, valor_en(s.a, 14, 150));
			lee();
			pinta_bandeja();
			continue;
		}
		if (s.clase != EV_RATON_BAJA)
			continue;
		k = zona_en(s.a, s.b, &dato);
		if (k < 0)
			continue;
		if (zonas[k].que == Z_VOLUMEN) {
			sc(SC_SONIDO, G_VOLUMEN, 0, valor_en(s.a, 14, 150));
			arrastra = 1;
		} else if (zonas[k].que == Z_SILENCIO)
			sc(SC_SONIDO, G_SILENCIO, 0, 2);
		else if (zonas[k].que == Z_CONFIG) {
			char r[48];

			h_ruta(r, sizeof r, "SONIDO.BIN");
			if (ES_ERROR(sc(SC_LANZAR_SUELTO, (u32)r, (u32)"/VENTANA", 0))) {
				formatear(r, sizeof r, "%c:\\SONIDO.BIN", h_sistema);
				sc(SC_LANZAR_SUELTO, (u32)r, (u32)"/VENTANA", 0);
			}
			break;
		}
		lee();
		pinta_bandeja();
	}
	sc(SC_CERRAR_VENTANA, vn, 0, 0);
	return 0;
}
