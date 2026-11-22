/* herramientas.c - Pintura 2: lo que hace cada herramienta con el ratón
 * sobre el lienzo (con el botón izquierdo, el color 1; con el derecho, el
 * 2), lo elegido (levantarlo, moverlo, copiarlo con Control, pegarlo,
 * borrarlo; transparente u opaco), la caja de texto, el polígono (cada
 * clic, un vértice; dos clics, Intro o volver al primero lo cierran), la
 * curva (la raya, y luego sus dos dobleces) y las asas que cambian el
 * tamaño del lienzo.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "pintura.h"
#include "letras.h"
#include "operaciones.h"

static const struct {
	const char *nombre, *ayuda;
	u32 tecla;
} herrs[H_N] = {
	{ "Selección rectangular", "Selección rectangular (S): arrastre para elegir; arrastre lo elegido para moverlo (con Control, una copia)", 's' },
	{ "Selección a mano alzada", "Selección a mano alzada (M): rodee lo que quiere elegir; luego se mueve como la rectangular", 'm' },
	{ "Lápiz", "Lápiz (L): puntos sueltos, sin suavizar, con el color exacto", 'l' },
	{ "Pincel", "Pincel (P): trazos suavizados; la forma y el grosor, en la barra de opciones", 'p' },
	{ "Aerógrafo", "Aerógrafo (A): rocía gotas mientras se mantiene pulsado", 'a' },
	{ "Borrador", "Borrador (B): pinta con el color 2, o deja transparente (barra de opciones)", 'b' },
	{ "Relleno", "Relleno (R): rellena lo del mismo color (con la tolerancia de la barra de opciones)", 'r' },
	{ "Cuentagotas", "Cuentagotas (C): toma el color de la imagen y dice su nombre y su código", 'c' },
	{ "Texto", "Texto (T): haga clic donde quiera escribir; la letra, en la barra de opciones; Esc anula", 't' },
	{ "Lupa", "Lupa (Z): un clic acerca; con el botón derecho, aleja (también Control y la rueda)", 'z' },
	{ "Línea", "Línea (I): arrastre de un extremo al otro; con Mayús, recta o a 45 grados", 'i' },
	{ "Curva", "Curva (U): arrastre la raya y luego arrastre dos veces para doblarla (Intro la deja)", 'u' },
	{ "Rectángulo", "Rectángulo (Q): arrastre de una esquina a otra; con Mayús, un cuadrado", 'q' },
	{ "Rectángulo redondeado", "Rectángulo redondeado (O): como el rectángulo, con las esquinas redondas", 'o' },
	{ "Elipse", "Elipse (E): arrastre la caja de la elipse; con Mayús, un círculo", 'e' },
	{ "Polígono", "Polígono (G): un clic en cada vértice; dos clics, Intro o el primer vértice lo cierran", 'g' },
};

const char *herr_nombre(int h)
{
	return h >= 0 && h < H_N ? herrs[h].nombre : "";
}

const char *herr_ayuda(int h)
{
	return h >= 0 && h < H_N ? herrs[h].ayuda : "";
}

u32 herr_tecla(int h)
{
	return h >= 0 && h < H_N ? herrs[h].tecla : 0;
}

/* ---- el estado del ratón ---- */
static int arrastra;		/* hay algo pulsado sobre el lienzo */
static int der;			/* con el botón derecho */
static int ax, ay, lx, ly;	/* dónde se pulsó y lo último (en la lámina) */
static float lfx, lfy;
static u32 tinta;
static int moviendo, mx0, my0;	/* (lo elegido, o la caja de texto) */
static int moviendo_texto;
static int asa_modo;
static u32 semilla = 12345;
static int quietos;		/* (el aerógrafo, sin moverse) */
#define MAX_POL 512
static int pol[2 * MAX_POL], n_pol, pol_activo;
static int cur[8], fase_curva;	/* 0 sin curva; 1 la raya puesta; 2 el primer doblez */
static int libre_cabe;
static const char *nombre_paso_sel = "Mover la selección";

int herr_arrastrando(void)
{
	return arrastra || moviendo || moviendo_texto || asa_modo;
}

static void repinta(int zona[4])
{
	pin_repinta_zona(zona);
}

/* la zona de lo elegido (en la lámina, con un punto de más) */
static void zona_sel(int z[4])
{
	const seleccion_t *s = &P.sel;
	int an = s->flota ? s->flotante.an : s->an, al = s->flota ? s->flotante.al : s->al;

	zona_suma(z, s->x - 2, s->y - 2, s->x + an + 2, s->y + al + 2);
}

static void zona_texto(int z[4])
{
	zona_suma(z, P.tx.x - 3, P.tx.y - 3, P.tx.x + (P.tx.an > 4 ? P.tx.an : 4) + 3, P.tx.y + P.tx.al + 3);
}

/* ===================================================================== */
/*  Lo elegido                                                           */
/* ===================================================================== */
int sel_hay(void)
{
	return P.sel.activa;
}

static void suelta_libre(void)
{
	so_suelta(P.sel.libre);
	P.sel.libre = NULL;
	P.sel.n_libre = 0;
	libre_cabe = 0;
}

/* lo elegido a mano alzada, como máscara de su caja (sin suavizar) */
static int mascara_libre(mascara *m)
{
	const seleccion_t *s = &P.sel;
	float *f;
	int i, n = s->n_libre;

	if (!masc_nueva(m, &P.img, s->x, s->y, s->x + s->an, s->y + s->al))
		return 0;
	f = so_pide((size_t)n * 2 * sizeof(float));
	if (!f) {
		masc_suelta(m);
		return 0;
	}
	for (i = 0; i < 2 * n; i++)
		f[i] = (float)s->libre[i] + 0.5f;
	masc_poligono(m, f, &n, 1, 0);
	so_suelta(f);
	return 1;
}

static void fija_flotante(void)
{
	seleccion_t *s = &P.sel;
	int z[4] = { 0, 0, 0, 0 };

	if (!s->flota)
		return;
	zona_sel(z);
	lamina_encima(&P.img, &s->flotante, s->x, s->y, P.sel_transparente, P.c2, z);
	his_acaba(nombre_paso_sel);
	s->an = s->flotante.an;
	s->al = s->flotante.al;
	lam_suelta(&s->flotante);
	s->flota = 0;
	suelta_libre();
	P.cambiado = 1;
	repinta(z);
}

void sel_nada(void)
{
	int z[4] = { 0, 0, 0, 0 };

	if (P.sel.activa)
		zona_sel(z);
	fija_flotante();
	suelta_libre();
	P.sel.activa = 0;
	repinta(z);
}

void sel_todo(void)
{
	herr_fija();
	sel_nada();
	P.sel.activa = 1;
	P.sel.x = P.sel.y = 0;
	P.sel.an = P.img.an;
	P.sel.al = P.img.al;
	if (P.herr != H_SELECCION && P.herr != H_LIBRE)
		pin_pon_herramienta(H_SELECCION);
	pin_repinta_todo();
}

void sel_levanta(int copia)
{
	seleccion_t *s = &P.sel;
	mascara m;
	int x, y, hay_m = 0;

	if (!s->activa || s->flota)
		return;
	his_empieza();
	if (!lam_trozo(&s->flotante, &P.img, s->x, s->y, s->an, s->al)) {
		his_cancela(NULL);
		pin_aviso("No hay memoria para levantar lo elegido (%d x %d)", s->an, s->al);
		return;
	}
	if (s->libre && s->n_libre >= 3)
		hay_m = mascara_libre(&m);
	if (hay_m)
		for (y = 0; y < m.al; y++)
			for (x = 0; x < m.an; x++)
				if (!m.a[(size_t)y * m.an + x])
					s->flotante.p[(size_t)y * s->an + x] = 0;
	if (!copia) {
		his_toca(s->x, s->y, s->x + s->an, s->y + s->al);
		for (y = 0; y < s->al; y++)
			for (x = 0; x < s->an; x++) {
				int ix = s->x + x, iy = s->y + y;

				if (ix < 0 || iy < 0 || ix >= P.img.an || iy >= P.img.al)
					continue;
				if (hay_m && !m.a[(size_t)y * m.an + x])
					continue;
				P.img.p[(size_t)iy * P.img.an + ix] = P.c2;
			}
	}
	if (hay_m)
		masc_suelta(&m);
	nombre_paso_sel = copia ? "Copiar la selección" : "Mover la selección";
	s->flota = 1;
}

void sel_borra(void)
{
	seleccion_t *s = &P.sel;
	int z[4] = { 0, 0, 0, 0 };

	if (!s->activa)
		return;
	zona_sel(z);
	if (s->flota) {
		lam_suelta(&s->flotante);
		s->flota = 0;
		his_acaba("Borrar la selección");
	} else {
		mascara m;
		int x, y, hay_m = s->libre && s->n_libre >= 3 && mascara_libre(&m);

		his_empieza();
		his_toca(s->x, s->y, s->x + s->an, s->y + s->al);
		for (y = 0; y < s->al; y++)
			for (x = 0; x < s->an; x++)
				if (!hay_m || m.a[(size_t)y * m.an + x])
					P.img.p[(size_t)(s->y + y) * P.img.an + s->x + x] = P.c2;
		if (hay_m)
			masc_suelta(&m);
		his_acaba("Borrar la selección");
	}
	suelta_libre();
	s->activa = 0;
	P.cambiado = 1;
	repinta(z);
}

int sel_copia(void)
{
	seleccion_t *s = &P.sel;

	if (!s->activa)
		return 0;
	lam_suelta(&P.papel);
	if (s->flota) {
		if (!lam_nueva(&P.papel, s->flotante.an, s->flotante.al, 0))
			return 0;
		memcpy(P.papel.p, s->flotante.p, (size_t)s->flotante.an * s->flotante.al * 4);
		return 1;
	}
	if (!lam_trozo(&P.papel, &P.img, s->x, s->y, s->an, s->al))
		return 0;
	if (s->libre && s->n_libre >= 3) {
		mascara m;
		int x, y;

		if (mascara_libre(&m)) {
			for (y = 0; y < m.al; y++)
				for (x = 0; x < m.an; x++)
					if (!m.a[(size_t)y * m.an + x])
						P.papel.p[(size_t)y * P.papel.an + x] = 0;
			masc_suelta(&m);
		}
	}
	return 1;
}

void sel_pega(const lamina *o, int x, int y, const char *que)
{
	seleccion_t *s = &P.sel;

	herr_fija();
	sel_nada();
	if (o->an > P.img.an || o->al > P.img.al) {
		/* lo pegado no cabe: el lienzo crece (con el color 2) */
		lamina n, antes;
		int an = o->an > P.img.an ? o->an : P.img.an, al = o->al > P.img.al ? o->al : P.img.al;

		if (!op_lienzo(&P.img, &n, an, al, P.c2)) {
			pin_aviso("No hay memoria para agrandar el lienzo a %d x %d", an, al);
			return;
		}
		antes = P.img;
		P.img = n;
		his_lamina(&P.img, 0);
		his_entera("Agrandar el lienzo", &antes);
		pin_cambio_imagen();
		pin_aviso("El lienzo ha crecido a %d x %d para que quepa lo pegado", an, al);
		x = y = 0;
	}
	if (!lam_nueva(&s->flotante, o->an, o->al, 0)) {
		pin_aviso("No hay memoria para pegar (%d x %d)", o->an, o->al);
		return;
	}
	memcpy(s->flotante.p, o->p, (size_t)o->an * o->al * 4);
	if (x + o->an > P.img.an)
		x = P.img.an - o->an;
	if (y + o->al > P.img.al)
		y = P.img.al - o->al;
	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;
	his_empieza();
	s->activa = s->flota = 1;
	s->x = x;
	s->y = y;
	s->an = o->an;
	s->al = o->al;
	nombre_paso_sel = que;
	if (P.herr != H_SELECCION && P.herr != H_LIBRE)
		P.herr = H_SELECCION;
	P.cambiado = 1;
	pin_repinta_todo();
}

lamina *sel_lamina_para_operar(void)
{
	if (!P.sel.activa)
		return NULL;
	if (!P.sel.flota)
		sel_levanta(0);
	if (!P.sel.flota)
		return NULL;
	nombre_paso_sel = "Cambiar la selección";
	return &P.sel.flotante;
}

void sel_cambiada(void)
{
	P.sel.an = P.sel.flotante.an;
	P.sel.al = P.sel.flotante.al;
	suelta_libre();
	pin_repinta_todo();
}

/* ===================================================================== */
/*  La caja de texto                                                     */
/* ===================================================================== */
static int cara_texto(int *falsas)
{
	return letra_cara(P.letra, P.negrita, P.cursiva, falsas);
}

void herr_texto_rehaz(void)
{
	caja_texto *t = &P.tx;
	int falsas, cara = cara_texto(&falsas), sube, baja, renglon, w = 0, lineas = 1, i, k, y;
	u32 a;
	int z[4] = { 0, 0, 0, 0 };

	if (!t->activa)
		return;
	zona_texto(z);
	texto_medidas(cara, P.tam_letra, &sube, &baja, &renglon);
	/* lo que mide: la línea más ancha y las líneas */
	for (i = 0, a = 0; (u32)i <= t->n; i++)
		if ((u32)i == t->n || t->t[i] == '\n') {
			int an = texto_ancho(cara, P.tam_letra, falsas, t->t + a, (u32)i - a);

			if (an > w)
				w = an;
			if ((u32)i < t->n)
				lineas++;
			a = (u32)i + 1;
		}
	t->an = w + 4 + (P.cursiva ? P.tam_letra / 4 : 0);
	t->al = lineas * renglon + 2;
	if (t->an > LAM_MAX_LADO)
		t->an = LAM_MAX_LADO;
	if (t->al > LAM_MAX_LADO)
		t->al = LAM_MAX_LADO;
	lam_suelta(&t->dibujo);
	if (!lam_nueva(&t->dibujo, t->an < 2 ? 2 : t->an, t->al, P.fondo_texto ? P.c2 : 0))
		return;
	y = 1 + sube;
	for (i = 0, a = 0, k = 0; (u32)i <= t->n; i++)
		if ((u32)i == t->n || t->t[i] == '\n') {
			texto_en_lamina(&t->dibujo, cara, P.tam_letra, falsas, 2, y + k * renglon, t->t + a, (u32)i - a, P.c1);
			k++;
			a = (u32)i + 1;
		}
	zona_texto(z);
	repinta(z);
}

void herr_texto_pinta_cursor(lienzo *L)
{
	caja_texto *t = &P.tx;
	int falsas, cara = cara_texto(&falsas), sube, baja, renglon, lineas = 0, x, y0, y1;
	u32 i, a = 0;

	if (!t->activa)
		return;
	texto_medidas(cara, P.tam_letra, &sube, &baja, &renglon);
	for (i = 0; i < t->n; i++)
		if (t->t[i] == '\n') {
			lineas++;
			a = i + 1;
		}
	x = vista_de_lamina_x(t->x + 2 + texto_ancho(cara, P.tam_letra, falsas, t->t + a, t->n - a));
	y0 = vista_de_lamina_y(t->y + 1 + lineas * renglon);
	y1 = vista_de_lamina_y(t->y + 1 + (lineas + 1) * renglon);
	lz_rect(L, x, y0, 2, y1 - y0 > 2 ? y1 - y0 : 2, 0x000000);
	lz_rect(L, x + 2, y0, 1, y1 - y0 > 2 ? y1 - y0 : 2, 0xFFFFFF);
}

static void texto_empieza(int x, int y)
{
	caja_texto *t = &P.tx;

	t->activa = 1;
	t->x = x;
	t->y = y;
	t->n = 0;
	herr_texto_rehaz();
	pin_aviso("Escriba el texto (Intro: otra línea; Esc: anular). Un clic fuera lo deja en la imagen");
}

static void texto_fija(void)
{
	caja_texto *t = &P.tx;
	int z[4] = { 0, 0, 0, 0 };

	if (!t->activa)
		return;
	zona_texto(z);
	herr_texto_rehaz();
	if (t->n && t->dibujo.p) {
		his_empieza();
		lamina_encima(&P.img, &t->dibujo, t->x, t->y, 0, 0, z);
		his_acaba("Texto");
		P.cambiado = 1;
		dice("pintura: texto puesto (%u letras) en %d, %d", t->n, t->x, t->y);
	}
	t->activa = 0;
	lam_suelta(&t->dibujo);
	repinta(z);
}

static void texto_anula(void)
{
	int z[4] = { 0, 0, 0, 0 };

	zona_texto(z);
	P.tx.activa = 0;
	lam_suelta(&P.tx.dibujo);
	repinta(z);
}

/* ===================================================================== */
/*  Las figuras                                                          */
/* ===================================================================== */
static void colores_figura(u32 *borde, u32 *dentro)
{
	*borde = der ? P.c2 : P.c1;
	*dentro = der ? P.c1 : P.c2;
}

/* con Mayús: un cuadrado (o un círculo), o la raya a 45 grados */
static void ajusta(int x0, int y0, int *x1, int *y1, int raya)
{
	int dx = *x1 - x0, dy = *y1 - y0, adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;

	if (raya) {
		if (adx > 2 * ady)
			*y1 = y0;
		else if (ady > 2 * adx)
			*x1 = x0;
		else {
			int m = adx > ady ? adx : ady;

			*x1 = x0 + (dx < 0 ? -m : m);
			*y1 = y0 + (dy < 0 ? -m : m);
		}
		return;
	}
	{
		int m = adx > ady ? adx : ady;

		*x1 = x0 + (dx < 0 ? -m : m);
		*y1 = y0 + (dy < 0 ? -m : m);
	}
}

static void figura(int x1, int y1, int mayus, int z[4])
{
	u32 borde, dentro;
	float g = (float)P.grosor_figura;

	colores_figura(&borde, &dentro);
	if (mayus)
		ajusta(ax, ay, &x1, &y1, P.herr == H_RAYA);
	switch (P.herr) {
	case H_RAYA:
		fig_raya(&P.img, ax, ay, x1, y1, g, borde, P.suave, z);
		break;
	case H_RECTANGULO:
	case H_REDONDEADO:
		fig_rectangulo(&P.img, ax, ay, x1, y1, P.herr == H_REDONDEADO, g, P.relleno, borde, dentro, P.suave, z);
		break;
	case H_ELIPSE:
		fig_elipse(&P.img, ax, ay, x1, y1, g, P.relleno, borde, dentro, P.suave, z);
		break;
	}
	lx = x1;
	ly = y1;
}

static void pinta_curva(int z[4])
{
	u32 borde, dentro;

	colores_figura(&borde, &dentro);
	fig_curva(&P.img, cur, (float)P.grosor_figura, borde, P.suave, z);
}

static void pinta_poligono(int cerrado, int z[4])
{
	u32 borde, dentro;

	colores_figura(&borde, &dentro);
	fig_poligono(&P.img, pol, n_pol, cerrado, (float)P.grosor_figura, cerrado ? P.relleno : RELLENO_NO, borde, dentro, P.suave, z);
}

static void poligono_cierra(void)
{
	int z[4] = { 0, 0, 0, 0 };

	if (!pol_activo)
		return;
	his_restaura(z);
	/* (el último punto repetido, fuera) */
	while (n_pol >= 2 && pol[2 * n_pol - 2] == pol[2 * n_pol - 4] && pol[2 * n_pol - 1] == pol[2 * n_pol - 3])
		n_pol--;
	pinta_poligono(n_pol >= 3, z);
	his_acaba("Polígono");
	pol_activo = 0;
	n_pol = 0;
	P.cambiado = 1;
	dice("pintura: polígono cerrado");
	repinta(z);
}

static void curva_acaba(void)
{
	if (!fase_curva && !arrastra)
		return;
	his_acaba("Curva");
	fase_curva = 0;
	P.cambiado = 1;
}

/* ===================================================================== */
/*  Lo pendiente                                                         */
/* ===================================================================== */
void herr_fija(void)
{
	texto_fija();
	if (pol_activo)
		poligono_cierra();
	if (fase_curva)
		curva_acaba();
	fija_flotante();
	arrastra = moviendo = moviendo_texto = 0;
}

void herr_cancela(void)
{
	int z[4] = { 0, 0, 0, 0 };

	if (P.tx.activa)
		texto_anula();
	if (pol_activo || fase_curva || P.sel.flota) {
		his_cancela(z);
		pol_activo = fase_curva = n_pol = 0;
		if (P.sel.flota) {
			lam_suelta(&P.sel.flotante);
			P.sel.flota = 0;
		}
		repinta(z);
	}
	if (P.sel.activa)
		sel_nada();
	arrastra = moviendo = moviendo_texto = 0;
}

/* ===================================================================== */
/*  El ratón                                                             */
/* ===================================================================== */
static void pincel_de(pincel *p)
{
	memset(p, 0, sizeof *p);
	p->color = tinta;
	p->modo = TINTA_SOBRE;
	p->forma = P.forma;
	p->grosor = (float)P.grosor_pincel;
	p->aa = P.suave;
	if (P.herr == H_BORRADOR) {
		p->forma = FORMA_CUADRADA;
		p->grosor = (float)P.grosor_borrador;
		p->aa = 0;
		if (P.borra_transparente)
			p->modo = TINTA_BORRA;
		else
			p->color = OPACO(der ? P.c1 : P.c2);
	}
}

static int dentro_sel(int ix, int iy)
{
	const seleccion_t *s = &P.sel;
	int an = s->flota ? s->flotante.an : s->an, al = s->flota ? s->flotante.al : s->al;

	return s->activa && ix >= s->x && iy >= s->y && ix < s->x + an && iy < s->y + al;
}

static void sel_rect(int x0, int y0, int x1, int y1)
{
	seleccion_t *s = &P.sel;

	if (x0 > x1) {
		int t = x0;

		x0 = x1;
		x1 = t;
	}
	if (y0 > y1) {
		int t = y0;

		y0 = y1;
		y1 = t;
	}
	if (x0 < 0)
		x0 = 0;
	if (y0 < 0)
		y0 = 0;
	if (x1 >= P.img.an)
		x1 = P.img.an - 1;
	if (y1 >= P.img.al)
		y1 = P.img.al - 1;
	s->x = x0;
	s->y = y0;
	s->an = x1 - x0 + 1;
	s->al = y1 - y0 + 1;
	s->activa = s->an > 0 && s->al > 0;
}

static void libre_mete(int x, int y)
{
	seleccion_t *s = &P.sel;

	x = x < 0 ? 0 : x >= P.img.an ? P.img.an - 1 : x;
	y = y < 0 ? 0 : y >= P.img.al ? P.img.al - 1 : y;
	if (s->n_libre && s->libre[2 * s->n_libre - 2] == x && s->libre[2 * s->n_libre - 1] == y)
		return;
	if (s->n_libre >= libre_cabe) {
		int c = libre_cabe ? libre_cabe * 2 : 256;
		int *n = so_repide(s->libre, (size_t)c * 2 * sizeof(int));

		if (!n)
			return;
		s->libre = n;
		libre_cabe = c;
	}
	s->libre[2 * s->n_libre] = x;
	s->libre[2 * s->n_libre + 1] = y;
	s->n_libre++;
}

static void aerografo(int x, int y, int z[4])
{
	int r = P.radio_aero, n = P.densidad == 0 ? r / 2 + 2 : P.densidad == 2 ? r * 3 + 8 : r + 4;

	trazo_aerografo(&P.img, tinta, x, y, r, n, &semilla, z);
}

void herr_baja(const so_suceso *s)
{
	int ix, iy, z[4] = { 0, 0, 0, 0 };
	float fx, fy;

	vista_a_lamina(s->x, s->y, &ix, &iy);
	vista_a_lamina_f(s->x, s->y, &fx, &fy);
	der = (s->botones & SO_BOTON_DER) != 0;
	tinta = der ? P.c2 : P.c1;
	/* las asas del lienzo */
	if (!P.sel.flota && !pol_activo && !fase_curva && !P.tx.activa && (asa_modo = vista_asa_en(s->x, s->y)) != 0) {
		asa_an = P.img.an;
		asa_al = P.img.al;
		asa_encima = asa_modo;		/* (PINTURA32) la flecha sigue mientras se arrastra */
		asa_mx = s->x;
		asa_my = s->y;
		pin_aviso("Tamaño del lienzo: %d x %d píxeles (suelte para aplicarlo)", asa_an, asa_al);
		return;
	}
	ax = lx = ix;
	ay = ly = iy;
	lfx = fx;
	lfy = fy;
	quietos = 0;
	switch (P.herr) {
	case H_LAPIZ:
		his_empieza();
		trazo_lapiz(&P.img, tinta, ix, iy, ix, iy, z);
		arrastra = 1;
		break;
	case H_PINCEL:
	case H_BORRADOR: {
		pincel p;

		pincel_de(&p);
		his_empieza();
		trazo(&P.img, &p, fx, fy, fx, fy, z);
		arrastra = 1;
		break;
	}
	case H_AEROGRAFO:
		his_empieza();
		semilla = 12345;
		aerografo(ix, iy, z);
		arrastra = 1;
		break;
	case H_RELLENO: {
		u32 n;

		if (ix < 0 || iy < 0 || ix >= P.img.an || iy >= P.img.al)
			break;
		his_empieza();
		n = rellena(&P.img, ix, iy, tinta, P.tolerancia, z);
		his_acaba("Relleno");
		if (n)
			P.cambiado = 1;
		dice("pintura: relleno de %u puntos en %d, %d", n, ix, iy);
		break;
	}
	case H_CUENTAGOTAS:
		if (ix >= 0 && iy >= 0 && ix < P.img.an && iy < P.img.al) {
			u32 c = P.img.p[(size_t)iy * P.img.an + ix];
			char d[120];

			pin_elige_color(der ? 2 : 1, c);
			color_describe(c, d, sizeof d);
			pin_aviso("Cuentagotas, color %d: %s", der ? 2 : 1, d);
			pin_pon_herramienta(P.herr_antes != H_CUENTAGOTAS ? P.herr_antes : H_PINCEL);
		}
		break;
	case H_TEXTO:
		if (P.tx.activa && ix >= P.tx.x && iy >= P.tx.y && ix < P.tx.x + P.tx.an && iy < P.tx.y + P.tx.al) {
			moviendo_texto = 1;
			mx0 = ix - P.tx.x;
			my0 = iy - P.tx.y;
			break;
		}
		if (P.tx.activa) {
			/* (un clic fuera deja el texto en la imagen; el siguiente, otra caja) */
			texto_fija();
			break;
		}
		if (ix >= 0 && iy >= 0 && ix < P.img.an && iy < P.img.al)
			texto_empieza(ix, iy);
		break;
	case H_LUPA:
		vista_zoom_en(vista_zoom_siguiente(P.zoom, !der), s->x, s->y);
		pin_aviso("Zoom: %d %%", P.zoom / 10);
		pin_repinta_todo();
		break;
	case H_RAYA:
	case H_RECTANGULO:
	case H_REDONDEADO:
	case H_ELIPSE:
		his_empieza();
		figura(ix, iy, (s->mods & SO_CON_MAYUS) != 0, z);
		arrastra = 1;
		break;
	case H_CURVA:
		if (fase_curva == 0) {
			his_empieza();
			cur[0] = cur[2] = cur[4] = cur[6] = ix;
			cur[1] = cur[3] = cur[5] = cur[7] = iy;
		} else if (fase_curva == 1) {
			cur[2] = cur[4] = ix;
			cur[3] = cur[5] = iy;
		} else {
			cur[4] = ix;
			cur[5] = iy;
		}
		his_restaura(z);
		pinta_curva(z);
		arrastra = 1;
		break;
	case H_POLIGONO:
		if (!pol_activo) {
			his_empieza();
			pol_activo = 1;
			pol[0] = pol[2] = ix;
			pol[1] = pol[3] = iy;
			n_pol = 2;
		} else {
			int ex = vista_de_lamina_x(pol[0]) - s->x, ey = vista_de_lamina_y(pol[1]) - s->y;

			if (s->veces >= 2 || (n_pol >= 3 && ex * ex + ey * ey <= 36)) {
				poligono_cierra();
				return;
			}
			if (n_pol >= 2 && pol[2 * n_pol - 2] == pol[2 * n_pol - 4] && pol[2 * n_pol - 1] == pol[2 * n_pol - 3]) {
				/* (el primer clic, sin arrastrar: este es el segundo vértice) */
				pol[2 * n_pol - 2] = ix;
				pol[2 * n_pol - 1] = iy;
			} else if (n_pol < MAX_POL) {
				pol[2 * n_pol] = ix;
				pol[2 * n_pol + 1] = iy;
				n_pol++;
			}
		}
		his_restaura(z);
		pinta_poligono(0, z);
		arrastra = 1;
		break;
	case H_SELECCION:
	case H_LIBRE:
		if (dentro_sel(ix, iy)) {
			zona_sel(z);
			if (!P.sel.flota)
				sel_levanta((s->mods & SO_CON_CONTROL) != 0);
			else if (s->mods & SO_CON_CONTROL) {
				/* una copia donde está, y se sigue moviendo la otra */
				int z2[4] = { 0, 0, 0, 0 };

				lamina_encima(&P.img, &P.sel.flotante, P.sel.x, P.sel.y, P.sel_transparente, P.c2, z2);
				nombre_paso_sel = "Copiar la selección";
			}
			moviendo = 1;
			mx0 = ix - P.sel.x;
			my0 = iy - P.sel.y;
			break;
		}
		if (P.sel.activa)
			zona_sel(z);
		fija_flotante();
		suelta_libre();
		P.sel.activa = 0;
		arrastra = 1;
		if (P.herr == H_LIBRE)
			libre_mete(ix, iy);
		break;
	}
	repinta(z);
}

void herr_mueve(const so_suceso *s)
{
	int ix, iy, z[4] = { 0, 0, 0, 0 };
	float fx, fy;

	vista_a_lamina(s->x, s->y, &ix, &iy);
	vista_a_lamina_f(s->x, s->y, &fx, &fy);
	if (asa_modo) {
		int an = asa_modo != 2 ? (ix < 1 ? 1 : ix > LAM_MAX_LADO ? LAM_MAX_LADO : ix) : P.img.an;
		int al = asa_modo != 1 ? (iy < 1 ? 1 : iy > LAM_MAX_LADO ? LAM_MAX_LADO : iy) : P.img.al;

		zona_suma(z, 0, 0, (an > asa_an ? an : asa_an) + 2, (al > asa_al ? al : asa_al) + 2);
		asa_an = an;
		asa_al = al;
		asa_mx = s->x;			/* (PINTURA32) */
		asa_my = s->y;
		pin_aviso("Tamaño del lienzo: %d x %d píxeles (suelte para aplicarlo)", an, al);
		pin_repinta_todo();
		return;
	}
	if (moviendo) {
		zona_sel(z);
		P.sel.x = ix - mx0;
		P.sel.y = iy - my0;
		zona_sel(z);
		repinta(z);
		return;
	}
	if (moviendo_texto) {
		zona_texto(z);
		P.tx.x = ix - mx0;
		P.tx.y = iy - my0;
		zona_texto(z);
		repinta(z);
		return;
	}
	if (!arrastra)
		return;
	switch (P.herr) {
	case H_LAPIZ:
		trazo_lapiz(&P.img, tinta, lx, ly, ix, iy, z);
		break;
	case H_PINCEL:
	case H_BORRADOR: {
		pincel p;

		pincel_de(&p);
		trazo(&P.img, &p, lfx, lfy, fx, fy, z);
		break;
	}
	case H_AEROGRAFO:
		aerografo(ix, iy, z);
		quietos = 0;
		break;
	case H_RAYA:
	case H_RECTANGULO:
	case H_REDONDEADO:
	case H_ELIPSE:
		his_restaura(z);
		figura(ix, iy, (s->mods & SO_CON_MAYUS) != 0, z);
		break;
	case H_CURVA:
		if (fase_curva == 0) {
			cur[2] = cur[4] = cur[6] = ix;
			cur[3] = cur[5] = cur[7] = iy;
			cur[2] = cur[0];
			cur[3] = cur[1];
		} else if (fase_curva == 1) {
			cur[2] = cur[4] = ix;
			cur[3] = cur[5] = iy;
		} else {
			cur[4] = ix;
			cur[5] = iy;
		}
		his_restaura(z);
		pinta_curva(z);
		break;
	case H_POLIGONO:
		pol[2 * n_pol - 2] = ix;
		pol[2 * n_pol - 1] = iy;
		his_restaura(z);
		pinta_poligono(0, z);
		break;
	case H_SELECCION:
		if (P.sel.activa)
			zona_sel(z);
		sel_rect(ax, ay, ix, iy);
		zona_sel(z);
		break;
	case H_LIBRE:
		libre_mete(ix, iy);
		zona_suma(z, (ix < lx ? ix : lx) - 2, (iy < ly ? iy : ly) - 2, (ix > lx ? ix : lx) + 3, (iy > ly ? iy : ly) + 3);
		break;
	}
	lx = ix;
	ly = iy;
	lfx = fx;
	lfy = fy;
	repinta(z);
}

void herr_sube(const so_suceso *s)
{
	int z[4] = { 0, 0, 0, 0 };

	(void)s;
	if (asa_modo) {
		int an = asa_an, al = asa_al;
		lamina n, antes;

		asa_modo = 0;
		asa_an = asa_al = 0;
		asa_encima = 0;
		if (an != P.img.an || al != P.img.al) {
			/* (PINTURA32) lo añadido, con el color 2; o transparente, si el
			 * borrador está en «Deja transparente» */
			if (!op_lienzo(&P.img, &n, an, al, P.borra_transparente ? 0x00000000u : P.c2)) {
				dlg_aviso("Pintura", "No hay memoria para un lienzo tan grande.");
				return;
			}
			antes = P.img;
			P.img = n;
			his_lamina(&P.img, 0);
			his_entera("Tamaño del lienzo", &antes);
			P.cambiado = 1;
			pin_cambio_imagen();
			pin_aviso("Lienzo de %d x %d", an, al);
			dice("pintura: lienzo de %d x %d", an, al);
		}
		pin_repinta_todo();
		return;
	}
	if (moviendo) {
		moviendo = 0;
		dice("pintura: selección movida a %d, %d", P.sel.x, P.sel.y);
		return;
	}
	if (moviendo_texto) {
		moviendo_texto = 0;
		return;
	}
	if (!arrastra)
		return;
	arrastra = 0;
	switch (P.herr) {
	case H_LAPIZ:
	case H_PINCEL:
	case H_BORRADOR:
	case H_AEROGRAFO:
		his_acaba(herr_nombre(P.herr));
		P.cambiado = 1;
		dice("pintura: %s", herr_nombre(P.herr));
		break;
	case H_RAYA:
	case H_RECTANGULO:
	case H_REDONDEADO:
	case H_ELIPSE:
		his_acaba(herr_nombre(P.herr));
		P.cambiado = 1;
		dice("pintura: %s de %d, %d a %d, %d", herr_nombre(P.herr), ax, ay, lx, ly);
		break;
	case H_CURVA:
		if (++fase_curva >= 3) {
			curva_acaba();
			dice("pintura: curva");
		}
		break;
	case H_SELECCION:
		if (P.sel.activa)
			dice("pintura: elegido %d, %d, %d x %d", P.sel.x, P.sel.y, P.sel.an, P.sel.al);
		break;
	case H_LIBRE: {
		seleccion_t *q = &P.sel;
		int i, x0, y0, x1, y1;

		if (q->n_libre < 3) {
			suelta_libre();
			break;
		}
		x0 = x1 = q->libre[0];
		y0 = y1 = q->libre[1];
		for (i = 1; i < q->n_libre; i++) {
			int x = q->libre[2 * i], y = q->libre[2 * i + 1];

			x0 = x < x0 ? x : x0;
			x1 = x > x1 ? x : x1;
			y0 = y < y0 ? y : y0;
			y1 = y > y1 ? y : y1;
		}
		q->x = x0;
		q->y = y0;
		q->an = x1 - x0 + 1;
		q->al = y1 - y0 + 1;
		q->activa = 1;
		zona_sel(z);
		dice("pintura: elegido a mano alzada %d, %d, %d x %d (%d puntos)", q->x, q->y, q->an, q->al, q->n_libre);
		break;
	}
	}
	repinta(z);
}

/* el aerógrafo sigue rociando sin moverse (lo llama la ventana cuando no
 * hay sucesos); 1 si ha hecho algo */
int herr_quieto(void)
{
	int z[4] = { 0, 0, 0, 0 };

	if (!arrastra || P.herr != H_AEROGRAFO || quietos > 60)
		return 0;
	quietos++;
	aerografo(lx, ly, z);
	repinta(z);
	return 1;
}

int herr_tecla_caja(const so_suceso *s)
{
	u32 k = s->tecla;
	int z[4] = { 0, 0, 0, 0 };

	if (P.tx.activa) {
		caja_texto *t = &P.tx;

		if (k == SO_ESC) {
			texto_anula();
			pin_aviso("Texto anulado");
			return 1;
		}
		if (k == SO_INTRO) {
			if (t->n < sizeof t->t / sizeof t->t[0])
				t->t[t->n++] = '\n';
			herr_texto_rehaz();
			return 1;
		}
		if (k == SO_RETROCESO) {
			if (t->n)
				t->n--;
			herr_texto_rehaz();
			return 1;
		}
		if (k == 22 && (s->mods & SO_CON_CONTROL)) {
			/* Ctrl+V: el texto del portapapeles */
			u32 n, m, i, c;
			char *p = so_del_portapapeles(&n);

			if (p) {
				for (i = 0; i < n && t->n < sizeof t->t / sizeof t->t[0];) {
					m = (u32)utf8_lee((u8 *)p + i, n - i, &c);
					i += m;
					if (c == '\r')
						continue;
					t->t[t->n++] = c;
				}
				so_suelta(p);
				herr_texto_rehaz();
			}
			return 1;
		}
		if (k >= ' ' && k < SO_T_BASE && !(s->mods & SO_CON_CONTROL)) {
			if (t->n < sizeof t->t / sizeof t->t[0])
				t->t[t->n++] = k;
			herr_texto_rehaz();
			return 1;
		}
		if (k == SO_TAB) {
			if (t->n + 4 < sizeof t->t / sizeof t->t[0]) {
				int i;

				for (i = 0; i < 4; i++)
					t->t[t->n++] = ' ';
			}
			herr_texto_rehaz();
			return 1;
		}
		return 0;
	}
	if (pol_activo) {
		if (k == SO_INTRO) {
			poligono_cierra();
			return 1;
		}
		if (k == SO_ESC) {
			his_cancela(z);
			pol_activo = 0;
			n_pol = 0;
			repinta(z);
			pin_aviso("Polígono anulado");
			return 1;
		}
	}
	if (fase_curva) {
		if (k == SO_INTRO) {
			curva_acaba();
			return 1;
		}
		if (k == SO_ESC) {
			his_cancela(z);
			fase_curva = 0;
			repinta(z);
			return 1;
		}
	}
	if (P.sel.activa) {
		int dx = k == SO_T_IZQUIERDA ? -1 : k == SO_T_DERECHA ? 1 : 0, dy = k == SO_T_ARRIBA ? -1 : k == SO_T_ABAJO ? 1 : 0;

		if (k == SO_ESC) {
			sel_nada();
			return 1;
		}
		if (dx || dy) {
			if (!P.sel.flota)
				sel_levanta(0);
			zona_sel(z);
			P.sel.x += dx * ((s->mods & SO_CON_MAYUS) ? 10 : 1);
			P.sel.y += dy * ((s->mods & SO_CON_MAYUS) ? 10 : 1);
			zona_sel(z);
			repinta(z);
			return 1;
		}
	}
	return 0;
}
