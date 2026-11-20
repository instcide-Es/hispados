; =============================================================================
;  Hispa-BASIC - PRINT USING (como en GW-BASIC)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  PRINT USING formato; datos...   El formato se recorre una y otra vez:
;    números:  #  una cifra       .  el punto decimal    ,  (antes del punto)
;              separa los miles   +  delante o detrás: el signo siempre
;              -  detrás: el signo menos detrás          **  rellena con *
;              $$  el $ pegado a la cifra                ^^^^  con exponente
;    cadenas:  !  el primer carácter   &  la cadena entera
;              \  \  tantos caracteres como hay entre las barras, contándolas
;    _  el carácter que sigue, tal cual; lo demás, tal cual.
;  Si el número no cabe, sale con «%» delante.
; =============================================================================
; variables de paso (en datos): v_us_* (ver datos.inc)

; ---- tras USING: SI en el formato ----
print_using:
        call evalua
        cmp byte [ac + AC_TIPO], T_CAD
        jne err_tipos
        mov bx, [ac + AC_M0]
        ; el formato, a b_us (se pueden pedir cadenas mientras: se copia)
        push si
        mov cl, [bx]
        xor ch, ch
        mov [v_us_largo], cx
        mov si, [bx + 1]
        mov di, b_us
        rep movsb
        pop si
        call libera_temp
        call salta_esp
        cmp al, ';'
        je .pyc
        cmp al, ','
        jne err_sintaxis
.pyc:
        inc si
        mov word [v_us_pos], 0
        mov byte [v_us_hubo], 0
.dato:
        call salta_esp
        call fin_de_sent
        je .fin
        ; el literal hasta el siguiente campo, y el campo
        call us_hasta_campo         ; AL = tipo de campo (0 ninguno)
        test al, al
        jnz .hay
        ; un formato sin campos: llamada no válida
        cmp byte [v_us_hubo], 0
        jne .vuelve
        jmp err_funcion
.vuelve:
        mov word [v_us_pos], 0
        mov byte [v_us_hubo], 0
        call us_hasta_campo
        test al, al
        jz err_funcion
.hay:
        mov byte [v_us_hubo], 1
        push ax
        call evalua
        pop ax
        cmp al, 'N'
        je .num
        ; un campo de cadena
        cmp byte [ac + AC_TIPO], T_CAD
        jne err_tipos
        mov bx, [ac + AC_M0]
        call libera_temp
        call us_cadena
        jmp .sig
.num:
        cmp byte [ac + AC_TIPO], T_CAD
        je err_tipos
        call us_numero
.sig:
        call salta_esp
        cmp al, ';'
        je .sep
        cmp al, ','
        jne .fin
.sep:
        inc si
        call salta_esp
        call fin_de_sent
        jne .dato
        ret                         ; con «;» o «,» al final: sin salto de línea
.fin:
        ; lo que quede del formato hasta el siguiente campo, y el salto
        call us_hasta_campo
        jmp pon_nl

; ---- escribe el formato desde v_us_pos hasta el siguiente campo; AL =
; 'N' (número) o 'C' (cadena) y v_us_campo/v_us_fin el campo; 0 si se acaba
us_hasta_campo:
        push si
        push bx
        mov bx, [v_us_pos]
.c:
        cmp bx, [v_us_largo]
        jae .nada
        mov al, [b_us + bx]
        ; ¿empieza un campo aquí?
        call us_es_campo            ; CF=1: es campo (AL tipo, CX largo)
        jc .campo
        mov al, [b_us + bx]
        cmp al, '_'
        jne .lit
        inc bx
        cmp bx, [v_us_largo]
        jae .nada
        mov al, [b_us + bx]
.lit:
        call pon_car
        inc bx
        jmp .c
.campo:
        mov [v_us_campo], bx
        add bx, cx
        mov [v_us_pos], bx
        mov [v_us_fin], bx
        pop bx
        pop si
        ret
.nada:
        mov [v_us_pos], bx
        xor al, al
        pop bx
        pop si
        ret

; ¿hay un campo en b_us+BX? CF=1 y AL = 'N' o 'C', CX = lo que ocupa
us_es_campo:
        push bx
        push dx
        mov dx, bx                  ; el principio
        mov al, [b_us + bx]
        cmp al, '!'
        je .uno_c
        cmp al, '&'
        je .uno_c
        cmp al, '\'
        je .barras
        ; ¿un número? +, $$, **, #, o «.#»
        cmp al, '+'
        jne .n0
        inc bx
.n0:
        call .c_en
        cmp al, '#'
        je .num
        cmp al, '.'
        jne .n1
        call .sig_es_almo           ; «.#»
        je .num
        jmp .no
.n1:
        cmp al, '$'
        jne .n2
        call .sig_igual
        je .num
        jmp .no
.n2:
        cmp al, '*'
        jne .no
        call .sig_igual
        jne .no
.num:
        ; ahora hasta el final: # . , ** $$ ^^^^ y + o - detrás
.nl:
        call .c_en
        cmp al, '#'
        je .na
        cmp al, '.'
        je .na
        cmp al, ','
        je .na
        cmp al, '*'
        je .na
        cmp al, '$'
        je .na
        cmp al, '^'
        je .na
        cmp al, '+'
        je .detras
        cmp al, '-'
        je .detras
        jmp .num_fin
.na:
        inc bx
        jmp .nl
.detras:
        inc bx
.num_fin:
        mov cx, bx
        sub cx, dx
        mov al, 'N'
        jmp .si
.uno_c:
        mov cx, 1
        mov al, 'C'
        jmp .si
.barras:
        inc bx
.b1:
        cmp bx, [v_us_largo]
        jae .no
        cmp byte [b_us + bx], '\'
        je .b2
        cmp byte [b_us + bx], ' '
        jne .no
        inc bx
        jmp .b1
.b2:
        inc bx
        mov cx, bx
        sub cx, dx
        mov al, 'C'
.si:
        pop dx
        pop bx
        stc
        ret
.no:
        pop dx
        pop bx
        clc
        ret
.c_en:                              ; AL = b_us[BX] (0 si se pasa)
        xor al, al
        cmp bx, [v_us_largo]
        jae .c_x
        mov al, [b_us + bx]
.c_x:
        ret
.sig_igual:                         ; ¿b_us[BX+1] == AL? ZF
        push bx
        inc bx
        mov ah, al
        call .c_en
        cmp al, ah
        pop bx
        ret
.sig_es_almo:                       ; ¿b_us[BX+1] == '#'? ZF
        push bx
        inc bx
        call .c_en
        cmp al, '#'
        pop bx
        ret

; ---- una cadena (descriptor BX) en el campo de cadena ----
us_cadena:
        push si
        mov di, [v_us_campo]
        mov al, [b_us + di]
        mov cl, [bx]
        xor ch, ch
        mov si, [bx + 1]
        cmp al, '&'
        je .todo
        mov dx, 1                   ; ! : uno
        cmp al, '!'
        je .n
        mov dx, [v_us_fin]          ; \  \ : lo que ocupa el campo
        sub dx, di
.n:
        ; DX caracteres: los de la cadena y, si falta, espacios
.c:
        test dx, dx
        jz .x
        mov al, ' '
        jcxz .esp
        lodsb
        dec cx
.esp:
        call pon_car
        dec dx
        jmp .c
.todo:
        jcxz .x
.t:
        lodsb
        call pon_car
        loop .t
.x:
        pop si
        ret

; ---- un número (el acumulador) en el campo de número ----
us_numero:
        push si
        ; mirar el campo: cifras delante (contando las de ** y $$), detrás,
        ; comas, signo delante/detrás, exponente
        mov bx, [v_us_campo]
        xor ax, ax
        mov [v_us_ent], ax          ; cifras de la parte entera
        mov [v_us_dec], ax          ; cifras decimales (-1: sin punto)
        mov byte [v_us_mas], 0      ; 1: + delante; 2: + detrás; 3: - detrás
        mov byte [v_us_ast], 0
        mov byte [v_us_dolar], 0
        mov byte [v_us_coma], 0
        mov byte [v_us_exp], 0
        mov word [v_us_dec], 0xFFFF
        cmp byte [b_us + bx], '+'
        jne .m1
        mov byte [v_us_mas], 1
        inc bx
.m1:
        cmp byte [b_us + bx], '*'
        jne .m2
        mov byte [v_us_ast], 1
        add word [v_us_ent], 2
        add bx, 2
        cmp byte [b_us + bx], '$'
        jne .m3
        mov byte [v_us_dolar], 1
        inc word [v_us_ent]         ; (**$: el $ cuenta como una posición)
        inc bx
        jmp .m3
.m2:
        cmp byte [b_us + bx], '$'
        jne .m3
        mov byte [v_us_dolar], 1
        add word [v_us_ent], 2      ; $$: dos posiciones (una, el $)
        add bx, 2
.m3:
        cmp bx, [v_us_fin]
        jae .m_fin
        mov al, [b_us + bx]
        inc bx
        cmp al, '#'
        jne .m4
        cmp word [v_us_dec], 0xFFFF
        je .m_ent
        inc word [v_us_dec]
        jmp .m3
.m_ent:
        inc word [v_us_ent]
        jmp .m3
.m4:
        cmp al, '.'
        jne .m5
        mov word [v_us_dec], 0
        jmp .m3
.m5:
        cmp al, ','
        jne .m6
        mov byte [v_us_coma], 1
        inc word [v_us_ent]         ; (la coma ocupa una cifra)
        jmp .m3
.m6:
        cmp al, '^'
        jne .m7
        inc byte [v_us_exp]
        jmp .m3
.m7:
        cmp al, '+'
        jne .m8
        mov byte [v_us_mas], 2
        jmp .m3
.m8:
        cmp al, '-'
        jne .m3
        mov byte [v_us_mas], 3
        jmp .m3
.m_fin:
        ; las cifras decimales que se piden
        mov cx, [v_us_dec]
        cmp cx, 0xFFFF
        jne .d1
        xor cx, cx
.d1:
        mov [v_us_ndec], cx
        ; el signo
        mov byte [v_us_neg], 0
        mov di, ac
        call a_flot
        cmp word [ac + AC_M3], 0
        je .pos
        test byte [ac + AC_SIG], 0x80
        jz .pos
        mov byte [v_us_neg], 1
.pos:
        mov byte [ac + AC_SIG], 0
        cmp byte [v_us_exp], 4
        jae us_exponente
        ; las cifras: redondeo(|x| * 10^dec) en b_cifras
        call us_cifras              ; b_cifras, CX = cuántas
        ; las que hay antes del punto
        mov ax, cx
        sub ax, [v_us_ndec]
        jg .e_hay
        xor ax, ax
.e_hay:
        mov [v_us_nent], ax         ; cifras enteras de verdad
        ; lo que hace falta: las cifras enteras (al menos un 0 si no hay
        ; decimales... GW pone 0 delante del punto si cabe), las comas, el $,
        ; y el - si va delante
        mov dx, ax
        test dx, dx
        jnz .t1
        inc dx                      ; un «0» delante del punto, si cabe
        mov byte [v_us_cero], 1
        jmp .t1b
.t1:
        mov byte [v_us_cero], 0
.t1b:
        cmp byte [v_us_coma], 0
        je .t2
        mov ax, [v_us_nent]
        dec ax
        jle .t2
        push dx
        xor dx, dx
        mov bx, 3
        div bx
        pop dx
        add dx, ax                  ; una coma cada tres
.t2:
        cmp byte [v_us_dolar], 0
        je .t3
        inc dx
.t3:
        cmp byte [v_us_neg], 0
        je .t4
        cmp byte [v_us_mas], 0      ; sin + ni - detrás: el - va delante
        jne .t4
        inc dx
.t4:
        mov ax, [v_us_ent]
        cmp byte [v_us_mas], 1      ; (el + de delante ocupa también)
        jne .t5
        inc ax
        inc dx
.t5:
        ; ¿cabe? AX = el sitio; DX = lo que hace falta
        cmp dx, ax
        jbe .cabe
        ; si lo que sobra es solo el 0 de delante, se quita
        cmp byte [v_us_cero], 0
        je .no_cabe
        dec dx
        mov byte [v_us_cero], 0
        cmp dx, ax
        jbe .cabe
.no_cabe:
        mov al, '%'
        call pon_car
        mov ax, dx                  ; (sin relleno)
.cabe:
        sub ax, dx                  ; lo que sobra: relleno
        mov cx, ax
        mov al, ' '
        cmp byte [v_us_ast], 0
        je .r
        mov al, '*'
.r:
        jcxz .r_x
.rl:
        call pon_car
        loop .rl
.r_x:
        ; signo delante, $, cifras enteras con comas, punto, decimales
        cmp byte [v_us_mas], 1
        jne .s1
        mov al, '+'
        cmp byte [v_us_neg], 0
        je .s0
        mov al, '-'
.s0:
        call pon_car
        jmp .s2
.s1:
        cmp byte [v_us_mas], 0
        jne .s2
        cmp byte [v_us_neg], 0
        je .s2
        mov al, '-'
        call pon_car
.s2:
        cmp byte [v_us_dolar], 0
        je .s3
        mov al, '$'
        call pon_car
.s3:
        cmp byte [v_us_cero], 0
        je .s4
        mov al, '0'
        call pon_car
.s4:
        mov si, b_cifras
        mov cx, [v_us_nent]
        jcxz .punto
        mov dx, cx                  ; las que quedan (para las comas)
.ce:
        lodsb
        call pon_car
        dec dx
        jz .punto
        cmp byte [v_us_coma], 0
        je .ce_s
        push dx
        mov ax, dx
        xor dx, dx
        mov bx, 3
        div bx
        test dx, dx
        pop dx
        jnz .ce_s
        mov al, ','
        call pon_car
.ce_s:
        jmp .ce
.punto:
        cmp word [v_us_dec], 0xFFFF
        je .detras
        mov al, '.'
        call pon_car
        mov cx, [v_us_ndec]
        ; si hay menos cifras que decimales (0,05 con dos): ceros delante
        mov di, b_cifras
        call largo_asciz            ; CX? (lo pisa: se rehace)
        mov ax, cx                  ; cifras totales
        mov cx, [v_us_ndec]
        cmp ax, cx
        jae .dl
        push cx
        sub cx, ax
.d0:
        mov al, '0'
        call pon_car
        loop .d0
        pop cx
        mov cx, ax
.dl:
        jcxz .detras
.dd:
        lodsb
        test al, al
        jz .detras
        call pon_car
        loop .dd
.detras:
        cmp byte [v_us_mas], 2
        jne .dt1
        mov al, '+'
        cmp byte [v_us_neg], 0
        je .dt0
        mov al, '-'
.dt0:
        call pon_car
        jmp .x
.dt1:
        cmp byte [v_us_mas], 3
        jne .x
        mov al, ' '
        cmp byte [v_us_neg], 0
        je .dt2
        mov al, '-'
.dt2:
        call pon_car
.x:
        pop si
        ret

; ---- las cifras de redondeo(|ac| * 10^ndec), en b_cifras: CX cuántas
; (al menos una: «0»)
us_cifras:
        push si
        call f_ac_a_ac2
        mov cx, [v_us_ndec]
        jcxz .sin
        mov di, ac3
        call f_pot10
        call f_mul_ac2_ac3
.sin:
        mov si, f_medio_cte
        mov di, ac3
        call f_carga_cte
        mov di, ac2
        mov si, ac3
        call f_suma
        mov si, ac2
        mov di, ac4
        call f_copia
        cmp word [ac4 + AC_EXP], 64
        jg .grande
        call f_a_u64                ; v_aux2
        call u64_cifras             ; CX, b_cifras
        ; «0» si es cero: el entero ya lo da así
        pop si
        ret
.grande:
        mov byte [b_cifras], '9'
        mov byte [b_cifras + 1], 0
        mov cx, 20
        pop si
        ret

; ---- con exponente (^^^^): d.ddE+nn en el sitio de las cifras ----
us_exponente:
        ; cifras significativas: las enteras (menos una, para el signo, si
        ; no hay + ni - detrás) y las decimales
        mov cx, [v_us_ent]
        cmp byte [v_us_mas], 0
        jne .e1
        dec cx                      ; (una posición para el signo)
.e1:
        test cx, cx
        jge .e2
        xor cx, cx
.e2:
        mov [v_us_nent], cx
        add cx, [v_us_ndec]
        jnz .e3
        inc cx
.e3:
        mov [v_us_sig], cx          ; cifras significativas
        ; e10 y las cifras: con num_a_texto no; a mano como en num_a_texto
        mov ax, [ac + AC_EXP]
        dec ax
        mov bx, 19728
        imul bx
        mov [v_us_e10], dx
.esc:
        call f_ac_a_ac2
        mov cx, [v_us_sig]
        dec cx
        sub cx, [v_us_e10]
        push cx
        mov di, ac3
        test cx, cx
        jns .p
        neg cx
.p:
        call f_pot10
        pop cx
        mov di, ac2
        mov si, ac3
        test cx, cx
        js .dv
        call f_mul
        jmp .r
.dv:
        call f_div
.r:
        mov si, f_medio_cte
        mov di, ac3
        call f_carga_cte
        mov di, ac2
        mov si, ac3
        call f_suma
        mov si, ac2
        mov di, ac4
        call f_copia
        call f_a_u64
        call u64_cifras             ; CX
        cmp word [ac + AC_M3], 0
        je .ok
        cmp cx, [v_us_sig]
        je .ok
        jb .menos
        inc word [v_us_e10]
        jmp .esc
.menos:
        dec word [v_us_e10]
        jmp .esc
.ok:
        ; el exponente: e10 - (cifras enteras - 1)
        mov ax, [v_us_e10]
        mov cx, [v_us_nent]
        test cx, cx
        jz .e4
        sub ax, cx
        inc ax
        jmp .e5
.e4:
        inc ax                      ; .ddd: todo detrás del punto
.e5:
        mov [v_us_e10], ax
        ; el signo delante
        cmp byte [v_us_mas], 1
        jne .g1
        mov al, '+'
        cmp byte [v_us_neg], 0
        je .g0
        mov al, '-'
.g0:
        call pon_car
        jmp .g2
.g1:
        cmp byte [v_us_mas], 0
        jne .g2
        mov al, ' '
        cmp byte [v_us_neg], 0
        je .g1b
        mov al, '-'
.g1b:
        call pon_car
.g2:
        mov si, b_cifras
        mov cx, [v_us_nent]
        jcxz .g3
.gl:
        lodsb
        call pon_car
        loop .gl
.g3:
        cmp word [v_us_dec], 0xFFFF
        je .g4
        mov al, '.'
        call pon_car
        mov cx, [v_us_ndec]
        jcxz .g4
.gd:
        lodsb
        call pon_car
        loop .gd
.g4:
        mov al, 'E'
        call pon_car
        mov ax, [v_us_e10]
        mov bl, '+'
        test ax, ax
        jns .g5
        mov bl, '-'
        neg ax
.g5:
        push ax
        mov al, bl
        call pon_car
        pop ax
        mov bl, 10
        div bl
        add ax, '00'
        push ax
        call pon_car
        pop ax
        mov al, ah
        call pon_car
        jmp us_numero.detras
