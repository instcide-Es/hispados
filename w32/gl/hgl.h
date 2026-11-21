/* hgl.h - lo de dentro de HispaGL, la OpenGL 1.1 por programa de Hispa-486 e
 * Hispa-Pentium (OPENGL32.DLL; docs/486/OPENGL.md). El estado de un contexto,
 * los vértices ya transformados y lo que se dicen las partes:
 *   estado.c   glEnable, glGet*, los errores, glGetString, glPushAttrib
 *   matriz.c   las matrices
 *   vertice.c  glBegin/glEnd, los vértices, las matrices de vértices
 *              (arrays), la luz, la niebla por vértice, el recorte y el
 *              montaje de las primitivas
 *   textura.c  los objetos de textura, la subida (a 16 bits), los mipmaps
 *   raster.c   los triángulos, las líneas y los puntos: el rasterizado en
 *              coma fija, la prueba de profundidad, la mezcla...
 *   pixeles.c  glClear, glReadPixels, glDrawPixels, glBitmap, glRasterPos
 *   listas.c   las listas de visualización
 *   mates.c    seno, coseno, raíz, potencia (con el x87)
 *   wgl.c      wgl*, el puente con GDI32 y la presentación (BitBlt de GDI o
 *              DirectDraw a pantalla completa); solo en la DLL
 * El búfer de color es de 16 bits (5-6-5) y el de profundidad de 16 bits; las
 * texturas se guardan a 16 bits (5-6-5, 1-5-5-5 o 4-4-4-4). Sala limpia: hecho
 * a partir de la especificación pública de OpenGL 1.1.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HGL_H
#define HGL_H
#include "GL/gl.h"

typedef unsigned int u32;
typedef int i32;
typedef unsigned short u16;
typedef short i16;
typedef unsigned char u8;
typedef signed char i8;
typedef unsigned long long u64;
#ifndef NULL
#define NULL ((void *)0)
#endif

/* ---- lo que pone cada plataforma (la DLL: wgl.c; el anfitrión: las pruebas) ---- */
void *hgl_pedir(u32 n);              /* memoria a cero */
void hgl_soltar(void *p);
void hgl_traza(const char *s, u32 v);
/* utilidades (mates.c) */
void hgl_copia(void *d, const void *s, u32 n);
void hgl_pon(void *d, int v, u32 n);
float hgl_raiz(float x);
float hgl_seno(float x);
float hgl_coseno(float x);
float hgl_pot(float b, float e);     /* b^e, b >= 0 */
float hgl_expo(float x);             /* e^x */
static inline int hgl_red(float f)   /* al entero más cercano (FISTP: sin cambiar el modo del x87) */
{
#if defined(__i386__)
	int i;
	__asm__("fistpl %0" : "=m"(i) : "t"(f) : "st");
	return i;
#else
	return (int)__builtin_lrintf(f);
#endif
}
static inline int hgl_suelo(float f) { int i = hgl_red(f); return (float)i > f ? i - 1 : i; }
static inline int hgl_techo(float f) { int i = hgl_red(f); return (float)i < f ? i + 1 : i; }
static inline float hgl_abs(float f) { return f < 0 ? -f : f; }
static inline float hgl_lim01(float f) { return f < 0 ? 0 : f > 1 ? 1 : f; }
static inline u32 hgl_fu(float f) { union { float f; u32 u; } x; x.f = f; return x.u; }
static inline float hgl_uf(u32 u) { union { float f; u32 u; } x; x.u = u; return x.f; }

/* ---- las matrices (por columnas, como las da OpenGL) ---- */
typedef struct { float m[16]; } Mat;
void mat_identidad(Mat *m);
void mat_mul(Mat *r, const Mat *a, const Mat *b);   /* r = a·b (r puede ser a o b) */
int mat_inversa(Mat *r, const Mat *a);
static inline void mat_xf4(const Mat *m, const float *v, float *r)
{
	const float *a = m->m;
	float x = v[0], y = v[1], z = v[2], w = v[3];
	r[0] = a[0] * x + a[4] * y + a[8] * z + a[12] * w;
	r[1] = a[1] * x + a[5] * y + a[9] * z + a[13] * w;
	r[2] = a[2] * x + a[6] * y + a[10] * z + a[14] * w;
	r[3] = a[3] * x + a[7] * y + a[11] * z + a[15] * w;
}

#define PILA_MV 32
#define PILA_PR 8
#define PILA_TX 8
#define UNIDADES 2
#define LUCES 8
#define PLANOS 6
#define NIVELES 12
#define TAM_MAX 1024                 /* lado máximo de una textura */
#define PILA_ATR 16

/* ---- las texturas ---- */
enum { TF_565 = 0, TF_1555, TF_4444 };
typedef struct { u16 *p; int w, h, lw, lh; } Nivel;
typedef struct Tex {
	u32 nombre;
	int dim;                         /* 1 o 2 */
	int base;                        /* GL_RGB, GL_RGBA, GL_ALPHA, GL_LUMINANCE... */
	int fmt;                         /* TF_* */
	int intfmt;                      /* el que pidió el programa */
	Nivel niv[NIVELES];
	int minf, magf, ws, wt;
	float borde[4], prioridad;
	int estado;                      /* 0: hay que mirar si está completa; 1 sí; 2 no */
	int nniv;                        /* los niveles que se usan (si completa) */
	u8 *paleta;                      /* GL_COLOR_INDEX (glColorTableEXT): 256×4 */
	struct Tex *sig;
} Tex;

/* ---- un vértice ya pasado por la transformación ---- */
typedef struct {
	float c[4];                      /* en el espacio de recorte */
	float e[4];                      /* en el del ojo (si hace falta) */
	float col[4], colb[4];           /* el color de delante y de detrás, 0..1 */
	float t[UNIDADES][4];            /* las coordenadas de textura */
	float niebla;                    /* 1: sin niebla, 0: toda */
	float x, y, z, iw;               /* en la ventana (y hacia abajo), z 0..65535 */
	u8 cod, borde, proy, pad;
} Vtx;

/* ---- las luces y los materiales ---- */
typedef struct {
	float amb[4], dif[4], esp[4];
	float pos[4];                    /* en el espacio del ojo */
	float dirn[3];                   /* (w = 0) la dirección hacia la luz, unitaria */
	float dir[3];                    /* la del foco, del ojo, normalizada */
	float dir_o[3];                  /* la del foco, tal cual (glGetLight) */
	float exp, corte, cos_corte;
	float att[3];
} Luz;
typedef struct { float amb[4], dif[4], esp[4], emi[4], brillo; } Material;

typedef struct {
	int modo;                        /* GL_OBJECT_LINEAR, GL_EYE_LINEAR, GL_SPHERE_MAP */
	float obj[4], ojo[4];
} TexGen;

/* lo que se habilita (glEnable): todo junto, para glPushAttrib */
typedef struct {
	u8 alfa, mezcla, cara, prof, niebla, luz, colmat, normaliza, tijera;
	u8 tex1d[UNIDADES], tex2d[UNIDADES], gen[UNIDADES][4];
	u8 luzi[LUCES], plano[PLANOS];
	u8 desplaza_r, desplaza_l, desplaza_p, punteado_p, punteado_l, suave_p, suave_l, suave_pol;
	u8 plantilla, logica, tramado, autonormal;
} Habil;

typedef struct {                     /* GL_CURRENT_BIT */
	float col[4], nor[3], tc[UNIDADES][4], indice;
	u8 borde;
	float rpos[4], rcol[4], rtc[4], rdist;
	int rvalida;
} Corriente;

typedef struct {                     /* GL_LIGHTING_BIT (menos lo de Habil) */
	Luz l[LUCES];
	Material m[2];
	float amb[4];
	int local, dos_caras;
	int cm_cara, cm_modo;
	int sombreado;                   /* GL_FLAT, GL_SMOOTH */
} Luces;

typedef struct {                     /* GL_COLOR_BUFFER_BIT */
	int alfa_f; float alfa_ref;
	int m_or, m_de;
	int mascara[4];
	float borra[4];
	int dibuja, logica_op;
} ColorBuf;

typedef struct {                     /* GL_DEPTH_BUFFER_BIT */
	int f, escribe;
	float borra;
} ProfBuf;

typedef struct {                     /* GL_FOG_BIT */
	int modo;
	float dens, ini, fin, col[4], indice;
} Niebla;

typedef struct {                     /* GL_POLYGON_BIT */
	int cara, frente, modo[2];
	float desp_f, desp_u;
} Poligono;

typedef struct {                     /* GL_TEXTURE_BIT */
	Tex *lig1d[UNIDADES], *lig2d[UNIDADES];
	int env[UNIDADES];
	float env_col[UNIDADES][4];
	TexGen gen[UNIDADES][4];
	int activa;
} Texturas;

typedef struct {                     /* GL_VIEWPORT_BIT */
	int x, y, w, h;
	float cerca, lejos;
} Vista;

typedef struct {                     /* GL_SCISSOR_BIT */
	int x, y, w, h;
} Tijera;

typedef struct {                     /* GL_TRANSFORM_BIT */
	int modo;
	double plano[PLANOS][4];         /* en el espacio del ojo */
} Transf;

typedef struct {                     /* GL_PIXEL_MODE_BIT (lo que se usa) */
	float zoom_x, zoom_y;
	float escala[4], sesgo[4];
	int leer;
} ModoPix;

typedef struct {                     /* glPixelStore */
	int alin, fila, salta_p, salta_f, lsb, inv;
} Almacen;

typedef struct {                     /* las plantillas (sin búfer: no hay) */
	int f, ref, masc, escr, op[3];
	int borra;
} Plantilla;

typedef struct {                     /* una matriz de vértices (array) */
	int tam, tipo, paso;
	const u8 *p;
	int si;
} Arr;
typedef struct {                     /* GL_CLIENT_VERTEX_ARRAY_BIT */
	Arr v, n, c, t[UNIDADES], b, i;
	int tex_cli;
} Arrays;

typedef struct {
	u32 mascara;
	Habil hab;
	Corriente cur; Luces luz; ColorBuf col; ProfBuf prof; Niebla nie;
	Poligono pol; Texturas tex; Vista vis; Tijera tij; Transf trf; ModoPix pix;
	float punto, linea; int punteado_f; u16 punteado_p;
	int sug[5];
	u32 lista_base;
	Plantilla pla;
	u8 punteo[128];
} Atrib;

typedef struct { u32 mascara; Almacen emp, des; Arrays arr; } AtribCli;

/* una lista de visualización */
typedef struct { u32 *d; u32 n, cap; } Lista;

/* las texturas, compartidas entre contextos (wglShareLists) */
typedef struct Comun {
	int usos;
	Tex *hash[256];
	u32 sig_tex;
	Lista *listas; u32 nlistas;      /* el número de la lista es el índice */
	Tex def1d, def2d, proxy1d, proxy2d;  /* las de nombre 0 y las de prueba (GL_PROXY_TEXTURE_*) */
	u8 paleta[256 * 4];              /* GL_SHARED_TEXTURE_PALETTE_EXT */
	int hay_paleta;
} Comun;

typedef struct Ctx {
	/* el búfer (la superficie de la ventana) */
	int an, al;
	u16 *color, *prof;
	int dobles;
	void *plat;                      /* lo de la plataforma (wgl.c) */

	Comun *com;
	Habil hab;
	Corriente cur;
	Luces luz;
	ColorBuf colb;
	ProfBuf profb;
	Niebla nie;
	Poligono pol;
	Texturas tex;
	Vista vis;
	Tijera tij;
	Transf trf;
	ModoPix pix;
	Almacen emp, des;                /* empaquetar (leer) y desempaquetar (subir) */
	Plantilla pla;
	Arrays arr;
	float punto, linea;
	int punteado_f; u16 punteado_p;
	u8 punteo[128];
	int sug[5];                      /* perspectiva, puntos, líneas, polígonos, niebla */

	/* las matrices */
	Mat mv[PILA_MV], pr[PILA_PR], tx[UNIDADES][PILA_TX];
	int nmv, npr, ntx[UNIDADES];
	Mat mvp;                         /* proyección · modelo-vista */
	float nor[9];                    /* la de las normales (3×3) */
	int sucio_mvp, sucio_nor;
	int tx_ident[UNIDADES];

	/* glBegin/glEnd */
	int prim;                        /* -1: fuera */
	int nv, par;
	Vtx vb[4];
	Vtx *pol_v; int pol_n, pol_cap;  /* GL_POLYGON (y GL_LINE_LOOP: el primero) */
	Vtx primero;

	/* lo que se precalcula al cambiar el estado */
	int necesita_ojo;
	float nie_k;

	/* listas */
	u32 lista_n;                     /* la que se está haciendo (0: ninguna) */
	int lista_modo;                  /* GL_COMPILE o GL_COMPILE_AND_EXECUTE */
	Lista lista_cur;
	int lista_prof;
	u32 lista_base;

	/* pilas de atributos */
	Atrib *atr; int natr;
	AtribCli *atrc; int natrc;

	int render_modo;
	int error;
	int bloqueados_ini, bloqueados_n;

	/* lo que cuenta el rasterizador (para medir) */
	u32 n_tri, n_pix;
} Ctx;

extern Ctx *hgl_c;
#define CTX(...) Ctx *C = hgl_c; if (!C) return __VA_ARGS__

void hgl_error(Ctx *C, int e);
Ctx *hgl_nuevo(Ctx *comparte);
void hgl_borra(Ctx *C);
int hgl_tamano(Ctx *C, int an, int al);  /* el búfer, del tamaño de la ventana */
void hgl_estado_cambia(Ctx *C);           /* tras glEnable, glFog... */

/* las listas: grabar una llamada (la función de dentro y sus argumentos) */
void hgl_graba(Ctx *C, void *fn, int n, ...);          /* n argumentos de 32 bits */
void hgl_graba_v(Ctx *C, void *fn, const void *p, u32 palabras);  /* fn(copia de p) */
#define F(x) hgl_fu(x)
#define LISTA(fn, n, ...) do { if (C->lista_n) { hgl_graba(C, (void *)(fn), n, __VA_ARGS__); if (C->lista_modo == GL_COMPILE) return; } } while (0)
#define LISTA0(fn) do { if (C->lista_n) { hgl_graba(C, (void *)(fn), 0); if (C->lista_modo == GL_COMPILE) return; } } while (0)

/* matriz.c */
void hgl_matrices_ini(Ctx *C);
Mat *hgl_mat_actual(Ctx *C);
void hgl_mat_al_dia(Ctx *C);
void i_mat_modo(u32 m);
void i_mat_carga(const float *m);
void i_mat_mul(const float *m);
void i_mat_ident(void);
void i_mat_empuja(void);
void i_mat_saca(void);
void i_rota(float a, float x, float y, float z);
void i_traslada(float x, float y, float z);
void i_escala(float x, float y, float z);
void i_frustum(float l, float r, float b, float t, float n, float f);
void i_orto(float l, float r, float b, float t, float n, float f);
void i_vista(int x, int y, int w, int h);
void i_prof_rango(float n, float f);

/* vertice.c */
void i_empieza(u32 modo);
void i_acaba(void);
void i_vertice(float x, float y, float z, float w);
void i_color(float r, float g, float b, float a);
void i_normal(float x, float y, float z);
void i_texc(u32 unidad, float s, float t, float r, float q);
void i_borde(u32 b);
void hgl_elemento(Ctx *C, int i, int graba);
void hgl_luz_vertice(Ctx *C, const float *eye, const float *n, float *col, float *colb);
void hgl_transforma(Ctx *C, Vtx *v, const float *o);
void hgl_vertice_ini(Ctx *C);

/* raster.c */
/* lado: 0 los colores de delante, 1 los de detrás; plano: el color de todo el
 * triángulo (GL_FLAT) o NULL */
void hgl_triangulo(Ctx *C, const Vtx *a, const Vtx *b, const Vtx *c, int lado, const float *plano);
void hgl_linea(Ctx *C, const Vtx *a, const Vtx *b, int lado, const float *plano);
void hgl_punto(Ctx *C, const Vtx *a, int lado);
void hgl_fragmento(Ctx *C, int x, int y, u32 z, int r, int g, int b, int a);  /* (x, y) de la pantalla */
void hgl_raster_ini(void);

/* textura.c */
Tex *hgl_tex_busca(Comun *c, u32 nombre, int crea);
void hgl_tex_suelta_todo(Comun *c);
Tex *hgl_tex_activa(Ctx *C, int u);      /* la que se usa en la unidad u (o NULL) */
int hgl_tex_completa(Tex *t);
void hgl_texturas_ini(Ctx *C);
void hgl_lee_pix(Ctx *C, const Almacen *a, int w, int h, u32 formato, u32 tipo, const void *p, u8 *rgba);

/* listas.c */
void hgl_listas_suelta(Comun *c);
void hgl_ejecuta_lista(Ctx *C, u32 n);

/* estado.c */
void hgl_estado_ini(Ctx *C);

/* pixeles.c */
void i_raster_pos(float x, float y, float z, float w);

/* las extensiones que se dan (estado.c) */
extern const char hgl_extensiones[];

static inline u16 hgl_565(int r, int g, int b) { return (u16)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)); }
#endif
