/* ram_alg.h - (TERMO, RAM) lo de la prueba de la memoria RAM que no habla
 * con el sistema (ram_alg.c): los patrones sobre un trozo de memoria, lo que
 * se apunta de cada fallo, el diagnóstico (agrupar: el mismo bit, el mismo
 * chip, una zona, dispersos, solo con calor) y leer el SMBIOS (tipos 16, 17
 * y 20: las ranuras, el tamaño, la velocidad y el fabricante; los números
 * de serie, nunca). Lo usan TERMO.BIN (ram.c) y, en el anfitrión, la prueba
 * con un fallo metido a propósito (pruebas/ram_prueba.c).
 * Escrito para Hispa-Pentium (la idea de las pruebas es la de siempre, la de
 * memtest86+ y la de cualquier libro de pruebas de memoria; el código, propio).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef RAM_ALG_H
#define RAM_ALG_H
#include <stdint.h>

typedef uint32_t ra_u32;
typedef uint64_t ra_u64;

/* las pruebas (para decir en cuál ha salido cada fallo) */
enum {
	RA_LINEAS = 1,		/* las líneas de dirección (cada potencia de 2, la suya) */
	RA_DIRECCION,		/* cada palabra guarda su dirección (y luego su contraria) */
	RA_ANDANTES,		/* unos andantes y ceros andantes */
	RA_PATRON,		/* 0x55555555 y 0xAAAAAAAA (y los de la completa) */
	RA_INVERSIONES,		/* «moving inversions» con azar sembrado (subiendo y bajando) */
	RA_TODOS,		/* con todos los núcleos a la vez (calor) */
	RA_NPRUEBAS
};
const char *ra_nombre_prueba(int k);

/* el congruencial del azar (el mismo que el de termoram.cas) y su vuelta atrás */
#define RA_LCG_A 1664525u
#define RA_LCG_C 1013904223u
#define RA_LCG_AINV 0xFEE058C5u	/* (el inverso de A, módulo 2^32) */
static inline ra_u32 ra_sig(ra_u32 x) { return x * RA_LCG_A + RA_LCG_C; }
static inline ra_u32 ra_ant(ra_u32 x) { return (x - RA_LCG_C) * RA_LCG_AINV; }

/* ---- los fallos ---- */
#define RA_DET 256	/* los que se guardan con detalle */
#define RA_BLOQUES 64	/* las zonas de 1 MB distintas que se cuentan (hasta aquí) */
struct ra_detalle {
	ra_u64 fis;		/* la dirección física (o la virtual, si no se sabe la física: fis_sabida = 0) */
	ra_u32 esperado, leido;
	unsigned char prueba, todos, fis_sabida, r;
};
struct ra_fallos {
	ra_u64 total;			/* los fallos (las palabras mal leídas) */
	ra_u64 por_bit[64];		/* cuántas veces ha fallado cada bit del bus de 64 (el 0-31 de las palabras de dirección ...0 y ...8, el 32-63 de las de ...4 y ...C) */
	ra_u64 por_prueba[RA_NPRUEBAS];
	ra_u64 en_todos, en_uno;	/* con todos los núcleos / con uno */
	ra_u64 a1_bit[64];		/* de esos, los que se han leído a 1 (debiendo ser 0) */
	ra_u64 fmin, fmax;		/* la menor y la mayor dirección física con fallo */
	ra_u32 bloques[RA_BLOQUES];	/* las zonas de 1 MB distintas con fallo (la dirección >> 20) */
	int nbloques, bloques_lleno;
	int ndet;
	struct ra_detalle det[RA_DET];
};
void ra_fallos_empieza(struct ra_fallos *f);
/* un fallo: dónde (física, si se sabe), lo esperado, lo leído, en qué prueba y si con todos */
void ra_apunta(struct ra_fallos *f, ra_u64 fis, int fis_sabida, ra_u32 esperado, ra_u32 leido, int prueba, int todos);

/* ---- las pruebas sobre un trozo (n palabras desde p) ----
 * (RA_LEE y RA_ESCRIBE: en TERMO.BIN, leer y escribir la memoria sin más; la
 * prueba del anfitrión los cambia para meter un fallo, -DRA_SIMULA) */
#ifdef RA_SIMULA
ra_u32 ra_simula_lee(volatile ra_u32 *p);
void ra_simula_escribe(volatile ra_u32 *p, ra_u32 v);
#define RA_LEE(p) ra_simula_lee(p)
#define RA_ESCRIBE(p, v) ra_simula_escribe((p), (v))
#else
#define RA_LEE(p) (*(p))
#define RA_ESCRIBE(p, v) (*(p) = (v))
#endif
/* Cada prueba llama a fallo() con la palabra mal leída (su dirección, lo
 * esperado, lo leído) y da cuántos fallos ha visto */
typedef void (*ra_fallo_fn)(void *ctx, volatile ra_u32 *dir, ra_u32 esperado, ra_u32 leido, int prueba);
struct ra_ctx {
	ra_fallo_fn fallo;
	void *ctx;
	ra_u64 fallos;
	int (*parar)(void *ctx);	/* (de vez en cuando) 1: dejarlo ya */
	int parado;
};
ra_u32 ra_lineas(struct ra_ctx *c, volatile ra_u32 *p, ra_u32 n);
ra_u32 ra_direccion(struct ra_ctx *c, volatile ra_u32 *p, ra_u32 n);
ra_u32 ra_andantes(struct ra_ctx *c, volatile ra_u32 *p, ra_u32 n, int ronda, int unos);
ra_u32 ra_patron(struct ra_ctx *c, volatile ra_u32 *p, ra_u32 n, ra_u32 patron);
ra_u32 ra_inversiones(struct ra_ctx *c, volatile ra_u32 *p, ra_u32 n, ra_u32 semilla);

/* ---- el diagnóstico ---- */
enum { RA_D_BIEN, RA_D_LINEAS, RA_D_BIT, RA_D_CHIP, RA_D_ZONA, RA_D_CALOR, RA_D_DISPERSO };
struct ra_diag {
	int clase;		/* RA_D_* */
	int bit;		/* (RA_D_BIT) el bit del bus de 64; (RA_D_CHIP) el primero de su octeto */
	int a_uno;		/* el bit se queda a 1 (1), a 0 (0) o de las dos maneras (-1) */
};
void ra_diagnostica(const struct ra_fallos *f, struct ra_diag *d);
/* el porqué, en palabras (UTF-8, sin depender de colores) */
int ra_diag_texto(const struct ra_fallos *f, const struct ra_diag *d, char *t, int cabe);

/* ---- el SMBIOS ---- */
#define RA_MODULOS 16
struct ra_modulo {
	unsigned handle;
	char ranura[32], banco[32];	/* «DIMM A», «ChannelA-DIMM0», «BANK 0» */
	ra_u32 mb;			/* 0: ranura vacía */
	unsigned mts, mts_conf;		/* la velocidad (MT/s): la del módulo y la puesta */
	unsigned tipo;			/* 0x18 DDR3, 0x1A DDR4... */
	unsigned ancho_total, ancho_datos;	/* 72 / 64: con ECC */
	char fabricante[32], pieza[32];	/* (el modelo, no el número de serie) */
};
struct ra_mapa {	/* tipo 20: qué direcciones van a cada módulo (si la BIOS lo dice) */
	ra_u64 ini, fin;	/* en octetos, [ini, fin] */
	unsigned handle;
};
struct ra_smbios {
	int hay, nmod, nmapa, ranuras;	/* ranuras: las que dice el tipo 16 (0: no lo dice) */
	int ecc;			/* 1 con ECC, 0 sin, -1 no se sabe */
	ra_u64 max_mb;			/* (tipo 16) lo más que admite la placa */
	struct ra_modulo m[RA_MODULOS];
	struct ra_mapa mapa[RA_MODULOS];
};
void ra_lee_smbios(const unsigned char *t, ra_u32 largo, struct ra_smbios *s);
const char *ra_tipo_memoria(unsigned tipo);
/* el módulo en que cae una dirección física (por el tipo 20): su índice en m[], o -1 */
int ra_modulo_de(const struct ra_smbios *s, ra_u64 fis);
/* «1 módulo de 8 GB DDR3 a 1600 MT/s (Samsung) en «ChannelA-DIMM0»; ...» */
int ra_texto_modulos(const struct ra_smbios *s, char *t, int cabe);
/* «12,3 GB», «512 MB» (d: 24 octetos) */
char *ra_tam(char *d, ra_u64 octetos);

#endif
