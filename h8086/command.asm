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
;       SI (IF)             IR A (GOTO)         PARA (FOR)
;       LLAMAR (CALL)       DESPLAZAR (SHIFT)   MOVER (MOVE)
;       FECHA (DATE)        HORA (TIME)         VOL
;       VERIFICAR (VERIFY)  MEMORIA (MEM)       PON (SET)
;       RUTA (PATH)         INDICADOR (PROMPT)
;  «C:» a secas cambia de unidad. Cualquier otra palabra es un programa
;  (NOMBRE.COM o NOMBRE.EXE) o un archivo por lotes (NOMBRE.BAT), del
;  directorio de trabajo o de los que diga la variable PATH (la pone RUTA).
;
;  Un archivo por lotes es un texto con una orden por línea. Se carga entero
;  en un bloque de memoria y se va ejecutando; «@» delante de una línea hace
;  que no se enseñe, y ECO OFF, que no se enseñe ninguna. Al arrancar el
;  sistema se ejecuta \AUTOEXEC.BAT, si existe. Dentro de un lote, %0 es su
;  nombre y %1 a %9 lo que se escribió detrás; IF [NOT] EXIST archivo,
;  IF [NOT] ERRORLEVEL n e IF [NOT] a==b ejecutan lo que sigue si se cumple;
;  GOTO etiqueta salta a la línea «:etiqueta». Un lote que nombra a otro le
;  pasa el mando y no vuelve; con LLAMAR (CALL), vuelve y sigue. DESPLAZAR
;  (SHIFT) corre los parámetros un sitio: %1 pasa a ser %0, %2 a ser %1...
;  FOR %%v IN (lista) DO orden (o PARA %%v EN (lista) HAZ orden) repite la orden
;  con cada elemento de la lista, y con cada archivo si lleva comodines; desde
;  el teclado la variable se escribe con un solo «%».
;
;  Redirección, al final de cualquier orden: «> archivo» manda a él lo que
;  iría a la pantalla, «>> archivo» lo añade, y «< archivo» hace que se lea de
;  él lo que se teclearía. Y tuberías: «orden1 | orden2» guarda lo que escribe
;  la primera en un archivo temporal (\TUBO1.$$$ y \TUBO2.$$$, en la raíz de
;  la unidad de trabajo, como hacía DOS) y se lo da a leer a la segunda.
;
;  Las órdenes que son programas del disco tienen también su nombre clásico:
;  FORMAT es FORMATEA, SYS es SISTEMA, FDISK es PARTIR, MORE es MAS (y «más»),
;  SORT es ORDENAR, CHKDSK es REVISAR, LABEL es ETIQUETA, ATTRIB es ATRIBUTO,
;  TREE es ARBOL, XCOPY es XCOPIAR, KEYB es TECLADO y EDIT es EDITAR: aquí se
;  traduce el nombre, sin duplicar los archivos.
;
;  SI y IR A son IF y GOTO en español, con el criterio de PARA (FOR): valen
;  las dos formas, y las palabras de dentro también (NO, EXISTE, NIVEL por
;  NOT, EXIST, ERRORLEVEL): «SI NO EXISTE X.TXT IR A FIN».
; =============================================================================
        cpu 8086
        bits 16
        org 0x100

LIN_MAX     equ 126                 ; letras de una línea de órdenes
RUTA_MAX    equ 80
LOTE_MAX    equ 16384               ; lo más que mide un archivo por lotes
LOTE_NOM_TAM equ 24                 ; letras de %0
ENT_TAM     equ 512                 ; lo que mide nuestro entorno
E_ARCHIVO   equ 2
E_RUTA      equ 3
E_MUCHOS    equ 4
E_ACCESO    equ 5
E_MEMORIA   equ 8
E_FORMATO   equ 11
E_UNIDAD    equ 15
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
        call ent_ini                ; un entorno propio, con sitio para crecer
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
        call ejecuta_tubo
        jmp bucle

; el indicador: lo que diga la variable PROMPT (la pone INDICADOR), o «$P$G»,
; que es «A:\CARTAS>». $P la unidad y el directorio, $N la unidad, $G «>»,
; $L «<», $B «|», $Q «=», $D la fecha, $T la hora, $_ línea nueva, $$ «$»
pon_indicador:
        push si
        push di
        call pon_nl
        mov si, t_prompt
        call ent_busca
        mov si, t_prompt_def
        jc .v
        mov si, arg2                ; (una copia, que el entorno está en otro
        push si                     ; segmento)
        mov cx, RUTA_MAX
.cp:
        mov al, [es:di]
        mov [si], al
        inc di
        inc si
        test al, al
        loopnz .cp
        mov byte [si], 0
        pop si
.v:
        push cs
        pop es
.c:
        lodsb
        test al, al
        jz .x
        cmp al, '$'
        je .d
.pon:
        call pon_car
        jmp .c
.d:
        lodsb
        test al, al
        jz .x
        call mayus
        mov bx, t_prompt_car
.b:
        mov ah, [bx]
        test ah, ah
        jz .otros
        cmp al, ah
        je .este
        add bx, 2
        jmp .b
.este:
        mov al, [bx+1]
        jmp .pon
.otros:
        cmp al, 'P'
        jne .o1
        call trae_dir
        call pon_unidad
        push si
        mov si, dir_txt
        call pon_cad
        pop si
        jmp .c
.o1:
        cmp al, 'N'
        jne .o2
        DOS 0x19
        add al, 'A'
        jmp .pon
.o2:
        cmp al, 'D'
        jne .o3
        call pon_fecha
        jmp .c
.o3:
        cmp al, 'T'
        jne .c
        call pon_hora
        jmp .c
.x:
        pop di
        pop si
        ret
t_prompt    db "PROMPT", 0
t_prompt_def db "$P$G", 0
t_prompt_car db "G>L<B|Q=$$_", 10, 0
; «C:\» (la unidad de trabajo)
pon_unidad:
        push ax
        DOS 0x19
        add al, 'A'
        call pon_car
        mov al, ':'
        call pon_car
        mov al, '\'
        call pon_car
        pop ax
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

; ---- ejecuta la línea de SI, con sus tuberías ----
; «a | b | c»: cada tramo se ejecuta con su redirección; lo que escribe va a
; un archivo temporal, que el tramo siguiente lee como si se tecleara. Se
; alternan dos archivos, que al acabar se borran
ejecuta_tubo:
        mov word [tubo_ent], 0
        mov byte [tubo_hay], 0
.otro:
        mov bx, si
.b:
        mov al, [bx]
        test al, al
        jz .ultimo
        cmp al, '|'
        je .hay
        inc bx
        jmp .b
.hay:
        mov byte [bx], 0            ; este tramo acaba aquí
        inc bx
        push bx
        mov ax, t_tubo1
        cmp word [tubo_ent], t_tubo1
        jne .s
        mov ax, t_tubo2
.s:
        mov [tubo_sal], ax
        mov byte [tubo_hay], 1
        call ejecuta_red
        pop si
        jc .borra                   ; (si no se ha podido, lo demás no se hace)
        mov ax, [tubo_sal]
        mov [tubo_ent], ax
        jmp .otro
.ultimo:
        mov word [tubo_sal], 0
        call ejecuta_red
.borra:
        cmp byte [tubo_hay], 0
        je .x
        mov dx, t_tubo1
        DOS 0x41
        mov dx, t_tubo2
        DOS 0x41
        mov word [tubo_ent], 0
        mov word [tubo_sal], 0
.x:
        ret
t_tubo1     db "\TUBO1.$$$", 0
t_tubo2     db "\TUBO2.$$$", 0

; ---- ejecuta la línea de SI, con su redirección ----
; «> archivo», «>> archivo» y «< archivo», al final de la orden: se abren los
; archivos, se le dice al núcleo (46h), se ejecuta lo de delante y se deshace.
; Si es un tramo de una tubería, lo que no se haya redirigido va al archivo
; temporal ([tubo_sal]) o viene de él ([tubo_ent]). CF=1 si no se ha podido
; abrir alguno (y entonces no se ejecuta nada)
ejecuta_red:
        push si
        mov word [r_sal], 0
        mov word [r_ent], 0
.b:
        mov al, [si]
        test al, al
        jz .va
        cmp al, '>'
        je .sal
        cmp al, '<'
        je .ent
        inc si
        jmp .b
.sal:
        mov byte [si], 0            ; la orden acaba aquí
        inc si
        xor bp, bp                  ; BP=1: añadir
        cmp byte [si], '>'
        jne .s1
        inc bp
        inc si
.s1:
        mov di, arg2
        call toma_red
        test bp, bp
        jz .crea
        mov dx, arg2                ; añadir: abrirlo e ir al final...
        mov al, 2
        DOS 0x3D
        jc .crea                    ; ...o crearlo, si no existe
        mov bx, ax
        xor cx, cx
        xor dx, dx
        mov al, 2
        DOS 0x42
        mov ax, bx
        jmp short .s2
.crea:
        mov dx, arg2
        xor cx, cx
        DOS 0x3C
        jc .mal
.s2:
        mov [r_sal], ax
        jmp .b
.ent:
        mov byte [si], 0
        inc si
        mov di, arg2
        call toma_red
        mov dx, arg2
        mov al, 0
        DOS 0x3D
        jc .mal
        mov [r_ent], ax
        jmp .b
.mal:
        call pon_error
        pop si
        call .deshace
        stc
        ret
.va:
        cmp word [r_ent], 0         ; la tubería: de dónde se lee...
        jne .t1
        mov dx, [tubo_ent]
        test dx, dx
        jz .t1
        mov al, 0
        DOS 0x3D
        jc .mal
        mov [r_ent], ax
.t1:
        cmp word [r_sal], 0         ; ...y adónde se escribe
        jne .t2
        mov dx, [tubo_sal]
        test dx, dx
        jz .t2
        mov cx, 0x02                ; (oculto: que no salga en «DIR | ORDENAR»)
        DOS 0x3C
        jc .mal
        mov [r_sal], ax
.t2:
        mov bx, [r_ent]
        test bx, bx
        jz .v1
        xor cx, cx
        DOS 0x46
.v1:
        mov bx, [r_sal]
        test bx, bx
        jz .v2
        mov cx, 1
        DOS 0x46
.v2:
        pop si
        call ejecuta_linea
.deshace:
        push cs
        pop ds
        push cs
        pop es
        mov bx, [r_sal]
        test bx, bx
        jz .d1
        push bx
        mov bx, 2                   ; la salida, otra vez a la pantalla
        mov cx, 1
        DOS 0x46
        pop bx
        DOS 0x3E
        mov word [r_sal], 0
.d1:
        mov bx, [r_ent]
        test bx, bx
        jz .d2
        push bx
        mov bx, 2
        xor cx, cx
        DOS 0x46
        pop bx
        DOS 0x3E
        mov word [r_ent], 0
.d2:
        clc
        ret
; (interna) el nombre de archivo de SI -> DI; SI queda en lo que lo acaba
toma_red:
        push ax
        push bx
        call salta_esp
        xor bx, bx
.c:
        mov al, [si]
        cmp al, ' '
        jbe .f
        cmp al, '>'
        je .f
        cmp al, '<'
        je .f
        cmp bx, RUTA_MAX - 1
        jae .f
        mov [di+bx], al
        inc bx
        inc si
        jmp .c
.f:
        mov byte [di+bx], 0
        pop bx
        pop ax
        ret

; ---- ejecuta la línea de SI ----
ejecuta_linea:
        call salta_esp
        cmp byte [si], 0
        je .nada
        cmp byte [si], ':'          ; (una etiqueta de un lote)
        jne .hay
.nada:
        ret
.hay:
        cmp byte [si+1], ':'        ; «C:» a secas: cambiar de unidad
        jne .orden
        cmp byte [si+2], ' '
        ja .orden
        mov al, [si]
        call mayus
        sub al, 'A'
        mov dl, al
        DOS 0x0E
        DOS 0x19
        cmp al, dl
        je .nada
        mov si, m_e_unidad
        jmp pon_cad
.orden:
        mov [pal_ini], si           ; (la palabra entera, por si es una ruta)
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
        mov word [borr_seg], 0      ; (una orden puede ejecutar otra: IF, FOR)
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
            dw t_if, o_if, t_goto, o_goto
            dw t_for, o_for, t_para, o_for
            dw t_llamar, o_call, t_call, o_call
            dw t_desplazar, o_shift, t_shift, o_shift
            dw t_si, o_if, t_ir, o_goto
            dw t_mover, o_mover, t_move, o_mover
            dw t_fecha, o_fecha, t_date, o_fecha
            dw t_hora, o_hora, t_time, o_hora
            dw t_vol, o_vol
            dw t_verificar, o_verificar, t_verify, o_verificar
            dw t_memoria, o_memoria, t_mem, o_memoria
            dw t_pon, o_pon, t_set, o_pon
            dw t_ruta, o_ruta, t_path, o_ruta
            dw t_indicador, o_indicador, t_prompt, o_indicador
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
t_if        db "IF", 0
t_goto      db "GOTO", 0
t_for       db "FOR", 0
t_para      db "PARA", 0
t_llamar    db "LLAMAR", 0
t_call      db "CALL", 0
t_desplazar db "DESPLAZA", 0
t_shift     db "SHIFT", 0
t_si        db "SI", 0
t_ir        db "IR", 0
t_mover     db "MOVER", 0
t_move      db "MOVE", 0
t_fecha     db "FECHA", 0
t_date      db "DATE", 0
t_hora      db "HORA", 0
t_time      db "TIME", 0
t_vol       db "VOL", 0
t_verificar db "VERIFICA", 0
t_verify    db "VERIFY", 0
t_memoria   db "MEMORIA", 0
t_mem       db "MEM", 0
t_pon       db "PON", 0
t_set       db "SET", 0
t_ruta      db "RUTA", 0
t_path      db "PATH", 0
t_indicador db "INDICADO", 0

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
        call nombre_de              ; «.» y «..» lo son (la raíz no los tiene
        cmp word [si], '.'          ; escritos, y no se encontrarían)
        je .x
        cmp word [si], '..'
        jne .busca
        cmp byte [si+2], 0
        je .x
.busca:
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
        cmp ax, E_DISCO             ; un fallo del disco: lo que dijo la BIOS
        jne .x
        push ax
        mov ax, 0xE807
        int 0x21
        push ax
        mov si, m_e_bios
        call pon_cad
        pop ax
        push ax
        mov cl, 4
        shr al, cl
        call .hex
        pop ax
        call .hex
        mov si, m_e_bios2
        call pon_cad
        pop ax
        jmp short .x
.hex:
        and al, 0x0F
        add al, '0'
        cmp al, '9'
        jbe .h1
        add al, 7
.h1:
        jmp pon_car
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
            dw E_UNIDAD, m_e_unidad
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

; ---- DIR [ruta o patrón] [/P] [/W]: /P se para cada pantalla; /W, solo los
;      nombres, cinco por línea ----
o_dir:
        mov word [dir_p], 0         ; (dir_p y dir_w)
        mov byte [dir_lin], 0
        mov byte [arg1], 0
.a:
        mov di, arg2
        call toma_arg
        jz .af
        cmp byte [arg2], '/'
        jne .ruta
        mov al, [arg2+1]
        and al, 0xDF
        cmp al, 'P'
        jne .a1
        mov byte [dir_p], 1
.a1:
        cmp al, 'W'
        jne .a
        mov byte [dir_w], 1
        jmp .a
.ruta:
        push si
        mov si, arg2
        mov di, arg1
        call copia_txt
        pop si
        jmp .a
.af:
        cmp byte [arg1], 0
        jne .hay
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
        cmp byte [dir_w], 0
        je .largo
        ; a lo ancho: el nombre en 16 columnas (los directorios, entre
        ; corchetes), cinco por línea
        mov cx, 16
        test byte [dta_int+21], 0x10
        jz .w1
        mov al, '['
        call pon_car
        dec cx
.w1:
        mov si, dta_int + 30
.w2:
        lodsb
        test al, al
        jz .w3
        call pon_car
        dec cx
        jmp .w2
.w3:
        test byte [dta_int+21], 0x10
        jz .w4
        mov al, ']'
        call pon_car
        dec cx
.w4:
        test byte [dta_int+21], 0x10
        jnz .w6
        mov ax, [dta_int+26]
        mov dx, [dta_int+28]
        add [d_total], ax
        adc [d_total+2], dx
.w6:
        call .quinto                ; el quinto de la línea: sin relleno (con
        jz .w7                      ; él se llegaría a la columna 80 y la
        mov al, ' '                 ; pantalla saltaría sola de línea)
.w5:
        call pon_car
        loop .w5
        jmp .sig
.w7:
        call .nl
        jmp .sig
; (interna) ZF=1 si el archivo que se lleva es el último de su línea (/W)
.quinto:
        mov ax, [d_n]
        inc ax
        xor dx, dx
        mov bx, 5
        div bx
        test dx, dx
        ret
.largo:
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
        jmp short .fecha
.arch:
        mov ax, [dta_int+26]
        mov dx, [dta_int+28]
        add [d_total], ax
        adc [d_total+2], dx
        mov cx, 10
        call pon_num32_ancho
.fecha:
        mov al, ' '                 ; su fecha y su hora
        call pon_car
        call pon_car
        mov ax, [dta_int+24]
        and al, 0x1F
        call pon_2
        mov al, '-'
        call pon_car
        mov ax, [dta_int+24]
        mov cl, 5
        shr ax, cl
        and al, 0x0F
        call pon_2
        mov al, '-'
        call pon_car
        mov ax, [dta_int+24]
        mov cl, 9
        shr ax, cl
        add ax, 1980
        call pon_num
        mov al, ' '
        call pon_car
        mov ax, [dta_int+22]
        mov cl, 11
        shr ax, cl
        call pon_2
        mov al, ':'
        call pon_car
        mov ax, [dta_int+22]
        mov cl, 5
        shr ax, cl
        and al, 0x3F
        call pon_2
        call .nl
.sig:
        inc word [d_n]
        DOS 0x4F
        jnc .uno
        cmp byte [dir_w], 0         ; (a lo ancho: la última línea, a medias)
        je .fin
        mov ax, [d_n]
        xor dx, dx
        mov bx, 5
        div bx
        test dx, dx
        jz .fin
        call pon_nl
        jmp short .fin
; (interna) línea nueva; con /P, cada 23 se espera una tecla
.nl:
        call pon_nl
        cmp byte [dir_p], 0
        je .nl1
        inc byte [dir_lin]
        cmp byte [dir_lin], 23
        jb .nl1
        mov byte [dir_lin], 0
        push si
        mov si, m_pausa
        call pon_cad
        DOS 0x08
        call pon_nl
        pop si
.nl1:
        ret
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
        xor dl, dl                  ; lo libre, de la unidad que se ha
        cmp byte [arg1+1], ':'      ; listado: la de la ruta, o la actual
        jne .libre
        mov dl, [arg1]
        and dl, 0xDF
        sub dl, 'A' - 1
.libre:
        DOS 0x36                    ; grupos x sectores x 512
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

; ---- COPIAR origen [destino] y MOVER origen [destino] ----
; El origen puede llevar comodines o ser un directorio (todo lo que tiene); el
; destino, un directorio (cada archivo conserva su nombre), un nombre, o un
; nombre con comodines («COPIAR *.TXT *.BAK»): lo que el destino no dice se
; toma del origen. Sin destino, el directorio de trabajo. Cualquiera de los
; dos puede ser un dispositivo: «COPIAR CON NOTA.TXT» (se acaba con Ctrl+Z e
; Intro), «COPIAR NOTA.TXT PRN». MOVER cambia el archivo de directorio sin
; copiarlo; si el destino está en otra unidad, lo copia y borra el original
o_mover:
        mov byte [cp_modo], 1
        jmp short cp_comun
o_copiar:
        mov byte [cp_modo], 0
cp_comun:
        mov di, arg1
        call toma_arg
        jnz .hay
        mov si, m_e_copiar
        jmp pon_cad
.hay:
        mov di, arg2
        call toma_arg
        mov dx, arg1                ; un directorio: lo que tiene dentro
        call es_directorio
        jne .o1
        mov di, arg1
        call al_final
        mov al, '\'
        stosb
        mov si, t_todos
        call copia_txt
.o1:
        ; el destino, en dos: el directorio (cp_dir) y el nombre (cp_pat)
        mov si, arg2
        mov di, cp_dir
        call copia_txt
        cmp byte [arg2], 0
        je .dir
        mov al, [di-1]
        cmp al, '\'
        je .dir
        cmp al, ':'
        je .dir
        mov dx, arg2
        call es_directorio
        jne .nombre
        mov ax, '\'
        stosw
.dir:
        mov si, t_todos
        mov di, cp_pat
        call copia_txt
        jmp short .listo
.nombre:
        mov si, cp_dir
        call nombre_de
        push si
        mov di, cp_pat
        call copia_txt
        pop si
        mov byte [si], 0
.listo:
        cmp byte [cp_modo], 0
        jne .va
        call borr_pide
        jc cp_fin.x
.va:
        mov word [cp_n], 0
        mov si, arg1
        call con_comodin
        je cp_varios
        mov si, arg1                ; uno solo: él mismo
        mov di, cp_org
        call copia_txt
        mov si, arg1
        call nombre_de
        call cp_este
; (común) dice cuántos han sido
cp_fin:
        mov ax, [cp_n]
        cmp ax, 1
        jne .n
        mov si, m_copiado
        cmp byte [cp_modo], 0
        je .d
        mov si, m_movido
        jmp short .d
.n:
        cmp byte [cp_modo], 2       ; (RENOMBRAR no dice nada)
        je .x
        call pon_num
        mov si, m_copiados
        cmp byte [cp_modo], 0
        je .d
        mov si, m_movidos
.d:
        cmp byte [cp_modo], 2
        je .x
        call pon_cad
.x:
        ret
; (común) con comodines: cada archivo que case con arg1
cp_varios:
        mov dx, cp_dta
        DOS 0x1A
        mov dx, arg1
        xor cx, cx
        DOS 0x4E
        jnc .uno
        call pon_error
        jmp short .fin
.uno:
        mov si, arg1                ; su ruta: la del patrón, con su nombre
        mov di, cp_org
        call copia_txt
        mov si, cp_org
        call nombre_de
        mov di, si
        mov si, cp_dta + 30
        call copia_txt
        cmp byte [cp_modo], 2
        je .u1
        mov si, cp_org
        call pon_cad
        call pon_nl
.u1:
        mov si, cp_dta + 30
        call cp_este
        DOS 0x4F
        jnc .uno
.fin:
        mov dx, dta_int
        DOS 0x1A
        jmp cp_fin

; (interna) el archivo de cp_org, que se llama como dice SI: se copia, se
; mueve o cambia de nombre ([cp_modo]: 0, 1, 2) a cp_dir + su nombre nuevo
cp_este:
        mov di, fcb1                ; el nombre nuevo: el del destino, y donde
        call a_fcb                  ; tiene comodines, lo del origen
        mov si, cp_pat
        mov di, fcb2
        call a_fcb
        xor bx, bx
.m:
        cmp byte [fcb2+bx], '?'
        jne .m1
        mov al, [fcb1+bx]
        mov [fcb2+bx], al
.m1:
        inc bx
        cmp bx, 11
        jb .m
        mov si, cp_dir
        mov di, cp_dst
        call copia_txt
        mov si, fcb2
        call de_fcb
        mov si, cp_org              ; ¿sobre sí mismo?
        mov di, cp_dst
.i:
        lodsb
        call mayus
        mov ah, al
        mov al, [di]
        call mayus
        inc di
        cmp al, ah
        jne .dist
        test al, al
        jnz .i
        mov si, m_e_mismo
        jmp pon_cad
.dist:
        cmp byte [cp_modo], 0
        je .copia
        mov dx, cp_org              ; mover o renombrar: solo cambia la entrada
        mov di, cp_dst
        DOS 0x56
        jnc .hecho
        cmp byte [cp_modo], 1
        jne .err
        cmp ax, E_OTRO_DISP         ; a otra unidad: copiar y borrar
        jne .err
        cmp word [borr_seg], 0
        jne .c1
        call borr_pide
        jc .x
.c1:
        call copia_arch
        jc .x
        mov dx, cp_org
        DOS 0x41
        jnc .hecho
.err:
        jmp pon_error
.copia:
        call copia_arch
        jc .x
.hecho:
        inc word [cp_n]
.x:
        ret

; copia el archivo cp_org a cp_dst. CF=1 (y lo dice) si no ha podido
copia_arch:
        mov dx, cp_org
        mov al, 0
        DOS 0x3D
        jnc .abierto
        call pon_error
        stc
        ret
.abierto:
        mov [o_h1], ax
        mov dx, cp_dst
        xor cx, cx
        DOS 0x3C
        jnc .creado
        push ax
        mov bx, [o_h1]
        DOS 0x3E
        pop ax
        call pon_error
        stc
        ret
.creado:
        mov [o_h2], ax
        mov byte [cp_mal], 0
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
        jz .cierra
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
        jmp short .m1
.mal:
        call pon_error
.m1:
        mov byte [cp_mal], 1
.cierra:
        mov bx, [o_h1]              ; la copia, con la fecha del original
        mov ax, 0x5700
        int 0x21
        jc .c0
        mov bx, [o_h2]
        mov ax, 0x5701
        int 0x21
.c0:
        mov bx, [o_h1]
        DOS 0x3E
        mov bx, [o_h2]
        DOS 0x3E
        jnc .c1
        call pon_error
        mov byte [cp_mal], 1
.c1:
        cmp byte [cp_mal], 1        ; CF=1 si [cp_mal] = 0... al revés:
        cmc                         ; CF=1 si ha ido mal
        ret

; el nombre de SI («CARTA.TXT», «*.BAK») -> en DI, como en el disco: once
; letras, con «?» donde vale cualquiera. Conserva los registros
a_fcb:
        push ax
        push cx
        push si
        push di
        push di
        mov al, ' '
        mov cx, 11
        rep stosb
        pop di
        mov cx, 8
.n:
        lodsb
        cmp al, ' '
        jbe .x
        cmp al, '.'
        je .e
        cmp al, '*'
        je .na
        jcxz .n
        call mayus
        stosb
        dec cx
        jmp .n
.na:
        mov al, '?'
        rep stosb
        jmp .n
.e:
        add di, cx
        mov cx, 3
.e1:
        lodsb
        cmp al, ' '
        jbe .x
        cmp al, '*'
        je .ea
        jcxz .e1
        call mayus
        stosb
        dec cx
        jmp .e1
.ea:
        mov al, '?'
        rep stosb
        jmp .e1
.x:
        pop di
        pop si
        pop cx
        pop ax
        ret
; las once letras de SI -> «NOMBRE.EXT», acabado en 0, en DI
de_fcb:
        push ax
        push cx
        mov cx, 8
.n:
        lodsb
        cmp al, ' '
        je .n1
        stosb
.n1:
        loop .n
        cmp byte [si], ' '
        je .f
        mov al, '.'
        stosb
        mov cx, 3
.e:
        lodsb
        cmp al, ' '
        je .e1
        stosb
.e1:
        loop .e
.f:
        mov byte [di], 0
        pop cx
        pop ax
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
        mov si, arg1                ; «*.*»: todos; se pregunta antes
        call nombre_de
        cmp word [si], '*.'
        jne .v1
        cmp word [si+2], '*'
        jne .v1
        mov si, m_seguro
        call pon_cad
        DOS 0x01
        call pon_nl
        and al, 0xDF
        cmp al, 'S'
        je .v1
        ret
.v1:
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
        mov si, arg1                ; con comodines en alguno de los dos:
        call con_comodin            ; cada archivo que case, con el nombre que
        je .varios                  ; salga de mezclarlo con el nuevo
        mov si, arg2
        call con_comodin
        je .varios
        mov dx, arg1
        mov di, arg2
        DOS 0x56
        jc pon_error
        ret
.varios:
        mov byte [cp_modo], 2
        mov word [cp_n], 0
        mov si, arg1                ; se quedan en su directorio
        mov di, cp_dir
        call copia_txt
        mov si, cp_dir
        call nombre_de
        mov byte [si], 0
        mov si, arg2
        call nombre_de
        mov di, cp_pat
        call copia_txt
        jmp cp_varios

; ---- CD [directorio]: sin nada, dice dónde se está ----
o_cd:
        mov di, arg1
        call toma_arg
        jnz .va
        call trae_dir
        call pon_unidad
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

; ---- IF [NOT] EXIST archivo orden / IF [NOT] ERRORLEVEL n orden /
;      IF [NOT] a==b orden ----
o_if:
        xor bp, bp                  ; BP=1: al revés (NOT)
        mov di, t_not
        call palabra
        je .no
        mov di, t_no
        call palabra
        jne .c
.no:
        inc bp
.c:
        mov di, t_exist
        call palabra
        je .existe
        mov di, t_existe
        call palabra
        je .existe
        mov di, t_errorlevel
        call palabra
        je .nivel
        mov di, t_nivel
        call palabra
        je .nivel
        ; a==b: se comparan letra a letra
        call salta_esp
        mov di, si
.i1:
        mov al, [si]
        cmp al, ' '
        jbe .mal
        cmp al, '='
        je .i2
        inc si
        jmp .i1
.i2:
        cmp byte [si+1], '='
        jne .mal
        mov cx, si
        sub cx, di                  ; el largo de «a»
        add si, 2
        mov bx, 1                   ; BX=1: son iguales
.i3:
        jcxz .i4
        mov al, [si]
        cmp al, [di]
        jne .i5
        inc si
        inc di
        dec cx
        jmp .i3
.i4:
        cmp byte [si], ' '          ; (y «b» no sigue)
        jbe .hecho
.i5:
        xor bx, bx
.i6:
        cmp byte [si], ' '          ; hasta el final de «b»
        jbe .hecho
        inc si
        jmp .i6
.existe:
        mov di, arg1
        call toma_arg
        jz .mal
        push si
        mov dx, arg1
        mov cx, 0x10
        DOS 0x4E
        pop si
        mov bx, 1
        jnc .hecho
        xor bx, bx
        jmp short .hecho
.nivel:
        call salta_esp
        xor dx, dx
.n1:
        mov al, [si]
        sub al, '0'
        cmp al, 9
        ja .n2
        mov ah, 10
        xchg ax, dx
        mul ah
        xchg ax, dx
        add dl, al
        inc si
        jmp .n1
.n2:
        DOS 0x4D                    ; el código del último programa: ¿es ese o más?
        mov bx, 1
        cmp al, dl
        jae .hecho
        xor bx, bx
.hecho:
        xor bx, bp
        jz .x
        jmp ejecuta_linea           ; se cumple: lo que queda de la línea
.mal:
        mov si, m_e_if
        jmp pon_cad
.x:
        ret
; (interna) ¿empieza SI (tras los espacios) por la palabra de DI, sin mirar
; mayúsculas? ZF=1 y SI detrás, si es así
palabra:
        push ax
        push bx
        call salta_esp
        mov bx, si
.c:
        mov ah, [di]
        test ah, ah
        jz .fin
        mov al, [si]
        call mayus
        cmp al, ah
        jne .no
        inc si
        inc di
        jmp .c
.fin:
        cmp byte [si], ' '          ; (palabra entera)
        ja .no
        cmp al, al
        jmp short .x
.no:
        mov si, bx
        or bx, bx                   ; ZF=0 (BX nunca es 0 aquí)
.x:
        pop bx
        pop ax
        ret

; ---- FOR %v IN (lista) DO orden / PARA %v EN (lista) HAZ orden ----
; La orden se ejecuta una vez por cada elemento de la lista (separados por
; espacios o comas), con «%v» cambiado por él; un elemento con comodines vale
; por todos los archivos que casen (con la ruta que lleve delante). Las
; búsquedas van a una zona propia, para que la orden pueda hacer las suyas.
; Como en DOS, un FOR no puede llevar otro dentro
o_for:
        cmp byte [for_dentro], 0
        je .va
        mov si, m_e_for2
        jmp pon_cad
.va:
        call salta_esp
        cmp byte [si], '%'
        jne .mal
        mov al, [si+1]
        cmp al, ' '
        jbe .mal
        mov [for_var], al
        add si, 2
        mov di, t_in
        call palabra
        je .p1
        mov di, t_en
        call palabra
        jne .mal
.p1:
        call salta_esp
        cmp byte [si], '('
        jne .mal
        inc si
        mov di, for_lista           ; la lista, hasta el paréntesis
.l:
        lodsb
        test al, al
        jz .mal
        cmp al, ')'
        je .lf
        stosb
        jmp .l
.mal:
        mov si, m_e_for
        jmp pon_cad
.lf:
        mov byte [di], 0
        mov di, t_do
        call palabra
        je .p2
        mov di, t_haz
        call palabra
        jne .mal
.p2:
        call salta_esp
        mov di, for_orden           ; la orden, aparte: la línea se va a pisar
        call copia_txt
        mov byte [for_dentro], 1
        mov word [for_pos], for_lista
.elem:
        mov si, [for_pos]
.sep:
        mov al, [si]
        cmp al, ' '
        je .s1
        cmp al, ','
        je .s1
        cmp al, ';'
        jne .s2
.s1:
        inc si
        jmp .sep
.s2:
        test al, al
        jnz .e0
        mov byte [for_dentro], 0    ; se acabó la lista
        ret
.e0:
        mov di, for_elem
        mov cx, RUTA_MAX - 1
.e1:
        mov al, [si]
        cmp al, ' '
        jbe .e2
        cmp al, ','
        je .e2
        cmp al, ';'
        je .e2
        inc si
        jcxz .e1
        stosb
        dec cx
        jmp .e1
.e2:
        mov byte [di], 0
        mov [for_pos], si
        mov si, for_elem
        call con_comodin
        je .varios
        mov si, for_elem            ; sin comodines: él mismo
        mov di, for_valor
        call copia_txt
        call .corre
        jmp .elem
.varios:
        mov dx, for_dta             ; con comodines: cada archivo que case
        DOS 0x1A
        mov dx, for_elem
        xor cx, cx
        DOS 0x4E
.v1:
        pushf
        mov dx, dta_int
        DOS 0x1A
        popf
        jc .elem
        mov si, for_elem            ; su ruta: la del patrón, con su nombre
        mov di, for_valor
        call copia_txt
        mov si, for_valor
        call nombre_de
        mov di, si
        mov si, for_dta + 30
        call copia_txt
        call .corre
        mov dx, for_dta
        DOS 0x1A
        DOS 0x4F
        jmp .v1
; (interna) ejecuta la orden con «%v» cambiado por for_valor
.corre:
        mov si, for_orden
        mov di, for_lin
        mov cx, LIN_MAX
.c:
        lodsb
        test al, al
        jz .cf
        cmp al, '%'
        jne .cp
        mov ah, [si]
        cmp ah, [for_var]
        jne .cp
        inc si
        push si
        mov si, for_valor
.cv:
        lodsb
        test al, al
        jz .cvf
        jcxz .cv
        stosb
        dec cx
        jmp .cv
.cvf:
        pop si
        jmp .c
.cp:
        jcxz .c
        stosb
        dec cx
        jmp .c
.cf:
        mov byte [di], 0
        mov si, for_lin
        jmp ejecuta_linea

; ---- LLAMAR orden (CALL): si la orden es otro lote, al acabar se vuelve a
;      este. El lote que llama (dónde está, por dónde va y sus parámetros) se
;      guarda en un bloque de memoria; [lote_ant] dice cuál, y dentro va el
;      que hubiera antes: los lotes llamados se apilan ----
o_call:
        cmp word [lote_seg], 0
        jne .lote
        jmp ejecuta_linea           ; desde el teclado: la orden, sin más
.lote:
        push si
        mov bx, (LOTE_EST_TAM + 15) / 16
        DOS 0x48
        pop si
        jnc .hay
        mov si, m_e_grande_lote
        jmp pon_cad
.hay:
        push si
        push es
        mov es, ax
        xor di, di
        mov si, lote_est
        mov cx, LOTE_EST_TAM
        rep movsb
        pop es
        pop si
        mov [lote_ant], ax
        mov word [lote_seg], 0
        call ejecuta_linea
        cmp word [lote_seg], 0      ; si no era un lote, ya ha acabado: se
        jne .x                      ; sigue con el que llamaba
        call lote_vuelve
.x:
        ret

; ---- DESPLAZAR (SHIFT): %1 pasa a ser %0, %2 a ser %1... ----
o_shift:
        mov si, lote_args
        call salta_esp
        mov di, lote_nom
        mov cx, LOTE_NOM_TAM - 1
.n:
        mov al, [si]
        cmp al, ' '
        jbe .nf
        inc si
        jcxz .n
        stosb
        dec cx
        jmp .n
.nf:
        mov byte [di], 0
        mov di, lote_args
        jmp copia_txt

; ---- GOTO etiqueta: sigue por la línea «:etiqueta» del lote ----
o_goto:
        cmp word [lote_seg], 0
        je .x
        push si                     ; «IR A etiqueta»: la «A» sobra si detrás
        mov di, t_a                 ; viene la etiqueta
        call palabra
        jne .sin_a
        call salta_esp
        cmp byte [si], ' '
        jbe .sin_a
        add sp, 2
        jmp short .et
.sin_a:
        pop si
.et:
        mov di, arg1
        call toma_arg
        jz .x
        mov bx, arg1
        cmp byte [bx], ':'
        jne .b0
        inc bx
.b0:
        push es
        mov es, [lote_seg]
        xor si, si
.lin:
        cmp si, [lote_fin]
        jae .no
        cmp byte [es:si], ':'
        jne .sig
        inc si
        mov di, bx                  ; ¿es esta?
.c:
        mov ah, [di]
        test ah, ah
        jz .fin_et
        mov al, [es:si]
        call mayus
        xchg al, ah
        call mayus
        cmp al, ah
        jne .sig
        inc si
        inc di
        jmp .c
.fin_et:
        cmp si, [lote_fin]
        jae .esta
        cmp byte [es:si], ' '
        ja .sig
.esta:
        mov [lote_pos], si          ; (lo que queda de su línea no hace nada)
        pop es
        ret
.sig:
        cmp si, [lote_fin]          ; a la línea siguiente
        jae .no
        mov al, [es:si]
        inc si
        cmp al, 10
        jne .sig
        jmp .lin
.no:
        pop es
        mov si, m_e_goto
        call pon_cad
        call lote_cierra
.x:
        ret
o_pausa:
        mov si, m_pausa
        call pon_cad
        DOS 0x08
        jmp pon_nl

; =============================================================================
;  El entorno: PON (SET), RUTA (PATH), INDICADOR (PROMPT)
; =============================================================================
; El entorno son las variables («NOMBRE=valor», cada una acabada en 0, y otro
; 0 al final) que hereda cada programa que se lanza. El que nos da el núcleo
; viene justo de tamaño, así que al empezar se pasa a un bloque propio de
; ENT_TAM octetos, donde pueden crecer
ent_ini:
        mov bx, ENT_TAM / 16
        DOS 0x48
        jc .x
        mov [ent_seg], ax
        push ds
        mov es, ax
        mov ds, [0x2C]
        xor si, si
        xor di, di
.c:
        lodsb
        stosb
        test al, al
        jnz .c
        cmp byte [si], 0            ; hasta los dos ceros seguidos
        jne .c
        stosb
        push ds
        pop es
        pop ds
        DOS 0x49                    ; el que traíamos ya no hace falta
        mov ax, [ent_seg]
        mov [0x2C], ax
        push cs
        pop es
        ret
.x:
        mov ax, [0x2C]              ; (sin memoria: el que hay, sin crecer)
        mov [ent_seg], ax
        ret
; busca la variable de nombre SI (en mayúsculas) -> ES:DI = su valor (ES es
; el entorno: quien llama lo repone). CF=1 si no está. Conserva SI
ent_busca:
        mov es, [ent_seg]
        xor di, di
.v:
        cmp byte [es:di], 0
        je .no
        push si
.c:
        lodsb
        test al, al
        jz .fin_nom
        cmp al, [es:di]
        jne .otra
        inc di
        jmp .c
.fin_nom:
        cmp byte [es:di], '='
        jne .otra
        pop si
        inc di
        clc
        ret
.otra:
        pop si
.s:
        inc di
        cmp byte [es:di-1], 0
        jne .s
        jmp .v
.no:
        stc
        ret
; pone la variable de nombre SI con el valor de DX (vacío: la quita).
; Conserva los registros; deja ES = el nuestro
ent_pon:
        push ax
        push bx
        push cx
        push si
        push di
        call ent_busca
        jc .nueva
        mov bx, di                  ; la que había, fuera: lo de detrás se
.i:
        dec bx                      ; corre a su sitio
        test bx, bx
        jz .i1
        cmp byte [es:bx-1], 0
        jne .i
.i1:
.f:
        inc di                      ; DI = detrás de su 0
        cmp byte [es:di-1], 0
        jne .f
        mov cx, di
.g:
        cmp byte [es:di], 0         ; hasta el final (el 0 que sobra)
        je .g2
.g1:
        inc di
        cmp byte [es:di-1], 0
        jne .g1
        jmp .g
.g2:
        sub di, cx
        xchg cx, di                 ; CX = cuánto, DI = desde dónde
        inc cx
        push si
        push ds
        push es
        pop ds
        mov si, di
        mov di, bx
        rep movsb
        pop ds
        pop si
.nueva:
        mov bx, dx
        cmp byte [bx], 0
        je .x
        xor di, di                  ; al final
.e:
        cmp byte [es:di], 0
        je .e2
.e1:
        inc di
        cmp byte [es:di-1], 0
        jne .e1
        jmp .e
.e2:
        push si                     ; ¿cabe?
        mov cx, di
.l1:
        lodsb
        inc cx
        test al, al
        jnz .l1
        mov si, dx
.l2:
        lodsb
        inc cx
        test al, al
        jnz .l2
        pop si
        cmp cx, ENT_TAM - 2
        jb .cabe
        push cs
        pop es
        mov si, m_e_entorno
        call pon_cad
        jmp short .x
.cabe:
        lodsb
        test al, al
        jz .v0
        stosb
        jmp .cabe
.v0:
        mov al, '='
        stosb
        mov si, dx
.v1:
        lodsb
        stosb
        test al, al
        jnz .v1
        stosb                       ; y el 0 del final
.x:
        push cs
        pop es
        pop di
        pop si
        pop cx
        pop bx
        pop ax
        ret

; ---- PON (SET): sin nada, las enseña; PON NOMBRE=valor; PON NOMBRE= la quita ----
o_pon:
        call salta_esp
        cmp byte [si], 0
        jne .pon
        mov es, [ent_seg]
        xor di, di
.v:
        cmp byte [es:di], 0
        je .fin
.c:
        mov al, [es:di]
        inc di
        test al, al
        jz .nl
        call pon_car
        jmp .c
.nl:
        call pon_nl
        jmp .v
.fin:
        push cs
        pop es
        ret
.pon:
        mov di, arg1                ; el nombre, en mayúsculas, hasta el «=»
.n:
        lodsb
        test al, al
        jz .mal
        cmp al, '='
        je .nf
        call mayus
        stosb
        jmp .n
.nf:
        mov byte [di], 0
        mov dx, si
        mov si, arg1
        jmp ent_pon
.mal:
        mov si, m_e_pon
        jmp pon_cad
; ---- RUTA (PATH): sin nada, la dice; RUTA dir;dir la pone; RUTA ; la quita ----
o_ruta:
        mov bx, t_path
        jmp short var_orden
; ---- INDICADOR (PROMPT) texto: sin nada, el de siempre ----
o_indicador:
        mov bx, t_prompt
var_orden:
        call salta_esp
        cmp bx, t_prompt
        je .pon
        cmp byte [si], 0
        jne .may
        mov si, bx                  ; RUTA a secas: cuál es
        call ent_busca
        jc .no
.c:
        mov al, [es:di]
        inc di
        test al, al
        jz .nl
        call pon_car
        jmp .c
.no:
        mov si, m_sin_ruta
        push cs
        pop es
        jmp pon_cad
.nl:
        push cs
        pop es
        jmp pon_nl
.may:
        push si                     ; (las rutas, en mayúsculas)
.m:
        mov al, [si]
        call mayus
        mov [si], al
        inc si
        test al, al
        jnz .m
        pop si
        cmp word [si], ';'          ; «;» a secas: ninguna
        jne .pon
        inc si
.pon:
        mov dx, si
        mov si, bx
        jmp ent_pon

; =============================================================================
;  FECHA, HORA, VOL, VERIFICAR, MEMORIA
; =============================================================================
; escribe AL con dos cifras
pon_2:
        push ax
        aam
        add ax, '00'
        xchg al, ah
        call pon_car
        mov al, ah
        call pon_car
        pop ax
        ret
; escribe la fecha: 16-11-2026
pon_fecha:
        push ax
        push cx
        push dx
        DOS 0x2A
        mov al, dl
        call pon_2
        mov al, '-'
        call pon_car
        mov al, dh
        call pon_2
        mov al, '-'
        call pon_car
        mov ax, cx
        call pon_num
        pop dx
        pop cx
        pop ax
        ret
; escribe la hora: 19:45:10
pon_hora:
        push ax
        push cx
        push dx
        DOS 0x2C
        mov al, ch
        call pon_2
        mov al, ':'
        call pon_car
        mov al, cl
        call pon_2
        mov al, ':'
        call pon_car
        mov al, dh
        call pon_2
        pop dx
        pop cx
        pop ax
        ret
; el número de SI -> AX; SI queda detrás de él y del separador que le siga.
; CF=1 si no hay número
toma_num:
        xor ax, ax
        mov bl, [si]
        sub bl, '0'
        cmp bl, 9
        ja .no
.c:
        mov bl, [si]
        sub bl, '0'
        cmp bl, 9
        ja .f
        mov cx, 10
        mul cx
        xor bh, bh
        add ax, bx
        inc si
        jmp .c
.f:
        cmp byte [si], ' '
        jbe .x
        inc si                      ; el separador: - / : .
.x:
        clc
        ret
.no:
        stc
        ret
; (común a FECHA y HORA) sin nada escrito, se pregunta: SI = lo tecleado
pide_dato:
        call salta_esp
        cmp byte [si], 0
        jne .x
        mov si, dx
        call pon_cad
        mov byte [lin_buf], 20
        mov dx, lin_buf
        DOS 0x0A
        mov bl, [lin_buf+1]
        xor bh, bh
        mov byte [lin_buf+2+bx], 0
        call pon_nl
        mov si, lin_buf + 2
        call salta_esp
.x:
        ret
; ---- FECHA [dd-mm-aaaa] (DATE) ----
o_fecha:
        call salta_esp
        cmp byte [si], 0
        jne .pon
        push si
        mov si, m_fecha
        call pon_cad
        call pon_fecha
        call pon_nl
        pop si
        mov dx, m_fecha_nueva
        call pide_dato
        cmp byte [si], 0
        je .x
.pon:
        call toma_num
        jc .mal
        mov [d_n], al               ; el día
        call toma_num
        jc .mal
        mov [d_n+1], al             ; el mes
        call toma_num
        jc .mal
        cmp ax, 100                 ; (con dos cifras: de 1980 a 2079)
        jae .a
        add ax, 1900
        cmp ax, 1980
        jae .a
        add ax, 100
.a:
        mov cx, ax
        mov dx, [d_n]
        DOS 0x2B
        test al, al
        jz .x
.mal:
        mov si, m_e_fecha
        jmp pon_cad
.x:
        ret
; ---- HORA [hh:mm[:ss]] (TIME) ----
o_hora:
        call salta_esp
        cmp byte [si], 0
        jne .pon
        push si
        mov si, m_hora
        call pon_cad
        call pon_hora
        call pon_nl
        pop si
        mov dx, m_hora_nueva
        call pide_dato
        cmp byte [si], 0
        je .x
.pon:
        call toma_num
        jc .mal
        mov [d_n+1], al
        call toma_num
        jc .mal
        mov [d_n], al
        call toma_num               ; (los segundos pueden faltar)
        mov dh, al
        xor dl, dl
        mov cx, [d_n]
        DOS 0x2D
        test al, al
        jz .x
.mal:
        mov si, m_e_hora
        jmp pon_cad
.x:
        ret
; ---- VOL [unidad:]: cómo se llama el disco ----
o_vol:
        mov di, arg1
        call toma_arg
        mov si, t_todos             ; se busca la etiqueta en su raíz
        mov di, arg2
        cmp byte [arg1+1], ':'
        jne .u
        mov ax, [arg1]
        stosw
.u:
        mov al, '\'
        stosb
        call copia_txt
        mov dx, arg2
        mov cx, 0x08
        DOS 0x4E
.b:
        jc .sin
        test byte [dta_int+21], 0x08
        jnz .esta
        DOS 0x4F
        jmp .b
.esta:
        mov si, m_vol
        call pon_cad
        mov si, dta_int + 30        ; (sin el punto que separa la extensión)
.c:
        lodsb
        test al, al
        jz .nl
        cmp al, '.'
        je .c
        call pon_car
        jmp .c
.sin:
        cmp ax, E_NO_MAS
        je .s1
        jmp pon_error
.s1:
        mov si, m_sin_vol
        jmp pon_cad
.nl:
        jmp pon_nl
; ---- VERIFICAR [ON | OFF] (VERIFY) ----
o_verificar:
        call salta_esp
        mov ax, [si]
        and ax, 0xDFDF
        cmp ax, 'ON'
        mov dl, 1
        je .pon
        cmp ax, 'OF'
        mov dl, 0
        je .pon
        DOS 0x54
        mov si, m_verif_on
        test al, al
        jnz .d
        mov si, m_verif_off
.d:
        jmp pon_cad
.pon:
        mov al, dl
        DOS 0x2E
        ret
; ---- MEMORIA (MEM): la convencional y, en un 286 con más de un mega, la
;      memoria alta (HMA) y la extendida, y quién las tiene ----
o_memoria:
        mov si, m_mem_total
        call pon_cad
        int 0x12
        call pon_num
        mov si, m_mem_libre
        call pon_cad
        mov bx, 0xFFFF
        DOS 0x48
        mov ax, bx                  ; párrafos, en octetos
        xor dx, dx
        mov cx, 4
.m:
        shl ax, 1
        rcl dx, 1
        loop .m
        call pon_num32
        mov si, m_octetos
        call pon_cad
        mov ax, 0xE806              ; lo de más allá del mega, según el núcleo
        xor bx, bx
        int 0x21
        test al, 1
        jnz .hay
        mov si, m_mem_sin
        jmp pon_cad
.hay:
        push ax
        push bx
        mov si, m_mem_hma
        call pon_cad
        mov si, m_hma_sis
        test al, 2
        jnz .h
        mov si, m_hma_prog
        test al, 4
        jnz .h
        mov si, m_hma_libre
.h:
        call pon_cad
        mov si, m_mem_ext
        call pon_cad
        pop ax                      ; (sin los 64 K de la HMA)
        sub ax, 64
        call pon_num
        mov si, m_mem_ext2
        call pon_cad
        mov ax, 0x4310              ; y la libre, que la dice el gestor
        int 0x2F
        mov [bloque], bx
        mov [bloque+2], es
        mov ah, 0x08
        call far [bloque]
        push cs
        pop es
        mov ax, dx
        call pon_num
        mov si, m_mem_ext3
        call pon_cad
        pop ax
        ret

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
        ; el nombre, entero (puede traer unidad y ruta: «C:\JUEGOS\AJEDREZ»),
        ; sin la extensión
        mov si, [pal_ini]
        mov di, orden_l
        xor bx, bx                  ; BX = donde está su último punto
        xor dx, dx                  ; DX = 1 si trae ruta
        mov cx, RUTA_MAX - 8
.n:
        mov al, [si]
        cmp al, ' '
        jbe .nf
        cmp al, '.'
        jne .n1
        mov bx, di
.n1:
        cmp al, '\'
        je .n2
        cmp al, ':'
        jne .n3
.n2:
        xor bx, bx                  ; (un punto de antes no era la extensión)
        mov dx, 1
.n3:
        stosb
        inc si
        loop .n
.nf:
        mov byte [di], 0
        test bx, bx
        jz .n4
        mov byte [bx], 0
.n4:
        mov [ej_ruta], dl
        test dx, dx                 ; con ruta, solo ahí
        jnz .cl3
        ; sin ruta: si es el nombre clásico de una orden del disco, su archivo
        ; lleva el español
        mov bx, clasicos
.cl:
        mov si, [bx]
        test si, si
        jz .cl3
        mov di, orden
.cl1:
        mov al, [si]
        cmp al, [di]
        jne .cl2
        inc si
        inc di
        test al, al
        jnz .cl1
        mov si, [bx+2]
        mov di, orden_l
        call copia_txt
        jmp short .cl3
.cl2:
        add bx, 4
        jmp .cl
.cl3:
        ; se busca en el directorio de trabajo y luego en los de la variable
        ; PATH (la pone RUTA), probando .COM, .EXE y .BAT en cada uno. Con
        ; ruta escrita, solo ahí
        mov word [ej_path], 0xFFFF  ; (aún no se ha mirado PATH)
        cmp byte [ej_ruta], 0
        je .c0
        mov word [ej_path], 0       ; (ni se mirará)
.c0:
        mov byte [ej_pref], 0
.pre:
        mov bx, extensiones
.i:
        mov si, [bx]
        test si, si
        jz .prefijo
        mov di, arg1
        mov si, ej_pref
        call copia_txt
        mov si, orden_l
        call copia_txt
        mov si, [bx]
        call copia_txt
        cmp word [bx], t_bat
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
        cmp ax, E_UNIDAD            ; (un directorio de PATH que no está)
        je .otro
        cmp ax, E_DISCO
        je .otro
        cmp ax, E_MEMORIA
        je .mem
        jmp pon_error
.mem:
        mov si, m_e_grande
        jmp pon_cad
.lote:
        mov si, arg1
        push bx
        call lote_abre
        pop bx
        jc .otro
        mov si, orden               ; %0 y lo de detrás, para %1 a %9
        mov di, lote_nom
        call copia_txt
        mov si, [resto]
        mov di, lote_args
        call copia_txt
        ret
.otro:
        add bx, 2
        jmp .i
.prefijo:
        ; el directorio siguiente de PATH -> ej_pref, acabado en barra
        mov di, [ej_path]
        test di, di
        jz .no
        cmp di, 0xFFFF
        jne .p1
        mov si, t_path
        call ent_busca
        jc .no_es
.p1:
        mov es, [ent_seg]
        mov si, ej_pref
        mov cx, RUTA_MAX - 2
.p2:
        mov al, [es:di]
        test al, al
        jz .p3
        inc di
        cmp al, ';'
        je .p3
        jcxz .p2
        mov [si], al
        inc si
        dec cx
        jmp .p2
.p3:
        test al, al                 ; (si era el último, ya no hay más)
        jnz .p4
        xor di, di
.p4:
        mov [ej_path], di
        push cs
        pop es
        cmp si, ej_pref
        je .prefijo                 ; (uno vacío: el siguiente)
        mov al, [si-1]
        cmp al, '\'
        je .p5
        cmp al, ':'
        je .p5
        mov byte [si], '\'
        inc si
.p5:
        mov byte [si], 0
        jmp .pre
.no_es:
        push cs
        pop es
.no:
        mov si, m_e_orden
        call pon_cad
.x:
        ret
            align 2
extensiones dw t_com, t_exe, t_bat, 0
clasicos    dw tc_format, tc_formatea, tc_sys, tc_sistema, tc_fdisk, tc_partir
            dw tc_more, tc_mas, tc_mas2, tc_mas, tc_sort, tc_ordenar
            dw tc_chkdsk, tc_revisar, tc_label, tc_etiqueta, tc_attrib, tc_atributo
            dw tc_tree, tc_arbol, tc_xcopy, tc_xcopiar, tc_keyb, tc_teclado
            dw tc_edit, tc_editar, 0
tc_format   db "FORMAT", 0
tc_formatea db "FORMATEA", 0
tc_sys      db "SYS", 0
tc_sistema  db "SISTEMA", 0
tc_fdisk    db "FDISK", 0
tc_partir   db "PARTIR", 0
tc_more     db "MORE", 0
tc_mas2     db "M", 0xA0, "S", 0    ; «más», con su tilde
tc_mas      db "MAS", 0
tc_sort     db "SORT", 0
tc_ordenar  db "ORDENAR", 0
tc_chkdsk   db "CHKDSK", 0
tc_revisar  db "REVISAR", 0
tc_label    db "LABEL", 0
tc_etiqueta db "ETIQUETA", 0
tc_attrib   db "ATTRIB", 0
tc_atributo db "ATRIBUTO", 0
tc_tree     db "TREE", 0
tc_arbol    db "ARBOL", 0
tc_xcopy    db "XCOPY", 0
tc_xcopiar  db "XCOPIAR", 0
tc_keyb     db "KEYB", 0
tc_teclado  db "TECLADO", 0
tc_edit     db "EDIT", 0
tc_editar   db "EDITAR", 0
t_exe       db ".EXE", 0
t_bat       db ".BAT", 0
t_autoexec  db "\AUTOEXEC.BAT", 0

; empieza a ejecutar el archivo por lotes de SI: lo carga entero en un bloque
; de memoria. CF=1 si no existe (o no cabe); entonces, si había otro lote en
; marcha, sigue como estaba
lote_abre:
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
        push ax
        mov bx, ax
        add bx, 15 + 16
        mov cl, 4
        shr bx, cl
        DOS 0x48
        pop bx
        jc .mal
        call lote_suelta            ; (si había otro, este ocupa su sitio)
        mov [lote_seg], ax
        mov [lote_fin], bx
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
; suelta la memoria del lote en marcha. Conserva los registros
lote_suelta:
        cmp word [lote_seg], 0
        je .x
        push ax
        push es
        mov es, [lote_seg]
        DOS 0x49
        pop es
        pop ax
        mov word [lote_seg], 0
.x:
        ret
; el lote se acaba: si lo había llamado otro (LLAMAR), se sigue con ese; si
; no, se vuelve al teclado
lote_cierra:
        call lote_suelta
        cmp word [lote_ant], 0
        jne lote_vuelve
        mov byte [eco], 1
        ret
; vuelve el lote que estaba guardado en [lote_ant] (y con él, el que tuviera
; guardado antes). Conserva los registros
lote_vuelve:
        push ax
        push cx
        push si
        push di
        mov ax, [lote_ant]
        test ax, ax
        jz .x
        push ds
        mov ds, ax
        xor si, si
        mov di, lote_est
        mov cx, LOTE_EST_TAM
        rep movsb
        pop ds
        push es
        mov es, ax
        DOS 0x49
        pop es
.x:
        pop di
        pop si
        pop cx
        pop ax
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
        cmp al, '%'
        je .pct
.pon:
        cmp cx, LIN_MAX
        jae .c
        mov [di], al
        inc di
        inc cx
        jmp .c
.pct:
        cmp si, [lote_fin]          ; «%%» es un «%»; «%n», el parámetro n
        jae .pon
        mov ah, [es:si]
        cmp ah, '%'
        jne .p1
        inc si
        jmp .pon
.p1:
        sub ah, '0'
        cmp ah, 9
        ja .pon
        inc si
        push si
        mov si, lote_nom
        test ah, ah
        jz .p4
        mov si, lote_args           ; el parámetro: la palabra número AH
.p2:
        cmp byte [si], ' '
        jne .p3
        inc si
        jmp .p2
.p3:
        dec ah
        jz .p4
.p3b:
        cmp byte [si], ' '          ; saltar esta palabra
        jbe .p2c
        inc si
        jmp .p3b
.p2c:
        cmp byte [si], 0
        jne .p2
.p4:
        mov al, [si]                ; copiarla
        cmp al, ' '
        jbe .p5
        cmp cx, LIN_MAX
        jae .p5
        mov [di], al
        inc di
        inc cx
        inc si
        jmp .p4
.p5:
        pop si
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
m_ver       db "Hispa-8086 versión 0.10", 10, 0
m_ayuda     db "Las órdenes de Hispa-8086 (entre paréntesis, el nombre clásico):", 10
            db "  AYUDA  VER  EQUIPO  MEMORIA (MEM)  LIMPIAR (CLS)  ACTUALIZAR", 10
            db "  DIR [ruta] [/P] [/W]   los archivos (LISTAR)    VOL  cómo se llama el disco", 10
            db "  MOSTRAR archivo        enseña un texto (TYPE)", 10
            db "  COPIAR origen destino  copia; valen * y ?, directorios, CON y PRN (COPY)", 10
            db "  MOVER origen destino   lo cambia de sitio (MOVE)", 10
            db "  BORRAR archivo (DEL)   RENOMBRAR viejo nuevo (REN)", 10
            db "  CD  CREARDIR (MD)  BORRARDIR (RD)            los directorios", 10
            db "  FECHA (DATE)  HORA (TIME)  VERIFICAR (VERIFY)", 10
            db "  PON (SET)  RUTA (PATH)  INDICADOR (PROMPT)   las variables", 10
            db "En los lotes: ECO, REM, PAUSA, SI (IF), IR A (GOTO), PARA (FOR),", 10
            db "  LLAMAR (CALL), DESPLAZAR (SHIFT). Detrás de una orden: > >> < |", 10
            db "Programas del disco:", 10
            db "  EDITAR archivo         el editor de texto", 10
            db "  FORMATEA (FORMAT)  SISTEMA (SYS)  PARTIR (FDISK)    preparar discos", 10
            db "  REVISAR (CHKDSK)  ETIQUETA (LABEL)  ATRIBUTO (ATTRIB)", 10
            db "  ARBOL (TREE)  XCOPIAR (XCOPY)  MAS (MORE)  ORDENAR (SORT)", 10
            db "  TECLADO (KEYB)  el teclado español     PASAR  archivos por el puerto serie", 10
            db "Cualquier otra palabra: el programa (.COM, .EXE) o el lote (.BAT) de ese nombre.", 10, 0
m_actualizar db "Los sistemas de la familia Hispa-DOS se actualizan entre sí:", 10
            db "  Hispa-8086  PC, XT y AT: 8086, 8088, V20, 80186 y 286", 10
            db "  Hispa-386   386 o superior, con el escritorio Hispa Ventanas", 10
            db "Las versiones y los discos, en https://hispados.es", 10, 0
m_archivos  db " archivos, ", 0
m_octetos   db " octetos", 10, 0
m_libres    db " octetos libres", 10, 0
m_dir       db "     <DIR>", 0
m_copiado   db "1 archivo copiado.", 10, 0
m_copiados  db " archivos copiados.", 10, 0
m_movido    db "1 archivo movido.", 10, 0
m_movidos   db " archivos movidos.", 10, 0
m_e_mismo   db "No se puede copiar un archivo sobre sí mismo.", 10, 0
m_seguro    db "Se van a borrar TODOS los archivos. ¿Seguro? (S/N) ", 0
m_e_entorno db "No queda sitio en el entorno.", 10, 0
m_e_pon     db "Se escribe así: PON NOMBRE=valor", 10, 0
m_sin_ruta  db "No hay ruta de búsqueda.", 10, 0
m_fecha     db "Fecha: ", 0
m_fecha_nueva db "Fecha nueva (dd-mm-aaaa), o Intro: ", 0
m_e_fecha   db "Esa fecha no vale.", 10, 0
m_hora      db "Hora: ", 0
m_hora_nueva db "Hora nueva (hh:mm:ss), o Intro: ", 0
m_e_hora    db "Esa hora no vale.", 10, 0
m_vol       db "El disco se llama ", 0
m_sin_vol   db "El disco no tiene etiqueta.", 10, 0
m_verif_on  db "VERIFICAR está puesto (ON).", 10, 0
m_verif_off db "VERIFICAR está quitado (OFF).", 10, 0
m_mem_total db "Memoria: ", 0
m_mem_libre db " K. Libre para los programas: ", 0
m_mem_sin   db "Sin memoria extendida (o sin un 286 que la aproveche).", 10, 0
m_mem_hma   db "Memoria alta (HMA), 64 K: ", 0
m_hma_sis   db "la usa el sistema (DOS=ALTO)", 10, 0
m_hma_prog  db "la tiene un programa", 10, 0
m_hma_libre db "libre", 10, 0
m_mem_ext   db "Memoria extendida: ", 0
m_mem_ext2  db " K. Libre: ", 0
m_mem_ext3  db " K", 10, 0
m_e_error   db "Error ", 0
m_e_disco   db "La unidad no está preparada.", 0
m_e_bios    db " (la BIOS dice: error ", 0
m_e_bios2   db "h)", 10, 0
m_e_unidad  db "No existe esa unidad.", 10, 0
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
m_e_copiar  db "Se escribe así: COPIAR origen [destino]", 10, 0
m_e_renombrar db "Se escribe así: RENOMBRAR viejo nuevo", 10, 0
m_pausa     db "Pulse una tecla para seguir...", 0
m_e_if      db "Se escribe así: SI [NO] EXISTE archivo orden, SI [NO] NIVEL n orden", 10
            db "o SI [NO] a==b orden (o IF [NOT] EXIST / ERRORLEVEL)", 10, 0
m_e_goto    db "El lote no tiene esa etiqueta.", 10, 0
m_e_for     db "Se escribe así: FOR %v IN (lista) DO orden", 10
            db "o PARA %v EN (lista) HAZ orden (en un lote, %%v)", 10, 0
m_e_for2    db "Un FOR no puede llevar otro dentro.", 10, 0
m_e_grande_lote db "No hay memoria para llamar a otro lote.", 10, 0
t_in        db "IN", 0
t_en        db "EN", 0
t_do        db "DO", 0
t_haz       db "HAZ", 0
t_not       db "NOT", 0
t_exist     db "EXIST", 0
t_errorlevel db "ERRORLEVEL", 0
t_no        db "NO", 0
t_existe    db "EXISTE", 0
t_nivel     db "NIVEL", 0
t_a         db "A", 0
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
r_sal       resw 1                  ; los archivos de la redirección
r_ent       resw 1
bloque      resw 7                  ; para lanzar un programa (4Bh)
orden       resb 10
            alignb 2
pal_ini     resw 1
orden_l     resb RUTA_MAX
cola        resb 130
; el lote en marcha: lo que LLAMAR guarda para poder volver a él
lote_est:
lote_seg    resw 1                  ; el archivo por lotes (0: no hay)
lote_pos    resw 1
lote_fin    resw 1
lote_ant    resw 1                  ; el lote que ha llamado a este, guardado
lote_nom    resb LOTE_NOM_TAM       ; %0
lote_args   resb 130                ; %1 a %9
LOTE_EST_TAM equ $ - lote_est
ent_seg     resw 1                  ; nuestro entorno
ej_path     resw 1                  ; ejecuta: por dónde va de PATH
cp_n        resw 1                  ; COPIAR: cuántos van
cp_modo     resb 1                  ; 0 copiar, 1 mover, 2 renombrar
cp_mal      resb 1
dir_p       resb 1                  ; DIR /P
dir_w       resb 1                  ; DIR /W (detrás de dir_p)
dir_lin     resb 1
ej_ruta     resb 1
fcb1        resb 11
fcb2        resb 11
cp_dta      resb 44
ej_pref     resb RUTA_MAX
cp_dir      resb RUTA_MAX + 16
cp_pat      resb 16
cp_org      resb RUTA_MAX + 16
cp_dst      resb RUTA_MAX + 16
            alignb 2
tubo_ent    resw 1                  ; la tubería: el archivo del que se lee...
tubo_sal    resw 1                  ; ...y en el que se escribe
tubo_hay    resb 1
for_dentro  resb 1                  ; FOR: se está ejecutando uno
for_var     resb 1                  ; la letra de su variable
            alignb 2
for_pos     resw 1                  ; por dónde va de la lista
for_dta     resb 44                 ; sus búsquedas
for_elem    resb RUTA_MAX           ; el elemento de la lista
for_valor   resb RUTA_MAX + 16      ; lo que vale la variable
for_lista   resb LIN_MAX + 2
for_orden   resb LIN_MAX + 2
for_lin     resb LIN_MAX + 2        ; la orden, con la variable cambiada
dta_int     resb 44                 ; lo que encuentran las búsquedas
dir_txt     resb 68
arg1        resb RUTA_MAX + 16
arg2        resb RUTA_MAX + 16
lin_buf     resb LIN_MAX + 6
pila_ini:
            resb 768
pila_fin:
