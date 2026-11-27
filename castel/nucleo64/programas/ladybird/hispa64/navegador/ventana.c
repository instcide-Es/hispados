/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * hispa64/navegador/ventana.c - Hispa-64, etapa 10 h (docs/64/HISPA-64.md,
 * sección 25): ventana.h, sobre la capa «so» de la oficina para Hispa-64
 * (so_hispa64.c, que va aquí dentro, con su main con otro nombre: el de
 * NAVEGADOR es el de LibMain de Ladybird) y el lienzo de la oficina. */
#define main so_hispa64_main_sin_usar
#include "so_hispa64.c"
#undef main
#include "lienzo.h"
#include "letras.h"
#include "ventana.h"

/* (so_hispa64.c llama a ofi_principal desde su main, que aquí no se usa) */
int ofi_principal(const char *args)
{
	(void)args;
	return 0;
}

int nv_hay_escritorio(void)
{
	return conecta();
}

void nv_pantalla(int *an, int *al)
{
	so_pantalla(an, al);
}

int nv_abre(const char *titulo, int an, int al)
{
	return so_ventana(titulo, an, al);
}

void nv_titulo(int v, const char *t)
{
	so_titulo(v, t);
}

void nv_maximiza(int v)
{
	so_maximiza(v);
}

void nv_cierra(int v)
{
	so_cierra_ventana(v);
}

void nv_mapa(int v, int x, int y, int an, int al, const uint32_t *px, int paso)
{
	so_mapa(v, x, y, an, al, px, paso);
}

int nv_suceso_de(int v, nv_suceso *s)
{
	return so_lee_suceso(v, (so_suceso *)s, 0);
}

/* (de otro hilo: solo mira la cola y el futex, no toca el búfer) */
int nv_espera(int v, int ms)
{
	hv_ventana *h;
	u32 visto;

	if (v < 1 || v > MAX_V || !V[v].h)
		return 0;
	h = V[v].h;
	visto = h->ev_aviso;
	__sync_synchronize();
	if (h->ev_toma != h->ev_pon)
		return 1;
	if (ctl->servidor > 0 && kill(ctl->servidor, 0) && errno == ESRCH)
		return 1;
	hv_espera(&h->ev_aviso, visto, ms);
	return h->ev_toma != h->ev_pon;
}

void nv_puntero(int v, int forma)
{
	so_puntero(v, forma);
}

int nv_control(void)
{
	return so_control();
}

void nv_al_portapapeles(const char *t, uint32_t n)
{
	so_al_portapapeles(t, n);
}

char *nv_del_portapapeles(uint32_t *n)
{
	u32 k = 0;
	char *d = so_del_portapapeles(&k);
	char *r;

	if (!d)
		return NULL;
	r = malloc(k + 1);
	if (r) {
		memcpy(r, d, k);
		r[k] = 0;
		*n = k;
	}
	so_suelta(d);
	return r;
}

static int cara_n = -1, cara_b = -1;

void nv_letras(void)
{
	letras_inicia();
	cara_n = letra_cara(FAM_SANS, 0, 0, NULL);
	cara_b = letra_cara(FAM_SANS, 1, 0, NULL);
}

int nv_texto(uint32_t *px, int an, int al, int paso, int x, int y, const char *t, uint32_t c, int negrita, int tam4)
{
	lienzo l;

	memset(&l, 0, sizeof l);
	l.p = px;
	l.an = an;
	l.al = al;
	l.paso = paso;
	lz_sin_recorte(&l);
	return lz_texto(&l, negrita ? cara_b : cara_n, tam4, 0, x, y, t, c);
}

int nv_ancho(const char *t, int negrita, int tam4)
{
	return lz_ancho(negrita ? cara_b : cara_n, tam4, 0, t);
}
