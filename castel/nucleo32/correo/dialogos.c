/* dialogos.c - Correo: los controles (botones, campos) y los cuadros que
 * esperan una respuesta (preguntas, avisos, pedir un texto o una contraseña,
 * elegir de una lista o un archivo), el asistente de las cuentas, la
 * libreta de direcciones y los ajustes. Con los colores y la forma de
 * controles.cas; el teclado en todo (Tab, Mayús+Tab, Intro, Esc, flechas) y
 * el que tiene el teclado, con un borde grueso (no solo de otro color).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include <stdio.h>
#include "ventana.h"

int cancelado;

/* (PARPADEO) lo de debajo de un cuadro: la primera vez, entero; luego, sin
 * que cuente como cambiado (ya está así en la pantalla), salvo donde estaba
 * el cuadro antes (por si ha cambiado de medida). Con el lienzo propio, nada
 * de esto se ve a medio pintar: se vuelca de una vez */
typedef struct {
	int hecho, y, al, van, val;
} Fondo;

static void fondo_caja(Fondo *f, int y, int al)
{
	if (f->hecho && (f->van != van || f->val != val))
		f->hecho = 0;
	if (repinta_fondo) {
		if (f->hecho)
			ui_callado(1);
		repinta_fondo();
		ui_callado(0);
	}
	if (f->hecho)
		ui_ensucia(f->y, f->al);
	f->hecho = 1;
	f->y = y;
	f->al = al + 6;
	f->van = van;
	f->val = val;
}

/* ---- las zonas ---- */
#define MAX_ZONAS 400
static struct { int x, y, an, al, id; } zonas[MAX_ZONAS];
static int n_zonas;

void zonas_olvida(void) { n_zonas = 0; }

void zona(int id, int x, int y, int an, int al)
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

int zona_en(int x, int y)
{
	int i;

	for (i = n_zonas; i-- > 0;)
		if (x >= zonas[i].x && y >= zonas[i].y && x < zonas[i].x + zonas[i].an && y < zonas[i].y + zonas[i].al)
			return zonas[i].id;
	return 0;
}

int ancho_texto(const char *t) { return (int)utf8_cuenta(t, (u32)strlen(t)) * LA; }
void texto(int x, int y, const char *t, int max, u32 c) { ui_texto(x, y, t, (u32)strlen(t), max, c, 0); }
void negrita(int x, int y, const char *t, int max, u32 c) { ui_texto(x, y, t, (u32)strlen(t), max, c, 1); }

int boton(int id, int x, int y, int an, const char *t, int principal, int apagado, int foco)
{
	u32 fondo = 0xE5E7EB, tinta = C_TINTA;
	int tl = ancho_texto(t);

	if (an <= 0)
		an = tl + 20;
	if (principal) {
		fondo = C_VERDE;
		tinta = C_PAPEL;
	}
	if (apagado) {
		fondo = 0xF3F4F6;
		tinta = 0x6B7280;
	}
	ui_redondo(x, y, an, ALTO_BOTON, 4, foco ? C_TINTA : C_BORDE, foco ? C_TINTA : C_BORDE);
	ui_redondo(x + (foco ? 2 : 1), y + (foco ? 2 : 1), an - (foco ? 4 : 2), ALTO_BOTON - (foco ? 4 : 2), 4, fondo, fondo);
	negrita(x + (an - tl) / 2, y + 5, t, (an - 4) / LA, tinta);
	if (!apagado)
		zona(id, x, y, an, ALTO_BOTON);
	return an;
}

void marco_hundido(int x, int y, int an, int al, u32 fondo)
{
	ui_rect(x, y, an, al, C_BORDE);
	ui_rect(x + 1, y + 1, an - 2, al - 2, fondo);
}

void caja(int x, int y, int an, int al, const char *titulo)
{
	ui_rect(x + 4, y + 4, an, al, 0x9CA3AF);
	ui_rect(x, y, an, al, C_BORDE);
	ui_rect(x + 1, y + 1, an - 2, al - 2, C_FONDO);
	ui_degradado(x + 1, y + 1, an - 2, 26, 0x2563EB, 0x1D4ED8);
	negrita(x + 10, y + 6, titulo, (an - 20) / LA, C_PAPEL);
}

/* ---- los campos ---- */
void campo_pon(Campo *c, const char *t)
{
	u32 i = 0, n = (u32)strlen(t);

	c->n = 0;
	while (i < n && c->n < CAMPO_MAX - 1)
		c->t[c->n++] = utf8_letra((const u8 *)t, n, &i);
	c->cur = c->n;
	c->desde = 0;
	c->sel = -1;
}

void campo_saca(const Campo *c, char *d, u32 cabe)
{
	u32 k = 0;
	int i;

	for (i = 0; i < c->n && k + 5 < cabe; i++)
		k += (u32)utf8_pon(c->t[i], d + k);
	d[k] = 0;
}

void campo_pinta(Campo *c, int x, int y, int an, int foco)
{
	char d[CAMPO_MAX * 4];
	int vis = (an - 12) / LA, i, k = 0;

	ui_rect(x, y, an, 24, foco ? C_AZUL : C_BORDE);
	if (foco)
		ui_rect(x + 1, y + 1, an - 2, 22, C_AZUL);
	ui_rect(x + (foco ? 2 : 1), y + (foco ? 2 : 1), an - (foco ? 4 : 2), foco ? 20 : 22, C_PAPEL);
	if (c->cur < c->desde)
		c->desde = c->cur;
	if (c->cur > c->desde + vis - 1)
		c->desde = c->cur - vis + 1;
	if (c->desde < 0)
		c->desde = 0;
	if (c->sel >= 0 && c->sel != c->cur) {
		int a = c->sel < c->cur ? c->sel : c->cur, b = c->sel < c->cur ? c->cur : c->sel;

		if (a < c->desde)
			a = c->desde;
		if (b > c->desde + vis)
			b = c->desde + vis;
		if (b > a)
			ui_rect(x + 6 + (a - c->desde) * LA, y + 4, (b - a) * LA, 16, C_AZUL_CLARO);
	}
	for (i = c->desde; i < c->n && i < c->desde + vis; i++)
		k += utf8_pon(c->oculto ? '*' : c->t[i], d + k);
	d[k] = 0;
	ui_texto(x + 6, y + 4, d, (u32)k, vis, C_TINTA, 0);
	if (foco)
		ui_rect(x + 6 + (c->cur - c->desde) * LA, y + 4, 2, 16, C_TINTA);
}

static void quita_sel(Campo *c)
{
	if (c->sel >= 0 && c->sel != c->cur) {
		int a = c->sel < c->cur ? c->sel : c->cur, b = c->sel < c->cur ? c->cur : c->sel;

		memmove(c->t + a, c->t + b, (size_t)(c->n - b) * sizeof(u32));
		c->n -= b - a;
		c->cur = a;
	}
	c->sel = -1;
}

int campo_tecla(Campo *c, u32 k, u32 mod)
{
	(void)mod;
	if (k == UT_BORRAR) {
		if (c->sel >= 0 && c->sel != c->cur)
			quita_sel(c);
		else if (c->cur > 0) {
			memmove(c->t + c->cur - 1, c->t + c->cur, (size_t)(c->n - c->cur) * sizeof(u32));
			c->n--;
			c->cur--;
		}
	} else if (k == UT_SUPR) {
		if (c->sel >= 0 && c->sel != c->cur)
			quita_sel(c);
		else if (c->cur < c->n) {
			memmove(c->t + c->cur, c->t + c->cur + 1, (size_t)(c->n - c->cur - 1) * sizeof(u32));
			c->n--;
		}
	} else if (k == UT_IZQUIERDA) {
		if (c->cur > 0)
			c->cur--;
		c->sel = -1;
	} else if (k == UT_DERECHA) {
		if (c->cur < c->n)
			c->cur++;
		c->sel = -1;
	} else if (k == UT_INICIO) {
		c->cur = 0;
		c->sel = -1;
	} else if (k == UT_FIN) {
		c->cur = c->n;
		c->sel = -1;
	} else if (k == 1) {		/* Ctrl+A */
		c->sel = 0;
		c->cur = c->n;
	} else if (k == 3 || k == 24) {	/* Ctrl+C, Ctrl+X */
		if (!c->oculto && c->sel >= 0 && c->sel != c->cur) {
			char d[CAMPO_MAX * 4];
			int a = c->sel < c->cur ? c->sel : c->cur, b = c->sel < c->cur ? c->cur : c->sel, i;
			u32 m = 0;

			for (i = a; i < b; i++)
				m += (u32)utf8_pon(c->t[i], d + m);
			so_al_portapapeles(d, m);
			if (k == 24)
				quita_sel(c);
		}
	} else if (k == 22) {		/* Ctrl+V */
		u32 n, i = 0;
		char *p = so_del_portapapeles(&n);

		if (p) {
			quita_sel(c);
			while (i < n && c->n < CAMPO_MAX - 1) {
				u32 l = utf8_letra((const u8 *)p, n, &i);

				if (l < 32)
					continue;
				memmove(c->t + c->cur + 1, c->t + c->cur, (size_t)(c->n - c->cur) * sizeof(u32));
				c->t[c->cur++] = l;
				c->n++;
			}
			so_suelta(p);
		}
	} else if (k >= 32 && k != 127 && (k < 0x100 || k >= 0x2000) && c->n < CAMPO_MAX - 1) {
		quita_sel(c);
		memmove(c->t + c->cur + 1, c->t + c->cur, (size_t)(c->n - c->cur) * sizeof(u32));
		c->t[c->cur++] = k;
		c->n++;
	} else
		return 0;
	return 1;
}

void campo_clic(Campo *c, int x, int px)
{
	int k = (px - x - 6 + LA / 2) / LA + c->desde;

	c->cur = k < 0 ? 0 : k > c->n ? c->n : k;
	c->sel = -1;
}

/* ---- los cuadros ---- */
static int renglones(int x, int y, const char *t, int max, u32 c, int pinta)
{
	char r[512];
	int n = 0;

	while (*t) {
		int k = 0, ult = -1, letras = 0;

		while (t[k] && t[k] != '\n' && letras < max && k < 500) {
			if (t[k] == ' ')
				ult = k;
			if (((u8)t[k] & 0xC0) != 0x80)
				letras++;
			k++;
		}
		while (((u8)t[k] & 0xC0) == 0x80 && k < 508)
			k++;
		if (t[k] && t[k] != '\n' && ult > 0)
			k = ult;
		memcpy(r, t, (size_t)k);
		r[k] = 0;
		if (pinta)
			texto(x, y + n * 18, r, max, c);
		n++;
		t += k;
		if (*t == ' ' || *t == '\n')
			t++;
	}
	return n;
}

static int pregunta_de(const char *titulo, const char *t, const char *b1, const char *b2, const char *b3, int peligro)
{
	Fondo fc = { 0, 0, 0, 0, 0 };
	const char *b[3] = { b1, b2, b3 };
	int an = van < 520 ? van - 20 : 500, al, x, y, n, i, elegido = 0, bx;
	Suceso s;

	for (;;) {
		n = renglones(0, 0, t, (an - 40) / LA - (peligro ? 5 : 0), 0, 0);
		al = 26 + 20 + n * 18 + 16 + ALTO_BOTON + 16;
		x = (van - an) / 2;
		y = (val - al) / 2;
		if (y < 0)
			y = 0;
		fondo_caja(&fc, y, al);
		zonas_olvida();
		caja(x, y, an, al, titulo);
		if (peligro) {
			ui_rect(x + 12, y + 34, an - 24, n * 18 + 12, C_ROJO);
			ui_rect(x + 14, y + 36, an - 28, n * 18 + 8, C_ROJO_CLARO);
			negrita(x + 20, y + 40, "[!]", 4, C_ROJO);
			renglones(x + 56, y + 40, t, (an - 40) / LA - 5, C_ROJO, 1);
		} else
			renglones(x + 20, y + 26 + 16, t, (an - 40) / LA, C_TINTA, 1);
		bx = x + an - 16;
		for (i = 2; i >= 0; i--) {
			if (!b[i])
				continue;
			bx -= ancho_texto(b[i]) + 20 + 8;
			boton(100 + i, bx, y + al - ALTO_BOTON - 14, 0, b[i], i == 0 && !peligro, 0, i == elegido);
		}
		if (!ui_espera(&s, 0))
			continue;
		if (s.clase == UE_CERRAR)
			return -1;
		if (s.clase == UE_TECLA) {
			if (s.a == UT_ESC)
				return -1;
			if (s.a == UT_INTRO || s.a == ' ')
				return elegido;
			if (s.a == UT_DERECHA || s.a == UT_TAB || s.a == UT_IZQUIERDA || s.a == UT_MAYUS_TAB) {
				int paso = s.a == UT_IZQUIERDA || s.a == UT_MAYUS_TAB ? 2 : 1;

				do
					elegido = (elegido + paso) % 3;
				while (!b[elegido]);
			}
		}
		if (s.clase == UE_BAJA) {
			i = zona_en((int)s.a, (int)s.b);
			if (i >= 100 && i < 103)
				return i - 100;
		}
		if (s.clase == UE_TAMANO) {
			van = (int)s.a;
			val = (int)s.b;
		}
	}
}

int pregunta(const char *titulo, const char *t, const char *b1, const char *b2, const char *b3)
{
	return pregunta_de(titulo, t, b1, b2, b3, 0);
}

int pregunta_peligro(const char *titulo, const char *t, const char *si, const char *no)
{
	/* (lo de no hacer nada, el primero: Intro no hace lo peligroso) */
	int r = pregunta_de(titulo, t, no, si, NULL, 1);

	return r == 1;
}

void aviso(const char *titulo, const char *t)
{
	pregunta(titulo, t, "Aceptar", NULL, NULL);
}

int pide_texto(const char *titulo, const char *preg, char *t, u32 cabe, int oculto)
{
	Fondo fc = { 0, 0, 0, 0, 0 };
	static Campo c;
	int an = van < 540 ? van - 20 : 520, al, x, y, n, i, foco = 0;
	Suceso s;

	campo_pon(&c, t);
	c.oculto = oculto;
	for (;;) {
		n = renglones(0, 0, preg, (an - 40) / LA, 0, 0);
		al = 26 + 16 + n * 18 + 8 + 26 + 16 + ALTO_BOTON + 14;
		x = (van - an) / 2;
		y = (val - al) / 2;
		if (y < 0)
			y = 0;
		fondo_caja(&fc, y, al);
		zonas_olvida();
		caja(x, y, an, al, titulo);
		renglones(x + 20, y + 26 + 12, preg, (an - 40) / LA, C_TINTA, 1);
		i = y + 26 + 12 + n * 18 + 6;
		campo_pinta(&c, x + 20, i, an - 40, foco == 0);
		zona(99, x + 20, i, an - 40, 24);
		boton(101, x + an - 16 - ancho_texto("Cancelar") - 20, y + al - ALTO_BOTON - 12, 0, "Cancelar", 0, 0, foco == 2);
		boton(100, x + an - 24 - ancho_texto("Cancelar") - ancho_texto("Aceptar") - 40, y + al - ALTO_BOTON - 12, 0, "Aceptar", 1, 0, foco == 1);
		if (!ui_espera(&s, 0))
			continue;
		if (s.clase == UE_CERRAR)
			return 0;
		if (s.clase == UE_BAJA) {
			i = zona_en((int)s.a, (int)s.b);
			if (i == 100)
				goto acepta;
			if (i == 101)
				return 0;
			if (i == 99) {
				foco = 0;
				campo_clic(&c, x + 20, (int)s.a);
			}
		}
		if (s.clase == UE_TAMANO) {
			van = (int)s.a;
			val = (int)s.b;
		}
		if (s.clase != UE_TECLA)
			continue;
		if (s.a == UT_INTRO) {
			if (foco == 2)
				return 0;
			goto acepta;
		}
		if (s.a == UT_ESC)
			return 0;
		if (s.a == UT_TAB)
			foco = (foco + 1) % 3;
		else if (s.a == UT_MAYUS_TAB)
			foco = (foco + 2) % 3;
		else if (foco == 0)
			campo_tecla(&c, s.a, s.b);
	}
acepta:
	campo_saca(&c, t, cabe);
	memset(&c, 0, sizeof c);
	return 1;
}

int elige(const char *titulo, const char *opciones, int marcada)
{
	Fondo fc = { 0, 0, 0, 0, 0 };
	const char *op[64];
	int lar[64], n = 0, an = van < 460 ? van - 20 : 440, al, x, y, i, cur = marcada < 0 ? 0 : marcada, primera = 0, filas;
	const char *p = opciones;
	Suceso s;

	while (*p && n < 64) {
		const char *q = strchr(p, '\n');

		op[n] = p;
		lar[n] = q ? (int)(q - p) : (int)strlen(p);
		n++;
		if (!q)
			break;
		p = q + 1;
	}
	if (!n)
		return -1;
	if (cur >= n)
		cur = 0;
	for (;;) {
		filas = n > 14 ? 14 : n;
		if (filas * FILA + 120 > val)
			filas = (val - 120) / FILA;
		if (filas < 3)
			filas = 3;
		al = 26 + 12 + filas * FILA + 12 + ALTO_BOTON + 14;
		x = (van - an) / 2;
		y = (val - al) / 2;
		if (y < 0)
			y = 0;
		if (cur < primera)
			primera = cur;
		if (cur >= primera + filas)
			primera = cur - filas + 1;
		fondo_caja(&fc, y, al);
		zonas_olvida();
		caja(x, y, an, al, titulo);
		marco_hundido(x + 16, y + 36, an - 32, filas * FILA + 4, C_PAPEL);
		for (i = primera; i < n && i < primera + filas; i++) {
			int fy = y + 38 + (i - primera) * FILA;

			if (i == cur) {
				ui_rect(x + 18, fy, an - 36, FILA, C_ELEGIDO);
				ui_texto(x + 24, fy + 2, ">", 1, 1, C_PAPEL, 1);
			}
			ui_texto(x + 36, fy + 2, op[i], (u32)lar[i], (an - 60) / LA, i == cur ? C_PAPEL : C_TINTA, i == cur);
			zona(200 + i, x + 18, fy, an - 36, FILA);
		}
		boton(101, x + an - 16 - ancho_texto("Cancelar") - 20, y + al - ALTO_BOTON - 12, 0, "Cancelar", 0, 0, 0);
		boton(100, x + an - 24 - ancho_texto("Cancelar") - ancho_texto("Elegir") - 40, y + al - ALTO_BOTON - 12, 0, "Elegir", 1, 0, 0);
		if (!ui_espera(&s, 0))
			continue;
		if (s.clase == UE_CERRAR)
			return -1;
		if (s.clase == UE_TAMANO) {
			van = (int)s.a;
			val = (int)s.b;
		}
		if (s.clase == UE_RUEDA) {
			cur -= (int)(signed char)(s.c & 0xFF);
			if (cur < 0)
				cur = 0;
			if (cur >= n)
				cur = n - 1;
		}
		if (s.clase == UE_BAJA) {
			i = zona_en((int)s.a, (int)s.b);
			if (i == 100)
				return cur;
			if (i == 101)
				return -1;
			if (i >= 200) {
				if (cur == i - 200 && (s.c >> 16) >= 2)
					return cur;
				cur = i - 200;
				if ((s.c >> 16) >= 2)
					return cur;
			}
		}
		if (s.clase == UE_TECLA) {
			if (s.a == UT_ESC)
				return -1;
			if (s.a == UT_INTRO)
				return cur;
			if (s.a == UT_ARRIBA && cur > 0)
				cur--;
			if (s.a == UT_ABAJO && cur < n - 1)
				cur++;
			if (s.a == UT_INICIO)
				cur = 0;
			if (s.a == UT_FIN)
				cur = n - 1;
		}
	}
}

/* ---- elegir un archivo: las unidades, las carpetas y lo de dentro ---- */
#define MAX_EN 300
typedef struct {
	char nombre[MAX_EN][96];
	u8 carpeta[MAX_EN];
	u32 tam[MAX_EN];
	int n;
	const char *filtro;
} Lista_ar;

static int igual_ext(const char *n, const char *f)
{
	const char *p = strrchr(n, '.');

	if (!f || !f[0])
		return 1;
	if (!p)
		return 0;
	p++;
	for (; *p && *f; p++, f++)
		if ((*p | 32) != (*f | 32))
			return 0;
	return !*p && !*f;
}

static int pon_entrada(void *u, const char *nombre, u32 tam, int carpeta)
{
	Lista_ar *l = u;
	int i, j;

	if (l->n >= MAX_EN || strlen(nombre) >= 96)
		return 0;
	if (!carpeta && !igual_ext(nombre, l->filtro))
		return 0;
	/* (las carpetas delante; por nombre) */
	for (i = 0; i < l->n; i++)
		if ((carpeta && !l->carpeta[i]) || (carpeta == l->carpeta[i] && strcmp(nombre, l->nombre[i]) < 0))
			break;
	for (j = l->n; j > i; j--) {
		memcpy(l->nombre[j], l->nombre[j - 1], 96);
		l->carpeta[j] = l->carpeta[j - 1];
		l->tam[j] = l->tam[j - 1];
	}
	snprintf(l->nombre[i], 96, "%s", nombre);
	l->carpeta[i] = (u8)carpeta;
	l->tam[i] = tam;
	l->n++;
	return 0;
}

int elige_archivo(const char *titulo, char *ruta, u32 cabe, int para_guardar, const char *filtro)
{
	Fondo fc = { 0, 0, 0, 0, 0 };
	static Lista_ar l;
	static Campo nom;
	char dir[260], *p;
	int an = van < 620 ? van - 20 : 600, al, x, y, i, cur = 0, primera = 0, filas, foco = para_guardar ? 1 : 0, rehacer = 1;
	Suceso s;
	char b = so_barra();

	/* de dónde se empieza: la carpeta de «ruta», o la de los documentos */
	snprintf(dir, sizeof dir, "%s", ruta);
	p = strrchr(dir, b);
	if (p && ruta[0]) {
		campo_pon(&nom, p + 1);
		*p = 0;
		if (dir[1] == ':' && !dir[2])
			strcat(dir, b == '\\' ? "\\" : "/");
	} else {
		campo_pon(&nom, ruta);
		so_carpeta_documentos(dir, sizeof dir);
	}
	for (;;) {
		if (rehacer) {
			memset(&l, 0, sizeof l.n);
			l.n = 0;
			l.filtro = filtro;
			/* (las unidades, arriba del todo, si es Hispa-Pentium) */
			if (b == '\\') {
				const char *u = "ACDEFGHIJKLMNOPQRSTUVWXYZ";

				for (; *u; u++) {
					char t[4] = { *u, ':', '\\', 0 };
					int c;

					if (so_existe(t, &c) && l.n < MAX_EN) {
						snprintf(l.nombre[l.n], 96, "%c:", *u);
						l.carpeta[l.n] = 2;
						l.n++;
					}
				}
			}
			if (strlen(dir) > 3 || b != '\\') {
				snprintf(l.nombre[l.n], 96, "..");
				l.carpeta[l.n] = 3;
				l.n++;
			}
			so_carpeta(dir, pon_entrada, &l);
			cur = 0;
			primera = 0;
			rehacer = 0;
		}
		filas = (val - 200) / FILA;
		if (filas > 16)
			filas = 16;
		if (filas < 4)
			filas = 4;
		al = 26 + 34 + filas * FILA + (para_guardar ? 40 : 12) + ALTO_BOTON + 14;
		x = (van - an) / 2;
		y = (val - al) / 2;
		if (y < 0)
			y = 0;
		if (cur < primera)
			primera = cur;
		if (cur >= primera + filas)
			primera = cur - filas + 1;
		fondo_caja(&fc, y, al);
		zonas_olvida();
		caja(x, y, an, al, titulo);
		texto(x + 16, y + 36, dir, (an - 32) / LA, C_SUAVE);
		marco_hundido(x + 16, y + 56, an - 32, filas * FILA + 4, C_PAPEL);
		for (i = primera; i < l.n && i < primera + filas; i++) {
			int fy = y + 58 + (i - primera) * FILA;
			char t[140];

			if (i == cur)
				ui_rect(x + 18, fy, an - 36, FILA, foco == 0 ? C_ELEGIDO : C_AZUL_CLARO);
			if (l.carpeta[i] == 2)
				snprintf(t, sizeof t, "[unidad] %s", l.nombre[i]);
			else if (l.carpeta[i] == 3)
				snprintf(t, sizeof t, "[subir]  ..");
			else if (l.carpeta[i])
				snprintf(t, sizeof t, "[carpeta] %s", l.nombre[i]);
			else
				snprintf(t, sizeof t, "%s  (%u KB)", l.nombre[i], (unsigned)((l.tam[i] + 1023) / 1024));
			ui_texto(x + 24, fy + 2, t, (u32)strlen(t), (an - 50) / LA, i == cur && foco == 0 ? C_PAPEL : C_TINTA, l.carpeta[i] != 0);
			zona(200 + i, x + 18, fy, an - 36, FILA);
		}
		if (para_guardar) {
			texto(x + 16, y + 64 + filas * FILA, "Nombre:", 8, C_TINTA);
			campo_pinta(&nom, x + 84, y + 60 + filas * FILA, an - 100, foco == 1);
			zona(99, x + 84, y + 60 + filas * FILA, an - 100, 24);
		}
		boton(101, x + an - 16 - ancho_texto("Cancelar") - 20, y + al - ALTO_BOTON - 12, 0, "Cancelar", 0, 0, 0);
		boton(100, x + an - 24 - ancho_texto("Cancelar") - ancho_texto(para_guardar ? "Guardar" : "Abrir") - 40,
		      y + al - ALTO_BOTON - 12, 0, para_guardar ? "Guardar" : "Abrir", 1, 0, 0);
		if (!ui_espera(&s, 0))
			continue;
		if (s.clase == UE_CERRAR)
			return 0;
		if (s.clase == UE_TAMANO) {
			van = (int)s.a;
			val = (int)s.b;
		}
		if (s.clase == UE_RUEDA) {
			cur -= 3 * (int)(signed char)(s.c & 0xFF);
			if (cur < 0)
				cur = 0;
			if (cur >= l.n)
				cur = l.n - 1;
		}
		{
			int abre = 0, k;

			if (s.clase == UE_BAJA) {
				k = zona_en((int)s.a, (int)s.b);
				if (k == 101)
					return 0;
				if (k == 99) {
					foco = 1;
					campo_clic(&nom, x + 84, (int)s.a);
				}
				if (k >= 200) {
					cur = k - 200;
					foco = 0;
					if ((s.c >> 16) >= 2)
						abre = 1;
					else if (para_guardar && !l.carpeta[cur])
						campo_pon(&nom, l.nombre[cur]);
				}
				if (k == 100)
					abre = 2;
			}
			if (s.clase == UE_TECLA) {
				if (s.a == UT_ESC)
					return 0;
				if (s.a == UT_TAB || s.a == UT_MAYUS_TAB)
					foco = para_guardar ? !foco : 0;
				else if (s.a == UT_INTRO)
					abre = foco == 1 ? 2 : 1;
				else if (foco == 0 && s.a == UT_ARRIBA && cur > 0)
					cur--;
				else if (foco == 0 && s.a == UT_ABAJO && cur < l.n - 1)
					cur++;
				else if (foco == 0 && s.a == UT_BORRAR) {
					abre = 1;
					cur = -1;
				} else if (foco == 1)
					campo_tecla(&nom, s.a, s.b);
				else if (foco == 0 && s.a > ' ' && s.a < 0x80) {
					/* buscar tecleando: lo que empieza por lo escrito (en un segundo) */
					static char bus[40];
					static u32 cuando;
					u32 bl;
					int j;

					if ((u32)(so_tics() - cuando) > so_hz())
						bus[0] = 0;
					cuando = so_tics();
					bl = (u32)strlen(bus);
					if (bl + 1 < sizeof bus) {
						bus[bl] = (char)(s.a >= 'a' && s.a <= 'z' ? s.a - 32 : s.a);
						bus[bl + 1] = 0;
					}
					for (j = 0; j < l.n; j++) {
						u32 q;

						for (q = 0; bus[q] && l.nombre[j][q] &&
							    (l.nombre[j][q] >= 'a' && l.nombre[j][q] <= 'z' ? l.nombre[j][q] - 32 : l.nombre[j][q]) == bus[q]; q++)
							;
						if (!bus[q] && l.carpeta[j] < 2) {
							cur = j;
							break;
						}
					}
				}
			}
			if (abre == 1 && cur < 0) {
				/* subir */
				p = strrchr(dir, b);
				if (p && (b != '\\' || strlen(dir) > 3)) {
					*p = 0;
					if (dir[1] == ':' && !dir[2])
						strcat(dir, "\\");
					if (!dir[0])
						strcpy(dir, "/");
				}
				rehacer = 1;
			} else if (abre == 1 && cur >= 0 && cur < l.n && l.carpeta[cur]) {
				if (l.carpeta[cur] == 2)
					snprintf(dir, sizeof dir, "%s\\", l.nombre[cur]);
				else if (l.carpeta[cur] == 3) {
					p = strrchr(dir, b);
					if (p) {
						*p = 0;
						if (dir[1] == ':' && !dir[2])
							strcat(dir, "\\");
						if (!dir[0])
							strcpy(dir, "/");
					}
				} else {
					u32 n = (u32)strlen(dir);

					snprintf(dir + n, sizeof dir - n, "%s%s", n && dir[n - 1] == b ? "" : (b == '\\' ? "\\" : "/"), l.nombre[cur]);
				}
				rehacer = 1;
			} else if (abre) {
				char nm[200];
				u32 n = (u32)strlen(dir);

				if (para_guardar)
					campo_saca(&nom, nm, sizeof nm);
				else if (cur >= 0 && cur < l.n && !l.carpeta[cur])
					snprintf(nm, sizeof nm, "%s", l.nombre[cur]);
				else
					nm[0] = 0;
				if (nm[0]) {
					if (nm[1] == ':' || nm[0] == b)
						snprintf(ruta, cabe, "%s", nm);	/* (una ruta entera escrita a mano) */
					else
						snprintf(ruta, cabe, "%s%s%s", dir, n && dir[n - 1] == b ? "" : (b == '\\' ? "\\" : "/"), nm);
					return 1;
				}
			}
		}
	}
}

/* =============================================================================
 *  La barra de progreso, encima de todo (mientras se espera a la red)
 * ============================================================================= */
static u32 ult_progreso;

void progreso(const char *que, u32 hecho, u32 total)
{
	int an = 420, x = (van - an) / 2, y = val / 2 - 50;
	int k;

	ui_rect(x + 3, y + 3, an, 92, 0x9CA3AF);
	ui_rect(x, y, an, 92, C_BORDE);
	ui_rect(x + 1, y + 1, an - 2, 90, C_FONDO);
	texto(x + 14, y + 12, que, (an - 28) / LA, C_TINTA);
	ui_rect(x + 14, y + 36, an - 28, 14, C_BORDE);
	ui_rect(x + 15, y + 37, an - 30, 12, C_PAPEL);
	if (total) {
		k = (int)((u64)hecho * (u32)(an - 30) / total);
		ui_rect(x + 15, y + 37, k, 12, 0x16A34A);
	} else {
		/* sin total: una franja que corre */
		k = (int)((so_tics() / 4) % (u32)(an - 90));
		ui_rect(x + 15 + k, y + 37, 60, 12, 0x16A34A);
	}
	zonas_olvida();
	boton(1, x + an - 14 - ancho_texto("Cancelar") - 20, y + 58, 0, "Cancelar", 0, 0, 0);
	ult_progreso = so_tics();
}

/* (red_espera) mientras se espera a la red: la barra y «Cancelar» (o Esc) */
static const char *que_espera = "Esperando al servidor...";

int espera_red(void *u)
{
	Suceso s;

	(void)u;
	if ((u32)(so_tics() - ult_progreso) * 1000 / so_hz() > 250) {
		progreso(ses && ses->estado[0] ? ses->estado : que_espera, 0, 0);
	}
	while (ui_suceso(&s, 0)) {
		if (s.clase == UE_TECLA && s.a == UT_ESC)
			cancelado = 1;
		if (s.clase == UE_BAJA && zona_en((int)s.a, (int)s.b) == 1)
			cancelado = 1;
		if (s.clase == UE_TAMANO) {
			van = (int)s.a;
			val = (int)s.b;
		}
	}
	return cancelado;
}

/* =============================================================================
 *  El asistente de una cuenta
 * ============================================================================= */
#define AC_CORREO 0
#define AC_NOMBRE 1
#define AC_CLAVE 2
#define AC_ENTRADA 3
#define AC_PE 4
#define AC_SALIDA 5
#define AC_PS 6
#define AC_USUARIO 7
#define AC_N 8

static void paso_auto(void *u, const char *que)
{
	(void)u;
	progreso(que, 0, 0);
}

static const char *nombre_modo(int m)
{
	switch (m) {
	case SO_TLS: return "TLS (cifrada desde el principio)";
	case SO_STARTTLS_IMAP: case SO_STARTTLS_SMTP: case SO_STARTTLS_POP3: return "STARTTLS (cifrada tras saludar)";
	}
	return "SIN CIFRAR";
}

int asistente_cuenta(int ic)
{
	Fondo fc = { 0, 0, 0, 0, 0 };
	static Campo f[AC_N];
	static const char *const rotulo[AC_N] = { "Direcci\xc3\xb3n de correo:", "Su nombre (lo ven los dem\xc3\xa1s):", "Contrase\xc3\xb1" "a:",
		"Servidor de entrada:", "Puerto:", "Servidor de salida (SMTP):", "Puerto:", "Usuario:" };
	Cuenta c;
	Autocfg ac;
	int paso = ic >= 0 ? 1 : 0, foco = 0, recordar = 1, pop = 0, sin_cifrar = 0, me = SO_TLS, ms = SO_TLS, i, x, y, an, al;
	char t[600], aviso_pr[400] = "", de_donde[160] = "";
	Suceso s;

	memset(f, 0, sizeof f);
	if (ic >= 0) {
		c = cuentas[ic];
		campo_pon(&f[AC_CORREO], c.correo);
		campo_pon(&f[AC_NOMBRE], c.visible);
		campo_pon(&f[AC_CLAVE], c.clave);
		campo_pon(&f[AC_ENTRADA], c.entrada);
		snprintf(t, sizeof t, "%u", (unsigned)c.puerto_entrada);
		campo_pon(&f[AC_PE], t);
		campo_pon(&f[AC_SALIDA], c.salida);
		snprintf(t, sizeof t, "%u", (unsigned)c.puerto_salida);
		campo_pon(&f[AC_PS], t);
		campo_pon(&f[AC_USUARIO], c.usuario);
		recordar = c.guardar_clave;
		pop = c.tipo == CT_POP3;
		sin_cifrar = c.sin_cifrar;
		me = c.modo_entrada;
		ms = c.modo_salida;
	} else
		memset(&c, 0, sizeof c);
	f[AC_CLAVE].oculto = 1;
	for (;;) {
		int primero = paso == 0 ? AC_CORREO : AC_ENTRADA, ultimo = paso == 0 ? AC_CLAVE : AC_USUARIO, nf = ultimo - primero + 1;
		int fy;

		an = van < 640 ? van - 16 : 620;
		al = paso == 0 ? 400 : 470;
		if (al > val - 10)
			al = val - 10;
		x = (van - an) / 2;
		y = (val - al) / 2;
		if (y < 0)
			y = 0;
		fondo_caja(&fc, y, al);
		zonas_olvida();
		caja(x, y, an, al, ic >= 0 ? "Cambiar la cuenta" : (paso == 0 ? "Cuenta de correo nueva (1 de 2)" : "Cuenta de correo nueva (2 de 2)"));
		fy = y + 36;
		if (paso == 0) {
			renglones(x + 16, fy, "Escriba su direcci\xc3\xb3n y su contrase\xc3\xb1" "a: los servidores se buscan solos.", (an - 32) / LA, C_TINTA, 1);
			fy += 26;
		} else {
			snprintf(t, sizeof t, "%s%s", de_donde[0] ? "Encontrado en " : "Escriba los servidores (los dice su proveedor).", de_donde);
			renglones(x + 16, fy, t, (an - 32) / LA, C_TINTA, 1);
			fy += 22;
		}
		for (i = primero; i <= ultimo; i++) {
			int ancho = an - 32 - 230, cx = x + 230;

			if (i == AC_PE || i == AC_PS)
				continue;	/* (van en el renglón de su servidor) */
			texto(x + 16, fy + 4, rotulo[i], 26, C_TINTA);
			if (i == AC_ENTRADA || i == AC_SALIDA) {
				int p = i + 1;

				ancho -= 170;
				texto(cx + ancho + 12, fy + 4, "Puerto:", 8, C_TINTA);
				campo_pinta(&f[p], x + an - 16 - 90, fy, 90, foco == p - primero);
				zona(300 + p, x + an - 16 - 90, fy, 90, 24);
			}
			campo_pinta(&f[i], cx, fy, ancho, foco == i - primero);
			zona(300 + i, cx, fy, ancho, 24);
			fy += 30;
			if (i == AC_ENTRADA || i == AC_SALIDA) {
				int m = i == AC_ENTRADA ? me : ms;

				snprintf(t, sizeof t, "Cifrado: %s", nombre_modo(m));
				if (m == SO_PLANO) {
					ui_rect(x + 230, fy - 2, an - 246, 20, C_ROJO_CLARO);
					negrita(x + 234, fy, "[!]", 3, C_ROJO);
					ui_texto(x + 266, fy, t, (u32)strlen(t), (an - 290) / LA, C_ROJO, 1);
				} else {
					icono(IC_CANDADO, x + 232, fy + 2, C_VERDE, C_FONDO);
					texto(x + 250, fy, t, (an - 290) / LA, C_TINTA);
				}
				zona(i == AC_ENTRADA ? 410 : 411, x + 230, fy - 2, an - 246, 20);
				fy += 24;
			}
		}
		/* las casillas */
		if (paso == 0) {
			marco_hundido(x + 16, fy + 2, 16, 16, C_PAPEL);
			if (recordar)
				negrita(x + 20, fy + 2, "X", 1, C_TINTA);
			texto(x + 40, fy + 2, "Recordar la contrase\xc3\xb1" "a en este equipo (solo la lee Correo)", (an - 56) / LA, C_TINTA);
			zona(400, x + 16, fy, an - 32, 20);
			fy += 28;
			renglones(x + 16, fy,
				  "Gmail, Outlook, Yahoo e iCloud piden una \xc2\xab" "contrase\xc3\xb1" "a de aplicaci\xc3\xb3n\xc2\xbb en lugar de la "
				  "de siempre: se hace en la web de la cuenta (Seguridad). Entrar con OAuth2 llegar\xc3\xa1 m\xc3\xa1s adelante.",
				  (an - 32) / LA, C_SUAVE, 1);
		} else {
			marco_hundido(x + 16, fy + 2, 16, 16, C_PAPEL);
			if (pop)
				negrita(x + 20, fy + 2, "X", 1, C_TINTA);
			texto(x + 40, fy + 2, "Usar POP3 (bajarlo todo aqu\xc3\xad) en vez de IMAP", (an - 56) / LA, C_TINTA);
			zona(401, x + 16, fy, an - 32, 20);
			fy += 24;
			marco_hundido(x + 16, fy + 2, 16, 16, C_PAPEL);
			if (sin_cifrar)
				negrita(x + 20, fy + 2, "X", 1, C_ROJO);
			texto(x + 40, fy + 2, "Permitir sin cifrar (solo servidores de casa: 192.168..., .local)", (an - 56) / LA,
			      sin_cifrar ? C_ROJO : C_TINTA);
			zona(402, x + 16, fy, an - 32, 20);
			fy += 26;
			if (aviso_pr[0]) {
				int nr = renglones(0, 0, aviso_pr, (an - 72) / LA, 0, 0);

				ui_rect(x + 16, fy, an - 32, nr * 18 + 8, C_AMBAR_CLARO);
				negrita(x + 20, fy + 4, "[i]", 3, C_TINTA);
				renglones(x + 52, fy + 4, aviso_pr, (an - 72) / LA, C_TINTA, 1);
			}
		}
		boton(101, x + an - 16 - ancho_texto("Cancelar") - 20, y + al - ALTO_BOTON - 12, 0, "Cancelar", 0, 0, foco == nf + 1);
		boton(100, x + an - 24 - ancho_texto("Cancelar") - ancho_texto(paso == 0 ? "Siguiente" : "Probar y guardar") - 40,
		      y + al - ALTO_BOTON - 12, 0, paso == 0 ? "Siguiente" : "Probar y guardar", 1, 0, foco == nf);
		if (paso == 1 && ic < 0)
			boton(102, x + 16, y + al - ALTO_BOTON - 12, 0, "Volver", 0, 0, 0);
		if (!ui_espera(&s, 0))
			continue;
		if (s.clase == UE_CERRAR)
			return 0;
		if (s.clase == UE_TAMANO) {
			van = (int)s.a;
			val = (int)s.b;
		}
		{
			int accion = 0, k;

			if (s.clase == UE_BAJA) {
				k = zona_en((int)s.a, (int)s.b);
				if (k >= 300 && k < 300 + AC_N) {
					foco = k - 300 - primero;
					campo_clic(&f[k - 300], 0, 0);
					f[k - 300].cur = f[k - 300].n;
				} else if (k == 400)
					recordar = !recordar;
				else if (k == 401) {
					pop = !pop;
					if (pop && me == SO_TLS && f[AC_PE].n == 3 && f[AC_PE].t[0] == '9')
						campo_pon(&f[AC_PE], "995"), me = SO_TLS;
					else if (!pop)
						campo_pon(&f[AC_PE], "993");
				} else if (k == 402) {
					if (!sin_cifrar) {
						if (pregunta_peligro("Permitir sin cifrar",
								     "Sin cifrar, la contrase\xc3\xb1" "a y los mensajes viajan a la vista de cualquiera que est\xc3\xa9 "
								     "en medio. Solo para un servidor de su propia red (de casa o de la asociaci\xc3\xb3n). "
								     "\xc2\xbf" "Permitirlo?", "S\xc3\xad, es de casa", "No"))
							sin_cifrar = 1;
					} else
						sin_cifrar = 0;
				} else if (k == 410 || k == 411) {
					int *m = k == 410 ? &me : &ms;
					int st = k == 411 ? SO_STARTTLS_SMTP : pop ? SO_STARTTLS_POP3 : SO_STARTTLS_IMAP;
					int e = elige("El cifrado", sin_cifrar ? "TLS (cifrada desde el principio: 993, 995, 465)\nSTARTTLS (cifrada tras saludar: 143, 110, 587)\nSin cifrar (solo en casa)" :
						      "TLS (cifrada desde el principio: 993, 995, 465)\nSTARTTLS (cifrada tras saludar: 143, 110, 587)",
						      *m == SO_TLS ? 0 : *m == SO_PLANO ? 2 : 1);

					if (e == 0)
						*m = SO_TLS;
					else if (e == 1)
						*m = st;
					else if (e == 2)
						*m = SO_PLANO;
				} else if (k == 100)
					accion = 1;
				else if (k == 101)
					return 0;
				else if (k == 102)
					paso = 0, foco = 0;
			}
			if (s.clase == UE_TECLA) {
				if (s.a == UT_ESC)
					return 0;
				if (s.a == UT_TAB)
					foco = (foco + 1) % (nf + 2);
				else if (s.a == UT_MAYUS_TAB)
					foco = (foco + nf + 1) % (nf + 2);
				else if (s.a == UT_INTRO) {
					if (foco == nf + 1)
						return 0;
					if (foco < nf - 1)
						foco++;
					else
						accion = 1;
				} else if (foco < nf)
					campo_tecla(&f[primero + foco], s.a, s.b);
			}
			if (!accion)
				continue;
			if (paso == 0) {
				char co[128];

				campo_saca(&f[AC_CORREO], co, sizeof co);
				if (!mime_dir_valida(co)) {
					aviso("Cuenta nueva", "La direcci\xc3\xb3n no parece v\xc3\xa1lida (ana@ejemplo.es).");
					foco = 0;
					continue;
				}
				cancelado = 0;
				red_espera = espera_red;
				autocfg_busca(co, &ac, paso_auto, NULL);
				c = ac.c;
				snprintf(aviso_pr, sizeof aviso_pr, "%s", ac.aviso);
				snprintf(de_donde, sizeof de_donde, "%s.", ac.encontrada ? ac.de_donde : "");
				if (!ac.encontrada)
					de_donde[0] = 0;
				campo_pon(&f[AC_ENTRADA], c.entrada);
				snprintf(t, sizeof t, "%u", (unsigned)c.puerto_entrada);
				campo_pon(&f[AC_PE], t);
				campo_pon(&f[AC_SALIDA], c.salida);
				snprintf(t, sizeof t, "%u", (unsigned)c.puerto_salida);
				campo_pon(&f[AC_PS], t);
				campo_pon(&f[AC_USUARIO], c.usuario);
				pop = c.tipo == CT_POP3;
				me = c.modo_entrada;
				ms = c.modo_salida;
				paso = 1;
				foco = AC_USUARIO - AC_ENTRADA + 1;	/* («Probar y guardar») */
				so_apunta("asistente: %s: %s", co, ac.encontrada ? ac.de_donde : "no encontrado");
				continue;
			}
			/* probar y guardar */
			{
				Cuenta n = c;
				u32 v;
				Imap m;
				Pop3 p;
				char clave[128], e[400];
				int ok;

				memset(&n, 0, sizeof n);
				campo_saca(&f[AC_CORREO], n.correo, sizeof n.correo);
				campo_saca(&f[AC_NOMBRE], n.visible, sizeof n.visible);
				campo_saca(&f[AC_ENTRADA], n.entrada, sizeof n.entrada);
				campo_saca(&f[AC_SALIDA], n.salida, sizeof n.salida);
				campo_saca(&f[AC_USUARIO], n.usuario, sizeof n.usuario);
				campo_saca(&f[AC_CLAVE], clave, sizeof clave);
				campo_saca(&f[AC_PE], t, sizeof t);
				for (v = 0, i = 0; t[i] >= '0' && t[i] <= '9'; i++)
					v = v * 10 + (u32)(t[i] - '0');
				n.puerto_entrada = v;
				campo_saca(&f[AC_PS], t, sizeof t);
				for (v = 0, i = 0; t[i] >= '0' && t[i] <= '9'; i++)
					v = v * 10 + (u32)(t[i] - '0');
				n.puerto_salida = v;
				snprintf(n.nombre, sizeof n.nombre, "%s", n.correo);
				n.tipo = pop ? CT_POP3 : CT_IMAP;
				if (me != SO_TLS && me != SO_PLANO)
					me = pop ? SO_STARTTLS_POP3 : SO_STARTTLS_IMAP;
				n.modo_entrada = me;
				n.modo_salida = ms;
				n.sin_cifrar = sin_cifrar;
				n.guardar_clave = recordar;
				n.dejar_en_servidor = 1;
				n.avisar = 1;
				if (ic >= 0) {
					n.num = cuentas[ic].num;
					n.dejar_en_servidor = cuentas[ic].dejar_en_servidor;
					n.max_kb = cuentas[ic].max_kb;
					snprintf(n.firma, sizeof n.firma, "%s", cuentas[ic].firma);
				}
				snprintf(n.clave, sizeof n.clave, "%s", recordar ? clave : "");
				if ((me == SO_PLANO || ms == SO_PLANO) && (!sin_cifrar || !servidor_local(me == SO_PLANO ? n.entrada : n.salida))) {
					aviso("Sin cifrar, no", "Sin cifrar solo se deja para un servidor de su propia red (192.168..., 10..., .local), "
					      "y con \xc2\xab" "Permitir sin cifrar\xc2\xbb marcado. Elija TLS o STARTTLS.");
					continue;
				}
				cancelado = 0;
				red_espera = espera_red;
				snprintf(ses ? ses->estado : t, 300, "Probando %s...", n.entrada);
				progreso("Probando la entrada...", 0, 0);
				if (pop) {
					ok = pop3_conecta(&p, &n, clave) == 0;
					snprintf(e, sizeof e, "%s", p.error);
					if (ok)
						pop3_cierra(&p);
				} else {
					ok = imap_conecta(&m, &n, clave) == 0;
					snprintf(e, sizeof e, "%s", m.error);
					imap_cierra(&m);
				}
				if (!ok) {
					snprintf(t, sizeof t, "No se ha podido entrar en %s: %s", n.entrada, e);
					if (strstr(e, "certificado") || strstr(e, "STARTTLS"))
						pregunta_peligro("La conexi\xc3\xb3n no es segura", t, "Entendido", "Volver");
					else
						aviso("No se ha podido entrar", t);
					memset(clave, 0, sizeof clave);
					continue;
				}
				if (ic >= 0)
					cuentas[ic] = n;
				else if (cuenta_nueva(&n) < 0) {
					aviso("Cuenta nueva", "No caben m\xc3\xa1s cuentas.");
					return 0;
				}
				cuentas_guarda();
				if (!recordar && ses) {
					/* (la de la sesión, solo en la memoria) */
				}
				if (ic < 0)
					snprintf(cuentas[n_cuentas - 1].clave, sizeof cuentas[0].clave, "%s", clave);
				memset(clave, 0, sizeof clave);
				so_apunta("asistente: cuenta guardada: %s (%s %s:%u, SMTP %s:%u)", n.correo, pop ? "POP3" : "IMAP", n.entrada,
					  (unsigned)n.puerto_entrada, n.salida, (unsigned)n.puerto_salida);
				return 1;
			}
		}
	}
}

/* =============================================================================
 *  La libreta de direcciones
 * ============================================================================= */
static int edita_contacto(Contacto *c)
{
	char t[256];

	snprintf(t, sizeof t, "%s", c->nombre);
	if (!pide_texto("Contacto", "Nombre:", t, sizeof t, 0))
		return 0;
	snprintf(c->nombre, sizeof c->nombre, "%s", t);
	snprintf(t, sizeof t, "%s", c->correo[0]);
	for (;;) {
		if (!pide_texto("Contacto", "Direcci\xc3\xb3n de correo:", t, sizeof t, 0))
			return 0;
		if (mime_dir_valida(t))
			break;
		aviso("Contacto", "La direcci\xc3\xb3n no parece v\xc3\xa1lida.");
	}
	snprintf(c->correo[0], sizeof c->correo[0], "%s", t);
	snprintf(t, sizeof t, "%s", c->telefono);
	if (pide_texto("Contacto", "Tel\xc3\xa9" "fono (si quiere):", t, sizeof t, 0))
		snprintf(c->telefono, sizeof c->telefono, "%s", t);
	return 1;
}

int ventana_libreta(char *elegida, u32 cabe)
{
	Fondo fc = { 0, 0, 0, 0, 0 };
	int cur = 0, primera = 0, filas, an, al, x, y, i;
	Suceso s;

	for (;;) {
		an = van - 40 > 700 ? 700 : van - 20;
		al = val - 40 > 480 ? 480 : val - 10;
		x = (van - an) / 2;
		y = (val - al) / 2;
		filas = (al - 26 - 40 - ALTO_BOTON - 30) / FILA;
		if (cur >= n_libreta)
			cur = n_libreta - 1;
		if (cur < 0)
			cur = 0;
		if (cur < primera)
			primera = cur;
		if (cur >= primera + filas)
			primera = cur - filas + 1;
		fondo_caja(&fc, y, al);
		zonas_olvida();
		caja(x, y, an, al, "Libreta de direcciones");
		marco_hundido(x + 12, y + 36, an - 24, filas * FILA + 4, C_PAPEL);
		if (!n_libreta)
			texto(x + 24, y + 44, "(vac\xc3\xad" "a: \xc2\xabNuevo\xc2\xbb, o \xc2\xabImportar\xc2\xbb un .vcf)", (an - 48) / LA, C_SUAVE);
		for (i = primera; i < n_libreta && i < primera + filas; i++) {
			int fy = y + 38 + (i - primera) * FILA;
			char t[300];

			if (i == cur)
				ui_rect(x + 14, fy, an - 28, FILA, C_ELEGIDO);
			snprintf(t, sizeof t, "%s%s<%s>%s%s", libreta[i].nombre, libreta[i].nombre[0] ? "  " : "", libreta[i].correo[0],
				 libreta[i].telefono[0] ? "  tel. " : "", libreta[i].telefono);
			ui_texto(x + 22, fy + 2, t, (u32)strlen(t), (an - 44) / LA, i == cur ? C_PAPEL : C_TINTA, i == cur);
			zona(200 + i, x + 14, fy, an - 28, FILA);
		}
		{
			int bx = x + 12, by = y + al - ALTO_BOTON - 12;

			if (elegida)
				bx += boton(110, bx, by, 0, "Escribirle", 1, !n_libreta, 0) + 6;
			bx += boton(111, bx, by, 0, "Nuevo", 0, 0, 0) + 6;
			bx += boton(112, bx, by, 0, "Cambiar", 0, !n_libreta, 0) + 6;
			bx += boton(113, bx, by, 0, "Quitar", 0, !n_libreta, 0) + 6;
			bx += boton(114, bx, by, 0, "Importar...", 0, 0, 0) + 6;
			bx += boton(115, bx, by, 0, "Exportar...", 0, !n_libreta, 0) + 6;
			boton(116, x + an - 12 - ancho_texto("Cerrar") - 20, by, 0, "Cerrar", 0, 0, 0);
		}
		if (!ui_espera(&s, 0))
			continue;
		if (s.clase == UE_CERRAR)
			return 0;
		if (s.clase == UE_TAMANO) {
			van = (int)s.a;
			val = (int)s.b;
		}
		if (s.clase == UE_RUEDA)
			cur -= 3 * (int)(signed char)(s.c & 0xFF);
		{
			int k = 0;

			if (s.clase == UE_TECLA) {
				if (s.a == UT_ESC)
					return 0;
				if (s.a == UT_ARRIBA)
					cur--;
				if (s.a == UT_ABAJO)
					cur++;
				if (s.a == UT_SUPR)
					k = 113;
				if (s.a == UT_INTRO)
					k = elegida ? 110 : 112;
			}
			if (s.clase == UE_BAJA) {
				k = zona_en((int)s.a, (int)s.b);
				if (k >= 200) {
					cur = k - 200;
					k = (s.c >> 16) >= 2 ? (elegida ? 110 : 112) : 0;
				}
			}
			if (k == 116)
				return 0;
			if (k == 110 && n_libreta && elegida) {
				snprintf(elegida, cabe, "%s%s<%s>", libreta[cur].nombre, libreta[cur].nombre[0] ? " " : "", libreta[cur].correo[0]);
				return 1;
			}
			if (k == 111 && n_libreta < MAX_CONTACTOS) {
				Contacto c;

				memset(&c, 0, sizeof c);
				if (edita_contacto(&c)) {
					libreta[n_libreta++] = c;
					cur = n_libreta - 1;
					libreta_guarda();
				}
			}
			if (k == 112 && n_libreta && edita_contacto(&libreta[cur]))
				libreta_guarda();
			if (k == 113 && n_libreta) {
				char t[300];

				snprintf(t, sizeof t, "\xc2\xbfQuitar a %s <%s> de la libreta?", libreta[cur].nombre, libreta[cur].correo[0]);
				if (pregunta("Quitar", t, "Quitar", "No", NULL) == 0) {
					memmove(&libreta[cur], &libreta[cur + 1], (size_t)(n_libreta - cur - 1) * sizeof(Contacto));
					n_libreta--;
					libreta_guarda();
				}
			}
			if (k == 114) {
				char r[260] = "";

				if (elige_archivo("Importar contactos (.vcf)", r, sizeof r, 0, "VCF")) {
					int m = libreta_importa(r);
					char t[200];

					if (m >= 0)
						snprintf(t, sizeof t, "%d contactos nuevos (de %s).", m, r);
					else
						snprintf(t, sizeof t, "No se ha podido leer %s.", r);
					aviso("Importar", t);
					so_apunta("libreta: importados %d de %s", m, r);
				}
			}
			if (k == 115) {
				char r[260] = "LIBRETA.VCF";

				if (elige_archivo("Exportar la libreta (.vcf)", r, sizeof r, 1, "VCF")) {
					int m = libreta_exporta(r);
					char t[300];

					if (m >= 0)
						snprintf(t, sizeof t, "%d contactos en %s.", m, r);
					else
						snprintf(t, sizeof t, "No se ha podido escribir %s.", r);
					aviso("Exportar", t);
					so_apunta("libreta: exportados %d a %s", m, r);
				}
			}
		}
	}
}

/* =============================================================================
 *  Los ajustes
 * ============================================================================= */
void ventana_ajustes(void)
{
	for (;;) {
		char t[1200];
		int e;

		snprintf(t, sizeof t,
			 "Ense\xc3\xb1" "ar el HTML de los mensajes (sin scripts): %s\n"
			 "La lista, por hilos (conversaciones): %s\n"
			 "Avisar del correo nuevo (abajo a la derecha): %s\n"
			 "Mirar si hay correo cada %u minutos (si el servidor no avisa solo)\n"
			 "Lo m\xc3\xa1s grande que se baja: %u MB\n"
			 "Lo m\xc3\xa1s que se guarda de cada carpeta: %s\n"
			 "La firma de esta cuenta...\n"
			 "Cerrar",
			 ajustes.html ? "s\xc3\xad" : "no", ajustes.hilos ? "s\xc3\xad" : "no", ajustes.avisar ? "s\xc3\xad" : "no",
			 (unsigned)ajustes.cada_min, (unsigned)(ajustes.max_mensaje_kb / 1024),
			 ajustes.max_almacen_mb ? "" : "sin tope");
		if (ajustes.max_almacen_mb) {
			char *p = strstr(t, ": \nLa firma");

			if (p) {
				char resto[200];

				snprintf(resto, sizeof resto, "%s", p + 2);
				snprintf(p, sizeof t - (size_t)(p - t), ": %u MB\n%s", (unsigned)ajustes.max_almacen_mb, resto);
			}
		}
		e = elige("Ajustes", t, 0);
		if (e < 0 || e == 7)
			break;
		if (e == 0)
			ajustes.html = !ajustes.html;
		else if (e == 1)
			ajustes.hilos = !ajustes.hilos;
		else if (e == 2)
			ajustes.avisar = !ajustes.avisar;
		else if (e >= 3 && e <= 5) {
			char v[32];
			u32 n = 0, i;

			snprintf(v, sizeof v, "%u", (unsigned)(e == 3 ? ajustes.cada_min : e == 4 ? ajustes.max_mensaje_kb / 1024 : ajustes.max_almacen_mb));
			if (pide_texto("Ajustes", e == 3 ? "Minutos:" : e == 4 ? "MB (de 1 a 200):" : "MB (0: sin tope):", v, sizeof v, 0)) {
				for (i = 0; v[i] >= '0' && v[i] <= '9' && n < 100000; i++)
					n = n * 10 + (u32)(v[i] - '0');
				if (e == 3)
					ajustes.cada_min = n < 1 ? 1 : n;
				else if (e == 4)
					ajustes.max_mensaje_kb = (n < 1 ? 1 : n > 200 ? 200 : n) * 1024;
				else
					ajustes.max_almacen_mb = n;
			}
		} else if (e == 6 && ses) {
			char v[600];
			u32 i, k = 0;

			for (i = 0; ses->c->firma[i] && k + 3 < sizeof v; i++)
				v[k++] = ses->c->firma[i] == '\n' ? '|' : ses->c->firma[i];
			v[k] = 0;
			if (pide_texto("La firma", "La firma (para cambiar de rengl\xc3\xb3n, una barra |):", v, sizeof v, 0)) {
				for (i = 0; v[i]; i++)
					if (v[i] == '|')
						v[i] = '\n';
				snprintf(ses->c->firma, sizeof ses->c->firma, "%s", v);
				cuentas_guarda();
			}
		}
		ajustes_guarda();
	}
}
