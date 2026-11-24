/*
 * iwl_analiza.c - En el anfitrión: lee un firmware de Intel (.ucode) con
 * wifi/nucleo/iwl_fw.c, el mismo lector que lleva WIFI.BIN, y dice lo que
 * trae (las imágenes, sus secciones, la calibración, las capacidades). Sin
 * archivos, se prueba a sí mismo con firmwares hechos a mano (bien, cortado,
 * sin magia, con un TLV que se sale): lo de castel/pruebas.sh (SOLO=wifi).
 *
 *   build/iwl_analiza                 las pruebas (… N bien, 0 fallos)
 *   build/iwl_analiza archivo.ucode   lo que trae
 *
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "iwl_fw.h"

static const char *const nom_img[3] = { "RT (la de trabajo)", "INIT (la de calibrar)", "WOWLAN" };

/* los bits que importan para el controlador (fw/file.h) */
static const struct { int b; const char *n; } capas[] = {
	{ 1, "LAR_SUPPORT" }, { 2, "UMAC_SCAN" }, { 3, "BEAMFORMER" }, { 6, "TDLS_SUPPORT" },
	{ 12, "DQA_SUPPORT" }, { 21, "CSUM_SUPPORT" }, { 22, "RADIO_BEACON_STATS" }, { 29, "LAR_MULTI_MCC" },
	{ 30, "BT_COEX_RRC" }, { 43, "TLC_OFFLOAD" }, { 44, "DYNAMIC_QUOTA" }, { 64, "EXTENDED_DTS_MEASURE" },
	{ -1, NULL } };
static const struct { int b; const char *n; } apis[] = {
	{ 8, "FRAGMENTED_SCAN" }, { 9, "WIFI_MCC_UPDATE" }, { 18, "LQ_SS_PARAMS" }, { 20, "NEW_VERSION" },
	{ 28, "SCAN_TSF_REPORT" }, { 29, "TKIP_MIC_KEYS" }, { 30, "STA_TYPE" }, { 31, "NAN2_VER2" },
	{ 32, "ADAPTIVE_DWELL" }, { 33, "OCE" }, { 34, "NEW_BEACON_TEMPLATE" }, { 35, "NEW_RX_STATS" },
	{ 42, "ADAPTIVE_DWELL_V2" }, { 50, "SCAN_OFFLOAD_CHANS" }, { 58, "SCAN_EXT_CHAN_VER" }, { -1, NULL } };

static void resumen(const struct iwl_fw *fw)
{
	int i, k;

	printf("Firmware: «%s»\n", fw->texto);
	if (iwl_fw_api(fw, IWL_API_NEW_VERSION) || fw->ver < 0x100)
		printf("Versión %u (build %u)", fw->ver, fw->build);
	else
		printf("Versión %u.%u.%u.%u (build %u)", fw->ver >> 24, (fw->ver >> 16) & 0xff, (fw->ver >> 8) & 0xff,
		       fw->ver & 0xff, fw->build);
	if (fw->mayor || fw->menor)
		printf(", FW_VERSION %u.%08x.%u", fw->mayor, fw->menor, fw->local);
	printf("\n%d TLV (%d de tipos que no se miran)\n", fw->n_tlv, fw->tlv_sin_saber);
	for (i = 0; i < 3; i++) {
		const struct iwl_imagen *im = &fw->img[i];
		if (!im->n)
			continue;
		printf("Imagen %s: %d secciones%s, %u octetos que cargar", nom_img[i], im->n,
		       im->segura ? " (seguras, estilo 8000)" : "", iwl_fw_octetos(im));
		if (im->hay_calib)
			printf("; calibración: flujo %08X, evento %08X", im->flujo, im->evento);
		printf("\n");
		for (k = 0; k < im->n; k++) {
			d32 dd = im->sec[k].destino;
			if (dd == IWL_SEP_CPU1_CPU2)
				printf("  %2d: (separador: lo que sigue es de la CPU2)\n", k);
			else if (dd == IWL_SEP_PAGINACION)
				printf("  %2d: (separador: lo que sigue es la memoria paginada)\n", k);
			else
				printf("  %2d: a %08X, %7u octetos\n", k, dd, im->sec[k].tam);
		}
	}
	if (fw->hay_phy_sku)
		printf("PHY_SKU %08X (tipo de radio %u, paso %u, guion %u; antenas TX %X, RX %X)\n", fw->phy_sku,
		       fw->phy_sku & 3, (fw->phy_sku >> 2) & 3, (fw->phy_sku >> 4) & 3,
		       (fw->phy_sku >> 16) & 0xf, (fw->phy_sku >> 20) & 0xf);
	printf("CPU: %u; canales por escaneo: %u; flags %08X; paginación %u\n", fw->n_cpu, fw->n_canales_escaneo,
	       fw->flags, fw->paginacion);
	printf("API %08X %08X %08X %08X:", fw->api[0], fw->api[1], fw->api[2], fw->api[3]);
	for (i = 0; apis[i].n; i++)
		if (iwl_fw_api(fw, apis[i].b))
			printf(" %s", apis[i].n);
	printf("\nCapacidades %08X %08X %08X %08X:", fw->capa[0], fw->capa[1], fw->capa[2], fw->capa[3]);
	for (i = 0; capas[i].n; i++)
		if (iwl_fw_capa(fw, capas[i].b))
			printf(" %s", capas[i].n);
	printf("\n");
	if (fw->n_vo)
		printf("Versiones de %d órdenes (TLV 48)\n", fw->n_vo);
	if (fw->iml)
		printf("IML: %u octetos (gen2: el cargador de la imagen)\n", fw->iml_tam);
	printf("Escaneo: %s\n", iwl_fw_capa(fw, IWL_CAPA_UMAC_SCAN) ? "UMAC (SCAN_REQ_UMAC)" : "LMAC (SCAN_OFFLOAD_REQUEST)");
}

/* ------------------------------------------------- las pruebas a mano */
static int bien, mal;
static void mira(int c, const char *q)
{
	if (c)
		bien++;
	else {
		mal++;
		printf("FALLO: %s\n", q);
	}
}

static o8 buf[4096];
static int nb;
static void pon32(d32 v) { buf[nb++] = v; buf[nb++] = v >> 8; buf[nb++] = v >> 16; buf[nb++] = v >> 24; }
static void tlv(d32 t, const void *d, d32 l)
{
	pon32(t);
	pon32(l);
	memcpy(buf + nb, d, l);
	nb += l;
	while (nb & 3)
		buf[nb++] = 0xEE;
}
static void cabecera(void)
{
	nb = 0;
	pon32(0);
	pon32(IWL_MAGIA);
	memset(buf + nb, 0, 64);
	strcpy((char *)buf + nb, "Prueba de Hispa 1.2.3.4");
	nb += 64;
	pon32(0x01020304);
	pon32(77);
	pon32(0);
	pon32(0);
}

static void pruebas(void)
{
	struct iwl_fw *fw = calloc(1, sizeof(*fw));
	char e[120];
	o8 s[16];
	d32 v[3];

	/* uno bien: dos secciones de RT (con un separador), una de INIT, calibración, PHY_SKU, capacidades */
	cabecera();
	memcpy(s, "\x00\x00\x00\x00" "ABCDE", 9);
	tlv(IWL_TLV_SEC_RT, s, 9);
	memcpy(s, "\xCC\xCC\xFF\xFF", 4);
	tlv(IWL_TLV_SEC_RT, s, 4);
	memcpy(s, "\x00\x00\x80\x00" "12345678", 12);
	tlv(IWL_TLV_SEC_RT, s, 12);
	memcpy(s, "\x00\x00\x40\x00" "xy", 6);
	tlv(IWL_TLV_SEC_INIT, s, 6);
	v[0] = 1; v[1] = 0x1F; v[2] = 0x2F;
	tlv(IWL_TLV_DEF_CALIB, v, 12);
	v[0] = 0x00330001;
	tlv(IWL_TLV_PHY_SKU, v, 4);
	v[0] = 0; v[1] = (1 << 2) | (1 << 12);
	tlv(IWL_TLV_ENABLED_CAPABILITIES, v, 8);
	v[0] = 1; v[1] = 1 << 8;
	tlv(IWL_TLV_ENABLED_CAPABILITIES, v, 8);
	v[0] = 0x55;
	tlv(9999, v, 3);			/* uno que no se sabe: se salta, con su relleno */
	memcpy(s, "\x08\x00\x02\x01" "\x0d\x01\x0c\x02", 8);
	tlv(IWL_TLV_CMD_VERSIONS, s, 8);
	mira(iwl_fw_lee(buf, nb, fw, e, sizeof e) == 0, "uno bien hecho se lee");
	mira(strcmp(fw->texto, "Prueba de Hispa 1.2.3.4") == 0 && fw->ver == 0x01020304 && fw->build == 77, "la cabecera");
	mira(fw->img[IWL_IMG_RT].n == 3 && fw->img[IWL_IMG_INIT].n == 1, "las secciones van a su imagen");
	mira(fw->img[IWL_IMG_RT].sec[0].tam == 5 && memcmp(fw->img[IWL_IMG_RT].sec[0].datos, "ABCDE", 5) == 0, "la primera sección, con sus datos");
	mira(fw->img[IWL_IMG_RT].sec[1].destino == IWL_SEP_CPU1_CPU2, "el separador de la CPU2");
	mira(fw->img[IWL_IMG_RT].sec[2].destino == 0x800000 && fw->img[IWL_IMG_RT].sec[2].tam == 8, "la tercera, detrás del separador");
	mira(iwl_fw_octetos(&fw->img[IWL_IMG_RT]) == 13, "los octetos que cargar (sin el separador)");
	mira(fw->img[IWL_IMG_INIT].hay_calib && fw->img[IWL_IMG_INIT].flujo == 0x1F && fw->img[IWL_IMG_INIT].evento == 0x2F, "la calibración de INIT");
	mira(fw->hay_phy_sku && fw->phy_sku == 0x00330001, "PHY_SKU");
	mira(iwl_fw_capa(fw, IWL_CAPA_UMAC_SCAN) && iwl_fw_capa(fw, IWL_CAPA_DQA) && iwl_fw_capa(fw, 40) && !iwl_fw_capa(fw, IWL_CAPA_LAR), "las capacidades, de 32 en 32");
	mira(fw->tlv_sin_saber == 1, "el TLV que no se sabe, saltado");
	mira(iwl_fw_ver_orden(fw, 0, 8, 0) == 2 && iwl_fw_ver_orden(fw, 1, 0x0d, 0) == 12 && iwl_fw_ver_orden(fw, 0, 9, 7) == 7, "las versiones de las órdenes (TLV 48)");
	/* cortado a la mitad de un TLV */
	mira(iwl_fw_lee(buf, nb - 6, fw, e, sizeof e) < 0 && strstr(e, "cortado"), "cortado: se dice");
	/* sin la magia */
	buf[4] ^= 1;
	mira(iwl_fw_lee(buf, nb, fw, e, sizeof e) < 0 && strstr(e, "magia"), "sin la magia: no es un firmware");
	buf[4] ^= 1;
	/* un largo enorme (que daría la vuelta) */
	cabecera();
	pon32(IWL_TLV_SEC_RT);
	pon32(0xFFFFFFF8);
	pon32(0);
	mira(iwl_fw_lee(buf, nb, fw, e, sizeof e) < 0, "un largo que se sale del archivo");
	/* sin imagen RT */
	cabecera();
	v[0] = 1;
	tlv(IWL_TLV_NUM_OF_CPU, v, 4);
	mira(iwl_fw_lee(buf, nb, fw, e, sizeof e) < 0 && strstr(e, "RT"), "sin imagen de trabajo");
	/* demasiadas secciones: no se escribe fuera de la tabla */
	{
		int i;
		cabecera();
		for (i = 0; i < IWL_MAX_SEC + 3; i++)
			tlv(IWL_TLV_SEC_RT, "\0\0\0\0z", 5);
		mira(iwl_fw_lee(buf, nb, fw, e, sizeof e) < 0 && strstr(e, "demasiadas"), "demasiadas secciones");
	}
	/* una sección de menos de 4 octetos */
	cabecera();
	tlv(IWL_TLV_SEC_RT, "\0\0", 2);
	mira(iwl_fw_lee(buf, nb, fw, e, sizeof e) < 0, "una sección sin dirección");
	free(fw);
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		pruebas();
		printf("iwl_analiza: el lector del firmware de Intel: %d bien, %d fallos\n", bien, mal);
		return mal != 0;
	}
	for (int i = 1; i < argc; i++) {
		FILE *f = fopen(argv[i], "rb");
		long n;
		o8 *d;
		struct iwl_fw *fw = calloc(1, sizeof(*fw));
		char e[120];
		if (!f) {
			printf("%s: no se puede abrir\n", argv[i]);
			return 1;
		}
		fseek(f, 0, SEEK_END);
		n = ftell(f);
		fseek(f, 0, SEEK_SET);
		d = malloc(n);
		if (fread(d, 1, n, f) != (size_t)n)
			return 1;
		fclose(f);
		printf("== %s (%ld octetos)\n", argv[i], n);
		if (iwl_fw_lee(d, n, fw, e, sizeof e) < 0) {
			printf("No vale: %s\n", e);
			return 1;
		}
		resumen(fw);
	}
	return 0;
}
