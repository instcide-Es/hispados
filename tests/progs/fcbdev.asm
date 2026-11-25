; FCBDEV.COM - escribir por FCB (15h) en CON, AUX y NUL: cada uno a su
; dispositivo (antes iba al disco de la SFT: la unidad A:).
; FCB I/O on character devices.
        cpu 386
        bits 16
        org 0x100
        mov dx, f_con
        mov si, r_con
        call uno
        mov dx, f_aux
        mov si, r_aux
        call uno
        mov dx, f_nul
        mov si, r_nul
        call uno
        mov dx, m_fin
        mov ah, 9
        int 0x21
        mov ax, 0x4C00
        int 0x21
; DX = FCB, SI = registro de 32 octetos
uno:
        push dx
        mov ah, 0x0F
        int 0x21
        pop bx
        test al, al
        jnz .mal
        mov word [bx+0x0E], 32      ; registro de 32
        mov dx, si
        mov ah, 0x1A
        int 0x21
        mov dx, bx
        mov ah, 0x15
        int 0x21
        test al, al
        jnz .mal
        mov dx, bx
        mov ah, 0x10
        int 0x21
        ret
.mal:
        mov dx, m_mal
        mov ah, 9
        int 0x21
        ret
f_con   db 0, "CON        "
        times 25 db 0
f_aux   db 0, "AUX        "
        times 25 db 0
f_nul   db 0, "NUL        "
        times 25 db 0
r_con   db "FCB-EN-CON bien.", 13, 10
        times 32-($-r_con) db ' '
r_aux   db "FCB-EN-AUX bien.", 13, 10
        times 32-($-r_aux) db ' '
r_nul   db "FCB-EN-NUL MAL!!", 13, 10
        times 32-($-r_nul) db ' '
m_mal   db "FCBDEV: fallo", 13, 10, "$"
m_fin   db "FCBDEV: fin", 13, 10, "$"
