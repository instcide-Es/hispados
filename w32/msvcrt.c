/* =============================================================================
 *  MSVCRT.DLL de Hispa-486: la biblioteca de C de los programas Win32 (los de
 *  MinGW y los de Visual C++ que la usan), sobre el KERNEL32 de HW32.
 *  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 *
 *  Escrita desde cero a partir de la norma de C y de la documentación pública
 *  de la biblioteca de Microsoft (nombres, estructuras y comportamiento). Las
 *  funciones son cdecl, como en la de Windows. Se compila con gcc -m32 sin
 *  biblioteca (-ffreestanding) y tools/elf2pe.py --dll.
 *  Los mensajes (strerror, perror...), en español, como todo Hispa-DOS.
 *  Sin cuentas de 64 bits que necesiten libgcc: las divisiones grandes, con
 *  «divl»; las conversiones de coma flotante, con el x87.
 * ============================================================================= */
#include <stdarg.h>

typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
typedef unsigned long long u64;
typedef long long s64;
typedef void *HANDLE;
typedef unsigned short wchar_t;
typedef unsigned int size_t;
#define WINAPI __attribute__((stdcall))
#define NULL ((void *)0)
#define INVALID ((HANDLE)-1)

/* ---- KERNEL32 ---- */
typedef struct { u32 lo, hi; } FT;
typedef struct { u16 anio, mes, sem, dia, hora, min, seg, ms; } ST;
typedef struct { u32 atrib; FT c, a, e; u32 tam_hi, tam_lo, r0, r1; char nombre[260]; char alt[14]; } FINDA;
HANDLE WINAPI GetStdHandle(int);
int WINAPI WriteFile(HANDLE, const void *, u32, u32 *, void *);
int WINAPI ReadFile(HANDLE, void *, u32, u32 *, void *);
HANDLE WINAPI CreateFileA(const char *, u32, u32, void *, u32, u32, HANDLE);
int WINAPI CloseHandle(HANDLE);
u32 WINAPI SetFilePointer(HANDLE, int, int *, u32);
u32 WINAPI GetFileSize(HANDLE, u32 *);
int WINAPI SetEndOfFile(HANDLE);
u32 WINAPI GetFileType(HANDLE);
int WINAPI FlushFileBuffers(HANDLE);
HANDLE WINAPI GetProcessHeap(void);
void *WINAPI HeapAlloc(HANDLE, u32, u32);
int WINAPI HeapFree(HANDLE, u32, void *);
void *WINAPI HeapReAlloc(HANDLE, u32, void *, u32);
u32 WINAPI HeapSize(HANDLE, u32, void *);
void WINAPI ExitProcess(u32);
char *WINAPI GetCommandLineA(void);
char *WINAPI GetEnvironmentStrings(void);
int WINAPI DeleteFileA(const char *);
int WINAPI MoveFileA(const char *, const char *);
u32 WINAPI GetFileAttributesA(const char *);
HANDLE WINAPI FindFirstFileA(const char *, FINDA *);
int WINAPI FindClose(HANDLE);
u32 WINAPI GetFullPathNameA(const char *, u32, char *, char **);
void WINAPI GetSystemTimeAsFileTime(FT *);
int WINAPI FileTimeToSystemTime(const FT *, ST *);
int WINAPI SystemTimeToFileTime(const ST *, FT *);
u32 WINAPI GetTickCount(void);
u32 WINAPI GetLastError(void);
int WINAPI CreateDirectoryA(const char *, void *);
int WINAPI RemoveDirectoryA(const char *);
u32 WINAPI GetCurrentDirectoryA(u32, char *);
int WINAPI SetCurrentDirectoryA(const char *);
u32 WINAPI GetModuleFileNameA(HANDLE, char *, u32);

/* ---- lo básico de C ---- */
#define EOF (-1)
#define ENOENT 2
#define EBADF 9
#define ENOMEM 12
#define EACCES 13
#define EEXIST 17
#define EINVAL 22
#define EMFILE 24
#define ENOSPC 28
#define ERANGE 34
#define ECHILD_ 10

static int errno_v;
int *_errno(void) { return &errno_v; }
unsigned long *__doserrno(void) { static unsigned long d; return &d; }

static void error_win(void) {
  u32 e = GetLastError();
  errno_v = e == 2 || e == 3 || e == 18 ? ENOENT : e == 5 ? EACCES : e == 80 || e == 183 ? EEXIST :
            e == 4 ? EMFILE : e == 6 ? EBADF : e == 8 ? ENOMEM : e == 112 ? ENOSPC : EINVAL;
}

/* ---- la memoria (el montón de KERNEL32) ---- */
static HANDLE monton;
void *malloc(size_t n) { void *p = HeapAlloc(monton, 0, n ? n : 1); if (!p) errno_v = ENOMEM; return p; }
void free(void *p) { if (p) HeapFree(monton, 0, p); }
void *calloc(size_t n, size_t t) {
  u64 x = (u64)n * t;
  if (x >> 32) { errno_v = ENOMEM; return NULL; }
  return HeapAlloc(monton, 8, (u32)x ? (u32)x : 1);   /* (a cero) */
}
void *realloc(void *p, size_t n) {
  if (!p) return malloc(n);
  if (!n) { free(p); return NULL; }
  void *q = HeapReAlloc(monton, 0, p, n);
  if (!q) errno_v = ENOMEM;
  return q;
}
size_t _msize(void *p) { return HeapSize(monton, 0, p); }
void *_expand(void *p, size_t n) { return HeapReAlloc(monton, 0x10, p, n); }

/* ---- memoria y textos ---- */
void *memset(void *d, int c, size_t n) { u8 *p = d; while (n--) *p++ = (u8)c; return d; }
void *memcpy(void *d, const void *s, size_t n) { u8 *p = d; const u8 *q = s; while (n--) *p++ = *q++; return d; }
void *memmove(void *d, const void *s, size_t n) {
  u8 *p = d; const u8 *q = s;
  if (p < q) while (n--) *p++ = *q++;
  else { p += n; q += n; while (n--) *--p = *--q; }
  return d;
}
int memcmp(const void *a, const void *b, size_t n) {
  const u8 *p = a, *q = b;
  for (; n; n--, p++, q++) if (*p != *q) return *p - *q;
  return 0;
}
void *memchr(const void *s, int c, size_t n) {
  const u8 *p = s;
  for (; n; n--, p++) if (*p == (u8)c) return (void *)p;
  return NULL;
}
size_t strlen(const char *s) { size_t n = 0; while (s[n]) n++; return n; }
size_t wcslen(const wchar_t *s) { size_t n = 0; while (s[n]) n++; return n; }
char *strcpy(char *d, const char *s) { char *r = d; while ((*d++ = *s++)); return r; }
char *strncpy(char *d, const char *s, size_t n) {
  char *r = d;
  for (; n && *s; n--) *d++ = *s++;
  for (; n; n--) *d++ = 0;
  return r;
}
char *strcat(char *d, const char *s) { strcpy(d + strlen(d), s); return d; }
char *strncat(char *d, const char *s, size_t n) {
  char *p = d + strlen(d);
  for (; n && *s; n--) *p++ = *s++;
  *p = 0;
  return d;
}
int strcmp(const char *a, const char *b) {
  for (; *a && *a == *b; a++, b++);
  return (u8)*a - (u8)*b;
}
int strncmp(const char *a, const char *b, size_t n) {
  for (; n; n--, a++, b++) { if (*a != *b) return (u8)*a - (u8)*b; if (!*a) return 0; }
  return 0;
}
int tolower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
int toupper(int c) { return c >= 'a' && c <= 'z' ? c - 32 : c; }
int _stricmp(const char *a, const char *b) {
  int x, y;
  do { x = tolower((u8)*a++); y = tolower((u8)*b++); } while (x && x == y);
  return x - y;
}
int _strnicmp(const char *a, const char *b, size_t n) {
  int x = 0, y = 0;
  for (; n; n--) { x = tolower((u8)*a++); y = tolower((u8)*b++); if (!x || x != y) break; }
  return n ? x - y : 0;
}
int _strcmpi(const char *a, const char *b) { return _stricmp(a, b); }
int _memicmp(const void *a, const void *b, size_t n) {
  const u8 *p = a, *q = b;
  for (; n; n--, p++, q++) { int x = tolower(*p), y = tolower(*q); if (x != y) return x - y; }
  return 0;
}
char *strchr(const char *s, int c) {
  for (;; s++) { if (*s == (char)c) return (char *)s; if (!*s) return NULL; }
}
char *strrchr(const char *s, int c) {
  const char *r = NULL;
  for (;; s++) { if (*s == (char)c) r = s; if (!*s) return (char *)r; }
}
size_t strspn(const char *s, const char *a) { size_t n = 0; while (s[n] && strchr(a, s[n])) n++; return n; }
size_t strcspn(const char *s, const char *a) { size_t n = 0; while (s[n] && !strchr(a, s[n])) n++; return n; }
char *strpbrk(const char *s, const char *a) { s += strcspn(s, a); return *s ? (char *)s : NULL; }
char *strstr(const char *s, const char *a) {
  size_t n = strlen(a);
  if (!n) return (char *)s;
  for (; *s; s++) if (*s == *a && !strncmp(s, a, n)) return (char *)s;
  return NULL;
}
char *_strdup(const char *s) { char *d = malloc(strlen(s) + 1); if (d) strcpy(d, s); return d; }
char *_strlwr(char *s) { for (char *p = s; *p; p++) *p = tolower((u8)*p); return s; }
char *_strupr(char *s) { for (char *p = s; *p; p++) *p = toupper((u8)*p); return s; }
static char *tok_sig;
char *strtok(char *s, const char *d) {
  if (!s) s = tok_sig;
  if (!s) return NULL;
  s += strspn(s, d);
  if (!*s) { tok_sig = NULL; return NULL; }
  char *f = s + strcspn(s, d);
  if (*f) { *f = 0; tok_sig = f + 1; } else tok_sig = NULL;
  return s;
}

/* ---- ctype (la de «C», ASCII) ---- */
int isdigit(int c) { return c >= '0' && c <= '9'; }
int isupper(int c) { return c >= 'A' && c <= 'Z'; }
int islower(int c) { return c >= 'a' && c <= 'z'; }
int isalpha(int c) { return isupper(c) || islower(c); }
int isalnum(int c) { return isalpha(c) || isdigit(c); }
int isxdigit(int c) { return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
int isspace(int c) { return c == ' ' || (c >= 9 && c <= 13); }
int iscntrl(int c) { return (c >= 0 && c < 32) || c == 127; }
int isprint(int c) { return c >= 32 && c < 127; }
int isgraph(int c) { return c > 32 && c < 127; }
int ispunct(int c) { return isgraph(c) && !isalnum(c); }
int __isascii(int c) { return (unsigned)c < 128; }
int _isctype(int c, int m) {
  int r = 0;
  if (isupper(c)) r |= 1; if (islower(c)) r |= 2; if (isdigit(c)) r |= 4; if (isspace(c)) r |= 8;
  if (ispunct(c)) r |= 16; if (iscntrl(c)) r |= 32; if (c == ' ') r |= 64; if (isxdigit(c)) r |= 128;
  if (isalpha(c)) r |= 0x100;
  return r & m;
}
/* la tabla de _pctype (que miran las macros de ctype.h de Microsoft) */
static u16 tabla_ctype[257];
u16 *_pctype;
u16 **__p__pctype(void) { return &_pctype; }
int __mb_cur_max = 1;
int *__p___mb_cur_max(void) { return &__mb_cur_max; }
int ___mb_cur_max_func(void) { return 1; }

/* ---- números ---- */
static u64 divmod64(u64 n, u32 d, u32 *resto) {
  u32 hi = (u32)(n >> 32), lo = (u32)n, qh = hi / d, r = hi % d, ql;
  __asm__("divl %4" : "=a"(ql), "=d"(r) : "a"(lo), "d"(r), "rm"(d));
  if (resto) *resto = r;
  return ((u64)qh << 32) | ql;
}
static int valor_cifra(int c) {
  if (isdigit(c)) return c - '0';
  if (c >= 'a' && c <= 'z') return c - 'a' + 10;
  if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
  return 99;
}
static u64 lee_entero(const char *s, char **fin, int base, int *neg, int *desb, u64 max) {
  const char *p = s, *ini;
  u64 v = 0;
  *neg = 0; *desb = 0;
  while (isspace((u8)*p)) p++;
  if (*p == '-') { *neg = 1; p++; } else if (*p == '+') p++;
  if ((base == 0 || base == 16) && p[0] == '0' && (p[1] == 'x' || p[1] == 'X') && isxdigit((u8)p[2])) { p += 2; base = 16; }
  else if (base == 0 && p[0] == '0') base = 8;
  else if (base == 0) base = 10;
  ini = p;
  for (;; p++) {
    int c = valor_cifra((u8)*p);
    if (c >= base) break;
    u64 n = v * base + c;
    if (v > divmod64(max - c, base, NULL)) *desb = 1;
    v = n;
  }
  if (p == ini) { if (fin) *fin = (char *)s; return 0; }
  if (fin) *fin = (char *)p;
  return v;
}
long strtol(const char *s, char **fin, int base) {
  int neg, d;
  u64 v = lee_entero(s, fin, base, &neg, &d, 0xFFFFFFFFull);
  if (d || v > (neg ? 0x80000000ull : 0x7FFFFFFFull)) { errno_v = ERANGE; return neg ? (long)0x80000000 : 0x7FFFFFFF; }
  return neg ? -(long)v : (long)v;
}
unsigned long strtoul(const char *s, char **fin, int base) {
  int neg, d;
  u64 v = lee_entero(s, fin, base, &neg, &d, 0xFFFFFFFFull);
  if (d || v > 0xFFFFFFFFull) { errno_v = ERANGE; return 0xFFFFFFFF; }
  return neg ? -(unsigned long)v : (unsigned long)v;
}
s64 _strtoi64(const char *s, char **fin, int base) {
  int neg, d;
  u64 v = lee_entero(s, fin, base, &neg, &d, ~0ull);
  if (d || v > (neg ? 0x8000000000000000ull : 0x7FFFFFFFFFFFFFFFull)) { errno_v = ERANGE; return neg ? (s64)0x8000000000000000ull : 0x7FFFFFFFFFFFFFFFll; }
  return neg ? -(s64)v : (s64)v;
}
u64 _strtoui64(const char *s, char **fin, int base) {
  int neg, d;
  u64 v = lee_entero(s, fin, base, &neg, &d, ~0ull);
  if (d) { errno_v = ERANGE; return ~0ull; }
  return neg ? -v : v;
}
int atoi(const char *s) { return (int)strtol(s, NULL, 10); }
long atol(const char *s) { return strtol(s, NULL, 10); }
s64 _atoi64(const char *s) { return _strtoi64(s, NULL, 10); }
int abs(int x) { return x < 0 ? -x : x; }
long labs(long x) { return x < 0 ? -x : x; }
typedef struct { int quot, rem; } div_t;
div_t div(int a, int b) { div_t r = { a / b, a % b }; return r; }
static u32 semilla = 1;
void srand(unsigned s) { semilla = s; }
int rand(void) { semilla = semilla * 214013 + 2531011; return (semilla >> 16) & 0x7FFF; }
static char *pon_u64(u64 v, char *fin, int base, int may) {
  const char *c = may ? "0123456789ABCDEF" : "0123456789abcdef";
  *fin = 0;
  do { u32 r; v = divmod64(v, base, &r); *--fin = c[r]; } while (v);
  return fin;
}
char *_itoa(int v, char *b, int base) {
  char t[40], *p;
  if (v < 0 && base == 10) { p = pon_u64(-(s64)v, t + 39, 10, 0); *--p = '-'; }
  else p = pon_u64((u32)v, t + 39, base, 0);
  return strcpy(b, p);
}
char *_ltoa(long v, char *b, int base) { return _itoa(v, b, base); }
char *_ultoa(unsigned long v, char *b, int base) { char t[40]; return strcpy(b, pon_u64(v, t + 39, base, 0)); }

/* ---- coma flotante (x87) ---- */
static s64 d_a_s64(double x) {     /* truncando */
  s64 r; u16 cw, cw2;
  __asm__ volatile("fnstcw %0" : "=m"(cw));
  cw2 = cw | 0x0C00;
  __asm__ volatile("fldcw %0" : : "m"(cw2));
  __asm__ volatile("fistpll %0" : "=m"(r) : "t"(x) : "st");
  __asm__ volatile("fldcw %0" : : "m"(cw));
  return r;
}
double floor(double x) { double t = (double)d_a_s64(x); return t > x ? t - 1 : t; }
double ceil(double x) { double t = (double)d_a_s64(x); return t < x ? t + 1 : t; }
double fabs(double x) { return x < 0 ? -x : x; }
double sqrt(double x) { double r; __asm__("fsqrt" : "=t"(r) : "0"(x)); return r; }
double sin(double x) { double r; __asm__("fsin" : "=t"(r) : "0"(x)); return r; }
double cos(double x) { double r; __asm__("fcos" : "=t"(r) : "0"(x)); return r; }
double tan(double x) { double r; __asm__("fptan; fstp %%st(0)" : "=t"(r) : "0"(x)); return r; }
double atan(double x) { double r; __asm__("fld1; fpatan" : "=t"(r) : "0"(x)); return r; }
double atan2(double y, double x) { double r; __asm__("fpatan" : "=t"(r) : "0"(x), "u"(y) : "st(1)"); return r; }
double log(double x) { double r; __asm__("fldln2; fxch; fyl2x" : "=t"(r) : "0"(x)); return r; }
double log10(double x) { double r; __asm__("fldlg2; fxch; fyl2x" : "=t"(r) : "0"(x)); return r; }
double exp(double x) {
  double r;
  __asm__("fldl2e; fmulp; fld %%st(0); frndint; fsub %%st, %%st(1); fxch; f2xm1; fld1; faddp; fscale; fstp %%st(1)" : "=t"(r) : "0"(x));
  return r;
}
double pow(double x, double y) {
  if (y == 0) return 1;
  if (x == 0) return 0;
  if (y == (double)d_a_s64(y) && fabs(y) < 1e9) {    /* potencia entera: exacta */
    s64 n = d_a_s64(y); double r = 1, b = x;
    int neg = n < 0; if (neg) n = -n;
    while (n) { if (n & 1) r *= b; b *= b; n >>= 1; }
    return neg ? 1 / r : r;
  }
  if (x < 0) { errno_v = EINVAL; return 0; }
  return exp(y * log(x));
}
double fmod(double x, double y) { double r; __asm__("1: fprem; fnstsw %%ax; sahf; jp 1b" : "=t"(r) : "0"(x), "u"(y) : "ax", "cc"); return r; }
double ldexp(double x, int e) { while (e > 0) { x *= 2; e--; } while (e < 0) { x /= 2; e++; } return x; }
double modf(double x, double *ip) { double t = (double)d_a_s64(x); *ip = t; return x - t; }
static double pot10(int e) {
  double r = 1, b = 10; int neg = e < 0;
  if (neg) e = -e;
  while (e) { if (e & 1) r *= b; b *= b; e >>= 1; }
  return neg ? 1 / r : r;
}
double strtod(const char *s, char **fin) {
  const char *p = s;
  double v = 0; int neg = 0, e = 0, hay = 0;
  while (isspace((u8)*p)) p++;
  if (*p == '-') { neg = 1; p++; } else if (*p == '+') p++;
  while (isdigit((u8)*p)) { v = v * 10 + (*p++ - '0'); hay = 1; }
  if (*p == '.') { p++; while (isdigit((u8)*p)) { v = v * 10 + (*p++ - '0'); e--; hay = 1; } }
  if (!hay) { if (fin) *fin = (char *)s; return 0; }
  if (*p == 'e' || *p == 'E') {
    const char *q = p + 1; int en = 0, x = 0;
    if (*q == '-') { en = 1; q++; } else if (*q == '+') q++;
    if (isdigit((u8)*q)) { while (isdigit((u8)*q)) x = x * 10 + (*q++ - '0'); e += en ? -x : x; p = q; }
  }
  if (fin) *fin = (char *)p;
  v *= pot10(e);
  return neg ? -v : v;
}
double atof(const char *s) { return strtod(s, NULL); }

/* ---- printf: el motor, que escribe con una función ---- */
typedef struct { void (*pon)(void *, const char *, int); void *dato; int n; } SALIDA;
static void sal_n(SALIDA *o, const char *s, int n) { if (n > 0) { o->pon(o->dato, s, n); o->n += n; } }
static void sal_rep(SALIDA *o, char c, int n) { char b[32]; memset(b, c, 32); while (n > 0) { sal_n(o, b, n > 32 ? 32 : n); n -= 32; } }
/* un número con coma: en b (con el signo aparte), estilo 'f', 'e' o 'g' */
static int fmt_coma(double v, int prec, int estilo, int alt, char *b) {
  char *p = b;
  if (v != v) { strcpy(b, "nan"); return 3; }
  if (v > 1.7976931348623157e308) { strcpy(b, "inf"); return 3; }
  if (prec < 0) prec = 6;
  if (prec > 40) prec = 40;
  int ex = 0;
  if (estilo != 'f') {                      /* el exponente decimal */
    double t = v;
    if (t != 0) {
      while (t >= 10) { t /= 10; ex++; }
      while (t < 1) { t *= 10; ex--; }
    }
    if (estilo == 'g') {
      int P = prec ? prec : 1;
      /* redondeo a P cifras, que puede subir el exponente */
      double r = t + 0.5 * pot10(-(P - 1));
      if (r >= 10) ex++;
      if (ex < -4 || ex >= P) { estilo = 'e'; prec = P - 1; }
      else { estilo = 'f'; prec = P - 1 - ex; }
      if (!alt) alt = -1;                   /* quitar los ceros del final */
    }
  }
  if (estilo == 'e') {
    double t = v;
    if (t != 0) { t *= pot10(-ex); t += 0.5 * pot10(-prec); if (t >= 10) { t /= 10; ex++; } }
    int d = (int)d_a_s64(t);
    *p++ = '0' + d; t -= d;
    if (prec || alt > 0) *p++ = '.';
    for (int i = 0; i < prec; i++) { t *= 10; d = (int)d_a_s64(t); if (d > 9) d = 9; *p++ = '0' + d; t -= d; }
    if (alt < 0) { while (p[-1] == '0') p--; if (p[-1] == '.') p--; }
    *p++ = 'e'; *p++ = ex < 0 ? '-' : '+';
    if (ex < 0) ex = -ex;
    if (ex >= 100) *p++ = '0' + ex / 100;
    *p++ = '0' + ex / 10 % 10; *p++ = '0' + ex % 10;
    *p = 0;
    return p - b;
  }
  /* 'f' */
  v += 0.5 * pot10(-prec);
  if (v >= 9.2e18) {                        /* (muy grande: sin decimales exactos) */
    int e2 = 0; while (v >= 1e18) { v /= 10; e2++; }
    p = pon_u64(d_a_s64(v), b + 60, 10, 0);
    memmove(b, p, strlen(p) + 1); p = b + strlen(b);
    while (e2--) *p++ = '0';
  } else {
    s64 ip = d_a_s64(v);
    char t[24]; char *q = pon_u64(ip, t + 23, 10, 0);
    while (*q) *p++ = *q++;
    v -= (double)ip;
  }
  if (prec || alt > 0) *p++ = '.';
  for (int i = 0; i < prec; i++) { v *= 10; int d = (int)d_a_s64(v); if (d > 9) d = 9; if (d < 0) d = 0; *p++ = '0' + d; v -= d; }
  if (alt < 0 && prec) { while (p[-1] == '0') p--; if (p[-1] == '.') p--; }
  *p = 0;
  return p - b;
}
static int motor(SALIDA *o, const char *f, va_list ap) {
  o->n = 0;
  while (*f) {
    if (*f != '%') { const char *q = f; while (*q && *q != '%') q++; sal_n(o, f, q - f); f = q; continue; }
    f++;
    int izq = 0, mas = 0, esp = 0, alt = 0, cero = 0, ancho = -1, prec = -1, lar = 0;
    for (;; f++) {
      if (*f == '-') izq = 1; else if (*f == '+') mas = 1; else if (*f == ' ') esp = 1;
      else if (*f == '#') alt = 1; else if (*f == '0') cero = 1; else break;
    }
    if (*f == '*') { ancho = va_arg(ap, int); if (ancho < 0) { izq = 1; ancho = -ancho; } f++; }
    else if (isdigit((u8)*f)) { ancho = 0; while (isdigit((u8)*f)) ancho = ancho * 10 + *f++ - '0'; }
    if (*f == '.') {
      f++; prec = 0;
      if (*f == '*') { prec = va_arg(ap, int); f++; }
      else while (isdigit((u8)*f)) prec = prec * 10 + *f++ - '0';
    }
    /* el largo: h hh l ll L I64 I32 I z j t */
    for (;;) {
      if (*f == 'h') { lar = lar == 'h' ? 'H' : 'h'; f++; }
      else if (*f == 'l') { lar = lar == 'l' ? 'q' : 'l'; f++; }
      else if (*f == 'L' || *f == 'q' || *f == 'j') { lar = 'q'; f++; }
      else if (f[0] == 'I' && f[1] == '6' && f[2] == '4') { lar = 'q'; f += 3; }
      else if (f[0] == 'I' && f[1] == '3' && f[2] == '2') { lar = 'l'; f += 3; }
      else if (*f == 'I' || *f == 'z' || *f == 't') { lar = 'l'; f++; }
      else break;
    }
    char c = *f++;
    char b[400]; const char *s = b; int n = 0; char signo = 0; const char *pre = "";
    if (!c) break;
    switch (c) {
    case 'd': case 'i': {
      s64 v = lar == 'q' ? va_arg(ap, s64) : (s64)va_arg(ap, int);
      if (lar == 'h') v = (short)v; else if (lar == 'H') v = (signed char)v;
      u64 u = v < 0 ? -(u64)v : (u64)v;
      if (v < 0) signo = '-'; else if (mas) signo = '+'; else if (esp) signo = ' ';
      s = pon_u64(u, b + 399, 10, 0);
      if (prec == 0 && u == 0) s = b + 399;
      n = strlen(s);
      goto entero;
    }
    case 'u': case 'x': case 'X': case 'o': {
      u64 u = lar == 'q' ? va_arg(ap, u64) : (u64)va_arg(ap, unsigned);
      if (lar == 'h') u = (u16)u; else if (lar == 'H') u = (u8)u;
      int base = c == 'u' ? 10 : c == 'o' ? 8 : 16;
      s = pon_u64(u, b + 399, base, c == 'X');
      if (prec == 0 && u == 0) s = b + 399;
      n = strlen(s);
      if (alt && u && base == 16) pre = c == 'X' ? "0X" : "0x";
      if (alt && base == 8 && *s != '0') pre = "0";
    entero:
      {
        int ceros = prec > n ? prec - n : 0;
        int tot = n + ceros + (signo ? 1 : 0) + strlen(pre);
        if (cero && !izq && prec < 0 && ancho > tot) { ceros += ancho - tot; tot = ancho; }
        if (!izq) sal_rep(o, ' ', ancho - tot);
        if (signo) sal_n(o, &signo, 1);
        sal_n(o, pre, strlen(pre));
        sal_rep(o, '0', ceros);
        sal_n(o, s, n);
        if (izq) sal_rep(o, ' ', ancho - tot);
      }
      continue;
    }
    case 'p':
      s = pon_u64((u32)va_arg(ap, void *), b + 399, 16, 1);
      { int k = strlen(s); while (k < 8) { *(char *)--s = '0'; k++; } }
      n = 8;
      break;
    case 'c':
      b[0] = (char)va_arg(ap, int); n = 1;
      break;
    case 'C':
      b[0] = (char)va_arg(ap, int); n = 1;
      break;
    case 's': case 'S':
      if (lar == 'l' || c == 'S') {
        const wchar_t *w = va_arg(ap, const wchar_t *);
        if (!w) { s = "(null)"; n = 6; break; }
        int k = 0;
        while (w[k] && k < 399 && (prec < 0 || k < prec)) { b[k] = w[k] > 255 ? '?' : (char)w[k]; k++; }
        n = k;
      } else {
        s = va_arg(ap, const char *);
        if (!s) s = "(null)";
        n = 0;
        while (s[n] && (prec < 0 || n < prec)) n++;
      }
      break;
    case 'n':
      *va_arg(ap, int *) = o->n;
      continue;
    case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': {
      double v = va_arg(ap, double);
      if (v < 0 || (v == 0 && 1 / v < 0)) { signo = '-'; v = -v; }
      else if (mas) signo = '+'; else if (esp) signo = ' ';
      int e = c == 'F' ? 'f' : c == 'E' ? 'e' : c == 'G' ? 'g' : c;
      n = fmt_coma(v, prec, e, alt, b);
      if (c == 'E' || c == 'G') for (int k = 0; k < n; k++) b[k] = toupper((u8)b[k]);
      int tot = n + (signo ? 1 : 0);
      int finito = isdigit((u8)b[0]);
      if (!izq && !(cero && finito)) sal_rep(o, ' ', ancho - tot);
      if (signo) sal_n(o, &signo, 1);
      if (!izq && cero && finito) sal_rep(o, '0', ancho - tot);
      sal_n(o, b, n);
      if (izq) sal_rep(o, ' ', ancho - tot);
      continue;
    }
    default:
      b[0] = c; n = 1;
      break;
    }
    if (!izq) sal_rep(o, ' ', ancho - n);
    sal_n(o, s, n);
    if (izq) sal_rep(o, ' ', ancho - n);
  }
  return o->n;
}
/* a un texto con límite */
typedef struct { char *p; size_t cabe; } TXT;
static void pon_txt(void *d, const char *s, int n) {
  TXT *t = d;
  while (n--) { if (t->cabe > 1) { *t->p++ = *s; t->cabe--; } s++; }
}
int _vsnprintf(char *b, size_t n, const char *f, va_list ap) {
  TXT t = { b, n ? n + 1 : 0 };    /* (_vsnprintf no deja sitio para el 0: se pone si cabe) */
  SALIDA o = { pon_txt, &t, 0 };
  int r = motor(&o, f, ap);
  if ((size_t)r < n) *t.p = 0;
  return (size_t)r > n ? -1 : r == (int)n ? r : r;
}
int vsnprintf(char *b, size_t n, const char *f, va_list ap) {
  TXT t = { b, n };
  SALIDA o = { pon_txt, &t, 0 };
  int r = motor(&o, f, ap);
  if (n) *t.p = 0;
  return r;
}
int vsprintf(char *b, const char *f, va_list ap) { return vsnprintf(b, 0x7FFFFFFF, f, ap); }
int sprintf(char *b, const char *f, ...) { va_list ap; va_start(ap, f); int r = vsprintf(b, f, ap); va_end(ap); return r; }
int _snprintf(char *b, size_t n, const char *f, ...) { va_list ap; va_start(ap, f); int r = _vsnprintf(b, n, f, ap); va_end(ap); return r; }
int snprintf(char *b, size_t n, const char *f, ...) { va_list ap; va_start(ap, f); int r = vsnprintf(b, n, f, ap); va_end(ap); return r; }
int _scprintf(const char *f, ...) { va_list ap; va_start(ap, f); int r = vsnprintf(NULL, 0, f, ap); va_end(ap); return r; }
int _vscprintf(const char *f, va_list ap) { return vsnprintf(NULL, 0, f, ap); }

/* =============================================================================
 *  Los archivos: descriptores (fd) sobre los handles de KERNEL32, y FILE con
 *  búfer encima. Modo texto: «\n» se escribe «\r\n»; al leer, «\r\n» es «\n»
 *  y Ctrl+Z (1Ah) es el final.
 * ============================================================================= */
#define N_FD 64
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_APPEND 8
#define O_CREAT 0x100
#define O_TRUNC 0x200
#define O_EXCL 0x400
#define O_TEXT 0x4000
#define O_BINARY 0x8000
static struct { HANDLE h; int abierto, binario, dispositivo, anadir, fin_texto; } fds[N_FD];
int _fmode = 0;
int _commode = 0;
int *__p__fmode(void) { return &_fmode; }
int *__p__commode(void) { return &_commode; }
static int fd_nuevo(HANDLE h, int binario) {
  for (int i = 3; i < N_FD; i++) if (!fds[i].abierto) {
    fds[i].h = h; fds[i].abierto = 1; fds[i].binario = binario; fds[i].anadir = 0; fds[i].fin_texto = 0;
    fds[i].dispositivo = GetFileType(h) == 2;
    return i;
  }
  errno_v = EMFILE;
  return -1;
}
static int fd_vale(int fd) { if (fd < 0 || fd >= N_FD || !fds[fd].abierto) { errno_v = EBADF; return 0; } return 1; }
int _open(const char *nombre, int modo, ...) {
  u32 acc = (modo & 3) == O_RDONLY ? 0x80000000 : (modo & 3) == O_WRONLY ? 0x40000000 : 0xC0000000;
  u32 como = 3;
  if (modo & O_CREAT) como = (modo & O_EXCL) ? 1 : (modo & O_TRUNC) ? 2 : 4;
  else if (modo & O_TRUNC) como = 5;
  HANDLE h = CreateFileA(nombre, acc, 3, NULL, como, 0x80, NULL);
  if (h == INVALID) { error_win(); return -1; }
  int bin = (modo & O_BINARY) ? 1 : (modo & O_TEXT) ? 0 : (_fmode & O_BINARY) ? 1 : 0;
  int fd = fd_nuevo(h, bin);
  if (fd < 0) { CloseHandle(h); return -1; }
  if (modo & O_APPEND) fds[fd].anadir = 1;
  return fd;
}
int _sopen(const char *n, int m, int sh, ...) { return _open(n, m); }
int _creat(const char *n, int p) { return _open(n, O_WRONLY | O_CREAT | O_TRUNC); }
int _close(int fd) {
  if (!fd_vale(fd)) return -1;
  if (fd > 2) CloseHandle(fds[fd].h);
  fds[fd].abierto = 0;
  return 0;
}
static int lee_crudo(int fd, void *b, u32 n) {
  u32 r;
  if (!ReadFile(fds[fd].h, b, n, &r, NULL)) { if (GetLastError() == 109) return 0; error_win(); return -1; }
  return r;
}
static int escribe_crudo(int fd, const void *b, u32 n) {
  u32 r;
  if (fds[fd].anadir) SetFilePointer(fds[fd].h, 0, NULL, 2);
  if (!WriteFile(fds[fd].h, b, n, &r, NULL)) { error_win(); return -1; }
  if (r < n && !fds[fd].dispositivo) { errno_v = ENOSPC; return r ? (int)r : -1; }
  return r;
}
int _read(int fd, void *buf, unsigned n) {
  if (!fd_vale(fd)) return -1;
  if (fds[fd].binario) return lee_crudo(fd, buf, n);
  /* texto: \r\n -> \n, y 1Ah es el final */
  if (fds[fd].fin_texto) return 0;
  char *b = buf; unsigned k = 0;
  while (k < n) {
    char t[512]; unsigned quiero = n - k > sizeof t ? sizeof t : n - k;
    int r = lee_crudo(fd, t, quiero);
    if (r <= 0) break;
    for (int i = 0; i < r; i++) {
      if (t[i] == 0x1A && !fds[fd].dispositivo) { fds[fd].fin_texto = 1; SetFilePointer(fds[fd].h, i - r, NULL, 1); return k; }
      if (t[i] == '\r') {
        if (i + 1 < r) { if (t[i + 1] == '\n') continue; }
        else {                                    /* el \r al final del trozo: mirar el siguiente */
          char sig; int r2 = lee_crudo(fd, &sig, 1);
          if (r2 == 1 && sig == '\n') { b[k++] = '\n'; continue; }
          if (r2 == 1) SetFilePointer(fds[fd].h, -1, NULL, 1);
        }
      }
      b[k++] = t[i];
    }
    if (fds[fd].dispositivo || (unsigned)r < quiero) break;   /* (la consola: una línea) */
  }
  return k;
}
int _write(int fd, const void *buf, unsigned n) {
  if (!fd_vale(fd)) return -1;
  if (fds[fd].binario) return escribe_crudo(fd, buf, n);
  const char *s = buf; char t[1024]; unsigned k = 0, j = 0;
  for (unsigned i = 0; i < n; i++) {
    if (s[i] == '\n') t[j++] = '\r';
    t[j++] = s[i];
    if (j >= sizeof t - 2) { if (escribe_crudo(fd, t, j) < 0) return -1; j = 0; }
    k++;
  }
  if (j && escribe_crudo(fd, t, j) < 0) return -1;
  return k;
}
long _lseek(int fd, long d, int como) {
  if (!fd_vale(fd)) return -1;
  fds[fd].fin_texto = 0;
  u32 r = SetFilePointer(fds[fd].h, d, NULL, como);
  if (r == 0xFFFFFFFF) { error_win(); return -1; }
  return r;
}
s64 _lseeki64(int fd, s64 d, int como) { return _lseek(fd, (long)d, como); }
long _tell(int fd) { return _lseek(fd, 0, 1); }
int _setmode(int fd, int modo) {
  if (!fd_vale(fd)) return -1;
  int viejo = fds[fd].binario ? O_BINARY : O_TEXT;
  fds[fd].binario = (modo & O_BINARY) != 0;
  return viejo;
}
int _isatty(int fd) { return fd_vale(fd) && fds[fd].dispositivo; }
long _get_osfhandle(int fd) { return fd_vale(fd) ? (long)fds[fd].h : -1; }
int _open_osfhandle(long h, int modo) { return fd_nuevo((HANDLE)h, (modo & O_BINARY) != 0); }
long _filelength(int fd) { return fd_vale(fd) ? (long)GetFileSize(fds[fd].h, NULL) : -1; }
s64 _filelengthi64(int fd) { return _filelength(fd); }
int _chsize(int fd, long tam) {
  if (!fd_vale(fd)) return -1;
  u32 ahora = SetFilePointer(fds[fd].h, 0, NULL, 1);
  SetFilePointer(fds[fd].h, tam, NULL, 0);
  int r = SetEndOfFile(fds[fd].h) ? 0 : -1;
  SetFilePointer(fds[fd].h, ahora, NULL, 0);
  if (r) error_win();
  return r;
}
int _commit(int fd) { return fd_vale(fd) && FlushFileBuffers(fds[fd].h) ? 0 : -1; }
int _eof(int fd) {
  if (!fd_vale(fd)) return -1;
  u32 p = SetFilePointer(fds[fd].h, 0, NULL, 1);
  return p >= GetFileSize(fds[fd].h, NULL);
}
int _dup(int fd) { if (!fd_vale(fd)) return -1; return fd_nuevo(fds[fd].h, fds[fd].binario); }

/* ---- FILE (como el de Microsoft: 32 octetos, y _iob[] para los tres de siempre) ---- */
typedef struct {
  char *ptr; int cnt; char *base; int flag; int file; int charbuf; int bufsiz; char *tmpname;
} FILE;
#define F_LEE 1
#define F_ESC 2
#define F_LEEESC 0x80
#define F_FIN 0x10
#define F_ERR 0x20
#define F_MIBUF 0x08
#define F_SINBUF 0x04
#define F_EN_USO 0
/* en uso: con lectura, escritura o las dos (las banderas de Microsoft; los
 * programas pueden tocar «flag», que es público en su FILE: nada de banderas
 * propias aquí) */
#define EN_USO(f) (((f)->flag & (F_LEE | F_ESC | F_LEEESC)) != 0)
#define N_IOB 20
FILE _iob[N_IOB];
FILE *__iob_func(void) { return _iob; }
FILE *__acrt_iob_func(unsigned i) { return &_iob[i]; }
static FILE *mas_files[N_FD];
#define BUF_TAM 4096

static int vacia(FILE *f) {      /* escribe lo que haya en el búfer */
  if ((f->flag & F_ESC) && f->base && f->ptr > f->base) {
    int n = f->ptr - f->base;
    int r = _write(f->file, f->base, n);
    f->ptr = f->base; f->cnt = 0;
    if (r != n) { f->flag |= F_ERR; return EOF; }
  }
  return 0;
}
static int pide_buf(FILE *f) {
  if (f->base || (f->flag & F_SINBUF)) return 0;
  f->base = malloc(BUF_TAM);
  if (!f->base) { f->flag |= F_SINBUF; return 0; }
  f->bufsiz = BUF_TAM; f->ptr = f->base; f->cnt = 0; f->flag |= F_MIBUF;
  return 0;
}
int fflush(FILE *f) {
  if (!f) { int r = 0; for (int i = 0; i < N_IOB; i++) if (EN_USO(&_iob[i])) r |= fflush(&_iob[i]);
            for (int i = 0; i < N_FD; i++) if (mas_files[i]) r |= fflush(mas_files[i]); return r; }
  if (f->flag & F_ESC) return vacia(f);
  if (f->flag & F_LEE) {      /* lo leído de más: volver atrás */
    if (f->cnt > 0) _lseek(f->file, -f->cnt, 1);
    f->cnt = 0; f->ptr = f->base;
  }
  return 0;
}
int _flushall(void) { fflush(NULL); return 0; }
static FILE *file_libre(void) {
  for (int i = 3; i < N_IOB; i++) if (!EN_USO(&_iob[i])) { memset(&_iob[i], 0, sizeof(FILE)); return &_iob[i]; }
  for (int i = 0; i < N_FD; i++) if (!mas_files[i]) { mas_files[i] = calloc(1, sizeof(FILE)); return mas_files[i]; }
  return NULL;
}
static int modo_de(const char *m, int *flags) {
  int o, f;
  switch (*m++) {
  case 'r': o = O_RDONLY; f = F_LEE; break;
  case 'w': o = O_WRONLY | O_CREAT | O_TRUNC; f = F_ESC; break;
  case 'a': o = O_WRONLY | O_CREAT | O_APPEND; f = F_ESC; break;
  default: errno_v = EINVAL; return -1;
  }
  for (; *m; m++) {
    if (*m == '+') { o = (o & ~3) | O_RDWR; f = F_LEEESC; }
    else if (*m == 'b') o |= O_BINARY;
    else if (*m == 't') o |= O_TEXT;
  }
  *flags = f;
  return o;
}
FILE *_fdopen(int fd, const char *m) {
  int fl; if (modo_de(m, &fl) < 0 || !fd_vale(fd)) return NULL;
  FILE *f = file_libre();
  if (!f) { errno_v = EMFILE; return NULL; }
  f->file = fd; f->flag = fl | F_EN_USO;
  return f;
}
FILE *fopen(const char *nombre, const char *m) {
  int fl, o = modo_de(m, &fl);
  if (o < 0) return NULL;
  FILE *f = file_libre();
  if (!f) { errno_v = EMFILE; return NULL; }
  int fd = _open(nombre, o);
  if (fd < 0) { f->flag = 0; return NULL; }
  f->file = fd; f->flag = fl | F_EN_USO; f->ptr = f->base = NULL; f->cnt = 0;
  return f;
}
FILE *_fsopen(const char *n, const char *m, int sh) { return fopen(n, m); }
int fclose(FILE *f) {
  if (!f || !EN_USO(f)) return EOF;
  int r = fflush(f);
  if (f->file > 2) r |= _close(f->file);
  if (f->flag & F_MIBUF) free(f->base);
  if (f->tmpname) { DeleteFileA(f->tmpname); free(f->tmpname); }
  int i;
  for (i = 0; i < N_FD; i++) if (mas_files[i] == f) { mas_files[i] = NULL; free(f); return r; }
  memset(f, 0, sizeof *f);
  return r;
}
int _fcloseall(void) { int n = 0; for (int i = 3; i < N_IOB; i++) if (EN_USO(&_iob[i])) { fclose(&_iob[i]); n++; } return n; }
FILE *freopen(const char *n, const char *m, FILE *f) {
  int fl, o = modo_de(m, &fl);
  if (o < 0) return NULL;
  fflush(f);
  if (f->file > 2) _close(f->file);
  int fd = _open(n, o);
  if (fd < 0) return NULL;
  if (f->file <= 2 && f->file >= 0) { fds[f->file] = fds[fd]; fds[fd].abierto = 0; fd = f->file; }
  f->file = fd; f->flag = fl | F_EN_USO | (f->flag & (F_MIBUF | F_SINBUF)); f->cnt = 0; f->ptr = f->base;
  return f;
}
static int escribe_f(FILE *f, const char *s, int n) {
  if (!(f->flag & (F_ESC | F_LEEESC))) { f->flag |= F_ERR; errno_v = EBADF; return 0; }
  if (f->flag & F_LEE) { fflush(f); f->flag &= ~F_LEE; }
  f->flag |= F_ESC;
  pide_buf(f);
  if (!f->base || fds[f->file].dispositivo) {   /* sin búfer (la consola): directo */
    vacia(f);
    int r = _write(f->file, s, n);
    if (r != n) { f->flag |= F_ERR; return r < 0 ? 0 : r; }
    return n;
  }
  int k = 0;
  while (k < n) {
    int cabe = f->bufsiz - (f->ptr - f->base);
    if (!cabe) { if (vacia(f)) return k; continue; }
    int t = n - k < cabe ? n - k : cabe;
    memcpy(f->ptr, s + k, t); f->ptr += t; k += t;
  }
  return k;
}
static int llena(FILE *f) {      /* -> el carácter siguiente o EOF */
  if (!(f->flag & (F_LEE | F_LEEESC))) { f->flag |= F_ERR; errno_v = EBADF; return EOF; }
  if (f->flag & F_ESC) { vacia(f); f->flag &= ~F_ESC; }
  f->flag |= F_LEE;
  if (f->file == 0) fflush(&_iob[1]);   /* lo pendiente de la salida, antes de leer la consola */
  pide_buf(f);
  if (!f->base) {
    u8 c; int r = _read(f->file, &c, 1);
    if (r <= 0) { f->flag |= r ? F_ERR : F_FIN; return EOF; }
    return c;
  }
  int r = _read(f->file, f->base, f->bufsiz);
  if (r <= 0) { f->flag |= r ? F_ERR : F_FIN; f->cnt = 0; return EOF; }
  f->ptr = f->base; f->cnt = r;
  f->cnt--; return (u8)*f->ptr++;
}
int fgetc(FILE *f) {
  if (f->charbuf) { int c = f->charbuf & 0xFF; f->charbuf = 0; return c; }
  if (f->cnt > 0 && (f->flag & F_LEE)) { f->cnt--; return (u8)*f->ptr++; }
  return llena(f);
}
int getc(FILE *f) { return fgetc(f); }
int _fgetchar(void) { return fgetc(&_iob[0]); }
int getchar(void) { return fgetc(&_iob[0]); }
int _filbuf(FILE *f) { return llena(f); }
int ungetc(int c, FILE *f) {
  if (c == EOF) return EOF;
  f->charbuf = 0x100 | (u8)c;
  f->flag &= ~F_FIN;
  return (u8)c;
}
int fputc(int c, FILE *f) { char x = (char)c; return escribe_f(f, &x, 1) == 1 ? (u8)c : EOF; }
int putc(int c, FILE *f) { return fputc(c, f); }
int _fputchar(int c) { return fputc(c, &_iob[1]); }
int putchar(int c) { return fputc(c, &_iob[1]); }
int _flsbuf(int c, FILE *f) { return fputc(c, f); }
int fputs(const char *s, FILE *f) { int n = strlen(s); return escribe_f(f, s, n) == n ? 0 : EOF; }
int puts(const char *s) { return fputs(s, &_iob[1]) || fputc('\n', &_iob[1]) == EOF ? EOF : 0; }
char *fgets(char *b, int n, FILE *f) {
  int k = 0;
  while (k < n - 1) {
    int c = fgetc(f);
    if (c == EOF) { if (!k) return NULL; break; }
    b[k++] = c;
    if (c == '\n') break;
  }
  b[k] = 0;
  return b;
}
char *gets(char *b) { char *r = fgets(b, 0x7FFFFFFF, &_iob[0]); if (r) { size_t n = strlen(b); if (n && b[n - 1] == '\n') b[n - 1] = 0; } return r; }
size_t fread(void *b, size_t t, size_t n, FILE *f) {
  u64 tot = (u64)t * n;
  if (!tot) return 0;
  u8 *p = b; u32 k = 0;
  while (k < tot) {
    if (f->charbuf) { p[k++] = (u8)f->charbuf; f->charbuf = 0; continue; }
    if (f->cnt > 0 && (f->flag & F_LEE)) {
      u32 x = tot - k < (u32)f->cnt ? tot - k : (u32)f->cnt;
      memcpy(p + k, f->ptr, x); f->ptr += x; f->cnt -= x; k += x; continue;
    }
    int c = llena(f);
    if (c == EOF) break;
    p[k++] = c;
  }
  return k / t;
}
size_t fwrite(const void *b, size_t t, size_t n, FILE *f) {
  u64 tot = (u64)t * n;
  if (!tot) return 0;
  return escribe_f(f, b, tot) / t;
}
int feof(FILE *f) { return (f->flag & F_FIN) != 0; }
int ferror(FILE *f) { return (f->flag & F_ERR) != 0; }
void clearerr(FILE *f) { f->flag &= ~(F_FIN | F_ERR); }
int _fileno(FILE *f) { return f->file; }
long ftell(FILE *f) {
  long p = _lseek(f->file, 0, 1);
  if (p < 0) return -1;
  if (f->flag & F_ESC) p += f->ptr - f->base;
  else if (f->flag & F_LEE) p -= f->cnt;
  if (f->charbuf) p--;
  return p;
}
s64 _ftelli64(FILE *f) { return ftell(f); }
int fseek(FILE *f, long d, int como) {
  if (como == 1) d = ftell(f) + d, como = 0;
  fflush(f);
  f->cnt = 0; f->ptr = f->base; f->charbuf = 0;
  f->flag &= ~F_FIN;
  if (f->flag & F_LEEESC) f->flag &= ~(F_LEE | F_ESC);
  return _lseek(f->file, d, como) < 0 ? -1 : 0;
}
int _fseeki64(FILE *f, s64 d, int como) { return fseek(f, (long)d, como); }
void rewind(FILE *f) { fseek(f, 0, 0); clearerr(f); }
int fgetpos(FILE *f, s64 *p) { long x = ftell(f); if (x < 0) return -1; *p = x; return 0; }
int fsetpos(FILE *f, const s64 *p) { return fseek(f, (long)*p, 0); }
int setvbuf(FILE *f, char *b, int modo, size_t t) {
  fflush(f);
  if (f->flag & F_MIBUF) free(f->base);
  f->flag &= ~(F_MIBUF | F_SINBUF);
  f->base = f->ptr = NULL; f->cnt = 0;
  if (modo == 4) { f->flag |= F_SINBUF; return 0; }    /* _IONBF */
  if (b && t) { f->base = f->ptr = b; f->bufsiz = t; }
  else if (t) { f->base = f->ptr = malloc(t); f->bufsiz = t; f->flag |= F_MIBUF; }
  return 0;
}
void setbuf(FILE *f, char *b) { setvbuf(f, b, b ? 0 : 4, 512); }
static void pon_file(void *d, const char *s, int n) { escribe_f(d, s, n); }
int vfprintf(FILE *f, const char *fm, va_list ap) {
  char tmp[512];
  if (f->file >= 0 && f->file < N_FD && fds[f->file].dispositivo) {
    /* a la consola: por trozos, de una vez */
    TXT t = { tmp, sizeof tmp };
    va_list c; va_copy(c, ap);
    SALIDA o = { pon_txt, &t, 0 };
    int r = motor(&o, fm, c);
    va_end(c);
    if (r < (int)sizeof tmp) { escribe_f(f, tmp, r); return r; }
  }
  SALIDA o = { pon_file, f, 0 };
  return motor(&o, fm, ap);
}
int fprintf(FILE *f, const char *fm, ...) { va_list ap; va_start(ap, fm); int r = vfprintf(f, fm, ap); va_end(ap); return r; }
int vprintf(const char *fm, va_list ap) { return vfprintf(&_iob[1], fm, ap); }
int printf(const char *fm, ...) { va_list ap; va_start(ap, fm); int r = vfprintf(&_iob[1], fm, ap); va_end(ap); return r; }

/* ---- scanf (lo justo: sscanf con %d %i %u %x %o %s %c %[ %n %f y anchos) ---- */
static int motor_scan(const char *s, const char *f, va_list ap) {
  const char *ini = s; int n = 0;
  while (*f) {
    if (isspace((u8)*f)) { while (isspace((u8)*s)) s++; f++; continue; }
    if (*f != '%') { if (*s != *f) break; s++; f++; continue; }
    f++;
    if (*f == '%') { if (*s != '%') break; s++; f++; continue; }
    int salta = 0, ancho = 0, lar = 0;
    if (*f == '*') { salta = 1; f++; }
    while (isdigit((u8)*f)) ancho = ancho * 10 + *f++ - '0';
    while (*f == 'h' || *f == 'l' || *f == 'L') { lar = *f == 'l' && lar == 'l' ? 'q' : *f; f++; }
    char c = *f++;
    if (c != 'c' && c != '[' && c != 'n') while (isspace((u8)*s)) s++;
    if (!*s && c != 'n') break;
    if (c == 'n') { if (!salta) *va_arg(ap, int *) = s - ini; continue; }
    if (c == 'd' || c == 'i' || c == 'u' || c == 'x' || c == 'o') {
      char t[40]; int k = 0, lim = ancho ? ancho : 39;
      while (s[k] && k < lim && (isalnum((u8)s[k]) || ((s[k] == '-' || s[k] == '+') && !k))) { t[k] = s[k]; k++; }
      t[k] = 0;
      char *fin; int base = c == 'd' || c == 'u' ? 10 : c == 'x' ? 16 : c == 'o' ? 8 : 0;
      s64 v = _strtoi64(t, &fin, base);
      if (fin == t) break;
      s += fin - t;
      if (!salta) { if (lar == 'q') *va_arg(ap, s64 *) = v; else if (lar == 'h') *va_arg(ap, short *) = v; else *va_arg(ap, int *) = v; n++; }
    } else if (c == 'f' || c == 'e' || c == 'g') {
      char *fin; double v = strtod(s, &fin);
      if (fin == s) break;
      s = fin;
      if (!salta) { if (lar == 'l' || lar == 'L') *va_arg(ap, double *) = v; else *va_arg(ap, float *) = v; n++; }
    } else if (c == 's') {
      char *d = salta ? NULL : va_arg(ap, char *); int k = 0;
      while (*s && !isspace((u8)*s) && (!ancho || k < ancho)) { if (d) d[k] = *s; s++; k++; }
      if (d) { d[k] = 0; n++; }
    } else if (c == 'c') {
      char *d = salta ? NULL : va_arg(ap, char *); int k = 0, lim = ancho ? ancho : 1;
      while (*s && k < lim) { if (d) d[k] = *s; s++; k++; }
      if (d) n++;
    } else if (c == '[') {
      int no = 0; char conj[256]; memset(conj, 0, 256);
      if (*f == '^') { no = 1; f++; }
      if (*f == ']') { conj[']'] = 1; f++; }
      while (*f && *f != ']') { if (f[1] == '-' && f[2] && f[2] != ']') { for (int x = (u8)f[0]; x <= (u8)f[2]; x++) conj[x] = 1; f += 3; } else conj[(u8)*f++] = 1; }
      if (*f) f++;
      char *d = salta ? NULL : va_arg(ap, char *); int k = 0;
      while (*s && (conj[(u8)*s] ^ no) && (!ancho || k < ancho)) { if (d) d[k] = *s; s++; k++; }
      if (!k) break;
      if (d) { d[k] = 0; n++; }
    } else break;
  }
  return n ? n : (*s ? 0 : EOF);
}
int vsscanf(const char *s, const char *f, va_list ap) { return motor_scan(s, f, ap); }
int sscanf(const char *s, const char *f, ...) { va_list ap; va_start(ap, f); int r = motor_scan(s, f, ap); va_end(ap); return r; }
int fscanf(FILE *fi, const char *f, ...) {
  char b[512];
  if (!fgets(b, sizeof b, fi)) return EOF;
  va_list ap; va_start(ap, f); int r = motor_scan(b, f, ap); va_end(ap); return r;
}
int scanf(const char *f, ...) {
  char b[512];
  fflush(&_iob[1]);
  if (!fgets(b, sizeof b, &_iob[0])) return EOF;
  va_list ap; va_start(ap, f); int r = motor_scan(b, f, ap); va_end(ap); return r;
}

/* ---- errores ---- */
static const char *mensajes[] = {
  "Sin error", "Operacion no permitida", "No existe el archivo o el directorio", "No existe el proceso",
  "Llamada interrumpida", "Error de entrada o salida", "No existe el dispositivo", "Demasiados argumentos",
  "Formato de ejecutable no valido", "Descriptor de archivo no valido", "No hay procesos hijos",
  "Recurso no disponible", "No hay memoria bastante", "Permiso denegado", "Direccion no valida", "Error desconocido",
  "Dispositivo ocupado", "El archivo ya existe", "Enlace entre dispositivos", "No existe el dispositivo",
  "No es un directorio", "Es un directorio", "Argumento no valido", "Demasiados archivos abiertos en el sistema",
  "Demasiados archivos abiertos", "No es una consola", "Error desconocido", "Archivo demasiado grande",
  "No queda sitio en el disco", "Busqueda no valida", "Sistema de archivos de solo lectura", "Demasiados enlaces",
  "Tuberia rota", "Fuera del dominio de la funcion", "Resultado fuera de rango",
};
char *strerror(int e) { return (char *)(e >= 0 && e < (int)(sizeof mensajes / sizeof *mensajes) ? mensajes[e] : "Error desconocido"); }
char *_strerror(const char *s) { static char b[200]; if (s && *s) snprintf(b, sizeof b, "%s: %s\n", s, strerror(errno_v)); else snprintf(b, sizeof b, "%s\n", strerror(errno_v)); return b; }
void perror(const char *s) {
  if (s && *s) fprintf(&_iob[2], "%s: %s\n", s, strerror(errno_v));
  else fprintf(&_iob[2], "%s\n", strerror(errno_v));
}
int *__sys_nerr(void) { static int n = sizeof mensajes / sizeof *mensajes; return &n; }

/* ---- el final del programa ---- */
typedef void (*FUNC)(void);
static FUNC al_salir[64];
static int n_al_salir;
FUNC _onexit(FUNC f) { if (n_al_salir >= 64) return NULL; al_salir[n_al_salir++] = f; return f; }
int atexit(FUNC f) { return _onexit(f) ? 0 : -1; }
FUNC __dllonexit(FUNC f, FUNC **ini, FUNC **fin) { return _onexit(f); }
static int saliendo;
static void termina(int correr) {
  if (saliendo++) return;
  if (correr) while (n_al_salir) al_salir[--n_al_salir]();
  fflush(NULL);
}
/* (si una función de atexit vuelve a llamar a exit, termina no repite las
 * funciones; pero los búferes se vacían siempre antes de acabar) */
void exit(int c) { termina(1); fflush(NULL); ExitProcess(c); }
void _exit(int c) { fflush(NULL); ExitProcess(c); }
void _cexit(void) { termina(1); saliendo = 0; }
void _c_exit(void) { }
void abort(void) {
  fputs("\r\nEl programa ha terminado de forma anormal.\r\n", &_iob[2]);
  _exit(3);
}
void _amsg_exit(int n) {
  fprintf(&_iob[2], "\r\nError en tiempo de ejecucion R60%02d\r\n", n);
  _exit(255);
}
void _assert(const char *e, const char *a, unsigned l) {
  fprintf(&_iob[2], "No se cumple: %s, archivo %s, linea %u\n", e, a, l);
  abort();
}
void _wassert(const wchar_t *e, const wchar_t *a, unsigned l) {
  fprintf(&_iob[2], "No se cumple: %S, archivo %S, linea %u\n", e, a, l);
  abort();
}
typedef void (*SENAL)(int);
SENAL signal(int s, SENAL f) { return 0; }
int raise(int s) { if (s == 22) abort(); return 0; }   /* SIGABRT */
void __setusermatherr(void *f) { }
static int tipo_app;
void __set_app_type(int t) { tipo_app = t; }
int _XcptFilter(unsigned long c, void *p) { return 0; }
void _initterm(FUNC *a, FUNC *b) { for (; a < b; a++) if (*a) (*a)(); }
int _initterm_e(int (**a)(void), int (**b)(void)) { for (; a < b; a++) if (*a) { int r = (*a)(); if (r) return r; } return 0; }
void _lock(int n) { }
void _unlock(int n) { }
void _lock_file(FILE *f) { }
void _unlock_file(FILE *f) { }

/* ---- argumentos y entorno ---- */
int __argc;
char **__argv;
wchar_t **__wargv;
char **_environ;
char **__initenv;
char *_acmdln;
char *_pgmptr;
unsigned _osver = 1998, _winver = 0x040A, _winmajor = 4, _winminor = 10;
int *__p___argc(void) { return &__argc; }
char ***__p___argv(void) { return &__argv; }
char ***__p__environ(void) { return &_environ; }
char **__p__acmdln(void) { return &_acmdln; }
char **__p__pgmptr(void) { return &_pgmptr; }
unsigned *__p__osver(void) { return &_osver; }
unsigned *__p__winver(void) { return &_winver; }
/* la línea de órdenes a argv, con las reglas de Microsoft (comillas y \) */
static int parte_linea(const char *s, char **argv, char *buf) {
  int n = 0;
  for (;;) {
    while (*s == ' ' || *s == '\t') s++;
    if (!*s) break;
    if (argv) argv[n] = buf;
    n++;
    int comillas = 0;
    for (;;) {
      int barras = 0;
      while (*s == '\\') { barras++; s++; }
      if (*s == '"') {
        for (int i = 0; i < barras / 2; i++) if (buf) *buf++ = '\\';
        if (barras & 1) { if (buf) *buf++ = '"'; s++; continue; }
        if (comillas && s[1] == '"') { if (buf) *buf++ = '"'; s += 2; continue; }
        comillas = !comillas; s++; continue;
      }
      for (int i = 0; i < barras; i++) if (buf) *buf++ = '\\';
      if (!*s || (!comillas && (*s == ' ' || *s == '\t'))) break;
      if (buf) *buf++ = *s;
      s++;
    }
    if (buf) *buf++ = 0;
  }
  if (argv) argv[n] = NULL;
  return n;
}
static void prepara_args(void) {
  if (__argv) return;
  _acmdln = GetCommandLineA();
  int n = parte_linea(_acmdln, NULL, NULL);
  __argv = malloc((n + 1) * sizeof(char *));
  char *buf = malloc(strlen(_acmdln) + n + 1);
  __argc = parte_linea(_acmdln, __argv, buf);
  static char modulo[260];
  GetModuleFileNameA(NULL, modulo, sizeof modulo);
  _pgmptr = modulo;
  /* el entorno */
  char *e = GetEnvironmentStrings();
  int k = 0;
  for (char *p = e; *p; p += strlen(p) + 1) k++;
  _environ = malloc((k + 1) * sizeof(char *));
  k = 0;
  for (char *p = e; *p; p += strlen(p) + 1) _environ[k++] = p;
  _environ[k] = NULL;
  __initenv = _environ;
}
int __getmainargs(int *argc, char ***argv, char ***envp, int expandir, int *info) {
  prepara_args();
  *argc = __argc; *argv = __argv; *envp = _environ;
  return 0;
}
char *getenv(const char *n) {
  prepara_args();
  size_t l = strlen(n);
  for (char **e = _environ; *e; e++) if (!_strnicmp(*e, n, l) && (*e)[l] == '=') return *e + l + 1;
  return NULL;
}
int _putenv(const char *s) { return 0; }

/* ---- archivos por nombre ---- */
int remove(const char *n) { if (DeleteFileA(n)) return 0; error_win(); return -1; }
int _unlink(const char *n) { return remove(n); }
int rename(const char *a, const char *b) { if (MoveFileA(a, b)) return 0; error_win(); return -1; }
int _access(const char *n, int m) {
  u32 a = GetFileAttributesA(n);
  if (a == 0xFFFFFFFF) { error_win(); return -1; }
  if ((m & 2) && (a & 1)) { errno_v = EACCES; return -1; }
  return 0;
}
int _mkdir(const char *n) { if (CreateDirectoryA(n, NULL)) return 0; error_win(); return -1; }
int _rmdir(const char *n) { if (RemoveDirectoryA(n)) return 0; error_win(); return -1; }
int _chdir(const char *n) { if (SetCurrentDirectoryA(n)) return 0; error_win(); return -1; }
char *_getcwd(char *b, int n) {
  char t[260];
  u32 l = GetCurrentDirectoryA(sizeof t, t);
  if (!b) { b = malloc(n > (int)l + 1 ? n : l + 1); if (!b) return NULL; n = l + 1; }
  if ((int)l >= n) { errno_v = ERANGE; return NULL; }
  return strcpy(b, t);
}
char *_fullpath(char *b, const char *n, size_t max) {
  char t[260];
  u32 l = GetFullPathNameA(n, sizeof t, t, NULL);
  if (!l || l >= sizeof t) { errno_v = ENOENT; return NULL; }
  if (!b) { b = malloc(l + 1); if (!b) return NULL; }
  else if (l >= max) { errno_v = ERANGE; return NULL; }
  return strcpy(b, t);
}
static char *ancho_a_nombre(const wchar_t *w, char *b) {   /* (los nombres: 8 bits) */
  int i = 0;
  for (; w[i] && i < 259; i++) b[i] = w[i] > 255 ? '_' : (char)w[i];
  b[i] = 0;
  return b;
}

/* ---- stat (struct _stat y _stati64 de Microsoft, con time_t de 32 bits) ---- */
struct stat_ms { u32 dev; u16 ino, mode; short nlink, uid, gid; u32 rdev; long size; long atime, mtime, ctime; };
struct stati64_ms { u32 dev; u16 ino, mode; short nlink, uid, gid; u32 rdev; s64 size __attribute__((aligned(8))); long atime, mtime, ctime; } __attribute__((aligned(8)));
static long t_de_ft(const FT *f) {
  u64 t = ((u64)f->hi << 32) | f->lo;
  if (t < 116444736000000000ull) return 0;
  return (long)divmod64(t - 116444736000000000ull, 10000000, NULL);
}
static int stat_comun(const char *n, u16 *modo, s64 *tam, long *mt, u32 *unidad) {
  FINDA f;
  size_t l = strlen(n);
  int raiz = (l == 3 && n[1] == ':' && (n[2] == '\\' || n[2] == '/')) || (l == 1 && (n[0] == '\\' || n[0] == '/')) || (l == 2 && n[1] == ':');
  *unidad = n[1] == ':' ? (u32)(toupper((u8)n[0]) - 'A') : 0;
  if (raiz || strpbrk(n, "*?")) {
    if (raiz && GetFileAttributesA(n) != 0xFFFFFFFF) { *modo = 0x4000 | 0x1FF; *tam = 0; *mt = 0; return 0; }
    errno_v = ENOENT; return -1;
  }
  char t[262]; strcpy(t, n);
  while (l > 1 && (t[l - 1] == '\\' || t[l - 1] == '/')) t[--l] = 0;
  HANDLE h = FindFirstFileA(t, &f);
  if (h == INVALID) { errno_v = ENOENT; return -1; }
  FindClose(h);
  *modo = (f.atrib & 0x10) ? 0x4000 | 0x1FF : 0x8000 | ((f.atrib & 1) ? 0x16D : 0x1B6);
  const char *x = strrchr(t, '.');
  if (x && !(f.atrib & 0x10) && (!_stricmp(x, ".exe") || !_stricmp(x, ".com") || !_stricmp(x, ".bat"))) *modo |= 0x49;
  *tam = f.tam_lo;
  *mt = t_de_ft(&f.e);
  return 0;
}
int _stati64(const char *n, struct stati64_ms *s) {
  u16 m; s64 t; long mt; u32 u;
  if (stat_comun(n, &m, &t, &mt, &u)) return -1;
  memset(s, 0, sizeof *s);
  s->mode = m; s->size = t; s->atime = s->mtime = s->ctime = mt; s->dev = s->rdev = u; s->nlink = 1;
  return 0;
}
int _stat(const char *n, struct stat_ms *s) {
  u16 m; s64 t; long mt; u32 u;
  if (stat_comun(n, &m, &t, &mt, &u)) return -1;
  memset(s, 0, sizeof *s);
  s->mode = m; s->size = (long)t; s->atime = s->mtime = s->ctime = mt; s->dev = s->rdev = u; s->nlink = 1;
  return 0;
}
int _stat32(const char *n, struct stat_ms *s) { return _stat(n, s); }
int _wstati64(const wchar_t *w, struct stati64_ms *s) { char b[262]; return _stati64(ancho_a_nombre(w, b), s); }
int _wstat(const wchar_t *w, struct stat_ms *s) { char b[262]; return _stat(ancho_a_nombre(w, b), s); }
static int fstat_comun(int fd, u16 *m, s64 *t) {
  if (!fd_vale(fd)) return -1;
  if (fds[fd].dispositivo) { *m = 0x2000 | 0x1B6; *t = 0; }
  else { *m = 0x8000 | 0x1B6; *t = GetFileSize(fds[fd].h, NULL); }
  return 0;
}
int _fstati64(int fd, struct stati64_ms *s) {
  u16 m; s64 t;
  if (fstat_comun(fd, &m, &t)) return -1;
  memset(s, 0, sizeof *s); s->mode = m; s->size = t; s->nlink = 1;
  return 0;
}
int _fstat(int fd, struct stat_ms *s) {
  u16 m; s64 t;
  if (fstat_comun(fd, &m, &t)) return -1;
  memset(s, 0, sizeof *s); s->mode = m; s->size = (long)t; s->nlink = 1;
  return 0;
}
int _fstat32(int fd, struct stat_ms *s) { return _fstat(fd, s); }
int _waccess(const wchar_t *w, int m) { char b[262]; return _access(ancho_a_nombre(w, b), m); }
FILE *_wfopen(const wchar_t *w, const wchar_t *wm) {
  char b[262], m[16]; int i = 0;
  for (; wm[i] && i < 15; i++) m[i] = (char)wm[i];
  m[i] = 0;
  return fopen(ancho_a_nombre(w, b), m);
}
int _wremove(const wchar_t *w) { char b[262]; return remove(ancho_a_nombre(w, b)); }
int _wunlink(const wchar_t *w) { return _wremove(w); }
int _wrename(const wchar_t *a, const wchar_t *c) { char b[262], d[262]; return rename(ancho_a_nombre(a, b), ancho_a_nombre(c, d)); }
int _wopen(const wchar_t *w, int m, ...) { char b[262]; return _open(ancho_a_nombre(w, b), m); }
int _wmkdir(const wchar_t *w) { char b[262]; return _mkdir(ancho_a_nombre(w, b)); }
FILE *tmpfile(void) {
  static int n;
  char nom[32]; snprintf(nom, sizeof nom, "~HW%05u.TMP", (GetTickCount() + n++) % 100000);
  FILE *f = fopen(nom, "w+b");
  if (f) f->tmpname = _strdup(nom);
  return f;
}
char *tmpnam(char *b) {
  static char t[32]; static int n;
  if (!b) b = t;
  snprintf(b, 16, "~HW%05u.TMP", (GetTickCount() + n++) % 100000);
  return b;
}

/* ---- la hora ---- */
typedef long time_t;
struct tm { int sec, min, hour, mday, mon, year, wday, yday, isdst; };
static const short antes_mes[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
static int bisiesto(int a) { return (a % 4 == 0 && a % 100 != 0) || a % 400 == 0; }
time_t time(time_t *t) {
  FT f; GetSystemTimeAsFileTime(&f);
  time_t r = t_de_ft(&f);
  if (t) *t = r;
  return r;
}
s64 _time64(s64 *t) { s64 r = time(NULL); if (t) *t = r; return r; }
static struct tm tm_global;
static struct tm *tm_de_t(time_t t) {
  if (t < 0) return NULL;
  u64 x = (u64)(unsigned long)t * 10000000ull + 116444736000000000ull;
  FT f = { (u32)x, (u32)(x >> 32) };
  ST s; FileTimeToSystemTime(&f, &s);
  struct tm *r = &tm_global;
  r->sec = s.seg; r->min = s.min; r->hour = s.hora; r->mday = s.dia; r->mon = s.mes - 1; r->year = s.anio - 1900;
  r->wday = s.sem; r->yday = antes_mes[s.mes - 1] + s.dia - 1 + (s.mes > 2 && bisiesto(s.anio)); r->isdst = 0;
  return r;
}
struct tm *gmtime(const time_t *t) { return tm_de_t(*t); }
struct tm *localtime(const time_t *t) { return tm_de_t(*t); }   /* (sin husos: la hora de DOS es la local) */
struct tm *_gmtime64(const s64 *t) { return tm_de_t((time_t)*t); }
struct tm *_localtime64(const s64 *t) { return tm_de_t((time_t)*t); }
time_t mktime(struct tm *m) {
  /* normalizar lo que se salga */
  int anio = m->year + 1900, mes = m->mon;
  while (mes < 0) { mes += 12; anio--; }
  while (mes > 11) { mes -= 12; anio++; }
  s64 dias = 0;
  for (int a = 1970; a < anio; a++) dias += 365 + bisiesto(a);
  for (int a = anio; a < 1970; a++) dias -= 365 + bisiesto(a);
  dias += antes_mes[mes] + (mes > 1 && bisiesto(anio)) + m->mday - 1;
  s64 t = dias * 86400 + m->hour * 3600 + m->min * 60 + m->sec;
  if (t < 0 || t > 0x7FFFFFFF) return -1;
  struct tm *r = tm_de_t((time_t)t);
  if (r) *m = *r;
  return (time_t)t;
}
s64 _mktime64(struct tm *m) { return mktime(m); }
double difftime(time_t a, time_t b) { return (double)(a - b); }
long clock(void) { static u32 ini; u32 t = GetTickCount(); if (!ini) ini = t; return t - ini; }
static const char *dias_sem[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
static const char *meses[] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };
size_t strftime(char *b, size_t max, const char *f, const struct tm *t) {
  size_t n = 0;
  char tmp[64];
  for (; *f; f++) {
    const char *s = tmp;
    if (*f != '%') { tmp[0] = *f; tmp[1] = 0; }
    else {
      f++;
      if (*f == '#') f++;
      switch (*f) {
      case 'a': snprintf(tmp, 64, "%.3s", dias_sem[t->wday % 7]); break;
      case 'A': s = dias_sem[t->wday % 7]; break;
      case 'b': case 'h': snprintf(tmp, 64, "%.3s", meses[t->mon % 12]); break;
      case 'B': s = meses[t->mon % 12]; break;
      case 'c': snprintf(tmp, 64, "%02d/%02d/%02d %02d:%02d:%02d", t->mon + 1, t->mday, t->year % 100, t->hour, t->min, t->sec); break;
      case 'd': snprintf(tmp, 64, "%02d", t->mday); break;
      case 'e': snprintf(tmp, 64, "%2d", t->mday); break;
      case 'H': snprintf(tmp, 64, "%02d", t->hour); break;
      case 'I': snprintf(tmp, 64, "%02d", t->hour % 12 ? t->hour % 12 : 12); break;
      case 'j': snprintf(tmp, 64, "%03d", t->yday + 1); break;
      case 'm': snprintf(tmp, 64, "%02d", t->mon + 1); break;
      case 'M': snprintf(tmp, 64, "%02d", t->min); break;
      case 'p': s = t->hour < 12 ? "AM" : "PM"; break;
      case 'S': snprintf(tmp, 64, "%02d", t->sec); break;
      case 'w': snprintf(tmp, 64, "%d", t->wday); break;
      case 'x': snprintf(tmp, 64, "%02d/%02d/%02d", t->mon + 1, t->mday, t->year % 100); break;
      case 'X': case 'T': snprintf(tmp, 64, "%02d:%02d:%02d", t->hour, t->min, t->sec); break;
      case 'y': snprintf(tmp, 64, "%02d", t->year % 100); break;
      case 'Y': snprintf(tmp, 64, "%d", t->year + 1900); break;
      case 'F': snprintf(tmp, 64, "%d-%02d-%02d", t->year + 1900, t->mon + 1, t->mday); break;
      case 'D': snprintf(tmp, 64, "%02d/%02d/%02d", t->mon + 1, t->mday, t->year % 100); break;
      case 'Z': case 'z': tmp[0] = 0; break;
      case '%': s = "%"; break;
      case 0: f--; tmp[0] = 0; break;
      default: tmp[0] = '%'; tmp[1] = *f; tmp[2] = 0; break;
      }
    }
    size_t l = strlen(s);
    if (n + l >= max) return 0;
    memcpy(b + n, s, l); n += l;
  }
  b[n] = 0;
  return n;
}
char *asctime(const struct tm *t) {
  static char b[32];
  snprintf(b, sizeof b, "%.3s %.3s %2d %02d:%02d:%02d %d\n", dias_sem[t->wday % 7], meses[t->mon % 12], t->mday, t->hour, t->min, t->sec, t->year + 1900);
  return b;
}
char *ctime(const time_t *t) { struct tm *m = localtime(t); return m ? asctime(m) : NULL; }

/* ---- locale («C») ---- */
struct lconv { char *dp, *ts, *gr, *ics, *cs, *mdp, *mts, *mgr, *ps, *ns; char ifd, fd, pcs, psb, ncs, nsb, psn, nsn; };
static struct lconv lc = { ".", "", "", "", "", "", "", "", "", "", 127, 127, 127, 127, 127, 127, 127, 127 };
struct lconv *localeconv(void) { return &lc; }
char *setlocale(int c, const char *l) {
  static char C[] = "C";
  if (!l || !*l || !strcmp(l, "C")) return C;
  return NULL;
}
int ___lc_codepage_func(void) { return 0; }
unsigned ___lc_handle_func(void) { return 0; }
int mblen(const char *s, size_t n) { return s && *s ? 1 : 0; }
int mbtowc(wchar_t *w, const char *s, size_t n) { if (!s) return 0; if (w) *w = (u8)*s; return *s ? 1 : 0; }
int wctomb(char *s, wchar_t w) { if (!s) return 0; *s = w > 255 ? '?' : (char)w; return 1; }
size_t mbstowcs(wchar_t *w, const char *s, size_t n) { size_t i = 0; for (; s[i] && (!w || i < n); i++) if (w) w[i] = (u8)s[i]; if (w && i < n) w[i] = 0; return i; }
size_t wcstombs(char *s, const wchar_t *w, size_t n) { size_t i = 0; for (; w[i] && (!s || i < n); i++) if (s) s[i] = w[i] > 255 ? '?' : (char)w[i]; if (s && i < n) s[i] = 0; return i; }
int _isleadbyte_l(int c, void *l) { return 0; }
int isleadbyte(int c) { return 0; }
int iswalpha(wchar_t c) { return c < 128 && isalpha(c); }
int iswdigit(wchar_t c) { return c < 128 && isdigit(c); }
int iswspace(wchar_t c) { return c < 128 && isspace(c); }
int iswctype(wchar_t c, u16 m) { return c < 256 ? _isctype(c, m) : 0; }
wchar_t towlower(wchar_t c) { return c < 128 ? tolower(c) : c; }
wchar_t towupper(wchar_t c) { return c < 128 ? toupper(c) : c; }

/* ---- qsort y bsearch ---- */
static void cambia(u8 *a, u8 *b, size_t t) { while (t--) { u8 x = *a; *a++ = *b; *b++ = x; } }
void qsort(void *base, size_t n, size_t t, int (*cmp)(const void *, const void *)) {
  /* montículo: sin recursión y sin pedir memoria */
  u8 *b = base;
  if (n < 2) return;
  for (size_t i = n / 2; i-- > 0;) {
    size_t r = i;
    for (;;) { size_t h = 2 * r + 1; if (h >= n) break; if (h + 1 < n && cmp(b + h * t, b + (h + 1) * t) < 0) h++;
               if (cmp(b + r * t, b + h * t) >= 0) break; cambia(b + r * t, b + h * t, t); r = h; }
  }
  for (size_t m = n - 1; m > 0; m--) {
    cambia(b, b + m * t, t);
    size_t r = 0;
    for (;;) { size_t h = 2 * r + 1; if (h >= m) break; if (h + 1 < m && cmp(b + h * t, b + (h + 1) * t) < 0) h++;
               if (cmp(b + r * t, b + h * t) >= 0) break; cambia(b + r * t, b + h * t, t); r = h; }
  }
}
void *bsearch(const void *k, const void *base, size_t n, size_t t, int (*cmp)(const void *, const void *)) {
  const u8 *b = base;
  while (n) {
    size_t m = n / 2;
    int c = cmp(k, b + m * t);
    if (!c) return (void *)(b + m * t);
    if (c > 0) { b += (m + 1) * t; n -= m + 1; } else n = m;
  }
  return NULL;
}

/* ---- setjmp y longjmp (el jmp_buf de Microsoft: Ebp, Ebx, Edi, Esi, Esp,
 * Eip, Registration, TryLevel...). Sin SEH: se vuelve a la vuelta de setjmp
 * con los registros que guardan las llamadas ---- */
__attribute__((naked)) int _setjmp3(void *jb, int n, ...) {
  __asm__("movl 4(%esp), %edx\n"
          "movl %ebp, 0(%edx)\n movl %ebx, 4(%edx)\n movl %edi, 8(%edx)\n movl %esi, 12(%edx)\n"
          "leal 4(%esp), %ecx\n movl %ecx, 16(%edx)\n"   /* la pila tras volver */
          "movl (%esp), %ecx\n movl %ecx, 20(%edx)\n"    /* la vuelta */
          "movl %fs:0, %ecx\n movl %ecx, 24(%edx)\n movl $-1, 28(%edx)\n"
          "xorl %eax, %eax\n ret\n");
}
__attribute__((naked)) int _setjmp(void *jb) { __asm__("jmp _setjmp3"); }
__attribute__((naked)) void longjmp(void *jb, int v) {
  __asm__("movl 4(%esp), %edx\n movl 8(%esp), %eax\n"
          "testl %eax, %eax\n jnz 1f\n incl %eax\n1:\n"
          "movl 0(%edx), %ebp\n movl 4(%edx), %ebx\n movl 8(%edx), %edi\n movl 12(%edx), %esi\n"
          "movl 16(%edx), %esp\n jmp *20(%edx)\n");
}

/* ---- el coprocesador (x87): la palabra de control. Las máscaras de
 * Microsoft (_MCW_EM 0x8001F, _MCW_RC 0x300, _MCW_PC 0x30000) no son los bits
 * del x87: se traducen ---- */
static u32 cw_a_ms(u16 cw) {
  u32 r = 0;
  if (cw & 0x01) r |= 0x10;      /* inválida */
  if (cw & 0x02) r |= 0x80000;   /* denormal */
  if (cw & 0x04) r |= 0x08;      /* división por cero */
  if (cw & 0x08) r |= 0x04;      /* desbordamiento */
  if (cw & 0x10) r |= 0x02;      /* subdesbordamiento */
  if (cw & 0x20) r |= 0x01;      /* inexacta */
  switch ((cw >> 10) & 3) { case 1: r |= 0x100; break; case 2: r |= 0x200; break; case 3: r |= 0x300; break; }
  switch ((cw >> 8) & 3) { case 0: r |= 0x20000; break; case 2: r |= 0x10000; break; }
  return r;
}
static u16 ms_a_cw(u32 m, u16 cw) {
  cw &= ~0x0F3F;
  if (m & 0x10) cw |= 0x01; if (m & 0x80000) cw |= 0x02; if (m & 0x08) cw |= 0x04;
  if (m & 0x04) cw |= 0x08; if (m & 0x02) cw |= 0x10; if (m & 0x01) cw |= 0x20;
  cw |= ((m & 0x100) ? 1 : 0) << 10 | ((m & 0x200) ? 2 : 0) << 10;
  cw |= (m & 0x20000) ? 0 : (m & 0x10000) ? 2 << 8 : 3 << 8;
  return cw;
}
unsigned _controlfp(unsigned nuevo, unsigned mascara) {
  u16 cw; __asm__ volatile("fnstcw %0" : "=m"(cw));
  u32 v = cw_a_ms(cw);
  if (mascara) {
    v = (v & ~mascara) | (nuevo & mascara);
    u16 c2 = ms_a_cw(v, cw);
    __asm__ volatile("fldcw %0" : : "m"(c2));
  }
  return v;
}
unsigned _control87(unsigned n, unsigned m) { return _controlfp(n, m); }
unsigned _clearfp(void) { u16 sw; __asm__ volatile("fnstsw %0; fnclex" : "=m"(sw)); return sw & 0x3F; }
unsigned _statusfp(void) { u16 sw; __asm__ volatile("fnstsw %0" : "=m"(sw)); return sw & 0x3F; }
void _fpreset(void) { __asm__ volatile("fninit"); }
/* el manejador de excepciones de Visual C++ (sin SEH: no se llega a usar) */
int _except_handler3(void *a, void *b, void *c, void *d) { return 1; }
int _except_handler4_common(void *a, void *b, void *c, void *d, void *e, void *f) { return 1; }

/* ---- otros programas: aún no (Hispa-486 todavía no lanza procesos) ---- */
int _spawnvp(int modo, const char *n, const char *const *argv) { errno_v = ENOENT; return -1; }
int _spawnv(int modo, const char *n, const char *const *argv) { errno_v = ENOENT; return -1; }
int _spawnl(int modo, const char *n, ...) { errno_v = ENOENT; return -1; }
int _cwait(int *r, int h, int a) { errno_v = ECHILD_; return -1; }
int system(const char *c) { if (!c) return 0; errno_v = ENOENT; return -1; }
int __lconv_init(void) { return 0; }

/* ---- consola (conio) ---- */
int _getch(void) {
  u8 c; u32 r;
  HANDLE h = GetStdHandle(-10);
  return ReadFile(h, &c, 1, &r, NULL) && r ? c : EOF;
}
int _putch(int c) { char x = (char)c; u32 r; WriteFile(GetStdHandle(-11), &x, 1, &r, NULL); return c; }
int _kbhit(void) { return 0; }

/* ---- el arranque de la DLL ---- */
int WINAPI DllMain(HANDLE h, u32 razon, void *x) {
  if (razon != 1) return 1;
  monton = GetProcessHeap();
  for (int i = 0; i < 3; i++) {
    fds[i].h = GetStdHandle(-10 - i);
    fds[i].abierto = 1;
    fds[i].binario = 0;
    fds[i].dispositivo = GetFileType(fds[i].h) == 2;
    _iob[i].file = i;
    _iob[i].flag = (i ? F_ESC : F_LEE) | F_EN_USO;
  }
  _iob[2].flag |= F_SINBUF;
  for (int c = 0; c < 256; c++) tabla_ctype[c + 1] = _isctype(c, 0x1FF);
  _pctype = tabla_ctype + 1;
  prepara_args();             /* (_acmdln, __argv y _environ, listos desde el principio) */
  return 1;
}
