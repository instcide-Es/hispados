/* libro.h - Cuentas 2: el libro (las hojas, sus celdas dispersas, los
 * estilos sin repetir, los formatos de número, los nombres definidos, las
 * celdas combinadas, los comentarios, el formato condicional, los gráficos,
 * el autofiltro y lo de imprimir) y lo que se hace con él. Es C portable:
 * lo mismo en Hispa-Pentium que en Hispa-64.
 *
 * Una hoja tiene CU_FILAS x CU_COLS celdas (65.536 x 256, como Excel 5),
 * pero solo se guardan las que tienen algo: en un montón (cel) con una
 * tabla de dispersión por la clave fila << 8 | columna. Las fórmulas, sus
 * dependencias y el recálculo, en formula.h y calculo.c.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef CU_LIBRO_H
#define CU_LIBRO_H
#include "so.h"
#include "texto.h"

#define CU_FILAS 65536u
#define CU_COLS 256u
#define CU_MAX_HOJAS 255
#define CU_ANCHO 88		/* el ancho de una columna, en puntos a 100% (el de Cuentas de 386) */
#define CU_ALTO 20		/* el alto de una fila */
#define CU_AUTO 0xFF000000u	/* (color) automático */
#define CU_SIN_FILA 0xFFFFFFFFu

/* ---- los valores ---- */
enum { V_VACIO, V_NUM, V_TXT, V_LOG, V_ERR, V_REF };
enum { ER_NADA, ER_NULO, ER_DIV0, ER_VALOR, ER_REF, ER_NOMBRE, ER_NUM, ER_NA, ER_CIRC, ER_N };
typedef struct {
	u8 t;			/* V_VACIO, V_NUM, V_TXT, V_LOG, V_ERR */
	u8 e;			/* (V_ERR) ER_* */
	double n;		/* (V_NUM) el número; (V_LOG) 0 o 1 */
	char *s;		/* (V_TXT) UTF-8, de so_pide */
} valor;
/* el texto de un error en español («#¡DIV/0!») y en inglés («#DIV/0!») */
const char *cu_error_texto(int e, int ingles);
int cu_error_de(const char *s, u32 *largo);	/* el ER_* que empieza en s (0 si no) */

/* ---- un rango (filas y columnas desde 0, f0 <= f1, c0 <= c1) ---- */
typedef struct {
	u32 f0, f1;
	u16 c0, c1;
} rango;

/* ---- los estilos de las celdas ---- */
enum { AH_GENERAL, AH_IZQ, AH_CENTRO, AH_DER, AH_RELLENAR, AH_JUSTIFICAR, AH_CENTRO_SEL };
enum { AV_ABAJO, AV_CENTRO, AV_ARRIBA };
/* las tramas del relleno (además del color): se distinguen sin color */
enum { TR_NADA, TR_SOLIDO, TR_GRIS75, TR_GRIS50, TR_GRIS25, TR_GRIS12, TR_HORIZ, TR_VERT, TR_DIAG_ABAJO,
       TR_DIAG_ARRIBA, TR_CUADRICULA, TR_ENREJADO, TR_N };
enum { BR_NADA, BR_FINO, BR_MEDIO, BR_GRUESO, BR_DOBLE, BR_PUNTOS, BR_GUIONES, BR_N };
enum { BO_IZQ, BO_ARRIBA, BO_DER, BO_ABAJO };
typedef struct {
	char familia[32];	/* «DejaVu Sans»; las que no están se ven con la más parecida */
	u16 tam;		/* en medios puntos (20 = 10 pt) */
	u8 negrita, cursiva, subrayado, tachado;
	u32 color;		/* la letra: 0xRRGGBB o CU_AUTO */
	u8 alin_h, alin_v, ajustar, sangria;	/* AH_*, AV_*, partir en líneas, sangría */
	u16 numfmt;		/* el formato del número (libro->fmt; 0 General) */
	u8 trama, girado;	/* TR_*; (girado: de momento solo se guarda) */
	u32 fondo;		/* el color del relleno (con TR_SOLIDO, o el fondo de la trama) */
	u32 color_trama;	/* el de las rayas o los puntos de la trama */
	u8 borde[4];		/* BR_* de cada lado (BO_*) */
	u32 color_borde[4];
} estilo;
#define CU_FAMILIA "DejaVu Sans"
#define CU_TAM 20

/* ---- los comentarios ---- */
typedef struct {
	u32 f;
	u16 c;
	char *texto;		/* UTF-8 */
	char autor[64];
} comentario;

/* ---- el formato condicional (sencillo): si el valor cumple, el estilo
 * cambia y sale un símbolo a la izquierda (no solo un color) ---- */
enum { FC_MAYOR, FC_MENOR, FC_ENTRE, FC_IGUAL, FC_DISTINTO, FC_MAYOR_IGUAL, FC_MENOR_IGUAL, FC_TEXTO, FC_N };
enum { SIM_NADA, SIM_ARRIBA, SIM_ABAJO, SIM_BIEN, SIM_MAL, SIM_AVISO, SIM_ESTRELLA, SIM_PUNTO, SIM_IGUAL, SIM_N };
typedef struct {
	rango r;
	u8 cond;		/* FC_* */
	u8 simbolo;		/* SIM_* */
	u8 negrita, cursiva;
	double v1, v2;		/* los umbrales (FC_ENTRE: los dos) */
	char texto[64];		/* (FC_TEXTO) lo que contiene */
	u32 color;		/* la letra (CU_AUTO: la de la celda) */
	u32 fondo;		/* el relleno (CU_AUTO: el de la celda) */
	u8 trama;		/* (con fondo) la trama: TR_SOLIDO de ordinario */
} condicional;

/* ---- los gráficos (grafico.c los pinta) ---- */
enum { GR_COLUMNAS, GR_BARRAS, GR_LINEAS, GR_SECTORES, GR_DISPERSION, GR_N };
enum { LY_NADA, LY_DERECHA, LY_ABAJO };
typedef struct {
	u8 tipo;		/* GR_* */
	u8 en_filas;		/* las series van por filas (si no, por columnas) */
	u8 rotulos;		/* la primera fila o columna del rango son los rótulos (y la otra, los nombres) */
	u8 leyenda;		/* LY_* */
	u8 apiladas;		/* (columnas y barras) una encima de otra */
	u8 rejilla;		/* las rayas guía */
	u16 hoja;		/* el id de la hoja de los datos */
	rango datos;
	char titulo[128], eje_x[64], eje_y[64];
	/* dónde está: la celda de su esquina, lo que se aparta de ella y lo que mide (en puntos a 100%) */
	u32 f;
	u16 c;
	i32 dx, dy, an, al;
} grafico;

/* ---- el autofiltro: el rango (con la fila de títulos arriba) y lo que pide cada columna ---- */
enum { FI_TODO, FI_IGUAL, FI_VACIAS, FI_NO_VACIAS, FI_MAYOR, FI_MENOR, FI_DIEZ_MAS, FI_DIEZ_MENOS, FI_CONTIENE };
typedef struct {
	u16 c;
	u8 tipo;		/* FI_* */
	double v;
	char texto[96];		/* (FI_IGUAL y FI_CONTIENE) */
} filtro_col;

/* ---- la página (al imprimir y exportar a PDF) ---- */
typedef struct {
	u8 horizontal;		/* apaisada */
	u8 papel;		/* 0 A4, 1 carta, 2 A3, 3 A5, 4 legal */
	u8 rejilla;		/* imprimir las líneas de la cuadrícula */
	u8 titulos;		/* imprimir los rótulos de filas y columnas */
	u8 centrar_h, centrar_v;
	u16 escala;		/* en tantos por ciento (100) */
	u16 encajar_an, encajar_al;	/* si no son 0: en tantas páginas de ancho y de alto */
	double marg[4];		/* izquierda, arriba, derecha, abajo: en cm */
	char encabezado[128], pie[128];	/* &P la página, &N cuántas, &A la hoja, &F el archivo, &D la fecha */
} pagina;

/* ---- una hoja ---- */
typedef struct celda celda;
struct formula;
typedef struct hoja {
	char nombre[96];	/* (UTF-8; hasta 31 letras) */
	u16 id;			/* el que llevan las fórmulas y las claves (no cambia al moverla) */
	/* las celdas */
	celda *cel;
	u32 n_cel, cabe_cel;
	u32 *tabla;		/* la dispersión: índice + 1 en cel (0: libre) */
	u32 cabe_tabla;
	u32 lim_f, lim_c;	/* hasta dónde ha habido celdas (la fila y la columna de después): no se mira más allá */
	u32 *ix;		/* el índice por columnas: las celdas de cada columna, por filas (hj_columna) */
	u32 ix_ini[CU_COLS + 1];
	int ix_bien;
	/* anchos y altos */
	u16 ancho[CU_COLS];	/* en puntos a 100% */
	u16 *alto;		/* [CU_FILAS]: 0 el de siempre (o NULL: todas) */
	u8 *oculta_f;		/* [CU_FILAS / 8] filas ocultas (o NULL) */
	u8 oculta_c[CU_COLS];
	u32 ancho_def;		/* el ancho de siempre de esta hoja */
	u16 estilo_col[CU_COLS];	/* el estilo de las celdas nuevas de cada columna (0 el normal) */
	u16 *estilo_fila;	/* [CU_FILAS] el de cada fila (manda sobre el de la columna; o NULL) */
	/* lo demás */
	u32 inm_f, inm_c;	/* filas y columnas inmovilizadas (arriba y a la izquierda) */
	rango *comb;		/* las celdas combinadas */
	u32 n_comb;
	comentario *com;
	u32 n_com;
	condicional *cond;
	u32 n_cond;
	grafico *gr;
	u32 n_gr;
	int hay_filtro;
	rango filtro;
	filtro_col *fcol;
	u32 n_fcol;
	int hay_area;
	rango area;		/* el área de impresión */
	u32 *salto_f, *salto_c;	/* los saltos de página puestos a mano (antes de esa fila o columna) */
	u32 n_salto_f, n_salto_c;
	pagina pag;
	/* la vista */
	u32 cur_f, cur_c;	/* la celda activa */
	rango sel;		/* lo elegido */
	u32 vista_f, vista_c;	/* la primera fila y columna que se ven (de lo que se mueve) */
	u16 zoom;
	u8 sin_rejilla;		/* no se ven las líneas de división */
	u8 oculta;
	void *dep;		/* (calculo.c) las dependencias de rangos, por columnas */
} hoja;

/* ---- una celda ---- */
enum { CM_SUCIA = 1, CM_CALCULANDO = 2, CM_VOLATIL = 4, CM_EN_COLA = 8 };
struct celda {
	u32 clave;		/* fila << 8 | columna */
	u16 estilo;
	u8 marca;		/* CM_* (calculo.c) */
	u8 de_otro;		/* (un .HOJ de 386: lo que se tecleó, si no se puede rehacer) */
	valor v;		/* lo escrito o, si hay fórmula, lo que da */
	struct formula *f;	/* la fórmula (NULL: no lo es) */
	u32 grado;		/* (calculo.c) */
};
#define CU_CLAVE(f, c) ((u32)(f) << 8 | (u32)(c))
#define CU_FILA(k) ((k) >> 8)
#define CU_COL(k) ((k) & 255)
/* la clave de todo el libro: el id de la hoja delante */
#define CU_GCLAVE(h, f, c) ((u32)(h) << 24 | CU_CLAVE(f, c))

/* ---- los nombres definidos ---- */
typedef struct {
	char nombre[64];
	struct formula *f;	/* lo que es (una referencia, un rango o una cuenta) */
} nombre_def;

/* ---- cómo se escriben los números y las fórmulas ---- */
typedef struct {
	char dec;		/* «,» o «.» */
	char sep;		/* el de los argumentos: «;» o «,» */
	char mil;		/* el de los millares: «.» o «,» */
	u8 ingles;		/* las funciones con su nombre en inglés */
	u8 ods;			/* (OpenFormula: [.A1], de los .ODS) */
} cu_local;
extern const cu_local CU_ES;		/* , ; .  español */
extern const cu_local CU_ARCHIVO;	/* . , ,  inglés: el de los .XLSX (y lo de dentro de los .HOJ de Cuentas 2) */
extern const cu_local CU_ODS;		/* . ; ,  inglés con [.A1] */

/* ---- el libro ---- */
typedef struct libro {
	hoja **h;
	u32 n_h, activa;
	u16 sig_id;		/* el id de la hoja nueva siguiente */
	estilo *est;
	u32 n_est, cabe_est;
	u32 *est_tabla;		/* (dispersión de los estilos) */
	u32 cabe_est_tabla;
	char **fmt;		/* los códigos de formato (como en los .XLSX: «#,##0.00 €»); 0 «General» */
	u32 n_fmt, cabe_fmt;
	nombre_def *nom;
	u32 n_nom;
	cu_local loc;		/* lo que ve y escribe quien usa Cuentas (CU_ES de ordinario) */
	int fecha1904;		/* (de algunos .XLSX) */
	u32 version;		/* sube con cada cambio */
	int cambiado;
	char moneda[16];	/* «€» */
	char autor[64];
	void *calc;		/* (calculo.c) las dependencias y lo pendiente */
	void *deshacer;		/* (deshacer.c) los pasos para deshacer y rehacer */
	u32 n_formulas;
	u32 circular;		/* la última referencia circular vista (CU_GCLAVE + 1; 0 ninguna) */
} libro;

/* ---- el libro y sus hojas ---- */
libro *lib_nuevo(int hojas);		/* con tantas hojas vacías (Hoja1, Hoja2...) */
void lib_suelta(libro *l);
hoja *lib_hoja(libro *l, u32 i);	/* la i-ésima (o NULL) */
hoja *lib_hoja_id(libro *l, u16 id);	/* la del id (o NULL) */
int lib_hoja_indice(libro *l, u16 id);	/* su posición (-1) */
hoja *lib_hoja_nombre(libro *l, const char *nombre, u32 n);	/* sin mirar mayúsculas (o NULL) */
hoja *lib_hoja_nueva(libro *l, const char *nombre, int pos);	/* en pos (-1: al final); NULL si no cabe */
/* quitarla del libro (no se suelta: la devuelve, para deshacer) */
hoja *lib_hoja_saca(libro *l, u32 i);
void lib_hoja_mete(libro *l, hoja *h, u32 i);
void hoja_suelta(hoja *h);
hoja *hoja_nueva(const char *nombre, u16 id);
void lib_nombre_libre(libro *l, char *d, u32 cabe);	/* «HojaN» que no esté */

/* ---- las celdas de una hoja ---- */
celda *hj_celda(const hoja *h, u32 f, u32 c);
celda *hj_celda_k(const hoja *h, u32 clave);
celda *hj_celda_crea(hoja *h, u32 f, u32 c);	/* NULL si no hay memoria */
void hj_celda_quita(hoja *h, u32 f, u32 c);	/* (suelta lo que lleva) */
void celda_vacia(celda *x);			/* suelta lo que lleva (y queda vacía) */
/* lo que se usa: la última fila y columna con algo (*f = CU_SIN_FILA si nada) */
void hj_usado(const hoja *h, u32 *f, u32 *c);
/* las claves de las celdas, por orden (de so_pide; *n cuántas) */
u32 *hj_claves_ordenadas(const hoja *h, u32 *n);
/* las celdas de la columna c, por filas (índices en h->cel; *n cuántas); NULL si no hay memoria.
 * Vale hasta que se crea o se quita una celda */
const u32 *hj_columna(hoja *h, u32 c, u32 *n);
/* el estilo de (f, c): el de la celda o, si no la hay, el de su fila o su columna */
u16 hj_estilo(const hoja *h, u32 f, u32 c);
u16 hj_estilo_def(const hoja *h, u32 f, u32 c);	/* (el de su fila o su columna) */
/* sacar la celda de la hoja sin soltar lo que lleva (a *d); 0 si no estaba */
int hj_celda_saca(hoja *h, u32 f, u32 c, celda *d);
u32 hj_ancho(const hoja *h, u32 c);		/* (0 si oculta) */
u32 hj_alto(const hoja *h, u32 f);
void hj_pon_ancho(hoja *h, u32 c, u32 an);
void hj_pon_alto(hoja *h, u32 f, u32 al);	/* (0: el de siempre) */
int hj_fila_oculta(const hoja *h, u32 f);
void hj_oculta_fila(hoja *h, u32 f, int si);
/* la combinada que tiene a (f, c), o -1 */
int hj_combinada(const hoja *h, u32 f, u32 c);
void hj_combina(hoja *h, rango r);		/* (quita las que se pisan) */
void hj_descombina(hoja *h, rango r);		/* las que se pisan con r */
comentario *hj_comentario(const hoja *h, u32 f, u32 c);
void hj_pon_comentario(hoja *h, u32 f, u32 c, const char *texto, const char *autor);	/* NULL o «» lo quita */

/* ---- los estilos y los formatos ---- */
void estilo_base(estilo *e);
const estilo *lib_estilo(const libro *l, u32 i);
u16 lib_estilo_id(libro *l, const estilo *e);	/* (el mismo número para los iguales) */
u16 lib_fmt_id(libro *l, const char *codigo);	/* (lo mete si no está) */
const char *lib_fmt(const libro *l, u32 i);

/* ---- poner y leer lo de las celdas (todo pasa por aquí: el recálculo
 * se entera) ---- */
/* lo que teclea quien usa Cuentas (con l->loc): un número (1.234,5; 12%;
 * 3,50 €; 22/11/2026; 14:30), un texto (con «'» delante, siempre texto),
 * VERDADERO/FALSO o una fórmula («=...»). Si el número trae formato (fecha,
 * %, €) y la celda no tiene ninguno, se le pone. 0 si va bien; si no, en
 * err por qué (una fórmula mal escrita) */
int lib_pon_entrada(libro *l, hoja *h, u32 f, u32 c, const char *t, char *err, u32 cabe);
void lib_pon_valor(libro *l, hoja *h, u32 f, u32 c, const valor *v);	/* (se copia) */
void lib_pon_numero(libro *l, hoja *h, u32 f, u32 c, double n);
void lib_pon_texto(libro *l, hoja *h, u32 f, u32 c, const char *t);
/* una fórmula ya hecha (se queda con ella) */
void lib_pon_formula(libro *l, hoja *h, u32 f, u32 c, struct formula *fm);
void lib_pon_estilo(libro *l, hoja *h, u32 f, u32 c, u16 est);
void lib_borra(libro *l, hoja *h, u32 f, u32 c);	/* el contenido (no el formato) */
/* el texto para corregir la celda (la fórmula con «=», el número como se teclea) */
void lib_texto_edicion(libro *l, hoja *h, u32 f, u32 c, char *d, u32 cabe);
/* lo que se ve en la celda con su formato; en *color, el del formato ([Rojo]) o CU_AUTO */
void lib_texto_visible(libro *l, const celda *x, char *d, u32 cabe, u32 *color);
/* el valor de una celda (vacío si no hay); no se copia */
const valor *lib_valor(const hoja *h, u32 f, u32 c);
void lib_cambio(libro *l);	/* (sube la versión) */

/* ---- los nombres definidos ---- */
int lib_nombre(const libro *l, const char *n, u32 largo);	/* su índice o -1 */
int lib_pon_nombre(libro *l, const char *n, struct formula *f);	/* (se queda con f; NULL lo quita) */
int nombre_valido(const char *n);

/* ---- las referencias como texto ---- */
void cu_col_texto(u32 c, char *d);		/* 0 -> «A», 26 -> «AA» */
void cu_ref_texto(u32 f, u32 c, char *d);	/* «B7» */
void cu_rango_texto(rango r, char *d);	/* «A1:C5» (o «B7») */
/* lee «B7», «$B$7», «B7:C9» (y las de columna y fila enteras) */
int cu_lee_ref(const char *s, u32 *f, u32 *c, const char **fin);
int cu_lee_rango(const char *s, rango *r, const char **fin);
rango cu_rango(u32 f0, u32 c0, u32 f1, u32 c1);	/* (los pone en orden) */
int rg_dentro(rango r, u32 f, u32 c);
int rg_pisa(rango a, rango b);

/* ---- los valores ---- */
void val_suelta(valor *v);
void val_copia(valor *d, const valor *o);
void val_num(valor *v, double n);
void val_txt(valor *v, const char *s);	/* (se copia) */
void val_err(valor *v, int e);

/* ---- los nombres de las cosas ---- */
extern const char *const NOMBRES_TRAMA[TR_N];
extern const char *const NOMBRES_SIMBOLO[SIM_N];
extern const char *const SIMBOLO_LETRA[SIM_N];	/* ▲ ▼ ✓ ✗ ⚠ ★ ● = */

#endif
