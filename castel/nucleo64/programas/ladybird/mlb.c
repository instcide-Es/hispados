/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/ladybird/mlb.c - Hispa-64, etapa 10 f (docs/64/HISPA-64.md,
 * sección 23): MLB corre TestLB (las pruebas de LibUnicode, LibURL, LibIPC,
 * LibCrypto y LibGfx de Ladybird, de terceros/ladybird/Tests, compiladas con hazlb.sh:
 * C++23 y Rust, con musl) y dice cuántos grupos y casos han salido bien.
 * Las pruebas de LibGfx leen sus archivos (test-inputs: imágenes, letras) de
 * la carpeta donde se corren y de LADYBIRD_SOURCE_DIR (/tmp/lb): MLB lleva
 * dentro esos archivos (un tar, lb.tar, con .incbin), los deja en /tmp/lb y
 * corre TestLB en /tmp/lb/Tests/LibGfx; al acabar, los borra.
 * Cada grupo se corre aparte («TestLB --suite GRUPO»), con un plazo de 300 s.
 * Lo que dice TestLB va a /tmp/testlb.txt (al puerto serie, solo el resumen).
 * Los fallos «conocidos» (abajo: los mismos en Linux, con el porqué) no
 * cuentan; cualquier otro, sí.
 * Al final, «MLB: todo bien (N pruebas)» (N, los casos bien) y código 0. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <spawn.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <ftw.h>
#include <signal.h>
#include <sys/utsname.h>

extern char **environ;
extern const unsigned char lb_tar[], lb_tar_fin[];

/* los fallos conocidos: el archivo y la línea (lo que dice LibTest tras «FAIL: .../»), y por qué */
static const struct { const char *dónde, *porqué; } conocidos[] = {
	{ "Tests/LibGfx/TestImageDecoder.cpp:1545:",
	  "AVIF de 10 bits: un píxel pasado de YUV a RGB sale con 2 de diferencia; Ladybird lo espera de la libyuv entera, aquí va el trozo de libavif (también en Linux)" },
};
#define N_CONOCIDOS (sizeof conocidos / sizeof conocidos[0])

/* los grupos que aún no salen en Hispa-64 por algo que le falta al núcleo
 * (no a Ladybird ni a lo compilado: en Linux salen bien): se corren y se dice
 * cómo acaban, con un plazo de 30 s, pero no cuentan. Quitar de aquí en cuanto
 * el núcleo lo tenga. */
static const struct { const char *grupo, *porqué; } pendientes[] = {
	{ "TestURLPattern", "se queda colgado en url_pattern_matches_named_groups (LibRegex, Rust): por mirar" },
	{ "TestConnection", "sync_wait_on_a_closed_peer_that_owns_this_process_shuts_down: el otro lado cerrado no se ve igual que en Linux" },
	{ "TestTransportSocket", "abre más descriptores de los que deja Hispa-64 (EMFILE)" },
	{ "TestSharedFontProvider", "pide symlink (la llamada 88), que Hispa-64 aún no tiene" },
};
#define N_PENDIENTES (sizeof pendientes / sizeof pendientes[0])

static const char *pendiente(const char *g)
{
	for (unsigned i = 0; i < N_PENDIENTES; i++) if (!strcmp(pendientes[i].grupo, g)) return pendientes[i].porqué;
	return NULL;
}

/* los grupos de TestLB (uno por archivo de pruebas de Ladybird) */
static const char *grupos_lb[] = {
	"TestBidi", "TestCollator", "TestDisplayNames", "TestFullwidthMapping", "TestIDNA", "TestLocale", "TestSegmenter", "TestTimeZone",
	"TestUnicodeCharacterTypes", "TestUnicodeNormalization",                                       /* LibUnicode */
	"TestString", "TestUtf16String",                                                               /* AK, con LibUnicode */
	"TestPublicSuffix", "TestURL", "TestURLPattern",                                               /* LibURL */
	"TestConnection", "TestDecoder", "TestFile", "TestTransportSocket",                            /* LibIPC */
	"TestEd25519", "TestHash", "TestHMAC", "TestRSA",                                              /* LibCrypto (la de Hispa-64) */
	"TestColor", "TestFont", "TestFontCatalog", "TestImageWriter", "TestRect", "TestSharedFontProvider", "TestWOFF", "TestWOFF2",
	"TestYUVData", "TestBitmapSequence", "TestShareableBitmap", "TestImageDecoder",                /* LibGfx */
};
#define N_GRUPOS (sizeof grupos_lb / sizeof grupos_lb[0])

static int mkdir_p(char *ruta)
{
	for (char *p = ruta + 1; *p; p++)
		if (*p == '/') { *p = 0; if (mkdir(ruta, 0755) && errno != EEXIST) return -1; *p = '/'; }
	return mkdir(ruta, 0755) && errno != EEXIST ? -1 : 0;
}

/* abre el tar (ustar: cabeceras de 512, el tamaño en octal) en base */
static int abre_tar(const char *base, int *n)
{
	const unsigned char *p = lb_tar, *fin = lb_tar_fin;
	char ruta[1024];
	*n = 0;
	while (p + 512 <= fin && p[0]) {
		char nombre[256], prefijo[156];
		memcpy(nombre, p, 100); nombre[100] = 0;
		memcpy(prefijo, p + 345, 155); prefijo[155] = 0;
		unsigned long largo = strtoul((const char *)p + 124, NULL, 8);
		char tipo = p[156];
		if (strstr(nombre, "..") || strstr(prefijo, "..")) return -1;
		snprintf(ruta, sizeof ruta, "%s/%s%s%s", base, prefijo, prefijo[0] ? "/" : "", nombre);
		p += 512;
		if (p + largo > fin) return -1;
		if (tipo == '5') {
			size_t l = strlen(ruta);
			if (l && ruta[l - 1] == '/') ruta[l - 1] = 0;
			if (mkdir_p(ruta)) return -1;
		} else if (tipo == '0' || tipo == 0) {
			char *b = strrchr(ruta, '/');
			*b = 0; if (mkdir_p(ruta)) return -1; *b = '/';
			int fd = open(ruta, O_WRONLY | O_CREAT | O_TRUNC, 0644);
			if (fd < 0) return -1;
			for (unsigned long h = 0; h < largo;) {
				ssize_t w = write(fd, p + h, largo - h);
				if (w <= 0) { close(fd); return -1; }
				h += w;
			}
			close(fd);
			(*n)++;
		}
		p += (largo + 511) / 512 * 512;
	}
	return 0;
}

/* lo último que ha dicho TestLB (hasta 8 líneas), para saber dónde se ha quedado */
static void cola(const char *sal)
{
	FILE *f = fopen(sal, "r");
	char l[512], u[8][256];
	int n = 0;
	while (f && fgets(l, sizeof l, f)) { l[strcspn(l, "\n")] = 0; snprintf(u[n % 8], sizeof u[0], "%.250s", l); n++; }
	if (f) fclose(f);
	for (int i = n > 8 ? n - 8 : 0; i < n; i++) printf("  TestLB dice: %s\n", u[i % 8]);
}

/* corre argv (con lo que dice, añadido a sal) con un plazo de «plazo» s: 1 si
 * ha acabado (su estado en *st), 0 si no (y lo cierra con SIGKILL), -errno si
 * no arranca; en *ms, cuánto ha tardado */
static int corre(const char *prog, char **argv, const char *sal, int plazo, int *st, long *ms)
{
	posix_spawn_file_actions_t fa;
	posix_spawn_file_actions_init(&fa);
	posix_spawn_file_actions_addopen(&fa, 1, sal, O_WRONLY | O_CREAT | O_APPEND, 0644);
	posix_spawn_file_actions_adddup2(&fa, 1, 2);
	pid_t pid;
	struct timespec g0, g1;
	clock_gettime(CLOCK_MONOTONIC, &g0);
	int e = posix_spawn(&pid, prog, &fa, NULL, argv, environ);
	posix_spawn_file_actions_destroy(&fa);
	if (e) return -e;
	int hecho = 0;
	for (;;) {
		if (waitpid(pid, st, WNOHANG) == pid) { hecho = 1; break; }
		clock_gettime(CLOCK_MONOTONIC, &g1);
		if (g1.tv_sec - g0.tv_sec >= plazo) break;
		struct timespec d = { 0, 10000000 };
		nanosleep(&d, NULL);
	}
	if (!hecho) { kill(pid, SIGKILL); waitpid(pid, st, 0); }
	clock_gettime(CLOCK_MONOTONIC, &g1);
	*ms = (g1.tv_sec - g0.tv_sec) * 1000 + (g1.tv_nsec - g0.tv_nsec) / 1000000;
	return hecho;
}

/* ¿dice gs alguno de los fallos conocidos? */
static int conocido_en(const char *gs)
{
	FILE *f = fopen(gs, "r");
	char l[2048];
	int si = 0;
	while (f && fgets(l, sizeof l, f))
		for (unsigned i = 0; i < N_CONOCIDOS; i++) if (strstr(l, conocidos[i].dónde)) si = 1;
	if (f) fclose(f);
	return si;
}

/* añade lo de b al final de a */
static void añade(const char *a, const char *b)
{
	FILE *fa = fopen(a, "a"), *fb = fopen(b, "r");
	char l[4096];
	size_t n;
	while (fa && fb && (n = fread(l, 1, sizeof l, fb)) > 0) fwrite(l, 1, n, fa);
	if (fa) fclose(fa);
	if (fb) fclose(fb);
}

static int borra_uno(const char *ruta, const struct stat *st, int tipo, struct FTW *ftw)
{
	(void)st; (void)tipo; (void)ftw;
	return remove(ruta);
}

/* borra la carpeta, con todo lo de dentro (lo más hondo primero) */
static void borra(const char *ruta)
{
	nftw(ruta, borra_uno, 16, FTW_DEPTH | FTW_PHYS);
}

int main(void)
{
	char carpeta[512], prog[600];
	ssize_t r = readlink("/proc/self/exe", carpeta, sizeof carpeta - 1);
	if (r > 0) { carpeta[r] = 0; char *b = strrchr(carpeta, '/'); if (b) *b = 0; } else strcpy(carpeta, ".");
	snprintf(prog, sizeof prog, "%s/TestLB", carpeta);
	setvbuf(stdout, NULL, _IOLBF, 0);
	printf("MLB: las pruebas de LibUnicode, LibURL, LibIPC, LibCrypto y LibGfx de Ladybird (TestLB: C++23 con GCC %d.%d, Rust y musl)\n", __GNUC__, __GNUC_MINOR__);
	int n = 0;
	char base[] = "/tmp/lb";
	borra(base);
	if (abre_tar(base, &n)) { printf("  no se pueden dejar los archivos de las pruebas en /tmp/lb (%s)\nMLB: 1 mal de 1\n", strerror(errno)); return 1; }
	printf("  %d archivos de las pruebas (%zu octetos) en /tmp/lb\n", n, (size_t)(lb_tar_fin - lb_tar));
	if (chdir("/tmp/lb/Tests/LibGfx")) { printf("  sin /tmp/lb/Tests/LibGfx\nMLB: 1 mal de 1\n"); return 1; }
	const char *sal = "/tmp/testlb.txt";
	unlink(sal);
	struct timespec t0, t1;
	clock_gettime(CLOCK_MONOTONIC, &t0);
	int colgados = 0, salidas_mal = 0, n_pend = 0;
	struct utsname un;
	int en_hispa = uname(&un) == 0 && strncmp(un.sysname, "Hispa", 5) == 0;
	/* cada grupo por separado (TestLB --suite GRUPO, lo mismo que hace TestLB
	 * solo, pero con un plazo para cada uno: 300 s), diciendo cómo acaba */
	/* primero, que arranca: «TestLB --list» (la lista de los casos) */
	{
		char *argv[] = { prog, "--list", NULL };
		int s1 = 0;
		long m;
		int r = corre(prog, argv, sal, 120, &s1, &m);
		if (r < 0) { printf("  TestLB no se puede poner en marcha (%s)\nMLB: 1 mal de 1\n", strerror(-r)); return 1; }
		int n = 0;
		FILE *f = fopen(sal, "r");
		char l[512];
		while (f && fgets(l, sizeof l, f)) if (strncmp(l, "    ", 4) == 0) n++;
		if (f) fclose(f);
		if (!r) { printf("  TestLB --list: no acaba en 120 s\n"); cola(sal); printf("MLB: 1 mal de 1\n"); return 1; }
		printf("  TestLB --list: %d casos (en %ld ms)\n", n, m);
		unlink(sal);
	}
	for (unsigned g = 0; g < N_GRUPOS; g++) {
		char *argv[] = { prog, "--suite", (char *)grupos_lb[g], NULL };
		int s1 = 0;
		long m;
		const char *gs = "/tmp/testlb1.txt";
		unlink(gs);
		const char *pend = en_hispa ? pendiente(grupos_lb[g]) : NULL;
		int r = corre(prog, argv, gs, pend ? 30 : 300, &s1, &m);
		if (r < 0) { printf("  TestLB no se puede poner en marcha (%s)\nMLB: 1 mal de 1\n", strerror(-r)); return 1; }
		if (pend) {
			int bien = r && WIFEXITED(s1) && WEXITSTATUS(s1) == 0;
			printf("  pendiente en Hispa-64: %s, %s (%s)\n", grupos_lb[g], !r ? "colgado" : bien ? "¡ya sale bien!" : "no sale", pend);
			n_pend++;
			unlink(gs);
			continue;
		}
		if (!r) colgados++;
		if (!r || !(WIFEXITED(s1) && WEXITSTATUS(s1) == 0)) {
			printf("  %s: %s %d (en %ld ms)\n", grupos_lb[g], !r ? "colgado: cerrado con la señal" : WIFEXITED(s1) ? "acaba con el código" : "acaba con la señal",
			       !r ? SIGKILL : WIFEXITED(s1) ? WEXITSTATUS(s1) : WTERMSIG(s1), m);
			salidas_mal++;
			/* lo último que ha dicho, salvo si es un fallo conocido */
			if (!conocido_en(gs)) cola(gs);
		}
		añade(sal, gs);
		unlink(gs);
	}
	clock_gettime(CLOCK_MONOTONIC, &t1);
	long ms = (t1.tv_sec - t0.tv_sec) * 1000 + (t1.tv_nsec - t0.tv_nsec) / 1000000;
	FILE *f = fopen(sal, "r");
	int casos = 0, casos_mal = 0, grupos = 0, bien = 0, malas = 0, sabidas = 0, dichas = 0, grupos_mal = 0;
	int visto[N_CONOCIDOS] = { 0 };
	char l[2048];
	while (f && fgets(l, sizeof l, f)) {
		int a, b, c;
		l[strcspn(l, "\n")] = 0;
		if (strncmp(l, "Completed test '", 16) == 0) casos++;
		else if (strncmp(l, "Failed test '", 13) == 0) { casos++; casos_mal++; }
		else if (sscanf(l, "All %d tests passed.", &a) == 1 || sscanf(l, "All %d test passed.", &a) == 1) { grupos++; bien += a; }
		else if (sscanf(l, "Out of %d tests, %d passed and %d failed.", &a, &b, &c) == 3) { grupos++; grupos_mal++; bien += b; }
		else if (strstr(l, "FAIL") || strstr(l, "ASSERTION") || strstr(l, "VERIFICATION")) {
			unsigned i;
			for (i = 0; i < N_CONOCIDOS; i++) if (strstr(l, conocidos[i].dónde)) break;
			if (i < N_CONOCIDOS) { if (!visto[i]++) sabidas++; continue; }
			malas++;
			if (dichas++ < 8) {
				const char *t = strstr(l, "Tests/");
				printf("  TestLB: %.200s\n", t ? t : l);
			}
		}
	}
	if (f) fclose(f);
	unlink(sal);
	borra(base);
	/* bien: sin fallos que no se sepan; los grupos con fallos, solo los de los conocidos (uno por fallo conocido) */
	int ok = colgados == 0 && malas == 0 && casos > 0 && grupos_mal == sabidas && salidas_mal == sabidas && casos_mal == sabidas && bien + sabidas == casos;
	if (sabidas) printf("  %d grupos, %d casos: %d bien y %d conocido%s\n", grupos, casos, bien, sabidas, sabidas > 1 ? "s" : "");
	else printf("  %d grupos, %d casos: todos bien\n", grupos, casos);
	for (unsigned i = 0; i < N_CONOCIDOS; i++)
		if (visto[i]) printf("  conocido (igual en Linux): %s %s\n", conocidos[i].dónde, conocidos[i].porqué);
	printf("  TestLB, los %zu grupos uno tras otro, en %ld ms\n", N_GRUPOS, ms);
	if (n_pend) printf("  %d grupos pendientes en Hispa-64 (no cuentan)\n", n_pend);
	if (ok) printf("MLB: todo bien (%d pruebas)\n", bien);
	else printf("MLB: %d mal de %d\n", malas ? malas : 1, casos);
	return ok ? 0 : 1;
}
