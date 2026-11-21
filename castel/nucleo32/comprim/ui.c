/* ui.c - lo poco de ventana que necesitan Comprimidos y el Mirador (en C,
 * sobre las llamadas 32 a 39): dibujar, los botones (con los colores y la
 * forma de controles.cas, para que se vean como los demás programas), un
 * campo para escribir y los cuadros que esperan una respuesta (un texto, sí
 * o no, un aviso), que se pintan encima de lo de la ventana y tienen su
 * propio bucle de sucesos.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "pentium.h"
#include "ui.h"

uint32_t ui_v;
int ui_an, ui_al;
static struct orden o;

static void dibuja(uint32_t op, int x, int y, int an, int al, uint32_t t1, uint32_t t2, const void *d, int n)
{
	o.op = op;
	o.x = x;
	o.y = y;
	o.ancho = an;
	o.alto = al;
	o.tinta = t1;
	o.tinta2 = t2;
	o.datos = d;
	o.n = n;
	sc(SC_DIBUJAR, ui_v, (uint32_t)&o, 0);
}

void ui_rect(int x, int y, int an, int al, uint32_t c) { if (an > 0 && al > 0) dibuja(OP_RECTANGULO, x, y, an, al, c, 0, 0, 0); }
void ui_degradado(int x, int y, int an, int al, uint32_t a, uint32_t b) { dibuja(OP_DEGRADADO, x, y, an, al, a, b, 0, 0); }
void ui_redondo(int x, int y, int an, int al, int r, uint32_t a, uint32_t b) { dibuja(OP_REDONDEADO, x, y, an, al, a, b, 0, r); }
void ui_marco(int x, int y, int an, int al, uint32_t c) { dibuja(OP_MARCO, x, y, an, al, c, 0, 0, 0); }
void ui_linea(int x1, int y1, int x2, int y2, uint32_t c) { dibuja(OP_LINEA, x1, y1, x2, y2, c, 0, 0, 0); }
void ui_mapa(int x, int y, int an, int al, const uint32_t *p) { if (an > 0 && al > 0) dibuja(OP_MAPA, x, y, an, al, 0, 0, p, 0); }

/* hasta «max» letras (si no caben, con «...» al final) */
static int corta(const char *t, int max, char *d)
{
	int n = (int)strlen(t);

	if (max < 1)
		return 0;
	if (n <= max) {
		memcpy(d, t, (size_t)n);
		return n;
	}
	if (max <= 3) {
		memcpy(d, t, (size_t)max);
		return max;
	}
	memcpy(d, t, (size_t)(max - 3));
	memcpy(d + max - 3, "...", 3);
	return max;
}

void ui_texto(int x, int y, const char *t, int max, uint32_t c)
{
	char d[256];
	int n = corta(t, max > 255 ? 255 : max, d);

	if (n)
		dibuja(OP_TEXTO, x, y, 0, 0, c, 0, d, n);
}

void ui_negrita(int x, int y, const char *t, int max, uint32_t c)
{
	char d[256];
	int n = corta(t, max > 255 ? 255 : max, d);

	if (n)
		dibuja(OP_NEGRITA, x, y, 0, 0, c, 0, d, n);
}

/* ---- los botones: se apuntan al pintarlos, y un clic dice cuál ---- */
#define MAX_ZONAS 48
static struct { int x, y, an, al, id; } zonas[MAX_ZONAS];
static int n_zonas;

void ui_olvida_zonas(void) { n_zonas = 0; }

void ui_zona(int id, int x, int y, int an, int al)
{
	if (n_zonas < MAX_ZONAS) {
		zonas[n_zonas].x = x;
		zonas[n_zonas].y = y;
		zonas[n_zonas].an = an;
		zonas[n_zonas].al = al;
		zonas[n_zonas].id = id;
		n_zonas++;
	}
}

int ui_en_zona(int x, int y)
{
	int i;

	for (i = n_zonas; i-- > 0;)
		if (x >= zonas[i].x && y >= zonas[i].y && x < zonas[i].x + zonas[i].an && y < zonas[i].y + zonas[i].al)
			return zonas[i].id;
	return 0;
}

int ui_ancho_boton(const char *t) { return (int)strlen(t) * ANCHO_LETRA + 24; }

int ui_boton(int id, int x, int y, int an, const char *t, int principal, int apagado)
{
	uint32_t fondo = 0xE5E7EB, tinta = C_TINTA;

	if (an <= 0)
		an = ui_ancho_boton(t);
	if (principal) {
		fondo = C_VERDE;
		tinta = C_PAPEL;
	}
	if (apagado) {
		fondo = 0xF3F4F6;
		tinta = 0x6B7280;
	}
	ui_redondo(x, y, an, ALTO_BOTON, 4, C_BORDE, C_BORDE);
	ui_redondo(x + 1, y + 1, an - 2, ALTO_BOTON - 2, 4, fondo, fondo);
	ui_negrita(x + (an - (int)strlen(t) * ANCHO_LETRA) / 2, y + 5, t, 80, tinta);
	if (!apagado)
		ui_zona(id, x, y, an, ALTO_BOTON);
	return an;
}

void ui_titulo(const char *t) { sc(SC_TITULO, ui_v, (uint32_t)t, 0); }

int ui_ventana(const char *titulo, int an, int al)
{
	ui_v = sc(SC_VENTANA, (uint32_t)titulo, (uint32_t)an, (uint32_t)al);
	if (ES_ERROR(ui_v) || ui_v == 0)
		return -1;
	ui_an = an;
	ui_al = al;
	return 0;
}

int ui_suceso(struct suceso *s, int espera)
{
	return sc(SC_SUCESO, ui_v, (uint32_t)s, espera ? 1 : 0) == 1;
}

void ui_menu(int x, int y, const char *opciones, uint32_t grises, uint32_t etiqueta)
{
	static struct pide_menu p;

	p.x = x;
	p.y = y;
	p.opciones = opciones;
	p.inactivas = grises;
	p.etiqueta = etiqueta;
	sc(SC_MENU, ui_v, (uint32_t)&p, 0);
}

/* ---- los cuadros ---- */
static void (*repinta)(void);
void ui_al_repintar(void (*f)(void)) { repinta = f; }

static void caja(int x, int y, int an, int al, const char *titulo)
{
	ui_rect(x + 4, y + 4, an, al, 0x9CA3AF);
	ui_rect(x, y, an, al, C_BORDE);
	ui_rect(x + 1, y + 1, an - 2, al - 2, C_FONDO);
	ui_degradado(x + 1, y + 1, an - 2, 26, 0x2563EB, 0x1D4ED8);
	ui_negrita(x + 10, y + 6, titulo, (an - 20) / ANCHO_LETRA, C_PAPEL);
}

/* parte un texto en renglones de «max» letras (por los espacios) y los pinta */
static int renglones(int x, int y, const char *t, int max, uint32_t c, int pinta)
{
	char r[256];
	int n = 0;

	while (*t) {
		int k = 0, ult = -1;

		while (t[k] && t[k] != '\n' && k < max) {
			if (t[k] == ' ')
				ult = k;
			k++;
		}
		if (t[k] && t[k] != '\n' && ult > 0)
			k = ult;
		memcpy(r, t, (size_t)k);
		r[k] = 0;
		if (pinta)
			ui_texto(x, y + n * 18, r, max, c);
		n++;
		t += k;
		if (*t == ' ' || *t == '\n')
			t++;
	}
	return n;
}

/* espera a que se elija un botón: da su número (desde 0) o -1 (Esc o cerrar) */
int ui_pregunta(const char *titulo, const char *texto, const char *b1, const char *b2, const char *b3)
{
	const char *b[3] = { b1, b2, b3 };
	int an = 460, al, x, y, n, i, elegido = 0, bx;
	struct suceso s;

	for (;;) {
		n = renglones(0, 0, texto, (an - 40) / ANCHO_LETRA, 0, 0);
		al = 26 + 20 + n * 18 + 16 + ALTO_BOTON + 16;
		x = (ui_an - an) / 2;
		y = (ui_al - al) / 2;
		if (y < 0)
			y = 0;
		if (repinta)
			repinta();
		ui_olvida_zonas();
		caja(x, y, an, al, titulo);
		renglones(x + 20, y + 26 + 16, texto, (an - 40) / ANCHO_LETRA, C_TINTA, 1);
		bx = x + an - 16;
		for (i = 2; i >= 0; i--) {
			if (!b[i])
				continue;
			bx -= ui_ancho_boton(b[i]) + 8;
			ui_boton(100 + i, bx, y + al - ALTO_BOTON - 14, 0, b[i], i == elegido, 0);
		}
		if (!ui_suceso(&s, 1))
			return -1;
		if (s.clase == EV_CERRAR)
			return -1;
		if (s.clase == EV_TECLA) {
			if (s.a == T_ESC)
				return -1;
			if (s.a == T_INTRO || s.a == ' ')
				return elegido;
			if (s.a == T_DERECHA || s.a == T_TAB)
				elegido = (elegido + 1) % 3;
			if (s.a == T_IZQUIERDA || s.a == T_MAYUS_TAB)
				elegido = (elegido + 2) % 3;
			while (!b[elegido])
				elegido = (elegido + 1) % 3;
		}
		if (s.clase == EV_RATON_BAJA) {
			i = ui_en_zona((int)s.a, (int)s.b);
			if (i >= 100 && i < 103)
				return i - 100;
		}
		if (s.clase == EV_TAMANO) {
			ui_an = (int)s.a;
			ui_al = (int)s.b;
		}
	}
}

void ui_aviso(const char *titulo, const char *texto)
{
	ui_pregunta(titulo, texto, "Aceptar", NULL, NULL);
}

/* pide un texto (de la página 437; con «oculto», con asteriscos): 1 si se
 * acepta, 0 si no */
int ui_pide_texto(const char *titulo, const char *pregunta, char *t, int cabe, int oculto)
{
	int an = 500, al, x, y, n, cur = (int)strlen(t), vis, desde, i, k;
	struct suceso s;
	char ver[256];

	for (;;) {
		n = renglones(0, 0, pregunta, (an - 40) / ANCHO_LETRA, 0, 0);
		al = 26 + 16 + n * 18 + 8 + 26 + 16 + ALTO_BOTON + 14;
		x = (ui_an - an) / 2;
		y = (ui_al - al) / 2;
		if (y < 0)
			y = 0;
		if (repinta)
			repinta();
		ui_olvida_zonas();
		caja(x, y, an, al, titulo);
		renglones(x + 20, y + 26 + 12, pregunta, (an - 40) / ANCHO_LETRA, C_TINTA, 1);
		k = y + 26 + 12 + n * 18 + 6;
		ui_rect(x + 20, k, an - 40, 24, C_AZUL);
		ui_rect(x + 22, k + 2, an - 44, 20, C_PAPEL);
		vis = (an - 52) / ANCHO_LETRA;
		desde = cur > vis ? cur - vis : 0;
		n = (int)strlen(t) - desde;
		if (n > vis)
			n = vis;
		for (i = 0; i < n && i < 255; i++)
			ver[i] = oculto ? '*' : t[desde + i];
		ver[i] = 0;
		ui_texto(x + 26, k + 4, ver, vis, C_TINTA);
		ui_rect(x + 26 + (cur - desde) * ANCHO_LETRA, k + 4, 2, 16, C_TINTA);
		ui_boton(101, x + an - 16 - ui_ancho_boton("Cancelar"), y + al - ALTO_BOTON - 12, 0, "Cancelar", 0, 0);
		ui_boton(100, x + an - 24 - ui_ancho_boton("Cancelar") - ui_ancho_boton("Aceptar"), y + al - ALTO_BOTON - 12, 0, "Aceptar", 1, 0);
		if (!ui_suceso(&s, 1) || s.clase == EV_CERRAR)
			return 0;
		if (s.clase == EV_RATON_BAJA) {
			i = ui_en_zona((int)s.a, (int)s.b);
			if (i == 100)
				return 1;
			if (i == 101)
				return 0;
		}
		if (s.clase == EV_TAMANO) {
			ui_an = (int)s.a;
			ui_al = (int)s.b;
		}
		if (s.clase != EV_TECLA)
			continue;
		k = (int)s.a;
		if (k == T_INTRO)
			return 1;
		if (k == T_ESC)
			return 0;
		n = (int)strlen(t);
		if (k == T_BORRAR && cur > 0) {
			memmove(t + cur - 1, t + cur, (size_t)(n - cur + 1));
			cur--;
		} else if (k == T_SUPR && cur < n)
			memmove(t + cur, t + cur + 1, (size_t)(n - cur));
		else if (k == T_IZQUIERDA && cur > 0)
			cur--;
		else if (k == T_DERECHA && cur < n)
			cur++;
		else if (k == T_INICIO)
			cur = 0;
		else if (k == T_FIN)
			cur = n;
		else if (k >= 32 && k < 256 && k != 127 && n + 1 < cabe) {
			memmove(t + cur + 1, t + cur, (size_t)(n - cur + 1));
			t[cur++] = (char)k;
		}
	}
}

/* una barra de progreso dentro de (x, y, an, 14) */
void ui_barra(int x, int y, int an, uint32_t hecho, uint32_t total)
{
	int k = total ? (int)((uint64_t)hecho * (uint32_t)(an - 2) / total) : 0;

	ui_rect(x, y, an, 14, C_BORDE);
	ui_rect(x + 1, y + 1, an - 2, 12, C_PAPEL);
	ui_rect(x + 1, y + 1, k, 12, 0x16A34A);
}

uint32_t ui_tics(void) { return sc(SC_TICS, 0, 0, 0); }

uint32_t ui_hz(void)
{
	uint32_t h = sc(SC_DATO, DATO_HZ, 0, 0);

	return h == 0 || ES_ERROR(h) ? 100 : h;
}

void ui_serie(const char *t) { sc(SC_SERIE, (uint32_t)t, (uint32_t)strlen(t), 0); }
