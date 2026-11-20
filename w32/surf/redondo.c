/* =============================================================================
 *  redondo.c - las esquinas redondas (border-radius) y las sombras
 *  (box-shadow) del Navegador completo (NAVEGA2.EXE), pintadas aquí
 *  directamente en el búfer de la página (libnsfb, 32 bits), con los bordes
 *  suavizados (4x4 muestras por punto en las esquinas). Lo llama el pintado
 *  de NetSurf (content/handlers/html/redraw.c, [HISPA]) con lo que ya ha
 *  medido. docs/486/NAVEGADOR2.md
 *  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * ============================================================================= */
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#include <libnsfb.h>
#include <libnsfb_plot.h>

#include "utils/errors.h"

#include "netsurf/types.h"
#include "netsurf/plot_style.h"

#include "surf.h"
#include "redondo.h"

/* el búfer y lo que se puede pintar (el recorte de libnsfb) */
static uint32_t *px;
static int paso;               /* en puntos */
static nsfb_bbox_t rec;

static bool prepara(void) {
  nsfb_t *fb = framebuffer_set_surface(NULL);
  uint8_t *p;
  int ll;
  framebuffer_set_surface(fb);
  if (!fb || nsfb_get_buffer(fb, &p, &ll) != 0 || !p) return false;
  px = (uint32_t *)p;
  paso = ll / 4;
  nsfb_plot_get_clip(fb, &rec);
  return true;
}

/* mezcla el color c (de NetSurf: 0xBBGGRR) con lo que hay, a 0..256 */
static inline void mezcla(int x, int y, colour c, int a) {
  uint32_t *d, v, r, g, b;
  if (a <= 0) return;
  d = &px[y * paso + x];
  if (a >= 256) { *d = ((c & 0xFF) << 16) | (c & 0xFF00) | ((c >> 16) & 0xFF); return; }
  v = *d;
  r = (((v >> 16) & 255) * (256 - a) + (c & 255) * a) >> 8;
  g = (((v >> 8) & 255) * (256 - a) + ((c >> 8) & 255) * a) >> 8;
  b = ((v & 255) * (256 - a) + ((c >> 16) & 255) * a) >> 8;
  *d = (r << 16) | (g << 8) | b;
}

/* ¿está (x, y) (en cuartos de punto, el centro de la muestra) dentro del
 * rectángulo redondo? (radios en puntos: arriba-izq., arriba-der.,
 * abajo-der., abajo-izq.) */
typedef struct { int x0, y0, x1, y1, r[4]; } Forma;

static int dentro_de(const Forma *f, int sx, int sy) {
  /* sx, sy: en 1/8 de punto */
  int x0 = f->x0 * 8, y0 = f->y0 * 8, x1 = f->x1 * 8, y1 = f->y1 * 8, r, cx, cy, dx, dy;
  if (sx < x0 || sx >= x1 || sy < y0 || sy >= y1) return 0;
  if (sx < x0 + f->r[0] * 8 && sy < y0 + f->r[0] * 8) { r = f->r[0] * 8; cx = x0 + r; cy = y0 + r; }
  else if (sx >= x1 - f->r[1] * 8 && sy < y0 + f->r[1] * 8) { r = f->r[1] * 8; cx = x1 - r; cy = y0 + r; }
  else if (sx >= x1 - f->r[2] * 8 && sy >= y1 - f->r[2] * 8) { r = f->r[2] * 8; cx = x1 - r; cy = y1 - r; }
  else if (sx < x0 + f->r[3] * 8 && sy >= y1 - f->r[3] * 8) { r = f->r[3] * 8; cx = x0 + r; cy = y1 - r; }
  else return 1;
  dx = sx - cx; dy = sy - cy;
  return (int64_t)dx * dx + (int64_t)dy * dy <= (int64_t)r * r;
}

/* cuánto del punto (x, y) cae dentro: 0..16 */
static int cubre(const Forma *f, int x, int y) {
  int i, j, n = 0;
  for (j = 0; j < 4; j++)
    for (i = 0; i < 4; i++) n += dentro_de(f, x * 8 + 1 + i * 2, y * 8 + 1 + j * 2);
  return n;
}

/* ¿la fila y del punto x cae en una esquina? */
static bool en_esquina(const Forma *f, int x, int y) {
  if (y < f->y0 + f->r[0] && x < f->x0 + f->r[0]) return true;
  if (y < f->y0 + f->r[1] && x >= f->x1 - f->r[1]) return true;
  if (y >= f->y1 - f->r[2] && x >= f->x1 - f->r[2]) return true;
  if (y >= f->y1 - f->r[3] && x < f->x0 + f->r[3]) return true;
  return false;
}

static void limita(Forma *f) {
  int w = f->x1 - f->x0, h = f->y1 - f->y0, k;
  for (k = 0; k < 4; k++) {
    if (f->r[k] < 0) f->r[k] = 0;
    if (f->r[k] * 2 > w) f->r[k] = w / 2;
    if (f->r[k] * 2 > h) f->r[k] = h / 2;
  }
}

bool redondo_caja(int x0, int y0, int x1, int y1, const int radio[4], colour fondo, bool con_fondo,
                  const int ancho[4], colour borde, bool con_borde, int alfa_fondo, int alfa_borde) {
  Forma fuera, dentro;
  int x, y, k, cx0, cx1, cy0, cy1;
  if (!prepara() || x1 <= x0 || y1 <= y0) return false;
  fuera.x0 = x0; fuera.y0 = y0; fuera.x1 = x1; fuera.y1 = y1;
  for (k = 0; k < 4; k++) fuera.r[k] = radio[k];
  limita(&fuera);
  dentro.x0 = x0 + (con_borde ? ancho[3] : 0); dentro.y0 = y0 + (con_borde ? ancho[0] : 0);
  dentro.x1 = x1 - (con_borde ? ancho[1] : 0); dentro.y1 = y1 - (con_borde ? ancho[2] : 0);
  /* el radio de dentro: el de fuera menos el borde */
  dentro.r[0] = fuera.r[0] - (con_borde ? (ancho[3] > ancho[0] ? ancho[3] : ancho[0]) : 0);
  dentro.r[1] = fuera.r[1] - (con_borde ? (ancho[1] > ancho[0] ? ancho[1] : ancho[0]) : 0);
  dentro.r[2] = fuera.r[2] - (con_borde ? (ancho[1] > ancho[2] ? ancho[1] : ancho[2]) : 0);
  dentro.r[3] = fuera.r[3] - (con_borde ? (ancho[3] > ancho[2] ? ancho[3] : ancho[2]) : 0);
  if (dentro.x1 < dentro.x0) dentro.x1 = dentro.x0;
  if (dentro.y1 < dentro.y0) dentro.y1 = dentro.y0;
  limita(&dentro);
  cx0 = x0 > rec.x0 ? x0 : rec.x0; cx1 = x1 < rec.x1 ? x1 : rec.x1;
  cy0 = y0 > rec.y0 ? y0 : rec.y0; cy1 = y1 < rec.y1 ? y1 : rec.y1;
  for (y = cy0; y < cy1; y++) {
    for (x = cx0; x < cx1; x++) {
      int a_fuera, a_dentro;
      bool esq = en_esquina(&fuera, x, y) || en_esquina(&dentro, x, y);
      if (esq) {
        a_fuera = cubre(&fuera, x, y) * 16;
        a_dentro = cubre(&dentro, x, y) * 16;
      } else {
        a_fuera = 256;
        a_dentro = (x >= dentro.x0 && x < dentro.x1 && y >= dentro.y0 && y < dentro.y1) ? 256 : 0;
      }
      if (con_fondo) mezcla(x, y, fondo, a_dentro * alfa_fondo >> 8);
      if (con_borde) {
        /* el borde: lo de fuera que no es de dentro (sobre el fondo) */
        int a = a_fuera - a_dentro;
        if (a > 0) mezcla(x, y, borde, a * alfa_borde >> 8);
      } else if (con_fondo && a_fuera > a_dentro) {
        mezcla(x, y, fondo, (a_fuera - a_dentro) * alfa_fondo >> 8);
      }
    }
  }
  return true;
}

/* la distancia (con signo, en puntos) del punto a un rectángulo redondo */
static float distancia(float px_, float py_, float cx, float cy, float hw, float hh, float r) {
  float qx = fabsf(px_ - cx) - (hw - r), qy = fabsf(py_ - cy) - (hh - r);
  float mx = qx > 0 ? qx : 0, my = qy > 0 ? qy : 0;
  float fuera = sqrtf(mx * mx + my * my);
  float m = qx > qy ? qx : qy;
  return fuera + (m < 0 ? m : 0) - r;
}

bool redondo_sombra(int x0, int y0, int x1, int y1, const int radio[4], int dx, int dy, int difu, int ext,
                    colour color, int alfa) {
  int x, y, sx0, sy0, sx1, sy1, k, r = 0, m;
  float cx, cy, hw, hh, rr;
  Forma caja;
  if (alfa <= 0 || !prepara() || x1 <= x0 || y1 <= y0) return false;
  caja.x0 = x0; caja.y0 = y0; caja.x1 = x1; caja.y1 = y1;
  for (k = 0; k < 4; k++) { caja.r[k] = radio[k]; if (radio[k] > r) r = radio[k]; }
  limita(&caja);
  if (difu < 0) difu = 0;
  /* la sombra: la caja movida (dx, dy) y agrandada (ext) */
  sx0 = x0 + dx - ext; sy0 = y0 + dy - ext; sx1 = x1 + dx + ext; sy1 = y1 + dy + ext;
  if (sx1 <= sx0 || sy1 <= sy0) return false;
  cx = (sx0 + sx1) / 2.0f; cy = (sy0 + sy1) / 2.0f;
  hw = (sx1 - sx0) / 2.0f; hh = (sy1 - sy0) / 2.0f;
  rr = (float)(r + ext);
  if (rr < 0) rr = 0;
  if (rr > hw) rr = hw;
  if (rr > hh) rr = hh;
  m = difu;
  {
    int ax0 = sx0 - m, ay0 = sy0 - m, ax1 = sx1 + m, ay1 = sy1 + m;
    if (ax0 < rec.x0) ax0 = rec.x0;
    if (ay0 < rec.y0) ay0 = rec.y0;
    if (ax1 > rec.x1) ax1 = rec.x1;
    if (ay1 > rec.y1) ay1 = rec.y1;
    for (y = ay0; y < ay1; y++) {
      for (x = ax0; x < ax1; x++) {
        float d;
        int a;
        /* bajo la caja no se ve (y no se pinta) */
        if (x >= x0 && x < x1 && y >= y0 && y < y1 && !en_esquina(&caja, x, y)) continue;
        d = distancia(x + 0.5f, y + 0.5f, cx, cy, hw, hh, rr);
        if (difu > 0) {
          float t = 0.5f - d / difu;
          if (t <= 0) continue;
          if (t > 1) t = 1;
          t = t * t * (3 - 2 * t);    /* suave, como el desenfoque */
          a = (int)(t * alfa);
        } else {
          a = d <= -0.5f ? alfa : d >= 0.5f ? 0 : (int)((0.5f - d) * alfa);
        }
        if (x >= x0 && x < x1 && y >= y0 && y < y1) a = a * (16 - cubre(&caja, x, y)) / 16;
        mezcla(x, y, color, a);
      }
    }
  }
  return true;
}
