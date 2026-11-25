/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/musl/msonido.c - Hispa-64, etapa 11 (docs/64/
 * HISPA-64.md, sección 17): el sonido por /dev/dsp, como OSS: un segundo
 * de un la (440 Hz) a 48 kHz en estéreo, un silencio y medio segundo de
 * 1000 Hz a 44,1 kHz en mono. Lo que suena lo graba QEMU (-audiodev wav) y
 * las pruebas lo miran (nucleo32/sonido/oye.py): dos ráfagas, de su
 * frecuencia y su largo, sin cortes. Y lo demás de OSS: SETFMT, SPEED,
 * CHANNELS, GETOSPACE, GETODELAY, SYNC, y que dos a la vez no (EBUSY). Sin
 * tarjeta, /dev/dsp da ENXIO. Estático. */
#include "comun.h"
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>
#include <sys/ioctl.h>
/* (lo de <linux/soundcard.h> que hace falta: musl no trae las cabeceras de Linux) */
#define AFMT_S16_LE 0x10
#define SNDCTL_DSP_SYNC 0x5001
#define SNDCTL_DSP_SPEED 0xC0045002
#define SNDCTL_DSP_SETFMT 0xC0045005
#define SNDCTL_DSP_CHANNELS 0xC0045006
#define SNDCTL_DSP_GETOSPACE 0x8010500C
#define SNDCTL_DSP_GETODELAY 0x80045017
typedef struct { int fragments, fragstotal, fragsize, bytes; } audio_buf_info;

static short b[2 * 4800];

/* «ms» milisegundos de un tono de «hz» a «fs» muestras por segundo, con «can» canales */
static int toca(int fd, double hz, int fs, int can, int ms)
{
	long n = (long)fs * ms / 1000, i = 0, k, j;
	double fase = 0, paso = 2 * M_PI * hz / fs;

	while (i < n) {
		k = n - i > 4800 ? 4800 : n - i;
		for (j = 0; j < k; j++) {
			short v = (short)(9000 * sin(fase));

			fase += paso;
			if (can == 2) {
				b[2 * j] = v;
				b[2 * j + 1] = v;
			} else
				b[j] = v;
		}
		if (write(fd, b, k * can * 2) != k * can * 2)
			return 0;
		i += k;
	}
	return 1;
}

static long ms_ahora(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

int main(void)
{
	int fd = open("/dev/dsp", O_WRONLY), x, otro, d;
	long t0, t1;
	audio_buf_info bi;

	printf("MSONIDO: el sonido por /dev/dsp (OSS); %s\n", cómo());
	if (fd < 0) {
		MIRA(errno == ENXIO, "sin tarjeta de sonido, /dev/dsp da ENXIO");
		printf("  /dev/dsp: sin tarjeta de sonido (ENXIO)\n");
		return acaba("MSONIDO");
	}
	otro = open("/dev/dsp", O_WRONLY);
	MIRA(otro < 0 && errno == EBUSY, "otro a la vez: EBUSY");
	x = AFMT_S16_LE;
	MIRA(ioctl(fd, SNDCTL_DSP_SETFMT, &x) == 0 && x == AFMT_S16_LE, "SNDCTL_DSP_SETFMT: 16 bits");
	x = 2;
	MIRA(ioctl(fd, SNDCTL_DSP_CHANNELS, &x) == 0 && x == 2, "SNDCTL_DSP_CHANNELS: 2");
	x = 48000;
	MIRA(ioctl(fd, SNDCTL_DSP_SPEED, &x) == 0 && x == 48000, "SNDCTL_DSP_SPEED: 48000");
	MIRA(ioctl(fd, SNDCTL_DSP_GETOSPACE, &bi) == 0 && bi.fragsize == 4096 && bi.bytes > 32768, "SNDCTL_DSP_GETOSPACE: sitio en el anillo");
	t0 = ms_ahora();
	MIRA(toca(fd, 440, 48000, 2, 1000), "un segundo de 440 Hz, estéreo, a 48 kHz");
	d = -1;
	MIRA(ioctl(fd, SNDCTL_DSP_GETODELAY, &d) == 0 && d > 0 && d < 65536, "SNDCTL_DSP_GETODELAY: lo que queda por sonar");
	MIRA(ioctl(fd, SNDCTL_DSP_SYNC, 0) == 0, "SNDCTL_DSP_SYNC");
	t1 = ms_ahora();
	MIRA(t1 - t0 >= 900, "SYNC: vuelve cuando ha sonado (un segundo, más o menos)");
	usleep(400000);
	x = 1;
	MIRA(ioctl(fd, SNDCTL_DSP_CHANNELS, &x) == 0 && x == 1, "SNDCTL_DSP_CHANNELS: 1");
	x = 44100;
	MIRA(ioctl(fd, SNDCTL_DSP_SPEED, &x) == 0 && x == 44100, "SNDCTL_DSP_SPEED: 44100");
	x = 22050;
	MIRA(ioctl(fd, SNDCTL_DSP_SPEED, &x) == 0 && x == 44100, "SNDCTL_DSP_SPEED: la que tiene, la más cercana");
	MIRA(toca(fd, 1000, 44100, 1, 500), "medio segundo de 1000 Hz, mono, a 44,1 kHz");
	MIRA(close(fd) == 0, "close: lo que queda, suena");
	fd = open("/dev/dsp", O_WRONLY);
	MIRA(fd >= 0, "cerrada, se puede abrir otra vez");
	if (fd >= 0)
		close(fd);
	printf("  /dev/dsp: 1 s de 440 Hz (48 kHz, estéreo), silencio y 0,5 s de 1000 Hz (44,1 kHz, mono); SYNC a los %ld ms\n", t1 - t0);
	return acaba("MSONIDO");
}
