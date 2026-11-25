/* prueba_xmm.c - XMM.BIN: prueba que cada programa de Hispa-Pentium tiene su
 * propio SSE (los 8 registros XMM y el MXCSR, con el x87 a la vez: el núcleo
 * los guarda con FXSAVE y los repone con FXRSTOR al cambiar de tarea,
 * tareas.cas, «cambia_coma»; desde la 0.8.12). Como X87.BIN (prueba_x87.c):
 * se lanza a sí mismo dos veces («XMM HIJO»: uno suelto y otro al que se
 * espera) y los tres, a la vez, llenan los XMM con lo suyo (distinto en cada
 * uno), ponen su MXCSR (cada uno con su redondeo) y un número en la pila del
 * x87, duermen un tic (así el núcleo cambia de tarea) y miran que todo sigue
 * igual. Al empezar, antes de tocar nada, miran que su SSE está limpio (los
 * XMM a 0 y MXCSR = 1F80h: nada de lo de otro programa). Y unas cuentas de
 * verdad (mulpd y addpd en bucle, entre sueño y sueño) que tienen que dar lo
 * mismo que sin dormir. Dice por el puerto serie «xmm: bien» o qué ha
 * fallado; acaba con 0 si todo ha ido bien. Sin SSE2 en el procesador (o con
 * un núcleo que no lo enciende) dice «xmm: sin SSE2» y acaba con 0.
 * (castel/pruebas.sh, «el Navegador», junto a X87.BIN)
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdint.h>
#include "pentium.h"

static void di(const char *t)
{
	uint32_t n = 0;
	while (t[n]) n++;
	sc(SC_SERIE, (uint32_t)t, n, 0);
}

static uint32_t entra[32] __attribute__((aligned(16)));
static uint32_t sale[32] __attribute__((aligned(16)));

/* los 8 XMM y el MXCSR, a la memoria */
static void lee_sse(uint32_t *x, uint32_t *mx)
{
	__asm__ volatile("movdqa %%xmm0, 0(%0)\n\tmovdqa %%xmm1, 16(%0)\n\tmovdqa %%xmm2, 32(%0)\n\tmovdqa %%xmm3, 48(%0)\n\t"
			 "movdqa %%xmm4, 64(%0)\n\tmovdqa %%xmm5, 80(%0)\n\tmovdqa %%xmm6, 96(%0)\n\tmovdqa %%xmm7, 112(%0)\n\tstmxcsr %1"
			 : : "r"(x), "m"(*mx) : "memory");
}

/* pone lo de «entra» en los XMM, su MXCSR y d en el x87; duerme un tic y
 * lo lee otra vez: cierto si sigue todo igual */
static int aguanta(uint32_t base, uint32_t mx, double d)
{
	uint32_t i, mx2 = 0, r;
	double d2 = 0;

	for (i = 0; i < 32; i++)
		entra[i] = base * 0x01000193u ^ (i * 0x9E3779B9u);
	__asm__ volatile("movdqa 0(%3), %%xmm0\n\tmovdqa 16(%3), %%xmm1\n\tmovdqa 32(%3), %%xmm2\n\tmovdqa 48(%3), %%xmm3\n\t"
			 "movdqa 64(%3), %%xmm4\n\tmovdqa 80(%3), %%xmm5\n\tmovdqa 96(%3), %%xmm6\n\tmovdqa 112(%3), %%xmm7\n\t"
			 "ldmxcsr %4\n\tfldl %5\n\t"
			 "int $0x80\n\t"
			 "fstpl %2\n\tstmxcsr %1\n\t"
			 "movdqa %%xmm0, 0(%6)\n\tmovdqa %%xmm1, 16(%6)\n\tmovdqa %%xmm2, 32(%6)\n\tmovdqa %%xmm3, 48(%6)\n\t"
			 "movdqa %%xmm4, 64(%6)\n\tmovdqa %%xmm5, 80(%6)\n\tmovdqa %%xmm6, 96(%6)\n\tmovdqa %%xmm7, 112(%6)"
			 : "=a"(r), "=m"(mx2), "=m"(d2)
			 : "S"(entra), "m"(mx), "m"(d), "D"(sale), "a"(SC_DORMIR), "b"(1), "c"(0), "d"(0)
			 : "memory");
	(void)r;
	for (i = 0; i < 32; i++)
		if (sale[i] != entra[i])
			return 0;
	return mx2 == mx && d2 == d;
}

/* unas cuentas de verdad: x := x * a + c, n veces, en xmm0 (dos dobles), con
 * el redondeo de mx; si «duerme», durmiendo un tic cada 50 */
static void cuenta(uint32_t mx, int duerme, double *res)
{
	static double ab[4] __attribute__((aligned(16))) = { 0.999, 0.9995, 0.001, 0.0015 };
	static double x[2] __attribute__((aligned(16)));
	int k;

	x[0] = 1.0 / 3.0;
	x[1] = 2.0 / 7.0;
	__asm__ volatile("ldmxcsr %0\n\tmovapd (%1), %%xmm0\n\tmovapd (%2), %%xmm1\n\tmovapd 16(%2), %%xmm2" : : "m"(mx), "r"(x), "r"(ab) : "memory");
	for (k = 0; k < 1000; k++) {
		__asm__ volatile("mulpd %%xmm1, %%xmm0\n\taddpd %%xmm2, %%xmm0" : : : "memory");
		if (duerme && k % 50 == 0) {
			uint32_t r;
			__asm__ volatile("int $0x80" : "=a"(r) : "a"(SC_DORMIR), "b"(1), "c"(0), "d"(0) : "memory");
		}
	}
	__asm__ volatile("movapd %%xmm0, (%0)" : : "r"(x) : "memory");
	res[0] = x[0];
	res[1] = x[1];
}

int navega_principal(void)
{
	char args[64];
	uint32_t n = sc(SC_ARGUMENTOS, (uint32_t)args, sizeof args, 0), hijo = 0, k, mal = 0, limpio = 1, mx0 = 0, base, mx;
	uint32_t r[4];
	int esh = !ES_ERROR(n) && n >= 4 && args[0] == 'H';
	double a[2], b[2];

	__asm__ volatile("cpuid" : "=a"(r[0]), "=b"(r[1]), "=c"(r[2]), "=d"(r[3]) : "a"(1), "c"(0));
	if ((r[3] & 0x05000000u) != 0x05000000u || !(sc(224, 8, 0, 0) & 0x100) || !(sc(224, 8, 0, 0) & 4)) {
		if (!esh)
			di("xmm: sin SSE2 (el procesador no lo tiene o el n\xa3" "cleo no lo enciende)\n");
		return 0;
	}
	/* antes de nada: limpio (lo de otro programa no se ve) */
	lee_sse(sale, &mx0);
	for (k = 0; k < 32; k++)
		if (sale[k])
			limpio = 0;
	if (mx0 != 0x1F80)
		limpio = 0;
	if (esh) {
		base = 1000 + (uint32_t)args[5];	/* «HIJO1» y «HIJO2» */
		mx = args[5] == '1' ? 0x3F80 : 0x5F80;	/* hacia abajo, hacia arriba */
	} else {
		base = 3;
		mx = 0x7F80;		/* hacia el cero */
		hijo = sc(SC_LANZAR_SUELTO, (uint32_t)"XMM.BIN", (uint32_t)"HIJO1", 0);
		if (ES_ERROR(hijo)) { di("xmm: no se puede lanzar el hijo\n"); return 2; }
		hijo = sc(SC_LANZAR, (uint32_t)"XMM.BIN", (uint32_t)"HIJO2", 0);	/* (y otro, al que se espera) */
	}
	for (k = 0; k < 150; k++)
		if (!aguanta(base + k, mx, (double)(base + k) / 3.0))
			mal++;
	/* las cuentas, durmiendo y sin dormir: lo mismo, octeto a octeto */
	cuenta(mx, 1, a);
	cuenta(mx, 0, b);
	if (a[0] != b[0] || a[1] != b[1])
		mal += 1000;
	if (esh) {
		if (mal || !limpio)
			di(args[5] == '1' ? "xmm: MAL en el hijo 1 (el suelto)\n" : "xmm: MAL en el hijo 2\n");
		return mal || !limpio ? 1 : 0;
	}
	n = sc(11, hijo, 0, 0);		/* esperar al hijo */
	if (!limpio) di("xmm: MAL: al empezar, los XMM o el MXCSR no estaban limpios (lo de otro programa)\n");
	else if (mal >= 1000) di("xmm: MAL: las cuentas con SSE dan otra cosa si el programa duerme entre medias\n");
	else if (mal) di("xmm: MAL: los XMM, el MXCSR o el x87 han cambiado mientras dorm\xa1" "a\n");
	else if (n != 0) di("xmm: MAL: a un hijo le ha cambiado su SSE (o no estaba limpio al empezar)\n");
	else di("xmm: bien (tres programas a la vez, cada uno con sus XMM, su MXCSR y su x87)\n");
	return mal || n || !limpio ? 1 : 0;
}
