; =============================================================================
;  Hispa-8086 - el intérprete de órdenes (COMMAND.COM)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Un programa como otro cualquiera: el núcleo lo lanza al arrancar (y otra
;  vez si termina) y él hace su trabajo con INT 21h. Al empezar se queda solo
;  con la memoria que ocupa (unos 8 K) y devuelve el resto, para los programas
;  que lance.
;
;  Las órdenes llevan los mismos nombres que en Hispa-DOS, el español y el
;  clásico:
;       AYUDA (HELP)        VER (VERSION)       EQUIPO
;       DIR (LISTAR)        MOSTRAR (TYPE)      COPIAR (COPY)
;       BORRAR (DEL)        RENOMBRAR (REN)     CD
;       CREARDIR (MD)       BORRARDIR (RD)      LIMPIAR (CLS)
;       ACTUALIZAR          ECO (ECHO)          REM         PAUSA (PAUSE)
;  Cualquier otra palabra es un programa (NOMBRE.COM o NOMBRE.EXE) o un
;  archivo por lotes (NOMBRE.BAT), del directorio de trabajo o de la raíz.
;
;  Un archivo por lotes es un texto con una orden por línea. Se carga entero
;  en un bloque de memoria y se va ejecutando; «@» delante de una línea hace
;  que no se enseñe, y ECO OFF, que no se enseñe ninguna. Al arrancar el
;  sistema se ejecuta \AUTOEXEC.BAT, si existe. (Todavía sin parámetros %1,
;  IF ni GOTO, y un lote que llama a otro no vuelve.)
; =============================================================================
        cpu 8086
        bits 16
        org 0x100

LIN_MAX     equ 126                 ; letras de una línea de órdenes
RUTA_MAX    equ 80
LOTE_MAX    equ 16384               ; lo más que mide un archivo por lotes
E_ARCHIVO   equ 2
E_RUTA      equ 3
E_MUCHOS    equ 4
E_ACCESO    equ 5
E_MEMORIA   equ 8
E_FORMATO   equ 11
E_DIR_ACT   equ 16
E_OTRO_DISP equ 17
E_NO_MAS    equ 18
E_PROTEGIDO equ 19
E_DISCO     equ 21
CPU_286     equ 3
CPU_386     equ 4

%macro DOS 1
        mov ah, %1
        int 0x21
%endmacro

inicio:
        cld
        mov sp, pila_fin            ; la pila, dentro de lo que nos quedamos
        mov bx, pila_fin + 15
        mov cl, 4
        shr bx, cl
        DOS 0x4A                    ; el resto de la memoria, para los demás
        mov di, bss_ini
        mov cx, pila_ini - bss_ini
        xor al, al
        rep stosb
        mov dx, dta_int
        DOS 0x1A
        int 0x11                    ; ¿pantalla monocroma?
        and al, 0x30
        cmp al, 0x30
        jne .color
        mov byte [es_mono], 1
.color:
        mov ax, 0xE800              ; el núcleo dice qué procesador hay
        int 0x21
        mov [cpu_tipo], bl
        mov byte [eco], 1
        ; ¿es el arranque del sistema? (el núcleo nos pasa «/P»)
        mov si, 0x81
.p:
        lodsb
        cmp al, 0x0D
        je bucle
        cmp al, '/'
        jne .p
        mov al, [si]
        and al, 0xDF
        cmp al, 'P'
        jne .p
        call pon_equipo
        call pon_consejo
        mov si, m_ayuda0
        call pon_cad
        mov si, t_autoexec
        call lote_abre

; ---- una línea tras otra: del lote, si hay uno; si no, del teclado ----
bucle:
        cli
        mov ax, cs
        mov ss, ax
        mov sp, pila_fin
        mov ds, ax
        mov es, ax
        sti
        cld
        cmp word [lote_seg], 0
        je .teclado
        call lote_linea
        jc bucle
        cmp byte [si], '@'          ; «@»: esta no se enseña
        jne .eco
        inc si
        jmp short .ejec
.eco:
        cmp byte [eco], 0
        je .ejec
        cmp byte [si], 0
        je .ejec
        call pon_indicador
        call pon_cad
        call pon_nl
        jmp short .ejec
.teclado:
        call pon_indicador
        mov byte [lin_buf], LIN_MAX
        mov dx, lin_buf
        DOS 0x0A
        mov bl, [lin_buf+1]
        xor bh, bh
        mov byte [lin_buf+2+bx], 0
        call pon_nl
        mov si, lin_buf + 2
.ejec:
        call ejecuta_linea
        jmp bucle

; «A:\CARTAS>»
pon_indicador:
        push si
        call trae_dir
        mov si, m_indicador
        call pon_cad
        mov si, dir_txt
        call pon_cad
        mov al, '>'
        call pon_car
        pop si
        ret
; el directorio de trabajo -> dir_txt
trae_dir:
        push ax
        push dx
        push si
        mov si, dir_txt
        xor dl, dl
        DOS 0x47
        pop si
        pop dx
        pop ax
        ret

; ---- ejecuta la línea de SI ----
ejecuta_linea:
        call salta_esp
        cmp byte [si], 0
        jne .hay
        ret
.hay:
        mov di, orden               ; la primera palabra, en mayúsculas y hasta
        mov cx, 8                   ; ocho letras
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
        jz ejecuta
        mov di, orden
.c:
        mov al, [si]
        cmp al, [di]
        jne .otra
        inc si
        inc di
        test al, al
        jnz .c
        ; una orden de dentro. Si pide memoria de borrador (borr_pide), se
        ; suelta al acabar
        mov word [borr_seg], 0
        mov si, [resto]
        call [bx+2]
        cmp word [borr_seg], 0
        je .x
        push cs
        pop ds
        mov es, [borr_seg]
        DOS 0x49
        push cs
        pop es
.x:
        ret
.otra:
        add bx, 4
        jmp .b

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
            dw t_eco, o_eco, t_echo, o_eco
            dw t_rem, o_rem
            dw t_pausa, o_pausa, t_pause, o_pausa
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
t_eco       db "ECO", 0
t_echo      db "ECHO", 0
t_rem       db "REM", 0
t_pausa     db "PAUSA", 0
t_pause     db "PAUSE", 0

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

; pide un trozo de memoria de borrador (8 K, o lo que haya) para la orden que
; corre: [borr_seg] y [borr_tam]. CF=1 (y lo dice) si no hay ni para empezar
borr_pide:
        push ax
        push bx
        push cx
        mov bx, 0x200
        DOS 0x48
        jnc .hay
        cmp bx, 0x20
        jb .no
        DOS 0x48
        jc .no
.hay:
        mov [borr_seg], ax
        mov cl, 4
        shl bx, cl
        mov [borr_tam], bx
        clc
        jmp short .x
.no:
        push si
        mov si, m_e_grande
        call pon_cad
        pop si
        stc
.x:
        pop cx
        pop bx
        pop ax
        ret

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
        call borr_pide
        jc .sin
        mov dx, arg1
        mov al, 0
        DOS 0x3D
        jc pon_error
        mov [o_h1], ax
        jmp short .otro
.sin:
        ret
.otro:
        push ds                     ; de 512 en 512, a la memoria libre
        mov bx, [o_h1]
        mov cx, 512
        mov ds, [borr_seg]
        xor dx, dx
        DOS 0x3F
        pop ds
        jc .fin
        test ax, ax
        jz .fin
        mov cx, ax
        push es
        mov es, [borr_seg]
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
        call borr_pide
        jc .x
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
        mov cx, [borr_tam]
        mov ds, [borr_seg]
        xor dx, dx
        DOS 0x3F
        pop ds
        jc .mal
        test ax, ax
        jz .hecho
        mov cx, ax
        push ds
        mov bx, [o_h2]
        mov ds, [borr_seg]
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
        call trae_dir
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

; ---- ECO texto, ECO ON, ECO OFF; REM; PAUSA ----
o_eco:
        call salta_esp
        mov ax, [si]
        and ax, 0xDFDF
        cmp ax, 'ON'
        jne .off
        cmp byte [si+2], ' '
        ja .txt
        mov byte [eco], 1
        ret
.off:
        cmp ax, 'OF'
        jne .txt
        mov al, [si+2]
        and al, 0xDF
        cmp al, 'F'
        jne .txt
        cmp byte [si+3], ' '
        ja .txt
        mov byte [eco], 0
        ret
.txt:
        call pon_cad
        jmp pon_nl
o_rem:
        ret
o_pausa:
        mov si, m_pausa
        call pon_cad
        DOS 0x08
        jmp pon_nl

; =============================================================================
;  Programas y archivos por lotes
; =============================================================================
; lo que no es una orden de dentro: ORDEN.COM, ORDEN.EXE u ORDEN.BAT, en el
; directorio de trabajo o en la raíz
ejecuta:
        mov si, [resto]             ; lo de detrás, como lo quiere un programa:
        mov di, cola + 1            ; cuántas letras, las letras y un retorno
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
        mov [cola], cl
        mov word [bloque], 0        ; el entorno: una copia del nuestro
        mov word [bloque+2], cola
        mov [bloque+4], cs
        mov bx, intentos
.i:
        mov si, [bx]                ; delante («» o «\») y detrás (la extensión)
        test si, si
        jz .no
        mov di, arg1
        call copia_txt
        mov si, orden
        call copia_txt
        mov si, [bx+2]
        call copia_txt
        cmp si, t_bat + 5           ; (copia_txt deja SI detrás del texto)
        je .lote
        push bx
        mov dx, arg1
        mov bx, bloque
        mov ax, 0x4B00
        int 0x21
        pop bx
        jnc .x
        cmp ax, E_ARCHIVO
        je .otro
        cmp ax, E_RUTA
        je .otro
        cmp ax, E_MEMORIA
        jne pon_error
        mov si, m_e_grande
        jmp pon_cad
.lote:
        mov si, arg1
        push bx
        call lote_abre
        pop bx
        jnc .x
.otro:
        add bx, 4
        jmp .i
.no:
        mov si, m_e_orden
        call pon_cad
.x:
        ret
            align 2
intentos    dw t_nada, t_com, t_nada, t_exe, t_nada, t_bat
            dw t_barra, t_com, t_barra, t_exe, t_barra, t_bat, 0
t_nada      db 0
t_barra     db "\", 0
t_exe       db ".EXE", 0
t_bat       db ".BAT", 0
t_autoexec  db "\AUTOEXEC.BAT", 0

; empieza a ejecutar el archivo por lotes de SI: lo carga entero en un bloque
; de memoria. CF=1 si no existe (o no cabe)
lote_abre:
        call lote_cierra            ; (si había otro, se deja)
        mov dx, si
        mov al, 0
        DOS 0x3D
        jc .x
        mov [o_h1], ax
        mov bx, ax
        xor cx, cx
        xor dx, dx
        mov al, 2
        DOS 0x42                    ; cuánto mide
        test dx, dx
        jnz .mal
        cmp ax, LOTE_MAX
        ja .mal
        mov [lote_fin], ax
        mov bx, ax
        add bx, 15 + 16
        mov cl, 4
        shr bx, cl
        DOS 0x48
        jc .mal
        mov [lote_seg], ax
        mov bx, [o_h1]
        xor cx, cx
        xor dx, dx
        mov al, 0
        DOS 0x42
        push ds
        mov cx, [lote_fin]
        mov ds, [lote_seg]
        xor dx, dx
        DOS 0x3F
        pop ds
        mov word [lote_pos], 0
        mov byte [eco], 1
        mov bx, [o_h1]
        DOS 0x3E
        clc
        ret
.mal:
        mov bx, [o_h1]
        DOS 0x3E
        stc
.x:
        ret
; deja el lote: suelta su memoria
lote_cierra:
        cmp word [lote_seg], 0
        je .x
        push ax
        push es
        mov es, [lote_seg]
        DOS 0x49
        pop es
        pop ax
        mov word [lote_seg], 0
        mov byte [eco], 1
.x:
        ret
; la línea siguiente del lote -> lin_buf+2 (SI). CF=1 si se ha acabado
lote_linea:
        push es
        mov es, [lote_seg]
        mov si, [lote_pos]
        mov di, lin_buf + 2
        xor cx, cx
.c:
        cmp si, [lote_fin]
        jae .fin
        mov al, [es:si]
        inc si
        cmp al, 0x1A
        je .eof
        cmp al, 13
        je .c
        cmp al, 10
        je .hecha
        cmp cx, LIN_MAX
        jae .c
        mov [di], al
        inc di
        inc cx
        jmp .c
.eof:
        mov si, [lote_fin]
.fin:
        jcxz .acaba
.hecha:
        mov [lote_pos], si
        mov byte [di], 0
        pop es
        mov si, lin_buf + 2
        clc
        ret
.acaba:
        pop es
        call lote_cierra
        stc
        ret

; =============================================================================
;  El equipo
; =============================================================================
; escribe el procesador y la memoria
pon_equipo:
        mov si, m_cpu
        call pon_cad
        mov bl, [cpu_tipo]
        xor bh, bh
        shl bx, 1
        mov si, [cpu_nombres+bx]
        call pon_cad
        mov si, m_mem
        call pon_cad
        int 0x12
        call pon_num
        mov si, m_mem2
        call pon_cad
        mov bx, 0xFFFF              ; lo libre: pedir de más dice cuánto hay
        DOS 0x48
        mov ax, bx
        mov cl, 6
        shr ax, cl
        call pon_num
        mov si, m_mem3
        jmp pon_cad
; si la máquina puede con un Hispa mayor, lo dice (toda la familia se
; recomienda entre sí: ver docs/8086/HISPA-8086.md)
pon_consejo:
        mov si, m_consejo_386
        cmp byte [cpu_tipo], CPU_386
        jae pon_cad
        mov si, m_consejo_286
        cmp byte [cpu_tipo], CPU_286
        je pon_cad
        ret

; =============================================================================
;  Escribir en la pantalla (con INT 21h)
; =============================================================================
; escribe AL tal cual. Conserva los registros
pon_car_dos:
        push ax
        push dx
        mov dl, al
        DOS 0x02
        pop dx
        pop ax
        ret
; escribe AL; el salto de línea (10) baja y vuelve al margen
pon_car:
        cmp al, 10
        jne pon_car_dos
        push ax
        mov al, 13
        call pon_car_dos
        mov al, 10
        call pon_car_dos
        pop ax
        ret
pon_nl:
        push ax
        mov al, 10
        call pon_car
        pop ax
        ret
; escribe el texto de DS:SI, que acaba en 0
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
; AL, a mayúscula (también la eñe de la página 437)
mayus:
        cmp al, 'a'
        jb .x
        cmp al, 'z'
        ja .otras
        sub al, 32
.x:
        ret
.otras:
        cmp al, 0xA4
        jne .x
        inc al
        ret
; escribe AX en decimal
pon_num:
        push dx
        xor dx, dx
        call pon_num32
        pop dx
        ret
; escribe DX:AX en decimal (pon_num32_ancho: arrimado a la derecha en CX)
pon_num32:
        push cx
        xor cx, cx
        call pon_num32_ancho
        pop cx
        ret
pon_num32_ancho:
        push ax
        push bx
        push cx
        push dx
        push si
        push di
        mov di, cx                  ; el ancho
        mov bx, 10
        xor cx, cx                  ; cuántas cifras
.d:
        push ax                     ; DX:AX entre 10, en dos pasos de 16 bits
        mov ax, dx
        xor dx, dx
        div bx
        mov si, ax                  ; la mitad alta del cociente
        pop ax
        div bx                      ; AX = la baja, DX = la cifra
        push dx
        inc cx
        mov dx, si
        mov si, ax
        or si, dx
        jnz .d
        mov si, cx
.esp:
        cmp si, di                  ; los espacios de delante
        jae .c
        mov al, ' '
        call pon_car
        inc si
        jmp .esp
.c:
        pop ax
        add al, '0'
        call pon_car
        loop .c
        pop di
        pop si
        pop dx
        pop cx
        pop bx
        pop ax
        ret

            align 2
cpu_nombres dw c_8086, c_v20, c_186, c_286, c_386, c_486
c_8086      db "8086 u 8088", 0
c_v20       db "NEC V20 o V30", 0
c_186       db "80186 u 80188", 0
c_286       db "80286", 0
c_386       db "80386", 0
c_486       db "80486 o superior", 0

; =============================================================================
;  Los textos (página 437)
; =============================================================================
t_todos     db "*.*", 0
t_com       db ".COM", 0
m_indicador db 10, "A:\", 0
m_ver       db "Hispa-8086 versión 0.4", 10, 0
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
            db "  ECO texto, REM, PAUSA     para los archivos por lotes (.BAT)", 10
            db "Cualquier otra palabra: el programa (.COM o .EXE) o el lote (.BAT)", 10
            db "de ese nombre, del directorio de trabajo o de la raíz.", 10, 0
m_actualizar db "Los sistemas de la familia Hispa-DOS se actualizan entre sí:", 10
            db "  Hispa-8086  PC, XT y AT: 8086, 8088, V20, 80186 y 286", 10
            db "  Hispa-386   386 o superior, con el escritorio Hispa Ventanas", 10
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
m_pausa     db "Pulse una tecla para seguir...", 0
m_cpu       db "Procesador: ", 0
m_mem       db 10, "Memoria:    ", 0
m_mem2      db " K (", 0
m_mem3      db " K libres para los programas)", 10, 0
m_consejo_386 db 10, "Este equipo tiene un procesador de 32 bits: puede con HISPA-386, el", 10
            db "hermano mayor de la familia Hispa-DOS, con su escritorio Hispa Ventanas.", 10
            db "Hispa-8086 funciona aquí, pero está hecho para los PC, XT y AT.", 10
            db "Escriba ACTUALIZAR.", 10, 0
m_consejo_286 db 10, "Este equipo tiene un 286: Hispa-8086 lo aprovecha (modo 286).", 10, 0
m_ayuda0    db 10, "Escriba AYUDA para ver las órdenes.", 10, 0

; =============================================================================
;  Las variables (no van en el archivo: se ponen a cero al empezar)
; =============================================================================
        absolute $
            alignb 2
bss_ini:
es_mono     resb 1
cpu_tipo    resb 1
eco         resb 1                  ; se enseñan las líneas de los lotes
            alignb 2
resto       resw 1
o_h1        resw 1                  ; los archivos que abre una orden
o_h2        resw 1
d_n         resw 1                  ; DIR: cuántos y cuánto ocupan
d_total     resw 2
borr_seg    resw 1                  ; el trozo de memoria de borrador
borr_tam    resw 1
lote_seg    resw 1                  ; el archivo por lotes (0: no hay)
lote_pos    resw 1
lote_fin    resw 1
bloque      resw 7                  ; para lanzar un programa (4Bh)
orden       resb 10
cola        resb 130
dta_int     resb 44                 ; lo que encuentran las búsquedas
dir_txt     resb 68
arg1        resb RUTA_MAX + 16
arg2        resb RUTA_MAX + 16
lin_buf     resb LIN_MAX + 6
pila_ini:
            resb 512
pila_fin:
