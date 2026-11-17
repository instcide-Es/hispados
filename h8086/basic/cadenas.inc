; =============================================================================
;  Hispa-BASIC - las cadenas
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Una cadena es un descriptor de 3 octetos: longitud (0 a 255) y dirección.
;  El texto está en el espacio de cadenas (de v_cads a v_cima, que crece hacia
;  abajo) o, si es una constante, dentro del programa. Las de paso (las que da
;  una expresión) tienen el descriptor en v_temps, que se vacía en cada
;  sentencia. Cuando el espacio se acaba, recoge_cadenas junta arriba las que
;  siguen vivas (las de las variables, las matrices y las de paso).
; =============================================================================

; ---- un descriptor de paso con CX (longitud) y DX (dirección): BX ----
nuevo_temp:
        cmp cx, 255
        ja .larga
        mov bx, [v_temp_n]
        cmp bx, N_TEMP
        jae .muchas
        inc word [v_temp_n]
        push ax
        mov ax, bx
        shl bx, 1
        add bx, ax
        add bx, v_temps
        mov [bx], cl
        mov [bx + 1], dx
        pop ax
        ret
.larga:
        mov al, 15
        jmp error
.muchas:
        mov al, 16
        jmp error

; ---- suelta el descriptor BX si es el último de paso (y su texto, si es lo
; último que se pidió). Guarda los registros ----
libera_temp:
        push ax
        push dx
        mov ax, [v_temp_n]
        test ax, ax
        jz .x
        dec ax
        mov dx, ax
        shl ax, 1
        add ax, dx
        add ax, v_temps
        cmp ax, bx
        jne .x
        dec word [v_temp_n]
        mov ax, [bx + 1]
        cmp ax, [v_cads]
        jne .x
        mov al, [bx]
        xor ah, ah
        add [v_cads], ax
.x:
        pop dx
        pop ax
        ret

; ---- pide CX octetos en el espacio de cadenas: DX = dónde ----
pide_cad:
        xor dx, dx
        jcxz .x
        push ax
        call .cabe
        jnc .si
        call recoge_cadenas
        call .cabe
        jnc .si
        mov al, 14
        jmp error
.si:
        sub [v_cads], cx
        mov dx, [v_cads]
        pop ax
.x:
        ret
.cabe:                              ; CF=1 si no cabe
        mov ax, [v_cads]
        sub ax, cx
        jc .no
        sub ax, 16
        jc .no
        cmp ax, [v_libre]
        jb .no
        clc
        ret
.no:
        stc
        ret

; ---- A$ = (el acumulador): BX = el descriptor de la variable ----
asigna_cadena:
        push si
        push di
        mov si, [ac + AC_M0]        ; el descriptor de lo que se asigna
        cmp si, bx
        je .x
        ; ¿es la última de paso y está en el espacio de cadenas? se queda
        ; con ella sin copiar
        mov ax, [v_temp_n]
        test ax, ax
        jz .copia
        dec ax
        mov dx, ax
        shl ax, 1
        add ax, dx
        add ax, v_temps
        cmp ax, si
        jne .copia
        mov ax, [si + 1]
        cmp ax, [v_cads]
        jb .copia
        dec word [v_temp_n]
        mov al, [si]
        mov [bx], al
        mov ax, [si + 1]
        mov [bx + 1], ax
        jmp .x
.copia:
        mov cl, [si]
        xor ch, ch
        push bx
        call pide_cad               ; DX (puede mover las cadenas)
        pop bx
        mov [bx], cl
        mov [bx + 1], dx
        push si
        mov si, [si + 1]
        mov di, dx
        rep movsb
        pop si
        mov bx, si
        call libera_temp
.x:
        pop di
        pop si
        ret

; ---- arg + ac (las dos cadenas): el resultado, de paso, en ac ----
concatena:
        cmp byte [ac + AC_TIPO], T_CAD
        jne err_tipos
        push si
        mov bx, [arg + AC_M0]
        mov di, [ac + AC_M0]
        mov cl, [bx]
        xor ch, ch
        mov al, [di]
        xor ah, ah
        add cx, ax
        cmp cx, 255
        ja .larga
        push cx
        call pide_cad               ; DX
        pop cx
        ; copiar (los descriptores, releídos: pueden haberse movido)
        push dx
        mov bx, [arg + AC_M0]
        mov di, dx
        mov si, [bx + 1]
        mov al, cl
        mov cl, [bx]
        xor ch, ch
        rep movsb
        mov bx, [ac + AC_M0]
        mov si, [bx + 1]
        mov cl, [bx]
        rep movsb
        mov cl, al
        pop dx
        ; soltar las dos de paso (primero la más nueva)
        mov bx, [ac + AC_M0]
        call libera_temp
        mov bx, [arg + AC_M0]
        call libera_temp
        call nuevo_temp
        mov [ac + AC_M0], bx
        mov byte [ac + AC_TIPO], T_CAD
        pop si
        ret
.larga:
        mov al, 15
        jmp error

; ---- una cadena nueva de paso con CX octetos de DS:SI (copiados): en ac.
; Guarda SI ----
cadena_de:
        push si
        push cx
        call pide_cad               ; (SI no puede señalar al espacio de
        pop cx                      ;  cadenas: si lo hiciera, se usa
        pop si                      ;  cadena_de_desc)
        push si
        push di
        push cx
        mov di, dx
        rep movsb
        pop cx
        pop di
        pop si
        call nuevo_temp
        mov [ac + AC_M0], bx
        mov byte [ac + AC_TIPO], T_CAD
        ret
; una de paso con CX octetos del descriptor BX desde el octeto AX (puede estar
; en el espacio de cadenas: se relee tras pedir el sitio). En ac
cadena_de_desc:
        push si
        push di
        push ax
        push bx
        push cx
        call pide_cad               ; DX
        pop cx
        pop bx
        pop ax
        mov si, [bx + 1]
        add si, ax
        mov di, dx
        push cx
        rep movsb
        pop cx
        call libera_temp            ; BX (el de origen, si era de paso)
        call nuevo_temp
        mov [ac + AC_M0], bx
        mov byte [ac + AC_TIPO], T_CAD
        pop di
        pop si
        ret

; =============================================================================
;  recoge_cadenas: junta arriba del todo las cadenas vivas. Guarda todo.
;  Busca cada vez la viva más alta que aún no se ha subido, la sube y sigue
;  por debajo de ella (como GW-BASIC: lento, pero sin memoria de más).
; =============================================================================
recoge_cadenas:
        push ax
        push bx
        push cx
        push dx
        push si
        push di
        mov ax, [v_cima]
        mov [v_rc_cima], ax         ; hasta dónde se ha llenado
        mov [v_rc_lim], ax          ; por debajo de esto se busca
.vuelta:
        mov word [v_rc_mejor], 0
        mov word [v_rc_dir], 0
        call rc_recorre
        mov bx, [v_rc_mejor]
        test bx, bx
        jz .fin
        ; subirla
        mov cl, [bx]
        xor ch, ch
        mov si, [bx + 1]
        mov [v_rc_lim], si
        add si, cx
        dec si
        mov di, [v_rc_cima]
        dec di
        std
        rep movsb
        cld
        inc di
        mov [bx + 1], di
        mov [v_rc_cima], di
        jmp .vuelta
.fin:
        mov ax, [v_rc_cima]
        mov [v_cads], ax
        pop di
        pop si
        pop dx
        pop cx
        pop bx
        pop ax
        ret

; mira todos los descriptores vivos y se queda con el de dirección más alta
; por debajo de v_rc_lim (y dentro del espacio de cadenas)
rc_recorre:
        ; las variables simples
        mov bx, [v_vars]
.s:
        cmp bx, [v_mats]
        jae .mats
        mov al, [bx + 1]
        xor ah, ah
        mov di, bx
        add di, ax
        add di, 2                   ; DI = el valor
        cmp byte [bx], T_CAD
        jne .s_sig
        call rc_mira
.s_sig:
        mov al, [bx]
        xor ah, ah
        add di, ax
        mov bx, di
        jmp .s
.mats:
        mov bx, [v_mats]
.m:
        cmp bx, [v_libre]
        jae .temps
        mov al, [bx + 1]
        xor ah, ah
        mov si, bx
        add si, ax
        add si, 2                   ; SI = el tamaño
        mov dx, [si]
        add dx, si
        add dx, 2                   ; DX = la siguiente
        cmp byte [bx], T_CAD
        jne .m_sig
        mov al, [si + 2]            ; dimensiones
        xor ah, ah
        shl ax, 1
        mov di, si
        add di, 3
        add di, ax                  ; DI = el primer elemento
.m_e:
        cmp di, dx
        jae .m_sig
        call rc_mira
        add di, 3
        jmp .m_e
.m_sig:
        mov bx, dx
        jmp .m
.temps:
        mov cx, [v_temp_n]
        mov di, v_temps
        jcxz .x
.t:
        call rc_mira
        add di, 3
        loop .t
.x:
        mov si, v_fn_pila           ; los valores guardados de los parámetros
.f:                                 ; de FN que son cadenas
        cmp si, [v_fnp]
        jae .y
        cmp byte [si + 2], T_CAD
        jne .f_sig
        lea di, [si + 4]
        call rc_mira
.f_sig:
        add si, FN_TAM
        jmp .f
.y:
        ret
; el descriptor DI: ¿es el mejor hasta ahora?
rc_mira:
        cmp byte [di], 0
        je .x
        mov ax, [di + 1]
        cmp ax, [v_cads]
        jb .x
        cmp ax, [v_rc_lim]
        jae .x
        cmp ax, [v_rc_dir]
        jb .x
        mov [v_rc_dir], ax
        mov [v_rc_mejor], di
.x:
        ret
