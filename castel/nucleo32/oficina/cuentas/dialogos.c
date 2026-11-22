/* dialogos.c - Cuentas 2: los cuadros de diálogo (abrir y guardar, con lo
 * de los .CSV; formato de celdas, con sus pestañas Número, Alineación,
 * Fuente, Bordes y Relleno; buscar y reemplazar; ir a; pegado especial;
 * insertar y eliminar; series; ordenar; el asistente para gráficos; los
 * nombres; los comentarios; el formato condicional; ancho y alto; zoom; las
 * hojas; preparar página; opciones; el asistente para funciones; el
 * autofiltro; la ayuda). Son de oficina/comun/ui.h y van dentro de la ventana.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "ventana.h"
#include "archivo.h"
#include "operar.h"
#include "deshacer.h"
#include "calculo.h"
#include "formula.h"
#include "numeros.h"
#include "celdas.h"
#include "grafico.h"
#include "pintor.h"
#include "letras.h"
#include "cuentas.h"

static ui_control K[48];
static int nK;
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
	C.cuadro = q;
	C.al_cerrar = f;
	C.sucio = 1;
	dice("cuentas: cuadro «%s»", q->titulo);
}

static void nuevo_cuadro(const char *titulo, int an, int al)
{
	U = lz_ui_alto();
	H = ui_alto_control();
	memset(&Q, 0, sizeof Q);
	memset(K, 0, sizeof K);
	nK = 0;
	Q.titulo = titulo;
	Q.an = an;
	Q.al = al;
}

static ui_control *ctl(int tipo, int id, int x, int y, int an, const char *rot)
{
	ui_control *c;

	if (nK >= 48)
		nK = 47;
	c = &K[nK++];
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
	Q.c = K;
	Q.n = nK;
	cuadro_abre(&Q, f);
}

static ui_control *c_(int id)
{
	return ui_control_id(&Q, id);
}

static int marcado(int id)
{
	ui_control *c = c_(id);

	return c ? c->marcado : 0;
}

static int elegida(int id)
{
	ui_control *c = c_(id);

	return c ? c->elegida : 0;
}

static const char *val_de(int id)
{
	ui_control *c = c_(id);

	return c ? c->valor : "";
}

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

static int cuantas_ops(const char *ops)
{
	int n = 1;

	for (; *ops; ops++)
		if (*ops == '|')
			n++;
	return n;
}

/* ===================================================================== */
/*  Avisos y preguntas                                                   */
/* ===================================================================== */
static char aviso_txt[900];

static void al_aviso(int id)
{
	(void)id;
}

void dlg_aviso(const char *titulo, const char *t)
{
	int lineas = 1, w = 0;
	const char *s;
	char l[400];
	const char *a;

	copia(aviso_txt, t, sizeof aviso_txt);
	for (s = aviso_txt; *s; s++)
		if (*s == '\n')
			lineas++;
	a = aviso_txt;
	while (a && *a) {
		const char *e = strchr(a, '\n');
		u32 k = e ? (u32)(e - a) : largo(a);

		copia(l, a, k + 1 < sizeof l ? k + 1 : sizeof l);
		if (lz_ui_ancho(l) > w)
			w = lz_ui_ancho(l);
		a = e ? e + 1 : NULL;
	}
	U = lz_ui_alto();
	H = ui_alto_control();
	nuevo_cuadro(titulo, w + 48 < 320 ? 320 : w + 48, lineas * U + H + 44);
	texto(20, 16, w, lineas * U, aviso_txt);
	boton(B_ACEPTAR, Q.an - 122, Q.al - H - 12, 110, "Aceptar", 1);
	abre(al_aviso);
	dice("cuentas: aviso «%s»: %s", titulo, t);
}

#define B_NO_GUARDAR 3
static char pregunta[300];

static void al_guardar_cambios(int id)
{
	int o = C.pendiente;

	if (id == B_ACEPTAR) {
		if (!C.ruta[0] || C.formato == FMT_CSV) {
			dlg_abrir(1);
			return;
		}
		if (cu_guarda(C.ruta, C.formato ? C.formato : FMT_HOJ))
			return;
		cu_orden(o);
	} else if (id == B_NO_GUARDAR) {
		C.lib->cambiado = 0;
		cu_orden(o);
	}
	C.pendiente = 0;
}

void dlg_guardar_cambios(int orden)
{
	C.pendiente = orden;
	fmt(pregunta, sizeof pregunta, "¿Guardar los cambios de %s?", C.ruta[0] ? sin_carpeta(C.ruta) : "Libro1");
	nuevo_cuadro("Cuentas", lz_ui_ancho(pregunta) + 60 < 420 ? 420 : lz_ui_ancho(pregunta) + 60, U * 2 + H + 40);
	texto(20, 16, Q.an - 40, U, pregunta);
	boton(B_ACEPTAR, 12, Q.al - H - 12, 120, "&Guardar", 1);
	boton(B_NO_GUARDAR, 144, Q.al - H - 12, 120, "&No guardar", 0);
	boton(B_CANCELAR, Q.an - 122, Q.al - H - 12, 110, "Cancelar", 0)->cancela = 1;
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

static int modo_abrir;		/* 0 abrir, 1 guardar como, 2 exportar a PDF */
static char ent_nombre[MAX_ENT][64];
static u8 ent_carpeta[MAX_ENT];
static int n_ent;
static char ops_lista[MAX_ENT * 66];
static char rot_carpeta[700];
static const char *const TIPOS_GUARDAR = "Libro de Cuentas (.HOJ)|Libro de Excel (.XLSX)|Hoja de cálculo OpenDocument (.ODS)|Texto separado por comas (.CSV)";
static const int fmt_tipo[] = { FMT_HOJ, FMT_XLSX, FMT_ODS, FMT_CSV };
static const char *const TIPOS_ABRIR = "Hojas de cálculo (HOJ, XLSX, XLS, ODS, CSV)|Texto separado (CSV, TXT): con opciones|Todos los archivos";
static int tipo_elegido;
static char nombre_puesto[260];
static char ruta_csv[700];

static int vale_ext(const char *n, int carpeta)
{
	char e[8];

	if (carpeta || (modo_abrir == 0 && tipo_elegido == 2))
		return 1;
	extension(n, e);
	if (modo_abrir == 2)
		return iguales(e, "PDF");
	if (modo_abrir == 1)
		return iguales(e, arch_extension(fmt_tipo[tipo_elegido])) || (fmt_tipo[tipo_elegido] == FMT_XLSX && iguales(e, "XLS"));
	if (tipo_elegido == 1)
		return iguales(e, "CSV") || iguales(e, "TXT") || iguales(e, "TSV");
	return iguales(e, "HOJ") || iguales(e, "XLSX") || iguales(e, "XLS") || iguales(e, "XLSM") || iguales(e, "ODS") || iguales(e, "CSV");
}

static void lee_carpeta(void)
{
	void *c = so_abre_carpeta(C.carpeta[0] ? C.carpeta : "\\");
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
	fmt(rot_carpeta, sizeof rot_carpeta, "Carpeta: %s", C.carpeta[0] ? C.carpeta : "\\");
}

static void abre_abrir(void);

static void sube_carpeta(void)
{
	u32 i, k = 0;

	for (i = 0; C.carpeta[i]; i++)
		if (C.carpeta[i] == '\\' || C.carpeta[i] == '/')
			k = i;
	if (k > 0 && !(k == 2 && C.carpeta[1] == ':'))
		C.carpeta[k] = 0;
	else if (C.carpeta[1] == ':')
		C.carpeta[2] = 0;
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
		C.pendiente = 0;
		return;
	}
	nom = n ? n->valor : "";
	if (!nom[0] && l && l->elegida >= 0 && l->elegida < n_ent) {
		if (ent_carpeta[l->elegida]) {
			anade(C.carpeta, "\\", sizeof C.carpeta);
			anade(C.carpeta, ent_nombre[l->elegida], sizeof C.carpeta);
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
		fmt(ruta, sizeof ruta, "%s\\%s", C.carpeta, nom);
	if (iguales(nom, "..")) {
		sube_carpeta();
		nombre_puesto[0] = 0;
		abre_abrir();
		return;
	}
	if (so_existe(ruta) == 2) {
		copia(C.carpeta, ruta, sizeof C.carpeta);
		nombre_puesto[0] = 0;
		abre_abrir();
		return;
	}
	switch (modo_abrir) {
	case 0:
		if (tipo_elegido == 1 || (arch_formato_ext(ruta) == FMT_CSV && tipo_elegido == 1)) {
			dlg_csv(0, ruta);
			return;
		}
		cu_abre(ruta);
		break;
	case 1:
	case 2: {
		int f = modo_abrir == 2 ? FMT_PDF : fmt_tipo[tipo_elegido];
		char e[8];

		/* (la extensión que se escribe manda: «libro.xlsx» es un .XLSX) */
		if (modo_abrir == 1 && arch_formato_ext(ruta) && arch_formato_ext(ruta) != FMT_PDF)
			f = arch_formato_ext(ruta);
		extension(ruta, e);
		if (!e[0] || (arch_formato_ext(ruta) != f && !(f == FMT_XLSX && iguales(e, "XLS")))) {
			anade(ruta, ".", sizeof ruta);
			anade(ruta, modo_abrir == 2 ? "pdf" : arch_extension(f), sizeof ruta);
		}
		if (f == FMT_CSV) {
			dlg_csv(1, ruta);
			return;
		}
		if (cu_guarda(ruta, f) == 0 && C.pendiente && modo_abrir == 1) {
			int o = C.pendiente;

			cu_orden(o);
		}
		break;
	}
	}
}

static void abre_abrir(void)
{
	static const char *const titulos[] = { "Abrir", "Guardar como", "Exportar a PDF" };
	int an = 580, al;
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
	if (modo_abrir != 2) {
		rotulo(14, 12 + U + 6 + lh + 20 + H, "&Tipo:");
		desple(ID_TIPO, 110, 12 + U + 6 + lh + 20 + H, an - 124, modo_abrir == 1 ? TIPOS_GUARDAR : TIPOS_ABRIR, tipo_elegido);
	}
	aceptar_cancelar(modo_abrir == 1 ? "&Guardar" : modo_abrir == 2 ? "&Exportar" : "&Abrir");
	abre(al_abrir);
	{
		int i;

		for (i = 0; i < Q.n; i++)
			if (Q.c[i].id == (modo_abrir ? ID_NOMBRE : ID_LISTA))
				Q.foco = i;
		if (modo_abrir) {
			ui_control *n = c_(ID_NOMBRE);

			n->ancla = 0;
			n->cursor = (int)utf8_cuenta(n->valor, largo(n->valor));
		}
	}
}

void dlg_abrir(int modo)
{
	modo_abrir = modo;
	nombre_puesto[0] = 0;
	tipo_elegido = 0;
	if (modo == 1) {
		int k;

		for (k = 0; k < 4; k++)
			if (fmt_tipo[k] == C.formato)
				tipo_elegido = k;
		copia(nombre_puesto, C.ruta[0] ? sin_carpeta(C.ruta) : "LIBRO1.HOJ", sizeof nombre_puesto);
	}
	if (modo == 2) {
		char *pt;

		copia(nombre_puesto, C.ruta[0] ? sin_carpeta(C.ruta) : "LIBRO1", sizeof nombre_puesto);
		pt = strchr(nombre_puesto, '.');
		if (pt)
			*pt = 0;
		anade(nombre_puesto, ".PDF", sizeof nombre_puesto);
	}
	abre_abrir();
}

/* ---- las opciones de los .CSV ---- */
#define ID_SEP 20
#define ID_COD 21
#define ID_FORM 22
static int csv_guardar;
static const char *const SEPS = "Punto y coma (;)|Coma (,)|Tabulador|El que más aparezca";
static const char *const CODS = "UTF-8|UTF-8 con marca (BOM)|Windows-1252 (Europa occidental)|437 (MS-DOS)|La que sea (UTF-8 o 1252)";

static void al_csv(int id)
{
	static const char sep[4] = { ';', ',', '\t', 0 };

	if (id != B_ACEPTAR)
		return;
	arch_csv.sep = sep[elegida(ID_SEP) & 3];
	if (csv_guardar && !arch_csv.sep)
		arch_csv.sep = ';';
	switch (elegida(ID_COD)) {
	case 0: arch_csv.cod = CSV_UTF8; arch_csv.bom = 0; break;
	case 1: arch_csv.cod = CSV_UTF8; arch_csv.bom = 1; break;
	case 2: arch_csv.cod = CSV_1252; break;
	case 3: arch_csv.cod = CSV_437; break;
	default: arch_csv.cod = CSV_AUTO;
	}
	arch_csv.formulas = marcado(ID_FORM);
	if (csv_guardar)
		cu_guarda(ruta_csv, FMT_CSV);
	else
		cu_abre(ruta_csv);
}

void dlg_csv(int guardar, const char *ruta)
{
	int y = 14;

	csv_guardar = guardar;
	copia(ruta_csv, ruta, sizeof ruta_csv);
	nuevo_cuadro(guardar ? "Guardar como texto (.CSV)" : "Abrir un texto separado (.CSV)", 460, 0);
	rotulo(14, y + 4, "&Separador:");
	desple(ID_SEP, 140, y, 300, SEPS, guardar ? 0 : 3);
	y += H + 10;
	rotulo(14, y + 4, "&Codificación:");
	desple(ID_COD, 140, y, 300, CODS, guardar ? 0 : 4);
	y += H + 10;
	if (guardar) {
		casilla(ID_FORM, 14, y, 420, "Las &fórmulas, con su «=» (si no, lo que dan)", 0);
		y += H + 6;
		texto(14, y, 430, U * 2, "Un .CSV guarda solo lo escrito en las celdas de la hoja\nque se ve: los formatos y las demás hojas se pierden.");
		y += U * 2 + 6;
	}
	Q.al = y + H + 24;
	aceptar_cancelar(guardar ? "&Guardar" : "&Abrir");
	abre(al_csv);
}

/* ===================================================================== */
/*  Formato de celdas                                                     */
/* ===================================================================== */
enum { P_NUMERO, P_ALINEACION, P_FUENTE, P_BORDES, P_RELLENO };
#define ID_PEST 100		/* + la pestaña */
#define ID_CAT 30
#define ID_DEC 31
#define ID_MIL 32
#define ID_ROJO 33
#define ID_CODIGO 34
#define ID_FECHA 35
#define ID_MUESTRA 36
#define ID_AH 40
#define ID_AV 41
#define ID_AJUSTAR 42
#define ID_SANGRIA 43
#define ID_COMB 44
#define ID_FAM 50
#define ID_EST 51
#define ID_TAM 52
#define ID_SUB 53
#define ID_TACH 54
#define ID_COLOR 55
#define ID_BIZQ 60
#define ID_BARR 61
#define ID_BDER 62
#define ID_BAB 63
#define ID_BCOLOR 64
#define ID_BTODOS 65
#define ID_BCONT 66
#define ID_BNADA 67
#define ID_FONDO 70
#define ID_TRAMA 71
#define ID_CTRAMA 72

static estilo est_trabajo, est_antes;
static int pest_actual, bordes_rapidos;
static char fam_ops[2000];
static const char *const CATS = "General|Número|Moneda|Porcentaje|Fecha|Hora|Científica|Texto|Personalizada";
static const char *const FECHAS = "dd/mm/aaaa|d/m/aa|d \"de\" mmmm \"de\" aaaa|dddd, d \"de\" mmmm \"de\" aaaa|dd-mmm-aa|mmmm \"de\" aaaa|hh:mm|hh:mm:ss|h:mm AM/PM|dd/mm/aaaa hh:mm|[h]:mm";
static const char *const AHS = "General|Izquierda|Centro|Derecha|Rellenar|Justificar|Centrar en la selección";
static const char *const AVS = "Abajo|Centro|Arriba";
static const char *const ESTILOS_LETRA = "Normal|Negrita|Cursiva|Negrita cursiva";
static const char *const BRS = "Ninguno|Fino|Medio|Grueso|Doble|Puntos|Guiones";
static const char *const COLS = "Automático|Negro|Rojo|Granate|Naranja|Amarillo oscuro|Verde|Verde azulado|Azul|Azul marino|Morado|Gris|Blanco";
static const u32 COLS_V[] = { CU_AUTO, 0x000000, 0xC62828, 0x7B1E1E, 0xE65100, 0xB38B00, 0x2E7D32, 0x00796B, 0x1565C0, 0x1A237E, 0x6A1B9A, 0x757575, 0xFFFFFF };
static const char *const FONDOS = "Sin color|Blanco|Amarillo|Amarillo claro|Verde claro|Turquesa|Azul claro|Rosa|Naranja claro|Gris claro|Gris|Negro";
static const u32 FONDOS_V[] = { 0xFFFFFF, 0xFFFFFF, 0xFFF176, 0xFFF9C4, 0xC5E1A5, 0x80DEEA, 0xBBDEFB, 0xF8BBD0, 0xFFCC80, 0xE0E0E0, 0x9E9E9E, 0x000000 };
static char tramas_ops[600];

static int indice_de(const u32 *t, int n, u32 c)
{
	int i;

	for (i = 0; i < n; i++)
		if (t[i] == c)
			return i;
	return 0;
}

/* el código de lo que dicen los controles de la pestaña Número */
static void codigo_numero(char *d, u32 cabe)
{
	int cat = elegida(ID_CAT), dec = ui_numero(c_(ID_DEC)) / 10, mil = marcado(ID_MIL), rojo = marcado(ID_ROJO), k;
	char b[100];

	if (dec < 0)
		dec = 0;
	if (dec > 15)
		dec = 15;
	copia(b, mil || cat == 2 ? "#,##0" : "0", sizeof b);
	if (dec) {
		anade(b, ".", sizeof b);
		for (k = 0; k < dec; k++)
			anade(b, "0", sizeof b);
	}
	switch (cat) {
	case 0: copia(d, "General", cabe); return;
	case 1:
		if (rojo)
			fmt(d, cabe, "%s;[Red]-%s", b, b);
		else
			copia(d, b, cabe);
		return;
	case 2:
		if (rojo)
			fmt(d, cabe, "%s %s;[Red]-%s %s", b, C.lib->moneda, b, C.lib->moneda);
		else
			fmt(d, cabe, "%s %s", b, C.lib->moneda);
		if (!iguales(C.lib->moneda, "€")) {
			/* (con comillas, si no es el €) */
			if (rojo)
				fmt(d, cabe, "%s \"%s\";[Red]-%s \"%s\"", b, C.lib->moneda, b, C.lib->moneda);
			else
				fmt(d, cabe, "%s \"%s\"", b, C.lib->moneda);
		}
		return;
	case 3: fmt(d, cabe, "%s%%", mil ? b : b); return;
	case 4:
	case 5: {
		char t[100];

		opcion_n(FECHAS, elegida(ID_FECHA), t, sizeof t);
		fmt_de_local(t, &C.lib->loc, d, cabe);
		return;
	}
	case 6:
		copia(d, "0", cabe);
		if (dec) {
			anade(d, ".", cabe);
			for (k = 0; k < dec; k++)
				anade(d, "0", cabe);
		}
		anade(d, "E+00", cabe);
		return;
	case 7: copia(d, "@", cabe); return;
	default:
		fmt_de_local(val_de(ID_CODIGO), &C.lib->loc, d, cabe);
	}
}

static void muestra_numero(void *u, lienzo *l, const ui_control *c, int x, int y)
{
	hoja *h = cu_hoja();
	const valor *v = lib_valor(h, h->cur_f, h->cur_c);
	char cod[200], t[300];
	u32 color;

	(void)u;
	codigo_numero(cod, sizeof cod);
	if (v->t == V_NUM)
		num_formatea(v->n, cod, &C.lib->loc, t, sizeof t, &color);
	else if (v->t == V_TXT)
		{ num_formatea_texto(v->s, cod, t, sizeof t); color = CU_AUTO; }
	else
		num_formatea(1234.5, cod, &C.lib->loc, t, sizeof t, &color);
	lz_ui(l, x + 8, y + (c->al - lz_ui_alto()) / 2 + lz_ui_base(), t, color == CU_AUTO ? UI_TINTA : color);
}

static void muestra_fuente(void *u, lienzo *l, const ui_control *c, int x, int y)
{
	estilo e = est_trabajo;
	char fam[64];
	int cara, falsas, tam;

	(void)u;
	opcion_n(fam_ops, elegida(ID_FAM), fam, sizeof fam);
	copia(e.familia, fam, sizeof e.familia);
	e.negrita = elegida(ID_EST) & 1;
	e.cursiva = (elegida(ID_EST) >> 1) & 1;
	cara = cel_cara(&e, &falsas);
	tam = ui_numero(c_(ID_TAM)) * 4 * 96 / 72 / 10;
	{
		u32 col = COLS_V[elegida(ID_COLOR) < 13 ? elegida(ID_COLOR) : 0];
		int w = lz_texto(l, cara, tam, falsas, x + 10, y + c->al * 2 / 3, "AaBbÑñ 123,45 €", col == CU_AUTO ? 0 : col);

		if (marcado(ID_SUB))
			lz_rect(l, x + 10, y + c->al * 2 / 3 + 2, w, 1, col == CU_AUTO ? 0 : col);
		if (marcado(ID_TACH))
			lz_rect(l, x + 10, y + c->al * 2 / 3 - tam / 12, w, 1, col == CU_AUTO ? 0 : col);
	}
}

static void muestra_relleno(void *u, lienzo *l, const ui_control *c, int x, int y)
{
	pintor p;
	int tr = elegida(ID_TRAMA), f = elegida(ID_FONDO);
	u32 fondo = FONDOS_V[f < 12 ? f : 0], col = COLS_V[elegida(ID_CTRAMA) < 13 ? elegida(ID_CTRAMA) : 1];

	(void)u;
	pt_de_lienzo(&p, l);
	if (f > 0 && tr == TR_NADA)
		tr = TR_SOLIDO;
	pt_trama(&p, x + 8, y + 8, c->an - 16, c->al - 16, tr, fondo, col == CU_AUTO ? 0 : col);
}

static void guarda_pestana(void)
{
	char cod[200];

	switch (pest_actual) {
	case P_NUMERO:
		codigo_numero(cod, sizeof cod);
		est_trabajo.numfmt = lib_fmt_id(C.lib, cod);
		break;
	case P_ALINEACION:
		est_trabajo.alin_h = (u8)elegida(ID_AH);
		est_trabajo.alin_v = (u8)elegida(ID_AV);
		est_trabajo.ajustar = (u8)marcado(ID_AJUSTAR);
		est_trabajo.sangria = (u8)(ui_numero(c_(ID_SANGRIA)) / 10);
		break;
	case P_FUENTE: {
		char fam[64];

		opcion_n(fam_ops, elegida(ID_FAM), fam, sizeof fam);
		copia(est_trabajo.familia, fam, sizeof est_trabajo.familia);
		est_trabajo.negrita = (u8)(elegida(ID_EST) & 1);
		est_trabajo.cursiva = (u8)((elegida(ID_EST) >> 1) & 1);
		est_trabajo.tam = (u16)(ui_numero(c_(ID_TAM)) / 5);
		est_trabajo.subrayado = (u8)marcado(ID_SUB);
		est_trabajo.tachado = (u8)marcado(ID_TACH);
		est_trabajo.color = COLS_V[elegida(ID_COLOR) < 13 ? elegida(ID_COLOR) : 0];
		break;
	}
	case P_BORDES: {
		u32 col = COLS_V[elegida(ID_BCOLOR) < 13 ? elegida(ID_BCOLOR) : 1];
		int k;

		est_trabajo.borde[BO_IZQ] = (u8)elegida(ID_BIZQ);
		est_trabajo.borde[BO_ARRIBA] = (u8)elegida(ID_BARR);
		est_trabajo.borde[BO_DER] = (u8)elegida(ID_BDER);
		est_trabajo.borde[BO_ABAJO] = (u8)elegida(ID_BAB);
		for (k = 0; k < 4; k++)
			est_trabajo.color_borde[k] = col == CU_AUTO ? 0 : col;
		break;
	}
	case P_RELLENO: {
		int f = elegida(ID_FONDO), tr = elegida(ID_TRAMA);

		est_trabajo.fondo = FONDOS_V[f < 12 ? f : 0];
		est_trabajo.trama = (u8)(f == 0 && tr == 0 ? TR_NADA : tr == 0 ? TR_SOLIDO : tr);
		est_trabajo.color_trama = COLS_V[elegida(ID_CTRAMA) < 13 ? elegida(ID_CTRAMA) : 1];
		if (est_trabajo.color_trama == CU_AUTO)
			est_trabajo.color_trama = 0;
		break;
	}
	}
}

/* lo cambiado (de est_antes a est_trabajo), a lo elegido: solo lo que cambia */
static void cb_cambios(libro *l, estilo *e, const void *u)
{
	const estilo *a = &est_antes, *b = &est_trabajo;
	int k;

	(void)l;
	(void)u;
	if (a->numfmt != b->numfmt)
		e->numfmt = b->numfmt;
	if (a->alin_h != b->alin_h)
		e->alin_h = b->alin_h;
	if (a->alin_v != b->alin_v)
		e->alin_v = b->alin_v;
	if (a->ajustar != b->ajustar)
		e->ajustar = b->ajustar;
	if (a->sangria != b->sangria)
		e->sangria = b->sangria;
	if (!iguales(a->familia, b->familia))
		copia(e->familia, b->familia, sizeof e->familia);
	if (a->negrita != b->negrita)
		e->negrita = b->negrita;
	if (a->cursiva != b->cursiva)
		e->cursiva = b->cursiva;
	if (a->tam != b->tam)
		e->tam = b->tam;
	if (a->subrayado != b->subrayado)
		e->subrayado = b->subrayado;
	if (a->tachado != b->tachado)
		e->tachado = b->tachado;
	if (a->color != b->color)
		e->color = b->color;
	for (k = 0; k < 4; k++)
		if (a->borde[k] != b->borde[k] || a->color_borde[k] != b->color_borde[k]) {
			e->borde[k] = b->borde[k];
			e->color_borde[k] = b->color_borde[k];
		}
	if (a->trama != b->trama || a->fondo != b->fondo || a->color_trama != b->color_trama) {
		e->trama = b->trama;
		e->fondo = b->fondo;
		e->color_trama = b->color_trama;
	}
}

static void abre_formato(void);

static void al_formato(int id)
{
	if (id < 0 || id == B_CANCELAR)
		return;
	guarda_pestana();
	if (id >= ID_PEST && id < ID_PEST + 5) {
		pest_actual = id - ID_PEST;
		abre_formato();
		return;
	}
	if (id == ID_BTODOS || id == ID_BCONT || id == ID_BNADA) {
		bordes_rapidos = id;
		abre_formato();
		return;
	}
	if (id == B_ACEPTAR) {
		hoja *h = cu_hoja();
		rango s = cu_seleccion();
		int combinar = pest_actual == P_ALINEACION ? marcado(ID_COMB) : -1;

		dh_empieza(C.lib, "Formato de celdas");
		op_formato(C.lib, h, s, cb_cambios, NULL, "Formato de celdas");
		if (bordes_rapidos == ID_BCONT || bordes_rapidos == ID_BTODOS) {
			/* (el contorno: solo lo de fuera) */
			u32 f, c;
			int fino = BR_FINO;

			for (f = s.f0; f <= s.f1 && (u64)(s.f1 - s.f0 + 1) * (s.c1 - s.c0 + 1) < 2000000; f++)
				for (c = s.c0; c <= s.c1; c++) {
					estilo e = *lib_estilo(C.lib, hj_estilo(h, f, c));

					if (bordes_rapidos == ID_BTODOS || f == s.f0)
						e.borde[BO_ARRIBA] = (u8)fino;
					if (bordes_rapidos == ID_BTODOS || f == s.f1)
						e.borde[BO_ABAJO] = (u8)fino;
					if (bordes_rapidos == ID_BTODOS || c == s.c0)
						e.borde[BO_IZQ] = (u8)fino;
					if (bordes_rapidos == ID_BTODOS || c == s.c1)
						e.borde[BO_DER] = (u8)fino;
					dh_celda(C.lib, h, f, c);
					lib_pon_estilo(C.lib, h, f, c, lib_estilo_id(C.lib, &e));
				}
		}
		if (combinar == 1 && hj_combinada(h, s.f0, s.c0) < 0)
			op_combina(C.lib, h, s, 0);
		else if (combinar == 0 && hj_combinada(h, s.f0, s.c0) >= 0)
			op_descombina(C.lib, h, s);
		dh_acaba(C.lib);
		cu_cambio();
	}
}

static void abre_formato(void)
{
	static const char *const nombres[5] = { "Número", "Alineación", "Fuente", "Bordes", "Relleno" };
	static char rot[5][32];
	int an = 600, y, k, x = 14;
	hoja *h = cu_hoja();

	nuevo_cuadro("Formato de celdas", an, 0);
	/* las pestañas: botones; la elegida, con «»» delante (no solo hundida) */
	for (k = 0; k < 5; k++) {
		fmt(rot[k], sizeof rot[k], "%s%s", k == pest_actual ? "\xc2\xbb " : "", nombres[k]);
		boton(ID_PEST + k, x, 10, 108, rot[k], 0);
		x += 114;
	}
	y = 10 + H + 16;
	switch (pest_actual) {
	case P_NUMERO: {
		const char *cod = lib_fmt(C.lib, est_trabajo.numfmt);
		int cat = fmt_categoria(cod), cats[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8 }, dec = fmt_decimales(cod);
		char local[200];

		(void)cats;
		fmt_a_local(cod, &C.lib->loc, local, sizeof local);
		rotulo(14, y, "&Categoría:");
		lista(ID_CAT, 14, y + U + 4, 170, U * 9 + 8, CATS, cat);
		rotulo(200, y + 4, "&Decimales:");
		numero_c(ID_DEC, 330, y, 90, dec * 10, 0, 150, 10, 0, NULL);
		casilla(ID_MIL, 200, y + H + 8, 380, "Con separador de &millares (1.234)", busca(cod, "#,##") != NULL);
		casilla(ID_ROJO, 200, y + 2 * (H + 8), 380, "Los negativos, en &rojo (y con su signo)", busca(cod, "[Red]") != NULL);
		rotulo(200, y + 3 * (H + 8) + 4, "&Fecha u hora:");
		desple(ID_FECHA, 330, y + 3 * (H + 8), 250, FECHAS, 0);
		rotulo(200, y + 4 * (H + 8) + 4, "Có&digo:");
		campo(ID_CODIGO, 330, y + 4 * (H + 8), 250, local);
		rotulo(200, y + 5 * (H + 8) + 4, "Muestra:");
		ctl(UC_MUESTRA, ID_MUESTRA, 330, y + 5 * (H + 8), 250, NULL)->al = H + 8;
		Q.pinta_extra = muestra_numero;
		y += U * 9 + 8 + U + 14;
		texto(14, y, an - 28, U * 2, "«Personalizada» usa el código: # y 0 las cifras, «,» los decimales, «.» los millares,\n% por cien, «aaaa», «mm», «dd» la fecha, «hh:mm» la hora; con «;», los negativos aparte.");
		y += U * 2 + 10;
		break;
	}
	case P_ALINEACION:
		rotulo(14, y + 4, "&Horizontal:");
		desple(ID_AH, 160, y, 260, AHS, est_trabajo.alin_h);
		y += H + 10;
		rotulo(14, y + 4, "&Vertical:");
		desple(ID_AV, 160, y, 260, AVS, est_trabajo.alin_v);
		y += H + 10;
		rotulo(14, y + 4, "&Sangría:");
		numero_c(ID_SANGRIA, 160, y, 100, est_trabajo.sangria * 10, 0, 150, 10, 0, NULL);
		y += H + 10;
		casilla(ID_AJUSTAR, 14, y, 400, "&Ajustar el texto (en varias líneas)", est_trabajo.ajustar);
		y += H + 8;
		casilla(ID_COMB, 14, y, 400, "&Combinar las celdas", hj_combinada(h, h->cur_f, h->cur_c) >= 0);
		y += H + 14;
		break;
	case P_FUENTE: {
		const char *f[LETRAS_MAX];
		int n = letras_familias(f, LETRAS_MAX), i, e = 0;

		fam_ops[0] = 0;
		for (i = 0; i < n; i++) {
			if (i)
				anade(fam_ops, "|", sizeof fam_ops);
			anade(fam_ops, f[i], sizeof fam_ops);
			if (iguales(f[i], est_trabajo.familia))
				e = i;
		}
		rotulo(14, y, "&Fuente:");
		lista(ID_FAM, 14, y + U + 4, 230, U * 7 + 8, fam_ops, e);
		rotulo(260, y + 4, "&Estilo:");
		desple(ID_EST, 360, y, 220, ESTILOS_LETRA, est_trabajo.negrita | est_trabajo.cursiva << 1);
		rotulo(260, y + H + 14, "&Tamaño:");
		numero_c(ID_TAM, 360, y + H + 10, 120, est_trabajo.tam * 5, 40, 4090, 10, 1, "pt");
		casilla(ID_SUB, 260, y + 2 * (H + 10), 300, "&Subrayado", est_trabajo.subrayado);
		casilla(ID_TACH, 260, y + 3 * (H + 10) - 4, 300, "Tac&hado", est_trabajo.tachado);
		rotulo(260, y + 4 * (H + 10) - 4, "&Color:");
		desple(ID_COLOR, 360, y + 4 * (H + 10) - 8, 220, COLS, indice_de(COLS_V, 13, est_trabajo.color));
		y += U * 7 + 8 + U + 12;
		ctl(UC_MUESTRA, ID_MUESTRA, 14, y, an - 28, NULL)->al = 52;
		Q.pinta_extra = muestra_fuente;
		y += 62;
		break;
	}
	case P_BORDES: {
		int bz = bordes_rapidos;

		rotulo(14, y + 4, "&Izquierdo:");
		desple(ID_BIZQ, 130, y, 170, BRS, bz == ID_BNADA ? 0 : est_trabajo.borde[BO_IZQ]);
		rotulo(310, y + 4, "&Arriba:");
		desple(ID_BARR, 410, y, 170, BRS, bz == ID_BNADA ? 0 : est_trabajo.borde[BO_ARRIBA]);
		y += H + 10;
		rotulo(14, y + 4, "&Derecho:");
		desple(ID_BDER, 130, y, 170, BRS, bz == ID_BNADA ? 0 : est_trabajo.borde[BO_DER]);
		rotulo(310, y + 4, "A&bajo:");
		desple(ID_BAB, 410, y, 170, BRS, bz == ID_BNADA ? 0 : est_trabajo.borde[BO_ABAJO]);
		y += H + 10;
		rotulo(14, y + 4, "C&olor:");
		desple(ID_BCOLOR, 130, y, 170, COLS, indice_de(COLS_V, 13, est_trabajo.color_borde[0]) ? indice_de(COLS_V, 13, est_trabajo.color_borde[0]) : 1);
		y += H + 14;
		rotulo(14, y + 4, "De una vez:");
		boton(ID_BTODOS, 130, y, 140, bz == ID_BTODOS ? "\xc2\xbb Todos" : "&Todos", 0);
		boton(ID_BCONT, 280, y, 140, bz == ID_BCONT ? "\xc2\xbb Contorno" : "Co&ntorno", 0);
		boton(ID_BNADA, 430, y, 140, "&Ninguno", 0);
		y += H + 14;
		break;
	}
	case P_RELLENO: {
		int k2;

		tramas_ops[0] = 0;
		for (k2 = 0; k2 < TR_N; k2++) {
			if (k2)
				anade(tramas_ops, "|", sizeof tramas_ops);
			anade(tramas_ops, NOMBRES_TRAMA[k2], sizeof tramas_ops);
		}
		rotulo(14, y + 4, "&Color de fondo:");
		desple(ID_FONDO, 170, y, 230, FONDOS, est_trabajo.trama == TR_NADA ? 0 : indice_de(FONDOS_V, 12, est_trabajo.fondo) ? indice_de(FONDOS_V, 12, est_trabajo.fondo) : 1);
		y += H + 10;
		rotulo(14, y + 4, "&Trama:");
		desple(ID_TRAMA, 170, y, 230, tramas_ops, est_trabajo.trama == TR_SOLIDO ? 0 : est_trabajo.trama);
		y += H + 10;
		rotulo(14, y + 4, "Color de la t&rama:");
		desple(ID_CTRAMA, 170, y, 230, COLS, indice_de(COLS_V, 13, est_trabajo.color_trama) ? indice_de(COLS_V, 13, est_trabajo.color_trama) : 1);
		ctl(UC_MUESTRA, ID_MUESTRA, 420, y - 2 * (H + 10), 160, NULL)->al = 3 * H + 20;
		Q.pinta_extra = muestra_relleno;
		y += H + 14;
		texto(14, y, an - 28, U, "La trama se ve también sin color (en gris o impreso en blanco y negro).");
		y += U + 10;
		break;
	}
	}
	Q.al = y + H + 22;
	aceptar_cancelar(NULL);
	abre(al_formato);
	/* (el foco, en la primera de la pestaña) */
	{
		int i;

		for (i = 5; i < Q.n; i++)
			if (Q.c[i].tipo != UC_ROTULO && Q.c[i].tipo != UC_TEXTO && Q.c[i].tipo != UC_MUESTRA) {
				Q.foco = i;
				break;
			}
	}
}

void dlg_formato_celdas(int pestana)
{
	hoja *h = cu_hoja();

	est_antes = *lib_estilo(C.lib, hj_estilo(h, h->cur_f, h->cur_c));
	est_trabajo = est_antes;
	pest_actual = pestana;
	bordes_rapidos = 0;
	abre_formato();
}

/* ===================================================================== */
/*  Buscar y reemplazar                                                   */
/* ===================================================================== */
#define ID_QUE 80
#define ID_POR 81
#define ID_CAJA 82
#define ID_ENTERA 83
#define ID_FORMULAS 84
#define ID_COLS 85
#define B_SIGUIENTE 86
#define B_REEMPLAZA 87
#define B_TODAS 88
static int con_reemplazo;

static void abre_buscar(void);

static void al_buscar(int id)
{
	hoja *h = cu_hoja();
	int opc;
	u32 f = h->cur_f, c = h->cur_c;

	copia(C.buscado, val_de(ID_QUE), sizeof C.buscado);
	if (con_reemplazo)
		copia(C.reemplazo, val_de(ID_POR), sizeof C.reemplazo);
	opc = (marcado(ID_CAJA) ? BU_CAJA : 0) | (marcado(ID_ENTERA) ? BU_ENTERA : 0) | (marcado(ID_FORMULAS) || con_reemplazo ? BU_FORMULAS : 0) |
	      (marcado(ID_COLS) ? BU_COLUMNAS : 0);
	C.bus_opc = opc;
	if (id == B_CANCELAR || id < 0)
		return;
	if (id == B_TODAS) {
		int n = op_reemplaza(C.lib, h, C.buscado, C.reemplazo, opc, 1, 0, 0);

		cu_aviso("%d reemplazadas", n);
		cu_cambio();
		return;
	}
	if (id == B_REEMPLAZA) {
		if (op_reemplaza(C.lib, h, C.buscado, C.reemplazo, opc, 0, f, c))
			cu_cambio();
	}
	if (op_busca(C.lib, h, C.buscado, opc, &f, &c)) {
		cu_elige(f, c, 0);
		cu_aviso("Encontrado en %c%u", 'A' + (int)(c % 26), f + 1);
	} else {
		cu_aviso("No se encuentra «%s»", C.buscado);
		dlg_aviso("Buscar", "No se encuentra lo que se busca.");
		return;
	}
	abre_buscar();
}

static void abre_buscar(void)
{
	int an = 520, y = 14;

	nuevo_cuadro(con_reemplazo ? "Reemplazar" : "Buscar", an, 0);
	rotulo(14, y + 4, "&Buscar:");
	campo(ID_QUE, 150, y, an - 164, C.buscado);
	y += H + 10;
	if (con_reemplazo) {
		rotulo(14, y + 4, "Reemplazar &con:");
		campo(ID_POR, 150, y, an - 164, C.reemplazo);
		y += H + 10;
	}
	casilla(ID_CAJA, 14, y, 240, "Coincidir &mayúsculas", C.bus_opc & BU_CAJA);
	casilla(ID_ENTERA, 260, y, 240, "La celda &entera", C.bus_opc & BU_ENTERA);
	y += H + 6;
	casilla(ID_FORMULAS, 14, y, 240, "En las &fórmulas", (C.bus_opc & BU_FORMULAS) || con_reemplazo);
	casilla(ID_COLS, 260, y, 240, "Por c&olumnas", C.bus_opc & BU_COLUMNAS);
	y += H + 14;
	boton(B_SIGUIENTE, 14, y, 150, "Buscar &siguiente", 1);
	if (con_reemplazo) {
		boton(B_REEMPLAZA, 172, y, 120, "&Reemplazar", 0);
		boton(B_TODAS, 300, y, 120, "&Todas", 0);
	}
	boton(B_CANCELAR, an - 110, y, 96, "Cerrar", 0)->cancela = 1;
	Q.al = y + H + 14;
	abre(al_buscar);
}

void dlg_buscar(int reemplazar)
{
	con_reemplazo = reemplazar;
	abre_buscar();
}

/* ===================================================================== */
/*  Ir a                                                                  */
/* ===================================================================== */
#define ID_REF 90
static char nombres_ops[3000];

static int ve_a(const char *t)
{
	rango r;
	const char *fin;
	char u[200];
	const char *p = t;
	u32 k = 0;
	hoja *h = cu_hoja();
	const char *ex = strchr(t, '!');

	if (ex) {
		const char *a = t, *b = ex;

		if (*a == '\'' && b > a + 1 && b[-1] == '\'') {
			a++;
			b--;
		}
		h = lib_hoja_nombre(C.lib, a, (u32)(b - a));
		if (!h)
			return 1;
		p = ex + 1;
	}
	for (; *p && k + 1 < sizeof u; p++)
		if (*p != '$')
			u[k++] = *p;
	u[k] = 0;
	if (cu_lee_rango(u, &r, &fin) && !*fin) {
		C.lib->activa = (u32)lib_hoja_indice(C.lib, h->id);
		cu_elige(r.f0, r.c0, 0);
		if (r.f0 != r.f1 || r.c0 != r.c1)
			cu_sel_rango(r);
		return 0;
	}
	{
		int n = lib_nombre(C.lib, u, largo(u));

		if (n >= 0 && C.lib->nom[n].f && C.lib->nom[n].f->n == 1 && (C.lib->nom[n].f->t[0].t == T_REF || C.lib->nom[n].f->t[0].t == T_RANGO)) {
			const cu_tok *tk = &C.lib->nom[n].f->t[0];
			int i = lib_hoja_indice(C.lib, tk->h);

			if (i >= 0)
				C.lib->activa = (u32)i;
			cu_elige((u32)tk->u.r.f0, tk->u.r.c0, 0);
			cu_sel_rango(cu_rango((u32)tk->u.r.f0, tk->u.r.c0, (u32)tk->u.r.f1, tk->u.r.c1));
			return 0;
		}
	}
	return 1;
}

static void al_ir_a(int id)
{
	const char *t = val_de(ID_REF);
	char x[100];

	if (id != B_ACEPTAR)
		return;
	if (!t[0] && C.lib->n_nom) {
		opcion_n(nombres_ops, elegida(ID_LISTA), x, sizeof x);
		t = x;
	}
	if (ve_a(t))
		dlg_aviso("Ir a", "Esa referencia no es válida (A1, B2:D9, Hoja2!C5 o un nombre).");
}

void dlg_ir_a(void)
{
	u32 i;
	int y = 14, an = 420;

	nombres_ops[0] = 0;
	for (i = 0; i < C.lib->n_nom; i++) {
		if (i)
			anade(nombres_ops, "|", sizeof nombres_ops);
		anade(nombres_ops, C.lib->nom[i].nombre, sizeof nombres_ops);
	}
	nuevo_cuadro("Ir a", an, 0);
	if (C.lib->n_nom) {
		rotulo(14, y, "&Nombres:");
		lista(ID_LISTA, 14, y + U + 4, an - 28, U * 6 + 8, nombres_ops, 0);
		y += U * 7 + 18;
	}
	rotulo(14, y + 4, "&Referencia:");
	campo(ID_REF, 140, y, an - 154, "");
	y += H + 14;
	Q.al = y + H + 14;
	aceptar_cancelar(NULL);
	abre(al_ir_a);
	{
		int k;

		for (k = 0; k < Q.n; k++)
			if (Q.c[k].id == ID_REF)
				Q.foco = k;
	}
}

/* ===================================================================== */
/*  Pegado especial, insertar y eliminar, series                          */
/* ===================================================================== */
#define ID_PG 110		/* + el modo */
#define ID_TRANSP 120

static void al_pegado(int id)
{
	int m, modo = PG_TODO;

	if (id != B_ACEPTAR)
		return;
	for (m = 0; m < 5; m++)
		if (marcado(ID_PG + m))
			modo = m;
	{
		hoja *h = cu_hoja();
		char e[300];
		rango r;

		if (op_pega(C.lib, h, h->cur_f, h->cur_c, modo, marcado(ID_TRANSP), &r, e, sizeof e))
			dlg_aviso("No se puede pegar", e);
		else
			cu_sel_rango(r);
	}
	cu_cambio();
}

void dlg_pegado_especial(void)
{
	static const char *const ops[5] = { "&Todo", "&Fórmulas", "&Valores (lo que dan las fórmulas)", "Fo&rmatos", "&Comentarios" };
	int y = 14, m;

	nuevo_cuadro("Pegado especial", 420, 0);
	rotulo(14, y, "Pegar:");
	y += U + 6;
	for (m = 0; m < 5; m++) {
		opcion(ID_PG + m, 24, y, 380, ops[m], 1, m == 0);
		y += H + 2;
	}
	y += 8;
	casilla(ID_TRANSP, 14, y, 380, "Tra&nsponer (las filas, en columnas)", 0);
	y += H + 14;
	Q.al = y + H + 14;
	aceptar_cancelar("&Pegar");
	abre(al_pegado);
}

#define ID_FILAS 130
#define ID_COLUMNAS 131
static int insertando;

static void al_eliminar(int id)
{
	hoja *h = cu_hoja();
	rango s = cu_seleccion();

	if (id != B_ACEPTAR)
		return;
	if (marcado(ID_FILAS)) {
		if (insertando)
			op_inserta(C.lib, h, 0, s.f0, s.f1 - s.f0 + 1);
		else
			op_elimina(C.lib, h, 0, s.f0, s.f1 - s.f0 + 1);
	} else {
		if (insertando)
			op_inserta(C.lib, h, 1, s.c0, (u32)s.c1 - s.c0 + 1);
		else
			op_elimina(C.lib, h, 1, s.c0, (u32)s.c1 - s.c0 + 1);
	}
	cu_cambio();
}

void dlg_eliminar(int insertar)
{
	rango s = cu_seleccion();
	int y = 14, cols = s.f0 == 0 && s.f1 == CU_FILAS - 1;

	insertando = insertar;
	nuevo_cuadro(insertar ? "Insertar" : "Eliminar", 380, 0);
	opcion(ID_FILAS, 14, y, 340, "&Filas enteras", 1, !cols);
	y += H + 4;
	opcion(ID_COLUMNAS, 14, y, 340, "&Columnas enteras", 1, cols);
	y += H + 14;
	Q.al = y + H + 14;
	aceptar_cancelar(NULL);
	abre(al_eliminar);
}

#define ID_SE_COLS 140
#define ID_SE_FILAS 141
#define ID_SE_TIPO 142
#define ID_SE_PASO 143
#define ID_SE_LIMITE 144
static const char *const TIPOS_SERIE = "Lineal|Geométrica|Días|Días laborables|Meses|Años";

static void al_series(int id)
{
	double paso = 1, lim = 0;
	int tipo = elegida(ID_SE_TIPO), con = 0, t;

	if (id != B_ACEPTAR)
		return;
	if (!num_lee_entrada(val_de(ID_SE_PASO), &C.lib->loc, "", &paso, &t))
		paso = 1;
	if (val_de(ID_SE_LIMITE)[0] && num_lee_entrada(val_de(ID_SE_LIMITE), &C.lib->loc, "", &lim, &t))
		con = 1;
	op_series(C.lib, cu_hoja(), cu_seleccion(), marcado(ID_SE_COLS), tipo, paso, con, lim);
	cu_cambio();
}

void dlg_series(void)
{
	rango s = cu_seleccion();
	int y = 14;

	nuevo_cuadro("Series", 420, 0);
	rotulo(14, y, "Series en:");
	opcion(ID_SE_FILAS, 120, y, 120, "&Filas", 1, s.f0 == s.f1);
	opcion(ID_SE_COLS, 250, y, 140, "&Columnas", 1, s.f0 != s.f1);
	y += H + 10;
	rotulo(14, y + 4, "&Tipo:");
	desple(ID_SE_TIPO, 150, y, 240, TIPOS_SERIE, 0);
	y += H + 10;
	rotulo(14, y + 4, "&Incremento:");
	campo(ID_SE_PASO, 150, y, 120, "1");
	y += H + 10;
	rotulo(14, y + 4, "&Límite:");
	campo(ID_SE_LIMITE, 150, y, 120, "");
	y += H + 14;
	Q.al = y + H + 14;
	aceptar_cancelar(NULL);
	abre(al_series);
}

/* ===================================================================== */
/*  Ordenar                                                               */
/* ===================================================================== */
#define ID_OR_COL 150		/* + 0, 1, 2 */
#define ID_OR_DESC 153		/* + 0, 1, 2 */
#define ID_OR_TIT 156
static char cols_ops[256 * 40];
static rango zona_orden;

static void opciones_columnas(int titulos)
{
	hoja *h = cu_hoja();
	u32 c;

	copia(cols_ops, "(ninguna)", sizeof cols_ops);
	for (c = zona_orden.c0; c <= zona_orden.c1; c++) {
		char t[64], l[8];
		const valor *v = lib_valor(h, zona_orden.f0, c);

		cu_col_texto(c, l);
		if (titulos && v->t == V_TXT && v->s)
			fmt(t, sizeof t, "|%s (%s)", v->s, l);
		else
			fmt(t, sizeof t, "|Columna %s", l);
		anade(cols_ops, t, sizeof cols_ops);
	}
}

static void al_ordenar(int id)
{
	int col[3], desc[3], k;

	if (id != B_ACEPTAR)
		return;
	for (k = 0; k < 3; k++) {
		int e = elegida(ID_OR_COL + k);

		col[k] = e > 0 ? (int)zona_orden.c0 + e - 1 : -1;
		desc[k] = elegida(ID_OR_DESC + k);
	}
	if (col[0] < 0) {
		dlg_aviso("Ordenar", "Elija al menos una columna para ordenar.");
		return;
	}
	op_ordena(C.lib, cu_hoja(), zona_orden, col, desc, marcado(ID_OR_TIT));
	cu_cambio();
}

void dlg_ordenar(void)
{
	hoja *h = cu_hoja();
	rango s = cu_seleccion();
	int y = 14, k, tit;

	if (s.f0 == s.f1 && s.c0 == s.c1) {
		/* la región de datos */
		u32 f1 = s.f0, c1 = s.c0, f0 = s.f0, c0 = s.c0;

		while (f0 > 0 && lib_valor(h, f0 - 1, c0)->t != V_VACIO)
			f0--;
		while (f1 + 1 < CU_FILAS && lib_valor(h, f1 + 1, c0)->t != V_VACIO)
			f1++;
		while (c0 > 0 && lib_valor(h, f0, c0 - 1)->t != V_VACIO)
			c0--;
		while (c1 + 1 < CU_COLS && lib_valor(h, f0, c1 + 1)->t != V_VACIO)
			c1++;
		s = cu_rango(f0, c0, f1, c1);
	}
	if (s.c1 - s.c0 > 200)
		s.c1 = (u16)(s.c0 + 200);
	zona_orden = s;
	tit = s.f1 > s.f0 && lib_valor(h, s.f0, h->cur_c)->t == V_TXT && lib_valor(h, s.f0 + 1, h->cur_c)->t != V_TXT;
	opciones_columnas(1);
	nuevo_cuadro("Ordenar", 480, 0);
	for (k = 0; k < 3; k++) {
		static const char *const r[3] = { "&Ordenar por:", "&Luego por:", "Y &después por:" };

		rotulo(14, y + 4, r[k]);
		desple(ID_OR_COL + k, 150, y, 190, cols_ops, k == 0 ? (int)(h->cur_c - s.c0) + 1 : 0);
		desple(ID_OR_DESC + k, 350, y, 116, "Ascendente|Descendente", 0);
		y += H + 10;
	}
	casilla(ID_OR_TIT, 14, y, 440, "La primera fila son los &títulos (no se ordena)", tit);
	y += H + 14;
	Q.al = y + H + 14;
	aceptar_cancelar("&Ordenar");
	abre(al_ordenar);
}

/* ===================================================================== */
/*  El asistente para gráficos                                            */
/* ===================================================================== */
#define ID_GR_TIPO 160
#define ID_GR_RANGO 161
#define ID_GR_FILAS 162
#define ID_GR_ROT 163
#define ID_GR_TIT 164
#define ID_GR_EX 165
#define ID_GR_EY 166
#define ID_GR_LEY 167
#define ID_GR_REJ 168
#define ID_GR_MUESTRA 169
static grafico gr_trabajo;
static int gr_indice;
static const char *const TIPOS_GR = "Columnas|Columnas apiladas|Barras|Barras apiladas|Líneas|Sectores|Dispersión (X Y)";

static void lee_gr(grafico *g)
{
	int t = elegida(ID_GR_TIPO);
	rango r;
	const char *fin;
	char u[100];
	const char *p = val_de(ID_GR_RANGO);
	u32 k = 0;

	g->tipo = (u8)(t <= 1 ? GR_COLUMNAS : t <= 3 ? GR_BARRAS : t == 4 ? GR_LINEAS : t == 5 ? GR_SECTORES : GR_DISPERSION);
	g->apiladas = (u8)(t == 1 || t == 3);
	{
		const char *ex = strchr(p, '!');

		if (ex) {
			hoja *h = lib_hoja_nombre(C.lib, p[0] == '\'' ? p + 1 : p, (u32)(ex - p) - (p[0] == '\'' ? 2 : 0));

			if (h)
				g->hoja = h->id;
			p = ex + 1;
		}
	}
	for (; *p && k + 1 < sizeof u; p++)
		if (*p != '$')
			u[k++] = *p;
	u[k] = 0;
	if (cu_lee_rango(u, &r, &fin) && !*fin)
		g->datos = r;
	g->en_filas = (u8)marcado(ID_GR_FILAS);
	g->rotulos = (u8)marcado(ID_GR_ROT);
	copia(g->titulo, val_de(ID_GR_TIT), sizeof g->titulo);
	copia(g->eje_x, val_de(ID_GR_EX), sizeof g->eje_x);
	copia(g->eje_y, val_de(ID_GR_EY), sizeof g->eje_y);
	g->leyenda = (u8)elegida(ID_GR_LEY);
	g->rejilla = (u8)marcado(ID_GR_REJ);
}

static void muestra_grafico(void *u, lienzo *l, const ui_control *c, int x, int y)
{
	grafico g = gr_trabajo;
	pintor p;

	(void)u;
	lee_gr(&g);
	pt_de_lienzo(&p, l);
	pt_recorte(&p, x + 1, y + 1, c->an - 2, c->al - 2);
	gr_pinta(&p, C.lib, &g, x + 2, y + 2, c->an - 4, c->al - 4, 0.6);
	pt_vuelve(&p);
}

static void al_grafico(int id)
{
	grafico g = gr_trabajo;

	if (id != B_ACEPTAR)
		return;
	lee_gr(&g);
	{
		int i = op_grafico(C.lib, cu_hoja(), gr_indice, &g);

		C.grafico = i;
	}
	cu_aviso("Gráfico: se arrastra (y su esquina, el tamaño)");
	cu_cambio();
}

void dlg_grafico(int i)
{
	hoja *h = cu_hoja();
	int y = 14, an = 720, t;
	static char rg[120];

	gr_indice = i;
	if (i >= 0 && (u32)i < h->n_gr)
		gr_trabajo = h->gr[i];
	else {
		rango s = cu_seleccion();

		if (s.f0 == s.f1 && s.c0 == s.c1) {
			/* la región de datos de alrededor */
			u32 f1 = s.f0, c1 = s.c0, f0 = s.f0, c0 = s.c0;

			while (f0 > 0 && lib_valor(h, f0 - 1, c0)->t != V_VACIO)
				f0--;
			while (f1 + 1 < CU_FILAS && lib_valor(h, f1 + 1, c0)->t != V_VACIO)
				f1++;
			while (c0 > 0 && lib_valor(h, f0 + (f1 > f0), c0 - 1)->t != V_VACIO)
				c0--;
			while (c1 + 1 < CU_COLS && lib_valor(h, f0 + (f1 > f0), c1 + 1)->t != V_VACIO)
				c1++;
			s = cu_rango(f0, c0, f1, c1);
		}
		gr_nuevo(&gr_trabajo, h, s, GR_COLUMNAS);
		/* al lado de los datos */
		gr_trabajo.f = s.f0;
		gr_trabajo.c = (u16)(s.c1 + 2 < CU_COLS ? s.c1 + 2 : s.c1);
		gr_trabajo.dx = gr_trabajo.dy = 0;
	}
	t = gr_trabajo.tipo == GR_COLUMNAS ? (gr_trabajo.apiladas ? 1 : 0) : gr_trabajo.tipo == GR_BARRAS ? (gr_trabajo.apiladas ? 3 : 2) :
	    gr_trabajo.tipo == GR_LINEAS ? 4 : gr_trabajo.tipo == GR_SECTORES ? 5 : 6;
	{
		hoja *hd = lib_hoja_id(C.lib, gr_trabajo.hoja);
		char r[32];

		cu_rango_texto(gr_trabajo.datos, r);
		if (hd && hd != h)
			fmt(rg, sizeof rg, "'%s'!%s", hd->nombre, r);
		else
			copia(rg, r, sizeof rg);
	}
	nuevo_cuadro(i >= 0 ? "Gráfico" : "Asistente para gráficos", an, 0);
	rotulo(14, y, "&Tipo:");
	lista(ID_GR_TIPO, 14, y + U + 4, 200, U * 7 + 8, TIPOS_GR, t);
	ctl(UC_MUESTRA, ID_GR_MUESTRA, 230, y, an - 244, NULL)->al = U * 8 + 12;
	Q.pinta_extra = muestra_grafico;
	y += U * 8 + 22;
	rotulo(14, y + 4, "&Datos:");
	campo(ID_GR_RANGO, 120, y, 200, rg);
	casilla(ID_GR_FILAS, 340, y, 170, "Series en &filas", gr_trabajo.en_filas);
	casilla(ID_GR_ROT, 520, y, 190, "Con &rótulos", gr_trabajo.rotulos);
	y += H + 8;
	rotulo(14, y + 4, "Tít&ulo:");
	campo(ID_GR_TIT, 120, y, an - 134, gr_trabajo.titulo);
	y += H + 8;
	rotulo(14, y + 4, "Eje &X:");
	campo(ID_GR_EX, 120, y, 230, gr_trabajo.eje_x);
	rotulo(370, y + 4, "Eje &Y:");
	campo(ID_GR_EY, 440, y, an - 454, gr_trabajo.eje_y);
	y += H + 8;
	rotulo(14, y + 4, "&Leyenda:");
	desple(ID_GR_LEY, 120, y, 200, "Ninguna|A la derecha|Abajo", gr_trabajo.leyenda);
	casilla(ID_GR_REJ, 340, y, 300, "Rayas &guía", gr_trabajo.rejilla);
	y += H + 14;
	Q.al = y + H + 14;
	aceptar_cancelar(i >= 0 ? "Aceptar" : "&Crear");
	abre(al_grafico);
}

/* ===================================================================== */
/*  Nombres, comentarios, condicionales                                   */
/* ===================================================================== */
#define ID_NOM 170
#define ID_NOM_REF 171
#define B_AGREGA 172
#define B_QUITA 173

static void abre_nombre(void);

static void al_nombre(int id)
{
	char e[300];

	if (id == B_CANCELAR || id < 0)
		return;
	if (id == B_QUITA) {
		char x[100];

		if (val_de(ID_NOM)[0])
			copia(x, val_de(ID_NOM), sizeof x);
		else
			opcion_n(nombres_ops, elegida(ID_LISTA), x, sizeof x);
		op_nombre(C.lib, cu_hoja(), x, NULL, e, sizeof e);
		abre_nombre();
		return;
	}
	if (op_nombre(C.lib, cu_hoja(), val_de(ID_NOM), val_de(ID_NOM_REF), e, sizeof e)) {
		dlg_aviso("Definir nombre", e);
		return;
	}
	cu_aviso("Nombre «%s» definido", val_de(ID_NOM));
	if (id == B_AGREGA)
		abre_nombre();
	cu_cambio();
}

static void abre_nombre(void)
{
	hoja *h = cu_hoja();
	rango s = cu_seleccion();
	char ref[200], r[40];
	u32 i;
	int y = 14, an = 480;

	nombres_ops[0] = 0;
	for (i = 0; i < C.lib->n_nom; i++) {
		char t[300], d[200];

		if (i)
			anade(nombres_ops, "|", sizeof nombres_ops);
		fm_texto(C.lib, C.lib->nom[i].f, NULL, &C.lib->loc, d, sizeof d);
		fmt(t, sizeof t, "%s  =%s", C.lib->nom[i].nombre, d);
		anade(nombres_ops, t, sizeof nombres_ops);
	}
	if (!C.lib->n_nom)
		copia(nombres_ops, "(no hay ninguno)", sizeof nombres_ops);
	{
		/* lo elegido, fijo y con su hoja */
		char a[16], b[16];

		cu_col_texto(s.c0, a);
		cu_col_texto(s.c1, b);
		if (s.f0 == s.f1 && s.c0 == s.c1)
			fmt(r, sizeof r, "$%s$%u", a, s.f0 + 1);
		else
			fmt(r, sizeof r, "$%s$%u:$%s$%u", a, s.f0 + 1, b, s.f1 + 1);
		fmt(ref, sizeof ref, "=%s%s%s!%s", strchr(h->nombre, ' ') ? "'" : "", h->nombre, strchr(h->nombre, ' ') ? "'" : "", r);
	}
	nuevo_cuadro("Definir nombre", an, 0);
	rotulo(14, y + 4, "&Nombre:");
	campo(ID_NOM, 140, y, an - 154, "");
	y += H + 10;
	rotulo(14, y + 4, "Se &refiere a:");
	campo(ID_NOM_REF, 140, y, an - 154, ref);
	y += H + 10;
	rotulo(14, y, "Los que hay:");
	lista(ID_LISTA, 14, y + U + 4, an - 28, U * 6 + 8, nombres_ops, 0);
	y += U * 7 + 22;
	boton(B_ACEPTAR, 14, y, 110, "Aceptar", 1);
	boton(B_AGREGA, 132, y, 110, "&Agregar", 0);
	boton(B_QUITA, 250, y, 110, "&Eliminar", 0);
	boton(B_CANCELAR, an - 110, y, 96, "Cerrar", 0)->cancela = 1;
	Q.al = y + H + 14;
	abre(al_nombre);
}

void dlg_nombre(void)
{
	abre_nombre();
}

#define ID_COM 180
#define B_COM_QUITA 181

static void al_comentario(int id)
{
	hoja *h = cu_hoja();

	if (id == B_ACEPTAR) {
		char t[260];
		u32 i;

		/* («\n» o «|» son saltos de línea) */
		copia(t, val_de(ID_COM), sizeof t);
		for (i = 0; t[i]; i++)
			if (t[i] == '|')
				t[i] = '\n';
		op_comentario(C.lib, h, h->cur_f, h->cur_c, t);
	} else if (id == B_COM_QUITA)
		op_comentario(C.lib, h, h->cur_f, h->cur_c, NULL);
	cu_cambio();
}

void dlg_comentario(void)
{
	hoja *h = cu_hoja();
	comentario *c = hj_comentario(h, h->cur_f, h->cur_c);
	char t[260];
	u32 i;
	int y = 14, an = 520;

	copia(t, c && c->texto ? c->texto : "", sizeof t);
	for (i = 0; t[i]; i++)
		if (t[i] == '\n')
			t[i] = '|';
	nuevo_cuadro("Comentario", an, 0);
	rotulo(14, y + 4, "&Texto:");
	campo(ID_COM, 100, y, an - 114, t);
	y += H + 6;
	texto(100, y, an - 114, U, "(«|» parte en líneas)");
	y += U + 14;
	boton(B_ACEPTAR, 14, y, 110, "Aceptar", 1);
	boton(B_COM_QUITA, 132, y, 110, "&Eliminar", 0);
	boton(B_CANCELAR, an - 110, y, 96, "Cancelar", 0)->cancela = 1;
	Q.al = y + H + 14;
	abre(al_comentario);
}

#define ID_FC_COND 190
#define ID_FC_V1 191
#define ID_FC_V2 192
#define ID_FC_SIM 193
#define ID_FC_NEG 194
#define ID_FC_COLOR 195
#define ID_FC_FONDO 196
#define ID_FC_MUESTRA 197
static const char *const CONDS = "Mayor que|Menor que|Entre|Igual a|Distinto de|Mayor o igual que|Menor o igual que|Contiene el texto";
static char sim_ops[400];

static void muestra_condicional(void *u, lienzo *l, const ui_control *c, int x, int y)
{
	int s = elegida(ID_FC_SIM), f = elegida(ID_FC_FONDO);
	u32 col = COLS_V[elegida(ID_FC_COLOR) < 13 ? elegida(ID_FC_COLOR) : 0];
	char t[64];
	pintor p;

	(void)u;
	pt_de_lienzo(&p, l);
	if (f > 0)
		pt_trama(&p, x + 4, y + 4, c->an - 8, c->al - 8, TR_SOLIDO, FONDOS_V[f < 12 ? f : 0], 0);
	fmt(t, sizeof t, "%s%s1.234,50", s == SIM_ESTRELLA ? "*" : s > 0 && s < SIM_N ? SIMBOLO_LETRA[s] : "", s > 0 ? " " : "");
	if (marcado(ID_FC_NEG))
		lz_ui_negrita(l, x + 10, y + (c->al - lz_ui_alto()) / 2 + lz_ui_base(), t, col == CU_AUTO ? 0 : col);
	else
		lz_ui(l, x + 10, y + (c->al - lz_ui_alto()) / 2 + lz_ui_base(), t, col == CU_AUTO ? 0 : col);
}

static void al_condicional(int id)
{
	condicional c;
	int t;

	if (id != B_ACEPTAR)
		return;
	memset(&c, 0, sizeof c);
	c.r = cu_seleccion();
	c.cond = (u8)elegida(ID_FC_COND);
	if (!num_lee_entrada(val_de(ID_FC_V1), &C.lib->loc, C.lib->moneda, &c.v1, &t))
		c.v1 = 0;
	if (!num_lee_entrada(val_de(ID_FC_V2), &C.lib->loc, C.lib->moneda, &c.v2, &t))
		c.v2 = 0;
	copia(c.texto, val_de(ID_FC_V1), sizeof c.texto);
	c.simbolo = (u8)elegida(ID_FC_SIM);
	c.negrita = (u8)marcado(ID_FC_NEG);
	c.color = COLS_V[elegida(ID_FC_COLOR) < 13 ? elegida(ID_FC_COLOR) : 0];
	c.fondo = elegida(ID_FC_FONDO) ? FONDOS_V[elegida(ID_FC_FONDO) < 12 ? elegida(ID_FC_FONDO) : 0] : CU_AUTO;
	c.trama = TR_SOLIDO;
	op_condicional(C.lib, cu_hoja(), &c);
	cu_aviso("Formato condicional para lo elegido (Formato > Quitar formato condicional, para quitarlo)");
	cu_cambio();
}

void dlg_condicional(void)
{
	int y = 14, an = 560, k;

	sim_ops[0] = 0;
	for (k = 0; k < SIM_N; k++) {
		char t[64];

		if (k)
			anade(sim_ops, "|", sizeof sim_ops);
		if (k == SIM_ESTRELLA)
			fmt(t, sizeof t, "%s", NOMBRES_SIMBOLO[k]);
		else
			fmt(t, sizeof t, "%s%s%s", SIMBOLO_LETRA[k], k ? " " : "", NOMBRES_SIMBOLO[k]);
		anade(sim_ops, t, sizeof sim_ops);
	}
	nuevo_cuadro("Formato condicional", an, 0);
	rotulo(14, y + 4, "Si el valor es:");
	desple(ID_FC_COND, 160, y, 220, CONDS, 0);
	y += H + 10;
	rotulo(14, y + 4, "&Valor:");
	campo(ID_FC_V1, 160, y, 150, "");
	rotulo(330, y + 4, "y:");
	campo(ID_FC_V2, 360, y, 150, "");
	y += H + 14;
	rotulo(14, y + 4, "&Símbolo:");
	desple(ID_FC_SIM, 160, y, 220, sim_ops, SIM_ARRIBA);
	casilla(ID_FC_NEG, 400, y, 140, "&Negrita", 1);
	y += H + 10;
	rotulo(14, y + 4, "&Color:");
	desple(ID_FC_COLOR, 160, y, 220, COLS, 0);
	y += H + 10;
	rotulo(14, y + 4, "&Relleno:");
	desple(ID_FC_FONDO, 160, y, 220, FONDOS, 0);
	ctl(UC_MUESTRA, ID_FC_MUESTRA, 400, y - H - 10, 140, NULL)->al = 2 * H + 10;
	Q.pinta_extra = muestra_condicional;
	y += H + 14;
	texto(14, y, an - 28, U, "El símbolo sale delante del número: se ve sin distinguir el color.");
	y += U + 10;
	Q.al = y + H + 14;
	aceptar_cancelar(NULL);
	abre(al_condicional);
}

/* ===================================================================== */
/*  Ancho, alto, zoom, la hoja                                            */
/* ===================================================================== */
#define ID_MEDIDA 200
static int midiendo_fila;

static void al_ancho(int id)
{
	hoja *h = cu_hoja();
	rango s = cu_seleccion();
	int v = ui_numero(c_(ID_MEDIDA)) / 10;

	if (id != B_ACEPTAR)
		return;
	if (midiendo_fila)
		op_alto(C.lib, h, s.f0, s.f1, (u32)v);
	else
		op_ancho(C.lib, h, s.c0, s.c1, (u32)v);
	cu_cambio();
}

void dlg_ancho(int fila)
{
	hoja *h = cu_hoja();
	int y = 14;

	midiendo_fila = fila;
	nuevo_cuadro(fila ? "Alto de fila" : "Ancho de columna", 360, 0);
	rotulo(14, y + 4, fila ? "&Alto:" : "&Ancho:");
	numero_c(ID_MEDIDA, 120, y, 160, (int)(fila ? hj_alto(h, h->cur_f) : hj_ancho(h, h->cur_c)) * 10, 0, fila ? 8000 : 20000, 10, 0, "puntos");
	y += H + 14;
	Q.al = y + H + 14;
	aceptar_cancelar(NULL);
	abre(al_ancho);
}

#define ID_ZOOM 210

static void al_zoom(int id)
{
	hoja *h = cu_hoja();

	if (id != B_ACEPTAR)
		return;
	h->zoom = (u16)(ui_numero(c_(ID_ZOOM)) / 10);
	if (h->zoom < 10)
		h->zoom = 10;
	if (h->zoom > 400)
		h->zoom = 400;
	cu_cambio();
}

void dlg_zoom(void)
{
	int y = 14;

	nuevo_cuadro("Zoom", 340, 0);
	rotulo(14, y + 4, "&Zoom:");
	numero_c(ID_ZOOM, 110, y, 160, cu_hoja()->zoom * 10, 100, 4000, 100, 0, "%");
	y += H + 6;
	texto(14, y, 300, U, "(También con Ctrl y la rueda del ratón)");
	y += U + 14;
	Q.al = y + H + 14;
	aceptar_cancelar(NULL);
	abre(al_zoom);
}

#define ID_HOJA_NOM 220

static void al_nombre_hoja(int id)
{
	char e[200];

	if (id != B_ACEPTAR)
		return;
	if (op_hoja_nombre(C.lib, C.lib->activa, val_de(ID_HOJA_NOM), e, sizeof e))
		dlg_aviso("Nombre de la hoja", e);
	cu_cambio();
}

void dlg_nombre_hoja(void)
{
	int y = 14;

	nuevo_cuadro("Nombre de la hoja", 400, 0);
	rotulo(14, y + 4, "&Nombre:");
	campo(ID_HOJA_NOM, 110, y, 270, cu_hoja()->nombre);
	y += H + 14;
	Q.al = y + H + 14;
	aceptar_cancelar(NULL);
	abre(al_nombre_hoja);
}

#define ID_COPIA_HOJA 230
static char hojas_ops[CU_MAX_HOJAS * 34];

static void al_mover_hoja(int id)
{
	int a = elegida(ID_LISTA);

	if (id != B_ACEPTAR)
		return;
	if (marcado(ID_COPIA_HOJA)) {
		hoja *n = op_hoja_copia(C.lib, C.lib->activa, a);

		if (n)
			C.lib->activa = (u32)lib_hoja_indice(C.lib, n->id);
	} else {
		u32 de = C.lib->activa, dest = (u32)a;

		if (dest > de)
			dest--;
		if (dest >= C.lib->n_h)
			dest = C.lib->n_h - 1;
		op_hoja_mueve(C.lib, de, dest);
	}
	cu_cambio();
}

void dlg_mover_hoja(void)
{
	u32 i;
	int y = 14, an = 400;

	hojas_ops[0] = 0;
	for (i = 0; i < C.lib->n_h; i++) {
		if (i)
			anade(hojas_ops, "|", sizeof hojas_ops);
		anade(hojas_ops, C.lib->h[i]->nombre, sizeof hojas_ops);
	}
	anade(hojas_ops, "|(al final)", sizeof hojas_ops);
	nuevo_cuadro("Mover o copiar la hoja", an, 0);
	rotulo(14, y, "&Antes de la hoja:");
	lista(ID_LISTA, 14, y + U + 4, an - 28, U * 8 + 8, hojas_ops, (int)C.lib->activa);
	y += U * 9 + 22;
	casilla(ID_COPIA_HOJA, 14, y, an - 28, "Crear una &copia", 0);
	y += H + 14;
	Q.al = y + H + 14;
	aceptar_cancelar(NULL);
	abre(al_mover_hoja);
}

/* ===================================================================== */
/*  Preparar página, opciones                                             */
/* ===================================================================== */
#define ID_PG_VERT 240
#define ID_PG_HORIZ 241
#define ID_PG_PAPEL 242
#define ID_PG_ESC 243
#define ID_PG_ENC_AN 244
#define ID_PG_ENC_AL 245
#define ID_PG_M0 246		/* + 0..3 */
#define ID_PG_CH 250
#define ID_PG_CV 251
#define ID_PG_ENC 252
#define ID_PG_PIE 253
#define ID_PG_REJ 254
#define B_PG_PREVIA 255
static const char *const PAPELES = "A4 (21 x 29,7 cm)|Carta (21,6 x 27,9 cm)|A3 (29,7 x 42 cm)|A5 (14,8 x 21 cm)|Legal (21,6 x 35,6 cm)";

static void al_preparar(int id)
{
	pagina p = cu_hoja()->pag;
	int k;

	if (id != B_ACEPTAR && id != B_PG_PREVIA)
		return;
	p.horizontal = (u8)marcado(ID_PG_HORIZ);
	p.papel = (u8)elegida(ID_PG_PAPEL);
	p.escala = (u16)(ui_numero(c_(ID_PG_ESC)) / 10);
	p.encajar_an = (u16)(ui_numero(c_(ID_PG_ENC_AN)) / 10);
	p.encajar_al = (u16)(ui_numero(c_(ID_PG_ENC_AL)) / 10);
	for (k = 0; k < 4; k++)
		p.marg[k] = ui_numero(c_(ID_PG_M0 + k)) / 10.0;
	p.centrar_h = (u8)marcado(ID_PG_CH);
	p.centrar_v = (u8)marcado(ID_PG_CV);
	copia(p.encabezado, val_de(ID_PG_ENC), sizeof p.encabezado);
	copia(p.pie, val_de(ID_PG_PIE), sizeof p.pie);
	p.rejilla = (u8)marcado(ID_PG_REJ);
	op_pagina(C.lib, cu_hoja(), &p);
	if (id == B_PG_PREVIA) {
		C.previa = 1;
		C.pag_previa = 0;
	}
	cu_cambio();
}

void dlg_preparar(void)
{
	const pagina *p = &cu_hoja()->pag;
	int y = 14, an = 600, k;
	static const char *const ms[4] = { "Izquierdo:", "Arriba:", "Derecho:", "Abajo:" };

	nuevo_cuadro("Preparar página", an, 0);
	rotulo(14, y, "Orientación:");
	opcion(ID_PG_VERT, 150, y, 140, "&Vertical", 1, !p->horizontal);
	opcion(ID_PG_HORIZ, 300, y, 160, "&Horizontal", 1, p->horizontal);
	y += H + 8;
	rotulo(14, y + 4, "&Papel:");
	desple(ID_PG_PAPEL, 150, y, 260, PAPELES, p->papel);
	y += H + 8;
	rotulo(14, y + 4, "&Escala:");
	numero_c(ID_PG_ESC, 150, y, 110, p->escala * 10, 100, 4000, 50, 0, "%");
	rotulo(280, y + 4, "o en:");
	numero_c(ID_PG_ENC_AN, 330, y, 80, p->encajar_an * 10, 0, 990, 10, 0, NULL);
	rotulo(420, y + 4, "de ancho por");
	numero_c(ID_PG_ENC_AL, 520, y, 70, p->encajar_al * 10, 0, 990, 10, 0, NULL);
	y += H + 4;
	texto(330, y, 260, U, "(páginas; 0: lo que salga)");
	y += U + 8;
	for (k = 0; k < 4; k++) {
		rotulo(14 + (k % 2) * 290, y + 4, ms[k]);
		numero_c(ID_PG_M0 + k, 120 + (k % 2) * 290, y, 140, (int)(p->marg[k] * 10 + 0.5), 0, 200, 1, 1, "cm");
		if (k % 2)
			y += H + 8;
	}
	casilla(ID_PG_CH, 14, y, 260, "Centrar a lo &ancho", p->centrar_h);
	casilla(ID_PG_CV, 300, y, 260, "Centrar a lo a&lto", p->centrar_v);
	y += H + 8;
	rotulo(14, y + 4, "E&ncabezado:");
	campo(ID_PG_ENC, 150, y, an - 164, p->encabezado);
	y += H + 8;
	rotulo(14, y + 4, "P&ie:");
	campo(ID_PG_PIE, 150, y, an - 164, p->pie);
	y += H + 4;
	texto(150, y, an - 164, U, "&P la página, &N cuántas, &A la hoja, &F el archivo, &D la fecha");
	y += U + 8;
	casilla(ID_PG_REJ, 14, y, 400, "Imprimir la &cuadrícula", p->rejilla);
	y += H + 14;
	boton(B_PG_PREVIA, 14, y, 160, "Vista p&reliminar", 0);
	Q.al = y + H + 14;
	aceptar_cancelar(NULL);
	abre(al_preparar);
}

#define ID_OP_COMA 260
#define ID_OP_PUNTO 261
#define ID_OP_EN 262
#define ID_OP_MONEDA 263
#define ID_OP_AUTOR 264
#define ID_OP_REJ 265

static void al_opciones(int id)
{
	if (id != B_ACEPTAR)
		return;
	if (marcado(ID_OP_PUNTO)) {
		C.lib->loc.dec = '.';
		C.lib->loc.sep = ',';
		C.lib->loc.mil = ',';
	} else {
		C.lib->loc.dec = ',';
		C.lib->loc.sep = ';';
		C.lib->loc.mil = '.';
	}
	C.lib->loc.ingles = (u8)marcado(ID_OP_EN);
	if (val_de(ID_OP_MONEDA)[0])
		copia(C.lib->moneda, val_de(ID_OP_MONEDA), sizeof C.lib->moneda);
	copia(C.lib->autor, val_de(ID_OP_AUTOR), sizeof C.lib->autor);
	cu_hoja()->sin_rejilla = (u8)!marcado(ID_OP_REJ);
	cu_aviso("Opciones: números con «%c», argumentos con «%c», funciones en %s", C.lib->loc.dec, C.lib->loc.sep, C.lib->loc.ingles ? "inglés" : "español");
	cu_cambio();
}

void dlg_opciones(void)
{
	int y = 14, an = 560;

	nuevo_cuadro("Opciones", an, 0);
	rotulo(14, y, "Los números y las fórmulas:");
	y += U + 6;
	opcion(ID_OP_COMA, 24, y, an - 40, "Coma decimal y «;» entre los argumentos (1.234,5; =SUMA(A1;B2))", 1, C.lib->loc.dec == ',');
	y += H + 2;
	opcion(ID_OP_PUNTO, 24, y, an - 40, "Punto decimal y «,» entre los argumentos (1,234.5; =SUM(A1,B2))", 1, C.lib->loc.dec == '.');
	y += H + 8;
	casilla(ID_OP_EN, 14, y, an - 28, "Las funciones con su nombre en &inglés (SUM, IF, VLOOKUP...)", C.lib->loc.ingles);
	y += H + 4;
	texto(24, y, an - 40, U, "(Al escribir se entienden siempre las dos: SUMA y SUM)");
	y += U + 10;
	rotulo(14, y + 4, "&Moneda:");
	campo(ID_OP_MONEDA, 120, y, 120, C.lib->moneda);
	rotulo(260, y + 4, "&Autor:");
	campo(ID_OP_AUTOR, 330, y, an - 344, C.lib->autor);
	y += H + 10;
	casilla(ID_OP_REJ, 14, y, an - 28, "Ver las &líneas de división", !cu_hoja()->sin_rejilla);
	y += H + 14;
	Q.al = y + H + 14;
	aceptar_cancelar(NULL);
	abre(al_opciones);
}

/* ===================================================================== */
/*  El asistente para funciones                                           */
/* ===================================================================== */
#define ID_FN 270
static char fn_ops[4096];

static void al_funcion(int id)
{
	char n[64], t[80];

	if (id != B_ACEPTAR)
		return;
	opcion_n(fn_ops, elegida(ID_FN), n, sizeof n);
	if (C.lib->loc.ingles) {
		int k = fn_busca(n, largo(n));

		if (k >= 0)
			copia(n, CU_FUNCIONES[k].en, sizeof n);
	}
	if (!C.editando) {
		hoja *h = cu_hoja();

		C.editando = 1;
		C.ed_nuevo = 1;
		C.ed_f = h->cur_f;
		C.ed_c = h->cur_c;
		C.ed_hoja = h->id;
		C.ed_n = C.ed_cur = C.ed_ancla = 0;
		C.ed[C.ed_n++] = '=';
		C.ed_cur = C.ed_ancla = 1;
	}
	fmt(t, sizeof t, "%s(", n);
	{
		u32 nn, *w = u32_de_utf8(t, largo(t), &nn), k;

		if (w) {
			if (C.ed_n + nn < 4090) {
				memmove(C.ed + C.ed_cur + nn, C.ed + C.ed_cur, (C.ed_n - C.ed_cur) * sizeof(u32));
				for (k = 0; k < nn; k++)
					C.ed[C.ed_cur + k] = w[k];
				C.ed_n += nn;
				C.ed_cur += nn;
				C.ed_ancla = C.ed_cur;
			}
			so_suelta(w);
		}
	}
	cu_aviso("Escriba los argumentos separados por «%c» (o señálelos con las flechas) y pulse Intro", C.lib->loc.sep);
	C.sucio = 1;
}

void dlg_funcion(void)
{
	int y = 14, an = 420;

	copia(fn_ops, fn_lista(), sizeof fn_ops);
	nuevo_cuadro("Asistente para funciones", an, 0);
	rotulo(14, y, "&Funciones (se entienden en español y en inglés):");
	lista(ID_FN, 14, y + U + 4, an - 28, U * 14 + 8, fn_ops, 0);
	y += U * 15 + 22;
	Q.al = y + H + 14;
	aceptar_cancelar("&Insertar");
	abre(al_funcion);
}

/* ===================================================================== */
/*  El autofiltro                                                         */
/* ===================================================================== */
static u32 filtro_c;
static char filtro_ops[8000];
static char filtro_val[60][96];
static int n_filtro_val;

void dlg_filtro(u32 c)
{
	hoja *h = cu_hoja();
	u32 f;
	int k;

	filtro_c = c;
	n_filtro_val = 0;
	copia(filtro_ops, "(Todas)|(Las 10 más altas)|(Las 10 más bajas)|(Vacías)|(No vacías)|-", sizeof filtro_ops);
	/* los valores que hay (sin repetir, los 60 primeros) */
	for (f = h->filtro.f0 + 1; f <= h->filtro.f1 && n_filtro_val < 60; f++) {
		const celda *x = hj_celda(h, f, c);
		char t[96];

		if (!x || x->v.t == V_VACIO)
			continue;
		lib_texto_visible(C.lib, x, t, sizeof t, NULL);
		for (k = 0; k < n_filtro_val; k++)
			if (iguales(filtro_val[k], t))
				break;
		if (k < n_filtro_val)
			continue;
		copia(filtro_val[n_filtro_val++], t, sizeof filtro_val[0]);
	}
	/* por orden */
	for (k = 1; k < n_filtro_val; k++) {
		char t[96];
		int j;

		copia(t, filtro_val[k], sizeof t);
		for (j = k; j > 0 && strcmp(filtro_val[j - 1], t) > 0; j--)
			copia(filtro_val[j], filtro_val[j - 1], sizeof filtro_val[0]);
		copia(filtro_val[j], t, sizeof filtro_val[0]);
	}
	for (k = 0; k < n_filtro_val; k++) {
		char t[110];
		u32 j;

		copia(t, filtro_val[k], sizeof t);
		for (j = 0; t[j]; j++)
			if (t[j] == '|')
				t[j] = '/';
		anade(filtro_ops, "|", sizeof filtro_ops);
		anade(filtro_ops, t[0] == '-' && !t[1] ? "- " : t, sizeof filtro_ops);
	}
	{
		int x, y, an, al;

		if (vs_celda_rect(h->filtro.f0, c, &x, &y, &an, &al))
			so_menu(C.v, x, y + al, filtro_ops, 0, 0x600);
	}
}

void dlg_filtro_elegido(int op)
{
	filtro_col fc;

	if (op < 0)
		return;
	memset(&fc, 0, sizeof fc);
	fc.c = (u16)filtro_c;
	switch (op) {
	case 0: fc.tipo = FI_TODO; break;
	case 1: fc.tipo = FI_DIEZ_MAS; fc.v = 10; break;
	case 2: fc.tipo = FI_DIEZ_MENOS; fc.v = 10; break;
	case 3: fc.tipo = FI_VACIAS; break;
	case 4: fc.tipo = FI_NO_VACIAS; break;
	case 5: return;
	default:
		if (op - 6 >= n_filtro_val)
			return;
		fc.tipo = FI_IGUAL;
		copia(fc.texto, filtro_val[op - 6], sizeof fc.texto);
	}
	op_filtra(C.lib, cu_hoja(), &fc);
	cu_aviso(fc.tipo == FI_TODO ? "Se ven todas las filas" : "Filtrado: las filas que no casan, ocultas (el botón lleva un embudo)");
	cu_cambio();
}

/* ===================================================================== */
/*  Acerca de y la ayuda                                                  */
/* ===================================================================== */
void dlg_acerca(void)
{
	char t[600];

	fmt(t, sizeof t, "Cuentas %s, la hoja de cálculo de Hispa-Pentium\n(32 bits, en C portable: la misma para Hispa-64).\n\n"
		  "Copyright (C) 2026 Carlos Sanz y contribuidores.\nGPL-2.0 o posterior.\n\nLleva miniz, stb y las letras DejaVu (licencias libres).\n\n"
		  "Este libro: %u hojas, %u fórmulas, %u estilos.",
	    CUENTAS_VERSION, C.lib->n_h, C.lib->n_formulas, C.lib->n_est);
	dlg_aviso("Acerca de Cuentas", t);
}

void dlg_ayuda(void)
{
	dlg_aviso("Ayuda de Cuentas",
		  "Escriba en una celda y pulse Intro. Una fórmula empieza por «=»: =SUMA(A1:A9).\n"
		  "Las flechas, Tab e Intro mueven la celda activa; con Mayús se elige un rango;\n"
		  "Ctrl y una flecha van al borde de los datos; Ctrl+Inicio a A1, Ctrl+Fin a la última.\n"
		  "F2 corrige la celda (y F4 pone y quita los «$»); Esc deja la celda como estaba.\n"
		  "Escribiendo una fórmula, las flechas o el ratón señalan las celdas.\n"
		  "El cuadradito de abajo a la derecha de lo elegido se arrastra para rellenar\n"
		  "(1, 2... enero, febrero... Producto 1, 2...); el borde, para moverlo.\n"
		  "Ctrl+C copia, Ctrl+X corta, Ctrl+V pega (Edición > Pegado especial: valores, formatos...).\n"
		  "Ctrl+Z deshace (sin límite) y Ctrl+Y rehace. F9 calcula todo otra vez.\n"
		  "Ctrl+rueda cambia el zoom; Ctrl+Av Pág y Ctrl+Re Pág pasan de hoja.\n"
		  "Archivo > Guardar como: .HOJ (de Cuentas), .XLSX (Excel), .ODS (LibreOffice), .CSV;\n"
		  "y Archivo > Exportar a PDF. Las funciones, en ? > Las funciones.");
}
