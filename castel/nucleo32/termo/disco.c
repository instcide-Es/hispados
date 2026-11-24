/* disco.c - (TERMO) los discos de dentro del equipo: su salud (SMART, el
 * registro de salud de NVMe, el desgaste del eMMC) y una prueba de lectura,
 * SOLO LEER (disco.h). Sin datos personales: del número de serie, las 4
 * últimas letras (ya las recorta el núcleo, y aquí otra vez).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifdef DI_ANFITRION
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#else
#include "juego.h"
#include <stdarg.h>
#endif
#include "disco.h"

/* ------------------------------------------------------------------ texto */

static void anade(char *d, int cabe, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
static void anade(char *d, int cabe, const char *fmt, ...)
{
	int n = (int)strlen(d);
	va_list v;

	if (n >= cabe - 1)
		return;
	va_start(v, fmt);
	vsnprintf(d + n, (size_t)(cabe - n), fmt, v);
	va_end(v);
}

/* una razón más: «; » entre ellas; y la salud, la peor */
static void razon(struct di_disco *d, int salud, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
static void razon(struct di_disco *d, int salud, const char *fmt, ...)
{
	int n = (int)strlen(d->porque);
	va_list v;

	if (salud != DI_NOSE && (d->salud == DI_NOSE || salud > d->salud))
		d->salud = salud;
	if (n && n < (int)sizeof d->porque - 3)
		strcpy(d->porque + n, "; "), n += 2;
	if (n >= (int)sizeof d->porque - 1)
		return;
	va_start(v, fmt);
	vsnprintf(d->porque + n, sizeof d->porque - (size_t)n, fmt, v);
	va_end(v);
}

/* un texto de ATA (palabras de 16 bits, cada par de letras al revés), sin
 * los espacios de los lados */
static void texto_ata(char *d, int cabe, const unsigned char *id, int palabra, int palabras)
{
	int i, n = 0, k;

	for (i = 0; i < palabras * 2 && n < cabe - 1; i++) {
		char c = (char)id[palabra * 2 + (i ^ 1)];

		d[n++] = (c >= 32 && c < 127) ? c : ' ';
	}
	d[n] = 0;
	while (n > 0 && d[n - 1] == ' ')
		d[--n] = 0;
	for (k = 0; d[k] == ' '; k++)
		;
	if (k)
		memmove(d, d + k, strlen(d + k) + 1);
}

static void texto_llano(char *d, int cabe, const unsigned char *o, int n0)
{
	int i, n = 0, k;

	for (i = 0; i < n0 && n < cabe - 1; i++)
		d[n++] = (o[i] >= 32 && o[i] < 127) ? (char)o[i] : ' ';
	d[n] = 0;
	while (n > 0 && d[n - 1] == ' ')
		d[--n] = 0;
	for (k = 0; d[k] == ' '; k++)
		;
	if (k)
		memmove(d, d + k, strlen(d + k) + 1);
}

/* del número de serie, solo las 4 últimas letras (las de antes, fuera) */
static void serie_corta(char *d, const char *s)
{
	int n = (int)strlen(s), i, k = 0;

	while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '*'))
		n--;
	i = n > 4 ? n - 4 : 0;
	for (; i < n && k < 4; i++)
		if (s[i] != '*' && s[i] != ' ')
			d[k++] = s[i];
	d[k] = 0;
}

static char *busca_en(char *t, const char *o)
{
	size_t n = strlen(o);

	for (; *t; t++)
		if (!strncmp(t, o, n))
			return t;
	return 0;
}

static unsigned long long le64(const unsigned char *p)
{
	unsigned long long v = 0;
	int i;

	for (i = 7; i >= 0; i--)
		v = (v << 8) | p[i];
	return v;
}

/* --------------------------------------------------------------- entender */

void di_empieza(struct di_disco *d, int clase, unsigned sectores, unsigned bloque, unsigned vida, const char *modelo)
{
	memset(d, 0, sizeof *d);
	d->clase = clase;
	d->sectores = sectores;
	d->bloque = bloque ? bloque : 512;
	d->vida = vida;
	d->estado = -1;
	d->errores_log = -1;
	d->temp = -1000;
	d->salud = DI_NOSE;
	d->ssd = clase == DI_NVME || clase == DI_EMMC;
	snprintf(d->modelo, sizeof d->modelo, "%s", modelo ? modelo : "");
}

void di_identify_ata(struct di_disco *d, const unsigned char *id)
{
	char s[24], m[48];
	unsigned w82 = id[164] | id[165] << 8, w85 = id[170] | id[171] << 8, w217 = id[434] | id[435] << 8;

	texto_ata(m, sizeof m, id, 27, 20);
	if (m[0])
		snprintf(d->modelo, sizeof d->modelo, "%s", m);
	texto_ata(s, sizeof s, id, 10, 10);
	serie_corta(d->serie, s);
	texto_ata(d->firmware, sizeof d->firmware, id, 23, 4);
	if (w217 == 1)
		d->ssd = 1;
	else if (w217 >= 0x401 && w217 < 0xFFFF)
		d->rpm = (int)w217;
	if (w82 == 0xFFFF || !(w82 & 1))
		d->smart = 0;
	else if (!(w85 & 1))
		d->smart = -1;
	else
		d->smart = -2;	/* (hasta que se lean los datos) */
}

void di_smart_ata(struct di_disco *d, const unsigned char *datos, const unsigned char *umbrales)
{
	int i, j;

	d->nat = 0;
	for (i = 0; i < DI_ATRIB; i++) {
		const unsigned char *a = datos + 2 + i * 12;
		struct di_atrib *x;

		if (!a[0])
			continue;
		x = &d->at[d->nat++];
		x->id = a[0];
		x->valor = a[3];
		x->peor = a[4];
		x->crudo = 0;
		for (j = 5; j >= 0; j--)
			x->crudo = (x->crudo << 8) | a[5 + j];
		x->umbral = -1;
		if (umbrales)
			for (j = 0; j < DI_ATRIB; j++)
				if (umbrales[2 + j * 12] == a[0]) {
					x->umbral = umbrales[2 + j * 12 + 1];
					break;
				}
	}
	d->smart = 1;
}

void di_errores_ata(struct di_disco *d, const unsigned char *reg)
{
	if (reg[0] == 0 && reg[1] == 0 && reg[452] == 0 && reg[453] == 0)
		d->errores_log = 0;
	else
		d->errores_log = reg[452] | reg[453] << 8;
}

void di_identify_nvme(struct di_disco *d, const unsigned char *id)
{
	char s[24], m[48];

	texto_llano(m, sizeof m, id + 24, 40);
	if (m[0])
		snprintf(d->modelo, sizeof d->modelo, "%s", m);
	texto_llano(s, sizeof s, id + 4, 20);
	serie_corta(d->serie, s);
	texto_llano(d->firmware, sizeof d->firmware, id + 64, 8);
}

void di_salud_nvme(struct di_disco *d, const unsigned char *r)
{
	unsigned k = r[1] | r[2] << 8;

	d->nvme = 1;
	d->critico = r[0];
	d->temp = k ? (int)k - 273 : -1000;
	d->repuesto = r[3];
	d->repuesto_umbral = r[4];
	d->usado = r[5];
	/* (las unidades de datos, de 1000 sectores de 512: 512.000 octetos) */
	d->leidos_gb = le64(r + 32) * 512000ull / 1000000000ull;
	d->escritos_gb = le64(r + 48) * 512000ull / 1000000000ull;
	d->encendidos = le64(r + 112);
	d->horas = le64(r + 128);
	d->apagados_mal = le64(r + 144);
	d->errores_medio = le64(r + 160);
	d->errores_reg = le64(r + 176);
}

const struct di_atrib *di_atributo(const struct di_disco *d, int id)
{
	int i;

	for (i = 0; i < d->nat; i++)
		if (d->at[i].id == id)
			return &d->at[i];
	return 0;
}

/* lo de dentro del crudo que vale (los fabricantes meten otras cosas en los
 * octetos altos: el 194 lleva la mínima y la máxima; el 9, a veces minutos) */
static unsigned long long crudo16(const struct di_atrib *a) { return a->crudo & 0xFFFF; }
static unsigned long long crudo32(const struct di_atrib *a) { return a->crudo & 0xFFFFFFFFull; }

static int cmp_u(const void *a, const void *b)
{
	unsigned x = *(const unsigned *)a, y = *(const unsigned *)b;

	return x < y ? -1 : x > y;
}

static void ordena(unsigned *v, int n)
{
	/* (sin qsort en la biblioteca de aquí: inserción, que son pocos miles) */
	int i, j;

	for (i = 1; i < n; i++) {
		unsigned x = v[i];

		for (j = i; j > 0 && cmp_u(&v[j - 1], &x) > 0; j--)
			v[j] = v[j - 1];
		v[j] = x;
	}
}

void di_lectura_cuenta(struct di_lectura *l, const unsigned *us, int n)
{
	static unsigned v[8192];
	unsigned long long suma = 0;
	int i;

	if (n > 8192)
		n = 8192;
	l->max_us = 0;
	for (i = 0; i < n; i++) {
		v[i] = us[i];
		suma += us[i];
		if (us[i] > l->max_us)
			l->max_us = us[i];
	}
	l->media_us = n ? (unsigned)(suma / (unsigned)n) : 0;
	ordena(v, n);
	l->p99_us = n ? v[(n * 99) / 100 < n ? (n * 99) / 100 : n - 1] : 0;
}

static const char *nombre_clase(int c)
{
	return c == DI_IDE ? "IDE" : c == DI_SATA ? "SATA" : c == DI_EMMC ? "eMMC" : c == DI_NVME ? "NVMe" : "?";
}

const char *di_salud_texto(int s)
{
	return s == DI_BIEN ? "BIEN" : s == DI_ATENCION ? "ATENCIÓN" : s == DI_MAL ? "MAL" : "NO SE SABE";
}

static int mb_s(int mb, unsigned us)
{
	/* (en décimas de MB/s) */
	return us ? (int)((unsigned long long)mb * 10000000ull / us) : 0;
}

/* lo que le queda de vida al SSD (%), de los atributos de desgaste (su valor
 * normalizado: 100 nuevo, baja con el uso); -1 si no lo dice */
static int vida_ssd(const struct di_disco *d, int *cual)
{
	static const int ids[] = { 231, 233, 177, 202, 169 };
	unsigned i;

	for (i = 0; i < sizeof ids / sizeof ids[0]; i++) {
		const struct di_atrib *a = di_atributo(d, ids[i]);

		if (a && a->valor > 0 && a->valor <= 100) {
			*cual = ids[i];
			return a->valor;
		}
	}
	return -1;
}

static const char *vida_emmc(int v)
{
	static const char *const t[] = { "no lo dice", "0-10 %", "10-20 %", "20-30 %", "30-40 %", "40-50 %", "50-60 %", "60-70 %", "70-80 %", "80-90 %", "90-100 %", "PASADA (más del 100 %)" };

	return v >= 0 && v <= 11 ? t[v] : "no lo dice";
}

void di_juzga(struct di_disco *d)
{
	const struct di_atrib *a;
	int v, cual = 0;

	d->salud = DI_NOSE;
	d->porque[0] = 0;
	/* SATA / IDE: SMART */
	if (d->clase == DI_SATA || d->clase == DI_IDE) {
		if (d->smart == 0)
			razon(d, DI_NOSE, "el disco no tiene SMART (no dice su salud): solo cuenta la prueba de lectura");
		else if (d->smart == -1)
			razon(d, DI_NOSE, "el disco tiene SMART apagado (no se enciende desde aquí: sería cambiar su configuración): solo cuenta la prueba de lectura");
		else if (d->smart == -2)
			razon(d, DI_NOSE, "el disco no ha dado sus datos SMART (la controladora o el disco no los deja leer)");
		if (d->estado == 1)
			razon(d, DI_MAL, "el propio disco dice que está fallando (SMART: algún valor ha pasado su umbral)");
		if (d->smart == 1) {
			int i, pasado = 0;

			for (i = 0; i < d->nat; i++)
				if (d->at[i].umbral > 0 && d->at[i].umbral < 0xFE && d->at[i].valor <= d->at[i].umbral && d->at[i].valor > 0)
					pasado = d->at[i].id;
			if (pasado && d->estado != 1)
				razon(d, DI_MAL, "el atributo %d de SMART ha llegado a su umbral (el disco lo da por gastado)", pasado);
			if ((a = di_atributo(d, 5)) && crudo32(a)) {
				unsigned long long n = crudo32(a);

				razon(d, n >= 50 ? DI_MAL : DI_ATENCION, "tiene %llu sectores reasignados (estropeados y cambiados por otros de reserva)%s", n,
				      n >= 50 ? ": son muchos, el disco se está estropeando" : ": vigílelo; si suben, cambie el disco");
			}
			if ((a = di_atributo(d, 197)) && crudo16(a)) {
				unsigned long long n = crudo16(a);

				razon(d, n >= 10 ? DI_MAL : DI_ATENCION, "tiene %llu %s pendientes (que no se dejan leer: el disco se queda reintentando y el equipo se atasca)", n, n == 1 ? "sector" : "sectores");
			}
			if ((a = di_atributo(d, 198)) && crudo16(a)) {
				unsigned long long n = crudo16(a);

				razon(d, n >= 10 ? DI_MAL : DI_ATENCION, "tiene %llu %s que no se pueden corregir (datos perdidos en ellos)", n, n == 1 ? "sector" : "sectores");
			}
			if ((a = di_atributo(d, 187)) && crudo16(a))
				razon(d, DI_ATENCION, "ha dado %llu errores de lectura que no ha podido corregir", crudo16(a));
			if ((a = di_atributo(d, 188)) && crudo16(a) >= 5)
				razon(d, DI_ATENCION, "se le han pasado %llu órdenes de tiempo (no contestaba)", crudo16(a));
			if ((a = di_atributo(d, 199)) && crudo16(a))
				razon(d, crudo16(a) >= 50 ? DI_ATENCION : DI_BIEN, "lleva %llu errores de transmisión (CRC): es el cable o el conector del disco, no el disco%s", crudo16(a),
				      crudo16(a) >= 50 ? "; mire que el disco esté bien encajado" : " (pocos: suele ser de un mal contacto de antes)");
			if ((a = di_atributo(d, 194)) && (a->crudo & 0xFF) > 0 && (a->crudo & 0xFF) < 120) {
				int t = (int)(a->crudo & 0xFF);

				if (t >= 60)
					razon(d, t >= 70 ? DI_MAL : DI_ATENCION, "está a %d °C (mucho para un disco: más de 55-60 °C lo gasta antes)", t);
			}
			if (d->ssd && (v = vida_ssd(d, &cual)) >= 0) {
				if (v <= 10)
					razon(d, DI_MAL, "el SSD está gastado: le queda un %d %% de vida (atributo %d)", v, cual);
				else if (v <= 30)
					razon(d, DI_ATENCION, "el SSD está bastante gastado: le queda un %d %% de vida (atributo %d)", v, cual);
			}
			if (d->errores_log > 0)
				razon(d, d->errores_log >= 10 ? DI_ATENCION : DI_BIEN, "el disco ha apuntado %d %s en su vida (su registro de errores)", d->errores_log, d->errores_log == 1 ? "error" : "errores");
			if (d->salud == DI_NOSE)
				d->salud = DI_BIEN;
		}
	}
	/* NVMe */
	if (d->clase == DI_NVME) {
		if (!d->nvme)
			razon(d, DI_NOSE, "el disco no ha dado su registro de salud (SMART/Health)");
		else {
			if (d->critico & 0x04)
				razon(d, DI_MAL, "el propio disco avisa de que ya no es fiable (aviso crítico: errores del medio o de dentro)");
			if (d->critico & 0x08)
				razon(d, DI_MAL, "el disco se ha puesto en solo lectura (lo hace él solo cuando se está muriendo)");
			if ((d->critico & 0x01) || (d->repuesto_umbral && d->repuesto < d->repuesto_umbral))
				razon(d, DI_MAL, "le queda poca reserva (%d %%, el mínimo es %d %%)", d->repuesto, d->repuesto_umbral);
			if (d->critico & 0x10)
				razon(d, DI_ATENCION, "avisa de que la copia de seguridad de su memoria rápida falla");
			if (d->critico & 0x02)
				razon(d, DI_ATENCION, "avisa de temperatura: ha pasado su límite");
			if (d->usado >= 100)
				razon(d, DI_MAL, "está gastado: ha usado el %d %% de la vida que da el fabricante", d->usado);
			else if (d->usado >= 80)
				razon(d, DI_ATENCION, "ha usado el %d %% de la vida que da el fabricante", d->usado);
			if (d->errores_medio)
				razon(d, d->errores_medio >= 10 ? DI_MAL : DI_ATENCION, "ha tenido %llu errores del medio (datos que no ha podido leer bien)", d->errores_medio);
			if (d->temp >= 70 && d->temp < 200)
				razon(d, DI_ATENCION, "está a %d °C (mucho: se frenará solo para enfriarse)", d->temp);
			if (d->salud == DI_NOSE)
				d->salud = DI_BIEN;
		}
	}
	/* eMMC */
	if (d->clase == DI_EMMC) {
		if (!(d->vida & 0x80000000u) || ((d->vida >> 24) & 0x7F) < 7)
			razon(d, DI_NOSE, "el eMMC no dice su desgaste (es anterior a la norma 5.0, o no se ha podido leer)");
		else {
			int pre = (int)(d->vida >> 16) & 0xFF, ta = (int)d->vida & 0xFF, tb = (int)(d->vida >> 8) & 0xFF, m = ta > tb ? ta : tb;

			if (pre == 3)
				razon(d, DI_MAL, "el eMMC avisa de que le queda muy poca reserva (PRE_EOL urgente)");
			else if (pre == 2)
				razon(d, DI_ATENCION, "el eMMC avisa de que ha gastado el 80 %% de su reserva (PRE_EOL)");
			if (m >= 11)
				razon(d, DI_MAL, "el eMMC ha pasado la vida que da el fabricante");
			else if (m >= 9)
				razon(d, DI_ATENCION, "el eMMC ha gastado el %s de su vida", vida_emmc(m));
			if (d->salud == DI_NOSE)
				d->salud = DI_BIEN;
		}
	}
	/* la prueba de lectura: manda sobre lo que diga el disco */
	if (d->lee.hecha) {
		if (d->lee.errores)
			razon(d, DI_MAL, "en la prueba de lectura, %d %s no se han podido leer (ni reintentando)", d->lee.errores, d->lee.errores == 1 ? "trozo" : "trozos");
		if (d->lee.lentas)
			razon(d, d->lee.lentas >= 3 ? DI_MAL : DI_ATENCION, "en la prueba de lectura se ha atascado %d %s (más de medio segundo para leer 64 K; la peor, %u ms)", d->lee.lentas,
			      d->lee.lentas == 1 ? "vez" : "veces", d->lee.max_us / 1000);
		if (d->lee.reintentos)
			razon(d, DI_ATENCION, "en la prueba de lectura ha hecho falta reintentar %d %s", d->lee.reintentos, d->lee.reintentos == 1 ? "vez" : "veces");
		if (!d->lee.errores && !d->lee.lentas && !d->lee.reintentos && d->salud == DI_NOSE)
			d->salud = DI_BIEN;
	}
	if (!d->porque[0] && d->salud == DI_BIEN)
		razon(d, DI_BIEN, "nada raro: %s", !d->lee.hecha ? "lo que dice el disco de sí está bien" : d->lee.cortada ? "lo leído, sin errores ni atascos (no todo: se acabó el minuto)" : "se lee entero sin errores ni atascos");
	if (!d->porque[0])
		razon(d, DI_NOSE, "no se ha podido mirar");
}

/* --------------------------------------------------------------- escribir */

static const char *d1(char *b, int decimas)
{
	snprintf(b, 16, "%d,%d", decimas / 10, decimas % 10);
	return b;
}

static void tamano(char *b, int cabe, unsigned sectores)
{
	unsigned gb = sectores / 2097152u;

	if (sectores == 0xFFFFFFFFu)
		snprintf(b, (size_t)cabe, "2 TB o más (se mira hasta 2 TB)");
	else if (gb >= 10)
		snprintf(b, (size_t)cabe, "%u GB", (unsigned)((unsigned long long)sectores * 512ull / 1000000000ull));
	else
		snprintf(b, (size_t)cabe, "%u MB", (unsigned)((unsigned long long)sectores * 512ull / 1000000ull));
}

int di_informe(const struct di_disco *d, int n, int escritos, char *t, int cabe)
{
	int i;

	t[0] = 0;
	anade(t, cabe, "\r\nDISCOS (solo se leen: en los discos del equipo no se escribe nada)\r\n");
	if (n == 0)
		anade(t, cabe, "  No se ha visto ningún disco de dentro (SATA, IDE, NVMe o eMMC). Si lo tiene, puede ser una controladora que Hispa-Pentium no lleva (RAID de Intel, «RST»: en la BIOS, ponga el disco en «AHCI»).\r\n");
	for (i = 0; i < n; i++) {
		const struct di_disco *x = &d[i];
		const struct di_atrib *a;
		char tm[48], b1[16], b2[16];

		tamano(tm, sizeof tm, x->sectores);
		anade(t, cabe, "  Disco %d: %s (%s%s, %s)", i + 1, x->modelo[0] ? x->modelo : "sin nombre", nombre_clase(x->clase), x->ssd ? ", SSD" : x->rpm ? "" : "", tm);
		if (x->rpm)
			anade(t, cabe, ", %d r.p.m.", x->rpm);
		if (x->serie[0])
			anade(t, cabe, ", serie ...%s", x->serie);
		if (x->firmware[0])
			anade(t, cabe, ", firmware %s", x->firmware);
		anade(t, cabe, "\r\n    Salud: %s. %c%s.\r\n", di_salud_texto(x->salud), x->porque[0] >= 'a' && x->porque[0] <= 'z' ? x->porque[0] - 32 : x->porque[0], x->porque[0] ? x->porque + 1 : "");
		/* lo principal, en una línea */
		if (x->smart == 1) {
			anade(t, cabe, "    SMART:");
			if ((a = di_atributo(x, 9)))
				anade(t, cabe, " %llu horas encendido;", crudo32(a));
			if ((a = di_atributo(x, 12)))
				anade(t, cabe, " %llu encendidos;", crudo32(a));
			if ((a = di_atributo(x, 194)) && (a->crudo & 0xFF) && (a->crudo & 0xFF) < 120)
				anade(t, cabe, " %d °C;", (int)(a->crudo & 0xFF));
			{
				const struct di_atrib *r5 = di_atributo(x, 5), *r197 = di_atributo(x, 197), *r198 = di_atributo(x, 198), *r199 = di_atributo(x, 199);

				anade(t, cabe, " reasignados %llu, pendientes %llu, incorregibles %llu, CRC del cable %llu", r5 ? crudo32(r5) : 0ull, r197 ? crudo16(r197) : 0ull,
				      r198 ? crudo16(r198) : 0ull, r199 ? crudo16(r199) : 0ull);
			}
			if (x->ssd) {
				int c = 0, v = vida_ssd(x, &c);

				if (v >= 0)
					anade(t, cabe, "; vida que le queda %d %%", v);
			}
			if (x->estado >= 0)
				anade(t, cabe, "; el disco dice de sí: %s", x->estado == 0 ? "bien" : x->estado == 1 ? "FALLANDO" : "no lo dice");
			anade(t, cabe, ".\r\n");
		}
		if (x->nvme)
			anade(t, cabe, "    Salud NVMe: %d °C; usado %d %% de su vida; reserva %d %% (mínimo %d %%); %llu horas encendido; %llu encendidos; %llu apagados sin avisar; %llu errores del medio; aviso crítico %02Xh%s; leídos %llu GB, escritos %llu GB.\r\n",
			      x->temp > -1000 ? x->temp : 0, x->usado, x->repuesto, x->repuesto_umbral, x->horas, x->encendidos, x->apagados_mal, x->errores_medio, x->critico,
			      x->critico ? "" : " (ninguno)", x->leidos_gb, x->escritos_gb);
		if (x->clase == DI_EMMC && (x->vida & 0x80000000u) && ((x->vida >> 24) & 0x7F) >= 7)
			anade(t, cabe, "    Desgaste del eMMC: %s (tipo A), %s (tipo B); reserva: %s.\r\n", vida_emmc((int)(x->vida & 0xFF)), vida_emmc((int)(x->vida >> 8) & 0xFF),
			      ((x->vida >> 16) & 0xFF) == 1 ? "normal" : ((x->vida >> 16) & 0xFF) == 2 ? "AVISO (80 % gastada)" : ((x->vida >> 16) & 0xFF) == 3 ? "URGENTE" : "no lo dice");
		if (x->lee.hecha) {
			anade(t, cabe, "    Prueba de lectura: %d MB repartidos por todo el disco a %s MB/s y %d MB seguidos a %s MB/s; cada lectura de 64 K: media %u ms, p99 %u ms, la peor %u ms; %d errores, %d reintentos, %d atascos (más de 0,5 s)%s%s.\r\n",
			      x->lee.mb_saltos, d1(b1, mb_s(x->lee.mb_saltos, x->lee.us_saltos)), x->lee.mb_seguido, d1(b2, mb_s(x->lee.mb_seguido, x->lee.us_seguido)), (x->lee.media_us + 500) / 1000,
			      (x->lee.p99_us + 500) / 1000, (x->lee.max_us + 500) / 1000, x->lee.errores, x->lee.reintentos, x->lee.lentas,
			      x->lee.cortada ? "; cortada al minuto (el disco iba lento: no se ha leído todo)" : "", x->lee.con_carga ? "; en pausa mientras duraba la prueba de esfuerzo (lo esperado no cuenta)" : "");
			if (x->lee.errores) {
				int k;

				anade(t, cabe, "    Los errores, hacia el MB:");
				for (k = 0; k < x->lee.errores && k < 4; k++)
					anade(t, cabe, " %u", x->lee.err_mb[k]);
				anade(t, cabe, " (de %u).\r\n", (unsigned)((unsigned long long)x->sectores * 512ull / 1048576ull));
			}
		} else
			anade(t, cabe, "    Prueba de lectura: no hecha.\r\n");
	}
	if (escritos >= 0)
		anade(t, cabe, "  Sectores escritos en los discos de dentro desde que arrancó la memoria: %d%s.\r\n", escritos, escritos ? " (¡NO debería haber ninguno!)" : " (como debe ser)");
	return (int)strlen(t);
}

int di_detalle(const struct di_disco *d, char *t, int cabe)
{
	int i;

	t[0] = 0;
	anade(t, cabe, "DISCO %s «%s» serie ...%s firmware %s sectores %u bloque %u ssd %d smart %d estado %d errores_log %d\r\n", nombre_clase(d->clase), d->modelo, d->serie,
	      d->firmware, d->sectores, d->bloque, d->ssd, d->smart, d->estado, d->errores_log);
	for (i = 0; i < d->nat; i++)
		anade(t, cabe, "  ATRIB %3d valor %3d peor %3d umbral %3d crudo %llu\r\n", d->at[i].id, d->at[i].valor, d->at[i].peor, d->at[i].umbral, d->at[i].crudo);
	if (d->nvme)
		anade(t, cabe, "  NVME critico=%02X temp=%d repuesto=%d/%d usado=%d horas=%llu encendidos=%llu apagados_mal=%llu errores_medio=%llu errores_reg=%llu\r\n", d->critico, d->temp,
		      d->repuesto, d->repuesto_umbral, d->usado, d->horas, d->encendidos, d->apagados_mal, d->errores_medio, d->errores_reg);
	if (d->clase == DI_EMMC)
		anade(t, cabe, "  EMMC vida=%08X\r\n", d->vida);
	if (d->lee.hecha)
		anade(t, cabe, "  LECTURA lecturas=%d saltos=%dMB/%uus seguido=%dMB/%uus media=%uus p99=%uus max=%uus errores=%d reintentos=%d lentas=%d cortada=%d con_carga=%d\r\n",
		      d->lee.lecturas, d->lee.mb_saltos, d->lee.us_saltos, d->lee.mb_seguido, d->lee.us_seguido, d->lee.media_us, d->lee.p99_us, d->lee.max_us, d->lee.errores,
		      d->lee.reintentos, d->lee.lentas, d->lee.cortada, d->lee.con_carga);
	anade(t, cabe, "  SALUD %s: %s\r\n", di_salud_texto(d->salud), d->porque);
	return (int)strlen(t);
}

int di_veredicto(const struct di_disco *d, int n, int parones, char *t, int cabe)
{
	int i, peor = -1;

	t[0] = 0;
	for (i = 0; i < n; i++)
		if (d[i].salud == DI_MAL || d[i].salud == DI_ATENCION)
			if (peor < 0 || d[i].salud > d[peor].salud)
				peor = i;
	if (peor < 0)
		return 0;
	{
		const struct di_disco *x = &d[peor];
		const struct di_atrib *p = di_atributo(x, 197);
		int atasca = (p && crudo16(p)) || x->lee.lentas || x->lee.errores || x->lee.reintentos;

		if (p && crudo16(p))
			snprintf(t, (size_t)cabe, "DISCO %s: el disco %d (%s) tiene %llu %s pendientes%s", x->salud == DI_MAL ? "MAL" : "CON AVISOS", peor + 1, x->modelo,
				 crudo16(p), crudo16(p) == 1 ? "sector" : "sectores", parones ? ": puede ser la causa de los parones" : ": puede dar parones y perder datos");
		else
		{
			char pq[700];
			char *c;

			snprintf(pq, sizeof pq, "%s", x->porque);
			if ((c = busca_en(pq, "; ")))
				*c = 0;
			snprintf(t, (size_t)cabe, "DISCO %s: el disco %d (%s): %s%s", x->salud == DI_MAL ? "MAL" : "CON AVISOS", peor + 1, x->modelo, pq,
				 parones && atasca ? ": puede ser la causa de los parones" : "");
		}
		anade(t, cabe, ". %s", x->salud == DI_MAL ? "Haga copia de los datos cuanto antes y cambie el disco." : "Conviene hacer copia de los datos y vigilarlo.");
	}
	return (int)strlen(t);
}

#ifndef DI_ANFITRION
/* ------------------------------------------------------------ TERMO.BIN */

#define SC_TDISCO 238

struct ficha_td {
	u32 clase, sectores, bloque, vida;
	char modelo[48];
};
struct pide_salud { u32 que, destino, cabe; };
struct pide_lee { u32 lba, n, fallos; };

static char ruta_est[120];
static u64 tsc_hz;
static unsigned char id_buf[4096], dat_buf[512], umb_buf[512];
static unsigned lat[8192];
static struct di_disco D[DI_MAX];
static int nD;

static void estado(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void estado(const char *fmt, ...)
{
	char t[300];
	va_list v;
	u32 m;

	va_start(v, fmt);
	vsnprintf(t, sizeof t, fmt, v);
	va_end(v);
	serie("termo: discos: %s\n", t);
	m = sc(SC_ABRIR, (u32)ruta_est, PARA_CREAR, 0);
	if (!ES_ERROR(m)) {
		sc(SC_ESCRIBIR_ARCHIVO, m, (u32)t, (u32)strlen(t));
		sc(SC_CERRAR, m, 0, 0);
	}
}

static void guarda(const char *carpeta, const char *nombre, const char *t, int n)
{
	char r[120];
	u32 m;

	snprintf(r, sizeof r, "%s\\%s", carpeta, nombre);
	m = sc(SC_ABRIR, (u32)r, PARA_CREAR, 0);
	if (ES_ERROR(m))
		return;
	sc(SC_ESCRIBIR_ARCHIVO, m, (u32)t, (u32)n);
	sc(SC_CERRAR, m, 0, 0);
}

static u32 pide(u32 k, u32 que, void *b, u32 cabe)
{
	struct pide_salud p = { que, (u32)b, cabe };

	return sc(SC_TDISCO, 1, k, (u32)&p);
}

static unsigned us_de(u64 ciclos)
{
	return tsc_hz ? (unsigned)(ciclos * 1000000ull / tsc_hz) : 0;
}

/* una lectura de n sectores de 512 desde lba: su tiempo (us), y si va mal,
 * otra vez (reintento); err: 1 si ni así */
static unsigned lee_una(int k, u32 lba, u32 n, struct di_lectura *l, int *err)
{
	struct pide_lee p = { lba, n, 0 };
	u64 c0 = rdtsc();
	u32 r = sc(SC_TDISCO, 2, (u32)k, (u32)&p);
	unsigned us;

	us = us_de(rdtsc() - c0);
	l->lecturas++;
	l->reintentos += (int)p.fallos;
	*err = 0;
	if (r != 0) {
		p.fallos = 0;
		c0 = rdtsc();
		r = sc(SC_TDISCO, 2, (u32)k, (u32)&p);
		us += us_de(rdtsc() - c0);
		if (r == 0)
			l->reintentos++;
		else {
			if (l->errores < 4)
				l->err_mb[l->errores] = lba / 2048;
			l->errores++;
			*err = 1;
		}
	}
	if (us > 500000)
		l->lentas++;
	return us;
}

/* con la prueba de esfuerzo en marcha (el Termómetro deja TERMO\DISCOS.PAU),
 * la lectura espera a que acabe: así ni la carga ni lo medido salen
 * falseados. Da los tics que ha esperado (para correr el tope) */
static char ruta_pau[120];
static u32 espera_carga(struct di_lectura *l)
{
	u32 t0 = jg_tics(), m;
	int dicho = 0;

	for (;;) {
		m = sc(SC_ABRIR, (u32)ruta_pau, PARA_LEER, 0);
		if (ES_ERROR(m))
			break;
		sc(SC_CERRAR, m, 0, 0);
		if (!dicho)
			estado("en pausa mientras dura la prueba de esfuerzo (para no falsear ni la una ni la otra)..."), dicho = 1;
		l->con_carga = 1;	/* (ha esperado: se dice) */
		if (jg_tics() - t0 > 40 * 60 * jg_hz)
			break;
		sc(SC_DORMIR, jg_hz, 0, 0);
	}
	return jg_tics() - t0;
}

/* la prueba de lectura del disco k: 256 trozos de 1 MB repartidos por todo
 * el disco y 256 MB seguidos desde el principio; hasta 60 s */
static void prueba_lectura(int k, struct di_disco *d, u32 t_tope)
{
	struct di_lectura *l = &d->lee;
	u32 total = d->sectores, trozos = 256, i, j, nl = 0, seg, t0, pausado = 0;
	int err;
	u64 c0;

	memset(l, 0, sizeof *l);
	l->hecha = 1;
	if (total < 4096) {
		l->hecha = 0;
		return;
	}
	/* 1. a saltos: el trozo i en (total - 2048) * i / 255, en MB enteros */
	t0 = jg_tics();
	c0 = rdtsc();
	for (i = 0; i < trozos; i++) {
		u32 lba = (u32)(((u64)(total - 2048) * i / (trozos - 1)) & ~2047ull);

		for (j = 0; j < 16; j++) {
			lat[nl < 8192 ? nl : 8191] = lee_una(k, lba + j * 128, 128, l, &err);
			nl++;
			if (err)
				break;	/* (el resto del MB, no: que no se quede atascado en lo malo) */
		}
		l->mb_saltos++;
		if (i % 8 == 7) {
			u32 e = espera_carga(l);

			t_tope += e, pausado += e;
			if (e)
				c0 += (u64)e * tsc_hz / jg_hz;	/* (lo esperado no cuenta en los MB/s) */
		}
		/* (a saltos, como mucho dos tercios del tiempo: lo demás, para leer seguido) */
		if (jg_tics() - t0 - pausado > (t_tope - pausado - t0) * 2 / 3) {
			l->cortada = 1;
			break;
		}
		if (i % 32 == 31)
			estado("disco %d: leyendo a saltos, %u de %u MB...", k + 1, i + 1, trozos);
	}
	l->us_saltos = us_de(rdtsc() - c0);
	/* 2. seguido: 256 MB (o lo que haya) desde el principio */
	seg = total / 2048 < 256 ? total / 2048 : 256;
	c0 = rdtsc();
	for (i = 0; i < seg && jg_tics() <= t_tope; i++) {
		for (j = 0; j < 16; j++) {
			lat[nl < 8192 ? nl : 8191] = lee_una(k, i * 2048 + j * 128, 128, l, &err);
			nl++;
			if (err)
				break;
		}
		l->mb_seguido++;
		if (i % 8 == 7) {
			u32 e = espera_carga(l);

			t_tope += e;
			if (e)
				c0 += (u64)e * tsc_hz / jg_hz;
		}
		if (jg_tics() > t_tope)
			l->cortada = 1;
		if (i % 64 == 63)
			estado("disco %d: leyendo seguido, %u de %u MB...", k + 1, i + 1, seg);
	}
	l->us_seguido = us_de(rdtsc() - c0);
	di_lectura_cuenta(l, lat, (int)(nl < 8192 ? nl : 8192));
}

static void mide_tsc_aqui(void)
{
	u32 t0 = jg_tics(), t1;
	u64 r0;

	while (jg_tics() == t0)
		;
	t0 = jg_tics();
	r0 = rdtsc();
	while ((t1 = jg_tics()) - t0 < 20)
		;
	tsc_hz = (rdtsc() - r0) * jg_hz / (t1 - t0);
}

int disco_principal(const char *carpeta)
{
	static char t[12000], v[700], v2[700], lg[16000];
	struct ficha_td f;
	int k, n, escritos;
	u32 r;

	snprintf(ruta_est, sizeof ruta_est, "%s\\DISCOS.EST", carpeta);
	snprintf(ruta_pau, sizeof ruta_pau, "%s\\DISCOS.PAU", carpeta);
	estado("Mirando los discos de dentro (solo leer)...");
	mide_tsc_aqui();
	nD = 0;
	lg[0] = 0;
	for (k = 0; k < DI_MAX; k++) {
		memset(&f, 0, sizeof f);
		r = sc(SC_TDISCO, 0, (u32)k, (u32)&f);
		if (ES_ERROR(r)) {
			estado("Hecho: este núcleo no sabe mirar los discos (la llamada 238 es de la 0.8.10).");
			guarda(carpeta, "DISCOS.TXT", "", 0);
			return 1;
		}
		if (r != 1)
			break;
		f.modelo[47] = 0;
		di_empieza(&D[nD], (int)f.clase, f.sectores, f.bloque, f.vida, f.modelo);
		nD++;
	}
	for (k = 0; k < nD; k++) {
		struct di_disco *d = &D[k];
		u32 t_tope = jg_tics() + 60 * jg_hz;

		estado("disco %d de %d (%s): su salud...", k + 1, nD, d->modelo);
		if (d->clase == DI_SATA || d->clase == DI_IDE) {
			if (pide((u32)k, 1, id_buf, 512) == 0)
				di_identify_ata(d, id_buf);
			if (d->smart != 0 && d->smart != -1) {
				if (pide((u32)k, 2, dat_buf, 512) == 0) {
					int hay_umb = pide((u32)k, 3, umb_buf, 512) == 0;

					di_smart_ata(d, dat_buf, hay_umb ? umb_buf : 0);
				}
				r = pide((u32)k, 4, 0, 0);
				d->estado = r <= 2 ? (int)r : -1;
				if (d->smart == 1 && pide((u32)k, 5, dat_buf, 512) == 0)
					di_errores_ata(d, dat_buf);
			}
		} else if (d->clase == DI_NVME) {
			if (pide((u32)k, 1, id_buf, 4096) == 0)
				di_identify_nvme(d, id_buf);
			if (pide((u32)k, 6, dat_buf, 512) == 0)
				di_salud_nvme(d, dat_buf);
		}
		estado("disco %d de %d (%s): prueba de lectura (hasta 1 minuto)...", k + 1, nD, d->modelo);
		prueba_lectura(k, d, t_tope);
		di_juzga(d);
		di_detalle(d, lg + strlen(lg), (int)(sizeof lg - strlen(lg)));
	}
	escritos = (int)sc(SC_TDISCO, 4, 0, 0);
	if (escritos < 0)
		escritos = -1;
	n = di_informe(D, nD, escritos, t, sizeof t);
	guarda(carpeta, "DISCOS.TXT", t, n);
	{
		char inf[1600];
		u32 m = sc(SC_TDISCO, 3, sizeof inf, (u32)inf);

		if (!ES_ERROR(m) && inf[0])
			snprintf(lg + strlen(lg), sizeof lg - strlen(lg), "%s", inf);
		anade(lg, sizeof lg, "ESCRITOS_DENTRO %d\r\n", escritos);
		guarda(carpeta, "DISCOS.LOG", lg, (int)strlen(lg));
	}
	di_veredicto(D, nD, 0, v, sizeof v);
	di_veredicto(D, nD, 1, v2, sizeof v2);
	{
		char b[1500];
		int m = snprintf(b, sizeof b, "%s\r\n%s\r\n", v, v2);

		guarda(carpeta, "DISCOS.VER", b, m);
	}
	{
		char res[240];
		int p = 0;

		res[0] = 0;
		for (k = 0; k < nD; k++)
			p += snprintf(res + p, sizeof res - (size_t)p, "%s%d %s", k ? ", " : "", k + 1, di_salud_texto(D[k].salud));
		if (nD)
			estado("Hecho: %d %s: %s (en INFORME.TXT, «Discos»).", nD, nD == 1 ? "disco" : "discos", res);
		else
			estado("Hecho: ningún disco de dentro (SATA, IDE, NVMe o eMMC).");
	}
	return 0;
}

/* --------------------------------------------------- la tarea principal */

static int lanzado, acabado;
static char ultimo[300];

void disco_lanza(const char *carpeta)
{
	char r[120];
	u32 x, m;

	if (!carpeta[0] || lanzado)
		return;
	snprintf(r, sizeof r, "%s\\DISCOS.EST", carpeta);
	m = sc(SC_ABRIR, (u32)r, PARA_CREAR, 0);
	if (!ES_ERROR(m)) {
		sc(SC_ESCRIBIR_ARCHIVO, m, (u32)"Empezando...", 12);
		sc(SC_CERRAR, m, 0, 0);
	}
	snprintf(r, sizeof r, "%s\\DISCOS.TXT", carpeta);
	sc(SC_BORRAR, (u32)r, 0, 0);
	snprintf(r, sizeof r, "%s\\DISCOS.VER", carpeta);
	sc(SC_BORRAR, (u32)r, 0, 0);
	snprintf(r, sizeof r, "%s\\DISCOS.PAU", carpeta);
	sc(SC_BORRAR, (u32)r, 0, 0);
	x = sc(SC_LANZAR_SUELTO, (u32)"\\TERMO.BIN", (u32)"/DISCO", 0);
	if (ES_ERROR(x))
		x = sc(SC_LANZAR_SUELTO, (u32)"\\HISPA\\TERMO.BIN", (u32)"/DISCO", 0);
	lanzado = ES_ERROR(x) ? -1 : 1;
}

static int lee_archivo(const char *carpeta, const char *nombre, char *t, int cabe)
{
	char r[120];
	u32 m, n;

	t[0] = 0;
	snprintf(r, sizeof r, "%s\\%s", carpeta, nombre);
	m = sc(SC_ABRIR, (u32)r, PARA_LEER, 0);
	if (ES_ERROR(m))
		return -1;
	n = sc(SC_LEER, m, (u32)t, (u32)cabe - 1);
	sc(SC_CERRAR, m, 0, 0);
	if (ES_ERROR(n))
		return -1;
	t[n] = 0;
	return (int)n;
}

void disco_pausa(const char *carpeta, int si)
{
	char r[120];
	u32 m;

	if (!carpeta[0] || lanzado != 1)
		return;
	snprintf(r, sizeof r, "%s\\DISCOS.PAU", carpeta);
	if (!si) {
		sc(SC_BORRAR, (u32)r, 0, 0);
		return;
	}
	m = sc(SC_ABRIR, (u32)r, PARA_CREAR, 0);
	if (!ES_ERROR(m)) {
		sc(SC_ESCRIBIR_ARCHIVO, m, (u32)"pausa", 5);
		sc(SC_CERRAR, m, 0, 0);
	}
}

int disco_acabado(void)
{
	return acabado;
}

int disco_mira(const char *carpeta, char *aviso, int cabe)
{
	char t[300];

	if (lanzado != 1 || acabado || lee_archivo(carpeta, "DISCOS.EST", t, sizeof t) <= 0)
		return 0;
	if (!strcmp(t, ultimo))
		return 0;
	snprintf(ultimo, sizeof ultimo, "%s", t);
	if (!strncmp(t, "Hecho", 5))
		acabado = 1;
	snprintf(aviso, (size_t)cabe, "Discos: %s", t);
	return 1;
}

int disco_informe(const char *carpeta, char *t, int cabe)
{
	int n = lee_archivo(carpeta, "DISCOS.TXT", t, cabe);

	if (n > 0)
		return n;
	if (lanzado == 1 && !acabado) {
		snprintf(t, (size_t)cabe, "\r\nDISCOS\r\n  La prueba de los discos no había acabado al escribir el informe (%s).\r\n", ultimo[0] ? ultimo : "empezando");
		return (int)strlen(t);
	}
	t[0] = 0;
	return 0;
}

void disco_veredicto(const char *carpeta, char *resumen, int cabe, int parones)
{
	char t[1500], *l2;
	int n = (int)strlen(resumen);

	if (busca_en(resumen, "DISCO MAL") || busca_en(resumen, "DISCO CON AVISOS"))
		return;	/* (ya está) */
	if (lee_archivo(carpeta, "DISCOS.VER", t, sizeof t) <= 0)
		return;
	l2 = busca_en(t, "\r\n");
	if (!l2)
		return;
	*l2 = 0;
	l2 += 2;
	if (parones) {
		char *f = busca_en(l2, "\r\n");

		if (f)
			*f = 0;
	}
	if (!(parones ? l2 : t)[0])
		return;
	snprintf(resumen + n, (size_t)(cabe - n), "%s%s", n ? " " : "", parones ? l2 : t);
}
#endif
