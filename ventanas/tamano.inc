; =============================================================================
;  Hispa Ventanas - [TAMAÑO] cambiar el tamaño de las ventanas con el ratón,
;  formas del cursor, arrastrar solo el contorno, la rueda del ratón y la
;  barra de desplazamiento (c_barra), con su uso en el Centro de programas
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Va en la parte alta (32 bits, sin el límite de 64 KB). Lo llama el gestor
;  (gestor.inc), el ratón (raton.inc, ventanas.asm) y el Centro (centro.inc).
;
;  Cambiar el tamaño: los bordes y las esquinas se agarran AGARRE puntos por
;  dentro y AGARRE_FUERA por fuera (ges_donde). Al empezar, el gestor pregunta
;  a la ventana su tamaño mínimo (WM_GETMINMAXINFO); mientras se arrastra, la
;  ventana recibe WM_SIZE en cada paso, o, en los equipos lentos (o con
;  /CONTORNO, AJ_CONTORNO), solo se dibuja un contorno y la ventana cambia una
;  vez, al soltar. Al soltar recibe WM_EXITSIZEMOVE (ECX=1 si cambió el tamaño).
;
;    cur_preparar     (rat_iniciar) los dibujos del cursor, a colores
;    cur_forma        EBP=ventana EAX=CUR_*: la forma del cursor sobre su zona
;    tam_cursor_zona  (gestor) ECX=zona EBP EDI de ges_donde: pone la forma
;    tam_empezar      (gestor) EBP=ventana, al empezar a moverla o a cambiarla
;    tam_aplicar      (gestor) EAX..EDX=rectángulo nuevo: la ventana o el contorno
;    tam_acabar       (gestor) al soltar
;    con_quitar       quita el contorno, si está
;    rueda_mascara    (rat_iniciar) si el driver da la rueda, se le pide
;    ges_rueda        (eventos) EAX=pasos de la rueda: WM_MOUSEWHEEL
;    c_barra          la clase de la barra de desplazamiento (BDS_HORIZ:
;                     horizontal); mensajes BDM_* (defs.inc)
; =============================================================================

; ---- los dibujos del cursor (16x16; «X» tinta, «o» ocre, «.» nada) ----
cd_horiz:
        db "................"
        db "................"
        db "................"
        db "....X.....X....."
        db "...XoX...XoX...."
        db "..XooXXXXXooX..."
        db ".XoooooooooooX.."
        db "XoooooooooooooX."
        db "XoooooooooooooX."
        db ".XoooXXXXXoooX.."
        db "..XooX...XooX..."
        db "...XoX...XoX...."
        db "....X.....X....."
        db "................"
        db "................"
        db "................"
cd_vert:
        db ".......XX......."
        db "......XooX......"
        db ".....XooooX....."
        db "....XooooooX...."
        db "...XooooooooX..."
        db "....XXoooXXX...."
        db ".....XoooX......"
        db ".....XoooX......"
        db ".....XoooX......"
        db "....XXoooXXX...."
        db "...XooooooooX..."
        db "....XooooooX...."
        db ".....XooooX....."
        db "......XooX......"
        db ".......XX......."
        db "................"
cd_diag1:
        db "................"
        db "................"
        db "...XXXXXX......."
        db "..XooooooX......"
        db "..XoooooX......."
        db "..XooooX........"
        db "..XoooooX...X..."
        db "..XooXoooX.XoX.."
        db "..XoX.XoooXooX.."
        db "...X...XoooooX.."
        db "........XooooX.."
        db ".......XoooooX.."
        db "......XooooooX.."
        db ".......XXXXXX..."
        db "................"
        db "................"
cd_diag2:
        db "................"
        db "................"
        db ".......XXXXXX..."
        db "......XooooooX.."
        db ".......XoooooX.."
        db "........XooooX.."
        db "...X...XoooooX.."
        db "..XoX.XoooXooX.."
        db "..XooXoooX.XoX.."
        db "..XoooooX...X..."
        db "..XooooX........"
        db "..XoooooX......."
        db "..XooooooX......"
        db "...XXXXXX......."
        db "................"
        db "................"

; por forma: dibujo, ancho del dibujo, filas, punta x, punta y
CT_TAM      equ 20
cur_tabla:
        dd cursor_dib, 12, 18, 0, 0
        dd cd_horiz, 16, 16, 7, 7
        dd cd_vert, 16, 16, 7, 7
        dd cd_diag1, 16, 16, 7, 7
        dd cd_diag2, 16, 16, 8, 7
CUR_IMG     equ CUR_ANCHO * CUR_ALTO

cur_preparar:
        pushad
        mov edi, cur_imgs
        mov ecx, CUR_N * CUR_IMG
        mov al, CLAVE
        rep stosb
        mov ebx, cur_tabla
        xor edx, edx
.f:
        mov esi, [ebx]
        imul edi, edx, CUR_IMG
        add edi, cur_imgs
        mov ebp, [ebx+8]
.fila:
        mov ecx, [ebx+4]
        push edi
.c:
        lodsb
        cmp al, '.'
        je .s
        mov ah, C_TINTA
        cmp al, 'X'
        je .p
        mov ah, C_OCRE
.p:
        mov [edi], ah
.s:
        inc edi
        loop .c
        pop edi
        add edi, CUR_ANCHO
        dec ebp
        jnz .fila
        add ebx, CT_TAM
        inc edx
        cmp edx, CUR_N
        jb .f
        mov dword [cur_forma_v], -1
        xor eax, eax
        call cur_poner
        mov al, [op_contorno]       ; /CONTORNO
        mov [aj_contorno], al
        popad
        ret

; (interna) EAX=forma: la pone (y el cursor se dibuja otra vez)
cur_poner:
        cmp eax, [cur_forma_v]
        je .x
        pushad
        cmp eax, CUR_N
        jb .a
        xor eax, eax
.a:
        mov [cur_forma_v], eax
        imul ebx, eax, CUR_IMG
        add ebx, cur_imgs
        mov [cur_img_p], ebx
        imul ebx, eax, CT_TAM
        mov ecx, [cur_tabla+ebx+12]
        mov [cur_hx], ecx
        mov ecx, [cur_tabla+ebx+16]
        mov [cur_hy], ecx
        cmp byte [cur_puesto], 0    ; donde estaba, a presentar otra vez
        je .b
        mov eax, [cur_x]
        mov ebx, [cur_y]
        lea ecx, [eax+CUR_ANCHO]
        lea edx, [ebx+CUR_ALTO]
        call pres_unir
.b:
        mov byte [actividad], 1
        popad
.x:
        ret

; (interfaz) EBP=ventana EAX=forma: el cursor tiene esa forma sobre la zona
; de la ventana (hasta que la pida otra o se destruya)
cur_forma:
        pushad
        cmp eax, CUR_N
        jb .a
        xor eax, eax
.a:
        mov [cur_duena], ebp
        mov [cur_pedida], eax
        cmp byte [arrastre], 0
        jne .x
        mov eax, [rat_x]
        mov ebx, [rat_y]
        call ges_donde
        call tam_cursor_zona
.x:
        popad
        ret

; (gestor) la forma según la zona ECX (de ges_donde: EBP ventana, EDI hija)
tam_cursor_zona:
        pushad
        xor eax, eax
        cmp ecx, Z_BORDE
        jb .cli
        test dword [ebp+V_ESTADO], S_INACTIVA
        jnz .pon
        sub ecx, Z_BORDE
        mov eax, CUR_HORIZ
        test ecx, 12
        jz .pon
        mov eax, CUR_VERT
        test ecx, 3
        jz .pon
        mov eax, CUR_DIAG1
        cmp ecx, 5
        je .pon
        cmp ecx, 10
        je .pon
        mov eax, CUR_DIAG2
        jmp .pon
.cli:
        cmp ecx, Z_CLIENTE
        jne .pon
        cmp edi, [cur_duena]
        jne .pon
        mov eax, [cur_pedida]
.pon:
        call cur_poner
        popad
        ret

; ---- mover y cambiar el tamaño ----
; (gestor) EBP=ventana; [arrastre] = 1 mover, 2 cambiar el tamaño
tam_empezar:
        pushad
        mov dword [arr_min_an], MIN_AN
        mov dword [arr_min_al], MIN_AL
        cmp byte [arrastre], 2
        jne .r
        mov eax, WM_GETMINMAXINFO   ; su tamaño mínimo
        xor ecx, ecx
        xor edx, edx
        call msg_enviar
        movzx ecx, ax
        cmp ecx, 40
        jb .m1
        mov [arr_min_an], ecx
.m1:
        shr eax, 16
        cmp eax, 40
        jb .r
        mov [arr_min_al], eax
.r:
        mov eax, [ebp+V_X]
        mov [arr_ux], eax
        mov eax, [ebp+V_Y]
        mov [arr_uy], eax
        mov eax, [ebp+V_AN]
        mov [arr_uan], eax
        mov [arr_ian], eax
        mov eax, [ebp+V_AL]
        mov [arr_ual], eax
        mov [arr_ial], eax
        mov byte [con_activo], 0    ; ¿solo el contorno?
        mov al, [aj_contorno]
        cmp al, 1
        je .si
        cmp al, 2
        je .x
        cmp byte [vel_medido], 0
        jne .m
        call vel_medir
.m:
        cmp byte [vel_lento], 0
        je .x
.si:
        mov byte [con_activo], 1
        mov eax, [arr_ux]
        mov ebx, [arr_uy]
        mov ecx, [arr_uan]
        mov edx, [arr_ual]
        call con_dibujar
.x:
        popad
        ret

; (interna) ¿es lenta la pantalla? Cuántas veces se presenta entera en dos
; tics de reloj (110 ms): menos de 4 (más de 27 ms cada una), lenta
vel_medir:
        pushad
        mov byte [vel_medido], 1
        call tics
        mov ebx, eax
.w:
        call tics
        cmp eax, ebx
        je .w
        mov edi, eax
        xor esi, esi
.p:
        xor eax, eax
        xor ebx, ebx
        mov ecx, [vid_ancho]
        mov edx, [vid_alto]
        call vid_presentar
        inc esi
        cmp esi, 4
        jae .x
        call tics
        sub eax, edi
        cmp eax, 2
        jb .p
        mov byte [vel_lento], 1
.x:
        popad
        ret

; (gestor) EBP=ventana EAX=x EBX=y ECX=ancho EDX=alto: así queda
tam_aplicar:
        cmp eax, [arr_ux]
        jne .a
        cmp ebx, [arr_uy]
        jne .a
        cmp ecx, [arr_uan]
        jne .a
        cmp edx, [arr_ual]
        je .x
.a:
        mov [arr_ux], eax
        mov [arr_uy], ebx
        mov [arr_uan], ecx
        mov [arr_ual], edx
        cmp byte [con_activo], 0
        jne con_dibujar
        jmp ven_mover
.x:
        ret

; (gestor) se ha soltado el botón al mover o cambiar el tamaño
tam_acabar:
        pushad
        mov ebp, [arr_ven]
        cmp byte [con_activo], 0
        je .msg
        call con_quitar
        mov byte [con_activo], 0
        test ebp, ebp
        jz .fin
        cmp dword [ebp+V_USO], 1
        jne .fin
        mov eax, [arr_ux]
        mov ebx, [arr_uy]
        mov ecx, [arr_uan]
        mov edx, [arr_ual]
        cmp eax, [ebp+V_X]
        jne .mv
        cmp ebx, [ebp+V_Y]
        jne .mv
        cmp ecx, [ebp+V_AN]
        jne .mv
        cmp edx, [ebp+V_AL]
        je .msg
.mv:
        call ven_mover
.msg:
        test ebp, ebp
        jz .fin
        cmp dword [ebp+V_USO], 1
        jne .fin
        xor ecx, ecx
        mov eax, [ebp+V_AN]
        cmp eax, [arr_ian]
        jne .c1
        mov eax, [ebp+V_AL]
        cmp eax, [arr_ial]
        je .c2
.c1:
        inc ecx
.c2:
        mov eax, WM_EXITSIZEMOVE
        xor edx, edx
        call msg_poner
.fin:
        mov eax, [rat_x]            ; la forma del cursor, donde ha quedado
        mov ebx, [rat_y]
        call ges_donde
        call tam_cursor_zona
        popad
        ret

; ---- el contorno: cuatro tiras de CON_G puntos dibujadas en la superficie
; (guardando lo que tapan), sin repintar las ventanas ----
CON_G       equ 3
CON_MAX     equ 2048

; EAX=x EBX=y ECX=ancho EDX=alto: el contorno, ahí
con_dibujar:
        pushad
        call cur_ocultar
        call con_reponer
        cmp ecx, CON_MAX
        jbe .a
        mov ecx, CON_MAX
.a:
        cmp edx, CON_MAX
        jbe .b
        mov edx, CON_MAX
.b:
        cmp ecx, 3 * CON_G
        jge .c
        mov ecx, 3 * CON_G
.c:
        cmp edx, 3 * CON_G
        jge .d
        mov edx, 3 * CON_G
.d:
        mov [con_x], eax
        mov [con_y], ebx
        mov [con_an], ecx
        mov [con_al], edx
        mov byte [con_op], 2        ; guardar lo de debajo
        call con_tiras
        call gdi_recorte_total
        COLOR C_TINTA
        call gdi_marco
        inc eax
        inc ebx
        sub ecx, 2
        sub edx, 2
        COLOR C_OCRE
        call gdi_marco
        inc eax
        inc ebx
        sub ecx, 2
        sub edx, 2
        COLOR C_TINTA
        call gdi_marco
        mov eax, [con_x]
        mov ebx, [con_y]
        mov ecx, [con_an]
        mov edx, [con_al]
        add ecx, eax
        add edx, ebx
        call pres_unir
        mov byte [con_puesto], 1
        mov byte [actividad], 1
        popad
        ret

con_quitar:
        cmp byte [con_puesto], 0
        je .x
        call cur_ocultar
        call con_reponer
.x:
        ret

; (interna) si está puesto, repone lo que tapaba
con_reponer:
        cmp byte [con_puesto], 0
        je .x
        pushad
        mov byte [con_op], 0
        call con_tiras
        mov eax, [con_x]
        mov ebx, [con_y]
        mov ecx, [con_an]
        mov edx, [con_al]
        add ecx, eax
        add edx, ebx
        call pres_unir
        mov byte [con_puesto], 0
        mov byte [actividad], 1
        popad
.x:
        ret

; (interna) las cuatro tiras de con_x...: con_op 2 leerlas, 0 ponerlas
con_tiras:
        pushad
        call gdi_recorte_total
        mov esi, con_buf
        mov eax, [con_x]
        mov ebx, [con_y]
        mov ecx, [con_an]
        mov edx, CON_G
        call .una                   ; arriba
        add ebx, [con_al]
        sub ebx, CON_G
        call .una                   ; abajo
        mov ebx, [con_y]
        add ebx, CON_G
        mov ecx, CON_G
        mov edx, [con_al]
        sub edx, 2 * CON_G
        call .una                   ; izquierda
        add eax, [con_an]
        sub eax, CON_G
        call .una                   ; derecha
        popad
        ret
.una:
        push edi
        cmp byte [con_op], 0
        je .poner
        mov edi, esi
        call gdi_leer
        jmp .sig
.poner:
        call gdi_copiar
.sig:
        push eax
        mov eax, ecx
        imul eax, edx
        add esi, eax
        pop eax
        pop edi
        ret

; ---- la rueda del ratón (API de CuteMouse: INT 33h AX=11h y el bit 7 de
; la máscara de sucesos, con los pasos en BH) ----
; (rat_iniciar) con [rmcs+0x18] = la máscara: le añade la rueda si la hay
rueda_mascara:
        pushad
        mov eax, 0x0011
        xor ecx, ecx
        int 0x33
        cmp ax, 0x574D              ; «WM»
        jne .x
        test cl, 1
        jz .x
        or dword [rmcs+0x18], 0x80
        mov byte [hay_rueda], 1
.x:
        popad
        ret

; (eventos) EAX=pasos (positivo: hacia el usuario, es decir, bajar): un
; WM_MOUSEWHEEL a la ventana que hay debajo del cursor (o a la que tiene el
; ratón), con ECX=pasos y EDX=x,y en su zona; la que no lo quiere lo pasa a
; su madre (ven_defecto)
ges_rueda:
        pushad
        test eax, eax
        jz .x
        cmp dword [menu_ven], 0
        jne .x
        cmp byte [arrastre], 0
        jne .x
        mov esi, eax
        call ev_act
        mov edi, [captura]
        test edi, edi
        jnz .env
        mov eax, [rat_x]
        mov ebx, [rat_y]
        call ges_donde
        cmp ecx, Z_CLIENTE
        jne .x
        test dword [ebp+V_ESTADO], S_INACTIVA
        jnz .x
        test dword [edi+V_ESTADO], S_INACTIVA
        jnz .x
.env:
        mov ebp, edi
        call ven_cli
        mov edx, [rat_y]
        sub edx, ebx
        shl edx, 16
        mov ecx, [rat_x]
        sub ecx, eax
        mov dx, cx
        mov ecx, esi
        mov eax, WM_MOUSEWHEEL
        call msg_poner
.x:
        popad
        ret

; =============================================================================
;  La barra de desplazamiento (c_barra), vertical u horizontal (BDS_HORIZ).
;  De BD_AN puntos de grueso; flechas en las puntas, la pista y el pulgar,
;  que es tan largo como la página respecto a todo. Flechas y pista repiten
;  mientras se tienen apretadas; el pulgar se arrastra; la rueda encima la
;  mueve. Cada vez que se mueve avisa a su madre con WM_VSCROLL o WM_HSCROLL
;  (ECX=posición nueva, EDX=la barra). La posición va de 0 al máximo.
;    BDM_PONER  ECX=máximo EDX=página      BDM_POS   ECX=posición (sin aviso)
;    BDM_LEER   -> EAX=posición            BDM_MOVER ECX=BD_* (con aviso)
;    BDM_LINEA  ECX=lo que avanza una flecha (y cada paso de la rueda)
;  V_D0 posición, V_D1 máximo, V_D2 página, V_D3 línea, V_D4 lo apretado
;  (1 la primera flecha, 2 la otra, 3 la pista antes del pulgar, 4 después,
;  5 el pulgar), V_D5 dónde se agarró el pulgar
; =============================================================================
c_barra     dd barra_proc
            db C_PIEDRA, 0, 0, 0

barra_proc:
        cmp eax, WM_PAINT
        je barra_pintar
        cmp eax, WM_LBUTTONDOWN
        je .pulsa
        cmp eax, WM_LBUTTONDBLCLK
        je .pulsa
        cmp eax, WM_MOUSEMOVE
        je .mueve
        cmp eax, WM_LBUTTONUP
        je .suelta
        cmp eax, WM_TIMER
        je .tiempo
        cmp eax, WM_MOUSEWHEEL
        je .rueda
        cmp eax, BDM_PONER
        je .poner
        cmp eax, BDM_POS
        je .pos
        cmp eax, BDM_LEER
        je .leer
        cmp eax, BDM_MOVER
        je .mover
        cmp eax, BDM_LINEA
        je .linea
        jmp ven_defecto
.leer:
        mov eax, [ebp+V_D0]
        ret
.linea:
        mov [ebp+V_D3], ecx
        jmp .nada
.mover:
        call bar_accion
        mov eax, [ebp+V_D0]
        ret
.poner:
        test ecx, ecx
        jns .p1
        xor ecx, ecx
.p1:
        test edx, edx
        jns .p2
        xor edx, edx
.p2:
        cmp ecx, [ebp+V_D1]
        jne .p3
        cmp edx, [ebp+V_D2]
        je .p4
.p3:
        mov [ebp+V_D1], ecx
        mov [ebp+V_D2], edx
        call ven_invalidar
.p4:
        mov ecx, [ebp+V_D0]
.pos:
        call bar_acotar
        cmp ecx, [ebp+V_D0]
        je .nada
        mov [ebp+V_D0], ecx
        jmp .inv
.rueda:
        mov eax, [ebp+V_D3]
        test eax, eax
        jnz .r1
        inc eax
.r1:
        imul eax, ecx
        add eax, [ebp+V_D0]
        call bar_ir
        jmp .nada
.pulsa:
        call bar_a
        push eax
        call bar_zona
        mov [ebp+V_D4], eax
        pop ecx
        cmp eax, 5
        jne .u1
        sub ecx, [bg_t0]            ; el pulgar: se arrastra
        mov [ebp+V_D5], ecx
        jmp .inv
.u1:
        test eax, eax
        jz .nada
        lea ecx, [eax-1]            ; flechas y pista: BD_LIN_ARR... BD_PAG_ABA
        call bar_accion
        mov ecx, 1
        mov edx, 400                ; repite si se tiene apretado
        call tmp_poner
        jmp .inv
.tiempo:
        mov eax, [ebp+V_D4]
        test eax, eax
        jz .nada
        cmp eax, 5
        jae .nada
        cmp byte [rat_boton], 0
        je .suelta
        mov edx, [rat_y]            ; ¿sigue encima de lo mismo?
        sub edx, [ebp+V_Y]
        shl edx, 16
        mov ecx, [rat_x]
        sub ecx, [ebp+V_X]
        mov dx, cx
        call bar_a
        call bar_zona
        cmp eax, [ebp+V_D4]
        jne .t1
        lea ecx, [eax-1]
        call bar_accion
.t1:
        mov ecx, 1
        mov edx, 55
        call tmp_poner
        jmp .nada
.mueve:
        cmp dword [ebp+V_D4], 5
        jne .nada
        call bar_a
        call bar_geo
        sub eax, [ebp+V_D5]
        sub eax, BD_AN
        jns .m1
        xor eax, eax
.m1:
        mov ecx, [bg_pista]
        sub ecx, [bg_tl]
        jle .nada
        mul dword [ebp+V_D1]
        div ecx
        call bar_ir
        jmp .nada
.suelta:
        cmp dword [ebp+V_D4], 0
        je .nada
        mov dword [ebp+V_D4], 0
        mov ecx, 1
        call tmp_quitar
.inv:
        call ven_invalidar
.nada:
        xor eax, eax
        ret

; (interna) EDX=x,y del ratón en la barra -> EAX=a lo largo
bar_a:
        movsx eax, dx
        test dword [ebp+V_ESTILO], BDS_HORIZ
        jnz .x
        mov eax, edx
        sar eax, 16
.x:
        ret

; (interna) ECX=posición -> ECX acotada entre 0 y el máximo
bar_acotar:
        test ecx, ecx
        jns .a
        xor ecx, ecx
.a:
        cmp ecx, [ebp+V_D1]
        jle .x
        mov ecx, [ebp+V_D1]
.x:
        ret

; (interna) EAX=posición nueva: la pone y avisa a la madre si cambia
bar_ir:
        pushad
        mov ecx, eax
        call bar_acotar
        cmp ecx, [ebp+V_D0]
        je .x
        mov [ebp+V_D0], ecx
        call ven_invalidar
        mov edx, ebp
        mov eax, WM_VSCROLL
        test dword [ebp+V_ESTILO], BDS_HORIZ
        jz .v
        mov eax, WM_HSCROLL
.v:
        mov ebp, [ebp+V_PADRE]
        call msg_enviar
.x:
        popad
        ret

; (interna) ECX=BD_*: mueve la barra (y avisa)
bar_accion:
        pushad
        mov eax, [ebp+V_D0]
        mov edx, [ebp+V_D3]
        test edx, edx
        jnz .a
        inc edx
.a:
        mov ebx, [ebp+V_D2]
        test ebx, ebx
        jnz .b
        mov ebx, edx
.b:
        cmp ecx, BD_LIN_ARR
        jne .c1
        sub eax, edx
.c1:
        cmp ecx, BD_LIN_ABA
        jne .c2
        add eax, edx
.c2:
        cmp ecx, BD_PAG_ARR
        jne .c3
        sub eax, ebx
.c3:
        cmp ecx, BD_PAG_ABA
        jne .c4
        add eax, ebx
.c4:
        cmp ecx, BD_INICIO
        jne .c5
        xor eax, eax
.c5:
        cmp ecx, BD_FIN
        jne .c6
        mov eax, [ebp+V_D1]
.c6:
        call bar_ir
        popad
        ret

; (interna) medidas: bg_largo, bg_pista, bg_t0 (dónde empieza el pulgar) y
; bg_tl (su largo; 0: no hay pulgar)
bar_geo:
        pushad
        mov eax, [ebp+V_AL]
        test dword [ebp+V_ESTILO], BDS_HORIZ
        jz .a
        mov eax, [ebp+V_AN]
.a:
        mov [bg_largo], eax
        sub eax, 2 * BD_AN
        jns .b
        xor eax, eax
.b:
        mov [bg_pista], eax
        mov dword [bg_tl], 0
        mov dword [bg_t0], BD_AN
        mov edi, [ebp+V_D1]
        test edi, edi
        jle .x
        cmp eax, 8
        jl .x
        mov ebx, edi
        add ebx, [ebp+V_D2]
        mul dword [ebp+V_D2]
        div ebx
        cmp eax, 12
        jae .c
        mov eax, 12
.c:
        cmp eax, [bg_pista]
        jbe .d
        mov eax, [bg_pista]
.d:
        mov [bg_tl], eax
        mov eax, [bg_pista]
        sub eax, [bg_tl]
        mul dword [ebp+V_D0]
        div edi
        add eax, BD_AN
        mov [bg_t0], eax
.x:
        popad
        ret

; (interna) EAX=punto a lo largo -> EAX=zona (0 ninguna, 1-5 como V_D4)
bar_zona:
        push ecx
        call bar_geo
        mov ecx, eax
        mov eax, 1
        cmp ecx, BD_AN
        jl .x
        mov eax, [bg_largo]
        sub eax, BD_AN
        cmp ecx, eax
        mov eax, 2
        jge .x
        xor eax, eax
        cmp dword [bg_tl], 0
        je .x
        mov eax, 3
        cmp ecx, [bg_t0]
        jl .x
        mov eax, [bg_t0]
        add eax, [bg_tl]
        cmp ecx, eax
        mov eax, 5
        jl .x
        mov eax, 4
.x:
        pop ecx
        ret

; (internas) EAX=a lo largo EBX=a lo ancho ECX=largo EDX=ancho: un
; rectángulo lleno o un marco, girado según la barra
bar_rect:
        pushad
        call bar_girar
        call dc_rect
        popad
        ret
bar_marco:
        pushad
        call bar_girar
        call dc_marco
        popad
        ret
bar_girar:
        test dword [ebp+V_ESTILO], BDS_HORIZ
        jnz .x
        xchg eax, ebx
        xchg ecx, edx
.x:
        ret

barra_pintar:
        call dc_pintar
        call bar_geo
        ; las flechas: la caja y el triángulo
        mov edi, 1
.fl:
        COLOR C_ARENA
        cmp [ebp+V_D4], edi
        jne .f1
        COLOR C_ARENA_OSC
.f1:
        xor eax, eax
        cmp edi, 1
        je .f2
        mov eax, [bg_largo]
        sub eax, BD_AN
.f2:
        xor ebx, ebx
        mov ecx, BD_AN
        mov edx, BD_AN
        call bar_rect
        COLOR C_MARRON
        call bar_marco
        COLOR C_TINTA
        cmp dword [ebp+V_D1], 0
        jg .f3
        COLOR C_ARENA_OSC           ; (no se puede mover: apagada)
.f3:
        mov esi, eax
        xor ecx, ecx                ; k = 0..3
.tr:
        push ecx
        lea eax, [esi+ecx+6]
        cmp edi, 1
        jne .tr2
        mov ebx, 8                  ; hacia el principio: crece
        sub ebx, ecx
        lea edx, [ecx*2+1]
        jmp .tr3
.tr2:
        lea ebx, [ecx+5]            ; hacia el final: mengua
        mov edx, 7
        sub edx, ecx
        sub edx, ecx
.tr3:
        mov ecx, 1
        call bar_rect
        pop ecx
        inc ecx
        cmp ecx, 4
        jb .tr
        inc edi
        cmp edi, 2
        jbe .fl
        ; la pista: un filete alrededor
        COLOR C_MARRON
        xor eax, eax
        xor ebx, ebx
        mov ecx, [bg_largo]
        mov edx, BD_AN
        call bar_marco
        ; el pulgar
        mov ecx, [bg_tl]
        test ecx, ecx
        jz .x
        mov eax, [bg_t0]
        xor ebx, ebx
        mov edx, BD_AN
        COLOR C_ARENA
        cmp dword [ebp+V_D4], 5
        jne .p1
        COLOR C_CREMA
.p1:
        call bar_rect
        COLOR C_MARRON
        call bar_marco
        cmp ecx, 16                 ; tres rayas para agarrarlo
        jb .x
        shr ecx, 1
        add eax, ecx
        sub eax, 3
        mov ebx, 4
        mov ecx, 1
        mov edx, BD_AN - 8
        mov edi, 3
.ry:
        call bar_rect
        add eax, 2
        dec edi
        jnz .ry
.x:
        call dc_cerrar
        xor eax, eax
        ret

%ifdef CON_PROGRAMAS
; =============================================================================
;  La barra en las vistas de iconos del Centro (el Centro y cada grupo).
;  V_D1 = la barra (hija, id 999), V_D2 = lo desplazado (en puntos)
; =============================================================================
VT_LINEA    equ CEN_FILA / 2
vt_plant:
        PLANTILLA c_barra, 0, E_HIJA | E_OCULTA, 0, 0, BD_AN, 3 * BD_AN, 999

; (WM_CREATE de la vista) su barra, oculta
vt_crear:
        pushad
        mov esi, vt_plant
        call ven_crear
        mov [ebp+V_D1], eax
        test eax, eax
        jz .x
        mov ebp, eax
        mov eax, BDM_LINEA
        mov ecx, VT_LINEA
        call msg_enviar
.x:
        popad
        ret

; (vista_proc) lo de la barra y el tamaño: CF=1 si lo ha hecho (EAX=resultado)
vt_mensaje:
        cmp eax, WM_SIZE
        je .tam
        cmp eax, WM_VSCROLL
        je .des
        cmp eax, WM_MOUSEWHEEL
        je .rueda
        cmp eax, WM_GETMINMAXINFO
        je .min
        cmp eax, WM_EXITSIZEMOVE
        je .fin
        clc
        ret
.tam:
        call vista_disponer
        call vista_ver
        call ven_invalidar
        jmp .hecho
.des:
        mov [ebp+V_D2], ecx
        call ven_invalidar
        jmp .hecho
.rueda:
        push ebp
        mov ebp, [ebp+V_D1]
        test ebp, ebp
        jz .r1
        test dword [ebp+V_ESTADO], S_VISIBLE
        jz .r1
        call msg_enviar             ; (la barra se mueve y avisa: .des)
.r1:
        pop ebp
        jmp .hecho
.min:
        mov eax, 150 + 110 * 65536  ; una columna y la barra con sus flechas
        stc
        ret
.fin:
        test ecx, ecx               ; un grupo al que el usuario da el tamaño
        jz .hecho                   ; ya no se alarga solo (CENTRO.INI: Medida=1)
        mov eax, [ebp+V_ID]
        sub eax, 100
        js .hecho
        shl eax, 6
        add eax, [gr_tabla]
        or byte [eax+GR_BAND], GRB_MEDIDA
.hecho:
        xor eax, eax
        stc
        ret

; (interna) EBP=vista ECX=ancho de la zona -> EAX=alto de todos sus iconos
; (con las columnas que caben en ese ancho)
vt_contenido:
        push ebx
        push ecx
        push edx
        mov eax, ecx
        sub eax, 12
        jns .a
        xor eax, eax
.a:
        xor edx, edx
        mov ecx, CEN_PASO
        div ecx
        test eax, eax
        jnz .b
        inc eax
.b:
        mov ebx, eax                ; columnas
        xor ecx, ecx                ; iconos
.c:
        call vista_ficha
        test eax, eax
        jz .d
        inc ecx
        jmp .c
.d:
        lea eax, [ecx+ebx-1]
        xor edx, edx
        div ebx
        imul eax, CEN_FILA
        add eax, 8
        cmp dword [ebp+V_ID], 0
        jne .x
        add eax, 40                 ; el Centro: y la pista de abajo
.x:
        pop edx
        pop ecx
        pop ebx
        ret

; EBP=vista: la barra, a la vista si no caben los iconos (y en su sitio)
vista_disponer:
        pushad
        mov edi, [ebp+V_D1]
        test edi, edi
        jz .x
        call ven_cli
        mov [vt_x], eax
        mov [vt_y], ebx
        mov [vt_al], edx
        call vt_contenido
        cmp eax, edx
        jle .no
        sub ecx, BD_AN              ; con la barra caben menos columnas
        mov [vt_an], ecx
        call vt_contenido
        sub eax, [vt_al]
        jg .si
        xor eax, eax
.si:
        mov [vt_max], eax
        push ebp
        mov ebp, edi
        mov eax, [vt_x]             ; en su sitio: a la derecha, de arriba abajo
        add eax, [vt_an]
        cmp eax, [ebp+V_X]
        jne .mv
        mov eax, [vt_y]
        cmp eax, [ebp+V_Y]
        jne .mv
        cmp dword [ebp+V_AN], BD_AN
        jne .mv
        mov eax, [vt_al]
        cmp eax, [ebp+V_AL]
        je .smv
.mv:
        mov eax, [vt_an]
        xor ebx, ebx
        mov ecx, BD_AN
        mov edx, [vt_al]
        call ven_mover
.smv:
        mov eax, BDM_PONER
        mov ecx, [vt_max]
        mov edx, [vt_al]
        call msg_enviar
        test dword [ebp+V_ESTADO], S_VISIBLE
        jnz .vis
        mov eax, 1
        call ven_mostrar
.vis:
        mov eax, BDM_LEER
        call msg_enviar
        pop ebp
        mov [ebp+V_D2], eax
        jmp .x
.no:
        test dword [edi+V_ESTADO], S_VISIBLE
        jz .n1
        push ebp
        mov ebp, edi
        xor eax, eax
        call ven_mostrar
        mov eax, BDM_POS
        xor ecx, ecx
        call msg_enviar
        pop ebp
        call ven_invalidar
.n1:
        mov dword [ebp+V_D2], 0
.x:
        popad
        ret

; EBP=vista: desplaza lo justo para que se vea el icono marcado
vista_ver:
        pushad
        mov edi, [ebp+V_D1]
        test edi, edi
        jz .x
        test dword [edi+V_ESTADO], S_VISIBLE
        jz .x
        call vista_cols
        mov eax, [ebp+V_D0]
        xor edx, edx
        div dword [vis_cols]
        imul eax, CEN_FILA          ; arriba del icono
        mov ecx, [ebp+V_D2]
        cmp eax, ecx
        jl .pon
        add eax, CEN_FILA
        sub eax, [vis_alto]
        cmp eax, ecx
        jle .x
.pon:
        mov ecx, eax
        push ebp
        mov ebp, edi
        mov eax, BDM_POS
        call msg_enviar
        mov eax, BDM_LEER
        call msg_enviar
        pop ebp
        cmp eax, [ebp+V_D2]
        je .x
        mov [ebp+V_D2], eax
        call ven_invalidar
.x:
        popad
        ret

; (vista_proc, teclas) ECX=tecla EDI=iconos EAX=el marcado (vis_cols ya
; calculado) -> CF=1 y EAX=el nuevo con Inicio, Fin, RePág y AvPág
vt_tecla:
        cmp ecx, TC_INICIO
        jne .a
        xor eax, eax
        stc
        ret
.a:
        cmp ecx, TC_FIN
        jne .b
        lea eax, [edi-1]
        test eax, eax
        jns .s
        xor eax, eax
.s:
        stc
        ret
.b:
        cmp ecx, TC_REPAG
        je .pag
        cmp ecx, TC_AVPAG
        je .pag
        clc
        ret
.pag:
        push ebx
        push ecx
        push edx
        push eax
        mov eax, [vis_alto]         ; filas de una página (al menos una)
        xor edx, edx
        mov ebx, CEN_FILA
        div ebx
        test eax, eax
        jnz .p1
        inc eax
.p1:
        imul eax, [vis_cols]
        mov ebx, eax
        pop eax
        cmp ecx, TC_REPAG
        jne .p3
        sub eax, ebx
        jns .p4
        xor edx, edx                ; a la primera fila, en su columna
        add eax, ebx
        div dword [vis_cols]
        mov eax, edx
        jmp .p4
.p3:
        add eax, ebx
        cmp eax, edi
        jl .p4
        lea eax, [edi-1]
        test eax, eax
        jns .p4
        xor eax, eax
.p4:
        pop edx
        pop ecx
        pop ebx
        stc
        ret

; (vista_pintar) -> EBX=dónde va la pista del Centro (abajo de todo)
vt_pista_y:
        push eax
        push ecx
        push edx
        mov eax, [vis_n]
        add eax, [vis_cols]
        dec eax
        xor edx, edx
        div dword [vis_cols]
        imul eax, CEN_FILA
        add eax, 48
        cmp eax, [vis_alto]
        jge .a
        mov eax, [vis_alto]
.a:
        lea ebx, [eax-43]
        sub ebx, [ebp+V_D2]
        pop edx
        pop ecx
        pop eax
        ret
%endif

%ifdef CON_PROGRAMAS
; =============================================================================
;  La barra horizontal del Bloc de notas: debajo del cuadro de texto (que
;  tiene la suya vertical), para ver las líneas largas. Se pone al día con
;  un temporizador (el cuadro de texto se desplaza solo al escribir).
; =============================================================================
BLOC_HB_T   equ 77
bloc_hb_plant:
        PLANTILLA c_barra, 0, E_HIJA | BDS_HORIZ, 0, 0, 100, BD_AN, 106

; (WM_CREATE del Bloc, EBP=su ventana)
bloc_hb_crear:
        pushad
        mov esi, bloc_hb_plant
        call ven_crear
        mov [bloc_hbar], eax
        mov ecx, BLOC_HB_T
        mov edx, 330
        call tmp_poner
        test eax, eax
        jz .x
        mov ebp, eax
        mov eax, BDM_LINEA
        mov ecx, 2
        call msg_enviar
.x:
        popad
        ret

; (WM_SIZE del Bloc) ECX=ancho EDX=alto de su zona
bloc_hb_mover:
        pushad
        mov ebp, [bloc_hbar]
        test ebp, ebp
        jz .x
        xor eax, eax
        lea ebx, [edx-BD_AN]
        mov edx, BD_AN
        call ven_mover
        call bloc_hb_sinc
.x:
        popad
        ret

; (bloc_proc) CF=1 si era suyo: WM_HSCROLL de la barra, o su temporizador
bloc_hb_msg:
        cmp eax, WM_HSCROLL
        je .des
        cmp eax, WM_TIMER
        jne .no
        cmp ecx, BLOC_HB_T
        jne .no
        call bloc_hb_sinc
        jmp .si
.des:
        push ebp
        mov ebp, [bloc_edit]
        test ebp, ebp
        jz .d1
        mov [ebp+V_D6], ecx         ; la primera columna a la vista
        call ven_invalidar
.d1:
        pop ebp
.si:
        xor eax, eax
        stc
        ret
.no:
        clc
        ret

; (interna) la barra, al día con el texto: la línea más larga y la columna
bloc_hb_sinc:
        pushad
        mov ebp, [bloc_edit]
        test ebp, ebp
        jz .x
        cmp dword [bloc_hbar], 0
        je .x
        call ed_geo
        mov esi, [ebp+V_D0]
        mov ecx, [ebp+V_D1]
        xor edx, edx                ; la más larga
        xor ebx, ebx                ; la que se cuenta
        test esi, esi
        jz .f
        jecxz .f
.l:
        lodsb
        cmp al, 10
        jne .c
        xor ebx, ebx
        jmp .s
.c:
        inc ebx
        cmp ebx, edx
        jbe .s
        mov edx, ebx
.s:
        loop .l
.f:
        inc edx                     ; (y el cursor detrás de la última letra)
        sub edx, [ed_cols]
        jns .m
        xor edx, edx
.m:
        mov edi, [ebp+V_D6]
        mov ebp, [bloc_hbar]
        mov ecx, edx
        mov edx, [ed_cols]
        mov eax, BDM_PONER
        call msg_enviar
        mov ecx, edi
        mov eax, BDM_POS
        call msg_enviar
.x:
        popad
        ret
%endif
