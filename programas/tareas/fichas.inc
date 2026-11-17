; =============================================================================
;  fichas.inc - lo que el Administrador de tareas comparte con el núcleo
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Copia de las constantes de ventanas/tareas_api.inc (tools/mkhvp.py no lleva
;  ese archivo a los módulos). TIENEN QUE COINCIDIR con las de allí.
; =============================================================================
TF_ID       equ 0                   ; identificador de la tarea
TF_VEN      equ 4                   ; su ventana principal (0: no tiene)
TF_MEM      equ 8                   ; memoria del módulo: código, datos y bss
TF_PEDIDA   equ 12                  ; memoria que ha pedido (montón, bloques, pila)
TF_ESTADO   equ 16                  ; TE_*
TF_NOMBRE   equ 20                  ; 40 octetos, acabado en 0
TF_NVEN     equ 60                  ; ventanas de primer nivel con título
TF_TAM      equ 64
TE_NORESP   equ 1                   ; no responde
TE_SISTEMA  equ 2                   ; es del sistema: no se cierra
TE_MINIM    equ 4                   ; minimizada
TE_PARADA   equ 8                   ; parada tras un Ctrl+Alt+Supr
WM_HV_REINICIAR equ WM_USER + 220   ; Ctrl+Alt+Supr con el Administrador delante
