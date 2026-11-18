; HOLA486.ASM - un programa de DOS para ensamblarlo con FASMW (el flat
; assembler de Windows) dentro de Hispa Ventanas: la prueba de la fase 3d de
; Hispa-486 (tests/run_win32fasmw_tests.sh).
; Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
        org 100h
        mov ah, 9
        mov dx, texto
        int 21h
        int 20h
texto   db 'Hola desde FASMW', 13, 10, '$'
