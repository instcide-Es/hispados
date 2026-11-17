; =============================================================================
;  Hispa-BASIC - ejecutar: el bucle de las sentencias y cada sentencia
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  DS:SI va por el texto tokenizado. Una sentencia normal vuelve con RET y
;  SI detrás de ella; las que saltan (GOTO, NEXT…) ponen SI en el 0 que hay
;  justo antes de la línea a la que van, o en un «:» o el 0 del final de la
;  sentencia de la que vienen, y vuelven igual.
;
;  La pila de control (b_ctrl, hacia arriba) guarda marcos de 24 octetos:
;     +0 tipo ('F' PARA, 'G' LLAMAR, 'W' MIENTRAS)   +1 tipo de la variable
;     +2 línea   +4 texto   +6 la variable   +8 el límite   +16 el paso
; =============================================================================
MARCO       equ 24

; ---- ejecuta desde DS:SI hasta que se acaba la línea directa ----
ejecuta:
.sent:
        mov word [v_temp_n], 0
        cmp byte [v_corte], 0
        jne interrumpido
        lodsb
        cmp al, ':'
        je .sent
        cmp al, ' '
        je .sent
        test al, al
        jz .fin_linea
        cmp al, 0x80
        jae .codigo
        dec si
        call sent_let
.tras:
        call salta_esp
        test al, al
        jz .sent
        cmp al, ':'
        je .sent
        jmp err_sintaxis
.codigo:
        cmp al, 0xFE
        je .fe
        cmp al, 0xFF
        je .ff
        cmp al, 0xA1                ; ELSE al que se llega: se salta lo demás
        je .resto
        cmp al, 0x81
        jb err_sintaxis
        cmp al, 0xCA
        ja err_sintaxis
        sub al, 0x81
        xor ah, ah
        shl ax, 1
        mov bx, ax
        call [cs:tabla_sent + bx]
        jmp .tras
.resto:
        call sent_rem
        jmp .tras
.fe:
        lodsb
        cmp al, 0x83                ; SYSTEM
        je salir
        cmp al, 0x87
        je .kill
        cmp al, 0x81
        je .files
        jmp err_sintaxis
.kill:
        call sent_kill
        jmp .tras
.files:
        call sent_files
        jmp .tras
.ff:
        lodsb
        cmp al, 0x83                ; MID$( ) = ...
        jne err_sintaxis
        call sent_mid
        jmp .tras
.fin_linea:
        ; SI está tras el 0: ¿era la línea directa?
        cmp si, PROG_INI
        jb .directa
        cmp word [si], 0
        je .fin_prog
        mov ax, [si + 2]
        mov [v_linea], ax
        add si, 4
        cmp byte [v_tron], 0
        je .sent
        push ax
        mov al, '['
        call pon_car
        pop ax
        call pon_natural
        mov al, ']'
        call pon_car
        jmp .sent
.directa:
        ret
.fin_prog:
        mov word [v_cont_lin], 0
        jmp ordenes_ok

; ---- Ctrl+Inter ----
interrumpido:
        mov byte [v_corte], 0
        call guarda_cont
        call col_cero
        mov si, m_interr
        jmp para_con
; ---- STOP ----
sent_stop:
        call guarda_cont
        call col_cero
        mov si, m_parado
para_con:
        call pon_cad_cs
        cmp word [v_linea], 0xFFFF
        je .x
        mov si, m_en
        call pon_cad_cs
        mov ax, [v_linea]
        call pon_natural
.x:
        call pon_nl
        jmp ordenes_ok
guarda_cont:
        mov word [v_cont_lin], 0
        cmp word [v_linea], 0xFFFF
        je .x
        mov ax, [v_linea]
        mov [v_cont_lin], ax
        mov [v_cont_ptr], si
.x:
        ret
; ---- END ----
sent_end:
        mov word [v_cont_lin], 0
        cmp word [v_linea], 0xFFFF
        je .x
        call cierra_todo
        jmp ordenes_ok
.x:
        ret
; ---- CONT ----
sent_cont:
        mov ax, [v_cont_lin]
        test ax, ax
        jz .no
        mov [v_linea], ax
        mov si, [v_cont_ptr]
        ret
.no:
        mov al, 17
        jmp error

tabla_sent:
        dw sent_end, sent_for, sent_next, sent_data, sent_input, sent_dim
        dw sent_read, sent_let, sent_goto, sent_run, sent_if, sent_restore
        dw sent_gosub, sent_return, sent_rem, sent_stop, sent_print
        dw sent_clear, sent_list, sent_new, sent_on, sent_mal, sent_def
        dw sent_poke, sent_cont, sent_mal, sent_mal, sent_out, sent_lprint
        dw sent_list, sent_mal, sent_width, sent_rem, sent_tron, sent_troff
        dw sent_swap, sent_erase, sent_mal, sent_error, sent_mal, sent_mal
        dw sent_mal, sent_mal, sent_defstr, sent_defint, sent_defsng
        dw sent_defdbl, sent_line, sent_while, sent_wend, sent_mal, sent_mal
        dw sent_mal, sent_mal, sent_mal, sent_mal, sent_randomize, sent_mal
        dw sent_mal, sent_load, sent_mal, sent_save, sent_color, sent_cls
        dw sent_rem, sent_mal, sent_mal, sent_rem_sent, sent_beep, sent_mal
        dw sent_mal, sent_screen, sent_key, sent_locate
%if ($ - tabla_sent) != 74 * 2
%error "tabla_sent no tiene 74 entradas"
%endif
sent_mal:
        jmp err_sintaxis

; ---- REM (y ELSE, y lo que se salta): hasta el final de la línea ----
sent_rem:
.c:
        lodsb
        test al, al
        jnz .c
        dec si
        ret
; ---- DATA, al pasar por ella: hasta el «:» que no está entre comillas ----
sent_data:
        xor ah, ah
.c:
        lodsb
        test al, al
        jz .x
        cmp al, '"'
        jne .d
        xor ah, 1
.d:
        test ah, ah
        jnz .c
        cmp al, ':'
        jne .c
.x:
        dec si
        ret

; ---- LET y la asignación sin LET ----
sent_let:
        call salta_esp
        cmp al, 'A'
        jb err_sintaxis
        cmp al, 'Z'
        ja err_sintaxis
        mov al, 1
        call busca_var              ; BX, AL = tipo
        push bx
        push ax
        mov ah, 0xE7
        call espera
        call evalua
        pop ax
        pop bx
        jmp guarda_valor

; ---- PRINT ----
sent_lprint:
sent_print:
        call salta_esp
        test al, al
        jz .nl
        cmp al, ':'
        je .nl
        cmp al, 0xA1                ; (ELSE detrás)
        je .nl
.item:
        call salta_esp
        test al, al
        jz .nl
        cmp al, ':'
        je .nl
        cmp al, ';'
        je .pyc
        cmp al, ','
        je .coma
        cmp al, 0xCE                ; TAB(
        je .tab
        cmp al, 0xD2                ; SPC(
        je .spc
        call evalua
        cmp byte [ac + AC_TIPO], T_CAD
        je .cad
        call num_a_texto
        ; ¿cabe en lo que queda de línea? (como GW-BASIC)
        mov di, b_num
        call largo_asciz
        inc cx
        mov ax, [v_col]
        add ax, cx
        cmp ax, [v_ancho]
        jbe .cabe
        call col_cero
.cabe:
        push si
        mov si, b_num
        call pon_cad
        pop si
        mov al, ' '
        call pon_car
        jmp .sig
.cad:
        mov bx, [ac + AC_M0]
        call libera_temp
        call pon_desc
.sig:
        ; tras un dato: «;» o «,» lo siguiente; si no, se sigue igual
        call salta_esp
        test al, al
        jz .nl
        cmp al, ':'
        je .nl
        cmp al, 0xA1
        je .nl
        jmp .item
.pyc:
        inc si
        call salta_esp
        call fin_de_sent
        jne .item
        ret
.coma:
        inc si
        ; a la siguiente zona de 14; si esa zona no cabe entera en la línea,
        ; a la línea siguiente (como GW-BASIC: en 80 columnas, desde la 56)
        mov ax, [v_col]
        mov cl, 14
        div cl
        mov cl, 14
        sub cl, ah
        xor ch, ch                  ; CX = lo que falta hasta la zona
        mov ax, [v_col]
        add ax, cx
        add ax, 14
        cmp ax, [v_ancho]
        jbe .zc
        call pon_nl
        jmp .c_fin
.zc:
        mov al, ' '
        call pon_car
        loop .zc
.c_fin:
        call salta_esp
        call fin_de_sent
        jne .item
        ret
.tab:
        inc si
        call eval_ent
        push ax
        ESPERA ')'
        pop ax
        dec ax                      ; columna desde 0
        js .t_cero
        xor dx, dx
        div word [v_ancho]
        mov ax, dx
        jmp .t1
.t_cero:
        xor ax, ax
.t1:
        cmp ax, [v_col]
        jae .t2
        call pon_nl
.t2:
        mov cx, ax
        sub cx, [v_col]
        jbe .t_x
.tc:
        mov al, ' '
        call pon_car
        loop .tc
.t_x:
        jmp .tras_tab
.spc:
        inc si
        call eval_ent
        push ax
        ESPERA ')'
        pop cx
        test cx, cx
        jle .tras_tab
        cmp cx, 255
        jbe .sc
        mov cx, 255
.sc:
        mov al, ' '
        call pon_car
        loop .sc
.tras_tab:
        ; TAB y SPC llevan un «;» implícito
        call salta_esp
        cmp al, ';'
        jne .tt
        inc si
.tt:
        call salta_esp
        call fin_de_sent
        jne .item
        ret
.nl:
        jmp pon_nl

; ¿AL es el final de una sentencia (0, «:», ELSE)? ZF=1
fin_de_sent:
        test al, al
        jz .x
        cmp al, ':'
        je .x
        cmp al, 0xA1
.x:
        ret
; longitud del texto ASCIZ de DI: CX
largo_asciz:
        push di
        xor cx, cx
.c:
        cmp byte [di], 0
        je .x
        inc di
        inc cx
        jmp .c
.x:
        pop di
        ret
; escribe la cadena del descriptor BX
pon_desc:
        push si
        push cx
        mov cl, [bx]
        xor ch, ch
        mov si, [bx + 1]
        jcxz .x
.c:
        lodsb
        call pon_car
        loop .c
.x:
        pop cx
        pop si
        ret

; ---- GOTO ----
sent_goto:
        call lee_linea_cte          ; AX = el número
va_a_linea:                         ; AX = la línea; SI queda antes de ella
        call busca_linea
        jnc .no
        lea si, [bx - 1]
        ret
.no:
        mov al, 8
        jmp error
; lee un número de línea (constante 0Eh) de DS:SI: AX
lee_linea_cte:
        call salta_esp
        cmp al, 0x0E
        jne .otro
        inc si
        lodsw
        ret
.otro:
        ; (por si viene como número normal)
        call eval_ent
        ret

; ---- GOSUB ----
sent_gosub:
        call lee_linea_cte
        push ax
        call marco_nuevo            ; DI = el marco
        mov byte [di], 'G'
        mov ax, [v_linea]
        mov [di + 2], ax
        mov [di + 4], si
        pop ax
        jmp va_a_linea
; un marco nuevo en la pila de control: DI
marco_nuevo:
        mov di, [v_ctrl]
        cmp di, b_ctrl + CTRL_TAM - MARCO
        ja .llena
        add word [v_ctrl], MARCO
        ret
.llena:
        mov al, 7
        jmp error
; ---- RETURN ----
sent_return:
        ; quita los marcos hasta el de un GOSUB
.b:
        mov di, [v_ctrl]
        cmp di, b_ctrl
        jbe .no
        sub di, MARCO
        mov [v_ctrl], di
        cmp byte [di], 'G'
        jne .b
        call salta_esp
        call fin_de_sent
        jne .con_linea
        mov ax, [di + 2]
        mov [v_linea], ax
        mov si, [di + 4]
        ret
.con_linea:
        mov ax, [di + 2]
        mov [v_linea], ax
        call lee_linea_cte
        jmp va_a_linea
.no:
        mov al, 3
        jmp error

; ---- IF ----
sent_if:
        call evalua
        cmp byte [ac + AC_TIPO], T_CAD
        je err_tipos
        call salta_esp
        cmp al, 0xCD                ; THEN
        je .then
        cmp al, 0x89                ; GOTO
        jne err_sintaxis
.then:
        inc si
        mov ah, al
        ; ¿se cumple? (distinto de cero)
        cmp byte [ac + AC_TIPO], T_ENT
        jne .flot
        cmp word [ac + AC_M0], 0
        jmp .ver
.flot:
        cmp word [ac + AC_M3], 0
.ver:
        je .falso
        ; cierto: un número de línea o sentencias
        call salta_esp
        cmp al, 0x0E
        je sent_goto
        cmp ah, 0x89
        je sent_goto
        pop ax                      ; (la vuelta: se sigue en el bucle)
        jmp ejecuta.sent
.falso:
        ; buscar el ELSE que le toca (contando los IF de dentro)
        xor cx, cx
.b:
        call siguiente_codigo       ; AL = el siguiente código o 0
        test al, al
        jz .sin_else
        cmp al, 0x8B
        jne .b1
        inc cx
        jmp .b
.b1:
        cmp al, 0xA1
        jne .b
        jcxz .else
        dec cx
        jmp .b
.else:
        call salta_esp
        cmp al, 0x0E
        je sent_goto
        pop ax
        jmp ejecuta.sent
.sin_else:
        dec si                      ; en el 0
        ret

; avanza SI por la línea hasta el siguiente código de palabra (>= 80h),
; saltando comillas, constantes y lo de detrás de REM. AL = el código (los
; de dos octetos, el primero) o 0 si se acaba la línea (SI tras el 0)
siguiente_codigo:
        cmp byte [v_sc_data], 0
        je .c
        mov byte [v_sc_data], 0
        call sent_data
.c:
        lodsb
        test al, al
        jz .x
        cmp al, '"'
        je .com
        cmp al, 0x20
        jb .cte
        cmp al, 0x80
        jb .c
        cmp al, 0x8F                ; REM: hasta el final
        je .rem
        cmp al, 0x84                ; DATA: lo de detrás se salta la próxima vez
        jne .no_data
        mov byte [v_sc_data], 1
        ret
.no_data:
        cmp al, 0xFD
        jb .x
        inc si                      ; (el segundo octeto)
.x:
        ret
.com:
        lodsb
        test al, al
        jz .x
        cmp al, '"'
        jne .com
        jmp .c
.rem:
        call sent_rem
        lodsb
        ret
.cte:
        call salta_cte
        jmp .c
; salta lo que ocupa la constante cuyo código (AL) se acaba de leer
salta_cte:
        cmp al, 0x0F
        je .uno
        cmp al, 0x1D
        je .cuatro
        cmp al, 0x1F
        je .ocho
        cmp al, 0x0B
        je .dos
        cmp al, 0x0C
        je .dos
        cmp al, 0x0E
        je .dos
        cmp al, 0x1C
        je .dos
        ret
.ocho:
        add si, 4
.cuatro:
        add si, 2
.dos:
        inc si
.uno:
        inc si
        ret

; ---- ON n GOTO / GOSUB ----
sent_on:
        call salta_esp
        cmp al, 0xA7                ; ON ERROR (en la 0.12)
        je err_sintaxis
        call eval_ent
        test ax, ax
        js .mal
        cmp ax, 255
        ja .mal
        mov cx, ax
        call salta_esp
        inc si
        cmp al, 0x89
        je .ir
        cmp al, 0x8D
        jne err_sintaxis
.ir:
        mov dl, al                  ; GOTO o GOSUB
        ; la n-ésima línea de la lista
        jcxz .ninguna
.l:
        call lee_linea_cte
        dec cx
        jz .esta
        call salta_esp
        cmp al, ','
        jne .ninguna_ya             ; no hay tantas
        inc si
        jmp .l
.esta:
        ; saltar el resto de la lista (para el GOSUB: volver detrás)
        push ax
.r:
        call salta_esp
        cmp al, ','
        jne .r_fin
        inc si
        call lee_linea_cte
        jmp .r
.r_fin:
        pop ax
        cmp dl, 0x89
        je va_a_linea
        push ax
        call marco_nuevo
        mov byte [di], 'G'
        mov ax, [v_linea]
        mov [di + 2], ax
        mov [di + 4], si
        pop ax
        jmp va_a_linea
.ninguna:
        ; 0: se sigue detrás de la sentencia
.ninguna_ya:
        call salta_esp
        call fin_de_sent
        je .x
        inc si
        cmp al, '"'
        jne .ninguna_ya
        jmp .ninguna_ya
.x:
        ret
.mal:
        jmp err_funcion

; ---- FOR ----
sent_for:
        call salta_esp
        mov al, 1
        call busca_var              ; BX = la variable, AL = su tipo
        cmp al, T_CAD
        je err_tipos
        push bx
        push ax
        ESPERA 0xE7
        call eval_num
        pop ax
        pop bx
        push bx
        push ax
        call guarda_valor
        ; si ya hay un PARA con esta variable, se quita (y lo de encima)
        pop ax
        pop bx
        call quita_for_de           ; BX
        call marco_nuevo
        mov byte [di], 'F'
        mov [di + 1], al
        mov [di + 6], bx
        push di
        ESPERA 0xCC                 ; TO
        call eval_num
        pop di
        push di
        lea bx, [di + 8]
        mov al, [di + 1]
        call guarda_paso            ; el límite
        call salta_esp
        cmp al, 0xCF                ; STEP
        jne .uno
        inc si
        call eval_num
        jmp .paso
.uno:
        mov ax, 1
        call ac_entero
.paso:
        pop di
        lea bx, [di + 16]
        mov al, [di + 1]
        call guarda_paso
        mov ax, [v_linea]
        mov [di + 2], ax
        mov [di + 4], si
        ; ¿se entra? (GW-BASIC lo mira al empezar; si no, va al NEXT que le
        ; toca y hace allí la cuenta una vez)
        call for_sigue              ; ZF=1 si ya está fuera
        jne .x
        ; buscar su NEXT
        xor cx, cx
.b:
        call siguiente_codigo
        test al, al
        jnz .b_cod
        ; fin de línea: la siguiente
        cmp si, PROG_INI
        jb .sin_next
        cmp word [si], 0
        je .sin_next
        mov ax, [si + 2]
        mov [v_linea], ax
        add si, 4
        jmp .b
.b_cod:
        cmp al, 0x82
        jne .b1
        inc cx
        jmp .b
.b1:
        cmp al, 0x83
        jne .b
        jcxz .su_next
        dec cx
        jmp .b
.su_next:
        ; el NEXT, como si se ejecutara (con la variable de este PARA)
        jmp sent_next
.sin_next:
        mov al, 26
        jmp error
.x:
        ret
; guarda el acumulador (número) en BX como límite/paso del tipo AL: un
; entero en 2 octetos; si no, en doble (8 octetos)
guarda_paso:
        push di
        cmp al, T_ENT
        jne .flot
        mov di, ac
        call a_entero
        mov [bx], ax
        pop di
        ret
.flot:
        push ax
        mov di, ac
        call a_flot
        pop ax
        mov [ac + AC_TIPO], al
        mov di, ac
        call f_redondea
        mov di, bx
        call ac_a_mbf8
        pop di
        ret
; quita de la pila de control el PARA de la variable BX (y lo de encima)
quita_for_de:
        push di
        mov di, [v_ctrl]
.b:
        cmp di, b_ctrl
        jbe .x
        sub di, MARCO
        cmp byte [di], 'F'
        jne .x                      ; (no se mira más allá de un GOSUB)
        cmp [di + 6], bx
        jne .b
        mov [v_ctrl], di
.x:
        pop di
        ret
; ¿sigue el PARA del marco DI? (la variable frente al límite, según el
; signo del paso). ZF=1 si ya no
for_sigue:
        push si
        push di
        mov bx, [di + 6]
        mov al, [di + 1]
        cmp al, T_ENT
        jne .flot
        mov ax, [bx]
        mov dx, [di + 8]            ; límite
        test word [di + 16], 0x8000
        jnz .ent_neg
        cmp ax, dx
        jg .fuera
        jmp .dentro
.ent_neg:
        cmp ax, dx
        jl .fuera
        jmp .dentro
.flot:
        ; ac = la variable; arg = el límite
        push di
        call carga_valor
        pop di
        lea si, [di + 8]
        push di
        mov di, arg
        call mbf8_a
        pop di
        mov al, [di + 16 + 6]       ; el signo del paso (octeto alto de la
        push ax                     ;  mantisa en MBF)
        mov di, ac
        mov si, arg
        mov byte [ac + AC_TIPO], T_DOB
        call f_cmp                  ; AX: variable frente a límite
        pop dx
        test dl, 0x80
        jnz .f_neg
        cmp ax, 1
        je .fuera
        jmp .dentro
.f_neg:
        cmp ax, -1
        je .fuera
.dentro:
        or al, 1                    ; ZF=0
        jmp .x
.fuera:
        xor al, al                  ; ZF=1
.x:
        pop di
        pop si
        ret

; ---- NEXT ----
sent_next:
        call salta_esp
        call fin_de_sent
        jne .con_var
        ; sin variable: el PARA de encima
        mov di, [v_ctrl]
        cmp di, b_ctrl
        jbe .sin_for
        sub di, MARCO
        cmp byte [di], 'F'
        jne .sin_for
        jmp .cuenta
.con_var:
        mov al, 1
        call busca_var              ; BX
        mov di, [v_ctrl]
.b:
        cmp di, b_ctrl
        jbe .sin_for
        sub di, MARCO
        cmp byte [di], 'F'
        jne .sin_for
        cmp [di + 6], bx
        jne .b
        lea ax, [di + MARCO]
        mov [v_ctrl], ax            ; los de encima, fuera
.cuenta:
        ; variable += paso
        mov bx, [di + 6]
        mov al, [di + 1]
        cmp al, T_ENT
        jne .flot
        mov ax, [bx]
        add ax, [di + 16]
        jo .desb
        mov [bx], ax
        jmp .mira
.flot:
        push si
        push di
        call carga_valor            ; ac = la variable
        pop di
        push di
        lea si, [di + 16]
        mov di, arg
        call mbf8_a                 ; arg = el paso
        mov di, ac
        mov si, arg
        call f_suma
        pop di
        mov al, [di + 1]
        mov bx, [di + 6]
        push di
        call guarda_valor           ; redondea a su tipo
        pop di
        pop si
.mira:
        call for_sigue
        je .acabado
        ; otra vuelta
        mov ax, [di + 2]
        mov [v_linea], ax
        mov si, [di + 4]
        ret
.acabado:
        ; fuera el marco; ¿más variables? (NEXT I, J)
        mov [v_ctrl], di
        call salta_esp
        cmp al, ','
        jne .x
        inc si
        jmp .con_var
.x:
        ret
.sin_for:
        mov al, 1
        jmp error
.desb:
        jmp err_desb

; ---- WHILE / WEND ----
sent_while:
        ; (el E9 de detrás del código)
        cmp byte [si], 0xE9
        jne .w
        inc si
.w:
        lea ax, [si - 1]            ; dónde vuelve WEND (tras el B1)
        cmp byte [si - 1], 0xE9
        jne .w1
        dec ax
.w1:
        push ax
        call evalua
        pop dx
        cmp byte [ac + AC_TIPO], T_CAD
        je err_tipos
        cmp byte [ac + AC_TIPO], T_ENT
        jne .flot
        cmp word [ac + AC_M0], 0
        jmp .ver
.flot:
        cmp word [ac + AC_M3], 0
.ver:
        je .falso
        call marco_nuevo
        mov byte [di], 'W'
        mov ax, [v_linea]
        mov [di + 2], ax
        mov [di + 4], dx
        ret
.falso:
        ; al WEND que le toca
        xor cx, cx
.b:
        call siguiente_codigo
        test al, al
        jnz .cod
        cmp si, PROG_INI
        jb .sin
        cmp word [si], 0
        je .sin
        mov ax, [si + 2]
        mov [v_linea], ax
        add si, 4
        jmp .b
.cod:
        cmp al, 0xB1
        jne .c1
        inc cx
        jmp .b
.c1:
        cmp al, 0xB2
        jne .b
        jcxz .x
        dec cx
        jmp .b
.x:
        ret
.sin:
        mov al, 29
        jmp error
sent_wend:
        mov di, [v_ctrl]
        cmp di, b_ctrl
        jbe .sin
        sub di, MARCO
        cmp byte [di], 'W'
        jne .sin
        mov [v_ctrl], di
        mov ax, [di + 2]
        mov [v_linea], ax
        mov si, [di + 4]
        inc si                      ; tras el B1
        jmp sent_while
.sin:
        mov al, 30
        jmp error

; ---- DIM ----
sent_dim:
.var:
        call salta_esp
        call lee_nombre             ; AL = el tipo
        mov dh, al
        ESPERA '('
        call guarda_nombre
        xor cx, cx
.d:
        push cx
        call eval_ent
        pop cx
        test ax, ax
        js .fuera
        inc ax
        jz .fuera
        push ax                     ; los tamaños, en la pila
        inc cx
        cmp cx, 8
        ja err_sintaxis
        call salta_esp
        inc si
        cmp al, ','
        je .d
        cmp al, ')'
        jne err_sintaxis
        call repon_nombre           ; b_nom y DH
        ; los tamaños, de la pila a v_aux2 (están al revés)
        mov bx, cx
        shl bx, 1
.t:
        sub bx, 2
        pop word [v_aux2 + bx]
        jnz .t
        push cx
        call busca_matriz
        pop cx
        jnc .ya_esta
        call crea_matriz
        call salta_esp
        cmp al, ','
        jne .x
        inc si
        jmp .var
.x:
        ret
.ya_esta:
        mov al, 10
        jmp error
.fuera:
        mov al, 9
        jmp error

; ---- ERASE ----
sent_erase:
.v:
        call salta_esp
        call lee_nombre
        mov dh, al
        call busca_matriz
        jc .no
        ; lo que ocupa
        mov al, [bx + 1]
        xor ah, ah
        mov di, bx
        add di, ax
        add di, 2
        mov cx, [di]
        add cx, 2
        add cx, ax
        add cx, 2                   ; todo
        push si
        mov di, bx
        mov si, bx
        add si, cx
        push cx
        mov cx, [v_libre]
        sub cx, si
        rep movsb
        pop cx
        sub [v_libre], cx
        pop si
        call salta_esp
        cmp al, ','
        jne .x
        inc si
        jmp .v
.x:
        ret
.no:
        jmp err_funcion

; ---- READ / RESTORE ----
sent_restore:
        mov word [v_dat_ptr], 0
        call salta_esp
        call fin_de_sent
        je .x
        call lee_linea_cte
        call busca_linea
        jnc .no
        lea ax, [bx - 1]
        mov [v_dat_ptr], ax         ; se busca el DATA desde ahí
        mov byte [v_dat_dentro], 0
.x:
        ret
.no:
        mov al, 8
        jmp error
sent_read:
.var:
        call salta_esp
        mov al, 1
        call busca_var              ; BX, AL
        push bx
        push ax
        call siguiente_dato         ; DS:DI = el dato; CX su largo; DL comillas
        pop ax
        pop bx
        push si
        call asigna_dato            ; AL = tipo, BX = variable
        pop si
        jnc .bien
        mov ax, [v_dat_lin]         ; GW-BASIC lo da en la línea del DATA
        mov [v_linea], ax
        jmp err_sintaxis
.bien:
        call salta_esp
        cmp al, ','
        jne .x
        inc si
        jmp .var
.x:
        ret

; el siguiente dato de los DATA: DI al principio, CX su largo (sin las
; comillas), DL = 1 si iba entre comillas. v_dat_ptr queda detrás
siguiente_dato:
        push si
        mov si, [v_dat_ptr]
        test si, si
        jnz .hay
        mov si, PROG_INI - 1
        mov byte [v_dat_dentro], 0
.hay:
        cmp byte [v_dat_dentro], 0
        jne .en_data
        ; buscar el siguiente DATA
.b:
        call siguiente_codigo
        test al, al
        jnz .b_cod
        cmp si, PROG_INI
        jb .sin
        cmp word [si], 0
        je .sin
        mov ax, [si + 2]
        mov [v_dat_lin], ax
        add si, 4
        jmp .b
.b_cod:
        cmp al, 0x84
        jne .b
        mov byte [v_sc_data], 0
        mov byte [v_dat_dentro], 1
        jmp .uno
.en_data:
        ; tras un dato: «,» sigue; «:» o 0, se acabó este DATA
        lodsb
        cmp al, ','
        je .uno
        dec si
        mov byte [v_dat_dentro], 0
        jmp .b
.uno:
        ; saltar espacios
.e:
        lodsb
        cmp al, ' '
        je .e
        dec si
        xor dl, dl
        cmp al, '"'
        jne .sin_com
        inc si
        mov di, si
        mov dl, 1
.c1:
        lodsb
        test al, al
        jz .c_fin0
        cmp al, '"'
        jne .c1
        mov cx, si
        sub cx, di
        dec cx
        ; tras las comillas, hasta la coma
.c2:
        lodsb
        cmp al, ' '
        je .c2
        dec si
        jmp .listo
.c_fin0:
        dec si
        mov cx, si
        sub cx, di
        jmp .listo
.sin_com:
        mov di, si
.s1:
        lodsb
        test al, al
        jz .s_fin
        cmp al, ','
        je .s_fin
        cmp al, ':'
        jne .s1
.s_fin:
        dec si
        mov cx, si
        sub cx, di
        ; sin los espacios de detrás
.s2:
        jcxz .listo
        mov bx, di
        add bx, cx
        cmp byte [bx - 1], ' '
        jne .listo
        dec cx
        jmp .s2
.listo:
        mov [v_dat_ptr], si
        pop si
        ret
.sin:
        mov al, 4
        jmp error

; asigna el texto de DI (CX octetos; DL = 1 si iba entre comillas) a la
; variable BX de tipo AL, como hacen READ e INPUT. CF=1 si no es un número
; y la variable es numérica
asigna_dato:
        cmp al, T_CAD
        je .cad
        test dl, dl
        jnz .no_num
        ; un número: lee_numero sobre una copia ASCIZ en b_num
        push ax
        push bx
        cmp cx, 40
        ja .no_num_p
        push si
        mov si, di
        mov di, b_num
        push cx
        rep movsb
        pop cx
        mov byte [di], 0
        mov si, b_num
.esp:
        lodsb
        cmp al, ' '
        je .esp
        dec si
        ; un signo delante
        xor dh, dh
        cmp al, '-'
        jne .mas
        inc si
        mov dh, 1
        jmp .num
.mas:
        cmp al, '+'
        jne .num
        inc si
.num:
        mov al, [si]
        test al, al
        jz .vacio
        cmp al, '&'
        je .hex
        call es_cifra
        jc .es
        cmp al, '.'
        jne .no_num_s
.es:
        push dx
        call lee_numero
        pop dx
.cmp_fin:
        ; tiene que acabar ahí (o en espacios)
.f:
        lodsb
        cmp al, ' '
        je .f
        test al, al
        jnz .no_num_s
        test dh, dh
        jz .pos
        call op_cambia_signo
.pos:
        pop si
        pop bx
        pop ax
        call guarda_valor
        clc
        ret
.vacio:
        ; vacío: vale 0
        xor ax, ax
        call ac_entero
        jmp .pos
.hex:
        inc si
        mov al, [si]
        call mayus
        mov bl, 16
        cmp al, 'H'
        je .h1
        mov bl, 8
        cmp al, 'O'
        jne .h2
.h1:
        inc si
.h2:
        call lee_base
        call ac_entero
        jmp .cmp_fin
.no_num_s:
        pop si
.no_num_p:
        pop bx
        pop ax
.no_num:
        stc
        ret
.cad:
        push si
        mov si, di
        push bx
        call cadena_de              ; ac = la cadena (copiada)
        pop bx
        mov al, T_CAD
        call guarda_valor
        pop si
        clc
        ret

; ---- INPUT ----
sent_input:
        ; ¿un «;» delante? (no salta de línea al acabar: no se usa aquí)
        call salta_esp
        cmp al, ';'
        jne .sin_pyc
        inc si
.sin_pyc:
        ; ¿un texto?
        mov word [v_in_txt], 0    ; el texto (descriptor), si lo hay
        mov byte [v_in_preg], 1    ; 1: con «? »
        call salta_esp
        cmp al, '"'
        jne .vars
        call ev_atomo               ; la cadena
        mov bx, [ac + AC_M0]
        mov al, [bx]
        mov [v_desc_aux], al
        mov ax, [bx + 1]
        mov [v_desc_aux + 1], ax
        mov word [v_in_txt], v_desc_aux
        call salta_esp
        inc si
        cmp al, ';'
        je .vars
        cmp al, ','
        jne err_sintaxis
        mov byte [v_in_preg], 0
.vars:
        mov [v_in_vars], si        ; dónde empiezan las variables
.pregunta:
        mov bx, [v_in_txt]
        test bx, bx
        jz .sin_texto
        call pon_desc
.sin_texto:
        cmp byte [v_in_preg], 0
        je .lee
        mov al, '?'
        call pon_car
        mov al, ' '
        call pon_car
.lee:
        call lee_linea_ent
        jc salir
        ; primera pasada: ¿cuadra todo? segunda: asignar
        mov byte [v_in_pasada], 0
.pasada:
        mov si, [v_in_vars]
        mov di, b_lin
.una:
        push di
        call salta_esp
        xor al, al                  ; sin crear en la primera pasada
        cmp byte [v_in_pasada], 0
        je .bv
        mov al, 1
.bv:
        call busca_var              ; BX, AL
        pop di
        push si
        push bx
        push ax
        call dato_de_linea          ; DI -> el dato (DI, CX, DL); v_aux+8 tras él
        pop ax
        pop bx
        jc .mal_p
        cmp byte [v_in_pasada], 0
        jne .asigna
        ; solo mirar
        cmp al, T_CAD
        je .vale
        test dl, dl
        jnz .mal_p
        push bx
        mov bx, v_cero_tmp          ; (se prueba en una variable de paso)
        call asigna_dato
        pop bx
        jc .mal_p
        jmp .vale
.asigna:
        call asigna_dato
.vale:
        pop si
        mov di, [v_in_tras]
        call salta_esp
        cmp al, ','
        jne .fin_vars
        inc si
        ; tiene que haber otro dato
        cmp byte [di], ','
        jne .mal
        inc di
        jmp .una
.fin_vars:
        ; no puede sobrar nada
        cmp byte [di], 0
        jne .mal
        cmp byte [v_in_pasada], 0
        jne .x
        mov byte [v_in_pasada], 1
        jmp .pasada
.x:
        ret
.mal_p:
        pop si
.mal:
        mov si, m_repita
        call pon_cad_cs
        jmp .pregunta

; un dato de la línea tecleada (DI): DI = su principio, CX su largo, DL = 1
; si iba entre comillas; [v_aux+8] = detrás (en la coma o el 0)
dato_de_linea:
        mov si, di
.e:
        lodsb
        cmp al, ' '
        je .e
        dec si
        xor dl, dl
        cmp al, '"'
        jne .sin
        inc si
        mov di, si
        mov dl, 1
.c:
        lodsb
        test al, al
        jz .c0
        cmp al, '"'
        jne .c
        mov cx, si
        sub cx, di
        dec cx
.c2:
        lodsb
        cmp al, ' '
        je .c2
        dec si
        jmp .x
.c0:
        dec si
        mov cx, si
        sub cx, di
        jmp .x
.sin:
        mov di, si
.s:
        lodsb
        test al, al
        jz .s_fin
        cmp al, ','
        jne .s
.s_fin:
        dec si
        mov cx, si
        sub cx, di
.s2:
        jcxz .x
        mov bx, di
        add bx, cx
        cmp byte [bx - 1], ' '
        jne .x
        dec cx
        jmp .s2
.x:
        mov [v_in_tras], si
        clc
        ret

; ---- LINE INPUT ----
sent_line:
        call salta_esp
        cmp al, 0x85                ; INPUT
        jne err_sintaxis
        inc si
        call salta_esp
        cmp al, ';'
        jne .t
        inc si
        call salta_esp
.t:
        cmp al, '"'
        jne .v
        call ev_atomo
        mov bx, [ac + AC_M0]
        call pon_desc
        ESPERA ';'
.v:
        mov al, 1
        call busca_var
        cmp al, T_CAD
        jne err_tipos
        push bx
        push si
        call lee_linea_ent
        jc salir
        mov di, b_lin
        call largo_asciz
        mov si, b_lin
        call cadena_de
        pop si
        pop bx
        mov al, T_CAD
        jmp guarda_valor

; ---- DEFINT / DEFSNG / DEFDBL / DEFSTR letra[-letra],... ----
sent_defint:
        mov dl, T_ENT
        jmp def_tipos
sent_defsng:
        mov dl, T_SIM
        jmp def_tipos
sent_defdbl:
        mov dl, T_DOB
        jmp def_tipos
sent_defstr:
        mov dl, T_CAD
def_tipos:
        call salta_esp
        call es_letra
        jnc err_sintaxis
        inc si
        mov bl, al
        mov bh, al
        call salta_esp
        cmp al, 0xEA                ; «-»
        jne .pon
        inc si
        call salta_esp
        call es_letra
        jnc err_sintaxis
        inc si
        mov bh, al
.pon:
        cmp bh, bl
        jb err_sintaxis
.p:
        push bx
        sub bl, 'A'
        xor bh, bh
        mov [v_def + bx], dl
        pop bx
        inc bl
        cmp bl, bh
        jbe .p
        call salta_esp
        cmp al, ','
        jne .x
        inc si
        jmp def_tipos
.x:
        ret

; ---- DEF FN / DEF SEG ----
sent_def:
        call salta_esp
        cmp al, 0xD1                ; FN
        je .fn
        ; DEF SEG [= n]
        cmp word [si], 'SE'
        jne err_sintaxis
        cmp byte [si + 2], 'G'
        jne err_sintaxis
        add si, 3
        call salta_esp
        cmp al, 0xE7
        je .seg_n
        mov ax, ds
        mov [v_defseg], ax
        ret
.seg_n:
        inc si
        call evalua
        mov di, ac
        call a_flot
        mov di, ac
        call f_trunca               ; (0 a 65535)
        mov di, ac
        call a_palabra
        mov [v_defseg], ax
        ret
.fn:
        cmp word [v_linea], 0xFFFF
        je .directo
        inc si
        call nombre_fn              ; b_nom = la clave; DH = el tipo
        ; crea (o rehace) la entrada: valor = dónde sigue el texto
        call busca_fn
        jnc .esta
        call crea_fn
.esta:
        mov [bx], si
        ; los parámetros tienen que existir ya (para no crearlos al llamar)
        call salta_esp
        cmp al, '('
        jne .sin_par
        inc si
.par:
        mov al, 1
        call busca_var
        call salta_esp
        inc si
        cmp al, ','
        je .par
        cmp al, ')'
        jne err_sintaxis
.sin_par:
        ; lo demás (= expresión) se salta
        jmp sent_rem_sent
.directo:
        mov al, 12
        jmp error
; salta hasta el final de la sentencia (el «:» o el 0, sin pasarlo)
sent_rem_sent:
.c:
        lodsb
        test al, al
        jz .x
        cmp al, ':'
        je .x
        cmp al, '"'
        je .com
        cmp al, 0x8F
        je .rem
        cmp al, 0x84
        je .data
        cmp al, 0x20
        jb .cte
        cmp al, 0xFD
        jb .c
        inc si
        jmp .c
.com:
        lodsb
        test al, al
        jz .x
        cmp al, '"'
        jne .com
        jmp .c
.rem:
        jmp sent_rem
.data:
        jmp sent_data
.cte:
        call salta_cte
        jmp .c
.x:
        dec si
        ret

; ---- RANDOMIZE ----
sent_randomize:
        call salta_esp
        call fin_de_sent
        jne .con
        ; sin número: lo pregunta, como GW-BASIC
.preg:
        mov si, m_semilla
        call pon_cad_cs
        call lee_linea_ent
        jc salir
        push si
        mov di, b_lin
        call largo_asciz
        mov dl, 0
        mov al, T_SIM
        mov bx, v_cero_tmp
        call asigna_dato
        pop si
        jc .preg
        push si
        mov si, v_cero_tmp
        call mbf4_a_ac
        pop si
        jmp .sem
.con:
        call eval_num
.sem:
        ; la semilla: la mantisa del número (como RND con negativo), más
        ; lo que lleve el exponente
        mov di, v_aux2
        mov bx, ac
        call a_mbf4
        mov ax, [v_aux2]
        mov dl, [v_aux2 + 2]
        add ah, [v_aux2 + 3]
        mov [v_azar], ax
        mov [v_azar + 2], dl
        ret

; ---- SWAP ----
sent_swap:
        mov al, 1
        call busca_var
        push bx
        push ax
        ESPERA ','
        mov al, 1
        call busca_var
        pop cx
        pop di
        cmp al, cl
        jne err_tipos
        ; intercambiar AL octetos de BX y DI
        xor ah, ah
        mov cx, ax
.c:
        mov al, [bx]
        xchg al, [di]
        mov [bx], al
        inc bx
        inc di
        loop .c
        ret

; ---- CLEAR [, n] ----
sent_clear:
        call cierra_todo
        call vacia
        jmp sent_rem_sent

; ---- NEW ----
sent_new:
        call cierra_todo
        call nuevo
        jmp ordenes_ok

; ---- RUN [línea | "archivo"] ----
sent_run:
        call salta_esp
        cmp al, '"'
        je .archivo
        push ax
        call cierra_todo
        call vacia
        mov word [v_cont_lin], 0
        pop ax
        cmp al, 0x0E
        je .linea
        mov si, PROG_INI - 1
        ret
.linea:
        call lee_linea_cte
        jmp va_a_linea
.archivo:
        mov byte [v_cargar_y_ejecutar], 1
        jmp sent_load_r

; ---- LIST [n][-[m]] ----
sent_list:
        mov al, [v_listar_es]
        mov [v_idioma], al
        xor bx, bx                  ; desde
        mov dx, 65535               ; hasta
        call salta_esp
        cmp al, 0x0E
        jne .menos
        call lee_linea_cte
        mov bx, ax
        mov dx, ax
        call salta_esp
        cmp al, 0xEA
        jne .rango
        mov dx, 65535
.menos:
        cmp al, 0xEA
        jne .rango
        inc si
        call salta_esp
        cmp al, 0x0E
        jne .rango
        push bx
        call lee_linea_cte
        pop bx
        mov dx, ax
.rango:
        push si
        mov ax, bx
        push dx
        call busca_linea            ; BX = la primera
        pop dx
.l:
        cmp word [bx], 0
        je .x
        cmp [bx + 2], dx
        ja .x
        cmp byte [v_corte], 0
        jne .x
        call lista_linea
        mov bx, [bx]
        jmp .l
.x:
        mov byte [v_corte], 0
        mov byte [v_idioma], 0
        pop si
        ret

; ---- TRON / TROFF ----
sent_tron:
        mov byte [v_tron], 1
        ret
sent_troff:
        mov byte [v_tron], 0
        ret

; ---- ERROR n ----
sent_error:
        call eval_ent
        test ax, ax
        jle .mal
        cmp ax, 255
        ja .mal
        jmp error
.mal:
        jmp err_funcion

; ---- WIDTH n ----
sent_width:
        call eval_ent
        cmp ax, 40
        je .bien
        cmp ax, 80
        jne err_funcion
.bien:
        mov [v_ancho], ax
        ret

; ---- POKE dir, valor ----
sent_poke:
        call evalua
        mov di, ac
        call a_palabra
        push ax
        ESPERA ','
        call eval_ent
        pop bx
        push es
        mov es, [v_defseg]
        mov [es:bx], al
        pop es
        ret
; ---- OUT puerto, valor ----
sent_out:
        call evalua
        mov di, ac
        call a_palabra
        push ax
        ESPERA ','
        call eval_ent
        pop dx
        out dx, al
        ret

; el de DI (0 a 65535) a palabra sin signo: AX
a_palabra:
        cmp byte [di + AC_TIPO], T_ENT
        jne .flot
        mov ax, [di + AC_M0]
        ret
.flot:
        call a_flot
        call f_es_cero
        jz .cero
        test byte [di + AC_SIG], 0x80
        jnz .mal
        mov cx, [di + AC_EXP]
        cmp cx, 16
        jg .mal
        cmp cx, 0
        jle .cero
        mov ax, [di + AC_M3]
        neg cx
        add cx, 16
        shr ax, cl
        ret
.cero:
        xor ax, ax
        ret
.mal:
        jmp err_desb

; ---- CLS ----
sent_cls:
        call es_pantalla
        jnc .x
        mov ax, 0x0600
        mov bh, 0x07
        xor cx, cx
        mov dx, 0x184F
        int 0x10
        mov ah, 2
        xor bh, bh
        xor dx, dx
        int 0x10
.x:
        mov word [v_col], 0
        ret
; ¿va la salida a la pantalla? CF=1 si sí
es_pantalla:
        push bx
        push dx
        mov ax, 0x4400
        mov bx, 1
        int 0x21
        jc .no
        test dl, 0x80
        jz .no
        test dl, 0x02
        jz .no
        stc
        jmp .x
.no:
        clc
.x:
        pop dx
        pop bx
        ret

; ---- LOCATE fila, columna ----
sent_locate:
        call salta_esp
        mov dh, 0xFF
        cmp al, ','
        je .col
        push dx
        call eval_ent
        pop dx
        dec ax
        mov dh, al
.col:
        call salta_esp
        cmp al, ','
        jne .pon
        inc si
        call salta_esp
        cmp al, ','
        je .pon
        call fin_de_sent
        je .pon
        push dx
        call eval_ent
        pop dx
        dec ax
        mov [v_col], ax
        mov dl, al
        jmp .pon2
.pon:
        mov dl, [v_col]
.pon2:
        cmp dh, 0xFF
        jne .pon3
        push dx
        mov ah, 3
        xor bh, bh
        int 0x10
        mov al, dh
        pop dx
        mov dh, al
.pon3:
        mov ah, 2
        xor bh, bh
        int 0x10
        jmp sent_rem_sent

; ---- COLOR, SCREEN, KEY: se aceptan (la pantalla llega en la 0.13) ----
sent_color:
        jmp sent_rem_sent
sent_screen:
        call salta_esp
        call fin_de_sent
        je .x
        call eval_ent
        test ax, ax
        jnz err_funcion
        jmp sent_rem_sent
.x:
        ret
sent_key:
        jmp sent_rem_sent
sent_beep:
        mov al, 7
        jmp pon_car

; ---- MID$(a$, n [, m]) = b$ ----
sent_mid:
        ESPERA '('
        mov al, 1
        call busca_var
        cmp al, T_CAD
        jne err_tipos
        push bx
        ESPERA ','
        call eval_ent
        dec ax
        js .mal
        cmp ax, 255
        jae .mal
        push ax
        mov cx, 255
        call salta_esp
        cmp al, ','
        jne .sin_m
        inc si
        call eval_ent
        test ax, ax
        js .mal
        mov cx, ax
.sin_m:
        push cx
        ESPERA ')'
        ESPERA 0xE7
        call eval_cad               ; BX = b$
        pop cx                      ; m
        pop dx                      ; n-1
        pop di                      ; a$
        ; cuántos: min(m, largo b$, largo a$ - n + 1)
        mov al, [bx]
        xor ah, ah
        cmp cx, ax
        jbe .c1
        mov cx, ax
.c1:
        mov al, [di]
        xor ah, ah
        sub ax, dx
        jle .x
        cmp cx, ax
        jbe .c2
        mov cx, ax
.c2:
        jcxz .x
        ; a$ tiene que estar en el espacio de cadenas (si no, se copia)
        mov ax, [di + 1]
        cmp ax, [v_cads]
        jae .copia
        push bx
        push cx
        push dx
        push di
        mov [ac + AC_M0], di
        mov byte [ac + AC_TIPO], T_CAD
        mov bx, v_desc_aux
        mov al, [di]
        mov [bx], al
        mov ax, [di + 1]
        mov [bx + 1], ax
        mov [ac + AC_M0], bx
        pop bx
        push bx
        call asigna_cadena          ; (copia v_desc_aux a la variable)
        pop di
        pop dx
        pop cx
        pop bx
.copia:
        push si
        mov si, [bx + 1]
        mov di, [di + 1]
        add di, dx
        rep movsb
        pop si
.x:
        ret
.mal:
        jmp err_funcion

; ---- KILL "archivo" ----
sent_kill:
        call nombre_archivo         ; b_nom (ASCIZ)
        mov dx, b_nom
        mov ah, 0x41
        int 0x21
        jc .no
        ret
.no:
        mov al, 53
        jmp error

; ---- FILES: lo deja para la 0.12 ----
sent_files:
        jmp sent_rem_sent

m_repita    db "?Repita desde el principio", 10, 0
m_semilla   db "Semilla del azar (-32768 a 32767)? ", 0
