/*
 * iwl_fw.c - Leer el firmware de las Wi-Fi de Intel (iwl_fw.h).
 *
 * El archivo: una cabecera (cuatro ceros, la magia «IWL\n», 64 letras con el
 * nombre y la versión, la versión en número, el «build» y 8 octetos que no
 * se usan) y detrás una lista de TLV (tipo, largo y los datos, rellenos
 * hasta múltiplo de 4). Las secciones de código llevan delante, en 4
 * octetos, la dirección de la SRAM de la tarjeta donde van.
 *
 * No se fía de nada del archivo: cada largo se mira contra lo que queda,
 * y lo que no cabe en las tablas es un error (no se escribe fuera).
 *
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 */
#include "iwl_fw.h"

static d32 le32(const o8 *p)
{
	return p[0] | (p[1] << 8) | (p[2] << 16) | ((d32)p[3] << 24);
}

static void di(char *e, int cabe, const char *t)
{
	int i;

	if (!e || cabe <= 0)
		return;
	for (i = 0; t[i] && i < cabe - 1; i++)
		e[i] = t[i];
	e[i] = 0;
}

static int imagen_de(d32 tipo, int *segura)
{
	*segura = tipo >= IWL_TLV_SECURE_SEC_RT;
	switch (tipo) {
	case IWL_TLV_SEC_RT: case IWL_TLV_SECURE_SEC_RT: return IWL_IMG_RT;
	case IWL_TLV_SEC_INIT: case IWL_TLV_SECURE_SEC_INIT: return IWL_IMG_INIT;
	default: return IWL_IMG_WOWLAN;
	}
}

int iwl_fw_lee(const o8 *d, d32 n, struct iwl_fw *fw, char *error, int cabe)
{
	d32 p, tipo, l, i;
	int k, seg;

	memset(fw, 0, sizeof(*fw));
	fw->n_cpu = 1;
	di(error, cabe, "");
	if (n < 88 || le32(d) != 0 || le32(d + 4) != IWL_MAGIA) {
		di(error, cabe, "no es un firmware de Intel con TLV (falta la magia «IWL»)");
		return -1;
	}
	memcpy(fw->texto, d + 8, 64);
	fw->texto[64] = 0;
	for (k = 0; k < 64; k++)
		if (fw->texto[k] && ((o8)fw->texto[k] < 32 || (o8)fw->texto[k] > 126))
			fw->texto[k] = '?';
	fw->ver = le32(d + 72);
	fw->build = le32(d + 76);
	p = 88;
	while (p + 8 <= n) {
		tipo = le32(d + p);
		l = le32(d + p + 4);
		p += 8;
		if (l > n - p) {
			di(error, cabe, "un TLV dice que es más largo que lo que queda del archivo (cortado)");
			return -1;
		}
		fw->n_tlv++;
		switch (tipo) {
		case IWL_TLV_SEC_RT: case IWL_TLV_SEC_INIT: case IWL_TLV_SEC_WOWLAN:
		case IWL_TLV_SECURE_SEC_RT: case IWL_TLV_SECURE_SEC_INIT: case IWL_TLV_SECURE_SEC_WOWLAN: {
			struct iwl_imagen *im = &fw->img[imagen_de(tipo, &seg)];
			if (l < 4) {
				di(error, cabe, "una sección sin su dirección");
				return -1;
			}
			if (im->n >= IWL_MAX_SEC) {
				di(error, cabe, "demasiadas secciones en una imagen");
				return -1;
			}
			im->sec[im->n].destino = le32(d + p);
			im->sec[im->n].tam = l - 4;
			im->sec[im->n].datos = d + p + 4;
			im->n++;
			if (seg)
				im->segura = 1;
			break;
		}
		case IWL_TLV_DEF_CALIB:
			if (l >= 12) {
				d32 t = le32(d + p);
				if (t <= IWL_IMG_WOWLAN) {
					fw->img[t].flujo = le32(d + p + 4);
					fw->img[t].evento = le32(d + p + 8);
					fw->img[t].hay_calib = 1;
				}
			}
			break;
		case IWL_TLV_PHY_SKU:
			if (l >= 4) {
				fw->phy_sku = le32(d + p);
				fw->hay_phy_sku = 1;
			}
			break;
		case IWL_TLV_FLAGS:
			if (l >= 4)
				fw->flags = le32(d + p);
			break;
		case IWL_TLV_NUM_OF_CPU:
			if (l >= 4)
				fw->n_cpu = le32(d + p);
			break;
		case IWL_TLV_N_SCAN_CHANNELS:
			if (l >= 4)
				fw->n_canales_escaneo = le32(d + p);
			break;
		case IWL_TLV_PAGING:
			if (l >= 4)
				fw->paginacion = le32(d + p);
			break;
		case IWL_TLV_API_CHANGES_SET:
		case IWL_TLV_ENABLED_CAPABILITIES:
			if (l >= 8) {
				d32 idx = le32(d + p), v = le32(d + p + 4);
				if (idx < 4) {
					if (tipo == IWL_TLV_API_CHANGES_SET)
						fw->api[idx] |= v;
					else
						fw->capa[idx] |= v;
				}
			}
			break;
		case IWL_TLV_FW_VERSION:
			if (l >= 12) {
				fw->mayor = le32(d + p);
				fw->menor = le32(d + p + 4);
				fw->local = le32(d + p + 8);
			}
			break;
		case IWL_TLV_CMD_VERSIONS:
			for (i = 0; i + 4 <= l && fw->n_vo < IWL_MAX_VER_ORDEN; i += 4) {
				fw->vo[fw->n_vo].orden = d[p + i];
				fw->vo[fw->n_vo].grupo = d[p + i + 1];
				fw->vo[fw->n_vo].ver = d[p + i + 2];
				fw->vo[fw->n_vo].ver_notif = d[p + i + 3];
				fw->n_vo++;
			}
			break;
		case IWL_TLV_IML:
			fw->iml = d + p;
			fw->iml_tam = l;
			break;
		default:
			fw->tlv_sin_saber++;
			break;
		}
		p += (l + 3) & ~3u;
	}
	if (fw->img[IWL_IMG_RT].n == 0) {
		di(error, cabe, "no trae la imagen de trabajo (RT)");
		return -1;
	}
	return 0;
}

int iwl_fw_capa(const struct iwl_fw *fw, int b)
{
	return b >= 0 && b < 128 && (fw->capa[b >> 5] >> (b & 31)) & 1;
}

int iwl_fw_api(const struct iwl_fw *fw, int b)
{
	return b >= 0 && b < 128 && (fw->api[b >> 5] >> (b & 31)) & 1;
}

int iwl_fw_ver_orden(const struct iwl_fw *fw, int grupo, int orden, int si_no)
{
	int i;

	for (i = 0; i < fw->n_vo; i++)
		if (fw->vo[i].grupo == grupo && fw->vo[i].orden == orden)
			return fw->vo[i].ver;
	return si_no;
}

d32 iwl_fw_octetos(const struct iwl_imagen *im)
{
	d32 t = 0;
	int i;

	for (i = 0; i < im->n; i++)
		if (im->sec[i].destino != IWL_SEP_CPU1_CPU2 && im->sec[i].destino != IWL_SEP_PAGINACION)
			t += im->sec[i].tam;
	return t;
}
