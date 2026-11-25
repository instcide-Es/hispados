; =============================================================================
;  BASIC.COM - Hispa-BASIC, el BASIC de Hispa-8086
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;       BASIC               el intérprete («Ok» y a escribir)
;       BASIC PROGRAMA      carga PROGRAMA.BAS y lo ejecuta
;
;  Compatible con GW-BASIC 3.23 y en español: ver docs/8086/BASIC.md.
;  El código va en el segmento del programa; los datos (variables del
;  intérprete, el programa del usuario, sus variables y sus cadenas), en un
;  segmento de 64 K que se pide al sistema (DS = ES = SS = ese segmento).
;  Solo instrucciones del 8086, y sin coprocesador.
; =============================================================================
        cpu 8086
        org 0x100

%define VERSION "0.11"

%macro DOS 1
        mov ah, %1
        int 0x21
%endmacro

inicio:
        cld
        ; ---- la memoria: un .COM tiene toda la que hay (hasta PSP:2); los
        ; datos van justo detrás del código, 64 K o lo que quede ----
        mov ax, cs
        mov bx, fin_codigo + 15
        mov cl, 4
        shr bx, cl
        add ax, bx                  ; AX = el segmento de los datos
        mov bx, [2]                 ; el primer segmento que no es nuestro
        sub bx, ax
        jbe .sin_mem
        cmp bx, (PROG_INI + 4096) / 16
        jae .tengo
.sin_mem:
        mov dx, m_sin_mem
        DOS 0x09
        mov ax, 0x4C01
        int 0x21
.tengo:
        ; AX = el segmento; BX = sus párrafos
        mov [cs:seg_datos], ax
        mov dx, cs
        mov es, ax
        ; las variables del intérprete, a cero
        push ax
        xor di, di
        mov cx, PROG_INI / 2
        xor ax, ax
        rep stosw
        pop ax
        ; la línea de órdenes (PSP:80h), al segmento nuevo, antes de cambiar
        push ax                     ; (AX = el segmento: el 0 de abajo lo pisaría)
        mov si, 0x80
        mov di, b_lin
        xor ch, ch
        mov cl, [si]
        inc si
        cmp cl, 126
        jbe .lc
        mov cl, 126
.lc:
        rep movsb
        xor al, al
        stosb
        pop ax
        ; DS = ES = SS = los datos
        mov ds, ax
        cli
        mov ss, ax
        mov sp, pila_cima
        sti
        mov [v_seg_cod], dx
        mov [v_pila0], sp
        mov [v_defseg], ds
        mov word [v_fnp], v_fn_pila
        ; la cima de lo del usuario
        mov ax, bx
        cmp ax, 0x1000
        jb .cima
        mov ax, 0x0FFF
.cima:
        mov cl, 4
        shl ax, cl
        and al, 0xFE
        mov [v_cima], ax
        mov word [v_ancho], 80
        mov word [v_col], 0
        mov byte [v_attr], 7
        mov byte [v_gcolor], 3
        call es_pantalla
        sbb al, al
        mov [v_es_pant], al
        mov byte [PROG_INI - 1], 0  ; (lo que hay antes de la primera línea)
        call nuevo
        ; RND: la semilla de GW-BASIC
        mov word [v_azar], 0x0000
        mov word [v_azar + 2], 0x0005
        ; Ctrl+Inter
        mov ax, 0x351B
        int 0x21
        mov [v_int1b], bx
        mov [v_int1b + 2], es
        push ds
        pop es
        push ds
        mov ds, [v_seg_cod]
        mov dx, int1b
        mov ax, 0x251B
        int 0x21
        mov dx, int23
        mov ax, 0x2523
        int 0x21
        pop ds
        ; ---- el saludo ----
        mov si, m_saludo
        call pon_cad_cs
        call pon_libres
        ; ---- ¿un programa en la línea de órdenes? ----
        mov si, b_lin
.arg:
        lodsb
        cmp al, ' '
        je .arg
        dec si
        test al, al
        jz ordenes_ok
        ; CARGAR "programa",R
        mov di, b_nom
.a1:
        lodsb
        cmp al, ' '
        jbe .a2
        cmp al, '/'
        je .a2
        stosb
        jmp .a1
.a2:
        xor al, al
        stosb
        mov byte [v_cargar_y_ejecutar], 1
        call carga_archivo          ; si va bien, ejecuta (no vuelve)
        jmp ordenes_ok

seg_datos   dw 0

; ---- Ctrl+Inter y Ctrl+C: se anota y se mira entre sentencia y sentencia
int1b:
int23:
        push ds
        mov ds, [cs:seg_datos]
        mov byte [v_corte], 1
        pop ds
        iret

; ---- salir al sistema ----
salir:
        push ds
        lds dx, [v_int1b]
        mov ax, 0x251B
        int 0x21
        pop ds
        mov ax, 0x4C00
        int 0x21

; =============================================================================
;  El bucle de las órdenes
; =============================================================================
ordenes_ok:
        mov sp, [v_pila0]
        call col_cero
        mov si, m_ok
        call pon_cad_cs
ordenes:
        mov sp, [v_pila0]
        mov word [v_linea], 0xFFFF
        mov word [v_temp_n], 0
        cmp byte [v_auto], 0
        je .normal
        ; ---- AUTO: el número, y lo que se escriba es esa línea ----
        mov ax, [v_auto_n]
        call pon_natural
        call busca_linea
        mov al, ' '
        jnc .a1
        mov al, '*'                 ; (ya existe: se cambiaría)
.a1:
        call pon_car
        call lee_linea_ent
        jc salir
        cmp byte [v_corte], 0
        jne .a_fin
        mov si, b_lin
.a2:
        lodsb
        cmp al, ' '
        je .a2
        dec si
        test al, al
        jz .a_fin                   ; una línea vacía: se acaba
        call tokeniza
        mov ax, [v_auto_n]
        call mete_linea
        mov ax, [v_auto_paso]
        add [v_auto_n], ax
        jc .a_fin
        jmp ordenes
.a_fin:
        mov byte [v_auto], 0
        mov byte [v_corte], 0
        jmp ordenes_ok
.normal:
        call lee_linea_ent          ; b_lin; CF=1 si se acabó la entrada
        jc salir
        mov si, b_lin
.esp:
        lodsb
        cmp al, ' '
        je .esp
        test al, al
        jz ordenes
        dec si
        call es_cifra
        jc .con_numero
        ; ---- una orden directa ----
        call tokeniza
        mov si, b_tok - 1           ; (b_tok-1 tiene que ser un 0: ver datos)
        mov byte [si], 0
        inc si
        mov word [v_linea], 0xFFFF
        call ejecuta                ; vuelve al acabar la línea
        jmp ordenes_ok
.con_numero:
        call lee_num_linea          ; AX = el número
        jc .mal_num
        ; un espacio detrás del número no se guarda
        cmp byte [si], ' '
        jne .n1
        inc si
.n1:
        push ax
        call tokeniza               ; CX = la longitud con el 0
        pop ax
        call mete_linea
        jmp ordenes
.mal_num:
        mov al, 2
        jmp error

; ---- lee un número de línea de DS:SI (0 a 65529): AX. CF=1 si no vale ----
lee_num_linea:
        xor ax, ax
        xor bx, bx
.c:
        mov bl, [si]
        sub bl, '0'
        jb .x
        cmp bl, 9
        ja .x
        mov dx, 10
        mul dx
        jc .no
        add ax, bx
        jc .no
        inc si
        jmp .c
.x:
        cmp ax, 65530
        jae .no
        clc
        ret
.no:
        stc
        ret

; =============================================================================
;  El programa: líneas [enlace][número][texto tokenizado][0], seguidas; al
;  final, un enlace 0. v_vars está justo detrás.
; =============================================================================

; ---- NEW: borra el programa y las variables ----
nuevo:
        mov word [PROG_INI], 0
        mov word [v_prog], PROG_INI
        mov word [v_vars], PROG_INI + 2
        mov word [v_cont_lin], 0
        mov byte [v_tron], 0
        ; (sigue)
; ---- CLEAR: borra las variables, las cadenas y las pilas ----
vacia:
        mov ax, [v_vars]
        mov [v_mats], ax
        mov [v_libre], ax
        mov ax, [v_cima]
        mov [v_cads], ax
        mov word [v_ctrl], b_ctrl
        mov word [v_temp_n], 0
        mov word [v_dat_lin], 0
        mov word [v_dat_ptr], 0
        mov word [v_fn_fin], 0
        mov word [v_onerr], 0
        mov byte [v_en_trampa], 0
        ; todas las letras, simple precisión
        push di
        push cx
        mov di, v_def
        mov cx, 26
        mov al, T_SIM
        rep stosb
        pop cx
        pop di
        ret

; ---- busca la línea AX: BX = ella (CF=1) o la primera mayor (o el final) ----
busca_linea:
        mov bx, PROG_INI
.c:
        cmp word [bx], 0
        je .no
        cmp [bx + 2], ax
        je .si
        ja .no
        mov bx, [bx]
        jmp .c
.si:
        stc
        ret
.no:
        clc
        ret

; ---- mete (o borra, si el texto está vacío) la línea AX con b_tok (CX) ----
mete_linea:
        push ax
        push cx
        call busca_linea
        jnc .no_esta
        ; borrar la que hay
        mov cx, [bx]
        sub cx, bx                  ; lo que ocupa
        call reenlaza_menos
        ; mover lo de detrás
        push si
        push di
        mov di, bx
        mov si, bx
        add si, cx
        push cx
        mov cx, [v_vars]
        sub cx, si
        rep movsb
        pop cx
        sub [v_vars], cx
        pop di
        pop si
.no_esta:
        pop cx
        pop ax
        cmp cx, 1                   ; solo el 0: era para borrar
        jbe .x
        ; meterla en BX
        push ax
        add cx, 4                   ; con el enlace y el número
        mov ax, [v_vars]
        add ax, cx
        jc .sin_sitio
        add ax, 64
        jc .sin_sitio
        cmp ax, [v_cads]
        jae .sin_sitio
        call reenlaza_mas
        ; abrir el hueco (de atrás adelante)
        push si
        push di
        push cx
        mov si, [v_vars]
        dec si
        mov di, si
        add di, cx
        mov cx, [v_vars]
        sub cx, bx
        std
        rep movsb
        cld
        pop cx
        add [v_vars], cx
        ; escribirla. En la pila: [DI de quien llama] [SI] [el número]
        mov di, bx
        mov ax, bx
        add ax, cx
        stosw                       ; el enlace
        mov bp, sp
        mov ax, [bp + 4]            ; el número
        stosw
        mov si, b_tok
        sub cx, 4
        rep movsb
        pop di
        pop si
        pop ax
.x:
        ; las variables se pierden al cambiar el programa (como en GW-BASIC)
        mov word [v_cont_lin], 0
        jmp vacia
.sin_sitio:
        pop ax
        mov al, 7
        jmp error
; los enlaces que apuntan más allá de BX, menos CX / más CX
reenlaza_menos:
        push ax
        push si
        mov si, PROG_INI
.c:
        mov ax, [si]
        test ax, ax
        jz .x
        cmp ax, bx
        jbe .sig
        sub [si], cx
.sig:
        mov si, ax
        jmp .c
.x:
        pop si
        pop ax
        ret
reenlaza_mas:
        push ax
        push si
        mov si, PROG_INI
.c:
        mov ax, [si]
        test ax, ax
        jz .x
        cmp ax, bx
        jbe .sig
        add [si], cx
.sig:
        mov si, ax
        jmp .c
.x:
        pop si
        pop ax
        ret

; =============================================================================
;  Leer una línea de la entrada (el teclado o lo redirigido) a b_lin, ASCIZ.
;  CF=1 si ya no hay más.
; =============================================================================
lee_linea_ent:
        push bx
        push cx
        push dx
        push di
        mov di, b_lin
.c:
        call lee_car_ent            ; AL; CF=1 al final
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
        cmp di, b_lin               ; lo que hubiera antes del final, vale
        jne .ya
        stc
        jmp .x
.ya:
        xor al, al
        stosb
        mov word [v_col], 0
        clc
.x:
        pop di
        pop dx
        pop cx
        pop bx
        ret
; un carácter de la entrada (handle 0)
lee_car_ent:
        mov bx, [v_lee_pos]
        cmp bx, [v_lee_fin]
        jb .hay
        mov dx, b_ent
        mov cx, LIN_TAM
        xor bx, bx
        DOS 0x3F
        jc .no
        test ax, ax
        jz .no
        mov bx, b_ent
        mov [v_lee_pos], bx
        add ax, bx
        mov [v_lee_fin], ax
.hay:
        mov al, [bx]
        inc bx
        mov [v_lee_pos], bx
        clc
        ret
.no:
        stc
        ret

; =============================================================================
;  Escribir
; =============================================================================
; AL a la salida, llevando la columna
pon_car:
        cmp word [v_sal_mem], 0     ; EDIT: a la memoria
        jne pon_car_mem
        cmp word [v_sal_f], 0       ; PRINT #: al archivo
        jne pon_car_f
        cmp word [v_sal_h], 0
        je .pantalla
        cmp al, 10
        jne pon_car_archivo
        mov al, 13
        call pon_car_archivo
        mov al, 10
        jmp pon_car_archivo
.pantalla:
        push ax
        push dx
        mov [v_ult_car], al
        cmp al, 10
        je .nl
        cmp al, 13
        je .cr
        cmp al, 8
        je .atras
        cmp al, 7
        je .tal
        inc word [v_col]
        cmp byte [v_attr], 7        ; COLOR en texto, a la pantalla: por la
        je .tal                     ; BIOS, con su atributo
        cmp byte [v_modo], 0
        jne .tal
        cmp byte [v_es_pant], 0
        je .tal
        call pon_car_color
        pop dx
        pop ax
        ret
.atras:
        cmp word [v_col], 0
        je .tal
        dec word [v_col]
        jmp .tal
.cr:
        mov word [v_col], 0
.tal:
        mov dl, al
        DOS 0x02
        pop dx
        pop ax
        ret
.nl:                                ; 10: retorno y salto
        mov dl, 13
        DOS 0x02
        mov dl, 10
        DOS 0x02
        mov word [v_col], 0
        pop dx
        pop ax
        ret
; (EDIT) AL a la memoria, donde diga [v_sal_mem] (hasta llenar b_us), sin
; los saltos de línea
pon_car_mem:
        cmp al, ' '
        jb .x
        push bx
        mov bx, [v_sal_mem]
        cmp bx, b_us + 254
        jae .lleno
        mov [bx], al
        inc word [v_sal_mem]
.lleno:
        pop bx
.x:
        ret
pon_nl:
        push ax
        mov al, 10
        call pon_car
        pop ax
        ret
; si no se está al principio de la línea, salto
col_cero:
        cmp word [v_col], 0
        je .x
        call pon_nl
.x:
        ret
; el texto de CS:SI (ASCIZ)
pon_cad_cs:
        push ax
        push si
.c:
        mov al, [cs:si]
        inc si
        test al, al
        jz .x
        call pon_car
        jmp .c
.x:
        pop si
        pop ax
        ret
; el texto de DS:SI (ASCIZ)
pon_cad:
        push ax
        push si
.c:
        lodsb
        test al, al
        jz .x
        call pon_car
        jmp .c
.x:
        pop si
        pop ax
        ret
; AX con signo, sin espacio delante
pon_entero_sin:
        test ax, ax
        jns pon_natural
        push ax
        mov al, '-'
        call pon_car
        pop ax
        neg ax
; AX sin signo
pon_natural:
        push bx
        mov bx, 10
        call pon_en_base
        pop bx
        ret
; AX sin signo en base BX
pon_en_base:
        push ax
        push cx
        push dx
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
        cmp al, '9'
        jbe .e
        add al, 'A' - '9' - 1
.e:
        call pon_car
        loop .p
        pop dx
        pop cx
        pop ax
        ret
; «NNNNN octetos libres»
pon_libres:
        mov ax, [v_cads]
        sub ax, [v_libre]
        call pon_natural
        mov si, m_libres
        jmp pon_cad_cs

; =============================================================================
;  Los errores: AL = el número. Escribe el mensaje y vuelve a las órdenes.
; =============================================================================
error:
        cld
        push ax
        call error_trampa           ; ON ERROR: si lo atiende, no vuelve
        pop ax
        call fin_print_arch
        mov sp, [v_pila0]
        xor ah, ah
        mov [v_err], ax
        mov bx, [v_linea]
        mov [v_ultima], bx
        mov word [v_temp_n], 0
        mov word [v_ctrl], b_ctrl
        mov word [v_nom_n], 0
        mov word [v_fnp], v_fn_pila
        mov word [v_sal_h], 0
        push ax
        call cierra_carga
        call col_cero
        pop ax
        ; el texto
        mov si, errores
.b:
        mov ah, [cs:si]
        test ah, ah
        jz .sin_nombre
        inc si
        cmp ah, al
        je .es
.salta:
        cmp byte [cs:si], 0
        je .s1
        inc si
        jmp .salta
.s1:
        inc si
        jmp .b
.sin_nombre:
        mov si, m_err_sin
.es:
        call pon_cad_cs
        cmp word [v_linea], 0xFFFF
        je .sin_lin
        mov si, m_en
        call pon_cad_cs
        mov ax, [v_linea]
        call pon_natural
        ; CONT no puede seguir tras un error
.sin_lin:
        mov word [v_cont_lin], 0
        call pon_nl
        jmp ordenes_ok

; un error de sintaxis (para saltar a él)
err_sintaxis:
        mov al, 2
        jmp error
err_funcion:
        mov al, 5
        jmp error
err_tipos:
        mov al, 13
        jmp error
err_desb:
        mov al, 6
        jmp error

; ---- los mensajes ----
m_saludo    db "Hispa-BASIC ", VERSION, " (Hispa-8086)", 10
            db "Copyright (C) 2026 Carlos Sanz y contribuidores.", 10
            db "Compatible con GW-BASIC. Software libre: GPL-2.0 o posterior.", 10, 0
m_libres    db " octetos libres", 10, 0
m_ok        db "Ok", 10, 0
m_en        db " en ", 0
m_sin_mem   db "Hispa-BASIC: no hay memoria bastante.", 13, 10, "$"
m_err_sin   db "Error sin nombre", 0
m_parado    db "Parado", 0
m_interr    db "Interrumpido", 0

errores:
        db 1, "SIGUIENTE sin PARA", 0
        db 2, "Error de sintaxis", 0
        db 3, "VOLVER sin LLAMAR", 0
        db 4, "No quedan DATOS", 0
        db 5, "Llamada no válida", 0
        db 6, "Desbordamiento", 0
        db 7, "Memoria agotada", 0
        db 8, "No existe esa línea", 0
        db 9, "Índice fuera de la matriz", 0
        db 10, "Matriz ya dimensionada", 0
        db 11, "División por cero", 0
        db 12, "No vale en modo directo", 0
        db 13, "Tipos que no casan", 0
        db 14, "Sin sitio para cadenas", 0
        db 15, "Cadena demasiado larga", 0
        db 16, "Expresión de cadenas muy compleja", 0
        db 17, "No se puede seguir", 0
        db 18, "Función FN sin definir", 0
        db 19, "Falta RESUME", 0
        db 20, "RESUME sin error", 0
        db 22, "Falta un operando", 0
        db 23, "Línea demasiado larga", 0
        db 26, "PARA sin SIGUIENTE", 0
        db 29, "MIENTRAS sin FINMIENTRAS", 0
        db 30, "FINMIENTRAS sin MIENTRAS", 0
        db 50, "Desbordamiento de FIELD", 0
        db 52, "Número de archivo no válido", 0
        db 53, "No existe el archivo", 0
        db 54, "Modo de archivo no válido", 0
        db 55, "El archivo ya está abierto", 0
        db 57, "Error de entrada/salida", 0
        db 58, "El archivo ya existe", 0
        db 61, "Disco lleno", 0
        db 62, "Se ha leído más allá del final", 0
        db 64, "Nombre de archivo no válido", 0
        db 66, "Sentencia directa en el archivo", 0
        db 67, "Demasiados archivos", 0
        db 70, "Permiso denegado", 0
        db 0

%include "tokens.inc"
%include "num.inc"
%include "expr.inc"
%include "cadenas.inc"
%include "sent.inc"
%include "funciones.inc"
%include "archivo.inc"
%include "ficheros.inc"
%include "using.inc"
%include "edicion.inc"
%include "grafico.inc"
%include "sonido.inc"

fin_codigo:

%include "datos.inc"
