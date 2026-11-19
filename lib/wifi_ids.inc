; Hispa-DOS - las tarjetas Wi-Fi que sabe usar HSPWIFI, para buscarlas en el
; PCI sin cargar el núcleo: pares (fabricante, aparato) en palabras, acabados
; en 0. Son las de wifi/nucleo/radio_brcm.c (Broadcom) y radio_ath9k.c
; (Atheros). Lo incluyen lib/redpref.inc (HSPRED, HSPWIFI) y ventanas/wifi.inc
; detrás de su etiqueta.
; Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
            dw 0x14E4, 0x4353, 0x14E4, 0x4357, 0x14E4, 0x4727, 0x14E4, 0x0576, 0x14E4, 0xA8D8
            dw 0x168C, 0x002B, 0x168C, 0x002A, 0x168C, 0x002E, 0x168C, 0x0029, 0x168C, 0x002D
            dw 0x168C, 0x0024, 0x168C, 0x0023, 0x168C, 0x0027, 0x168C, 0x002C
            dw 0
