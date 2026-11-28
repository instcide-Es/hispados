/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/ladybird/mweb.c - Hispa-64, etapa 10 g (docs/64/HISPA-64.md,
 * sección 24): MWEB prueba Ladybird sin ventana en Hispa-64: el Hub de Ladybird
 * con «--headless=text» (el navegador sin ventana: el Hub lanza WebContent,
 * RequestServer, ImageDecoder y Compositor, y saca el texto de la página)
 * sobre una página de prueba (hispa64/paginas/prueba.html: HTML, CSS y
 * JavaScript que cambia la página), y compara el texto con el que debe salir.
 *
 * Ladybird busca sus recursos (los temas, ladybird.css...) en
 * PREFIJO/share/Lagom, con PREFIJO la carpeta de encima de la del programa
 * (/proc/self/exe), y los programas que lanza en PREFIJO/bin; sin
 * fontconfig, las letras en las carpetas de XDG_DATA_DIRS (.../fonts). Los
 * programas están en la unidad («/bin/Hub»: PREFIJO es «/»). MWEB lleva
 * dentro los recursos (Base/res, sin lo que no hace falta), las letras
 * DejaVu y la página (un tar, web.tar, con .incbin), los deja en
 * /tmp/web/share (Lagom, fonts) y /tmp/web/paginas, hace el enlace
 * /share -> /tmp/web/share (en la unidad, de solo leer, el enlace va a la
 * tabla de enlaces en memoria del núcleo) y lanza /bin/Hub con
 * XDG_DATA_DIRS=/share. Al acabar, quita /share y borra /tmp/web.
 * En Linux (para probar), PREFIJO es la carpeta de encima de la de MWEB, y
 * el enlace es PREFIJO/share (MWEB_PREFIJO=... lo cambia).
 * Uso: MWEB                 la página de prueba, comparada
 *      MWEB --cripto        [etapa 10 i] las pruebas de la WebCrypto de
 *                           Ladybird, comparadas con lo de Linux (mwebcripto.c)
 *      MWEB --prepara       [etapa 10 h] solo deja los recursos en /tmp/web
 *                           y el enlace /share (para NAVEGADOR, Ladybird con
 *                           ventana), y no los quita al acabar
 *      MWEB URL [MODO [CA]] cualquier página (file://, https://...), con
 *                           MODO text (lo de siempre), layout-tree o
 *                           screenshot (a /tmp/web/captura.png); dice lo que
 *                           saca, sin comparar; CA, el archivo de las
 *                           autoridades de certificados (PEM) para https
 *                           (sin él, las raíces de Let's Encrypt que lleva:
 *                           hispa64/autoridades.pem)
 * Al final, «MWEB: todo bien» y el código 0. */
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

extern char **environ;
extern const unsigned char web_tar[], web_tar_fin[];
int mweb_cripto(const char *hub);   /* [etapa 10 i] mwebcripto.c */

/* lo que tiene que decir «--headless=text» de la página de prueba (las
 * líneas con texto, sin los blancos de los lados) */
static const char *esperado[] = {
	"Hola desde Hispa-64",
	"Ladybird sin ventana, con LibWeb y LibJS.",
	"uno",
	"dos",
	"tres",
	"JavaScript ha corrido: 1+4+9 = 14.",
	"Fecha: 27 de noviembre de 2026; ñandú en mayúsculas: ÑANDÚ.",
};
#define N_ESPERADO (sizeof esperado / sizeof esperado[0])

static int mkdir_p(char *ruta)
{
	for (char *p = ruta + 1; *p; p++)
		if (*p == '/') { *p = 0; if (mkdir(ruta, 0755) && errno != EEXIST) return -1; *p = '/'; }
	return mkdir(ruta, 0755) && errno != EEXIST ? -1 : 0;
}

static int abre_tar(const char *base, int *n)
{
	const unsigned char *p = web_tar, *fin = web_tar_fin;
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

static int borra_uno(const char *ruta, const struct stat *st, int tipo, struct FTW *ftw)
{
	(void)st; (void)tipo; (void)ftw;
	return remove(ruta);
}

static void borra(const char *ruta)
{
	nftw(ruta, borra_uno, 16, FTW_DEPTH | FTW_PHYS);
}

static int corre(const char *prog, char **argv, const char *sal, int plazo, int *st, long *ms)
{
	posix_spawn_file_actions_t fa;
	posix_spawn_file_actions_init(&fa);
	posix_spawn_file_actions_addopen(&fa, 1, sal, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	posix_spawn_file_actions_addopen(&fa, 2, "/tmp/web/errores.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
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
		struct timespec d = { 0, 20000000 };
		nanosleep(&d, NULL);
	}
	if (!hecho) { kill(pid, SIGKILL); waitpid(pid, st, 0); }
	clock_gettime(CLOCK_MONOTONIC, &g1);
	*ms = (g1.tv_sec - g0.tv_sec) * 1000 + (g1.tv_nsec - g0.tv_nsec) / 1000000;
	return hecho;
}

/* las líneas de un archivo, sin blancos a los lados, y las que no están vacías */
static int líneas(const char *archivo, char l[][512], int máx)
{
	FILE *f = fopen(archivo, "r");
	char b[4096];
	int n = 0;
	while (f && n < máx && fgets(b, sizeof b, f)) {
		char *p = b, *q;
		while (*p == ' ' || *p == '\t') p++;
		q = p + strlen(p);
		while (q > p && (q[-1] == '\n' || q[-1] == '\r' || q[-1] == ' ' || q[-1] == '\t')) *--q = 0;
		if (*p) snprintf(l[n++], 512, "%s", p);
	}
	if (f) fclose(f);
	return n;
}

static void dice(const char *archivo, const char *qué, int máx)
{
	static char l[64][512];
	int n = líneas(archivo, l, 64);
	for (int i = 0; i < n && i < máx; i++) printf("  %s: %.200s\n", qué, l[i]);
}

int main(int argc, char **argv)
{
	char carpeta[512], hub[600];
	ssize_t r = readlink("/proc/self/exe", carpeta, sizeof carpeta - 1);
	if (r > 0) { carpeta[r] = 0; char *b = strrchr(carpeta, '/'); if (b) *b = 0; } else strcpy(carpeta, ".");
	setvbuf(stdout, NULL, _IOLBF, 0);
	int prepara = argc > 1 && !strcmp(argv[1], "--prepara");
	if (!prepara)
		printf("MWEB: Ladybird sin ventana (el Hub con --headless: LibWeb, LibJS, WebContent, RequestServer, ImageDecoder y Compositor)\n");
	int n = 0;
	char base[] = "/tmp/web";
	borra(base);
	if (abre_tar(base, &n)) { printf("  no se pueden dejar los recursos en /tmp/web (%s)\nMWEB: 1 mal de 1\n", strerror(errno)); return 1; }
	printf("  %d archivos de los recursos y la página (%zu octetos) en /tmp/web\n", n, (size_t)(web_tar_fin - web_tar));
	if (!carpeta[0]) strcpy(carpeta, "/bin");     /* (MWEB en «/»: los programas, también en /bin) */
	/* PREFIJO: la carpeta de encima de la de los programas («/» en Hispa-64) */
	char prefijo[512], share[600], xdg[640];
	snprintf(prefijo, sizeof prefijo, "%s", getenv("MWEB_PREFIJO") ? getenv("MWEB_PREFIJO") : carpeta);
	if (!getenv("MWEB_PREFIJO")) { char *b = strrchr(prefijo, '/'); if (b) *b = 0; }
	snprintf(share, sizeof share, "%s/share", prefijo);
	unlink(share);
	if (symlink("/tmp/web/share", share)) { printf("  no se hace el enlace %s -> /tmp/web/share (%s)\nMWEB: 1 mal de 1\n", share, strerror(errno)); return 1; }
	snprintf(xdg, sizeof xdg, "XDG_DATA_DIRS=%s", share);
	putenv(xdg);
	/* (la casa: Ladybird deja ahí lo suyo, ~/.config y ~/.local; en Hispa-64,
	 * sin usuarios, HOME es «/», de solo leer: la de /tmp/web) */
	mkdir("/tmp/web/casa", 0755);
	setenv("HOME", "/tmp/web/casa", 1);
	setenv("XDG_CONFIG_HOME", "/tmp/web/casa/.config", 1);
	setenv("XDG_DATA_HOME", "/tmp/web/casa/.local/share", 1);
	setenv("XDG_CACHE_HOME", "/tmp/web/casa/.cache", 1);
	mkdir("/tmp/web/run", 0700);
	setenv("XDG_RUNTIME_DIR", "/tmp/web/run", 1);         /* (si no, /run/user/0: los sockets de Ladybird) */
	if (prepara) {
		printf("MWEB: los recursos de Ladybird en /tmp/web (%d archivos) y el enlace %s\n", n, share);
		return 0;
	}
	snprintf(hub, sizeof hub, "%s/Hub", carpeta);
	if (argc > 1 && !strcmp(argv[1], "--cripto")) {
		/* [etapa 10 i] las pruebas de la WebCrypto de Ladybird (mwebcripto.c) */
		int r = mweb_cripto(hub);
		unlink(share);
		borra(base);
		return r;
	}
	const char *url = argc > 1 ? argv[1] : "file:///tmp/web/paginas/prueba.html";
	const char *modo = argc > 2 ? argv[2] : "text";
	char opción[64];
	snprintf(opción, sizeof opción, "--headless=%s", modo);
	/* (sin la caja de arena de Linux: seccomp y Landlock, que Hispa-64 no tiene) */
	char certificado[600] = "";
	if (argc > 3) snprintf(certificado, sizeof certificado, "--certificate=%s", argv[3]);
	else if (!strncmp(url, "https:", 6)) snprintf(certificado, sizeof certificado, "--certificate=/tmp/web/share/autoridades.pem");
	/* ([etapa 10 h] con la base de datos de Ladybird: sqlite en modo WAL, con su
	 * «-shm» por mmap MAP_SHARED de escribir de un archivo, que Hispa-64 ya
	 * tiene; antes, «--disable-sql-database») */
	char *av[] = { hub, opción, "--disable-sandbox", "--screenshot-path=/tmp/web/captura.png", (char *)url, certificado[0] ? certificado : NULL, NULL };
	const char *sal = "/tmp/web/salida.txt";
	int st = 0;
	long ms = 0;
	int c = corre(hub, av, sal, 300, &st, &ms);
	if (c < 0) { printf("  el Hub no se puede poner en marcha (%s)\nMWEB: 1 mal de 1\n", strerror(-c)); return 1; }
	printf("  Hub %s %s: %s %d, en %ld ms\n", opción, url, !c ? "colgado, cerrado con la señal" : WIFEXITED(st) ? "acaba con el código" : "acaba con la señal",
	       !c ? SIGKILL : WIFEXITED(st) ? WEXITSTATUS(st) : WTERMSIG(st), ms);
	int ok = c == 1 && WIFEXITED(st) && WEXITSTATUS(st) == 0;
	if (argc > 1) {
		/* otra página: lo que saca */
		dice(sal, "dice", 60);
		if (!strcmp(modo, "screenshot")) {
			struct stat s;
			if (stat("/tmp/web/captura.png", &s) == 0) {
				/* su tamaño (el IHDR del PNG) y, si hay un disco de escribir en
				 * /sata0, una copia (CAPTURA.PNG), para verla fuera */
				unsigned char c8[24] = { 0 };
				int fd = open("/tmp/web/captura.png", O_RDONLY);
				if (fd >= 0) { if (read(fd, c8, 24) != 24) memset(c8, 0, 24); close(fd); }
				unsigned an = (unsigned)c8[16] << 24 | c8[17] << 16 | c8[18] << 8 | c8[19];
				unsigned al = (unsigned)c8[20] << 24 | c8[21] << 16 | c8[22] << 8 | c8[23];
				printf("  la captura: /tmp/web/captura.png, %lld octetos, PNG de %u x %u\n", (long long)s.st_size, an, al);
				struct stat d;
				if (stat("/sata0", &d) == 0 && S_ISDIR(d.st_mode)) {
					int a1 = open("/tmp/web/captura.png", O_RDONLY), a2 = open("/sata0/CAPTURA.PNG", O_WRONLY | O_CREAT | O_TRUNC, 0644);
					char b[4096];
					ssize_t k;
					while (a1 >= 0 && a2 >= 0 && (k = read(a1, b, sizeof b)) > 0) if (write(a2, b, k) != k) break;
					if (a2 >= 0) { fsync(a2); close(a2); printf("  copiada a /sata0/CAPTURA.PNG\n"); }
					if (a1 >= 0) close(a1);
				}
			} else { printf("  sin captura\n"); ok = 0; }
		}
		if (!ok) dice("/tmp/web/errores.txt", "errores", 20);
		unlink(share);
		printf(ok ? "MWEB: todo bien\n" : "MWEB: 1 mal de 1\n");
		return ok ? 0 : 1;
	}
	static char l[256][512];
	int nl = líneas(sal, l, 256), bien = 0;
	for (unsigned i = 0; i < N_ESPERADO; i++) {
		int está = 0;
		for (int k = 0; k < nl; k++) if (!strcmp(l[k], esperado[i])) está = 1;
		if (está) bien++;
		else printf("  falta: «%s»\n", esperado[i]);
	}
	printf("  el texto de la página: %d de %zu líneas como deben\n", bien, N_ESPERADO);
	if (bien != (int)N_ESPERADO) { dice(sal, "dice", 30); ok = 0; }
	if (!ok) dice("/tmp/web/errores.txt", "errores", 20);
	unlink(share);
	borra(base);
	if (ok && bien == (int)N_ESPERADO) printf("MWEB: todo bien (%d líneas)\n", bien);
	else printf("MWEB: %d mal de %zu\n", (int)N_ESPERADO - bien ? (int)N_ESPERADO - bien : 1, N_ESPERADO);
	return ok && bien == (int)N_ESPERADO ? 0 : 1;
}
