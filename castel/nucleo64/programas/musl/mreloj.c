/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/musl/mreloj.c - Hispa-64, etapa 10 f (docs/64/HISPA-64.md,
 * sección 23): MRELOJ (estático) y MRELOJD (dinámico: el cargador de musl
 * toma el vDSO como una biblioteca más), la prueba del vDSO y de la hora:
 *   - el vDSO está (AT_SYSINFO_EHDR): un ELF de posición libre con
 *     __vdso_clock_gettime, __vdso_gettimeofday y __vdso_time (buscados
 *     como hace musl: sus segmentos, DT_HASH, DT_SYMTAB y DT_STRTAB);
 *   - clock_gettime (MONOTONIC, REALTIME, BOOTTIME, MONOTONIC_RAW y los
 *     COARSE) por musl (el vDSO) frente a la llamada al sistema
 *     (syscall(SYS_clock_gettime)): lo mismo, con 5 ms como mucho entre uno
 *     y otro; gettimeofday y time, de acuerdo con REALTIME; los relojes del
 *     proceso y del hilo (CPUTIME), por la llamada;
 *   - 200.000 lecturas seguidas que nunca van hacia atrás, y 4 hilos a la vez
 *     (en procesadores distintos) que comparan lo suyo con lo último que ha
 *     leído cualquiera (con un cerrojo): ninguna hacia atrás;
 *   - nanosleep de 20 ms dura de 20 a 200 ms medido con el vDSO.
 * En Hispa-64 dice además si el vDSO va con el TSC (la página de los datos:
 * con_tsc) o llama al sistema, y lo que tarda cada lectura (eso, y lo del TSC,
 * no se compara con Linux). Al final, «MRELOJ: todo bien (N pruebas)». */
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include "comun.h"

static int64_t ns_de(struct timespec t) { return (int64_t)t.tv_sec * 1000000000 + t.tv_nsec; }

static int64_t por_musl(clockid_t c)
{
	struct timespec t;
	if (clock_gettime(c, &t)) return -1;
	return ns_de(t);
}

static int64_t por_llamada(clockid_t c)
{
	struct timespec t;
	if (syscall(SYS_clock_gettime, c, &t)) return -1;
	return ns_de(t);
}

/* el símbolo s del vDSO (como __vdsosym de musl), o 0 */
static void *del_vdso(const char *s)
{
	unsigned long e = getauxval(AT_SYSINFO_EHDR);
	if (!e) return 0;
	Elf64_Ehdr *eh = (void *)e;
	Elf64_Phdr *ph = (void *)(e + eh->e_phoff);
	unsigned long base = (unsigned long)-1;
	Elf64_Dyn *dyn = 0;
	for (int i = 0; i < eh->e_phnum; i++) {
		if (ph[i].p_type == PT_LOAD) base = e + ph[i].p_offset - ph[i].p_vaddr;
		else if (ph[i].p_type == PT_DYNAMIC) dyn = (void *)(e + ph[i].p_offset);
	}
	if (!dyn || base == (unsigned long)-1) return 0;
	char *nom = 0; Elf64_Sym *sim = 0; uint32_t *hash = 0;
	for (; dyn->d_tag; dyn++) {
		if (dyn->d_tag == DT_STRTAB) nom = (char *)(base + dyn->d_un.d_ptr);
		if (dyn->d_tag == DT_SYMTAB) sim = (void *)(base + dyn->d_un.d_ptr);
		if (dyn->d_tag == DT_HASH) hash = (void *)(base + dyn->d_un.d_ptr);
	}
	if (!nom || !sim || !hash) return 0;
	for (uint32_t i = 0; i < hash[1]; i++)
		if (sim[i].st_shndx && sim[i].st_name && !strcmp(nom + sim[i].st_name, s)) return (void *)(base + sim[i].st_value);
	return 0;
}

static pthread_mutex_t cerrojo = PTHREAD_MUTEX_INITIALIZER;
static int64_t último;
static int atrás_hilos;

static void *lector(void *a)
{
	(void)a;
	for (int i = 0; i < 20000; i++) {
		pthread_mutex_lock(&cerrojo);
		int64_t t = por_musl(CLOCK_MONOTONIC);
		if (t < último) atrás_hilos++;
		else último = t;
		pthread_mutex_unlock(&cerrojo);
	}
	return 0;
}

int main(void)
{
	struct utsname u;
	int hispa = uname(&u) == 0 && !strcmp(u.sysname, "Hispa-64");
	printf("MRELOJ: el vDSO y la hora (%s)\n", cómo());
	unsigned long e = getauxval(AT_SYSINFO_EHDR);
	MIRA(e != 0, "AT_SYSINFO_EHDR (el vDSO) está");
	MIRA(e && !memcmp((void *)e, "\177ELF", 4) && ((Elf64_Ehdr *)e)->e_type == ET_DYN, "el vDSO es un ELF de posición libre");
	int (*cgt)(clockid_t, struct timespec *) = (int (*)(clockid_t, struct timespec *))del_vdso("__vdso_clock_gettime");
	MIRA(cgt != 0, "el vDSO tiene __vdso_clock_gettime");
	MIRA(del_vdso("__vdso_gettimeofday") != 0, "el vDSO tiene __vdso_gettimeofday");
	MIRA(del_vdso("__vdso_time") != 0, "el vDSO tiene __vdso_time");
	printf("  el vDSO: __vdso_clock_gettime, __vdso_gettimeofday y __vdso_time\n");
	/* lo de musl (el vDSO) frente a la llamada, en cada reloj */
	static const clockid_t relojes[] = { CLOCK_MONOTONIC, CLOCK_REALTIME, CLOCK_BOOTTIME, CLOCK_MONOTONIC_RAW, CLOCK_REALTIME_COARSE, CLOCK_MONOTONIC_COARSE };
	static const char *nombres[] = { "MONOTONIC", "REALTIME", "BOOTTIME", "MONOTONIC_RAW", "REALTIME_COARSE", "MONOTONIC_COARSE" };
	int iguales = 0;
	for (int i = 0; i < 6; i++) {
		int64_t a = por_musl(relojes[i]), b = por_llamada(relojes[i]), c = por_musl(relojes[i]);
		int64_t d = 0;
		if (cgt) { struct timespec t; d = cgt(relojes[i], &t) ? -1 : ns_de(t); }
		/* (los COARSE de Linux van por tics: hasta 4 ms por detrás) */
		int bien = a > 0 && b > 0 && c > 0 && d > 0 && b >= a - 5000000 && c >= b - 5000000 && c - a < 5000000 && llabs(d - b) < 5000000;
		if (bien) iguales++;
		else printf("  MAL: %s: %lld %lld %lld %lld\n", nombres[i], (long long)a, (long long)b, (long long)c, (long long)d);
		MIRA(bien, "clock_gettime por musl, por el vDSO y por la llamada, lo mismo");
	}
	printf("  clock_gettime: %d relojes, lo mismo por musl, por el vDSO y por la llamada al sistema\n", iguales);
	/* REALTIME, gettimeofday y time */
	struct timeval tv;
	MIRA(gettimeofday(&tv, 0) == 0, "gettimeofday");
	int64_t r = por_musl(CLOCK_REALTIME);
	time_t t1 = time(0);
	MIRA(llabs((int64_t)tv.tv_sec * 1000000000 + tv.tv_usec * 1000 - r) < 5000000, "gettimeofday, como REALTIME");
	MIRA(t1 >= r / 1000000000 && t1 <= r / 1000000000 + 1, "time, como REALTIME");
	MIRA(r / 1000000000 > 1735689600, "REALTIME, después de 2025");
	MIRA(por_musl(CLOCK_PROCESS_CPUTIME_ID) >= 0 && por_musl(CLOCK_THREAD_CPUTIME_ID) >= 0, "los relojes del proceso y del hilo");
	MIRA(por_musl((clockid_t)99) < 0, "un reloj que no existe: error");
	printf("  gettimeofday y time, de acuerdo con REALTIME\n");
	/* seguidas, nunca hacia atrás */
	int atrás = 0;
	int64_t ant = por_musl(CLOCK_MONOTONIC), t0 = ant;
	for (int i = 0; i < 200000; i++) {
		int64_t t = por_musl(CLOCK_MONOTONIC);
		if (t < ant) atrás++;
		ant = t;
	}
	int64_t lleva = ant - t0;
	MIRA(atrás == 0, "200.000 lecturas seguidas, ninguna hacia atrás");
	int64_t l0 = por_musl(CLOCK_MONOTONIC);
	for (int i = 0; i < 2000; i++) por_llamada(CLOCK_MONOTONIC);
	int64_t lleva_l = por_musl(CLOCK_MONOTONIC) - l0;
	printf("  200.000 lecturas seguidas: ninguna hacia atrás\n");
	/* cuatro hilos a la vez */
	pthread_t h[4];
	último = por_musl(CLOCK_MONOTONIC);
	for (int i = 0; i < 4; i++) pthread_create(&h[i], 0, lector, 0);
	for (int i = 0; i < 4; i++) pthread_join(h[i], 0);
	MIRA(atrás_hilos == 0, "4 hilos a la vez: ninguna lectura hacia atrás de lo último leído");
	printf("  4 hilos a la vez, 80.000 lecturas: ninguna hacia atrás de lo último leído por otro\n");
	/* dormir, medido con el vDSO */
	int64_t d0 = por_musl(CLOCK_MONOTONIC);
	struct timespec dz = { 0, 20000000 };
	nanosleep(&dz, 0);
	int64_t dd = por_musl(CLOCK_MONOTONIC) - d0;
	MIRA(dd >= 20000000 && dd < 200000000, "nanosleep de 20 ms, medido con el vDSO");
	printf("  nanosleep de 20 ms: entre 20 y 200 ms\n");
	if (hispa && e) {
		/* (la página de los datos del vDSO de Hispa-64, la de después: con_tsc) */
		volatile uint64_t *datos = (void *)(e + 4096);
		printf("MRELOJ: en Hispa-64, el vDSO %s; una lectura, %lld ns por musl y %lld ns por la llamada al sistema\n",
		       datos[1] ? "con el TSC, sin llamar al sistema" : "con la llamada al sistema (el reloj del núcleo no es el TSC)",
		       (long long)(lleva / 200000), (long long)(lleva_l / 2000));
	}
	return acaba("MRELOJ");
}
