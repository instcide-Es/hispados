/* mcasa.c - Hispa-64, etapa 11 a (docs/64/HISPA-64.md, sección 27): la casa
 * en el disco (/casa, con el diario de Discos64).
 *   MCASA              en $HOME (la casa): crear, escribir, fsync, leer,
 *                      renombrar, statfs, borrar; lo deja como estaba
 *   MCASA escribe      escribe archivos en $HOME/corte sin parar (la prueba
 *                      del corte de luz: QEMU se mata a mitad): A0000.BIN,
 *                      A0001.BIN... con su patrón; cada tres, fsync del
 *                      archivo y de la carpeta, y ULTIMO.TXT con su número
 *                      (lo guardado de verdad); cada cinco, borra el de cinco
 *                      antes
 *   MCASA comprueba    después del corte: lo guardado (hasta ULTIMO.TXT),
 *                      entero; lo de después, entero, a medias (lo de
 *                      delante, con su patrón) o sin estar; lo borrado antes
 *                      de lo guardado, sin estar
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <unistd.h>

static char casa[256];

static unsigned char patron(int i, long j)
{
	return (unsigned char)((i * 31 + j * 7 + (j >> 9)) & 255);
}

static long largo_de(int i)
{
	return (i % 7 + 1) * 37000L;
}

static int mal(const char *que)
{
	printf("MCASA: MAL: %s (%s)\n", que, strerror(errno));
	return 1;
}

static int escribe_archivo(const char *r, int i, int con_fsync)
{
	unsigned char b[8192];
	long n = largo_de(i), j = 0;
	int fd = open(r, O_WRONLY | O_CREAT | O_TRUNC, 0644);

	if (fd < 0)
		return -1;
	while (j < n) {
		long t = n - j > (long)sizeof b ? (long)sizeof b : n - j;
		for (long k = 0; k < t; k++)
			b[k] = patron(i, j + k);
		if (write(fd, b, t) != t) {
			close(fd);
			return -1;
		}
		j += t;
	}
	if (con_fsync && fsync(fd)) {
		close(fd);
		return -1;
	}
	return close(fd);
}

/* 0: entero; 1: a medias (lo de delante bien); 2: mal */
static int mira_archivo(const char *r, int i, long *largo)
{
	unsigned char b[8192];
	long j = 0;
	int fd = open(r, O_RDONLY), x;

	*largo = 0;
	if (fd < 0)
		return 2;
	while ((x = read(fd, b, sizeof b)) > 0) {
		for (int k = 0; k < x; k++)
			if (b[k] != patron(i, j + k)) {
				close(fd);
				return 2;
			}
		j += x;
	}
	close(fd);
	*largo = j;
	if (x < 0 || j > largo_de(i))
		return 2;
	return j == largo_de(i) ? 0 : 1;
}

static int sola(void)
{
	char d[300], r[320], r2[320];
	struct statfs sf;
	struct stat st;
	long largo;

	snprintf(d, sizeof d, "%s/.prueba-casa", casa);
	if (mkdir(d, 0755) && errno != EEXIST)
		return mal("mkdir");
	snprintf(r, sizeof r, "%s/uno.bin", d);
	snprintf(r2, sizeof r2, "%s/Dos con nombre largo, ñandú.bin", d);
	if (escribe_archivo(r, 3, 1))
		return mal("escribir y fsync");
	if (mira_archivo(r, 3, &largo) || largo != largo_de(3))
		return mal("leer lo escrito");
	if (rename(r, r2))
		return mal("rename");
	if (stat(r2, &st) || st.st_size != largo_de(3))
		return mal("stat");
	if (statfs(casa, &sf))
		return mal("statfs");
	if (unlink(r2) || rmdir(d))
		return mal("borrar");
	sync();
	printf("MCASA: la casa (%s), bien: escribir con fsync, leer, renombrar con un nombre largo, borrar; %llu MB libres de %llu\n",
	       casa, (unsigned long long)sf.f_bavail * sf.f_bsize / 1048576, (unsigned long long)sf.f_blocks * sf.f_bsize / 1048576);
	return 0;
}

static int escribe(void)
{
	char d[300], r[320], u[320], t[32];

	snprintf(d, sizeof d, "%s/corte", casa);
	if (mkdir(d, 0755) && errno != EEXIST)
		return mal("mkdir corte");
	snprintf(u, sizeof u, "%s/ULTIMO.TXT", d);
	for (int i = 0; i < 100000; i++) {
		snprintf(r, sizeof r, "%s/A%04d.BIN", d, i);
		if (escribe_archivo(r, i, 0))
			return mal("escribir");
		if (i >= 5 && (i - 5) % 5 == 0) {
			snprintf(r, sizeof r, "%s/A%04d.BIN", d, i - 5);
			if (unlink(r))
				return mal("unlink");
		}
		if (i % 3 == 0) {
			int fd = open(u, O_WRONLY | O_CREAT | O_TRUNC, 0644), dfd;
			int n = snprintf(t, sizeof t, "%d\n", i);

			if (fd < 0 || write(fd, t, n) != n || fsync(fd) || close(fd))
				return mal("ULTIMO.TXT");
			dfd = open(d, O_RDONLY | O_DIRECTORY);
			if (dfd < 0 || fsync(dfd) || close(dfd))
				return mal("fsync de la carpeta");
			printf("MCASA: hecho %d (guardado)\n", i);
		} else
			printf("MCASA: hecho %d\n", i);
		fflush(stdout);
	}
	return 0;
}

static int comprueba(void)
{
	char d[300], r[320], u[320], t[32];
	int g = -1, fd, n, max = -1, enteros = 0, medias = 0, faltan = 0, malos = 0;
	DIR *dd;
	struct dirent *e;
	long largo;

	snprintf(d, sizeof d, "%s/corte", casa);
	snprintf(u, sizeof u, "%s/ULTIMO.TXT", d);
	fd = open(u, O_RDONLY);
	if (fd >= 0) {
		n = read(fd, t, sizeof t - 1);
		close(fd);
		if (n > 0) {
			t[n] = 0;
			g = atoi(t);
		}
	}
	dd = opendir(d);
	if (!dd)
		return mal("opendir corte");
	while ((e = readdir(dd))) {
		int i;
		if (sscanf(e->d_name, "A%d.BIN", &i) == 1 && i > max)
			max = i;
	}
	closedir(dd);
	for (int i = 0; i <= (max > g ? max : g); i++) {
		int borrado = i % 5 == 0 && i + 5 <= g, x;
		snprintf(r, sizeof r, "%s/A%04d.BIN", d, i);
		if (access(r, F_OK)) {
			if (i <= g && !borrado) {
				printf("MCASA: MAL: falta A%04d.BIN, que estaba guardado\n", i);
				malos++;
			} else
				faltan++;
			continue;
		}
		if (borrado && i + 5 <= g) {
			printf("MCASA: MAL: está A%04d.BIN, que se borró antes de guardar\n", i);
			malos++;
			continue;
		}
		x = mira_archivo(r, i, &largo);
		if (x == 2 || (x == 1 && i <= g)) {
			printf("MCASA: MAL: A%04d.BIN, %ld octetos de %ld, %s\n", i, largo, largo_de(i), x == 2 ? "con otra cosa dentro" : "a medias, y estaba guardado");
			malos++;
		} else if (x == 1)
			medias++;
		else
			enteros++;
	}
	printf("MCASA: comprobado después del corte: guardado hasta el %d (el último, el %d): %d enteros, %d a medias (de después de lo guardado), %d sin estar (borrados o de después), %d mal\n",
	       g, max, enteros, medias, faltan, malos);
	return malos ? 1 : 0;
}

int main(int argc, char **argv)
{
	const char *h = getenv("HOME");

	if (!h || strcmp(h, "/casa")) {
		printf("MCASA: sin casa (HOME=%s)\n", h ? h : "");
		return argc > 1 ? 1 : 0;
	}
	snprintf(casa, sizeof casa, "%s", h);
	if (argc > 1 && !strcmp(argv[1], "escribe"))
		return escribe();
	if (argc > 1 && !strcmp(argv[1], "comprueba"))
		return comprueba();
	return sola();
}
