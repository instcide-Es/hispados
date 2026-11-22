/* ventana.c - Pintura 2: la ventana. Arriba, la barra de menús (Archivo,
 * Edición, Ver, Imagen, Colores, Herramientas y ?) y la barra de opciones
 * (archivo y edición, y lo de la herramienta: grosor, forma, suavizado,
 * relleno, tolerancia, letra...; y el zoom); a la izquierda, la caja de
 * herramientas (con su nombre debajo); en medio, las reglas y el lienzo;
 * abajo, la paleta (el color 1 y el 2 con su nombre, siempre a la vista, y
 * los 28 colores) y la barra de estado (dónde está el ratón y el nombre
 * del color que tiene debajo, lo elegido, el lienzo, el zoom). Se pinta
 * solo lo que cambia, y al sistema va solo eso (so_mapa por trozos).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include <stdarg.h>
#include "pintura.h"
#include "formatos.h"
#include "operaciones.h"
#include "letras.h"

pintura_t P;

#define ETIQ_MENU 0x100
#define ETIQ_LISTA 0x300
#define ETIQ_CONTEXTO 0x200
#define CON_FLECHAS 0x40000000u

#define CAJA 68
#define BOTON 28
#define PAL_AL 52

static const char *const MENUS = "&Archivo|&Edición|&Ver|&Imagen|&Colores|&Herramientas|&?";
#define N_MENUS 7

typedef struct {
	const char *texto;
	int orden;
} opcion_t;

#define R { "-", O_NADA }
static const opcion_t m_archivo[] = {
	{ "Nuevo... (Ctrl+U)", O_NUEVO }, { "Abrir... (Ctrl+A)", O_ABRIR }, { "Guardar (Ctrl+G)", O_GUARDAR },
	{ "Guardar como... (F12)", O_GUARDAR_COMO }, R, { "Exportar a PDF...", O_PDF }, { "Imprimir... (Ctrl+P)", O_IMPRIMIR }, R,
	{ "Usar como fondo de escritorio", O_FONDO }, { "Propiedades de la imagen...", O_PROPIEDADES }, R, { "Salir", O_SALIR }, { NULL, 0 }
};
static const opcion_t m_edicion[] = {
	{ "Deshacer (Ctrl+Z)", O_DESHACER }, { "Rehacer (Ctrl+Y)", O_REHACER }, R, { "Cortar (Ctrl+X)", O_CORTAR },
	{ "Copiar (Ctrl+C)", O_COPIAR }, { "Pegar (Ctrl+V)", O_PEGAR }, { "Borrar la selección (Supr)", O_BORRAR_SEL },
	{ "Seleccionar todo (Ctrl+E)", O_TODO }, { "Quitar la selección (Esc)", O_SIN_SEL }, R,
	{ "Copiar a un archivo...", O_COPIAR_A }, { "Pegar desde un archivo...", O_PEGAR_DE }, { NULL, 0 }
};
static const opcion_t m_ver[] = {
	{ "Caja de herramientas", O_VER_CAJA }, { "Barra de opciones", O_VER_OPCIONES }, { "Paleta de colores", O_VER_PALETA },
	{ "Barra de estado", O_VER_ESTADO }, { "Reglas", O_VER_REGLAS }, { "Rejilla (con zoom de 400 % o más)", O_VER_REJILLA }, R,
	{ "Acercar (+)", O_ACERCAR }, { "Alejar (-)", O_ALEJAR }, { "Tamaño real (100 %)", O_ZOOM_100 }, { "Ajustar a la ventana", O_AJUSTAR },
	{ NULL, 0 }
};
static const opcion_t m_imagen[] = {
	{ "Cambiar el tamaño... (Ctrl+W)", O_TAMANO }, { "Estirar e inclinar...", O_INCLINAR }, R,
	{ "Girar 90° a la derecha (Ctrl+R)", O_GIRA_DER }, { "Girar 90° a la izquierda", O_GIRA_IZQ }, { "Girar 180°", O_GIRA_180 },
	{ "Girar...", O_GIRAR }, { "Voltear de izquierda a derecha", O_VOLTEA_H }, { "Voltear de arriba abajo", O_VOLTEA_V }, R,
	{ "Recortar a la selección (Ctrl+K)", O_RECORTAR }, { "Atributos del lienzo... (Ctrl+T)", O_ATRIBUTOS },
	{ "Borrar la imagen (con el color 2)", O_LIMPIAR }, { NULL, 0 }
};
static const opcion_t m_colores[] = {
	{ "Editar colores...", O_EDITAR_COLORES }, { "Intercambiar el color 1 y el 2 (X)", O_CAMBIA_COLORES }, R,
	{ "Invertir los colores (Ctrl+N)", O_INVERTIR }, { "Escala de grises", O_GRISES }, { "Brillo y contraste...", O_BRILLO }, R,
	{ "Volver a la paleta de fábrica", O_PALETA_FABRICA }, { NULL, 0 }
};
static opcion_t m_herr[H_N + 1];
static char t_herr[H_N][64];
static const opcion_t m_ayuda[] = {
	{ "Ayuda de Pintura (F1)", O_AYUDA }, { "Las teclas", O_TECLAS }, R, { "Acerca de Pintura", O_ACERCA }, { NULL, 0 }
};
static const opcion_t *menus[N_MENUS] = { m_archivo, m_edicion, m_ver, m_imagen, m_colores, m_herr, m_ayuda };

/* ---- las listas de la barra de opciones ---- */
static const int grosores_pincel[] = { 1, 2, 3, 4, 5, 8, 12, 16, 24, 32, 48 };
static const int grosores_figura[] = { 1, 2, 3, 4, 5, 8, 12, 16 };
static const int grosores_borrador[] = { 4, 8, 12, 16, 24, 32, 48 };
static const int radios_aero[] = { 5, 8, 12, 16, 24, 32 };
static const int tolerancias[] = { 0, 5, 10, 15, 25, 40, 60 };
static const int tamanos[] = { 8, 9, 10, 11, 12, 14, 16, 18, 20, 24, 28, 32, 36, 48, 60, 72, 96 };
static const char *const FORMAS = "Redonda|Cuadrada|Barra inclinada /|Barra inclinada \\";
static const char *const RELLENOS = "Solo el contorno|Solo el relleno (color 1)|Contorno (1) y relleno (2)";
static const char *const DENSIDADES = "Poca|Normal|Mucha";

/* la barra de opciones (la hace arma_barra para cada herramienta) */
static ui_boton barra[32];
static int n_barra;
/* la geometría */
static int y_barra, y_cuerpo, y_pal, y_estado, x_vista, y_vista;
static ui_desp desp_v, desp_h;
static int encima_menu = -1, menu_abierto = -1, encima_barra = -1, pulsado_barra = -1;
static int encima_herr = -1, pulsado_herr = -1, encima_pal = -1, encima_cambia;
static int arrastra_desp, arr_de, arr_pos0;
static int raton_x = -1, raton_y = -1;	/* (sobre el lienzo) */
static char globo[200];
static int globo_x, globo_y;
static int ultimo_paleta = -1;
/* lo que hay que pintar */
static int s_menus, s_barra, s_caja, s_reglas, s_paleta, s_estado, s_desp;
static int zv[4];			/* la zona de la vista (en la pantalla) */
static u32 tics_pintar, peor_pintar;

/* la opción n de «a|b|c» */
static void opcion_de(const char *ops, int n, char *d, u32 cabe)
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
/*  Lo pequeño                                                           */
/* ===================================================================== */
void pin_aviso(const char *f, ...)
{
	va_list a;

	va_start(a, f);
	vfmt(P.aviso, sizeof P.aviso, f, a);
	va_end(a);
	dice("pintura: aviso: %s", P.aviso);
	s_estado = 1;
	P.sucio = 1;
}

void pin_titulo(void)
{
	char t[300];

	fmt(t, sizeof t, "%s%s - Pintura", P.ruta[0] ? sin_carpeta(P.ruta) : "Sin título", P.cambiado ? " *" : "");
	so_titulo(P.v, t);
}

void pin_repinta_todo(void)
{
	P.todo = 1;
	P.sucio = 1;
}

static void suma_pantalla(int x0, int y0, int x1, int y1)
{
	if (x1 <= x0 || y1 <= y0)
		return;
	if (zv[2] <= zv[0] || zv[3] <= zv[1]) {
		zv[0] = x0;
		zv[1] = y0;
		zv[2] = x1;
		zv[3] = y1;
	} else {
		if (x0 < zv[0])
			zv[0] = x0;
		if (y0 < zv[1])
			zv[1] = y0;
		if (x1 > zv[2])
			zv[2] = x1;
		if (y1 > zv[3])
			zv[3] = y1;
	}
	P.sucio = 1;
}

void pin_repinta_zona(int zona[4])
{
	int x0, y0, x1, y1;

	if (!zona || zona[2] <= zona[0] || zona[3] <= zona[1])
		return;
	vista_zona(zona, &x0, &y0, &x1, &y1);
	/* (las asas y el marco, que van al lado) */
	suma_pantalla(x0 - 2, y0 - 2, x1 + 10, y1 + 10);
	s_estado = 1;
}

void pin_elige_color(int cual, u32 c)
{
	if (cual == 2)
		P.c2 = c;
	else
		P.c1 = c;
	s_paleta = s_estado = 1;
	P.sucio = 1;
	if (P.tx.activa)
		herr_texto_rehaz();
}

static void pon_cursor(void);

void pin_pon_herramienta(int h)
{
	if (h < 0 || h >= H_N)
		return;
	if (h != P.herr) {
		herr_fija();
		if (h != H_SELECCION && h != H_LIBRE)
			sel_nada();
		if (h == H_CUENTAGOTAS)
			P.herr_antes = P.herr;
		P.herr = h;
	}
	pin_aviso("%s", herr_ayuda(h));
	pon_cursor();
	dice("pintura: herramienta %s", herr_nombre(h));
	pin_repinta_todo();
}

/* ===================================================================== */
/*  La barra de opciones                                                 */
/* ===================================================================== */
static ui_boton *mete(int id, int tipo, int icono, const char *ayuda, int ancho)
{
	ui_boton *b;

	if (n_barra >= (int)(sizeof barra / sizeof barra[0]))
		return &barra[n_barra - 1];
	b = &barra[n_barra++];
	memset(b, 0, sizeof *b);
	b->id = id;
	b->tipo = tipo;
	b->icono = icono;
	b->ayuda = ayuda;
	b->ancho = ancho;
	return b;
}

static int grosor_de_herr(void)
{
	switch (P.herr) {
	case H_PINCEL: return P.grosor_pincel;
	case H_BORRADOR: return P.grosor_borrador;
	case H_AEROGRAFO: return P.radio_aero;
	}
	return P.grosor_figura;
}

static void arma_barra(void)
{
	char t[64];
	ui_boton *b;
	int h = P.herr, estrecha = P.l.an < 900;

	n_barra = 0;
	mete(O_NUEVO, UB_BOTON, IC_NUEVO, "Imagen nueva (Ctrl+U)", 0);
	mete(O_ABRIR, UB_BOTON, IC_ABRIR, "Abrir una imagen (Ctrl+A)", 0);
	mete(O_GUARDAR, UB_BOTON, IC_GUARDAR, "Guardar (Ctrl+G)", 0);
	mete(0, UB_SEPARA, 0, NULL, 0);
	mete(O_CORTAR, UB_BOTON, IC_CORTAR, "Cortar lo elegido (Ctrl+X)", 0)->apagado = !sel_hay();
	mete(O_COPIAR, UB_BOTON, IC_COPIAR, "Copiar lo elegido (Ctrl+C)", 0)->apagado = !sel_hay();
	mete(O_PEGAR, UB_BOTON, IC_PEGAR, "Pegar (Ctrl+V)", 0)->apagado = !P.papel.p;
	mete(0, UB_SEPARA, 0, NULL, 0);
	mete(O_DESHACER, UB_BOTON, IC_DESHACER, "Deshacer (Ctrl+Z)", 0)->apagado = !his_se_deshace() && !his_en_paso();
	mete(O_REHACER, UB_BOTON, IC_REHACER, "Rehacer (Ctrl+Y)", 0)->apagado = !his_se_rehace();
	mete(0, UB_SEPARA, 0, NULL, 0);
	switch (h) {
	case H_PINCEL:
		b = mete(O_L_FORMA, UB_LISTA, 0, "La forma del pincel", estrecha ? 120 : 150);
		opcion_de(FORMAS, P.forma, b->texto, sizeof b->texto);
		__attribute__((fallthrough));	/* (sigue con el grosor) */
	case H_BORRADOR:
	case H_AEROGRAFO:
	case H_RAYA:
	case H_CURVA:
	case H_RECTANGULO:
	case H_REDONDEADO:
	case H_ELIPSE:
	case H_POLIGONO:
		b = mete(O_L_GROSOR, UB_LISTA, 0, h == H_AEROGRAFO ? "El tamaño del rociado" : "El grosor", 124);
		fmt(b->texto, sizeof b->texto, "%s: %d px", h == H_AEROGRAFO ? "Radio" : "Grosor", grosor_de_herr());
		break;
	}
	if (h == H_RECTANGULO || h == H_REDONDEADO || h == H_ELIPSE || h == H_POLIGONO) {
		b = mete(O_L_RELLENO, UB_LISTA, 0, "Contorno, relleno o los dos", estrecha ? 170 : 240);
		opcion_de(RELLENOS, P.relleno, b->texto, sizeof b->texto);
	}
	if (h == H_PINCEL || h == H_RAYA || h == H_CURVA || h == H_RECTANGULO || h == H_REDONDEADO || h == H_ELIPSE || h == H_POLIGONO) {
		b = mete(O_L_SUAVE, UB_LISTA, 0, "Los bordes suavizados (o en escalera, punto a punto)", 130);
		copia(b->texto, P.suave ? "Suavizado: sí" : "Suavizado: no", sizeof b->texto);
	}
	if (h == H_BORRADOR) {
		b = mete(O_L_BORRA, UB_LISTA, 0, "Lo que deja el borrador", 190);
		copia(b->texto, P.borra_transparente ? "Deja transparente" : "Pinta con el color 2", sizeof b->texto);
	}
	if (h == H_AEROGRAFO) {
		b = mete(O_L_DENSIDAD, UB_LISTA, 0, "Cuántas gotas", 140);
		fmt(t, sizeof t, "Densidad: %s", P.densidad == 0 ? "poca" : P.densidad == 2 ? "mucha" : "normal");
		copia(b->texto, t, sizeof b->texto);
	}
	if (h == H_RELLENO) {
		b = mete(O_L_TOLERANCIA, UB_LISTA, 0, "Cuánto se pueden parecer los colores que se rellenan", 150);
		fmt(b->texto, sizeof b->texto, "Tolerancia: %d %%", P.tolerancia);
	}
	if (h == H_SELECCION || h == H_LIBRE) {
		b = mete(O_L_SELECCION, UB_LISTA, 0, "Transparente: lo del color 2 no tapa al moverlo ni al pegarlo", 200);
		copia(b->texto, P.sel_transparente ? "Selección transparente" : "Selección opaca", sizeof b->texto);
	}
	if (h == H_TEXTO) {
		b = mete(O_L_LETRA, UB_LISTA, 0, "La letra (las TrueType: las DejaVu y las de \\DOCUMENT\\LETRAS)", estrecha ? 130 : 170);
		copia(b->texto, P.letra, sizeof b->texto);
		b = mete(O_L_TAMANO, UB_LISTA, 0, "El tamaño de la letra (puntos de la imagen)", 60);
		fmt(b->texto, sizeof b->texto, "%d", P.tam_letra);
		mete(O_NEGRITA, UB_CONMUTA, IC_NEGRITA, "Negrita", 0)->activo = P.negrita;
		mete(O_CURSIVA, UB_CONMUTA, IC_CURSIVA, "Cursiva", 0)->activo = P.cursiva;
		b = mete(O_L_FONDO_TEXTO, UB_LISTA, 0, "Detrás de las letras", estrecha ? 130 : 170);
		copia(b->texto, P.fondo_texto ? "Fondo del color 2" : "Fondo transparente", sizeof b->texto);
	}
	mete(0, UB_SEPARA, 0, NULL, 0);
	b = mete(O_L_ZOOM, UB_LISTA, 0, "Zoom (Control y la rueda; + y -)", 100);
	if (P.zoom % 10)
		fmt(b->texto, sizeof b->texto, "%d,%d %%", P.zoom / 10, P.zoom % 10);
	else
		fmt(b->texto, sizeof b->texto, "%d %%", P.zoom / 10);
}

/* ===================================================================== */
/*  Los menús                                                            */
/* ===================================================================== */
static int activa(int o)
{
	switch (o) {
	case O_DESHACER: return his_se_deshace() || his_en_paso();
	case O_REHACER: return his_se_rehace();
	case O_CORTAR: case O_COPIAR: case O_BORRAR_SEL: case O_SIN_SEL: case O_COPIAR_A: case O_RECORTAR:
		return sel_hay();
	case O_PEGAR: return P.papel.p != NULL;
	}
	return 1;
}

static int marcada(int o)
{
	switch (o) {
	case O_VER_CAJA: return P.ver_caja;
	case O_VER_OPCIONES: return P.ver_opciones;
	case O_VER_PALETA: return P.ver_paleta;
	case O_VER_ESTADO: return P.ver_estado;
	case O_VER_REGLAS: return P.ver_reglas;
	case O_VER_REJILLA: return P.ver_rejilla;
	}
	return o >= O_HERR0 && o < O_HERR0 + H_N && o - O_HERR0 == P.herr;
}

static void texto_menu(int m, char *t, u32 cabe, u32 *grises)
{
	const opcion_t *o = menus[m];
	int i;

	t[0] = 0;
	*grises = 0;
	for (i = 0; o[i].texto; i++) {
		if (i)
			anade(t, "|", cabe);
		if (marcada(o[i].orden))
			anade(t, "\xc2\xbb ", cabe);		/* «» » delante: lo encendido */
		anade(t, o[i].texto, cabe);
		if (o[i].orden != O_NADA && !activa(o[i].orden))
			*grises |= 1u << i;
	}
}

static void abre_menu(int m)
{
	char t[1100], r[64];
	u32 g;
	int i, cx = 6, x0 = 6;

	if (m < 0 || m >= N_MENUS)
		return;
	herr_fija();
	for (i = 0; i <= m; i++) {
		opcion_de(MENUS, i, r, sizeof r);
		if (i == m)
			x0 = cx;
		cx += ui_ancho_amp(r) + 16;
	}
	texto_menu(m, t, sizeof t, &g);
	menu_abierto = m;
	so_menu(P.v, x0, ui_alto_menus(), t, g, (ETIQ_MENU + (u32)m) | CON_FLECHAS);
	s_menus = 1;
	P.sucio = 1;
}

/* ===================================================================== */
/*  La geometría y el pintar                                             */
/* ===================================================================== */
static void coloca(void)
{
	int y = ui_alto_menus();

	y_barra = P.ver_opciones ? y : -1000;
	if (P.ver_opciones)
		y += ui_alto_barra();
	y_cuerpo = y;
	y_estado = P.l.al - (P.ver_estado ? ui_alto_estado() : 0);
	y_pal = y_estado - (P.ver_paleta ? PAL_AL : 0);
	x_vista = (P.ver_caja ? CAJA : 0) + (P.ver_reglas ? regla_ancho() : 0);
	y_vista = y_cuerpo + (P.ver_reglas ? regla_alto() : 0);
	vista_coloca(x_vista, y_vista, P.l.an - x_vista - UI_DESP, y_pal - y_vista - UI_DESP);
	desp_v.x = VI.x + VI.an;
	desp_v.y = VI.y;
	desp_v.an = UI_DESP;
	desp_v.al = VI.al;
	desp_h.x = VI.x;
	desp_h.y = VI.y + VI.al;
	desp_h.an = VI.an;
	desp_h.al = UI_DESP;
}

static void pon_cursor(void)
{
	int g = P.herr == H_PINCEL ? P.grosor_pincel : P.herr == H_BORRADOR ? P.grosor_borrador : P.herr == H_AEROGRAFO ? 2 * P.radio_aero : 0;
	int r = (int)((i64)g * P.zoom / 2000);

	if (raton_x < 0 || r < 3) {
		if (cursor_r > 0) {
			int x0, y0, x1, y1;

			vista_cursor_zona(&x0, &y0, &x1, &y1);
			suma_pantalla(x0, y0, x1, y1);
		}
		cursor_r = -1;
		return;
	}
	if (cursor_r > 0) {
		int x0, y0, x1, y1;

		vista_cursor_zona(&x0, &y0, &x1, &y1);
		suma_pantalla(x0, y0, x1, y1);
	}
	cursor_x = raton_x;
	cursor_y = raton_y;
	cursor_r = r;
	{
		int x0, y0, x1, y1;

		vista_cursor_zona(&x0, &y0, &x1, &y1);
		suma_pantalla(x0, y0, x1, y1);
	}
}

static void boton_herr(lienzo *L, int h, int x, int y)
{
	int activo = h == P.herr;

	if (activo) {
		lz_redondeado(L, x, y, BOTON, BOTON, 4, UI_PULSADO);
		lz_marco_redondeado(L, x, y, BOTON, BOTON, 4, UI_ACENTO);
		lz_marco_redondeado(L, x + 1, y + 1, BOTON - 2, BOTON - 2, 3, UI_ACENTO);
		/* (y una raya abajo: no solo el color) */
		lz_rect(L, x + 5, y + BOTON - 4, BOTON - 10, 2, UI_ACENTO);
	} else if (h == encima_herr || h == pulsado_herr) {
		lz_redondeado(L, x, y, BOTON, BOTON, 4, h == pulsado_herr ? UI_PULSADO : UI_ENCIMA);
		lz_marco_redondeado(L, x, y, BOTON, BOTON, 4, UI_RAYA_FUERTE);
	}
	pin_icono(L, h, x + 2, y + 2, BOTON - 4, 0);
}

static void herr_en_caja(int h, int *x, int *y)
{
	*x = 5 + (h % 2) * (BOTON + 3);
	*y = y_cuerpo + 6 + (h / 2) * (BOTON + 3);
}

static int herr_en(int x, int y)
{
	int h;

	for (h = 0; h < H_N; h++) {
		int bx, by;

		herr_en_caja(h, &bx, &by);
		if (x >= bx && y >= by && x < bx + BOTON && y < by + BOTON)
			return h;
	}
	return -1;
}

/* el texto en líneas que quepan en «an» */
static int parrafo(lienzo *L, int x, int y, int an, const char *t, int negrita, u32 c)
{
	char l[80], pal[64];
	const char *s = t;
	int lin = 0;

	l[0] = 0;
	while (*s) {
		u32 k = 0;
		char prueba[160];

		while (*s == ' ')
			s++;
		while (*s && *s != ' ' && k + 1 < sizeof pal)
			pal[k++] = *s++;
		pal[k] = 0;
		if (!k)
			break;
		fmt(prueba, sizeof prueba, "%s%s%s", l, l[0] ? " " : "", pal);
		if (l[0] && (negrita ? lz_ui_ancho_negrita(prueba) : lz_ui_ancho(prueba)) > an) {
			if (L)
				(negrita ? lz_ui_negrita : lz_ui)(L, x, y + lin * lz_ui_alto() + lz_ui_base(), l, c);
			lin++;
			copia(l, pal, sizeof l);
		} else
			copia(l, prueba, sizeof l);
	}
	if (l[0]) {
		if (L)
			(negrita ? lz_ui_negrita : lz_ui)(L, x, y + lin * lz_ui_alto() + lz_ui_base(), l, c);
		lin++;
	}
	return lin;
}

static void pinta_caja(lienzo *L)
{
	int h, y0 = y_cuerpo, al = y_pal - y_cuerpo, yt;

	lz_rect(L, 0, y0, CAJA, al, UI_FONDO);
	lz_raya_v(L, CAJA - 1, y0, al, UI_RAYA);
	for (h = 0; h < H_N; h++) {
		int x, y;

		herr_en_caja(h, &x, &y);
		boton_herr(L, h, x, y);
	}
	/* el nombre de la que está elegida (y su grosor) */
	yt = y_cuerpo + 6 + (H_N / 2) * (BOTON + 3) + 6;
	lz_raya_h(L, 6, yt - 4, CAJA - 12, UI_RAYA);
	lz_recorte(L, 0, y0, CAJA - 1, al);
	yt += parrafo(L, 4, yt, CAJA - 8, herr_nombre(P.herr), 1, UI_TINTA) * lz_ui_alto() + 4;
	if (P.herr == H_PINCEL || P.herr == H_BORRADOR || P.herr == H_RAYA || P.herr == H_CURVA || P.herr == H_RECTANGULO ||
	    P.herr == H_REDONDEADO || P.herr == H_ELIPSE || P.herr == H_POLIGONO || P.herr == H_AEROGRAFO) {
		char t[24];
		int g = grosor_de_herr(), d = g > 40 ? 40 : g;

		fmt(t, sizeof t, "%d px", g);
		lz_ui(L, 6, yt + lz_ui_base(), t, UI_TINTA_GRIS);
		yt += lz_ui_alto() + 2;
		if (P.herr == H_AEROGRAFO)
			lz_marco_redondeado(L, CAJA / 2 - d / 2, yt, d, d, d / 2, UI_TINTA);
		else
			lz_circulo(L, (CAJA / 2) * 16, (yt + d / 2) * 16, d * 8 > 8 ? d * 8 : 8, UI_TINTA);
	}
	lz_sin_recorte(L);
}

/* ---- la paleta ---- */
static int pal_x0(void)
{
	return 252;
}

static int color_en_pal(int x, int y)
{
	int i, px = pal_x0();

	for (i = 0; i < PALETA_N; i++) {
		int cx = px + (i % 14) * 22, cy = y_pal + 5 + (i / 14) * 22;

		if (x >= cx && y >= cy && x < cx + 20 && y < cy + 20)
			return i;
	}
	return -1;
}

static void cuadro_color(lienzo *L, int x, int y, int lado, u32 c)
{
	/* lo transparente, sobre cuadros */
	if (ALFA(c) != 255) {
		int i, j;

		for (j = 0; j < lado; j += 5)
			for (i = 0; i < lado; i += 5)
				lz_rect(L, x + i, y + j, lado - i < 5 ? lado - i : 5, lado - j < 5 ? lado - j : 5, ((i / 5 + j / 5) & 1) ? 0xCDCDCD : 0xFFFFFF);
		lz_rect_a(L, x, y, lado, lado, c & 0xFFFFFF, (int)ALFA(c));
	} else
		lz_rect(L, x, y, lado, lado, c & 0xFFFFFF);
	lz_marco(L, x, y, lado, lado, UI_TINTA);
}

/* la cifra «1» o «2» en una esquina (lo que no depende del color) */
static void marca_cifra(lienzo *L, int x, int y, const char *d)
{
	int cara = letra_cara(FAM_SANS, 1, 0, NULL);

	lz_rect(L, x, y, 9, 11, 0xFFFFFF);
	lz_marco(L, x, y, 9, 11, 0x000000);
	lz_texto(L, cara, 9 * 4, 0, x + 2, y + 9, d, 0x000000);
}

static void pinta_paleta(lienzo *L)
{
	int y = y_pal, i, px = pal_x0();
	char t[140], d[120];

	lz_rect(L, 0, y, P.l.an, PAL_AL, UI_FONDO);
	lz_raya_h(L, 0, y, P.l.an, UI_RAYA);
	/* el color 1 y el 2, con su nombre */
	for (i = 0; i < 2; i++) {
		u32 c = i ? P.c2 : P.c1;
		int cy = y + 5 + i * 22;

		lz_ui_negrita(L, 6, cy + 1 + lz_ui_base() - 3, i ? "2" : "1", UI_TINTA);
		cuadro_color(L, 18, cy, 20, c);
		color_describe(c, d, sizeof d);
		fmt(t, sizeof t, "%s", d);
		lz_recorte(L, 42, y, 180, PAL_AL);
		lz_ui(L, 42, cy + lz_ui_base() - 2, t, UI_TINTA);
		lz_sin_recorte(L);
	}
	/* intercambiar */
	{
		int bx = 224, by = y + 10;

		if (encima_cambia)
			lz_redondeado(L, bx - 2, by - 2, 24, 30, 3, UI_ENCIMA);
		lz_linea(L, (bx + 4) * 16, (by + 6) * 16, (bx + 17) * 16, (by + 6) * 16, 24, UI_TINTA);
		lz_linea(L, (bx + 13) * 16, (by + 2) * 16, (bx + 17) * 16, (by + 6) * 16, 24, UI_TINTA);
		lz_linea(L, (bx + 3) * 16, (by + 19) * 16, (bx + 16) * 16, (by + 19) * 16, 24, UI_TINTA);
		lz_linea(L, (bx + 3) * 16, (by + 19) * 16, (bx + 7) * 16, (by + 23) * 16, 24, UI_TINTA);
	}
	for (i = 0; i < PALETA_N; i++) {
		int cx = px + (i % 14) * 22, cy = y + 5 + (i / 14) * 22;

		cuadro_color(L, cx, cy, 20, paleta[i]);
		if (i == encima_pal)
			lz_marco_grueso(L, cx - 2, cy - 2, 24, 24, 2, UI_ACENTO);
		if (paleta[i] == P.c1)
			marca_cifra(L, cx, cy, "1");
		if (paleta[i] == P.c2)
			marca_cifra(L, cx + 11, cy + 9, "2");
	}
	/* editar */
	{
		int bx = px + 14 * 22 + 10, w = 130;

		if (bx + w < P.l.an) {
			lz_redondeado(L, bx, y + 14, w, 24, 3, UI_BLANCO);
			lz_marco_redondeado(L, bx, y + 14, w, 24, 3, UI_RAYA_FUERTE);
			lz_ui(L, bx + (w - lz_ui_ancho("Editar colores...")) / 2, y + 14 + (24 - lz_ui_alto()) / 2 + lz_ui_base(), "Editar colores...", UI_TINTA);
		}
	}
}

static int en_editar_colores(int x, int y)
{
	int bx = pal_x0() + 14 * 22 + 10;

	return x >= bx && x < bx + 130 && y >= y_pal + 14 && y < y_pal + 38;
}

static int en_cambia(int x, int y)
{
	return x >= 222 && x < 246 && y >= y_pal + 8 && y < y_pal + 40;
}

/* ---- la barra de estado ---- */
static char aviso_corto[300];

static void texto_estado(char *t, u32 cabe)
{
	char pos[160], sel[60], lz[60], zm[24], c[120];

	pos[0] = sel[0] = 0;
	if (raton_x >= 0) {
		int ix, iy;

		vista_a_lamina(raton_x, raton_y, &ix, &iy);
		if (ix >= 0 && iy >= 0 && ix < P.img.an && iy < P.img.al) {
			color_describe(P.img.p[(size_t)iy * P.img.an + ix], c, sizeof c);
			fmt(pos, sizeof pos, "%d, %d px: %s", ix, iy, c);
		} else
			fmt(pos, sizeof pos, "%d, %d px", ix, iy);
	}
	if (P.sel.activa)
		fmt(sel, sizeof sel, "Elegido: %d x %d", P.sel.flota ? P.sel.flotante.an : P.sel.an, P.sel.flota ? P.sel.flotante.al : P.sel.al);
	fmt(lz, sizeof lz, "Lienzo: %d x %d", P.img.an, P.img.al);
	if (P.zoom % 10)
		fmt(zm, sizeof zm, "%d,%d %%", P.zoom / 10, P.zoom % 10);
	else
		fmt(zm, sizeof zm, "%d %%", P.zoom / 10);
	{
		/* (el aviso, recortado a lo que quepa: lo demás se ve siempre) */
		char resto[400];
		int cabe_av;
		u32 k;

		fmt(resto, sizeof resto, "%s   %s   %s   %s   %s", pos, sel, lz, zm, P.ver_paleta ? "" : "1: Negro (#000000) · 2: Blanco (#FFFFFF)");
		cabe_av = P.l.an - lz_ui_ancho(resto) - 60;
		copia(aviso_corto, P.aviso, sizeof aviso_corto);
		if (lz_ui_ancho(aviso_corto) > cabe_av) {
			while ((k = largo(aviso_corto)) > 0 && lz_ui_ancho(aviso_corto) + lz_ui_ancho("...") > cabe_av) {
				while (k > 1 && ((u8)aviso_corto[k - 1] & 0xC0) == 0x80)
					k--;
				aviso_corto[k - 1] = 0;
			}
			anade(aviso_corto, "...", sizeof aviso_corto);
		}
	}
	if (!P.ver_paleta) {
		char c1[100], c2[100];

		color_describe(P.c1, c1, sizeof c1);
		color_describe(P.c2, c2, sizeof c2);
		fmt(t, cabe, "%s|%s|1: %s · 2: %s|%s|%s|%s", aviso_corto, pos, c1, c2, sel, lz, zm);
	} else
		fmt(t, cabe, "%s|%s|%s|%s|%s", aviso_corto, pos, sel, lz, zm);
}

/* ---- el globo de ayuda ---- */
static void pon_globo(const char *t, int x, int y)
{
	if (!t) {
		if (globo[0]) {
			globo[0] = 0;
			pin_repinta_todo();
		}
		return;
	}
	if (iguales(t, globo) && x == globo_x && y == globo_y)
		return;
	copia(globo, t, sizeof globo);
	globo_x = x;
	globo_y = y;
	pin_repinta_todo();
}

static void pinta_globo(lienzo *L)
{
	int w, h, x, y;

	if (!globo[0])
		return;
	w = lz_ui_ancho(globo) + 12;
	h = lz_ui_alto() + 6;
	x = globo_x;
	y = globo_y;
	if (x + w > L->an - 2)
		x = L->an - 2 - w;
	if (x < 2)
		x = 2;
	if (y + h > L->al - 2)
		y = globo_y - h - 28;
	lz_rect(L, x + 2, y + 2, w, h, 0x9AA3AF);
	lz_rect(L, x, y, w, h, UI_AVISO);
	lz_marco(L, x, y, w, h, UI_TINTA);
	lz_ui(L, x + 6, y + 3 + lz_ui_base(), globo, UI_TINTA);
}

/* ---- todo ---- */
static void al_sistema(int x0, int y0, int x1, int y1)
{
	if (x0 < 0)
		x0 = 0;
	if (y0 < 0)
		y0 = 0;
	if (x1 > P.l.an)
		x1 = P.l.an;
	if (y1 > P.l.al)
		y1 = P.l.al;
	if (x1 <= x0 || y1 <= y0)
		return;
	if (x0 == 0 && x1 == P.l.an)
		so_mapa(P.v, 0, y0, P.l.an, y1 - y0, P.l.p + (size_t)y0 * P.l.paso, P.l.paso);
	else
		so_mapa(P.v, x0, y0, x1 - x0, y1 - y0, P.l.p + (size_t)y0 * P.l.paso + x0, P.l.paso);
}

static void pinta_desp(lienzo *L)
{
	desp_v.total = vista_total_y();
	desp_v.visto = VI.al;
	desp_v.pos = P.dy;
	desp_h.total = vista_total_x();
	desp_h.visto = VI.an;
	desp_h.pos = P.dx;
	ui_desp_pinta(L, &desp_v, arrastra_desp == 1 ? 5 : 0);
	ui_desp_pinta(L, &desp_h, arrastra_desp == 2 ? 5 : 0);
	lz_rect(L, VI.x + VI.an, VI.y + VI.al, UI_DESP, UI_DESP, UI_FONDO);
}

static void pinta_reglas(lienzo *L)
{
	if (!P.ver_reglas)
		return;
	lz_rect(L, x_vista - regla_ancho(), y_cuerpo, regla_ancho(), regla_alto(), UI_FONDO);
	regla_pinta(L, 1, x_vista, y_cuerpo, P.l.an - x_vista, regla_alto(), raton_x);
	regla_pinta(L, 0, x_vista - regla_ancho(), y_vista, regla_ancho(), y_pal - y_vista, raton_y);
}

void pin_pinta(void)
{
	char t[600];
	u32 t0 = so_tics();
	int todo;
	static int geo = -1;

	if (!P.l.p)
		return;
	coloca();
	todo = P.todo || P.cuadro || globo[0] || geo != (P.l.an ^ P.l.al << 12 ^ y_vista << 24 ^ x_vista << 20);
	geo = P.l.an ^ P.l.al << 12 ^ y_vista << 24 ^ x_vista << 20;
	if (todo || s_barra)
		arma_barra();
	if (todo || s_menus) {
		ui_barra_menus(&P.l, 0, 0, P.l.an, MENUS, menu_abierto, encima_menu);
		lz_raya_h(&P.l, 0, ui_alto_menus() - 1, P.l.an, UI_RAYA);
		if (!todo)
			al_sistema(0, 0, P.l.an, ui_alto_menus());
	}
	if (P.ver_opciones && (todo || s_barra)) {
		ui_barra(&P.l, 0, y_barra, P.l.an, barra, n_barra, encima_barra, pulsado_barra);
		if (!todo)
			al_sistema(0, y_barra, P.l.an, y_barra + ui_alto_barra());
	}
	if (P.ver_caja && (todo || s_caja)) {
		pinta_caja(&P.l);
		if (!todo)
			al_sistema(0, y_cuerpo, CAJA, y_pal);
	}
	if (todo) {
		vista_pinta(&P.l, VI.x, VI.y, VI.x + VI.an, VI.y + VI.al);
		pinta_desp(&P.l);
	} else {
		if (zv[2] > zv[0] && zv[3] > zv[1]) {
			vista_pinta(&P.l, zv[0], zv[1], zv[2], zv[3]);
			al_sistema(zv[0] > VI.x ? zv[0] : VI.x, zv[1] > VI.y ? zv[1] : VI.y, zv[2] < VI.x + VI.an ? zv[2] : VI.x + VI.an,
				   zv[3] < VI.y + VI.al ? zv[3] : VI.y + VI.al);
		}
		if (s_desp) {
			pinta_desp(&P.l);
			al_sistema(VI.x, VI.y, VI.x + VI.an + UI_DESP, VI.y + VI.al + UI_DESP);
			al_sistema(VI.x + VI.an, VI.y, VI.x + VI.an + UI_DESP, VI.y + VI.al);
		}
	}
	if (P.ver_reglas && (todo || s_reglas)) {
		pinta_reglas(&P.l);
		if (!todo) {
			al_sistema(x_vista - regla_ancho(), y_cuerpo, P.l.an, y_cuerpo + regla_alto());
			al_sistema(x_vista - regla_ancho(), y_vista, x_vista, y_pal);
		}
	}
	if (P.ver_paleta && (todo || s_paleta)) {
		pinta_paleta(&P.l);
		if (!todo)
			al_sistema(0, y_pal, P.l.an, y_pal + PAL_AL);
	}
	if (P.ver_estado && (todo || s_estado)) {
		texto_estado(t, sizeof t);
		ui_estado(&P.l, 0, y_estado, P.l.an, t);
		if (!todo)
			al_sistema(0, y_estado, P.l.an, P.l.al);
	}
	if (todo) {
		pinta_globo(&P.l);
		if (P.cuadro)
			ui_cuadro_pinta(&P.l, P.cuadro);
		al_sistema(0, 0, P.l.an, P.l.al);
	}
	s_menus = s_barra = s_caja = s_reglas = s_paleta = s_estado = s_desp = 0;
	zv[0] = zv[1] = zv[2] = zv[3] = 0;
	P.todo = 0;
	P.sucio = 0;
	tics_pintar = so_tics() - t0;
	if (tics_pintar > peor_pintar)
		peor_pintar = tics_pintar;
}

void pin_cambio_imagen(void)
{
	sel_nada();
	coloca();
	vista_limita();
	pin_titulo();
	pin_repinta_todo();
}

/* ===================================================================== */
/*  Las órdenes                                                          */
/* ===================================================================== */
void pin_aplica(int que, int a, int b, double g, const char *nombre)
{
	lamina *s = NULL, n, antes;
	int ok = 1, nueva = 0;

	if (que != A_LIENZO && que != A_RECORTA && que != A_LIMPIA) {
		/* (con lo elegido: solo a eso) */
		if (P.tx.activa || his_en_paso()) {
			if (!P.sel.flota)
				herr_fija();
		}
		s = sel_lamina_para_operar();
	} else
		herr_fija();
	memset(&n, 0, sizeof n);
	{
		lamina *o = s ? s : &P.img;

		switch (que) {
		case A_GIRA90: ok = op_gira90(o, &n, a); nueva = 1; break;
		case A_GIRA: ok = op_gira(o, &n, g, s ? 0 : P.c2); nueva = 1; break;
		case A_ESCALA: ok = op_escala(o, &n, a, b); nueva = 1; break;
		case A_INCLINA: ok = op_inclina(o, &n, a, b, s ? 0 : P.c2); nueva = 1; break;
		case A_LIENZO: ok = op_lienzo(o, &n, a, b, P.c2); nueva = 1; break;
		case A_RECORTA:
			ok = P.sel.activa && lam_trozo(&n, o, P.sel.x, P.sel.y, P.sel.an, P.sel.al);
			nueva = 1;
			break;
		default:
			if (!s) {
				/* (en el sitio: lo de antes, para deshacer) */
				if (!lam_nueva(&antes, P.img.an, P.img.al, 0)) {
					ok = 0;
					break;
				}
				memcpy(antes.p, P.img.p, (size_t)P.img.an * P.img.al * 4);
			}
			switch (que) {
			case A_VOLTEA: op_voltea(o, a); break;
			case A_INVIERTE: op_invierte(o); break;
			case A_GRISES: op_grises(o); break;
			case A_BRILLO: op_brillo(o, a, b); break;
			case A_LIMPIA: {
				u32 i;

				for (i = 0; i < (u32)o->an * (u32)o->al; i++)
					o->p[i] = P.c2;
				break;
			}
			}
			if (!s) {
				his_entera(nombre, &antes);
				P.cambiado = 1;
			}
		}
	}
	if (!ok) {
		dlg_aviso("Pintura", "No hay memoria para hacerlo (o la imagen pasaría de 4096 x 4096 puntos).");
		return;
	}
	if (nueva) {
		if (s) {
			lam_suelta(s);
			*s = n;
			sel_cambiada();
		} else {
			antes = P.img;
			P.img = n;
			his_lamina(&P.img, 0);
			his_entera(nombre, &antes);
			P.cambiado = 1;
			pin_cambio_imagen();
		}
	}
	if (que == A_RECORTA)
		sel_nada();
	pin_titulo();
	pin_aviso("%s%s", nombre, s ? " (lo elegido)" : "");
	dice("pintura: %s%s: %d x %d", nombre, s ? " (lo elegido)" : "", s ? s->an : P.img.an, s ? s->al : P.img.al);
	pin_repinta_todo();
}

static void zoom_centro(int z)
{
	vista_zoom_en(z, VI.x + VI.an / 2, VI.y + VI.al / 2);
	pin_aviso("Zoom: %d %%", P.zoom / 10);
	pin_repinta_todo();
}

static void deshaz(int rehaz)
{
	int tam = 0, z[4] = { 0, 0, 0, 0 };
	const char *que;

	if (!rehaz && (P.tx.activa || P.sel.flota || his_en_paso())) {
		herr_cancela();
		pin_aviso("Deshecho");
		pin_repinta_todo();
		return;
	}
	herr_fija();
	sel_nada();
	que = rehaz ? his_que_rehace() : his_que_deshace();
	{
		char q[48];

		copia(q, que, sizeof q);
		if (!(rehaz ? his_rehace(&P.img, &tam, z) : his_deshace(&P.img, &tam, z))) {
			pin_aviso(rehaz ? "No hay nada que rehacer" : "No hay nada que deshacer");
			return;
		}
		pin_aviso("%s: %s", rehaz ? "Rehecho" : "Deshecho", q);
		dice("pintura: %s «%s»", rehaz ? "rehecho" : "deshecho", q);
	}
	P.cambiado = 1;
	if (tam)
		pin_cambio_imagen();
	else
		pin_repinta_zona(z);
	pin_titulo();
	s_barra = 1;
}

void pin_orden(int o)
{
	if (o >= O_HERR0 && o < O_HERR0 + H_N) {
		pin_pon_herramienta(o - O_HERR0);
		return;
	}
	switch (o) {
	case O_NUEVO:
		herr_fija();
		if (P.cambiado)
			dlg_guardar_cambios(O_NUEVO);
		else
			dlg_nuevo();
		break;
	case O_ABRIR:
		herr_fija();
		if (P.cambiado)
			dlg_guardar_cambios(O_ABRIR);
		else
			dlg_abrir(DLG_ABRIR);
		break;
	case O_GUARDAR:
		herr_fija();
		if (P.ruta[0] && fi_se_escribe(P.formato))
			pin_guarda(P.ruta, P.formato);
		else
			dlg_abrir(DLG_GUARDAR);
		break;
	case O_GUARDAR_COMO:
		herr_fija();
		dlg_abrir(DLG_GUARDAR);
		break;
	case O_PDF:
		herr_fija();
		dlg_abrir(DLG_PDF);
		break;
	case O_IMPRIMIR:
		herr_fija();
		dlg_abrir(DLG_PDF);
		pin_aviso("Aún no hay impresoras en Hispa-Pentium: la imagen va a un PDF, para imprimirla en otro equipo");
		break;
	case O_FONDO:
		herr_fija();
		pin_fondo();
		break;
	case O_PROPIEDADES:
		herr_fija();
		dlg_propiedades();
		break;
	case O_SALIR:
		herr_fija();
		if (P.cambiado)
			dlg_guardar_cambios(O_SALIR);
		else
			P.sale = 1;
		break;
	case O_DESHACER: deshaz(0); break;
	case O_REHACER: deshaz(1); break;
	case O_CORTAR:
		if (sel_copia()) {
			sel_borra();
			pin_aviso("Cortado (%d x %d): Ctrl+V lo pega", P.papel.an, P.papel.al);
		}
		break;
	case O_COPIAR:
		if (sel_copia()) {
			pin_aviso("Copiado (%d x %d): Ctrl+V lo pega dentro de Pintura (el portapapeles del sistema aún no lleva imágenes)", P.papel.an, P.papel.al);
			dice("pintura: copiado %d x %d", P.papel.an, P.papel.al);
		}
		break;
	case O_PEGAR:
		if (P.papel.p) {
			int x, y;

			vista_a_lamina(VI.x, VI.y, &x, &y);
			sel_pega(&P.papel, x < 0 ? 0 : x, y < 0 ? 0 : y, "Pegar");
			pin_aviso("Pegado (%d x %d): arrástrelo donde lo quiera; un clic fuera lo deja", P.papel.an, P.papel.al);
			dice("pintura: pegado %d x %d", P.papel.an, P.papel.al);
		} else
			pin_aviso("No hay nada copiado (el portapapeles del sistema aún no lleva imágenes)");
		break;
	case O_BORRAR_SEL: sel_borra(); break;
	case O_TODO: sel_todo(); break;
	case O_SIN_SEL: sel_nada(); break;
	case O_COPIAR_A:
		if (sel_hay())
			dlg_abrir(DLG_COPIAR_A);
		break;
	case O_PEGAR_DE: dlg_abrir(DLG_PEGAR_DE); break;
	case O_VER_CAJA: P.ver_caja = !P.ver_caja; pin_repinta_todo(); break;
	case O_VER_OPCIONES: P.ver_opciones = !P.ver_opciones; pin_repinta_todo(); break;
	case O_VER_PALETA: P.ver_paleta = !P.ver_paleta; pin_repinta_todo(); break;
	case O_VER_ESTADO: P.ver_estado = !P.ver_estado; pin_repinta_todo(); break;
	case O_VER_REGLAS: P.ver_reglas = !P.ver_reglas; pin_repinta_todo(); break;
	case O_VER_REJILLA: P.ver_rejilla = !P.ver_rejilla; pin_repinta_todo(); break;
	case O_ACERCAR: zoom_centro(vista_zoom_siguiente(P.zoom, 1)); break;
	case O_ALEJAR: zoom_centro(vista_zoom_siguiente(P.zoom, 0)); break;
	case O_ZOOM_100: zoom_centro(1000); break;
	case O_AJUSTAR: zoom_centro(vista_zoom_ajustado()); break;
	case O_TAMANO: dlg_tamano(); break;
	case O_INCLINAR: dlg_inclinar(); break;
	case O_GIRA_DER: pin_aplica(A_GIRA90, 1, 0, 0, "Girar 90° a la derecha"); break;
	case O_GIRA_IZQ: pin_aplica(A_GIRA90, 3, 0, 0, "Girar 90° a la izquierda"); break;
	case O_GIRA_180: pin_aplica(A_GIRA90, 2, 0, 0, "Girar 180°"); break;
	case O_GIRAR: dlg_girar(); break;
	case O_VOLTEA_H: pin_aplica(A_VOLTEA, 1, 0, 0, "Voltear de izquierda a derecha"); break;
	case O_VOLTEA_V: pin_aplica(A_VOLTEA, 0, 0, 0, "Voltear de arriba abajo"); break;
	case O_RECORTAR:
		if (sel_hay()) {
			if (P.sel.flota)
				herr_fija();
			pin_aplica(A_RECORTA, 0, 0, 0, "Recortar a la selección");
		}
		break;
	case O_ATRIBUTOS: herr_fija(); dlg_atributos(); break;
	case O_LIMPIAR: pin_aplica(A_LIMPIA, 0, 0, 0, "Borrar la imagen"); break;
	case O_EDITAR_COLORES: dlg_colores(1); break;
	case O_CAMBIA_COLORES: {
		u32 t = P.c1;

		P.c1 = P.c2;
		P.c2 = t;
		pin_elige_color(1, P.c1);
		pin_aviso("Colores intercambiados");
		break;
	}
	case O_INVERTIR: pin_aplica(A_INVIERTE, 0, 0, 0, "Invertir los colores"); break;
	case O_GRISES: pin_aplica(A_GRISES, 0, 0, 0, "Escala de grises"); break;
	case O_BRILLO: dlg_brillo(); break;
	case O_PALETA_FABRICA: paleta_de_fabrica(); s_paleta = 1; P.sucio = 1; break;
	case O_AYUDA: dlg_ayuda(0); break;
	case O_TECLAS: dlg_ayuda(1); break;
	case O_ACERCA: dlg_acerca(); break;
	case O_NEGRITA: P.negrita = !P.negrita; herr_texto_rehaz(); s_barra = 1; P.sucio = 1; break;
	case O_CURSIVA: P.cursiva = !P.cursiva; herr_texto_rehaz(); s_barra = 1; P.sucio = 1; break;
	}
	s_barra = 1;
	P.sucio = 1;
	pin_titulo();
}

/* ---- las listas ---- */
static void lista_de_numeros(char *t, u32 cabe, const int *v, int n, const char *unidad)
{
	int i;

	t[0] = 0;
	for (i = 0; i < n; i++) {
		char x[24];

		fmt(x, sizeof x, "%s%d%s", i ? "|" : "", v[i], unidad);
		anade(t, x, cabe);
	}
}

static void abre_lista(int id, int x, int y)
{
	char t[1100];

	t[0] = 0;
	switch (id) {
	case O_L_GROSOR:
		if (P.herr == H_PINCEL)
			lista_de_numeros(t, sizeof t, grosores_pincel, 11, " px");
		else if (P.herr == H_BORRADOR)
			lista_de_numeros(t, sizeof t, grosores_borrador, 7, " px");
		else if (P.herr == H_AEROGRAFO)
			lista_de_numeros(t, sizeof t, radios_aero, 6, " px");
		else
			lista_de_numeros(t, sizeof t, grosores_figura, 8, " px");
		break;
	case O_L_FORMA: copia(t, FORMAS, sizeof t); break;
	case O_L_SUAVE: copia(t, "Suavizado: sí|Suavizado: no (punto a punto)", sizeof t); break;
	case O_L_RELLENO: copia(t, RELLENOS, sizeof t); break;
	case O_L_TOLERANCIA: lista_de_numeros(t, sizeof t, tolerancias, 7, " %"); break;
	case O_L_BORRA: copia(t, "Pinta con el color 2|Deja transparente", sizeof t); break;
	case O_L_DENSIDAD: copia(t, DENSIDADES, sizeof t); break;
	case O_L_SELECCION: copia(t, "Selección opaca|Selección transparente (sin el color 2)", sizeof t); break;
	case O_L_LETRA: {
		const char *f[32];
		int n = letras_familias(f, 31), i;

		for (i = 0; i < n; i++) {
			if (i)
				anade(t, "|", sizeof t);
			anade(t, f[i], sizeof t);
		}
		break;
	}
	case O_L_TAMANO: lista_de_numeros(t, sizeof t, tamanos, 17, ""); break;
	case O_L_FONDO_TEXTO: copia(t, "Fondo transparente|Fondo del color 2", sizeof t); break;
	case O_L_ZOOM: copia(t, "12,5 %|25 %|50 %|100 %|200 %|300 %|400 %|600 %|800 %|1200 %|1600 %|3200 %|-|Ajustar a la ventana", sizeof t); break;
	default: return;
	}
	so_menu(P.v, x, y, t, 0, ETIQ_LISTA + (u32)id);
}

static void elige_de_lista(int id, int op)
{
	static const int zooms_l[] = { 125, 250, 500, 1000, 2000, 3000, 4000, 6000, 8000, 12000, 16000, 32000 };

	if (op < 0)
		return;
	switch (id) {
	case O_L_GROSOR:
		if (P.herr == H_PINCEL && op < 11)
			P.grosor_pincel = grosores_pincel[op];
		else if (P.herr == H_BORRADOR && op < 7)
			P.grosor_borrador = grosores_borrador[op];
		else if (P.herr == H_AEROGRAFO && op < 6)
			P.radio_aero = radios_aero[op];
		else if (op < 8)
			P.grosor_figura = grosores_figura[op];
		pin_aviso("Grosor: %d px", grosor_de_herr());
		break;
	case O_L_FORMA: if (op < FORMA_N) P.forma = op; break;
	case O_L_SUAVE: P.suave = op == 0; break;
	case O_L_RELLENO: if (op < 3) P.relleno = op; break;
	case O_L_TOLERANCIA: if (op < 7) P.tolerancia = tolerancias[op]; break;
	case O_L_BORRA: P.borra_transparente = op == 1; break;
	case O_L_DENSIDAD: if (op < 3) P.densidad = op; break;
	case O_L_SELECCION:
		P.sel_transparente = op == 1;
		if (P.sel.flota)
			pin_repinta_todo();
		break;
	case O_L_LETRA: {
		const char *f[32];
		int n = letras_familias(f, 31);

		if (op < n)
			copia(P.letra, f[op], sizeof P.letra);
		herr_texto_rehaz();
		break;
	}
	case O_L_TAMANO: if (op < 17) P.tam_letra = tamanos[op]; herr_texto_rehaz(); break;
	case O_L_FONDO_TEXTO: P.fondo_texto = op == 1; herr_texto_rehaz(); break;
	case O_L_ZOOM:
		if (op < 12)
			zoom_centro(zooms_l[op]);
		else
			zoom_centro(vista_zoom_ajustado());
		break;
	}
	pon_cursor();
	s_barra = s_caja = 1;
	P.sucio = 1;
}

/* ===================================================================== */
/*  El teclado, el ratón y la rueda                                      */
/* ===================================================================== */
static void tecla(const so_suceso *s)
{
	u32 k = s->tecla;
	int ctrl = (s->mods & SO_CON_CONTROL) != 0;

	if (P.cuadro) {
		if (ui_cuadro_suceso(P.cuadro, P.v, s)) {
			if (P.al_cambiar)
				P.al_cambiar();
			P.sucio = 1;
		}
		return;
	}
	if (k >= SO_T_ALT && k < SO_T_ALT + 26) {
		int m = ui_menu_letra(MENUS, 'a' + (k - SO_T_ALT));

		if (m >= 0)
			abre_menu(m);
		return;
	}
	if (herr_tecla_caja(s))
		return;
	if (ctrl && k >= 1 && k <= 26) {
		switch (k + 'a' - 1) {
		case 'u': pin_orden(O_NUEVO); return;
		case 'a': pin_orden(O_ABRIR); return;
		case 'g': pin_orden(O_GUARDAR); return;
		case 'p': pin_orden(O_IMPRIMIR); return;
		case 'z': pin_orden(O_DESHACER); return;
		case 'y': pin_orden(O_REHACER); return;
		case 'x': pin_orden(O_CORTAR); return;
		case 'c': pin_orden(O_COPIAR); return;
		case 'v': pin_orden(O_PEGAR); return;
		case 'e': pin_orden(O_TODO); return;
		case 'w': pin_orden(O_TAMANO); return;
		case 'r': pin_orden(O_GIRA_DER); return;
		case 'k': pin_orden(O_RECORTAR); return;
		case 't': pin_orden(O_ATRIBUTOS); return;
		case 'n': pin_orden(O_INVERTIR); return;
		}
		return;
	}
	switch (k) {
	case SO_T_F1: pin_orden(O_AYUDA); return;
	case SO_T_F12: pin_orden(O_GUARDAR_COMO); return;
	case SO_T_SUPR: pin_orden(O_BORRAR_SEL); return;
	case SO_ESC: sel_nada(); return;
	case '+': pin_orden(O_ACERCAR); return;
	case '-': pin_orden(O_ALEJAR); return;
	case 'x': case 'X': pin_orden(O_CAMBIA_COLORES); return;
	case SO_T_ARRIBA: P.dy -= 40; break;
	case SO_T_ABAJO: P.dy += 40; break;
	case SO_T_IZQUIERDA: P.dx -= 40; break;
	case SO_T_DERECHA: P.dx += 40; break;
	case SO_T_RE_PAG: P.dy -= VI.al * 9 / 10; break;
	case SO_T_AV_PAG: P.dy += VI.al * 9 / 10; break;
	case SO_T_INICIO: P.dx = P.dy = 0; break;
	default: {
		int h;
		u32 m = minuscula(k);

		for (h = 0; h < H_N; h++)
			if (herr_tecla(h) == m) {
				pin_pon_herramienta(h);
				return;
			}
		return;
	}
	}
	vista_limita();
	pin_repinta_todo();
}

static void menu_contexto(int x, int y)
{
	so_menu(P.v, x, y, "Cortar|Copiar|Pegar|Borrar la selección|-|Seleccionar todo|Recortar a la selección|-|Voltear de izquierda a derecha|Voltear de arriba abajo|Girar 90° a la derecha|Invertir los colores",
		sel_hay() ? (P.papel.p ? 0 : 4) : (P.papel.p ? 1 | 2 | 8 | 64 : 1 | 2 | 4 | 8 | 64), ETIQ_CONTEXTO);
}

static void elige_contexto(int op)
{
	static const int o[] = { O_CORTAR, O_COPIAR, O_PEGAR, O_BORRAR_SEL, O_NADA, O_TODO, O_RECORTAR, O_NADA, O_VOLTEA_H, O_VOLTEA_V, O_GIRA_DER, O_INVERTIR };

	if (op >= 0 && op < (int)(sizeof o / sizeof o[0]) && o[op] != O_NADA)
		pin_orden(o[op]);
}

/* (PINTURA32) el ratón sobre un asa del lienzo (o fuera): se resalta, sale
 * la flecha de cambiar el tamaño y la barra de estado dice qué hacer */
static void mira_asa(int x, int y)
{
	int h = x < 0 || P.sel.flota || P.tx.activa ? 0 : vista_asa_en(x, y);

	if (h == asa_encima && (!h || (x == asa_mx && y == asa_my)))
		return;
	if (h && !asa_encima)
		pin_aviso("Arrastre para cambiar el tamaño del lienzo (ahora, %d x %d píxeles)", P.img.an, P.img.al);
	else if (!h && asa_encima)
		pin_aviso("%s", "");
	asa_encima = h;
	asa_mx = x;
	asa_my = y;
	pin_repinta_todo();
}

static void sale_de_la_vista(void)
{
	mira_asa(-1, -1);			/* (PINTURA32) */
	if (raton_x >= 0) {
		raton_x = raton_y = -1;
		pon_cursor();
		s_reglas = s_estado = 1;
		P.sucio = 1;
	}
}

/* (PUNTEROS) la forma del puntero (la pone el núcleo, la llamada 177): en
 * un asa, la de cambiar el tamaño; en el lienzo, la cruz para dibujar (la
 * «I» con el texto); en lo demás, la flecha */
static void puntero_de_pintura(const so_suceso *s)
{
	int f = SO_P_FLECHA;

	if (asa_encima)
		f = asa_encima == 1 ? SO_P_TAMANO_H : asa_encima == 2 ? SO_P_TAMANO_V : SO_P_TAMANO_D1;
	else if (!P.cuadro && s->x >= VI.x && s->y >= VI.y && s->x < VI.x + VI.an && s->y < VI.y + VI.al)
		f = P.herr == H_TEXTO ? SO_P_TEXTO : P.herr == H_LUPA ? SO_P_FLECHA : SO_P_CRUZ;
	so_puntero(P.v, f);
}

static void raton(const so_suceso *s)
{
	int flecha;

	if (P.cuadro) {
		if (P.al_raton && P.al_raton(s)) {
			P.sucio = 1;
			return;
		}
		if (ui_cuadro_suceso(P.cuadro, P.v, s)) {
			if (P.al_cambiar)
				P.al_cambiar();
			P.sucio = 1;
		}
		return;
	}
	/* lo que se arrastra sigue aunque se salga */
	if (arrastra_desp) {
		if (s->clase == SO_MUEVE && (s->botones & SO_BOTON_IZQ)) {
			if (arrastra_desp == 1)
				P.dy = ui_desp_arrastra(&desp_v, arr_pos0, arr_de, s->y);
			else
				P.dx = ui_desp_arrastra(&desp_h, arr_pos0, arr_de, s->x);
			vista_limita();
			pin_repinta_todo();
		} else if (s->clase == SO_SUBE)
			arrastra_desp = 0;
		return;
	}
	if (herr_arrastrando()) {
		if (s->x >= VI.x && s->y >= VI.y && s->x < VI.x + VI.an && s->y < VI.y + VI.al) {
			raton_x = s->x;
			raton_y = s->y;
			pon_cursor();
		}
		s_reglas = s_estado = 1;
		P.sucio = 1;
		if (s->clase == SO_MUEVE)
			herr_mueve(s);
		else if (s->clase == SO_SUBE)
			herr_sube(s);
		s_barra = s->clase == SO_SUBE;
		return;
	}
	/* la barra de menús */
	if (s->y < ui_alto_menus()) {
		int m = ui_menu_en(MENUS, 0, s->x, NULL);

		sale_de_la_vista();
		pon_globo(NULL, 0, 0);
		if (s->clase == SO_BAJA && m >= 0)
			abre_menu(m);
		else if (m != encima_menu) {
			encima_menu = m;
			s_menus = 1;
			P.sucio = 1;
		}
		return;
	}
	if (encima_menu >= 0) {
		encima_menu = -1;
		s_menus = 1;
		P.sucio = 1;
	}
	/* la barra de opciones */
	if (P.ver_opciones && s->y >= y_barra && s->y < y_barra + ui_alto_barra()) {
		int i = ui_boton_en(barra, n_barra, s->x, s->y, &flecha);

		sale_de_la_vista();
		if (s->clase == SO_MUEVE) {
			if (i != encima_barra) {
				encima_barra = i;
				s_barra = 1;
				P.sucio = 1;
				if (i >= 0 && barra[i].ayuda) {
					pin_aviso("%s", barra[i].ayuda);
					pon_globo(barra[i].ayuda, s->x + 8, y_barra + ui_alto_barra() + 4);
				} else
					pon_globo(NULL, 0, 0);
			}
		} else if (s->clase == SO_BAJA && i >= 0 && !barra[i].apagado) {
			pulsado_barra = i;
			s_barra = 1;
			P.sucio = 1;
			pon_globo(NULL, 0, 0);
			if (barra[i].tipo == UB_LISTA)
				abre_lista(barra[i].id, barra[i].x, barra[i].y + barra[i].al);
		} else if (s->clase == SO_SUBE) {
			int p = pulsado_barra;

			pulsado_barra = -1;
			s_barra = 1;
			P.sucio = 1;
			if (p >= 0 && p == i && barra[i].tipo != UB_LISTA)
				pin_orden(barra[i].id);
		}
		return;
	}
	if (encima_barra >= 0 || pulsado_barra >= 0) {
		encima_barra = pulsado_barra = -1;
		s_barra = 1;
		P.sucio = 1;
	}
	/* la caja de herramientas */
	if (P.ver_caja && s->x < CAJA && s->y >= y_cuerpo && s->y < y_pal) {
		int h = herr_en(s->x, s->y);

		sale_de_la_vista();
		if (s->clase == SO_MUEVE && h != encima_herr) {
			encima_herr = h;
			s_caja = 1;
			P.sucio = 1;
			if (h >= 0) {
				int bx, by;

				herr_en_caja(h, &bx, &by);
				pin_aviso("%s", herr_ayuda(h));
				{
					char g[80];

					fmt(g, sizeof g, "%s (%c)", herr_nombre(h), (char)mayuscula(herr_tecla(h)));
					pon_globo(g, bx + BOTON + 6, by + 4);
				}
			} else
				pon_globo(NULL, 0, 0);
		} else if (s->clase == SO_BAJA && h >= 0) {
			pulsado_herr = h;
			pon_globo(NULL, 0, 0);
			pin_pon_herramienta(h);
			pulsado_herr = -1;
		}
		return;
	}
	if (encima_herr >= 0) {
		encima_herr = -1;
		s_caja = 1;
		P.sucio = 1;
	}
	/* la paleta */
	if (P.ver_paleta && s->y >= y_pal && s->y < y_pal + PAL_AL) {
		int i = color_en_pal(s->x, s->y), ec = en_cambia(s->x, s->y);

		sale_de_la_vista();
		if (s->clase == SO_MUEVE) {
			if (i != encima_pal || ec != encima_cambia) {
				encima_pal = i;
				encima_cambia = ec;
				s_paleta = 1;
				P.sucio = 1;
				if (i >= 0) {
					char d[120], g[200];

					color_describe(paleta[i], d, sizeof d);
					fmt(g, sizeof g, "%s", d);
					pin_aviso("%s: un clic, color 1; con el botón derecho, color 2; dos clics, cambiarlo", d);
					pon_globo(g, s->x + 6, y_pal - lz_ui_alto() - 10);
				} else if (ec) {
					pin_aviso("Intercambiar el color 1 y el 2 (X)");
					pon_globo("Intercambiar el color 1 y el 2 (X)", s->x + 6, y_pal - lz_ui_alto() - 10);
				} else
					pon_globo(NULL, 0, 0);
			}
		} else if (s->clase == SO_BAJA) {
			pon_globo(NULL, 0, 0);
			if (i >= 0) {
				pin_elige_color((s->botones & SO_BOTON_DER) ? 2 : 1, paleta[i]);
				ultimo_paleta = i;
				{
					char d[120];

					color_describe(paleta[i], d, sizeof d);
					pin_aviso("Color %d: %s", (s->botones & SO_BOTON_DER) ? 2 : 1, d);
				}
				if (s->veces >= 2) {
					paleta_editada = i;
					dlg_colores((s->botones & SO_BOTON_DER) ? 2 : 1);
				}
			} else if (ec)
				pin_orden(O_CAMBIA_COLORES);
			else if (en_editar_colores(s->x, s->y))
				pin_orden(O_EDITAR_COLORES);
		}
		return;
	}
	if (encima_pal >= 0 || encima_cambia) {
		encima_pal = -1;
		encima_cambia = 0;
		s_paleta = 1;
		P.sucio = 1;
	}
	pon_globo(NULL, 0, 0);
	/* las barras de desplazamiento */
	if (s->clase == SO_BAJA) {
		int z = ui_desp_en(&desp_v, s->x, s->y), h = ui_desp_en(&desp_h, s->x, s->y);

		if (z || h) {
			if (z == 5 || h == 5) {
				arrastra_desp = z == 5 ? 1 : 2;
				arr_de = z == 5 ? s->y : s->x;
				arr_pos0 = z == 5 ? P.dy : P.dx;
			} else if (z)
				P.dy += z == 1 ? -40 : z == 2 ? 40 : z == 3 ? -VI.al : VI.al;
			else
				P.dx += h == 1 ? -40 : h == 2 ? 40 : h == 3 ? -VI.an : VI.an;
			vista_limita();
			pin_repinta_todo();
			return;
		}
	}
	/* el lienzo */
	if (s->x >= VI.x && s->y >= VI.y && s->x < VI.x + VI.an && s->y < VI.y + VI.al) {
		raton_x = s->x;
		raton_y = s->y;
		pon_cursor();
		s_reglas = s_estado = 1;
		P.sucio = 1;
		if (s->clase == SO_MUEVE)
			mira_asa(s->x, s->y);	/* (PINTURA32) */
		if (s->clase == SO_BAJA) {
			int ix, iy;

			vista_a_lamina(s->x, s->y, &ix, &iy);
			if ((s->botones & SO_BOTON_DER) && (P.herr == H_SELECCION || P.herr == H_LIBRE)) {
				menu_contexto(s->x, s->y);
				return;
			}
			herr_baja(s);
			s_barra = 1;
		} else if (s->clase == SO_MUEVE)
			herr_mueve(s);
		else if (s->clase == SO_SUBE)
			herr_sube(s);
	} else
		sale_de_la_vista();
}

static void rueda(const so_suceso *s)
{
	if (P.cuadro) {
		if (ui_cuadro_suceso(P.cuadro, P.v, s))
			P.sucio = 1;
		return;
	}
	if (s->mods & SO_CON_CONTROL) {
		int z = P.zoom, i, n = s->pasos > 0 ? s->pasos : -s->pasos;

		for (i = 0; i < n; i++)
			z = vista_zoom_siguiente(z, s->pasos > 0);
		vista_zoom_en(z, s->x, s->y);
		pin_aviso("Zoom: %d %%", P.zoom / 10);
	} else if (s->mods & SO_CON_MAYUS)
		P.dx -= s->pasos * 60;
	else
		P.dy -= s->pasos * 60;
	vista_limita();
	pon_cursor();
	pin_repinta_todo();
}

static void menu_elegido(const so_suceso *s)
{
	u32 et = s->etiqueta;

	if (P.cuadro && et == UI_ETIQUETA_MENU) {
		ui_cuadro_suceso(P.cuadro, P.v, s);
		if (P.al_cambiar)
			P.al_cambiar();
		P.sucio = 1;
		return;
	}
	if (et >= ETIQ_MENU && et < ETIQ_MENU + N_MENUS) {
		int m = (int)(et - ETIQ_MENU);

		menu_abierto = -1;
		s_menus = 1;
		P.sucio = 1;
		if (s->opcion == -2) {
			abre_menu((m + N_MENUS - 1) % N_MENUS);
			return;
		}
		if (s->opcion == -3) {
			abre_menu((m + 1) % N_MENUS);
			return;
		}
		if (s->opcion >= 0) {
			int i;

			for (i = 0; menus[m][i].texto; i++)
				if (i == s->opcion) {
					if (menus[m][i].orden != O_NADA && activa(menus[m][i].orden))
						pin_orden(menus[m][i].orden);
					break;
				}
		}
		return;
	}
	if (et == ETIQ_CONTEXTO) {
		elige_contexto(s->opcion);
		return;
	}
	if (et >= ETIQ_LISTA && et < ETIQ_LISTA + O_N) {
		pulsado_barra = -1;
		elige_de_lista((int)(et - ETIQ_LISTA), s->opcion);
	}
}

/* ===================================================================== */
/*  El principio                                                         */
/* ===================================================================== */
static void valores_de_siempre(void)
{
	int h;

	memset(&P, 0, sizeof P);
	P.c1 = 0xFF000000u;
	P.c2 = 0xFFFFFFFFu;
	P.herr = P.herr_antes = H_PINCEL;
	P.grosor_pincel = 5;
	P.grosor_figura = 2;
	P.grosor_borrador = 12;
	P.radio_aero = 12;
	P.densidad = 1;
	P.suave = 1;
	P.tolerancia = 10;
	P.tam_letra = 24;
	copia(P.letra, FAM_SANS, sizeof P.letra);
	P.zoom = 1000;
	P.calidad = 90;
	P.formato = 1;	/* FI_PNG */
	P.ver_caja = P.ver_opciones = P.ver_paleta = P.ver_estado = P.ver_reglas = P.ver_rejilla = 1;
	paleta_de_fabrica();
	for (h = 0; h < H_N; h++) {
		fmt(t_herr[h], sizeof t_herr[h], "%s (%c)", herr_nombre(h), (char)mayuscula(herr_tecla(h)));
		m_herr[h].texto = t_herr[h];
		m_herr[h].orden = O_HERR0 + h;
	}
	m_herr[H_N].texto = NULL;
}

static int prepara_ventana(void)
{
	int an, al;

	so_pantalla(&an, &al);
	P.v = so_ventana("Pintura", an * 9 / 10, al * 9 / 10);
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
	if (!lz_nuevo(&P.l, an, al))
		lz_nuevo(&P.l, 320, 200);
	pin_repinta_todo();
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

static void limpia(void)
{
	his_suelta();
	lam_suelta(&P.img);
	lam_suelta(&P.papel);
	lam_suelta(&P.tx.dibujo);
	lam_suelta(&P.sel.flotante);
	so_suelta(P.sel.libre);
	lz_suelta(&P.l);
}

static void bucle(void)
{
	so_suceso s;

	while (!P.sale) {
		int hay = so_lee_suceso(P.v, &s, !P.sucio && !herr_arrastrando());

		if (!hay) {
			if (P.sucio)
				pin_pinta();
			else if (herr_quieto())
				so_duerme(so_hz() / 30 ? so_hz() / 30 : 1);
			else if (herr_arrastrando()) {
				/* (sin nada: a esperar el suceso siguiente) */
				hay = so_lee_suceso(P.v, &s, 1);
				if (!hay)
					continue;
			} else
				continue;
			if (!hay)
				continue;
		}
		switch (s.clase) {
		case SO_TECLA: tecla(&s); break;
		case SO_BAJA:
		case SO_SUBE:
		case SO_MUEVE: raton(&s); puntero_de_pintura(&s); break;	/* (PUNTEROS) */
		case SO_RUEDA: rueda(&s); break;
		case SO_MENU: menu_elegido(&s); break;
		case SO_TAMANO: cambia_tamano(s.x, s.y); break;
		case SO_FOCO: pin_repinta_todo(); break;
		case SO_SALE:
			sale_de_la_vista();
			if (encima_barra >= 0 || encima_herr >= 0 || encima_pal >= 0 || encima_menu >= 0) {
				encima_barra = encima_herr = encima_pal = encima_menu = -1;
				pon_globo(NULL, 0, 0);
				pin_repinta_todo();
			}
			break;
		case SO_CERRAR:
			if (P.cuadro) {
				P.cuadro->cerrado = -1;
				P.sucio = 1;
			} else
				pin_orden(O_SALIR);
			break;
		}
		if (P.cuadro && P.cuadro->cerrado != -9999) {
			void (*f)(int) = P.al_cerrar;
			int id = P.cuadro->cerrado;

			P.cuadro = NULL;
			P.al_cerrar = NULL;
			P.al_cambiar = NULL;
			P.al_raton = NULL;
			if (f)
				f(id);
			pin_repinta_todo();
		}
	}
}

int pintura_ventana(const char *archivo)
{
	valores_de_siempre();
	lee_letras_usuario();
	{
		char u = so_unidad_sistema();

		fmt(P.carpeta, sizeof P.carpeta, "%c:\\DOCUMENT", u);
		if (so_existe(P.carpeta) != 2)
			fmt(P.carpeta, sizeof P.carpeta, "%c:", u);
	}
	if (!prepara_ventana()) {
		dice("pintura: no se puede abrir la ventana");
		return 1;
	}
	if (!lam_nueva(&P.img, 800, 600, 0xFFFFFFFFu)) {
		dice("pintura: no hay memoria");
		return 1;
	}
	his_lamina(&P.img, 1);
	if (archivo && archivo[0])
		pin_abre(archivo);
	pin_titulo();
	if (!P.aviso[0])
		pin_aviso("%s", herr_ayuda(P.herr));
	pin_repinta_todo();
	dice("pintura: lista (%dx%d)", P.l.an, P.l.al);
	bucle();
	dice("pintura: adiós");
	limpia();
	so_cierra_ventana(P.v);
	return 0;
}

/* ---- medir (PINTURA --mide AN AL PASOS): una imagen de an x al, y lo
 * que tarda cada paso del pincel (pintar en la lámina y en la pantalla) ---- */
int pintura_mide(int an, int al, int pasos)
{
	u32 hz = so_hz(), t0, t1, peor = 0, ta;
	int k;
	so_suceso s;

	valores_de_siempre();
	if (an < 16)
		an = 1600;
	if (al < 16)
		al = 1200;
	if (pasos < 1)
		pasos = 200;
	if (!prepara_ventana()) {
		dice("pintura: mide: sin ventana");
		return 1;
	}
	if (!lam_nueva(&P.img, an, al, 0xFFFFFFFFu)) {
		dice("pintura: mide: no hay memoria para %d x %d", an, al);
		return 1;
	}
	his_lamina(&P.img, 1);
	/* (la ventana, ya a su medida: maximizada) */
	for (k = 0; k < 20; k++) {
		if (so_lee_suceso(P.v, &s, 0)) {
			if (s.clase == SO_TAMANO) {
				cambia_tamano(s.x, s.y);
				break;
			}
			continue;
		}
		so_duerme(so_hz() / 20 ? so_hz() / 20 : 1);
	}
	coloca();
	t0 = so_tics();
	pin_repinta_todo();
	pin_pinta();
	t1 = so_tics();
	dice("pintura: mide: ventana %d x %d, lienzo %d x %d; pintarla entera, %u ms", P.l.an, P.l.al, an, al, (t1 - t0) * 1000 / hz);
	/* el pincel (de 5 y suavizado), en zigzag por lo que se ve */
	memset(&s, 0, sizeof s);
	s.clase = SO_BAJA;
	s.botones = SO_BOTON_IZQ;
	s.x = VI.x + 20;
	s.y = VI.y + 20;
	raton(&s);
	pin_pinta();
	s.clase = SO_MUEVE;
	t0 = ta = so_tics();
	for (k = 0; k < pasos; k++) {
		u32 tb;

		s.x = VI.x + 20 + (k * 7) % (VI.an - 40);
		s.y = VI.y + 20 + ((k / ((VI.an - 40) / 7 + 1)) * 30 + (k % 2) * 6) % (VI.al - 40);
		raton(&s);
		pin_pinta();
		tb = so_tics();
		if (tb - ta > peor)
			peor = tb - ta;
		ta = tb;
	}
	t1 = so_tics();
	s.clase = SO_SUBE;
	s.botones = 0;
	raton(&s);
	pin_pinta();
	dice("pintura: mide: pincel: %d pasos en %u ms: %u ms por paso (el peor, %u ms)", pasos, (t1 - t0) * 1000 / hz,
	     (t1 - t0) * 1000 / hz / (u32)pasos, peor * 1000 / hz);
	/* deshacer la pincelada y el relleno de todo */
	t0 = so_tics();
	pin_orden(O_DESHACER);
	pin_pinta();
	t1 = so_tics();
	dice("pintura: mide: deshacer la pincelada, %u ms", (t1 - t0) * 1000 / hz);
	t0 = so_tics();
	his_empieza();
	rellena(&P.img, 1, 1, 0xFF2050E0u, 0, NULL);
	his_acaba("Relleno");
	pin_repinta_todo();
	pin_pinta();
	t1 = so_tics();
	dice("pintura: mide: rellenar el lienzo entero, %u ms", (t1 - t0) * 1000 / hz);
	t0 = so_tics();
	pin_orden(O_ACERCAR);
	pin_pinta();
	pin_orden(O_ALEJAR);
	pin_pinta();
	t1 = so_tics();
	dice("pintura: mide: acercar y alejar, %u ms", (t1 - t0) * 1000 / hz / 2);
	dice("pintura: mide: memoria: %u KB (la historia: %u KB en %d pasos)", so_memoria_usada() / 1024, his_memoria() / 1024, his_pasos());
	limpia();
	so_cierra_ventana(P.v);
	return 0;
}
