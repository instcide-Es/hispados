/* =============================================================================
 *  descarga.c - NAVEGA.BIN, el Navegador de Hispa-Pentium: las descargas
 *  (gui_download_table de NetSurf). Lo que no se puede enseñar (un .ZIP, un
 *  .PDF, lo que el servidor manda como «attachment») o lo que se pide con
 *  «Descargar el enlace» va a la carpeta de las descargas (C:\DESCARGA, o la
 *  de «Descargas=» en NAVEGA.INI), con un nombre de 8+3 sacado del suyo
 *  (el disco no sabe hacer nombres largos): «Informe anual 2026.pdf» queda
 *  INFORMEA.PDF, y si ya hay uno, INFORM~1.PDF... La línea de estado dice lo
 *  que lleva y, al acabar, dónde está.
 *  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * ============================================================================= */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "utils/errors.h"
#include "utils/nsurl.h"
#include "utils/utf8.h"
#include "netsurf/download.h"
#include "desktop/download.h"

#include "pentium.h"
#include "frontal.h"

int __attribute__((stdcall)) CreateDirectoryA(const char *, void *);
uint32_t __attribute__((stdcall)) GetFileAttributesA(const char *);
int __attribute__((stdcall)) DeleteFileA(const char *);

struct gui_download_window {
  FILE *f;
  char ruta[300];
  char nombre[16];
  unsigned long long total, hecho;
  uint32_t t_ult;
};

/* una letra que vale en un nombre 8+3 (sin tilde; en mayúsculas), o 0 */
static int letra_83(uint32_t c)
{
  static const char *const tildes[] = { "\xc3\xa1" "A", "\xc3\xa0" "A", "\xc3\xa2" "A", "\xc3\xa4" "A", "\xc3\xa9" "E", "\xc3\xa8" "E", "\xc3\xaa" "E",
    "\xc3\xab" "E", "\xc3\xad" "I", "\xc3\xac" "I", "\xc3\xaf" "I", "\xc3\xb3" "O", "\xc3\xb2" "O", "\xc3\xb4" "O", "\xc3\xb6" "O", "\xc3\xba" "U",
    "\xc3\xb9" "U", "\xc3\xbc" "U", "\xc3\xb1" "N", "\xc3\xa7" "C", NULL };
  int k;
  if (c >= 'a' && c <= 'z') return (int)(c - 32);
  if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_') return (int)c;
  if (c >= 0xC0 && c < 0x100) {
    char u[4];
    int n;
    c |= 0x20;                  /* (las mayúsculas acentuadas, como las minúsculas) */
    u[0] = (char)(0xC0 | c >> 6); u[1] = (char)(0x80 | (c & 63)); n = 2;
    for (k = 0; tildes[k]; k++) if (!memcmp(tildes[k], u, n)) return tildes[k][2];
  }
  return 0;
}
/* el nombre 8+3 de «largo» (UTF-8): BASE (hasta 8) y EXT (hasta 3) */
static void nombre_83(const char *largo, char *base, char *ext)
{
  const char *p = strrchr(largo, '/'), *punto;
  size_t n, k;
  int nb = 0, ne = 0;
  if (p) largo = p + 1;
  if ((p = strrchr(largo, '\\'))) largo = p + 1;
  n = strlen(largo);
  punto = strrchr(largo, '.');
  for (k = 0; k < n && nb < 8;) {
    uint32_t c = utf8_to_ucs4(largo + k, n - k);
    int l;
    if (punto && largo + k >= punto) break;
    k = utf8_next(largo, n, k);
    if ((l = letra_83(c))) base[nb++] = (char)l;
  }
  if (punto) {
    size_t m = strlen(punto + 1);
    for (k = 0; k < m && ne < 3;) {
      uint32_t c = utf8_to_ucs4(punto + 1 + k, m - k);
      int l;
      k = utf8_next(punto + 1, m, k);
      if ((l = letra_83(c))) ext[ne++] = (char)l;
    }
  }
  if (!nb) { memcpy(base, "DESCARGA", 8); nb = 8; }
  base[nb] = 0;
  ext[ne] = 0;
}
static void kb(char *d, size_t cabe, unsigned long long n)
{
  if (n < 1024 * 1024) snprintf(d, cabe, "%u KB", (unsigned)((n + 1023) / 1024));
  else snprintf(d, cabe, "%u,%u MB", (unsigned)(n >> 20), (unsigned)((n & 0xFFFFF) * 10 >> 20));
}

static struct gui_download_window *d_crea(struct download_context *ctx, struct gui_window *padre)
{
  struct gui_download_window *d = calloc(1, sizeof *d);
  const char *n = download_context_get_filename(ctx);
  char base[12], ext[4], t[400];
  int k;
  (void)padre;
  if (!d) return NULL;
  nombre_83(n ? n : "descarga", base, ext);
  CreateDirectoryA(frontal_descargas, NULL);
  /* si ya hay uno con ese nombre: NOMBR~1, NOMBR~2... */
  for (k = 0; k < 100; k++) {
    char b[12];
    if (!k) snprintf(b, sizeof b, "%s", base);
    else {
      char num[8];
      int ln = snprintf(num, sizeof num, "~%d", k), lb = (int)strlen(base);
      if (lb > 8 - ln) lb = 8 - ln;
      snprintf(b, sizeof b, "%.*s%s", lb, base, num);
    }
    snprintf(d->nombre, sizeof d->nombre, "%s%s%s", b, ext[0] ? "." : "", ext);
    snprintf(d->ruta, sizeof d->ruta, "%s\\%s", frontal_descargas, d->nombre);
    if (GetFileAttributesA(d->ruta) == 0xFFFFFFFFu) break;
  }
  d->f = fopen(d->ruta, "wb");
  if (!d->f) {
    snprintf(t, sizeof t, "No se puede guardar la descarga en %s.", d->ruta);
    frontal_aviso_estado(t);
    pent_serie("navega: descarga: no se puede crear %s", d->ruta);
    free(d);
    return NULL;
  }
  d->total = download_context_get_total_length(ctx);
  d->t_ult = pent_tics();
  snprintf(t, sizeof t, "Descargando %s en %s\xe2\x80\xa6", n ? n : d->nombre, d->ruta);
  frontal_aviso_estado(t);
  pent_serie("navega: descarga: %s -> %s", n ? n : "", d->ruta);
  return d;
}
static nserror d_datos(struct gui_download_window *d, const char *datos, unsigned int n)
{
  if (fwrite(datos, 1, n, d->f) != n) return NSERROR_SAVE_FAILED;
  d->hecho += n;
  if ((int)(pent_tics() - d->t_ult) > (int)pent_hz() / 2) {
    char t[300], a[32], b[32];
    d->t_ult = pent_tics();
    kb(a, sizeof a, d->hecho);
    if (d->total) { kb(b, sizeof b, d->total); snprintf(t, sizeof t, "Descargando %s: %s de %s", d->nombre, a, b); }
    else snprintf(t, sizeof t, "Descargando %s: %s", d->nombre, a);
    frontal_aviso_estado(t);
  }
  return NSERROR_OK;
}
static void d_error(struct gui_download_window *d, const char *m)
{
  char t[400];
  if (d->f) fclose(d->f);
  d->f = NULL;
  DeleteFileA(d->ruta);
  snprintf(t, sizeof t, "No se ha podido descargar %s: %s", d->nombre, m ? m : "");
  frontal_aviso_estado(t);
  pent_serie("navega: descarga: error %s", m ? m : "");
}
static void d_hecha(struct gui_download_window *d)
{
  char t[400], a[32];
  if (d->f) {
    fclose(d->f);
    kb(a, sizeof a, d->hecho);
    snprintf(t, sizeof t, "Descargado: %s (%s)", d->ruta, a);
    frontal_aviso_estado(t);
    pent_serie("navega: descarga: hecha %s, %u octetos", d->ruta, (unsigned)d->hecho);
  }
  free(d);
}
static struct gui_download_table tabla = { .create = d_crea, .data = d_datos, .error = d_error, .done = d_hecha };
struct gui_download_table *frontal_tabla_descargas = &tabla;
