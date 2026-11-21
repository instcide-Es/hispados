/* tests/gl/escenas.c - las escenas de prueba de HispaGL (docs/486/OPENGL.md):
 * el mismo código se dibuja con HispaGL (ejecuta_hgl.c, 32 bits) y con Mesa
 * (ejecuta_mesa.c, la referencia de fuera), y se comparan las imágenes
 * (compara.py). Solo usa OpenGL 1.1 y GL_ARB_multitexture; sin biblioteca de
 * C (las texturas se hacen aquí, y el seno también).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "GL/gl.h"
#include "escenas.h"

#define PI 3.14159265358979f
static float sen(float x)
{
	float x2, r;
	while (x > PI) x -= 2 * PI;
	while (x < -PI) x += 2 * PI;
	x2 = x * x;
	r = x * (1 - x2 / 6 * (1 - x2 / 20 * (1 - x2 / 42 * (1 - x2 / 72 * (1 - x2 / 110)))));
	return r;
}
static float cosn(float x) { return sen(x + PI / 2); }
static float raiz(float x) { float r = x > 1 ? x : 1; int k; if (x <= 0) return 0; for (k = 0; k < 30; k++) r = 0.5f * (r + x / r); return r; }

static void perspectiva(float fov, float asp, float n, float f)
{
	float t = n * sen(fov * PI / 360) / cosn(fov * PI / 360);
	glFrustum(-t * asp, t * asp, -t, t, n, f);
}

/* ---- texturas hechas aquí ---- */
static unsigned char img[256 * 256 * 4];
static void ajedrez(int w, int h, int c, const unsigned char *a, const unsigned char *b, int alfa_agujero)
{
	int x, y;
	for (y = 0; y < h; y++)
		for (x = 0; x < w; x++) {
			const unsigned char *s = (((x / c) + (y / c)) & 1) ? a : b;
			unsigned char *o = img + (y * w + x) * 4;
			o[0] = s[0]; o[1] = s[1]; o[2] = s[2]; o[3] = 255;
			/* un degradado suave encima, para que se note el filtro */
			o[0] = (unsigned char)((o[0] * 3 + x * 255 / w) / 4);
			if (alfa_agujero) {
				int dx = (x % 32) - 16, dy = (y % 32) - 16;
				o[3] = dx * dx + dy * dy < 64 ? 0 : 255;
			}
		}
}
/* los niveles de mipmap: la media de 2×2 */
static void sube_con_mipmaps(int w, int h, int fi, unsigned fmt)
{
	int nivel = 0;
	glTexImage2D(GL_TEXTURE_2D, 0, fi, w, h, 0, fmt, GL_UNSIGNED_BYTE, img);
	while (w > 1 || h > 1) {
		int nw = w > 1 ? w / 2 : 1, nh = h > 1 ? h / 2 : 1, x, y, k;
		for (y = 0; y < nh; y++)
			for (x = 0; x < nw; x++)
				for (k = 0; k < 4; k++) {
					int x0 = x * 2, y0 = y * 2, x1 = w > 1 ? x0 + 1 : x0, y1 = h > 1 ? y0 + 1 : y0;
					int s = img[(y0 * w + x0) * 4 + k] + img[(y0 * w + x1) * 4 + k] + img[(y1 * w + x0) * 4 + k] + img[(y1 * w + x1) * 4 + k];
					img[(y * nw + x) * 4 + k] = (unsigned char)((s + 2) / 4);
				}
		w = nw; h = nh; nivel++;
		glTexImage2D(GL_TEXTURE_2D, nivel, fi, w, h, 0, fmt, GL_UNSIGNED_BYTE, img);
	}
}

static void base(int w, int h, float r, float g, float b)
{
	glViewport(0, 0, w, h);
	glClearColor(r, g, b, 1);
	glClearDepth(1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	perspectiva(60, (float)w / h, 1, 100);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

/* 1: triángulos de colores, Gouraud y plano, que se cruzan (la profundidad) */
static void triangulos(int w, int h)
{
	base(w, h, 0.1f, 0.1f, 0.2f);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glShadeModel(GL_SMOOTH);
	glTranslatef(0, 0, -5);
	glBegin(GL_TRIANGLES);
	glColor3f(1, 0, 0); glVertex3f(-2, -1.5f, -1);
	glColor3f(0, 1, 0); glVertex3f(2, -1.2f, 1);
	glColor3f(0, 0, 1); glVertex3f(0, 1.8f, 0);
	glColor3f(1, 1, 0); glVertex3f(-2, 1.2f, 1);
	glColor3f(1, 1, 0); glVertex3f(2, 1.0f, -1);
	glColor3f(1, 0, 1); glVertex3f(0, -1.8f, 0);
	glEnd();
	glShadeModel(GL_FLAT);
	glBegin(GL_QUADS);
	glColor3f(1, 1, 1); glVertex3f(-2.6f, -2, -0.5f); glVertex3f(-1.6f, -2, -0.5f); glVertex3f(-1.6f, 2, -0.5f);
	glColor3f(0, 0.8f, 0.8f); glVertex3f(-2.6f, 2, -0.5f);
	glEnd();
	glShadeModel(GL_SMOOTH);
	glBegin(GL_TRIANGLE_FAN);
	{
		int k;
		glColor3f(1, 1, 1); glVertex3f(2.0f, 0, 0.5f);
		for (k = 0; k <= 12; k++) {
			float a = k * 2 * PI / 12;
			glColor3f(0.5f + 0.5f * cosn(a), 0.5f + 0.5f * sen(a), 0.3f);
			glVertex3f(2.0f + 0.7f * cosn(a), 0.7f * sen(a), 0.5f);
		}
	}
	glEnd();
	glDisable(GL_DEPTH_TEST);
}

/* 2: un toro con luz: dos luces (una direccional y otra de punto con brillo) */
static void toro(float R, float r, int nu, int nv)
{
	int i, j;
	for (i = 0; i < nu; i++) {
		glBegin(GL_QUAD_STRIP);
		for (j = 0; j <= nv; j++) {
			int k;
			for (k = 1; k >= 0; k--) {
				float u = (i + k) * 2 * PI / nu, v = j * 2 * PI / nv;
				float cx = cosn(u), cy = sen(u), cv = cosn(v), sv = sen(v);
				glNormal3f(cx * cv, cy * cv, sv);
				glTexCoord2f((i + k) / (float)nu * 4, j / (float)nv);
				glVertex3f((R + r * cv) * cx, (R + r * cv) * cy, r * sv);
			}
		}
		glEnd();
	}
}
static void luces(int w, int h)
{
	float l0[4] = { -1, 1, 1, 0 }, d0[4] = { 0.8f, 0.7f, 0.6f, 1 };
	float l1[4] = { 2, -1, 3, 1 }, d1[4] = { 0.2f, 0.4f, 1, 1 }, e1[4] = { 1, 1, 1, 1 };
	float ma[4] = { 0.2f, 0.1f, 0.1f, 1 }, md[4] = { 0.9f, 0.5f, 0.3f, 1 }, me[4] = { 0.8f, 0.8f, 0.8f, 1 };
	float amb[4] = { 0.1f, 0.1f, 0.1f, 1 };
	base(w, h, 0, 0, 0);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_LIGHT1);
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, amb);
	glLightfv(GL_LIGHT0, GL_POSITION, l0);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, d0);
	glLightfv(GL_LIGHT1, GL_POSITION, l1);
	glLightfv(GL_LIGHT1, GL_DIFFUSE, d1);
	glLightfv(GL_LIGHT1, GL_SPECULAR, e1);
	glLightf(GL_LIGHT1, GL_LINEAR_ATTENUATION, 0.1f);
	glMaterialfv(GL_FRONT, GL_AMBIENT, ma);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, md);
	glMaterialfv(GL_FRONT, GL_SPECULAR, me);
	glMaterialf(GL_FRONT, GL_SHININESS, 40);
	glEnable(GL_CULL_FACE);
	glTranslatef(-0.8f, 0, -5);
	glRotatef(55, 1, 0.3f, 0);
	toro(1.1f, 0.45f, 32, 16);
	/* otro con el color del material */
	glLoadIdentity();
	glTranslatef(1.8f, 0.8f, -6);
	glRotatef(-30, 1, 1, 0);
	glEnable(GL_COLOR_MATERIAL);
	glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
	glColor3f(0.3f, 0.9f, 0.3f);
	toro(0.7f, 0.3f, 24, 12);
	glDisable(GL_COLOR_MATERIAL);
	glDisable(GL_CULL_FACE);
	glDisable(GL_LIGHTING);
	glDisable(GL_LIGHT0);
	glDisable(GL_LIGHT1);
	glDisable(GL_DEPTH_TEST);
}

/* 3: texturas: un suelo en perspectiva con mipmaps, y cuadros con y sin repetición */
static unsigned tx[8];
static void hace_texturas(void)
{
	static const unsigned char gris[3] = { 200, 200, 200 }, azul[3] = { 40, 60, 160 }, rojo[3] = { 220, 40, 30 }, blanco[3] = { 250, 250, 240 };
	glGenTextures(8, tx);
	glBindTexture(GL_TEXTURE_2D, tx[0]);
	ajedrez(64, 64, 8, gris, azul, 0);
	sube_con_mipmaps(64, 64, GL_RGB, GL_RGBA);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, tx[1]);
	ajedrez(32, 32, 4, rojo, blanco, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	/* la reja: con agujeros (alfa 0 o 1) */
	glBindTexture(GL_TEXTURE_2D, tx[2]);
	ajedrez(64, 64, 16, blanco, rojo, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	/* el mapa de luz: de luminancia, 16×16, bilineal */
	glBindTexture(GL_TEXTURE_2D, tx[3]);
	{
		int x, y;
		for (y = 0; y < 16; y++)
			for (x = 0; x < 16; x++) {
				int dx = x - 5, dy = y - 9, v = 255 - (dx * dx + dy * dy) * 4;
				img[y * 16 + x] = (unsigned char)(v < 40 ? 40 : v);
			}
	}
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 16, 16, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, img);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	/* la del suelo, bilineal con mipmaps (LINEAR_MIPMAP_NEAREST, la de GLQuake) */
	glBindTexture(GL_TEXTURE_2D, tx[4]);
	ajedrez(128, 128, 16, gris, azul, 0);
	sube_con_mipmaps(128, 128, GL_RGB, GL_RGBA);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	/* un humo: alfa que va de 0 a 1 (4-4-4-4) */
	glBindTexture(GL_TEXTURE_2D, tx[5]);
	{
		int x, y;
		for (y = 0; y < 32; y++)
			for (x = 0; x < 32; x++) {
				int dx = x - 16, dy = y - 16, v = 255 - (dx * dx + dy * dy) * 255 / 256;
				unsigned char *o = img + (y * 32 + x) * 4;
				o[0] = 255; o[1] = 220; o[2] = 160; o[3] = (unsigned char)(v < 0 ? 0 : v);
			}
	}
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}
static void suelo(float tam, float rep)
{
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0); glVertex3f(-tam, 0, tam);
	glTexCoord2f(rep, 0); glVertex3f(tam, 0, tam);
	glTexCoord2f(rep, rep); glVertex3f(tam, 0, -tam);
	glTexCoord2f(0, rep); glVertex3f(-tam, 0, -tam);
	glEnd();
}
static void texturas(int w, int h)
{
	base(w, h, 0.3f, 0.5f, 0.7f);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glColor3f(1, 1, 1);
	/* el suelo (punto más cercano con mipmaps) */
	glBindTexture(GL_TEXTURE_2D, tx[0]);
	glPushMatrix();
	glTranslatef(0, -1.5f, -10);
	suelo(12, 12);
	glPopMatrix();
	/* un cuadro bilineal sin repetición, de colores (MODULATE con Gouraud) */
	glBindTexture(GL_TEXTURE_2D, tx[1]);
	glPushMatrix();
	glTranslatef(-1.6f, 0.4f, -4);
	glRotatef(25, 0, 1, 0);
	glBegin(GL_QUADS);
	glColor3f(1, 1, 1); glTexCoord2f(-0.25f, -0.25f); glVertex3f(-1, -1, 0);
	glColor3f(1, 1, 0); glTexCoord2f(1.25f, -0.25f); glVertex3f(1, -1, 0);
	glColor3f(0, 1, 1); glTexCoord2f(1.25f, 1.25f); glVertex3f(1, 1, 0);
	glColor3f(1, 1, 1); glTexCoord2f(-0.25f, 1.25f); glVertex3f(-1, 1, 0);
	glEnd();
	glPopMatrix();
	/* uno con GL_REPLACE */
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glBindTexture(GL_TEXTURE_2D, tx[0]);
	glColor3f(0, 0, 0);
	glPushMatrix();
	glTranslatef(1.7f, 0.5f, -4.5f);
	glRotatef(-35, 0, 1, 0);
	glRotatef(20, 1, 0, 0);
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0); glVertex3f(-1, -1, 0);
	glTexCoord2f(2, 0); glVertex3f(1, -1, 0);
	glTexCoord2f(2, 2); glVertex3f(1, 1, 0);
	glTexCoord2f(0, 2); glVertex3f(-1, 1, 0);
	glEnd();
	glPopMatrix();
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glColor3f(1, 1, 1);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_DEPTH_TEST);
}

/* 4: la mezcla y la prueba de alfa: reja, cristales y humo sumado */
static void mezcla(int w, int h)
{
	int k;
	base(w, h, 0.15f, 0.15f, 0.15f);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_TEXTURE_2D);
	/* el suelo bilineal con mipmaps */
	glBindTexture(GL_TEXTURE_2D, tx[4]);
	glPushMatrix();
	glTranslatef(0, -1.5f, -10);
	suelo(12, 6);
	glPopMatrix();
	/* la reja: prueba de alfa */
	glBindTexture(GL_TEXTURE_2D, tx[2]);
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.5f);
	glPushMatrix();
	glTranslatef(-1.2f, 0, -5);
	glRotatef(30, 0, 1, 0);
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0); glVertex3f(-1.2f, -1.2f, 0);
	glTexCoord2f(2, 0); glVertex3f(1.2f, -1.2f, 0);
	glTexCoord2f(2, 2); glVertex3f(1.2f, 1.2f, 0);
	glTexCoord2f(0, 2); glVertex3f(-1.2f, 1.2f, 0);
	glEnd();
	glPopMatrix();
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_TEXTURE_2D);
	/* cristales de colores (SRC_ALPHA, ONE_MINUS_SRC_ALPHA), de atrás adelante, sin escribir z */
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_FALSE);
	for (k = 0; k < 3; k++) {
		glColor4f(k == 0, k == 1, k == 2, 0.45f);
		glPushMatrix();
		glTranslatef(0.6f + k * 0.5f, -0.3f + k * 0.35f, -6 + k * 0.6f);
		glBegin(GL_QUADS);
		glVertex3f(-0.8f, -0.8f, 0); glVertex3f(0.8f, -0.8f, 0); glVertex3f(0.8f, 0.8f, 0); glVertex3f(-0.8f, 0.8f, 0);
		glEnd();
		glPopMatrix();
	}
	/* el humo, sumado (SRC_ALPHA, ONE) */
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, tx[5]);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	glColor4f(1, 1, 1, 1);
	for (k = 0; k < 4; k++) {
		glPushMatrix();
		glTranslatef(-2.2f + k * 0.4f, 0.6f + k * 0.25f, -4.5f);
		glBegin(GL_QUADS);
		glTexCoord2f(0, 0); glVertex3f(-0.5f, -0.5f, 0);
		glTexCoord2f(1, 0); glVertex3f(0.5f, -0.5f, 0);
		glTexCoord2f(1, 1); glVertex3f(0.5f, 0.5f, 0);
		glTexCoord2f(0, 1); glVertex3f(-0.5f, 0.5f, 0);
		glEnd();
		glPopMatrix();
	}
	glDisable(GL_TEXTURE_2D);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
}

/* 5: la niebla: un pasillo con niebla lineal, y unos cubos con niebla exponencial */
static void cubo(float s)
{
	static const float v[8][3] = { { -1, -1, -1 }, { 1, -1, -1 }, { 1, 1, -1 }, { -1, 1, -1 }, { -1, -1, 1 }, { 1, -1, 1 }, { 1, 1, 1 }, { -1, 1, 1 } };
	static const int c[6][4] = { { 0, 3, 2, 1 }, { 4, 5, 6, 7 }, { 0, 1, 5, 4 }, { 2, 3, 7, 6 }, { 1, 2, 6, 5 }, { 0, 4, 7, 3 } };
	static const float n[6][3] = { { 0, 0, -1 }, { 0, 0, 1 }, { 0, -1, 0 }, { 0, 1, 0 }, { 1, 0, 0 }, { -1, 0, 0 } };
	int f, k;
	glBegin(GL_QUADS);
	for (f = 0; f < 6; f++) {
		glNormal3fv(n[f]);
		for (k = 0; k < 4; k++) {
			glTexCoord2f(k == 1 || k == 2, k >= 2);
			glVertex3f(v[c[f][k]][0] * s, v[c[f][k]][1] * s, v[c[f][k]][2] * s);
		}
	}
	glEnd();
}
static void niebla(int w, int h)
{
	float gris[4] = { 0.5f, 0.55f, 0.6f, 1 };
	int k;
	base(w, h, 0.5f, 0.55f, 0.6f);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_FOG);
	glFogi(GL_FOG_MODE, GL_LINEAR);
	glFogf(GL_FOG_START, 3);
	glFogf(GL_FOG_END, 30);
	glFogfv(GL_FOG_COLOR, gris);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, tx[0]);
	glColor3f(1, 1, 1);
	/* el pasillo: suelo, techo y paredes */
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0); glVertex3f(-2, -1.5f, 0); glTexCoord2f(2, 0); glVertex3f(2, -1.5f, 0);
	glTexCoord2f(2, 20); glVertex3f(2, -1.5f, -40); glTexCoord2f(0, 20); glVertex3f(-2, -1.5f, -40);
	glTexCoord2f(0, 0); glVertex3f(-2, 1.5f, 0); glTexCoord2f(0, 20); glVertex3f(-2, 1.5f, -40);
	glTexCoord2f(2, 20); glVertex3f(2, 1.5f, -40); glTexCoord2f(2, 0); glVertex3f(2, 1.5f, 0);
	glTexCoord2f(0, 0); glVertex3f(-2, -1.5f, 0); glTexCoord2f(20, 0); glVertex3f(-2, -1.5f, -40);
	glTexCoord2f(20, 2); glVertex3f(-2, 1.5f, -40); glTexCoord2f(0, 2); glVertex3f(-2, 1.5f, 0);
	glEnd();
	glDisable(GL_TEXTURE_2D);
	glFogi(GL_FOG_MODE, GL_EXP);
	glFogf(GL_FOG_DENSITY, 0.12f);
	for (k = 0; k < 5; k++) {
		glPushMatrix();
		glTranslatef(1.0f, -0.6f, -4.0f - k * 5.0f);
		glRotatef(20.0f + k * 15, 0, 1, 0);
		glColor3f(0.9f, 0.3f + k * 0.1f, 0.2f);
		cubo(0.5f);
		glPopMatrix();
	}
	glDisable(GL_FOG);
	glDisable(GL_DEPTH_TEST);
}

/* 6: listas y matrices de vértices: unos engranajes (glNewList) y una rejilla con glDrawElements */
static void engranaje(float r0, float r1, float ancho, int dientes, float alto)
{
	int i;
	float r2 = r1 + alto, da = 2 * PI / dientes / 4;
	glShadeModel(GL_FLAT);
	glNormal3f(0, 0, 1);
	glBegin(GL_QUAD_STRIP);
	for (i = 0; i <= dientes; i++) {
		float a = i * 2 * PI / dientes;
		glVertex3f(r0 * cosn(a), r0 * sen(a), ancho * 0.5f);
		glVertex3f(r1 * cosn(a), r1 * sen(a), ancho * 0.5f);
		glVertex3f(r0 * cosn(a), r0 * sen(a), ancho * 0.5f);
		glVertex3f(r1 * cosn(a + 3 * da), r1 * sen(a + 3 * da), ancho * 0.5f);
	}
	glEnd();
	glBegin(GL_QUADS);
	for (i = 0; i < dientes; i++) {
		float a = i * 2 * PI / dientes;
		glVertex3f(r1 * cosn(a), r1 * sen(a), ancho * 0.5f);
		glVertex3f(r2 * cosn(a + da), r2 * sen(a + da), ancho * 0.5f);
		glVertex3f(r2 * cosn(a + 2 * da), r2 * sen(a + 2 * da), ancho * 0.5f);
		glVertex3f(r1 * cosn(a + 3 * da), r1 * sen(a + 3 * da), ancho * 0.5f);
	}
	glEnd();
	glNormal3f(0, 0, -1);
	glBegin(GL_QUAD_STRIP);
	for (i = 0; i <= dientes; i++) {
		float a = i * 2 * PI / dientes;
		glVertex3f(r1 * cosn(a), r1 * sen(a), -ancho * 0.5f);
		glVertex3f(r0 * cosn(a), r0 * sen(a), -ancho * 0.5f);
		glVertex3f(r1 * cosn(a + 3 * da), r1 * sen(a + 3 * da), -ancho * 0.5f);
		glVertex3f(r0 * cosn(a), r0 * sen(a), -ancho * 0.5f);
	}
	glEnd();
	glBegin(GL_QUAD_STRIP);
	for (i = 0; i < dientes; i++) {
		float a = i * 2 * PI / dientes, u, v, len;
		glVertex3f(r1 * cosn(a), r1 * sen(a), ancho * 0.5f);
		glVertex3f(r1 * cosn(a), r1 * sen(a), -ancho * 0.5f);
		u = r2 * cosn(a + da) - r1 * cosn(a);
		v = r2 * sen(a + da) - r1 * sen(a);
		len = raiz(u * u + v * v);
		u /= len; v /= len;
		glNormal3f(v, -u, 0);
		glVertex3f(r2 * cosn(a + da), r2 * sen(a + da), ancho * 0.5f);
		glVertex3f(r2 * cosn(a + da), r2 * sen(a + da), -ancho * 0.5f);
		glNormal3f(cosn(a), sen(a), 0);
		glVertex3f(r2 * cosn(a + 2 * da), r2 * sen(a + 2 * da), ancho * 0.5f);
		glVertex3f(r2 * cosn(a + 2 * da), r2 * sen(a + 2 * da), -ancho * 0.5f);
		u = r1 * cosn(a + 3 * da) - r2 * cosn(a + 2 * da);
		v = r1 * sen(a + 3 * da) - r2 * sen(a + 2 * da);
		glNormal3f(v, -u, 0);
		glVertex3f(r1 * cosn(a + 3 * da), r1 * sen(a + 3 * da), ancho * 0.5f);
		glVertex3f(r1 * cosn(a + 3 * da), r1 * sen(a + 3 * da), -ancho * 0.5f);
		glNormal3f(cosn(a), sen(a), 0);
	}
	glVertex3f(r1, 0, ancho * 0.5f);
	glVertex3f(r1, 0, -ancho * 0.5f);
	glEnd();
	glShadeModel(GL_SMOOTH);
	glBegin(GL_QUAD_STRIP);
	for (i = 0; i <= dientes; i++) {
		float a = i * 2 * PI / dientes;
		glNormal3f(-cosn(a), -sen(a), 0);
		glVertex3f(r0 * cosn(a), r0 * sen(a), -ancho * 0.5f);
		glVertex3f(r0 * cosn(a), r0 * sen(a), ancho * 0.5f);
	}
	glEnd();
}
static unsigned listas_eng;
void escenas_engranajes(float ang)
{
	static const float pos[4] = { 5, 5, 10, 0 }, rojo[4] = { 0.8f, 0.1f, 0, 1 }, verde[4] = { 0, 0.8f, 0.2f, 1 }, azul[4] = { 0.2f, 0.2f, 1, 1 };
	if (!listas_eng) {
		listas_eng = glGenLists(3);
		glNewList(listas_eng, GL_COMPILE);
		glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, rojo);
		engranaje(1, 4, 1, 20, 0.7f);
		glEndList();
		glNewList(listas_eng + 1, GL_COMPILE);
		glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, verde);
		engranaje(0.5f, 2, 2, 10, 0.7f);
		glEndList();
		glNewList(listas_eng + 2, GL_COMPILE);
		glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, azul);
		engranaje(1.3f, 2, 0.5f, 10, 0.7f);
		glEndList();
	}
	glLightfv(GL_LIGHT0, GL_POSITION, pos);
	glEnable(GL_CULL_FACE);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_NORMALIZE);
	glPushMatrix();
	glRotatef(20, 1, 0, 0);
	glRotatef(30, 0, 1, 0);
	glPushMatrix();
	glTranslatef(-3, -2, 0);
	glRotatef(ang, 0, 0, 1);
	glCallList(listas_eng);
	glPopMatrix();
	glPushMatrix();
	glTranslatef(3.1f, -2, 0);
	glRotatef(-2 * ang - 9, 0, 0, 1);
	glCallList(listas_eng + 1);
	glPopMatrix();
	glPushMatrix();
	glTranslatef(-3.1f, 4.2f, 0);
	glRotatef(-2 * ang - 25, 0, 0, 1);
	glCallList(listas_eng + 2);
	glPopMatrix();
	glPopMatrix();
	glDisable(GL_NORMALIZE);
	glDisable(GL_CULL_FACE);
	glDisable(GL_LIGHTING);
	glDisable(GL_LIGHT0);
	glDisable(GL_DEPTH_TEST);
}
static void listas(int w, int h)
{
	static float malla[9 * 9][6];      /* t2f c3f... no: v3f + c3f */
	static unsigned short ind[8 * 8 * 6];
	int i, j, n = 0;
	glViewport(0, 0, w, h);
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glFrustum(-1, 1, -(float)h / w, (float)h / w, 5, 60);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(0, 0, -40);
	escenas_engranajes(15);
	/* la rejilla de colores con glInterleavedArrays (C3F_V3F) y glDrawElements */
	for (j = 0; j < 9; j++)
		for (i = 0; i < 9; i++) {
			float *m = malla[j * 9 + i];
			m[0] = i / 8.0f; m[1] = j / 8.0f; m[2] = 1 - i / 8.0f;
			m[3] = 4 + i * 0.6f; m[4] = 2 + j * 0.6f + 0.3f * sen(i * 1.3f); m[5] = 0;
		}
	for (j = 0; j < 8; j++)
		for (i = 0; i < 8; i++) {
			unsigned short a = (unsigned short)(j * 9 + i);
			ind[n++] = a; ind[n++] = a + 1; ind[n++] = a + 10;
			ind[n++] = a; ind[n++] = a + 10; ind[n++] = a + 9;
		}
	glLoadIdentity();
	glTranslatef(0, 0, -40);
	glInterleavedArrays(GL_C3F_V3F, 0, malla);
	glDrawElements(GL_TRIANGLES, n, GL_UNSIGNED_SHORT, ind);
	/* y glDrawArrays con una tira de líneas */
	glTranslatef(-11, -9, 0);
	glDrawArrays(GL_LINE_STRIP, 0, 9);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
}

/* 7: varias texturas (GL_ARB_multitexture): la pared por el mapa de luz,
 * a la vez (dos unidades) y en dos pasadas (GL_ZERO, GL_SRC_COLOR), como GLQuake */
static void pared(float x, float sep)
{
	(void)sep;
	glBegin(GL_QUADS);
	glMultiTexCoord2fARB(GL_TEXTURE0_ARB, 0, 0); glMultiTexCoord2fARB(GL_TEXTURE1_ARB, 0, 0); glVertex3f(x - 1.3f, -1.5f, 0);
	glMultiTexCoord2fARB(GL_TEXTURE0_ARB, 3, 0); glMultiTexCoord2fARB(GL_TEXTURE1_ARB, 1, 0); glVertex3f(x + 1.3f, -1.5f, 0);
	glMultiTexCoord2fARB(GL_TEXTURE0_ARB, 3, 3); glMultiTexCoord2fARB(GL_TEXTURE1_ARB, 1, 1); glVertex3f(x + 1.3f, 1.5f, -2.5f);
	glMultiTexCoord2fARB(GL_TEXTURE0_ARB, 0, 3); glMultiTexCoord2fARB(GL_TEXTURE1_ARB, 0, 1); glVertex3f(x - 1.3f, 1.5f, -2.5f);
	glEnd();
}
static void multitextura(int w, int h)
{
	base(w, h, 0, 0, 0);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glTranslatef(0, 0, -5);
	/* a la vez */
	glActiveTextureARB(GL_TEXTURE0_ARB);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, tx[4]);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glActiveTextureARB(GL_TEXTURE1_ARB);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, tx[3]);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	pared(-1.5f, 0);
	glDisable(GL_TEXTURE_2D);
	glActiveTextureARB(GL_TEXTURE0_ARB);
	/* en dos pasadas */
	glBindTexture(GL_TEXTURE_2D, tx[4]);
	pared(1.5f, 0);
	glBindTexture(GL_TEXTURE_2D, tx[3]);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ZERO, GL_SRC_COLOR);
	glDepthMask(GL_FALSE);
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0); glVertex3f(1.5f - 1.3f, -1.5f, 0);
	glTexCoord2f(1, 0); glVertex3f(1.5f + 1.3f, -1.5f, 0);
	glTexCoord2f(1, 1); glVertex3f(1.5f + 1.3f, 1.5f, -2.5f);
	glTexCoord2f(0, 1); glVertex3f(1.5f - 1.3f, 1.5f, -2.5f);
	glEnd();
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_DEPTH_TEST);
}

/* 8: el recorte: un triángulo que cruza el plano cercano, un plano del programa,
 * la tijera, una vista más pequeña, polígonos en líneas y caras ocultas */
static void recorte(int w, int h)
{
	double plano[4] = { 1, 1, 0, 0.5 };
	base(w, h, 0.2f, 0.2f, 0.2f);
	glEnable(GL_DEPTH_TEST);
	/* cruza el plano cercano (z = -1) */
	glBegin(GL_TRIANGLES);
	glColor3f(1, 0.5f, 0); glVertex3f(-3, -2, -6);
	glColor3f(0, 0.5f, 1); glVertex3f(3, -2, -6);
	glColor3f(1, 1, 1); glVertex3f(0, -0.6f, 2);
	glEnd();
	/* un plano del programa: se queda lo que está por encima de x + y = -0.5 */
	glPushMatrix();
	glTranslatef(-1.5f, 0.8f, -5);
	glClipPlane(GL_CLIP_PLANE0, plano);
	glEnable(GL_CLIP_PLANE0);
	glColor3f(0.2f, 0.9f, 0.3f);
	glBegin(GL_QUADS);
	glVertex3f(-1, -1, 0); glVertex3f(1, -1, 0); glVertex3f(1, 1, 0); glVertex3f(-1, 1, 0);
	glEnd();
	glDisable(GL_CLIP_PLANE0);
	glPopMatrix();
	/* en líneas, y la cara de atrás quitada */
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glColor3f(1, 1, 0);
	glPushMatrix();
	glTranslatef(1.5f, 0.8f, -5);
	glBegin(GL_TRIANGLES);
	glVertex3f(-1, -1, 0); glVertex3f(1, -1, 0); glVertex3f(0, 1, 0);
	glEnd();
	glPopMatrix();
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glEnable(GL_CULL_FACE);
	glColor3f(1, 0, 0);
	glBegin(GL_TRIANGLES);   /* de espaldas (en el sentido del reloj): no sale */
	glVertex3f(0, 0, -4); glVertex3f(0.5f, 1, -4); glVertex3f(1, 0, -4);
	glEnd();
	glDisable(GL_CULL_FACE);
	/* la tijera */
	glEnable(GL_SCISSOR_TEST);
	glScissor(w / 8, h / 8, w / 4, h / 5);
	glColor3f(0.6f, 0.2f, 0.9f);
	glBegin(GL_QUADS);
	glVertex3f(-5, -5, -3); glVertex3f(5, -5, -3); glVertex3f(5, 5, -3); glVertex3f(-5, 5, -3);
	glEnd();
	glDisable(GL_SCISSOR_TEST);
	/* una vista pequeña arriba a la derecha, con proyección ortogonal */
	glViewport(w * 3 / 4, h * 3 / 4, w / 4, h / 4);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-1, 1, -1, 1, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glBegin(GL_TRIANGLES);
	glColor3f(1, 1, 1); glVertex2f(-1.5f, -1.5f);
	glColor3f(0, 1, 0); glVertex2f(1.5f, -0.5f);
	glColor3f(1, 0, 1); glVertex2f(0, 1.5f);
	glEnd();
	glPointSize(3);
	glDisable(GL_DEPTH_TEST);
	glBegin(GL_POINTS);
	glColor3f(0, 0, 0); glVertex2f(0, 0);
	glEnd();
	glPointSize(1);
	glViewport(0, 0, w, h);
	glDisable(GL_DEPTH_TEST);
}

/* 9: los píxeles: glDrawPixels, glBitmap, glReadPixels (de vuelta) y glCopyTexSubImage2D */
static void pixeles(int w, int h)
{
	static unsigned char cuadro[32 * 32 * 3], leido[64 * 64 * 4];
	static const unsigned char letra[16] = { 0x00, 0x00, 0x7E, 0x42, 0x42, 0x42, 0x42, 0x7E, 0x42, 0x42, 0x42, 0x24, 0x18, 0x00, 0x00, 0x00 };
	int x, y;
	glViewport(0, 0, w, h);
	glClearColor(0.1f, 0.2f, 0.1f, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, w, 0, h, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	for (y = 0; y < 32; y++)
		for (x = 0; x < 32; x++) {
			unsigned char *o = cuadro + (y * 32 + x) * 3;
			o[0] = (unsigned char)(x * 8); o[1] = (unsigned char)(y * 8); o[2] = (unsigned char)(((x ^ y) & 8) ? 255 : 0);
		}
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glRasterPos2i(10, 10);
	glDrawPixels(32, 32, GL_RGB, GL_UNSIGNED_BYTE, cuadro);
	glPixelZoom(2, 2);
	glRasterPos2i(60, 10);
	glDrawPixels(32, 32, GL_RGB, GL_UNSIGNED_BYTE, cuadro);
	glPixelZoom(1, 1);
	/* unas letras con glBitmap (A), en amarillo */
	glColor3f(1, 1, 0);
	glRasterPos2i(10, 120);
	for (x = 0; x < 6; x++) glBitmap(8, 16, 0, 0, 10, 0, letra);
	/* un degradado dibujado con triángulos, leído con glReadPixels y vuelto a poner más allá */
	glBegin(GL_QUADS);
	glColor3f(1, 0, 0); glVertex2i(140, 10);
	glColor3f(0, 1, 0); glVertex2i(204, 10);
	glColor3f(0, 0, 1); glVertex2i(204, 74);
	glColor3f(1, 1, 1); glVertex2i(140, 74);
	glEnd();
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(140, 10, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, leido);
	glRasterPos2i(220, 10);
	glDrawPixels(64, 64, GL_RGBA, GL_UNSIGNED_BYTE, leido);
	/* el mismo trozo a una textura (glCopyTexSubImage2D) y dibujada girada */
	glBindTexture(GL_TEXTURE_2D, tx[6]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 64, 64, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 140, 10, 64, 64);
	glEnable(GL_TEXTURE_2D);
	glColor3f(1, 1, 1);
	glTranslatef(240, 160, 0);
	glRotatef(30, 0, 0, 1);
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0); glVertex2i(-40, -40);
	glTexCoord2f(1, 0); glVertex2i(40, -40);
	glTexCoord2f(1, 1); glVertex2i(40, 40);
	glTexCoord2f(0, 1); glVertex2i(-40, 40);
	glEnd();
	glDisable(GL_TEXTURE_2D);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glPixelStorei(GL_PACK_ALIGNMENT, 4);
}

/* 10: el trilineal (GL_LINEAR_MIPMAP_LINEAR) y la generación de coordenadas (esfera) */
static void trilineal(int w, int h)
{
	base(w, h, 0.2f, 0.2f, 0.25f);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, tx[7]);
	{
		static const unsigned char gris[3] = { 230, 230, 230 }, verde[3] = { 20, 120, 40 };
		ajedrez(64, 64, 8, gris, verde, 0);
		sube_con_mipmaps(64, 64, GL_RGB, GL_RGBA);
	}
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glColor3f(1, 1, 1);
	glPushMatrix();
	glTranslatef(0, -1.5f, -10);
	suelo(14, 14);
	glPopMatrix();
	/* un toro con el mapa de esfera (los reflejos de la época) */
	glBindTexture(GL_TEXTURE_2D, tx[1]);
	glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
	glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
	glEnable(GL_TEXTURE_GEN_S);
	glEnable(GL_TEXTURE_GEN_T);
	glPushMatrix();
	glTranslatef(0, 0.8f, -5);
	glRotatef(60, 1, 0, 0);
	toro(1, 0.4f, 24, 12);
	glPopMatrix();
	glDisable(GL_TEXTURE_GEN_S);
	glDisable(GL_TEXTURE_GEN_T);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_DEPTH_TEST);
}

const Escena escenas[] = {
	{ "triangulos", triangulos },
	{ "luces", luces },
	{ "texturas", texturas },
	{ "mezcla", mezcla },
	{ "niebla", niebla },
	{ "listas", listas },
	{ "multitextura", multitextura },
	{ "recorte", recorte },
	{ "pixeles", pixeles },
	{ "trilineal", trilineal },
	{ 0, 0 }
};

void escenas_ini(void) { hace_texturas(); }
