/* imagen.c - el Navegador de Hispa-486: de un archivo de imagen (PNG, GIF,
 * JPEG, BMP, ICO...) a un mapa de bits en la paleta de Hispa Ventanas, ya del
 * tamaño en que se va a ver. Los formatos son los de Mirador, tal cual
 * (imagen.asm); aquí se reduce (la media de los puntos que caen en cada uno:
 * la imagen grande nunca está entera en memoria, solo una fila) y se pasa a
 * los 256 colores fijos (el cubo de 6x6x6 y los 24 grises), repartiendo el
 * error (Floyd-Steinberg).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "nav.h"

int img_abre(const u8 *datos, u32 n, u32 *medidas);
int img_reduce(int k);
void img_medidas(u32 *medidas);
int img_prepara(void *memoria);
int img_fila(int y, u8 *destino);

#define MEM_MAX (3u << 20)       /* la memoria de trabajo que se le da como mucho al formato */

/* el color más parecido de la paleta: del cubo (16 + 36r + 6g + b, de 51 en
 * 51) o de los grises (232 + k, de 11 en 11) */
static int cerca(int r, int g, int b, int *rr, int *gg, int *bb) {
  int ir = (r + 25) / 51, ig = (g + 25) / 51, ib = (b + 25) / 51, d1, d2, gr, k;
  int cr = ir * 51, cg = ig * 51, cb = ib * 51;
  d1 = (r - cr) * (r - cr) + (g - cg) * (g - cg) + (b - cb) * (b - cb);
  gr = (r * 3 + g * 6 + b) / 10;
  k = (gr + 5) / 11;
  if (k > 23) k = 23;
  d2 = (r - k * 11) * (r - k * 11) + (g - k * 11) * (g - k * 11) + (b - k * 11) * (b - k * 11);
  if (d2 < d1) { *rr = *gg = *bb = k * 11; return 232 + k; }
  *rr = cr; *gg = cg; *bb = cb;
  return 16 + ir * 36 + ig * 6 + ib;
}

/* -> los puntos (w x h, filas de (w + 3) & ~3 octetos; malloc) o NULL con el
 * porqué en error. pw, ph: el tamaño que pide la página (0: el suyo; si solo
 * dice uno, el otro sale de las proporciones); max_w, max_h: lo más grande
 * que se pinta (se reduce, nunca se amplía más que lo que pida la página) */
u8 *img_descodifica(const u8 *d, u32 n, int pw, int ph, int max_w, int max_h, int *w, int *h, char *error) {
  u32 m[3], *suma = NULL;
  int W, H, tw, th, k, y, ty, x, r;
  u8 *trabajo = NULL, *fila = NULL, *sal = NULL;
  short *err = NULL;
  int paso;
  error[0] = 0;
  r = img_abre(d, n, m);
  if (r) {
    strcpy(error, r == 4 ? "no es una imagen que se conozca" : r == 2 ? "una variante que no se admite" : r == 5 ? "demasiado grande" : "est\341 da\361ada");
    return NULL;
  }
  W = (int)m[0]; H = (int)m[1];
  if (W < 1 || H < 1) { strcpy(error, "est\341 vac\355a"); return NULL; }
  /* el tamaño en que se va a ver */
  if (pw > 0 && ph > 0) { tw = pw; th = ph; }
  else if (pw > 0) { tw = pw; th = (int)(H * pw / W); }
  else if (ph > 0) { th = ph; tw = (int)(W * ph / H); }
  else { tw = W; th = H; }
  if (tw < 1) tw = 1;
  if (th < 1) th = 1;
  if (tw > max_w) { th = (int)(th * max_w / tw); tw = max_w; }
  if (th > max_h) { tw = (int)(tw * max_h / th); th = max_h; }
  if (tw < 1) tw = 1;
  if (th < 1) th = 1;
  if (tw > W) tw = W;                       /* (no se amplía: se ve más pequeña) */
  if (th > H) th = H;
  /* un JPEG grande se descodifica ya reducido (1/2, 1/4, 1/8): mucho menos trabajo */
  for (k = 3; k >= 1; k--)
    if ((W >> k) >= tw && (H >> k) >= th && img_reduce(k)) { img_medidas(m); W = (int)m[0]; H = (int)m[1]; break; }
  if (m[2] > MEM_MAX) { strcpy(error, "necesita demasiada memoria"); return NULL; }
  paso = (tw + 3) & ~3;
  trabajo = malloc(m[2] ? m[2] : 4);
  fila = malloc(W * 3 + 16);
  suma = malloc(tw * 4 * sizeof(u32));
  sal = malloc(paso * th);
  err = malloc((tw + 2) * 3 * 2 * sizeof(short));
  if (!trabajo || !fila || !suma || !sal || !err) { strcpy(error, "no hay memoria"); goto mal; }
  if ((r = img_prepara(trabajo))) { strcpy(error, "est\341 da\361ada"); goto mal; }
  /* filas de la imagen a filas de lo que se ve: cada punto, la media de su cuadro */
  memset(err, 0, (tw + 2) * 3 * 2 * sizeof(short));
  y = 0;
  for (ty = 0; ty < th; ty++) {
    int y1 = (int)((ty + 1) * H / th), cuenta = 0;
    short *e0 = err + ((ty & 1) ? (tw + 2) * 3 : 0), *e1 = err + ((ty & 1) ? 0 : (tw + 2) * 3);
    u8 *o = sal + ty * paso;
    if (y1 <= y) y1 = y + 1;
    memset(suma, 0, tw * 4 * sizeof(u32));
    for (; y < y1 && y < H; y++) {
      if (img_fila(y, fila)) { if (!cuenta) memset(fila, 255, W * 3); }   /* (cortada: lo que falte, blanco) */
      for (x = 0; x < tw; x++) {
        int x0 = (int)(x * W / tw), x1 = (int)((x + 1) * W / tw), j;
        u32 *s = suma + x * 4;
        if (x1 <= x0) x1 = x0 + 1;
        for (j = x0; j < x1; j++) { s[0] += fila[j * 3]; s[1] += fila[j * 3 + 1]; s[2] += fila[j * 3 + 2]; s[3]++; }
      }
      cuenta++;
    }
    /* a la paleta, repartiendo el error a la derecha y a la fila de abajo */
    memset(e1, 0, (tw + 2) * 3 * sizeof(short));
    for (x = 0; x < tw; x++) {
      u32 *s = suma + x * 4, c = s[3] ? s[3] : 1;
      int v[3], q[3], j;
      for (j = 0; j < 3; j++) {
        v[j] = (int)((s[j] + c / 2) / c) + e0[(x + 1) * 3 + j] / 16;
        if (v[j] < 0) v[j] = 0;
        if (v[j] > 255) v[j] = 255;
      }
      o[x] = (u8)cerca(v[0], v[1], v[2], &q[0], &q[1], &q[2]);
      for (j = 0; j < 3; j++) {
        int e = v[j] - q[j];
        e0[(x + 2) * 3 + j] += (short)(e * 7);
        e1[x * 3 + j] += (short)(e * 3);
        e1[(x + 1) * 3 + j] += (short)(e * 5);
        e1[(x + 2) * 3 + j] += (short)e;
      }
    }
  }
  free(trabajo); free(fila); free(suma); free(err);
  *w = tw; *h = th;
  return sal;
mal:
  free(trabajo); free(fila); free(suma); free(sal); free(err);
  return NULL;
}
