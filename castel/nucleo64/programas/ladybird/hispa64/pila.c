/*
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * hispa64/pila.c - Hispa-64, etapa 10 g (docs/64/HISPA-64.md, sección 24):
 * la pila del hilo principal para Ladybird.
 *
 * LibJS (AK/Rust/src/stack.rs, AK/StackInfo.cpp) mira con
 * pthread_getattr_np() dónde acaba la pila del hilo en que corre, y deja a
 * JavaScript usarla hasta un poco antes. En el hilo principal, musl no lo
 * sabe: lo busca bajando página a página con mremap() hasta la primera que
 * no está, y en Linux la pila del hilo principal crece sola al usarla (solo
 * está lo usado hasta entonces): sale muy poca, y LibJS dice «Maximum call
 * stack size exceeded» al empezar (glibc la da del tamaño de RLIMIT_STACK).
 * Aquí, como glibc: los programas de Ladybird se enlazan con
 * -Wl,--wrap=pthread_getattr_np, y en el hilo principal la pila es la de
 * RLIMIT_STACK (8 MB si no tiene límite o pasa de 64 MB), con el mismo
 * final de arriba que dice musl. En los demás hilos, lo de musl tal cual.
 */
#define _GNU_SOURCE
#include <pthread.h>
#include <sys/resource.h>
#include <unistd.h>

int __real_pthread_getattr_np(pthread_t t, pthread_attr_t *a);

int __wrap_pthread_getattr_np(pthread_t t, pthread_attr_t *a)
{
	int r = __real_pthread_getattr_np(t, a);
	if (r != 0 || t != pthread_self() || gettid() != getpid())
		return r;
	void *dir;
	size_t tam;
	if (pthread_attr_getstack(a, &dir, &tam) != 0)
		return r;
	char *arriba = (char *)dir + tam;
	struct rlimit l;
	size_t quiero = 8u << 20;
	if (getrlimit(RLIMIT_STACK, &l) == 0 && l.rlim_cur != RLIM_INFINITY && l.rlim_cur <= (64u << 20))
		quiero = l.rlim_cur;
	quiero &= ~(size_t)4095;
	/* (lo de arriba de la pila que ya hay cuenta: el entorno, los argumentos) */
	if (quiero > tam)
		pthread_attr_setstack(a, arriba - quiero, quiero);
	return r;
}
