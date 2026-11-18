; =============================================================================
;  VTDON.COM - (pruebas) hace de firmware UEFI con la protección de DMA
;  puesta: enciende la traducción de VT-d de QEMU (-machine q35 -device
;  intel-iommu, unidad en FED90000h) con todo cerrado, y se queda residente
;  (las tablas tienen que seguir en la memoria).
;     VTDON      el bus 0 con su tabla de contexto vacía (nadie tiene entrada)
;     VTDON P    todos los dispositivos del bus 0 con entrada, y una tabla
;                de páginas vacía (nada abierto)
;  Así la tarjeta de sonido no puede leer la memoria, como en el MacBook Air
;  2017 arrancado con HispaBIOS. Lo usa tests/run_vtd_tests.sh para probar
;  lib/vtd.inc (HSPSND y AUDICION). Ver docs/AUDICION.md.
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
; =============================================================================
        bits 16
        org 0x100
UNIDAD      equ 0xFED90000
        ; A20 abierta (los registros están por encima de 1 MB) y FS plano
        in al, 0x92
        mov [a20], al
        or al, 2
        and al, 0xFE
        out 0x92, al
        xor eax, eax
        mov ax, cs
        shl eax, 4
        mov ebx, eax
        add eax, gdt
        mov [gdtr+2], eax
        cli
        lgdt [gdtr]
        mov eax, cr0
        or al, 1
        mov cr0, eax
        jmp short .pm
.pm:
        mov dx, 8
        mov fs, dx
        and al, 0xFE
        mov cr0, eax
        jmp short .rm
.rm:
        xor dx, dx
        mov fs, dx
        sti
        mov esi, UNIDAD
        mov eax, [fs:esi]
        cmp eax, 0x10
        jb .nohay
        cmp eax, 0x7F
        ja .nohay
        ; las tres páginas: raíz, contexto y páginas (vacía), a cero
        lea eax, [ebx+fin+4095]
        and eax, 0xFFFFF000
        mov [raiz], eax
        mov edi, eax
        mov ecx, 3 * 4096 / 4
.z:
        mov dword [fs:edi], 0
        add edi, 4
        dec ecx
        jnz .z
        mov edi, [raiz]
        lea eax, [edi+0x1000]       ; tabla de contexto del bus 0
        or al, 1
        mov [fs:edi], eax
        ; ¿P? todas las entradas presentes, hacia la tabla de páginas vacía
        mov si, 0x81
.a:
        lodsb
        cmp al, ' '
        je .a
        and al, 0xDF
        cmp al, 'P'
        jne .tab
        mov esi, UNIDAD
        mov eax, [fs:esi+8]
        shr eax, 8
        and eax, 0x0E
        bsr edx, eax                ; ancho de direcciones
        or edx, 0x100               ; dominio 1
        mov edi, [raiz]
        lea eax, [edi+0x2000]
        or al, 1                    ; presente, traducción normal
        add edi, 0x1000
        mov cx, 256
.p:
        mov [fs:edi], eax
        mov [fs:edi+8], edx
        add edi, 16
        loop .p
.tab:
        mov esi, UNIDAD
        mov eax, [raiz]
        mov [fs:esi+0x20], eax
        mov dword [fs:esi+0x24], 0
        mov eax, [fs:esi+0x1C]
        and eax, 0x96FFFFFF
        or eax, 0x40000000          ; SRTP: la tabla raíz
        mov [fs:esi+0x18], eax
        mov edx, 0x40000000
        call espera1
        mov dword [fs:esi+0x28], 0  ; cachés vacías
        mov dword [fs:esi+0x2C], 0xA0000000
        mov eax, [fs:esi+0x10]
        shr eax, 8
        and eax, 0x3FF
        shl eax, 4
        mov dword [fs:esi+eax+8], 0
        mov dword [fs:esi+eax+12], 0x90000000
        mov eax, [fs:esi+0x1C]
        and eax, 0x96FFFFFF
        or eax, 0x80000000          ; TE: traducción encendida
        mov [fs:esi+0x18], eax
        mov edx, 0x80000000
        call espera1
        jc .nohay
        call a20_fuera
        mov dx, msg_ok
        mov ah, 9
        int 0x21
        ; residente hasta el final de las tablas
        mov edx, [raiz]
        add edx, 3 * 4096 + 15
        xor eax, eax
        mov ax, cs
        shl eax, 4
        sub edx, eax
        shr edx, 4
        mov ax, 0x3100
        int 0x21
.nohay:
        call a20_fuera
        mov dx, msg_no
        mov ah, 9
        int 0x21
        mov ax, 0x4C01
        int 0x21
a20_fuera:
        mov al, [a20]
        out 0x92, al
        ret
; espera a que GSTS tenga EDX -> CF si no llega
espera1:
        mov ecx, 0x100000
.l:
        test [fs:esi+0x1C], edx
        jnz .ok
        dec ecx
        jnz .l
        stc
        ret
.ok:
        clc
        ret
a20         db 0
raiz        dd 0
gdtr        dw 15
            dd 0
gdt         dq 0
            dw 0xFFFF, 0
            db 0, 0x92, 0xCF, 0
msg_ok      db "VTDON: VT-d encendida, la DMA cerrada.", 13, 10, "$"
msg_no      db "VTDON: no hay unidad de VT-d en FED90000h.", 13, 10, "$"
fin:
