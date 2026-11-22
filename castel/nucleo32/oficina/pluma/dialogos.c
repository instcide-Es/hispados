/* dialogos.c - Pluma 2: los cuadros de diálogo (abrir y guardar, fuente,
 * párrafo, tabulaciones, página, tabla, imagen, buscar y reemplazar, ir a,
 * zoom, contar, encabezado y pie, fecha, símbolo, estilos, resumen,
 * ortografía, opciones...). Son de oficina/comun/ui.h y van dentro de la
 * ventana de Pluma (uno a la vez).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "ventana.h"
#include "archivo.h"
#include "maqueta.h"
#include "letras.h"
#include "texto.h"
#include "pluma.h"

static ui_control C[48];
static int nC;
static ui_cuadro Q;
static int U, H;		/* el renglón y lo alto de un control */

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
	P.sucio = 1;
	dice("pluma: cuadro «%s»", q->titulo);
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

static ui_control *numero_c(int id, int x, int y, int an, int dec, int min, int max, int paso, int decimales, const char *unidad)
{
	ui_control *c = ctl(UC_NUMERO, id, x, y, an, NULL);

	c->min = min;
	c->max = max;
	c->paso = paso;
	c->decimales = decimales;
	c->unidad = unidad;
	ui_pon_numero(c, dec);
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

/* Aceptar y Cancelar abajo a la derecha */
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

/* cm (en décimas) y twips */
static int dec_cm(i32 tw)
{
	return (int)(((i64)tw * 10 + (tw >= 0 ? 283 : -283)) / 567);
}

static i32 tw_cm(int dec)
{
	return (i32)((i64)dec * 567 / 10);
}

/* (la n-ésima opción de «a|b|c») */
static void opcion_n(const char *ops, int n, char *d, u32 cabe)
{
	int k = 0;
	u32 j = 0;

	while (*ops && k < n) {
		if (*ops == '|')
			k++;
		ops++;
	}
	while (*ops && *ops != '|' && j + 1 < cabe)
		d[j++] = *ops++;
	d[j] = 0;
}

/* ===================================================================== */
/*  Avisos y preguntas                                                   */
/* ===================================================================== */
static char aviso_txt[700];

static void al_aviso(int id)
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
		/* lo ancho: la línea más larga */
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
	abre(al_aviso);
	dice("pluma: aviso «%s»: %s", titulo, t);
}

/* ¿guardar los cambios? */
#define B_NO_GUARDAR 3
static char pregunta[300];

static void al_guardar_cambios(int id)
{
	int o = P.pendiente;

	if (id == B_ACEPTAR) {
		if (!P.ruta[0] || P.formato == FMT_TXT) {
			dlg_abrir(1, 0);	/* (al guardar, sigue P.pendiente) */
			return;
		}
		if (pl_guarda(P.ruta, P.formato))
			return;
		P.pendiente = 0;
		pl_orden(o);
	} else if (id == B_NO_GUARDAR) {
		P.d->cambiado = 0;
		P.pendiente = 0;
		pl_orden(o);
	} else
		P.pendiente = 0;
}

void dlg_guardar_cambios(int orden)
{
	P.pendiente = orden;
	fmt(pregunta, sizeof pregunta, "¿Guardar los cambios de %s?", P.ruta[0] ? sin_carpeta(P.ruta) : "Documento1");
	nuevo_cuadro("Pluma", lz_ui_ancho(pregunta) + 60 < 420 ? 420 : lz_ui_ancho(pregunta) + 60, U * 2 + H + 40);
	texto(20, 16, Q.an - 40, U, pregunta);
	boton(B_ACEPTAR, 12, Q.al - H - 12, 120, "&Guardar", 1);
	boton(B_NO_GUARDAR, 144, Q.al - H - 12, 120, "&No guardar", 0);
	boton(B_CANCELAR, Q.an - 122, Q.al - H - 12, 110, "Cancelar", 0);
	abre(al_guardar_cambios);
}

/* ===================================================================== */
/*  Abrir y guardar                                                      */
/* ===================================================================== */
#define ID_LISTA 10
#define ID_NOMBRE 11
#define ID_TIPO 12
#define ID_SUBIR 13
#define MAX_ENT 400

static int modo_abrir;		/* 0 abrir, 1 guardar, 2 insertar archivo, 3 imagen; 4 exportar a PDF */
static char ent_nombre[MAX_ENT][64];
static u8 ent_carpeta[MAX_ENT];
static int n_ent;
static char ops_lista[MAX_ENT * 66];
static char rot_carpeta[700];
static const char *const TIPOS_GUARDAR = "Documento de Pluma (.PLU)|Documento de texto OpenDocument (.ODT)|Documento de Word (.DOCX)|Texto enriquecido (.RTF)|Texto sin formato (.TXT)";
static const int fmt_tipo[] = { FMT_PLU, FMT_ODT, FMT_DOCX, FMT_RTF, FMT_TXT };
static const char *const TIPOS_ABRIR = "Todos los documentos (PLU, ODT, DOCX, DOC, RTF, TXT)|Todos los archivos";
static const char *const TIPOS_IMAGEN = "Imágenes (PNG, JPG, BMP, GIF)|Todos los archivos";
static int tipo_elegido;
static char nombre_puesto[260];

static int vale_ext(const char *n, int carpeta)
{
	char e[8];
	int todos = (modo_abrir != 1 && modo_abrir != 4 && tipo_elegido == 1);

	if (carpeta || todos)
		return 1;
	extension(n, e);
	if (modo_abrir == 3)
		return iguales(e, "PNG") || iguales(e, "JPG") || iguales(e, "JPEG") || iguales(e, "JPE") || iguales(e, "BMP") || iguales(e, "GIF");
	if (modo_abrir == 4)
		return iguales(e, "PDF");
	if (modo_abrir == 1)
		return iguales(e, arch_extension(fmt_tipo[tipo_elegido])) || (fmt_tipo[tipo_elegido] == FMT_DOCX && iguales(e, "DOC"));
	return iguales(e, "PLU") || iguales(e, "ODT") || iguales(e, "DOCX") || iguales(e, "DOC") || iguales(e, "RTF") || iguales(e, "TXT");
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
	/* las carpetas primero, y por orden */
	for (i = 1; i < n_ent; i++) {
		char t[64];
		u8 k = ent_carpeta[i];

		copia(t, ent_nombre[i], sizeof t);
		for (j = i; j > 0; j--) {
			int antes = ent_carpeta[j - 1] > k || (ent_carpeta[j - 1] == k && !(minuscula((u8)ent_nombre[j - 1][0]) > minuscula((u8)t[0]) ||
				    (minuscula((u8)ent_nombre[j - 1][0]) == minuscula((u8)t[0]) && strcmp(ent_nombre[j - 1], t) > 0)));

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

static void al_abrir(int id)
{
	ui_control *l = c_(ID_LISTA), *n = c_(ID_NOMBRE), *t = c_(ID_TIPO);
	char ruta[700];
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
	/* (cambió el tipo: otra lista) */
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
	if (strchr(nom, ':') || nom[0] == '\\' || nom[0] == '/')
		copia(ruta, nom, sizeof ruta);
	else
		fmt(ruta, sizeof ruta, "%s\\%s", P.carpeta, nom);
	if (iguales(nom, "..")) {
		sube_carpeta();
		nombre_puesto[0] = 0;
		abre_abrir();
		return;
	}
	if (so_existe(ruta) == 2) {
		copia(P.carpeta, ruta, sizeof P.carpeta);
		nombre_puesto[0] = 0;
		abre_abrir();
		return;
	}
	switch (modo_abrir) {
	case 0:
		pl_abre(ruta);
		break;
	case 1:
	case 4: {
		int f = modo_abrir == 4 ? FMT_PDF : fmt_tipo[tipo_elegido];
		char e[8];

		extension(ruta, e);
		if (!e[0] || (arch_formato_ext(ruta) != f && !(f == FMT_DOCX && iguales(e, "DOC")))) {
			anade(ruta, ".", sizeof ruta);
			anade(ruta, modo_abrir == 4 ? "pdf" : arch_extension(f), sizeof ruta);
		}
		if (pl_guarda(ruta, f) == 0 && P.pendiente && modo_abrir == 1) {
			int o = P.pendiente;

			P.pendiente = 0;
			pl_orden(o);
		}
		break;
	}
	case 2: {
		char err[300];
		int f;
		documento *d = arch_abre(ruta, &f, err, sizeof err);

		if (!d) {
			dlg_aviso("No se puede insertar", err);
			break;
		}
		{
			/* su texto (sin formato), donde está el cursor */
			bufer b = { 0 };

			txt_escribe(d, &b, 1);
			buf_c(&b, 0);
			if (!b.mal)
				ed_pega(&P.e, (char *)b.d, b.n - 1);
			buf_suelta(&b);
		}
		doc_suelta(d);
		pl_cambio();
		break;
	}
	case 3: {
		char err[300];
		u32 m;
		u8 *datos = pl_lee_imagen(ruta, &m, err, sizeof err);

		if (!datos) {
			dlg_aviso("No se puede insertar la imagen", err);
			break;
		}
		ed_imagen(&P.e, datos, m);
		pl_cambio();
		pl_aviso("Imagen insertada: Formato > Imagen para su tamaño y el texto alrededor");
		break;
	}
	}
}

static void abre_abrir(void)
{
	static const char *const titulos[] = { "Abrir", "Guardar como", "Insertar archivo", "Insertar imagen", "Exportar a PDF" };
	int an = 560, al;
	int lh = U * 10 + 8;

	nuevo_cuadro(titulos[modo_abrir], an, 0);
	lee_carpeta();
	al = 12 + U + 6 + lh + 10 + H + 10 + H + 10 + H + 12;
	Q.al = al;
	ctl(UC_ROTULO, 0, 14, 12, an - 140, rot_carpeta);
	boton(ID_SUBIR, an - 120, 8, 106, "&Subir", 0);
	lista(ID_LISTA, 14, 12 + U + 6, an - 28, lh, ops_lista, 0);
	rotulo(14, 12 + U + 6 + lh + 10, "&Nombre:");
	campo(ID_NOMBRE, 110, 12 + U + 6 + lh + 10, an - 124, nombre_puesto);
	if (modo_abrir != 4) {
		rotulo(14, 12 + U + 6 + lh + 20 + H, "&Tipo:");
		desple(ID_TIPO, 110, 12 + U + 6 + lh + 20 + H, an - 124, modo_abrir == 1 ? TIPOS_GUARDAR : modo_abrir == 3 ? TIPOS_IMAGEN : TIPOS_ABRIR, tipo_elegido);
	}
	aceptar_cancelar(modo_abrir == 1 ? "&Guardar" : modo_abrir == 4 ? "&Exportar" : modo_abrir >= 2 ? "&Insertar" : "&Abrir");
	abre(al_abrir);
	/* el foco, en el nombre (al guardar) o en la lista */
	{
		int i;

		for (i = 0; i < Q.n; i++)
			if (Q.c[i].id == (modo_abrir == 1 || modo_abrir == 4 ? ID_NOMBRE : ID_LISTA))
				Q.foco = i;
		if (modo_abrir == 1 || modo_abrir == 4) {
			ui_control *n = c_(ID_NOMBRE);

			n->ancla = 0;
			n->cursor = (int)utf8_cuenta(n->valor, largo(n->valor));
		}
	}
}

void dlg_abrir(int modo, int exportar)
{
	modo_abrir = exportar ? 4 : modo;
	nombre_puesto[0] = 0;
	tipo_elegido = 0;
	if (modo_abrir == 1) {
		int k;

		for (k = 0; k < 5; k++)
			if (fmt_tipo[k] == P.formato)
				tipo_elegido = k;
		if (P.ruta[0])
			copia(nombre_puesto, sin_carpeta(P.ruta), sizeof nombre_puesto);
		else
			copia(nombre_puesto, "DOCUMEN1.PLU", sizeof nombre_puesto);
	}
	if (modo_abrir == 4) {
		/* el mismo nombre, con .PDF */
		char *pt;

		copia(nombre_puesto, P.ruta[0] ? sin_carpeta(P.ruta) : "DOCUMEN1", sizeof nombre_puesto);
		pt = strchr(nombre_puesto, '.');
		if (pt)
			*pt = 0;
		anade(nombre_puesto, ".PDF", sizeof nombre_puesto);
	}
	abre_abrir();
}

u8 *pl_lee_imagen(const char *ruta, u32 *n, char *error, u32 cabe)
{
	const char *e = NULL;
	u8 *d = so_lee_archivo(ruta, n, 32u * 1024 * 1024, &e);

	if (!d) {
		fmt(error, cabe, "%s: %s.", sin_carpeta(ruta), e ? e : "?");
		return NULL;
	}
	return d;
}

/* ===================================================================== */
/*  Fuente                                                               */
/* ===================================================================== */
#define ID_FAM 20
#define ID_ESTILO_L 21
#define ID_TAM 22
#define ID_SUB 23
#define ID_TACH 24
#define ID_VERS 25
#define ID_SUP 26
#define ID_SUBI 27
#define ID_COLOR 28
#define ID_RES 29
#define ID_MUESTRA 30
static const char *familias[32];
static int n_familias;
static char ops_fam[32 * 50];
static const char *const COLORES_D = "Automático (negro)|Rojo|Granate|Naranja|Amarillo oscuro|Verde|Verde azulado|Azul|Azul marino|Morado|Gris|Gris oscuro";
static const u32 colores_d[] = { SIN_COLOR, 0xC62828, 0x7B1E1E, 0xE65100, 0xB38B00, 0x2E7D32, 0x00796B, 0x1565C0, 0x1A237E, 0x6A1B9A, 0x757575, 0x424242 };
static const char *const RESALTES_D = "Sin resaltar|Amarillo|Verde claro|Turquesa|Rosa|Azul claro|Gris claro|Naranja claro";
static const u32 resaltes_d[] = { SIN_COLOR, 0xFFF176, 0xC5E1A5, 0x80DEEA, 0xF8BBD0, 0xBBDEFB, 0xE0E0E0, 0xFFCC80 };

static int indice_color(const u32 *t, int n, u32 c)
{
	int i;

	for (i = 0; i < n; i++)
		if (t[i] == c)
			return i;
	return 0;
}

static fmt_letra fuente_de_cuadro(void)
{
	fmt_letra f = ed_formato_actual(&P.e);
	int e = c_(ID_ESTILO_L)->elegida;
	ui_control *fam = c_(ID_FAM);

	if (fam->elegida >= 0 && fam->elegida < n_familias)
		copia(f.familia, familias[fam->elegida], sizeof f.familia);
	f.negrita = e == 1 || e == 3;
	f.cursiva = e == 2 || e == 3;
	{
		int t = ui_numero(c_(ID_TAM));

		if (t < 10)
			t = 10;
		if (t > 4000)
			t = 4000;
		f.tam = (u16)((t + 2) / 5);
	}
	f.subrayado = (u8)c_(ID_SUB)->marcado;
	f.tachado = (u8)c_(ID_TACH)->marcado;
	f.versalitas = (u8)c_(ID_VERS)->marcado;
	f.vert = c_(ID_SUP)->marcado ? 1 : c_(ID_SUBI)->marcado ? 2 : 0;
	f.color = colores_d[c_(ID_COLOR)->elegida];
	f.resalte = resaltes_d[c_(ID_RES)->elegida];
	return f;
}

static void muestra_fuente(void *u, lienzo *l, const ui_control *c, int x, int y)
{
	fmt_letra f = fuente_de_cuadro();
	int falsas, cara = letra_cara(f.familia, f.negrita, f.cursiva, &falsas);
	int tam = f.tam * 2 * 4 * 96 / 72 / 4;	/* (cuartos de punto, a 100%) */
	int w;
	const char *t = "Pluma: El veloz murciélago hindú";
	u32 col = f.color == SIN_COLOR ? 0 : f.color;

	(void)u;
	tam = (int)((i64)f.tam * 100 * 2 / 75);
	if (tam > 4 * 40)
		tam = 4 * 40;
	w = lz_ancho(cara, tam, falsas, t);
	if (f.resalte != SIN_COLOR)
		lz_rect(l, x + 10, y + c->al / 2 - tam / 6, w, tam / 3, f.resalte);
	lz_texto(l, cara, tam, falsas, x + 10, y + c->al / 2 + tam / 10, t, col);
	if (f.subrayado)
		lz_raya_h(l, x + 10, y + c->al / 2 + tam / 10 + 3, w, col);
	if (f.tachado)
		lz_raya_h(l, x + 10, y + c->al / 2 - tam / 20, w, col);
	if (!letra_hay_familia(f.familia)) {
		char aviso[120];

		fmt(aviso, sizeof aviso, "(«%s» no está: se ve con %s)", f.familia, letra_familia(cara));
		lz_ui(l, x + 6, y + c->al - 6, aviso, 0x8A4B00);
	}
}

static void al_fuente(int id)
{
	if (id == B_ACEPTAR) {
		fmt_letra f = fuente_de_cuadro();

		ed_formato(&P.e, CF_FAMILIA | CF_TAM | CF_NEGRITA | CF_CURSIVA | CF_SUBRAYADO | CF_TACHADO | CF_VERSALITAS | CF_VERT | CF_COLOR | CF_RESALTE, &f);
		pl_cambio();
	}
}

void dlg_fuente(void)
{
	fmt_letra f = ed_formato_actual(&P.e);
	int i, sel = 0, an = 600, lh;

	nuevo_cuadro("Fuentes", an, 0);
	n_familias = letras_familias(familias, 31);
	ops_fam[0] = 0;
	for (i = 0; i < n_familias; i++) {
		if (i)
			anade(ops_fam, "|", sizeof ops_fam);
		anade(ops_fam, familias[i], sizeof ops_fam);
		if (iguales_sin_caja(familias[i], f.familia))
			sel = i;
	}
	if (!letra_hay_familia(f.familia) && n_familias < 31) {
		/* la del documento, aunque no esté (para no perderla) */
		familias[n_familias] = ed_formato_actual(&P.e).familia;
		anade(ops_fam, "|", sizeof ops_fam);
		anade(ops_fam, f.familia, sizeof ops_fam);
		sel = n_familias++;
	}
	lh = U * 6 + 6;
	rotulo(14, 10, "&Fuente:");
	lista(ID_FAM, 14, 10 + U + 4, 250, lh, ops_fam, sel);
	rotulo(280, 10, "&Estilo:");
	desple(ID_ESTILO_L, 280, 10 + U + 4, 180, "Normal|Negrita|Cursiva|Negrita cursiva", (f.negrita ? 1 : 0) + (f.cursiva ? 2 : 0));
	rotulo(476, 10, "&Tamaño:");
	numero_c(ID_TAM, 476, 10 + U + 4, 100, f.tam * 5, 10, 4000, 10, 1, NULL);
	casilla(ID_SUB, 280, 10 + U + 14 + H, 150, "&Subrayado", f.subrayado);
	casilla(ID_TACH, 440, 10 + U + 14 + H, 150, "Ta&chado", f.tachado);
	casilla(ID_VERS, 280, 10 + U + 20 + 2 * H, 150, "&Versalitas", f.versalitas);
	casilla(ID_SUP, 440, 10 + U + 20 + 2 * H, 150, "Su&períndice", f.vert == 1);
	casilla(ID_SUBI, 440, 10 + U + 26 + 3 * H, 150, "Su&bíndice", f.vert == 2);
	{
		int y = 10 + U + 4 + lh + 12;

		rotulo(14, y, "C&olor:");
		desple(ID_COLOR, 100, y, 180, COLORES_D, indice_color(colores_d, 12, f.color));
		rotulo(300, y, "&Resaltado:");
		desple(ID_RES, 400, y, 180, RESALTES_D, indice_color(resaltes_d, 8, f.resalte));
		y += H + 12;
		ctl(UC_MUESTRA, ID_MUESTRA, 14, y, an - 28, NULL)->al = 76;
		Q.pinta_extra = muestra_fuente;
		Q.al = y + 76 + 16 + H + 12;
	}
	aceptar_cancelar(NULL);
	abre(al_fuente);
}

/* ===================================================================== */
/*  Párrafo                                                              */
/* ===================================================================== */
#define ID_ALIN 40
#define ID_SIZQ 41
#define ID_SDER 42
#define ID_SPRI 43
#define ID_ANTES 44
#define ID_DESP 45
#define ID_INTER 46
#define ID_INTERV 47
#define ID_SALTO 48
#define ID_JUNTOS 49
static const char *const INTERS = "Sencillo|1,15 líneas|1,5 líneas|Doble|Exacto (puntos)|Mínimo (puntos)";

static void al_parrafo(int id)
{
	fmt_parrafo p = P.d->par[P.e.cur.p].p;
	int it;

	if (id != B_ACEPTAR)
		return;
	p.alin = (u8)c_(ID_ALIN)->elegida;
	p.sang_izq = tw_cm(ui_numero(c_(ID_SIZQ)));
	p.sang_der = tw_cm(ui_numero(c_(ID_SDER)));
	p.sang_primera = tw_cm(ui_numero(c_(ID_SPRI)));
	p.antes = ui_numero(c_(ID_ANTES)) * TW_PT / 10;
	p.despues = ui_numero(c_(ID_DESP)) * TW_PT / 10;
	it = c_(ID_INTER)->elegida;
	if (it <= 3) {
		static const int m[] = { 100, 115, 150, 200 };

		p.inter_tipo = INTER_MULTIPLE;
		p.inter = m[it];
	} else {
		p.inter_tipo = it == 4 ? INTER_EXACTO : INTER_MINIMO;
		p.inter = ui_numero(c_(ID_INTERV)) * TW_PT / 10;
		if (p.inter < 40)
			p.inter = 240;
	}
	p.salto_antes = (u8)c_(ID_SALTO)->marcado;
	p.juntos = (u8)c_(ID_JUNTOS)->marcado;
	ed_formato_parrafo(&P.e, CP_ALIN | CP_SANG_IZQ | CP_SANG_DER | CP_SANG_PRIMERA | CP_ANTES | CP_DESPUES | CP_INTER | CP_SALTO | CP_JUNTOS, &p);
	pl_cambio();
}

void dlg_parrafo(void)
{
	const fmt_parrafo *p = &P.d->par[P.e.cur.p].p;
	int it = 0, y = 12, x2 = 300;

	if (p->inter_tipo == INTER_EXACTO)
		it = 4;
	else if (p->inter_tipo == INTER_MINIMO)
		it = 5;
	else
		it = p->inter >= 190 ? 3 : p->inter >= 140 ? 2 : p->inter >= 110 ? 1 : 0;
	nuevo_cuadro("Párrafo", 560, 0);
	rotulo(14, y, "&Alineación:");
	desple(ID_ALIN, 150, y, 200, "Izquierda|Centrada|Derecha|Justificada", p->alin);
	y += H + 14;
	rotulo(14, y, "Sangría (cm)");
	y += U + 4;
	rotulo(14, y, "&Izquierda:");
	numero_c(ID_SIZQ, 150, y, 110, dec_cm(p->sang_izq), -50, 300, 1, 1, NULL);
	rotulo(x2, y, "&Derecha:");
	numero_c(ID_SDER, x2 + 110, y, 110, dec_cm(p->sang_der), -50, 300, 1, 1, NULL);
	y += H + 8;
	rotulo(14, y, "&Primera línea:");
	numero_c(ID_SPRI, 150, y, 110, dec_cm(p->sang_primera), -100, 300, 1, 1, NULL);
	texto(x2, y + 4, 240, U, "(negativa: sangría francesa)");
	y += H + 14;
	rotulo(14, y, "Espacio (puntos)");
	y += U + 4;
	rotulo(14, y, "A&ntes:");
	numero_c(ID_ANTES, 150, y, 110, p->antes * 10 / TW_PT, 0, 1000, 60, 1, NULL);
	rotulo(x2, y, "Desp&ués:");
	numero_c(ID_DESP, x2 + 110, y, 110, p->despues * 10 / TW_PT, 0, 1000, 60, 1, NULL);
	y += H + 14;
	rotulo(14, y, "In&terlineado:");
	desple(ID_INTER, 150, y, 190, INTERS, it);
	numero_c(ID_INTERV, 360, y, 110, it >= 4 ? p->inter * 10 / TW_PT : 120, 10, 2000, 10, 1, NULL);
	y += H + 14;
	casilla(ID_SALTO, 14, y, 260, "&Salto de página antes", p->salto_antes);
	casilla(ID_JUNTOS, x2, y, 260, "&Con el siguiente", p->juntos);
	y += H + 16;
	Q.al = y + H + 12;
	aceptar_cancelar(NULL);
	abre(al_parrafo);
}

/* ===================================================================== */
/*  Tabulaciones                                                         */
/* ===================================================================== */
#define ID_TABS 50
#define ID_TPOS 51
#define ID_TTIPO 52
#define ID_TREL 53
#define B_FIJAR 54
#define B_BORRAR 55
#define B_BORRAR_TODAS 56
static fmt_parrafo tabs_p;
static char ops_tabs[MAX_TABS * 40];

static void abre_tabs(void);

static void al_tabs(int id)
{
	int k;

	if (id == B_FIJAR) {
		i32 p = tw_cm(ui_numero(c_(ID_TPOS)));
		int j;

		if (p > 0 && tabs_p.n_tabs < MAX_TABS) {
			for (k = 0; k < tabs_p.n_tabs && tabs_p.tabs[k].pos < p; k++)
				;
			if (!(k < tabs_p.n_tabs && tabs_p.tabs[k].pos == p)) {
				for (j = tabs_p.n_tabs; j > k; j--)
					tabs_p.tabs[j] = tabs_p.tabs[j - 1];
				tabs_p.n_tabs++;
			}
			tabs_p.tabs[k].pos = p;
			tabs_p.tabs[k].tipo = (u8)c_(ID_TTIPO)->elegida;
			tabs_p.tabs[k].relleno = (u8)" ._-"[c_(ID_TREL)->elegida];
			if (tabs_p.tabs[k].relleno == ' ')
				tabs_p.tabs[k].relleno = 0;
		}
		abre_tabs();
		return;
	}
	if (id == B_BORRAR) {
		k = c_(ID_TABS)->elegida;
		if (k >= 0 && k < tabs_p.n_tabs) {
			for (; k + 1 < tabs_p.n_tabs; k++)
				tabs_p.tabs[k] = tabs_p.tabs[k + 1];
			tabs_p.n_tabs--;
		}
		abre_tabs();
		return;
	}
	if (id == B_BORRAR_TODAS) {
		tabs_p.n_tabs = 0;
		abre_tabs();
		return;
	}
	if (id == B_ACEPTAR) {
		ed_formato_parrafo(&P.e, CP_TABS, &tabs_p);
		pl_cambio();
	}
}

static void abre_tabs(void)
{
	int k, y = 12;
	static const char *const tipos[] = { "izquierda", "centrada", "derecha", "decimal" };

	nuevo_cuadro("Tabulaciones", 520, 0);
	ops_tabs[0] = 0;
	for (k = 0; k < tabs_p.n_tabs; k++) {
		char t[48];

		fmt(t, sizeof t, "%s%d,%d cm (%s%s)", k ? "|" : "", dec_cm(tabs_p.tabs[k].pos) / 10, dec_cm(tabs_p.tabs[k].pos) % 10,
		    tipos[tabs_p.tabs[k].tipo & 3], tabs_p.tabs[k].relleno == '.' ? ", con puntos" : tabs_p.tabs[k].relleno ? ", con raya" : "");
		anade(ops_tabs, t, sizeof ops_tabs);
	}
	if (!tabs_p.n_tabs)
		copia(ops_tabs, "(ninguna: cada 1,25 cm)", sizeof ops_tabs);
	rotulo(14, y, "&Posición (cm):");
	numero_c(ID_TPOS, 170, y, 110, 25, 1, 500, 5, 1, NULL);
	y += H + 8;
	rotulo(14, y, "&Tipo:");
	desple(ID_TTIPO, 170, y, 160, "Izquierda|Centrada|Derecha|Decimal", 0);
	y += H + 8;
	rotulo(14, y, "&Relleno:");
	desple(ID_TREL, 170, y, 160, "Ninguno|Puntos (....)|Guiones (----)|Raya (____)", 0);
	y += H + 12;
	lista(ID_TABS, 14, y, 330, U * 5 + 8, ops_tabs, 0);
	boton(B_FIJAR, 360, 12, 146, "&Fijar", 0);
	boton(B_BORRAR, 360, y, 146, "&Borrar", 0);
	boton(B_BORRAR_TODAS, 360, y + H + 8, 146, "Borrar &todas", 0);
	y += U * 5 + 20;
	Q.al = y + H + 12;
	aceptar_cancelar(NULL);
	abre(al_tabs);
}

void dlg_tabulaciones(void)
{
	tabs_p = P.d->par[P.e.cur.p].p;
	abre_tabs();
}

/* ===================================================================== */
/*  Preparar página                                                      */
/* ===================================================================== */
#define ID_PAPEL 60
#define ID_VERT 61
#define ID_HORIZ 62
#define ID_MSUP 63
#define ID_MINF 64
#define ID_MIZQ 65
#define ID_MDER 66
#define ID_NUMI 67
static const char *const PAPELES = "A4 (21 x 29,7 cm)|A5 (14,8 x 21 cm)|A3 (29,7 x 42 cm)|Carta (21,6 x 27,9 cm)|Legal (21,6 x 35,6 cm)";
static const i32 papel_an[] = { 11906, 8391, 16838, 12240, 12240 };
static const i32 papel_al[] = { 16838, 11906, 23811, 15840, 20160 };

static void al_pagina(int id)
{
	documento *d = P.d;
	int k = c_(ID_PAPEL)->elegida, horiz = c_(ID_HORIZ)->marcado;

	if (id != B_ACEPTAR)
		return;
	ed_empieza(&P.e, 0, 0, PASO_PAGINA);
	d->pag_an = horiz ? papel_al[k] : papel_an[k];
	d->pag_al = horiz ? papel_an[k] : papel_al[k];
	d->marg_sup = tw_cm(ui_numero(c_(ID_MSUP)));
	d->marg_inf = tw_cm(ui_numero(c_(ID_MINF)));
	d->marg_izq = tw_cm(ui_numero(c_(ID_MIZQ)));
	d->marg_der = tw_cm(ui_numero(c_(ID_MDER)));
	d->num_inicio = (u32)(ui_numero(c_(ID_NUMI)) / 10);
	if (d->num_inicio < 1)
		d->num_inicio = 1;
	{
		u32 i;

		for (i = 0; i < d->n_par; i++)
			d->par[i].ancho_maq = -1;
		/* (las tablas, al ancho nuevo si ya no caben) */
		for (i = 0; i < d->n_tab; i++) {
			i32 tot = 0, a = doc_ancho_texto(d);
			u32 c;

			for (c = 0; c < d->tab[i].cols; c++)
				tot += d->tab[i].ancho[c];
			if (tot > a)
				for (c = 0; c < d->tab[i].cols; c++)
					d->tab[i].ancho[c] = (i32)((i64)d->tab[i].ancho[c] * a / tot);
		}
	}
	d->enc.ancho_maq = d->pie.ancho_maq = -1;
	ed_acaba(&P.e, 0);
	pl_cambio();
}

void dlg_pagina(void)
{
	documento *d = P.d;
	int k, papel = 0, horiz = d->pag_an > d->pag_al, y = 12;

	for (k = 0; k < 5; k++)
		if ((papel_an[k] == d->pag_an && papel_al[k] == d->pag_al) || (papel_al[k] == d->pag_an && papel_an[k] == d->pag_al))
			papel = k;
	nuevo_cuadro("Preparar página", 520, 0);
	rotulo(14, y, "&Papel:");
	desple(ID_PAPEL, 160, y, 300, PAPELES, papel);
	y += H + 10;
	opcion(ID_VERT, 160, y, 140, "&Vertical", 1, !horiz);
	opcion(ID_HORIZ, 310, y, 160, "&Horizontal", 1, horiz);
	y += H + 14;
	rotulo(14, y, "Márgenes (cm)");
	y += U + 6;
	rotulo(14, y, "&Superior:");
	numero_c(ID_MSUP, 120, y, 100, dec_cm(d->marg_sup), 0, 100, 1, 1, NULL);
	rotulo(260, y, "&Inferior:");
	numero_c(ID_MINF, 360, y, 100, dec_cm(d->marg_inf), 0, 100, 1, 1, NULL);
	y += H + 8;
	rotulo(14, y, "I&zquierdo:");
	numero_c(ID_MIZQ, 120, y, 100, dec_cm(d->marg_izq), 0, 100, 1, 1, NULL);
	rotulo(260, y, "&Derecho:");
	numero_c(ID_MDER, 360, y, 100, dec_cm(d->marg_der), 0, 100, 1, 1, NULL);
	y += H + 14;
	rotulo(14, y, "La primera página es la &número:");
	numero_c(ID_NUMI, 330, y, 100, (int)d->num_inicio * 10, 10, 99990, 10, 0, NULL);
	y += H + 16;
	Q.al = y + H + 12;
	aceptar_cancelar(NULL);
	abre(al_pagina);
}

/* ===================================================================== */
/*  Tablas                                                               */
/* ===================================================================== */
#define ID_FILAS 70
#define ID_COLS 71
#define ID_BORDES 72
#define ID_CABEZA 73
#define ID_FONDO 74
#define ID_ANCHO_COL 75
#define ID_ALIN_TABLA 76
static const char *const BORDES = "Sin bordes|Todos los bordes|Solo el contorno|El contorno y la cabecera";
static const char *const FONDOS = "Sin fondo|Gris claro|Azul claro|Verde claro|Amarillo claro|Rosa claro";
static const u32 fondos_v[] = { SIN_COLOR, 0xEEEEEE, 0xDCE9F8, 0xE2F0D9, 0xFFF4C2, 0xF8DDE6 };

static void al_tabla(int id)
{
	if (id != B_ACEPTAR)
		return;
	ed_tabla(&P.e, ui_numero(c_(ID_FILAS)) / 10, ui_numero(c_(ID_COLS)) / 10);
	if (P.d->par[P.e.cur.p].p.tabla) {
		tabla *t = &P.d->tab[P.d->par[P.e.cur.p].p.tabla - 1];

		t->bordes = (u8)c_(ID_BORDES)->elegida;
		t->cabecera = (u8)c_(ID_CABEZA)->marcado;
	}
	pl_cambio();
}

void dlg_tabla(void)
{
	int y = 12;

	nuevo_cuadro("Insertar tabla", 440, 0);
	rotulo(14, y, "Número de &columnas:");
	numero_c(ID_COLS, 260, y, 100, 30, 10, MAX_COLS * 10, 10, 0, NULL);
	y += H + 8;
	rotulo(14, y, "Número de &filas:");
	numero_c(ID_FILAS, 260, y, 100, 20, 10, 5000, 10, 0, NULL);
	y += H + 8;
	rotulo(14, y, "&Bordes:");
	desple(ID_BORDES, 140, y, 280, BORDES, BORDE_TODOS);
	y += H + 8;
	casilla(ID_CABEZA, 14, y, 400, "La primera fila, de &títulos (y se repite)", 0);
	y += H + 16;
	Q.al = y + H + 12;
	aceptar_cancelar(NULL);
	abre(al_tabla);
}

static void al_bordes(int id)
{
	u32 ini, n, k;

	if (id != B_ACEPTAR || !P.d->par[P.e.cur.p].p.tabla)
		return;
	ini = doc_tabla_ini(P.d, P.e.cur.p, &n);
	ed_empieza(&P.e, ini, ini + n, 3 | PASO_TABLAS);
	P.d->tab[P.d->par[P.e.cur.p].p.tabla - 1].bordes = (u8)c_(ID_BORDES)->elegida;
	{
		pos a, b;
		u32 col = fondos_v[c_(ID_FONDO)->elegida];

		/* el fondo: a las celdas elegidas (o a la del cursor) */
		ed_eleccion(&P.e, &a, &b);
		for (k = a.p; k <= b.p; k++)
			if (P.d->par[k].p.tabla)
				P.d->par[k].p.fondo = col;
	}
	for (k = ini; k < ini + n; k++)
		P.d->par[k].ancho_maq = -1;
	ed_acaba(&P.e, n);
	pl_cambio();
}

void dlg_bordes(void)
{
	const tabla *t;
	int y = 12;

	if (!P.d->par[P.e.cur.p].p.tabla) {
		dlg_aviso("Bordes y sombreado", "Ponga el cursor en una tabla.");
		return;
	}
	t = &P.d->tab[P.d->par[P.e.cur.p].p.tabla - 1];
	nuevo_cuadro("Bordes y sombreado", 460, 0);
	rotulo(14, y, "&Bordes:");
	desple(ID_BORDES, 160, y, 280, BORDES, t->bordes);
	y += H + 10;
	rotulo(14, y, "&Fondo de las celdas:");
	desple(ID_FONDO, 220, y, 220, FONDOS, indice_color(fondos_v, 6, P.d->par[P.e.cur.p].p.fondo));
	y += H + 16;
	Q.al = y + H + 12;
	aceptar_cancelar(NULL);
	abre(al_bordes);
}

static void al_eliminar(int id)
{
	if (id != B_ACEPTAR)
		return;
	if (c_(1001)->marcado)
		ed_quita_fila(&P.e);
	else if (c_(1002)->marcado)
		ed_quita_columna(&P.e);
	else
		ed_quita_tabla(&P.e);
	pl_cambio();
}

void dlg_eliminar_celdas(void)
{
	int y = 12;

	nuevo_cuadro("Eliminar celdas", 360, 0);
	opcion(1001, 14, y, 320, "Eliminar la &fila", 1, 1);
	y += H + 4;
	opcion(1002, 14, y, 320, "Eliminar la &columna", 1, 0);
	y += H + 4;
	opcion(1003, 14, y, 320, "Eliminar la &tabla entera", 1, 0);
	y += H + 16;
	Q.al = y + H + 12;
	aceptar_cancelar(NULL);
	abre(al_eliminar);
}

static void al_ancho(int id)
{
	u32 ini, n, k, c;

	if (id != B_ACEPTAR || !P.d->par[P.e.cur.p].p.tabla)
		return;
	ini = doc_tabla_ini(P.d, P.e.cur.p, &n);
	ed_empieza(&P.e, ini, ini + n, 3 | PASO_TABLAS);
	{
		tabla *t = &P.d->tab[P.d->par[P.e.cur.p].p.tabla - 1];

		c = P.d->par[P.e.cur.p].p.col;
		t->ancho[c] = tw_cm(ui_numero(c_(ID_ANCHO_COL)));
		if (t->ancho[c] < 300)
			t->ancho[c] = 300;
		t->alin = (u8)c_(ID_ALIN_TABLA)->elegida;
	}
	for (k = ini; k < ini + n; k++)
		P.d->par[k].ancho_maq = -1;
	ed_acaba(&P.e, n);
	pl_cambio();
}

void dlg_ancho_celda(void)
{
	const tabla *t;
	int y = 12;

	if (!P.d->par[P.e.cur.p].p.tabla)
		return;
	t = &P.d->tab[P.d->par[P.e.cur.p].p.tabla - 1];
	nuevo_cuadro("Alto y ancho de celda", 440, 0);
	rotulo(14, y, "&Ancho de la columna (cm):");
	numero_c(ID_ANCHO_COL, 280, y, 110, dec_cm(t->ancho[P.d->par[P.e.cur.p].p.col]), 5, 500, 1, 1, NULL);
	y += H + 10;
	rotulo(14, y, "La &tabla:");
	desple(ID_ALIN_TABLA, 160, y, 240, "A la izquierda|Centrada|A la derecha", t->alin);
	y += H + 8;
	texto(14, y, 400, U, "(el alto: el que pida lo que lleva dentro)");
	y += U + 16;
	Q.al = y + H + 12;
	aceptar_cancelar(NULL);
	abre(al_ancho);
}

/* ===================================================================== */
/*  Imagen                                                               */
/* ===================================================================== */
#define ID_IAN 80
#define ID_IAL 81
#define ID_IPROP 82
#define ID_IAJ 83

static void al_imagen(int id)
{
	int i = ed_imagen_elegida(&P.e);
	i32 an, al;

	if (id != B_ACEPTAR || i < 0)
		return;
	an = tw_cm(ui_numero(c_(ID_IAN)));
	al = tw_cm(ui_numero(c_(ID_IAL)));
	if (c_(ID_IPROP)->marcado) {
		const imagen *m = &P.d->img[i];

		/* lo que se ha cambiado manda */
		if (an != m->ancho)
			al = (i32)((i64)an * m->alto / (m->ancho ? m->ancho : 1));
		else if (al != m->alto)
			an = (i32)((i64)al * m->ancho / (m->alto ? m->alto : 1));
	}
	ed_tamano_imagen(&P.e, an, al);
	ed_ajuste_imagen(&P.e, c_(ID_IAJ)->elegida);
	pl_cambio();
}

void dlg_imagen(void)
{
	int i = ed_imagen_elegida(&P.e), y = 12;
	const imagen *m;
	static char info[120];

	if (i < 0) {
		dlg_aviso("Imagen", "Elija antes la imagen (un clic en ella).");
		return;
	}
	m = &P.d->img[i];
	fmt(info, sizeof info, "%s: %d x %d puntos", m->tipo == IMG_PNG ? "PNG" : m->tipo == IMG_JPG ? "JPEG" : m->tipo == IMG_BMP ? "BMP" : "GIF",
	    m->px_an, m->px_al);
	nuevo_cuadro("Imagen", 500, 0);
	texto(14, y, 460, U, info);
	y += U + 10;
	rotulo(14, y, "&Ancho (cm):");
	numero_c(ID_IAN, 140, y, 110, dec_cm(m->ancho), 2, 500, 5, 1, NULL);
	rotulo(270, y, "A&lto (cm):");
	numero_c(ID_IAL, 370, y, 110, dec_cm(m->alto), 2, 700, 5, 1, NULL);
	y += H + 8;
	casilla(ID_IPROP, 14, y, 300, "Mantener la &proporción", 1);
	y += H + 10;
	rotulo(14, y, "&Texto:");
	desple(ID_IAJ, 100, y, 380, "En la línea, como una letra|A la izquierda, con el texto alrededor|A la derecha, con el texto alrededor", m->ajuste);
	y += H + 16;
	Q.al = y + H + 12;
	aceptar_cancelar(NULL);
	abre(al_imagen);
}

/* ===================================================================== */
/*  Buscar y reemplazar                                                  */
/* ===================================================================== */
#define ID_BUSCA 90
#define ID_POR 91
#define ID_EXACTO 92
#define B_SIGUIENTE 93
#define B_ANTERIOR 94
#define B_REEMPLAZA 95
#define B_TODO 96
#define B_CERRAR 97
static int con_reemplazo;
static char busca_txt[200], por_txt[200];
static int busca_exacto;

static void abre_buscar(void);

static void al_buscar(int id)
{
	u32 n, m;
	u32 *t, *r;

	copia(busca_txt, c_(ID_BUSCA)->valor, sizeof busca_txt);
	if (con_reemplazo)
		copia(por_txt, c_(ID_POR)->valor, sizeof por_txt);
	busca_exacto = c_(ID_EXACTO)->marcado;
	if (id == B_CERRAR || id == B_CANCELAR || id < 0)
		return;
	t = u32_de_utf8(busca_txt, largo(busca_txt), &n);
	r = u32_de_utf8(por_txt, largo(por_txt), &m);
	if (t && r && n) {
		int bien = 1;

		if (id == B_SIGUIENTE || id == B_ACEPTAR || id == B_ANTERIOR)
			bien = ed_busca(&P.e, t, n, busca_exacto, id == B_ANTERIOR);
		else if (id == B_REEMPLAZA)
			bien = ed_reemplaza(&P.e, t, n, r, m, busca_exacto);
		else if (id == B_TODO) {
			int k = ed_reemplaza_todo(&P.e, t, n, r, m, busca_exacto);

			pl_aviso("Reemplazadas: %d", k);
			bien = 1;
		}
		if (!bien)
			pl_aviso("No se encuentra «%s»", busca_txt);
		else if (id != B_TODO)
			pl_aviso("");
		pl_cambio();
	}
	so_suelta(t);
	so_suelta(r);
	abre_buscar();
}

static void abre_buscar(void)
{
	int y = 12, an = 600, bx = an - 190;

	nuevo_cuadro(con_reemplazo ? "Reemplazar" : "Buscar", an, 0);
	rotulo(14, y, "&Buscar:");
	campo(ID_BUSCA, 140, y, bx - 160, busca_txt);
	boton(B_SIGUIENTE, bx, y, 176, "Buscar &siguiente", 1);
	y += H + 8;
	if (con_reemplazo) {
		rotulo(14, y, "&Reemplazar por:");
		campo(ID_POR, 140, y, bx - 160, por_txt);
	}
	boton(B_ANTERIOR, bx, y, 176, "Buscar &anterior", 0);
	y += H + 8;
	casilla(ID_EXACTO, 14, y, 340, "Mayúsculas y tildes &exactas", busca_exacto);
	if (con_reemplazo) {
		boton(B_REEMPLAZA, bx, y, 176, "&Reemplazar", 0);
		y += H + 8;
		boton(B_TODO, bx, y, 176, "Reemplazar &todo", 0);
	}
	y += H + 8;
	boton(B_CERRAR, bx, y, 176, "Cerrar", 0);
	y += H + 12;
	Q.al = y;
	abre(al_buscar);
}

void dlg_buscar(int reemplazar)
{
	pos a, b;

	con_reemplazo = reemplazar;
	/* lo elegido (si es corto y de un párrafo), para buscar */
	if (ed_eleccion(&P.e, &a, &b) && a.p == b.p && b.o - a.o < 60) {
		char *t = utf8_de_u32(P.d->par[a.p].t + a.o, b.o - a.o);

		if (t) {
			copia(busca_txt, t, sizeof busca_txt);
			so_suelta(t);
		}
	}
	abre_buscar();
}

/* ===================================================================== */
/*  Ir a, zoom, contar                                                   */
/* ===================================================================== */
#define ID_PAG 100

static void al_ir_a(int id)
{
	u32 n, k;

	if (id != B_ACEPTAR)
		return;
	n = (u32)(ui_numero(c_(ID_PAG)) / 10);
	if (n < P.d->num_inicio)
		n = P.d->num_inicio;
	n -= P.d->num_inicio;
	if (n >= P.d->n_pag)
		n = P.d->n_pag - 1;
	k = P.d->pag[n].t0;
	if (k < P.d->n_trz) {
		const trozo *t = &P.d->trz[k];
		pos c;

		c.p = t->par;
		c.o = t->fila_fin || t->lin0 >= P.d->par[t->par].n_lin ? 0 : P.d->par[t->par].lin[t->lin0].ini;
		ed_cursor(&P.e, c, 0);
		pl_cambio();
	}
}

void dlg_ir_a(void)
{
	nuevo_cuadro("Ir a", 380, 0);
	rotulo(14, 14, "Ir a la &página:");
	numero_c(ID_PAG, 200, 14, 110, (int)(vista_pagina_cursor(&P.vi) + P.d->num_inicio) * 10, 10, 999990, 10, 0, NULL);
	Q.al = 14 + H + 20 + H + 12;
	aceptar_cancelar("&Ir");
	abre(al_ir_a);
}

#define ID_ZOOM_N 110
static void al_zoom(int id)
{
	int k;

	if (id != B_ACEPTAR)
		return;
	for (k = 0; k < 8; k++)
		if (C[k].tipo == UC_OPCION && C[k].marcado) {
			static const int z[] = { 50, 75, 100, 150, 200 };

			if (C[k].id - 1100 < 5)
				vista_zoom(&P.vi, z[C[k].id - 1100]);
			else if (C[k].id == 1105)
				vista_zoom(&P.vi, vista_zoom_ancho(&P.vi));
			else if (C[k].id == 1106)
				vista_zoom(&P.vi, ui_numero(c_(ID_ZOOM_N)) / 10);
			break;
		}
	P.sucio = 1;
}

void dlg_zoom(void)
{
	static const char *const t[] = { "&50%", "&75%", "&100%", "15&0%", "&200%", "&Ancho de página", "&Otro:" };
	static const int z[] = { 50, 75, 100, 150, 200 };
	int k, y = 12, alguno = 0;

	nuevo_cuadro("Zoom", 360, 0);
	for (k = 0; k < 7; k++) {
		int m = k < 5 ? P.vi.zoom == z[k] : 0;

		if (m)
			alguno = 1;
		if (k == 6 && !alguno)
			m = 1;
		opcion(1100 + k, 14, y, 200, t[k], 1, m);
		y += H + 2;
	}
	numero_c(ID_ZOOM_N, 120, y - H - 2, 100, P.vi.zoom * 10, 250, 4000, 100, 0, "%");
	Q.al = y + 14 + H + 12;
	aceptar_cancelar(NULL);
	abre(al_zoom);
}

void dlg_contar(void)
{
	u32 pal, le, se, pa;
	static char t[400];

	ed_cuenta(P.d, &pal, &le, &se, &pa);
	maq_documento(P.d);
	fmt(t, sizeof t, "Páginas: %u\nPalabras: %u\nCaracteres (sin espacios): %u\nCaracteres (con espacios): %u\nPárrafos: %u",
	    P.d->n_pag, pal, se, le, pa);
	dlg_aviso("Contar palabras", t);
}

/* ===================================================================== */
/*  Encabezado y pie                                                     */
/* ===================================================================== */
#define ID_HAY_ENC 120
#define ID_ENC 121
#define ID_HAY_PIE 122
#define ID_PIE 123
#define ID_NO_PRIMERA 124
#define ID_ALIN_ENC 125
#define ID_ALIN_PIE 126

/* el texto del encabezado con {página} y {páginas} */
static void a_texto(const parrafo *p, char *d, u32 cabe)
{
	u32 k;

	d[0] = 0;
	for (k = 0; k < p->n; k++) {
		char u[8];

		if (p->t[k] == CAMPO_PAGINA)
			anade(d, "{página}", cabe);
		else if (p->t[k] == CAMPO_PAGINAS)
			anade(d, "{páginas}", cabe);
		else if (!es_objeto(p->t[k])) {
			u[utf8_pon(p->t[k], u)] = 0;
			anade(d, u, cabe);
		}
	}
}

static void de_texto(parrafo *p, const char *s, u16 f)
{
	u32 n, k = 0, i = 0;

	par_quita(p, 0, p->n);
	n = largo(s);
	while (i < n) {
		u32 c;

		if (empieza(s + i, "{página}")) {
			par_inserta_uno(p, k++, CAMPO_PAGINA, f);
			i += largo("{página}");
			continue;
		}
		if (empieza(s + i, "{páginas}")) {
			par_inserta_uno(p, k++, CAMPO_PAGINAS, f);
			i += largo("{páginas}");
			continue;
		}
		i += (u32)utf8_lee((const u8 *)s + i, n - i, &c);
		par_inserta_uno(p, k++, c, f);
	}
	p->ancho_maq = -1;
}

static void al_enc_pie(int id)
{
	documento *d = P.d;

	if (id != B_ACEPTAR)
		return;
	ed_empieza(&P.e, 0, 0, PASO_PAGINA);
	d->hay_enc = (u8)c_(ID_HAY_ENC)->marcado;
	d->hay_pie = (u8)c_(ID_HAY_PIE)->marcado;
	d->enc_no_primera = (u8)c_(ID_NO_PRIMERA)->marcado;
	de_texto(&d->enc, c_(ID_ENC)->valor, doc_fmt_estilo(d, EST_ENC));
	de_texto(&d->pie, c_(ID_PIE)->valor, doc_fmt_estilo(d, EST_PIE));
	d->enc.p.alin = (u8)c_(ID_ALIN_ENC)->elegida;
	d->pie.p.alin = (u8)c_(ID_ALIN_PIE)->elegida;
	if (d->enc.n)
		d->hay_enc = d->hay_enc || 1;
	ed_acaba(&P.e, 0);
	pl_cambio();
}

void dlg_enc_pie(void)
{
	documento *d = P.d;
	char t[260];
	int y = 12;

	nuevo_cuadro("Encabezado y pie de página", 600, 0);
	casilla(ID_HAY_ENC, 14, y, 200, "&Encabezado:", d->hay_enc);
	a_texto(&d->enc, t, sizeof t);
	campo(ID_ENC, 200, y, 380, t);
	y += H + 6;
	rotulo(200, y, "Alineado:");
	desple(ID_ALIN_ENC, 300, y, 180, "A la izquierda|Centrado|A la derecha", d->enc.p.alin < 3 ? d->enc.p.alin : 1);
	y += H + 12;
	casilla(ID_HAY_PIE, 14, y, 200, "&Pie de página:", d->hay_pie);
	a_texto(&d->pie, t, sizeof t);
	if (!d->pie.n && !d->hay_pie)
		copia(t, "Página {página} de {páginas}", sizeof t);
	campo(ID_PIE, 200, y, 380, t);
	y += H + 6;
	rotulo(200, y, "Alineado:");
	desple(ID_ALIN_PIE, 300, y, 180, "A la izquierda|Centrado|A la derecha", d->pie.p.alin < 3 ? d->pie.p.alin : 1);
	y += H + 12;
	casilla(ID_NO_PRIMERA, 14, y, 500, "&No en la primera página (una portada)", d->enc_no_primera);
	y += H + 6;
	texto(14, y, 560, U, "{página}: el número de la página; {páginas}: cuántas hay.");
	y += U + 16;
	Q.al = y + H + 12;
	aceptar_cancelar(NULL);
	abre(al_enc_pie);
}

/* ===================================================================== */
/*  Insertar: fecha, campo, símbolo, salto                               */
/* ===================================================================== */
#define ID_FECHAS 130
static char ops_fecha[600];
static char fechas[6][80];

static void al_fecha(int id)
{
	int k = c_(ID_FECHAS)->elegida;
	u32 n;
	u32 *t;

	if (id != B_ACEPTAR || k < 0 || k > 5)
		return;
	t = u32_de_utf8(fechas[k], largo(fechas[k]), &n);
	if (t) {
		ed_escribe(&P.e, t, n);
		so_suelta(t);
		pl_cambio();
	}
}

void dlg_fecha(void)
{
	static const char *const meses[] = { "enero", "febrero", "marzo", "abril", "mayo", "junio", "julio", "agosto", "septiembre", "octubre", "noviembre", "diciembre" };
	static const char *const dias[] = { "domingo", "lunes", "martes", "miércoles", "jueves", "viernes", "sábado" };
	int a, m, d, h, mi, s, k, ds;

	so_ahora(&a, &m, &d, &h, &mi, &s);
	if (m < 1 || m > 12)
		m = 1;
	{
		/* el día de la semana (Zeller) */
		int y = a, mm = m;

		if (mm < 3) {
			mm += 12;
			y--;
		}
		ds = (d + 13 * (mm + 1) / 5 + y + y / 4 - y / 100 + y / 400 + 6) % 7;
	}
	fmt(fechas[0], 80, "%02d/%02d/%d", d, m, a);
	fmt(fechas[1], 80, "%d de %s de %d", d, meses[m - 1], a);
	fmt(fechas[2], 80, "%s, %d de %s de %d", dias[ds], d, meses[m - 1], a);
	fmt(fechas[3], 80, "%d-%02d-%02d", a, m, d);
	fmt(fechas[4], 80, "%02d:%02d", h, mi);
	fmt(fechas[5], 80, "%02d/%02d/%d %02d:%02d", d, m, a, h, mi);
	ops_fecha[0] = 0;
	for (k = 0; k < 6; k++) {
		if (k)
			anade(ops_fecha, "|", sizeof ops_fecha);
		anade(ops_fecha, fechas[k], sizeof ops_fecha);
	}
	nuevo_cuadro("Fecha y hora", 440, 0);
	lista(ID_FECHAS, 14, 12, 410, U * 6 + 8, ops_fecha, 1);
	Q.al = 12 + U * 6 + 8 + 16 + H + 12;
	aceptar_cancelar("&Insertar");
	abre(al_fecha);
}

static void al_campo(int id)
{
	if (id != B_ACEPTAR)
		return;
	if (c_(1201)->marcado)
		ed_campo(&P.e, CAMPO_PAGINA);
	else if (c_(1202)->marcado)
		ed_campo(&P.e, CAMPO_PAGINAS);
	else {
		dlg_fecha();
		return;
	}
	pl_cambio();
}

void dlg_campo(void)
{
	int y = 12;

	nuevo_cuadro("Campo", 380, 0);
	opcion(1201, 14, y, 340, "Número de &página", 1, 1);
	y += H + 4;
	opcion(1202, 14, y, 340, "Número de pá&ginas", 1, 0);
	y += H + 4;
	opcion(1203, 14, y, 340, "&Fecha (como texto)", 1, 0);
	y += H + 16;
	Q.al = y + H + 12;
	aceptar_cancelar("&Insertar");
	abre(al_campo);
}

#define ID_SIMB 140
static const struct { u32 c; const char *n; } simbolos[] = {
	{ 0x20AC, "€ euro" }, { 0xA7, "§ sección" }, { 0xB6, "¶ párrafo" }, { 0xA9, "© copyright" }, { 0xAE, "® registrada" },
	{ 0x2122, "™ marca" }, { 0xB0, "° grados" }, { 0xB1, "± más o menos" }, { 0xD7, "× por" }, { 0xF7, "÷ entre" },
	{ 0xBD, "½ medio" }, { 0xBC, "¼ cuarto" }, { 0xBE, "¾ tres cuartos" }, { 0xAB, "« comillas" }, { 0xBB, "» comillas" },
	{ 0x201C, "“ comillas inglesas" }, { 0x201D, "” comillas inglesas" }, { 0x2013, "– raya corta" }, { 0x2014, "— raya" },
	{ 0x2026, "… puntos suspensivos" }, { 0x2022, "• viñeta" }, { 0x2192, "→ flecha" }, { 0x2190, "← flecha" },
	{ 0x2191, "↑ flecha" }, { 0x2193, "↓ flecha" }, { 0x2713, "✓ visto" }, { 0x2717, "✗ cruz" }, { 0x25A0, "■ cuadrado" },
	{ 0x25A1, "□ cuadrado vacío" }, { 0x2030, "‰ por mil" }, { 0xAA, "ª ordinal" }, { 0xBA, "º ordinal" }, { 0xB2, "² al cuadrado" },
	{ 0xB3, "³ al cubo" }, { 0xB5, "µ micro" }, { 0xA3, "£ libra" }, { 0xA5, "¥ yen" }, { 0x2020, "† cruz" },
};
static char ops_simb[2400];

static void al_simbolo(int id)
{
	int k = c_(ID_SIMB)->elegida;

	if (id != B_ACEPTAR || k < 0 || k >= (int)(sizeof simbolos / sizeof simbolos[0]))
		return;
	ed_escribe_letra(&P.e, simbolos[k].c);
	pl_cambio();
}

void dlg_simbolo(void)
{
	u32 k;

	ops_simb[0] = 0;
	for (k = 0; k < sizeof simbolos / sizeof simbolos[0]; k++) {
		if (k)
			anade(ops_simb, "|", sizeof ops_simb);
		anade(ops_simb, simbolos[k].n, sizeof ops_simb);
	}
	nuevo_cuadro("Símbolo", 420, 0);
	lista(ID_SIMB, 14, 12, 390, U * 10 + 8, ops_simb, 0);
	Q.al = 12 + U * 10 + 8 + 16 + H + 12;
	aceptar_cancelar("&Insertar");
	abre(al_simbolo);
}

static void al_salto(int id)
{
	if (id != B_ACEPTAR)
		return;
	if (c_(1301)->marcado)
		ed_salto_pagina(&P.e);
	else
		ed_salto_linea(&P.e);
	pl_cambio();
}

void dlg_salto(void)
{
	int y = 12;

	nuevo_cuadro("Salto", 380, 0);
	opcion(1301, 14, y, 340, "Salto de &página (Ctrl+R)", 1, 1);
	y += H + 4;
	opcion(1302, 14, y, 340, "Salto de &línea (Mayús+Intro)", 1, 0);
	y += H + 16;
	Q.al = y + H + 12;
	aceptar_cancelar(NULL);
	abre(al_salto);
}

/* ===================================================================== */
/*  Formato: mayúsculas, listas, estilos                                 */
/* ===================================================================== */
static void al_mayus(int id)
{
	int k;

	if (id != B_ACEPTAR)
		return;
	for (k = 0; k < 4; k++)
		if (c_(1400 + k)->marcado) {
			ed_mayusculas(&P.e, k);
			pl_cambio();
			return;
		}
}

void dlg_mayus(void)
{
	static const char *const t[] = { "&minúsculas", "&MAYÚSCULAS", "&Tipo oración", "&Cada Palabra Con Mayúscula" };
	int k, y = 12;

	nuevo_cuadro("Cambiar mayúsculas y minúsculas", 400, 0);
	for (k = 0; k < 4; k++) {
		opcion(1400 + k, 14, y, 360, t[k], 1, k == 2);
		y += H + 4;
	}
	Q.al = y + 12 + H + 12;
	aceptar_cancelar(NULL);
	abre(al_mayus);
}

#define ID_INICIO_LISTA 150
static void al_listas(int id)
{
	int k;

	if (id != B_ACEPTAR)
		return;
	for (k = 0; k < 5; k++)
		if (c_(1500 + k)->marcado) {
			fmt_parrafo p = P.d->par[P.e.cur.p].p;

			if (k == 0) {
				if (p.lista)
					ed_lista(&P.e, p.lista);
			} else {
				int tipo = k == 1 ? LISTA_VINETAS : k == 2 ? LISTA_NUMEROS : k == 3 ? LISTA_LETRAS : LISTA_ROMANOS;

				if (p.lista != tipo)
					ed_lista(&P.e, tipo);
				if (tipo != LISTA_VINETAS) {
					p = P.d->par[P.e.cur.p].p;
					p.inicio_lista = (u32)(ui_numero(c_(ID_INICIO_LISTA)) / 10);
					if (p.inicio_lista <= 1)
						p.inicio_lista = 0;
					ed_formato_parrafo(&P.e, CP_LISTA, &p);
				}
			}
			pl_cambio();
			return;
		}
}

void dlg_listas(void)
{
	static const char *const t[] = { "&Ninguna", "&Viñetas  •", "Números  &1. 2. 3.", "&Letras  a) b) c)", "&Romanos  i. ii. iii." };
	const fmt_parrafo *p = &P.d->par[P.e.cur.p].p;
	int k, y = 12, sel = p->lista == LISTA_VINETAS ? 1 : p->lista == LISTA_NUMEROS ? 2 : p->lista == LISTA_LETRAS ? 3 : p->lista == LISTA_ROMANOS ? 4 : 0;

	nuevo_cuadro("Numeración y viñetas", 420, 0);
	for (k = 0; k < 5; k++) {
		opcion(1500 + k, 14, y, 380, t[k], 1, k == sel);
		y += H + 4;
	}
	y += 6;
	rotulo(14, y, "&Empezar en el número:");
	numero_c(ID_INICIO_LISTA, 240, y, 100, p->inicio_lista ? (int)p->inicio_lista * 10 : 10, 10, 99990, 10, 0, NULL);
	y += H + 16;
	Q.al = y + H + 12;
	aceptar_cancelar(NULL);
	abre(al_listas);
}

#define ID_ESTILOS 160
#define ID_NOMBRE_EST 161
#define B_APLICAR 162
#define B_DEFINIR 163
#define B_NUEVO_EST 164
static char ops_est[2400];

static void abre_estilo(void);

static void al_estilo(int id)
{
	int k = c_(ID_ESTILOS)->elegida;
	documento *d = P.d;

	if (id == B_APLICAR || id == B_ACEPTAR) {
		ed_estilo(&P.e, k);
		pl_cambio();
		return;
	}
	if (id == B_DEFINIR && k >= 0 && (u32)k < d->n_est) {
		/* el estilo, como lo que hay en el cursor; y se aplica de nuevo a los suyos */
		u32 i;

		ed_empieza(&P.e, 0, d->n_par, 3);
		d->est[k].letra = ed_formato_actual(&P.e);
		{
			fmt_parrafo p = d->par[P.e.cur.p].p;

			p.tabla = 0;
			p.fila = p.col = 0;
			p.abarca_cols = p.abarca_filas = p.cubierta = 0;
			p.lista = 0;
			p.estilo = (u16)k;
			d->est[k].p = p;
		}
		for (i = 0; i < d->n_par; i++)
			if (d->par[i].p.estilo == k)
				d->par[i].ancho_maq = -1;
		ed_acaba(&P.e, d->n_par);
		pl_aviso("Estilo «%s» definido con lo del cursor", d->est[k].nombre);
		pl_cambio();
		return;
	}
	if (id == B_NUEVO_EST) {
		const char *n = c_(ID_NOMBRE_EST)->valor;

		if (n[0] && doc_estilo(d, n) < 0) {
			estilo e;

			memset(&e, 0, sizeof e);
			copia(e.nombre, n, sizeof e.nombre);
			e.letra = ed_formato_actual(&P.e);
			e.p = d->par[P.e.cur.p].p;
			e.p.tabla = 0;
			e.p.fila = e.p.col = 0;
			e.p.abarca_cols = e.p.abarca_filas = e.p.cubierta = 0;
			e.p.estilo = (u16)d->n_est;
			e.siguiente = (u16)d->n_est;
			doc_nuevo_estilo(d, &e);
			ed_estilo(&P.e, (int)d->n_est - 1);
			pl_aviso("Estilo nuevo: «%s»", n);
			pl_cambio();
		}
		abre_estilo();
		return;
	}
}

static void abre_estilo(void)
{
	u32 k;
	int y = 12;

	ops_est[0] = 0;
	for (k = 0; k < P.d->n_est; k++) {
		if (k)
			anade(ops_est, "|", sizeof ops_est);
		anade(ops_est, P.d->est[k].nombre, sizeof ops_est);
	}
	nuevo_cuadro("Estilo", 560, 0);
	lista(ID_ESTILOS, 14, y, 300, U * 10 + 8, ops_est, P.d->par[P.e.cur.p].p.estilo);
	boton(B_APLICAR, 330, y, 216, "&Aplicar", 1);
	boton(B_DEFINIR, 330, y + H + 8, 216, "&Definir con lo elegido", 0);
	rotulo(330, y + 2 * H + 24, "&Nombre del nuevo:");
	campo(ID_NOMBRE_EST, 330, y + 2 * H + 26 + U, 216, "");
	boton(B_NUEVO_EST, 330, y + 3 * H + 34 + U, 216, "Crear &nuevo", 0);
	y += U * 10 + 8 + 16;
	Q.al = y + H + 12;
	boton(B_CANCELAR, Q.an - 122, Q.al - H - 12, 110, "Cerrar", 0);
	abre(al_estilo);
}

void dlg_estilo(void)
{
	abre_estilo();
}

/* ===================================================================== */
/*  Resumen, idioma, opciones, acerca de, ayuda                          */
/* ===================================================================== */
static void al_resumen(int id)
{
	if (id != B_ACEPTAR)
		return;
	copia(P.d->titulo, c_(1601)->valor, sizeof P.d->titulo);
	copia(P.d->autor, c_(1602)->valor, sizeof P.d->autor);
	copia(P.d->asunto, c_(1603)->valor, sizeof P.d->asunto);
	P.d->cambiado = 1;
	pl_titulo();
}

void dlg_resumen(void)
{
	int y = 12;
	char nom[80];

	nuevo_cuadro("Resumen", 560, 0);
	rotulo(14, y, "&Título:");
	campo(1601, 120, y, 420, P.d->titulo);
	y += H + 8;
	rotulo(14, y, "&Autor:");
	if (!P.d->autor[0]) {
		nom[0] = 0;
		campo(1602, 120, y, 420, nom);
	} else
		campo(1602, 120, y, 420, P.d->autor);
	y += H + 8;
	rotulo(14, y, "A&sunto:");
	campo(1603, 120, y, 420, P.d->asunto);
	y += H + 16;
	Q.al = y + H + 12;
	aceptar_cancelar(NULL);
	abre(al_resumen);
}

void dlg_idioma(void)
{
	dlg_aviso("Idioma", pl_orto_hay() ? "Español (de España y de América).\nEl diccionario es el de LibreOffice (RLA-ES), el mismo de\nla oficina de Hispa-386: DICCIO.DAT. Sus palabras: \\DOCUMENT\\PROPIAS.TXT." :
		  "Español. No se encuentra el diccionario (DICCIO.DAT):\nsin él, Pluma no revisa la ortografía.");
}

#define ID_ORTO_ESC 170
#define ID_LETRA_UI 171
static void al_opciones(int id)
{
	static const int t[] = { 11, 13, 16, 20 };

	if (id != B_ACEPTAR)
		return;
	P.orto_al_escribir = c_(ID_ORTO_ESC)->marcado;
	P.vi.faltas = pl_orto_hay() && P.orto_al_escribir ? pl_orto_faltas : NULL;
	P.vi.con_faltas = P.orto_al_escribir;
	lz_ui_tamano(t[c_(ID_LETRA_UI)->elegida]);
	P.tam_ui = t[c_(ID_LETRA_UI)->elegida];
	P.sucio = 1;
}

void dlg_opciones(void)
{
	int y = 12, e = P.tam_ui >= 20 ? 3 : P.tam_ui >= 16 ? 2 : P.tam_ui && P.tam_ui <= 11 ? 0 : 1;

	nuevo_cuadro("Opciones", 520, 0);
	casilla(ID_ORTO_ESC, 14, y, 480, "Revisar la &ortografía mientras se escribe", P.orto_al_escribir);
	y += H + 10;
	rotulo(14, y, "&Letra de los menús y las barras:");
	desple(ID_LETRA_UI, 300, y, 200, "Pequeña (11)|Normal (13)|Grande (16)|Muy grande (20)", e);
	y += H + 10;
	texto(14, y, 480, 2 * U, "Las letras TrueType: las DejaVu de Pluma y las .TTF\nque ponga en \\DOCUMENT\\LETRAS (al abrir Pluma).");
	y += 2 * U + 16;
	Q.al = y + H + 12;
	aceptar_cancelar(NULL);
	abre(al_opciones);
}

void dlg_acerca(void)
{
	dlg_aviso("Acerca de Pluma", "Pluma " PLUMA_VERSION ", el procesador de textos de Hispa-Pentium (32 bits).\n"
		  "Abre y guarda .PLU (el de Hispa-386), .ODT, .DOCX, .RTF y .TXT;\nexporta a PDF con las letras dentro.\n"
		  "Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.\n"
		  "Lleva stb_truetype, stb_image (dominio público o MIT), miniz (MIT)\ny las letras DejaVu (licencia de Bitstream Vera).");
}

void dlg_ayuda(void)
{
	dlg_aviso("Ayuda de Pluma", "Ctrl+U nuevo · Ctrl+A abrir · Ctrl+G guardar · Ctrl+P imprimir (PDF)\n"
		  "Ctrl+Z deshacer · Ctrl+Y rehacer · Ctrl+X, Ctrl+C, Ctrl+V · Ctrl+E todo\n"
		  "Ctrl+N negrita · Ctrl+K cursiva · Ctrl+S subrayado · Ctrl+O/W letra más grande/pequeña\n"
		  "Ctrl+Q izquierda · Ctrl+T centro · Ctrl+D derecha · Ctrl+R salto de página\n"
		  "Ctrl+B buscar · Ctrl+L reemplazar · F5 ir a · F7 ortografía\n"
		  "Mayús+Intro: salto de línea · Tab en una tabla: la celda siguiente (al final, una fila)\n"
		  "Ctrl+rueda: zoom · Alt+letra: los menús · botón derecho: propuestas y más");
}

/* ===================================================================== */
/*  Ortografía (F7)                                                      */
/* ===================================================================== */
#define ID_SUGS 180
#define ID_CAMBIA 181
#define B_CAMBIAR 182
#define B_CAMBIAR_TODAS 183
#define B_OMITIR 184
#define B_OMITIR_TODAS 185
#define B_ANADIR 186
static char orto_rot[200];
static char orto_sug[8][64];
static char ops_sug[8 * 66];
static pos orto_a, orto_b;

/* la falta siguiente desde el cursor (0 si no hay más) */
static int falta_siguiente(void)
{
	documento *d = P.d;
	u32 i = P.e.cur.p, o = P.e.cur.o, vueltas;

	for (vueltas = 0; vueltas <= d->n_par; vueltas++) {
		const parrafo *p = &d->par[i];

		while (o < p->n) {
			u32 a, b;

			while (o < p->n && !es_letra(p->t[o]))
				o++;
			a = o;
			while (o < p->n && (es_letra(p->t[o]) || ((p->t[o] == '\'' || p->t[o] == 0x2019 || p->t[o] == '-') && o + 1 < p->n && es_letra(p->t[o + 1]))))
				o++;
			b = o;
			if (b > a && b - a < 64 && !pl_orto_bien(p->t + a, b - a)) {
				orto_a.p = orto_b.p = i;
				orto_a.o = a;
				orto_b.o = b;
				return 1;
			}
		}
		i++;
		o = 0;
		if (i >= d->n_par)
			return 0;	/* (hasta el final; F7 otra vez empieza arriba) */
	}
	return 0;
}

static void abre_orto(void);

static void al_orto(int id)
{
	const parrafo *p = &P.d->par[orto_a.p];
	u32 n;
	u32 *w;

	if (id == B_CANCELAR || id < 0)
		return;
	if (id == B_CAMBIAR || id == B_CAMBIAR_TODAS || id == B_ACEPTAR) {
		const char *v = c_(ID_CAMBIA)->valor;
		int k = c_(ID_SUGS)->elegida;

		if (!v[0] && k >= 0 && k < 8)
			v = orto_sug[k];
		w = u32_de_utf8(v, largo(v), &n);
		if (w && n) {
			if (id == B_CAMBIAR_TODAS) {
				u32 m = orto_b.o - orto_a.o;
				u32 *viejo = so_pide(m * 4 + 4);

				if (viejo) {
					memcpy(viejo, p->t + orto_a.o, m * 4);
					ed_reemplaza_todo(&P.e, viejo, m, w, n, 1);
					so_suelta(viejo);
				}
			} else {
				ed_elige(&P.e, orto_a, orto_b);
				ed_escribe(&P.e, w, n);
			}
			pl_cambio();
		}
		so_suelta(w);
	} else if (id == B_OMITIR) {
		P.e.cur = P.e.ancla = orto_b;
	} else if (id == B_OMITIR_TODAS || id == B_ANADIR) {
		if (id == B_ANADIR)
			pl_orto_anade(p->t + orto_a.o, orto_b.o - orto_a.o);
		else
			pl_orto_omite(p->t + orto_a.o, orto_b.o - orto_a.o);
		{
			u32 i;

			for (i = 0; i < P.d->n_par; i++)
				P.d->par[i].faltas_version = P.d->par[i].version - 1;
		}
		P.e.cur = P.e.ancla = orto_b;
	}
	abre_orto();
}

static void abre_orto(void)
{
	int n, k, y = 12;
	char *w;

	if (!falta_siguiente()) {
		pl_aviso("Ortografía: no hay más faltas hasta el final del documento.");
		dlg_aviso("Ortografía", "Revisión acabada: no hay más faltas\nhasta el final del documento.");
		return;
	}
	ed_elige(&P.e, orto_a, orto_b);
	P.vi.sigue = 1;
	w = utf8_de_u32(P.d->par[orto_a.p].t + orto_a.o, orto_b.o - orto_a.o);
	fmt(orto_rot, sizeof orto_rot, "No está en el diccionario: «%s»", w ? w : "");
	n = pl_orto_sugiere(P.d->par[orto_a.p].t + orto_a.o, orto_b.o - orto_a.o, orto_sug, 8);
	ops_sug[0] = 0;
	for (k = 0; k < n; k++) {
		if (k)
			anade(ops_sug, "|", sizeof ops_sug);
		anade(ops_sug, orto_sug[k], sizeof ops_sug);
	}
	if (!n)
		copia(ops_sug, "(sin propuestas)", sizeof ops_sug);
	nuevo_cuadro("Ortografía: español", 580, 0);
	texto(14, y, 550, U, orto_rot);
	y += U + 10;
	rotulo(14, y, "Cam&biar por:");
	campo(ID_CAMBIA, 150, y, 230, n ? orto_sug[0] : (w ? w : ""));
	boton(B_OMITIR, 396, y, 170, "&Omitir", 0);
	y += H + 8;
	rotulo(14, y, "&Propuestas:");
	lista(ID_SUGS, 150, y, 230, U * 6 + 8, ops_sug, 0);
	boton(B_OMITIR_TODAS, 396, y, 170, "Omitir &todas", 0);
	boton(B_CAMBIAR, 396, y + H + 8, 170, "&Cambiar", 1);
	boton(B_CAMBIAR_TODAS, 396, y + 2 * H + 16, 170, "Cambiar t&odas", 0);
	boton(B_ANADIR, 396, y + 3 * H + 24, 170, "&Añadir", 0);
	y += U * 6 + 8 + 16;
	Q.al = y + H + 12;
	boton(B_CANCELAR, Q.an - 182, Q.al - H - 12, 170, "Cerrar", 0);
	so_suelta(w);
	abre(al_orto);
	/* el foco, en Cambiar */
	for (k = 0; k < Q.n; k++)
		if (Q.c[k].id == B_CAMBIAR)
			Q.foco = k;
}

void dlg_ortografia(void)
{
	if (!pl_orto_hay()) {
		dlg_idioma();
		return;
	}
	/* desde el cursor (o desde lo elegido) */
	{
		pos a, b;

		if (ed_eleccion(&P.e, &a, &b))
			P.e.cur = P.e.ancla = a;
	}
	abre_orto();
}
