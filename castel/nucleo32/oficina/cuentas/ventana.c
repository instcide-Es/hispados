/* ventana.c - Cuentas 2: la ventana, al estilo de Excel 5 en español:
 * arriba la barra de menús (Archivo, Edición, Ver, Insertar, Formato,
 * Herramientas, Datos, Ventana, ?: lo que aún no hay sale en gris), la barra
 * Estándar (archivo, edición, Σ, fx, ordenar, gráfico, filtro, PDF, zoom) y
 * la de Formato (fuente, tamaño, N K S, alineación, combinar y centrar,
 * moneda, porcentaje, millares, decimales, bordes, relleno, color), y la
 * barra de fórmulas con el cuadro de nombres; en medio la hoja (vista.c); abajo
 * las pestañas de las hojas con la barra de desplazamiento y la barra de
 * estado (Listo, Introducir, Modificar, Señalar; la suma, el promedio y la
 * cuenta de lo elegido). Todo pintado en un lienzo y dado al sistema de una vez.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include <stdarg.h>
#include "ventana.h"
#include "archivo.h"
#include "operar.h"
#include "deshacer.h"
#include "calculo.h"
#include "formula.h"
#include "numeros.h"
#include "celdas.h"
#include "grafico.h"
#include "letras.h"
#include "cuentas.h"
#include "f_pdf.h"

cuentas_t C;

#define ETIQ_MENU 0x100
#define ETIQ_CONTEXTO 0x200
#define ETIQ_LISTA 0x300
#define ETIQ_NOMBRES 0x500
#define ETIQ_FILTRO 0x600
#define CON_FLECHAS 0x40000000u

static const char *const MENUS = "&Archivo|&Edición|&Ver|&Insertar|&Formato|&Herramientas|&Datos|Ve&ntana|&?";

typedef struct {
	const char *texto;
	int orden;
} opcion_t;

#define R { "-", O_NADA }
static const opcion_t m_archivo[] = {
	{ "Nuevo (Ctrl+U)", O_NUEVO }, { "Abrir... (Ctrl+A)", O_ABRIR }, { "Cerrar", O_CERRAR }, R,
	{ "Guardar (Ctrl+G)", O_GUARDAR }, { "Guardar como... (F12)", O_GUARDAR_COMO }, { "Exportar a PDF...", O_EXPORTAR_PDF }, R,
	{ "Preparar página...", O_PREPARAR }, { "Establecer área de impresión", O_AREA }, { "Quitar área de impresión", O_QUITAR_AREA },
	{ "Vista preliminar", O_PREVIA }, { "Imprimir... (Ctrl+P)", O_IMPRIMIR }, R, { "Propiedades...", O_PROPIEDADES }, R,
	{ "Salir", O_SALIR }, { NULL, 0 }
};
static const opcion_t m_edicion[] = {
	{ "Deshacer (Ctrl+Z)", O_DESHACER }, { "Rehacer (Ctrl+Y)", O_REHACER }, R, { "Cortar (Ctrl+X)", O_CORTAR },
	{ "Copiar (Ctrl+C)", O_COPIAR }, { "Pegar (Ctrl+V)", O_PEGAR }, { "Pegado especial...", O_PEGADO_ESPECIAL }, R,
	{ "Rellenar hacia abajo", O_RELLENAR_ABAJO }, { "Rellenar hacia la derecha", O_RELLENAR_DERECHA }, { "Rellenar series...", O_SERIES }, R,
	{ "Borrar todo", O_BORRAR_TODO }, { "Borrar formatos", O_BORRAR_FORMATOS }, { "Borrar contenido (Supr)", O_BORRAR_CONTENIDO },
	{ "Borrar comentarios", O_BORRAR_COMENTARIOS }, { "Eliminar...", O_ELIMINAR }, R, { "Eliminar hoja", O_ELIMINAR_HOJA },
	{ "Mover o copiar hoja...", O_MOVER_HOJA }, R, { "Buscar... (Ctrl+B)", O_BUSCAR }, { "Reemplazar... (Ctrl+L)", O_REEMPLAZAR },
	{ "Ir a... (F5)", O_IR_A }, { "Seleccionar todo (Ctrl+E)", O_TODO }, { NULL, 0 }
};
static const opcion_t m_ver[] = {
	{ "Barra de fórmulas", O_VER_BARRA_FORMULAS }, { "Barra de estado", O_VER_ESTADO }, { "Barras de herramientas", O_VER_BARRAS }, R,
	{ "Pantalla completa", O_COMPLETA }, { "Zoom...", O_ZOOM }, R, { "Fórmulas en las celdas", O_VER_FORMULAS },
	{ "Líneas de división", O_VER_REJILLA }, { NULL, 0 }
};
static const opcion_t m_insertar[] = {
	{ "Celdas...", O_INS_CELDAS }, { "Filas", O_INS_FILAS }, { "Columnas", O_INS_COLUMNAS }, { "Hoja de cálculo (Mayús+F11)", O_INS_HOJA }, R,
	{ "Gráfico... (F11)", O_GRAFICO }, R, { "Salto de página", O_INS_SALTO }, { "Quitar salto de página", O_QUITAR_SALTO }, R,
	{ "Función...", O_FUNCION }, { "Nombre...", O_NOMBRE }, { "Comentario", O_COMENTARIO }, R, { "Imagen...", O_INS_IMAGEN },
	{ NULL, 0 }
};
static const opcion_t m_formato[] = {
	{ "Celdas... (Ctrl+1)", O_FMT_CELDAS }, R, { "Fila: alto...", O_ALTO_FILA }, { "Fila: autoajustar", O_AUTO_FILA },
	{ "Fila: ocultar", O_OCULTAR_FILAS }, { "Fila: mostrar", O_MOSTRAR_FILAS }, R, { "Columna: ancho...", O_ANCHO_COL },
	{ "Columna: autoajustar", O_AUTO_COL }, { "Columna: ocultar", O_OCULTAR_COLS }, { "Columna: mostrar", O_MOSTRAR_COLS },
	{ "Columna: ancho estándar", O_ANCHO_ESTANDAR }, R, { "Hoja: cambiar nombre...", O_NOMBRE_HOJA }, R,
	{ "Combinar y centrar", O_COMBINAR_CENTRAR }, { "Combinar celdas", O_COMBINAR }, { "Separar celdas", O_SEPARAR }, R,
	{ "Formato condicional...", O_CONDICIONAL }, { "Quitar formato condicional", O_QUITAR_CONDICIONAL }, R,
	{ "Autoformato...", O_AUTOFORMATO }, { "Estilo...", O_ESTILO }, { NULL, 0 }
};
static const opcion_t m_herramientas[] = {
	{ "Ortografía...", O_ORTOGRAFIA }, { "Proteger...", O_PROTEGER }, R, { "Calcular ahora (F9)", O_RECALCULAR }, R,
	{ "Opciones...", O_OPCIONES }, { NULL, 0 }
};
static const opcion_t m_datos[] = {
	{ "Ordenar...", O_ORDENAR }, { "Autofiltro", O_AUTOFILTRO }, { "Mostrar todo", O_MOSTRAR_TODO }, R, { "Formulario...", O_FORMULARIO },
	{ "Subtotales...", O_SUBTOTALES }, { "Validación...", O_VALIDACION }, { "Texto en columnas...", O_TEXTO_COLUMNAS },
	{ "Tabla dinámica...", O_DINAMICA }, { NULL, 0 }
};
static const opcion_t m_ventana[] = {
	{ "Nueva ventana", O_NUEVA_VENTANA }, { "Inmovilizar paneles", O_INMOVILIZAR }, { "Dividir", O_DIVIDIR }, R,
	{ "1 Libro1", O_LIBRO1 }, { NULL, 0 }
};
static const opcion_t m_ayuda[] = {
	{ "Contenido (F1)", O_CONTENIDO }, { "Las funciones", O_FUNCIONES }, R, { "Acerca de Cuentas", O_ACERCA }, { NULL, 0 }
};
static const opcion_t *const menus[9] = { m_archivo, m_edicion, m_ver, m_insertar, m_formato, m_herramientas, m_datos, m_ventana, m_ayuda };

static ui_boton barra1[] = {
	{ O_NUEVO, UB_BOTON, IC_NUEVO, "Libro nuevo (Ctrl+U)" },
	{ O_ABRIR, UB_BOTON, IC_ABRIR, "Abrir un libro (Ctrl+A)" },
	{ O_GUARDAR, UB_BOTON, IC_GUARDAR, "Guardar (Ctrl+G)" },
	{ 0, UB_SEPARA },
	{ O_IMPRIMIR, UB_BOTON, IC_IMPRIMIR, "Imprimir: aún no hay impresoras; ofrece exportar a PDF (Ctrl+P)" },
	{ O_PREVIA, UB_CONMUTA, IC_VISTA_PREVIA, "Vista preliminar" },
	{ 0, UB_SEPARA },
	{ O_CORTAR, UB_BOTON, IC_CORTAR, "Cortar (Ctrl+X)" },
	{ O_COPIAR, UB_BOTON, IC_COPIAR, "Copiar (Ctrl+C)" },
	{ O_PEGAR, UB_BOTON, IC_PEGAR, "Pegar (Ctrl+V)" },
	{ O_BROCHA, UB_CONMUTA, IC_BROCHA, "Copiar formato: elija adónde" },
	{ 0, UB_SEPARA },
	{ O_DESHACER, UB_BOTON, IC_DESHACER, "Deshacer (Ctrl+Z)" },
	{ O_REHACER, UB_BOTON, IC_REHACER, "Rehacer (Ctrl+Y)" },
	{ 0, UB_SEPARA },
	{ O_AUTOSUMA, UB_BOTON, IC_AUTOSUMA, "Autosuma: la suma de los números de encima (o de la izquierda)" },
	{ O_FUNCION, UB_BOTON, IC_FX, "Asistente para funciones" },
	{ O_ORDEN_ASC, UB_BOTON, IC_ORDEN_ASC, "Ordenar de menor a mayor por la columna de la celda activa" },
	{ O_ORDEN_DESC, UB_BOTON, IC_ORDEN_DESC, "Ordenar de mayor a menor por la columna de la celda activa" },
	{ 0, UB_SEPARA },
	{ O_GRAFICO, UB_BOTON, IC_GRAFICO, "Asistente para gráficos: un gráfico de lo elegido (F11)" },
	{ O_AUTOFILTRO, UB_CONMUTA, IC_FILTRO, "Autofiltro" },
	{ O_EXPORTAR_PDF, UB_BOTON, IC_PDF, "Exportar a PDF" },
	{ 0, UB_SEPARA },
	{ O_LISTA_ZOOM, UB_LISTA, 0, "Zoom (Ctrl+rueda del ratón)", 72 },
	{ O_CONTENIDO, UB_BOTON, IC_AYUDA, "Ayuda (F1)" },
};
#define N1 ((int)(sizeof barra1 / sizeof barra1[0]))
static ui_boton barra2[] = {
	{ O_LISTA_FUENTE, UB_LISTA, 0, "Fuente (las letras TrueType: las DejaVu y las de \\DOCUMENT\\LETRAS)", 150 },
	{ O_LISTA_TAMANO, UB_LISTA, 0, "Tamaño de la letra (puntos)", 52 },
	{ 0, UB_SEPARA },
	{ O_NEGRITA, UB_CONMUTA, IC_NEGRITA, "Negrita (Ctrl+N)" },
	{ O_CURSIVA, UB_CONMUTA, IC_CURSIVA, "Cursiva (Ctrl+K)" },
	{ O_SUBRAYADO, UB_CONMUTA, IC_SUBRAYADO, "Subrayado (Ctrl+S)" },
	{ 0, UB_SEPARA },
	{ O_IZQ, UB_CONMUTA, IC_IZQUIERDA, "Alinear a la izquierda" },
	{ O_CENTRO, UB_CONMUTA, IC_CENTRO, "Centrar" },
	{ O_DER, UB_CONMUTA, IC_DERECHA, "Alinear a la derecha" },
	{ O_COMBINAR_CENTRAR, UB_CONMUTA, IC_COMBINAR, "Combinar y centrar" },
	{ 0, UB_SEPARA },
	{ O_MONEDA, UB_BOTON, IC_MONEDA, "Formato de moneda: 1.234,50 €" },
	{ O_PORCENTAJE, UB_BOTON, IC_PORCENTAJE, "Formato de porcentaje: 12%" },
	{ O_MILES, UB_BOTON, IC_MILES, "Separador de millares: 1.234,50" },
	{ O_DEC_MAS, UB_BOTON, IC_DEC_MAS, "Un decimal más" },
	{ O_DEC_MENOS, UB_BOTON, IC_DEC_MENOS, "Un decimal menos" },
	{ 0, UB_SEPARA },
	{ O_BORDES, UB_COLOR, IC_BORDES, "Bordes (la flecha: elegir cuáles)" },
	{ O_RELLENO, UB_COLOR, IC_RELLENO, "Color de relleno (la flecha: elegir el color y la trama)" },
	{ O_COLOR, UB_COLOR, IC_COLOR_LETRA, "Color de la letra (la flecha: elegir otro)" },
	{ 0, UB_SEPARA },
	{ O_COMENTARIO, UB_BOTON, IC_COMENTARIO, "Insertar un comentario en la celda" },
	{ O_INMOVILIZAR, UB_CONMUTA, IC_INMOVILIZAR, "Inmovilizar paneles (las filas de encima y las columnas de la izquierda de la celda activa)" },
};
#define N2 ((int)(sizeof barra2 / sizeof barra2[0]))

static const char *const TAMANOS = "8|9|10|11|12|14|16|18|20|22|24|26|28|36|48|72";
static const char *const ZOOMS = "50%|75%|85%|100%|125%|150%|200%|Lo elegido";
static const char *const COLORES = "Automático (negro)|Rojo|Granate|Naranja|Amarillo oscuro|Verde|Verde azulado|Azul|Azul marino|Morado|Gris|Blanco";
static const u32 colores_v[] = { CU_AUTO, 0xC62828, 0x7B1E1E, 0xE65100, 0xB38B00, 0x2E7D32, 0x00796B, 0x1565C0, 0x1A237E, 0x6A1B9A, 0x757575, 0xFFFFFF };
static const char *const RELLENOS = "Sin relleno|Amarillo|Verde claro|Turquesa|Rosa|Azul claro|Gris claro|Naranja claro|Gris 50% (trama)|Rayas diagonales (trama)|Cuadrícula (trama)|Otro (con trama)...";
static const u32 rellenos_v[] = { 0, 0xFFF176, 0xC5E1A5, 0x80DEEA, 0xF8BBD0, 0xBBDEFB, 0xE0E0E0, 0xFFCC80, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF };
static const char *const BORDES = "Todos los bordes|Borde exterior|Borde exterior grueso|Borde de abajo|Borde de abajo doble|Borde de arriba|Borde izquierdo|Borde derecho|Sin bordes";

/* la geometría */
static int y_b1, y_b2, y_fx, al_fx, y_hoja, al_hoja, y_pie, al_pie, y_estado, an_hoja;
static ui_desp desp_v, desp_h;
static int encima1 = -1, encima2 = -1, encima_menu = -1, pulsado1 = -1, pulsado2 = -1;
static int menu_abierto = -1;
static int an_nombres = 110, x_texto_fx, an_pestanas, tab0;
static int pestana_x[CU_MAX_HOJAS + 1], pestana_an[CU_MAX_HOJAS + 1], n_pestanas_vistas;
static u32 tics_pintar;

/* ===================================================================== */
void cu_aviso(const char *f, ...)
{
	va_list a;

	va_start(a, f);
	vfmt(C.aviso, sizeof C.aviso, f, a);
	va_end(a);
	dice("cuentas: aviso: %s", C.aviso);
}

hoja *cu_hoja(void)
{
	hoja *h = lib_hoja(C.lib, C.lib->activa);

	if (!h) {
		C.lib->activa = 0;
		h = C.lib->h[0];
	}
	return h;
}

void cu_titulo(void)
{
	char t[300];

	fmt(t, sizeof t, "%s%s - Cuentas", C.ruta[0] ? sin_carpeta(C.ruta) : "Libro1", C.lib && C.lib->cambiado ? " *" : "");
	so_titulo(C.v, t);
}

rango cu_seleccion(void)
{
	hoja *h = cu_hoja();
	rango s = h->sel;

	if (s.f1 >= CU_FILAS || s.c1 >= CU_COLS || s.f0 > s.f1 || s.c0 > s.c1 || !rg_dentro(s, h->cur_f, h->cur_c))
		s = cu_rango(h->cur_f, h->cur_c, h->cur_f, h->cur_c);
	return s;
}

void cu_sel_rango(rango r)
{
	hoja *h = cu_hoja();

	h->sel = r;
	if (!rg_dentro(r, h->cur_f, h->cur_c)) {
		h->cur_f = r.f0;
		h->cur_c = r.c0;
	}
	C.sucio = 1;
}

/* la celda activa a (f, c); con «ampliar», lo elegido va del ancla a ella */
void cu_elige(u32 f, u32 c, int ampliar)
{
	hoja *h = cu_hoja();

	if (f >= CU_FILAS)
		f = CU_FILAS - 1;
	if (c >= CU_COLS)
		c = CU_COLS - 1;
	if (ampliar) {
		h->sel = cu_rango(C.ancla_f, C.ancla_c, f, c);
		/* (las combinadas, enteras) */
		{
			u32 i;
			int otra = 1;

			while (otra) {
				otra = 0;
				for (i = 0; i < h->n_comb; i++)
					if (rg_pisa(h->comb[i], h->sel) && !(h->comb[i].f0 >= h->sel.f0 && h->comb[i].f1 <= h->sel.f1 &&
									    h->comb[i].c0 >= h->sel.c0 && h->comb[i].c1 <= h->sel.c1)) {
						rango a = h->sel, b = h->comb[i];

						h->sel = cu_rango(a.f0 < b.f0 ? a.f0 : b.f0, a.c0 < b.c0 ? a.c0 : b.c0, a.f1 > b.f1 ? a.f1 : b.f1, a.c1 > b.c1 ? a.c1 : b.c1);
						otra = 1;
					}
			}
		}
		cu_ve_celda(f, c);
	} else {
		int i = hj_combinada(h, f, c);

		if (i >= 0) {
			f = h->comb[i].f0;
			c = h->comb[i].c0;
		}
		h->cur_f = f;
		h->cur_c = c;
		C.ancla_f = f;
		C.ancla_c = c;
		h->sel = cu_rango(f, c, f, c);
		if (i >= 0)
			h->sel = h->comb[i];
		cu_ve_celda(f, c);
	}
	C.grafico = -1;
	C.sucio = 1;
}

void cu_ve_celda(u32 f, u32 c)
{
	vs_a_la_vista(f, c);
	C.sucio = 1;
}

void cu_cambio(void)
{
	C.sucio = 1;
	C.todo = 1;
	cu_titulo();
}

/* ===================================================================== */
/*  ¿Se puede hacer?                                                      */
/* ===================================================================== */
static int activa(int o)
{
	hoja *h = cu_hoja();

	switch (o) {
	case O_DESHACER: return dh_hay(C.lib, 0);
	case O_REHACER: return dh_hay(C.lib, 1);
	case O_PEGADO_ESPECIAL: return op_hay_recorte();
	case O_ELIMINAR_HOJA: return C.lib->n_h > 1;
	case O_QUITAR_AREA: return h->hay_area;
	case O_SEPARAR: return hj_combinada(h, h->cur_f, h->cur_c) >= 0;
	case O_MOSTRAR_TODO: return h->hay_filtro;
	case O_QUITAR_CONDICIONAL: return h->n_cond > 0;
	/* lo que aún no hay */
	case O_INS_IMAGEN: case O_AUTOFORMATO: case O_ESTILO: case O_ORTOGRAFIA: case O_PROTEGER: case O_FORMULARIO:
	case O_SUBTOTALES: case O_VALIDACION: case O_TEXTO_COLUMNAS: case O_DINAMICA: case O_DIVIDIR:
		return 0;
	}
	return 1;
}

static void texto_menu(int m, char *t, u32 cabe, u32 *grises)
{
	const opcion_t *o = menus[m];
	hoja *h = cu_hoja();
	int i;

	t[0] = 0;
	*grises = 0;
	for (i = 0; o[i].texto; i++) {
		char x[140];
		int marca = 0;

		if (i)
			anade(t, "|", cabe);
		copia(x, o[i].texto, sizeof x);
		if (o[i].orden == O_LIBRO1)
			fmt(x, sizeof x, "1 %s", C.ruta[0] ? sin_carpeta(C.ruta) : "Libro1");
		if (o[i].orden == O_INMOVILIZAR && (h->inm_f || h->inm_c))
			copia(x, "Movilizar paneles", sizeof x);
		if (o[i].orden == O_DESHACER && dh_hay(C.lib, 0))
			fmt(x, sizeof x, "Deshacer %s (Ctrl+Z)", dh_nombre(C.lib, 0));
		if (o[i].orden == O_REHACER && dh_hay(C.lib, 1))
			fmt(x, sizeof x, "Rehacer %s (Ctrl+Y)", dh_nombre(C.lib, 1));
		marca = (o[i].orden == O_VER_BARRA_FORMULAS && C.ver_formulas_barra) || (o[i].orden == O_VER_ESTADO && C.ver_estado) ||
			(o[i].orden == O_VER_BARRAS && C.ver_barras) || (o[i].orden == O_COMPLETA && C.completa) ||
			(o[i].orden == O_VER_FORMULAS && C.ver_formulas) || (o[i].orden == O_VER_REJILLA && !h->sin_rejilla) ||
			(o[i].orden == O_AUTOFILTRO && h->hay_filtro) || (o[i].orden == O_PREVIA && C.previa);
		if (marca) {
			char y[150];

			fmt(y, sizeof y, "\xc2\xbb %s", x);
			copia(x, y, sizeof x);
		}
		anade(t, x, cabe);
		if (o[i].orden != O_NADA && !activa(o[i].orden))
			*grises |= 1u << i;
	}
}

static void abre_menu(int m)
{
	char t[1400];
	u32 g;
	int x0 = 0, i, cx = 6;

	if (m < 0 || m > 8)
		return;
	for (i = 0; i <= m; i++) {
		const char *s = MENUS;
		int k = 0;
		u32 j = 0;
		char r[64];

		while (*s && k < i) {
			if (*s == '|')
				k++;
			s++;
		}
		while (*s && *s != '|' && j + 1 < sizeof r)
			r[j++] = *s++;
		r[j] = 0;
		if (i == m)
			x0 = cx;
		cx += ui_ancho_amp(r) + 16;
	}
	texto_menu(m, t, sizeof t, &g);
	menu_abierto = m;
	so_menu(C.v, x0, ui_alto_menus(), t, g, (ETIQ_MENU + (u32)m) | CON_FLECHAS);
	C.sucio = 1;
}

/* ===================================================================== */
/*  La geometría y lo de las barras                                        */
/* ===================================================================== */
static void coloca(void)
{
	int y = C.completa ? 0 : ui_alto_menus();

	if (C.ver_barras && !C.completa) {
		y_b1 = y;
		y += ui_alto_barra();
		y_b2 = y;
		y += ui_alto_barra();
	} else
		y_b1 = y_b2 = -1000;
	al_fx = lz_ui_alto() + 10;
	if (C.ver_formulas_barra && !C.completa) {
		y_fx = y;
		y += al_fx;
	} else
		y_fx = -1000;
	y_estado = C.l.al - (C.ver_estado && !C.completa ? ui_alto_estado() : 0);
	al_pie = lz_ui_alto() + 6;
	if (al_pie < UI_DESP)
		al_pie = UI_DESP;
	y_pie = y_estado - al_pie;
	y_hoja = y;
	al_hoja = y_pie - y_hoja;
	if (al_hoja < 40)
		al_hoja = 40;
	an_hoja = C.l.an - UI_DESP;
	vs_coloca(0, y_hoja, an_hoja, al_hoja);
	desp_v.x = an_hoja;
	desp_v.y = y_hoja;
	desp_v.an = UI_DESP;
	desp_v.al = al_hoja;
	an_pestanas = C.l.an * 55 / 100;
	desp_h.x = an_pestanas;
	desp_h.y = y_pie;
	desp_h.an = C.l.an - an_pestanas - UI_DESP;
	desp_h.al = al_pie;
	an_nombres = C.l.an < 700 ? 90 : 110;
	x_texto_fx = 4 + an_nombres + 6 + 3 * (al_fx - 4) + 8;
	if (C.l.an < 900)
		ui_boton_id(barra2, N2, O_LISTA_FUENTE)->ancho = 120;
	else
		ui_boton_id(barra2, N2, O_LISTA_FUENTE)->ancho = 160;
}

static const estilo *estilo_activo(void)
{
	hoja *h = cu_hoja();

	return lib_estilo(C.lib, hj_estilo(h, h->cur_f, h->cur_c));
}

static void estado_barras(void)
{
	const estilo *e = estilo_activo();
	hoja *h = cu_hoja();
	ui_boton *b;
	int ah = e->alin_h;

	ui_boton_id(barra2, N2, O_NEGRITA)->activo = e->negrita;
	ui_boton_id(barra2, N2, O_CURSIVA)->activo = e->cursiva;
	ui_boton_id(barra2, N2, O_SUBRAYADO)->activo = e->subrayado;
	ui_boton_id(barra2, N2, O_IZQ)->activo = ah == AH_IZQ;
	ui_boton_id(barra2, N2, O_CENTRO)->activo = ah == AH_CENTRO;
	ui_boton_id(barra2, N2, O_DER)->activo = ah == AH_DER;
	ui_boton_id(barra2, N2, O_COMBINAR_CENTRAR)->activo = hj_combinada(h, h->cur_f, h->cur_c) >= 0;
	ui_boton_id(barra2, N2, O_INMOVILIZAR)->activo = h->inm_f || h->inm_c;
	b = ui_boton_id(barra2, N2, O_LISTA_FUENTE);
	copia(b->texto, e->familia, sizeof b->texto);
	b = ui_boton_id(barra2, N2, O_LISTA_TAMANO);
	if (e->tam & 1)
		fmt(b->texto, sizeof b->texto, "%u,5", e->tam / 2);
	else
		fmt(b->texto, sizeof b->texto, "%u", e->tam / 2);
	ui_boton_id(barra2, N2, O_COLOR)->color = C.color_letra == CU_AUTO ? 0 : C.color_letra;
	ui_boton_id(barra2, N2, O_RELLENO)->color = C.color_relleno;
	ui_boton_id(barra2, N2, O_BORDES)->color = 0x101820;
	ui_boton_id(barra1, N1, O_DESHACER)->apagado = !dh_hay(C.lib, 0);
	ui_boton_id(barra1, N1, O_REHACER)->apagado = !dh_hay(C.lib, 1);
	ui_boton_id(barra1, N1, O_BROCHA)->activo = C.con_brocha;
	ui_boton_id(barra1, N1, O_AUTOFILTRO)->activo = h->hay_filtro;
	ui_boton_id(barra1, N1, O_PREVIA)->activo = C.previa;
	fmt(ui_boton_id(barra1, N1, O_LISTA_ZOOM)->texto, 64, "%d%%", h->zoom);
}

/* ---- la barra de fórmulas ---- */
static void texto_nombres(char *t, u32 cabe)
{
	hoja *h = cu_hoja();
	rango s = cu_seleccion();
	u32 i;

	if (C.arrastra == AR_SEL && (s.f0 != s.f1 || s.c0 != s.c1)) {
		/* (mientras se elige: cuántas filas y columnas, como Excel) */
		fmt(t, cabe, "%uF x %uC", s.f1 - s.f0 + 1, (u32)s.c1 - s.c0 + 1);
		return;
	}
	/* un nombre que sea justo lo elegido */
	for (i = 0; i < C.lib->n_nom; i++) {
		const formula *f = C.lib->nom[i].f;

		if (f && f->n == 1 && (f->t[0].t == T_REF || f->t[0].t == T_RANGO) && f->t[0].h == h->id &&
		    (u32)f->t[0].u.r.f0 == s.f0 && (u32)f->t[0].u.r.f1 == s.f1 && f->t[0].u.r.c0 == s.c0 && f->t[0].u.r.c1 == s.c1) {
			copia(t, C.lib->nom[i].nombre, cabe);
			return;
		}
	}
	cu_ref_texto(h->cur_f, h->cur_c, t);
	(void)cabe;
}

static void pinta_barra_formulas(lienzo *l)
{
	int y = y_fx, al = al_fx, x, k, lado = al - 4;
	char t[64];
	hoja *h = cu_hoja();

	lz_rect(l, 0, y, l->an, al, UI_FONDO);
	lz_raya_h(l, 0, y + al - 1, l->an, UI_RAYA);
	/* el cuadro de nombres, con su flecha */
	lz_rect(l, 4, y + 2, an_nombres, al - 4, UI_BLANCO);
	lz_marco(l, 4, y + 2, an_nombres, al - 4, UI_RAYA_FUERTE);
	texto_nombres(t, sizeof t);
	lz_recorte(l, 5, y + 2, an_nombres - lado - 2, al - 4);
	lz_ui(l, 9, y + 2 + (al - 4 - lz_ui_alto()) / 2 + lz_ui_base(), t, UI_TINTA);
	lz_sin_recorte(l);
	{
		int fx = 4 + an_nombres - lado, cx = fx + lado / 2, cy = y + al / 2;
		int xy[6] = { (cx - 4) * 16, (cy - 2) * 16, (cx + 4) * 16, (cy - 2) * 16, cx * 16, (cy + 3) * 16 };

		lz_raya_v(l, fx, y + 3, al - 6, UI_RAYA);
		lz_poligono(l, xy, 3, UI_TINTA);
	}
	/* ✗ ✓ fx */
	x = 4 + an_nombres + 6;
	for (k = 0; k < 3; k++) {
		int apagado = k < 2 && !C.editando, bx = x + k * lado;
		u32 c = apagado ? UI_TINTA_GRIS : k == 0 ? 0xB03A2E : k == 1 ? 0x1E7B34 : UI_TINTA;

		if (k == 0) {
			lz_linea(l, (bx + 6) * 16, (y + 7) * 16, (bx + lado - 6) * 16, (y + al - 7) * 16, 32, c);
			lz_linea(l, (bx + lado - 6) * 16, (y + 7) * 16, (bx + 6) * 16, (y + al - 7) * 16, 32, c);
		} else if (k == 1) {
			lz_linea(l, (bx + 5) * 16, (y + al / 2) * 16, (bx + lado / 2 - 1) * 16, (y + al - 7) * 16, 32, c);
			lz_linea(l, (bx + lado / 2 - 1) * 16, (y + al - 7) * 16, (bx + lado - 5) * 16, (y + 6) * 16, 32, c);
		} else
			ui_icono(l, IC_FX, bx + 2, y + 3, lado - 4, 0);
	}
	/* lo de la celda */
	x = x_texto_fx;
	lz_rect(l, x, y + 2, l->an - x - 4, al - 4, UI_BLANCO);
	lz_marco(l, x, y + 2, l->an - x - 4, al - 4, C.editando == 2 ? UI_ACENTO : UI_RAYA_FUERTE);
	if (C.editando == 2)
		lz_marco(l, x + 1, y + 3, l->an - x - 6, al - 6, UI_ACENTO);
	lz_recorte(l, x + 2, y + 2, l->an - x - 8, al - 4);
	{
		int base = y + 2 + (al - 4 - lz_ui_alto()) / 2 + lz_ui_base();

		if (C.editando) {
			char *s = utf8_de_u32(C.ed, C.ed_n), *p0 = utf8_de_u32(C.ed, C.ed_cur);
			int xc = x + 5 + (p0 ? lz_ui_ancho(p0) : 0), desp = 0;

			/* (que el cursor se vea) */
			if (xc > l->an - 20)
				desp = xc - (l->an - 20);
			if (C.ed_ancla != C.ed_cur) {
				u32 a = C.ed_ancla < C.ed_cur ? C.ed_ancla : C.ed_cur, b = C.ed_ancla < C.ed_cur ? C.ed_cur : C.ed_ancla;
				char *q0 = utf8_de_u32(C.ed, a), *q1 = utf8_de_u32(C.ed, b);

				if (q0 && q1)
					lz_rect(l, x + 5 + lz_ui_ancho(q0) - desp, y + 4, lz_ui_ancho(q1) - lz_ui_ancho(q0), al - 8, 0xB5CCF0);
				so_suelta(q0);
				so_suelta(q1);
			}
			if (s)
				lz_ui(l, x + 5 - desp, base, s, UI_TINTA);
			if (C.editando == 2)
				lz_rect(l, xc - desp, y + 4, 2, al - 8, UI_TINTA);
			so_suelta(s);
			so_suelta(p0);
		} else {
			char e[2000];

			lib_texto_edicion(C.lib, h, h->cur_f, h->cur_c, e, sizeof e);
			{
				/* (los saltos de línea, como «¶») */
				char *p;

				for (p = e; *p; p++)
					if (*p == '\n')
						*p = ' ';
			}
			lz_ui(l, x + 5, base, e, UI_TINTA);
		}
	}
	lz_sin_recorte(l);
}

/* ---- las pestañas de las hojas ---- */
static void pinta_pestanas(lienzo *l)
{
	int y = y_pie, al = al_pie, x, k, lado = al;
	u32 i;

	lz_rect(l, 0, y, an_pestanas, al, UI_FONDO);
	lz_raya_h(l, 0, y, an_pestanas, UI_RAYA_FUERTE);
	/* los cuatro botones: |◄ ◄ ► ►| */
	for (k = 0; k < 4; k++) {
		int bx = 2 + k * lado, cx = bx + lado / 2, cy = y + al / 2;
		int iz = k < 2;
		int xy[6] = { (cx + (iz ? 3 : -3)) * 16, (cy - 4) * 16, (cx + (iz ? 3 : -3)) * 16, (cy + 4) * 16, (cx + (iz ? -3 : 3)) * 16, cy * 16 };

		lz_poligono(l, xy, 3, UI_TINTA);
		if (k == 0)
			lz_rect(l, cx - 5, cy - 4, 2, 9, UI_TINTA);
		if (k == 3)
			lz_rect(l, cx + 4, cy - 4, 2, 9, UI_TINTA);
	}
	x = 4 + 4 * lado;
	n_pestanas_vistas = 0;
	if (tab0 >= (int)C.lib->n_h)
		tab0 = 0;
	for (i = 0; i <= C.lib->n_h; i++) {
		pestana_x[i] = pestana_an[i] = 0;
	}
	for (i = (u32)tab0; i <= C.lib->n_h; i++) {
		int act = i == C.lib->activa, w;
		const char *nom = i < C.lib->n_h ? C.lib->h[i]->nombre : "+";

		w = (act ? lz_ui_ancho_negrita(nom) : lz_ui_ancho(nom)) + 20;
		if (x + w > an_pestanas - 2)
			break;
		pestana_x[i] = x;
		pestana_an[i] = w;
		{
			/* un trapecio: la activa, blanca, sin raya arriba (pegada a la hoja) y en negrita */
			int xy[8] = { x * 16, (y + 1) * 16, (x + w) * 16, (y + 1) * 16, (x + w - 6) * 16, (y + al - 1) * 16, (x + 6) * 16, (y + al - 1) * 16 };
			int xi[8] = { (x + 1) * 16, (y + 1) * 16, (x + w - 1) * 16, (y + 1) * 16, (x + w - 7) * 16, (y + al - 2) * 16, (x + 7) * 16, (y + al - 2) * 16 };

			lz_poligono(l, xy, 4, 0x5A6678);
			lz_poligono(l, xi, 4, act ? UI_BLANCO : i == C.lib->n_h ? UI_FONDO2 : 0xD9DFE8);
			if (act)
				lz_rect(l, x + 1, y, w - 2, 2, UI_BLANCO);
		}
		if (act)
			lz_ui_negrita(l, x + 10, y + (al - lz_ui_alto()) / 2 + lz_ui_base(), nom, UI_TINTA);
		else
			lz_ui(l, x + 10, y + (al - lz_ui_alto()) / 2 + lz_ui_base(), nom, i == C.lib->n_h ? UI_ACENTO : UI_TINTA);
		x += w - 4;
		n_pestanas_vistas++;
	}
}

static int pestana_en(int x, int y)
{
	u32 i;

	if (y < y_pie || y >= y_pie + al_pie || x >= an_pestanas)
		return -10;
	if (x < 4 + 4 * al_pie)
		return -1 - (x - 2) / al_pie;	/* -1 .. -4: los botones */
	for (i = 0; i <= C.lib->n_h; i++)
		if (pestana_an[i] && x >= pestana_x[i] && x < pestana_x[i] + pestana_an[i] - 4)
			return (int)i;
	return -10;
}

/* ---- la barra de estado ---- */
static void texto_estado(char *t, u32 cabe)
{
	hoja *h = cu_hoja();
	rango s = cu_seleccion();
	const char *modo = C.editando ? (C.senala ? "Señalar" : C.ed_nuevo ? "Introducir" : "Modificar") : C.con_brocha ? "Copiar formato" : "Listo";
	char cuentas[200];

	cuentas[0] = 0;
	if (s.f0 != s.f1 || s.c0 != s.c1) {
		double suma;
		u32 nums, llenas;

		op_suma_rango(C.lib, h, s, &suma, &nums, &llenas);
		if (llenas > 1 || nums) {
			char a[64], b[64];

			num_general(suma, &C.lib->loc, 14, a, sizeof a);
			if (nums)
				num_general(suma / nums, &C.lib->loc, 14, b, sizeof b);
			else
				copia(b, "-", sizeof b);
			fmt(cuentas, sizeof cuentas, "Suma=%s  Promedio=%s  Recuento=%u", a, b, llenas);
		}
	}
	if (C.lib->circular) {
		char r[16];
		hoja *hc = lib_hoja_id(C.lib, (u16)((C.lib->circular - 1) >> 24));

		cu_ref_texto(CU_FILA((C.lib->circular - 1) & 0xFFFFFF), CU_COL(C.lib->circular - 1), r);
		fmt(t, cabe, "%s|Referencia circular: %s%s%s|%s|%s|%d%%", modo, hc && hc != h ? hc->nombre : "", hc && hc != h ? "!" : "", r,
		    C.ayuda_encima ? C.ayuda_encima : C.aviso, cuentas, h->zoom);
		return;
	}
	fmt(t, cabe, "%s|%s|%s|%d%%", modo, C.ayuda_encima ? C.ayuda_encima : C.aviso, cuentas, h->zoom);
}

/* ---- pintar ---- */
static u32 firma(u32 h, const void *d, u32 n)
{
	const u8 *p = d;
	u32 i;

	for (i = 0; i < n; i++)
		h = (h ^ p[i]) * 16777619u;
	return h;
}

static u32 firma_barra(const ui_boton *b, int n, int enc, int pul)
{
	u32 h = 2166136261u;
	int i;

	for (i = 0; i < n; i++) {
		h = firma(h, &b[i].activo, sizeof b[i].activo);
		h = firma(h, &b[i].apagado, sizeof b[i].apagado);
		h = firma(h, &b[i].color, sizeof b[i].color);
		h = firma(h, &b[i].ancho, sizeof b[i].ancho);
		h = firma(h, b[i].texto, largo(b[i].texto));
	}
	h = firma(h, &enc, sizeof enc);
	return firma(h, &pul, sizeof pul);
}

static u32 f_menus, f_b1, f_b2;
static int geo_antes = -1;

static void pinta_previa(lienzo *l);

void cu_pinta(void)
{
	char t[600];
	u32 t0 = so_tics(), f;
	int y0, todo;

	if (!C.l.p)
		return;
	coloca();
	estado_barras();
	todo = C.cuadro != NULL || geo_antes != (int)((u32)C.l.an ^ (u32)C.l.al << 12 ^ (u32)y_hoja << 24) || C.todo;
	geo_antes = (int)((u32)C.l.an ^ (u32)C.l.al << 12 ^ (u32)y_hoja << 24);
	C.todo = C.cuadro != NULL;
	y0 = C.l.al;
	if (!C.completa) {
		f = firma(firma(firma(2166136261u, &menu_abierto, sizeof menu_abierto), &encima_menu, sizeof encima_menu), &C.l.an, sizeof C.l.an);
		if (todo || f != f_menus) {
			ui_barra_menus(&C.l, 0, 0, C.l.an, MENUS, menu_abierto, encima_menu);
			lz_raya_h(&C.l, 0, ui_alto_menus() - 1, C.l.an, UI_RAYA);
			f_menus = f;
			y0 = 0;
		}
		if (C.ver_barras) {
			f = firma_barra(barra1, N1, encima1, pulsado1);
			if (todo || f != f_b1) {
				ui_barra(&C.l, 0, y_b1, C.l.an, barra1, N1, encima1, pulsado1);
				f_b1 = f;
				if (y_b1 < y0)
					y0 = y_b1;
			}
			f = firma_barra(barra2, N2, encima2, pulsado2);
			if (todo || f != f_b2) {
				ui_barra(&C.l, 0, y_b2, C.l.an, barra2, N2, encima2, pulsado2);
				f_b2 = f;
				if (y_b2 < y0)
					y0 = y_b2;
			}
		}
	}
	if (y_fx >= 0) {
		pinta_barra_formulas(&C.l);
		if (y_fx < y0)
			y0 = y_fx;
	}
	if (y_hoja < y0)
		y0 = y_hoja;
	if (todo)
		y0 = 0;
	if (C.previa)
		pinta_previa(&C.l);
	else
		vs_pinta(&C.l);
	{
		hoja *h = cu_hoja();
		u32 vf = vs_filas_vistas(), vc = vs_cols_vistas();
		u32 tot_f = (h->lim_f > h->cur_f + 1 ? h->lim_f : h->cur_f + 1) + vf, tot_c = (h->lim_c > h->cur_c + 1 ? h->lim_c : h->cur_c + 1) + vc;

		if (tot_f < h->vista_f + vf)
			tot_f = h->vista_f + vf;
		if (tot_c < h->vista_c + vc)
			tot_c = h->vista_c + vc;
		if (tot_f > CU_FILAS)
			tot_f = CU_FILAS;
		if (tot_c > CU_COLS)
			tot_c = CU_COLS;
		desp_v.total = (i32)(tot_f - h->inm_f);
		desp_v.visto = (i32)vf;
		desp_v.pos = (i32)(h->vista_f - h->inm_f);
		desp_h.total = (i32)(tot_c - h->inm_c);
		desp_h.visto = (i32)vc;
		desp_h.pos = (i32)(h->vista_c - h->inm_c);
		ui_desp_pinta(&C.l, &desp_v, C.arrastra == AR_DESP_V ? 5 : 0);
		ui_desp_pinta(&C.l, &desp_h, C.arrastra == AR_DESP_H ? 5 : 0);
		lz_rect(&C.l, an_hoja, y_pie, UI_DESP, al_pie, UI_FONDO);
	}
	pinta_pestanas(&C.l);
	if (C.ver_estado && !C.completa) {
		texto_estado(t, sizeof t);
		ui_estado(&C.l, 0, y_estado, C.l.an, t);
	}
	if (C.cuadro)
		ui_cuadro_pinta(&C.l, C.cuadro);
	so_mapa(C.v, 0, y0, C.l.an, C.l.al - y0, C.l.p + y0 * C.l.paso, C.l.paso);
	C.sucio = 0;
	tics_pintar = so_tics() - t0;
}

/* ===================================================================== */
/*  Abrir y guardar                                                       */
/* ===================================================================== */
static void pon_libro(libro *l, const char *ruta, int formato)
{
	if (C.lib) {
		dh_suelta(C.lib);
		lib_suelta(C.lib);
	}
	C.lib = l;
	copia(C.ruta, ruta ? ruta : "", sizeof C.ruta);
	C.formato = formato;
	C.grafico = -1;
	C.editando = 0;
	C.senala = 0;
	tab0 = 0;
	{
		hoja *h = cu_hoja();

		C.ancla_f = h->cur_f;
		C.ancla_c = h->cur_c;
		if (!h->zoom)
			h->zoom = 100;
	}
	cu_titulo();
	C.sucio = C.todo = 1;
}

int cu_abre(const char *ruta)
{
	char e[400];
	int f;
	libro *l = arch_abre(ruta, &f, e, sizeof e);

	if (!l) {
		dlg_aviso("No se puede abrir", e);
		return 1;
	}
	pon_libro(l, ruta, f);
	{
		char *p = strrchr(C.carpeta, '\\');

		(void)p;
	}
	cu_aviso("%s: %u hojas (%s)", sin_carpeta(ruta), l->n_h, arch_nombre_formato(f));
	dice("cuentas: abierto %s", sin_carpeta(ruta));
	return 0;
}

int cu_guarda(const char *ruta, int formato)
{
	char e[400], real[600];

	if (formato == FMT_CSV && C.lib->n_h > 1)
		cu_aviso("Un .CSV guarda solo la hoja que se ve (%s)", cu_hoja()->nombre);
	if (arch_guarda(C.lib, ruta, formato, real, sizeof real, e, sizeof e)) {
		dlg_aviso("No se puede guardar", e);
		return 1;
	}
	if (formato == FMT_PDF) {
		cu_aviso("Exportado a PDF: %s", sin_carpeta(real));
		dice("cuentas: pdf %s", real);
		return 0;
	}
	copia(C.ruta, real, sizeof C.ruta);
	C.formato = formato;
	cu_titulo();
	if (!iguales_sin_caja(sin_carpeta(real), sin_carpeta(ruta)) && !so_nombres_largos())
		cu_aviso("Guardado como %s (el sistema aún guarda los nombres en 8+3; se abre igual)", sin_carpeta(real));
	else
		cu_aviso("Guardado: %s", sin_carpeta(real));
	dice("cuentas: guardado %s", real);
	C.sucio = 1;
	return 0;
}

/* ===================================================================== */
/*  Corregir una celda                                                    */
/* ===================================================================== */
static void ed_pon(const char *t)
{
	u32 n = 0, i = 0, k = largo(t);

	while (i < k && n < 4095) {
		u32 c;

		i += (u32)utf8_lee((const u8 *)t + i, k - i, &c);
		C.ed[n++] = c;
	}
	C.ed_n = n;
	C.ed_cur = C.ed_ancla = n;
}

static void ed_empieza(const char *t, int nuevo, int donde)
{
	hoja *h = cu_hoja();

	C.editando = donde;
	C.ed_nuevo = nuevo;
	C.ed_f = h->cur_f;
	C.ed_c = h->cur_c;
	C.ed_hoja = h->id;
	C.senala = 0;
	ed_pon(t);
	C.grafico = -1;
	cu_ve_celda(h->cur_f, h->cur_c);
	C.sucio = 1;
}

static void ed_cancela(void)
{
	int i = lib_hoja_indice(C.lib, C.ed_hoja);

	C.editando = 0;
	C.senala = 0;
	if (i >= 0)
		C.lib->activa = (u32)i;
	C.sucio = C.todo = 1;
}

/* acaba de corregir y lo pone; 0 si va bien (si no, se sigue corrigiendo) */
static int ed_acepta(int en_todo)
{
	char *t = utf8_de_u32(C.ed, C.ed_n), e[300];
	hoja *h = lib_hoja_id(C.lib, C.ed_hoja);
	int r;

	if (!t || !h) {
		so_suelta(t);
		ed_cancela();
		return 0;
	}
	/* (una fórmula sin cerrar sus paréntesis: se cierran, como Excel) */
	if (t[0] == '=') {
		int abiertos = 0, com = 0;
		char *p;

		for (p = t; *p; p++) {
			if (*p == '"')
				com = !com;
			else if (!com && *p == '(')
				abiertos++;
			else if (!com && *p == ')')
				abiertos--;
		}
		if (abiertos > 0 && abiertos < 30 && !com) {
			u32 n = largo(t);
			char *q = so_repide(t, n + (u32)abiertos + 1);

			if (q) {
				t = q;
				while (abiertos-- > 0)
					t[n++] = ')';
				t[n] = 0;
			}
		}
	}
	if (en_todo) {
		rango s = cu_seleccion();

		r = op_entrada_rango(C.lib, h, s, t, e, sizeof e);
	} else
		r = op_entrada(C.lib, h, C.ed_f, C.ed_c, t, e, sizeof e);
	so_suelta(t);
	if (r) {
		char m[400];

		fmt(m, sizeof m, "La fórmula tiene un error:\n%s", e);
		dlg_aviso("Cuentas", m);
		C.senala = 0;
		return 1;
	}
	C.editando = 0;
	C.senala = 0;
	/* (si se señaló en otra hoja, a la de la celda) */
	{
		int i = lib_hoja_indice(C.lib, C.ed_hoja);

		if (i >= 0 && (u32)i != C.lib->activa) {
			C.lib->activa = (u32)i;
			C.todo = 1;
		}
	}
	cu_cambio();
	return 0;
}

static void ed_borra_eleccion(void)
{
	u32 a = C.ed_ancla < C.ed_cur ? C.ed_ancla : C.ed_cur, b = C.ed_ancla < C.ed_cur ? C.ed_cur : C.ed_ancla;

	if (a == b)
		return;
	memmove(C.ed + a, C.ed + b, (C.ed_n - b) * sizeof(u32));
	C.ed_n -= b - a;
	C.ed_cur = C.ed_ancla = a;
}

static void ed_mete(const u32 *s, u32 n)
{
	ed_borra_eleccion();
	if (C.ed_n + n >= 4095)
		n = 4095 - C.ed_n;
	memmove(C.ed + C.ed_cur + n, C.ed + C.ed_cur, (C.ed_n - C.ed_cur) * sizeof(u32));
	memcpy(C.ed + C.ed_cur, s, n * sizeof(u32));
	C.ed_n += n;
	C.ed_cur += n;
	C.ed_ancla = C.ed_cur;
}

static void ed_mete_txt(const char *t)
{
	u32 n, *w = u32_de_utf8(t, largo(t), &n);

	if (w) {
		ed_mete(w, n);
		so_suelta(w);
	}
}

/* ¿se puede señalar aquí? (escribiendo una fórmula, detrás de un signo) */
static int puede_senalar(void)
{
	u32 c;

	if (!C.editando || C.ed_n == 0 || C.ed[0] != '=')
		return 0;
	if (C.senala)
		return 1;
	if (C.ed_cur != C.ed_n && !C.ed_nuevo)
		return 0;
	if (C.ed_cur == 0)
		return 0;
	c = C.ed[C.ed_cur - 1];
	return c == '=' || c == '+' || c == '-' || c == '*' || c == '/' || c == '^' || c == '&' || c == '(' || c == ';' || c == ',' ||
	       c == '<' || c == '>' || c == ':';
}

/* pone (o cambia) la referencia señalada en lo que se escribe */
static void senala_a(u32 f, u32 c, int ampliar)
{
	char r[40], a[16], b[16];
	hoja *h = cu_hoja();
	u32 n, *w;

	if (!C.senala) {
		C.senala = 1;
		C.sen_ini = C.sen_fin = C.ed_cur;
		C.sen_f0 = f;
		C.sen_c0 = c;
	}
	if (!ampliar) {
		C.sen_f0 = f;
		C.sen_c0 = c;
	}
	C.sen_f = f;
	C.sen_c = c;
	cu_ref_texto(C.sen_f0, C.sen_c0, a);
	cu_ref_texto(f, c, b);
	if (C.sen_f0 == f && C.sen_c0 == c)
		copia(r, a, sizeof r);
	else {
		rango g = cu_rango(C.sen_f0, C.sen_c0, f, c);

		cu_rango_texto(g, r);
	}
	/* (de otra hoja: con su nombre) */
	if (h->id != C.ed_hoja) {
		char t[160];

		fmt(t, sizeof t, "%s!%s", h->nombre, r);
		{
			int raro = 0;
			const char *p;

			for (p = h->nombre; *p; p++)
				if (*p == ' ' || *p == '-' || *p == '(')
					raro = 1;
			if (raro)
				fmt(t, sizeof t, "'%s'!%s", h->nombre, r);
		}
		copia(r, t, sizeof r);
	}
	/* fuera lo de antes, dentro lo nuevo */
	if (C.sen_fin > C.ed_n)
		C.sen_fin = C.ed_n;
	memmove(C.ed + C.sen_ini, C.ed + C.sen_fin, (C.ed_n - C.sen_fin) * sizeof(u32));
	C.ed_n -= C.sen_fin - C.sen_ini;
	C.ed_cur = C.ed_ancla = C.sen_ini;
	w = u32_de_utf8(r, largo(r), &n);
	if (w) {
		ed_mete(w, n);
		so_suelta(w);
	}
	C.sen_fin = C.ed_cur;
	(void)b;
	cu_ve_celda(f, c);
	C.sucio = 1;
}

/* F4: A1 -> $A$1 -> A$1 -> $A1 -> A1, la referencia del cursor */
static void ed_f4(void)
{
	u32 a = C.ed_cur, b;
	char t[24];
	u32 k = 0, i;
	int fc = 0, ff = 0;
	u32 letras = 0, cifras = 0;

	while (a > 0 && (C.ed[a - 1] == '$' || (C.ed[a - 1] >= 'A' && C.ed[a - 1] <= 'Z') || (C.ed[a - 1] >= 'a' && C.ed[a - 1] <= 'z') ||
			 (C.ed[a - 1] >= '0' && C.ed[a - 1] <= '9')))
		a--;
	b = a;
	while (b < C.ed_n && (C.ed[b] == '$' || (C.ed[b] >= 'A' && C.ed[b] <= 'Z') || (C.ed[b] >= 'a' && C.ed[b] <= 'z') || (C.ed[b] >= '0' && C.ed[b] <= '9')))
		b++;
	if (b - a < 2 || b - a > 12)
		return;
	for (i = a; i < b; i++) {
		if (C.ed[i] == '$') {
			if (letras == 0)
				fc = 1;
			else
				ff = 1;
			continue;
		}
		if (C.ed[i] >= '0' && C.ed[i] <= '9')
			cifras++;
		else {
			if (cifras)
				return;
			letras++;
		}
	}
	if (!letras || !cifras || letras > 3)
		return;
	/* el siguiente */
	if (!fc && !ff)
		fc = ff = 1;
	else if (fc && ff)
		fc = 0;
	else if (!fc && ff)
		fc = 1, ff = 0;
	else
		fc = ff = 0;
	if (fc)
		t[k++] = '$';
	for (i = a; i < b; i++)
		if (C.ed[i] != '$' && !(C.ed[i] >= '0' && C.ed[i] <= '9'))
			t[k++] = (char)(C.ed[i] >= 'a' ? C.ed[i] - 32 : C.ed[i]);
	if (ff)
		t[k++] = '$';
	for (i = a; i < b; i++)
		if (C.ed[i] >= '0' && C.ed[i] <= '9')
			t[k++] = (char)C.ed[i];
	t[k] = 0;
	C.ed_cur = a;
	C.ed_ancla = b;
	ed_mete_txt(t);
	C.sucio = 1;
}

/* ===================================================================== */
/*  Moverse                                                               */
/* ===================================================================== */
static int con_algo(hoja *h, u32 f, u32 c)
{
	const valor *v = lib_valor(h, f, c);

	return v->t != V_VACIO;
}

/* Ctrl+flecha: al borde de los datos */
static void borde_datos(int df, int dc, u32 *f, u32 *c)
{
	hoja *h = cu_hoja();
	i64 x = *f, y = *c;
	int ahora = con_algo(h, *f, *c), sig;

	if ((df < 0 && x == 0) || (dc < 0 && y == 0))
		return;
	sig = con_algo(h, (u32)(x + df), (u32)(y + dc));
	if (ahora && sig) {
		/* hasta la última llena */
		while (x + df >= 0 && y + dc >= 0 && x + df < CU_FILAS && y + dc < CU_COLS && con_algo(h, (u32)(x + df), (u32)(y + dc))) {
			x += df;
			y += dc;
		}
	} else {
		/* hasta la siguiente llena (o el final) */
		x += df;
		y += dc;
		while (x >= 0 && y >= 0 && x < CU_FILAS && y < CU_COLS && !con_algo(h, (u32)x, (u32)y)) {
			if ((df > 0 && x >= h->lim_f) || (dc > 0 && y >= h->lim_c)) {
				x = df > 0 ? CU_FILAS - 1 : x;
				y = dc > 0 ? CU_COLS - 1 : y;
				break;
			}
			x += df;
			y += dc;
		}
		if (x < 0)
			x = 0;
		if (y < 0)
			y = 0;
		if (x >= CU_FILAS)
			x = CU_FILAS - 1;
		if (y >= CU_COLS)
			y = CU_COLS - 1;
	}
	*f = (u32)x;
	*c = (u32)y;
}

/* la celda de al lado (saltando las ocultas y las combinadas) */
static void al_lado(int df, int dc, u32 *f, u32 *c)
{
	hoja *h = cu_hoja();
	i64 x = *f, y = *c;
	int i = hj_combinada(h, *f, *c);

	if (i >= 0) {
		if (df > 0)
			x = h->comb[i].f1;
		if (dc > 0)
			y = h->comb[i].c1;
		if (df < 0)
			x = h->comb[i].f0;
		if (dc < 0)
			y = h->comb[i].c0;
	}
	do {
		x += df;
		y += dc;
	} while (x >= 0 && y >= 0 && x < CU_FILAS && y < CU_COLS && ((df && hj_alto(h, (u32)x) == 0) || (dc && hj_ancho(h, (u32)y) == 0)));
	if (x < 0 || y < 0 || x >= CU_FILAS || y >= CU_COLS)
		return;
	*f = (u32)x;
	*c = (u32)y;
}

static void mueve(int df, int dc, int ctrl, int mayus)
{
	hoja *h = cu_hoja();
	/* (con Mayús se mueve el extremo de lo elegido, no la activa) */
	u32 f = mayus ? (h->sel.f0 == C.ancla_f ? h->sel.f1 : h->sel.f0) : h->cur_f;
	u32 c = mayus ? (h->sel.c0 == C.ancla_c ? h->sel.c1 : h->sel.c0) : h->cur_c;

	if (!mayus) {
		C.ancla_f = h->cur_f;
		C.ancla_c = h->cur_c;
	}
	if (ctrl)
		borde_datos(df, dc, &f, &c);
	else
		al_lado(df, dc, &f, &c);
	cu_elige(f, c, mayus);
}

/* Intro: abajo (y, tras escribir con Tab, a la columna del primer Tab) */
static void intro_abajo(void)
{
	hoja *h = cu_hoja();

	if (C.tab_col >= 0) {
		u32 f = h->cur_f, c = h->cur_c;

		al_lado(1, 0, &f, &c);
		cu_elige(f, (u32)C.tab_col, 0);
		C.tab_col = -1;
		return;
	}
	mueve(1, 0, 0, 0);
}

static void a_pagina(int dir, int mayus)
{
	hoja *h = cu_hoja();
	u32 n = vs_filas_vistas(), f = h->cur_f;

	if (n > 1)
		n--;
	if (dir > 0) {
		f = f + n < CU_FILAS ? f + n : CU_FILAS - 1;
		h->vista_f = h->vista_f + n < CU_FILAS ? h->vista_f + n : CU_FILAS - 1;
	} else {
		f = f > n ? f - n : 0;
		h->vista_f = h->vista_f > n + h->inm_f ? h->vista_f - n : h->inm_f;
	}
	cu_elige(f, h->cur_c, mayus);
}

/* ===================================================================== */
/*  Los formatos                                                          */
/* ===================================================================== */
static void cb_negrita(libro *l, estilo *e, const void *u) { (void)l; e->negrita = (u8)*(const int *)u; }
static void cb_cursiva(libro *l, estilo *e, const void *u) { (void)l; e->cursiva = (u8)*(const int *)u; }
static void cb_subrayado(libro *l, estilo *e, const void *u) { (void)l; e->subrayado = (u8)*(const int *)u; }
static void cb_alin(libro *l, estilo *e, const void *u) { (void)l; e->alin_h = (u8)*(const int *)u; }
static void cb_fmt(libro *l, estilo *e, const void *u) { e->numfmt = lib_fmt_id(l, (const char *)u); }
static void cb_color(libro *l, estilo *e, const void *u) { (void)l; e->color = *(const u32 *)u; }
static void cb_fuente(libro *l, estilo *e, const void *u) { (void)l; copia(e->familia, (const char *)u, sizeof e->familia); }
static void cb_tam(libro *l, estilo *e, const void *u) { (void)l; e->tam = (u16)*(const int *)u; }
static void cb_brocha(libro *l, estilo *e, const void *u) { *e = *lib_estilo(l, *(const u16 *)u); }
static void cb_relleno(libro *l, estilo *e, const void *u)
{
	const u32 *v = u;

	(void)l;
	e->trama = (u8)v[0];
	e->fondo = v[1];
	e->color_trama = v[2];
}
static void cb_decimales(libro *l, estilo *e, const void *u)
{
	char c[200];
	const char *f = lib_fmt(l, e->numfmt);

	if (iguales(f, "General"))
		f = *(const int *)u > 0 ? "0" : "0";
	fmt_cambia_decimales(f, *(const int *)u, c, sizeof c);
	if (iguales(lib_fmt(l, e->numfmt), "General") && *(const int *)u > 0)
		fmt_cambia_decimales("0", 1, c, sizeof c);
	e->numfmt = lib_fmt_id(l, c);
}

static void formato(op_cambia_estilo f, const void *u, const char *nombre)
{
	op_formato(C.lib, cu_hoja(), cu_seleccion(), f, u, nombre);
	cu_cambio();
}

static void bordes(int que)
{
	hoja *h = cu_hoja();
	rango s = cu_seleccion();
	u32 f, c;

	if ((u64)(s.f1 - s.f0 + 1) * (s.c1 - s.c0 + 1) > 2000000) {
		cu_aviso("Demasiadas celdas para ponerles bordes de una vez");
		return;
	}
	dh_empieza(C.lib, "Bordes");
	dh_region(C.lib, h, s);
	for (f = s.f0; f <= s.f1; f++)
		for (c = s.c0; c <= s.c1; c++) {
			estilo e = *lib_estilo(C.lib, hj_estilo(h, f, c));
			int k;

			switch (que) {
			case 0: /* todos */
				for (k = 0; k < 4; k++)
					e.borde[k] = BR_FINO;
				break;
			case 1: /* exterior */
			case 2:
				if (f == s.f0)
					e.borde[BO_ARRIBA] = que == 2 ? BR_GRUESO : BR_FINO;
				if (f == s.f1)
					e.borde[BO_ABAJO] = que == 2 ? BR_GRUESO : BR_FINO;
				if (c == s.c0)
					e.borde[BO_IZQ] = que == 2 ? BR_GRUESO : BR_FINO;
				if (c == s.c1)
					e.borde[BO_DER] = que == 2 ? BR_GRUESO : BR_FINO;
				break;
			case 3:
				if (f == s.f1)
					e.borde[BO_ABAJO] = BR_FINO;
				break;
			case 4:
				if (f == s.f1)
					e.borde[BO_ABAJO] = BR_DOBLE;
				break;
			case 5:
				if (f == s.f0)
					e.borde[BO_ARRIBA] = BR_FINO;
				break;
			case 6:
				if (c == s.c0)
					e.borde[BO_IZQ] = BR_FINO;
				break;
			case 7:
				if (c == s.c1)
					e.borde[BO_DER] = BR_FINO;
				break;
			default:
				for (k = 0; k < 4; k++)
					e.borde[k] = BR_NADA;
			}
			lib_pon_estilo(C.lib, h, f, c, lib_estilo_id(C.lib, &e));
		}
	dh_acaba(C.lib);
	lib_cambio(C.lib);
	C.borde_elegido = que;
	cu_cambio();
}

/* ---- la autosuma ---- */
static void autosuma(void)
{
	hoja *h = cu_hoja();
	rango s = cu_seleccion();

	if (s.f0 == s.f1 && s.c0 == s.c1) {
		/* encima, o a la izquierda: los números seguidos */
		u32 f = h->cur_f, c = h->cur_c, a;
		char t[64], r[24];

		if (f > 0 && lib_valor(h, f - 1, c)->t == V_NUM) {
			a = f - 1;
			while (a > 0 && (lib_valor(h, a - 1, c)->t == V_NUM || hj_celda(h, a - 1, c)))
				a--;
			while (a < f && lib_valor(h, a, c)->t != V_NUM)
				a++;
			cu_rango_texto(cu_rango(a, c, f - 1, c), r);
		} else if (c > 0 && lib_valor(h, f, c - 1)->t == V_NUM) {
			a = c - 1;
			while (a > 0 && lib_valor(h, f, a - 1)->t == V_NUM)
				a--;
			cu_rango_texto(cu_rango(f, a, f, c - 1), r);
		} else
			r[0] = 0;
		fmt(t, sizeof t, "=%s(%s)", C.lib->loc.ingles ? "SUM" : "SUMA", r);
		ed_empieza(t, 1, 1);
		if (!r[0])
			C.ed_cur = C.ed_ancla = C.ed_n - 1;
		return;
	}
	{
		/* de lo elegido: la suma debajo de cada columna (o, con una fila, a su derecha) */
		u32 c, f;
		char e[200];

		dh_empieza(C.lib, "Autosuma");
		if (s.f0 == s.f1) {
			char t[80], r[24];

			if (s.c1 + 1 < CU_COLS) {
				cu_rango_texto(s, r);
				fmt(t, sizeof t, "=SUMA(%s)", r);
				dh_celda(C.lib, h, s.f0, s.c1 + 1);
				lib_pon_entrada(C.lib, h, s.f0, s.c1 + 1, t, e, sizeof e);
			}
		} else
			for (c = s.c0; c <= s.c1; c++) {
				char t[80], r[24];

				f = s.f1;
				/* (si la última fila está vacía, ahí va la suma) */
				if (lib_valor(h, s.f1, c)->t == V_VACIO && s.f1 > s.f0) {
					cu_rango_texto(cu_rango(s.f0, c, s.f1 - 1, c), r);
				} else {
					f = s.f1 + 1;
					cu_rango_texto(cu_rango(s.f0, c, s.f1, c), r);
				}
				if (f >= CU_FILAS)
					continue;
				fmt(t, sizeof t, "=SUMA(%s)", r);
				dh_celda(C.lib, h, f, c);
				lib_pon_entrada(C.lib, h, f, c, t, e, sizeof e);
			}
		dh_acaba(C.lib);
		calc_recalcula(C.lib);
		cu_cambio();
	}
}

/* la región de datos alrededor de la celda activa (para ordenar y los gráficos) */
static rango region_actual(void)
{
	hoja *h = cu_hoja();
	rango r = cu_rango(h->cur_f, h->cur_c, h->cur_f, h->cur_c);
	int crece = 1;

	while (crece) {
		u32 k;

		crece = 0;
		if (r.f0 > 0)
			for (k = r.c0 ? r.c0 - 1 : 0; k <= (u32)r.c1 + 1 && k < CU_COLS; k++)
				if (con_algo(h, r.f0 - 1, k)) {
					r.f0--;
					crece = 1;
					break;
				}
		if (r.f1 + 1 < CU_FILAS)
			for (k = r.c0 ? r.c0 - 1 : 0; k <= (u32)r.c1 + 1 && k < CU_COLS; k++)
				if (con_algo(h, r.f1 + 1, k)) {
					r.f1++;
					crece = 1;
					break;
				}
		if (r.c0 > 0)
			for (k = r.f0 ? r.f0 - 1 : 0; k <= r.f1 + 1 && k < CU_FILAS; k++)
				if (con_algo(h, k, r.c0 - 1u)) {
					r.c0--;
					crece = 1;
					break;
				}
		if (r.c1 + 1 < CU_COLS)
			for (k = r.f0 ? r.f0 - 1 : 0; k <= r.f1 + 1 && k < CU_FILAS; k++)
				if (con_algo(h, k, r.c1 + 1u)) {
					r.c1++;
					crece = 1;
					break;
				}
		if ((r.f1 - r.f0) > 60000 || (r.c1 - r.c0) > 250)
			break;
	}
	return r;
}

/* ¿la primera fila son títulos? (textos encima de algún número) */
static int tiene_titulos(rango r)
{
	hoja *h = cu_hoja();
	u32 c;
	int tit = 0;

	if (r.f0 == r.f1)
		return 0;
	for (c = r.c0; c <= r.c1; c++) {
		const valor *a = lib_valor(h, r.f0, c), *b = lib_valor(h, r.f0 + 1, c);

		if (a->t != V_TXT && a->t != V_VACIO)
			return 0;	/* (un número arriba: no son títulos) */
		if (a->t == V_TXT && b->t != V_TXT && b->t != V_VACIO)
			tit = 1;
		if (a->t == V_TXT && lib_estilo(C.lib, hj_estilo(h, r.f0, c))->negrita != lib_estilo(C.lib, hj_estilo(h, r.f0 + 1, c))->negrita)
			tit = 1;
	}
	return tit;
}

static void ordena_rapido(int desc)
{
	hoja *h = cu_hoja();
	rango s = cu_seleccion();
	int col[3] = { (int)h->cur_c, -1, -1 }, d[3] = { desc, 0, 0 };

	if (s.f0 == s.f1 && s.c0 == s.c1)
		s = region_actual();
	op_ordena(C.lib, h, s, col, d, tiene_titulos(s));
	cu_aviso("Ordenado de %s por la columna %c", desc ? "mayor a menor" : "menor a mayor", 'A' + (int)(h->cur_c % 26));
	cu_cambio();
}

/* ---- el portapapeles ---- */
static char *ultimo_copiado;

static void copia_o_corta(int cortar)
{
	u32 n;
	char *t;

	C.grafico = -1;
	t = op_copia(C.lib, cu_hoja(), cu_seleccion(), cortar, &n);
	if (t) {
		so_al_portapapeles(t, n);
		so_suelta(ultimo_copiado);
		ultimo_copiado = t;
	}
	cu_aviso(cortar ? "Cortado: elija dónde pegarlo y pulse Ctrl+V" : "Copiado: elija dónde pegarlo y pulse Ctrl+V (Pegado especial, en Edición)");
	C.sucio = 1;
}

static void pega(int modo, int transponer)
{
	hoja *h = cu_hoja();
	u32 n;
	char *t = so_del_portapapeles(&n), e[300];
	rango r;

	/* lo del sistema, si no es lo que se copió aquí */
	if (t && n && (!op_hay_recorte() || !ultimo_copiado || !iguales(t, ultimo_copiado)) && modo == PG_TODO) {
		op_pega_texto(C.lib, h, h->cur_f, h->cur_c, t, n, &r);
		so_suelta(t);
		cu_sel_rango(r);
		cu_cambio();
		return;
	}
	so_suelta(t);
	if (op_pega(C.lib, h, h->cur_f, h->cur_c, modo, transponer, &r, e, sizeof e)) {
		dlg_aviso("No se puede pegar", e);
		return;
	}
	cu_sel_rango(r);
	cu_cambio();
}

/* ---- la vista preliminar: la página en pequeño ---- */
static void pinta_previa(lienzo *l)
{
	pintor p;
	pdf_plan pl;
	hoja *h = cu_hoja();
	double e;
	int x, y, w, hh, n;
	char t[96];

	lz_rect(l, C.g.x, C.g.y, C.g.an, C.g.al, 0x7A8699);
	if (pdf_plan_hoja(C.lib, h, &pl) || pl.n == 0) {
		if (!pl.n)
			pdf_plan_suelta(&pl);
		lz_ui_negrita(l, C.g.x + 20, C.g.y + 30, "No hay nada que imprimir en esta hoja (Esc para volver)", 0xFFFFFF);
		return;
	}
	n = (int)pl.n;
	if (C.pag_previa >= n)
		C.pag_previa = n - 1;
	if (C.pag_previa < 0)
		C.pag_previa = 0;
	e = (C.g.al - 44) / pl.al;
	if ((C.g.an - 40) / pl.an < e)
		e = (C.g.an - 40) / pl.an;
	w = (int)(pl.an * e);
	hh = (int)(pl.al * e);
	x = C.g.x + (C.g.an - w) / 2;
	y = C.g.y + 26;
	lz_rect(l, x + 4, y + 4, w, hh, 0x3A4454);
	lz_rect(l, x, y, w, hh, 0xFFFFFF);
	pt_de_lienzo(&p, l);
	pt_recorte(&p, x, y, w, hh);
	pdf_pinta_pagina(&p, C.lib, h, &pl, (u32)C.pag_previa, x, y, e, C.ruta[0] ? sin_carpeta(C.ruta) : "Libro1", 0, pl.n);
	pt_vuelve(&p);
	pdf_plan_suelta(&pl);
	fmt(t, sizeof t, "Vista preliminar: página %d de %d (Av Pág, Re Pág; Esc para volver)", C.pag_previa + 1, n);
	lz_ui_negrita(l, C.g.x + 10, C.g.y + 6 + lz_ui_base(), t, 0xFFFFFF);
}

/* ===================================================================== */
/*  Las órdenes                                                           */
/* ===================================================================== */
static void hacer_guardar_y(int orden)
{
	if (C.lib->cambiado) {
		dlg_guardar_cambios(orden);
		return;
	}
	cu_orden(orden);
}

void cu_orden(int o)
{
	hoja *h = cu_hoja();
	rango s = cu_seleccion();

	if (!activa(o)) {
		cu_aviso("Eso aún no lo hace Cuentas");
		return;
	}
	if (C.editando && o != O_FUNCION && o != O_AUTOSUMA) {
		if (ed_acepta(0))
			return;
	}
	switch (o) {
	case O_NUEVO:
	case O_CERRAR:
		if (C.lib->cambiado && !C.pendiente) {
			dlg_guardar_cambios(o);
			return;
		}
		C.pendiente = 0;
		pon_libro(lib_nuevo(3), NULL, FMT_HOJ);
		cu_aviso("Un libro nuevo");
		break;
	case O_ABRIR:
		if (C.lib->cambiado && !C.pendiente) {
			dlg_guardar_cambios(o);
			return;
		}
		C.pendiente = 0;
		dlg_abrir(0);
		break;
	case O_GUARDAR:
		if (!C.ruta[0] || C.formato == FMT_CSV)
			dlg_abrir(1);
		else
			cu_guarda(C.ruta, C.formato ? C.formato : FMT_HOJ);
		break;
	case O_GUARDAR_COMO: dlg_abrir(1); break;
	case O_EXPORTAR_PDF: dlg_abrir(2); break;
	case O_IMPRIMIR:
#ifdef OFI_HISPA64
		dlg_imprimir();		/* [Hispa-64, etapa 11 a] por IPP o a PDF */
#else
		dlg_aviso("Imprimir", "Aún no hay impresoras en Hispa-Pentium.\nExporte la hoja a PDF (Archivo > Exportar a PDF)\ny llévela a otro equipo para imprimirla.");
#endif
		break;
	case O_PREPARAR: dlg_preparar(); break;
	case O_AREA:
		op_area(C.lib, h, &s);
		cu_aviso("Área de impresión: %s", "lo elegido");
		break;
	case O_QUITAR_AREA: op_area(C.lib, h, NULL); break;
	case O_PREVIA:
		C.previa = !C.previa;
		C.pag_previa = 0;
		C.todo = 1;
		break;
	case O_PROPIEDADES: dlg_acerca(); break;
	case O_SALIR:
		if (C.lib->cambiado && !C.pendiente) {
			dlg_guardar_cambios(o);
			return;
		}
		C.sale = 1;
		break;
	case O_DESHACER:
		if (dh_deshace(C.lib)) {
			cu_aviso("Deshecho");
			h = cu_hoja();
			C.ancla_f = h->cur_f;
			C.ancla_c = h->cur_c;
			cu_ve_celda(h->cur_f, h->cur_c);
		}
		break;
	case O_REHACER:
		if (dh_rehace(C.lib)) {
			cu_aviso("Rehecho");
			h = cu_hoja();
			cu_ve_celda(h->cur_f, h->cur_c);
		}
		break;
	case O_CORTAR: copia_o_corta(1); break;
	case O_COPIAR: copia_o_corta(0); break;
	case O_PEGAR: pega(PG_TODO, 0); break;
	case O_PEGADO_ESPECIAL: dlg_pegado_especial(); break;
	case O_RELLENAR_ABAJO:
		if (s.f1 > s.f0)
			op_rellena(C.lib, h, cu_rango(s.f0, s.c0, s.f0, s.c1), s);
		break;
	case O_RELLENAR_DERECHA:
		if (s.c1 > s.c0)
			op_rellena(C.lib, h, cu_rango(s.f0, s.c0, s.f1, s.c0), s);
		break;
	case O_SERIES: dlg_series(); break;
	case O_BORRAR_TODO: op_borra(C.lib, h, s, BR_TODO); break;
	case O_BORRAR_FORMATOS: op_borra(C.lib, h, s, BR_FORMATO); break;
	case O_BORRAR_CONTENIDO: op_borra(C.lib, h, s, BR_CONTENIDO); break;
	case O_BORRAR_COMENTARIOS: op_borra(C.lib, h, s, BR_COMENTARIOS); break;
	case O_ELIMINAR: dlg_eliminar(0); break;
	case O_ELIMINAR_HOJA:
		op_hoja_quita(C.lib, C.lib->activa);
		C.grafico = -1;
		break;
	case O_MOVER_HOJA: dlg_mover_hoja(); break;
	case O_BUSCAR: dlg_buscar(0); break;
	case O_REEMPLAZAR: dlg_buscar(1); break;
	case O_IR_A: dlg_ir_a(); break;
	case O_TODO:
		C.ancla_f = 0;
		C.ancla_c = 0;
		h->sel = cu_rango(0, 0, CU_FILAS - 1, CU_COLS - 1);
		break;
	case O_VER_BARRA_FORMULAS: C.ver_formulas_barra = !C.ver_formulas_barra; C.todo = 1; break;
	case O_VER_ESTADO: C.ver_estado = !C.ver_estado; C.todo = 1; break;
	case O_VER_BARRAS: C.ver_barras = !C.ver_barras; C.todo = 1; break;
	case O_COMPLETA: C.completa = !C.completa; C.todo = 1; break;
	case O_ZOOM: dlg_zoom(); break;
	case O_VER_FORMULAS: C.ver_formulas = !C.ver_formulas; break;
	case O_VER_REJILLA: h->sin_rejilla = !h->sin_rejilla; lib_cambio(C.lib); break;
	case O_INS_CELDAS: dlg_eliminar(1); break;
	case O_INS_FILAS: op_inserta(C.lib, h, 0, s.f0, s.f1 - s.f0 + 1); break;
	case O_INS_COLUMNAS: op_inserta(C.lib, h, 1, s.c0, (u32)s.c1 - s.c0 + 1); break;
	case O_INS_HOJA: {
		hoja *n = op_hoja_nueva(C.lib, (int)C.lib->activa);

		if (n)
			C.lib->activa = (u32)lib_hoja_indice(C.lib, n->id);
		C.grafico = -1;
		break;
	}
	case O_GRAFICO: dlg_grafico(C.grafico); break;
	case O_INS_SALTO: op_salto(C.lib, h, 0, h->cur_f, 1); if (h->cur_c) op_salto(C.lib, h, 1, h->cur_c, 1); break;
	case O_QUITAR_SALTO: op_salto(C.lib, h, 0, h->cur_f, 0); op_salto(C.lib, h, 1, h->cur_c, 0); break;
	case O_FUNCION: dlg_funcion(); break;
	case O_NOMBRE: dlg_nombre(); break;
	case O_COMENTARIO: dlg_comentario(); break;
	case O_FMT_CELDAS: dlg_formato_celdas(0); break;
	case O_ALTO_FILA: dlg_ancho(1); break;
	case O_AUTO_FILA: op_alto(C.lib, h, s.f0, s.f1, 0); break;
	case O_OCULTAR_FILAS: op_oculta(C.lib, h, 0, s.f0, s.f1, 1); break;
	case O_MOSTRAR_FILAS: op_oculta(C.lib, h, 0, s.f0 ? s.f0 - 1 : 0, s.f1 + 1, 0); break;
	case O_ANCHO_COL: dlg_ancho(0); break;
	case O_AUTO_COL: op_autoancho(C.lib, h, s.c0, s.c1, cel_ancho_texto); break;
	case O_OCULTAR_COLS: op_oculta(C.lib, h, 1, s.c0, s.c1, 1); break;
	case O_MOSTRAR_COLS: op_oculta(C.lib, h, 1, s.c0 ? s.c0 - 1 : 0, s.c1 + 1u, 0); break;
	case O_ANCHO_ESTANDAR: op_ancho(C.lib, h, s.c0, s.c1, 0); break;
	case O_NOMBRE_HOJA: dlg_nombre_hoja(); break;
	case O_COMBINAR_CENTRAR:
		if (hj_combinada(h, h->cur_f, h->cur_c) >= 0)
			op_descombina(C.lib, h, s);
		else
			op_combina(C.lib, h, s, 1);
		break;
	case O_COMBINAR: op_combina(C.lib, h, s, 0); break;
	case O_SEPARAR: op_descombina(C.lib, h, s); break;
	case O_CONDICIONAL: dlg_condicional(); break;
	case O_QUITAR_CONDICIONAL:
		dh_empieza(C.lib, "Quitar formato condicional");
		op_quita_condicional(C.lib, h, s);
		dh_acaba(C.lib);
		lib_cambio(C.lib);
		break;
	case O_RECALCULAR: {
		u32 n = calc_todo(C.lib);

		cu_aviso("Calculado: %u fórmulas", n);
		break;
	}
	case O_OPCIONES: dlg_opciones(); break;
	case O_ORDENAR: dlg_ordenar(); break;
	case O_AUTOFILTRO:
		if (h->hay_filtro)
			op_autofiltro(C.lib, h, NULL);
		else {
			rango r = s.f0 == s.f1 && s.c0 == s.c1 ? region_actual() : s;

			op_autofiltro(C.lib, h, &r);
			cu_aviso("Autofiltro: pulse el botón de la fila de títulos para elegir lo que se ve");
		}
		break;
	case O_MOSTRAR_TODO:
		if (h->hay_filtro) {
			rango r = h->filtro;

			op_autofiltro(C.lib, h, &r);
		}
		break;
	case O_NUEVA_VENTANA: {
		char p[300];

		so_programa(p, sizeof p);
		so_lanza(p, "");
		break;
	}
	case O_INMOVILIZAR:
		if (h->inm_f || h->inm_c)
			op_inmoviliza(C.lib, h, 0, 0);
		else {
			op_inmoviliza(C.lib, h, h->cur_f, h->cur_c);
			h->vista_f = h->cur_f > h->vista_f ? h->cur_f : h->vista_f;
			h->vista_c = h->cur_c > h->vista_c ? h->cur_c : h->vista_c;
		}
		C.todo = 1;
		break;
	case O_CONTENIDO: dlg_ayuda(); break;
	case O_FUNCIONES: dlg_funcion(); break;
	case O_ACERCA: dlg_acerca(); break;
	case O_AUTOSUMA: autosuma(); break;
	case O_ORDEN_ASC: ordena_rapido(0); break;
	case O_ORDEN_DESC: ordena_rapido(1); break;
	case O_NEGRITA: { int v = !estilo_activo()->negrita; formato(cb_negrita, &v, "Negrita"); break; }
	case O_CURSIVA: { int v = !estilo_activo()->cursiva; formato(cb_cursiva, &v, "Cursiva"); break; }
	case O_SUBRAYADO: { int v = !estilo_activo()->subrayado; formato(cb_subrayado, &v, "Subrayado"); break; }
	case O_IZQ: case O_CENTRO: case O_DER: {
		int v = o == O_IZQ ? AH_IZQ : o == O_CENTRO ? AH_CENTRO : AH_DER;

		if (estilo_activo()->alin_h == v)
			v = AH_GENERAL;
		formato(cb_alin, &v, "Alineación");
		break;
	}
	case O_MONEDA: formato(cb_fmt, "#,##0.00 €", "Formato de moneda"); break;
	case O_PORCENTAJE: formato(cb_fmt, "0%", "Formato de porcentaje"); break;
	case O_MILES: formato(cb_fmt, "#,##0.00", "Separador de millares"); break;
	case O_DEC_MAS: { int v = 1; formato(cb_decimales, &v, "Más decimales"); break; }
	case O_DEC_MENOS: { int v = -1; formato(cb_decimales, &v, "Menos decimales"); break; }
	case O_BORDES: bordes(C.borde_elegido); break;
	case O_RELLENO: {
		u32 v[3] = { C.color_relleno ? TR_SOLIDO : TR_NADA, C.color_relleno ? C.color_relleno : 0xFFFFFF, 0 };

		formato(cb_relleno, v, "Relleno");
		break;
	}
	case O_COLOR: formato(cb_color, &C.color_letra, "Color de la letra"); break;
	case O_BROCHA:
		C.con_brocha = !C.con_brocha;
		C.brocha = hj_estilo(h, h->cur_f, h->cur_c);
		if (C.con_brocha)
			cu_aviso("Copiar formato: elija las celdas a las que se le pone");
		break;
	case O_HOJA_SIG:
		if (C.lib->activa + 1 < C.lib->n_h)
			C.lib->activa++;
		C.grafico = -1;
		break;
	case O_HOJA_ANT:
		if (C.lib->activa > 0)
			C.lib->activa--;
		C.grafico = -1;
		break;
	case O_FECHA_HOY: {
		int a, m, d, hh, mi, se;
		char t[32];

		so_ahora(&a, &m, &d, &hh, &mi, &se);
		fmt(t, sizeof t, "%02d/%02d/%04d", d, m, a);
		if (C.editando)
			ed_mete_txt(t);
		else
			ed_empieza(t, 1, 1);
		break;
	}
	default:
		cu_aviso("Eso aún no lo hace Cuentas");
	}
	cu_cambio();
}

/* ===================================================================== */
/*  Las listas de las barras                                              */
/* ===================================================================== */
static const char *familias_txt(void)
{
	static char t[2000];
	const char *f[LETRAS_MAX];
	int n = letras_familias(f, LETRAS_MAX), i;

	t[0] = 0;
	for (i = 0; i < n; i++) {
		if (i)
			anade(t, "|", sizeof t);
		anade(t, f[i], sizeof t);
	}
	return t;
}

static void abre_lista(int orden, int x, int y)
{
	const char *ops = orden == O_LISTA_FUENTE ? familias_txt() : orden == O_LISTA_TAMANO ? TAMANOS : orden == O_LISTA_ZOOM ? ZOOMS :
			  orden == O_COLOR_MENU ? COLORES : orden == O_RELLENO_MENU ? RELLENOS : orden == O_BORDES_MENU ? BORDES : "";

	so_menu(C.v, x, y, ops, 0, ETIQ_LISTA + (u32)orden);
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

static void elige_de_lista(int orden, int op)
{
	char t[100];
	hoja *h = cu_hoja();

	if (op < 0)
		return;
	switch (orden) {
	case O_LISTA_FUENTE:
		opcion_n(familias_txt(), op, t, sizeof t);
		formato(cb_fuente, t, "Fuente");
		break;
	case O_LISTA_TAMANO: {
		int v;

		opcion_n(TAMANOS, op, t, sizeof t);
		v = (int)numero(t, NULL) * 2;
		formato(cb_tam, &v, "Tamaño");
		break;
	}
	case O_LISTA_ZOOM: {
		static const int z[] = { 50, 75, 85, 100, 125, 150, 200 };

		if (op < 7)
			h->zoom = (u16)z[op];
		else {
			/* lo elegido, a lo ancho de la ventana */
			rango s = cu_seleccion();
			u32 c, an = 0;

			for (c = s.c0; c <= s.c1; c++)
				an += hj_ancho(h, c);
			if (an)
				h->zoom = (u16)(C.g.can * 100 / an);
			if (h->zoom > 400)
				h->zoom = 400;
			if (h->zoom < 10)
				h->zoom = 10;
			h->vista_c = s.c0 >= h->inm_c ? s.c0 : h->vista_c;
		}
		cu_aviso("Zoom: %d%%", h->zoom);
		C.todo = 1;
		break;
	}
	case O_COLOR_MENU:
		if (op < (int)(sizeof colores_v / sizeof colores_v[0])) {
			C.color_letra = colores_v[op];
			formato(cb_color, &C.color_letra, "Color de la letra");
		}
		break;
	case O_RELLENO_MENU:
		if (op >= 11) {
			dlg_formato_celdas(4);
			break;
		}
		{
			u32 v[3] = { TR_SOLIDO, rellenos_v[op], 0x000000 };

			if (op == 0)
				v[0] = TR_NADA;
			else if (op == 8)
				v[0] = TR_GRIS50;
			else if (op == 9)
				v[0] = TR_DIAG_ARRIBA;
			else if (op == 10)
				v[0] = TR_CUADRICULA;
			if (op >= 8)
				v[2] = 0x3A4454;
			if (op < 8)
				C.color_relleno = rellenos_v[op];
			formato(cb_relleno, v, "Relleno");
		}
		break;
	case O_BORDES_MENU:
		bordes(op);
		break;
	}
}

/* ===================================================================== */
/*  El menú del botón derecho                                             */
/* ===================================================================== */
static int contexto;	/* 0 celdas, 1 columnas, 2 filas, 3 pestaña, 4 gráfico */
static const int ctx_celdas[] = { O_CORTAR, O_COPIAR, O_PEGAR, O_PEGADO_ESPECIAL, O_NADA, O_INS_CELDAS, O_ELIMINAR, O_BORRAR_CONTENIDO, O_NADA,
				  O_COMENTARIO, O_NADA, O_FMT_CELDAS, O_NOMBRE, -1 };
static const int ctx_cols[] = { O_CORTAR, O_COPIAR, O_PEGAR, O_NADA, O_INS_COLUMNAS, O_ELIMINAR, O_BORRAR_CONTENIDO, O_NADA, O_FMT_CELDAS,
				O_ANCHO_COL, O_AUTO_COL, O_OCULTAR_COLS, O_MOSTRAR_COLS, -1 };
static const int ctx_filas[] = { O_CORTAR, O_COPIAR, O_PEGAR, O_NADA, O_INS_FILAS, O_ELIMINAR, O_BORRAR_CONTENIDO, O_NADA, O_FMT_CELDAS,
				 O_ALTO_FILA, O_AUTO_FILA, O_OCULTAR_FILAS, O_MOSTRAR_FILAS, -1 };
static const int ctx_hoja[] = { O_INS_HOJA, O_ELIMINAR_HOJA, O_NOMBRE_HOJA, O_MOVER_HOJA, -1 };
static const int ctx_graf[] = { O_GRAFICO, O_BORRAR_CONTENIDO, -1 };
static const int *const ctx_listas[] = { ctx_celdas, ctx_cols, ctx_filas, ctx_hoja, ctx_graf };

static const char *nombre_orden(int o)
{
	switch (o) {
	case O_CORTAR: return "Cortar";
	case O_COPIAR: return "Copiar";
	case O_PEGAR: return "Pegar";
	case O_PEGADO_ESPECIAL: return "Pegado especial...";
	case O_INS_CELDAS: return "Insertar...";
	case O_ELIMINAR: return "Eliminar...";
	case O_BORRAR_CONTENIDO: return "Borrar contenido";
	case O_COMENTARIO: return "Comentario...";
	case O_FMT_CELDAS: return "Formato de celdas...";
	case O_NOMBRE: return "Definir nombre...";
	case O_INS_COLUMNAS: return "Insertar columnas";
	case O_INS_FILAS: return "Insertar filas";
	case O_ANCHO_COL: return "Ancho de columna...";
	case O_AUTO_COL: return "Autoajustar el ancho";
	case O_OCULTAR_COLS: return "Ocultar";
	case O_MOSTRAR_COLS: return "Mostrar";
	case O_ALTO_FILA: return "Alto de fila...";
	case O_AUTO_FILA: return "Autoajustar el alto";
	case O_OCULTAR_FILAS: return "Ocultar";
	case O_MOSTRAR_FILAS: return "Mostrar";
	case O_INS_HOJA: return "Insertar hoja";
	case O_ELIMINAR_HOJA: return "Eliminar hoja";
	case O_NOMBRE_HOJA: return "Cambiar nombre...";
	case O_MOVER_HOJA: return "Mover o copiar...";
	case O_GRAFICO: return "Modificar el gráfico...";
	}
	return "-";
}

static void menu_contexto(int cual, int x, int y)
{
	char t[800];
	const int *l = ctx_listas[cual];
	int i;
	u32 g = 0;

	contexto = cual;
	t[0] = 0;
	for (i = 0; l[i] >= 0; i++) {
		if (i)
			anade(t, "|", sizeof t);
		if (cual == 4 && l[i] == O_BORRAR_CONTENIDO)
			anade(t, "Eliminar el gráfico", sizeof t);
		else
			anade(t, nombre_orden(l[i]), sizeof t);
		if (l[i] != O_NADA && !activa(l[i]))
			g |= 1u << i;
	}
	so_menu(C.v, x, y, t, g, ETIQ_CONTEXTO);
}

static void elige_contexto(int op)
{
	const int *l = ctx_listas[contexto];
	int i;

	for (i = 0; l[i] >= 0; i++)
		if (i == op && l[i] != O_NADA) {
			if (contexto == 4 && l[i] == O_BORRAR_CONTENIDO) {
				op_grafico(C.lib, cu_hoja(), C.grafico, NULL);
				C.grafico = -1;
				cu_cambio();
			} else
				cu_orden(l[i]);
			return;
		}
}

/* ===================================================================== */
/*  El teclado                                                            */
/* ===================================================================== */
static void tecla_editando(const so_suceso *s)
{
	u32 t = s->tecla;
	int mayus = (s->mods & SO_CON_MAYUS) != 0, ctrl = (s->mods & SO_CON_CONTROL) != 0;
	hoja *h = cu_hoja();

	/* señalar con las flechas */
	if ((t == SO_T_ARRIBA || t == SO_T_ABAJO || t == SO_T_IZQUIERDA || t == SO_T_DERECHA) && puede_senalar() && C.editando == 1) {
		u32 f = C.senala ? C.sen_f : C.ed_f, c = C.senala ? C.sen_c : C.ed_c;
		int df = t == SO_T_ARRIBA ? -1 : t == SO_T_ABAJO ? 1 : 0, dc = t == SO_T_IZQUIERDA ? -1 : t == SO_T_DERECHA ? 1 : 0;

		if (ctrl)
			borde_datos(df, dc, &f, &c);
		else
			al_lado(df, dc, &f, &c);
		senala_a(f, c, mayus);
		return;
	}
	if (t != SO_T_F4)
		C.senala = 0;
	switch (t) {
	case SO_ESC:
		ed_cancela();
		return;
	case SO_INTRO:
		if (mayus && C.editando) {
			/* (Mayús+Intro: acepta y sube) */
			if (!ed_acepta(0))
				mueve(-1, 0, 0, 0);
			return;
		}
		if (!ed_acepta(ctrl))
			intro_abajo();
		return;
	case SO_TAB:
		if (C.tab_col < 0)
			C.tab_col = (int)C.ed_c;
		if (!ed_acepta(0))
			mueve(0, 1, 0, 0);
		return;
	case SO_T_MAYUS_TAB:
		if (!ed_acepta(0))
			mueve(0, -1, 0, 0);
		return;
	case SO_T_ARRIBA:
	case SO_T_ABAJO:
		if (C.ed_nuevo || C.editando == 1) {
			if (!ed_acepta(0))
				mueve(t == SO_T_ARRIBA ? -1 : 1, 0, 0, 0);
			return;
		}
		return;
	case SO_T_IZQUIERDA:
	case SO_T_DERECHA:
		if (C.ed_nuevo && C.editando == 1) {
			if (!ed_acepta(0))
				mueve(0, t == SO_T_IZQUIERDA ? -1 : 1, 0, 0);
			return;
		}
		if (t == SO_T_IZQUIERDA && C.ed_cur > 0)
			C.ed_cur--;
		if (t == SO_T_DERECHA && C.ed_cur < C.ed_n)
			C.ed_cur++;
		if (!mayus)
			C.ed_ancla = C.ed_cur;
		C.sucio = 1;
		return;
	case SO_T_INICIO:
		C.ed_cur = 0;
		if (!mayus)
			C.ed_ancla = 0;
		C.ed_nuevo = 0;
		C.sucio = 1;
		return;
	case SO_T_FIN:
		C.ed_cur = C.ed_n;
		if (!mayus)
			C.ed_ancla = C.ed_n;
		C.ed_nuevo = 0;
		C.sucio = 1;
		return;
	case SO_RETROCESO:
		if (C.ed_cur != C.ed_ancla)
			ed_borra_eleccion();
		else if (C.ed_cur > 0) {
			memmove(C.ed + C.ed_cur - 1, C.ed + C.ed_cur, (C.ed_n - C.ed_cur) * sizeof(u32));
			C.ed_n--;
			C.ed_cur--;
			C.ed_ancla = C.ed_cur;
		}
		C.sucio = 1;
		return;
	case SO_T_SUPR:
		if (C.ed_cur != C.ed_ancla)
			ed_borra_eleccion();
		else if (C.ed_cur < C.ed_n) {
			memmove(C.ed + C.ed_cur, C.ed + C.ed_cur + 1, (C.ed_n - C.ed_cur - 1) * sizeof(u32));
			C.ed_n--;
		}
		C.sucio = 1;
		return;
	case SO_T_F2:
		C.ed_nuevo = !C.ed_nuevo;
		C.sucio = 1;
		return;
	case SO_T_F4:
		ed_f4();
		return;
	}
	if (ctrl && t < 32) {
		switch (t) {
		case 'C' - 64: {
			/* copiar lo elegido del texto */
			u32 a = C.ed_ancla < C.ed_cur ? C.ed_ancla : C.ed_cur, b = C.ed_ancla < C.ed_cur ? C.ed_cur : C.ed_ancla;
			char *x = utf8_de_u32(C.ed + a, b - a);

			if (x) {
				so_al_portapapeles(x, largo(x));
				so_suelta(x);
			}
			return;
		}
		case 'V' - 64: {
			u32 n;
			char *x = so_del_portapapeles(&n);

			if (x) {
				char *p;

				for (p = x; *p; p++)
					if (*p == '\r' || *p == '\n' || *p == '\t')
						*p = ' ';
				ed_mete_txt(x);
				so_suelta(x);
			}
			C.sucio = 1;
			return;
		}
		case 'E' - 64:
			C.ed_ancla = 0;
			C.ed_cur = C.ed_n;
			C.sucio = 1;
			return;
		}
		return;
	}
	if (t >= 32 && t < SO_T_BASE) {
		ed_mete(&t, 1);
		C.sucio = 1;
	}
	(void)h;
}

static void tecla(const so_suceso *s)
{
	u32 t = s->tecla;
	int mayus = (s->mods & SO_CON_MAYUS) != 0, ctrl = (s->mods & SO_CON_CONTROL) != 0;
	hoja *h = cu_hoja();

	/* (moverse sin escribir olvida la columna del primer Tab; escribiendo, no) */
	if (!C.cuadro && !C.editando && t != SO_TAB && t != SO_INTRO && !(t >= 32 && t < SO_T_BASE) && t != SO_RETROCESO && t != SO_T_F2)
		C.tab_col = -1;

	if (C.cuadro) {
		if (ui_cuadro_suceso(C.cuadro, C.v, s))
			C.sucio = 1;
		return;
	}
	C.ayuda_encima = NULL;
	if (t >= SO_T_ALT && t < SO_T_ALT + 26) {
		int m = ui_menu_letra(MENUS, 'a' + (t - SO_T_ALT));

		if (C.editando && ed_acepta(0))
			return;
		if (m >= 0)
			abre_menu(m);
		return;
	}
	if (C.previa) {
		if (t == SO_ESC)
			C.previa = 0;
		else if (t == SO_T_AV_PAG || t == SO_T_DERECHA || t == SO_T_ABAJO)
			C.pag_previa++;
		else if (t == SO_T_RE_PAG || t == SO_T_IZQUIERDA || t == SO_T_ARRIBA)
			C.pag_previa--;
		C.todo = 1;
		C.sucio = 1;
		return;
	}
	if (C.editando) {
		tecla_editando(s);
		return;
	}
	switch (t) {
	case SO_ESC:
		if (C.completa)
			C.completa = 0;
		else if (C.con_brocha)
			C.con_brocha = 0;
		else if (op_hay_recorte())
			op_recorte_olvida();
		C.grafico = -1;
		C.todo = 1;
		C.sucio = 1;
		return;
	case SO_T_F1: cu_orden(O_CONTENIDO); return;
	case SO_T_F2: {
		char e[4096];

		lib_texto_edicion(C.lib, h, h->cur_f, h->cur_c, e, sizeof e);
		ed_empieza(e, 0, 1);
		return;
	}
	case SO_T_F5: cu_orden(O_IR_A); return;
	case SO_T_F9: cu_orden(O_RECALCULAR); return;
	case SO_T_F10: abre_menu(0); return;
	case SO_T_F11:
		if (mayus)
			cu_orden(O_INS_HOJA);
		else
			cu_orden(O_GRAFICO);
		return;
	case SO_T_F12: cu_orden(O_GUARDAR_COMO); return;
	case SO_T_ARRIBA: mueve(-1, 0, ctrl, mayus); return;
	case SO_T_ABAJO: mueve(1, 0, ctrl, mayus); return;
	case SO_T_IZQUIERDA: mueve(0, -1, ctrl, mayus); return;
	case SO_T_DERECHA: mueve(0, 1, ctrl, mayus); return;
	case SO_TAB:
		if (C.tab_col < 0)
			C.tab_col = (int)h->cur_c;
		mueve(0, 1, 0, 0);
		return;
	case SO_T_MAYUS_TAB: mueve(0, -1, 0, 0); return;
	case SO_INTRO:
		if (mayus)
			mueve(-1, 0, 0, 0);
		else
			intro_abajo();
		return;
	case SO_T_RE_PAG:
		if (ctrl)
			cu_orden(O_HOJA_ANT);
		else
			a_pagina(-1, mayus);
		return;
	case SO_T_AV_PAG:
		if (ctrl)
			cu_orden(O_HOJA_SIG);
		else
			a_pagina(1, mayus);
		return;
	case SO_T_INICIO:
		if (!mayus) {
			C.ancla_f = h->cur_f;
			C.ancla_c = h->cur_c;
		}
		cu_elige(ctrl ? 0 : h->cur_f, 0, mayus);
		return;
	case SO_T_FIN:
		if (ctrl) {
			if (!mayus) {
				C.ancla_f = h->cur_f;
				C.ancla_c = h->cur_c;
			}
			cu_elige(h->lim_f ? h->lim_f - 1 : 0, h->lim_c ? h->lim_c - 1 : 0, mayus);
		}
		return;
	case SO_T_SUPR:
		if (C.grafico >= 0) {
			op_grafico(C.lib, h, C.grafico, NULL);
			C.grafico = -1;
			cu_cambio();
			return;
		}
		cu_orden(O_BORRAR_CONTENIDO);
		return;
	case SO_RETROCESO:
		ed_empieza("", 1, 1);
		return;
	}
	if (ctrl && t < 32) {
		static const int o[27] = { 0, O_ABRIR, O_BUSCAR, O_COPIAR, O_DER, O_TODO, O_NADA, O_GUARDAR, O_NADA, O_NADA, O_NADA,
					   O_CURSIVA, O_REEMPLAZAR, O_NADA, O_NEGRITA, O_NADA, O_IMPRIMIR, O_IZQ, O_NADA, O_SUBRAYADO, O_CENTRO,
					   O_NUEVO, O_PEGAR, O_NADA, O_CORTAR, O_REHACER, O_DESHACER };

		if (t <= 26 && o[t] != O_NADA)
			cu_orden(o[t]);
		return;
	}
	/* (en Hispa-Pentium, Ctrl+1 y Ctrl+; llegan sin la marca de Control: se mira) */
	if ((t == '1' || t == ';' || t == ',') && !ctrl && so_control())
		ctrl = 1;
	if (ctrl && t == '1') {
		cu_orden(O_FMT_CELDAS);
		return;
	}
	if (ctrl && (t == ';' || t == ',')) {
		cu_orden(O_FECHA_HOY);
		return;
	}
	if (t >= 32 && t < SO_T_BASE) {
		/* a escribir (lo de antes se cambia) */
		char b[8];
		int k = utf8_pon(t, b);

		b[k] = 0;
		ed_empieza(b, 1, 1);
	}
}

/* ===================================================================== */
/*  El ratón                                                              */
/* ===================================================================== */
static void brocha_aplica(void)
{
	if (!C.con_brocha)
		return;
	formato(cb_brocha, &C.brocha, "Copiar formato");
	C.con_brocha = 0;
}

static void raton_hoja(const so_suceso *s)
{
	hoja *h = cu_hoja();
	u32 f, c;
	int i, zona = vs_en(s->x, s->y, &f, &c, &i);

	if (s->clase == SO_MUEVE && !(s->botones & SO_BOTON_IZQ)) {
		/* (el comentario de debajo del ratón: se repinta si cambia de celda) */
		static u32 antes = 0xFFFFFFFF;
		u32 k = zona == VS_CELDA ? CU_CLAVE(f, c) : 0xFFFFFFFE;

		if (k != antes && (hj_comentario(h, f, c) || antes != 0xFFFFFFFF)) {
			C.sucio = 1;
			antes = hj_comentario(h, f, c) ? k : 0xFFFFFFFF;
		}
		C.ayuda_encima = zona == VS_RELLENO ? "Arrastre el cuadradito para rellenar (series, fórmulas, meses...)" :
				 zona == VS_BORDE_SEL ? "Arrastre el borde para mover lo elegido" :
				 zona == VS_BORDE_COL ? "Arrastre para cambiar el ancho (doble clic: autoajustar)" :
				 zona == VS_BORDE_FILA ? "Arrastre para cambiar el alto" :
				 zona == VS_FILTRO ? "Autofiltro: elegir lo que se ve" : NULL;
		return;
	}
	if (s->clase == SO_BAJA && (s->botones & SO_BOTON_DER)) {
		if (zona == VS_GRAFICO || zona == VS_GRAF_TAM) {
			C.grafico = i;
			menu_contexto(4, s->x, s->y);
		} else if (zona == VS_ROT_COL || zona == VS_BORDE_COL) {
			rango sel = cu_seleccion();

			if (!(sel.f0 == 0 && sel.f1 == CU_FILAS - 1 && c >= sel.c0 && c <= sel.c1)) {
				C.ancla_f = 0;
				C.ancla_c = c;
				h->cur_f = h->vista_f;
				h->cur_c = c;
				h->sel = cu_rango(0, c, CU_FILAS - 1, c);
			}
			menu_contexto(1, s->x, s->y);
		} else if (zona == VS_ROT_FILA || zona == VS_BORDE_FILA) {
			rango sel = cu_seleccion();

			if (!(sel.c0 == 0 && sel.c1 == CU_COLS - 1 && f >= sel.f0 && f <= sel.f1)) {
				h->cur_f = f;
				h->cur_c = h->vista_c;
				h->sel = cu_rango(f, 0, f, CU_COLS - 1);
			}
			menu_contexto(2, s->x, s->y);
		} else if (zona == VS_CELDA || zona == VS_RELLENO || zona == VS_BORDE_SEL) {
			if (!rg_dentro(cu_seleccion(), f, c))
				cu_elige(f, c, 0);
			menu_contexto(0, s->x, s->y);
		}
		C.sucio = 1;
		return;
	}
	if (s->clase != SO_BAJA)
		return;
	C.tab_col = -1;
	/* escribiendo una fórmula: el clic señala */
	if (C.editando == 1 || C.editando == 2) {
		if (zona == VS_CELDA && puede_senalar()) {
			senala_a(f, c, (s->mods & SO_CON_MAYUS) != 0);
			C.arrastra = AR_SEL;
			C.arr_f = f;
			C.arr_c = c;
			return;
		}
		if (zona == VS_CELDA && C.ed_hoja == h->id && f == C.ed_f && c == C.ed_c)
			return;
		if (ed_acepta(0))
			return;
	}
	C.grafico = -1;
	switch (zona) {
	case VS_GRAFICO:
		C.grafico = i;
		C.arrastra = AR_GRAFICO;
		C.arr_x = s->x;
		C.arr_y = s->y;
		C.arr_i = i;
		C.arr_v0 = h->gr[i].dx;
		C.arr_v1 = h->gr[i].dy;
		if (s->veces >= 2) {
			C.arrastra = AR_NADA;
			dlg_grafico(i);
		}
		break;
	case VS_GRAF_TAM:
		C.grafico = i;
		C.arrastra = AR_GRAF_TAM;
		C.arr_x = s->x;
		C.arr_y = s->y;
		C.arr_i = i;
		C.arr_v0 = h->gr[i].an;
		C.arr_v1 = h->gr[i].al;
		break;
	case VS_ESQUINA:
		cu_orden(O_TODO);
		break;
	case VS_ROT_COL:
		if (s->mods & SO_CON_MAYUS) {
			h->sel = cu_rango(0, C.ancla_c, CU_FILAS - 1, c);
		} else {
			C.ancla_c = c;
			C.ancla_f = 0;
			h->cur_c = c;
			if (h->cur_f < h->vista_f)
				h->cur_f = h->vista_f;
			h->sel = cu_rango(0, c, CU_FILAS - 1, c);
		}
		C.arrastra = AR_COLS;
		brocha_aplica();
		break;
	case VS_ROT_FILA:
		if (s->mods & SO_CON_MAYUS) {
			h->sel = cu_rango(C.ancla_f, 0, f, CU_COLS - 1);
		} else {
			C.ancla_f = f;
			C.ancla_c = 0;
			h->cur_f = f;
			if (h->cur_c < h->vista_c)
				h->cur_c = h->vista_c;
			h->sel = cu_rango(f, 0, f, CU_COLS - 1);
		}
		C.arrastra = AR_FILAS;
		brocha_aplica();
		break;
	case VS_BORDE_COL:
		if (s->veces >= 2) {
			rango sel = cu_seleccion();

			if (sel.f0 == 0 && sel.f1 == CU_FILAS - 1 && c >= sel.c0 && c <= sel.c1)
				op_autoancho(C.lib, h, sel.c0, sel.c1, cel_ancho_texto);
			else
				op_autoancho(C.lib, h, c, c, cel_ancho_texto);
			cu_cambio();
			break;
		}
		C.arrastra = AR_ANCHO;
		C.arr_c = c;
		C.arr_x = s->x;
		C.arr_v0 = (i32)hj_ancho(h, c);
		C.arr_v1 = C.arr_v0;
		break;
	case VS_BORDE_FILA:
		if (s->veces >= 2) {
			op_alto(C.lib, h, f, f, 0);
			cu_cambio();
			break;
		}
		C.arrastra = AR_ALTO;
		C.arr_f = f;
		C.arr_y = s->y;
		C.arr_v0 = (i32)hj_alto(h, f);
		C.arr_v1 = C.arr_v0;
		break;
	case VS_RELLENO:
		C.arrastra = AR_RELLENO;
		C.arr_r = cu_seleccion();
		break;
	case VS_BORDE_SEL:
		C.arrastra = AR_MOVER;
		C.arr_r = cu_seleccion();
		C.arr_f = f;
		C.arr_c = c;
		break;
	case VS_FILTRO:
		dlg_filtro(c);
		break;
	case VS_CELDA:
		if (s->veces >= 2 && !(s->mods & SO_CON_MAYUS)) {
			/* doble clic: corregirla */
			char e[4096];

			cu_elige(f, c, 0);
			lib_texto_edicion(C.lib, h, f, c, e, sizeof e);
			ed_empieza(e, 0, 1);
			break;
		}
		if (s->mods & SO_CON_MAYUS)
			cu_elige(f, c, 1);
		else
			cu_elige(f, c, 0);
		C.arrastra = AR_SEL;
		break;
	}
	C.sucio = 1;
}

static void arrastrando(const so_suceso *s)
{
	hoja *h = cu_hoja();
	u32 f, c;
	int i;

	if (s->clase == SO_SUBE || !(s->botones & SO_BOTON_IZQ)) {
		int a = C.arrastra;

		C.arrastra = AR_NADA;
		switch (a) {
		case AR_SEL:
		case AR_COLS:
		case AR_FILAS:
			if (!C.editando)
				brocha_aplica();
			break;
		case AR_ANCHO:
			if (C.arr_v1 != C.arr_v0) {
				rango sel = cu_seleccion();

				if (sel.f0 == 0 && sel.f1 == CU_FILAS - 1 && C.arr_c >= sel.c0 && C.arr_c <= sel.c1)
					op_ancho(C.lib, h, sel.c0, sel.c1, (u32)C.arr_v1);
				else
					op_ancho(C.lib, h, C.arr_c, C.arr_c, (u32)C.arr_v1);
			}
			break;
		case AR_ALTO:
			if (C.arr_v1 != C.arr_v0) {
				rango sel = cu_seleccion();

				if (sel.c0 == 0 && sel.c1 == CU_COLS - 1 && C.arr_f >= sel.f0 && C.arr_f <= sel.f1)
					op_alto(C.lib, h, sel.f0, sel.f1, (u32)C.arr_v1);
				else
					op_alto(C.lib, h, C.arr_f, C.arr_f, (u32)C.arr_v1);
			}
			break;
		case AR_RELLENO: {
			rango fu = cu_seleccion();

			if (C.arr_r.f0 != fu.f0 || C.arr_r.f1 != fu.f1 || C.arr_r.c0 != fu.c0 || C.arr_r.c1 != fu.c1) {
				op_rellena(C.lib, h, fu, C.arr_r);
				cu_sel_rango(C.arr_r);
			}
			break;
		}
		case AR_MOVER: {
			rango de = cu_seleccion();

			if (C.arr_r.f0 != de.f0 || C.arr_r.c0 != de.c0) {
				char e[200];

				if (op_mueve(C.lib, h, de, C.arr_r.f0, C.arr_r.c0, e, sizeof e))
					dlg_aviso("No se puede mover", e);
				else {
					h->cur_f = C.arr_r.f0 + (h->cur_f - de.f0);
					h->cur_c = C.arr_r.c0 + (h->cur_c - de.c0);
					cu_sel_rango(C.arr_r);
				}
			}
			break;
		}
		case AR_GRAFICO:
		case AR_GRAF_TAM:
			if (C.arr_i >= 0 && (u32)C.arr_i < h->n_gr) {
				grafico g = h->gr[C.arr_i];
				i32 dx = h->gr[C.arr_i].dx, dy = h->gr[C.arr_i].dy, an = h->gr[C.arr_i].an, al = h->gr[C.arr_i].al;

				/* (lo movido se apunta como un paso: se deshace) */
				h->gr[C.arr_i].dx = C.arr_v0;
				h->gr[C.arr_i].dy = C.arr_v1;
				if (a == AR_GRAF_TAM) {
					h->gr[C.arr_i].dx = dx;
					h->gr[C.arr_i].dy = dy;
					h->gr[C.arr_i].an = C.arr_v0;
					h->gr[C.arr_i].al = C.arr_v1;
				}
				g.dx = dx;
				g.dy = dy;
				g.an = an;
				g.al = al;
				/* la esquina: la celda que queda debajo */
				{
					int gx, gy, gan, gal;

					vs_grafico_rect(&g, &gx, &gy, &gan, &gal);
					if (vs_en(gx + 1, gy + 1, &f, &c, &i) != VS_NADA && f < CU_FILAS && c < CU_COLS) {
						int cx0, cy0;

						if (vs_col_x(c, &cx0) && vs_fila_y(f, &cy0)) {
							g.f = f;
							g.c = (u16)c;
							g.dx = (i32)((gx - cx0) * 100 / C.g.zoom);
							g.dy = (i32)((gy - cy0) * 100 / C.g.zoom);
						}
					}
				}
				op_grafico(C.lib, h, C.arr_i, &g);
			}
			break;
		}
		C.sucio = 1;
		C.todo = 1;
		return;
	}
	if (s->clase != SO_MUEVE)
		return;
	switch (C.arrastra) {
	case AR_SEL:
		if (vs_en(s->x < C.g.cx ? C.g.cx : s->x, s->y < C.g.cy ? C.g.cy : s->y, &f, &c, &i) == VS_NADA)
			break;
		if (f >= CU_FILAS || c >= CU_COLS)
			break;
		if (C.editando && C.senala) {
			senala_a(f, c, 1);
			break;
		}
		cu_elige(f, c, 1);
		break;
	case AR_COLS:
		vs_en(s->x, C.g.y + 2, &f, &c, &i);
		if (c < CU_COLS)
			h->sel = cu_rango(0, C.ancla_c, CU_FILAS - 1, c);
		C.sucio = 1;
		break;
	case AR_FILAS:
		vs_en(C.g.x + 2, s->y, &f, &c, &i);
		if (f < CU_FILAS)
			h->sel = cu_rango(C.ancla_f, 0, f, CU_COLS - 1);
		C.sucio = 1;
		break;
	case AR_ANCHO: {
		i32 v = C.arr_v0 + (s->x - C.arr_x) * 100 / C.g.zoom;

		if (v < 0)
			v = 0;
		C.arr_v1 = v;
		h->ancho[C.arr_c] = (u16)(v ? v : 1);
		h->oculta_c[C.arr_c] = v == 0;
		cu_aviso("Ancho: %d puntos", v);
		C.sucio = 1;
		break;
	}
	case AR_ALTO: {
		i32 v = C.arr_v0 + (s->y - C.arr_y) * 100 / C.g.zoom;

		if (v < 4)
			v = 4;
		C.arr_v1 = v;
		hj_pon_alto(h, C.arr_f, (u32)v);
		cu_aviso("Alto: %d puntos", v);
		C.sucio = 1;
		break;
	}
	case AR_RELLENO: {
		rango fu = cu_seleccion();

		if (vs_en(s->x, s->y, &f, &c, &i) == VS_NADA || f >= CU_FILAS || c >= CU_COLS)
			break;
		/* hacia donde más se aleja */
		{
			i64 df = (i64)f > fu.f1 ? (i64)f - fu.f1 : (i64)f < fu.f0 ? (i64)fu.f0 - f : 0;
			i64 dc = (i64)c > fu.c1 ? (i64)c - fu.c1 : (i64)c < fu.c0 ? (i64)fu.c0 - c : 0;

			C.arr_r = fu;
			if (df >= dc && df > 0) {
				if (f > fu.f1)
					C.arr_r.f1 = f;
				else
					C.arr_r.f0 = f;
			} else if (dc > 0) {
				if (c > fu.c1)
					C.arr_r.c1 = (u16)c;
				else
					C.arr_r.c0 = (u16)c;
			}
		}
		C.sucio = 1;
		break;
	}
	case AR_MOVER: {
		rango de = cu_seleccion();

		if (vs_en(s->x, s->y, &f, &c, &i) == VS_NADA || f >= CU_FILAS || c >= CU_COLS)
			break;
		{
			i64 nf = (i64)de.f0 + ((i64)f - C.arr_f), nc = (i64)de.c0 + ((i64)c - C.arr_c);
			u32 al = de.f1 - de.f0, an = (u32)de.c1 - de.c0;

			if (nf < 0)
				nf = 0;
			if (nc < 0)
				nc = 0;
			if (nf + al >= CU_FILAS)
				nf = CU_FILAS - 1 - al;
			if (nc + an >= CU_COLS)
				nc = CU_COLS - 1 - an;
			C.arr_r = cu_rango((u32)nf, (u32)nc, (u32)nf + al, (u32)nc + an);
		}
		C.sucio = 1;
		break;
	}
	case AR_GRAFICO:
		h->gr[C.arr_i].dx = C.arr_v0 + (s->x - C.arr_x) * 100 / C.g.zoom;
		h->gr[C.arr_i].dy = C.arr_v1 + (s->y - C.arr_y) * 100 / C.g.zoom;
		C.sucio = 1;
		break;
	case AR_GRAF_TAM: {
		i32 an = C.arr_v0 + (s->x - C.arr_x) * 100 / C.g.zoom, al = C.arr_v1 + (s->y - C.arr_y) * 100 / C.g.zoom;

		h->gr[C.arr_i].an = an < 80 ? 80 : an;
		h->gr[C.arr_i].al = al < 60 ? 60 : al;
		C.sucio = 1;
		break;
	}
	case AR_DESP_V:
		h->vista_f = h->inm_f + (u32)ui_desp_arrastra(&desp_v, C.arr_v0, C.arr_y, s->y);
		C.sucio = 1;
		break;
	case AR_DESP_H:
		h->vista_c = h->inm_c + (u32)ui_desp_arrastra(&desp_h, C.arr_v0, C.arr_x, s->x);
		C.sucio = 1;
		break;
	}
}

static void raton(const so_suceso *s)
{
	int flecha;
	hoja *h = cu_hoja();

	C.raton_x = s->x;
	C.raton_y = s->y;
	if (C.cuadro) {
		if (ui_cuadro_suceso(C.cuadro, C.v, s))
			C.sucio = 1;
		return;
	}
	if (C.arrastra) {
		arrastrando(s);
		return;
	}
	/* la barra de menús */
	if (!C.completa && s->y < ui_alto_menus()) {
		int m = ui_menu_en(MENUS, 0, s->x, NULL);

		if (s->clase == SO_BAJA && m >= 0) {
			if (C.editando && ed_acepta(0))
				return;
			abre_menu(m);
		} else if (m != encima_menu) {
			encima_menu = m;
			C.sucio = 1;
		}
		return;
	}
	if (encima_menu >= 0) {
		encima_menu = -1;
		C.sucio = 1;
	}
	/* las barras */
	if (C.ver_barras && !C.completa && s->y >= y_b1 && s->y < y_b2 + ui_alto_barra()) {
		ui_boton *b = s->y < y_b2 ? barra1 : barra2;
		int n = s->y < y_b2 ? N1 : N2, i = ui_boton_en(b, n, s->x, s->y, &flecha);
		int *enc = s->y < y_b2 ? &encima1 : &encima2, *pul = s->y < y_b2 ? &pulsado1 : &pulsado2;

		if (s->clase == SO_MUEVE) {
			if (i != *enc) {
				*enc = i;
				(s->y < y_b2 ? &encima2 : &encima1)[0] = -1;
				C.ayuda_encima = i >= 0 ? b[i].ayuda : NULL;
				C.sucio = 1;
			}
		} else if (s->clase == SO_BAJA && i >= 0 && !b[i].apagado) {
			*pul = i;
			C.sucio = 1;
			if (b[i].tipo == UB_LISTA)
				abre_lista(b[i].id, b[i].x, b[i].y + b[i].al);
			else if (b[i].tipo == UB_COLOR && flecha)
				abre_lista(b[i].id == O_COLOR ? O_COLOR_MENU : b[i].id == O_RELLENO ? O_RELLENO_MENU : O_BORDES_MENU, b[i].x, b[i].y + b[i].al);
		} else if (s->clase == SO_SUBE) {
			int p = *pul;

			*pul = -1;
			C.sucio = 1;
			if (p >= 0 && p == i && b[i].tipo != UB_LISTA && !(b[i].tipo == UB_COLOR && flecha))
				cu_orden(b[i].id);
		}
		return;
	}
	if (encima1 >= 0 || encima2 >= 0 || pulsado1 >= 0 || pulsado2 >= 0) {
		encima1 = encima2 = pulsado1 = pulsado2 = -1;
		C.ayuda_encima = NULL;
		C.sucio = 1;
	}
	/* la barra de fórmulas */
	if (y_fx >= 0 && s->y >= y_fx && s->y < y_fx + al_fx) {
		if (s->clase != SO_BAJA)
			return;
		if (s->x < 4 + an_nombres) {
			/* el cuadro de nombres: la flecha, los nombres; si no, Ir a */
			if (s->x > 4 + an_nombres - al_fx && C.lib->n_nom) {
				char t[1600];
				u32 k;

				t[0] = 0;
				for (k = 0; k < C.lib->n_nom; k++) {
					if (k)
						anade(t, "|", sizeof t);
					anade(t, C.lib->nom[k].nombre, sizeof t);
				}
				so_menu(C.v, 4, y_fx + al_fx, t, 0, ETIQ_NOMBRES);
			} else
				dlg_ir_a();
			return;
		}
		{
			int bx = 4 + an_nombres + 6, lado = al_fx - 4, k = (s->x - bx) / lado;

			if (s->x >= bx && k >= 0 && k < 3) {
				if (k == 0 && C.editando)
					ed_cancela();
				else if (k == 1 && C.editando)
					ed_acepta(0);
				else if (k == 2)
					cu_orden(O_FUNCION);
				return;
			}
		}
		if (s->x >= x_texto_fx) {
			if (!C.editando) {
				char e[4096];

				lib_texto_edicion(C.lib, h, h->cur_f, h->cur_c, e, sizeof e);
				ed_empieza(e, 0, 2);
			}
			C.editando = 2;
			C.ed_nuevo = 0;
			/* el cursor, donde el clic */
			{
				u32 k;

				for (k = 0; k <= C.ed_n; k++) {
					char *p = utf8_de_u32(C.ed, k);
					int w = p ? lz_ui_ancho(p) : 0;

					so_suelta(p);
					if (x_texto_fx + 5 + w >= s->x) {
						break;
					}
				}
				C.ed_cur = C.ed_ancla = k > C.ed_n ? C.ed_n : k;
			}
			C.sucio = 1;
		}
		return;
	}
	/* las pestañas */
	if (s->y >= y_pie && s->y < y_pie + al_pie && s->x < an_pestanas) {
		int p = pestana_en(s->x, s->y);

		if (s->clase != SO_BAJA)
			return;
		if (C.editando && C.senala && p >= 0 && p < (int)C.lib->n_h) {
			/* (señalando: se puede pasar a otra hoja) */
			C.lib->activa = (u32)p;
			C.sucio = C.todo = 1;
			return;
		}
		if (C.editando && ed_acepta(0))
			return;
		if (p == -1)
			tab0 = 0;
		else if (p == -2 && tab0 > 0)
			tab0--;
		else if (p == -3 && tab0 + 1 < (int)C.lib->n_h)
			tab0++;
		else if (p == -4)
			tab0 = C.lib->n_h > (u32)n_pestanas_vistas ? (int)C.lib->n_h - n_pestanas_vistas : 0;
		else if (p == (int)C.lib->n_h)
			cu_orden(O_INS_HOJA);
		else if (p >= 0) {
			C.lib->activa = (u32)p;
			C.grafico = -1;
			if (s->botones & SO_BOTON_DER)
				menu_contexto(3, s->x, s->y);
			else if (s->veces >= 2)
				dlg_nombre_hoja();
		}
		C.sucio = C.todo = 1;
		return;
	}
	/* las barras de desplazamiento */
	if (s->clase == SO_BAJA) {
		int z = ui_desp_en(&desp_v, s->x, s->y), zh = ui_desp_en(&desp_h, s->x, s->y);

		if (z) {
			if (z == 5) {
				C.arrastra = AR_DESP_V;
				C.arr_y = s->y;
				C.arr_v0 = (i32)(h->vista_f - h->inm_f);
			} else {
				i64 v = (i64)h->vista_f + (z == 1 ? -1 : z == 2 ? 1 : z == 3 ? -(i64)vs_filas_vistas() : (i64)vs_filas_vistas());

				if (v < h->inm_f)
					v = h->inm_f;
				if (v >= CU_FILAS)
					v = CU_FILAS - 1;
				h->vista_f = (u32)v;
			}
			C.sucio = 1;
			return;
		}
		if (zh) {
			if (zh == 5) {
				C.arrastra = AR_DESP_H;
				C.arr_x = s->x;
				C.arr_v0 = (i32)(h->vista_c - h->inm_c);
			} else {
				i64 v = (i64)h->vista_c + (zh == 1 ? -1 : zh == 2 ? 1 : zh == 3 ? -(i64)vs_cols_vistas() : (i64)vs_cols_vistas());

				if (v < h->inm_c)
					v = h->inm_c;
				if (v >= CU_COLS)
					v = CU_COLS - 1;
				h->vista_c = (u32)v;
			}
			C.sucio = 1;
			return;
		}
	}
	if (C.previa) {
		if (s->clase == SO_BAJA) {
			C.pag_previa++;
			C.todo = C.sucio = 1;
		}
		return;
	}
	if (s->y >= y_hoja && s->y < y_hoja + al_hoja && s->x < an_hoja)
		raton_hoja(s);
}

static void rueda(const so_suceso *s)
{
	hoja *h = cu_hoja();

	if (C.cuadro) {
		if (ui_cuadro_suceso(C.cuadro, C.v, s))
			C.sucio = 1;
		return;
	}
	if (s->mods & SO_CON_CONTROL) {
		int z = h->zoom + (s->pasos > 0 ? 10 : -10) * (s->pasos > 0 ? s->pasos : -s->pasos);

		if (z < 10)
			z = 10;
		if (z > 400)
			z = 400;
		h->zoom = (u16)z;
		cu_aviso("Zoom: %d%%", h->zoom);
		C.todo = 1;
	} else if (s->mods & SO_CON_MAYUS) {
		i64 v = (i64)h->vista_c - s->pasos * 2;

		if (v < h->inm_c)
			v = h->inm_c;
		if (v >= CU_COLS)
			v = CU_COLS - 1;
		h->vista_c = (u32)v;
	} else {
		i64 v = (i64)h->vista_f - s->pasos * 3;

		if (v < h->inm_f)
			v = h->inm_f;
		if (v >= CU_FILAS)
			v = CU_FILAS - 1;
		h->vista_f = (u32)v;
	}
	C.sucio = 1;
}

static void menu_elegido(const so_suceso *s)
{
	u32 et = s->etiqueta;

	if (C.cuadro && et == UI_ETIQUETA_MENU) {
		ui_cuadro_suceso(C.cuadro, C.v, s);
		C.sucio = 1;
		return;
	}
	if (et >= ETIQ_MENU && et < ETIQ_MENU + 9) {
		int m = (int)(et - ETIQ_MENU), i;

		menu_abierto = -1;
		C.sucio = 1;
		if (s->opcion == -2) {
			abre_menu((m + 8) % 9);
			return;
		}
		if (s->opcion == -3) {
			abre_menu((m + 1) % 9);
			return;
		}
		if (s->opcion >= 0)
			for (i = 0; menus[m][i].texto; i++)
				if (i == s->opcion) {
					if (menus[m][i].orden != O_NADA)
						cu_orden(menus[m][i].orden);
					break;
				}
		return;
	}
	if (et == ETIQ_CONTEXTO) {
		elige_contexto(s->opcion);
		C.sucio = 1;
		return;
	}
	if (et == ETIQ_NOMBRES) {
		if (s->opcion >= 0 && (u32)s->opcion < C.lib->n_nom) {
			const formula *f = C.lib->nom[s->opcion].f;

			if (f && f->n == 1 && (f->t[0].t == T_REF || f->t[0].t == T_RANGO)) {
				int i = lib_hoja_indice(C.lib, f->t[0].h);

				if (i >= 0)
					C.lib->activa = (u32)i;
				cu_elige((u32)f->t[0].u.r.f0, f->t[0].u.r.c0, 0);
				cu_sel_rango(cu_rango((u32)f->t[0].u.r.f0, f->t[0].u.r.c0, (u32)f->t[0].u.r.f1, f->t[0].u.r.c1));
			}
		}
		C.sucio = 1;
		return;
	}
	if (et >= ETIQ_FILTRO && et < ETIQ_FILTRO + 0x100) {
		/* (dialogos.c lo lleva) */
		extern void dlg_filtro_elegido(int op);

		dlg_filtro_elegido(s->opcion);
		C.sucio = 1;
		return;
	}
	if (et >= ETIQ_LISTA && et < ETIQ_LISTA + O_N) {
		pulsado1 = pulsado2 = -1;
		elige_de_lista((int)(et - ETIQ_LISTA), s->opcion);
		C.sucio = 1;
	}
}

/* ===================================================================== */
/*  El principio                                                          */
/* ===================================================================== */
static int prepara_ventana(void)
{
	int an, al;

	so_pantalla(&an, &al);
	C.v = so_ventana("Cuentas", an * 9 / 10, al * 9 / 10);
	if (!C.v)
		return 0;
	so_maximiza(C.v);
	if (!lz_nuevo(&C.l, an * 9 / 10, al * 9 / 10))
		return 0;
	return 1;
}

static void cambia_tamano(int an, int al)
{
	if (an < 200 || al < 140 || (an == C.l.an && al == C.l.al))
		return;
	lz_suelta(&C.l);
	if (!lz_nuevo(&C.l, an, al))
		lz_nuevo(&C.l, 320, 200);
	C.sucio = C.todo = 1;
}

static void lee_letras_usuario(void)
{
	char r[64];
	char u = so_unidad_sistema();

	fmt(r, sizeof r, "%c:\\DOCUMENT\\LETRAS", u);
	letras_carpeta(r);
	fmt(r, sizeof r, "%c:\\LETRAS", u);
	letras_carpeta(r);
	fmt(r, sizeof r, "%c:\\HISPA\\LETRAS", u);
	letras_carpeta(r);
}

static void inicia_estado(void)
{
	memset(&C, 0, sizeof C);
	C.ver_barras = C.ver_formulas_barra = C.ver_estado = 1;
	C.color_letra = 0xC62828;
	C.color_relleno = 0xFFF176;
	C.grafico = -1;
	C.tab_col = -1;
	C.borde_elegido = 1;
	{
		char u = so_unidad_sistema();

		fmt(C.carpeta, sizeof C.carpeta, "%c:\\DOCUMENT", u);
		if (so_existe(C.carpeta) != 2)
			fmt(C.carpeta, sizeof C.carpeta, "%c:", u);
	}
}

static void bucle(void)
{
	so_suceso s;

	while (!C.sale) {
		int hay = so_lee_suceso(C.v, &s, !C.sucio);

		if (!hay) {
			if (C.sucio)
				cu_pinta();
			continue;
		}
		switch (s.clase) {
		case SO_TECLA: tecla(&s); break;
		case SO_BAJA:
		case SO_SUBE:
		case SO_MUEVE: raton(&s); break;
		case SO_RUEDA: rueda(&s); break;
		case SO_MENU: menu_elegido(&s); break;
		case SO_TAMANO: cambia_tamano(s.x, s.y); break;
		case SO_FOCO: C.sucio = 1; break;
		case SO_SALE:
			if (encima1 >= 0 || encima2 >= 0 || encima_menu >= 0) {
				encima1 = encima2 = encima_menu = -1;
				C.ayuda_encima = NULL;
				C.sucio = 1;
			}
			break;
		case SO_CERRAR:
			if (C.cuadro) {
				C.cuadro->cerrado = -1;
				C.sucio = 1;
			} else
				cu_orden(O_SALIR);
			break;
		}
		if (C.cuadro && C.cuadro->cerrado != -9999) {
			void (*f)(int) = C.al_cerrar;
			int id = C.cuadro->cerrado;

			C.cuadro = NULL;
			C.al_cerrar = NULL;
			if (f)
				f(id);
			C.sucio = C.todo = 1;
		}
	}
}

int cuentas_ventana(const char *archivo)
{
	inicia_estado();
	lee_letras_usuario();
	if (!prepara_ventana()) {
		dice("cuentas: no se puede abrir la ventana");
		return 1;
	}
	pon_libro(lib_nuevo(3), NULL, FMT_HOJ);
	if (!C.lib) {
		dice("cuentas: no hay memoria");
		return 1;
	}
	copia(C.lib->autor, "Hispa-Pentium", sizeof C.lib->autor);
	if (archivo && archivo[0])
		cu_abre(archivo);
	if (!archivo || !archivo[0])
		C.aviso[0] = 0;
	C.sucio = C.todo = 1;
	dice("cuentas: lista (%dx%d)", C.l.an, C.l.al);
	bucle();
	dice("cuentas: adiós");
	op_recorte_olvida();
	so_suelta(ultimo_copiado);
	ultimo_copiado = NULL;
	dh_suelta(C.lib);
	lib_suelta(C.lib);
	lz_suelta(&C.l);
	so_cierra_ventana(C.v);
	return 0;
}

/* ---- medir (CUENTAS --mide N): una hoja con N fórmulas que dependen unas
 * de otras, lo que tarda recalcularla entera, cambiar una celda de la que
 * dependen todas, y bajar con Av Pág ---- */
int cuentas_mide(int n)
{
	u32 t0, t1, hz = so_hz(), i, hechas;
	hoja *h;
	int ventana = 0, k;
	char e[100];

	inicia_estado();
	if (n < 10)
		n = 10;
	if (n > 200000)
		n = 200000;
	C.lib = lib_nuevo(1);
	if (!C.lib)
		return 1;
	h = C.lib->h[0];
	t0 = so_tics();
	/* A: números; B: =A*2+B(de antes); C: =SUMA($A$1:A); D: =SI(B>C;B-C;C-B)/2 */
	for (i = 0; i < (u32)n / 3 + 1; i++) {
		char t[96];

		lib_pon_numero(C.lib, h, i, 0, (double)(i % 97) + 0.5);
		fmt(t, sizeof t, i ? "=A%u*2+B%u/1000" : "=A%u*2", i + 1, i);
		lib_pon_entrada(C.lib, h, i, 1, t, e, sizeof e);
		fmt(t, sizeof t, "=SUMA($A$1:A%u)", i + 1);
		lib_pon_entrada(C.lib, h, i, 2, t, e, sizeof e);
		fmt(t, sizeof t, "=SI(B%u>C%u;B%u-C%u;C%u-B%u)/2", i + 1, i + 1, i + 1, i + 1, i + 1, i + 1);
		lib_pon_entrada(C.lib, h, i, 3, t, e, sizeof e);
	}
	t1 = so_tics();
	dice("cuentas: mide: hoja de %u fórmulas hecha en %u ms", C.lib->n_formulas, (t1 - t0) * 1000 / hz);
	t0 = so_tics();
	hechas = calc_todo(C.lib);
	t1 = so_tics();
	dice("cuentas: mide: recalcular todo: %u fórmulas en %u ms", hechas, (t1 - t0) * 1000 / hz);
	t0 = so_tics();
	lib_pon_numero(C.lib, h, 0, 0, 1234.5);
	hechas = calc_recalcula(C.lib);
	t1 = so_tics();
	dice("cuentas: mide: cambiar A1 (de la que dependen todas): %u fórmulas en %u ms", hechas, (t1 - t0) * 1000 / hz);
	t0 = so_tics();
	lib_pon_numero(C.lib, h, (u32)n / 3, 0, 7);
	hechas = calc_recalcula(C.lib);
	t1 = so_tics();
	dice("cuentas: mide: cambiar la última fila: %u fórmulas en %u ms", hechas, (t1 - t0) * 1000 / hz);
	/* la ventana, si hay escritorio */
	if (prepara_ventana()) {
		ventana = 1;
		lee_letras_usuario();
		cu_titulo();
		cu_pinta();
		t0 = so_tics();
		for (k = 0; k < 20; k++) {
			a_pagina(1, 0);
			cu_pinta();
		}
		t1 = so_tics();
		dice("cuentas: mide: 20 Av Pág en %u ms: %u ms cada una (pintar la ventana: %u ms)", (t1 - t0) * 1000 / hz,
		     (t1 - t0) * 1000 / hz / 20, tics_pintar * 1000 / hz);
		t0 = so_tics();
		for (k = 0; k < 20; k++) {
			mueve(1, 0, 0, 0);
			cu_pinta();
		}
		t1 = so_tics();
		dice("cuentas: mide: 20 flechas abajo en %u ms: %u ms cada una", (t1 - t0) * 1000 / hz, (t1 - t0) * 1000 / hz / 20);
		t0 = so_tics();
		for (k = 0; k < 10; k++) {
			char t[16];

			fmt(t, sizeof t, "%d", k * 3 + 1);
			op_entrada(C.lib, h, 0, 0, t, e, sizeof e);
			cu_pinta();
		}
		t1 = so_tics();
		dice("cuentas: mide: escribir en A1 y recalcular y pintar: %u ms cada vez", (t1 - t0) * 1000 / hz / 10);
	}
	dice("cuentas: mide: memoria %u KB", so_memoria_usada() / 1024);
	dh_suelta(C.lib);
	lib_suelta(C.lib);
	if (ventana) {
		lz_suelta(&C.l);
		so_cierra_ventana(C.v);
	}
	C.lib = NULL;
	return 0;
}
