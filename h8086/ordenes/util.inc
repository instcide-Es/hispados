; =============================================================================
;  Hispa-8086 - lo común de las órdenes de disco (FORMATEA, SISTEMA, PARTIR)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Escribir en la pantalla, preguntar «¿seguir?», y los sectores del disco
;  pedidos directamente a la BIOS (INT 13h): estas órdenes trabajan con discos
;  que el núcleo todavía no conoce (sin formato, sin particiones), así que no
;  pueden ir por él. Cuando acaban, se lo dicen (INT 21h/E8h con AL=1) para
;  que olvide lo que sabía de los discos y los mire otra vez.
;
;  Todas suponen DS = ES = SS = el programa (.COM). Quien lo incluye define
;  antes CON_SISTEMA si quiere además lo de copiar el sistema, y pone al final
;  de sus variables las de aquí (macro UTIL_BSS).
; =============================================================================
%ifndef XFER_TAM
%define XFER_TAM 512                ; lo más que se pide de una vez a la BIOS
%endif
%macro DOS 1
        mov ah, %1
        int 0x21
%endmacro

; ---- la pantalla ----
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
; escribe AX en decimal
pon_num:
        push dx
        xor dx, dx
        call pon_num32
        pop dx
        ret
; escribe DX:AX en decimal. Conserva los registros
pon_num32:
        push ax
        push bx
        push cx
        push dx
        push si
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
.c:
        pop ax
        add al, '0'
        call pon_car
        loop .c
        pop si
        pop dx
        pop cx
        pop bx
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
; SI salta los espacios
salta_esp:
        cmp byte [si], ' '
        jne .x
        inc si
        jmp salta_esp
.x:
        ret
; escribe la pregunta de SI y espera una tecla: ZF=1 si es «S» (sí). Con
; [sin_preguntar] puesto (el parámetro /A, para los lotes), ni pregunta
confirma:
        cmp byte [sin_preguntar], 0
        jne .si
        call pon_cad
        push ax
        DOS 0x01
        call mayus
        call pon_nl
        cmp al, 'S'
        pop ax
        ret
.si:
        cmp al, al
        ret
; ¿es Hispa-8086 0.9 o posterior? Estas órdenes necesitan que el núcleo sepa
; olvidar los discos (E8h). Si no lo es, lo dice y termina
pide_nucleo:
        mov ax, 0xE800
        xor bx, bx
        int 0x21
        cmp ax, 0x4869
        jne .no
        cmp bh, 9
        jb .no
        ret
.no:
        mov si, m_u_nucleo
        call pon_cad
        mov ax, 0x4C04
        int 0x21
; le dice al núcleo que los discos han cambiado por debajo
avisa_nucleo:
        push ax
        mov ax, 0xE801
        int 0x21
        pop ax
        ret

%ifndef SOLO_TEXTO                  ; (quien solo quiere la pantalla lo define,
                                    ; y declara él [sin_preguntar])
; ---- los sectores, con la BIOS ----
; Todo lo que se le pide a la BIOS pasa por un búfer «de paso»: las
; disqueteras (y los discos duros de los XT) llevan los datos por DMA, que no
; puede cruzar una frontera de 64 K de la memoria, y un .COM no elige dónde lo
; cargan. Se reserva el doble de lo que hace falta y se usa la mitad que no
; cruza ninguna. Hay que llamarla al empezar
xfer_ini:
        push ax
        push bx
        push cx
        mov ax, cs
        mov cl, 4
        shl ax, cl
        add ax, xfer_zona           ; dónde cae dentro de sus 64 K
        mov bx, xfer_zona
        add ax, XFER_TAM - 1
        jnc .x
        add bx, XFER_TAM            ; la primera mitad cruza: la segunda
.x:
        mov [p_xfer], bx
        pop cx
        pop bx
        pop ax
        ret

; lee (disco_lee) o escribe (disco_escribe) el sector AX, contado desde
; [d_base], en ES:BX, en la unidad [d_bios] con las medidas [d_spp] y
; [d_caras]. Tres intentos. CF=1 y el error de la BIOS en [d_err] si no se
; puede. Conserva los registros
disco_lee:
        mov byte [d_op], 2
        jmp short disco_es
disco_escribe:
        mov byte [d_op], 3
disco_es:
        push bx
        push cx
        push dx
        push si
        push di
        push ax
        mov [d_buf], bx
        cmp byte [d_op], 3          ; lo que se escribe, al búfer de paso
        jne .va
        mov si, bx
        mov di, [p_xfer]
        mov cx, 256
        rep movsw
.va:
        mov di, 3
.otra:
        pop ax
        push ax
        xor dx, dx                  ; el sector, contado desde el principio del
        add ax, [d_base]            ; disco (32 bits)
        adc dx, [d_base+2]
        mov bx, ax
        mov ax, dx                  ; entre los sectores de una pista, en dos pasos
        xor dx, dx
        div word [d_spp]
        mov ax, bx
        div word [d_spp]            ; AX = pista x caras + cara, DX = sector - 1
        mov cx, dx
        inc cl
        xor dx, dx
        div word [d_caras]          ; AX = cilindro, DX = cara
        mov dh, dl
        mov ch, al
        mov al, ah                  ; (los dos bits altos del cilindro, con el sector)
        ror al, 1
        ror al, 1
        and al, 0xC0
        or cl, al
        mov bx, [p_xfer]
        mov dl, [d_bios]
        mov ah, [d_op]
        mov al, 1
        int 0x13
        jnc .bien
        mov [d_err], ah
        cmp ah, 3                   ; protegido contra escritura: no insistir
        je .mal
        xor ax, ax
        int 0x13
        dec di
        jnz .otra
.mal:
        pop ax
        stc
        jmp short .x
.bien:
        cmp byte [d_op], 2          ; lo leído, a donde se pidió
        jne .b1
        mov si, [p_xfer]
        mov di, [d_buf]
        mov cx, 256
        rep movsw
.b1:
        pop ax
        clc
.x:
        pop di
        pop si
        pop dx
        pop cx
        pop bx
        ret

; el disco duro: sus medidas ([d_bios] = 80h, [d_spp], [d_caras] y [d_cil],
; cuántos cilindros) y su primer sector, el de las particiones, en `mbr`.
; CF=1 si no hay disco duro (o no se deja leer)
duro_lee:
        push es
        mov ah, 8
        mov dl, 0x80
        int 0x13
        pop es
        jc .no
        test dl, dl
        jz .no
        mov byte [d_bios], 0x80
        mov al, ch                  ; el último cilindro: 10 bits
        mov ah, cl
        rol ah, 1
        rol ah, 1
        and ah, 3
        inc ax
        mov [d_cil], ax
        and cx, 63
        mov [d_spp], cx
        mov cl, dh
        xor ch, ch
        inc cx
        mov [d_caras], cx
        xor ax, ax
        mov [d_base], ax
        mov [d_base+2], ax
        mov bx, mbr
        call disco_lee
        ret
.no:
        stc
        ret
; escribe `mbr` en el primer sector del disco duro. CF=1 si falla
duro_escribe:
        xor ax, ax
        mov [d_base], ax
        mov [d_base+2], ax
        mov bx, mbr
        jmp disco_escribe
; busca en `mbr` la primera partición de DOS que este sistema entiende (FAT12,
; FAT16 o FAT16 «grande») -> SI = su fila de la tabla. CF=1 si no hay
duro_particion:
        cmp word [mbr+510], 0xAA55
        jne .no
        mov si, mbr + 0x1BE
        mov cx, 4
.p:
        mov al, [si+4]
        cmp al, 1
        je .esta
        cmp al, 4
        je .esta
        cmp al, 6
        je .esta
        add si, 16
        loop .p
.no:
        stc
        ret
.esta:
        clc
        ret
; si el sector de particiones de `mbr` no tiene código (un disco nuevo o hecho
; con otra herramienta), se le pone el nuestro, y la firma. La tabla no se toca
duro_codigo:
        push cx
        push si
        push di
        mov word [mbr+510], 0xAA55
        mov si, mbr
        mov cx, 16
.c:
        lodsb
        test al, al
        jnz .x                      ; ya tiene uno: se deja
        loop .c
        mov si, mbr_codigo          ; (el programa solo lleva lo que no es cero)
        mov di, mbr
        mov cx, MBR_COD
        rep movsb
        mov cx, 446 - MBR_COD
        xor al, al
        rep stosb
.x:
        pop di
        pop si
        pop cx
        ret
; la partición de la fila SI pasa a ser la activa (la que arranca), y solo ella
duro_activa:
        push bx
        push cx
        mov bx, mbr + 0x1BE
        mov cx, 4
.b:
        mov byte [bx], 0
        add bx, 16
        loop .b
        mov byte [si], 0x80
        pop cx
        pop bx
        ret

%endif
%ifdef CON_SISTEMA
; ---- copiar el sistema ----
; la unidad de donde ha salido este programa (está detrás de su entorno, con
; su ruta entera) -> AL, la letra. Sin ruta, la unidad de trabajo
mi_unidad:
        push es
        push di
        push cx
        mov es, [0x2C]
        xor di, di
        xor al, al
        mov cx, 0x7FFF
.b:
        repne scasb                 ; hasta los dos ceros del final
        scasb
        jne .b
        add di, 2
        mov al, [es:di]
        cmp byte [es:di+1], ':'
        je .x
        DOS 0x19
        add al, 'A'
.x:
        call mayus
        pop cx
        pop di
        pop es
        ret
; lee NUCLEO86.SYS y COMMAND.COM de la raíz de la unidad de este programa y
; los deja en la memoria, detrás del programa. CF=1 si no están o no caben
sis_lee:
        call mi_unidad
        mov [t_sis_nucleo], al
        mov [t_sis_command], al
        mov word [sis_fin], sis_mem
        mov dx, t_sis_nucleo
        call .uno
        jc .x
        mov [sis_tam], ax
        mov dx, t_sis_command
        call .uno
        mov [sis_tam+2], ax
.x:
        ret
; (interna) el archivo de DX, entero, a [sis_fin] -> AX = lo que mide
.uno:
        mov al, 0
        DOS 0x3D
        jc .mal
        mov bx, ax
        mov cx, sp                  ; lo que queda hasta la pila (con un margen)
        sub cx, 1024
        sub cx, [sis_fin]
        jbe .cierra
        mov dx, [sis_fin]
        DOS 0x3F
        jc .cierra
        cmp ax, cx                  ; (si lo llena, es que no ha cabido)
        jae .cierra
        add [sis_fin], ax
        push ax
        DOS 0x3E
        pop ax
        clc
        ret
.cierra:
        DOS 0x3E
.mal:
        stc
        ret
; escribe los dos archivos en la raíz de la unidad AL (la letra). CF=1 si no
; se puede (el disco está lleno o protegido)
sis_escribe:
        mov [t_sis_nucleo], al
        mov [t_sis_command], al
        mov dx, t_sis_nucleo
        mov si, sis_mem
        mov cx, [sis_tam]
        call .uno
        jc .x
        mov dx, t_sis_command
        mov si, sis_mem
        add si, [sis_tam]
        mov cx, [sis_tam+2]
        call .uno
.x:
        ret
; (interna) crea el archivo de DX con los CX octetos de SI
.uno:
        push cx
        xor cx, cx
        DOS 0x3C
        pop cx
        jc .mal
        mov bx, ax
        mov dx, si
        DOS 0x40
        jc .cierra
        cmp ax, cx
        jne .cierra
        DOS 0x3E
        ret
.cierra:
        DOS 0x3E
.mal:
        stc
        ret
t_sis_nucleo  db "A:\NUCLEO86.SYS", 0
t_sis_command db "A:\COMMAND.COM", 0
%endif

m_u_nucleo  db "Esta orden necesita Hispa-8086 0.9 o posterior.", 10, 0

; las variables de todo esto (van con las del programa, que no están en el
; archivo)
%macro UTIL_BSS 0
sin_preguntar resb 1                ; /A: no se pregunta nada
d_bios      resb 1                  ; la unidad, para la BIOS
d_op        resb 1
d_err       resb 1
            alignb 2
d_base      resw 2                  ; el sector donde empieza (la partición)
d_spp       resw 1                  ; sectores por pista
d_caras     resw 1
d_cil       resw 1                  ; cilindros del disco duro
d_buf       resw 1
p_xfer      resw 1                  ; el búfer de paso: la mitad buena de...
xfer_zona   resb 2 * XFER_TAM       ; ...esta zona
sis_fin     resw 1
sis_tam     resw 2
mbr         resb 512                ; el sector de particiones
%endmacro
