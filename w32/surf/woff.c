/* =============================================================================
 *  woff.c - las letras de la web (@font-face) para NAVEGA2.EXE: WOFF 1.0 y
 *  WOFF2 pasados a sfnt (TTF/OTF), que es lo que entiende stb_truetype.
 *
 *  - sfnt (TTF 0x00010000 / 'true' / OTF 'OTTO'): se devuelve una copia.
 *  - WOFF 1.0 (W3C, 2012): tablas sueltas, cada una con zlib o sin comprimir;
 *    el zlib es el de stb_image (imagen.c, surf_zlib_descomprime).
 *  - WOFF2 (W3C, 2018): todas las tablas juntas con Brotli (terceros/brotli,
 *    el descompresor oficial, MIT) y glyf/loca y hmtx «transformadas», que
 *    aquí se rehacen como dice la especificación (apartados 5.1 a 5.4).
 *    Las colecciones (ttcf) no.
 *
 *  El diccionario de Brotli (122.784 bytes) no va dentro del EXE: se lee de
 *  BROTLI.DIC (la carpeta RECURSOS, woff_carpeta; comprimido con zlib) solo
 *  mientras se descomprime un WOFF2 y se suelta enseguida. Todo se comprueba contra el tamaño de lo
 *  leído: un archivo roto o cortado da NULL, nunca se lee fuera.
 *  docs/486/NAVEGADOR2.md
 *  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * ============================================================================= */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <brotli/decode.h>
#include "woff.h"

/* terceros/brotli/c/common/dictionary.c (con BROTLI_EXTERNAL_DICTIONARY_DATA;
   NULL lo quita: cambio [HISPA]) */
void BrotliSetDictionaryData(const uint8_t *data);
/* imagen.c: el zlib de stb_image; da los bytes sacados o -1 */
int surf_zlib_descomprime(char *o, int ol, const char *i, int il);

#define MAXIMO (8u << 20)           /* ni una letra de más de 8 MB (486 con 16 MB) */
#define DIC_TAM 122784u             /* el diccionario de Brotli (RFC 7932, apéndice A) */
#define DIC_SUMA 0xCEF157D9u        /* su suma (dic_suma), para no usar uno roto */
#define ETQ(a, b, c, d) ((uint32_t)(a) << 24 | (uint32_t)(b) << 16 | (uint32_t)(c) << 8 | (uint32_t)(d))
#define T_GLYF ETQ('g', 'l', 'y', 'f')
#define T_LOCA ETQ('l', 'o', 'c', 'a')
#define T_HMTX ETQ('h', 'm', 't', 'x')
#define T_HHEA ETQ('h', 'h', 'e', 'a')
#define T_MAXP ETQ('m', 'a', 'x', 'p')
#define T_HEAD ETQ('h', 'e', 'a', 'd')

static uint32_t l16(const unsigned char *p) { return (uint32_t)p[0] << 8 | p[1]; }
static uint32_t l32(const unsigned char *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
static void e16(unsigned char *p, uint32_t v) { p[0] = (unsigned char)(v >> 8); p[1] = (unsigned char)v; }
static void e32(unsigned char *p, uint32_t v) { e16(p, v >> 16); e16(p + 2, v); }

/* ---------------------------------------------------------------- el sfnt */

/* una tabla del sfnt que se arma: sus datos (o NULL, si se rellenan luego en
   su sitio: los de zlib del WOFF 1.0) y dónde queda */
typedef struct {
  uint32_t etq, lon, pos;
  const unsigned char *dat;
  const unsigned char *z;           /* WOFF 1.0: lo comprimido con zlib */
  uint32_t zlon;
} tsal;

static int cmp_etq(const void *a, const void *b) {
  uint32_t x = ((const tsal *)a)->etq, y = ((const tsal *)b)->etq;
  return x < y ? -1 : x > y;
}

/* ordena las tablas por etiqueta (el sfnt las quiere así, para buscarlas),
   les da sitio (cada una alineada a 4) y copia las que traen datos; los
   huecos y el relleno quedan a cero */
static unsigned char *sfnt_reserva(tsal *t, unsigned nt, size_t *total) {
  size_t p = 12 + 16 * (size_t)nt;
  unsigned i;
  unsigned char *o;
  qsort(t, nt, sizeof *t, cmp_etq);
  for (i = 0; i < nt; i++) {
    if (i && t[i].etq == t[i - 1].etq) return NULL;   /* tabla repetida */
    if (t[i].lon > MAXIMO || p > MAXIMO - t[i].lon) return NULL;
    t[i].pos = (uint32_t)p;
    p = (p + t[i].lon + 3) & ~(size_t)3;
  }
  if (p > MAXIMO) return NULL;
  o = calloc(1, p);
  if (!o) return NULL;
  for (i = 0; i < nt; i++)
    if (t[i].dat && t[i].lon) memcpy(o + t[i].pos, t[i].dat, t[i].lon);
  *total = p;
  return o;
}

static uint32_t suma32(const unsigned char *p, size_t n) {   /* n múltiplo de 4 */
  uint32_t s = 0;
  size_t i;
  for (i = 0; i < n; i += 4) s += l32(p + i);
  return s;
}

/* la cabecera y el directorio del sfnt, con sus sumas de comprobación (y la
   de head, checkSumAdjustment, como manda OpenType) */
static void sfnt_cierra(unsigned char *o, size_t total, uint32_t sabor, const tsal *t, unsigned nt) {
  unsigned i, es = 0;
  unsigned char *head = NULL;
  while ((2u << es) <= nt) es++;
  e32(o, sabor);
  e16(o + 4, nt);
  e16(o + 6, 16u << es);
  e16(o + 8, es);
  e16(o + 10, nt * 16 - (16u << es));
  for (i = 0; i < nt; i++) {
    unsigned char *r = o + 12 + 16 * i;
    if (t[i].etq == T_HEAD && t[i].lon >= 12) { head = o + t[i].pos; e32(head + 8, 0); }
    e32(r, t[i].etq);
    e32(r + 4, suma32(o + t[i].pos, (t[i].lon + 3) & ~3u));
    e32(r + 8, t[i].pos);
    e32(r + 12, t[i].lon);
  }
  if (head) e32(head + 8, 0xB1B0AFBAu - suma32(o, total));
}

/* ---------------------------------------------------------------- WOFF 1.0 */

static unsigned char *de_woff1(const unsigned char *d, size_t n, size_t *sale) {
  unsigned nt, i;
  tsal *t;
  unsigned char *o = NULL;
  size_t total = 0;
  if (n < 44) return NULL;
  nt = l16(d + 12);
  if (!nt || 44 + 20 * (size_t)nt > n) return NULL;
  t = calloc(nt, sizeof *t);
  if (!t) return NULL;
  for (i = 0; i < nt; i++) {
    const unsigned char *e = d + 44 + 20 * i;
    uint32_t pos = l32(e + 4), cl = l32(e + 8), ol = l32(e + 12);
    if (pos > n || cl > n - pos || ol > MAXIMO || cl > ol) goto mal;
    t[i].etq = l32(e);
    t[i].lon = ol;
    if (cl == ol) t[i].dat = d + pos;
    else { t[i].z = d + pos; t[i].zlon = cl; }
  }
  o = sfnt_reserva(t, nt, &total);
  if (!o) goto mal;
  for (i = 0; i < nt; i++)
    if (t[i].z && surf_zlib_descomprime((char *)o + t[i].pos, (int)t[i].lon, (const char *)t[i].z, (int)t[i].zlon) != (int)t[i].lon)
      goto mal;
  sfnt_cierra(o, total, l32(d + 4), t, nt);
  free(t);
  *sale = total;
  return o;
mal:
  free(o);
  free(t);
  return NULL;
}

/* ---------------------------------------------------------------- WOFF2 */

/* las 63 etiquetas que se dicen con un número (apartado 4.1, tabla de tags);
 * [5]: con el 0 del final, que no se usa (GCC 15 no deja recortarlo) */
static const char etiquetas[63][5] = {
  "cmap", "head", "hhea", "hmtx", "maxp", "name", "OS/2", "post", "cvt ", "fpgm", "glyf", "loca", "prep",
  "CFF ", "VORG", "EBDT", "EBLC", "gasp", "hdmx", "kern", "LTSH", "PCLT", "VDMX", "vhea", "vmtx", "BASE",
  "GDEF", "GPOS", "GSUB", "EBSC", "JSTF", "MATH", "CBDT", "CBLC", "COLR", "CPAL", "SVG ", "sbix", "acnt",
  "avar", "bdat", "bloc", "bsln", "cvar", "fdsc", "feat", "fmtx", "fvar", "gvar", "hsty", "just", "lcar",
  "mort", "morx", "opbd", "prop", "trak", "Zapf", "Silf", "Glat", "Gloc", "Feat", "Sill"};

/* un trozo de lo descomprimido, que se lee sin salirse */
typedef struct { const unsigned char *p; size_t n, i; } flujo;

static int f8(flujo *f, uint32_t *v) {
  if (f->i >= f->n) return 0;
  *v = f->p[f->i++];
  return 1;
}
static int f16(flujo *f, uint32_t *v) {
  if (f->n - f->i < 2 || f->i > f->n) return 0;
  *v = l16(f->p + f->i);
  f->i += 2;
  return 1;
}
static int f32(flujo *f, uint32_t *v) {
  if (f->n - f->i < 4 || f->i > f->n) return 0;
  *v = l32(f->p + f->i);
  f->i += 4;
  return 1;
}
static const unsigned char *fbytes(flujo *f, size_t k) {   /* k bytes seguidos */
  const unsigned char *r;
  if (f->i > f->n || f->n - f->i < k) return NULL;
  r = f->p + f->i;
  f->i += k;
  return r;
}
/* UIntBase128 (apartado 4.2): hasta 5 bytes, sin ceros delante, sin pasar de 32 bits */
static int fbase128(flujo *f, uint32_t *v) {
  uint32_t r = 0, b;
  int k;
  for (k = 0; k < 5; k++) {
    if (!f8(f, &b) || (k == 0 && b == 0x80) || (r & 0xFE000000u)) return 0;
    r = r << 7 | (b & 0x7F);
    if (!(b & 0x80)) { *v = r; return 1; }
  }
  return 0;
}
/* 255UInt16 (apartado 4.3) */
static int f255(flujo *f, uint32_t *v) {
  uint32_t c, b;
  if (!f8(f, &c)) return 0;
  if (c == 253) return f16(f, v);
  if (c == 254 || c == 255) {
    if (!f8(f, &b)) return 0;
    *v = b + (c == 255 ? 253 : 506);
    return 1;
  }
  *v = c;
  return 1;
}

/* lo que se va escribiendo (la glyf rehecha), que crece si hace falta */
typedef struct { unsigned char *p; size_t n, cap; } buf;

static int b_sitio(buf *b, size_t mas) {
  size_t c;
  unsigned char *q;
  if (mas > MAXIMO || b->n > MAXIMO - mas) return 0;
  if (b->n + mas <= b->cap) return 1;
  c = b->n + mas;                   /* (poco de más: la memoria va justa) */
  c += c / 16 + 4096;
  q = realloc(b->p, c);
  if (!q) return 0;
  b->p = q;
  b->cap = c;
  return 1;
}

/* el punto siguiente de un glifo simple (apartado 5.2, la tabla de las 128
   formas de un «triplete»): dx y dy relativos al anterior */
static int triplete(uint32_t fl, flujo *g, int32_t *dx, int32_t *dy) {
  const unsigned char *b;
  uint32_t b0;
  int k = fl < 84 ? 1 : fl < 120 ? 2 : fl < 124 ? 3 : 4;
  int32_t x, y;
  if (!(b = fbytes(g, k))) return 0;
  if (fl < 10) { x = 0; y = (int32_t)(((fl & 14) << 7) + b[0]); }
  else if (fl < 20) { x = (int32_t)((((fl - 10) & 14) << 7) + b[0]); y = 0; }
  else if (fl < 84) {
    b0 = fl - 20;
    x = (int32_t)(1 + (b0 & 0x30) + (b[0] >> 4));
    y = (int32_t)(1 + ((b0 & 0x0C) << 2) + (b[0] & 0x0F));
  } else if (fl < 120) {
    b0 = fl - 84;
    x = (int32_t)(1 + ((b0 / 12) << 8) + b[0]);
    y = (int32_t)(1 + (((b0 % 12) >> 2) << 8) + b[1]);
  } else if (fl < 124) {
    x = (int32_t)((b[0] << 4) + (b[1] >> 4));
    y = (int32_t)(((b[1] & 0x0F) << 8) + b[2]);
  } else {
    x = (int32_t)((b[0] << 8) + b[1]);
    y = (int32_t)((b[2] << 8) + b[3]);
  }
  /* el signo: el bit 0 para la x y el bit 1 para la y; si solo se mueve una
     (banderas 0 a 19), su signo va en el bit 0 */
  *dx = (fl & 1) ? x : -x;
  *dy = ((fl < 20 ? fl : fl >> 1) & 1) ? y : -y;
  return 1;
}

/* glyf/loca transformadas (apartado 5.1) a glyf y loca de siempre. Deja en
   xmin[] el xMin de cada glifo (para hmtx) */
typedef struct { int32_t x, y; unsigned char on; } punto;

static int rehace_glyf(const unsigned char *g, size_t gn, uint32_t ori, buf *sg, unsigned char **loca, uint32_t *loca_lon,
                       int *fmt, unsigned *ngl, int16_t **xmin) {
  flujo f = {g, gn, 0}, nc_f, np_f, fl_f, gl_f, co_f, bb_f, in_f;
  const unsigned char *bbmapa, *solape = NULL;
  uint32_t opc, ng, ifmt, tam[7], v;
  size_t pos;
  uint32_t *off = NULL;
  punto *pt = NULL;
  size_t ptcap = 0;
  uint16_t *fin = NULL;              /* endPtsOfContours */
  size_t fincap = 0;
  int16_t *xm = NULL;
  unsigned i, k;
  int ok = 0;

  if (!f16(&f, &v) || !f16(&f, &opc) || !f16(&f, &ng) || !f16(&f, &ifmt)) return 0;
  if (ifmt > 1 || !ng) return 0;
  for (k = 0; k < 7; k++) if (!f32(&f, &tam[k])) return 0;
  pos = f.i;
  {
    flujo *fs[7] = {&nc_f, &np_f, &fl_f, &gl_f, &co_f, &bb_f, &in_f};
    for (k = 0; k < 7; k++) {
      if (tam[k] > gn - pos) return 0;
      fs[k]->p = g + pos; fs[k]->n = tam[k]; fs[k]->i = 0;
      pos += tam[k];
    }
  }
  if (opc & 1) {                     /* overlapSimpleBitmap */
    if ((ng + 7) / 8 > gn - pos) return 0;
    solape = g + pos;
  }
  bbmapa = fbytes(&bb_f, 4 * ((ng + 31) / 32));
  if (!bbmapa) return 0;

  off = malloc(((size_t)ng + 1) * sizeof *off);
  xm = calloc(ng, sizeof *xm);
  if (!off || !xm) goto fuera;
  sg->n = 0;
  if (!b_sitio(sg, ori < MAXIMO ? ori + 64 : 64)) goto fuera;

  for (i = 0; i < ng; i++) {
    uint32_t nc;
    int con_caja = (bbmapa[i >> 3] >> (7 - (i & 7))) & 1;
    int32_t caja[4] = {0, 0, 0, 0};
    off[i] = (uint32_t)sg->n;
    if (!f16(&nc_f, &nc)) goto fuera;
    if (con_caja) {
      const unsigned char *c = fbytes(&bb_f, 8);
      if (!c) goto fuera;
      for (k = 0; k < 4; k++) caja[k] = (int16_t)l16(c + 2 * k);
    }
    if (nc == 0) {                   /* vacío */
      if (con_caja) goto fuera;
    } else if (nc == 0xFFFF) {       /* compuesto: los trozos van tal cual */
      size_t ini = co_f.i, clon;
      uint32_t cf, ilon = 0;
      int instr = 0;
      unsigned char *o;
      const unsigned char *ib = NULL;
      if (!con_caja) goto fuera;
      do {
        size_t k2;
        if (!f16(&co_f, &cf)) goto fuera;
        k2 = 2 + ((cf & 1) ? 4 : 2) + ((cf & 8) ? 2 : (cf & 0x40) ? 4 : (cf & 0x80) ? 8 : 0);
        if (!fbytes(&co_f, k2)) goto fuera;
        if (cf & 0x100) instr = 1;
      } while (cf & 0x20);
      clon = co_f.i - ini;
      if (instr) {
        if (!f255(&gl_f, &ilon) || !(ib = fbytes(&in_f, ilon))) goto fuera;
      }
      if (!b_sitio(sg, 10 + clon + 2 + ilon + 3)) goto fuera;
      o = sg->p + sg->n;
      e16(o, 0xFFFF);
      for (k = 0; k < 4; k++) e16(o + 2 + 2 * k, (uint32_t)caja[k]);
      memcpy(o + 10, co_f.p + ini, clon);
      o += 10 + clon;
      if (instr) { e16(o, ilon); memcpy(o + 2, ib, ilon); o += 2 + ilon; }
      sg->n = (size_t)(o - sg->p);
    } else if (nc < 0x8000) {        /* simple */
      size_t np = 0, q;
      uint32_t ilon, c, pf = 0, rep = 0, flant = 0;
      int32_t x = 0, y = 0, ax, ay;
      const unsigned char *ib;
      unsigned char *o;
      if (nc > fincap) {
        uint16_t *nf = realloc(fin, nc * sizeof *fin);
        if (!nf) goto fuera;
        fin = nf; fincap = nc;
      }
      for (k = 0; k < nc; k++) {
        if (!f255(&np_f, &c)) goto fuera;
        np += c;
        if (np > 65536) goto fuera;
        fin[k] = (uint16_t)(np - 1);
      }
      if (!np) goto fuera;           /* contornos sin puntos: no */
      if (np > ptcap) {
        punto *npt = realloc(pt, np * sizeof *pt);
        if (!npt) goto fuera;
        pt = npt; ptcap = np;
      }
      for (q = 0; q < np; q++) {
        uint32_t fl;
        int32_t dx, dy;
        if (!f8(&fl_f, &fl) || !triplete(fl & 0x7F, &gl_f, &dx, &dy)) goto fuera;
        x += dx; y += dy;
        pt[q].x = x; pt[q].y = y; pt[q].on = !(fl & 0x80);
      }
      if (!f255(&gl_f, &ilon) || !(ib = fbytes(&in_f, ilon))) goto fuera;
      if (!con_caja && np) {         /* la caja, de los puntos */
        caja[0] = caja[2] = pt[0].x; caja[1] = caja[3] = pt[0].y;
        for (q = 1; q < np; q++) {
          if (pt[q].x < caja[0]) caja[0] = pt[q].x;
          if (pt[q].x > caja[2]) caja[2] = pt[q].x;
          if (pt[q].y < caja[1]) caja[1] = pt[q].y;
          if (pt[q].y > caja[3]) caja[3] = pt[q].y;
        }
      }
      if (!b_sitio(sg, 10 + 2 * (size_t)nc + 2 + ilon + 5 * np + 3)) goto fuera;
      o = sg->p + sg->n;
      e16(o, nc);
      for (k = 0; k < 4; k++) e16(o + 2 + 2 * k, (uint32_t)caja[k]);
      o += 10;
      for (k = 0; k < nc; k++, o += 2) e16(o, fin[k]);
      e16(o, ilon); memcpy(o + 2, ib, ilon); o += 2 + ilon;
      /* las banderas (con las repeticiones), luego las x y luego las y */
      ax = ay = 0;
      for (q = 0; q < np; q++) {
        int32_t dx = pt[q].x - ax, dy = pt[q].y - ay;
        uint32_t fl = pt[q].on;
        ax = pt[q].x; ay = pt[q].y;
        if (dx == 0) fl |= 0x10;
        else if (dx >= -255 && dx <= 255) fl |= 0x02 | (dx > 0 ? 0x10 : 0);
        if (dy == 0) fl |= 0x20;
        else if (dy >= -255 && dy <= 255) fl |= 0x04 | (dy > 0 ? 0x20 : 0);
        if (q == 0 && solape && ((solape[i >> 3] >> (7 - (i & 7))) & 1)) fl |= 0x40;
        if (q && fl == flant && rep < 255) {
          if (!rep) { sg->p[pf] |= 0x08; *o++ = 1; }
          else o[-1]++;
          rep++;
        } else {
          pf = (uint32_t)(o - sg->p);
          *o++ = (unsigned char)fl;
          flant = fl;
          rep = 0;
        }
      }
      for (k = 0; k < 2; k++) {
        int32_t ant = 0;
        for (q = 0; q < np; q++) {
          int32_t c2 = k ? pt[q].y : pt[q].x, dd = c2 - ant;
          ant = c2;
          if (dd == 0) continue;
          if (dd >= -255 && dd <= 255) *o++ = (unsigned char)(dd < 0 ? -dd : dd);
          else { e16(o, (uint32_t)dd); o += 2; }
        }
      }
      sg->n = (size_t)(o - sg->p);
    } else goto fuera;               /* otros números negativos: no */
    xm[i] = (int16_t)caja[0];
    while (sg->n & 3) sg->p[sg->n++] = 0;   /* cada glifo alineado a 4 (b_sitio dejó +3) */
  }
  off[ng] = (uint32_t)sg->n;

  /* loca: corta (offset/2 en 16 bits) si se pidió y cabe; si no, larga */
  if (ifmt == 0 && off[ng] / 2 > 0xFFFF) ifmt = 1;
  *loca_lon = (ng + 1) * (ifmt ? 4 : 2);
  *loca = malloc(*loca_lon);
  if (!*loca) goto fuera;
  for (i = 0; i <= ng; i++) {
    if (ifmt) e32(*loca + 4 * i, off[i]);
    else e16(*loca + 2 * i, off[i] / 2);
  }
  *fmt = (int)ifmt;
  *ngl = ng;
  *xmin = xm;
  xm = NULL;
  ok = 1;
fuera:
  free(off);
  free(pt);
  free(fin);
  free(xm);
  return ok;
}

/* hmtx transformada (apartado 5.4): los lsb que faltan son el xMin del glifo */
static unsigned char *rehace_hmtx(const unsigned char *h, size_t hn, unsigned ng, unsigned nhm, const int16_t *xm, uint32_t *lon) {
  flujo f = {h, hn, 0};
  uint32_t fl, v;
  const unsigned char *av, *lp = NULL, *lm = NULL;
  unsigned char *o;
  unsigned i;
  if (!nhm || nhm > ng || !f8(&f, &fl) || (fl & 0xFC)) return NULL;
  if (!(av = fbytes(&f, 2 * (size_t)nhm))) return NULL;
  if (!(fl & 1) && !(lp = fbytes(&f, 2 * (size_t)nhm))) return NULL;
  if (!(fl & 2) && !(lm = fbytes(&f, 2 * (size_t)(ng - nhm)))) return NULL;
  *lon = 4 * nhm + 2 * (ng - nhm);
  o = malloc(*lon);
  if (!o) return NULL;
  for (i = 0; i < nhm; i++) {
    v = lp ? l16(lp + 2 * i) : (uint16_t)xm[i];
    e16(o + 4 * i, l16(av + 2 * i));
    e16(o + 4 * i + 2, v);
  }
  for (i = nhm; i < ng; i++)
    e16(o + 4 * nhm + 2 * (i - nhm), lm ? l16(lm + 2 * (i - nhm)) : (uint16_t)xm[i]);
  return o;
}

static char carpeta[200];

void woff_carpeta(const char *dir) {
  size_t l = dir ? strlen(dir) : 0;
  carpeta[0] = 0;
  if (l && l < sizeof carpeta) memcpy(carpeta, dir, l + 1);
}

static uint32_t dic_suma(const unsigned char *p) {
  uint32_t s = 0, i;
  for (i = 0; i < DIC_TAM; i++) s = (s << 5 | s >> 27) + p[i];
  return s;
}

/* BROTLI.DIC, de la carpeta de woff_carpeta: el diccionario comprimido con
   zlib (58 KB en el disquete en vez de 120; tools/brotli_dic.py). NULL si no
   está o no es él */
#define DIC_ZMAX 65536u
static unsigned char *carga_dic(void) {
  char r[sizeof carpeta + 16];
  size_t l = strlen(carpeta), k = 0;
  FILE *f;
  unsigned char *z, *p = NULL;
  memcpy(r, carpeta, l);
  if (l && r[l - 1] != '\\' && r[l - 1] != '/' && r[l - 1] != ':') r[l++] = strchr(carpeta, '/') ? '/' : '\\';
  memcpy(r + l, "BROTLI.DIC", 11);
  f = fopen(r, "rb");
  if (!f) return NULL;
  z = malloc(DIC_ZMAX);
  if (z) k = fread(z, 1, DIC_ZMAX, f);
  fclose(f);
  if (z && k && k < DIC_ZMAX && (p = malloc(DIC_TAM))) {
    if (surf_zlib_descomprime((char *)p, (int)DIC_TAM, (const char *)z, (int)k) != (int)DIC_TAM || dic_suma(p) != DIC_SUMA) {
      free(p);
      p = NULL;
    }
  }
  free(z);
  return p;
}

typedef struct { uint32_t etq, ori, tlon, pos; int transf; } t2;

static unsigned char *de_woff2(const unsigned char *d, size_t n, size_t *sale) {
  flujo f = {d, n, 48};
  uint32_t sabor, nt, comp, total = 0, v;
  t2 *t = NULL;
  tsal *ts = NULL;
  unsigned char *u = NULL, *dic, *loca = NULL, *hm = NULL, *o = NULL;
  buf sg = {NULL, 0, 0};
  int16_t *xm = NULL;
  int ig = -1, il = -1, ih = -1, ihh = -1, imx = -1, ihd = -1, fmt = 0;
  unsigned i, ng = 0;
  uint32_t loca_lon = 0, hm_lon = 0;
  size_t un, tam = 0;

  if (n < 48) return NULL;
  sabor = l32(d + 4);
  nt = l16(d + 12);
  comp = l32(d + 20);
  if (sabor == ETQ('t', 't', 'c', 'f') || !nt) return NULL;
  t = calloc(nt, sizeof *t);
  if (!t) return NULL;
  for (i = 0; i < nt; i++) {
    uint32_t fl, ver;
    if (!f8(&f, &fl)) goto mal;
    if ((fl & 63) == 63) { if (!f32(&f, &t[i].etq)) goto mal; }
    else t[i].etq = l32((const unsigned char *)etiquetas[fl & 63]);
    ver = fl >> 6;
    if (!fbase128(&f, &t[i].ori) || t[i].ori > MAXIMO) goto mal;
    if (t[i].etq == T_GLYF || t[i].etq == T_LOCA) {
      if (ver == 1 || ver == 2) goto mal;
      t[i].transf = ver == 0;
    } else if (t[i].etq == T_HMTX) {
      if (ver > 1) goto mal;
      t[i].transf = ver == 1;
    } else if (ver) goto mal;
    t[i].tlon = t[i].ori;
    if (t[i].transf && !fbase128(&f, &t[i].tlon)) goto mal;
    if (t[i].tlon > MAXIMO || total > MAXIMO - t[i].tlon) goto mal;
    if (t[i].etq == T_LOCA && t[i].transf && t[i].tlon) goto mal;
    t[i].pos = total;
    total += t[i].tlon;
    if (t[i].etq == T_GLYF) ig = (int)i;
    if (t[i].etq == T_LOCA) il = (int)i;
    if (t[i].etq == T_HMTX) ih = (int)i;
    if (t[i].etq == T_HHEA) ihh = (int)i;
    if (t[i].etq == T_MAXP) imx = (int)i;
    if (t[i].etq == T_HEAD) ihd = (int)i;
  }
  if (!total || comp > n - f.i) goto mal;
  /* glyf y loca, transformadas las dos o ninguna; hmtx, solo con glyf */
  if ((ig >= 0) != (il >= 0) || (ig >= 0 && t[ig].transf != t[il].transf)) goto mal;
  if (ih >= 0 && t[ih].transf && (ig < 0 || !t[ig].transf || ihh < 0 || imx < 0)) goto mal;

  /* Brotli, con el diccionario solo mientras tanto (sin él puede salir bien si
     el archivo no lo usa; si lo usa, Brotli da error) */
  u = malloc(total);
  if (!u) goto mal;
  dic = carga_dic();
  BrotliSetDictionaryData(dic);
  un = total;
  v = BrotliDecoderDecompress(comp, d + f.i, &un, u) == BROTLI_DECODER_RESULT_SUCCESS && un == total;
  BrotliSetDictionaryData(NULL);
  free(dic);
  if (!v) goto mal;

  if (ig >= 0 && t[ig].transf) {
    if (!rehace_glyf(u + t[ig].pos, t[ig].tlon, t[ig].ori, &sg, &loca, &loca_lon, &fmt, &ng, &xm)) goto mal;
    if (imx >= 0 && (t[imx].tlon < 6 || l16(u + t[imx].pos + 4) != ng)) goto mal;
    if (ihd < 0 || t[ihd].tlon < 54) goto mal;
    e16(u + t[ihd].pos + 50, (uint32_t)fmt);   /* indexToLocFormat, la que salió */
  }
  if (ih >= 0 && t[ih].transf) {
    if (t[ihh].tlon < 36) goto mal;
    hm = rehace_hmtx(u + t[ih].pos, t[ih].tlon, ng, l16(u + t[ihh].pos + 34), xm, &hm_lon);
    if (!hm) goto mal;
  }
  free(xm);
  xm = NULL;

  ts = calloc(nt, sizeof *ts);
  if (!ts) goto mal;
  for (i = 0; i < nt; i++) {
    ts[i].etq = t[i].etq;
    if ((int)i == ig && t[i].transf) { ts[i].dat = sg.p; ts[i].lon = (uint32_t)sg.n; }
    else if ((int)i == il && t[i].transf) { ts[i].dat = loca; ts[i].lon = loca_lon; }
    else if ((int)i == ih && t[i].transf) { ts[i].dat = hm; ts[i].lon = hm_lon; }
    else { ts[i].dat = u + t[i].pos; ts[i].lon = t[i].tlon; }
  }
  o = sfnt_reserva(ts, nt, &tam);
  if (!o) goto mal;
  sfnt_cierra(o, tam, sabor, ts, nt);
  *sale = tam;
mal:
  free(t);
  free(ts);
  free(u);
  free(sg.p);
  free(loca);
  free(hm);
  free(xm);
  return o;
}

/* ---------------------------------------------------------------- la puerta */

unsigned char *woff_a_sfnt(const unsigned char *d, size_t n, size_t *sale) {
  uint32_t s;
  unsigned char *o;
  size_t dummy;
  if (!sale) sale = &dummy;
  *sale = 0;
  if (!d || n < 12) return NULL;
  s = l32(d);
  if (s == ETQ('w', 'O', 'F', 'F')) return de_woff1(d, n, sale);
  if (s == ETQ('w', 'O', 'F', '2')) return de_woff2(d, n, sale);
  if (s != 0x00010000u && s != ETQ('t', 'r', 'u', 'e') && s != ETQ('O', 'T', 'T', 'O')) return NULL;
  if (n > MAXIMO || !(o = malloc(n))) return NULL;
  memcpy(o, d, n);
  *sale = n;
  return o;
}
