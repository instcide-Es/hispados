/* math.h mínimo para la oficina de 32 bits en Hispa-Pentium (sin biblioteca de C: -ffreestanding).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef OFI_MATH_H
#define OFI_MATH_H
double sqrt(double);
double floor(double);
double ceil(double);
double fabs(double);
double pow(double, double);
double fmod(double, double);
double cos(double);
double acos(double);
double frexp(double, int *);     /* (la pide stb_image_write; los gcc nuevos no dejan pasar la declaración implícita) */
#endif
