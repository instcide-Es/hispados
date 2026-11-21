/* tests/gl/ejecuta_mesa.c - dibuja las escenas de prueba de HispaGL con Mesa
 * (llvmpipe, por EGL sin pantalla) para tener una referencia que no es
 * nuestra: tests/run_gl_tests.sh compara las dos. Programa del PC (64 bits,
 * con la biblioteca de C).
 *   ejecuta_mesa CARPETA
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdio.h>
#include <dlfcn.h>
#include "GL/gl.h"
#include "escenas.h"

typedef void *EGLDisplay, *EGLConfig, *EGLContext, *EGLSurface;
typedef int EGLint;

static unsigned char px[320 * 240 * 4];
static void guarda(const char *ruta, int w, int h)
{
	FILE *f = fopen(ruta, "wb");
	int y;
	if (!f) { perror(ruta); return; }
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px);
	fprintf(f, "P6\n%d %d\n255\n", w, h);
	for (y = h - 1; y >= 0; y--) fwrite(px + y * w * 3, 1, (size_t)w * 3, f);
	fclose(f);
}

int main(int argc, char **argv)
{
	void *e = dlopen("libEGL.so.1", RTLD_NOW);
	void *(*gpa)(const char *);
	EGLDisplay (*getpd)(int, void *, const EGLint *);
	EGLDisplay d;
	EGLConfig c;
	EGLContext x;
	EGLSurface s;
	EGLint n = 0, ma, mi;
	EGLint at[] = { 0x3024, 8, 0x3023, 8, 0x3022, 8, 0x3025, 16, 0x3033, 0x0001, 0x3040, 0x0008, 0x3038 };
	EGLint pa[] = { 0x3057, 320, 0x3056, 240, 0x3038 };
	EGLint ca[] = { 0x3038 };
	char ruta[1024];
	int k;
	if (argc < 2) { fprintf(stderr, "uso: ejecuta_mesa CARPETA\n"); return 1; }
	if (!e) { fprintf(stderr, "sin EGL\n"); return 2; }
	gpa = (void *(*)(const char *))dlsym(e, "eglGetProcAddress");
	getpd = (EGLDisplay (*)(int, void *, const EGLint *))gpa("eglGetPlatformDisplayEXT");
	if (!getpd) return 2;
	d = getpd(0x31DD, NULL, NULL);       /* EGL_PLATFORM_SURFACELESS_MESA */
	if (!((int (*)(EGLDisplay, EGLint *, EGLint *))dlsym(e, "eglInitialize"))(d, &ma, &mi)) return 2;
	((int (*)(int))dlsym(e, "eglBindAPI"))(0x30A2);   /* EGL_OPENGL_API */
	((int (*)(EGLDisplay, const EGLint *, EGLConfig *, EGLint, EGLint *))dlsym(e, "eglChooseConfig"))(d, at, &c, 1, &n);
	if (n < 1) return 2;
	x = ((EGLContext (*)(EGLDisplay, EGLConfig, EGLContext, const EGLint *))dlsym(e, "eglCreateContext"))(d, c, NULL, ca);
	s = ((EGLSurface (*)(EGLDisplay, EGLConfig, const EGLint *))dlsym(e, "eglCreatePbufferSurface"))(d, c, pa);
	if (!((int (*)(EGLDisplay, EGLSurface, EGLSurface, EGLContext))dlsym(e, "eglMakeCurrent"))(d, s, s, x)) return 2;
	escenas_ini();
	juego_ini();
	for (k = 0; escenas[k].nombre; k++) {
		escenas[k].dibuja(320, 240);
		snprintf(ruta, sizeof ruta, "%s/%s.ppm", argv[1], escenas[k].nombre);
		guarda(ruta, 320, 240);
		if (glGetError()) fprintf(stderr, "error de GL en %s\n", escenas[k].nombre);
	}
	juego_dibuja(320, 240, 40);
	snprintf(ruta, sizeof ruta, "%s/juego.ppm", argv[1]);
	guarda(ruta, 320, 240);
	return 0;
}
