; =============================================================================
;  Hispa Ventanas - [NAVEGADOR] lo que le faltaba al Navegador de Hispa-486
;  (w32/navega, docs/486/NAVEGADOR.md) de KERNEL32:
;    WinExec             abrir otro programa (el Lector con un PDF bajado)
;  Lo demás que pedía (SetDIBitsToDevice, GetKeyState, GetOpenFileNameA y
;  GetSaveFileNameA) lo da ya la fase 3d (win32b.inc y win32x.inc).
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
; =============================================================================

; UINT WinExec(línea, cómo): «programa parámetros» con prog_abrir (como un
; acceso directo: un .HVP se abre dentro de Hispa Ventanas). -> 33 (abierto)
WE_TAM      equ 160
k_WinExec:
        mov ecx, WE_TAM
        call mem_pedir
        jc .no
        mov ebx, eax
        mov esi, [ebp]
        mov edi, ebx
        mov ecx, WE_TAM - 1
        call w32_texto
        mov esi, ebx                ; el programa hasta el primer espacio
        xor edi, edi
.b:
        mov al, [esi]
        test al, al
        jz .p
        inc esi
        cmp al, ' '
        jne .b
        mov byte [esi-1], 0
        mov edi, esi                ; los parámetros
.p:
        mov esi, ebx
        xor edx, edx
        xor ecx, ecx
        push ebx
        xor ebx, ebx
        call prog_abrir
        pop eax
        call mem_soltar
        mov eax, 33
        ret
.no:
        xor eax, eax
        ret
