/* formula.h - Cuentas 2: las fórmulas. Se leen («=SUMA(A1:B3)*2») a una
 * lista de piezas en notación polaca inversa (lo que se calcula, en
 * orden), y se escriben de vuelta desde ellas: en español o en inglés, con
 * la coma o el punto decimal, y en las formas de los archivos (la de los
 * .XLSX, en inglés; la de los .ODS, con [.A1]). Las referencias guardan la
 * fila y la columna de verdad (no lo que se apartan): al copiar una
 * fórmula se corren las que no llevan «$» (fm_copia_desplaza), y al meter
 * o quitar filas y columnas se arreglan todas (fm_ajusta).
 *
 * De menos a más prioridad (como en las hojas de cálculo de siempre):
 *   comparaciones (= <> < > <= >=), &, + -, * /, ^, el signo (- +), %
 * («-2^2» es 4). Referencias: A1, $A$1, A$1, Hoja2!A1, 'Mi hoja'!A1:B5,
 * A:C (columnas enteras), 3:7 (filas enteras), los nombres definidos.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef CU_FORMULA_H
#define CU_FORMULA_H
#include "libro.h"

enum {
	T_NUM, T_TXT, T_LOG, T_ERR, T_REF, T_RANGO, T_NOMBRE, T_FUNC, T_FALTA,
	T_SUMA, T_RESTA, T_POR, T_ENTRE, T_POT, T_UNE, T_IGUAL, T_DISTINTO, T_MENOR, T_MAYOR, T_MENOR_IG, T_MAYOR_IG,
	T_NEG, T_POS, T_PORC, T_PAREN, T_NOMBRE_MAL,
	T_DOSP			/* «:» entre dos referencias (A1:INDICE(...)): el rango que las abarca */
};
/* (T_REF, T_RANGO) a: los «$» y lo demás */
#define RA_C0 1			/* columna (de la primera) fija */
#define RA_F0 2
#define RA_C1 4
#define RA_F1 8
#define RA_HOJA 16		/* con la hoja escrita (Hoja2!A1) */
#define RA_COLS 32		/* columnas enteras (A:C) */
#define RA_FILAS 64		/* filas enteras (3:7) */
typedef struct {
	u8 t;			/* T_* */
	u8 a;			/* RA_* */
	u16 h;			/* (T_REF, T_RANGO) el id de la hoja; (T_FUNC) la función */
	union {
		double n;	/* T_NUM, T_LOG */
		struct {
			i32 f0, f1;
			u16 c0, c1;
		} r;		/* T_REF (solo f0, c0), T_RANGO */
		struct {
			u32 i;	/* T_TXT: dónde empieza en «textos»; T_NOMBRE: su índice en el libro; T_ERR: ER_* */
			u32 n;	/* T_FUNC: cuántos argumentos; T_TXT: lo que mide */
		} x;
	} u;
} cu_tok;

typedef struct formula {
	u32 n;			/* las piezas */
	u32 n_txt;		/* lo que miden los textos */
	u8 volatil;		/* (HOY, AHORA, ALEATORIO, INDIRECTO, DESREF...) se calcula siempre */
	u8 con_nombres;
	u16 _r;
	char *txt;		/* los textos (de las piezas T_TXT y de los nombres que no existen) */
	u32 *nodos;		/* (calculo.c) sus dependencias apuntadas */
	u32 n_nodos;
	cu_tok t[1];		/* (y las demás detrás) */
} formula;

/* lee lo de detrás del «=» (s, n octetos) escrito en «loc», como de la
 * celda (f, c) de la hoja h (la de las referencias sin hoja). NULL si está
 * mal: en err, por qué (en español) y en *donde, la letra */
formula *fm_lee(libro *l, const hoja *h, const char *s, u32 n, const cu_local *loc, char *err, u32 cabe, u32 *donde);
/* escribe la fórmula (sin el «=») en «loc» */
void fm_escribe(const libro *l, const formula *fm, const hoja *h, const cu_local *loc, bufer *b);
/* lo mismo, a un texto (cortado si no cabe); da lo que mide */
u32 fm_texto(const libro *l, const formula *fm, const hoja *h, const cu_local *loc, char *d, u32 cabe);
formula *fm_copia(const formula *fm);
/* la copia corrida df filas y dc columnas (lo que no lleva «$»); lo que se sale, #¡REF! */
formula *fm_copia_desplaza(const formula *fm, int df, int dc);
/* meter (n > 0) o quitar (n < 0) filas (eje 0) o columnas (eje 1) en la hoja de id h,
 * desde la fila o columna «desde»: se corren las referencias; las que se quitan, #¡REF!.
 * «aqui»: el id de la hoja de la celda de la fórmula (para las referencias sin hoja). 1 si cambia */
int fm_ajusta(formula *fm, u16 aqui, u16 h, int eje, u32 desde, int n);
/* lo mismo al mover un rango (cortar y pegar, arrastrar): lo que apuntaba a
 * él pasa a apuntar a donde va */
int fm_mueve(formula *fm, u16 aqui, u16 h, rango de, int df, int dc, u16 h_a);
/* las referencias a la hoja h (que se quita) pasan a ser #¡REF! */
int fm_sin_hoja(formula *fm, u16 aqui, u16 h);
void fm_suelta(formula *fm);
/* ¿lleva la fórmula el nombre «n»? (para borrar un nombre) */

/* ---- las funciones (funciones.c) ---- */
#define FN_VOLATIL 1		/* se calcula siempre */
#define FN_REFS 2		/* quiere los rangos tal cual (no sus valores) */
#define FN_SIN_ERR 4		/* ve los errores de sus argumentos (SI.ERROR, ESERROR...) */
#define FN_PEREZOSA 8		/* SI, ELEGIR, SI.ERROR: los argumentos se calculan cuando hacen falta */
typedef struct {
	const char *es, *en;	/* los nombres (en mayúsculas) */
	u8 min, max;		/* argumentos (max 255: los que sean) */
	u8 opc;			/* FN_* */
} cu_funcion;
extern const cu_funcion CU_FUNCIONES[];
extern const u32 CU_N_FUNCIONES;
/* la de ese nombre (en español o en inglés, sin mirar mayúsculas ni tildes; con o sin «_xlfn.»); -1 */
int fn_busca(const char *s, u32 n);
/* para el asistente: los nombres en español, por orden, separados por «|» */
const char *fn_lista(void);

#endif
