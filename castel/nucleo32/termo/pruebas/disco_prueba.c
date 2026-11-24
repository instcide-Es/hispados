/* disco_prueba.c - (TERMO) la prueba en el anfitrión de lo que entiende
 * disco.c de los discos: con datos simulados (IDENTIFY, SMART y sus
 * umbrales, el registro de errores, el Identify y el registro de salud de
 * NVMe, el desgaste del eMMC y unas latencias de la prueba de lectura),
 * mira la salud (BIEN / ATENCIÓN / MAL), el porqué, el informe y el
 * veredicto. Cada línea «bien ...» o «MAL ...»; sale con 1 si alguna falla.
 *   gcc -DDI_ANFITRION -I.. disco_prueba.c ../disco.c -o disco_prueba
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "disco.h"

static int fallos;

static void mira_(const char *que, int ok, const char *dicho)
{
	printf("%s %s%s%s\n", ok ? "bien" : "MAL", que, ok ? "" : ": ", ok ? "" : dicho);
	if (!ok)
		fallos++;
}

static void pon_ata(unsigned char *id, int palabra, int palabras, const char *t)
{
	int i, n = (int)strlen(t);

	for (i = 0; i < palabras * 2; i++)
		id[palabra * 2 + (i ^ 1)] = (unsigned char)(i < n ? t[i] : ' ');
}

static void pon16(unsigned char *id, int palabra, unsigned v)
{
	id[palabra * 2] = (unsigned char)v;
	id[palabra * 2 + 1] = (unsigned char)(v >> 8);
}

static void atrib(unsigned char *d, unsigned char *u, int k, int id, int valor, int peor, unsigned long long crudo, int umbral)
{
	unsigned char *a = d + 2 + k * 12;
	int j;

	a[0] = (unsigned char)id;
	a[1] = 0x33;
	a[3] = (unsigned char)valor;
	a[4] = (unsigned char)peor;
	for (j = 0; j < 6; j++)
		a[5 + j] = (unsigned char)(crudo >> (8 * j));
	u[2 + k * 12] = (unsigned char)id;
	u[2 + k * 12 + 1] = (unsigned char)umbral;
}

static void identify(unsigned char *id, const char *modelo, const char *serie, int ssd)
{
	memset(id, 0, 512);
	pon_ata(id, 27, 20, modelo);
	pon_ata(id, 10, 10, serie);
	pon_ata(id, 23, 4, "2BC10007");
	pon16(id, 82, 0x7469);	/* SMART */
	pon16(id, 85, 0x7469);	/* encendido */
	pon16(id, 217, ssd ? 1 : 5400);
}

#define mira(q, c, d) mira_((q), (c) ? 1 : 0, (d))

static unsigned lat[8192];

int main(void)
{
	static struct di_disco D[4];
	static unsigned char id[4096], dat[512], umb[512], reg[512];
	static char t[12000], v[800];
	const struct di_atrib *a;
	int i;

	/* 1. un disco duro sano (horas 9: 12345; encendidos 12: 678; 194: 38 °C con la mínima/máxima en los octetos altos) */
	di_empieza(&D[0], DI_SATA, 976773168u, 512, 0, "ST1000LM024 HN-M101MBB");
	identify(id, "ST1000LM024 HN-M101MBB", "    S2ZTJ9AF123456", 0);
	di_identify_ata(&D[0], id);
	memset(dat, 0, sizeof dat), memset(umb, 0, sizeof umb);
	atrib(dat, umb, 0, 1, 100, 100, 0, 51);
	atrib(dat, umb, 1, 5, 252, 252, 0, 10);
	atrib(dat, umb, 2, 9, 99, 99, 12345, 0);
	atrib(dat, umb, 3, 12, 99, 99, 678, 0);
	atrib(dat, umb, 4, 194, 64, 50, 0x0032001A0026ull, 0);
	atrib(dat, umb, 5, 197, 252, 252, 0, 0);
	atrib(dat, umb, 6, 199, 200, 200, 0, 0);
	di_smart_ata(&D[0], dat, umb);
	D[0].estado = 0;
	memset(reg, 0, sizeof reg);
	di_errores_ata(&D[0], reg);
	for (i = 0; i < 4096; i++)
		lat[i] = 3000 + (unsigned)(i % 50) * 100;	/* 3 a 8 ms */
	D[0].lee.hecha = 1, D[0].lee.mb_saltos = 256, D[0].lee.us_saltos = 4000000, D[0].lee.mb_seguido = 256, D[0].lee.us_seguido = 2560000;
	di_lectura_cuenta(&D[0].lee, lat, 4096);
	di_juzga(&D[0]);
	mira("disco duro sano: BIEN", D[0].salud == DI_BIEN, D[0].porque);
	mira("el número de serie, solo las 4 últimas letras (3456)", !strcmp(D[0].serie, "3456"), D[0].serie);
	mira("el modelo y el firmware de IDENTIFY", !strcmp(D[0].modelo, "ST1000LM024 HN-M101MBB") && !strcmp(D[0].firmware, "2BC10007"), D[0].modelo);
	mira("5400 r.p.m. (IDENTIFY 217), no SSD", D[0].rpm == 5400 && !D[0].ssd, "");
	a = di_atributo(&D[0], 9);
	mira("atributo 9: 12345 horas; umbral del 1: 51", a && a->crudo == 12345 && di_atributo(&D[0], 1)->umbral == 51, "");
	mira("latencias: media de unos 5,4 ms, p99 7,9 ms, la peor 7,9 ms", D[0].lee.media_us > 5400 && D[0].lee.media_us < 5500 && D[0].lee.p99_us == 7900 && D[0].lee.max_us == 7900, "");

	/* 2. el de los parones: 12 pendientes, 3 reasignados, CRC 2, atascos en la lectura */
	di_empieza(&D[1], DI_SATA, 625142448u, 512, 0, "WDC WD3200BEVT-22A23T0");
	identify(id, "WDC WD3200BEVT-22A23T0", "WD-WXE1A60K1234", 0);
	di_identify_ata(&D[1], id);
	memset(dat, 0, sizeof dat), memset(umb, 0, sizeof umb);
	atrib(dat, umb, 0, 5, 199, 199, 3, 140);
	atrib(dat, umb, 1, 9, 50, 50, 37000, 0);
	atrib(dat, umb, 2, 197, 200, 200, 12, 0);
	atrib(dat, umb, 3, 198, 100, 100, 0, 0);
	atrib(dat, umb, 4, 199, 200, 200, 2, 0);
	di_smart_ata(&D[1], dat, umb);
	D[1].estado = 0;
	memset(reg, 0, sizeof reg);
	reg[0] = 1, reg[452] = 27;
	di_errores_ata(&D[1], reg);
	D[1].lee.hecha = 1, D[1].lee.lentas = 2, D[1].lee.reintentos = 1, D[1].lee.max_us = 2300000;
	di_juzga(&D[1]);
	mira("12 pendientes: MAL", D[1].salud == DI_MAL, di_salud_texto(D[1].salud));
	mira("el porqué, en palabras: pendientes, reasignados, atascos, errores apuntados",
	     strstr(D[1].porque, "12 sectores pendientes") && strstr(D[1].porque, "3 sectores reasignados") && strstr(D[1].porque, "se ha atascado 2 veces") &&
	     strstr(D[1].porque, "27 errores en su vida") && strstr(D[1].porque, "CRC"), D[1].porque);
	di_veredicto(D, 2, 1, v, sizeof v);
	mira("el veredicto, con parones: «el disco tiene 12 sectores pendientes: puede ser la causa de los parones»",
	     strstr(v, "tiene 12 sectores pendientes: puede ser la causa de los parones") && strstr(v, "disco 2") && strstr(v, "copia"), v);
	di_veredicto(D, 2, 0, v, sizeof v);
	mira("el veredicto, sin parones: sin la causa de los parones", strstr(v, "12 sectores pendientes") && !strstr(v, "causa de los parones"), v);

	/* 3. un SSD gastado (231: le queda un 8 %) y con el umbral pasado */
	di_empieza(&D[2], DI_SATA, 250069680u, 512, 0, "");
	identify(id, "KINGSTON SA400S37120G", "50026B7782A1B2C3", 1);
	di_identify_ata(&D[2], id);
	memset(dat, 0, sizeof dat), memset(umb, 0, sizeof umb);
	atrib(dat, umb, 0, 231, 8, 8, 8, 10);
	atrib(dat, umb, 1, 9, 100, 100, 20000, 0);
	di_smart_ata(&D[2], dat, umb);
	D[2].estado = 1;
	di_juzga(&D[2]);
	mira("SSD gastado (231 = 8 %, umbral pasado, RETURN STATUS dice que falla): MAL", D[2].salud == DI_MAL && D[2].ssd &&
	     strstr(D[2].porque, "le queda un 8 % de vida") && strstr(D[2].porque, "está fallando"), D[2].porque);
	mira("el modelo sale de IDENTIFY si no lo había", !strcmp(D[2].modelo, "KINGSTON SA400S37120G"), D[2].modelo);

	/* 4. sin SMART */
	{
		struct di_disco x;

		di_empieza(&x, DI_IDE, 2000000, 512, 0, "QEMU HARDDISK");
		identify(id, "QEMU HARDDISK", "QM00001", 0);
		pon16(id, 82, 0x0000);
		di_identify_ata(&x, id);
		x.lee.hecha = 1;
		di_juzga(&x);
		mira("sin SMART: lo dice, y con la lectura bien, BIEN", x.salud == DI_BIEN && strstr(x.porque, "no tiene SMART"), x.porque);
		identify(id, "QEMU HARDDISK", "QM00001", 0);
		pon16(id, 85, 0x0000);
		di_empieza(&x, DI_IDE, 2000000, 512, 0, "QEMU HARDDISK");
		di_identify_ata(&x, id);
		di_juzga(&x);
		mira("SMART apagado: lo dice (y no se enciende), NO SE SABE sin lectura", x.salud == DI_NOSE && strstr(x.porque, "SMART apagado"), x.porque);
		x.lee.hecha = 1, x.lee.errores = 3, x.lee.err_mb[0] = 100;
		di_juzga(&x);
		mira("errores de lectura: MAL", x.salud == DI_MAL && strstr(x.porque, "3 trozos no se han podido leer"), x.porque);
	}

	/* 5. NVMe: sano y luego con aviso crítico */
	di_empieza(&D[3], DI_NVME, 1000215216u, 512, 0, "");
	memset(id, 0, sizeof id);
	memcpy(id + 4, "S4EWNX0R123456      ", 20);
	memcpy(id + 24, "Samsung SSD 970 EVO Plus 500GB          ", 40);
	memcpy(id + 64, "2B2QEXM7", 8);
	di_identify_nvme(&D[3], id);
	memset(reg, 0, sizeof reg);
	reg[1] = (unsigned char)(310 & 0xFF), reg[2] = (unsigned char)(310 >> 8);	/* 37 °C */
	reg[3] = 100, reg[4] = 10, reg[5] = 3;
	reg[112] = 200;		/* encendidos */
	reg[128] = 0x10, reg[129] = 0x27;	/* 10000 horas */
	reg[144] = 15;		/* apagados sin avisar */
	reg[32] = 0x40, reg[33] = 0x42, reg[34] = 0x0F;	/* 1.000.000 unidades = 512 GB leídos */
	di_salud_nvme(&D[3], reg);
	di_juzga(&D[3]);
	mira("NVMe sano: BIEN, 37 °C, 3 % usado, 10000 horas, 512 GB leídos", D[3].salud == DI_BIEN && D[3].temp == 37 && D[3].usado == 3 && D[3].horas == 10000 && D[3].leidos_gb == 512, D[3].porque);
	mira("NVMe: modelo, serie (3456) y firmware", !strcmp(D[3].modelo, "Samsung SSD 970 EVO Plus 500GB") && !strcmp(D[3].serie, "3456") && !strcmp(D[3].firmware, "2B2QEXM7"), D[3].serie);
	{
		struct di_disco x = D[3];

		reg[0] = 0x04, reg[160] = 7, reg[5] = 85;
		di_salud_nvme(&x, reg);
		di_juzga(&x);
		mira("NVMe con aviso crítico (fiabilidad), 7 errores del medio, 85 % usado: MAL", x.salud == DI_MAL && strstr(x.porque, "ya no es fiable") && strstr(x.porque, "7 errores del medio") &&
		     strstr(x.porque, "85 %"), x.porque);
	}

	/* 6. eMMC: PRE_EOL en aviso, tipo A 90-100 % */
	{
		struct di_disco x;

		di_empieza(&x, DI_EMMC, 61071360u, 512, 0x80000000u | (8u << 24) | (2u << 16) | (3u << 8) | 10u, "BJTD4R");
		di_juzga(&x);
		mira("eMMC con PRE_EOL en aviso y el 90-100 % gastado: ATENCIÓN", x.salud == DI_ATENCION && strstr(x.porque, "80 % de su reserva") && strstr(x.porque, "90-100 %"), x.porque);
		di_empieza(&x, DI_EMMC, 61071360u, 512, 0x80000000u | (6u << 24), "OLD");
		di_juzga(&x);
		mira("eMMC de antes de la 5.0: no lo dice", x.salud == DI_NOSE && strstr(x.porque, "no dice su desgaste"), x.porque);
	}

	/* 7. el informe */
	di_informe(D, 4, 0, t, sizeof t);
	mira("el informe: la sección, cada disco con su salud en palabras", strstr(t, "DISCOS (solo se leen") && strstr(t, "Disco 1: ST1000LM024 HN-M101MBB (SATA, 500 GB)") &&
	     strstr(t, "Salud: BIEN") && strstr(t, "Salud: MAL") && strstr(t, "serie ...3456"), t);
	mira("el informe: lo principal de SMART (horas, encendidos, °C, pendientes) y de NVMe",
	     strstr(t, "12345 horas encendido; 678 encendidos; 38 °C;") && strstr(t, "pendientes 12") && strstr(t, "Salud NVMe: 37 °C; usado 3 %"), t);
	mira("el informe: la prueba de lectura en MB/s y ms", strstr(t, "256 MB repartidos por todo el disco a 64,0 MB/s y 256 MB seguidos a 100,0 MB/s") && strstr(t, "media 5 ms, p99 8 ms"), t);
	mira("el informe: los sectores escritos en los discos de dentro, 0", strstr(t, "escritos en los discos de dentro desde que arrancó la memoria: 0 (como debe ser)"), t);
	mira("el informe: ningún número de serie entero", !strstr(t, "S2ZTJ9AF") && !strstr(t, "WXE1A60K") && !strstr(t, "S4EWNX0R"), t);
	di_informe(D, 0, 0, t, sizeof t);
	mira("sin discos: lo dice (y lo del RAID de Intel)", strstr(t, "ningún disco de dentro") && strstr(t, "AHCI"), t);
	di_detalle(&D[1], t, sizeof t);
	mira("el detalle para DISCOS.LOG: todos los atributos", strstr(t, "ATRIB 197 valor 200 peor 200 umbral   0 crudo 12") && strstr(t, "SALUD MAL"), t);
	if (getenv("DI_ENSENA"))
		di_informe(D, 4, 0, t, sizeof t), fputs(t, stdout);
	return fallos ? 1 : 0;
}
