/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/musl/msenales.c - Hispa-64, etapa 7 (docs/64/
 * HISPA-64.md, sección 13): las señales de verdad: sigaction con
 * SA_SIGINFO (si_signo, si_code, si_pid), raise y kill, la máscara
 * (sigprocmask, sigpending), los fallos del procesador atendidos (SIGSEGV
 * con si_addr, SIGFPE, SIGILL) y salir de ellos con siglongjmp, la pila de
 * las señales (sigaltstack), los registros de coma flotante intactos al
 * volver, SA_RESTART y EINTR, SIGCHLD, sigsuspend, SIG_IGN, SIGPIPE,
 * pthread_kill (tgkill) y un hijo que no llama al sistema cerrado con kill
 * (SIGTERM y SIGINT: le llega en el tic); [etapa 10] alarm y setitimer
 * (SIGALRM: una vez y con intervalo, getitimer, lo que quedaba, quitarla,
 * y que el hijo de fork no la hereda); [etapa 11] ITIMER_VIRTUAL e
 * ITIMER_PROF, getrusage y CLOCK_PROCESS_CPUTIME_ID. Estático y dinámico. */
#include "comun.h"
#include <stdlib.h>
#include <signal.h>
#include <setjmp.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/resource.h>

static volatile sig_atomic_t veces, último, código_, pid_;
static volatile void *dir_;
static volatile long tid_;
static volatile int en_alt;
static sigjmp_buf salto;
static char *alt;
static volatile int estado_hijo;

static void atiende(int s, siginfo_t *i, void *uc)
{
	(void)uc;
	veces++;
	último = s;
	código_ = i->si_code;
	pid_ = i->si_pid;
	tid_ = syscall(SYS_gettid);
	{ char x; en_alt = alt && &x >= alt && &x < alt + 65536; }
	/* coma flotante en la función: lo de fuera no debe cambiar */
	{ volatile double a = 1.0; int k; for (k = 0; k < 10; k++) a = a * 3.7 + 0.1; }
}

static void al_fallo(int s, siginfo_t *i, void *uc)
{
	(void)uc;
	último = s;
	código_ = i->si_code;
	dir_ = i->si_addr;
	siglongjmp(salto, 1);
}

static void al_hijo(int s, siginfo_t *i, void *uc)
{
	(void)uc;
	último = s;
	código_ = i->si_code;
	pid_ = i->si_pid;
	estado_hijo = i->si_status;
}

static void pon(int s, void (*f)(int, siginfo_t *, void *), int marcas)
{
	struct sigaction a;
	memset(&a, 0, sizeof a);
	a.sa_sigaction = f;
	a.sa_flags = SA_SIGINFO | marcas;
	sigemptyset(&a.sa_mask);
	sigaction(s, &a, 0);
}

static void *hilo_espera(void *a)
{
	(void)a;
	tid_ = 0;
	*(volatile long *)a = syscall(SYS_gettid);
	while (!veces) usleep(1000);
	return 0;
}

int main(void)
{
	sigset_t ms, pend;
	volatile double d = 1.5;
	volatile int cero = 0, x = 0;
	volatile int *volatile mala = (volatile int *)0x10;
	pid_t h;
	int st, tubo[2];
	char c;
	ssize_t n;
	stack_t ss;
	struct timespec dur = { 2, 0 }, resto;
	pthread_t ph;
	volatile long su_tid = 0;

	printf("MSENALES: las señales de verdad; %s\n", cómo());
	/* raise (tkill) y kill */
	pon(SIGUSR1, atiende, 0);
	pon(SIGUSR2, atiende, 0);
	raise(SIGUSR1);
	MIRA(veces == 1 && último == SIGUSR1 && código_ == SI_TKILL, "raise(SIGUSR1): su función, con SI_TKILL");
	MIRA(d == 1.5, "la coma flotante, intacta al volver de la señal");
	kill(getpid(), SIGUSR2);
	MIRA(veces == 2 && último == SIGUSR2 && código_ == SI_USER && pid_ == getpid(), "kill(getpid(), SIGUSR2): SI_USER y si_pid");
	printf("  raise y kill: la función de la señal, con su siginfo; la coma flotante, intacta\n");
	/* la máscara */
	sigemptyset(&ms);
	sigaddset(&ms, SIGUSR1);
	sigprocmask(SIG_BLOCK, &ms, 0);
	raise(SIGUSR1);
	MIRA(veces == 2, "bloqueada: no llega");
	sigpending(&pend);
	MIRA(sigismember(&pend, SIGUSR1), "sigpending la tiene");
	sigprocmask(SIG_UNBLOCK, &ms, 0);
	MIRA(veces == 3 && último == SIGUSR1, "al desbloquearla, llega ya");
	printf("  sigprocmask: bloqueada no llega (sigpending la tiene); al desbloquearla, llega\n");
	/* los fallos del procesador, atendidos */
	pon(SIGSEGV, al_fallo, SA_NODEFER);
	pon(SIGFPE, al_fallo, SA_NODEFER);
	pon(SIGILL, al_fallo, SA_NODEFER);
	if (!sigsetjmp(salto, 1)) { *mala = 5; MIRA(0, "escribir en 0x10 sin fallo"); }
	MIRA(último == SIGSEGV && dir_ == (void *)0x10 && código_ == SEGV_MAPERR, "SIGSEGV: si_addr 0x10, SEGV_MAPERR");
	if (!sigsetjmp(salto, 1)) { x = 7 / cero; MIRA(0, "dividir entre cero sin fallo"); }
	MIRA(último == SIGFPE && código_ == FPE_INTDIV && x == 0, "SIGFPE: FPE_INTDIV");
	if (!sigsetjmp(salto, 1)) { __builtin_trap(); }
	MIRA(último == SIGILL && código_ == ILL_ILLOPN, "SIGILL: ILL_ILLOPN");
	if (!sigsetjmp(salto, 1)) { *(volatile int *)(void *)main = 1; MIRA(0, "escribir en el código sin fallo"); }
	MIRA(último == SIGSEGV && dir_ == (void *)main && código_ == SEGV_ACCERR, "SIGSEGV al escribir en el código: SEGV_ACCERR");
	printf("  los fallos (SIGSEGV en 0x10 y en su código, SIGFPE, SIGILL), atendidos y salidos con siglongjmp\n");
	/* la pila de las señales */
	alt = malloc(65536);
	ss.ss_sp = alt;
	ss.ss_size = 65536;
	ss.ss_flags = 0;
	MIRA(sigaltstack(&ss, 0) == 0, "sigaltstack");
	pon(SIGUSR1, atiende, SA_ONSTACK);
	raise(SIGUSR1);
	MIRA(en_alt, "SA_ONSTACK: en la pila de las señales");
	pon(SIGUSR1, atiende, 0);
	raise(SIGUSR1);
	MIRA(!en_alt, "sin SA_ONSTACK: en la de siempre");
	printf("  sigaltstack y SA_ONSTACK: la función corre en la pila de las señales\n");
	/* SA_RESTART y EINTR: read de una tubería, interrumpido por el hijo */
	pipe(tubo);
	pon(SIGUSR2, atiende, SA_RESTART);
	h = fork();
	if (h == 0) { usleep(50000); kill(getppid(), SIGUSR2); usleep(50000); write(tubo[1], "x", 1); _exit(0); }
	veces = 0;
	n = read(tubo[0], &c, 1);
	MIRA(n == 1 && c == 'x' && veces == 1, "SA_RESTART: read se repite tras la señal");
	waitpid(h, &st, 0);
	pon(SIGUSR2, atiende, 0);
	h = fork();
	if (h == 0) { usleep(50000); kill(getppid(), SIGUSR2); _exit(0); }
	veces = 0;
	n = read(tubo[0], &c, 1);
	MIRA(n == -1 && errno == EINTR && veces == 1, "sin SA_RESTART: read da EINTR");
	waitpid(h, &st, 0);
	printf("  read de una tubería interrumpido: con SA_RESTART se repite; sin él, EINTR\n");
	/* nanosleep interrumpido */
	h = fork();
	if (h == 0) { usleep(30000); kill(getppid(), SIGUSR2); _exit(0); }
	MIRA(nanosleep(&dur, &resto) == -1 && errno == EINTR && resto.tv_sec >= 1, "nanosleep de 2 s interrumpido: EINTR y lo que quedaba");
	waitpid(h, &st, 0);
	/* SIGCHLD */
	pon(SIGCHLD, al_hijo, 0);
	h = fork();
	if (h == 0) _exit(3);
	MIRA(waitpid(h, &st, 0) == h && WIFEXITED(st) && WEXITSTATUS(st) == 3, "waitpid: el hijo acaba con 3");
	MIRA(último == SIGCHLD && código_ == CLD_EXITED && pid_ == h && estado_hijo == 3, "SIGCHLD: CLD_EXITED, si_pid y si_status");
	signal(SIGCHLD, SIG_DFL);
	printf("  SIGCHLD al acabar un hijo: CLD_EXITED, con su pid y su código\n");
	/* un hijo que no llama al sistema, cerrado con kill (la señal le llega en el tic) */
	h = fork();
	if (h == 0) { volatile unsigned long k = 0; for (;;) k++; }
	usleep(50000);
	kill(h, SIGTERM);
	MIRA(waitpid(h, &st, 0) == h && WIFSIGNALED(st) && WTERMSIG(st) == SIGTERM, "un hijo que solo cuenta, cerrado con SIGTERM");
	h = fork();
	if (h == 0) { volatile unsigned long k = 0; for (;;) k++; }
	usleep(30000);
	kill(h, SIGINT);
	MIRA(waitpid(h, &st, 0) == h && WIFSIGNALED(st) && WTERMSIG(st) == SIGINT, "un hijo que solo cuenta, cerrado con SIGINT (lo de Ctrl+C)");
	printf("  un hijo que no llama al sistema, cerrado con SIGTERM y con SIGINT\n");
	/* sigsuspend */
	sigprocmask(SIG_BLOCK, &ms, 0);
	pon(SIGUSR1, atiende, 0);
	h = fork();
	if (h == 0) { usleep(30000); kill(getppid(), SIGUSR1); _exit(0); }
	veces = 0;
	sigemptyset(&pend);
	MIRA(sigsuspend(&pend) == -1 && errno == EINTR && veces == 1, "sigsuspend: espera la señal");
	sigprocmask(SIG_SETMASK, 0, &pend);
	MIRA(sigismember(&pend, SIGUSR1), "tras sigsuspend, la máscara de antes");
	sigprocmask(SIG_UNBLOCK, &ms, 0);
	waitpid(h, &st, 0);
	/* SIG_IGN y SIGPIPE */
	signal(SIGUSR1, SIG_IGN);
	veces = 0;
	raise(SIGUSR1);
	MIRA(veces == 0, "SIG_IGN: no pasa nada");
	signal(SIGPIPE, SIG_IGN);
	close(tubo[0]);
	MIRA(write(tubo[1], "y", 1) == -1 && errno == EPIPE, "escribir sin quien lea: EPIPE (SIGPIPE ignorada)");
	close(tubo[1]);
	pipe(tubo);
	close(tubo[0]);	/* antes del fork: si no, el hijo podría escribir mientras el padre aún lee */
	h = fork();
	if (h == 0) { signal(SIGPIPE, SIG_DFL); write(tubo[1], "z", 1); _exit(0); }
	close(tubo[1]);
	MIRA(waitpid(h, &st, 0) == h && WIFSIGNALED(st) && WTERMSIG(st) == SIGPIPE, "sin ignorarla, SIGPIPE cierra al hijo");
	printf("  SIG_IGN; SIGPIPE: ignorada, EPIPE; sin ignorarla, cierra el proceso\n");
	/* pthread_kill: a un hilo (tgkill) */
	pon(SIGUSR2, atiende, 0);
	veces = 0;
	pthread_create(&ph, 0, hilo_espera, (void *)&su_tid);
	while (!su_tid) usleep(1000);
	pthread_kill(ph, SIGUSR2);
	pthread_join(ph, 0);
	MIRA(veces == 1 && tid_ == su_tid, "pthread_kill: la atiende ese hilo");
	printf("  pthread_kill (tgkill): la señal la atiende el hilo %ld\n", (long)su_tid);
	/* [etapa 10] alarm y setitimer (ITIMER_REAL) */
	{
		struct itimerval it, vi;
		struct timespec a, b;
		long ms;
		unsigned q;
		pon(SIGALRM, atiende, 0);
		veces = 0;
		MIRA(alarm(0) == 0, "alarm(0) sin alarma: 0");
		clock_gettime(CLOCK_MONOTONIC, &a);
		alarm(1);
		while (!veces) pause();
		clock_gettime(CLOCK_MONOTONIC, &b);
		ms = (b.tv_sec - a.tv_sec) * 1000 + (b.tv_nsec - a.tv_nsec) / 1000000;
		MIRA(veces == 1 && último == SIGALRM && código_ == SI_KERNEL && ms >= 990 && ms < 3000, "alarm(1): SIGALRM al segundo");
		q = alarm(5);
		MIRA(q == 0, "alarm: la de antes ya había vencido (0)");
		q = alarm(0);
		MIRA(q >= 4 && q <= 5, "alarm(0): lo que le quedaba a la de 5 s, y quitada");
		memset(&it, 0, sizeof it);
		it.it_value.tv_usec = 50000;
		it.it_interval.tv_usec = 20000;
		veces = 0;
		clock_gettime(CLOCK_MONOTONIC, &a);
		MIRA(setitimer(ITIMER_REAL, &it, 0) == 0, "setitimer: 50 ms y luego cada 20 ms");
		MIRA(getitimer(ITIMER_REAL, &vi) == 0 && vi.it_interval.tv_usec == 20000 && vi.it_value.tv_sec == 0 && vi.it_value.tv_usec > 0 && vi.it_value.tv_usec <= 50000, "getitimer: el intervalo y lo que queda");
		while (veces < 5) pause();
		clock_gettime(CLOCK_MONOTONIC, &b);
		memset(&it, 0, sizeof it);
		MIRA(setitimer(ITIMER_REAL, &it, &vi) == 0 && vi.it_interval.tv_usec == 20000, "setitimer a cero: la quita y da la de antes");
		ms = (b.tv_sec - a.tv_sec) * 1000 + (b.tv_nsec - a.tv_nsec) / 1000000;
		MIRA(ms >= 125, "cinco SIGALRM con intervalo: 50 + 4 x 20 ms como poco");
		veces = 0;
		usleep(100000);
		MIRA(veces == 0, "quitada: no llega ninguna más");
		/* [etapa 11] las de tiempo de procesador: ITIMER_VIRTUAL (el del
		 * programa) e ITIMER_PROF; dormir no cuenta; getrusage y
		 * CLOCK_PROCESS_CPUTIME_ID, con lo gastado */
		{
			struct timespec c0, c1, w0, w1;
			struct rusage ru;
			long cpu_ms, vueltas = 0, j;
			volatile unsigned long k = 0;
			pon(SIGVTALRM, atiende, 0);
			pon(SIGPROF, atiende, 0);
			memset(&it, 0, sizeof it);
			it.it_value.tv_usec = 50000;
			veces = 0;
			MIRA(setitimer(ITIMER_VIRTUAL, &it, 0) == 0, "setitimer ITIMER_VIRTUAL: 50 ms del programa");
			usleep(150000);
			MIRA(veces == 0, "ITIMER_VIRTUAL: durmiendo no cuenta");
			clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &c0);
			clock_gettime(CLOCK_MONOTONIC, &w0);
			/* (el trabajo, en el programa: el reloj, de vez en cuando) */
			do {
				for (j = 0; j < 100000 && !veces; j++) k++;
				clock_gettime(CLOCK_MONOTONIC, &w1);
			} while (!veces && w1.tv_sec - w0.tv_sec < 20);
			clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &c1);
			cpu_ms = (c1.tv_sec - c0.tv_sec) * 1000 + (c1.tv_nsec - c0.tv_nsec) / 1000000;
			MIRA(veces == 1 && último == SIGVTALRM && código_ == SI_KERNEL, "ITIMER_VIRTUAL: SIGVTALRM al gastar 50 ms");
			MIRA(cpu_ms >= 30 && cpu_ms <= 2000, "CLOCK_PROCESS_CPUTIME_ID: lo gastado (unos 50 ms)");
			memset(&it, 0, sizeof it);
			it.it_value.tv_usec = 30000;
			it.it_interval.tv_usec = 30000;
			veces = 0;
			MIRA(setitimer(ITIMER_PROF, &it, 0) == 0, "setitimer ITIMER_PROF: 30 ms y luego cada 30");
			MIRA(getitimer(ITIMER_PROF, &vi) == 0 && vi.it_interval.tv_usec == 30000 && vi.it_value.tv_usec > 0 && vi.it_value.tv_usec <= 30000, "getitimer ITIMER_PROF");
			clock_gettime(CLOCK_MONOTONIC, &w0);
			do {
				for (j = 0; j < 100000 && veces < 3; j++) k++;
				getppid();      /* (también lo del núcleo por él) */
				clock_gettime(CLOCK_MONOTONIC, &w1);
			} while (veces < 3 && w1.tv_sec - w0.tv_sec < 20);
			MIRA(veces >= 3 && último == SIGPROF, "ITIMER_PROF: tres SIGPROF, una cada 30 ms gastados");
			memset(&it, 0, sizeof it);
			MIRA(setitimer(ITIMER_PROF, &it, &vi) == 0 && vi.it_interval.tv_usec == 30000, "ITIMER_PROF quitada");
			vueltas = veces;
			usleep(50000);
			MIRA(veces == vueltas, "quitada: ninguna más");
			MIRA(getrusage(RUSAGE_SELF, &ru) == 0 && ru.ru_utime.tv_sec * 1000 + ru.ru_utime.tv_usec / 1000 >= 80, "getrusage: el tiempo del programa (más de 80 ms)");
			MIRA(setitimer(3, &it, 0) == -1 && errno == EINVAL, "setitimer de una que no hay: EINVAL");
			printf("  ITIMER_VIRTUAL y ITIMER_PROF: SIGVTALRM y SIGPROF al gastar el procesador (%ld ms medidos con CLOCK_PROCESS_CPUTIME_ID)\n", cpu_ms);
		}
		alarm(10);
		h = fork();
		if (h == 0) _exit(alarm(0) == 0 ? 0 : 1);
		MIRA(waitpid(h, &st, 0) == h && WIFEXITED(st) && WEXITSTATUS(st) == 0, "fork: el hijo no hereda la alarma");
		MIRA(alarm(0) >= 9, "y la del padre sigue");
		printf("  alarm y setitimer: SIGALRM a su tiempo (%ld ms las cinco del intervalo)\n", ms);
	}
	return acaba("MSENALES");
}
