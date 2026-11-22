/* sintes.c - (JUEGOS) el sonido de los juegos: todo se sintetiza aquí
 * (nada grabado ni de nadie). Un canal del servicio de sonido (SONIDO.BIN,
 * las llamadas 190 a 192) de 22.050 Hz, mono y 16 bits; si no hay servicio
 * o tarjeta, en silencio y sin fallar. Hasta 12 voces a la vez (cuadrada,
 * triángulo, sierra, ruido, seno y campana), cada una con su barrido de
 * frecuencia, su ataque y su caída; y la música: dos voces leídas de una
 * partitura en texto, como las de los BASIC de siempre:
 *
 *   T120   el tempo (negras por minuto)     O4   la octava (la 4: el la de 440 Hz)
 *   L8     lo que dura una nota sin número  V60  el volumen (0-100)
 *   @0-5   la forma de la onda (ON_*)        > <  una octava arriba o abajo
 *   c d e f g a b, con + o # (sostenido) o - (bemol), y su duración (4, 8.,
 *   16...); r, un silencio; | y los espacios no cuentan.
 *
 * Se le dan al canal muestras para unos 50 ms por delante (lo mira en cada
 * pulso: snd_alimentar), sin esperar nunca (el bit 31 de la 191).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "juego.h"

#define HZ 22050
#define MAX_VOCES 12
#define POR_DELANTE (HZ * 50 / 1000)	/* muestras */
#define TOPE (HZ * 80 / 1000)

int snd_hay, snd_mudo, snd_sin_musica;
u32 snd_efectos_dados;
static u32 canal;
static int abierto;

struct voz {
	int viva, onda, musica;
	u32 fase, paso;		/* 32 bits de fase por muestra */
	i32 dpaso;		/* lo que cambia el paso en cada muestra (el barrido) */
	int n, quedan;		/* las muestras que dura y las que le quedan */
	int ataque, caida;	/* en muestras */
	int vol;		/* 0-32767 */
	int espera;		/* las muestras que espera antes de empezar */
	u32 ruido, retiene;	/* (el ruido: su registro y lo que se queda igual) */
	int val;
};
static struct voz voces[MAX_VOCES];
static i16 buf[TOPE + 64];

static u32 paso_de(float hz) { return (u32)(hz * (4294967296.0f / (float)HZ)); }

static struct voz *voz_libre(int musica)
{
	int i, peor = -1, q = 1 << 30;

	for (i = 0; i < MAX_VOCES; i++)
		if (!voces[i].viva)
			return &voces[i];
	/* la que menos le queda (que no sea de la música) */
	for (i = 0; i < MAX_VOCES; i++)
		if (voces[i].musica == musica && voces[i].quedan < q) {
			q = voces[i].quedan;
			peor = i;
		}
	return peor >= 0 ? &voces[peor] : &voces[0];
}

static void suena_tras(int onda, float f0, float f1, int ms, int vol, int ataque, int caida, int musica, int tras)
{
	struct voz *v;
	int n = HZ * ms / 1000;

	if (!snd_hay || n <= 0 || vol <= 0)
		return;
	f0 = LIMITA(f0, 20.0f, 10000.0f);
	f1 = LIMITA(f1, 20.0f, 10000.0f);
	v = voz_libre(musica);
	memset(v, 0, sizeof *v);
	v->viva = 1;
	v->musica = musica;
	v->onda = onda;
	v->n = v->quedan = n;
	v->paso = paso_de(f0);
	v->dpaso = (i32)(((float)paso_de(f1) - (float)v->paso) / (float)n);
	v->ataque = MAX(1, HZ * ataque / 1000);
	v->caida = MAX(1, HZ * caida / 1000);
	if (v->ataque + v->caida > n) {
		v->ataque = n / 4 + 1;
		v->caida = n - v->ataque;
	}
	v->vol = vol * 32767 / 100 / 3;
	v->ruido = 0xACE1u ^ (u32)azar();
	v->retiene = 0;
	v->espera = HZ * tras / 1000;
}

static void suena(int onda, float f0, float f1, int ms, int vol, int ataque, int caida, int musica)
{
	suena_tras(onda, f0, f1, ms, vol, ataque, caida, musica, 0);
}

void snd_efecto(const struct efecto *e)
{
	if (snd_mudo)
		return;
	snd_efectos_dados++;
	suena(e->onda, e->f0, e->f1, e->ms, e->vol, e->ataque, e->caida, 0);
}

void snd_nota(int onda, float hz, int ms, int vol)
{
	snd_nota_tras(onda, hz, ms, vol, 0);
}

void snd_nota_tras(int onda, float hz, int ms, int vol, int tras)
{
	if (snd_mudo)
		return;
	snd_efectos_dados++;
	suena_tras(onda, hz, hz, ms, vol, 4, ms / 2, 0, tras);
}

void snd_efecto_tras(const struct efecto *e, int tras)
{
	if (snd_mudo)
		return;
	snd_efectos_dados++;
	suena_tras(e->onda, e->f0, e->f1, e->ms, e->vol, e->ataque, e->caida, 0, tras);
}

/* ---- la música ---- */
struct pista {
	const char *ini, *p;
	int octava, largo, tempo, vol, onda;
	int espera;		/* muestras hasta la nota siguiente */
};
static struct pista pistas[2];
static int musica_bucle, musica_viva;

static void pista_empieza(struct pista *t, const char *s)
{
	memset(t, 0, sizeof *t);
	t->ini = t->p = s;
	t->octava = 4;
	t->largo = 8;
	t->tempo = 120;
	t->vol = 50;
	t->onda = ON_TRIANGULO;
}

static int numero(const char **p)
{
	int v = 0;

	while (**p >= '0' && **p <= '9')
		v = v * 10 + (*(*p)++ - '0');
	return v;
}

/* lee lo siguiente de la pista: 0 si se ha acabado */
static int pista_sigue(struct pista *t)
{
	static const int semitono[7] = { 9, 11, 0, 2, 4, 5, 7 };	/* a b c d e f g */

	if (!t->p)
		return 0;
	for (;;) {
		char c = *t->p;

		if (!c) {
			if (!musica_bucle)
				return 0;
			t->p = t->ini;
			continue;
		}
		t->p++;
		if (c >= 'A' && c <= 'Z')
			c = (char)(c + 32);
		if (c == ' ' || c == '|' || c == '\n')
			continue;
		if (c == 't' || c == 'o' || c == 'l' || c == 'v' || c == '@') {
			int v = numero(&t->p);	/* (una vez: MAX y LIMITA lo mirarían dos) */

			if (c == 't') t->tempo = MAX(30, v);
			else if (c == 'o') t->octava = LIMITA(v, 1, 7);
			else if (c == 'l') t->largo = MAX(1, v);
			else if (c == 'v') t->vol = LIMITA(v, 0, 100);
			else t->onda = LIMITA(v, 0, 5);
			continue;
		}
		if (c == '>') { if (t->octava < 7) t->octava++; continue; }
		if (c == '<') { if (t->octava > 1) t->octava--; continue; }
		if ((c >= 'a' && c <= 'g') || c == 'r') {
			int s = 0, largo, ms, nota = c != 'r';
			float puntillo = 1.0f;

			if (nota) {
				s = semitono[c - 'a'];
				while (*t->p == '+' || *t->p == '#' || *t->p == '-') {
					s += *t->p == '-' ? -1 : 1;
					t->p++;
				}
			}
			largo = numero(&t->p);
			if (!largo)
				largo = t->largo;
			while (*t->p == '.') {
				puntillo *= 1.5f;
				t->p++;
			}
			ms = (int)(240000.0f / (float)t->tempo / (float)largo * puntillo);
			t->espera = HZ * ms / 1000;
			if (nota && !snd_sin_musica) {
				/* (la del la de la octava 4: 440 Hz) */
				int n = (t->octava - 4) * 12 + s - 9;
				float hz = 440.0f;

				while (n >= 12) { hz *= 2; n -= 12; }
				while (n < 0) { hz *= 0.5f; n += 12; }
				{
					static const float semi[12] = { 1.0f, 1.0594631f, 1.1224620f, 1.1892071f, 1.2599210f, 1.3348399f,
						1.4142136f, 1.4983071f, 1.5874011f, 1.6817928f, 1.7817974f, 1.8877486f };

					hz *= semi[n];
				}
				suena(t->onda, hz, hz, ms * 9 / 10, t->vol, 6, ms / 3, 1);
			}
			return 1;
		}
	}
}

void snd_musica(const char *v1, const char *v2, int bucle)
{
	int i;

	snd_musica_parar();
	musica_bucle = bucle;
	pista_empieza(&pistas[0], v1);
	pista_empieza(&pistas[1], v2);
	musica_viva = 1;
	(void)i;
}

void snd_musica_parar(void)
{
	int i;

	musica_viva = 0;
	pistas[0].p = pistas[1].p = 0;
	for (i = 0; i < MAX_VOCES; i++)
		if (voces[i].musica)
			voces[i].viva = 0;
}

/* ---- la mezcla ---- */
static void mezcla_muestras(i16 *d, int n)
{
	static i32 acc[TOPE + 64];
	int i, k;

	memset(acc, 0, (size_t)n * 4);
	/* la música: las notas empiezan en su muestra */
	for (k = 0; k < n; k++) {
		if (musica_viva) {
			int t;

			for (t = 0; t < 2; t++)
				if (pistas[t].p) {
					if (pistas[t].espera <= 0 && !pista_sigue(&pistas[t]))
						pistas[t].p = 0;
					pistas[t].espera--;
				}
			if (!pistas[0].p && !pistas[1].p)
				musica_viva = 0;
		}
		for (i = 0; i < MAX_VOCES; i++) {
			struct voz *v = &voces[i];
			int s, e, hecho;

			if (!v->viva)
				continue;
			if (v->espera > 0) {
				v->espera--;
				continue;
			}
			hecho = v->n - v->quedan;
			if (hecho < v->ataque)
				e = hecho * 256 / v->ataque;
			else if (v->quedan < v->caida)
				e = v->quedan * 256 / v->caida;
			else
				e = 256;
			switch (v->onda) {
			case ON_CUADRADA:
				s = (v->fase & 0x80000000u) ? 24000 : -24000;
				break;
			case ON_TRIANGULO: {
				u32 f = v->fase >> 16;

				s = f < 32768 ? (int)f - 16384 : 49151 - (int)f;
				s *= 2;
				break;
			}
			case ON_SIERRA:
				s = (int)(v->fase >> 17) - 16384;
				s *= 2;
				break;
			case ON_RUIDO:
				v->retiene += v->paso >> 8;
				if (v->retiene >= 0x00800000u) {
					v->retiene -= 0x00800000u;
					v->ruido = (v->ruido >> 1) ^ (-(v->ruido & 1u) & 0xB400u);
					v->val = (v->ruido & 1) ? 22000 : -22000;
				}
				s = v->val;
				break;
			default: {
				/* seno por una parábola (sin tablas): bastante para esto */
				int x = (int)(v->fase >> 16) - 32768, y = (x < 0 ? -x : x);

				s = (x * (32768 - y)) >> 13;	/* hasta 32768 */
				s = s * 3 / 4;
				if (v->onda == ON_CAMPANA) {
					u32 f2 = v->fase * 2;
					int x2 = (int)(f2 >> 16) - 32768, y2 = x2 < 0 ? -x2 : x2;

					s = (s * 2 + ((x2 * (32768 - y2)) >> 13) / 2) / 3;
					/* (la campana: cae desde el principio) */
					e = v->quedan * 256 / v->n;
					e = e * e / 256;
					if (hecho < v->ataque)
						e = e * hecho / v->ataque;
				}
			}
			}
			acc[k] += (s * e >> 8) * v->vol >> 15;
			v->fase += v->paso;
			v->paso = (u32)((i32)v->paso + v->dpaso);
			if (--v->quedan <= 0)
				v->viva = 0;
		}
	}
	for (k = 0; k < n; k++)
		d[k] = (i16)LIMITA(acc[k], -32767, 32767);
}

int snd_abrir(void)
{
	u32 r;

	if (abierto)
		return snd_hay;
	abierto = 1;
	if (arg_hay("/MUDO"))
		return 0;
	r = sc(190, HZ, 1 | (16 << 8), 0);
	if (ES_ERROR(r) || r == 0 || r > 64) {
		serie("juego: sin sonido (no hay servicio de sonido o tarjeta)\n");
		return 0;
	}
	canal = r;
	snd_hay = 1;
	serie("juego: sonido por el canal %u (22050 Hz)\n", canal);
	return 1;
}

void snd_alimentar(void)
{
	u32 q;
	int n;

	if (!snd_hay)
		return;
	q = sc(192, canal, 4, 0);
	if (ES_ERROR(q))
		return;
	q /= 2;
	if (q >= POR_DELANTE)
		return;
	n = TOPE - (int)q;
	if (n <= 0)
		return;
	mezcla_muestras(buf, n);
	sc(191, canal | 0x80000000u, (u32)buf, (u32)n * 2);
}

void snd_cerrar(void)
{
	if (snd_hay) {
		sc(192, canal, 7, 0);	/* cerrarlo cuando acabe lo que tiene */
		snd_hay = 0;
	}
}

float nota_hz(int midi)
{
	static const float semi[12] = { 1.0f, 1.0594631f, 1.1224620f, 1.1892071f, 1.2599210f, 1.3348399f,
		1.4142136f, 1.4983071f, 1.5874011f, 1.6817928f, 1.7817974f, 1.8877486f };
	int n = midi - 69;
	float hz = 440.0f;

	while (n >= 12) { hz *= 2; n -= 12; }
	while (n < 0) { hz *= 0.5f; n += 12; }
	return hz * semi[n];
}
