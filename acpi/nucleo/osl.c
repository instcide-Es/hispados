/* osl.c - Lo que ACPICA pide al sistema (AcpiOs...), para el núcleo ACPI de
 * Hispa-486: un solo hilo, sin interrupciones, la memoria física a mano y
 * los puertos directos.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "acpi.h"
#include "accommon.h"
#include "nucleo.h"

u32 osl_errores;		/* mensajes de error o aviso de ACPICA */
u32 osl_dormido_ms;		/* lo que los métodos han pedido dormir */

ACPI_STATUS AcpiOsInitialize(void) { return AE_OK; }
ACPI_STATUS AcpiOsTerminate(void) { return AE_OK; }

ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer(void)
{
	return mq_rsdp();
}

/* \_OS: ACPICA dice «Microsoft Windows NT». En el modo mínimo decimos
 * «Microsoft Windows» (Windows 98, el primero con ACPI): los equipos hacen
 * entonces lo menos posible al avisar a su controlador embebido (el HP Mini
 * 5103 no le cambia el tipo de sistema ni toca el ventilador). */
ACPI_STATUS AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES *o, ACPI_STRING *n)
{
	*n = NULL;
	if ((ini.opciones & LL_OP_OS98) && !strncmp(o->Name, "_OS_", 4))
		*n = "Microsoft Windows";
	return AE_OK;
}
ACPI_STATUS AcpiOsTableOverride(ACPI_TABLE_HEADER *t, ACPI_TABLE_HEADER **n)
{
	*n = NULL;
	return AE_OK;
}
ACPI_STATUS AcpiOsPhysicalTableOverride(ACPI_TABLE_HEADER *t, ACPI_PHYSICAL_ADDRESS *d, UINT32 *l)
{
	*d = 0;
	return AE_OK;
}

/* ---- candados y semáforos: hay un solo hilo ---- */
ACPI_STATUS AcpiOsCreateLock(ACPI_SPINLOCK *l) { *l = (ACPI_SPINLOCK)1; return AE_OK; }
void AcpiOsDeleteLock(ACPI_SPINLOCK l) { }
ACPI_CPU_FLAGS AcpiOsAcquireLock(ACPI_SPINLOCK l) { return 0; }
void AcpiOsReleaseLock(ACPI_SPINLOCK l, ACPI_CPU_FLAGS f) { }

struct sem { u32 hay, max; };
ACPI_STATUS AcpiOsCreateSemaphore(UINT32 max, UINT32 ini, ACPI_SEMAPHORE *s)
{
	struct sem *m = mem_pide(sizeof *m);

	if (!m)
		return AE_NO_MEMORY;
	m->hay = ini;
	m->max = max;
	*s = m;
	return AE_OK;
}
ACPI_STATUS AcpiOsDeleteSemaphore(ACPI_SEMAPHORE s)
{
	mem_suelta(s);
	return AE_OK;
}
ACPI_STATUS AcpiOsWaitSemaphore(ACPI_SEMAPHORE s, UINT32 n, UINT16 espera)
{
	struct sem *m = s;

	if (!m)
		return AE_BAD_PARAMETER;
	if (m->hay < n) {
		/* nadie más lo va a soltar: sería esperar para siempre */
		if (espera == ACPI_WAIT_FOREVER) {
			reg("ACPI: un método espera un Mutex/Event que nadie suelta: se sigue sin él\n");
			osl_errores++;
		}
		return AE_TIME;
	}
	m->hay -= n;
	return AE_OK;
}
ACPI_STATUS AcpiOsSignalSemaphore(ACPI_SEMAPHORE s, UINT32 n)
{
	struct sem *m = s;

	if (!m)
		return AE_BAD_PARAMETER;
	m->hay += n;
	return AE_OK;
}

/* ---- memoria ---- */
void *AcpiOsAllocate(ACPI_SIZE t) { return mem_pide(t); }
void AcpiOsFree(void *p) { mem_suelta(p); }
void *AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS d, ACPI_SIZE t) { return mq_mapa(d, t); }
void AcpiOsUnmapMemory(void *p, ACPI_SIZE t) { mq_desmapa(p, t); }

ACPI_STATUS AcpiOsReadMemory(ACPI_PHYSICAL_ADDRESS d, UINT64 *v, UINT32 bits)
{
	void *p = mq_mapa(d, bits / 8);

	if (!p)
		return AE_NOT_EXIST;
	switch (bits) {
	case 8: *v = *(volatile u8 *)p; break;
	case 16: *v = *(volatile u16 *)p; break;
	case 32: *v = *(volatile u32 *)p; break;
	case 64: *v = *(volatile u64 *)p; break;
	default: return AE_BAD_PARAMETER;
	}
	mq_desmapa(p, bits / 8);
	return AE_OK;
}
ACPI_STATUS AcpiOsWriteMemory(ACPI_PHYSICAL_ADDRESS d, UINT64 v, UINT32 bits)
{
	void *p = mq_mapa(d, bits / 8);

	if (!p)
		return AE_NOT_EXIST;
	mq_traza_mem((u32)d, (u32)v, bits);	/* (TERMO) */
	switch (bits) {
	case 8: *(volatile u8 *)p = v; break;
	case 16: *(volatile u16 *)p = v; break;
	case 32: *(volatile u32 *)p = v; break;
	case 64: *(volatile u64 *)p = v; break;
	default: return AE_BAD_PARAMETER;
	}
	mq_desmapa(p, bits / 8);
	return AE_OK;
}

/* ---- puertos y configuración PCI (mecanismo 1: CF8h/CFCh) ---- */
ACPI_STATUS AcpiOsReadPort(ACPI_IO_ADDRESS p, UINT32 *v, UINT32 bits)
{
	*v = mq_es_lee(p, bits / 8);
	return AE_OK;
}
ACPI_STATUS AcpiOsWritePort(ACPI_IO_ADDRESS p, UINT32 v, UINT32 bits)
{
	mq_es_escribe(p, bits / 8, v);
	return AE_OK;
}
static u32 pci_dir(ACPI_PCI_ID *id, UINT32 r)
{
	return 0x80000000u | (id->Bus & 0xFF) << 16 | (id->Device & 31) << 11 | (id->Function & 7) << 8 | (r & 0xFC);
}
ACPI_STATUS AcpiOsReadPciConfiguration(ACPI_PCI_ID *id, UINT32 r, UINT64 *v, UINT32 bits)
{
	if (r > 255 || id->Segment) {
		*v = ~0ULL;
		return AE_OK;
	}
	mq_es_escribe(0xCF8, 4, pci_dir(id, r));
	*v = mq_es_lee(0xCFC + (r & (bits == 8 ? 3 : bits == 16 ? 2 : 0)), bits / 8);
	return AE_OK;
}
ACPI_STATUS AcpiOsWritePciConfiguration(ACPI_PCI_ID *id, UINT32 r, UINT64 v, UINT32 bits)
{
	if (r > 255 || id->Segment)
		return AE_OK;
	mq_es_escribe(0xCF8, 4, pci_dir(id, r));
	mq_es_escribe(0xCFC + (r & (bits == 8 ? 3 : bits == 16 ? 2 : 0)), bits / 8, v);
	return AE_OK;
}

/* ---- interrupciones: no se usa la de ACPI (SCI); el equipo se queda como
 * lo dejó la BIOS y los avisos del controlador embebido se miran a mano ---- */
UINT32 AcpiOsInstallInterruptHandler(UINT32 n, ACPI_OSD_HANDLER f, void *c) { return AE_OK; }
ACPI_STATUS AcpiOsRemoveInterruptHandler(UINT32 n, ACPI_OSD_HANDLER f) { return AE_OK; }

/* ---- hilos y tiempo ---- */
ACPI_THREAD_ID AcpiOsGetThreadId(void) { return 1; }

/* lo que ACPICA deja para después (los Notify): se hace al acabar el método */
#define NCOLA 16
static struct { ACPI_OSD_EXEC_CALLBACK f; void *c; } cola[NCOLA];
static int ncola;
ACPI_STATUS AcpiOsExecute(ACPI_EXECUTE_TYPE t, ACPI_OSD_EXEC_CALLBACK f, void *c)
{
	if (ncola == NCOLA)
		return AE_NO_MEMORY;
	cola[ncola].f = f;
	cola[ncola].c = c;
	ncola++;
	return AE_OK;
}
void AcpiOsWaitEventsComplete(void)
{
	while (ncola) {
		ACPI_OSD_EXEC_CALLBACK f = cola[0].f;
		void *c = cola[0].c;

		ncola--;
		memmove(cola, cola + 1, ncola * sizeof cola[0]);
		f(c);
	}
}

void AcpiOsSleep(UINT64 ms)
{
	if (ms > 5000)
		ms = 5000;
	osl_dormido_ms += ms;
	tarea_duerme(ms);
}
void AcpiOsStall(UINT32 us)
{
	mq_espera_us(us);
}
UINT64 AcpiOsGetTimer(void)
{
	return mq_ahora_us() * 10;	/* en décimas de microsegundo */
}
ACPI_STATUS AcpiOsEnterSleep(UINT8 e, UINT32 a, UINT32 b) { return AE_OK; }

ACPI_STATUS AcpiOsSignal(UINT32 f, void *i)
{
	if (f == ACPI_SIGNAL_FATAL)
		reg("ACPI: el equipo ejecuta Fatal()\n");
	return AE_OK;
}

/* ---- lo que ACPICA escribe (errores, avisos y la lista de tablas) ---- */
void AcpiOsVprintf(const char *f, va_list a)
{
	if (!strncmp(f, "ACPI Error", 10) || !strncmp(f, "ACPI Warning", 12) || !strncmp(f, "ACPI BIOS", 9) ||
	    !strncmp(f, "ACPI Exception", 14) || !strncmp(f, ACPI_MSG_ERROR, 10))
		osl_errores++;
	reg_v(f, a);
}
void ACPI_INTERNAL_VAR_XFACE AcpiOsPrintf(const char *f, ...)
{
	va_list a;

	va_start(a, f);
	AcpiOsVprintf(f, a);
	va_end(a);
}
