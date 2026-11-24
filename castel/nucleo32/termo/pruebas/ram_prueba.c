/* ram_prueba.c - (TERMO, RAM) en el anfitrión: los patrones de ram_alg.c con
 * fallos metidos a propósito (-DRA_SIMULA: la memoria pasa por
 * ra_simula_lee/escribe): un bit pegado a 1 en unas cuantas palabras, una
 * celda que es otra (las líneas de dirección), bits sueltos en un mismo
 * octeto (un chip), fallos dispersos y solo con todos los núcleos; que cada
 * prueba lo vea, que se agrupe bien y lo que dice. Y el SMBIOS: una tabla
 * hecha aquí con dos módulos (y sus números de serie, que NO pueden salir).
 * cc -DRA_SIMULA -I.. ram_prueba.c ../ram_alg.c -o ram_prueba && ./ram_prueba
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ram_alg.h"

#define N (1u << 20)	/* 4 MB */
static ra_u32 *mem;
static int malos;

/* ---- la memoria de mentira ---- */
static struct { ra_u32 i, mascara, valor; } pegado[64];
static int npegados;
static ra_u32 alias_de = 0xFFFFFFFF, alias_a;	/* p[alias_de] es la misma celda que p[alias_a] */

static ra_u32 indice(volatile ra_u32 *p)
{
	ra_u32 i = (ra_u32)((ra_u32 *)p - mem);

	return i == alias_de ? alias_a : i;
}

ra_u32 ra_simula_lee(volatile ra_u32 *p)
{
	ra_u32 i = indice(p), v = mem[i];
	int k;

	for (k = 0; k < npegados; k++)
		if (pegado[k].i == i)
			v = (v & ~pegado[k].mascara) | pegado[k].valor;
	return v;
}

void ra_simula_escribe(volatile ra_u32 *p, ra_u32 v)
{
	mem[indice(p)] = v;
}

static void limpia(void)
{
	npegados = 0;
	alias_de = 0xFFFFFFFF;
}

static void pega(ra_u32 i, ra_u32 mascara, ra_u32 valor)
{
	pegado[npegados].i = i;
	pegado[npegados].mascara = mascara;
	pegado[npegados++].valor = valor;
}

/* ---- lo que apunta TERMO.BIN de cada fallo: aquí, la física = 1 GB + el desplazamiento ---- */
static struct ra_fallos F;
static int todos_ahora;

static void fallo(void *ctx, volatile ra_u32 *d, ra_u32 e, ra_u32 l, int prueba)
{
	(void)ctx;
	ra_apunta(&F, 0x40000000ull + (ra_u64)((ra_u32 *)d - mem) * 4, 1, e, l, prueba, todos_ahora);
}

static void ok(int bien, const char *que)
{
	printf("%s %s\n", bien ? "bien:" : "MAL: ", que);
	if (!bien)
		malos++;
}

/* todas las pruebas de un núcleo, como las hace ram.c */
static ra_u64 pasa_todo(struct ra_ctx *c, ra_u64 *por)
{
	ra_u64 antes = c->fallos;

	por[RA_LINEAS] = ra_lineas(c, mem, N);
	por[RA_DIRECCION] = ra_direccion(c, mem, N);
	por[RA_ANDANTES] = ra_andantes(c, mem, N, 0, 1) + ra_andantes(c, mem, N, 0, 0);
	por[RA_PATRON] = ra_patron(c, mem, N, 0x55555555u);
	por[RA_INVERSIONES] = ra_inversiones(c, mem, N, 0x12345678u);
	return c->fallos - antes;
}

static int contiene(const char *t, const char *o) { return strstr(t, o) != NULL; }

int main(void)
{
	struct ra_ctx c;
	struct ra_diag d;
	ra_u64 por[RA_NPRUEBAS], n;
	char t[2000];
	ra_u32 x, i;
	int k, bien;

	mem = malloc(N * 4);
	if (!mem)
		return 2;
	memset(&c, 0, sizeof c);
	c.fallo = fallo;

	/* el azar hacia atrás */
	for (bien = 1, x = 1, i = 0; i < 100000; i++, x = ra_sig(x))
		if (ra_ant(ra_sig(x)) != x)
			bien = 0;
	ok(bien, "el azar (x * 1664525 + 1013904223) vuelve atrás bien (el inverso de 1664525 es FEE058C5h)");

	/* 1. sin fallos */
	limpia();
	ra_fallos_empieza(&F);
	n = pasa_todo(&c, por);
	ra_diagnostica(&F, &d);
	ok(n == 0 && d.clase == RA_D_BIEN, "sin fallos: ni uno en ninguna prueba, y el diagnóstico es BIEN");

	/* 2. el bit 5 pegado a 1 en 6 palabras sueltas (todas del carril bajo: su dirección acaba en 0 u 8) */
	limpia();
	for (k = 0; k < 6; k++)
		pega(1000 + 2 * 37717 * (ra_u32)k, 1u << 5, 1u << 5);
	ra_fallos_empieza(&F);
	n = pasa_todo(&c, por);
	ra_diagnostica(&F, &d);
	ra_diag_texto(&F, &d, t, sizeof t);
	ok(por[RA_DIRECCION] > 0 && por[RA_ANDANTES] > 0 && por[RA_PATRON] > 0 && por[RA_INVERSIONES] > 0,
	   "el bit pegado lo ven la dirección propia, los andantes, 0x55/0xAA y moving inversions");
	ok(d.clase == RA_D_BIT && d.bit == 5 && d.a_uno == 1, "se agrupa como «el mismo bit» (D5, que se queda a 1): un chip");
	ok(contiene(t, "MISMO BIT") && contiene(t, "D5 ") && contiene(t, "que se queda a 1") && contiene(t, "UN CHIP"), "y lo dice en palabras");
	printf("      %s\n", t);
	for (bien = 0, k = 0; k < F.ndet; k++)
		if (F.det[k].fis == 0x40000000ull + 1000 * 4 && F.det[k].fis_sabida && (F.det[k].esperado ^ F.det[k].leido) == (1u << 5) && (F.det[k].leido & (1u << 5)))
			bien = 1;
	ok(bien,
	   "el primer fallo: su dirección física (40000FA0h), lo esperado y lo leído, y el bit que falla");
	/* (el mismo bit en el carril alto: las palabras de dirección ...4/...C son los bits 32-63 del bus) */
	limpia();
	pega(2001, 1u << 3, 0);
	pega(4001, 1u << 3, 0);
	ra_fallos_empieza(&F);
	pasa_todo(&c, por);
	ra_diagnostica(&F, &d);
	ok(d.clase == RA_D_BIT && d.bit == 35 && d.a_uno == 0, "el bit 3 de las palabras del carril alto, pegado a 0: el D35 del bus, que se queda a 0");

	/* 3. una celda que es otra: p[4096] es p[0] (la línea de dirección 14 pegada) */
	limpia();
	alias_de = 4096;
	alias_a = 0;
	ra_fallos_empieza(&F);
	n = pasa_todo(&c, por);
	ra_diagnostica(&F, &d);
	ra_diag_texto(&F, &d, t, sizeof t);
	ok(por[RA_LINEAS] > 0 && por[RA_DIRECCION] > 0, "una celda que es otra: la ven las líneas de dirección y la dirección propia");
	ok(d.clase == RA_D_LINEAS && contiene(t, "MAL ASENTADO"), "y se agrupa como las líneas de dirección (módulo mal asentado, ranura)");

	/* 4. un chip: bits distintos del mismo octeto (D8-D15) en palabras distintas */
	limpia();
	for (k = 0; k < 8; k++)
		pega(50000 + 2 * 1999 * (ra_u32)k, 1u << (8 + k), 0);
	ra_fallos_empieza(&F);
	pasa_todo(&c, por);
	ra_diagnostica(&F, &d);
	ok(d.clase == RA_D_CHIP && d.bit == 8, "bits distintos del mismo octeto (D8-D15): un chip de 8 bits");

	/* 5. dispersos: bits distintos en zonas distintas (por todo el trozo, y más allá de 16 MB de física) */
	ra_fallos_empieza(&F);
	for (k = 0; k < 40; k++)
		ra_apunta(&F, 0x100000000ull + (ra_u64)k * 0x2340000ull, 1, 0, 1u << (k % 29), RA_INVERSIONES, 0);
	ra_diagnostica(&F, &d);
	ra_diag_texto(&F, &d, t, sizeof t);
	ok(d.clase == RA_D_DISPERSO && contiene(t, "DISPERSOS"), "fallos dispersos (bits y zonas distintos): controlador, calor, tensión");

	/* 6. solo con todos los núcleos */
	ra_fallos_empieza(&F);
	for (k = 0; k < 40; k++)
		ra_apunta(&F, 0x80000000ull + (ra_u64)k * 0x3000000ull, 1, 0xFFFFFFFFu, ~(1u << (k % 31)), RA_TODOS, 1);
	ra_diagnostica(&F, &d);
	ra_diag_texto(&F, &d, t, sizeof t);
	ok(d.clase == RA_D_CALOR && contiene(t, "TODOS LOS NÚCLEOS"), "solo con todos los núcleos a la vez: el calor, el controlador o la tensión");

	/* 7. una zona pequeña: varios bits, en 1 MB */
	limpia();
	pega(300000, 0x00010001u, 0x00010001u);
	pega(300100, 0x10000000u, 0);
	pega(300203, 0x00000400u, 0x400);
	ra_fallos_empieza(&F);
	pasa_todo(&c, por);
	ra_diagnostica(&F, &d);
	ok(d.clase == RA_D_ZONA, "varios bits distintos en una zona pequeña: celdas de un módulo");

	/* 8. parar a mitad */
	{
		static int veces;
		int para(void *x) { (void)x; return ++veces > 1; }

		limpia();
		c.parar = para;
		c.parado = 0;
		ra_inversiones(&c, mem, N, 1);
		ok(c.parado == 1, "se para a mitad si se le pide (Esc)");
		c.parar = 0;
		c.parado = 0;
	}

	/* 9. el SMBIOS: tipo 16 (2 ranuras, sin ECC), dos del 17 y dos del 20 */
	{
		static unsigned char tb[1024];
		struct ra_smbios s;
		int o = 0, j;

#define B(v) (tb[o++] = (unsigned char)(v))
#define W(v) (B((v) & 0xFF), B(((v) >> 8) & 0xFF))
#define D(v) (W((v) & 0xFFFF), W(((unsigned)(v) >> 16) & 0xFFFF))
		/* 16 */
		B(16); B(0x17); W(0x1000); B(3); B(3); B(3); D(16u << 20); W(0xFFFE); W(2); for (j = 0; j < 8; j++) B(0);
		B(0); B(0);
		for (k = 0; k < 2; k++) {
			int ini = o;

			B(17); B(0x28); W(0x1100 + k); W(0x1000); W(0xFFFE); W(64); W(64); W(8192); B(0x0D); B(0);
			B(1); B(2); B(0x18); W(0x80); W(1600); B(3); B(4); B(5); B(6); B(0); D(0); W(1600);
			while (o < ini + 0x28)
				B(0);
			memcpy(tb + o, k ? "ChannelB-DIMM0" : "ChannelA-DIMM0", 15); o += 15;
			memcpy(tb + o, k ? "BANK 2" : "BANK 0", 7); o += 7;
			memcpy(tb + o, k ? "Kingston" : "80CE", k ? 9 : 5); o += k ? 9 : 5;
			memcpy(tb + o, "SERIE-SECRETA-123", 18); o += 18;
			memcpy(tb + o, "ETIQUETA-99", 12); o += 12;
			memcpy(tb + o, k ? "KVR16S11/8" : "M471B1G73DB0-YK0", k ? 11 : 17); o += k ? 11 : 17;
			B(0);
		}
		for (k = 0; k < 2; k++) {
			B(20); B(0x13); W(0x1400 + k); D(k ? 8u << 20 : 0); D(k ? (16u << 20) - 1 : (8u << 20) - 1); W(0x1100 + k); W(0x1300); B(1); B(0xFF); B(0xFF);
			B(0); B(0);
		}
		B(127); B(4); W(0xFFFF); B(0); B(0);
		ra_lee_smbios(tb, (ra_u32)o, &s);
		ra_texto_modulos(&s, t, sizeof t);
		printf("      %s\n", t);
		ok(s.nmod == 2 && s.m[0].mb == 8192 && s.m[1].mb == 8192 && s.ranuras == 2 && s.ecc == 0, "SMBIOS: 2 módulos de 8 GB, 2 ranuras, sin ECC");
		ok(contiene(t, "ChannelA-DIMM0") && contiene(t, "DDR3") && contiene(t, "1600 MT/s") && contiene(t, "Samsung") && contiene(t, "Kingston KVR16S11/8"),
		   "SMBIOS: las ranuras, DDR3 a 1600 MT/s, el fabricante (80CE: Samsung) y el modelo");
		ok(!contiene(t, "SERIE") && !contiene(t, "ETIQUETA"), "SMBIOS: ni el número de serie ni la etiqueta");
		ok(ra_modulo_de(&s, 0x40000000ull) == 0 && ra_modulo_de(&s, 0x240000000ull) == 1 && ra_modulo_de(&s, 0x500000000ull) < 0,
		   "SMBIOS (tipo 20): 1 GB cae en ChannelA-DIMM0, 9 GB en ChannelB-DIMM0, 20 GB en ninguno");
	}
	printf("%s\n", malos ? "ram_prueba: HAY FALLOS" : "ram_prueba: todo bien");
	free(mem);
	return malos ? 1 : 0;
}
