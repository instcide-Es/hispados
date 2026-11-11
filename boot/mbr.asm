; =============================================================================
;  HispaDOS - registro maestro de arranque (MBR) / Master Boot Record
;  Copyright (C) 2026 HispaDOS contributors. GPL-2.0-or-later.
;
;  Se copia a 0000:0600, busca la partición activa, carga su sector de
;  arranque en 0000:7C00 (LBA si la BIOS lo admite, si no CHS) y salta a él
;  con DL = unidad y DS:SI = entrada de la partición.
;  FDISK copia los primeros 446 bytes; la tabla de particiones no se toca.
; =============================================================================
        cpu 386
        bits 16
        org 0x0600

start:
        cli
        xor ax, ax
        mov ss, ax
        mov sp, 0x7C00
        mov ds, ax
        mov es, ax
        sti
        cld
        mov si, 0x7C00
        mov di, 0x0600
        mov cx, 256
        rep movsw
        jmp 0:reloc
reloc:
        mov [drive], dl
        mov si, table
        mov cx, 4
.find:
        test byte [si], 0x80
        jnz .found
        add si, 16
        loop .find
        mov si, msg_noact
        jmp fail
.found:
        mov [part], si
        mov di, 5
.retry:
        mov si, [part]
        mov ah, 0x41
        mov bx, 0x55AA
        mov dl, [drive]
        int 0x13
        jc .chs
        cmp bx, 0xAA55
        jne .chs
        test cl, 1
        jz .chs
        mov si, [part]
        mov eax, [si+8]
        mov [dap_lba], eax
        mov si, dap
        mov ah, 0x42
        mov dl, [drive]
        int 0x13
        jnc .loaded
        jmp .again
.chs:
        mov si, [part]
        mov dh, [si+1]
        mov cx, [si+2]
        mov bx, 0x7C00
        mov ax, 0x0201
        mov dl, [drive]
        int 0x13
        jnc .loaded
.again:
        xor ax, ax
        mov dl, [drive]
        int 0x13
        dec di
        jnz .retry
        mov si, msg_err
        jmp fail
.loaded:
        cmp word [0x7DFE], 0xAA55
        je .go
        mov si, msg_noos
        jmp fail
.go:
        mov si, [part]
        mov dl, [drive]
        jmp 0:0x7C00

fail:
        lodsb
        test al, al
        jz .h
        mov ah, 0x0E
        mov bx, 7
        int 0x10
        jmp fail
.h:
        hlt
        jmp .h

dap:        db 16, 0
            dw 1
            dw 0x7C00, 0
dap_lba:    dd 0, 0
drive       db 0x80
part        dw 0

msg_noact   db "HispaDOS MBR: no active partition", 13, 10, 0
msg_err     db "HispaDOS MBR: disk read error", 13, 10, 0
msg_noos    db "HispaDOS MBR: missing operating system", 13, 10, 0

        times 446 - ($ - $$) db 0
table:
        times 64 db 0
        dw 0xAA55
