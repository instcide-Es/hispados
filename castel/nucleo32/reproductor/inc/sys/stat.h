/* sys/stat.h para el Reproductor en Hispa-Pentium (libxmp lo nombra; no se usa). Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <sys/types.h>
struct stat { long st_size; int st_mode; };
#define S_ISDIR(m) 0
#define S_ISREG(m) 1
int stat(const char *, struct stat *);
int fstat(int, struct stat *);
