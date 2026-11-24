; =============================================================================
;  CUENTAS - las fórmulas: leerlas, calcularlas, moverlas y el recálculo
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Se calculan leyendo el texto (como se teclearon), con la precedencia de
;  siempre: comparaciones, luego + y -, luego * y /, luego ^, el signo y el
;  tanto por ciento (21%). Dos maneras de escribirlas:
;    - la de hoy: =SUMA(A1:A10;B3)*1,21   (coma decimal, «;» entre argumentos)
;    - la de VisiCalc y 1-2-3: +A1*2, @SUM(A1...A10), @SUM(A1..A10,B3)
;      (punto decimal, «,» entre argumentos)
;  Las funciones, en español y en inglés: SUMA SUM, MEDIA PROMEDIO AVERAGE AVG,
;  MAX, MIN, CONTAR COUNT, SI IF, REDONDEAR ROUND, ABS, ENTERO INT, HOY TODAY.
;
;  El recálculo: cada fórmula sabe la caja de celdas que lee. Al cambiar algo
;  se marcan pendientes las fórmulas cuya caja lo toca, y las que tocan a
;  esas, y solo esas se calculan, por pasadas: una fórmula que lee otra aún
;  pendiente espera a la pasada siguiente. Si en una pasada no se puede
;  calcular ninguna, las que quedan están en un círculo: #CIRC y un aviso, en
;  lugar de dar vueltas para siempre.
; =============================================================================

; ---- AL, ¿una letra (A-Z)? CF si no ----
es_letra:
        cmp al, 'A'
        jb .no
        cmp al, 'Z'
        ja .no
        clc
        ret
.no:
        stc
        ret
; ---- AL, ¿letra, cifra o «(»? (lo que no puede seguir a una referencia) ----
pega_ref:
        call es_letra
        jnc .si
        cmp al, '('
        je .si
        sub al, '0'
        cmp al, 10
        ret
.si:
        stc                         ; (CF = 1: sí pega)
        ret

; ---- lee una referencia ($A$1, AB12...) de SI: DX = fila, BL = columna,
; AH = bit 0 columna fija, bit 1 fila fija. CF si no la hay (SI no se mueve)
lee_ref:
        push si
        mov byte [lr_fl], 0
        cmp byte [si], '$'
        jne .l1
        inc si
        mov byte [lr_fl], 1
.l1:
        mov al, [si]
        call es_letra
        jc .mal
        inc si
        sub al, 'A'
        mov bl, al
        mov al, [si]
        call es_letra
        jc .dig
        inc si
        sub al, 'A'
        mov cl, al
        mov al, bl
        inc al
        mov ch, 26
        mul ch
        add al, cl
        adc ah, 0
        cmp ax, 255
        ja .mal
        mov bl, al
        mov al, [si]
        call es_letra
        jnc .mal
.dig:
        cmp byte [si], '$'
        jne .d0
        inc si
        or byte [lr_fl], 2
.d0:
        xor dx, dx
        xor cx, cx
.d1:
        mov al, [si]
        sub al, '0'
        cmp al, 9
        ja .d2
        inc si
        inc cx
        cmp cx, 4
        ja .mal
        push ax
        mov ax, 10
        mul dx
        mov dx, ax
        pop ax
        cbw
        add dx, ax
        jmp .d1
.d2:
        jcxz .mal
        test dx, dx
        jz .mal
        dec dx
        mov al, [si]
        call pega_ref
        jc .mal
        pop ax                      ; (el SI de antes)
        mov ah, [lr_fl]
        clc
        ret
.mal:
        pop si
        stc
        ret

; ---- escribe en DI la referencia DX, BL (AH: los «$») ----
pon_ref:
        test ah, 1
        jz .c
        mov byte [di], '$'
        inc di
.c:
        call pon_col
        test ah, 2
        jz .f
        mov byte [di], '$'
        inc di
.f:
        mov ax, dx
        inc ax
        jmp pon_dec

; ---- escribe en DI la letra de la columna BL ----
pon_col:
        push ax
        mov al, bl
        xor ah, ah
        mov cl, 26
        div cl
        test al, al
        jz .una
        add al, 'A' - 1
        stosb
.una:
        mov al, ah
        add al, 'A'
        stosb
        pop ax
        ret

; ---- escribe AX en decimal en DI ----
pon_dec:
        mov cx, 10
        xor bx, bx
.d:
        xor dx, dx
        div cx
        push dx
        inc bx
        test ax, ax
        jnz .d
.e:
        pop ax
        add al, '0'
        stosb
        dec bx
        jnz .e
        ret

; ---- salta los blancos de SI; AL = lo que hay ----
blancos:
        mov al, [si]
        cmp al, ' '
        jne .r
        inc si
        jmp blancos
.r:
        ret

; ---- la pila de valores: empuja acc / saca opb ----
empuja:
        pop bx
        push word [acc+6]
        push word [acc+4]
        push word [acc+2]
        push word [acc]
        jmp bx
saca_opb:
        pop bx
        pop word [opb]
        pop word [opb+2]
        pop word [opb+4]
        pop word [opb+6]
        jmp bx

; =============================================================================
;  ev_formula: calcula la fórmula de `ftxt`. En acc el resultado. CF si lee
;  una celda pendiente (habrá que volver). ev_modo = 1: solo recoge en `caja`
;  lo que lee, sin calcular celdas; ev_mal = 1 si está mal escrita (ev_pos:
;  dónde).
; =============================================================================
ev_formula:
        mov [ev_sp], sp
        mov byte [ev_mal], 0
        mov si, ftxt
        mov byte [nmodo], 2
        call blancos
        cmp al, '='
        jne .c
        inc si
        mov byte [nmodo], 1
.c:
        call expr
        call blancos
        test al, al
        jnz sintaxis
        clc
        ret
sintaxis:
        mov sp, [ev_sp]
        mov [ev_pos], si
        mov byte [ev_mal], 1
        mov al, E_ERROR
        call pon_err
        clc
        ret
falta:
        mov sp, [ev_sp]
        stc
        ret

expr:
        call e_suma
        call blancos
        mov ah, 1
        cmp al, '='
        je .uno
        mov ah, 3
        cmp al, '<'
        je .menor
        mov ah, 4
        cmp al, '>'
        jne .r
        inc si
        cmp byte [si], '='
        jne .op
        mov ah, 6
        jmp short .uno
.menor:
        inc si
        mov al, [si]
        mov ah, 2
        cmp al, '>'
        je .uno
        mov ah, 5
        cmp al, '='
        je .uno
        mov ah, 3
        jmp short .op
.uno:
        inc si
.op:
        push ax
        call empuja
        call e_suma
        call saca_opb
        pop ax
        push si
        call compara
        pop si
.r:
        ret

e_suma:
        call e_prod
.l:
        call blancos
        cmp al, '+'
        je .s
        cmp al, '-'
        jne .r
.s:
        inc si
        push ax
        call empuja
        call e_prod
        call saca_opb
        pop ax
        push si
        cmp al, '+'
        je .mas
        call resta
        jmp short .sig
.mas:
        call suma
.sig:
        pop si
        jmp .l
.r:
        ret

e_prod:
        call e_una
.l:
        call blancos
        cmp al, '*'
        je .s
        cmp al, '/'
        jne .r
.s:
        inc si
        push ax
        call empuja
        call e_una
        call saca_opb
        pop ax
        push si
        cmp al, '*'
        je .por
        call div64
        jmp short .sig
.por:
        call mul64
.sig:
        pop si
        jmp .l
.r:
        ret

e_una:
        call blancos
        cmp al, '+'
        je .mas
        cmp al, '-'
        jne e_pot
        inc si
        call e_una
        cmp word [acc+6], 0x8000
        je .r
        push si
        mov si, acc
        call neg64
        pop si
.r:
        ret
.mas:
        inc si
        jmp e_una

e_pot:
        call e_post
        call blancos
        cmp al, '^'
        jne .r
        inc si
        call empuja
        call e_una
        call saca_opb
        push si
        call potencia
        pop si
.r:
        ret

e_post:
        call e_prim
.l:
        call blancos
        cmp al, '%'
        jne .r
        inc si
        push si
        mov bx, 100
        call divr
        pop si
        jmp .l
.r:
        ret

e_prim:
        call blancos
        cmp al, '('
        jne .n1
        inc si
        call expr
        call blancos
        cmp al, ')'
        jne .mal
        inc si
        ret
.n1:
        cmp al, '#'
        jne .n2
        cmp word [si+1], 'RE'
        jne .mal
        cmp byte [si+3], 'F'
        jne .mal
        add si, 4
        cmp byte [si], '!'
        jne .ref_e
        inc si
.ref_e:
        mov al, E_REF
        jmp pon_err
.n2:
        mov ah, al
        sub ah, '0'
        cmp ah, 9
        jbe .num
        cmp al, '.'
        je .num
        cmp al, ','
        jne .n3
.num:
        call lee_num
        jc .mal
        ret
.n3:
        cmp al, '@'
        jne .n4
        inc si
        jmp short .func
.n4:
        call lee_ref
        jc .func
        call blancos
        cmp al, ':'
        je .mal
        cmp al, '.'
        je .mal
        jmp valor_ref
.func:
        mov di, nombre_f            ; el nombre, hasta 10 letras
        xor cx, cx
.f1:
        mov al, [si]
        call es_letra
        jc .f2
        inc si
        inc cx
        cmp cx, 10
        ja .mal
        mov [di], al
        inc di
        jmp .f1
.f2:
        jcxz .mal
        call blancos
        cmp al, '('
        jne .mal
        inc si
        push si
        push ds
        pop es
        mov si, funciones
.b:
        lodsb
        test al, al
        jz .nohay
        cmp al, cl
        jne .otra
        push si
        push cx
        mov di, nombre_f
        repe cmpsb
        pop cx
        pop si
        je .esta
.otra:
        cbw
        add si, ax
        inc si
        jmp .b
.esta:
        add si, cx
        mov al, [si]
        pop si
        cmp al, 5
        jb f_agrega
        sub al, 5
        cbw
        mov bx, ax
        shl bx, 1
        jmp [bx + tabla_f]
.nohay:
        pop si
.mal:
        jmp sintaxis

funciones:
        db 4, "SUMA", 0
        db 3, "SUM", 0
        db 5, "MEDIA", 1
        db 8, "PROMEDIO", 1
        db 7, "AVERAGE", 1
        db 3, "AVG", 1
        db 3, "MAX", 2
        db 3, "MIN", 3
        db 6, "CONTAR", 4
        db 5, "COUNT", 4
        db 2, "SI", 5
        db 2, "IF", 5
        db 9, "REDONDEAR", 6
        db 5, "ROUND", 6
        db 3, "ABS", 7
        db 6, "ENTERO", 8
        db 3, "INT", 8
        db 3, "HOY", 9
        db 5, "TODAY", 9
        db 0
tabla_f     dw f_si, f_red, f_abs, f_ent, f_hoy

; ---- tras un argumento: «;» o «,» (CF = 0, hay otro) o «)» (CF = 1) ----
arg_sep:
        call blancos
        inc si
        cmp al, ';'
        je .otro
        cmp al, ','
        je .otro
        cmp al, ')'
        jne .mal
        stc
        ret
.otro:
        clc
        ret
.mal:
        jmp sintaxis

; ---- SUMA, MEDIA, MAX, MIN, CONTAR (AL = cuál, de 0 a 4) ----
; el marco: [bp-8] la suma, [bp-16] el mayor o el menor, [bp-18] cuántos,
; [bp-20] el primer error, [bp-22] la función
f_agrega:
        push bp
        mov bp, sp
        sub sp, 22
        mov di, sp
        mov cx, 11
.z:
        mov word [di], 0
        inc di
        inc di
        loop .z
        mov [bp-22], al
        call blancos
        cmp al, ')'
        jne .arg
        inc si
        jmp short .fin
.arg:
        push si
        call lee_ref                ; ¿un rango?
        jc .expr
        push dx
        push bx
        call blancos
        cmp al, ':'
        je .dos
        cmp al, '.'
        jne .no_rango
.p:
        inc si                      ; «..» o «...»
        cmp byte [si], '.'
        je .p
        jmp short .r2
.dos:
        inc si
.r2:
        call blancos
        call lee_ref
        jc .mal
        mov cx, dx
        mov bh, bl
        pop ax
        mov bl, al
        pop dx
        pop ax                      ; (el SI de antes)
        call recorre_rango
        jmp short .sep
.no_rango:
        pop bx
        pop dx
.expr:
        pop si
        call expr
        call agrega
.sep:
        call arg_sep
        jnc .arg
.fin:
        push si
        mov al, [bp-22]
        cmp al, 4
        je .contar
        cmp word [bp-20], 0
        je .bien
        mov ax, [bp-20]
        call pon_err
        jmp short .x
.bien:
        lea si, [bp-8]
        cmp al, 1
        jb .copia
        je .media
        cmp word [bp-18], 0         ; MAX y MIN de nada: 0
        je .cero
        lea si, [bp-16]
.copia:
        mov di, acc
        call copia8
        jmp short .x
.cero:
        call acc_cero
        jmp short .x
.media:
        mov di, opb
        call copia8
        mov ax, [bp-18]
        test ax, ax
        jnz .m1
        mov al, E_DIV0
        call pon_err
        jmp short .x
.m1:
        push si
        call pon_ent
        call div64
        pop si
        jmp short .x
.contar:
        mov ax, [bp-18]
        push si
        call pon_ent
        pop si
.x:
        pop si
        mov sp, bp
        pop bp
        ret
.mal:
        jmp sintaxis

; ---- suma acc a lo que lleva la función del marco de BP ----
agrega:
        cmp word [acc+6], 0x8000
        jne .ok
        cmp byte [bp-22], 4         ; (CONTAR no cuenta los errores)
        je .r
        cmp word [bp-20], 0
        jne .r
        mov ax, [acc]
        mov [bp-20], ax
.r:
        ret
.ok:
        inc word [bp-18]
        mov ax, [acc]
        add [bp-8], ax
        mov ax, [acc+2]
        adc [bp-6], ax
        mov ax, [acc+4]
        adc [bp-4], ax
        mov ax, [acc+6]
        adc [bp-2], ax
        jno .m
        cmp word [bp-20], 0
        jne .m
        mov word [bp-20], E_DESB
.m:
        push si
        lea di, [bp-16]
        cmp word [bp-18], 1
        je .pon
        mov si, acc
        call cmp64                  ; AL = acc frente al que hay
        mov ah, [bp-22]
        cmp ah, 2
        jne .min
        cmp al, 1
        je .pon
        jmp short .x
.min:
        cmp al, -1
        jne .x
.pon:
        mov si, acc
        call copia8
.x:
        pop si
        ret

; ---- SI(condición; si sí; si no) ----
f_si:
        call expr
        call empuja
        call arg_sep
        jc .mal
        call expr
        call empuja
        call arg_sep
        jc .sinb
        call expr
        call arg_sep
        jnc .mal
        jmp short .elige
.sinb:
        call acc_cero
.elige:
        call saca_opb
        pop ax
        pop bx
        pop cx
        pop dx
        cmp dx, 0x8000
        je .err
        or ax, bx
        or ax, cx
        or ax, dx
        jz .r
        push si
        mov si, opb
        mov di, acc
        call copia8
        pop si
.r:
        ret
.err:
        mov [acc], ax
        mov [acc+2], bx
        mov [acc+4], cx
        mov [acc+6], dx
        ret
.mal:
        jmp sintaxis

; ---- REDONDEAR(número; decimales) ----
f_red:
        call expr
        call empuja
        call arg_sep
        jc .mal
        call expr
        call arg_sep
        jnc .mal
        call saca_opb
        push si
        call err_acc_entero         ; AX = los decimales
        jc .e
        push ax
        mov si, opb
        mov di, acc
        call copia8
        pop ax
        call redondea
.e:
        pop si
        ret
.mal:
        jmp sintaxis
; AX = acc como entero (truncado); si acc es un error, CF
err_acc_entero:
        cmp word [acc+6], 0x8000
        je .e
        push word [acc+6]
        call absoluto
        mov si, acc
        mov cx, 4
        mov bx, 10000
        call divn
        call divn
        mov ax, [acc]
        pop dx
        test dh, 0x80
        jz .p
        neg ax
.p:
        clc
        ret
.e:
        stc
        ret

; ---- ABS(x), ENTERO(x) ----
f_abs:
        call expr
        call arg_sep
        jnc f_red.mal
        push si
        call absoluto
        pop si
        ret
f_ent:
        call expr
        call arg_sep
        jnc f_red.mal
        push si
        call entero
        pop si
        ret
; ---- HOY() ----
f_hoy:
        call arg_sep
        jnc f_red.mal
        push si
        mov ah, 0x2A
        int 0x21
        mov ax, cx
        mov bx, dx
        xchg bl, bh
        call fecha_serie
        pop si
        ret

; ---- el valor de la celda DX, BL ----
valor_ref:
        push si
        cmp byte [ev_modo], 1
        jne .n
        mov cx, dx
        mov bh, bl
        call caja_mas
        jmp short .cero
.n:
        call busca
        jc .cero
        mov al, [es:di+R_TIPO]
        test al, 3
        jz .cero
        test al, PENDIENTE
        jnz .falta
        call valor_a_acc
        pop si
        ret
.cero:
        call acc_cero
        pop si
        ret
.falta:
        jmp falta

; ---- acc = el valor del registro ES:DI ----
valor_a_acc:
        push ds
        push es
        lea si, [di+R_VAL]
        mov di, acc
        push ds
        push es
        pop ds
        pop es
        movsw
        movsw
        movsw
        movsw
        pop es
        pop ds
        ret

; ---- la caja crece hasta las filas DX..CX y columnas BL..BH ----
caja_mas:
        cmp dx, cx                  ; (en orden)
        jbe .a
        xchg dx, cx
.a:
        cmp bl, bh
        jbe .b
        xchg bl, bh
.b:
        cmp word [caja], 0xFFFF
        jne .c
        mov [caja], dx
        mov [caja+2], cx
        mov [caja+4], bx
        ret
.c:
        cmp dx, [caja]
        jae .d
        mov [caja], dx
.d:
        cmp cx, [caja+2]
        jbe .e
        mov [caja+2], cx
.e:
        cmp bl, [caja+4]
        jae .f
        mov [caja+4], bl
.f:
        cmp bh, [caja+5]
        jbe .g
        mov [caja+5], bh
.g:
        ret

; ---- recorre el rango DX,BL - CX,BH y agrega sus números ----
recorre_rango:
        cmp byte [ev_modo], 1
        je caja_mas
        cmp dx, cx
        jbe .a
        xchg dx, cx
.a:
        cmp bl, bh
        jbe .b
        xchg bl, bh
.b:
        push si
        mov [rr_f2], cx
        mov [rr_c1], bl
        mov [rr_c2], bh
.busca:
        call busca
.anda:
        mov ax, [n_cel]
        shl ax, 1
        cmp si, ax
        jae .fin
        call del_indice
        mov dx, [es:di]
        cmp dx, [rr_f2]
        ja .fin
        mov bl, [rr_c1]
        mov al, [es:di+2]
        cmp al, bl
        jb .busca                   ; (a la primera columna de esta fila)
        cmp al, [rr_c2]
        jbe .esta
        inc dx                      ; pasada la última: a la fila siguiente
        jmp .busca
.esta:
        mov al, [es:di+R_TIPO]
        test al, 3
        jz .sig
        test al, PENDIENTE
        jnz .falta
        push si
        call valor_a_acc
        call agrega
        pop si
.sig:
        inc si
        inc si
        jmp .anda
.fin:
        pop si
        ret
.falta:
        jmp falta

; =============================================================================
;  Mover las referencias (al copiar y al ordenar): copia la fórmula de SI a
;  DI cambiando cada referencia sin «$» en dsp_f filas y dsp_c columnas. La
;  que se sale de la hoja queda en #REF. CF si no cabe (más de 240 letras).
; =============================================================================
desplaza:
        push ds
        pop es
        mov [dsp_ini], di
        xor cl, cl                  ; CL = 1: lo de antes era letra o cifra
.l:
        mov ax, di
        sub ax, [dsp_ini]
        cmp ax, 240
        ja .larga
        mov al, [si]
        test al, al
        jz .fin
        test cl, cl
        jnz .copia
        push cx
        call lee_ref
        pop cx
        jc .copia
        test byte [lr_fl], 1        ; la columna
        jnz .f
        mov al, bl
        xor ah, ah
        add ax, [dsp_c]
        cmp ax, 255
        ja .ref
        mov bl, al
.f:
        test byte [lr_fl], 2        ; la fila
        jnz .pon
        add dx, [dsp_f]
        cmp dx, NFIL
        jae .ref
.pon:
        mov ah, [lr_fl]
        call pon_ref
        jmp .l
.ref:
        mov ax, '#R'
        stosw
        mov ax, 'EF'
        stosw
        jmp .l
.copia:
        lodsb
        stosb
        mov cl, 1
        call es_letra
        jnc .l
        cmp al, '0'
        jb .no
        cmp al, '9'
        jbe .l
.no:
        xor cl, cl
        jmp .l
.fin:
        mov byte [di], 0
        clc
        ret
.larga:
        stc
        ret

; ---- copia el texto de la fórmula ES:DI a ftxt (acabado en 0) ----
form_ftxt:
        push ds
        push es
        mov cl, [es:di+R_LEN]
        xor ch, ch
        lea si, [di+20]
        mov di, ftxt
        push ds
        push es
        pop ds
        pop es
        rep movsb
        mov byte [es:di], 0
        pop es
        pop ds
        ret

; ---- la caja de la fórmula de nr (y si está bien escrita: CF si no) ----
caja_nr:
        mov cl, [nr+R_LEN]
        xor ch, ch
        mov si, nr+20
        mov di, ftxt
        push ds
        pop es
        rep movsb
        mov byte [di], 0
        mov word [caja], 0xFFFF
        mov byte [ev_modo], 1
        call ev_formula
        mov byte [ev_modo], 0
        mov ax, [caja]
        mov [nr+R_CAJA], ax
        mov ax, [caja+2]
        mov [nr+R_CAJA+2], ax
        mov ax, [caja+4]
        mov [nr+R_CAJA+4], ax
        cmp byte [ev_mal], 1
        cmc
        ret

; =============================================================================
;  El recálculo
; =============================================================================
; ---- todo: todas las fórmulas, pendientes ----
recalcula_todo:
        xor si, si
.l:
        mov ax, [n_cel]
        shl ax, 1
        cmp si, ax
        jae evalua_pend
        call del_indice
        mov al, [es:di+R_TIPO]
        and al, 3
        cmp al, T_FORM
        jne .s
        or byte [es:di+R_TIPO], PENDIENTE
.s:
        inc si
        inc si
        jmp .l

; ---- lo que depende de lo cambiado (la caja d_f1, d_f2, d_c1, d_c2) ----
recalcula:
.vuelta:
        mov byte [r_otra], 0
        xor si, si
.l:
        mov ax, [n_cel]
        shl ax, 1
        cmp si, ax
        jae .fin
        call del_indice
        mov al, [es:di+R_TIPO]
        cmp al, T_FORM              ; (fórmula y no pendiente)
        jne .s
        mov ax, [es:di+R_CAJA]      ; ¿se tocan las cajas?
        cmp ax, 0xFFFF
        je .s
        cmp ax, [d_f2]
        ja .s
        mov ax, [es:di+R_CAJA+2]
        cmp ax, [d_f1]
        jb .s
        mov al, [es:di+R_CAJA+4]
        cmp al, [d_c2]
        ja .s
        mov al, [es:di+R_CAJA+5]
        cmp al, [d_c1]
        jb .s
        or byte [es:di+R_TIPO], PENDIENTE
        mov byte [r_otra], 1
        mov dx, [es:di]             ; y lo cambiado crece con ella
        mov bl, [es:di+2]
        cmp dx, [d_f1]
        jae .a
        mov [d_f1], dx
.a:
        cmp dx, [d_f2]
        jbe .b
        mov [d_f2], dx
.b:
        cmp bl, [d_c1]
        jae .c
        mov [d_c1], bl
.c:
        cmp bl, [d_c2]
        jbe .s
        mov [d_c2], bl
.s:
        inc si
        inc si
        jmp .l
.fin:
        cmp byte [r_otra], 0
        jne .vuelta
        ; (sigue en evalua_pend)

; ---- calcula las pendientes, por pasadas ----
evalua_pend:
        mov word [circ_f], 0xFFFF
.pasada:
        mov byte [r_avanza], 0
        mov byte [r_quedan], 0
        xor si, si
.l:
        mov ax, [n_cel]
        shl ax, 1
        cmp si, ax
        jae .fin
        push si
        call del_indice
        mov al, [es:di+R_TIPO]
        cmp al, T_FORM + PENDIENTE
        jne .s
        mov ax, [es:di]
        mov [r_f], ax
        mov al, [es:di+2]
        mov [r_c], al
        call form_ftxt
        mov byte [ev_modo], 0
        call ev_formula
        pop si
        push si
        jc .espera
        call del_indice
        mov byte [es:di+R_TIPO], T_FORM
        add di, R_VAL
        mov si, acc
        movsw
        movsw
        movsw
        movsw
        mov byte [r_avanza], 1
        jmp short .s
.espera:
        mov byte [r_quedan], 1
.s:
        pop si
        inc si
        inc si
        jmp .l
.fin:
        cmp byte [r_quedan], 0
        je .r
        cmp byte [r_avanza], 0
        jne .pasada
        ; en un círculo: las que quedan, #CIRC
        xor si, si
.c:
        mov ax, [n_cel]
        shl ax, 1
        cmp si, ax
        jae .r
        call del_indice
        cmp byte [es:di+R_TIPO], T_FORM + PENDIENTE
        jne .c1
        cmp word [circ_f], 0xFFFF
        jne .c0
        mov ax, [es:di]
        mov [circ_f], ax
        mov al, [es:di+2]
        mov [circ_c], al
.c0:
        mov byte [es:di+R_TIPO], T_FORM
        mov word [es:di+R_VAL], E_CIRC
        xor ax, ax
        mov [es:di+R_VAL+2], ax
        mov [es:di+R_VAL+4], ax
        mov word [es:di+R_VAL+6], 0x8000
.c1:
        inc si
        inc si
        jmp .c
.r:
        ret
