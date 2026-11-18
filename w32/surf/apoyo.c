/* =============================================================================
 *  apoyo.c - lo que NetSurf (terceros/netsurf) pide a la biblioteca de C y
 *  no tiene MSVCRT.DLL de Hispa-486: los nombres de POSIX (strcasecmp, stat,
 *  opendir...), iconv (UTF-8, Latin-1, 1252, ASCII y UTF-16), la hora en
 *  microsegundos, lo poco de zlib que se usa, las cuentas en float...
 *  Para NAVEGA2.EXE, el Navegador completo (docs/486/NAVEGADOR2.md).
 *  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * ============================================================================= */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <iconv.h>
#include <dirent.h>
#include <zlib.h>
#include <regex.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <fcntl.h>

#define WINAPI __attribute__((stdcall))
typedef void *HANDLE;
typedef struct { uint32_t lo, hi; } FT;
typedef struct { uint32_t atrib; FT c, a, e; uint32_t tam_hi, tam_lo, r0, r1; char nombre[260]; char alt[14]; } FINDA;
HANDLE WINAPI FindFirstFileA(const char *, FINDA *);
int WINAPI FindNextFileA(HANDLE, FINDA *);
int WINAPI FindClose(HANDLE);
void WINAPI GetSystemTimeAsFileTime(FT *);
uint32_t WINAPI GetTickCount(void);
void WINAPI Sleep(uint32_t);
int WINAPI MessageBoxA(HANDLE, const char *, const char *, uint32_t);

/* de MSVCRT, con su nombre de allí */
int _stricmp(const char *, const char *);
int _strnicmp(const char *, const char *, size_t);
char *_strdup(const char *);
long long _strtoi64(const char *, char **, int);
unsigned long long _strtoui64(const char *, char **, int);
struct stat;
int _stat32(const char *, struct stat *);
int _fstat32(int, struct stat *);
int _mkdir(const char *);
int _rmdir(const char *);
int _unlink(const char *);
int _access(const char *, int);
int _close(int);
int _read(int, void *, unsigned);
int _write(int, const void *, unsigned);
long _lseek(int, long, int);
char *_getcwd(char *, int);
int _chdir(const char *);
int _isatty(int);
char *_fullpath(char *, const char *, size_t);
int _open(const char *, int, ...);
int _putenv(const char *);

/* ---- cadenas ---- */
int strcasecmp(const char *a, const char *b) { return _stricmp(a, b); }
int strncasecmp(const char *a, const char *b, size_t n) { return n ? _strnicmp(a, b, n) : 0; }
char *strdup(const char *s) { return _strdup(s); }
size_t strnlen(const char *s, size_t n) { size_t i = 0; while (i < n && s[i]) i++; return i; }
void *memrchr(const void *s, int c, size_t n) {
  const unsigned char *p = (const unsigned char *)s + n;
  while (n--) if (*--p == (unsigned char)c) return (void *)p;
  return NULL;
}
void *memmem(const void *h, size_t nh, const void *n, size_t nn) {
  const char *p = h;
  if (!nn) return (void *)h;
  for (; nh >= nn; p++, nh--) if (*p == *(const char *)n && !memcmp(p, n, nn)) return (void *)p;
  return NULL;
}
size_t strlcpy(char *d, const char *s, size_t n) {
  size_t l = strlen(s);
  if (n) { size_t c = l < n - 1 ? l : n - 1; memcpy(d, s, c); d[c] = 0; }
  return l;
}
size_t strlcat(char *d, const char *s, size_t n) {
  size_t l = strnlen(d, n);
  return l == n ? n + strlen(s) : l + strlcpy(d + l, s, n - l);
}
char *stpcpy(char *d, const char *s) { size_t l = strlen(s); memcpy(d, s, l + 1); return d + l; }
char *strtok_r(char *s, const char *sep, char **q) {
  if (!s) s = *q;
  s += strspn(s, sep);
  if (!*s) { *q = s; return NULL; }
  char *e = s + strcspn(s, sep);
  if (*e) *e++ = 0;
  *q = e;
  return s;
}
char *strsep(char **q, const char *sep) {
  char *s = *q, *e;
  if (!s) return NULL;
  e = s + strcspn(s, sep);
  if (*e) { *e = 0; *q = e + 1; } else *q = NULL;
  return s;
}
int strcoll(const char *a, const char *b) { return strcmp(a, b); }
long long strtoll(const char *s, char **e, int b) { return _strtoi64(s, e, b); }
unsigned long long strtoull(const char *s, char **e, int b) { return _strtoui64(s, e, b); }
float strtof(const char *s, char **e) { return (float)strtod(s, e); }
long long llabs(long long v) { return v < 0 ? -v : v; }
long random(void) { return ((long)rand() << 15) ^ rand(); }
void srandom(unsigned s) { srand(s); }

int vasprintf(char **r, const char *f, va_list ap) {
  va_list c;
  va_copy(c, ap);
  int n = vsnprintf(NULL, 0, f, c);
  va_end(c);
  *r = NULL;
  if (n < 0 || !(*r = malloc(n + 1))) return -1;
  return vsnprintf(*r, n + 1, f, ap);
}
int asprintf(char **r, const char *f, ...) {
  va_list ap; va_start(ap, f); int n = vasprintf(r, f, ap); va_end(ap); return n;
}
ssize_t getline(char **l, size_t *n, FILE *f) {
  size_t k = 0;
  int c;
  if (!*l || !*n) { *n = 128; *l = malloc(*n); if (!*l) return -1; }
  while ((c = fgetc(f)) != EOF) {
    if (k + 2 > *n) { char *t = realloc(*l, *n * 2); if (!t) return -1; *l = t; *n *= 2; }
    (*l)[k++] = c;
    if (c == '\n') break;
  }
  (*l)[k] = 0;
  return k ? (ssize_t)k : -1;
}

/* ---- el aserto ---- */
void hs_aserto(const char *e, const char *f, int l) {
  char b[400];
  snprintf(b, sizeof b, "Fallo interno de NetSurf:\n%s\n(%s, línea %d)", e, f, l);
  MessageBoxA(NULL, b, "Navegador completo", 0x10);
  abort();
}

/* ---- archivos y carpetas (POSIX sobre MSVCRT) ---- */
int stat(const char *n, struct stat *s) { return _stat32(n, s); }
int lstat(const char *n, struct stat *s) { return _stat32(n, s); }
int fstat(int f, struct stat *s) { return _fstat32(f, s); }
int mkdir(const char *n, mode_t m) { (void)m; return _mkdir(n); }
int rmdir(const char *n) { return _rmdir(n); }
int unlink(const char *n) { return _unlink(n); }
int access(const char *n, int m) { return _access(n, m & 6); }
int close(int f) { return _close(f); }
ssize_t read(int f, void *b, size_t n) { return _read(f, b, n); }
ssize_t write(int f, const void *b, size_t n) { return _write(f, b, n); }
off_t lseek(int f, off_t o, int w) { return _lseek(f, o, w); }
ssize_t pread(int f, void *b, size_t n, off_t o) { if (_lseek(f, o, 0) < 0) return -1; return _read(f, b, n); }
ssize_t pwrite(int f, const void *b, size_t n, off_t o) { if (_lseek(f, o, 0) < 0) return -1; return _write(f, b, n); }
char *getcwd(char *b, size_t n) { return _getcwd(b, n); }
int chdir(const char *n) { return _chdir(n); }
int isatty(int f) { return _isatty(f); }
int chmod(const char *n, mode_t m) { (void)n; (void)m; return 0; }
int fsync(int f) { (void)f; return 0; }
int ftruncate(int f, off_t n) { (void)f; (void)n; return -1; }
pid_t getpid(void) { return 1; }
uid_t getuid(void) { return 0; }
long sysconf(int n) { return n == _SC_PAGESIZE ? 4096 : -1; }
unsigned sleep(unsigned s) { Sleep(s * 1000); return 0; }
int usleep(unsigned u) { Sleep(u / 1000); return 0; }
int gethostname(char *b, size_t n) { strlcpy(b, "hispa486", n); return 0; }
char *realpath(const char *n, char *r) {
  char *b = r ? r : malloc(260);
  if (!b) return NULL;
  if (!_fullpath(b, n, 260)) { if (!r) free(b); return NULL; }
  return b;
}
int setenv(const char *n, const char *v, int o) {
  char b[300];
  if (!o && getenv(n)) return 0;
  snprintf(b, sizeof b, "%s=%s", n, v);
  return _putenv(b);
}
int unsetenv(const char *n) { char b[300]; snprintf(b, sizeof b, "%s=", n); return _putenv(b); }
int open(const char *n, int m, ...) { return _open(n, m | O_BINARY, 0x180); }
int uname(struct utsname *u) {
  strcpy(u->sysname, "Hispa-486"); strcpy(u->nodename, "hispa486"); strcpy(u->release, "0.9");
  strcpy(u->version, "Hispa-DOS"); strcpy(u->machine, "i486");
  return 0;
}
int mkstemp(char *t) { (void)t; errno = ENOSYS; return -1; }
char *mkdtemp(char *t) { (void)t; errno = ENOSYS; return NULL; }
FILE *fdopen(int f, const char *m) { FILE *_fdopen(int, const char *); return _fdopen(f, m); }

struct HS_DIR { HANDLE h; int primera, fin; FINDA d; struct dirent e; };
DIR *opendir(const char *n) {
  char b[300];
  size_t l = strlen(n);
  DIR *d = calloc(1, sizeof *d);
  if (!d) return NULL;
  snprintf(b, sizeof b, "%s%s*.*", n, l && (n[l - 1] == '\\' || n[l - 1] == '/') ? "" : "\\");
  d->h = FindFirstFileA(b, &d->d);
  if (d->h == (HANDLE)-1) { free(d); errno = ENOENT; return NULL; }
  d->primera = 1;
  return d;
}
struct dirent *readdir(DIR *d) {
  if (d->fin) return NULL;
  if (!d->primera && !FindNextFileA(d->h, &d->d)) { d->fin = 1; return NULL; }
  d->primera = 0;
  strlcpy(d->e.d_name, d->d.nombre, sizeof d->e.d_name);
  d->e.d_type = (d->d.atrib & 0x10) ? DT_DIR : DT_REG;
  return &d->e;
}
int closedir(DIR *d) { FindClose(d->h); free(d); return 0; }

/* ---- la hora ---- */
static uint64_t us_1970(void) {
  FT f;
  GetSystemTimeAsFileTime(&f);
  uint64_t t = ((uint64_t)f.hi << 32) | f.lo;
  return (t - 116444736000000000ull) / 10;
}
int gettimeofday(struct timeval *tv, void *tz) {
  (void)tz;
  uint64_t u = us_1970();
  tv->tv_sec = (long)(u / 1000000);
  tv->tv_usec = (long)(u % 1000000);
  return 0;
}
int clock_gettime(clockid_t c, struct timespec *t) {
  if (c == CLOCK_MONOTONIC) {
    uint32_t ms = GetTickCount();
    t->tv_sec = ms / 1000; t->tv_nsec = (ms % 1000) * 1000000;
  } else {
    uint64_t u = us_1970();
    t->tv_sec = (time_t)(u / 1000000); t->tv_nsec = (long)(u % 1000000) * 1000;
  }
  return 0;
}
int nanosleep(const struct timespec *r, struct timespec *q) {
  (void)q;
  Sleep(r->tv_sec * 1000 + r->tv_nsec / 1000000);
  return 0;
}
long timezone;
void tzset(void) { }
struct tm *gmtime_r(const time_t *t, struct tm *r) { struct tm *g = gmtime(t); if (!g) return NULL; *r = *g; return r; }
struct tm *localtime_r(const time_t *t, struct tm *r) { struct tm *g = localtime(t); if (!g) return NULL; *r = *g; return r; }
time_t timegm(struct tm *m) {
  static const int dm[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
  int a = m->tm_year + 1900, mes = m->tm_mon;
  while (mes < 0) { mes += 12; a--; }
  while (mes > 11) { mes -= 12; a++; }
  long d = (a - 1970) * 365L + (a - 1969) / 4 - (a - 1901) / 100 + (a - 1601) / 400 + dm[mes] + m->tm_mday - 1;
  if (mes > 1 && ((a % 4 == 0 && a % 100) || a % 400 == 0)) d++;
  return (time_t)(d * 86400L + m->tm_hour * 3600L + m->tm_min * 60L + m->tm_sec);
}

/* ---- cuentas en float (con las de double de MSVCRT) ---- */
/* (con otro nombre en C: si no, gcc cambia «(float)floor(x)» por floorf y
 * floorf se llamaría a sí misma) */
extern double m_floor(double) __asm__("floor");
extern double m_ceil(double) __asm__("ceil");
extern double m_sqrt(double) __asm__("sqrt");
extern double m_sin(double) __asm__("sin");
extern double m_cos(double) __asm__("cos");
extern double m_tan(double) __asm__("tan");
extern double m_atan2(double, double) __asm__("atan2");
extern double m_pow(double, double) __asm__("pow");
extern double m_fmod(double, double) __asm__("fmod");
float fabsf(float x) { return x < 0 ? -x : x; }
float floorf(float x) { return (float)m_floor(x); }
float ceilf(float x) { return (float)m_ceil(x); }
float sqrtf(float x) { return (float)m_sqrt(x); }
float sinf(float x) { return (float)m_sin(x); }
float cosf(float x) { return (float)m_cos(x); }
float tanf(float x) { return (float)m_tan(x); }
float atan2f(float y, float x) { return (float)m_atan2(y, x); }
float powf(float x, float y) { return (float)m_pow(x, y); }
float fmodf(float x, float y) { return (float)m_fmod(x, y); }
double trunc(double x) { return x < 0 ? m_ceil(x) : m_floor(x); }
float truncf(float x) { return (float)trunc(x); }
double round(double x) { return x < 0 ? -m_floor(-x + 0.5) : m_floor(x + 0.5); }
float roundf(float x) { return (float)round(x); }
long lround(double x) { return (long)round(x); }
long lroundf(float x) { return (long)round(x); }
double rint(double x) { return round(x); }
long lrint(double x) { return (long)round(x); }
double fmin(double a, double b) { return a < b ? a : b; }
double fmax(double a, double b) { return a > b ? a : b; }
float fminf(float a, float b) { return a < b ? a : b; }
float fmaxf(float a, float b) { return a > b ? a : b; }
double hypot(double a, double b) { return sqrt(a * a + b * b); }
double log2(double x) { return log(x) / M_LN2; }
double copysign(double a, double b) { a = fabs(a); return signbit(b) ? -a : a; }
double asin(double x) { return atan2(x, sqrt(1 - x * x)); }
double acos(double x) { return atan2(sqrt(1 - x * x), x); }
double frexp(double x, int *e) {
  *e = 0;
  if (x == 0 || isnan(x) || isinf(x)) return x;
  double a = fabs(x);
  while (a >= 1) { a /= 2; (*e)++; }
  while (a < 0.5) { a *= 2; (*e)--; }
  return x < 0 ? -a : a;
}
void sincos(double x, double *s, double *c) { *s = m_sin(x); *c = m_cos(x); }
void sincosf(float x, float *s, float *c) { *s = (float)m_sin(x); *c = (float)m_cos(x); }

/* ---- zlib: lo justo para leer los Messages sin comprimir ---- */
gzFile gzopen(const char *n, const char *m) { return (gzFile)fopen(n, m[0] == 'w' ? "wb" : "rb"); }
char *gzgets(gzFile f, char *b, int n) { return fgets(b, n, (FILE *)f); }
int gzputs(gzFile f, const char *s) { return fputs(s, (FILE *)f); }
int gzclose(gzFile f) { return fclose((FILE *)f); }
int inflateInit2(z_stream *z, int w) { (void)z; (void)w; return Z_DATA_ERROR; }
int inflate(z_stream *z, int f) { (void)z; (void)f; return Z_DATA_ERROR; }
int inflateEnd(z_stream *z) { (void)z; return Z_OK; }


/* ---- iconv: UTF-8, UTF-16 (LE y BE), UTF-32, ISO-8859-1, ISO-8859-15,
 * Windows-1252 y ASCII. Lo que no cabe en el destino: «?» ---- */
enum { CS_UTF8 = 1, CS_LATIN1, CS_1252, CS_ASCII, CS_UTF16LE, CS_UTF16BE, CS_UTF32LE, CS_UTF32BE, CS_LATIN9 };
static const uint16_t w1252[32] = {
  0x20AC, 0, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0, 0x017D, 0,
  0, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0, 0x017E, 0x0178 };
static int cs_de(const char *n) {
  char b[32];
  size_t i;
  for (i = 0; n[i] && n[i] != '/' && i < sizeof b - 1; i++) b[i] = n[i] >= 'a' && n[i] <= 'z' ? n[i] - 32 : n[i];
  b[i] = 0;
  if (!strcmp(b, "UTF-8") || !strcmp(b, "UTF8")) return CS_UTF8;
  if (!strcmp(b, "ISO-8859-1") || !strcmp(b, "LATIN1") || !strcmp(b, "ISO8859-1") || !strcmp(b, "ISO_8859-1")) return CS_LATIN1;
  if (!strcmp(b, "ISO-8859-15") || !strcmp(b, "LATIN9")) return CS_LATIN9;
  if (!strcmp(b, "WINDOWS-1252") || !strcmp(b, "CP1252")) return CS_1252;
  if (!strcmp(b, "US-ASCII") || !strcmp(b, "ASCII") || !strcmp(b, "ANSI_X3.4-1968")) return CS_ASCII;
  if (!strcmp(b, "UTF-16LE") || !strcmp(b, "UTF-16") || !strcmp(b, "UCS-2LE") || !strcmp(b, "UCS-2")) return CS_UTF16LE;
  if (!strcmp(b, "UTF-16BE") || !strcmp(b, "UCS-2BE")) return CS_UTF16BE;
  if (!strcmp(b, "UTF-32LE") || !strcmp(b, "UCS-4LE") || !strcmp(b, "UTF-32") || !strcmp(b, "UCS-4")) return CS_UTF32LE;
  if (!strcmp(b, "UTF-32BE") || !strcmp(b, "UCS-4BE")) return CS_UTF32BE;
  return 0;
}
iconv_t iconv_open(const char *a, const char *de) {
  int x = cs_de(a), y = cs_de(de);
  if (!x || !y) { errno = EINVAL; return (iconv_t)-1; }
  return (iconv_t)(intptr_t)(x << 8 | y);
}
int iconv_close(iconv_t c) { (void)c; return 0; }
/* lee una letra de o (n octetos): la letra, o -1 si está cortada, -2 si no vale; *k octetos */
static long lee_cs(int cs, const unsigned char *o, size_t n, int *k) {
  switch (cs) {
  case CS_UTF8: {
    unsigned c = o[0], m, l;
    if (c < 0x80) { *k = 1; return c; }
    if (c < 0xC2) return -2;
    l = c < 0xE0 ? 2 : c < 0xF0 ? 3 : c < 0xF5 ? 4 : 0;
    if (!l) return -2;
    if (n < l) return -1;
    m = c & (0x7F >> l);
    for (unsigned i = 1; i < l; i++) { if ((o[i] & 0xC0) != 0x80) return -2; m = m << 6 | (o[i] & 0x3F); }
    *k = l;
    return m;
  }
  case CS_UTF16LE: case CS_UTF16BE: {
    if (n < 2) return -1;
    unsigned c = cs == CS_UTF16LE ? o[0] | o[1] << 8 : o[0] << 8 | o[1];
    if (c >= 0xD800 && c < 0xDC00) {
      if (n < 4) return -1;
      unsigned d = cs == CS_UTF16LE ? o[2] | o[3] << 8 : o[2] << 8 | o[3];
      *k = 4;
      return 0x10000 + ((c - 0xD800) << 10) + (d - 0xDC00);
    }
    *k = 2;
    return c;
  }
  case CS_UTF32LE: case CS_UTF32BE:
    if (n < 4) return -1;
    *k = 4;
    return cs == CS_UTF32LE ? (long)(o[0] | o[1] << 8 | o[2] << 16 | (uint32_t)o[3] << 24)
                            : (long)((uint32_t)o[0] << 24 | o[1] << 16 | o[2] << 8 | o[3]);
  case CS_1252:
    *k = 1;
    if (o[0] >= 0x80 && o[0] < 0xA0) return w1252[o[0] - 0x80] ? w1252[o[0] - 0x80] : o[0];
    return o[0];
  case CS_LATIN9:
    *k = 1;
    switch (o[0]) { case 0xA4: return 0x20AC; case 0xA6: return 0x160; case 0xA8: return 0x161; case 0xB4: return 0x17D;
                    case 0xB8: return 0x17E; case 0xBC: return 0x152; case 0xBD: return 0x153; case 0xBE: return 0x178; }
    return o[0];
  case CS_ASCII:
    *k = 1;
    return o[0] < 0x80 ? o[0] : -2;
  default:
    *k = 1;
    return o[0];
  }
}
/* escribe la letra c en d (n octetos libres): los que escribe, -1 si no cabe */
static int pon_cs(int cs, unsigned long c, unsigned char *d, size_t n) {
  switch (cs) {
  case CS_UTF8:
    if (c < 0x80) { if (n < 1) return -1; d[0] = c; return 1; }
    if (c < 0x800) { if (n < 2) return -1; d[0] = 0xC0 | c >> 6; d[1] = 0x80 | (c & 63); return 2; }
    if (c < 0x10000) { if (n < 3) return -1; d[0] = 0xE0 | c >> 12; d[1] = 0x80 | (c >> 6 & 63); d[2] = 0x80 | (c & 63); return 3; }
    if (n < 4) return -1;
    d[0] = 0xF0 | c >> 18; d[1] = 0x80 | (c >> 12 & 63); d[2] = 0x80 | (c >> 6 & 63); d[3] = 0x80 | (c & 63);
    return 4;
  case CS_UTF16LE: case CS_UTF16BE: {
    unsigned a = c, b = 0, l = 2;
    if (c >= 0x10000) { c -= 0x10000; a = 0xD800 + (c >> 10); b = 0xDC00 + (c & 0x3FF); l = 4; }
    if (n < l) return -1;
    if (cs == CS_UTF16LE) { d[0] = a; d[1] = a >> 8; if (l == 4) { d[2] = b; d[3] = b >> 8; } }
    else { d[0] = a >> 8; d[1] = a; if (l == 4) { d[2] = b >> 8; d[3] = b; } }
    return l;
  }
  case CS_UTF32LE: case CS_UTF32BE:
    if (n < 4) return -1;
    if (cs == CS_UTF32LE) { d[0] = c; d[1] = c >> 8; d[2] = c >> 16; d[3] = c >> 24; }
    else { d[0] = c >> 24; d[1] = c >> 16; d[2] = c >> 8; d[3] = c; }
    return 4;
  default: {
    unsigned v = '?';
    if (n < 1) return -1;
    if (c < 0x80 || (c < 0x100 && cs != CS_ASCII && !(cs == CS_1252 && c < 0xA0))) v = c;
    else if (cs == CS_1252) { for (int i = 0; i < 32; i++) if (w1252[i] == c) v = 0x80 + i; }
    else if (cs == CS_LATIN9) {
      switch (c) { case 0x20AC: v = 0xA4; break; case 0x160: v = 0xA6; break; case 0x161: v = 0xA8; break;
                   case 0x17D: v = 0xB4; break; case 0x17E: v = 0xB8; break; case 0x152: v = 0xBC; break;
                   case 0x153: v = 0xBD; break; case 0x178: v = 0xBE; break; }
    }
    d[0] = v;
    return 1;
  }
  }
}
size_t iconv(iconv_t cd, char **ib, size_t *il, char **ob, size_t *ol) {
  int a = (intptr_t)cd >> 8, de = (intptr_t)cd & 255;
  size_t hechas = 0;
  if (!ib || !*ib) return 0;
  while (*il) {
    int k = 1;
    long c = lee_cs(de, (const unsigned char *)*ib, *il, &k);
    if (c == -1) { errno = EINVAL; return (size_t)-1; }
    if (c == -2) { errno = EILSEQ; return (size_t)-1; }
    int w = pon_cs(a, (unsigned long)c, (unsigned char *)*ob, *ol);
    if (w < 0) { errno = E2BIG; return (size_t)-1; }
    *ib += k; *il -= k; *ob += w; *ol -= w;
    hechas++;
  }
  (void)hechas;
  return 0;
}

/* ---- las divisiones de 64 bits que pide gcc (las de libgcc): restando y
 * desplazando, sin «/» de 64 bits (que llamaría a ellas mismas) ---- */
static uint64_t divmod_u64(uint64_t n, uint64_t d, uint64_t *r) {
  uint64_t q = 0;
  int k;
  if (!d) { *r = 0; return 0; }
  if (!(n >> 32) && !(d >> 32)) { *r = (uint32_t)n % (uint32_t)d; return (uint32_t)n / (uint32_t)d; }
  for (k = 63; k >= 0; k--) {
    if ((d << k) >> k != d) continue;          /* (d << k) se saldría */
    if ((d << k) <= n) { n -= d << k; q |= (uint64_t)1 << k; }
  }
  *r = n;
  return q;
}
uint64_t __udivdi3(uint64_t n, uint64_t d) { uint64_t r; return divmod_u64(n, d, &r); }
uint64_t __umoddi3(uint64_t n, uint64_t d) { uint64_t r; divmod_u64(n, d, &r); return r; }
int64_t __divdi3(int64_t n, int64_t d) {
  uint64_t r, q = divmod_u64(n < 0 ? -(uint64_t)n : (uint64_t)n, d < 0 ? -(uint64_t)d : (uint64_t)d, &r);
  return (n < 0) != (d < 0) ? -(int64_t)q : (int64_t)q;
}
int64_t __moddi3(int64_t n, int64_t d) {
  uint64_t r;
  divmod_u64(n < 0 ? -(uint64_t)n : (uint64_t)n, d < 0 ? -(uint64_t)d : (uint64_t)d, &r);
  return n < 0 ? -(int64_t)r : (int64_t)r;
}
