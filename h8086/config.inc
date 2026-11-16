; =============================================================================
;  Hispa-8086 - CONFIG.SYS
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Al arrancar, antes de lanzar el intérprete, el núcleo lee \CONFIG.SYS de la
;  unidad de arranque, si existe: un texto con una orden por línea.
;
;       FILES=n         cuántos archivos pueden estar abiertos a la vez (de 8,
;                       que es lo que hay sin decir nada, a 64). Cada uno son
;                       24 octetos de memoria.
;       BUFFERS=n       se admite y no hace nada: el núcleo tiene un búfer de
;                       sector y otro de FAT, y no más (ver el documento).
;       SHELL=ruta ...  el intérprete de órdenes que se lanza en vez de
;                       \COMMAND.COM, y lo que se le pasa detrás (para que
;                       ejecute AUTOEXEC.BAT hay que ponerle /P, como en DOS).
;       REM ...         un comentario. Las líneas vacías no cuentan.
;  Lo que no se entiende se dice y se sigue con la línea siguiente.
;
;  DEVICE= (los controladores de dispositivo) es de otra etapa. Encajaría
;  aquí: por cada línea, cargar el archivo en lo alto de la memoria del núcleo
;  (antes de calcular [mem_seg], como crece la tabla de archivos), enlazar su
;  cabecera en la cadena de dispositivos y llamar a su rutina de inicio, que
;  dice cuánto se queda.
; =============================================================================
config_lee:
        mov si, t_config
        mov di, ruta
.c:
        lodsb
        stosb
        test al, al
        jnz .c
        call abre_ruta
        jc .x                       ; no hay CONFIG.SYS: nada que hacer
        mov [cf_ficha], bx
        mov byte [cf_files], N_ARCH
        mov byte [cf_fin], 0
.linea:
        mov di, linea
        xor cx, cx
.car:
        push cx                     ; un octeto del archivo
        push di
        mov [es_seg], cs
        mov word [es_off], red_car
        mov byte [es_op], 0
        mov bx, [cf_ficha]
        mov cx, 1
        call h_es
        pop di
        pop cx
        jc .eof
        test ax, ax
        jz .eof
        mov al, [red_car]
        cmp al, 0x1A
        je .eof
        cmp al, 10
        je .hecha
        cmp al, 13
        je .car
        cmp al, 9                   ; (el tabulador, como un espacio)
        jne .c1
        mov al, ' '
.c1:
        cmp cx, LIN_MAX
        jae .car
        stosb
        inc cx
        jmp .car
.eof:
        mov byte [cf_fin], 1
.hecha:
        mov byte [di], 0
        call config_linea
        cmp byte [cf_fin], 0
        je .linea
        mov bx, [cf_ficha]
        call h_cierra
        ; la tabla de archivos abiertos, del tamaño pedido: lo que crece, a cero
        mov al, [cf_files]
        cmp al, N_ARCH
        jbe .x
        mov [n_arch], al
        sub al, N_ARCH
        mov ah, H_TAM_F
        mul ah
        mov cx, ax
        mov di, fichas + N_ARCH * H_TAM_F
        xor al, al
        rep stosb
.x:
        ret

; una línea de CONFIG.SYS (en `linea`, acabada en 0)
config_linea:
        mov si, linea
        call cf_esp
        cmp byte [si], 0
        je .x
        mov di, t_rem
        call cf_clave
        jne .files
        cmp byte [si], ' '          ; «REM» a secas o seguido de un espacio
        jbe .x
        jmp .mal
.files:
        mov di, t_files
        call cf_clave
        jne .buffers
        call cf_igual
        jne .mal
        xor ax, ax                  ; el número
.n:
        mov bl, [si]
        sub bl, '0'
        cmp bl, 9
        ja .nf
        cmp ax, 100                 ; (ya es más de lo que cabe)
        jae .n1
        mov bh, 10
        mul bh
        xor bh, bh
        add ax, bx
.n1:
        inc si
        jmp .n
.nf:
        cmp ax, N_ARCH
        jae .f1
        mov ax, N_ARCH
.f1:
        cmp ax, FILES_MAX
        jbe .f2
        mov ax, FILES_MAX
.f2:
        mov [cf_files], al
        ret
.buffers:
        mov di, t_buffers
        call cf_clave
        jne .shell
        call cf_igual
        jne .mal
        ret
.shell:
        mov di, t_shell
        call cf_clave
        jne .mal
        call cf_igual
        jne .mal
        cmp byte [si], 0
        je .mal
        mov di, sh_ruta             ; el programa, hasta el primer espacio...
        mov cx, RUTA_MAX - 1
.s1:
        mov al, [si]
        cmp al, ' '
        jbe .s2
        stosb
        inc si
        loop .s1
.s2:
        mov byte [di], 0
        mov di, sh_cola + 1         ; ...y lo demás, como lo recibe un programa:
        xor cx, cx                  ; cuántas letras, las letras y un retorno
.s3:
        lodsb
        test al, al
        jz .s4
        cmp cx, 60
        jae .s4
        stosb
        inc cx
        jmp .s3
.s4:
        mov byte [di], 13
        mov [sh_cola], cl
.x:
        ret
.mal:
        mov si, m_cf_no
        call pon_cad
        mov si, linea
        call pon_cad
        jmp pon_nl

; SI salta los espacios
cf_esp:
        cmp byte [si], ' '
        jne .x
        inc si
        jmp cf_esp
.x:
        ret
; ¿empieza SI por la palabra de DI (en mayúsculas), sin mirar mayúsculas?
; ZF=1 y SI detrás de ella, si es así; si no, SI no cambia
cf_clave:
        push ax
        push bx
        mov bx, si
.c:
        mov ah, [di]
        test ah, ah
        jz .x                       ; (ZF=1)
        mov al, [si]
        call mayus
        inc si
        inc di
        cmp al, ah
        je .c
        mov si, bx                  ; (ZF=0)
.x:
        pop bx
        pop ax
        ret
; detrás de la palabra tiene que venir «=» (con o sin espacios). ZF=1 y SI
; detrás, si es así
cf_igual:
        call cf_esp
        cmp byte [si], '='
        jne .x
        inc si
        call cf_esp
        cmp al, al
.x:
        ret

t_config    db "\CONFIG.SYS", 0
t_rem       db "REM", 0
t_files     db "FILES", 0
t_buffers   db "BUFFERS", 0
t_shell     db "SHELL", 0
m_cf_no     db "CONFIG.SYS: no se entiende esta línea: ", 0
