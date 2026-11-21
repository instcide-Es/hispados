/* anfitrion.c - el motor de ESCUDO.BIN en Linux («escudo-anfitrion»): para las
 * pruebas (castel/pruebas.sh) y para hacer FIRMAS.ESC en el VPS
 * (tools/escudo_firmas.py). Es el mismo motor que va en Hispa-Pentium.
 *
 *   escudo-anfitrion [-f FIRMAS]... [-m MB] [-z] [-t] RUTA...
 *       revisa los archivos (y carpetas) con esas firmas (.hdb .hsb .mdb .msb
 *       .ndb .ldb .fp .sfp .ign2, .cvd/.cld o FIRMAS.ESC): una línea por
 *       archivo, «RUTA: NOMBRE ENCONTRADO» o «RUTA: limpio»; -t dice lo que
 *       tarda (MB/s); -z no mira dentro de los ZIP
 *   escudo-anfitrion -f ... -o FIRMAS.ESC [-F] [-d AAAAMMDD] [-l MB]
 *       hace FIRMAS.ESC con lo cargado (-F: los nombres de las sumas, por familia;
 *       -l: que no pase de tantos MB, dejando primero las sumas de main;
 *       -O main|dia: solo lo que viene de main.cvd, o solo lo demás)
 *   escudo-anfitrion -c ARCHIVO.cvd
 *       solo comprueba la firma digital de un .cvd y dice su cabecera
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <dirent.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "escudo.h"

extern int motor_guarda_lineas;

void *pl_reserva(u32 n) { return malloc(n ? n : 1); }
void *pl_rereserva(void *p, u32 n) { return realloc(p, n ? n : 1); }
void pl_libera(void *p) { free(p); }
int pl_abrir(const char *r) { return open(r, O_RDONLY); }
int pl_crear(const char *r) { return open(r, O_WRONLY | O_CREAT | O_TRUNC, 0644); }
int pl_leer(int h, void *d, u32 n)
{
	u32 k = 0;
	while (k < n) {
		ssize_t r = read(h, (char *)d + k, n - k);
		if (r < 0)
			return k ? (int)k : -1;
		if (r == 0)
			break;
		k += (u32)r;
	}
	return (int)k;
}
int pl_escribir(int h, const void *d, u32 n) { return (int)write(h, d, n); }
int pl_mover(int h, u32 p) { return lseek(h, p, SEEK_SET) < 0 ? -1 : 0; }
u32 pl_largo(int h)
{
	struct stat s;
	return fstat(h, &s) ? 0 : (u32)s.st_size;
}
void pl_cerrar(int h) { close(h); }
u32 pl_ms(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (u32)(t.tv_sec * 1000 + t.tv_nsec / 1000000);
}
int pl_cortar(void) { return 0; }
int formatear(char *d, int cabe, const char *f, ...)
{
	va_list va;
	int r;
	va_start(va, f);
	r = vsnprintf(d, (size_t)cabe, f, va);
	va_end(va);
	return r < cabe ? r : cabe - 1;
}
static int callado;
void pl_apunta(const char *f, ...)
{
	va_list va;
	if (callado)
		return;
	va_start(va, f);
	vfprintf(stderr, f, va);
	va_end(va);
	fputc('\n', stderr);
}

static int encontrados, revisados;
static double octetos;

static void revisa_ruta(const char *r)
{
	struct stat s;
	resultado res;
	if (stat(r, &s))
		return;
	if (S_ISDIR(s.st_mode)) {
		DIR *d = opendir(r);
		struct dirent *e;
		if (!d)
			return;
		while ((e = readdir(d))) {
			char n[4096];
			if (e->d_name[0] == '.' && (!e->d_name[1] || (e->d_name[1] == '.' && !e->d_name[2])))
				continue;
			snprintf(n, sizeof n, "%s/%s", r, e->d_name);
			revisa_ruta(n);
		}
		closedir(d);
		return;
	}
	if (motor_revisa_archivo(r, r, &res) < 0) {
		printf("%s: no se puede abrir\n", r);
		return;
	}
	revisados++;
	octetos += res.octetos;
	if (res.hallado) {
		encontrados++;
		printf("%s%s%s: %s ENCONTRADO\n", r, res.dentro[0] ? " > " : "", res.dentro, res.nombre);
	} else
		printf("%s: limpio%s%s\n", r, res.cifrados ? " (con miembros cifrados sin mirar)" : "",
		       res.sin_mirar ? " (con partes sin mirar)" : "");
}

int main(int argc, char **argv)
{
	info_firmas inf;
	const char *salida = 0;
	int familias = 0, tiempo = 0, i, n_rutas = 0;
	u32 fecha = 0, t0, limite = 0;
	int origen = 0;
	const char *rutas[4096];
	memset(&inf, 0, sizeof inf);
	motor_inicia();
	for (i = 1; i < argc; i++)	/* (con -o, las líneas del cuerpo se guardan desde la primera) */
		if (!strcmp(argv[i], "-o"))
			motor_guarda_lineas = 1;
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-f") && i + 1 < argc) {
			int r;
			const char *f = argv[++i];
			r = motor_carga(f, &inf);
			if (r < 0)
				fprintf(stderr, "escudo: %s\n", inf.error);
		} else if (!strcmp(argv[i], "-o") && i + 1 < argc) {
			salida = argv[++i];
			motor_guarda_lineas = 1;
		} else if (!strcmp(argv[i], "-F"))
			familias = 1;
		else if (!strcmp(argv[i], "-l") && i + 1 < argc)
			limite = (u32)atoi(argv[++i]) << 20;
		else if (!strcmp(argv[i], "-O") && i + 1 < argc) {
			i++;
			origen = !strcmp(argv[i], "main") ? 1 : !strcmp(argv[i], "dia") ? 2 : 0;
		} else if (!strcmp(argv[i], "-q"))
			callado = 1;
		else if (!strcmp(argv[i], "-d") && i + 1 < argc)
			fecha = (u32)atoi(argv[++i]);
		else if (!strcmp(argv[i], "-m") && i + 1 < argc)
			motor_max_archivo = (u32)atoi(argv[++i]) << 20;
		else if (!strcmp(argv[i], "-z"))
			motor_zip = 0;
		else if (!strcmp(argv[i], "-t"))
			tiempo = 1;
		else if (!strcmp(argv[i], "-c") && i + 1 < argc) {
			int h = pl_abrir(argv[++i]), n;
			u8 c[512], b[65536], m5[16];
			cabecera_cvd cab;
			md5_ctx m;
			if (h < 0 || pl_leer(h, c, 512) != 512 || cvd_cabecera(c, &cab)) {
				printf("%s: no es un .cvd\n", argv[i]);
				return 2;
			}
			md5_inicia(&m);
			while ((n = pl_leer(h, b, sizeof b)) > 0)
				md5_suma(&m, b, (u32)n);
			md5_acaba(&m, m5);
			printf("%s: versión %u, %u firmas, nivel %u, de %s, hecho el %u: firma digital %s\n", argv[i],
			       cab.version, cab.firmas, cab.flevel, cab.autor, fecha_de_segundos(cab.segundos),
			       cvd_firma_buena(&cab, m5) ? "BUENA" : "MALA");
			return cvd_firma_buena(&cab, m5) ? 0 : 1;
		} else if (n_rutas < 4096)
			rutas[n_rutas++] = argv[i];
	}
	/* motor_prepara también ordena las sumas para hacer FIRMAS.ESC */
	motor_prepara(&inf);
	if (salida) {
		if (!fecha)
			fecha = inf.fecha;
		if (motor_escribe_esc(salida, familias, fecha, limite, origen)) {
			fprintf(stderr, "escudo: no se puede escribir %s\n", salida);
			return 2;
		}
	}
	t0 = pl_ms();
	for (i = 0; i < n_rutas; i++)
		revisa_ruta(rutas[i]);
	if (tiempo) {
		u32 ms = pl_ms() - t0;
		fprintf(stderr, "escudo: %d archivos, %.1f MB en %u ms: %.1f MB/s\n", revisados, octetos / 1048576.0, ms,
			ms ? octetos / 1048576.0 / (ms / 1000.0) : 0.0);
	}
	return encontrados ? 1 : 0;
}
