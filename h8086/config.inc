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
;       BUFFERS=n       cuántos sectores guarda la reserva de sectores de
;                       disco.inc (de 0, sin reserva, a 30; sin decir nada,
;                       4). Cada uno son 516 octetos de memoria.
;       DEVICE=ruta ... carga un controlador de dispositivo (ver más abajo).
;       SHELL=ruta ...  el intérprete de órdenes que se lanza en vez de
;                       \COMMAND.COM, y lo que se le pasa detrás (para que
;                       ejecute AUTOEXEC.BAT hay que ponerle /P, como en DOS).
;       REM ...         un comentario. Las líneas vacías no cuentan.
;  Lo que no se entiende se dice y se sigue con la línea siguiente.
;
;  DEVICE=. El archivo (un binario con su cabecera de dispositivo al
;  principio, como los .SYS de DOS; no valen los que son .EXE) se carga en lo
;  alto del núcleo, detrás de la tabla de archivos y de la reserva; se llama
;  a su rutina de inicio (orden 0), que devuelve hasta dónde se queda, y:
;    - si es de carácter, se enlaza detrás de NUL: se abre por su nombre;
;    - si es de bloque, cada unidad que diga tener recibe la letra siguiente
;      (D:, E:) con las medidas (BPB) que dé. Se le piden los sectores de uno
;      en uno (órdenes 4 y 8). No se le pregunta si han cambiado el disco
;      (órdenes 1 y 2): vale para discos fijos, como uno en memoria.
;  Por eso CONFIG.SYS se lee dos veces: primero todo lo demás, que decide lo
;  que miden la tabla y la reserva, y luego los DEVICE=, que van detrás.
; =============================================================================
config_lee:
        mov byte [cf_files], N_ARCH
        mov byte [cf_bufs], BUF_DEF
        mov byte [cf_uni], 3
        mov byte [cf_pasada], 1
        mov si, t_config
        mov di, ruta
.c:
        lodsb
        stosb
        test al, al
        jnz .c
        call abre_ruta
        mov word [cf_ficha], 0
        jc .medidas                 ; no hay CONFIG.SYS: lo de siempre
        mov [cf_ficha], bx
        call config_pasada
.medidas:
        ; la tabla de archivos abiertos, del tamaño pedido: lo que crece, a cero
        mov al, [cf_files]
        mov [n_arch], al
        sub al, N_ARCH
        mov ah, H_TAM_F
        mul ah
        mov cx, ax
        mov di, fichas + N_ARCH * H_TAM_F
        xor al, al
        rep stosb
        ; detrás, la reserva de sectores, vacía
        mov [cache_p], di
        mov cl, [cf_bufs]
        xor ch, ch
        jcxz .sin
.b:
        mov byte [di+C_UNI], 0xFF
        add di, C_TAM
        loop .b
.sin:
        mov al, [cf_bufs]
        mov [n_cache], al
        add di, 15                  ; y detrás, los controladores
        mov cl, 4
        shr di, cl
        mov ax, cs
        add ax, di
        mov [cf_tope], ax
        mov bx, [cf_ficha]
        test bx, bx
        jz .x
        mov byte [cf_pasada], 2     ; otra vez desde el principio: los DEVICE=
        xor ax, ax
        mov [bx+H_POS], ax
        mov [bx+H_POS+2], ax
        mov word [bx+H_CCLU], 0
        call config_pasada
        mov bx, [cf_ficha]
        call h_cierra
.x:
        ret

; lee CONFIG.SYS entero, línea a línea
config_pasada:
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
        ret

; una línea de CONFIG.SYS (en `linea`, acabada en 0)
config_linea:
        mov si, linea
        call cf_esp
        cmp byte [si], 0
        je .x0
        mov di, t_device
        call cf_clave
        jne .otras
        call cf_igual
        jne .mal0
        cmp byte [cf_pasada], 2
        jne .x0
        jmp config_device
.mal0:
        jmp .mal
.otras:
        cmp byte [cf_pasada], 1     ; lo demás, solo la primera vez
        je .rem
.x0:
        ret
.rem:
        mov di, t_rem
        call cf_clave
        jne .files
        cmp byte [si], ' '          ; «REM» a secas o seguido de un espacio
        jbe .x0
        jmp .mal
.files:
        mov di, t_files
        call cf_clave
        jne .buffers
        call cf_igual
        jne .mal0
        call cf_numero
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
        call cf_numero
        cmp ax, BUF_MAX
        jbe .b1
        mov ax, BUF_MAX
.b1:
        mov [cf_bufs], al
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
        ret
.mal:
        mov si, m_cf_no
        call pon_cad
        mov si, linea
        call pon_cad
        jmp pon_nl

; el número de SI -> AX (hasta 999); SI queda detrás
cf_numero:
        xor ax, ax
.n:
        mov bl, [si]
        sub bl, '0'
        cmp bl, 9
        ja .x
        cmp ax, 100
        jae .n1
        mov bh, 10
        mul bh
        xor bh, bh
        add ax, bx
.n1:
        inc si
        jmp .n
.x:
        ret

; DEVICE=ruta parámetros (SI: detrás del «=»): carga el controlador
config_device:
        mov [cf_arg], si
        mov di, ruta
        mov cx, RUTA_MAX - 1
.r:
        mov al, [si]
        cmp al, ' '
        jbe .rf
        stosb
        inc si
        loop .r
.rf:
        mov byte [di], 0
        call abre_ruta
        jnc .abierto
.no_carga:
        mov si, m_cf_dev
.dice:
        call pon_cad
        mov si, linea
        call pon_cad
        jmp pon_nl
.abierto:
        ; tiene que caber: menos de 64 K y dejando sitio a los programas
        cmp word [bx+H_TAM+2], 0
        jne .grande
        mov ax, [bx+H_TAM]
        mov cl, 4
        shr ax, cl
        add ax, [cf_tope]
        add ax, 0x1000              ; (64 K, por lo menos, para lo demás)
        jc .grande
        mov dx, [mem_kb]
        mov cl, 6
        shl dx, cl
        cmp ax, dx
        jb .cabe
.grande:
        call h_cierra
        jmp .no_carga
.cabe:
        mov ax, [cf_tope]
        mov [es_seg], ax
        mov word [es_off], 0
        mov byte [es_op], 0
        mov cx, [bx+H_TAM]
        push bx
        call h_es
        pop bx
        pushf
        call h_cierra
        popf
        jc .no_carga
        xor si, si                  ; SI = la cabecera (puede haber varias)
.cab:
        mov es, [cf_tope]
        push word [es:si+D_SIG]     ; (la siguiente del archivo, antes de
        push cs                     ; enlazar esta)
        pop es
        ; la orden 0, iniciar: le decimos dónde está su línea y qué unidad
        ; le tocaría; nos dice hasta dónde se queda y, si es de bloque,
        ; cuántas unidades tiene y sus medidas
        mov byte [pet+P_ORDEN], 0
        mov byte [pet+1], 0
        mov byte [pet+P_MEDIO], 0
        mov word [pet+P_DONDE], 0
        mov ax, [cf_tope]
        mov [pet+P_DONDE+2], ax
        mov ax, [cf_arg]
        mov [pet+18], ax
        mov [pet+20], cs
        mov al, [cf_uni]
        mov [pet+22], al
        mov ax, si
        mov dx, [cf_tope]
        call dev_llama
        mov ax, [pet+P_DONDE]       ; hasta dónde se queda: si no pasa de
        add ax, 15                  ; donde empieza, es que no se instala
        mov cl, 4
        shr ax, cl
        add ax, [pet+P_DONDE+2]
        mov [cf_tope2], ax          ; (el tope nuevo)
        mov dx, [mem_kb]            ; (y que deje 64 K, por lo menos)
        mov cl, 6
        shl dx, cl
        sub dx, 0x1000
        cmp ax, dx
        ja .no_inst
        cmp ax, [cf_tope]
        ja .instala
.no_inst:
        add sp, 2
        mov si, m_cf_dev2
        jmp .dice
.instala:
        mov es, [cf_tope]
        test byte [es:si+D_ATR+1], 0x80
        jz .bloque
        mov ax, [dev_nul+D_SIG]     ; de carácter: detrás de NUL
        mov [es:si+D_SIG], ax
        mov ax, [dev_nul+D_SIG+2]
        mov [es:si+D_SIG+2], ax
        mov [dev_nul+D_SIG], si
        mov [dev_nul+D_SIG+2], es
        jmp short .sig
.bloque:
        ; de bloque: a cada unidad suya, una letra (mientras queden)
        mov cl, [pet+P_MEDIO]       ; cuántas tiene
        xor ch, ch
        jcxz .sig
        xor dx, dx                  ; DX = cuál, dentro del controlador
.u:
        mov al, [cf_uni]
        cmp al, N_UNI
        jae .sig
        push cx
        push es
        push cs
        pop es
        mov ah, U_TAM
        mul ah
        add ax, u_guardadas
        mov di, ax
        mov bx, ax
        mov cx, U_TAM
        xor al, al
        rep stosb
        pop es
        pop cx
        mov byte [bx + (u_bios - u_ini)], 0x80      ; (no se cambia: como un disco duro)
        mov [bx + (u_dev - u_ini)], si
        mov [bx + (u_dev - u_ini) + 2], es
        mov [bx + (u_sub - u_ini)], dl
        push es                     ; sus medidas: la tabla que ha dado tiene
        push si                     ; un puntero por unidad
        les si, [pet+18]
        add si, dx
        add si, dx
        mov ax, [es:si]
        mov [bx + (u_bpb - u_ini)], ax
        mov [bx + (u_bpb - u_ini) + 2], es
        pop si
        pop es
        mov bl, [cf_uni]
        xor bh, bh
        mov byte [u_hay+bx], 1
        inc byte [cf_uni]
        inc dx
        loop .u
.sig:
        push cs
        pop es
        pop si                      ; ¿hay otra cabecera en el archivo?
        cmp si, 0xFFFF
        je .fin
        jmp .cab
.fin:
        mov ax, [cf_tope2]
        mov [cf_tope], ax
        ret

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
t_device    db "DEVICE", 0
m_cf_dev    db "CONFIG.SYS: no se puede cargar el controlador: ", 0
m_cf_dev2   db "CONFIG.SYS: el controlador no se ha instalado: ", 0
m_cf_no     db "CONFIG.SYS: no se entiende esta línea: ", 0
