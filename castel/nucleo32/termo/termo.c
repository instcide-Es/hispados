/* termo.c - (TERMO) el Termómetro de Hispa-Pentium (TERMO.BIN): las
 * temperaturas del equipo, para ver si un portátil se calienta (y si hay que
 * limpiarle el ventilador o cambiarle la pasta térmica).
 *
 *  - El procesador: Intel (CPUID 6: el sensor digital, DTS; IA32_THERM_STATUS
 *    19Ch del núcleo que corre y IA32_PACKAGE_THERM_STATUS 1B1h de todo el
 *    procesador; la TjMax de MSR_TEMPERATURE_TARGET 1A2h o, si no lo tiene,
 *    de una tabla) y AMD (como k10temp de Linux: K8, PCI 0:18.3 E4h;
 *    familias 10h, 11h, 12h, 14h, 15h modelos 00h-5Fh y 16h: 0:18.3 A4h;
 *    15h modelos 60h-7Fh: SMN D8200CA4h por 0:0.0 B8h/BCh; 17h en adelante:
 *    SMN 59800h por 0:0.0 60h/64h, con -49 °C si el bit 19 lo dice). Lo lee el núcleo (las llamadas 220 a 222, termo.cas): solo
 *    lectura y solo esos registros.
 *  - ACPI: las zonas térmicas (_TMP, _CRT, _PSV, _HOT) y los ventiladores
 *    (_FST), que lee el servicio de ACPI (ACPI.BIN) y da el núcleo (la 223);
 *    la batería (la 154); el fabricante y el modelo del equipo (SMBIOS).
 *  - Una tabla grande (nombre, ahora, mínima, máxima, límite y una barra con
 *    su estado escrito: «bien», «templado», «caliente», «peligro»; nunca solo
 *    el color), la gráfica de los últimos 10 minutos (cada sensor con su
 *    trazo y su marca), la frecuencia (APERF/MPERF, o la del TSC).
 *  - «Calentar»: el procesador al 100 % (un programa hijo, TERMO.BIN /CARGA,
 *    con /FUERTE la carga fuerte, SSE2: TERMO 1.2,
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
#include "veredicto.h"
#include "envio.h"
#include "rendimiento.h"
#include "disco.h"
#include "nvidia.h"
#include "potencia.h"
#include "ram.h"

static int mas_caliente(void);
static void mira_envio(void);
static int en_reposo(void);
static int informe_frenos(char *d, int cabe);
static void mira_mide(void);
static void mira_ram(void);
static void empieza_ram(void);
static void lanza_ram(int completa);
static int ram_simula;	/* (RAM) «ramsimula» en la línea del cargador: un fallo de mentira (las pruebas) */
static int midiendo;
static u32 t_mide;
static char aviso_mide[320];
static void mira_anterior(void);
static void log_arranque(void);
static void log_escribe(const char *clase, const char *texto);
static char ruta_log[64], anterior[700];
static int log_malo;
static u32 cuenta_s;
static int fecha_mala, pidio_hora;
/* (TERMO) los parones: los SMI (MSR 34h: -1 sin mirar, 0 no, 1 sí; los de
 * los últimos 2 s) y lo que el equipo entero ha estado parado en ellos (ms) */
static int smi_hay = -1, parado_dos;
static int carga_vueltas, carga_rota, ap_relanzados;	/* (TERMO) la carga rota por un parón */
static u32 smi_dos;
static int ap_cargados;		/* (TERMO) los otros procesadores que cargan */	/* (TERMO) el año del reloj, si parece mal */
/* (TERMO) lo ocupado (mide_ocupacion) */
static int ocup_pct = -1, c0_pct = -1, c6_pct = -1;
static u32 oc_ociosa, oc_tics, oc_mperf, oc_mperfh, oc_c6, oc_c6h;
static u64 oc_tsc;
static int hay_cestados;
/* (PARPADEO) lo pintado la vez anterior y lo que se cuenta (vuelca_cambios) */
static u32 *antes_p;
static int antes_an, antes_al;
static u32 cuadros_pintados, puntos_subidos, pinta_sin_motivo;
static int motivo_pinta = 1;
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
#define SC_GPU 225	/* (TERMO) las gráficas NVIDIA Maxwell: solo leer (termo.cas) */
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
enum { CL_CPU, CL_ACPI, CL_SIM, CL_GPU };
struct sensor {
	char nombre[48];	/* (UTF-8) */
	char csv[32];		/* (sin acentos) */
	int clase, zona;	/* (CL_ACPI: el número de su zona) */
	int t, min, max;	/* décimas de grado; NO, no se sabe */
	int limite, supuesto;	/* el límite (TjMax, _CRT), y si es de tabla */
	int fija;		/* (TERMO) no ha cambiado en toda la última prueba */
	int no_fiable;		/* (TERMO) del procesador, clavada 30 s en carga (el Toshiba NB250: 6 °C siempre) */
	u32 estado_dicho;	/* (ACPI) el porqué de «--» ya apuntado */
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
	int amd_metodo;		/* 1 K8, 2 0:18.3 A4h, 3 SMN D8200CA4h, 4 SMN 59800h */
	char no_hay[160];	/* por qué no hay temperatura del procesador */
	int aproximada;		/* (TERMO) un Atom: su sensor es poco de fiar */
	char msr_texto[120];	/* lo que da el 19Ch la primera vez (para el registro) */
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

static int tjmax_tabla(void)
{
	u32 lo = 0, hi;
	int ee = (cpu.modelo == 0x0F || cpu.modelo == 0x16 || cpu.modelo == 0x17) && lee_msr(0xEE, &lo, &hi) && (lo & (1u << 30));

	return ve_tjmax_intel(cpu.fam, cpu.modelo, cpu.pasos, cpu.marca, ee);
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
		/* la TjMax: del MSR 1A2h (solo de Nehalem en adelante: en los de
		 * antes y en los Atom de la 1Ch no existe o dice otra cosa), o de
		 * la tabla (la de coretemp de Linux, con los datos de Intel) */
		cpu.tjmax = tjmax_tabla();
		if (cpu.fam == 6 && cpu.modelo > 0x0E && cpu.modelo != 0x1C && lee_msr(0x1A2, &lo, &hi) &&
		    ((lo >> 16) & 0xFF) >= 70 && ((lo >> 16) & 0xFF) <= 130) {
			cpu.tjmax = (int)((lo >> 16) & 0xFF);
			cpu.tj_de_msr = 1;
		}
		serie("termo: TjMax %u °C (%s)\n", cpu.tjmax, cpu.tj_de_msr ? "del MSR 1A2h" : "de la tabla: el procesador no la dice");
		/* (TERMO) los Atom de la 06_1Ch (N2xx, N4xx, D4xx, D5xx): su sensor es
		 * poco fiable (Intel y coretemp de Linux lo dicen: sobre todo frío), y
		 * en el Toshiba NB250 llegó a dar 6 °C */
		cpu.aproximada = cpu.fam == 6 && (cpu.modelo == 0x1C || cpu.modelo == 0x26);
		if (lee_msr(0x19C, &lo, &hi)) {
			serie("termo: MSR 19Ch = %08Xh: %s, %u °C por debajo de la TjMax, resolución %u °C\n", lo, (lo >> 31) ? "lectura válida" : "SIN lectura válida",
			      (lo >> 16) & 0x7F, (lo >> 27) & 15);
			snprintf(cpu.msr_texto, sizeof cpu.msr_texto, "MSR 19Ch = %08Xh (%s; %u bajo la TjMax de %u; resolución %u °C)", lo,
				 (lo >> 31) ? "válida" : "sin lectura válida", (lo >> 16) & 0x7F, cpu.tjmax, (lo >> 27) & 15);
		}
		if (lee_msr(0x19C, &lo, &hi) && (k = nuevo_sensor(cpu.aproximada ? "Núcleo 0 (aproximada)" : "Núcleo 0 del procesador", "Nucleo 0", CL_CPU, cpu.tjmax * 10, !cpu.tj_de_msr)) >= 0)
			S[k].zona = 0x19C;
		else
			snprintf(cpu.no_hay, sizeof cpu.no_hay, "El procesador dice tener sensor, pero no deja leerlo (MSR 19Ch).");
		/* (los estados de reposo: de Nehalem en adelante, sin los Atom de antes) */
		hay_cestados = cpu.fam == 6 && cpu.modelo >= 0x1A && cpu.modelo != 0x1C && cpu.modelo != 0x26 && cpu.modelo != 0x27 &&
			       cpu.modelo != 0x35 && cpu.modelo != 0x36 && lee_msr(0x3FD, &lo, &hi);
		if (cpu.ptm && lee_msr(0x1B1, &lo, &hi) && (k = nuevo_sensor("Procesador entero (paquete)", "Paquete", CL_CPU, cpu.tjmax * 10, !cpu.tj_de_msr)) >= 0)
			S[k].zona = 0x1B1;
	} else if (cpu.amd) {
		int cl = 900;

		v = lee_pci(0x18, 3, 0);
		if (cpu.fam == 0x0F && (v & 0xFFFF) == 0x1022)
			cpu.amd_metodo = 1, cl = 950;
		else if (cpu.fam == 0x15 && (cpu.modelo & 0xF0) >= 0x60 && (cpu.modelo & 0xF0) <= 0x70 && !ES_ERROR(sc(SC_SMN, 0xD8200CA4u, 0, 0)))
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
		v = sc(SC_SMN, 0xD8200CA4u, 0, 0);
		return ES_ERROR(v) ? NO : (int)(v >> 21) * 5 / 4;
	case 4:
		v = sc(SC_SMN, 0x59800u, 0, 0);
		if (ES_ERROR(v))
			return NO;
		return (int)(v >> 21) * 5 / 4 - ((v & 0x80000u) ? 490 : 0);
	}
	return NO;
}

/* ---------------------------------------------------------------- las NVIDIA
 * (TERMO) (NVIDIA) Las gráficas NVIDIA (Maxwell: las GTX 970M/980M del MSI
 * GT80; Pascal: la GTX 1050 del ASUS GL553V): lo hace nvidia.c (con el
 * núcleo, la 225). Aquí, «GPU 1», «GPU 2»... como sensores. Con «sololeer»
 * no se escribe nada en ellas (ni lo volátil de nouveau) */
static struct nv_todo NV;
static int gpu_estado[MAXS], gpu_frena[MAXS], gpu_critico[MAXS], hay_gpus, nv_sololeer;

static void mira_gpus(void)
{
	int i;

	nv_busca(&NV, !nv_sololeer);
	for (i = 0; i < NV.n; i++) {
		char n[48], c[16], l[900];
		const char *m = nv_modelo(NV.g[i].f.id);
		int k;

		snprintf(n, sizeof n, "GPU %d (NVIDIA %s)", i + 1, m[0] ? m + (strncmp(m, "GeForce ", 8) ? 0 : 8) : "10DE");
		snprintf(c, sizeof c, "GPU %d", i + 1);
		k = nuevo_sensor(n, c, CL_GPU, NO, 0);
		if (k < 0)
			break;
		S[k].zona = i;
		hay_gpus++;
		nv_describe(&NV, i, l, sizeof l);
		serie("termo: %s\n", l);
	}
	for (i = 0; i < NV.np; i++) {
		char l[600];

		nv_describe_puerto(&NV, i, l, sizeof l);
		serie("termo: %s\n", l);
	}
}

static void lee_gpu(int k)
{
	const struct nv_ficha *f = &NV.g[S[k].zona].f;

	S[k].t = nv_lee(&NV, S[k].zona, !nv_sololeer);
	gpu_estado[k] = (int)f->estado;
	gpu_critico[k] = f->estado == 0 && ((f->boot >> 20) & 0x1F0) != 0x130 && (f->critico & 1);
	gpu_frena[k] = f->estado == 0 && ((f->boot >> 20) & 0x1F0) != 0x130 && ((f->frena >> 24) & 7);
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
static char sin_bat_texto[160];	/* (TERMO) por qué no hay batería, si no la hay (a los 20 s) */
/* (TERMO) la batería sin enchufar: el primer y el último porcentaje vistos y
 * cuándo (cuenta_s), si ha habido carga entretanto, y lo que gastaba (mW, de
 * _BST: lo más alto y la última lectura); para el consumo y la autonomía del
 * informe */
static int desc_pct0 = -1, desc_pct1, desc_carga, desc_enchufado, desc_mw = -1, desc_mw_max = -1;
static u32 desc_s0, desc_s1;
static int en_carga(void);

/* un ventilador de los ASUS: «parado», «no disponible» o sus r.p.m. */
static const char *vent_asus(char *t, int v)
{
	if (v == NO)
		return "no disponible";
	if (v == 0)
		return "parado";
	snprintf(t, 16, "%d r.p.m.", v);
	return t;
}

/* los vatios, con una cifra decimal («12,3») */
static const char *vatios(char *t, int mw)
{
	snprintf(t, 16, "%d,%d", mw / 1000, (mw % 1000 + 50) / 100 > 9 ? 9 : (mw % 1000 + 50) / 100);
	return t;
}

static u32 t_inicio;
static int acpi_tarde;
static void lee_zonas(void)
{
	u32 i;

	/* (TERMO) con lecturas = 0, ACPI.BIN aún no ha leído nada (el bloque
	 * que da antes de la primera lectura, a cero): en el ThinkPad E14 salía
	 * «0 zonas térmicas, equipo  , placa  , BIOS » y luego llegaba la zona */
	if (sc(SC_ZONAS, (u32)&za, 0, 0) != 1 || za.t.version != TA_VERSION || za.t.lecturas == 0)
		return;
	/* (TERMO 1.2, solo las pruebas) «acpitarde» en la línea del cargador:
	 * lo de ACPI no se mira hasta 15 s después de abrir (como el ASUS
	 * GL553VD, recién arrancado): Calentar tiene que esperar a ello */
	if (acpi_tarde && (!t_inicio || (int)(jg_tics() - t_inicio) < 15 * (int)jg_hz))
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
	/* (TERMO) los ASUS (WMI, DSTS): su temperatura, como un sensor más */
	if (za.t.asus_hay && za.t.asus_temp != NO) {
		int k;

		for (k = 0; k < ns; k++)
			if (S[k].clase == CL_ACPI && S[k].zona == 100)
				break;
		if (k == ns && (k = nuevo_sensor("Temperatura ASUS (WMI)", "ASUS WMI", CL_ACPI, NO, 0)) >= 0)
			S[k].zona = 100;
		if (k >= 0 && k < ns)
			S[k].t = za.t.asus_escribe ? NO : za.t.asus_temp;
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
/* (TERMO) dónde va todo: X:\TERMO en la memoria del Termómetro (la unidad del
 * sistema es una memoria USB con \TERMO.BIN en la raíz); instalado (el
 * paquete «termo»), la carpeta del usuario, \USUARIOS\<u>\TERMO (la llamada
 * 200), o \TERMO de la unidad del sistema si no hay usuarios */
static char carpeta[96];
static int en_memoria, arranque_auto;
static int sin_llamadas;	/* el núcleo no tiene las llamadas 220-224 (antes de la 0.8.9) */
static char ruta_csv[64], ruta_txt[64], aviso_archivo[160];
static int csv_listo, csv_malo;

/* (TERMO) la hora del registro no va hacia atrás: en el Monovate 14.1 (el
 * 8254 parado, el tic del HPET) las primeras líneas salían con 13:59:24 y
 * las siguientes con 13:59:20 (23/11/2026). Si el reloj de la CMOS dice una
 * hora de hasta 10 min antes que la última dada (el mismo día), se da otra
 * vez la última; un salto mayor (o al día siguiente) se toma tal cual */
static u32 fh_ultima;

static void fecha_hora(u32 *a, u32 *me, u32 *d, u32 *h, u32 *mi, u32 *s)
{
	u32 f = sc(SC_DATO, DATO_FECHA, 0, 0), fe, ho;

	if (fh_ultima && (f >> 16) == (fh_ultima >> 16) && (f & 0xFFFF) < (fh_ultima & 0xFFFF)) {
		u32 x = f & 0xFFFF, y = fh_ultima & 0xFFFF;
		u32 sx = (x >> 11) * 3600 + ((x >> 5) & 63) * 60 + (x & 31) * 2, sy = (y >> 11) * 3600 + ((y >> 5) & 63) * 60 + (y & 31) * 2;

		if (sy - sx <= 600)
			f = fh_ultima;
	}
	fh_ultima = f;
	fe = f >> 16, ho = f & 0xFFFF;

	*a = 1980 + (fe >> 9);
	*me = (fe >> 5) & 15;
	*d = fe & 31;
	*h = ho >> 11;
	*mi = (ho >> 5) & 63;
	*s = (ho & 31) * 2;
}

/* (TERMO) lo último que ha fallado al escribir (el código del sistema) */
static u32 ult_error;

/* Escribe (al final, o creándolo) y lo cierra: al cerrar, el sistema lo pasa
 * todo a la memoria (la entrada, la FAT y SYNCHRONIZE CACHE), así que un
 * apagado a lo bruto no pierde lo ya escrito. Desde la 0.8.9, el cierre dice
 * también si esa escritura a la memoria ha fallado. */
static u32 tics_escribe;	/* (TERMO) los tics que se han ido escribiendo (los parones) */
static int escribe_de_verdad(const char *ruta, const void *b, u32 n, int crear);

static int escribe(const char *ruta, const void *b, u32 n, int crear)
{
	u32 t = jg_tics();
	int r = escribe_de_verdad(ruta, b, n, crear);

	tics_escribe += jg_tics() - t;
	return r;
}

/* (TERMO 1.2.1) Lo que se añade al .LOG y al .CSV no se escribe en el
 * momento: se junta aquí y va a la memoria USB al final de cada segundo,
 * después de leer los sensores, mirar la parada de seguridad y pintar la
 * pantalla (pulso). El ASUS GL553VD (26/11/2026, Termómetro 1.2): cada línea
 * era abrir, añadir y cerrar (con SYNCHRONIZE CACHE) y la memoria tardaba
 * de 25 a 50 ms por orden: «leer y apuntar» llegó a 8 s, el contador de la
 * pantalla iba de 4 en 4 s y las líneas DATOS salían cada 2 a 6 s. Ahora,
 * una escritura por archivo y segundo; si una tanda tarda (más de 0,5 s),
 * la siguiente espera 4 veces lo que tardó (10 s como mucho), y la pantalla
 * y la parada de seguridad siguen cada segundo. Lo que se pierde si se va la
 * luz: lo del último segundo (o de esa espera). */
#define DIF_LOG 49152
#define DIF_CSV 16384
static char dif_log[DIF_LOG], dif_csv[DIF_CSV];
static u32 dif_log_n, dif_csv_n, dif_no_antes, dif_tandas, dif_lentas, dif_peor_ms;
static void no_se_guarda(const char *ruta, const char *por_que);
static const char *motivo_error(u32 e);

static void vuelca_diferido(int ya)
{
	u32 t0 = jg_tics(), d;

	if (!dif_log_n && !dif_csv_n)
		return;
	if (!ya && dif_no_antes && (int)(t0 - dif_no_antes) < 0 && dif_log_n < DIF_LOG / 2 && dif_csv_n < DIF_CSV / 2)
		return;
	if (dif_log_n && ruta_log[0] && !log_malo) {
		if (!escribe(ruta_log, dif_log, dif_log_n, 0) && !escribe(ruta_log, dif_log, dif_log_n, 1)) {
			log_malo = 1;
			no_se_guarda(ruta_log, motivo_error(ult_error));
		}
	}
	dif_log_n = 0;
	if (dif_csv_n && ruta_csv[0] && !csv_malo) {
		if (!escribe(ruta_csv, dif_csv, dif_csv_n, 0)) {
			csv_malo = 1;
			snprintf(aviso_archivo, sizeof aviso_archivo, "Se ha dejado de poder escribir %s: %s.", ruta_csv, motivo_error(ult_error));
			no_se_guarda(ruta_csv, motivo_error(ult_error));
		}
	}
	dif_csv_n = 0;
	d = jg_tics() - t0;
	dif_tandas++;
	if (d * 1000 / jg_hz > dif_peor_ms)
		dif_peor_ms = d * 1000 / jg_hz;
	if (d * 1000 / jg_hz >= 500) {
		dif_lentas++;
		dif_no_antes = jg_tics() + (d * 4 < 10 * jg_hz ? d * 4 : 10 * jg_hz);
	} else
		dif_no_antes = 0;
}

static void diferido(char *buf, u32 *n, u32 cabe, const void *b, u32 k)
{
	if (*n + k > cabe)
		vuelca_diferido(1);
	if (k > cabe)
		k = cabe;
	memcpy(buf + *n, b, k);
	*n += k;
}

/* (TERMO 1.2.1) el ritmo de la carga: cada cuánto llega cada segundo (la
 * pantalla y la parada de seguridad: mira_carga va en cada uno), cada
 * cuánto cada lectura de 2 s (las zonas, la potencia, el .CSV) y cada
 * cuánto se pinta. Para el INFORME.TXT y el puerto serie (las pruebas) */
struct ritmo { u32 n, tarde, peor, suma, ult; };
static struct ritmo ri_seg, ri_lee, ri_pin;
static int ri_de_fondo = -1;	/* la carga, de fondo (el núcleo de la 1.2.1): 1; 0 no; -1 no se sabe */
static void ritmo_cuenta(struct ritmo *r, u32 ahora, u32 tarde_tics)
{
	if (r->ult) {
		u32 d = ahora - r->ult;

		r->n++;
		r->suma += d;
		if (d > r->peor)
			r->peor = d;
		if (d > tarde_tics)
			r->tarde++;
	}
	r->ult = ahora;
}
static void ritmo_empieza(void)
{
	memset(&ri_seg, 0, sizeof ri_seg);
	memset(&ri_lee, 0, sizeof ri_lee);
	memset(&ri_pin, 0, sizeof ri_pin);
}
/* «cada 1,00 s (la más lenta, 1,02 s; ninguna de más de 1,5 s)» */
static void ritmo_texto(char *d, int cabe, const struct ritmo *r, const char *tope)
{
	u32 med = r->n ? r->suma * 100 / r->n / jg_hz : 0, pe = r->peor * 100 / jg_hz;

	if (!r->n) {
		snprintf(d, (size_t)cabe, "sin medir");
		return;
	}
	snprintf(d, (size_t)cabe, "cada %u,%02u s de media (la más lenta, %u,%02u s; %s%u de más de %s)", med / 100, med % 100, pe / 100, pe % 100,
		 r->tarde ? "" : "ninguna: ", r->tarde, tope);
}
static void pinta(void);

static int escribe_de_verdad(const char *ruta, const void *b, u32 n, int crear)
{
	u32 m = sc(SC_ABRIR, (u32)ruta, crear ? PARA_CREAR : PARA_ESCRIBIR, 0), r, c;

	if (ES_ERROR(m)) {
		ult_error = m;
		return 0;
	}
	if (!crear)
		sc(SC_MOVER, m, 0, 2);
	r = sc(SC_ESCRIBIR_ARCHIVO, m, (u32)b, n);
	c = sc(SC_CERRAR, m, 0, 0);
	if (r != n) {
		ult_error = ES_ERROR(r) ? r : 0xFFFFFFF8u;	/* (menos de lo pedido: lleno) */
		return 0;
	}
	if (ES_ERROR(c)) {
		ult_error = c;
		return 0;
	}
	return 1;
}

static const char *motivo_error(u32 e)
{
	switch (e) {
	case 0xFFFFFFFFu: return "la carpeta o la unidad no existe";
	case 0xFFFFFFFCu: return "error al escribir en el aparato (el USB no contesta o falla)";
	case 0xFFFFFFF8u: return "el disco está lleno";
	case 0xFFFFFFF5u: return "es de solo lectura (¿protegida contra escritura?)";
	case 0xFFFFFFF0u: return "sin permiso para escribir ahí";
	case 0xFFFFFFF9u: return "el sistema no lo acepta";
	default: return "error del sistema";
	}
}

/* (TERMO) no se puede guardar: se dice en grande, en la pantalla, en el
 * puerto serie y, si se puede, en el registro */
static int guarda_mal;
static char guarda_aviso[200];

static void no_se_guarda(const char *ruta, const char *motivo)
{
	if (guarda_mal)
		return;
	guarda_mal = 1;
	if (!ruta || !ruta[0])
		snprintf(guarda_aviso, sizeof guarda_aviso, "NO encuentro la memoria USB: el registro se pierde al apagar");
	else
		snprintf(guarda_aviso, sizeof guarda_aviso, "NO se puede guardar en %c: (%s): %s", ruta[0], ruta, motivo);
	serie("termo: %s\n", guarda_aviso);
}

/* lee lo escrito y lo compara (lo que ha llegado de verdad a la memoria: lo
 * del archivo no se queda en ningún sitio del sistema al cerrarlo) */
static int comprueba(const char *ruta, const char *b, u32 n)
{
	static char t[2048];
	u32 m = sc(SC_ABRIR, (u32)ruta, PARA_LEER, 0), hecho = 0, r;
	int bien = 1;

	if (ES_ERROR(m)) {
		ult_error = m;
		return 0;
	}
	while (hecho < n && bien) {
		u32 q = n - hecho > sizeof t ? (u32)sizeof t : n - hecho;

		r = sc(SC_LEER, m, (u32)t, q);
		if (ES_ERROR(r) || r != q || memcmp(t, b + hecho, q))
			bien = 0, ult_error = 0xFFFFFFFCu;
		hecho += q;
	}
	sc(SC_CERRAR, m, 0, 0);
	return bien;
}

typedef struct {
	u8 letra, clase, medio, solo;
	u32 total_k, libre_k, del_sistema;
	char nombre[41], etiqueta[12];
	u8 quitable, relleno[2];
} info_unidad;

static int existe(const char *r)
{
	u32 m = sc(SC_ABRIR, (u32)r, PARA_LEER, 0);

	if (ES_ERROR(m))
		return 0;
	sc(SC_CERRAR, m, 0, 0);
	return 1;
}

/* la unidad del sistema y la carpeta (arriba). En la memoria, nunca C: (el
 * disco del portátil ajeno) ni la M: (la de los módulos, de solo lectura) */
static void elige_carpeta(void)
{
	info_unidad u;
	int i;
	char r[24];

	for (i = 0; i < 40; i++) {
		memset(&u, 0, sizeof u);
		if (sc(43, (u32)i, (u32)&u, 0) != 1)
			break;
		if (u.del_sistema && u.letra)
			unidad = (char)u.letra;
	}
	if (!unidad || unidad == 'M')
		return;
	snprintf(r, sizeof r, "%c:\\TERMO.BIN", unidad);
	en_memoria = arranque_auto || (unidad != 'C' && existe(r));
	if (en_memoria) {
		snprintf(carpeta, sizeof carpeta, "%c:\\TERMO", unidad);
		return;
	}
	{
		char c[128];
		u32 n = sc(200, 0, 0, 0), k = 0;

		if (!ES_ERROR(n) && n > 0 && n < 1000)
			k = sc(200, 4, (u32)c, sizeof c);
		if (!ES_ERROR(k) && k > 1 && k < sizeof c - 8) {
			c[k] = 0;
			while (k > 1 && c[k - 1] == '\\')
				c[--k] = 0;
			snprintf(carpeta, sizeof carpeta, "%c:%s\\TERMO", unidad, c[0] == '\\' ? c : "\\USUARIOS");
		} else
			snprintf(carpeta, sizeof carpeta, "%c:\\TERMO", unidad);
	}
}

/* (TERMO) lo que ha encontrado el sistema al arrancar (las líneas «usb:»,
 * «disco», «unidades:» y «arranque:» del registro del arranque, la 224 con
 * 5): para verlo en la pantalla aunque no haya memoria USB donde guardar */
#define MAX_HALLADO 14
static char hallado[MAX_HALLADO][100];
static int nhallado;

static void lee_hallado(void)
{
	static char r[12001];
	u32 n = sc(SC_CARGA, 5, (u32)r, sizeof r - 1), i = 0;

	if (ES_ERROR(n) || n == 0 || n >= sizeof r)
		return;
	r[n] = 0;
	while (i < n && nhallado < MAX_HALLADO) {
		u32 j = i, k;

		while (j < n && r[j] != '\r' && r[j] != '\n')
			j++;
		if (!strncmp(r + i, "usb:", 4) || !strncmp(r + i, "unidades:", 9) || !strncmp(r + i, "disco ", 6) || !strncmp(r + i, "arranque:", 9) || !strncmp(r + i, "pci:", 4)) {
			/* (TERMO 1.1.1) en UTF-8: \ARRANQUE.TXT puede traer letras del DOS
			 * (CP437: «l\xA1nea», en el ASUS GL553VD), que en INFORME.TXT
			 * salían como un octeto suelto y rompían el UTF-8 */
			static const char *const cp[] = { "\xC3\xBC", "\xC3\xA9", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "\xC3\x89" };	/* 81h ü, 82h é; 90h É */
			static const char *const cpa[] = { "\xC3\xA1", "\xC3\xAD", "\xC3\xB3", "\xC3\xBA", "\xC3\xB1", "\xC3\x91", 0, 0, "\xC2\xBF", 0, 0, 0, 0, "\xC2\xA1" };	/* A0h-ADh */
			u32 q = 0, x;

			for (x = i; x < j && q < sizeof hallado[0] - 4; x++) {
				u8 c = (u8)r[x];
				const char *u = 0;

				if (c < 0x80) {
					hallado[nhallado][q++] = (char)c;
					continue;
				}
				if (c >= 0xC2 && c <= 0xDF && x + 1 < j && ((u8)r[x + 1] & 0xC0) == 0x80) {	/* (ya era UTF-8) */
					hallado[nhallado][q++] = (char)c;
					hallado[nhallado][q++] = r[++x];
					continue;
				}
				if (c >= 0x81 && c <= 0x90)
					u = cp[c - 0x81];
				else if (c >= 0xA0 && c <= 0xAD)
					u = cpa[c - 0xA0];
				if (u) {
					hallado[nhallado][q++] = u[0];
					hallado[nhallado][q++] = u[1];
				} else
					hallado[nhallado][q++] = '?';
			}
			(void)k;
			hallado[nhallado++][q] = 0;
		}
		while (j < n && (r[j] == '\r' || r[j] == '\n'))
			j++;
		i = j;
	}
}

/* (TERMO) el disco C: del equipo no se toca nunca (es el del portátil
 * ajeno; la memoria arranca con «escritura=usb»): si no se puede guardar en
 * la memoria, se dice, y lo que ha encontrado el sistema sale en pantalla */
/* escribe, lee de vuelta y borra un archivo de 4 K en la carpeta: así se
 * sabe al empezar si lo que se guarde va a llegar de verdad */
static int prueba_escritura(void)
{
	static char b[4096];
	char r[120];
	u32 i;
	int bien;

	for (i = 0; i < sizeof b; i++)
		b[i] = (char)('A' + (i * 7 + i / 512) % 26);
	snprintf(r, sizeof r, "%s\\PRUEBA.TMP", carpeta);
	bien = escribe(r, b, sizeof b, 1) && comprueba(r, b, sizeof b);
	sc(SC_BORRAR, (u32)r, 0, 0);
	serie("termo: prueba de escritura en %s: %s%s\n", carpeta, bien ? "bien (escrita y leída de vuelta)" : "MAL: ", bien ? "" : motivo_error(ult_error));
	return bien;
}

static void prepara_archivos(void)
{
	u32 a, me, d, h, mi, s;

	lee_hallado();
	if (!carpeta[0] || (en_memoria && unidad == 'C')) {
		/* (TERMO) no está la memoria USB (X:): el registro, a C: si es FAT */
		no_se_guarda("", "no encuentro la memoria USB: el registro se perdería al apagar");
		{
			snprintf(aviso_archivo, sizeof aviso_archivo, "No se guarda nada: encima, lo que ha encontrado el sistema (haga una foto).");
			csv_malo = 1;
			return;
		}
	}
	sc(SC_HACER_CARPETA, (u32)carpeta, 0, 0);
	if (!prueba_escritura()) {
		char c0[96];

		snprintf(c0, sizeof c0, "%s", carpeta);
		no_se_guarda(c0, motivo_error(ult_error));
		carpeta[0] = 0;
		{
			snprintf(aviso_archivo, sizeof aviso_archivo, "No se guarda nada: encima, lo que ha encontrado el sistema (haga una foto).");
			csv_malo = 1;
			log_malo = 1;
			return;
		}
	}
	fecha_hora(&a, &me, &d, &h, &mi, &s);
	snprintf(ruta_csv, sizeof ruta_csv, "%s\\%02u%02u%02u%02u.CSV", carpeta, me, d, h, mi);
	snprintf(ruta_txt, sizeof ruta_txt, "%s\\%02u%02u%02u%02u.TXT", carpeta, me, d, h, mi);
	mira_anterior();
	snprintf(ruta_log, sizeof ruta_log, "%s\\%02u%02u%02u%02u.LOG", carpeta, me, d, h, mi);
	{
		char t[120];
		int n = snprintf(t, sizeof t, "Registro del Termometro de Hispa-Pentium, %02u/%02u/%04u\r\n", d, me, a);

		if (!escribe(ruta_log, t, (u32)n, 1)) {
			log_malo = 1;
			no_se_guarda(ruta_log, motivo_error(ult_error));
			return;
		}
	}
	log_arranque();
	if (guarda_aviso[0])
		log_escribe("SUCESO", guarda_aviso);
	{
		int i;

		for (i = 0; i < nhallado; i++)
			log_escribe("HALLADO", hallado[i]);
	}
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
	if (csv_listo) {
		/* (TERMO) han llegado sensores nuevos (los núcleos de la carga): otra
		 * línea de cabecera, con las columnas de ahora */
		diferido(dif_csv, &dif_csv_n, DIF_CSV, b, (u32)m);	/* (1.2.1: en su sitio, tras lo ya juntado) */
		return;
	}
	if (!escribe(ruta_csv, b, (u32)m, 1)) {
		csv_malo = 1;
		snprintf(aviso_archivo, sizeof aviso_archivo, "No se puede escribir %s: %s.", ruta_csv, motivo_error(ult_error));
		no_se_guarda(ruta_csv, motivo_error(ult_error));
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
	diferido(dif_csv, &dif_csv_n, DIF_CSV, b, (u32)m);	/* (1.2.1: a la memoria USB, al final del segundo) */
}

/* ---------------------------------------------------------------- el registro de sucesos (.LOG)
 * X:\TERMO\MMDDHHMM.LOG: todo lo que pasa, para saber, si el equipo se apaga
 * de golpe (el corte térmico de la placa), qué hacía y a qué temperatura.
 * Lo de cada segundo se escribe de una vez (1.2.1: vuelca_diferido, antes
 * cada línea) abriendo, añadiendo y cerrando el archivo: la FAT del núcleo
 * escribe en el momento (fat.cas: nada se queda en la memoria) y al cerrar
 * queda el tamaño en la carpeta; si se va la luz, como mucho se pierde lo
 * del último segundo. Las líneas:
 *   hh:mm:ss ARRANQUE ...   lo del arranque (X:\ARRANQUE.TXT del núcleo)
 *   hh:mm:ss SUCESO ...     lo que pasa (la prueba, los estados, avisos)
 *   hh:mm:ss DATOS fase=... max=...; sensor=...; ...   cada 2 s
 *   hh:mm:ss CIERRE ...     el Termómetro se ha cerrado bien
 */
static void log_escribe(const char *clase, const char *texto)
{
	char b[1700], l[1650];	/* (el RESULTADO entero: salía cortado) */
	u32 a, me, d, h, mi, se;
	int n;

	if (!ruta_log[0] || log_malo)
		return;
	fecha_hora(&a, &me, &d, &h, &mi, &se);
	llano(l, texto, sizeof l);
	n = snprintf(b, sizeof b, "%02u:%02u:%02u %s %s\r\n", h, mi, se, clase, l);
	if (n > (int)sizeof b - 1)
		n = (int)sizeof b - 1;
	diferido(dif_log, &dif_log_n, DIF_LOG, b, (u32)n);	/* (1.2.1: a la memoria USB, al final del segundo) */
}

static void suceso_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void suceso_log(const char *fmt, ...)
{
	char t[1600];
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
	if (hay_zonas && za.t.asus_hay) {
		char v1[16], v2[16];

		m += snprintf(b + m, sizeof b - (size_t)m, " | vent_cpu=%s vent_gpu=%s", vent_asus(v1, za.t.asus_vent_cpu), vent_asus(v2, za.t.asus_vent_gpu));
	}
	if (ocup_pct >= 0)
		m += snprintf(b + m, sizeof b - (size_t)m, " | ocupado=%d%%", ocup_pct);
	if (c0_pct >= 0)
		m += snprintf(b + m, sizeof b - (size_t)m, " | C0=%d%%", c0_pct);
	if (c6_pct >= 0)
		m += snprintf(b + m, sizeof b - (size_t)m, " | C6=%d%%", c6_pct);
	/* (TERMO) los segundos desde que se abrió, por el reloj del sistema: la
	 * hora de la línea es la de la CMOS, de 2 en 2 s (en el MSI GT80, con
	 * un apunte cada 3 s, salían dos líneas con la misma hora) */
	m += snprintf(b + m, sizeof b - (size_t)m, " | seg=%u", (jg_tics() - t_inicio) / jg_hz);
	if (smi_hay == 1)
		m += snprintf(b + m, sizeof b - (size_t)m, " | smi=%u", smi_dos);
	if (m < (int)sizeof b - 1)
		m += po_datos(b + m, (int)sizeof b - m);	/* (TERMO) los vatios, los frenos y los C-states (potencia.c) */
	if (parado_dos)
		m += snprintf(b + m, sizeof b - (size_t)m, " | parado=%d ms", parado_dos);
	if (hay_zonas && za.t.bat_mv > 0 && za.t.bat_mv < 100000)
		m += snprintf(b + m, sizeof b - (size_t)m, " | V=%u,%02u", za.t.bat_mv / 1000, za.t.bat_mv % 1000 / 10);
	if (hay_bateria && en.pct <= 100)
		m += snprintf(b + m, sizeof b - (size_t)m, " | bateria=%u%% %s", en.pct, en.ca == 1 ? "enchufado" : en.ca == 0 ? "sin enchufar" : "");
	if (hay_bateria && hay_zonas && za.t.bat_mw != NO && (za.t.bat_estado & 3)) {
		char w[16];

		snprintf(b + m, sizeof b - (size_t)m, " | %s=%s W", za.t.bat_estado & 1 ? "gasta" : "carga", vatios(w, za.t.bat_mw));
	}
	log_escribe("DATOS", b);
}

/* (TERMO) por qué una zona o un ventilador no da nada: lo que dijo ACPICA
 * (ACPI_STATUS, acpica/include/acexcep.h) */
static const char *texto_acpi(u32 e, char *d)
{
	static const struct { u32 c; const char *t; } tabla[] = {
		{ 0x0005, "AE_NOT_FOUND: la BIOS no tiene ese método" },
		{ 0x0006, "AE_NOT_EXIST" },
		{ 0x0011, "AE_TIME: tiempo agotado (¿el controlador embebido no contesta?)" },
		{ 0x001A, "AE_NO_HANDLER: una región que no se sabe leer" },
		{ 0x0016, "AE_AML_MUTEX_NOT_ACQUIRED" },
		{ 0x0010, "AE_NOT_ACQUIRED: un Mutex que no se suelta" },
		{ 0x0001, "AE_ERROR" },
		{ 0x000E, "AE_TYPE: no devuelve un número" },
		{ 0x3001, "AE_AML_BAD_OPCODE" },
		{ 0x3003, "AE_AML_OPERAND_TYPE" },
		{ 0x3005, "AE_AML_UNINITIALIZED_LOCAL" },
		{ 0x3006, "AE_AML_UNINITIALIZED_ARG" },
		{ 0x3007, "AE_AML_UNINITIALIZED_ELEMENT: usa un dato sin valor (lo suele poner un _INI o un aviso de la BIOS que en el modo mínimo no corre; el método, en \\ACPI.TXT)" },
		{ 0x3009, "AE_AML_REGION_LIMIT" },
		{ 0x300E, "AE_AML_NAME_NOT_FOUND: llama a algo que no está" },
		{ 0x3010, "AE_AML_INVALID_SPACE_ID: una región de un tipo que no se sabe leer" },
		{ 0x3012, "AE_AML_NO_RETURN_VALUE: no devuelve nada" },
		{ 0x3021, "AE_AML_LOOP_TIMEOUT: un bucle que no acaba (esperando al EC)" },
	};
	u32 i;

	for (i = 0; i < sizeof tabla / sizeof tabla[0]; i++)
		if (tabla[i].c == e)
			return tabla[i].t;
	snprintf(d, 40, "ACPICA %04Xh", e);
	return d;
}

static void motivo_zona(char *d, int cabe, const struct ta_zona *z)
{
	char x[40];

	if (z->estado)
		snprintf(d, (size_t)cabe, "_TMP falla: %s", texto_acpi(z->estado, x));
	else if (z->crudo >= 0)
		snprintf(d, (size_t)cabe, "_TMP devuelve %d (décimas de kelvin: %d,%d °C), que no puede ser", z->crudo, (z->crudo - 2732) / 10, abs(z->crudo - 2732) % 10);
	else
		snprintf(d, (size_t)cabe, "_TMP no devuelve un número");
}

/* los cambios: de estado de cada sensor, del ventilador, de la batería */
static int est_antes[MAXS], vent_antes[TA_VENT], ca_antes = -1, pct_antes = -1, vistos_antes[MAXS];

static void mira_cambios(void)
{
	char g[24], g2[24];
	static int zonas_dichas;
	int k;
	u32 i;

	if (fecha_mala && cuenta_s == 2)
		suceso_log("AVISO: la fecha del equipo parece mal (año %d: ¿la pila del reloj?); %s", fecha_mala, pidio_hora ? "se pide la hora por Internet (HORARED /AHORA)" : "sin red no se puede poner en hora");
	if (fecha_mala && cuenta_s % 10 == 0) {
		u32 a, me, d, h, mi, se;

		fecha_hora(&a, &me, &d, &h, &mi, &se);
		if (a >= 2025) {
			suceso_log("la hora ya está bien (por Internet): %02u/%02u/%04u %02u:%02u", d, me, a, h, mi);
			fecha_mala = 0;
		}
	}
	if (!zonas_dichas && cpu.msr_texto[0] && cuenta_s >= 2) {
		suceso_log("procesador: %s%s", cpu.msr_texto, cpu.aproximada ? ". La medida del sensor de este Atom es aproximada (sobre todo en frío): si sale por debajo de 25-30 °C con el equipo encendido, no es de fiar" : "");
		cpu.msr_texto[0] = 0;
	}
	if (hay_zonas && !zonas_dichas) {
		zonas_dichas = 1;
		suceso_log("ACPI (ACPI.BIN): %u zonas térmicas, %u ventiladores; equipo «%s %s», placa «%s %s», BIOS «%s»", za.t.nzonas, za.t.nvent,
			   za.t.fabricante, za.t.producto, za.t.placa_fab, za.t.placa, za.t.bios);
	}
	for (k = 0; k < ns && cuenta_s == 4; k++)
		if (S[k].clase == CL_GPU && S[k].t == NO)
			suceso_log("sensor «%s»: sin dato: %s", S[k].nombre, nv_estado((u32)gpu_estado[k]));
	/* (TERMO) sin batería (la Medion S2217, con la suya muerta: se apaga al
	 * desenchufar): que se diga, en el .LOG y en el informe. A los 20 s, con
	 * la primera lectura de ACPI.BIN ya hecha */
	if (cuenta_s == 20 && !sin_bat_texto[0] && (!hay_bateria || en.pct > 100)) {
		struct energia e2;
		int r = sc(SC_ENERGIA, (u32)&e2, 0, 0) == 1;

		if (hay_bateria)
			snprintf(sin_bat_texto, sizeof sin_bat_texto, "está puesta, pero no dice cuánto le queda (¿gastada o muerta?)");
		else
			snprintf(sin_bat_texto, sizeof sin_bat_texto, "no la encuentro o no está (va con el cable): %s",
				 !r ? "el servicio de ACPI no dice nada de ella" : e2.fase == 2 ? "el equipo no declara ninguna batería (ACPI)" :
				 e2.fase == 3 ? "ACPI no va en este equipo" : e2.fase == 0 ? "ACPI aún no ha acabado de leerla" : "el equipo la declara, pero dice que no está puesta (_STA)");
		suceso_log("batería: %s", sin_bat_texto);
	}
	if (cuenta_s == 4) {	/* (TERMO) (NVIDIA) cada gráfica y cada puerto PCI Express, con lo del ACPI */
		char l[900];

		if (hay_zonas)
			nv_acpi(&NV, &za.t);
		if (!NV.n)
			suceso_log("gráficas NVIDIA: no hay gráficas NVIDIA (ninguna en el PCI, mirados todos los buses)");
		for (k = 0; k < NV.n; k++) {
			nv_describe(&NV, k, l, sizeof l);
			suceso_log("gráficas NVIDIA: %s", l);
		}
		for (k = 0; k < NV.np; k++) {
			nv_describe_puerto(&NV, k, l, sizeof l);
			suceso_log("gráficas NVIDIA: %s", l);
		}
	}
	if (hay_zonas && za.t.asus_hay && cuenta_s == 6)
	{
		char v1[16], v2[16];

		suceso_log("ASUS (WMI, DSTS: solo leer): ventilador CPU %s, ventilador GPU %s (DSTS: %08Xh, %08Xh), temperatura %d décimas%s", vent_asus(v1, za.t.asus_vent_cpu),
			   vent_asus(v2, za.t.asus_vent_gpu), za.t.asus_crudo_cpu, za.t.asus_crudo_gpu, za.t.asus_temp == NO ? -1 : za.t.asus_temp,
			   za.t.asus_escribe ? "; el AML quiso escribir en el EC al leerlo: no se fía" : "");
	}
	if (hay_zonas && (cuenta_s == 10 || cuenta_s == 120) && za.t.ec_hay)
		suceso_log("ACPI: el controlador embebido (EC), al leer las zonas: %u lecturas, %u sin respuesta a tiempo, %u escrituras del AML no hechas (solo lectura); una lectura entera tarda %u ms",
			   za.t.ec_lecturas, za.t.ec_fallos, za.t.ec_ignoradas, za.t.ms_lectura);
	else if (hay_zonas && cuenta_s == 10)
		suceso_log("ACPI: el equipo no declara controlador embebido (EC); una lectura de las zonas tarda %u ms", za.t.ms_lectura);
	for (k = 0; hay_zonas && k < ns; k++) {
		const struct ta_zona *z;

		if (S[k].clase != CL_ACPI || S[k].zona >= TA_ZONAS)
			continue;
		z = &za.t.zona[S[k].zona];
		if (z->tmp == NO && (S[k].estado_dicho != z->estado + 1 || cuenta_s == 4)) {
			char m[160];

			if (S[k].estado_dicho != z->estado + 1) {
				motivo_zona(m, sizeof m, z);
				suceso_log("sensor «%s»: sin dato (--): %s", S[k].nombre, m);
			}
			S[k].estado_dicho = z->estado + 1;
		}
	}
	if (!hay_zonas && cuenta_s == 20)
		suceso_log("AVISO: el servicio de ACPI no da las zonas térmicas (¿arrancado con «sinacpi», o ACPI.BIN no va?)");
	for (k = 0; k < ns; k++) {
		int e = estado_de(S[k].t, S[k].limite);

		/* (TERMO) con margen al bajar: de un estado al de debajo solo
		 * cuando está 3 °C por debajo del umbral (en el Monovate 14.1, a
		 * 59-63 °C, salía cada 2-4 s «de bien a templado» y «de templado a
		 * bien»); al subir, en cuanto lo pasa */
		if (e != E_NADA && est_antes[k] != E_NADA && e < est_antes[k] && S[k].t != NO) {
			int e3 = estado_de(S[k].t + 30, S[k].limite);

			if (e3 > e)
				e = e3 < est_antes[k] ? e3 : est_antes[k];
		}
		/* (los núcleos de los demás procesadores solo se leen en la carga: no es un fallo) */
		if (S[k].t == NO && vistos_antes[k] && !(S[k].clase == CL_CPU && (S[k].zona & 0x10000)))
			suceso_log("AVISO: el sensor «%s» ha dejado de responder", S[k].nombre);
		if (S[k].t != NO && !vistos_antes[k]) {
			suceso_log("sensor «%s»: %s (límite %s)", S[k].nombre, grados(g, S[k].t), S[k].limite == NO ? "no dice" : grados(g2, S[k].limite));
			if (en_reposo() && !ve_creible(S[k].t, S[k].limite) && !S[k].no_fiable) {
				S[k].no_fiable = VE_NF_IMPOSIBLE;
				suceso_log("AVISO: «%s» da %s estando en reposo (fuera de lo posible o por encima de su límite): lectura no fiable; no cuenta para la parada ni para el veredicto",
					   S[k].nombre, grados(g, S[k].t));
			}
		}
		vistos_antes[k] = S[k].t != NO;
		if (e != E_NADA && est_antes[k] != E_NADA && e != est_antes[k])
			suceso_log("%s: de «%s» a «%s» (%s)", S[k].nombre, nombre_estado[est_antes[k]], nombre_estado[e], grados(g, S[k].t));
		if (e != E_NADA)
			est_antes[k] = e;
	}
	for (i = 0; hay_zonas && i < za.t.nvent && i < TA_VENT; i++) {
		struct ta_vent *v = &za.t.vent[i];
		int x = v->velocidad != NO ? v->velocidad / 500 + 10 : v->encendido;

		if (x != vent_antes[i]) {
			char m[200], e1[40], e2[40];

			if (v->velocidad == NO && v->encendido < 0)
				snprintf(m, sizeof m, "sin datos (%s: _FST %s; _PSC %s)", v->nombre, v->estado_fst ? texto_acpi(v->estado_fst, e1) : "sin r.p.m.",
					 v->estado_psc ? texto_acpi(v->estado_psc, e2) : "no dice");
			else if (v->velocidad != NO)
				snprintf(m, sizeof m, "%d r.p.m.", v->velocidad);
			else
				snprintf(m, sizeof m, "%s", v->encendido == 1 ? "en marcha" : "parado");
			suceso_log("ventilador %u: %s", i + 1, m);
		}
		vent_antes[i] = x;
	}
	/* (TERMO) ACPI.BIN lee la batería cada 10 s: si en 40 s no ha leído
	 * otra vez, lo que se ve de ella es viejo (el Monovate 14.1, 23/11/2026:
	 * «78 % sin enchufar, 15,8 W» de principio a fin, enchufado o no) */
	if (hay_bateria) {
		static u32 lect_antes, lect_s;
		static int lect_dicho;

		if (en.lecturas != lect_antes || !lect_s) {
			if (lect_dicho)
				suceso_log("batería: el servicio de ACPI vuelve a leerla (lectura n.º %u)", en.lecturas);
			lect_antes = en.lecturas, lect_s = cuenta_s, lect_dicho = 0;
		} else if (!lect_dicho && cuenta_s - lect_s >= 40) {
			lect_dicho = 1;
			suceso_log("AVISO: el servicio de ACPI no ha vuelto a leer la batería en %u s (sigue en la lectura n.º %u): lo que se ve de ella puede ser viejo",
				   cuenta_s - lect_s, en.lecturas);
		}
	}
	if (hay_bateria) {
		if ((int)en.ca != ca_antes)
			suceso_log("batería: %s", en.ca == 1 ? "enchufado a la corriente" : en.ca == 0 ? "SIN enchufar (con la batería)" : "no se sabe si está enchufado");
		ca_antes = (int)en.ca;
		if (en.pct <= 100 && (pct_antes < 0 || abs((int)en.pct - pct_antes) >= 10)) {
			suceso_log("batería al %u %%", en.pct);
			pct_antes = (int)en.pct;
		}
		if (en.ca == 0 && en.pct <= 100) {
			if (desc_pct0 < 0 || desc_enchufado || (int)en.pct > desc_pct1) {	/* (empieza, otra vez sin enchufar o ha subido: de nuevo) */
				desc_pct0 = (int)en.pct, desc_s0 = cuenta_s, desc_carga = 0, desc_mw_max = -1;
				desc_mw = -1;
			}
			desc_pct1 = (int)en.pct, desc_s1 = cuenta_s, desc_enchufado = 0;
			if (en_carga())
				desc_carga = 1;
			if (hay_zonas && za.t.bat_mw != NO && (za.t.bat_estado & 1)) {
				desc_mw = za.t.bat_mw;
				if (za.t.bat_mw > desc_mw_max)
					desc_mw_max = za.t.bat_mw;
				if (cuenta_s % 60 == 0) {
					char w[16];

					suceso_log("batería: gasta unos %s W (ACPI, _BST), al %u %%", vatios(w, za.t.bat_mw), en.pct);
				}
			}
		} else if (en.ca == 1)
			desc_enchufado = 1;	/* (lo medido se queda para el informe hasta que se desenchufe otra vez) */
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
	char mejor[16] = "", r[64], *b, *p, *ult = 0;
	struct entrada e;
	u32 m, n, largo, desde;
	char hora[12] = "", fase_a[16] = "", maxi[16] = "", h60[12] = "";
	int lim_apagado = 1000;		/* (el límite más bajo de los del procesador; 100 °C si no se sabe) */
	int lmin = NO, lmax = NO, cerrado = 0, k, nlin = 0;
	const char *lineas[40];

	m = sc(SC_ABRIR, (u32)carpeta, PARA_LEER, 0);
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
	snprintf(r, sizeof r, "%s\\%s", carpeta, mejor);
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
	for (k = 0; k < ns; k++)
		if (S[k].clase == CL_CPU && S[k].limite != NO && S[k].limite < lim_apagado)
			lim_apagado = S[k].limite;
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
			 (lmax != NO && lmax >= lim_apagado - 200) ? " Estaba muy caliente: seguramente lo apagó la protección térmica; conviene revisar el ventilador, el polvo y la pasta térmica." :
			 lmax != NO ? " Se apagó con el procesador por debajo de su límite: el apagado no parece por temperatura; conviene revisar la alimentación (cargador, batería, regulador de tensión de la placa)." : "");
	}
	ver_anterior = 1;
	free(b);
}

/* la pantalla (F12), en .BMP de 24 bits */
static void captura_en(const char *nombre);
static u8 *bmp_pend;		/* (PARPADEO/TERMO) la pantalla que se está guardando */
static u32 bmp_total, bmp_hecho;
static char bmp_ruta[120];
static char informe_dicho[200];

/* cada pulso: otro trozo de la pantalla que se está guardando */
static void bmp_trozo(void)
{
	u32 k;

	if (!bmp_pend)
		return;
	/* (abriendo, añadiendo y cerrando cada trozo, como el .LOG: si se va la
	 * luz a medias, la FAT queda sana y la imagen, cortada) */
	k = bmp_total - bmp_hecho > 32768 ? 32768 : bmp_total - bmp_hecho;
	if (!escribe(bmp_ruta, bmp_pend + bmp_hecho, k, 0)) {
		/* (TERMO) no cabe (una pantalla de 1920 x 1080 son 6 MB): lo
		 * escrito de ella, fuera, para que el .LOG, el .CSV y el
		 * INFORME.TXT, que importan más, sigan teniendo sitio (en la
		 * memoria de 32 MB de las pruebas, la media pantalla de F12 y luego
		 * INFORME.BMP la llenaban: ni INFORME.TXT ni la prueba de escritura) */
		sc(SC_BORRAR, (u32)bmp_ruta, 0, 0);
		free(bmp_pend);
		bmp_pend = 0;
		serie("termo: captura en %s: no cabe en la memoria (%s): borrada\n", bmp_ruta, motivo_error(ult_error));
		suceso_log("la pantalla no cabe en la memoria (%s): no se guarda; lo demás, sí", motivo_error(ult_error));
		snprintf(aviso_archivo, sizeof aviso_archivo, "La pantalla no cabe en la memoria: no se guarda (lo demás, sí).");
		return;
	}
	bmp_hecho += k;
	if (bmp_hecho >= bmp_total) {
		free(bmp_pend);
		bmp_pend = 0;
		if (informe_dicho[0] && busca(bmp_ruta, "INFORME.BMP"))	/* (lo del informe se queda a la vista) */
			snprintf(aviso_archivo, sizeof aviso_archivo, "%s (y la pantalla, INFORME.BMP)", informe_dicho);
		else
			snprintf(aviso_archivo, sizeof aviso_archivo, "Pantalla guardada.");
	}
}

static void captura(void)
{
	captura_en(0);
}

/* la pantalla, en .BMP: con nombre (INFORME.BMP) o con la hora */
static void captura_en(const char *nombre)
{
	char ruta[120];
	u8 cab[54], *fila;
	u32 a, me, d, h, mi, s, m, tf, total;
	int y, x;

	if (!carpeta[0] || csv_malo)
		return;
	fecha_hora(&a, &me, &d, &h, &mi, &s);
	if (nombre)
		snprintf(ruta, sizeof ruta, "%s\\%s", carpeta, nombre);
	else
		snprintf(ruta, sizeof ruta, "%s\\%02u%02u%02u%02u.BMP", carpeta, d, h, mi, s);
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
	if (bmp_pend)
		return;		/* (ya se está escribiendo otra) */
	if (!escribe(ruta, cab, 54, 1))
		return;		/* (la cabecera: el archivo, hecho; lo demás, poco a poco) */
	m = 0;
	/* (TERMO) entero en la memoria y de una vez (a trozos de 256 K): fila a
	 * fila, en el Toshiba NB250, INFORME.BMP tardaba 9 s y paraba las lecturas */
	fila = (u8 *)malloc(total);
	if (fila) {
		u32 hecho = 0;

		memcpy(fila, cab, 54);
		memset(fila + 54, 0, total - 54);
		for (y = LZ.al - 1; y >= 0; y--) {
			const u32 *p = LZ.p + y * LZ.paso;
			u8 *f = fila + 54 + (u32)(LZ.al - 1 - y) * tf;

			for (x = 0; x < LZ.an; x++) {
				f[x * 3] = (u8)p[x];
				f[x * 3 + 1] = (u8)(p[x] >> 8);
				f[x * 3 + 2] = (u8)(p[x] >> 16);
			}
		}
		/* se escribe poco a poco, 32 K en cada pulso (bmp_trozo): la
		 * memoria USB va a 150 K/s y escribirla de una vez paraba 10 s las
		 * lecturas (el HP Mini y el Toshiba) */
		(void)hecho;
		bmp_pend = fila;
		bmp_total = total;
		bmp_hecho = 54;		/* (la cabecera ya está) */
		snprintf(bmp_ruta, sizeof bmp_ruta, "%s", ruta);
		snprintf(aviso_archivo, sizeof aviso_archivo, "Guardando la pantalla en %s...", ruta);
		serie("termo: captura en %s (poco a poco)\n", ruta);
		return;
	}
	(void)m;
	snprintf(aviso_archivo, sizeof aviso_archivo, "Pantalla guardada en %s (solo la cabecera: sin memoria).", ruta);
	serie("termo: captura en %s\n", ruta);
}

/* ---------------------------------------------------------------- la prueba */
enum { F_REPOSO, F_CARGA, F_ENFRIA, F_RESULTADO };
static int fase = F_REPOSO, minutos = 10, dura_s, enfria_max = 180;

static int en_carga(void)
{
	return fase == F_CARGA;
}
static u32 t_fase;
static int t_ini, pico, por_seguridad, por_esc, seg_carga;
static struct ve_prueba VP;	/* (veredicto.c) con qué sensor se mide y qué ha pasado */
static int hubo_prueba;
static char resumen[1200], motivo[160];
static int hija = -1, solo_aqui;
static u32 espera_acpi;	/* (TERMO 1.2) Calentar pedido, esperando a ACPI: desde cuándo (0: no) */
/* (TERMO 1.2) la carga: la ligera (x87, la de siempre) o la fuerte (SSE2,
 * para llegar a los vatios que deja la BIOS, PL1 y PL2, y ver si la fuente o
 * el cargador frenan al equipo); la fuerte, solo si el núcleo ha encendido el
 * SSE2 (la 224 con EBX = 8: la base 0.8.12) */
static int carga_fuerte, fuerte_puede, sse_nucleo;
static int ap_fuertes, ap_cache_apagada;	/* (de los demás procesadores: los que hacen la fuerte; los que llegaron con la caché apagada, CR0.CD) */
static int puede_enviar, puede_terminal, enviando;	/* (TERMO) */
static u32 t_envio;
static char aviso_envio[820];
/* (TERMO 1.2) «Enviar a Carlos» (tecla E): cómo ha ido, en un cartel con
 * letras («ENVIANDO», «ENVIADO», «NO ENVIADO»: nunca solo el color) hasta la
 * siguiente tecla; y en el .LOG y en INFORME.TXT. envio_ok: -1 sin enviar
 * en este arranque, 1 enviado, 0 no */
static char envio_fin[820], envio_hora[6];
static int envio_ok = -1, envio_cartel;
static void envio_acaba(const char *t);

/* ---------------------------------------------------------------- (TERMO 1.1.1) las pruebas de este arranque
 * Carlos, 25/11/2026, en un ASUS GL553VD (Calentar enchufado, Rendimiento,
 * la RAM y Calentar con la batería, en el mismo arranque): «después de
 * finalizar una prueba, al comenzar otra se queda el resultado de la primera
 * en pantalla». El cuadro del resultado iba con «fase == F_RESULTADO», que
 * solo cambiaba Calentar: Rendimiento y la RAM empezaban y acababan debajo
 * de él, sin pintarse; y el INFORME.TXT de las dos de Calentar salía
 * mezclado (las cuentas de la batería, los frenos y la potencia, de todo el
 * arranque). Ahora cada prueba es una ficha: cuándo, qué, su veredicto en
 * una línea y su sección del informe, guardada tal cual al acabar; lo
 * contado (frenos, vatios, parones, batería) vuelve a 0 para la siguiente.
 * En la pantalla: la que está en marcha, en la franja de abajo; al acabar,
 * su resultado y la lista de todas (tecla P: verla otra vez) */
#define MAX_HECHAS 24
enum { PR_CALENTAR, PR_RENDIMIENTO, PR_RAM, PR_DISCOS };
struct hecha {
	int tipo, en_marcha, num;
	int fuerte;		/* (TERMO 1.2, Calentar) con la carga fuerte (SSE2) */
	/* (TERMO 1.2) la alimentación, con TODAS las lecturas de la carga (cada
	 * 2 s): los segundos enchufado (1) y con la batería (0); la última vista */
	int seg_ca[2], ca_ult;
	char dura[12];
	int ca, ca_cambio;	/* (Calentar) 1 enchufado, 0 con la batería, -1 no se sabe; si ha cambiado en medio */
	char hora[6], hora_fin[6], hms[10];
	char nombre[56];
	char linea[300];	/* el veredicto, en una línea */
	int b_ini, b_len;	/* su sección del INFORME.TXT, en archivo[] (-1 ninguna, -2 no cabía) */
	/* (Calentar) para la tabla que las compara */
	int t_ini, t_max, mw, mw_max, mhz, pct_fr, seg, bat_ini, bat_fin, bat_mw, seguridad, esc;
	char ritmo[700];	/* (TERMO 1.2.1, Calentar) cada cuánto la pantalla, la parada de seguridad y las lecturas */
};
static struct hecha HE[MAX_HECHAS];
static int nhechas, hechas_total, he_cal = -1, he_mide = -1, he_ram = -1, he_vista = -1, ver_cuadro, discos_en_lista;
static char archivo[100000];	/* las secciones de cada prueba, una detrás de otra */
static int narchivo;
static u32 cuadro_dicho;	/* (para las pruebas: lo que enseña el cuadro, por el puerto serie, una vez) */
static void reinicia_durante(void);
static void reinicia_despues(void);

static void he_corre(int *i)
{
	*i = *i > 0 ? *i - 1 : -1;
}

static int he_nueva(int tipo, const char *nombre, int cierra)
{
	struct hecha *h;
	u32 a, me, d, hh, mi, se;
	char t[200];

	if (nhechas == MAX_HECHAS) {	/* (la más vieja sale de la lista; su sección se queda en el informe) */
		memmove(HE, HE + 1, sizeof HE[0] * (MAX_HECHAS - 1));
		nhechas--;
		he_corre(&he_cal), he_corre(&he_mide), he_corre(&he_ram), he_corre(&he_vista);
	}
	h = &HE[nhechas];
	memset(h, 0, sizeof *h);
	h->tipo = tipo;
	h->en_marcha = 1;
	h->num = ++hechas_total;
	h->ca = -1;
	h->b_ini = -1;
	h->bat_ini = h->bat_fin = h->bat_mw = h->mw = h->mw_max = h->pct_fr = -1;
	fecha_hora(&a, &me, &d, &hh, &mi, &se);
	snprintf(h->hora, sizeof h->hora, "%02u:%02u", hh, mi);
	snprintf(h->hms, sizeof h->hms, "%02u:%02u:%02u", hh, mi, se);
	snprintf(h->nombre, sizeof h->nombre, "%s", nombre);
	snprintf(h->linea, sizeof h->linea, "en marcha");
	if (cierra) {
		ver_cuadro = 0;
		if (fase == F_RESULTADO)
			fase = F_REPOSO;
	}
	snprintf(t, sizeof t, "===== PRUEBA %d de este arranque: %s, empieza a las %s =====", h->num, nombre, h->hms);
	log_escribe("PRUEBA", t);
	serie("termo: prueba %d: empieza «%s»\n", h->num, nombre);
	jg_cambio = 1;
	motivo_pinta = 1;
	return nhechas++;
}

/* acabada: su línea, su sección (si la hay) y, si «ver», el cuadro del resultado */
static void he_acaba(int i, const char *linea, const char *bloque, int nb, int ver)
{
	struct hecha *h;
	u32 a, me, d, hh, mi, se;
	char t[480];
	int k, j = 0;

	if (i < 0 || i >= nhechas)
		return;
	h = &HE[i];
	h->en_marcha = 0;
	fecha_hora(&a, &me, &d, &hh, &mi, &se);
	snprintf(h->hora_fin, sizeof h->hora_fin, "%02u:%02u", hh, mi);
	for (k = 0; linea[k] && j < (int)sizeof h->linea - 1; k++)	/* (en una línea: sin saltos ni espacios dobles) */
		if (linea[k] == '\r' || linea[k] == '\n' || linea[k] == ' ') {
			if (j && h->linea[j - 1] != ' ')
				h->linea[j++] = ' ';
		} else
			h->linea[j++] = linea[k];
	while (j && h->linea[j - 1] == ' ')
		j--;
	h->linea[j] = 0;
	if (bloque && nb > 0) {
		if (narchivo + nb <= (int)sizeof archivo) {
			memcpy(archivo + narchivo, bloque, (size_t)nb);
			h->b_ini = narchivo;
			h->b_len = nb;
			narchivo += nb;
		} else
			h->b_ini = -2;
	}
	snprintf(t, sizeof t, "===== PRUEBA %d: %s, acaba a las %02u:%02u:%02u: %s =====", h->num, h->nombre, hh, mi, se, h->linea);
	log_escribe("PRUEBA", t);
	serie("termo: prueba %d: acaba «%s»: %s\n", h->num, h->nombre, h->linea);
	if (ver) {
		he_vista = i;
		ver_cuadro = 1;
	}
	jg_cambio = 1;
	motivo_pinta = 1;
}

/* la última de Calentar acabada (la del «resumen»): -1 si no hay */
static int ultima_calentar(void)
{
	int i;

	for (i = nhechas - 1; i >= 0; i--)
		if (HE[i].tipo == PR_CALENTAR && !HE[i].en_marcha)
			return i;
	return -1;
}

/* un texto de la carpeta \TERMO (RENDIMIE.TXT...): octetos, 0 si no está */
static int lee_de_carpeta(const char *nombre, char *d, int cabe)
{
	char r[120];
	u32 m, n;

	d[0] = 0;
	if (!carpeta[0] || cabe < 2)
		return 0;
	snprintf(r, sizeof r, "%s\\%s", carpeta, nombre);
	m = sc(SC_ABRIR, (u32)r, PARA_LEER, 0);
	if (ES_ERROR(m))
		return 0;
	n = sc(SC_LEER, m, (u32)d, (u32)cabe - 1);
	sc(SC_CERRAR, m, 0, 0);
	if (ES_ERROR(n))
		return 0;
	d[n] = 0;
	return (int)n;
}

/* «  Calentar con batería...» con tantas letras (UTF-8) como «ancho»: recortado o con espacios */
static const char *columna(char *d, int cabe, const char *s, int ancho)
{
	int n = 0, letras = 0;

	for (; *s && n < cabe - 1; s++) {
		if ((*s & 0xC0) != 0x80) {
			if (letras == ancho)
				break;
			letras++;
		}
		d[n++] = *s;
	}
	while (letras < ancho && n < cabe - 1)
		d[n++] = ' ', letras++;
	d[n] = 0;
	return d;
}

static int en_reposo(void)
{
	return fase == F_REPOSO || fase == F_RESULTADO;
}

static const char *fase_texto(void)
{
	return fase == F_CARGA ? "carga" : fase == F_ENFRIA ? "enfriando" : "reposo";
}

/* la más caliente de las del procesador (si hay alguna que lea) o, si no,
 * de todas: es la «max=» del registro. (Una zona de ACPI clavada en 50 °C
 * no debe tapar al procesador: el HP Mini 5103.) */
static int mas_caliente(void)
{
	int k, m = NO, pasada;

	for (pasada = 0; pasada < 2 && m == NO; pasada++)
		for (k = 0; k < ns; k++)
			if ((pasada || S[k].clase != CL_ACPI) && !S[k].no_fiable && S[k].t != NO && (m == NO || S[k].t > m))
				m = S[k].t;
	return m;
}

static void temperaturas(int *t)
{
	int k;

	for (k = 0; k < VE_MAXS; k++)
		t[k] = k < ns ? S[k].t : VE_NO;
}

/* ---------------------------------------------------------------- (TERMO) los demás procesadores
 * Con la carga, el núcleo despierta a los demás procesadores (termo.cas: «Los
 * otros procesadores») y cada uno carga y lee su propio 19Ch. Aquí: cuántos,
 * por qué no si no, y un sensor por cada núcleo (los hilos de un mismo
 * núcleo, juntos: lo más caliente) que solo vale mientras se carga. */
struct ap_ficha { u32 apic, estado, vueltas, msr, lecturas, latido, tsc, z; };	/* (z, TERMO 1.2: su CR0 al llegar; bit 31, hace la fuerte) */
struct ap_tabla { u32 n, motivo, base, r; struct ap_ficha e[16]; };
static struct ap_tabla apt;
static int smt_bits, nucleo_bsp, nucleos_vistos = 1, hilos_vistos = 1;	/* (cuántos núcleos y hilos de verdad: los vistos en la carga) */
/* (TERMO) los híbridos: cuántos núcleos con hilos (y cuántos hilos), cuántos
 * con uno solo, si CPUID dice que es híbrido y el tipo de este (1Ah) */
static int topo_varios, topo_uno, topo_hilos_varios = 1, topo_hibrido, topo_tipo_bsp;
static const char *topologia_texto(void);
static const char *const motivo_ap[] = { "", "sin APIC", "el APIC está apagado (lo dejó así la BIOS)", "no se puede ver el APIC",
					 "no hay sitio libre en la memoria baja", "el trampolín no cabe", "ningún otro procesador ha contestado (tiene uno, o no responden)" };

/* La topología: de qué núcleo es cada procesador (su APIC sin los bits de
 * los hilos). Lo bueno es CPUID 0Bh (nivel 1, el de los hilos: EAX[4:0], los
 * bits que ocupan). Sin él, CPUID 1 EBX[23:16] y 4 EAX[31:26]+1, que son
 * «cuántos caben» (potencias de 2), no cuántos hay: un i5-7300HQ (4 núcleos,
 * 4 hilos) dice 16 y 8, como si tuviera 2 hilos por núcleo. Por eso cuántos
 * núcleos e hilos hay se cuenta después, con los procesadores que contestan. */
static int log2_arriba(int n)
{
	int b = 0;

	while ((1 << b) < n && b < 8)
		b++;
	return b;
}

static void cuenta_hilos(void)
{
	u32 r[4];
	int logicos, nucleos = 1;

	cpuid(1, r);
	nucleo_bsp = (int)(r[1] >> 24);
	logicos = (r[3] & (1u << 28)) ? (int)((r[1] >> 16) & 0xFF) : 1;
	smt_bits = 0;
	if ((cpu.intel || cpu.amd) && cpu.max_hoja >= 0xB) {
		__asm__ volatile("cpuid" : "=a"(r[0]), "=b"(r[1]), "=c"(r[2]), "=d"(r[3]) : "a"(0xB), "c"(0));
		if (((r[2] >> 8) & 0xFF) == 1 && (r[1] & 0xFFFF)) {	/* (nivel 0: el de los hilos, «SMT») */
			smt_bits = (int)(r[0] & 0x1F);
			goto hecho;
		}
	}
	if (cpu.intel && cpu.max_hoja >= 4) {
		__asm__ volatile("cpuid" : "=a"(r[0]), "=b"(r[1]), "=c"(r[2]), "=d"(r[3]) : "a"(4), "c"(0));
		nucleos = (int)(r[0] >> 26) + 1;
		if (logicos > nucleos)
			smt_bits = log2_arriba(logicos / nucleos);
	}	/* (AMD sin 0Bh: cada uno, como núcleo) */
hecho:
	if (smt_bits > 3)
		smt_bits = 0;
	serie("termo: topología: CPUID 0Bh %s, %d bits de hilo en el APIC\n", cpu.max_hoja >= 0xB ? "sí" : "no", smt_bits);
	nucleo_bsp >>= smt_bits;
}

static void ap_sensores(void)
{
	u32 i;
	int k;

	if (sc(SC_CARGA, 4, (u32)&apt, 0) > 16 || apt.n == 0) {
		ap_cargados = 0;
		suceso_log("la carga, en este procesador solo: los demás no (%s)", apt.motivo < 7 ? motivo_ap[apt.motivo] : "?");
		return;
	}
	ap_cargados = (int)apt.n;
	ap_fuertes = ap_cache_apagada = 0;
	for (i = 0; i < apt.n && i < 16; i++) {	/* (TERMO 1.2) +28: su CR0 al llegar; el bit 31, la fuerte */
		if (apt.e[i].z & 0x80000000u)
			ap_fuertes++;
		if (apt.e[i].z & 0x40000000u)
			ap_cache_apagada++;
	}
	for (i = 0; i < apt.n && i < 16; i++) {
		int nucleo = (int)apt.e[i].apic >> smt_bits;
		char n[48], c[24];

		if (nucleo == nucleo_bsp || !cpu.intel || !cpu.dts)
			continue;
		for (k = 0; k < ns; k++)
			if (S[k].clase == CL_CPU && S[k].zona == (0x10000 | nucleo))
				break;
		if (k < ns)
			continue;
		snprintf(n, sizeof n, "Núcleo %d del procesador (en la carga)", nucleo);
		snprintf(c, sizeof c, "Nucleo %d", nucleo);
		k = nuevo_sensor(n, c, CL_CPU, cpu.tjmax * 10, !cpu.tj_de_msr);
		if (k >= 0)
			S[k].zona = 0x10000 | nucleo;
	}
	{	/* los núcleos de verdad: los distintos entre este y los que han contestado */
		int vistos[17], nv = 0, j;

		vistos[nv++] = nucleo_bsp;
		for (i = 0; i < apt.n && i < 16; i++) {
			int nucleo = (int)apt.e[i].apic >> smt_bits;

			for (j = 0; j < nv && vistos[j] != nucleo; j++)
				;
			if (j == nv)
				vistos[nv++] = nucleo;
		}
		nucleos_vistos = nv;
		hilos_vistos = (ap_cargados + 1 + nv - 1) / nv;
		/* (TERMO) los híbridos (Intel de 12.ª generación en adelante: el
		 * i7-1355U, 2 núcleos de rendimiento con 2 hilos y 8 eficientes con
		 * 1): cuántos hilos tiene cada núcleo, de los procesadores que han
		 * contestado (los «Atom» no llevan hilos), y CPUID 07h EDX[15]
		 * (híbrido) y 1Ah (el tipo de este: 40h Core, 20h Atom) */
		{
			int cuantos[17] = { 0 };

			for (j = 0; j < nv; j++) {
				cuantos[j] = vistos[j] == nucleo_bsp ? 1 : 0;
				for (i = 0; i < apt.n && i < 16; i++)
					if ((int)apt.e[i].apic >> smt_bits == vistos[j])
						cuantos[j]++;
			}
			topo_varios = topo_uno = 0;
			topo_hilos_varios = 1;
			for (j = 0; j < nv; j++)
				if (cuantos[j] > 1) {
					topo_varios++;
					if (cuantos[j] > topo_hilos_varios)
						topo_hilos_varios = cuantos[j];
				} else
					topo_uno++;
		}
		topo_hibrido = 0;
		if (cpu.intel && cpu.max_hoja >= 7) {
			u32 r[4];

			__asm__ volatile("cpuid" : "=a"(r[0]), "=b"(r[1]), "=c"(r[2]), "=d"(r[3]) : "a"(7), "c"(0));
			topo_hibrido = (r[3] >> 15) & 1;
			if (topo_hibrido && cpu.max_hoja >= 0x1A) {
				__asm__ volatile("cpuid" : "=a"(r[0]), "=b"(r[1]), "=c"(r[2]), "=d"(r[3]) : "a"(0x1A), "c"(0));
				topo_tipo_bsp = (int)(r[0] >> 24);
			}
		}
	}
	suceso_log("la carga, en %d procesadores (este y %d más: %s)", ap_cargados + 1, ap_cargados, topologia_texto());
	if (csv_listo)
		csv_cabecera();
}

/* lo de los núcleos de la carga: cada 1 s, de la tabla del núcleo */
static void lee_ap(void)
{
	int k;
	u32 i;

	if (fase != F_CARGA || sc(SC_CARGA, 4, (u32)&apt, 0) > 16) {
		for (k = 0; k < ns; k++)
			if (S[k].clase == CL_CPU && (S[k].zona & 0x10000))
				S[k].t = NO;
		return;
	}
	for (k = 0; k < ns; k++) {
		int t = NO;

		if (S[k].clase != CL_CPU || !(S[k].zona & 0x10000))
			continue;
		for (i = 0; i < apt.n && i < 16; i++)
			if ((int)apt.e[i].apic >> smt_bits == (S[k].zona & 0xFFFF) && apt.e[i].lecturas && (apt.e[i].msr & 0x80000000u)) {
				int v = (cpu.tjmax - (int)((apt.e[i].msr >> 16) & 0x7F)) * 10;

				if (t == NO || v > t)
					t = v;
			}
		S[k].t = t;
	}
}

/* la tarea hija de la carga (TERMO.BIN /CARGA=s): su número en «hija», o -1 */
static void lanza_hija(int segundos)
{
	static const char *const donde[] = { "\\TERMO.BIN", "TERMO.BIN", "\\HISPA\\TERMO.BIN" };
	char a[40];
	int i;

	snprintf(a, sizeof a, "/CARGA=%d%s", segundos, carga_fuerte ? " /FUERTE" : "");
	hija = -1;
	for (i = 0; i < 3 && hija < 0; i++) {
		u32 r = sc(SC_LANZAR_SUELTO, (u32)donde[i], (u32)a, 0);

		if (!ES_ERROR(r))
			hija = (int)r;
	}
}

static void empieza(void)
{
	char a[40];

	if (fase == F_CARGA)
		return;
	fase = F_CARGA;
	t_fase = jg_tics();
	carga_vueltas = carga_rota = 0;
	ritmo_empieza();	/* (TERMO 1.2.1) */
	dif_tandas = dif_lentas = dif_peor_ms = 0;
	t_ini = mas_caliente();
	pico = t_ini;
	por_seguridad = por_esc = 0;
	motivo[0] = 0;
	/* (TERMO 1.1.1) su ficha en la lista de las pruebas de este arranque
	 * (enchufado o con la batería, como estaba al empezar), y lo que se
	 * cuenta «durante la prueba», a 0: lo de una no se mezcla con la otra */
	{
		char nm[56], dm[16];
		int ca = hay_bateria && en.pct <= 100 ? (en.ca == 1 ? 1 : en.ca == 0 ? 0 : -1) : -1;

		if (dura_s % 60)
			snprintf(dm, sizeof dm, "%d s", dura_s);
		else
			snprintf(dm, sizeof dm, "%d min", dura_s / 60);
		snprintf(nm, sizeof nm, "Calentar%s%s (%s)", carga_fuerte ? " fuerte" : "", ca == 1 ? " enchufado" : ca == 0 ? " con batería" : "", dm);
		he_cal = he_nueva(PR_CALENTAR, nm, 1);
		HE[he_cal].ca = ca;
		HE[he_cal].ca_ult = ca;
		snprintf(HE[he_cal].dura, sizeof HE[he_cal].dura, "%s", dm);
		HE[he_cal].fuerte = carga_fuerte;
		HE[he_cal].bat_ini = hay_bateria && en.pct <= 100 ? (int)en.pct : -1;
		reinicia_durante();
	}
	{
		struct ve_sensor vs[VE_MAXS];
		int t[VE_MAXS], k;

		for (k = 0; k < ns && k < VE_MAXS; k++) {
			snprintf(vs[k].nombre, sizeof vs[k].nombre, "%s", S[k].nombre);
			vs[k].cpu = S[k].clase != CL_ACPI && S[k].clase != CL_GPU;
			vs[k].solo_carga = 0;
			vs[k].aproximado = S[k].clase == CL_CPU && cpu.aproximada;
			vs[k].limite = S[k].limite;
			vs[k].no_fiable = S[k].no_fiable;
			S[k].fija = 0;
		}
		temperaturas(t);
		ve_empieza(&VP, ns, vs, t);
		hubo_prueba = 1;
	}
	carga_on = 1;
	disco_pausa(carpeta, 1);	/* (TERMO) la lectura de los discos espera (disco.c) */
	sc(SC_CARGA, 1, carga_fuerte ? 3 : 1, 0);	/* (TERMO 1.2) ECX bit 1: la fuerte, también en los demás */
	/* (TERMO) los demás procesadores: sus núcleos, y otra vez la foto de
	 * empezar, ya con ellos */
	ap_sensores();
	{
		struct ve_sensor vs[VE_MAXS];
		int t[VE_MAXS], k;

		for (k = 0; k < ns && k < VE_MAXS; k++) {
			snprintf(vs[k].nombre, sizeof vs[k].nombre, "%s", S[k].nombre);
			vs[k].cpu = S[k].clase != CL_ACPI && S[k].clase != CL_GPU;
			vs[k].limite = S[k].limite;
			vs[k].no_fiable = S[k].no_fiable;
			vs[k].solo_carga = S[k].clase == CL_CPU && (S[k].zona & 0x10000);
			vs[k].aproximado = S[k].clase == CL_CPU && cpu.aproximada;
		}
		temperaturas(t);
		ve_empieza(&VP, ns, vs, t);
	}
	/* la carga: un programa hijo (este mismo, con /CARGA) que no suelta el
	 * procesador; si no se puede lanzar, aquí, entre pulso y pulso */
	(void)a;
	solo_aqui = 0;
	lanza_hija(dura_s + 60);
	if (hija < 0)
		solo_aqui = 1;
	serie("termo: calentar %d s (%s); empieza a %d décimas\n", dura_s, solo_aqui ? "la carga en esta misma tarea" : "la carga en una tarea hija", t_ini);
	{
		char g[24];

		/* (lo de los demás procesadores ya está: ap_sensores, al pedir la carga) */
		const char *cg = carga_fuerte ? "fuerte (SSE2)" : "de coma flotante (x87)";

		if (ap_cargados > 0)
			suceso_log("EMPIEZA la prueba de esfuerzo: %d s, carga %s en %d procesadores (este, %s, y %d más, del núcleo); el más caliente, a %s",
				   dura_s, cg, ap_cargados + 1, solo_aqui ? "en la tarea del Termómetro, unos 8 ms de cada 10" : "con la tarea hija TERMO.BIN /CARGA al 100 %", ap_cargados, grados(g, t_ini));
		else
			suceso_log("EMPIEZA la prueba de esfuerzo: %d s, carga %s en 1 procesador (%s); el más caliente, a %s",
				   dura_s, cg, solo_aqui ? "en la tarea del Termómetro, unos 8 ms de cada 10" : "tarea hija TERMO.BIN /CARGA, al 100 %", grados(g, t_ini));
		if (carga_fuerte)
			serie("termo: carga fuerte (SSE2): %s y %d de los %d procesadores más\n", solo_aqui ? "este (en la tarea del Termómetro, con x87: sin tarea hija)" : "este (la tarea hija)",
			      ap_fuertes, ap_cargados);
		if (ap_cache_apagada)
			suceso_log("AVISO: %d de los demás procesadores llegaron con la caché apagada (CR0.CD, como los dejó la BIOS)%s", ap_cache_apagada,
				   carga_fuerte ? "; la carga fuerte la enciende" : "; la ligera los deja así (como hasta ahora): cargan menos de lo que podrían");
	}
}

/* (TERMO 1.2) la alimentación de la prueba, con todas las lecturas de la
 * carga (el ASUS GL553VD: la primera decía «no se sabe» porque ACPI aún no
 * había leído la batería, y otra, «enchufado» por la primera lectura,
 * cuando casi toda fue sin enchufar): lo que ha sido la mayor parte del
 * tiempo; «no se sabe» solo sin ninguna lectura; si ha habido de las dos
 * (4 s o más de cada), el cambio. Y su nombre, con lo que de verdad ha sido */
static void decide_alimentacion(struct hecha *h)
{
	int a = h->seg_ca[1], b = h->seg_ca[0], ca;
	char nm[56];

	if (a + b == 0)
		ca = h->ca;
	else
		ca = b > a ? 0 : 1;
	h->ca_cambio = a >= 4 && b >= 4;
	if (ca != h->ca && h->tipo == PR_CALENTAR) {
		snprintf(nm, sizeof nm, "Calentar%s%s (%s)", h->fuerte ? " fuerte" : "", ca == 1 ? " enchufado" : ca == 0 ? " con batería" : "", h->dura);
		suceso_log("la prueba %d, «%s», ha sido en realidad «%s» (%d s enchufado, %d s con la batería)", h->num, h->nombre, nm, a, b);
		snprintf(h->nombre, sizeof h->nombre, "%s", nm);
	}
	h->ca = ca;
}

static void para_carga(int seguridad, int esc)
{
	char a[24], b[24], c[24];

	if (fase != F_CARGA)
		return;
	carga_on = 0;
	if (ap_cargados && sc(SC_CARGA, 4, (u32)&apt, 0) <= 16) {
		char v[200];
		int m = 0;
		u32 i;

		for (i = 0; i < apt.n && i < 16 && m < (int)sizeof v - 20; i++)
			m += snprintf(v + m, sizeof v - (size_t)m, " %u", apt.e[i].vueltas);
		serie("termo: los demás procesadores, al parar: %u, vueltas de carga:%s\n", apt.n, v);
	}
	sc(SC_CARGA, 1, 0, 0);
	seg_carga = (int)((jg_tics() - t_fase) / jg_hz);
	if (he_cal >= 0 && he_cal < nhechas)
		decide_alimentacion(&HE[he_cal]);
	/* (TERMO 1.2.1) el ritmo de la carga: al puerto serie y a su ficha */
	{
		char r1[160], r2[160], r3[160], rp[260];
		int ln, lt, lp, lpe, lv;

		po_lecturas(&ln, &lt, &lp, &lpe, &lv);
		serie("termo: ritmo de la carga: segundos %u, el más lento %u ms, %u de más de 1,5 s; pantalla %u, la más lenta %u ms, %u de más de 1,5 s; "
		      "lecturas %u, la más lenta %u ms, %u de más de 3 s; rapl %d, %d tarde, %d perdidas, la más lenta %d ms, vuelta %d s; de fondo %d; escribir %u tandas, %u lentas, la peor %u ms\n",
		      ri_seg.n, ri_seg.peor * 1000 / jg_hz, ri_seg.tarde, ri_pin.n, ri_pin.peor * 1000 / jg_hz, ri_pin.tarde, ri_lee.n, ri_lee.peor * 1000 / jg_hz, ri_lee.tarde,
		      ln, lt, lp, lpe, lv, ri_de_fondo, dif_tandas, dif_lentas, dif_peor_ms);
		ritmo_texto(r1, sizeof r1, &ri_pin, "1,5 s");
		ritmo_texto(r2, sizeof r2, &ri_seg, "1,5 s");
		ritmo_texto(r3, sizeof r3, &ri_lee, "3 s");
		if (ln < 0)
			snprintf(rp, sizeof rp, "sin RAPL (este procesador no dice sus vatios)");
		else if (!lp)
			snprintf(rp, sizeof rp, "%d, %d tarde (más de 3 s; la más lenta, %d,%d s): NINGUNA PERDIDA (el contador del paquete, de 32 bits, da la vuelta en unos %d min a la potencia más alta)",
				 ln, lt, lpe / 1000, lpe % 1000 / 100, lv / 60);
		else
			snprintf(rp, sizeof rp, "%d, %d tarde; OJO: %d PERDIDAS (tan tarde que el contador de 32 bits ha podido dar la vuelta, cada unos %d s: esos ratos no cuentan en los vatios)", ln, lt, lp, lv);
		if (he_cal >= 0 && he_cal < nhechas)
			snprintf(HE[he_cal].ritmo, sizeof HE[he_cal].ritmo,
				 "Ritmo durante la carga: la pantalla, %s; la parada de seguridad (se mira cada segundo), %s; las lecturas de 2 s, %s; la carga, %s.\r\n"
				 "Lecturas de la potencia (RAPL) en la carga: %s.\r\n",
				 r1, r2, r3, ri_de_fondo == 1 ? "de fondo (solo cuando nadie más necesita el procesador)" : "por turnos con el Termómetro (núcleo de antes de la 1.2.1)", rp);
	}
	por_seguridad = seguridad;
	por_esc = esc;
	{
		int t[VE_MAXS];

		temperaturas(t);
		if (ve_para(&VP, t) >= 0) {
			t_ini = VP.t_ini;
			pico = VP.pico_ref;
			serie("termo: se mide con «%s»\n", VP.s[VP.ref].nombre);
		}
	}
	fase = F_ENFRIA;
	t_fase = jg_tics();
	serie("termo: fin de la carga a los %d s (%s); pico %s, ahora %s, al empezar %s\n", seg_carga,
	      seguridad ? "PARADA DE SEGURIDAD" : esc ? "Esc" : "se acabó el tiempo", grados(a, pico), grados(b, VP.t_parada), grados(c, t_ini));
	suceso_log("ACABA la carga a los %d s: %s%s; llegó a %s, al empezar %s. Empieza el enfriamiento", seg_carga,
		   seguridad ? "PARADA DE SEGURIDAD: " : esc ? "parada con Esc" : "se acabó el tiempo", seguridad ? motivo : "", grados(a, pico), grados(c, t_ini));
}

/* ---------------------------------------------------------------- los parones
 * (TERMO) El MSI GT80 (23/11/2026, por la BIOS/CSM): «leer y apuntar ha
 * tardado 8640 ms» a los 18 s de carga, y luego 2 s cada pocos segundos; los
 * núcleos 1-3 a «--» para siempre y la carga, sin decirlo, rota (el núcleo
 * para la carga y duerme a los demás procesadores si el Termómetro no
 * pregunta en 6 s; y los otros procesadores se paran solos si su latido no
 * cambia en unos 8 s). Aquí, lo que hace falta para saber qué ha sido:
 *  - los SMI (MSR_SMI_COUNT, 34h, Intel de Nehalem en adelante): cuántas
 *    veces el firmware se ha quedado el procesador (cada línea DATOS: smi=);
 *  - el equipo entero parado: el reloj del procesador (TSC) sigue contando
 *    en un SMI, el tic del sistema no (sus interrupciones esperan): lo que
 *    el TSC cuenta de más es el tiempo que nadie ha podido correr (parado=);
 *    y, por si el TSC no sirve, el reloj de la CMOS frente a los tics;
 *  - en qué se ha ido el rato: cada parte de leer y apuntar (los sensores,
 *    ACPI, escribir en la memoria USB), lo que ha corrido cada tarea (la
 *    llamada 224 con 7) y lo que han tardado las órdenes de la memoria USB.
 * Y si la carga se ha roto, se vuelve a poner en marcha (y se dice); si no
 * se puede, el veredicto lo dice (VP.carga_cortada). */
struct uso_tarea { u32 id, tics, estado; char nombre[12]; };
struct uso_tabla { u32 usb_ordenes, usb_lentas, usb_peor, usb_reinicios, usb_fallos, tics, ociosa, r; struct uso_tarea t[32]; };
static struct uso_tabla uso_ini;	/* al empezar el pulso */
static int uso_n_ini, uso_hay = 1;
static u32 smi_ult, smi_total;	/* el contador la última vez; los de la prueba */
static u64 blk_tsc;			/* el TSC y los tics de los últimos 2 s */
static u32 blk_tics;
static int parones, paron_peor_ms, parado_total_ms, parones_smi;	/* para el informe: el equipo entero parado */
static int bloqueos, bloqueo_peor_ms;	/* y el Termómetro sin poder correr (con el equipo en marcha) */
static int cmos_atras, cmos_atras_dicho;	/* (s) lo que el reloj del sistema se ha quedado atrás de golpe; cuántas veces */
static u32 cmos_s0, cmos_t0;
static int paron_a, paron_b;		/* (las pruebas: TERMO\\PARON.PRU) */

static void lee_paron_prueba(void)
{
	char r[120], t[32];
	u32 m, n;
	int i = 0, *v = &paron_a;

	snprintf(r, sizeof r, "%s\\PARON.PRU", carpeta);
	m = sc(SC_ABRIR, (u32)r, PARA_LEER, 0);
	if (ES_ERROR(m))
		return;
	n = sc(SC_LEER, m, (u32)t, sizeof t - 1);
	sc(SC_CERRAR, m, 0, 0);
	if (ES_ERROR(n))
		return;
	t[n] = 0;
	for (; t[i]; i++)
		if (t[i] >= '0' && t[i] <= '9')
			*v = *v * 10 + t[i] - '0';
		else if (v == &paron_a && paron_a)
			v = &paron_b;
	if (paron_b <= 0 || paron_b > 60)
		paron_a = paron_b = 0;
	else
		serie("termo: (prueba) PARON.PRU: a los %d s de carga, %d s parado\n", paron_a, paron_b);
}

static u32 lee_smi(void)
{
	u32 lo, hi;

	if (smi_hay < 0)
		smi_hay = cpu.intel && cpu.fam == 6 && cpu.modelo >= 0x1A && lee_msr(0x34, &lo, &hi) ? 1 : 0;
	if (smi_hay != 1 || !lee_msr(0x34, &lo, &hi))
		return 0;
	return lo;
}

static u32 ms_de_tsc(u64 d)
{
	return tsc_hz ? (u32)(d * 1000 / tsc_hz) : 0;
}

static int uso_lee(struct uso_tabla *u)
{
	u32 r;

	if (!uso_hay)
		return -1;
	r = sc(SC_CARGA, 7, (u32)u, sizeof *u);
	if (ES_ERROR(r) || r > 32 || u->tics == 0) {
		uso_hay = 0;	/* (un núcleo de antes: la 224 con 7 es otra cosa) */
		return -1;
	}
	return (int)r;
}

/* lo que ha corrido cada tarea entre a (na) y b (nb): las que más, a d */
static void uso_texto(char *d, int cabe, const struct uso_tabla *a, int na, const struct uso_tabla *b, int nb, u32 dt)
{
	int i, j, m = 0, dichas = 0;
	u32 hecho[32];
	int nh = 0;

	d[0] = 0;
	if (na < 0 || nb < 0 || !dt)
		return;
	while (dichas < 4) {
		int mejor = -1;
		u32 mt = 0;

		for (i = 0; i < nb && i < 32; i++) {
			u32 t0 = 0, dtt;
			int k, ya = 0;

			for (k = 0; k < nh; k++)
				if (hecho[k] == b->t[i].id)
					ya = 1;
			if (ya)
				continue;
			for (j = 0; j < na && j < 32; j++)
				if (a->t[j].id == b->t[i].id)
					t0 = a->t[j].tics;
			dtt = b->t[i].tics - t0;
			if (dtt > mt)
				mt = dtt, mejor = i;
		}
		if (mejor < 0 || mt * 100 < dt * 5)	/* (menos del 5 %: no se dice) */
			break;
		hecho[nh++] = b->t[mejor].id;
		{
			char nom[13];

			memcpy(nom, b->t[mejor].nombre, 12);
			nom[12] = 0;
			m += snprintf(d + m, (size_t)(cabe > m ? cabe - m : 0), "%s%s %u,%u s", dichas ? ", " : "", nom[0] ? nom : "?", mt / jg_hz, mt * 10 / jg_hz % 10);
		}
		if (m >= cabe - 1)
			break;
		dichas++;
	}
}

static void hora_texto(char *d, int cabe)
{
	u32 a, me, dd, h, mi, s;

	fecha_hora(&a, &me, &dd, &h, &mi, &s);
	snprintf(d, (size_t)cabe, "%02u:%02u:%02u", h, mi, s);
}

/* cada 2 s: los SMI, el equipo parado (TSC frente a los tics) y la CMOS
 * frente a los tics */
static void mira_parones(void)
{
	u32 s = lee_smi(), t = jg_tics();
	u64 x = rdtsc();

	if (smi_hay == 1 && smi_ult)
		smi_dos = s - smi_ult, smi_total += smi_dos;
	smi_ult = s ? s : 1;
	parado_dos = 0;
	if (blk_tsc && tsc_hz && x > blk_tsc) {
		int ms_tsc = (int)ms_de_tsc(x - blk_tsc), ms_tics = (int)((t - blk_tics) * 1000 / jg_hz);

		if (ms_tsc - ms_tics >= 700 && ms_tsc - ms_tics < 3600000) {
			parado_dos = ms_tsc - ms_tics;
			parado_total_ms += parado_dos;
		}
	}
	blk_tsc = x, blk_tics = t;
	/* la CMOS (de 2 en 2 s) frente a los tics, por si el TSC no lo ve: un
	 * salto de golpe de 4 s o más entre dos apuntes (lo que se va
	 * atrasando poco a poco, una máquina emulada lenta, no) */
	{
		u32 a, me, d, h, mi, se, cs;

		fecha_hora(&a, &me, &d, &h, &mi, &se);
		cs = d * 86400 + h * 3600 + mi * 60 + se;
		if (cmos_t0 && cs >= cmos_s0) {
			int salto = (int)(cs - cmos_s0) - (int)((t - cmos_t0) / jg_hz);

			if (salto >= 4 && salto < 3600) {
				cmos_atras += salto;
				if (++cmos_atras_dicho <= 10)
					suceso_log("AVISO: el reloj del sistema se ha quedado %d s atrás de golpe frente al de la CMOS: el equipo entero ha estado parado (nadie corría, ni el reloj del sistema; %s)",
						   salto, smi_hay == 1 ? (smi_dos ? "con SMI del firmware en ese rato" : "sin SMI del firmware en ese rato: ¿un hipervisor?") : "este procesador no cuenta los SMI");
			}
		}
		cmos_s0 = cs, cmos_t0 = t;
	}
	if (parado_dos) {
		char sm[80];

		if (smi_hay == 1 && smi_dos)
			snprintf(sm, sizeof sm, "%u SMI en ese rato: ha sido el firmware", smi_dos);
		else if (smi_hay == 1)
			snprintf(sm, sizeof sm, "0 SMI en ese rato: no ha sido el firmware (¿un hipervisor?)");
		else
			snprintf(sm, sizeof sm, "este procesador no cuenta los SMI");
		parones++;
		if (smi_dos)
			parones_smi++;
		if (parado_dos > paron_peor_ms)
			paron_peor_ms = parado_dos;
		if (parones <= 20 && smi_dos)
			suceso_log("AVISO: el firmware (SMI) se ha quedado el procesador %d,%d s (%u SMI en estos 2 s): el equipo entero, parado (el reloj del procesador ha seguido y el del sistema no)",
				   parado_dos / 1000, parado_dos % 1000 / 100, smi_dos);
		else if (parones <= 20)
			suceso_log("AVISO: el equipo entero ha estado parado %d,%d s en estos 2 s (el reloj del procesador ha seguido y el del sistema no; %s)", parado_dos / 1000,
				   parado_dos % 1000 / 100, sm);
	}
}

/* (TERMO) la carga, con un parón por medio: si el núcleo la ha parado (el
 * Termómetro no preguntaba) o los otros procesadores se han parado solos (su
 * latido no cambiaba), se vuelve a poner en marcha; si no se puede, la prueba
 * deja de ser completa desde ahora (VP.carga_cortada) */
static void mira_carga(void)
{
	int seg = (int)((jg_tics() - t_fase) / jg_hz), parados = 0, n_ap = 0;
	u32 i, r;
	char h[12];

	if (fase != F_CARGA || !carga_on || carga_rota)
		return;
	r = sc(SC_CARGA, 0, 0, 0);
	if (sc(SC_CARGA, 4, (u32)&apt, 0) <= 16 && apt.base)
		for (i = 0; i < apt.n && i < 16; i++)
			if (apt.e[i].estado == 2)
				parados++;
	if (r == 1 && !parados)
		return;
	hora_texto(h, sizeof h);
	if (carga_vueltas >= 5) {
		carga_rota = 1;
		VP.carga_cortada = seg + 1;
		snprintf(VP.carga_cortada_hora, sizeof VP.carga_cortada_hora, "%s", h);
		suceso_log("AVISO: la carga se ha vuelto a romper (ya van %d) y no se pone en marcha otra vez: desde las %s (a los %d s) NO es completa", carga_vueltas + 1, h, seg);
		return;
	}
	carga_vueltas++;
	if (r != 1) {
		/* el núcleo la ha parado y ha dormido a los demás: otra vez, y la hija */
		sc(SC_CARGA, 1, carga_fuerte ? 3 : 1, 0);
		if (!solo_aqui) {
			lanza_hija(dura_s - seg + 60);
			if (hija < 0)
				solo_aqui = 1;
		}
		n_ap = sc(SC_CARGA, 4, (u32)&apt, 0) <= 16 ? (int)apt.n : 0;
	} else {
		/* los demás se han parado solos (su vigía): se duermen y despiertan */
		u32 k = sc(SC_CARGA, 6, 0, 0);

		n_ap = ES_ERROR(k) || k > 16 ? 0 : (int)k;
		ap_relanzados += parados;
	}
	VP.carga_vuelta = carga_vueltas;
	if (n_ap < ap_cargados || (r != 1 && hija < 0 && !solo_aqui)) {
		carga_rota = 1;
		VP.carga_cortada = seg + 1;
		snprintf(VP.carga_cortada_hora, sizeof VP.carga_cortada_hora, "%s", h);
		suceso_log("AVISO: la carga se había parado (%s) y solo vuelve en %d de los %d procesadores: desde las %s (a los %d s) NO es completa",
			   r != 1 ? "el núcleo la para si el Termómetro no puede preguntar en 6 s" : "los demás procesadores se paran solos si no les llega el latido en unos 8 s",
			   n_ap + 1, ap_cargados + 1, h, seg);
	} else
		suceso_log("AVISO: la carga se había parado (%s%s) y se vuelve a poner en marcha a las %s (a los %d s), en %d procesadores%s", r != 1 ? "el núcleo la para si el Termómetro no puede preguntar en 6 s" : "",
			   r != 1 ? "" : parados == 1 ? "1 de los demás procesadores se había parado solo: no le llegaba el latido" : "varios de los demás procesadores se habían parado solos: no les llegaba el latido",
			   h, seg, n_ap + 1, solo_aqui ? " (este, en la tarea del Termómetro)" : "");
}

/* ---------------------------------------------------------------- Machine Check
 * (TERMO) El MSI GT80: «si es algo de la alimentación, interferiría en más
 * cosas que el procesador» (Carlos). Los bancos de Machine Check (MCG_CAP
 * 179h; MCi_STATUS, ADDR y MISC desde 400h, de cuatro en cuatro) guardan los
 * errores de hardware que ha visto el procesador (caché, controlador de
 * memoria, bus, PCIe), corregidos o no, y sobreviven a un reinicio en
 * caliente: lo de Windows se ve aquí. Solo leer; al abrir y al acabar la
 * prueba (los nuevos, aparte). */
#define MCA_MAX 32
static int mca_bancos = -1, mca_antes, mca_nuevos;
static u32 mca_st0[MCA_MAX][2];
static char mca_texto[900];	/* para el informe */

static const char *mca_clase(u32 cod)
{
	if (cod == 0)
		return "sin error";
	if (cod == 1)
		return "error sin clasificar";
	if (cod == 2)
		return "error de la ROM del microcódigo";
	if (cod == 3)
		return "error externo (otro procesador o la placa, por BINIT o MCERR)";
	if (cod == 5)
		return "error de paridad interno";
	if (cod == 0x400)
		return "vigilante interno del procesador (se ha quedado colgado un núcleo)";
	if ((cod & 0xFC00) == 0x0400)
		return "error interno del procesador sin clasificar";
	if ((cod & 0xEFF0) == 0x0010)
		return "error de la TLB (traducción de direcciones)";
	if ((cod & 0xEF80) == 0x0080)
		return "error del controlador de memoria (la RAM o su canal)";
	if ((cod & 0xEF00) == 0x0100)
		return "error de la caché";
	if ((cod & 0xE800) == 0x0800)
		return "error del bus o de la interconexión (PCIe, el anillo, la memoria por el bus)";
	return "error de otra clase (del fabricante)";
}

static void mira_mca(int al_final)
{
	u32 r[4], lo, hi;
	int i, n = 0;

	if (mca_bancos == 0 || (!cpu.intel && !cpu.amd))
		return;
	if (mca_bancos < 0) {
		cpuid(1, r);
		if (!(r[3] & (1u << 14)) || !(r[3] & (1u << 7)) || !lee_msr(0x179, &lo, &hi)) {
			mca_bancos = 0;
			suceso_log("Machine Check: este procesador no deja leer sus errores de hardware");
			return;
		}
		mca_bancos = (int)(lo & 0xFF) > MCA_MAX ? MCA_MAX : (int)(lo & 0xFF);
	}
	for (i = 0; i < mca_bancos; i++) {
		u32 a = 0, ah = 0, mi = 0, mh = 0, cod;
		char t[400];
		int nuevo;

		if (!lee_msr(0x401 + 4 * (u32)i, &lo, &hi) || !(hi & 0x80000000u))
			continue;
		nuevo = al_final && (lo != mca_st0[i][0] || hi != mca_st0[i][1]);
		if (al_final && !nuevo)
			continue;
		mca_st0[i][0] = lo, mca_st0[i][1] = hi;	/* (TERMO 1.1.1) también al acabar: la prueba siguiente no lo vuelve a contar */
		if (hi & (1u << 26))
			lee_msr(0x402 + 4 * (u32)i, &a, &ah);
		if (hi & (1u << 27))
			lee_msr(0x403 + 4 * (u32)i, &mi, &mh);
		cod = lo & 0xFFFF;
		snprintf(t, sizeof t, "Machine Check, banco %d: %s, %s%s%s (código %04Xh, el del modelo %04Xh; estado %08X%08Xh%s", i, mca_clase(cod),
			 hi & (1u << 29) ? "NO corregido" : "corregido", hi & (1u << 30) ? ", y hubo más que no caben (desbordado)" : "",
			 hi & (1u << 25) ? ", el procesador quedó en mal estado" : "", cod, lo >> 16, hi, lo, (hi & (1u << 26)) ? "" : ")");
		if (hi & (1u << 26)) {
			size_t k = strlen(t);

			snprintf(t + k, sizeof t - k, "; dirección %08X%08Xh)", ah, a);
		}
		suceso_log("%s%s", t, al_final ? " NUEVO: ha pasado durante el Termómetro" : " (anotado ANTES de abrir el Termómetro: desde Windows o el arranque; sobrevive al reinicio en caliente)");
		if (strlen(mca_texto) + strlen(t) + 80 < sizeof mca_texto) {
			size_t k = strlen(mca_texto);

			snprintf(mca_texto + k, sizeof mca_texto - k, "  - %s%s\r\n", al_final ? "NUEVO: " : "De antes: ", t);
		}
		n++;
		(void)mi, (void)mh;
	}
	if (al_final)
		mca_nuevos += n;
	else {
		mca_antes = n;
		suceso_log("Machine Check: %d bancos; %s", mca_bancos, n ? "con errores anotados (arriba)" : "ningún error de hardware anotado");
	}
}

/* ---------------------------------------------------------------- la alimentación
 * (TERMO) el voltaje de la batería (_BST) y el porcentaje con el cargador
 * puesto: si baja enchufado, el cargador no da bastante; el voltaje, el de
 * reposo frente al más bajo en la carga */
static int bat_mv_reposo, bat_mv_min_carga, bat_pct_ca_ini = -1, bat_pct_ca_min = 101, bat_baja_dicho;

static void mira_alimentacion(void)
{
	int mv = hay_zonas ? (int)za.t.bat_mv : 0;

	/* (TERMO 1.1.1) la ficha de la prueba de esfuerzo en marcha: la batería
	 * al acabar, lo que daba sin enchufar y si se ha enchufado o
	 * desenchufado en medio */
	if (he_cal >= 0 && he_cal < nhechas && HE[he_cal].en_marcha && hay_bateria && en.pct <= 100) {
		struct hecha *h = &HE[he_cal];
		int ca = en.ca == 1 ? 1 : en.ca == 0 ? 0 : -1;

		h->bat_fin = (int)en.pct;
		/* (TERMO 1.2) _PSR (en.ca); si no lo dice, _BST (descargando: con la batería) */
		if (ca < 0 && hay_zonas && (za.t.bat_estado & 3))
			ca = za.t.bat_estado & 1 ? 0 : 1;
		if (fase == F_CARGA && ca >= 0)
			h->seg_ca[ca] += 2;
		if (ca >= 0 && h->ca_ult >= 0 && ca != h->ca_ult)
			suceso_log("OJO: durante la prueba de esfuerzo se ha %s el cargador: esta prueba no compara bien con las demás", ca ? "enchufado" : "desenchufado");
		if (ca >= 0)
			h->ca_ult = ca;
		if (fase == F_CARGA && ca == 0 && hay_zonas && za.t.bat_mw != NO && (za.t.bat_estado & 1) && (int)za.t.bat_mw > h->bat_mw)
			h->bat_mw = (int)za.t.bat_mw;
	}

	if (mv > 0 && mv < 100000) {
		if (fase == F_REPOSO)
			bat_mv_reposo = mv;
		else if (fase == F_CARGA && (!bat_mv_min_carga || mv < bat_mv_min_carga))
			bat_mv_min_carga = mv;
	}
	if (hay_bateria && en.pct <= 100 && en.ca == 1) {
		if (bat_pct_ca_ini < 0)
			bat_pct_ca_ini = (int)en.pct;
		if ((int)en.pct < bat_pct_ca_min)
			bat_pct_ca_min = (int)en.pct;
		if (bat_pct_ca_ini - bat_pct_ca_min >= 2 && !bat_baja_dicho) {
			bat_baja_dicho = 1;
			suceso_log("OJO: con el cargador puesto, la batería BAJA (del %d al %d %%): el cargador no da bastante para lo que gasta el equipo%s",
				   bat_pct_ca_ini, bat_pct_ca_min, fase == F_CARGA ? " en la prueba de esfuerzo" : "");
		}
	}
}

static int frenos_placa_en_carga(void);	/* (abajo, con los frenos) */
static int freno_frio_hay(void);
static int frenos_690(void);

/* (TERMO) INFORME.TXT: lo de la última prueba, para enseñárselo a un técnico
 * (el dueño del portátil se lo lleva): el equipo, la fecha, cada sensor en
 * reposo, en carga y al acabar, lo que no era de fiar, la parada si la hubo
 * y el veredicto, con prudencia. Sin datos de nadie. En UTF-8 (con su marca,
 * para el Bloc de notas de Windows); y la pantalla con la gráfica, en
 * INFORME.BMP */

/* (TERMO) el consumo y la autonomía, para el informe: lo que dice _BST
 * (vatios), lo que calcula la BIOS (minutos) y lo medido aquí (lo que ha
 * bajado el porcentaje sin enchufar: con 3 minutos y 2 puntos, al menos) */
static void informe_autonomia(char *b, size_t cabe)
{
	int m = 0, d_s = (int)(desc_s1 - desc_s0), d_p = desc_pct0 - desc_pct1;
	char w[16], w2[16];

	b[0] = 0;
	if (desc_pct0 < 0)
		return;
	if (desc_mw > 0)
		m += snprintf(b + m, cabe - (size_t)m, "  Consumo sin enchufar: unos %s W en la última lectura (lo dice la batería, ACPI _BST)%s%s%s.\r\n", vatios(w, desc_mw),
			      desc_mw_max > desc_mw + 500 ? "; lo más alto visto, " : "", desc_mw_max > desc_mw + 500 ? vatios(w2, desc_mw_max) : "",
			      desc_mw_max > desc_mw + 500 ? " W" : "");
	else
		m += snprintf(b + m, cabe - (size_t)m, "  Consumo: la batería no dice cuánto gasta (ACPI _BST sin el ritmo).\r\n");
	if (en.ca == 0 && en.minutos != 0xFFFF && en.minutos > 0 && en.minutos < 6000)
		m += snprintf(b + m, cabe - (size_t)m, "  Autonomía según la batería: unas %u h %02u min con lo que le queda.\r\n", en.minutos / 60, en.minutos % 60);
	if (d_s >= 180 && d_p >= 2) {
		int ppm10 = d_p * 600 / d_s;		/* (décimas de punto por minuto) */
		int queda = desc_pct1 * d_s / d_p / 60, llena = 100 * d_s / d_p / 60;

		m += snprintf(b + m, cabe - (size_t)m, "  Medido aquí: del %d %% al %d %% en %d min (%d,%d puntos por minuto). A ese ritmo, con lo que le queda,"
			      " unas %d h %02d min; con la batería llena, unas %d h %02d min.\r\n", desc_pct0, desc_pct1, d_s / 60, ppm10 / 10, ppm10 % 10,
			      queda / 60, queda % 60, llena / 60, llena % 60);
		if (desc_carga)
			m += snprintf(b + m, cabe - (size_t)m, "  (Ha sido con la prueba de esfuerzo, que gasta mucho más que el uso normal: con un uso normal durará bastante más.)\r\n");
	} else if (d_s < 180)
		m += snprintf(b + m, cabe - (size_t)m, "  Para medir la autonomía aquí hace falta al menos 3 minutos sin enchufar.\r\n");
}

/* (TERMO 1.1.1) la sección de una prueba de esfuerzo, hecha al acabarla con
 * lo suyo (antes iba directa al INFORME.TXT y la siguiente la pisaba; las
 * cuentas de la batería, de los frenos y de la potencia eran de todo el
 * arranque): el ASUS GL553VD decía a la vez «sin enchufar» y «con el
 * cargador puesto, la batería no ha bajado» */
static int bloque_calentar(char *t, int cabe, const struct hecha *h, const char *fin)
{
	char g1[24], g2[24], g3[24], c1[120], w[16], cambio_txt[200], t1[24], t2[24];
	int n = 0, k;
#define IN(...) (n += snprintf(t + n, (size_t)(cabe - n), __VA_ARGS__), n = n < cabe ? n : cabe - 1)

	IN("\r\n=====================================================================\r\n");
	IN("PRUEBA %d DE ESTE ARRANQUE: %s, de %s a %s\r\n", h->num, h->nombre, h->hms, fin);
	IN("=====================================================================\r\n");
	IN("Alimentación: %s%s\r\n", h->ca == 1 ? "enchufado a la corriente" : h->ca == 0 ? "con la batería (sin enchufar)" :
	   hay_bateria ? "no se sabe si estaba enchufado" : "a la corriente (no se ve batería)",
	   h->ca_cambio ? (snprintf(cambio_txt, sizeof cambio_txt, "; OJO: se ha enchufado o desenchufado durante la prueba: %s enchufado y %s con la batería (no compara bien con las demás)",
				    tiempo(t1, h->seg_ca[1]), tiempo(t2, h->seg_ca[0])), cambio_txt) : "");
	IN("Prueba de esfuerzo: %s, %d s; carga de coma flotante en %d %s\r\n", por_seguridad ? "PARADA DE SEGURIDAD" : por_esc ? "parada a mano (Esc)" :
	   VP.carga_cortada ? "con la carga rota (ver abajo)" : "completa", seg_carga, 1 + ap_cargados, ap_cargados ? "procesadores (hilos)" : "núcleo (Hispa-Pentium usa uno)");
	if (h->fuerte)
		IN("Carga usada: FUERTE, SSE2 (multiplicaciones y sumas de dobles en paralelo, sobre un bloque que cabe en la caché L1: sin depender de la memoria) en %s y en %d de los %d procesadores más%s: "
		   "es la que se acerca a los vatios que deja la BIOS (PL1 y PL2) sin AVX; si el paquete se queda muy por debajo de PL1 enchufado, lo frena otra cosa (la fuente, el cargador o la placa).\r\n",
		   solo_aqui ? "este (en la tarea del Termómetro, con x87: sin tarea hija)" : "este", ap_fuertes, ap_cargados,
		   ap_fuertes < ap_cargados ? " (los demás, con la ligera: sin SSE2)" : "");
	else
		IN("Carga usada: ligera, x87 (la coma flotante de siempre, raíz y seno, en cada procesador): en un procesador de hoy no llega a los vatios que deja la BIOS (PL1), "
		   "así que no dice si la fuente o el cargador darían para el máximo%s.\r\n", fuerte_puede ? " (para eso, la fuerte: tecla F)" : "");
	if (h->ritmo[0])
		IN("%s", h->ritmo);	/* (TERMO 1.2.1) */
	if (ap_cache_apagada)
		IN("OJO: %d de los demás procesadores llegaron con la caché apagada (CR0.CD a 1, como los dejó la BIOS)%s.\r\n", ap_cache_apagada,
		   h->fuerte ? "; la carga fuerte la enciende" : "; con la ligera se quedan así (como hasta la 1.1.1) y cargan mucho menos de lo que podrían");
	if (por_seguridad)
		IN("Motivo de la parada: %s\r\n", motivo);
	IN("\r\nSENSOR                                  REPOSO    MÁXIMA    AL ACABAR  NOTA\r\n");
	for (k = 0; k < ns && k < VE_MAXS; k++) {
		const char *nota = S[k].no_fiable ? "lectura no fiable (no cuenta)" : S[k].fija ? "fija (no cambia; no cuenta)" :
				   VP.n[k] == 0 ? "sin dato" : k == VP.ref ? "con este se mide" : "";
		char c2[40], c3[40], c4[40];

		IN("%s  %s  %s  %s  %s\r\n", columna(c1, sizeof c1, S[k].nombre, 38), columna(c2, sizeof c2, ve_grados(g1, VP.ini[k]), 8),
		   columna(c3, sizeof c3, ve_grados(g2, VP.pico[k]), 8), columna(c4, sizeof c4, ve_grados(g3, S[k].t), 9), nota);
	}
	{
		char fr[1500];

		if (ve_mhz_carga(&VP)) {
			char le[240];

			IN("\r\nFrecuencia en la carga: %d MHz de media (la nominal: %d MHz).\r\n", ve_mhz_carga(&VP), VP.mhz_nominal);
			if (ve_lento(&VP, le, (int)sizeof le))
				IN("  %s\r\n", le);
		}
		IN("\r\nFRENADOS DEL PROCESADOR\r\n");
		if (!cpu.intel || !cpu.dts)
			IN("  (este procesador no dice por qué se frena)\r\n");
		else if (informe_frenos(fr, sizeof fr))
			IN("%s", fr);
		else
			IN("  Ninguno visto.\r\n");
	}
	{
		static char po[3500];	/* (TERMO) POTENCIA, POR QUÉ SE FRENA y ESTADOS DE REPOSO (potencia.c) */

		if (po_informe(po, (int)sizeof po))
			IN("%s", po);
	}
	/* (TERMO) los errores de hardware (Machine Check) */
	if (mca_bancos > 0) {
		IN("\r\nERRORES DE HARDWARE ANOTADOS POR EL PROCESADOR (MACHINE CHECK)\r\n");
		if (mca_antes + mca_nuevos == 0)
			IN("  Ninguno (%d bancos mirados al abrir y al acabar).\r\n", mca_bancos);
		else
			IN("%s  (Los de antes los anotó el procesador antes de abrir el Termómetro, con Windows o al arrancar: estos registros sobreviven al reinicio en caliente.)\r\n", mca_texto);
	}
	/* (TERMO) «qué apunta a la alimentación y qué no», sin sacar conclusiones */
	{
		int fav = 0, con = 0, placa_prueba;
		char f[1400], c[1400];
		int nf = 0, nc = 0;

		f[0] = c[0] = 0;
		placa_prueba = frenos_placa_en_carga();
		if (placa_prueba) {
			nf += snprintf(f + nf, sizeof f - (size_t)nf, "  + En la carga se ha frenado por una señal de la placa o del regulador (PROCHOT, alarma del regulador, corriente o EDP)%s.\r\n",
				       freno_frio_hay() ? ", con el procesador lejos de su límite" : "");
			fav++;
		} else if (cpu.intel && cpu.dts) {
			nc += snprintf(c + nc, sizeof c - (size_t)nc, "  - En la prueba no se ha visto ni PROCHOT ni la alarma del regulador de tensión ni el límite de corriente.\r\n");
			con++;
		}
		if (bat_baja_dicho) {
			nf += snprintf(f + nf, sizeof f - (size_t)nf, "  + Con el cargador puesto, la batería ha bajado (del %d al %d %%): el cargador no da bastante.\r\n", bat_pct_ca_ini, bat_pct_ca_min);
			fav++;
		} else if (bat_pct_ca_ini >= 0) {
			nc += snprintf(c + nc, sizeof c - (size_t)nc, "  - Con el cargador puesto, la batería no ha bajado (del %d %%, como mucho al %d %%).\r\n", bat_pct_ca_ini, bat_pct_ca_min > 100 ? bat_pct_ca_ini : bat_pct_ca_min);
			con++;
		}
		if (bat_mv_reposo && bat_mv_min_carga) {
			int caida = (bat_mv_reposo - bat_mv_min_carga) * 1000 / bat_mv_reposo;

			if (caida >= 50) {
				nf += snprintf(f + nf, sizeof f - (size_t)nf, "  + El voltaje de la batería cae en la carga: de %d,%02d V en reposo a %d,%02d V (un %d,%d %% menos)%s.\r\n", bat_mv_reposo / 1000,
					       bat_mv_reposo % 1000 / 10, bat_mv_min_carga / 1000, bat_mv_min_carga % 1000 / 10, caida / 10, caida % 10,
					       h->ca == 0 ? "; sin enchufar, la batería lo da todo: que baje algo es normal" : "");
				fav++;
			} else {
				nc += snprintf(c + nc, sizeof c - (size_t)nc, "  - El voltaje de la batería aguanta en la carga (de %d,%02d V a %d,%02d V como mucho).\r\n", bat_mv_reposo / 1000,
					       bat_mv_reposo % 1000 / 10, bat_mv_min_carga / 1000, bat_mv_min_carga % 1000 / 10);
				con++;
			}
		}
		if (mca_nuevos) {
			nf += snprintf(f + nf, sizeof f - (size_t)nf, "  + Durante la prueba el procesador ha anotado %d errores de hardware (Machine Check): una alimentación floja puede darlos, pero también la RAM, la placa o el propio procesador.\r\n", mca_nuevos);
			fav++;
		} else if (mca_bancos > 0) {
			nc += snprintf(c + nc, sizeof c - (size_t)nc, "  - Durante la prueba no ha habido errores de hardware (Machine Check)%s.\r\n", mca_antes ? "; los que hay son de antes" : "");
			con++;
		}
		if (frenos_690())
			nc += snprintf(c + nc, sizeof c - (size_t)nc, "  · Los límites de potencia PL1/PL2 son los vatios que deja la BIOS: frenarse por ellos es normal y no dice nada de la alimentación.\r\n");
		if (bloqueos || parones)
			nc += snprintf(c + nc, sizeof c - (size_t)nc, "  · Los parones (ver PARONES DEL EQUIPO) no son de la alimentación por sí mismos: apuntan al firmware o a la memoria USB; si fueran cortes de corriente, el equipo se reiniciaría.\r\n");
		IN("\r\nQUÉ APUNTA A LA ALIMENTACIÓN Y QUÉ NO, EN ESTA PRUEBA (sin sacar conclusiones antes de tiempo)\r\n");
		IN("A favor de un problema de alimentación:\r\n%s", fav ? f : "  (nada visto)\r\n");
		IN("En contra, o que no dice nada de ella:\r\n%s", c[0] ? c : "  (nada visto)\r\n");
		if (ram_resultado() < 0)	/* (RAM) la de la memoria, si ya se ha hecho: ver MEMORIA RAM; la lectura de los discos, en DISCOS */
			IN("  Falta, para otra vez: una prueba de la memoria RAM (tecla M; si fuera la alimentación, también fallaría).\r\n");
	}
	/* (TERMO) los parones (el MSI GT80): para el técnico */
	if (parones || bloqueos || cmos_atras_dicho || smi_total || carga_vueltas) {
		int pt = parado_total_ms > cmos_atras * 1000 ? parado_total_ms : cmos_atras * 1000;

		IN("\r\nPARONES DEL EQUIPO\r\n");
		if (smi_hay == 1)
			IN("  Interrupciones del firmware (SMI) contadas durante la prueba: %u.\r\n", smi_total);
		if (pt)
			IN("  El equipo entero ha estado parado (nadie podía correr, ni el reloj del sistema) %d,%d s en total%s%s. %s\r\n", pt / 1000, pt % 1000 / 100,
			   cmos_atras_dicho ? "; el reloj del sistema se ha quedado atrás de golpe " : "", cmos_atras_dicho ? "frente al de la CMOS (el de la placa)" : "",
			   smi_hay == 1 && smi_total ? "Con SMI contados a la vez: es el firmware (la BIOS o el controlador embebido) el que se queda el procesador. Es una pista para el técnico: "
						     "en este equipo, el firmware (BIOS/EC) para todo el sistema durante ratos largos; puede ser la emulación del teclado y el ratón USB de la BIOS, "
						     "el controlador embebido (ventiladores, batería) o un fallo de la placa. Conviene probar con la BIOS al día y, en Windows, mirar si hay tirones (latencia DPC)." :
			   smi_hay == 1 ? "Sin SMI contados: no ha sido el firmware." : "Este procesador no cuenta los SMI: no se sabe si ha sido el firmware.");
		if (bloqueos)
			IN("  El Termómetro se ha quedado sin poder leer y apuntar, con el equipo en marcha, %d %s (la peor, %d,%d s): el .LOG dice en qué parte y quién tenía el procesador.\r\n", bloqueos,
			   bloqueos == 1 ? "vez" : "veces", bloqueo_peor_ms / 1000, bloqueo_peor_ms % 1000 / 100);
		if (carga_vueltas)
			IN("  La carga se paró por eso y se volvió a poner en marcha %d %s%s.\r\n", carga_vueltas, carga_vueltas == 1 ? "vez" : "veces",
			   VP.carga_cortada ? "; al final NO quedó entera: el resultado no da por buena la temperatura en carga" : "");
	}
	if (h->bat_ini >= 0) {
		IN("\r\nLa batería en esta prueba: del %d %% al %d %%", h->bat_ini, h->bat_fin >= 0 ? h->bat_fin : h->bat_ini);
		if (h->bat_mw > 0)
			IN("; sin enchufar, en la carga daba hasta %s W (lo dice la batería, ACPI _BST)", vatios(w, h->bat_mw));
		IN(".\r\n");
	}
	IN("\r\nResultado de esta prueba:\r\n%s\r\n", resumen);
#undef IN
	return n;
}

/* el veredicto de una prueba de esfuerzo, en una línea (para la lista) */
static void linea_calentar(char *d, int cabe, const struct hecha *h)
{
	static const char *const EST[] = { "SIN DATO", "BIEN", "TEMPLADO", "CALIENTE", "PELIGRO" };
	char a[24], b[24], w[16];
	int m;

	if (por_seguridad)
		m = snprintf(d, (size_t)cabe, "PARADA DE SEGURIDAD: %s", motivo);
	else if (VP.ref < 0 || h->t_max == NO || h->t_ini == NO)
		m = snprintf(d, (size_t)cabe, "SIN JUZGAR: ningún sensor del procesador ha medido bien la carga (ver su sección)");
	else {
		int e = estado_de(h->t_max, S[VP.ref].limite), sube = (h->t_max - h->t_ini) / 10;

		m = snprintf(d, (size_t)cabe, "%s: llegó a %s (empezó en %s, %s%d °C)", EST[e], grados(a, h->t_max), grados(b, h->t_ini), sube >= 0 ? "+" : "", sube);
	}
#define LC(...) (m += snprintf(d + m, (size_t)(cabe > m ? cabe - m : 0), __VA_ARGS__), m = m < cabe ? m : cabe - 1)
	LC("; carga %s", h->fuerte ? "fuerte (SSE2)" : "ligera (x87)");	/* (TERMO 1.2) */
	if (h->mw >= 0)
		LC("; el procesador, %s W de media", vatios(w, h->mw));
	if (h->mhz > 0)
		LC("; %d MHz", h->mhz);
	if (h->pct_fr >= 0)
		LC("; frenado el %d %% de la carga", h->pct_fr);
	if (h->bat_mw > 0)
		LC("; la batería daba hasta %s W", vatios(w, h->bat_mw));
	if (por_esc)
		LC("; parada a mano (Esc)");
	if (VP.carga_cortada)
		LC("; la carga se rompió (ver PARONES DEL EQUIPO)");
	if (h->ca_cambio)
		LC("; se enchufó o desenchufó en medio");
#undef LC
}

/* (TERMO 1.1.1) la tabla que compara las de Calentar (enchufado frente a
 * con la batería: el caso del MSI GT80, ¿lo frena la fuente?) */
static int compara_calentar(char *t, int cabe)
{
	char c[10][64], v[10][40];
	int n = 0, i, j, ult[2] = { -1, -1 }, cuantas = 0;
	static const char *const cab[] = { "N.º", "HORA", "ALIMENTACIÓN", "DURACIÓN", "MÁXIMA", "SUBIDA", "PROCESADOR (media/máx.)", "MHz", "FRENADO", "BATERÍA" };
	static const int an[] = { 4, 6, 14, 9, 9, 7, 24, 6, 8, 8 };
#define IN(...) (n += snprintf(t + n, (size_t)(cabe - n), __VA_ARGS__), n = n < cabe ? n : cabe - 1)

	for (i = 0; i < nhechas; i++)
		if (HE[i].tipo == PR_CALENTAR && !HE[i].en_marcha)
			cuantas++;
	if (cuantas < 2)
		return 0;
	IN("\r\nCOMPARACIÓN DE LAS PRUEBAS DE ESFUERZO DE ESTE ARRANQUE (cada «Calentar»)\r\n ");
	for (j = 0; j < 10; j++)
		IN(" %s", columna(c[j], sizeof c[j], cab[j], an[j]));
	IN("\r\n");
	for (i = 0; i < nhechas; i++) {
		const struct hecha *h = &HE[i];
		char w1[16], w2[16];

		if (h->tipo != PR_CALENTAR || h->en_marcha)
			continue;
		if (h->ca == 1 || h->ca == 0)
			ult[h->ca] = i;
		snprintf(v[0], sizeof v[0], "%d", h->num);
		snprintf(v[1], sizeof v[1], "%s", h->hora);
		snprintf(v[2], sizeof v[2], "%s%s", h->ca == 1 ? "enchufado" : h->ca == 0 ? "con batería" : "no se sabe", h->ca_cambio ? " (!)" : "");
		snprintf(v[3], sizeof v[3], "%d s", h->seg);
		grados(v[4], h->t_max);
		if (h->t_max != NO && h->t_ini != NO)
			snprintf(v[5], sizeof v[5], "%s%d °C", h->t_max >= h->t_ini ? "+" : "", (h->t_max - h->t_ini) / 10);
		else
			strcpy(v[5], "--");
		if (h->mw >= 0)
			snprintf(v[6], sizeof v[6], "%s / %s W", vatios(w1, h->mw), vatios(w2, h->mw_max));
		else
			strcpy(v[6], "no lo dice");
		if (h->mhz > 0)
			snprintf(v[7], sizeof v[7], "%d", h->mhz);
		else
			strcpy(v[7], "--");
		if (h->pct_fr >= 0)
			snprintf(v[8], sizeof v[8], "%d %%", h->pct_fr);
		else
			strcpy(v[8], "--");
		if (h->bat_mw > 0)
			snprintf(v[9], sizeof v[9], "%s W", vatios(w1, h->bat_mw));
		else
			strcpy(v[9], "--");
		IN(" ");
		for (j = 0; j < 10; j++)
			IN(" %s", columna(c[j], sizeof c[j], v[j], an[j]));
		IN("%s\r\n", h->seguridad ? "  PARADA DE SEGURIDAD" : h->esc ? "  (parada a mano)" : "");
	}
	IN("  PROCESADOR: los vatios del procesador entero en la carga (RAPL, el paquete); MHz: la frecuencia media en la carga; FRENADO: el %% de la carga en que el procesador se ha frenado "
	   "(por potencia, por la placa o por temperatura); BATERÍA: lo más que daba la batería en la carga, si era sin enchufar (ACPI _BST). Con «(!)»: se enchufó o desenchufó en medio.\r\n");
	if (ult[0] >= 0 && ult[1] >= 0) {
		const struct hecha *e = &HE[ult[1]], *b = &HE[ult[0]];
		char g1[24], g2[24], w1[16], w2[16];

		IN("  Enchufado (prueba %d) frente a con la batería (prueba %d): máxima %s frente a %s", e->num, b->num, grados(g1, e->t_max), grados(g2, b->t_max));
		if (e->mw >= 0 && b->mw >= 0)
			IN("; el procesador, %s W frente a %s W", vatios(w1, e->mw), vatios(w2, b->mw));
		if (e->mhz > 0 && b->mhz > 0)
			IN("; %d MHz frente a %d MHz", e->mhz, b->mhz);
		if (e->pct_fr >= 0 && b->pct_fr >= 0)
			IN("; frenado el %d %% frente al %d %%", e->pct_fr, b->pct_fr);
		if (e->fuerte != b->fuerte)
			IN(". OJO: una con la carga ligera y otra con la fuerte: no se comparan bien (repita la que falte con la misma carga).\r\n");
		else if (e->fuerte)
			IN(". (Las dos con la carga fuerte, SSE2: si enchufado no pasa de lo de la batería, o se queda muy por debajo de PL1, lo frena la fuente o el cargador.)\r\n");
		else
			IN(". (Con la carga x87, ligera, puede que ninguna llegue al límite de la BIOS: entonces no se ve si la fuente frena; mejor con la fuerte, tecla F.)\r\n");
	}
#undef IN
	return n;
}

static void escribe_informe(void)
{
	static char t[150000];	/* (TERMO 1.1.1) cada prueba del arranque, con su sección (archivo[]), y lo de siempre */
	char r[120];
	u32 a, me, d, h, mi, se;
	int n = 0, i, hay_ram = 0, hay_rend = 0;
#define IN(...) (n += snprintf(t + n, sizeof t - (size_t)n, __VA_ARGS__), n = n < (int)sizeof t ? n : (int)sizeof t - 1)

	vuelca_diferido(1);	/* (TERMO 1.2.1) el .LOG y el .CSV, al día */
	if (!carpeta[0] || csv_malo)
		return;
	fecha_hora(&a, &me, &d, &h, &mi, &se);
	IN("\xEF\xBB\xBFINFORME DEL TERMÓMETRO DE HISPA-PENTIUM\r\n");
	IN("=======================================\r\n\r\n");
	IN("Fecha y hora del informe: %02u/%02u/%04u %02u:%02u (la del reloj del equipo%s)\r\n", d, me, a, h, mi, fecha_mala ? ": PARECE MAL" : "");
	IN("Equipo: %s %s %s\r\n", za.t.fabricante, za.t.producto, za.t.version_eq);
	IN("Placa: %s %s   BIOS: %s\r\n", za.t.placa_fab, za.t.placa, za.t.bios);
	IN("Procesador: %s (TjMax %d °C, %s)\r\n", cpu.marca, cpu.tjmax, cpu.tj_de_msr ? "la que dice el procesador" : "de tabla");
	/* (TERMO 1.2) «Enviar a Carlos», la última vez de este arranque */
	if (envio_ok == 1)
		IN("Enviado a Carlos: sí, a las %s: %s\r\n", envio_hora, envio_fin);
	else if (envio_ok == 0)
		IN("Enviado a Carlos: NO, a las %s: %s\r\n", envio_hora, envio_fin);
	else if (puede_enviar)
		IN("Enviado a Carlos: todavía no (tecla E, con la red puesta)\r\n");
	if (ap_cargados)
		IN("Núcleos: %s (%d procesadores)\r\n", topologia_texto(), ap_cargados + 1);
	IN("\r\nPRUEBAS DE ESTE ARRANQUE (en el orden en que se han hecho; cada una, con su sección más abajo)\r\n");
	if (hechas_total > nhechas)
		IN("  (Las %d primeras ya no caben en esta lista: están en el .LOG.)\r\n", hechas_total - nhechas);
	for (i = 0; i < nhechas; i++) {
		const struct hecha *x = &HE[i];

		IN("  %d. %s a %s  %s%s\r\n", x->num, x->hora, x->en_marcha ? "(en marcha)" : x->hora_fin, x->nombre,
		   x->b_ini == -2 ? " (su sección no cabe en el informe: ver el .LOG)" : "");
		if (x->tipo == PR_RAM && !x->en_marcha)
			hay_ram = 1;
		if (x->tipo == PR_RENDIMIENTO && !x->en_marcha)
			hay_rend = 1;
	}
	if (!nhechas)
		IN("  Ninguna acabada todavía.\r\n");
	if (ruta_log[0])
		IN("  (El .LOG de este arranque, %s, lo tiene todo seguido: cada prueba empieza y acaba con una línea «PRUEBA».)\r\n", ruta_log);
	/* las secciones de cada una, tal cual se escribieron al acabarla */
	for (i = 0; i < nhechas; i++)
		if (HE[i].b_ini >= 0 && n + HE[i].b_len < (int)sizeof t - 45000) {
			memcpy(t + n, archivo + HE[i].b_ini, (size_t)HE[i].b_len);
			n += HE[i].b_len;
		} else if (HE[i].b_ini >= 0)
			IN("\r\n(La sección de la prueba %d no cabe en el informe: ver el .LOG.)\r\n", HE[i].num);
	n += compara_calentar(t + n, (int)sizeof t - n);
	n = n < (int)sizeof t ? n : (int)sizeof t - 1;
	IN("\r\n=====================================================================\r\n");
	IN("LO DEL EQUIPO (al escribir este informe, %02u:%02u)\r\n", h, mi);
	IN("=====================================================================\r\n");
	if (hay_bateria && en.pct <= 100) {
		IN("\r\nBatería: %u %%, %s", en.pct, en.ca == 1 ? "enchufado a la corriente" : en.ca == 0 ? "sin enchufar" : "");
		if (en.salud_pct <= 150)
			IN("; salud %u %% (lo que guarda llena frente a nueva)", en.salud_pct);
		if (en.ciclos1 != 0xFFFF && en.ciclos1 != 0 && en.ciclos1 < 100000)
			IN("; %u ciclos de carga", en.ciclos1);
		IN("\r\n");
		{
			char au[900];

			informe_autonomia(au, sizeof au);
			IN("%s", au);
		}
		if (en.salud_pct < 50)
			IN("  La batería está muy gastada (menos de la mitad de lo que guardaba nueva): conviene cambiarla. Si la tapa de abajo o el ratón táctil abomban, puede estar hinchada: llévelo al técnico cuanto antes.\r\n");
	}
	if (sin_bat_texto[0] && !(hay_bateria && en.pct <= 100))
		IN("\r\nBatería: %s.\r\n", sin_bat_texto);
	{
		static char dt[8000];	/* (TERMO) los discos de dentro (disco.c: TERMO\\DISCOS.TXT) */

		if (disco_informe(carpeta, dt, (int)sizeof dt) > 0)
			IN("%s", dt);
	}
	if (anterior[0])
		IN("\r\nLA VEZ ANTERIOR\r\n  %s\r\n", anterior);
	if (hay_zonas && za.t.equipo_hecho == 1 && (za.t.equipo_tablas || za.t.equipo_pci))
		IN("\r\nGuardado también, para Carlos: %c:\\TERMO\\EQUIPO (%u tablas ACPI tal cual%s): solo lo que dice el equipo de sí mismo, sin datos personales.\r\n",
		   unidad, za.t.equipo_tablas, za.t.equipo_pci ? " y la lista PCI, PCI.TXT" : "");
	if (nhallado) {
		IN("\r\nLO QUE HA ENCONTRADO EL SISTEMA AL ARRANCAR\r\n");
		for (i = 0; i < nhallado; i++)
			IN("  %s\r\n", hallado[i]);
	}
	if (!hay_rend) {
		char r2[120];

		snprintf(r2, sizeof r2, "%s\\RENDIMIE.TXT", carpeta);
		if (existe(r2))
			IN("\r\nRENDIMIENTO: lo medido con «Rendimiento» (otra vez) está en RENDIMIE.TXT (con la tabla de los equipos medidos).\r\n");
	}
	if (cpu.aproximada)
		IN("Nota: el sensor de este procesador (Atom) es aproximado.\r\n");
	if (hay_zonas)
		nv_acpi(&NV, &za.t);
	n += nv_informe(&NV, t + n, (int)sizeof t - n);		/* (NVIDIA) «GRÁFICAS» */
	n = n < (int)sizeof t ? n : (int)sizeof t - 1;
	if (!NV.n)
		IN("No se puede leer sin su controlador la temperatura de las tarjetas gráficas que no son NVIDIA Maxwell o Pascal (AMD...), salvo que la BIOS la dé como zona ACPI.\r\n");
	if (!hay_ram) {	/* (RAM) sin prueba de la memoria en este arranque: lo de siempre (no se ha probado, o la vez anterior) */
		static char ri[17000];

		ram_informe(ri, sizeof ri);
		IN("%s", ri);
	}
	IN("\r\nRESULTADO\r\n---------\r\n");
	IN("(Una línea por prueba, en orden; el detalle de cada una, en su sección.)\r\n");
	for (i = 0; i < nhechas; i++)
		IN("%d. %s %s: %s\r\n", HE[i].num, HE[i].hora, HE[i].nombre, HE[i].linea);
	{
		char rv[1600];

		if (ram_veredicto(rv, sizeof rv))
			IN("%s\r\n", rv);
		rv[0] = 0;
		disco_veredicto(carpeta, rv, (int)sizeof rv, parones || bloqueos);	/* (TERMO) lo de los discos */
		if (rv[0])
			IN("%s\r\n", rv);
	}
	IN("\r\nLos datos son los que dan los sensores del propio equipo; el registro completo (cada 2 s) está en los .CSV y .LOG de esta carpeta, y la pantalla con la gráfica en INFORME.BMP.\r\n");
	IN("Hecho con el Termómetro de Hispa-Pentium (GPL-2.0 o posterior). No contiene datos personales.\r\n");
#undef IN
	snprintf(r, sizeof r, "%s\\INFORME.TXT", carpeta);
	if (escribe(r, t, (u32)n, 1) && comprueba(r, t, (u32)n)) {
		snprintf(informe_dicho, sizeof informe_dicho, "Informe guardado y comprobado (leído de vuelta): %s", r);
		snprintf(aviso_archivo, sizeof aviso_archivo, "%s", informe_dicho);
	} else {
		no_se_guarda(r, motivo_error(ult_error));
		snprintf(informe_dicho, sizeof informe_dicho, "NO se ha podido guardar el informe en %s: %s", r, motivo_error(ult_error));
	}
	captura_en("INFORME.BMP");
	suceso_log("informe para el técnico: %s y INFORME.BMP (%s)", r, guarda_mal ? informe_dicho : "comprobado: leído de vuelta");
}

static int frenado_en_carga(void);

static void escribe_resumen(void)
{
	static char bl[24000];
	int k;

	VP.frenado = frenado_en_carga() || po_frenado_carga();
	mira_mca(1);
	ve_resumen(&VP, resumen, (int)sizeof resumen, seg_carga, por_seguridad, motivo, por_esc, simula);
	/* (los discos, ya no aquí: son otra prueba de la lista, con su línea; y en el RESULTADO del informe) */
	nv_veredicto(&NV, resumen + strlen(resumen), (int)(sizeof resumen - strlen(resumen)));	/* (NVIDIA) más de 90 °C, no responde */
	{
		char pv[600];	/* (TERMO) la potencia (potencia.c), si cabe */
		size_t lr = strlen(resumen);
		int npv = po_veredicto(pv, (int)sizeof pv);

		if (npv && lr + (size_t)npv + 2 < sizeof resumen)
			snprintf(resumen + lr, sizeof resumen - lr, "%s%s", lr ? " " : "", pv);
	}
	for (k = 0; k < ns && k < VE_MAXS; k++)
		S[k].fija = ve_fija(&VP, k) && k != VP.ref;
	if (he_cal >= 0 && he_cal < nhechas) {	/* (TERMO 1.1.1) su ficha y su sección, antes de que la siguiente lo cambie */
		struct hecha *h = &HE[he_cal];
		char fin[12], ln[300];
		int nb;

		h->t_ini = t_ini;
		h->t_max = pico;
		h->mhz = ve_mhz_carga(&VP);
		h->seg = seg_carga;
		h->seguridad = por_seguridad;
		h->esc = por_esc;
		po_cifras(&h->mw, &h->mw_max, &h->pct_fr);
		if (hay_bateria && en.pct <= 100)
			h->bat_fin = (int)en.pct;
		hora_texto(fin, sizeof fin);
		linea_calentar(ln, sizeof ln, h);
		nb = bloque_calentar(bl, (int)sizeof bl, h, fin);
		he_acaba(he_cal, ln, bl, nb, 1);
		he_cal = -1;
	}
	escribe_informe();
	disco_pausa(carpeta, 0);	/* (TERMO) acabada la prueba (y el enfriamiento): los discos, otra vez */
	serie("termo: resumen: %s\n", resumen);
	suceso_log("RESULTADO: %s", resumen);
	if (carpeta[0] && !csv_malo && ruta_txt[0]) {
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
	reinicia_despues();
}

/* lo de la prueba, cada segundo */
static void prueba_segundo(void)
{
	int k, p = mas_caliente(), seg = (int)((jg_tics() - t_fase) / jg_hz), t[VE_MAXS];

	temperaturas(t);
	if (fase == F_CARGA) {
		ve_carga(&VP, t);
		VP.procesadores = ap_cargados + 1;
		VP.mhz_nominal = ve_mhz_nominal(cpu.marca, (int)mhz_tsc);
		if (cpu.aperf && seg >= 5)	/* (la frecuencia de la carga: sin los primeros segundos, que sube) */
			ve_frecuencia(&VP, (int)mhz_ahora);
		if (p != NO && (pico == NO || p > pico))
			pico = p;
		/* la parada de seguridad: el límite menos 10 °C (o 95 °C sin límite) */
		for (k = 0; k < ns; k++) {
			int lim = S[k].limite != NO ? S[k].limite : 1050;

			/* (solo con lecturas creíbles: el ThinkPad A275 se paraba al segundo
			 * con 114,3 °C de un registro equivocado) */
			if (S[k].t != NO && !S[k].no_fiable && S[k].t >= lim - 100) {
				char a[24], b[24];

				snprintf(motivo, sizeof motivo, "%s llegó a %s (su límite: %s)", S[k].nombre, grados(a, S[k].t), grados(b, lim));
				para_carga(1, 0);
				return;
			}
		}
		/* (TERMO) un sensor del procesador que en 30 s de carga no se mueve
		 * ni un grado no está midiendo: «sin lectura fiable» */
		/* (en un Atom, «aproximado», el sensor es lento: el Toshiba NB250
		 * se quedó en 16 °C los primeros 150 s; entonces se mira a los 3 min) */
		for (k = 0; k < ns && k < VE_MAXS; k++)
			if (S[k].clase == CL_CPU && !(S[k].zona & 0x10000) && seg == (cpu.aproximada ? 180 : 30) && VP.n[k] >= 10 && VP.hi[k] - VP.lo[k] < 10 && !S[k].no_fiable) {
				S[k].no_fiable = VE_NF_QUIETO;
				VP.s[k].no_fiable = VE_NF_QUIETO;
				suceso_log("AVISO: «%s» no se ha movido en %d s de carga (%d,%d °C): de momento, sin lectura fiable", S[k].nombre, seg, VP.lo[k] / 10, VP.lo[k] % 10);
			}
		/* el que no se movía y ahora sube con la carga (3 °C o más): vuelve a contar */
		for (k = 0; k < ns && k < VE_MAXS; k++)
			if (S[k].no_fiable == VE_NF_QUIETO && VP.hi[k] - VP.lo[k] >= 30) {
				S[k].no_fiable = 0;
				VP.s[k].no_fiable = 0;
				VP.s[k].aproximado = 1;
				suceso_log("«%s» ya se mueve con la carga (de %d,%d a %d,%d °C): vuelve a contar, como medida aproximada", S[k].nombre, VP.lo[k] / 10, VP.lo[k] % 10,
					   VP.hi[k] / 10, VP.hi[k] % 10);
			}
		if (seg >= dura_s)
			para_carga(0, 0);
	} else if (fase == F_ENFRIA) {
		if (ve_enfria(&VP, seg, t) || seg >= enfria_max) {
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

/* (TERMO 1.2) la carga fuerte de la tarea hija (TERMO.BIN /CARGA=s /FUERTE):
 * SSE2, como la de los demás procesadores (termo.cas, «termo_ap_fuerte»):
 * siete cadenas de x := x * a + c, dos dobles en cada registro XMM, con a
 * de un bloque de 16 K (cabe en la caché L1 de cualquier procesador con
 * SSE2: sin esperar a la memoria) y c en XMM7. a entre 1 - 2^-14 y
 * 1 - 2^-17, c = 2^-13: x se queda entre 1 y 16 (ni desbordes ni
 * subnormales). Cada vuelta, los siete a la memoria: si alguno se sale,
 * es que alguien le ha tocado los registros (el núcleo los guarda al cambiar
 * de tarea: FXSAVE, tareas.cas) y se dice. */
#define FU_TROZOS 146	/* 146 x 112 octetos: 16.352, casi 16 K */
static double fu_bloque[FU_TROZOS * 14] __attribute__((aligned(16)));
static double fu_suma[2] __attribute__((aligned(16)));
static double fu_x[14] __attribute__((aligned(16)));

static void fuerte_vuelta(int pasadas)
{
	__asm__ volatile("movapd (%[c]), %%xmm7\n\t"
			 "movapd 0(%[x]), %%xmm0\n\tmovapd 16(%[x]), %%xmm1\n\tmovapd 32(%[x]), %%xmm2\n\tmovapd 48(%[x]), %%xmm3\n\t"
			 "movapd 64(%[x]), %%xmm4\n\tmovapd 80(%[x]), %%xmm5\n\tmovapd 96(%[x]), %%xmm6\n"
			 "1:\n\tmovl %[b], %%esi\n\tmovl %[t], %%ecx\n"
			 "2:\n\t"
			 "mulpd (%%esi), %%xmm0\n\taddpd %%xmm7, %%xmm0\n\t"
			 "mulpd 16(%%esi), %%xmm1\n\taddpd %%xmm7, %%xmm1\n\t"
			 "mulpd 32(%%esi), %%xmm2\n\taddpd %%xmm7, %%xmm2\n\t"
			 "mulpd 48(%%esi), %%xmm3\n\taddpd %%xmm7, %%xmm3\n\t"
			 "mulpd 64(%%esi), %%xmm4\n\taddpd %%xmm7, %%xmm4\n\t"
			 "mulpd 80(%%esi), %%xmm5\n\taddpd %%xmm7, %%xmm5\n\t"
			 "mulpd 96(%%esi), %%xmm6\n\taddpd %%xmm7, %%xmm6\n\t"
			 "addl $112, %%esi\n\tdecl %%ecx\n\tjnz 2b\n\t"
			 "decl %%edx\n\tjnz 1b\n\t"
			 "movapd %%xmm0, 0(%[x])\n\tmovapd %%xmm1, 16(%[x])\n\tmovapd %%xmm2, 32(%[x])\n\tmovapd %%xmm3, 48(%[x])\n\t"
			 "movapd %%xmm4, 64(%[x])\n\tmovapd %%xmm5, 80(%[x])\n\tmovapd %%xmm6, 96(%[x])"
			 : "+d"(pasadas)
			 : [c] "r"(fu_suma), [x] "r"(fu_x), [b] "g"((u32)fu_bloque), [t] "i"(FU_TROZOS)
			 : "esi", "ecx", "memory", "cc");
}

static int carga_hija_fuerte(int segundos)
{
	u32 fin = jg_tics() + (u32)segundos * 100, *w, r[4], vueltas = 0;
	int i, mal = 0;

	cpuid(1, r);
	if ((r[3] & 0x05000000u) != 0x05000000u || !(sc(SC_CARGA, 8, 0, 0) & 0x100) || !(sc(SC_CARGA, 8, 0, 0) & 4)) {
		serie("termo: carga fuerte: sin SSE2 (o el núcleo no lo enciende): la ligera\n");
		return carga_hija(segundos);
	}
	w = (u32 *)fu_bloque;
	for (i = 0; i < FU_TROZOS * 14; i++) {	/* (lo mismo que el bloque de termo.cas) */
		w[2 * i] = 0;
		w[2 * i + 1] = 0x3FEFFF80u + (u32)((i * 37) % 64);
	}
	w = (u32 *)fu_suma;
	w[0] = w[2] = 0;
	w[1] = w[3] = 0x3F200000u;	/* 2^-13 */
	for (i = 0; i < 14; i++)
		fu_x[i] = 1.0;
	serie("termo: carga en marcha (fuerte, SSE2)\n");
	for (;;) {
		fuerte_vuelta(3000);
		vueltas++;
		for (i = 0; i < 14; i++)
			if (!(fu_x[i] >= 0.9 && fu_x[i] <= 16.5))
				break;
		if (i < 14 && !mal) {
			mal = 1;
			serie("termo: carga fuerte: MAL: los registros XMM se han salido de lo que dan las cuentas (el %d, en la vuelta %u)\n", i, vueltas);
		}
		if (sc(SC_CARGA, 0, 0, 0) != 1 || (int)(jg_tics() - fin) > 0)
			break;
	}
	serie("termo: carga parada (fuerte: %u vueltas de %u multiplicaciones y sumas de dobles; los registros %s)\n", vueltas, 3000u * FU_TROZOS * 28u,
	      mal ? "MAL" : "bien");
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

/* ---------------------------------------------------------------- lo ocupado
 * (TERMO) Cada 2 s: lo ocupado que está el procesador, con lo que el núcleo
 * no ha corrido la tarea ociosa (el «hlt»; la 224 con 2 y 3: cualquier
 * procesador, también QEMU) y, si los hay, con MPERF/TSC (lo que ha estado
 * despierto, en C0) y lo que el núcleo ha dormido en C6 (Intel de Nehalem en
 * adelante, MSR 3FDh). Sin esto, en el MacBook Air 2012 parecía que el
 * ventilador no enfriaba, y lo que pasaba es que algo no lo dejaba en reposo. */

static void mide_ocupacion(void)
{
	u32 o = sc(SC_CARGA, 2, 0, 0), t = sc(SC_CARGA, 3, 0, 0), lo, hi;
	u64 tsc = rdtsc();

	if (!ES_ERROR(o) && !ES_ERROR(t) && oc_tics && t - oc_tics >= 50) {
		u32 dt = t - oc_tics, dociosa = o - oc_ociosa;

		ocup_pct = dociosa >= dt ? 0 : (int)((dt - dociosa) * 100 / dt);
	}
	if (!ES_ERROR(o) && !ES_ERROR(t))
		oc_ociosa = o, oc_tics = t;
	if (cpu.aperf && lee_msr(0xE7, &lo, &hi)) {
		if (oc_tsc && tsc > oc_tsc) {
			u64 dm = (((u64)hi << 32) | lo) - (((u64)oc_mperfh << 32) | oc_mperf);

			c0_pct = (int)(dm * 100 / (tsc - oc_tsc));
			if (c0_pct > 100)
				c0_pct = 100;
		}
		oc_mperf = lo, oc_mperfh = hi;
	}
	if (hay_cestados && lee_msr(0x3FD, &lo, &hi)) {
		if (oc_tsc && tsc > oc_tsc) {
			u64 d6 = (((u64)hi << 32) | lo) - (((u64)oc_c6h << 32) | oc_c6);

			c6_pct = (int)(d6 * 100 / (tsc - oc_tsc));
			if (c6_pct > 100)
				c6_pct = 100;
		}
		oc_c6 = lo, oc_c6h = hi;
	}
	oc_tsc = tsc;
	if (fase == F_ENFRIA && ocup_pct >= 0)
		ve_ocupacion(&VP, c0_pct >= 0 && c0_pct > ocup_pct ? c0_pct : ocup_pct);
}

/* ---------------------------------------------------------------- los frenos
 * (TERMO) Por qué se frena el procesador (Intel; solo leer): IA32_THERM_STATUS
 * (19Ch, el núcleo) e IA32_PACKAGE_THERM_STATUS (1B1h, el paquete), con su
 * PROCHOT#/FORCEPR# (la placa pide frenar), el umbral crítico y el aviso de
 * límite de potencia; y, de Haswell en adelante, MSR_CORE_PERF_LIMIT_REASONS
 * (690h): PROCHOT, térmico, la alarma térmica del regulador de tensión (VR
 * Therm Alert), la corriente (EDP), los límites de potencia PL1 y PL2... Cada
 * 2 s: el estado (lo de ahora) se cuenta por fase; el registro (lo que queda
 * anotado aunque ya no pase) se dice en el .LOG la primera vez. Para el
 * MSI GT80: si se frena por la placa o por el regulador con el procesador
 * fresco, apunta a la alimentación, no a la temperatura. */
struct freno { u32 msr; int bit; const char *texto; int placa; };
static const struct freno frenos[] = {
	{ 0x19C, 2, "señal PROCHOT externa (la placa pide frenar al núcleo)", 1 },
	{ 0x19C, 4, "temperatura crítica del núcleo", 0 },
	{ 0x19C, 10, "aviso de límite de potencia del núcleo", 0 },
	{ 0x1B1, 2, "señal PROCHOT externa (la placa pide frenar al procesador)", 1 },
	{ 0x1B1, 4, "temperatura crítica del procesador", 0 },
	{ 0x1B1, 10, "aviso de límite de potencia del procesador", 0 },
	{ 0x690, 0, "PROCHOT (límites del núcleo)", 1 },
	{ 0x690, 1, "temperatura (límites del núcleo)", 0 },
	{ 0x690, 6, "alarma de temperatura del regulador de tensión (VRM)", 1 },
	{ 0x690, 7, "corriente máxima del regulador de tensión", 1 },
	{ 0x690, 8, "límite eléctrico (EDP)", 1 },
	{ 0x690, 10, "límite de potencia PL1", 0 },
	{ 0x690, 11, "límite de potencia PL2", 0 },
};
#define NFRENOS (int)(sizeof frenos / sizeof frenos[0])
static int frenos_hay[3];		/* 19Ch, 1B1h, 690h: ¿se pueden leer? (-1 no) */
static u32 frenos_cuenta[NFRENOS][3], frenos_log_dicho[NFRENOS];
static u32 frenos_al_abrir[NFRENOS];	/* (TERMO) anotado ya al abrir el Termómetro: de antes */
static int freno_frio;			/* por la placa con el procesador fresco */

static void mira_frenos(void)
{
	static const u32 msrs[3] = { 0x19C, 0x1B1, 0x690 };
	u32 v[3] = { 0, 0, 0 }, lo, hi;
	int i, f = fase == F_CARGA ? 1 : fase == F_ENFRIA ? 2 : 0, cpu_max = NO, k;

	if (!cpu.intel || !cpu.dts)
		return;
	for (i = 0; i < 3; i++) {
		if (frenos_hay[i] < 0)
			continue;
		if (!lee_msr(msrs[i], &lo, &hi)) {
			frenos_hay[i] = -1;
			continue;
		}
		frenos_hay[i] = 1;
		v[i] = lo;
	}
	for (k = 0; k < ns; k++)
		if (S[k].clase == CL_CPU && !S[k].no_fiable && S[k].t != NO && (cpu_max == NO || S[k].t > cpu_max))
			cpu_max = S[k].t;
	for (i = 0; i < NFRENOS; i++) {
		int m = frenos[i].msr == 0x19C ? 0 : frenos[i].msr == 0x1B1 ? 1 : 2, log = frenos[i].msr == 0x690 ? frenos[i].bit + 16 : frenos[i].bit + 1;

		if (frenos_hay[m] <= 0)
			continue;
		if (v[m] & (1u << frenos[i].bit)) {
			frenos_cuenta[i][f]++;
			if (frenos[i].placa && cpu_max != NO && cpu.tjmax && cpu_max < cpu.tjmax * 10 - 200)
				freno_frio = 1;
		}
		if ((v[m] & (1u << log)) && !frenos_log_dicho[i]) {
			char g[24];

			frenos_log_dicho[i] = 1;
			frenos_al_abrir[i] = cuenta_s <= 4;
			suceso_log("el procesador se ha frenado (o está anotado que se frenó) por: %s%s; ahora, %s%s", frenos[i].texto,
				   cuenta_s <= 4 ? " (ya estaba anotado al abrir el Termómetro)" : "", grados(g, cpu_max),
				   frenos[i].placa && cpu_max != NO && cpu_max < cpu.tjmax * 10 - 200 ? ": con el procesador fresco, apunta a la alimentación o a la placa (regulador de tensión), no a la temperatura" : "");
		}
	}
}

/* (TERMO) ¿se ha frenado en la carga por una señal de la placa o del
 * regulador? ¿se lee el 690h (los límites de potencia)? */
static int frenos_placa_en_carga(void)
{
	int i;

	for (i = 0; i < NFRENOS; i++)
		if (frenos[i].placa && frenos_cuenta[i][1])
			return 1;
	return 0;
}

static int freno_frio_hay(void)
{
	return freno_frio;
}

static int frenos_690(void)
{
	return frenos_hay[2] > 0;
}

/* ¿se ha frenado durante la carga (por lo que sea)? */
static int frenado_en_carga(void)
{
	int i;

	for (i = 0; i < NFRENOS; i++)
		if (frenos_cuenta[i][1])
			return 1;
	return 0;
}

/* lo de los frenos, para el informe (0 si no hay nada) */
static int informe_frenos(char *d, int cabe)
{
	static const char *const nf[3] = { "en reposo", "en carga", "enfriando" };
	int i, n = 0, j;

	for (i = 0; i < NFRENOS; i++) {
		u32 t = frenos_cuenta[i][0] + frenos_cuenta[i][1] + frenos_cuenta[i][2];

		if (!t && !frenos_log_dicho[i])
			continue;
		n += snprintf(d + n, (size_t)(cabe - n), "  - Por %s: ", frenos[i].texto);
		if (t) {
			n += snprintf(d + n, (size_t)(cabe - n), "visto %u veces (cada 2 s):", t);
			for (j = 0; j < 3; j++)
				if (frenos_cuenta[i][j])
					n += snprintf(d + n, (size_t)(cabe - n), " %s %u", nf[j], frenos_cuenta[i][j]);
		} else if (frenos_al_abrir[i])
			n += snprintf(d + n, (size_t)(cabe - n), "ya estaba anotado al abrir el Termómetro (de antes: lo anotó el procesador desde que se encendió el equipo), no visto en la prueba");
		else
			n += snprintf(d + n, (size_t)(cabe - n), "anotado por el procesador (desde que se encendió), no visto en la prueba");
		n += snprintf(d + n, (size_t)(cabe - n), "\r\n");
		if (n >= cabe - 200)
			break;
	}
	/* (TERMO) el MSI GT80: «límite de potencia del núcleo / del procesador /
	 * PL1 / PL2 (ya estaba anotado al abrir)»: qué quiere decir */
	{
		int potencia = 0, visto = 0;

		for (i = 0; i < NFRENOS; i++)
			if ((frenos[i].msr != 0x690 && frenos[i].bit == 10) || (frenos[i].msr == 0x690 && (frenos[i].bit == 10 || frenos[i].bit == 11))) {
				if (frenos_al_abrir[i])
					potencia = 1;
				if (frenos_cuenta[i][0] + frenos_cuenta[i][1] + frenos_cuenta[i][2])
					visto = 1;
			}
		if (potencia)
			n += snprintf(d + n, (size_t)(cabe - n), "  Lo de «límite de potencia» (PL1, PL2) que «ya estaba anotado al abrir»: lo anotó el procesador ANTES de arrancar el Termómetro "
				      "(con el sistema de antes, p. ej. Windows si se reinició desde él, o al arrancar; estas marcas solo se borran al apagar). Quiere decir que en algún momento "
				      "se frenó porque llegó al límite de potencia (los vatios que le deja la BIOS), no por temperatura. En un portátil para juegos con mucha carga es lo normal; "
				      "si pasa con poca carga, o junto a PROCHOT o a la alarma del regulador, apunta a la alimentación (el cargador, la batería o el regulador de la placa).%s\r\n",
				      visto ? "" : " En esta prueba no se ha visto frenarse por eso.");
	}
	if (freno_frio)
		n += snprintf(d + n, (size_t)(cabe - n), "  Se ha frenado por una señal de la placa o del regulador de tensión con el procesador lejos de su límite: "
			      "los datos apuntan más a la alimentación (cargador, batería, regulador de tensión de la placa) que a la temperatura. Conviene que lo revise un técnico.\r\n");
	return n;
}

/* (TERMO 1.1.1) al empezar una prueba de esfuerzo: lo que se cuenta
 * «durante la prueba» (la batería con el cargador, el voltaje en la carga,
 * los parones, los SMI, los Machine Check nuevos), a 0 */
static int mca_len_antes = -1;

static void reinicia_durante(void)
{
	bat_mv_min_carga = 0;
	bat_pct_ca_ini = -1;
	bat_pct_ca_min = 101;
	bat_baja_dicho = 0;
	parones = paron_peor_ms = parado_total_ms = parones_smi = 0;
	bloqueos = bloqueo_peor_ms = 0;
	cmos_atras = cmos_atras_dicho = 0;
	smi_total = 0;
	mca_nuevos = 0;
	if (mca_len_antes < 0)
		mca_len_antes = (int)strlen(mca_texto);
	else if (mca_len_antes < (int)sizeof mca_texto)
		mca_texto[mca_len_antes] = 0;
}

/* y al acabarla (su sección ya escrita): lo contado por fases (los frenos y
 * la potencia), a 0; así el reposo de la siguiente es el de después de esta */
static void reinicia_despues(void)
{
	memset(frenos_cuenta, 0, sizeof frenos_cuenta);
	freno_frio = 0;
	po_nueva_prueba();
}

/* ---------------------------------------------------------------- la potencia
 * (TERMO) potencia.c: los vatios (RAPL), los límites de la BIOS (PL1, PL2,
 * TDP), por qué se frena (19Ch, 1B1h, 690h, 6B0h, 6B1h, 64Fh, cada 2 s y
 * con sus marcas de registro puestas a 0) y los estados de reposo. Cada 2 s,
 * después de mira_frenos (que ve antes las marcas «de antes»). Lo único que
 * se escribe: 0 en esos MSR de motivos (la 220 con EDX = 1, termo.cas) */
static int po_limpia(u32 m)
{
	u32 b[2] = { 0, 0 };

	return sc(SC_MSR, m, (u32)b, 1) == 1;
}

static void po_suceso(const char *t)
{
	suceso_log("%s", t);
}

static void po_serie(const char *t)
{
	serie("%s", t);
}

static void mira_potencia(void)
{
	static int empezada;
	int k, cpu_max = NO;

	if (!empezada) {
		struct po_lector l = { lee_msr, po_limpia, po_suceso, po_serie };

		empezada = 1;
		po_empieza(&l, cpu.intel && cpu.msr_ok, cpu.amd && cpu.msr_ok, cpu.fam, cpu.modelo);
	}
	for (k = 0; k < ns; k++)
		if (S[k].clase == CL_CPU && !S[k].no_fiable && S[k].t != NO && (cpu_max == NO || S[k].t > cpu_max))
			cpu_max = S[k].t;
	po_mide(fase == F_CARGA ? PO_CARGA : fase == F_ENFRIA ? PO_ENFRIA : PO_REPOSO, rdtsc(), tsc_hz,
		hay_bateria && en.pct <= 100 ? (en.ca == 1 ? 1 : en.ca == 0 ? 0 : -1) : -1, cpu_max == NO ? -100000 : cpu_max,
		cpu.tjmax ? cpu.tjmax * 10 : -100000);
}

/* ---------------------------------------------------------------- leer todo */

static void lee_todo(void)
{
	int k;

	for (k = 0; k < ns; k++)
		if (S[k].clase == CL_CPU)
			S[k].t = lee_cpu((u32)S[k].zona);
		else if (S[k].clase == CL_GPU) {
			lee_gpu(k);
			nv_apunta(&NV.g[S[k].zona], fase, S[k].t);	/* (NVIDIA) mínima, media y máxima de cada fase */
		}
	lee_ap();
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
	snprintf(b + m, sizeof b - (size_t)m, " mhz=%u pintados=%u subidos=%u sin_motivo=%u\n", mhz_ahora, cuadros_pintados, puntos_subidos, pinta_sin_motivo);
	serie("%s", b);
}

/* (TERMO) cada segundo por el reloj, no contando pulsos: el marco de los
 * juegos no recupera más de 10 pulsos seguidos, y escribir en la memoria USB
 * puede tardar más de 0,1 s; contando pulsos, el registro salía en el HP
 * Mini cada 4 s en vez de cada 2. Si se pierde un segundo entero, se sigue
 * desde ahora (sin correr para recuperarlo) */
/* (TERMO 1.2) Calentar, esperando a la primera lectura de ACPI */
static void mira_espera_acpi(void)
{
	if (!espera_acpi || fase == F_CARGA)
		return;
	if (!hay_zonas && (int)(jg_tics() - espera_acpi) < 15 * (int)jg_hz)
		return;
	if (!hay_zonas)
		suceso_log("Calentar: ACPI no ha dado nada en 15 s: se empieza sin las zonas ni la batería");
	else
		suceso_log("Calentar: ya está la primera lectura de ACPI (a los %u s de pedirlo): se empieza", (jg_tics() - espera_acpi) / jg_hz);
	espera_acpi = 0;
	if (fase != F_CARGA && !ram_activa() && !midiendo) {
		fase = F_REPOSO;
		empieza();
	}
	jg_cambio = 1;
}


static void pulso(void)
{
	static u32 proximo, lento_dicho;
	u32 ahora = jg_tics(), t0, t_lee = 0, t_zon = 0, esc0, smi0;
	u64 x0;

	if (solo_aqui && carga_on)
		carga_aqui();
	if (!proximo)
		proximo = ahora + jg_hz;
	if ((int)(ahora - proximo) < 0)
		return;
	proximo += jg_hz;
	if ((int)(ahora - proximo) >= 0)
		proximo = ahora + jg_hz;
	t0 = ahora;
	x0 = rdtsc();
	esc0 = tics_escribe;
	smi0 = lee_smi();
	uso_n_ini = uso_lee(&uso_ini);
	cuenta_s++;
	if (fase == F_CARGA)
		ritmo_cuenta(&ri_seg, ahora, jg_hz * 3 / 2);
	lee_todo();
	if (espera_acpi) {	/* (TERMO 1.2) */
		lee_zonas();
		mira_espera_acpi();
	}
	t_lee = jg_tics() - t0;
	if (cuenta_s % 2 == 0) {
		u32 tz = jg_tics();

		if (fase == F_CARGA)
			ritmo_cuenta(&ri_lee, tz, jg_hz * 3);

		lee_zonas();
		t_zon = jg_tics() - tz;
		mide_ocupacion();
		mira_parones();
		mira_alimentacion();
		mira_frenos();
		mira_potencia();
		mira_envio();
		mira_mide();
		{
			char ad[320];	/* (TERMO) los discos (disco.c): cómo va; al acabar, al informe si ya estaba hecho */

			if (disco_mira(carpeta, ad, sizeof ad)) {
				suceso_log("%s", ad);
				/* (TERMO 1.1.1) acabada: a la lista de las pruebas (sin tapar
				 * el cuadro de otra) y, si ya hay informe, otra vez entero */
				if (disco_acabado() && !discos_en_lista) {
					const char *x = busca(ad, "Hecho: ");

					discos_en_lista = 1;
					if (x && x[7] >= '1' && x[7] <= '9') {	/* (con algún disco: sin ninguno, nada en la lista) */
						int i = he_nueva(PR_DISCOS, "Discos (se leen solos al abrir)", 0);

						he_acaba(i, x + 7, 0, 0, 0);
					}
					if (informe_dicho[0])
						escribe_informe();
				}
			}
		}
		mira_ram();
		if (!csv_listo && !csv_malo && cuenta_s >= 6)
			csv_cabecera();
		apunta_historia();
		csv_linea();
		mira_cambios();
		log_datos();
		if (cuenta_s % 10 == 0)
			estado_serie();
	}
	/* (las pruebas: TERMO\\PARON.PRU con «a b»: a los a s de carga, el
	 * Termómetro se queda b s sin poder hacer nada, como en el MSI GT80; el
	 * equipo sigue: los tics, la hija y los demás procesadores) */
	if (paron_a && fase == F_CARGA && (int)((jg_tics() - t_fase) / jg_hz) >= paron_a) {
		u32 h = jg_tics() + (u32)paron_b * jg_hz;

		serie("termo: (prueba) parón de %d s a los %d s de carga\n", paron_b, paron_a);
		paron_a = 0;
		while ((int)(jg_tics() - h) < 0)
			;
	}
	mira_carga();
	prueba_segundo();
	/* (TERMO 1.2.1) la pantalla, ya (antes de escribir en la memoria USB:
	 * lo que tarde eso no la retrasa); luego lo juntado del .LOG y del .CSV
	 * y, sin la carga en marcha, el trozo de la pantalla que se guarda */
	motivo_pinta = 1;
	pinta();
	jg_subir();
	jg_cambio = 0;
	vuelca_diferido(0);
	if (fase != F_CARGA)
		bmp_trozo();	/* (32 K por segundo; con la carga, espera: son muchas órdenes a la memoria) */
	ahora = jg_tics() - t0;
	/* (TERMO) un parón: cuánto, en qué parte, los SMI y quién ha tenido el
	 * procesador; los 5 primeros enteros, y luego uno de cada 10 */
	if (ahora * 1000 / jg_hz >= 900) {
		static struct uso_tabla u2;
		char quien[160], usb[200], sm[120];
		int n2 = uso_lee(&u2), ms = (int)(ahora * 1000 / jg_hz), ms_tsc = (int)ms_de_tsc(rdtsc() - x0);
		u32 es = (tics_escribe - esc0) * 1000 / jg_hz, smi = lee_smi() - smi0;

		lento_dicho++;
		bloqueos++;
		if (ms > bloqueo_peor_ms)
			bloqueo_peor_ms = ms;
		uso_texto(quien, sizeof quien, &uso_ini, uso_n_ini, &u2, n2, ahora);
		usb[0] = 0;
		if (n2 >= 0 && uso_n_ini >= 0 && u2.usb_ordenes != uso_ini.usb_ordenes)
			snprintf(usb, sizeof usb, "; la memoria USB: %u órdenes, %u de más de 0,5 s%s%s", u2.usb_ordenes - uso_ini.usb_ordenes, u2.usb_lentas - uso_ini.usb_lentas,
				 u2.usb_reinicios != uso_ini.usb_reinicios ? ", con reinicios" : "", u2.usb_peor != uso_ini.usb_peor ? " (la más lenta, nueva)" : "");
		/* (TERMO 1.2.1) el rato sin nadie que correr (la tarea ociosa): si
		 * es mucho, se esperaba a algo (la memoria USB, un cerrojo), no al
		 * procesador */
		if (n2 >= 0 && uso_n_ini >= 0 && u2.ociosa - uso_ini.ociosa >= jg_hz / 10) {
			u32 oc = (u2.ociosa - uso_ini.ociosa) * 10 / jg_hz, k = (u32)strlen(usb);

			snprintf(usb + k, sizeof usb - k, "; el procesador, sin nadie que correr: %u,%u s", oc / 10, oc % 10);
		}
		if (smi_hay == 1)
			snprintf(sm, sizeof sm, "%u SMI del firmware", smi);
		else
			snprintf(sm, sizeof sm, "los SMI, este procesador no los cuenta");
		if (lento_dicho <= 5 || lento_dicho % 10 == 0)
			suceso_log("AVISO: leer y apuntar ha tardado %u ms%s: los sensores %u ms, ACPI %u ms (su lectura, la última: %u ms), escribir en la memoria %u ms; en ese rato, %s; "
				   "el equipo entero parado: %d ms (el reloj del procesador frente al del sistema)%s%s%s%s",
				   (u32)ms, lento_dicho > 5 ? " (otra vez)" : "", t_lee * 1000 / jg_hz, t_zon * 1000 / jg_hz, hay_zonas ? za.t.ms_lectura : 0, es, sm,
				   ms_tsc - ms > 0 ? ms_tsc - ms : 0, quien[0] ? "; el procesador lo han tenido: " : "", quien, usb,
				   lento_dicho == 5 ? " (de los siguientes, uno de cada 10)" : "");
	}
}

/* «2 núcleos, 2 hilos por núcleo»; o, si no todos los núcleos tienen los
 * mismos hilos, «10 núcleos: 2 de rendimiento (2 hilos cada uno) y 8
 * eficientes» (los híbridos; sin el CPUID de híbrido, «2 con 2 hilos y 8
 * con 1») */
static const char *topologia_texto(void)
{
	static char t[120];

	if (topo_varios && topo_uno) {
		if (topo_hibrido)
			snprintf(t, sizeof t, "%d núcleos: %d de rendimiento (%d hilos cada uno) y %d %s", nucleos_vistos, topo_varios, topo_hilos_varios, topo_uno,
				 topo_uno == 1 ? "eficiente" : "eficientes");
		else
			snprintf(t, sizeof t, "%d núcleos: %d con %d hilos y %d con 1", nucleos_vistos, topo_varios, topo_hilos_varios, topo_uno);
	} else
		snprintf(t, sizeof t, "%d %s, %d %s%s", nucleos_vistos, nucleos_vistos == 1 ? "núcleo" : "núcleos", hilos_vistos, hilos_vistos == 1 ? "hilo por núcleo" : "hilos por núcleo",
			 topo_hibrido ? (topo_tipo_bsp == 0x20 ? "; híbrido, este es eficiente" : "; híbrido") : "");
	return t;
}

static const char *nucleos_texto(void)
{
	static char t[160];

	if (!ap_cargados)
		return "solo 1 núcleo cargado";
	snprintf(t, sizeof t, "%d procesadores cargados (%s)", ap_cargados + 1, topologia_texto());
	return t;
}

/* ---------------------------------------------------------------- pintar */
struct boton { int x, y, an, al; const char *t; int id; };
enum { B_CALENTAR = 1, B_5, B_10, B_15, B_PARAR, B_CAPTURA, B_ENVIAR, B_TERMINAL, B_MIDE, B_RAM, B_CARGA };
static struct boton bot[12];
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
	if (nbot < 12) {
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
/* (PARPADEO) lo pintado se compara con lo de la vez anterior y solo se suben
 * a la ventana las franjas de filas que han cambiado (y, de cada una, de la
 * primera a la última columna distinta): antes se subía la ventana entera
 * cada segundo y, en el MacBook Air 2012, el puntero del ratón parpadeaba
 * encima. El ratón que solo pasa no hace pintar nada (pinta_sin_motivo
 * lo cuenta: tiene que ser 0; lo dice estado_serie) */

static void vuelca_cambios(void)
{
	int y, y0 = -1, x0 = 0, x1 = 0, an = LZ.an, al = LZ.al;

	if (!antes_p || antes_an != an || antes_al != al) {
		free(antes_p);
		antes_p = (u32 *)malloc((size_t)an * (size_t)al * 4);
		antes_an = an;
		antes_al = al;
		if (antes_p)
			for (y = 0; y < al; y++)
				memcpy(antes_p + (size_t)y * an, LZ.p + (size_t)y * LZ.paso, (size_t)an * 4);
		jg_todo_sucio();
		jg_cambio = 0;
		puntos_subidos += (u32)(an * al);
		return;
	}
	for (y = 0; y <= al; y++) {
		int a = -1, b = -1;

		if (y < al) {
			const u32 *n = LZ.p + (size_t)y * LZ.paso;
			u32 *v = antes_p + (size_t)y * an;

			if (memcmp(n, v, (size_t)an * 4)) {
				for (a = 0; n[a] == v[a]; a++)
					;
				for (b = an - 1; n[b] == v[b]; b--)
					;
				memcpy(v + a, n + a, (size_t)(b - a + 1) * 4);
			}
		}
		if (a >= 0) {
			if (y0 < 0)
				y0 = y, x0 = a, x1 = b;
			else {
				x0 = MIN(x0, a);
				x1 = MAX(x1, b);
			}
		} else if (y0 >= 0) {
			jg_sucio(x0, y0, x1 - x0 + 1, y - y0);
			puntos_subidos += (u32)((x1 - x0 + 1) * (y - y0));
			y0 = -1;
		}
	}
	/* (jg_sucio pide otro cuadro: no hace falta, lo sucio se sube ahora) */
	jg_cambio = 0;
}

/* (TERMO) hasta dónde pueden ir las notas (encima de la franja de la
 * prueba): en el Acer One 10, a 800 x 600 con muchos sensores, salían unas
 * encima de otras; la que no cabe, no se pinta (está en el .LOG) */
static int nota_tope = 1 << 30, notas_fuera;

static int nota(int y, const char *t, u32 c)
{
	int n = lz_parrafo(0, 0, 0, LZ.an - 28, t, 0, 1, 16);

	if (y + 16 * (n > 0 ? n : 1) > nota_tope) {
		notas_fuera++;
		return 0;
	}
	return lz_parrafo(&LZ, 14, y, LZ.an - 28, t, c, 1, 16);
}

/* una línea de la lista, recortada (sin partir una letra) para que quepa en «max» puntos */
static void recorta(char *d, int max)
{
	int n = (int)strlen(d);

	if (ancho_texto(d, 1) <= max)
		return;
	while (n > 0 && ancho_texto(d, 1) + ancho_texto("...", 1) > max) {
		n--;
		while (n > 0 && (d[n] & 0xC0) == 0x80)
			n--;
		d[n] = 0;
	}
	strcpy(d + n, "...");
}

static void pinta_cuadro(int W, int ym)
{
	const struct hecha *v = &HE[he_vista];
	char tit[160], l[480];
	const char *txt = v->tipo == PR_CALENTAR && resumen[0] && he_vista == ultima_calentar() ? resumen : v->linea;
	int an = W - 80, rx = 40, lt, ll = nhechas, esc, al, ry, i, y, desde = 0, cabe = ym - 10 - 84, corto = 0;
	u32 firma;

	snprintf(tit, sizeof tit, "Resultado: %s (prueba %d, %s a %s)", v->nombre, v->num, v->hora, v->hora_fin);
	esc = ancho_texto(tit, 2) <= an - 28 ? 2 : 1;
	lt = lz_parrafo(0, 0, 0, an - 28, txt, 0, 1, 18);
	al = 26 + 8 * esc * 2 + 6 + 18 * lt + 10 + 20 + 18 * ll + 30;
	if (al > cabe && txt != v->linea) {	/* (no cabe todo: el resultado en una línea; el entero, en INFORME.TXT) */
		txt = v->linea;
		corto = 1;
		lt = lz_parrafo(0, 0, 0, an - 28, txt, 0, 1, 18);
		al = 26 + 8 * esc * 2 + 6 + 18 * lt + 10 + 20 + 18 * ll + 30;
	}
	while (al > cabe && ll > 1) {	/* (y si aún no cabe, las últimas de la lista) */
		ll--;
		desde++;
		al -= 18;
	}
	ry = ym - al - 10;
	if (ry < 84)
		ry = 84;
	lz_rect(&LZ, rx + 4, ry + 4, an, al, 0x9A948A);
	lz_rect(&LZ, rx, ry, an, al, 0xFFFDF5);
	lz_marco(&LZ, rx, ry, an, al, C_TINTA);
	lz_marco(&LZ, rx + 1, ry + 1, an - 2, al - 2, C_TINTA);
	lz_negrita(&LZ, rx + 14, ry + 10, tit, C_TINTA, esc);
	y = ry + 10 + 8 * esc * 2 + 6;
	lz_parrafo(&LZ, rx + 14, y, an - 28, txt, C_TINTA, 1, 18);
	y += 18 * lt + 10;
	lz_rect(&LZ, rx + 14, y - 4, an - 28, 1, C_RAYA);
	snprintf(l, sizeof l, "Pruebas de este arranque (%d)%s:", nhechas, desde ? ", las últimas" : "");
	lz_negrita(&LZ, rx + 14, y, l, C_TINTA, 1);
	y += 20;
	for (i = desde; i < nhechas; i++, y += 18) {
		const struct hecha *h = &HE[i];

		snprintf(l, sizeof l, "%s%d. %s a %s  %s: %s", i == he_vista ? "> " : "  ", h->num, h->hora, h->en_marcha ? "..." : h->hora_fin, h->nombre, h->linea);
		recorta(l, an - 28);
		if (i == he_vista)
			lz_negrita(&LZ, rx + 14, y, l, C_TINTA, 1);
		else
			lz_texto(&LZ, rx + 14, y, l, C_TINTA, 1);
	}
	lz_texto(&LZ, rx + 14, ry + al - 22, corto ? "Intro o un clic: cerrar. P: volver a verlo. (El resultado entero, en INFORME.TXT de \\TERMO.)" :
		 "Intro o un clic: cerrar. P: volver a verlo. (Todo queda en INFORME.TXT de \\TERMO.)", C_GRIS, 1);
	/* (las pruebas: lo que enseña, por el puerto serie, cada vez que cambia) */
	firma = (u32)he_vista * 7919u + (u32)nhechas * 131u + (u32)desde * 17u + (u32)corto;
	for (i = 0; i < nhechas; i++)
		firma = firma * 31u + (u32)HE[i].en_marcha + (u32)strlen(HE[i].linea);
	if (firma != cuadro_dicho) {
		static char b[3000];
		int m;

		cuadro_dicho = firma;
		m = snprintf(b, sizeof b, "termo: pantalla: cuadro «%s»; lista:", tit);
		for (i = desde; i < nhechas && m < (int)sizeof b - 200; i++)
			m += snprintf(b + m, sizeof b - (size_t)m, " %s%d. %s %s: %s |", i == he_vista ? ">" : "", HE[i].num, HE[i].hora, HE[i].nombre, HE[i].linea);
		serie("%s\n", b);
	}
}

static void pinta(void)
{
	int W = LZ.an, H = LZ.al, y, k, fila = 30, cn, ca, cmi, cma, cli, ce, ance, lineas;
	char a[200], g[24], h[24], g2[24], g3[24];

	if (fase == F_CARGA)
		ritmo_cuenta(&ri_pin, jg_tics(), jg_hz * 3 / 2);
	nbot = 0;
	cuadros_pintados++;
	if (!motivo_pinta)
		pinta_sin_motivo++;
	motivo_pinta = 0;
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
	if (ocup_pct >= 0) {
		size_t n = strlen(a);

		snprintf(a + n, sizeof a - n, "  ·  ocupado: %d %%", ocup_pct);
	}
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
	/* la tabla: con muchos sensores en poca altura (800 x 600), filas más
	 * bajas, para que quepan la tabla, una nota y la franja de la prueba */
	nota_tope = H - 60 - 30 - 6;
	notas_fuera = 0;
	if (ns > 0 && 110 + ns * fila > nota_tope - 20)
		fila = LIMITA((nota_tope - 20 - 110) / ns, 18, 30);
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
		lz_texto(&LZ, cn, y + (fila - 16) / 2, s->nombre, C_TINTA, 1);
		if (s->fija)
			lz_texto(&LZ, cn + ancho_texto(s->nombre, 1) + 6, y + (fila - 16) / 2, "(fija, no cambia)", C_GRIS, 1);
		if (s->clase == CL_CPU && (s->zona & 0x10000) && s->t == NO)
			lz_texto(&LZ, ca, y + (fila - 16) / 2, "solo en la carga", C_GRIS, 1);
		else if (s->clase == CL_GPU && s->t == NO && gpu_estado[k] > 0 && gpu_estado[k] < NV_E_N)
			lz_texto(&LZ, ca, y + (fila - 16) / 2, nv_estado((u32)gpu_estado[k]), C_GRIS, 1);
		else
			lz_negrita(&LZ, ca, y + (fila - 16) / 2, s->no_fiable ? "sin lectura fiable" : grados(g, s->t), s->no_fiable ? C_PELIGRO : C_TINTA, 1);
		lz_texto(&LZ, cmi, y + (fila - 16) / 2, grados(g, s->min), C_TINTA, 1);
		lz_texto(&LZ, cma, y + (fila - 16) / 2, grados(g, s->max), C_TINTA, 1);
		if (s->limite != NO)
			snprintf(h, sizeof h, "%d °C%s", s->limite / 10, s->supuesto ? " (?)" : "");
		else
			strcpy(h, "no dice");
		lz_texto(&LZ, cli, y + (fila - 16) / 2, h, C_GRIS, 1);
		/* la barra, de 20 °C al límite, con su trama y su estado escrito */
		lz_rect(&LZ, bx, y + 2, ban, fila - 4, 0xECE8E0);
		llena = s->t == NO ? 0 : LIMITA((s->t - 200) * ban / (lim - 200), 0, ban);
		lz_rect(&LZ, bx, y + 2, llena, fila - 4, color_estado[e]);
		trama(bx, y + 2, llena, fila - 4, e, e == E_PELIGRO ? 0xFFFFFF : aclara(color_estado[e], -90));
		lz_marco(&LZ, bx, y + 2, ban, fila - 4, 0x8A8378);
		{
			const char *t = nombre_estado[e];
			int an = ancho_texto(t, 1) + 10;

			lz_rect(&LZ, bx + ban - an - 3, y + (fila - 16) / 2 - 1, an, fila - 12 < 14 ? 14 : fila - 12, 0xFFFFFF);
			lz_negrita(&LZ, bx + ban - an + 2, y + (fila - 16) / 2, t, C_TINTA, 1);
		}
		y += fila;
	}
	/* lo que no se puede leer, dicho */
	lineas = 0;
	a[0] = 0;
	if (cpu.no_hay[0])
		lineas += nota(y + 4 + 16 * lineas, cpu.no_hay, C_PELIGRO);
	if (fecha_mala) {
		char f[200];

		snprintf(f, sizeof f, "La fecha del equipo parece mal (%d): seguramente la pila del reloj está gastada. %s", fecha_mala,
			 pidio_hora ? "Se ha pedido la hora por Internet." : "Con red (cable o Wi-Fi) se pone en hora sola; las horas del registro pueden no valer.");
		lineas += nota(y + 4 + 16 * lineas, f, C_CALIENTE);
	}
	if (cpu.aproximada)
		lineas += nota(y + 4 + 16 * lineas, "La medida del sensor de este Atom es aproximada (sobre todo en frío): menos de 25-30 °C con el equipo encendido no es de fiar; mire cuánto sube, más que la cifra.", C_CALIENTE);
	if (cpu.intel && cpu.dts)
		lineas += nota(y + 4 + 16 * lineas, "Hispa-Pentium usa un solo núcleo: se lee el núcleo 0; el «paquete» es el más caliente de todos.", C_GRIS);
	if (!hay_zonas)
		lineas += nota(y + 4 + 16 * lineas, "Las zonas térmicas de ACPI: esperando al servicio de ACPI (ACPI.BIN)...", C_GRIS);
	else if (za.t.nzonas == 0)
		lineas += nota(y + 4 + 16 * lineas, "La BIOS de este equipo no tiene zonas térmicas de ACPI.", C_GRIS);
	if (!hay_gpus) {
		if (busca(za.t.producto, "GT80"))
			snprintf(a, sizeof a, "No se ve ninguna NVIDIA: en INFORME.TXT («GRÁFICAS»), cómo está cada puerto PCI Express.");
		else
			snprintf(a, sizeof a, "Gráfica dedicada: las NVIDIA Maxwell y Pascal se miden; las demás, solo si la BIOS las da como zona ACPI.");
		lineas += nota(y + 4 + 16 * lineas, a, C_GRIS);
	} else {
		for (k = 0; k < ns; k++)
			if (S[k].clase == CL_GPU && (gpu_frena[k] || gpu_critico[k])) {
				snprintf(a, sizeof a, "%s: %s", S[k].nombre, gpu_critico[k] ? "en temperatura CRÍTICA (lo dice la tarjeta)" : "frenada por temperatura (deducido de la tarjeta)");
				lineas += nota(y + 4 + 16 * lineas, a, C_PELIGRO);
			}
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
			if (hay_zonas && za.t.asus_hay) {
				char v1[16], v2[16];

				if (za.t.asus_vent_cpu == NO && za.t.asus_vent_gpu == NO)
					m += snprintf(a + m, sizeof a - (size_t)m, "  ·  Ventiladores ASUS (WMI): no disponible");
				else
					m += snprintf(a + m, sizeof a - (size_t)m, "  ·  Ventilador CPU (ASUS): %s  ·  Ventilador GPU (ASUS): %s",
						      vent_asus(v1, za.t.asus_vent_cpu), vent_asus(v2, za.t.asus_vent_gpu));
			} else if (hay_zonas && za.t.nvent == 0)
				m += snprintf(a + m, sizeof a - (size_t)m, "  ·  Ventilador: la BIOS no lo da por ACPI (que se oiga)");
		lineas += nota(y + 4 + 16 * lineas, a, C_TINTA);
	}
	y += 8 + 16 * lineas;
	/* abajo: la prueba */
	{
		int yb = H - 60, ym = yb - 30, xb = 10, gal;
		/* (TERMO 1.2.1) antes de Calentar, qué carga está puesta, en grande
		 * (el ASUS GL553VD, 26/11/2026: se probó con la ligera creyendo que
		 * era la fuerte) */
		int rotulo = fuerte_puede && fase != F_CARGA && fase != F_ENFRIA && !midiendo && !ram_activa();

		gal = ym - y - 6 - (rotulo ? 44 : 0);
		if (gal > 60)
			pinta_grafica(10, y, W - 20, gal);
		if (rotulo) {
			static int dicho = -1;
			const char *tr = carga_fuerte ? "Carga: FUERTE (pulse F para la ligera)" : "Carga: LIGERA (pulse F para la fuerte)";
			int an = ancho_texto(tr, 2) + 24, yr = ym - 42;

			lz_rect(&LZ, 10, yr, an < W - 20 ? an : W - 20, 36, carga_fuerte ? C_CALIENTE : C_TEMPLADO);
			lz_negrita(&LZ, 22, yr + 4, tr, C_TINTA, 2);
			if (dicho != carga_fuerte) {
				dicho = carga_fuerte;
				serie("termo: en pantalla: «%s»\n", tr);
			}
		}
		/* la franja de la prueba */
		if (fase == F_CARGA) {
			int seg = (int)((jg_tics() - t_fase) / jg_hz);

			snprintf(a, sizeof a, "CALENTANDO: %s de %s  ·  el más caliente: %s  ·  Esc para parar  ·  %s", tiempo(g, seg), tiempo(h, dura_s),
				 grados(g2, mas_caliente()), solo_aqui ? "carga sin tarea propia (~80 %)" : nucleos_texto());
			lz_rect(&LZ, 10, ym, W - 20, 26, C_CALIENTE);
			trama(10, ym, W - 20, 26, E_CALIENTE, aclara(C_CALIENTE, -40));
			lz_rect(&LZ, 14, ym + 4, ancho_texto(a, 1) + 8, 18, 0xFFFFFF);
			lz_negrita(&LZ, 18, ym + 5, a, C_TINTA, 1);
		} else if (fase == F_ENFRIA) {
			int seg = (int)((jg_tics() - t_fase) / jg_hz);

			snprintf(a, sizeof a, "ENFRIANDO: %s  ·  %s: ahora %s (empezó en %s, llegó a %s)  ·  Esc: acabar ya", tiempo(g, seg),
				 VP.ref >= 0 ? S[VP.ref].nombre : "el más caliente", grados(h, VP.ref >= 0 ? S[VP.ref].t : mas_caliente()), grados(g2, t_ini), grados(g3, pico));
			lz_rect(&LZ, 10, ym, W - 20, 26, C_BIEN);
			lz_rect(&LZ, 14, ym + 4, ancho_texto(a, 1) + 8, 18, 0xFFFFFF);
			lz_negrita(&LZ, 18, ym + 5, a, C_TINTA, 1);
		} else if (midiendo || ram_activa()) {	/* (TERMO 1.1.1) la que está en marcha, aunque no sea Calentar */
			int i = midiendo ? he_mide : he_ram;

			snprintf(a, sizeof a, "EN MARCHA%s%s: %s  ·  Esc para parar", i >= 0 && i < nhechas ? ", prueba " : "",
				 i >= 0 && i < nhechas ? (snprintf(g, sizeof g, "%d", HE[i].num), g) : "", midiendo ? aviso_mide : ram_aviso());
			lz_rect(&LZ, 10, ym, W - 20, 26, C_TEMPLADO);
			trama(10, ym, W - 20, 26, E_TEMPLADO, aclara(C_TEMPLADO, -60));
			lz_rect(&LZ, 14, ym + 4, LIMITA(ancho_texto(a, 1) + 8, 0, W - 28), 18, 0xFFFFFF);
			lz_negrita(&LZ, 18, ym + 5, a, C_TINTA, 1);
		} else if (espera_acpi) {
			lz_negrita(&LZ, 14, ym + 5, "CALENTAR, EN ESPERA: falta la primera lectura de ACPI (la batería y las zonas térmicas); empieza sola en unos segundos (15 como mucho).", C_TINTA, 1);
		} else if (nhechas) {
			snprintf(a, sizeof a, "Pruebas hechas en este arranque: %d (P: verlas con su resultado). Calentar (C), Rendimiento (R), Memoria RAM (M)%s.", nhechas,
				 puede_enviar ? ", Enviar a Carlos (E)" : "");
			lz_texto(&LZ, 14, ym + 5, a, C_TINTA, 1);
		} else {
			lz_texto(&LZ, 14, ym + 5, fuerte_puede ? "Prueba de esfuerzo: elija el tiempo y la carga (F: ligera o fuerte) y pulse «Calentar» (C). Se para sola si se calienta."
							     : "Prueba de esfuerzo: elija el tiempo y pulse «Calentar» (C). Se para sola si se calienta.", C_TINTA, 1);
		}
		boton(xb, yb, 150, 32, "Calentar (C)", B_CALENTAR, fase != F_CARGA, 0);
		xb += 160;
		boton(xb, yb, 90, 32, "5 min (1)", B_5, fase != F_CARGA, minutos == 5);
		xb += 96;
		boton(xb, yb, 96, 32, "10 min (2)", B_10, fase != F_CARGA, minutos == 10);
		xb += 102;
		boton(xb, yb, 96, 32, "15 min (3)", B_15, fase != F_CARGA, minutos == 15);
		xb += 106;
		/* (TERMO 1.2) la carga: ligera o fuerte (marcado: la fuerte) */
		boton(xb, yb, 170, 32, carga_fuerte ? "Carga: fuerte (F)" : "Carga: ligera (F)", B_CARGA, fase != F_CARGA && fuerte_puede, carga_fuerte);
		xb += 186;
		boton(xb, yb, 120, 32, "Parar (Esc)", B_PARAR, fase == F_CARGA || fase == F_ENFRIA || midiendo || ram_activa(), 0);
		xb += 130;
		if (W >= 1000) {
			boton(xb, yb, 150, 32, "Rendimiento (R)", B_MIDE, !midiendo && fase != F_CARGA && fase != F_ENFRIA, midiendo);
			xb += 160;
		}
		if (W >= 1280) {	/* (RAM) (más estrecho, solo la tecla M: que quepan «Enviar a Carlos» y «Terminal») */
			boton(xb, yb, 150, 32, "Memoria RAM (M)", B_RAM, !ram_activa() && !midiendo && fase != F_CARGA && fase != F_ENFRIA, ram_activa());
			xb += 160;
		}
		{
			/* (TERMO) «Enviar a Carlos» y «Terminal», si la memoria está preparada */
			int estrecho = W < 1000, an_env = estrecho ? 100 : 190, an_ter = estrecho ? 90 : 100,
			    an_cap = estrecho ? 50 : 140, pide = (puede_enviar ? an_env + 6 : 0) + (puede_terminal ? an_ter + 6 : 0);

			if (xb + an_cap + pide <= W) {
				boton(xb, yb, an_cap, 32, estrecho ? "F12" : "Captura (F12)", B_CAPTURA, 1, 0);
				xb += an_cap + 6;
			}
			if (puede_enviar && xb + an_env <= W) {
				boton(xb, yb, an_env, 32, estrecho ? "Enviar (E)" : "Enviar a Carlos (E)", B_ENVIAR, 1, enviando);
				xb += an_env + 6;
			}
			if (puede_terminal && xb + an_ter <= W)
				boton(xb, yb, an_ter, 32, "Terminal", B_TERMINAL, 1, 0);
		}
		lz_texto(&LZ, 12, H - 20, ram_activa() ? ram_aviso() : midiendo || aviso_mide[0] ? aviso_mide : aviso_envio[0] ? aviso_envio : ram_aviso()[0] ? ram_aviso() : aviso_archivo,
			 ram_activa() || ram_aviso()[0] || midiendo || aviso_mide[0] || aviso_envio[0] || informe_dicho[0] ? C_TINTA : C_GRIS, 1);
		/* (TERMO) no se puede guardar en la memoria: en grande, en rojo,
		 * encima de la línea de abajo (y lo que se ha hecho en su lugar) */
		if (guarda_aviso[0]) {
			int esc = (int)strlen(guarda_aviso) * 16 + 40 <= W ? 2 : 1, al = esc == 2 ? 44 : 30, i;

			lz_rect(&LZ, 6, H - 30 - al, W - 12, al, 0xB3261E);
			lz_negrita(&LZ, 16, H - 30 - al + (al - 8 * esc * 2 + 4) / 2 + 2, guarda_aviso, 0xFFFFFF, esc);
			/* lo que ha encontrado el sistema (para la foto), encima */
			if (nhallado) {
				int ah = 26 + 16 * nhallado, y0 = H - 30 - al - 6 - ah;

				lz_rect(&LZ, 6, y0, W - 12, ah, 0xFFF3C4);
				lz_negrita(&LZ, 14, y0 + 4, "Qué ha encontrado el sistema al arrancar (haga una foto y mándemela):", C_TINTA, 1);
				for (i = 0; i < nhallado; i++)
					lz_texto(&LZ, 14, y0 + 22 + 16 * i, hallado[i], C_TINTA, 1);
			}
		}
		/* (TERMO 1.2) «Enviar a Carlos»: cómo va y cómo ha ido, en letras */
		if (envio_cartel && (enviando || envio_fin[0])) {
			const char *t = enviando ? (strncmp(aviso_envio, "Envío: ", 8) ? aviso_envio : aviso_envio + 8) : envio_fin;
			int an = W - 120, rx = 60, al = 76 + 18 * lz_parrafo(0, 0, 0, an - 28, t, 0, 1, 18), ry = H - 40 - al - (guarda_aviso[0] ? 50 : 0);

			lz_rect(&LZ, rx + 4, ry + 4, an, al, 0x9A948A);
			lz_rect(&LZ, rx, ry, an, al, enviando ? 0xFFFFFF : envio_ok == 1 ? 0xE6F2E6 : 0xFFF3C4);
			if (!enviando && envio_ok != 1)
				trama(rx, ry, an, 8, E_PELIGRO, C_PELIGRO);
			lz_marco(&LZ, rx, ry, an, al, C_TINTA);
			lz_negrita(&LZ, rx + 14, ry + 14, enviando ? "ENVIANDO A CARLOS..." : envio_ok == 1 ? "ENVIADO A CARLOS" : "NO ENVIADO", C_TINTA, 2);
			lz_parrafo(&LZ, rx + 14, ry + 50, an - 28, t, C_TINTA, 1, 18);
			if (!enviando)
				lz_texto(&LZ, rx + 14, ry + al - 20, "Cualquier tecla: cerrar este aviso (E: enviar otra vez).", C_GRIS, 1);
		}
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
		/* el resultado de la última prueba acabada, encima de la gráfica, y
		 * (TERMO 1.1.1) la lista de todas las de este arranque, para
		 * compararlas sin salir: todo en palabras (el daltonismo), la de
		 * este cuadro marcada con «>» */
		if (ver_cuadro && he_vista >= 0 && he_vista < nhechas)
			pinta_cuadro(W, ym);
	}
	vuelca_cambios();
}

/* ---------------------------------------------------------------- (TERMO) enviar y la terminal */
static int hay_red_ya(void)
{
	u32 ip;

	return sc(97, (u32)&ip, 0, 0) == 4;	/* (SC_ESTADO_RED: RED_CONECTADA) */
}

static const char *sin_red_texto = "No hay red: enchufe el cable de red, conecte el Wi-Fi o el móvil por USB (enchúfelo y active el Anclaje de USB: en Android, Ajustes › Conexión y compartir › Anclaje de USB) (el Wi-Fi: Inicio > Configuración > Red; o en la Consola: REDWIFI /CONECTAR nombre). Luego, otra vez.";

static void lanza_envio(void)
{
	char r[120];
	u32 x;

	if (enviando && (int)(jg_tics() - t_envio) < 600 * (int)jg_hz) {
		snprintf(aviso_envio, sizeof aviso_envio, "Envío: ya se está enviando; espere a que acabe.");
		return;
	}
	envio_cartel = 1;
	if (!hay_red_ya()) {
		char t[800];
		u32 ip, e = sc(97, (u32)&ip, 0, 0);

		/* (TERMO 1.2.1) con enlace y pidiendo la IP (RED_BUSCANDO), no «no hay
		 * cable»: el ASUS GL553VD, con el cable puesto y la luz verde */
		if (e == 3)
			snprintf(t, sizeof t, "NO ENVIADO: sin red todavía: hay cable (o Wi-Fi o el móvil), pero aún no hay dirección IP (se está pidiendo por DHCP). Espere unos segundos y pulse E otra vez; "
				 "si no llega, mire el router (o desenchufe y enchufe el cable). O súbalo al FTP: los archivos siguen en %s.", carpeta);
		else
			snprintf(t, sizeof t, "NO ENVIADO: sin red (%s). Enchufe el cable de red, conecte el Wi-Fi o el móvil por USB (enchúfelo y active el Anclaje de USB: en Android, Ajustes › Conexión y compartir › Anclaje de USB) y pulse E otra vez; "
				 "o súbalo al FTP: los archivos siguen en %s.", e == 2 ? "no hay cable ni Wi-Fi conectado: la tarjeta no tiene enlace" : "no hay cable ni Wi-Fi conectado, o todavía no hay dirección IP", carpeta);
		envio_acaba(t);
		return;
	}
	envio_fin[0] = 0;
	snprintf(r, sizeof r, "%s\\ENVIO.EST", carpeta);
	escribe(r, "Empezando a enviar...", 21, 1);
	suceso_log("Enviar a Carlos: se manda %s", carpeta);
	vuelca_diferido(1);	/* (TERMO 1.2.1) que vaya el .LOG al día */
	x = sc(SC_LANZAR_SUELTO, (u32)"\\TERMO.BIN", (u32)"/ENVIAR /VENTANA", 0);
	if (ES_ERROR(x))
		x = sc(SC_LANZAR_SUELTO, (u32)"\\HISPA\\TERMO.BIN", (u32)"/ENVIAR /VENTANA", 0);
	if (ES_ERROR(x)) {
		char t[300];

		snprintf(t, sizeof t, "NO ENVIADO: no se puede poner en marcha TERMO.BIN /ENVIAR. Súbalo al FTP: los archivos siguen en %s.", carpeta);
		envio_acaba(t);
		return;
	}
	enviando = 1;
	t_envio = jg_tics();
	snprintf(aviso_envio, sizeof aviso_envio, "Envío: Enviando... (empezando)");
}

/* cada 2 s, mientras se envía: lo que dice ENVIO.EST */
static void mira_envio(void)
{
	char r[120], t[800];
	u32 m, n;

	if (!enviando)
		return;
	snprintf(r, sizeof r, "%s\\ENVIO.EST", carpeta);
	m = sc(SC_ABRIR, (u32)r, PARA_LEER, 0);
	if (ES_ERROR(m))
		return;
	n = sc(SC_LEER, m, (u32)t, sizeof t - 1);
	sc(SC_CERRAR, m, 0, 0);
	if (ES_ERROR(n))
		return;
	t[n] = 0;
	if (strcmp(t, aviso_envio + 8)) {
		snprintf(aviso_envio, sizeof aviso_envio, "Envío: %s", t);
		serie("termo: envío dice: %s\n", t);
		if (!strncmp(t, "ENVIADO", 7) || !strncmp(t, "NO ENVIADO", 10) || busca(t, "Hecho") || busca(t, "Enviados ") || busca(t, "No hay ") || busca(t, "no está preparada") || busca(t, "no vale")) {
			enviando = 0;
			envio_acaba(t);
		}
	}
	if (enviando && (int)(jg_tics() - t_envio) > 900 * (int)jg_hz) {
		char x[300];

		enviando = 0;
		snprintf(x, sizeof x, "NO ENVIADO: el envío no ha acabado en 15 minutos (la red va muy lenta o se ha colgado). Pulse E otra vez; o súbalo al FTP: los archivos siguen en %s.", carpeta);
		envio_acaba(x);
	}
}

/* (TERMO 1.2) se acabó el envío (bien o mal): el cartel, el .LOG y el informe */
static void envio_acaba(const char *t)
{
	snprintf(envio_fin, sizeof envio_fin, "%s", t);
	envio_ok = !strncmp(t, "ENVIADO", 7) || busca(t, "Hecho") ? 1 : 0;
	hora_texto(envio_hora, sizeof envio_hora);
	snprintf(aviso_envio, sizeof aviso_envio, "Envío: %s", t);
	envio_cartel = 1;
	serie("termo: envío: resultado: %s\n", t);
	suceso_log("Enviar a Carlos: %s", t);
	escribe_informe();	/* («Enviado a Carlos: ...», también sin ninguna prueba hecha) */
	jg_cambio = 1;
	motivo_pinta = 1;
}

/* (RAM) «Memoria RAM»: TERMO.BIN /RAM, una tarea hija (ram.c); lo que va
 * diciendo, en RAM.EST (al .LOG, cada cambio: si el equipo se cuelga, queda
 * qué se estaba probando); al acabar, su sección en INFORME.TXT */
static void lanza_ram(int completa)
{
	if (ram_activa() || midiendo || fase == F_CARGA || fase == F_ENFRIA || !carpeta[0])
		return;
	if (ram_lanza(carpeta, completa, ram_simula)) {
		he_ram = he_nueva(PR_RAM, completa ? "Memoria RAM (la completa)" : "Memoria RAM (la rápida)", 1);
		suceso_log("EMPIEZA la prueba de la memoria RAM (TERMO.BIN /RAM%s; %s; Esc la para)", completa ? " /COMPLETA" : "",
			   completa ? "la completa: dos pasadas de todo" : "la rápida: menos de 2 min");
	}
	jg_cambio = 1;
}

static void mira_ram(void)
{
	char t[420];
	int acabada;

	if (!ram_mira(t, sizeof t, &acabada))
		return;
	suceso_log("memoria RAM: %s", t);
	if (acabada) {	/* (TERMO 1.1.1) su ficha, con su sección (lo de RAM.TXT de esta vez) y el informe entero */
		static char ri[17500];
		char ban[200];
		int nb = 0, i = he_ram;

		if (i >= 0 && i < nhechas) {
			char fin[12];

			hora_texto(fin, sizeof fin);
			nb = snprintf(ri, sizeof ri, "\r\n=====================================================================\r\n"
				      "PRUEBA %d DE ESTE ARRANQUE: %s, de %s a %s\r\n=====================================================================",
				      HE[i].num, HE[i].nombre, HE[i].hms, fin);
			nb += ram_informe(ri + nb, (int)sizeof ri - nb);
			snprintf(ban, sizeof ban, "%s", !strncmp(t, "Hecho: ", 7) ? t + 7 : t);
			he_acaba(i, ban, ri, nb, 1);
		}
		he_ram = -1;
		escribe_informe();
	}
	jg_cambio = 1;
}

/* al abrir: lo de la vez anterior y, si el menú del arranque lo pide
 * («ramcompleta», «ramrapida»), la prueba, sola */
static void empieza_ram(void)
{
	char t[600];

	if (!carpeta[0])
		return;
	if (ram_al_abrir(carpeta, t, sizeof t)) {
		suceso_log("AVISO: %s", t);
		if (strlen(anterior) + strlen(t) + 4 < sizeof anterior)
			snprintf(anterior + strlen(anterior), sizeof anterior - strlen(anterior), "%s%s", anterior[0] ? " " : "", t);
		ver_anterior = 1;
	}
	ram_simula = ram_linea("ramsimula");
	if (ram_linea("ramcompleta"))
		lanza_ram(1);
	else if (ram_linea("ramrapida"))
		lanza_ram(0);
}

/* (TERMO) «Medir rendimiento»: TERMO.BIN /MIDE, una tarea hija; lo que va
 * diciendo, en MIDE.EST; Esc deja MIDE.PAR y se para */

static void lanza_mide(void)
{
	char r[120];
	u32 x;

	if (midiendo || fase == F_CARGA || fase == F_ENFRIA)
		return;
	snprintf(r, sizeof r, "%s\\MIDE.EST", carpeta);
	escribe(r, "Empezando...", 12, 1);
	x = sc(SC_LANZAR_SUELTO, (u32)"\\TERMO.BIN", (u32)"/MIDE", 0);
	if (ES_ERROR(x))
		x = sc(SC_LANZAR_SUELTO, (u32)"\\HISPA\\TERMO.BIN", (u32)"/MIDE", 0);
	if (ES_ERROR(x)) {
		snprintf(aviso_mide, sizeof aviso_mide, "Rendimiento: no se puede poner en marcha TERMO.BIN /MIDE.");
		return;
	}
	midiendo = 1;
	he_mide = he_nueva(PR_RENDIMIENTO, "Rendimiento", 1);
	disco_pausa(carpeta, 1);	/* (TERMO) los discos esperan al rendimiento */
	t_mide = jg_tics();
	snprintf(aviso_mide, sizeof aviso_mide, "Rendimiento: empezando...");
	suceso_log("EMPIEZA la medida del rendimiento (TERMO.BIN /MIDE; menos de 2 min; Esc la para)");
}

static void para_mide(void)
{
	char r[120];

	snprintf(r, sizeof r, "%s\\MIDE.PAR", carpeta);
	escribe(r, "parar", 5, 1);
}

/* (TERMO 1.1.1) acabada la medida: su ficha, con su sección (RENDIMIE.TXT de esta vez) */
static void acaba_mide(const char *linea, int hecha)
{
	static char rb[7000];
	char fin[12];
	int nb = 0, i = he_mide;

	he_mide = -1;
	if (i < 0 || i >= nhechas)
		return;
	hora_texto(fin, sizeof fin);
	nb = snprintf(rb, sizeof rb, "\r\n=====================================================================\r\n"
		      "PRUEBA %d DE ESTE ARRANQUE: %s, de %s a %s\r\n=====================================================================\r\n"
		      "RENDIMIENTO\r\n-----------\r\n", HE[i].num, HE[i].nombre, HE[i].hms, fin);
	if (hecha) {
		int k = lee_de_carpeta("RENDIMIE.TXT", rb + nb, (int)sizeof rb - nb - 80);
		const char *p = rb + nb;

		if (k >= 3 && !strncmp(p, "\xEF\xBB\xBF", 3)) {	/* (sin la marca UTF-8 de en medio) */
			memmove(rb + nb, rb + nb + 3, (size_t)k - 2);
			k -= 3;
		}
		nb += k > 0 ? k : 0;
		if (k <= 0)
			nb += snprintf(rb + nb, sizeof rb - (size_t)nb, "  (RENDIMIE.TXT no se puede leer: %s)\r\n", linea);
	} else
		nb += snprintf(rb + nb, sizeof rb - (size_t)nb, "  %s\r\n", linea);
	if (nb > 0 && rb[nb - 1] != '\n')
		nb += snprintf(rb + nb, sizeof rb - (size_t)nb, "\r\n");
	he_acaba(i, linea, rb, nb, 1);
	escribe_informe();
}

static void mira_mide(void)
{
	char r[120], t[320];
	u32 m, n;

	if (!midiendo)
		return;
	snprintf(r, sizeof r, "%s\\MIDE.EST", carpeta);
	m = sc(SC_ABRIR, (u32)r, PARA_LEER, 0);
	if (ES_ERROR(m))
		return;
	n = sc(SC_LEER, m, (u32)t, sizeof t - 1);
	sc(SC_CERRAR, m, 0, 0);
	if (ES_ERROR(n))
		return;
	t[n] = 0;
	if (strcmp(t, aviso_mide + 13)) {
		snprintf(aviso_mide, sizeof aviso_mide, "Rendimiento: %s", t);
		if (busca(t, "Hecho") || busca(t, "cancelada") || busca(t, "No hay memoria")) {
			midiendo = 0;
			if (fase != F_CARGA && fase != F_ENFRIA)
				disco_pausa(carpeta, 0);
			suceso_log("medida del rendimiento: %s", t);
			acaba_mide(!strncmp(t, "Hecho: ", 7) ? t + 7 : t, busca(t, "Hecho") != 0);
		}
	}
	if ((int)(jg_tics() - t_mide) > 300 * (int)jg_hz && midiendo) {
		midiendo = 0;
		if (fase != F_CARGA && fase != F_ENFRIA)
			disco_pausa(carpeta, 0);
		acaba_mide("no ha dicho nada en 5 minutos: se da por perdida", 0);
	}
}

/* SSH.INI: equipo=, puerto=, usuario= */
static char ventana_ssh[16];	/* (SSH.INI: ventana=640x400) la medida de la ventana del terminal */
static int graba_ssh;	/* (SSH.INI: grabar=si) lo que llega, a TERMO\SSH\GRABA.BIN (las pruebas) */

static int lee_ssh_ini(char *eq, int ceq, char *us, int cus, u32 *pu)
{
	char r[120], t[600], *p;
	u32 m, n;

	snprintf(r, sizeof r, "%s\\SSH.INI", carpeta);
	m = sc(SC_ABRIR, (u32)r, PARA_LEER, 0);
	if (ES_ERROR(m))
		return 0;
	n = sc(SC_LEER, m, (u32)t, sizeof t - 1);
	sc(SC_CERRAR, m, 0, 0);
	if (ES_ERROR(n))
		return 0;
	t[n] = 0;
	eq[0] = us[0] = 0;
	*pu = 22;
	for (p = t; *p;) {
		char *f = p;

		while (*f && *f != '\r' && *f != '\n')
			f++;
		if (*f)
			*f++ = 0;
		if (!strncmp(p, "equipo=", 7))
			snprintf(eq, (size_t)ceq, "%s", p + 7);
		else if (!strncmp(p, "usuario=", 8))
			snprintf(us, (size_t)cus, "%s", p + 8);
		else if (!strncmp(p, "grabar=si", 9))
			graba_ssh = 1;
		else if (!strncmp(p, "ventana=", 8)) {
			int i;

			for (i = 0; i < (int)sizeof ventana_ssh - 1 && ((p[8 + i] >= '0' && p[8 + i] <= '9') || p[8 + i] == 'x'); i++)
				ventana_ssh[i] = p[8 + i];
			ventana_ssh[i] = 0;
		}
		else if (!strncmp(p, "puerto=", 7))
			for (*pu = 0, p += 7; *p >= '0' && *p <= '9'; p++)
				*pu = *pu * 10 + (u32)(*p - '0');
		while (*f == '\r' || *f == '\n')
			f++;
		p = f;
	}
	/* (solo letras, cifras, «.», «-» y «_»: nada que se cuele en la orden) */
	for (p = eq; *p; p++)
		if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '.' || *p == '-' || *p == '_'))
			return 0;
	for (p = us; *p; p++)
		if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '.' || *p == '-' || *p == '_'))
			return 0;
	return eq[0] && us[0] && *pu > 0 && *pu < 65536;
}

static void lanza_terminal(void)
{
	char eq[80], us[40], a[300];
	u32 pu, x;

	if (!lee_ssh_ini(eq, sizeof eq, us, sizeof us, &pu)) {
		snprintf(aviso_envio, sizeof aviso_envio, "Terminal: %s\\SSH.INI no dice bien el equipo, el puerto y el usuario.", carpeta);
		return;
	}
	if (!hay_red_ya()) {
		snprintf(aviso_envio, sizeof aviso_envio, "%s", sin_red_texto);
		return;
	}
	{
		int n = snprintf(a, sizeof a, "-k %s\\SSH ", carpeta);

		if (ventana_ssh[0])
			n += snprintf(a + n, sizeof a - (size_t)n, "-t %s ", ventana_ssh);
		if (graba_ssh)
			n += snprintf(a + n, sizeof a - (size_t)n, "-g %s\\SSH\\GRABA.BIN ", carpeta);
		snprintf(a + n, sizeof a - (size_t)n, "-p %u %s@%s", pu, us, eq);
	}
	x = sc(SC_LANZAR_SUELTO, (u32)"\\SSH.BIN", (u32)a, 0);
	if (ES_ERROR(x))
		x = sc(SC_LANZAR_SUELTO, (u32)"\\HISPA\\SSH.BIN", (u32)a, 0);
	if (ES_ERROR(x))
		snprintf(aviso_envio, sizeof aviso_envio, "Terminal: no se encuentra SSH.BIN (el paquete «ssh»).");
	else {
		snprintf(aviso_envio, sizeof aviso_envio, "Terminal: conectando con %s@%s (puerto %u)...", us, eq, pu);
		suceso_log("Terminal: SSH a %s@%s, puerto %u", us, eq, pu);
	}
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
		if (ram_activa()) {	/* (RAM) primero, que acabe la de la memoria (o Esc) */
			suceso_log("Calentar: espere a que acabe la prueba de la memoria RAM (Esc la para)");
			break;
		}
		if (midiendo) {	/* (TERMO 1.1.1) ni con «Rendimiento» en marcha: se falsearían las dos */
			suceso_log("Calentar: espere a que acabe «Rendimiento» (Esc la para)");
			break;
		}
		if (fase != F_CARGA) {
			fase = F_REPOSO;
			/* (TERMO 1.2) sin la primera lectura de ACPI (la batería y las
			 * zonas: el ASUS GL553VD, nada más arrancar), se espera a ella,
			 * 15 s como mucho (pulso: mira_espera_acpi), y se dice */
			if (!hay_zonas && !simula) {
				if (!espera_acpi) {
					espera_acpi = jg_tics();
					if (!espera_acpi)
						espera_acpi = 1;
					suceso_log("Calentar: se espera a la primera lectura de ACPI (la batería y las zonas térmicas), 15 s como mucho");
				}
				break;
			}
			empieza();
		}
		break;
	case B_5: pide_minutos(5); break;
	case B_10: pide_minutos(10); break;
	case B_15: pide_minutos(15); break;
	case B_CARGA:	/* (TERMO 1.2) ligera o fuerte; no con la carga en marcha */
		if (fase == F_CARGA)
			break;
		if (!fuerte_puede) {
			suceso_log("Carga fuerte: no se puede (%s): se queda la ligera", sse_nucleo ? "el procesador no tiene SSE2" : "necesita Hispa-Pentium 0.8.12");
			break;
		}
		carga_fuerte = !carga_fuerte;
		suceso_log("la carga, ahora: %s", carga_fuerte ? "fuerte (SSE2: para llegar a PL1/PL2)" : "ligera (x87, la de siempre)");
		break;
	case B_PARAR:
		if (ram_activa())
			ram_para();
		else if (midiendo)
			para_mide();
		else if (fase == F_CARGA)
			para_carga(0, 1);
		else if (fase == F_ENFRIA) {
			fase = F_RESULTADO;
			escribe_resumen();
		}
		break;
	case B_CAPTURA:
		captura();
		break;
	case B_ENVIAR:
		lanza_envio();
		break;
	case B_TERMINAL:
		lanza_terminal();
		break;
	case B_MIDE:
		lanza_mide();
		break;
	case B_RAM:
		lanza_ram(0);
		break;
	}
	jg_cambio = 1;
}

static void suceso(const struct suceso *s)
{
	int i;

	if (s->clase != EV_RATON_MUEVE && s->clase != EV_RATON_SUBE)
		motivo_pinta = 1;	/* (PARPADEO) lo que pinta: teclas, clics, la rueda */

	if (ver_anterior && (s->clase == EV_RATON_BAJA || (s->clase == EV_TECLA && (ES_INTRO(s->a) || s->a == T_ESC)))) {
		ver_anterior = 0;
		jg_cambio = 1;
		return;
	}
	if (s->clase == EV_TECLA) {
		if (envio_cartel && !enviando && s->a != 'e' && s->a != 'E' && s->a != 0x158) {	/* (TERMO 1.2) el cartel del envío, hasta la siguiente tecla */
			envio_cartel = 0;
			jg_cambio = 1;
			if (ES_INTRO(s->a) || s->a == T_ESC)
				return;
		}
		switch (s->a) {
		case 'c': case 'C': haz(B_CALENTAR); break;
		case '1': haz(B_5); break;
		case '2': haz(B_10); break;
		case '3': haz(B_15); break;
		case 'f': case 'F': haz(B_CARGA); break;	/* (TERMO 1.2) */
		case T_ESC: haz(B_PARAR); break;
		case 0x158: captura(); estado_serie(); break;	/* F12 */
		case 'e': case 'E':	/* (TERMO 1.2) sin ENVIO.INI, también se dice */
			if (puede_enviar)
				lanza_envio();
			else {
				char t[300];

				snprintf(t, sizeof t, "NO ENVIADO: esta copia del Termómetro no está preparada para enviar (falta ENVIO.INI). Súbalo al FTP: los archivos siguen en %s.",
					 carpeta[0] ? carpeta : "la carpeta TERMO");
				envio_acaba(t);
			}
			break;
		case 't': case 'T': if (puede_terminal) lanza_terminal(); break;
		case 'r': case 'R': lanza_mide(); break;
		case 'm': case 'M': lanza_ram(0); break;
		case 'p': case 'P':	/* (TERMO 1.1.1) la lista de las pruebas, otra vez */
			if (nhechas) {
				int i;

				for (i = nhechas - 1; i > 0 && HE[i].en_marcha; i--)
					;
				he_vista = i;
				ver_cuadro = !ver_cuadro || he_vista < 0 ? 1 : 0;
				jg_cambio = 1;
			}
			break;
		default:
			if (ES_INTRO(s->a) && (ver_cuadro || fase == F_RESULTADO)) {
				ver_cuadro = 0;
				if (fase == F_RESULTADO)
					fase = F_REPOSO;
				jg_cambio = 1;
			}
		}
	} else if (s->clase == EV_RATON_BAJA) {
		int x = (int)(int32_t)s->a, y = (int)(int32_t)s->b;

		if (ver_cuadro || fase == F_RESULTADO) {
			ver_cuadro = 0;
			if (fase == F_RESULTADO)
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
	vuelca_diferido(1);	/* (TERMO 1.2.1) */
	return 1;
}

static void tamano(int an, int al)
{
	(void)an;
	(void)al;
	jg_cambio = 1;
	motivo_pinta = 1;
}

static const struct juego J = { pulso, pinta, suceso, tamano, al_cerrar, 10 };

/* (TERMO) con un núcleo de antes de la 0.8.9: solo el aviso */
static void pinta_viejo(void)
{
	lz_rect(&LZ, 0, 0, LZ.an, LZ.al, C_FONDO);
	lz_negrita(&LZ, 16, 16, "El Termómetro necesita Hispa-Pentium 0.8.9 o posterior", C_TINTA, 1);
	lz_parrafo(&LZ, 16, 44, LZ.an - 32, "Lee las temperaturas con unas llamadas del núcleo que llegan en la 0.8.9. Actualice el sistema en Programas y actualizaciones (Inicio) y ábralo otra vez.", C_TINTA, 1, 18);
	vuelca_cambios();
}

static const struct juego J_viejo = { 0, pinta_viejo, 0, 0, 0, 10 };

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

		/* (TERMO 1.2.1) de fondo: solo corre cuando no hay otra tarea lista
		 * (el Termómetro, la memoria USB, la red, el escritorio, antes); con
		 * un núcleo de antes, por turnos como hasta ahora */
		if (sc(SC_CARGA, 9, 0, 0) == 0x200)
			serie("termo: carga: de fondo (solo corre cuando no hay otra tarea lista)\n");
		else
			serie("termo: carga: el núcleo no sabe de tareas de fondo (de antes de la 1.2.1): comparte el procesador por turnos\n");
		return arg_aqui("/FUERTE") ? carga_hija_fuerte(s > 0 ? s : 1000) : carga_hija(s > 0 ? s : 1000);
	}
	/* (TERMO) dónde va todo, y «Enviar a Carlos» (TERMO /ENVIAR: desde la
	 * Consola lo dice en la pantalla; con /VENTANA, en ENVIO.EST) */
	arranque_auto = arg_aqui("/ARRANQUE");
	elige_carpeta();
	if (arg_aqui("/MIDE")) {
		char eq[120];
		u32 r[4];
		int i;

		/* (el procesador y el equipo, para el archivo: la marca de CPUID y el SMBIOS que ya tenga el servicio) */
		memset(cpu.marca, 0, sizeof cpu.marca);
		for (i = 0; i < 3; i++) {
			cpuid(0x80000002u + (u32)i, r);
			memcpy(cpu.marca + i * 16, r, 16);
		}
		lee_zonas();
		snprintf(eq, sizeof eq, "%s %s", hay_zonas ? za.t.fabricante : "", hay_zonas ? za.t.producto : "equipo");
		return carpeta[0] ? rendimiento_principal(carpeta, eq, cpu.marca) : 1;
	}
	if (arg_aqui("/DISCO"))		/* (TERMO) los discos de dentro, solo leer (disco.c) */
		return carpeta[0] ? disco_principal(carpeta) : 1;
	if (arg_aqui("/RAM"))	/* (RAM) la prueba de la memoria, en una tarea hija (ram.c) */
		return carpeta[0] ? ram_principal(carpeta, arg_aqui("/COMPLETA"), arg_aqui("/SIMULAFALLO")) : 1;
	if (arg_aqui("/ENVIAR")) {
		if (!carpeta[0]) {
			sc(SC_ESCRIBIR, (u32)"TERMO: no hay carpeta TERMO.\n", 29, 0);
			return 1;
		}
		return envio_principal(carpeta, en_memoria ? unidad : 0, !arg_aqui("/VENTANA"));
	}
	/* (TERMO) las llamadas 220-224 son de la base 0.8.9: con una de antes,
	 * se dice en vez de fallar */
	sin_llamadas = ES_ERROR(sc(SC_CARGA, 3, 0, 0));
	if (sin_llamadas) {
		static const char t[] = "El Termometro necesita Hispa-Pentium 0.8.9 o posterior (las lecturas de temperatura del nucleo). "
					"Actualicelo en Programas y actualizaciones.\n";

		if (sc(SC_DATO, DATO_ESCRITORIO, 0, 0) != 1)
			sc(SC_ESCRIBIR, (u32)t, sizeof t - 1, 0);
		else if (jg_abrir("Termómetro", 620, 140, 0) >= 0)
			jg_correr(&J_viejo);
		return 1;
	}
	/* (TERMO) la fecha: con la pila de la CMOS gastada (el Toshiba NB250
	 * decía 19/03/2010), los nombres de los registros y las horas no valen.
	 * Con red, se pide la hora por Internet (HORARED /AHORA) */
	{
		u32 a, me, d, h, mi, se;

		fecha_hora(&a, &me, &d, &h, &mi, &se);
		if (a < 2025) {
			fecha_mala = (int)a;
			if (hay_red_ya() && !ES_ERROR(sc(SC_LANZAR_SUELTO, (u32)"\\HORARED.BIN", (u32)"/AHORA /FORZAR", 0)))
				pidio_hora = 1;
		}
	}
	puede_enviar = carpeta[0] && envio_hay(carpeta);
	{
		char eq[80], us[40];
		u32 pu;

		puede_terminal = carpeta[0] && lee_ssh_ini(eq, sizeof eq, us, sizeof us, &pu);
	}
	/* (los argumentos, para arg_entero: jg_correr los vuelve a leer) */
	memcpy(jg_args, args, sizeof args);
	simula = arg_aqui("/SIMULA");
	acpi_tarde = ram_linea("acpitarde");	/* (TERMO 1.2, solo las pruebas) */
	/* (TERMO 1.2) la carga fuerte: ¿la puede hacer este núcleo? (la 224 con
	 * EBX = 8: 100h más 4 si ha encendido el SSE2); por omisión, la ligera;
	 * «cargafuerte» en la línea del cargador (o /FUERTE), la fuerte */
	{
		u32 r = sc(SC_CARGA, 8, 0, 0);

		sse_nucleo = ES_ERROR(r) || !(r & 0x100) ? 0 : (int)(r & 0xFF) | 0x100;
		ri_de_fondo = !ES_ERROR(r) && (r & 0x200) ? 1 : 0;	/* (TERMO 1.2.1) */
		fuerte_puede = (sse_nucleo & 0x104) == 0x104;
		carga_fuerte = fuerte_puede && (ram_linea("cargafuerte") || arg_aqui("/FUERTE"));
		serie("termo: coma flotante del núcleo: %s; la carga, por omisión: %s\n",
		      !sse_nucleo ? "núcleo de antes de la 0.8.12 (sin la carga fuerte)" : sse_nucleo & 4 ? "FXSAVE, SSE y SSE2" : sse_nucleo & 2 ? "FXSAVE y SSE (sin SSE2: sin la carga fuerte)" :
		      sse_nucleo & 1 ? "FXSAVE (sin SSE)" : "solo x87 (fnsave)", carga_fuerte ? "fuerte (SSE2)" : "ligera (x87)");
	}
	nv_sololeer = arg_aqui("/SOLOLEER");	/* (NVIDIA) ni lo volátil de nouveau */
	dura_s = arg_entero("/DURA", minutos * 60);
	enfria_max = arg_entero("/ENFRIA", 180);
	mira_procesador();
	cuenta_hilos();
	mira_gpus();
	if (simula) {
		nuevo_sensor("Procesador simulado (prueba)", "Simulado", CL_SIM, 1000, 0);
		nuevo_sensor("Zona simulada (prueba)", "Zona simulada", CL_SIM, 950, 0);
	}
	jg_tamano_maximo(&an, &al);
	if (jg_abrir("Termómetro", an, al, 0) < 0)
		return 1;
	mide_tsc();
	prepara_archivos();
	disco_lanza(carpeta);	/* (TERMO) los discos de dentro, antes de la carga (disco.c) */
	lee_paron_prueba();
	t_inicio = jg_tics();
	lee_todo();
	lee_zonas();
	serie("termo: en marcha, %d sensores; ventana %d x %d\n", ns, LZ.an, LZ.al);
	suceso_log("Termómetro abierto%s: procesador «%s»; %d sensores del procesador%s; registro %s", arg_aqui("/ARRANQUE") ? " (solo, al arrancar desde la memoria)" : "",
		   cpu.marca, ns, cpu.no_hay[0] ? " (no hay: se dice en pantalla)" : "", ruta_csv);
	if (anterior[0])
		serie("termo: aviso: %s\n", anterior);
	mira_mca(0);
	empieza_ram();
	jg_correr(&J);
	sc(SC_CARGA, 1, 0, 0);
	return 0;
}
