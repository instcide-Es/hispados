/* dialogos.c - Pintura 2: los cuadros de diálogo (abrir y guardar, imagen
 * nueva, cambiar el tamaño, estirar e inclinar, girar, atributos del
 * lienzo, brillo y contraste, el editor de colores con RGB, HSV y el
 * código #, la calidad del JPG, la ayuda...). Son los de oficina/comun/ui.h,
 * dentro de la ventana de Pintura (uno a la vez).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "pintura.h"
#include "formatos.h"
#include "operaciones.h"
#include "letras.h"

int paleta_editada = -1;

static ui_control C[48];
static int nC;
static ui_cuadro Q;
static int U, H;

void cuadro_abre(ui_cuadro *q, void (*f)(int id))
{
	int i;

	q->cerrado = -9999;
	q->pulsado = -1;
	q->menu_de = -1;
	q->arrastra = -1;
	q->foco = 0;
	for (i = 0; i < q->n; i++)
		if (!q->c[i].apagado && q->c[i].tipo != UC_ROTULO && q->c[i].tipo != UC_TEXTO && q->c[i].tipo != UC_MUESTRA) {
			q->foco = i;
			break;
		}
	if (q->foco < q->n && (q->c[q->foco].tipo == UC_CAMPO || q->c[q->foco].tipo == UC_NUMERO)) {
		q->c[q->foco].ancla = 0;
		q->c[q->foco].cursor = (int)utf8_cuenta(q->c[q->foco].valor, largo(q->c[q->foco].valor));
	}
	P.cuadro = q;
	P.al_cerrar = f;
	P.al_cambiar = NULL;
	P.al_raton = NULL;
	pin_repinta_todo();
	dice("pintura: cuadro «%s»", q->titulo);
}

static void nuevo_cuadro(const char *titulo, int an, int al)
{
	U = lz_ui_alto();
	H = ui_alto_control();
	memset(&Q, 0, sizeof Q);
	memset(C, 0, sizeof C);
	nC = 0;
	Q.titulo = titulo;
	Q.an = an;
	Q.al = al;
}

static ui_control *ctl(int tipo, int id, int x, int y, int an, const char *rot)
{
	ui_control *c;

	if (nC >= 48)
		nC = 47;
	c = &C[nC++];
	memset(c, 0, sizeof *c);
	c->tipo = tipo;
	c->id = id;
	c->x = x;
	c->y = y;
	c->an = an;
	c->rotulo = rot;
	return c;
}

static void rotulo(int x, int y, const char *t)
{
	ctl(UC_ROTULO, 0, x, y, 0, t);
}

static ui_control *campo(int id, int x, int y, int an, const char *v)
{
	ui_control *c = ctl(UC_CAMPO, id, x, y, an, NULL);

	copia(c->valor, v ? v : "", sizeof c->valor);
	return c;
}

/* un número entero (con sus límites) */
static ui_control *numero_c(int id, int x, int y, int an, int v, int min, int max, const char *unidad)
{
	ui_control *c = ctl(UC_NUMERO, id, x, y, an, NULL);

	c->min = min * 10;
	c->max = max * 10;
	c->paso = 10;
	c->decimales = 0;
	c->unidad = unidad;
	ui_pon_numero(c, v * 10);
	return c;
}

static ui_control *boton(int id, int x, int y, int an, const char *t, int principal)
{
	ui_control *c = ctl(UC_BOTON, id, x, y, an, t);

	c->principal = principal;
	return c;
}

static ui_control *casilla(int id, int x, int y, int an, const char *t, int m)
{
	ui_control *c = ctl(UC_CASILLA, id, x, y, an, t);

	c->marcado = m;
	return c;
}

static ui_control *opcion(int id, int x, int y, int an, const char *t, int grupo, int m)
{
	ui_control *c = ctl(UC_OPCION, id, x, y, an, t);

	c->grupo = grupo;
	c->marcado = m;
	return c;
}

static ui_control *desple(int id, int x, int y, int an, const char *ops, int e)
{
	ui_control *c = ctl(UC_DESPLEGABLE, id, x, y, an, NULL);

	c->opciones = ops;
	c->elegida = e;
	return c;
}

static ui_control *lista(int id, int x, int y, int an, int al, const char *ops, int e)
{
	ui_control *c = ctl(UC_LISTA, id, x, y, an, NULL);

	c->al = al;
	c->opciones = ops;
	c->elegida = e;
	return c;
}

static ui_control *texto(int x, int y, int an, int al, const char *t)
{
	ui_control *c = ctl(UC_TEXTO, 0, x, y, an, t);

	c->al = al;
	return c;
}

#define B_ACEPTAR 1
#define B_CANCELAR 2
static void aceptar_cancelar(const char *ok)
{
	int w = 110;

	boton(B_ACEPTAR, Q.an - 2 * w - 24, Q.al - H - 12, w, ok ? ok : "Aceptar", 1);
	boton(B_CANCELAR, Q.an - w - 12, Q.al - H - 12, w, "Cancelar", 0)->cancela = 1;
}

static void abre(void (*f)(int))
{
	Q.c = C;
	Q.n = nC;
	cuadro_abre(&Q, f);
}

static ui_control *c_(int id)
{
	return ui_control_id(&Q, id);
}

static int num(int id)
{
	ui_control *c = c_(id);

	return c ? (ui_numero(c) + (ui_numero(c) >= 0 ? 5 : -5)) / 10 : 0;
}

static void pon_num(int id, int v)
{
	ui_control *c = c_(id);

	if (c)
		ui_pon_numero(c, v * 10);
}

/* ===================================================================== */
/*  Avisos y preguntas                                                   */
/* ===================================================================== */
static char aviso_txt[1600];

static void al_nada(int id)
{
	(void)id;
}

void dlg_aviso(const char *titulo, const char *t)
{
	int lineas = 1, w = 0;
	const char *s;

	copia(aviso_txt, t, sizeof aviso_txt);
	for (s = aviso_txt; *s; s++)
		if (*s == '\n')
			lineas++;
	{
		char l[300];
		const char *a = aviso_txt;

		while (a && *a) {
			const char *e = strchr(a, '\n');
			u32 k = e ? (u32)(e - a) : largo(a);

			copia(l, a, k + 1 < sizeof l ? k + 1 : sizeof l);
			if (lz_ui_ancho(l) > w)
				w = lz_ui_ancho(l);
			a = e ? e + 1 : NULL;
		}
	}
	U = lz_ui_alto();
	H = ui_alto_control();
	nuevo_cuadro(titulo, w + 48 < 320 ? 320 : w + 48, lineas * U + H + 44);
	texto(20, 16, w, lineas * U, aviso_txt);
	boton(B_ACEPTAR, Q.an - 122, Q.al - H - 12, 110, "Aceptar", 1);
	abre(al_nada);
	dice("pintura: aviso «%s»: %s", titulo, t);
}

#define B_NO_GUARDAR 3
static char pregunta[300];

static void al_guardar_cambios(int id)
{
	int o = P.pendiente;

	if (id == B_ACEPTAR) {
		if (!P.ruta[0] || !fi_se_escribe(P.formato)) {
			dlg_abrir(DLG_GUARDAR);		/* (al guardar, sigue P.pendiente) */
			return;
		}
		if (pin_guarda(P.ruta, P.formato))
			return;
		P.pendiente = 0;
		pin_orden(o);
	} else if (id == B_NO_GUARDAR) {
		P.cambiado = 0;
		P.pendiente = 0;
		pin_orden(o);
	} else
		P.pendiente = 0;
}

void dlg_guardar_cambios(int orden)
{
	P.pendiente = orden;
	fmt(pregunta, sizeof pregunta, "¿Guardar los cambios de %s?", P.ruta[0] ? sin_carpeta(P.ruta) : "la imagen sin título");
	nuevo_cuadro("Pintura", lz_ui_ancho(pregunta) + 60 < 420 ? 420 : lz_ui_ancho(pregunta) + 60, U * 2 + H + 40);
	texto(20, 16, Q.an - 40, U, pregunta);
	boton(B_ACEPTAR, 12, Q.al - H - 12, 120, "&Guardar", 1);
	boton(B_NO_GUARDAR, 144, Q.al - H - 12, 120, "&No guardar", 0);
	boton(B_CANCELAR, Q.an - 122, Q.al - H - 12, 110, "Cancelar", 0)->cancela = 1;
	abre(al_guardar_cambios);
}

static void sigue_pendiente(void)
{
	if (P.pendiente) {
		int o = P.pendiente;

		P.pendiente = 0;
		pin_orden(o);
	}
}

/* ===================================================================== */
/*  Abrir y guardar                                                      */
/* ===================================================================== */
#define ID_LISTA 10
#define ID_NOMBRE 11
#define ID_TIPO 12
#define ID_SUBIR 13
#define MAX_ENT 400

static int modo_abrir;
static char ent_nombre[MAX_ENT][64];
static u8 ent_carpeta[MAX_ENT];
static int n_ent;
static char ops_lista[MAX_ENT * 66];
static char rot_carpeta[700];
static const char *const TIPOS_GUARDAR = "PNG (sin pérdida, con transparencia)|JPG (fotos: con pérdida; la calidad, luego)|BMP (mapa de bits de Windows)|GIF (hasta 256 colores)|PCX (PC Paintbrush)";
static const int fmt_tipo[] = { FI_PNG, FI_JPG, FI_BMP, FI_GIF, FI_PCX };
static const char *const TIPOS_ABRIR = "Imágenes (PNG, JPG, BMP, GIF, PCX, ICO, TGA)|Todos los archivos";
static int tipo_elegido;
static char nombre_puesto[260];

static int vale_ext(const char *n, int carpeta)
{
	int f = fi_por_extension(n);

	if (carpeta)
		return 1;
	if (modo_abrir == DLG_PDF) {
		char e[8];

		extension(n, e);
		return iguales(e, "PDF");
	}
	if (modo_abrir == DLG_GUARDAR || modo_abrir == DLG_COPIAR_A)
		return f == fmt_tipo[tipo_elegido];
	return tipo_elegido == 1 || f != FI_NADA;
}

static void lee_carpeta(void)
{
	void *c = so_abre_carpeta(P.carpeta[0] ? P.carpeta : "\\");
	so_entrada e;
	int i, j;

	n_ent = 0;
	if (c) {
		while (so_lee_carpeta(c, &e) && n_ent < MAX_ENT) {
			if (!vale_ext(e.nombre, e.carpeta))
				continue;
			copia(ent_nombre[n_ent], e.nombre, sizeof ent_nombre[0]);
			ent_carpeta[n_ent] = (u8)e.carpeta;
			n_ent++;
		}
		so_cierra_carpeta(c);
	}
	for (i = 1; i < n_ent; i++) {
		char t[64];
		u8 k = ent_carpeta[i];

		copia(t, ent_nombre[i], sizeof t);
		for (j = i; j > 0; j--) {
			int antes = ent_carpeta[j - 1] > k || (ent_carpeta[j - 1] == k && strcmp(ent_nombre[j - 1], t) <= 0);

			if (antes)
				break;
			copia(ent_nombre[j], ent_nombre[j - 1], sizeof ent_nombre[0]);
			ent_carpeta[j] = ent_carpeta[j - 1];
		}
		copia(ent_nombre[j], t, sizeof ent_nombre[0]);
		ent_carpeta[j] = k;
	}
	ops_lista[0] = 0;
	for (i = 0; i < n_ent; i++) {
		if (i)
			anade(ops_lista, "|", sizeof ops_lista);
		if (ent_carpeta[i])
			anade(ops_lista, "[", sizeof ops_lista);
		anade(ops_lista, ent_nombre[i], sizeof ops_lista);
		if (ent_carpeta[i])
			anade(ops_lista, "]", sizeof ops_lista);
	}
	if (!n_ent)
		copia(ops_lista, "(nada)", sizeof ops_lista);
	fmt(rot_carpeta, sizeof rot_carpeta, "Carpeta: %s", P.carpeta[0] ? P.carpeta : "\\");
}

static void abre_abrir(void);

static void sube_carpeta(void)
{
	u32 i, k = 0;

	for (i = 0; P.carpeta[i]; i++)
		if (P.carpeta[i] == '\\' || P.carpeta[i] == '/')
			k = i;
	if (k > 0 && !(k == 2 && P.carpeta[1] == ':'))
		P.carpeta[k] = 0;
	else if (P.carpeta[1] == ':')
		P.carpeta[2] = 0;
}

static char ruta_calidad[700];

static void al_abrir(int id)
{
	ui_control *l = c_(ID_LISTA), *n = c_(ID_NOMBRE), *t = c_(ID_TIPO);
	char ruta[700], err[300];
	const char *nom;

	tipo_elegido = t ? t->elegida : 0;
	copia(nombre_puesto, n ? n->valor : "", sizeof nombre_puesto);
	if (id == ID_SUBIR) {
		sube_carpeta();
		abre_abrir();
		return;
	}
	if (id == B_CANCELAR || id < 0) {
		P.pendiente = 0;
		return;
	}
	nom = n ? n->valor : "";
	if (!nom[0] && l && l->elegida >= 0 && l->elegida < n_ent) {
		if (ent_carpeta[l->elegida]) {
			anade(P.carpeta, "\\", sizeof P.carpeta);
			anade(P.carpeta, ent_nombre[l->elegida], sizeof P.carpeta);
			nombre_puesto[0] = 0;
			abre_abrir();
			return;
		}
		nom = ent_nombre[l->elegida];
	}
	if (!nom[0]) {
		abre_abrir();
		return;
	}
	if (iguales(nom, "..")) {
		sube_carpeta();
		nombre_puesto[0] = 0;
		abre_abrir();
		return;
	}
	if (strchr(nom, ':') || nom[0] == '\\' || nom[0] == '/')
		copia(ruta, nom, sizeof ruta);
	else
		fmt(ruta, sizeof ruta, "%s\\%s", P.carpeta, nom);
	if (so_existe(ruta) == 2) {
		copia(P.carpeta, ruta, sizeof P.carpeta);
		nombre_puesto[0] = 0;
		abre_abrir();
		return;
	}
	switch (modo_abrir) {
	case DLG_ABRIR:
		pin_abre(ruta);
		break;
	case DLG_GUARDAR: {
		int f = fmt_tipo[tipo_elegido], fe = fi_por_extension(ruta);

		/* (la extensión escrita manda, si es de las que se guardan) */
		if (fe && fi_se_escribe(fe))
			f = fe;
		pin_ruta_con_ext(ruta, sizeof ruta, f);
		if (f == FI_JPG) {
			copia(ruta_calidad, ruta, sizeof ruta_calidad);
			dlg_calidad(ruta_calidad);
			return;
		}
		if (pin_guarda(ruta, f) == 0)
			sigue_pendiente();
		break;
	}
	case DLG_PDF:
		pin_pdf(ruta);
		break;
	case DLG_PEGAR_DE: {
		lamina im;
		int x, y;

		if (!pin_lee_imagen(ruta, &im, err, sizeof err)) {
			dlg_aviso("No se puede pegar", err);
			break;
		}
		vista_a_lamina(VI.x, VI.y, &x, &y);
		sel_pega(&im, x < 0 ? 0 : x, y < 0 ? 0 : y, "Pegar desde un archivo");
		lam_suelta(&im);
		pin_aviso("Pegada %s: arrástrela donde la quiera", sin_carpeta(ruta));
		break;
	}
	case DLG_COPIAR_A: {
		int f = fmt_tipo[tipo_elegido], fe = fi_por_extension(ruta);

		if (fe && fi_se_escribe(fe))
			f = fe;
		pin_ruta_con_ext(ruta, sizeof ruta, f);
		if (!sel_copia() || pin_guarda_lamina(&P.papel, ruta, f, P.calidad, err, sizeof err))
			dlg_aviso("No se puede copiar a un archivo", err);
		else
			pin_aviso("Lo elegido, guardado en %s", sin_carpeta(ruta));
		break;
	}
	}
}

static void abre_abrir(void)
{
	static const char *const titulos[] = { "Abrir", "Guardar como", "Exportar a PDF", "Pegar desde un archivo", "Copiar a un archivo" };
	int an = 560, al, lh = U * 10 + 8;

	nuevo_cuadro(titulos[modo_abrir], an, 0);
	lee_carpeta();
	al = 12 + U + 6 + lh + 10 + H + 10 + H + 10 + H + 12;
	Q.al = al;
	ctl(UC_ROTULO, 0, 14, 12, an - 140, rot_carpeta);
	boton(ID_SUBIR, an - 120, 8, 106, "&Subir", 0);
	lista(ID_LISTA, 14, 12 + U + 6, an - 28, lh, ops_lista, 0);
	rotulo(14, 12 + U + 6 + lh + 10, "&Nombre:");
	campo(ID_NOMBRE, 110, 12 + U + 6 + lh + 10, an - 124, nombre_puesto);
	if (modo_abrir != DLG_PDF) {
		rotulo(14, 12 + U + 6 + lh + 20 + H, "&Tipo:");
		desple(ID_TIPO, 110, 12 + U + 6 + lh + 20 + H, an - 124,
		       modo_abrir == DLG_GUARDAR || modo_abrir == DLG_COPIAR_A ? TIPOS_GUARDAR : TIPOS_ABRIR, tipo_elegido);
	}
	aceptar_cancelar(modo_abrir == DLG_GUARDAR || modo_abrir == DLG_COPIAR_A ? "&Guardar" : modo_abrir == DLG_PDF ? "&Exportar" :
			 modo_abrir == DLG_PEGAR_DE ? "&Pegar" : "&Abrir");
	abre(al_abrir);
	{
		int i, guardar = modo_abrir == DLG_GUARDAR || modo_abrir == DLG_PDF || modo_abrir == DLG_COPIAR_A;

		for (i = 0; i < Q.n; i++)
			if (Q.c[i].id == (guardar ? ID_NOMBRE : ID_LISTA))
				Q.foco = i;
		if (guardar) {
			ui_control *n = c_(ID_NOMBRE);

			n->ancla = 0;
			n->cursor = (int)utf8_cuenta(n->valor, largo(n->valor));
		}
	}
}

void dlg_abrir(int modo)
{
	int k;

	modo_abrir = modo;
	nombre_puesto[0] = 0;
	tipo_elegido = 0;
	if (modo == DLG_GUARDAR || modo == DLG_COPIAR_A) {
		for (k = 0; k < 5; k++)
			if (fmt_tipo[k] == P.formato)
				tipo_elegido = k;
		if (modo == DLG_GUARDAR && P.ruta[0]) {
			char *pt;

			copia(nombre_puesto, sin_carpeta(P.ruta), sizeof nombre_puesto);
			if (!fi_se_escribe(P.formato) && (pt = strchr(nombre_puesto, '.')) != NULL)
				*pt = 0;
		} else
			copia(nombre_puesto, modo == DLG_GUARDAR ? "IMAGEN.PNG" : "TROZO.PNG", sizeof nombre_puesto);
		if (!fi_se_escribe(P.formato))
			tipo_elegido = 0;
	}
	if (modo == DLG_PDF) {
		char *pt;

		copia(nombre_puesto, P.ruta[0] ? sin_carpeta(P.ruta) : "IMAGEN", sizeof nombre_puesto);
		pt = strchr(nombre_puesto, '.');
		if (pt)
			*pt = 0;
		anade(nombre_puesto, ".PDF", sizeof nombre_puesto);
	}
	abre_abrir();
}

/* ---- la calidad del JPG ---- */
#define ID_CALIDAD 20
static void al_calidad(int id)
{
	if (id != B_ACEPTAR) {
		P.pendiente = 0;
		return;
	}
	P.calidad = num(ID_CALIDAD);
	if (pin_guarda(ruta_calidad, FI_JPG) == 0)
		sigue_pendiente();
}

void dlg_calidad(const char *ruta)
{
	static char t[300];

	nuevo_cuadro("Calidad del JPG", 440, 3 * U + 2 * H + 52);
	fmt(t, sizeof t, "%s: cuanto más calidad, más se parece y más ocupa.", sin_carpeta(ruta));
	texto(16, 14, Q.an - 32, U * 2, t);
	rotulo(16, 22 + 2 * U, "&Calidad (1-100):");
	numero_c(ID_CALIDAD, 190, 18 + 2 * U, 110, P.calidad, 1, 100, NULL);
	aceptar_cancelar("&Guardar");
	abre(al_calidad);
}

/* ===================================================================== */
/*  Imagen nueva, tamaño, inclinar, girar, atributos, brillo             */
/* ===================================================================== */
#define ID_AN 30
#define ID_AL 31
#define ID_FONDO 32
#define ID_PORC 33
#define ID_PUNTOS 34
#define ID_PROP 35
#define ID_H 36
#define ID_V 37
#define ID_IH 38
#define ID_IV 39
static const char *const FONDOS = "Blanco|Transparente|El color 2";

static u32 fondo_elegido(int i)
{
	return i == 1 ? 0 : i == 2 ? P.c2 : 0xFFFFFFFFu;
}

static int cabe_en_memoria(int an, int al)
{
	void *p = so_pide((size_t)an * (size_t)al * 4);

	if (!p)
		return 0;
	so_suelta(p);
	return 1;
}

static void al_nuevo(int id)
{
	int an = num(ID_AN), al = num(ID_AL);
	lamina n;

	if (id != B_ACEPTAR)
		return;
	if (!lam_nueva(&n, an, al, fondo_elegido(c_(ID_FONDO)->elegida))) {
		char t[200];

		fmt(t, sizeof t, "No hay memoria para una imagen de %d x %d\n(harían falta %u MB, más lo de deshacer).", an, al,
		    (u32)(lam_octetos(an, al) >> 20) + 1);
		dlg_aviso("Imagen nueva", t);
		return;
	}
	herr_cancela();
	lam_suelta(&P.img);
	P.img = n;
	his_lamina(&P.img, 1);
	P.ruta[0] = 0;
	P.formato = FI_PNG;
	P.cambiado = 0;
	P.dx = P.dy = 0;
	P.zoom = 1000;
	pin_cambio_imagen();
	if (VI.san > VI.an || VI.sal > VI.al) {
		P.zoom = vista_zoom_ajustado();
		pin_cambio_imagen();
	}
	pin_aviso("Imagen nueva de %d x %d", an, al);
	dice("pintura: imagen nueva de %d x %d", an, al);
}

void dlg_nuevo(void)
{
	nuevo_cuadro("Imagen nueva", 380, 3 * (H + 10) + H + 36);
	rotulo(16, 18, "&Ancho:");
	numero_c(ID_AN, 140, 14, 140, P.img.an, 1, LAM_MAX_LADO, "px");
	rotulo(16, 18 + H + 10, "A&lto:");
	numero_c(ID_AL, 140, 14 + H + 10, 140, P.img.al, 1, LAM_MAX_LADO, "px");
	rotulo(16, 18 + 2 * (H + 10), "&Fondo:");
	desple(ID_FONDO, 140, 14 + 2 * (H + 10), 200, FONDOS, 0);
	aceptar_cancelar("&Crear");
	abre(al_nuevo);
}

/* cambiar el tamaño: en % o en puntos, con la proporción */
static int t_an0, t_al0, t_ph, t_pv, t_h, t_v;

static void al_cambiar_tamano(void)
{
	int porc = c_(ID_PORC)->marcado, prop = c_(ID_PROP)->marcado, h = num(ID_H), v = num(ID_V);

	if (porc != t_ph) {
		/* (de % a puntos o al revés: los mismos números, en la otra unidad) */
		if (porc) {
			pon_num(ID_H, (int)((i64)h * 100 / t_an0));
			pon_num(ID_V, (int)((i64)v * 100 / t_al0));
		} else {
			pon_num(ID_H, (int)((i64)h * t_an0 / 100));
			pon_num(ID_V, (int)((i64)v * t_al0 / 100));
		}
		c_(ID_H)->max = c_(ID_V)->max = porc ? 5000 * 10 : LAM_MAX_LADO * 10;
		c_(ID_H)->unidad = c_(ID_V)->unidad = porc ? "%" : "px";
		t_ph = porc;
		t_h = num(ID_H);
		t_v = num(ID_V);
		return;
	}
	if (prop) {
		if (h != t_h)
			pon_num(ID_V, porc ? h : (int)(((i64)h * t_al0 + t_an0 / 2) / t_an0));
		else if (v != t_v)
			pon_num(ID_H, porc ? v : (int)(((i64)v * t_an0 + t_al0 / 2) / t_al0));
	}
	t_h = num(ID_H);
	t_v = num(ID_V);
	(void)t_pv;
}

static void al_tamano(int id)
{
	int an, al, porc = c_(ID_PORC)->marcado;

	if (id != B_ACEPTAR)
		return;
	an = porc ? (int)(((i64)t_an0 * num(ID_H) + 50) / 100) : num(ID_H);
	al = porc ? (int)(((i64)t_al0 * num(ID_V) + 50) / 100) : num(ID_V);
	if (an < 1)
		an = 1;
	if (al < 1)
		al = 1;
	if (an > LAM_MAX_LADO || al > LAM_MAX_LADO) {
		dlg_aviso("Cambiar el tamaño", "La imagen no puede pasar de 4096 x 4096 puntos.");
		return;
	}
	if (!cabe_en_memoria(an, al)) {
		dlg_aviso("Cambiar el tamaño", "No hay memoria para una imagen tan grande.");
		return;
	}
	pin_aplica(A_ESCALA, an, al, 0, "Cambiar el tamaño");
}

void dlg_tamano(void)
{
	static char t[120];
	lamina *s = P.sel.activa ? (P.sel.flota ? &P.sel.flotante : NULL) : NULL;

	t_an0 = s ? s->an : P.sel.activa ? P.sel.an : P.img.an;
	t_al0 = s ? s->al : P.sel.activa ? P.sel.al : P.img.al;
	fmt(t, sizeof t, "%s: %d x %d puntos", P.sel.activa ? "Lo elegido" : "La imagen", t_an0, t_al0);
	nuevo_cuadro("Cambiar el tamaño", 420, U + 5 * (H + 10) + H + 40);
	texto(16, 12, Q.an - 32, U, t);
	opcion(ID_PORC, 16, 18 + U, 180, "Por &porcentaje", 1, 1);
	opcion(ID_PUNTOS, 200, 18 + U, 180, "Por p&untos", 1, 0);
	rotulo(16, 22 + U + H + 10, "&Horizontal:");
	numero_c(ID_H, 160, 18 + U + H + 10, 140, 100, 1, 5000, "%");
	rotulo(16, 22 + U + 2 * (H + 10), "&Vertical:");
	numero_c(ID_V, 160, 18 + U + 2 * (H + 10), 140, 100, 1, 5000, "%");
	casilla(ID_PROP, 16, 18 + U + 3 * (H + 10), 300, "&Mantener la proporción", 1);
	aceptar_cancelar(NULL);
	abre(al_tamano);
	t_ph = 1;
	t_h = t_v = 100;
	P.al_cambiar = al_cambiar_tamano;
}

static void al_inclinar(int id)
{
	int h = num(ID_H), v = num(ID_V), ih = num(ID_IH), iv = num(ID_IV);
	int an0 = P.sel.flota ? P.sel.flotante.an : P.sel.activa ? P.sel.an : P.img.an;
	int al0 = P.sel.flota ? P.sel.flotante.al : P.sel.activa ? P.sel.al : P.img.al;

	if (id != B_ACEPTAR)
		return;
	if (h != 100 || v != 100) {
		int an = (int)(((i64)an0 * h + 50) / 100), al = (int)(((i64)al0 * v + 50) / 100);

		if (an < 1 || al < 1 || an > LAM_MAX_LADO || al > LAM_MAX_LADO || !cabe_en_memoria(an, al)) {
			dlg_aviso("Estirar e inclinar", "Así pasaría de 4096 x 4096 puntos (o no hay memoria).");
			return;
		}
		pin_aplica(A_ESCALA, an, al, 0, "Estirar");
	}
	if (ih || iv)
		pin_aplica(A_INCLINA, ih, iv, 0, "Inclinar");
}

void dlg_inclinar(void)
{
	int y = 14;

	nuevo_cuadro("Estirar e inclinar", 420, 6 * (H + 10) + 30);
	rotulo(16, y, "Estirar:");
	y += U + 4;
	rotulo(30, y + 4, "&Horizontal:");
	numero_c(ID_H, 200, y, 130, 100, 1, 1000, "%");
	y += H + 8;
	rotulo(30, y + 4, "&Vertical:");
	numero_c(ID_V, 200, y, 130, 100, 1, 1000, "%");
	y += H + 14;
	rotulo(16, y, "Inclinar:");
	y += U + 4;
	rotulo(30, y + 4, "H&orizontal:");
	numero_c(ID_IH, 200, y, 130, 0, -89, 89, "°");
	y += H + 8;
	rotulo(30, y + 4, "V&ertical:");
	numero_c(ID_IV, 200, y, 130, 0, -89, 89, "°");
	aceptar_cancelar(NULL);
	abre(al_inclinar);
}

#define ID_GRADOS 40
static void al_girar(int id)
{
	ui_control *c = c_(ID_GRADOS);

	if (id != B_ACEPTAR || !c)
		return;
	pin_aplica(A_GIRA, 0, 0, ui_numero(c) / 10.0, "Girar");
}

void dlg_girar(void)
{
	ui_control *c;

	nuevo_cuadro("Girar", 420, U * 2 + 2 * H + 46);
	texto(16, 12, Q.an - 32, U * 2, "Grados, a la derecha (los negativos, a la izquierda).\nEl lienzo crece para que quepa la imagen girada.");
	rotulo(16, 22 + 2 * U, "&Grados:");
	c = ctl(UC_NUMERO, ID_GRADOS, 140, 18 + 2 * U, 150, NULL);
	c->min = -3600;
	c->max = 3600;
	c->paso = 50;
	c->decimales = 1;
	c->unidad = "°";
	ui_pon_numero(c, 150);
	aceptar_cancelar(NULL);
	abre(al_girar);
}

static void al_atributos(int id)
{
	int an = num(ID_AN), al = num(ID_AL);
	u32 f = fondo_elegido(c_(ID_FONDO)->elegida);

	if (id != B_ACEPTAR || (an == P.img.an && al == P.img.al))
		return;
	if (!cabe_en_memoria(an, al)) {
		dlg_aviso("Atributos del lienzo", "No hay memoria para un lienzo tan grande.");
		return;
	}
	{
		u32 c2 = P.c2;

		P.c2 = f;	/* (lo nuevo, de ese color) */
		pin_aplica(A_LIENZO, an, al, 0, "Tamaño del lienzo");
		P.c2 = c2;
	}
}

void dlg_atributos(void)
{
	static char t[200];
	u32 mb = (u32)(lam_octetos(P.img.an, P.img.al) >> 10);

	fmt(t, sizeof t, "Ahora: %d x %d puntos (%u KB en la memoria). Lo que crece, abajo y a la derecha.", P.img.an, P.img.al, mb);
	nuevo_cuadro("Atributos del lienzo", 470, U * 2 + 3 * (H + 10) + H + 40);
	texto(16, 12, Q.an - 32, U * 2, t);
	rotulo(16, 22 + 2 * U, "&Ancho:");
	numero_c(ID_AN, 150, 18 + 2 * U, 140, P.img.an, 1, LAM_MAX_LADO, "px");
	rotulo(16, 22 + 2 * U + H + 10, "A&lto:");
	numero_c(ID_AL, 150, 18 + 2 * U + H + 10, 140, P.img.al, 1, LAM_MAX_LADO, "px");
	rotulo(16, 22 + 2 * U + 2 * (H + 10), "Lo &nuevo:");
	desple(ID_FONDO, 150, 18 + 2 * U + 2 * (H + 10), 200, FONDOS, 2);
	aceptar_cancelar(NULL);
	abre(al_atributos);
}

static void al_brillo(int id)
{
	if (id != B_ACEPTAR)
		return;
	pin_aplica(A_BRILLO, num(ID_H), num(ID_V), 0, "Brillo y contraste");
}

void dlg_brillo(void)
{
	nuevo_cuadro("Brillo y contraste", 400, U + 2 * (H + 10) + H + 40);
	texto(16, 12, Q.an - 32, U, "De -100 a 100 (0: como está).");
	rotulo(16, 22 + U, "&Brillo:");
	numero_c(ID_H, 160, 18 + U, 130, 0, -100, 100, NULL);
	rotulo(16, 22 + U + H + 10, "&Contraste:");
	numero_c(ID_V, 160, 18 + U + H + 10, 130, 0, -100, 100, NULL);
	aceptar_cancelar(NULL);
	abre(al_brillo);
}

/* ===================================================================== */
/*  El editor de colores                                                 */
/* ===================================================================== */
#define ID_R 50
#define ID_G 51
#define ID_B 52
#define ID_A 53
#define ID_HUE 54
#define ID_SAT 55
#define ID_VAL 56
#define ID_COD 57
#define ID_MUESTRA 58
#define ID_C1 59
#define ID_C2 60
static u32 ed_color;
static int ed_r, ed_g, ed_b, ed_a, ed_h, ed_s, ed_v;
static char ed_cod[16];
#define SV 112

static void ed_pon_todo(int menos)
{
	char cod[16];

	ed_r = (ed_color >> 16) & 255;
	ed_g = (ed_color >> 8) & 255;
	ed_b = ed_color & 255;
	ed_a = (int)ALFA(ed_color);
	if (menos != 1) {
		pon_num(ID_R, ed_r);
		pon_num(ID_G, ed_g);
		pon_num(ID_B, ed_b);
		pon_num(ID_A, ed_a);
	}
	if (menos != 2) {
		color_a_hsv(ed_color, &ed_h, &ed_s, &ed_v);
		pon_num(ID_HUE, ed_h);
		pon_num(ID_SAT, ed_s);
		pon_num(ID_VAL, ed_v);
	}
	color_codigo(ed_color, cod, sizeof cod);
	copia(ed_cod, cod, sizeof ed_cod);
	if (menos != 3) {
		ui_control *c = c_(ID_COD);

		copia(c->valor, cod, sizeof c->valor);
		c->cursor = c->ancla = (int)largo(cod);
	}
}

static void al_cambiar_color(void)
{
	int r = num(ID_R), g = num(ID_G), b = num(ID_B), a = num(ID_A), h = num(ID_HUE), s = num(ID_SAT), v = num(ID_VAL);
	const char *cod = c_(ID_COD)->valor;
	u32 c;

	if (r != ed_r || g != ed_g || b != ed_b || a != ed_a) {
		ed_color = (u32)a << 24 | (u32)r << 16 | (u32)g << 8 | (u32)b;
		ed_pon_todo(1);
	} else if (h != ed_h || s != ed_s || v != ed_v) {
		ed_color = (color_de_hsv(h, s, v) & 0xFFFFFF) | (u32)ed_a << 24;
		ed_pon_todo(2);
		ed_h = h;
		ed_s = s;
		ed_v = v;
	} else if (!iguales(cod, ed_cod) && color_de_codigo(cod, &c)) {
		ed_color = c;
		ed_pon_todo(3);
	}
}

static void muestra_color(void *u, lienzo *l, const ui_control *c, int x, int y)
{
	char d[120];
	int i, j;

	(void)u;
	(void)c;
	/* el cuadro de saturación (a lo ancho) y brillo (a lo alto) del tono */
	for (j = 0; j < SV; j++)
		for (i = 0; i < SV; i++)
			l->p[(size_t)(y + 6 + j) * l->paso + x + 6 + i] = color_de_hsv(ed_h, i * 100 / (SV - 1), 100 - j * 100 / (SV - 1)) & 0xFFFFFF;
	lz_marco(l, x + 5, y + 5, SV + 2, SV + 2, UI_TINTA);
	/* dónde está (un aro negro y blanco: se ve sobre cualquier color) */
	{
		int cx = x + 6 + ed_s * (SV - 1) / 100, cy = y + 6 + (100 - ed_v) * (SV - 1) / 100;

		lz_marco(l, cx - 4, cy - 4, 9, 9, 0x000000);
		lz_marco(l, cx - 3, cy - 3, 7, 7, 0xFFFFFF);
	}
	/* la barra de los tonos */
	for (j = 0; j < SV; j++) {
		u32 t = color_de_hsv(j * 359 / (SV - 1), 100, 100) & 0xFFFFFF;

		lz_raya_h(l, x + SV + 14, y + 6 + j, 18, t);
	}
	lz_marco(l, x + SV + 13, y + 5, 20, SV + 2, UI_TINTA);
	{
		int hy = y + 6 + ed_h * (SV - 1) / 359;

		lz_rect(l, x + SV + 10, hy - 1, 26, 3, 0x000000);
		lz_raya_h(l, x + SV + 11, hy, 24, 0xFFFFFF);
	}
	/* el color, con su nombre */
	{
		int mx = x + SV + 46, w = c->an - SV - 54;

		for (j = 0; j < 56; j += 7)
			for (i = 0; i < w; i += 7)
				lz_rect(l, mx + i, y + 6 + j, w - i < 7 ? w - i : 7, 7, ((i / 7 + j / 7) & 1) ? 0xCDCDCD : 0xFFFFFF);
		lz_rect_a(l, mx, y + 6, w, 56, ed_color & 0xFFFFFF, (int)ALFA(ed_color));
		lz_marco(l, mx - 1, y + 5, w + 2, 58, UI_TINTA);
		color_describe(ed_color, d, sizeof d);
		{
			/* (en dos líneas si no cabe) */
			char a[120], *p = strchr(d, '(');

			if (p && lz_ui_ancho(d) > w) {
				copia(a, d, (u32)(p - d) + 1);
				lz_ui(l, mx, y + 72 + lz_ui_base(), a, UI_TINTA);
				lz_ui(l, mx, y + 72 + lz_ui_alto() + lz_ui_base(), p, UI_TINTA);
			} else
				lz_ui(l, mx, y + 72 + lz_ui_base(), d, UI_TINTA);
		}
	}
}

/* los clics en el cuadro de los tonos (los ve la ventana antes que el cuadro) */
static int raton_color(const so_suceso *s)
{
	ui_control *c = c_(ID_MUESTRA);
	int th = lz_ui_alto() + 10, x, y;

	if (!c || !P.cuadro || !(s->clase == SO_BAJA || (s->clase == SO_MUEVE && (s->botones & SO_BOTON_IZQ))))
		return 0;
	x = s->x - (Q.x + c->x) - 6;
	y = s->y - (Q.y + th + c->y) - 6;
	if (y < -4 || y > SV + 4)
		return 0;
	if (x >= -4 && x < SV + 4) {
		x = x < 0 ? 0 : x >= SV ? SV - 1 : x;
		y = y < 0 ? 0 : y >= SV ? SV - 1 : y;
		ed_s = x * 100 / (SV - 1);
		ed_v = 100 - y * 100 / (SV - 1);
		ed_color = (color_de_hsv(ed_h, ed_s, ed_v) & 0xFFFFFF) | (u32)ed_a << 24;
		{
			int h = ed_h, sa = ed_s, v = ed_v;

			ed_pon_todo(2);
			ed_h = h;
			ed_s = sa;
			ed_v = v;
			pon_num(ID_HUE, h);
			pon_num(ID_SAT, sa);
			pon_num(ID_VAL, v);
		}
		return 1;
	}
	if (x >= SV + 4 && x < SV + 30) {
		y = y < 0 ? 0 : y >= SV ? SV - 1 : y;
		ed_h = y * 359 / (SV - 1);
		ed_color = (color_de_hsv(ed_h, ed_s, ed_v) & 0xFFFFFF) | (u32)ed_a << 24;
		{
			int h = ed_h, sa = ed_s, v = ed_v;

			ed_pon_todo(2);
			ed_h = h;
			ed_s = sa;
			ed_v = v;
			pon_num(ID_HUE, h);
		}
		return 1;
	}
	return 0;
}

static void al_colores(int id)
{
	int cual = c_(ID_C2)->marcado ? 2 : 1;
	char d[120];

	if (id != B_ACEPTAR) {
		paleta_editada = -1;
		return;
	}
	al_cambiar_color();
	pin_elige_color(cual, ed_color);
	if (paleta_editada >= 0 && paleta_editada < PALETA_N)
		paleta[paleta_editada] = ed_color;
	paleta_editada = -1;
	color_describe(ed_color, d, sizeof d);
	pin_aviso("Color %d: %s", cual, d);
	dice("pintura: color %d: %s", cual, d);
}

void dlg_colores(int cual)
{
	int y0, x2 = 250;

	ed_color = cual == 2 ? P.c2 : P.c1;
	nuevo_cuadro(paleta_editada >= 0 ? "Editar el color (y el de la paleta)" : "Editar colores", 540, 0);
	ctl(UC_MUESTRA, ID_MUESTRA, 16, 12, Q.an - 32, NULL)->al = SV + 14;
	Q.pinta_extra = muestra_color;
	y0 = 12 + SV + 24;
	rotulo(16, y0 + 4, "&Rojo:");
	numero_c(ID_R, 110, y0, 110, 0, 0, 255, NULL);
	rotulo(x2, y0 + 4, "&Tono:");
	numero_c(ID_HUE, x2 + 110, y0, 110, 0, 0, 359, "°");
	y0 += H + 8;
	rotulo(16, y0 + 4, "&Verde:");
	numero_c(ID_G, 110, y0, 110, 0, 0, 255, NULL);
	rotulo(x2, y0 + 4, "&Saturación:");
	numero_c(ID_SAT, x2 + 110, y0, 110, 0, 0, 100, "%");
	y0 += H + 8;
	rotulo(16, y0 + 4, "A&zul:");
	numero_c(ID_B, 110, y0, 110, 0, 0, 255, NULL);
	rotulo(x2, y0 + 4, "&Brillo:");
	numero_c(ID_VAL, x2 + 110, y0, 110, 0, 0, 100, "%");
	y0 += H + 8;
	rotulo(16, y0 + 4, "&Opacidad:");
	numero_c(ID_A, 110, y0, 110, 0, 0, 255, NULL);
	rotulo(x2, y0 + 4, "&Código:");
	campo(ID_COD, x2 + 110, y0, 110, "");
	y0 += H + 12;
	opcion(ID_C1, 16, y0, 200, "Para el color &1", 1, cual != 2);
	opcion(ID_C2, x2, y0, 200, "Para el color &2", 1, cual == 2);
	y0 += H + 10;
	Q.al = y0 + H + 14;
	aceptar_cancelar(NULL);
	abre(al_colores);
	ed_pon_todo(0);
	P.al_cambiar = al_cambiar_color;
	P.al_raton = raton_color;
}

/* ===================================================================== */
/*  Propiedades, ayuda y acerca de                                       */
/* ===================================================================== */
void dlg_propiedades(void)
{
	static char t[900];
	u32 n = (u32)P.img.an * (u32)P.img.al, i, transp = 0;

	for (i = 0; i < n; i++)
		if (ALFA(P.img.p[i]) != 255)
			transp++;
	fmt(t, sizeof t, "Archivo: %s\nFormato: %s\nTamaño: %d x %d puntos (%u KB en la memoria)\nTransparencia: %s\n"
	    "Deshacer: %d pasos guardados (%u KB, comprimidos)\nMemoria de Pintura: %u KB",
	    P.ruta[0] ? P.ruta : "(sin guardar)", fi_nombre(P.formato), P.img.an, P.img.al, n / 256,
	    transp ? "sí" : "no (toda opaca)", his_pasos(), his_memoria() / 1024, so_memoria_usada() / 1024);
	(void)transp;
	dlg_aviso("Propiedades de la imagen", t);
}

void dlg_ayuda(int teclas)
{
	if (teclas)
		dlg_aviso("Las teclas de Pintura",
			  "Ctrl+U nueva, Ctrl+A abrir, Ctrl+G guardar, F12 guardar como, Ctrl+P imprimir (a PDF)\n"
			  "Ctrl+Z deshacer, Ctrl+Y rehacer, Ctrl+X cortar, Ctrl+C copiar, Ctrl+V pegar\n"
			  "Ctrl+E seleccionar todo, Supr borrar lo elegido, Esc quitar la selección\n"
			  "Las flechas mueven lo elegido (con Mayús, de 10 en 10); sin selección, la vista\n"
			  "Ctrl+W cambiar el tamaño, Ctrl+R girar a la derecha, Ctrl+K recortar, Ctrl+T atributos\n"
			  "Ctrl+N invertir los colores, X intercambiar el color 1 y el 2\n"
			  "+ y - (o Control y la rueda) acercar y alejar; Mayús y la rueda, a los lados\n"
			  "Las herramientas: S, M, L, P, A, B, R, C, T, Z, I, U, Q, O, E y G (su letra sale en el globo)");
	else
		dlg_aviso("Ayuda de Pintura",
			  "Pintura 2: dibujar y retocar imágenes en color verdadero, con transparencia.\n\n"
			  "Con el botón izquierdo se pinta con el color 1 y con el derecho, con el 2. Los dos colores\n"
			  "se ven abajo, a la izquierda de la paleta, con su nombre; al pasar el ratón por la imagen,\n"
			  "la barra de estado dice el nombre y el código del color que hay debajo.\n\n"
			  "Las figuras: el contorno, con el color 1 y el grosor de la barra de opciones; el relleno\n"
			  "(si se pide), con el color 2. Con Mayús, cuadrados, círculos y rayas a 45 grados.\n"
			  "Lo elegido se arrastra para moverlo; con Control, se copia. Transparente: lo del color 2 no tapa.\n"
			  "Las asas de la esquina del lienzo cambian su tamaño. Deshacer no tiene límite.\n\n"
			  "Abre PNG, JPG, BMP, GIF, PCX, ICO y TGA; guarda en PNG, JPG, BMP, GIF y PCX; exporta a PDF.\n"
			  "Archivo > Usar como fondo de escritorio pone la imagen en el escritorio.");
}

void dlg_acerca(void)
{
	dlg_aviso("Acerca de Pintura",
		  "Pintura " PINTURA_VERSION " de Hispa-Pentium (32 bits, en C portable)\n"
		  "Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.\n\n"
		  "Lleva dentro: stb_image, stb_image_write y stb_truetype (Sean Barrett; dominio público o MIT),\n"
		  "miniz (MIT) y las letras DejaVu (licencia de Bitstream Vera).");
}
