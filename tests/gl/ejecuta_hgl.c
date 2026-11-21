/* tests/gl/ejecuta_hgl.c - dibuja las escenas de prueba con HispaGL en el PC
 * (Linux, 32 bits, x87: el mismo código de la DLL, compilado igual) y deja
 * cada imagen en un .ppm; o mide lo que tarda la escena del juego. No usa la
 * biblioteca de C (no hace falta la de 32 bits en el PC): llama al núcleo de
 * Linux con INT 80h y lleva su propio montón.
 *   ejecuta_hgl CARPETA [calidad]          las escenas y el juego, en CARPETA (.ppm)
 *   ejecuta_hgl --mide ANCHO ALTO N calidad filtro   N cuadros del juego
 *        (filtro: 0 el de punto más cercano, 1 bilineal con mipmaps)
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "hgl.h"
#include "escenas.h"

void juego_filtro(int min, int mag);
extern int hgl_calidad;

static int sc(int n, int a, int b, int c)
{
	int r;
	__asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "b"(a), "c"(b), "d"(c) : "memory");
	return r;
}
static void sale(int c) { sc(1, c, 0, 0); for (;;); }
static int largo(const char *s) { int n = 0; while (s[n]) n++; return n; }
static void escribe(int fd, const char *s) { sc(4, fd, (int)s, largo(s)); }
static void numero(int fd, u32 v)
{
	char b[12]; int k = 11;
	b[k] = 0;
	do { b[--k] = (char)('0' + v % 10); v /= 10; } while (v);
	escribe(fd, b + k);
}
static int a_int(const char *s) { int v = 0; while (*s >= '0' && *s <= '9') v = v * 10 + *s++ - '0'; return v; }
static u64 ahora_us(void)
{
	struct { int s, ns; } t;
	sc(265, 1, (int)&t, 0);              /* clock_gettime(CLOCK_MONOTONIC) */
	return (u64)t.s * 1000000u + (u32)t.ns / 1000u;
}

/* ---- el montón: cajas de potencias de 2 ---- */
#define MONTON (256u << 20)
static u8 monton[MONTON] __attribute__((aligned(16)));
static u32 usado;
static void *libres[32];
void *hgl_pedir(u32 n)
{
	int c = 4;
	u32 *p;
	while ((1u << c) < n + 16) c++;
	if (libres[c]) {
		p = libres[c];
		libres[c] = *(void **)(p + 4);
		hgl_pon(p + 4, 0, (1u << c) - 16);
	} else {
		if (usado + (1u << c) > MONTON) { escribe(2, "sin memoria\n"); sale(3); }
		p = (u32 *)(monton + usado);
		usado += 1u << c;
	}
	p[0] = (u32)c;
	return p + 4;
}
void hgl_soltar(void *q)
{
	u32 *p;
	if (!q) return;
	p = (u32 *)q - 4;
	*(void **)(p + 4) = libres[p[0]];
	libres[p[0]] = p;
}
void hgl_traza(const char *s, u32 v) { (void)s; (void)v; }

static void guarda_ppm(Ctx *C, const char *ruta)
{
	static u8 lin[4096 * 3];
	static const char cab[] = "P6\n";
	char t[32];
	int fd = sc(5, (int)ruta, 0x241, 0644), x, y;   /* O_WRONLY|O_CREAT|O_TRUNC */
	if (fd < 0) { escribe(2, "no se puede escribir "); escribe(2, ruta); escribe(2, "\n"); sale(2); }
	escribe(fd, cab);
	{
		int k = 0;
		u32 v;
		char b[12]; int j;
		for (v = (u32)C->an, j = 11, b[11] = 0; ; ) { b[--j] = (char)('0' + v % 10); v /= 10; if (!v) break; }
		while (b[j]) t[k++] = b[j++];
		t[k++] = ' ';
		for (v = (u32)C->al, j = 11; ; ) { b[--j] = (char)('0' + v % 10); v /= 10; if (!v) break; }
		while (b[j]) t[k++] = b[j++];
		t[k++] = '\n'; t[k++] = '2'; t[k++] = '5'; t[k++] = '5'; t[k++] = '\n'; t[k] = 0;
		escribe(fd, t);
	}
	for (y = 0; y < C->al; y++) {
		for (x = 0; x < C->an; x++) {
			u16 p = C->color[y * C->an + x];
			int r = p >> 11, g = (p >> 5) & 63, b = p & 31;
			lin[x * 3] = (u8)((r << 3) | (r >> 2));
			lin[x * 3 + 1] = (u8)((g << 2) | (g >> 4));
			lin[x * 3 + 2] = (u8)((b << 3) | (b >> 2));
		}
		sc(4, fd, (int)lin, C->an * 3);
	}
	sc(6, fd, 0, 0);
}

static void une(char *d, const char *a, const char *b, const char *c)
{
	while (*a) *d++ = *a++;
	while (*b) *d++ = *b++;
	while (*c) *d++ = *c++;
	*d = 0;
}

int inicio(int argc, char **argv);
__asm__(".globl _start\n_start:\n\txor %ebp, %ebp\n\tmov %esp, %eax\n\tand $-16, %esp\n\tsub $8, %esp\n\tlea 4(%eax), %ecx\n\tpush %ecx\n\tpush (%eax)\n\tcall inicio\n\tmov %eax, %ebx\n\tmov $1, %eax\n\tint $0x80\n");

int inicio(int argc, char **argv)
{
	Ctx *C;
	int k;
	if (argc >= 7 && argv[1][0] == '-') {
		int w = a_int(argv[2]), h = a_int(argv[3]), n = a_int(argv[4]), cal = a_int(argv[5]), fil = a_int(argv[6]);
		u64 t0, t1;
		u32 tri = 0, pix = 0;
		hgl_calidad = cal;
		C = hgl_nuevo(NULL);
		hgl_c = C;
		hgl_tamano(C, w, h);
		juego_ini();
		juego_filtro(fil ? GL_LINEAR_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_NEAREST, fil ? GL_LINEAR : GL_NEAREST);
		juego_dibuja(w, h, 0);
		C->n_tri = C->n_pix = 0;
		t0 = ahora_us();
		for (k = 0; k < n; k++) juego_dibuja(w, h, k);
		t1 = ahora_us();
		tri = C->n_tri; pix = C->n_pix;
		escribe(1, "us_por_cuadro "); numero(1, (u32)(t1 - t0) / (u32)n);
		escribe(1, " triangulos "); numero(1, tri / (u32)n);
		escribe(1, " puntos "); numero(1, pix / (u32)n);
		escribe(1, "\n");
		if (argc >= 8) guarda_ppm(C, argv[7]);
		return 0;
	}
	if (argc < 2) { escribe(2, "uso: ejecuta_hgl CARPETA [calidad]\n"); return 1; }
	hgl_calidad = argc >= 3 ? a_int(argv[2]) : 2;
	C = hgl_nuevo(NULL);
	hgl_c = C;
	hgl_tamano(C, 320, 240);
	escenas_ini();
	juego_ini();
	for (k = 0; escenas[k].nombre; k++) {
		char ruta[512];
		escenas[k].dibuja(320, 240);
		une(ruta, argv[1], "/", escenas[k].nombre);
		une(ruta + largo(ruta), ".ppm", "", "");
		guarda_ppm(C, ruta);
		if (glGetError()) { escribe(2, "error de GL en "); escribe(2, escenas[k].nombre); escribe(2, "\n"); }
	}
	{
		char ruta[512];
		juego_dibuja(320, 240, 40);
		une(ruta, argv[1], "/juego.ppm", "");
		guarda_ppm(C, ruta);
	}
	return 0;
}
