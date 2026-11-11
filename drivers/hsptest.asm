; =============================================================================
;  HSPTEST.SYS - driver de carácter de prueba (dispositivo HSPTEST$)
;  Copyright (C) 2026 HispaDOS contributors. GPL-2.0-or-later.
;
;  Al leerlo devuelve lo último que se le escribió (o un mensaje de prueba)
;  y después fin de fichero. Sirve para probar DEVICE= y la cadena de
;  dispositivos, y como ejemplo de driver de carácter.
; =============================================================================
        cpu 386
        bits 16
        org 0

header:
        dw 0xFFFF, 0xFFFF
        dw 0x8000                   ; dispositivo de carácter
        dw strategy
        dw interrupt
        db "HSPTEST$"

req     dd 0
pos     dw 0
len     dw msg_end - buf
buf     db "HispaDOS test driver OK", 13, 10
msg_end:
        times 128 - (msg_end - buf) db 0
BUFLEN  equ 128

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
        cmp al, 4
        je do_read
        cmp al, 5
        je do_peek
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

do_read:
        mov cx, [bx+0x12]
        mov ax, [cs:len]
        sub ax, [cs:pos]
        jnz .some
        ; fin: la próxima lectura empieza de nuevo
        mov word [cs:pos], 0
        mov word [bx+0x12], 0
        jmp done
.some:
        cmp cx, ax
        jbe .c
        mov cx, ax
.c:
        mov [bx+0x12], cx
        les di, [bx+0x0E]
        push ds
        push cs
        pop ds
        mov si, buf
        add si, [pos]
        add [pos], cx
        cld
        rep movsb
        pop ds
        jmp done

do_peek:
        mov ax, [cs:pos]
        cmp ax, [cs:len]
        jb .have
        mov word [bx+3], 0x0300     ; ocupado: no hay datos
        jmp exit
.have:
        mov si, ax
        mov al, [cs:buf+si]
        mov [bx+0x0D], al
        jmp done

do_write:
        mov cx, [bx+0x12]
        cmp cx, BUFLEN
        jbe .c
        mov cx, BUFLEN
.c:
        mov [cs:len], cx
        mov word [cs:pos], 0
        push ds
        lds si, [bx+0x0E]
        push cs
        pop es
        mov di, buf
        cld
        rep movsb
        pop ds
        jmp done

init:
        mov word [bx+0x0E], resident_end
        mov [bx+0x10], cs
        push ds
        push cs
        pop ds
        mov dx, msg_init
        mov ah, 9
        int 0x21
        pop ds
        jmp done

resident_end:
msg_init db "HSPTEST.SYS: test device installed", 13, 10, "$"
