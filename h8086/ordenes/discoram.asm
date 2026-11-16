; =============================================================================
;  DISCORAM.SYS - un disco en la memoria (controlador de dispositivo de bloque)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  En CONFIG.SYS:      DEVICE=\DISCORAM.SYS [tamaño en K]
;
;  Sin tamaño, 64 K; de 16 a 512. El disco recibe la letra siguiente a las que
;  hay (D:), ya con formato (FAT12, 64 entradas en la raíz) y vacío; lo que se
;  guarde en él se pierde al apagar. La memoria que ocupa se le quita a los
;  programas: en un PC de 256 K, un disco de 64 K ya es mucho.
;
;  Es también el ejemplo de cómo se hace un controlador para Hispa-8086, que
;  es como se hacen para MS-DOS: una cabecera, la rutina de «estrategia» (que
;  apunta la petición) y la de «interrupción» (que la atiende). Atiende las
;  órdenes 0 (iniciar), 1 (¿han cambiado el disco?: no), 2 (sus medidas), 4
;  (leer) y 8 y 9 (escribir). Los sectores están detrás del controlador, 32
;  párrafos cada uno. Solo instrucciones del 8086.
; =============================================================================
        cpu 8086
        org 0

cabecera:
        dd -1                       ; no hay otro detrás
        dw 0x0000                   ; de bloque
        dw estrategia
        dw interrupcion
        db 1                        ; una unidad
        times 7 db 0

peticion    dd 0
datos_seg   dw 0                    ; donde empieza el disco
tabla_bpb   dw bpb
; sus medidas, como las pide DOS (y como van en su primer sector)
bpb:
        dw 512                      ; octetos por sector
        db 1                        ; sectores por grupo
        dw 1                        ; reservados
        db 1                        ; una FAT
        dw 64                       ; entradas en la raíz
b_tot   dw 128                      ; sectores
        db 0xFA                     ; un disco en memoria
b_spf   dw 1                        ; sectores de la FAT

estrategia:
        mov [cs:peticion], bx
        mov [cs:peticion+2], es
        retf

interrupcion:
        push ax
        push bx
        push cx
        push dx
        push si
        push di
        push ds
        push es
        push cs
        pop ds
        cld
        les bx, [peticion]
        mov word [es:bx+3], 0x0100  ; hecho, mientras no se diga otra cosa
        mov al, [es:bx+2]
        test al, al
        jnz .n0
        jmp inicia
.n0:
        cmp al, 1
        jne .n1
        mov byte [es:bx+14], 1      ; el disco no ha cambiado
        jmp short fin
.n1:
        cmp al, 2
        jne .n2
        mov word [es:bx+18], bpb
        mov [es:bx+20], cs
        jmp short fin
.n2:
        cmp al, 4
        je .es
        cmp al, 8
        je .es
        cmp al, 9
        jne fin
.es:
        mov ax, [es:bx+20]          ; desde qué sector y cuántos
        mov dx, [es:bx+18]
        mov cx, ax
        add cx, dx
        jc .mal
        cmp cx, [b_tot]
        jbe .cabe
.mal:
        mov word [es:bx+18], 0
        mov word [es:bx+3], 0x8108  ; ese sector no existe
        jmp short fin
.cabe:
        mov cl, 5                   ; cada sector, 32 párrafos
        shl ax, cl
        add ax, [datos_seg]
        mov ch, [es:bx+2]
        les di, [es:bx+14]          ; ES:DI = la memoria de quien pide
        test dx, dx
        jz fin
.sector:
        push ds
        push es
        cmp ch, 4
        jne .escribe
        mov ds, ax                  ; leer: del disco a él
        xor si, si
        jmp short .copia
.escribe:
        push es                     ; escribir: de él al disco
        pop ds
        mov si, di
        mov es, ax
        xor di, di
.copia:
        push cx
        mov cx, 256
        rep movsw
        pop cx
        cmp ch, 4
        je .c1
        mov di, si                  ; (por dónde va en su memoria)
.c1:
        pop es
        pop ds
        add ax, 32
        dec dx
        jnz .sector
fin:
        pop es
        pop ds
        pop di
        pop si
        pop dx
        pop cx
        pop bx
        pop ax
        retf

; ---- lo de aquí abajo solo hace falta al arrancar ----
inicia:
        ; el tamaño: el número que venga detrás del nombre del archivo
        push es
        push bx
        les si, [es:bx+18]
.nom:
        mov al, [es:si]             ; (el nombre)
        cmp al, ' '
        jbe .esp
        inc si
        jmp .nom
.esp:
        mov al, [es:si]
        cmp al, ' '
        jne .num
        inc si
        jmp .esp
.num:
        xor ax, ax
.n:
        mov bl, [es:si]
        sub bl, '0'
        cmp bl, 9
        ja .nf
        cmp ax, 1000
        jae .nf
        mov cx, 10
        mul cx
        xor bh, bh
        add ax, bx
        inc si
        jmp .n
.nf:
        pop bx
        pop es
        test ax, ax
        jnz .t1
        mov ax, 64
.t1:
        cmp ax, 16
        jae .t2
        mov ax, 16
.t2:
        cmp ax, 512
        jbe .t3
        mov ax, 512
.t3:
        mov [m_k], ax
        shl ax, 1
        mov [b_tot], ax
        ; la FAT: doce bits por grupo
        add ax, 2
        mov cx, ax
        shl ax, 1
        add ax, cx
        shr ax, 1
        add ax, 512
        mov cl, 9
        shr ax, cl
        mov [b_spf], ax
        ; dónde va el disco (detrás de todo esto) y dónde acaba
        mov ax, final + 15
        mov cl, 4
        shr ax, cl
        mov dx, cs
        add ax, dx
        mov [datos_seg], ax
        mov dx, [b_tot]
        mov cl, 5
        shl dx, cl
        add dx, ax                  ; DX = el primer segmento que ya no es suyo
        int 0x12                    ; ¿cabe, dejando 96 K a los programas?
        mov cl, 6
        shl ax, cl
        sub ax, 0x1800
        cmp dx, ax
        jbe .cabe
        mov word [es:bx+14], 0      ; no: no se instala
        mov [es:bx+16], cs
        mov dx, m_no_cabe
        mov ah, 9
        int 0x21
        jmp fin
.cabe:
        mov word [es:bx+14], 0
        mov [es:bx+16], dx
        mov byte [es:bx+13], 1
        mov word [es:bx+18], tabla_bpb
        mov [es:bx+20], cs
        mov al, [es:bx+22]
        add al, 'A'
        mov [m_u], al
        ; el formato: arranque (con las medidas), FAT y raíz, a cero; los dos
        ; primeros grupos de la FAT, que no existen; y la etiqueta
        push es
        mov es, [datos_seg]
        mov cx, [b_spf]
        add cx, 5                   ; (el de arranque y los 4 de la raíz)
        xchg ch, cl                 ; x 256 palabras
        xor di, di
        xor ax, ax
        rep stosw
        mov word [es:0], 0xFEEB     ; (no arranca: un salto a sí mismo)
        mov byte [es:2], 0x90
        mov si, m_oem
        mov di, 3
        mov cx, 8
        rep movsb
        mov si, bpb
        mov cx, 13
        rep movsb
        mov word [es:510], 0xAA55
        mov word [es:512], 0xFFFA
        mov byte [es:514], 0xFF
        mov ax, [b_spf]             ; la raíz: detrás de la FAT
        inc ax
        mov cl, 9
        shl ax, cl
        mov di, ax
        mov si, m_etiqueta
        mov cx, 12
        rep movsb
        pop es
        ; y se dice
        mov ax, [m_k]
        mov si, m_k_txt + 2
        mov cx, 10
.d:
        xor dx, dx
        div cx
        add dl, '0'
        mov [si], dl
        dec si
        test ax, ax
        jnz .d
        mov dx, m_hay
        mov ah, 9
        int 0x21
        jmp fin

m_k         dw 0
m_oem       db "DISCORAM"
m_etiqueta  db "DISCO RAM  ", 0x08
m_hay       db "DISCORAM: disco en memoria de "
m_k_txt     db "    K en la unidad "
m_u         db "D:", 13, 10, "$"
m_no_cabe   db "DISCORAM: no hay memoria para ese disco.", 13, 10, "$"
final:
