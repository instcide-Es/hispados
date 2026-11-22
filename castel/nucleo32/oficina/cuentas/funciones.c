/* funciones.c - Cuentas 2: las funciones de las fórmulas, con su nombre en
 * español (el de las hojas de cálculo en español: SUMA, BUSCARV, AÑO...) y
 * en inglés (SUM, VLOOKUP, YEAR...): las dos se entienden siempre al
 * escribir. Las de Cuentas de Hispa-386 están todas. Las cuentas de dinero
 * (PAGO, VA, VF, TASA, NPER, VNA, TIR, PAGOINT, PAGOPRIN) son las de
 * siempre, con «tipo» 0 (al final del periodo) o 1 (al principio).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "evalua.h"
#include "numeros.h"
#include "mate.h"

#define LISTA \
	F(SUMA, "SUMA", "SUM", 1, 255, 0) \
	F(PROMEDIO, "PROMEDIO", "AVERAGE", 1, 255, 0) \
	F(CONTAR, "CONTAR", "COUNT", 1, 255, 0) \
	F(CONTARA, "CONTARA", "COUNTA", 1, 255, 0) \
	F(CONTAR_BLANCO, "CONTAR.BLANCO", "COUNTBLANK", 1, 1, 0) \
	F(MAX, "MAX", "MAX", 1, 255, 0) \
	F(MIN, "MIN", "MIN", 1, 255, 0) \
	F(PRODUCTO, "PRODUCTO", "PRODUCT", 1, 255, 0) \
	F(MEDIANA, "MEDIANA", "MEDIAN", 1, 255, 0) \
	F(MODA, "MODA", "MODE", 1, 255, 0) \
	F(VAR, "VAR", "VAR", 1, 255, 0) \
	F(VARP, "VARP", "VARP", 1, 255, 0) \
	F(DESVEST, "DESVEST", "STDEV", 1, 255, 0) \
	F(DESVESTP, "DESVESTP", "STDEVP", 1, 255, 0) \
	F(K_MAYOR, "K.ESIMO.MAYOR", "LARGE", 2, 2, 0) \
	F(K_MENOR, "K.ESIMO.MENOR", "SMALL", 2, 2, 0) \
	F(JERARQUIA, "JERARQUIA", "RANK", 2, 3, 0) \
	F(SUMA_CUADRADOS, "SUMA.CUADRADOS", "SUMSQ", 1, 255, 0) \
	F(PROMEDIOA, "PROMEDIOA", "AVERAGEA", 1, 255, 0) \
	F(MAXA, "MAXA", "MAXA", 1, 255, 0) \
	F(MINA, "MINA", "MINA", 1, 255, 0) \
	F(REDONDEAR, "REDONDEAR", "ROUND", 2, 2, 0) \
	F(REDONDEAR_MAS, "REDONDEAR.MAS", "ROUNDUP", 2, 2, 0) \
	F(REDONDEAR_MENOS, "REDONDEAR.MENOS", "ROUNDDOWN", 2, 2, 0) \
	F(TRUNCAR, "TRUNCAR", "TRUNC", 1, 2, 0) \
	F(ENTERO, "ENTERO", "INT", 1, 1, 0) \
	F(ABS, "ABS", "ABS", 1, 1, 0) \
	F(RAIZ, "RAIZ", "SQRT", 1, 1, 0) \
	F(POTENCIA, "POTENCIA", "POWER", 2, 2, 0) \
	F(RESIDUO, "RESIDUO", "MOD", 2, 2, 0) \
	F(COCIENTE, "COCIENTE", "QUOTIENT", 2, 2, 0) \
	F(SIGNO, "SIGNO", "SIGN", 1, 1, 0) \
	F(PI, "PI", "PI", 0, 0, 0) \
	F(ALEATORIO, "ALEATORIO", "RAND", 0, 0, FN_VOLATIL) \
	F(ALEATORIO_ENTRE, "ALEATORIO.ENTRE", "RANDBETWEEN", 2, 2, FN_VOLATIL) \
	F(EXP, "EXP", "EXP", 1, 1, 0) \
	F(LN, "LN", "LN", 1, 1, 0) \
	F(LOG, "LOG", "LOG", 1, 2, 0) \
	F(LOG10, "LOG10", "LOG10", 1, 1, 0) \
	F(SENO, "SENO", "SIN", 1, 1, 0) \
	F(COS, "COS", "COS", 1, 1, 0) \
	F(TAN, "TAN", "TAN", 1, 1, 0) \
	F(ASENO, "ASENO", "ASIN", 1, 1, 0) \
	F(ACOS, "ACOS", "ACOS", 1, 1, 0) \
	F(ATAN, "ATAN", "ATAN", 1, 1, 0) \
	F(ATAN2, "ATAN2", "ATAN2", 2, 2, 0) \
	F(GRADOS, "GRADOS", "DEGREES", 1, 1, 0) \
	F(RADIANES, "RADIANES", "RADIANS", 1, 1, 0) \
	F(FACT, "FACT", "FACT", 1, 1, 0) \
	F(COMBINAT, "COMBINAT", "COMBIN", 2, 2, 0) \
	F(MULTIPLO_SUPERIOR, "MULTIPLO.SUPERIOR", "CEILING", 2, 2, 0) \
	F(MULTIPLO_INFERIOR, "MULTIPLO.INFERIOR", "FLOOR", 2, 2, 0) \
	F(REDOND_MULT, "REDOND.MULT", "MROUND", 2, 2, 0) \
	F(PAR, "REDONDEA.PAR", "EVEN", 1, 1, 0) \
	F(IMPAR, "REDONDEA.IMPAR", "ODD", 1, 1, 0) \
	F(MCD, "M.C.D", "GCD", 1, 255, 0) \
	F(MCM, "M.C.M", "LCM", 1, 255, 0) \
	F(SUMAPRODUCTO, "SUMAPRODUCTO", "SUMPRODUCT", 1, 255, 0) \
	F(SUMAR_SI, "SUMAR.SI", "SUMIF", 2, 3, 0) \
	F(CONTAR_SI, "CONTAR.SI", "COUNTIF", 2, 2, 0) \
	F(PROMEDIO_SI, "PROMEDIO.SI", "AVERAGEIF", 2, 3, 0) \
	F(SUMAR_SI_CONJ, "SUMAR.SI.CONJUNTO", "SUMIFS", 3, 255, 0) \
	F(CONTAR_SI_CONJ, "CONTAR.SI.CONJUNTO", "COUNTIFS", 2, 255, 0) \
	F(SUBTOTALES, "SUBTOTALES", "SUBTOTAL", 2, 255, 0) \
	F(ROMANO, "NUMERO.ROMANO", "ROMAN", 1, 2, 0) \
	F(SI, "SI", "IF", 2, 3, FN_SIN_ERR) \
	F(Y, "Y", "AND", 1, 255, 0) \
	F(O, "O", "OR", 1, 255, 0) \
	F(NO, "NO", "NOT", 1, 1, 0) \
	F(VERDADERO, "VERDADERO", "TRUE", 0, 0, 0) \
	F(FALSO, "FALSO", "FALSE", 0, 0, 0) \
	F(SI_ERROR, "SI.ERROR", "IFERROR", 2, 2, FN_SIN_ERR) \
	F(ESERROR, "ESERROR", "ISERROR", 1, 1, FN_SIN_ERR) \
	F(ESERR, "ESERR", "ISERR", 1, 1, FN_SIN_ERR) \
	F(ESNOD, "ESNOD", "ISNA", 1, 1, FN_SIN_ERR) \
	F(ESNUMERO, "ESNUMERO", "ISNUMBER", 1, 1, FN_SIN_ERR) \
	F(ESTEXTO, "ESTEXTO", "ISTEXT", 1, 1, FN_SIN_ERR) \
	F(ESNOTEXTO, "ESNOTEXTO", "ISNONTEXT", 1, 1, FN_SIN_ERR) \
	F(ESBLANCO, "ESBLANCO", "ISBLANK", 1, 1, FN_SIN_ERR) \
	F(ESLOGICO, "ESLOGICO", "ISLOGICAL", 1, 1, FN_SIN_ERR) \
	F(ESREF, "ESREF", "ISREF", 1, 1, FN_SIN_ERR) \
	F(ESPAR, "ES.PAR", "ISEVEN", 1, 1, 0) \
	F(ESIMPAR, "ES.IMPAR", "ISODD", 1, 1, 0) \
	F(NOD, "NOD", "NA", 0, 0, 0) \
	F(TIPO, "TIPO", "TYPE", 1, 1, FN_SIN_ERR) \
	F(N, "N", "N", 1, 1, 0) \
	F(TIPO_ERROR, "TIPO.DE.ERROR", "ERROR.TYPE", 1, 1, FN_SIN_ERR) \
	F(BUSCARV, "BUSCARV", "VLOOKUP", 3, 4, 0) \
	F(BUSCARH, "BUSCARH", "HLOOKUP", 3, 4, 0) \
	F(BUSCAR, "BUSCAR", "LOOKUP", 2, 3, 0) \
	F(COINCIDIR, "COINCIDIR", "MATCH", 2, 3, 0) \
	F(INDICE, "INDICE", "INDEX", 2, 3, FN_REFS) \
	F(ELEGIR, "ELEGIR", "CHOOSE", 2, 255, FN_REFS) \
	F(FILA, "FILA", "ROW", 0, 1, FN_REFS) \
	F(COLUMNA, "COLUMNA", "COLUMN", 0, 1, FN_REFS) \
	F(FILAS, "FILAS", "ROWS", 1, 1, FN_REFS) \
	F(COLUMNAS, "COLUMNAS", "COLUMNS", 1, 1, FN_REFS) \
	F(INDIRECTO, "INDIRECTO", "INDIRECT", 1, 2, FN_VOLATIL) \
	F(DESREF, "DESREF", "OFFSET", 3, 5, FN_VOLATIL | FN_REFS) \
	F(DIRECCION, "DIRECCION", "ADDRESS", 2, 5, 0) \
	F(CONCATENAR, "CONCATENAR", "CONCATENATE", 1, 255, 0) \
	F(IZQUIERDA, "IZQUIERDA", "LEFT", 1, 2, 0) \
	F(DERECHA, "DERECHA", "RIGHT", 1, 2, 0) \
	F(EXTRAE, "EXTRAE", "MID", 3, 3, 0) \
	F(LARGO, "LARGO", "LEN", 1, 1, 0) \
	F(MAYUSC, "MAYUSC", "UPPER", 1, 1, 0) \
	F(MINUSC, "MINUSC", "LOWER", 1, 1, 0) \
	F(NOMPROPIO, "NOMPROPIO", "PROPER", 1, 1, 0) \
	F(ESPACIOS, "ESPACIOS", "TRIM", 1, 1, 0) \
	F(ENCONTRAR, "ENCONTRAR", "FIND", 2, 3, 0) \
	F(HALLAR, "HALLAR", "SEARCH", 2, 3, 0) \
	F(SUSTITUIR, "SUSTITUIR", "SUBSTITUTE", 3, 4, 0) \
	F(REEMPLAZAR, "REEMPLAZAR", "REPLACE", 4, 4, 0) \
	F(REPETIR, "REPETIR", "REPT", 2, 2, 0) \
	F(TEXTO, "TEXTO", "TEXT", 2, 2, 0) \
	F(VALOR, "VALOR", "VALUE", 1, 1, 0) \
	F(CARACTER, "CARACTER", "CHAR", 1, 1, 0) \
	F(CODIGO, "CODIGO", "CODE", 1, 1, 0) \
	F(IGUAL, "IGUAL", "EXACT", 2, 2, 0) \
	F(DECIMAL, "DECIMAL", "FIXED", 1, 3, 0) \
	F(MONEDA, "MONEDA", "DOLLAR", 1, 2, 0) \
	F(T, "T", "T", 1, 1, 0) \
	F(LIMPIAR, "LIMPIAR", "CLEAN", 1, 1, 0) \
	F(HOY, "HOY", "TODAY", 0, 0, FN_VOLATIL) \
	F(AHORA, "AHORA", "NOW", 0, 0, FN_VOLATIL) \
	F(FECHA, "FECHA", "DATE", 3, 3, 0) \
	F(NSHORA, "NSHORA", "TIME", 3, 3, 0) \
	F(ANO, "AÑO", "YEAR", 1, 1, 0) \
	F(MES, "MES", "MONTH", 1, 1, 0) \
	F(DIA, "DIA", "DAY", 1, 1, 0) \
	F(HORA, "HORA", "HOUR", 1, 1, 0) \
	F(MINUTO, "MINUTO", "MINUTE", 1, 1, 0) \
	F(SEGUNDO, "SEGUNDO", "SECOND", 1, 1, 0) \
	F(DIASEM, "DIASEM", "WEEKDAY", 1, 2, 0) \
	F(DIAS360, "DIAS360", "DAYS360", 2, 3, 0) \
	F(FECHANUMERO, "FECHANUMERO", "DATEVALUE", 1, 1, 0) \
	F(HORANUMERO, "HORANUMERO", "TIMEVALUE", 1, 1, 0) \
	F(FIN_MES, "FIN.MES", "EOMONTH", 2, 2, 0) \
	F(FECHA_MES, "FECHA.MES", "EDATE", 2, 2, 0) \
	F(DIAS_LAB, "DIAS.LAB", "NETWORKDAYS", 2, 3, 0) \
	F(DIA_LAB, "DIA.LAB", "WORKDAY", 2, 3, 0) \
	F(NUM_SEMANA, "NUM.DE.SEMANA", "WEEKNUM", 1, 2, 0) \
	F(SIFECHA, "SIFECHA", "DATEDIF", 3, 3, 0) \
	F(DIAS, "DIAS", "DAYS", 2, 2, 0) \
	F(PAGO, "PAGO", "PMT", 3, 5, 0) \
	F(VA, "VA", "PV", 3, 5, 0) \
	F(VF, "VF", "FV", 3, 5, 0) \
	F(TASA, "TASA", "RATE", 3, 6, 0) \
	F(NPER, "NPER", "NPER", 3, 5, 0) \
	F(VNA, "VNA", "NPV", 2, 255, 0) \
	F(TIR, "TIR", "IRR", 1, 2, 0) \
	F(PAGOINT, "PAGOINT", "IPMT", 4, 6, 0) \
	F(PAGOPRIN, "PAGOPRIN", "PPMT", 4, 6, 0) \
	F(SLN, "SLN", "SLN", 3, 3, 0) \
	F(SYD, "SYD", "SYD", 4, 4, 0) \
	F(DDB, "DDB", "DDB", 4, 5, 0)

enum {
#define F(id, es, en, mi, ma, op) FN_##id,
	LISTA
#undef F
	FN_TOTAL
};

const cu_funcion CU_FUNCIONES[] = {
#define F(id, es, en, mi, ma, op) { es, en, mi, ma, op },
	LISTA
#undef F
};
const u32 CU_N_FUNCIONES = FN_TOTAL;

/* ---- los nombres ---- */
static u32 pliega(u32 c)
{
	return sin_tilde(mayuscula(c));
}

static int mismo_nombre(const char *a, u32 na, const char *b)
{
	u32 i = 0, j = 0, nb = largo(b);

	while (i < na && j < nb) {
		u32 x, y;

		i += (u32)utf8_lee((const u8 *)a + i, na - i, &x);
		j += (u32)utf8_lee((const u8 *)b + j, nb - j, &y);
		if (pliega(x) != pliega(y))
			return 0;
	}
	return i == na && j == nb;
}

int fn_busca(const char *s, u32 n)
{
	u32 i;

	if (n > 6 && (strncmp(s, "_xlfn.", 6) == 0 || strncmp(s, "_xlws.", 6) == 0)) {
		s += 6;
		n -= 6;
	}
	if (n > 4 && strncmp(s, "COM.MICROSOFT.", 14 < n ? 14 : n) == 0 && n > 14) {
		s += 14;
		n -= 14;
	}
	for (i = 0; i < FN_TOTAL; i++)
		if (mismo_nombre(s, n, CU_FUNCIONES[i].es) || mismo_nombre(s, n, CU_FUNCIONES[i].en))
			return (int)i;
	/* los de otros nombres (los de antes, los de LibreOffice) */
	{
		static const struct {
			const char *n;
			int f;
		} otros[] = { { "PROMEDIO.SI.CONJUNTO", -1 }, { "ESNOD", FN_ESNOD }, { "AÑOS", -1 }, { "CEILING.MATH", FN_MULTIPLO_SUPERIOR },
			      { "FLOOR.MATH", FN_MULTIPLO_INFERIOR }, { "STDEV.S", FN_DESVEST }, { "STDEV.P", FN_DESVESTP },
			      { "VAR.S", FN_VAR }, { "VAR.P", FN_VARP }, { "MODE.SNGL", FN_MODA }, { "RANK.EQ", FN_JERARQUIA },
			      { "CONCAT", FN_CONCATENAR }, { "PAR", FN_PAR }, { "IMPAR", FN_IMPAR }, { "ANO", FN_ANO },
			      { "DESVEST.M", FN_DESVEST }, { "DESVEST.P", FN_DESVESTP }, { "VAR.S", FN_VAR }, { "MODA.UNO", FN_MODA },
			      { "JERARQUIA.EQV", FN_JERARQUIA }, { "MULTIPLO.SUPERIOR.MAT", FN_MULTIPLO_SUPERIOR },
			      { "MULTIPLO.INFERIOR.MAT", FN_MULTIPLO_INFERIOR }, { "CONCAT", FN_CONCATENAR } };

		for (i = 0; i < sizeof otros / sizeof otros[0]; i++)
			if (otros[i].f >= 0 && mismo_nombre(s, n, otros[i].n))
				return otros[i].f;
	}
	return -1;
}

const char *fn_lista(void)
{
	static char t[4096];
	static int hecha;
	int orden[FN_TOTAL], i, j;

	if (hecha)
		return t;
	for (i = 0; i < FN_TOTAL; i++)
		orden[i] = i;
	for (i = 1; i < FN_TOTAL; i++) {
		int k = orden[i];

		for (j = i; j > 0 && strcmp(CU_FUNCIONES[orden[j - 1]].es, CU_FUNCIONES[k].es) > 0; j--)
			orden[j] = orden[j - 1];
		orden[j] = k;
	}
	t[0] = 0;
	for (i = 0; i < FN_TOTAL; i++) {
		if (i)
			anade(t, "|", sizeof t);
		anade(t, CU_FUNCIONES[orden[i]].es, sizeof t);
	}
	hecha = 1;
	return t;
}

/* ---- ayudas ---- */
#define FALTA_E 0xFF
static int falta(const ev *a)
{
	return a->t == V_VACIO && a->e == FALTA_E;
}

/* un argumento numérico (si falta, def) */
static int num(ctx *x, const ev *a, double def, double *v)
{
	if (falta(a)) {
		*v = def;
		return 0;
	}
	return ev_a_num(x, a, v);
}

#define NUM(i, def, v)                                \
	do {                                          \
		double v_ = 0;                        \
		int e_ = (i) < n ? num(x, &a[i], (def), &v_) : (v_ = (def), 0); \
		if (e_) {                             \
			ev_err(r, e_);                \
			return;                       \
		}                                     \
		v = v_;                               \
	} while (0)

#define TXT(i, s, sn)                                           \
	do {                                                    \
		int e_ = ev_a_txt(x, &a[i], &(s), &(sn));       \
		if (e_) {                                       \
			ev_err(r, e_);                          \
			return;                                 \
		}                                               \
	} while (0)

/* la lista de los números de unos argumentos (los rangos: solo sus números;
 * con «con_a», los textos valen 0 y los lógicos 0 o 1, como PROMEDIOA) */
typedef struct {
	double *v;
	u32 n, cabe;
	double suma;
	u32 cuenta, cuenta_a;
	int err;
	int guarda;
	int con_a;
	int sin_ocultas;	/* (SUBTOTALES 101...) */
	int sin_subtotales;
	hoja *h;
	ctx *x;
} lista;

static void l_mete(lista *L, double v)
{
	L->suma += v;
	L->cuenta++;
	if (!L->guarda)
		return;
	if (L->n >= L->cabe) {
		u32 c = L->cabe ? L->cabe * 2 : 64;
		double *n = so_repide(L->v, c * sizeof(double));

		if (!n) {
			L->err = ER_VALOR;
			return;
		}
		L->v = n;
		L->cabe = c;
	}
	L->v[L->n++] = v;
}

static int es_subtotal(const celda *k)
{
	u32 i;

	if (!k || !k->f)
		return 0;
	for (i = 0; i < k->f->n; i++)
		if (k->f->t[i].t == T_FUNC && k->f->t[i].h == FN_SUBTOTALES)
			return 1;
	return 0;
}

static int cada_lista(void *u, const valor *v, u32 f, u32 c)
{
	lista *L = u;

	if (L->sin_ocultas && hj_fila_oculta(L->h, f))
		return 0;
	if (L->sin_subtotales && es_subtotal(hj_celda(L->h, f, c)))
		return 0;
	switch (v->t) {
	case V_NUM:
		l_mete(L, v->n);
		L->cuenta_a++;
		break;
	case V_ERR:
		if (!L->err)
			L->err = v->e;
		return 1;
	case V_TXT:
		L->cuenta_a++;
		if (L->con_a)
			l_mete(L, 0);
		break;
	case V_LOG:
		L->cuenta_a++;
		if (L->con_a)
			l_mete(L, v->n);
		break;
	}
	return 0;
}

static void junta(ctx *x, ev *a, int n, lista *L)
{
	int i;

	for (i = 0; i < n && !L->err; i++) {
		if (falta(&a[i]))
			continue;
		if (a[i].t == V_REF) {
			L->h = lib_hoja_id(x->l, a[i].h);
			ev_recorre(x, &a[i], 0, cada_lista, L);
		} else if (a[i].t == V_ERR) {
			L->err = a[i].e;
		} else if (a[i].t == V_NUM || a[i].t == V_LOG) {
			l_mete(L, a[i].n);
			L->cuenta_a++;
		} else if (a[i].t == V_TXT) {
			double v;

			L->cuenta_a++;
			if (ev_a_num(x, &a[i], &v) == 0)
				l_mete(L, v);
			else if (L->con_a)
				l_mete(L, 0);
			else if (!L->guarda || 1) {
				/* (un texto escrito que no es número: CONTAR no lo cuenta; SUMA da #¡VALOR!) */
				L->err = L->err ? L->err : (L->guarda == 2 ? 0 : ER_VALOR);
			}
		} else if (a[i].t == V_VACIO) {
			/* (un argumento vacío vale 0) */
		}
	}
}

static void ordena(double *v, u32 n)
{
	u32 i, k;

	if (n < 2)
		return;
	for (i = n / 2; i-- > 0;)
		for (k = i;;) {
			u32 h = 2 * k + 1;
			double t;

			if (h >= n)
				break;
			if (h + 1 < n && v[h + 1] > v[h])
				h++;
			if (v[k] >= v[h])
				break;
			t = v[k];
			v[k] = v[h];
			v[h] = t;
			k = h;
		}
	for (i = n - 1; i > 0; i--) {
		double t = v[0];

		v[0] = v[i];
		v[i] = t;
		for (k = 0;;) {
			u32 h = 2 * k + 1;

			if (h >= i)
				break;
			if (h + 1 < i && v[h + 1] > v[h])
				h++;
			if (v[k] >= v[h])
				break;
			t = v[k];
			v[k] = v[h];
			v[h] = t;
			k = h;
		}
	}
}

static double varianza(const lista *L, int poblacion)
{
	double m = L->suma / L->n, s = 0;
	u32 i;

	for (i = 0; i < L->n; i++)
		s += (L->v[i] - m) * (L->v[i] - m);
	return s / (poblacion ? L->n : L->n - 1);
}

/* ---- los criterios (SUMAR.SI, CONTAR.SI...) ---- */
typedef struct {
	int op;			/* 0 =, 1 <>, 2 <, 3 >, 4 <=, 5 >= */
	int es_num;
	double n;
	const char *s;
	u32 sn;
	int vacio;		/* «» o «=»: las vacías */
	int no_vacio;		/* «<>» */
	ctx *x;
} criterio;

static void lee_criterio(ctx *x, const ev *a0, criterio *c)
{
	ev a;

	memset(c, 0, sizeof *c);
	c->x = x;
	ev_valor(x, a0, &a);
	if (a.t == V_NUM || a.t == V_LOG) {
		c->es_num = 1;
		c->n = a.n;
		return;
	}
	if (a.t == V_VACIO) {
		c->es_num = 1;
		c->n = 0;
		return;
	}
	if (a.t != V_TXT) {
		c->es_num = 1;
		c->n = 0;
		return;
	}
	{
		const char *s = a.s;
		u32 n = a.sn;
		int tipo;

		if (n >= 2 && s[0] == '<' && s[1] == '=')
			c->op = 4, s += 2, n -= 2;
		else if (n >= 2 && s[0] == '>' && s[1] == '=')
			c->op = 5, s += 2, n -= 2;
		else if (n >= 2 && s[0] == '<' && s[1] == '>')
			c->op = 1, s += 2, n -= 2;
		else if (n >= 1 && s[0] == '<')
			c->op = 2, s++, n--;
		else if (n >= 1 && s[0] == '>')
			c->op = 3, s++, n--;
		else if (n >= 1 && s[0] == '=')
			c->op = 0, s++, n--;
		c->s = s;
		c->sn = n;
		if (!n) {
			if (c->op == 1)
				c->no_vacio = 1;
			else
				c->vacio = 1;
			return;
		}
		if (num_lee_entrada(s, &x->l->loc, x->l->moneda, &c->n, &tipo))
			c->es_num = 1;
	}
}

static int cumple(const criterio *c, const valor *v)
{
	if (c->vacio)
		return v->t == V_VACIO || (v->t == V_TXT && (!v->s || !v->s[0]));
	if (c->no_vacio)
		return !(v->t == V_VACIO || (v->t == V_TXT && (!v->s || !v->s[0])));
	if (c->es_num) {
		double d;

		if (v->t != V_NUM && !(v->t == V_LOG && 0))
			return c->op == 1 ? 1 : 0;
		d = v->n - c->n;
		if (so_abs(d) <= (so_abs(v->n) > so_abs(c->n) ? so_abs(v->n) : so_abs(c->n)) * 1e-15)
			d = 0;
		switch (c->op) {
		case 0: return d == 0;
		case 1: return d != 0;
		case 2: return d < 0;
		case 3: return d > 0;
		case 4: return d <= 0;
		default: return d >= 0;
		}
	}
	/* un texto */
	{
		const char *s = "";
		u32 sn = 0;

		if (v->t == V_TXT && v->s) {
			s = v->s;
			sn = largo(s);
		} else if (v->t != V_VACIO)
			return c->op == 1;
		if (c->op == 0)
			return v->t == V_TXT && ev_texto_igual(s, sn, c->s, c->sn, 1);
		if (c->op == 1)
			return !ev_texto_igual(s, sn, c->s, c->sn, 1);
		if (v->t != V_TXT)
			return 0;
		{
			int k = ev_texto_cmp(s, sn, c->s, c->sn);

			return c->op == 2 ? k < 0 : c->op == 3 ? k > 0 : c->op == 4 ? k <= 0 : k >= 0;
		}
	}
}

/* el valor de la celda (f0 + df, c0 + dc) de un rango: para SUMAR.SI con su rango de sumar */
static const valor *en_rango(ctx *x, const ev *ref, u32 df, u32 dc)
{
	static const valor vacio = { V_VACIO, 0, 0, NULL };
	u32 f = ref->r.f0 + df, c = ref->r.c0 + dc;

	if (f >= CU_FILAS || c >= CU_COLS)
		return &vacio;
	return ev_celda(x, ref->h, f, c);
}

typedef struct {
	criterio c;
	const ev *suma;		/* (o NULL) */
	const ev *rango;
	double total;
	u32 cuenta;
	int err;
	ctx *x;
} si_t;

static int cada_si(void *u, const valor *v, u32 f, u32 c)
{
	si_t *S = u;

	if (!cumple(&S->c, v))
		return 0;
	S->cuenta++;
	if (S->suma) {
		const valor *w = en_rango(S->x, S->suma, f - S->rango->r.f0, c - S->rango->r.c0);

		if (w->t == V_NUM)
			S->total += w->n;
		else if (w->t == V_ERR && !S->err)
			S->err = w->e;
	} else if (v->t == V_NUM)
		S->total += v->n;
	return 0;
}

/* (el criterio vacío pide mirar también las celdas que no existen) */
static void sumar_si(ctx *x, ev *a, int n, ev *r, int que)
{
	si_t S;

	memset(&S, 0, sizeof S);
	S.x = x;
	if (a[0].t != V_REF) {
		ev_err(r, ER_VALOR);
		return;
	}
	lee_criterio(x, &a[1], &S.c);
	S.rango = &a[0];
	if (n >= 3 && !falta(&a[2])) {
		if (a[2].t != V_REF) {
			ev_err(r, ER_VALOR);
			return;
		}
		S.suma = &a[2];
	}
	if (S.c.vacio) {
		/* las vacías: todas las posiciones (con su tope) */
		u64 area = (u64)(a[0].r.f1 - a[0].r.f0 + 1) * (a[0].r.c1 - a[0].r.c0 + 1);

		if (que == 1 && area > 1048576) {
			/* (CONTAR.SI(A:A; "") de una columna entera: las que no tienen nada) */
			si_t T = S;

			T.c.vacio = 0;
			T.c.no_vacio = 1;
			ev_recorre(x, &a[0], 0, cada_si, &T);
			ev_num(r, (double)(area - T.cuenta));
			return;
		}
		ev_recorre(x, &a[0], 1, cada_si, &S);
	} else
		ev_recorre(x, &a[0], 0, cada_si, &S);
	if (S.err) {
		ev_err(r, S.err);
		return;
	}
	if (que == 1)
		ev_num(r, S.cuenta);
	else if (que == 2) {
		if (!S.cuenta)
			ev_err(r, ER_DIV0);
		else
			ev_num(r, S.total / S.cuenta);
	} else
		ev_num(r, S.total);
}

/* SUMAR.SI.CONJUNTO y CONTAR.SI.CONJUNTO: por posiciones */
static void si_conjunto(ctx *x, ev *a, int n, ev *r, int contar)
{
	int primero = contar ? 0 : 1, k, np = (n - primero) / 2;
	criterio cs[127];
	u32 f, c, alto, ancho;
	double total = 0;
	u32 cuenta = 0;

	if ((n - primero) % 2 || np < 1 || np > 127) {
		ev_err(r, ER_VALOR);
		return;
	}
	for (k = 0; k < n; k++)
		if ((k == 0 && !contar) || (k >= primero && (k - primero) % 2 == 0))
			if (a[k].t != V_REF) {
				ev_err(r, ER_VALOR);
				return;
			}
	alto = a[primero].r.f1 - a[primero].r.f0 + 1;
	ancho = a[primero].r.c1 - a[primero].r.c0 + 1;
	for (k = 0; k < np; k++) {
		const ev *rg = &a[primero + 2 * k];

		if (rg->r.f1 - rg->r.f0 + 1 != alto || rg->r.c1 - rg->r.c0 + 1 != ancho) {
			ev_err(r, ER_VALOR);
			return;
		}
		lee_criterio(x, &a[primero + 2 * k + 1], &cs[k]);
	}
	if (!contar && (a[0].r.f1 - a[0].r.f0 + 1 != alto || a[0].r.c1 - a[0].r.c0 + 1 != ancho)) {
		ev_err(r, ER_VALOR);
		return;
	}
	{
		/* (no se mira más allá de lo usado de la primera hoja) */
		hoja *h = lib_hoja_id(x->l, a[primero].h);

		if (h && h->lim_f && alto > h->lim_f - (a[primero].r.f0 < h->lim_f ? a[primero].r.f0 : h->lim_f) && !cs[0].vacio)
			alto = a[primero].r.f0 < h->lim_f ? h->lim_f - a[primero].r.f0 : 0;
		if ((u64)alto * ancho > 4u * 1024 * 1024) {
			ev_err(r, ER_VALOR);
			return;
		}
	}
	for (f = 0; f < alto; f++)
		for (c = 0; c < ancho; c++) {
			int bien = 1;

			for (k = 0; k < np && bien; k++)
				bien = cumple(&cs[k], en_rango(x, &a[primero + 2 * k], f, c));
			if (!bien)
				continue;
			cuenta++;
			if (!contar) {
				const valor *w = en_rango(x, &a[0], f, c);

				if (w->t == V_NUM)
					total += w->n;
			}
		}
	ev_num(r, contar ? cuenta : total);
}

/* ---- buscar ---- */
static void de_valor_ev(ctx *x, const valor *v, ev *r)
{
	ev q;

	memset(&q, 0, sizeof q);
	switch (v->t) {
	case V_NUM: ev_num(r, v->n); return;
	case V_TXT: ev_txt(x, r, v->s ? v->s : "", v->s ? largo(v->s) : 0); return;
	case V_LOG: ev_log(r, v->n != 0); return;
	case V_ERR: ev_err(r, v->e); return;
	}
	*r = q;
	r->t = V_VACIO;
}

/* la posición (desde 0) de «que» en un vector (de una fila o una columna del
 * rango ref, desde (f0, c0), n celdas, por filas si «por_f»): exacto o el
 * mayor <= (orden 1) o el menor >= (orden -1); -1 si no */
static i32 busca_vector(ctx *x, const ev *que, u16 h, u32 f0, u32 c0, u32 n, int por_f, int orden)
{
	u32 i;
	i32 ult = -1;
	hoja *hh = lib_hoja_id(x->l, h);

	if (!hh)
		return -1;
	/* (no más allá de lo usado) */
	if (por_f && hh->lim_f && f0 + n > hh->lim_f)
		n = f0 < hh->lim_f ? hh->lim_f - f0 : 0;
	if (!por_f && hh->lim_c && c0 + n > hh->lim_c)
		n = c0 < hh->lim_c ? hh->lim_c - c0 : 0;
	for (i = 0; i < n; i++) {
		const valor *v = ev_celda(x, h, por_f ? f0 + i : f0, por_f ? c0 : c0 + i);
		ev e;
		int k;

		if (v->t == V_VACIO)
			continue;
		de_valor_ev(x, v, &e);
		if (orden == 0) {
			if (que->t == V_TXT && e.t == V_TXT) {
				if (ev_texto_igual(e.s, e.sn, que->s, que->sn, 1))
					return (i32)i;
				continue;
			}
			if ((que->t == V_NUM) != (e.t == V_NUM) || (que->t == V_LOG) != (e.t == V_LOG))
				continue;
			if (ev_compara(x, &e, que) == 0)
				return (i32)i;
			continue;
		}
		/* (de otra clase: no cuenta) */
		if ((que->t == V_TXT) != (e.t == V_TXT) || (que->t == V_LOG) != (e.t == V_LOG))
			continue;
		k = ev_compara(x, &e, que);
		if (orden > 0) {
			if (k <= 0)
				ult = (i32)i;
			else
				break;
		} else {
			if (k >= 0)
				ult = (i32)i;
			else
				break;
		}
	}
	return ult;
}

static void buscarv(ctx *x, ev *a, int n, ev *r, int horiz)
{
	ev que;
	double col;
	int aprox = 1;
	i32 p;
	u32 alto, ancho;

	ev_valor(x, &a[0], &que);
	if (que.t == V_ERR) {
		ev_err(r, que.e);
		return;
	}
	if (a[1].t != V_REF) {
		ev_err(r, ER_NA);
		return;
	}
	NUM(2, 1, col);
	if (n >= 4 && !falta(&a[3])) {
		int e = ev_a_log(x, &a[3], &aprox);

		if (e) {
			ev_err(r, e);
			return;
		}
	} else if (n >= 4)
		aprox = 0;
	alto = a[1].r.f1 - a[1].r.f0 + 1;
	ancho = a[1].r.c1 - a[1].r.c0 + 1;
	if (col < 1) {
		ev_err(r, ER_VALOR);
		return;
	}
	if ((u32)col > (horiz ? alto : ancho)) {
		ev_err(r, ER_REF);
		return;
	}
	if (que.t == V_VACIO) {
		ev_err(r, ER_NA);
		return;
	}
	p = horiz ? busca_vector(x, &que, a[1].h, a[1].r.f0, a[1].r.c0, ancho, 0, aprox ? 1 : 0)
		  : busca_vector(x, &que, a[1].h, a[1].r.f0, a[1].r.c0, alto, 1, aprox ? 1 : 0);
	if (p < 0) {
		ev_err(r, ER_NA);
		return;
	}
	memset(r, 0, sizeof *r);
	r->t = V_REF;
	r->h = a[1].h;
	if (horiz) {
		r->r.f0 = r->r.f1 = a[1].r.f0 + (u32)col - 1;
		r->r.c0 = r->r.c1 = (u16)(a[1].r.c0 + (u32)p);
	} else {
		r->r.f0 = r->r.f1 = a[1].r.f0 + (u32)p;
		r->r.c0 = r->r.c1 = (u16)(a[1].r.c0 + (u32)col - 1);
	}
	ev_valor(x, r, r);
	if (r->t == V_VACIO)
		ev_num(r, 0);
}

/* ---- el texto ---- */
static u32 letras(const char *s, u32 n)
{
	return utf8_cuenta(s, n);
}

/* el octeto donde empieza la letra k (desde 0) */
static u32 octeto(const char *s, u32 n, u32 k)
{
	u32 i = 0, j = 0;

	while (i < n && j < k) {
		u32 c;

		i += (u32)utf8_lee((const u8 *)s + i, n - i, &c);
		j++;
	}
	return i;
}

static void cambia_caja(ctx *x, const char *s, u32 n, ev *r, int modo)
{
	bufer b = { 0 };
	u32 i = 0;
	int ini = 1;

	while (i < n) {
		u32 c;

		i += (u32)utf8_lee((const u8 *)s + i, n - i, &c);
		if (modo == 0)
			c = mayuscula(c);
		else if (modo == 1)
			c = minuscula(c);
		else {
			c = ini ? mayuscula(c) : minuscula(c);
			ini = !es_letra(c);
		}
		buf_u32(&b, c);
	}
	ev_txt(x, r, b.d ? (char *)b.d : "", b.n);
	buf_suelta(&b);
}

/* «encontrar» de a en b, desde la letra k (desde 0): la letra donde está o -1 */
static i32 encuentra(const char *b, u32 nb, const char *a, u32 na, u32 k, int sin_caja)
{
	u32 i = octeto(b, nb, k), letra = k;

	if (!na)
		return (i32)k;
	while (i <= nb) {
		if (sin_caja) {
			/* (con comodines: lo que casa desde aquí, de cualquier largo) */
			u32 fin;

			for (fin = i; fin <= nb; fin++) {
				if (fin > i && ((u8)b[fin - 1] & 0xC0) == 0x80)
					continue;
				if (ev_texto_igual(b + i, fin - i, a, na, 1))
					return (i32)letra;
			}
		} else if (i + na <= nb && memcmp(b + i, a, na) == 0)
			return (i32)letra;
		if (i >= nb)
			break;
		{
			u32 c;

			i += (u32)utf8_lee((const u8 *)b + i, nb - i, &c);
			letra++;
		}
	}
	return -1;
}

static u32 de_1252(u32 c)
{
	static const u16 t[32] = { 0x20AC, 0x81, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x8D, 0x017D, 0x8F,
				   0x90, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x9D, 0x017E, 0x0178 };

	if (c >= 0x80 && c < 0xA0)
		return t[c - 0x80];
	return c;
}

static void romano(int v, char *d)
{
	static const int val[] = { 1000, 900, 500, 400, 100, 90, 50, 40, 10, 9, 5, 4, 1 };
	static const char *const sim[] = { "M", "CM", "D", "CD", "C", "XC", "L", "XL", "X", "IX", "V", "IV", "I" };
	int i;

	d[0] = 0;
	for (i = 0; i < 13; i++)
		while (v >= val[i]) {
			anade(d, sim[i], 32);
			v -= val[i];
		}
}

/* ---- las fechas ---- */
static int dia_lab(double s)
{
	int d = dia_semana(s);

	return d != 0 && d != 6;
}

typedef struct {
	double *v;
	u32 n, cabe;
} fiestas;

static int cada_fiesta(void *u, const valor *v, u32 f, u32 c)
{
	fiestas *F = u;

	(void)f;
	(void)c;
	if (v->t != V_NUM)
		return 0;
	if (F->n >= F->cabe) {
		u32 k = F->cabe ? F->cabe * 2 : 32;
		double *n = so_repide(F->v, k * sizeof(double));

		if (!n)
			return 1;
		F->v = n;
		F->cabe = k;
	}
	F->v[F->n++] = so_suelo(v->n);
	return 0;
}

static int es_fiesta(const fiestas *F, double s)
{
	u32 i;

	for (i = 0; i < F->n; i++)
		if (F->v[i] == s)
			return 1;
	return 0;
}

static void lee_fiestas(ctx *x, ev *a, int n, int i, fiestas *F)
{
	memset(F, 0, sizeof *F);
	if (n > i && !falta(&a[i])) {
		if (a[i].t == V_REF)
			ev_recorre(x, &a[i], 0, cada_fiesta, F);
		else {
			double v;

			if (ev_a_num(x, &a[i], &v) == 0)
				cada_fiesta(F, &(valor){ V_NUM, 0, v, NULL }, 0, 0);
		}
	}
}

/* ---- el dinero ---- */
static double vf(double t, double n, double p, double va, int tipo)
{
	if (t == 0)
		return -(va + p * n);
	{
		double k = cu_pot(1 + t, n);

		return -(va * k + p * (1 + t * tipo) * (k - 1) / t);
	}
}

static double pago(double t, double n, double va, double vf_, int tipo)
{
	if (t == 0)
		return -(va + vf_) / n;
	{
		double k = cu_pot(1 + t, n);

		return -(t * (vf_ + va * k)) / ((1 + t * tipo) * (k - 1));
	}
}

static double pagoint(double t, double per, double n, double va, double vf_, int tipo)
{
	double p = pago(t, n, va, vf_, tipo), i;

	if (per == 1)
		return tipo ? 0 : -va * t;
	if (tipo)
		i = vf(t, per - 2, p, va, 1) - p;
	else
		i = vf(t, per - 1, p, va, 0);
	return i * t;
}

typedef struct {
	double *v;
	u32 n, cabe;
	int err;
} flujos;

static int cada_flujo(void *u, const valor *v, u32 f, u32 c)
{
	flujos *F = u;

	(void)f;
	(void)c;
	if (v->t == V_ERR) {
		F->err = v->e;
		return 1;
	}
	if (v->t != V_NUM)
		return 0;
	if (F->n >= F->cabe) {
		u32 k = F->cabe ? F->cabe * 2 : 32;
		double *n = so_repide(F->v, k * sizeof(double));

		if (!n) {
			F->err = ER_VALOR;
			return 1;
		}
		F->v = n;
		F->cabe = k;
	}
	F->v[F->n++] = v->n;
	return 0;
}

static void lee_flujos(ctx *x, ev *a, int n, flujos *F)
{
	int i;

	memset(F, 0, sizeof *F);
	for (i = 0; i < n && !F->err; i++) {
		if (a[i].t == V_REF) {
			/* (por orden: posición a posición) */
			u32 f, c;
			hoja *h = lib_hoja_id(x->l, a[i].h);
			rango g = a[i].r;

			if (h && h->lim_f && g.f1 >= h->lim_f)
				g.f1 = h->lim_f ? h->lim_f - 1 : 0;
			if (h && h->lim_c && g.c1 >= h->lim_c)
				g.c1 = (u16)(h->lim_c - 1);
			if ((u64)(g.f1 - g.f0 + 1) * (g.c1 - g.c0 + 1) > 1048576) {
				F->err = ER_VALOR;
				return;
			}
			for (f = g.f0; f <= g.f1 && !F->err; f++)
				for (c = g.c0; c <= g.c1 && !F->err; c++)
					cada_flujo(F, ev_celda(x, a[i].h, f, c), f, c);
		} else if (!falta(&a[i])) {
			double v;
			int e = ev_a_num(x, &a[i], &v);

			if (e)
				F->err = e;
			else
				cada_flujo(F, &(valor){ V_NUM, 0, v, NULL }, 0, 0);
		}
	}
}

/* ---- el azar ---- */
static u32 azar_estado;
static double azar(void)
{
	u32 x = azar_estado ? azar_estado : (so_tics() * 2654435761u) | 1;

	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	azar_estado = x;
	return (x >> 8) / 16777216.0;
}

static double fact(int n)
{
	double r = 1;
	int i;

	for (i = 2; i <= n; i++)
		r *= i;
	return r;
}

static double mcd(double a, double b)
{
	while (b > 0.5) {
		double t = a - b * so_suelo(a / b);

		a = b;
		b = t;
	}
	return a;
}

/* ===================================================================== */
void fn_llama(ctx *x, int id, ev *a, int n, ev *r)
{
	int i;
	const cu_funcion *fd = &CU_FUNCIONES[id];

	memset(r, 0, sizeof *r);
	/* los errores de los argumentos sueltos pasan (salvo en las que los miran) */
	if (!(fd->opc & FN_SIN_ERR))
		for (i = 0; i < n; i++)
			if (a[i].t == V_ERR) {
				ev_err(r, a[i].e);
				return;
			}
	switch (id) {
	/* ---- las de muchos números ---- */
	case FN_SUMA: case FN_PROMEDIO: case FN_CONTAR: case FN_CONTARA: case FN_MAX: case FN_MIN: case FN_PRODUCTO:
	case FN_MEDIANA: case FN_MODA: case FN_VAR: case FN_VARP: case FN_DESVEST: case FN_DESVESTP: case FN_SUMA_CUADRADOS:
	case FN_PROMEDIOA: case FN_MAXA: case FN_MINA: {
		lista L;
		u32 k;

		memset(&L, 0, sizeof L);
		L.x = x;
		L.guarda = (id == FN_SUMA || id == FN_PROMEDIO || id == FN_CONTAR || id == FN_CONTARA || id == FN_PROMEDIOA) ? 0 : 1;
		if (id == FN_CONTAR || id == FN_CONTARA)
			L.guarda = 2;	/* (los textos sueltos no dan error) */
		L.con_a = id == FN_PROMEDIOA || id == FN_MAXA || id == FN_MINA;
		if (id == FN_CONTAR || id == FN_CONTARA) {
			/* CONTARA cuenta lo que no está vacío; CONTAR, los números */
			u32 cuenta = 0, cuenta_a = 0;

			for (i = 0; i < n; i++) {
				if (falta(&a[i]))
					continue;
				if (a[i].t == V_REF) {
					lista M;

					memset(&M, 0, sizeof M);
					M.x = x;
					M.h = lib_hoja_id(x->l, a[i].h);
					ev_recorre(x, &a[i], 0, cada_lista, &M);
					/* (los errores de un rango cuentan en CONTARA) */
					cuenta += M.cuenta;
					cuenta_a += M.cuenta_a + (M.err ? 1 : 0);
					if (M.err) {
						/* (seguir contando tras un error: a mano) */
						u32 f, c;
						hoja *h = M.h;
						rango g = a[i].r;

						cuenta -= M.cuenta;
						cuenta_a -= M.cuenta_a + 1;
						if (h && g.f1 >= h->lim_f)
							g.f1 = h->lim_f ? h->lim_f - 1 : 0;
						if (h && g.c1 >= h->lim_c)
							g.c1 = (u16)(h->lim_c ? h->lim_c - 1 : 0);
						if (h && h->lim_f)
							for (f = g.f0; f <= g.f1; f++)
								for (c = g.c0; c <= g.c1; c++) {
									const valor *v;

									if (!hj_celda(h, f, c))
										continue;
									v = ev_celda(x, a[i].h, f, c);
									if (v->t == V_NUM)
										cuenta++;
									if (v->t != V_VACIO)
										cuenta_a++;
								}
					}
				} else {
					double v;

					if (a[i].t != V_VACIO)
						cuenta_a++;
					if (a[i].t == V_NUM || a[i].t == V_LOG || (a[i].t == V_TXT && ev_a_num(x, &a[i], &v) == 0))
						cuenta++;
				}
			}
			ev_num(r, id == FN_CONTAR ? cuenta : cuenta_a);
			return;
		}
		junta(x, a, n, &L);
		if (L.err) {
			so_suelta(L.v);
			ev_err(r, L.err);
			return;
		}
		switch (id) {
		case FN_SUMA:
			ev_num(r, L.suma);
			break;
		case FN_PROMEDIO:
		case FN_PROMEDIOA:
			if (!L.cuenta)
				ev_err(r, ER_DIV0);
			else
				ev_num(r, L.suma / L.cuenta);
			break;
		case FN_MAX:
		case FN_MIN:
		case FN_MAXA:
		case FN_MINA: {
			double m = 0;

			for (k = 0; k < L.n; k++)
				if (k == 0 || ((id == FN_MAX || id == FN_MAXA) ? L.v[k] > m : L.v[k] < m))
					m = L.v[k];
			ev_num(r, m);
			break;
		}
		case FN_PRODUCTO: {
			double p = L.n ? 1 : 0;

			for (k = 0; k < L.n; k++)
				p *= L.v[k];
			ev_num(r, p);
			break;
		}
		case FN_SUMA_CUADRADOS: {
			double s = 0;

			for (k = 0; k < L.n; k++)
				s += L.v[k] * L.v[k];
			ev_num(r, s);
			break;
		}
		case FN_MEDIANA:
			if (!L.n) {
				ev_err(r, ER_NUM);
				break;
			}
			ordena(L.v, L.n);
			ev_num(r, L.n % 2 ? L.v[L.n / 2] : (L.v[L.n / 2 - 1] + L.v[L.n / 2]) / 2);
			break;
		case FN_MODA: {
			u32 mejor = 0, j;
			double m = 0;

			for (k = 0; k < L.n; k++) {
				u32 veces = 0;

				for (j = 0; j < L.n; j++)
					if (L.v[j] == L.v[k])
						veces++;
				if (veces > mejor) {
					mejor = veces;
					m = L.v[k];
				}
				if (L.n > 3000)
					break;	/* (con miles: la primera que se repite más de las 3000 primeras) */
			}
			if (mejor < 2)
				ev_err(r, ER_NA);
			else
				ev_num(r, m);
			break;
		}
		case FN_VAR:
		case FN_DESVEST:
			if (L.n < 2) {
				ev_err(r, ER_DIV0);
				break;
			}
			ev_num(r, id == FN_VAR ? varianza(&L, 0) : so_raiz(varianza(&L, 0)));
			break;
		case FN_VARP:
		case FN_DESVESTP:
			if (L.n < 1) {
				ev_err(r, ER_DIV0);
				break;
			}
			ev_num(r, id == FN_VARP ? varianza(&L, 1) : so_raiz(varianza(&L, 1)));
			break;
		}
		so_suelta(L.v);
		return;
	}
	case FN_CONTAR_BLANCO: {
		lista M;
		u64 area;

		if (a[0].t != V_REF) {
			ev_err(r, ER_VALOR);
			return;
		}
		memset(&M, 0, sizeof M);
		M.x = x;
		M.h = lib_hoja_id(x->l, a[0].h);
		area = (u64)(a[0].r.f1 - a[0].r.f0 + 1) * (a[0].r.c1 - a[0].r.c0 + 1);
		{
			/* las que tienen algo (los textos vacíos cuentan como en blanco) */
			u32 f, c, llenas = 0;
			hoja *h = M.h;
			rango g = a[0].r;

			if (h && h->lim_f) {
				if (g.f1 >= h->lim_f)
					g.f1 = h->lim_f - 1;
				if (g.c1 >= h->lim_c)
					g.c1 = (u16)(h->lim_c - 1);
				if (g.f0 <= g.f1 && g.c0 <= g.c1 && (u64)(g.f1 - g.f0 + 1) * (g.c1 - g.c0 + 1) <= 4u * 1024 * 1024)
					for (f = g.f0; f <= g.f1; f++)
						for (c = g.c0; c <= g.c1; c++) {
							const valor *v;

							if (!hj_celda(h, f, c))
								continue;
							v = ev_celda(x, a[0].h, f, c);
							if (v->t != V_VACIO && !(v->t == V_TXT && (!v->s || !v->s[0])))
								llenas++;
						}
			}
			ev_num(r, (double)(area - llenas));
		}
		return;
	}
	case FN_K_MAYOR:
	case FN_K_MENOR:
	case FN_JERARQUIA: {
		lista L;
		double k = 0, v0 = 0, ord = 0;

		memset(&L, 0, sizeof L);
		L.x = x;
		L.guarda = 1;
		if (id == FN_JERARQUIA) {
			NUM(0, 0, v0);
			NUM(2, 0, ord);
			junta(x, &a[1], 1, &L);
		} else {
			NUM(1, 0, k);
			junta(x, &a[0], 1, &L);
		}
		if (L.err) {
			so_suelta(L.v);
			ev_err(r, L.err);
			return;
		}
		if (id == FN_JERARQUIA) {
			u32 j, pos = 1, esta = 0;

			for (j = 0; j < L.n; j++) {
				if (L.v[j] == v0)
					esta = 1;
				else if (ord == 0 ? L.v[j] > v0 : L.v[j] < v0)
					pos++;
			}
			if (esta)
				ev_num(r, pos);
			else
				ev_err(r, ER_NA);
		} else {
			u32 kk = (u32)k;

			if (k < 1 || kk > L.n)
				ev_err(r, ER_NUM);
			else {
				ordena(L.v, L.n);
				ev_num(r, id == FN_K_MAYOR ? L.v[L.n - kk] : L.v[kk - 1]);
			}
		}
		so_suelta(L.v);
		return;
	}
	/* ---- redondear y demás ---- */
	case FN_REDONDEAR: {
		double v, d;

		NUM(0, 0, v);
		NUM(1, 0, d);
		ev_num(r, num_redondea(v, (int)(d < 0 ? -so_techo(-d) : so_suelo(d))));
		return;
	}
	case FN_REDONDEAR_MAS:
	case FN_REDONDEAR_MENOS:
	case FN_TRUNCAR: {
		double v, d, p, y;

		NUM(0, 0, v);
		NUM(1, 0, d);
		d = d < 0 ? -so_suelo(-d) : so_suelo(d);
		if (d > 15)
			d = 15;
		if (d < -15)
			d = -15;
		p = cu_pot(10, d);
		y = num_redondea(so_abs(v) * p, 9);
		y = id == FN_REDONDEAR_MAS ? so_techo(y) : so_suelo(y);
		y /= p;
		ev_num(r, v < 0 ? -y : y);
		return;
	}
	case FN_ENTERO: {
		double v;

		NUM(0, 0, v);
		ev_num(r, so_suelo(v));
		return;
	}
	case FN_ABS: {
		double v;

		NUM(0, 0, v);
		ev_num(r, so_abs(v));
		return;
	}
	case FN_RAIZ: {
		double v;

		NUM(0, 0, v);
		if (v < 0)
			ev_err(r, ER_NUM);
		else
			ev_num(r, so_raiz(v));
		return;
	}
	case FN_POTENCIA: {
		ev b0, b1;

		b0 = a[0];
		b1 = a[1];
		{
			double p, q;

			NUM(0, 0, p);
			NUM(1, 0, q);
			(void)b0;
			(void)b1;
			if (p == 0 && q <= 0) {
				ev_err(r, q == 0 ? ER_NUM : ER_DIV0);
				return;
			}
			if (p < 0 && q != so_suelo(q)) {
				ev_err(r, ER_NUM);
				return;
			}
			ev_num(r, cu_pot(p, q));
		}
		return;
	}
	case FN_RESIDUO: {
		double p, q;

		NUM(0, 0, p);
		NUM(1, 0, q);
		if (q == 0) {
			ev_err(r, ER_DIV0);
			return;
		}
		{
			double m = p - q * so_suelo(p / q);

			/* (que no salga 0,999999 de más) */
			if (so_abs(m - q) < so_abs(q) * 1e-14)
				m = 0;
			ev_num(r, m);
		}
		return;
	}
	case FN_COCIENTE: {
		double p, q, d;

		NUM(0, 0, p);
		NUM(1, 0, q);
		if (q == 0) {
			ev_err(r, ER_DIV0);
			return;
		}
		d = p / q;
		ev_num(r, d < 0 ? -so_suelo(-d) : so_suelo(d));
		return;
	}
	case FN_SIGNO: {
		double v;

		NUM(0, 0, v);
		ev_num(r, v > 0 ? 1 : v < 0 ? -1 : 0);
		return;
	}
	case FN_PI:
		ev_num(r, CU_PI);
		return;
	case FN_ALEATORIO:
		ev_num(r, azar());
		return;
	case FN_ALEATORIO_ENTRE: {
		double p, q;

		NUM(0, 0, p);
		NUM(1, 0, q);
		p = so_techo(p);
		q = so_suelo(q);
		if (p > q) {
			ev_err(r, ER_NUM);
			return;
		}
		ev_num(r, p + so_suelo(azar() * (q - p + 1)));
		return;
	}
	case FN_EXP: {
		double v;

		NUM(0, 0, v);
		ev_num(r, cu_exp(v));
		return;
	}
	case FN_LN:
	case FN_LOG10:
	case FN_LOG: {
		double v, b = 10;

		NUM(0, 0, v);
		if (id == FN_LOG)
			NUM(1, 10, b);
		if (v <= 0 || (id == FN_LOG && (b <= 0 || b == 1))) {
			ev_err(r, b == 1 ? ER_DIV0 : ER_NUM);
			return;
		}
		ev_num(r, id == FN_LN ? cu_ln(v) : id == FN_LOG10 ? cu_log10(v) : cu_ln(v) / cu_ln(b));
		return;
	}
	case FN_SENO: case FN_COS: case FN_TAN: case FN_ASENO: case FN_ACOS: case FN_ATAN: case FN_GRADOS: case FN_RADIANES: {
		double v;

		NUM(0, 0, v);
		switch (id) {
		case FN_SENO: ev_num(r, cu_sen(v)); break;
		case FN_COS: ev_num(r, cu_cos(v)); break;
		case FN_TAN:
			if (cu_cos(v) == 0)
				ev_err(r, ER_DIV0);
			else
				ev_num(r, cu_sen(v) / cu_cos(v));
			break;
		case FN_ASENO:
			if (v < -1 || v > 1)
				ev_err(r, ER_NUM);
			else
				ev_num(r, cu_atan2(v, so_raiz(1 - v * v)));
			break;
		case FN_ACOS:
			if (v < -1 || v > 1)
				ev_err(r, ER_NUM);
			else
				ev_num(r, cu_atan2(so_raiz(1 - v * v), v));
			break;
		case FN_ATAN: ev_num(r, cu_atan2(v, 1)); break;
		case FN_GRADOS: ev_num(r, v * 180 / CU_PI); break;
		default: ev_num(r, v * CU_PI / 180); break;
		}
		return;
	}
	case FN_ATAN2: {
		double p, q;

		NUM(0, 0, p);
		NUM(1, 0, q);
		if (p == 0 && q == 0) {
			ev_err(r, ER_DIV0);
			return;
		}
		ev_num(r, cu_atan2(q, p));
		return;
	}
	case FN_FACT: {
		double v;

		NUM(0, 0, v);
		if (v < 0 || v > 170) {
			ev_err(r, ER_NUM);
			return;
		}
		ev_num(r, fact((int)v));
		return;
	}
	case FN_COMBINAT: {
		double p, q, c = 1;
		int k;

		NUM(0, 0, p);
		NUM(1, 0, q);
		p = so_suelo(p);
		q = so_suelo(q);
		if (p < 0 || q < 0 || q > p || p > 1e9) {
			ev_err(r, ER_NUM);
			return;
		}
		if (q > p - q)
			q = p - q;
		for (k = 1; k <= (int)q; k++)
			c = c * (p - q + k) / k;
		ev_num(r, so_suelo(c + 0.5));
		return;
	}
	case FN_MULTIPLO_SUPERIOR:
	case FN_MULTIPLO_INFERIOR:
	case FN_REDOND_MULT: {
		double v, m, q;

		NUM(0, 0, v);
		NUM(1, 1, m);
		if (m == 0) {
			ev_num(r, 0);
			return;
		}
		if ((v > 0 && m < 0) || (id == FN_REDOND_MULT && v < 0 && m > 0)) {
			ev_err(r, ER_NUM);
			return;
		}
		q = num_redondea(v / m, 9);
		q = id == FN_MULTIPLO_SUPERIOR ? (q >= 0 ? so_techo(q) : -so_techo(-q)) : id == FN_MULTIPLO_INFERIOR ? (q >= 0 ? so_suelo(q) : -so_suelo(-q)) : so_suelo(q + 0.5);
		if (v < 0 && m < 0 && id != FN_REDOND_MULT)
			q = id == FN_MULTIPLO_SUPERIOR ? so_techo(num_redondea(v / m, 9)) : so_suelo(num_redondea(v / m, 9));
		ev_num(r, num_redondea(q * m, 12));
		return;
	}
	case FN_PAR:
	case FN_IMPAR: {
		double v, k;

		NUM(0, 0, v);
		k = so_techo(so_abs(v));
		if (id == FN_PAR) {
			if ((i64)k % 2)
				k += 1;
		} else {
			if (!((i64)k % 2))
				k += 1;
		}
		ev_num(r, v < 0 ? -k : k);
		return;
	}
	case FN_MCD:
	case FN_MCM: {
		lista L;
		double g = 0;
		u32 k;

		memset(&L, 0, sizeof L);
		L.x = x;
		L.guarda = 1;
		junta(x, a, n, &L);
		if (L.err) {
			so_suelta(L.v);
			ev_err(r, L.err);
			return;
		}
		for (k = 0; k < L.n; k++) {
			double v = so_suelo(L.v[k]);

			if (v < 0) {
				so_suelta(L.v);
				ev_err(r, ER_NUM);
				return;
			}
			if (id == FN_MCD)
				g = k ? mcd(g, v) : v;
			else
				g = k ? (v == 0 || g == 0 ? 0 : g / mcd(g, v) * v) : v;
		}
		so_suelta(L.v);
		ev_num(r, g);
		return;
	}
	case FN_SUMAPRODUCTO: {
		u32 alto, ancho, f, c;
		double s = 0;

		for (i = 0; i < n; i++)
			if (a[i].t != V_REF) {
				/* (números sueltos: una matriz de 1 x 1) */
				if (n == 1) {
					double v;

					NUM(0, 0, v);
					ev_num(r, v);
					return;
				}
				ev_err(r, ER_VALOR);
				return;
			}
		alto = a[0].r.f1 - a[0].r.f0 + 1;
		ancho = a[0].r.c1 - a[0].r.c0 + 1;
		for (i = 1; i < n; i++)
			if (a[i].r.f1 - a[i].r.f0 + 1 != alto || a[i].r.c1 - a[i].r.c0 + 1 != ancho) {
				ev_err(r, ER_VALOR);
				return;
			}
		{
			hoja *h = lib_hoja_id(x->l, a[0].h);

			if (h && alto > (h->lim_f > a[0].r.f0 ? h->lim_f - a[0].r.f0 : 0))
				alto = h->lim_f > a[0].r.f0 ? h->lim_f - a[0].r.f0 : 0;
		}
		if ((u64)alto * ancho > 4u * 1024 * 1024) {
			ev_err(r, ER_VALOR);
			return;
		}
		for (f = 0; f < alto; f++)
			for (c = 0; c < ancho; c++) {
				double p = 1;

				for (i = 0; i < n; i++) {
					const valor *v = en_rango(x, &a[i], f, c);

					if (v->t == V_ERR) {
						ev_err(r, v->e);
						return;
					}
					p *= v->t == V_NUM ? v->n : 0;
				}
				s += p;
			}
		ev_num(r, s);
		return;
	}
	case FN_SUMAR_SI:
		sumar_si(x, a, n, r, 0);
		return;
	case FN_CONTAR_SI:
		sumar_si(x, a, n, r, 1);
		return;
	case FN_PROMEDIO_SI:
		sumar_si(x, a, n, r, 2);
		return;
	case FN_SUMAR_SI_CONJ:
		si_conjunto(x, a, n, r, 0);
		return;
	case FN_CONTAR_SI_CONJ:
		si_conjunto(x, a, n, r, 1);
		return;
	case FN_SUBTOTALES: {
		double q;
		int k, sin_oc;
		static const int que[] = { 0, FN_PROMEDIO, FN_CONTAR, FN_CONTARA, FN_MAX, FN_MIN, FN_PRODUCTO, FN_DESVEST, FN_DESVESTP, FN_SUMA, FN_VAR, FN_VARP };

		NUM(0, 0, q);
		k = (int)q;
		sin_oc = k > 100;
		if (sin_oc)
			k -= 100;
		if (k < 1 || k > 11) {
			ev_err(r, ER_VALOR);
			return;
		}
		{
			lista L;
			u32 j;

			memset(&L, 0, sizeof L);
			L.x = x;
			L.guarda = 1;
			L.sin_ocultas = sin_oc || 1;	/* (como en Excel 5: lo filtrado no cuenta) */
			L.sin_subtotales = 1;
			for (i = 1; i < n; i++) {
				if (a[i].t != V_REF)
					continue;
				L.h = lib_hoja_id(x->l, a[i].h);
				L.sin_ocultas = sin_oc || (L.h && L.h->hay_filtro);
				ev_recorre(x, &a[i], 0, cada_lista, &L);
			}
			if (L.err) {
				so_suelta(L.v);
				ev_err(r, L.err);
				return;
			}
			switch (que[k]) {
			case FN_SUMA: ev_num(r, L.suma); break;
			case FN_PROMEDIO:
				if (!L.cuenta)
					ev_err(r, ER_DIV0);
				else
					ev_num(r, L.suma / L.cuenta);
				break;
			case FN_CONTAR: ev_num(r, L.cuenta); break;
			case FN_CONTARA: ev_num(r, L.cuenta_a); break;
			case FN_MAX:
			case FN_MIN: {
				double m = 0;

				for (j = 0; j < L.n; j++)
					if (!j || (que[k] == FN_MAX ? L.v[j] > m : L.v[j] < m))
						m = L.v[j];
				ev_num(r, m);
				break;
			}
			case FN_PRODUCTO: {
				double p = L.n ? 1 : 0;

				for (j = 0; j < L.n; j++)
					p *= L.v[j];
				ev_num(r, p);
				break;
			}
			default:
				if (L.n < 2 && (que[k] == FN_VAR || que[k] == FN_DESVEST))
					ev_err(r, ER_DIV0);
				else if (L.n < 1)
					ev_err(r, ER_DIV0);
				else {
					double v = varianza(&L, que[k] == FN_VARP || que[k] == FN_DESVESTP);

					ev_num(r, que[k] == FN_DESVEST || que[k] == FN_DESVESTP ? so_raiz(v) : v);
				}
			}
			so_suelta(L.v);
		}
		return;
	}
	case FN_ROMANO: {
		double v;
		char t[40];

		NUM(0, 0, v);
		if (v < 0 || v > 3999) {
			ev_err(r, ER_VALOR);
			return;
		}
		romano((int)v, t);
		ev_txt(x, r, t, largo(t));
		return;
	}
	/* ---- lógicas ---- */
	case FN_SI: {
		int c;
		int e = ev_a_log(x, &a[0], &c);

		if (e) {
			ev_err(r, e);
			return;
		}
		if (c) {
			if (falta(&a[1]))
				ev_num(r, 0);
			else
				*r = a[1];
		} else if (n < 3)
			ev_log(r, 0);
		else if (falta(&a[2]))
			ev_num(r, 0);
		else
			*r = a[2];
		return;
	}
	case FN_Y:
	case FN_O: {
		int hay = 0, todo = 1, alguno = 0;

		for (i = 0; i < n; i++) {
			if (falta(&a[i]))
				continue;
			if (a[i].t == V_REF) {
				u32 f, c;
				hoja *h = lib_hoja_id(x->l, a[i].h);
				rango g = a[i].r;

				if (!h || !h->lim_f)
					continue;
				if (g.f1 >= h->lim_f)
					g.f1 = h->lim_f - 1;
				if (g.c1 >= h->lim_c)
					g.c1 = (u16)(h->lim_c - 1);
				for (f = g.f0; f <= g.f1; f++)
					for (c = g.c0; c <= g.c1; c++) {
						const valor *v;

						if (!hj_celda(h, f, c))
							continue;
						v = ev_celda(x, a[i].h, f, c);
						if (v->t == V_ERR) {
							ev_err(r, v->e);
							return;
						}
						if (v->t == V_NUM || v->t == V_LOG) {
							hay = 1;
							if (v->n != 0)
								alguno = 1;
							else
								todo = 0;
						}
					}
			} else {
				int b;
				int e = ev_a_log(x, &a[i], &b);

				if (e) {
					ev_err(r, e);
					return;
				}
				hay = 1;
				if (b)
					alguno = 1;
				else
					todo = 0;
			}
		}
		if (!hay)
			ev_err(r, ER_VALOR);
		else
			ev_log(r, id == FN_Y ? todo : alguno);
		return;
	}
	case FN_NO: {
		int b;
		int e = ev_a_log(x, &a[0], &b);

		if (e)
			ev_err(r, e);
		else
			ev_log(r, !b);
		return;
	}
	case FN_VERDADERO:
		ev_log(r, 1);
		return;
	case FN_FALSO:
		ev_log(r, 0);
		return;
	case FN_SI_ERROR: {
		ev v;

		ev_valor(x, &a[0], &v);
		if (v.t == V_ERR)
			ev_valor(x, &a[1], r);
		else
			*r = v;
		if (r->t == V_VACIO)
			ev_num(r, 0);
		return;
	}
	case FN_ESERROR: case FN_ESERR: case FN_ESNOD: case FN_ESNUMERO: case FN_ESTEXTO: case FN_ESNOTEXTO: case FN_ESBLANCO:
	case FN_ESLOGICO: case FN_ESREF: case FN_TIPO: case FN_TIPO_ERROR: {
		ev v;

		if (id == FN_ESREF) {
			ev_log(r, a[0].t == V_REF);
			return;
		}
		ev_valor(x, &a[0], &v);
		switch (id) {
		case FN_ESERROR: ev_log(r, v.t == V_ERR); break;
		case FN_ESERR: ev_log(r, v.t == V_ERR && v.e != ER_NA); break;
		case FN_ESNOD: ev_log(r, v.t == V_ERR && v.e == ER_NA); break;
		case FN_ESNUMERO: ev_log(r, v.t == V_NUM); break;
		case FN_ESTEXTO: ev_log(r, v.t == V_TXT); break;
		case FN_ESNOTEXTO: ev_log(r, v.t != V_TXT); break;
		case FN_ESBLANCO: ev_log(r, v.t == V_VACIO); break;
		case FN_ESLOGICO: ev_log(r, v.t == V_LOG); break;
		case FN_TIPO: ev_num(r, v.t == V_NUM || v.t == V_VACIO ? 1 : v.t == V_TXT ? 2 : v.t == V_LOG ? 4 : 16); break;
		default:
			if (v.t != V_ERR)
				ev_err(r, ER_NA);
			else {
				static const int k[ER_N] = { 0, 1, 2, 3, 4, 5, 6, 7, 3 };

				ev_num(r, k[v.e < ER_N ? v.e : ER_VALOR]);
			}
		}
		return;
	}
	case FN_ESPAR:
	case FN_ESIMPAR: {
		double v;
		i64 k;

		NUM(0, 0, v);
		k = (i64)so_suelo(so_abs(v));
		ev_log(r, id == FN_ESPAR ? !(k % 2) : (int)(k % 2));
		return;
	}
	case FN_NOD:
		ev_err(r, ER_NA);
		return;
	case FN_N: {
		ev v;

		ev_valor(x, &a[0], &v);
		ev_num(r, v.t == V_NUM || v.t == V_LOG ? v.n : 0);
		return;
	}
	/* ---- buscar ---- */
	case FN_BUSCARV:
		buscarv(x, a, n, r, 0);
		return;
	case FN_BUSCARH:
		buscarv(x, a, n, r, 1);
		return;
	case FN_BUSCAR: {
		ev que;
		i32 p;
		const ev *b = &a[1], *c = n >= 3 ? &a[2] : &a[1];
		u32 alto, ancho;
		int por_f;

		ev_valor(x, &a[0], &que);
		if (b->t != V_REF || c->t != V_REF) {
			ev_err(r, ER_NA);
			return;
		}
		alto = b->r.f1 - b->r.f0 + 1;
		ancho = b->r.c1 - b->r.c0 + 1;
		por_f = alto >= ancho;
		p = busca_vector(x, &que, b->h, b->r.f0, b->r.c0, por_f ? alto : ancho, por_f, 1);
		if (p < 0) {
			ev_err(r, ER_NA);
			return;
		}
		memset(r, 0, sizeof *r);
		r->t = V_REF;
		r->h = c->h;
		if (n >= 3) {
			int cf = (c->r.f1 - c->r.f0 + 1) >= (u32)(c->r.c1 - c->r.c0 + 1);

			r->r.f0 = r->r.f1 = cf ? c->r.f0 + (u32)p : c->r.f0;
			r->r.c0 = r->r.c1 = (u16)(cf ? c->r.c0 : c->r.c0 + (u32)p);
		} else {
			/* (la última fila o columna del rango) */
			r->r.f0 = r->r.f1 = por_f ? b->r.f0 + (u32)p : b->r.f1;
			r->r.c0 = r->r.c1 = (u16)(por_f ? b->r.c1 : b->r.c0 + (u32)p);
		}
		ev_valor(x, r, r);
		if (r->t == V_VACIO)
			ev_num(r, 0);
		return;
	}
	case FN_COINCIDIR: {
		ev que;
		double t;
		i32 p;
		u32 alto, ancho;

		ev_valor(x, &a[0], &que);
		NUM(2, 1, t);
		if (a[1].t != V_REF) {
			ev_err(r, ER_NA);
			return;
		}
		alto = a[1].r.f1 - a[1].r.f0 + 1;
		ancho = a[1].r.c1 - a[1].r.c0 + 1;
		if (alto > 1 && ancho > 1) {
			ev_err(r, ER_NA);
			return;
		}
		p = busca_vector(x, &que, a[1].h, a[1].r.f0, a[1].r.c0, alto > 1 ? alto : ancho, alto > 1, t > 0 ? 1 : t < 0 ? -1 : 0);
		if (p < 0)
			ev_err(r, ER_NA);
		else
			ev_num(r, p + 1);
		return;
	}
	case FN_INDICE: {
		double fi, co = 0;
		u32 alto, ancho;

		if (a[0].t != V_REF) {
			/* (un valor suelto: INDICE(5; 1) es 5) */
			NUM(1, 0, fi);
			if (fi > 1) {
				ev_err(r, ER_REF);
				return;
			}
			*r = a[0];
			return;
		}
		NUM(1, 0, fi);
		NUM(2, 0, co);
		alto = a[0].r.f1 - a[0].r.f0 + 1;
		ancho = a[0].r.c1 - a[0].r.c0 + 1;
		if (n < 3 && alto == 1 && ancho > 1) {
			co = fi;
			fi = 1;
		}
		if (fi < 0 || co < 0 || (u32)fi > alto || (u32)co > ancho) {
			ev_err(r, ER_REF);
			return;
		}
		*r = a[0];
		if (fi >= 1)
			r->r.f0 = r->r.f1 = a[0].r.f0 + (u32)fi - 1;
		if (co >= 1)
			r->r.c0 = r->r.c1 = (u16)(a[0].r.c0 + (u32)co - 1);
		else if (ancho == 1)
			r->r.c0 = r->r.c1 = a[0].r.c0;
		return;
	}
	case FN_ELEGIR: {
		double k;

		NUM(0, 0, k);
		if (k < 1 || (int)k >= n) {
			ev_err(r, ER_VALOR);
			return;
		}
		*r = a[(int)k];
		if (falta(r))
			ev_num(r, 0);
		return;
	}
	case FN_FILA:
	case FN_COLUMNA:
		if (n == 0 || falta(&a[0]))
			ev_num(r, (id == FN_FILA ? x->f : x->c) + 1);
		else if (a[0].t != V_REF)
			ev_err(r, ER_VALOR);
		else
			ev_num(r, (id == FN_FILA ? a[0].r.f0 : a[0].r.c0) + 1);
		return;
	case FN_FILAS:
	case FN_COLUMNAS:
		if (a[0].t != V_REF) {
			ev_num(r, 1);
			return;
		}
		ev_num(r, id == FN_FILAS ? a[0].r.f1 - a[0].r.f0 + 1 : (u32)a[0].r.c1 - a[0].r.c0 + 1);
		return;
	case FN_INDIRECTO: {
		const char *s;
		u32 sn;
		char t[300];
		const char *p, *fin;
		hoja *h = x->h;
		rango g;

		TXT(0, s, sn);
		copia(t, s, sn + 1 < sizeof t ? sn + 1 : sizeof t);
		p = t;
		/* Hoja!A1 o 'Mi hoja'!A1:B2 */
		{
			char *ex = strchr(t, '!');

			if (ex) {
				char *a0 = t, *b0 = ex;

				if (*a0 == '\'' && b0 > a0 + 1 && b0[-1] == '\'') {
					a0++;
					b0--;
				}
				h = lib_hoja_nombre(x->l, a0, (u32)(b0 - a0));
				p = ex + 1;
			}
		}
		{
			/* sin los «$» */
			char u[300];
			u32 k = 0;

			for (; *p && k + 1 < sizeof u; p++)
				if (*p != '$')
					u[k++] = *p;
			u[k] = 0;
			if (!h || !cu_lee_rango(u, &g, &fin) || *fin) {
				/* (un nombre definido) */
				int nn = lib_nombre(x->l, u, largo(u));

				if (nn >= 0 && x->l->nom[nn].f && x->l->nom[nn].f->n == 1 &&
				    (x->l->nom[nn].f->t[0].t == T_REF || x->l->nom[nn].f->t[0].t == T_RANGO)) {
					const cu_tok *tk = &x->l->nom[nn].f->t[0];

					memset(r, 0, sizeof *r);
					r->t = V_REF;
					r->h = tk->h;
					r->r = cu_rango((u32)tk->u.r.f0, tk->u.r.c0, (u32)tk->u.r.f1, tk->u.r.c1);
					return;
				}
				ev_err(r, ER_REF);
				return;
			}
		}
		memset(r, 0, sizeof *r);
		r->t = V_REF;
		r->h = h->id;
		r->r = g;
		return;
	}
	case FN_DESREF: {
		double df, dc, al, an;
		i64 f0, c0;

		if (a[0].t != V_REF) {
			ev_err(r, ER_VALOR);
			return;
		}
		NUM(1, 0, df);
		NUM(2, 0, dc);
		NUM(3, a[0].r.f1 - a[0].r.f0 + 1, al);
		NUM(4, a[0].r.c1 - a[0].r.c0 + 1, an);
		f0 = (i64)a[0].r.f0 + (i64)df;
		c0 = (i64)a[0].r.c0 + (i64)dc;
		if (al < 1 || an < 1 || f0 < 0 || c0 < 0 || f0 + (i64)al > CU_FILAS || c0 + (i64)an > CU_COLS) {
			ev_err(r, ER_REF);
			return;
		}
		*r = a[0];
		r->r.f0 = (u32)f0;
		r->r.f1 = (u32)(f0 + (i64)al - 1);
		r->r.c0 = (u16)c0;
		r->r.c1 = (u16)(c0 + (i64)an - 1);
		return;
	}
	case FN_DIRECCION: {
		double fi, co, ab;
		char t[160], c[8];
		int a1 = 1;

		NUM(0, 0, fi);
		NUM(1, 0, co);
		NUM(2, 1, ab);
		if (n >= 4 && !falta(&a[3]))
			ev_a_log(x, &a[3], &a1);
		if (fi < 1 || co < 1 || fi > CU_FILAS || co > CU_COLS) {
			ev_err(r, ER_VALOR);
			return;
		}
		t[0] = 0;
		if (n >= 5 && !falta(&a[4])) {
			const char *s;
			u32 sn;

			TXT(4, s, sn);
			copia(t, s, sn + 1 < 120 ? sn + 1 : 120);
			anade(t, "!", sizeof t);
		}
		if (!a1) {
			int k = (int)ab;

			fmt(t + largo(t), sizeof t - largo(t), k == 1 ? "F%uC%u" : k == 2 ? "F%uC[%u]" : k == 3 ? "F[%u]C%u" : "F[%u]C[%u]", (u32)fi, (u32)co);
		} else {
			int k = (int)ab;

			cu_col_texto((u32)co - 1, c);
			fmt(t + largo(t), sizeof t - largo(t), "%s%s%s%u", k == 1 || k == 3 ? "$" : "", c, k == 1 || k == 2 ? "$" : "", (u32)fi);
		}
		ev_txt(x, r, t, largo(t));
		return;
	}
	/* ---- el texto ---- */
	case FN_CONCATENAR: {
		bufer b = { 0 };

		for (i = 0; i < n; i++) {
			const char *s;
			u32 sn;
			int e;

			if (a[i].t == V_REF && (a[i].r.f0 != a[i].r.f1 || a[i].r.c0 != a[i].r.c1)) {
				/* (un rango entero: sus celdas, por orden) */
				u32 f, c;
				hoja *h = lib_hoja_id(x->l, a[i].h);
				rango g = a[i].r;

				if (h && g.f1 >= h->lim_f)
					g.f1 = h->lim_f ? h->lim_f - 1 : 0;
				if (h && g.c1 >= h->lim_c)
					g.c1 = (u16)(h->lim_c ? h->lim_c - 1 : 0);
				if (h && h->lim_f)
					for (f = g.f0; f <= g.f1; f++)
						for (c = g.c0; c <= g.c1; c++) {
							ev q, w;

							memset(&q, 0, sizeof q);
							q.t = V_REF;
							q.h = a[i].h;
							q.r = cu_rango(f, c, f, c);
							ev_valor(x, &q, &w);
							e = ev_a_txt(x, &w, &s, &sn);
							if (e) {
								buf_suelta(&b);
								ev_err(r, e);
								return;
							}
							buf_pon(&b, s, sn);
						}
				continue;
			}
			e = ev_a_txt(x, &a[i], &s, &sn);
			if (e) {
				buf_suelta(&b);
				ev_err(r, e);
				return;
			}
			buf_pon(&b, s, sn);
			if (b.n > 32767)
				break;
		}
		ev_txt(x, r, b.d ? (char *)b.d : "", b.n);
		buf_suelta(&b);
		return;
	}
	case FN_IZQUIERDA:
	case FN_DERECHA: {
		const char *s;
		u32 sn, nl;
		double k;

		TXT(0, s, sn);
		NUM(1, 1, k);
		if (k < 0) {
			ev_err(r, ER_VALOR);
			return;
		}
		nl = letras(s, sn);
		if ((u32)k >= nl) {
			ev_txt(x, r, s, sn);
			return;
		}
		if (id == FN_IZQUIERDA)
			ev_txt(x, r, s, octeto(s, sn, (u32)k));
		else {
			u32 o = octeto(s, sn, nl - (u32)k);

			ev_txt(x, r, s + o, sn - o);
		}
		return;
	}
	case FN_EXTRAE: {
		const char *s;
		u32 sn, o, e2;
		double d, k;

		TXT(0, s, sn);
		NUM(1, 1, d);
		NUM(2, 0, k);
		if (d < 1 || k < 0) {
			ev_err(r, ER_VALOR);
			return;
		}
		o = octeto(s, sn, (u32)d - 1);
		e2 = octeto(s, sn, (u32)d - 1 + (u32)(k > 100000 ? 100000 : k));
		ev_txt(x, r, s + o, e2 - o);
		return;
	}
	case FN_LARGO: {
		const char *s;
		u32 sn;

		TXT(0, s, sn);
		ev_num(r, letras(s, sn));
		return;
	}
	case FN_MAYUSC:
	case FN_MINUSC:
	case FN_NOMPROPIO: {
		const char *s;
		u32 sn;

		TXT(0, s, sn);
		cambia_caja(x, s, sn, r, id == FN_MAYUSC ? 0 : id == FN_MINUSC ? 1 : 2);
		return;
	}
	case FN_ESPACIOS: {
		const char *s;
		u32 sn, k;
		bufer b = { 0 };
		int esp = 0;

		TXT(0, s, sn);
		for (k = 0; k < sn; k++) {
			if (s[k] == ' ') {
				esp = 1;
				continue;
			}
			if (esp && b.n)
				buf_c(&b, ' ');
			esp = 0;
			buf_c(&b, (u8)s[k]);
		}
		ev_txt(x, r, b.d ? (char *)b.d : "", b.n);
		buf_suelta(&b);
		return;
	}
	case FN_ENCONTRAR:
	case FN_HALLAR: {
		const char *s, *q;
		u32 sn, qn;
		double d;
		i32 p;

		TXT(0, q, qn);
		TXT(1, s, sn);
		NUM(2, 1, d);
		if (d < 1 || (u32)d > letras(s, sn) + 1) {
			ev_err(r, ER_VALOR);
			return;
		}
		p = encuentra(s, sn, q, qn, (u32)d - 1, id == FN_HALLAR);
		if (p < 0)
			ev_err(r, ER_VALOR);
		else
			ev_num(r, p + 1);
		return;
	}
	case FN_SUSTITUIR: {
		const char *s, *v, *nu;
		u32 sn, vn, nn, k = 0, cual = 0;
		double ins;
		bufer b = { 0 };

		TXT(0, s, sn);
		TXT(1, v, vn);
		TXT(2, nu, nn);
		NUM(3, 0, ins);
		if (n >= 4 && ins < 1) {
			ev_err(r, ER_VALOR);
			return;
		}
		if (!vn) {
			ev_txt(x, r, s, sn);
			return;
		}
		while (k < sn) {
			if (k + vn <= sn && memcmp(s + k, v, vn) == 0) {
				cual++;
				if (ins == 0 || cual == (u32)ins) {
					buf_pon(&b, nu, nn);
					k += vn;
					continue;
				}
			}
			buf_c(&b, (u8)s[k++]);
			if (b.n > 65536)
				break;
		}
		ev_txt(x, r, b.d ? (char *)b.d : "", b.n);
		buf_suelta(&b);
		return;
	}
	case FN_REEMPLAZAR: {
		const char *s, *nu;
		u32 sn, nn, o, e2;
		double d, k;
		bufer b = { 0 };

		TXT(0, s, sn);
		NUM(1, 1, d);
		NUM(2, 0, k);
		TXT(3, nu, nn);
		if (d < 1 || k < 0) {
			ev_err(r, ER_VALOR);
			return;
		}
		o = octeto(s, sn, (u32)d - 1);
		e2 = octeto(s, sn, (u32)d - 1 + (u32)(k > 100000 ? 100000 : k));
		buf_pon(&b, s, o);
		buf_pon(&b, nu, nn);
		buf_pon(&b, s + e2, sn - e2);
		ev_txt(x, r, b.d ? (char *)b.d : "", b.n);
		buf_suelta(&b);
		return;
	}
	case FN_REPETIR: {
		const char *s;
		u32 sn, k;
		double v;
		bufer b = { 0 };

		TXT(0, s, sn);
		NUM(1, 0, v);
		if (v < 0 || v * sn > 32767) {
			ev_err(r, ER_VALOR);
			return;
		}
		for (k = 0; k < (u32)v; k++)
			buf_pon(&b, s, sn);
		ev_txt(x, r, b.d ? (char *)b.d : "", b.n);
		buf_suelta(&b);
		return;
	}
	case FN_TEXTO: {
		const char *s;
		u32 sn;
		char c[200], t[300];
		ev v;

		ev_valor(x, &a[0], &v);
		TXT(1, s, sn);
		copia(c, s, sn + 1 < sizeof c ? sn + 1 : sizeof c);
		if (v.t == V_TXT) {
			double d;

			if (ev_a_num(x, &v, &d) == 0) {
				num_texto_funcion(d, c, &x->l->loc, t, sizeof t);
				ev_txt(x, r, t, largo(t));
			} else {
				char f2[200];

				fmt_de_local(c, &x->l->loc, f2, sizeof f2);
				num_formatea_texto(v.s, f2, t, sizeof t);
				ev_txt(x, r, t, largo(t));
			}
			return;
		}
		{
			double d;
			int e = ev_a_num(x, &v, &d);

			if (e) {
				ev_err(r, e);
				return;
			}
			num_texto_funcion(d, c, &x->l->loc, t, sizeof t);
			ev_txt(x, r, t, largo(t));
		}
		return;
	}
	case FN_VALOR:
	case FN_FECHANUMERO:
	case FN_HORANUMERO: {
		double v;
		ev q;
		int e;

		ev_valor(x, &a[0], &q);
		if (q.t == V_NUM && id == FN_VALOR) {
			*r = q;
			return;
		}
		if (q.t != V_TXT && id != FN_VALOR) {
			ev_err(r, ER_VALOR);
			return;
		}
		e = ev_a_num(x, &q, &v);
		if (e) {
			ev_err(r, e);
			return;
		}
		if (id == FN_FECHANUMERO)
			v = so_suelo(v);
		else if (id == FN_HORANUMERO)
			v = v - so_suelo(v);
		ev_num(r, v);
		return;
	}
	case FN_CARACTER: {
		double v;
		char t[8];
		int k;

		NUM(0, 0, v);
		if (v < 1 || v > 255) {
			ev_err(r, ER_VALOR);
			return;
		}
		k = utf8_pon(de_1252((u32)v), t);
		ev_txt(x, r, t, (u32)k);
		return;
	}
	case FN_CODIGO: {
		const char *s;
		u32 sn, c, k;

		TXT(0, s, sn);
		if (!sn) {
			ev_err(r, ER_VALOR);
			return;
		}
		utf8_lee((const u8 *)s, sn, &c);
		if (c > 255) {
			for (k = 0x80; k < 0xA0; k++)
				if (de_1252(k) == c) {
					c = k;
					break;
				}
			if (c > 255)
				c = '?';
		}
		ev_num(r, c);
		return;
	}
	case FN_IGUAL: {
		const char *s, *q;
		u32 sn, qn;

		TXT(0, s, sn);
		TXT(1, q, qn);
		ev_log(r, sn == qn && memcmp(s, q, sn) == 0);
		return;
	}
	case FN_DECIMAL:
	case FN_MONEDA: {
		double v, d;
		int sin_miles = 0;
		char c[64], t[128];

		NUM(0, 0, v);
		NUM(1, 2, d);
		if (id == FN_DECIMAL && n >= 3 && !falta(&a[2]))
			ev_a_log(x, &a[2], &sin_miles);
		if (d > 15)
			d = 15;
		if (d < 0) {
			v = num_redondea(v, (int)d);
			d = 0;
		}
		copia(c, sin_miles ? "0" : "#,##0", sizeof c);
		if (d >= 1) {
			int k;

			anade(c, ".", sizeof c);
			for (k = 0; k < (int)d; k++)
				anade(c, "0", sizeof c);
		}
		if (id == FN_MONEDA) {
			anade(c, " ", sizeof c);
			anade(c, x->l->moneda, sizeof c);
		}
		num_formatea(v, c, &x->l->loc, t, sizeof t, NULL);
		ev_txt(x, r, t, largo(t));
		return;
	}
	case FN_T: {
		ev v;

		ev_valor(x, &a[0], &v);
		if (v.t == V_TXT)
			*r = v;
		else
			ev_txt(x, r, "", 0);
		return;
	}
	case FN_LIMPIAR: {
		const char *s;
		u32 sn, k;
		bufer b = { 0 };

		TXT(0, s, sn);
		for (k = 0; k < sn; k++)
			if ((u8)s[k] >= 32)
				buf_c(&b, (u8)s[k]);
		ev_txt(x, r, b.d ? (char *)b.d : "", b.n);
		buf_suelta(&b);
		return;
	}
	/* ---- las fechas ---- */
	case FN_HOY:
	case FN_AHORA: {
		int ano, mes, dia, h, mi, se;

		so_ahora(&ano, &mes, &dia, &h, &mi, &se);
		ev_num(r, fecha_serie(ano, mes, dia) + (id == FN_AHORA ? (h * 3600.0 + mi * 60.0 + se) / 86400.0 : 0));
		return;
	}
	case FN_FECHA: {
		double p, q, s;

		NUM(0, 0, p);
		NUM(1, 0, q);
		NUM(2, 0, s);
		if (p < 0 || p > 9999) {
			ev_err(r, ER_NUM);
			return;
		}
		{
			double v = fecha_serie((int)p, (int)q, (int)s);

			if (v < 0) {
				ev_err(r, ER_NUM);
				return;
			}
			ev_num(r, v);
		}
		return;
	}
	case FN_NSHORA: {
		double h, m, s, t;

		NUM(0, 0, h);
		NUM(1, 0, m);
		NUM(2, 0, s);
		t = (so_suelo(h) * 3600 + so_suelo(m) * 60 + so_suelo(s)) / 86400;
		if (t < 0) {
			ev_err(r, ER_NUM);
			return;
		}
		ev_num(r, t - so_suelo(t));
		return;
	}
	case FN_ANO: case FN_MES: case FN_DIA: case FN_HORA: case FN_MINUTO: case FN_SEGUNDO: {
		double v;
		int aa, mm, dd, h, mi, se;

		NUM(0, 0, v);
		if (v < 0) {
			ev_err(r, ER_NUM);
			return;
		}
		serie_fecha(v, &aa, &mm, &dd);
		serie_hora(v, &h, &mi, &se);
		ev_num(r, id == FN_ANO ? aa : id == FN_MES ? mm : id == FN_DIA ? dd : id == FN_HORA ? h : id == FN_MINUTO ? mi : se);
		return;
	}
	case FN_DIASEM: {
		double v, t;
		int d;

		NUM(0, 0, v);
		NUM(1, 1, t);
		d = dia_semana(v);	/* 0 domingo */
		if (t == 2)
			ev_num(r, d == 0 ? 7 : d);
		else if (t == 3)
			ev_num(r, d == 0 ? 6 : d - 1);
		else if (t == 1)
			ev_num(r, d + 1);
		else
			ev_err(r, ER_NUM);
		return;
	}
	case FN_DIAS360: {
		double p, q;
		int eur = 0, a1, m1, d1, a2, m2, d2;

		NUM(0, 0, p);
		NUM(1, 0, q);
		if (n >= 3 && !falta(&a[2]))
			ev_a_log(x, &a[2], &eur);
		serie_fecha(p, &a1, &m1, &d1);
		serie_fecha(q, &a2, &m2, &d2);
		if (eur) {
			if (d1 == 31)
				d1 = 30;
			if (d2 == 31)
				d2 = 30;
		} else {
			if (d1 == 31 || (m1 == 2 && d1 == dias_mes(a1, 2)))
				d1 = 30;
			if (d2 == 31 && d1 >= 30)
				d2 = 30;
		}
		ev_num(r, (a2 - a1) * 360 + (m2 - m1) * 30 + (d2 - d1));
		return;
	}
	case FN_FIN_MES:
	case FN_FECHA_MES: {
		double p, q;
		int aa, mm, dd, m2, a2;

		NUM(0, 0, p);
		NUM(1, 0, q);
		if (p < 0) {
			ev_err(r, ER_NUM);
			return;
		}
		serie_fecha(p, &aa, &mm, &dd);
		m2 = mm + (int)(q < 0 ? -so_suelo(-q) : so_suelo(q));
		a2 = aa + (m2 - 1 >= 0 ? (m2 - 1) / 12 : -((12 - m2) / 12));
		m2 = ((m2 - 1) % 12 + 12) % 12 + 1;
		if (id == FN_FIN_MES)
			dd = dias_mes(a2, m2);
		else if (dd > dias_mes(a2, m2))
			dd = dias_mes(a2, m2);
		ev_num(r, fecha_serie(a2, m2, dd));
		return;
	}
	case FN_DIAS_LAB: {
		double p, q, s;
		fiestas F;
		i64 k = 0;
		int paso;

		NUM(0, 0, p);
		NUM(1, 0, q);
		p = so_suelo(p);
		q = so_suelo(q);
		lee_fiestas(x, a, n, 2, &F);
		paso = q >= p ? 1 : -1;
		if (so_abs(q - p) > 400000) {
			so_suelta(F.v);
			ev_err(r, ER_NUM);
			return;
		}
		for (s = p;; s += paso) {
			if (dia_lab(s) && !es_fiesta(&F, s))
				k++;
			if (s == q)
				break;
		}
		so_suelta(F.v);
		ev_num(r, (double)(paso * k));
		return;
	}
	case FN_DIA_LAB: {
		double p, q, s;
		fiestas F;
		i64 k;
		int paso;

		NUM(0, 0, p);
		NUM(1, 0, q);
		s = so_suelo(p);
		lee_fiestas(x, a, n, 2, &F);
		k = (i64)(q < 0 ? -so_suelo(-q) : so_suelo(q));
		paso = k >= 0 ? 1 : -1;
		if (k < 0)
			k = -k;
		if (k > 300000) {
			so_suelta(F.v);
			ev_err(r, ER_NUM);
			return;
		}
		while (k > 0) {
			s += paso;
			if (dia_lab(s) && !es_fiesta(&F, s))
				k--;
		}
		so_suelta(F.v);
		ev_num(r, s);
		return;
	}
	case FN_NUM_SEMANA: {
		double v, t, ene1;
		int aa, mm, dd, d1, ini;

		NUM(0, 0, v);
		NUM(1, 1, t);
		serie_fecha(v, &aa, &mm, &dd);
		ene1 = fecha_serie(aa, 1, 1);
		d1 = dia_semana(ene1);	/* 0 domingo */
		ini = t == 2 ? 1 : 0;	/* la semana empieza el lunes (2) o el domingo (1) */
		{
			int off = (d1 - ini + 7) % 7;

			ev_num(r, (double)(((i64)(so_suelo(v) - ene1) + off) / 7 + 1));
		}
		return;
	}
	case FN_SIFECHA: {
		double p, q;
		const char *u;
		u32 un;
		int a1, m1, d1, a2, m2, d2;

		NUM(0, 0, p);
		NUM(1, 0, q);
		TXT(2, u, un);
		if (q < p) {
			ev_err(r, ER_NUM);
			return;
		}
		serie_fecha(p, &a1, &m1, &d1);
		serie_fecha(q, &a2, &m2, &d2);
		if (un == 1 && (u[0] == 'Y' || u[0] == 'y' || u[0] == 'A' || u[0] == 'a')) {
			int k = a2 - a1;

			if (m2 < m1 || (m2 == m1 && d2 < d1))
				k--;
			ev_num(r, k);
		} else if (un == 1 && (u[0] == 'M' || u[0] == 'm')) {
			int k = (a2 - a1) * 12 + m2 - m1;

			if (d2 < d1)
				k--;
			ev_num(r, k);
		} else if (un == 1 && (u[0] == 'D' || u[0] == 'd'))
			ev_num(r, so_suelo(q) - so_suelo(p));
		else if (un == 2 && (u[0] == 'M' || u[0] == 'm') && (u[1] == 'D' || u[1] == 'd')) {
			int k = d2 - d1;

			if (k < 0) {
				int pm = m2 == 1 ? 12 : m2 - 1, pa = m2 == 1 ? a2 - 1 : a2;

				k += dias_mes(pa, pm);
			}
			ev_num(r, k);
		} else if (un == 2 && (u[0] == 'Y' || u[0] == 'y' || u[0] == 'A' || u[0] == 'a') && (u[1] == 'M' || u[1] == 'm')) {
			int k = m2 - m1;

			if (d2 < d1)
				k--;
			if (k < 0)
				k += 12;
			ev_num(r, k);
		} else if (un == 2 && (u[0] == 'Y' || u[0] == 'y' || u[0] == 'A' || u[0] == 'a') && (u[1] == 'D' || u[1] == 'd')) {
			double s = fecha_serie(a2, m1, d1);

			if (s > q)
				s = fecha_serie(a2 - 1, m1, d1);
			ev_num(r, so_suelo(q) - s);
		} else
			ev_err(r, ER_NUM);
		return;
	}
	case FN_DIAS: {
		double p, q;

		NUM(0, 0, p);
		NUM(1, 0, q);
		ev_num(r, so_suelo(p) - so_suelo(q));
		return;
	}
	/* ---- el dinero ---- */
	case FN_PAGO: {
		double t, nn, va, vff, ti;

		NUM(0, 0, t);
		NUM(1, 0, nn);
		NUM(2, 0, va);
		NUM(3, 0, vff);
		NUM(4, 0, ti);
		if (nn == 0) {
			ev_err(r, ER_NUM);
			return;
		}
		ev_num(r, pago(t, nn, va, vff, ti != 0));
		return;
	}
	case FN_VA: {
		double t, nn, p, vff, ti;

		NUM(0, 0, t);
		NUM(1, 0, nn);
		NUM(2, 0, p);
		NUM(3, 0, vff);
		NUM(4, 0, ti);
		if (t == 0)
			ev_num(r, -(vff + p * nn));
		else {
			double k = cu_pot(1 + t, nn);

			ev_num(r, -(vff + p * (1 + t * (ti != 0)) * (k - 1) / t) / k);
		}
		return;
	}
	case FN_VF: {
		double t, nn, p, va, ti;

		NUM(0, 0, t);
		NUM(1, 0, nn);
		NUM(2, 0, p);
		NUM(3, 0, va);
		NUM(4, 0, ti);
		ev_num(r, vf(t, nn, p, va, ti != 0));
		return;
	}
	case FN_NPER: {
		double t, p, va, vff, ti;

		NUM(0, 0, t);
		NUM(1, 0, p);
		NUM(2, 0, va);
		NUM(3, 0, vff);
		NUM(4, 0, ti);
		if (t == 0) {
			if (p == 0) {
				ev_err(r, ER_NUM);
				return;
			}
			ev_num(r, -(va + vff) / p);
			return;
		}
		{
			double q = p * (1 + t * (ti != 0)), num_ = q - vff * t, den = q + va * t;

			if (den == 0 || num_ / den <= 0 || t <= -1) {
				ev_err(r, ER_NUM);
				return;
			}
			ev_num(r, cu_ln(num_ / den) / cu_ln(1 + t));
		}
		return;
	}
	case FN_TASA: {
		double nn, p, va, vff, ti, g, t;
		int k;

		NUM(0, 0, nn);
		NUM(1, 0, p);
		NUM(2, 0, va);
		NUM(3, 0, vff);
		NUM(4, 0, ti);
		NUM(5, 0.1, g);
		t = g;
		for (k = 0; k < 200; k++) {
			double f, d, kk, h = 1e-7;

			if (t <= -1) {
				ev_err(r, ER_NUM);
				return;
			}
			if (so_abs(t) < 1e-12)
				f = va + p * nn + vff;
			else {
				kk = cu_pot(1 + t, nn);
				f = va * kk + p * (1 + t * (ti != 0)) * (kk - 1) / t + vff;
			}
			{
				double t2 = t + h, f2;

				kk = cu_pot(1 + t2, nn);
				f2 = va * kk + p * (1 + t2 * (ti != 0)) * (kk - 1) / t2 + vff;
				d = (f2 - f) / h;
			}
			if (d == 0)
				break;
			{
				double nt = t - f / d;

				if (so_abs(nt - t) < 1e-12) {
					t = nt;
					ev_num(r, t);
					return;
				}
				t = nt;
			}
		}
		ev_err(r, ER_NUM);
		return;
	}
	case FN_VNA: {
		double t, s = 0, k = 1;
		flujos F;
		u32 j;

		NUM(0, 0, t);
		if (t == -1) {
			ev_err(r, ER_DIV0);
			return;
		}
		lee_flujos(x, a + 1, n - 1, &F);
		if (F.err) {
			so_suelta(F.v);
			ev_err(r, F.err);
			return;
		}
		for (j = 0; j < F.n; j++) {
			k *= 1 + t;
			s += F.v[j] / k;
		}
		so_suelta(F.v);
		ev_num(r, s);
		return;
	}
	case FN_TIR: {
		double g, t;
		flujos F;
		int k;
		u32 j;

		NUM(1, 0.1, g);
		lee_flujos(x, a, 1, &F);
		if (F.err || F.n < 2) {
			so_suelta(F.v);
			ev_err(r, F.err ? F.err : ER_NUM);
			return;
		}
		t = g;
		for (k = 0; k < 200; k++) {
			double f = 0, d = 0, q = 1;

			if (t <= -1)
				t = -0.99;
			for (j = 0; j < F.n; j++) {
				f += F.v[j] / q;
				d -= j * F.v[j] / (q * (1 + t));
				q *= 1 + t;
			}
			if (d == 0)
				break;
			{
				double nt = t - f / d;

				if (so_abs(nt - t) < 1e-12) {
					so_suelta(F.v);
					ev_num(r, nt);
					return;
				}
				t = nt;
			}
		}
		so_suelta(F.v);
		ev_err(r, ER_NUM);
		return;
	}
	case FN_PAGOINT:
	case FN_PAGOPRIN: {
		double t, per, nn, va, vff, ti, ip;

		NUM(0, 0, t);
		NUM(1, 0, per);
		NUM(2, 0, nn);
		NUM(3, 0, va);
		NUM(4, 0, vff);
		NUM(5, 0, ti);
		if (per < 1 || per > nn || nn == 0) {
			ev_err(r, ER_NUM);
			return;
		}
		ip = pagoint(t, per, nn, va, vff, ti != 0);
		ev_num(r, id == FN_PAGOINT ? ip : pago(t, nn, va, vff, ti != 0) - ip);
		return;
	}
	case FN_SLN: {
		double c, v, vi;

		NUM(0, 0, c);
		NUM(1, 0, v);
		NUM(2, 0, vi);
		if (vi == 0) {
			ev_err(r, ER_DIV0);
			return;
		}
		ev_num(r, (c - v) / vi);
		return;
	}
	case FN_SYD: {
		double c, v, vi, p;

		NUM(0, 0, c);
		NUM(1, 0, v);
		NUM(2, 0, vi);
		NUM(3, 0, p);
		if (vi <= 0 || p < 1 || p > vi) {
			ev_err(r, ER_NUM);
			return;
		}
		ev_num(r, (c - v) * (vi - p + 1) * 2 / (vi * (vi + 1)));
		return;
	}
	case FN_DDB: {
		double c, v, vi, p, fa, total = 0, d = 0;
		int k;

		NUM(0, 0, c);
		NUM(1, 0, v);
		NUM(2, 0, vi);
		NUM(3, 0, p);
		NUM(4, 2, fa);
		if (vi <= 0 || p < 1 || p > vi || fa <= 0 || c < 0 || v < 0) {
			ev_err(r, ER_NUM);
			return;
		}
		for (k = 1; k <= (int)p; k++) {
			d = (c - total) * fa / vi;
			if (d > c - v - total)
				d = c - v - total;
			if (d < 0)
				d = 0;
			total += d;
		}
		ev_num(r, d);
		return;
	}
	}
	ev_err(r, ER_NOMBRE);
}
