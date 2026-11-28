/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/ladybird/mwebcripto.c - Hispa-64, etapa 10 i (docs/64/HISPA-64.md,
 * sección 26): «MWEB --cripto», las pruebas de la WebCrypto de Ladybird
 * (terceros/ladybird/Tests/LibWeb/Text/input/Crypto, las mismas que corre su
 * test-web en Linux) en el navegador sin ventana de Hispa-64, comparadas con
 * lo que sale en Linux (Tests/LibWeb/Text/expected/Crypto).
 *
 * Cómo: test-web de Ladybird saca el texto de cada prueba por
 * internals.signalTestIsDone (su objeto «internals» y su canal de pruebas, que
 * aquí no hay). Aquí, MWEB sirve las pruebas por HTTP en 127.0.0.1 (un hilo:
 * http://127.0.0.1 es un contexto seguro, como pide crypto.subtle) con un
 * include.js que lleva delante un «internals» nuestro
 * (hispa64/cripto-preludio.js): cuando acaba una prueba, manda su texto a
 * MWEB (POST /resultado/NOMBRE) y se va a la siguiente (/siguiente/NOMBRE,
 * que MWEB contesta con una redirección). El Hub, con --headless=manual,
 * corre todas en la misma pestaña; si una se queda sin acabar (60 s sin
 * nada), MWEB cierra el Hub y lo arranca otra vez en la que viene detrás.
 * Al final, cada prueba, igual o distinta (con la primera línea que cambia),
 * y «MWEB: la WebCrypto de Ladybird: N de M iguales que en Linux».
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <spawn.h>
#include <time.h>
#include <errno.h>
#include <dirent.h>
#include <signal.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

extern char **environ;
#define MAX_PRUEBAS 200
#define PUERTO 18080
#define RAIZ "/tmp/web/cripto"

static char *nombres[MAX_PRUEBAS];
static char *resultados[MAX_PRUEBAS];
static int n_pruebas, n_hechas, actual = -1;
static struct timespec ultima;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/* las que Ladybird tampoco corre (Tests/LibWeb/TestConfig.ini, [Skipped]):
 * SubtleCrypto-generateKey, «Times out» (genera RSA de 256 bits, que su
 * LibCrypto ya no deja: lo que espera es de antes) */
static const char *saltadas[] = { "SubtleCrypto-generateKey" };
static int saltada(const char *n)
{
	for (size_t i = 0; i < sizeof saltadas / sizeof *saltadas; i++) if (!strcmp(n, saltadas[i])) return 1;
	return 0;
}

static int busca(const char *n)
{
	for (int i = 0; i < n_pruebas; i++) if (!strcmp(nombres[i], n)) return i;
	return -1;
}
static int compara(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }
static char *lee_archivo(const char *ruta, size_t *n)
{
	FILE *f = fopen(ruta, "rb");
	if (!f) return NULL;
	fseek(f, 0, SEEK_END);
	long l = ftell(f);
	fseek(f, 0, SEEK_SET);
	char *b = malloc((size_t)l + 1);
	if (b && fread(b, 1, (size_t)l, f) != (size_t)l) { free(b); b = NULL; }
	fclose(f);
	if (b) { b[l] = 0; if (n) *n = (size_t)l; }
	return b;
}
static void manda(int c, const char *estado, const char *tipo, const char *extra, const char *cuerpo, size_t n)
{
	char cab[512];
	int l = snprintf(cab, sizeof cab, "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nCache-Control: no-store\r\nConnection: close\r\n%s\r\n",
		estado, tipo, n, extra ? extra : "");
	if (write(c, cab, (size_t)l) != l) return;
	for (size_t h = 0; h < n;) {
		ssize_t w = write(c, cuerpo + h, n - h);
		if (w <= 0) return;
		h += (size_t)w;
	}
}
/* una petición: la cabecera entera y el cuerpo (Content-Length) */
static void atiende(int c)
{
	size_t cap = 65536, n = 0;
	char *b = malloc(cap + 1);
	if (!b) return;
	char *fin_cab = NULL;
	while (!fin_cab) {
		if (n == cap) { cap *= 2; char *t = realloc(b, cap + 1); if (!t) { free(b); return; } b = t; }
		ssize_t r = read(c, b + n, cap - n);
		if (r <= 0) { free(b); return; }
		n += (size_t)r;
		b[n] = 0;
		fin_cab = strstr(b, "\r\n\r\n");
	}
	size_t cab = (size_t)(fin_cab - b) + 4, largo = 0;
	char *cl = strcasestr(b, "\r\nContent-Length:");
	if (cl && cl < fin_cab) largo = strtoul(cl + 17, NULL, 10);
	while (n < cab + largo) {
		if (n == cap) { cap = cab + largo; char *t = realloc(b, cap + 1); if (!t) { free(b); return; } b = t; }
		ssize_t r = read(c, b + n, cap - n);
		if (r <= 0) break;
		n += (size_t)r;
	}
	b[n] = 0;
	char metodo[8] = "", ruta[512] = "";
	sscanf(b, "%7s %511s", metodo, ruta);
	char *q = strchr(ruta, '?');
	if (q) *q = 0;
	if (!strcmp(metodo, "POST") && !strncmp(ruta, "/resultado/", 11)) {
		pthread_mutex_lock(&mutex);
		int i = busca(ruta + 11);
		if (i >= 0 && !resultados[i]) {
			resultados[i] = strndup(b + cab, n - cab);
			n_hechas++;
		}
		clock_gettime(CLOCK_MONOTONIC, &ultima);
		pthread_mutex_unlock(&mutex);
		manda(c, "200 OK", "text/plain", NULL, "bien", 4);
	} else if (!strncmp(ruta, "/siguiente/", 11)) {
		pthread_mutex_lock(&mutex);
		int i = busca(ruta + 11), j = i + 1;
		while (j < n_pruebas && resultados[j]) j++;
		pthread_mutex_unlock(&mutex);
		char loc[600];
		if (i >= 0 && j < n_pruebas) snprintf(loc, sizeof loc, "Location: /Crypto/%s.html\r\n", nombres[j]);
		else snprintf(loc, sizeof loc, "Location: /fin.html\r\n");
		manda(c, "302 Found", "text/plain", loc, "", 0);
	} else if (!strcmp(metodo, "GET") && (!strcmp(ruta, "/include.js") || !strcmp(ruta, "/fin.html") || !strncmp(ruta, "/Crypto/", 8)) && !strstr(ruta, "..")) {
		char archivo[700];
		if (!strcmp(ruta, "/fin.html")) {
			const char *t = "<!doctype html><p>Hispa-64: las pruebas de la WebCrypto, acabadas.</p>";
			manda(c, "200 OK", "text/html; charset=utf-8", NULL, t, strlen(t));
		} else {
			snprintf(archivo, sizeof archivo, RAIZ "%s", ruta);
			size_t l = 0;
			char *d = lee_archivo(archivo, &l);
			if (!d) manda(c, "404 Not Found", "text/plain", NULL, "no", 2);
			else {
				int js = strstr(ruta, ".js") != NULL;
				manda(c, "200 OK", js ? "text/javascript; charset=utf-8" : "text/html; charset=utf-8", NULL, d, l);
				free(d);
				if (!strncmp(ruta, "/Crypto/", 8)) {
					char n2[512];
					snprintf(n2, sizeof n2, "%s", ruta + 8);
					char *p = strstr(n2, ".html");
					if (p) *p = 0;
					pthread_mutex_lock(&mutex);
					actual = busca(n2);
					clock_gettime(CLOCK_MONOTONIC, &ultima);
					pthread_mutex_unlock(&mutex);
				}
			}
		}
	} else manda(c, "404 Not Found", "text/plain", NULL, "no", 2);
	free(b);
}
static int escucha = -1;
static void *servidor(void *x)
{
	(void)x;
	for (;;) {
		int c = accept(escucha, NULL, NULL);
		if (c < 0) { if (errno == EINTR) continue; return NULL; }
		atiende(c);
		close(c);
	}
}

static pid_t arranca_hub(const char *hub, int i)
{
	char url[400];
	snprintf(url, sizeof url, "http://127.0.0.1:%d/Crypto/%s.html", PUERTO, nombres[i]);
	char *av[] = { (char *)hub, "--headless=manual", "--disable-sandbox", url, NULL };
	posix_spawn_file_actions_t fa;
	posix_spawn_file_actions_init(&fa);
	posix_spawn_file_actions_addopen(&fa, 1, "/tmp/web/cripto-hub.txt", O_WRONLY | O_CREAT | O_APPEND, 0644);
	posix_spawn_file_actions_addopen(&fa, 2, "/tmp/web/cripto-hub.txt", O_WRONLY | O_CREAT | O_APPEND, 0644);
	pid_t pid = -1;
	if (posix_spawn(&pid, hub, &fa, NULL, av, environ)) pid = -1;
	posix_spawn_file_actions_destroy(&fa);
	return pid;
}
static void cierra_hub(pid_t pid)
{
	if (pid <= 0) return;
	kill(pid, SIGTERM);
	for (int i = 0; i < 250; i++) {
		if (waitpid(pid, NULL, WNOHANG) == pid) return;
		struct timespec d = { 0, 20000000 };
		nanosleep(&d, NULL);
	}
	kill(pid, SIGKILL);
	waitpid(pid, NULL, 0);
}
static double segundos_desde(const struct timespec *t)
{
	struct timespec a;
	clock_gettime(CLOCK_MONOTONIC, &a);
	return (double)(a.tv_sec - t->tv_sec) + (double)(a.tv_nsec - t->tv_nsec) / 1e9;
}
/* sin los blancos del final de cada línea ni las líneas vacías del final */
static void normaliza(char *t)
{
	char *w = t, *r = t, *fin_linea = t;
	while (*r) {
		*w = *r;
		if (*r == '\r') { r++; continue; }
		if (*r == '\n') {
			while (w > fin_linea && (w[-1] == ' ' || w[-1] == '\t')) w--;
			*w++ = '\n';
			fin_linea = w;
			r++;
			continue;
		}
		w++;
		r++;
	}
	*w = 0;
	while (w > t && (w[-1] == '\n' || w[-1] == ' ' || w[-1] == '\t')) *--w = 0;
}
static void primera_diferencia(const char *a, const char *b, char *sal, size_t n)
{
	int l = 1;
	while (*a && *b) {
		const char *fa = strchr(a, '\n'), *fb = strchr(b, '\n');
		size_t la = fa ? (size_t)(fa - a) : strlen(a), lb = fb ? (size_t)(fb - b) : strlen(b);
		if (la != lb || memcmp(a, b, la)) {
			snprintf(sal, n, "línea %d: «%.*s» en Linux, «%.*s» aquí", l, (int)(la > 120 ? 120 : la), a, (int)(lb > 120 ? 120 : lb), b);
			return;
		}
		a += la + (fa ? 1 : 0);
		b += lb + (fb ? 1 : 0);
		l++;
	}
	snprintf(sal, n, "línea %d: %s", l, *a ? "aquí acaba antes" : "aquí hay más");
}

int mweb_cripto(const char *hub)
{
	printf("MWEB: la WebCrypto de Ladybird (sus pruebas de Tests/LibWeb/Text/input/Crypto, en el navegador sin ventana)\n");
	DIR *d = opendir(RAIZ "/Crypto");
	struct dirent *e;
	while (d && (e = readdir(d)) && n_pruebas < MAX_PRUEBAS) {
		size_t l = strlen(e->d_name);
		if (l > 5 && !strcmp(e->d_name + l - 5, ".html")) {
			char *n = strndup(e->d_name, l - 5);
			if (saltada(n)) { printf("  cripto saltada: %s (también en Ladybird: TestConfig.ini)\n", n); free(n); continue; }
			nombres[n_pruebas++] = n;
		}
	}
	if (d) closedir(d);
	if (!n_pruebas) { printf("  no están las pruebas en " RAIZ "/Crypto\nMWEB: 1 mal de 1\n"); return 1; }
	qsort(nombres, (size_t)n_pruebas, sizeof *nombres, compara);
	escucha = socket(AF_INET, SOCK_STREAM, 0);
	int uno = 1;
	setsockopt(escucha, SOL_SOCKET, SO_REUSEADDR, &uno, sizeof uno);
	struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons(PUERTO) };
	a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (escucha < 0 || bind(escucha, (struct sockaddr *)&a, sizeof a) || listen(escucha, 16)) {
		printf("  no se puede escuchar en 127.0.0.1:%d (%s)\nMWEB: 1 mal de 1\n", PUERTO, strerror(errno));
		return 1;
	}
	pthread_t h;
	pthread_create(&h, NULL, servidor, NULL);
	struct timespec t0;
	clock_gettime(CLOCK_MONOTONIC, &t0);
	int siguiente = 0, arranques = 0;
	char sin_acabar[MAX_PRUEBAS] = { 0 };
	while (siguiente < n_pruebas) {
		clock_gettime(CLOCK_MONOTONIC, &ultima);
		pid_t pid = arranca_hub(hub, siguiente);
		arranques++;
		if (pid <= 0) { printf("  el Hub no se puede poner en marcha\n"); break; }
		int muerto = 0;
		for (;;) {
			struct timespec dd = { 0, 100000000 };
			nanosleep(&dd, NULL);
			pthread_mutex_lock(&mutex);
			int hechas = n_hechas, act = actual;
			double quieto = segundos_desde(&ultima);
			pthread_mutex_unlock(&mutex);
			if (hechas + 0 >= n_pruebas) break;
			if (waitpid(pid, NULL, WNOHANG) == pid) { muerto = 1; break; }
			if (quieto > 60) {
				/* la de ahora no acaba: fuera, y la siguiente */
				int i = act >= 0 ? act : siguiente;
				sin_acabar[i] = 1;
				printf("  (%s: sin acabar en 60 s; el Hub, otra vez)\n", nombres[i]);
				break;
			}
		}
		if (!muerto) cierra_hub(pid);
		/* la siguiente sin resultado ni marcada */
		pthread_mutex_lock(&mutex);
		siguiente = 0;
		while (siguiente < n_pruebas && (resultados[siguiente] || sin_acabar[siguiente])) siguiente++;
		pthread_mutex_unlock(&mutex);
		if (muerto && siguiente < n_pruebas) {
			printf("  (el Hub se ha ido en %s; otra vez)\n", nombres[siguiente]);
			sin_acabar[siguiente] = 1;
			siguiente++;
		}
		if (arranques > n_pruebas) break;
	}
	int iguales = 0;
	for (int i = 0; i < n_pruebas; i++) {
		char ruta[600], dif[400] = "";
		snprintf(ruta, sizeof ruta, RAIZ "/expected/%s.txt", nombres[i]);
		char *esp = lee_archivo(ruta, NULL);
		if (!resultados[i] || !esp) {
			printf("  cripto distinta: %s (%s)\n", nombres[i], !esp ? "sin lo de Linux" : "sin acabar");
			free(esp);
			continue;
		}
		normaliza(esp);
		normaliza(resultados[i]);
		if (!strcmp(esp, resultados[i])) { iguales++; printf("  cripto igual: %s\n", nombres[i]); }
		else {
			primera_diferencia(esp, resultados[i], dif, sizeof dif);
			printf("  cripto distinta: %s (%s)\n", nombres[i], dif);
		}
		free(esp);
	}
	printf("  en %.0f s, con %d arranques del Hub\n", segundos_desde(&t0), arranques);
	printf("MWEB: la WebCrypto de Ladybird: %d de %d iguales que en Linux\n", iguales, n_pruebas);
	return 0;
}
