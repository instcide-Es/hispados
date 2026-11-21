/* engrana.c - «ENGRANA», los engranajes de HispaGL: tres ruedas dentadas con
 * luz (listas de visualización, sombreado plano y suave, GL_NORMALIZE), que
 * giran; los cuadros por segundo, en el título y abajo (wglUseFontBitmapsA).
 * Es la prueba de velocidad de siempre de OpenGL (como la de los sistemas de
 * ventanas de Unix), hecha aquí. Uso: ENGRANA [/PRUEBA] [/ANCHO n /ALTO n].
 * Flechas: girar la vista. Esc sale.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * Se compila con gcc -m32 y tools/elf2pe.py --gui (ver el Makefile). */
#include "../../tests/progs/win32min.h"
#include "GL/gl.h"

typedef unsigned char u8;
int WINAPI PeekMessageA(MSG *, HANDLE, u32, u32, u32);

HANDLE WINAPI GetDC(HANDLE);
int WINAPI ReleaseDC(HANDLE, HANDLE);
int WINAPI ChoosePixelFormat(HANDLE, const void *);
int WINAPI SetPixelFormat(HANDLE, int, const void *);
int WINAPI SwapBuffers(HANDLE);
HANDLE WINAPI wglCreateContext(HANDLE);
int WINAPI wglMakeCurrent(HANDLE, HANDLE);
int WINAPI wglDeleteContext(HANDLE);
int WINAPI wglUseFontBitmapsA(HANDLE, u32, u32, u32);

#define PI 3.14159265358979f
static float seno(float x) { float r; __asm__("fsin" : "=t"(r) : "0"(x)); return r; }
static float coseno(float x) { float r; __asm__("fcos" : "=t"(r) : "0"(x)); return r; }
static float raiz(float x) { float r; __asm__("fsqrt" : "=t"(r) : "0"(x)); return r; }
static void *pon(void *d, int v, u32 n) { u8 *p = d; while (n--) *p++ = (u8)v; return d; }

static HANDLE ven, dc, rc;
static int ancho = 300, alto = 300, salir, prueba;
static float vista_x = 20, vista_y = 30;
static u32 listas, letras;

static void rueda(float r0, float r1, float ancho_, int dientes, float alto_)
{
	int i;
	float r2 = r1 + alto_, da = 2 * PI / dientes / 4, w = ancho_ * 0.5f;
	glShadeModel(GL_FLAT);
	glNormal3f(0, 0, 1);
	glBegin(GL_QUAD_STRIP);
	for (i = 0; i <= dientes; i++) {
		float a = i * 2 * PI / dientes;
		glVertex3f(r0 * coseno(a), r0 * seno(a), w);
		glVertex3f(r1 * coseno(a), r1 * seno(a), w);
		if (i < dientes) {
			glVertex3f(r0 * coseno(a), r0 * seno(a), w);
			glVertex3f(r1 * coseno(a + 3 * da), r1 * seno(a + 3 * da), w);
		}
	}
	glEnd();
	glBegin(GL_QUADS);
	for (i = 0; i < dientes; i++) {
		float a = i * 2 * PI / dientes;
		glVertex3f(r1 * coseno(a), r1 * seno(a), w);
		glVertex3f(r2 * coseno(a + da), r2 * seno(a + da), w);
		glVertex3f(r2 * coseno(a + 2 * da), r2 * seno(a + 2 * da), w);
		glVertex3f(r1 * coseno(a + 3 * da), r1 * seno(a + 3 * da), w);
	}
	glEnd();
	glNormal3f(0, 0, -1);
	glBegin(GL_QUAD_STRIP);
	for (i = 0; i <= dientes; i++) {
		float a = i * 2 * PI / dientes;
		glVertex3f(r1 * coseno(a), r1 * seno(a), -w);
		glVertex3f(r0 * coseno(a), r0 * seno(a), -w);
		if (i < dientes) {
			glVertex3f(r1 * coseno(a + 3 * da), r1 * seno(a + 3 * da), -w);
			glVertex3f(r0 * coseno(a), r0 * seno(a), -w);
		}
	}
	glEnd();
	glBegin(GL_QUADS);
	for (i = 0; i < dientes; i++) {
		float a = i * 2 * PI / dientes;
		glVertex3f(r1 * coseno(a + 3 * da), r1 * seno(a + 3 * da), -w);
		glVertex3f(r2 * coseno(a + 2 * da), r2 * seno(a + 2 * da), -w);
		glVertex3f(r2 * coseno(a + da), r2 * seno(a + da), -w);
		glVertex3f(r1 * coseno(a), r1 * seno(a), -w);
	}
	glEnd();
	glBegin(GL_QUAD_STRIP);
	for (i = 0; i < dientes; i++) {
		float a = i * 2 * PI / dientes, u, v, l;
		glVertex3f(r1 * coseno(a), r1 * seno(a), w);
		glVertex3f(r1 * coseno(a), r1 * seno(a), -w);
		u = r2 * coseno(a + da) - r1 * coseno(a);
		v = r2 * seno(a + da) - r1 * seno(a);
		l = raiz(u * u + v * v);
		glNormal3f(v / l, -u / l, 0);
		glVertex3f(r2 * coseno(a + da), r2 * seno(a + da), w);
		glVertex3f(r2 * coseno(a + da), r2 * seno(a + da), -w);
		glNormal3f(coseno(a), seno(a), 0);
		glVertex3f(r2 * coseno(a + 2 * da), r2 * seno(a + 2 * da), w);
		glVertex3f(r2 * coseno(a + 2 * da), r2 * seno(a + 2 * da), -w);
		u = r1 * coseno(a + 3 * da) - r2 * coseno(a + 2 * da);
		v = r1 * seno(a + 3 * da) - r2 * seno(a + 2 * da);
		glNormal3f(v, -u, 0);
		glVertex3f(r1 * coseno(a + 3 * da), r1 * seno(a + 3 * da), w);
		glVertex3f(r1 * coseno(a + 3 * da), r1 * seno(a + 3 * da), -w);
		glNormal3f(coseno(a), seno(a), 0);
	}
	glVertex3f(r1, 0, w);
	glVertex3f(r1, 0, -w);
	glEnd();
	glShadeModel(GL_SMOOTH);
	glBegin(GL_QUAD_STRIP);
	for (i = 0; i <= dientes; i++) {
		float a = i * 2 * PI / dientes;
		glNormal3f(-coseno(a), -seno(a), 0);
		glVertex3f(r0 * coseno(a), r0 * seno(a), -w);
		glVertex3f(r0 * coseno(a), r0 * seno(a), w);
	}
	glEnd();
}

static void prepara(void)
{
	static const float pos[4] = { 5, 5, 10, 0 }, rojo[4] = { 0.8f, 0.1f, 0, 1 }, verde[4] = { 0, 0.8f, 0.2f, 1 }, azul[4] = { 0.2f, 0.2f, 1, 1 };
	glLightfv(GL_LIGHT0, GL_POSITION, pos);
	glEnable(GL_CULL_FACE);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_DEPTH_TEST);
	listas = glGenLists(3);
	glNewList(listas, GL_COMPILE);
	glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, rojo);
	rueda(1, 4, 1, 20, 0.7f);
	glEndList();
	glNewList(listas + 1, GL_COMPILE);
	glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, verde);
	rueda(0.5f, 2, 2, 10, 0.7f);
	glEndList();
	glNewList(listas + 2, GL_COMPILE);
	glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, azul);
	rueda(1.3f, 2, 0.5f, 10, 0.7f);
	glEndList();
	glEnable(GL_NORMALIZE);
}

static void dibuja(float ang, int cps)
{
	float h = (float)alto / ancho;
	glViewport(0, 0, ancho, alto);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glFrustum(-1, 1, -h, h, 5, 60);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(0, 0, -40);
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_LIGHTING);
	glEnable(GL_DEPTH_TEST);
	glPushMatrix();
	glRotatef(vista_x, 1, 0, 0);
	glRotatef(vista_y, 0, 1, 0);
	glPushMatrix();
	glTranslatef(-3, -2, 0);
	glRotatef(ang, 0, 0, 1);
	glCallList(listas);
	glPopMatrix();
	glPushMatrix();
	glTranslatef(3.1f, -2, 0);
	glRotatef(-2 * ang - 9, 0, 0, 1);
	glCallList(listas + 1);
	glPopMatrix();
	glPushMatrix();
	glTranslatef(-3.1f, 4.2f, 0);
	glRotatef(-2 * ang - 25, 0, 0, 1);
	glCallList(listas + 2);
	glPopMatrix();
	glPopMatrix();
	if (letras && !prueba) {
		char t[24];
		int n = 0, v = cps, j = 0;
		char d[12];
		do { d[j++] = (char)('0' + v % 10); v /= 10; } while (v);
		while (j) t[n++] = d[--j];
		{ const char *s = " cuadros/s"; while (*s) t[n++] = *s++; }
		glDisable(GL_LIGHTING);
		glDisable(GL_DEPTH_TEST);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0, ancho, 0, alto, -1, 1);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glColor3f(1, 1, 1);
		glRasterPos2i(6, 6);
		glListBase(letras);
		glCallLists(n, GL_UNSIGNED_BYTE, t);
	}
}

static int WINAPI proc(HANDLE h, u32 m, u32 w, u32 l)
{
	switch (m) {
	case WM_SIZE:
		if (l & 0xFFFF) { ancho = (int)(l & 0xFFFF); alto = (int)(l >> 16); }
		return 0;
	case WM_KEYDOWN:
		if (w == 27) salir = 1;
		if (w == 0x25) vista_y += 5;      /* las flechas */
		if (w == 0x27) vista_y -= 5;
		if (w == 0x26) vista_x += 5;
		if (w == 0x28) vista_x -= 5;
		return 0;
	case WM_DESTROY:
		salir = 1;
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProcA(h, m, w, l);
}

static int contiene(const char *s, const char *t)
{
	for (; *s; s++) {
		const char *a = s, *b = t;
		while (*a && *b && (*a == *b || (*a ^ 32) == *b)) { a++; b++; }
		if (!*b) return 1;
	}
	return 0;
}

void inicio(void)
{
	WNDCLASSA c;
	MSG msg;
	u8 pfd[40];
	int f, cps = 0;
	u32 t0, t5, n5 = 0, n_cps = 0, t_cps;
	RECT r;
	prueba = contiene(GetCommandLineA(), "/PRUEBA");
	pon(&c, 0, sizeof c);
	c.style = 0x20;
	c.lpfnWndProc = proc;
	c.hInstance = GetModuleHandleA(NULL);
	c.lpszClassName = "EngranaHispa32";
	RegisterClassA(&c);
	ven = CreateWindowExA(0, "EngranaHispa32", "Engranajes (HispaGL)", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 60, 40,
			      ancho + 8, alto + 27, NULL, NULL, c.hInstance, NULL);
	if (GetClientRect(ven, &r) && r.right > 0) { ancho = r.right; alto = r.bottom; }
	dc = GetDC(ven);
	pon(pfd, 0, sizeof pfd);
	*(u16 *)pfd = 40; *(u16 *)(pfd + 2) = 1;
	*(u32 *)(pfd + 4) = 4 | 0x20 | 1;
	pfd[9] = 16; pfd[23] = 16;
	f = ChoosePixelFormat(dc, pfd);
	if (!f || !SetPixelFormat(dc, f, pfd) || !(rc = wglCreateContext(dc)) || !wglMakeCurrent(dc, rc)) {
		MessageBoxA(ven, "ENGRANA necesita OPENGL32.DLL (HispaGL).", "ENGRANA", 0);
		ExitProcess(1);
	}
	prepara();
	letras = glGenLists(128);
	if (!wglUseFontBitmapsA(dc, 0, 128, letras)) letras = 0;
	t0 = t5 = t_cps = GetTickCount();
	while (!salir) {
		u32 ahora;
		while (PeekMessageA(&msg, NULL, 0, 0, 1)) {
			if (msg.message == 0x12) salir = 1;
			TranslateMessage(&msg);
			DispatchMessageA(&msg);
		}
		if (salir) break;
		ahora = GetTickCount();
		dibuja(prueba ? 15.0f : (float)(ahora - t0) * 0.07f, cps);
		SwapBuffers(dc);
		n5++; n_cps++;
		if (ahora - t_cps >= 1000) { cps = (int)(n_cps * 1000 / (ahora - t_cps)); n_cps = 0; t_cps = ahora; }
		if (ahora - t5 >= 5000) {
			/* como la de siempre: «N cuadros en 5 s = X c/s», en el título */
			char t[64];
			int n = 0, v, j;
			char d[12];
			const char *s = "Engranajes: ";
			while (*s) t[n++] = *s++;
			v = (int)(n5 * 10000 / (ahora - t5));   /* en décimas */
			j = 0;
			do { d[j++] = (char)('0' + v % 10); v /= 10; } while (v);
			if (j == 1) d[j++] = '0';
			while (j > 1) t[n++] = d[--j];
			t[n++] = ','; t[n++] = d[0];
			s = " cuadros/s"; while (*s) t[n++] = *s++;
			t[n] = 0;
			SetWindowTextA(ven, t);
			n5 = 0; t5 = ahora;
		}
		if (prueba) Sleep(20);
	}
	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(rc);
	ReleaseDC(ven, dc);
	DestroyWindow(ven);
	ExitProcess(0);
}
