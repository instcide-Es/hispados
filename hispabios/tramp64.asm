; =============================================================================
;  HispaBIOS (x64) - trampolín entre el modo del firmware y el modo real
; =============================================================================
;  Se copia a la dirección física TR_BASE (memoria baja pedida al firmware) y
;  se llama desde la parte UEFI con interrupciones cerradas. Guarda el estado
;  del firmware (GDT, IDT, CR0/CR3/CR4, EFER, pila y selectores), baja al
;  modo real pasando por el modo compatible de 32 bits y el protegido de 16,
;  hace el trabajo de modo real y vuelve por el mismo camino.
;
;  Paso 3: el trabajo de modo real es solo escribir una marca en TR_MARCA.
;
;  Referencias: Intel SDM vol. 3, 9.8 (salir del modo largo) y 9.9 (pasar
;  entre modo real y protegido).
; =============================================================================
TR_BASE         equ 8000h

        org TR_BASE

SEL_C64         equ 08h
SEL_C32         equ 10h
SEL_D32         equ 18h
SEL_C16         equ 20h
SEL_D16         equ 28h
EFER            equ 0C0000080h

; ---- entrada desde el firmware (64 bits) ----
        bits 64
entrada:
        push rbx
        push rbp
        push r12
        push r13
        push r14
        push r15
        pushfq
        cli
        mov [rel sv_rsp], rsp
        sgdt [rel sv_gdt]
        sidt [rel sv_idt]
        mov rax, cr0
        mov [rel sv_cr0], rax
        mov rax, cr3
        mov [rel sv_cr3], rax
        mov rax, cr4
        mov [rel sv_cr4], rax
        mov ecx, EFER
        rdmsr
        mov [rel sv_efer], eax
        mov [rel sv_efer + 4], edx
        mov [rel sv_ds], ds
        mov [rel sv_es], es
        mov [rel sv_ss], ss
        mov [rel sv_fs], fs
        mov [rel sv_gs], gs
        mov [rel sv_cs], cs
        lgdt [rel gdtr]
        push SEL_C32                    ; al modo compatible de 32 bits
        mov rax, baja32
        push rax
        retfq

; ---- bajada: 32 bits sin paginación ----
        bits 32
baja32:
        mov ax, SEL_D32
        mov ds, ax
        mov es, ax
        mov ss, ax
        mov eax, cr0
        and eax, 7FFFFFFFh              ; sin paginación: se sale del modo largo
        mov cr0, eax
        mov ecx, EFER
        rdmsr
        and eax, ~100h                  ; LME a 0
        wrmsr
        jmp SEL_C16:baja16

; ---- protegido de 16 bits -> real ----
        bits 16
baja16:
        mov ax, SEL_D16
        mov ds, ax
        mov es, ax
        mov ss, ax
        mov fs, ax
        mov gs, ax
        mov eax, cr0
        and eax, ~1
        mov cr0, eax
        jmp 0:real

real:
        xor ax, ax
        mov ds, ax
        mov es, ax
        mov ss, ax
        mov sp, TR_PILA
        lidt [idtr_real]

        ; ===== el trabajo de modo real (paso 3: una marca) =====
        mov word [TR_MARCA], 1234h
        ; =========================================================

; ---- subida: real -> protegido de 32 -> largo ----
        cli
        o32 lgdt [gdtr]
        mov eax, cr0
        or eax, 1
        mov cr0, eax
        jmp dword SEL_C32:sube32

        bits 32
sube32:
        mov ax, SEL_D32
        mov ds, ax
        mov es, ax
        mov ss, ax
        mov eax, [sv_cr4]
        mov cr4, eax                    ; (con PAE, como lo tenía)
        mov eax, [sv_cr3]
        mov cr3, eax                    ; las tablas de páginas del firmware
        mov ecx, EFER
        mov eax, [sv_efer]
        mov edx, [sv_efer + 4]
        wrmsr                           ; LME como estaba
        mov eax, [sv_cr0]
        mov cr0, eax                    ; paginación: modo largo (compatible)
        jmp SEL_C64:sube64

        bits 64
sube64:
        lgdt [rel sv_gdt]
        lidt [rel sv_idt]
        mov ax, [rel sv_ds]
        mov ds, ax
        mov ax, [rel sv_es]
        mov es, ax
        mov ax, [rel sv_ss]
        mov ss, ax
        mov ax, [rel sv_fs]
        mov fs, ax
        mov ax, [rel sv_gs]
        mov gs, ax
        mov rsp, [rel sv_rsp]
        movzx eax, word [rel sv_cs]     ; el selector de código del firmware
        push rax
        lea rax, [rel vuelta]
        push rax
        retfq
vuelta:
        popfq
        pop r15
        pop r14
        pop r13
        pop r12
        pop rbp
        pop rbx
        ret

; ---- datos ----
        align 16
gdt:
        dq 0
        dq 00AF9A000000FFFFh            ; 08h código de 64 bits
        dq 00CF9A000000FFFFh            ; 10h código de 32 bits
        dq 00CF92000000FFFFh            ; 18h datos de 32 bits
        dq 00009A000000FFFFh            ; 20h código de 16 bits (base 0)
        dq 000092000000FFFFh            ; 28h datos de 16 bits (base 0)
gdt_fin:
gdtr:   dw gdt_fin - gdt - 1
        dd gdt
        dd 0
idtr_real:
        dw 3FFh
        dd 0

        align 8
sv_rsp  dq 0
sv_cr0  dq 0
sv_cr3  dq 0
sv_cr4  dq 0
sv_efer dq 0
sv_gdt  dw 0
        dq 0
sv_idt  dw 0
        dq 0
sv_ds   dw 0
sv_es   dw 0
sv_ss   dw 0
sv_fs   dw 0
sv_gs   dw 0
sv_cs   dw 0

TR_FIN:
TR_MARCA equ TR_BASE + 0E00h            ; marca que deja el modo real (paso 3)

TR_PILA equ TR_BASE + 0F00h             ; pila de modo real (en la misma página)
        times 0E00h - (TR_FIN - entrada) db 0   ; (falla si no cabe antes de la marca)
