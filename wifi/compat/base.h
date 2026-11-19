/* base.h - Lo de base.c que usan el núcleo y la radio (no lo de Linux).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HISPA_BASE_H
#define HISPA_BASE_H
void hl_monton(void *ini, u32 tam);
void hl_monton_info(u32 *tam, u32 *usado, u32 *pico);
int hl_monton_repasa(void);
int hl_tiempo_inicia(void);
u32 hl_tsc_khz(void);
void hl_trabajos(void);
void hl_olvida_dwork(struct delayed_work *d);
int hl_reg_lee(char *b, int max);
int hl_reg_hay(void);
void hl_reg_c(char c);
extern int hl_nivel, hl_pantalla;
u32 hl_pci_lee(int bus, int devfn, int w);
u32 hl_pci_dir(void);
void hl_pci_dir_pon(u32 v);
void pci_apaga(struct pci_dev *d);
void hl_fw_pon(int i, const char *nombre, const u8 *datos, u32 tam);
u32 hl_skb_vivos(void);
u32 hl_azar(void);
void __iomem *ioremap(u32 fis, unsigned long tam);
#endif
