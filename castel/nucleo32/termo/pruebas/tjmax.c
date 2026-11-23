/* tjmax.c - (TERMO) en el anfitrión: la tabla de la TjMax de veredicto.c con
 * los procesadores que se sabe cómo son (Intel; la de coretemp de Linux).
 * Dice «bien: ...» o «mal: ...» por cada uno.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdio.h>
#include "veredicto.h"

int main(void)
{
	static const struct { int fam, mod, paso, ee; const char *marca; int tj; } c[] = {
		{ 6, 0x1C, 10, 0, "Intel(R) Atom(TM) CPU N455   @ 1.66GHz", 100 },	/* HP Mini 5103 */
		{ 6, 0x1C, 10, 0, "Intel(R) Atom(TM) CPU D525   @ 1.80GHz", 100 },
		{ 6, 0x1C, 2, 0, "Intel(R) Atom(TM) CPU N270   @ 1.60GHz", 90 },
		{ 6, 0x1C, 2, 0, "Intel(R) Atom(TM) CPU N280   @ 1.66GHz", 90 },
		{ 6, 0x1C, 2, 0, "Intel(R) Atom(TM) CPU Z520   @ 1.33GHz", 90 },
		{ 6, 0x1C, 2, 0, "Intel(R) Atom(TM) CPU  230   @ 1.60GHz", 100 },
		{ 6, 0x1C, 2, 0, "Intel(R) Atom(TM) CPU  330   @ 1.60GHz", 125 },
		{ 6, 0x36, 1, 0, "Intel(R) Atom(TM) CPU N2600   @ 1.60GHz", 100 },
		{ 6, 0x26, 1, 0, "Intel(R) Atom(TM) CPU Z670   @ 1.50GHz", 90 },
		{ 6, 0x17, 10, 0, "Intel(R) Core(TM)2 Duo CPU     T6600  @ 2.20GHz", 100 },
		{ 6, 0x17, 10, 1, "Intel(R) Core(TM)2 Duo CPU     E8400  @ 3.00GHz", 85 },
		{ 6, 0x0E, 8, 0, "Genuine Intel(R) CPU           T2300  @ 1.66GHz", 100 },
	};
	unsigned i;
	int mal = 0;

	for (i = 0; i < sizeof c / sizeof c[0]; i++) {
		int t = ve_tjmax_intel(c[i].fam, c[i].mod, c[i].paso, c[i].marca, c[i].ee);

		printf("%s: TjMax de «%s» (06_%02Xh paso %d): %d °C%s\n", t == c[i].tj ? "bien" : "mal", c[i].marca, c[i].mod, c[i].paso, t,
		       t == c[i].tj ? "" : " (debía ser otra)");
		mal |= t != c[i].tj;
	}
	return mal;
}
