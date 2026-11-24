; =============================================================================
;  PLUMA - las páginas: imprimir, la vista de la página, el texto de DOS y
;  contar; y buscar y reemplazar
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  El documento se lee de principio a fin, párrafo a párrafo (a PB), y cada
;  párrafo se parte en líneas con lo mismo que la pantalla (mide y compon).
;  Las líneas se van poniendo en páginas: margen de arriba con el encabezado,
;  el cuerpo, y el margen de abajo con el pie («#» es el número de página).
;  Adónde van depende de [c_modo]: a la impresora (o a un archivo), a la
;  pantalla (la vista), a un texto sin páginas, o a ninguna parte (contar).
; =============================================================================
C_IMPRIME   equ 0
C_VISTA     equ 1
C_TEXTO     equ 2
C_CUENTA    equ 3

compone_doc:
        mov al, [ver_cod]           ; (al imprimir, los códigos no se ven)
        mov [c_vercod], al
        mov byte [ver_cod], 0
        mov word [g_ini], 0xFFFF    ; (en PB no hay hueco)
        mov word [g_fin], 0xFFFF
        xor ax, ax
        xor dx, dx
        call lee_desde
        mov al, [cab+CAB_PAG1]
        mov ah, 0
        mov [c_pag], ax
        mov word [c_lin], 0
        mov word [c_vlin], 0
        mov byte [c_parar], 0
        mov word [pb_atras], 0xFFFF
        mov byte [pb_sigue], 0
        mov al, [cab+CAB_LARGO]     ; el cuerpo de la página
        sub al, [cab+CAB_MSUP]
        jbe .c1
        sub al, [cab+CAB_MINF]
        ja .c2
.c1:
        mov al, 1
.c2:
        mov ah, 0
        mov [c_cuerpo], ax
        mov al, 3                   ; la impresora (para el texto, «solo
        cmp byte [c_modo], C_TEXTO  ; texto»), preparada
        je .d
        mov al, [cab+CAB_IMPR]
.d:
        call driver
        cmp byte [c_modo], C_IMPRIME
        jne .par
        mov si, [d_ini]
        call sal_pascal
.par:
        cmp byte [c_parar], 0
        jne .fin
        call lee_par
        jc .fin
        cmp byte [PB], 0x0C
        jne .p1
        call fin_pagina             ; un salto de página
        jmp .par
.p1:
        mov si, PB
        cmp byte [pb_sigue], 0
        je .p2
        mov [l_p], si               ; (el mismo párrafo, que no cabía)
        mov byte [l_pri], 0
        mov al, [c_estp]
        mov [l_est], al
        jmp short .lin
.p2:
        call ctx_parrafo
.lin:
        call mide
        call compon
        call linea_cuerpo
        cmp byte [c_parar], 0
        jne .fin
        mov al, [m_est]
        mov [c_estp], al
        cmp byte [m_ult], 0         ; (el párrafo acaba aquí: el siguiente,
        jne .par                    ; de PB)
        call linea_sig
        jnc .lin
        jmp .par
.fin:
        cmp word [c_lin], 0
        je .f1
        call fin_pagina
.f1:
        cmp byte [c_modo], C_IMPRIME
        jne .f2
        mov si, [d_fin]
        call sal_pascal
.f2:
        mov al, [c_vercod]
        mov [ver_cod], al
        call pon_hueco
        mov byte [filas_ok], 0
        ret

; un párrafo del documento a PB, acabado en 0. CF=1 si ya no hay nada
lee_par:
        mov al, [pb_cortado]
        mov [pb_sigue], al
        mov byte [pb_cortado], 0
        mov di, PB
.c:
        mov ax, [pb_atras]          ; (lo que se dejó la otra vez)
        cmp ax, 0xFFFF
        je .l
        mov word [pb_atras], 0xFFFF
        jmp short .t
.l:
        call lee_letra
        jc .fin
.t:
        cmp al, 0x0C
        jne .t1
        cmp di, PB                  ; el salto, solo
        jne .deja
        stosb
        jmp short .x
.deja:
        mov ah, 0
        mov [pb_atras], ax
        jmp short .x
.t1:
        stosb
        cmp al, 10
        je .x
        cmp di, PB + PB_TAM - 2     ; no cabe: se corta (por un espacio, si
        jae .corta                  ; puede ser)
        cmp di, PB + PB_TAM - 200
        jb .c
        cmp al, ' '
        jne .c
.corta:
        mov byte [pb_cortado], 1
        jmp short .x
.fin:
        cmp di, PB
        jne .x
        stc
        ret
.x:
        mov byte [di], 0
        clc
        ret

; ---- una línea del cuerpo (la de `fila`) ----
linea_cuerpo:
        cmp byte [c_modo], C_TEXTO
        je emite
        cmp word [c_lin], 0
        jne .c
        mov si, fila                ; (el encabezado usa `fila`)
        mov di, fila2
        mov cx, 80
        rep movsw
        call cabecera
        mov si, fila2
        mov di, fila
        mov cx, 80
        rep movsw
.c:
        call emite
        inc word [c_lin]
        mov ax, [c_lin]
        cmp ax, [c_cuerpo]
        jb .x
        call pie
.x:
        ret
; se acaba la página: lo que falte del cuerpo, en blanco, y el pie
fin_pagina:
        cmp byte [c_modo], C_TEXTO
        je .x
        cmp word [c_lin], 0
        jne .b
        call cabecera               ; (una página vacía también es página)
.b:
        mov ax, [c_lin]
        cmp ax, [c_cuerpo]
        jae .p
        call en_blanco
        call emite
        inc word [c_lin]
        jmp .b
.p:
        jmp pie
.x:
        ret
; el margen de arriba, con el encabezado en la penúltima de sus líneas
cabecera:
        mov cl, [cab+CAB_MSUP]
        mov ch, 0
        mov bx, cab + CAB_ENC
        mov dx, cx
        sub dx, 2                   ; DX = la línea del encabezado
        jmp margen
; el de abajo, con el pie en su segunda línea; y la página siguiente
pie:
        mov cl, [cab+CAB_MINF]
        mov ch, 0
        mov bx, cab + CAB_PIE
        mov dx, 1
        call margen
        mov word [c_lin], 0
        inc word [c_pag]
        cmp byte [c_modo], C_VISTA
        jne .imp
        mov di, fila                ; en la vista, una raya entre páginas
        mov cx, 80
        mov ax, F_CRUDO * 256 + 0xCD
        rep stosw
        mov si, m_v_pagina
        mov di, fila + 2 * 30
        mov ah, F_CRUDO
.r:
        lodsb
        test al, al
        jz .r1
        stosw
        jmp .r
.r1:
        mov ax, [c_pag]
        dec ax
        call .num
        jmp emite
.imp:
        cmp byte [c_modo], C_IMPRIME
        jne .x
        mov al, 12                  ; a la hoja siguiente
        call sal_letra
        cmp byte [cab+CAB_HOJAS], 0 ; hojas sueltas: se espera a que pongan
        je .x                       ; otra (si es la impresora)
        cmp byte [c_disp], 0
        je .x
        call sal_vacia
        mov si, m_otra_hoja
        call pregunta
        cmp al, 27
        jne .x
        mov byte [c_parar], 1
.x:
        ret
.num:
        mov bx, 10
        xor cx, cx
.n1:
        xor dx, dx
        div bx
        push dx
        inc cx
        test ax, ax
        jnz .n1
.n2:
        pop ax
        add al, '0'
        mov ah, F_CRUDO
        stosw
        loop .n2
        ret
; CX líneas de margen; en la DX (desde 0), el texto de BX (si lo hay)
margen:
        mov [mg_n], cx
        mov [mg_t], bx
        mov [mg_d], dx
        mov word [mg_i], 0
.c:
        mov ax, [mg_i]
        cmp ax, [mg_n]
        jae .x
        call en_blanco
        mov ax, [mg_i]
        cmp ax, [mg_d]
        jne .e
        mov bx, [mg_t]
        cmp byte [bx], 0
        je .e
        call .texto
.e:
        call emite
        inc word [mg_i]
        jmp .c
.x:
        ret
; (interna) el texto de BX en `fila`, centrado en el ancho, con «#» = página
.texto:
        push si
        push di
        push bx
        mov si, bx
        mov di, tmp
.t:
        lodsb
        cmp al, '#'
        jne .t1
        mov ax, [c_pag]
        call num_a_texto
        jmp .t
.t1:
        stosb
        test al, al
        jnz .t
        mov cx, di
        sub cx, tmp + 1             ; CX = lo que mide
        mov al, [cab+CAB_ANCHO]
        mov ah, 0
        sub ax, cx
        jae .t2
        xor ax, ax
.t2:
        shr ax, 1
        mov di, ax
        shl di, 1
        add di, fila
        mov si, tmp
.t3:
        lodsb
        test al, al
        jz .t4
        cmp di, fila + 158
        jae .t4
        mov ah, 0
        stosw
        jmp .t3
.t4:
        pop bx
        pop di
        pop si
        ret
en_blanco:
        mov di, fila
        mov cx, 80
        mov ax, ' '
        rep stosw
        ret

; ---- `fila`, adonde vaya ----
emite:
        mov al, [c_modo]
        cmp al, C_CUENTA
        je .x
        cmp al, C_VISTA
        je a_vista
        jmp a_impresora
.x:
        ret
; en la vista: la línea número c_vlin, si cae en la pantalla
a_vista:
        mov ax, [c_vlin]
        inc word [c_vlin]
        sub ax, [v_top]
        jb .x
        cmp ax, 23
        jae .para
        inc ax                      ; (fila 1 a 23)
        push ax
        ; con el margen izquierdo del papel delante (lo que quepa)
        mov cl, [cab+CAB_MIZQ]
        mov al, 79
        sub al, [cab+CAB_ANCHO]
        cmp cl, al
        jbe .m
        mov cl, al
.m:
        mov ch, 0
        jcxz .v
        mov si, fila + 2 * 79       ; las casillas, CX más a la derecha
        sub si, cx
        sub si, cx
        mov di, fila + 2 * 79
        push cx
        mov dx, 80
        sub dx, cx
        mov cx, dx
        std
        rep movsw
        cld
        pop cx
        mov di, fila
        mov ax, ' '
        rep stosw
.v:
        pop ax
        jmp vuelca
.para:
        mov byte [c_parar], 1       ; (ya está la pantalla llena)
.x:
        ret
; a la impresora (o al texto de DOS): con el driver de [cab+CAB_IMPR]
a_impresora:
        mov ah, 1                   ; ¿Esc? Se deja
        int 0x16
        jz .s
        call lee_tecla
        cmp al, 27
        jne .s
        mov byte [c_parar], 1
        ret
.s:
        mov bx, 156                 ; hasta la última que no sea espacio (la
                                    ; columna 79 es la de las marcas)
.u:
        cmp byte [fila+bx], ' '
        jne .u1
        sub bx, 2
        jae .u
.u1:
        add bx, 2
        mov [c_hasta], bx
        cmp byte [c_modo], C_TEXTO
        je .c0
        test bx, bx                 ; el margen izquierdo del papel
        jz .c0
        mov cl, [cab+CAB_MIZQ]
        mov ch, 0
        jcxz .c0
.m:
        mov al, ' '
        call sal_letra
        loop .m
.c0:
        mov byte [d_est], 0
        xor bx, bx
.c:
        cmp bx, [c_hasta]
        jae .fin
        mov ax, [fila+bx]
        cmp byte [c_modo], C_TEXTO
        je .letra
        mov dl, ah                  ; ¿cambia el formato?
        and dl, 7
        call pon_formato
.letra:
        push bx
        call imp_letra
        pop bx
        add bx, 2
        jmp .c
.fin:
        mov dl, 0
        call pon_formato
        jmp sal_crlf
; deja la impresora con el formato DL (bits): apaga y enciende lo que cambie
pon_formato:
        push ax
        push bx
        mov al, [d_est]
        xor al, dl
        jz .x
        mov [d_est], dl
        mov bx, 0
.c:
        test al, [.bits+bx]
        jz .s
        push ax
        mov si, bx                  ; la orden: encender o apagar
        shl si, 1
        shl si, 1
        test dl, [.bits+bx]
        jnz .on
        add si, 2
.on:
        mov si, [d_cods+si]
        call sal_pascal
        pop ax
.s:
        inc bx
        cmp bx, 3
        jb .c
.x:
        pop bx
        pop ax
        ret
.bits       db 1, 2, 4
; una letra (AL, de la 858) para la impresora de [d_tipo]
imp_letra:
        cmp al, 0x80
        jae .alta
        jmp sal_letra
.alta:
        mov [d_ch], al
        cmp byte [d_tipo], 3        ; solo texto: en la 437, sin más
        je a_dos_sal
        cmp al, C_EURO
        jne .ne
        mov al, 'C'                 ; €: una C con un = encima
        call sal_letra
        mov al, 8
        call sal_letra
        mov al, '='
        jmp sal_letra
.ne:
        cmp byte [d_tipo], 0
        jne .pc
        ; Epson FX con el juego español (ESC R 7): ñ Ñ ¿ ¡ en lugar de | \ ] [
        mov si, nac_es
.n:
        lodsw
        test al, al
        jz .sobre
        cmp al, [d_ch]
        jne .n
        mov al, ah
        jmp sal_letra
.pc:
        mov al, [d_ch]
        mov bx, a437 - 0x80         ; las de la tabla PC 437
        xlat
        test al, al
        jnz .y
.sobre:
        ; la letra sin tilde, un retroceso y la tilde encima
        mov al, [d_ch]
        mov bx, base - 0x80
        xlat
        test al, al
        jnz .b
        mov al, '?'
.b:
        call sal_letra
        mov al, [d_ch]
        mov bx, marca - 0x80
        xlat
        test al, al
        jz .x
        cmp byte [d_tipo], 0
        jne .b1
        cmp al, '"'                 ; (la diéresis, la del juego español)
        jne .b1
        mov al, 0x7B
.b1:
        push ax
        mov al, 8
        call sal_letra
        pop ax
.y:
        jmp sal_letra
.x:
        ret
a_dos_sal:
        call a_dos
        jmp sal_letra
; las letras del juego España I de Epson (ESC R 7), y dónde están
nac_es      db 0xA4, 0x7C, 0xA5, 0x5C, 0xA8, 0x5D, 0xAD, 0x5B, 0
; un texto que empieza por su largo
sal_pascal:
        lodsb
        mov cl, al
        mov ch, 0
        jcxz .x
.c:
        lodsb
        call sal_letra
        loop .c
.x:
        ret
; deja en d_* lo de la impresora AL
driver:
        mov [d_tipo], al
        mov ah, 0
        mov bx, ax
        shl bx, 1
        mov si, [drv_ini+bx]
        mov [d_ini], si
        mov si, [drv_fin+bx]
        mov [d_fin], si
        mov si, [drv_cods+bx]
        mov di, d_cods
        mov cx, 6
        rep movsw
        ret
drv_ini     dw d_fx_ini, d_lx_ini, d_ibm_ini, d_nada
drv_fin     dw d_ep_fin, d_ep_fin, d_nada, d_nada
drv_cods    dw d_ep_cods, d_ep_cods, d_ibm_cods, d_no_cods
d_nada      db 0
d_fx_ini    db 5, 27, '@', 27, 'R', 7               ; iniciar; juego España I
d_lx_ini    db 7, 27, '@', 27, 't', 1, 27, '6'      ; la tabla PC 437
d_ibm_ini   db 2, 27, '6'                           ; el juego de caracteres 2
d_ep_fin    db 2, 27, '@'
d_ep_cods   dw d_n_si, d_n_no, d_s_si, d_s_no, d_c_si, d_c_no
d_ibm_cods  dw d_n_si, d_n_no, d_s_si, d_s_no, d_nada, d_nada
d_no_cods   dw d_nada, d_nada, d_nada, d_nada, d_nada, d_nada
d_n_si      db 2, 27, 'E'
d_n_no      db 2, 27, 'F'
d_s_si      db 3, 27, '-', 1
d_s_no      db 3, 27, '-', 0
d_c_si      db 2, 27, '4'
d_c_no      db 2, 27, '5'

; =============================================================================
;  Imprimir (^KP): en la impresora (PRN) o en un archivo
; =============================================================================
imprimir:
        mov si, m_imprimir
        mov di, imp_nombre
        mov cl, 64
        call pide
        jc .x
        mov dx, imp_nombre          ; ¿un dispositivo que ya existe (PRN)?
        mov ax, 0x3D01
        int 0x21
        jnc .ab
        xor cx, cx
        DOS 0x3C
        jc .mal
.ab:
        mov bx, ax
        call sal_a
        mov ax, 0x4400              ; ¿es la impresora (o un archivo)?
        int 0x21
        mov byte [c_disp], 0
        jc .p
        test dl, 0x80
        jz .p
        mov byte [c_disp], 1
.p:
        mov si, m_imprimiendo
        call pinta_abajo
        mov byte [c_modo], C_IMPRIME
        call compone_doc
        call sal_fin
        pushf
        mov bx, [s_h]
        DOS 0x3E
        popf
        jc .mal
        mov word [mensaje], m_impreso
.x:
        ret
.mal:
        mov si, m_e_imprimir
        jmp aviso

; =============================================================================
;  La vista de la página (F9): como saldrá, con sus márgenes, el encabezado y
;  el pie, y una raya entre página y página
; =============================================================================
vista:
        mov word [v_top], 0
.p:
        push es                     ; la pantalla, en blanco
        mov es, [vseg]
        xor di, di
        mov cx, 25 * 80
        mov ax, 0x0720
        rep stosw
        pop es
        mov si, m_vista
        call pinta_abajo
        mov ax, 0                   ; arriba, lo que es
        mov si, m_vista_arriba
        mov di, fila
        mov cx, 80
        push ax
        mov ax, F_CRUDO * 256 + ' '
        rep stosw
        pop ax
        mov di, fila
.t:
        lodsb
        test al, al
        jz .t1
        mov ah, F_CRUDO
        stosw
        jmp .t
.t1:
        mov ax, 0
        call vuelca_inverso
        mov byte [c_modo], C_VISTA
        call compone_doc
        mov dx, 0x184F
        call pon_cursor
        call lee_tecla
        mov bx, [v_top]
        cmp al, 27
        je .x
        test al, al
        jnz .p
        cmp ah, 0x43                ; F9 otra vez: se sale
        je .x
        mov cl, [cab+CAB_LARGO]     ; una página (y su raya)
        mov ch, 0
        inc cx
        cmp ah, 0x50
        jne .k1
        inc bx
.k1:
        cmp ah, 0x48
        jne .k2
        dec bx
.k2:
        cmp ah, 0x51
        jne .k3
        add bx, cx
.k3:
        cmp ah, 0x49
        jne .k4
        sub bx, cx
.k4:
        cmp ah, 0x47
        jne .k5
        xor bx, bx
.k5:
        test bx, bx
        jns .k6
        xor bx, bx
.k6:
        mov [v_top], bx
        jmp .p
.x:
        mov byte [filas_ok], 0
        ret

; =============================================================================
;  Contar: palabras, letras, párrafos y páginas
; =============================================================================
contar:
        mov si, m_contando
        call pinta_abajo
        xor ax, ax
        mov [k_pal], ax
        mov [k_pal+2], ax
        mov [k_let], ax
        mov [k_let+2], ax
        mov [k_par], ax
        mov [k_par+2], ax
        mov byte [k_dentro], 0
        xor dx, dx
        call lee_desde
        mov byte [k_ultima], 10
.c:
        call lee_letra
        jc .fin
        mov [k_ultima], al
        cmp al, 10
        jne .c1
        add word [k_par], 1
        adc word [k_par+2], 0
.c1:
        call es_codigo
        jc .c
        call es_de_palabra
        jnc .no
        cmp byte [k_dentro], 0
        jne .l
        mov byte [k_dentro], 1
        add word [k_pal], 1
        adc word [k_pal+2], 0
        jmp short .l
.no:
        mov byte [k_dentro], 0
        cmp al, ' '
        jbe .c
.l:
        add word [k_let], 1
        adc word [k_let+2], 0
        jmp .c
.fin:
        cmp byte [k_ultima], 10     ; (el último párrafo, sin su fin)
        je .f1
        add word [k_par], 1
        adc word [k_par+2], 0
.f1:
        mov byte [c_modo], C_CUENTA
        call compone_doc
        mov al, [cab+CAB_PAG1]
        mov ah, 0
        neg ax
        add ax, [c_pag]
        mov [k_pags], ax
        ; «1.234 palabras, 6.789 letras, 45 párrafos y 3 páginas»
        mov di, lin_abajo
        mov ax, [k_pal]
        mov dx, [k_pal+2]
        mov si, m_k_pal
        call .uno
        mov ax, [k_let]
        mov dx, [k_let+2]
        mov si, m_k_let
        call .uno
        mov ax, [k_par]
        mov dx, [k_par+2]
        mov si, m_k_par
        call .uno
        mov ax, [k_pags]
        xor dx, dx
        mov si, m_k_pag
        cmp ax, 1
        jne .pl
        mov si, m_k_pag1
.pl:
        call .uno
        mov byte [di], 0
        mov si, lin_abajo
        jmp aviso
.uno:
        push si
        call num32_a_texto
        pop si
        jmp copia_z

; =============================================================================
;  Buscar (F3, ^QF; otra vez: F3 e Intro, o ^L) y reemplazar (F8, ^QA). Sin
;  mirar mayúsculas ni tildes (canción = CANCION), pero la ñ es la ñ
; =============================================================================
buscar:
        mov si, m_buscar
        mov di, busca
        mov cl, 30
        call pide
        jc buscar_sig.x
buscar_sig:
        call busca_desde_cursor
        jnc .esta
        mov byte [bs_cero], 1       ; no está detrás: desde el principio
        call busca_desde_cursor
        jc .no
.esta:
        mov ax, [bs_ini]
        mov dx, [bs_ini+2]
        jmp va_pos
.no:
        mov word [mensaje], m_no_esta
.x:
        ret
; busca [busca] desde detrás del cursor: bs_ini y bs_fin. CF=1 si no está
busca_desde_cursor:
        mov di, busca
        call largo_z
        mov [bs_n], cx
        test cx, cx
        jnz .hay
        jmp .no
.hay:
        mov si, busca               ; lo que se busca, ya sin tildes
        mov di, bs_pat
        mov bx, pliega
.p:
        lodsb
        xlat
        stosb
        loop .p
        call cursor32
        add ax, 1
        adc dx, 0
        cmp byte [bs_cero], 0       ; (reemplazar empieza por el principio)
        je .d
        mov byte [bs_cero], 0
        xor ax, ax
        xor dx, dx
.d:
        call lee_desde
        mov word [bs_k], 0          ; las letras vistas (en el anillo)
.c:
        mov ax, [it_pos]
        mov dx, [it_pos+2]
        mov [bs_pos], ax
        mov [bs_pos+2], dx
        call lee_letra
        jc .no
        call es_codigo
        jc .c
        mov bx, pliega
        xlat
        mov bx, [bs_k]              ; al anillo: la letra y dónde estaba
        and bx, 31
        mov [bs_ani+bx], al
        shl bx, 1
        shl bx, 1
        mov cx, [bs_pos]
        mov [bs_dnd+bx], cx
        mov cx, [bs_pos+2]
        mov [bs_dnd+bx+2], cx
        inc word [bs_k]
        mov cx, [bs_n]
        cmp [bs_k], cx
        jb .c
        mov bx, cx                  ; ¿es la última de lo que se busca?
        cmp al, [bs_pat+bx-1]
        jne .c
        ; se comparan las bs_n últimas, de atrás adelante
        mov dx, [bs_k]
        dec dx
        lea si, [bs_pat+bx-1]
.cmp:
        mov bx, dx
        and bx, 31
        mov al, [bs_ani+bx]
        cmp al, [si]
        jne .c
        dec si
        dec dx
        loop .cmp
        inc dx                      ; DX = la primera
        mov bx, dx
        and bx, 31
        shl bx, 1
        shl bx, 1
        mov ax, [bs_dnd+bx]
        mov [bs_ini], ax
        mov ax, [bs_dnd+bx+2]
        mov [bs_ini+2], ax
        mov ax, [bs_pos]
        mov dx, [bs_pos+2]
        add ax, 1
        adc dx, 0
        mov [bs_fin], ax
        mov [bs_fin+2], dx
        clc
        ret
.no:
        stc
        ret
reemplazar:
        mov si, m_buscar
        mov di, busca
        mov cl, 30
        call pide
        jc .x
        mov si, m_por
        mov di, cambio
        mov cl, 30
        call pide
        jc .x
        mov word [r_cuenta], 0
        mov byte [r_todos], 0
        mov byte [bs_cero], 1       ; desde el principio del documento
.otro:
        call busca_desde_cursor
        jc .fin
        mov ax, [bs_ini]
        mov dx, [bs_ini+2]
        call ir_a
        cmp byte [r_todos], 0
        jne .si
        mov byte [centrar], 1       ; se enseña y se pregunta
        mov ax, [bs_ini]
        mov [b_ini], ax
        mov ax, [bs_ini+2]
        mov [b_ini+2], ax
        mov ax, [bs_fin]
        mov [b_fin], ax
        mov ax, [bs_fin+2]
        mov [b_fin+2], ax
        call pinta
        call desmarca
        mov si, m_reempl
        call pregunta
        cmp al, 'T'
        jne .r1
        mov byte [r_todos], 1
        jmp short .si
.r1:
        cmp al, 'S'
        je .si
        cmp al, 'N'
        jne .fin
        jmp .otro
.si:
        mov cx, [bs_fin]            ; fuera lo encontrado...
        sub cx, [bs_ini]
        call borra_delante
        mov si, cambio              ; ...y en su sitio, lo nuevo
        mov di, si
        call largo_z
        call inserta
        jc .fin
        inc word [r_cuenta]
        call cursor32               ; (se sigue detrás de lo puesto)
        sub ax, 1
        sbb dx, 0
        call ir_a
        jmp .otro
.fin:
        mov ax, [r_cuenta]
        mov di, lin_abajo
        call num_a_texto
        mov si, m_cambiados
        call copia_z
        mov byte [di], 0
        mov word [mensaje], lin_abajo
        mov byte [centrar], 1
.x:
        ret
