; =============================================================================
;  Hispa-DOS - sector de arranque FAT16 (disco duro) / FAT16 boot sector
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  FORMAT rellena el BPB. Se copia a 8000:0000, busca KERNEL.SYS en el
;  directorio raíz, lo carga en 0070:0000 y salta a él con DL = unidad y
;  DS:SI = este sector. Lee por LBA (INT 13h 42h) o, si no hay, por CHS.
; =============================================================================
        cpu 386
        bits 16
        org 0

RELOC_SEG   equ 0x8000
KERNEL_SEG  equ 0x0070
BUF         equ 0x0400          ; búfer de un sector (8000:0400)
uselba      equ 0x0200          ; variables fuera del sector
csec        equ 0x0201
root_lba    equ 0x0204
data_lba    equ 0x0208
root_secs   equ 0x020C
cluster     equ 0x020E
lseg        equ 0x0210

        jmp short start
        nop
oem         db "HISPADOS"
bps         dw 512
spc         db 4
rsv         dw 1
nfats       db 2
rootent     dw 512
totsec      dw 0
media       db 0xF8
spf         dw 0
spt         dw 63
heads       dw 16
hidden      dd 0
totsec32    dd 0
drive       db 0x80
            db 0
bootsig     db 0x29
serial      dd 0
vlabel      db "NO NAME    "
fstype      db "FAT16   "

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
        mov byte [uselba], 0
        mov ah, 0x41
        mov bx, 0x55AA
        int 0x13
        jc .nolba
        cmp bx, 0xAA55
        jne .nolba
        test cl, 1
        jz .nolba
        mov byte [uselba], 1
.nolba:
        movzx eax, byte [nfats]
        movzx ecx, word [spf]
        mul ecx
        movzx ecx, word [rsv]
        add eax, ecx
        mov [root_lba], eax
        movzx ecx, word [rootent]
        shr cx, 4
        mov [root_secs], cx
        add eax, ecx
        mov [data_lba], eax

        mov eax, [root_lba]
        mov cx, [root_secs]
.rs:
        push cx
        mov bx, BUF
        call read_sec
        mov di, BUF
        mov dx, 16
.ent:
        push di
        mov si, kname
        mov cx, 11
        repe cmpsb
        pop di
        je .found
        add di, 32
        dec dx
        jnz .ent
        pop cx
        inc eax
        loop .rs
        mov si, msg_nok
        jmp fail
.found:
        pop cx
        mov ax, [di+26]
        mov [cluster], ax
        mov word [lseg], KERNEL_SEG
.next:
        movzx eax, word [cluster]
        sub eax, 2
        movzx ecx, byte [spc]
        mul ecx
        add eax, [data_lba]
        movzx cx, byte [spc]
.rc:
        push es
        mov es, [lseg]
        xor bx, bx
        call read_sec
        pop es
        add word [lseg], 0x20
        inc eax
        loop .rc
        ; siguiente clúster en la FAT16
        movzx eax, word [cluster]
        shr ax, 8
        movzx ecx, word [rsv]
        add eax, ecx
        mov bx, BUF
        call read_sec
        mov si, [cluster]
        and si, 0xFF
        shl si, 1
        mov ax, [BUF+si]
        mov [cluster], ax
        cmp ax, 0xFFF8
        jb .next

        mov dl, [drive]
        xor si, si
        jmp KERNEL_SEG:0000

; EAX = sector relativo a la partición, ES:BX = destino
read_sec:
        pushad
        add eax, [hidden]
        cmp byte [uselba], 0
        je .chs
        push dword 0
        push eax
        push es
        push bx
        push word 1
        push word 16
        mov si, sp
        mov ah, 0x42
        mov dl, [drive]
        int 0x13
        lea sp, [si+16]
        jc .err
        popad
        ret
.chs:
        xor edx, edx
        movzx ecx, word [spt]
        div ecx
        inc dl
        mov [csec], dl
        xor edx, edx
        movzx ecx, word [heads]
        div ecx
        mov dh, dl
        mov ch, al
        shl ah, 6
        mov cl, [csec]
        or cl, ah
        mov dl, [drive]
        mov ax, 0x0201
        int 0x13
        jc .err
        popad
        ret
.err:
        mov si, msg_err

fail:
        lodsb
        test al, al
        jz .h
        mov ah, 0x0E
        mov bx, 7
        int 0x10
        jmp fail
.h:
        jmp short .h

kname       db "KERNEL  SYS"
msg_nok     db "KERNEL.SYS?", 13, 10, 0
msg_err     db "Disk?", 13, 10, 0

        times 510 - ($ - $$) db 0
        dw 0xAA55
