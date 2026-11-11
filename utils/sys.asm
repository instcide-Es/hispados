; =============================================================================
;  Hispa-DOS SYS.COM - hacer arrancable una unidad ya formateada
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  SYS unidad:     copia KERNEL.SYS y COMMAND.COM desde la unidad de arranque
;                  y escribe el código de arranque de Hispa-DOS (FAT12/FAT16),
;                  conservando los parámetros (BPB) de la unidad.
; =============================================================================
        cpu 386
        bits 16
        org 0x100

start:
        mov si, msg_title
        call puts
        ; unidad de destino
        mov si, 0x81
.sk:
        lodsb
        cmp al, ' '
        je .sk
        cmp al, 13
        je usage
        and al, 0xDF
        sub al, 'A'
        jb usage
        cmp al, 26
        jae usage
        cmp byte [si], ':'
        jne usage
        mov [drv], al
        add al, 'A'
        mov [dst_k], al
        mov [dst_c], al
        mov [msg_drv], al
        ; unidad de arranque (origen)
        mov ax, 0x3305
        int 0x21
        add dl, 'A' - 1
        mov [src_k], dl
        mov [src_c], dl
        cmp dl, [dst_k]
        jne .diff
        mov si, msg_same
        call puts
        mov ax, 0x4C01
        int 0x21
.diff:
        ; sector de arranque de la unidad (INT 25h, paquete de 32 bits)
        mov dword [pk_sec], 0
        mov word [pk_cnt], 1
        mov word [pk_off], boot
        mov [pk_seg], ds
        mov al, [drv]
        mov cx, 0xFFFF
        mov bx, packet
        int 0x25
        pop word [junk]
        jc ioerr
        cmp word [boot+0x0B], 512
        jne nofat
        ; FAT16 si tiene 4085 clústeres o más
        movzx eax, word [boot+0x13]
        test eax, eax
        jnz .t
        mov eax, [boot+0x20]
.t:
        movzx ecx, word [boot+0x16]
        movzx edx, byte [boot+0x10]
        imul ecx, edx
        movzx edx, word [boot+0x0E]
        add ecx, edx
        movzx edx, word [boot+0x11]
        add edx, 15
        shr edx, 4
        add ecx, edx
        sub eax, ecx
        movzx ecx, byte [boot+0x0D]
        test ecx, ecx
        jz nofat
        xor edx, edx
        div ecx
        mov si, boot12
        cmp eax, 4085
        jb .copy
        mov si, boot16
.copy:
        ; código de arranque nuevo sin tocar el BPB (0Bh-3Dh)
        mov di, boot
        mov cx, 0x0B
        rep movsb
        add si, 0x3E - 0x0B
        add di, 0x3E - 0x0B
        mov cx, 512 - 0x3E
        rep movsb
        ; copiar los ficheros del sistema
        mov si, src_k
        mov di, dst_k
        call copy_file
        jc cperr
        mov si, src_c
        mov di, dst_c
        call copy_file
        jc cperr
        mov dx, dst_k
        mov ax, 0x4301
        mov cx, 0x07                ; sistema, oculto, solo lectura
        int 0x21
        ; escribir el sector de arranque
        mov al, [drv]
        mov cx, 0xFFFF
        mov bx, packet
        int 0x26
        pop word [junk]
        jc ioerr
        mov ah, 0x0D
        int 0x21
        mov si, msg_ok
        call puts
        mov ax, 0x4C00
        int 0x21

usage:
        mov si, msg_usage
        call puts
        mov ax, 0x4C01
        int 0x21
ioerr:
        mov si, msg_ioerr
        call puts
        mov ax, 0x4C02
        int 0x21
nofat:
        mov si, msg_nofat
        call puts
        mov ax, 0x4C03
        int 0x21
cperr:
        mov si, msg_cperr
        call puts
        mov ax, 0x4C04
        int 0x21

; SI = origen, DI = destino (si existe se sustituye, aunque sea de sistema)
copy_file:
        mov dx, di
        mov ax, 0x4301
        xor cx, cx
        int 0x21                    ; quitar atributos de un KERNEL.SYS anterior
        mov dx, si
        mov ax, 0x3D00
        int 0x21
        jc .r
        mov [hsrc], ax
        mov dx, di
        mov ah, 0x3C
        xor cx, cx
        int 0x21
        jc .cl
        mov [hdst], ax
.l:
        mov bx, [hsrc]
        mov dx, buf
        mov cx, 16384
        mov ah, 0x3F
        int 0x21
        jc .both
        test ax, ax
        jz .done
        mov cx, ax
        mov bx, [hdst]
        mov dx, buf
        mov ah, 0x40
        int 0x21
        jc .both
        cmp ax, cx
        jne .both
        jmp .l
.done:
        mov bx, [hsrc]
        mov ax, 0x5700
        int 0x21
        mov bx, [hdst]
        mov ax, 0x5701
        int 0x21
        mov ah, 0x3E
        int 0x21
        mov bx, [hsrc]
        mov ah, 0x3E
        int 0x21
        clc
        ret
.both:
        mov bx, [hdst]
        mov ah, 0x3E
        int 0x21
.cl:
        mov bx, [hsrc]
        mov ah, 0x3E
        int 0x21
        stc
.r:
        ret

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

drv         db 0
junk        dw 0
hsrc        dw 0
hdst        dw 0
packet:
pk_sec      dd 0
pk_cnt      dw 0
pk_off      dw 0
pk_seg      dw 0
src_k       db "A:\KERNEL.SYS", 0
src_c       db "A:\COMMAND.COM", 0
dst_k       db "A:\KERNEL.SYS", 0
dst_c       db "A:\COMMAND.COM", 0
msg_title   db "Hispa-DOS SYS 0.2", 13, 10, 0
msg_usage   db "Uso: SYS unidad:", 13, 10, 0
msg_same    db "No se puede transferir el sistema a la unidad de arranque.", 13, 10, 0
msg_ioerr   db "Error al leer o escribir el disco.", 13, 10, 0
msg_nofat   db "La unidad no tiene un sistema de ficheros FAT12/FAT16.", 13, 10, 0
msg_cperr   db "No se han podido copiar los ficheros del sistema.", 13, 10, 0
msg_ok      db "Sistema transferido a la unidad "
msg_drv     db "?:", 13, 10, 0

boot12:     incbin "build/boot.bin"
boot16:     incbin "build/boot16.bin"
boot:       times 512 db 0
buf:
