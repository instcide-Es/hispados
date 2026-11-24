/* ram_alg.c - (TERMO, RAM) los patrones, los fallos, el diagnóstico y el
 * SMBIOS de la prueba de la memoria RAM (ram_alg.h). Sin llamadas al
 * sistema: corre igual en TERMO.BIN y en el anfitrión (pruebas/ram_prueba.c).
 * Las pruebas son las clásicas (las de memtest86+ y de cualquier libro: la
 * dirección propia, unos y ceros andantes, 0x55/0xAA, «moving inversions»),
 * escritas aquí desde cero.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdio.h>
#include <string.h>
#include "ram_alg.h"

const char *ra_nombre_prueba(int k)
{
	static const char *const n[RA_NPRUEBAS] = { "?", "las líneas de dirección", "la dirección propia", "unos y ceros andantes", "los patrones 0x55/0xAA",
						    "moving inversions (azar, subiendo y bajando)", "todos los núcleos a la vez (calor)" };

	return k > 0 && k < RA_NPRUEBAS ? n[k] : n[0];
}

/* ------------------------------------------------------------ los fallos */
void ra_fallos_empieza(struct ra_fallos *f)
{
	memset(f, 0, sizeof *f);
	f->fmin = ~(ra_u64)0;
}

void ra_apunta(struct ra_fallos *f, ra_u64 fis, int fis_sabida, ra_u32 esperado, ra_u32 leido, int prueba, int todos)
{
	ra_u32 x = esperado ^ leido, b;
	int carril = (int)((fis >> 2) & 1), i;

	f->total++;
	if (prueba > 0 && prueba < RA_NPRUEBAS)
		f->por_prueba[prueba]++;
	if (todos)
		f->en_todos++;
	else
		f->en_uno++;
	for (i = 0; i < 32; i++)
		if (x & (1u << i)) {
			f->por_bit[carril * 32 + i]++;
			if (leido & (1u << i))
				f->a1_bit[carril * 32 + i]++;
		}
	if (fis < f->fmin)
		f->fmin = fis;
	if (fis > f->fmax)
		f->fmax = fis;
	b = (ra_u32)(fis >> 20);
	for (i = 0; i < f->nbloques && f->bloques[i] != b; i++)
		;
	if (i == f->nbloques) {
		if (f->nbloques < RA_BLOQUES)
			f->bloques[f->nbloques++] = b;
		else
			f->bloques_lleno = 1;
	}
	if (f->ndet < RA_DET) {
		struct ra_detalle *d = &f->det[f->ndet++];

		d->fis = fis;
		d->esperado = esperado;
		d->leido = leido;
		d->prueba = (unsigned char)prueba;
		d->todos = (unsigned char)(todos != 0);
		d->fis_sabida = (unsigned char)(fis_sabida != 0);
	}
}

/* ------------------------------------------------------------ las pruebas */
#define CADA (1u << 20)	/* cada tantas palabras, se mira si hay que parar */

static int ha_parado(struct ra_ctx *c)
{
	if (!c->parado && c->parar && c->parar(c->ctx))
		c->parado = 1;
	return c->parado;
}

static void mal(struct ra_ctx *c, volatile ra_u32 *d, ra_u32 e, ra_u32 l, int prueba)
{
	c->fallos++;
	if (c->fallo)
		c->fallo(c->ctx, d, e, l, prueba);
}

/* las líneas de dirección: p[0] y cada p[2^k] (las que caben); cada una se
 * pone sola a unos y las demás tienen que seguir a cero (si una línea de
 * dirección está pegada a otra, dos de ellas son la misma celda) */
ra_u32 ra_lineas(struct ra_ctx *c, volatile ra_u32 *p, ra_u32 n)
{
	ra_u64 antes = c->fallos;
	ra_u32 k, j, a, b;

	if (n < 2)
		return 0;
	RA_ESCRIBE(&p[0], 0);
	for (k = 1; k < n && k; k <<= 1)
		RA_ESCRIBE(&p[k], 0);
	for (a = 0; a < n; a = a ? a << 1 : 1) {
		RA_ESCRIBE(&p[a], 0xFFFFFFFFu);
		for (b = 0; b < n; b = b ? b << 1 : 1) {
			ra_u32 v = RA_LEE(&p[b]);

			if (b != a && v != 0)
				mal(c, &p[b], 0, v, RA_LINEAS);
		}
		j = RA_LEE(&p[a]);
		if (j != 0xFFFFFFFFu)
			mal(c, &p[a], 0xFFFFFFFFu, j, RA_LINEAS);
		RA_ESCRIBE(&p[a], 0);
		if (a & 0x80000000u)
			break;
	}
	return (ra_u32)(c->fallos - antes);
}

/* cada palabra, su dirección; y luego su contraria (todos los bits pasan por los dos valores) */
ra_u32 ra_direccion(struct ra_ctx *c, volatile ra_u32 *p, ra_u32 n)
{
	ra_u64 antes = c->fallos;
	ra_u32 i, v, e;
	int vuelta;

	for (vuelta = 0; vuelta < 2 && !c->parado; vuelta++) {
		ra_u32 x = vuelta ? 0xFFFFFFFFu : 0;

		for (i = 0; i < n; i++)
			RA_ESCRIBE(&p[i], (ra_u32)(unsigned long)&p[i] ^ x);
		for (i = 0; i < n; i++) {
			v = RA_LEE(&p[i]);
			e = (ra_u32)(unsigned long)&p[i] ^ x;
			if (v != e)
				mal(c, &p[i], e, v, RA_DIRECCION);
			if ((i & (CADA - 1)) == CADA - 1 && ha_parado(c))
				break;
		}
	}
	return (ra_u32)(c->fallos - antes);
}

/* unos (o ceros) andantes: la palabra i lleva el bit (i + ronda) mod 32 (o
 * todos menos ese): en cada 32 palabras seguidas, cada bit, solo, una vez */
ra_u32 ra_andantes(struct ra_ctx *c, volatile ra_u32 *p, ra_u32 n, int ronda, int unos)
{
	ra_u64 antes = c->fallos;
	ra_u32 i, v, e, x = unos ? 0 : 0xFFFFFFFFu;

	for (i = 0; i < n; i++)
		RA_ESCRIBE(&p[i], (1u << ((i + (ra_u32)ronda) & 31)) ^ x);
	for (i = 0; i < n; i++) {
		v = RA_LEE(&p[i]);
		e = (1u << ((i + (ra_u32)ronda) & 31)) ^ x;
		if (v != e)
			mal(c, &p[i], e, v, RA_ANDANTES);
		if ((i & (CADA - 1)) == CADA - 1 && ha_parado(c))
			break;
	}
	return (ra_u32)(c->fallos - antes);
}

/* un patrón y su contrario (0x55555555 / 0xAAAAAAAA: cada bit, distinto de
 * sus vecinos): llenar; subiendo, comprobar y poner el contrario; bajando,
 * comprobar el contrario */
ra_u32 ra_patron(struct ra_ctx *c, volatile ra_u32 *p, ra_u32 n, ra_u32 patron)
{
	ra_u64 antes = c->fallos;
	ra_u32 i, v, q = ~patron;

	for (i = 0; i < n; i++)
		RA_ESCRIBE(&p[i], patron);
	for (i = 0; i < n; i++) {
		v = RA_LEE(&p[i]);
		if (v != patron)
			mal(c, &p[i], patron, v, RA_PATRON);
		RA_ESCRIBE(&p[i], q);
		if ((i & (CADA - 1)) == CADA - 1 && ha_parado(c))
			return (ra_u32)(c->fallos - antes);
	}
	for (i = n; i-- > 0;) {
		v = RA_LEE(&p[i]);
		if (v != q)
			mal(c, &p[i], q, v, RA_PATRON);
		if ((i & (CADA - 1)) == 0 && ha_parado(c))
			break;
	}
	return (ra_u32)(c->fallos - antes);
}

/* «moving inversions» con azar sembrado: llenar con el azar; subiendo,
 * comprobar y poner el contrario; bajando (el azar hacia atrás), comprobar
 * el contrario y poner el de antes; y subiendo otra vez, comprobarlo. Lo
 * que una celda escribe en su vecina sale al cambiar de sentido */
ra_u32 ra_inversiones(struct ra_ctx *c, volatile ra_u32 *p, ra_u32 n, ra_u32 semilla)
{
	ra_u64 antes = c->fallos;
	ra_u32 i, v, x = semilla;

	if (!n)
		return 0;
	for (i = 0; i < n; i++, x = ra_sig(x))
		RA_ESCRIBE(&p[i], x);
	for (i = 0, x = semilla; i < n; i++, x = ra_sig(x)) {
		v = RA_LEE(&p[i]);
		if (v != x)
			mal(c, &p[i], x, v, RA_INVERSIONES);
		RA_ESCRIBE(&p[i], ~x);
		if ((i & (CADA - 1)) == CADA - 1 && ha_parado(c))
			return (ra_u32)(c->fallos - antes);
	}
	/* (x es ahora el de la palabra n: hacia atrás) */
	for (i = n; i-- > 0;) {
		x = ra_ant(x);
		v = RA_LEE(&p[i]);
		if (v != ~x)
			mal(c, &p[i], ~x, v, RA_INVERSIONES);
		RA_ESCRIBE(&p[i], x);
		if ((i & (CADA - 1)) == 0 && ha_parado(c))
			return (ra_u32)(c->fallos - antes);
	}
	for (i = 0, x = semilla; i < n; i++, x = ra_sig(x)) {
		v = RA_LEE(&p[i]);
		if (v != x)
			mal(c, &p[i], x, v, RA_INVERSIONES);
		if ((i & (CADA - 1)) == CADA - 1 && ha_parado(c))
			break;
	}
	return (ra_u32)(c->fallos - antes);
}

/* ------------------------------------------------------------ el diagnóstico */
void ra_diagnostica(const struct ra_fallos *f, struct ra_diag *d)
{
	ra_u64 suma = 0, max = 0, oct[8] = { 0 }, maxo = 0;
	int i, bmax = 0, omax = 0, bits_oct = 0;

	memset(d, 0, sizeof *d);
	d->a_uno = -1;
	if (!f->total) {
		d->clase = RA_D_BIEN;
		return;
	}
	for (i = 0; i < 64; i++) {
		suma += f->por_bit[i];
		oct[i / 8] += f->por_bit[i];
		if (f->por_bit[i] > max)
			max = f->por_bit[i], bmax = i;
	}
	for (i = 0; i < 8; i++)
		if (oct[i] > maxo)
			maxo = oct[i], omax = i;
	for (i = omax * 8; i < omax * 8 + 8; i++)
		if (f->por_bit[i])
			bits_oct++;
	/* el mismo bit del bus casi siempre (9 de cada 10): un chip, o su patilla */
	if (suma && max * 10 >= suma * 9) {
		d->clase = RA_D_BIT;
		d->bit = bmax;
		if (f->a1_bit[bmax] * 10 >= f->por_bit[bmax] * 9)
			d->a_uno = 1;
		else if (f->a1_bit[bmax] * 10 <= f->por_bit[bmax])
			d->a_uno = 0;
		return;
	}
	/* el mismo octeto del bus (los 8 bits de un chip x8), con varios bits */
	if (suma && maxo * 10 >= suma * 9 && bits_oct >= 2) {
		d->clase = RA_D_CHIP;
		d->bit = omax * 8;
		return;
	}
	/* las líneas de dirección: una celda que es otra */
	if (f->por_prueba[RA_LINEAS]) {
		d->clase = RA_D_LINEAS;
		return;
	}
	/* en una zona pequeña (pocas zonas de 1 MB, juntas) */
	if (!f->bloques_lleno && f->nbloques <= 4 && f->fmax - f->fmin < (16u << 20)) {
		d->clase = RA_D_ZONA;
		return;
	}
	/* solo con todos los núcleos (calor, tráfico) */
	if (f->en_todos && !f->en_uno) {
		d->clase = RA_D_CALOR;
		return;
	}
	d->clase = RA_D_DISPERSO;
}

static const char *ra_veces(ra_u64 n, char *d)
{
	snprintf(d, 32, "%llu %s", (unsigned long long)n, n == 1 ? "vez" : "veces");
	return d;
}

int ra_diag_texto(const struct ra_fallos *f, const struct ra_diag *d, char *t, int cabe)
{
	char a[32], b[32], c[24];
	int k = 0, i, pr = 0;

	if (cabe <= 0)
		return 0;
	t[0] = 0;
#define TX(...) do { if (k < cabe) k += snprintf(t + k, (size_t)(cabe - k), __VA_ARGS__); } while (0)
	switch (d->clase) {
	case RA_D_BIEN:
		TX("ni un solo fallo: cada palabra ha devuelto lo que se escribió.");
		return k;
	case RA_D_BIT:
		TX("siempre falla el MISMO BIT de datos (el D%d de los 64 del bus%s): %s. Es lo típico de UN CHIP del módulo estropeado (o de su soldadura): "
		   "cambiar el módulo.", d->bit, d->a_uno == 1 ? ", que se queda a 1" : d->a_uno == 0 ? ", que se queda a 0" : "", ra_veces(f->por_bit[d->bit], a));
		break;
	case RA_D_CHIP:
		TX("los fallos caen siempre en el mismo octeto del bus (los bits D%d a D%d: los de un mismo chip de 8 bits): lo típico de UN CHIP del módulo estropeado: "
		   "cambiar el módulo.", d->bit, d->bit + 7);
		break;
	case RA_D_LINEAS:
		TX("al escribir en una dirección cambia otra (las líneas de dirección): suele ser el módulo MAL ASENTADO en su ranura, la ranura sucia o dañada, "
		   "o el controlador de memoria. Quitar el módulo, limpiar los contactos (goma de borrar, sin tocar los dorados con los dedos) y volver a ponerlo.");
		break;
	case RA_D_ZONA:
		TX("los fallos están en una zona pequeña (de %s a %s): celdas estropeadas de UN módulo: cambiar el módulo.",
		   (snprintf(a, sizeof a, "%llXh", (unsigned long long)f->fmin), a), (snprintf(b, sizeof b, "%llXh", (unsigned long long)f->fmax), b));
		break;
	case RA_D_CALOR:
		TX("solo falla con TODOS LOS NÚCLEOS a la vez (más calor y más tráfico); con uno solo, bien: apunta al controlador de memoria (en el procesador), "
		   "a la temperatura o a la alimentación (tensión), o a un módulo al límite. Mirar las temperaturas de la prueba y limpiar el polvo; probar con un solo módulo.");
		break;
	default:
		TX("los fallos salen DISPERSOS (en muchas direcciones y en bits distintos): eso es más del controlador de memoria, del calor, de la tensión o de un "
		   "módulo mal asentado que de un chip concreto. Limpiar y asentar los módulos y probar con uno solo cada vez.");
	}
	/* en qué pruebas */
	TX(" Ha fallado en:");
	for (i = 1; i < RA_NPRUEBAS; i++)
		if (f->por_prueba[i]) {
			TX("%s %s (%llu)", pr ? ";" : "", ra_nombre_prueba(i), (unsigned long long)f->por_prueba[i]);
			pr++;
		}
	TX(". En total, %llu %s mal leídas%s.", (unsigned long long)f->total, f->total == 1 ? "palabra" : "palabras",
	   f->bloques_lleno ? ", en más de 64 zonas de 1 MB" : (snprintf(c, sizeof c, ", en %d %s de 1 MB", f->nbloques, f->nbloques == 1 ? "zona" : "zonas"), c));
#undef TX
	return k;
}

/* ------------------------------------------------------------ el SMBIOS */
static unsigned u16le(const unsigned char *p) { return (unsigned)p[0] | ((unsigned)p[1] << 8); }
static ra_u32 u32le(const unsigned char *p) { return (ra_u32)p[0] | ((ra_u32)p[1] << 8) | ((ra_u32)p[2] << 16) | ((ra_u32)p[3] << 24); }
static ra_u64 u64le(const unsigned char *p) { return (ra_u64)u32le(p) | ((ra_u64)u32le(p + 4) << 32); }

/* la cadena número k de la estructura (las cadenas van detrás de lo formateado) */
static void cadena(const unsigned char *e, const unsigned char *fin, int k, char *d, int cabe)
{
	const unsigned char *s = e + e[1];
	int i = 0, n;

	d[0] = 0;
	if (k <= 0)
		return;
	while (--k > 0 && s < fin) {
		while (s < fin && *s)
			s++;
		s++;
	}
	if (s >= fin)
		return;
	while (s < fin && *s == ' ')
		s++;
	for (; s < fin && *s && i < cabe - 1; s++)
		d[i++] = *s >= 0x20 && *s < 0x7F ? (char)*s : '?';
	while (i > 0 && d[i - 1] == ' ')
		i--;
	d[i] = 0;
	/* lo que no dice nada */
	n = (int)strlen(d);
	if (!n || !strcmp(d, "Not Specified") || !strcmp(d, "Unknown") || !strcmp(d, "NO DIMM") || !strcmp(d, "Undefined") || !strcmp(d, "None")
	    || !strcmp(d, "To be filled by O.E.M.") || !strcmp(d, "0000") || !strcmp(d, "00000000"))
		d[0] = 0;
}

/* los códigos JEDEC que algunas BIOS dan en vez del nombre */
static void fabricante_claro(char *d, int cabe)
{
	static const struct { const char *c, *n; } t[] = {
		{ "80CE", "Samsung" }, { "CE00", "Samsung" }, { "802C", "Micron" }, { "2C00", "Micron" }, { "80AD", "SK Hynix" }, { "AD00", "SK Hynix" },
		{ "0198", "Kingston" }, { "9801", "Kingston" }, { "029E", "Corsair" }, { "9E02", "Corsair" }, { "04CD", "G.Skill" }, { "CD04", "G.Skill" },
		{ "859B", "Crucial" }, { "9B85", "Crucial" }, { "8551", "Qimonda" }, { "5105", "Qimonda" }, { "0443", "Ramaxel" }, { "4304", "Ramaxel" },
		{ "8C26", "Elpida" }, { "02FE", "Elpida" }, { "FE02", "Elpida" }, { "0B03", "Nanya" }, { "830B", "Nanya" }, { "0D9B", "Transcend" },
	};
	unsigned i;

	for (i = 0; i < sizeof t / sizeof t[0]; i++)
		if (!strncmp(d, t[i].c, 4) && (d[4] == 0 || d[4] == '0')) {
			snprintf(d, (size_t)cabe, "%s", t[i].n);
			return;
		}
}

void ra_lee_smbios(const unsigned char *t, ra_u32 largo, struct ra_smbios *s)
{
	ra_u32 o = 0;
	int ecc16 = -1, ancho_ecc = -1;

	memset(s, 0, sizeof *s);
	s->ecc = -1;
	while (o + 4 <= largo) {
		const unsigned char *e = t + o, *f;
		unsigned tipo = e[0], l = e[1];
		ra_u32 q;

		if (l < 4 || o + l > largo)
			break;
		/* el final: detrás de las cadenas, dos ceros */
		for (q = o + l; q + 1 < largo && (t[q] || t[q + 1]); q++)
			;
		f = t + (q + 2 <= largo ? q + 2 : largo);
		s->hay = 1;
		if (tipo == 16 && l >= 0x0F && e[5] == 3) {	/* la memoria del sistema */
			ra_u32 kb = u32le(e + 7);

			s->ranuras += (int)u16le(e + 0x0D);
			if (e[6] == 5 || e[6] == 6 || e[6] == 7)
				ecc16 = 1;
			else if (e[6] == 3 && ecc16 < 0)
				ecc16 = 0;
			if (kb == 0x80000000u && l >= 0x17)
				s->max_mb += u64le(e + 0x0F) >> 20;
			else if (kb != 0x80000000u)
				s->max_mb += kb >> 10;
		} else if (tipo == 17 && l >= 0x15 && s->nmod < RA_MODULOS) {
			struct ra_modulo *m = &s->m[s->nmod++];
			unsigned tam = u16le(e + 0x0C);

			memset(m, 0, sizeof *m);
			m->handle = u16le(e + 2);
			m->ancho_total = u16le(e + 8);
			m->ancho_datos = u16le(e + 0x0A);
			if (tam == 0x7FFF && l >= 0x20)
				m->mb = u32le(e + 0x1C) & 0x7FFFFFFFu;
			else if (tam != 0xFFFF && tam)
				m->mb = tam & 0x8000 ? (tam & 0x7FFF) / 1024 : tam;
			cadena(e, f, e[0x10], m->ranura, sizeof m->ranura);
			cadena(e, f, e[0x11], m->banco, sizeof m->banco);
			m->tipo = e[0x12];
			if (l >= 0x17)
				m->mts = u16le(e + 0x15);
			if (m->mts == 0xFFFF)
				m->mts = l >= 0x58 ? u32le(e + 0x54) : 0;
			if (l >= 0x18)
				cadena(e, f, e[0x17], m->fabricante, sizeof m->fabricante);	/* (0x18: el número de serie y 0x19 la etiqueta: no se leen) */
			fabricante_claro(m->fabricante, sizeof m->fabricante);
			if (l >= 0x1B)
				cadena(e, f, e[0x1A], m->pieza, sizeof m->pieza);
			if (l >= 0x22)
				m->mts_conf = u16le(e + 0x20);
			if (m->mts_conf == 0xFFFF)
				m->mts_conf = l >= 0x5C ? u32le(e + 0x58) : 0;
			if (m->mb && m->ancho_total != 0xFFFF && m->ancho_datos != 0xFFFF && m->ancho_total && m->ancho_datos)
				ancho_ecc = m->ancho_total > m->ancho_datos ? 1 : (ancho_ecc < 0 ? 0 : ancho_ecc);
		} else if (tipo == 20 && l >= 0x0F && s->nmapa < RA_MODULOS) {
			struct ra_mapa *a = &s->mapa[s->nmapa];
			ra_u32 ini = u32le(e + 4), fin = u32le(e + 8);

			a->handle = u16le(e + 0x0C);
			if (ini == 0xFFFFFFFFu && l >= 0x23) {
				a->ini = u64le(e + 0x13);
				a->fin = u64le(e + 0x1B);
			} else {
				a->ini = (ra_u64)ini << 10;
				a->fin = ((ra_u64)fin << 10) | 1023;
			}
			if (a->fin > a->ini)
				s->nmapa++;
		} else if (tipo == 127)
			break;
		o = (ra_u32)(f - t);
	}
	s->ecc = ecc16 >= 0 ? ecc16 : ancho_ecc;
}

const char *ra_tipo_memoria(unsigned tipo)
{
	switch (tipo) {
	case 0x0F: return "SDRAM";
	case 0x12: return "DDR";
	case 0x13: return "DDR2";
	case 0x14: return "DDR2 FB-DIMM";
	case 0x18: return "DDR3";
	case 0x1A: return "DDR4";
	case 0x1B: return "LPDDR";
	case 0x1C: return "LPDDR2";
	case 0x1D: return "LPDDR3";
	case 0x1E: return "LPDDR4";
	case 0x22: return "DDR5";
	case 0x23: return "LPDDR5";
	}
	return "";
}

int ra_modulo_de(const struct ra_smbios *s, ra_u64 fis)
{
	int i, j;

	for (i = 0; i < s->nmapa; i++)
		if (fis >= s->mapa[i].ini && fis <= s->mapa[i].fin)
			for (j = 0; j < s->nmod; j++)
				if (s->m[j].handle == s->mapa[i].handle && s->m[j].mb)
					return j;
	return -1;
}

char *ra_tam(char *d, ra_u64 o)
{
	ra_u64 mb = o >> 20;

	if (mb >= 1024) {
		ra_u64 dg = (o * 10 + (1ull << 29)) >> 30;

		if (dg % 10)
			snprintf(d, 24, "%llu,%llu GB", (unsigned long long)(dg / 10), (unsigned long long)(dg % 10));
		else
			snprintf(d, 24, "%llu GB", (unsigned long long)(dg / 10));
	} else
		snprintf(d, 24, "%llu MB", (unsigned long long)mb);
	return d;
}

int ra_texto_modulos(const struct ra_smbios *s, char *t, int cabe)
{
	char a[24];
	int k = 0, i, puestos = 0, vacias = 0;
	ra_u64 total = 0;

	if (cabe <= 0)
		return 0;
	t[0] = 0;
#define TX(...) do { if (k < cabe) k += snprintf(t + k, (size_t)(cabe - k), __VA_ARGS__); } while (0)
	if (!s->hay || !s->nmod) {
		TX("la BIOS no dice qué módulos hay (SMBIOS sin el tipo 17)");
		return k;
	}
	for (i = 0; i < s->nmod; i++)
		if (s->m[i].mb)
			puestos++, total += s->m[i].mb;
		else
			vacias++;
	TX("%d %s, %s en total", puestos, puestos == 1 ? "módulo" : "módulos", ra_tam(a, total << 20));
	for (i = 0; i < s->nmod; i++) {
		const struct ra_modulo *m = &s->m[i];

		if (!m->mb)
			continue;
		TX("; «%s%s%s»: %s", m->ranura[0] ? m->ranura : "?", m->banco[0] ? " / " : "", m->banco, ra_tam(a, (ra_u64)m->mb << 20));
		if (ra_tipo_memoria(m->tipo)[0])
			TX(" %s", ra_tipo_memoria(m->tipo));
		if (m->mts_conf && m->mts && m->mts_conf != m->mts)
			TX(" a %u MT/s (puesta a %u)", m->mts, m->mts_conf);
		else if (m->mts || m->mts_conf)
			TX(" a %u MT/s", m->mts ? m->mts : m->mts_conf);
		if (m->fabricante[0])
			TX(", %s", m->fabricante);
		if (m->pieza[0])
			TX(" %s", m->pieza);
	}
	if (vacias)
		TX("; %d %s", vacias, vacias == 1 ? "ranura vacía" : "ranuras vacías");
	if (s->ecc == 1)
		TX("; con ECC (corrige errores: los corregidos los cuenta el procesador)");
	else if (s->ecc == 0)
		TX("; sin ECC (lo normal en un portátil: el procesador no ve los fallos de la RAM, solo esta prueba)");
#undef TX
	return k;
}
