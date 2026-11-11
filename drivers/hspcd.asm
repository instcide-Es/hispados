; =============================================================================
;  HSPCD.SYS - lectores de CD/DVD (ATAPI) para HispaDOS
;  Copyright (C) 2026 HispaDOS contributors. GPL-2.0-or-later.
;
;  CONFIG.SYS:  DEVICE=C:\DRIVERS\HSPCD.SYS
;
;  Busca lectores ATAPI en los canales IDE clásicos (1F0h y 170h, maestro y
;  esclavo) y los ofrece como unidades de bloque de sectores de 512 bytes
;  (cada sector de 2048 bytes del disco son cuatro). El núcleo de HispaDOS
;  reconoce el sistema de archivos ISO 9660 de los CD y DVD de datos.
; =============================================================================
        cpu 486
        bits 16
        org 0

MAXCD       equ 4

header:
        dw 0xFFFF, 0xFFFF
        dw 0x0802                   ; bloque, sectores de 32 bits
        dw strategy
        dw interrupt
        db 1, "HSPCD  "

req         dd 0
ncd         db 0
            db 0
cd_port     dw 0, 0, 0, 0
cd_sel      db 0, 0, 0, 0
buf_unit    db 0xFF
            db 0
buf_lba     dd 0xFFFFFFFF
; BPB ficticio: el núcleo detecta ISO 9660 leyendo el sector 16
bpb:
            dw 512
            db 4
            dw 1
            db 0                    ; sin FAT: el núcleo prueba ISO 9660
            dw 16
            dw 0
            db 0xF8
            dw 1
            dw 1
            dw 1
            dd 0
            dd 0x7FFFFFFF
bpbarr      dw bpb, bpb, bpb, bpb

strategy:
        mov [cs:req], bx
        mov [cs:req+2], es
        retf

interrupt:
        pushad
        push ds
        push es
        lds bx, [cs:req]
        mov al, [bx+2]
        cmp al, 0
        je init
        cmp al, 1
        je .media
        cmp al, 2
        je .bpb
        cmp al, 4
        je .read
        cmp al, 8
        je .wp
        cmp al, 9
        je .wp
.done:
        mov word [bx+3], 0x0100
.out:
        pop es
        pop ds
        popad
        retf
.media:
        mov byte [bx+0x0E], 0       ; "no se sabe": puede haber cambiado el disco
        jmp .done
.bpb:
        mov word [bx+0x12], bpb
        mov [bx+0x14], cs
        jmp .done
.wp:
        mov word [bx+3], 0x8100     ; protegido contra escritura
        jmp .out
.read:
        movzx eax, word [bx+0x14]
        cmp ax, 0xFFFF
        jne .s
        mov eax, [bx+0x1A]
.s:
        mov [cs:r_lba], eax
        mov cx, [bx+0x12]
        les di, [bx+0x0E]
        mov al, [bx+1]
        mov [cs:r_unit], al
        cld
.l:
        jcxz .done
        ; bloque de 2048 bytes que contiene el sector de 512
        mov eax, [cs:r_lba]
        shr eax, 2
        call get_block
        jc .err
        mov si, [cs:r_lba]
        and si, 3
        shl si, 9
        add si, cdbuf
        push ds
        push cs
        pop ds
        push cx
        mov cx, 256
        rep movsw
        pop cx
        pop ds
        ; normalizar ES:DI
        mov ax, di
        shr ax, 4
        mov dx, es
        add dx, ax
        mov es, dx
        and di, 15
        inc dword [cs:r_lba]
        dec cx
        jmp .l
.err:
        mov ax, [bx+0x12]
        sub ax, cx
        mov [bx+0x12], ax
        mov word [bx+3], 0x8102     ; unidad no preparada (sin disco)
        jmp .out
r_lba       dd 0
r_unit      db 0

; EAX = bloque de 2048 de la unidad r_unit -> cdbuf; CF si falla
get_block:
        mov dl, [cs:r_unit]
        cmp dl, [cs:buf_unit]
        jne .rd
        cmp eax, [cs:buf_lba]
        je .ok
.rd:
        push bx
        push cx
        push di
        push es
        mov [cs:buf_lba], eax
        mov [cs:buf_unit], dl
        mov cx, 3
.try:
        push cx
        call atapi_read
        pop cx
        jnc .good
        loop .try
        mov byte [cs:buf_unit], 0xFF
        pop es
        pop di
        pop cx
        pop bx
        stc
        ret
.good:
        pop es
        pop di
        pop cx
        pop bx
.ok:
        clc
        ret

; leer el bloque [buf_lba] de la unidad [buf_unit] con READ(10) por PIO
atapi_read:
        movzx bx, byte [cs:buf_unit]
        mov al, [cs:cd_sel+bx]
        shl bx, 1
        mov dx, [cs:cd_port+bx]
        mov [cs:port], dx
        add dx, 6
        out dx, al
        call delay400
        call wait_nbsy
        jc .r
        mov dx, [cs:port]
        inc dx
        xor al, al
        out dx, al                  ; PIO
        add dx, 3
        out dx, al                  ; límite de bytes: 2048
        inc dx
        mov al, 0x08
        out dx, al
        add dx, 2
        mov al, 0xA0                ; PACKET
        out dx, al
        call delay400
        call wait_drq
        jc .r
        ; paquete READ(10) de un bloque
        mov eax, [cs:buf_lba]
        bswap eax
        mov [cs:pkt+2], eax
        mov word [cs:pkt+7], 0x0100
        mov dx, [cs:port]
        mov si, pkt
        mov cx, 6
.p:
        mov ax, [cs:si]
        out dx, ax
        add si, 2
        loop .p
        call delay400
        call wait_drq
        jc .r
        mov dx, [cs:port]
        push cs
        pop es
        mov di, cdbuf
        mov cx, 1024
        cld
        rep insw
        call delay400
        call wait_nbsy
.r:
        ret
pkt         db 0x28, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
port        dw 0

delay400:
        push dx
        push cx
        mov dx, [cs:port]
        add dx, 7
        mov cx, 4
.l:
        in al, dx
        loop .l
        pop cx
        pop dx
        ret
; esperar que no esté ocupado (CF si error o plazo)
wait_nbsy:
        push cx
        push dx
        mov dx, [cs:port]
        add dx, 7
        mov ecx, 0x200000
.l:
        in al, dx
        test al, 0x80
        jz .ok
        loop .l, ecx
        stc
        jmp .r
.ok:
        test al, 1
        jz .c
        stc
        jmp .r
.c:
        clc
.r:
        pop dx
        pop cx
        ret
; esperar datos (DRQ) (CF si error o plazo)
wait_drq:
        push cx
        push dx
        mov dx, [cs:port]
        add dx, 7
        mov ecx, 0x200000
.l:
        in al, dx
        test al, 0x80
        jnz .n
        test al, 1
        jnz .e
        test al, 8
        jnz .ok
.n:
        loop .l, ecx
.e:
        stc
        jmp .r
.ok:
        clc
.r:
        pop dx
        pop cx
        ret

cdbuf       times 2048 db 0

resident_end:

; =============================================================================
init:
        push ds
        push cs
        pop ds
        mov si, msg_title
        call puts
        ; buscar lectores ATAPI
        mov si, ide_list
.l:
        mov dx, [si]
        test dx, dx
        jz .end
        mov al, [si+2]
        push si
        call probe
        pop si
        add si, 3
        jmp .l
.end:
        pop ds
        lds bx, [cs:req]
        cmp byte [cs:ncd], 0
        jne .found
        push cs
        pop ds
        mov si, msg_none
        call puts
        lds bx, [cs:req]
        mov byte [bx+0x0D], 0
        mov word [bx+0x0E], 0
        mov [bx+0x10], cs
        jmp interrupt.done
.found:
        mov al, [cs:ncd]
        mov [bx+0x0D], al
        mov word [bx+0x0E], resident_end
        mov [bx+0x10], cs
        mov word [bx+0x12], bpbarr
        mov [bx+0x14], cs
        ; letras
        mov al, [bx+0x16]
        add al, 'A'
        mov [cs:msg_drv], al
        push cs
        pop ds
        mov si, msg_units
        call puts
        jmp interrupt.done

; DX = puerto, AL = A0h/B0h
probe:
        cmp byte [ncd], MAXCD
        jae .no
        mov [port], dx
        mov [tsel], al
        add dx, 6
        out dx, al
        call delay400
        mov dx, [port]
        add dx, 7
        in al, dx
        cmp al, 0xFF
        je .no
        mov al, 0xA1                ; IDENTIFY PACKET DEVICE
        out dx, al
        call delay400
        mov cx, 0xFFFF
.w:
        in al, dx
        test al, 0x80
        jnz .n
        test al, 1
        jnz .no
        test al, 8
        jnz .data
.n:
        loop .w
        jmp .no
.data:
        mov dx, [port]
        push cs
        pop es
        mov di, cdbuf
        mov cx, 256
        cld
        rep insw
        ; registrar la unidad
        movzx bx, byte [ncd]
        mov al, [tsel]
        mov [cd_sel+bx], al
        shl bx, 1
        mov ax, [port]
        mov [cd_port+bx], ax
        inc byte [ncd]
        ; modelo
        mov si, msg_model
        call puts
        mov si, cdbuf + 54
        mov cx, 20
.m:
        lodsw
        xchg al, ah
        push ax
        mov dl, al
        mov ah, 2
        int 0x21
        pop ax
        mov dl, ah
        mov ah, 2
        int 0x21
        loop .m
        mov si, msg_crlf
        call puts
.no:
        ret
tsel        db 0

puts:
        lodsb
        test al, al
        jz .r
        mov dl, al
        mov ah, 2
        int 0x21
        jmp puts
.r:
        ret

ide_list    dw 0x1F0
            db 0xA0
            dw 0x1F0
            db 0xB0
            dw 0x170
            db 0xA0
            dw 0x170
            db 0xB0
            dw 0
msg_title   db "HSPCD 0.1 - lectores de CD/DVD de HispaDOS", 13, 10, 0
msg_model   db "Lector: ", 0
msg_units   db "Primera unidad de CD/DVD: "
msg_drv     db "?:", 13, 10, 0
msg_none    db "No se ha encontrado ningún lector de CD/DVD (ATAPI).", 13, 10, 0
msg_crlf    db 13, 10, 0
