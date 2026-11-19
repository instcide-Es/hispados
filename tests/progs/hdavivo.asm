; HDAVIVO.COM - [REINICIO-SILENCIO] (pruebas) deja la tarjeta HD Audio como
; la deja la BIOS de los portátiles (Toshiba NB250, HP Mini 5103): la
; controladora fuera de reinicio (GCTL.CRST = 1) y, en el códec 0, los pines
; con salida (control 40h), EAPD encendido y los amplificadores sin silencio.
; Sin HSPSND. Así tests/run_reinicio_silencio_tests.sh puede ver que, con
; Ctrl+Alt+Supr o REINICIAR, el núcleo (lib/silencio.inc) la calla él solo.
; Usa las piezas de lib/silencio.inc (sil_pci, sil_cmd, sil_nodo).
;   HDAVIVO   -> «HDAVIVO: lista» o «HDAVIVO: no hay HD Audio»
        cpu 386
        bits 16
        org 0x100

start:
        ; la primera HD Audio del bus 0
        mov ebx, 0x80000000
.l:
        mov al, 8
        call sil_pci
        shr eax, 16
        cmp ax, 0x0403
        je .hay
        add ebx, 0x100
        cmp ebx, 0x80010000
        jb .l
        mov dx, m_no
        jmp fin
.hay:
        mov al, 0x10
        call sil_pci
        and eax, 0xFFFFFFF0
        mov esi, eax
        call irreal
        ; fuera de reinicio
        or byte [fs:esi+0x08], 1
        mov cx, 0xFFFF
.c:
        in al, 0x80
        test byte [fs:esi+0x08], 1
        loopz .c
        mov cx, 4000                ; que se presenten los códecs
.e:
        in al, 0x80
        loop .e
        xor cl, cl                  ; códec 0, el grupo de audio 1
        mov bl, 1
        mov eax, 0x00070500         ; D0
        call sil_nodo
        mov eax, 0x000F0004
        call sil_nodo
        jc .mal
        mov dl, al
        shr eax, 16
        mov bl, al
.w:
        mov eax, 0x000F0009
        call sil_nodo
        jc .sig
        mov edi, eax
        test di, 4
        jz .nb
        mov eax, 0x0003B040         ; salida, los dos lados, sin silencio
        call sil_nodo
.nb:
        shr edi, 20
        and di, 0x0F
        cmp di, 4
        jne .sig
        mov eax, 0x00070740         ; pin con salida
        call sil_nodo
        mov eax, 0x00070C02         ; EAPD encendido
        call sil_nodo
.sig:
        inc bl
        dec dl
        jnz .w
        mov dx, m_si
        jmp fin
.mal:
        mov dx, m_mal
fin:
        mov ah, 9
        int 0x21
        mov ax, 0x4C00
        int 0x21

irreal:
        cli
        push ds
        mov ax, cs
        movzx eax, ax
        shl eax, 4
        add eax, gdt
        mov [gdtr+2], eax
        lgdt [gdtr]
        mov eax, cr0
        or al, 1
        mov cr0, eax
        jmp short .pm
.pm:
        mov bx, 8
        mov fs, bx
        and al, 0xFE
        mov cr0, eax
        jmp short .rm
.rm:
        xor bx, bx
        mov fs, bx
        pop ds
        sti
        ret

%include "silencio.inc"

m_si    db "HDAVIVO: lista", 13, 10, "$"
m_no    db "HDAVIVO: no hay HD Audio", 13, 10, "$"
m_mal   db "HDAVIVO: el codec no contesta", 13, 10, "$"
        align 8
gdt     dq 0
        dw 0xFFFF, 0
        db 0, 0x92, 0xCF, 0
gdtr    dw 15
        dd 0
