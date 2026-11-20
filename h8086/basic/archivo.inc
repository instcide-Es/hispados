; =============================================================================
;  Hispa-BASIC - cargar y guardar programas
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  LOAD lee los dos formatos de GW-BASIC: el de texto (lo que guarda SAVE
;  "X",A, y los listados) y el binario (empieza por FFh: las líneas tal cual
;  están en la memoria, que es como las guarda Hispa-BASIC). SAVE guarda en
;  texto (el binario, en la 0.12); GW-BASIC lo carga igual.
; =============================================================================

; ---- el nombre de archivo (una expresión de cadena) a b_arch, ASCIZ, con
; .BAS si no lleva extensión ----
nombre_archivo:
        call eval_cad               ; BX
        push si
        mov cl, [bx]
        xor ch, ch
        jcxz .mal
        cmp cx, 60
        ja .mal
        mov si, [bx + 1]
        mov di, b_arch
        xor dx, dx                  ; DL: hay punto (tras la última barra)
.c:
        lodsb
        cmp al, '.'
        jne .c1
        mov dl, 1
.c1:
        cmp al, '\'
        jne .c2
        xor dl, dl
.c2:
        call mayus
        stosb
        loop .c
        test dl, dl
        jnz .ya
        mov ax, '.B'
        stosw
        mov ax, 'AS'
        stosw
.ya:
        xor al, al
        stosb
        pop si
        ret
.mal:
        mov al, 64
        jmp error

; ---- LOAD "archivo"[,R] ----
sent_load:
        mov byte [v_cargar_y_ejecutar], 0
sent_load_r:
        call nombre_archivo
        call salta_esp
        cmp al, ','
        jne .ya
        inc si
        call salta_esp
        cmp al, 'R'
        jne err_sintaxis
        inc si
        mov byte [v_cargar_y_ejecutar], 1
.ya:
        call cierra_todo
        ; (sigue)
; ---- carga b_arch (si no lo encuentra, prueba sin la extensión puesta) y,
; si v_cargar_y_ejecutar, lo ejecuta ----
carga_archivo:
        cmp byte [b_arch], 0
        jne .con
        ; desde la línea de órdenes: b_nom -> b_arch con .BAS
        mov si, b_nom
        mov di, b_arch
        xor dl, dl
.cp:
        lodsb
        call mayus
        cmp al, '.'
        jne .cp1
        mov dl, 1
.cp1:
        stosb
        test al, al
        jnz .cp
        test dl, dl
        jnz .con
        dec di
        mov ax, '.B'
        stosw
        mov ax, 'AS'
        stosw
        xor al, al
        stosb
.con:
        mov dx, b_arch
        mov ax, 0x3D00
        int 0x21
        jnc .abierto
        mov byte [b_arch], 0
        mov al, 53
        jmp error
.abierto:
        mov byte [b_arch], 0
        mov [v_fichero], ax
        mov word [v_dpos], 0
        mov word [v_dfin], 0
        call nuevo
        ; ¿binario?
        call lee_car_arch
        jc .fin
        cmp al, 0xFF
        je .binario
        dec word [v_dpos]           ; (se vuelve a leer)
.lineas:
        call lee_linea_arch         ; b_lin; CF=1 al final
        jc .fin
        mov si, b_lin
.e:
        lodsb
        cmp al, ' '
        je .e
        test al, al
        jz .lineas                  ; (una línea vacía, se salta)
        dec si
        call es_cifra
        jnc .directa
        call lee_num_linea
        jc .directa
        cmp byte [si], ' '
        jne .n1
        inc si
.n1:
        push ax
        call tokeniza
        pop ax
        call mete_linea
        jmp .lineas
.directa:
        call cierra_carga
        mov al, 66
        jmp error
.binario:
        ; todo el archivo a PROG_INI, y se rehacen los enlaces
        mov di, PROG_INI
.b:
        call lee_car_arch
        jc .b_fin
        cmp di, [v_cads]
        jae .grande
        stosb
        jmp .b
.b_fin:
        mov byte [di], 0
        mov byte [di + 1], 0
        mov byte [di + 2], 0
        call reenlaza_todo
.fin:
        call cierra_carga
        cmp byte [v_cargar_y_ejecutar], 0
        je ordenes_ok
        mov byte [v_cargar_y_ejecutar], 0
        call vacia
        mov si, PROG_INI - 1
        mov sp, [v_pila0]
        call ejecuta
        jmp ordenes_ok
.grande:
        call cierra_carga
        call nuevo
        mov al, 7
        jmp error

; rehace los enlaces de un programa recién cargado en binario: cada línea
; acaba en el 0 que no es parte de una constante
reenlaza_todo:
        mov bx, PROG_INI
.l:
        cmp word [bx], 0
        je .fin
        lea si, [bx + 4]
.c:
        lodsb
        test al, al
        jz .fin_l
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
        jz .fin_l
        cmp al, '"'
        jne .com
        jmp .c
.rem:
        lodsb
        test al, al
        jnz .rem
        jmp .fin_l
.data:
        call sent_data              ; hasta el «:» (o el 0), sin pasarlo
        jmp .c
.cte:
        call salta_cte
        jmp .c
.fin_l:
        mov [bx], si
        mov bx, si
        jmp .l
.fin:
        lea ax, [bx + 2]
        mov [v_vars], ax
        jmp vacia

; un octeto del archivo de LOAD: AL; CF=1 al final
lee_car_arch:
        push bx
        push cx
        push dx
        mov bx, [v_dpos]
        cmp bx, [v_dfin]
        jb .hay
        mov bx, [v_fichero]
        mov cx, 512
        mov dx, b_disco
        mov ah, 0x3F
        int 0x21
        jc .no
        test ax, ax
        jz .no
        mov [v_dfin], ax
        xor bx, bx
.hay:
        mov al, [b_disco + bx]
        inc bx
        mov [v_dpos], bx
        clc
        jmp .x
.no:
        stc
.x:
        pop dx
        pop cx
        pop bx
        ret
; una línea del archivo a b_lin (ASCIZ); CF=1 si ya no hay
lee_linea_arch:
        mov di, b_lin
.c:
        call lee_car_arch
        jc .fin
        cmp al, 0x1A
        je .fin
        cmp al, 13
        je .c
        cmp al, 10
        je .ya
        cmp di, b_lin + LIN_TAM - 1
        jae .c
        stosb
        jmp .c
.fin:
        cmp di, b_lin
        jne .ya
        stc
        ret
.ya:
        xor al, al
        stosb
        clc
        ret
cierra_carga:
        mov bx, [v_fichero]
        test bx, bx
        jz .x
        mov ah, 0x3E
        int 0x21
        mov word [v_fichero], 0
.x:
        ret

; ---- SAVE "archivo"[,A] (siempre en texto, por ahora) ----
sent_save:
        call nombre_archivo
        mov byte [v_aux2], 0        ; 1: en texto (,A)
        call salta_esp
        cmp al, ','
        jne .ya
        inc si
        call salta_esp
        call mayus
        cmp al, 'A'
        jne err_sintaxis
        inc si
        mov byte [v_aux2], 1
.ya:
        push si
        mov dx, b_arch
        xor cx, cx
        mov ah, 0x3C
        int 0x21
        jc .no
        cmp byte [v_aux2], 0
        jne .texto
        ; ---- en binario, como GW-BASIC: FFh, las líneas tal cual (los
        ; enlaces los rehace quien carga) y 1Ah ----
        mov bx, ax
        mov byte [b_disco], 0xFF
        mov dx, b_disco
        mov cx, 1
        mov ah, 0x40
        int 0x21
        jc .b_mal
        mov dx, PROG_INI
        mov cx, [v_vars]
        sub cx, dx
        mov ah, 0x40
        int 0x21
        jc .b_mal
        cmp ax, cx
        jne .b_mal
        mov byte [b_disco], 0x1A
        mov dx, b_disco
        mov cx, 1
        mov ah, 0x40
        int 0x21
        mov ah, 0x3E
        int 0x21
        pop si
        ret
.b_mal:
        mov ah, 0x3E
        int 0x21
        mov al, 61
        jmp error
.texto:
        mov [v_sal_h], ax
        mov word [v_sal_n], 0
        push word [v_col]
        mov byte [v_idioma], 0
        mov bx, PROG_INI
.l:
        cmp word [bx], 0
        je .fin
        call lista_linea
        mov bx, [bx]
        jmp .l
.fin:
        mov al, 0x1A
        call pon_car
        call vacia_salida
        mov bx, [v_sal_h]
        mov word [v_sal_h], 0
        mov ah, 0x3E
        int 0x21
        pop word [v_col]
        pop si
        ret
.no:
        mov al, 64
        jmp error
; escribe al archivo lo que haya en su búfer (b_disco)
vacia_salida:
        push ax
        push bx
        push cx
        push dx
        mov cx, [v_sal_n]
        jcxz .x
        mov bx, [v_sal_h]
        mov dx, b_disco
        mov ah, 0x40
        int 0x21
        jc .lleno
        cmp ax, cx
        jne .lleno
        mov word [v_sal_n], 0
.x:
        jmp sal_dx
.lleno:
        mov bx, [v_sal_h]
        mov word [v_sal_h], 0
        mov ah, 0x3E
        int 0x21
        mov al, 61
        jmp error
; un carácter al archivo de SAVE
pon_car_archivo:
        push bx
        mov bx, [v_sal_n]
        mov [b_disco + bx], al
        inc bx
        mov [v_sal_n], bx
        cmp bx, 512
        jb .x
        call vacia_salida
.x:
        pop bx
        ret

