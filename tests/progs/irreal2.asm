; =============================================================================
;  IRREAL2.COM - prueba: ¿respeta un residente el modo «irreal» de los demás?
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Hace lo que HSPUSB, HSPSND o HSPRED con la e1000: deja DS, ES, FS y GS con
;  el límite de 4 GB y lee por encima de 1 MB durante unos 3 segundos con las
;  interrupciones abiertas. Si un residente que trabaja desde el reloj
;  (HSPWIFI: pasa a modo protegido en cada tic) vuelve con el límite de 64 KB,
;  el procesador da la excepción 13: aquí se dice y se sale (en un equipo de
;  verdad, el controlador interrumpido se quedaría colgado).
;  Solo vale con un procesador de verdad o con KVM: QEMU sin KVM no comprueba
;  los límites. Lo usa tests/run_wifi_tests.sh.
; =============================================================================
        cpu 386
        org 0x100
        mov ax, 0x350D
        int 0x21
        mov [old0d], bx
        mov [old0d+2], es
        mov dx, int0d
        mov ax, 0x250D
        int 0x21
        xor eax, eax
        mov ax, cs
        shl eax, 4
        add eax, gdt
        mov [gdtr+2], eax
        cli
        lgdt [gdtr]
        mov eax, cr0
        or al, 1
        mov cr0, eax
        jmp short $+2
        mov bx, 8
        mov fs, bx
        mov gs, bx
        mov ds, bx
        mov es, bx
        and al, 0xFE
        mov cr0, eax
        jmp short $+2
        xor bx, bx
        mov fs, bx
        mov gs, bx
        push cs
        pop ds
        push cs
        pop es
        sti
        xor ax, ax
        mov es, ax
        mov bx, [es:0x46C]
        push cs
        pop es
.l:
        mov eax, [dword fs:0x00200000]    ; por encima de 1 MB
        mov eax, [dword gs:0x00200004]
        mov esi, 0x00200008
        mov eax, [esi]              ; DS
        mov eax, [es:esi]
        push es
        xor ax, ax
        mov es, ax
        mov ax, [es:0x46C]
        pop es
        sub ax, bx
        cmp ax, 55
        jb .l
        mov dx, bien
fin:
        push cs
        pop ds
        mov ah, 9
        int 0x21
        lds dx, [cs:old0d]
        mov ax, 0x250D
        int 0x21
        mov ax, 0x4C00
        int 0x21
int0d:
        ; ¿excepción o IRQ 5? (no hay IRQ 5 en la prueba: excepción)
        mov dx, roto
        jmp fin
old0d   dd 0
gdtr    dw 15
        dd 0
gdt     dq 0
        dq 0x008F92000000FFFF
bien    db "IRREAL-BIEN: el limite de 4 GB sigue puesto", 13, 10, "$"
roto    db "IRREAL-ROTO: excepcion 13, alguien ha quitado el limite de 4 GB", 13, 10, "$"
