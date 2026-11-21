/* matriz.c - HispaGL: las matrices (modelo-vista, proyección y la de textura
 * de cada unidad), sus pilas, glViewport y glDepthRange.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "hgl.h"

void mat_identidad(Mat *m)
{
	int k;
	for (k = 0; k < 16; k++) m->m[k] = (k % 5) ? 0.0f : 1.0f;
}

void mat_mul(Mat *r, const Mat *a, const Mat *b)
{
	Mat t;
	int i, j;
	for (j = 0; j < 4; j++)
		for (i = 0; i < 4; i++)
			t.m[j * 4 + i] = a->m[i] * b->m[j * 4] + a->m[4 + i] * b->m[j * 4 + 1] +
				a->m[8 + i] * b->m[j * 4 + 2] + a->m[12 + i] * b->m[j * 4 + 3];
	*r = t;
}

/* la inversa (Gauss-Jordan con pivote): 0 si no la tiene */
int mat_inversa(Mat *r, const Mat *a)
{
	float m[4][8];
	int i, j, k;
	for (i = 0; i < 4; i++)
		for (j = 0; j < 4; j++) {
			m[i][j] = a->m[j * 4 + i];
			m[i][j + 4] = i == j;
		}
	for (k = 0; k < 4; k++) {
		int p = k;
		float mx = hgl_abs(m[k][k]), d;
		for (i = k + 1; i < 4; i++)
			if (hgl_abs(m[i][k]) > mx) { mx = hgl_abs(m[i][k]); p = i; }
		if (mx < 1e-20f) return 0;
		if (p != k)
			for (j = 0; j < 8; j++) { float t = m[k][j]; m[k][j] = m[p][j]; m[p][j] = t; }
		d = 1.0f / m[k][k];
		for (j = 0; j < 8; j++) m[k][j] *= d;
		for (i = 0; i < 4; i++)
			if (i != k && m[i][k] != 0) {
				float f = m[i][k];
				for (j = 0; j < 8; j++) m[i][j] -= f * m[k][j];
			}
	}
	for (i = 0; i < 4; i++)
		for (j = 0; j < 4; j++) r->m[j * 4 + i] = m[i][j + 4];
	return 1;
}

void hgl_matrices_ini(Ctx *C)
{
	int u;
	C->nmv = C->npr = 0;
	mat_identidad(&C->mv[0]);
	mat_identidad(&C->pr[0]);
	for (u = 0; u < UNIDADES; u++) { C->ntx[u] = 0; mat_identidad(&C->tx[u][0]); C->tx_ident[u] = 1; }
	C->trf.modo = GL_MODELVIEW;
	C->sucio_mvp = C->sucio_nor = 1;
}

Mat *hgl_mat_actual(Ctx *C)
{
	switch (C->trf.modo) {
	case GL_PROJECTION: return &C->pr[C->npr];
	case GL_TEXTURE: return &C->tx[C->tex.activa][C->ntx[C->tex.activa]];
	default: return &C->mv[C->nmv];
	}
}

static int es_ident(const Mat *m)
{
	int k;
	for (k = 0; k < 16; k++)
		if (m->m[k] != ((k % 5) ? 0.0f : 1.0f)) return 0;
	return 1;
}

static void cambia(Ctx *C)
{
	if (C->trf.modo == GL_MODELVIEW) C->sucio_mvp = C->sucio_nor = 1;
	else if (C->trf.modo == GL_PROJECTION) C->sucio_mvp = 1;
	else C->tx_ident[C->tex.activa] = es_ident(&C->tx[C->tex.activa][C->ntx[C->tex.activa]]);
}

/* la de proyección·modelo-vista y la de las normales, si han cambiado */
void hgl_mat_al_dia(Ctx *C)
{
	if (C->sucio_mvp) {
		mat_mul(&C->mvp, &C->pr[C->npr], &C->mv[C->nmv]);
		C->sucio_mvp = 0;
	}
	if (C->sucio_nor) {
		/* la inversa traspuesta de la parte 3×3 de la modelo-vista */
		const float *m = C->mv[C->nmv].m;
		float a = m[0], b = m[4], c = m[8], d = m[1], e = m[5], f = m[9], g = m[2], h = m[6], i = m[10];
		float A = e * i - f * h, B = -(d * i - f * g), Cc = d * h - e * g;
		float det = a * A + b * B + c * Cc;
		float *n = C->nor;
		if (det == 0) det = 1;
		det = 1.0f / det;
		/* n[fila*3+col]: normal_ojo = n · normal */
		n[0] = A * det; n[1] = B * det; n[2] = Cc * det;
		n[3] = -(b * i - c * h) * det; n[4] = (a * i - c * g) * det; n[5] = -(a * h - b * g) * det;
		n[6] = (b * f - c * e) * det; n[7] = -(a * f - c * d) * det; n[8] = (a * e - b * d) * det;
		C->sucio_nor = 0;
	}
}

void i_mat_modo(u32 m)
{
	CTX();
	if (m != GL_MODELVIEW && m != GL_PROJECTION && m != GL_TEXTURE) { hgl_error(C, GL_INVALID_ENUM); return; }
	C->trf.modo = m;
}
void i_mat_carga(const float *m)
{
	CTX();
	Mat *a = hgl_mat_actual(C);
	int k;
	for (k = 0; k < 16; k++) a->m[k] = m[k];
	cambia(C);
}
void i_mat_mul(const float *m)
{
	CTX();
	Mat b, *a = hgl_mat_actual(C);
	int k;
	for (k = 0; k < 16; k++) b.m[k] = m[k];
	mat_mul(a, a, &b);
	cambia(C);
}
void i_mat_ident(void)
{
	CTX();
	mat_identidad(hgl_mat_actual(C));
	cambia(C);
}
void i_mat_empuja(void)
{
	CTX();
	switch (C->trf.modo) {
	case GL_PROJECTION:
		if (C->npr + 1 >= PILA_PR) { hgl_error(C, GL_STACK_OVERFLOW); return; }
		C->pr[C->npr + 1] = C->pr[C->npr]; C->npr++;
		break;
	case GL_TEXTURE: {
		int u = C->tex.activa;
		if (C->ntx[u] + 1 >= PILA_TX) { hgl_error(C, GL_STACK_OVERFLOW); return; }
		C->tx[u][C->ntx[u] + 1] = C->tx[u][C->ntx[u]]; C->ntx[u]++;
		break;
	}
	default:
		if (C->nmv + 1 >= PILA_MV) { hgl_error(C, GL_STACK_OVERFLOW); return; }
		C->mv[C->nmv + 1] = C->mv[C->nmv]; C->nmv++;
	}
}
void i_mat_saca(void)
{
	CTX();
	switch (C->trf.modo) {
	case GL_PROJECTION:
		if (!C->npr) { hgl_error(C, GL_STACK_UNDERFLOW); return; }
		C->npr--;
		break;
	case GL_TEXTURE:
		if (!C->ntx[C->tex.activa]) { hgl_error(C, GL_STACK_UNDERFLOW); return; }
		C->ntx[C->tex.activa]--;
		break;
	default:
		if (!C->nmv) { hgl_error(C, GL_STACK_UNDERFLOW); return; }
		C->nmv--;
	}
	cambia(C);
}
void i_rota(float ang, float x, float y, float z)
{
	float m[16], l = hgl_raiz(x * x + y * y + z * z), s, c, t;
	if (l == 0) return;
	x /= l; y /= l; z /= l;
	ang *= 3.14159265358979f / 180.0f;
	s = hgl_seno(ang); c = hgl_coseno(ang); t = 1 - c;
	m[0] = x * x * t + c;     m[4] = x * y * t - z * s; m[8] = x * z * t + y * s;  m[12] = 0;
	m[1] = y * x * t + z * s; m[5] = y * y * t + c;     m[9] = y * z * t - x * s;  m[13] = 0;
	m[2] = x * z * t - y * s; m[6] = y * z * t + x * s; m[10] = z * z * t + c;     m[14] = 0;
	m[3] = 0; m[7] = 0; m[11] = 0; m[15] = 1;
	i_mat_mul(m);
}
void i_traslada(float x, float y, float z)
{
	CTX();
	float *a = hgl_mat_actual(C)->m;
	a[12] += a[0] * x + a[4] * y + a[8] * z;
	a[13] += a[1] * x + a[5] * y + a[9] * z;
	a[14] += a[2] * x + a[6] * y + a[10] * z;
	a[15] += a[3] * x + a[7] * y + a[11] * z;
	cambia(C);
}
void i_escala(float x, float y, float z)
{
	CTX();
	float *a = hgl_mat_actual(C)->m;
	int k;
	for (k = 0; k < 4; k++) { a[k] *= x; a[4 + k] *= y; a[8 + k] *= z; }
	cambia(C);
}
void i_frustum(float l, float r, float b, float t, float n, float f)
{
	float m[16];
	CTX();
	if (n <= 0 || f <= 0 || l == r || b == t || n == f) { hgl_error(C, GL_INVALID_VALUE); return; }
	hgl_pon(m, 0, sizeof m);
	m[0] = 2 * n / (r - l);
	m[5] = 2 * n / (t - b);
	m[8] = (r + l) / (r - l);
	m[9] = (t + b) / (t - b);
	m[10] = -(f + n) / (f - n);
	m[11] = -1;
	m[14] = -2 * f * n / (f - n);
	i_mat_mul(m);
}
void i_orto(float l, float r, float b, float t, float n, float f)
{
	float m[16];
	CTX();
	if (l == r || b == t || n == f) { hgl_error(C, GL_INVALID_VALUE); return; }
	hgl_pon(m, 0, sizeof m);
	m[0] = 2 / (r - l);
	m[5] = 2 / (t - b);
	m[10] = -2 / (f - n);
	m[12] = -(r + l) / (r - l);
	m[13] = -(t + b) / (t - b);
	m[14] = -(f + n) / (f - n);
	m[15] = 1;
	i_mat_mul(m);
}
void i_vista(int x, int y, int w, int h)
{
	CTX();
	if (w < 0 || h < 0) { hgl_error(C, GL_INVALID_VALUE); return; }
	if (w > 4096) w = 4096;
	if (h > 4096) h = 4096;
	C->vis.x = x; C->vis.y = y; C->vis.w = w; C->vis.h = h;
}
void i_prof_rango(float n, float f)
{
	CTX();
	C->vis.cerca = hgl_lim01(n);
	C->vis.lejos = hgl_lim01(f);
}

/* ---- las de fuera ---- */
void APIENTRY glMatrixMode(GLenum m) { CTX(); LISTA(i_mat_modo, 1, m); i_mat_modo(m); }
void APIENTRY glLoadMatrixf(const GLfloat *m)
{
	CTX();
	if (C->lista_n) {
		hgl_graba_v(C, (void *)i_mat_carga, m, 16);
		if (C->lista_modo == GL_COMPILE) return;
	}
	i_mat_carga(m);
}
void APIENTRY glLoadMatrixd(const GLdouble *m) { float f[16]; int k; for (k = 0; k < 16; k++) f[k] = (float)m[k]; glLoadMatrixf(f); }
void APIENTRY glMultMatrixf(const GLfloat *m)
{
	CTX();
	if (C->lista_n) {
		hgl_graba_v(C, (void *)i_mat_mul, m, 16);
		if (C->lista_modo == GL_COMPILE) return;
	}
	i_mat_mul(m);
}
void APIENTRY glMultMatrixd(const GLdouble *m) { float f[16]; int k; for (k = 0; k < 16; k++) f[k] = (float)m[k]; glMultMatrixf(f); }
void APIENTRY glLoadIdentity(void) { CTX(); LISTA0(i_mat_ident); i_mat_ident(); }
void APIENTRY glPushMatrix(void) { CTX(); LISTA0(i_mat_empuja); i_mat_empuja(); }
void APIENTRY glPopMatrix(void) { CTX(); LISTA0(i_mat_saca); i_mat_saca(); }
void APIENTRY glRotatef(GLfloat a, GLfloat x, GLfloat y, GLfloat z) { CTX(); LISTA(i_rota, 4, F(a), F(x), F(y), F(z)); i_rota(a, x, y, z); }
void APIENTRY glRotated(GLdouble a, GLdouble x, GLdouble y, GLdouble z) { glRotatef((float)a, (float)x, (float)y, (float)z); }
void APIENTRY glTranslatef(GLfloat x, GLfloat y, GLfloat z) { CTX(); LISTA(i_traslada, 3, F(x), F(y), F(z)); i_traslada(x, y, z); }
void APIENTRY glTranslated(GLdouble x, GLdouble y, GLdouble z) { glTranslatef((float)x, (float)y, (float)z); }
void APIENTRY glScalef(GLfloat x, GLfloat y, GLfloat z) { CTX(); LISTA(i_escala, 3, F(x), F(y), F(z)); i_escala(x, y, z); }
void APIENTRY glScaled(GLdouble x, GLdouble y, GLdouble z) { glScalef((float)x, (float)y, (float)z); }
void APIENTRY glFrustum(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f)
{
	CTX();
	LISTA(i_frustum, 6, F((float)l), F((float)r), F((float)b), F((float)t), F((float)n), F((float)f));
	i_frustum((float)l, (float)r, (float)b, (float)t, (float)n, (float)f);
}
void APIENTRY glOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f)
{
	CTX();
	LISTA(i_orto, 6, F((float)l), F((float)r), F((float)b), F((float)t), F((float)n), F((float)f));
	i_orto((float)l, (float)r, (float)b, (float)t, (float)n, (float)f);
}
void APIENTRY glViewport(GLint x, GLint y, GLsizei w, GLsizei h) { CTX(); LISTA(i_vista, 4, x, y, w, h); i_vista(x, y, w, h); }
void APIENTRY glDepthRange(GLclampd n, GLclampd f) { CTX(); LISTA(i_prof_rango, 2, F((float)n), F((float)f)); i_prof_rango((float)n, (float)f); }
