; EXEPILA.EXE - un .EXE con la pila mínima, como los lanzadores de algunos
; juegos: SS apunta al final de la imagen, SP = 2 y MINALLOC = 0. MS-DOS le
; pasa el control sin tocar su pila. Si el núcleo apila ahí CS:IP para saltar,
; SP da la vuelta (2 -> 0 -> FFFEh), se pisa la palabra de SS:0000 y el salto
; puede acabar en 0000:0000 (el PC se reinicia). Lo usa run_memprog_tests.sh.
; De paso comprueba que no lo han cargado en los primeros 64 KB (los .EXE
; comprimidos con EXEPACK no arrancan ahí: «Packed file is corrupt»).
        bits 16
        org 0
hdr:
        db "MZ"
        dw (img_end - img_start + 32) % 512
        dw (img_end - img_start + 32 + 511) / 512
        dw 0                    ; reubicaciones
        dw 2                    ; cabecera = 2 párrafos
        dw 0                    ; minalloc
        dw 0xFFFF               ; maxalloc
        dw (pila - img_start) / 16      ; SS
        dw 2                    ; SP
        dw 0
        dw code - img_start     ; IP
        dw 0                    ; CS
        dw 0x1C
        dw 0
        times 32 - ($ - hdr) db 0
img_start:
code:
        mov bx, [ss:0]          ; lo que hay en SS:0000 al llegar
        mov cx, sp
        mov di, es              ; el PSP: ningún .EXE por debajo de 64 KB
        mov sp, 0x1E            ; 30 octetos de pila: lo justo para INT 21h
        push cs
        pop ds
        mov dx, m_ok - img_start
        cmp bx, 0xBEEF
        jne .mal
        cmp cx, 2
        jne .mal
        cmp di, 0x1000
        jae .di
        mov dx, m_bajo - img_start
        jmp .di
.mal:
        mov dx, m_mal - img_start
.di:
        mov ah, 9
        int 0x21
        mov ax, 0x4C00
        int 0x21
m_ok    db "EXEPILA OK", 13, 10, "$"
m_bajo  db "EXEPILA FALLO: .EXE cargado por debajo de 64 KB (EXEPACK)", 13, 10, "$"
m_mal   db "EXEPILA FALLO: el n", 0xA3, "cleo ha tocado la pila del programa", 13, 10, "$"
        align 16, db 0
pila:
        dw 0xBEEF
        times 30 db 0
img_end:
