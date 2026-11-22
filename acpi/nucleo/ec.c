/* ec.c - Núcleo ACPI de Hispa-486: el controlador embebido (PNP0C09), el
 * microcontrolador del portátil que conoce la batería. Los métodos del
 * equipo lo leen por una «región» EmbeddedControl: aquí está el manejador de
 * esa región, con el protocolo de sus dos puertos (ACPI 6, cap. 12):
 *   puerto de datos (62h) y de órdenes/estado (66h);
 *   estado: bit 0 OBF (hay un dato para leer), bit 1 IBF (aún no ha
 *   recogido lo último escrito), bit 5 SCI_EVT (tiene un aviso pendiente);
 *   órdenes: 80h leer (dirección -> dato), 81h escribir (dirección, dato),
 *   84h preguntar el aviso (-> número: se ejecuta el método _Qxx).
 * No se usa la interrupción de ACPI: los avisos se miran en cada lectura.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "acpi.h"
#include "accommon.h"
#include "nucleo.h"

#define EC_OBF 1
#define EC_IBF 2
#define EC_SCI 0x20
#define EC_LEE 0x80
#define EC_ESCRIBE 0x81
#define EC_AVISO 0x84
#define EC_ESPERA_US 100000	/* cada espera, como mucho 0,1 s */

static ACPI_HANDLE ec;
static u32 p_datos = 0x62, p_orden = 0x66;
static u32 detalle_quedan = 600;	/* accesos que se apuntan uno a uno */

void *ec_disp(void) { return ec; }

/* (TERMO) mientras el Termómetro lee las zonas térmicas (ACPI.BIN de
 * Hispa-Pentium, servicio.c), el EC es de solo lectura: lo que el AML quiera
 * escribir en la región no se escribe (se cuenta y se apunta) y se le dice
 * que ha ido bien, para que el método siga y lea */
int ec_solo_lectura;
unsigned int ec_ignoradas;

/* Lo normal es que conteste en microsegundos. Si tarda más de 2 ms, se
 * suelta el procesador entre miradas (el equipo sigue) hasta el plazo. */
static int espera(u32 mascara, u32 valor)
{
	u64 ini = mq_ahora_us();

	for (;;) {
		u64 va;

		if ((mq_es_lee(p_orden, 1) & mascara) == valor)
			return 0;
		va = mq_ahora_us() - ini;
		if (va > EC_ESPERA_US)
			return -1;
		if (va > 2000)
			tarea_duerme(10);
		else
			mq_espera_us(10);
	}
}

/* una orden entera; -1 si el controlador no contesta a tiempo */
static int orden(u32 cmd, int dir, int dato)
{
	if (espera(EC_IBF, 0))
		return -1;
	/* un dato viejo sin recoger estorbaría la respuesta */
	if (cmd != EC_ESCRIBE && (mq_es_lee(p_orden, 1) & EC_OBF))
		mq_es_lee(p_datos, 1);
	mq_es_escribe(p_orden, 1, cmd);
	if (espera(EC_IBF, 0))
		return -1;
	if (dir >= 0) {
		mq_es_escribe(p_datos, 1, dir);
		if (espera(EC_IBF, 0))
			return -1;
	}
	if (cmd == EC_ESCRIBE) {
		mq_es_escribe(p_datos, 1, dato);
		return espera(EC_IBF, 0);
	}
	if (espera(EC_OBF, EC_OBF))
		return -1;
	return mq_es_lee(p_datos, 1);
}

static int orden_insiste(u32 cmd, int dir, int dato)
{
	int i, r = -1;

	for (i = 0; i < 3 && r < 0; i++) {
		r = orden(cmd, dir, dato);
		if (r < 0) {
			estado.ec_fallos++;
			reg("EC: sin respuesta a la orden %02Xh (dirección %02Xh, estado %02Xh), intento %d\n",
			    cmd, dir & 0xFF, mq_es_lee(p_orden, 1), i + 1);
			tarea_duerme(10);
		}
	}
	return r;
}

static ACPI_STATUS ec_manejador(UINT32 funcion, ACPI_PHYSICAL_ADDRESS dir, UINT32 bits, UINT64 *valor,
				void *ctx, void *ctx_region)
{
	u32 n = (bits + 7) / 8, i;
	u8 *v = (u8 *)valor;

	if (dir + n > 256 || !valor)
		return AE_BAD_PARAMETER;
	for (i = 0; i < n; i++) {
		int r;

		if ((funcion & ACPI_IO_MASK) == ACPI_READ) {
			r = orden_insiste(EC_LEE, dir + i, 0);
			if (r < 0)
				return AE_TIME;
			v[i] = r;
			estado.ec_lecturas++;
			if (detalle_quedan && (ini.opciones & LL_OP_DETALLE)) {
				detalle_quedan--;
				reg("  EC[%02Xh] -> %02Xh\n", (u32)dir + i, r);
			}
		} else if (ec_solo_lectura) {
			/* (TERMO) no se escribe */
			if (ec_ignoradas++ < 40)
				reg("EC: (TERMO) el AML quiere escribir %02Xh en EC[%02Xh]: no se escribe (solo lectura)\n", v[i], (u32)dir + i);
		} else {
			r = orden_insiste(EC_ESCRIBE, dir + i, v[i]);
			if (r < 0)
				return AE_TIME;
			estado.ec_escrituras++;
			if (detalle_quedan && (ini.opciones & LL_OP_DETALLE)) {
				detalle_quedan--;
				reg("  EC[%02Xh] <- %02Xh\n", (u32)dir + i, v[i]);
			}
		}
	}
	return AE_OK;
}

static ACPI_STATUS ec_hallado(ACPI_HANDLE h, UINT32 nivel, void *ctx, void **ret)
{
	if (!ec)
		ec = h;
	return AE_CTRL_TERMINATE;
}

/* los puertos, del _CRS del dispositivo: el primero es el de datos y el
 * segundo el de órdenes (descriptores de E/S, cortos: 47h de 8 octetos, o
 * 4Bh, de dirección fija) */
static void ec_puertos(void)
{
	ACPI_BUFFER b = { ACPI_ALLOCATE_BUFFER, NULL };
	ACPI_OBJECT *o;
	u32 i = 0, n = 0, p[2];
	u8 *d;

	if (ACPI_FAILURE(AcpiEvaluateObject(ec, "_CRS", NULL, &b)))
		return;
	o = b.Pointer;
	if (o && o->Type == ACPI_TYPE_BUFFER) {
		d = o->Buffer.Pointer;
		while (i < o->Buffer.Length && n < 2) {
			u8 t = d[i];

			if (t & 0x80) {
				if (i + 3 > o->Buffer.Length)
					break;
				i += 3 + (d[i + 1] | d[i + 2] << 8);
				continue;
			}
			if ((t & 0x78) == 0x40 && i + 4 <= o->Buffer.Length)	/* E/S */
				p[n++] = d[i + 2] | d[i + 3] << 8;
			else if ((t & 0x78) == 0x48 && i + 3 <= o->Buffer.Length)	/* E/S fija */
				p[n++] = d[i + 1] | (d[i + 2] & 3) << 8;
			else if ((t & 0x78) == 0x78)
				break;
			i += 1 + (t & 7);
		}
		if (n == 2) {
			p_datos = p[0];
			p_orden = p[1];
		}
	}
	AcpiOsFree(b.Pointer);
}

int ec_prepara(void)
{
	ACPI_BUFFER b = { sizeof estado.ec_ruta, estado.ec_ruta };
	ACPI_STATUS s;

	AcpiGetDevices("PNP0C09", ec_hallado, NULL, NULL);
	if (!ec) {
		reg("EC: el equipo no declara controlador embebido (PNP0C09)\n");
		return 0;
	}
	AcpiGetName(ec, ACPI_FULL_PATHNAME, &b);
	ec_puertos();
	estado.ec_hay = 1;
	estado.ec_datos = p_datos;
	estado.ec_orden = p_orden;
	reg("EC: %s, datos en %02Xh, órdenes y estado en %02Xh (estado ahora: %02Xh)\n", estado.ec_ruta,
	    p_datos, p_orden, mq_es_lee(p_orden, 1));
	s = AcpiInstallAddressSpaceHandlerNo_Reg(ec, ACPI_ADR_SPACE_EC, ec_manejador, NULL, NULL);
	if (ACPI_FAILURE(s)) {
		reg("EC: no se puede poner el manejador de la región: %s\n", AcpiFormatException(s));
		return -1;
	}
	return 1;
}

void ec_reg(int conecta)
{
	ACPI_OBJECT a[2];
	ACPI_OBJECT_LIST l = { 2, a };
	ACPI_STATUS s;

	if (!ec)
		return;
	if (conecta) {
		s = AcpiExecuteRegMethods(ec, ACPI_ADR_SPACE_EC);
		reg("EC: _REG (3, 1): %s\n", AcpiFormatException(s));
		return;
	}
	a[0].Type = a[1].Type = ACPI_TYPE_INTEGER;
	a[0].Integer.Value = ACPI_ADR_SPACE_EC;
	a[1].Integer.Value = 0;
	s = AcpiEvaluateObject(ec, "_REG", &l, NULL);
	reg("EC: _REG (3, 0): %s\n", AcpiFormatException(s));
}

/* los avisos: mientras el controlador diga que tiene uno, se le pregunta
 * cuál y se ejecuta su método _Qxx (así el equipo se entera de que la
 * batería ha cambiado: en los HP, sin esto, _BST repite lo último) */
void ec_eventos(void)
{
	int i, q;
	char m[8];
	ACPI_STATUS s;

	if (!ec || (ini.opciones & LL_OP_SINEVENTOS))
		return;
	for (i = 0; i < 8 && (mq_es_lee(p_orden, 1) & EC_SCI); i++) {
		q = orden(EC_AVISO, -1, 0);
		if (q <= 0)
			break;
		estado.ec_eventos++;
		snprintf(m, sizeof m, "_Q%02X", q);
		s = AcpiEvaluateObject(ec, m, NULL, NULL);
		if (estado.ec_eventos <= 40)
			reg("EC: aviso %02Xh, %s: %s\n", q, m, s == AE_NOT_FOUND ? "el equipo no lo usa" : AcpiFormatException(s));
		AcpiOsWaitEventsComplete();
	}
}
