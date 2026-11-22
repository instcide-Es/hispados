/* dsp.c - el Reproductor: remuestrear, la velocidad sin cambiar el tono, el
 * ecualizador y el espectro (ver dsp.h). Portable.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include <math.h>
#include "dsp.h"

#define PI 3.14159265358979323846

float potf_(float x, float y)
{
	return (float)pow((double)x, (double)y);
}

float db_a_veces(int c)
{
	return (float)pow(10.0, (double)c / 2000.0);
}

/* ============================================================ remuestrear */
static double bessel0(double x)
{
	double s = 1, t = 1;
	int k;

	for (k = 1; k < 30; k++) {
		t *= (x / (2 * k)) * (x / (2 * k));
		s += t;
		if (t < 1e-12 * s)
			break;
	}
	return s;
}

int rm_prepara(remuestreo *r, u32 de, u32 a)
{
	int f, k;
	double fc, beta = 7.0, i0b;

	so_suelta(r->ent);
	memset(r, 0, sizeof *r);
	r->de = de;
	r->a = a;
	r->directo = de == a;
	r->paso = (u64)(((double)de / (double)a) * 4294967296.0);
	r->fase = (u64)(RM_TAPS / 2 - 1) << 32;
	fc = de > a ? (double)a / de : 1.0;
	fc *= 0.91;
	i0b = bessel0(beta);
	for (f = 0; f <= RM_FASES; f++) {
		double fr = (double)f / RM_FASES, suma = 0;

		for (k = 0; k < RM_TAPS; k++) {
			double x = k - (RM_TAPS / 2 - 1) - fr, s, w, q = x / (RM_TAPS / 2);

			s = x == 0 ? fc : sin(PI * fc * x) / (PI * x);
			w = q * q < 1 ? bessel0(beta * sqrt(1 - q * q)) / i0b : 0;
			r->h[f][k] = (float)(s * w);
			suma += s * w;
		}
		for (k = 0; k < RM_TAPS; k++)
			r->h[f][k] = (float)(r->h[f][k] / suma);
	}
	r->cabe_ent = 8192;
	r->ent = so_pide((size_t)r->cabe_ent * 2 * sizeof(float));
	if (!r->ent)
		return -1;
	memset(r->ent, 0, (size_t)(RM_TAPS / 2 - 1) * 2 * sizeof(float));
	r->en_ent = RM_TAPS / 2 - 1;
	return 0;
}

void rm_vacia(remuestreo *r)
{
	if (!r->ent)
		return;
	memset(r->ent, 0, (size_t)(RM_TAPS / 2 - 1) * 2 * sizeof(float));
	r->en_ent = RM_TAPS / 2 - 1;
	r->fase = (u64)(RM_TAPS / 2 - 1) << 32;
}

void rm_suelta(remuestreo *r)
{
	so_suelta(r->ent);
	r->ent = NULL;
}

int rm_hace(remuestreo *r, const float *e, int n, float *s, int cabe)
{
	int hechas = 0;

	if (r->directo) {
		/* (también aquí puede quedar algo de antes: lo que no cupo) */
		if (n > 0) {
			if (r->en_ent + n > r->cabe_ent) {
				int c2 = (r->en_ent + n) * 2;
				float *ne = so_repide(r->ent, (size_t)c2 * 2 * sizeof(float));

				if (!ne)
					return 0;
				r->ent = ne;
				r->cabe_ent = c2;
			}
			memcpy(r->ent + 2 * r->en_ent, e, (size_t)n * 2 * sizeof(float));
			r->en_ent += n;
		}
		n = r->en_ent - (RM_TAPS / 2 - 1);
		if (n > cabe)
			n = cabe;
		if (n <= 0)
			return 0;
		memcpy(s, r->ent + 2 * (RM_TAPS / 2 - 1), (size_t)n * 2 * sizeof(float));
		memmove(r->ent + 2 * (RM_TAPS / 2 - 1), r->ent + 2 * (RM_TAPS / 2 - 1 + n), (size_t)(r->en_ent - (RM_TAPS / 2 - 1) - n) * 2 * sizeof(float));
		r->en_ent -= n;
		return n;
	}
	/* lo que entra, detrás de lo que había */
	if (r->en_ent + n > r->cabe_ent) {
		int cabe2 = (r->en_ent + n) * 2;
		float *ne = so_repide(r->ent, (size_t)cabe2 * 2 * sizeof(float));

		if (!ne)
			return 0;
		r->ent = ne;
		r->cabe_ent = cabe2;
	}
	if (n > 0)
		memcpy(r->ent + 2 * r->en_ent, e, (size_t)n * 2 * sizeof(float));
	r->en_ent += n;
	for (;;) {
		u32 c = (u32)(r->fase >> 32);
		int ph = (int)((r->fase >> 24) & (RM_FASES - 1)), k;
		const float *h, *x;
		float l = 0, d = 0;

		if ((int)c + RM_TAPS / 2 + 1 > r->en_ent || hechas >= cabe)
			break;
		h = r->h[ph];
		x = r->ent + 2 * ((int)c - (RM_TAPS / 2 - 1));
		for (k = 0; k < RM_TAPS; k++) {
			l += h[k] * x[2 * k];
			d += h[k] * x[2 * k + 1];
		}
		s[2 * hechas] = l;
		s[2 * hechas + 1] = d;
		hechas++;
		r->fase += r->paso;
	}
	/* lo que ya no hace falta, fuera (se dejan las de antes del centro) */
	{
		int c = (int)(r->fase >> 32), quita = c - (RM_TAPS / 2 - 1);

		if (quita > 0) {
			if (quita > r->en_ent)
				quita = r->en_ent;
			memmove(r->ent, r->ent + 2 * quita, (size_t)(r->en_ent - quita) * 2 * sizeof(float));
			r->en_ent -= quita;
			r->fase -= (u64)quita << 32;
		}
	}
	return hechas;
}

/* ============================================================ la velocidad */
int es_prepara(estira *e, u32 hz)
{
	int i;

	es_suelta(e);
	memset(e, 0, sizeof *e);
	e->hz = hz;
	e->velocidad = 1;
	e->n = (int)(hz / 25) & ~1;	/* 40 ms */
	e->hop = e->n / 2;
	e->busca = (int)(hz / 80);	/* 12,5 ms a cada lado */
	e->cabe_ent = e->n * 8 + e->busca * 4;
	e->ent = so_pide((size_t)e->cabe_ent * 2 * sizeof(float));
	e->cola = so_pide((size_t)e->hop * 2 * sizeof(float));
	e->ventana = so_pide((size_t)e->n * sizeof(float));
	if (!e->ent || !e->cola || !e->ventana)
		return -1;
	for (i = 0; i < e->n; i++)
		e->ventana[i] = (float)(0.5 - 0.5 * cos(2 * PI * i / e->n));
	es_vacia(e);
	return 0;
}

void es_vacia(estira *e)
{
	e->en_ent = 0;
	e->pos = e->busca;
	e->primero = 1;
	if (e->cola)
		memset(e->cola, 0, (size_t)e->hop * 2 * sizeof(float));
}

void es_velocidad(estira *e, float v)
{
	if (v < 0.5f)
		v = 0.5f;
	if (v > 2.0f)
		v = 2.0f;
	if ((v == 1.0f) != (e->velocidad == 1.0f))
		es_vacia(e);
	e->velocidad = v;
}

void es_suelta(estira *e)
{
	so_suelta(e->ent);
	so_suelta(e->cola);
	so_suelta(e->ventana);
	e->ent = e->cola = e->ventana = NULL;
}

/* dónde encaja mejor (de -busca a +busca) el trozo que empieza cerca de p
 * con lo que seguía al de antes (en «nat»): la correlación normalizada, de
 * cuatro en cuatro y luego fina */
static int mejor_sitio(estira *e, int p, int nat)
{
	const float *x = e->ent;
	int lag, mejor = 0, paso, desde, hasta, k;
	double best = -1e30;

	for (paso = 8; paso >= 1; paso = paso == 8 ? 2 : paso == 2 ? 1 : 0) {
		if (paso == 8)
			desde = -e->busca, hasta = e->busca;
		else
			desde = mejor - paso * 4, hasta = mejor + paso * 4;
		for (lag = desde; lag <= hasta; lag += paso) {
			double c = 0, en = 1e-9;
			int q = p + lag;

			if (q < 0 || q + e->hop > e->en_ent)
				continue;
			for (k = 0; k < e->hop; k += 4) {
				float a = x[2 * (q + k)] + x[2 * (q + k) + 1], b = x[2 * (nat + k)] + x[2 * (nat + k) + 1];

				c += a * b;
				en += a * a;
			}
			c /= sqrt(en);
			if (c > best) {
				best = c;
				mejor = lag;
			}
		}
		if (paso == 1)
			break;
	}
	return mejor;
}

int es_hace(estira *e, const float *ent, int n, float *s, int cabe)
{
	int hechas = 0, i;

	if (e->velocidad == 1.0f) {
		if (n > cabe)
			n = cabe;
		if (n > 0)
			memcpy(s, ent, (size_t)n * 2 * sizeof(float));
		return n;
	}
	if (e->en_ent + n > e->cabe_ent) {
		int c2 = (e->en_ent + n) * 2;
		float *ne = so_repide(e->ent, (size_t)c2 * 2 * sizeof(float));

		if (!ne)
			return 0;
		e->ent = ne;
		e->cabe_ent = c2;
	}
	if (n > 0)
		memcpy(e->ent + 2 * e->en_ent, ent, (size_t)n * 2 * sizeof(float));
	e->en_ent += n;
	for (;;) {
		int p = (int)e->pos, q = p;

		if (p + e->busca + e->n > e->en_ent || hechas + e->hop > cabe)
			break;
		/* donde mejor encaja con lo que seguía al de antes (prev + hop) */
		if (!e->primero && e->prev + e->n <= e->en_ent)
			q = p + mejor_sitio(e, p, e->prev + e->hop);
		if (q < 0)
			q = 0;
		/* la salida: la cola de antes + la primera mitad de este */
		for (i = 0; i < e->hop; i++) {
			float w = e->ventana[i];

			s[2 * (hechas + i)] = e->cola[2 * i] + e->ent[2 * (q + i)] * w;
			s[2 * (hechas + i) + 1] = e->cola[2 * i + 1] + e->ent[2 * (q + i) + 1] * w;
		}
		for (i = 0; i < e->hop; i++) {
			float w = e->ventana[e->hop + i];

			e->cola[2 * i] = e->ent[2 * (q + e->hop + i)] * w;
			e->cola[2 * i + 1] = e->ent[2 * (q + e->hop + i) + 1] * w;
		}
		e->primero = 0;
		e->prev = q;
		hechas += e->hop;
		/* el siguiente: donde quedó este, más lo que avanza la entrada */
		e->pos += e->hop * e->velocidad;
		/* (y lo que ya no hace falta, fuera) */
		{
			int quita = (int)e->pos - e->busca;

			if (quita > e->prev)
				quita = e->prev;
			if (quita > e->n * 2) {
				memmove(e->ent, e->ent + 2 * quita, (size_t)(e->en_ent - quita) * 2 * sizeof(float));
				e->en_ent -= quita;
				e->pos -= quita;
				e->prev -= quita;
			}
		}
	}
	return hechas;
}

/* ============================================================ el ecualizador */
const int frecuencias_eq[BANDAS] = { 60, 170, 310, 600, 1000, 3000, 6000, 12000, 14000, 16000 };

/* los preajustes (en dB: el preamplificador y las diez bandas), nuestros */
const preajuste preajustes[] = {
	{ "Plano", { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{ "Rock", { -3, 5, 3, -3, -5, -2, 2, 5, 6, 6, 6 } },
	{ "Pop", { -2, -1, 3, 5, 5, 3, 0, -1, -1, -1, -1 } },
	{ "Clásica", { 0, 0, 0, 0, 0, 0, 0, -4, -4, -4, -6 } },
	{ "Jazz", { -1, 3, 2, 0, 1, -1, -1, 0, 2, 3, 3 } },
	{ "Voz", { -2, -4, -3, 0, 3, 4, 4, 3, 1, 0, -2 } },
	{ "Graves", { -5, 7, 6, 4, 1, 0, 0, 0, 0, 0, 0 } },
	{ "Agudos", { -5, 0, 0, 0, 0, 0, 1, 4, 6, 7, 7 } },
	{ "Graves y agudos", { -5, 6, 4, 0, -3, -2, 0, 4, 6, 6, 6 } },
	{ "Baile", { -4, 7, 5, 2, 0, 0, -2, -3, -3, 0, 0 } },
	{ "Directo", { -2, -3, 0, 3, 4, 4, 4, 3, 2, 2, 2 } },
	{ "Fiesta", { -4, 5, 5, 0, 0, 0, 0, 0, 0, 5, 5 } },
	{ "Suave", { -2, 3, 1, 0, -1, 0, 3, 5, 6, 6, 7 } },
	{ "Reggae", { -2, 0, 0, 0, -4, 0, 5, 5, 0, 0, 0 } },
	{ "Ska", { -2, -2, -3, -2, 0, 3, 4, 6, 6, 7, 6 } },
	{ "Techno", { -4, 6, 5, 0, -4, -3, 0, 6, 6, 6, 5 } },
	{ "Auriculares", { -4, 4, 6, 3, -2, -1, 2, 4, 6, 7, 7 } },
	{ "Altavoz pequeño", { -5, 8, 6, 4, 2, 0, -2, -3, -4, -4, -5 } },
};
const int n_preajustes = (int)(sizeof preajustes / sizeof preajustes[0]);

void eq_prepara(ecualizador *q, u32 hz)
{
	q->hz = hz;
	eq_cambia(q);
	eq_vacia(q);
}

void eq_vacia(ecualizador *q)
{
	memset(q->z, 0, sizeof q->z);
}

/* filtros de pico (los de Robert Bristow-Johnson), con Q de 1,4 */
void eq_cambia(ecualizador *q)
{
	int b;

	for (b = 0; b < BANDAS; b++) {
		double f = frecuencias_eq[b], A, w, al, a0, Q = 1.4;

		if (f > q->hz * 0.45)
			f = q->hz * 0.45;
		A = pow(10.0, q->banda[b] / 400.0);	/* (décimas de dB: /20 /10 /2) */
		w = 2 * PI * f / q->hz;
		al = sin(w) / (2 * Q);
		a0 = 1 + al / A;
		q->b0[b] = (float)((1 + al * A) / a0);
		q->b1[b] = (float)((-2 * cos(w)) / a0);
		q->b2[b] = (float)((1 - al * A) / a0);
		q->a1[b] = q->b1[b];
		q->a2[b] = (float)((1 - al / A) / a0);
	}
	q->g_pre = (float)pow(10.0, q->preamp / 200.0);
	q->calculado = 1;
}

void eq_hace(ecualizador *q, float *s, int tramas)
{
	int b, i, c;

	if (!q->activo)
		return;
	if (q->preamp)
		for (i = 0; i < 2 * tramas; i++)
			s[i] *= q->g_pre;
	for (b = 0; b < BANDAS; b++) {
		float b0, b1, b2, a1, a2;

		if (!q->banda[b])
			continue;
		b0 = q->b0[b], b1 = q->b1[b], b2 = q->b2[b], a1 = q->a1[b], a2 = q->a2[b];
		for (c = 0; c < 2; c++) {
			float z1 = q->z[b][c][0], z2 = q->z[b][c][1];
			float *p = s + c;

			/* (forma directa II traspuesta) */
			for (i = 0; i < tramas; i++, p += 2) {
				float x = *p, y = b0 * x + z1;

				z1 = b1 * x - a1 * y + z2;
				z2 = b2 * x - a2 * y;
				*p = y;
			}
			/* (que no queden números «desnormalizados»: lentísimos en el x87) */
			if (z1 < 1e-20f && z1 > -1e-20f)
				z1 = 0;
			if (z2 < 1e-20f && z2 > -1e-20f)
				z2 = 0;
			q->z[b][c][0] = z1;
			q->z[b][c][1] = z2;
		}
	}
}

/* ============================================================ el espectro */
void espectro(const float *mono, float *mag)
{
	static float re[FFT_N], im[FFT_N], cs[FFT_N / 2], sn[FFT_N / 2], ven[FFT_N];
	static int hecho;
	int i, j, k, m, len;

	if (!hecho) {
		for (i = 0; i < FFT_N / 2; i++) {
			cs[i] = (float)cos(2 * PI * i / FFT_N);
			sn[i] = (float)-sin(2 * PI * i / FFT_N);
		}
		for (i = 0; i < FFT_N; i++)
			ven[i] = (float)(0.5 - 0.5 * cos(2 * PI * i / (FFT_N - 1)));
		hecho = 1;
	}
	/* en el orden de los bits al revés */
	for (i = 0, j = 0; i < FFT_N; i++) {
		re[j] = mono[i] * ven[i];
		im[j] = 0;
		for (m = FFT_N >> 1; m >= 1 && (j & m); m >>= 1)
			j ^= m;
		j |= m;
	}
	for (len = 2; len <= FFT_N; len <<= 1) {
		int salto = FFT_N / len;

		for (i = 0; i < FFT_N; i += len)
			for (k = 0; k < len / 2; k++) {
				float wr = cs[k * salto], wi = sn[k * salto];
				int a = i + k, b = i + k + len / 2;
				float tr = re[b] * wr - im[b] * wi, ti = re[b] * wi + im[b] * wr;

				re[b] = re[a] - tr;
				im[b] = im[a] - ti;
				re[a] += tr;
				im[a] += ti;
			}
	}
	for (i = 0; i < FFT_N / 2; i++)
		mag[i] = (float)sqrt(re[i] * re[i] + im[i] * im[i]) * (4.0f / FFT_N);
}
