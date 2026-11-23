/* memoria.c - Hispa-64, etapa 4 (docs/64/HISPA-64.md, sección 10): brk, mmap,
 * munmap y mprotect, y que las páginas se ponen al tocarlas. Acaba con 0 si
 * todo ha ido bien.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "sis.h"

#define MAPA(d, l, p, f) sis6(S_MMAP, (i64)(d), (i64)(l), (p), (f), -1, 0)

int principal(i64 *sp)
{
	u64 b0, i, suma, a, a2;
	unsigned char *p;
	(void)sp;

	/* brk: crecer, escribir, menguar, no pasarse */
	b0 = sis(S_BRK, 0, 0, 0);
	mira(b0 > 0x400000 && (b0 & 4095) == 0, "brk(0)");
	mira(sis(S_BRK, b0 + 100000, 0, 0) == (i64)(b0 + 100000), "brk: crecer 100000");
	p = (unsigned char *)b0;
	for (i = 0; i < 100000; i++) p[i] = (unsigned char)i;
	for (i = 0, suma = 0; i < 100000; i++) suma += p[i];
	mira(suma == 12742320, "brk: lo escrito");
	mira(sis(S_BRK, b0 + 5000, 0, 0) == (i64)(b0 + 5000), "brk: menguar");
	mira(p[4999] == (unsigned char)4999, "brk: lo que queda");
	mira(sis(S_BRK, b0 - 4096, 0, 0) == (i64)(b0 + 5000), "brk por debajo del principio (no)");
	mira(sis(S_BRK, b0 + (2UL << 30), 0, 0) == (i64)(b0 + 5000), "brk de 2 GB (no: como mucho 1 GB)");
	mira(sis(S_BRK, b0 + 200000, 0, 0) == (i64)(b0 + 200000) && p[150000] == 0, "brk: crecer otra vez (a cero)");
	pon("  brk: desde "); pon_hex(b0); pon(", crece, mengua y vuelve a crecer (a cero)\n");

	/* mmap de 1 MB, escribir, quitar un trozo del medio, ponerlo otra vez fijo */
	a = MAPA(0, 1 << 20, 3, 0x22);
	mira(a > 0 && (a & 4095) == 0, "mmap de 1 MB");
	p = (unsigned char *)a;
	for (i = 0; i < (1 << 20); i += 4096) p[i] = (unsigned char)(i >> 12);
	for (i = 0, suma = 0; i < (1 << 20); i += 4096) suma += p[i];
	mira(suma == 32640, "mmap: lo escrito");
	mira(sis(S_MUNMAP, a + 0x40000, 0x40000, 0) == 0, "munmap del medio");
	mira(p[0x3F000] == 0x3F && p[0x80000] == 0x80, "lo de los lados sigue");
	a2 = MAPA(a + 0x40000, 0x40000, 3, 0x32);
	mira(a2 == a + 0x40000 && p[0x40000] == 0, "mmap fijo en el hueco (a cero)");
	mira(sis(S_MPROTECT, a, 4096, 1) == 0, "mprotect de solo leer");
	mira(p[0] == 0, "leer lo de solo leer");
	mira(sis(S_MPROTECT, a, 4096, 3) == 0, "mprotect otra vez para escribir");
	p[0] = 77;
	mira(p[0] == 77, "escribir otra vez");
	mira(sis(S_MUNMAP, a, 1 << 20, 0) == 0, "munmap de todo");
	pon("  mmap: 1 MB puesto, escrito, un trozo quitado y vuelto a poner fijo, mprotect y munmap\n");

	/* 64 MB, solo se tocan 64 páginas */
	a = MAPA(0, 64 << 20, 3, 0x22);
	mira(a > 0, "mmap de 64 MB");
	for (i = 0; i < (64 << 20); i += 1 << 20) ((unsigned char *)a)[i] = 1;
	mira(sis(S_MUNMAP, a, 64 << 20, 0) == 0, "munmap de 64 MB");
	a = MAPA(0, 8192, 3, 0x62);
	mira(a > 0 && a < 0x80000000UL, "MAP_32BIT");
	mira(sis(S_MUNMAP, a, 8192, 0) == 0, "munmap de MAP_32BIT");
	pon("  64 MB pedidos y 64 p\xc3\xa1ginas tocadas (se ponen al tocarlas); MAP_32BIT en "); pon_hex(a); pon("\n");

	/* los errores */
	mira(MAPA(0, 0, 3, 0x22) == -EINVAL, "mmap de 0 octetos");
	mira(MAPA(0, 4096, 3, 0x20) == -EINVAL, "mmap sin MAP_PRIVATE ni MAP_SHARED");
	mira(MAPA(0, 4096, 9, 0x22) == -EINVAL, "mmap con permisos que no hay");
	mira(MAPA(0, 1UL << 41, 3, 0x22) == -ENOMEM, "mmap de 2 TB");
	mira(MAPA(0, 4096, 3, 0x02) == -EBADF, "mmap de un archivo que no est\xc3\xa1 abierto");
	mira(sis(S_MUNMAP, 0x10001, 4096, 0) == -EINVAL, "munmap sin alinear");
	mira(sis(S_MPROTECT, 0x0000600000000000UL, 4096, 1) == -ENOMEM, "mprotect de lo que no hay");
	if (fallos == 0) { pon("MEMORIA: "); pon_n(pruebas); pon(" pruebas, todas bien\n"); }
	else { pon("MEMORIA: "); pon_n(fallos); pon(" MAL\n"); }
	return fallos;
}
