/* =============================================================================
 *  memoria.c - malloc, free, realloc y calloc propios de NAVEGA2.EXE.
 *  NetSurf pide miles de trozos pequeños (el DOM, las cajas, las hojas de
 *  estilo); el montón de HW32 los redondea a potencias de dos (de 33 a 64
 *  octetos gasta 64) y en 16 MB eso es mucho. Aquí: trozos de 8 en 8 con su
 *  cabecera y las listas de libres por tamaño (exactas hasta 512 octetos,
 *  por potencias de dos después), juntando los libres vecinos, dentro de
 *  bloques de 1 MB pedidos con VirtualAlloc (los muy grandes, su bloque). Un
 *  bloque que queda vacío del todo se devuelve.
 *  Ojo: lo que da MSVCRT.DLL (_strdup...) no se suelta con este free; por eso
 *  apoyo.c hace strdup con este malloc.
 *  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * ============================================================================= */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define WINAPI __attribute__((stdcall))
void *WINAPI VirtualAlloc(void *, uint32_t, uint32_t, uint32_t);
int WINAPI VirtualFree(void *, uint32_t, uint32_t);

/* un trozo: [tamaño del anterior][tamaño | EN_USO | ANT_EN_USO][lo del programa...] */
typedef struct Trozo {
  uint32_t ant;               /* el tamaño del anterior (si está libre) */
  uint32_t tam;
  struct Trozo *sig, *prev;   /* (solo si está libre: en su lista) */
} Trozo;
#define EN_USO 1u
#define ANT_EN_USO 2u
#define CAB 8u
#define MINIMO ((uint32_t)(CAB + 2 * sizeof(void *)))   /* (16 en 32 bits) */
#define TAM(t) ((t)->tam & ~7u)
#define SIG_T(t) ((Trozo *)((char *)(t) + TAM(t)))
#define ANT_T(t) ((Trozo *)((char *)(t) - (t)->ant))
#define BLOQUE (1u << 20)

#define N_EXACTAS 64            /* 16, 24, ... 520 */
#define N_LISTAS (N_EXACTAS + 24)
static Trozo *lista[N_LISTAS];
static uint32_t en_bloques, usado, usado_max;
#define N_BLOQUES 60
static char *bloques[N_BLOQUES];

static int lista_de(uint32_t t) {
  int k;
  if (t < MINIMO + 8 * N_EXACTAS) return (t - MINIMO) >> 3;
  for (k = 0, t >>= 10; t > 1 && k < 23; k++) t >>= 1;
  return N_EXACTAS + k;
}
static void mete(Trozo *t) {
  int k = lista_de(TAM(t));
  t->prev = NULL;
  t->sig = lista[k];
  if (lista[k]) lista[k]->prev = t;
  lista[k] = t;
}
static void saca(Trozo *t) {
  if (t->prev) t->prev->sig = t->sig;
  else lista[lista_de(TAM(t))] = t->sig;
  if (t->sig) t->sig->prev = t->prev;
}
/* un trozo libre de tamaño t (pone el «tamaño del anterior» del siguiente) */
static void libre(Trozo *t, uint32_t tam, uint32_t ant_uso) {
  Trozo *s;
  t->tam = tam | ant_uso;
  s = SIG_T(t);
  s->ant = tam;
  s->tam &= ~ANT_EN_USO;
  mete(t);
}

/* un bloque nuevo: [valla del principio (en uso)][libre...][valla del final (en uso, tamaño 0)] */
static int bloque(uint32_t necesita) {
  uint32_t b = BLOQUE;
  int k;
  char *m;
  Trozo *t, *fin;
  if (necesita + 64 > b) b = (necesita + 64 + 0xFFFF) & ~0xFFFFu;
  for (k = 0; k < N_BLOQUES && bloques[k]; k++) ;
  if (k == N_BLOQUES) return 0;
  m = VirtualAlloc(NULL, b, 0x3000, 4);
  if (!m) return 0;
  bloques[k] = m;
  en_bloques += b;
  t = (Trozo *)m;
  fin = (Trozo *)(m + b - CAB);
  fin->tam = 0 | EN_USO;
  libre(t, b - CAB, ANT_EN_USO);      /* (no hay anterior: como si estuviera en uso) */
  return 1;
}

static uint32_t medida(size_t n) {
  uint32_t t = (uint32_t)((n + CAB + 7) & ~(size_t)7);
  return t < MINIMO ? MINIMO : t;
}

/* parte t (en uso, de tamaño TAM) para que se quede en tam; lo que sobra, libre */
static void recorta(Trozo *t, uint32_t tam) {
  uint32_t sobra = TAM(t) - tam;
  Trozo *r, *s;
  if (sobra < MINIMO) return;
  t->tam = tam | (t->tam & 7);
  r = SIG_T(t);
  s = (Trozo *)((char *)r + sobra);
  if (!(s->tam & EN_USO) && TAM(s)) { saca(s); sobra += TAM(s); }
  libre(r, sobra, ANT_EN_USO);
}

void *malloc(size_t n) {
  uint32_t t;
  int k, vuelta;
  if (n > 0x7FFF0000u) return NULL;
  t = medida(n);
  for (vuelta = 0; vuelta < 2; vuelta++) {
    for (k = lista_de(t); k < N_LISTAS; k++) {
      Trozo *c;
      for (c = lista[k]; c; c = c->sig) {
        if (TAM(c) >= t) {
          Trozo *s;
          saca(c);
          c->tam |= EN_USO;
          s = SIG_T(c);
          s->tam |= ANT_EN_USO;
          recorta(c, t);
          usado += TAM(c);
          if (usado > usado_max) usado_max = usado;
          return (char *)c + CAB;
        }
        if (k < N_EXACTAS) break;     /* (en las exactas, todos valen igual) */
      }
    }
    if (!bloque(t)) return NULL;
  }
  return NULL;
}

void free(void *p) {
  Trozo *t, *s;
  uint32_t tam;
  if (!p) return;
  t = (Trozo *)((char *)p - CAB);
  tam = TAM(t);
  usado -= tam;
  s = SIG_T(t);
  if (!(s->tam & EN_USO)) { saca(s); tam += TAM(s); }
  if (!(t->tam & ANT_EN_USO)) { Trozo *a = ANT_T(t); saca(a); tam += TAM(a); t = a; }
  libre(t, tam, t->tam & ANT_EN_USO);
  /* ¿el bloque entero libre? (de la valla del principio a la del final) */
  if (SIG_T(t)->tam == EN_USO && en_bloques > BLOQUE) {
    int k;
    for (k = 0; k < N_BLOQUES; k++)
      if (bloques[k] == (char *)t) {
        saca(t);
        en_bloques -= tam + CAB;
        bloques[k] = NULL;
        VirtualFree(t, 0, 0x8000);
        break;
      }
  }
}

void *calloc(size_t a, size_t b) {
  size_t n = a * b;
  void *p;
  if (b && n / b != a) return NULL;
  p = malloc(n);
  if (p) memset(p, 0, n);
  return p;
}

void *realloc(void *p, size_t n) {
  Trozo *t, *s;
  uint32_t q, tam;
  void *nuevo;
  if (!p) return malloc(n);
  if (!n) { free(p); return NULL; }
  t = (Trozo *)((char *)p - CAB);
  q = medida(n);
  tam = TAM(t);
  if (q <= tam) { usado -= tam; recorta(t, q); usado += TAM(t); return p; }
  s = SIG_T(t);
  if (!(s->tam & EN_USO) && tam + TAM(s) >= q) {      /* crecer sobre el libre de al lado */
    saca(s);
    usado -= tam;
    t->tam = (tam + TAM(s)) | (t->tam & 7);
    SIG_T(t)->tam |= ANT_EN_USO;
    recorta(t, q);
    usado += TAM(t);
    if (usado > usado_max) usado_max = usado;
    return p;
  }
  nuevo = malloc(n);
  if (!nuevo) return NULL;
  memcpy(nuevo, p, tam - CAB);
  free(p);
  return nuevo;
}

/* para el registro y NAVEGADOR2.md: lo pedido al sistema, lo usado y lo más usado (KB) */
void memoria_cuenta(unsigned *bloques, unsigned *ahora, unsigned *max) {
  *bloques = en_bloques >> 10; *ahora = usado >> 10; *max = usado_max >> 10;
}
