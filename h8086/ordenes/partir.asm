; =============================================================================
;  PARTIR.COM (FDISK) - las particiones del disco duro
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;       PARTIR                 enseña la tabla de particiones
;       PARTIR CREAR [/A]      crea la partición de DOS
;       PARTIR ACTIVAR n [/A]  la partición n pasa a ser la que arranca
;       PARTIR BORRAR n [/A]   borra la partición n
;
;  Trabaja con el primer disco duro. CREAR hace una sola partición primaria
;  que ocupa el disco desde su segunda pista, hasta donde llega este sistema:
;  32 megas (65.535 sectores), en cilindros enteros. Es de tipo 04 (FAT16) si
;  tiene 16 megas o más, y 01 (FAT12) si tiene menos; queda marcada como
;  activa, con su primer sector a cero (para que nadie tome por bueno un
;  formato viejo), y si el sector de particiones no tenía código se le pone el
;  nuestro (mbr.asm). Después hay que darle formato: FORMATEA C: /S.
;
;  Todo lo que cambia el disco se pregunta antes; /A no pregunta (para los
;  lotes). Al acabar se avisa al núcleo (INT 21h/E8h) para que vuelva a mirar
;  el disco duro. Devuelve 0 si ha ido bien, 4 si no y 5 si se contesta que
;  no. Solo instrucciones del 8086.
; =============================================================================
        cpu 8086
        org 0x100

        jmp inicio
%include "util.inc"

inicio:
        cld
        mov di, bss_ini
        mov cx, bss_fin - bss_ini
        xor al, al
        rep stosb
        call pide_nucleo
        call xfer_ini
        ; ---- lo que se pide: una palabra (vale su primera letra), un número
        ;      y /A ----
        mov si, 0x81
.arg:
        call salta_esp
        mov al, [si]
        cmp al, ' '
        jb .fin_args
        cmp al, '/'
        jne .a1
        mov al, [si+1]
        call mayus
        cmp al, 'A'
        jne .uso
        mov byte [sin_preguntar], 1
        add si, 2
        jmp .arg
.a1:
        cmp al, '1'
        jb .uso
        cmp al, '4'
        ja .a2
        sub al, '0'
        mov [numero], al
        inc si
        cmp byte [si], ' '          ; (una cifra sola: con «12» ganaba el 2, y
        ja .uso                     ; con /A ni se preguntaba)
        jmp .arg
.a2:
        call mayus
        mov [que], al
.a3:
        inc si                      ; (el resto de la palabra)
        cmp byte [si], ' '
        ja .a3
        jmp .arg
.uso:
        mov si, m_uso
        call pon_cad
        mov ax, 0x4C04
        int 0x21
.fin_args:
        call duro_lee
        jnc .hay
        mov si, m_e_sin_duro
        jmp error
.hay:
        cmp word [mbr+510], 0xAA55  ; sin firma no hay tabla: como si
        je .tabla                   ; estuviera vacía
        mov di, mbr + 0x1BE
        mov cx, 64
        xor al, al
        rep stosb
.tabla:
        mov ax, [d_caras]           ; lo que mide un cilindro, en sectores
        mul word [d_spp]
        mov [cil_sec], ax
        mov al, [que]
        test al, al
        jnz .q1
        jmp ver
.q1:
        cmp al, 'V'
        jne .q2
        jmp ver
.q2:
        cmp al, 'C'
        jne .q3
        jmp crear
.q3:
        cmp al, 'A'
        je .con_n
        cmp al, 'B'
        jne .uso
.con_n:
        mov al, [numero]            ; ACTIVAR y BORRAR: qué partición
        test al, al
        jz .uso
        add al, '0'
        mov [m_activar.n], al
        mov [m_borrar.n], al
        mov al, [numero]
        dec al
        mov cl, 4
        shl al, cl
        xor ah, ah
        add ax, mbr + 0x1BE
        mov si, ax
        mov [fila], si
        cmp byte [si+4], 0
        jne .existe
        mov si, m_e_no_hay
        jmp error
.existe:
        cmp byte [que], 'A'
        je activar
        jmp borrar

; ---- ACTIVAR n ----
activar:
        mov si, m_activar
        call confirma
        je .va
        jmp no_quiere
.va:
        mov si, [fila]
        call duro_codigo
        call duro_activa
        call duro_escribe
        jc mal_escribir
        mov si, m_hecho_a
        jmp bien

; ---- BORRAR n ----
borrar:
        cmp byte [sin_preguntar], 0
        jne .va
        mov si, m_borrar
        call pon_cad
        mov si, [fila]
        mov ax, [si+12]
        mov dx, [si+14]
        call megas
        call pon_num
        mov si, m_borrar2
        call confirma
        je .va
        jmp no_quiere
.va:
        mov di, [fila]
        mov cx, 16
        xor al, al
        rep stosb
        call duro_escribe
        jc mal_escribir
        mov si, m_hecho_b
        jmp bien

mal_escribir:
        mov si, m_e_escribir
        jmp error

; ---- CREAR ----
crear:
        mov si, mbr + 0x1BE         ; solo en un disco sin particiones
        mov cx, 4
.v:
        cmp byte [si+4], 0
        je .v1
        mov si, m_e_ya_hay
        jmp error
.v1:
        add si, 16
        loop .v
        ; cuántos cilindros: todos, o los que caben en 65.535 sectores
        mov ax, 0xFFFF
        xor dx, dx
        add ax, [d_spp]
        adc dx, 0
        div word [cil_sec]
        cmp ax, [d_cil]
        jbe .c1
        mov ax, [d_cil]
.c1:
        mov [n_cil], ax
        mul word [cil_sec]          ; menos la primera pista, que se deja
        sub ax, [d_spp]             ; (ahí va el sector de particiones)
        sbb dx, 0
        jnz .pequeno                ; (no puede pasar de 16 bits)
        mov [tam], ax
        cmp ax, 256
        jae .c2
.pequeno:
        mov si, m_e_pequeno
        jmp error
.c2:
        cmp byte [sin_preguntar], 0
        jne .va
        mov si, m_crear
        call pon_cad
        mov ax, [tam]
        xor dx, dx
        call megas
        call pon_num
        mov si, m_crear2
        call confirma
        je .va
        jmp no_quiere
.va:
        mov di, mbr + 0x1BE         ; la primera fila
        mov [fila], di
        mov ax, [d_spp]             ; empieza en la segunda pista
        xor dx, dx
        call chs
        mov byte [di], 0x80
        mov al, 4                   ; FAT16 desde 16 megas; si no, FAT12
        cmp word [tam], 32680
        jae .tipo
        mov al, 1
.tipo:
        mov [di+4], al
        mov ax, [d_spp]
        mov [di+8], ax
        mov word [di+10], 0
        mov ax, [tam]
        mov [di+12], ax
        mov word [di+14], 0
        xor dx, dx
        add ax, [d_spp]             ; y acaba en su último sector
        adc dx, 0
        sub ax, 1
        sbb dx, 0
        add di, 4
        call chs
        mov si, [fila]
        call duro_codigo
        call duro_activa
        call duro_escribe
        jc mal_escribir
        ; su primer sector, a cero: sin formato
        mov di, sec
        mov cx, 256
        xor ax, ax
        rep stosw
        mov ax, [d_spp]
        mov [d_base], ax
        xor ax, ax
        mov bx, sec
        call disco_escribe
        jc mal_escribir
        mov si, m_hecho_c
bien:
        call pon_cad
        call avisa_nucleo
        mov ax, 0x4C00
        int 0x21

; el sector DX:AX del disco -> en [di+1], su cara, su sector y su cilindro,
; como van en la tabla de particiones. Conserva DI
chs:
        div word [d_spp]            ; AX = pista x caras + cara, DX = sector - 1
        mov cx, dx
        inc cx
        xor dx, dx
        div word [d_caras]          ; AX = cilindro, DX = cara
        cmp ax, 1023                ; (la tabla no sabe decir más)
        jbe .c
        mov ax, 1023
.c:
        mov [di+1], dl
        ror ah, 1
        ror ah, 1
        or cl, ah
        mov [di+2], cl
        mov [di+3], al
        ret

; DX:AX sectores -> AX megas (redondeando)
megas:
        add ax, 1024
        adc dx, 0
        mov cx, 11
.m:
        shr dx, 1
        rcr ax, 1
        loop .m
        ret

; ---- la tabla, en la pantalla ----
ver:
        mov si, m_disco
        call pon_cad
        mov ax, [d_cil]
        call pon_num
        mov si, m_disco2
        call pon_cad
        mov ax, [d_caras]
        call pon_num
        mov si, m_disco3
        call pon_cad
        mov ax, [d_spp]
        call pon_num
        mov si, m_disco4
        call pon_cad
        mov ax, [d_cil]             ; cilindros x sectores de cada uno
        mul word [cil_sec]
        call megas
        call pon_num
        mov si, m_megas
        call pon_cad
        mov si, mbr + 0x1BE
        mov byte [numero], 0
        xor bp, bp                  ; cuántas hay
.fila:
        inc byte [numero]
        cmp byte [si+4], 0
        jne .f0
        jmp .sig
.f0:
        test bp, bp
        jnz .f1
        push si
        mov si, m_cabecera
        call pon_cad
        pop si
.f1:
        inc bp
        mov al, ' '
        call pon_car
        mov al, [numero]
        add al, '0'
        call pon_car
        push si
        mov bx, si
        mov si, m_no
        cmp byte [bx], 0x80
        jne .f2
        mov si, m_si
.f2:
        call pon_cad
        mov al, [bx+4]              ; el tipo, con su nombre si lo conocemos
        mov si, t_fat12
        cmp al, 1
        je .f3
        mov si, t_fat16
        cmp al, 4
        je .f3
        mov si, t_fat16g
        cmp al, 6
        je .f3
        mov si, t_ext
        cmp al, 5
        je .f3
        mov si, t_otra
.f3:
        call pon_cad
        mov al, [bx+4]
        call pon_hex
        mov al, 'h'
        call pon_car
        mov si, m_sep
        call pon_cad
        mov al, [bx+3]              ; del cilindro...
        mov ah, [bx+2]
        call .cil
        mov al, '-'
        call pon_car
        mov al, [bx+7]              ; ...al cilindro
        mov ah, [bx+6]
        call .cil
        mov si, m_sep
        call pon_cad
        mov ax, [bx+12]
        mov dx, [bx+14]
        call megas
        call pon_num
        mov si, m_megas
        call pon_cad
        pop si
.sig:
        add si, 16
        cmp byte [numero], 4
        jae .fin
        jmp .fila
.fin:
        test bp, bp
        jnz .x
        mov si, m_ninguna
        call pon_cad
.x:
        mov si, m_uso
        call pon_cad
        mov ax, 0x4C00
        int 0x21
; (interna) escribe el cilindro de AL (los ocho bits bajos) y AH (los dos
; altos, arriba)
.cil:
        rol ah, 1
        rol ah, 1
        and ah, 3
        jmp pon_num

; escribe AL en hexadecimal, con dos cifras
pon_hex:
        push ax
        push cx
        mov ah, al
        mov cl, 4
        shr al, cl
        call .c
        mov al, ah
        and al, 0x0F
        call .c
        pop cx
        pop ax
        ret
.c:
        add al, '0'
        cmp al, '9'
        jbe .p
        add al, 7
.p:
        jmp pon_car

no_quiere:
        mov si, m_nada
        call pon_cad
        mov ax, 0x4C05
        int 0x21
; escribe el mensaje de SI y termina con el código 4
error:
        call pon_cad
        mov ax, 0x4C04
        int 0x21

m_uso       db 10, "PARTIR - las particiones del disco duro", 10
            db "  PARTIR                 enseña la tabla", 10
            db "  PARTIR CREAR           crea la partición de DOS (hasta 32 megas)", 10
            db "  PARTIR ACTIVAR n       la partición n pasa a ser la que arranca", 10
            db "  PARTIR BORRAR n        borra la partición n", 10
            db "Con /A no pregunta. Después de CREAR: FORMATEA C: /S", 10, 0
m_disco     db "Disco duro: ", 0
m_disco2    db " cilindros, ", 0
m_disco3    db " caras, ", 0
m_disco4    db " sectores por pista: ", 0
m_megas     db " megas", 10, 0
m_cabecera  db " N  Activa  Tipo                    Cilindros  Tamaño", 10, 0
m_si        db "  sí      ", 0
m_no        db "  no      ", 0
m_sep       db "  ", 0
t_fat12     db "DOS, FAT12         ", 0
t_fat16     db "DOS, FAT16         ", 0
t_fat16g    db "DOS, FAT16 grande  ", 0
t_ext       db "extendida          ", 0
t_otra      db "de otro sistema    ", 0
m_ninguna   db "No hay ninguna partición.", 10, 0
m_crear     db "Se va a crear una partición de DOS de ", 0
m_crear2    db " megas en el disco duro.", 10
            db "Pulse S para seguir (otra tecla lo deja): ", 0
m_activar   db "La partición "
.n          db "1 pasará a ser la que arranca.", 10
            db "Pulse S para seguir (otra tecla lo deja): ", 0
m_borrar    db "Atención: se va a BORRAR la partición "
.n          db "1 (", 0
m_borrar2   db " megas) con todo lo que tiene.", 10
            db "Pulse S para seguir (otra tecla lo deja): ", 0
m_nada      db "No se ha tocado nada.", 10, 0
m_hecho_c   db "Partición creada y activa. Ahora dele formato: FORMATEA C: /S", 10, 0
m_hecho_a   db "Partición activada.", 10, 0
m_hecho_b   db "Partición borrada.", 10, 0
m_e_sin_duro db "No hay disco duro.", 10, 0
m_e_no_hay  db "Esa partición no existe.", 10, 0
m_e_ya_hay  db "El disco ya tiene particiones: hay que borrarlas antes (PARTIR BORRAR n).", 10, 0
m_e_pequeno db "El disco es demasiado pequeño.", 10, 0
m_e_escribir db "Error al escribir en el disco.", 10, 0

; el código del sector de particiones, tal como sale de ensamblar
mbr_codigo:
        incbin "../mbr.bin"

        absolute $
            alignb 2
bss_ini:
que         resb 1                  ; la primera letra de lo que se pide
numero      resb 1
            alignb 2
fila        resw 1
cil_sec     resw 1                  ; sectores de un cilindro
n_cil       resw 1
tam         resw 1                  ; sectores de la partición nueva
            UTIL_BSS
sec         resb 512
bss_fin:
