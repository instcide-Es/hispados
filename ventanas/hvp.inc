; =============================================================================
;  Hispa Ventanas - plantilla de un programa aparte (módulo .HVP)
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Un .HVP es un binario plano de 32 bits que Hispa Ventanas carga en su
;  memoria y pone en marcha como una tarea más (docs/VENTANAS.md, «Programas
;  aparte»). Se hace con NASM y tools/mkhvp.py (regla del Makefile). Así:
;
;       %include "hvp.inc"
;       HVP_CABECERA "Nombre", entrada      ; nombre (31 letras) y entrada
;                                           ; (un tercer dato, 1: va al grupo
;                                           ; «Accesorios» y no a «Juegos»;
;                                           ; 2: al grupo «Sonido»)
;               db "................................"    ; el icono: 32 filas
;               ...                                     ; de 32 caracteres
;       HVP_CODIGO
;       entrada:                    ; EBX=base del módulo ESI=su ruta
;               ...                 ; EDI=parámetros del acceso directo
;               ret                 ; al volver, el programa ha terminado
;       %include "hvplib.inc"       ; (si se quieren sus rutinas)
;               section .bss        ; variables sin valor inicial (a cero)
;       mi_var  resd 1
;       HVP_FIN
;
;  Las funciones de Hispa Ventanas se llaman con «HV nombre» (o «HVJ nombre»
;  para saltar, como en «HVJ ven_defecto»), con los mismos registros que
;  dentro de VENTANAS.COM; la lista, en api.inc. «COLOR c» pone el color de
;  dibujo. Las constantes (mensajes, estilos, ficha de la ventana, colores)
;  son las de defs.inc. Las direcciones son absolutas, como en cualquier
;  programa: mkhvp.py saca la tabla de reubicaciones.
; =============================================================================
%include "defs.inc"
%include "api.inc"

%assign hv_n 0
%macro HVF 1
HV_%1       equ hv_n
%assign hv_n hv_n + 1
%endmacro
        HV_LISTA
HV_API_N    equ hv_n                ; las entradas de la interfaz que se conocen

%ifndef HVP_ORG
%define HVP_ORG 0                   ; (mkhvp.py ensambla dos veces, con dos valores)
%endif

; ---- la cabecera (64 octetos) y, detrás, el icono (512) ----
;   0 "HVP1"            4 dw versión del formato     6 dw entradas de la interfaz
;   8 dd tamaño del archivo        12 dd memoria que necesita
;  16 dd entrada                   20 dd tamaño de la imagen (detrás van las
;  24 dd reubicaciones             reubicaciones: un dd por puntero)
;  28 dd grupo                       32 nombre (32 octetos, acabado en 0)
;  64 icono de 32x32 a 4 bits     576 tabla de la interfaz (la rellena el cargador)
%macro HVP_CABECERA 2-3 0
        cpu 386
        bits 32
        org HVP_ORG
        section .text
hvp_base:
        db "HVP1"
        dw 1
        dw HV_API_N
        dd 0
        dd hvp_fin
        dd %2
        dd 0, 0
        dd %3                       ; el grupo: 0 «Juegos», 1 «Accesorios», 2 «Sonido»
%%n:    db %1
        times 32 - ($ - %%n) db 0
hvp_icono:
%endmacro

%macro HVP_CODIGO 0
        times 576 - ($ - hvp_base) db 0
hv_api:
        times HV_API_N dd 0
%endmacro

%macro HVP_FIN 0
        section .bss
        alignb 4
hvp_fin:
%endmacro

; llamada (o salto) a una función de Hispa Ventanas
%macro HV 1
        call [hv_api + HV_%1 * 4]
%endmacro
%macro HVJ 1
        jmp [hv_api + HV_%1 * 4]
%endmacro
; el color con el que se dibuja (conserva los registros)
%macro COLOR 1
        push eax
        mov eax, %1
        call [hv_api + HV_dc_color * 4]
        pop eax
%endmacro
; negrita: NEGRITA 1 / NEGRITA 0
%macro NEGRITA 1
        push eax
        mov eax, %1
        call [hv_api + HV_dc_negrita * 4]
        pop eax
%endmacro
