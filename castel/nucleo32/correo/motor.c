/* motor.c - Correo: lo que se hace con una cuenta (motor.h).
 *
 * Poner al día una carpeta (motor_al_dia), pensado para buzones grandes:
 * SELECT; si UIDVALIDITY ha cambiado, lo guardado no vale; UID SEARCH ALL
 * dice qué hay (lo que ya no está se quita de aquí; lo que falta se pide);
 * las banderas de lo que ya se tenía (de 20.000 para arriba, solo las de los
 * 2.000 últimos); y las fichas nuevas, DE LAS MÁS NUEVAS A LAS MÁS VIEJAS,
 * en tramos de TRAMO, guardando el índice y repintando la lista detrás de
 * cada tramo: la ventana enseña lo de hoy enseguida aunque el buzón tenga
 * 50.000 mensajes, y si se corta (o se cancela), lo traído se queda.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include <stdio.h>
#include "motor.h"
#include "html.h"

static void especiales(Sesion *s);
static void dice(Sesion *s, const char *f, const char *a)
{
	snprintf(s->estado, sizeof s->estado, f, a ? a : "");
}

static void avanza(Sesion *s, u32 h, u32 t, const char *que)
{
	if (s->avance)
		s->avance(s->avance_u, h, t, que);
}

int motor_carpeta_especial(Sesion *s, u32 marca)
{
	int i;

	for (i = 0; i < s->n_car; i++)
		if (s->car[i].marcas & marca)
			return i;
	return -1;
}

/* ---- las carpetas de una cuenta POP3 (solo de aquí) ---- */
static void carpetas_pop3(Sesion *s)
{
	static const struct { const char *n; u32 m; } v[] = {
		{ "Bandeja de entrada", CM_ENTRADA }, { "Enviados", CM_ENVIADOS }, { "Borradores", CM_BORRADORES },
		{ "Papelera", CM_PAPELERA },
	};
	int i;

	s->n_car = 4;
	for (i = 0; i < 4; i++) {
		memset(&s->car[i], 0, sizeof s->car[i]);
		snprintf(s->car[i].nombre, sizeof s->car[i].nombre, "%s", v[i].n);
		snprintf(s->car[i].crudo, sizeof s->car[i].crudo, "%s", v[i].n);
		s->car[i].marcas = v[i].m | CM_LOCAL;
		s->car[i].num = i + 1;
	}
}

static void cuenta_leidos(Sesion *s)
{
	u32 i, nl = 0;

	if (!s->buz_abierto || s->sel < 0)
		return;
	for (i = 0; i < s->buz.n; i++)
		if (!(s->buz.f[i].banderas & B_LEIDO))
			nl++;
	s->car[s->sel].total = s->buz.n;
	s->car[s->sel].no_leidos = nl;
}

int motor_abre(Sesion *s, int ic)
{
	memset(s, 0, sizeof *s);
	s->ic = ic;
	s->c = &cuentas[ic];
	s->sel = -1;
	if (s->c->guardar_clave)
		snprintf(s->clave, sizeof s->clave, "%s", s->c->clave);
	if (s->c->tipo == CT_POP3) {
		Carpeta g[8];
		int n = alm_carpetas_lee(s->c->num, g, 8), i;

		carpetas_pop3(s);
		for (i = 0; i < n && i < 4; i++) {
			s->car[i].total = g[i].total;
			s->car[i].no_leidos = g[i].no_leidos;
		}
	} else {
		s->n_car = alm_carpetas_lee(s->c->num, s->car, MAX_CARPETAS);
		especiales(s);
	}
	return 0;
}

static void sin_idle(Sesion *s)
{
	if (s->conectada && s->imap.en_idle)
		if (imap_idle_acaba(&s->imap) < 0)
			s->conectada = 0;
}

void motor_desconecta(Sesion *s)
{
	if (s->conectada)
		imap_cierra(&s->imap);
	s->conectada = 0;
}

void motor_cierra(Sesion *s)
{
	if (s->buz_abierto)
		buzon_cierra(&s->buz);
	s->buz_abierto = 0;
	alm_carpetas_guarda(s->c->num, s->car, s->n_car);
	motor_desconecta(s);
	memset(s->clave, 0, sizeof s->clave);
}

/* lo cambiado sin conexión, al servidor */
static void sube_pendientes(Sesion *s)
{
	u32 i, *v[2] = { NULL, NULL }, n[2] = { 0, 0 }, k;

	if (!s->buz_abierto || !s->conectada || s->c->tipo == CT_POP3)
		return;
	for (i = 0; i < s->buz.n; i++)
		if (s->buz.f[i].banderas & B_PENDIENTE) {
			for (k = 0; k < 2; k++) {
				if (!v[k])
					v[k] = so_pide(s->buz.n * sizeof(u32));
				if (!v[k])
					return;
			}
			k = (s->buz.f[i].banderas & B_LEIDO) ? 0 : 1;
			v[k][n[k]++] = s->buz.f[i].uid;
		}
	if (n[0])
		imap_marca(&s->imap, v[0], n[0], B_LEIDO, 1);
	if (n[1])
		imap_marca(&s->imap, v[1], n[1], B_LEIDO, 0);
	for (i = 0; i < s->buz.n; i++)
		if (s->buz.f[i].banderas & B_PENDIENTE) {
			u32 uid = s->buz.f[i].uid;

			imap_marca(&s->imap, &uid, 1, B_DESTACADO, (s->buz.f[i].banderas & B_DESTACADO) != 0);
			s->buz.f[i].banderas &= ~B_PENDIENTE;
			buzon_cambia(&s->buz, &s->buz.f[i]);
		}
	so_suelta(v[0]);
	so_suelta(v[1]);
}

static void envia_salida(Sesion *s);

int motor_conecta(Sesion *s)
{
	if (s->conectada)
		return 0;
	if (s->c->tipo == CT_POP3)
		return 0;	/* (el POP3 se conecta cada vez: motor_pop3) */
	if (!s->clave[0]) {
		snprintf(s->error, sizeof s->error, "Falta la contrase\xc3\xb1" "a.");
		return -1;
	}
	dice(s, "Conectando con %s...", s->c->entrada);
	if (imap_conecta(&s->imap, s->c, s->clave)) {
		snprintf(s->error, sizeof s->error, "%s", s->imap.error);
		dice(s, "Sin conexi\xc3\xb3n: %s", s->imap.error);
		s->sin_red = 1;
		return -1;
	}
	s->conectada = 1;
	s->sin_red = 0;
	dice(s, "Conectado con %s", s->c->entrada);
	if (s->imap.con.cifrado[0]) {
		char t[300];

		snprintf(t, sizeof t, "Conectado con %s (%s)", s->c->entrada, s->imap.con.cifrado);
		snprintf(s->estado, sizeof s->estado, "%s", t);
	}
	envia_salida(s);
	return 0;
}

/* las de enviados, borradores y papelera de la cuenta (y se guardan) */
static void especiales(Sesion *s)
{
	char e[200], b[200], p[200];
	int i;

	snprintf(e, sizeof e, "%s", s->c->enviados);
	snprintf(b, sizeof b, "%s", s->c->borradores);
	snprintf(p, sizeof p, "%s", s->c->papelera);
	if ((i = motor_carpeta_especial(s, CM_ENVIADOS)) >= 0)
		snprintf(s->c->enviados, sizeof s->c->enviados, "%s", s->car[i].crudo);
	if ((i = motor_carpeta_especial(s, CM_BORRADORES)) >= 0)
		snprintf(s->c->borradores, sizeof s->c->borradores, "%s", s->car[i].crudo);
	if ((i = motor_carpeta_especial(s, CM_PAPELERA)) >= 0)
		snprintf(s->c->papelera, sizeof s->c->papelera, "%s", s->car[i].crudo);
	if (strcmp(e, s->c->enviados) || strcmp(b, s->c->borradores) || strcmp(p, s->c->papelera))
		cuentas_guarda();
}

int motor_carpetas(Sesion *s)
{
	Carpeta *v;
	int n, i, j;

	if (s->c->tipo == CT_POP3)
		return s->n_car;
	if (motor_conecta(s))
		return -1;
	sin_idle(s);
	v = so_pide(MAX_CARPETAS * sizeof(Carpeta));
	if (!v)
		return -1;
	n = imap_carpetas(&s->imap, v, MAX_CARPETAS, 1);
	if (n < 0) {
		snprintf(s->error, sizeof s->error, "%s", s->imap.error);
		so_suelta(v);
		if (!s->imap.con.c)
			s->conectada = 0;
		return -1;
	}
	/* los números de almacén de las que ya se conocían */
	for (i = 0; i < n; i++) {
		v[i].num = 0;
		for (j = 0; j < s->n_car; j++)
			if (!strcmp(s->car[j].crudo, v[i].crudo))
				v[i].num = s->car[j].num;
	}
	/* (los números de las que ya no están no se vuelven a dar) */
	{
		int mayor = 0;

		for (j = 0; j < s->n_car; j++)
			if (s->car[j].num > mayor)
				mayor = s->car[j].num;
		for (i = 0; i < n; i++)
			if (!v[i].num)
				v[i].num = ++mayor;
	}
	/* la carpeta abierta: dónde ha quedado */
	if (s->sel >= 0) {
		char cr[256];

		snprintf(cr, sizeof cr, "%s", s->car[s->sel].crudo);
		s->sel = -1;
		for (i = 0; i < n; i++)
			if (!strcmp(v[i].crudo, cr))
				s->sel = i;
		if (s->sel < 0 && s->buz_abierto) {
			buzon_cierra(&s->buz);
			s->buz_abierto = 0;
		}
	}
	memcpy(s->car, v, (size_t)n * sizeof(Carpeta));
	s->n_car = n;
	so_suelta(v);
	/* las de enviados, borradores y papelera de la cuenta */
	especiales(s);
	alm_carpetas_guarda(s->c->num, s->car, s->n_car);
	so_apunta("carpetas: %d en %s", n, s->c->correo);
	return n;
}

int motor_elige(Sesion *s, int k, int al_dia)
{
	if (k < 0 || k >= s->n_car)
		return -1;
	if (s->buz_abierto && s->sel == k) {
		if (al_dia)
			return motor_al_dia(s);
		return 0;
	}
	if (s->buz_abierto)
		buzon_cierra(&s->buz);
	s->buz_abierto = 0;
	s->sel = k;
	if (buzon_abre(&s->buz, s->c->num, s->car[k].num) < 0) {
		snprintf(s->error, sizeof s->error, "No hay memoria para el \xc3\xadndice de %s.", s->car[k].nombre);
		return -1;
	}
	s->buz_abierto = 1;
	cuenta_leidos(s);
	if (al_dia && !(s->car[k].marcas & CM_NOSELECT))
		return motor_al_dia(s);
	return 0;
}

/* ---- poner al día ---- */
typedef struct {
	Sesion *s;
	u32 llegadas, nuevas;
} Recoge;

static void recoge_ficha(void *u, Ficha *f)
{
	Recoge *r = u;
	Ficha *g;

	if (r->s->c->max_kb && f->tam > r->s->c->max_kb * 1024)
		f->banderas |= B_GRANDE;
	g = buzon_pon(&r->s->buz, f);
	if (g && !(g->banderas & B_LEIDO)) {
		g->banderas |= B_NUEVO;
		r->nuevas++;
	}
	r->llegadas++;
}

static void recoge_banderas(void *u, u32 uid, u32 b)
{
	Sesion *s = u;
	Ficha *f = buzon_busca(&s->buz, uid);
	const u32 de_servidor = B_LEIDO | B_DESTACADO | B_RESPONDIDO | B_BORRADO | B_BORRADOR | B_REENVIADO;

	if (!f || (f->banderas & B_PENDIENTE))
		return;
	if ((f->banderas & de_servidor) != (b & de_servidor)) {
		f->banderas = (f->banderas & ~de_servidor) | (b & de_servidor);
		buzon_cambia(&s->buz, f);
	}
}

int motor_al_dia(Sesion *s)
{
	Carpeta *c;
	u32 *todos = NULL, nt = 0, i, j, *faltan = NULL, nf = 0, t0 = so_tics();
	Recoge r = { s, 0, 0 };
	int x;

	if (s->sel < 0 || !s->buz_abierto)
		return -1;
	c = &s->car[s->sel];
	if (s->c->tipo == CT_POP3)
		return (c->marcas & CM_ENTRADA) ? motor_pop3(s) : 0;
	if (c->marcas & CM_NOSELECT)
		return 0;
	if (motor_conecta(s))
		return -1;
	sin_idle(s);
	dice(s, "Abriendo %s...", c->nombre);
	x = imap_selecciona(&s->imap, c->crudo, 0);
	if (x) {
		snprintf(s->error, sizeof s->error, "%s", s->imap.error);
		if (!s->imap.con.c)
			s->conectada = 0;
		return -1;
	}
	if (s->buz.uidvalidity != s->imap.uidvalidity) {
		if (s->buz.n)
			so_apunta("al d\xc3\xad" "a: %s: UIDVALIDITY nueva: lo guardado no vale", c->nombre);
		buzon_vacia(&s->buz, s->imap.uidvalidity);
	}
	/* qué hay */
	if (imap_busca_uids(&s->imap, "ALL", NULL, &todos, &nt)) {
		snprintf(s->error, sizeof s->error, "%s", s->imap.error);
		return -1;
	}
	/* lo que ya no está */
	{
		u32 k = 0, quitados = 0;

		for (i = 0, j = 0; i < s->buz.n; i++) {
			u32 uid = s->buz.f[i].uid;

			while (j < nt && todos[j] < uid)
				j++;
			if (j < nt && todos[j] == uid)
				s->buz.f[k++] = s->buz.f[i];
			else {
				s->buz.muertos += s->buz.f[i].largo;
				quitados++;
			}
		}
		if (quitados) {
			s->buz.n = k;
			s->buz.reescribir = 1;
		}
	}
	/* las banderas de lo que ya estaba */
	if (s->buz.n) {
		u32 desde = s->buz.n > 20000 ? s->buz.f[s->buz.n - 2000].uid : 1;

		dice(s, "Mirando las banderas de %s...", c->nombre);
		if (imap_banderas(&s->imap, desde, buzon_max_uid(&s->buz), recoge_banderas, s) < 0) {
			s->conectada = 0;
			so_suelta(todos);
			return -1;
		}
	}
	sube_pendientes(s);
	/* lo que falta, de lo más nuevo a lo más viejo */
	if (nt) {
		faltan = so_pide(nt * sizeof(u32));
		if (!faltan) {
			so_suelta(todos);
			return -1;
		}
		for (i = nt; i-- > 0;)
			if (!buzon_busca(&s->buz, todos[i]))
				faltan[nf++] = todos[i];
	}
	so_suelta(todos);
	for (i = 0; i < nf; i += TRAMO) {
		u32 n = nf - i > TRAMO ? TRAMO : nf - i, a, b;
		char t[120];

		/* (el tramo, de menor a mayor, para el conjunto) */
		for (a = i, b = i + n - 1; a < b; a++, b--) {
			u32 z = faltan[a];

			faltan[a] = faltan[b];
			faltan[b] = z;
		}
		snprintf(t, sizeof t, "Trayendo %s: %u de %u", c->nombre, (unsigned)(i + n), (unsigned)nf);
		snprintf(s->estado, sizeof s->estado, "%s", t);
		x = imap_fichas(&s->imap, faltan + i, n, (c->marcas & (CM_ENVIADOS | CM_BORRADORES)) != 0, recoge_ficha, &r);
		if (x) {
			snprintf(s->error, sizeof s->error, "%s", s->imap.error);
			if (!s->imap.con.c)
				s->conectada = 0;
			break;
		}
		buzon_guarda(&s->buz);
		cuenta_leidos(s);
		avanza(s, i + n, nf, t);
	}
	so_suelta(faltan);
	s->buz.uidnext = s->imap.uidnext;
	buzon_guarda(&s->buz);
	cuenta_leidos(s);
	c->uidnext = s->imap.uidnext;
	c->uidvalidity = s->imap.uidvalidity;
	s->nuevos = r.nuevas;
	so_apunta("al d\xc3\xad" "a: %s: %u mensajes (%u nuevos, %u fichas tra\xc3\xad" "das) en %u ms", c->nombre, (unsigned)s->buz.n,
		  (unsigned)r.nuevas, (unsigned)r.llegadas, (unsigned)((so_tics() - t0) * 1000 / so_hz()));
	{
		char t[200];

		snprintf(t, sizeof t, "%s: %u mensajes, %u sin leer", c->nombre, (unsigned)c->total, (unsigned)c->no_leidos);
		snprintf(s->estado, sizeof s->estado, "%s", t);
	}
	return x ? -1 : 0;
}

/* ---- un mensaje ---- */
u8 *motor_mensaje(Sesion *s, Ficha *f, u32 *n)
{
	u8 *d;
	u32 uid = f->uid;

	*n = 0;
	if (f->largo) {
		d = buzon_cuerpo(&s->buz, f, n);
		if (d)
			return d;
	}
	if (s->c->tipo == CT_POP3) {
		snprintf(s->error, sizeof s->error, "No est\xc3\xa1 guardado en este equipo.");
		return NULL;
	}
	if (motor_conecta(s)) {
		snprintf(s->error, sizeof s->error, "Sin conexi\xc3\xb3n, y este mensaje a\xc3\xban no se ha bajado.");
		return NULL;
	}
	sin_idle(s);
	if (strcmp(s->imap.sel, s->car[s->sel].crudo) && imap_selecciona(&s->imap, s->car[s->sel].crudo, 0))
		goto mal;
	if (imap_mensaje(&s->imap, uid, &d, n, ajustes.max_mensaje_kb * 1024u + 65536, 0))
		goto mal;
	f = buzon_busca(&s->buz, uid);
	if (f && buzon_guarda_cuerpo(&s->buz, f, d, *n) == 0)
		buzon_guarda(&s->buz);
	so_apunta("bajado: %u (%u octetos)", (unsigned)uid, (unsigned)*n);
	return d;
mal:
	snprintf(s->error, sizeof s->error, "%s", s->imap.error);
	if (!s->imap.con.c)
		s->conectada = 0;
	return NULL;
}

static u32 *uids_de(Ficha **v, int n)
{
	u32 *u = so_pide((size_t)(n + 1) * sizeof(u32));
	int i, j;

	if (!u)
		return NULL;
	for (i = 0; i < n; i++)
		u[i] = v[i]->uid;
	/* ordenados (de inserción: lo elegido es poco) */
	for (i = 1; i < n; i++) {
		u32 x = u[i];

		for (j = i; j > 0 && u[j - 1] > x; j--)
			u[j] = u[j - 1];
		u[j] = x;
	}
	return u;
}

int motor_marca(Sesion *s, Ficha **v, int n, u32 banderas, int poner)
{
	int i, r = 0;
	u32 *u;

	for (i = 0; i < n; i++) {
		if (poner)
			v[i]->banderas |= banderas;
		else
			v[i]->banderas &= ~banderas;
		v[i]->banderas &= ~B_NUEVO;
		buzon_cambia(&s->buz, v[i]);
	}
	cuenta_leidos(s);
	if (s->c->tipo == CT_POP3) {
		buzon_guarda(&s->buz);
		return 0;
	}
	if (!s->conectada || (sin_idle(s), !s->conectada)) {
		for (i = 0; i < n; i++)
			v[i]->banderas |= B_PENDIENTE;
		buzon_guarda(&s->buz);
		return 0;
	}
	u = uids_de(v, n);
	if (!u)
		return -1;
	if (strcmp(s->imap.sel, s->car[s->sel].crudo))
		imap_selecciona(&s->imap, s->car[s->sel].crudo, 0);
	if (imap_marca(&s->imap, u, (u32)n, banderas, poner)) {
		for (i = 0; i < n; i++)
			v[i]->banderas |= B_PENDIENTE;
		r = -1;
	}
	so_suelta(u);
	buzon_guarda(&s->buz);
	return r;
}

/* lo de un buzón local a otro (POP3) */
static int mueve_local(Sesion *s, Ficha **v, int n, int destino)
{
	Buzon d;
	int i;
	u32 uid;

	if (buzon_abre(&d, s->c->num, s->car[destino].num) < 0)
		return -1;
	uid = buzon_max_uid(&d);
	for (i = 0; i < n; i++) {
		u32 len;
		u8 *m = buzon_cuerpo(&s->buz, v[i], &len);
		Ficha f = *v[i], *g;

		f.uid = ++uid;
		f.largo = 0;
		f.banderas &= ~B_BAJADO;
		g = buzon_pon(&d, &f);
		if (g && m)
			buzon_guarda_cuerpo(&d, g, m, len);
		so_suelta(m);
	}
	buzon_cierra(&d);
	s->car[destino].total += (u32)n;
	{
		u32 *u = uids_de(v, n);

		if (u)
			for (i = 0; i < n; i++)
				buzon_quita(&s->buz, u[i]);
		so_suelta(u);
	}
	buzon_guarda(&s->buz);
	cuenta_leidos(s);
	return 0;
}

int motor_mueve(Sesion *s, Ficha **v, int n, int destino)
{
	u32 *u;
	int i, r;

	if (destino < 0 || destino >= s->n_car || destino == s->sel || n <= 0)
		return -1;
	if (s->c->tipo == CT_POP3)
		return mueve_local(s, v, n, destino);
	if (motor_conecta(s)) {
		snprintf(s->error, sizeof s->error, "Para mover hace falta conexi\xc3\xb3n con el servidor.");
		return -1;
	}
	sin_idle(s);
	u = uids_de(v, n);
	if (!u)
		return -1;
	if (strcmp(s->imap.sel, s->car[s->sel].crudo))
		imap_selecciona(&s->imap, s->car[s->sel].crudo, 0);
	r = imap_mueve(&s->imap, u, (u32)n, s->car[destino].crudo);
	if (r) {
		snprintf(s->error, sizeof s->error, "%s", s->imap.error);
		so_suelta(u);
		return -1;
	}
	for (i = 0; i < n; i++)
		buzon_quita(&s->buz, u[i]);
	so_suelta(u);
	buzon_guarda(&s->buz);
	cuenta_leidos(s);
	s->car[destino].total += (u32)n;
	so_apunta("movidos %d a %s", n, s->car[destino].nombre);
	return 0;
}

int motor_borra(Sesion *s, Ficha **v, int n)
{
	int p = motor_carpeta_especial(s, CM_PAPELERA), i, r;
	u32 *u;

	if (p >= 0 && p != s->sel)
		return motor_mueve(s, v, n, p);
	/* en la papelera (o sin ella): del todo */
	if (s->c->tipo == CT_POP3) {
		u = uids_de(v, n);
		if (!u)
			return -1;
		for (i = 0; i < n; i++)
			buzon_quita(&s->buz, u[i]);
		so_suelta(u);
		buzon_guarda(&s->buz);
		cuenta_leidos(s);
		return 0;
	}
	if (motor_conecta(s))
		return -1;
	sin_idle(s);
	u = uids_de(v, n);
	if (!u)
		return -1;
	if (strcmp(s->imap.sel, s->car[s->sel].crudo))
		imap_selecciona(&s->imap, s->car[s->sel].crudo, 0);
	r = imap_borra(&s->imap, u, (u32)n);
	if (!r)
		for (i = 0; i < n; i++)
			buzon_quita(&s->buz, u[i]);
	else
		snprintf(s->error, sizeof s->error, "%s", s->imap.error);
	so_suelta(u);
	buzon_guarda(&s->buz);
	cuenta_leidos(s);
	return r ? -1 : 0;
}

/* ---- buscar ---- */
int motor_busca(Sesion *s, const char *texto, int donde, Ficha ***v, int *n)
{
	static const char *const crit[] = { "TEXT", "FROM", "SUBJECT" };
	u32 *u, nu, i;
	Ficha **r;
	int k = 0;

	*v = NULL;
	*n = 0;
	if (s->c->tipo == CT_POP3 || motor_conecta(s))
		return motor_busca_local(s, texto, v, n);
	sin_idle(s);
	if (strcmp(s->imap.sel, s->car[s->sel].crudo))
		imap_selecciona(&s->imap, s->car[s->sel].crudo, 0);
	if (imap_busca_uids(&s->imap, crit[donde >= 0 && donde < 3 ? donde : 0], texto, &u, &nu)) {
		snprintf(s->error, sizeof s->error, "%s", s->imap.error);
		return -1;
	}
	r = so_pide((nu + 1) * sizeof(Ficha *));
	if (!r) {
		so_suelta(u);
		return -1;
	}
	for (i = nu; i-- > 0;) {
		Ficha *f = buzon_busca(&s->buz, u[i]);

		if (f)
			r[k++] = f;
	}
	so_suelta(u);
	*v = r;
	*n = k;
	so_apunta("buscar en el servidor \xc2\xab%s\xc2\xbb: %u", texto, (unsigned)k);
	return 0;
}

static int contiene(const char *t, u32 n, const char *p)
{
	char *a;
	u32 an;
	int r;

	a = utf8_plano(t, n, &an);
	if (!a)
		return 0;
	r = strstr(a, p) != NULL;
	so_suelta(a);
	return r;
}

int motor_busca_local(Sesion *s, const char *texto, Ficha ***v, int *n)
{
	char *p;
	u32 pn, i;
	Ficha **r;
	int k = 0;

	*v = NULL;
	*n = 0;
	if (!s->buz_abierto)
		return -1;
	p = utf8_plano(texto, (u32)strlen(texto), &pn);
	r = so_pide((s->buz.n + 1) * sizeof(Ficha *));
	if (!p || !r) {
		so_suelta(p);
		so_suelta(r);
		return -1;
	}
	for (i = s->buz.n; i-- > 0;) {
		Ficha *f = &s->buz.f[i];
		int si = contiene(f->asunto, (u32)strlen(f->asunto), p) || contiene(f->de, (u32)strlen(f->de), p) ||
			 contiene(f->dir, (u32)strlen(f->dir), p);

		if (!si && f->largo) {
			u32 m;
			u8 *d = buzon_cuerpo(&s->buz, f, &m);

			if (d) {
				Mensaje *ms = mime_analiza(d, m);

				if (ms) {
					int pr = mime_principal(ms, 0);

					if (pr >= 0) {
						u32 tn;
						char *t = mime_texto(ms, pr, &tn);

						if (t) {
							if (!strcmp(ms->partes[pr].tipo, "text/html")) {
								Doc *dd = html_a_doc(t, tn);

								if (dd) {
									si = contiene(dd->texto, dd->n, p);
									doc_libera(dd);
								}
							} else
								si = contiene(t, tn, p);
							so_suelta(t);
						}
					}
					mime_libera(ms);
				}
				so_suelta(d);
			}
		}
		if (si)
			r[k++] = f;
	}
	so_suelta(p);
	*v = r;
	*n = k;
	so_apunta("buscar en lo bajado \xc2\xab%s\xc2\xbb: %u", texto, (unsigned)k);
	return 0;
}

/* ---- enviar ---- */
static int guarda_salida(Sesion *s, const u8 *d, u32 n, const char *de, const char *destinos)
{
	char r[260], t[40];
	int k;
	void *f;

	alm_ruta(r, sizeof r, s->c->num, 0, "SALIDA");
	so_haz_carpeta(r);
	for (k = 1; k < 1000; k++) {
		int c;

		snprintf(t, sizeof t, "SALIDA%cS%04d.EML", so_barra(), k);
		alm_ruta(r, sizeof r, s->c->num, 0, t);
		if (!so_existe(r, &c))
			break;
	}
	f = so_abre(r, SO_CREAR);
	if (!f)
		return -1;
	snprintf(t, sizeof t, "X-Correo-Sobre: ");
	so_escribe(f, t, (u32)strlen(t));
	so_escribe(f, de, (u32)strlen(de));
	so_escribe(f, " ", 1);
	so_escribe(f, destinos, (u32)strlen(destinos));
	so_escribe(f, "\r\n", 2);
	k = so_escribe(f, d, n);
	return so_cierra(f) || k ? -1 : 0;
}

typedef struct {
	Sesion *s;
	char nombres[20][16];
	int n;
} ListaSalida;

static int ve_salida(void *u, const char *nombre, u32 tam, int carpeta)
{
	ListaSalida *l = u;

	(void)tam;
	if (!carpeta && l->n < 20 && strlen(nombre) < 16)
		snprintf(l->nombres[l->n++], 16, "%s", nombre);
	return 0;
}

static void guarda_en_enviados(Sesion *s, const u8 *d, u32 n)
{
	if (s->c->tipo == CT_POP3) {
		Buzon b;
		Ficha f, *g;

		if (buzon_abre(&b, s->c->num, 2) < 0)
			return;
		memset(&f, 0, sizeof f);
		ficha_de_cabeceras(&f, d, n, 1);
		f.uid = buzon_max_uid(&b) + 1;
		f.tam = n;
		f.banderas |= B_LEIDO;
		g = buzon_pon(&b, &f);
		if (g)
			buzon_guarda_cuerpo(&b, g, d, n);
		buzon_cierra(&b);
		s->car[1].total++;
		return;
	}
	if (s->conectada && s->c->enviados[0]) {
		u32 uid;

		sin_idle(s);
		if (imap_anade(&s->imap, s->c->enviados, d, n, B_LEIDO, &uid) == 0)
			so_apunta("guardado en %s (UID %u)", s->c->enviados, (unsigned)uid);
		else
			so_apunta("no se ha podido guardar en %s: %s", s->c->enviados, s->imap.error);
	}
}

/* lo que quedó en la bandeja de salida (sin conexión), otra vez */
static void envia_salida(Sesion *s)
{
	ListaSalida l;
	char r[260], t[60];
	int i;

	memset(&l, 0, sizeof l);
	alm_ruta(r, sizeof r, s->c->num, 0, "SALIDA");
	if (so_carpeta(r, ve_salida, &l) < 0 || !l.n)
		return;
	for (i = 0; i < l.n; i++) {
		u32 n;
		u8 *d;
		char *fin, de[260], e[300];

		snprintf(t, sizeof t, "SALIDA%c%s", so_barra(), l.nombres[i]);
		alm_ruta(r, sizeof r, s->c->num, 0, t);
		d = so_lee_archivo(r, &n, 64u << 20);
		if (!d)
			continue;
		fin = strstr((char *)d, "\r\n");
		if (fin && !strncmp((char *)d, "X-Correo-Sobre: ", 16)) {
			char *sp = strchr((char *)d + 16, ' ');

			if (sp && sp < fin) {
				u32 cab = (u32)(fin + 2 - (char *)d);

				snprintf(de, sizeof de, "%.*s", (int)(sp - (char *)d - 16), (char *)d + 16);
				*fin = 0;
				if (!smtp_envia(s->c, s->clave, de, sp + 1, d + cab, n - cab, e, sizeof e)) {
					guarda_en_enviados(s, d + cab, n - cab);
					so_borra(r);
					so_apunta("enviado lo de la bandeja de salida: %s", l.nombres[i]);
				}
			}
		}
		so_suelta(d);
	}
}

int motor_envia(Sesion *s, const Redactado *r, const char *destinos)
{
	char e[300];
	u8 *d;
	u32 n;
	int x;

	d = mime_construye(r, &n, e, sizeof e);
	if (!d) {
		snprintf(s->error, sizeof s->error, "No se ha podido hacer el mensaje: %s", e);
		return -1;
	}
	if (!s->clave[0]) {
		snprintf(s->error, sizeof s->error, "Falta la contrase\xc3\xb1" "a.");
		so_suelta(d);
		return -1;
	}
	x = smtp_envia(s->c, s->clave, s->c->correo, destinos, d, n, e, sizeof e);
	if (x) {
		/* sin red: a la bandeja de salida, y se envía al conectar */
		if (!so_hay_red() || strstr(e, "No hay red") || strstr(e, "no contesta") || strstr(e, "No se encuentra")) {
			if (!guarda_salida(s, d, n, s->c->correo, destinos)) {
				snprintf(s->error, sizeof s->error, "Sin conexi\xc3\xb3n: el mensaje queda en la bandeja de salida y se enviar\xc3\xa1 al conectar. (%s)", e);
				so_suelta(d);
				return 1;
			}
		}
		snprintf(s->error, sizeof s->error, "%s", e);
		so_suelta(d);
		return -1;
	}
	if (s->c->tipo != CT_POP3)
		motor_conecta(s);
	guarda_en_enviados(s, d, n);
	so_suelta(d);
	return 0;
}

int motor_borrador(Sesion *s, const Redactado *r, u32 *uid_viejo)
{
	char e[300];
	u8 *d;
	u32 n, uid = 0;
	Redactado x = *r;
	int k;

	x.con_cco = 1;
	d = mime_construye(&x, &n, e, sizeof e);
	if (!d) {
		snprintf(s->error, sizeof s->error, "No se ha podido hacer el borrador: %s", e);
		return -1;
	}
	if (s->c->tipo == CT_POP3 || motor_conecta(s) || !s->c->borradores[0]) {
		/* en este equipo */
		Buzon b;
		Ficha f, *g;
		int car = motor_carpeta_especial(s, CM_BORRADORES);

		if (s->c->tipo != CT_POP3 || car < 0) {
			char ru[260];
			void *a;

			alm_ruta(ru, sizeof ru, s->c->num, 0, "BORRADOR.EML");
			a = so_abre(ru, SO_CREAR);
			k = a ? so_escribe(a, d, n) : -1;
			if (so_cierra(a) || k) {
				so_suelta(d);
				snprintf(s->error, sizeof s->error, "No se ha podido guardar el borrador.");
				return -1;
			}
			so_suelta(d);
			snprintf(s->error, sizeof s->error, "Sin conexi\xc3\xb3n: el borrador se ha guardado en este equipo.");
			return 1;
		}
		if (buzon_abre(&b, s->c->num, s->car[car].num) < 0) {
			so_suelta(d);
			return -1;
		}
		if (uid_viejo && *uid_viejo)
			buzon_quita(&b, *uid_viejo);
		memset(&f, 0, sizeof f);
		ficha_de_cabeceras(&f, d, n, 1);
		f.uid = buzon_max_uid(&b) + 1;
		f.tam = n;
		f.banderas |= B_LEIDO | B_BORRADOR;
		g = buzon_pon(&b, &f);
		if (g)
			buzon_guarda_cuerpo(&b, g, d, n);
		if (uid_viejo)
			*uid_viejo = f.uid;
		buzon_cierra(&b);
		so_suelta(d);
		return 0;
	}
	sin_idle(s);
	k = imap_anade(&s->imap, s->c->borradores, d, n, B_LEIDO | B_BORRADOR, &uid);
	so_suelta(d);
	if (k) {
		snprintf(s->error, sizeof s->error, "%s", s->imap.error);
		return -1;
	}
	so_apunta("borrador guardado en %s (UID %u)", s->c->borradores, (unsigned)uid);
	if (uid_viejo && *uid_viejo && strcmp(s->imap.sel, s->c->borradores) == 0) {
		imap_borra(&s->imap, uid_viejo, 1);
	} else if (uid_viejo && *uid_viejo) {
		char sel[256];

		snprintf(sel, sizeof sel, "%s", s->imap.sel);
		if (!imap_selecciona(&s->imap, s->c->borradores, 0))
			imap_borra(&s->imap, uid_viejo, 1);
		if (sel[0])
			imap_selecciona(&s->imap, sel, 0);
	}
	if (uid_viejo)
		*uid_viejo = uid;
	return 0;
}

/* ---- lo nuevo ---- */
int motor_mira(Sesion *s, u32 espera_ms)
{
	int r;

	if (s->c->tipo == CT_POP3 || !s->conectada || s->sel < 0)
		return 0;
	if (s->imap.caps & IC_IDLE) {
		if (!s->imap.en_idle) {
			if (imap_idle_empieza(&s->imap))
				return 0;
			s->ult_ruido = so_tics();
		}
		r = imap_idle_mira(&s->imap, espera_ms);
		if (r < 0) {
			s->conectada = 0;
			return 0;
		}
		/* cada 25 minutos, otra vez (RFC 2177: el servidor puede cortar a los 30) */
		if (!r && (u32)(so_tics() - s->ult_ruido) > 25u * 60 * so_hz()) {
			sin_idle(s);
			return 0;
		}
		if (!r)
			return 0;
		so_apunta("IDLE: el servidor avisa (EXISTS %u)", (unsigned)s->imap.nuevo_exists);
		motor_al_dia(s);
		return 1;
	}
	so_duerme(espera_ms);
	if ((u32)(so_tics() - s->desde_noop) < (ajustes.cada_min ? ajustes.cada_min : 10) * 60u * so_hz())
		return 0;
	s->desde_noop = so_tics();
	motor_al_dia(s);
	return s->nuevos > 0;
}

/* ---- POP3 ---- */
typedef struct {
	char (*uidl)[72];
	u32 *num, *tam;
	u32 n, cabe;
} Uidls;

static void apunta_uidl(void *u, u32 num, const char *uidl, u32 tam)
{
	Uidls *l = u;

	if (l->n >= l->cabe)
		return;
	snprintf(l->uidl[l->n], 72, "%s", uidl);
	l->num[l->n] = num;
	l->tam[l->n] = tam;
	l->n++;
}

int motor_pop3(Sesion *s)
{
	Pop3 p;
	Uidls l;
	char r[260];
	u8 *vistos;
	u32 nv, i, traidos = 0;
	void *f;
	int err = 0;

	if (!s->clave[0]) {
		snprintf(s->error, sizeof s->error, "Falta la contrase\xc3\xb1" "a.");
		return -1;
	}
	if (pop3_conecta(&p, s->c, s->clave)) {
		snprintf(s->error, sizeof s->error, "%s", p.error);
		dice(s, "Sin conexi\xc3\xb3n: %s", p.error);
		return -1;
	}
	memset(&l, 0, sizeof l);
	l.cabe = p.n + 1;
	l.uidl = so_pide(l.cabe * 72);
	l.num = so_pide(l.cabe * sizeof(u32));
	l.tam = so_pide(l.cabe * sizeof(u32));
	if (!l.uidl || !l.num || !l.tam || pop3_uidl(&p, apunta_uidl, &l)) {
		err = 1;
		goto fin;
	}
	alm_ruta(r, sizeof r, s->c->num, 0, "UIDL.LST");
	{
		u8 *v = so_lee_archivo(r, &nv, 16u << 20);

		/* (con un salto delante, para buscar «\nUIDL\r») */
		vistos = v ? so_pide(nv + 2) : NULL;
		if (vistos) {
			vistos[0] = '\n';
			memcpy(vistos + 1, v, nv + 1);
		}
		so_suelta(v);
	}
	f = so_abre(r, SO_ANADIR);
	for (i = 0; i < l.n; i++) {
		char t[80];
		u8 *d;
		u32 n;
		Ficha fi, *g;

		snprintf(t, sizeof t, "\n%s\r", l.uidl[i]);
		if (vistos && strstr((char *)vistos, t))
			continue;
		if (s->c->max_kb && l.tam[i] > s->c->max_kb * 1024)
			continue;
		if (pop3_mensaje(&p, l.num[i], &d, &n, ajustes.max_mensaje_kb * 1024u)) {
			if (!p.con.c) {
				err = 1;
				break;
			}
			continue;
		}
		memset(&fi, 0, sizeof fi);
		ficha_de_cabeceras(&fi, d, n, 0);
		fi.uid = buzon_max_uid(&s->buz) + 1;
		fi.tam = n;
		fi.banderas |= B_NUEVO;
		g = buzon_pon(&s->buz, &fi);
		if (g)
			buzon_guarda_cuerpo(&s->buz, g, d, n);
		so_suelta(d);
		if (f) {
			so_escribe(f, l.uidl[i], (u32)strlen(l.uidl[i]));
			so_escribe(f, "\r\n", 2);
		}
		if (!s->c->dejar_en_servidor)
			pop3_borra(&p, l.num[i]);
		traidos++;
		if (!(traidos % 20)) {
			buzon_guarda(&s->buz);
			cuenta_leidos(s);
			avanza(s, i + 1, l.n, "Trayendo por POP3");
		}
	}
	so_cierra(f);
	so_suelta(vistos);
	buzon_guarda(&s->buz);
	cuenta_leidos(s);
	s->nuevos = traidos;
	so_apunta("pop3: %u nuevos de %u", (unsigned)traidos, (unsigned)l.n);
fin:
	so_suelta(l.uidl);
	so_suelta(l.num);
	so_suelta(l.tam);
	pop3_cierra(&p);
	if (err) {
		snprintf(s->error, sizeof s->error, "%s", p.error);
		return -1;
	}
	{
		char t[200];

		snprintf(t, sizeof t, "Bandeja de entrada: %u mensajes, %u sin leer", (unsigned)s->car[0].total, (unsigned)s->car[0].no_leidos);
		snprintf(s->estado, sizeof s->estado, "%s", t);
	}
	return 0;
}
