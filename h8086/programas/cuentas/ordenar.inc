; =============================================================================
;  CUENTAS - ordenar las filas de un rango
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Ordena las filas del rango q_ por la columna del cursor: los números antes
;  que los textos, los textos sin distinguir mayúsculas y con la eñe detrás de
;  la ene, y las vacías siempre al final. El orden es estable (las iguales
;  quedan como estaban), por mezcla.
;
;  Cómo: primero se guarda en el búfer de deshacer lo que hay en el rango (es
;  a la vez la copia de donde se lee y lo que permite deshacer), y en el otro
;  búfer se hacen las listas: P (el orden), T (para mezclar), K (la celda que
;  decide de cada fila) y O (dónde empieza cada fila en la copia). Luego se
;  escribe cada fila en su sitio nuevo; las referencias relativas de sus
;  fórmulas se mueven con ella, como al copiar.
; =============================================================================

ordena:
        mov ax, [q_f2]
        sub ax, [q_f1]
        inc ax
        mov [o_n], ax
        mov cl, 3                   ; ¿caben las cuatro listas?
        shl ax, cl
        jc .grande
        cmp ax, [ud_tam]
        ja .grande
        mov ax, [o_n]
        shl ax, 1
        mov [o_t], ax
        add ax, [o_n]
        add ax, [o_n]
        mov [o_k], ax
        add ax, [o_n]
        add ax, [o_n]
        mov [o_o], ax
        mov al, [cc]                ; la columna que decide, dentro del rango
        cmp al, [q_c1]
        jae .k1
        mov al, [q_c1]
.k1:
        cmp al, [q_c2]
        jbe .k2
        mov al, [q_c2]
.k2:
        mov [o_kc], al
        xor cx, cx                  ; P[i] = i, K[i] = la celda (o FFFFh)
.l1:
        mov dx, [q_f1]
        add dx, cx
        mov bl, [o_kc]
        call busca
        mov ax, 0xFFFF
        jc .k3
        cmp byte [es:di+R_TIPO], T_TEXTO
        jne .k4
        cmp byte [es:di+R_LEN], 0   ; (un texto vacío cuenta como vacía)
        je .k3
.k4:
        mov es, [ix_seg]
        mov ax, [es:si]
.k3:
        mov es, [ud_b]
        mov di, cx
        shl di, 1
        mov [es:di], cx
        add di, [o_k]
        mov [es:di], ax
        inc cx
        cmp cx, [o_n]
        jb .l1
        call mezcla
        ; la copia de lo que hay (y lo que se pondrá donde no hay nada)
        call abre_ud
        xor cx, cx
.s1:
        mov es, [ud_b]
        mov di, cx
        shl di, 1
        mov si, [es:di]             ; SI = la fila de donde vendrá
        add di, [o_o]
        mov ax, [ud_fin]
        mov [es:di], ax
        mov bl, [q_c1]
.s2:
        mov dx, [q_f1]
        add dx, cx
        push si
        push cx
        call busca
        jnc .s3
        pop cx
        pop si
        push si
        push cx
        mov dx, [q_f1]
        add dx, si
        call busca
        jc .s4
.s3:
        pop cx
        pop si
        push si
        push cx
        mov dx, [q_f1]
        add dx, cx
        call guarda_viejo
.s4:
        pop cx
        pop si
        cmp bl, [q_c2]
        je .s5
        inc bl
        jmp .s2
.s5:
        inc cx
        cmp cx, [o_n]
        jb .s1
        cmp byte [ud_ok], 0
        je .grande0
        ; y ahora, cada fila a su sitio
        mov byte [ud_graba], 0
        xor cx, cx
.p1:
        mov es, [ud_b]
        mov di, cx
        shl di, 1
        mov si, [es:di]             ; la fila de origen
        cmp si, cx
        je .p9
        mov [o_src], si
        shl si, 1
        add si, [o_o]
        mov ax, [es:si]
        mov [o_pos], ax             ; donde empieza en la copia
        mov bl, [q_c1]
.p2:
        push cx
        push bx
        mov [en_c], bl
        mov ax, [q_f1]
        add ax, cx
        mov [en_f], ax
        call de_la_copia            ; nr y CX, o CX = 0
        mov dx, [en_f]
        mov bl, [en_c]
        mov si, nr
        call pon_celda
        pop bx
        pop cx
        cmp bl, [q_c2]
        je .p9
        inc bl
        jmp .p2
.p9:
        inc cx
        cmp cx, [o_n]
        jb .p1
        mov byte [ud_graba], 1
        call recalcula_todo
        jmp aviso_circ
.grande0:
        mov byte [ud_hay], 0
.grande:
        mov word [aviso], m_grande
        ret

; ---- busca en la copia la celda de la fila o_src, columna en_c (desde
; o_pos); la deja en nr, con su sitio nuevo, y CX = su tamaño (0: vacía) ----
de_la_copia:
        mov es, [ud_a]
        mov bx, [o_pos]
        mov dx, [q_f1]
        add dx, [o_src]
.b:
        cmp bx, [ud_fin]
        jae .no
        cmp [es:bx], dx
        jne .no
        mov al, [es:bx+2]
        cmp al, [en_c]
        je .si
        ja .no
        add bx, [es:bx+3]
        add bx, 7
        jmp .b
.si:
        mov cx, [es:bx+3]
        jcxz .no
        push cx
        push ds
        lea si, [bx+5]
        mov di, nr
        push ds
        push es
        pop ds
        pop es
        rep movsb
        pop ds
        pop cx
        mov ax, [en_f]
        mov [nr], ax
        mov al, [nr + R_TIPO]
        and al, 3
        cmp al, T_FORM
        jne .r
        mov ax, [en_f]              ; una fórmula: sus referencias, con la fila
        sub ax, dx
        mov [dsp_f], ax
        mov word [dsp_c], 0
        call mueve_nr
        jnc .r
.no:
        xor cx, cx
.r:
        ret

; ---- ordena P (n = o_n) mirando K: de abajo arriba, mezclando de dos en dos
mezcla:
        mov word [o_w], 1
.pasada:
        mov ax, [o_w]
        cmp ax, [o_n]
        jae .fin
        xor ax, ax                  ; lo = 0
.tramo:
        mov [o_lo], ax
        cmp ax, [o_n]
        jae .copia
        add ax, [o_w]
        call .min_n
        mov [o_mid], ax
        add ax, [o_w]
        call .min_n
        mov [o_hi], ax
        mov si, [o_lo]              ; i
        mov di, [o_mid]             ; j
        mov bx, [o_lo]              ; k
.m:
        cmp si, [o_mid]
        jae .resto_j
        cmp di, [o_hi]
        jae .resto_i
        push si
        push di
        push bx
        call .compara               ; ¿P[j] va antes que P[i]?
        pop bx
        pop di
        pop si
        jl .toma_j
.toma_i:
        call .lee_p_si
        inc si
        jmp short .pon
.toma_j:
        xchg si, di
        call .lee_p_si
        xchg si, di
        inc di
.pon:
        push di
        mov di, bx
        shl di, 1
        add di, [o_t]
        mov [es:di], ax
        pop di
        inc bx
        jmp .m
.resto_j:
        cmp di, [o_hi]
        jae .sig
        jmp .toma_j
.resto_i:
        jmp .toma_i
.sig:
        mov ax, [o_hi]
        jmp .tramo
.copia:
        push ds                     ; T vuelve a P
        mov es, [ud_b]
        push es
        pop ds
        mov si, [cs:o_t]
        xor di, di
        mov cx, [cs:o_n]
        rep movsw
        pop ds
        shl word [o_w], 1
        jmp .pasada
.fin:
        ret
.min_n:
        jc .n
        cmp ax, [o_n]
        jbe .r
.n:
        mov ax, [o_n]
.r:
        ret
.lee_p_si:
        mov es, [ud_b]
        push si
        shl si, 1
        mov ax, [es:si]
        pop si
        ret
; compara las filas P[DI] y P[SI]: flags como de un CMP con signo
.compara:
        xchg si, di
        call .lee_p_si
        xchg si, di
        push ax
        call .lee_p_si
        mov dx, ax                  ; DX = P[i]
        pop ax                      ; AX = P[j]
        mov si, ax
        shl si, 1
        add si, [o_k]
        mov ax, [es:si]
        mov si, dx
        shl si, 1
        add si, [o_k]
        mov dx, [es:si]
        call cmp_celdas
        cmp al, 0
        ret

; ---- compara las celdas de los punteros AX y DX: AL = -1, 0 o 1 ----
cmp_celdas:
        cmp ax, 0xFFFF              ; las vacías, al final siempre
        jne .a
        cmp dx, 0xFFFF
        je .igual
        mov al, 1
        ret
.a:
        cmp dx, 0xFFFF
        jne .b
        mov al, -1
        ret
.b:
        mov [o_pa], ax
        mov [o_pb], dx
        call apunta
        mov al, [es:di+R_TIPO]
        and al, 3
        mov [o_ta], al
        mov ax, [o_pb]
        call apunta
        mov ah, [es:di+R_TIPO]
        and ah, 3
        mov al, [o_ta]
        test al, al
        jz .ta
        test ah, ah                 ; número frente a texto: antes
        jz .menor
        call valor_a_acc            ; dos números
        mov si, acc
        mov di, opb
        call copia8
        mov ax, [o_pa]
        call apunta
        call valor_a_acc
        mov si, acc
        mov di, opb
        call cmp64
        jmp short .dir
.ta:
        test ah, ah
        jnz .mayor
        mov ax, [o_pa]              ; dos textos: el primero, a ftxt
        call apunta
        call txt_rec
        mov si, ax
        add si, di
        mov cl, [es:di+R_LEN]
        xor ch, ch
        mov [o_la], cx
        push ds
        mov di, ftxt
        push ds
        push es
        pop ds
        pop es
        rep movsb
        pop ds
        mov ax, [o_pb]
        call apunta
        mov cl, [es:di+R_LEN]
        xor ch, ch
        call txt_rec
        add di, ax
        mov si, ftxt
        mov bx, [o_la]
.t:
        test bx, bx
        jz .t_fin_a
        jcxz .mayor
        mov al, [si]
        call peso
        mov dx, ax
        mov al, [es:di]
        call peso
        cmp dx, ax
        jb .menor
        ja .mayor
        inc si
        inc di
        dec bx
        dec cx
        jmp .t
.t_fin_a:
        jcxz .igual
        jmp short .menor
.menor:
        mov al, -1
        jmp short .dir
.mayor:
        mov al, 1
.dir:
        cmp byte [o_desc], 0
        je .r
        neg al
.r:
        ret
.igual:
        xor al, al
        ret

; ---- AX = el peso de la letra AL al ordenar ----
peso:
        push si
        call mayus
        mov si, acentos
        push cx
        mov cx, (acentos_fin - acentos) / 2
.l:
        cmp al, [cs:si]
        je .es
        add si, 2
        loop .l
        pop cx
        pop si
        xor ah, ah
        shl ax, 1
        ret
.es:
        mov al, [cs:si+1]
        pop cx
        xor ah, ah
        shl ax, 1
        cmp byte [cs:si], 0xA4      ; la eñe, justo detrás de la ene
        je .e
        cmp byte [cs:si], 0xA5
        jne .r
.e:
        inc ax
.r:
        pop si
        ret
acentos     db 0xA0, 'A', 0x85, 'A', 0x83, 'A', 0x84, 'A', 0x82, 'E', 0x8A, 'E'
            db 0x90, 'E', 0x88, 'E', 0xA1, 'I', 0x8D, 'I', 0x8B, 'I', 0xA2, 'O'
            db 0x95, 'O', 0x94, 'O', 0xA3, 'U', 0x97, 'U', 0x81, 'U', 0x9A, 'U'
            db 0x87, 'C', 0x80, 'C', 0xA4, 'N', 0xA5, 'N'
acentos_fin:
