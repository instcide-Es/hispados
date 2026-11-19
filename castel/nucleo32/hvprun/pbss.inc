; =============================================================================
;  HVPRUN - sus variables (a cero al arrancar)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
; =============================================================================
        alignb 4
p_hz        resd 1                  ; los tics por segundo de Hispa-Pentium
pan_max_an  resd 1                  ; la pantalla de Hispa-Pentium
pan_max_al  resd 1
pan_ven     resd 1                  ; la ventana de Hispa-Pentium (0: aún no)
pan_ppal    resd 1                  ; la principal del programa (ficha de Ventanas)
pan_suceso  resd 4                  ; el suceso que se atiende: clase, a, b, c
pp_visto    resd 1                  ; la cuenta del portapapeles de Ventanas al ponerlo al día
pan_hay_guardado resb 1             ; 1: hay uno en pan_suceso (de esperar)
        alignb 4
pr_ox       resd 1                  ; presentar
pr_an       resd 1
pr_filas    resd 1
pr_orden    resd 9                  ; la orden de dibujar (op, x, y, ancho, alto, tinta, tinta2, datos, n)
pal32       resd 256                ; la paleta, como 0x00RRGGBB
mb_tabla    resb MB_MAX * 12        ; mem_bloque: puntero, tamaño, en uso
p_dta_ptr   resd 1                  ; la zona de transferencia (INT 21h, 1Ah)
bus_m       resd 1                  ; buscar: la carpeta abierta (su manejador)
bus_abierta resb 1                  ; 1: lo está
bus_atr     resb 1
bus_patron  resb 11
bus_nom     resb 11
        alignb 4
bus_ent     resb 32                 ; una entrada de la carpeta (llamada 12)
bus_dir     resb 104
p_info      resb 64                 ; lo que se sabe de una unidad (llamada 43)
dos_dta     resb 128                ; la zona de transferencia de Ventanas
p_linea     resb 256
p_aux       resb 128
p_aux2      resb 128
        alignb 16
pres_buf    resd PRES_MAX           ; una tira de la superficie, en 32 bits
