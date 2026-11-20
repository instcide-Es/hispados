/* servicio.c - ACPI.BIN, el servicio de ACPI de Hispa-Pentium (docs/PENTIUM.md,
 * «Apagar de verdad y la batería»).
 *
 * Es un programa del anillo 3 que lanza el núcleo al arrancar y que lleva
 * dentro el mismo núcleo de C que HSPACPI.BIN de Hispa-DOS (acpi/nucleo/,
 * con ACPICA): el mismo «modo mínimo» (no se pasa el equipo a modo ACPI ni
 * se usa la interrupción SCI; _REG del controlador embebido; en el HP Mini,
 * PWUP antes de cada lectura y sin _PSR; docs/ACPI.md). Lo de aquí:
 *
 *  1. Arranca la tarea de nucleo.c (LL_INICIA) y la deja correr (LL_PASO);
 *     entre paso y paso se le da al núcleo lo último leído de la batería y
 *     lo que hace falta para apagar y reiniciar (struct pe_energia, llamada
 *     153), y se espera ahí, dormido, a que toque otra vez o a que el núcleo
 *     mande algo.
 *  2. «Apagar» (el núcleo ya ha avisado a los programas y vaciado los
 *     discos): con la tarea quieta, lo de ACPICA para entrar en S5:
 *     AcpiEnterSleepStatePrep (\_S5, _PTS(5), _SST), el modo ACPI si el
 *     equipo no está en él (como APAGAR de Hispa-DOS: SMI_CMD <- ACPI_ENABLE)
 *     y AcpiEnterSleepState (SLP_TYP y luego SLP_EN en PM1a/PM1b_CNT). Si el
 *     equipo sigue encendido, se le dice al núcleo (RESP_FALLO).
 *  3. «Reiniciar»: _REG (3, 0) (como HSPACPI /para) y RESP_PARADO; el
 *     reinicio lo hace el núcleo (el registro de la FADT, 8042, CF9h).
 *  4. El registro (lo que en Hispa-DOS es BATERIA.LOG) sale por el puerto
 *     serie y, con «registro», también a \ACPI.TXT de la unidad del sistema.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "acpi.h"
#include "accommon.h"
#include "pentium.h"

int tarea_quieta(void);
u32 mq_mide_tsc(void);
extern u32 mq_puestos;

#define MONTON (2u << 20)	/* para ACPICA: el HP Mini 5103 pide unos 570 KB */

static struct ll_inicio pet;
static struct pe_energia pe;
static int al_archivo, detalle;
static char trozo[512];

/* ---- el registro, al puerto serie y (con «registro») a \ACPI.TXT ---- */
static void vacia_registro(int archivo)
{
	u32 n, m = 0xFFFFFFFFu;

	if (archivo && al_archivo) {
		m = sc(SC_ABRIR, (u32)"\\ACPI.TXT", 3, 0);
		if (ES_ERROR(m))
			m = 0xFFFFFFFFu;
	}
	while ((n = reg_saca(trozo, sizeof trozo)) != 0) {
		sc(SC_SERIE, (u32)trozo, n, 0);
		if (m != 0xFFFFFFFFu)
			sc(SC_ESCRIBIR_ARCHIVO, m, (u32)trozo, n);
	}
	if (m != 0xFFFFFFFFu)
		sc(SC_CERRAR, m, 0, 0);
}

/* ---- lo de apagar y reiniciar, de la FADT y de \_S5 ---- */
static u32 gas_es(const ACPI_GENERIC_ADDRESS *g)
{
	return g->SpaceId == ACPI_ADR_SPACE_SYSTEM_IO && g->Address < 0x10000 ? (u32)g->Address : 0;
}

/* si ACPICA no ha podido con \_S5 (o con la DSDT), se busca a mano, como
 * APAGAR de Hispa-DOS: Name (_S5_, Package () {a, b, ...}) en la DSDT */
static int s5_a_mano(u32 *a, u32 *b)
{
	ACPI_TABLE_HEADER *t;
	const u8 *p, *f;
	u32 v[2];
	int i;

	if (ACPI_FAILURE(AcpiGetTable(ACPI_SIG_DSDT, 1, &t)))
		return 0;
	p = (const u8 *)t + sizeof *t;
	f = (const u8 *)t + t->Length;
	for (; p + 8 < f; p++) {
		if (p[0] != '_' || p[1] != 'S' || p[2] != '5' || p[3] != '_' || p[4] != 0x12)
			continue;
		if (p[-1] != 0x08 && !(p[-1] == '\\' && p[-2] == 0x08))
			continue;
		p += 5;
		p += 1 + (*p >> 6);		/* la longitud del paquete */
		p++;				/* cuántos elementos */
		for (i = 0; i < 2 && p < f; i++) {
			switch (*p) {
			case 0x00: v[i] = 0; p++; break;
			case 0x01: v[i] = 1; p++; break;
			case 0xFF: v[i] = 0xFF; p++; break;
			case 0x0A: v[i] = p[1]; p += 2; break;
			case 0x0B: v[i] = p[1] | p[2] << 8; p += 3; break;
			case 0x0C: v[i] = p[1] | p[2] << 8 | p[3] << 16 | (u32)p[4] << 24; p += 5; break;
			default: return 0;
			}
		}
		*a = v[0] & 7;
		*b = v[1] & 7;
		reg("ACPI: \\_S5 buscado a mano en la DSDT: %u, %u\n", *a, *b);
		return 1;
	}
	return 0;
}

static void prepara_energia(void)
{
	UINT8 a, b;
	u32 la, lb;
	ACPI_TABLE_HEADER *t;

	pe.version = PE_VERSION;
	pe.reset_esp = 0xFF;
	if (ACPI_FAILURE(AcpiGetTable(ACPI_SIG_FADT, 1, &t))) {
		reg("ACPI: no hay FADT: ni apagar ni reiniciar por ACPI\n");
		return;
	}
	pe.pm1a_cnt = gas_es(&AcpiGbl_FADT.XPm1aControlBlock);
	pe.pm1b_cnt = gas_es(&AcpiGbl_FADT.XPm1bControlBlock);
	pe.smi_cmd = AcpiGbl_FADT.SmiCommand < 0x10000 ? AcpiGbl_FADT.SmiCommand : 0;
	pe.acpi_enable = AcpiGbl_FADT.AcpiEnable;
	if ((AcpiGbl_FADT.Flags & ACPI_FADT_RESET_REGISTER) && AcpiGbl_FADT.ResetRegister.Address) {
		u64 d = AcpiGbl_FADT.ResetRegister.Address;

		switch (AcpiGbl_FADT.ResetRegister.SpaceId) {
		case ACPI_ADR_SPACE_SYSTEM_MEMORY:
			if (d < 0x100000000ull) {
				pe.reset_esp = 0;
				pe.reset_dir = (u32)d;
			}
			break;
		case ACPI_ADR_SPACE_SYSTEM_IO:
			if (d < 0x10000) {
				pe.reset_esp = 1;
				pe.reset_dir = (u32)d;
			}
			break;
		case ACPI_ADR_SPACE_PCI_CONFIG:
			/* (en el bus 0) dispositivo en los bits 32-47, función en 16-31 y registro en 0-15 */
			pe.reset_esp = 2;
			pe.reset_dir = ((u32)(d >> 32) & 31) << 11 | ((u32)(d >> 16) & 7) << 8 | ((u32)d & 0xFF);
			break;
		}
		pe.reset_valor = AcpiGbl_FADT.ResetValue;
	}
	if (ACPI_SUCCESS(AcpiGetSleepTypeData(ACPI_STATE_S5, &a, &b))) {
		la = a;
		lb = b;
	} else if (!s5_a_mano(&la, &lb))
		la = 0xFFFFFFFFu;
	if (la != 0xFFFFFFFFu && pe.pm1a_cnt) {
		pe.s5 = 1;
		pe.slp_typa = la;
		pe.slp_typb = lb;
	}
	reg("ACPI: para apagar: PM1a_CNT %Xh, PM1b_CNT %Xh, \\_S5 %s%u, %u; SMI_CMD %Xh, ACPI_ENABLE %Xh\n",
	    pe.pm1a_cnt, pe.pm1b_cnt, pe.s5 ? "" : "(no se sabe) ", pe.slp_typa, pe.slp_typb, pe.smi_cmd, pe.acpi_enable);
	if (pe.reset_esp == 0xFF)
		reg("ACPI: la FADT no trae registro de reinicio (se reinicia por el teclado o CF9h)\n");
	else
		reg("ACPI: para reiniciar: el registro de la FADT (%s %Xh) <- %Xh\n",
		    pe.reset_esp == 0 ? "memoria" : pe.reset_esp == 1 ? "E/S" : "PCI", pe.reset_dir, pe.reset_valor);
}

/* lo último leído (nucleo.c: apm y salud), para el núcleo */
static void cocina(void)
{
	int i, k = 0;

	pe.fase = apm.fase;
	pe.nbat = apm.nbat;
	pe.pct = apm.pct;
	pe.ca = apm.ca;
	pe.estado = apm.estado;
	pe.cargando = apm.estado == 3;
	pe.minutos = apm.minutos == 0xFFFF ? 0xFFFF : apm.minutos & 0x7FFF;
	pe.lecturas = apm.lecturas;
	pe.salud_pct = salud.pct;
	pe.salud_n = salud.nbat;
	pe.diseno = pe.llena = 0;
	pe.ciclos1 = pe.ciclos2 = 0xFFFF;
	pe.unidad = 1;
	for (i = 0; i < LL_NBAT; i++) {
		if (!salud.bat[i].valida)
			continue;
		pe.unidad = salud.bat[i].unidad;
		pe.diseno += salud.bat[i].diseno;
		pe.llena += salud.bat[i].llena;
		if (k == 0)
			pe.ciclos1 = salud.bat[i].ciclos;
		else if (k == 1)
			pe.ciclos2 = salud.bat[i].ciclos;
		k++;
	}
}

/* deja correr la tarea hasta que esté quieta (fuera de los métodos del
 * equipo): como mucho, unos segundos */
static void hasta_quieta(u32 ms)
{
	u64 fin = mq_ahora_us() + (u64)ms * 1000;

	while (!tarea_quieta() && mq_ahora_us() < fin) {
		int n = nucleo_llamada(LL_PASO, 0);

		if (n < 0)
			break;
		sc(SC_DORMIR, 1, 0, 0);
	}
}

static void apaga(void)
{
	ACPI_STATUS s;
	u32 v = 0;

	hasta_quieta(3000);
	reg("ACPI: el núcleo pide apagar el equipo (S5)\n");
	if (!pe.s5) {
		reg("ACPI: no se sabe apagar este equipo por ACPI\n");
		goto falla;
	}
	s = AcpiEnterSleepStatePrep(ACPI_STATE_S5);
	reg("ACPI: preparar S5 (\\_S5, _PTS, _SST): %s\n", AcpiFormatException(s));
	/* como APAGAR de Hispa-DOS: si el equipo no está en modo ACPI (SCI_EN a
	 * 0) y la FADT dice cómo, se pone; hay BIOS que si no, no apagan */
	if (pe.pm1a_cnt)
		v = mq_es_lee(pe.pm1a_cnt, 2);
	if (!(v & 1) && pe.smi_cmd && pe.acpi_enable) {
		s = AcpiEnable();
		reg("ACPI: modo ACPI (SMI_CMD %Xh <- %Xh): %s\n", pe.smi_cmd, pe.acpi_enable, AcpiFormatException(s));
	}
	vacia_registro(0);
	s = AcpiEnterSleepState(ACPI_STATE_S5);
	reg("ACPI: S5 pedido (%s) y el equipo sigue encendido\n", AcpiFormatException(s));
falla:
	pe.respuesta = RESP_FALLO;
	vacia_registro(0);
}

/* ¿está «p» en lo que le dice el núcleo (la línea del cargador)? Da lo que
 * va detrás, o 0 */
static const char *tiene(const char *t, const char *p)
{
	u32 n = strlen(p);

	for (; *t; t++)
		if (!strncmp(t, p, n))
			return t + n;
	return 0;
}

int servicio_principal(void)
{
	static char args[256];
	const char *p;
	u32 fin, orden, cada = 10;
	int n, preparado = 0;
	u32 vistas = 0xFFFFFFFFu, fase = 0xFFFFFFFFu;

	sc(SC_CORTE, 1, 0, 0);		/* Ctrl+C no acaba con el servicio */
	/* el núcleo apunta qué tarea es el servicio justo después de lanzarlo:
	 * hasta entonces (y para cualquier otro que lo lance), la 153 no vale */
	for (n = 0; n < 100 && ES_ERROR(sc(SC_SERVICIO, 0, 0, 0)); n++)
		sc(SC_DORMIR, 1, 0, 0);
	if (n == 100) {
		static const char m[] = "ACPI.BIN: es el servicio de ACPI; lo lanza el núcleo al arrancar\n";

		sc(SC_ESCRIBIR, (u32)m, sizeof m - 1, 0);
		return 1;
	}
	n = sc(SC_ARGUMENTOS, (u32)args, sizeof args - 1, 0);
	args[n < sizeof args ? n : 0] = 0;
	al_archivo = tiene(args, "registro") != 0;
	detalle = tiene(args, "acpidetalle") != 0;
	if ((p = tiene(args, "acpicada=")) != 0)
		for (cada = 0; *p >= '0' && *p <= '9'; p++)
			cada = cada * 10 + *p - '0';
	if (cada < 1 || cada > 600)
		cada = 10;
	fin = sc(SC_MAS_MEMORIA, MONTON, 0, 0);
	if (ES_ERROR(fin)) {
		static const char m[] = "ACPI: no hay memoria para el servicio\n";

		sc(SC_SERIE, (u32)m, sizeof m - 1, 0);
		return 1;
	}
	if (al_archivo) {
		u32 m = sc(SC_ABRIR, (u32)"\\ACPI.TXT", 2, 0);

		if (ES_ERROR(m))
			al_archivo = 0;
		else
			sc(SC_CERRAR, m, 0, 0);
	}
	pet.base = 0x40000000;
	pet.tam = fin + MONTON;		/* el montón va desde el final de las variables hasta aquí */
	pet.opciones = LL_OP_MINIMO | (detalle ? LL_OP_DETALLE : 0);
	pet.osi = LL_OSI_NADA;
	pet.tsc_ms = mq_mide_tsc();
	pet.cada_s = cada;
	nucleo_llamada(LL_INICIA, (u32)&pet);
	reg("ACPI.BIN: el servicio de ACPI de Hispa-Pentium (núcleo %u); el reloj del procesador da %u vueltas por ms\n",
	    LL_VERSION, pet.tsc_ms);
	pe.version = PE_VERSION;
	pe.reset_esp = 0xFF;
	for (;;) {
		n = nucleo_llamada(LL_PASO, 0);
		if (!preparado && tarea_quieta()) {
			preparado = 1;
			if (apm.fase != FASE_ERROR || mq_rsdp())
				prepara_energia();
			reg("ACPI: el núcleo ha puesto a la vista %u KB de memoria física\n", mq_puestos >> 10);
		}
		if (apm.lecturas != vistas || apm.fase != fase) {
			vistas = apm.lecturas;
			fase = apm.fase;
			cocina();
		}
		vacia_registro(1);
		/* un tic de HSPACPI son 54,9 ms; uno de Hispa-Pentium, 10 */
		n = n < 0 ? 100 : n * 11 / 2;
		if (n < 1)
			n = 1;
		if (n > 100)
			n = 100;
		orden = sc(SC_SERVICIO, (u32)&pe, n, 0);
		if (orden == ORDEN_SIN_ARCHIVOS) {
			al_archivo = 0;
			pe.sin_archivos = 1;
			orden = sc(SC_SERVICIO, (u32)&pe, n, 0);
		}
		if (orden == ORDEN_APAGAR && pe.respuesta == RESP_NADA) {
			apaga();
			sc(SC_SERVICIO, (u32)&pe, 0, 0);
		} else if (orden == ORDEN_REINICIAR && pe.respuesta == RESP_NADA) {
			hasta_quieta(2000);
			nucleo_llamada(LL_PARA, 0);
			reg("ACPI: el núcleo va a reiniciar el equipo: controlador embebido avisado (_REG 3, 0)\n");
			vacia_registro(0);
			pe.respuesta = RESP_PARADO;
			sc(SC_SERVICIO, (u32)&pe, 0, 0);
		}
		if (pe.respuesta != RESP_NADA)
			for (;;)
				sc(SC_SERVICIO, (u32)&pe, 100, 0);
	}
}
