/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/musl/comun.h - Hispa-64, etapa 7: lo de todas las
 * pruebas con musl (docs/64/HISPA-64.md, sección 13): contar lo que sale
 * bien y decir lo que sale mal, y cómo se ha hecho el programa. */
#ifndef COMUN_H
#define COMUN_H
#include <stdio.h>
#include <string.h>
#include <elf.h>
#include <sys/auxv.h>

static volatile int bien_, mal_;      /* (volatile: con siglongjmp de por medio) */
#define MIRA(c, d) do { if (c) bien_++; else { mal_++; printf("  MAL: %s (línea %d)\n", (d), __LINE__); fflush(stdout); } } while (0)

extern char __ehdr_start[];
/* estático, dinámico (con su cargador) o estático de posición libre */
static const char *cómo(void)
{
	static char t[120];
	unsigned long base = getauxval(AT_BASE);
	if (base) snprintf(t, sizeof t, "dinámico: ld-musl en 0x%lx, el programa en 0x%lx", base, (unsigned long)__ehdr_start);
	else if ((unsigned long)__ehdr_start != 0x400000) snprintf(t, sizeof t, "estático de posición libre, en 0x%lx", (unsigned long)__ehdr_start);
	else snprintf(t, sizeof t, "estático");
	return t;
}

static int acaba(const char *quién)
{
	if (mal_) printf("%s: %d mal de %d\n", quién, mal_, bien_ + mal_);
	else printf("%s: todo bien (%d pruebas)\n", quién, bien_);
	fflush(stdout);
	return mal_ ? 1 : 0;
}
#endif
