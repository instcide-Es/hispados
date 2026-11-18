; =============================================================================
;  Hispa-DOS - sector de arranque FAT32 (disco duro) / FAT32 boot sector
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  FORMAT rellena el BPB. Se copia a 8000:0000, busca KERNEL.SYS en el
;  directorio raíz (en FAT32, una cadena de clústeres), lo carga en
;  0070:0000 y salta a él con DL = unidad y DS:SI = este sector. Lee por LBA
;  (INT 13h 42h: hace falta por encima de 8 GB) o, si la BIOS no lo tiene,
;  por CHS. Cabe en un solo sector: los otros reservados de FAT32 (FSInfo,
;  la copia de este sector) no llevan código. Usa la primera FAT (FORMAT y
;  los demás sistemas las dejan en espejo).
; =============================================================================
        cpu 386
        bits 16
        org 0

RELOC_SEG   equ 0x8000
KERNEL_SEG  equ 0x0070
BUF         equ 0x0400          ; búfer de un sector (8000:0400)
csec        equ 0x0201          ; variables fuera del sector
data_lba    equ 0x0208
cluster     equ 0x020C
lseg        equ 0x0210

        jmp short start
        nop
oem         db "HISPADOS"
bps         dw 512
spc         db 8
rsv         dw 32
nfats       db 2
rootent     dw 0
totsec      dw 0
media       db 0xF8
spf16       dw 0
spt         dw 63
heads       dw 255
hidden      dd 0
totsec32    dd 0
spf         dd 0                ; +24h: sectores por FAT
extflags    dw 0                ; +28h: bit 7 = sin espejo, bits 0-3 = FAT activa
fsver       dw 0
rootclus    dd 2                ; +2Ch
fsinfo      dw 1                ; +30h
backup      dw 6                ; +32h
            times 12 db 0
drive       db 0x80             ; +40h
            db 0
bootsig     db 0x29
serial      dd 0
vlabel      db "NO NAME    "
fstype      db "FAT32   "

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
        mov ah, 0x41
        mov bx, 0x55AA
        int 0x13
        jc .nolba
        cmp bx, 0xAA55
        jne .nolba
        shr cl, 1
        adc byte [uselba], 0
.nolba:
        movzx eax, byte [nfats]
        mul dword [spf]
        movzx ecx, word [rsv]
        add eax, ecx
        mov [data_lba], eax
        ; buscar KERNEL.SYS en la raíz
        mov eax, [rootclus]
.dc:
        mov [cluster], eax
        call clus_sec
.rs:
        mov bx, BUF
        call read_sec
        mov di, bx
.ent:
        mov si, kname
        push di
        push cx
        mov cx, 11
        repe cmpsb
        pop cx
        pop di
        je .found
        add di, 32
        cmp di, BUF+512
        jb .ent
        inc eax
        loop .rs
        call fat_next
        jb .dc
        mov si, kname               ; «KERNEL  SYS?»
        jmp fail
.found:
        push word [di+20]
        push word [di+26]
        pop eax
        mov word [lseg], KERNEL_SEG
.next:
        mov [cluster], eax
        call clus_sec
.rc:
        push es
        mov es, [lseg]
        xor bx, bx
        call read_sec
        pop es
        add word [lseg], 0x20
        inc eax
        loop .rc
        call fat_next
        jb .next

        mov dl, [drive]
        xor si, si
        jmp KERNEL_SEG:0000

; [cluster] -> EAX = su primer sector, CX = sectores por clúster
clus_sec:
        mov eax, [cluster]
        dec eax
        dec eax
        movzx ecx, byte [spc]
        mul ecx
        add eax, [data_lba]
        ret

; [cluster] -> EAX = el siguiente; CF (JB) si hay más
fat_next:
        mov eax, [cluster]
        shr eax, 7
        movzx ebx, word [rsv]       ; (la primera FAT: FORMAT las deja en espejo)
        add eax, ebx
        mov bx, BUF
        call read_sec
        mov bx, [cluster]
        and bx, 127
        shl bx, 2
        mov eax, [BUF+bx]
        and eax, 0x0FFFFFFF
        cmp eax, 0x0FFFFFF8
        ret

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

uselba      db 0
kname       db "KERNEL  SYS"
            db "?", 0
msg_err     db "Disk?", 0

        times 510 - ($ - $$) db 0
        dw 0xAA55
