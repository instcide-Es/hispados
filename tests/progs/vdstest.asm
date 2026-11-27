; VDSTEST.COM - prueba de los Virtual DMA Services (INT 4Bh AX=81xxh) de
; HSPEMM (docs/HSPEMM.md, «VDS»), con la especificación de Microsoft tal como
; la recoge la Interrupt List de Ralf Brown (INT 4Bh). Cada comprobación dice
; «VDS ...: BIEN» o «VDS ...: MAL»; al final, «VDS: todo bien» o «VDS: hay
; fallos». Sin VDS (sin HSPEMM, o con un gestor que no lo tenga) lo dice y
; acaba con el código 1.
; Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
        cpu 386
        bits 16
        org 0x100

        ; 1. el bit 5 de 0040:007Bh
        push ds
        mov ax, 0x40
        mov ds, ax
        mov al, [0x7B]
        pop ds
        test al, 0x20
        mov si, m_bit
        call mira_nz
        ; 2. 8102h: la versión (antes, que el vector no sea 0000:0000)
        mov ax, 0x354B
        int 0x21
        mov ax, es
        or ax, bx
        push cs
        pop es
        jnz .vec
        mov si, m_nohay
        call puts
        mov ax, 0x4C01
        int 0x21
.vec:
        mov ax, 0x8102
        xor dx, dx
        stc
        int 0x4B
        jnc .hay
        mov si, m_nohay
        call puts
        mov ax, 0x4C01
        int 0x21
.hay:
        push ax
        mov si, m_ver
        call puts
        pop ax
        push ax
        mov al, ah
        call hex2
        mov al, '.'
        call putc
        pop ax
        call hex2
        mov si, m_prod
        call puts
        mov ax, bx
        call hex4
        mov si, m_crlf
        call puts
        ; 3. bloquear 4 KB de la memoria convencional (este programa): la
        ; física es la lineal
        mov word [dds+0], 4096
        mov word [dds+2], 0
        mov word [dds+4], buf
        mov word [dds+6], 0
        mov [dds+8], cs
        mov ax, 0x8103
        xor dx, dx
        push cs
        pop es
        mov di, dds
        int 0x4B
        setnc bl
        movzx eax, word [dds+8]
        shl eax, 4
        movzx ecx, word [dds+4]
        add eax, ecx
        cmp eax, [dds+0x0C]
        sete bh
        and bl, bh
        cmp word [dds+0x0A], 0
        sete bh
        and bl, bh
        test bl, bl
        mov si, m_conv
        call mira_nz
        ; 4. soltarlo
        mov ax, 0x8104
        xor dx, dx
        mov di, dds
        int 0x4B
        setnc al
        test al, al
        mov si, m_solt
        call mira_nz
        ; 5. un bloque de la memoria superior (estrategia de DOS: solo arriba)
        mov ax, 0x5802
        int 0x21
        mov [enlace], al
        mov ax, 0x5800
        int 0x21
        mov [estrat], ax
        mov ax, 0x5803
        mov bx, 1
        int 0x21
        mov ax, 0x5801
        mov bx, 0x0040              ; solo memoria superior, el primero que quepa
        int 0x21
        mov ah, 0x48
        mov bx, 0x0100              ; 4 KB
        int 0x21
        jc .noumb                   ; (umbseg sigue a 0: no se libera nada)
        mov [umbseg], ax
        cmp ax, 0xA000
        jb .noumb
        mov word [dds+0], 4096
        mov word [dds+2], 0
        mov word [dds+4], 0
        mov word [dds+6], 0
        mov [dds+8], ax
        mov ax, 0x8103
        xor dx, dx
        mov di, dds
        int 0x4B
        jc .umbmal
        movzx eax, word [umbseg]
        shl eax, 4
        cmp eax, [dds+0x0C]         ; en la memoria superior, la física NO es la lineal
        je .umbmal
        cmp dword [dds+0x0C], 0x100000
        jb .umbmal                  ; (es RAM de más arriba del primer mega)
        mov si, m_umb
        call puts
        mov eax, [dds+0x0C]
        call hex8
        mov al, ' '
        call putc
        mov si, m_bien
        call puts
        jmp .umbfin
.umbmal:
        mov si, m_umb
        call puts
        mov si, m_mal
        call puts
        inc byte [fallos]
        jmp .umbfin
.noumb:
        mov si, m_noumb
        call puts
.umbfin:
        cmp word [umbseg], 0
        je .nolib
        mov ah, 0x49
        mov es, [umbseg]
        int 0x21
        push cs
        pop es
.nolib:
        mov ax, 0x5801
        mov bx, [estrat]
        int 0x21
        mov ax, 0x5803
        movzx bx, byte [enlace]
        int 0x21
        ; 6. una región que cruza un límite de 64 KB físico, con DX bit 4: error 02h
        ;    (de 0000:FF00, 512 octetos: cruza 10000h)
        mov dword [dds+0], 512
        mov dword [dds+4], 0xFF00
        mov word [dds+8], 0
        mov ax, 0x8103
        mov dx, 0x0010
        mov di, dds
        int 0x4B
        setc bl
        cmp al, 0x02
        sete bh
        and bl, bh
        cmp dword [dds+0], 0x100    ; lo que cabe hasta el límite
        sete bh
        and bl, bh
        test bl, bl
        mov si, m_lim
        call mira_nz
        ; 7. una función que no hay (8105h, scatter/gather): error 0Fh
        mov ax, 0x8105
        xor dx, dx
        mov di, dds
        int 0x4B
        setc bl
        cmp al, 0x0F
        sete bh
        and bl, bh
        test bl, bl
        mov si, m_nof
        call mira_nz
        ; 8. bits reservados en DX: error 10h
        mov ax, 0x8103
        mov dx, 0x0001
        mov di, dds
        int 0x4B
        setc bl
        cmp al, 0x10
        sete bh
        and bl, bh
        test bl, bl
        mov si, m_rsv
        call mira_nz
        ; 9. no traducir el canal 1 y volver a traducirlo: 810Ch da ZF = 1
        mov ax, 0x810B
        mov bx, 1
        xor dx, dx
        int 0x4B
        setnc cl
        mov ax, 0x810C
        mov bx, 1
        xor dx, dx
        int 0x4B
        setnc ch
        setz al
        and cl, ch
        and cl, al
        test cl, cl
        mov si, m_can
        call mira_nz
        ; 10. canal 9: error 0Ch
        mov ax, 0x810B
        mov bx, 9
        xor dx, dx
        int 0x4B
        setc bl
        cmp al, 0x0C
        sete bh
        and bl, bh
        test bl, bl
        mov si, m_can9
        call mira_nz
        ; fin
        cmp byte [fallos], 0
        jne .mal
        mov si, m_todo
        call puts
        mov ax, 0x4C00
        int 0x21
.mal:
        mov si, m_hay
        call puts
        mov ax, 0x4C02
        int 0x21

; ZF = 0 (bien) o 1 (mal) -> «texto: BIEN/MAL»
mira_nz:
        pushf
        call puts
        popf
        jz .m
        mov si, m_bien
        jmp puts
.m:
        inc byte [fallos]
        mov si, m_mal
        jmp puts

hex8:
        push eax
        shr eax, 16
        call hex4
        pop eax
hex4:
        push ax
        mov al, ah
        call hex2
        pop ax
hex2:
        push ax
        shr al, 4
        call .d
        pop ax
.d:
        and al, 15
        add al, '0'
        cmp al, '9'
        jbe putc
        add al, 7
putc:
        push ax
        push dx
        mov dl, al
        mov ah, 2
        int 0x21
        pop dx
        pop ax
        ret
puts:
        lodsb
        test al, al
        jz .r
        call putc
        jmp puts
.r:
        ret

fallos  db 0
enlace  db 0
estrat  dw 0
umbseg  dw 0
dds     times 16 db 0
m_bit   db "VDS el bit 5 de 0040:007Bh: ", 0
m_nohay db "VDS: no hay (INT 4Bh AX=8102h da CF)", 13, 10, 0
m_ver   db "VDS version ", 0
m_prod  db ", producto ", 0
m_conv  db "VDS bloquear 4 KB de la memoria convencional (fisica = lineal): ", 0
m_solt  db "VDS soltarlos: ", 0
m_umb   db "VDS bloquear 4 KB de la memoria superior (fisica distinta, de mas arriba del primer mega): ", 0
m_noumb db "VDS (sin memoria superior: no se prueba)", 13, 10, 0
m_lim   db "VDS una region que cruza 64 KB, con DX bit 4: error 02h y 256 octetos: ", 0
m_nof   db "VDS 8105h (scatter/gather, no admitida): error 0Fh: ", 0
m_rsv   db "VDS bits reservados en DX: error 10h: ", 0
m_can   db "VDS no traducir el canal 1 y volver a traducirlo (ZF = 1): ", 0
m_can9  db "VDS canal 9: error 0Ch: ", 0
m_bien  db "BIEN", 13, 10, 0
m_mal   db "MAL", 13, 10, 0
m_todo  db "VDS: todo bien", 13, 10, 0
m_hay   db "VDS: hay fallos", 13, 10, 0
m_crlf  db 13, 10, 0
        align 16
buf     times 4096 db 0
