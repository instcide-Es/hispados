/* servicio.c - ACPICA64.BIN (Hispa-64): las llamadas del núcleo (hispa64.h).
 *
 * Lo de la batería, el cable y el controlador embebido es lo de Hispa-DOS y
 * Hispa-Pentium (acpi/nucleo/bateria.c y ec.c), sin su «tarea»: aquí lo
 * llama el hilo «acpi» del núcleo (castel/nucleo64/acpica64.cas) cada vez,
 * y dormir es soltar el procesador. Lo de aquí, que allí no hacía falta:
 *  - el modo APIC: \_PIC(1) antes de nada (para que los _PRT den las rutas
 *    del IOAPIC);
 *  - las rutas de las INTx de un puente (_PRT) resueltas por ACPICA,
 *    también las de un enlace (PNP0C0F) con su _CRS hecho por un método (los
 *    LNKA de siempre), que Aml64 no sabe resolver sin interpretar el AML;
 *  - apagar de verdad: AcpiEnterSleepStatePrep(5) (\_S5, _PTS(5), _SST) y
 *    AcpiEnterSleepState(5) (SLP_TYP y SLP_EN en PM1a y PM1b);
 *  - el botón de encendido de los que avisan con Notify (PNP0C0C, el de
 *    los portátiles, por el controlador embebido); el «fijo» de la FADT lo
 *    sigue atendiendo el núcleo (su SCI);
 *  - el brillo de la pantalla (_BCL, _BCM, _BQC) del primer dispositivo que
 *    lo tenga.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "acpi.h"
#include "accommon.h"
#include "hispa64.h"

int bateria_arranca(void);
int bateria_lectura(void);
int bateria_cambio(void);
extern u32 osl_errores;

static int arrancado;		/* 0 aún no; 1 ACPICA en marcha; -1 no va */
static u32 botones, avisos_n;
static ACPI_HANDLE brillo_h;
static u32 brillo_niveles[101], brillo_n;
static int s5_hay;
static u8 s5_a, s5_b;
static char texto[512];

/* ---- el registro, al puerto serie del núcleo ---- */
static void vacia_registro(void)
{
	u32 n;

	while ((n = reg_saca(texto, sizeof texto)) != 0)
		castel2(h64.t.escribe, (u64)texto, n);
}

/* [bateria.c] los demás avisos del equipo */
void nucleo_aviso(ACPI_HANDLE h, UINT32 valor)
{
	ACPI_DEVICE_INFO *di;

	avisos_n++;
	if (valor != 0x80 && valor != 0x02)
		return;
	if (ACPI_FAILURE(AcpiGetObjectInfo(h, &di)))
		return;
	if ((di->Valid & ACPI_VALID_HID) && !strcmp(di->HardwareId.String, "PNP0C0C")) {
		botones++;
		reg("ACPI: el botón de encendido (Notify %02Xh)\n", valor);
	}
	AcpiOsFree(di);
}

/* ---- el brillo ---- */
static ACPI_STATUS con_bcm(ACPI_HANDLE h, UINT32 nivel, void *ctx, void **ret)
{
	ACPI_HANDLE m;

	if (brillo_h || ACPI_FAILURE(AcpiGetHandle(h, "_BCM", &m)))
		return AE_OK;
	brillo_h = h;
	return AE_CTRL_TERMINATE;
}

static void prepara_brillo(void)
{
	ACPI_BUFFER b = { ACPI_ALLOCATE_BUFFER, NULL };
	ACPI_OBJECT *p;
	u32 i, v;

	AcpiWalkNamespace(ACPI_TYPE_DEVICE, ACPI_ROOT_OBJECT, ACPI_UINT32_MAX, con_bcm, NULL, NULL, NULL);
	if (!brillo_h)
		return;
	if (ACPI_SUCCESS(AcpiEvaluateObjectTyped(brillo_h, "_BCL", NULL, &b, ACPI_TYPE_PACKAGE))) {
		p = b.Pointer;
		/* los dos primeros son los de «con cable» y «con batería»; luego, los niveles */
		for (i = 2; i < p->Package.Count && brillo_n < 101; i++)
			if (p->Package.Elements[i].Type == ACPI_TYPE_INTEGER) {
				v = (u32)p->Package.Elements[i].Integer.Value;
				if (v <= 100)
					brillo_niveles[brillo_n++] = v;
			}
		AcpiOsFree(b.Pointer);
	}
	{
		char r[48];
		ACPI_BUFFER n = { sizeof r, r };

		r[0] = 0;
		AcpiGetName(brillo_h, ACPI_FULL_PATHNAME, &n);
		reg("ACPI: el brillo, en %s (_BCM; %u niveles en _BCL)\n", r, brillo_n);
	}
}

static int brillo_lee(void)
{
	ACPI_BUFFER b = { ACPI_ALLOCATE_BUFFER, NULL };
	int v = -1;

	if (!brillo_h)
		return -1;
	if (ACPI_SUCCESS(AcpiEvaluateObjectTyped(brillo_h, "_BQC", NULL, &b, ACPI_TYPE_INTEGER))) {
		v = (int)((ACPI_OBJECT *)b.Pointer)->Integer.Value;
		AcpiOsFree(b.Pointer);
	}
	return v;
}

/* el nivel de _BCL más cercano a lo pedido */
static int brillo_pon(u32 pct)
{
	ACPI_OBJECT a;
	ACPI_OBJECT_LIST l = { 1, &a };
	u32 i, mejor = pct, d = 1000;

	if (!brillo_h)
		return -1;
	for (i = 0; i < brillo_n; i++) {
		u32 e = brillo_niveles[i] > pct ? brillo_niveles[i] - pct : pct - brillo_niveles[i];

		if (e < d) {
			d = e;
			mejor = brillo_niveles[i];
		}
	}
	a.Type = ACPI_TYPE_INTEGER;
	a.Integer.Value = mejor;
	if (ACPI_FAILURE(AcpiEvaluateObject(brillo_h, "_BCM", &l, NULL)))
		return -1;
	return brillo_lee();
}

/* ---- el arranque ---- */
static int arranca(void)
{
	ACPI_OBJECT a;
	ACPI_OBJECT_LIST l = { 1, &a };
	ACPI_STATUS s;
	int r;

	r = bateria_arranca();
	if (r < 0) {
		arrancado = -1;
		return -1;
	}
	arrancado = 1;
	/* el modo APIC (si no está \_PIC, no pasa nada) */
	a.Type = ACPI_TYPE_INTEGER;
	a.Integer.Value = 1;
	s = AcpiEvaluateObject(NULL, "\\_PIC", &l, NULL);
	reg("ACPI: \\_PIC(1), el modo APIC: %s\n", ACPI_SUCCESS(s) ? "puesto" : s == AE_NOT_FOUND ? "no hay \\_PIC" : AcpiFormatException(s));
	if (ACPI_SUCCESS(AcpiGetSleepTypeData(ACPI_STATE_S5, &s5_a, &s5_b)))
		s5_hay = 1;
	reg("ACPI: para apagar, \\_S5 %s(%u, %u)\n", s5_hay ? "" : "no está ", s5_a, s5_b);
	prepara_brillo();
	return r;
}

/* ---- las rutas de un _PRT ---- */
static int marcas_de(UINT8 disparo, UINT8 polaridad)
{
	return (polaridad == ACPI_ACTIVE_LOW ? 3 : 1) | (disparo == ACPI_LEVEL_SENSITIVE ? 12 : 4);
}

/* la IRQ que tiene puesta un enlace (su _CRS) */
static int irq_de_enlace(ACPI_HANDLE e, u32 indice, u32 *gsi, u32 *marcas)
{
	ACPI_BUFFER b = { ACPI_ALLOCATE_BUFFER, NULL };
	ACPI_RESOURCE *r;
	int hay = 0;

	if (ACPI_FAILURE(AcpiGetCurrentResources(e, &b)))
		return 0;
	for (r = b.Pointer; r->Type != ACPI_RESOURCE_TYPE_END_TAG && r->Length; r = ACPI_NEXT_RESOURCE(r)) {
		if (r->Type == ACPI_RESOURCE_TYPE_IRQ && r->Data.Irq.InterruptCount > indice && r->Data.Irq.Interrupts[indice]) {
			*gsi = r->Data.Irq.Interrupts[indice];
			*marcas = marcas_de(r->Data.Irq.Triggering, r->Data.Irq.Polarity);
			hay = 1;
			break;
		}
		if (r->Type == ACPI_RESOURCE_TYPE_EXTENDED_IRQ && r->Data.ExtendedIrq.InterruptCount > indice &&
		    r->Data.ExtendedIrq.Interrupts[indice]) {
			*gsi = r->Data.ExtendedIrq.Interrupts[indice];
			*marcas = marcas_de(r->Data.ExtendedIrq.Triggering, r->Data.ExtendedIrq.Polarity);
			hay = 1;
			break;
		}
	}
	AcpiOsFree(b.Pointer);
	return hay;
}

static int rutas(const char *ruta, struct h64_rutas *o)
{
	ACPI_BUFFER b = { ACPI_ALLOCATE_BUFFER, NULL };
	ACPI_PCI_ROUTING_TABLE *t;
	ACPI_HANDLE h, e;
	u8 *p;

	o->n = o->sin_resolver = 0;
	if (ACPI_FAILURE(AcpiGetHandle(NULL, (char *)ruta, &h)))
		return -1;
	if (ACPI_FAILURE(AcpiGetIrqRoutingTable(h, &b)))
		return -1;
	for (p = b.Pointer; ; p += t->Length) {
		struct h64_ruta *x;

		t = (ACPI_PCI_ROUTING_TABLE *)p;
		if (!t->Length)
			break;
		if (o->n >= H64_MAX_RUTAS)
			break;
		x = &o->r[o->n];
		x->ranura = (u32)(t->Address >> 16) & 0xFFFF;
		x->patilla = t->Pin;
		if (!t->Source[0]) {
			x->gsi = t->SourceIndex;
			x->marcas = 3 | 12;	/* lo de PCI: por nivel, activa en bajo */
			x->enlace = 0;
			o->n++;
		} else if (ACPI_SUCCESS(AcpiGetHandle(h, t->Source, &e)) && irq_de_enlace(e, t->SourceIndex, &x->gsi, &x->marcas)) {
			x->enlace = 1;
			o->n++;
		} else
			o->sin_resolver++;
	}
	AcpiOsFree(b.Pointer);
	return (int)o->n;
}

/* ---- apagar ---- */
static int apaga(void)
{
	ACPI_STATUS s;

	if (arrancado != 1)
		return -1;
	s = AcpiEnterSleepStatePrep(ACPI_STATE_S5);
	reg("ACPI: apagar: _PTS(5) y lo de antes (AcpiEnterSleepStatePrep): %s\n", AcpiFormatException(s));
	vacia_registro();
	if (ACPI_FAILURE(s))
		return (int)s;
	__asm__ __volatile__("cli");
	s = AcpiEnterSleepState(ACPI_STATE_S5);
	/* si sigue aquí, no se ha apagado */
	__asm__ __volatile__("sti");
	reg("ACPI: el equipo sigue encendido tras SLP_EN (%s)\n", AcpiFormatException(s));
	return ACPI_FAILURE(s) ? (int)s : -2;
}

/* ---- lo que se sabe ---- */
static u32 cuantos_kb(u32 o) { return (o + 1023) >> 10; }
extern u32 mem_usada, mem_max;

static u32 copia(struct h64_estado *e, u32 tam)
{
	struct h64_estado x;
	const struct ll_bat *b = &estado.bat[0];

	memset(&x, 0, sizeof x);
	x.version = H64_VERSION;
	x.fase = apm.fase;
	x.nbat = apm.nbat;
	x.pct = apm.pct;
	x.ca = apm.ca;
	x.estado = apm.estado;
	x.minutos = apm.minutos == 0xFFFF ? 0xFFFF : apm.minutos & 0x7FFF;
	x.lecturas = apm.lecturas;
	x.unidad = b->unidad;
	x.diseno = b->diseno;
	x.llena = b->llena;
	x.queda = b->queda;
	x.ritmo = b->ritmo;
	x.voltaje = b->voltaje;
	x.bst_estado = b->estado;
	x.ciclos = salud.bat[0].valida ? salud.bat[0].ciclos : 0xFFFFFFFFu;
	x.s5 = s5_hay;
	x.slp_a = s5_a;
	x.slp_b = s5_b;
	x.botones = botones;
	x.brillo_hay = brillo_h != NULL;
	x.brillo = (u32)brillo_lee();
	x.brillo_n = brillo_n;
	x.ec_hay = estado.ec_hay;
	x.ec_eventos = estado.ec_eventos;
	x.errores = osl_errores;
	x.memoria_kb = cuantos_kb(mem_usada);
	x.memoria_max_kb = cuantos_kb(mem_max);
	x.avisos = avisos_n;
	memcpy(x.bat_ruta, b->ruta, sizeof x.bat_ruta);
	x.bat_ruta[sizeof x.bat_ruta - 1] = 0;
	memcpy(x.modelo, b->modelo, sizeof x.modelo);
	x.modelo[sizeof x.modelo - 1] = 0;
	memcpy(x.ca_ruta, estado.ca_ruta, sizeof x.ca_ruta);
	x.ca_ruta[sizeof x.ca_ruta - 1] = 0;
	memcpy(x.ec_ruta, estado.ec_ruta, sizeof x.ec_ruta);
	x.ec_ruta[sizeof x.ec_ruta - 1] = 0;
	if (brillo_h) {
		ACPI_BUFFER n = { sizeof x.brillo_ruta, x.brillo_ruta };

		AcpiGetName(brillo_h, ACPI_FULL_PATHNAME, &n);
	}
	if (tam > sizeof x)
		tam = sizeof x;
	memcpy(e, &x, tam);
	return tam;
}

/* (pruebas) un método sin argumentos que devuelve un entero */
static long prueba(u64 que, const char *nombre)
{
	ACPI_BUFFER b = { ACPI_ALLOCATE_BUFFER, NULL };
	long v = -1;

	if (que != 1 || arrancado != 1)
		return -1;
	if (ACPI_SUCCESS(AcpiEvaluateObjectTyped(NULL, (char *)nombre, NULL, &b, ACPI_TYPE_INTEGER))) {
		v = (long)((ACPI_OBJECT *)b.Pointer)->Integer.Value;
		AcpiOsFree(b.Pointer);
	}
	return v;
}

/* ---- la entrada (entrada.asm) ---- */
long nucleo64_llamada(u64 f, u64 a, u64 b)
{
	long r = -1;

	switch (f) {
	case H64_INICIA: {
		const struct h64_inicio *i = (const struct h64_inicio *)a;

		if (i->version != H64_VERSION)
			return -2;
		h64 = *i;
		mem_inicia((void *)h64.monton, (u32)h64.monton_largo);
		memset(&ini, 0, sizeof ini);
		ini.opciones = (h64.opciones & H64_OP_DETALLE ? LL_OP_DETALLE : 0) | (h64.opciones & H64_OP_SININI ? LL_OP_SININI : 0);
		ini.osi = LL_OSI_TODO;		/* a _OSI, que sí a todos los Windows (como Linux) */
		ini.cada_s = 10;
		memset(&estado, 0, sizeof estado);
		memset(&apm, 0, sizeof apm);
		memset(&salud, 0, sizeof salud);
		salud.pct = 0xFF;
		estado.version = LL_VERSION;
		apm.ca = apm.estado = apm.pct = 0xFF;
		apm.minutos = 0xFFFF;
		apm.banderas = 0x80;
		return 0;
	}
	case H64_ARRANCA:
		r = arrancado ? (arrancado < 0 ? -1 : 0) : arranca();
		break;
	case H64_LEE:
		if (arrancado == 1 && apm.fase != FASE_SIN_BATERIA && apm.fase != FASE_ERROR)
			r = bateria_lectura();
		break;
	case H64_EVENTOS: {
		u32 b0 = botones;

		if (arrancado == 1) {
			ec_eventos();
			AcpiOsWaitEventsComplete();
			r = bateria_cambio() || botones != b0;
		}
		break;
	}
	case H64_ESTADO:
		r = copia((struct h64_estado *)a, (u32)b);
		break;
	case H64_PRT:
		r = arrancado == 1 ? rutas((const char *)a, (struct h64_rutas *)b) : -1;
		break;
	case H64_APAGA:
		r = apaga();
		break;
	case H64_BRILLO:
		r = arrancado == 1 ? ((long)a < 0 ? brillo_lee() : brillo_pon((u32)a)) : -1;
		break;
	case H64_REGISTRO:
		return reg_saca((char *)a, (u32)b);
	case H64_PRUEBA:
		r = prueba(a, (const char *)b);
		break;
	}
	vacia_registro();
	return r;
}
