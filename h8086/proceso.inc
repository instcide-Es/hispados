; =============================================================================
;  Hispa-8086 - la memoria y los programas
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  LA MEMORIA. Todo lo que queda detrás del núcleo se reparte en bloques,
;  como en MS-DOS: delante de cada uno hay una cabecera de un párrafo (16
;  octetos) con una letra («M», o «Z» si es el último), el dueño (el
;  programa que lo tiene, por el segmento de su prefijo; 0 si está libre) y
;  su tamaño en párrafos. INT 21h: 48h pedir, 49h soltar, 4Ah cambiar el
;  tamaño. Los bloques libres que quedan seguidos se juntan al buscar.
;
;  LOS PROGRAMAS. lanza carga un .COM o un .EXE y le pasa el mando:
;    - le da un bloque con su entorno (una copia del de quien lo lanza: las
;      variables y, detrás, su propia ruta) y otro con toda la memoria libre
;      (un .EXE puede pedir menos: lo dice su cabecera);
;    - delante del programa va su prefijo (PSP, 256 octetos): INT 20h, hasta
;      dónde llega su memoria, quién lo ha lanzado, su entorno y, en 80h, lo
;      que se escribió detrás de su nombre;
;    - a un .EXE se le suman a las direcciones de su tabla el segmento donde
;      ha quedado.
;  Un programa puede lanzar otro (INT 21h/4Bh) y espera a que acabe: al
;  terminar el hijo (4Ch) se cierran sus archivos, se suelta su memoria y
;  el padre sigue donde estaba, con el código de salida en 4Dh.
;  [psp_act] es el programa que corre (0: el intérprete del núcleo).
; =============================================================================
MCB_TIPO    equ 0
MCB_DUENO   equ 1
MCB_TAM     equ 3
E_MEMORIA   equ 8                   ; no hay memoria bastante
E_BLOQUE    equ 9                   ; eso no es un bloque de memoria
E_FORMATO   equ 11                  ; el programa no es válido
ENT_MAX     equ 1024                ; lo más que mide un entorno
; el prefijo de un programa
PSP_FIN     equ 0x02                ; dw: el primer segmento que ya no es suyo
PSP_PADRE   equ 0x16                ; dw: quien lo lanzó
PSP_ENT     equ 0x2C                ; dw: su entorno
PSP_PILA    equ 0x2E                ; dd: la pila de él mientras espera a un hijo
PSP_DTA     equ 0x38                ; dd: la zona de búsquedas de quien lo lanzó

; ---- la memoria ----
; todo lo que hay detrás del núcleo, un solo bloque libre
mem_ini:
        push es
        mov ax, [mem_seg]
        mov es, ax
        mov byte [es:MCB_TIPO], 'Z'
        mov word [es:MCB_DUENO], 0
        mov bx, [mem_kb]
        mov cl, 6
        shl bx, cl
        sub bx, ax
        dec bx
        mov [es:MCB_TAM], bx
        pop es
        ret
; (interna) al bloque de la cabecera ES se le juntan los libres que le siguen
mem_junta:
        push ax
        push bx
        push ds
.b:
        cmp byte [es:MCB_TIPO], 'Z'
        je .x
        mov ax, es
        add ax, [es:MCB_TAM]
        inc ax
        mov ds, ax                  ; la cabecera siguiente
        cmp word [MCB_DUENO], 0
        jne .x
        mov bx, [MCB_TAM]
        inc bx
        add [es:MCB_TAM], bx
        mov al, [MCB_TIPO]
        mov [es:MCB_TIPO], al
        jmp .b
.x:
        pop ds
        pop bx
        pop ax
        ret
; (interna) el bloque de la cabecera ES se queda en BX párrafos; lo que sobra
; pasa a ser un bloque libre detrás
mem_parte:
        push ax
        push cx
        push ds
        mov cx, [es:MCB_TAM]
        sub cx, bx
        jbe .x                      ; (no sobra nada)
        dec cx                      ; un párrafo es la cabecera nueva
        mov ax, es
        add ax, bx
        inc ax
        mov ds, ax
        mov al, [es:MCB_TIPO]
        mov [MCB_TIPO], al
        mov word [MCB_DUENO], 0
        mov [MCB_TAM], cx
        mov byte [es:MCB_TIPO], 'M'
        mov [es:MCB_TAM], bx
.x:
        pop ds
        pop cx
        pop ax
        ret
; pide BX párrafos para el dueño DX -> AX=el segmento del bloque. CF=1,
; AX=E_MEMORIA y BX=lo más que hay seguido, si no hay
mem_pide:
        push cx
        push si
        push es
        xor si, si
        mov ax, [mem_seg]
.b:
        mov es, ax
        cmp word [es:MCB_DUENO], 0
        jne .sig
        call mem_junta
        mov cx, [es:MCB_TAM]
        cmp cx, si
        jbe .n
        mov si, cx
.n:
        cmp cx, bx
        jae .cabe
.sig:
        cmp byte [es:MCB_TIPO], 'Z'
        je .no
        add ax, [es:MCB_TAM]
        inc ax
        jmp .b
.cabe:
        call mem_parte
        mov [es:MCB_DUENO], dx
        inc ax
        clc
        jmp short .x
.no:
        mov bx, si
        mov ax, E_MEMORIA
        stc
.x:
        pop es
        pop si
        pop cx
        ret
; (interna) AX=segmento de un bloque -> ES=su cabecera. CF=1, AX=E_BLOQUE si
; no lo es
mem_cab:
        dec ax
        mov es, ax
        inc ax
        cmp byte [es:MCB_TIPO], 'M'
        je .x
        cmp byte [es:MCB_TIPO], 'Z'
        je .x
        mov ax, E_BLOQUE
        stc
        ret
.x:
        clc
        ret
; suelta el bloque del segmento AX
mem_suelta:
        push es
        call mem_cab
        jc .x
        mov word [es:MCB_DUENO], 0
.x:
        pop es
        ret
; el bloque del segmento AX pasa a tener BX párrafos. CF=1, AX=E_MEMORIA y
; BX=lo más que puede crecer, si no cabe
mem_cambia:
        push dx
        push es
        call mem_cab
        jc .x
        mov dx, [es:MCB_DUENO]      ; (mem_junta lo necesita con dueño)
        call mem_junta
        cmp bx, [es:MCB_TAM]
        ja .no
        call mem_parte
        clc
        jmp short .x
.no:
        mov bx, [es:MCB_TAM]
        mov ax, E_MEMORIA
        stc
.x:
        pop es
        pop dx
        ret
; suelta todos los bloques del dueño DX
mem_suelta_de:
        push ax
        push es
        mov ax, [mem_seg]
.b:
        mov es, ax
        cmp [es:MCB_DUENO], dx
        jne .s
        mov word [es:MCB_DUENO], 0
.s:
        cmp byte [es:MCB_TIPO], 'Z'
        je .x
        add ax, [es:MCB_TAM]
        inc ax
        jmp .b
.x:
        pop es
        pop ax
        ret
; -> BX=el mayor hueco libre, en párrafos
mem_mayor:
        push ax
        push dx
        mov bx, 0xFFFF
        xor dx, dx
        call mem_pide
        pop dx
        pop ax
        ret

; ---- INT 21h: 48h, 49h, 4Ah ----
f48:
        mov bx, [bp+R_BX]
        mov dx, [psp_act]
        call mem_pide
        jc mem_error
        mov [bp+R_AX], ax
        ret
f49:
        mov ax, [bp+R_ES]
        call mem_suelta
        jc f_error
        ret
f4A:
        mov ax, [bp+R_ES]
        mov bx, [bp+R_BX]
        call mem_cambia
        jc mem_error
        ret
mem_error:
        cmp ax, E_MEMORIA           ; con «no hay memoria», BX dice cuánta hay
        jne f_error
        mov [bp+R_BX], bx
        jmp f_error
; ---- 50h: el programa que corre pasa a ser BX; 51h y 62h: cuál es -> BX ----
f50:
        mov ax, [bp+R_BX]
        mov [psp_act], ax
        ret
f51:
        mov ax, [psp_act]
        mov [bp+R_BX], ax
        ret

; ---- 4Bh: lanzar el programa de DS:DX y esperar a que acabe. ES:BX: dw el
;      entorno (0: una copia del mío), dd lo que va detrás de su nombre ----
f4B:
        cmp byte [bp+R_AX], 0
        je .va
        mov ax, E_FUNCION
        jmp f_error
.va:
        mov es, [bp+R_DS]
        mov si, [bp+R_DX]
        call ruta_copia
        mov es, [bp+R_ES]
        mov bx, [bp+R_BX]
        mov ax, [es:bx]
        mov [lz_ent], ax
        mov ax, [es:bx+2]
        mov [lz_cola], ax
        mov ax, [es:bx+4]
        mov [lz_cola+2], ax
        push cs
        pop es
        mov ax, [psp_act]           ; dónde seguir cuando el hijo acabe
        test ax, ax
        jz .l
        push es
        mov es, ax
        mov [es:PSP_PILA], bp
        mov [es:PSP_PILA+2], ss
        pop es
.l:
        call lanza                  ; (si vuelve, es que no ha podido)
        jmp f_error

; ---- lanzar: el programa de `ruta`, con [lz_cola] (cuenta, texto y retorno)
;      y el entorno [lz_ent]. Si va bien NO VUELVE: corre el programa. Si no,
;      CF=1 y AX=error ----
lanza:
        call abre_ruta
        jnc .abierto
        ret
.abierto:
        mov [lz_ficha], bx
        mov word [lz_ent_seg], 0
        mov word [lz_psp], 0
        ; la cabecera: ¿es un .EXE?
        mov [es_seg], cs
        mov word [es_off], lz_cab
        mov cx, 28
        mov byte [es_op], 0
        call h_es
        jc .mal
        mov byte [lz_exe], 0
        cmp ax, 28
        jb .tipo
        cmp word [lz_cab], 'MZ'
        je .exe
        cmp word [lz_cab], 'ZM'
        jne .tipo
.exe:
        mov byte [lz_exe], 1
.tipo:
        ; ---- el entorno: una copia, y detrás la ruta del programa ----
        mov ax, [lz_ent]
        test ax, ax
        jnz .e1
        mov ax, [psp_act]           ; el de quien lanza...
        test ax, ax
        jz .e0
        mov es, ax
        mov ax, [es:PSP_ENT]
        test ax, ax
        jnz .e1
.e0:
        mov ax, cs                  ; ...o el del núcleo
        mov word [lz_ent_off], ent_nucleo
        jmp short .e2
.e1:
        mov word [lz_ent_off], 0
.e2:
        mov [lz_ent_org], ax
        mov es, ax                  ; cuánto mide: hasta los dos ceros
        mov di, [lz_ent_off]
        xor cx, cx
.el:
        cmp word [es:di], 0
        je .ef
        inc di
        inc cx
        cmp cx, ENT_MAX
        jb .el
.ef:
        inc cx                      ; (con el cero de la última variable)
        mov [lz_ent_tam], cx
        push cs
        pop es
        mov bx, cx
        add bx, 2 + 2 + RUTA_MAX + 8 + 15
        mov cl, 4
        shr bx, cl
        mov dx, 0xFFFF              ; (el dueño se pone luego)
        call mem_pide
        jc .mal
        mov [lz_ent_seg], ax
        push ds
        mov es, ax
        xor di, di
        mov cx, [lz_ent_tam]
        mov si, [lz_ent_off]
        mov ds, [lz_ent_org]
        rep movsb
        pop ds
        xor al, al                  ; el segundo cero, un 1 y la ruta
        stosb
        mov ax, 1
        stosw
        mov si, ruta
        cmp byte [si+1], ':'
        je .er
        mov al, [unidad_act]
        add al, 'A'
        mov ah, ':'
        stosw
        cmp byte [si], '\'
        je .er
        mov al, '\'
        stosb
        push si
        mov si, dir_txt
        cmp byte [si], 0
        je .ed
.ec:
        lodsb
        test al, al
        jz .ed0
        stosb
        jmp .ec
.ed0:
        mov al, '\'
        stosb
.ed:
        pop si
.er:
        lodsb
        call mayus
        stosb
        test al, al
        jnz .er
        push cs
        pop es
        ; ---- la memoria del programa ----
        cmp byte [lz_exe], 0
        jne .mexe
        mov bx, [lz_ficha]          ; un .COM: su tamaño, el prefijo y pila
        cmp word [bx+H_TAM+2], 0
        jne .grande
        mov ax, [bx+H_TAM]
        cmp ax, 0xFE00
        ja .grande
        add ax, 0x100 + 0x100 + 15
        mov cl, 4
        shr ax, cl
        mov [lz_min], ax
        mov word [lz_max], 0xFFFF
        jmp short .mpide
.mexe:
        ; un .EXE: lo que ocupa cargado = páginas x 512 (la última, lo que
        ; diga) menos su cabecera; más lo que pide de más
        mov ax, [lz_cab+4]
        mov cl, 5
        shl ax, cl                  ; páginas, en párrafos (x 32)
        cmp word [lz_cab+2], 0
        je .m1
        sub ax, 32
        mov bx, [lz_cab+2]
        add bx, 15
        mov cl, 4
        shr bx, cl
        add ax, bx
.m1:
        sub ax, [lz_cab+8]
        jbe .formato
        mov [lz_img], ax            ; párrafos de lo que se carga
        add ax, 0x10                ; y el prefijo
        mov bx, ax
        add ax, [lz_cab+0x0A]
        jc .grande
        mov [lz_min], ax
        add bx, [lz_cab+0x0C]
        jnc .m2
        mov bx, 0xFFFF
.m2:
        cmp bx, ax
        jae .m3
        mov bx, ax
.m3:
        mov [lz_max], bx
.mpide:
        call mem_mayor
        cmp bx, [lz_min]
        jb .grande
        cmp bx, [lz_max]
        jbe .m4
        mov bx, [lz_max]
.m4:
        mov [lz_tam], bx
        mov dx, 0xFFFF
        call mem_pide
        jc .mal
        mov [lz_psp], ax
        ; los dos bloques son suyos
        dec ax
        mov es, ax
        inc ax
        mov [es:MCB_DUENO], ax
        mov bx, [lz_ent_seg]
        dec bx
        mov es, bx
        mov [es:MCB_DUENO], ax
        ; ---- el prefijo ----
        mov es, ax
        xor di, di
        mov cx, 0x80
        xor ax, ax
        rep stosw
        mov word [es:0], 0x20CD
        mov ax, [lz_psp]
        add ax, [lz_tam]
        mov [es:PSP_FIN], ax
        mov ax, [psp_act]
        mov [es:PSP_PADRE], ax
        mov ax, [lz_ent_seg]
        mov [es:PSP_ENT], ax
        mov ax, [dta_off]
        mov [es:PSP_DTA], ax
        mov ax, [dta_seg]
        mov [es:PSP_DTA+2], ax
        mov di, 0x5C                ; los dos nombres de archivo de la vieja
        call .fcb                   ; interfaz, en blanco
        mov di, 0x6C
        call .fcb
        push ds                     ; lo que va detrás del nombre
        lds si, [lz_cola]
        mov di, 0x80
        mov cl, [si]
        xor ch, ch
        cmp cx, 126
        jbe .c1
        mov cx, 126
.c1:
        mov al, cl
        stosb
        inc si
        rep movsb
        mov al, 0x0D
        stosb
        pop ds
        push cs
        pop es
        ; ---- cargarlo ----
        mov bx, [lz_ficha]
        cmp byte [lz_exe], 0
        jne .cexe
        xor ax, ax
        mov [bx+H_POS], ax
        mov [bx+H_POS+2], ax
        mov ax, [lz_psp]
        mov [es_seg], ax
        mov word [es_off], 0x100
        mov cx, [bx+H_TAM]
        mov byte [es_op], 0
        call h_es
        jc .mal
        jmp .cargado
.cexe:
        mov ax, [lz_cab+8]          ; lo cargable empieza detrás de la cabecera
        xor dx, dx
        mov cx, 4
.s4:
        shl ax, 1
        rcl dx, 1
        loop .s4
        mov [bx+H_POS], ax
        mov [bx+H_POS+2], dx
        mov ax, [lz_psp]
        add ax, 0x10
        mov [lz_base], ax           ; donde queda el programa
        mov [es_seg], ax
        mov si, [lz_img]            ; párrafos que faltan: de 2 K en 2 K (32 K)
.trozo:
        test si, si
        jz .reub
        mov cx, si
        cmp cx, 0x0800
        jbe .t1
        mov cx, 0x0800
.t1:
        sub si, cx
        push cx
        mov ax, cx
        mov cl, 4
        shl ax, cl
        mov cx, ax
        mov word [es_off], 0
        mov byte [es_op], 0
        push si
        call h_es
        pop si
        pop cx
        jc .mal
        add [es_seg], cx
        jmp .trozo
.reub:
        ; las direcciones de la tabla: a cada una, el segmento de carga
        mov ax, [lz_cab+0x18]
        mov [bx+H_POS], ax
        mov word [bx+H_POS+2], 0
        mov si, [lz_cab+6]          ; cuántas
.r:
        test si, si
        jz .cargado
        mov [es_seg], cs
        mov word [es_off], lz_reub
        mov cx, 4
        mov byte [es_op], 0
        push si
        call h_es
        pop si
        jc .mal
        cmp ax, 4
        jne .formato
        push es
        mov ax, [lz_reub+2]
        add ax, [lz_base]
        mov es, ax
        mov di, [lz_reub]
        mov ax, [lz_base]
        add [es:di], ax
        pop es
        dec si
        jmp .r
.cargado:
        mov bx, [lz_ficha]
        call h_cierra
        ; ---- y a él ----
        mov ax, [lz_psp]
        mov [psp_act], ax
        mov [dta_seg], ax
        mov word [dta_off], 0x80
        cmp byte [lz_exe], 0
        jne .salta_exe
        mov bx, [lz_tam]            ; un .COM: todo en su segmento, la pila
        mov dx, 0xFFFE              ; arriba (hasta donde llegue su memoria)
        cmp bx, 0x1000
        jae .pila
        mov dx, bx
        mov cl, 4
        shl dx, cl
        sub dx, 2
.pila:
        cli
        mov ds, ax
        mov es, ax
        mov ss, ax
        mov sp, dx
        sti
        xor bx, bx
        push bx                     ; un cero: «ret» salta a PSP:0000
        push ax
        mov bx, 0x100
        push bx
        xor ax, ax
        retf
.salta_exe:
        mov bx, [lz_base]
        mov dx, [lz_cab+0x0E]       ; su pila
        add dx, bx
        mov cx, [lz_cab+0x10]
        add bx, [lz_cab+0x16]       ; y por dónde empieza
        mov si, [lz_cab+0x14]
        cli
        mov ss, dx
        mov sp, cx
        sti
        mov ds, ax
        mov es, ax
        push bx
        push si
        xor ax, ax
        retf
.formato:
        mov ax, E_FORMATO
        jmp short .mal
.grande:
        mov ax, E_MEMORIA
.mal:
        push ax                     ; lo pedido, de vuelta
        push cs
        pop es
        mov bx, [lz_ficha]
        call h_cierra
        mov ax, [lz_ent_seg]
        test ax, ax
        jz .l1
        call mem_suelta
.l1:
        mov ax, [lz_psp]
        test ax, ax
        jz .l2
        call mem_suelta
.l2:
        pop ax
        stc
        ret
; (interna) un nombre de archivo en blanco en ES:DI
.fcb:
        xor al, al
        stosb
        mov al, ' '
        mov cx, 11
        rep stosb
        ret

; ---- el programa que corre termina con el código AL ----
termina:
        cli
        mov bx, cs
        mov ds, bx
        mov ss, bx
        mov sp, pila_fin
        sti
        cld
        mov [cod_salida], al
        mov dx, [psp_act]
        test dx, dx
        jz .nucleo
        push cs
        pop es
        call h_cierra_de            ; sus archivos y su memoria
        mov es, dx
        mov ax, [es:PSP_PADRE]
        mov [psp_act], ax
        mov bx, [es:PSP_DTA]
        mov [dta_off], bx
        mov bx, [es:PSP_DTA+2]
        mov [dta_seg], bx
        call mem_suelta_de
        test ax, ax
        jz .nucleo
        mov es, ax                  ; el padre sigue donde estaba: dentro de
        cli                         ; su INT 21h/4Bh, que ahora acaba bien
        mov ss, [es:PSP_PILA+2]
        mov sp, [es:PSP_PILA]
        sti
        mov bp, sp
        and byte [bp+R_BAND], 0xFE
        pop es
        pop ds
        pop bp
        pop di
        pop si
        pop dx
        pop cx
        pop bx
        pop ax
        iret
.nucleo:
        push cs
        pop es
        mov [dta_seg], cs
        mov word [dta_off], dta_int
        jmp interprete

; el entorno de partida
ent_nucleo  db "PATH=A:\", 0
            db "COMSPEC=A:\NUCLEO86.SYS", 0
            db 0, 0
