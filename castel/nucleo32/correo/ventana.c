/* ventana.c - Correo: la ventana. Tres paneles: las carpetas de cada cuenta
 * (con los sin leer en negrita y con su número), la lista (con sus columnas,
 * ordenar con ▲/▼, hilos si se quiere, buscar en el servidor y en lo bajado)
 * y el mensaje (vista.c); si la ventana es estrecha (de 900 puntos para
 * abajo), dos: la lista o el mensaje, con «Volver a la lista». La barra de
 * botones, la de abajo (lo que pasa: conectado y cifrado, o sin conexión) y
 * el aviso del correo nuevo (IDLE, o cada tantos minutos). Y /BANDEJA: sin
 * ventana grande, mira de vez en cuando y avisa con una ventanita abajo a la
 * derecha.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "ventana.h"

int van = 1000, val = 560;
Sesion *ses;
int cuenta_abierta = -1;
void (*repinta_fondo)(void);

/* la lista que se ve: punteros a las fichas del buzón (o lo encontrado) */
static Ficha **lista;
static u8 *hondo;		/* (hilos) cuánto va metido cada uno */
static u32 nlista, clista;
static u32 cursor, primera;
static u8 *elegidos;		/* (Control y Mayús: varios) */
static int buscando;		/* la lista es lo encontrado */
static char buscado[200];
static int panel = 1;		/* el que tiene el teclado: 0 carpetas, 1 lista, 2 mensaje, 3 buscar */
static int solo_mensaje;	/* (estrecha) se ve el mensaje en vez de la lista */
static int completo;		/* (1.0.2) el mensaje, a toda la ventana (dos clics o Intro en la lista) */
static Campo busca;
static char estado[400];
static int nuevos_aviso;	/* lo nuevo que se avisa arriba */
static u32 aviso_hasta;
static int x_car, an_car, x_lis, an_lis, x_men, an_men, y_pan, al_pan;
static int dos_paneles;
static int fila_car[MAX_CARPETAS * 2];	/* (las carpetas que se ven: a qué cuenta y carpeta) */

#define ALTO_BARRA 38
#define ALTO_ABAJO 22
#define ALTO_CAB 22		/* la cabecera de las columnas */

void di_estado(const char *f, ...)
{
	va_list a;

	va_start(a, f);
	vsnprintf(estado, sizeof estado, f, a);
	va_end(a);
}

/* ---- la lista: ordenar y los hilos ---- */
static int col_orden(void) { return ajustes.orden_col; }	/* 0 de, 1 asunto, 2 fecha, 3 tamaño */

static int compara(const Ficha *a, const Ficha *b)
{
	int r;

	switch (col_orden()) {
	case 0: r = strcmp(a->de, b->de); break;
	case 1: {
		const char *x = a->asunto, *y = b->asunto;

		/* (sin los «Re:» delante) */
		while ((x[0] | 32) == 'r' && (x[1] | 32) == 'e' && x[2] == ':')
			x += x[3] == ' ' ? 4 : 3;
		while ((y[0] | 32) == 'r' && (y[1] | 32) == 'e' && y[2] == ':')
			y += y[3] == ' ' ? 4 : 3;
		r = strcmp(x, y);
		break;
	}
	case 3: r = a->tam < b->tam ? -1 : a->tam > b->tam; break;
	default: r = a->fecha < b->fecha ? -1 : a->fecha > b->fecha;
	}
	if (!r)
		r = a->uid < b->uid ? -1 : a->uid > b->uid;
	return ajustes.orden_baja ? -r : r;
}

/* ordenar por mezcla (estable, sin recursión) */
static void ordena(Ficha **v, u32 n)
{
	Ficha **t = so_pide((n + 1) * sizeof(Ficha *));
	u32 w, i;

	if (!t)
		return;
	for (w = 1; w < n; w *= 2) {
		for (i = 0; i < n; i += 2 * w) {
			u32 a = i, m = i + w < n ? i + w : n, z = i + 2 * w < n ? i + 2 * w : n, b = m, k = i;

			while (a < m && b < z)
				t[k++] = compara(v[a], v[b]) <= 0 ? v[a++] : v[b++];
			while (a < m)
				t[k++] = v[a++];
			while (b < z)
				t[k++] = v[b++];
		}
		memcpy(v, t, n * sizeof(Ficha *));
	}
	so_suelta(t);
}

static int sitio_lista(u32 n)
{
	if (n > clista) {
		Ficha **q = so_repide(lista, (n + 64) * sizeof(Ficha *));
		u8 *h, *e;

		if (!q)
			return -1;
		lista = q;
		h = so_repide(hondo, n + 64);
		e = so_repide(elegidos, n + 64);
		if (!h || !e)
			return -1;
		hondo = h;
		elegidos = e;
		clista = n + 64;
	}
	return 0;
}

/* los hilos: cada mensaje debajo del que contesta (por In-Reply-To y
 * References), los hilos por lo más nuevo de cada uno */
static void hilos(void)
{
	u32 n = nlista, i, *tabla, cabe = 1, *raiz, *ult;
	Ficha **v;
	u8 *h;

	while (cabe < n * 2)
		cabe *= 2;
	tabla = so_pide(cabe * sizeof(u32));
	raiz = so_pide((n + 1) * sizeof(u32));
	ult = so_pide((n + 1) * sizeof(u32));
	v = so_pide((n + 1) * sizeof(Ficha *));
	h = so_pide(n + 1);
	if (!tabla || !raiz || !ult || !v || !h)
		goto fin;
	memset(tabla, 0xFF, cabe * sizeof(u32));
	for (i = 0; i < n; i++) {
		u32 k = lista[i]->hilo & (cabe - 1);

		while (tabla[k] != 0xFFFFFFFF)
			k = (k + 1) & (cabe - 1);
		tabla[k] = i;
	}
	/* la raíz y lo hondo de cada uno */
	for (i = 0; i < n; i++) {
		u32 x = i, d = 0;

		while (lista[x]->padre && d < 30) {
			u32 k = lista[x]->padre & (cabe - 1), y = 0xFFFFFFFF;

			while (tabla[k] != 0xFFFFFFFF) {
				if (lista[tabla[k]]->hilo == lista[x]->padre) {
					y = tabla[k];
					break;
				}
				k = (k + 1) & (cabe - 1);
			}
			if (y == 0xFFFFFFFF || y == x)
				break;
			x = y;
			d++;
		}
		raiz[i] = x;
		h[i] = (u8)(d > 8 ? 8 : d);
	}
	/* lo más nuevo de cada hilo */
	for (i = 0; i < n; i++)
		ult[i] = 0;
	for (i = 0; i < n; i++)
		if (lista[i]->fecha > ult[raiz[i]])
			ult[raiz[i]] = lista[i]->fecha;
	/* las raíces por lo más nuevo (de más a menos); detrás de cada una, lo suyo por fecha */
	{
		u32 k = 0, r, j;
		Ficha **rs = so_pide((n + 1) * sizeof(Ficha *));
		u32 nr = 0;

		if (!rs)
			goto fin;
		for (i = 0; i < n; i++)
			if (raiz[i] == i)
				rs[nr++] = lista[i];
		/* (ordenar las raíces por ult: de inserción por tramos sería lento; se usa la fecha
		 * de lo último de cada una puesta en una copia) */
		for (r = 0; r < nr; r++) {
			u32 best = r;

			(void)best;
		}
		{
			/* ordenar los índices de las raíces por ult[] (de más nuevo a más viejo), con Shell */
			u32 *ri = so_pide((nr + 1) * sizeof(u32)), g, a;

			if (!ri) {
				so_suelta(rs);
				goto fin;
			}
			for (i = 0, a = 0; i < n; i++)
				if (raiz[i] == i)
					ri[a++] = i;
			for (g = 1; g < nr / 3; g = 3 * g + 1)
				;
			for (; g; g /= 3)
				for (i = g; i < nr; i++) {
					u32 x = ri[i];

					for (j = i; j >= g && ult[ri[j - g]] < ult[x]; j -= g)
						ri[j] = ri[j - g];
					ri[j] = x;
				}
			/* lo de cada raíz: los que tienen esa raíz, por fecha (de más viejo a más nuevo) */
			{
				u32 *cab = so_pide((n + 1) * sizeof(u32)), *sig = so_pide((n + 1) * sizeof(u32));

				if (cab && sig) {
					for (i = 0; i < n; i++)
						cab[i] = 0xFFFFFFFF;
					/* (de atrás adelante, para que salgan por el orden de la lista: por fecha) */
					for (i = n; i-- > 0;) {
						sig[i] = cab[raiz[i]];
						cab[raiz[i]] = i;
					}
					for (a = 0; a < nr; a++) {
						u32 q;

						v[k] = lista[ri[a]];
						elegidos[k] = h[ri[a]];
						k++;
						for (q = cab[ri[a]]; q != 0xFFFFFFFF; q = sig[q])
							if (q != ri[a]) {
								v[k] = lista[q];
								elegidos[k] = h[q] ? h[q] : 1;
								k++;
							}
					}
					memcpy(lista, v, k * sizeof(Ficha *));
					memcpy(hondo, elegidos, k);
					memset(elegidos, 0, k);
				}
				so_suelta(cab);
				so_suelta(sig);
			}
			so_suelta(ri);
		}
		so_suelta(rs);
	}
fin:
	so_suelta(tabla);
	so_suelta(raiz);
	so_suelta(ult);
	so_suelta(v);
	so_suelta(h);
}

static u32 uid_cursor(void) { return cursor < nlista ? lista[cursor]->uid : 0; }

/* rehace la lista (del buzón, o de lo encontrado), sin perder el cursor */
static void rehaz_lista(Ficha **de, u32 n)
{
	u32 uid = uid_cursor(), i;
	int orden_viejo = ajustes.orden_col, baja = ajustes.orden_baja;

	if (!ses || !ses->buz_abierto) {
		nlista = 0;
		return;
	}
	if (!de)
		n = ses->buz.n;
	if (sitio_lista(n + 1))
		return;
	for (i = 0; i < n; i++)
		lista[i] = de ? de[i] : &ses->buz.f[i];
	nlista = n;
	memset(hondo, 0, n + 1);
	memset(elegidos, 0, n + 1);
	if (ajustes.hilos && !de) {
		/* por fecha, y luego los hilos */
		ajustes.orden_col = 2;
		ajustes.orden_baja = 0;
		ordena(lista, nlista);
		ajustes.orden_col = orden_viejo;
		ajustes.orden_baja = baja;
		hilos();
	} else
		ordena(lista, nlista);
	cursor = 0;
	for (i = 0; i < nlista; i++)
		if (lista[i]->uid == uid)
			cursor = i;
	if (primera > cursor)
		primera = cursor;
}

/* ---- abrir una cuenta y una carpeta ---- */
static void elige_carpeta(int k, int al_dia);

static void abre_cuenta(int i)
{
	if (i < 0 || i >= n_cuentas)
		return;
	vista_cierra();
	if (ses) {
		motor_cierra(ses);
	} else {
		ses = so_pide(sizeof(Sesion));
		if (!ses)
			return;
	}
	motor_abre(ses, i);
	cuenta_abierta = i;
	nlista = 0;
	buscando = 0;
	red_espera = espera_red;
	if (!ses->clave[0]) {
		char t[300];

		snprintf(t, sizeof t, "La contrase\xc3\xb1" "a de %s (no se guarda):", ses->c->correo);
		if (!pide_texto("Contrase\xc3\xb1" "a", t, ses->clave, sizeof ses->clave, 1))
			ses->clave[0] = 0;
	}
	if (ses->clave[0] && ses->c->tipo == CT_IMAP) {
		cancelado = 0;
		progreso("Conectando...", 0, 0);
		if (motor_conecta(ses) == 0)
			motor_carpetas(ses);
		else
			di_estado("Sin conexi\xc3\xb3n (%s): se ve lo guardado.", ses->error);
	}
	{
		int e = motor_carpeta_especial(ses, CM_ENTRADA);

		if (e >= 0)
			elige_carpeta(e, ses->clave[0] != 0);
	}
}

static void avance(void *u, u32 hecho, u32 total, const char *que)
{
	(void)u;
	rehaz_lista(NULL, 0);
	repinta_todo();
	progreso(que, hecho, total);
}

static void elige_carpeta(int k, int al_dia)
{
	if (!ses || k < 0 || k >= ses->n_car)
		return;
	if (ses->car[k].marcas & CM_NOSELECT)
		return;
	vista_cierra();
	buscando = 0;
	solo_mensaje = 0;
	cancelado = 0;
	red_espera = espera_red;
	ses->avance = avance;
	ses->avance_u = NULL;
	if (ses->buz_abierto && ses->sel != k)
		nlista = 0;
	if (motor_elige(ses, k, 0) == 0) {
		cursor = primera = 0;
		rehaz_lista(NULL, 0);
		repinta_todo();
		if (al_dia && (ses->conectada || ses->c->tipo == CT_POP3) && motor_al_dia(ses) < 0 && ses->error[0])
			di_estado("%s", ses->error);
		else if (al_dia)
			di_estado("%s", ses->estado);
		rehaz_lista(NULL, 0);
		if (!ses->conectada && ses->c->tipo == CT_IMAP)
			di_estado("Sin conexi\xc3\xb3n: se ve lo guardado de %s (%u mensajes).", ses->car[k].nombre, (unsigned)ses->buz.n);
	}
	so_apunta("carpeta: %s (%u en la lista)", ses->car[k].nombre, (unsigned)nlista);
}

static void recibir(void)
{
	if (!ses)
		return;
	cancelado = 0;
	red_espera = espera_red;
	if (!ses->conectada && ses->c->tipo == CT_IMAP) {
		if (!ses->clave[0] && !pide_texto("Contrase\xc3\xb1" "a", "La contrase\xc3\xb1" "a:", ses->clave, sizeof ses->clave, 1))
			return;
		progreso("Conectando...", 0, 0);
		if (motor_conecta(ses)) {
			di_estado("Sin conexi\xc3\xb3n: %s", ses->error);
			return;
		}
		motor_carpetas(ses);
	} else if (ses->c->tipo == CT_IMAP)
		motor_carpetas(ses);
	if (ses->sel >= 0) {
		if (motor_al_dia(ses) < 0 && ses->error[0])
			di_estado("%s", ses->error);
		else
			di_estado("%s", ses->estado);
	}
	rehaz_lista(NULL, 0);
}

/* ---- lo elegido ---- */
static int elegidas(Ficha ***v)
{
	u32 i, n = 0;
	Ficha **r;

	*v = NULL;
	for (i = 0; i < nlista; i++)
		if (elegidos[i])
			n++;
	if (!n && cursor < nlista)
		n = 1;
	if (!n)
		return 0;
	r = so_pide(n * sizeof(Ficha *));
	if (!r)
		return 0;
	n = 0;
	for (i = 0; i < nlista; i++)
		if (elegidos[i])
			r[n++] = lista[i];
	if (!n)
		r[n++] = lista[cursor];
	*v = r;
	return (int)n;
}

static void tras_quitar(void)
{
	u32 c = cursor;

	vista_cierra();
	if (buscando)
		buscando = 0;
	rehaz_lista(NULL, 0);
	cursor = c < nlista ? c : nlista ? nlista - 1 : 0;
}

static void borrar(void)
{
	Ficha **v;
	int n = elegidas(&v), r, p;
	char t[200];

	if (!n)
		return;
	p = motor_carpeta_especial(ses, CM_PAPELERA);
	if (p < 0 || p == ses->sel) {
		snprintf(t, sizeof t, "\xc2\xbf" "Borrar del todo %d mensaje%s? No se pueden recuperar.", n, n > 1 ? "s" : "");
		if (pregunta("Borrar", t, "Borrar", "No", NULL) != 0) {
			so_suelta(v);
			return;
		}
	}
	cancelado = 0;
	r = motor_borra(ses, v, n);
	so_suelta(v);
	if (r)
		di_estado("No se ha podido borrar: %s", ses->error);
	else
		di_estado(p >= 0 && p != ses->sel ? "%d a la papelera." : "%d borrados.", n);
	tras_quitar();
}

static void mover(void)
{
	Ficha **v;
	int n = elegidas(&v), i, k = 0, mapa[MAX_CARPETAS], e;
	char *op;

	if (!n)
		return;
	op = so_pide(MAX_CARPETAS * 210);
	if (!op) {
		so_suelta(v);
		return;
	}
	op[0] = 0;
	for (i = 0; i < ses->n_car; i++) {
		if (i == ses->sel || (ses->car[i].marcas & CM_NOSELECT))
			continue;
		if (k)
			strcat(op, "\n");
		strcat(op, ses->car[i].nombre);
		mapa[k++] = i;
	}
	e = elige("Mover a la carpeta", op, 0);
	so_suelta(op);
	if (e >= 0) {
		cancelado = 0;
		if (motor_mueve(ses, v, n, mapa[e]))
			di_estado("No se ha podido mover: %s", ses->error);
		else
			di_estado("%d movido%s a %s.", n, n > 1 ? "s" : "", ses->car[mapa[e]].nombre);
		tras_quitar();
	}
	so_suelta(v);
}

static void marca(u32 b)
{
	Ficha **v;
	int n = elegidas(&v), poner;

	if (!n)
		return;
	poner = !(v[0]->banderas & b);
	motor_marca(ses, v, n, b, poner);
	so_apunta("marcados %d: %s %s", n, b == B_LEIDO ? "le\xc3\xad" "do" : "destacado", poner ? "s\xc3\xad" : "no");
	so_suelta(v);
}

static void abre_mensaje(void)
{
	Ficha *f;

	if (cursor >= nlista)
		return;
	f = lista[cursor];
	vista_abre(f);
	if (vista_hay()) {
		f = buzon_busca(&ses->buz, uid_abierto());
		if (f && !(f->banderas & B_LEIDO))
			motor_marca(ses, &f, 1, B_LEIDO, 1);
		if (dos_paneles)
			solo_mensaje = 1;
		panel = 2;
		if (f && (ses->car[ses->sel].marcas & CM_BORRADORES))
			di_estado("Es un borrador: \xc2\xab" "Seguir\xc2\xbb para seguir escribi\xc3\xa9ndolo.");
	}
}

/* (1.0.2) dos clics o Intro en la lista: el mensaje a toda la ventana, con
 * sus botones; Esc (o «< Volver a la lista») vuelve, con el mismo elegido */
static int panel_antes = 1;

static void abre_completo(void)
{
	if (!completo)
		panel_antes = panel == 2 ? 2 : 1;
	abre_mensaje();
	if (!vista_hay())
		return;
	completo = 1;
	solo_mensaje = 0;
	panel = 2;
	so_apunta("mensaje a toda la ventana: %u (fila %u de %u)", (unsigned)uid_abierto(), (unsigned)cursor + 1, (unsigned)nlista);
}

static void vuelve_lista(void)
{
	completo = 0;
	panel = panel_antes;	/* (el que tenía el teclado antes: así queda todo como estaba) */
	so_apunta("vuelta a la lista: el elegido, %u (fila %u; la primera que se ve, %u)", (unsigned)uid_cursor(), (unsigned)cursor + 1,
		  (unsigned)primera + 1);
}

/* el anterior o el siguiente de la lista, a toda la ventana */
static void otro_completo(int paso)
{
	if ((paso < 0 && cursor == 0) || (paso > 0 && cursor + 1 >= nlista))
		return;
	cursor = (u32)((int)cursor + paso);
	memset(elegidos, 0, nlista);
	abre_completo();
}

static void haz_busca(void)
{
	char t[200];
	Ficha **v;
	int n;

	campo_saca(&busca, t, sizeof t);
	if (!t[0] || !ses || !ses->buz_abierto) {
		buscando = 0;
		rehaz_lista(NULL, 0);
		return;
	}
	cancelado = 0;
	red_espera = espera_red;
	if (ses->conectada && !motor_busca(ses, t, 0, &v, &n))
		di_estado("En el servidor, \xc2\xab%s\xc2\xbb: %d.", t, n);
	else if (!motor_busca_local(ses, t, &v, &n))
		di_estado("En lo bajado (sin conexi\xc3\xb3n), \xc2\xab%s\xc2\xbb: %d.", t, n);
	else
		return;
	snprintf(buscado, sizeof buscado, "%s", t);
	buscando = 1;
	rehaz_lista(v, (u32)n);
	so_suelta(v);
	cursor = primera = 0;
}

/* ---- pintar ---- */
static void medidas(void)
{
	y_pan = ALTO_BARRA;
	al_pan = val - ALTO_BARRA - ALTO_ABAJO;
	dos_paneles = van < 900;
	an_car = van < 700 ? 150 : van < 1100 ? 180 : 210;
	x_car = 0;
	x_lis = an_car + 2;
	if (dos_paneles) {
		an_lis = van - x_lis;
		x_men = x_lis;
		an_men = an_lis;
	} else {
		an_lis = (van - x_lis) * 48 / 100;
		if (an_lis < 360)
			an_lis = 360;
		x_men = x_lis + an_lis + 2;
		an_men = van - x_men;
	}
}

static const char *const botones[] = { "Recibir", "Escribir", "Responder", "A todos", "Reenviar", "Borrar", "Mover...", "Destacar",
	"Le\xc3\xad" "do", "Libreta", "M\xc3\xa1s..." };

static void pinta_barra(void)
{
	int x = 8, i, apagado;
	int hay = vista_hay() || cursor < nlista;

	ui_degradado(0, 0, van, ALTO_BARRA, 0xF9FAFB, 0xE5E7EB);
	ui_rect(0, ALTO_BARRA - 1, van, 1, C_GRIS);
	for (i = 0; i < 11; i++) {
		int w = ancho_texto(botones[i]) + 16;

		/* lo que no cabe, en «Más...» */
		if (i < 10 && x + w + ancho_texto(botones[10]) + 30 > van)
			continue;
		apagado = !ses || ((i >= 2 && i <= 4) && !vista_hay()) || ((i >= 5 && i <= 8) && !hay);
		boton(500 + i, x, 6, w, botones[i], i == 1, apagado, 0);
		x += w + 4;
	}
}

static void pinta_carpetas(void)
{
	int y = y_pan + 4, f = 0, c, i;

	ui_rect(x_car, y_pan, an_car, al_pan, C_BARRA);
	ui_rect(x_car + an_car, y_pan, 2, al_pan, C_GRIS);
	for (c = 0; c < n_cuentas && y < y_pan + al_pan - FILA; c++) {
		char t[200];

		snprintf(t, sizeof t, "%s", cuentas[c].nombre[0] ? cuentas[c].nombre : cuentas[c].correo);
		ui_rect(x_car, y, an_car, FILA, c == cuenta_abierta ? C_AZUL_CLARO : C_BARRA);
		if (c == cuenta_abierta && ses && ses->conectada)
			icono(ses->imap.con.cifrado[0] ? IC_CANDADO : IC_CANDADO_ABIERTO, x_car + 4, y + 4,
			      ses->imap.con.cifrado[0] ? C_VERDE : C_ROJO, C_AZUL_CLARO);
		negrita(x_car + 20, y + 2, t, (an_car - 24) / LA, C_TINTA);
		zona(1000 + c, x_car, y, an_car, FILA);
		fila_car[f++] = -1 - c;
		y += FILA;
		if (c != cuenta_abierta || !ses)
			continue;
		for (i = 0; i < ses->n_car && y < y_pan + al_pan - FILA; i++) {
			Carpeta *k = &ses->car[i];
			const char *n = k->nombre;
			int sx = x_car + 12 + k->nivel * 10, el = i == ses->sel;
			u32 fondo = el ? C_ELEGIDO : C_BARRA, tinta = el ? C_PAPEL : (k->marcas & CM_NOSELECT) ? C_SUAVE : C_TINTA;

			if (k->nivel && k->delim) {
				const char *b = strrchr(n, k->delim);

				if (b)
					n = b + 1;
			}
			ui_rect(x_car, y, an_car, FILA, fondo);
			if (el)
				negrita(x_car + 2, y + 2, ">", 1, C_PAPEL);
			if (k->no_leidos) {
				/* el nombre en negrita y, a la derecha, el número entre paréntesis */
				int w;

				snprintf(t, sizeof t, "(%u)", (unsigned)k->no_leidos);
				w = ancho_texto(t);
				negrita(x_car + an_car - w - 6, y + 2, t, 12, tinta);
				negrita(sx, y + 2, n, (x_car + an_car - sx - w - 12) / LA, tinta);
			} else
				texto(sx, y + 2, n, (x_car + an_car - sx - 4) / LA, tinta);
			zona(1100 + i, x_car, y, an_car, FILA);
			y += FILA;
		}
	}
	if (y < y_pan + al_pan - ALTO_BOTON - 8)
		boton(1050, x_car + 8, y + 8, an_car - 16, "+ Cuenta nueva", 0, 0, 0);
}

static int filas_lista(void) { return (al_pan - ALTO_CAB - 32) / FILA; }

static void pinta_lista(void)
{
	int x = x_lis, y = y_pan, an = an_lis, filas, i, ancho_de, ancho_fe = 13 * LA, ancho_ta = an > 560 ? 7 * LA : 0, xa, xf, xt;
	static const char *const cols[] = { "De", "Asunto", "Fecha", "Tama\xc3\xb1o" };
	char t[300];

	ui_rect(x, y, an, al_pan, C_PAPEL);
	/* buscar */
	ui_rect(x, y, an, 30, C_FONDO);
	campo_pinta(&busca, x + 4, y + 3, an - 98 - (buscando ? 0 : 0), panel == 3);
	zona(600, x + 4, y + 3, an - 98, 24);
	boton(601, x + an - 90, y + 2, 86, buscando ? "Quitar" : "Buscar", 0, 0, 0);
	y += 30;
	/* las columnas */
	ancho_de = (an - 44 - ancho_fe - ancho_ta) * 35 / 100;
	xa = x + 44 + ancho_de;
	xf = x + an - ancho_fe - ancho_ta - 10;
	xt = x + an - ancho_ta - 10;
	ui_rect(x, y, an, ALTO_CAB, 0xE5E7EB);
	ui_rect(x, y + ALTO_CAB - 1, an, 1, C_GRIS);
	{
		int xs[4] = { x + 44, xa, xf, xt }, i2;

		for (i2 = 0; i2 < 4; i2++) {
			if (i2 == 3 && !ancho_ta)
				continue;
			negrita(xs[i2] + 2, y + 3, cols[i2], 10, C_TINTA);
			if (col_orden() == i2)
				icono(ajustes.orden_baja ? IC_BAJA : IC_SUBE, xs[i2] + 2 + ancho_texto(cols[i2]) + 4, y + 5, C_TINTA, 0xE5E7EB);
			zona(610 + i2, xs[i2], y, (i2 < 3 ? (i2 == 0 ? xa : i2 == 1 ? xf : xt) : x + an) - xs[i2], ALTO_CAB);
		}
		if (ajustes.hilos)
			texto(x + 4, y + 3, "hilos", 5, C_SUAVE);
	}
	y += ALTO_CAB;
	filas = filas_lista();
	if (cursor >= nlista && nlista)
		cursor = nlista - 1;
	if (cursor < primera)
		primera = cursor;
	if (cursor >= primera + (u32)filas)
		primera = cursor - (u32)filas + 1;
	if (buscando) {
		snprintf(t, sizeof t, "Lo encontrado de \xc2\xab%s\xc2\xbb: %u", buscado, (unsigned)nlista);
		ui_rect(x, y, an, FILA, C_AMBAR_CLARO);
		negrita(x + 6, y + 2, t, (an - 12) / LA, C_TINTA);
		y += FILA;
		filas--;
	}
	if (!nlista)
		texto(x + 12, y + 8, ses && ses->buz_abierto ? (buscando ? "No se ha encontrado nada." : "No hay mensajes.") : "", (an - 24) / LA, C_SUAVE);
	for (i = 0; i < filas && primera + (u32)i < nlista; i++) {
		u32 k = primera + (u32)i;
		Ficha *f = lista[k];
		int fy = y + i * FILA, el = k == cursor, mar = elegidos[k], neg = !(f->banderas & B_LEIDO);
		u32 fondo = el ? (panel == 1 ? C_ELEGIDO : 0x93C5FD) : mar ? C_AZUL_CLARO : (k & 1) ? 0xF9FAFB : C_PAPEL;
		u32 tinta = el && panel == 1 ? C_PAPEL : C_TINTA;
		char fe[40];
		int sx;

		ui_rect(x, fy, an, FILA, fondo);
		if (el)
			ui_rect(x, fy, 3, FILA, el && panel == 1 ? C_PAPEL : C_ELEGIDO);
		/* los dibujitos: sin leer (un sobre), respondido, destacado (estrella), adjunto (clip), aviso */
		if (neg)
			icono(IC_SOBRE, x + 4, fy + 4, tinta, fondo);
		else if (f->banderas & B_RESPONDIDO)
			icono(IC_RESPONDIDO, x + 4, fy + 4, tinta, fondo);
		icono((f->banderas & B_DESTACADO) ? IC_ESTRELLA : IC_ESTRELLA_VACIA, x + 17, fy + 4,
		      (f->banderas & B_DESTACADO) ? (el && panel == 1 ? C_PAPEL : 0xB45309) : (el && panel == 1 ? 0xBFDBFE : C_GRIS), fondo);
		zona(700000 + (int)k, x + 15, fy, 14, FILA);
		if (f->banderas & B_SUPLANTA)
			icono(IC_AVISO, x + 30, fy + 4, el && panel == 1 ? C_PAPEL : C_ROJO, fondo);
		else if (f->banderas & B_ADJUNTO)
			icono(IC_CLIP, x + 30, fy + 4, tinta, fondo);
		ui_texto(x + 46, fy + 2, f->de, (u32)strlen(f->de), (xa - x - 50) / LA, tinta, neg);
		sx = xa + 2;
		if (hondo[k]) {
			sx += (hondo[k] > 5 ? 5 : hondo[k]) * 10;
			icono(IC_HILO, sx - 12, fy + 4, tinta, fondo);
		}
		ui_texto(sx, fy + 2, f->asunto[0] ? f->asunto : "(sin asunto)", f->asunto[0] ? (u32)strlen(f->asunto) : 12, (xf - sx - 6) / LA, tinta, neg);
		mime_fecha_corta(f->fecha, so_zona(f->fecha), so_ahora(), fe, sizeof fe);
		ui_texto(xf + 2, fy + 2, fe, (u32)strlen(fe), 13, tinta, neg);
		if (ancho_ta) {
			if (f->tam >= 1024 * 1024)
				snprintf(fe, sizeof fe, "%u,%u MB", (unsigned)(f->tam >> 20), (unsigned)((f->tam % (1 << 20)) * 10 >> 20));
			else
				snprintf(fe, sizeof fe, "%u KB", (unsigned)((f->tam + 1023) / 1024));
			ui_texto(xt + 2, fy + 2, fe, (u32)strlen(fe), 7, tinta, 0);
		}
		zona(100000 + (int)k, x, fy, an, FILA);
	}
	/* la barra de desplazar */
	if (nlista > (u32)filas) {
		int alto = filas * FILA, bh = (int)((u64)alto * (u32)filas / nlista), by;

		if (bh < 16)
			bh = 16;
		by = y + (int)((u64)(alto - bh) * primera / (nlista - (u32)filas));
		ui_rect(x + an - 8, y, 8, alto, C_BARRA);
		ui_rect(x + an - 7, by, 6, bh, C_BORDE);
	}
	if (!dos_paneles)
		ui_rect(x + an, y_pan, 2, al_pan, C_GRIS);
}

static void pinta_abajo(void)
{
	int y = val - ALTO_ABAJO;
	char t[500];

	ui_rect(0, y, van, ALTO_ABAJO, 0xE5E7EB);
	ui_rect(0, y, van, 1, C_GRIS);
	if (ses && ses->conectada && ses->imap.con.cifrado[0]) {
		icono(IC_CANDADO, 6, y + 5, C_VERDE, 0xE5E7EB);
		snprintf(t, sizeof t, "%s", estado[0] ? estado : ses->estado);
	} else if (ses && ses->c->tipo == CT_IMAP && !ses->conectada) {
		icono(IC_CANDADO_ABIERTO, 6, y + 5, C_SUAVE, 0xE5E7EB);
		snprintf(t, sizeof t, "%s", estado[0] ? estado : "Sin conexi\xc3\xb3n: se ve lo guardado.");
	} else
		snprintf(t, sizeof t, "%s", estado);
	texto(22, y + 3, t, (van - 30) / LA, C_TINTA);
}

static void pinta_aviso(void)
{
	char t[200];
	int w;

	if (!nuevos_aviso || (int)(aviso_hasta - so_tics()) <= 0) {
		nuevos_aviso = 0;
		return;
	}
	snprintf(t, sizeof t, "Correo nuevo: %d mensaje%s", nuevos_aviso, nuevos_aviso > 1 ? "s" : "");
	w = ancho_texto(t) + 40;
	ui_rect(van - w - 12, ALTO_BARRA + 6, w, 26, C_VERDE);
	ui_rect(van - w - 10, ALTO_BARRA + 8, w - 4, 22, 0xDCFCE7);
	icono(IC_SOBRE, van - w - 4, ALTO_BARRA + 13, C_VERDE, 0xDCFCE7);
	negrita(van - w + 14, ALTO_BARRA + 11, t, 40, C_VERDE);
}

static void pinta_completo(void)
{
	static const char *const b[] = { "< Volver a la lista", "Responder", "A todos", "Reenviar", "Borrar", "< Anterior", "Siguiente >" };
	int x = 8, i;

	ui_degradado(0, 0, van, ALTO_BARRA, 0xF9FAFB, 0xE5E7EB);
	ui_rect(0, ALTO_BARRA - 1, van, 1, C_GRIS);
	for (i = 0; i < 7; i++) {
		int w = ancho_texto(b[i]) + 16, apagado = (i == 5 && cursor == 0) || (i == 6 && cursor + 1 >= nlista);

		if (x + w > van - 4)
			break;
		boton(660 + i, x, 6, w, b[i], i == 0, apagado, 0);
		x += w + (i == 0 || i == 4 ? 14 : 4);
	}
	vista_pinta(0, ALTO_BARRA, van, val - ALTO_BARRA - ALTO_ABAJO);
	pinta_abajo();
	pinta_aviso();
}

void repinta_todo(void)
{
	medidas();
	zonas_olvida();
	if (completo && vista_hay()) {
		pinta_completo();
		return;
	}
	completo = 0;
	pinta_barra();
	pinta_carpetas();
	if (!dos_paneles || !solo_mensaje)
		pinta_lista();
	if (!dos_paneles || solo_mensaje) {
		if (dos_paneles) {
			ui_rect(x_men, y_pan, an_men, 30, C_FONDO);
			boton(650, x_men + 4, y_pan + 2, 0, "< Volver a la lista", 0, 0, 0);
			vista_pinta(x_men, y_pan + 30, an_men, al_pan - 30);
		} else
			vista_pinta(x_men, y_pan, an_men, al_pan);
	}
	pinta_abajo();
	pinta_aviso();
}

/* ---- el menú «Más...» ---- */
#define M_MAS 3000
#define M_FILA 2000

static void menu_mas(int x, int y)
{
	char t[600];

	snprintf(t, sizeof t, "Cuenta nueva...|Cambiar esta cuenta...|Quitar esta cuenta...|La lista por hilos: %s|Ajustes...|"
			      "Guardar el mensaje como .EML...|Seguir el borrador|Acerca de Correo",
		 ajustes.hilos ? "s\xc3\xad" : "no");
	ui_menu(x, y, t, 0, M_MAS);
}

static void guarda_eml(void)
{
	u32 n;
	const u8 *d = vista_datos(&n);
	char r[260] = "MENSAJE.EML";
	void *f;

	if (!vista_hay() || !d)
		return;
	if (elige_archivo("Guardar el mensaje", r, sizeof r, 1, "EML")) {
		f = so_abre(r, SO_CREAR);
		if (!f || so_escribe(f, d, n) || so_cierra(f))
			aviso("Guardar", "No se ha podido guardar.");
		else
			di_estado("Guardado en %s", r);
	}
}

static void del_menu(u32 etiqueta, u32 op)
{
	if (op == UI_NINGUNA)
		return;
	if (etiqueta >= 1000 && etiqueta < 1100) {
		vista_adjunto((int)(etiqueta - 1000), op == 1);
		return;
	}
	if (etiqueta == M_FILA) {
		switch (op) {
		case 0: abre_mensaje(); break;
		case 1: abre_mensaje(); if (vista_hay()) redacta(RD_RESPONDER, NULL); break;
		case 2: abre_mensaje(); if (vista_hay()) redacta(RD_REENVIAR, NULL); break;
		case 3: marca(B_LEIDO); break;
		case 4: marca(B_DESTACADO); break;
		case 5: mover(); break;
		case 6: borrar(); break;
		}
		return;
	}
	if (etiqueta == M_MAS) {
		switch (op) {
		case 0:
			if (asistente_cuenta(-1))
				abre_cuenta(n_cuentas - 1);
			break;
		case 1:
			if (cuenta_abierta >= 0 && asistente_cuenta(cuenta_abierta))
				abre_cuenta(cuenta_abierta);
			break;
		case 2:
			if (cuenta_abierta >= 0) {
				char t[300];

				snprintf(t, sizeof t, "\xc2\xbfQuitar la cuenta %s de este equipo? (Lo del servidor no se toca.)", cuentas[cuenta_abierta].correo);
				if (pregunta("Quitar la cuenta", t, "Quitarla", "No", NULL) == 0) {
					int i = cuenta_abierta;

					vista_cierra();
					motor_cierra(ses);
					cuenta_quita(i);
					cuentas_guarda();
					cuenta_abierta = -1;
					nlista = 0;
					if (n_cuentas)
						abre_cuenta(0);
				}
			}
			break;
		case 3:
			ajustes.hilos = !ajustes.hilos;
			ajustes_guarda();
			rehaz_lista(NULL, 0);
			break;
		case 4: ventana_ajustes(); rehaz_lista(NULL, 0); break;
		case 5: guarda_eml(); break;
		case 6: if (vista_hay()) redacta(RD_BORRADOR, NULL); break;
		case 7:
			aviso("Acerca de Correo", "Correo " CORREO_VERSION ", el cliente de correo de Hispa-Pentium: IMAP, POP3 y SMTP, siempre "
			      "cifrado (TLS o STARTTLS), con los mensajes guardados para leerlos sin red. Copyright (C) 2026 Carlos Sanz y "
			      "contribuidores. GPL-2.0 o posterior.");
			break;
		}
	}
}

/* ---- el ratón y el teclado ---- */
static void boton_barra(int i)
{
	switch (i) {
	case 0: recibir(); break;
	case 1: redacta(RD_NUEVO, NULL); break;
	case 2: if (vista_hay()) redacta(RD_RESPONDER, NULL); break;
	case 3: if (vista_hay()) redacta(RD_TODOS, NULL); break;
	case 4: if (vista_hay()) redacta(RD_REENVIAR, NULL); break;
	case 5: borrar(); break;
	case 6: mover(); break;
	case 7: marca(B_DESTACADO); break;
	case 8: marca(B_LEIDO); break;
	case 9: { char d[400]; if (ventana_libreta(d, sizeof d)) redacta(RD_NUEVO, d); } break;
	case 10: menu_mas(van - 200, ALTO_BARRA); break;
	}
}

static void raton(int x, int y, u32 c)
{
	int z = zona_en(x, y), veces = (int)(c >> 16);

	if (completo) {
		switch (z) {
		case 660: vuelve_lista(); return;
		case 661: redacta(RD_RESPONDER, NULL); return;
		case 662: redacta(RD_TODOS, NULL); return;
		case 663: redacta(RD_REENVIAR, NULL); return;
		case 664: borrar(); if (cursor < nlista && completo) abre_completo(); else vuelve_lista(); return;
		case 665: otro_completo(-1); return;
		case 666: otro_completo(1); return;
		}
		vista_raton(x, y, c);
		return;
	}

	if (z >= 500 && z < 511) {
		boton_barra(z - 500);
		return;
	}
	if (z >= 1000 && z < 1000 + MAX_CUENTAS) {
		if (z - 1000 != cuenta_abierta)
			abre_cuenta(z - 1000);
		panel = 0;
		return;
	}
	if (z == 1050) {
		if (asistente_cuenta(-1))
			abre_cuenta(n_cuentas - 1);
		return;
	}
	if (z >= 1100 && z < 1100 + MAX_CARPETAS) {
		panel = 0;
		if (ses && z - 1100 != ses->sel)
			elige_carpeta(z - 1100, 1);
		return;
	}
	if (z == 600) {
		panel = 3;
		campo_clic(&busca, x_lis + 4, x);
		return;
	}
	if (z == 601) {
		if (buscando) {
			buscando = 0;
			campo_pon(&busca, "");
			rehaz_lista(NULL, 0);
		} else
			haz_busca();
		return;
	}
	if (z >= 610 && z < 614) {
		if (col_orden() == z - 610)
			ajustes.orden_baja = !ajustes.orden_baja;
		else {
			ajustes.orden_col = z - 610;
			ajustes.orden_baja = z - 610 == 2;
		}
		ajustes_guarda();
		if (!buscando)
			rehaz_lista(NULL, 0);
		else
			ordena(lista, nlista);
		return;
	}
	if (z == 650) {
		solo_mensaje = 0;
		panel = 1;
		return;
	}
	if (z >= 700000) {
		u32 k = (u32)(z - 700000);

		if (k < nlista) {
			Ficha *f = lista[k];

			motor_marca(ses, &f, 1, B_DESTACADO, !(f->banderas & B_DESTACADO));
		}
		return;
	}
	if (z >= 100000 && z < 700000) {
		u32 k = (u32)(z - 100000), i;

		panel = 1;
		if (c & UC_CONTROL)
			elegidos[k] = !elegidos[k];
		else if (c & UC_MAYUS) {
			u32 a = k < cursor ? k : cursor, b = k < cursor ? cursor : k;

			for (i = a; i <= b && i < nlista; i++)
				elegidos[i] = 1;
		} else
			memset(elegidos, 0, nlista);
		cursor = k;
		if (c & UB_DER) {
			ui_menu(x, y, "Abrir|Responder|Reenviar|Le\xc3\xad" "do / sin leer|Destacar|Mover a...|Borrar", 0, M_FILA);
			return;
		}
		if (!(c & (UC_CONTROL | UC_MAYUS)) && veces >= 2)
			abre_completo();		/* (1.0.2) dos clics: a toda la ventana */
		else if (!(c & (UC_CONTROL | UC_MAYUS)) && !dos_paneles) {
			abre_mensaje();
			panel = 1;	/* (1.0.2) el teclado sigue en la lista (las flechas, Intro), como en Thunderbird */
		}
		return;
	}
	if (x >= x_men && (!dos_paneles || solo_mensaje)) {
		panel = 2;
		vista_raton(x, y, c);
	}
}

static void tecla(u32 k, u32 mod)
{
	if (completo) {
		if (k == UT_ESC || k == UT_BORRAR) {
			vuelve_lista();
			return;
		}
		if (k == 0x141) {	/* F7: el anterior */
			otro_completo(-1);
			return;
		}
		if (k == 0x142) {	/* F8: el siguiente */
			otro_completo(1);
			return;
		}
		if (k == UT_SUPR) {
			borrar();
			if (cursor < nlista && completo)
				abre_completo();
			else
				vuelve_lista();
			return;
		}
		if (vista_tecla(k, mod))
			return;
		if (k == 18 || k == 20 || k == 12 || k == UT_F5 || k == 14 || k == 11) {
			/* (Ctrl+R, Ctrl+T, Ctrl+L, F5, Ctrl+N, Ctrl+K: como siempre) */
		} else
			return;
	}
	if (panel == 3) {
		if (k == UT_INTRO) {
			haz_busca();
			panel = 1;
			return;
		}
		if (k == UT_ESC) {
			campo_pon(&busca, "");
			buscando = 0;
			rehaz_lista(NULL, 0);
			panel = 1;
			return;
		}
		if (k != UT_TAB && campo_tecla(&busca, k, mod))
			return;
	}
	switch (k) {
	case UT_F5: recibir(); return;
	case 14: redacta(RD_NUEVO, NULL); return;			/* Ctrl+N */
	case 18: if (vista_hay()) redacta(RD_RESPONDER, NULL); return;	/* Ctrl+R */
	case 20: if (vista_hay()) redacta(RD_TODOS, NULL); return;	/* Ctrl+T */
	case 12: if (vista_hay()) redacta(RD_REENVIAR, NULL); return;	/* Ctrl+L */
	case 6: panel = 3; return;					/* Ctrl+F */
	case 11: if (cursor < nlista) mover(); return;			/* Ctrl+K */
	case UT_TAB: panel = (panel + 1) % 4; if (dos_paneles && panel == 2 && !solo_mensaje) panel = 3; return;
	case UT_MAYUS_TAB: panel = (panel + 3) % 4; return;
	case UT_SUPR: borrar(); return;
	}
	if (k == 's' && panel != 3) {
		marca(B_DESTACADO);
		return;
	}
	if (k == 'm' && panel != 3) {
		marca(B_LEIDO);
		return;
	}
	if (panel == 2 && vista_tecla(k, mod))
		return;
	if (panel == 2 && (k == UT_ESC || k == UT_BORRAR) && dos_paneles) {
		solo_mensaje = 0;
		panel = 1;
		return;
	}
	if (panel == 0 && ses) {
		int s = ses->sel;

		if (k == UT_ARRIBA) {
			do
				s--;
			while (s >= 0 && (ses->car[s].marcas & CM_NOSELECT));
			if (s >= 0)
				elige_carpeta(s, 1);
		}
		if (k == UT_ABAJO) {
			do
				s++;
			while (s < ses->n_car && (ses->car[s].marcas & CM_NOSELECT));
			if (s < ses->n_car)
				elige_carpeta(s, 1);
		}
		if (k == UT_DERECHA || k == UT_INTRO)
			panel = 1;
		return;
	}
	if (panel == 1 || panel == 2) {
		u32 filas = (u32)filas_lista();

		if (k == UT_ARRIBA && cursor > 0)
			cursor--;
		else if (k == UT_ABAJO && cursor + 1 < nlista)
			cursor++;
		else if (k == UT_RE_PAG)
			cursor = cursor > filas ? cursor - filas : 0;
		else if (k == UT_AV_PAG)
			cursor = cursor + filas < nlista ? cursor + filas : nlista ? nlista - 1 : 0;
		else if (k == UT_INICIO)
			cursor = 0;
		else if (k == UT_FIN)
			cursor = nlista ? nlista - 1 : 0;
		else if (k == UT_INTRO)
			abre_completo();	/* (1.0.2) Intro: a toda la ventana */
		else if (k == ' ')
			abre_mensaje();
		else if (k == 1)
			memset(elegidos, 1, nlista);
		else if (k == UT_IZQUIERDA && panel == 1)
			panel = 0;
		else
			return;
		/* (moverse no abre: así no se marca leído sin querer) */
	}
}

/* =============================================================================
 *  La entrada de la ventana
 * ============================================================================= */
static void nada(void) { repinta_todo(); }

int correo_ventana(const char *args)
{
	Suceso s;
	int pa, pal, sucio;
	u32 ult_mira = 0;

	ui_pantalla(&pa, &pal);
	van = pa > 1100 ? 1040 : pa - 8;
	val = pal > 700 ? 680 : pal - 30;
	if (van < 480)
		van = 480;
	if (val < 380)
		val = 380;
	if (ui_abre("Correo", van, val)) {
		so_dice("Correo: hace falta el escritorio.\n");
		return 1;
	}
	if (pa <= 1100)
		ui_maximiza();
	repinta_fondo = nada;
	red_espera = espera_red;
	medidas();
	so_apunta("ventana: %d x %d (%s)", van, val, van < 900 ? "dos paneles" : "tres paneles");
	/* un .EML de Mi equipo, o un mailto: */
	if (args && args[0] && strncmp(args, "mailto:", 7)) {
		u32 n;
		u8 *d = so_lee_archivo(args, &n, 64u << 20);

		if (d) {
			vista_abre_datos(d, n);
			solo_mensaje = 1;
			panel = 2;
			ui_titulo("Correo: mensaje guardado");
			so_apunta("abierto el archivo %s", args);
		}
	}
	if (!n_cuentas) {
		repinta_todo();
		if (asistente_cuenta(-1))
			abre_cuenta(0);
	} else if (!vista_hay())
		abre_cuenta(0);
	if (args && !strncmp(args, "mailto:", 7))
		redacta(RD_NUEVO, args + 7);
	/* (PARPADEO) se repinta solo si algo ha cambiado (no con el ratón que
	 * solo se mueve, ni cada vuelta), y en el lienzo propio: un vuelco */
	sucio = 1;
	for (;;) {
		if (sucio || (nuevos_aviso && (int)(aviso_hasta - so_tics()) <= 0)) {
			repinta_todo();
			sucio = 0;
		}
		if (!ui_espera(&s, 40)) {
			/* nada que hacer: ¿algo nuevo en el servidor? */
			if (ses && ses->conectada && (u32)(so_tics() - ult_mira) > so_hz() / 4) {
				u32 antes = ses->car[ses->sel >= 0 ? ses->sel : 0].no_leidos;

				red_espera = NULL;
				if (motor_mira(ses, 150) > 0) {
					if (ses->nuevos) {
						nuevos_aviso = (int)ses->nuevos;
						aviso_hasta = so_tics() + 20 * so_hz();
						so_apunta("correo nuevo: %u", (unsigned)ses->nuevos);
						{
							char t[80];

							snprintf(t, sizeof t, "Correo (%u sin leer)", (unsigned)ses->car[ses->sel].no_leidos);
							ui_titulo(t);
						}
					}
					rehaz_lista(buscando ? NULL : NULL, 0);
					buscando = 0;
					sucio = 1;
				}
				(void)antes;
				red_espera = espera_red;
				ult_mira = so_tics();
			}
			continue;
		}
		sucio = 1;
		switch (s.clase) {
		case UE_CERRAR:
			goto fin;
		case UE_TAMANO:
			van = (int)s.a;
			val = (int)s.b;
			break;
		case UE_TECLA:
			estado[0] = estado[0] ? estado[0] : 0;
			tecla(s.a, s.b);
			break;
		case UE_BAJA:
			raton((int)s.a, (int)s.b, s.c);
			break;
		case UE_RUEDA: {
			int pasos = (int)(signed char)(s.c & 0xFF);

			if (completo || ((int)s.a >= x_men && (!dos_paneles || solo_mensaje)))
				vista_rueda(pasos);
			else if ((int)s.a >= x_lis) {
				int p = (int)primera - pasos * 3;

				if (p > (int)nlista - filas_lista())
					p = (int)nlista - filas_lista();
				if (p < 0)
					p = 0;
				primera = (u32)p;
				if (cursor < primera)
					cursor = primera;
				if (cursor >= primera + (u32)filas_lista())
					cursor = primera + (u32)filas_lista() - 1;
			}
			break;
		}
		case UE_MENU:
			del_menu(s.b, s.a);
			break;
		}
	}
fin:
	vista_cierra();
	if (ses)
		motor_cierra(ses);
	libreta_guarda();
	ui_cierra();
	so_apunta("ventana: cerrada");
	return 0;
}

/* =============================================================================
 *  /BANDEJA: mirar sin ventana y avisar abajo a la derecha
 * ============================================================================= */
static int aviso_bandeja(const char *t)
{
	Suceso s;
	u32 hasta = so_tics() + 45 * so_hz();
	int an = 380, al = 96, r = 0, pintada = 0;

	if (ui_abre_aviso(an, al))
		return 0;
	van = an;
	val = al;
	so_apunta("bandeja: aviso: %s", t);
	for (;;) {
		zonas_olvida();
		ui_callado(pintada);	/* (PARPADEO) la primera vez, entera; luego, ya está */
		ui_rect(0, 0, an, al, 0xDCFCE7);
		ui_rect(0, 0, an, 3, C_VERDE);
		icono_correo(8, 10);
		negrita(48, 12, "Correo nuevo", 20, C_VERDE);
		texto(48, 32, t, (an - 56) / LA, C_TINTA);
		boton(1, an - 200, al - ALTO_BOTON - 8, 90, "Leerlo", 1, 0, 0);
		boton(2, an - 104, al - ALTO_BOTON - 8, 96, "Ahora no", 0, 0, 0);
		ui_callado(0);
		pintada = 1;
		if ((int)(hasta - so_tics()) <= 0)
			break;
		if (!ui_espera(&s, 1000))
			continue;
		if (s.clase == UE_CERRAR)
			break;
		if (s.clase == UE_TECLA && s.a == UT_ESC)
			break;
		if (s.clase == UE_TECLA && s.a == UT_INTRO) {
			r = 1;
			break;
		}
		if (s.clase == UE_BAJA) {
			int z = zona_en((int)s.a, (int)s.b);

			if (z == 1) {
				r = 1;
				break;
			}
			if (z == 2)
				break;
		}
	}
	ui_cierra();
	return r;
}

int correo_bandeja(void)
{
	u32 visto[MAX_CUENTAS];
	int i, primera_vez = 1;

	if (!ajustes.avisar || !n_cuentas)
		return 0;
	memset(visto, 0, sizeof visto);
	/* lo que ya se sabía (lo guardado: el UIDNEXT de la entrada) */
	for (i = 0; i < n_cuentas; i++) {
		Carpeta v[MAX_CARPETAS / 4];
		int n = alm_carpetas_lee(cuentas[i].num, v, MAX_CARPETAS / 4), k;

		for (k = 0; k < n; k++)
			if (v[k].marcas & CM_ENTRADA)
				visto[i] = v[k].uidnext;
	}
	so_apunta("bandeja: mirando %d cuentas cada %u minutos", n_cuentas, (unsigned)ajustes.cada_min);
	for (;;) {
		u32 i2;

		/* (la primera vez, al minuto: que la red esté) */
		for (i2 = 0; i2 < (primera_vez ? 60u : ajustes.cada_min * 60u); i2++)
			so_duerme(1000);
		primera_vez = 0;
		for (i = 0; i < n_cuentas; i++) {
			Cuenta *c = &cuentas[i];
			Imap m;
			Carpeta e;

			if (c->tipo != CT_IMAP || !c->avisar || !c->guardar_clave || !c->clave[0])
				continue;
			if (imap_conecta(&m, c, c->clave)) {
				imap_cierra(&m);
				continue;
			}
			memset(&e, 0, sizeof e);
			snprintf(e.crudo, sizeof e.crudo, "INBOX");
			e.marcas = CM_ENTRADA;
			if (!imap_status(&m, &e) && e.uidnext > visto[i] && visto[i]) {
				char t[300];

				snprintf(t, sizeof t, "%s: %u sin leer", c->correo, (unsigned)e.no_leidos);
				visto[i] = e.uidnext;
				imap_cierra(&m);
				if (aviso_bandeja(t)) {
					char p[260];

					if (so_programa("CORREO.BIN", p, sizeof p))
						so_lanza(p, "");
				}
				continue;
			}
			if (e.uidnext)
				visto[i] = e.uidnext;
			imap_cierra(&m);
		}
	}
}
