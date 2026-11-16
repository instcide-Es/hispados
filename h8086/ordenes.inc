; =============================================================================
;  Hispa-8086 - el intérprete de órdenes
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Provisional: va dentro del núcleo hasta que sea un programa aparte (etapa
;  3), y por eso hace su trabajo como lo haría un programa: con INT 21h. Las
;  órdenes llevan los mismos nombres que en Hispa-DOS, el español y el
;  clásico:
;       AYUDA (HELP)        VER (VERSION)       EQUIPO
;       DIR (LISTAR)        MOSTRAR (TYPE)      COPIAR (COPY)
;       BORRAR (DEL)        RENOMBRAR (REN)     CD
;       CREARDIR (MD)       BORRARDIR (RD)      LIMPIAR (CLS)
;       ACTUALIZAR
;  Cualquier otra palabra es un programa: NOMBRE.COM o NOMBRE.EXE, en el
;  directorio de trabajo o en la raíz.
; =============================================================================
%macro DOS 1
        mov ah, %1
        int 0x21
%endmacro

%macro VARS_ORDENES 0
orden       resb 10
            alignb 2
resto       resw 1
o_h1        resw 1                  ; los archivos que abre una orden
o_h2        resw 1
d_n         resw 1                  ; DIR: cuántos y cuánto ocupan
d_total     resw 2
arg1        resb RUTA_MAX + 16
arg2        resb RUTA_MAX + 16
%endmacro

COPIA_TAM   equ 0x2000              ; se copia de 8 K en 8 K

interprete:
        cli
        mov ax, cs
        mov ss, ax
        mov sp, pila_fin
        mov ds, ax
        mov es, ax
        sti
        cld
        mov si, m_indicador         ; «A:\CARTAS>»
        call pon_cad
        mov si, dir_txt
        call pon_cad
        mov al, '>'
        call pon_car
        mov di, linea
        mov cx, LIN_MAX
        call lee_linea
        call pon_nl
        ; la orden: la primera palabra, en mayúsculas y hasta ocho letras
        mov si, linea
        call salta_esp
        cmp byte [si], 0
        je interprete
        mov di, orden
        mov cx, 8
.p:
        mov al, [si]
        cmp al, ' '
        jbe .pf
        cmp al, '.'
        je .p2
        inc si
        jcxz .p
        call mayus
        stosb
        dec cx
        jmp .p
.p2:
        mov al, [si]                ; (la extensión no cuenta)
        cmp al, ' '
        jbe .pf
        inc si
        jmp .p2
.pf:
        xor al, al
        stosb
        mov [resto], si             ; lo que viene detrás
        mov bx, ordenes             ; ¿es una orden de las de dentro?
.b:
        mov si, [bx]
        test si, si
        jz .programa
        mov di, orden
.c:
        mov al, [si]
        cmp al, [di]
        jne .otra
        inc si
        inc di
        test al, al
        jnz .c
        mov si, [resto]
        call [bx+2]
        jmp interprete
.otra:
        add bx, 4
        jmp .b
.programa:
        call ejecuta
        jmp interprete

            align 2
ordenes     dw t_ayuda, o_ayuda, t_help, o_ayuda
            dw t_ver, o_ver, t_version, o_ver
            dw t_equipo, o_equipo
            dw t_dir, o_dir, t_listar, o_dir
            dw t_mostrar, o_mostrar, t_type, o_mostrar
            dw t_copiar, o_copiar, t_copy, o_copiar
            dw t_borrar, o_borrar, t_del, o_borrar
            dw t_renombrar, o_renombrar, t_ren, o_renombrar
            dw t_cd, o_cd
            dw t_creardir, o_md, t_md, o_md
            dw t_borrardir, o_rd, t_rd, o_rd
            dw t_limpiar, o_limpiar, t_cls, o_limpiar
            dw t_actualizar, o_actualizar
            dw 0
; (las órdenes se miran a ocho letras: RENOMBRAR es RENOMBRA)
t_ayuda     db "AYUDA", 0
t_help      db "HELP", 0
t_ver       db "VER", 0
t_version   db "VERSION", 0
t_equipo    db "EQUIPO", 0
t_dir       db "DIR", 0
t_listar    db "LISTAR", 0
t_mostrar   db "MOSTRAR", 0
t_type      db "TYPE", 0
t_copiar    db "COPIAR", 0
t_copy      db "COPY", 0
t_borrar    db "BORRAR", 0
t_del       db "DEL", 0
t_renombrar db "RENOMBRA", 0
t_ren       db "REN", 0
t_cd        db "CD", 0
t_creardir  db "CREARDIR", 0
t_md        db "MD", 0
t_borrardir db "BORRARDI", 0
t_rd        db "RD", 0
t_limpiar   db "LIMPIAR", 0
t_cls       db "CLS", 0
t_actualizar db "ACTUALIZ", 0

; SI salta los espacios
salta_esp:
        cmp byte [si], ' '
        jne .x
        inc si
        jmp salta_esp
.x:
        ret
; la palabra siguiente de SI -> DI (acabada en 0); SI avanza. ZF=1 si no hay
toma_arg:
        push ax
        push bx
        call salta_esp
        xor bx, bx
.c:
        mov al, [si]
        cmp al, ' '
        jbe .f
        cmp bx, RUTA_MAX - 1
        jae .f
        mov [di+bx], al
        inc bx
        inc si
        jmp .c
.f:
        mov byte [di+bx], 0
        test bx, bx
        pop bx
        pop ax
        ret
; ¿tiene comodines el texto de SI? ZF=1 si los tiene. Conserva los registros
con_comodin:
        push ax
        push si
.c:
        lodsb
        test al, al
        jz .no
        cmp al, '*'
        je .x
        cmp al, '?'
        jne .c
.x:
        pop si
        pop ax
        ret
.no:
        inc al                      ; ZF=0
        jmp .x
; DI -> el 0 del final del texto de DI
al_final:
        cmp byte [di], 0
        je .x
        inc di
        jmp al_final
.x:
        ret
; SI=ruta -> SI=donde empieza su nombre (detrás de la última barra o los dos
; puntos)
nombre_de:
        push ax
        push bx
        mov bx, si
.c:
        lodsb
        test al, al
        jz .f
        cmp al, '\'
        je .m
        cmp al, '/'
        je .m
        cmp al, ':'
        jne .c
.m:
        mov bx, si
        jmp .c
.f:
        mov si, bx
        pop bx
        pop ax
        ret
; copia el texto de SI a DI (con su 0); DI queda en el 0
copia_txt:
        push ax
.c:
        lodsb
        stosb
        test al, al
        jnz .c
        dec di
        pop ax
        ret
; ¿es un directorio lo que nombra DX? ZF=1 si lo es (y no lleva comodines)
es_directorio:
        push ax
        push cx
        push si
        mov si, dx
        call con_comodin
        je .no
        mov cx, 0x10
        DOS 0x4E
        jc .no
        test byte [dta_int+21], 0x10
        jz .no
        cmp al, al
        jmp short .x
.no:
        or al, 1                    ; ZF=0
.x:
        pop si
        pop cx
        pop ax
        ret

; escribe el error AX de una orden
pon_error:
        push bx
        push si
        mov bx, errores
.b:
        cmp word [bx], 0
        je .otro
        cmp ax, [bx]
        je .esta
        add bx, 4
        jmp .b
.esta:
        mov si, [bx+2]
        call pon_cad
        jmp short .x
.otro:
        mov si, m_e_error
        call pon_cad
        call pon_num
        call pon_nl
.x:
        pop si
        pop bx
        ret
            align 2
errores     dw E_FORMATO, m_e_formato, E_ARCHIVO, m_e_no_esta, E_RUTA, m_e_ruta, E_MUCHOS, m_e_muchos
            dw E_ACCESO, m_e_acceso, E_DIR_ACT, m_e_dir_act, E_OTRO_DISP, m_e_otro_dir
            dw E_NO_MAS, m_e_no_esta, E_PROTEGIDO, m_e_protegido, E_DISCO, m_e_disco
            dw 0

; ---- las sencillas ----
o_ayuda:
        mov si, m_ayuda
        jmp pon_cad
o_ver:
        mov si, m_ver
        jmp pon_cad
o_equipo:
        call pon_equipo
        jmp pon_consejo
o_limpiar:
        mov ax, 0x0003              ; el modo de texto, otra vez: pantalla limpia
        cmp byte [es_mono], 0
        je .m
        mov al, 7
.m:
        int 0x10
        ret
o_actualizar:
        mov si, m_actualizar
        call pon_cad
        jmp pon_consejo

; ---- DIR [ruta o patrón] ----
o_dir:
        mov di, arg1
        call toma_arg
        jnz .hay
        mov si, t_todos
        mov di, arg1
        call copia_txt
        jmp short .busca
.hay:
        mov di, arg1                ; un directorio: lo que tiene dentro
        call al_final
        mov al, [di-1]
        cmp al, '\'
        je .dentro
        cmp al, ':'
        je .dentro
        mov dx, arg1
        call es_directorio
        jne .busca
        mov al, '\'
        stosb
.dentro:
        mov si, t_todos
        call copia_txt
.busca:
        mov word [d_n], 0
        mov word [d_total], 0
        mov word [d_total+2], 0
        mov dx, arg1
        mov cx, 0x10
        DOS 0x4E
        jc .nada
.uno:
        mov si, dta_int + 30        ; el nombre, en 14 columnas
        mov cx, 14
.n:
        lodsb
        test al, al
        jz .pad
        call pon_car
        loop .n
.pad:
        mov al, ' '
        call pon_car
        loop .pad
        test byte [dta_int+21], 0x10
        jz .arch
        mov si, m_dir
        call pon_cad
        jmp short .sig
.arch:
        mov ax, [dta_int+26]
        mov dx, [dta_int+28]
        add [d_total], ax
        adc [d_total+2], dx
        mov cx, 10
        call pon_num32_ancho
        call pon_nl
.sig:
        inc word [d_n]
        DOS 0x4F
        jnc .uno
.fin:
        mov ax, [d_n]
        call pon_num
        mov si, m_archivos
        call pon_cad
        mov ax, [d_total]
        mov dx, [d_total+2]
        call pon_num32
        mov si, m_octetos
        call pon_cad
        DOS 0x36                    ; lo libre: grupos x sectores x 512
        cmp ax, 0xFFFF
        je .x
        mul bx
        mov cl, 9                   ; DX:AX por 512
        shl dx, cl
        mov bx, ax
        mov cl, 7
        shr bx, cl
        or dx, bx
        mov cl, 9
        shl ax, cl
        call pon_num32
        mov si, m_libres
        call pon_cad
.x:
        ret
.nada:
        cmp ax, E_NO_MAS
        je .fin
        jmp pon_error

; ---- MOSTRAR archivo ----
o_mostrar:
        mov di, arg1
        call toma_arg
        jnz .n
        mov si, m_e_falta_nombre
        jmp pon_cad
.n:
        mov dx, arg1
        mov al, 0
        DOS 0x3D
        jc pon_error
        mov [o_h1], ax
.otro:
        push ds                     ; de 512 en 512, a la memoria libre
        mov bx, [o_h1]
        mov cx, 512
        mov ds, [prog_seg]
        xor dx, dx
        DOS 0x3F
        pop ds
        jc .fin
        test ax, ax
        jz .fin
        mov cx, ax
        push es
        mov es, [prog_seg]
        xor si, si
.c:
        mov al, [es:si]
        cmp al, 0x1A                ; el fin de archivo de los textos
        je .corta
        call pon_car_dos
        inc si
        loop .c
        pop es
        jmp .otro
.corta:
        pop es
.fin:
        mov bx, [o_h1]
        DOS 0x3E
        jmp pon_nl

; ---- COPIAR origen destino ----
o_copiar:
        mov di, arg1
        call toma_arg
        jz .falta
        mov di, arg2
        call toma_arg
        jnz .hay
.falta:
        mov si, m_e_copiar
        jmp pon_cad
.hay:
        mov di, arg2                ; si el destino es un directorio, dentro
        call al_final               ; de él y con el mismo nombre
        mov al, [di-1]
        cmp al, '\'
        je .dentro
        cmp al, ':'
        je .dentro
        mov dx, arg2
        call es_directorio
        jne .abre
        mov al, '\'
        stosb
.dentro:
        mov si, arg1
        call nombre_de
        call copia_txt
.abre:
        mov dx, arg1
        mov al, 0
        DOS 0x3D
        jc pon_error
        mov [o_h1], ax
        mov dx, arg2
        xor cx, cx
        DOS 0x3C
        jnc .creado
        push ax
        mov bx, [o_h1]
        DOS 0x3E
        pop ax
        jmp pon_error
.creado:
        mov [o_h2], ax
.trozo:
        push ds
        mov bx, [o_h1]
        mov cx, COPIA_TAM
        mov ds, [prog_seg]
        xor dx, dx
        DOS 0x3F
        pop ds
        jc .mal
        test ax, ax
        jz .hecho
        mov cx, ax
        push ds
        mov bx, [o_h2]
        mov ds, [prog_seg]
        xor dx, dx
        DOS 0x40
        pop ds
        jc .mal
        cmp ax, cx
        je .trozo
        mov si, m_e_lleno           ; no ha cabido todo
        call pon_cad
        jmp short .cierra
.mal:
        call pon_error
        jmp short .cierra
.hecho:
        mov si, m_copiado
        call pon_cad
.cierra:
        mov bx, [o_h1]
        DOS 0x3E
        mov bx, [o_h2]
        DOS 0x3E
        jnc .x
        call pon_error
.x:
        ret

; ---- BORRAR archivo (o varios, con comodines) ----
o_borrar:
        mov di, arg1
        call toma_arg
        jnz .n
        mov si, m_e_falta_nombre
        jmp pon_cad
.n:
        mov si, arg1
        call con_comodin
        je .varios
        mov dx, arg1
        DOS 0x41
        jc pon_error
        ret
.varios:
        mov dx, arg1
        xor cx, cx
        DOS 0x4E
        jc pon_error
.uno:
        mov si, arg1                ; su ruta: la del patrón, con su nombre
        mov di, arg2
        call copia_txt
        mov si, arg2
        call nombre_de
        mov di, si
        mov si, dta_int + 30
        call copia_txt
        mov dx, arg2
        DOS 0x41
        jnc .sig
        call pon_error
.sig:
        DOS 0x4F
        jnc .uno
        ret

; ---- RENOMBRAR viejo nuevo ----
o_renombrar:
        mov di, arg1
        call toma_arg
        jz .falta
        mov di, arg2
        call toma_arg
        jnz .hay
.falta:
        mov si, m_e_renombrar
        jmp pon_cad
.hay:
        mov dx, arg1
        mov di, arg2
        DOS 0x56
        jc pon_error
        ret

; ---- CD [directorio]: sin nada, dice dónde se está ----
o_cd:
        mov di, arg1
        call toma_arg
        jnz .va
        mov si, m_indicador + 1
        call pon_cad
        mov si, dir_txt
        call pon_cad
        jmp pon_nl
.va:
        mov dx, arg1
        DOS 0x3B
        jc pon_error
        ret
; ---- CREARDIR y BORRARDIR ----
o_md:
        mov bl, 0x39
        jmp short dir_orden
o_rd:
        mov bl, 0x3A
dir_orden:
        mov di, arg1
        call toma_arg
        jnz .n
        mov si, m_e_falta_nombre
        jmp pon_cad
.n:
        mov dx, arg1
        mov ah, bl
        int 0x21
        jc pon_error
        ret

; ---- un programa: ORDEN.COM u ORDEN.EXE, en el directorio de trabajo o en la
;      raíz, con lo que venga detrás como parámetros ----
ejecuta:
        mov si, [resto]             ; lo de detrás, como lo quiere un programa:
        mov di, arg2 + 1            ; cuántas letras, las letras y un retorno
        xor cx, cx
.t:
        lodsb
        test al, al
        jz .tf
        stosb
        inc cx
        cmp cx, 100
        jb .t
.tf:
        mov byte [di], 0x0D
        mov [arg2], cl
        mov word [lz_cola], arg2
        mov [lz_cola+2], cs
        mov word [lz_ent], 0
        mov bx, intentos
.i:
        mov si, [bx]                ; delante («» o «\») y detrás (.COM, .EXE)
        test si, si
        jz .no
        mov di, ruta
        call copia_txt
        mov si, orden
        call copia_txt
        mov si, [bx+2]
        call copia_txt
        push bx
        call lanza                  ; (si vuelve, no ha podido)
        pop bx
        cmp ax, E_ARCHIVO
        je .otro
        cmp ax, E_RUTA
        je .otro
        cmp ax, E_MEMORIA
        jne pon_error
        mov si, m_e_grande
        jmp pon_cad
.otro:
        add bx, 4
        jmp .i
.no:
        mov si, m_e_orden
        jmp pon_cad
            align 2
intentos    dw t_nada, t_com, t_nada, t_exe, t_barra, t_com, t_barra, t_exe, 0
t_nada      db 0
t_barra     db "\", 0
t_exe       db ".EXE", 0

; ---- los textos de las órdenes ----
t_todos     db "*.*", 0
t_com       db ".COM", 0
m_indicador db 10, "A:\", 0
m_ver       db "Hispa-8086 versión 0.3", 10, 0
m_ayuda     db "Las órdenes de Hispa-8086 (nombre español y nombre clásico):", 10
            db "  AYUDA                     esta lista", 10
            db "  VER                       la versión del sistema", 10
            db "  EQUIPO                    el procesador y la memoria", 10
            db "  DIR [ruta]                los archivos (LISTAR)", 10
            db "  MOSTRAR archivo           enseña un archivo de texto (TYPE)", 10
            db "  COPIAR origen destino     copia un archivo (COPY)", 10
            db "  BORRAR archivo            lo borra; valen * y ? (DEL)", 10
            db "  RENOMBRAR viejo nuevo     le cambia el nombre (REN)", 10
            db "  CD [directorio]           cambia de directorio, o dice cuál es", 10
            db "  CREARDIR directorio       lo crea (MD)", 10
            db "  BORRARDIR directorio      lo quita, si está vacío (RD)", 10
            db "  LIMPIAR                   limpia la pantalla (CLS)", 10
            db "  ACTUALIZAR                cómo pasar a otro sistema Hispa", 10
            db "Cualquier otra palabra: el programa (.COM o .EXE) de ese nombre.", 10, 0
m_actualizar db "Todos los sistemas Hispa se actualizan entre sí:", 10
            db "  Hispa-8086  PC, XT y AT: 8086, 8088, V20, 80186 y 286", 10
            db "  Hispa-DOS   386, 486 y superiores, con Hispa Ventanas", 10
            db "Las versiones y los discos, en https://hispados.es", 10, 0
m_archivos  db " archivos, ", 0
m_octetos   db " octetos", 10, 0
m_libres    db " octetos libres", 10, 0
m_dir       db "     <DIR>", 10, 0
m_copiado   db "1 archivo copiado.", 10, 0
m_e_error   db "Error ", 0
m_e_disco   db "La unidad no está preparada.", 10, 0
m_e_protegido db "El disco está protegido contra escritura.", 10, 0
m_e_no_esta db "No existe ese archivo.", 10, 0
m_e_ruta    db "No existe esa ruta.", 10, 0
m_e_muchos  db "Hay demasiados archivos abiertos.", 10, 0
m_e_acceso  db "No se puede: acceso denegado.", 10, 0
m_e_dir_act db "No se puede quitar el directorio en el que se está.", 10, 0
m_e_otro_dir db "Para cambiar el nombre, los dos tienen que estar en el mismo directorio.", 10, 0
m_e_grande  db "El programa no cabe en la memoria.", 10, 0
m_e_formato db "Ese programa no es válido.", 10, 0
m_e_lleno   db "El disco está lleno: la copia ha quedado a medias.", 10, 0
m_e_orden   db "Orden desconocida. Escriba AYUDA.", 10, 0
m_e_falta_nombre db "Falta el nombre.", 10, 0
m_e_copiar  db "Se escribe así: COPIAR origen destino", 10, 0
m_e_renombrar db "Se escribe así: RENOMBRAR viejo nuevo", 10, 0
