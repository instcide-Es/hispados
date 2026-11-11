; =============================================================================
;  HSPRAM.SYS - disco RAM de HispaDOS (driver de bloque)
;  Copyright (C) 2026 HispaDOS contributors. GPL-2.0-or-later.
;
;  CONFIG.SYS:  DEVICE=C:\DRIVERS\HSPRAM.SYS [KB]     (16-512, por defecto 64)
;  Reserva la memoria convencional justo detrás del driver, le da formato
;  FAT12 y aparece como la siguiente letra de unidad libre.
; =============================================================================
        cpu 386
        bits 16
        org 0

header:
        dw 0xFFFF, 0xFFFF           ; siguiente driver
        dw 0x0002                   ; bloque, sectores de 32 bits
        dw strategy
        dw interrupt
        db 1                        ; unidades
        db "HSPRAM "

req         dd 0
bpb:
b_bps       dw 512
b_spc       db 1
b_rsv       dw 1
b_nfats     db 1
b_root      dw 64
b_tot       dw 0
b_media     db 0xF8
b_spf       dw 0
b_spt       dw 32
b_heads     dw 1
b_hidden    dd 0
b_tot32     dd 0
bpbarr      dw bpb
disk_seg    dw 0
nsec        dw 0

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
        je media_check
        cmp al, 2
        je build_bpb
        cmp al, 4
        je do_read
        cmp al, 8
        je do_write
        cmp al, 9
        je do_write
done:
        mov word [bx+3], 0x0100
exit:
        pop es
        pop ds
        popad
        retf
fail_sector:
        mov word [bx+3], 0x8108
        jmp exit

media_check:
        mov byte [bx+0x0E], 1       ; el medio no ha cambiado
        jmp done

build_bpb:
        mov word [bx+0x12], bpb
        mov [bx+0x14], cs
        jmp done

; EAX = primer sector, CX = cuenta -> CF si se sale del disco
range:
        movzx eax, word [bx+0x14]
        cmp ax, 0xFFFF
        jne .s
        mov eax, [bx+0x1A]
.s:
        movzx ecx, word [bx+0x12]
        mov edx, eax
        add edx, ecx
        movzx esi, word [cs:nsec]
        cmp edx, esi
        ja .bad
        clc
        ret
.bad:
        stc
        ret

do_read:
        call range
        jc fail_sector
        les di, [bx+0x0E]
        cld
.l:
        jcxz .d
        push ds
        push cx
        mov dx, ax
        shl dx, 5
        add dx, [cs:disk_seg]
        mov ds, dx
        xor si, si
        mov cx, 256
        rep movsw
        pop cx
        pop ds
        inc ax
        dec cx
        jmp .l
.d:
        jmp done

do_write:
        call range
        jc fail_sector
        push ds
        lds si, [bx+0x0E]
        cld
.l:
        jcxz .d
        push cx
        mov dx, ax
        shl dx, 5
        add dx, [cs:disk_seg]
        mov es, dx
        xor di, di
        mov cx, 256
        rep movsw
        pop cx
        inc ax
        dec cx
        jmp .l
.d:
        pop ds
        jmp done

resident_end:

; -----------------------------------------------------------------------------
;  Inicialización (esta parte se descarta tras cargar)
; -----------------------------------------------------------------------------
init:
        ; tamaño en KB desde la línea de CONFIG.SYS (tras el nombre)
        push ds
        push bx
        lds si, [bx+0x12]
.skipname:
        lodsb
        cmp al, ' '
        je .sp
        cmp al, 9
        je .sp
        cmp al, 13
        je .def
        cmp al, 10
        je .def
        test al, al
        jz .def
        jmp .skipname
.sp:
        lodsb
        cmp al, ' '
        je .sp
        dec si
        xor dx, dx
.num:
        lodsb
        sub al, '0'
        jb .numd
        cmp al, 9
        ja .numd
        imul dx, dx, 10
        movzx ax, al
        add dx, ax
        jmp .num
.numd:
        test dx, dx
        jnz .have
.def:
        mov dx, 64
.have:
        pop bx
        pop ds
        cmp dx, 16
        jae .min
        mov dx, 16
.min:
        cmp dx, 512
        jbe .max
        mov dx, 512
.max:
        mov [cs:kb], dx
        shl dx, 1
        mov [cs:nsec], dx
        mov [cs:b_tot], dx
        ; sectores por FAT12: (clústeres + 2) * 1,5 bytes
        movzx eax, dx
        add eax, 2
        imul eax, eax, 3
        shr eax, 1
        add eax, 511
        shr eax, 9
        mov [cs:b_spf], ax
        ; el disco empieza en el primer párrafo libre tras el driver
        ; tras todo el fichero: el código de inicio aún se está ejecutando
        mov ax, file_end + 15
        shr ax, 4
        mov dx, cs
        add ax, dx
        mov [cs:disk_seg], ax
        ; ponerlo a cero
        mov cx, [cs:nsec]
        mov dx, ax
        cld
.z:
        push cx
        mov es, dx
        xor di, di
        xor ax, ax
        mov cx, 256
        rep stosw
        add dx, 32
        pop cx
        loop .z
        ; sector de arranque con el BPB
        mov es, [cs:disk_seg]
        mov byte [es:0], 0xEB
        mov byte [es:1], 0x3C
        mov byte [es:2], 0x90
        mov eax, 'HISP'
        mov [es:3], eax
        mov eax, 'ADOS'
        mov [es:7], eax
        push ds
        push cs
        pop ds
        mov si, bpb
        mov di, 0x0B
        mov cx, 25
        rep movsb
        pop ds
        mov word [es:510], 0xAA55
        ; FAT: medio + FF FF
        mov ax, es
        add ax, 32
        mov es, ax
        mov byte [es:0], 0xF8
        mov word [es:1], 0xFFFF
        ; etiqueta de volumen en la primera entrada del directorio raíz
        mov ax, [cs:b_spf]
        inc ax
        shl ax, 5
        add ax, [cs:disk_seg]
        mov es, ax
        push ds
        push cs
        pop ds
        mov si, vlabel
        xor di, di
        mov cx, 11
        rep movsb
        pop ds
        mov byte [es:11], 0x08
        ; respuesta al núcleo
        mov byte [bx+0x0D], 1
        mov word [bx+0x0E], 0
        mov ax, [cs:nsec]
        shl ax, 5
        add ax, [cs:disk_seg]
        mov [bx+0x10], ax
        mov word [bx+0x12], bpbarr
        mov [bx+0x14], cs
        ; mensaje
        mov al, [bx+0x16]
        add al, 'A'
        mov [cs:msg_drive], al
        push ds
        push bx
        push cs
        pop ds
        mov dx, msg1
        mov ah, 9
        int 0x21
        mov ax, [kb]
        call print_dec
        mov dx, msg2
        mov ah, 9
        int 0x21
        pop bx
        pop ds
        jmp done

; AX -> decimal
print_dec:
        mov cx, 0
        mov bx, 10
.l:
        xor dx, dx
        div bx
        push dx
        inc cx
        test ax, ax
        jnz .l
.p:
        pop dx
        add dl, '0'
        mov ah, 2
        int 0x21
        loop .p
        ret

kb          dw 0
vlabel      db "HSPRAMDISK "
msg1        db "Disco RAM de HispaDOS: $"
msg2        db " KB en la unidad "
msg_drive   db "?:", 13, 10, "$"
file_end:
