; =============================================================================
;  N8.COM - prueba de lo nuevo del núcleo en la versión 0.8
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  1. INT 25h e INT 26h (sectores «en crudo»): lee el primer sector de la
;     unidad de trabajo y mira que es un sector de arranque; escribe un dibujo
;     en los dos últimos sectores del disco, los vuelve a leer y compara.
;  2. FILES= de CONFIG.SYS: abre LEEME.TXT una y otra vez hasta que el núcleo
;     dice que no caben más archivos abiertos, y escribe cuántos han cabido.
; =============================================================================
        cpu 8086
        org 0x100

%macro DOS 1
        mov ah, %1
        int 0x21
%endmacro

inicio:
        cld
        DOS 0x19
        mov [unidad], al
        mov cx, 1                   ; el sector de arranque
        xor dx, dx
        mov bx, buf
        int 0x25
        pop dx                      ; (las banderas que deja en la pila)
        jc .mal
        cmp word [buf+510], 0xAA55
        jne .mal
        cmp word [buf+11], 512
        jne .mal
        mov ax, [buf+19]            ; los dos últimos sectores del disco
        sub ax, 2
        mov [ultimo], ax
        mov di, buf
        mov cx, 1024
        mov al, 0x5A
.d:
        stosb
        add al, 7
        loop .d
        mov al, [unidad]
        mov cx, 2
        mov dx, [ultimo]
        mov bx, buf
        int 0x26
        pop dx
        jc .mal
        mov al, [unidad]
        mov cx, 2
        mov dx, [ultimo]
        mov bx, buf2
        int 0x25
        pop dx
        jc .mal
        mov si, buf
        mov di, buf2
        mov cx, 512
        repe cmpsw
        jne .mal
        mov al, 9                   ; una unidad que no existe: error
        mov cx, 1
        xor dx, dx
        mov bx, buf2
        int 0x25
        pop dx
        jnc .mal
        mov dx, m_bien
        jmp short .dice
.mal:
        mov dx, m_mal
.dice:
        DOS 0x09
        ; ---- cuántos archivos caben abiertos ----
        xor bp, bp
.abre:
        mov dx, t_leeme
        mov al, 0
        DOS 0x3D
        jc .cuenta
        inc bp
        cmp bp, 200
        jb .abre
.cuenta:
        mov ax, bp
        mov bl, 10
        div bl
        add ax, '00'
        mov [m_caben.n], ax
        mov dx, m_caben
        DOS 0x09
        mov ax, 0x4C00              ; (al terminar, el núcleo los cierra)
        int 0x21

m_bien      db "N8: sectores en crudo bien", 13, 10, "$"
m_mal       db "N8: sectores en crudo MAL", 13, 10, "$"
m_caben     db "N8: caben "
.n          db "00 archivos abiertos", 13, 10, "$"
t_leeme     db "\LEEME.TXT", 0
unidad      db 0
ultimo      dw 0

        absolute $
buf         resb 1024
buf2        resb 1024
