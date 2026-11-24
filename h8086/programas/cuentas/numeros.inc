; =============================================================================
;  CUENTAS - los números: coma fija de 64 bits, del texto al número y al revés,
;  y las fechas
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Un número es un entero de 64 bits con signo que cuenta cienmillonésimas
;  (ocho decimales), como en Cuentas de Hispa Ventanas: 0,1 es 10.000.000 y
;  0,1 + 0,2 da 0,3 exacto, porque no hay coma flotante. Sumar y restar es
;  exacto; multiplicar y dividir se hace con 128 bits y se redondea a la
;  cienmillonésima más cercana (la mitad, lejos del cero). Caben unos
;  ±92.230 millones.
;
;  Los errores viajan como un número más: la palabra alta vale 8000h y la
;  baja dice cuál (E_...). Lo que caería ahí es desbordamiento.
; =============================================================================

E_ERROR     equ 1                   ; fórmula mal escrita
E_DIV0      equ 2                   ; división por cero
E_CIRC      equ 3                   ; referencia circular
E_DESB      equ 4                   ; no cabe
E_REF       equ 5                   ; referencia fuera de la hoja
E_VALOR     equ 6                   ; argumento que no vale

; ---- [SI] (CX palabras) por BX; DX = lo que sobra por arriba ----
muln:
        push si
        push cx
        push di
        xor di, di
.m:
        mov ax, [si]
        mul bx
        add ax, di
        adc dx, 0
        mov [si], ax
        mov di, dx
        inc si
        inc si
        loop .m
        mov dx, di
        pop di
        pop cx
        pop si
        ret

; ---- [SI] (CX palabras) entre BX; DX = el resto ----
divn:
        push si
        push cx
        add si, cx
        add si, cx
        xor dx, dx
.d:
        dec si
        dec si
        mov ax, [si]
        div bx
        mov [si], ax
        loop .d
        pop cx
        pop si
        ret

; ---- [SI] = -[SI] (64 bits) ----
neg64:
        push si
        push cx
        mov cx, 4
        clc
.n:
        mov ax, 0
        sbb ax, [si]
        mov [si], ax
        inc si
        inc si
        loop .n
        pop cx
        pop si
        ret

; ---- [SI] a positivo; si era negativo, cambia el bit 0 de BH ----
absg:
        test byte [si+7], 0x80
        jz .r
        call neg64
        xor bh, 1
.r:
        ret

; ---- acc = el error AL ----
pon_err:
        xor ah, ah
        mov [acc], ax
        xor ax, ax
        mov [acc+2], ax
        mov [acc+4], ax
        mov word [acc+6], 0x8000
        ret

; ---- acc = AX (entero, con signo) ----
pon_ent:
        push ax
        test ax, ax
        jns .p
        neg ax
.p:
        mov [acc], ax
        xor ax, ax
        mov [acc+2], ax
        mov [acc+4], ax
        mov [acc+6], ax
        mov si, acc
        mov cx, 4
        mov bx, 10000
        call muln
        call muln
        pop ax
        test ax, ax
        jns .r
        jmp neg64
.r:
        ret

acc_cero:
        xor ax, ax
        mov [acc], ax
        mov [acc+2], ax
        mov [acc+4], ax
        mov [acc+6], ax
        ret

; ---- ¿es cero acc? (ZF) ----
es_cero:
        mov ax, [acc]
        or ax, [acc+2]
        or ax, [acc+4]
        or ax, [acc+6]
        ret

; ---- si opb o acc son un error, deja el error en acc y CF ----
err2:
        cmp word [opb+6], 0x8000
        jne .a
        mov si, opb
        mov di, acc
        call copia8
        stc
        ret
.a:
        cmp word [acc+6], 0x8000
        je .s
        clc
        ret
.s:
        stc
        ret

; ---- [DI] = [SI], 8 octetos (en DS) ----
copia8:
        push cx
        push si
        push di
        mov cx, 4
.c:
        mov ax, [si]
        mov [di], ax
        inc si
        inc si
        inc di
        inc di
        loop .c
        pop di
        pop si
        pop cx
        ret

; ---- acc = opb + acc;  acc = opb - acc ----
resta:
        call err2
        jc ar_r
        mov si, acc
        call neg64
        jmp short suma0
suma:
        call err2
        jc ar_r
suma0:
        mov ax, [opb]
        add [acc], ax
        mov ax, [opb+2]
        adc [acc+2], ax
        mov ax, [opb+4]
        adc [acc+4], ax
        mov ax, [opb+6]
        adc [acc+6], ax
        jo desborda
        cmp word [acc+6], 0x8000
        je desborda
ar_r:
        ret
desborda:
        mov al, E_DESB
        jmp pon_err

; ---- acc = opb * acc ----
mul64:
        call err2
        jc ar_r
        xor bh, bh
        mov si, opb
        call absg
        mov si, acc
        call absg
        push bx
        mov di, prod
        xor ax, ax
        mov cx, 12
.z:
        mov [di], ax
        inc di
        inc di
        loop .z
        xor si, si                  ; SI = 2i
.i:
        xor di, di                  ; DI = 2j
        xor cx, cx                  ; CX = lo que se lleva
.j:
        mov ax, [opb+si]
        mul word [acc+di]
        add ax, cx
        adc dx, 0
        mov bx, si
        add bx, di
        add [prod+bx], ax
        adc dx, 0
        mov cx, dx
        inc di
        inc di
        cmp di, 8
        jb .j
        mov [prod+8+si], cx
        inc si
        inc si
        cmp si, 8
        jb .i
        mov si, prod                ; entre 10^8, en dos veces, y redondeado
        mov cx, 8
        mov bx, 10000
        call divn
        call divn
        cmp dx, 5000
        jb .n
        call inc128
.n:
        pop bx
        jmp short cabe128

; ---- prod = prod + 1 (128 bits) ----
inc128:
        mov si, prod
        mov cx, 8
        stc
.l:
        adc word [si], 0
        inc si
        inc si
        loop .l
        ret

; ---- si prod cabe, acc = prod con el signo de BH; si no, desbordamiento ----
cabe128:
        mov ax, [prod+8]
        or ax, [prod+10]
        or ax, [prod+12]
        or ax, [prod+14]
        jnz desborda
        cmp word [prod+6], 0x7FFF
        jae desborda
        mov si, prod
        mov di, acc
        call copia8
        test bh, 1
        jz .r
        mov si, acc
        jmp neg64
.r:
        ret

; ---- acc = opb / acc ----
div64:
        call err2
        jc ar_r
        call es_cero
        jnz .b
        mov al, E_DIV0
        jmp pon_err
.b:
        push bp
        xor bh, bh
        mov si, opb
        call absg
        mov si, acc
        call absg
        push bx
        mov si, opb                 ; prod = |opb| * 10^8 (y el resto, a cero)
        mov di, prod
        call copia8
        xor ax, ax
        mov cx, 8
        mov di, prod+8
.z:
        mov [di], ax
        inc di
        inc di
        loop .z
        mov si, prod
        mov cx, 8
        mov bx, 10000
        call muln
        call muln
        mov bp, 128                 ; las vueltas: las palabras de arriba a
.s0:                                ; cero se saltan de golpe
        cmp word [prod+14], 0
        jne .l
        mov si, prod+12
        mov di, prod+14
        mov cx, 7
        std
.s1:
        mov ax, [si]
        mov [di], ax
        dec si
        dec si
        dec di
        dec di
        loop .s1
        cld
        mov word [prod], 0
        sub bp, 16
        jnz .s0
        jmp short .fin
.l:
        mov si, prod                ; prod y el resto (que va detrás), un
        mov cx, 12                  ; puesto a la izquierda
        clc
.r1:
        rcl word [si], 1
        inc si
        inc si
        loop .r1
        call resto_cmp              ; ¿resto >= divisor?
        jb .no
        call resto_sub
        or byte [prod], 1
.no:
        dec bp
        jnz .l
.fin:
        mov si, resto               ; redondeo: ¿2 * resto >= divisor?
        mov cx, 4
        clc
.d2:
        rcl word [si], 1
        inc si
        inc si
        loop .d2
        jc .sube
        call resto_cmp
        jb .q
.sube:
        call inc128
.q:
        pop bx
        pop bp
        jmp cabe128
; resto (64 bits) frente a acc, sin signo
resto_cmp:
        mov bx, 6
.c:
        mov ax, [resto+bx]
        cmp ax, [acc+bx]
        jne .x
        sub bx, 2
        jnc .c
        xor ax, ax                  ; iguales: CF = 0
.x:
        ret
resto_sub:
        mov si, resto
        xor bx, bx
        mov cx, 4
        clc
.s:
        mov ax, [acc+bx]
        sbb [si+bx], ax
        inc bx
        inc bx
        loop .s
        ret

; ---- acc = acc / BX (con signo, redondeado) ----
divr:
        cmp word [acc+6], 0x8000
        je .r
        push bx
        xor bh, bh
        mov si, acc
        call absg
        mov dl, bh
        pop bx
        push dx
        mov cx, 4
        call divn
        shl dx, 1
        cmp dx, bx
        jb .s
        add word [acc], 1
        adc word [acc+2], 0
        adc word [acc+4], 0
        adc word [acc+6], 0
.s:
        pop dx
        test dl, 1
        jz .r
        jmp neg64
.r:
        ret

; ---- compara [SI] con [DI] (con signo): AL = -1, 0 o 1 ----
cmp64:
        mov ax, [si+6]
        cmp ax, [di+6]
        jl .m
        jg .p
        mov bx, 4
.l:
        mov ax, [si+bx]
        cmp ax, [di+bx]
        jb .m
        ja .p
        sub bx, 2
        jnc .l
        xor al, al
        ret
.m:
        mov al, -1
        ret
.p:
        mov al, 1
        ret

; ---- acc = opb (op AH) acc: 1 =, 2 <>, 3 <, 4 >, 5 <=, 6 >=; da 1 o 0 ----
compara:
        push ax
        call err2
        pop ax
        jc .r
        push ax
        mov si, opb
        mov di, acc
        call cmp64
        pop bx                      ; BH = la operación
        mov bl, bh
        xor bh, bh
        mov ah, [bx + tabla_cmp - 1]
        inc al                      ; AL = 0 menor, 1 igual, 2 mayor
        mov cl, al
        shr ah, cl
        and ah, 1
        mov al, ah
        cbw
        jmp pon_ent
.r:
        ret
; por cada operación, qué resultados la cumplen: bit 0 menor, 1 igual, 2 mayor
tabla_cmp   db 2, 5, 1, 4, 3, 6

; ---- acc = opb ^ acc (exponente entero) ----
potencia:
        call err2
        jc .r
        mov si, acc                 ; el exponente, entero y pequeño
        mov di, prod
        call copia8
        xor bh, bh
        mov si, prod
        call absg
        push bx
        mov cx, 4
        mov bx, 10000
        call divn
        mov di, dx
        call divn
        or di, dx
        jnz .valor
        mov ax, [prod+2]
        or ax, [prod+4]
        or ax, [prod+6]
        jnz .desb
        mov cx, [prod]
        cmp cx, 64
        ja .desb
        mov si, opb                 ; la base, a salvo
        mov di, base
        call copia8
        push cx
        mov ax, 1
        call pon_ent
        pop cx
        jcxz .neg
.m:
        push cx
        mov si, base
        mov di, opb
        call copia8
        call mul64
        pop cx
        cmp word [acc+6], 0x8000
        je .neg
        loop .m
.neg:
        pop bx
        test bh, 1
        jz .r
        cmp word [acc+6], 0x8000
        je .r
        mov si, acc                 ; exponente negativo: 1 / x^n
        mov di, prod2
        call copia8
        mov ax, 1
        call pon_ent
        mov si, acc
        mov di, opb
        call copia8
        mov si, prod2
        mov di, acc
        call copia8
        jmp div64
.r:
        ret
.valor:
        pop bx
        mov al, E_VALOR
        jmp pon_err
.desb:
        pop bx
        jmp desborda

; ---- acc = acc redondeado a AX decimales (de -10 a 8) ----
redondea:
        cmp word [acc+6], 0x8000
        je .r
        cmp ax, 8
        jge .r
        cmp ax, -10
        jge .b
        mov ax, -10
.b:
        mov cx, 8
        sub cx, ax                  ; CX = cifras que se quitan (1 a 18)
        push cx
        xor bh, bh
        mov si, acc
        call absg
        mov [signo], bh
        dec cx
        jz .u
.t:
        push cx
        mov cx, 4
        mov bx, 10
        call divn
        pop cx
        loop .t
.u:
        mov bx, 10
        call divr
        pop cx
.m:
        push cx
        mov cx, 4
        mov bx, 10
        mov si, acc
        call muln
        pop cx
        loop .m
        cmp word [acc+6], 0x7FFF
        jae .d
        test byte [signo], 1
        jz .r
        mov si, acc
        jmp neg64
.r:
        ret
.d:
        jmp desborda

; ---- acc = la parte entera de acc, hacia abajo (ENTERO) ----
entero:
        cmp word [acc+6], 0x8000
        je .r
        xor bh, bh
        mov si, acc
        call absg
        mov [signo], bh
        mov cx, 4
        mov bx, 10000
        call divn
        mov di, dx
        call divn
        or di, dx                   ; ¿tenía decimales?
        jz .m
        test byte [signo], 1        ; negativo con decimales: uno más abajo
        jz .m
        add word [acc], 1
        adc word [acc+2], 0
        adc word [acc+4], 0
        adc word [acc+6], 0
.m:
        call muln
        call muln
        test byte [signo], 1
        jz .r
        jmp neg64
.r:
        ret

; ---- acc = |acc| ----
absoluto:
        cmp word [acc+6], 0x8000
        je .r
        mov si, acc
        jmp absg
.r:
        ret

; =============================================================================
;  Del texto al número. SI = el texto; nmodo: 0 lo que se teclea en una celda
;  (coma decimal; el punto separa los miles si le siguen tres cifras), 1 en una
;  fórmula de las de hoy (coma o punto decimal), 2 en una fórmula clásica
;  (punto decimal: la coma separa argumentos). Deja el número en acc y SI
;  detrás; CF si allí no hay un número (SI no se mueve). Si no cabe, acc es el
;  error de desbordamiento.
; =============================================================================
lee_num:
        push si
        call acc_cero
        mov byte [nd_ent], 0        ; cifras leídas
        mov byte [nd_dec], 0xFF     ; decimales (FFh: aún no hay coma)
        mov byte [n_red], 0         ; la cifra que decide el redondeo
        mov byte [n_desb], 0
.l:
        mov al, [si]
        cmp al, '0'
        jb .sep
        cmp al, '9'
        ja .sep
        inc si
        sub al, '0'
        inc byte [nd_ent]
        mov ah, [nd_dec]
        cmp ah, 0xFF
        je .cifra
        cmp ah, 8
        jb .dec
        jne .l                      ; (de la décima en adelante, nada)
        mov [n_red], al             ; la novena decide el redondeo
        inc byte [nd_dec]
        jmp .l
.dec:
        inc byte [nd_dec]
.cifra:
        xor ah, ah
        push ax
        push si
        mov si, acc
        mov cx, 4
        mov bx, 10
        call muln
        pop si
        pop ax
        or dx, dx
        jnz .db
        add [acc], ax
        adc word [acc+2], 0
        adc word [acc+4], 0
        adc word [acc+6], 0
        jno .l
.db:
        mov byte [n_desb], 1
        jmp .l
.sep:
        cmp byte [nd_dec], 0xFF
        jne .fin                    ; (ya hubo coma)
        cmp al, ','
        je .coma
        cmp al, '.'
        jne .fin
        cmp byte [nmodo], 0
        jne .punto_dec
        ; al teclear: ¿punto de los miles? (tres cifras detrás y no más)
        call es_cero
        jz .punto_dec
        push si
        mov cx, 3
.p3:
        inc si
        mov al, [si]
        sub al, '0'
        cmp al, 9
        ja .p_no
        loop .p3
        mov al, [si+1]
        sub al, '0'
        cmp al, 9
        jbe .p_no
        pop si
        inc si                      ; es de los miles: se salta
        jmp .l
.p_no:
        pop si
.punto_dec:
        jmp short .coma1
.coma:
        cmp byte [nmodo], 2         ; en las clásicas, la coma separa
        je .fin
.coma1:
        mov al, [si+1]              ; detrás tiene que venir una cifra (o
        sub al, '0'                 ; delante haber alguna)
        cmp al, 9
        jbe .c2
        cmp byte [nd_ent], 0
        je .fin
.c2:
        inc si
        mov byte [nd_dec], 0
        jmp .l
.fin:
        cmp byte [nd_ent], 0
        je .nada
        mov cl, [nd_dec]            ; a cienmillonésimas
        cmp cl, 0xFF
        jne .f1
        xor cl, cl
.f1:
        cmp cl, 8
        ja .redondeo
.f2:
        cmp cl, 8
        jae .ok
        push cx
        push si
        mov si, acc
        mov cx, 4
        mov bx, 10
        call muln
        pop si
        pop cx
        or dx, dx
        jz .f3
        mov byte [n_desb], 1
.f3:
        inc cl
        jmp .f2
.redondeo:
        cmp byte [n_red], 5
        jb .ok
        add word [acc], 1
        adc word [acc+2], 0
        adc word [acc+4], 0
        adc word [acc+6], 0
.ok:
        cmp byte [n_desb], 0
        jne .desb
        cmp word [acc+6], 0x7FFF
        jae .desb
        pop ax                      ; (el SI del principio)
        clc
        ret
.desb:
        call desborda
        pop ax
        clc
        ret
.nada:
        pop si
        stc
        ret

; =============================================================================
;  Del número al texto
; =============================================================================
; ---- las 20 cifras de |acc| en `cifras` (y BH bit 0 = negativo) ----
genera_cifras:
        push bp
        mov si, acc
        mov di, prod
        call copia8
        xor bh, bh
        mov si, prod
        call absg
        push bx
        mov di, cifras + 20
        mov bx, 10000
        mov bp, 5
.g:
        mov si, prod
        mov cx, 4
        call divn
        mov ax, dx
        mov cx, 4
        mov bx, 10
.c:
        xor dx, dx
        div bx
        add dl, '0'
        dec di
        mov [di], dl
        loop .c
        mov bx, 10000
        dec bp
        jnz .g
        pop bx
        pop bp
        ret

; ---- escribe acc en DI (DS) con CL decimales (FFh: los que hagan falta, hasta
; ocho; FEh: hasta seis, para la pantalla), CH = 1 con puntos de los miles, y la coma en la cifra `npunto` (12;
; 14 para los tantos por ciento, que son el número por cien). DI queda detrás.
num_txt:
        cmp word [acc+6], 0x8000
        jne .n
        mov si, [acc]               ; un error: su nombre
        shl si, 1
        mov si, [si + nom_err - 2]
        jmp copia_z
.n:
        push di
        push cx
        call genera_cifras
        pop cx
        pop di
        mov dl, 20
        sub dl, [npunto]            ; DL = decimales que hay
        mov dh, 0                   ; DH = 1: sin ceros al final
        cmp cl, 0xFE
        jb .fijo
        mov dh, 1
        mov al, cl
        mov cl, dl                  ; FFh: todos los que hay
        cmp al, 0xFF
        je .fijo
        mov cl, 6                   ; FEh: hasta seis (lo que se ve)
.fijo:
        cmp cl, dl
        jbe .f1
        mov cl, dl
.f1:
        mov al, [npunto]            ; redondeo a CL decimales
        add al, cl
        xor ah, ah
        mov si, ax
        add si, cifras
        cmp si, cifras + 20
        jae .quita
        cmp byte [si], '5'
        jb .quita
.inc:
        dec si
        cmp si, cifras
        jb .quita
        inc byte [si]
        cmp byte [si], '9'
        jbe .quita
        mov byte [si], '0'
        jmp .inc
.quita:
        test dh, dh                 ; los ceros del final, fuera
        jz .sin
        mov al, [npunto]
        add al, cl
        xor ah, ah
        mov si, ax
        add si, cifras - 1
.t:
        test cl, cl
        jz .sin
        cmp byte [si], '0'
        jne .sin
        dec si
        dec cl
        jmp .t
.sin:
        mov al, [npunto]            ; ¿todo ceros? entonces sin signo
        add al, cl
        xor ah, ah
        mov si, cifras
        mov dh, 0
.z:
        cmp byte [si], '0'
        je .z1
        mov dh, 1
.z1:
        inc si
        dec ax
        jnz .z
        and bh, dh
        test bh, 1
        jz .ent
        mov al, '-'
        stosb
.ent:
        mov si, cifras              ; la parte entera, sin ceros delante
        mov al, [npunto]
        xor ah, ah
        mov dx, ax                  ; DX = cifras enteras que quedan
.e0:
        cmp dx, 1
        je .e1
        cmp byte [si], '0'
        jne .e1
        inc si
        dec dx
        jmp .e0
.e1:
        mov ax, dx                  ; AH = cuántas hasta el próximo punto
        mov bl, 3
        div bl
        mov al, ah
        test al, al
        jnz .e2
        mov al, 3
.e2:
        mov ah, al
.e3:
        movsb
        dec dx
        jz .dec
        dec ah
        jnz .e3
        test ch, ch
        jz .e2b
        mov al, '.'
        stosb
.e2b:
        mov ah, 3
        jmp .e3
.dec:
        test cl, cl
        jz .r
        mov al, ','
        stosb
        mov si, cifras
        mov al, [npunto]
        xor ah, ah
        add si, ax
        xor ch, ch
        rep movsb
.r:
        ret

; ---- copia el texto de SI (acabado en 0) a DI, sin el 0 ----
copia_z:
        lodsb
        test al, al
        jz .r
        stosb
        jmp copia_z
.r:
        ret

; =============================================================================
;  Fechas: la serie cuenta días desde el 30-12-1899 (el 1-3-1900 es el 61, como
;  en las hojas de cálculo de hoy, pero sin el 29-2-1900 que no existió)
; =============================================================================
; ---- CF si el año AX es bisiesto ----
bisiesto:
        push ax
        push cx
        push dx
        push bx
        mov bx, ax
        test al, 3
        jnz .no
        xor dx, dx
        mov cx, 100
        div cx
        test dx, dx
        jnz .si
        mov ax, bx
        xor dx, dx
        mov cx, 400
        div cx
        test dx, dx
        jz .si
.no:
        clc
        jmp short .x
.si:
        stc
.x:
        pop bx
        pop dx
        pop cx
        pop ax
        ret

; ---- DX:AX = días del 1-1-1900 al 1-1 del año AX ----
dias_antes:
        push bx
        push cx
        mov bx, ax
        dec ax                      ; los bisiestos hasta el año anterior
        mov cx, ax
        shr ax, 1
        shr ax, 1                   ; /4
        push ax
        mov ax, cx
        xor dx, dx
        mov cx, 100
        div cx
        pop cx
        sub cx, ax                  ; - /100
        shr ax, 1
        shr ax, 1                   ; /400 = (/100)/4
        add cx, ax
        sub cx, 460                 ; (los de hasta 1899)
        mov ax, bx
        sub ax, 1900
        mov dx, 365
        mul dx
        add ax, cx
        adc dx, 0
        pop cx
        pop bx
        ret

; ---- AX = año, BL = mes, BH = día → acc = la serie; CF si la fecha no vale ----
fecha_serie:
        cmp ax, 1900
        jb .mal
        cmp ax, 9999
        ja .mal
        cmp bl, 1
        jb .mal
        cmp bl, 12
        ja .mal
        call dias_mes
        cmp bh, 1
        jb .mal
        cmp bh, cl
        ja .mal
        mov [f_ano], ax
        mov [f_mes], bx
        call dias_antes
        push dx
        push ax
        xor ax, ax                  ; los días de los meses de antes
        mov si, meses
        mov cl, [f_mes]
        xor ch, ch
        dec cx
        jz .m3
.m2:
        mov dl, [si]
        xor dh, dh
        add ax, dx
        inc si
        loop .m2
.m3:
        cmp byte [f_mes], 2         ; de marzo en adelante, el bisiesto
        jbe .m4
        push ax
        mov ax, [f_ano]
        call bisiesto
        pop ax
        jnc .m4
        inc ax
.m4:
        mov dl, [f_mes+1]           ; el día
        xor dh, dh
        add ax, dx
        inc ax                      ; (+2 de la serie, -1 del día 1)
        pop cx
        pop dx
        add ax, cx
        adc dx, 0
        mov [acc], ax
        mov [acc+2], dx
        xor ax, ax
        mov [acc+4], ax
        mov [acc+6], ax
        mov si, acc
        mov cx, 4
        mov bx, 10000
        call muln
        call muln
        clc
        ret
.mal:
        stc
        ret

; ---- CL = días del mes BL del año AX ----
dias_mes:
        push bx
        xor bh, bh
        mov cl, [bx + meses - 1]
        pop bx
        cmp bl, 2
        jne .r
        call bisiesto
        jnc .r
        inc cl
.r:
        ret
meses       db 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31

; ---- escribe en DI la fecha de la serie de acc (dd/mm/aaaa); CF si no vale ----
serie_fecha:
        cmp word [acc+6], 0
        jne .mal                    ; (negativa o error)
        push di
        mov si, acc
        mov di, prod
        call copia8
        mov si, prod
        mov cx, 4
        mov bx, 10000
        call divn
        call divn
        pop di
        mov ax, [prod]
        mov dx, [prod+2]
        sub ax, 2                   ; días desde el 1-1-1900
        sbb dx, 0
        jb .mal
        cmp dx, 45                  ; (hasta el año 9999)
        ja .mal
        mov [s_dias], ax
        mov [s_dias+2], dx
        mov cx, 366
        div cx
        add ax, 1900                ; un año que no se pasa
.a:
        push ax
        inc ax
        call dias_antes
        cmp dx, [s_dias+2]
        jb .sig
        ja .este
        cmp ax, [s_dias]
        ja .este
.sig:
        pop ax
        inc ax
        jmp .a
.este:
        pop ax
        push ax                     ; AX = el año
        call dias_antes
        mov cx, [s_dias]
        sub cx, ax                  ; CX = días dentro del año (desde 0)
        pop ax
        mov bl, 1
.mes:
        push cx
        call dias_mes
        mov dl, cl
        xor dh, dh
        pop cx
        cmp cx, dx
        jb .dia
        sub cx, dx
        inc bl
        jmp .mes
.dia:
        inc cl
        push ax
        mov al, cl
        call dos_cifras
        mov al, '/'
        stosb
        mov al, bl
        call dos_cifras
        mov al, '/'
        stosb
        pop ax
        mov bx, 1000                ; el año, cuatro cifras
        mov cx, 4
.y:
        xor dx, dx
        div bx
        add al, '0'
        stosb
        mov ax, dx
        push ax
        mov ax, bx
        mov bl, 10
        div bl
        xor ah, ah
        mov bx, ax
        pop ax
        loop .y
        clc
        ret
.mal:
        stc
        ret
dos_cifras:
        aam
        xchg al, ah
        add ax, '00'
        stosw
        ret

; ---- lee una fecha d/m/aaaa (o d-m-aaaa) de SI; acc = su serie; CF si no ----
lee_fecha:
        push si
        call ent_corto
        jc .mal
        mov [f_dia], al
        call sep_fecha
        jc .mal
        call ent_corto
        jc .mal
        mov [f_mes2], al
        call sep_fecha
        jc .mal
        call ent_corto
        jc .mal
        cmp ax, 100                 ; dos cifras: del 50 al 99, 19..; si no, 20..
        jae .y
        add ax, 1900
        cmp ax, 1950
        jae .y
        add ax, 100
.y:
        mov bl, [f_mes2]
        mov bh, [f_dia]
        call fecha_serie
        jc .mal
        pop ax
        clc
        ret
.mal:
        pop si
        stc
        ret
sep_fecha:
        lodsb
        cmp al, '/'
        je .s
        cmp al, '-'
        je .s
        stc
        ret
.s:
        clc
        ret
; AX = un entero de 1 a 4 cifras en SI; CF si no hay
ent_corto:
        xor ax, ax
        xor cx, cx
.l:
        mov bl, [si]
        sub bl, '0'
        cmp bl, 9
        ja .f
        inc si
        inc cx
        cmp cx, 4
        ja .mal
        xor bh, bh
        mov dx, 10
        mul dx
        add ax, bx
        jmp .l
.f:
        jcxz .mal
        clc
        ret
.mal:
        stc
        ret
