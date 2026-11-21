/* =============================================================================
 *  prueba.c - prueba de w32/surf/woff.c en el anfitrión (tests/run_surf_woff_tests.sh)
 *    prueba compara ORIGINAL.TTF LETRA.(woff|woff2|ttf)  mismos glifos que el TTF
 *    prueba rotos LETRA                                   cortada/estropeada: NULL o algo, sin colgarse
 *    prueba glifos LETRA.woff2                            glyf/loca/hmtx transformadas estropeadas
 *    prueba info LETRA [U+XXXX ...] [-o SALIDA.TTF]       glifos, cmap, memoria
 *  La memoria de woff.c y Brotli se cuenta con cuenta.h (-include al compilarlos).
 *  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * ============================================================================= */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "woff.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#include "stb_image.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include <brotli/decode.h>
void BrotliSetDictionaryData(const unsigned char *data);

/* lo de imagen.c */
int surf_zlib_descomprime(char *o, int ol, const char *i, int il) { return stbi_zlib_decode_buffer(o, ol, i, il); }

/* la cuenta de memoria (cuenta.h) */
static size_t en_uso, pico;
void *cuenta_malloc(size_t n) {
  size_t *p = malloc(n + 16);
  if (!p) return NULL;
  *p = n; en_uso += n; if (en_uso > pico) pico = en_uso;
  return (char *)p + 16;
}
void cuenta_free(void *q) {
  size_t *p;
  if (!q) return;
  p = (size_t *)((char *)q - 16);
  en_uso -= *p;
  free(p);
}
void *cuenta_calloc(size_t a, size_t b) {
  void *q;
  if (b && a > (size_t)-1 / b) return NULL;
  q = cuenta_malloc(a * b);
  if (q) memset(q, 0, a * b);
  return q;
}
void *cuenta_realloc(void *q, size_t n) {
  void *r;
  size_t v;
  if (!q) return cuenta_malloc(n);
  v = *(size_t *)((char *)q - 16);
  r = cuenta_malloc(n);
  if (!r) return NULL;
  memcpy(r, q, v < n ? v : n);
  cuenta_free(q);
  return r;
}

static unsigned char *lee(const char *nom, size_t *n) {
  FILE *f = fopen(nom, "rb");
  unsigned char *p;
  long l;
  if (!f) { perror(nom); exit(2); }
  fseek(f, 0, SEEK_END); l = ftell(f); fseek(f, 0, SEEK_SET);
  p = malloc(l ? l : 1);
  if (fread(p, 1, l, f) != (size_t)l) exit(2);
  fclose(f);
  *n = (size_t)l;
  return p;
}

static int abre(stbtt_fontinfo *fi, const unsigned char *d) {
  return stbtt_InitFont(fi, d, stbtt_GetFontOffsetForIndex(d, 0));
}

static int compara(const char *a, const char *b) {
  size_t na, nb, ns;
  unsigned char *da = lee(a, &na), *db = lee(b, &nb), *s;
  stbtt_fontinfo fa, fb;
  int g, fallos = 0, puntos = 0;
  unsigned cp;
  s = woff_a_sfnt(db, nb, &ns);
  if (!s) { printf("FALLO %s: woff_a_sfnt da NULL\n", b); return 1; }
  if (en_uso != ns) { printf("FALLO %s: queda memoria suelta (%zu en uso, %zu del sfnt)\n", b, en_uso, ns); fallos++; }
  if (!abre(&fa, da) || !abre(&fb, s)) { printf("FALLO %s: stb_truetype no la abre\n", b); return 1; }
  if (fa.numGlyphs != fb.numGlyphs) { printf("FALLO %s: %d glifos, el original %d\n", b, fb.numGlyphs, fa.numGlyphs); return 1; }
  for (g = 0; g < fa.numGlyphs; g++) {
    stbtt_vertex *va, *vb;
    int ka = stbtt_GetGlyphShape(&fa, g, &va), kb = stbtt_GetGlyphShape(&fb, g, &vb);
    int aa, la, ab, lb, x0a, y0a, x1a, y1a, x0b, y0b, x1b, y1b, ca, cb;
    stbtt_GetGlyphHMetrics(&fa, g, &aa, &la);
    stbtt_GetGlyphHMetrics(&fb, g, &ab, &lb);
    ca = stbtt_GetGlyphBox(&fa, g, &x0a, &y0a, &x1a, &y1a);
    cb = stbtt_GetGlyphBox(&fb, g, &x0b, &y0b, &x1b, &y1b);
    if (ka != kb || (ka && memcmp(va, vb, ka * sizeof *va)))
      { printf("FALLO %s: glifo %d con otros puntos (%d / %d)\n", b, g, kb, ka); fallos++; }
    if (aa != ab || la != lb) { printf("FALLO %s: glifo %d avance %d/%d lsb %d/%d\n", b, g, ab, aa, lb, la); fallos++; }
    if (ca != cb || (ca && (x0a != x0b || y0a != y0b || x1a != x1b || y1a != y1b)))
      { printf("FALLO %s: glifo %d con otra caja\n", b, g); fallos++; }
    puntos += ka;
    stbtt_FreeShape(&fa, va);
    stbtt_FreeShape(&fb, vb);
  }
  for (cp = 0; cp < 0x30000; cp++)
    if (stbtt_FindGlyphIndex(&fa, cp) != stbtt_FindGlyphIndex(&fb, cp)) { printf("FALLO %s: cmap U+%04X\n", b, cp); fallos++; break; }
  printf("%s %s: %d glifos, %d puntos, %zu -> %zu bytes, pico %zu bytes\n", fallos ? "FALLO" : "bien", b, fa.numGlyphs, puntos, nb, ns, pico);
  cuenta_free(s);
  free(da);
  free(db);
  return fallos != 0;
}

/* cortada en todos los tamaños y con bytes cambiados: que no se cuelgue ni lea
   fuera (se compila con -fsanitize=address,undefined) ni deje memoria suelta */
static int rotos(const char *a) {
  size_t n, k, ns, nulos = 0, salen = 0;
  unsigned char *d = lee(a, &n), *c = malloc(n), *s;
  unsigned semilla = 12345, r;
  for (k = 0; k < n; k += k < 400 || k + 8 > n ? 1 : 7) {   /* (todos al principio y al final) */
    unsigned char *t = malloc(k ? k : 1);
    memcpy(t, d, k);
    s = woff_a_sfnt(t, k, &ns);
    if (s) { if (k + 4 <= n) salen++; cuenta_free(s); } else nulos++;   /* (sin el relleno del final, sí) */
    free(t);
  }
  if (salen) { printf("FALLO %s: cortada sale algo %zu veces\n", a, salen); return 1; }
  for (r = 0; r < 1500; r++) {
    int j;
    memcpy(c, d, n);
    for (j = 0; j < 1 + (int)(r % 4); j++) {
      semilla = semilla * 1103515245u + 12345u;
      c[(semilla >> 8) % n] ^= (unsigned char)(1 + ((semilla >> 20) & 0xFE));
    }
    s = woff_a_sfnt(c, n, &ns);
    if (s) cuenta_free(s);           /* (lo que salga no se abre: stb_truetype se fía de lo que lee) */
    if (en_uso) { printf("FALLO %s: memoria sin soltar tras un error (%zu)\n", a, en_uso); return 1; }
  }
  printf("bien %s: %zu cortes dan NULL; 1500 estropeados sin colgarse\n", a, nulos);
  free(c);
  free(d);
  return 0;
}

/* ---- glifos rotos: el WOFF2 se descomprime aquí, se estropean bytes de lo
   descomprimido (sobre todo de glyf/loca/hmtx transformadas) y se vuelve a
   meter en un Brotli «sin comprimir» (bloques ISUNCOMPRESSED, RFC 7932 9.2),
   para que lo estropeado llegue de verdad a rehace_glyf y rehace_hmtx */
typedef struct { unsigned char *p; size_t n; unsigned bits, nb; } bits;
static void pon(bits *b, unsigned v, unsigned k) {
  while (k--) {
    b->bits |= (v & 1) << b->nb++; v >>= 1;
    if (b->nb == 8) { b->p[b->n++] = (unsigned char)b->bits; b->bits = b->nb = 0; }
  }
}
static void alinea(bits *b) { if (b->nb) pon(b, 0, 8 - b->nb); }
static size_t brotli_crudo(const unsigned char *d, size_t n, unsigned char *o) {
  bits b = {o, 0, 0, 0};
  size_t i = 0;
  pon(&b, 0, 1);                     /* WBITS = 16 */
  while (i < n) {
    size_t k = n - i > 65536 ? 65536 : n - i;
    pon(&b, 0, 1); pon(&b, 0, 2); pon(&b, (unsigned)(k - 1), 16); pon(&b, 1, 1);   /* no último, 4 nibbles, sin comprimir */
    alinea(&b);
    memcpy(b.p + b.n, d + i, k); b.n += k; i += k;
  }
  pon(&b, 1, 1); pon(&b, 1, 1);      /* ISLAST, ISLASTEMPTY */
  alinea(&b);
  return b.n;
}
static int b128(const unsigned char *d, size_t *i, unsigned *v) {
  unsigned r = 0, k;
  for (k = 0; k < 5; k++) { unsigned c = d[(*i)++]; r = r << 7 | (c & 0x7F); if (!(c & 0x80)) { *v = r; return 1; } }
  return 0;
}
static int glifos_rotos(const char *a) {
  size_t n, i = 48, total = 0, gi = 0, gn = 0, un, ns;
  unsigned char *d = lee(a, &n), *u, *c, *w, *s, *dic;
  unsigned nt = d[12] << 8 | d[13], t, r, comp, semilla = 777, salen = 0;
  size_t dn;
  for (t = 0; t < nt; t++) {
    unsigned fl = d[i++], ori, tl, tag = fl & 63;
    if (tag == 63) { tag = 1000; i += 4; }
    if (!b128(d, &i, &ori)) return 1;
    tl = ori;
    if (((tag == 10 || tag == 11) && (fl >> 6) == 0) || (tag == 3 && (fl >> 6) == 1)) b128(d, &i, &tl);
    if (tag == 10 || tag == 11 || tag == 3) { if (!gn) gi = total; gn = total + tl - gi; }
    total += tl;
  }
  comp = (unsigned)d[20] << 24 | d[21] << 16 | d[22] << 8 | d[23];
  {                                  /* BROTLI.DIC va con zlib */
    unsigned char *z = lee(getenv("WOFF_DIC"), &dn);
    int dl;
    dic = (unsigned char *)stbi_zlib_decode_malloc((const char *)z, (int)dn, &dl);
    free(z);
    if (!dic || dl != 122784) { printf("FALLO: WOFF_DIC no es el diccionario\n"); return 1; }
  }
  BrotliSetDictionaryData(dic);
  u = malloc(total); un = total;
  if (BrotliDecoderDecompress(comp, d + i, &un, u) != BROTLI_DECODER_RESULT_SUCCESS || un != total) { printf("FALLO %s: no se descomprime\n", a); return 1; }
  BrotliSetDictionaryData(NULL);
  c = malloc(total);
  w = malloc(i + total + total / 65536 * 8 + 64);
  memcpy(w, d, i);
  for (r = 0; r < 2500; r++) {
    int j;
    size_t wn;
    memcpy(c, u, total);
    if (r) for (j = 0; j < 1 + (int)(r % 3); j++) {
      size_t p;
      semilla = semilla * 1103515245u + 12345u;
      p = (semilla >> 4) % (r & 1 ? total : gn);
      if (!(r & 1)) p += gi;
      semilla = semilla * 1103515245u + 12345u;
      c[p] = (r & 2) ? (unsigned char)(semilla >> 16) : c[p] ^ (unsigned char)(1u << ((semilla >> 16) & 7));
    }
    wn = i + brotli_crudo(c, total, w + i);
    w[20] = (unsigned char)((wn - i) >> 24); w[21] = (unsigned char)((wn - i) >> 16); w[22] = (unsigned char)((wn - i) >> 8); w[23] = (unsigned char)(wn - i);
    w[8] = (unsigned char)(wn >> 24); w[9] = (unsigned char)(wn >> 16); w[10] = (unsigned char)(wn >> 8); w[11] = (unsigned char)wn;
    s = woff_a_sfnt(w, wn, &ns);
    if (r == 0 && !s) { printf("FALLO %s: el Brotli sin comprimir no se lee\n", a); return 1; }
    if (s) { salen++; cuenta_free(s); }
    if (en_uso) { printf("FALLO %s: memoria sin soltar tras un error (%zu)\n", a, en_uso); return 1; }
  }
  printf("bien %s: 2500 con glyf/loca/hmtx estropeadas sin colgarse (%u dan letra)\n", a, salen);
  free(u); free(c); free(w); free(d); STBI_FREE(dic);
  return 0;
}

static int info(int argc, char **argv) {
  size_t n, ns;
  unsigned char *d = lee(argv[2], &n), *s;
  stbtt_fontinfo fi;
  const char *salida = NULL;
  int i, puntos = 0, g;
  s = woff_a_sfnt(d, n, &ns);
  if (!s) { printf("%s: NULL\n", argv[2]); free(d); return 1; }
  if (!abre(&fi, s)) { printf("%s: stb_truetype no la abre\n", argv[2]); return 1; }
  for (g = 0; g < fi.numGlyphs; g++) {
    stbtt_vertex *v;
    puntos += stbtt_GetGlyphShape(&fi, g, &v);
    stbtt_FreeShape(&fi, v);
  }
  printf("%s: %zu -> %zu bytes; %d glifos, %d puntos; pico de memoria %zu bytes\n", argv[2], n, ns, fi.numGlyphs, puntos, pico);
  for (i = 3; i < argc; i++) {
    if (!strcmp(argv[i], "-o") && i + 1 < argc) { salida = argv[++i]; continue; }
    if (!strncmp(argv[i], "U+", 2)) {
      unsigned cp = (unsigned)strtoul(argv[i] + 2, NULL, 16);
      int gl = stbtt_FindGlyphIndex(&fi, cp), av, lsb;
      stbtt_vertex *v;
      int k = gl ? stbtt_GetGlyphShape(&fi, gl, &v) : 0;
      stbtt_GetGlyphHMetrics(&fi, gl, &av, &lsb);
      printf("  U+%04X -> glifo %d, %d puntos, avance %d, lsb %d\n", cp, gl, k, av, lsb);
      if (gl) stbtt_FreeShape(&fi, v);
    }
  }
  if (salida) { FILE *f = fopen(salida, "wb"); fwrite(s, 1, ns, f); fclose(f); }
  cuenta_free(s);
  free(d);
  return 0;
}

int main(int argc, char **argv) {
  woff_carpeta(getenv("WOFF_CARPETA"));     /* donde está BROTLI.DIC */
  if (argc >= 4 && !strcmp(argv[1], "compara")) return compara(argv[2], argv[3]);
  if (argc >= 3 && !strcmp(argv[1], "rotos")) return rotos(argv[2]);
  if (argc >= 3 && !strcmp(argv[1], "info")) return info(argc, argv);
  if (argc >= 3 && !strcmp(argv[1], "glifos")) return glifos_rotos(argv[2]);
  fprintf(stderr, "uso: prueba compara|rotos|info ...\n");
  return 2;
}
