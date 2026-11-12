; =============================================================================
;  Hispa-DOS - sector de arranque FAT12 / FAT12 boot sector
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  La BIOS carga este sector en 0000:7C00. Se copia a 8000:0000, busca
;  KERNEL.SYS en el directorio raíz, lo carga en 0070:0000 y salta a él con:
;     DL    = unidad de arranque
;     DS:SI = copia de este sector (con su BPB) en 8000:0000
; =============================================================================
        cpu 386
        bits 16
        org 0

RELOC_SEG   equ 0x8000
BUF_OFF     equ 0x0200
KERNEL_SEG  equ 0x0070

        jmp short start
        nop
oem         db "HISPADOS"
bps         dw 512
spc         db 1
rsv         dw 1
nfats       db 2
rootent     dw 224
totsec      dw 2880
media       db 0xF0
spf         dw 9
spt         dw 18
heads       dw 2
hidden      dd 0
totsec32    dd 0
drive       db 0
            db 0
bootsig     db 0x29
serial      dd 0x20261111
vlabel      db "HISPADOS   "
fstype      db "FAT12   "

start:
        jmp 0x07C0:norm
norm:
        cli
        mov ax, cs
        mov ds, ax
        mov ax, RELOC_SEG
        mov es, ax
        mov ss, ax
        mov sp, 0xFFF0
        xor si, si
        xor di, di
        mov cx, 256
        cld
        rep movsw
        jmp RELOC_SEG:reloc
reloc:
        sti
        mov ax, cs
        mov ds, ax
        mov [drive], dl

        mov al, [nfats]
        xor ah, ah
        mul word [spf]
        add ax, [rsv]
        mov [root_lba], ax
        mov ax, [rootent]
        shr ax, 4
        mov [root_secs], ax
        add ax, [root_lba]
        mov [data_lba], ax

        mov ax, [root_lba]
        mov cx, [root_secs]
        mov bx, BUF_OFF
.rdroot:
        call read_sector
        inc ax
        add bx, 512
        loop .rdroot

        mov cx, [rootent]
        mov di, BUF_OFF
.find:
        push cx
        push di
        mov si, kname
        mov cx, 11
        repe cmpsb
        pop di
        pop cx
        je .found
        add di, 32
        loop .find
        jmp fail
.found:
        mov ax, [di+26]
        mov [cluster], ax

        mov ax, [rsv]
        mov cx, [spf]
        mov bx, BUF_OFF
.rdfat:
        call read_sector
        inc ax
        add bx, 512
        loop .rdfat

        mov ax, KERNEL_SEG
        mov es, ax
        xor bx, bx
.next:
        mov ax, [cluster]
        sub ax, 2
        xor ch, ch
        mov cl, [spc]
        mul cx
        add ax, [data_lba]
        xor ch, ch
        mov cl, [spc]
.rdclus:
        call read_sector
        inc ax
        mov dx, es
        add dx, 0x20
        mov es, dx
        loop .rdclus

        mov ax, [cluster]
        mov si, ax
        shr si, 1
        add si, ax
        mov dx, [si+BUF_OFF]
        test al, 1
        jz .even
        shr dx, 4
        jmp .gotn
.even:
        and dx, 0x0FFF
.gotn:
        mov [cluster], dx
        cmp dx, 0x0FF8
        jb .next

        mov dl, [drive]
        xor si, si
        jmp KERNEL_SEG:0000

; leer el sector LBA AX en ES:BX, con reintentos
read_sector:
        pusha
        mov di, 5
.retry:
        push ax
        ; sector absoluto = relativo + sectores ocultos: en un disquete son 0,
        ; pero una partición FAT12 de un disco duro (menos de 16 MB, una
        ; tarjeta CF pequeña) empieza después de la tabla de particiones
        xor dx, dx
        add ax, [hidden]
        adc dx, [hidden+2]
        div word [spt]
        mov cx, dx
        inc cx
        xor dx, dx
        div word [heads]
        mov dh, dl
        mov ch, al
        shl ah, 6
        or cl, ah
        mov dl, [drive]
        mov ax, 0x0201
        int 0x13
        pop ax
        jnc .ok
        push ax
        xor ax, ax
        int 0x13
        pop ax
        dec di
        jnz .retry
        jmp fail
.ok:
        popa
        ret

fail:
        mov si, msg_fail
.p:     lodsb
        test al, al
        jz .halt
        mov ah, 0x0E
        mov bx, 7
        int 0x10
        jmp .p
.halt:
        xor ax, ax
        int 0x16
        int 0x19

kname       db "KERNEL  SYS"
msg_fail    db "KERNEL.SYS?", 13, 10, 0

        times 0x1F0 - ($-$$) db 0
root_lba    dw 0
root_secs   dw 0
data_lba    dw 0
cluster     dw 0
        times 510 - ($-$$) db 0
        dw 0xAA55
