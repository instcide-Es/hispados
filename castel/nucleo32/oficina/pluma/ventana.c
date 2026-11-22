/* ventana.c - Pluma 2: la ventana. Arriba, la barra de menús (los de
 * Carlos, al estilo de Word 6: lo que aún no hay sale en gris), la barra
 * de herramientas (archivo, edición, insertar) y la de formato (Estilo,
 * Fuente, Tamaño, negrita, cursiva, subrayado, colores, alineación, listas
 * y sangrías), y la regla; en medio, las páginas; abajo, la barra de
 * estado (página X de Y, palabras, idioma). Todo pintado en un lienzo
 * propio y dado al sistema de una vez (so_mapa).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include <stdarg.h>
#include "ventana.h"
#include "archivo.h"
#include "maqueta.h"
#include "letras.h"
#include "texto.h"
#include "pluma.h"

pluma_t P;

#define ETIQ_MENU 0x100			/* + el número del menú */
#define ETIQ_CONTEXTO 0x200
#define ETIQ_LISTA 0x300		/* + la orden de la lista */
#define CON_FLECHAS 0x40000000u

static const char *const MENUS = "&Archivo|&Edición|&Ver|&Insertar|&Formato|&Herramientas|&Tabla|Ve&ntana|&?";

typedef struct {
	const char *texto;
	int orden;
} opcion_t;

#define R { "-", O_NADA }
static const opcion_t m_archivo[] = {
	{ "Nuevo (Ctrl+U)", O_NUEVO }, { "Abrir... (Ctrl+A)", O_ABRIR }, { "Cerrar", O_CERRAR }, R,
	{ "Guardar (Ctrl+G)", O_GUARDAR }, { "Guardar como...", O_GUARDAR_COMO }, { "Guardar todo", O_GUARDAR_TODO },
	{ "Exportar a PDF...", O_EXPORTAR_PDF }, R, { "Buscar archivo...", O_BUSCAR_ARCHIVO }, { "Resumen...", O_RESUMEN },
	{ "Plantillas...", O_PLANTILLAS }, R, { "Preparar página...", O_PREPARAR }, { "Presentación preliminar", O_PREVIA },
	{ "Imprimir... (Ctrl+P)", O_IMPRIMIR }, R, { "Salir", O_SALIR }, { NULL, 0 }
};
static const opcion_t m_edicion[] = {
	{ "Deshacer (Ctrl+Z)", O_DESHACER }, { "Repetir (Ctrl+Y)", O_REHACER }, R, { "Cortar (Ctrl+X)", O_CORTAR },
	{ "Copiar (Ctrl+C)", O_COPIAR }, { "Pegar (Ctrl+V)", O_PEGAR }, { "Pegado especial (sin formato)", O_PEGAR_SIN },
	{ "Borrar (Supr)", O_BORRAR }, { "Seleccionar todo (Ctrl+E)", O_TODO }, R, { "Buscar... (Ctrl+B)", O_BUSCAR },
	{ "Reemplazar... (Ctrl+L)", O_REEMPLAZAR }, { "Ir a... (F5)", O_IR_A }, { "Autotexto...", O_AUTOTEXTO },
	{ "Marcador...", O_MARCADOR }, R, { "Vínculos", O_VINCULOS }, { "Objeto", O_OBJETO }, { NULL, 0 }
};
static const opcion_t m_ver[] = {
	{ "Normal (diseño de página)", O_NORMAL }, { "Esquema", O_ESQUEMA }, { "Documento maestro", O_MAESTRO }, R,
	{ "Pantalla completa", O_COMPLETA }, R, { "Barras de herramientas", O_BARRAS }, { "Regla", O_REGLA }, R,
	{ "Encabezado y pie de página...", O_ENC_PIE }, { "Notas al pie", O_NOTAS }, { "Anotaciones", O_ANOTACIONES }, R,
	{ "Marcas de formato (¶)", O_MARCAS }, { "Zoom...", O_ZOOM }, { NULL, 0 }
};
static const opcion_t m_insertar[] = {
	{ "Salto...", O_SALTO }, { "Anotación", O_ANOTACION }, { "Fecha y hora...", O_FECHA }, { "Campo...", O_CAMPO },
	{ "Símbolo...", O_SIMBOLO }, { "Campo de formulario...", O_FORMULARIO }, R, { "Nota al pie", O_NOTA },
	{ "Epígrafe...", O_EPIGRAFE }, { "Referencia cruzada...", O_REFERENCIA }, { "Índice y tablas...", O_INDICE }, R,
	{ "Archivo...", O_INS_ARCHIVO }, { "Marco", O_MARCO }, { "Imagen...", O_IMAGEN }, { "Objeto...", O_INS_OBJETO },
	{ "Base de datos...", O_BASE_DATOS }, { NULL, 0 }
};
static const opcion_t m_formato[] = {
	{ "Fuentes...", O_FUENTES }, { "Párrafo...", O_PARRAFO }, { "Tabulaciones...", O_TABULACIONES },
	{ "Bordes y sombreado...", O_BORDES }, { "Columnas...", O_COLUMNAS }, { "Cambiar mayús/minús...", O_MAYUS },
	{ "Letra capital...", O_CAPITAL }, R, { "Numeración y viñetas...", O_LISTAS }, { "Numeración de títulos...", O_NUM_TITULOS }, R,
	{ "Autoformato...", O_AUTOFORMATO }, { "Galería de estilos...", O_GALERIA }, { "Estilo...", O_ESTILO }, R,
	{ "Marco...", O_FMT_MARCO }, { "Imagen...", O_FMT_IMAGEN }, { "Dibujar objetos", O_DIBUJAR }, { NULL, 0 }
};
static const opcion_t m_herramientas[] = {
	{ "Ortografía... (F7)", O_ORTOGRAFIA }, { "Sinónimos...", O_SINONIMOS }, { "Guiones...", O_GUIONES },
	{ "Idioma...", O_IDIOMA }, { "Contar palabras...", O_CONTAR }, R, { "Autocorrección...", O_AUTOCORRECCION },
	{ "Combinar correspondencia...", O_COMBINAR }, { "Sobres y etiquetas...", O_SOBRES }, R,
	{ "Proteger documentos...", O_PROTEGER }, { "Revisiones...", O_REVISIONES }, R, { "Macros...", O_MACROS },
	{ "Personalizar...", O_PERSONALIZAR }, { "Opciones...", O_OPCIONES }, { NULL, 0 }
};
static const opcion_t m_tabla[] = {
	{ "Insertar tabla...", O_TABLA }, { "Insertar fila encima", O_FILA_ARRIBA }, { "Insertar fila debajo", O_FILA_ABAJO },
	{ "Insertar columna a la izquierda", O_COL_IZQ }, { "Insertar columna a la derecha", O_COL_DER },
	{ "Eliminar celdas...", O_ELIMINAR_CELDAS }, { "Unir celdas", O_UNIR }, { "Dividir celdas", O_DIVIDIR }, R,
	{ "Seleccionar fila", O_SEL_FILA }, { "Seleccionar columna", O_SEL_COLUMNA }, { "Seleccionar tabla", O_SEL_TABLA }, R,
	{ "Autoformato de tablas...", O_AUTOFORMATO_TABLA }, { "Alto y ancho de celda...", O_ANCHO_CELDA },
	{ "Títulos (la primera fila)", O_TITULOS }, R, { "Convertir texto en tabla...", O_TEXTO_A_TABLA },
	{ "Ordenar texto...", O_ORDENAR }, { "Fórmula...", O_FORMULA }, { "Dividir tabla", O_DIVIDIR_TABLA },
	{ "Cuadrícula", O_CUADRICULA }, { NULL, 0 }
};
static const opcion_t m_ventana[] = {
	{ "Nueva ventana", O_NUEVA_VENTANA }, { "Organizar todo", O_ORGANIZAR }, { "Dividir", O_DIVIDIR_VENTANA }, R,
	{ "1 Documento1", O_DOCUMENTO1 }, { NULL, 0 }
};
static const opcion_t m_ayuda[] = {
	{ "Contenido (F1)", O_CONTENIDO }, { "Buscar ayuda acerca de...", O_BUSCAR_AYUDA }, R, { "Introducción", O_INTRODUCCION },
	{ "Ejemplos y demos", O_EJEMPLOS }, { "Sugerencias", O_SUGERENCIAS }, R, { "Ayuda para Pluma", O_AYUDA_PLUMA }, R,
	{ "Acerca de Pluma", O_ACERCA }, { NULL, 0 }
};
static const opcion_t *const menus[9] = { m_archivo, m_edicion, m_ver, m_insertar, m_formato, m_herramientas, m_tabla, m_ventana, m_ayuda };

/* las barras */
static ui_boton barra1[] = {
	{ O_NUEVO, UB_BOTON, IC_NUEVO, "Nuevo documento (Ctrl+U)" },
	{ O_ABRIR, UB_BOTON, IC_ABRIR, "Abrir un documento (Ctrl+A)" },
	{ O_GUARDAR, UB_BOTON, IC_GUARDAR, "Guardar (Ctrl+G)" },
	{ 0, UB_SEPARA },
	{ O_IMPRIMIR, UB_BOTON, IC_IMPRIMIR, "Imprimir: aún no hay impresoras; ofrece exportar a PDF (Ctrl+P)" },
	{ O_PREVIA, UB_BOTON, IC_VISTA_PREVIA, "Presentación preliminar" },
	{ O_ORTOGRAFIA, UB_BOTON, IC_ORTOGRAFIA, "Ortografía (F7)" },
	{ 0, UB_SEPARA },
	{ O_CORTAR, UB_BOTON, IC_CORTAR, "Cortar (Ctrl+X)" },
	{ O_COPIAR, UB_BOTON, IC_COPIAR, "Copiar (Ctrl+C)" },
	{ O_PEGAR, UB_BOTON, IC_PEGAR, "Pegar (Ctrl+V)" },
	{ O_BROCHA, UB_CONMUTA, IC_BROCHA, "Copiar formato: elija lo que quiere cambiar" },
	{ 0, UB_SEPARA },
	{ O_DESHACER, UB_BOTON, IC_DESHACER, "Deshacer (Ctrl+Z)" },
	{ O_REHACER, UB_BOTON, IC_REHACER, "Rehacer (Ctrl+Y)" },
	{ 0, UB_SEPARA },
	{ O_TABLA, UB_BOTON, IC_TABLA, "Insertar tabla" },
	{ O_IMAGEN, UB_BOTON, IC_IMAGEN, "Insertar imagen" },
	{ O_SALTO_PAGINA, UB_BOTON, IC_SALTO, "Salto de página (Ctrl+R)" },
	{ O_EXPORTAR_PDF, UB_BOTON, IC_PDF, "Exportar a PDF" },
	{ 0, UB_SEPARA },
	{ O_MARCAS, UB_CONMUTA, IC_MARCAS, "Ver las marcas de formato (¶)" },
	{ O_BUSCAR, UB_BOTON, IC_BUSCAR, "Buscar (Ctrl+B)" },
	{ O_LISTA_ZOOM, UB_LISTA, 0, "Zoom (Ctrl+rueda del ratón)", 80 },
};
#define N1 ((int)(sizeof barra1 / sizeof barra1[0]))
static ui_boton barra2[] = {
	{ O_LISTA_ESTILO, UB_LISTA, 0, "Estilo del párrafo", 138 },
	{ O_LISTA_FUENTE, UB_LISTA, 0, "Fuente (las letras TrueType: las DejaVu y las de \\DOCUMENT\\LETRAS)", 160 },
	{ O_LISTA_TAMANO, UB_LISTA, 0, "Tamaño de la letra (puntos)", 56 },
	{ 0, UB_SEPARA },
	{ O_NEGRITA, UB_CONMUTA, IC_NEGRITA, "Negrita (Ctrl+N)" },
	{ O_CURSIVA, UB_CONMUTA, IC_CURSIVA, "Cursiva (Ctrl+K)" },
	{ O_SUBRAYADO, UB_CONMUTA, IC_SUBRAYADO, "Subrayado (Ctrl+S)" },
	{ O_COLOR, UB_COLOR, IC_COLOR_LETRA, "Color de la letra (la flecha: elegir otro)" },
	{ O_RESALTE, UB_COLOR, IC_RESALTE, "Resaltar (la flecha: elegir el color)" },
	{ 0, UB_SEPARA },
	{ O_IZQ, UB_CONMUTA, IC_IZQUIERDA, "Alinear a la izquierda (Ctrl+Q)" },
	{ O_CENTRO, UB_CONMUTA, IC_CENTRO, "Centrar (Ctrl+T)" },
	{ O_DER, UB_CONMUTA, IC_DERECHA, "Alinear a la derecha (Ctrl+D)" },
	{ O_JUST, UB_CONMUTA, IC_JUSTIFICADO, "Justificar" },
	{ 0, UB_SEPARA },
	{ O_NUMEROS, UB_CONMUTA, IC_NUMEROS, "Numeración" },
	{ O_VINETAS, UB_CONMUTA, IC_VINETAS, "Viñetas" },
	{ O_SANGRIA_MENOS, UB_BOTON, IC_SANGRIA_MENOS, "Reducir la sangría (Mayús+Tab)" },
	{ O_SANGRIA_MAS, UB_BOTON, IC_SANGRIA_MAS, "Aumentar la sangría (Tab al principio)" },
	{ O_BORDES, UB_BOTON, IC_BORDES, "Bordes de la tabla" },
};
#define N2 ((int)(sizeof barra2 / sizeof barra2[0]))

static const char *const TAMANOS = "8|9|10|11|12|14|16|18|20|22|24|26|28|36|48|72";
static const char *const ZOOMS = "50%|75%|100%|125%|150%|200%|Ancho de página|Página entera";
static const char *const COLORES = "Automático (negro)|Rojo|Granate|Naranja|Amarillo oscuro|Verde|Verde azulado|Azul|Azul marino|Morado|Gris|Gris oscuro";
static const u32 colores_v[] = { SIN_COLOR, 0xC62828, 0x7B1E1E, 0xE65100, 0xB38B00, 0x2E7D32, 0x00796B, 0x1565C0, 0x1A237E, 0x6A1B9A, 0x757575, 0x424242 };
static const char *const RESALTES = "Sin resaltar|Amarillo|Verde claro|Turquesa|Rosa|Azul claro|Gris claro|Naranja claro";
static const u32 resaltes_v[] = { SIN_COLOR, 0xFFF176, 0xC5E1A5, 0x80DEEA, 0xF8BBD0, 0xBBDEFB, 0xE0E0E0, 0xFFCC80 };

/* la geometría (la rehace coloca()) */
static int y_b1, y_b2, y_regla, y_doc, al_doc, y_estado, an_doc;
static ui_desp desp_v, desp_h;
static int hay_h;
static int encima1 = -1, encima2 = -1, encima_menu = -1, pulsado1 = -1, pulsado2 = -1;
static int menu_abierto = -1;
static const char *ayuda_encima;
/* el ratón */
static int arrastra_doc, arrastra_desp, arrastra_regla, arrastra_img;
static int arr_de, cual_tab;
static i32 arr_pos0;
static i32 regla_valor;
static pos pos_clic;
static int veces_clic;
static ui_regla regla;
/* contar palabras sin parar (cada segundo, si ha cambiado) */
static u32 palabras, version_palabras = 0xFFFFFFFF, tics_palabras;
/* (para medir) */
static u32 tics_pintar;

void pl_aviso(const char *f, ...)
{
	va_list a;

	va_start(a, f);
	vfmt(P.aviso, sizeof P.aviso, f, a);
	va_end(a);
	dice("pluma: aviso: %s", P.aviso);
}

void pl_titulo(void)
{
	char t[300];

	fmt(t, sizeof t, "%s%s - Pluma", P.ruta[0] ? sin_carpeta(P.ruta) : "Documento1", P.d && P.d->cambiado ? " *" : "");
	so_titulo(P.v, t);
}

static int en_tabla(void)
{
	return P.d->par[P.e.cur.p].p.tabla != 0;
}

/* ¿se puede hacer la orden ahora? (si no, en gris) */
static int activa(int o)
{
	pos a, b;
	int hay = ed_eleccion(&P.e, &a, &b);

	switch (o) {
	case O_GUARDAR_TODO:
	case O_GUARDAR: return 1;
	case O_DESHACER: return ed_se_deshace(&P.e);
	case O_REHACER: return ed_se_rehace(&P.e);
	case O_CORTAR: case O_COPIAR: case O_BORRAR: return hay;
	case O_FILA_ARRIBA: case O_FILA_ABAJO: case O_COL_IZQ: case O_COL_DER: case O_ELIMINAR_CELDAS:
	case O_SEL_FILA: case O_SEL_TABLA: case O_TITULOS: case O_CUADRICULA: case O_ANCHO_CELDA: case O_BORDES:
		return en_tabla();
	case O_UNIR: return hay && P.d->par[a.p].p.tabla && P.d->par[a.p].p.tabla == P.d->par[b.p].p.tabla && a.p != b.p;
	case O_DIVIDIR: return en_tabla() && (P.d->par[P.e.cur.p].p.abarca_cols > 1 || P.d->par[P.e.cur.p].p.abarca_filas > 1);
	case O_TABLA: return !en_tabla();
	case O_FMT_IMAGEN: return ed_imagen_elegida(&P.e) >= 0;
	case O_ORTOGRAFIA: return pl_orto_hay();
	/* lo que aún no hay */
	case O_BUSCAR_ARCHIVO: case O_PLANTILLAS: case O_AUTOTEXTO: case O_MARCADOR: case O_VINCULOS: case O_OBJETO:
	case O_ESQUEMA: case O_MAESTRO: case O_ANOTACIONES: case O_ANOTACION: case O_FORMULARIO: case O_EPIGRAFE:
	case O_REFERENCIA: case O_INDICE: case O_MARCO: case O_INS_OBJETO: case O_BASE_DATOS: case O_COLUMNAS:
	case O_CAPITAL: case O_NUM_TITULOS: case O_AUTOFORMATO: case O_GALERIA: case O_FMT_MARCO: case O_DIBUJAR:
	case O_SINONIMOS: case O_GUIONES: case O_AUTOCORRECCION: case O_COMBINAR: case O_SOBRES: case O_PROTEGER:
	case O_REVISIONES: case O_MACROS: case O_PERSONALIZAR: case O_SEL_COLUMNA: case O_AUTOFORMATO_TABLA:
	case O_TEXTO_A_TABLA: case O_ORDENAR: case O_FORMULA: case O_DIVIDIR_TABLA: case O_ORGANIZAR:
	case O_DIVIDIR_VENTANA: case O_BUSCAR_AYUDA: case O_INTRODUCCION: case O_EJEMPLOS: case O_SUGERENCIAS:
	case O_NOTAS:
		return 0;
	}
	return 1;
}

/* el texto de un menú con sus grises */
static void texto_menu(int m, char *t, u32 cabe, u32 *grises)
{
	const opcion_t *o = menus[m];
	int i;

	t[0] = 0;
	*grises = 0;
	for (i = 0; o[i].texto; i++) {
		char x[120];

		if (i)
			anade(t, "|", cabe);
		copia(x, o[i].texto, sizeof x);
		if (o[i].orden == O_DOCUMENTO1)
			fmt(x, sizeof x, "1 %s", P.ruta[0] ? sin_carpeta(P.ruta) : "Documento1");
		/* (lo que se enciende y se apaga, con su marca) */
		if ((o[i].orden == O_MARCAS && P.vi.marcas) || (o[i].orden == O_REGLA && P.ver_regla) || (o[i].orden == O_BARRAS && P.ver_barras) ||
		    (o[i].orden == O_COMPLETA && P.completa) || (o[i].orden == O_PREVIA && P.vi.previa) ||
		    (o[i].orden == O_TITULOS && en_tabla() && P.d->tab[P.d->par[P.e.cur.p].p.tabla - 1].cabecera) ||
		    (o[i].orden == O_CUADRICULA && en_tabla() && P.d->tab[P.d->par[P.e.cur.p].p.tabla - 1].bordes == BORDE_TODOS)) {
			char y[124];

			fmt(y, sizeof y, "\xe2\x9c\x93 %s", x);	/* ✓ (en la 437 sale «?»: se pone «»» */
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
	char t[1100];
	u32 g;
	int x0 = 0;
	char r[64];

	if (m < 0 || m > 8)
		return;
	/* dónde empieza su rótulo */
	{
		int i, cx = 6;

		for (i = 0; i <= m; i++) {
			const char *s = MENUS;
			int k = 0;
			u32 j = 0;

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
	}
	texto_menu(m, t, sizeof t, &g);
	menu_abierto = m;
	so_menu(P.v, x0, ui_alto_menus(), t, g, (ETIQ_MENU + (u32)m) | CON_FLECHAS);
	P.sucio = 1;
}

/* ---- la geometría ---- */
static void coloca(void)
{
	int y = ui_alto_menus();

	if (P.completa) {
		y = 0;
		y_b1 = y_b2 = y_regla = -1000;
	} else {
		if (P.ver_barras) {
			y_b1 = y;
			y += ui_alto_barra();
			y_b2 = y;
			y += ui_alto_barra();
		} else
			y_b1 = y_b2 = -1000;
		if (P.ver_regla && !P.vi.previa) {
			y_regla = y;
			y += ui_alto_regla();
		} else
			y_regla = -1000;
	}
	y_doc = y;
	y_estado = P.l.al - (P.completa ? 0 : ui_alto_estado());
	an_doc = P.l.an - UI_DESP;
	hay_h = vista_ancho_total(&P.vi) > an_doc;
	al_doc = y_estado - y_doc - (hay_h ? UI_DESP : 0);
	if (al_doc < 40)
		al_doc = 40;
	P.vi.x = 0;
	P.vi.y = y_doc;
	P.vi.an = an_doc;
	P.vi.al = al_doc;
	desp_v.x = an_doc;
	desp_v.y = y_doc;
	desp_v.an = UI_DESP;
	desp_v.al = al_doc;
	desp_h.x = 0;
	desp_h.y = y_doc + al_doc;
	desp_h.an = an_doc;
	desp_h.al = UI_DESP;
	/* las listas, más estrechas en una pantalla pequeña */
	if (P.l.an < 900) {
		ui_boton_id(barra2, N2, O_LISTA_ESTILO)->ancho = 104;
		ui_boton_id(barra2, N2, O_LISTA_FUENTE)->ancho = 118;
	} else {
		ui_boton_id(barra2, N2, O_LISTA_ESTILO)->ancho = 140;
		ui_boton_id(barra2, N2, O_LISTA_FUENTE)->ancho = 170;
	}
}

/* lo de las barras según el cursor */
static void estado_barras(void)
{
	fmt_letra f = ed_formato_actual(&P.e);
	const parrafo *p = &P.d->par[P.e.cur.p];
	ui_boton *b;

	ui_boton_id(barra2, N2, O_NEGRITA)->activo = f.negrita;
	ui_boton_id(barra2, N2, O_CURSIVA)->activo = f.cursiva;
	ui_boton_id(barra2, N2, O_SUBRAYADO)->activo = f.subrayado;
	ui_boton_id(barra2, N2, O_IZQ)->activo = p->p.alin == AL_IZQ;
	ui_boton_id(barra2, N2, O_CENTRO)->activo = p->p.alin == AL_CENTRO;
	ui_boton_id(barra2, N2, O_DER)->activo = p->p.alin == AL_DER;
	ui_boton_id(barra2, N2, O_JUST)->activo = p->p.alin == AL_JUST;
	ui_boton_id(barra2, N2, O_VINETAS)->activo = p->p.lista == LISTA_VINETAS;
	ui_boton_id(barra2, N2, O_NUMEROS)->activo = p->p.lista >= LISTA_NUMEROS;
	ui_boton_id(barra2, N2, O_BORDES)->apagado = !en_tabla();
	b = ui_boton_id(barra2, N2, O_LISTA_ESTILO);
	copia(b->texto, p->p.estilo < P.d->n_est ? P.d->est[p->p.estilo].nombre : "Normal", sizeof b->texto);
	b = ui_boton_id(barra2, N2, O_LISTA_FUENTE);
	copia(b->texto, f.familia, sizeof b->texto);
	b = ui_boton_id(barra2, N2, O_LISTA_TAMANO);
	if (f.tam & 1)
		fmt(b->texto, sizeof b->texto, "%u,5", f.tam / 2);
	else
		fmt(b->texto, sizeof b->texto, "%u", f.tam / 2);
	ui_boton_id(barra2, N2, O_COLOR)->color = P.color_letra == SIN_COLOR ? 0x000000 : P.color_letra;
	ui_boton_id(barra2, N2, O_RESALTE)->color = P.color_resalte;
	ui_boton_id(barra1, N1, O_DESHACER)->apagado = !ed_se_deshace(&P.e);
	ui_boton_id(barra1, N1, O_REHACER)->apagado = !ed_se_rehace(&P.e);
	ui_boton_id(barra1, N1, O_ORTOGRAFIA)->apagado = !pl_orto_hay();
	ui_boton_id(barra1, N1, O_MARCAS)->activo = P.vi.marcas;
	ui_boton_id(barra1, N1, O_BROCHA)->activo = P.con_brocha;
	ui_boton_id(barra1, N1, O_TABLA)->apagado = en_tabla();
	{
		pos a, c;
		int hay = ed_eleccion(&P.e, &a, &c);

		ui_boton_id(barra1, N1, O_CORTAR)->apagado = !hay;
		ui_boton_id(barra1, N1, O_COPIAR)->apagado = !hay;
	}
	fmt(ui_boton_id(barra1, N1, O_LISTA_ZOOM)->texto, 64, "%d%%", P.vi.zoom);
}

/* la regla, con lo del párrafo del cursor */
static void datos_regla(void)
{
	const parrafo *p = &P.d->par[P.e.cur.p];
	int px, py, k;

	memset(&regla, 0, sizeof regla);
	regla.ancho_pag = P.d->pag_an;
	regla.marg_izq = P.d->marg_izq;
	regla.marg_der = P.d->marg_der;
	regla.sang_izq = p->p.sang_izq;
	regla.sang_der = p->p.sang_der;
	regla.sang_primera = p->p.sang_primera;
	for (k = 0; k < p->p.n_tabs && k < 16; k++) {
		regla.tabs[k] = p->p.tabs[k].pos;
		regla.tipo_tab[k] = p->p.tabs[k].tipo;
	}
	regla.n_tabs = p->p.n_tabs;
	if (p->p.tabla && p->p.tabla <= P.d->n_tab) {
		regla.inicio = maq_x_col(P.d, p->p.tabla - 1, p->p.col) + P.d->tab[p->p.tabla - 1].margen;
		regla.fin = regla.inicio + maq_ancho_celda(P.d, p);
	}
	vista_sitio_pag(&P.vi, P.d->n_pag ? vista_pagina_cursor(&P.vi) : 0, &px, &py);
	regla.x_pag = px;
	regla.zoom = P.vi.zoom;
	regla.tab_nuevo = arrastra_regla == RG_TIPO ? 0 : regla.tab_nuevo;
}

static int tab_tipo = TAB_IZQ;

static void texto_estado(char *t, u32 cabe)
{
	u32 pg = vista_pagina_cursor(&P.vi);
	const parrafo *p = &P.d->par[P.e.cur.p];
	u32 l = p->n_lin ? maq_linea_de(p, P.e.cur.o, 0) : 0;
	char pal[24];

	/* (la línea, en la página: como los procesadores de siempre) */
	{
		u32 pg2, tz;

		if (maq_donde(P.d, P.e.cur.p, l, &pg2, &tz) == 0) {
			u32 k, n = 0;

			for (k = P.d->pag[pg2].t0; k < tz; k++)
				n += P.d->trz[k].fila_fin ? 1 : P.d->trz[k].lin1 - P.d->trz[k].lin0;
			l = n + (P.d->trz[tz].fila_fin ? l : l - P.d->trz[tz].lin0);
		}
	}
	if (P.d->version != version_palabras && (so_tics() - tics_palabras > so_hz() || version_palabras == 0xFFFFFFFF || P.d->n_par < 400)) {
		u32 le, se, pa;

		ed_cuenta(P.d, &palabras, &le, &se, &pa);
		version_palabras = P.d->version;
		tics_palabras = so_tics();
	}
	if (palabras >= 1000)
		fmt(pal, sizeof pal, "%u.%03u", palabras / 1000, palabras % 1000);
	else
		fmt(pal, sizeof pal, "%u", palabras);
	fmt(t, cabe, "Página %u de %u|Línea %u|%s palabras|Español%s|%s|%d%%", pg + P.d->num_inicio, P.d->n_pag + P.d->num_inicio - 1,
	    l + 1, pal, pl_orto_hay() && P.orto_al_escribir ? " \xe2\x9c\x93" : "",
	    ayuda_encima ? ayuda_encima : P.aviso, P.vi.zoom);
}

/* la «firma» de lo que se ve de algo: si no cambia, no se vuelve a pintar */
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

static u32 f_menus, f_b1, f_b2, f_regla;
static int geometria_vista = -1;

/* pinta lo que ha cambiado (con «todo», todo) */
void pl_pinta_todo(void)
{
	char t[400];
	u32 t0 = so_tics(), f;
	int y0 = P.l.al, todo;

	if (!P.l.p)
		return;
	maq_documento(P.d);
	coloca();
	estado_barras();
	todo = P.cuadro != NULL || geometria_vista != (y_doc << 16 ^ P.l.an ^ P.l.al << 8) || P.todo;
	geometria_vista = y_doc << 16 ^ P.l.an ^ P.l.al << 8;
	P.todo = P.cuadro != NULL;
	if (!P.completa) {
		f = firma(firma(firma(2166136261u, &menu_abierto, sizeof menu_abierto), &encima_menu, sizeof encima_menu), &P.l.an, sizeof P.l.an);
		if (todo || f != f_menus) {
			ui_barra_menus(&P.l, 0, 0, P.l.an, MENUS, menu_abierto, encima_menu);
			lz_raya_h(&P.l, 0, ui_alto_menus() - 1, P.l.an, UI_RAYA);
			f_menus = f;
			y0 = 0;
		}
		if (P.ver_barras) {
			f = firma_barra(barra1, N1, encima1, pulsado1);
			if (todo || f != f_b1) {
				ui_barra(&P.l, 0, y_b1, P.l.an, barra1, N1, encima1, pulsado1);
				f_b1 = f;
				if (y_b1 < y0)
					y0 = y_b1;
			}
			f = firma_barra(barra2, N2, encima2, pulsado2);
			if (todo || f != f_b2) {
				ui_barra(&P.l, 0, y_b2, P.l.an, barra2, N2, encima2, pulsado2);
				f_b2 = f;
				if (y_b2 < y0)
					y0 = y_b2;
			}
		}
		if (y_regla >= 0) {
			datos_regla();
			regla.tab_nuevo = tab_tipo;
			if (arrastra_regla == RG_PRIMERA || arrastra_regla == RG_IZQ || arrastra_regla == RG_DER || arrastra_regla == RG_TAB) {
				/* lo que se arrastra, donde va */
				if (arrastra_regla == RG_PRIMERA)
					regla.sang_primera = regla_valor - regla.sang_izq;
				else if (arrastra_regla == RG_IZQ) {
					regla.sang_primera = regla.sang_primera + regla.sang_izq - regla_valor;
					regla.sang_izq = regla_valor;
				} else if (arrastra_regla == RG_DER)
					regla.sang_der = regla_valor;
				else if (cual_tab >= 0 && cual_tab < regla.n_tabs)
					regla.tabs[cual_tab] = regla_valor;
			}
			f = firma(firma(firma(2166136261u, &regla, sizeof regla), &arrastra_regla, sizeof arrastra_regla), &regla_valor, sizeof regla_valor);
			if (todo || f != f_regla) {
				ui_regla_pinta(&P.l, 0, y_regla, P.l.an, &regla, arrastra_regla);
				f_regla = f;
				if (y_regla < y0)
					y0 = y_regla;
			}
		}
	}
	if (y_doc < y0)
		y0 = y_doc;
	if (todo)
		y0 = 0;
	vista_pinta(&P.vi, &P.l);
	if (arrastra_regla == RG_PRIMERA || arrastra_regla == RG_IZQ || arrastra_regla == RG_DER || arrastra_regla == RG_TAB) {
		/* la raya que sigue a lo que se arrastra, por la página */
		int x = ui_regla_px(&regla, regla.inicio + (arrastra_regla == RG_DER ? (regla.fin > regla.inicio ? regla.fin - regla.inicio : P.d->pag_an - P.d->marg_izq - P.d->marg_der) - regla_valor : regla_valor));

		lz_punteada_v(&P.l, x, y_doc, al_doc, 0x1B2633);
	}
	desp_v.total = vista_alto_total(&P.vi);
	desp_v.visto = al_doc;
	desp_v.pos = P.vi.dy;
	ui_desp_pinta(&P.l, &desp_v, arrastra_desp == 1 ? 5 : 0);
	if (hay_h) {
		desp_h.total = vista_ancho_total(&P.vi);
		desp_h.visto = an_doc;
		desp_h.pos = P.vi.dx;
		ui_desp_pinta(&P.l, &desp_h, arrastra_desp == 2 ? 5 : 0);
		lz_rect(&P.l, an_doc, y_doc + al_doc, UI_DESP, UI_DESP, UI_FONDO);
	} else
		lz_rect(&P.l, an_doc, y_estado - 1, UI_DESP, 1, UI_FONDO);
	if (!P.completa) {
		texto_estado(t, sizeof t);
		ui_estado(&P.l, 0, y_estado, P.l.an, t);
	}
	if (P.cuadro)
		ui_cuadro_pinta(&P.l, P.cuadro);
	/* al sistema, de y0 para abajo (filas enteras: sin copiar) */
	so_mapa(P.v, 0, y0, P.l.an, P.l.al - y0, P.l.p + y0 * P.l.paso, P.l.paso);
	P.sucio = 0;
	tics_pintar = so_tics() - t0;
}

/* ---- abrir y guardar ---- */
static void pon_documento(documento *d, const char *ruta, int formato)
{
	if (P.d) {
		ed_suelta(&P.e);
		doc_suelta(P.d);
	}
	P.d = d;
	ed_inicia(&P.e, d);
	{
		vista v0 = P.vi;

		vista_inicia(&P.vi, d, &P.e);
		P.vi.zoom = v0.zoom ? v0.zoom : 100;
		P.vi.marcas = v0.marcas;
		P.vi.faltas = pl_orto_hay() && P.orto_al_escribir ? pl_orto_faltas : NULL;
	}
	copia(P.ruta, ruta ? ruta : "", sizeof P.ruta);
	P.formato = formato;
	version_palabras = 0xFFFFFFFF;
	maq_documento(d);
	pl_titulo();
}

int pl_abre(const char *ruta)
{
	char e[300];
	int f;
	documento *d = arch_abre(ruta, &f, e, sizeof e);

	if (!d) {
		dlg_aviso("No se puede abrir", e);
		return -1;
	}
	pon_documento(d, ruta, f);
	{
		u32 i, k = 0;

		/* la carpeta, para la vez siguiente */
		copia(P.carpeta, ruta, sizeof P.carpeta);
		for (i = 0; P.carpeta[i]; i++)
			if (P.carpeta[i] == '\\' || P.carpeta[i] == '/')
				k = i;
		P.carpeta[k] = 0;
	}
	pl_aviso("Abierto: %s (%s, %u páginas)", sin_carpeta(ruta), arch_extension(f), P.d->n_pag);
	dice("pluma: abierto %s: %u párrafos, %u páginas", ruta, P.d->n_par, P.d->n_pag);
	P.sucio = 1;
	return 0;
}

int pl_guarda(const char *ruta, int formato)
{
	char e[300], real[600];

	if (arch_guarda(P.d, ruta, formato, real, sizeof real, e, sizeof e)) {
		dlg_aviso("No se ha podido guardar", e);
		return -1;
	}
	if (formato == FMT_PDF) {
		pl_aviso("Exportado a PDF: %s (%u páginas)", sin_carpeta(real), P.d->n_pag);
		dice("pluma: pdf %s", real);
		P.sucio = 1;
		return 0;
	}
	copia(P.ruta, real, sizeof P.ruta);
	P.formato = formato;
	P.e.version_guardada = P.d->version;
	if (e[0])
		pl_aviso("%s", e);
	else if (!iguales_sin_caja(sin_carpeta(real), sin_carpeta(ruta)))
		pl_aviso("Guardado como %s (el sistema aún crea solo nombres 8+3)", sin_carpeta(real));
	else
		pl_aviso("Guardado: %s", sin_carpeta(real));
	dice("pluma: guardado %s (%s)", real, arch_extension(formato));
	pl_titulo();
	P.sucio = 1;
	return 0;
}

void pl_cambio(void)
{
	P.vi.sigue = 1;
	P.vi.x_deseada = -1;
	P.sucio = 1;
	if (P.d->sin_memoria) {
		P.d->sin_memoria = 0;
		pl_aviso("¡No hay memoria! Lo último no se ha podido hacer entero: guarde el documento.");
	}
}

/* ---- las órdenes ---- */
static void formato_conmuta(u32 marca)
{
	fmt_letra f = ed_formato_actual(&P.e), v = f;

	if (marca == CF_NEGRITA)
		v.negrita = !f.negrita;
	else if (marca == CF_CURSIVA)
		v.cursiva = !f.cursiva;
	else if (marca == CF_SUBRAYADO)
		v.subrayado = !f.subrayado;
	else if (marca == CF_TACHADO)
		v.tachado = !f.tachado;
	ed_formato(&P.e, marca, &v);
	pl_cambio();
}

static void alinea(int a)
{
	fmt_parrafo p = P.d->par[P.e.cur.p].p;

	p.alin = (u8)a;
	ed_formato_parrafo(&P.e, CP_ALIN, &p);
	pl_cambio();
}

static void abre_lista(int orden, int x, int y)
{
	char t[1100];

	t[0] = 0;
	if (orden == O_LISTA_ESTILO) {
		u32 i;

		for (i = 0; i < P.d->n_est && i < 32; i++) {
			if (i)
				anade(t, "|", sizeof t);
			anade(t, P.d->est[i].nombre, sizeof t);
		}
	} else if (orden == O_LISTA_FUENTE) {
		const char *f[32];
		int n = letras_familias(f, 31), i;

		for (i = 0; i < n; i++) {
			if (i)
				anade(t, "|", sizeof t);
			anade(t, f[i], sizeof t);
		}
		anade(t, "|-|Más fuentes... (el cuadro Fuentes)", sizeof t);
	} else if (orden == O_LISTA_TAMANO)
		copia(t, TAMANOS, sizeof t);
	else if (orden == O_LISTA_ZOOM)
		copia(t, ZOOMS, sizeof t);
	else if (orden == O_COLOR_MENU)
		copia(t, COLORES, sizeof t);
	else if (orden == O_RESALTE_MENU)
		copia(t, RESALTES, sizeof t);
	so_menu(P.v, x, y, t, 0, ETIQ_LISTA + (u32)orden);
}

static void elige_de_lista(int orden, int op)
{
	if (op < 0)
		return;
	if (orden == O_LISTA_ESTILO) {
		ed_estilo(&P.e, op);
		pl_cambio();
	} else if (orden == O_LISTA_FUENTE) {
		const char *f[32];
		int n = letras_familias(f, 31);
		fmt_letra v;

		if (op >= n) {
			dlg_fuente();
			return;
		}
		copia(v.familia, f[op], sizeof v.familia);
		ed_formato(&P.e, CF_FAMILIA, &v);
		pl_cambio();
	} else if (orden == O_LISTA_TAMANO) {
		static const u8 t[] = { 8, 9, 10, 11, 12, 14, 16, 18, 20, 22, 24, 26, 28, 36, 48, 72 };
		fmt_letra v;

		if (op >= (int)sizeof t)
			return;
		v.tam = (u16)(t[op] * 2);
		ed_formato(&P.e, CF_TAM, &v);
		pl_cambio();
	} else if (orden == O_LISTA_ZOOM) {
		static const int z[] = { 50, 75, 100, 125, 150, 200 };

		if (op < 6)
			vista_zoom(&P.vi, z[op]);
		else if (op == 6)
			vista_zoom(&P.vi, vista_zoom_ancho(&P.vi));
		else
			vista_zoom(&P.vi, (int)((i64)(al_doc - 2 * VISTA_HUECO) * 1500 / P.d->pag_al));
		P.sucio = 1;
	} else if (orden == O_COLOR_MENU) {
		fmt_letra v;

		if (op >= (int)(sizeof colores_v / 4))
			return;
		P.color_letra = colores_v[op];
		v.color = P.color_letra;
		ed_formato(&P.e, CF_COLOR, &v);
		pl_cambio();
	} else if (orden == O_RESALTE_MENU) {
		fmt_letra v;

		if (op >= (int)(sizeof resaltes_v / 4))
			return;
		P.color_resalte = resaltes_v[op];
		v.resalte = P.color_resalte;
		ed_formato(&P.e, CF_RESALTE, &v);
		pl_cambio();
	}
}

static void copia_al_sistema(void)
{
	char *t = ed_copia(&P.e);

	if (t) {
		so_al_portapapeles(t, largo(t));
		so_suelta(t);
	}
}

static void pega(int con_formato)
{
	u32 n;
	char *t = so_del_portapapeles(&n);

	if (!t) {
		pl_aviso("El portapapeles está vacío.");
		return;
	}
	if (!con_formato) {
		/* (con un espacio delante, para que no case con lo copiado aquí: va sin formato) */
		char *u = so_pide(n + 2);

		if (u) {
			u[0] = '\r';
			memcpy(u + 1, t, n);
			u[n + 1] = 0;
			ed_pega(&P.e, u + 1, n);
			so_suelta(u);
		}
	} else
		ed_pega(&P.e, t, n);
	so_suelta(t);
	pl_cambio();
}

static void selecciona_fila(int tabla_entera)
{
	u32 ini, n, f = P.d->par[P.e.cur.p].p.fila, k, a = 0xFFFFFFFF, b = 0;
	pos pa, pb;

	if (!en_tabla())
		return;
	ini = doc_tabla_ini(P.d, P.e.cur.p, &n);
	for (k = ini; k < ini + n; k++)
		if (tabla_entera || P.d->par[k].p.fila == f) {
			if (a == 0xFFFFFFFF)
				a = k;
			b = k;
		}
	pa.p = a;
	pa.o = 0;
	pb.p = b;
	pb.o = P.d->par[b].n;
	ed_elige(&P.e, pa, pb);
	P.sucio = 1;
}

static void hacer_guardar_y(int orden);

void pl_orden(int o)
{
	if (!activa(o) && o != O_NADA) {
		pl_aviso("Eso aún no está en Pluma 2.");
		P.sucio = 1;
		return;
	}
	if (P.con_brocha && o != O_BROCHA) {
		P.con_brocha = 0;
	}
	switch (o) {
	case O_NUEVO:
		if (P.d->cambiado) {
			dlg_guardar_cambios(O_NUEVO);
			return;
		}
		pon_documento(doc_nuevo(), NULL, FMT_PLU);
		pl_aviso("Documento nuevo");
		break;
	case O_ABRIR:
		if (P.d->cambiado) {
			dlg_guardar_cambios(O_ABRIR);
			return;
		}
		dlg_abrir(0, 0);
		break;
	case O_CERRAR:
		if (P.d->cambiado) {
			dlg_guardar_cambios(O_CERRAR);
			return;
		}
		pon_documento(doc_nuevo(), NULL, FMT_PLU);
		break;
	case O_GUARDAR:
	case O_GUARDAR_TODO:
		if (!P.ruta[0] || P.formato == FMT_TXT || P.formato == FMT_NADA)
			dlg_abrir(1, 0);
		else
			pl_guarda(P.ruta, P.formato);
		break;
	case O_GUARDAR_COMO: dlg_abrir(1, 0); break;
	case O_EXPORTAR_PDF: dlg_abrir(1, 1); break;
	case O_IMPRIMIR:
		dlg_aviso("Imprimir", "Hispa-Pentium aún no tiene impresoras.\nPluma deja el documento listo para imprimir en un PDF\n(Archivo > Exportar a PDF): se imprime igual que se ve.");
		break;
	case O_RESUMEN: dlg_resumen(); break;
	case O_PREPARAR: dlg_pagina(); break;
	case O_PREVIA:
		P.vi.previa = !P.vi.previa;
		if (P.vi.previa) {
			vista_zoom(&P.vi, (int)((i64)(al_doc - 3 * VISTA_HUECO) * 1500 / P.d->pag_al));
			pl_aviso("Presentación preliminar: Esc para volver");
		} else
			vista_zoom(&P.vi, 100);
		P.sucio = 1;
		break;
	case O_SALIR:
		if (P.d->cambiado) {
			dlg_guardar_cambios(O_SALIR);
			return;
		}
		P.sale = 1;
		break;
	case O_DESHACER:
		ed_deshacer(&P.e);
		pl_cambio();
		break;
	case O_REHACER:
		ed_rehacer(&P.e);
		pl_cambio();
		break;
	case O_CORTAR:
		copia_al_sistema();
		ed_borra_eleccion(&P.e);
		pl_cambio();
		break;
	case O_COPIAR:
		copia_al_sistema();
		pl_aviso("Copiado");
		P.sucio = 1;
		break;
	case O_PEGAR: pega(1); break;
	case O_PEGAR_SIN: pega(0); break;
	case O_BORRAR:
		ed_borra_eleccion(&P.e);
		pl_cambio();
		break;
	case O_TODO:
		ed_elige_todo(&P.e);
		P.sucio = 1;
		break;
	case O_BUSCAR: dlg_buscar(0); break;
	case O_REEMPLAZAR: dlg_buscar(1); break;
	case O_IR_A: dlg_ir_a(); break;
	case O_NORMAL:
		P.vi.previa = 0;
		P.completa = 0;
		P.sucio = 1;
		break;
	case O_COMPLETA:
		P.completa = !P.completa;
		pl_aviso(P.completa ? "Pantalla completa: Esc para volver" : "");
		P.sucio = 1;
		break;
	case O_BARRAS:
		P.ver_barras = !P.ver_barras;
		P.sucio = 1;
		break;
	case O_REGLA:
		P.ver_regla = !P.ver_regla;
		P.sucio = 1;
		break;
	case O_ENC_PIE: dlg_enc_pie(); break;
	case O_MARCAS:
		P.vi.marcas = !P.vi.marcas;
		P.sucio = 1;
		break;
	case O_ZOOM: dlg_zoom(); break;
	case O_SALTO: dlg_salto(); break;
	case O_SALTO_PAGINA:
		ed_salto_pagina(&P.e);
		pl_cambio();
		break;
	case O_FECHA: dlg_fecha(); break;
	case O_CAMPO: dlg_campo(); break;
	case O_SIMBOLO: dlg_simbolo(); break;
	case O_NOTA:
		ed_nota(&P.e);
		pl_cambio();
		pl_aviso("Nota al pie: su texto, en Ver > Notas al pie (aún sin hacer: la nota sale numerada)");
		break;
	case O_INS_ARCHIVO: dlg_abrir(2, 0); break;
	case O_IMAGEN: dlg_abrir(3, 0); break;
	case O_FUENTES: dlg_fuente(); break;
	case O_PARRAFO: dlg_parrafo(); break;
	case O_TABULACIONES: dlg_tabulaciones(); break;
	case O_BORDES: dlg_bordes(); break;
	case O_MAYUS: dlg_mayus(); break;
	case O_LISTAS: dlg_listas(); break;
	case O_ESTILO: dlg_estilo(); break;
	case O_FMT_IMAGEN: dlg_imagen(); break;
	case O_ORTOGRAFIA: dlg_ortografia(); break;
	case O_IDIOMA: dlg_idioma(); break;
	case O_CONTAR: dlg_contar(); break;
	case O_OPCIONES: dlg_opciones(); break;
	case O_TABLA: dlg_tabla(); break;
	case O_FILA_ARRIBA: ed_fila(&P.e, 0); pl_cambio(); break;
	case O_FILA_ABAJO: ed_fila(&P.e, 1); pl_cambio(); break;
	case O_COL_IZQ: ed_columna(&P.e, 0); pl_cambio(); break;
	case O_COL_DER: ed_columna(&P.e, 1); pl_cambio(); break;
	case O_ELIMINAR_CELDAS: dlg_eliminar_celdas(); break;
	case O_UNIR: ed_une_celdas(&P.e); pl_cambio(); break;
	case O_DIVIDIR: ed_divide_celda(&P.e); pl_cambio(); break;
	case O_SEL_FILA: selecciona_fila(0); break;
	case O_SEL_TABLA: selecciona_fila(1); break;
	case O_ANCHO_CELDA: dlg_ancho_celda(); break;
	case O_TITULOS:
		if (en_tabla()) {
			u32 ini, n;

			ini = doc_tabla_ini(P.d, P.e.cur.p, &n);
			ed_empieza(&P.e, ini, ini + n, 3 | PASO_TABLAS);
			P.d->tab[P.d->par[P.e.cur.p].p.tabla - 1].cabecera ^= 1;
			ed_acaba(&P.e, n);
			pl_cambio();
		}
		break;
	case O_CUADRICULA:
		if (en_tabla())
			ed_bordes(&P.e, P.d->tab[P.d->par[P.e.cur.p].p.tabla - 1].bordes == BORDE_TODOS ? BORDE_NO : BORDE_TODOS);
		pl_cambio();
		break;
	case O_NUEVA_VENTANA: {
		char prog[300];

		so_programa(prog, sizeof prog);
		so_lanza(prog, "");
		pl_aviso("Abriendo otra ventana de Pluma...");
		P.sucio = 1;
		break;
	}
	case O_DOCUMENTO1: break;
	case O_CONTENIDO:
	case O_AYUDA_PLUMA: dlg_ayuda(); break;
	case O_ACERCA: dlg_acerca(); break;
	case O_NEGRITA: formato_conmuta(CF_NEGRITA); break;
	case O_CURSIVA: formato_conmuta(CF_CURSIVA); break;
	case O_SUBRAYADO: formato_conmuta(CF_SUBRAYADO); break;
	case O_TACHADO: formato_conmuta(CF_TACHADO); break;
	case O_IZQ: alinea(AL_IZQ); break;
	case O_CENTRO: alinea(AL_CENTRO); break;
	case O_DER: alinea(AL_DER); break;
	case O_JUST: alinea(AL_JUST); break;
	case O_VINETAS: ed_lista(&P.e, LISTA_VINETAS); pl_cambio(); break;
	case O_NUMEROS: ed_lista(&P.e, LISTA_NUMEROS); pl_cambio(); break;
	case O_SANGRIA_MENOS: ed_sangria(&P.e, 0); pl_cambio(); break;
	case O_SANGRIA_MAS: ed_sangria(&P.e, 1); pl_cambio(); break;
	case O_COLOR: {
		fmt_letra v;

		v.color = P.color_letra;
		ed_formato(&P.e, CF_COLOR, &v);
		pl_cambio();
		break;
	}
	case O_RESALTE: {
		fmt_letra v;

		v.resalte = P.color_resalte;
		ed_formato(&P.e, CF_RESALTE, &v);
		pl_cambio();
		break;
	}
	case O_BROCHA:
		if (P.con_brocha)
			P.con_brocha = 0;
		else {
			P.brocha = ed_formato_actual(&P.e);
			P.brocha_p = P.d->par[P.e.cur.p].p;
			P.con_brocha = 1;
			pl_aviso("Copiar formato: elija con el ratón lo que quiere cambiar");
		}
		P.sucio = 1;
		break;
	case O_MAS_GRANDE:
	case O_MAS_PEQUENA: {
		fmt_letra v = ed_formato_actual(&P.e);
		int t = v.tam / 2;

		t = o == O_MAS_GRANDE ? (t < 12 ? t + 1 : t < 28 ? t + 2 : t + 8) : (t > 28 ? t - 8 : t > 12 ? t - 2 : t - 1);
		if (t < 4)
			t = 4;
		if (t > 400)
			t = 400;
		v.tam = (u16)(t * 2);
		ed_formato(&P.e, CF_TAM, &v);
		pl_cambio();
		break;
	}
	}
	(void)hacer_guardar_y;
}

static void hacer_guardar_y(int orden)
{
	(void)orden;
}

/* ---- el teclado ---- */
static void mueve(pos c, int ampliar)
{
	ed_cursor(&P.e, c, ampliar);
	P.vi.sigue = 1;
	P.sucio = 1;
}

static void tecla(const so_suceso *s)
{
	u32 t = s->tecla;
	int mayus = (s->mods & SO_CON_MAYUS) != 0, ctrl = (s->mods & SO_CON_CONTROL) != 0;
	editor *e = &P.e;

	if (P.cuadro) {
		if (ui_cuadro_suceso(P.cuadro, P.v, s))
			P.sucio = 1;
		return;
	}
	ayuda_encima = NULL;
	if (t >= SO_T_ALT && t < SO_T_ALT + 26) {
		int m = ui_menu_letra(MENUS, 'a' + (t - SO_T_ALT));

		if (m >= 0)
			abre_menu(m);
		return;
	}
	if (t != SO_T_ARRIBA && t != SO_T_ABAJO && t != SO_T_RE_PAG && t != SO_T_AV_PAG)
		P.vi.x_deseada = -1;
	switch (t) {
	case SO_ESC:
		if (P.vi.previa || P.completa) {
			P.vi.previa = 0;
			P.completa = 0;
			vista_zoom(&P.vi, 100);
		} else if (P.con_brocha)
			P.con_brocha = 0;
		else
			ed_cursor(e, e->cur, 0);
		P.sucio = 1;
		return;
	case SO_T_F1: pl_orden(O_CONTENIDO); return;
	case SO_T_F5: pl_orden(O_IR_A); return;
	case SO_T_F7: pl_orden(O_ORTOGRAFIA); return;
	case SO_T_F10: abre_menu(0); return;
	case SO_T_F12: pl_orden(O_GUARDAR_COMO); return;
	case SO_T_IZQUIERDA: mueve(ed_izquierda(e, e->cur, ctrl), mayus); return;
	case SO_T_DERECHA: mueve(ed_derecha(e, e->cur, ctrl), mayus); return;
	case SO_T_ARRIBA:
		if (ctrl) {
			pos c = e->cur;

			if (c.o == 0 && c.p > 0)
				c.p--;
			c.o = 0;
			mueve(c, mayus);
		} else
			mueve(vista_arriba(&P.vi, e->cur, -1), mayus);
		return;
	case SO_T_ABAJO:
		if (ctrl) {
			pos c = e->cur;

			if (c.p + 1 < P.d->n_par)
				c.p++, c.o = 0;
			else
				c.o = P.d->par[c.p].n;
			mueve(c, mayus);
		} else
			mueve(vista_arriba(&P.vi, e->cur, 1), mayus);
		return;
	case SO_T_RE_PAG:
	case SO_T_AV_PAG: {
		int lineas = al_doc / (vista_px(&P.vi, 300) + 1) - 2;

		if (lineas < 1)
			lineas = 1;
		mueve(vista_arriba(&P.vi, e->cur, t == SO_T_RE_PAG ? -lineas : lineas), mayus);
		return;
	}
	case SO_T_INICIO:
		if (ctrl) {
			pos c = { 0, 0 };

			mueve(c, mayus);
		} else
			mueve(vista_inicio_linea(&P.vi, e->cur), mayus);
		return;
	case SO_T_FIN:
		if (ctrl) {
			pos c;

			c.p = P.d->n_par - 1;
			c.o = P.d->par[c.p].n;
			mueve(c, mayus);
		} else
			mueve(vista_fin_linea(&P.vi, e->cur), mayus);
		return;
	case SO_RETROCESO: ed_retroceso(e, ctrl); pl_cambio(); return;
	case SO_T_SUPR:
		if (mayus) {
			pl_orden(O_CORTAR);
			return;
		}
		ed_suprime(e, ctrl);
		pl_cambio();
		return;
	case SO_T_INSERTAR:
		if (mayus)
			pl_orden(O_PEGAR);
		else if (ctrl)
			pl_orden(O_COPIAR);
		return;
	case SO_INTRO:
		if (mayus)
			ed_salto_linea(e);
		else
			ed_intro(e);
		pl_cambio();
		return;
	case SO_TAB:
	case SO_T_MAYUS_TAB: {
		int atras = t == SO_T_MAYUS_TAB || mayus;
		pos a, b;

		if (en_tabla())
			ed_celda_siguiente(e, atras);
		else if (atras)
			ed_sangria(e, 0);
		else if ((ed_eleccion(e, &a, &b) && a.p != b.p) || (e->cur.o == 0 && P.d->par[e->cur.p].p.lista))
			ed_sangria(e, 1);
		else
			ed_escribe_letra(e, '\t');
		pl_cambio();
		return;
	}
	}
	if (ctrl && t >= 1 && t <= 26) {
		switch (t + 'A' - 1) {
		case 'N': pl_orden(O_NEGRITA); break;
		case 'K': pl_orden(O_CURSIVA); break;
		case 'S': pl_orden(O_SUBRAYADO); break;
		case 'U': pl_orden(O_NUEVO); break;
		case 'A': pl_orden(O_ABRIR); break;
		case 'G': pl_orden(O_GUARDAR); break;
		case 'P': pl_orden(O_IMPRIMIR); break;
		case 'Z': pl_orden(O_DESHACER); break;
		case 'Y': pl_orden(O_REHACER); break;
		case 'X': pl_orden(O_CORTAR); break;
		case 'C': pl_orden(O_COPIAR); break;
		case 'V': pl_orden(O_PEGAR); break;
		case 'E': pl_orden(O_TODO); break;
		case 'B': pl_orden(O_BUSCAR); break;
		case 'L': pl_orden(O_REEMPLAZAR); break;
		case 'R': pl_orden(O_SALTO_PAGINA); break;
		case 'T': pl_orden(O_CENTRO); break;
		case 'D': pl_orden(O_DER); break;
		case 'Q': pl_orden(O_IZQ); break;
		case 'O': pl_orden(O_MAS_GRANDE); break;	/* (Ctrl+O, Ctrl+W: más grande y más pequeña) */
		case 'W': pl_orden(O_MAS_PEQUENA); break;
		}
		return;
	}
	if (t >= 0x20 && t < SO_T_BASE) {
		ed_escribe_letra(e, t);
		pl_cambio();
	}
}

/* ---- el ratón ---- */
static int en_menu_contexto;
static char sug_menu[8][64];
static int n_sug;
static pos sug_a, sug_b;

static void menu_contexto(int x, int y)
{
	char t[1100];
	pos c;
	u32 g = 0;
	int k = 0;

	t[0] = 0;
	n_sug = 0;
	if (vista_pos_en(&P.vi, x, y, &c)) {
		pos a, b;

		if (!ed_eleccion(&P.e, &a, &b) || pos_menor(c, a) || pos_menor(b, c))
			ed_cursor(&P.e, c, 0);
		/* ¿una falta? las propuestas, arriba */
		if (pl_orto_hay()) {
			pos wa, wb;
			const parrafo *p = &P.d->par[c.p];

			ed_palabra(&P.e, c, &wa, &wb);
			while (wb.o > wa.o && !es_letra(p->t[wb.o - 1]))
				wb.o--;
			if (wb.o > wa.o && wb.o - wa.o < 64 && !pl_orto_bien(p->t + wa.o, wb.o - wa.o)) {
				int i;

				sug_a = wa;
				sug_b = wb;
				n_sug = pl_orto_sugiere(p->t + wa.o, wb.o - wa.o, sug_menu, 6);
				for (i = 0; i < n_sug; i++) {
					anade(t, sug_menu[i], sizeof t);
					anade(t, "|", sizeof t);
					k++;
				}
				if (!n_sug) {
					anade(t, "(sin propuestas)|", sizeof t);
					g |= 1u << k;
					k++;
					n_sug = 1;
					sug_menu[0][0] = 0;
				}
				anade(t, "Añadir al diccionario|Omitir todas|-|", sizeof t);
				k += 3;
			}
		}
	}
	anade(t, "Cortar|Copiar|Pegar|-|Fuentes...|Párrafo...|Numeración y viñetas...", sizeof t);
	{
		pos a, b;

		if (!ed_eleccion(&P.e, &a, &b))
			g |= 3u << k;
	}
	if (en_tabla())
		anade(t, "|-|Insertar fila debajo|Insertar columna a la derecha|Eliminar la fila|Eliminar la columna|Unir celdas", sizeof t);
	if (ed_imagen_elegida(&P.e) >= 0)
		anade(t, "|-|Imagen...", sizeof t);
	en_menu_contexto = k;
	so_menu(P.v, x, y, t, g, ETIQ_CONTEXTO);
	P.sucio = 1;
}

static void elige_contexto(int op)
{
	int k = en_menu_contexto;

	if (op < 0)
		return;
	if (k) {
		/* las propuestas (y añadir, omitir) */
		if (op < k - 3) {
			if (op < n_sug && sug_menu[op][0]) {
				u32 n;
				u32 *w = u32_de_utf8(sug_menu[op], largo(sug_menu[op]), &n);

				if (w) {
					ed_elige(&P.e, sug_a, sug_b);
					ed_escribe(&P.e, w, n);
					so_suelta(w);
					pl_cambio();
				}
			}
			return;
		}
		if (op == k - 3 || op == k - 2) {
			const parrafo *p = &P.d->par[sug_a.p];

			if (op == k - 3)
				pl_orto_anade(p->t + sug_a.o, sug_b.o - sug_a.o);
			else
				pl_orto_omite(p->t + sug_a.o, sug_b.o - sug_a.o);
			{
				u32 i;

				for (i = 0; i < P.d->n_par; i++)
					P.d->par[i].faltas_version = P.d->par[i].version - 1;
			}
			P.sucio = 1;
			return;
		}
		op -= k;
		if (op < 0)
			return;
	}
	switch (op) {
	case 0: pl_orden(O_CORTAR); break;
	case 1: pl_orden(O_COPIAR); break;
	case 2: pl_orden(O_PEGAR); break;
	case 4: pl_orden(O_FUENTES); break;
	case 5: pl_orden(O_PARRAFO); break;
	case 6: pl_orden(O_LISTAS); break;
	default:
		if (en_tabla()) {
			switch (op) {
			case 8: pl_orden(O_FILA_ABAJO); break;
			case 9: pl_orden(O_COL_DER); break;
			case 10: ed_quita_fila(&P.e); pl_cambio(); break;
			case 11: ed_quita_columna(&P.e); pl_cambio(); break;
			case 12: pl_orden(O_UNIR); break;
			case 14: pl_orden(O_FMT_IMAGEN); break;
			}
		} else if (op == 8)
			pl_orden(O_FMT_IMAGEN);
	}
}

/* ¿está el punto en el tirador de abajo a la derecha de la imagen elegida? */
static int en_tirador(int x, int y, int *img)
{
	int i = ed_imagen_elegida(&P.e), cx, cy, ch;
	pos a, b;

	*img = i;
	if (i < 0)
		return 0;
	ed_eleccion(&P.e, &a, &b);
	if (P.d->img[i].ajuste != AJUSTE_LINEA)
		return 0;
	if (!vista_cursor_xy(&P.vi, a, &cx, &cy, &ch))
		return 0;
	{
		const parrafo *p = &P.d->par[a.p];
		u32 l = maq_linea_de(p, a.o, 0);
		int base = cy + vista_px(&P.vi, p->lin[l].base);
		int x1 = cx + vista_px(&P.vi, P.d->img[i].ancho), y1 = base;

		return x >= x1 - 8 && x <= x1 + 8 && y >= y1 - 8 && y <= y1 + 8;
	}
}

static void raton_doc(const so_suceso *s)
{
	pos c;

	if (s->clase == SO_BAJA && (s->botones & SO_BOTON_DER)) {
		menu_contexto(s->x, s->y);
		return;
	}
	if (s->clase == SO_BAJA && (s->botones & SO_BOTON_IZQ)) {
		int img;

		if (en_tirador(s->x, s->y, &img)) {
			arrastra_img = 1;
			arr_de = s->x;
			arr_pos0 = P.d->img[img].ancho;
			return;
		}
		if (!vista_pos_en(&P.vi, s->x, s->y, &c))
			return;
		veces_clic = s->veces;
		pos_clic = c;
		if (s->veces == 2) {
			pos a, b;
			const parrafo *p = &P.d->par[c.p];

			/* dos clics en una imagen: elegirla */
			if (c.o < p->n && es_imagen(p->t[c.o])) {
				a = c;
				b = c;
				b.o++;
			} else if (c.o > 0 && es_imagen(p->t[c.o - 1])) {
				a = c;
				a.o--;
				b = c;
			} else
				ed_palabra(&P.e, c, &a, &b);
			ed_elige(&P.e, a, b);
		} else if (s->veces >= 3) {
			pos a = c, b = c;

			a.o = 0;
			b.o = P.d->par[c.p].n;
			ed_elige(&P.e, a, b);
		} else {
			const parrafo *p = &P.d->par[c.p];

			ed_cursor(&P.e, c, (s->mods & SO_CON_MAYUS) != 0);
			/* un clic en una imagen: elegida (para el tamaño y el ajuste) */
			if (!(s->mods & SO_CON_MAYUS) && c.o < p->n && es_imagen(p->t[c.o])) {
				pos b = c;

				b.o++;
				ed_elige(&P.e, c, b);
			}
		}
		arrastra_doc = 1;
		P.vi.x_deseada = -1;
		P.sucio = 1;
		return;
	}
	if (s->clase == SO_MUEVE && arrastra_img && (s->botones & SO_BOTON_IZQ)) {
		int i = ed_imagen_elegida(&P.e);

		if (i >= 0) {
			i32 w = arr_pos0 + vista_tw(&P.vi, s->x - arr_de), h;

			if (w < 300)
				w = 300;
			if (w > doc_ancho_texto(P.d))
				w = doc_ancho_texto(P.d);
			h = (i32)((i64)w * P.d->img[i].alto / (P.d->img[i].ancho ? P.d->img[i].ancho : 1));
			/* (mientras se arrastra, sin deshacer; al soltar, con él) */
			P.d->img[i].ancho = w;
			P.d->img[i].alto = h;
			P.d->par[P.e.cur.p].ancho_maq = -1;
			P.d->par[P.e.ancla.p].ancho_maq = -1;
			P.sucio = 1;
		}
		return;
	}
	if (s->clase == SO_MUEVE && arrastra_doc && (s->botones & SO_BOTON_IZQ)) {
		if (vista_pos_en(&P.vi, s->x, s->y, &c)) {
			if (veces_clic == 2) {
				/* por palabras */
				pos a, b, a0, b0;

				ed_palabra(&P.e, pos_clic, &a0, &b0);
				ed_palabra(&P.e, c, &a, &b);
				if (pos_menor(c, pos_clic))
					ed_elige(&P.e, b0, a);
				else
					ed_elige(&P.e, a0, b);
			} else
				ed_cursor(&P.e, c, 1);
			P.sucio = 1;
			/* cerca de los bordes, se desplaza */
			if (s->y < y_doc + 6)
				P.vi.dy -= 30;
			else if (s->y > y_doc + al_doc - 6)
				P.vi.dy += 30;
			if (P.vi.dy < 0)
				P.vi.dy = 0;
		}
		return;
	}
	if (s->clase == SO_SUBE) {
		if (arrastra_img) {
			int i = ed_imagen_elegida(&P.e);

			arrastra_img = 0;
			if (i >= 0) {
				i32 w = P.d->img[i].ancho, h = P.d->img[i].alto;

				P.d->img[i].ancho = arr_pos0;
				P.d->img[i].alto = (i32)((i64)arr_pos0 * h / (w ? w : 1));
				ed_tamano_imagen(&P.e, w, h);
				pl_cambio();
			}
		}
		if (arrastra_doc && P.con_brocha) {
			pos a, b;

			if (ed_eleccion(&P.e, &a, &b)) {
				ed_copia_formato(&P.e, &P.brocha, &P.brocha_p);
				P.con_brocha = 0;
				pl_cambio();
			}
		}
		arrastra_doc = 0;
	}
}

static void raton_regla(const so_suceso *s)
{
	int yr = s->y - y_regla, cual;

	datos_regla();
	if (s->clase == SO_BAJA && (s->botones & SO_BOTON_IZQ)) {
		int r = ui_regla_en(&regla, s->x, yr, &cual);

		if (r == RG_TIPO) {
			tab_tipo = (tab_tipo + 1) % 4;
			pl_aviso("Tabulación nueva: %s", tab_tipo == TAB_IZQ ? "a la izquierda" : tab_tipo == TAB_CENTRO ? "centrada" :
				 tab_tipo == TAB_DER ? "a la derecha" : "decimal");
			P.sucio = 1;
			return;
		}
		if (r == RG_ZONA) {
			/* una tabulación nueva */
			fmt_parrafo p = P.d->par[P.e.cur.p].p;
			i32 pos_ = ui_regla_tw(&regla, s->x) - regla.inicio;
			int k, j;

			if (p.n_tabs >= MAX_TABS)
				return;
			pos_ = (pos_ + 50) / 100 * 100;
			for (k = 0; k < p.n_tabs && p.tabs[k].pos < pos_; k++)
				;
			for (j = p.n_tabs; j > k; j--)
				p.tabs[j] = p.tabs[j - 1];
			p.tabs[k].pos = pos_;
			p.tabs[k].tipo = (u8)tab_tipo;
			p.tabs[k].relleno = 0;
			p.n_tabs++;
			ed_formato_parrafo(&P.e, CP_TABS, &p);
			pl_cambio();
			arrastra_regla = RG_TAB;
			cual_tab = k;
			regla_valor = pos_;
			return;
		}
		if (r == RG_PRIMERA || r == RG_IZQ || r == RG_DER || r == RG_TAB) {
			arrastra_regla = r;
			cual_tab = cual;
			regla_valor = r == RG_PRIMERA ? regla.sang_izq + regla.sang_primera : r == RG_IZQ ? regla.sang_izq : r == RG_DER ? regla.sang_der : regla.tabs[cual];
			P.sucio = 1;
		}
		return;
	}
	if (s->clase == SO_MUEVE && arrastra_regla && (s->botones & SO_BOTON_IZQ)) {
		i32 tw = ui_regla_tw(&regla, s->x) - regla.inicio;

		tw = (tw + 25) / 50 * 50;	/* (de 50 en 50 twips: unos 0,9 mm) */
		if (arrastra_regla == RG_DER) {
			i32 ancho = regla.fin > regla.inicio ? regla.fin - regla.inicio : P.d->pag_an - P.d->marg_izq - P.d->marg_der;

			tw = ancho - tw;
		}
		regla_valor = tw;
		P.sucio = 1;
		return;
	}
	if (s->clase == SO_SUBE && arrastra_regla) {
		fmt_parrafo p = P.d->par[P.e.cur.p].p;
		int r = arrastra_regla;

		arrastra_regla = 0;
		if (r == RG_PRIMERA) {
			p.sang_primera = regla_valor - p.sang_izq;
			ed_formato_parrafo(&P.e, CP_SANG_PRIMERA, &p);
		} else if (r == RG_IZQ) {
			/* la izquierda mueve también la primera línea (con ella) */
			i32 prim_abs = p.sang_izq + p.sang_primera;

			p.sang_izq = regla_valor < -P.d->marg_izq ? -P.d->marg_izq : regla_valor;
			p.sang_primera = prim_abs - p.sang_izq + (regla_valor - (prim_abs - p.sang_primera)) * 0;
			p.sang_primera = P.d->par[P.e.cur.p].p.sang_primera;
			ed_formato_parrafo(&P.e, CP_SANG_IZQ | CP_SANG_PRIMERA, &p);
		} else if (r == RG_DER) {
			p.sang_der = regla_valor;
			ed_formato_parrafo(&P.e, CP_SANG_DER, &p);
		} else if (r == RG_TAB && cual_tab >= 0 && cual_tab < p.n_tabs) {
			/* fuera de la regla (por abajo), se quita */
			if (s->y > y_regla + ui_alto_regla() + 10 || regla_valor < 0) {
				int j;

				for (j = cual_tab; j + 1 < p.n_tabs; j++)
					p.tabs[j] = p.tabs[j + 1];
				p.n_tabs--;
				pl_aviso("Tabulación quitada");
			} else
				p.tabs[cual_tab].pos = regla_valor;
			ed_formato_parrafo(&P.e, CP_TABS, &p);
		}
		pl_cambio();
	}
}

static void raton(const so_suceso *s)
{
	int flecha;

	if (P.cuadro) {
		if (ui_cuadro_suceso(P.cuadro, P.v, s))
			P.sucio = 1;
		return;
	}
	/* lo que se está arrastrando sigue aunque se salga */
	if (arrastra_desp) {
		if (s->clase == SO_MUEVE && (s->botones & SO_BOTON_IZQ)) {
			if (arrastra_desp == 1)
				P.vi.dy = ui_desp_arrastra(&desp_v, arr_pos0, arr_de, s->y);
			else
				P.vi.dx = ui_desp_arrastra(&desp_h, arr_pos0, arr_de, s->x);
			P.sucio = 1;
		} else if (s->clase == SO_SUBE)
			arrastra_desp = 0;
		return;
	}
	if (arrastra_regla) {
		raton_regla(s);
		return;
	}
	if (arrastra_doc || arrastra_img) {
		raton_doc(s);
		return;
	}
	/* la barra de menús */
	if (!P.completa && s->y < ui_alto_menus()) {
		int m = ui_menu_en(MENUS, 0, s->x, NULL);

		if (s->clase == SO_BAJA && m >= 0)
			abre_menu(m);
		else if (m != encima_menu) {
			encima_menu = m;
			P.sucio = 1;
		}
		return;
	}
	if (encima_menu >= 0) {
		encima_menu = -1;
		P.sucio = 1;
	}
	/* las barras */
	if (P.ver_barras && !P.completa && s->y >= y_b1 && s->y < y_b2 + ui_alto_barra()) {
		ui_boton *b = s->y < y_b2 ? barra1 : barra2;
		int n = s->y < y_b2 ? N1 : N2, i = ui_boton_en(b, n, s->x, s->y, &flecha);
		int *enc = s->y < y_b2 ? &encima1 : &encima2, *pul = s->y < y_b2 ? &pulsado1 : &pulsado2;

		if (s->clase == SO_MUEVE) {
			if (i != *enc) {
				*enc = i;
				(s->y < y_b2 ? &encima2 : &encima1)[0] = -1;
				ayuda_encima = i >= 0 ? b[i].ayuda : NULL;
				P.sucio = 1;
			}
		} else if (s->clase == SO_BAJA && i >= 0 && !b[i].apagado) {
			*pul = i;
			P.sucio = 1;
			if (b[i].tipo == UB_LISTA)
				abre_lista(b[i].id, b[i].x, b[i].y + b[i].al);
			else if (b[i].tipo == UB_COLOR && flecha)
				abre_lista(b[i].id == O_COLOR ? O_COLOR_MENU : O_RESALTE_MENU, b[i].x, b[i].y + b[i].al);
		} else if (s->clase == SO_SUBE) {
			int p = *pul;

			*pul = -1;
			P.sucio = 1;
			if (p >= 0 && p == i && b[i].tipo != UB_LISTA && !(b[i].tipo == UB_COLOR && flecha))
				pl_orden(b[i].id);
		}
		return;
	}
	if (encima1 >= 0 || encima2 >= 0 || pulsado1 >= 0 || pulsado2 >= 0) {
		encima1 = encima2 = pulsado1 = pulsado2 = -1;
		ayuda_encima = NULL;
		P.sucio = 1;
	}
	/* la regla */
	if (y_regla >= 0 && s->y >= y_regla && s->y < y_regla + ui_alto_regla()) {
		raton_regla(s);
		return;
	}
	/* las barras de desplazamiento */
	if (s->clase == SO_BAJA) {
		int z = ui_desp_en(&desp_v, s->x, s->y), h = hay_h ? ui_desp_en(&desp_h, s->x, s->y) : 0;

		if (z) {
			if (z == 5) {
				arrastra_desp = 1;
				arr_de = s->y;
				arr_pos0 = P.vi.dy;
			} else
				P.vi.dy += z == 1 ? -40 : z == 2 ? 40 : z == 3 ? -al_doc : al_doc;
			if (P.vi.dy > vista_alto_total(&P.vi) - al_doc)
				P.vi.dy = vista_alto_total(&P.vi) - al_doc;
			if (P.vi.dy < 0)
				P.vi.dy = 0;
			P.sucio = 1;
			return;
		}
		if (h) {
			if (h == 5) {
				arrastra_desp = 2;
				arr_de = s->x;
				arr_pos0 = P.vi.dx;
			} else
				P.vi.dx += h == 1 ? -40 : h == 2 ? 40 : h == 3 ? -an_doc : an_doc;
			if (P.vi.dx > vista_ancho_total(&P.vi) - an_doc)
				P.vi.dx = vista_ancho_total(&P.vi) - an_doc;
			if (P.vi.dx < 0)
				P.vi.dx = 0;
			P.sucio = 1;
			return;
		}
	}
	if (s->y >= y_doc && s->y < y_doc + al_doc && s->x < an_doc && !P.vi.previa)
		raton_doc(s);
	else if (P.vi.previa && s->clase == SO_BAJA && s->veces >= 2 && vista_pos_en(&P.vi, s->x, s->y, &pos_clic)) {
		/* dos clics en una página de la vista previa: a ella */
		P.vi.previa = 0;
		vista_zoom(&P.vi, 100);
		ed_cursor(&P.e, pos_clic, 0);
		P.vi.sigue = 1;
		P.sucio = 1;
	}
}

static void rueda(const so_suceso *s)
{
	if (P.cuadro) {
		if (ui_cuadro_suceso(P.cuadro, P.v, s))
			P.sucio = 1;
		return;
	}
	if (s->mods & SO_CON_CONTROL) {
		int z = P.vi.zoom + (s->pasos > 0 ? 10 : -10) * (s->pasos > 0 ? s->pasos : -s->pasos);

		vista_zoom(&P.vi, z);
		pl_aviso("Zoom: %d%%", P.vi.zoom);
	} else {
		/* tres líneas por paso (unos 60 puntos a 100%), sin mover el cursor */
		P.vi.dy -= s->pasos * vista_px(&P.vi, 3 * 330);
		if (P.vi.dy > vista_alto_total(&P.vi) - al_doc)
			P.vi.dy = vista_alto_total(&P.vi) - al_doc;
		if (P.vi.dy < 0)
			P.vi.dy = 0;
	}
	P.sucio = 1;
}

static void menu_elegido(const so_suceso *s)
{
	u32 et = s->etiqueta;

	if (P.cuadro && et == UI_ETIQUETA_MENU) {
		ui_cuadro_suceso(P.cuadro, P.v, s);
		P.sucio = 1;
		return;
	}
	if (et >= ETIQ_MENU && et < ETIQ_MENU + 9) {
		int m = (int)(et - ETIQ_MENU);

		menu_abierto = -1;
		P.sucio = 1;
		if (s->opcion == -2) {
			abre_menu((m + 8) % 9);
			return;
		}
		if (s->opcion == -3) {
			abre_menu((m + 1) % 9);
			return;
		}
		if (s->opcion >= 0) {
			int i;

			for (i = 0; menus[m][i].texto; i++)
				if (i == s->opcion) {
					if (menus[m][i].orden != O_NADA)
						pl_orden(menus[m][i].orden);
					break;
				}
		}
		return;
	}
	if (et == ETIQ_CONTEXTO) {
		elige_contexto(s->opcion);
		P.sucio = 1;
		return;
	}
	if (et >= ETIQ_LISTA && et < ETIQ_LISTA + O_N) {
		pulsado1 = pulsado2 = -1;
		elige_de_lista((int)(et - ETIQ_LISTA), s->opcion);
		P.sucio = 1;
	}
}

/* ---- el principio ---- */
static int prepara_ventana(void)
{
	int an, al;

	so_pantalla(&an, &al);
	P.v = so_ventana("Pluma", an * 9 / 10, al * 9 / 10);
	if (!P.v)
		return 0;
	so_maximiza(P.v);
	if (!lz_nuevo(&P.l, an * 9 / 10, al * 9 / 10))
		return 0;
	return 1;
}

static void cambia_tamano(int an, int al)
{
	if (an < 200 || al < 140 || (an == P.l.an && al == P.l.al))
		return;
	lz_suelta(&P.l);
	if (!lz_nuevo(&P.l, an, al)) {
		/* (sin memoria para el lienzo nuevo: uno pequeño) */
		lz_nuevo(&P.l, 320, 200);
	}
	P.sucio = 1;
}

static void lee_letras_usuario(void)
{
	char r[64];
	char u = so_unidad_sistema();
	int n;

	fmt(r, sizeof r, "%c:\\DOCUMENT\\LETRAS", u);
	n = letras_carpeta(r);
	fmt(r, sizeof r, "%c:\\LETRAS", u);
	n += letras_carpeta(r);
	fmt(r, sizeof r, "%c:\\HISPA\\LETRAS", u);
	n += letras_carpeta(r);
	if (n)
		dice("pluma: %d letras TrueType del usuario", n);
}

int pluma_ventana(const char *archivo)
{
	so_suceso s;

	memset(&P, 0, sizeof P);
	P.ver_barras = P.ver_regla = 1;
	P.color_letra = 0xC62828;
	P.color_resalte = 0xFFF176;
	P.orto_al_escribir = 1;
	lee_letras_usuario();
	pl_orto_inicia();
	{
		char u = so_unidad_sistema();

		fmt(P.carpeta, sizeof P.carpeta, "%c:\\DOCUMENT", u);
		if (so_existe(P.carpeta) != 2)
			fmt(P.carpeta, sizeof P.carpeta, "%c:", u);
	}
	if (!prepara_ventana()) {
		dice("pluma: no se puede abrir la ventana");
		return 1;
	}
	pon_documento(doc_nuevo(), NULL, FMT_PLU);
	if (!P.d) {
		dice("pluma: no hay memoria");
		return 1;
	}
	if (archivo && archivo[0])
		pl_abre(archivo);
	pl_aviso("Listo");
	P.sucio = 1;
	dice("pluma: lista (%dx%d)", P.l.an, P.l.al);
	while (!P.sale) {
		int hay = so_lee_suceso(P.v, &s, !P.sucio);

		if (!hay) {
			if (P.sucio) {
				if (P.vi.sigue) {
					maq_documento(P.d);
					coloca();
					vista_al_cursor(&P.vi);
					P.vi.sigue = 0;
				}
				pl_pinta_todo();
			}
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
		case SO_FOCO:
			P.vi.foco = s.foco;
			P.sucio = 1;
			break;
		case SO_SALE:
			if (encima1 >= 0 || encima2 >= 0 || encima_menu >= 0) {
				encima1 = encima2 = encima_menu = -1;
				ayuda_encima = NULL;
				P.sucio = 1;
			}
			break;
		case SO_CERRAR:
			if (P.cuadro) {
				P.cuadro->cerrado = -1;
				P.sucio = 1;
			} else
				pl_orden(O_SALIR);
			break;
		}
		/* el cuadro, cerrado: lo que toca */
		if (P.cuadro && P.cuadro->cerrado != -9999) {
			ui_cuadro *q = P.cuadro;
			int id = q->cerrado;
			void (*f)(int) = P.al_cerrar;

			P.cuadro = NULL;
			P.al_cerrar = NULL;
			if (f)
				f(id);
			P.sucio = 1;
		}
	}
	dice("pluma: adiós");
	ed_suelta(&P.e);
	doc_suelta(P.d);
	lz_suelta(&P.l);
	so_cierra_ventana(P.v);
	return 0;
}

/* ---- medir (PLUMA --mide PAGINAS LETRAS): un documento de tantas páginas,
 * y lo que tarda cada letra que se escribe en medio (maquetar y pintar) ---- */
int pluma_mide(int paginas, int letras)
{
	static const char *const frase = "Lorem ipsum: en un lugar de la Mancha, de cuyo nombre no quiero acordarme, no ha mucho tiempo que vivía un hidalgo de los de lanza en astillero. ";
	u32 t0, t1, hz = so_hz(), i;
	int k, ventana = 0;
	u32 fl[200];
	u32 n;

	memset(&P, 0, sizeof P);
	P.ver_barras = P.ver_regla = 1;
	if (paginas < 1)
		paginas = 1;
	if (paginas > 2000)
		paginas = 2000;
	pon_documento(doc_nuevo(), NULL, FMT_PLU);
	{
		u32 *w = u32_de_utf8(frase, largo(frase), &n);

		if (!w)
			return 1;
		memcpy(fl, w, n * 4);
		so_suelta(w);
	}
	/* unos 3 párrafos de 4 líneas por página... hasta tener las páginas */
	t0 = so_tics();
	while (P.d->n_pag < (u32)paginas) {
		for (k = 0; k < 20; k++) {
			for (i = 0; i < 3; i++)
				ed_escribe(&P.e, fl, n);
			ed_intro(&P.e);
		}
		maq_documento(P.d);
	}
	t1 = so_tics();
	dice("pluma: mide: documento de %u páginas, %u párrafos, hecho en %u ms", P.d->n_pag, P.d->n_par, (t1 - t0) * 1000 / hz);
	/* en medio del documento */
	{
		pos c;

		c.p = P.d->n_par / 2;
		c.o = P.d->par[c.p].n / 2;
		ed_cursor(&P.e, c, 0);
	}
	/* la ventana, si hay escritorio (si no, solo maquetar) */
	{
		int an, al;

		so_pantalla(&an, &al);
		if (prepara_ventana())
			ventana = 1;
		(void)an;
		(void)al;
	}
	maq_documento(P.d);
	if (ventana) {
		coloca();
		vista_al_cursor(&P.vi);
		pl_pinta_todo();
	}
	t0 = so_tics();
	{
		u32 peor = 0, ta = so_tics();

		for (k = 0; k < letras; k++) {
			u32 tb;

			ed_escribe_letra(&P.e, k % 7 == 6 ? ' ' : 'a' + (u32)(k % 26));
			maq_documento(P.d);
			if (ventana) {
				vista_al_cursor(&P.vi);
				pl_pinta_todo();
			}
			tb = so_tics();
			if (tb - ta > peor)
				peor = tb - ta;
			ta = tb;
		}
		t1 = so_tics();
		dice("pluma: mide: %d letras en %u ms: %u ms por letra (la peor, %u ms), con %s; pintar la ventana, %u ms",
		     letras, (t1 - t0) * 1000 / hz, letras ? (t1 - t0) * 1000 / hz / (u32)letras : 0, peor * 1000 / hz,
		     ventana ? "la ventana" : "solo la maqueta", tics_pintar * 1000 / hz);
	}
	/* bajar por el documento: Av Pág hasta el final */
	if (ventana) {
		so_suceso s;

		memset(&s, 0, sizeof s);
		s.clase = SO_TECLA;
		s.tecla = SO_T_AV_PAG;
		t0 = so_tics();
		for (k = 0; k < 20; k++) {
			tecla(&s);
			maq_documento(P.d);
			coloca();
			vista_al_cursor(&P.vi);
			pl_pinta_todo();
		}
		t1 = so_tics();
		dice("pluma: mide: Av Pág: %u ms cada una", (t1 - t0) * 1000 / hz / 20);
	}
	dice("pluma: mide: memoria: %u KB (letras: %u KB)", so_memoria_usada() / 1024, letras_memoria() / 1024);
	ed_suelta(&P.e);
	doc_suelta(P.d);
	if (ventana) {
		lz_suelta(&P.l);
		so_cierra_ventana(P.v);
	}
	return 0;
}
