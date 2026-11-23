/* cuenta.c - Hispa-64, etapa 4 (docs/64/HISPA-64.md, sección 10): para correr
 * varios a la vez. «CUENTA k» hace la misma cuenta dos veces (con enteros y
 * con coma flotante, en los registros SSE): una de un tirón y otra cediendo
 * el procesador (sched_yield) y durmiendo a ratos; si el núcleo no guardase
 * bien sus registros al cambiar de programa (los SSE, FS), no darían lo
 * mismo. Acaba con el código 10 * k si salen iguales.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "sis.h"

static u64 tls[2];

static void cuenta(u64 k, int cediendo, u64 *ent, double *fl)
{
	u64 i, a = k;
	double d = 0;
	u64 ts[2] = { 0, 2000000 };
	for (i = 1; i <= 300000; i++) {
		a = a * 6364136223846793005UL + i * k;
		d += 1.0 / (double)(i + k);
		if (cediendo && i % 30000 == 0) {
			sis(S_YIELD, 0, 0, 0);
			if (i % 90000 == 0) sis(S_NANOSLEEP, ts, 0, 0);
		}
	}
	*ent = a;
	*fl = d;
}

int principal(i64 *sp)
{
	char **argv = (char **)(sp + 1);
	u64 k = 1, e1, e2, fs;
	double f1, f2;
	if (sp[0] > 1 && argv[1][0] >= '0' && argv[1][0] <= '9') k = argv[1][0] - '0';
	tls[0] = k * 1111;
	sis(S_ARCH_PRCTL, 0x1002, tls, 0);
	cuenta(k, 0, &e1, &f1);
	cuenta(k, 1, &e2, &f2);
	__asm__ volatile ("mov %%fs:0, %0" : "=r"(fs));
	pon("CUENTA "); pon_n(k); pon(": proceso "); pon_n(sis(S_GETPID, 0, 0, 0)); pon(", ");
	pon_hex(e2); pon(", "); pon_n((i64)(f2 * 1000000.0));
	if (e1 == e2 && f1 == f2 && fs == k * 1111) { pon(", igual en las dos vueltas\n"); return (int)(10 * k); }
	pon(", MAL: no da lo mismo cediendo el procesador\n");
	return 1;
}
