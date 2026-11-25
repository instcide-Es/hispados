; =============================================================================
;  MENU.COM - el menú de las aplicaciones
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;       MENU
;
;  Una lista con las aplicaciones que hay en el disco de trabajo (se mira si
;  está cada archivo: en la raíz, en \OFICINA o en \JUEGOS, como las deja
;  INSTALAR), y al final «Volver a la línea de órdenes». Se elige con las
;  flechas e Intro, o con su número. El programa se lanza desde su carpeta y,
;  al acabar, vuelve el menú. EDITAR pregunta antes qué archivo.
;  En modo de texto, sin colores: vale para cualquier tarjeta. Solo
;  instrucciones del 8086.
; =============================================================================
        cpu 8086
        org 0x100

%macro DOS 1
        mov ah, %1
        int 0x21
%endmacro

inicio:
        cld
        mov sp, pila_fin
        mov bx, (pila_fin - $$ + 0x100 + 15) / 16
        DOS 0x4A                    ; lo demás, para lo que se lance
        mov dx, dta
        DOS 0x1A
        ; ---- qué hay: por cada aplicación, el primero de sus archivos que
        ; exista (su ruta, en hay[]) ----
        mov si, apps
        xor bx, bx                  ; BX = cuántas hay
.app:
        cmp byte [si], 0
        je .listo
        mov [nombre+bx], si         ; (su nombre, por si está)
        call salta_txt              ; SI, a la primera ruta
.ruta:
        cmp byte [si], 0
        je .no_esta
        mov dx, si
        xor cx, cx
        DOS 0x4E
        jnc .esta
        call salta_txt
        jmp .ruta
.esta:
        mov [ruta+bx], si
        add bx, 2
.no_esta:
        cmp byte [si], 0            ; detrás de la última ruta
        je .fin_app
        call salta_txt
        jmp .no_esta
.fin_app:
        inc si
        jmp .app
.listo:
        mov word [nombre+bx], m_volver
        mov word [ruta+bx], 0
        shr bx, 1
        inc bx
        mov [n], bx
        mov word [sel], 0

; ---- el menú ----
pinta:
        mov ah, 0x0F                ; la pantalla, limpia (en su modo)
        int 0x10
        xor ah, ah
        int 0x10
        mov si, m_titulo
        call pon_cad
        xor bx, bx
.l:
        cmp bx, [n]
        jae .pie
        mov si, m_no_sel
        cmp bx, [sel]
        jne .m
        mov si, m_sel
.m:
        call pon_cad
        mov al, bl
        add al, '1'
        call pon_car
        mov al, ' '
        call pon_car
        call pon_car
        mov di, bx
        shl di, 1
        mov si, [nombre+di]
        call pon_cad
        mov si, m_nl
        call pon_cad
        inc bx
        jmp .l
.pie:
        mov si, m_pie
        call pon_cad
.tecla:
        xor ah, ah
        int 0x16
        mov bx, [sel]
        cmp ah, 0x48                ; arriba
        jne .t1
        dec bx
        jns .mueve
        mov bx, [n]
        dec bx
        jmp short .mueve
.t1:
        cmp ah, 0x50                ; abajo
        jne .t2
        inc bx
        cmp bx, [n]
        jb .mueve
        xor bx, bx
.mueve:
        mov [sel], bx
        jmp pinta
.t2:
        cmp al, 13
        je elige
        cmp al, 27                  ; Esc: como «Volver»
        je fuera
        sub al, '1'
        jb .tecla
        xor ah, ah
        cmp ax, [n]
        jae .tecla
        mov [sel], ax
        jmp short elige

fuera:
        mov ah, 0x0F
        int 0x10
        xor ah, ah
        int 0x10
        mov ax, 0x4C00
        int 0x21

; ---- lanza la elegida, desde su carpeta, y vuelve al menú ----
elige:
        mov bx, [sel]
        shl bx, 1
        mov si, [ruta+bx]
        test si, si
        jz fuera
        mov [lanzar], si
        mov byte [cola], 0
        mov byte [cola+1], 13
        cmp si, r_editar            ; EDITAR: ¿qué archivo?
        jne .va
        mov si, m_archivo
        call pon_cad
        mov byte [linea], 70
        mov dx, linea
        DOS 0x0A
        mov al, [linea+1]
        test al, al
        jz pinta
        inc al
        mov [cola], al
        mov byte [cola+1], ' '
        mov si, linea + 2
        mov di, cola + 2
        mov cl, [linea+1]
        xor ch, ch
        rep movsb
        mov byte [di], 13
.va:
        mov byte [actual], '\'      ; la carpeta de ahora, para volver
        mov si, actual + 1
        xor dl, dl
        DOS 0x47
        mov si, [lanzar]            ; la del programa: hasta su última barra
        mov di, carpeta
        mov bx, di
.c:
        lodsb
        stosb
        cmp al, '\'
        jne .c1
        mov bx, di
.c1:
        test al, al
        jnz .c
        cmp bx, carpeta + 1         ; (en la raíz se deja la «\»; si no, se
        ja .c2                      ; quita la del final)
        inc bx
.c2:
        mov byte [bx-1], 0
        mov dx, carpeta
        DOS 0x3B
        mov ah, 0x0F                ; la pantalla, limpia, para el programa
        int 0x10
        xor ah, ah
        int 0x10
        mov word [bloque], 0
        mov word [bloque+2], cola
        mov [bloque+4], cs
        mov bx, bloque
        mov dx, [lanzar]
        mov ax, 0x4B00
        int 0x21
        push cs
        pop ds
        push cs
        pop es
        mov sp, pila_fin
        cld
        jnc .vuelta
        mov si, m_e_lanzar
        call pon_cad
        xor ah, ah
        int 0x16
.vuelta:
        mov dx, actual
        DOS 0x3B
        mov dx, dta                 ; (el programa ha podido cambiarla)
        DOS 0x1A
        jmp pinta

; SI, detrás del 0 de su texto
salta_txt:
        lodsb
        test al, al
        jnz salta_txt
        ret
; escribe el texto de SI (acabado en 0)
pon_cad:
        lodsb
        test al, al
        jz .x
        call pon_car
        jmp pon_cad
.x:
        ret
pon_car:
        push dx
        mov dl, al
        DOS 0x02
        pop dx
        ret

; cada aplicación: su nombre y las rutas donde puede estar (acaba en un 0)
apps:
        db "Editar textos (EDITAR)", 0
r_editar db "\EDITAR.COM", 0, 0
        db "Hispa-BASIC (BASIC)", 0, "\BASIC.COM", 0, 0
        db "Pluma, el procesador de textos", 0, "\OFICINA\PLUMA.COM", 0, "\PLUMA.COM", 0, 0
        db "Cuentas, la hoja de cálculo", 0, "\OFICINA\CUENTAS.COM", 0, "\CUENTAS.COM", 0, 0
        db "Derribo (juego)", 0, "\JUEGOS\DERRIBO.COM", 0, "\DERRIBO.COM", 0, 0
        db "Marcianada (juego)", 0, "\JUEGOS\MARCIANA.COM", 0, "\MARCIANA.COM", 0, 0
        db 0
m_titulo    db 13, 10, "  Hispa-8086: las aplicaciones", 13, 10, 13, 10, 0
m_sel       db "  -> ", 0
m_no_sel    db "     ", 0
m_nl        db 13, 10, 0
m_volver    db "Volver a la línea de órdenes", 0
m_pie       db 13, 10, "  Elija con las flechas e Intro, o con su número.", 13, 10, 0
m_archivo   db 13, 10, "  Archivo que editar (Intro: ninguno): ", 0
m_e_lanzar  db 13, 10, "  No se ha podido lanzar. Pulse una tecla.", 0

        absolute $
            alignb 2
n           resw 1
sel         resw 1
lanzar      resw 1
nombre      resw 8
ruta        resw 8
bloque      resw 7
linea       resb 74
cola        resb 80
actual      resb 68
carpeta     resb 80
dta         resb 44
            resb 512
pila_fin:
