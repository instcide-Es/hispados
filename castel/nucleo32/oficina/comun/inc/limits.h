/* limits.h mínimo para la oficina de 32 bits en Hispa-Pentium (sin biblioteca de C: -ffreestanding).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef OFI_LIMITS_H
#define OFI_LIMITS_H
#define CHAR_BIT 8
#define SCHAR_MIN (-128)
#define SCHAR_MAX 127
#define UCHAR_MAX 255
#define CHAR_MIN SCHAR_MIN
#define CHAR_MAX SCHAR_MAX
#define SHRT_MIN (-32768)
#define SHRT_MAX 32767
#define USHRT_MAX 65535
#define INT_MIN (-2147483647 - 1)
#define INT_MAX 2147483647
#define UINT_MAX 4294967295U
#define LONG_MIN INT_MIN
#define LONG_MAX INT_MAX
#define ULONG_MAX UINT_MAX
#endif
