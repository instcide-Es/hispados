; =============================================================================
;  Hispa-BASIC - las expresiones y las variables
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  evalua: la expresión de DS:SI al acumulador (ac), con su tipo. La
;  precedencia es la de GW-BASIC, de más fuerte a más débil:
;     ^   - (signo)   * /   \   MOD   + -   = <> < > <= >=   NOT   AND   OR
;     XOR   EQV   IMP
;  Un operando se guarda en la pila de la máquina mientras se calcula el otro.
;  Una cadena es, en el acumulador, la dirección de su descriptor
;  (longitud, dirección); si es «de paso» (v_temps) se suelta al usarla.
; =============================================================================

; ---- saltar espacios: AL = lo siguiente (sin pasarlo) ----
salta_esp:
        mov al, [si]
        cmp al, ' '
        jne .x
        inc si
        jmp salta_esp
.x:
        ret
; ---- lo siguiente tiene que ser AH; si no, error de sintaxis ----
espera:
        call salta_esp
        cmp al, ah
        jne err_sintaxis
        inc si
        ret
%macro ESPERA 1
        mov ah, %1
        call espera
%endmacro
; ---- ¿es lo siguiente AH? si lo es, lo pasa y ZF=1 ----
mira:
        call salta_esp
        cmp al, ah
        jne .x
        inc si
.x:
        ret

; ---- guardar el acumulador en la pila y sacarlo luego en arg ----
empuja_ac:
        pop bp
        push word [ac + 10]
        push word [ac + 8]
        push word [ac + 6]
        push word [ac + 4]
        push word [ac + 2]
        push word [ac]
        jmp bp
saca_arg:
        pop bp
        pop word [arg]
        pop word [arg + 2]
        pop word [arg + 4]
        pop word [arg + 6]
        pop word [arg + 8]
        pop word [arg + 10]
        jmp bp
saca_ac:
        pop bp
        pop word [ac]
        pop word [ac + 2]
        pop word [ac + 4]
        pop word [ac + 6]
        pop word [ac + 8]
        pop word [ac + 10]
        jmp bp

; ---- las tres formas de pedir una expresión ----
; una numérica
eval_num:
        call evalua
        cmp byte [ac + AC_TIPO], T_CAD
        je err_tipos
        ret
; una numérica, como entero (redondeando): AX
eval_ent:
        call eval_num
        mov di, ac
        jmp a_entero
; una de cadenas: BX = el descriptor (se suelta si es de paso, pero lo que
; señala sigue valiendo hasta que se pida otra cadena)
eval_cad:
        call evalua
        cmp byte [ac + AC_TIPO], T_CAD
        jne err_tipos
        mov bx, [ac + AC_M0]
        jmp libera_temp

; el de DI, a entero: AX (error de desbordamiento si no cabe)
a_entero:
        cmp byte [di + AC_TIPO], T_ENT
        jne .flot
        mov ax, [di + AC_M0]
        ret
.flot:
        cmp byte [di + AC_TIPO], T_CAD
        je err_tipos
        call f_a_entero
        jc err_desb
        ret
; el de DI, a número con decimales (si era entero, a simple)
a_flot:
        cmp byte [di + AC_TIPO], T_ENT
        jne .x
        mov ax, [di + AC_M0]
        call f_de_entero
.x:
        ret

; =============================================================================
evalua:
        cmp sp, b_pila + 200        ; demasiado anidado
        jb .honda
        call ev_eqv
.l:
        call salta_esp
        cmp al, 0xF2                ; IMP
        jne .x
        inc si
        call empuja_ac
        call ev_eqv
        call saca_arg
        call op_logica
        mov ax, [arg + AC_M0]       ; a IMP b = (NOT a) OR b
        not ax
        or ax, dx
        mov [ac + AC_M0], ax
        jmp .l
.x:
        ret
.honda:
        mov al, 7
        jmp error
ev_eqv:
        call ev_xor
.l:
        call salta_esp
        cmp al, 0xF1
        jne .x
        inc si
        call empuja_ac
        call ev_xor
        call saca_arg
        call op_logica
        mov ax, [arg + AC_M0]
        xor ax, dx
        not ax
        mov [ac + AC_M0], ax
        jmp .l
.x:
        ret
ev_xor:
        call ev_or
.l:
        call salta_esp
        cmp al, 0xF0
        jne .x
        inc si
        call empuja_ac
        call ev_or
        call saca_arg
        call op_logica
        mov ax, [arg + AC_M0]
        xor ax, dx
        mov [ac + AC_M0], ax
        jmp .l
.x:
        ret
ev_or:
        call ev_and
.l:
        call salta_esp
        cmp al, 0xEF
        jne .x
        inc si
        call empuja_ac
        call ev_and
        call saca_arg
        call op_logica
        mov ax, [arg + AC_M0]
        or ax, dx
        mov [ac + AC_M0], ax
        jmp .l
.x:
        ret
ev_and:
        call ev_not
.l:
        call salta_esp
        cmp al, 0xEE
        jne .x
        inc si
        call empuja_ac
        call ev_not
        call saca_arg
        call op_logica
        mov ax, [arg + AC_M0]
        and ax, dx
        mov [ac + AC_M0], ax
        jmp .l
.x:
        ret
; los dos (arg y ac) a enteros: arg en su sitio, ac en DX; ac queda entero
op_logica:
        mov di, arg
        call a_entero
        mov [arg + AC_M0], ax
        mov di, ac
        call a_entero
        mov dx, ax
        mov byte [ac + AC_TIPO], T_ENT
        ret
ev_not:
        call salta_esp
        cmp al, 0xD3
        jne ev_rel
        inc si
        call ev_not
        mov di, ac
        call a_entero
        not ax
        mov [ac + AC_M0], ax
        mov byte [ac + AC_TIPO], T_ENT
        ret

; ---- las comparaciones ----
ev_rel:
        call ev_suma
.l:
        call salta_esp
        xor cx, cx                  ; CL: 1 «<», 2 «=», 4 «>»
.op:
        cmp al, 0xE8
        jne .o1
        or cl, 1
        jmp .sig
.o1:
        cmp al, 0xE7
        jne .o2
        or cl, 2
        jmp .sig
.o2:
        cmp al, 0xE6
        jne .fin_op
        or cl, 4
.sig:
        inc si
        call salta_esp
        jmp .op
.fin_op:
        test cl, cl
        jz .x
        push cx
        call empuja_ac
        call ev_suma
        call saca_arg
        call compara                ; AX = arg frente a ac: -1, 0, 1
        pop cx
        ; ¿se cumple?
        mov dx, 0
        test ax, ax
        js .menor
        jz .igual
        test cl, 4
        jmp .res
.menor:
        test cl, 1
        jmp .res
.igual:
        test cl, 2
.res:
        jz .falso
        mov dx, -1
.falso:
        mov [ac + AC_M0], dx
        mov byte [ac + AC_TIPO], T_ENT
        jmp .l
.x:
        ret

; ---- compara arg con ac (números o cadenas): AX = -1, 0 o 1 ----
compara:
        mov al, [arg + AC_TIPO]
        mov ah, [ac + AC_TIPO]
        cmp al, T_CAD
        je .cad
        cmp ah, T_CAD
        je err_tipos
        cmp ax, (T_ENT << 8) | T_ENT
        jne .flot
        mov ax, [arg + AC_M0]
        cmp ax, [ac + AC_M0]
        je .igual
        jl .menor
        mov ax, 1
        ret
.igual:
        xor ax, ax
        ret
.menor:
        mov ax, -1
        ret
.flot:
        mov di, arg
        call a_flot
        mov di, ac
        call a_flot
        call ajusta_tipos           ; los dos al mismo (simple o doble)
        push si
        mov di, arg
        mov si, ac
        call f_cmp
        pop si
        ret
.cad:
        cmp ah, T_CAD
        jne err_tipos
        push si
        push bx
        mov bx, [ac + AC_M0]
        call libera_temp
        mov bx, [arg + AC_M0]
        call libera_temp
        mov bx, [arg + AC_M0]
        mov di, [ac + AC_M0]
        mov cl, [bx]                ; longitudes
        mov ch, [di]
        mov si, [bx + 1]
        mov di, [di + 1]
.c:
        test cl, cl
        jz .a_fin
        test ch, ch
        jz .mayor_c
        mov al, [si]
        cmp al, [di]
        jb .menor_c
        ja .mayor_c
        inc si
        inc di
        dec cl
        dec ch
        jmp .c
.a_fin:
        test ch, ch
        jnz .menor_c
        xor ax, ax
        jmp .cx
.menor_c:
        mov ax, -1
        jmp .cx
.mayor_c:
        mov ax, 1
.cx:
        pop bx
        pop si
        ret

; los dos (arg y ac, ya con decimales) al mismo tipo: doble si alguno lo es
ajusta_tipos:
        mov al, [arg + AC_TIPO]
        cmp al, [ac + AC_TIPO]
        je .x
        mov byte [arg + AC_TIPO], T_DOB
        mov byte [ac + AC_TIPO], T_DOB
.x:
        ret

; ---- + y - ----
ev_suma:
        call ev_mod
.l:
        call salta_esp
        cmp al, 0xE9
        je .mas
        cmp al, 0xEA
        jne .x
        inc si
        call empuja_ac
        call ev_mod
        call saca_arg
        call op_resta
        jmp .l
.mas:
        inc si
        call empuja_ac
        call ev_mod
        call saca_arg
        call op_suma
        jmp .l
.x:
        ret

ev_mod:
        call ev_idiv
.l:
        call salta_esp
        cmp al, 0xF3
        jne .x
        inc si
        call empuja_ac
        call ev_idiv
        call saca_arg
        call op_ent_div             ; AX = cociente, DX = resto
        mov [ac + AC_M0], dx
        jmp .l
.x:
        ret
ev_idiv:
        call ev_mul
.l:
        call salta_esp
        cmp al, 0xF4
        jne .x
        inc si
        call empuja_ac
        call ev_mul
        call saca_arg
        call op_ent_div
        mov [ac + AC_M0], ax
        jmp .l
.x:
        ret
; arg \ ac y arg MOD ac, con enteros
op_ent_div:
        mov di, arg
        call a_entero
        mov cx, ax
        mov di, ac
        call a_entero
        test ax, ax
        jz .cero
        mov bx, ax
        mov ax, cx
        cmp ax, 0x8000              ; -32768 \ -1 no cabe
        jne .bien
        cmp bx, -1
        je err_desb
.bien:
        cwd
        idiv bx
        mov byte [ac + AC_TIPO], T_ENT
        ret
.cero:
        mov al, 11
        jmp error

ev_mul:
        call ev_signo
.l:
        call salta_esp
        cmp al, 0xEB
        je .por
        cmp al, 0xEC
        jne .x
        inc si
        call empuja_ac
        call ev_signo
        call saca_arg
        call op_div
        jmp .l
.por:
        inc si
        call empuja_ac
        call ev_signo
        call saca_arg
        call op_mul
        jmp .l
.x:
        ret

; ---- el signo de delante (más débil que ^: -2^2 = -4) ----
ev_signo:
        call salta_esp
        cmp al, 0xEA
        je .menos
        cmp al, 0xE9
        jne ev_pot
        inc si
        jmp ev_signo
.menos:
        inc si
        call ev_signo
        jmp op_cambia_signo

ev_pot:
        call ev_atomo
.l:
        call salta_esp
        cmp al, 0xED
        jne .x
        inc si
        call empuja_ac
        ; el exponente puede llevar signo: 2^-1
        call ev_signo_pot
        call saca_arg
        call op_pot
        jmp .l
.x:
        ret
ev_signo_pot:
        call salta_esp
        cmp al, 0xEA
        je .menos
        cmp al, 0xE9
        jne ev_atomo
        inc si
        jmp ev_signo_pot
.menos:
        inc si
        call ev_signo_pot
        jmp op_cambia_signo

; ---- cambiar el signo del acumulador ----
op_cambia_signo:
        mov al, [ac + AC_TIPO]
        cmp al, T_CAD
        je err_tipos
        cmp al, T_ENT
        jne .flot
        mov ax, [ac + AC_M0]
        cmp ax, 0x8000
        je .a_flot                  ; -(-32768) = 32768, en simple
        neg ax
        mov [ac + AC_M0], ax
        ret
.a_flot:
        mov di, ac
        call a_flot
.flot:
        cmp word [ac + AC_M3], 0
        je .x
        xor byte [ac + AC_SIG], 0x80
.x:
        ret

; ---- las cuatro operaciones: ac = arg (op) ac ----
op_suma:
        mov al, [arg + AC_TIPO]
        cmp al, T_CAD
        je concatena
        cmp byte [ac + AC_TIPO], T_CAD
        je err_tipos
        cmp al, T_ENT
        jne .flot
        cmp byte [ac + AC_TIPO], T_ENT
        jne .flot
        mov ax, [arg + AC_M0]
        add ax, [ac + AC_M0]
        jo .flot
        mov [ac + AC_M0], ax
        ret
.flot:
        call prepara_flot
        mov si, ac
        mov di, arg
        call f_suma
        jmp resultado_arg
op_resta:
        call solo_numeros
        cmp byte [arg + AC_TIPO], T_ENT
        jne .flot
        cmp byte [ac + AC_TIPO], T_ENT
        jne .flot
        mov ax, [arg + AC_M0]
        sub ax, [ac + AC_M0]
        jo .flot
        mov [ac + AC_M0], ax
        ret
.flot:
        call prepara_flot
        mov si, ac
        mov di, arg
        call f_resta
        jmp resultado_arg
op_mul:
        call solo_numeros
        cmp byte [arg + AC_TIPO], T_ENT
        jne .flot
        cmp byte [ac + AC_TIPO], T_ENT
        jne .flot
        mov ax, [arg + AC_M0]
        imul word [ac + AC_M0]
        jo .flot
        mov [ac + AC_M0], ax
        ret
.flot:
        call prepara_flot
        mov si, ac
        mov di, arg
        call f_mul
        jmp resultado_arg
op_div:
        call solo_numeros
        call prepara_flot
        cmp word [ac + AC_M3], 0
        jne .bien
        ; entre cero: GW-BASIC avisa «División por cero» y sigue con el mayor
        ; número (con el signo del dividendo)
        pop si
        push si
        call col_cero
        mov si, m_div_cero
        call pon_cad_cs
        call pon_nl
        mov al, [arg + AC_SIG]
        mov [ac + AC_SIG], al
        call desborda_max
        pop si
        ret
.bien:
        mov si, ac
        mov di, arg
        call f_div
        jmp resultado_arg
; ¿son números los dos? (si no, tipos que no casan)
solo_numeros:
        cmp byte [arg + AC_TIPO], T_CAD
        je err_tipos
        cmp byte [ac + AC_TIPO], T_CAD
        je err_tipos
        ret
; los dos a decimales y al mismo tipo; guarda SI (el texto) en la pila
prepara_flot:
        pop bp
        push si
        push bp
        mov di, arg
        call a_flot
        mov di, ac
        call a_flot
        jmp ajusta_tipos
; el resultado está en arg: se redondea a su tipo y pasa a ac
resultado_arg:
        mov di, arg
        call f_redondea
        mov si, arg
        mov di, ac
        call f_copia
        pop si                      ; el texto, guardado en prepara_flot
; ¿se ha salido del margen (más de 1,7E+38)? Como GW-BASIC: avisa y sigue
; con el mayor número, con su signo
mira_desborde:
        cmp word [ac + AC_M3], 0
        je .x
        cmp word [ac + AC_EXP], 127
        jle .x
        push ax
        mov al, [ac + AC_TIPO]
        push ax
        call desborda_aviso
        pop ax
        mov [ac + AC_TIPO], al
        pop ax
.x:
        ret

; ---- ^ ----
op_pot:
        call solo_numeros
        call prepara_flot
        ; ¿el exponente es entero (y no muy grande)? multiplicando
        mov di, ac
        call f_tiene_dec
        jnz .real
        call f_a_entero
        jc .real
        ; AX = n; arg^n
        mov cx, ax
        test cx, cx
        jns .n_pos
        neg cx
.n_pos:
        push ax
        ; ac2 = arg (la base); ac = 1
        mov si, arg
        mov di, ac2
        call f_copia
        mov al, [arg + AC_TIPO]
        mov si, f_uno_cte
        mov di, ac
        call f_carga_cte
        mov [ac + AC_TIPO], al
.b:
        jcxz .b_fin
        shr cx, 1
        jnc .b_no
        mov di, ac
        mov si, ac2
        call f_mul
.b_no:
        jcxz .b_fin
        mov di, ac2
        mov si, ac2
        push si
        mov si, ac2
        mov di, ac3
        call f_copia
        call f_mul_ac2_ac3
        pop si
        jmp .b
.b_fin:
        pop ax
        test ax, ax
        jns .fin
        ; negativo: 1 / eso
        cmp word [ac + AC_M3], 0
        je .div0
        mov si, ac
        mov di, arg
        call f_copia
        mov al, [ac + AC_TIPO]
        mov si, f_uno_cte
        mov di, ac
        call f_carga_cte
        mov [ac + AC_TIPO], al
        call f_ac_a_ac2
        mov si, arg
        mov di, ac2
        call f_div
        mov si, ac2
        mov di, ac
        call f_copia
.fin:
        mov di, ac
        call f_redondea
        pop si
        jmp mira_desborde
.div0:
        mov al, 11
        jmp error
.real:
        ; x^y = EXP(y * LOG(x)); x tiene que ser positivo
        cmp word [arg + AC_M3], 0
        je .base0
        test byte [arg + AC_SIG], 0x80
        jnz err_funcion
        mov al, [ac + AC_TIPO]
        push ax
        mov si, ac
        mov di, ac_pot
        call f_copia                ; ac_pot = y (LOG usa ac2-ac5)
        mov si, arg
        mov di, ac
        call f_copia                ; ac = x
        call fn_log_ac
        mov di, ac
        mov si, ac_pot
        call f_mul
        call fn_exp_ac
        pop ax
        mov [ac + AC_TIPO], al
        jmp .fin
.base0:
        ; 0^y: 0 si y > 0
        test byte [ac + AC_SIG], 0x80
        jnz .div0
        mov di, ac
        mov al, [di + AC_TIPO]
        call f_cero
        mov [di + AC_TIPO], al
        pop si
        ret

; =============================================================================
;  Un operando
; =============================================================================
ev_atomo:
        call salta_esp
        cmp al, '('
        je .parentesis
        cmp al, '"'
        je .literal
        cmp al, 0x20
        jb .constante
        cmp al, 'A'
        jb .no
        cmp al, 'Z'
        jbe .variable
        cmp al, 0xFF
        je .funcion
        cmp al, 0xD1
        je .fn
        cmp al, 0xD3                ; NOT aquí: (NOT a) se toma como operando
        je .not
        cmp al, 0xFE
        je .funcion2
        cmp al, 0xD4
        je .erl
        cmp al, 0xD5
        je .err
        cmp al, 0xDB
        je .csrlin
        cmp al, 0xDE
        je .inkey
        cmp al, 0xD6
        je .string
        cmp al, 0xD8
        je .instr
        cmp al, 0xDA
        je .varptr
        cmp al, 0xDC                ; POINT(x, y)
        jne .no
        inc si
        jmp fn_point
.no:
        test al, al
        jz .falta
        cmp al, ':'
        je .falta
        cmp al, ','
        je .falta
        cmp al, ')'
        je .falta
        jmp err_sintaxis
.falta:
        mov al, 22                  ; Falta un operando
        jmp error
.parentesis:
        inc si
        call evalua
        ESPERA ')'
        ret
.not:
        inc si
        call ev_signo
        mov di, ac
        call a_entero
        not ax
        mov [ac + AC_M0], ax
        mov byte [ac + AC_TIPO], T_ENT
        ret
.literal:
        inc si
        mov dx, si
        xor cx, cx
.l1:
        lodsb
        test al, al
        jz .l_fin0
        cmp al, '"'
        je .l_fin
        inc cx
        jmp .l1
.l_fin0:
        dec si
.l_fin:
        call nuevo_temp             ; CX longitud, DX dirección -> BX
        mov [ac + AC_M0], bx
        mov byte [ac + AC_TIPO], T_CAD
        ret
.constante:
        jmp lee_constante
.variable:
        xor al, al                  ; sin crearla
        call busca_var              ; BX = su valor, AL = su tipo
        jmp carga_valor
.funcion:
        inc si
        lodsb
        jmp funcion_ff
.funcion2:
        inc si
        lodsb
        jmp funcion_fe
.fn:
        inc si
        jmp llama_fn
.erl:
        inc si
        mov ax, [v_ultima]
        cmp ax, 0xFFFF
        jne .e1
        xor ax, ax
.e1:
        jmp ac_natural
.err:
        inc si
        mov ax, [v_err]
        jmp ac_entero
.csrlin:
        inc si
        jmp fn_csrlin
.inkey:
        inc si
        jmp fn_inkey
.string:
        inc si
        jmp fn_string
.instr:
        inc si
        jmp fn_instr
.varptr:                            ; VARPTR(variable): su dirección
        inc si
        ESPERA '('
        call salta_esp
        xor al, al
        call busca_var
        ESPERA ')'
        mov ax, bx
        jmp ac_natural

; ---- AX como entero en el acumulador ----
ac_entero:
        mov [ac + AC_M0], ax
        mov byte [ac + AC_TIPO], T_ENT
        ret
; AX sin signo: entero si cabe, si no simple
ac_natural:
        test ax, ax
        jns ac_entero
        push di
        mov di, ac
        call f_cero
        mov byte [di + AC_TIPO], T_SIM
        mov [di + AC_M3], ax
        mov word [di + AC_EXP], 16
        pop di
        ret

; ---- una constante del programa al acumulador ----
lee_constante:
        lodsb
        cmp al, 0x11
        jb .otras
        cmp al, 0x1B
        ja .otras
        sub al, 0x11
        cbw
        jmp ac_entero
.otras:
        cmp al, 0x0F
        jne .o1
        lodsb
        xor ah, ah
        jmp ac_entero
.o1:
        cmp al, 0x1C
        je .palabra
        cmp al, 0x0B
        je .palabra
        cmp al, 0x0C
        je .palabra
        cmp al, 0x0E
        je .linea
        cmp al, 0x1D
        je .sim
        cmp al, 0x1F
        je .dob
        jmp err_sintaxis
.palabra:
        lodsw
        jmp ac_entero
.linea:
        lodsw
        jmp ac_natural
.sim:
        call mbf4_a_ac
        add si, 4
        ret
.dob:
        call mbf8_a_ac
        add si, 8
        ret

; ---- el valor de BX (de tipo AL) al acumulador ----
carga_valor:
        mov [ac + AC_TIPO], al
        cmp al, T_ENT
        je .ent
        cmp al, T_CAD
        je .cad
        push si
        mov si, bx
        push di
        mov di, ac
        cmp al, T_SIM
        jne .dob
        call mbf4_a
        jmp .f_x
.dob:
        call mbf8_a
.f_x:
        pop di
        pop si
        ret
.ent:
        mov ax, [bx]
        mov [ac + AC_M0], ax
        ret
.cad:
        mov [ac + AC_M0], bx
        ret

; ---- guarda el acumulador en BX (de tipo AL), convirtiendo ----
guarda_valor:
        cmp al, T_CAD
        je .cad
        cmp byte [ac + AC_TIPO], T_CAD
        je err_tipos
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
        mov di, bx
        cmp al, T_SIM
        jne .dob
        call ac_a_mbf4
        pop di
        ret
.dob:
        call ac_a_mbf8
        pop di
        ret
.cad:
        cmp byte [ac + AC_TIPO], T_CAD
        jne err_tipos
        jmp asigna_cadena           ; BX = el descriptor de la variable

; =============================================================================
;  Las variables
;  simples: [tipo][largo][nombre][valor]       el valor ocupa «tipo» octetos
;  matrices: [tipo][largo][nombre][tamaño w][dims b][cada dim w]...[datos]
;  (el tamaño cuenta desde detrás de él hasta el final)
; =============================================================================

; ---- lee un nombre de DS:SI a b_nom (largo y letras): AL = su tipo ----
lee_nombre:
        push di
        push cx
        mov di, b_nom + 1
        xor cx, cx
.c:
        mov al, [si]
        call es_alfanum
        jnc .fin
        inc si
        cmp cx, 40
        jae .c
        stosb
        inc cx
        jmp .c
.fin:
        mov [b_nom], cl
        jcxz .mal
        ; el tipo
        mov ah, T_ENT
        cmp al, '%'
        je .suf
        mov ah, T_SIM
        cmp al, '!'
        je .suf
        mov ah, T_DOB
        cmp al, '#'
        je .suf
        mov ah, T_CAD
        cmp al, '$'
        je .suf
        ; por la primera letra
        mov bl, [b_nom + 1]
        sub bl, 'A'
        xor bh, bh
        mov al, [v_def + bx]
        jmp .x
.suf:
        inc si
        mov al, ah
.x:
        pop cx
        pop di
        ret
.mal:
        jmp err_sintaxis

; ---- busca_var: la variable de DS:SI. AL = 1 para crearla si no existe.
; Sale con BX = la dirección de su valor y AL = su tipo. Sin crear, una que
; no existe vale 0 (o ""): BX señala un valor a cero que no se debe tocar.
busca_var:
        push cx
        push dx
        push di
        mov dl, al                  ; crear
        call salta_esp              ; (tras la palabra clave puede haber espacios)
        call lee_nombre             ; AL = el tipo
        mov dh, al
        mov [v_tipo_var], al
        call salta_esp
        cmp al, '('
        je .matriz
        cmp al, '['
        je .matriz
        ; ---- simple ----
        call busca_simple
        mov al, dh
        pop di
        pop dx
        pop cx
        ret
.matriz:
        inc si
        call elem_matriz            ; BX = el elemento
        mov al, dh
        pop di
        pop dx
        pop cx
        ret

; ---- la variable simple b_nom de tipo DH (DL = 1: crearla): BX = su valor
busca_simple:
        push cx
        push di
        mov bx, [v_vars]
.b:
        cmp bx, [v_mats]
        jae .no_esta
        call nombre_igual           ; ZF=1 si la de BX es esta
        je .esta
        mov al, [bx]                ; el tamaño del valor = el tipo
        xor ah, ah
        add al, [bx + 1]
        adc ah, 0
        add ax, 2
        add bx, ax
        jmp .b
.esta:
        mov al, [bx + 1]
        xor ah, ah
        add bx, ax
        add bx, 2
        jmp .fin
.no_esta:
        test dl, dl
        jnz .crear
        mov bx, v_cero_val
        jmp .fin
.crear:
        ; hace sitio en v_mats (las matrices se mueven)
        mov al, dh
        xor ah, ah
        add al, [b_nom]
        adc ah, 0
        add ax, 2
        mov cx, ax                  ; lo que ocupa
        call hay_sitio
        push si
        mov si, [v_libre]
        dec si
        mov di, si
        add di, cx
        push cx
        mov cx, [v_libre]
        sub cx, [v_mats]
        std
        rep movsb
        cld
        pop cx
        mov bx, [v_mats]
        add [v_mats], cx
        add [v_libre], cx
        ; escribirla
        mov di, bx
        call escribe_nombre
        ; el valor, a cero
        mov bx, di
        mov cl, dh
        xor ch, ch
        xor al, al
        rep stosb
        pop si
.fin:
        pop di
        pop cx
        ret

; ¿la variable de BX (tipo, largo, nombre) es la de DH y b_nom? ZF=1
nombre_igual:
        push si
        push di
        push cx
        cmp [bx], dh
        jne .x
        mov cl, [bx + 1]
        cmp cl, [b_nom]
        jne .x
        xor ch, ch
        lea si, [bx + 2]
        mov di, b_nom + 1
        repe cmpsb
.x:
        pop cx
        pop di
        pop si
        ret
; escribe en ES:DI el tipo (DH), el largo y el nombre de b_nom
escribe_nombre:
        push si
        push cx
        mov al, dh
        stosb
        mov si, b_nom
        mov cl, [si]
        xor ch, ch
        inc cx
        rep movsb
        pop cx
        pop si
        ret
; ¿caben CX octetos más entre v_libre y las cadenas? si no, recoge las
; cadenas; si aún no, «Memoria agotada»
hay_sitio:
        push ax
        mov ax, [v_libre]
        add ax, cx
        jc .no
        add ax, 32
        jc .no
        cmp ax, [v_cads]
        jb .x
        call recoge_cadenas
        mov ax, [v_libre]
        add ax, cx
        add ax, 32
        jc .no
        cmp ax, [v_cads]
        jb .x
.no:
        mov al, 7
        jmp error
.x:
        pop ax
        ret

; ---- un elemento de matriz: el nombre ya está en b_nom, DH = el tipo, SI
; tras el «(». Lee los índices y el «)». BX = el elemento ----
elem_matriz:
        call guarda_nombre          ; (un índice puede llevar otras variables)
        push bp
        mov bp, sp                  ; los índices irán en [bp-2], [bp-4]...
        xor cx, cx
.i:
        push cx
        push bp
        call eval_ent
        pop bp
        pop cx
        test ax, ax
        js .fuera
        push ax
        inc cx
        cmp cx, 8
        ja err_sintaxis
        call salta_esp
        inc si
        cmp al, ','
        je .i
        cmp al, ')'
        je .ya
        cmp al, ']'
        jne err_sintaxis
.ya:
        mov [v_em_n], cx
        call repon_nombre           ; b_nom, DH
        mov [v_tipo_var], dh
        call busca_matriz           ; BX = la matriz o CF=1
        jnc .esta
        ; no está: con 10 en cada dimensión (11 elementos)
        mov cx, [v_em_n]
        mov di, v_aux2
.d10:
        mov word [di], 11
        add di, 2
        loop .d10
        mov cx, [v_em_n]
        call crea_matriz            ; BX = la matriz
.esta:
        ; BX en el tipo; tras [largo][nombre][tamaño] van [dims][dim..][datos]
        mov al, [bx + 1]
        xor ah, ah
        add bx, ax
        add bx, 4                   ; BX en «dims»
        mov cl, [bx]
        xor ch, ch
        cmp cx, [v_em_n]
        jne .fuera
        lea di, [bx + 1]            ; las dimensiones
        lea bx, [bp - 2]            ; el primer índice
        xor ax, ax                  ; lo que se lleva
.off:
        mul word [di]
        test dx, dx
        jnz .fuera
        mov dx, [bx]
        cmp dx, [di]
        jae .fuera
        add ax, dx
        jc .fuera
        sub bx, 2
        add di, 2
        loop .off
        ; DI = los datos; AX = el elemento
        mov bl, [v_tipo_var]
        xor bh, bh
        mul bx
        add ax, di
        mov bx, ax
        mov dh, [v_tipo_var]        ; (los MUL de arriba pisan DX: el tipo)
        mov sp, bp
        pop bp
        ret
.fuera:
        mov al, 9
        jmp error

; ---- la pila de nombres: guarda b_nom y DH / los repone ----
guarda_nombre:
        push si
        push di
        push cx
        mov ax, [v_nom_n]
        cmp ax, N_NOMS
        jae .honda
        inc word [v_nom_n]
        push dx
        mov cx, 42
        mul cx
        pop dx
        add ax, v_nom_pila
        mov di, ax
        mov si, b_nom
        mov cx, 41
        rep movsb
        mov [di], dh
        pop cx
        pop di
        pop si
        ret
.honda:
        mov al, 16
        jmp error
repon_nombre:
        push si
        push di
        push cx
        push ax
        dec word [v_nom_n]
        mov ax, [v_nom_n]
        push dx
        mov cx, 42
        mul cx
        pop dx
        add ax, v_nom_pila
        mov si, ax
        mov di, b_nom
        mov cx, 41
        rep movsb
        mov dh, [si]
        pop ax
        pop cx
        pop di
        pop si
        ret

; ---- busca la matriz b_nom / DH: BX, o CF=1 ----
busca_matriz:
        mov bx, [v_mats]
.b:
        cmp bx, [v_libre]
        jae .no
        call nombre_igual
        je .si
        mov al, [bx + 1]
        xor ah, ah
        add bx, ax
        add bx, 2
        add bx, [bx]
        add bx, 2
        jmp .b
.si:
        clc
        ret
.no:
        stc
        ret

; ---- crea la matriz b_nom / DH con CX dimensiones (sus tamaños en v_aux2):
; BX = ella ----
crea_matriz:
        push si
        push dx
        ; los elementos: el producto (MUL pisa DX: el tipo, antes a BL)
        mov bl, dh
        mov ax, 1
        mov si, v_aux2
        push cx
.p:
        mul word [si]
        jc .grande
        test dx, dx
        jnz .grande
        add si, 2
        loop .p
        pop cx
        xor bh, bh
        mul bx                      ; en octetos
        jc .grande2
        test dx, dx
        jnz .grande2
        ; + las dimensiones (1 + 2 por cada una)
        mov dx, cx
        shl dx, 1
        inc dx
        add ax, dx
        jc .grande2
        mov [v_aux + 12], ax        ; el tamaño (lo que va tras él)
        ; todo: tipo, largo, nombre, tamaño
        mov dl, [b_nom]
        xor dh, dh
        add ax, dx
        jc .grande2
        add ax, 4
        jc .grande2
        push cx
        mov cx, ax
        call hay_sitio
        mov bx, [v_libre]
        add [v_libre], cx
        ; a cero
        mov di, bx
        xor al, al
        rep stosb
        pop cx
        pop dx
        push dx
        mov di, bx
        call escribe_nombre
        mov ax, [v_aux + 12]
        stosw
        mov al, cl
        stosb
        mov si, v_aux2
        rep movsw
        pop dx
        pop si
        ret
.grande:
        pop cx
.grande2:
        mov al, 7
        jmp error

m_div_cero  db "División por cero", 0
