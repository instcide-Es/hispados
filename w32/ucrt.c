/* ucrt.c - UCRT.DLL de Hispa-486 (fase 3d, docs/486/HISPA-486.md): la
 * biblioteca de C de los programas de Visual C++ 2015 y siguientes (las DLL
 * api-ms-win-crt-*.dll, VCRUNTIME140.DLL y UCRTBASE.DLL, que HW32 lleva todas
 * a esta). Es MSVCRT.DLL (w32/msvcrt.c, que va en la misma DLL) más lo que
 * cambió de nombre o de forma en el CRT «universal»: el printf de todo
 * (__stdio_common_vfprintf...), el arranque (_configure_narrow_argv,
 * _initialize_onexit_table...) y alguna más.
 * Sala limpia: a partir de la documentación pública de la biblioteca.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * Se compila con gcc -m32 y tools/elf2pe.py --dll (ver el Makefile). */
#include <stdarg.h>
typedef unsigned int u32;
typedef unsigned long long u64;
typedef unsigned int size_t;
typedef struct _iobuf FILE;
typedef void (*FUNC)(void);
#define NULL ((void *)0)

int vfprintf(FILE *, const char *, va_list);
int vsnprintf(char *, size_t, const char *, va_list);
int _vscprintf(const char *, va_list);
int vsscanf(const char *, const char *, va_list);
int atexit(FUNC);
void abort(void);
double floor(double);
unsigned _controlfp(unsigned, unsigned);
extern char *_acmdln;

/* el printf y el scanf de todo: (opciones de 64 bits, ..., locale, va_list) */
int __stdio_common_vfprintf(u64 op, FILE *f, const char *fm, void *loc, va_list ap)
{
  return vfprintf(f, fm, ap);
}
int __stdio_common_vfprintf_s(u64 op, FILE *f, const char *fm, void *loc, va_list ap)
{
  return vfprintf(f, fm, ap);
}
int __stdio_common_vsprintf(u64 op, char *b, size_t n, const char *fm, void *loc, va_list ap)
{
  int r;
  if (!b) return _vscprintf(fm, ap);
  r = vsnprintf(b, n, fm, ap);
  if (n && r >= (int)n && !(op & 2)) {   /* (sin la conducta de C99: -1 si no cabe) */
    b[n - 1] = 0;
    return -1;
  }
  return r;
}
int __stdio_common_vsprintf_s(u64 op, char *b, size_t n, const char *fm, void *loc, va_list ap)
{
  return __stdio_common_vsprintf(op, b, n, fm, loc, ap);
}
int __stdio_common_vsnprintf_s(u64 op, char *b, size_t n, size_t m, const char *fm, void *loc, va_list ap)
{
  return __stdio_common_vsprintf(op, b, m < n ? m + 1 : n, fm, loc, ap);
}
int __stdio_common_vsscanf(u64 op, const char *s, size_t n, const char *fm, void *loc, va_list ap)
{
  return vsscanf(s, fm, ap);
}

/* el arranque */
int _configure_narrow_argv(int modo) { return 0; }
int _initialize_narrow_environment(void) { return 0; }
char *_get_narrow_winmain_command_line(void)
{
  char *p = _acmdln;
  int comillas = 0;
  if (!p) return "";
  while (*p && (comillas || (*p != ' ' && *p != '\t'))) {   /* el nombre del programa */
    if (*p == '"') comillas = !comillas;
    p++;
  }
  while (*p == ' ' || *p == '\t') p++;
  return p;
}
typedef struct { FUNC *primera, *ultima, *fin; } ONEXIT;
int _initialize_onexit_table(ONEXIT *t)
{
  if (t) t->primera = t->ultima = t->fin = NULL;
  return 0;
}
int _register_onexit_function(ONEXIT *t, FUNC f) { return atexit(f); }
int _execute_onexit_table(ONEXIT *t) { return 0; }
int _crt_atexit(FUNC f) { return atexit(f); }
int _crt_at_quick_exit(FUNC f) { return 0; }
void _register_thread_local_exe_atexit_callback(void *f) { }
int _seh_filter_exe(u32 codigo, void *ep) { return 0; }   /* EXCEPTION_CONTINUE_SEARCH */
int _seh_filter_dll(u32 codigo, void *ep) { return 0; }
void _set_app_type(int t) { }
int _set_fmode(int m) { return 0; }
int _get_fmode(int *m) { if (m) *m = 0x4000; return 0; }
int _set_new_mode(int m) { return 0; }
int _configthreadlocale(int m) { return 0; }
int _controlfp_s(unsigned *actual, unsigned nuevo, unsigned mascara)
{
  unsigned v = _controlfp(nuevo, mascara);
  if (actual) *actual = v;
  return 0;
}
void terminate(void) { abort(); }
int _query_new_mode(void) { return 0; }
int *__p___argc(void);
char ***__p___argv(void);
int _initialize_wide_environment(void) { return 0; }
int _configure_wide_argv(int m) { return 0; }

/* VCRUNTIME140: lo de las excepciones de C++ (no las hay) */
static void *nada;
void **__current_exception(void) { return &nada; }
void **__current_exception_context(void) { return &nada; }
void _CxxThrowException(void *o, void *t) { abort(); }
int __std_type_info_compare(void *a, void *b) { return a != b; }

/* math */
float roundf(float x) { return x < 0 ? -(float)floor(-x + 0.5) : (float)floor(x + 0.5); }
double round(double x) { return x < 0 ? -floor(-x + 0.5) : floor(x + 0.5); }
float floorf(float x) { return (float)floor(x); }
float ceilf(float x) { return -(float)floor(-x); }
float fabsf(float x) { return x < 0 ? -x : x; }
