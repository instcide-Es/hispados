/* hola.c - Hispa-64, etapa 4 (docs/64/HISPA-64.md, sección 10): el primer
 * programa en C en el anillo 3. Un ELF64 estático sin biblioteca, con las
 * llamadas al sistema directas (las de Linux): escribe, mira sus argumentos y
 * el vector auxiliar, getpid, uname, el reloj, nanosleep, arch_prctl (FS),
 * set_tid_address, lee un archivo de la unidad en memoria (open, read,
 * lseek, close), writev, sched_yield, una llamada que no existe, y unas
 * cuentas con coma flotante (SSE). Acaba con 0 si todo ha ido bien.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "sis.h"

extern char _start[];
static u64 bloque_tls[4] = { 0, 0x1234, 0, 0 };

int principal(i64 *sp)
{
	i64 argc = sp[0], i, r, fd;
	char **argv = (char **)(sp + 1);
	char **envp = argv + argc + 1;
	u64 *aux, pagesz = 0, entry = 0, phdr = 0, phnum = 0, rnd = 0;
	u64 ts[2], ts2[2], fs = 0, iov[6];
	char nombre[390], t[200];
	volatile double d = 0;

	pon("Hola desde el anillo 3: soy HOLA, un programa en C (ELF64 est\xc3\xa1tico, sin biblioteca)\n");
	pon("  argc = "); pon_n(argc); pon(":");
	for (i = 0; i < argc; i++) { pon(" "); pon(argv[i]); }
	pon("\n");
	while (*envp) envp++;
	for (aux = (u64 *)(envp + 1); aux[0]; aux += 2) {
		if (aux[0] == 6) pagesz = aux[1];
		if (aux[0] == 9) entry = aux[1];
		if (aux[0] == 3) phdr = aux[1];
		if (aux[0] == 5) phnum = aux[1];
		if (aux[0] == 25) rnd = aux[1];
	}
	mira(((u64)sp & 15) == 0, "la pila no empieza alineada a 16");
	mira(pagesz == 4096, "AT_PAGESZ");
	mira(entry == (u64)_start, "AT_ENTRY");
	mira(phdr != 0 && phnum > 0 && *(unsigned *)phdr <= 6, "AT_PHDR");
	mira(rnd != 0 && (((u64 *)rnd)[0] | ((u64 *)rnd)[1]) != 0, "AT_RANDOM");
	pon("  el vector auxiliar: AT_PAGESZ "); pon_n(pagesz); pon(", AT_ENTRY "); pon_hex(entry);
	pon(", AT_PHDR con "); pon_n(phnum); pon(" segmentos, AT_RANDOM "); pon(rnd ? "s\xc3\xad" : "no"); pon("\n");
	r = sis(S_GETPID, 0, 0, 0);
	pon("  getpid: "); pon_n(r); pon("\n");
	mira(r > 0, "getpid");
	mira(sis(S_SET_TID, &fd, 0, 0) == r, "set_tid_address");
	mira(sis(S_UNAME, nombre, 0, 0) == 0, "uname");
	pon("  uname: "); pon(nombre); pon(" "); pon(nombre + 130); pon(" "); pon(nombre + 260); pon("\n");
	sis(S_CLOCK_GETTIME, 1, ts, 0);
	ts2[0] = 0; ts2[1] = 20000000;
	mira(sis(S_NANOSLEEP, ts2, 0, 0) == 0, "nanosleep");
	sis(S_CLOCK_GETTIME, 1, ts2, 0);
	r = (i64)((ts2[0] - ts[0]) * 1000000000 + ts2[1] - ts[1]) / 1000000;
	pon("  nanosleep de 20 ms: han pasado "); pon_n(r); pon(" ms\n");
	mira(r >= 20 && r < 1000, "nanosleep: el tiempo");
	mira(sis(S_CLOCK_GETTIME, 0, ts, 0) == 0 && ts[0] > 1700000000, "CLOCK_REALTIME");
	pon("  la hora (CLOCK_REALTIME): "); pon_n(ts[0]); pon(" segundos de Unix (a\xc3\xb1o ");
	pon_n(1970 + (i64)(ts[0] / 31556952)); pon(")\n");
	mira(sis(S_CLOCK_GETTIME, 99, ts, 0) == -EINVAL, "clock_gettime de un reloj que no hay");
	mira(sis(S_ARCH_PRCTL, 0x1002, bloque_tls + 1, 0) == 0, "arch_prctl(ARCH_SET_FS)");
	__asm__ volatile ("mov %%fs:0, %0" : "=r"(fs));
	sis(S_ARCH_PRCTL, 0x1003, &ts[0], 0);
	pon("  arch_prctl: FS en "); pon_hex(ts[0]); pon("; %fs:0 da "); pon_hex(fs); pon("\n");
	mira(fs == 0x1234 && ts[0] == (u64)(bloque_tls + 1), "arch_prctl: FS");
	fd = sis(S_OPEN, "/SALUDO.TXT", 0, 0);
	mira(fd >= 3, "open de SALUDO.TXT");
	r = sis(S_READ, fd, t, sizeof t - 1);
	if (r < 0) r = 0;
	t[r] = 0;
	while (r > 0 && t[r - 1] == '\n') t[--r] = 0;
	pon("  la unidad en memoria: SALUDO.TXT dice \xc2\xab"); pon(t); pon("\xc2\xbb\n");
	mira(r > 10, "read de SALUDO.TXT");
	mira(sis(S_LSEEK, fd, 0, 2) > 10, "lseek al final");
	mira(sis(S_LSEEK, fd, 5, 0) == 5 && sis(S_READ, fd, t, 4) == 4, "lseek y read");
	mira(sis(S_LSEEK, 1, 0, 0) == -ESPIPE, "lseek de la consola");
	mira(sis(S_CLOSE, fd, 0, 0) == 0 && sis(S_CLOSE, fd, 0, 0) == -EBADF, "close dos veces");
	mira(sis(S_OPEN, "NOESTA.TXT", 0, 0) == -ENOENT, "open de uno que no est\xc3\xa1");
	mira(sis(S_OPEN, "SALUDO.TXT", 1, 0) == -EROFS, "open para escribir");
	mira(sis6(S_OPENAT, -100, (i64)"saludo.txt", 0, 0, 0, 0) >= 3, "openat (sin mirar may\xc3\xbasculas)");
	mira(sis(S_IOCTL, 1, 0x5413, t) == -ENOTTY, "ioctl");
	iov[0] = (u64)"  writev: "; iov[1] = 10;
	iov[2] = (u64)"en tres "; iov[3] = 8;
	iov[4] = (u64)"trozos\n"; iov[5] = 7;
	mira(sis(S_WRITEV, 1, iov, 3) == 25, "writev");
	mira(sis(S_YIELD, 0, 0, 0) == 0, "sched_yield");
	mira(sis(400, 0, 0, 0) == -ENOSYS, "una llamada que no existe");
	for (i = 1; i <= 1000; i++) d += 1.0 / (double)(i * (i + 1));
	pon("  coma flotante (SSE): la suma de 1/(n(n+1)) hasta 1000, por un mill\xc3\xb3n: "); pon_n((i64)(d * 1000000.0)); pon("\n");
	mira((i64)(d * 1000000.0) == 999000, "la coma flotante");
	if (fallos == 0) { pon("HOLA: todo bien ("); pon_n(pruebas); pon(" pruebas)\n"); }
	else { pon("HOLA: "); pon_n(fallos); pon(" MAL\n"); }
	return fallos;
}
