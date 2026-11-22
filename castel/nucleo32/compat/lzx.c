/* lzx.c - (COMPAT) el descompresor LZX (lzx.h). Lo esencial:
 *  - la entrada se lee a palabras de 16 bits (de las que se toma primero el
 *    bit de más peso), seguida aunque cambie de bloque CFDATA;
 *  - al principio de la carpeta, 1 bit (y 32 si es 1): el tamaño para la
 *    traducción E8;
 *  - cada bloque: su clase (3 bits: 1 literal, 2 alineado, 3 sin comprimir)
 *    y lo que mide (24 bits); los árboles de Huffman canónicos (de 16 bits
 *    como mucho), con sus largos dichos con el «pre-árbol» de 20 símbolos
 *    y como diferencia con los del bloque anterior;
 *  - la salida, a trozos de 32768 (una coincidencia puede pasarse del
 *    trozo: lo de más es del siguiente); al acabar cada trozo, la entrada
 *    se alinea a 16 bits.
 * Todo se comprueba: un archivo mal hecho da error, nunca se lee ni se
 * escribe fuera de lo suyo.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "lzx.h"

#define TROZO 32768u
#define MAX_RANURAS 51
#define N_LARGOS 249
#define N_PRE 20
#define N_ALIN 8
#define MAX_PRINCIPAL (256 + 50 * 8)
#define RAPIDA 10		/* los códigos de hasta 10 bits, de una vez */

typedef struct {
	u16 rapida[1 << RAPIDA];	/* (símbolo << 5) | largo; 0xFFFF: más largo o no vale */
	u16 primero[17], cuantos[17], desde[17];
	u16 orden[MAX_PRINCIPAL];
	int vacio;
} arbol;

struct lzx {
	u8 *ventana;
	u32 tam, mascara;
	u32 r0, r1, r2;
	int ranuras;
	u32 n_principal;
	u8 l_principal[MAX_PRINCIPAL], l_largos[N_LARGOS], l_alin[N_ALIN];
	arbol a_principal, a_largos, a_alin, a_pre;
	int cabecera;		/* ya leída la de la carpeta */
	u32 e8_tam;		/* el tamaño para la traducción E8 (0: no hay) */
	int e8_empezada;
	u32 e8_pos, n_trozos;
	int clase;
	u32 b_largo, b_queda;
	u32 hecho;		/* lo descomprimido de la carpeta */
	u32 trozo_ini;		/* dónde empieza el trozo que se está haciendo */
	/* la entrada */
	lzx_tira tira;
	void *u;
	const u8 *ip, *ifin;
	int acabada;
	u32 bits;
	int n_bits;
	const char *error;
};

static const u8 extra[MAX_RANURAS] = {
	0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14,
	15, 15, 16, 16, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17
};
static u32 base_pos[MAX_RANURAS];

lzx *lzx_nuevo(int bits)
{
	lzx *z;
	int k;
	static const u8 ranuras[7] = { 30, 32, 34, 36, 38, 42, 50 };

	if (bits < 15 || bits > 21)
		return NULL;
	z = so_pide(sizeof *z);
	if (!z)
		return NULL;
	memset(z, 0, sizeof *z);
	z->tam = 1u << bits;
	z->mascara = z->tam - 1;
	z->ventana = so_pide(z->tam);
	if (!z->ventana) {
		so_suelta(z);
		return NULL;
	}
	z->ranuras = ranuras[bits - 15];
	z->n_principal = 256 + (u32)z->ranuras * 8;
	if (!base_pos[MAX_RANURAS - 1])
		for (k = 1; k < MAX_RANURAS; k++)
			base_pos[k] = base_pos[k - 1] + (1u << extra[k - 1]);
	return z;
}

void lzx_suelta(lzx *z)
{
	if (!z)
		return;
	so_suelta(z->ventana);
	so_suelta(z);
}

void lzx_empieza(lzx *z, lzx_tira tira, void *u)
{
	z->r0 = z->r1 = z->r2 = 1;
	memset(z->l_principal, 0, sizeof z->l_principal);
	memset(z->l_largos, 0, sizeof z->l_largos);
	z->cabecera = 0;
	z->e8_tam = 0;
	z->e8_empezada = 0;
	z->e8_pos = 0;
	z->n_trozos = 0;
	z->clase = 0;
	z->b_largo = z->b_queda = 0;
	z->hecho = 0;
	z->tira = tira;
	z->u = u;
	z->ip = z->ifin = NULL;
	z->acabada = 0;
	z->bits = 0;
	z->n_bits = 0;
	z->error = NULL;
	memset(z->ventana, 0, z->tam);
}

const char *lzx_error(const lzx *z) { return z->error ? z->error : "los datos LZX están mal"; }

/* ---- la entrada ---- */
static int octeto(lzx *z)
{
	while (z->ip >= z->ifin) {
		const u8 *d;
		u32 n;
		int r;

		if (z->acabada)
			return -1;
		r = z->tira(z->u, &d, &n);
		if (r <= 0 || !n) {
			z->acabada = 1;
			if (r < 0)
				z->error = "no se puede leer lo comprimido";
			return -1;
		}
		z->ip = d;
		z->ifin = d + n;
	}
	return *z->ip++;
}

/* que haya n bits (n <= 17); al acabarse la entrada, ceros (como se hace siempre) */
static void asegura(lzx *z, int n)
{
	while (z->n_bits < n) {
		int a = octeto(z), b = a < 0 ? 0 : octeto(z);

		if (a < 0)
			a = 0;
		if (b < 0)
			b = 0;
		z->bits |= (u32)(a | (b << 8)) << (16 - z->n_bits);
		z->n_bits += 16;
	}
}
static u32 mira(lzx *z, int n) { return z->bits >> (32 - n); }
static void quita(lzx *z, int n)
{
	z->bits <<= n;
	z->n_bits -= n;
}
static u32 lee_bits(lzx *z, int n)
{
	u32 v;

	if (!n)
		return 0;
	if (n > 16) {
		v = lee_bits(z, 16) << (n - 16);
		return v | lee_bits(z, n - 16);
	}
	asegura(z, n);
	v = mira(z, n);
	quita(z, n);
	return v;
}

/* ---- los árboles de Huffman canónicos ---- */
static int haz_arbol(arbol *a, const u8 *l, u32 n)
{
	u32 k, c = 0, len, cuenta[17], pos[17];
	u32 usados = 0;

	memset(cuenta, 0, sizeof cuenta);
	for (k = 0; k < n; k++) {
		if (l[k] > 16)
			return -1;
		cuenta[l[k]]++;
	}
	cuenta[0] = 0;
	/* Kraft: que no sobren códigos */
	for (len = 1; len <= 16; len++) {
		a->primero[len] = (u16)c;
		a->cuantos[len] = (u16)cuenta[len];
		a->desde[len] = (u16)usados;
		pos[len] = usados;
		usados += cuenta[len];
		c += cuenta[len];
		if (c > (1u << len))
			return -1;
		c <<= 1;
	}
	a->vacio = usados == 0;
	for (k = 0; k < n; k++)
		if (l[k])
			a->orden[pos[l[k]]++] = (u16)k;
	for (k = 0; k < (1u << RAPIDA); k++)
		a->rapida[k] = 0xFFFF;
	for (len = 1; len <= RAPIDA; len++) {
		u32 j;

		for (j = 0; j < a->cuantos[len]; j++) {
			u32 cod = a->primero[len] + j, s = a->orden[a->desde[len] + j];
			u32 de = cod << (RAPIDA - len), cuantas = 1u << (RAPIDA - len), q;

			for (q = 0; q < cuantas; q++)
				a->rapida[de + q] = (u16)((s << 5) | len);
		}
	}
	return 0;
}

static int simbolo(lzx *z, const arbol *a)
{
	u32 v, len;
	u16 r;

	asegura(z, 16);
	v = mira(z, 16);
	r = a->rapida[v >> (16 - RAPIDA)];
	if (r != 0xFFFF) {
		quita(z, r & 31);
		return r >> 5;
	}
	for (len = RAPIDA + 1; len <= 16; len++) {
		u32 c = v >> (16 - len);

		if (c - a->primero[len] < a->cuantos[len]) {
			quita(z, (int)len);
			return a->orden[a->desde[len] + c - a->primero[len]];
		}
	}
	z->error = "un código de Huffman que no está en el árbol";
	return -1;
}

/* los largos de l[de..a) con el pre-árbol, como diferencia con los de antes */
static int lee_largos(lzx *z, u8 *l, u32 de, u32 a)
{
	u8 pl[N_PRE];
	u32 k, x;

	for (k = 0; k < N_PRE; k++)
		pl[k] = (u8)lee_bits(z, 4);
	if (haz_arbol(&z->a_pre, pl, N_PRE) < 0) {
		z->error = "el pre-árbol está mal";
		return -1;
	}
	for (x = de; x < a;) {
		int s = simbolo(z, &z->a_pre);
		u32 y;

		if (s < 0)
			return -1;
		if (s == 17 || s == 18) {
			y = s == 17 ? lee_bits(z, 4) + 4 : lee_bits(z, 5) + 20;
			if (x + y > a) {
				z->error = "los largos se pasan";
				return -1;
			}
			while (y--)
				l[x++] = 0;
		} else if (s == 19) {
			int w;

			y = lee_bits(z, 1) + 4;
			w = simbolo(z, &z->a_pre);
			if (w < 0 || w > 16)
				return -1;
			w = l[x] - w;
			if (w < 0)
				w += 17;
			if (x + y > a) {
				z->error = "los largos se pasan";
				return -1;
			}
			while (y--)
				l[x++] = (u8)w;
		} else {
			int w = l[x] - s;

			if (w < 0)
				w += 17;
			l[x++] = (u8)w;
		}
	}
	return 0;
}

/* la cabecera de un bloque nuevo */
static int bloque(lzx *z)
{
	u32 k, hi, lo;

	/* tras uno sin comprimir de largo impar, un octeto de relleno */
	if (z->clase == 3 && (z->b_largo & 1))
		(void)octeto(z);
	if (z->clase == 3) {
		z->bits = 0;
		z->n_bits = 0;
	}
	z->clase = (int)lee_bits(z, 3);
	hi = lee_bits(z, 16);
	lo = lee_bits(z, 8);
	z->b_largo = z->b_queda = (hi << 8) | lo;
	switch (z->clase) {
	case 2:
		for (k = 0; k < N_ALIN; k++)
			z->l_alin[k] = (u8)lee_bits(z, 3);
		if (haz_arbol(&z->a_alin, z->l_alin, N_ALIN) < 0) {
			z->error = "el árbol alineado está mal";
			return -1;
		}
		/* (y lo de los literales) */
		/* fall through */
	case 1:
		if (lee_largos(z, z->l_principal, 0, 256) < 0 || lee_largos(z, z->l_principal, 256, z->n_principal) < 0)
			return -1;
		if (haz_arbol(&z->a_principal, z->l_principal, z->n_principal) < 0 || z->a_principal.vacio) {
			z->error = "el árbol principal está mal";
			return -1;
		}
		if (z->l_principal[0xE8])
			z->e8_empezada = 1;
		if (lee_largos(z, z->l_largos, 0, N_LARGOS) < 0)
			return -1;
		if (haz_arbol(&z->a_largos, z->l_largos, N_LARGOS) < 0) {
			z->error = "el árbol de los largos está mal";
			return -1;
		}
		break;
	case 3: {
		u8 r[12];

		z->e8_empezada = 1;
		/* alinear a 16 bits (de 1 a 16 bits de relleno) */
		if (z->n_bits == 0)
			asegura(z, 16);
		z->bits = 0;
		z->n_bits = 0;
		for (k = 0; k < 12; k++) {
			int c = octeto(z);

			if (c < 0) {
				z->error = "se acaba en la cabecera de un bloque sin comprimir";
				return -1;
			}
			r[k] = (u8)c;
		}
		z->r0 = r[0] | (u32)r[1] << 8 | (u32)r[2] << 16 | (u32)r[3] << 24;
		z->r1 = r[4] | (u32)r[5] << 8 | (u32)r[6] << 16 | (u32)r[7] << 24;
		z->r2 = r[8] | (u32)r[9] << 8 | (u32)r[10] << 16 | (u32)r[11] << 24;
		break;
	}
	default:
		z->error = "una clase de bloque que no existe";
		return -1;
	}
	return 0;
}

/* descomprime hasta que lo hecho llegue a «hasta» (puede pasarse, con una
 * coincidencia, sin salirse del bloque) */
static int descomprime(lzx *z, u32 hasta)
{
	while ((i32)(hasta - z->hecho) > 0) {
		u32 tanda;

		if (!z->b_queda) {
			if (bloque(z) < 0)
				return -1;
			if (!z->b_queda) {
				z->error = "un bloque vacío";
				return -1;
			}
		}
		tanda = hasta - z->hecho;
		if (tanda > z->b_queda)
			tanda = z->b_queda;
		if (z->clase == 3) {
			u32 k;

			for (k = 0; k < tanda; k++) {
				int c = octeto(z);

				if (c < 0) {
					z->error = "se acaba en un bloque sin comprimir";
					return -1;
				}
				z->ventana[z->hecho++ & z->mascara] = (u8)c;
			}
			z->b_queda -= tanda;
			continue;
		}
		{
			i32 queda = (i32)tanda;

			while (queda > 0) {
				int s = simbolo(z, &z->a_principal);
				u32 largo, ranura, desp;

				if (s < 0)
					return -1;
				if (s < 256) {
					z->ventana[z->hecho++ & z->mascara] = (u8)s;
					queda--;
					continue;
				}
				s -= 256;
				largo = (u32)s & 7;
				if (largo == 7) {
					int l2;

					if (z->a_largos.vacio) {
						z->error = "falta el árbol de los largos";
						return -1;
					}
					l2 = simbolo(z, &z->a_largos);
					if (l2 < 0)
						return -1;
					largo += (u32)l2;
				}
				largo += 2;
				ranura = (u32)s >> 3;
				if (ranura == 0)
					desp = z->r0;
				else if (ranura == 1) {
					desp = z->r1;
					z->r1 = z->r0;
					z->r0 = desp;
				} else if (ranura == 2) {
					desp = z->r2;
					z->r2 = z->r0;
					z->r0 = desp;
				} else {
					u32 e = extra[ranura];

					if (ranura >= (u32)z->ranuras) {
						z->error = "una ranura de posición que no hay";
						return -1;
					}
					desp = base_pos[ranura] - 2;
					if (z->clase == 2 && e >= 3) {
						int al;

						if (e > 3)
							desp += lee_bits(z, (int)e - 3) << 3;
						al = simbolo(z, &z->a_alin);
						if (al < 0)
							return -1;
						desp += (u32)al;
					} else
						desp += lee_bits(z, (int)e);
					z->r2 = z->r1;
					z->r1 = z->r0;
					z->r0 = desp;
				}
				if (!desp || desp > z->hecho || desp > z->tam || (i32)largo - queda > (i32)(z->b_queda - tanda)) {
					z->error = "una coincidencia fuera de lo hecho";
					return -1;
				}
				{
					u32 k, de = z->hecho - desp;

					for (k = 0; k < largo; k++)
						z->ventana[(z->hecho + k) & z->mascara] = z->ventana[(de + k) & z->mascara];
				}
				z->hecho += largo;
				queda -= (i32)largo;
			}
			/* (queda <= 0: lo que se ha pasado también es de este bloque) */
			z->b_queda -= tanda + (u32)(-queda);
		}
	}
	return 0;
}

int lzx_trozo(lzx *z, u8 *d, u32 n)
{
	u32 k;

	if (n > TROZO || !n) {
		z->error = "un trozo de más de 32768";
		return -1;
	}
	if (!z->cabecera) {
		u32 hi = 0, lo = 0;

		if (lee_bits(z, 1)) {
			hi = lee_bits(z, 16);
			lo = lee_bits(z, 16);
		}
		z->e8_tam = (hi << 16) | lo;
		z->cabecera = 1;
		z->trozo_ini = 0;
	}
	if (descomprime(z, z->trozo_ini + n) < 0)
		return -1;
	for (k = 0; k < n; k++)
		d[k] = z->ventana[(z->trozo_ini + k) & z->mascara];
	z->trozo_ini += n;
	/* la entrada, a 16 bits */
	if (z->n_bits & 15)
		quita(z, z->n_bits & 15);
	/* la traducción E8 (en lo que sale, no en la ventana) */
	if (z->n_trozos++ < 32768 && z->e8_tam) {
		if (n <= 6 || !z->e8_empezada)
			z->e8_pos += n;
		else {
			u32 i = 0, cur = z->e8_pos;
			i32 tam = (i32)z->e8_tam;

			while (i < n - 10) {
				if (d[i++] != 0xE8) {
					cur++;
					continue;
				}
				{
					i32 abs_ = (i32)(d[i] | (u32)d[i + 1] << 8 | (u32)d[i + 2] << 16 | (u32)d[i + 3] << 24);

					if (abs_ >= -(i32)cur && abs_ < tam) {
						i32 rel = abs_ >= 0 ? abs_ - (i32)cur : abs_ + tam;

						d[i] = (u8)rel;
						d[i + 1] = (u8)(rel >> 8);
						d[i + 2] = (u8)(rel >> 16);
						d[i + 3] = (u8)(rel >> 24);
					}
				}
				i += 4;
				cur += 5;
			}
			z->e8_pos += n;
		}
	}
	return 0;
}
