/* anfitrion.c - el Reproductor en el anfitrión: las pruebas de los
 * decodificadores, las etiquetas, las listas y el motor (con ASan), y el
 * fuzz. Uso:
 *   repro decodifica ARCHIVO SALIDA.WAV     a su frecuencia, en 16 bits (para compararlo con ffmpeg)
 *   repro motor SALIDA.WAV ARCHIVO...       por el motor (a REPRO_HZ, 48000), como suena
 *   repro busca ARCHIVO MS SALIDA.WAV       desde ahí (buscar)
 *   repro etiquetas ARCHIVO                 lo que dicen sus etiquetas
 *   repro lista ARCHIVO                     una .M3U, .PLS o .CUE: sus canciones
 *   repro duracion ARCHIVO                  la duración rápida
 *   repro fuzz ARCHIVO                      lo pasa por todo (para el fuzz: que no reviente)
 *   repro dsp                               el ecualizador, el remuestreador y la velocidad con tonos
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../decod.h"
#include "../etiquetas.h"
#include "../lista.h"
#include "../motor.h"
#include "../util.h"

void anfitrion_argumentos(int argc, char **argv);

static void escribe_wav(FILE *f, u32 hz, u64 tramas)
{
	u32 d = (u32)(tramas * 4);
	u8 c[44] = { 'R', 'I', 'F', 'F' };

	c[4] = (u8)(d + 36), c[5] = (u8)((d + 36) >> 8), c[6] = (u8)((d + 36) >> 16), c[7] = (u8)((d + 36) >> 24);
	memcpy(c + 8, "WAVEfmt ", 8);
	c[16] = 16, c[20] = 1, c[22] = 2;
	c[24] = (u8)hz, c[25] = (u8)(hz >> 8), c[26] = (u8)(hz >> 16);
	c[28] = (u8)(hz * 4), c[29] = (u8)(hz * 4 >> 8), c[30] = (u8)(hz * 4 >> 16);
	c[32] = 4, c[34] = 16;
	memcpy(c + 36, "data", 4);
	c[40] = (u8)d, c[41] = (u8)(d >> 8), c[42] = (u8)(d >> 16), c[43] = (u8)(d >> 24);
	fseek(f, 0, SEEK_SET);
	fwrite(c, 1, 44, f);
	fseek(f, 0, SEEK_END);
}

static int abre_listo(dec *d, const char *ruta)
{
	origen o;
	int i;

	origen_de_ruta(&o, ruta);
	if (dec_abre(d, &o)) {
		fprintf(stderr, "no se abre: %s\n", d->error);
		return -1;
	}
	for (i = 0; i < 100000; i++) {
		int r = dec_paso(d);

		if (r > 0)
			return 0;
		if (r < 0) {
			fprintf(stderr, "no se prepara: %s\n", d->error);
			return -1;
		}
	}
	return -1;
}

static int decodifica(const char *ruta, const char *sal, i64 desde_ms)
{
	dec d;
	float b[4096 * 2];
	i16 s[4096 * 2];
	FILE *f;
	u64 n = 0;
	int k, i;

	if (abre_listo(&d, ruta))
		return 1;
	printf("formato=%s hz=%u canales=%d bits=%d total=%llu kbps=%u buscable=%d\n", d.nombre_formato, d.hz, d.canales,
	       d.bits, (unsigned long long)d.total, d.kbps, d.buscable);
	if (desde_ms >= 0 && dec_busca(&d, (u64)desde_ms * d.hz / 1000))
		fprintf(stderr, "no se puede buscar\n");
	f = fopen(sal, "wb");
	if (!f)
		return 1;
	escribe_wav(f, d.hz, 0);
	while ((k = dec_lee(&d, b, 4096)) > 0) {
		for (i = 0; i < 2 * k; i++) {
			float x = b[i] * 32768.0f;
			int v = (int)lrintf(x);

			s[i] = (i16)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
		}
		fwrite(s, 4, (size_t)k, f);
		n += (u64)k;
	}
	escribe_wav(f, d.hz, n);
	fclose(f);
	printf("tramas=%llu\n", (unsigned long long)n);
	dec_cierra(&d);
	return 0;
}

static void ensena_etiquetas(const char *ruta)
{
	etiquetas e;

	etiquetas_lee(ruta, &e, 1);
	printf("titulo=%s\nartista=%s\nalbum=%s\nanyo=%s\npista=%s\ngenero=%s\ncomentario=%s\n", e.titulo, e.artista, e.album,
	       e.anyo, e.pista, e.genero, e.comentario);
	printf("rg_pista=%d (%d) rg_album=%d (%d) picos=%d %d\n", e.rg_pista, e.hay_rg_pista, e.rg_album, e.hay_rg_album,
	       e.rg_pista_pico, e.rg_album_pico);
	printf("caratula=%u", e.caratula_largo);
	if (e.caratula_largo >= 4)
		printf(" (%02x %02x %02x %02x)", e.caratula[0], e.caratula[1], e.caratula[2], e.caratula[3]);
	printf("\n");
	etiquetas_suelta(&e);
}

static void ensena_lista(const char *ruta)
{
	lista l;
	int i;

	lista_nueva(&l);
	lista_anade_archivo(&l, ruta);
	while (lista_paso(&l))
		;
	for (i = 0; i < l.n; i++) {
		cancion *c = &l.c[i];

		printf("%d|%s|%s|%s|%u|desde=%llu|largo=%llu|tramo=%u-%u|lector=%c\n", i, c->o.ruta, c->artista, c->titulo,
		       c->duracion, (unsigned long long)c->o.desde, (unsigned long long)c->o.largo, c->o.tramo_desde,
		       c->o.tramo_hasta, c->o.lector ? c->o.lector : '-');
	}
	lista_vacia(&l);
}

static int por_motor(const char *sal, int n, char **rutas, i64 busca_ms)
{
	lista l;
	motor m;
	int i, vueltas = 0, fundido = getenv("REPRO_FUNDIDO") ? atoi(getenv("REPRO_FUNDIDO")) : 0;

	setenv("REPRO_WAV", sal, 1);
	lista_nueva(&l);
	for (i = 0; i < n; i++)
		lista_anade_archivo(&l, rutas[i]);
	while (lista_paso(&l))
		;
	if (motor_prepara(&m, &l))
		return 1;
	m.replaygain = getenv("REPRO_RG") ? atoi(getenv("REPRO_RG")) : RG_NO;
	m.fundido = fundido;
	if (getenv("REPRO_VELOCIDAD"))
		motor_velocidad(&m, (float)atof(getenv("REPRO_VELOCIDAD")));
	if (getenv("REPRO_EQ")) {
		/* «preamp,b1,...,b10» en dB */
		const char *p = getenv("REPRO_EQ");

		m.eq.activo = 1;
		m.eq.preamp = atoi(p) * 10;
		for (i = 0; i < BANDAS && (p = strchr(p, ',')); i++) {
			p++;
			m.eq.banda[i] = atoi(p) * 10;
		}
		eq_cambia(&m.eq);
	}
	if (getenv("REPRO_SIN_HUECOS"))
		m.sin_huecos = atoi(getenv("REPRO_SIN_HUECOS"));
	motor_toca(&m, 0);
	if (busca_ms >= 0) {
		while (!m.v[m.va].lista && vueltas++ < 1000)
			motor_ciclo(&m, 0);
		motor_busca_ms(&m, (u32)busca_ms);
	}
	while (m.estado != M_PARADO && vueltas++ < 10000000)
		motor_ciclo(&m, 50);
	printf("salida=%llu hz=%u error=%s\n", (unsigned long long)m.salida, m.hz, m.error);
	for (i = 0; i < l.n; i++)
		printf("cancion %d: %s duracion=%u fallo=%d\n", i, l.c[i].titulo, l.c[i].duracion, l.c[i].fallo);
	motor_cierra(&m);
	lista_vacia(&l);
	return 0;
}

static void fuzz(const char *ruta)
{
	u32 n;
	u8 *d = so_lee_todo(ruta, &n, 64u * 1024 * 1024);
	etiquetas e;
	dec x;
	float b[2048 * 2];
	int k, vueltas = 0;
	lista l;

	if (!d)
		return;
	memset(&e, 0, sizeof e);
	etiquetas_id3v2(d, n, &e, 1);
	etiquetas_suelta(&e);
	memset(&e, 0, sizeof e);
	etiquetas_id3v1(d, n, &e);
	etiquetas_vorbis(d, n, &e, 1);
	etiquetas_suelta(&e);
	memset(&e, 0, sizeof e);
	etiquetas_flac(d, n, &e, 1);
	etiquetas_suelta(&e);
	memset(&e, 0, sizeof e);
	etiquetas_ogg(d, n, &e, 1);
	etiquetas_suelta(&e);
	memset(&e, 0, sizeof e);
	etiquetas_wav(d, n, &e, 1);
	etiquetas_suelta(&e);
	free(d);
	/* (como lista, como hoja y como canción) */
	lista_nueva(&l);
	lista_lee(&l, ruta);
	lista_anade_cue(&l, ruta);
	lista_vacia(&l);
	if (abre_listo(&x, ruta) == 0) {
		while ((k = dec_lee(&x, b, 2048)) > 0 && vueltas++ < 20000)
			;
		dec_busca(&x, x.total / 2);
		while ((k = dec_lee(&x, b, 2048)) > 0 && vueltas++ < 40000)
			;
	}
	dec_cierra(&x);
}

/* tonos por el ecualizador, el remuestreador y la velocidad: lo que sale */
static double nivel_tono(const float *s, int n, double f, u32 hz)
{
	double c = 0, q = 0;
	int i;

	for (i = 0; i < n; i++) {
		c += s[2 * i] * cos(2 * M_PI * f * i / hz);
		q += s[2 * i] * sin(2 * M_PI * f * i / hz);
	}
	return 2 * sqrt(c * c + q * q) / n;
}

static void prueba_dsp(void)
{
	ecualizador q;
	remuestreo r;
	estira e;
	static float t[48000 * 2], s[96000 * 2];
	int i, b, n;

	memset(&q, 0, sizeof q);
	for (b = 0; b < BANDAS; b++) {
		double f = frecuencias_eq[b];

		for (i = 0; i < 48000; i++)
			t[2 * i] = t[2 * i + 1] = (float)(0.25 * sin(2 * M_PI * f * i / 48000));
		memset(&q, 0, sizeof q);
		q.activo = 1;
		q.banda[b] = 60;	/* +6 dB */
		eq_prepara(&q, 48000);
		eq_hace(&q, t, 48000);
		printf("eq banda %d (%g Hz) +6 dB: %.2f dB\n", b, f, 20 * log10(nivel_tono(t + 2 * 24000, 24000, f, 48000) / 0.25));
	}
	/* remuestrear 1 kHz de 44100 a 48000 */
	for (i = 0; i < 44100; i++)
		t[2 * i] = t[2 * i + 1] = (float)(0.5 * sin(2 * M_PI * 1000 * i / 44100));
	memset(&r, 0, sizeof r);
	rm_prepara(&r, 44100, 48000);
	n = rm_hace(&r, t, 44100, s, 96000);
	printf("rm 44100->48000: %d tramas, 1 kHz a %.2f dB, 3 kHz a %.1f dB\n", n, 20 * log10(nivel_tono(s + 4800, 40000, 1000, 48000) / 0.5),
	       20 * log10(nivel_tono(s + 4800, 40000, 3000, 48000) / 0.5 + 1e-12));
	rm_suelta(&r);
	/* la velocidad: 1,5 con 440 Hz: dura 2/3 y sigue a 440 */
	for (i = 0; i < 48000; i++)
		t[2 * i] = t[2 * i + 1] = (float)(0.5 * sin(2 * M_PI * 440 * i / 48000));
	memset(&e, 0, sizeof e);
	es_prepara(&e, 48000);
	es_velocidad(&e, 1.5f);
	n = es_hace(&e, t, 48000, s, 96000);
	printf("velocidad 1,5: %d tramas (de 48000), 440 Hz a %.2f dB, 660 Hz a %.1f dB\n", n,
	       20 * log10(nivel_tono(s + 2 * 4000, n - 8000, 440, 48000) / 0.5), 20 * log10(nivel_tono(s + 2 * 4000, n - 8000, 660, 48000) / 0.5 + 1e-12));
	es_velocidad(&e, 0.5f);
	es_vacia(&e);
	n = es_hace(&e, t, 48000, s, 96000);
	printf("velocidad 0,5: %d tramas, 440 Hz a %.2f dB\n", n, 20 * log10(nivel_tono(s + 2 * 4000, n - 8000, 440, 48000) / 0.5));
	es_suelta(&e);
}

int main(int argc, char **argv)
{
	anfitrion_argumentos(argc, argv);
	if (argc >= 4 && !strcmp(argv[1], "decodifica"))
		return decodifica(argv[2], argv[3], -1);
	if (argc >= 5 && !strcmp(argv[1], "busca"))
		return decodifica(argv[2], argv[4], atoll(argv[3]));
	if (argc >= 4 && !strcmp(argv[1], "motor"))
		return por_motor(argv[2], argc - 3, argv + 3, getenv("REPRO_BUSCA") ? atoll(getenv("REPRO_BUSCA")) : -1);
	if (argc >= 3 && !strcmp(argv[1], "etiquetas")) {
		ensena_etiquetas(argv[2]);
		return 0;
	}
	if (argc >= 3 && !strcmp(argv[1], "lista")) {
		ensena_lista(argv[2]);
		return 0;
	}
	if (argc >= 3 && !strcmp(argv[1], "duracion")) {
		origen o;

		origen_de_ruta(&o, argv[2]);
		printf("%u\n", duracion_rapida(&o));
		return 0;
	}
	if (argc >= 3 && !strcmp(argv[1], "fuzz")) {
		int i;

		for (i = 2; i < argc; i++)
			fuzz(argv[i]);
		return 0;
	}
	if (argc >= 2 && !strcmp(argv[1], "dsp")) {
		prueba_dsp();
		return 0;
	}
	fprintf(stderr, "uso: repro decodifica|busca|motor|etiquetas|lista|duracion|fuzz|dsp ...\n");
	return 2;
}
