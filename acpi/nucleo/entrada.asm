; =============================================================================
;  Hispa-486 - entrada del núcleo ACPI (HSPACPI.BIN)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  El residente (drivers/hspacpi.asm) carga este archivo en un bloque de
;  memoria extendida, hace unos segmentos de 32 bits con la base en el
;  principio del bloque (así el núcleo está enlazado para la dirección 0 y no
;  hay que reubicar nada) y llama aquí con una llamada lejana de 32 bits:
;     EAX = función (acpi/nucleo/llamadas.h), EDX = dirección física de la
;     petición, EBX = dirección física del bloque (la base)
;     DS = ES = SS = segmento de datos de 32 bits, ESP = pila_tope (cabecera)
;     interrupciones cerradas
;  Devuelve EAX. Si dentro hay una excepción del procesador, se apunta en el
;  registro y se vuelve con EAX = -1000 - vector (el núcleo queda parado).
; =============================================================================
        bits 32
        extern nucleo_llamada, nucleo_excepcion, mq_base
        extern _ini_bss, _fin_bss, _fin_imagen
        global _inicio, pila_tope

SEL_COD equ 0x08                    ; el segmento de código de 32 bits

        section .entrada progbits alloc exec
_inicio:
        jmp short entra
        align 4
        db "HAC1"                   ; +4  firma
        dd _fin_imagen              ; +8  tamaño del archivo
        dd _fin_bss                 ; +12 memoria (archivo y datos a cero)
        dd pila_tope                ; +16 pila
        dd idt                      ; +20 tabla de interrupciones (256 octetos)
        dd 1                        ; +24 versión (LL_VERSION)
entra:
        cld
        test eax, eax
        jnz .ya
        ; primera llamada: los datos a cero y la tabla de excepciones
        push eax
        mov edi, _ini_bss
        mov ecx, _fin_bss
        sub ecx, edi
        xor eax, eax
        rep stosb
        mov edi, idt
        mov esi, trampas
        mov ecx, 32
.t:
        mov eax, esi
        mov [edi], ax
        mov word [edi+2], SEL_COD
        mov word [edi+4], 0x8E00    ; puerta de interrupción de 32 bits
        shr eax, 16
        mov [edi+6], ax
        add edi, 8
        add esi, TRAMPA
        loop .t
        pop eax
        mov [mq_base], ebx
.ya:
        mov [esp_vuelta], esp
        push edx
        push eax
        call nucleo_llamada
        add esp, 8
        retf

; una trampa por excepción (8 octetos cada una): apunta el vector y salta
; a la común
        align 8
trampas:
%assign i 0
%rep 32
        push dword i
        jmp near excepcion
        align 8
%assign i i+1
%endrep
TRAMPA  equ 8

excepcion:
        pop eax                     ; el vector
        mov edx, esp                ; lo que dejó el procesador en la pila
        mov cx, ss
        mov ds, cx
        mov es, cx
        cld
        push eax
        push edx
        push eax
        call nucleo_excepcion
        add esp, 8
        pop eax
        neg eax
        sub eax, 1000
        mov esp, [esp_vuelta]
        retf

        section .data
        align 8
idt     times 32 * 8 db 0
esp_vuelta dd 0

        section .pila nobits alloc write
        align 16
        resb 65536
pila_tope:
