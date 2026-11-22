/* prueba_sonido.c - (JUEGOS) la música y los efectos de sintes.c, en el
 * anfitrión: lo que sale, a un .WAV, y cuánto suena cada décima.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "juego.h"
extern int16_t *sonido_grabado;
extern size_t sonido_n;
extern u32 tics_falsos;
char jg_args[256];
int arg_hay(const char *o) { (void)o; return 0; }
u32 azar(void) { static u32 s = 1; s = s * 1103515245u + 12345u; return s >> 8; }
void serie(const char *f, ...) { (void)f; }
int main(int argc, char **argv)
{
	static const char m1[] = "T132 O5 L8 V34 @1 e4 a b > c4 < b a | g4 e f g4 a b |";
	static const char m2[] = "T132 L4 V52 @1 o2 a o3 e o2 a o3 e | o2 g o3 d o2 g o3 d |";
	int i;
	FILE *f;

	snd_abrir();
	snd_musica(m1, m2, 0);
	for (i = 0; i < 400; i++) {
		tics_falsos++;
		snd_alimentar();
	}
	f = fopen(argc > 1 ? argv[1] : "/tmp/sintes.raw", "wb");
	fwrite(sonido_grabado, 2, sonido_n, f);
	fclose(f);
	printf("muestras=%zu\n", sonido_n);
	for (i = 0; i < (int)sonido_n / 2205; i++) {
		int k, m = 0;

		for (k = 0; k < 2205; k++)
			if (abs(sonido_grabado[i * 2205 + k]) > m)
				m = abs(sonido_grabado[i * 2205 + k]);
		printf("%d ", m);
	}
	printf("\n");
	return 0;
}
