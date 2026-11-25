/* intel.c - (BLUETOOTH) los adaptadores Bluetooth de Intel (8087:xxxx), lo
 * que hay que hacer con ellos antes del reset, como Linux en lo mínimo
 * (drivers/bluetooth/btintel.c de Linux 6.6: btintel_setup_combined,
 * btintel_legacy_rom_setup, btintel_bootloader_setup y lo que llaman; y
 * btusb.c: btusb_send_frame_intel, btusb_recv_bulk_intel).
 *
 *  - Se pregunta su versión (la orden de Intel 0xFC05, con la forma de
 *    antes: es la que valen todos hasta los de la familia 0x14, CcP).
 *  - Los de ROM (familias 0x07 WP y 0x08 StP: el 8087:0A2A de la 7265 del
 *    ASUS GL553VD) funcionan sin nada; si está el parche
 *    (ibt-hw-*.bseq), se pone con el modo de fabricante (0xFC11); si no,
 *    se sigue sin él, como Linux.
 *  - Los de cargador (0x0B SfP, 0x0C WsP, 0x11 JfP —el 8087:0AAA del
 *    Monovate—, 0x12 ThP, 0x13 HrP —el 8087:0026 de la AX201 del ThinkPad
 *    E14—, 0x14 CcP) arrancan en su cargador (fw_variant 0x06) y hay que
 *    darles su firmware (ibt-<familia>-<rev>-<rev>.sfi: el de Intel, que se
 *    puede repartir sin cambios pero no es libre, así que NO va con el
 *    sistema: se busca en \HISPA\FIRMWARE y \FIRMWARE, y si no está se dice
 *    dónde se ha mirado y de dónde bajarlo). Se manda por «secure send»
 *    (0xFC09, en trozos de 252, por el extremo a granel mientras está en el
 *    cargador), se espera a que diga que está bien (el suceso del
 *    fabricante 0x06), se le hace arrancar (0xFC01, con la dirección de
 *    arranque que trae el propio firmware en su orden 0xFC0E) y se espera a
 *    que avise (el 0x02). Luego, si está, su configuración (.ddc, 0xFC8B).
 *    Si ya lo tenía puesto (fw_variant 0x23: un reinicio en caliente desde
 *    otro sistema), no hace falta.
 *  - Los de después (AX200 y siguientes, con la versión en TLV: 8087:0029,
 *    0032, 0033, 0035...) aún no: se dice.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "bt.h"

#define FW_MAX (1536 * 1024)
static u8 fw[FW_MAX];
static int resultado = -1, arrancado;

static u16 le16(const u8 *p) { return p[0] | (p[1] << 8); }
static u32 le32(const u8 *p) { return p[0] | (p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24); }

void intel_suceso(const u8 *p, int n)
{
	if (n < 1)
		return;
	if (p[0] == 0x06 && n >= 5) {
		/* el resultado de cargar el firmware (btintel_secure_send_result) */
		resultado = p[1] ? 0x100 | p[1] : 0;
		bt_log("Intel: el firmware %s (resultado %02X, orden %04X, estado %02X)\n", p[1] ? "NO se ha cargado bien" : "se ha cargado",
		       p[1], le16(p + 2), p[4]);
	} else if (p[0] == 0x02) {
		arrancado = 1;
		bt_log("Intel: ha arrancado su firmware\n");
	}
}

/* espera, recogiendo lo que llegue, a que «*v» cambie (hasta «tics») */
static int espera_a(volatile int *v, int como_estaba, u32 tics)
{
	u32 t0 = bt_tics();

	while (*v == como_estaba && bt_tics() - t0 < tics)
		hci_recoge(0x80000000u | 2);
	return *v != como_estaba;
}

static int secure_send(u8 tipo, const u8 *d, u32 n)
{
	u8 p[253], r[8];

	while (n > 0) {
		u32 k = n > 252 ? 252 : n;
		p[0] = tipo;
		memcpy(p + 1, d, k);
		if (hci_espera(0xFC09, p, k + 1, r, sizeof r, 500) < 1 || r[0]) {
			bt_log("Intel: al mandar el firmware, la orden 0xFC09 no ha salido\n");
			return -1;
		}
		d += k, n -= k;
	}
	return 0;
}

static const char *familia(int v)
{
	switch (v) {
	case 0x07: return "WP (Wireless 7260)";
	case 0x08: return "StP (Wireless 7265/3165/3168)";
	case 0x0B: return "SfP (Wireless 8260)";
	case 0x0C: return "WsP (Wireless 8265)";
	case 0x11: return "JfP (Wireless 9460/9560)";
	case 0x12: return "ThP (Wireless 9160/9260)";
	case 0x13: return "HrP (Wi-Fi 6 AX201/AX101)";
	case 0x14: return "CcP (Wi-Fi 6 AX200)";
	}
	return "desconocida";
}

/* ---- los de ROM: el parche (btintel_legacy_rom_setup y _patching) ---- */
static int parchea(u32 n)
{
	u8 r[260], p[2];
	u32 i = 0;
	int quita = 1, k;

	p[0] = 1, p[1] = 0;			/* el modo de fabricante */
	if (hci_espera(0xFC11, p, 2, r, sizeof r, 300) < 1 || r[0]) {
		bt_log("Intel: no entra en el modo de fabricante: sin parche\n");
		return -1;
	}
	while (i < n) {
		const u8 *orden, *ev = 0;
		u16 op;
		int pl, el = 0;
		if (fw[i] != 0x01 || i + 4 > n) {
			bt_log("Intel: el parche está mal (en %u)\n", i);
			goto mal;
		}
		op = le16(fw + i + 1), pl = fw[i + 3];
		orden = fw + i + 4;
		i += 4 + pl;
		if (op == 0xFC8E)
			quita = 0;
		while (i + 3 <= n && fw[i] == 0x02) {
			el = fw[i + 2];
			ev = fw + i + 3;
			i += 3 + el;
		}
		if (!ev || i > n) {
			bt_log("Intel: el parche está mal (sin su suceso)\n");
			goto mal;
		}
		k = hci_espera(op, orden, pl, r, sizeof r, 300);
		/* (lo que se espera es el Command Complete entero: el número de
		 * órdenes, el código y lo que da; aquí solo llega lo que da) */
		if (k < 0 || (el >= 3 && (k != el - 3 || memcmp(r, ev + 3, k)))) {
			bt_log("Intel: la orden %04X del parche no contesta lo que dice el parche\n", op);
			goto mal;
		}
	}
	if (quita) {
		p[0] = 0, p[1] = 0;
		hci_espera(0xFC11, p, 2, r, sizeof r, 300);
		bt_log("Intel: el parche no hace falta (lo dice él)\n");
		return 0;
	}
	p[0] = 0, p[1] = 2;			/* fuera del modo, con reset y el parche puesto */
	hci_espera(0xFC11, p, 2, r, sizeof r, 500);
	bt_log("Intel: parche puesto\n");
	return 0;
mal:
	p[0] = 0, p[1] = 1;			/* fuera, con reset, sin el parche */
	hci_espera(0xFC11, p, 2, r, sizeof r, 500);
	return -1;
}

/* ---- los de cargador ---- */
static void ddc(const char *nombre)
{
	int n = bt_firmware(nombre, fw, FW_MAX), i = 0, k = 0;
	u8 r[8];

	if (n < 0) {
		bt_log("Intel: sin %s (su configuración; funciona sin ella)\n", nombre);
		return;
	}
	while (i < n && i + fw[i] + 1 <= n) {
		if (hci_espera(0xFC8B, fw + i, fw[i] + 1, r, sizeof r, 300) < 1 || r[0]) {
			bt_log("Intel: la configuración de %s no entra (en el octeto %u)\n", nombre, i);
			return;
		}
		i += fw[i] + 1, k++;
	}
	bt_log("Intel: configuración puesta (%s, %u partes)\n", nombre, k);
}

static int cargador(const u8 *v)
{
	u8 r[64], p[8];
	char nombre[48], ddcn[48];
	int n, k;
	u32 i, arranque = 0;

	if (v[4] == 0x23) {
		bt_log("Intel: ya tiene puesto su firmware (versión %u.%u, %u-%u)\n", v[5], v[6], v[7], 2000 + v[8]);
		bt.intel_cargador = 0;
		return 0;
	}
	if (v[4] != 0x06) {
		bt_log("Intel: su firmware es de una clase que no se conoce (%02X)\n", v[4]);
		return -1;
	}
	if ((k = hci_espera(0xFC0D, 0, 0, r, sizeof r, 300)) < 7 || r[0]) {
		bt_log("Intel: no da sus parámetros de arranque\n");
		return -1;
	}
	bt_log("Intel: en su cargador; revisión %u, arranque seguro %s, CCE %s\n", le16(r + 4), r[6] ? "sí" : "no", k > 21 && r[21] ? "limitado" : "normal");
	if (k > 21 && r[21]) {
		bt_log("Intel: con el CCE limitado no se puede cargar el firmware\n");
		return -1;
	}
	if (v[2] == 0x0B || v[2] == 0x0C) {
		formatear(nombre, sizeof nombre, "ibt-%u-%u.sfi", v[2], le16(r + 4));
		formatear(ddcn, sizeof ddcn, "ibt-%u-%u.ddc", v[2], le16(r + 4));
	} else {
		formatear(nombre, sizeof nombre, "ibt-%u-%u-%u.sfi", v[2], v[3], v[5]);
		formatear(ddcn, sizeof ddcn, "ibt-%u-%u-%u.ddc", v[2], v[3], v[5]);
	}
	n = bt_firmware(nombre, fw, FW_MAX);
	if (n < 0) {
		bt_log("Intel: hace falta su firmware, %s (de linux-firmware: "
		       "https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git/tree/intel ; "
		       "con su licencia, LICENCE.ibt_firmware), en una carpeta FIRMWARE de la memoria USB o en \\HISPA\\FIRMWARE\n", nombre);
		formatear(bt.aviso, sizeof bt.aviso, "Falta el firmware de Intel %s (lo dice BT.LOG)", nombre);
		return -1;
	}
	if (n < 644 + 4) {
		bt_log("Intel: %s es demasiado corto (%u octetos)\n", nombre, n);
		return -1;
	}
	/* la dirección de arranque, de su orden 0xFC0E (btintel_firmware_version) */
	for (i = 644; i + 3 <= (u32)n; i += 3 + fw[i + 2])
		if (le16(fw + i) == 0xFC0E && fw[i + 2] >= 4)
			arranque = le32(fw + i + 3);
	bt_log("Intel: cargando %s (%u K; arranca en %08X)\n", nombre, n / 1024, arranque);
	bt.intel_cargador = 2;
	resultado = -1;
	/* la cabecera CSS (128), la llave (256) y la firma (256): btintel_sfi_rsa_header_secure_send */
	if (secure_send(0x00, fw, 128) || secure_send(0x03, fw + 128, 256) || secure_send(0x02, fw + 388, 256))
		return -1;
	/* y lo demás, de varias órdenes en varias, con el largo múltiplo de 4
	 * (el firmware trae sus «Intel_NOP» para eso): btintel_download_firmware_payload */
	{
		u32 desde = 644, largo = 0, dichos = 0;
		while (desde + largo < (u32)n) {
			if (desde + largo + 3 > (u32)n) {
				bt_log("Intel: %s acaba a medias\n", nombre);
				return -1;
			}
			largo += 3 + fw[desde + largo + 2];
			if (!(largo % 4)) {
				if (secure_send(0x01, fw + desde, largo))
					return -1;
				desde += largo, largo = 0;
				if ((desde - 644) / (64 * 1024) != dichos) {
					dichos = (desde - 644) / (64 * 1024);
					bt_log("Intel: %u K mandados\n", (desde - 644) / 1024);
				}
			}
		}
	}
	if (!espera_a(&resultado, -1, 500)) {
		bt_log("Intel: no dice si el firmware ha entrado bien (5 s)\n");
		return -1;
	}
	if (resultado)
		return -1;
	/* arrancarlo (btintel_send_intel_reset): no contesta con Command Complete */
	p[0] = 0x00, p[1] = 0x01, p[2] = 0x00, p[3] = 0x01;
	p[4] = arranque, p[5] = arranque >> 8, p[6] = arranque >> 16, p[7] = arranque >> 24;
	arrancado = 0;
	bt.ordenes_libres = 1;
	hci_manda(0xFC01, p, 8);
	bt.ordenes_libres = 1;
	if (!espera_a(&arrancado, 0, 500)) {
		bt_log("Intel: no avisa de que ha arrancado (5 s)\n");
		return -1;
	}
	bt.intel_cargador = 0;
	ddc(ddcn);
	return 0;
}

int intel_prepara(u16 pid)
{
	u8 v[64];
	int k;

	/* (mientras no se sepa si está en su cargador, lo que llegue por el
	 * granel son sucesos: btusb_recv_bulk_intel) */
	bt.intel_cargador = 1;
	k = hci_espera(0xFC05, 0, 0, v, sizeof v, 300);
	if (k < 10 || v[0]) {
		bt_log("Intel %04X: no da su versión (%s): se sigue como uno cualquiera\n", pid, k < 1 ? "no contesta" : "falla");
		bt.intel_cargador = 0;
		return 0;
	}
	bt_log("Intel %04X: plataforma %02X, familia %02X %s, revisión %u; firmware %02X (%s), versión %u, %u-%u, parche %u\n",
	       pid, v[1], v[2], familia(v[2]), v[3], v[4], v[4] == 0x06 ? "el cargador" : v[4] == 0x23 ? "el de funcionar" : "?",
	       v[5], v[7], 2000 + v[8], v[9]);
	if (v[1] != 0x37) {
		bt_log("Intel: no es de los que se conocen (plataforma %02X): se sigue como uno cualquiera\n", v[1]);
		bt.intel_cargador = 0;
		return 0;
	}
	switch (v[2]) {
	case 0x07:
	case 0x08:
		bt.intel_cargador = 0;
		if (v[9]) {
			bt_log("Intel: ya está parcheado (%u)\n", v[9]);
			return 0;
		}
		{
			char nombre[64];
			int n;
			formatear(nombre, sizeof nombre, "ibt-hw-%x.%x.%x-fw-%x.%x.%x.%x.%x.bseq", v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8]);
			n = bt_firmware(nombre, fw, FW_MAX);
			if (n < 0) {
				formatear(nombre, sizeof nombre, "ibt-hw-%x.%x.bseq", v[1], v[2]);
				n = bt_firmware(nombre, fw, FW_MAX);
			}
			if (n < 0) {
				bt_log("Intel: sin parche: funciona con el de su ROM (como Linux)\n");
				return 0;
			}
			bt_log("Intel: parche %s (%u octetos)\n", nombre, n);
			parchea(n);
		}
		return 0;
	case 0x0B:
	case 0x0C:
	case 0x11:
	case 0x12:
	case 0x13:
	case 0x14:
		k = cargador(v);
		bt.intel_cargador = 0;
		return k;
	}
	bt_log("Intel: la familia %02X aún no se lleva (los AX200 y siguientes piden la versión en TLV y otro firmware)\n", v[2]);
	bt.intel_cargador = 0;
	return -1;
}
