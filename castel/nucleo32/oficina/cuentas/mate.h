/* mate.h - Cuentas 2: las funciones de coma flotante que no están en el
 * math.h pequeño de Hispa-Pentium (oficina/comun/inc): en Pentium, las del
 * x87 de so_pentium.c; en Linux, las de la libm.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef CU_MATE_H
#define CU_MATE_H
#include <math.h>
#if defined(OFI_PENTIUM)
double so_ln(double x);
double so_pot(double x, double y);
double so_sen(double x);
double so_cos(double x);
double so_atan2(double y, double x);
#define cu_ln(x) so_ln(x)
#define cu_pot(x, y) so_pot(x, y)
#define cu_sen(x) so_sen(x)
#define cu_cos(x) so_cos(x)
#define cu_atan2(y, x) so_atan2(y, x)
#else
#define cu_ln(x) log(x)
#define cu_pot(x, y) pow(x, y)
#define cu_sen(x) sin(x)
#define cu_cos(x) cos(x)
#define cu_atan2(y, x) atan2(y, x)
#endif
#define CU_E 2.71828182845904523536
#define CU_PI 3.14159265358979323846
static inline double cu_exp(double x)
{
	if (x > 709.78)
		return 1e308 * 10;
	if (x < -745)
		return 0;
	return cu_pot(CU_E, x);
}
static inline double cu_log10(double x)
{
	return cu_ln(x) / 2.30258509299404568402;
}
#endif
