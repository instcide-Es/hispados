; =============================================================================
;  Hispa-Pentium - CORREO.BIN (Correo): la cabecera de programa del
;  anillo 3 (HPU3, docs/PENTIUM.md sección 6) y la entrada.
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  El C de Correo (castel/nucleo32/correo)
;  enlazado para 0x40000000, donde el núcleo pone los programas; como TLS.BIN
;  (nucleo32/tls/entrada.asm). El núcleo pone la pila (arriba del todo de la
;  zona del programa) y deja a cero lo que va detrás del archivo.
; =============================================================================
        bits 32
        extern correo_pentium, _fin_imagen, _fin_bss
        global _inicio

        section .entrada progbits alloc exec
_inicio:
        jmp short salto
        align 4, db 0
        dd 0x33555048               ; «HPU3»
        dd _inicio                  ; dónde va (0x40000000)
        dd _fin_imagen              ; hasta dónde llega lo del archivo
        dd _fin_bss                 ; hasta dónde lo que empieza a cero
        dd salto                    ; por dónde se empieza
salto:
        cld
        and esp, 0xFFFFFFF0
        fninit
        call correo_pentium
        mov ebx, eax                ; acabar (llamada 2) con lo que devuelve
        mov eax, 2
        int 0x80
.nunca:
        jmp .nunca

; ---- so_marca y so_salta (setjmp y longjmp): EBX, ESI, EDI, EBP, ESP y
; adónde se vuelve ----
        global so_marca, so_salta
        section .text
so_marca:
        mov eax, [esp+4]
        mov [eax], ebx
        mov [eax+4], esi
        mov [eax+8], edi
        mov [eax+12], ebp
        lea ecx, [esp+4]
        mov [eax+16], ecx
        mov ecx, [esp]
        mov [eax+20], ecx
        xor eax, eax
        ret
so_salta:
        mov edx, [esp+4]
        mov eax, [esp+8]
        test eax, eax
        jnz .no_cero
        inc eax
.no_cero:
        mov ebx, [edx]
        mov esi, [edx+4]
        mov edi, [edx+8]
        mov ebp, [edx+12]
        mov esp, [edx+16]
        jmp dword [edx+20]
