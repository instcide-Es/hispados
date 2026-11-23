; =============================================================================
;  trampolin.asm - Hispa-64, etapa 3: el trampolín de los demás procesadores
;  (docs/64/HISPA-64.md, sección 9). Es la fuente de los octetos que van en
;  smp64.cas (entre «<trampolín>» y «</trampolín>»: los ensambladores de la casa
;  no saben de 16 bits); pruebas.sh comprueba que NASM saca de aquí lo mismo.
;
;  Smp64 lo copia a una página por debajo de 1 MB (Fisica64.pide_bajo), le
;  rellena los datos y manda INIT-SIPI-SIPI con esa página: el procesador
;  empieza en modo real en su principio (CS = página << 8, IP = 0). Todo va
;  relativo a donde esté (EBX = su dirección física), sin nada que cambiar en
;  el código; la pila de los dos primeros pasos, lo alto de la página:
;    16 bits: la GDT de aquí, el modo protegido, y a 32 bits (con «retf»);
;    32 bits: PAE, las tablas de páginas del trampolín (CR3: la identidad de
;             los 2 primeros MB y la mitad alta del núcleo), EFER.LME (y NXE,
;             si lo hay: las tablas del núcleo llevan NX), la paginación con
;             WP, y a 64 bits (otro «retf»);
;    64 bits: la pila que le toca y el salto a la mitad alta (Smp64.ap_entra).
;  Los datos (desde el octeto 8):
;     8 la GDT (0x08 código de 64 bits, 0x10 datos, 0x18 código de 32 bits)
;    40 su GDTR (el límite y, en 42, la base: la rellena Smp64)
;    48 CR3 (la PML4 del trampolín, por debajo de 4 GB)   52 lo que se suma a EFER
;    56 lo alto de su pila (64 bits)                       64 adónde saltar (64 bits)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
; =============================================================================
bits 16
org 0
        jmp short inicio
        align 8, db 0
gdt:    dq 0
        dq 0x00AF9A000000FFFF
        dq 0x00CF92000000FFFF
        dq 0x00CF9A000000FFFF
gdtr:   dw 31
        dd 0
        align 8, db 0
v_cr3:  dd 0
v_efer: dd 0
v_pila: dq 0
v_salto: dq 0
inicio:
        cli
        cld
        mov ax, cs
        mov ds, ax
        mov ss, ax
        mov sp, 0x1000                  ; (la pila: lo alto de la página del trampolín)
        xor ebx, ebx
        mov bx, ax
        shl ebx, 4
        o32 lgdt [gdtr]
        mov eax, cr0
        or al, 1
        mov cr0, eax
        lea eax, [ebx + p32]
        push dword 0x18
        push eax
        o32 retf
bits 32
p32:
        mov ax, 0x10
        mov ds, ax
        mov es, ax
        mov ss, ax
        mov fs, ax
        mov gs, ax
        lea esp, [ebx + 0x1000]
        mov eax, cr4
        or eax, 0x20
        mov cr4, eax
        mov eax, [ebx + v_cr3]
        mov cr3, eax
        mov ecx, 0xC0000080
        rdmsr
        or eax, [ebx + v_efer]
        wrmsr
        mov eax, cr0
        or eax, 0x80010000
        mov cr0, eax
        lea eax, [ebx + p64]
        push dword 0x08
        push eax
        retf
bits 64
p64:
        mov ebx, ebx
        mov ax, 0x10
        mov ds, ax
        mov es, ax
        mov ss, ax
        xor eax, eax
        mov fs, ax
        mov gs, ax
        mov rsp, [rbx + v_pila]
        mov rax, [rbx + v_salto]
        xor ebp, ebp
        jmp rax
