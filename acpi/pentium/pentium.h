/* pentium.h - ACPI.BIN, el servicio de ACPI de Hispa-Pentium: las llamadas al
 * núcleo y lo que se le da (la misma estructura que Energía en
 * castel/nucleo32/energia.cas: si cambia una, la otra).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef PENTIUM_ACPI_H
#define PENTIUM_ACPI_H
#include "nucleo.h"

/* ---- las llamadas al sistema (int 0x80) ---- */
#define SC_ESCRIBIR 1
#define SC_DORMIR 3
#define SC_TICS 4
#define SC_ABRIR 7
#define SC_CERRAR 9
#define SC_ARGUMENTOS 14
#define SC_MAS_MEMORIA 17
#define SC_ESCRIBIR_ARCHIVO 18
#define SC_CORTE 29		/* Ctrl+C: lo atiende el programa (no se le acaba) */
#define SC_SERIE 61		/* solo al puerto serie */
#define SC_PUERTO 150		/* EBX puerto, ECX valor, EDX octetos | ES_ESCRIBE -> lo leído */
#define SC_MEMORIA 151		/* EBX física, ECX octetos -> dónde se ve en el programa (0: no se puede) */
#define SC_PCI 152		/* EBX dirección (como en CF8h; bits 0-1: el octeto), ECX valor, EDX octetos | ES_ESCRIBE */
#define SC_SERVICIO 153		/* EBX struct pe_energia (o 0), ECX tics que esperar como mucho -> la orden */
#define SC_DISPOSITIVO 156	/* (dispositivos) EBX struct pe_dispositivo, ECX 0 apuntar / 1 olvidar los suyos */
#define ES_ESCRIBE 0x100
#define ES_ERROR(r) ((r) >= 0xFFFFFFF0u)

void mq_pon_rsdp(u32 d);	/* (UEFI) la RSDP que dice el núcleo («rsdp=»): maquina.c */

/* las órdenes del núcleo (SC_SERVICIO) */
#define ORDEN_NADA 0
#define ORDEN_APAGAR 1
#define ORDEN_REINICIAR 2
#define ORDEN_SIN_ARCHIVOS 3	/* no escribir más en el disco (el núcleo lo va a vaciar para apagar) */

/* lo que el servicio responde (pe_energia.respuesta) */
#define RESP_NADA 0
#define RESP_PARADO 1		/* (reiniciar) el controlador embebido avisado; ya no se toca ACPI */
#define RESP_FALLO 2		/* (apagar) se ha pedido S5 y el equipo sigue encendido */

int nucleo_llamada(u32 f, u32 dir_pet);

static inline u32 sc(u32 n, u32 a, u32 b, u32 c)
{
	u32 r;

	__asm__ __volatile__("int $0x80" : "=a"(r) : "a"(n), "b"(a), "c"(b), "d"(c) : "memory");
	return r;
}

/* la batería y lo que hace falta para apagar y reiniciar, para el núcleo */
#define PE_VERSION 2		/* (PANEL) la 2 lleva «boton» */
struct pe_energia {
	u32 version;		/* PE_VERSION */
	u32 fase;		/* FASE_... de llamadas.h */
	u32 nbat;		/* baterías puestas */
	u32 pct;		/* 0..100; FFh no se sabe */
	u32 ca;			/* 0 sin cable, 1 con cable, FFh no se sabe */
	u32 cargando;		/* 1 cargando */
	u32 minutos;		/* lo que queda; FFFFh no se sabe */
	u32 estado;		/* 0 alta, 1 baja, 2 crítica, 3 cargando, FFh */
	u32 lecturas;		/* cuántas lecturas completas van (para ver que cambia) */
	u32 salud_pct;		/* guarda llena / guardaba nueva, en %; FFh no se sabe */
	u32 salud_n;		/* baterías con salud */
	u32 unidad;		/* 0 mWh, 1 mAh */
	u32 diseno, llena;	/* las de todas las baterías con salud, sumadas */
	u32 ciclos1, ciclos2;	/* ciclos de carga de la primera y la segunda; FFFFh no se sabe */
	u32 s5;			/* 1: se sabe apagar (lo de abajo vale) */
	u32 pm1a_cnt, pm1b_cnt;	/* puertos de PM1a_CNT y PM1b_CNT (0: no hay) */
	u32 slp_typa, slp_typb;	/* lo de \_S5 */
	u32 smi_cmd, acpi_enable;	/* para pasar a modo ACPI (0: no hace falta) */
	u32 reset_esp;		/* el registro de reinicio de la FADT: 0 memoria, 1 E/S, 2 PCI, FFh no hay */
	u32 reset_dir;		/* (PCI: bus << 16 | disp << 11 | func << 8 | registro) */
	u32 reset_valor;
	u32 respuesta;		/* RESP_... */
	u32 sin_archivos;	/* 1: ya no se escribe en el disco (ORDEN_SIN_ARCHIVOS) */
	u32 boton;		/* (PANEL) las veces que se ha pulsado el botón de encendido (PWRBTN_STS de PM1) */
};

/* (dispositivos) lo que el servicio ha hallado en el espacio de nombres de
 * ACPI, para el Administrador de dispositivos (la llamada 156). La misma
 * ficha que Dispositivo de castel/nucleo32/dispositivos.cas (320 octetos):
 * si cambia una, la otra. */
#define DISP_BUS_ACPI 3
#define DISP_SIN_CONTROLADOR 0
#define DISP_FUNCIONA 1
#define DISP_NO_HACE_FALTA 4
#define DISP_PUERTOS 1
#define DISP_MEMORIA 2
struct pe_recurso {
	u32 clase, base, largo;
};
struct pe_dispositivo {
	u32 bus, direccion, padre, fabricante, producto, clase, subsistema, revision;
	u32 estado, irq, n_recursos, dato1, dato2, dato3;
	struct pe_recurso recursos[6];
	char pnp[16], controlador[16], nombre[48], marca[32], detalle[80];
};

#endif
