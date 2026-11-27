/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/musl/mcandados.c - Hispa-64, etapa 10 f (docs/64/HISPA-64.md,
 * sección 23): MCANDADOS, la prueba de los cerrojos de fcntl (en /tmp):
 *   - F_GETLK sin nada: F_UNLCK; uno de escribir del padre: el hijo lo ve
 *     (con su l_pid) y no puede poner otro encima (EAGAIN), sí al lado;
 *   - los de leer se comparten; uno de escribir encima de ellos, no;
 *   - el mismo dueño cambia el tipo de un trozo (se parte) y quita uno de en
 *     medio; SEEK_CUR, SEEK_END y largo 0 (hasta el final) y negativo;
 *   - cerrar CUALQUIER descriptor del archivo suelta los de POSIX del proceso
 *     (también uno que no es con el que se puso); acabar, también;
 *   - F_SETLKW espera a que el otro suelte (unos 100 ms) y una señal (alarm,
 *     sin SA_RESTART) la corta con EINTR;
 *   - los hilos de un proceso son el mismo dueño;
 *   - los OFD (F_OFD_SETLK): dos open del mismo proceso se estorban; uno de
 *     POSIX y uno OFD también; lo heredado al bifurcar es el mismo dueño; se
 *     sueltan al cerrar el último descriptor; F_OFD_GETLK da l_pid -1;
 *   - los errores: whence malo, l_pid en un OFD y antes del principio
 *     (EINVAL), uno de escribir con el archivo abierto para leer (EBADF);
 *   - sqlite con su VFS de Unix de verdad (el de los cerrojos de fcntl): dos
 *     procesos con la misma base; mientras uno la tiene en BEGIN EXCLUSIVE,
 *     el otro recibe SQLITE_BUSY; al acabar el primero, el otro escribe; al
 *     final, las filas de los dos e integrity_check bien.
 * Al final, «MCANDADOS: todo bien (N pruebas)» y código 0. Corre igual en Linux. */
#define _GNU_SOURCE
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include "comun.h"
#include "sqlite3.h"

#define RUTA "/tmp/mcandados.dat"
#define BASE "/tmp/mcandados.db"

static int pon(int fd, int orden, short tipo, short dónde, off_t ini, off_t largo)
{
	struct flock f = { .l_type = tipo, .l_whence = dónde, .l_start = ini, .l_len = largo };
	return fcntl(fd, orden, &f) == 0 ? 0 : -errno;
}

/* lo que dice F_GETLK (o F_OFD_GETLK) de [ini, ini + largo) para tipo: el tipo que estorba (o F_UNLCK) */
static int mira(int fd, int orden, short tipo, off_t ini, off_t largo, struct flock *f)
{
	memset(f, 0, sizeof *f);
	f->l_type = tipo; f->l_whence = SEEK_SET; f->l_start = ini; f->l_len = largo;
	if (fcntl(fd, orden, f)) return -errno;
	return f->l_type;
}

static int64_t ms_ahora(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (int64_t)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

/* el hijo hace f(fd) y acaba con lo que devuelva (0: bien) */
static int en_hijo(int (*f)(int), int fd)
{
	pid_t h = fork();
	if (h == 0) _exit(f(fd));
	int st = -1;
	waitpid(h, &st, 0);
	return WIFEXITED(st) ? WEXITSTATUS(st) : 100;
}

static pid_t padre;
static int h_ve_el_del_padre(int fd)
{
	struct flock f;
	if (mira(fd, F_GETLK, F_RDLCK, 10, 5, &f) != F_WRLCK) return 1;
	if (f.l_pid != padre || f.l_start != 0 || f.l_len != 100) return 2;
	if (pon(fd, F_SETLK, F_RDLCK, SEEK_SET, 50, 10) != -EAGAIN && pon(fd, F_SETLK, F_RDLCK, SEEK_SET, 50, 10) != -EACCES) return 3;
	if (pon(fd, F_SETLK, F_WRLCK, SEEK_SET, 100, 100) != 0) return 4;
	return 0;
}
static int h_lee_compartido(int fd)
{
	if (pon(fd, F_SETLK, F_RDLCK, SEEK_SET, 200, 100) != 0) return 1;
	if (pon(fd, F_SETLK, F_WRLCK, SEEK_SET, 250, 10) != -EAGAIN) return 2;
	return 0;
}
static int h_trozos(int fd)
{
	struct flock f;
	/* el padre: [0, 20) WR, [20, 30) RD, [30, 40) nada, [40, 100) WR */
	if (mira(fd, F_GETLK, F_RDLCK, 0, 5, &f) != F_WRLCK) return 1;
	if (mira(fd, F_GETLK, F_RDLCK, 20, 10, &f) != F_UNLCK) return 2;
	if (mira(fd, F_GETLK, F_WRLCK, 20, 10, &f) != F_RDLCK || f.l_start != 20 || f.l_len != 10) return 3;
	if (mira(fd, F_GETLK, F_WRLCK, 30, 10, &f) != F_UNLCK) return 4;
	if (mira(fd, F_GETLK, F_WRLCK, 45, 1, &f) != F_WRLCK || f.l_start != 40 || f.l_len != 60) return 5;
	return 0;
}
static int h_libre(int fd)
{
	return pon(fd, F_SETLK, F_WRLCK, SEEK_SET, 0, 0) == 0 ? 0 : 1;
}
static int h_ocupado(int fd)
{
	return pon(fd, F_SETLK, F_WRLCK, SEEK_SET, 0, 0) == -EAGAIN ? 0 : 1;
}
static int h_pon_y_acaba(int fd)
{
	return pon(fd, F_SETLK, F_WRLCK, SEEK_SET, 1000, 10) == 0 ? 0 : 1;
}
static int h_ofd_el_mismo(int fd)
{
	/* lo heredado (lo abierto es el mismo): el mismo dueño */
	return pon(fd, F_OFD_SETLK, F_WRLCK, SEEK_SET, 0, 10) == 0 ? 0 : 1;
}

static void nada(int s) { (void)s; }

static int fd_hilo;
static void *hilo_pone(void *a)
{
	(void)a;
	/* (otro hilo, el mismo proceso: el mismo dueño, no estorba) */
	return (void *)(long)pon(fd_hilo, F_SETLK, F_WRLCK, SEEK_SET, 0, 50);
}

/* ---- sqlite con su VFS de Unix ---- */
static int sql(sqlite3 *db, const char *q) { return sqlite3_exec(db, q, 0, 0, 0); }

static int prueba_sqlite(void)
{
	unlink(BASE); unlink(BASE "-journal");
	sqlite3 *db;
	if (sqlite3_open_v2(BASE, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, 0) != SQLITE_OK) return 1;
	if (sql(db, "CREATE TABLE t(quién TEXT, n INTEGER);") != SQLITE_OK) return 2;
	int tubo[2], vuelta[2];
	if (pipe(tubo) || pipe(vuelta)) return 3;
	pid_t h = fork();
	if (h == 0) {
		/* el hijo: espera a que el padre la tenga en EXCLUSIVE; prueba: BUSY; luego escribe */
		char c;
		sqlite3 *d2;
		if (read(tubo[0], &c, 1) != 1) _exit(10);
		if (sqlite3_open_v2(BASE, &d2, SQLITE_OPEN_READWRITE, 0) != SQLITE_OK) _exit(11);
		int r = sql(d2, "INSERT INTO t VALUES('hijo', 0);");
		c = r == SQLITE_BUSY ? 'B' : 'x';
		if (write(vuelta[1], &c, 1) != 1) _exit(12);
		if (read(tubo[0], &c, 1) != 1) _exit(13);
		sqlite3_busy_timeout(d2, 5000);
		r = sql(d2, "BEGIN; INSERT INTO t VALUES('hijo', 1); INSERT INTO t VALUES('hijo', 2); COMMIT;");
		sqlite3_close(d2);
		_exit(r == SQLITE_OK ? 0 : 14);
	}
	char c = 0;
	int bien = sql(db, "BEGIN EXCLUSIVE; INSERT INTO t VALUES('padre', 1);") == SQLITE_OK;
	if (write(tubo[1], "1", 1) != 1) bien = 0;
	if (read(vuelta[0], &c, 1) != 1) bien = 0;
	int busy = c == 'B';
	bien = bien && sql(db, "INSERT INTO t VALUES('padre', 2); COMMIT;") == SQLITE_OK;
	if (write(tubo[1], "2", 1) != 1) bien = 0;
	int st = -1;
	waitpid(h, &st, 0);
	int hijo_bien = WIFEXITED(st) && WEXITSTATUS(st) == 0;
	sqlite3_stmt *s;
	int filas = -1;
	if (sqlite3_prepare_v2(db, "SELECT count(*) FROM t", -1, &s, 0) == SQLITE_OK && sqlite3_step(s) == SQLITE_ROW) filas = sqlite3_column_int(s, 0);
	sqlite3_finalize(s);
	char integ[16] = "";
	if (sqlite3_prepare_v2(db, "PRAGMA integrity_check", -1, &s, 0) == SQLITE_OK && sqlite3_step(s) == SQLITE_ROW)
		snprintf(integ, sizeof integ, "%s", (const char *)sqlite3_column_text(s, 0));
	sqlite3_finalize(s);
	sqlite3_close(db);
	unlink(BASE);
	MIRA(bien, "sqlite: el padre, BEGIN EXCLUSIVE y COMMIT");
	MIRA(busy, "sqlite: el hijo, SQLITE_BUSY mientras el padre la tiene");
	MIRA(hijo_bien, "sqlite: el hijo escribe después");
	MIRA(filas == 4 && !strcmp(integ, "ok"), "sqlite: las 4 filas e integrity_check");
	printf("  sqlite %s con el VFS de Unix: dos procesos; el segundo, SQLITE_BUSY mientras el primero está en BEGIN EXCLUSIVE; luego escribe; %d filas, integrity_check: %s\n",
	       sqlite3_libversion(), filas, integ);
	return 0;
}

int main(void)
{
	struct flock f;
	padre = getpid();
	printf("MCANDADOS: los cerrojos de fcntl (%s)\n", cómo());
	unlink(RUTA);
	int fd = open(RUTA, O_RDWR | O_CREAT | O_TRUNC, 0644);
	MIRA(fd >= 0, "abrir " RUTA);
	char cero[2048] = { 0 };
	MIRA(write(fd, cero, sizeof cero) == sizeof cero, "2 KB en el archivo");
	/* 1. nada puesto; uno de escribir que el hijo ve */
	MIRA(mira(fd, F_GETLK, F_WRLCK, 0, 0, &f) == F_UNLCK, "F_GETLK sin nada: F_UNLCK");
	MIRA(pon(fd, F_SETLK, F_WRLCK, SEEK_SET, 0, 100) == 0, "F_SETLK de escribir [0, 100)");
	MIRA(en_hijo(h_ve_el_del_padre, fd) == 0, "el hijo lo ve (con el l_pid del padre), no pone encima y sí al lado");
	/* 2. los de leer, compartidos */
	MIRA(pon(fd, F_SETLK, F_RDLCK, SEEK_SET, 200, 100) == 0, "F_SETLK de leer [200, 300)");
	MIRA(en_hijo(h_lee_compartido, fd) == 0, "el hijo también lee ahí; de escribir, no");
	printf("  POSIX: uno de escribir se ve desde otro proceso (con su l_pid) y estorba; los de leer se comparten\n");
	/* 3. el mismo dueño: partir y quitar de en medio */
	MIRA(pon(fd, F_SETLK, F_UNLCK, SEEK_SET, 0, 0) == 0, "quitarlos todos (F_UNLCK, largo 0)");
	MIRA(pon(fd, F_SETLK, F_WRLCK, SEEK_SET, 0, 100) == 0, "otra vez [0, 100) de escribir");
	MIRA(pon(fd, F_SETLK, F_RDLCK, SEEK_SET, 20, 10) == 0, "[20, 30) pasa a ser de leer");
	MIRA(pon(fd, F_SETLK, F_UNLCK, SEEK_SET, 30, 10) == 0, "[30, 40), fuera");
	MIRA(en_hijo(h_trozos, fd) == 0, "el hijo ve [0, 20) de escribir, [20, 30) de leer, [30, 40) libre y [40, 100) de escribir");
	MIRA(lseek(fd, 1500, SEEK_SET) == 1500 && pon(fd, F_SETLK, F_WRLCK, SEEK_CUR, -100, 50) == 0, "SEEK_CUR: [1400, 1450)");
	MIRA(mira(fd, F_GETLK, F_WRLCK, 1400, 1, &f) == F_UNLCK, "lo suyo no le estorba (F_GETLK: F_UNLCK)");
	MIRA(pon(fd, F_SETLK, F_WRLCK, SEEK_END, -48, 0) == 0, "SEEK_END: desde 2000 hasta el final");
	MIRA(pon(fd, F_SETLK, F_WRLCK, SEEK_SET, 1300, -50) == 0, "largo negativo: [1250, 1300)");
	printf("  POSIX: el mismo dueño cambia el tipo de un trozo (se parte), quita de en medio; SEEK_CUR, SEEK_END, largo 0 y negativo\n");
	/* 4. cerrar cualquier descriptor del archivo los suelta */
	int fd2 = open(RUTA, O_RDONLY);
	MIRA(fd2 >= 0 && en_hijo(h_ocupado, fd) == 0, "con lo puesto, el hijo no puede");
	close(fd2);
	MIRA(en_hijo(h_libre, fd) == 0, "al cerrar otro descriptor del mismo archivo, se sueltan todos");
	MIRA(en_hijo(h_pon_y_acaba, fd) == 0 && pon(fd, F_SETLK, F_WRLCK, SEEK_SET, 1000, 10) == 0, "los del que acaba se sueltan");
	printf("  POSIX: cerrar cualquier descriptor del archivo los suelta, y acabar también\n");
	/* 5. F_SETLKW espera; una señal la corta */
	MIRA(pon(fd, F_SETLK, F_WRLCK, SEEK_SET, 500, 10) == 0, "[500, 510) para F_SETLKW");
	pid_t h = fork();
	if (h == 0) {
		int64_t t0 = ms_ahora();
		int r = pon(fd, F_SETLKW, F_WRLCK, SEEK_SET, 505, 1);
		int64_t t = ms_ahora() - t0;
		_exit(r == 0 && t >= 80 && t < 3000 ? 0 : 1);
	}
	struct timespec dz = { 0, 100000000 };
	nanosleep(&dz, 0);
	MIRA(pon(fd, F_SETLK, F_UNLCK, SEEK_SET, 500, 10) == 0, "soltarlo a los 100 ms");
	int st = -1;
	waitpid(h, &st, 0);
	MIRA(WIFEXITED(st) && WEXITSTATUS(st) == 0, "F_SETLKW: el hijo esperó (de 80 ms a 3 s) y lo tuvo");
	MIRA(pon(fd, F_SETLK, F_WRLCK, SEEK_SET, 600, 10) == 0, "[600, 610) para cortar la espera");
	h = fork();
	if (h == 0) {
		struct sigaction sa = { .sa_handler = nada };
		sigaction(SIGALRM, &sa, 0);
		alarm(1);
		int r = pon(fd, F_SETLKW, F_WRLCK, SEEK_SET, 600, 1);
		_exit(r == -EINTR ? 0 : 1);
	}
	st = -1;
	waitpid(h, &st, 0);
	MIRA(WIFEXITED(st) && WEXITSTATUS(st) == 0, "F_SETLKW cortada por una señal: EINTR");
	printf("  F_SETLKW espera a que el otro suelte, y una señal la corta (EINTR)\n");
	/* 6. los hilos: el mismo dueño */
	pthread_t th;
	void *rh;
	fd_hilo = fd;
	MIRA(pon(fd, F_SETLK, F_WRLCK, SEEK_SET, 0, 50) == 0, "[0, 50) de escribir");
	pthread_create(&th, 0, hilo_pone, 0);
	pthread_join(th, &rh);
	MIRA(rh == 0, "otro hilo del mismo proceso pone el mismo trozo (el mismo dueño)");
	MIRA(pon(fd, F_SETLK, F_UNLCK, SEEK_SET, 0, 0) == 0, "todos fuera");
	/* 7. los OFD */
	int a = open(RUTA, O_RDWR), b = open(RUTA, O_RDWR);
	MIRA(a >= 0 && b >= 0, "dos open del mismo archivo");
	MIRA(pon(a, F_OFD_SETLK, F_WRLCK, SEEK_SET, 0, 10) == 0, "F_OFD_SETLK de escribir por el primero");
	MIRA(pon(b, F_OFD_SETLK, F_WRLCK, SEEK_SET, 5, 10) == -EAGAIN, "el segundo open (otro dueño) no puede, aunque sea el mismo proceso");
	MIRA(pon(fd, F_SETLK, F_WRLCK, SEEK_SET, 5, 1) == -EAGAIN, "uno de POSIX del mismo proceso tampoco");
	MIRA(mira(b, F_OFD_GETLK, F_WRLCK, 0, 1, &f) == F_WRLCK && f.l_pid == -1, "F_OFD_GETLK: l_pid -1");
	MIRA(en_hijo(h_ofd_el_mismo, a) == 0, "el hijo, con el descriptor heredado, es el mismo dueño");
	int c = dup(a);
	close(a);
	MIRA(pon(b, F_OFD_SETLK, F_WRLCK, SEEK_SET, 0, 1) == -EAGAIN, "cerrado uno, queda (el dup lo tiene)");
	close(c);
	MIRA(pon(b, F_OFD_SETLK, F_WRLCK, SEEK_SET, 0, 1) == 0, "cerrado el último, se suelta");
	close(b);
	printf("  OFD: dos open del mismo proceso se estorban (y con los de POSIX); lo heredado es el mismo dueño; se sueltan con el último descriptor\n");
	/* 8. los errores */
	struct flock mal = { .l_type = F_WRLCK, .l_whence = 7, .l_start = 0, .l_len = 1 };
	MIRA(fcntl(fd, F_SETLK, &mal) == -1 && errno == EINVAL, "whence malo: EINVAL");
	struct flock ofdmal = { .l_type = F_WRLCK, .l_whence = SEEK_SET, .l_start = 0, .l_len = 1, .l_pid = 33 };
	MIRA(fcntl(fd, F_OFD_SETLK, &ofdmal) == -1 && errno == EINVAL, "un OFD con l_pid: EINVAL");
	int lee = open(RUTA, O_RDONLY);
	MIRA(pon(lee, F_SETLK, F_WRLCK, SEEK_SET, 0, 1) == -EBADF, "de escribir, abierto para leer: EBADF");
	MIRA(pon(lee, F_SETLK, F_RDLCK, SEEK_SET, 0, 1) == 0, "de leer, abierto para leer: bien");
	close(lee);
	MIRA(pon(fd, F_SETLK, F_WRLCK, SEEK_SET, -5, 1) == -EINVAL, "antes del principio: EINVAL");
	printf("  los errores: whence malo, l_pid en un OFD, antes del principio (EINVAL) y de escribir sin poder escribir (EBADF)\n");
	close(fd);
	unlink(RUTA);
	prueba_sqlite();
	return acaba("MCANDADOS");
}
