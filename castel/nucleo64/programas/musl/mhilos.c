/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/musl/mhilos.c - Hispa-64, etapa 7 (docs/64/HISPA-64.md,
 * sección 13): los hilos de POSIX de musl (clone con CLONE_THREAD, futex,
 * set_robust_list...): 8 hilos que suman con un mutex, productores y
 * consumidores con una condición, TLS (__thread), pthread_join con lo que
 * devuelven, semáforos, una barrera, un hilo suelto, el plazo de
 * pthread_cond_timedwait, y los registros de AVX de cada hilo intactos
 * aunque cambien de procesador. [etapa 10 b] Y 8 hilos que abren y cierran
 * descriptores a la vez (sockets, eventfd, tuberías, /etc/hosts): ninguno
 * repetido (antes, dos hilos se podían llevar el mismo). Estático (MHILOS) y
 * dinámico (MHILOSD). */
#include "comun.h"
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sched.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/eventfd.h>

#define N 8
#define VUELTAS 2000

static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t hay = PTHREAD_COND_INITIALIZER, sitio = PTHREAD_COND_INITIALIZER;
static long cuenta;
static __thread long mío = 7;
static long tids[N];
static int cola[16], c_ini, c_n, fin_;
static long consumido[N];
static sem_t sem;
static pthread_barrier_t barrera;
static volatile int avx_mal, avx_hecho;

/* [etapa 10 b] los descriptores, a la vez: cada uno apunta de quién es el que le dan */
#define FD_VUELTAS 20
static volatile int dueño_fd[1024], fd_repetidos, fd_fallos, fd_hechos;
static void apunta_fd(int f, long k)
{
	if (f < 0 || f >= 1024) { __sync_fetch_and_add(&fd_fallos, 1); return; }
	if (__sync_val_compare_and_swap(&dueño_fd[f], 0, (int)k + 1) != 0) __sync_fetch_and_add(&fd_repetidos, 1);
	__sync_fetch_and_add(&fd_hechos, 1);
}
static void suelta_fd(int f, long k)
{
	if (f >= 0 && f < 1024) __sync_val_compare_and_swap(&dueño_fd[f], (int)k + 1, 0);
	close(f);
}
static void *descriptores(void *a)
{
	long k = (long)a;
	char b[64];
	for (int i = 0; i < FD_VUELTAS; i++) {
		int s = socket(AF_INET, SOCK_DGRAM, 0), e = eventfd(0, 0), t[2] = { -1, -1 }, f;
		apunta_fd(s, k);
		apunta_fd(e, k);
		if (pipe(t) == 0) { apunta_fd(t[0], k); apunta_fd(t[1], k); } else __sync_fetch_and_add(&fd_fallos, 1);
		f = open("/etc/hosts", O_RDONLY);
		apunta_fd(f, k);
		if (f >= 0 && read(f, b, sizeof b) <= 0) __sync_fetch_and_add(&fd_fallos, 1);
		suelta_fd(f, k);
		suelta_fd(t[0], k);
		suelta_fd(t[1], k);
		suelta_fd(e, k);
		suelta_fd(s, k);
	}
	return 0;
}

static void *suma(void *a)
{
	long k = (long)a, i;
	mío = 1000 + k;
	tids[k] = syscall(SYS_gettid);
	for (i = 0; i < VUELTAS; i++) {
		pthread_mutex_lock(&m);
		cuenta++;
		pthread_mutex_unlock(&m);
		if (i % 500 == 0) sched_yield();
	}
	return (void *)(mío * 10);
}

static void *consume(void *a)
{
	long k = (long)a;
	for (;;) {
		pthread_mutex_lock(&m);
		while (c_n == 0 && !fin_) pthread_cond_wait(&hay, &m);
		if (c_n == 0 && fin_) { pthread_mutex_unlock(&m); break; }
		consumido[k] += cola[c_ini];
		c_ini = (c_ini + 1) % 16;
		c_n--;
		pthread_cond_signal(&sitio);
		pthread_mutex_unlock(&m);
	}
	return 0;
}

static void *con_sem(void *a)
{
	(void)a;
	sem_wait(&sem);
	pthread_mutex_lock(&m);
	cuenta++;
	pthread_mutex_unlock(&m);
	pthread_barrier_wait(&barrera);
	return 0;
}

static void *suelto(void *a)
{
	(void)a;
	pthread_mutex_lock(&m);
	cuenta += 100;
	pthread_mutex_unlock(&m);
	pthread_exit(0);
}

/* AVX: un patrón en ymm0-ymm7, ceder el procesador muchas veces, y mirarlo */
__attribute__((target("avx"))) static void *avx(void *a)
{
	long k = (long)a, i, j;
	unsigned long en[32] __attribute__((aligned(32))), sale[32] __attribute__((aligned(32)));
	for (i = 0; i < 40; i++) {
		for (j = 0; j < 32; j++) en[j] = (k << 56) ^ (i << 32) ^ (j * 0x9E3779B97F4A7C15UL);
		/* (cargar, ceder el procesador con syscall y guardar, en un solo asm:
		 * una función de C de por medio podría usar los registros) */
		__asm__ volatile("vmovdqa (%0), %%ymm0\n vmovdqa 32(%0), %%ymm1\n vmovdqa 64(%0), %%ymm2\n vmovdqa 96(%0), %%ymm3\n"
		                 "vmovdqa 128(%0), %%ymm4\n vmovdqa 160(%0), %%ymm5\n vmovdqa 192(%0), %%ymm6\n vmovdqa 224(%0), %%ymm7\n"
		                 "mov $24, %%eax\n syscall\n"
		                 "vmovdqa %%ymm0, (%1)\n vmovdqa %%ymm1, 32(%1)\n vmovdqa %%ymm2, 64(%1)\n vmovdqa %%ymm3, 96(%1)\n"
		                 "vmovdqa %%ymm4, 128(%1)\n vmovdqa %%ymm5, 160(%1)\n vmovdqa %%ymm6, 192(%1)\n vmovdqa %%ymm7, 224(%1)\n"
		                 :: "r"(en), "r"(sale) : "rax", "rcx", "r11", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "memory");
		/* y unas vueltas sin llamar al sistema (el tic puede cambiarlo de procesador) */
		__asm__ volatile("vmovdqa (%0), %%ymm8\n mov $200000, %%ecx\n 1: dec %%ecx\n jnz 1b\n vmovdqa %%ymm8, (%1)\n"
		                 :: "r"(en + 8), "r"(sale + 8) : "rcx", "xmm8", "memory");
		if (memcmp(en, sale, 256)) avx_mal++;
	}
	__atomic_add_fetch(&avx_hecho, 1, __ATOMIC_SEQ_CST);
	return 0;
}

int main(void)
{
	pthread_t h[N], s;
	void *r;
	long i, total, esperado;
	int ok, j;
	struct timespec t0, t1, plazo;

	printf("MHILOS: los hilos de POSIX de musl; %s\n", cómo());
	/* 8 hilos que suman con un mutex */
	for (i = 0; i < N; i++) MIRA(pthread_create(&h[i], 0, suma, (void *)i) == 0, "pthread_create");
	ok = 1;
	for (i = 0; i < N; i++) {
		MIRA(pthread_join(h[i], &r) == 0, "pthread_join");
		if ((long)r != (1000 + i) * 10) ok = 0;
	}
	MIRA(ok, "lo que devuelve cada hilo (su TLS)");
	MIRA(cuenta == (long)N * VUELTAS, "la suma con el mutex, exacta");
	MIRA(mío == 7, "el TLS del primer hilo, intacto");
	ok = 1;
	for (i = 0; i < N; i++) for (j = 0; j < i; j++) if (tids[i] == tids[j] || tids[i] <= 0) ok = 0;
	MIRA(ok && tids[0] != getpid(), "cada hilo con su tid (gettid)");
	printf("  8 hilos que suman %d veces con un mutex: %ld (exacto); tids de %ld a %ld\n", VUELTAS, cuenta, tids[0], tids[N - 1]);
	/* productores y consumidores con una condición */
	for (i = 0; i < N; i++) pthread_create(&h[i], 0, consume, (void *)i);
	esperado = 0;
	for (i = 1; i <= 2000; i++) {
		pthread_mutex_lock(&m);
		while (c_n == 16) pthread_cond_wait(&sitio, &m);
		cola[(c_ini + c_n) % 16] = (int)i;
		c_n++;
		esperado += i;
		pthread_cond_signal(&hay);
		pthread_mutex_unlock(&m);
	}
	pthread_mutex_lock(&m);
	fin_ = 1;
	pthread_cond_broadcast(&hay);
	pthread_mutex_unlock(&m);
	total = 0;
	for (i = 0; i < N; i++) { pthread_join(h[i], 0); total += consumido[i]; }
	MIRA(total == esperado, "productor y 8 consumidores con pthread_cond");
	printf("  un productor y 8 consumidores (pthread_cond_wait, signal y broadcast): la suma, %ld (exacta)\n", total);
	/* semáforos y una barrera */
	cuenta = 0;
	sem_init(&sem, 0, 0);
	pthread_barrier_init(&barrera, 0, N + 1);
	for (i = 0; i < N; i++) pthread_create(&h[i], 0, con_sem, 0);
	for (i = 0; i < N; i++) sem_post(&sem);
	pthread_barrier_wait(&barrera);
	MIRA(cuenta == N, "sem_post, sem_wait y una barrera");
	for (i = 0; i < N; i++) pthread_join(h[i], 0);
	/* un hilo suelto (pthread_detach) que acaba con pthread_exit */
	MIRA(pthread_create(&s, 0, suelto, 0) == 0 && pthread_detach(s) == 0, "un hilo suelto");
	for (i = 0; i < 100; i++) {
		pthread_mutex_lock(&m);
		total = cuenta;
		pthread_mutex_unlock(&m);
		if (total == N + 100) break;
		usleep(5000);
	}
	MIRA(total == N + 100, "el hilo suelto ha hecho lo suyo");
	/* el plazo de una condición */
	clock_gettime(CLOCK_REALTIME, &t0);
	plazo = t0;
	plazo.tv_nsec += 50000000;
	if (plazo.tv_nsec >= 1000000000) { plazo.tv_sec++; plazo.tv_nsec -= 1000000000; }
	pthread_mutex_lock(&m);
	j = pthread_cond_timedwait(&hay, &m, &plazo);
	pthread_mutex_unlock(&m);
	clock_gettime(CLOCK_REALTIME, &t1);
	total = (t1.tv_sec - t0.tv_sec) * 1000 + (t1.tv_nsec - t0.tv_nsec) / 1000000;
	MIRA(j == ETIMEDOUT && total >= 40 && total < 1000, "pthread_cond_timedwait: ETIMEDOUT a su hora");
	printf("  pthread_cond_timedwait de 50 ms: ETIMEDOUT en %ld ms\n", total);
	/* [etapa 10 b] los descriptores, a la vez */
	for (i = 0; i < N; i++) pthread_create(&h[i], 0, descriptores, (void *)i);
	for (i = 0; i < N; i++) pthread_join(h[i], 0);
	MIRA(fd_repetidos == 0 && fd_fallos == 0 && fd_hechos == N * FD_VUELTAS * 5, "8 hilos que abren y cierran descriptores a la vez: ninguno repetido");
	printf("  descriptores: 8 hilos abren y cierran a la vez %d (sockets, eventfd, tuberías y /etc/hosts): %d repetidos, %d fallos\n", fd_hechos, fd_repetidos, fd_fallos);
	/* AVX */
	if (__builtin_cpu_supports("avx")) {
		for (i = 0; i < N; i++) pthread_create(&h[i], 0, avx, (void *)i);
		for (i = 0; i < N; i++) pthread_join(h[i], 0);
		MIRA(avx_hecho == N && avx_mal == 0, "los registros de AVX (ymm0 a ymm7) de 8 hilos, intactos");
		printf("  AVX: 8 hilos con ymm0 a ymm7 llenos, cediendo el procesador 40 veces: %s\n", avx_mal ? "¡cambiados!" : "intactos");
	} else {
		printf("  AVX: este procesador no lo tiene\n");
	}
	return acaba("MHILOS");
}
