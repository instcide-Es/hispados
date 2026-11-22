/* termo.c - (TERMO) el Termómetro de Hispa-Pentium (TERMO.BIN): las
 * temperaturas del equipo, para ver si un portátil se calienta (y si hay que
 * limpiarle el ventilador o cambiarle la pasta térmica).
 *
 *  - El procesador: Intel (CPUID 6: el sensor digital, DTS; IA32_THERM_STATUS
 *    19Ch del núcleo que corre y IA32_PACKAGE_THERM_STATUS 1B1h de todo el
 *    procesador; la TjMax de MSR_TEMPERATURE_TARGET 1A2h o, si no lo tiene,
 *    de una tabla) y AMD (K8: PCI 0:18.3 E4h; familias 10h a 16h: 0:18.3 A4h;
 *    15h modelos 60h+ y 16h 30h+: SMN D8200C64h; 17h en adelante: SMN
 *    59800h). Lo lee el núcleo (las llamadas 220 a 222, termo.cas): solo
 *    lectura y solo esos registros.
 *  - ACPI: las zonas térmicas (_TMP, _CRT, _PSV, _HOT) y los ventiladores
 *    (_FST), que lee el servicio de ACPI (ACPI.BIN) y da el núcleo (la 223);
 *    la batería (la 154); el fabricante y el modelo del equipo (SMBIOS).
 *  - Una tabla grande (nombre, ahora, mínima, máxima, límite y una barra con
 *    su estado escrito: «bien», «templado», «caliente», «peligro»; nunca solo
 *    el color), la gráfica de los últimos 10 minutos (cada sensor con su
 *    trazo y su marca), la frecuencia (APERF/MPERF, o la del TSC).
 *  - «Calentar»: el procesador al 100 % (un programa hijo, TERMO.BIN /CARGA,
 *    con cuentas de coma flotante; Hispa-Pentium usa un solo núcleo: se dice)
 *    5, 10 o 15 minutos; se para sola si un sensor llega a su límite menos
 *    10 °C (o a 95 °C si no tiene límite), o con Esc. Luego mide cómo
 *    se enfría y dice qué le parece.
 *  - El registro: cada 2 s una línea en X:\TERMO\MMDDHHMM.CSV (la unidad del
 *    sistema: la memoria USB), el resumen de cada prueba en .TXT y, con F12,
 *    la pantalla en .BMP. En el disco del equipo no se escribe nunca.
 * Para las pruebas: /SIMULA (dos sensores de mentira que suben con la carga),
 * /DURA=s (la prueba dura s segundos), /ENFRIA=s y /ARRANQUE (lo lanza el
 * escritorio). Por el puerto serie, «termo: ...».
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "juego.h"
#include <stdarg.h>
#include "termo_acpi.h"

static int mas_caliente(void);
static void mira_anterior(void);
static void log_arranque(void);
static void log_escribe(const char *clase, const char *texto);
static char ruta_log[64], anterior[700];
static int log_malo;
static u32 cuenta_s;
static const char *busca(const char *t, const char *o)
{
	size_t n = strlen(o);

	for (; *t; t++)
		if (!strncmp(t, o, n))
			return t;
	return 0;
}

#define SC_MSR 220
#define SC_PCI_TERMO 221
#define SC_SMN 222
#define SC_ZONAS 223
#define SC_CARGA 224
#define SC_ENERGIA 154

#define NO TA_NO
#define MAXS 14
#define HIST 300		/* 10 minutos, cada 2 s */

/* ---------------------------------------------------------------- colores */
#define C_FONDO 0xF4F1EA
#define C_PANEL 0xFFFFFF
#define C_TINTA 0x1A1A1A
#define C_GRIS 0x6A6A6A
#define C_RAYA 0xC9C3B6
#define C_CABECERA 0x23395B
#define C_BIEN 0x3A7BD5		/* azul */
#define C_TEMPLADO 0xE8C22A	/* amarillo */
#define C_CALIENTE 0xE07B1F	/* naranja */
#define C_PELIGRO 0xA01848	/* granate */
static const u32 trazo_color[8] = { 0x1F4E9A, 0xC0392B, 0x2E8B57, 0x7D3C98, 0xB9770E, 0x117A8B, 0x444444, 0xD35400 };

/* ---------------------------------------------------------------- los sensores */
enum { CL_CPU, CL_ACPI, CL_SIM };
struct sensor {
	char nombre[48];	/* (UTF-8) */
	char csv[32];		/* (sin acentos) */
	int clase, zona;	/* (CL_ACPI: el número de su zona) */
	int t, min, max;	/* décimas de grado; NO, no se sabe */
	int limite, supuesto;	/* el límite (TjMax, _CRT), y si es de tabla */
	int hist[HIST];
};
static struct sensor S[MAXS];
static int ns, hpos, hn;

/* ---------------------------------------------------------------- el procesador */
static struct {
	int intel, amd, fam, modelo, pasos, max_hoja;
	char marca[52], fab[16];
	int dts, ptm, aperf, msr_ok;
	int tjmax, tj_de_msr;
	int amd_metodo;		/* 1 K8, 2 0:18.3 A4h, 3 SMN D8200C64h, 4 SMN 59800h */
	char no_hay[160];	/* por qué no hay temperatura del procesador */
} cpu;
static u64 tsc_hz;
static u32 mhz_ahora, mhz_tsc;
static u32 aperf0, mperf0, aperf0h, mperf0h;
static int simula;

static void cpuid(u32 h, u32 r[4])
{
	__asm__ volatile("cpuid" : "=a"(r[0]), "=b"(r[1]), "=c"(r[2]), "=d"(r[3]) : "a"(h), "c"(0));
}

static int lee_msr(u32 m, u32 *lo, u32 *hi)
{
	u32 b[2] = { 0, 0 };

	if (sc(SC_MSR, m, (u32)b, 0) != 1)
		return 0;
	*lo = b[0];
	if (hi)
		*hi = b[1];
	return 1;
}

static u32 lee_pci(u32 disp, u32 fun, u32 reg)
{
	return sc(SC_PCI_TERMO, disp << 11 | fun << 8 | reg, 0, 0);
}

static int nuevo_sensor(const char *nombre, const char *csv, int clase, int limite, int supuesto)
{
	struct sensor *s;
	int i;

	if (ns >= MAXS)
		return -1;
	s = &S[ns];
	memset(s, 0, sizeof *s);
	snprintf(s->nombre, sizeof s->nombre, "%s", nombre);
	snprintf(s->csv, sizeof s->csv, "%s", csv);
	s->clase = clase;
	s->t = s->min = s->max = NO;
	s->limite = limite;
	s->supuesto = supuesto;
	for (i = 0; i < HIST; i++)
		s->hist[i] = NO;
	return ns++;
}

static void mira_procesador(void)
{
	u32 r[4], lo, hi, v;
	int i, k;

	cpuid(0, r);
	cpu.max_hoja = (int)r[0];
	memcpy(cpu.fab, &r[1], 4);
	memcpy(cpu.fab + 4, &r[3], 4);
	memcpy(cpu.fab + 8, &r[2], 4);
	cpu.fab[12] = 0;
	cpu.intel = !strcmp(cpu.fab, "GenuineIntel");
	cpu.amd = !strcmp(cpu.fab, "AuthenticAMD");
	cpuid(1, r);
	cpu.fam = (int)((r[0] >> 8) & 15);
	cpu.modelo = (int)((r[0] >> 4) & 15);
	cpu.pasos = (int)(r[0] & 15);
	if (cpu.fam == 15)
		cpu.fam += (int)((r[0] >> 20) & 0xFF);
	if (cpu.fam == 6 || cpu.fam >= 15)
		cpu.modelo |= (int)((r[0] >> 12) & 0xF0);
	cpu.msr_ok = (r[3] >> 5) & 1;
	cpuid(0x80000000u, r);
	if (r[0] >= 0x80000004u) {
		for (i = 0; i < 3; i++) {
			cpuid(0x80000002u + (u32)i, r);
			memcpy(cpu.marca + i * 16, r, 16);
		}
		cpu.marca[48] = 0;
		/* (sin los blancos de delante) */
		for (i = 0; cpu.marca[i] == ' '; i++)
			;
		memmove(cpu.marca, cpu.marca + i, strlen(cpu.marca + i) + 1);
	} else
		snprintf(cpu.marca, sizeof cpu.marca, "%s, familia %Xh, modelo %Xh", cpu.fab, cpu.fam, cpu.modelo);
	if (cpu.max_hoja >= 6) {
		cpuid(6, r);
		cpu.dts = r[0] & 1;
		cpu.ptm = (r[0] >> 6) & 1;
		cpu.aperf = r[2] & 1;
	}
	serie("termo: procesador «%s» (%s, familia %Xh, modelo %Xh, paso %u); CPUID 6: sensor %u, paquete %u, APERF %u\n",
	      cpu.marca, cpu.fab, cpu.fam, cpu.modelo, cpu.pasos, cpu.dts, cpu.ptm, cpu.aperf);
	if (!cpu.dts)	/* (también para las pruebas: si no lo tiene, el núcleo recoge la excepción 13) */
		serie("termo: MSR 19Ch sin sensor: %s\n", lee_msr(0x19C, &lo, &hi) ? "se lee (sin el bit de CPUID 6)" : "no lo tiene (lectura recogida por el núcleo)");
	if (cpu.intel) {
		if (!cpu.dts) {
			snprintf(cpu.no_hay, sizeof cpu.no_hay, "Este procesador Intel no dice tener sensor de temperatura (CPUID 6). En una máquina virtual es lo normal.");
			return;
		}
		/* la TjMax: del MSR 1A2h, o de la tabla */
		if (lee_msr(0x1A2, &lo, &hi) && ((lo >> 16) & 0xFF) >= 70 && ((lo >> 16) & 0xFF) <= 130) {
			cpu.tjmax = (int)((lo >> 16) & 0xFF);
			cpu.tj_de_msr = 1;
		} else {
			cpu.tjmax = 100;
			if (cpu.fam == 6 && (cpu.modelo == 0x1C || cpu.modelo == 0x26))
				cpu.tjmax = 90;	/* los Atom N270/N280/Z5xx */
			else if (cpu.fam == 6 && (cpu.modelo == 0x0F || cpu.modelo == 0x16 || cpu.modelo == 0x17) &&
				 lee_msr(0xEE, &lo, &hi) && (lo & (1u << 30)))
				cpu.tjmax = 85;	/* los Core 2 de sobremesa */
		}
		serie("termo: TjMax %u °C (%s)\n", cpu.tjmax, cpu.tj_de_msr ? "del MSR 1A2h" : "de la tabla: el procesador no la dice");
		if (lee_msr(0x19C, &lo, &hi) && (k = nuevo_sensor("Núcleo 0 del procesador", "Nucleo 0", CL_CPU, cpu.tjmax * 10, !cpu.tj_de_msr)) >= 0)
			S[k].zona = 0x19C;
		else
			snprintf(cpu.no_hay, sizeof cpu.no_hay, "El procesador dice tener sensor, pero no deja leerlo (MSR 19Ch).");
		if (cpu.ptm && lee_msr(0x1B1, &lo, &hi) && (k = nuevo_sensor("Procesador entero (paquete)", "Paquete", CL_CPU, cpu.tjmax * 10, !cpu.tj_de_msr)) >= 0)
			S[k].zona = 0x1B1;
	} else if (cpu.amd) {
		int cl = 900;

		v = lee_pci(0x18, 3, 0);
		if (cpu.fam == 0x0F && (v & 0xFFFF) == 0x1022)
			cpu.amd_metodo = 1, cl = 950;
		else if (((cpu.fam == 0x15 && cpu.modelo >= 0x60) || (cpu.fam == 0x16 && cpu.modelo >= 0x30)) && !ES_ERROR(sc(SC_SMN, 0xD8200C64u, 0, 0)))
			cpu.amd_metodo = 3;
		else if (cpu.fam >= 0x10 && cpu.fam <= 0x16 && (v & 0xFFFF) == 0x1022)
			cpu.amd_metodo = 2;
		else if (cpu.fam >= 0x17 && !ES_ERROR(sc(SC_SMN, 0x59800u, 0, 0)))
			cpu.amd_metodo = 4, cl = 950;
		if (cpu.amd_metodo) {
			nuevo_sensor(cpu.amd_metodo == 1 ? "Procesador (sensor del K8)" : "Procesador (Tctl)", "Procesador", CL_CPU, cl, 1);
			serie("termo: AMD, sensor por el método %u\n", cpu.amd_metodo);
		} else
			snprintf(cpu.no_hay, sizeof cpu.no_hay, "No se encuentra el sensor de este AMD (ni en PCI 0:18.3 ni por el SMN). En una máquina virtual es lo normal.");
	} else
		snprintf(cpu.no_hay, sizeof cpu.no_hay, "No es un procesador Intel ni AMD: no se sabe leer su temperatura.");
}

/* la temperatura del procesador ahora (décimas), del sensor i de la CPU */
static int lee_cpu(u32 msr)
{
	u32 lo, hi, v;

	if (cpu.intel) {
		if (!lee_msr(msr, &lo, &hi) || !(lo & 0x80000000u))
			return NO;
		return (cpu.tjmax - (int)((lo >> 16) & 0x7F)) * 10;
	}
	switch (cpu.amd_metodo) {
	case 1:
		v = lee_pci(0x18, 3, 0xE4);
		if (ES_ERROR(v))
			return NO;
		if (cpu.modelo >= 0x40)
			return (int)((v >> 14) & 0x3FF) * 10 / 4 - 490;
		return ((int)((v >> 16) & 0xFF) - 49) * 10;
	case 2:
		v = lee_pci(0x18, 3, 0xA4);
		return ES_ERROR(v) ? NO : (int)(v >> 21) * 5 / 4;
	case 3:
		v = sc(SC_SMN, 0xD8200C64u, 0, 0);
		return ES_ERROR(v) ? NO : (int)(v >> 21) * 5 / 4;
	case 4:
		v = sc(SC_SMN, 0x59800u, 0, 0);
		if (ES_ERROR(v))
			return NO;
		return (int)(v >> 21) * 5 / 4 - ((v & 0x80000u) ? 490 : 0);
	}
	return NO;
}

/* ---------------------------------------------------------------- ACPI y la batería */
static union { struct termo_acpi t; char b[TA_TAM]; } za;
static int hay_zonas;
static u32 zonas_lecturas;
struct energia {
	u32 version, fase, nbat, pct, ca, cargando, minutos, estado, lecturas;
	u32 salud_pct, salud_n, unidad, diseno, llena, ciclos1, ciclos2;
	u32 s5, pm1a_cnt, pm1b_cnt, slp_typa, slp_typb, smi_cmd, acpi_enable;
	u32 reset_esp, reset_dir, reset_valor, respuesta, sin_archivos;
};
static struct energia en;
static int hay_bateria;

static void lee_zonas(void)
{
	u32 i;

	if (sc(SC_ZONAS, (u32)&za, 0, 0) != 1 || za.t.version != TA_VERSION)
		return;
	if (!hay_zonas) {
		hay_zonas = 1;
		serie("termo: ACPI: %u zonas térmicas, %u ventiladores; equipo «%s» «%s»\n", za.t.nzonas, za.t.nvent, za.t.fabricante, za.t.producto);
	}
	zonas_lecturas = za.t.lecturas;
	for (i = 0; i < za.t.nzonas && i < TA_ZONAS; i++) {
		int k, lim;

		for (k = 0; k < ns; k++)
			if (S[k].clase == CL_ACPI && S[k].zona == (int)i)
				break;
		if (k == ns) {
			char n[48], c[32];

			lim = za.t.zona[i].crt != NO ? za.t.zona[i].crt : za.t.zona[i].hot;
			snprintf(n, sizeof n, "Zona ACPI %s", za.t.zona[i].nombre);
			snprintf(c, sizeof c, "ACPI %s", za.t.zona[i].nombre);
			k = nuevo_sensor(n, c, CL_ACPI, lim, 0);
			if (k < 0)
				break;
			S[k].zona = (int)i;
		}
		S[k].t = za.t.zona[i].tmp;
	}
	hay_bateria = sc(SC_ENERGIA, (u32)&en, 0, 0) == 1 && en.nbat > 0;
}

/* ---------------------------------------------------------------- la simulación (/SIMULA) */
static int carga_on;
static void simula_paso(void)
{
	int k;

	for (k = 0; k < ns; k++) {
		int meta, *t = &S[k].t;

		if (S[k].clase != CL_SIM)
			continue;
		meta = carga_on ? (k & 1 ? 980 : 1050) : (k & 1 ? 400 : 420);
		if (*t == NO)
			*t = meta;
		*t += (meta - *t) / 12;
	}
}

/* ---------------------------------------------------------------- el estado */
enum { E_NADA, E_BIEN, E_TEMPLADO, E_CALIENTE, E_PELIGRO };
static const char *const nombre_estado[] = { "sin dato", "bien", "templado", "caliente", "peligro" };
static const u32 color_estado[] = { 0xBBBBBB, C_BIEN, C_TEMPLADO, C_CALIENTE, C_PELIGRO };

static int estado_de(int t, int lim)
{
	if (t == NO)
		return E_NADA;
	if (lim == NO)
		lim = 1000;
	if (t >= lim - 100)
		return E_PELIGRO;
	if (t >= lim - 250)
		return E_CALIENTE;
	if (t >= lim - 400)
		return E_TEMPLADO;
	return E_BIEN;
}

/* ---------------------------------------------------------------- textos */
static char *grados(char *d, int t)
{
	if (t == NO)
		strcpy(d, "--");
	else if (t < 0)
		snprintf(d, 16, "-%d,%d °C", -t / 10, -t % 10);
	else
		snprintf(d, 16, "%d,%d °C", t / 10, t % 10);
	return d;
}

static char *tiempo(char *d, int seg)
{
	if (seg >= 60)
		snprintf(d, 24, "%d min %d s", seg / 60, seg % 60);
	else
		snprintf(d, 24, "%d s", seg);
	return d;
}

/* sin acentos (para el CSV) */
static void llano(char *d, const char *o, int cabe)
{
	int n = 0;

	while (*o && n + 1 < cabe) {
		unsigned char c = (unsigned char)*o;

		if (c == 0xC3 && o[1]) {
			unsigned char e = (unsigned char)o[1];
			const char *de = "aaaaaaaceeeeiiiidnooooo/ouuuuyty";

			d[n++] = e >= 0xA0 && e <= 0xBF ? de[e - 0xA0] : e >= 0x80 && e < 0xA0 ? (char)(de[e - 0x80] - 32) : '?';
			o += 2;
		} else if (c == 0xC2 && o[1]) {
			o += 2;		/* (el grado y demás, fuera) */
		} else if (c >= 0x80) {
			o++;
		} else {
			d[n++] = *o == ';' ? ',' : *o;
			o++;
		}
	}
	d[n] = 0;
}

/* ---------------------------------------------------------------- los archivos */
static char unidad = 0;
static char ruta_csv[64], ruta_txt[64], aviso_archivo[160];
static int csv_listo, csv_malo;
static u32 t_inicio;

static void fecha_hora(u32 *a, u32 *me, u32 *d, u32 *h, u32 *mi, u32 *s)
{
	u32 f = sc(SC_DATO, DATO_FECHA, 0, 0), fe = f >> 16, ho = f & 0xFFFF;

	*a = 1980 + (fe >> 9);
	*me = (fe >> 5) & 15;
	*d = fe & 31;
	*h = ho >> 11;
	*mi = (ho >> 5) & 63;
	*s = (ho & 31) * 2;
}

static int escribe(const char *ruta, const void *b, u32 n, int crear)
{
	u32 m = sc(SC_ABRIR, (u32)ruta, crear ? PARA_CREAR : PARA_ESCRIBIR, 0), r;

	if (ES_ERROR(m))
		return 0;
	if (!crear)
		sc(SC_MOVER, m, 0, 2);
	r = sc(SC_ESCRIBIR_ARCHIVO, m, (u32)b, n);
	sc(SC_CERRAR, m, 0, 0);
	return r == n;
}

typedef struct {
	u8 letra, clase, medio, solo;
	u32 total_k, libre_k, del_sistema;
	char nombre[41], etiqueta[12];
	u8 quitable, relleno[2];
} info_unidad;

/* la unidad donde se guarda: la del sistema (la memoria USB). Nunca C: ni
 * la M: (la de los módulos, de solo lectura) */
static void prepara_archivos(void)
{
	info_unidad u;
	int i;
	char c[24];
	u32 a, me, d, h, mi, s;

	for (i = 0; i < 40; i++) {
		memset(&u, 0, sizeof u);
		if (sc(43, (u32)i, (u32)&u, 0) != 1)
			break;
		if (u.del_sistema && u.letra)
			unidad = (char)u.letra;
	}
	if (!unidad || unidad == 'M' || unidad == 'C') {
		snprintf(aviso_archivo, sizeof aviso_archivo, "No se guarda el registro: el sistema no ha arrancado desde una memoria USB en la que se pueda escribir.");
		csv_malo = 1;
		return;
	}
	snprintf(c, sizeof c, "%c:\\TERMO", unidad);
	sc(SC_HACER_CARPETA, (u32)c, 0, 0);
	fecha_hora(&a, &me, &d, &h, &mi, &s);
	snprintf(ruta_csv, sizeof ruta_csv, "%c:\\TERMO\\%02u%02u%02u%02u.CSV", unidad, me, d, h, mi);
	snprintf(ruta_txt, sizeof ruta_txt, "%c:\\TERMO\\%02u%02u%02u%02u.TXT", unidad, me, d, h, mi);
	mira_anterior();
	snprintf(ruta_log, sizeof ruta_log, "%c:\\TERMO\\%02u%02u%02u%02u.LOG", unidad, me, d, h, mi);
	{
		char t[120];
		int n = snprintf(t, sizeof t, "Registro del Termometro de Hispa-Pentium, %02u/%02u/%04u\r\n", d, me, a);

		if (!escribe(ruta_log, t, (u32)n, 1)) {
			log_malo = 1;
			return;
		}
	}
	log_arranque();
	if (anterior[0])
		log_escribe("SUCESO", anterior);
}

static void csv_cabecera(void)
{
	char b[1024], n[40];
	int k, m;

	m = snprintf(b, sizeof b, "fecha;hora;segundos;fase;");
	for (k = 0; k < ns; k++) {
		llano(n, S[k].csv, sizeof n);
		m += snprintf(b + m, sizeof b - (size_t)m, "%s (grados C);", n);
	}
	m += snprintf(b + m, sizeof b - (size_t)m, "frecuencia (MHz);bateria (%%);enchufado\r\n");
	if (!escribe(ruta_csv, b, (u32)m, 1)) {
		csv_malo = 1;
		snprintf(aviso_archivo, sizeof aviso_archivo, "No se puede escribir %s (¿la memoria está protegida contra escritura?).", ruta_csv);
		serie("termo: no se puede crear %s\n", ruta_csv);
		return;
	}
	csv_listo = 1;
	snprintf(aviso_archivo, sizeof aviso_archivo, "Registro: %s (una línea cada 2 s).  F12: guarda la pantalla.", ruta_csv);
	serie("termo: registro en %s, %d sensores\n", ruta_csv, ns);
}

static const char *fase_texto(void);

static void csv_linea(void)
{
	char b[512];
	int k, m;
	u32 a, me, d, h, mi, s;

	if (!csv_listo || csv_malo)
		return;
	fecha_hora(&a, &me, &d, &h, &mi, &s);
	m = snprintf(b, sizeof b, "%02u/%02u/%04u;%02u:%02u:%02u;%u;%s;", d, me, a, h, mi, s, (jg_tics() - t_inicio) / jg_hz, fase_texto());
	for (k = 0; k < ns; k++)
		if (S[k].t == NO)
			m += snprintf(b + m, sizeof b - (size_t)m, ";");
		else
			m += snprintf(b + m, sizeof b - (size_t)m, "%s%d,%d;", S[k].t < 0 ? "-" : "", abs(S[k].t) / 10, abs(S[k].t) % 10);
	m += snprintf(b + m, sizeof b - (size_t)m, "%u;", mhz_ahora);
	if (hay_bateria && en.pct <= 100)
		m += snprintf(b + m, sizeof b - (size_t)m, "%u;%s\r\n", en.pct, en.ca == 1 ? "si" : en.ca == 0 ? "no" : "");
	else
		m += snprintf(b + m, sizeof b - (size_t)m, ";\r\n");
	if (!escribe(ruta_csv, b, (u32)m, 0)) {
		csv_malo = 1;
		snprintf(aviso_archivo, sizeof aviso_archivo, "Se ha dejado de poder escribir %s.", ruta_csv);
	}
}

/* ---------------------------------------------------------------- el registro de sucesos (.LOG)
 * X:\TERMO\MMDDHHMM.LOG: todo lo que pasa, para saber, si el equipo se apaga
 * de golpe (el corte térmico de la placa), qué hacía y a qué temperatura.
 * Cada línea se escribe abriendo, añadiendo y cerrando el archivo: la FAT
 * del núcleo escribe en el momento (fat.cas: nada se queda en la memoria) y
 * al cerrar queda el tamaño en la carpeta; si se va la luz, como mucho se
 * pierde la línea que se estaba escribiendo. Las líneas:
 *   hh:mm:ss ARRANQUE ...   lo del arranque (X:\ARRANQUE.TXT del núcleo)
 *   hh:mm:ss SUCESO ...     lo que pasa (la prueba, los estados, avisos)
 *   hh:mm:ss DATOS fase=... max=...; sensor=...; ...   cada 2 s
 *   hh:mm:ss CIERRE ...     el Termómetro se ha cerrado bien
 */
static void log_escribe(const char *clase, const char *texto)
{
	char b[600], l[560];
	u32 a, me, d, h, mi, se;
	int n;

	if (!ruta_log[0] || log_malo)
		return;
	fecha_hora(&a, &me, &d, &h, &mi, &se);
	llano(l, texto, sizeof l);
	n = snprintf(b, sizeof b, "%02u:%02u:%02u %s %s\r\n", h, mi, se, clase, l);
	if (n > (int)sizeof b - 1)
		n = (int)sizeof b - 1;
	if (!escribe(ruta_log, b, (u32)n, 0) && !escribe(ruta_log, b, (u32)n, 1))
		log_malo = 1;
}

static void suceso_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void suceso_log(const char *fmt, ...)
{
	char t[400];
	va_list v;

	va_start(v, fmt);
	vsnprintf(t, sizeof t, fmt, v);
	va_end(v);
	serie("termo: suceso: %s\n", t);
	log_escribe("SUCESO", t);
}

/* cada 2 s: la fase y todas las temperaturas */
static void log_datos(void)
{
	char b[520], g[24];
	int k, m;

	m = snprintf(b, sizeof b, "fase=%s max=%s", fase_texto(), grados(g, mas_caliente()));
	for (k = 0; k < ns; k++)
		m += snprintf(b + m, sizeof b - (size_t)m, " | %s=%s", S[k].csv, grados(g, S[k].t));
	m += snprintf(b + m, sizeof b - (size_t)m, " | MHz=%u", mhz_ahora);
	if (hay_bateria && en.pct <= 100)
		snprintf(b + m, sizeof b - (size_t)m, " | bateria=%u%% %s", en.pct, en.ca == 1 ? "enchufado" : en.ca == 0 ? "sin enchufar" : "");
	log_escribe("DATOS", b);
}

/* los cambios: de estado de cada sensor, del ventilador, de la batería */
static int est_antes[MAXS], vent_antes[TA_VENT], ca_antes = -1, pct_antes = -1, vistos_antes[MAXS];

static void mira_cambios(void)
{
	char g[24], g2[24];
	static int zonas_dichas;
	int k;
	u32 i;

	if (hay_zonas && !zonas_dichas) {
		zonas_dichas = 1;
		suceso_log("ACPI (ACPI.BIN): %u zonas térmicas, %u ventiladores; equipo «%s %s», placa «%s %s», BIOS «%s»", za.t.nzonas, za.t.nvent,
			   za.t.fabricante, za.t.producto, za.t.placa_fab, za.t.placa, za.t.bios);
	}
	if (!hay_zonas && cuenta_s == 20)
		suceso_log("AVISO: el servicio de ACPI no da las zonas térmicas (¿arrancado con «sinacpi», o ACPI.BIN no va?)");
	for (k = 0; k < ns; k++) {
		int e = estado_de(S[k].t, S[k].limite);

		if (S[k].t == NO && vistos_antes[k])
			suceso_log("AVISO: el sensor «%s» ha dejado de responder", S[k].nombre);
		if (S[k].t != NO && !vistos_antes[k])
			suceso_log("sensor «%s»: %s (límite %s)", S[k].nombre, grados(g, S[k].t), S[k].limite == NO ? "no dice" : grados(g2, S[k].limite));
		vistos_antes[k] = S[k].t != NO;
		if (e != E_NADA && est_antes[k] != E_NADA && e != est_antes[k])
			suceso_log("%s: de «%s» a «%s» (%s)", S[k].nombre, nombre_estado[est_antes[k]], nombre_estado[e], grados(g, S[k].t));
		if (e != E_NADA)
			est_antes[k] = e;
	}
	for (i = 0; hay_zonas && i < za.t.nvent && i < TA_VENT; i++) {
		struct ta_vent *v = &za.t.vent[i];
		int x = v->velocidad != NO ? v->velocidad / 500 + 10 : v->encendido;

		if (x != vent_antes[i])
			suceso_log("ventilador %u: %s", i + 1, v->velocidad != NO ? (snprintf(g, sizeof g, "%d r.p.m.", v->velocidad), g) :
				   v->encendido == 1 ? "en marcha" : v->encendido == 0 ? "parado" : "sin datos");
		vent_antes[i] = x;
	}
	if (hay_bateria) {
		if ((int)en.ca != ca_antes)
			suceso_log("batería: %s", en.ca == 1 ? "enchufado a la corriente" : en.ca == 0 ? "SIN enchufar (con la batería)" : "no se sabe si está enchufado");
		ca_antes = (int)en.ca;
		if (en.pct <= 100 && (pct_antes < 0 || abs((int)en.pct - pct_antes) >= 10)) {
			suceso_log("batería al %u %%", en.pct);
			pct_antes = (int)en.pct;
		}
	}
}

/* lo del arranque (el núcleo lo deja en \ARRANQUE.TXT con «registro»), al .LOG */
static void log_arranque(void)
{
	char r[24], *b, *p;
	u32 m, n, largo;

	snprintf(r, sizeof r, "%c:\\ARRANQUE.TXT", unidad);
	m = sc(SC_ABRIR, (u32)r, PARA_LEER, 0);
	if (ES_ERROR(m)) {
		log_escribe("ARRANQUE", "(no hay \\ARRANQUE.TXT del nucleo)");
		return;
	}
	largo = sc(SC_LARGO, m, 0, 0);
	if (ES_ERROR(largo))
		largo = 0;
	if (largo > 12000)
		largo = 12000;
	b = (char *)malloc(largo + 1);
	n = b ? sc(SC_LEER, m, (u32)b, largo) : 0;
	sc(SC_CERRAR, m, 0, 0);
	if (!b)
		return;
	if (ES_ERROR(n))
		n = 0;
	b[n] = 0;
	for (p = b; *p;) {
		char *f = p;

		while (*f && *f != '\n' && *f != '\r')
			f++;
		if (f > p) {
			char c = *f;

			*f = 0;
			log_escribe("ARRANQUE", p);
			*f = c;
		}
		p = *f ? f + 1 : f;
	}
	free(b);
}

/* ---- la vez anterior: ¿se cerró bien? ---- */
static int ver_anterior;

static int saca_campo(const char *l, const char *clave, char *d, int cabe)
{
	const char *p = l;
	size_t n = strlen(clave);
	int i = 0;

	for (; *p; p++)
		if (!strncmp(p, clave, n))
			break;
	if (!*p)
		return 0;
	p += n;
	while (*p && *p != ';' && *p != ' ' && i + 1 < cabe)
		d[i++] = *p++;
	d[i] = 0;
	return 1;
}

/* décimas de «45,0» (o NO) */
static int decimas(const char *t)
{
	int v = 0, neg = 0, d = 0;

	if (*t == '-')
		neg = 1, t++;
	if (*t < '0' || *t > '9')
		return NO;
	while (*t >= '0' && *t <= '9')
		v = v * 10 + *t++ - '0';
	if (*t == ',' && t[1] >= '0' && t[1] <= '9')
		d = t[1] - '0';
	v = v * 10 + d;
	return neg ? -v : v;
}

static void mira_anterior(void)
{
	char c[24], mejor[16] = "", r[64], *b, *p, *ult = 0;
	struct entrada e;
	u32 m, n, largo, desde;
	char hora[12] = "", fase_a[16] = "", maxi[16] = "", h60[12] = "";
	int lmin = NO, lmax = NO, cerrado = 0, k, nlin = 0;
	const char *lineas[40];

	snprintf(c, sizeof c, "%c:\\TERMO", unidad);
	m = sc(SC_ABRIR, (u32)c, PARA_LEER, 0);
	if (ES_ERROR(m))
		return;
	while (sc(SC_ENTRADA, m, (u32)&e, 0) == 1) {
		char nb[14];

		memcpy(nb, e.nombre, 13);
		nb[13] = 0;
		if (strlen(nb) == 12 && !strcmp(nb + 8, ".LOG") && strcmp(nb, mejor) > 0)
			strcpy(mejor, nb);
	}
	sc(SC_CERRAR, m, 0, 0);
	if (!mejor[0])
		return;
	snprintf(r, sizeof r, "%c:\\TERMO\\%s", unidad, mejor);
	m = sc(SC_ABRIR, (u32)r, PARA_LEER, 0);
	if (ES_ERROR(m))
		return;
	largo = sc(SC_LARGO, m, 0, 0);
	if (ES_ERROR(largo))
		largo = 0;
	desde = largo > 8000 ? largo - 8000 : 0;
	sc(SC_MOVER, m, desde, 0);
	b = (char *)malloc(8001);
	n = b ? sc(SC_LEER, m, (u32)b, largo - desde) : 0;
	sc(SC_CERRAR, m, 0, 0);
	if (!b)
		return;
	if (ES_ERROR(n))
		n = 0;
	b[n] = 0;
	/* las líneas de DATOS (las últimas 40) y si hay CIERRE */
	for (p = b; *p;) {
		char *f = p;

		while (*f && *f != '\n' && *f != '\r')
			f++;
		if (*f)
			*f++ = 0;
		if (busca(p, " CIERRE "))
			cerrado = 1;
		else if (busca(p, " DATOS ")) {
			cerrado = 0;
			if (nlin == 40) {
				memmove(lineas, lineas + 1, sizeof lineas[0] * 39);
				nlin--;
			}
			lineas[nlin++] = p;
			ult = p;
		} else if (busca(p, " SUCESO ") || busca(p, " ARRANQUE "))
			cerrado = 0;
		while (*f == '\n' || *f == '\r')
			f++;
		p = f;
	}
	serie("termo: el registro anterior, %s: %s\n", mejor, cerrado ? "se cerró bien" : "NO se cerró");
	if (cerrado || !ult) {
		free(b);
		return;
	}
	memcpy(hora, ult, 8);
	hora[8] = 0;
	saca_campo(ult, "fase=", fase_a, sizeof fase_a);
	saca_campo(ult, "max=", maxi, sizeof maxi);
	/* los últimos 60 s (30 líneas) */
	for (k = nlin > 30 ? nlin - 30 : 0; k < nlin; k++) {
		char t[16];
		int v;

		if (!h60[0]) {
			memcpy(h60, lineas[k], 8);
			h60[8] = 0;
		}
		if (saca_campo(lineas[k], "max=", t, sizeof t) && (v = decimas(t)) != NO) {
			if (lmin == NO || v < lmin)
				lmin = v;
			if (lmax == NO || v > lmax)
				lmax = v;
		}
	}
	{
		char a[24], bb[24];
		const char *fs = !strcmp(fase_a, "carga") ? "la PRUEBA DE ESFUERZO (calentando)" : !strcmp(fase_a, "enfriando") ? "el enfriamiento, después de la prueba" : "el reposo (sin prueba)";

		snprintf(anterior, sizeof anterior,
			 "La última vez (registro %s) el Termómetro no se cerró: el equipo se apagó solo (o lo apagaron sin cerrarlo) durante %s, a las %s, con el más caliente a %s °C. "
			 "En sus últimos 60 s (desde las %s) estuvo entre %s y %s.%s",
			 mejor, fs, hora, maxi, h60, grados(a, lmin), grados(bb, lmax),
			 (lmax != NO && lmax >= 850) || !strcmp(fase_a, "carga") ? " Si fue durante la prueba y estaba muy caliente, seguramente lo apagó la protección térmica: limpie el ventilador y cambie la pasta." : "");
	}
	ver_anterior = 1;
	free(b);
}

/* la pantalla (F12), en .BMP de 24 bits */
static void captura(void)
{
	char ruta[64];
	u8 cab[54], *fila;
	u32 a, me, d, h, mi, s, m, tf, total;
	int y, x;

	if (!unidad || csv_malo)
		return;
	fecha_hora(&a, &me, &d, &h, &mi, &s);
	snprintf(ruta, sizeof ruta, "%c:\\TERMO\\%02u%02u%02u%02u.BMP", unidad, d, h, mi, s);
	tf = ((u32)LZ.an * 3 + 3) & ~3u;
	total = 54 + tf * (u32)LZ.al;
	memset(cab, 0, sizeof cab);
	cab[0] = 'B';
	cab[1] = 'M';
	memcpy(cab + 2, &total, 4);
	cab[10] = 54;
	cab[14] = 40;
	memcpy(cab + 18, &LZ.an, 4);
	memcpy(cab + 22, &LZ.al, 4);
	cab[26] = 1;
	cab[28] = 24;
	m = sc(SC_ABRIR, (u32)ruta, PARA_CREAR, 0);
	if (ES_ERROR(m))
		return;
	fila = (u8 *)malloc(tf);
	if (fila) {
		sc(SC_ESCRIBIR_ARCHIVO, m, (u32)cab, 54);
		for (y = LZ.al - 1; y >= 0; y--) {
			const u32 *p = LZ.p + y * LZ.paso;

			memset(fila, 0, tf);
			for (x = 0; x < LZ.an; x++) {
				fila[x * 3] = (u8)p[x];
				fila[x * 3 + 1] = (u8)(p[x] >> 8);
				fila[x * 3 + 2] = (u8)(p[x] >> 16);
			}
			sc(SC_ESCRIBIR_ARCHIVO, m, (u32)fila, tf);
		}
		free(fila);
	}
	sc(SC_CERRAR, m, 0, 0);
	snprintf(aviso_archivo, sizeof aviso_archivo, "Pantalla guardada en %s.", ruta);
	serie("termo: captura en %s\n", ruta);
}

/* ---------------------------------------------------------------- la prueba */
enum { F_REPOSO, F_CARGA, F_ENFRIA, F_RESULTADO };
static int fase = F_REPOSO, minutos = 10, dura_s, enfria_max = 180;
static u32 t_fase;
static int t_ini, pico, t_parada, por_seguridad, por_esc, seg_carga;
static int baja30 = NO, baja60 = NO, vuelve_s = -1;
static char resumen[1200], motivo[160];
static int hija = -1, solo_aqui;

static const char *fase_texto(void)
{
	return fase == F_CARGA ? "carga" : fase == F_ENFRIA ? "enfriando" : "reposo";
}

/* la más caliente de las que se leen (para la prueba) */
static int mas_caliente(void)
{
	int k, m = NO;

	for (k = 0; k < ns; k++)
		if (S[k].t != NO && (m == NO || S[k].t > m))
			m = S[k].t;
	return m;
}

static void empieza(void)
{
	char a[40];

	if (fase == F_CARGA)
		return;
	fase = F_CARGA;
	t_fase = jg_tics();
	t_ini = mas_caliente();
	pico = t_ini;
	por_seguridad = por_esc = 0;
	baja30 = baja60 = NO;
	vuelve_s = -1;
	motivo[0] = 0;
	carga_on = 1;
	sc(SC_CARGA, 1, 1, 0);
	/* la carga: un programa hijo (este mismo, con /CARGA) que no suelta el
	 * procesador; si no se puede lanzar, aquí, entre pulso y pulso */
	snprintf(a, sizeof a, "/CARGA=%d", dura_s + 60);
	solo_aqui = 0;
	hija = -1;
	{
		static const char *const donde[] = { "\\TERMO.BIN", "TERMO.BIN", "\\HISPA\\TERMO.BIN" };
		int i;

		for (i = 0; i < 3 && hija < 0; i++) {
			u32 r = sc(SC_LANZAR_SUELTO, (u32)donde[i], (u32)a, 0);

			if (!ES_ERROR(r))
				hija = (int)r;
		}
	}
	if (hija < 0)
		solo_aqui = 1;
	serie("termo: calentar %d s (%s); empieza a %d décimas\n", dura_s, solo_aqui ? "la carga en esta misma tarea" : "la carga en una tarea hija", t_ini);
	{
		char g[24];

		suceso_log("EMPIEZA la prueba de esfuerzo: %d s, carga de coma flotante (x87) en 1 núcleo (%s); el más caliente, a %s",
			   dura_s, solo_aqui ? "en la tarea del Termómetro, unos 8 ms de cada 10" : "tarea hija TERMO.BIN /CARGA, al 100 %", grados(g, t_ini));
	}
}

static void para_carga(int seguridad, int esc)
{
	char a[24], b[24], c[24];

	if (fase != F_CARGA)
		return;
	carga_on = 0;
	sc(SC_CARGA, 1, 0, 0);
	seg_carga = (int)((jg_tics() - t_fase) / jg_hz);
	por_seguridad = seguridad;
	por_esc = esc;
	t_parada = mas_caliente();
	fase = F_ENFRIA;
	t_fase = jg_tics();
	serie("termo: fin de la carga a los %d s (%s); pico %s, ahora %s, al empezar %s\n", seg_carga,
	      seguridad ? "PARADA DE SEGURIDAD" : esc ? "Esc" : "se acabó el tiempo", grados(a, pico), grados(b, t_parada), grados(c, t_ini));
	suceso_log("ACABA la carga a los %d s: %s%s; llegó a %s, al empezar %s. Empieza el enfriamiento", seg_carga,
		   seguridad ? "PARADA DE SEGURIDAD: " : esc ? "parada con Esc" : "se acabó el tiempo", seguridad ? motivo : "", grados(a, pico), grados(c, t_ini));
}

static void escribe_resumen(void)
{
	char a[24], b[24], c[24], d[24], e[24];
	int sube = (pico != NO && t_ini != NO) ? pico - t_ini : 0, m = 0, lim = NO, k;

	for (k = 0; k < ns; k++)
		if (S[k].limite != NO && (lim == NO || S[k].limite < lim))
			lim = S[k].limite;
	if (por_seguridad)
		m += snprintf(resumen + m, sizeof resumen - (size_t)m, "PARADA DE SEGURIDAD a los %s: %s. ", tiempo(a, seg_carga), motivo);
	else
		m += snprintf(resumen + m, sizeof resumen - (size_t)m, "Prueba de %s%s. ", tiempo(a, seg_carga), por_esc ? " (parada con Esc)" : "");
	m += snprintf(resumen + m, sizeof resumen - (size_t)m, "Subió de %s a %s (%s%d °C). ", grados(b, t_ini), grados(c, pico), sube >= 0 ? "+" : "", sube / 10);
	if (baja60 != NO)
		m += snprintf(resumen + m, sizeof resumen - (size_t)m, "Al parar, bajó %d °C en el primer minuto", baja60 / 10);
	else if (baja30 != NO)
		m += snprintf(resumen + m, sizeof resumen - (size_t)m, "Al parar, bajó %d °C en 30 s", baja30 / 10);
	if (vuelve_s >= 0)
		m += snprintf(resumen + m, sizeof resumen - (size_t)m, " y volvió a %s en %s. ", grados(d, t_ini + 50), tiempo(e, vuelve_s));
	else
		m += snprintf(resumen + m, sizeof resumen - (size_t)m, ". ");
	m += snprintf(resumen + m, sizeof resumen - (size_t)m, "\n\n");
	if (por_seguridad)
		m += snprintf(resumen + m, sizeof resumen - (size_t)m, "PELIGRO: se calienta demasiado. Limpie el polvo del ventilador y de las rejillas y cambie la pasta térmica del procesador. ");
	else if (lim != NO && pico != NO && pico >= lim - 200)
		m += snprintf(resumen + m, sizeof resumen - (size_t)m, "Se calienta bastante: en carga es normal en un portátil, pero queda cerca del límite; una limpieza le vendría bien. ");
	else if (sube < 50 && !simula)
		m += snprintf(resumen + m, sizeof resumen - (size_t)m, "Casi no ha subido: o se enfría muy bien, o la carga de un solo núcleo no basta para calentarlo. ");
	else
		m += snprintf(resumen + m, sizeof resumen - (size_t)m, "La temperatura en carga es buena. ");
	if (sube >= 100) {
		int b60 = baja60 != NO ? baja60 : baja30 != NO ? baja30 : 0;

		if (b60 >= 150 || b60 * 10 >= sube * 4)
			m += snprintf(resumen + m, sizeof resumen - (size_t)m, "Al parar se enfría deprisa: el ventilador y el disipador sacan bien el calor.");
		else if (b60 < 50)
			m += snprintf(resumen + m, sizeof resumen - (size_t)m, "Al parar baja muy despacio: el ventilador o el disipador no hacen bien su trabajo (polvo, ventilador que no gira o pasta seca).");
		else
			m += snprintf(resumen + m, sizeof resumen - (size_t)m, "Al parar baja algo despacio: puede tener polvo.");
	}
	serie("termo: resumen: %s\n", resumen);
	suceso_log("RESULTADO: %s", resumen);
	if (unidad && !csv_malo && ruta_txt[0]) {
		char t[1400];
		int n = 0;
		u32 aa, me, dd, h, mi, s;

		fecha_hora(&aa, &me, &dd, &h, &mi, &s);
		n = snprintf(t, sizeof t, "Termometro de Hispa-Pentium - prueba del %02u/%02u/%04u a las %02u:%02u\r\nProcesador: %s\r\nEquipo: %s %s\r\n", dd, me, aa, h, mi,
			     cpu.marca, za.t.fabricante, za.t.producto);
		llano(t + n, resumen, (int)sizeof t - n - 4);
		n += (int)strlen(t + n);
		n += snprintf(t + n, sizeof t - (size_t)n, "\r\n\r\n");
		if (!escribe(ruta_txt, t, (u32)n, 0))
			escribe(ruta_txt, t, (u32)n, 1);
	}
}

/* lo de la prueba, cada segundo */
static void prueba_segundo(void)
{
	int k, p = mas_caliente(), seg = (int)((jg_tics() - t_fase) / jg_hz);

	if (fase == F_CARGA) {
		if (p != NO && (pico == NO || p > pico))
			pico = p;
		/* la parada de seguridad: el límite menos 10 °C (o 95 °C sin límite) */
		for (k = 0; k < ns; k++) {
			int lim = S[k].limite != NO ? S[k].limite : 1050;

			if (S[k].t != NO && S[k].t >= lim - 100) {
				char a[24], b[24];

				snprintf(motivo, sizeof motivo, "%s llegó a %s (su límite: %s)", S[k].nombre, grados(a, S[k].t), grados(b, lim));
				para_carga(1, 0);
				return;
			}
		}
		if (seg >= dura_s)
			para_carga(0, 0);
	} else if (fase == F_ENFRIA) {
		if (seg == 30 && p != NO)
			baja30 = t_parada - p;
		if (seg == 60 && p != NO)
			baja60 = t_parada - p;
		if (vuelve_s < 0 && p != NO && t_ini != NO && p <= t_ini + 50)
			vuelve_s = seg;
		if (seg >= enfria_max || (vuelve_s >= 0 && seg >= 60)) {
			fase = F_RESULTADO;
			escribe_resumen();
		}
	}
}

/* ---------------------------------------------------------------- la carga (la tarea hija) */
static int carga_hija(int segundos)
{
	u32 fin = jg_tics() + (u32)segundos * 100;
	volatile float sale = 0;
	float a = 1.0001f, b = 0.9999f, c = 0.5f;
	int i;

	sc(SC_SERIE, (u32)"termo: carga en marcha\n", 23, 0);
	for (;;) {
		for (i = 0; i < 200000; i++) {
			c = m_raiz(a * b + c) * 0.7f + m_sen(c) * 0.3f;
			a = a * 1.0000001f + 0.0000001f;
			if (a > 2.0f)
				a = 1.0001f;
			b = b * 0.9999999f + c * 0.0000001f;
		}
		sale = c;
		if (sc(SC_CARGA, 0, 0, 0) != 1 || (int)(jg_tics() - fin) > 0)
			break;
	}
	(void)sale;
	sc(SC_SERIE, (u32)"termo: carga parada\n", 20, 0);
	return 0;
}

/* sin tarea hija: unos 8 ms de cada 10 */
static void carga_aqui(void)
{
	u64 hasta = rdtsc() + tsc_hz / 125;
	volatile float c = 0.5f;
	float a = 1.0001f;

	while (rdtsc() < hasta) {
		int i;

		for (i = 0; i < 2000; i++) {
			c = m_raiz(a + c) * 0.7f + m_sen(c) * 0.3f;
			a = a * 1.0000001f;
			if (a > 2.0f)
				a = 1.0001f;
		}
	}
}

/* ---------------------------------------------------------------- la frecuencia */
static void mide_tsc(void)
{
	u32 t0 = jg_tics(), t1;
	u64 r0, r1;

	while (jg_tics() == t0)
		;
	t0 = jg_tics();
	r0 = rdtsc();
	sc(SC_DORMIR, 50, 0, 0);
	t1 = jg_tics();
	r1 = rdtsc();
	if (t1 > t0)
		tsc_hz = (r1 - r0) * jg_hz / (t1 - t0);
	mhz_tsc = (u32)(tsc_hz / 1000000);
	mhz_ahora = mhz_tsc;
	if (cpu.aperf)
		cpu.aperf = lee_msr(0xE7, &mperf0, &mperf0h) && lee_msr(0xE8, &aperf0, &aperf0h);
	serie("termo: el reloj del procesador (TSC) va a %u MHz; APERF/MPERF %s\n", mhz_tsc, cpu.aperf ? "sí" : "no");
}

static void mide_frecuencia(void)
{
	u32 a, ah, m, mh;
	u64 da, dm;

	if (!cpu.aperf)
		return;
	if (!lee_msr(0xE7, &m, &mh) || !lee_msr(0xE8, &a, &ah))
		return;
	dm = (((u64)mh << 32) | m) - (((u64)mperf0h << 32) | mperf0);
	da = (((u64)ah << 32) | a) - (((u64)aperf0h << 32) | aperf0);
	mperf0 = m, mperf0h = mh, aperf0 = a, aperf0h = ah;
	if (dm > 1000 && da < dm * 8)
		mhz_ahora = (u32)((u64)mhz_tsc * da / dm);
}

/* ---------------------------------------------------------------- leer todo */

static void lee_todo(void)
{
	int k;

	for (k = 0; k < ns; k++)
		if (S[k].clase == CL_CPU)
			S[k].t = lee_cpu((u32)S[k].zona);
	if (simula)
		simula_paso();
	for (k = 0; k < ns; k++) {
		if (S[k].t == NO)
			continue;
		if (S[k].min == NO || S[k].t < S[k].min)
			S[k].min = S[k].t;
		if (S[k].max == NO || S[k].t > S[k].max)
			S[k].max = S[k].t;
	}
	mide_frecuencia();
}

static void apunta_historia(void)
{
	int k;

	for (k = 0; k < ns; k++)
		S[k].hist[hpos] = S[k].t;
	hpos = (hpos + 1) % HIST;
	if (hn < HIST)
		hn++;
}

static void estado_serie(void)
{
	char b[400], g[24];
	int k, m;

	m = snprintf(b, sizeof b, "termo: estado fase=%s", fase_texto());
	for (k = 0; k < ns; k++)
		m += snprintf(b + m, sizeof b - (size_t)m, " [%s=%s %s]", S[k].csv, grados(g, S[k].t), nombre_estado[estado_de(S[k].t, S[k].limite)]);
	snprintf(b + m, sizeof b - (size_t)m, " mhz=%u\n", mhz_ahora);
	serie("%s", b);
}

static void pulso(void)
{
	static u32 tic;

	if (solo_aqui && carga_on)
		carga_aqui();
	if (++tic % 100)
		return;
	cuenta_s++;
	lee_todo();
	if (cuenta_s % 2 == 0) {
		lee_zonas();
		if (!csv_listo && !csv_malo && cuenta_s >= 6)
			csv_cabecera();
		apunta_historia();
		csv_linea();
		mira_cambios();
		log_datos();
		if (cuenta_s % 10 == 0)
			estado_serie();
	}
	prueba_segundo();
	jg_cambio = 1;
}

/* ---------------------------------------------------------------- pintar */
struct boton { int x, y, an, al; const char *t; int id; };
enum { B_CALENTAR = 1, B_5, B_10, B_15, B_PARAR, B_CAPTURA };
static struct boton bot[8];
static int nbot;

static void pixel_mezcla(int x, int y, u32 c)
{
	if (x >= 0 && y >= 0 && x < LZ.an && y < LZ.al)
		LZ.p[y * LZ.paso + x] = c;
}

/* la trama de cada estado (que no se distinga solo por el color) */
static void trama(int x, int y, int an, int al, int e, u32 c)
{
	int i, j;

	for (j = 0; j < al; j++)
		for (i = 0; i < an; i++) {
			int u = x + i, v = y + j, si = 0;

			switch (e) {
			case E_TEMPLADO: si = (u % 6 == 0) && (v % 6 == 0); break;		/* puntos */
			case E_CALIENTE: si = ((u + v) % 7) < 2; break;				/* rayas */
			case E_PELIGRO: si = ((u + v) % 6) < 2 || ((u - v + 600) % 6) < 2; break;	/* cuadros */
			}
			if (si)
				pixel_mezcla(u, v, c);
		}
}

/* la marca de cada sensor, en la gráfica y en la tabla */
static void marca(int k, int x, int y, u32 c)
{
	float p[8];

	switch (k % 6) {
	case 0: lz_circulo(&LZ, x, y, 4, c); break;
	case 1: lz_rect(&LZ, x - 4, y - 4, 9, 9, c); break;
	case 2:
		p[0] = (float)x; p[1] = (float)(y - 5); p[2] = (float)(x + 5); p[3] = (float)(y + 4); p[4] = (float)(x - 5); p[5] = (float)(y + 4);
		lz_poligono(&LZ, p, 3, c);
		break;
	case 3: lz_linea_gruesa(&LZ, x - 4, y - 4, x + 4, y + 4, 2, c); lz_linea_gruesa(&LZ, x - 4, y + 4, x + 4, y - 4, 2, c); break;
	case 4:
		p[0] = (float)x; p[1] = (float)(y - 5); p[2] = (float)(x + 5); p[3] = (float)y; p[4] = (float)x; p[5] = (float)(y + 5); p[6] = (float)(x - 5); p[7] = (float)y;
		lz_poligono(&LZ, p, 4, c);
		break;
	default: lz_anillo(&LZ, x, y, 4, 2, c); break;
	}
}

static void boton(int x, int y, int an, int al, const char *t, int id, int activo, int marcado)
{
	u32 fondo = !activo ? 0xE2DED6 : marcado ? 0xFFF3C4 : 0xFFFFFF;

	lz_redondo(&LZ, x, y, an, al, 5, 0x8A8378);
	lz_redondo(&LZ, x + 1, y + 1, an - 2, al - 2, 4, fondo);
	if (marcado)
		lz_marco(&LZ, x + 3, y + 3, an - 6, al - 6, C_TINTA);
	lz_texto_centro(&LZ, x + an / 2, y + (al - 16) / 2, t, activo ? C_TINTA : 0x9A948A, 1, marcado);
	if (nbot < 8) {
		bot[nbot].x = x, bot[nbot].y = y, bot[nbot].an = an, bot[nbot].al = al, bot[nbot].t = t, bot[nbot].id = id;
		nbot++;
	}
}

static void pinta_grafica(int x0, int y0, int an, int al)
{
	int tmin = 200, tmax = 1000, k, i, t, gx = x0 + 44, gan = an - 54, gy = y0 + 22, gal = al - 44;
	char g[24];

	for (k = 0; k < ns; k++)
		if (S[k].limite != NO && S[k].limite + 50 > tmax)
			tmax = S[k].limite + 50;
	tmax = (tmax + 99) / 100 * 100;
	lz_rect(&LZ, x0, y0, an, al, C_PANEL);
	lz_marco(&LZ, x0, y0, an, al, C_RAYA);
	lz_negrita(&LZ, x0 + 8, y0 + 4, "Los últimos 10 minutos (una marca cada 30 s)", C_TINTA, 1);
	if (gal < 40 || gan < 100)
		return;
	for (t = tmin; t <= tmax; t += 100) {
		int y = gy + gal - (t - tmin) * gal / (tmax - tmin);

		lz_rect(&LZ, gx, y, gan, 1, t % 200 ? 0xEEEAE2 : C_RAYA);
		if (t % 200 == 0) {
			snprintf(g, sizeof g, "%d°", t / 10);
			lz_texto(&LZ, x0 + 6, y - 8, g, C_GRIS, 1);
		}
	}
	for (i = 0; i <= 10; i++) {
		int x = gx + gan - i * gan / 10;

		lz_rect(&LZ, x, gy, 1, gal, 0xEEEAE2);
		if (i % 2 == 0) {
			snprintf(g, sizeof g, i ? "-%d min" : "ahora", i);
			lz_texto(&LZ, x - ancho_texto(g, 1) / 2, gy + gal + 4, g, C_GRIS, 1);
		}
	}
	/* los límites, a rayas */
	for (k = 0; k < ns; k++)
		if (S[k].limite != NO && S[k].limite <= tmax) {
			int y = gy + gal - (S[k].limite - tmin) * gal / (tmax - tmin);

			for (i = 0; i < gan; i += 12)
				lz_rect(&LZ, gx + i, y, 6, 2, C_PELIGRO);
			snprintf(g, sizeof g, "límite %d °C", S[k].limite / 10);
			lz_texto(&LZ, gx + gan - ancho_texto(g, 1) - 4, y - 17, g, C_PELIGRO, 1);
			break;
		}
	for (k = 0; k < ns; k++) {
		u32 c = trazo_color[k % 8];
		int px = -1, py = -1;

		for (i = 0; i < hn; i++) {
			int idx = (hpos - hn + i + HIST) % HIST, v = S[k].hist[idx], x, y, edad = hn - 1 - i;

			if (v == NO) {
				px = -1;
				continue;
			}
			v = LIMITA(v, tmin, tmax);
			x = gx + gan - edad * gan / (HIST - 1);
			y = gy + gal - (v - tmin) * gal / (tmax - tmin);
			if (px >= 0) {
				/* los trazos: seguido, a rayas o de puntos, por sensor */
				int patron = k % 3;

				if (patron == 0 || (patron == 1 && (i / 3) % 2 == 0) || (patron == 2 && i % 2 == 0))
					lz_linea_gruesa(&LZ, px, py, x, y, 2, c);
			}
			if (edad % 15 == 0)
				marca(k, x, y, c);
			px = x;
			py = y;
		}
	}
}

/* una nota bajo la tabla, partida en renglones si no cabe: da cuántos */
static int nota(int y, const char *t, u32 c)
{
	return lz_parrafo(&LZ, 14, y, LZ.an - 28, t, c, 1, 16);
}

static void pinta(void)
{
	int W = LZ.an, H = LZ.al, y, k, fila = 30, cn, ca, cmi, cma, cli, ce, ance, lineas;
	char a[200], g[24], h[24], g2[24], g3[24];

	nbot = 0;
	lz_rect(&LZ, 0, 0, W, H, C_FONDO);
	/* arriba: el procesador, el equipo, la frecuencia, la batería */
	lz_rect(&LZ, 0, 0, W, 78, C_CABECERA);
	lz_negrita(&LZ, 10, 6, "Termómetro", 0xFFFFFF, 2);
	lz_texto(&LZ, 10 + ancho_texto("Termómetro", 2) + 16, 14, "de Hispa-Pentium: ¿se calienta este equipo?", 0xD8E2F0, 1);
	snprintf(a, sizeof a, "Procesador: %s", cpu.marca);
	lz_texto(&LZ, 10, 40, a, 0xFFFFFF, 1);
	if (cpu.aperf)
		snprintf(a, sizeof a, "Frecuencia: %u MHz (la de serie: %u MHz)", mhz_ahora, mhz_tsc);
	else
		snprintf(a, sizeof a, "Frecuencia de serie: %u MHz", mhz_tsc);
	lz_texto(&LZ, W - ancho_texto(a, 1) - 10, 40, a, 0xFFFFFF, 1);
	if (hay_zonas && za.t.smbios == 1)
	{
		snprintf(a, sizeof a, "Equipo: %s %s %s  ·  Placa: %s %s  ·  BIOS: %s", za.t.fabricante, za.t.producto, za.t.version_eq,
			 za.t.placa_fab, za.t.placa, za.t.bios);
		if (ancho_texto(a, 1) > W - 20)
			snprintf(a, sizeof a, "Equipo: %s %s  ·  BIOS: %s", za.t.fabricante, za.t.producto, za.t.bios);
		if (ancho_texto(a, 1) > W - 20)
			snprintf(a, sizeof a, "Equipo: %s %s", za.t.fabricante, za.t.producto);
	} else
		snprintf(a, sizeof a, "Equipo: %s", hay_zonas ? "la BIOS no dice cuál (SMBIOS)" : "esperando al servicio de ACPI...");
	lz_texto(&LZ, 10, 58, a, 0xD8E2F0, 1);
	/* la tabla */
	y = 86;
	ance = W - 20;
	cn = 30;
	ca = cn + ance * 34 / 100;
	cmi = ca + 128;
	cma = cmi + 80;
	cli = cma + 80;
	ce = cli + 96;
	if (ce > W - 120) {	/* (a 800 de ancho) */
		ca = cn + 250;
		cmi = ca + 120;
		cma = cmi + 72;
		cli = cma + 72;
		ce = cli + 80;
	}
	lz_rect(&LZ, 10, y, ance, 22, 0xE6E1D6);
	lz_negrita(&LZ, cn, y + 3, "Sensor", C_TINTA, 1);
	lz_negrita(&LZ, ca, y + 3, "Ahora", C_TINTA, 1);
	lz_negrita(&LZ, cmi, y + 3, "Mínima", C_TINTA, 1);
	lz_negrita(&LZ, cma, y + 3, "Máxima", C_TINTA, 1);
	lz_negrita(&LZ, cli, y + 3, "Límite", C_TINTA, 1);
	lz_negrita(&LZ, ce, y + 3, "Estado", C_TINTA, 1);
	y += 24;
	if (ns == 0) {
		lz_rect(&LZ, 10, y, ance, fila * 2, C_PANEL);
		lz_parrafo(&LZ, cn, y + 6, ance - 40, "No hay ningún sensor que leer todavía. ", C_TINTA, 1, 18);
		y += fila * 2 + 2;
	}
	for (k = 0; k < ns; k++) {
		struct sensor *s = &S[k];
		int e = estado_de(s->t, s->limite), bx = ce, ban = W - 20 - ce, lim = s->limite != NO ? s->limite : 1000, llena;

		lz_rect(&LZ, 10, y, ance, fila, k % 2 ? 0xFAF8F3 : C_PANEL);
		marca(k, 20, y + fila / 2, trazo_color[k % 8]);
		lz_texto(&LZ, cn, y + 7, s->nombre, C_TINTA, 1);
		lz_negrita(&LZ, ca, y + 7, grados(g, s->t), C_TINTA, 1);
		lz_texto(&LZ, cmi, y + 7, grados(g, s->min), C_TINTA, 1);
		lz_texto(&LZ, cma, y + 7, grados(g, s->max), C_TINTA, 1);
		if (s->limite != NO)
			snprintf(h, sizeof h, "%d °C%s", s->limite / 10, s->supuesto ? " (?)" : "");
		else
			strcpy(h, "no dice");
		lz_texto(&LZ, cli, y + 7, h, C_GRIS, 1);
		/* la barra, de 20 °C al límite, con su trama y su estado escrito */
		lz_rect(&LZ, bx, y + 4, ban, fila - 8, 0xECE8E0);
		llena = s->t == NO ? 0 : LIMITA((s->t - 200) * ban / (lim - 200), 0, ban);
		lz_rect(&LZ, bx, y + 4, llena, fila - 8, color_estado[e]);
		trama(bx, y + 4, llena, fila - 8, e, e == E_PELIGRO ? 0xFFFFFF : aclara(color_estado[e], -90));
		lz_marco(&LZ, bx, y + 4, ban, fila - 8, 0x8A8378);
		{
			const char *t = nombre_estado[e];
			int an = ancho_texto(t, 1) + 10;

			lz_rect(&LZ, bx + ban - an - 3, y + 6, an, fila - 12, 0xFFFFFF);
			lz_negrita(&LZ, bx + ban - an + 2, y + 7, t, C_TINTA, 1);
		}
		y += fila;
	}
	/* lo que no se puede leer, dicho */
	lineas = 0;
	a[0] = 0;
	if (cpu.no_hay[0])
		lineas += nota(y + 4 + 16 * lineas, cpu.no_hay, C_PELIGRO);
	if (cpu.intel && cpu.dts)
		lineas += nota(y + 4 + 16 * lineas, "Hispa-Pentium usa un solo núcleo: se lee el núcleo 0; el «paquete» es el más caliente de todos.", C_GRIS);
	if (!hay_zonas)
		lineas += nota(y + 4 + 16 * lineas, "Las zonas térmicas de ACPI: esperando al servicio de ACPI (ACPI.BIN)...", C_GRIS);
	else if (za.t.nzonas == 0)
		lineas += nota(y + 4 + 16 * lineas, "La BIOS de este equipo no tiene zonas térmicas de ACPI.", C_GRIS);
	{
		int m = snprintf(a, sizeof a, "Gráfica dedicada (NVIDIA/AMD): no se puede leer sin su controlador; solo sale si la BIOS la da como zona ACPI.");

		(void)m;
		lineas += nota(y + 4 + 16 * lineas, a, C_GRIS);
	}
	{
		int m = 0;
		u32 i;

		if (hay_bateria) {
			m += snprintf(a + m, sizeof a - (size_t)m, "Batería: %s%u %%, %s", en.pct > 100 ? "?" : "", en.pct > 100 ? 0 : en.pct,
				      en.ca == 1 ? "enchufado" : en.ca == 0 ? "sin enchufar" : "¿enchufado?");
			if (en.salud_pct <= 150)
				m += snprintf(a + m, sizeof a - (size_t)m, ", salud %u %%", en.salud_pct);
		} else
			m += snprintf(a + m, sizeof a - (size_t)m, "Batería: no hay (o no se ve)");
		for (i = 0; hay_zonas && i < za.t.nvent && i < TA_VENT; i++) {
			struct ta_vent *v = &za.t.vent[i];

			if (v->velocidad != NO)
				m += snprintf(a + m, sizeof a - (size_t)m, "  ·  Ventilador %u: %d r.p.m.", i + 1, v->velocidad);
			else
				m += snprintf(a + m, sizeof a - (size_t)m, "  ·  Ventilador %u: %s", i + 1, v->encendido == 1 ? "en marcha" : v->encendido == 0 ? "parado" : "sin datos");
		}
		if (hay_zonas && za.t.nvent == 0)
			m += snprintf(a + m, sizeof a - (size_t)m, "  ·  Ventilador: la BIOS no lo da por ACPI (que se oiga)");
		lineas += nota(y + 4 + 16 * lineas, a, C_TINTA);
	}
	y += 8 + 16 * lineas;
	/* abajo: la prueba */
	{
		int yb = H - 60, ym = yb - 30, xb = 10, gal;

		gal = ym - y - 6;
		if (gal > 60)
			pinta_grafica(10, y, W - 20, gal);
		/* la franja de la prueba */
		if (fase == F_CARGA) {
			int seg = (int)((jg_tics() - t_fase) / jg_hz);

			snprintf(a, sizeof a, "CALENTANDO: %s de %s  ·  el más caliente: %s  ·  Esc para parar  ·  %s", tiempo(g, seg), tiempo(h, dura_s),
				 grados(g2, mas_caliente()), solo_aqui ? "carga sin tarea propia (~80 %)" : "solo 1 núcleo cargado");
			lz_rect(&LZ, 10, ym, W - 20, 26, C_CALIENTE);
			trama(10, ym, W - 20, 26, E_CALIENTE, aclara(C_CALIENTE, -40));
			lz_rect(&LZ, 14, ym + 4, ancho_texto(a, 1) + 8, 18, 0xFFFFFF);
			lz_negrita(&LZ, 18, ym + 5, a, C_TINTA, 1);
		} else if (fase == F_ENFRIA) {
			int seg = (int)((jg_tics() - t_fase) / jg_hz);

			snprintf(a, sizeof a, "ENFRIANDO: %s  ·  ahora %s (empezó en %s, llegó a %s)  ·  Esc: acabar ya", tiempo(g, seg), grados(h, mas_caliente()),
				 grados(g2, t_ini), grados(g3, pico));
			lz_rect(&LZ, 10, ym, W - 20, 26, C_BIEN);
			lz_rect(&LZ, 14, ym + 4, ancho_texto(a, 1) + 8, 18, 0xFFFFFF);
			lz_negrita(&LZ, 18, ym + 5, a, C_TINTA, 1);
		} else {
			lz_texto(&LZ, 14, ym + 5, "Prueba de esfuerzo: elija el tiempo y pulse «Calentar» (C). Se para sola si se calienta.", C_TINTA, 1);
		}
		boton(xb, yb, 150, 32, "Calentar (C)", B_CALENTAR, fase != F_CARGA, 0);
		xb += 160;
		boton(xb, yb, 90, 32, "5 min (1)", B_5, fase != F_CARGA, minutos == 5);
		xb += 96;
		boton(xb, yb, 96, 32, "10 min (2)", B_10, fase != F_CARGA, minutos == 10);
		xb += 102;
		boton(xb, yb, 96, 32, "15 min (3)", B_15, fase != F_CARGA, minutos == 15);
		xb += 112;
		boton(xb, yb, 120, 32, "Parar (Esc)", B_PARAR, fase == F_CARGA || fase == F_ENFRIA, 0);
		xb += 130;
		if (xb + 140 <= W)
			boton(xb, yb, 140, 32, "Captura (F12)", B_CAPTURA, 1, 0);
		lz_texto(&LZ, 12, H - 20, aviso_archivo, C_GRIS, 1);
		/* la vez anterior no acabó bien: se dice, encima de todo */
		if (ver_anterior) {
			int an = W - 120, rx = 60, ry = 100, al = 90 + 18 * lz_parrafo(0, 0, 0, an - 28, anterior, 0, 1, 18);

			lz_rect(&LZ, rx + 4, ry + 4, an, al, 0x9A948A);
			lz_rect(&LZ, rx, ry, an, al, 0xFFF3C4);
			trama(rx, ry, an, 8, E_PELIGRO, C_PELIGRO);
			lz_marco(&LZ, rx, ry, an, al, C_TINTA);
			lz_negrita(&LZ, rx + 14, ry + 16, "Aviso: la última vez no se cerró bien", C_TINTA, 2);
			lz_parrafo(&LZ, rx + 14, ry + 54, an - 28, anterior, C_TINTA, 1, 18);
			lz_texto(&LZ, rx + 14, ry + al - 22, "Intro, Esc o un clic: cerrar este aviso.", C_GRIS, 1);
		}
		/* el resultado, encima de la gráfica */
		if (fase == F_RESULTADO) {
			int an = W - 80, rx = 40, al = 90 + 18 * lz_parrafo(0, 0, 0, an - 28, resumen, 0, 1, 18), ry = ym - al - 10;

			if (ry < 84)
				ry = 84;
			lz_rect(&LZ, rx + 4, ry + 4, an, al, 0x9A948A);
			lz_rect(&LZ, rx, ry, an, al, 0xFFFDF5);
			lz_marco(&LZ, rx, ry, an, al, C_TINTA);
			lz_marco(&LZ, rx + 1, ry + 1, an - 2, al - 2, C_TINTA);
			lz_negrita(&LZ, rx + 14, ry + 10, "Resultado de la prueba", C_TINTA, 2);
			lz_parrafo(&LZ, rx + 14, ry + 46, an - 28, resumen, C_TINTA, 1, 18);
			lz_texto(&LZ, rx + 14, ry + al - 22, "Intro o un clic: cerrar este cuadro. (El resumen también queda en el .TXT de \\TERMO.)", C_GRIS, 1);
		}
	}
	jg_todo_sucio();
}

/* ---------------------------------------------------------------- las teclas y el ratón */
static void pide_minutos(int m)
{
	if (fase == F_CARGA)
		return;
	minutos = m;
	dura_s = arg_entero("/DURA", minutos * 60);
	jg_cambio = 1;
}

static void haz(int id)
{
	switch (id) {
	case B_CALENTAR:
		if (fase != F_CARGA) {
			fase = F_REPOSO;
			empieza();
		}
		break;
	case B_5: pide_minutos(5); break;
	case B_10: pide_minutos(10); break;
	case B_15: pide_minutos(15); break;
	case B_PARAR:
		if (fase == F_CARGA)
			para_carga(0, 1);
		else if (fase == F_ENFRIA) {
			fase = F_RESULTADO;
			escribe_resumen();
		}
		break;
	case B_CAPTURA:
		captura();
		break;
	}
	jg_cambio = 1;
}

static void suceso(const struct suceso *s)
{
	int i;

	if (ver_anterior && (s->clase == EV_RATON_BAJA || (s->clase == EV_TECLA && (ES_INTRO(s->a) || s->a == T_ESC)))) {
		ver_anterior = 0;
		jg_cambio = 1;
		return;
	}
	if (s->clase == EV_TECLA) {
		switch (s->a) {
		case 'c': case 'C': haz(B_CALENTAR); break;
		case '1': haz(B_5); break;
		case '2': haz(B_10); break;
		case '3': haz(B_15); break;
		case T_ESC: haz(B_PARAR); break;
		case 0x158: captura(); estado_serie(); break;	/* F12 */
		default:
			if (ES_INTRO(s->a) && fase == F_RESULTADO) {
				fase = F_REPOSO;
				jg_cambio = 1;
			}
		}
	} else if (s->clase == EV_RATON_BAJA) {
		int x = (int)(int32_t)s->a, y = (int)(int32_t)s->b;

		if (fase == F_RESULTADO) {
			fase = F_REPOSO;
			jg_cambio = 1;
			return;
		}
		for (i = 0; i < nbot; i++)
			if (x >= bot[i].x && y >= bot[i].y && x < bot[i].x + bot[i].an && y < bot[i].y + bot[i].al)
				haz(bot[i].id);
	}
}

static int al_cerrar(void)
{
	if (fase == F_CARGA)
		suceso_log("la carga se para: se cierra el Termómetro");
	sc(SC_CARGA, 1, 0, 0);
	carga_on = 0;
	log_escribe("CIERRE", "el Termometro se ha cerrado bien");
	return 1;
}

static void tamano(int an, int al)
{
	(void)an;
	(void)al;
	jg_cambio = 1;
}

static const struct juego J = { pulso, pinta, suceso, tamano, al_cerrar, 10 };

/* ---------------------------------------------------------------- principal */
static char args[256];


static int arg_aqui(const char *o)
{
	return busca(args, o) != 0;
}

int principal(void)
{
	int an, al, i;
	u32 n = sc(SC_ARGUMENTOS, (u32)args, sizeof args - 1, 0);

	if (ES_ERROR(n))
		n = 0;
	args[MIN(n, sizeof args - 1)] = 0;
	for (i = 0; args[i]; i++)
		if (args[i] >= 'a' && args[i] <= 'z')
			args[i] = (char)(args[i] - 32);
	if (arg_aqui("/CARGA")) {
		const char *p = busca(args, "/CARGA=");
		int s = 0;

		for (p = p ? p + 7 : "1000"; *p >= '0' && *p <= '9'; p++)
			s = s * 10 + *p - '0';

		return carga_hija(s > 0 ? s : 1000);
	}
	/* (los argumentos, para arg_entero: jg_correr los vuelve a leer) */
	memcpy(jg_args, args, sizeof args);
	simula = arg_aqui("/SIMULA");
	dura_s = arg_entero("/DURA", minutos * 60);
	enfria_max = arg_entero("/ENFRIA", 180);
	mira_procesador();
	if (simula) {
		nuevo_sensor("Procesador simulado (prueba)", "Simulado", CL_SIM, 1000, 0);
		nuevo_sensor("Zona simulada (prueba)", "Zona simulada", CL_SIM, 950, 0);
	}
	jg_tamano_maximo(&an, &al);
	if (jg_abrir("Termómetro", an, al, 0) < 0)
		return 1;
	mide_tsc();
	prepara_archivos();
	t_inicio = jg_tics();
	lee_todo();
	lee_zonas();
	serie("termo: en marcha, %d sensores; ventana %d x %d\n", ns, LZ.an, LZ.al);
	suceso_log("Termómetro abierto%s: procesador «%s»; %d sensores del procesador%s; registro %s", arg_aqui("/ARRANQUE") ? " (solo, al arrancar desde la memoria)" : "",
		   cpu.marca, ns, cpu.no_hay[0] ? " (no hay: se dice en pantalla)" : "", ruta_csv);
	if (anterior[0])
		serie("termo: aviso: %s\n", anterior);
	jg_correr(&J);
	sc(SC_CARGA, 1, 0, 0);
	return 0;
}
