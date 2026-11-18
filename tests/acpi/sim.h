/* sim.h - Pruebas del núcleo ACPI en el equipo de compilación.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef SIM_ACPI_H
#define SIM_ACPI_H
extern int sim_charla;
extern u32 sim_rsdp, sim_paginas, sim_ec_ordenes;
extern u8 sim_ec[256], sim_ec_tocado[256];
extern int sim_ec_mudo;
void sim_muere(const char *que) __attribute__((noreturn));
void sim_region(u32 fis, u32 tam, void *p);
void *sim_trozo(u32 tam);
void sim_avanza_us(u32 us);
void sim_evento(int q);
void sim_es_fija(u32 puerto, u32 valor);
u32 sim_pci_lee(u32 cf8, u32 desp, u32 n);
int nucleo_llamada(u32 f, u32 dir_pet);
#endif
