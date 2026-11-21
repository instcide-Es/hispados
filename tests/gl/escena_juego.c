/* tests/gl/escena_juego.c - la escena de medir de HispaGL: una sala como las
 * de los juegos de 1997-1999 (al estilo del renderizador de GLQuake, pero
 * propia): paredes con textura y mipmaps, el mapa de luz con la segunda
 * unidad (GL_ARB_multitexture, MODULATE), unos modelos con luz por vértice
 * (Gouraud, GL_MODULATE sobre su piel), dos llamas con mezcla, el arma en
 * primer plano (glDepthRange) y la barra de abajo en 2D con prueba de alfa.
 * La cámara da vueltas: «cuadro» es el número de fotograma.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "GL/gl.h"
#include "escenas.h"

#define PI 3.14159265358979f
static float sen(float x)
{
	float x2;
	while (x > PI) x -= 2 * PI;
	while (x < -PI) x += 2 * PI;
	x2 = x * x;
	return x * (1 - x2 / 6 * (1 - x2 / 20 * (1 - x2 / 42 * (1 - x2 / 72 * (1 - x2 / 110)))));
}
static float cosn(float x) { return sen(x + PI / 2); }

static unsigned tex[6];
static unsigned char im[128 * 128 * 4];
static unsigned semilla = 12345;
static int azar(int n) { semilla = semilla * 1103515245u + 12345u; return (int)((semilla >> 16) % (unsigned)n); }

static void mipmaps(int w, int h, int fi)
{
	int nivel = 0;
	glTexImage2D(GL_TEXTURE_2D, 0, fi, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, im);
	while (w > 1 || h > 1) {
		int nw = w > 1 ? w / 2 : 1, nh = h > 1 ? h / 2 : 1, x, y, k;
		for (y = 0; y < nh; y++)
			for (x = 0; x < nw; x++)
				for (k = 0; k < 4; k++) {
					int a = im[((y * 2) * w + x * 2) * 4 + k], b = im[((y * 2) * w + (w > 1 ? x * 2 + 1 : x * 2)) * 4 + k];
					int c = im[(((h > 1 ? y * 2 + 1 : y * 2)) * w + x * 2) * 4 + k], d = im[(((h > 1 ? y * 2 + 1 : y * 2)) * w + (w > 1 ? x * 2 + 1 : x * 2)) * 4 + k];
					im[(y * nw + x) * 4 + k] = (unsigned char)((a + b + c + d + 2) / 4);
				}
		w = nw; h = nh;
		glTexImage2D(GL_TEXTURE_2D, ++nivel, fi, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, im);
	}
}

/* ladrillos, losas, piel del modelo, mapa de luz, llama, barra */
static void texturas_juego(void)
{
	int x, y;
	glGenTextures(6, tex);
	for (y = 0; y < 64; y++)
		for (x = 0; x < 64; x++) {
			unsigned char *o = im + (y * 64 + x) * 4;
			int fila = y / 16, junta = (y % 16 == 0) || ((x + (fila & 1) * 16) % 32 == 0), n = azar(40);
			o[0] = (unsigned char)(junta ? 70 : 150 + n); o[1] = (unsigned char)(junta ? 65 : 70 + n / 2); o[2] = (unsigned char)(junta ? 60 : 50 + n / 3); o[3] = 255;
		}
	glBindTexture(GL_TEXTURE_2D, tex[0]);
	mipmaps(64, 64, GL_RGB);
	for (y = 0; y < 64; y++)
		for (x = 0; x < 64; x++) {
			unsigned char *o = im + (y * 64 + x) * 4;
			int junta = (x % 32 == 0) || (y % 32 == 0), n = azar(30);
			o[0] = o[1] = (unsigned char)(junta ? 50 : 110 + n); o[2] = (unsigned char)(junta ? 55 : 120 + n); o[3] = 255;
		}
	glBindTexture(GL_TEXTURE_2D, tex[1]);
	mipmaps(64, 64, GL_RGB);
	for (y = 0; y < 32; y++)
		for (x = 0; x < 32; x++) {
			unsigned char *o = im + (y * 32 + x) * 4;
			int n = azar(50);
			o[0] = (unsigned char)(90 + n); o[1] = (unsigned char)(120 + (y * 3)); o[2] = (unsigned char)(60 + n / 2); o[3] = 255;
		}
	glBindTexture(GL_TEXTURE_2D, tex[2]);
	mipmaps(32, 32, GL_RGB);
	/* el mapa de luz: 32×32 de luminancia, dos focos */
	for (y = 0; y < 32; y++)
		for (x = 0; x < 32; x++) {
			int d1 = (x - 8) * (x - 8) + (y - 10) * (y - 10), d2 = (x - 24) * (x - 24) + (y - 20) * (y - 20);
			int v = 50 + 9000 / (d1 + 60) + 7000 / (d2 + 60);
			im[y * 32 + x] = (unsigned char)(v > 255 ? 255 : v);
		}
	glBindTexture(GL_TEXTURE_2D, tex[3]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 32, 32, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, im);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	for (y = 0; y < 32; y++)
		for (x = 0; x < 32; x++) {
			unsigned char *o = im + (y * 32 + x) * 4;
			int dx = x - 16, dy = (y - 12) * 2, d = dx * dx + dy * dy, a = 255 - d / 2;
			o[0] = 255; o[1] = (unsigned char)(140 + y * 3); o[2] = 40; o[3] = (unsigned char)(a < 0 ? 0 : a > 255 ? 255 : a);
		}
	glBindTexture(GL_TEXTURE_2D, tex[4]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, im);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	for (y = 0; y < 16; y++)
		for (x = 0; x < 64; x++) {
			unsigned char *o = im + (y * 64 + x) * 4;
			int borde = y < 2 || y > 13 || (x % 16) < 2;
			o[0] = (unsigned char)(borde ? 200 : 60); o[1] = (unsigned char)(borde ? 180 : 50); o[2] = (unsigned char)(borde ? 60 : 40);
			o[3] = (unsigned char)((x % 16) == 15 ? 0 : 255);
		}
	glBindTexture(GL_TEXTURE_2D, tex[5]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 64, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, im);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

static int filtro_juego = GL_LINEAR_MIPMAP_NEAREST, mag_juego = GL_LINEAR;
void juego_filtro(int min, int mag) { filtro_juego = min; mag_juego = mag; }

void juego_ini(void) { texturas_juego(); }

/* una pared con su mapa de luz (unidad 1) */
static void cara(float x0, float y0, float z0, float ux, float uy, float uz, float vx, float vy, float vz, float rep_u, float rep_v)
{
	glBegin(GL_QUADS);
	glMultiTexCoord2fARB(GL_TEXTURE0_ARB, 0, 0); glMultiTexCoord2fARB(GL_TEXTURE1_ARB, 0.05f, 0.05f);
	glVertex3f(x0, y0, z0);
	glMultiTexCoord2fARB(GL_TEXTURE0_ARB, rep_u, 0); glMultiTexCoord2fARB(GL_TEXTURE1_ARB, 0.95f, 0.05f);
	glVertex3f(x0 + ux, y0 + uy, z0 + uz);
	glMultiTexCoord2fARB(GL_TEXTURE0_ARB, rep_u, rep_v); glMultiTexCoord2fARB(GL_TEXTURE1_ARB, 0.95f, 0.95f);
	glVertex3f(x0 + ux + vx, y0 + uy + vy, z0 + uz + vz);
	glMultiTexCoord2fARB(GL_TEXTURE0_ARB, 0, rep_v); glMultiTexCoord2fARB(GL_TEXTURE1_ARB, 0.05f, 0.95f);
	glVertex3f(x0 + vx, y0 + vy, z0 + vz);
	glEnd();
}
/* un «monstruo»: un cuerpo de esferas aplastadas con luz por vértice (unos 400 triángulos) */
static void bola(float rx, float ry, float rz, int nu, int nv, float luz_x, float luz_z)
{
	int i, j;
	for (j = 0; j < nv; j++) {
		glBegin(GL_TRIANGLE_STRIP);
		for (i = 0; i <= nu; i++) {
			int k;
			for (k = 0; k < 2; k++) {
				float v = (j + k) * PI / nv - PI / 2, u = i * 2 * PI / nu;
				float nx = cosn(v) * cosn(u), ny = sen(v), nz = cosn(v) * sen(u);
				float l = 0.35f + 0.65f * (nx * luz_x + ny * 0.5f + nz * luz_z);
				if (l < 0.2f) l = 0.2f;
				if (l > 1) l = 1;
				glColor3f(l, l, l);
				glTexCoord2f(u / (2 * PI) * 2, (v + PI / 2) / PI);
				glVertex3f(nx * rx, ny * ry, nz * rz);
			}
		}
		glEnd();
	}
}

void juego_dibuja(int w, int h, int cuadro)
{
	float ang = cuadro * 0.03f, cx = 3.0f * sen(ang), cz = 3.0f * cosn(ang * 0.7f);
	int k;
	glViewport(0, 0, w, h);
	glClearColor(0, 0, 0, 1);
	glClear(GL_DEPTH_BUFFER_BIT);         /* (los juegos de la época no borran el color: lo tapa todo la sala) */
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	{
		float n = 4, t = n * 0.6f;
		glFrustum(-t * w / h, t * w / h, -t, t, n, 4096);
	}
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glRotatef(-90, 1, 0, 0);              /* z hacia arriba, como en los mapas */
	glRotatef(90, 0, 0, 1);
	glRotatef(ang * 57.3f, 0, 0, 1);
	glTranslatef(-cx * 32, -cz * 32, -48);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);                 /* (GLQuake: las caras van en el sentido del reloj) */
	glFrontFace(GL_CW);
	glCullFace(GL_BACK);
	glDisable(GL_CULL_FACE);
	/* la sala: 512×512×192, el suelo y el techo con losas, las paredes de ladrillo */
	glActiveTextureARB(GL_TEXTURE0_ARB);
	glEnable(GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glActiveTextureARB(GL_TEXTURE1_ARB);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, tex[3]);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glActiveTextureARB(GL_TEXTURE0_ARB);
	glBindTexture(GL_TEXTURE_2D, tex[1]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filtro_juego);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_juego);
	for (k = 0; k < 4; k++) {
		float x = -256 + (k & 1) * 256, y = -256 + (k >> 1) * 256;
		cara(x, y, 0, 256, 0, 0, 0, 256, 0, 4, 4);
		cara(x, y, 192, 0, 256, 0, 256, 0, 0, 4, 4);
	}
	glBindTexture(GL_TEXTURE_2D, tex[0]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filtro_juego);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_juego);
	for (k = 0; k < 2; k++) {
		cara(-256 + k * 256, -256, 0, 256, 0, 0, 0, 0, 192, 4, 3);
		cara(256 - k * 256, 256, 0, -256, 0, 0, 0, 0, 192, 4, 3);
		cara(-256, 256 - k * 256, 0, 0, -256, 0, 0, 0, 192, 4, 3);
		cara(256, -256 + k * 256, 0, 0, 256, 0, 0, 0, 192, 4, 3);
	}
	/* un pilar en medio */
	cara(-32, -32, 0, 64, 0, 0, 0, 0, 160, 1, 2.5f);
	cara(32, -32, 0, 0, 64, 0, 0, 0, 160, 1, 2.5f);
	cara(32, 32, 0, -64, 0, 0, 0, 0, 160, 1, 2.5f);
	cara(-32, 32, 0, 0, -64, 0, 0, 0, 160, 1, 2.5f);
	glActiveTextureARB(GL_TEXTURE1_ARB);
	glDisable(GL_TEXTURE_2D);
	glActiveTextureARB(GL_TEXTURE0_ARB);
	/* tres modelos con luz por vértice */
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glBindTexture(GL_TEXTURE_2D, tex[2]);
	glShadeModel(GL_SMOOTH);
	for (k = 0; k < 3; k++) {
		glPushMatrix();
		glTranslatef(-150.0f + k * 140, 120.0f - k * 60, 30);
		glRotatef(cuadro * 2.0f + k * 50, 0, 0, 1);
		bola(16, 12, 26, 16, 12, 0.6f, 0.5f);
		glTranslatef(0, 0, 34);
		bola(10, 10, 10, 10, 8, 0.6f, 0.5f);
		glPopMatrix();
	}
	/* dos llamas con mezcla, sin escribir la profundidad */
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_FALSE);
	glBindTexture(GL_TEXTURE_2D, tex[4]);
	glColor4f(1, 1, 1, 1);
	for (k = 0; k < 2; k++) {
		float px = k ? 200.0f : -200.0f, py = k ? -200.0f : 200.0f;
		glBegin(GL_QUADS);
		glTexCoord2f(0, 1); glVertex3f(px - 12, py, 60);
		glTexCoord2f(1, 1); glVertex3f(px + 12, py, 60);
		glTexCoord2f(1, 0); glVertex3f(px + 12, py, 96);
		glTexCoord2f(0, 0); glVertex3f(px - 12, py, 96);
		glEnd();
	}
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	/* el arma: delante de todo (glDepthRange, como GLQuake) */
	glDepthRange(0, 0.3);
	glLoadIdentity();
	glBindTexture(GL_TEXTURE_2D, tex[2]);
	glTranslatef(8, -10, -24);
	glRotatef(-80, 1, 0, 0);
	glRotatef(10, 0, 0, 1);
	bola(3, 3, 14, 10, 6, 0.3f, 0.8f);
	glDepthRange(0, 1);
	/* la barra de abajo: 2D, prueba de alfa */
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, 320, 240, 0, -99999, 99999);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.666f);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glBindTexture(GL_TEXTURE_2D, tex[5]);
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0); glVertex2f(0, 216);
	glTexCoord2f(5, 0); glVertex2f(320, 216);
	glTexCoord2f(5, 1); glVertex2f(320, 240);
	glTexCoord2f(0, 1); glVertex2f(0, 240);
	glEnd();
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}
