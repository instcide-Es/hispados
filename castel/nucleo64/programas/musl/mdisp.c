/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/musl/mdisp.c - Hispa-64, etapa 8: los dispositivos
 * para los programas (docs/64/HISPA-64.md, sección 14): /dev/null y
 * /dev/zero; la memoria compartida con nombre (shm_open de musl, entre
 * procesos que no son padre e hijo: el otro es este mismo programa,
 * arrancado otra vez con execve); la pantalla (/dev/fb0: los ioctl de
 * fbdev, mmap y el doble búfer, si hay pantalla); y los sucesos
 * (/dev/input/event0: sus ioctl, sin esperar y poll).
 *   MDISP                 las pruebas (las de cada arranque)
 *   MDISP hijo NOMBRE     (lo arranca la prueba de la memoria compartida)
 *   MDISP sucesos N       agarra el teclado y el puntero y dice los N
 *                         primeros sucesos (las pruebas con el monitor de QEMU)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include "comun.h"
#include "../ventanas/hdisp.h"

#define TAM_SHM (256 * 1024)

static int hijo(const char *nombre)
{
	int fd = shm_open(nombre, O_RDWR, 0);
	unsigned *p;
	unsigned i, bien = 1;
	struct stat st;

	if (fd < 0) {
		printf("  MAL: el otro proceso no abre %s (errno %d)\n", nombre, errno);
		return 2;
	}
	if (fstat(fd, &st) || st.st_size != TAM_SHM)
		bien = 0;
	p = mmap(0, TAM_SHM, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	if (p == MAP_FAILED)
		return 3;
	for (i = 0; i < TAM_SHM / 4; i++)
		if (p[i] != i * 2654435761u)
			bien = 0;
	/* lo suyo, para que lo vea el primero */
	for (i = 0; i < TAM_SHM / 4; i++)
		p[i] = ~i;
	munmap(p, TAM_SHM);
	return bien ? 0 : 4;
}

static void pruebas_shm(char *yo)
{
	char nombre[64];
	unsigned *p, i, bien;
	int fd, st, r;
	pid_t h;

	snprintf(nombre, sizeof nombre, "/mdisp-%d", (int)getpid());
	fd = shm_open(nombre, O_CREAT | O_EXCL | O_RDWR, 0600);
	MIRA(fd >= 0, "shm_open con O_CREAT");
	if (fd < 0)
		return;
	MIRA(shm_open(nombre, O_CREAT | O_EXCL | O_RDWR, 0600) < 0 && errno == EEXIST, "shm_open con O_EXCL de uno que ya está: EEXIST");
	MIRA(ftruncate(fd, TAM_SHM) == 0, "ftruncate de la memoria compartida");
	p = mmap(0, TAM_SHM, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	MIRA(p != MAP_FAILED, "mmap de la memoria compartida");
	if (p == MAP_FAILED)
		return;
	for (i = 0; i < TAM_SHM / 4; i++)
		p[i] = i * 2654435761u;
	h = fork();
	if (h == 0) {
		char *argv[] = { yo, "hijo", nombre, 0 };
		execve(yo, argv, 0);
		_exit(9);
	}
	r = waitpid(h, &st, 0);
	MIRA(r == h && WIFEXITED(st) && WEXITSTATUS(st) == 0, "otro programa (execve) la abre por su nombre, la lee y escribe");
	bien = 1;
	for (i = 0; i < TAM_SHM / 4; i++)
		if (p[i] != ~i)
			bien = 0;
	MIRA(bien, "lo que ha escrito el otro programa se ve aquí");
	MIRA(shm_unlink(nombre) == 0, "shm_unlink");
	MIRA(shm_open(nombre, O_RDWR, 0) < 0 && errno == ENOENT, "ya sin nombre: ENOENT");
	/* (sin nombre, la de antes sigue: el descriptor y lo puesto) */
	p[5] = 1234;
	MIRA(ftruncate(fd, 8192) == 0, "ftruncate más corto");
	close(fd);
	MIRA(munmap(p, TAM_SHM) == 0, "munmap (las páginas, sueltas con el último que las tenía)");
	printf("  /dev/shm: %d KB compartidos con otro programa (execve), por su nombre\n", TAM_SHM / 1024);
}

static void pruebas_nulo(void)
{
	char b[300];
	int fd = open("/dev/null", O_RDWR), i, ceros = 1;

	MIRA(fd >= 0 && write(fd, "hola", 4) == 4 && read(fd, b, sizeof b) == 0, "/dev/null");
	close(fd);
	fd = open("/dev/zero", O_RDONLY);
	memset(b, 7, sizeof b);
	MIRA(fd >= 0 && read(fd, b, sizeof b) == sizeof b, "/dev/zero se lee");
	for (i = 0; i < (int)sizeof b; i++)
		if (b[i])
			ceros = 0;
	MIRA(ceros, "/dev/zero: ceros");
	close(fd);
}

static void pruebas_fb(void)
{
	struct fb_var_screeninfo v;
	struct fb_fix_screeninfo f;
	uint8_t *m;
	size_t largo;
	unsigned x, y, bien, r[4];
	int fd = open("/dev/fb0", O_RDWR), st;
	pid_t h;

	if (fd < 0) {
		MIRA(errno == ENXIO, "sin pantalla, /dev/fb0 da ENXIO");
		printf("  /dev/fb0: sin pantalla gráfica (ENXIO)\n");
		return;
	}
	MIRA(ioctl(fd, FBIOGET_VSCREENINFO, &v) == 0, "FBIOGET_VSCREENINFO");
	MIRA(ioctl(fd, FBIOGET_FSCREENINFO, &f) == 0, "FBIOGET_FSCREENINFO");
	MIRA(v.bits_per_pixel == 32 || v.bits_per_pixel == 24, "32 o 24 bits por punto");
	MIRA(f.line_length >= v.xres * (v.bits_per_pixel / 8) && f.visual == FB_VISUAL_TRUECOLOR, "line_length y FB_VISUAL_TRUECOLOR");
	MIRA(v.yres_virtual == v.yres * 2 || v.yres_virtual == v.yres, "yres_virtual");
	largo = f.smem_len;
	m = mmap(0, largo, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	MIRA(m != MAP_FAILED, "mmap de la pantalla y del búfer de atrás");
	if (m == MAP_FAILED) {
		close(fd);
		return;
	}
	/* (otro proceso no puede tenerla a la vez) */
	h = fork();
	if (h == 0) {
		int g = open("/dev/fb0", O_RDWR);
		void *q = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, g, 0);
		_exit(q == MAP_FAILED && errno == EBUSY ? 0 : 1);
	}
	MIRA(waitpid(h, &st, 0) == h && WIFEXITED(st) && WEXITSTATUS(st) == 0, "otro proceso no la puede poner a la vez: EBUSY");
	/* en el búfer de atrás, un dibujo; copiado un rectángulo, en la pantalla */
	{
		uint8_t *atras = m + largo / 2;
		unsigned bpp = v.bits_per_pixel / 8;

		for (y = 0; y < 64; y++)
			for (x = 0; x < 128; x++) {
				uint32_t c = ((x * 2) << v.red.offset) | ((y * 4) << v.green.offset) | (0x80u << v.blue.offset);
				memcpy(atras + (size_t)(y + 10) * f.line_length + (size_t)(x + 20) * bpp, &c, bpp);
			}
		r[0] = 20; r[1] = 10; r[2] = 128; r[3] = 64;
		MIRA(ioctl(fd, FBIO_HISPA_COPIA, r) == 0, "FBIO_HISPA_COPIA (un rectángulo, de atrás a la pantalla)");
		bien = 1;
		for (y = 0; y < 64; y += 7)
			for (x = 0; x < 128; x += 5)
				if (memcmp(m + (size_t)(y + 10) * f.line_length + (size_t)(x + 20) * bpp,
					   atras + (size_t)(y + 10) * f.line_length + (size_t)(x + 20) * bpp, bpp))
					bien = 0;
		MIRA(bien, "lo copiado está en la pantalla");
		if (v.yres_virtual == v.yres * 2) {
			v.yoffset = v.yres;
			MIRA(ioctl(fd, FBIOPAN_DISPLAY, &v) == 0, "FBIOPAN_DISPLAY (todo el búfer de atrás a la pantalla)");
		}
	}
	printf("  /dev/fb0: %ux%u, %u bits, %u octetos por línea, %u KB (con el búfer de atrás)\n",
	       v.xres, v.yres, v.bits_per_pixel, f.line_length, f.smem_len / 1024);
	munmap(m, largo);
	close(fd);
}

static void pruebas_sucesos(void)
{
	char nombre[80] = "";
	int ver = 0, fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);
	struct input_event e;
	struct pollfd pf;

	MIRA(fd >= 0, "abrir /dev/input/event0");
	if (fd < 0)
		return;
	MIRA(ioctl(fd, EVIOCGNAME(sizeof nombre), nombre) > 0 && strstr(nombre, "Hispa-64"), "EVIOCGNAME");
	MIRA(ioctl(fd, EVIOCGVERSION, &ver) == 0 && ver == 0x010001, "EVIOCGVERSION");
	MIRA(read(fd, &e, sizeof e) < 0 && errno == EAGAIN, "sin sucesos y O_NONBLOCK: EAGAIN");
	pf.fd = fd;
	pf.events = POLLIN;
	MIRA(poll(&pf, 1, 30) == 0, "poll sin sucesos: el plazo");
	MIRA(ioctl(fd, EVIOCGRAB, 1) == 0 && ioctl(fd, EVIOCGRAB, 0) == 0, "EVIOCGRAB");
	printf("  /dev/input/event0: «%s»\n", nombre);
	close(fd);
}

/* MDISP sucesos N: los N primeros (para las pruebas con las teclas y el ratón del monitor) */
static int sucesos(int n)
{
	int fd = open("/dev/input/event0", O_RDONLY), k = 0;
	struct input_event e;

	if (fd < 0 || ioctl(fd, EVIOCGRAB, 1)) {
		printf("MDISP: no se abre /dev/input/event0\n");
		return 1;
	}
	printf("MDISP: esperando %d sucesos\n", n);
	fflush(stdout);
	while (k < n && read(fd, &e, sizeof e) == sizeof e) {
		if (e.type == EV_SYN)
			continue;
		if (e.type == EV_LETRA)
			printf("suceso: letra %d\n", e.value);
		else if (e.type == EV_KEY)
			printf("suceso: tecla %d %d\n", e.code, e.value);
		else if (e.type == EV_ABS)
			printf("suceso: posición %c %d\n", e.code == ABS_X ? 'x' : 'y', e.value);
		else
			printf("suceso: %d %d %d\n", e.type, e.code, e.value);
		fflush(stdout);
		k++;
	}
	close(fd);
	printf("MDISP: %d sucesos\n", k);
	return 0;
}

int main(int argc, char **argv)
{
	if (argc >= 3 && !strcmp(argv[1], "hijo"))
		return hijo(argv[2]);
	if (argc >= 3 && !strcmp(argv[1], "sucesos"))
		return sucesos(atoi(argv[2]));
	printf("MDISP: los dispositivos (/dev/shm, /dev/fb0, /dev/input/event0, /dev/null, /dev/zero); %s\n", cómo());
	pruebas_nulo();
	pruebas_shm(argv[0][0] == '/' ? argv[0] : "/MDISP");
	pruebas_fb();
	pruebas_sucesos();
	return acaba("MDISP");
}
