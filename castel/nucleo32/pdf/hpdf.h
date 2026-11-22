/* hpdf.h - Hispa-PDF: el núcleo para trabajar PDF (leer, escribir, las
 * páginas, el cifrado, el texto y pintarlas). C99 portable: lo del sistema,
 * en so.h. Ver docs/PENTIUM.md, «Hispa-PDF».
 *
 * Seguridad: los PDF vienen de Internet. Todo lleva su límite (lo que mide
 * el archivo, los objetos, lo hondo que se anida, lo que crece un flujo al
 * descomprimirlo, las páginas, las vueltas del árbol) y nada se recorre con
 * una recursión sin tope. Lo que no se puede seguir (sin memoria, un límite
 * pasado) salta a la marca de la orden (so_marca) con su mensaje.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HPDF_H
#define HPDF_H
#include <stdarg.h>
#include "so.h"

/* ---- los límites ---- */
#define MAX_ARCHIVO_PDF (96u * 1024 * 1024)	/* lo que mide un PDF que se abre */
#define MAX_OBJETOS (4u * 1024 * 1024)		/* el número más alto de objeto */
#define MAX_HONDO 64				/* arreglos y diccionarios, uno dentro de otro */
#define MAX_FLUJO (128u * 1024 * 1024)		/* un flujo, descomprimido */
#define MAX_PAGINAS 50000
#define MAX_XREF 256				/* las tablas de un archivo (/Prev) */
#define MAX_ELEMENTOS (1u << 20)		/* en un arreglo o un diccionario */
#define MAX_CADENA (32u * 1024 * 1024)

/* ---- los objetos ---- */
enum { O_NULO, O_BOOL, O_ENT, O_REAL, O_NOMBRE, O_CADENA, O_ARREGLO, O_DICC, O_REF, O_FLUJO };

typedef struct pobj pobj;
typedef struct pdoc pdoc;

struct pobj {
	u8 t;
	u8 hex;			/* la cadena se escribe en hexadecimal */
	u8 cifrada;		/* (flujo) los datos aún llevan el cifrado del archivo */
	u8 propio;		/* (flujo) los datos son del objeto (no del archivo) */
	u32 n;			/* largo de la cadena o del nombre; elementos */
	u32 cap;
	union {
		int b;
		i32 i;
		double r;
		u8 *s;		/* nombre (sin la barra) o cadena; con un 0 detrás */
		pobj **v;	/* arreglo: n; diccionario: 2n (clave, valor) */
		struct { u32 num; u32 gen; } ref;
		struct { pobj *dic; const u8 *datos; u32 largo; u32 num, gen; } f;
	} u;
};

/* una entrada de la tabla de objetos */
typedef struct {
	u8 tipo;		/* 0 libre, 1 en el archivo, 2 en un flujo de objetos */
	u8 cargando;		/* para no dar vueltas */
	u8 malo;		/* no se ha podido leer */
	u16 gen;
	u32 pos;		/* 1: dónde; 2: el número del flujo de objetos */
	u32 indice;		/* 2: cuál dentro */
	pobj *obj;
} pentrada;

typedef struct bloque bloque;

/* el cifrado (el manejador estándar del PDF) */
typedef struct {
	int v, r, largo;	/* V, R y el largo de la clave en octetos */
	int metodo_flujo;	/* 0 nada, 1 RC4, 2 AES-128, 3 AES-256 */
	int metodo_cadena;
	int cifra_metadatos;
	u8 clave[32];
	u8 o[48], u[48], oe[32], ue[32], perms[16];
	i32 p;
	u8 id0[64];
	int n_id0;
	u32 num_encrypt;	/* el objeto del diccionario /Encrypt (no se descifra) */
	int listo;
} pcifrado;

struct pdoc {
	u8 *datos;		/* el archivo, entero (de so_pide) */
	u32 largo;
	int version;		/* 10 * mayor + menor */
	pentrada *e;
	u32 n;			/* entradas */
	pobj *trailer;
	pobj *raiz;		/* el catálogo */
	int reparado;		/* se ha rehecho la tabla buscando los objetos */
	int intentado_reparar;
	int cifrado;		/* tiene /Encrypt */
	pcifrado c;
	bloque *bloques;	/* la memoria del documento */
	u32 n_paginas;
	pobj **paginas;		/* los diccionarios de las páginas, en orden */
	u32 *num_pagina;	/* su número de objeto */
	pobj *nulo;
	void *fuentes;		/* (fuente.c) las letras ya leídas */
	u32 n_fuentes;
	char error[160];
};

/* ---- los errores sin vuelta ---- */
extern so_salto *hpdf_salto;
extern char hpdf_mensaje[200];
void hpdf_falla(const char *fmt, ...) __attribute__((noreturn));

/* ---- la memoria del documento (se suelta toda al cerrarlo) ---- */
void *ar_pide(pdoc *d, u32 n);
void ar_suelta(pdoc *d);
void *pide(size_t n);		/* so_pide o hpdf_falla */
void *repide(void *p, size_t n);

/* ---- objetos (objetos.c) ---- */
pobj *o_nuevo(pdoc *d, int t);
pobj *o_ent(pdoc *d, i32 v);
pobj *o_real(pdoc *d, double v);
pobj *o_nombre(pdoc *d, const char *s);
pobj *o_cadena(pdoc *d, const u8 *s, u32 n);
pobj *o_ref(pdoc *d, u32 num, u32 gen);
pobj *o_arreglo(pdoc *d);
pobj *o_dicc(pdoc *d);
void o_mete(pdoc *d, pobj *a, pobj *x);			/* al final de un arreglo */
void o_pon(pdoc *d, pobj *dic, const char *clave, pobj *x);	/* (NULL: lo quita) */
pobj *o_dic_clave(pobj *dic, const char *clave);	/* sin resolver */
int o_es_nombre(pobj *o, const char *s);
double o_num(pobj *o, double si_no);
int o_es_num(pobj *o);
pobj *o_dic_de(pobj *o);				/* el diccionario (también el de un flujo) */

/* el analizador (de un trozo de memoria) */
typedef struct {
	const u8 *p, *ini, *fin;
} plex;
enum { T_FIN, T_ENT, T_REAL, T_NOMBRE, T_CADENA, T_HEX, T_ABRE_A, T_CIERRA_A, T_ABRE_D, T_CIERRA_D,
	T_ABRE_L, T_CIERRA_L, T_PALABRA, T_MALO };
typedef struct {
	int t;
	const u8 *p;		/* dónde empieza */
	u32 n;
	i32 i;
	double r;
} ptok;
int es_blanco(int c);
int es_delim(int c);
void lex_blancos(plex *l);
int lex_token(plex *l, ptok *t);
int tok_es(ptok *t, const char *s);
pobj *lee_objeto(pdoc *d, plex *l, int hondo);		/* NULL: mal */
pobj *lee_desde_token(pdoc *d, plex *l, ptok *t, int hondo);
u32 cadena_literal(const u8 *p, const u8 *fin, u8 *sal, const u8 **tras);
u32 cadena_hex(const u8 *p, const u8 *fin, u8 *sal, const u8 **tras);

/* ---- el documento (documento.c) ---- */
pdoc *pdf_abre(u8 *datos, u32 largo, const char *clave, int *necesita_clave);
pdoc *pdf_abre_archivo(const char *ruta, const char *clave, int *necesita_clave, const char **error);
void pdf_cierra(pdoc *d);
pdoc *pdf_vacio(void);					/* para hacer uno nuevo */
pobj *resuelve(pdoc *d, pobj *o);			/* sigue las referencias */
pobj *objeto(pdoc *d, u32 num);
pobj *dic_r(pdoc *d, pobj *dic, const char *clave);	/* la clave, resuelta */
int pdf_paginas(pdoc *d);				/* el árbol, a d->paginas */
pobj *pagina_heredada(pdoc *d, u32 i, const char *clave);	/* Resources, MediaBox, CropBox, Rotate */
void pagina_caja(pdoc *d, u32 i, double caja[4]);	/* la CropBox (o la MediaBox), en orden */
int pagina_giro(pdoc *d, u32 i);

/* los datos de un flujo, sin cifrado y descomprimidos (de pide; *n su
 * largo). Si «imagen», se para antes de un filtro de imagen (DCT, JPX,
 * JBIG2, CCITT) y dice cuál en *queda (si no, NULL). NULL si no se puede */
u8 *flujo_datos(pdoc *d, pobj *f, u32 *n, int imagen, const char **queda);
/* los datos tal cual (sin cifrado, aún comprimidos) */
u8 *flujo_crudo(pdoc *d, pobj *f, u32 *n);

/* ---- los filtros (filtros.c) ---- */
u8 *flate_dec(const u8 *d, u32 n, u32 *sal, u32 max);
u8 *flate_cod(const u8 *d, u32 n, u32 *sal, int nivel);
u8 *filtro(pdoc *doc, const char *nombre, pobj *parm, const u8 *d, u32 n, u32 *sal);

/* ---- el cifrado (cifra.c) ---- */
void md5(const u8 *d, u32 n, u8 sal[16]);
typedef struct { u8 s[256]; u8 i, j; } rc4_t;
void rc4_pon(rc4_t *r, const u8 *k, int n);
void rc4(rc4_t *r, u8 *d, u32 n);
typedef struct { u32 k[60]; int rondas; } aes_t;
void aes_pon(aes_t *a, const u8 *k, int bits);
void aes_cifra(const aes_t *a, const u8 e[16], u8 s[16]);
void aes_descifra(const aes_t *a, const u8 e[16], u8 s[16]);
void sha256(const u8 *d, u32 n, u8 sal[32]);
void sha384(const u8 *d, u32 n, u8 sal[48]);
void sha512(const u8 *d, u32 n, u8 sal[64]);
int cifrado_prepara(pdoc *d, pobj *enc, const char *clave);	/* 1 bien; 0 clave mala; -1 no se sabe */
u8 *descifra(pdoc *d, u32 num, u32 gen, const u8 *e, u32 n, u32 *sal, int cadena);
void descifra_cadenas(pdoc *d, pobj *o, u32 num, u32 gen, int hondo);
/* para escribir uno cifrado */
typedef struct {
	int metodo;		/* 1 RC4-128, 2 AES-128, 3 AES-256 */
	u8 clave[32];
	int largo;
	pobj *dic;		/* el /Encrypt */
} pcifra_sal;
int cifrado_nuevo(pdoc *d, pcifra_sal *c, int metodo, const char *usuario, const char *dueno, const u8 id0[16], i32 permisos);
u8 *cifra(pcifra_sal *c, u32 num, u32 gen, const u8 *e, u32 n, u32 *sal);

/* ---- escribir (escribe.c) ---- */
typedef struct psal psal;
typedef struct {
	int comprime;		/* volver a comprimir los flujos con Flate */
	int objetos_en_flujo;	/* los objetos sueltos, en flujos de objetos (PDF 1.5) */
	int calidad_img;	/* 0 no tocar las imágenes; si no, la calidad JPEG (1-100) */
	int lado_img;		/* lo más que mide una imagen al comprimir (puntos) */
	int cifrado;		/* 0, o 1 RC4, 2 AES-128, 3 AES-256 */
	const char *clave_usuario, *clave_dueno;
	i32 permisos;
} popciones;

/* el documento que se escribe: sus objetos, nuevos o copiados de otros */
typedef struct {
	pobj *o;
	pdoc *de;		/* de qué documento vienen sus flujos */
	u32 origen;		/* su número allí */
} psalobj;
typedef struct {
	pdoc *d;		/* la memoria de lo nuevo */
	psalobj *v;
	u32 n, cap;
	u32 raiz, info;
	int version;
	/* el copiado */
	struct pmapa { pdoc *de; u32 *a; u32 n; } mapa[64];
	int n_mapa;
	u32 *pend_num, *pend_de;	/* lo que falta por copiar: el número nuevo y su mapa */
	u32 n_pend, cap_pend;
	pobj *dests;		/* (unir) los destinos con nombre de todos */
} pnuevo;
pnuevo *nuevo_doc(void);
void nuevo_suelta(pnuevo *s);
u32 nuevo_obj(pnuevo *s, pobj *o, pdoc *de);		/* da su número */
pobj *copia_de(pnuevo *s, pdoc *de, pobj *o);		/* lo de «de», con sus referencias renumeradas */
u32 *mapa_num(pnuevo *s, pdoc *de, u32 num);		/* dónde va el número nuevo de ese objeto */
void copia_pendientes(pnuevo *s);
int escribe_pdf(pnuevo *s, const char *ruta, popciones *op, const char **error);

/* ---- las páginas (paginas.c) ---- */
typedef struct {
	pdoc *d;
	u32 pagina;		/* desde 0 */
	int giro;		/* lo que se suma a su /Rotate */
} ppag;
/* hace un documento con esas páginas, en ese orden (de uno o de varios) */
pnuevo *compone(ppag *p, u32 n, int con_marcadores);
u32 arbol_paginas(pnuevo *s, u32 *hojas, u32 n);	/* el árbol de esas páginas: da su raíz */
/* «1-3,5,8-» (y «z» o «fin»: la última) a una lista de páginas (desde 0) */
int rangos(const char *t, u32 total, u32 *sal, u32 cabe);

/* ---- el texto (texto.c) ---- */
typedef struct ptexto ptexto;
u8 *pagina_texto(pdoc *d, u32 i, u32 *n);		/* UTF-8 */

/* ---- pintar (pinta.c) ---- */
typedef struct {
	int an, al;
	u32 *px;		/* 0x00RRGGBB, fila a fila */
} pimagen;
/* pinta la página i a «ppp» puntos por pulgada (o, con lado > 0, que quepa en lado x lado) */
pimagen *pinta_pagina(pdoc *d, u32 i, double ppp, int lado);
void imagen_suelta(pimagen *im);
void pinta_fuentes(const u8 *sans, u32 n_sans, const u8 *serif, u32 n_serif, const u8 *mono, u32 n_mono,
		   const u8 *sansn, u32 n_sansn, const u8 *serifn, u32 n_serifn, const u8 *monon, u32 n_monon);

/* las imágenes de un flujo (pinta.c), a 0xAARRGGBB */
u32 *imagen_de(pdoc *d, pobj *f, int *an, int *al, u32 relleno, int hondo);

/* ---- las imágenes (imagen.c) ---- */
typedef struct {
	double an, al;		/* la página, en puntos (0: lo que mida la imagen) */
	double margen;
} ppapel;
u32 pagina_de_imagen(pnuevo *s, const u8 *datos, u32 n, ppapel *papel, const char **error);
u8 *imagen_a_archivo(pimagen *im, int jpg, int calidad, u32 *n);
u8 *decodifica_imagen(const u8 *d, u32 n, int *w, int *h, int *comp, int quiero);
void suelta_stbi(void *p);
u8 *jpeg_menor(const u8 *d, u32 n, int calidad, int lado, int *an, int *al, u32 *sal);

/* ---- texto suelto (util.c) ---- */
u32 utf8_a_437(const u8 *s, u32 n, u8 *d);
u32 de_437(u8 c);
int fmt(char *d, int cabe, const char *p, ...);
int vfmt(char *d, int cabe, const char *p, va_list va);
void dice(const char *p, ...);
int igual_sin_caja(const char *a, const char *b);
int empieza_sin_caja(const char *a, const char *b);
void a_utf8(u32 c, u8 **p);

#endif
