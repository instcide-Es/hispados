; =============================================================================
;  Hispa-BASIC - la pantalla: SCREEN, COLOR, PSET, PRESET, LINE, CIRCLE,
;  PAINT, DRAW y POINT (los modos de GW-BASIC con CGA)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;    SCREEN 0   texto (40 u 80 columnas, según WIDTH)
;    SCREEN 1   320x200, 4 colores (modo 4 de la BIOS)
;    SCREEN 2   640x200, 2 colores (modo 6)
;  Los puntos se escriben directamente en la memoria de vídeo (B800h, con
;  las líneas pares y las impares en dos mitades, como en la CGA). El texto
;  sigue saliendo por la BIOS, también en los modos gráficos.
; =============================================================================

; ---- SCREEN n ----
sent_screen:
        call salta_esp
        call fin_de_sent
        je .x
        cmp al, ','
        je .resto
        call eval_ent
        cmp ax, 2
        ja .mal
        mov [v_modo], al
        push si
        mov bl, 3                   ; modo 3: texto de 80
        cmp word [v_ancho], 40
        jne .t
        mov bl, 1                   ; modo 1: texto de 40
.t:
        test al, al
        jz .pon
        mov bl, 4
        mov word [v_ancho], 40
        mov byte [v_gcolor], 3
        mov byte [v_gmask], 3
        mov word [v_gmax_x], 320
        cmp al, 1
        je .pon
        mov bl, 6
        mov word [v_ancho], 80
        mov byte [v_gcolor], 1
        mov byte [v_gmask], 1
        mov word [v_gmax_x], 640
.pon:
        mov al, bl
        xor ah, ah
        int 0x10
        mov word [v_col], 0
        ; el último punto, en el centro
        mov word [v_lp_x], 160
        cmp byte [v_modo], 2
        jne .lp
        mov word [v_lp_x], 320
.lp:
        mov word [v_lp_y], 100
        mov byte [v_attr], 7
        pop si
.resto:
        jmp sent_rem_sent
.x:
        ret
.mal:
        jmp err_funcion

; ---- COLOR [primer plano] [, fondo] ----
; en texto: el atributo con que se escribe; en SCREEN 1: COLOR fondo,
; paleta; en SCREEN 2 no vale
sent_color:
        cmp byte [v_modo], 1
        je .cga
        jb .texto
        jmp err_funcion
.texto:
        mov bl, [v_attr]
        call salta_esp
        cmp al, ','
        je .fondo
        call fin_de_sent
        je .x
        push bx
        call eval_ent
        pop bx
        cmp ax, 31
        ja .mal
        and bl, 0x70
        mov ah, al
        and ah, 0x0F
        or bl, ah
        test al, 16                 ; (16-31: parpadeando)
        jz .f0
        or bl, 0x80
.f0:
        call salta_esp
        cmp al, ','
        jne .pon
.fondo:
        inc si
        call salta_esp
        cmp al, ','
        je .pon
        call fin_de_sent
        je .pon
        push bx
        call eval_ent
        pop bx
        cmp ax, 15
        ja .mal
        and bl, 0x8F
        and al, 7
        mov cl, 4
        shl al, cl
        or bl, al
.pon:
        mov [v_attr], bl
        jmp sent_rem_sent
.x:
        ret
.mal:
        jmp err_funcion
.cga:
        call salta_esp
        cmp al, ','
        je .paleta
        call eval_ent
        mov bl, al
        and bl, 15
        mov bh, 0
        mov ah, 0x0B
        int 0x10
        call salta_esp
        cmp al, ','
        jne .x2
.paleta:
        inc si
        call eval_ent
        mov bl, al
        and bl, 1
        mov bh, 1
        mov ah, 0x0B
        int 0x10
.x2:
        jmp sent_rem_sent

; ---- escribir un carácter con el atributo de COLOR (texto, pantalla) ----
; (lo llama pon_car cuando el atributo no es el normal; AL = el carácter)
pon_car_color:
        push ax
        push bx
        push cx
        cmp al, 32
        jb .control
        mov ah, 0x09                ; con su atributo, donde está el cursor
        mov bl, [v_attr]
        xor bh, bh
        mov cx, 1
        int 0x10
.control:
        mov ah, 0x0E                ; y la BIOS avanza (y sube la pantalla)
        xor bh, bh
        mov bl, [v_attr]
        int 0x10
        pop cx
        pop bx
        pop ax
        ret

; =============================================================================
;  Los puntos
; =============================================================================
; ¿hay modo gráfico? si no, llamada no válida
pide_grafico:
        cmp byte [v_modo], 0
        je .no
        ret
.no:
        jmp err_funcion

; ---- pone el punto (CX, DX) del color AL (fuera de la pantalla, nada) ----
punto:
        push ax
        push bx
        push cx
        push dx
        push di
        push es
        cmp cx, [v_gmax_x]
        jae .x
        cmp dx, 200
        jae .x
        mov bx, 0xB800
        mov es, bx
        call dir_punto              ; DI = el octeto; CL = el desplazamiento; BL = la máscara
        and al, [v_gmask]
        shl al, cl
        not bl
        and [es:di], bl
        or [es:di], al
.x:
        pop es
        pop di
        pop dx
        pop cx
        pop bx
        pop ax
        ret
; el color del punto (CX, DX): AL (0 fuera de la pantalla)
lee_punto:
        push bx
        push cx
        push dx
        push di
        push es
        xor al, al
        cmp cx, [v_gmax_x]
        jae .x
        cmp dx, 200
        jae .x
        mov bx, 0xB800
        mov es, bx
        call dir_punto
        mov al, [es:di]
        shr al, cl
        and al, [v_gmask]
.x:
        pop es
        pop di
        pop dx
        pop cx
        pop bx
        ret
; (CX, DX) -> DI = el octeto en B800, CL = cuánto se desplaza el color,
; BL = la máscara ya desplazada (CX se pierde: quien llama lo guarda)
dir_punto:
        push ax
        push dx
        mov ax, dx
        shr ax, 1
        mov di, 80
        push dx
        mul di
        pop dx
        mov di, ax
        test dl, 1
        jz .par
        add di, 0x2000              ; las líneas impares, en la otra mitad
.par:
        mov ax, cx
        cmp byte [v_modo], 1
        jne .m6
        shr ax, 1                   ; SCREEN 1: 4 puntos por octeto
        shr ax, 1
        add di, ax
        mov al, cl
        and al, 3
        mov cl, 3
        sub cl, al
        shl cl, 1
        mov bl, 3
        shl bl, cl
        pop dx
        pop ax
        ret
.m6:
        push cx                     ; SCREEN 2: 8 puntos por octeto
        mov cl, 3
        shr ax, cl
        add di, ax
        pop cx
        mov al, cl
        and al, 7
        mov cl, 7
        sub cl, al
        mov bl, 1
        shl bl, cl
        pop dx
        pop ax
        ret

; =============================================================================
;  Las coordenadas: [STEP] (x, y) -> CX, DX (STEP: desde el último punto)
; =============================================================================
lee_xy:
        call salta_esp
        xor bl, bl
        cmp al, 0xCF                ; STEP
        jne .p
        inc si
        mov bl, 1
.p:
        push bx
        ESPERA '('
        call eval_ent
        push ax
        ESPERA ','
        call eval_ent
        push ax
        ESPERA ')'
        pop dx
        pop cx
        pop bx
        test bl, bl
        jz .x
        add cx, [v_lp_x]
        add dx, [v_lp_y]
.x:
        ret
; [, color]: AL = el color (o el de dibujo si no se da)
lee_color_opc:
        mov al, [v_gcolor]
        call salta_esp
        cmp al, ','
        mov al, [v_gcolor]
        jne .x
        inc si
        call salta_esp
        cmp al, ','
        mov al, [v_gcolor]
        je .x
        call fin_de_sent
        mov al, [v_gcolor]
        je .x
        push cx
        push dx
        call eval_ent
        pop dx
        pop cx
.x:
        ret

; ---- PSET (x,y)[,c] / PRESET (x,y)[,c] ----
sent_pset:
        call pide_grafico
        call lee_xy
        call lee_color_opc
        jmp pon_y_guarda
sent_preset:
        call pide_grafico
        call lee_xy
        mov byte [v_aux2], 0        ; (el fondo)
        call salta_esp
        cmp al, ','
        jne .ya
        call lee_color_opc
        mov [v_aux2], al
.ya:
        mov al, [v_aux2]
pon_y_guarda:
        call punto
        mov [v_lp_x], cx
        mov [v_lp_y], dx
        ret

; ---- POINT(x, y) ----
fn_point:
        call pide_grafico
        ESPERA '('
        call eval_ent
        push ax
        ESPERA ','
        call eval_ent
        push ax
        ESPERA ')'
        pop dx
        pop cx
        call lee_punto
        xor ah, ah
        jmp ac_entero

; ---- LINE [(x1,y1)]-(x2,y2)[,[c][,B[F]]] ----
sent_line_graf:
        call pide_grafico
        mov cx, [v_lp_x]
        mov dx, [v_lp_y]
        call salta_esp
        cmp al, 0xEA                ; «-»: desde el último punto
        je .hasta
        call lee_xy
.hasta:
        push cx
        push dx
        ESPERA 0xEA
        call lee_xy                 ; el final
        mov [v_lp_x], cx
        mov [v_lp_y], dx
        mov [v_g_x2], cx
        mov [v_g_y2], dx
        pop dx
        pop cx
        mov [v_g_x1], cx
        mov [v_g_y1], dx
        call lee_color_opc
        mov [v_g_col], al
        ; ¿B o BF?
        mov byte [v_g_caja], 0
        call salta_esp
        cmp al, ','
        jne .dibuja
        inc si
        call salta_esp
        call mayus
        cmp al, 'B'
        jne .dibuja
        inc si
        mov byte [v_g_caja], 1
        mov al, [si]
        call mayus
        cmp al, 'F'
        jne .dibuja
        inc si
        mov byte [v_g_caja], 2
.dibuja:
        cmp byte [v_g_caja], 0
        je .recta
        cmp byte [v_g_caja], 2
        je .llena
        ; la caja: cuatro rectas
        mov ax, [v_g_y1]
        mov [v_g_ya], ax
        mov [v_g_yb], ax
        call recta_h                ; arriba
        mov ax, [v_g_y2]
        mov [v_g_ya], ax
        call recta_h                ; abajo
        mov cx, [v_g_x1]
        call recta_v
        mov cx, [v_g_x2]
        call recta_v
        ret
.llena:
        mov ax, [v_g_y1]
        mov bx, [v_g_y2]
        cmp ax, bx
        jle .l1
        xchg ax, bx
.l1:
        mov [v_g_ya], ax
.ll:
        call recta_h
        inc word [v_g_ya]
        mov ax, [v_g_ya]
        cmp ax, bx
        jle .ll
        ret
.recta:
        mov ax, [v_g_x1]
        mov [v_g_ax], ax
        mov ax, [v_g_y1]
        mov [v_g_ay], ax
        mov ax, [v_g_x2]
        mov [v_g_bx], ax
        mov ax, [v_g_y2]
        mov [v_g_by], ax
        mov al, [v_g_col]
        jmp recta
; una horizontal de v_g_x1 a v_g_x2 en v_g_ya
recta_h:
        push ax
        push bx
        push cx
        push dx
        mov cx, [v_g_x1]
        mov bx, [v_g_x2]
        cmp cx, bx
        jle .a
        xchg cx, bx
.a:
        mov dx, [v_g_ya]
        mov al, [v_g_col]
.c:
        call punto
        inc cx
        cmp cx, bx
        jle .c
        pop dx
        pop cx
        pop bx
        pop ax
        ret
; una vertical de v_g_y1 a v_g_y2 en CX
recta_v:
        push ax
        push bx
        push dx
        mov dx, [v_g_y1]
        mov bx, [v_g_y2]
        cmp dx, bx
        jle .a
        xchg dx, bx
.a:
        mov al, [v_g_col]
.c:
        call punto
        inc dx
        cmp dx, bx
        jle .c
        pop dx
        pop bx
        pop ax
        ret

; ---- una recta de (v_g_ax, v_g_ay) a (v_g_bx, v_g_by) del color AL
; (Bresenham) ----
recta:
        push ax
        push bx
        push cx
        push dx
        push si
        push di
        mov [v_g_col], al
        mov cx, [v_g_ax]
        mov dx, [v_g_ay]
        ; dx = |bx-ax|, sx; dy = -|by-ay|, sy
        mov ax, [v_g_bx]
        sub ax, cx
        mov si, 1
        jge .x1
        neg ax
        neg si
.x1:
        mov [v_g_dx], ax
        mov [v_g_sx], si
        mov ax, [v_g_by]
        sub ax, dx
        mov si, 1
        jge .y1
        neg ax
        neg si
.y1:
        neg ax
        mov [v_g_dy], ax            ; (negativo)
        mov [v_g_sy], si
        mov di, [v_g_dx]
        add di, ax                  ; err = dx + dy
.b:
        mov al, [v_g_col]
        call punto
        cmp cx, [v_g_bx]
        jne .sig
        cmp dx, [v_g_by]
        je .fin
.sig:
        mov ax, di
        shl ax, 1                   ; e2
        cmp ax, [v_g_dy]
        jl .no_x
        add di, [v_g_dy]
        add cx, [v_g_sx]
.no_x:
        cmp ax, [v_g_dx]
        jg .b
        add di, [v_g_dx]
        add dx, [v_g_sy]
        jmp .b
.fin:
        pop di
        pop si
        pop dx
        pop cx
        pop bx
        pop ax
        ret

; ---- CIRCLE (x,y), r [, color] (la elipse con el aspecto de la pantalla:
; en SCREEN 1, 5/6; en SCREEN 2, 5/12) ----
sent_circle:
        call pide_grafico
        call lee_xy
        mov [v_g_x1], cx
        mov [v_g_y1], dx
        mov [v_lp_x], cx
        mov [v_lp_y], dx
        ESPERA ','
        call eval_ent
        test ax, ax
        js .mal
        mov [v_g_r], ax
        call lee_color_opc
        mov [v_g_col], al
        ; el radio vertical: r * 5 / 6 (o 5 / 12)
        mov ax, [v_g_r]
        mov bx, 5
        mul bx
        mov bx, 6
        cmp byte [v_modo], 2
        jne .a
        mov bx, 12
.a:
        div bx
        mov [v_g_ry], ax
        call elipse
        jmp sent_rem_sent
.mal:
        jmp err_funcion
; la elipse de centro (v_g_x1, v_g_y1) y radios v_g_r (x), v_g_ry (y): por
; cada x del cuarto, y = ry*sqrt(r²-x²)/r, y se une con el punto anterior
; con rectas (en los cuatro cuartos): así el contorno queda cerrado y PAINT
; no se escapa por las partes empinadas
elipse:
        xor cx, cx
        mov ax, [v_g_ry]
        mov [v_g_px], cx            ; el punto anterior (0, ry)
        mov [v_g_py], ax
.x:
        cmp cx, [v_g_r]
        ja .fin
        mov ax, [v_g_r]
        mul ax
        push dx
        push ax
        mov ax, cx
        mul ax
        pop bx
        sub bx, ax
        pop ax
        sbb ax, dx                  ; AX:BX = r² - x²
        mov dx, ax
        mov ax, bx
        call raiz32                 ; AX = sqrt(DX:AX)
        mul word [v_g_ry]
        cmp word [v_g_r], 0
        je .fin
        div word [v_g_r]            ; AX = y
        call cuatro_rectas          ; del anterior a (CX, AX), en los 4 cuartos
        mov [v_g_px], cx
        mov [v_g_py], ax
        inc cx
        jmp .x
.fin:
        ret
; las cuatro rectas de (±px, ±py) a (±CX, ±AX) alrededor del centro
cuatro_rectas:
        push ax
        push bx
        push cx
        push dx
        push si
        push di
        mov si, cx
        mov di, ax
        mov bx, 4                   ; los cuatro signos
.q:
        ; (sx, sy) = (+,+) (-,+) (-,-) (+,-)
        mov ax, [v_g_px]
        mov cx, si
        cmp bx, 3
        je .negx
        cmp bx, 2
        jne .x_ok
.negx:
        neg ax
        neg cx
.x_ok:
        add ax, [v_g_x1]
        add cx, [v_g_x1]
        mov [v_g_ax], ax
        mov [v_g_bx], cx
        mov ax, [v_g_py]
        mov cx, di
        cmp bx, 2
        je .negy
        cmp bx, 1
        jne .y_ok
.negy:
        neg ax
        neg cx
.y_ok:
        add ax, [v_g_y1]
        add cx, [v_g_y1]
        mov [v_g_ay], ax
        mov [v_g_by], cx
        mov al, [v_g_col]
        call recta
        dec bx
        jnz .q
        pop di
        pop si
        pop dx
        pop cx
        pop bx
        pop ax
        ret

; los cuatro puntos (x0±CX, y0±AX) (los que se salen, punto los descarta)
cuatro_puntos:
        push ax
        push bx
        push cx
        push dx
        push si
        mov si, cx                  ; lo que se aparta en x
        mov bx, ax                  ; y en y
        mov al, [v_g_col]
        mov cx, [v_g_x1]
        add cx, si
        mov dx, [v_g_y1]
        add dx, bx
        call punto
        mov cx, [v_g_x1]
        sub cx, si
        call punto
        mov dx, [v_g_y1]
        sub dx, bx
        call punto
        mov cx, [v_g_x1]
        add cx, si
        call punto
        pop si
        pop dx
        pop cx
        pop bx
        pop ax
        ret
; la raíz entera de DX:AX (hasta 2^32): AX
raiz32:
        push bx
        push cx
        push si
        push di
        mov si, ax
        mov di, dx                  ; DI:SI = n
        xor bx, bx                  ; la raíz, bit a bit (de 2^15 hacia abajo)
        mov cx, 0x8000
.b:
        mov ax, bx
        or ax, cx
        push ax
        mul ax                      ; DX:AX = prueba²
        cmp dx, di
        ja .no
        jb .si
        cmp ax, si
        ja .no
.si:
        pop bx
        jmp .s
.no:
        pop ax
.s:
        shr cx, 1
        jnz .b
        mov ax, bx
        pop di
        pop si
        pop cx
        pop bx
        ret

; ---- PAINT (x,y) [, color [, borde]]: rellena por líneas desde el punto
; hasta el borde (de serie, el mismo color) ----
sent_paint:
        call pide_grafico
        call lee_xy
        push cx
        push dx
        call lee_color_opc
        mov [v_g_col], al
        mov [v_g_borde], al
        call salta_esp
        cmp al, ','
        jne .ya
        inc si
        call eval_ent
        mov [v_g_borde], al
.ya:
        pop dx
        pop cx
        push si
        mov si, b_relleno           ; la pila de semillas
        call .mete
.saca:
        cmp si, b_relleno
        je .fin
        sub si, 4
        mov cx, [si]
        mov dx, [si + 2]
        ; ¿ya pintado o borde?
        call lee_punto
        cmp al, [v_g_borde]
        je .saca
        cmp al, [v_g_col]
        je .saca
        ; ir a la izquierda
.izq:
        test cx, cx
        jz .tramo
        dec cx
        call lee_punto
        cmp al, [v_g_borde]
        je .izq_x
        cmp al, [v_g_col]
        je .izq_x
        jmp .izq
.izq_x:
        inc cx
.tramo:
        ; pintar hacia la derecha, metiendo las semillas de arriba y abajo
        mov byte [v_g_arr], 0
        mov byte [v_g_aba], 0
.t:
        cmp cx, [v_gmax_x]
        jae .saca
        call lee_punto
        cmp al, [v_g_borde]
        je .saca
        cmp al, [v_g_col]
        je .saca
        mov al, [v_g_col]
        call punto
        ; arriba
        test dx, dx
        jz .t_ab
        dec dx
        call lee_punto
        inc dx
        cmp al, [v_g_borde]
        je .arr_no
        cmp al, [v_g_col]
        je .arr_no
        cmp byte [v_g_arr], 0
        jne .t_ab
        mov byte [v_g_arr], 1
        dec dx
        call .mete
        inc dx
        jmp .t_ab
.arr_no:
        mov byte [v_g_arr], 0
.t_ab:
        cmp dx, 199
        jae .t_sig
        inc dx
        call lee_punto
        dec dx
        cmp al, [v_g_borde]
        je .aba_no
        cmp al, [v_g_col]
        je .aba_no
        cmp byte [v_g_aba], 0
        jne .t_sig
        mov byte [v_g_aba], 1
        inc dx
        call .mete
        dec dx
        jmp .t_sig
.aba_no:
        mov byte [v_g_aba], 0
.t_sig:
        inc cx
        jmp .t
.fin:
        pop si
        jmp sent_rem_sent
.mete:
        cmp si, b_relleno + RELLENO_TAM - 4
        jae .llena
        mov [si], cx
        mov [si + 2], dx
        add si, 4
.llena:
        ret

; ---- DRAW "órdenes" (U D L R E F G H n, M x,y, B, N, C n, S n) ----
sent_draw:
        call pide_grafico
        call eval_cad               ; BX
        push si
        mov cl, [bx]
        xor ch, ch
        mov si, [bx + 1]
        mov [v_d_fin], cx
        add [v_d_fin], si
.ord:
        cmp si, [v_d_fin]
        jae .x
        lodsb
        call mayus
        cmp al, ' '
        je .ord
        cmp al, ';'
        je .ord
        cmp al, 'B'
        jne .n
        mov byte [v_d_b], 1
        jmp .ord
.n:
        cmp al, 'N'
        jne .c
        mov byte [v_d_n], 1
        jmp .ord
.c:
        cmp al, 'C'
        jne .s
        call d_num
        mov [v_gcolor], al
        jmp .ord
.s:
        cmp al, 'S'
        jne .m
        call d_num
        mov [v_d_escala], ax
        jmp .ord
.m:
        cmp al, 'M'
        jne .dir
        ; M x,y (con + o - delante: relativo)
        xor dl, dl
        call d_salta_esp
        cmp al, '+'
        je .rel
        cmp al, '-'
        jne .abs
.rel:
        mov dl, 1
.abs:
        push dx
        call d_num_s                ; con signo
        push ax
        call d_salta_esp
        cmp al, ','
        jne .m_mal
        inc si
        call d_num_s
        mov bx, ax
        pop cx
        pop dx
        test dl, dl
        jz .m_abs
        ; relativo: por la escala
        mov ax, cx
        call d_escala
        add ax, [v_lp_x]
        mov cx, ax
        mov ax, bx
        call d_escala
        add ax, [v_lp_y]
        mov bx, ax
.m_abs:
        mov dx, bx
        call d_mueve                ; a (CX, DX)
        jmp .ord
.m_mal:
        pop ax
        pop dx
        jmp .mal
.dir:
        ; U D L R E F G H: el paso (dx, dy) de cada una
        mov bx, d_dirs
.d:
        cmp byte [cs:bx], 0
        je .mal
        cmp [cs:bx], al
        je .d_es
        add bx, 3
        jmp .d
.d_es:
        push bx
        call d_num                  ; (sin número: 1)
        test ax, ax
        jnz .d_n
        inc ax
.d_n:
        call d_escala
        pop bx
        mov cx, ax
        mov al, [cs:bx + 1]
        cbw
        imul cx
        add ax, [v_lp_x]
        push ax
        mov al, [cs:bx + 2]
        cbw
        imul cx
        add ax, [v_lp_y]
        mov dx, ax
        pop cx
        call d_mueve
        jmp .ord
.x:
        pop si
        ret
.mal:
        pop si
        jmp err_funcion
; mueve (dibujando si no hay B) del último punto a (CX, DX); con N, vuelve
d_mueve:
        cmp byte [v_d_b], 0
        jne .sin
        mov ax, [v_lp_x]
        mov [v_g_ax], ax
        mov ax, [v_lp_y]
        mov [v_g_ay], ax
        mov [v_g_bx], cx
        mov [v_g_by], dx
        mov al, [v_gcolor]
        call recta
.sin:
        cmp byte [v_d_n], 0
        jne .vuelve
        mov [v_lp_x], cx
        mov [v_lp_y], dx
.vuelve:
        mov byte [v_d_b], 0
        mov byte [v_d_n], 0
        ret
; un número sin signo de la cadena de DRAW (0 si no hay)
d_num:
        call d_salta_esp
        xor ax, ax
        xor dx, dx
.c:
        cmp si, [v_d_fin]
        jae .x
        mov dl, [si]
        sub dl, '0'
        jb .x
        cmp dl, 9
        ja .x
        inc si
        push dx
        mov dx, 10
        mul dx
        pop dx
        xor dh, dh
        add ax, dx
        jmp .c
.x:
        ret
d_num_s:
        call d_salta_esp
        cmp al, '-'
        je .neg
        cmp al, '+'
        jne d_num
        inc si
        jmp d_num
.neg:
        inc si
        call d_num
        neg ax
        ret
d_salta_esp:
        cmp si, [v_d_fin]
        jae .x
        mov al, [si]
        cmp al, ' '
        jne .x
        inc si
        jmp d_salta_esp
.x:
        ret
; AX por la escala (S: en cuartos; 0 = 4). Si no cabe en 16 bits,
; «Desbordamiento» (con IDIV, un S255U600 daba INT 0: el equipo colgado)
d_escala:
        push dx
        push cx
        mov cx, [v_d_escala]
        test cx, cx
        jnz .e
        mov cx, 4
.e:
        imul cx                     ; DX:AX, entre 4 (hacia el cero, como
        test dx, dx                 ; hacía IDIV)
        jns .p
        add ax, 3
        adc dx, 0
.p:
        sar dx, 1
        rcr ax, 1
        sar dx, 1
        rcr ax, 1
        mov cx, dx                  ; ¿lo alto es solo el signo de AX?
        cwd
        cmp cx, dx
        jne .desb
        pop cx
        pop dx
        ret
.desb:
        jmp err_desb
d_dirs  db 'U', 0, -1, 'D', 0, 1, 'L', -1, 0, 'R', 1, 0
        db 'E', 1, -1, 'F', 1, 1, 'G', -1, 1, 'H', -1, -1, 0
