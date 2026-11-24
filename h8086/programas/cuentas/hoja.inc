; =============================================================================
;  CUENTAS - la hoja: celdas dispersas, el índice y deshacer
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Solo ocupan memoria las celdas que tienen algo. Cada celda es un registro
;  en el montón (hasta 256 K, fuera del segmento del programa) y el índice es
;  una lista de punteros de 16 bits ordenada por fila y columna: se busca por
;  bisección. Un puntero cuenta grupos de cuatro octetos desde el principio
;  del montón, así que 16 bits llegan a 256 K.
;
;  El registro:
;       +0  fila (palabra, desde 0; FFFFh = hueco libre)
;       +2  columna (0 = A ... 255 = IV)
;       +3  tipo: bits 0-1 (0 texto, 1 número, 2 fórmula); bit 7 = pendiente
;       +4  formato: bits 4-7 la clase (F_...), bits 0-3 los decimales
;       +5  largo del texto
;       +6  el valor, 8 octetos (números y fórmulas)
;       +14 la caja de lo que lee una fórmula: fila1, fila2 (palabras),
;           columna1, columna2 (octetos); fila1 = FFFFh si no lee nada
;       el texto: en +6 (texto), +20 (fórmula); los números no lo guardan
;
;  Deshacer: antes de tocar una celda, pon_celda guarda cómo estaba en el
;  búfer de deshacer (otro segmento). Deshacer la repone, guardando a su vez
;  en el otro búfer cómo estaba: Ctrl+Z otra vez lo rehace.
; =============================================================================

R_TIPO      equ 3
R_FMT       equ 4
R_LEN       equ 5
R_VAL       equ 6
R_CAJA      equ 14
T_TEXTO     equ 0
T_NUM       equ 1
T_FORM      equ 2
PENDIENTE   equ 0x80

; ---- AX = puntero → ES:DI = el registro (AX se pierde) ----
apunta:
        mov di, ax
        and di, 3
        shl di, 1
        shl di, 1
        shr ax, 1
        shr ax, 1
        add ax, [hp_seg]
        mov es, ax
        ret

; ---- ES:SI-ésimo del índice → ES:DI (SI = desplazamiento en el índice) ----
del_indice:
        mov es, [ix_seg]
        mov ax, [es:si]
        jmp apunta

; ---- busca la celda DX (fila), BL (columna). Encontrada: CF = 0, SI = su
; sitio en el índice y ES:DI = el registro. Si no: CF = 1 y SI = donde iría
busca:
        push cx
        push bp
        xor bp, bp
        mov cx, [n_cel]
.b:
        cmp bp, cx
        jae .no
        mov si, bp
        add si, cx
        shr si, 1
        push si
        shl si, 1
        call del_indice
        pop si
        cmp [es:di], dx
        jne .c
        cmp [es:di+2], bl
        je .si
.c:
        jb .menor
        mov cx, si
        jmp .b
.menor:
        inc si
        mov bp, si
        jmp .b
.si:
        shl si, 1
        clc
        jmp short .x
.no:
        mov si, bp
        shl si, 1
        stc
.x:
        pop bp
        pop cx
        ret

; ---- CX = octetos del registro ES:DI;  AX = dónde empieza su texto ----
tam_rec:
        call txt_rec
        mov cl, [es:di+R_LEN]
        xor ch, ch
        add cx, ax
        ret
txt_rec:
        mov al, [es:di+R_TIPO]
        and al, 3
        mov ah, 6
        jz .t
        mov ah, 14
        cmp al, T_FORM
        jne .t
        mov ah, 20
.t:
        mov al, ah
        xor ah, ah
        ret

; ---- AX = puntero a CX octetos del montón; CF si no caben ----
reserva:
        add cx, 3
        shr cx, 1
        shr cx, 1
        mov ax, [hp_top]
        add ax, cx
        jc .compacta
        cmp ax, [hp_lim]
        jbe .ok
.compacta:
        push cx
        call compacta
        pop cx
        mov ax, [hp_top]
        add ax, cx
        jc .no
        cmp ax, [hp_lim]
        ja .no
.ok:
        xchg ax, [hp_top]
        clc
        ret
.no:
        stc
        ret

; ---- junta los registros vivos al principio del montón ----
compacta:
        push dx
        push bx
        push si
        push di
        xor ax, ax
        mov [c_orig], ax
        mov [c_dest], ax
.l:
        mov ax, [c_orig]
        cmp ax, [hp_top]
        jae .fin
        call apunta
        call tam_rec
        add cx, 3
        shr cx, 1
        shr cx, 1
        mov [c_un], cx
        mov dx, [es:di]
        cmp dx, 0xFFFF
        je .sig
        mov ax, [c_dest]
        cmp ax, [c_orig]
        je .igual
        mov bl, [es:di+2]
        call busca                  ; su sitio en el índice: el nuevo puntero
        mov ax, [c_dest]
        mov es, [ix_seg]
        mov [es:si], ax
        push ds                     ; y se baja (hacia abajo no se pisa)
        mov ax, [c_orig]
        call apunta
        mov si, di
        mov bx, es
        mov ax, [c_dest]
        call apunta
        mov cx, [c_un]
        shl cx, 1
        mov ds, bx
        rep movsw
        pop ds
.igual:
        mov ax, [c_un]
        add [c_dest], ax
.sig:
        mov ax, [c_un]
        add [c_orig], ax
        jmp .l
.fin:
        mov ax, [c_dest]
        mov [hp_top], ax
        pop di
        pop si
        pop bx
        pop dx
        ret

; ---- pon_celda: la celda DX, BL pasa a ser el registro de DS:SI, de CX
; octetos (CX = 0: la celda se borra). Guarda antes cómo estaba (deshacer).
; CF si no hay memoria. El registro de SI ya debe llevar su fila y columna.
pon_celda:
        mov [pc_f], dx
        mov [pc_c], bl
        mov [pc_src], si
        mov [pc_tam], cx
        call guarda_viejo
        mov byte [cambios], 1
        mov dx, [pc_f]
        mov bl, [pc_c]
        call busca
        jnc .hay
        cmp word [pc_tam], 0        ; no estaba: ¿cabe en el índice?
        je .r
        mov ax, [n_cel]
        cmp ax, [ix_max]
        jae .llena
.hay:
        mov ax, 0xFFFF              ; AX = el puntero nuevo
        mov cx, [pc_tam]
        jcxz .borrar
        call reserva
        jc .llena
        push ax
        call apunta                 ; se copia
        mov si, [pc_src]
        mov cx, [pc_tam]
        rep movsb
        pop ax
.borrar:
        push ax
        mov dx, [pc_f]              ; (compactar puede haberlo movido todo)
        mov bl, [pc_c]
        call busca
        pop ax
        jc .nueva
        push ax                     ; el viejo queda libre
        call tam_rec
        mov word [es:di], 0xFFFF
        pop ax
        mov es, [ix_seg]
        cmp ax, 0xFFFF
        je .quita
        mov [es:si], ax
        clc
        ret
.quita:
        push ds                     ; se quita del índice
        mov di, si
        lea si, [di+2]
        mov cx, [n_cel]
        shl cx, 1
        sub cx, si
        shr cx, 1
        push es
        pop ds
        rep movsw
        pop ds
        dec word [n_cel]
        clc
        ret
.nueva:
        cmp ax, 0xFFFF
        je .r
        push ax                     ; se mete en el índice, en su sitio
        mov es, [ix_seg]
        mov cx, [n_cel]
        shl cx, 1
        mov di, cx
        sub cx, si
        shr cx, 1
        push ds
        push es
        pop ds
        lea si, [di-2]
        std
        rep movsw
        cld
        pop ds
        pop ax
        stosw                       ; (DI quedó en su sitio)
        inc word [n_cel]
.r:
        clc
        ret
.llena:
        mov word [aviso], m_llena
        stc
        ret

; ---- guarda en el búfer de deshacer cómo está la celda DX, BL ----
; la entrada: fila, columna, octetos (0 = vacía), el registro, y lo que mide
; la entrada entera (para recorrerlo de atrás adelante)
guarda_viejo:
        cmp byte [ud_graba], 0
        je .r
        cmp byte [ud_ok], 0
        je .r
        call busca
        mov cx, 0
        jc .v
        call tam_rec
.v:
        mov ax, [ud_fin]
        add ax, cx
        jc .no
        add ax, 7
        jc .no
        cmp ax, [ud_tam]
        ja .no
        push ds
        push es
        mov ax, es
        mov si, di
        mov es, [ud_a]
        mov di, [ud_fin]
        push ax
        mov ax, dx
        stosw
        mov al, bl
        stosb
        mov ax, cx
        stosw
        pop ds
        rep movsb
        pop ax                      ; (el ES de antes, que no hace falta)
        pop ds
        mov ax, di
        sub ax, [ud_fin]
        add ax, 2
        stosw
        mov [ud_fin], di
.r:
        ret
.no:
        mov byte [ud_ok], 0
        ret

; ---- antes de cada cambio: el búfer de deshacer, vacío ----
abre_ud:
        mov word [ud_fin], 0
        mov byte [ud_ok], 1
        mov byte [ud_hay], 1
        mov byte [ud_graba], 1
        ret

; ---- deshace el último cambio (y, la siguiente vez, lo rehace) ----
deshacer:
        mov word [aviso], m_nada_ud
        cmp byte [ud_hay], 0
        je .r
        mov word [aviso], m_no_ud
        cmp byte [ud_ok], 0
        je .r
        mov word [aviso], m_deshecho
        mov ax, [ud_a]              ; el otro búfer recoge lo de ahora
        xchg ax, [ud_b]
        mov [ud_a], ax
        mov bx, [ud_fin]
        call abre_ud
.l:
        test bx, bx
        jz .fin
        mov es, [ud_b]
        sub bx, [es:bx-2]           ; el principio de la entrada
        push bx
        mov dx, [es:bx]
        mov cx, [es:bx+3]
        push cx
        push ds                     ; el registro, a nr
        lea si, [bx+5]
        mov di, nr
        push ds
        push es
        pop ds
        pop es
        rep movsb
        pop ds
        mov es, [ud_b]
        mov bl, [es:bx+2]
        pop cx
        mov si, nr
        call pon_celda
        pop bx
        jmp .l
.fin:
        call recalcula_todo
        mov byte [sucio], 2
.r:
        ret
