; =============================================================================
;  Hispa-BASIC - los números con decimales (sin coprocesador)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Un número va «desempaquetado» en una estructura de AC_TAM octetos (ver
;  datos.inc): signo, exponente con signo y mantisa de 64 bits con el bit alto
;  puesto; valor = 0,1mmm… × 2^exp. Las operaciones son f_xxx con DI = el
;  destino y SI = el otro operando; guardan todos los registros.
;  En memoria y en el programa van en el formato binario de Microsoft (MBF),
;  como en GW-BASIC: simple, 4 octetos; doble, 8.
; =============================================================================

; ---- poner a cero el número de DI ----
f_cero:
        mov word [di + AC_M0], 0
        mov word [di + AC_M1], 0
        mov word [di + AC_M2], 0
        mov word [di + AC_M3], 0
        mov word [di + AC_EXP], 0
        mov byte [di + AC_SIG], 0
        ret

; ---- ¿es cero el de DI? ZF=1 si lo es ----
f_es_cero:
        cmp word [di + AC_M3], 0
        ret

; ---- copiar el de SI en DI (la estructura entera) ----
f_copia:
        push cx
        push si
        push di
        mov cx, AC_TAM / 2
        rep movsw
        pop di
        pop si
        pop cx
        ret

; ---- mantisa de DI a la derecha CL bits (CL de 0 a 255) ----
f_dcha:
        push cx
        xor ch, ch
.w:
        cmp cx, 16
        jb .b
        push ax
        mov ax, [di + AC_M1]
        mov [di + AC_M0], ax
        mov ax, [di + AC_M2]
        mov [di + AC_M1], ax
        mov ax, [di + AC_M3]
        mov [di + AC_M2], ax
        mov word [di + AC_M3], 0
        pop ax
        sub cx, 16
        jmp .w
.b:
        jcxz .x
.b1:
        shr word [di + AC_M3], 1
        rcr word [di + AC_M2], 1
        rcr word [di + AC_M1], 1
        rcr word [di + AC_M0], 1
        loop .b1
.x:
        pop cx
        ret

; ---- normalizar el de DI (bit alto puesto, o cero) ----
f_normaliza:
        push ax
        mov ax, [di + AC_M3]
        or ax, [di + AC_M2]
        or ax, [di + AC_M1]
        or ax, [di + AC_M0]
        jz .cero
.w:
        cmp word [di + AC_M3], 0
        jne .b
        mov ax, [di + AC_M2]
        mov [di + AC_M3], ax
        mov ax, [di + AC_M1]
        mov [di + AC_M2], ax
        mov ax, [di + AC_M0]
        mov [di + AC_M1], ax
        mov word [di + AC_M0], 0
        sub word [di + AC_EXP], 16
        jmp .w
.b:
        test byte [di + AC_M3 + 1], 0x80
        jnz .x
        shl word [di + AC_M0], 1
        rcl word [di + AC_M1], 1
        rcl word [di + AC_M2], 1
        rcl word [di + AC_M3], 1
        dec word [di + AC_EXP]
        jmp .b
.cero:
        mov word [di + AC_EXP], 0
        mov byte [di + AC_SIG], 0
.x:
        pop ax
        ret

; ---- redondear el de DI a su tipo (simple: 24 bits; doble: 56) ----
f_redondea:
        cmp byte [di + AC_TIPO], T_DOB
        je .dob
        call f_es_cero
        jz .x
        add word [di + AC_M2], 0x80
        adc word [di + AC_M3], 0
        jc .lleva
        and word [di + AC_M2], 0xFF00
        mov word [di + AC_M1], 0
        mov word [di + AC_M0], 0
        ret
.dob:
        call f_es_cero
        jz .x
        add word [di + AC_M0], 0x80
        adc word [di + AC_M1], 0
        adc word [di + AC_M2], 0
        adc word [di + AC_M3], 0
        jc .lleva
        and word [di + AC_M0], 0xFF00
.x:
        ret
.lleva:                             ; 0,1111… redondeado: 1,0
        mov word [di + AC_M3], 0x8000
        mov word [di + AC_M2], 0
        mov word [di + AC_M1], 0
        mov word [di + AC_M0], 0
        inc word [di + AC_EXP]
        ret

; ---- un entero (AX, con signo) al número de DI, de tipo simple ----
f_de_entero:
        push ax
        call f_cero
        mov byte [di + AC_TIPO], T_SIM
        test ax, ax
        jz .x
        jns .pos
        neg ax
        mov byte [di + AC_SIG], 0x80
.pos:
        mov [di + AC_M3], ax
        mov word [di + AC_EXP], 16
        call f_normaliza
.x:
        pop ax
        ret

; ---- suma: DI = DI + SI ----
f_resta:
        xor byte [si + AC_SIG], 0x80
        call f_suma
        xor byte [si + AC_SIG], 0x80
        ret
f_suma:
        push ax
        push bx
        push cx
        push si
        cmp word [si + AC_M3], 0
        je .x                       ; + 0
        cmp word [di + AC_M3], 0
        jne .los_dos
        mov al, [di + AC_TIPO]
        call f_copia
        mov [di + AC_TIPO], al
        jmp .x
.los_dos:
        ; el otro, a la derecha, en ac_suma (no se toca el de SI; una zona
        ; propia: quien llama puede usar cualquier acumulador, ac4 también)
        push di
        mov di, ac_suma
        call f_copia
        pop di
        mov si, ac_suma
        mov ax, [di + AC_EXP]
        sub ax, [si + AC_EXP]
        jge .ya
        ; el de SI es mayor: se intercambian las mantisas y los exponentes
        call f_cambia_man
        neg ax
.ya:
        cmp ax, 66
        jae .x                      ; el pequeño no llega a contar
        mov cl, al
        push di
        mov di, si
        call f_dcha
        pop di
        mov al, [di + AC_SIG]
        cmp al, [si + AC_SIG]
        jne .restar
        ; mismo signo: se suman
        mov ax, [si + AC_M0]
        add [di + AC_M0], ax
        mov ax, [si + AC_M1]
        adc [di + AC_M1], ax
        mov ax, [si + AC_M2]
        adc [di + AC_M2], ax
        mov ax, [si + AC_M3]
        adc [di + AC_M3], ax
        jnc .x
        rcr word [di + AC_M3], 1
        rcr word [di + AC_M2], 1
        rcr word [di + AC_M1], 1
        rcr word [di + AC_M0], 1
        inc word [di + AC_EXP]
        jmp .x
.restar:
        ; el mayor menos el menor; el signo, el del mayor
        mov ax, [di + AC_M3]
        cmp ax, [si + AC_M3]
        jne .cmp_ya
        mov ax, [di + AC_M2]
        cmp ax, [si + AC_M2]
        jne .cmp_ya
        mov ax, [di + AC_M1]
        cmp ax, [si + AC_M1]
        jne .cmp_ya
        mov ax, [di + AC_M0]
        cmp ax, [si + AC_M0]
.cmp_ya:
        jae .res
        call f_cambia_man
        mov al, [si + AC_SIG]       ; (se cambiaron también los signos)
.res:
        mov ax, [si + AC_M0]
        sub [di + AC_M0], ax
        mov ax, [si + AC_M1]
        sbb [di + AC_M1], ax
        mov ax, [si + AC_M2]
        sbb [di + AC_M2], ax
        mov ax, [si + AC_M3]
        sbb [di + AC_M3], ax
        call f_normaliza
.x:
        pop si
        pop cx
        pop bx
        pop ax
        ret
; intercambia mantisa, exponente y signo de DI y SI
f_cambia_man:
        push ax
        push bx
        mov bx, AC_SIG
.c:
        mov al, [di + bx]
        xchg al, [si + bx]
        mov [di + bx], al
        inc bx
        cmp bx, AC_TAM
        jb .c
        pop bx
        pop ax
        ret

; ---- multiplicación: DI = DI × SI ----
f_mul:
        push ax
        push bx
        push cx
        push dx
        push bp
        cmp word [di + AC_M3], 0
        je .x
        cmp word [si + AC_M3], 0
        jne .no_cero
        mov al, [di + AC_TIPO]
        call f_cero
        mov [di + AC_TIPO], al
        jmp .x
.no_cero:
        mov al, [si + AC_SIG]
        xor [di + AC_SIG], al
        mov ax, [si + AC_EXP]
        add [di + AC_EXP], ax
        ; el producto de 128 bits en v_aux2 (8 palabras)
        push di
        mov bp, v_aux2
        mov cx, 8
.z:
        mov word [bp], 0
        add bp, 2
        loop .z
        pop di
        xor bx, bx                  ; i (en octetos: 0, 2, 4, 6)
.i:
        xor bp, bp                  ; j
.j:
        mov ax, [di + AC_M0 + bx]
        mul word [si + AC_M0 + bp]
        push bx
        add bx, bp
        add [v_aux2 + bx], ax
        adc [v_aux2 + bx + 2], dx
        jnc .sin
        add bx, 4
.lleva:
        cmp bx, 16
        jae .sin
        add word [v_aux2 + bx], 1
        jnc .sin
        add bx, 2
        jmp .lleva
.sin:
        pop bx
        add bp, 2
        cmp bp, 8
        jb .j
        add bx, 2
        cmp bx, 8
        jb .i
        ; las cuatro palabras altas son la mantisa
        mov ax, [v_aux2 + 8]
        mov [di + AC_M0], ax
        mov ax, [v_aux2 + 10]
        mov [di + AC_M1], ax
        mov ax, [v_aux2 + 12]
        mov [di + AC_M2], ax
        mov ax, [v_aux2 + 14]
        mov [di + AC_M3], ax
        test ah, 0x80
        jnz .x
        mov ax, [v_aux2 + 6]        ; un bit más de abajo
        shl ax, 1
        rcl word [di + AC_M0], 1
        rcl word [di + AC_M1], 1
        rcl word [di + AC_M2], 1
        rcl word [di + AC_M3], 1
        dec word [di + AC_EXP]
.x:
        pop bp
        pop dx
        pop cx
        pop bx
        pop ax
        ret

; ---- división: DI = DI / SI (SI no puede ser cero: lo mira quien llama) ----
f_div:
        push ax
        push bx
        push cx
        push dx
        cmp word [di + AC_M3], 0
        je .x
        mov al, [si + AC_SIG]
        xor [di + AC_SIG], al
        mov ax, [si + AC_EXP]
        sub [di + AC_EXP], ax
        inc word [di + AC_EXP]
        ; resto R = la mantisa de DI (en v_aux, 4 palabras); cociente en
        ; v_aux2; 64 vueltas
        mov ax, [di + AC_M0]
        mov [v_aux], ax
        mov ax, [di + AC_M1]
        mov [v_aux + 2], ax
        mov ax, [di + AC_M2]
        mov [v_aux + 4], ax
        mov ax, [di + AC_M3]
        mov [v_aux + 6], ax
        xor ax, ax
        mov [v_aux2], ax
        mov [v_aux2 + 2], ax
        mov [v_aux2 + 4], ax
        mov [v_aux2 + 6], ax
        mov cx, 64
        xor dl, dl                  ; el bit 65 del resto
.v:
        ; ¿R >= divisor?
        test dl, dl
        jnz .cabe
        mov ax, [v_aux + 6]
        cmp ax, [si + AC_M3]
        jne .cmp
        mov ax, [v_aux + 4]
        cmp ax, [si + AC_M2]
        jne .cmp
        mov ax, [v_aux + 2]
        cmp ax, [si + AC_M1]
        jne .cmp
        mov ax, [v_aux]
        cmp ax, [si + AC_M0]
.cmp:
        jb .no_cabe
.cabe:
        mov ax, [si + AC_M0]
        sub [v_aux], ax
        mov ax, [si + AC_M1]
        sbb [v_aux + 2], ax
        mov ax, [si + AC_M2]
        sbb [v_aux + 4], ax
        mov ax, [si + AC_M3]
        sbb [v_aux + 6], ax
        stc
        jmp .bit
.no_cabe:
        clc
.bit:
        rcl word [v_aux2], 1
        rcl word [v_aux2 + 2], 1
        rcl word [v_aux2 + 4], 1
        rcl word [v_aux2 + 6], 1
        shl word [v_aux], 1
        rcl word [v_aux + 2], 1
        rcl word [v_aux + 4], 1
        rcl word [v_aux + 6], 1
        mov dl, 0
        rcl dl, 1
        loop .v
        mov ax, [v_aux2]
        mov [di + AC_M0], ax
        mov ax, [v_aux2 + 2]
        mov [di + AC_M1], ax
        mov ax, [v_aux2 + 4]
        mov [di + AC_M2], ax
        mov ax, [v_aux2 + 6]
        mov [di + AC_M3], ax
        ; el cociente está entre 0,5 y 2: el primer bit vale 1
        ; (exp+1 ya puesto); si no está, se normaliza
        call f_normaliza
.x:
        pop dx
        pop cx
        pop bx
        pop ax
        ret

; ---- comparar DI con SI: AX = -1, 0 o 1 (DI menor, igual, mayor) ----
f_cmp:
        push bx
        mov bl, [di + AC_SIG]
        cmp word [di + AC_M3], 0
        jne .a_no0
        cmp word [si + AC_M3], 0
        je .igual
        ; 0 frente a otro: el signo del otro, al revés
        test byte [si + AC_SIG], 0x80
        jnz .mayor
        jmp .menor
.a_no0:
        cmp word [si + AC_M3], 0
        je .por_signo
        cmp bl, [si + AC_SIG]
        jne .por_signo
        ; mismo signo: por la magnitud
        mov ax, [di + AC_EXP]
        cmp ax, [si + AC_EXP]
        jne .mag
        mov ax, [di + AC_M3]
        cmp ax, [si + AC_M3]
        jne .mag_u
        mov ax, [di + AC_M2]
        cmp ax, [si + AC_M2]
        jne .mag_u
        mov ax, [di + AC_M1]
        cmp ax, [si + AC_M1]
        jne .mag_u
        mov ax, [di + AC_M0]
        cmp ax, [si + AC_M0]
        jne .mag_u
.igual:
        xor ax, ax
        pop bx
        ret
.mag:
        jg .mag_mayor
        jmp .mag_menor
.mag_u:
        ja .mag_mayor
.mag_menor:
        test bl, 0x80
        jnz .mayor
        jmp .menor
.mag_mayor:
        test bl, 0x80
        jnz .menor
        jmp .mayor
.por_signo:
        test bl, 0x80
        jnz .menor
.mayor:
        mov ax, 1
        pop bx
        ret
.menor:
        mov ax, -1
        pop bx
        ret

; ---- la parte entera (hacia cero) del de DI ----
f_trunca:
        push cx
        call f_es_cero
        jz .x
        mov cx, [di + AC_EXP]
        cmp cx, 0
        jg .algo
        mov al, [di + AC_TIPO]
        call f_cero
        mov [di + AC_TIPO], al
        jmp .x
.algo:
        cmp cx, 64
        jge .x
        ; borra los 64-exp bits de abajo
        push bx
        mov bx, AC_M0
        mov ax, 64
        sub ax, cx                  ; los que se borran
.w:
        cmp ax, 16
        jb .b
        mov word [di + bx], 0
        add bx, 2
        sub ax, 16
        jmp .w
.b:
        mov cl, al
        mov ax, 0xFFFF
        shl ax, cl
        and [di + bx], ax
        pop bx
.x:
        pop cx
        ret

; ---- ¿tiene decimales el de DI? ZF=0 si los tiene ----
f_tiene_dec:
        push si
        push di
        mov si, di
        mov di, ac3
        call f_copia
        call f_trunca
        call f_cmp
        test ax, ax
        pop di
        pop si
        ret

; ---- INT: el entero de debajo (el de DI) ----
f_int:
        test byte [di + AC_SIG], 0x80
        jz f_trunca
        call f_tiene_dec
        pushf
        call f_trunca
        popf
        jz .x
        push si
        mov si, f_uno_cte
        call f_carga_cte_aux        ; ac4 = 1
        mov si, ac4
        call f_resta
        pop si
.x:
        ret

; ---- el de DI a entero de 16 bits redondeando (CINT): AX. CF=1 si no cabe
f_a_entero:
        push bx
        push cx
        push dx
        call f_es_cero
        jz .cero
        mov cx, [di + AC_EXP]
        cmp cx, 0
        jl .cero
        cmp cx, 16
        jg .no
        ; el valor por 2^16 está en M3:M2 desplazado 16-exp
        mov ax, [di + AC_M3]
        mov dx, [di + AC_M2]
        neg cx
        add cx, 16                  ; los que hay que bajar
        jz .ya
.d:
        shr ax, 1
        rcr dx, 1
        loop .d
.ya:
        ; redondeo: el bit alto de DX es el medio
        shl dx, 1
        adc ax, 0
        jc .no
        test byte [di + AC_SIG], 0x80
        jz .pos
        cmp ax, 0x8000
        ja .no
        neg ax
        jmp .si
.pos:
        cmp ax, 0x7FFF
        ja .no
.si:
        clc
        pop dx
        pop cx
        pop bx
        ret
.cero:
        ; menos de 1: redondea a 0 o a ±1
        call f_es_cero
        jz .c0
        cmp word [di + AC_EXP], 0
        jne .c0
        mov ax, 1                   ; 0,5 <= x < 1
        test byte [di + AC_SIG], 0x80
        jz .si
        neg ax
        jmp .si
.c0:
        xor ax, ax
        jmp .si
.no:
        stc
        pop dx
        pop cx
        pop bx
        ret

; ---- las constantes: en el código, en MBF doble. f_carga_cte_aux pone en
; ac4 la de CS:SI ----
f_carga_cte_aux:
        push di
        mov di, ac4
        call f_carga_cte
        pop di
        ret
; pone en DI la constante de CS:SI (8 octetos MBF), de tipo doble
f_carga_cte:
        push ax                     ; (todo se conserva: f_pot10 lleva AX)
        push si
        push cx
        push di
        mov cx, 8
        mov di, v_aux
.c:
        mov al, [cs:si]
        mov [di], al
        inc si
        inc di
        loop .c
        pop di
        mov si, v_aux
        call mbf8_a
        pop cx
        pop si
        pop ax
        ret
f_uno_cte   db 0, 0, 0, 0, 0, 0, 0x00, 0x81       ; 1
f_diez_cte  db 0, 0, 0, 0, 0, 0, 0x20, 0x84       ; 10
f_medio_cte db 0, 0, 0, 0, 0, 0, 0x00, 0x80       ; 0,5

; ---- de MBF a desempaquetado ----
; DS:SI = 4 octetos; al número de DI (tipo simple)
mbf4_a:
        push ax
        call f_cero
        mov byte [di + AC_TIPO], T_SIM
        mov al, [si + 3]
        test al, al
        jz .x
        xor ah, ah
        sub ax, 128
        mov [di + AC_EXP], ax
        mov al, [si + 2]
        and al, 0x80
        mov [di + AC_SIG], al
        mov ah, [si + 2]
        or ah, 0x80
        mov al, [si + 1]
        mov [di + AC_M3], ax
        mov ah, [si]
        xor al, al
        mov [di + AC_M2], ax
.x:
        pop ax
        ret
; DS:SI = 8 octetos; al número de DI (tipo doble)
mbf8_a:
        push ax
        call f_cero
        mov byte [di + AC_TIPO], T_DOB
        mov al, [si + 7]
        test al, al
        jz .x
        xor ah, ah
        sub ax, 128
        mov [di + AC_EXP], ax
        mov al, [si + 6]
        and al, 0x80
        mov [di + AC_SIG], al
        mov ah, [si + 6]
        or ah, 0x80
        mov al, [si + 5]
        mov [di + AC_M3], ax
        mov ax, [si + 3]
        mov [di + AC_M2], ax
        mov ax, [si + 1]
        mov [di + AC_M1], ax
        mov ah, [si]
        xor al, al
        mov [di + AC_M0], ax
.x:
        pop ax
        ret
; los de siempre, con el acumulador
mbf4_a_ac:
        push di
        mov di, ac
        call mbf4_a
        pop di
        ret
mbf8_a_ac:
        push di
        mov di, ac
        call mbf8_a
        pop di
        ret

; ---- de desempaquetado (el de BX) a MBF en ES:DI (avanza DI) ----
; simple: 4 octetos. Redondea antes. Desbordamiento si no cabe
a_mbf4:
        push ax
        push cx
        mov cl, [bx + AC_TIPO]
        mov byte [bx + AC_TIPO], T_SIM
        push di
        mov di, bx
        call f_redondea
        pop di
        mov [bx + AC_TIPO], cl
        cmp word [bx + AC_M3], 0
        je .cero
        mov ax, [bx + AC_EXP]
        add ax, 128
        jle .cero                   ; tan pequeño que es 0
        cmp ax, 255
        jg .desb
        push ax
        mov al, [bx + AC_M2 + 1]
        stosb
        mov al, [bx + AC_M3]
        stosb
        mov al, [bx + AC_M3 + 1]
        and al, 0x7F
        or al, [bx + AC_SIG]
        stosb
        pop ax
        stosb
        pop cx
        pop ax
        ret
.cero:
        xor ax, ax
        stosw
        stosw
        pop cx
        pop ax
        ret
.desb:
        mov al, 6
        jmp error
; doble: 8 octetos
a_mbf8:
        push ax
        push cx
        mov cl, [bx + AC_TIPO]
        mov byte [bx + AC_TIPO], T_DOB
        push di
        mov di, bx
        call f_redondea
        pop di
        mov [bx + AC_TIPO], cl
        cmp word [bx + AC_M3], 0
        je .cero
        mov ax, [bx + AC_EXP]
        add ax, 128
        jle .cero
        cmp ax, 255
        jg a_mbf4.desb
        push ax
        mov al, [bx + AC_M0 + 1]
        stosb
        mov ax, [bx + AC_M1]
        stosw
        mov ax, [bx + AC_M2]
        stosw
        mov al, [bx + AC_M3]
        stosb
        mov al, [bx + AC_M3 + 1]
        and al, 0x7F
        or al, [bx + AC_SIG]
        stosb
        pop ax
        stosb
        pop cx
        pop ax
        ret
.cero:
        xor ax, ax
        stosw
        stosw
        stosw
        stosw
        pop cx
        pop ax
        ret
ac_a_mbf4:
        push bx
        mov bx, ac
        call a_mbf4
        pop bx
        ret
ac_a_mbf8:
        push bx
        mov bx, ac
        call a_mbf8
        pop bx
        ret

; ---- 10 elevado a CX (con signo) en el de DI, con toda la precisión ----
f_pot10:
        push ax
        push cx
        push si
        push di
        mov si, f_uno_cte
        call f_carga_cte            ; DI = 1
        mov ax, cx
        test ax, ax
        jns .pos
        neg ax
.pos:
        push di
        mov di, ac5
        mov si, f_diez_cte
        call f_carga_cte            ; ac5 = 10 (la base, que se va elevando)
        pop di
        mov si, ac5
.b:
        test ax, ax
        jz .fin
        shr ax, 1
        jnc .no_mul
        call f_mul                  ; DI *= base
.no_mul:
        test ax, ax
        jz .fin
        push di
        mov di, ac5
        call f_mul                  ; base *= base
        pop di
        jmp .b
.fin:
        pop di
        pop si
        pop cx
        pop ax
        ret

; =============================================================================
;  lee_numero: el número escrito en DS:SI, al acumulador (con su tipo).
;  SI queda detrás. Como GW-BASIC: entero si no lleva punto ni exponente y
;  cabe; doble con D, con «#» o con más de 7 cifras; si no, simple.
; =============================================================================
lee_numero:
        push bx
        push cx
        push dx
        push di
        mov di, ac
        call f_cero
        xor bx, bx                  ; BL: cifras que cuentan; BH: banderas
        xor cx, cx                  ; CX: el exponente decimal que se lleva
        ; BH: 1 hubo punto, 2 hubo exponente, 4 doble, 8 más de 19 cifras
.cifra:
        lodsb
        cmp al, '.'
        je .punto
        call es_cifra
        jnc .fin_man
        sub al, '0'
        ; ¿un cero de delante? no cuenta
        test bl, bl
        jnz .cuenta
        test al, al
        jnz .cuenta
        test bh, 1
        jz .cifra
        dec cx                      ; 0,00x: el punto se corre
        jmp .cifra
.cuenta:
        inc bl
        cmp bl, 19
        jbe .acum
        ; ya no caben: solo cuenta para el exponente
        test bh, 1
        jnz .cifra
        inc cx
        jmp .cifra
.acum:
        call man_por10_mas          ; mantisa = mantisa*10 + AL
        test bh, 1
        jz .cifra
        dec cx
        jmp .cifra
.punto:
        test bh, 1
        jnz .fin_man                ; un segundo punto acaba
        or bh, 1
        jmp .cifra
.fin_man:
        ; ¿exponente?
        call mayus
        cmp al, 'E'
        je .exp
        cmp al, 'D'
        jne .sufijo
        or bh, 4
.exp:
        ; detrás tiene que venir signo o cifra; si no, no es exponente
        mov dl, [si]
        cmp dl, '+'
        je .exp_ok
        cmp dl, '-'
        je .exp_ok
        cmp dl, '0'
        jb .no_exp
        cmp dl, '9'
        ja .no_exp
.exp_ok:
        or bh, 2
        xor dx, dx                  ; DX: el exponente
        push bx
        xor bl, bl                  ; BL: negativo
        lodsb
        cmp al, '-'
        jne .e_mas
        mov bl, 1
        jmp .e_c
.e_mas:
        cmp al, '+'
        je .e_c
        dec si
.e_c:
        lodsb
        call es_cifra
        jnc .e_fin
        sub al, '0'
        cbw
        xchg ax, dx
        push bx
        mov bx, 10
        push dx
        mul bx
        pop dx
        pop bx
        add dx, ax
        cmp dx, 1000
        jb .e_c
        mov dx, 1000
        jmp .e_c
.e_fin:
        dec si
        test bl, bl
        jz .e_pos
        neg dx
.e_pos:
        pop bx
        add cx, dx
        lodsb
        jmp .sufijo
.no_exp:
        and bh, ~4
.sufijo:
        ; AL = lo que sigue al número
        mov dl, T_ENT
        cmp al, '%'
        je .con_suf
        mov dl, T_SIM
        cmp al, '!'
        je .con_suf
        mov dl, T_DOB
        cmp al, '#'
        je .con_suf
        dec si                      ; no era sufijo
        ; el tipo, por cómo se escribió
        mov dl, T_DOB
        test bh, 4
        jnz .tipo
        cmp bl, 7
        ja .tipo
        mov dl, T_SIM
        test bh, 3
        jnz .tipo
        mov dl, T_ENT
        jmp .tipo
.con_suf:
.tipo:
        mov [di + AC_TIPO], dl
        ; la mantisa (entero de 64 bits) a número: exp 64 y normalizar
        mov word [di + AC_EXP], 64
        call f_normaliza
        ; por 10^CX
        jcxz .ya
        push di
        mov di, ac3
        push cx
        test cx, cx
        jns .p
        neg cx
.p:
        call f_pot10                ; ac3 = 10^|CX|
        pop cx
        pop di
        push si
        mov si, ac3
        test cx, cx
        js .div
        call f_mul
        jmp .p_ya
.div:
        call f_div
.p_ya:
        pop si
.ya:
        cmp byte [di + AC_TIPO], T_ENT
        jne .redondea
        ; ¿cabe en un entero?
        push ax
        call f_tiene_dec
        jnz .ent_no
        call f_a_entero
        jc .ent_no
        mov [di + AC_M0], ax
        pop ax
        jmp .x
.ent_no:
        pop ax
        mov byte [di + AC_TIPO], T_SIM
        test bh, 3                  ; con «%» no puede ser otra cosa
        jnz .redondea
        cmp bl, 7
        jbe .redondea
        mov byte [di + AC_TIPO], T_DOB
.redondea:
        call f_redondea
.x:
        pop di
        pop dx
        pop cx
        pop bx
        ret

; mantisa (de DI, como entero de 64 bits) * 10 + AL
man_por10_mas:
        push ax
        push bx
        push cx
        push dx
        xor ah, ah
        mov cx, ax                  ; lo que se suma (y lo que se lleva)
        mov bx, AC_M0
.c:
        mov ax, [di + bx]
        mov dx, 10
        mul dx
        add ax, cx
        adc dx, 0
        mov [di + bx], ax
        mov cx, dx
        add bx, 2
        cmp bx, AC_M3 + 2
        jb .c
        pop dx
        pop cx
        pop bx
        pop ax
        ret

; =============================================================================
;  num_a_texto: el acumulador a texto en b_num, como lo escribe PRINT (con
;  espacio o «-» delante y nada detrás). Guarda los registros.
; =============================================================================
num_a_texto:
        push ax
        push bx
        push cx
        push dx
        push si
        push di
        mov di, b_num
        cmp byte [ac + AC_TIPO], T_ENT
        jne .flot
        mov ax, [ac + AC_M0]
        mov byte [di], ' '
        test ax, ax
        jns .ent
        mov byte [di], '-'
        neg ax
.ent:
        inc di
        call texto_natural
        jmp .x
.flot:
        mov byte [di], ' '
        test byte [ac + AC_SIG], 0x80
        jz .f1
        cmp word [ac + AC_M3], 0
        je .f1
        mov byte [di], '-'
.f1:
        inc di
        cmp word [ac + AC_M3], 0
        jne .no_cero
        mov ax, '0'
        stosw
        jmp .x
.no_cero:
        ; las cifras que se escriben: 7 o 16
        mov dx, 7
        cmp byte [ac + AC_TIPO], T_DOB
        jne .d
        mov dx, 16
.d:
        mov [v_aux + 8], dx         ; D
        push di
        ; ac2 = |ac|
        mov si, ac
        mov di, ac2
        call f_copia
        mov byte [ac2 + AC_SIG], 0
        ; e10 aproximado: (exp-1) * log10(2)
        mov ax, [ac2 + AC_EXP]
        dec ax
        mov bx, 19728               ; log10(2) * 65536
        imul bx                     ; DX:AX = (exp-1) * 19728
        mov [v_aux + 10], dx        ; e10 (redondeado hacia abajo)
.escala:
        ; ac3 = 10^(D - 1 - e10); y = ac2 * o / ac3
        mov cx, [v_aux + 8]
        dec cx
        sub cx, [v_aux + 10]
        mov di, ac4
        mov si, ac2
        call f_copia
        mov di, ac3
        push cx
        test cx, cx
        jns .e1
        neg cx
.e1:
        call f_pot10
        pop cx
        mov di, ac4
        mov si, ac3
        test cx, cx
        js .e_div
        call f_mul
        jmp .e_ya
.e_div:
        call f_div
.e_ya:
        ; ac4 + 0,5, truncado: el entero de D cifras (64 bits)
        mov si, f_medio_cte
        mov di, ac3
        call f_carga_cte
        mov di, ac4
        mov si, ac3
        call f_suma
        call f_a_u64                ; v_aux2 = entero de 64 bits de ac4
        ; ¿tiene D cifras? comparar con 10^(D-1) y 10^D
        call u64_cifras             ; CX = cuántas cifras tiene
        cmp cx, [v_aux + 8]
        je .cifras_ok
        jb .pocas
        inc word [v_aux + 10]
        jmp .escala
.pocas:
        dec word [v_aux + 10]
        jmp .escala
.cifras_ok:
        ; las cifras, en v_aux2+16.. (texto); quita los ceros de detrás
        call u64_texto              ; b_lin+200.. con las D cifras, ASCIZ
        pop di
        mov si, b_cifras
        mov cx, [v_aux + 8]         ; nd
.quita:
        mov bx, cx
        cmp byte [si + bx - 1], '0'
        jne .q_ya
        dec cx
        cmp cx, 1
        ja .quita
.q_ya:
        mov bx, [v_aux + 10]        ; e10
        ; ¿normal o con exponente?
        cmp bx, [v_aux + 8]
        jge .con_exp
        test bx, bx
        jge .normal
        mov ax, bx
        neg ax
        dec ax                      ; los ceros tras el punto
        add ax, cx
        cmp ax, [v_aux + 8]
        jg .con_exp
        ; .000ddd
        mov al, '.'
        stosb
        mov ax, bx
        neg ax
        dec ax
        push cx
        mov cx, ax
        mov al, '0'
        rep stosb
        pop cx
        rep movsb
        mov byte [di], 0
        jmp .x
.normal:
        ; parte entera: e10+1 cifras
        inc bx
.n1:
        test bx, bx
        jz .n_punto
        test cx, cx
        jz .n_cero
        movsb
        dec cx
        dec bx
        jmp .n1
.n_cero:
        mov al, '0'
        stosb
        dec bx
        jmp .n1
.n_punto:
        jcxz .n_fin
        mov al, '.'
        stosb
        rep movsb
.n_fin:
        mov byte [di], 0
        jmp .x
.con_exp:
        movsb
        dec cx
        jz .ce1
        mov al, '.'
        stosb
        rep movsb
.ce1:
        mov al, 'E'
        cmp byte [ac + AC_TIPO], T_DOB
        jne .ce2
        mov al, 'D'
.ce2:
        stosb
        mov al, '+'
        mov ax, [v_aux + 10]
        test ax, ax
        mov byte [di], '+'
        jns .ce3
        mov byte [di], '-'
        neg ax
.ce3:
        inc di
        cmp ax, 10
        jae .ce4
        mov byte [di], '0'
        inc di
.ce4:
        call texto_natural
.x:
        pop di
        pop si
        pop dx
        pop cx
        pop bx
        pop ax
        ret

; AX (sin signo) en decimal en ES:DI, ASCIZ (DI queda en el 0)
texto_natural:
        push ax
        push bx
        push cx
        push dx
        mov bx, 10
        xor cx, cx
.d:
        xor dx, dx
        div bx
        push dx
        inc cx
        test ax, ax
        jnz .d
.p:
        pop ax
        add al, '0'
        stosb
        loop .p
        mov byte [di], 0
        pop dx
        pop cx
        pop bx
        pop ax
        ret

; el de ac4 (positivo, menos de 2^64) a entero de 64 bits en v_aux2 (trunca)
f_a_u64:
        push cx
        push si
        push di
        mov si, ac4
        mov di, v_aux2 - AC_M0      ; (para usar f_dcha sobre v_aux2)
        mov ax, [si + AC_M0]
        mov [v_aux2], ax
        mov ax, [si + AC_M1]
        mov [v_aux2 + 2], ax
        mov ax, [si + AC_M2]
        mov [v_aux2 + 4], ax
        mov ax, [si + AC_M3]
        mov [v_aux2 + 6], ax
        mov cx, 64
        sub cx, [si + AC_EXP]
        jle .x
        cmp cx, 64
        jbe .d
        mov cx, 64
.d:
        call f_dcha
.x:
        pop di
        pop si
        pop cx
        ret

; cuántas cifras decimales tiene el entero de v_aux2: CX
u64_cifras:
        call u64_texto
        push di
        mov di, b_cifras
        xor cx, cx
.c:
        cmp byte [di], 0
        je .x
        inc cx
        inc di
        jmp .c
.x:
        pop di
        ret

; el entero de 64 bits de v_aux2 en decimal en b_lin+200 (ASCIZ); no lo toca
u64_texto:
        push ax
        push bx
        push cx
        push dx
        push si
        push di
        ; copia en v_aux+16.. (4 palabras) para irlo dividiendo
        mov si, v_aux2
        mov di, v_aux + 0
        mov cx, 4
        rep movsw
        xor cx, cx                  ; cifras
.d:
        ; v_aux / 10, resto en DX
        xor dx, dx
        mov bx, 10
        mov ax, [v_aux + 6]
        div bx
        mov [v_aux + 6], ax
        mov ax, [v_aux + 4]
        div bx
        mov [v_aux + 4], ax
        mov ax, [v_aux + 2]
        div bx
        mov [v_aux + 2], ax
        mov ax, [v_aux]
        div bx
        mov [v_aux], ax
        push dx
        inc cx
        mov ax, [v_aux]
        or ax, [v_aux + 2]
        or ax, [v_aux + 4]
        or ax, [v_aux + 6]
        jnz .d
        mov di, b_cifras
.p:
        pop ax
        add al, '0'
        stosb
        loop .p
        mov byte [di], 0
        pop di
        pop si
        pop dx
        pop cx
        pop bx
        pop ax
        ret
