/* glprueba.c - «GLPRUEBA», el programa de ejemplo de HispaGL (OPENGL32.DLL
 * de Hispa-486 e Hispa-Pentium; docs/486/OPENGL.md): una escena con todo lo
 * de OpenGL 1.1 que usaban los juegos de la época:
 *  - un suelo de baldosas con mipmaps y niebla lineal;
 *  - una caja que gira, con textura y luz (GL_MODULATE sobre la luz);
 *  - una esfera de GLU32 (gluSphere) con brillo especular y una luz que da
 *    vueltas;
 *  - dos cristales de colores (mezcla con alfa) y una estrella con prueba de
 *    alfa;
 *  - el texto con wglUseFontBitmapsA (la letra del sistema) en 2D: los
 *    cuadros por segundo.
 * Uso: GLPRUEBA [/COMPLETA] [/PRUEBA] [/SALA] [/ANCHO n /ALTO n]
 *   /COMPLETA  a pantalla completa: ChangeDisplaySettingsA a 320×240 y una
 *              ventana sin marco de ese tamaño (como los juegos)
 *   /PRUEBA    la escena quieta (para las capturas de las pruebas)
 *   /SALA      la sala de medir (sala.c: como la de un juego de 1997, con
 *              multitextura), con los cuadros por segundo
 * Esc sale. Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * Se compila con gcc -m32 y tools/elf2pe.py --gui (ver el Makefile). */
#include "../../tests/progs/win32min.h"
#include "GL/gl.h"
#include "sala.h"

typedef unsigned char u8;
int WINAPI PeekMessageA(MSG *, HANDLE, u32, u32, u32);

HANDLE WINAPI GetDC(HANDLE);
int WINAPI ReleaseDC(HANDLE, HANDLE);
int WINAPI ChoosePixelFormat(HANDLE, const void *);
int WINAPI SetPixelFormat(HANDLE, int, const void *);
int WINAPI SwapBuffers(HANDLE);
int WINAPI ChangeDisplaySettingsA(void *, u32);
int WINAPI GetSystemMetrics(int);
HANDLE WINAPI wglCreateContext(HANDLE);
int WINAPI wglMakeCurrent(HANDLE, HANDLE);
int WINAPI wglDeleteContext(HANDLE);
int WINAPI wglUseFontBitmapsA(HANDLE, u32, u32, u32);
void WINAPI gluPerspective(GLdouble, GLdouble, GLdouble, GLdouble);
void *WINAPI gluNewQuadric(void);
void WINAPI gluQuadricNormals(void *, GLenum);
void WINAPI gluSphere(void *, GLdouble, GLint, GLint);
int WINAPI gluBuild2DMipmaps(GLenum, GLint, GLint, GLint, GLenum, GLenum, const void *);
#define WS_POPUP 0x80000000
#define GLU_SMOOTH 100000

static HANDLE ven, dc, rc;
static int ancho = 400, alto = 300, salir, prueba, completa, sala;
static unsigned tex_suelo, tex_caja, tex_estrella;
static void *esfera;
static u32 listas_letra;

static float seno(float x) { float r; __asm__("fsin" : "=t"(r) : "0"(x)); return r; }
static float coseno(float x) { float r; __asm__("fcos" : "=t"(r) : "0"(x)); return r; }
static void *pon(void *d, int v, u32 n) { u8 *p = d; while (n--) *p++ = (u8)v; return d; }
static int contiene(const char *s, const char *t)
{
	for (; *s; s++) {
		const char *a = s, *b = t;
		while (*a && *b && (*a == *b || (*a ^ 32) == *b)) { a++; b++; }
		if (!*b) return 1;
	}
	return 0;
}
static int num_tras(const char *s, const char *t, int def)
{
	for (; *s; s++) {
		const char *a = s, *b = t;
		while (*a && *b && (*a == *b || (*a ^ 32) == *b)) { a++; b++; }
		if (!*b) {
			int v = 0;
			while (*a == ' ') a++;
			while (*a >= '0' && *a <= '9') v = v * 10 + *a++ - '0';
			return v ? v : def;
		}
	}
	return def;
}

/* ---- las texturas, hechas aquí ---- */
static u8 img[128 * 128 * 4];
static void texturas(void)
{
	int x, y;
	/* baldosas: 128×128, con sus mipmaps de GLU */
	for (y = 0; y < 128; y++)
		for (x = 0; x < 128; x++) {
			u8 *o = img + (y * 128 + x) * 4;
			int junta = (x % 32) < 2 || (y % 32) < 2, cuadro = ((x / 32) + (y / 32)) & 1;
			o[0] = (u8)(junta ? 60 : cuadro ? 190 : 140);
			o[1] = (u8)(junta ? 55 : cuadro ? 170 : 110);
			o[2] = (u8)(junta ? 50 : cuadro ? 140 : 90);
			o[3] = 255;
		}
	glGenTextures(1, &tex_suelo);
	glBindTexture(GL_TEXTURE_2D, tex_suelo);
	gluBuild2DMipmaps(GL_TEXTURE_2D, 3, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, img);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	/* la caja: madera con una «H» */
	for (y = 0; y < 64; y++)
		for (x = 0; x < 64; x++) {
			u8 *o = img + (y * 64 + x) * 4;
			int borde = x < 4 || y < 4 || x >= 60 || y >= 60, veta = ((x * 7 + y * 3) / 5) % 9 < 2;
			int h = ((x >= 18 && x < 26) || (x >= 38 && x < 46) || (y >= 28 && y < 36 && x >= 18 && x < 46)) && y >= 14 && y < 50;
			o[0] = (u8)(h ? 230 : borde ? 90 : veta ? 150 : 190);
			o[1] = (u8)(h ? 200 : borde ? 50 : veta ? 90 : 120);
			o[2] = (u8)(h ? 40 : borde ? 20 : veta ? 40 : 60);
			o[3] = 255;
		}
	glGenTextures(1, &tex_caja);
	glBindTexture(GL_TEXTURE_2D, tex_caja);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	/* una estrella de cinco puntas con alfa todo o nada */
	for (y = 0; y < 64; y++)
		for (x = 0; x < 64; x++) {
			u8 *o = img + (y * 64 + x) * 4;
			int dx = x - 32, dy = y - 32, d2 = dx * dx + dy * dy, k, dentro = d2 < 100;
			/* cinco puntas: rayos desde el centro */
			for (k = 0; k < 5 && !dentro; k++) {
				static const int px[5] = { 0, 29, 18, -18, -29 }, py[5] = { -30, -9, 25, 25, -9 };
				int cr = dx * py[k] - dy * px[k], pr = dx * px[k] + dy * py[k];
				if (pr > 0 && pr < 30 * 30 && (cr < 0 ? -cr : cr) < (30 * 30 - pr) / 3) dentro = 1;
			}
			o[0] = 255; o[1] = (u8)(200 + y / 2); o[2] = 40; o[3] = (u8)(dentro ? 255 : 0);
		}
	glGenTextures(1, &tex_estrella);
	glBindTexture(GL_TEXTURE_2D, tex_estrella);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

static void caja(float s)
{
	static const float v[8][3] = { { -1, -1, -1 }, { 1, -1, -1 }, { 1, 1, -1 }, { -1, 1, -1 }, { -1, -1, 1 }, { 1, -1, 1 }, { 1, 1, 1 }, { -1, 1, 1 } };
	static const int c[6][4] = { { 0, 3, 2, 1 }, { 4, 5, 6, 7 }, { 0, 1, 5, 4 }, { 2, 3, 7, 6 }, { 1, 2, 6, 5 }, { 0, 4, 7, 3 } };
	static const float n[6][3] = { { 0, 0, -1 }, { 0, 0, 1 }, { 0, -1, 0 }, { 0, 1, 0 }, { 1, 0, 0 }, { -1, 0, 0 } };
	static const float t[4][2] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
	int f, k;
	glBegin(GL_QUADS);
	for (f = 0; f < 6; f++) {
		glNormal3fv(n[f]);
		for (k = 0; k < 4; k++) {
			glTexCoord2fv(t[k]);
			glVertex3f(v[c[f][k]][0] * s, v[c[f][k]][1] * s, v[c[f][k]][2] * s);
		}
	}
	glEnd();
}

static void texto(float x, float y, const char *s)
{
	int n = 0;
	if (!listas_letra) return;
	while (s[n]) n++;
	glRasterPos2f(x, y);
	glListBase(listas_letra);
	glCallLists(n, GL_UNSIGNED_BYTE, s);
}

static void dibuja(float t, int cps)
{
	static const float fondo[4] = { 0.45f, 0.55f, 0.7f, 1 };
	float luz_pos[4] = { 3 * coseno(t * 0.7f), 2.5f, 3 * seno(t * 0.7f) - 6, 1 };
	static const float blanco[4] = { 1, 1, 1, 1 }, rojo[4] = { 0.8f, 0.15f, 0.1f, 1 }, amb[4] = { 0.25f, 0.25f, 0.3f, 1 };
	char linea[40];
	int k;
	glViewport(0, 0, ancho, alto);
	glClearColor(fondo[0], fondo[1], fondo[2], 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60, (double)ancho / alto, 0.5, 60);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(0, -0.6f, 0);
	glRotatef(12, 1, 0, 0);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_CULL_FACE);
	/* la niebla */
	glEnable(GL_FOG);
	glFogi(GL_FOG_MODE, GL_LINEAR);
	glFogf(GL_FOG_START, 6);
	glFogf(GL_FOG_END, 40);
	glFogfv(GL_FOG_COLOR, fondo);
	/* el suelo */
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, tex_suelo);
	glColor3f(1, 1, 1);
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0); glVertex3f(-20, -1, 2);
	glTexCoord2f(10, 0); glVertex3f(20, -1, 2);
	glTexCoord2f(10, 10); glVertex3f(20, -1, -38);
	glTexCoord2f(0, 10); glVertex3f(-20, -1, -38);
	glEnd();
	/* la luz */
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, amb);
	glLightfv(GL_LIGHT0, GL_POSITION, luz_pos);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, blanco);
	glLightfv(GL_LIGHT0, GL_SPECULAR, blanco);
	/* la caja con textura (la luz la modula) */
	glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, blanco);
	glMaterialf(GL_FRONT, GL_SHININESS, 0);
	glBindTexture(GL_TEXTURE_2D, tex_caja);
	glPushMatrix();
	glTranslatef(-1.3f, 0.1f, -5);
	glRotatef(t * 40, 0, 1, 0);
	glRotatef(t * 23, 1, 0, 0);
	caja(0.8f);
	glPopMatrix();
	glDisable(GL_TEXTURE_2D);
	/* la esfera, con brillo */
	glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, rojo);
	glMaterialfv(GL_FRONT, GL_SPECULAR, blanco);
	glMaterialf(GL_FRONT, GL_SHININESS, 30);
	glPushMatrix();
	glTranslatef(1.4f, 0, -6);
	gluSphere(esfera, 0.9, 20, 14);
	glPopMatrix();
	glMaterialfv(GL_FRONT, GL_SPECULAR, (const float[4]){ 0, 0, 0, 1 });
	glDisable(GL_LIGHTING);
	glDisable(GL_CULL_FACE);
	/* la estrella: prueba de alfa, mirando a la cámara */
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, tex_estrella);
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.5f);
	glPushMatrix();
	glTranslatef(0, 1.4f, -9);
	glRotatef(t * 30, 0, 0, 1);
	glBegin(GL_QUADS);
	glTexCoord2f(0, 1); glVertex3f(-1, -1, 0);
	glTexCoord2f(1, 1); glVertex3f(1, -1, 0);
	glTexCoord2f(1, 0); glVertex3f(1, 1, 0);
	glTexCoord2f(0, 0); glVertex3f(-1, 1, 0);
	glEnd();
	glPopMatrix();
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_FOG);
	/* los cristales: con mezcla, de atrás adelante, sin escribir z */
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_FALSE);
	for (k = 0; k < 2; k++) {
		glColor4f(k ? 0.2f : 0.1f, k ? 0.5f : 0.9f, k ? 1.0f : 0.3f, 0.45f);
		glPushMatrix();
		glTranslatef(k ? 0.5f : -0.4f, -0.2f, k ? -3.2f : -3.8f);
		glRotatef(k ? -20.0f : 25.0f, 0, 1, 0);
		glBegin(GL_QUADS);
		glVertex3f(-0.6f, -0.7f, 0); glVertex3f(0.6f, -0.7f, 0); glVertex3f(0.6f, 0.7f, 0); glVertex3f(-0.6f, 0.7f, 0);
		glEnd();
		glPopMatrix();
	}
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	/* el texto, en 2D */
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, ancho, 0, alto, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glColor3f(1, 1, 0.3f);
	texto(8, (float)alto - 20, "HispaGL - OpenGL 1.1");
	{
		int n = 0, v = cps;
		char d[12];
		int j = 0;
		do { d[j++] = (char)('0' + v % 10); v /= 10; } while (v);
		while (j) linea[n++] = d[--j];
		{ const char *s = " cuadros/s"; while (*s) linea[n++] = *s++; }
		linea[n] = 0;
	}
	glColor3f(1, 1, 1);
	if (!prueba) texto(8, 8, linea);
}

static int WINAPI proc(HANDLE h, u32 m, u32 w, u32 l)
{
	switch (m) {
	case WM_SIZE:
		if (l & 0xFFFF) { ancho = (int)(l & 0xFFFF); alto = (int)(l >> 16); }
		return 0;
	case WM_KEYDOWN:
		if (w == 27) salir = 1;
		return 0;
	case WM_DESTROY:
		salir = 1;
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProcA(h, m, w, l);
}

void inicio(void)
{
	WNDCLASSA c;
	MSG msg;
	u8 pfd[40];
	int f;
	u32 t0, t_cps, n_cps = 0, cps = 0;
	const char *cl = GetCommandLineA();
	prueba = contiene(cl, "/PRUEBA");
	completa = contiene(cl, "/COMPLETA");
	sala = contiene(cl, "/SALA");
	ancho = num_tras(cl, "/ANCHO", completa ? 320 : 400);
	alto = num_tras(cl, "/ALTO", completa ? 240 : 300);
	pon(&c, 0, sizeof c);
	c.style = 0x20;                     /* CS_OWNDC */
	c.lpfnWndProc = proc;
	c.hInstance = GetModuleHandleA(NULL);
	c.lpszClassName = "GLPruebaHispa32";
	RegisterClassA(&c);
	if (completa) {
		u8 modo[156];
		pon(modo, 0, sizeof modo);
		*(u16 *)(modo + 36) = sizeof modo;
		*(u32 *)(modo + 40) = 0x1C0000;   /* DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT */
		*(u32 *)(modo + 104) = 16;
		*(u32 *)(modo + 108) = (u32)ancho;
		*(u32 *)(modo + 112) = (u32)alto;
		ChangeDisplaySettingsA(modo, 4);   /* CDS_FULLSCREEN */
		ven = CreateWindowExA(0, "GLPruebaHispa32", "GLPRUEBA", WS_POPUP | WS_VISIBLE, 0, 0, ancho, alto, NULL, NULL, c.hInstance, NULL);
	} else {
		RECT r = { 0, 0, ancho, alto };
		/* el tamaño de fuera para que dentro quede ancho×alto (más o menos: el marco de Hispa) */
		ven = CreateWindowExA(0, "GLPruebaHispa32", "GLPRUEBA: HispaGL", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 40, 30,
				      ancho + 8, alto + 27, NULL, NULL, c.hInstance, NULL);
		GetClientRect(ven, &r);
		if (r.right > 0) { ancho = r.right; alto = r.bottom; }
	}
	dc = GetDC(ven);
	pon(pfd, 0, sizeof pfd);
	*(u16 *)pfd = 40; *(u16 *)(pfd + 2) = 1;
	*(u32 *)(pfd + 4) = 4 | 0x20 | 1;   /* PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER */
	pfd[9] = 16;                        /* cColorBits */
	pfd[23] = 16;                       /* cDepthBits */
	f = ChoosePixelFormat(dc, pfd);
	if (!f || !SetPixelFormat(dc, f, pfd) || !(rc = wglCreateContext(dc)) || !wglMakeCurrent(dc, rc)) {
		MessageBoxA(ven, "GLPRUEBA necesita OPENGL32.DLL (HispaGL).", "GLPRUEBA", 0);
		ExitProcess(1);
	}
	if (sala) juego_ini();
	else texturas();
	esfera = gluNewQuadric();
	gluQuadricNormals(esfera, GLU_SMOOTH);
	listas_letra = glGenLists(128);
	if (!wglUseFontBitmapsA(dc, 0, 128, listas_letra)) listas_letra = 0;
	t0 = t_cps = GetTickCount();
	while (!salir) {
		u32 ahora;
		while (PeekMessageA(&msg, NULL, 0, 0, 1)) {
			if (msg.message == 0x12) salir = 1;   /* WM_QUIT */
			TranslateMessage(&msg);
			DispatchMessageA(&msg);
		}
		if (salir) break;
		ahora = GetTickCount();
		if (sala) {
			static int cuadro;
			char t[24];
			int n = 0, v = cps, j = 0;
			char d[12];
			juego_dibuja(ancho, alto, prueba ? 40 : cuadro++);
			do { d[j++] = (char)('0' + v % 10); v /= 10; } while (v);
			while (j) t[n++] = d[--j];
			{ const char *q = " cuadros/s"; while (*q) t[n++] = *q++; }
			t[n] = 0;
			glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
			glOrtho(0, ancho, 0, alto, -1, 1);
			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();
			glColor3f(1, 1, 0.3f);
			if (!prueba) texto(8, (float)alto - 18, t);
		} else dibuja(prueba ? 1.0f : (float)(ahora - t0) / 1000.0f, cps);
		SwapBuffers(dc);
		n_cps++;
		if (ahora - t_cps >= 1000) { cps = (int)(n_cps * 1000 / (ahora - t_cps)); n_cps = 0; t_cps = ahora; }
		if (prueba) Sleep(20);
	}
	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(rc);
	ReleaseDC(ven, dc);
	if (completa) ChangeDisplaySettingsA(NULL, 0);
	DestroyWindow(ven);
	ExitProcess(0);
}
