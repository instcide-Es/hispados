/* menergia.c - Hispa-64, etapa 11 a (docs/64/HISPA-64.md, sección 27):
 * /dev/energia, lo que lee el escritorio (la batería, el cable, la tapa, el
 * brillo, si se puede suspender) y las órdenes que se le escriben («brillo=NN»).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int pruebas, malas;

static void mira(int bien, const char *que)
{
	pruebas++;
	if (!bien) {
		malas++;
		printf("  MAL: %s (%s)\n", que, strerror(errno));
	}
}

static int lee(char *t, int cabe)
{
	int fd = open("/dev/energia", O_RDONLY), n;

	if (fd < 0)
		return -1;
	n = read(fd, t, cabe - 1);
	if (n >= 0)
		t[n] = 0;
	/* (una lectura por descriptor: la segunda, fin del archivo) */
	mira(read(fd, t + (n > 0 ? n + 1 : 0), 1) == 0 || n < 0, "la segunda lectura, fin del archivo");
	close(fd);
	if (n > 0 && t[n - 1] == '\n')
		t[n - 1] = 0;
	return n;
}

static int valor(const char *t, const char *clave)
{
	char k[32];
	const char *p;

	snprintf(k, sizeof k, "%s=", clave);
	for (p = t; (p = strstr(p, k)); p++)
		if (p == t || p[-1] == ' ')
			return atoi(p + strlen(k));
	return -99;
}

static int escribe(const char *orden)
{
	int fd = open("/dev/energia", O_WRONLY), r;

	if (fd < 0)
		return -1;
	r = write(fd, orden, strlen(orden));
	close(fd);
	return r;
}

int main(void)
{
	char t[300];

	mira(lee(t, sizeof t) > 0, "leer /dev/energia");
	printf("MENERGIA: /dev/energia: %s\n", t);
	mira(valor(t, "baterias") >= 0 && valor(t, "bateria") >= -1 && valor(t, "cable") >= -1 && valor(t, "aviso") >= 0 &&
	     valor(t, "tapa") >= 0 && valor(t, "brillo") >= -1 && valor(t, "suspender") >= 0, "las claves");
	mira(escribe("volar\n") < 0 && errno == EINVAL, "una orden que no es, EINVAL");
	if (valor(t, "brillo") >= 0) {
		int antes = valor(t, "brillo");

		mira(escribe("brillo=30\n") > 0, "escribir brillo=30");
		lee(t, sizeof t);
		printf("MENERGIA: brillo=30 escrito; leído: %d\n", valor(t, "brillo"));
		mira(valor(t, "brillo") == 30, "el brillo, al 30 %");
		mira(escribe("brillo=101") < 0 && errno == EINVAL, "brillo=101, EINVAL");
		snprintf(t, sizeof t, "brillo=%d", antes);
		escribe(t);
	} else
		mira(escribe("brillo=30") < 0, "sin brillo, brillo=30 falla");
	if (malas)
		printf("MENERGIA: %d mal de %d pruebas\n", malas, pruebas);
	else
		printf("MENERGIA: todo bien (%d pruebas)\n", pruebas);
	return malas != 0;
}
