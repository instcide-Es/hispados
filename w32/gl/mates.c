/* mates.c - HispaGL: lo de matemáticas y memoria que no da la DLL (sin la
 * biblioteca de C): raíz, seno, coseno, potencia y exponencial con el x87
 * (FSQRT, FSIN, FCOS, FYL2X, F2XM1: del 387 y del 486 en adelante), y las
 * copias de memoria (gcc las llama solo para copiar estructuras).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "hgl.h"

#ifndef HGL_ANFITRION
void *memcpy(void *d, const void *s, u32 n);
void *memset(void *d, int v, u32 n);
void *memmove(void *d, const void *s, u32 n);
void *memcpy(void *d, const void *s, u32 n) { hgl_copia(d, s, n); return d; }
void *memset(void *d, int v, u32 n) { hgl_pon(d, v, n); return d; }
void *memmove(void *d, const void *s, u32 n)
{
	u8 *a = d; const u8 *b = s;
	if (a < b || a >= b + n) { hgl_copia(d, s, n); return d; }
	while (n--) a[n] = b[n];
	return d;
}
#endif

void hgl_copia(void *d, const void *s, u32 n)
{
#if defined(__i386__)
	u32 k = n >> 2;
	__asm__ volatile("rep movsl" : "+D"(d), "+S"(s), "+c"(k) :: "memory");
	k = n & 3;
	__asm__ volatile("rep movsb" : "+D"(d), "+S"(s), "+c"(k) :: "memory");
#else
	u8 *a = d; const u8 *b = s;
	while (n--) *a++ = *b++;
#endif
}

void hgl_pon(void *d, int v, u32 n)
{
#if defined(__i386__)
	u32 x = (u8)v * 0x01010101u, k = n >> 2;
	__asm__ volatile("rep stosl" : "+D"(d), "+c"(k) : "a"(x) : "memory");
	k = n & 3;
	__asm__ volatile("rep stosb" : "+D"(d), "+c"(k) : "a"(x) : "memory");
#else
	u8 *a = d;
	while (n--) *a++ = (u8)v;
#endif
}

#if defined(__i386__)
float hgl_raiz(float x) { float r; if (x <= 0) return 0; __asm__("fsqrt" : "=t"(r) : "0"(x)); return r; }
float hgl_seno(float x) { float r; __asm__("fsin" : "=t"(r) : "0"(x)); return r; }
float hgl_coseno(float x) { float r; __asm__("fcos" : "=t"(r) : "0"(x)); return r; }
/* 2^x: la parte entera con FSCALE y la fracción con F2XM1 (|f| <= 1) */
static float dos_a(float x)
{
	float r;
	__asm__("fld %%st(0)\n\t"         /* x, x */
		"frndint\n\t"                /* n, x */
		"fxch\n\t"                   /* x, n */
		"fsub %%st(1), %%st\n\t"     /* x - n, n */
		"f2xm1\n\t"
		"fld1\n\t"
		"faddp\n\t"
		"fscale\n\t"
		"fstp %%st(1)"
		: "=t"(r) : "0"(x));
	return r;
}
float hgl_pot(float b, float e)
{
	float l;
	if (b <= 0) return e == 0 ? 1 : 0;
	if (e == 0) return 1;
	if (e == 1) return b;
	__asm__("fyl2x" : "=t"(l) : "0"(b), "u"(e) : "st(1)");   /* e·log2(b) */
	if (l < -126) return 0;
	if (l > 126) l = 126;
	return dos_a(l);
}
float hgl_expo(float x)
{
	float l = x * 1.4426950408889634f;
	if (l < -126) return 0;
	if (l > 126) l = 126;
	return dos_a(l);
}
#else
float hgl_raiz(float x) { return x <= 0 ? 0 : __builtin_sqrtf(x); }
float hgl_seno(float x) { return __builtin_sinf(x); }
float hgl_coseno(float x) { return __builtin_cosf(x); }
float hgl_pot(float b, float e) { return b <= 0 ? (e == 0 ? 1 : 0) : __builtin_powf(b, e); }
float hgl_expo(float x) { return __builtin_expf(x); }
#endif
