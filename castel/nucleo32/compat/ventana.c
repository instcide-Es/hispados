/* ventana.c - (COMPAT) la ventana del Asistente de compatibilidad: arriba
 * el programa o la carpeta elegidos («Elegir...» y «Comprobar»), a la
 * izquierda lo comprobado de este usuario (cada uno con «Está todo» o
 * «Faltan N», con su marca: no solo el color) y a la derecha lo que falta,
 * agrupado por DLL y explicado (vista.c); abajo «Enviar informe a
 * Hispa-DOS...» (con el cuadro que enseña exactamente lo que va),
 * «Componentes de Microsoft...» y «Olvidar». Los cuadros son los de la
 * oficina (oficina/comun/ui.h). Y la ventanita del aviso, abajo a la
 * derecha (ventana_aviso), como la de las actualizaciones.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "compat.h"
#include "plat.h"
#include "vista.h"
#include "ui.h"
#include "lienzo.h"
#include "texto.h"

extern void (*comprueba_espera)(void *u);
extern void *comprueba_u;

enum { B_NADA, B_ELEGIR, B_COMPROBAR, B_ENVIAR, B_COMPONENTES, B_OLVIDAR, B_N };
#define MAX_LIN 4000
#define MAX_CTL 16

typedef struct { int clase, x, negrita; u32 tinta; char t[240]; } linea;

static struct {
	int v, an, al, vivo;
	lienzo l;
	/* lo comprobado */
	c_programa *progs;
	int n_progs, elegido, prog_desp;
	/* lo que se ve */
	char ruta[RUTA_MAX], nombre[64];
	c_informe inf;
	int hay_inf;
	vista vis;
	linea *lin;
	int n_lin, desp;
	ui_desp barra;
	int arrastra_barra, desp0, y0;
	char estado[240], aviso[400];
	int clase_aviso;		/* 0 nada; 1 un fallo; 2 novedades */
	/* lo que está ocupado */
	int ocupado;
	char ocupado_que[120];
	u32 hecho, total;
	/* los botones */
	struct { int x, y, an, al, apagado; const char *t; } b[B_N];
	int pulsado, encima;
	/* las zonas */
	int lx, ly, lan, lal;		/* la lista de la izquierda */
	int rx, ry, ran, ral;		/* el resultado */
	/* el cuadro abierto */
	ui_cuadro q;
	ui_control c[MAX_CTL];
	int hay_cuadro;
	void (*al_cerrar)(int id);
	void (*tras_suceso)(void);
	char *opciones;		/* (las de la lista del cuadro: de so_pide) */
	char texto_cuadro[1200];
} V;

static void pinta(void);
static void comprobar(const char *ruta);

/* ---- utilidades ---- */
static int alto_u(void) { return lz_ui_alto(); }

/* parte «t» en renglones que quepan en «an» puntos (a las palabras) */
static void envuelve(const char *t, int an, int negrita, void (*pon)(void *u, const char *s), void *u)
{
	char r[240];
	u32 n = 0;
	const char *p = t;

	while (*p) {
		const char *q = p;
		char prueba[240];
		u32 m;

		/* la palabra siguiente (con su espacio de delante) */
		while (*q == ' ')
			q++;
		while (*q && *q != ' ')
			q++;
		m = (u32)(q - p);
		if (n + m >= sizeof r - 1)
			m = sizeof r - 1 - n;
		memcpy(prueba, r, n);
		memcpy(prueba + n, p, m);
		prueba[n + m] = 0;
		if (n && (negrita ? lz_ui_ancho_negrita(prueba) : lz_ui_ancho(prueba)) > an) {
			r[n] = 0;
			pon(u, r);
			while (*p == ' ')
				p++;
			n = 0;
			continue;
		}
		memcpy(r + n, p, m);
		n += m;
		p += m;
		if (!m)
			break;
	}
	r[n] = 0;
	if (n)
		pon(u, r);
}

typedef struct { int clase, x, negrita; u32 tinta; } pon_lin;
static void pon_linea(void *u, const char *s)
{
	pon_lin *p = u;
	linea *l;

	if (V.n_lin >= MAX_LIN)
		return;
	l = &V.lin[V.n_lin++];
	l->clase = p->clase;
	l->x = p->x;
	l->negrita = p->negrita;
	l->tinta = p->tinta;
	copia(l->t, s, sizeof l->t);
	/* (los que siguen de un mismo renglón no llevan otra vez la marca) */
	if (p->clase == R_BIEN || p->clase == R_MAL)
		p->clase = R_TITULO;
}

/* los renglones de la vista, a lo ancho de la zona */
static void rehaz_lineas(void)
{
	int k, an = V.ran - UI_DESP - 24;

	if (!V.lin)
		V.lin = so_pide(sizeof *V.lin * MAX_LIN);
	V.n_lin = 0;
	if (!V.lin)
		return;
	for (k = 0; k < V.vis.n; k++) {
		const renglon *r = &V.vis.r[k];
		pon_lin p;

		p.clase = r->clase;
		p.x = 10 + r->sangria * 18 + ((r->clase == R_BIEN || r->clase == R_MAL) ? 26 : 0);
		p.negrita = r->clase == R_BIEN || r->clase == R_MAL || r->clase == R_APARTADO || r->clase == R_DLL || r->clase == R_TITULO;
		p.tinta = r->clase == R_GRIS ? 0x5A6472 : UI_TINTA;
		if (r->clase == R_APARTADO && V.n_lin) {
			pon_lin hueco = { R_GRIS, 10, 0, UI_TINTA };

			pon_linea(&hueco, " ");
		}
		envuelve(r->t, an - p.x, p.negrita, pon_linea, &p);
	}
	if (V.desp > V.n_lin)
		V.desp = 0;
}

static void pon_vista(void)
{
	vista_suelta(&V.vis);
	if (V.hay_inf)
		vista_haz(&V.vis, &V.inf, V.nombre);
	rehaz_lineas();
	V.desp = 0;
}

static void lee_lista(void)
{
	so_suelta(V.progs);
	V.progs = NULL;
	V.n_progs = 0;
	hist_lee(&V.progs, &V.n_progs);
}

/* ---- las medidas ---- */
static void medidas(int an, int al)
{
	int H = ui_alto_control(), U = alto_u(), y, ab, x;

	if (an < 520)
		an = 520;
	if (al < 360)
		al = 360;
	if (an != V.an || al != V.al || !V.l.p) {
		lz_suelta(&V.l);
		lz_nuevo(&V.l, an, al);
	}
	V.an = an;
	V.al = al;
	y = 10;
	V.b[B_COMPROBAR].an = 120;
	V.b[B_ELEGIR].an = 110;
	V.b[B_COMPROBAR].x = an - 10 - 120;
	V.b[B_ELEGIR].x = V.b[B_COMPROBAR].x - 8 - 110;
	V.b[B_COMPROBAR].y = V.b[B_ELEGIR].y = y;
	V.b[B_COMPROBAR].al = V.b[B_ELEGIR].al = H;
	V.b[B_COMPROBAR].t = "&Comprobar";
	V.b[B_ELEGIR].t = "&Elegir...";
	y += H + 10;
	if (V.clase_aviso)
		y += 2 * U + 18;
	ab = al - ui_alto_estado() - H - 14;
	V.lx = 10;
	V.ly = y;
	V.lan = an < 700 ? 200 : 250;
	V.lal = ab - 8 - y;
	V.rx = V.lx + V.lan + 8;
	V.ry = y;
	V.ran = an - 10 - V.rx;
	V.ral = V.lal;
	x = 10;
	V.b[B_ENVIAR].t = "En&viar informe a Hispa-DOS...";
	V.b[B_COMPONENTES].t = "Com&ponentes de Microsoft...";
	V.b[B_OLVIDAR].t = "&Olvidar este";
	{
		int k;

		for (k = B_ENVIAR; k <= B_OLVIDAR; k++) {
			V.b[k].an = ui_ancho_amp(V.b[k].t) + 28;
			V.b[k].x = x;
			V.b[k].y = ab;
			V.b[k].al = H;
			x += V.b[k].an + 8;
		}
	}
	V.barra.x = V.rx + V.ran - UI_DESP - 1;
	V.barra.y = V.ry + 1;
	V.barra.an = UI_DESP;
	V.barra.al = V.ral - 2;
	rehaz_lineas();
}

static void apaga_botones(void)
{
	V.b[B_COMPROBAR].apagado = !V.ruta[0] || V.ocupado;
	V.b[B_ELEGIR].apagado = V.ocupado;
	V.b[B_ENVIAR].apagado = !V.hay_inf || V.ocupado;
	V.b[B_OLVIDAR].apagado = V.elegido < 0 || V.ocupado;
	V.b[B_COMPONENTES].apagado = V.ocupado;
}

/* ---- pintar ---- */
static void boton(int k)
{
	lienzo *l = &V.l;
	int x = V.b[k].x, y = V.b[k].y, an = V.b[k].an, al = V.b[k].al, ap = V.b[k].apagado;
	int principal = k == B_COMPROBAR, pul = V.pulsado == k && !ap;
	u32 fondo = ap ? UI_FONDO : principal ? (pul ? 0x1F55B0 : 0x2F6FDB) : pul ? UI_PULSADO : V.encima == k ? UI_ENCIMA : UI_BLANCO;
	int w = ui_ancho_amp(V.b[k].t);

	lz_redondeado(l, x, y, an, al, 4, fondo);
	lz_marco_redondeado(l, x, y, an, al, 4, principal ? 0x1E4A99 : UI_RAYA_FUERTE);
	ui_texto_amp(l, x + (an - w) / 2 + (pul ? 1 : 0), y + (al - alto_u()) / 2 + lz_ui_base() + (pul ? 1 : 0), V.b[k].t,
		     ap ? UI_TINTA_GRIS : principal ? UI_BLANCO : UI_TINTA, principal);
}

/* la marca de «está todo» (un círculo con ✓) y la de «falta» (un triángulo con !) */
static void marca(int x, int y, int lado, int bien)
{
	lienzo *l = &V.l;

	if (bien) {
		lz_circulo(l, (x + lado / 2) * 16, (y + lado / 2) * 16, lado * 8, 0x1F7A3A);
		lz_linea(l, (x + lado / 4) * 16, (y + lado / 2) * 16, (x + lado * 7 / 16) * 16, (y + lado * 11 / 16) * 16, 36, UI_BLANCO);
		lz_linea(l, (x + lado * 7 / 16) * 16, (y + lado * 11 / 16) * 16, (x + lado * 3 / 4) * 16, (y + lado / 4) * 16, 36, UI_BLANCO);
	} else {
		int xy[6];

		xy[0] = (x + lado / 2) * 16, xy[1] = y * 16;
		xy[2] = (x + lado) * 16, xy[3] = (y + lado) * 16;
		xy[4] = x * 16, xy[5] = (y + lado) * 16;
		lz_poligono(l, xy, 3, 0xB45309);
		lz_rect(l, x + lado / 2 - 1, y + lado / 3, 3, lado / 3, UI_BLANCO);
		lz_rect(l, x + lado / 2 - 1, y + lado * 3 / 4, 3, 3, UI_BLANCO);
	}
}

static int alto_prog(void) { return 2 * alto_u() + 10; }

static void pinta_lista(void)
{
	lienzo *l = &V.l;
	int k, y, U = alto_u(), h = alto_prog(), vis = (V.lal - 2) / h;

	lz_rect(l, V.lx, V.ly, V.lan, V.lal, UI_BLANCO);
	lz_marco(l, V.lx, V.ly, V.lan, V.lal, UI_RAYA_FUERTE);
	if (!V.n_progs) {
		lz_recorte(l, V.lx + 1, V.ly + 1, V.lan - 2, V.lal - 2);
		lz_ui(l, V.lx + 10, V.ly + 10 + lz_ui_base(), "Aún no ha comprobado", UI_TINTA_GRIS);
		lz_ui(l, V.lx + 10, V.ly + 10 + U + lz_ui_base(), "ningún programa.", UI_TINTA_GRIS);
		lz_sin_recorte(l);
		return;
	}
	if (V.elegido >= 0 && V.elegido < V.prog_desp)
		V.prog_desp = V.elegido;
	if (V.elegido >= V.prog_desp + vis)
		V.prog_desp = V.elegido - vis + 1;
	if (V.prog_desp < 0)
		V.prog_desp = 0;
	lz_recorte(l, V.lx + 1, V.ly + 1, V.lan - 2, V.lal - 2);
	for (k = V.prog_desp, y = V.ly + 1; k < V.n_progs && y < V.ly + V.lal; k++, y += h) {
		c_programa *p = &V.progs[k];
		char t[120];
		int sel = k == V.elegido, bien = p->importantes == 0;

		if (sel) {
			lz_rect(l, V.lx + 1, y, V.lan - 2, h, UI_PULSADO);
			lz_rect(l, V.lx + 1, y, 4, h, UI_ACENTO);
			lz_marco(l, V.lx + 1, y, V.lan - 2, h, UI_ACENTO);
		}
		marca(V.lx + 10, y + 6, U - 2, bien);
		lz_ui_negrita(l, V.lx + 14 + U, y + 4 + lz_ui_base(), p->nombre, UI_TINTA);
		if (!p->faltan)
			copia(t, "Está todo", sizeof t);
		else if (!p->importantes)
			fmt(t, sizeof t, "Lo que hace falta, está");
		else
			fmt(t, sizeof t, "%s %u", p->importantes == 1 ? "Falta" : "Faltan", p->importantes);
		lz_ui(l, V.lx + 14 + U, y + 4 + U + lz_ui_base(), t, 0x3A4555);
		lz_raya_h(l, V.lx + 1, y + h - 1, V.lan - 2, UI_RAYA);
	}
	lz_sin_recorte(l);
}

static void pinta_resultado(void)
{
	lienzo *l = &V.l;
	int U = alto_u(), y, k, vis;

	lz_rect(l, V.rx, V.ry, V.ran, V.ral, UI_BLANCO);
	lz_marco(l, V.rx, V.ry, V.ran, V.ral, UI_RAYA_FUERTE);
	lz_recorte(l, V.rx + 1, V.ry + 1, V.ran - UI_DESP - 2, V.ral - 2);
	if (!V.n_lin) {
		static const char *const AYUDA[] = {
			"Elija un programa de Windows (un .EXE) o la carpeta donde está",
			"(con «Elegir...») y pulse «Comprobar»: el Asistente mira, sin",
			"ponerlo en marcha, lo que pide a Windows y dice lo que aún",
			"falta en Hispa-Pentium, agrupado y explicado.",
			"",
			"También se abre desde Mi equipo: botón derecho en un .EXE >",
			"«Comprobar compatibilidad».",
		};
		for (k = 0; k < (int)(sizeof AYUDA / sizeof AYUDA[0]); k++)
			lz_ui(l, V.rx + 14, V.ry + 14 + k * U + lz_ui_base(), AYUDA[k], 0x3A4555);
	}
	vis = (V.ral - 16) / U;
	for (k = V.desp, y = V.ry + 8; k < V.n_lin && k < V.desp + vis; k++, y += U) {
		const linea *r = &V.lin[k];
		int bx = V.rx + r->x;

		if (r->clase == R_BIEN || r->clase == R_MAL)
			marca(V.rx + 10, y + 2, U - 4, r->clase == R_BIEN);
		if (r->clase == R_APARTADO) {
			lz_rect(l, V.rx + 4, y, V.ran - UI_DESP - 10, U, UI_FONDO);
			lz_raya_h(l, V.rx + 4, y + U - 1, V.ran - UI_DESP - 10, UI_RAYA_FUERTE);
		}
		if (r->clase == R_FUNC)
			lz_ui(l, bx - 10, y + lz_ui_base(), "·", UI_TINTA);
		if (r->negrita)
			lz_ui_negrita(l, bx, y + lz_ui_base(), r->t, r->tinta);
		else
			lz_ui(l, bx, y + lz_ui_base(), r->t, r->tinta);
	}
	lz_sin_recorte(l);
	V.barra.total = V.n_lin;
	V.barra.visto = vis;
	V.barra.pos = V.desp;
	ui_desp_pinta(l, &V.barra, 0);
}

static void pinta(void)
{
	lienzo *l = &V.l;
	int U = alto_u(), H = ui_alto_control(), k;
	char t[RUTA_MAX + 40];

	if (!l->p)
		return;
	apaga_botones();
	lz_rect(l, 0, 0, V.an, V.al, UI_FONDO2);
	/* arriba: lo elegido */
	lz_ui_negrita(l, 10, 10 + (H - U) / 2 + lz_ui_base(), "Programa o carpeta:", UI_TINTA);
	{
		int x0 = 10 + lz_ui_ancho_negrita("Programa o carpeta:") + 10, an = V.b[B_ELEGIR].x - 10 - x0;

		lz_rect(l, x0, 10, an, H, UI_BLANCO);
		lz_marco(l, x0, 10, an, H, UI_RAYA_FUERTE);
		lz_recorte(l, x0 + 2, 10, an - 4, H);
		lz_ui(l, x0 + 6, 10 + (H - U) / 2 + lz_ui_base(), V.ruta[0] ? V.ruta : "(ninguno: pulse «Elegir...»)", V.ruta[0] ? UI_TINTA : UI_TINTA_GRIS);
		lz_sin_recorte(l);
	}
	boton(B_ELEGIR);
	boton(B_COMPROBAR);
	/* el aviso de arriba (un fallo, o las novedades) */
	if (V.clase_aviso) {
		int y = 10 + H + 8, h = 2 * U + 12;

		lz_rect(l, 10, y, V.an - 20, h, V.clase_aviso == 1 ? UI_AVISO : 0xE7F3EA);
		lz_marco(l, 10, y, V.an - 20, h, V.clase_aviso == 1 ? 0xB45309 : 0x1F7A3A);
		marca(18, y + 6, U, V.clase_aviso == 2);
		lz_recorte(l, 18 + U + 8, y, V.an - 50 - U, h);
		{
			char a[400], b2[400];
			const char *n = strchr(V.aviso, '\n');

			if (n) {
				copia(a, V.aviso, (u32)(n - V.aviso) + 1 < sizeof a ? (u32)(n - V.aviso) + 1 : sizeof a);
				copia(b2, n + 1, sizeof b2);
			} else {
				copia(a, V.aviso, sizeof a);
				b2[0] = 0;
			}
			lz_ui_negrita(l, 18 + U + 8, y + 5 + lz_ui_base(), a, UI_TINTA);
			lz_ui(l, 18 + U + 8, y + 5 + U + lz_ui_base(), b2, UI_TINTA);
		}
		lz_sin_recorte(l);
	}
	pinta_lista();
	pinta_resultado();
	for (k = B_ENVIAR; k <= B_OLVIDAR; k++)
		boton(k);
	/* la barra de estado (y el avance, si se está haciendo algo) */
	if (V.ocupado) {
		if (V.total)
			fmt(t, sizeof t, "%s... %u%%", V.ocupado_que, (u32)((u64)V.hecho * 100 / V.total));
		else
			fmt(t, sizeof t, "%s...", V.ocupado_que);
	} else
		copia(t, V.estado[0] ? V.estado : "Listo.", sizeof t);
	{
		char e[RUTA_MAX + 80];
		char w[24];

		version_win32(w, sizeof w);
		fmt(e, sizeof e, "%s|win32 %s", t, w);
		ui_estado(l, 0, V.al - ui_alto_estado(), V.an, e);
	}
	if (V.ocupado && V.total) {
		int bx = V.an - 230, by = V.al - ui_alto_estado() + 4, ban = 120, bal = ui_alto_estado() - 8;

		lz_rect(l, bx, by, ban, bal, UI_BLANCO);
		lz_rect(l, bx, by, (int)((u64)ban * V.hecho / V.total), bal, UI_ACENTO);
		lz_marco(l, bx, by, ban, bal, UI_TINTA);
	}
	if (V.hay_cuadro)
		ui_cuadro_pinta(l, &V.q);
	so_mapa(V.v, 0, 0, V.an, V.al, l->p, l->paso);
}

/* mientras se trabaja: pintar y atender lo justo (el tamaño) */
static void ocupado(const char *que, u32 hecho, u32 total)
{
	so_suceso s;
	static u32 ult;

	V.ocupado = 1;
	copia(V.ocupado_que, que, sizeof V.ocupado_que);
	V.hecho = hecho;
	V.total = total;
	if (so_tics() - ult < so_hz() / 5 && hecho && hecho != total)
		return;
	ult = so_tics();
	while (so_lee_suceso(V.v, &s, 0))
		if (s.clase == SO_TAMANO)
			medidas(s.x, s.y);
	pinta();
}
static void espera_comprueba(void *u)
{
	(void)u;
	ocupado("Comprobando con WIN32", 0, 0);
}
static void avance_red(void *u, const char *que, u32 hecho, u32 total)
{
	(void)u;
	ocupado(que, hecho, total);
}

/* ---- los cuadros ---- */
static void cierra_cuadro(void)
{
	V.hay_cuadro = 0;
	V.tras_suceso = NULL;
	so_suelta(V.opciones);
	V.opciones = NULL;
}

static ui_control *ctl(int tipo, int id, int x, int y, int an, int al, const char *rot)
{
	ui_control *c;

	if (V.q.n >= MAX_CTL)
		return &V.c[MAX_CTL - 1];
	c = &V.c[V.q.n++];
	memset(c, 0, sizeof *c);
	c->tipo = tipo;
	c->id = id;
	c->x = x;
	c->y = y;
	c->an = an;
	c->al = al;
	c->rotulo = rot;
	return c;
}

static void abre_cuadro(const char *titulo, int an, int al, void (*f)(int id))
{
	int i;

	V.q.titulo = titulo;
	V.q.an = an;
	V.q.al = al;
	V.q.c = V.c;
	V.q.cerrado = -9999;
	V.q.pulsado = -1;
	V.q.menu_de = -1;
	V.q.arrastra = -1;
	V.q.foco = 0;
	for (i = 0; i < V.q.n; i++)
		if (!V.c[i].apagado && V.c[i].tipo != UC_ROTULO && V.c[i].tipo != UC_TEXTO && V.c[i].tipo != UC_MUESTRA) {
			V.q.foco = i;
			break;
		}
	for (i = 0; i < V.q.n; i++)
		if (V.c[i].tipo == UC_CAMPO) {
			V.c[i].cursor = V.c[i].ancla = (int)utf8_cuenta(V.c[i].valor, largo(V.c[i].valor));
		}
	V.al_cerrar = f;
	V.hay_cuadro = 1;
	dice("compat: cuadro «%s»", titulo);
}

static void nuevo_cuadro(void)
{
	memset(&V.q, 0, sizeof V.q);
	memset(V.c, 0, sizeof V.c);
	V.q.n = 0;
}

/* un texto en renglones (con \n) que quepan en «an» */
static void a_renglones(const char *t, int an, char *d, u32 cabe)
{
	char x[600];
	const char *p = t;

	d[0] = 0;
	while (*p) {
		const char *e = strchr(p, '\n');
		u32 n = e ? (u32)(e - p) : largo(p);
		struct { char *d; u32 cabe; } a = { d, cabe };

		copia(x, p, n + 1 < sizeof x ? n + 1 : sizeof x);
		if (!x[0])
			anade(d, "\n", cabe);
		else {
			/* (envuelve llama a esto por renglón) */
			extern void compat_anade_renglon(void *u, const char *s);

			envuelve(x, an, 0, compat_anade_renglon, &a);
		}
		p += n;
		if (*p == '\n')
			p++;
	}
}
void compat_anade_renglon(void *u, const char *s)
{
	struct { char *d; u32 cabe; } *a = u;

	anade(a->d, s, a->cabe);
	anade(a->d, "\n", a->cabe);
}

static void al_aviso(int id) { (void)id; }

/* lo alto de una lista de un cuadro: hasta «filas» renglones, sin que el
 * cuadro (con «fijo» de lo demás y su título) se salga de la ventana */
static int alto_lista(int filas, int fijo)
{
	int U = alto_u(), fila = U + 2, max = V.al - (U + 10) - fijo - 12, h = filas * fila + 4;

	if (h > max)
		h = (max - 4) / fila * fila + 4;
	if (h < 3 * fila + 4)
		h = 3 * fila + 4;
	return h;
}

static void dlg_aviso(const char *titulo, const char *t)
{
	int an = 520, U = alto_u(), H = ui_alto_control(), n = 0;
	const char *p;

	nuevo_cuadro();
	a_renglones(t, an - 40, V.texto_cuadro, sizeof V.texto_cuadro);
	for (p = V.texto_cuadro; *p; p++)
		if (*p == '\n')
			n++;
	ctl(UC_TEXTO, 1, 20, 14, an - 40, n * U, V.texto_cuadro);
	ctl(UC_BOTON, 2, an - 20 - 110, 24 + n * U, 110, H, "Aceptar")->principal = 1;
	abre_cuadro(titulo, an, 24 + n * U + H + 14, al_aviso);
}

/* ---- Elegir ---- */
#define MAX_ENTRADAS 400
static char eleg_carpeta[RUTA_MAX];
static char (*eleg_nombres)[64];
static u8 *eleg_es_carpeta;
static int eleg_n;
static void abre_elegir(void);

static int compara_nombre(const char *a, const char *b)
{
	for (;; a++, b++) {
		u32 x = minuscula((u8)*a), y = minuscula((u8)*b);

		if (x != y || !x)
			return (int)x - (int)y;
	}
}

static void lee_eleccion(void)
{
	void *h;
	so_entrada e;

	if (!eleg_nombres) {
		eleg_nombres = so_pide(64 * MAX_ENTRADAS);
		eleg_es_carpeta = so_pide(MAX_ENTRADAS);
	}
	eleg_n = 0;
	if (!eleg_nombres || !eleg_es_carpeta)
		return;
	if (!eleg_carpeta[0]) {
		/* las unidades */
		char u;

		for (u = 'A'; u <= 'Z' && eleg_n < MAX_ENTRADAS; u++) {
			char r[4] = { u, ':', '\\', 0 };

			if (u != 'A' && u != 'B' && so_existe(r) == 2) {
				copia(eleg_nombres[eleg_n], r, 64);
				eleg_es_carpeta[eleg_n++] = 2;
			}
		}
		return;
	}
	h = so_abre_carpeta(eleg_carpeta);
	if (!h)
		return;
	while (so_lee_carpeta(h, &e) == 1 && eleg_n < MAX_ENTRADAS) {
		char ext[8];
		int k;

		if (e.nombre[0] == '.')
			continue;
		extension(e.nombre, ext);
		if (!e.carpeta && !iguales(ext, "EXE"))
			continue;
		/* (en orden: las carpetas primero) */
		for (k = eleg_n; k > 0; k--) {
			int c = eleg_es_carpeta[k - 1] != (u8)e.carpeta ? (e.carpeta ? -1 : 1) : compara_nombre(e.nombre, eleg_nombres[k - 1]);

			if (c >= 0)
				break;
			memcpy(eleg_nombres[k], eleg_nombres[k - 1], 64);
			eleg_es_carpeta[k] = eleg_es_carpeta[k - 1];
		}
		copia(eleg_nombres[k], e.nombre, 64);
		eleg_es_carpeta[k] = (u8)e.carpeta;
		eleg_n++;
	}
	so_cierra_carpeta(h);
}

static void sube(char *r)
{
	u32 n = largo(r);

	while (n > 0 && (r[n - 1] == '\\' || r[n - 1] == '/'))
		n--;
	while (n > 0 && r[n - 1] != '\\' && r[n - 1] != '/')
		n--;
	r[n] = 0;
	/* («C:\» se queda «C:\»; de ahí, a las unidades) */
	if (n == 3 && r[1] == ':')
		return;
	if (n > 1 && (r[n - 1] == '\\' || r[n - 1] == '/') && !(n == 3 && r[1] == ':'))
		r[n - 1] = 0;
	if (n == 2 && r[1] == ':')
		r[0] = 0;
}

enum { E_CAMPO = 1, E_LISTA, E_ABRIR, E_CARPETA, E_CANCELAR };

static void al_elegir(int id)
{
	ui_control *campo = ui_control_id(&V.q, E_CAMPO), *lista = ui_control_id(&V.q, E_LISTA);
	char r[RUTA_MAX];
	int k = lista ? lista->elegida : -1, sube_primero = eleg_carpeta[0] ? 1 : 0;

	if (id == E_CANCELAR || id < 0)
		return;
	if (id == E_CARPETA) {
		if (eleg_carpeta[0])
			comprobar(eleg_carpeta);
		return;
	}
	/* Abrir: lo escrito (si se ha cambiado) o lo elegido de la lista */
	if (campo && campo->valor[0] && !iguales(campo->valor, eleg_carpeta)) {
		int e = so_existe(campo->valor);

		if (e == 2) {
			copia(eleg_carpeta, campo->valor, sizeof eleg_carpeta);
			abre_elegir();
			return;
		}
		if (e == 1) {
			comprobar(campo->valor);
			return;
		}
		dlg_aviso("Elegir", "No se encuentra eso. Escriba la ruta entera de un programa (.EXE) o de una carpeta (C:\\JUEGOS\\...).");
		return;
	}
	if (sube_primero && k == 0) {
		sube(eleg_carpeta);
		abre_elegir();
		return;
	}
	k -= sube_primero;
	if (k < 0 || k >= eleg_n)
		return;
	if (eleg_es_carpeta[k] == 2) {
		copia(eleg_carpeta, eleg_nombres[k], sizeof eleg_carpeta);
		abre_elegir();
		return;
	}
	une_ruta(r, sizeof r, eleg_carpeta, eleg_nombres[k]);
	if (eleg_es_carpeta[k]) {
		copia(eleg_carpeta, r, sizeof eleg_carpeta);
		abre_elegir();
		return;
	}
	comprobar(r);
}

static void abre_elegir(void)
{
	int an = 600, H = ui_alto_control(), U = alto_u(), al_l = alto_lista(12, 48 + 2 * H + U), k;
	bufer b;
	ui_control *c;

	lee_eleccion();
	memset(&b, 0, sizeof b);
	if (eleg_carpeta[0])
		buf_txt(&b, "[Subir] ..");
	for (k = 0; k < eleg_n; k++) {
		if (b.n)
			buf_c(&b, '|');
		if (eleg_es_carpeta[k] == 2)
			buf_fmt(&b, "[Unidad] %s", eleg_nombres[k]);
		else if (eleg_es_carpeta[k])
			buf_fmt(&b, "[Carpeta] %s", eleg_nombres[k]);
		else
			buf_txt(&b, eleg_nombres[k]);
	}
	buf_c(&b, 0);
	nuevo_cuadro();
	V.opciones = (char *)b.d;
	ctl(UC_ROTULO, 0, 16, 12, 90, 0, "Está en:");
	c = ctl(UC_CAMPO, E_CAMPO, 100, 12, an - 116, 0, NULL);
	copia(c->valor, eleg_carpeta, sizeof c->valor);
	c = ctl(UC_LISTA, E_LISTA, 16, 20 + H, an - 32, al_l, NULL);
	c->opciones = V.opciones ? V.opciones : "";
	c->elegida = eleg_carpeta[0] && eleg_n ? 1 : 0;
	ctl(UC_TEXTO, 0, 16, 28 + H + al_l, an - 32, U, "Los .EXE y las carpetas. «Abrir» entra en una carpeta o elige el programa.");
	ctl(UC_BOTON, E_ABRIR, 16, 36 + H + al_l + U, 110, H, "&Abrir")->principal = 1;
	ctl(UC_BOTON, E_CARPETA, 134, 36 + H + al_l + U, 230, H, "Comprobar &esta carpeta")->apagado = !eleg_carpeta[0];
	ctl(UC_BOTON, E_CANCELAR, an - 16 - 110, 36 + H + al_l + U, 110, H, "Cancelar")->cancela = 1;
	abre_cuadro("Elegir un programa o una carpeta", an, 48 + 2 * H + al_l + U, al_elegir);
	/* (el foco, en la lista) */
	V.q.foco = 2;
}

/* ---- comprobar ---- */
static void comprobar(const char *ruta)
{
	char e[300], w[24];

	copia(V.ruta, ruta, sizeof V.ruta);
	hist_nombre(ruta, V.nombre, sizeof V.nombre);
	if (V.hay_inf)
		inf_suelta(&V.inf);
	V.hay_inf = 0;
	vista_suelta(&V.vis);
	V.n_lin = 0;
	comprueba_espera = espera_comprueba;
	comprueba_u = NULL;
	ocupado("Comprobando con WIN32", 0, 0);
	if (comprueba(ruta, &V.inf, 0, e, sizeof e)) {
		V.ocupado = 0;
		fmt(V.estado, sizeof V.estado, "No se ha podido comprobar: %s", e);
		dlg_aviso("Comprobar", e);
		return;
	}
	V.ocupado = 0;
	V.hay_inf = 1;
	version_win32(w, sizeof w);
	hist_apunta(ruta, V.nombre, w, &V.inf);
	lee_lista();
	{
		char id[9];
		int k;

		hist_id(ruta, id);
		V.elegido = -1;
		for (k = 0; k < V.n_progs; k++)
			if (iguales(V.progs[k].id, id))
				V.elegido = k;
	}
	pon_vista();
	{
		u32 t = inf_faltan(&V.inf, 0), i = inf_faltan(&V.inf, 1);

		dice("compat: comprobado %s: faltan %u (%u importantes), %d archivos, win32 %s", ruta, t, i, V.inf.n_a, w);
		if (!t)
			fmt(V.estado, sizeof V.estado, "%s: está todo.", V.nombre);
		else
			fmt(V.estado, sizeof V.estado, "%s: %s %u (%u en total). Comprobado con win32 %s.", V.nombre, i == 1 ? "falta" : "faltan", i, t, w);
	}
}

/* ---- Enviar ---- */
enum { C_LISTA = 1, C_CASILLA, C_ENVIAR, C_NO };
static char *env_actual;

static void tras_consentimiento(void)
{
	ui_control *c = ui_control_id(&V.q, C_CASILLA), *b = ui_control_id(&V.q, C_ENVIAR);

	if (c && b)
		b->apagado = !c->marcado;
}

static void al_consentir(int id)
{
	ui_control *c = ui_control_id(&V.q, C_CASILLA);
	char pq[300], t[500];
	int r;

	if (id != C_ENVIAR || !c || !c->marcado || !env_actual) {
		so_suelta(env_actual);
		env_actual = NULL;
		if (id == C_NO || id < 0)
			copia(V.estado, "No se ha mandado nada.", sizeof V.estado);
		return;
	}
	ocupado("Mandando el informe", 0, 0);
	r = env_manda(env_actual, pq, sizeof pq);
	V.ocupado = 0;
	so_suelta(env_actual);
	env_actual = NULL;
	dice("compat: informe %s: %s", r ? "no mandado" : "mandado", pq);
	if (!r) {
		copia(V.estado, "Informe mandado.", sizeof V.estado);
		fmt(t, sizeof t, "Mandado. Gracias: así se sabe qué hace falta antes.\n(%s)", pq);
		dlg_aviso("Enviar informe a Hispa-DOS", t);
	} else {
		fmt(V.estado, sizeof V.estado, "No se ha mandado: %s", pq);
		fmt(t, sizeof t, "No se ha mandado: %s.%s", pq, r == 2 ? "\nPruebe otra vez cuando haya red." : "");
		dlg_aviso("Enviar informe a Hispa-DOS", t);
	}
}

static void abre_enviar(void)
{
	c_informe i;
	char e[300], w[24], h[24];
	bufer b;
	const char *p;
	int an = 720, H = ui_alto_control(), U = alto_u(), al_l = alto_lista(12, 18 + 3 * U + 8 + H + 8 + H + 14), y;
	ui_control *c;

	if (!V.ruta[0])
		return;
	/* (otra vez, con los tamaños y las sumas: lo que va) */
	comprueba_espera = espera_comprueba;
	ocupado("Midiendo los archivos y sus sumas", 0, 0);
	if (comprueba(V.ruta, &i, 1, e, sizeof e)) {
		V.ocupado = 0;
		dlg_aviso("Enviar informe a Hispa-DOS", e);
		return;
	}
	V.ocupado = 0;
	version_win32(w, sizeof w);
	version_hispa(h, sizeof h);
	so_suelta(env_actual);
	env_actual = env_texto(&i, w, h);
	inf_suelta(&i);
	if (!env_actual)
		return;
	/* los renglones de lo que va, para la lista (tal cual; el tabulador, como « · ») */
	memset(&b, 0, sizeof b);
	for (p = env_actual; *p; p++) {
		if (*p == '\n') {
			if (p[1])
				buf_c(&b, '|');
		} else if (*p == '\t')
			buf_txt(&b, "  ·  ");
		else if (*p != '|')
			buf_c(&b, (u8)*p);
	}
	buf_c(&b, 0);
	nuevo_cuadro();
	V.opciones = (char *)b.d;
	ctl(UC_TEXTO, 0, 16, 10, an - 32, 3 * U,
	    "Esto es exactamente lo que se manda, y nada más: el nombre de cada .EXE y .DLL\n"
	    "(sin sus carpetas), lo que mide, su suma SHA-256, lo que le falta y las versiones.\n"
	    "No va su nombre, ni sus carpetas, ni nada de lo que hay dentro de los archivos.");
	y = 18 + 3 * U;
	c = ctl(UC_LISTA, C_LISTA, 16, y, an - 32, al_l, NULL);
	c->opciones = V.opciones ? V.opciones : "";
	y += al_l + 8;
	ctl(UC_CASILLA, C_CASILLA, 16, y, an - 32, H, "He &leído lo que se manda y quiero mandarlo a Hispa-DOS");
	y += H + 8;
	c = ctl(UC_BOTON, C_ENVIAR, an - 16 - 120 - 8 - 160, y, 160, H, "&Enviar");
	c->principal = 1;
	c->apagado = 1;
	ctl(UC_BOTON, C_NO, an - 16 - 120, y, 120, H, "&No enviar")->cancela = 1;
	abre_cuadro("Enviar el informe a Hispa-DOS", an, y + H + 14, al_consentir);
	V.tras_suceso = tras_consentimiento;
	V.q.foco = 2;
}

/* ---- Componentes de Microsoft ---- */
enum { M_LISTA = 1, M_TEXTO, M_PONER, M_CERRAR, L_LISTA, L_ACEPTO, L_NO };
static c_redist *redists;
static int n_redists, redist_elegido;
static char redist_ruta[RUTA_MAX];
static char texto_redist[900];

static void tras_componentes(void)
{
	ui_control *l = ui_control_id(&V.q, M_LISTA);
	const c_redist *r;
	char t[900];
	int p;

	if (!l || l->elegida < 0 || l->elegida >= n_redists)
		return;
	r = &redists[l->elegida];
	p = red_puesto(r);
	fmt(t, sizeof t, "%s\nDLL: %s\nDe %s (%u KB). %s", r->para, r->dlls, r->url, r->tam / 1024,
	    p == 2 ? "Ya está puesto." : p == 1 ? "Está puesto a medias." : "Aún no está puesto.");
	a_renglones(t, 620, texto_redist, sizeof texto_redist);
}

static void abre_componentes(void);

static void al_licencia(int id)
{
	char e[300], t[500];
	int n;

	if (id != L_ACEPTO) {
		copia(V.estado, "No se ha puesto nada (la licencia no se ha aceptado).", sizeof V.estado);
		so_borra(redist_ruta);
		return;
	}
	ocupado("Sacando las DLL", 0, 0);
	n = red_saca(&redists[redist_elegido], redist_ruta, avance_red, NULL, e, sizeof e);
	V.ocupado = 0;
	dice("compat: componente %s: %d DLL puestas%s%s", redists[redist_elegido].id, n, n < 0 ? ": " : "", n < 0 ? e : "");
	if (n < 0) {
		dlg_aviso("Componentes de Microsoft", e);
		return;
	}
	fmt(t, sizeof t, "Puestas %d DLL de %s en \\WIN32.%s", n, redists[redist_elegido].nombre,
	    V.ruta[0] ? "\nSe vuelve a comprobar lo elegido." : "");
	fmt(V.estado, sizeof V.estado, "Puestas %d DLL de %s.", n, redists[redist_elegido].nombre);
	if (V.ruta[0]) {
		char r[RUTA_MAX];

		copia(r, V.ruta, sizeof r);
		comprobar(r);
	}
	dlg_aviso("Componentes de Microsoft", t);
}

static void al_componentes(int id)
{
	ui_control *l = ui_control_id(&V.q, M_LISTA);
	char e[300];
	char *lic;
	bufer b;
	const char *p;
	int an = 720, H = ui_alto_control(), U = alto_u(), al_l = alto_lista(14, 16 + 2 * U + 10 + H + 14), y;
	ui_control *c;

	if (id != M_PONER || !l || l->elegida < 0 || l->elegida >= n_redists)
		return;
	redist_elegido = l->elegida;
	ocupado("Bajando de Microsoft", 0, redists[redist_elegido].tam);
	if (red_baja(&redists[redist_elegido], redist_ruta, sizeof redist_ruta, avance_red, NULL, e, sizeof e)) {
		V.ocupado = 0;
		dlg_aviso("Componentes de Microsoft", e);
		return;
	}
	dice("compat: componente %s: bajado y comprobado (SHA-256)", redists[redist_elegido].id);
	ocupado("Leyendo la licencia", 0, 0);
	lic = red_licencia(&redists[redist_elegido], redist_ruta, e, sizeof e);
	V.ocupado = 0;
	if (!lic) {
		so_borra(redist_ruta);
		dlg_aviso("Componentes de Microsoft", e);
		return;
	}
	/* la licencia, en renglones que quepan, para la lista */
	memset(&b, 0, sizeof b);
	{
		char *r = so_pide(largo(lic) * 2 + 4096);

		if (r) {
			u32 cab = largo(lic) * 2 + 4096;
			const char *q = lic;

			r[0] = 0;
			while (*q) {
				const char *f = strchr(q, '\n');
				u32 n = f ? (u32)(f - q) : largo(q);
				char x[600];
				struct { char *d; u32 cabe; } a = { r, cab };

				copia(x, q, n + 1 < sizeof x ? n + 1 : sizeof x);
				if (x[0])
					envuelve(x, an - 70, 0, compat_anade_renglon, &a);
				else
					anade(r, " \n", cab);
				q += n;
				if (*q == '\n')
					q++;
			}
			for (p = r; *p; p++) {
				if (*p == '\n') {
					if (p[1])
						buf_c(&b, '|');
				} else if (*p != '|' && *p != '\t')
					buf_c(&b, (u8)*p);
				else
					buf_c(&b, ' ');
			}
			so_suelta(r);
		}
	}
	buf_c(&b, 0);
	so_suelta(lic);
	nuevo_cuadro();
	V.opciones = (char *)b.d;
	fmt(V.texto_cuadro, sizeof V.texto_cuadro, "Microsoft deja usar %s con esta licencia (la que trae el paquete).\n"
	    "Léala: si no la acepta, no se pone nada y se borra lo bajado.", redists[redist_elegido].nombre);
	ctl(UC_TEXTO, 0, 16, 10, an - 32, 2 * U, V.texto_cuadro);
	y = 16 + 2 * U;
	c = ctl(UC_LISTA, L_LISTA, 16, y, an - 32, al_l, NULL);
	c->opciones = V.opciones ? V.opciones : "";
	y += al_l + 10;
	ctl(UC_BOTON, L_ACEPTO, an - 16 - 130 - 8 - 200, y, 200, H, "&Acepto la licencia");
	ctl(UC_BOTON, L_NO, an - 16 - 130, y, 130, H, "&No acepto")->cancela = 1;
	abre_cuadro("La licencia de Microsoft", an, y + H + 14, al_licencia);
	V.q.foco = 1;
}

static void abre_componentes(void)
{
	bufer b;
	int k, an = 660, H = ui_alto_control(), U = alto_u(), al_l = alto_lista(5, 18 + 3 * U + 8 + 5 * U + 8 + H + 14), y;
	ui_control *c;

	so_suelta(redists);
	redists = NULL;
	if (red_lista(&redists, &n_redists) || !n_redists) {
		dlg_aviso("Componentes de Microsoft", "No está la lista de componentes (\\HISPA\\COMPAT\\REDIST.LST): reinstale el paquete «compat».");
		return;
	}
	memset(&b, 0, sizeof b);
	for (k = 0; k < n_redists; k++) {
		int p = red_puesto(&redists[k]);

		if (k)
			buf_c(&b, '|');
		buf_fmt(&b, "%s  —  %s", redists[k].nombre, p == 2 ? "puesto" : p == 1 ? "a medias" : "sin poner");
	}
	buf_c(&b, 0);
	nuevo_cuadro();
	V.opciones = (char *)b.d;
	ctl(UC_TEXTO, 0, 16, 10, an - 32, 3 * U,
	    "Muchos programas de antes piden piezas de Microsoft que se pueden bajar gratis\n"
	    "de su web. Se bajan de download.microsoft.com, se comprueba su suma, se enseña\n"
	    "su licencia y, si la acepta, se ponen solo sus DLL en \\WIN32 (modo administrador).");
	y = 18 + 3 * U;
	c = ctl(UC_LISTA, M_LISTA, 16, y, an - 32, al_l, NULL);
	c->opciones = V.opciones ? V.opciones : "";
	y += al_l + 8;
	texto_redist[0] = 0;
	ctl(UC_TEXTO, M_TEXTO, 16, y, an - 32, 5 * U, texto_redist);
	y += 5 * U + 8;
	ctl(UC_BOTON, M_PONER, an - 16 - 120 - 8 - 200, y, 200, H, "&Bajar y poner...")->principal = 1;
	ctl(UC_BOTON, M_CERRAR, an - 16 - 120, y, 120, H, "&Cerrar")->cancela = 1;
	abre_cuadro("Componentes de Microsoft", an, y + H + 14, al_componentes);
	V.tras_suceso = tras_componentes;
	V.q.foco = 1;
	tras_componentes();
}

/* ---- los sucesos ---- */
static int boton_en(int x, int y)
{
	int k;

	for (k = 1; k < B_N; k++)
		if (x >= V.b[k].x && x < V.b[k].x + V.b[k].an && y >= V.b[k].y && y < V.b[k].y + V.b[k].al)
			return k;
	return 0;
}

static void elige_prog(int k)
{
	if (k < 0 || k >= V.n_progs)
		return;
	V.elegido = k;
	if (so_existe(V.progs[k].ruta))
		comprobar(V.progs[k].ruta);
	else {
		copia(V.ruta, V.progs[k].ruta, sizeof V.ruta);
		fmt(V.estado, sizeof V.estado, "%s ya no está (¿se ha borrado o es de otra memoria?).", V.progs[k].ruta);
	}
}

static void pulsa(int k)
{
	switch (k) {
	case B_ELEGIR:
		if (!eleg_carpeta[0] && V.ruta[0]) {
			copia(eleg_carpeta, V.ruta, sizeof eleg_carpeta);
			if (so_existe(eleg_carpeta) == 1)
				sube(eleg_carpeta);
		}
		abre_elegir();
		break;
	case B_COMPROBAR:
		if (V.ruta[0]) {
			char r[RUTA_MAX];

			copia(r, V.ruta, sizeof r);
			comprobar(r);
		}
		break;
	case B_ENVIAR:
		abre_enviar();
		break;
	case B_COMPONENTES:
		abre_componentes();
		break;
	case B_OLVIDAR:
		if (V.elegido >= 0 && V.elegido < V.n_progs) {
			hist_olvida(V.progs[V.elegido].ruta);
			fmt(V.estado, sizeof V.estado, "Olvidado: %s.", V.progs[V.elegido].nombre);
			lee_lista();
			V.elegido = -1;
			V.ruta[0] = 0;
			if (V.hay_inf)
				inf_suelta(&V.inf);
			V.hay_inf = 0;
			pon_vista();
		}
		break;
	}
}

static void desplaza(int n)
{
	int vis = (V.ral - 16) / alto_u();

	V.desp += n;
	if (V.desp > V.n_lin - vis)
		V.desp = V.n_lin - vis;
	if (V.desp < 0)
		V.desp = 0;
}

static void suceso(const so_suceso *s)
{
	if (V.hay_cuadro) {
		if (s->clase == SO_CERRAR) {
			V.vivo = 0;
			return;
		}
		if (s->clase == SO_TAMANO) {
			medidas(s->x, s->y);
			return;
		}
		ui_cuadro_suceso(&V.q, V.v, s);
		if (V.tras_suceso)
			V.tras_suceso();
		if (V.q.cerrado != -9999) {
			int id = V.q.cerrado;
			void (*f)(int) = V.al_cerrar;

			cierra_cuadro();
			if (f)
				f(id);
		}
		return;
	}
	switch (s->clase) {
	case SO_CERRAR:
		V.vivo = 0;
		break;
	case SO_TAMANO:
		medidas(s->x, s->y);
		break;
	case SO_TECLA:
		if (s->tecla == SO_ESC)
			V.vivo = 0;
		else if (s->tecla == SO_T_F5 || s->tecla == SO_T_ALT + ('c' - 'a'))
			pulsa(B_COMPROBAR);
		else if (s->tecla == SO_T_ALT + ('e' - 'a'))
			pulsa(B_ELEGIR);
		else if (s->tecla == SO_T_ALT + ('v' - 'a') && !V.b[B_ENVIAR].apagado)
			pulsa(B_ENVIAR);
		else if (s->tecla == SO_T_ALT + ('p' - 'a'))
			pulsa(B_COMPONENTES);
		else if ((s->tecla == SO_T_ALT + ('o' - 'a') || s->tecla == SO_T_SUPR) && !V.b[B_OLVIDAR].apagado)
			pulsa(B_OLVIDAR);
		else if (s->tecla == SO_T_ABAJO)
			elige_prog(V.elegido + 1 < V.n_progs ? V.elegido + 1 : V.elegido);
		else if (s->tecla == SO_T_ARRIBA)
			elige_prog(V.elegido > 0 ? V.elegido - 1 : 0);
		else if (s->tecla == SO_T_AV_PAG)
			desplaza(10);
		else if (s->tecla == SO_T_RE_PAG)
			desplaza(-10);
		break;
	case SO_RUEDA:
		if (s->x >= V.lx && s->x < V.lx + V.lan) {
			V.prog_desp -= s->pasos;
			if (V.prog_desp < 0)
				V.prog_desp = 0;
		} else
			desplaza(-s->pasos * 3);
		break;
	case SO_BAJA: {
		int k = boton_en(s->x, s->y);

		if (k && !V.b[k].apagado) {
			V.pulsado = k;
			break;
		}
		if (s->x >= V.lx && s->x < V.lx + V.lan && s->y >= V.ly && s->y < V.ly + V.lal) {
			int i = V.prog_desp + (s->y - V.ly - 1) / alto_prog();

			if (i < V.n_progs)
				elige_prog(i);
			break;
		}
		{
			int en = ui_desp_en(&V.barra, s->x, s->y), vis = (V.ral - 16) / alto_u();

			if (en == 1)
				desplaza(-1);
			else if (en == 2)
				desplaza(1);
			else if (en == 3)
				desplaza(-vis);
			else if (en == 4)
				desplaza(vis);
			else if (en == 5) {
				V.arrastra_barra = 1;
				V.desp0 = V.desp;
				V.y0 = s->y;
			}
		}
		break;
	}
	case SO_MUEVE:
		if (V.arrastra_barra && (s->botones & SO_BOTON_IZQ)) {
			V.desp = (int)ui_desp_arrastra(&V.barra, V.desp0, V.y0, s->y);
			desplaza(0);
		} else
			V.encima = boton_en(s->x, s->y);
		break;
	case SO_SUBE: {
		int k = boton_en(s->x, s->y);

		V.arrastra_barra = 0;
		if (V.pulsado && k == V.pulsado && !V.b[k].apagado) {
			V.pulsado = 0;
			pulsa(k);
		}
		V.pulsado = 0;
		break;
	}
	}
}

int ventana_principal(const char *ruta, const char *fallo)
{
	so_suceso s;
	int an, al;

	memset(&V, 0, sizeof V);
	V.elegido = -1;
	so_pantalla(&an, &al);
	an = an > 900 ? 860 : an - 40;
	al = al > 640 ? 600 : al - 60;
	V.v = so_ventana("Asistente de compatibilidad", an, al);
	if (!V.v) {
		plat_escribe("El Asistente de compatibilidad necesita el escritorio (o pruebe COMPAT /ORDEN).\n");
		return 1;
	}
	so_centra(V.v);
	lee_lista();
	if (fallo && ruta && ruta[0]) {
		char c[RUTA_MAX];

		V.clase_aviso = 1;
		fmt(V.aviso, sizeof V.aviso, "WIN32 no ha podido seguir con %s%s%s.\nAbajo, lo que le falta a todo su programa (su carpeta), explicado.",
		    sin_carpeta(ruta), iguales(fallo, "?") ? "" : ": le falta ", iguales(fallo, "?") ? "" : fallo);
		/* (la carpeta del programa: con sus DLL) */
		copia(c, ruta, sizeof c);
		if (so_existe(c) == 1)
			sube(c);
		medidas(an, al);
		pinta();
		comprobar(c[0] ? c : ruta);
	} else if (ruta && iguales_sin_caja(ruta, "/AVISOS")) {
		char c[RUTA_MAX], r[RUTA_MAX], *t;

		V.clase_aviso = 2;
		copia(V.aviso, "Hay novedades: win32 ha cambiado.\n", sizeof V.aviso);
		if (!hist_carpeta(c, sizeof c)) {
			une_ruta(r, sizeof r, c, "AVISOS.TXT");
			t = lee_texto(r, NULL, 64 * 1024);
			if (t) {
				/* (el último aviso: lo de detrás de la última fecha) */
				char *p = t, *ult = t;

				while ((p = strstr(p, "(win32 ")) != NULL) {
					char *f = strchr(p, '\n');

					if (!f)
						break;
					ult = f + 1;
					p = f;
				}
				{
					char x[400];
					u32 j = 0;

					while (*ult && j + 1 < sizeof x) {
						x[j++] = *ult == '\r' ? ' ' : *ult == '\n' ? ' ' : *ult;
						ult++;
					}
					x[j] = 0;
					anade(V.aviso, x, sizeof V.aviso);
				}
				so_suelta(t);
			}
		}
		medidas(an, al);
	} else {
		medidas(an, al);
		if (ruta && ruta[0]) {
			pinta();
			comprobar(ruta);
		}
	}
	V.vivo = 1;
	pinta();
	while (V.vivo) {
		if (!so_lee_suceso(V.v, &s, 1))
			continue;
		suceso(&s);
		/* (lo que se haya juntado, de una vez) */
		while (V.vivo && so_lee_suceso(V.v, &s, 0))
			suceso(&s);
		if (V.vivo)
			pinta();
	}
	so_cierra_ventana(V.v);
	return 0;
}

/* ---- la ventanita del aviso (abajo a la derecha, 45 s) ---- */
int ventana_aviso(const char *avisos)
{
	lienzo l;
	int v, an = 460, al, n = 0, U, x, y, sw, sh, vivo = 1;
	const char *p;
	u32 fin;
	so_suceso s;
	int bx1, bx2, by, bw = 120, H;

	for (p = avisos; *p; p++)
		if (*p == '\n')
			n++;
	if (n > 4)
		n = 4;
	U = lz_ui_alto();
	H = ui_alto_control();
	al = 14 + U + 6 + n * U + 10 + H + 12;
	v = so_ventana("Asistente de compatibilidad", an, al);
	if (!v) {
		plat_escribe(avisos);
		return 0;
	}
	so_pantalla(&sw, &sh);
	if (sw > an + 40 && sh > al + 200)
		plat_coloca(v, sw - an - 20, sh - 34 - al - 44);
	if (lz_nuevo(&l, an, al) == 0) {
		so_cierra_ventana(v);
		return 0;
	}
	bx1 = 54;
	bx2 = 54 + bw + 10;
	by = al - H - 12;
	fin = so_tics() + 45 * so_hz();
	dice("compat: aviso en la bandeja: %s", avisos);
	while (vivo && so_tics() < fin) {
		int k;
		const char *q = avisos;

		lz_degradado(&l, 0, 0, an, al, 0xF4FBF6, 0xDDEFE2);
		lz_marco(&l, 0, 0, an, al, 0x1F7A3A);
		/* el icono: un círculo con ✓ */
		lz_circulo(&l, 28 * 16, 26 * 16, 14 * 16, 0x1F7A3A);
		lz_linea(&l, 21 * 16, 26 * 16, 26 * 16, 31 * 16, 40, UI_BLANCO);
		lz_linea(&l, 26 * 16, 31 * 16, 35 * 16, 20 * 16, 40, UI_BLANCO);
		lz_ui_negrita(&l, 54, 12 + lz_ui_base(), "Asistente de compatibilidad: novedades", UI_TINTA);
		y = 12 + U + 6;
		for (k = 0; k < n && *q; k++) {
			char t[240];
			const char *e = strchr(q, '\n');
			u32 m = e ? (u32)(e - q) : largo(q);

			copia(t, q, m + 1 < sizeof t ? m + 1 : sizeof t);
			lz_recorte(&l, 54, 0, an - 62, al);
			lz_ui(&l, 54, y + lz_ui_base(), t, UI_TINTA);
			lz_sin_recorte(&l);
			y += U;
			q = e ? e + 1 : q + m;
		}
		lz_redondeado(&l, bx1, by, bw, H, 5, UI_BLANCO);
		lz_marco_redondeado(&l, bx1, by, bw, H, 5, 0x1E4A99);
		lz_ui_negrita(&l, bx1 + (bw - lz_ui_ancho_negrita("Verlo")) / 2, by + (H - U) / 2 + lz_ui_base(), "Verlo", UI_TINTA);
		lz_redondeado(&l, bx2, by, bw, H, 5, UI_BLANCO);
		lz_marco_redondeado(&l, bx2, by, bw, H, 5, UI_RAYA_FUERTE);
		lz_ui_negrita(&l, bx2 + (bw - lz_ui_ancho_negrita("Ahora no")) / 2, by + (H - U) / 2 + lz_ui_base(), "Ahora no", UI_TINTA);
		so_mapa(v, 0, 0, an, al, l.p, l.paso);
		x = 0;
		while (so_lee_suceso(v, &s, 0)) {
			x = 1;
			if (s.clase == SO_CERRAR || (s.clase == SO_TECLA && s.tecla == SO_ESC))
				vivo = 0;
			else if (s.clase == SO_BAJA && s.y >= by && s.y < by + H) {
				if (s.x >= bx1 && s.x < bx1 + bw) {
					char pr[60];

					so_programa(pr, sizeof pr);
					plat_lanza(pr, "/AVISOS");
					vivo = 0;
				} else if (s.x >= bx2 && s.x < bx2 + bw)
					vivo = 0;
			}
		}
		if (!x)
			so_duerme(so_hz() / 10 ? so_hz() / 10 : 1);
	}
	lz_suelta(&l);
	so_cierra_ventana(v);
	return 0;
}
