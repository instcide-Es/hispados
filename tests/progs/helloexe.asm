; HELLOEXE.EXE - programa MZ con una reubicación de segmento
        bits 16
        org 0
hdr:
        db "MZ"
        dw (img_end - img_start + 32) % 512
        dw (img_end - img_start + 32 + 511) / 512
        dw 1                    ; reubicaciones
        dw 2                    ; cabecera = 2 párrafos
        dw 0x40                 ; minalloc
        dw 0xFFFF               ; maxalloc
        dw 0                    ; SS
        dw stack_top - img_start
        dw 0
        dw code - img_start     ; IP
        dw 0                    ; CS
        dw 0x1C
        dw 0
        dw reloc_site - img_start, 0
        times 32 - ($ - hdr) db 0
img_start:
code:
        mov ax, 0
reloc_site equ $ - 2
        add ax, (data - img_start) / 16
        mov ds, ax
        mov dx, msg - data
        mov ah, 9
        int 0x21
        mov ax, 0x4C00
        int 0x21
        align 16, db 0
data:
msg     db "Hello from a relocated .EXE program!", 13, 10, "$"
        align 16, db 0
        times 256 db 0
stack_top:
img_end:
