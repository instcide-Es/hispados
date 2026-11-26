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
 *  5. (TERMO) Solo si el Termómetro (TERMO.BIN) lo está pidiendo (la llamada
 *     223 lo dice), cada 2 s y con la tarea quieta: _TMP de las zonas
 *     térmicas (y una vez sus _CRT, _PSV, _HOT y _AC0), los ventiladores
 *     (PNP0C0B: _FST, _STA, _PSC) y, una vez, lo que dice el SMBIOS (el
 *     fabricante y el modelo del equipo, la placa, la BIOS). Solo se leen.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "acpi.h"
#include "accommon.h"
#include "pentium.h"
#include "../../castel/nucleo32/termo/termo_acpi.h"	/* (TERMO) */

int tarea_quieta(void);
void hp_radio_borra(void);	/* (HPRADIO) hpradio.c */
void hp_radio(int solo_ver);
u32 mq_mide_tsc(void);
extern u32 mq_puestos;

#define MONTON (2u << 20)	/* para ACPICA: el HP Mini 5103 pide unos 570 KB */

static struct ll_inicio pet;
static struct pe_energia pe;
static int al_archivo, detalle;
static char trozo[512];

/* ---- el registro, al puerto serie y (con «registro») a \ACPI.TXT ---- */
/* (PANEL) al puerto serie, de línea en línea (cada una de una vez: la
 * llamada 61 toma el cerrojo de la consola), para que lo que dice el núcleo
 * a la vez («escritorio: en marcha...») no caiga en medio de una línea; lo
 * que queda sin su fin de línea espera a la vez siguiente */
static char linea_serie[256];
static u32 n_linea;

static void serie_por_lineas(const char *t, u32 n)
{
	u32 i;

	for (i = 0; i < n; i++) {
		linea_serie[n_linea++] = t[i];
		if (t[i] == '\n' || n_linea == sizeof linea_serie) {
			sc(SC_SERIE, (u32)linea_serie, n_linea, 0);
			n_linea = 0;
		}
	}
}

static void vacia_registro(int archivo)
{
	u32 n, m = 0xFFFFFFFFu;

	if (archivo && al_archivo) {
		m = sc(SC_ABRIR, (u32)"\\ACPI.TXT", 3, 0);
		if (ES_ERROR(m))
			m = 0xFFFFFFFFu;
	}
	while ((n = reg_saca(trozo, sizeof trozo)) != 0) {
		serie_por_lineas(trozo, n);	/* (PANEL) */
		if (m != 0xFFFFFFFFu)
			sc(SC_ESCRIBIR_ARCHIVO, m, (u32)trozo, n);
	}
	if (m != 0xFFFFFFFFu)
		sc(SC_CERRAR, m, 0, 0);
}

/* (TERMO) maquina.c, antes de apuntar una escritura del AML en \ACPIESC.TXT:
 * lo que lleve el registro, ya a \ACPI.TXT (para ver en qué paso iba) */
void mq_vacia_registro(void)
{
	vacia_registro(1);
}

/* ---- lo de apagar y reiniciar, de la FADT y de \_S5 ---- */
static u32 gas_es(const ACPI_GENERIC_ADDRESS *g)
{
	return g->SpaceId == ACPI_ADR_SPACE_SYSTEM_IO && g->Address < 0x10000 ? (u32)g->Address : 0;
}

/* (PANEL) el botón de encendido: PWRBTN_STS (el bit 8 de PM1a_STS) se mira
 * en cada vuelta del servicio (como mucho, cada segundo) y se borra
 * escribiendo un 1; para que el equipo lo apunte hace falta PWRBTN_EN (el
 * bit 8 de PM1a_EN, la segunda mitad del bloque). Sin el modo ACPI, en
 * muchos equipos el botón lo atiende la BIOS (SMI) y aquí no llega; en
 * QEMU (system_powerdown) y en los que ya arrancan en modo ACPI, sí. El
 * núcleo decide qué hacer (Panel de control > Energía) */
static u32 pm1a_evt, pm1_largo;

static void prepara_boton(void)
{
	u32 en;

	if (!pm1a_evt || pm1_largo < 4)
		return;
	en = mq_es_lee(pm1a_evt + pm1_largo / 2, 2);
	mq_es_escribe(pm1a_evt + pm1_largo / 2, 2, en | 0x100);
	mq_es_escribe(pm1a_evt, 2, 0x100);	/* lo que hubiera de antes, fuera */
	reg("ACPI: el botón de encendido, en PM1a_EVT %xh (PWRBTN_EN puesto)\n", pm1a_evt);
}

static void mira_boton(void)
{
	if (!pm1a_evt || pm1_largo < 4)
		return;
	if (mq_es_lee(pm1a_evt, 2) & 0x100) {
		mq_es_escribe(pm1a_evt, 2, 0x100);
		pe.boton++;
		reg("ACPI: se ha pulsado el botón de encendido (%u)\n", pe.boton);
	}
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
	pm1a_evt = gas_es(&AcpiGbl_FADT.XPm1aEventBlock);	/* (PANEL) el botón de encendido */
	pm1_largo = AcpiGbl_FADT.Pm1EventLength;
	prepara_boton();
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

/* ---- (dispositivos) lo que hay en el espacio de nombres, para el
 * Administrador de dispositivos del escritorio (la llamada 156) ----
 * Se recorren los dispositivos que tienen _HID (sin los enlaces de
 * interrupción, PNP0C0F, que son muchos y no dicen nada), con su _STA (los
 * que dicen que no están, fuera), su ruta y lo que dice su _CRS (la IRQ,
 * los puertos y la memoria). Se hace una vez, con la tarea quieta, después
 * de la primera lectura de la batería (así va también su marca y su
 * modelo). Con «acpisindisp» en la línea del cargador, no se hace. */
static struct pe_dispositivo disp;
static u32 n_disp;

static void pon_texto(char *d, const char *o, u32 cabe)
{
	u32 i;

	for (i = 0; i + 1 < cabe && o[i]; i++)
		d[i] = o[i];
	d[i] = 0;
}

static void recurso(u32 clase, u32 base, u32 largo)
{
	if (disp.n_recursos >= 6 || !largo)
		return;
	disp.recursos[disp.n_recursos].clase = clase;
	disp.recursos[disp.n_recursos].base = base;
	disp.recursos[disp.n_recursos].largo = largo;
	disp.n_recursos++;
}

static ACPI_STATUS un_recurso(ACPI_RESOURCE *r, void *ctx)
{
	(void)ctx;
	switch (r->Type) {
	case ACPI_RESOURCE_TYPE_IRQ:
		if (disp.irq == 0xFFFFFFFFu && r->Data.Irq.InterruptCount)
			disp.irq = r->Data.Irq.Interrupts[0];
		break;
	case ACPI_RESOURCE_TYPE_EXTENDED_IRQ:
		if (disp.irq == 0xFFFFFFFFu && r->Data.ExtendedIrq.InterruptCount)
			disp.irq = r->Data.ExtendedIrq.Interrupts[0];
		break;
	case ACPI_RESOURCE_TYPE_IO:
		recurso(DISP_PUERTOS, r->Data.Io.Minimum, r->Data.Io.AddressLength);
		break;
	case ACPI_RESOURCE_TYPE_FIXED_IO:
		recurso(DISP_PUERTOS, r->Data.FixedIo.Address, r->Data.FixedIo.AddressLength);
		break;
	case ACPI_RESOURCE_TYPE_MEMORY32:
		recurso(DISP_MEMORIA, r->Data.Memory32.Minimum, r->Data.Memory32.AddressLength);
		break;
	case ACPI_RESOURCE_TYPE_FIXED_MEMORY32:
		recurso(DISP_MEMORIA, r->Data.FixedMemory32.Address, r->Data.FixedMemory32.AddressLength);
		break;
	}
	return AE_OK;
}

static int es(const char *a, const char *b)
{
	return !strcmp(a, b);
}

static ACPI_STATUS un_dispositivo(ACPI_HANDLE h, UINT32 nivel, void *ctx, void **ret)
{
	ACPI_PNP_DEVICE_ID *hid;
	ACPI_BUFFER b;
	ACPI_OBJECT o;
	UINT64 sta = 0x0F;
	char ruta[64];
	int i;

	(void)nivel; (void)ctx; (void)ret;
	if (n_disp >= 60)
		return AE_CTRL_TERMINATE;
	if (ACPI_FAILURE(AcpiUtExecute_HID((ACPI_NAMESPACE_NODE *)h, &hid)))
		return AE_OK;
	memset(&disp, 0, sizeof disp);
	pon_texto(disp.pnp, hid->String, sizeof disp.pnp);
	ACPI_FREE(hid);
	if (es(disp.pnp, "PNP0C0F"))
		return AE_OK;
	/* _STA: si no está (bit 0), fuera; las baterías, con su bit 4 */
	b.Length = sizeof o;
	b.Pointer = &o;
	if (ACPI_SUCCESS(AcpiEvaluateObjectTyped(h, "_STA", NULL, &b, ACPI_TYPE_INTEGER)))
		sta = o.Integer.Value;
	if (!(sta & 1))
		return AE_OK;
	b.Length = sizeof ruta;
	b.Pointer = ruta;
	if (ACPI_FAILURE(AcpiGetName(h, ACPI_FULL_PATHNAME, &b)))
		ruta[0] = 0;
	disp.bus = DISP_BUS_ACPI;
	disp.direccion = ++n_disp;
	disp.padre = 0xFFFFFFFFu;
	disp.irq = 0xFFFFFFFFu;
	disp.dato1 = (u32)sta;
	pon_texto(disp.nombre, ruta, sizeof disp.nombre);
	AcpiWalkResources(h, METHOD_NAME__CRS, un_recurso, NULL);
	/* quién lo lleva */
	if (es(disp.pnp, "PNP0C0A")) {
		pon_texto(disp.controlador, "ACPI.BIN", sizeof disp.controlador);
		disp.estado = DISP_FUNCIONA;
		for (i = 0; i < LL_NBAT; i++)
			if (estado.bat[i].hay && !strcmp(estado.bat[i].ruta, ruta)) {
				pon_texto(disp.marca, estado.bat[i].marca, sizeof disp.marca);
				snprintf(disp.detalle, sizeof disp.detalle, "%s%s%s%s", estado.bat[i].modelo,
					 estado.bat[i].tipo[0] ? " (" : "", estado.bat[i].tipo, estado.bat[i].tipo[0] ? ")" : "");
			}
		if (!(sta & 0x10))
			pon_texto(disp.detalle, "el hueco está vacío (no hay batería puesta)", sizeof disp.detalle);
	} else if (es(disp.pnp, "ACPI0003") || es(disp.pnp, "PNP0C09")) {
		pon_texto(disp.controlador, "ACPI.BIN", sizeof disp.controlador);
		disp.estado = DISP_FUNCIONA;
	} else if (es(disp.pnp, "PNP0A03") || es(disp.pnp, "PNP0A08")) {
		pon_texto(disp.controlador, "pci.cas", sizeof disp.controlador);
		disp.estado = DISP_FUNCIONA;
	} else if (es(disp.pnp, "PNP0C02") || es(disp.pnp, "PNP0C01") || es(disp.pnp, "PNP0A05") ||
		   es(disp.pnp, "PNP0A06") || es(disp.pnp, "ACPI0010") || es(disp.pnp, "ACPI0004") ||
		   es(disp.pnp, "ACPI0006") || es(disp.pnp, "QEMU0002") || es(disp.pnp, "PNP0C04") ||
		   es(disp.pnp, "PNP0200") || es(disp.pnp, "PNP0800") || es(disp.pnp, "ACPI0007") ||
		   es(disp.pnp, "PNP0C80") || es(disp.pnp, "INT0800"))
		disp.estado = DISP_NO_HACE_FALTA;
	else
		disp.estado = DISP_SIN_CONTROLADOR;
	sc(SC_DISPOSITIVO, (u32)&disp, 0, 0);
	return AE_OK;
}

static void apunta_dispositivos(void)
{
	n_disp = 0;
	AcpiWalkNamespace(ACPI_TYPE_DEVICE, ACPI_ROOT_OBJECT, ACPI_UINT32_MAX, un_dispositivo, NULL, NULL, NULL);
	reg("ACPI: %u dispositivos del espacio de nombres, apuntados para el Administrador de dispositivos\n", n_disp);
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

/* ---- (TERMO) las zonas térmicas, los ventiladores y el SMBIOS, para el
 * Termómetro (TERMO.BIN, castel/nucleo32/termo/): la llamada 223 ---- */
#define SC_TERMO 223
/* (el núcleo copia TA_TAM octetos: los de detrás, a cero) */
static union { struct termo_acpi t; char b[TA_TAM]; } ta_u;
#define ta ta_u.t
static ACPI_HANDLE ta_zh[TA_ZONAS], ta_fh[TA_VENT];
static int ta_buscado;

/* (si «estado» no es 0: lo que dijo ACPICA y lo que devolvió, para saber
 * por qué no hay dato) */
static int32_t ta_kelvin_de(ACPI_HANDLE h, const char *m, uint32_t *estado, int32_t *crudo)
{
	ACPI_BUFFER b;
	ACPI_OBJECT o;
	ACPI_STATUS s;
	UINT64 v;

	b.Length = sizeof o;
	b.Pointer = &o;
	s = AcpiEvaluateObjectTyped(h, (char *)m, NULL, &b, ACPI_TYPE_INTEGER);
	if (estado)
		*estado = s;
	if (crudo)
		*crudo = -1;
	if (ACPI_FAILURE(s))
		return TA_NO;
	v = o.Integer.Value;
	if (crudo)
		*crudo = v > 0x7FFFFFFF ? 0x7FFFFFFF : (int32_t)v;
	/* en décimas de kelvin; lo que no puede ser (0, basura), fuera */
	if (v < 2000 || v > 4732)
		return TA_NO;
	return (int32_t)v - 2732;
}

static int32_t ta_kelvin(ACPI_HANDLE h, const char *m)
{
	return ta_kelvin_de(h, m, 0, 0);
}

static ACPI_STATUS ta_una_zona(ACPI_HANDLE h, UINT32 nivel, void *ctx, void **ret)
{
	ACPI_BUFFER b;
	struct ta_zona *z;

	(void)nivel; (void)ctx; (void)ret;
	if (ta.nzonas >= TA_ZONAS)
		return AE_CTRL_TERMINATE;
	z = &ta.zona[ta.nzonas];
	ta_zh[ta.nzonas++] = h;
	b.Length = sizeof z->nombre;
	b.Pointer = z->nombre;
	if (ACPI_FAILURE(AcpiGetName(h, ACPI_FULL_PATHNAME, &b)))
		pon_texto(z->nombre, "?", sizeof z->nombre);
	z->crt = ta_kelvin(h, "_CRT");
	z->psv = ta_kelvin(h, "_PSV");
	z->hot = ta_kelvin(h, "_HOT");
	z->ac0 = ta_kelvin(h, "_AC0");
	z->tmp = TA_NO;
	reg("ACPI: (TERMO) zona térmica %s: _CRT %d, _PSV %d, _HOT %d, _AC0 %d (décimas de grado)\n",
	    z->nombre, z->crt, z->psv, z->hot, z->ac0);
	return AE_OK;
}

static ACPI_STATUS ta_un_ventilador(ACPI_HANDLE h, UINT32 nivel, void *ctx, void **ret)
{
	ACPI_PNP_DEVICE_ID *hid;
	ACPI_BUFFER b;
	int es_vent;

	(void)nivel; (void)ctx; (void)ret;
	if (ta.nvent >= TA_VENT)
		return AE_CTRL_TERMINATE;
	if (ACPI_FAILURE(AcpiUtExecute_HID((ACPI_NAMESPACE_NODE *)h, &hid)))
		return AE_OK;
	es_vent = !strcmp(hid->String, "PNP0C0B");
	ACPI_FREE(hid);
	if (!es_vent)
		return AE_OK;
	b.Length = sizeof ta.vent[0].nombre;
	b.Pointer = ta.vent[ta.nvent].nombre;
	if (ACPI_FAILURE(AcpiGetName(h, ACPI_FULL_PATHNAME, &b)))
		pon_texto(ta.vent[ta.nvent].nombre, "?", sizeof ta.vent[0].nombre);
	reg("ACPI: (TERMO) ventilador %s\n", ta.vent[ta.nvent].nombre);
	ta_fh[ta.nvent++] = h;
	return AE_OK;
}

static void ta_lee_vent(int i)
{
	ACPI_BUFFER b;
	ACPI_OBJECT *o;
	ACPI_BUFFER e;
	ACPI_OBJECT x;
	struct ta_vent *v = &ta.vent[i];

	v->velocidad = v->control = TA_NO;
	v->encendido = -1;
	b.Length = ACPI_ALLOCATE_BUFFER;
	b.Pointer = NULL;
	v->estado_fst = (uint16_t)AcpiEvaluateObjectTyped(ta_fh[i], "_FST", NULL, &b, ACPI_TYPE_PACKAGE);
	v->estado_psc = 0;
	if (ACPI_SUCCESS(v->estado_fst)) {
		o = b.Pointer;
		if (o->Package.Count >= 3 && o->Package.Elements[1].Type == ACPI_TYPE_INTEGER &&
		    o->Package.Elements[2].Type == ACPI_TYPE_INTEGER) {
			UINT64 c = o->Package.Elements[1].Integer.Value, r = o->Package.Elements[2].Integer.Value;

			if (c <= 100)
				v->control = (int32_t)c;
			if (r < 0xFFFFFFFFull && r < 100000)
				v->velocidad = (int32_t)r;
			if (v->velocidad != TA_NO)
				v->encendido = v->velocidad > 0;
		}
		ACPI_FREE(b.Pointer);
	}
	if (v->encendido < 0) {
		e.Length = sizeof x;
		e.Pointer = &x;
		v->estado_psc = (uint16_t)AcpiEvaluateObjectTyped(ta_fh[i], "_PSC", NULL, &e, ACPI_TYPE_INTEGER);
		if (ACPI_SUCCESS(v->estado_psc))
			v->encendido = x.Integer.Value == 0;
	}
}

/* (TERMO) los ASUS: \_SB.ATKD.WMNB (0, DSTS, {id, 0}) -> lo que dice el EC de
 * ese «dispositivo» (como asus-wmi de Linux). Solo DSTS (0x53545344), que es
 * leer; nunca DEVS (0x53564544), que escribe. ~0: no existe; sin el bit
 * 0x00010000: no está. -1 si no vale */
static ACPI_HANDLE ta_atkd;
static u32 ta_asus_crudo;	/* lo último que ha contestado, tal cual (0xFFFFFFFF: el método falla) */

static int64_t ta_asus(u32 id)
{
	ACPI_OBJECT a[3], r;
	ACPI_OBJECT_LIST l = { 3, a };
	ACPI_BUFFER b = { sizeof r, &r };
	static u8 entrada[8];
	u32 v;

	entrada[0] = id, entrada[1] = id >> 8, entrada[2] = id >> 16, entrada[3] = id >> 24;
	entrada[4] = entrada[5] = entrada[6] = entrada[7] = 0;
	a[0].Type = ACPI_TYPE_INTEGER;
	a[0].Integer.Value = 0;
	a[1].Type = ACPI_TYPE_INTEGER;
	a[1].Integer.Value = 0x53545344;	/* DSTS */
	a[2].Type = ACPI_TYPE_BUFFER;
	a[2].Buffer.Length = 8;
	a[2].Buffer.Pointer = entrada;
	ta_asus_crudo = 0xFFFFFFFFu;
	if (ACPI_FAILURE(AcpiEvaluateObjectTyped(ta_atkd, "WMNB", &l, &b, ACPI_TYPE_INTEGER)))
		return -1;
	v = (u32)r.Integer.Value;
	ta_asus_crudo = v;
	if (v == 0xFFFFFFFEu || v == 0xFFFFFFFFu || !(v & 0x00010000))
		return -1;
	return v;
}

/* un ventilador de ASUS: r.p.m.; 0 si está parado (con el bit de «está» y 0,
 * o, sin el bit, si alguna vez ha dado vueltas: en el GL553VD, el del CPU
 * contesta así hasta que arranca); TA_NO, «no disponible» */
static int32_t ta_asus_vent(u32 id, int *visto, uint32_t *crudo)
{
	int64_t v = ta_asus(id);

	*crudo = ta_asus_crudo;
	if (v >= 0 && (v & 0xFFFF) < 200) {
		if (v & 0xFFFF)
			*visto = 1;
		return (int32_t)(v & 0xFFFF) * 100;
	}
	if (*visto && ta_asus_crudo != 0xFFFFFFFFu && ta_asus_crudo != 0xFFFFFFFEu)
		return 0;
	return TA_NO;
}

static void ta_lee_asus(void)
{
	static int visto_cpu, visto_gpu;
	int64_t v;
	u32 antes = ec_ignoradas;

	ta.asus_vent_cpu = ta.asus_vent_gpu = ta.asus_temp = TA_NO;
	if (!ta.asus_hay)
		return;
	ta.asus_vent_cpu = ta_asus_vent(0x00110013, &visto_cpu, &ta.asus_crudo_cpu);
	ta.asus_vent_gpu = ta_asus_vent(0x00110014, &visto_gpu, &ta.asus_crudo_gpu);
	v = ta_asus(0x00110011);
	if (v >= 0 && (v & 0xFFFF) >= 2732 && (v & 0xFFFF) <= 4232)
		ta.asus_temp = (int32_t)(v & 0xFFFF) - 2732;
	if (ec_ignoradas != antes)
		ta.asus_escribe = 1;
}

/* lo de un texto del SMBIOS (el número k de los de detrás de la ficha) */
static void ta_texto_smbios(char *d, u32 cabe, const u8 *f, u32 k)
{
	const char *t = (const char *)f + f[1];
	u32 i;

	d[0] = 0;
	if (k == 0)
		return;
	while (--k && *t)
		t += strlen(t) + 1;
	if (!*t)
		return;
	for (i = 0; i + 1 < cabe && t[i]; i++)
		d[i] = (t[i] >= 32 && t[i] < 127) ? t[i] : '?';
	d[i] = 0;
	/* (sin los blancos del final) */
	while (i > 0 && d[i - 1] == ' ')
		d[--i] = 0;
}

static void ta_smbios(void)
{
	/* (UEFI) el punto de entrada: el que dice el núcleo («smbios=», por UEFI)
	 * o el de F0000h-FFFFFh (mq_smbios, maquina.c) */
	u32 pe = mq_smbios(), dir = 0, largo = 0;
	const u8 *e = pe ? mq_mapa(pe, 32) : 0, *f, *fin;

	ta.smbios = 2;		/* (mirado; 1 si se ha encontrado) */
	if (!e)
		return;
	if (!memcmp((void *)e, "_SM3_", 5)) {
		if (*(const u32 *)(e + 0x14) == 0) {
			dir = *(const u32 *)(e + 0x10);
			largo = *(const u32 *)(e + 0x0C);
		} else
			reg("ACPI: (TERMO) la tabla del SMBIOS 3 está por encima de los 4 GB (%X%08Xh): no se alcanza\n",
			    *(const u32 *)(e + 0x14), *(const u32 *)(e + 0x10));
	} else if (!memcmp((void *)e, "_SM_", 4)) {
		largo = *(const u16 *)(e + 0x16);
		dir = *(const u32 *)(e + 0x18);
	} else if (!memcmp((void *)e, "_DMI_", 5)) {
		largo = *(const u16 *)(e + 6);
		dir = *(const u32 *)(e + 8);
	} else
		e = 0;
	if (!e || !dir || !largo || largo > 0x10000) {
		reg("ACPI: (TERMO) no se encuentra el SMBIOS\n");
		return;
	}
	f = mq_mapa(dir, largo);
	if (!f)
		return;
	fin = f + largo;
	while (f + 4 <= fin && f[0] != 127 && f[1] >= 4) {
		const u8 *s = f + f[1];

		if (f[0] == 0 && f[1] >= 9) {
			ta_texto_smbios(ta.bios, sizeof ta.bios, f, f[5]);
			{
				char fe[16];
				u32 n = strlen(ta.bios);

				ta_texto_smbios(fe, sizeof fe, f, f[8]);
				if (fe[0] && n + strlen(fe) + 4 < sizeof ta.bios)
					snprintf(ta.bios + n, sizeof ta.bios - n, " (%s)", fe);
			}
		} else if (f[0] == 1 && f[1] >= 8) {
			ta_texto_smbios(ta.fabricante, sizeof ta.fabricante, f, f[4]);
			ta_texto_smbios(ta.producto, sizeof ta.producto, f, f[5]);
			ta_texto_smbios(ta.version_eq, sizeof ta.version_eq, f, f[6]);
		} else if (f[0] == 2 && f[1] >= 6) {
			ta_texto_smbios(ta.placa_fab, sizeof ta.placa_fab, f, f[4]);
			ta_texto_smbios(ta.placa, sizeof ta.placa, f, f[5]);
		}
		/* los textos de detrás acaban con dos ceros */
		while (s + 1 < fin && (s[0] || s[1]))
			s++;
		f = s + 2;
	}
	ta.smbios = 1;
	reg("ACPI: (TERMO) SMBIOS: «%s» «%s», placa «%s» «%s», BIOS «%s»\n", ta.fabricante, ta.producto,
	    ta.placa_fab, ta.placa, ta.bios);
}

/* (TERMO) lo que gasta (o carga) la batería ahora, en mW: de lo último que
 * ha leído bateria.c de _BST (el ritmo; con mA, por el voltaje: el de ahora
 * o, si no lo dice, el de diseño) */
static void ta_lee_bateria(void)
{
	u32 i, mw = 0, est = 0;
	int hay = 0;

	for (i = 0; i < LL_NBAT; i++) {
		const struct ll_bat *b = &estado.bat[i];
		u32 vol = b->voltaje && b->voltaje != 0xFFFFFFFFu ? b->voltaje : b->voltaje_diseno;

		if (!b->hay || !b->valida || !(b->estado & 3) || b->queda == 0xFFFFFFFFu)
			continue;
		est |= b->estado & 3;
		if (b->ritmo == 0 || b->ritmo == 0xFFFFFFFFu || b->ritmo > 0x7FFFFFFF)	/* (0: no lo dice) */
			continue;
		if (b->unidad == 0)
			mw += b->ritmo;
		else if (vol && vol != 0xFFFFFFFFu && vol < 100000)
			mw += (u32)((u64)b->ritmo * vol / 1000);
		else
			continue;
		hay = 1;
	}
	ta.bat_estado = est;
	ta.bat_mw = hay && mw < 1000000 ? (int32_t)mw : TA_NO;
	ta.bat_mv = 0;
	for (i = 0; i < LL_NBAT && !ta.bat_mv; i++)
		if (estado.bat[i].hay && estado.bat[i].valida && estado.bat[i].voltaje && estado.bat[i].voltaje < 100000)
			ta.bat_mv = estado.bat[i].voltaje;
}

/* (TERMO) a \TERMO\EQUIPO: las tablas ACPI tal cual y la lista PCI. Solo
 * leer el equipo (la configuración PCI, sin tocar nada: sin medir las zonas,
 * que pararía los aparatos un momento); las tablas no llevan datos
 * personales. Una vez, la primera vez que el Termómetro pregunta */
/* (0.8.12) la carpeta: \TERMO\EQUIPO (el Termómetro) o, con «fichaequipo» en
 * la línea del cargador, también \EQUIPO (la memoria del Acer One 10: la
 * pantalla táctil y el acelerómetro) */
static const char *ta_carpeta = "\\TERMO\\EQUIPO";

static int ta_archivo(const char *nombre, const void *d, u32 n)
{
	char r[48];
	u32 m, w;

	snprintf(r, sizeof r, "%s\\%s", ta_carpeta, nombre);
	m = sc(SC_ABRIR, (u32)r, 2, 0);
	if (ES_ERROR(m))
		return 0;
	w = sc(SC_ESCRIBIR_ARCHIVO, m, (u32)d, n);
	sc(SC_CERRAR, m, 0, 0);
	return w == n;
}

static u32 ta_pci(u32 bus, u32 dis, u32 fun, u32 reg)
{
	return sc(SC_PCI, 0x80000000u | (bus << 16) | (dis << 11) | (fun << 8) | reg, 0, 4);
}

static void ta_equipo(void)
{
	static char t[24576];
	ACPI_TABLE_HEADER *h;
	u32 i, n = 0, nssdt = 0, bus, dis, fun, k;
	char nombre[16];

	ta.equipo_hecho = 1;
	if (ta_carpeta[1] == 'T')
		sc(SC_CREAR_CARPETA, (u32)"\\TERMO", 0, 0);
	sc(SC_CREAR_CARPETA, (u32)ta_carpeta, 0, 0);
	for (i = 0; i < 64; i++) {
		if (ACPI_FAILURE(AcpiGetTableByIndex(i, &h)))
			break;
		if (!h || h->Length < sizeof *h || h->Length > 0x200000)
			continue;
		/* MSDM y SLIC llevan la licencia de Windows del equipo (la clave del
		   producto): no se copian nunca (el equipo puede ser de otro) */
		if (!memcmp(h->Signature, "MSDM", 4) || !memcmp(h->Signature, "SLIC", 4)) {
			reg("ACPI: (TERMO) la tabla %c%c%c%c no se guarda: lleva la licencia de Windows\n", h->Signature[0], h->Signature[1], h->Signature[2], h->Signature[3]);
			continue;
		}
		if (!memcmp(h->Signature, "SSDT", 4))
			snprintf(nombre, sizeof nombre, "SSDT%u.BIN", ++nssdt);
		else {
			for (k = 0; k < 4; k++)
				nombre[k] = (h->Signature[k] >= 'A' && h->Signature[k] <= 'Z') || (h->Signature[k] >= '0' && h->Signature[k] <= '9') ? h->Signature[k] : '_';
			snprintf(nombre + 4, sizeof nombre - 4, ".BIN");
		}
		if (ta_archivo(nombre, h, h->Length))
			n++;
		else if (n == 0) {
			ta.equipo_hecho = 2;
			reg("ACPI: (TERMO) no se puede escribir en %s\n", ta_carpeta);
			return;
		}
	}
	ta.equipo_tablas = n;
	/* la lista PCI: lo que dice cada aparato de sí mismo */
	k = (u32)snprintf(t, sizeof t, "Lista PCI del equipo (Termómetro de Hispa-Pentium): bus:aparato.función, fabricante:producto, clase, y sus zonas\r\n"
			  "(tal como están ahora: si el núcleo ha movido alguna por encima de los 4 GB, lo dice la línea «pci:» de INFORME.TXT)\r\n\r\n");
	for (bus = 0; bus < 256 && k < sizeof t - 400; bus++)
		for (dis = 0; dis < 32 && k < sizeof t - 400; dis++)
			for (fun = 0; fun < 8 && k < sizeof t - 400; fun++) {
				u32 id = ta_pci(bus, dis, fun, 0), cl, cab, b, nb, j;

				if ((id & 0xFFFF) == 0xFFFF || (id & 0xFFFF) == 0) {
					if (fun == 0)
						break;
					continue;
				}
				cl = ta_pci(bus, dis, fun, 8);
				cab = (ta_pci(bus, dis, fun, 0x0C) >> 16) & 0xFF;
				k += (u32)snprintf(t + k, sizeof t - k, "%02X:%02X.%u %04X:%04X clase %06X rev %02X cab %02X subsistema %08X\r\n", bus, dis, fun,
						   id & 0xFFFF, id >> 16, cl >> 8, cl & 0xFF, cab, (cab & 0x7F) == 0 ? ta_pci(bus, dis, fun, 0x2C) : 0);
				nb = (cab & 0x7F) == 0 ? 6 : (cab & 0x7F) == 1 ? 2 : 0;
				for (j = 0; j < nb; j++) {
					b = ta_pci(bus, dis, fun, 0x10 + j * 4);
					if (!b)
						continue;
					if (b & 1)
						k += (u32)snprintf(t + k, sizeof t - k, "    zona %u: puertos %04Xh\r\n", j, b & 0xFFFC);
					else if ((b & 6) == 4 && j + 1 < nb) {
						u32 hi = ta_pci(bus, dis, fun, 0x10 + (j + 1) * 4);

						k += (u32)snprintf(t + k, sizeof t - k, "    zona %u: memoria de 64 bits %08X%08Xh%s%s\r\n", j, hi, b & 0xFFFFFFF0u,
								   b & 8 ? ", prefetchable" : "", hi ? " (POR ENCIMA DE LOS 4 GB)" : "");
						j++;
					} else
						k += (u32)snprintf(t + k, sizeof t - k, "    zona %u: memoria %08Xh%s\r\n", j, b & 0xFFFFFFF0u, b & 8 ? ", prefetchable" : "");
				}
				if ((cab & 0x7F) == 1) {
					u32 bb = ta_pci(bus, dis, fun, 0x18), mv = ta_pci(bus, dis, fun, 0x20);

					k += (u32)snprintf(t + k, sizeof t - k, "    puente: buses %u a %u; ventana de memoria %08Xh-%08Xh\r\n", (bb >> 8) & 0xFF, (bb >> 16) & 0xFF,
							   (mv & 0xFFF0) << 16, (mv & 0xFFF00000u) | 0xFFFFF);
				}
				if (fun == 0 && !(cab & 0x80))
					break;
			}
	if (ta_archivo("PCI.TXT", t, k))
		ta.equipo_pci = 1;
	reg("ACPI: (TERMO) en %s: %u tablas ACPI y la lista PCI%s\n", ta_carpeta, ta.equipo_tablas, ta.equipo_pci ? "" : " (NO)");
}

/* (0.8.12) «fichaequipo»: la ficha técnica en \EQUIPO (las tablas ACPI sin MSDM
 * ni SLIC y PCI.TXT, como el Termómetro), sin tocar lo del Termómetro */
static void ficha_equipo(void)
{
	u32 h = ta.equipo_hecho, n = ta.equipo_tablas, p = ta.equipo_pci;

	ta_carpeta = "\\EQUIPO";
	ta_equipo();
	ta_carpeta = "\\TERMO\\EQUIPO";
	ta.equipo_hecho = h;
	ta.equipo_tablas = n;
	ta.equipo_pci = p;
}

/* (TERMO) (NVIDIA) los puertos PCI Express del bus 0 en el ACPI (los de la
 * gráfica: PEG0, RP01...) y su hijo de _ADR 0: qué objetos de energía tienen
 * (sin llamarlos) y, para leer, el _PSC del hijo y el _STA del primer
 * recurso de _PR0. Nunca _PS0, _ON, _OFF ni _DSM: encender o apagar la
 * gráfica (Optimus) es cosa de su controlador */
static uint8_t ta_peg_objetos(ACPI_HANDLE h)
{
	static const struct { const char *n; uint8_t b; } o[] = {
		{ "_PR0", TA_PEG_PR0 }, { "_PR3", TA_PEG_PR3 }, { "_PS0", TA_PEG_PS0 }, { "_PS3", TA_PEG_PS3 },
		{ "_DSM", TA_PEG_DSM }, { "_ON_", TA_PEG_ON }, { "_OFF", TA_PEG_OFF },
	};
	ACPI_HANDLE x;
	uint8_t r = 0;
	unsigned i;

	for (i = 0; i < sizeof o / sizeof o[0]; i++)
		if (ACPI_SUCCESS(AcpiGetHandle(h, (char *)o[i].n, &x)))
			r |= o[i].b;
	return r;
}

static int ta_adr(ACPI_HANDLE h, UINT64 *adr)
{
	ACPI_BUFFER e;
	ACPI_OBJECT x;

	e.Length = sizeof x;
	e.Pointer = &x;
	if (ACPI_FAILURE(AcpiEvaluateObjectTyped(h, "_ADR", NULL, &e, ACPI_TYPE_INTEGER)))
		return 0;
	*adr = x.Integer.Value;
	return 1;
}

/* el _STA del primer recurso de energía de _PR0 de h: 1 encendido, 2 apagado, 0 no hay */
static uint8_t ta_pr0_sta(ACPI_HANDLE h)
{
	ACPI_BUFFER b, e;
	ACPI_OBJECT *o, x;
	uint8_t r = 0;

	b.Length = ACPI_ALLOCATE_BUFFER;
	b.Pointer = NULL;
	if (ACPI_FAILURE(AcpiEvaluateObjectTyped(h, "_PR0", NULL, &b, ACPI_TYPE_PACKAGE)))
		return 0;
	o = b.Pointer;
	if (o->Package.Count >= 1 && o->Package.Elements[0].Type == ACPI_TYPE_LOCAL_REFERENCE && o->Package.Elements[0].Reference.Handle) {
		e.Length = sizeof x;
		e.Pointer = &x;
		if (ACPI_SUCCESS(AcpiEvaluateObjectTyped(o->Package.Elements[0].Reference.Handle, "_STA", NULL, &e, ACPI_TYPE_INTEGER)))
			r = (x.Integer.Value & 1) ? 1 : 2;
	}
	ACPI_FREE(b.Pointer);
	return r;
}

static ACPI_HANDLE ult_padre;
static int ult_raiz;

static ACPI_STATUS ta_un_peg(ACPI_HANDLE h, UINT32 nivel, void *ctx, void **ret)
{
	ACPI_HANDLE padre, hijo = NULL, gpu = NULL;
	ACPI_DEVICE_INFO *info;
	ACPI_BUFFER b, e;
	ACPI_OBJECT x;
	UINT64 adr, a2;
	struct ta_peg *p;
	char ruta[48];
	int raiz;
	u32 dis, fun;

	(void)nivel; (void)ctx; (void)ret;
	if (ta.npeg >= sizeof ta.peg / sizeof ta.peg[0])
		return AE_CTRL_TERMINATE;
	if (!ta_adr(h, &adr) || adr == 0 || (adr >> 16) > 31 || (adr & 0xFFFF) > 7)
		return AE_OK;
	if (ACPI_FAILURE(AcpiGetParent(h, &padre)))
		return AE_OK;
	if (padre != ult_padre) {		/* (el mismo padre, PCI0, para casi todos: una vez) */
		if (ACPI_FAILURE(AcpiGetObjectInfo(padre, &info)))
			return AE_OK;
		ult_padre = padre;
		ult_raiz = (info->Flags & ACPI_PCI_ROOT_BRIDGE) != 0;
		ACPI_FREE(info);
	}
	raiz = ult_raiz;
	dis = (u32)(adr >> 16);
	fun = (u32)(adr & 7);
	/* (solo los puentes PCI-PCI del bus 0 que están) */
	if (!raiz || (ta_pci(0, dis, fun, 0) & 0xFFFF) == 0xFFFF || ((ta_pci(0, dis, fun, 0x0C) >> 16) & 0x7F) != 1)
		return AE_OK;
	p = &ta.peg[ta.npeg++];
	p->ap_fun = (uint8_t)(dis << 3 | fun);
	p->puerto = ta_peg_objetos(h);
	while (ACPI_SUCCESS(AcpiGetNextObject(ACPI_TYPE_DEVICE, h, hijo, &hijo)))
		if (ta_adr(hijo, &a2) && a2 == 0) {
			gpu = hijo;
			break;
		}
	p->estado = 0x0F;
	if (gpu) {
		p->hijo = (uint8_t)(ta_peg_objetos(gpu) | TA_PEG_HAY);
		if (p->hijo & TA_PEG_PS0) {
			e.Length = sizeof x;
			e.Pointer = &x;
			if (ACPI_SUCCESS(AcpiEvaluateObjectTyped(gpu, "_PSC", NULL, &e, ACPI_TYPE_INTEGER)) && x.Integer.Value <= 3)
				p->estado = (uint8_t)x.Integer.Value;
		}
		if (p->hijo & TA_PEG_PR0)
			p->estado |= (uint8_t)(ta_pr0_sta(gpu) << 4);
	}
	if (!(p->estado & 0x30) && (p->puerto & TA_PEG_PR0))
		p->estado |= (uint8_t)(ta_pr0_sta(h) << 4);
	b.Length = sizeof ruta;
	b.Pointer = ruta;
	if (ACPI_FAILURE(AcpiGetName(h, ACPI_FULL_PATHNAME, &b)))
		pon_texto(ruta, "?", sizeof ruta);
	reg("ACPI: (TERMO) puerto PCI Express 00:%02X.%u = %s: objetos %02Xh, hijo %02Xh, _PSC %u, recurso _PR0 %u\n", dis, fun, ruta, p->puerto, p->hijo,
	    p->estado & 0xF, p->estado >> 4);
	return AE_OK;
}

static void ta_lee(void)
{
	u32 i;

	if (!ta_buscado) {
		ta_buscado = 1;
		ta.version = TA_VERSION;
		ta_smbios();
		ta_equipo();			/* (TERMO) las tablas ACPI y la lista PCI, a \TERMO\EQUIPO */
		if (apm.fase != FASE_ERROR || mq_rsdp()) {
			AcpiWalkNamespace(ACPI_TYPE_THERMAL, ACPI_ROOT_OBJECT, ACPI_UINT32_MAX, ta_una_zona, NULL, NULL, NULL);
			AcpiWalkNamespace(ACPI_TYPE_DEVICE, ACPI_ROOT_OBJECT, ACPI_UINT32_MAX, ta_un_ventilador, NULL, NULL, NULL);
			ec_solo_lectura = 1;	/* (TERMO) (NVIDIA) los puertos de las gráficas (solo leer) */
			AcpiWalkNamespace(ACPI_TYPE_DEVICE, ACPI_ROOT_OBJECT, ACPI_UINT32_MAX, ta_un_peg, NULL, NULL, NULL);
			ec_solo_lectura = 0;
		}
		reg("ACPI: (TERMO) %u zonas térmicas y %u ventiladores\n", ta.nzonas, ta.nvent);
		ta.asus_hay = ACPI_SUCCESS(AcpiGetHandle(ACPI_ROOT_OBJECT, "\\_SB.ATKD", &ta_atkd)) && ACPI_SUCCESS(AcpiGetHandle(ta_atkd, "WMNB", &(ACPI_HANDLE){ 0 }));
		if (ta.asus_hay)
			reg("ACPI: (TERMO) ASUS: \\_SB.ATKD.WMNB (se leen con DSTS los ventiladores y la temperatura)\n");
	}
	/* (TERMO) el EC, de solo lectura mientras tanto; y cuánto se tarda y por
	 * qué falla cada zona, apuntado (la primera vez y cuando cambia) */
	{
		u64 t0 = mq_ahora_us();
		u32 l0 = estado.ec_lecturas, f0 = estado.ec_fallos;
		static uint32_t antes[TA_ZONAS];

		ec_solo_lectura = 1;
		for (i = 0; i < ta.nzonas; i++) {
			struct ta_zona *z = &ta.zona[i];

			/* (TERMO) la que falla, otra vez solo una vez por minuto: cada
			 * intento cuesta interpretar el AML (en el HP Mini 5103, cinco
			 * zonas con 3007h, AE_AML_UNINITIALIZED_ELEMENT, ocupaban el
			 * procesador más de medio segundo cada vez) */
			if (ta.lecturas > 0 && z->tmp == TA_NO && z->estado != 0 && ta.lecturas % 30 != 0)
				continue;
			z->tmp = ta_kelvin_de(ta_zh[i], "_TMP", &z->estado, &z->crudo);
			if (z->tmp == TA_NO && (z->estado != antes[i] || ta.lecturas == 0))
				reg("ACPI: (TERMO) %s._TMP sin dato: %s, devuelve %d\n", z->nombre, AcpiFormatException(z->estado), z->crudo);
			antes[i] = z->tmp == TA_NO ? z->estado : 0xFFFFFFFFu;
		}
		for (i = 0; i < ta.nvent; i++)
			if (ta.lecturas == 0 || ta.lecturas % 30 == 0 || (ta.vent[i].estado_fst == 0 || ta.vent[i].estado_psc == 0))
				ta_lee_vent(i);		/* (los que no dan nada: una vez por minuto) */
		ta_lee_asus();			/* (TERMO) los ASUS, por WMI (DSTS) */
		ta_lee_bateria();		/* (TERMO) lo que gasta la batería (de lo último de _BST) */
		ec_solo_lectura = 0;
		ta.ec_hay = estado.ec_hay;
		ta.ec_lecturas += estado.ec_lecturas - l0;
		ta.ec_fallos += estado.ec_fallos - f0;
		ta.ec_ignoradas = ec_ignoradas;
		ta.ms_lectura = (uint32_t)((mq_ahora_us() - t0) / 1000);
	}
	ta.lecturas++;
}

/* en cada vuelta del bucle: se da lo último (y el núcleo dice si alguien
 * lo quiere); si lo quieren, cada 2 s, se lee otra vez */
static void ta_vuelta(void)
{
	static u64 ultima;
	u64 ahora;

	ta.version = TA_VERSION;
	if (sc(SC_TERMO, (u32)&ta, 0, 1) != 1)
		return;
	ahora = mq_ahora_us();
	/* cada 2 s; si leer cuesta mucho (más de 0,2 s), cada 10 s */
	if (ultima && ahora - ultima < (ta.ms_lectura > 200 ? 9900000u : 1900000u))
		return;
	if (!tarea_quieta())
		return;
	ultima = ahora;
	ta_lee();
	sc(SC_TERMO, (u32)&ta, 0, 1);
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
	ec_espera_activa_us = 50;	/* (TERMO) esperar al EC durmiendo, no dando vueltas */
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
	{
		extern int mq_traza;

		mq_traza = al_archivo && !tiene(args, "acpisintraza");	/* (TERMO) \ACPIESC.TXT */
	}
	detalle = tiene(args, "acpidetalle") != 0;
	/* (HPRADIO) RADIOHP.TXT del arranque de antes, fuera */
	if (!tiene(args, "sinhpradio"))
		hp_radio_borra();
	if ((p = tiene(args, "acpicada=")) != 0)
		for (cada = 0; *p >= '0' && *p <= '9'; p++)
			cada = cada * 10 + *p - '0';
	if (cada < 1 || cada > 600)
		cada = 10;
	/* (UEFI) la RSDP, si la dice el núcleo (en hexadecimal) */
	if ((p = tiene(args, "rsdp=")) != 0) {
		u32 d = 0;

		for (; (*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'F'); p++)
			d = d * 16 + (u32)(*p <= '9' ? *p - '0' : *p - 'A' + 10);
		mq_pon_rsdp(d);
	}
	/* (UEFI) y el SMBIOS (mq_smbios) */
	if ((p = tiene(args, "smbios=")) != 0) {
		u32 d = 0;

		for (; (*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'F'); p++)
			d = d * 16 + (u32)(*p <= '9' ? *p - '0' : *p - 'A' + 10);
		mq_pon_smbios(d);
	}
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
			if (apm.fase != FASE_ERROR || mq_rsdp()) {
				prepara_energia();
				/* (HPRADIO) los HP: el Wi-Fi, encendido por la BIOS (WMI 1Bh) */
				if (!tiene(args, "sinhpradio"))
					hp_radio(tiene(args, "hpradiosolover") != 0);
			}
			reg("ACPI: SMBIOS en %08Xh\n", mq_smbios());	/* (UEFI) */
			reg("ACPI: el núcleo ha puesto a la vista %u KB de memoria física\n", mq_puestos >> 10);
			/* (I2CHID) los touchpads HID sobre I2C, para el núcleo (llamada 159) */
			if (!tiene(args, "sini2c")) {
				static struct pe_i2chid tp[I2CHID_MAX];
				int k, m = i2chid_busca(tp, I2CHID_MAX);

				for (k = 0; k < m; k++)
					sc(SC_I2CHID, (u32)&tp[k], 0, 0);
				sc(SC_I2CHID, 0, 0, 0);
			}
			/* (0.8.12) la ficha técnica en \EQUIPO y, en ACPI.TXT, los aparatos de
			 * las I2C (la pantalla táctil, el acelerómetro), solo leer */
			if (tiene(args, "fichaequipo")) {
				ficha_equipo();
				i2c_lista();
			}
			/* (SDHCI) las controladoras SD/eMMC de «modo ACPI», para el núcleo
			 * (llamada 160): el eMMC y la microSD de las tabletas Bay Trail */
			if (!tiene(args, "sinsdhci")) {
				static struct pe_sdhci sd[SDHCI_MAX];
				sdhci_sin_sdio = tiene(args, "sinsdio") != 0;	/* (RTL8723BS) */
				int k, m = sdhci_busca(sd, SDHCI_MAX);

				for (k = 0; k < m; k++)
					sc(SC_SDHCI, (u32)&sd[k], 0, 0);
				sc(SC_SDHCI, 0, 0, 0);
			}
		}
		/* (dispositivos) una vez, con la tarea quieta y ya leída la batería */
		if (preparado == 1 && apm.fase != FASE_EMPEZANDO && apm.fase != FASE_ERROR && tarea_quieta()) {
			preparado = 2;
			if (!tiene(args, "acpisindisp"))
				apunta_dispositivos();
		}
		if (apm.lecturas != vistas || apm.fase != fase) {
			vistas = apm.lecturas;
			fase = apm.fase;
			cocina();
		}
		if (preparado)
			mira_boton();	/* (PANEL) */
		ta_vuelta();			/* (TERMO) */
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
