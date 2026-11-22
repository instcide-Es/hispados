/* numeros.c - Cuentas 2: los números como texto, los formatos y las
 * fechas (numeros.h). Las cifras se sacan con long double (los 64 bits de
 * mantisa del x87: lo que hace falta para 17 cifras exactas de un double).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "numeros.h"

typedef long double ld;

const char *const MESES[12] = { "enero", "febrero", "marzo", "abril", "mayo", "junio", "julio", "agosto", "septiembre", "octubre", "noviembre", "diciembre" };
const char *const MESES_CORTOS[12] = { "ene", "feb", "mar", "abr", "may", "jun", "jul", "ago", "sep", "oct", "nov", "dic" };
const char *const DIAS[7] = { "domingo", "lunes", "martes", "miércoles", "jueves", "viernes", "sábado" };
const char *const DIAS_CORTOS[7] = { "dom", "lun", "mar", "mié", "jue", "vie", "sáb" };
static const char *const MESES_EN[12] = { "jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec" };

static int es_nan(double x)
{
	return x != x;
}

static int es_inf(double x)
{
	return x > 1.7976931348623157e308 || x < -1.7976931348623157e308;
}

/* 10^n en long double (exacto hasta 10^27) */
static ld p10(int n)
{
	static const ld t[] = { 1e0L, 1e1L, 1e2L, 1e3L, 1e4L, 1e5L, 1e6L, 1e7L, 1e8L, 1e9L, 1e10L, 1e11L, 1e12L, 1e13L,
				1e14L, 1e15L, 1e16L, 1e17L, 1e18L, 1e19L, 1e20L, 1e21L, 1e22L, 1e23L, 1e24L, 1e25L, 1e26L, 1e27L };
	ld r = 1, b = 10;
	int neg = n < 0, k;

	if (neg)
		n = -n;
	if (n <= 27)
		r = t[n];
	else {
		k = n;
		while (k) {
			if (k & 1)
				r *= b;
			b *= b;
			k >>= 1;
		}
	}
	return neg ? 1 / r : r;
}

/* las cifras de x > 0 redondeado a «sig» cifras (1-18): en dig (sin el
 * punto) y en *e10 el exponente de la primera (x = 0,d1d2... * 10^(e10+1)) */
static void cifras(double x, int sig, char *dig, int *e10)
{
	ld v = x, s;
	int e = 0, i;
	i64 r, tope;

	/* el exponente: una idea por los bits y luego afinarlo */
	{
		int b = 0;
		double m = x;

		while (m >= 2 && b < 1100) {
			m /= 2;
			b++;
			if (m >= 1e16) {
				m /= 1e16;
				b += 53;	/* (más o menos: se afina abajo) */
			}
		}
		while (m < 1 && b > -1100) {
			m *= 2;
			b--;
			if (m < 1e-16) {
				m *= 1e16;
				b -= 53;
			}
		}
		e = (int)(b * 0.30102999566398);
	}
	while (e < 330 && v >= p10(e + 1))
		e++;
	while (e > -330 && v < p10(e))
		e--;
	s = v * p10(sig - 1 - e);
	r = (i64)(s + 0.5L);
	tope = 1;
	for (i = 0; i < sig; i++)
		tope *= 10;
	if (r >= tope) {
		r /= 10;
		e++;
	}
	for (i = sig - 1; i >= 0; i--) {
		dig[i] = (char)('0' + (int)(r % 10));
		r /= 10;
	}
	dig[sig] = 0;
	*e10 = e;
}

/* lee mantisa y exponente a long double; da el double */
static double compon(i64 m, int e)
{
	ld v = (ld)m;

	if (!m)
		return 0;
	if (e > 330)
		return 1e308 * 10;
	if (e < -360)
		return 0;
	v *= p10(e);
	return (double)v;
}

int num_lee_c(const char *s, double *x, const char **fin)
{
	const char *p = s;
	int neg = 0, e = 0, hay = 0, cifras_m = 0;
	i64 m = 0;

	while (*p == ' ')
		p++;
	if (*p == '-' || *p == '+') {
		neg = *p == '-';
		p++;
	}
	while (*p >= '0' && *p <= '9') {
		hay = 1;
		if (cifras_m < 18) {
			m = m * 10 + (*p - '0');
			if (m)
				cifras_m++;
		} else
			e++;
		p++;
	}
	if (*p == '.') {
		p++;
		while (*p >= '0' && *p <= '9') {
			hay = 1;
			if (cifras_m < 18) {
				m = m * 10 + (*p - '0');
				if (m)
					cifras_m++;
				e--;
			}
			p++;
		}
	}
	if (!hay)
		return 0;
	if (*p == 'e' || *p == 'E') {
		const char *q = p + 1;
		int en = 0, ee = 0;

		if (*q == '-' || *q == '+') {
			en = *q == '-';
			q++;
		}
		if (*q >= '0' && *q <= '9') {
			while (*q >= '0' && *q <= '9') {
				if (ee < 10000)
					ee = ee * 10 + (*q - '0');
				q++;
			}
			e += en ? -ee : ee;
			p = q;
		}
	}
	*x = compon(m, e);
	if (neg)
		*x = -*x;
	if (fin)
		*fin = p;
	return 1;
}

double num_de_texto_c(const char *s)
{
	double x = 0;

	num_lee_c(s, &x, NULL);
	return x;
}

/* d con las cifras dig (sig) y el exponente: sin exponente si cabe */
static void escribe_cifras(const char *dig, int sig, int e, int neg, char dec, char *d, u32 cabe)
{
	char t[64];
	u32 n = 0;
	int i;

	/* (sin ceros de más al final) */
	while (sig > 1 && dig[sig - 1] == '0')
		sig--;
	if (neg)
		t[n++] = '-';
	if (e >= 21 || e < -9) {
		t[n++] = dig[0];
		if (sig > 1) {
			t[n++] = dec;
			for (i = 1; i < sig; i++)
				t[n++] = dig[i];
		}
		n += (u32)fmt(t + n, sizeof t - n, "E%c%d", e < 0 ? '-' : '+', e < 0 ? -e : e);
	} else if (e < 0) {
		t[n++] = '0';
		t[n++] = dec;
		for (i = 0; i < -e - 1; i++)
			t[n++] = '0';
		for (i = 0; i < sig; i++)
			t[n++] = dig[i];
	} else {
		for (i = 0; i <= e; i++)
			t[n++] = i < sig ? dig[i] : '0';
		if (sig > e + 1) {
			t[n++] = dec;
			for (i = e + 1; i < sig; i++)
				t[n++] = dig[i];
		}
	}
	t[n] = 0;
	copia(d, t, cabe);
}

void num_a_texto_c(double x, char *d, u32 cabe)
{
	char dig[24];
	int e, sig;

	if (es_nan(x) || es_inf(x)) {
		copia(d, "0", cabe);
		return;
	}
	if (x == 0) {
		copia(d, "0", cabe);
		return;
	}
	for (sig = 15; sig <= 17; sig++) {
		char t[64];

		cifras(x < 0 ? -x : x, sig, dig, &e);
		escribe_cifras(dig, sig, e, x < 0, '.', t, sizeof t);
		if (sig == 17 || num_de_texto_c(t) == x) {
			copia(d, t, cabe);
			return;
		}
	}
}

void num_general(double x, const cu_local *loc, int max, char *d, u32 cabe)
{
	char dig[24];
	int e, sig;

	if (max <= 0)
		max = 11;
	if (es_nan(x) || es_inf(x)) {
		copia(d, "#¡NUM!", cabe);
		return;
	}
	if (x == 0) {
		copia(d, "0", cabe);
		return;
	}
	for (sig = 10; sig >= 1; sig--) {
		char t[64];

		cifras(x < 0 ? -x : x, sig, dig, &e);
		if (e >= 11 || e < -9) {
			/* científica: 1,23457E+11 */
			int s2 = sig > 6 ? 6 : sig;

			cifras(x < 0 ? -x : x, s2, dig, &e);
			{
				u32 n = 0;
				int i;

				if (x < 0)
					t[n++] = '-';
				t[n++] = dig[0];
				while (s2 > 1 && dig[s2 - 1] == '0')
					s2--;
				if (s2 > 1) {
					t[n++] = loc->dec;
					for (i = 1; i < s2; i++)
						t[n++] = dig[i];
				}
				fmt(t + n, sizeof t - n, "E%c%02d", e < 0 ? '-' : '+', e < 0 ? -e : e);
			}
		} else
			escribe_cifras(dig, sig, e, x < 0, loc->dec, t, sizeof t);
		if ((int)largo(t) <= max || sig == 1) {
			copia(d, t, cabe);
			return;
		}
	}
}

double num_redondea(double x, int d)
{
	char dig[24];
	int e, sig, neg = x < 0;
	ld v;
	i64 r;

	if (x == 0 || es_nan(x) || es_inf(x))
		return x;
	if (d > 15)
		d = 15;
	if (d < -15)
		d = -15;
	/* (con 15 cifras primero: así 2,675 es 2,675 y no 2,67499999) */
	cifras(neg ? -x : x, 15, dig, &e);
	{
		i64 m = 0;
		int i;

		for (i = 0; i < 15; i++)
			m = m * 10 + (dig[i] - '0');
		/* x = m * 10^(e - 14) */
		sig = e - 14;	/* el exponente de la última cifra */
		if (-sig <= d)
			return x;	/* ya tiene menos decimales */
		{
			int quita = -sig - d;	/* cifras que sobran */

			if (quita > 15)
				return 0;
			v = (ld)m;
			{
				i64 div = 1;
				int k;

				for (k = 0; k < quita; k++)
					div *= 10;
				r = (m + div / 2) / div;
				v = (ld)r * p10(sig + quita);
			}
		}
	}
	return neg ? -(double)v : (double)v;
}

/* ---- las fechas ---- */
static int bisiesto(int a)
{
	return (a % 4 == 0 && a % 100 != 0) || a % 400 == 0;
}

int dias_mes(int a, int m)
{
	static const int d[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

	if (m < 1 || m > 12)
		return 30;
	return d[m - 1] + (m == 2 && bisiesto(a));
}

/* los días desde el 1/3/0 (para las cuentas) */
static i64 dias_civil(i64 a, int m, int d)
{
	i64 era;
	u32 ya, dy, dp;

	a -= m <= 2;
	era = (a >= 0 ? a : a - 399) / 400;
	ya = (u32)(a - era * 400);
	dp = (u32)((m + 9) % 12);
	dy = (153 * dp + 2) / 5 + (u32)d - 1;
	return era * 146097 + (i64)(ya * 365 + ya / 4 - ya / 100 + dy) - 719468;
}

double fecha_serie(int a, int m, int d)
{
	i64 k;

	/* (los meses de más o de menos pasan al año) */
	if (a >= 0 && a < 1900)
		a += 1900;
	a += (m - 1) >= 0 ? (m - 1) / 12 : -((12 - m) / 12);
	m = ((m - 1) % 12 + 12) % 12 + 1;
	k = dias_civil(a, m, 1) + d - 1;
	/* el 30/12/1899 es el día 0 (y el 1/1/1970, el 25569) */
	return (double)(k + 25569);
}

void serie_fecha(double s, int *ano, int *mes, int *dia)
{
	i64 z = (i64)so_suelo(s) - 25569 + 719468, era;
	u32 doe, yoe, doy, mp;
	i64 y;

	era = (z >= 0 ? z : z - 146096) / 146097;
	doe = (u32)(z - era * 146097);
	yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	y = (i64)yoe + era * 400;
	doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
	mp = (5 * doy + 2) / 153;
	*dia = (int)(doy - (153 * mp + 2) / 5 + 1);
	*mes = (int)(mp < 10 ? mp + 3 : mp - 9);
	*ano = (int)(y + (*mes <= 2));
}

void serie_hora(double s, int *h, int *mi, int *se)
{
	double f = s - so_suelo(s);
	i64 t = (i64)(f * 86400 + 0.5);

	if (t >= 86400)
		t = 86399;
	*h = (int)(t / 3600);
	*mi = (int)(t / 60 % 60);
	*se = (int)(t % 60);
}

int dia_semana(double s)
{
	i64 k = (i64)so_suelo(s);

	/* (el 30/12/1899 fue sábado) */
	return (int)(((k + 6) % 7 + 7) % 7);
}

/* ---- leer lo que se teclea ---- */
static int letra_igual(u8 a, u8 b)
{
	if (a >= 'A' && a <= 'Z')
		a += 32;
	if (b >= 'A' && b <= 'Z')
		b += 32;
	return a == b;
}

/* el mes por su nombre (ene, enero, jan...) al principio de s: 1-12, y lo que ocupa */
static int mes_nombre(const char *s, u32 *lg)
{
	int m;
	u32 k;

	for (m = 0; m < 12; m++) {
		const char *a = MESES[m];

		for (k = 0; a[k] && s[k] && letra_igual((u8)s[k], (u8)a[k]); k++)
			;
		if (!a[k] && !((s[k] | 0x20) >= 'a' && (s[k] | 0x20) <= 'z')) {
			*lg = k;
			return m + 1;
		}
		if (k >= 3 && !((s[3] | 0x20) >= 'a' && (s[3] | 0x20) <= 'z') && letra_igual((u8)s[0], (u8)a[0])) {
			*lg = 3;
			return m + 1;
		}
		a = MESES_EN[m];
		if (letra_igual((u8)s[0], (u8)a[0]) && letra_igual((u8)s[1], (u8)a[1]) && letra_igual((u8)s[2], (u8)a[2]) &&
		    !((s[3] | 0x20) >= 'a' && (s[3] | 0x20) <= 'z')) {
			*lg = 3;
			return m + 1;
		}
	}
	return 0;
}

static int lee_entero(const char **p, int *v, int max_cifras)
{
	int n = 0, k = 0;

	while (**p >= '0' && **p <= '9' && k < max_cifras) {
		n = n * 10 + (**p - '0');
		(*p)++;
		k++;
	}
	if (!k)
		return 0;
	*v = n;
	return k;
}

static int ano_corto(int a, int cifras_)
{
	if (cifras_ <= 2)
		return a < 30 ? 2000 + a : 1900 + a;
	return a;
}

/* una hora (14:30, 14:30:15, 2:30 pm) al principio de p: la fracción del día */
static int lee_hora(const char **p, double *t, int *con_seg)
{
	const char *q = *p;
	int h, mi, se = 0;

	if (!lee_entero(&q, &h, 4) || *q != ':')
		return 0;
	q++;
	if (!lee_entero(&q, &mi, 2) || mi > 59)
		return 0;
	*con_seg = 0;
	{
		double fs = 0;

		if (*q == ':') {
			q++;
			if (!lee_entero(&q, &se, 2) || se > 59)
				return 0;
			*con_seg = 1;
			if (*q == ',' || *q == '.') {
				double div = 10;

				q++;
				while (*q >= '0' && *q <= '9') {
					fs += (*q - '0') / div;
					div *= 10;
					q++;
				}
			}
		}
		while (*q == ' ')
			q++;
		if ((letra_igual((u8)q[0], 'a') || letra_igual((u8)q[0], 'p')) && (letra_igual((u8)q[1], 'm') || q[1] == '.')) {
			int pm = letra_igual((u8)q[0], 'p');

			if (h > 12 || h == 0)
				return 0;
			if (h == 12)
				h = 0;
			if (pm)
				h += 12;
			q += 2;
			if (*q == '.')
				q++;
			if (letra_igual((u8)*q, 'm'))
				q++;
			if (*q == '.')
				q++;
		}
		*t = (h * 3600.0 + mi * 60.0 + se + fs) / 86400.0;
	}
	*p = q;
	return 1;
}

/* una fecha (22/11/2026, 22-11-26, 22/11, 2026-11-22, 22-nov-2026, 22 nov 2026) */
static int lee_fecha(const char **p, double *s)
{
	const char *q = *p;
	int a, m, d, k1, k2, k3;
	u32 lg;

	k1 = lee_entero(&q, &d, 4);
	if (!k1)
		return 0;
	if (k1 == 4 && *q == '-') {
		/* ISO: 2026-11-22 */
		a = d;
		q++;
		if (!lee_entero(&q, &m, 2) || *q != '-')
			return 0;
		q++;
		if (!lee_entero(&q, &d, 2))
			return 0;
		if (m < 1 || m > 12 || d < 1 || d > dias_mes(a, m))
			return 0;
		*s = fecha_serie(a, m, d);
		*p = q;
		return 1;
	}
	if (k1 > 2)
		return 0;
	if (*q == '/' || *q == '-' || *q == '.' || *q == ' ') {
		char sep = *q;

		q++;
		if (sep == ' ' || sep == '-' || sep == '.') {
			while (*q == ' ')
				q++;
			if ((m = mes_nombre(q, &lg)) != 0) {
				q += lg;
				if (*q == '.')
					q++;
				if (*q == sep || (*q == ' ' && sep != '.') || *q == '-') {
					const char *r = q + 1;

					while (*r == ' ')
						r++;
					if ((k3 = lee_entero(&r, &a, 4)) != 0) {
						q = r;
						a = ano_corto(a, k3);
					} else {
						int dd, mm, ss;

						so_ahora(&a, &mm, &dd, &k2, &k3, &ss);
					}
				} else {
					int mm, dd, hh, mi, ss;

					so_ahora(&a, &mm, &dd, &hh, &mi, &ss);
				}
				if (d < 1 || d > dias_mes(a, m))
					return 0;
				*s = fecha_serie(a, m, d);
				*p = q;
				return 1;
			}
			if (sep == ' ')
				return 0;
		}
		k2 = lee_entero(&q, &m, 2);
		if (!k2 || m < 1 || m > 12)
			return 0;
		if (*q == sep) {
			q++;
			k3 = lee_entero(&q, &a, 4);
			if (!k3 || k3 == 3)
				return 0;
			a = ano_corto(a, k3);
		} else {
			int mm, dd, hh, mi, ss;

			if (sep == '.')
				return 0;	/* (22.11 es un número) */
			so_ahora(&a, &mm, &dd, &hh, &mi, &ss);
		}
		if (d < 1 || d > dias_mes(a, m))
			return 0;
		*s = fecha_serie(a, m, d);
		*p = q;
		return 1;
	}
	return 0;
}

int num_lee_entrada(const char *s0, const cu_local *loc, const char *moneda, double *x, int *tipo)
{
	char s[160];
	const char *p;
	int neg = 0, paren = 0, pct = 0, mon = 0, miles = 0, hay = 0, dec_hay = 0, cien = 0;
	i64 m = 0;
	int e = 0, cm = 0;
	u32 n, lm = moneda ? largo(moneda) : 0;

	*tipo = NE_NADA;
	copia(s, s0, sizeof s);
	/* sin blancos a los lados */
	n = largo(s);
	while (n && (s[n - 1] == ' ' || s[n - 1] == '\t'))
		s[--n] = 0;
	p = s;
	while (*p == ' ' || *p == '\t')
		p++;
	if (!*p)
		return 0;
	/* fecha y hora */
	{
		const char *q = p;
		double f, h;
		int cs;

		if (lee_fecha(&q, &f)) {
			while (*q == ' ')
				q++;
			if (!*q) {
				*x = f;
				*tipo = NE_FECHA;
				return 1;
			}
			if (lee_hora(&q, &h, &cs) && !*q) {
				*x = f + h;
				*tipo = NE_FECHA_HORA;
				return 1;
			}
		}
		q = p;
		if (lee_hora(&q, &h, &cs) && !*q) {
			*x = h;
			*tipo = cs ? NE_HORA_SEG : NE_HORA;
			return 1;
		}
	}
	/* el signo, los paréntesis, la moneda */
	if (*p == '-' || *p == '+') {
		neg = *p == '-';
		p++;
		while (*p == ' ')
			p++;
	}
	if (*p == '(') {
		paren = 1;
		p++;
	}
	if (lm && strncmp(p, moneda, lm) == 0) {
		mon = 1;
		p += lm;
		while (*p == ' ')
			p++;
	} else if (*p == '$' || (p[0] == 'E' && p[1] == 'U' && p[2] == 'R' && p[3] == ' ')) {
		mon = 1;
		p += *p == '$' ? 1 : 4;
		while (*p == ' ')
			p++;
	}
	if (!neg && (*p == '-' || *p == '+')) {
		neg = *p == '-';
		p++;
	}
	/* las cifras, con los millares en grupos de tres */
	{
		int grupo = -1;

		for (;;) {
			if (*p >= '0' && *p <= '9') {
				hay = 1;
				if (cm < 18) {
					m = m * 10 + (*p - '0');
					if (m)
						cm++;
				} else
					e++;
				if (grupo >= 0)
					grupo++;
				p++;
			} else if (*p == loc->mil && !dec_hay && hay && p[1] >= '0' && p[1] <= '9') {
				if (grupo >= 0 && grupo != 3)
					return 0;
				grupo = 0;
				miles = 1;
				p++;
			} else
				break;
		}
		if (grupo >= 0 && grupo != 3)
			return 0;
	}
	if (*p == loc->dec) {
		p++;
		dec_hay = 1;
		while (*p >= '0' && *p <= '9') {
			hay = 1;
			if (cm < 18) {
				m = m * 10 + (*p - '0');
				if (m)
					cm++;
				e--;
			}
			p++;
		}
	}
	if (!hay)
		return 0;
	if ((*p == 'e' || *p == 'E') && ((p[1] >= '0' && p[1] <= '9') || ((p[1] == '+' || p[1] == '-') && p[2] >= '0' && p[2] <= '9'))) {
		int en = 0, ee = 0;

		p++;
		if (*p == '+' || *p == '-')
			en = *p++ == '-';
		while (*p >= '0' && *p <= '9') {
			if (ee < 10000)
				ee = ee * 10 + (*p - '0');
			p++;
		}
		e += en ? -ee : ee;
		cien = 1;
	}
	while (*p == ' ')
		p++;
	if (*p == '%') {
		pct = 1;
		p++;
		while (*p == ' ')
			p++;
	}
	if (!mon && lm && strncmp(p, moneda, lm) == 0) {
		mon = 1;
		p += lm;
	} else if (!mon && (strncmp(p, "EUR", 3) == 0 || *p == '$')) {
		mon = 1;
		p += *p == '$' ? 1 : 3;
	}
	while (*p == ' ')
		p++;
	if (paren) {
		if (*p != ')')
			return 0;
		p++;
		neg = !neg;
	}
	while (*p == ' ')
		p++;
	if (*p)
		return 0;
	if (pct && mon)
		return 0;
	*x = compon(m, e - (pct ? 2 : 0));
	if (neg)
		*x = -*x;
	*tipo = pct ? NE_PORCENTAJE : mon ? NE_MONEDA : cien ? NE_CIENTIFICO : miles ? (dec_hay ? NE_MILES_DEC : NE_MILES) : NE_NADA;
	return 1;
}

const char *num_fmt_de_entrada(int tipo, const char *s, const cu_local *loc, const char *moneda)
{
	static char c[48];
	const char *d = strchr(s, loc->dec);

	(void)moneda;
	switch (tipo) {
	case NE_PORCENTAJE:
		return d ? "0.00%" : "0%";
	case NE_MONEDA:
		fmt(c, sizeof c, "#,##0.00 \"%s\"", moneda && moneda[0] ? moneda : "€");
		if (moneda && iguales(moneda, "€"))
			return "#,##0.00 €";
		return c;
	case NE_FECHA:
		return "dd/mm/yyyy";
	case NE_HORA:
		return "h:mm";
	case NE_HORA_SEG:
		return "h:mm:ss";
	case NE_FECHA_HORA:
		return "dd/mm/yyyy h:mm";
	case NE_CIENTIFICO:
		return "0.00E+00";
	case NE_MILES:
		return "#,##0";
	case NE_MILES_DEC:
		return "#,##0.00";
	}
	return "General";
}

/* ---- los códigos de formato ---- */
typedef struct {
	const char *s;		/* la sección */
	u32 n;
} seccion;

static int secciones(const char *c, seccion *sec, int max)
{
	int k = 0, com = 0, cor = 0;
	const char *a = c, *p;

	for (p = c;; p++) {
		if (*p == '\\' && p[1]) {
			p++;
			continue;
		}
		if (*p == '"')
			com = !com;
		else if (!com && *p == '[')
			cor = 1;
		else if (!com && *p == ']')
			cor = 0;
		if (!*p || (*p == ';' && !com && !cor)) {
			if (k < max) {
				sec[k].s = a;
				sec[k].n = (u32)(p - a);
				k++;
			}
			if (!*p)
				break;
			a = p + 1;
		}
	}
	return k;
}

/* ¿es de fecha u hora la sección? */
static int sec_fecha(const seccion *s)
{
	u32 i;
	int com = 0;

	for (i = 0; i < s->n; i++) {
		char c = s->s[i];

		if (c == '\\' || c == '_' || c == '*') {
			i++;
			continue;
		}
		if (c == '"') {
			com = !com;
			continue;
		}
		if (com)
			continue;
		if (c == '[') {
			/* [h] [mm] [ss]: horas; [Red], [$€-C0A]: no */
			if (i + 1 < s->n && (letra_igual((u8)s->s[i + 1], 'h') || letra_igual((u8)s->s[i + 1], 'm') || letra_igual((u8)s->s[i + 1], 's')) &&
			    i + 2 < s->n && (s->s[i + 2] == ']' || letra_igual((u8)s->s[i + 2], s->s[i + 1])))
				return 1;
			while (i < s->n && s->s[i] != ']')
				i++;
			continue;
		}
		if (letra_igual((u8)c, 'd') || letra_igual((u8)c, 'y') || letra_igual((u8)c, 'h') || letra_igual((u8)c, 's') ||
		    (letra_igual((u8)c, 'm') && !(i + 1 < s->n && letra_igual((u8)s->s[i + 1], 'p'))) ||
		    (letra_igual((u8)c, 'a') && i + 1 < s->n && letra_igual((u8)s->s[i + 1], 'a')))
			return 1;
	}
	return 0;
}

int fmt_es_fecha(const char *codigo)
{
	seccion s[4];
	int n;

	if (!codigo || iguales_sin_caja(codigo, "General"))
		return 0;
	n = secciones(codigo, s, 4);
	return n > 0 && sec_fecha(&s[0]);
}

int fmt_categoria(const char *c)
{
	seccion s[4];
	int n;

	if (!c || !c[0] || iguales_sin_caja(c, "General"))
		return CAT_GENERAL;
	if (iguales(c, "@"))
		return CAT_TEXTO;
	n = secciones(c, s, 4);
	if (n && sec_fecha(&s[0])) {
		/* ¿solo hora? */
		u32 i;
		int com = 0, f = 0;

		for (i = 0; i < s[0].n; i++) {
			char k = s[0].s[i];

			if (k == '"')
				com = !com;
			if (!com && (letra_igual((u8)k, 'd') || letra_igual((u8)k, 'y')))
				f = 1;
		}
		return f ? CAT_FECHA : CAT_HORA;
	}
	if (busca(c, "E+") || busca(c, "E-") || busca(c, "e+"))
		return CAT_CIENTIFICA;
	if (strchr(c, '%'))
		return CAT_PORCENTAJE;
	if (busca(c, "€") || strchr(c, '$') || busca(c, "EUR"))
		return CAT_MONEDA;
	{
		/* número: solo 0 # , . y un - delante */
		const char *p;

		for (p = c; *p; p++)
			if (!strchr("0#,.-;()[]Redolj ", *p))
				return CAT_PERSONAL;
		return CAT_NUMERO;
	}
}

int fmt_decimales(const char *c)
{
	const char *p;
	int n = 0, com = 0, en = 0;

	for (p = c; *p && !(*p == ';' && !com); p++) {
		if (*p == '"')
			com = !com;
		if (com)
			continue;
		if (*p == '.')
			en = 1;
		else if (en && (*p == '0' || *p == '#' || *p == '?'))
			n++;
		else if (en)
			break;
	}
	return n;
}

void fmt_cambia_decimales(const char *c, int mas, char *d, u32 cabe)
{
	bufer b = { 0 };
	const char *p;
	int com = 0;

	if (!c || iguales_sin_caja(c, "General"))
		c = mas > 0 ? "0" : "0";
	/* cada sección: tras la última cifra de la parte entera */
	for (p = c;;) {
		const char *ini = p, *pt = NULL, *ult = NULL;

		while (*p && !(*p == ';' && !com)) {
			if (*p == '"')
				com = !com;
			else if (!com && *p == '.' && !pt)
				pt = p;
			else if (!com && (*p == '0' || *p == '#' || *p == '?'))
				ult = p;
			p++;
		}
		if (!ult) {
			buf_pon(&b, ini, (u32)(p - ini));
		} else if (mas > 0) {
			if (pt) {
				buf_pon(&b, ini, (u32)(ult - ini + 1));
				buf_c(&b, '0');
			} else {
				buf_pon(&b, ini, (u32)(ult - ini + 1));
				buf_txt(&b, ".0");
			}
			buf_pon(&b, ult + 1, (u32)(p - ult - 1));
		} else {
			if (pt && ult > pt) {
				/* quita la última cifra de los decimales (y el punto, si se queda sin ninguna) */
				if (ult == pt + 1) {
					buf_pon(&b, ini, (u32)(pt - ini));
				} else
					buf_pon(&b, ini, (u32)(ult - ini));
				buf_pon(&b, ult + 1, (u32)(p - ult - 1));
			} else
				buf_pon(&b, ini, (u32)(p - ini));
		}
		if (!*p)
			break;
		buf_c(&b, ';');
		p++;
	}
	buf_c(&b, 0);
	copia(d, b.d ? (char *)b.d : "General", cabe);
	buf_suelta(&b);
}

/* «#,##0.00» <-> «#.##0,00»; «yyyy» <-> «aaaa» */
void fmt_a_local(const char *c, const cu_local *loc, char *d, u32 cabe)
{
	u32 n = 0;
	int com = 0, cor = 0, fecha = fmt_es_fecha(c);

	if (!cabe)
		return;
	if (!c || iguales_sin_caja(c, "General")) {
		copia(d, "Estándar", cabe);
		return;
	}
	for (; *c && n + 4 < cabe; c++) {
		char k = *c;

		if (k == '"')
			com = !com;
		else if (!com && k == '[')
			cor = 1;
		else if (!com && k == ']')
			cor = 0;
		if (k == '\\' && c[1]) {
			d[n++] = k;
			d[n++] = *++c;
			continue;
		}
		if (!com && !cor) {
			if (k == '.' && !fecha)
				k = loc->dec;
			else if (k == ',' && !fecha)
				k = loc->mil;
			else if ((k == 'y' || k == 'Y') && loc->dec == ',')
				k = 'a';
		}
		d[n++] = k;
	}
	d[n] = 0;
}

void fmt_de_local(const char *t, const cu_local *loc, char *d, u32 cabe)
{
	u32 n = 0;
	int com = 0, cor = 0, fecha;
	char prov[200];

	/* (en las de fecha, la coma y el punto son tal cual) */
	{
		u32 i;
		int q = 0;

		for (i = 0; t[i] && i + 1 < sizeof prov; i++) {
			char k = t[i];

			if (k == '"')
				q = !q;
			if (!q && (k == 'a' || k == 'A') && !(t[i + 1] == 'm' || t[i + 1] == 'M' || t[i + 1] == '/'))
				k = 'y';
			prov[i] = k;
		}
		prov[i] = 0;
		fecha = fmt_es_fecha(prov);
	}

	if (!cabe)
		return;
	if (!t[0] || iguales_sin_caja(t, "Estándar") || iguales_sin_caja(t, "General")) {
		copia(d, "General", cabe);
		return;
	}
	for (; *t && n + 4 < cabe; t++) {
		char k = *t;

		if (k == '"')
			com = !com;
		else if (!com && k == '[')
			cor = 1;
		else if (!com && k == ']')
			cor = 0;
		if (k == '\\' && t[1]) {
			d[n++] = k;
			d[n++] = *++t;
			continue;
		}
		if (!com && !cor) {
			if (k == loc->dec && !fecha)
				k = '.';
			else if (k == loc->mil && !fecha)
				k = ',';
			else if (k == 'a' || k == 'A') {
				/* aaaa es el año (salvo AM/PM) */
				if (!(t[1] == 'm' || t[1] == 'M' || t[1] == '/'))
					k = 'y';
			}
		}
		d[n++] = k;
	}
	d[n] = 0;
}

/* ---- escribir con un código ---- */
static u32 color_de(const char *s, u32 n)
{
	static const struct {
		const char *a, *b;
		u32 c;
	} t[] = { { "Red", "Rojo", 0xD32F2F }, { "Blue", "Azul", 0x1F4E9E }, { "Green", "Verde", 0x2E7D32 },
		  { "Black", "Negro", 0x000000 }, { "White", "Blanco", 0xFFFFFF }, { "Yellow", "Amarillo", 0xB38B00 },
		  { "Magenta", "Magenta", 0xA0208F }, { "Cyan", "Aguamarina", 0x00838F } };
	u32 i, k;

	for (i = 0; i < sizeof t / sizeof t[0]; i++) {
		const char *x[2] = { t[i].a, t[i].b };

		for (k = 0; k < 2; k++)
			if (largo(x[k]) == n) {
				u32 j;

				for (j = 0; j < n && letra_igual((u8)s[j], (u8)x[k][j]); j++)
					;
				if (j == n)
					return t[i].c;
			}
	}
	return CU_AUTO;
}

/* las cifras enteras y decimales de v >= 0 con «nd» decimales: en ent (sin
 * ceros delante; «» si es cero) y dec (nd cifras). Se redondea con 15
 * cifras primero, como se ve: 2,675 con dos decimales es 2,68 */
static void parte(double v, int nd, char *ent, u32 ce, char *dec)
{
	char d15[24], r[460];
	int e15, q, i;
	u32 nr = 0;

	ent[0] = 0;
	for (i = 0; i < nd; i++)
		dec[i] = '0';
	dec[nd] = 0;
	if (v == 0)
		return;
	cifras(v, 15, d15, &e15);
	/* v = m * 10^(e15 - 14), con m las 15 cifras; hace falta round(v * 10^nd) */
	q = e15 - 14 + nd;
	if (q >= 0) {
		for (i = 0; i < 15; i++)
			r[nr++] = d15[i];
		for (i = 0; i < q && nr + 1 < sizeof r - 40; i++)
			r[nr++] = '0';
	} else {
		int quita = -q;
		i64 m = 0, div = 1, x;

		for (i = 0; i < 15; i++)
			m = m * 10 + (d15[i] - '0');
		if (quita > 15)
			x = 0;
		else {
			for (i = 0; i < quita; i++)
				div *= 10;
			x = (m + div / 2) / div;
		}
		{
			char t[24];
			int nt = 0;

			while (x > 0) {
				t[nt++] = (char)('0' + (int)(x % 10));
				x /= 10;
			}
			while (nt > 0)
				r[nr++] = t[--nt];
		}
	}
	/* con ceros delante hasta tener nd + 1 cifras */
	while ((int)nr < nd + 1) {
		memmove(r + 1, r, nr);
		r[0] = '0';
		nr++;
	}
	r[nr] = 0;
	memcpy(dec, r + nr - (u32)nd, (u32)nd);
	dec[nd] = 0;
	r[nr - (u32)nd] = 0;
	{
		char *z = r;

		while (*z == '0')
			z++;
		copia(ent, z, ce);
	}
}

/* un trozo literal de la sección: lo que no son cifras */
static void literal(bufer *b, const char *s, u32 n)
{
	u32 i;

	for (i = 0; i < n; i++) {
		char c = s[i];

		if (c == '"') {
			i++;
			while (i < n && s[i] != '"')
				buf_c(b, (u8)s[i++]);
			continue;
		}
		if (c == '\\' && i + 1 < n) {
			buf_c(b, (u8)s[++i]);
			continue;
		}
		if (c == '_' && i + 1 < n) {
			i++;
			buf_c(b, ' ');
			continue;
		}
		if (c == '*' && i + 1 < n) {
			i++;
			continue;
		}
		if (c == '[') {
			/* [$€-C0A] es la moneda; lo demás ([Red], [>100]) no se ve */
			if (i + 1 < n && s[i + 1] == '$') {
				i += 2;
				while (i < n && s[i] != '-' && s[i] != ']')
					buf_c(b, (u8)s[i++]);
			}
			while (i < n && s[i] != ']')
				i++;
			continue;
		}
		buf_c(b, (u8)c);
	}
}

static void formatea_num(double x, const seccion *s, const cu_local *loc, int con_signo, bufer *b)
{
	u32 i, ini = 0, fin = 0, hay = 0;
	int com = 0, enteros = 0, opc_ent = 0, decs = 0, decs_opc = 0, pt = 0, miles = 0, escala = 0, pct = 0, cient = 0;
	int exp_cifras = 0, exp_signo = 0;
	u32 pos_e = 0;

	/* dónde están las cifras (de la primera a la última) y cómo son */
	for (i = 0; i < s->n; i++) {
		char c = s->s[i];

		if (c == '"') {
			com = !com;
			continue;
		}
		if (com)
			continue;
		if (c == '\\' || c == '_' || c == '*') {
			i++;
			continue;
		}
		if (c == '[') {
			while (i < s->n && s->s[i] != ']')
				i++;
			continue;
		}
		if (c == '%')
			pct++;
		if (cient) {
			if (c == '0' || c == '#')
				exp_cifras++;
			if (c == '0' || c == '#')
				fin = i + 1;
			continue;
		}
		if (c == '0' || c == '#' || c == '?') {
			if (!hay)
				ini = i;
			hay = 1;
			fin = i + 1;
			if (pt) {
				if (c == '0')
					decs++;
				else
					decs_opc++;
			} else {
				if (c == '0')
					enteros++;
				else
					opc_ent++;
				escala = 0;
			}
		} else if (c == '.' && hay && !pt) {
			pt = 1;
			fin = i + 1;
		} else if (c == '.' && !hay && !pt && i + 1 < s->n && (s->s[i + 1] == '0' || s->s[i + 1] == '#')) {
			pt = 1;
			ini = i;
			hay = 1;
		} else if (c == ',' && hay && !pt) {
			/* entre cifras: millares; detrás de ellas: entre mil */
			if (i + 1 < s->n && (s->s[i + 1] == '0' || s->s[i + 1] == '#' || s->s[i + 1] == '?'))
				miles = 1;
			else
				escala++;
			fin = i + 1;
		} else if ((c == 'E' || c == 'e') && hay && i + 1 < s->n && (s->s[i + 1] == '+' || s->s[i + 1] == '-')) {
			cient = 1;
			pos_e = i;
			exp_signo = s->s[i + 1] == '+';
			i++;
			fin = i + 1;
		}
	}
	if (!hay) {
		/* (sin cifras: solo el texto, como «"cero"») */
		literal(b, s->s, s->n);
		return;
	}
	{
		double v = x < 0 ? -x : x;
		char ent[400], dec[40];
		int nd = decs + decs_opc, k, e10 = 0;
		u32 n0;

		if (pct)
			v *= 100.0 * (pct > 1 ? 100.0 : 1);
		while (escala-- > 0)
			v /= 1000;
		if (nd > 30)
			nd = 30;
		if (cient && v != 0) {
			char dig[24];
			int ne = enteros + opc_ent;

			if (ne < 1)
				ne = 1;
			cifras(v, nd + ne > 17 ? 17 : nd + ne, dig, &e10);
			/* (con «##0.0E+0», el exponente de 3 en 3) */
			if (ne > 1 && opc_ent > 0) {
				int a = e10 >= 0 ? e10 / ne * ne : -((-e10 + ne - 1) / ne) * ne;

				e10 = a;
			} else
				e10 = e10 - (ne - 1);
			v = (double)((ld)v / p10(e10));
		}
		parte(v, nd, ent, sizeof ent, dec);
		/* los decimales opcionales (#) sin ceros al final */
		k = nd;
		while (k > decs && dec[k - 1] == '0')
			k--;
		dec[k] = 0;
		/* el signo */
		literal(b, s->s, ini);
		n0 = b->n;
		if (x < 0 && con_signo && (ent[0] || largo(dec) || 1)) {
			/* (un -0,00 no lleva signo) */
			int cero = 1;
			const char *q;

			for (q = ent; *q; q++)
				if (*q != '0')
					cero = 0;
			for (q = dec; *q; q++)
				if (*q != '0')
					cero = 0;
			if (!cero)
				buf_c(b, '-');
		}
		(void)n0;
		{
			/* la parte entera: con los ceros que pide y los millares */
			u32 ne = largo(ent), j;
			char t[440];
			u32 nt = 0;

			while ((int)ne < enteros && ne + 1 < sizeof ent) {
				memmove(ent + 1, ent, ne + 1);
				ent[0] = '0';
				ne++;
			}
			for (j = 0; j < ne && nt + 2 < sizeof t; j++) {
				t[nt++] = ent[j];
				if (miles && (ne - 1 - j) % 3 == 0 && j + 1 < ne)
					t[nt++] = loc->mil;
			}
			t[nt] = 0;
			buf_txt(b, t);
			if (pt && (k > 0 || decs > 0 || (decs_opc == 0 && decs == 0 && pt))) {
				if (k > 0 || decs > 0)
					buf_c(b, (u8)loc->dec);
				else if (decs_opc == 0)
					buf_c(b, (u8)loc->dec);
				buf_txt(b, dec);
			}
		}
		if (cient) {
			buf_c(b, 'E');
			if (e10 < 0)
				buf_c(b, '-');
			else if (exp_signo)
				buf_c(b, '+');
			{
				char t[16];
				int a = e10 < 0 ? -e10 : e10;

				fmt(t, sizeof t, "%0*d", exp_cifras > 0 ? exp_cifras : 1, a);
				buf_txt(b, t);
			}
			(void)pos_e;
		}
		literal(b, s->s + fin, s->n - fin);
	}
}

static void formatea_fecha(double x, const seccion *s, const cu_local *loc, bufer *b)
{
	int a, m, d, h, mi, se, ampm = 0, i;
	int ult = 0;	/* lo último que se puso: 1 hora, 2 minutos o mes */
	double frac;

	(void)loc;
	if (x < 0) {
		buf_txt(b, "########");
		return;
	}
	serie_fecha(x, &a, &m, &d);
	serie_hora(x, &h, &mi, &se);
	frac = x * 86400 - so_suelo(x * 86400);
	/* ¿lleva AM/PM? */
	for (i = 0; i + 1 < (int)s->n; i++)
		if ((letra_igual((u8)s->s[i], 'a') && letra_igual((u8)s->s[i + 1], 'm') && i + 2 < (int)s->n && s->s[i + 2] == '/') ||
		    (letra_igual((u8)s->s[i], 'a') && s->s[i + 1] == '/'))
			ampm = 1;
	for (i = 0; i < (int)s->n; i++) {
		char c = s->s[i];
		int k = 1;

		while (i + k < (int)s->n && letra_igual((u8)s->s[i + k], (u8)c))
			k++;
		if (c == '"') {
			i++;
			while (i < (int)s->n && s->s[i] != '"')
				buf_c(b, (u8)s->s[i++]);
			continue;
		}
		if (c == '\\' && i + 1 < (int)s->n) {
			buf_c(b, (u8)s->s[++i]);
			continue;
		}
		if (c == '_' || c == '*') {
			i++;
			if (c == '_')
				buf_c(b, ' ');
			continue;
		}
		if (c == '[') {
			/* [h] [mm] [ss]: lo que ha pasado entero */
			int j = i + 1;
			char q = j < (int)s->n ? s->s[j] : 0;

			while (j < (int)s->n && s->s[j] != ']')
				j++;
			if (letra_igual((u8)q, 'h'))
				buf_fmt(b, "%u", (u32)(x * 24 + 1e-9));
			else if (letra_igual((u8)q, 'm'))
				buf_fmt(b, "%u", (u32)(x * 1440 + 1e-9));
			else if (letra_igual((u8)q, 's'))
				buf_fmt(b, "%u", (u32)(x * 86400 + 0.5));
			else if (j > i + 1 && s->s[i + 1] == '$') {
				int z = i + 2;

				while (z < j && s->s[z] != '-')
					buf_c(b, (u8)s->s[z++]);
			}
			if (letra_igual((u8)q, 'h'))
				ult = 1;
			i = j;
			continue;
		}
		if (letra_igual((u8)c, 'd')) {
			if (k == 1)
				buf_fmt(b, "%d", d);
			else if (k == 2)
				buf_fmt(b, "%02d", d);
			else if (k == 3)
				buf_txt(b, DIAS_CORTOS[dia_semana(x)]);
			else
				buf_txt(b, DIAS[dia_semana(x)]);
			i += k - 1;
			continue;
		}
		if (letra_igual((u8)c, 'y') || (letra_igual((u8)c, 'a') && k >= 2)) {
			if (k <= 2)
				buf_fmt(b, "%02d", a % 100);
			else
				buf_fmt(b, "%d", a);
			i += k - 1;
			continue;
		}
		if (letra_igual((u8)c, 'h')) {
			int hh = h;

			if (ampm) {
				hh = h % 12;
				if (!hh)
					hh = 12;
			}
			if (k >= 2)
				buf_fmt(b, "%02d", hh);
			else
				buf_fmt(b, "%d", hh);
			ult = 1;
			i += k - 1;
			continue;
		}
		if (letra_igual((u8)c, 'm') && !(k == 1 && i + 1 < (int)s->n && s->s[i + 1] == '/' && ampm && i > 0 && letra_igual((u8)s->s[i - 1], 'a'))) {
			/* ¿minutos? (detrás de la hora, o delante de los segundos) */
			int min = 0, j;

			if (ult == 1 && k <= 2)
				min = 1;
			for (j = i + k; j < (int)s->n && !min; j++) {
				char z = s->s[j];

				if (letra_igual((u8)z, 's') && k <= 2)
					min = 1;
				if (letra_igual((u8)z, 'd') || letra_igual((u8)z, 'y') || letra_igual((u8)z, 'h') || letra_igual((u8)z, 'm'))
					break;
			}
			if (min)
				buf_fmt(b, k >= 2 ? "%02d" : "%d", mi);
			else if (k == 1)
				buf_fmt(b, "%d", m);
			else if (k == 2)
				buf_fmt(b, "%02d", m);
			else if (k == 3)
				buf_txt(b, MESES_CORTOS[m - 1]);
			else if (k == 4)
				buf_txt(b, MESES[m - 1]);
			else
				buf_c(b, (u8)(MESES[m - 1][0] - 32));
			ult = 2;
			i += k - 1;
			continue;
		}
		if (letra_igual((u8)c, 's')) {
			buf_fmt(b, k >= 2 ? "%02d" : "%d", se);
			i += k - 1;
			/* ,0 las décimas */
			if (i + 1 < (int)s->n && (s->s[i + 1] == '.' || s->s[i + 1] == ',') && i + 2 < (int)s->n && s->s[i + 2] == '0') {
				int nd = 0;

				i++;
				while (i + 1 < (int)s->n && s->s[i + 1] == '0') {
					nd++;
					i++;
				}
				buf_c(b, (u8)loc->dec);
				{
					double f = frac;
					int z;

					for (z = 0; z < nd; z++) {
						f *= 10;
						buf_c(b, (u8)('0' + ((int)f) % 10));
					}
				}
			}
			continue;
		}
		if (letra_igual((u8)c, 'a') && i + 1 < (int)s->n && (letra_igual((u8)s->s[i + 1], 'm') || s->s[i + 1] == '/')) {
			/* AM/PM o a/p */
			if (letra_igual((u8)s->s[i + 1], 'm')) {
				buf_txt(b, h < 12 ? "a. m." : "p. m.");
				i += 4;
			} else {
				buf_c(b, h < 12 ? 'a' : 'p');
				i += 2;
			}
			continue;
		}
		buf_c(b, (u8)c);
	}
}

void num_formatea(double x, const char *codigo, const cu_local *loc, char *d, u32 cabe, u32 *color)
{
	seccion s[4];
	int n, k = 0, con_signo = 1;
	bufer b = { 0 };
	u32 i;

	if (color)
		*color = CU_AUTO;
	if (!cabe)
		return;
	if (es_nan(x) || es_inf(x)) {
		copia(d, "#¡NUM!", cabe);
		return;
	}
	if (!codigo || !codigo[0] || iguales_sin_caja(codigo, "General")) {
		num_general(x, loc, 11, d, cabe);
		return;
	}
	n = secciones(codigo, s, 4);
	if (n >= 2 && x < 0) {
		k = 1;
		con_signo = 0;
	} else if (n >= 3 && x == 0)
		k = 2;
	/* ([>100] y otras condiciones: las de las dos primeras secciones) */
	if (n >= 2 && s[0].n && s[0].s[0] == '[' && (s[0].s[1] == '<' || s[0].s[1] == '>' || s[0].s[1] == '=')) {
		const char *p = s[0].s + 1;
		int op = 0;
		double v;

		if (p[0] == '<' && p[1] == '=')
			op = 1, p += 2;
		else if (p[0] == '>' && p[1] == '=')
			op = 2, p += 2;
		else if (p[0] == '<' && p[1] == '>')
			op = 5, p += 2;
		else if (*p == '<')
			op = 3, p++;
		else if (*p == '>')
			op = 4, p++;
		else
			op = 6, p++;
		if (num_lee_c(p, &v, NULL)) {
			int si = op == 1 ? x <= v : op == 2 ? x >= v : op == 3 ? x < v : op == 4 ? x > v : op == 5 ? x != v : x == v;

			k = si ? 0 : 1;
			con_signo = !si && n < 3 ? 1 : con_signo;
			if (si)
				con_signo = 1;
		}
	}
	if (k >= n)
		k = 0;
	/* el color de la sección */
	for (i = 0; i < s[k].n; i++)
		if (s[k].s[i] == '[') {
			u32 j = i + 1;

			while (j < s[k].n && s[k].s[j] != ']')
				j++;
			if (color && j > i + 1 && color_de(s[k].s + i + 1, j - i - 1) != CU_AUTO)
				*color = color_de(s[k].s + i + 1, j - i - 1);
			i = j;
		} else
			break;
	if (s[k].n == 0) {
		d[0] = 0;
		return;
	}
	if (s[k].n == 1 && s[k].s[0] == '@') {
		num_general(x, loc, 11, d, cabe);
		return;
	}
	if (sec_fecha(&s[k]))
		formatea_fecha(x, &s[k], loc, &b);
	else
		formatea_num(x, &s[k], loc, con_signo, &b);
	buf_c(&b, 0);
	copia(d, b.d && !b.mal ? (char *)b.d : "", cabe);
	buf_suelta(&b);
}

void num_formatea_texto(const char *t, const char *codigo, char *d, u32 cabe)
{
	seccion s[4];
	int n, i;
	const seccion *q = NULL;

	if (!codigo || !strchr(codigo, '@')) {
		copia(d, t, cabe);
		return;
	}
	n = secciones(codigo, s, 4);
	if (n == 4)
		q = &s[3];
	else
		for (i = 0; i < n; i++) {
			u32 j;

			for (j = 0; j < s[i].n; j++)
				if (s[i].s[j] == '@')
					q = &s[i];
		}
	if (!q) {
		copia(d, t, cabe);
		return;
	}
	{
		bufer b = { 0 };
		u32 j;

		for (j = 0; j < q->n; j++) {
			if (q->s[j] == '@')
				buf_txt(&b, t);
			else if (q->s[j] == '"') {
				j++;
				while (j < q->n && q->s[j] != '"')
					buf_c(&b, (u8)q->s[j++]);
			} else if (q->s[j] == '\\' && j + 1 < q->n)
				buf_c(&b, (u8)q->s[++j]);
			else if (q->s[j] == '_' && j + 1 < q->n) {
				j++;
				buf_c(&b, ' ');
			} else if (q->s[j] == '[') {
				while (j < q->n && q->s[j] != ']')
					j++;
			} else
				buf_c(&b, (u8)q->s[j]);
		}
		buf_c(&b, 0);
		copia(d, b.d ? (char *)b.d : t, cabe);
		buf_suelta(&b);
	}
}

void num_edicion(double x, const char *codigo, const cu_local *loc, char *d, u32 cabe)
{
	int c = fmt_categoria(codigo);

	if (c == CAT_FECHA || c == CAT_HORA) {
		double f = x - so_suelo(x);
		int h, mi, se;

		serie_hora(x, &h, &mi, &se);
		if (c == CAT_HORA && x < 1 && x >= 0) {
			num_formatea(x, "h:mm:ss", loc, d, cabe, NULL);
			return;
		}
		if (f > 1e-9 && !(h == 0 && mi == 0 && se == 0))
			num_formatea(x, se ? "dd/mm/yyyy h:mm:ss" : "dd/mm/yyyy h:mm", loc, d, cabe, NULL);
		else
			num_formatea(x, "dd/mm/yyyy", loc, d, cabe, NULL);
		return;
	}
	if (c == CAT_PORCENTAJE) {
		char t[64];
		u32 k;

		num_general(x * 100, loc, 20, t, sizeof t);
		k = largo(t);
		if (k + 2 < sizeof t) {
			t[k] = '%';
			t[k + 1] = 0;
		}
		copia(d, t, cabe);
		return;
	}
	{
		/* todas las cifras (15), sin millares */
		char dig[24];
		int e;

		if (x == 0) {
			copia(d, "0", cabe);
			return;
		}
		cifras(x < 0 ? -x : x, 15, dig, &e);
		escribe_cifras(dig, 15, e, x < 0, loc->dec, d, cabe);
	}
}

void num_texto_funcion(double x, const char *codigo_local, const cu_local *loc, char *d, u32 cabe)
{
	char c[200];

	fmt_de_local(codigo_local, loc, c, sizeof c);
	num_formatea(x, c, loc, d, cabe, NULL);
}
