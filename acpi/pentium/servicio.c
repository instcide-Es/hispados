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
	const u8 *b = mq_mapa(0xF0000, 0x10000), *e = 0, *f, *fin;
	u32 i, dir = 0, largo = 0;

	ta.smbios = 2;		/* (mirado; 1 si se ha encontrado) */
	if (!b)
		return;
	for (i = 0; i + 32 <= 0x10000; i += 16)
		if (!memcmp((void *)(b + i), "_SM3_", 5)) {
			e = b + i;
			if (*(const u32 *)(e + 0x14) == 0) {
				dir = *(const u32 *)(e + 0x10);
				largo = *(const u32 *)(e + 0x0C);
			}
			break;
		} else if (!memcmp((void *)(b + i), "_SM_", 4)) {
			e = b + i;
			largo = *(const u16 *)(e + 0x16);
			dir = *(const u32 *)(e + 0x18);
			break;
		} else if (!memcmp((void *)(b + i), "_DMI_", 5)) {
			e = b + i;
			largo = *(const u16 *)(e + 6);
			dir = *(const u32 *)(e + 8);
			break;
		}
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

static void ta_lee(void)
{
	u32 i;

	if (!ta_buscado) {
		ta_buscado = 1;
		ta.version = TA_VERSION;
		ta_smbios();
		if (apm.fase != FASE_ERROR || mq_rsdp()) {
			AcpiWalkNamespace(ACPI_TYPE_THERMAL, ACPI_ROOT_OBJECT, ACPI_UINT32_MAX, ta_una_zona, NULL, NULL, NULL);
			AcpiWalkNamespace(ACPI_TYPE_DEVICE, ACPI_ROOT_OBJECT, ACPI_UINT32_MAX, ta_un_ventilador, NULL, NULL, NULL);
		}
		reg("ACPI: (TERMO) %u zonas térmicas y %u ventiladores\n", ta.nzonas, ta.nvent);
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

			z->tmp = ta_kelvin_de(ta_zh[i], "_TMP", &z->estado, &z->crudo);
			if (z->tmp == TA_NO && (z->estado != antes[i] || ta.lecturas == 0))
				reg("ACPI: (TERMO) %s._TMP sin dato: %s, devuelve %d\n", z->nombre, AcpiFormatException(z->estado), z->crudo);
			antes[i] = z->tmp == TA_NO ? z->estado : 0xFFFFFFFFu;
		}
		for (i = 0; i < ta.nvent; i++)
			ta_lee_vent(i);
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
	if (ultima && ahora - ultima < 1900000)
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
