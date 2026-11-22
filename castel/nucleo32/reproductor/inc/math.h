/* math.h mínimo para el Reproductor en Hispa-Pentium: lo del x87 (so_pentium.c). Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef REPRO_MATH_H
#define REPRO_MATH_H
double sqrt(double); float sqrtf(float);
double floor(double); double ceil(double); float floorf(float); float ceilf(float);
double fabs(double); float fabsf(float);
double pow(double, double); float powf(float, float);
double fmod(double, double);
double cos(double); double sin(double); float cosf(float); float sinf(float);
double atan(double); double atan2(double, double);
double exp(double); float expf(float);
double log(double); float logf(float); double log10(double);
double ldexp(double, int); float ldexpf(float, int);
double frexp(double, int *);
double modf(double, double *);
double round(double);
long lrintf(float);
#define M_PI 3.14159265358979323846
#define HUGE_VAL __builtin_huge_val()
#define INFINITY __builtin_inff()
#define NAN __builtin_nanf("")
#define isnan(x) __builtin_isnan(x)
#define isinf(x) __builtin_isinf(x)
#endif
