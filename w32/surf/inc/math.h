/* Cabecera de la biblioteca de C para compilar NetSurf (terceros/netsurf) en
 * NAVEGA2.EXE, el Navegador completo de Hispa-486 (w32/surf, docs/486/NAVEGADOR2.md):
 * lo de MSVCRT.DLL con sus estructuras, y lo que falta en w32/surf/apoyo.c.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HS_MATH_H
#define HS_MATH_H
#define NAN __builtin_nanf("")
#define INFINITY __builtin_inff()
#define HUGE_VAL __builtin_huge_val()
#define HUGE_VALF __builtin_huge_valf()
#define M_PI 3.14159265358979323846
#define M_PI_2 1.57079632679489661923
#define M_SQRT2 1.41421356237309504880
#define M_E 2.7182818284590452354
#define M_LN2 0.69314718055994530942
#define isnan(x) __builtin_isnan(x)
#define isinf(x) __builtin_isinf(x)
#define isfinite(x) __builtin_isfinite(x)
#define signbit(x) __builtin_signbit(x)
double fabs(double); double floor(double); double ceil(double); double trunc(double); double round(double);
double sqrt(double); double hypot(double, double); double pow(double, double);
double exp(double); double log(double); double log10(double); double log2(double);
double sin(double); double cos(double); double tan(double); double asin(double); double acos(double); double atan(double); double atan2(double, double);
double fmod(double, double); double fmin(double, double); double fmax(double, double); double copysign(double, double);
double ldexp(double, int); double frexp(double, int *); double modf(double, double *);
long lround(double); long lrint(double); double rint(double);
float fabsf(float); float floorf(float); float ceilf(float); float sqrtf(float); float roundf(float);
float sinf(float); float cosf(float); float tanf(float); float atan2f(float, float); float powf(float, float);
float fmodf(float, float); float truncf(float); float fminf(float, float); float fmaxf(float, float);
long lroundf(float);
#endif
