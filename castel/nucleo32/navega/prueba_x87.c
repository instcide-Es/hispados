/* prueba_x87.c - X87.BIN: prueba que cada programa de Hispa-Pentium tiene su
 * propia coma flotante (el x87: el núcleo lo guarda y lo repone al cambiar de
 * tarea, tareas.cas, «sin_x87»). Se lanza a sí mismo («X87 HIJO») y los dos, a
 * la vez, dejan números distintos en la pila del x87 y, durmiendo un tic tras
 * otro (así el núcleo cambia de tarea), miran que siguen allí y que las
 * cuentas dan lo de siempre. Dice por el puerto serie «x87: bien» o qué ha
 * fallado; acaba con 0 si todo ha ido bien. (castel/pruebas.sh, «el Navegador»)
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdint.h>
#include "pentium.h"

static void di(const char *t)
{
	uint32_t n = 0;
	while (t[n]) n++;
	sc(SC_SERIE, (uint32_t)t, n, 0);
}

/* deja a en st(1) y b en st(0), duerme y los lee: cierto si siguen igual */
static int aguanta(double a, double b)
{
	double x, y;
	uint32_t r;
	/* (todo en uno: el compilador no sabe que la pila del x87 queda ocupada) */
	__asm__ volatile("fldl %3\n\tfldl %4\n\tint $0x80\n\tfstpl %1\n\tfstpl %2"
			 : "=a"(r), "=m"(y), "=m"(x) : "m"(a), "m"(b), "a"(SC_DORMIR), "b"(1), "c"(0), "d"(0) : "memory");
	(void)r;
	return x == a && y == b;
}

int navega_principal(void)
{
	char args[64];
	uint32_t n = sc(SC_ARGUMENTOS, (uint32_t)args, sizeof args, 0), hijo = 0, k, mal = 0;
	volatile double s = 0, base;
	if (!ES_ERROR(n) && n >= 4 && args[0] == 'H') {
		base = 1000.0;
	} else {
		base = 3.0;
		hijo = sc(SC_LANZAR_SUELTO, (uint32_t)"X87.BIN", (uint32_t)"HIJO", 0);
		if (ES_ERROR(hijo)) { di("x87: no se puede lanzar el hijo\n"); return 2; }
		hijo = sc(SC_LANZAR, (uint32_t)"X87.BIN", (uint32_t)"HIJO", 0);	/* (y otro, al que se espera) */
	}
	for (k = 0; k < 200; k++) {
		if (!aguanta(base + k, base * 0.5 + k)) mal++;
		s += (base + k) / 7.0;
	}
	/* la suma, con lo que sale en un PC: 200 términos */
	{
		volatile double esperado = 0;
		for (k = 0; k < 200; k++) esperado += (base + k) / 7.0;
		if (s != esperado) mal++;
	}
	if (base == 1000.0) return mal ? 1 : 0;
	n = sc(11, hijo, 0, 0);		/* esperar al hijo */
	if (mal) di("x87: MAL: la pila del x87 ha cambiado mientras dorm\xa1" "a\n");
	else if (n != 0) di("x87: MAL: al hijo le ha cambiado su x87\n");
	else di("x87: bien (dos programas a la vez, cada uno con su x87)\n");
	return mal || n ? 1 : 0;
}
