; =============================================================================
;  Hispa-BASIC - ON ERROR / RESUME y los archivos de datos (OPEN, CLOSE,
;  PRINT #, WRITE, INPUT #, LINE INPUT #, EOF, LOF, LOC)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Como en GW-BASIC (con /F:3): tres archivos, #1 a #3, de texto: INPUT
;  (leer), OUTPUT (escribir de nuevo) y APPEND (añadir). Cada uno con su
;  búfer de 256 octetos (v_arch, fichas de F_TAM).
; =============================================================================

; =============================================================================
;  ON ERROR GOTO n / RESUME
; =============================================================================
; (desde sent_on, tras el código de ERROR)
sent_on_error:
        call salta_esp
        cmp al, 0x89                ; GOTO
        jne err_sintaxis
        inc si
        call lee_linea_cte
        mov [v_onerr], ax
        test ax, ax
        jnz .x
        ; ON ERROR GOTO 0 dentro de la rutina: el error se da como siempre
        cmp byte [v_en_trampa], 0
        je .x
        mov byte [v_en_trampa], 0
        mov ax, [v_err]
        jmp error
.x:
        ret

; ---- el error AL, ¿lo atiende ON ERROR? Si es así no vuelve: salta a la
; rutina del programa. (Lo llama error al empezar) ----
error_trampa:
        cmp word [v_onerr], 0
        je .no
        cmp byte [v_en_trampa], 0
        jne .no
        cmp word [v_linea], 0xFFFF
        je .no
        xor ah, ah
        mov [v_err], ax
        mov bx, [v_sent_lin]
        mov [v_ultima], bx
        mov bx, [v_sent_ptr]
        mov [v_err_ptr], bx
        mov byte [v_en_trampa], 1
        mov word [v_temp_n], 0
        mov word [v_nom_n], 0
        call fin_print_arch
        mov ax, [v_onerr]
        call busca_linea
        jnc .no
        mov sp, [v_pila0]
        mov ax, ordenes_ok          ; (por si algo vuelve: a las órdenes)
        push ax
        lea si, [bx - 1]
        jmp ejecuta.sent
.no:
        ret

; ---- RESUME [0 | NEXT | línea] ----
sent_resume:
        cmp byte [v_en_trampa], 0
        je .sin_error
        mov byte [v_en_trampa], 0
        mov ax, [v_ultima]
        mov [v_linea], ax
        call salta_esp
        call fin_de_sent
        je .otra_vez
        cmp al, 0x83                ; NEXT
        je .siguiente
        call lee_linea_cte
        test ax, ax
        jz .otra_vez
        jmp va_a_linea
.otra_vez:                          ; la sentencia del error, otra vez
        mov si, [v_err_ptr]
        pop ax
        jmp ejecuta.sent
.siguiente:                         ; la que va detrás
        mov si, [v_err_ptr]
        jmp sent_rem_sent
.sin_error:
        mov al, 20
        jmp error

; =============================================================================
;  Los archivos
; =============================================================================
; ---- [#]n (1 a N_ARCH): BX = su ficha. Error 52 si no vale ----
lee_num_arch:
        call salta_esp
        cmp al, '#'
        jne .n
        inc si
.n:
        call eval_ent
ficha_de:                           ; AX = el número -> BX = la ficha
        test ax, ax
        jle .mal
        cmp ax, N_ARCH
        ja .mal
        dec ax
        mov bx, F_TAM
        mul bx
        add ax, v_arch
        mov bx, ax
        ret
.mal:
        mov al, 52
        jmp error

; ---- OPEN "archivo" FOR INPUT|OUTPUT|APPEND AS [#]n
;      OPEN "I"|"O"|"A", [#]n, "archivo" ----
sent_open:
        call eval_cad               ; BX = el primero (nombre o modo)
        call desc_a_arch            ; (a b_arch, por si es el nombre)
        mov dl, [b_arch]            ; (y su primera letra, por si es el modo)
        call salta_esp
        cmp al, ','
        je .corta
        ; ---- la forma larga ----
        cmp al, 0x82                ; FOR
        jne err_sintaxis
        inc si
        call salta_esp
        mov dl, 'I'
        cmp al, 0x85                ; INPUT
        jne .palabra
        inc si
        jmp .as
.palabra:
        call lee_nombre             ; OUTPUT, APPEND (no son palabras clave)
        mov dl, [b_nom + 1]
        cmp dl, 'O'
        je .as
        cmp dl, 'A'
        je .as
        mov al, 54                  ; RANDOM: no está (aún)
        jmp error
.as:
        call salta_esp
        call mayus
        cmp al, 'A'
        jne err_sintaxis
        mov al, [si + 1]
        call mayus
        cmp al, 'S'
        jne err_sintaxis
        add si, 2
        push dx
        call lee_num_arch           ; BX = la ficha
        pop dx
        jmp abre
.corta:
        inc si
        mov al, dl
        call mayus
        mov dl, al
        push dx
        call lee_num_arch
        push bx
        ESPERA ','
        call eval_cad
        call desc_a_arch
        pop bx
        pop dx
; DL = el modo ('I', 'O', 'A'), BX = la ficha, b_arch = el nombre
abre:
        cmp byte [bx + F_MODO], 0
        jne .ya
        push si
        mov word [bx + F_POS], 0
        mov word [bx + F_LEN], 0
        mov word [bx + F_COL], 0
        mov byte [bx + F_FIN], 0
        mov si, dx                  ; (SI = el modo, de paso)
        mov dx, b_arch
        cmp byte [b_arch], 0
        je .nombre_mal
        mov ax, si
        cmp al, 'I'
        je .leer
        cmp al, 'O'
        je .crear
        cmp al, 'A'
        jne .modo_mal
        ; añadir: abrirlo (o crearlo) y al final
        mov ax, 0x3D02
        int 0x21
        jnc .al_final
        xor cx, cx
        mov ah, 0x3C
        int 0x21
        jc .no_crea
        jmp .abierto
.al_final:
        ; al final; si lo último es el Ctrl+Z de GW-BASIC, encima de él
        push bx
        mov bx, ax
        mov ax, 0x4202
        mov cx, 0xFFFF
        mov dx, 0xFFFF              ; (un octeto antes del final)
        int 0x21
        jc .af_vacio
        push dx
        mov dx, b_num
        mov cx, 1
        mov ah, 0x3F
        int 0x21
        pop dx
        cmp byte [b_num], 0x1A
        jne .af_fin
        mov ax, 0x4201              ; volver un octeto: se escribe encima
        mov cx, 0xFFFF
        mov dx, 0xFFFF
        int 0x21
        jmp .af_x
.af_vacio:
.af_fin:
        mov ax, 0x4202
        xor cx, cx
        xor dx, dx
        int 0x21
.af_x:
        mov ax, bx
        pop bx
        jmp .abierto
.leer:
        mov ax, 0x3D00
        int 0x21
        jc .no_esta
        jmp .abierto
.crear:
        xor cx, cx
        mov ah, 0x3C
        int 0x21
        jc .no_crea
.abierto:
        mov [bx + F_HAN], ax
        mov ax, si
        mov [bx + F_MODO], al
        pop si
        ret
.ya:
        mov al, 55
        jmp error
.no_esta:
        pop si
        mov al, 53
        jmp error
.no_crea:
.nombre_mal:
        pop si
        mov al, 64
        jmp error
.modo_mal:
        pop si
        mov al, 54
        jmp error

; el texto del descriptor BX a b_arch (ASCIZ, en mayúsculas, hasta 60)
desc_a_arch:
        push si
        push cx
        mov cl, [bx]
        xor ch, ch
        cmp cx, 60
        jbe .c
        mov cx, 60
.c:
        mov si, [bx + 1]
        mov di, b_arch
        jcxz .f
.l:
        lodsb
        call mayus
        stosb
        loop .l
.f:
        mov byte [di], 0
        pop cx
        pop si
        ret

; ---- CLOSE [[#]n [, [#]n]...] ----
sent_close:
        call salta_esp
        call fin_de_sent
        jne close_lista
cierra_todo:
        push bx
        push ax
        mov ax, 1
.t:
        push ax
        call ficha_de
        call cierra_ficha
        pop ax
        inc ax
        cmp ax, N_ARCH
        jbe .t
        pop ax
        pop bx
        ret
close_lista:
        call lee_num_arch
        call cierra_ficha
        call salta_esp
        cmp al, ','
        jne .x
        inc si
        jmp close_lista
.x:
        ret
; cierra la ficha BX (si está abierta; vacía antes lo escrito)
cierra_ficha:
        cmp byte [bx + F_MODO], 0
        je .x
        cmp byte [bx + F_MODO], 'I'
        je .c
        ; como GW-BASIC: un Ctrl+Z al final de lo escrito
        push ax
        mov al, 0x1A
        push word [v_sal_f]
        mov [v_sal_f], bx
        call pon_car_f.pon
        pop word [v_sal_f]
        pop ax
        call vacia_ficha
.c:
        push bx
        mov bx, [bx + F_HAN]
        mov ah, 0x3E
        int 0x21
        pop bx
        mov byte [bx + F_MODO], 0
.x:
        ret
; escribe lo que haya en el búfer de la ficha BX
vacia_ficha:
        push ax
        push cx
        push dx
        mov cx, [bx + F_LEN]
        jcxz .x
        push bx
        lea dx, [bx + F_BUF]
        mov bx, [bx + F_HAN]
        mov ah, 0x40
        int 0x21
        pop bx
        jc .lleno
        cmp ax, cx
        jne .lleno
        mov word [bx + F_LEN], 0
.x:
        pop dx
        pop cx
        pop ax
        ret
.lleno:
        mov word [bx + F_LEN], 0
        mov al, 61
        jmp error

; ---- un carácter (AL) al archivo de PRINT # (v_sal_f) ----
pon_car_f:
        push bx
        mov bx, [v_sal_f]
        cmp al, 10
        jne .uno
        mov al, 13
        call .pon
        mov al, 10
        call .pon
        mov word [v_col], 0
        pop bx
        ret
.uno:
        call .pon
        inc word [v_col]
        pop bx
        ret
.pon:
        push di
        mov di, [bx + F_LEN]
        mov [bx + F_BUF + di], al
        inc di
        mov [bx + F_LEN], di
        pop di
        cmp word [bx + F_LEN], F_BUF_TAM
        jb .p_x
        call vacia_ficha
.p_x:
        ret

; ---- PRINT # y WRITE #: la salida pasa al archivo (con su columna) ----
; tras «#»: lee n y la coma; la salida va al archivo hasta fin_print_arch
empieza_print_arch:
        call lee_num_arch
        cmp byte [bx + F_MODO], 'O'
        je .bien
        cmp byte [bx + F_MODO], 'A'
        je .bien
        mov al, 54
        jmp error
.bien:
        push bx
        ESPERA ','
        pop bx
        mov [v_sal_f], bx
        mov ax, [v_col]
        mov [v_col_guard], ax
        mov ax, [v_ancho]
        mov [v_ancho_g], ax
        mov word [v_ancho], 255
        mov ax, [bx + F_COL]
        mov [v_col], ax
        ret
fin_print_arch:
        push bx
        push ax
        mov bx, [v_sal_f]
        test bx, bx
        jz .x
        mov ax, [v_col]
        mov [bx + F_COL], ax
        mov word [v_sal_f], 0
        mov ax, [v_col_guard]
        mov [v_col], ax
        mov ax, [v_ancho_g]
        mov [v_ancho], ax
.x:
        pop ax
        pop bx
        ret

; ---- WRITE [#n,] lista: las cadenas entre comillas, los números sin
; espacios, separados por comas ----
sent_write:
        call salta_esp
        cmp al, '#'
        jne .pant
        call empieza_print_arch
.pant:
        call salta_esp
        call fin_de_sent
        je .nl
.item:
        call evalua
        cmp byte [ac + AC_TIPO], T_CAD
        je .cad
        call num_a_texto
        push si
        mov si, b_num
        cmp byte [si], ' '
        jne .n1
        inc si
.n1:
        call pon_cad
        pop si
        jmp .sig
.cad:
        mov bx, [ac + AC_M0]
        call libera_temp
        mov al, '"'
        call pon_car
        call pon_desc
        mov al, '"'
        call pon_car
.sig:
        call salta_esp
        cmp al, ','
        jne .nl
        inc si
        mov al, ','
        call pon_car
        jmp .item
.nl:
        call pon_nl
        jmp fin_print_arch

; ---- leer del archivo de la ficha BX: un octeto (AL); CF=1 al final ----
lee_car_f:
        push di
        mov di, [bx + F_POS]
        cmp di, [bx + F_LEN]
        jb .hay
        push cx
        push dx
        push bx
        lea dx, [bx + F_BUF]
        mov cx, F_BUF_TAM
        mov bx, [bx + F_HAN]
        mov ah, 0x3F
        int 0x21
        pop bx
        pop dx
        pop cx
        jc .fin
        test ax, ax
        jz .fin
        mov [bx + F_LEN], ax
        xor di, di
.hay:
        mov al, [bx + F_BUF + di]
        cmp al, 0x1A                ; Ctrl+Z: el final
        je .fin
        inc di
        mov [bx + F_POS], di
        pop di
        clc
        ret
.fin:
        mov byte [bx + F_FIN], 1
        pop di
        stc
        ret
; ¿queda algo que leer? CF=1 si no (sin gastarlo)
mira_fin_f:
        call lee_car_f
        jc .x
        dec word [bx + F_POS]
        clc
.x:
        ret

; ---- una línea del archivo BX a b_lin (ASCIZ); CF=1 si ya no había ----
lee_linea_f:
        push di
        mov di, b_lin
        call lee_car_f
        jc .nada
        jmp .t
.c:
        call lee_car_f
        jc .ya
.t:
        cmp al, 13
        je .c
        cmp al, 10
        je .ya
        cmp di, b_lin + LIN_TAM - 1
        jae .c
        mov [di], al
        inc di
        jmp .c
.ya:
        mov byte [di], 0
        pop di
        clc
        ret
.nada:
        mov byte [b_lin], 0
        pop di
        stc
        ret

; ---- un dato del archivo BX (para INPUT #) a b_lin: hasta la coma o el
; final de la línea; entre comillas, hasta la otra comilla. DI = el texto,
; CX = su largo, DL = 1 si iba entre comillas ----
lee_dato_f:
        ; saltar espacios y saltos de línea de delante
.e:
        call lee_car_f
        jc .vacio
        cmp al, ' '
        je .e
        cmp al, 13
        je .e
        cmp al, 10
        je .e
        mov di, b_lin
        xor dl, dl
        cmp al, '"'
        jne .sin
        mov dl, 1
.com:
        call lee_car_f
        jc .fin
        cmp al, '"'
        je .tras_com
        call .guarda
        jmp .com
.tras_com:                          ; hasta la coma o el final de la línea
        call lee_car_f
        jc .fin
        cmp al, ','
        je .fin
        cmp al, 10
        je .fin
        jmp .tras_com
.sin:
        call .guarda
        call lee_car_f
        jc .fin_q
        cmp al, ','
        je .fin_q
        cmp al, 13
        je .cr
        cmp al, 10
        je .fin_q
        jmp .sin
.cr:                                ; tras el CR, el LF también es suyo
        call lee_car_f
        jc .fin_q
        cmp al, 10
        je .fin_q
        dec word [bx + F_POS]       ; (no lo era: se deja)
.fin_q:
        ; sin los espacios de detrás
.q:
        cmp di, b_lin
        je .fin
        cmp byte [di - 1], ' '
        jne .fin
        dec di
        jmp .q
.fin:
        mov cx, di
        sub cx, b_lin
        mov di, b_lin
        ret
.vacio:
        mov al, 62                  ; se ha leído más allá del final
        jmp error
.guarda:
        cmp di, b_lin + LIN_TAM - 1
        jae .g_x
        mov [di], al
        inc di
.g_x:
        ret

; ---- INPUT #n, lista ----
sent_input_arch:
        call lee_num_arch
        cmp byte [bx + F_MODO], 'I'
        jne .modo
        push bx
        ESPERA ','
        pop bx
.var:
        push bx
        mov al, 1
        call busca_var              ; BX = la variable, AL = el tipo
        mov cx, bx
        pop bx
        push bx
        push cx
        push ax
        call lee_dato_f             ; DI, CX, DL
        pop ax
        pop bx                      ; (la variable)
        push si
        call asigna_dato
        pop si
        pop bx                      ; (la ficha)
        jnc .bien
        mov al, 13                  ; no era un número
        jmp error
.bien:
        call salta_esp
        cmp al, ','
        jne .x
        inc si
        jmp .var
.x:
        ret
.modo:
        mov al, 54
        jmp error

; ---- LINE INPUT #n, a$ ----
sent_line_input_arch:
        call lee_num_arch
        cmp byte [bx + F_MODO], 'I'
        jne sent_input_arch.modo
        push bx
        ESPERA ','
        mov al, 1
        call busca_var
        cmp al, T_CAD
        jne err_tipos
        mov cx, bx
        pop bx
        push cx
        call lee_linea_f
        jnc .hay
        mov al, 62
        jmp error
.hay:
        push si
        mov di, b_lin
        call largo_asciz
        mov si, b_lin
        call cadena_de
        pop si
        pop bx
        mov al, T_CAD
        jmp guarda_valor

; ---- EOF(n), LOF(n), LOC(n) ----
fn_eof:
        call arg_ent
        call ficha_de
        cmp byte [bx + F_MODO], 'I'
        jne .no_fin
        call mira_fin_f
        jc .fin
.no_fin:
        xor ax, ax
        jmp ac_entero
.fin:
        mov ax, -1
        jmp ac_entero
fn_lof:
        call arg_ent
        call ficha_de
        cmp byte [bx + F_MODO], 0
        je .mal
        push bx
        mov bx, [bx + F_HAN]
        mov ax, 0x4201              ; dónde está
        xor cx, cx
        xor dx, dx
        int 0x21
        push dx
        push ax
        mov ax, 0x4202              ; el final: el tamaño
        xor cx, cx
        xor dx, dx
        int 0x21
        mov di, ax                  ; (DX:DI = el tamaño)
        pop ax                      ; volver a donde estaba
        pop cx
        push dx
        push di
        mov dx, ax
        mov ax, 0x4200
        int 0x21
        pop ax
        pop dx
        pop bx
        ; DX:AX a número
        mov di, ac
        push ax
        call f_cero
        pop ax
        mov byte [di + AC_TIPO], T_SIM
        mov [di + AC_M3], dx
        mov [di + AC_M2], ax
        mov word [di + AC_EXP], 32
        jmp f_normaliza
.mal:
        mov al, 52
        jmp error
fn_loc:
        call arg_ent
        call ficha_de
        ; (en un archivo de texto, GW-BASIC da los bloques de 128 octetos
        ; leídos o escritos; aquí, lo que lleva el búfer)
        mov ax, [bx + F_POS]
        jmp ac_entero
