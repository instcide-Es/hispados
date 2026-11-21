/* mezcla.c - (SONIDO) la mezcla de SONIDO.BIN: todas las voces (los canales
 * de los programas, que el núcleo guarda en sus colas; los .WAV que se
 * tocan con la llamada 194; los sonidos del sistema y los tonos de prueba)
 * sumadas en estéreo de 16 bits a la frecuencia de la tarjeta, cada una con
 * su volumen y el general, y recortadas.
 *
 * Cada voz se pasa a la frecuencia de la tarjeta con interpolación lineal
 * (el paso, en 16.16: lo que avanza la voz por cada trama de la salida). El
 * volumen sigue la curva de HSPSND y del Mezclador de Hispa-486: el 100 %
 * es 0 dB y cada punto menos, 0,47 dB menos (el 1 %, 46,5 dB menos); el 0 %,
 * silencio. En silencio (o con el volumen a 0) las voces siguen avanzando:
 * un programa que suena no se queda esperando.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "sonido.h"

u32 vol_general = 80;
int en_silencio;
static u32 ganancia[101];		/* 16.16 */
static i16 seno[256];

enum { V_LIBRE, V_CANAL, V_MEMORIA, V_ARCHIVO, V_TONO };
struct voz {
	int clase;
	u32 id, dueno;			/* (la 194) su número y su tarea */
	int canal;			/* V_CANAL: el del núcleo (1 a 8) */
	u32 hz, can, bits, paso, frac;
	i32 l0, r0, l1, r1;
	int lista;			/* ya tiene sus dos primeras tramas */
	int pausa, volumen, lados;
	u8 buf[4096];			/* lo leído y aún no tocado (canales y archivos) */
	u32 n, pos;
	const u8 *mem;			/* V_MEMORIA */
	u32 mem_n, mem_pos;
	u32 fich, restan;		/* V_ARCHIVO */
	u32 fase, fpaso, tramas, hechas;	/* V_TONO */
};
static struct voz voces[MAX_VOCES];
static int canales_con_datos;		/* algún canal del núcleo tiene muestras en su cola */

void mezcla_prepara(void)
{
	int v, i;

	/* 10^(-0,47/20) = 0,94735 = 62085 / 65536 */
	ganancia[100] = 65536;
	for (v = 99; v >= 1; v--)
		ganancia[v] = ganancia[v + 1] * 62085u >> 16;
	ganancia[0] = 0;
	/* el seno, por la aproximación de Bhaskara (sin coma flotante) */
	for (i = 0; i < 256; i++) {
		int x = i & 127, s;		/* media vuelta: 128 pasos de pi/128 */
		/* sen(x·pi/128) ≈ 16x(128-x) / (5·128² - 4x(128-x)) */
		s = 32000 * (16 * x * (128 - x)) / (5 * 128 * 128 - 4 * x * (128 - x));
		seno[i] = (i16)(i < 128 ? s : -s);
	}
}

static struct voz *voz_libre(void)
{
	int i;

	for (i = 0; i < MAX_VOCES; i++)
		if (voces[i].clase == V_LIBRE) {
			memset(&voces[i], 0, sizeof voces[i]);
			voces[i].volumen = 100;
			voces[i].lados = 3;
			return &voces[i];
		}
	return 0;
}

static void suelta(struct voz *v)
{
	if (v->clase == V_ARCHIVO && v->fich)
		sc(SC_CERRAR, v->fich, 0, 0);
	v->clase = V_LIBRE;
}

static void pon_paso(struct voz *v)
{
	v->paso = divide64((u64)v->hz << 16, hz_salida);
	if (!v->paso)
		v->paso = 1;
}

/* la trama siguiente de la voz: 0 si no hay (aún, o ya) */
static int sig_trama(struct voz *v, i32 *l, i32 *r)
{
	u32 tb = v->can * v->bits / 8;
	const u8 *p;

	if (v->clase == V_MEMORIA) {
		if (v->mem_pos + tb > v->mem_n)
			return 0;
		p = v->mem + v->mem_pos;
		v->mem_pos += tb;
	} else {
		if (v->pos + tb > v->n) {
			u32 queda = v->n - v->pos, k = 0;

			memmove(v->buf, v->buf + v->pos, queda);
			v->n = queda;
			v->pos = 0;
			if (v->clase == V_CANAL && v->canal) {
				k = sc(SC_SACAR, v->canal, (u32)(v->buf + queda), sizeof v->buf - queda);
			} else if (v->clase == V_ARCHIVO && v->restan) {
				u32 pide = sizeof v->buf - queda;

				if (pide > v->restan)
					pide = v->restan;
				k = sc(SC_LEER, v->fich, (u32)(v->buf + queda), pide);
				if (ES_ERROR(k) || k == 0) {
					k = 0;
					v->restan = 0;
				} else
					v->restan -= k;
			}
			if (ES_ERROR(k))
				k = 0;
			v->n += k;
			if (v->pos + tb > v->n)
				return 0;
		}
		p = v->buf + v->pos;
		v->pos += tb;
	}
	if (v->bits == 8) {
		*l = ((i32)p[0] - 128) << 8;
		*r = v->can == 2 ? ((i32)p[1] - 128) << 8 : *l;
	} else {
		*l = (i16)(p[0] | p[1] << 8);
		*r = v->can == 2 ? (i16)(p[2] | p[3] << 8) : *l;
	}
	return 1;
}

static int prepara_voz(struct voz *v)
{
	if (!sig_trama(v, &v->l0, &v->r0))
		return 0;
	if (!sig_trama(v, &v->l1, &v->r1)) {
		v->l1 = v->l0;
		v->r1 = v->r0;
	}
	v->frac = 0;
	v->lista = 1;
	return 1;
}

/* suma la voz v a acc (tramas estéreo) */
static void suma(struct voz *v, i32 *acc, u32 tramas)
{
	u32 g, i;

	if (v->pausa)
		return;
	g = (u32)((u64)ganancia[v->volumen] * ganancia[vol_general] >> 16);
	if (en_silencio)
		g = 0;
	if (v->clase == V_TONO) {
		for (i = 0; i < tramas && v->hechas < v->tramas; i++, v->hechas++) {
			i32 s = seno[v->fase >> 24], env = 256;
			u32 rampa = hz_salida / 100;	/* 10 ms de subida y de bajada */

			if (v->hechas < rampa)
				env = v->hechas * 256 / rampa;
			else if (v->tramas - v->hechas < rampa)
				env = (v->tramas - v->hechas) * 256 / rampa;
			s = (i32)((i64)s * env / 256 * g >> 16);
			if (v->lados & 1)
				acc[2 * i] += s;
			if (v->lados & 2)
				acc[2 * i + 1] += s;
			v->fase += v->fpaso;
		}
		if (v->hechas >= v->tramas)
			suelta(v);
		return;
	}
	if (!v->lista && !prepara_voz(v)) {
		if (v->clase != V_CANAL || !v->canal)
			suelta(v);	/* (un .WAV vacío, o ya acabado) */
		return;
	}
	for (i = 0; i < tramas; i++) {
		i32 l = v->l0 + (i32)(((i64)(v->l1 - v->l0) * v->frac) >> 16);
		i32 r = v->r0 + (i32)(((i64)(v->r1 - v->r0) * v->frac) >> 16);

		l = (i32)((i64)l * g >> 16);
		r = (i32)((i64)r * g >> 16);
		if (v->lados == 1) {
			acc[2 * i] += (l + r) / 2;
		} else if (v->lados == 2) {
			acc[2 * i + 1] += (l + r) / 2;
		} else {
			acc[2 * i] += l;
			acc[2 * i + 1] += r;
		}
		v->frac += v->paso;
		while (v->frac >= 0x10000) {
			v->frac -= 0x10000;
			v->l0 = v->l1;
			v->r0 = v->r1;
			if (!sig_trama(v, &v->l1, &v->r1)) {
				/* no hay más: un canal espera a que el programa dé más;
				 * lo demás se ha acabado */
				v->lista = 0;
				if (v->clase != V_CANAL || !v->canal)
					suelta(v);
				return;
			}
		}
	}
}

void mezcla(i16 *d, u32 tramas)
{
	static i32 acc[2 * 2048];
	u32 i, k;
	int j;

	while (tramas) {
		k = tramas > 2048 ? 2048 : tramas;
		memset(acc, 0, k * 8);
		for (j = 0; j < MAX_VOCES; j++)
			if (voces[j].clase != V_LIBRE)
				suma(&voces[j], acc, k);
		for (i = 0; i < 2 * k; i++) {
			i32 s = acc[i];

			d[i] = (i16)(s > 32767 ? 32767 : s < -32768 ? -32768 : s);
		}
		d += 2 * k;
		tramas -= k;
	}
}

int mezcla_activa(void)
{
	int j;

	if (canales_con_datos)
		return 1;
	for (j = 0; j < MAX_VOCES; j++) {
		struct voz *v = &voces[j];

		if (v->clase == V_LIBRE || v->pausa)
			continue;
		if (v->clase != V_CANAL || v->lista || v->n > v->pos)
			return 1;
	}
	return 0;
}

u32 mezcla_voces(void)
{
	int j;
	u32 n = 0;

	for (j = 0; j < MAX_VOCES; j++)
		if (voces[j].clase != V_LIBRE && voces[j].clase != V_CANAL)
			n++;
	return n;
}

/* los canales del núcleo: uno nuevo, una voz; uno que se ha ido, fuera */
void mezcla_canales(struct para_servicio *p)
{
	int i, j;

	for (j = 0; j < MAX_VOCES; j++) {
		struct voz *v = &voces[j];
		struct info_canal *c;

		if (v->clase != V_CANAL || !v->canal)
			continue;
		c = &p->canales[v->canal - 1];
		if (c->dueno != v->dueno || c->hz != v->hz || c->canales != v->can || c->bits != v->bits) {
			/* el canal se ha cerrado: lo que ya había sacado de la cola suena hasta el final */
			v->canal = 0;
			if (!v->lista && v->n <= v->pos)
				suelta(v);
		}
	}
	canales_con_datos = 0;
	for (i = 0; i < MAX_CANALES; i++) {
		struct info_canal *c = &p->canales[i];
		struct voz *v = 0;

		if (!c->dueno)
			continue;
		if (c->en_cola && !(c->pausa & 1))
			canales_con_datos = 1;
		for (j = 0; j < MAX_VOCES; j++)
			if (voces[j].clase == V_CANAL && voces[j].canal == i + 1)
				v = &voces[j];
		if (!v) {
			v = voz_libre();
			if (!v)
				continue;
			v->clase = V_CANAL;
			v->canal = i + 1;
			v->dueno = c->dueno;
			v->hz = c->hz;
			v->can = c->canales;
			v->bits = c->bits;
			pon_paso(v);
		}
		v->volumen = c->volumen > 100 ? 100 : c->volumen;
		v->pausa = c->pausa & 1;
	}
}

u32 wav_cabecera(const u8 *d, u32 n, u32 *hz, u32 *canales, u32 *bits, u32 *largo)
{
	u32 k = 12, fmt = 0;

	if (n < 44 || memcmp(d, "RIFF", 4) || memcmp(d + 8, "WAVE", 4))
		return 0;
	*hz = *canales = *bits = 0;
	while (k + 8 <= n) {
		u32 t = d[k + 4] | d[k + 5] << 8 | d[k + 6] << 16 | (u32)d[k + 7] << 24;

		if (!memcmp(d + k, "fmt ", 4) && k + 24 <= n) {
			fmt = d[k + 8] | d[k + 9] << 8;
			*canales = d[k + 10] | d[k + 11] << 8;
			*hz = d[k + 12] | d[k + 13] << 8 | d[k + 14] << 16;
			*bits = d[k + 22] | d[k + 23] << 8;
		} else if (!memcmp(d + k, "data", 4)) {
			if ((fmt != 1 && fmt != 0xFFFE) || (*canales != 1 && *canales != 2) || (*bits != 8 && *bits != 16) ||
			    *hz < 4000 || *hz > 96000)
				return 0;
			*largo = t;
			return k + 8;
		}
		k += 8 + t + (t & 1);
	}
	return 0;
}

int voz_memoria(const u8 *wav, u32 n, u32 id, u32 dueno, int lados, int volumen)
{
	u32 hz, can, bits, largo, ini = wav_cabecera(wav, n, &hz, &can, &bits, &largo);
	struct voz *v;

	if (!ini || !(v = voz_libre()))
		return 0;
	if (largo > n - ini)
		largo = n - ini;
	v->clase = V_MEMORIA;
	v->id = id;
	v->dueno = dueno;
	v->hz = hz;
	v->can = can;
	v->bits = bits;
	v->mem = wav + ini;
	v->mem_n = largo;
	v->lados = lados;
	v->volumen = volumen;
	pon_paso(v);
	return 1;
}

int voz_archivo(const char *ruta, u32 id, u32 dueno)
{
	struct voz *v = voz_libre();
	u32 f, k, hz, can, bits, largo, ini;

	if (!v)
		return 0;
	f = sc(SC_ABRIR, (u32)ruta, 0, 0);
	if (ES_ERROR(f))
		return 0;
	k = sc(SC_LEER, f, (u32)v->buf, sizeof v->buf);
	if (ES_ERROR(k) || !(ini = wav_cabecera(v->buf, k, &hz, &can, &bits, &largo))) {
		sc(SC_CERRAR, f, 0, 0);
		return 0;
	}
	v->clase = V_ARCHIVO;
	v->id = id;
	v->dueno = dueno;
	v->fich = f;
	v->hz = hz;
	v->can = can;
	v->bits = bits;
	v->pos = ini;
	v->n = k;
	if (largo < k - ini) {
		v->n = ini + largo;
		v->restan = 0;
	} else
		v->restan = largo - (k - ini);
	pon_paso(v);
	return 1;
}

int voz_tono(u32 hz, u32 ms, int lados, int volumen)
{
	struct voz *v = voz_libre();

	if (!v)
		return 0;
	v->clase = V_TONO;
	v->fpaso = divide64((u64)hz << 32, hz_salida);
	v->tramas = hz_salida / 10 * ms / 100;
	v->lados = lados;
	v->volumen = volumen;
	return 1;
}

void voz_parar(u32 id)
{
	int j;

	for (j = 0; j < MAX_VOCES; j++)
		if (voces[j].clase != V_LIBRE && voces[j].clase != V_CANAL && voces[j].id == id)
			suelta(&voces[j]);
}

void voces_de(u32 dueno)
{
	int j;

	for (j = 0; j < MAX_VOCES; j++)
		if (voces[j].clase == V_ARCHIVO && voces[j].dueno == dueno)
			suelta(&voces[j]);
}
