; NUCEXE.EXE - EXE con MAXALLOC (1) menor que MINALLOC (800h): MS-DOS da
; al menos MINALLOC. La pila está en la zona de MINALLOC, fuera de la imagen.
; Devuelve 55h si su bloque de memoria cubre imagen + MINALLOC; AAh si no.
; EXE whose MAXALLOC is below MINALLOC: its block must still cover MINALLOC.
        cpu 386
        bits 16
        org 0
MINALLOC equ 0x800
hdr:
        db "MZ"
        dw (img_end - img_start + 32) % 512
        dw (img_end - img_start + 32 + 511) / 512
        dw 0                    ; reubicaciones
        dw 2                    ; cabecera = 2 párrafos
        dw MINALLOC
        dw 1                    ; maxalloc < minalloc
        dw (img_end - img_start) / 16       ; SS: justo tras la imagen
        dw 0x4000               ; SP: dentro de MINALLOC
        dw 0
        dw code - img_start     ; IP
        dw 0                    ; CS
        dw 0x1C
        dw 0
        times 32 - ($ - hdr) db 0
img_start:
code:
        ; ES = PSP; su MCB está un párrafo antes
        mov ax, es
        dec ax
        mov ds, ax
        mov ax, [3]             ; párrafos del bloque
        cmp ax, 0x10 + (img_end - img_start) / 16 + MINALLOC
        jb .mal
        mov ax, 0x4C55
        int 0x21
.mal:
        mov ax, 0x4CAA
        int 0x21
        align 16, db 0
img_end:
