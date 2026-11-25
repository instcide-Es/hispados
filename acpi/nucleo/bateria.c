/* bateria.c - Núcleo ACPI de Hispa-486: arrancar ACPICA y leer la batería.
 *
 * Lo que se hace, en orden (y todo se apunta en el registro):
 *  1. Las tablas: RSDP -> RSDT/XSDT -> FADT, DSDT y las SSDT; ACPICA monta
 *     con ellas el árbol de nombres del equipo.
 *  2. El controlador embebido (PNP0C09): se pone el manejador de su región.
 *  3. El _REG (3, 1) del controlador embebido, que es el aviso de «ya
 *     puedes usarlo» (sin él, el Toshiba contesta valores de fábrica). En
 *     el modo mínimo (el normal), nada más. Con /COMPLETO, antes, los
 *     métodos de arranque del equipo como hace un sistema con ACPI:
 *     \_SB._INI y el _INI de cada dispositivo presente.
 *     NO se pasa el equipo a «modo ACPI» (no se escribe ACPI_ENABLE): la
 *     BIOS sigue llevando el botón de encendido, el ventilador y las teclas.
 *  4. Las baterías (PNP0C0A) y el adaptador (ACPI0003).
 *  5. Cada pocos segundos: avisos del controlador embebido (_Qxx) y, de
 *     cada batería, _STA, _BIF (o _BIX) la primera vez y
 *     cuando el equipo avisa de que ha cambiado, y _BST. De ahí salen el
 *     porcentaje, si está con cable o cargando y los minutos que quedan,
 *     que el residente contesta por INT 15h AX=530Ah como una BIOS con APM.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "acpi.h"
#include "accommon.h"
#include "nucleo.h"

int tarea_descansa(u32 ms);
extern u32 osl_errores, osl_dormido_ms;

static ACPI_HANDLE bat[LL_NBAT], ca, refresco;
static int nbat, releer_info[LL_NBAT], fallos[LL_NBAT];
static u32 ciclos;
static u32 ciclos_carga[LL_NBAT];	/* [SALUD] de _BIX (FFFFFFFFh: no se sabe) */
static int hablar;			/* esta lectura se apunta entera */

/* ---- apuntar un objeto de ACPI en el registro ---- */
static void obj_reg(const ACPI_OBJECT *o, int nivel)
{
	u32 i;

	if (!o) {
		reg("(nada)");
		return;
	}
	switch (o->Type) {
	case ACPI_TYPE_INTEGER:
		reg("%Xh", (u32)o->Integer.Value);
		break;
	case ACPI_TYPE_STRING:
		reg("\"");
		for (i = 0; i < o->String.Length && i < 40; i++)
			reg("%c", o->String.Pointer[i] >= 32 && o->String.Pointer[i] < 127 ? o->String.Pointer[i] : '.');
		reg("\"");
		break;
	case ACPI_TYPE_BUFFER:
		reg("Buffer(%u){", o->Buffer.Length);
		for (i = 0; i < o->Buffer.Length && i < 16; i++)
			reg("%s%02X", i ? " " : "", o->Buffer.Pointer[i]);
		reg(i < o->Buffer.Length ? " ...}" : "}");
		break;
	case ACPI_TYPE_PACKAGE:
		reg("Package(%u){", o->Package.Count);
		for (i = 0; i < o->Package.Count && i < 24; i++) {
			if (i)
				reg(", ");
			if (nivel < 2)
				obj_reg(&o->Package.Elements[i], nivel + 1);
			else
				reg("...");
		}
		reg("}");
		break;
	default:
		reg("(tipo %u)", o->Type);
	}
}

/* evalúa un método sin argumentos; *res hay que soltarlo con AcpiOsFree */
static ACPI_STATUS evalua(ACPI_HANDLE h, const char *ruta, const char *nombre, ACPI_OBJECT **res)
{
	ACPI_BUFFER b = { ACPI_ALLOCATE_BUFFER, NULL };
	u32 ec0 = estado.ec_lecturas + estado.ec_escrituras, d0 = osl_dormido_ms;
	u64 t0 = mq_ahora_us();
	ACPI_STATUS s = AcpiEvaluateObject(h, (char *)nombre, NULL, &b);

	AcpiOsWaitEventsComplete();
	if (hablar || (ACPI_FAILURE(s) && s != AE_NOT_FOUND && ciclos < 50)) {
		reg("%s.%s -> ", ruta, nombre);
		if (ACPI_FAILURE(s))
			reg("ERROR: %s", AcpiFormatException(s));
		else
			obj_reg(b.Pointer, 0);
		reg("   [%u accesos al EC, %u ms; durmió %u ms]\n", estado.ec_lecturas + estado.ec_escrituras - ec0,
		    (u32)((mq_ahora_us() - t0) / 1000), osl_dormido_ms - d0);
	}
	if (ACPI_FAILURE(s)) {
		*res = NULL;
		return s;
	}
	*res = b.Pointer;
	return AE_OK;
}

static int entero(const ACPI_OBJECT *p, u32 i, u32 *v)
{
	if (!p || p->Type != ACPI_TYPE_PACKAGE || i >= p->Package.Count ||
	    p->Package.Elements[i].Type != ACPI_TYPE_INTEGER)
		return 0;
	*v = (u32)p->Package.Elements[i].Integer.Value;
	return 1;
}

static void texto(const ACPI_OBJECT *p, u32 i, char *d, u32 tam)
{
	const ACPI_OBJECT *e;
	const u8 *o;
	u32 n, j, k = 0;

	d[0] = 0;
	if (!p || p->Type != ACPI_TYPE_PACKAGE || i >= p->Package.Count)
		return;
	e = &p->Package.Elements[i];
	if (e->Type == ACPI_TYPE_STRING) {
		o = (const u8 *)e->String.Pointer;
		n = e->String.Length;
	} else if (e->Type == ACPI_TYPE_BUFFER) {
		o = e->Buffer.Pointer;
		n = e->Buffer.Length;
	} else
		return;
	for (j = 0; j < n && o[j] && k + 1 < tam; j++)
		if (o[j] >= 32 && o[j] < 127)
			d[k++] = o[j];
	d[k] = 0;
}

/* ---- buscar los dispositivos ---- */
static ACPI_STATUS bat_hallada(ACPI_HANDLE h, UINT32 nivel, void *ctx, void **ret)
{
	if (nbat < LL_NBAT) {
		ACPI_BUFFER b = { sizeof estado.bat[0].ruta, estado.bat[nbat].ruta };

		AcpiGetName(h, ACPI_FULL_PATHNAME, &b);
		estado.bat[nbat].hay = 1;
		releer_info[nbat] = 1;
		bat[nbat++] = h;
	}
	return AE_OK;
}
static ACPI_STATUS ca_hallado(ACPI_HANDLE h, UINT32 nivel, void *ctx, void **ret)
{
	if (!ca) {
		ACPI_BUFFER b = { sizeof estado.ca_ruta, estado.ca_ruta };

		AcpiGetName(h, ACPI_FULL_PATHNAME, &b);
		estado.ca_hay = 1;
		ca = h;
	}
	return AE_OK;
}

/* los Notify del equipo: 80h «ha cambiado el estado», 81h «ha cambiado la
 * información» (batería distinta, capacidad recalculada) */
static u32 avisos;
static int cambio;			/* el equipo avisa de que la batería o el cable han cambiado */
/* [Hispa-64] los demás avisos (el botón de encendido PNP0C0C, el brillo),
 * para quien los quiera (acpi/hispa64/servicio.c; en Hispa-DOS no hay) */
void nucleo_aviso(ACPI_HANDLE h, UINT32 valor) __attribute__((weak));
static void aviso(ACPI_HANDLE h, UINT32 valor, void *ctx)
{
	int i;

	if (nucleo_aviso)
		nucleo_aviso(h, valor);

	for (i = 0; i < nbat; i++)
		if (h == bat[i]) {
			cambio = 1;
			if (valor == 0x81)
				releer_info[i] = 1;
		}
	if (h == ca)
		cambio = 1;
	if (avisos++ < 30) {
		char r[40];
		ACPI_BUFFER b = { sizeof r, r };

		r[0] = 0;
		AcpiGetName(h, ACPI_FULL_PATHNAME, &b);
		reg("Notify (%s, %02Xh)\n", r, valor);
	}
}

static const char *osi_xp[] = { "Windows 2000", "Windows 2001", "Windows 2001 SP1", "Windows 2001 SP2", 0 };
static const char *osi_w7[] = { "Windows 2001.1", "Windows 2001.1 SP1", "Windows 2006", "Windows 2006.1",
				"Windows 2006 SP1", "Windows 2006 SP2", "Windows 2009", 0 };

#define PASO(que, llamada) do { \
		s = llamada; \
		if (ACPI_FAILURE(s)) { \
			reg("ACPI: falla %s: %s\n", que, AcpiFormatException(s)); \
			return s; \
		} \
	} while (0)

static ACPI_STATUS acpi_arranca(void)
{
	ACPI_STATUS s;
	ACPI_HANDLE h;
	int i;

	reg("ACPI: RSDP en %08Xh\n", (u32)mq_rsdp());
	if (!mq_rsdp()) {
		reg("ACPI: el equipo no tiene ACPI (no se encuentra la RSDP)\n");
		return AE_NOT_FOUND;
	}
	PASO("el arranque de ACPICA", AcpiInitializeSubsystem());
	/* un bucle de un método que no acaba (espera algo que no llega) se
	 * corta a los 3 s, no a los 30 de ACPICA: el equipo está parado */
	AcpiGbl_MaxLoopIterations = 3;
	PASO("la lectura de las tablas", AcpiInitializeTables(NULL, 32, FALSE));
	if (ini.osi != LL_OSI_TODO) {
		AcpiUpdateInterfaces(ACPI_DISABLE_ALL_VENDOR_STRINGS);
		if (ini.osi >= LL_OSI_XP)
			for (i = 0; osi_xp[i]; i++)
				AcpiInstallInterface((char *)osi_xp[i]);
		if (ini.osi >= LL_OSI_W7)
			for (i = 0; osi_w7[i]; i++)
				AcpiInstallInterface((char *)osi_w7[i]);
	}
	reg("ACPI: al equipo se le dice que somos (\\_OS) «%s» y (_OSI) %s\n",
	    ini.opciones & LL_OP_OS98 ? "Microsoft Windows" : "Microsoft Windows NT",
	    ini.osi == LL_OSI_NADA ? "ningún Windows" : ini.osi == LL_OSI_XP ? "como Windows XP SP2" :
	    ini.osi == LL_OSI_W7 ? "como Windows 7" : "cualquier Windows");
	PASO("la carga de la DSDT y las SSDT", AcpiLoadTables());
	reg("ACPI: tablas cargadas; memoria en uso %u KB\n", mem_usada >> 10);
	if (ec_prepara() < 0)
		return AE_ERROR;
	PASO("AcpiEnableSubsystem",
	     AcpiEnableSubsystem(ACPI_NO_ACPI_ENABLE | ACPI_NO_EVENT_INIT | ACPI_NO_HANDLER_INIT));
	AcpiInstallNotifyHandler(ACPI_ROOT_OBJECT, ACPI_ALL_NOTIFY, aviso, NULL);
	if (ini.opciones & LL_OP_SININI)
		reg("ACPI: modo mínimo: no se ejecutan los _INI del equipo\n");
	else
		reg("ACPI: se ejecutan los _INI del equipo...\n");
	PASO("los métodos de arranque (_INI, _STA, _REG)",
	     AcpiInitializeObjects((ini.opciones & LL_OP_SININI) ? ACPI_NO_DEVICE_INIT : 0));
	AcpiOsWaitEventsComplete();
	/* sin baterías declaradas (un sobremesa) no hay nada que leer: no se
	 * le dice nada al controlador embebido */
	AcpiGetDevices("PNP0C0A", bat_hallada, NULL, NULL);
	AcpiGetDevices("ACPI0003", ca_hallado, NULL, NULL);
	if (!nbat)
		return AE_OK;
	ec_reg(1);
	AcpiOsWaitEventsComplete();
	reg("ACPI: equipo preparado; memoria en uso %u KB (máximo %u KB); %u avisos o errores de ACPICA; "
	    "los métodos han dormido %u ms\n", mem_usada >> 10, mem_max >> 10, osl_errores, osl_dormido_ms);

	for (i = 0; i < nbat; i++)
		reg("Batería %d: %s\n", i + 1, estado.bat[i].ruta);
	reg(ca ? "Adaptador de corriente: %s\n" : "Adaptador de corriente: el equipo no lo declara (ACPI0003)\n",
	    estado.ca_ruta);
	/* HP: los métodos de la batería repiten lo último leído hasta que un
	 * aviso del controlador embebido marca «hay que releer» (PWUP). Como
	 * aquí no hay interrupción de ACPI, se marca antes de cada lectura. */
	if (ec_disp() && !(ini.opciones & LL_OP_SINREFRESCO) &&
	    ACPI_SUCCESS(AcpiGetHandle(ec_disp(), "PWUP", &h))) {
		ACPI_DEVICE_INFO *di;

		if (ACPI_SUCCESS(AcpiGetObjectInfo(h, &di))) {
			if (di->Type == ACPI_TYPE_METHOD && di->ParamCount == 2) {
				refresco = h;
				reg("Equipo de HP: antes de cada lectura se llama a %s.PWUP (7, FFh) para que relea\n",
				    estado.ec_ruta);
			}
			AcpiOsFree(di);
		}
	}
	return AE_OK;
}

static void lee_info(int i, struct ll_bat *b)
{
	ACPI_OBJECT *p;
	int x = 0;	/* _BIX lleva un elemento más delante (la revisión) */
	u32 v;

	b->valida = 0;
	if (ACPI_FAILURE(evalua(bat[i], b->ruta, "_BIF", &p))) {
		if (ACPI_FAILURE(evalua(bat[i], b->ruta, "_BIX", &p)))
			return;
		x = 1;
	}
	if (entero(p, x + 0, &v) && entero(p, x + 1, &b->diseno) && entero(p, x + 2, &b->llena)) {
		b->unidad = v;
		if (!entero(p, x + 4, &b->voltaje_diseno))
			b->voltaje_diseno = 0xFFFFFFFF;
		texto(p, x ? 16 : 9, b->modelo, sizeof b->modelo);
		texto(p, x ? 18 : 11, b->tipo, sizeof b->tipo);
		texto(p, x ? 19 : 12, b->marca, sizeof b->marca);
		b->valida = 1;
		releer_info[i] = 0;
		/* [SALUD] los ciclos de carga: solo los da _BIX (el octavo
		 * elemento tras la revisión). Si se ha leído _BIF, se mira si el
		 * equipo tiene también _BIX, y solo entonces se evalúa. */
		ciclos_carga[i] = 0xFFFFFFFF;
		if (x)
			entero(p, 8, &ciclos_carga[i]);
		else {
			ACPI_HANDLE h;
			ACPI_OBJECT *q;

			if (ACPI_SUCCESS(AcpiGetHandle(bat[i], "_BIX", &h)) &&
			    ACPI_SUCCESS(evalua(bat[i], b->ruta, "_BIX", &q))) {
				entero(q, 8, &ciclos_carga[i]);
				AcpiOsFree(q);
			}
		}
	} else
		reg("%s: la información de la batería no tiene la forma esperada\n", b->ruta);
	AcpiOsFree(p);
}

/* [SALUD] el desgaste, para INT 2Fh AX=DB05h: de cada batería puesta con
 * _BIF/_BIX leído, la capacidad de fábrica y la de la última carga completa
 * (si la segunda es mayor que la primera, o falta alguna, no se sabe: no se
 * inventa), y la salud de todas juntas (si todas van en la misma unidad). */
static void salud_cocina(void)
{
	struct ll_salud s;
	u32 dis = 0, lle = 0;
	int i, unidad = -1, mezcla = 0;

	memset(&s, 0, sizeof s);
	s.pct = 0xFF;
	for (i = 0; i < nbat && i < LL_NBAT; i++) {
		struct ll_bat *b = &estado.bat[i];
		struct ll_salud_bat *d = &s.bat[i];
		u32 c = ciclos_carga[i];

		d->ciclos = c == 0xFFFFFFFF || c > 0xFFFE ? 0xFFFF : (unsigned short)c;
		if (!(b->sta & 0x10) || !b->valida || b->diseno == 0 || b->diseno >= 0x7FFFFFFF ||
		    b->llena == 0 || b->llena >= 0x7FFFFFFF || b->llena > b->diseno + b->diseno / 20) {
			d->ciclos = 0xFFFF;
			continue;
		}
		d->valida = 1;
		d->unidad = b->unidad ? 1 : 0;
		d->diseno = b->diseno;
		d->llena = b->llena > b->diseno ? b->diseno : b->llena;
		if (unidad >= 0 && unidad != d->unidad)
			mezcla = 1;
		unidad = d->unidad;
		dis += d->diseno;
		lle += d->llena;
		s.nbat++;
	}
	if (s.nbat && !mezcla && dis)
		s.pct = (unsigned char)((u64)lle * 100 / dis);
	s.lecturas = (unsigned short)ciclos;
	salud = s;
}

static int desconocido(u32 v)
{
	return v == 0xFFFFFFFF || v == 0;
}

static void lectura(void)
{
	ACPI_OBJECT *p;
	struct ll_apm a = apm;
	u32 queda = 0, llena = 0, ritmo = 0, v;
	int i, psr = 0, puestas = 0, validas = 0, cargando = 0, descargando = 0, critica = 0, ritmo_ok = 1;

	hablar = ciclos < 2 || (ini.opciones & LL_OP_DETALLE && ciclos < 6);
	if (hablar)
		reg("---- lectura %u (a los %u s) ----\n", ciclos + 1, (u32)(mq_ahora_us() / 1000000));
	ec_eventos();
	if (refresco) {
		ACPI_OBJECT ar[2];
		ACPI_OBJECT_LIST l = { 2, ar };
		ACPI_STATUS s;

		ar[0].Type = ar[1].Type = ACPI_TYPE_INTEGER;
		ar[0].Integer.Value = 7;
		ar[1].Integer.Value = 0xFF;
		s = AcpiEvaluateObject(refresco, NULL, &l, NULL);
		if (hablar || ACPI_FAILURE(s))
			reg("%s.PWUP (7, FFh): %s\n", estado.ec_ruta, AcpiFormatException(s));
	}
	/* El cable: lo normal es deducirlo de la batería (si no se está
	 * descargando, hay cable), porque en algunos equipos _PSR hace más
	 * cosas que contestar: en el HP Mini 5103, al cambiar el cable apaga
	 * o enciende la tarjeta de red por cable. Con /PSR se pregunta. */
	if (ca && (ini.opciones & LL_OP_PSR)) {
		if (ACPI_SUCCESS(evalua(ca, estado.ca_ruta, "_PSR", &p))) {
			if (p && p->Type == ACPI_TYPE_INTEGER) {
				estado.ca = p->Integer.Value ? 1 : 0;
				psr = 1;
			}
			AcpiOsFree(p);
		}
	}
	for (i = 0; i < nbat; i++) {
		struct ll_bat *b = &estado.bat[i];
		u32 est, rit, que, vol;

		b->sta = 0x1F;		/* sin _STA: está y funciona */
		if (ACPI_SUCCESS(evalua(bat[i], b->ruta, "_STA", &p))) {
			if (p && p->Type == ACPI_TYPE_INTEGER)
				b->sta = p->Integer.Value;
			AcpiOsFree(p);
		}
		if (!(b->sta & 0x10)) {
			b->valida = 0;
			releer_info[i] = 1;
			continue;
		}
		puestas++;
		if (releer_info[i] || !b->valida || ciclos % 60 == 59)
			lee_info(i, b);
		if (!b->valida)
			continue;
		if (ACPI_FAILURE(evalua(bat[i], b->ruta, "_BST", &p)) || !entero(p, 0, &est) ||
		    !entero(p, 1, &rit) || !entero(p, 2, &que) || !entero(p, 3, &vol)) {
			AcpiOsFree(p);
			/* un fallo suelto: se queda lo último; al tercero, no se sabe */
			if (++fallos[i] < 3 && b->queda != 0xFFFFFFFF)
				goto suma;
			b->queda = 0xFFFFFFFF;
			continue;
		}
		AcpiOsFree(p);
		fallos[i] = 0;
		b->estado = est;
		b->ritmo = rit;
		b->queda = que;
		b->voltaje = vol;
suma:
		v = desconocido(b->llena) ? b->diseno : b->llena;
		if (b->queda == 0xFFFFFFFF || desconocido(v))
			continue;
		validas++;
		queda += b->queda;
		llena += v;
		if (b->estado & 1)
			descargando = 1;
		if (b->estado & 2)
			cargando = 1;
		if (b->estado & 4)
			critica = 1;
		/* el ritmo, solo de las que se están gastando: con dos baterías
		 * (ThinkPad A275), la que está quieta da ritmo 0 y no debe anular
		 * los minutos de la otra (18/11/2026, su BATERIA.LOG) */
		if (b->estado & 1) {
			if (desconocido(b->ritmo) || b->ritmo > 0x7FFFFFFF)
				ritmo_ok = 0;
			else
				ritmo += b->ritmo;
		}
	}
	AcpiOsWaitEventsComplete();

	a.nbat = puestas;
	a.pct = 0xFF;
	a.minutos = 0xFFFF;
	a.estado = 0xFF;
	a.banderas = 0x80;
	if (psr)
		a.ca = estado.ca;
	else
		a.ca = estado.ca = validas ? !descargando : puestas ? 0xFF : 1;
	if (validas) {
		u32 pct = (u32)((u64)queda * 100 / llena);

		if (pct > 100)
			pct = 100;
		a.pct = pct;
		a.estado = critica || pct < 5 ? 2 : pct < 25 ? 1 : 0;
		a.banderas = 1 << a.estado;
		if (cargando) {
			a.estado = 3;
			a.banderas |= 8;
		}
		/* Los minutos, solo con un ritmo creíble: justo al quitar el
		 * cable algunas baterías dan unas lecturas un ritmo de casi nada
		 * (el Toshiba NB250: 11h → «quedan 6564 min») o el de carga en 16
		 * bits con signo (FFAAh → «quedan 1 min»). La lectura en que se
		 * pasa de cargar o tener cable a descargar no da minutos (la
		 * primera de todas sí), y el ritmo tiene que estar entre llena/50
		 * (50 horas) y llena×2 (media hora). (18/11/2026, BATERIA.LOG del
		 * Toshiba) */
		static int antes = -1;	/* -1: primera lectura; 0/1: descargaba */
		int recien = descargando && antes == 0;
		antes = descargando;
		if (descargando && ritmo_ok && ritmo && !recien &&
		    ritmo >= llena / 50 && ritmo <= llena * 2) {
			u32 m = (u32)((u64)queda * 60 / ritmo);

			if (m && m < 0x7FFF)
				a.minutos = 0x8000 | m;
		}
	} else if (puestas) {
		a.banderas = 0xFF;	/* hay batería, pero no se sabe nada de ella */
	}
	a.lecturas = ++ciclos;
	a.fase = FASE_LISTO;
	if (hablar || a.pct != apm.pct || a.ca != apm.ca || a.estado != apm.estado || a.nbat != apm.nbat) {
		reg("Lectura %u: ", ciclos);
		if (!puestas)
			reg("no hay batería puesta");
		else if (a.pct == 0xFF)
			reg("batería puesta, nivel desconocido");
		else
			reg("%u %% (%u de %u %s)", a.pct, queda, llena, estado.bat[0].unidad ? "mAh" : "mWh");
		reg(a.ca == 1 ? ", con cable" : a.ca == 0 ? ", sin cable" : ", cable: no se sabe");
		reg(cargando ? ", cargando" : descargando ? ", descargando" : "");
		if (a.minutos != 0xFFFF)
			reg(", quedan %u min", a.minutos & 0x7FFF);
		reg("\n");
	}
	apm = a;
	salud_cocina();
	apm_publica();
}

void bateria_para(void)
{
	ec_reg(0);
}

/* [Hispa-64] el arranque y una lectura, por separado: Hispa-64 los llama
 * desde su hilo «acpi» (acpi/hispa64/servicio.c), sin la tarea de aquí.
 * bateria_arranca: 0 si hay baterías que leer; 1 si ACPI va pero no hay;
 * -1 si ACPI no va */
int bateria_arranca(void)
{
	ACPI_STATUS s;

	reg("HSPACPI: núcleo %u, opciones %Xh, lectura cada %u s\n", LL_VERSION, ini.opciones, ini.cada_s);
	s = acpi_arranca();
	if (ACPI_FAILURE(s)) {
		apm.fase = FASE_ERROR;
		apm.error = s;
		apm_publica();
		return -1;
	}
	if (!nbat) {
		reg("El equipo no declara baterías (PNP0C0A): es de sobremesa o no las lleva por ACPI\n");
		apm.fase = FASE_SIN_BATERIA;
		apm_publica();
		return 1;
	}
	return 0;
}

/* una lectura; devuelve 1 si el equipo ha avisado de un cambio desde la anterior */
int bateria_lectura(void)
{
	int c = cambio;

	lectura();
	cambio = 0;
	return c;
}

/* ¿ha avisado el equipo (Notify) de que la batería o el cable han cambiado? */
int bateria_cambio(void)
{
	return cambio;
}

void bateria_tarea(void)
{
	if (bateria_arranca())
		return;
	for (;;) {
		u64 fin;

		bateria_lectura();
		fin = mq_ahora_us() + (u64)(ini.cada_s ? ini.cada_s : 10) * 1000000;
		/* entre lecturas, los avisos del controlador embebido se miran
		 * cuatro veces por segundo (es leer un puerto): una tecla de
		 * brillo o el cable no esperan a la próxima lectura */
		while (mq_ahora_us() < fin) {
			if (ini.opciones & LL_OP_SINEVENTOS) {
				if (tarea_descansa((u32)((fin - mq_ahora_us()) / 1000) + 1))
					break;
				continue;
			}
			if (tarea_descansa(250))
				break;
			ec_eventos();
			if (cambio)
				break;
		}
	}
}
