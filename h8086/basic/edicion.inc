; =============================================================================
;  Hispa-BASIC - las órdenes de editar el programa (DELETE, RENUM, AUTO,
;  EDIT) y las de archivos sueltas (NAME, FILES)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
; =============================================================================

; ---- el rango «[n][-[m]]» de DELETE y LIST: BX = desde, DX = hasta ----
lee_rango:
        xor bx, bx
        mov dx, 65535
        call salta_esp
        cmp al, 0x0E
        jne .menos
        call lee_linea_cte
        mov bx, ax
        mov dx, ax
        call salta_esp
        cmp al, 0xEA
        jne .x
        mov dx, 65535
.menos:
        cmp al, 0xEA
        jne .x
        inc si
        call salta_esp
        cmp al, 0x0E
        jne .x
        push bx
        call lee_linea_cte
        pop bx
        mov dx, ax
.x:
        ret

; ---- DELETE n[-m] ----
sent_delete:
        call lee_rango
        mov [v_aux2], bx
        mov [v_aux2 + 2], dx
.l:
        ; la primera línea del rango que quede
        mov ax, [v_aux2]
        call busca_linea            ; BX = ella o la siguiente
        cmp word [bx], 0
        je .x
        mov ax, [bx + 2]
        cmp ax, [v_aux2 + 2]
        ja .x
        mov cx, 1                   ; (sin texto: se borra)
        push si
        call mete_linea
        pop si
        jmp .l
.x:
        cmp word [v_linea], 0xFFFF  ; (en un programa, sigue en las órdenes)
        jne ordenes_ok
        ret

; ---- RENUM [nueva [, vieja [, paso]]] ----
sent_renum:
        mov word [v_aux2], 10       ; la primera nueva
        mov word [v_aux2 + 2], 0    ; desde la vieja
        mov word [v_aux2 + 4], 10   ; el paso
        call salta_esp
        call fin_de_sent
        je .ya
        cmp al, ','
        je .r2
        call lee_linea_cte
        mov [v_aux2], ax
.r2:
        call salta_esp
        cmp al, ','
        jne .ya
        inc si
        call salta_esp
        cmp al, ','
        je .r3
        call lee_linea_cte
        mov [v_aux2 + 2], ax
.r3:
        call salta_esp
        cmp al, ','
        jne .ya
        inc si
        call eval_ent
        test ax, ax
        jle .mal
        mov [v_aux2 + 4], ax
.ya:
        ; ¿caben? la última nueva no puede pasar de 65529, y las nuevas no
        ; pueden ir por delante de una línea que no se cambia
        mov bx, PROG_INI
        mov ax, [v_aux2]
.c:
        cmp word [bx], 0
        je .cabe
        mov cx, [bx + 2]
        cmp cx, [v_aux2 + 2]
        jb .c_sig_viejo
        cmp ax, 65530
        jae .mal
        add ax, [v_aux2 + 4]
        jc .mal_c
        jmp .c_sig
.c_sig_viejo:
        cmp cx, [v_aux2]            ; una que se queda, con número mayor que
        jae .mal                    ; la primera nueva: se desordenaría
.c_sig:
        mov bx, [bx]
        jmp .c
.mal_c:
        ; (pasarse del todo con la última es posible si no hay más)
        mov bx, [bx]
        cmp word [bx], 0
        jne .mal
.cabe:
        ; 1) las referencias (0Eh nn nn) de todas las líneas, a su número nuevo
        mov bx, PROG_INI
.lr:
        cmp word [bx], 0
        je .numeros
        lea di, [bx + 4]
.t:
        mov al, [di]
        inc di
        test al, al
        jz .lr_sig
        cmp al, '"'
        je .com
        cmp al, 0x8F                ; REM: hasta el final
        je .lr_sig
        cmp al, 0x84                ; DATA: hasta «:»
        je .dat
        cmp al, 0x0E
        je .ref
        cmp al, 0x20
        jb .cte
        cmp al, 0xFD
        jb .t
        inc di
        jmp .t
.com:
        mov al, [di]
        inc di
        test al, al
        jz .lr_sig
        cmp al, '"'
        jne .com
        jmp .t
.dat:
        mov al, [di]
        test al, al
        jz .t
        cmp al, ':'
        je .t
        inc di
        cmp al, '"'
        jne .dat
.dat_c:
        mov al, [di]
        test al, al
        jz .t
        inc di
        cmp al, '"'
        jne .dat_c
        jmp .dat
.cte:
        push si
        mov si, di
        call salta_cte
        mov di, si
        pop si
        jmp .t
.ref:
        mov ax, [di]
        call renum_nuevo            ; AX viejo -> AX nuevo (CF=1: no está)
        jc .ref_x
        mov [di], ax
.ref_x:
        add di, 2
        jmp .t
.lr_sig:
        mov bx, [bx]
        jmp .lr
        ; 2) los números de las líneas
.numeros:
        mov bx, PROG_INI
        mov ax, [v_aux2]
.n:
        cmp word [bx], 0
        je .fin
        mov cx, [bx + 2]
        cmp cx, [v_aux2 + 2]
        jb .n_sig
        mov [bx + 2], ax
        add ax, [v_aux2 + 4]
.n_sig:
        mov bx, [bx]
        jmp .n
.fin:
        call vacia
        cmp word [v_linea], 0xFFFF
        jne ordenes_ok
        ret
.mal:
        jmp err_funcion
; el número nuevo de la línea vieja AX (las que se cambian van en orden)
renum_nuevo:
        push bx
        push cx
        mov cx, [v_aux2]
        mov bx, PROG_INI
.b:
        cmp word [bx], 0
        je .no
        cmp [bx + 2], ax
        je .es
        cmp word [bx + 2], 0        ; (no se usa)
        push ax
        mov ax, [bx + 2]
        cmp ax, [v_aux2 + 2]
        pop ax
        jb .s
        add cx, [v_aux2 + 4]
.s:
        mov bx, [bx]
        jmp .b
.es:
        push ax
        mov ax, [bx + 2]
        cmp ax, [v_aux2 + 2]
        pop ax
        jb .igual                   ; una que no se cambia
        mov ax, cx
.igual:
        clc
        pop cx
        pop bx
        ret
.no:
        stc
        pop cx
        pop bx
        ret

; ---- AUTO [n [, paso]]: numera solo las líneas que se escriben ----
sent_auto:
        mov word [v_auto_n], 10
        mov word [v_auto_paso], 10
        call salta_esp
        call fin_de_sent
        je .ya
        cmp al, ','
        je .p
        call lee_linea_cte
        mov [v_auto_n], ax
.p:
        call salta_esp
        cmp al, ','
        jne .ya
        inc si
        call eval_ent
        test ax, ax
        jle .mal
        mov [v_auto_paso], ax
.ya:
        mov byte [v_auto], 1
        jmp ordenes                 ; (sin «Ok»: ya pide la primera línea)
.mal:
        jmp err_funcion

; ---- EDIT n: deja la línea en la pantalla para corregirla ----
; El texto de la línea se le da al sistema (Hispa-8086, INT 21h/E808h): la
; línea que se teclee a continuación empieza con él, con el cursor al final,
; y se corrige con las flechas, Inicio, Fin, Supr, Insert... Si no puede ser
; (dentro de un programa, con la entrada o la salida redirigidas, o en otro
; DOS), la línea se enseña y se escribe otra vez
sent_edit:
        call lee_linea_cte
        call busca_linea
        jnc .no
        cmp word [v_linea], 0xFFFF
        jne .lista
        mov word [v_sal_mem], b_us
        call lista_linea
        push bx
        mov bx, [v_sal_mem]
        mov byte [bx], 0
        pop bx
        mov word [v_sal_mem], 0
        push bx
        mov dx, b_us
        mov ax, 0xE808
        stc
        int 0x21
        pop bx
        jc .lista
        cmp ax, 0x4564
        jne .lista
        jmp ordenes                 ; (sin «Ok»: lo siguiente es esa línea)
.lista:
        jmp lista_linea
.no:
        mov al, 8
        jmp error

; ---- NAME "viejo" AS "nuevo" ----
sent_name:
        call eval_cad
        call desc_a_arch
        push si
        mov si, b_arch
        mov di, b_us                ; (de paso)
.c:
        lodsb
        stosb
        test al, al
        jnz .c
        pop si
        call salta_esp
        call mayus
        cmp al, 'A'
        jne err_sintaxis
        mov al, [si + 1]
        call mayus
        cmp al, 'S'
        jne err_sintaxis
        add si, 2
        call eval_cad
        call desc_a_arch
        push si
        mov dx, b_us
        mov di, b_arch
        mov ah, 0x56
        int 0x21
        pop si
        jc .no
        ret
.no:
        cmp ax, 2
        je .no_esta
        mov al, 58                  ; ya existe (o no se puede)
        jmp error
.no_esta:
        mov al, 53
        jmp error

; ---- FILES ["patrón"]: los archivos, como DIR /W ----
sent_files:
        mov word [b_arch], '*.'
        mov word [b_arch + 2], '*'
        call salta_esp
        call fin_de_sent
        je .ya
        call eval_cad
        call desc_a_arch
.ya:
        push si
        ; el DTA, en b_disco
        mov dx, b_disco
        mov ah, 0x1A
        int 0x21
        mov dx, b_arch
        mov cx, 0x10                ; con los directorios
        mov ah, 0x4E
        int 0x21
        jc .nada
.uno:
        ; el nombre (b_disco+1Eh), en 13 columnas; los directorios con <DIR>
        mov si, b_disco + 0x1E
        xor cx, cx
.n:
        lodsb
        test al, al
        jz .n_f
        call pon_car
        inc cx
        jmp .n
.n_f:
        test byte [b_disco + 0x15], 0x10
        jz .rel
        mov al, '<'
        call pon_car
        mov al, 'D'
        call pon_car
        mov al, '>'
        call pon_car
        add cx, 3
.rel:
        mov al, ' '
.r:
        call pon_car
        inc cx
        cmp cx, 18
        jb .r
        mov ax, [v_col]
        add ax, 18
        cmp ax, [v_ancho]
        jbe .sig
        call pon_nl
.sig:
        mov ah, 0x4F
        int 0x21
        jnc .uno
        call col_cero
        ; los libres
        mov ah, 0x36
        mov dl, 0
        int 0x21
        cmp ax, 0xFFFF
        je .x
        mul cx
        mul bx                      ; (DX:AX = octetos libres; hasta 64 K
        mov di, ac                  ;  en un disquete de 360 K cabe)
        push ax
        call f_cero
        pop ax
        mov byte [di + AC_TIPO], T_SIM
        mov [di + AC_M3], dx
        mov [di + AC_M2], ax
        mov word [di + AC_EXP], 32
        call f_normaliza
        call num_a_texto
        mov si, b_num
        call pon_cad
        mov si, m_libres
        call pon_cad_cs
.x:
        pop si
        ret
.nada:
        pop si
        mov al, 53
        jmp error
