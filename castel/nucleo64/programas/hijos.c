/* hijos.c - Hispa-64, etapa 6 (docs/64/HISPA-64.md, sección 12): arrancar
 * programas desde un programa y la consola como terminal, desde el anillo 3.
 * fork (las páginas copiadas: lo que cambia el hijo no lo ve el padre), los
 * descriptores compartidos (la posición), getppid, wait4 (varios hijos, el
 * estado como el de Linux, WNOHANG, -ECHILD, rusage), un hijo que falla (con
 * su señal: SIGSEGV), vfork y clone(SIGCHLD) como fork, execve (argv, envp,
 * O_CLOEXEC; los errores: ENOENT, ENOEXEC, EACCES), los huérfanos y los
 * zombis que nadie recoge (el núcleo mira después que no queda ninguno), y
 * la consola: isatty (TCGETS), TCSETS (el modo crudo, con VMIN y VTIME),
 * TIOCGWINSZ, TIOCGPGRP, FIONREAD, fstat (un dispositivo de caracteres),
 * /dev/tty; poll, ppoll, select y pselect6 con plazo. Acaba con 0 si todo va
 * bien. También corre en Linux («HIJOS en CARPETA», con HIJOS y SALUDO.TXT en
 * ella): lo que solo es de Hispa-64 se salta.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "sis.h"

enum { S_FSTAT_ = 5, S_STAT_ = 4 };
enum { ECHILD = 10, EACCES = 13, ENOEXEC = 8, EBADF_ = 9 };

static int global = 7;
int memcmp_(const void *a, const void *b, u64 n);
static int en_hispa;
static char base[200];

static i64 espera(i64 pid, int *st, int op) { return sis6(S_WAIT4, pid, (i64)st, op, 0, 0, 0); }
static void duerme_ms(i64 ms) { u64 ts[2] = { (u64)ms / 1000, ((u64)ms % 1000) * 1000000 }; sis(S_NANOSLEEP, ts, 0, 0); }
static i64 ahora_ms(void) { u64 ts[2]; sis(S_CLOCK_GETTIME, 1, ts, 0); return (i64)(ts[0] * 1000 + ts[1] / 1000000); }
static void sal(int c) { sis(S_EXIT_GROUP, c, 0, 0); for (;;) ; }
static int igual(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return *a == *b; }
static void junta(char *d, const char *a, const char *b)
{
	while (*a) *d++ = *a++;
	while (*b) *d++ = *b++;
	*d = 0;
}
static void num(char *t, i64 v)
{
	char x[24]; int i = 0;
	do { x[i++] = '0' + v % 10; v /= 10; } while (v);
	while (i) *t++ = x[--i];
	*t = 0;
}
static i64 atoi_(const char *s) { i64 v = 0; while (*s >= '0' && *s <= '9') v = v * 10 + *s++ - '0'; return v; }
static unsigned modo_de(int fd) { u64 st[18]; if (sis(S_FSTAT_, fd, st, 0) < 0) return 0; return *(unsigned *)((char *)st + 24); }

/* el hijo de execve: «HIJOS hijo A B»: A (con O_CLOEXEC) cerrado, B abierto, y el entorno */
static int hijo_exec(i64 argc, char **argv, char **envp)
{
	u64 st[18];
	int bien = 1;
	if (argc != 4) bien = 0;
	if (bien && sis(S_FSTAT_, atoi_(argv[2]), st, 0) != -EBADF_) bien = 0;
	if (bien && sis(S_FSTAT_, atoi_(argv[3]), st, 0) != 0) bien = 0;
	if (bien && (!envp[0] || !igual(envp[0], "HIJOS=1") || envp[1])) bien = 0;
	return bien ? 42 : 1;
}

/* «HIJOS lee»: lo que llega por la entrada, línea a línea, hasta el fin del
 * archivo (Ctrl+D en la consola); para probar Ctrl+C y Ctrl+D desde el intérprete */
static int lee_entrada(void)
{
	char t[300];
	i64 r;
	pon("HIJOS lee: escriba l\xc3\xadneas (Ctrl+D acaba)\n");
	for (;;) {
		r = sis(S_READ, 0, t, sizeof t - 1);
		if (r < 0) { pon("HIJOS lee: error "); pon_n(r); pon("\n"); return 2; }
		if (r == 0) { pon("HIJOS lee: fin del archivo\n"); return 0; }
		t[r] = 0;
		pon("  le\xc3\xad" "do: "); pon(t);
		if (t[r - 1] != '\n') pon("\n");
	}
}

/* la consola, como terminal */
static void consola(void)
{
	unsigned char tio[60], viejo[60];
	unsigned short ws[4];
	int pg = 0, n = 0, fd;
	i64 t0, t;
	if (sis(S_IOCTL, 0, TCGETS, tio) != 0) {
		mira(!en_hispa, "isatty(0): TCGETS de la consola");
		pon("  (la entrada no es una terminal: sin las pruebas de la consola)\n");
		return;
	}
	memcpy(viejo, tio, 36);
	mira((*(unsigned *)(tio + 12) & 0xA) == 0xA, "TCGETS: ICANON y ECHO");
	mira(sis(S_IOCTL, 0, TIOCGWINSZ, ws) == 0 && ((ws[0] > 0 && ws[1] > 0) || !en_hispa), "TIOCGWINSZ");
	pon("  la consola: una terminal de "); pon_n(ws[1]); pon(" x "); pon_n(ws[0]); pon(" (TIOCGWINSZ)\n");
	mira(sis(S_IOCTL, 0, TIOCGPGRP, &pg) == 0 && pg > 0, "TIOCGPGRP");
	mira(sis(S_IOCTL, 0, FIONREAD, &n) == 0 && n == 0, "FIONREAD");
	mira((modo_de(0) & 0xF000) == 0x2000, "fstat(0): un dispositivo de caracteres");
	/* el modo crudo: sin ICANON ni ECHO, VMIN 0 y VTIME 0: read no espera */
	*(unsigned *)(tio + 12) &= ~0xAu;
	tio[17 + 6] = 0;
	tio[17 + 5] = 0;
	mira(sis(S_IOCTL, 0, TCSETS, tio) == 0, "TCSETS (crudo)");
	memset(tio, 0, sizeof tio);
	mira(sis(S_IOCTL, 0, TCGETS, tio) == 0 && (*(unsigned *)(tio + 12) & 0xA) == 0 && tio[23] == 0, "TCGETS tras TCSETS");
	mira(sis(S_READ, 0, tio, 10) == 0, "read crudo con VMIN = 0 y VTIME = 0: no espera");
	tio[17 + 5] = 2;
	sis(S_IOCTL, 0, TCSETS, tio);
	t0 = ahora_ms();
	mira(sis(S_READ, 0, tio + 40, 10) == 0, "read crudo con VTIME = 2");
	t = ahora_ms() - t0;
	mira(t >= 150 && t < 2000, "VTIME = 2: 200 ms");
	pon("  el modo crudo: read sin esperar y con VTIME de 200 ms ("); pon_n(t); pon(" ms)\n");
	mira(sis(S_IOCTL, 0, TCSETS, viejo) == 0 && sis(S_IOCTL, 0, TCGETS, tio) == 0 && memcmp_(tio, viejo, 36) == 0,
	     "TCSETS: otra vez como estaba");
	fd = sis(S_OPEN, "/dev/tty", 2, 0);
	mira(fd >= 0, "open de /dev/tty");
	if (fd >= 0) {
		mira(sis(S_WRITE, fd, "  /dev/tty: se abre y se escribe en ella\n", 41) == 41, "write a /dev/tty");
		sis(S_CLOSE, fd, 0, 0);
	}
}

/* poll, ppoll, select y pselect6, con plazo */
static void plazos(void)
{
	char ruta[260];
	int pf[6];
	u64 r[16], w[16], tv[2];
	i64 t0, t, fd;
	junta(ruta, base, "SALUDO.TXT");
	fd = sis(S_OPEN, ruta, 0, 0);
	mira(fd >= 3, "open de SALUDO.TXT");
	/* la consola, sin nada: se acaba el plazo */
	pf[0] = 0; pf[1] = 1;
	t0 = ahora_ms();
	mira(sis(S_POLL, pf, 1, 50) == 0, "poll de la consola con 50 ms");
	t = ahora_ms() - t0;
	mira(t >= 40 && t < 2000, "poll: el plazo");
	pon("  poll de la consola (sin teclas) con 50 ms: 0 en "); pon_n(t); pon(" ms\n");
	/* un archivo, listo; uno que no está abierto, POLLNVAL; -1, nada */
	pf[0] = (int)fd; pf[1] = 1 | 4;
	pf[2] = 29; pf[3] = 1;
	pf[4] = -1; pf[5] = 1;
	mira(sis(S_POLL, pf, 3, 1000) == 2 && (pf[1] >> 16) == 5 && (pf[3] >> 16) == 0x20 && (pf[5] >> 16) == 0,
	     "poll de un archivo (POLLIN y POLLOUT), uno cerrado (POLLNVAL) y -1");
	/* la consola: lista para escribir */
	pf[0] = 1; pf[1] = 4;
	mira(sis(S_POLL, pf, 1, 0) == 1 && (pf[1] >> 16) == 4, "poll de la consola: POLLOUT");
	tv[0] = 0; tv[1] = 30000000;
	pf[0] = 0; pf[1] = 1;
	t0 = ahora_ms();
	mira(sis6(S_PPOLL, (i64)pf, 1, (i64)tv, 0, 8, 0) == 0, "ppoll con 30 ms");
	t = ahora_ms() - t0;
	mira(t >= 20 && t < 2000, "ppoll: el plazo");
	/* select */
	memset(r, 0, sizeof r);
	r[0] = 1;
	tv[0] = 0; tv[1] = 30000;
	t0 = ahora_ms();
	mira(sis6(S_SELECT, 1, (i64)r, 0, 0, (i64)tv, 0) == 0 && r[0] == 0, "select de la consola con 30 ms");
	t = ahora_ms() - t0;
	mira(t >= 20 && t < 2000, "select: el plazo");
	pon("  select de la consola con 30 ms: 0 en "); pon_n(t); pon(" ms\n");
	memset(r, 0, sizeof r);
	memset(w, 0, sizeof w);
	r[0] = (u64)1 << fd;
	w[0] = 2;
	tv[0] = 5; tv[1] = 0;
	mira(sis6(S_SELECT, fd + 1, (i64)r, (i64)w, 0, (i64)tv, 0) == 2 && r[0] == ((u64)1 << fd) && w[0] == 2,
	     "select de un archivo (leer) y de la consola (escribir)");
	tv[0] = 0; tv[1] = 20000000;
	t0 = ahora_ms();
	mira(sis6(S_PSELECT6, 0, 0, 0, 0, (i64)tv, 0) == 0, "pselect6 sin descriptores, con 20 ms");
	t = ahora_ms() - t0;
	mira(t >= 10 && t < 2000, "pselect6: el plazo");
	sis(S_CLOSE, fd, 0, 0);
}

int memcmp_(const void *a, const void *b, u64 n)
{
	const unsigned char *p = a, *q = b;
	while (n--) { if (*p != *q) return *p - *q; p++; q++; }
	return 0;
}

int principal(i64 *sp)
{
	i64 argc = sp[0], i, pid, r, fd, a, b, yo, hijos[8];
	char **argv = (char **)(sp + 1);
	char **envp = argv + argc + 1;
	char nombre[390], ruta[260], ta[24], tb[24];
	char *av[6], *ev[2];
	int st, suma, vistos;
	u64 ru[18];

	if (argc >= 2 && igual(argv[1], "hijo")) return hijo_exec(argc, argv, envp);
	if (argc >= 2 && igual(argv[1], "lee")) return lee_entrada();
	en_hispa = sis(S_UNAME, nombre, 0, 0) == 0 && igual(nombre, "Hispa-64");
	base[0] = '/'; base[1] = 0;
	if (argc >= 3 && igual(argv[1], "en")) junta(base, argv[2], "/");
	yo = sis(S_GETPID, 0, 0, 0);
	pon("HIJOS: fork, execve y wait4, la consola y los plazos (proceso "); pon_n(yo); pon(")\n");
	if (en_hispa) {
		mira(sis(S_GETPPID, 0, 0, 0) == 0, "getppid (lo arranca el n\xc3\xba" "cleo: 0)");
		mira(sis(S_GETPGRP, 0, 0, 0) == yo, "getpgrp (el suyo)");
	}

	/* fork: el hijo cambia una variable; el padre no lo ve */
	pid = sis(S_FORK, 0, 0, 0);
	if (pid == 0) {
		global = 99;
		sal(sis(S_GETPPID, 0, 0, 0) == yo && global == 99 ? 10 : 1);
	}
	mira(pid > yo, "fork: el id del hijo");
	mira(espera(pid, &st, 0) == pid && (st & 0x7F) == 0 && (st >> 8) == 10, "wait4: el c\xc3\xb3" "digo del hijo (10 << 8)");
	mira(global == 7, "fork: lo que cambia el hijo no lo ve el padre");
	pon("  fork: el hijo "); pon_n(pid); pon(" acaba con 10 (getppid, su copia de la memoria); el padre sigue con lo suyo\n");

	/* cinco hijos a la vez; WNOHANG mientras duermen; luego todos */
	for (i = 0; i < 5; i++) {
		hijos[i] = sis(S_FORK, 0, 0, 0);
		if (hijos[i] == 0) { duerme_ms(60 + 15 * i); sal(20 + (int)i); }
	}
	mira(espera(-1, &st, 1) == 0, "wait4 con WNOHANG: 0 (ninguno ha acabado)");
	suma = 0; vistos = 0;
	for (i = 0; i < 5; i++) {
		r = espera(-1, &st, 0);
		for (a = 0; a < 5; a++) if (r == hijos[a]) vistos++;
		suma += st >> 8;
	}
	mira(vistos == 5 && suma == 20 + 21 + 22 + 23 + 24, "wait4(-1): los cinco, con sus c\xc3\xb3" "digos");
	mira(espera(-1, &st, 0) == -ECHILD, "wait4 sin hijos: -ECHILD");
	pon("  cinco hijos a la vez: WNOHANG da 0; luego los cinco (20 a 24); y despu\xc3\xa9s, -ECHILD\n");

	/* un hijo que falla: lo cierra SIGSEGV */
	pon("  un hijo que escribe en el puntero nulo:\n");
	pid = sis(S_FORK, 0, 0, 0);
	if (pid == 0) { *(volatile int *)0 = 1; sal(1); }
	mira(espera(pid, &st, 0) == pid && (st & 0x7F) == 11, "wait4: el hijo cerrado por SIGSEGV (11)");
	pon("  el hijo cerrado por su se\xc3\xb1" "al: estado "); pon_n(st & 0x7F); pon(" (SIGSEGV)\n");

	/* lo abierto se comparte: la posición */
	junta(ruta, base, "SALUDO.TXT");
	fd = sis(S_OPEN, ruta, 0, 0);
	mira(fd >= 3, "open de SALUDO.TXT");
	pid = sis(S_FORK, 0, 0, 0);
	if (pid == 0) sal(sis(S_READ, fd, nombre, 5) == 5 ? 0 : 1);
	mira(espera(pid, &st, 0) == pid && st == 0 && sis(S_LSEEK, fd, 0, 1) == 5, "fork: la posici\xc3\xb3n de un archivo, compartida");
	sis(S_CLOSE, fd, 0, 0);

	/* vfork y clone(SIGCHLD), como fork */
	pid = sis(S_VFORK, 0, 0, 0);
	if (pid == 0) sal(5);
	mira(pid > 0 && espera(pid, &st, 0) == pid && (st >> 8) == 5, "vfork");
	pid = sis6(S_CLONE, 17, 0, 0, 0, 0, 0);
	if (pid == 0) sal(6);
	mira(pid > 0 && espera(pid, &st, 0) == pid && (st >> 8) == 6, "clone(SIGCHLD)");
	if (en_hispa) mira(sis6(S_CLONE, 0x100 | 17, 0, 0, 0, 0, 0) == -EINVAL, "clone con CLONE_VM: a\xc3\xban no (-EINVAL)");

	/* execve: argv, envp y O_CLOEXEC */
	a = sis(S_OPEN, ruta, 0x80000, 0);
	b = sis(S_OPEN, ruta, 0, 0);
	mira(a >= 3 && b >= 3, "open con O_CLOEXEC y sin");
	mira(sis(S_FCNTL, a, 1, 0) == 1 && sis(S_FCNTL, b, 1, 0) == 0, "fcntl(F_GETFD)");
	num(ta, a); num(tb, b);
	junta(ruta, base, "HIJOS");
	av[0] = "HIJOS"; av[1] = "hijo"; av[2] = ta; av[3] = tb; av[4] = 0;
	ev[0] = "HIJOS=1"; ev[1] = 0;
	pid = sis(S_FORK, 0, 0, 0);
	if (pid == 0) { sis(S_EXECVE, ruta, av, ev); sal(99); }
	mira(espera(pid, &st, 0) == pid && (st >> 8) == 42, "execve: argv, envp y O_CLOEXEC (el hijo acaba con 42)");
	pon("  execve de HIJOS en un hijo: sus argumentos y su entorno, lo de O_CLOEXEC cerrado: "); pon_n(st >> 8); pon("\n");
	sis(S_CLOSE, a, 0, 0);
	sis(S_CLOSE, b, 0, 0);
	junta(ruta, base, "NOESTA");
	mira(sis(S_EXECVE, ruta, av, ev) == -ENOENT, "execve de uno que no est\xc3\xa1: -ENOENT");
	junta(ruta, base, "SALUDO.TXT");
	r = sis(S_EXECVE, ruta, av, ev);
	mira(r == -ENOEXEC || (!en_hispa && r == -EACCES), "execve de un texto: -ENOEXEC");
	mira(sis(S_EXECVE, base, av, ev) == -EACCES, "execve de una carpeta: -EACCES");
	mira(sis(S_EXECVE, 16, av, ev) == -EFAULT, "execve con un puntero malo: -EFAULT");

	/* rusage, a cero */
	pid = sis(S_FORK, 0, 0, 0);
	if (pid == 0) sal(0);
	memset(ru, 0xFF, sizeof ru);
	mira(sis6(S_WAIT4, pid, (i64)&st, 0, (i64)ru, 0, 0) == pid && (!en_hispa || ru[0] == 0), "wait4 con rusage");

	/* treinta seguidos */
	vistos = 0;
	for (i = 0; i < 30; i++) {
		pid = sis(S_FORK, 0, 0, 0);
		if (pid == 0) sal((int)i);
		if (pid > 0 && espera(pid, &st, 0) == pid && (st >> 8) == i) vistos++;
	}
	mira(vistos == 30, "treinta fork y wait4 seguidos");

	/* un huérfano (el nieto sigue cuando su padre ya ha acabado) y un zombi que nadie recoge */
	pid = sis(S_FORK, 0, 0, 0);
	if (pid == 0) {
		if (sis(S_FORK, 0, 0, 0) == 0) { duerme_ms(100); sal(0); }
		sal(0);
	}
	mira(espera(pid, &st, 0) == pid && st == 0, "un hijo que deja un nieto hu\xc3\xa9rfano");
	pid = sis(S_FORK, 0, 0, 0);
	if (pid == 0) sal(3);
	pon("  un nieto hu\xc3\xa9rfano y un zombi sin recoger (los suelta el n\xc3\xba" "cleo)\n");

	consola();
	plazos();
	if (fallos == 0) { pon("HIJOS: todo bien ("); pon_n(pruebas); pon(" pruebas)\n"); }
	else { pon("HIJOS: "); pon_n(fallos); pon(" MAL\n"); }
	return fallos;
}
