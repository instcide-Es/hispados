/* cab.c - (COMPAT) leer los CAB (cab.h): la cabecera (CFHEADER), las
 * carpetas (CFFOLDER), los archivos (CFFILE) y los datos (CFDATA), como los
 * describe la documentación pública de Microsoft («Microsoft Cabinet
 * Format»). Cada carpeta se descomprime de principio a fin (en LZX y MSZIP
 * lo de antes hace falta para lo de después) y solo hasta el último archivo
 * que se quiere de ella; de lo que sale, a cada archivo lo suyo.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "cab.h"
#include "lzx.h"
#include "miniz_cfg.h"

#define MAX_DATOS 38912u	/* 32768 + 6144: lo más que puede medir un CFDATA comprimido */
#define MAX_ARCHIVOS 4096
#define MAX_CARPETAS 1024

static u32 le16(const u8 *p) { return p[0] | (u32)p[1] << 8; }
static u32 le32(const u8 *p) { return p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }

i32 cab_lee_memoria(void *u, u32 pos, void *d, u32 n)
{
	cab_memoria *m = u;

	if (pos > m->n || n > m->n - pos)
		return -1;
	memcpy(d, m->d + pos, n);
	return (i32)n;
}

static int lee(cab *c, u32 pos, void *d, u32 n)
{
	if (pos > c->tam || n > c->tam - pos)
		return -1;
	return c->lee(c->u, c->base + pos, d, n) == (i32)n ? 0 : -1;
}

int cab_abre(cab *c, cab_lee le, void *u, u32 base, u32 tam_origen, const char **error)
{
	u8 h[36], b[8];
	u32 p, k, flags, archivos;

	memset(c, 0, sizeof *c);
	c->lee = le;
	c->u = u;
	c->base = base;
	*error = "no es un CAB";
	if (base > tam_origen || tam_origen - base < 36 || le(u, base, h, 36) != 36 || memcmp(h, "MSCF\0\0\0\0", 8))
		return -1;
	c->tam = le32(h + 8);
	if (c->tam < 36 || c->tam > tam_origen - base)
		return -1;
	if (h[25] != 1) {
		*error = "un CAB de una versión que no se conoce";
		return -1;
	}
	archivos = le32(h + 16);
	c->n_carpetas = (u16)le16(h + 26);
	c->n_archivos = (u16)le16(h + 28);
	flags = le16(h + 30);
	if (flags & 3) {
		*error = "el CAB sigue en otro (de varios discos): no se lleva";
		return -1;
	}
	if (!c->n_carpetas || c->n_carpetas > MAX_CARPETAS || !c->n_archivos || c->n_archivos > MAX_ARCHIVOS)
		return -1;
	p = 36;
	{
		u32 res_cab = 0, res_carp = 0;

		if (flags & 4) {
			u8 r[4];

			if (lee(c, p, r, 4))
				return -1;
			res_cab = le16(r);
			res_carp = r[2];
			c->res_datos = r[3];
			p += 4 + res_cab;
		}
		c->carpetas = so_pide(sizeof *c->carpetas * c->n_carpetas);
		c->archivos = so_pide(sizeof *c->archivos * c->n_archivos);
		if (!c->carpetas || !c->archivos) {
			*error = "no hay memoria";
			cab_cierra(c);
			return -1;
		}
		for (k = 0; k < c->n_carpetas; k++) {
			if (lee(c, p, b, 8))
				goto mal;
			c->carpetas[k].inicio = le32(b);
			c->carpetas[k].n_datos = (u16)le16(b + 4);
			c->carpetas[k].tipo = (u16)le16(b + 6);
			if (c->carpetas[k].inicio >= c->tam)
				goto mal;
			p += 8 + res_carp;
		}
	}
	p = archivos;
	for (k = 0; k < c->n_archivos; k++) {
		u8 f[16];
		cab_archivo *a = &c->archivos[k];
		u32 j;

		if (lee(c, p, f, 16))
			goto mal;
		a->tam = le32(f);
		a->desde = le32(f + 4);
		a->carpeta = (u16)le16(f + 8);
		a->atributos = (u16)le16(f + 14);
		p += 16;
		for (j = 0;; j++) {
			u8 ch;

			if (lee(c, p + j, &ch, 1) || j >= sizeof a->nombre - 1)
				goto mal;
			a->nombre[j] = (char)ch;
			if (!ch)
				break;
		}
		p += j + 1;
		if (a->carpeta >= c->n_carpetas) {
			*error = "un archivo que sigue en otro CAB: no se lleva";
			goto mal2;
		}
		if (a->tam > 0x10000000u || a->desde > 0x7FFF8000u) {
			*error = "un archivo demasiado grande";
			goto mal2;
		}
	}
	*error = NULL;
	return 0;
mal:
	*error = "el CAB está mal hecho";
mal2:
	cab_cierra(c);
	return -1;
}

void cab_cierra(cab *c)
{
	so_suelta(c->carpetas);
	so_suelta(c->archivos);
	c->carpetas = NULL;
	c->archivos = NULL;
}

i32 cab_busca(cab_lee le, void *u, u32 tam, u32 desde)
{
	u8 t[4096 + 8];
	u32 p = desde;

	while (p + 36 <= tam) {
		u32 n = tam - p < 4096 + 8 ? tam - p : 4096 + 8, k;

		if (le(u, p, t, n) != (i32)n)
			return -1;
		for (k = 0; k + 8 <= n; k++)
			if (t[k] == 'M' && !memcmp(t + k, "MSCF\0\0\0\0", 8)) {
				cab c;
				const char *e;

				if (!cab_abre(&c, le, u, p + k, tam, &e)) {
					cab_cierra(&c);
					return (i32)(p + k);
				}
			}
		if (n < 4096 + 8)
			break;
		p += 4096;
	}
	return -1;
}

/* ---- sacar ---- */
typedef struct {
	cab *c;
	u32 pos;		/* el CFDATA siguiente */
	u32 quedan;		/* los CFDATA que quedan */
	u8 *dat;		/* el último leído (lo comprimido) */
	u32 n_dat, n_desc;	/* lo que mide comprimido y descomprimido */
	const char *error;
	u32 hecho, total;
	cab_avance avance;
	void *u;
} datos;

/* el CFDATA siguiente, a d->dat */
static int siguiente(datos *d)
{
	u8 h[8];

	if (!d->quedan)
		return 0;
	if (lee(d->c, d->pos, h, 8)) {
		d->error = "el CAB se acaba antes de tiempo";
		return -1;
	}
	d->n_dat = le16(h + 4);
	d->n_desc = le16(h + 6);
	if (d->n_dat > MAX_DATOS || d->n_desc > 32768) {
		d->error = "un bloque de datos que no puede ser";
		return -1;
	}
	if (lee(d->c, d->pos + 8 + d->c->res_datos, d->dat, d->n_dat)) {
		d->error = "el CAB se acaba antes de tiempo";
		return -1;
	}
	d->pos += 8 + d->c->res_datos + d->n_dat;
	d->quedan--;
	d->hecho += 8 + d->n_dat;
	if (d->avance)
		d->avance(d->u, d->hecho, d->total);
	return 1;
}

/* para LZX: la entrada sigue de un CFDATA al otro */
static int tira_lzx(void *u, const u8 **p, u32 *n)
{
	datos *d = u;
	int r = siguiente(d);

	if (r <= 0)
		return r;
	*p = d->dat;
	*n = d->n_dat;
	return 1;
}

/* lo que sale de la carpeta, de «o» a «o + n», a los archivos que lo quieren */
static int reparte(cab *c, int k, const u8 *quiere, u32 o, const u8 *s, u32 n, cab_escribe escribe, void *u)
{
	u32 j;

	for (j = 0; j < c->n_archivos; j++) {
		cab_archivo *a = &c->archivos[j];
		u32 de, a_;

		if (a->carpeta != k || !quiere[j])
			continue;
		if (!a->tam) {
			if (o == 0 && escribe(u, a, s, 0, 1))
				return -1;
			continue;
		}
		if (a->desde >= o + n || a->desde + a->tam <= o)
			continue;
		de = a->desde > o ? a->desde : o;
		a_ = a->desde + a->tam < o + n ? a->desde + a->tam : o + n;
		if (escribe(u, a, s + (de - o), a_ - de, a_ == a->desde + a->tam))
			return -1;
	}
	return 0;
}

int cab_saca(cab *c, cab_quiere qf, cab_escribe escribe, cab_avance avance, void *u, const char **error)
{
	u8 *quiere, *sal = NULL;
	u32 j, total = 0;
	int k, r = -1;
	datos d;
	lzx *z = NULL;
	int bits_z = 0;

	*error = NULL;
	quiere = so_pide(c->n_archivos);
	memset(&d, 0, sizeof d);
	d.dat = so_pide(MAX_DATOS);
	sal = so_pide(32768);
	if (!quiere || !d.dat || !sal) {
		*error = "no hay memoria";
		goto fin;
	}
	for (j = 0; j < c->n_archivos; j++)
		quiere[j] = (u8)(qf(u, &c->archivos[j]) ? 1 : 0);
	/* el total (para el avance): lo de las carpetas que hacen falta */
	for (k = 0; k < c->n_carpetas; k++) {
		u32 sig = c->tam;
		int q;

		for (q = 0; q < c->n_carpetas; q++)
			if (c->carpetas[q].inicio > c->carpetas[k].inicio && c->carpetas[q].inicio < sig)
				sig = c->carpetas[q].inicio;
		for (j = 0; j < c->n_archivos; j++)
			if (quiere[j] && c->archivos[j].carpeta == k) {
				total += sig - c->carpetas[k].inicio;
				break;
			}
	}
	d.total = total;
	d.avance = avance;
	d.u = u;
	d.c = c;
	for (k = 0; k < c->n_carpetas; k++) {
		cab_carpeta *f = &c->carpetas[k];
		u32 hasta = 0, o = 0, tipo = f->tipo & 15;
		int alguno = 0;

		for (j = 0; j < c->n_archivos; j++)
			if (quiere[j] && c->archivos[j].carpeta == k) {
				alguno = 1;
				if (c->archivos[j].desde + c->archivos[j].tam > hasta)
					hasta = c->archivos[j].desde + c->archivos[j].tam;
			}
		if (!alguno)
			continue;
		d.pos = f->inicio;
		d.quedan = f->n_datos;
		if (tipo == 3) {
			int bits = (f->tipo >> 8) & 31;

			if (!z || bits != bits_z) {
				lzx_suelta(z);
				z = lzx_nuevo(bits);
				bits_z = bits;
				if (!z) {
					*error = bits < 15 || bits > 21 ? "un LZX con una ventana que no puede ser" : "no hay memoria para el LZX";
					goto fin;
				}
			}
			lzx_empieza(z, tira_lzx, &d);
			/* (en LZX no se sabe lo que mide cada trozo hasta leerlo: los
			 * trozos son de 32768 y el último, lo que quede; lo que quede se
			 * sabe de lo que dicen los CFDATA, así que se lleva la cuenta de
			 * lo que dicen los que se han leído y de los que faltan) */
			{
				/* lo que mide la carpeta descomprimida: lo dicen los CFDATA */
				u32 p = f->inicio, q, tot = 0;

				for (q = 0; q < f->n_datos; q++) {
					u8 h[8];

					if (lee(c, p, h, 8)) {
						*error = "el CAB se acaba antes de tiempo";
						goto fin;
					}
					tot += le16(h + 6);
					p += 8 + c->res_datos + le16(h + 4);
				}
				while (o < hasta && o < tot) {
					u32 n = tot - o < 32768 ? tot - o : 32768;

					if (lzx_trozo(z, sal, n) < 0) {
						*error = d.error ? d.error : lzx_error(z);
						goto fin;
					}
					if (reparte(c, k, quiere, o, sal, n, escribe, u) < 0) {
						*error = "no se puede escribir lo sacado";
						goto fin;
					}
					o += n;
				}
			}
		} else if (tipo == 0 || tipo == 1) {
			tinfl_decompressor inf;
			u8 *dic = NULL;

			if (tipo == 1) {
				dic = so_pide(TINFL_LZ_DICT_SIZE);
				if (!dic) {
					*error = "no hay memoria";
					goto fin;
				}
				memset(dic, 0, TINFL_LZ_DICT_SIZE);
			}
			{
				u32 dpos = 0;

				while (o < hasta) {
					int s = siguiente(&d);

					if (s <= 0) {
						*error = s < 0 ? d.error : "el CAB se acaba antes que sus archivos";
						so_suelta(dic);
						goto fin;
					}
					if (tipo == 0) {
						if (d.n_dat != d.n_desc) {
							*error = "un bloque sin comprimir que no cuadra";
							so_suelta(dic);
							goto fin;
						}
						memcpy(sal, d.dat, d.n_dat);
					} else {
						/* MSZIP: «CK» y un deflate entero, con lo de antes como diccionario */
						size_t ent, sale, hecho = 0;
						const u8 *ip;
						tinfl_status st;

						if (d.n_dat < 2 || d.dat[0] != 'C' || d.dat[1] != 'K') {
							*error = "un bloque MSZIP sin su «CK»";
							so_suelta(dic);
							goto fin;
						}
						tinfl_init(&inf);
						ip = d.dat + 2;
						ent = d.n_dat - 2;
						for (;;) {
							u32 libre = TINFL_LZ_DICT_SIZE - dpos;
							size_t e2 = ent;

							sale = libre;
							st = tinfl_decompress(&inf, ip, &e2, dic, dic + dpos, &sale, 0);
							ip += e2;
							ent -= e2;
							if (hecho + sale > d.n_desc) {
								*error = "un bloque MSZIP que da más de lo que dice";
								so_suelta(dic);
								goto fin;
							}
							memcpy(sal + hecho, dic + dpos, sale);
							hecho += sale;
							dpos = (dpos + (u32)sale) & (TINFL_LZ_DICT_SIZE - 1);
							if (st == TINFL_STATUS_DONE)
								break;
							if (st != TINFL_STATUS_HAS_MORE_OUTPUT && !(st == TINFL_STATUS_NEEDS_MORE_INPUT && ent)) {
								*error = "un bloque MSZIP mal hecho";
								so_suelta(dic);
								goto fin;
							}
						}
						if (hecho != d.n_desc) {
							*error = "un bloque MSZIP que no da lo que dice";
							so_suelta(dic);
							goto fin;
						}
					}
					if (reparte(c, k, quiere, o, sal, d.n_desc, escribe, u) < 0) {
						*error = "no se puede escribir lo sacado";
						so_suelta(dic);
						goto fin;
					}
					o += d.n_desc;
				}
			}
			so_suelta(dic);
		} else {
			*error = tipo == 2 ? "un CAB en Quantum: no se lleva" : "una compresión de CAB que no se conoce";
			goto fin;
		}
	}
	r = 0;
fin:
	lzx_suelta(z);
	so_suelta(quiere);
	so_suelta(d.dat);
	so_suelta(sal);
	return r;
}
