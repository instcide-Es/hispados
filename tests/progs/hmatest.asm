; =============================================================================
;  HMATEST.COM - prueba de DOS=HIGH (tests/run_hma_tests.sh)
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  HMATEST       informe: memoria convencional libre (LIBRE=, MAYOR=), si el
;                DOS dice estar en la HMA (INT 21h AX=3306h), la HMA libre
;                (INT 2Fh AX=4A01h), una reserva (4A02h) y el estado de A20
;  HMATEST /C    cierra la línea A20 a propósito (XMS 06h hasta agotar la
;                cuenta local y 04h) y termina: lo que venga después tiene
;                que seguir funcionando
;  HMATEST /A    con A20 cerrada antes de cada llamada: crear, escribir,
;                leer y borrar un fichero, reservar memoria y EXEC de
;                \BIN\HELLO.COM (en la unidad actual)
; =============================================================================
        cpu 386
        bits 16
        org 0x100

start:
        mov sp, pila_fin
        ; soltar la memoria que no se usa (para EXEC)
        mov bx, (fin - start + 0x100 + 15) / 16
        mov ah, 0x4A
        int 0x21
        ; gestor XMS
        mov ax, 0x4300
        int 0x2F
        cmp al, 0x80
        jne .noxms
        mov ax, 0x4310
        int 0x2F
        mov [xms], bx
        mov [xms+2], es
        mov byte [hayxms], 1
.noxms:
        push ds
        pop es
        mov si, 0x81
.sw:
        lodsb
        cmp al, 13
        je informe
        cmp al, '/'
        jne .sw
        lodsb
        and al, 0xDF
        cmp al, 'C'
        je cerrar
        cmp al, 'A'
        je a20off_test
        jmp informe

; ---- informe ----
informe:
        ; memoria libre: recorrer la cadena de MCB
        mov ah, 0x52
        int 0x21
        mov ax, [es:bx-2]
        xor ebx, ebx
        xor edx, edx
.l:
        mov es, ax
        movzx esi, word [es:3]
        cmp word [es:1], 0
        jne .u
        mov ebp, esi
        shl ebp, 4
        add ebx, ebp
        cmp ebp, edx
        jbe .u
        mov edx, ebp
.u:
        cmp byte [es:0], 'Z'
        je .d
        add ax, si
        inc ax
        jmp .l
.d:
        push ds
        pop es
        push edx
        mov si, m_libre
        call puts
        mov eax, ebx
        call putdec
        call crlf
        mov si, m_mayor
        call puts
        pop eax
        call putdec
        call crlf
        ; ¿DOS en la HMA?
        mov ax, 0x3306
        xor dx, dx
        int 0x21
        mov si, m_doshma0
        test dh, 0x10
        jz .nh
        mov si, m_doshma1
.nh:
        call puts
        ; HMA libre
        mov ax, 0x4A01
        xor bx, bx
        mov di, 0xFFFF
        int 0x2F
        push ds
        pop es
        mov [hlibre], bx
        mov [hdir], di
        mov si, m_hmalibre
        call puts
        movzx eax, word [hlibre]
        call putdec
        mov si, m_en
        call puts
        mov ax, [hdir]
        call puthex
        call crlf
        ; reservar 100 octetos: se redondean a 112 y la zona libre encoge
        cmp word [hlibre], 200
        jb .nores
        mov ax, 0x4A02
        mov bx, 100
        int 0x2F
        push ds
        pop es
        cmp bx, 112
        jne .resbad
        cmp di, [hdir]
        jne .resbad
        mov ax, 0x4A01
        int 0x2F
        push ds
        pop es
        mov ax, [hlibre]
        sub ax, 112
        cmp bx, ax
        jne .resbad
        mov ax, [hdir]
        add ax, 112
        cmp di, ax
        jne .resbad
        mov si, m_resok
        call puts
        jmp .nores
.resbad:
        mov si, m_resbad
        call puts
.nores:
        cmp byte [hayxms], 0
        je .fin
        mov ah, 0x07
        call far [xms]
        mov si, m_a20on
        test ax, ax
        jnz .a
        mov si, m_a20off
.a:
        call puts
.fin:
        mov ax, 0x4C00
        int 0x21

; ---- cerrar A20 y terminar ----
cerrar:
        call cierra
        mov si, m_a20off
        test ax, ax
        jz .p
        mov si, m_a20on
.p:
        call puts
        mov ax, 0x4C00
        int 0x21

; cerrar A20 por XMS: 06h hasta agotar la cuenta local y 04h -> AX = estado
cierra:
        cmp byte [hayxms], 0
        je .no
        mov cx, 16
.l:
        mov ah, 0x06
        call far [xms]
        mov ah, 0x04
        call far [xms]
        mov ah, 0x07
        call far [xms]
        test ax, ax
        jz .r
        loop .l
.r:
        ret
.no:
        mov ax, 1
        ret

; ---- cada llamada a INT 21h, con A20 cerrada ----
%macro CIERRA 0
        call cierra
        test ax, ax
        jnz a20_no_cierra
        inc word [veces]
%endmacro
a20off_test:
        CIERRA
        mov ah, 0x3C                ; crear
        xor cx, cx
        mov dx, fname
        int 0x21
        jc fallo
        mov [h], ax
        CIERRA
        mov ah, 0x40                ; escribir
        mov bx, [h]
        mov cx, 1500
        mov dx, datos
        int 0x21
        jc fallo
        cmp ax, 1500
        jne fallo
        CIERRA
        mov ah, 0x3E
        mov bx, [h]
        int 0x21
        jc fallo
        CIERRA
        mov ax, 0x3D00              ; abrir y leer
        mov dx, fname
        int 0x21
        jc fallo
        mov [h], ax
        CIERRA
        mov ah, 0x3F
        mov bx, [h]
        mov cx, 1500
        mov dx, leido
        int 0x21
        jc fallo
        cmp ax, 1500
        jne fallo
        mov si, datos
        mov di, leido
        mov cx, 1500
        repe cmpsb
        jne fallo
        CIERRA
        mov ah, 0x3E
        mov bx, [h]
        int 0x21
        CIERRA
        mov ah, 0x41                ; borrar
        mov dx, fname
        int 0x21
        jc fallo
        CIERRA
        mov ah, 0x48                ; memoria
        mov bx, 0x100
        int 0x21
        jc fallo
        mov es, ax
        CIERRA
        mov ah, 0x49
        int 0x21
        jc fallo
        push ds
        pop es
        CIERRA
        ; EXEC
        mov [pb+4], cs
        mov [pb+8], cs
        mov [pb+12], cs
        mov ax, 0x4B00
        mov dx, hijo
        mov bx, pb
        int 0x21
        mov bx, cs                  ; (EXEC cambia los registros)
        mov ds, bx
        mov es, bx
        mov ss, bx
        mov sp, pila_fin
        jc fallo
        CIERRA
        mov ah, 0x4D
        int 0x21
        CIERRA
        mov si, m_a20ok
        call puts
        movzx eax, word [veces]
        call putdec
        mov si, m_llam
        call puts
        mov ax, 0x4C00
        int 0x21
fallo:
        mov si, m_fallo
        call puts
        mov ax, 0x4C01
        int 0x21
a20_no_cierra:
        mov si, m_nocierra
        call puts
        mov ax, 0x4C02
        int 0x21

; ---- salida ----
puts:
        lodsb
        test al, al
        jz .r
        mov dl, al
        mov ah, 2
        int 0x21
        jmp puts
.r:
        ret
crlf:
        mov si, m_crlf
        jmp puts
; EAX -> decimal
putdec:
        mov ebx, 10
        xor cx, cx
.l:
        xor edx, edx
        div ebx
        push dx
        inc cx
        test eax, eax
        jnz .l
.p:
        pop dx
        add dl, '0'
        mov ah, 2
        int 0x21
        loop .p
        ret
; AX -> FFFF:xxxx
puthex:
        push ax
        mov si, m_ffff
        call puts
        pop ax
        mov cx, 4
.l:
        rol ax, 4
        push ax
        and al, 15
        add al, '0'
        cmp al, '9'
        jbe .o
        add al, 7
.o:
        mov dl, al
        mov ah, 2
        int 0x21
        pop ax
        loop .l
        ret

hayxms      db 0
xms         dd 0
h           dw 0
veces       dw 0
hlibre      dw 0
hdir        dw 0
fname       db "HMATEST.TMP", 0
hijo        db "\BIN\HELLO.COM", 0
cola        db 8, " cerrada", 13
pb          dw 0
            dw cola, 0
            dw 0x5C, 0
            dw 0x6C, 0
m_libre     db "LIBRE=", 0
m_mayor     db "MAYOR=", 0
m_doshma0   db "DOSHMA=0", 13, 10, 0
m_doshma1   db "DOSHMA=1", 13, 10, 0
m_hmalibre  db "HMALIBRE=", 0
m_en        db " en ", 0
m_ffff      db "FFFF:", 0
m_resok     db "HMA reserva OK", 13, 10, 0
m_resbad    db "HMA reserva MAL", 13, 10, 0
m_a20on     db "A20=abierta", 13, 10, 0
m_a20off    db "A20=cerrada", 13, 10, 0
m_a20ok     db "HMATEST A20 OK: ", 0
m_llam      db " llamadas con A20 cerrada", 13, 10, 0
m_fallo     db "HMATEST FALLO", 13, 10, 0
m_nocierra  db "HMATEST: A20 no se cierra", 13, 10, 0
m_crlf      db 13, 10, 0
datos:
%assign i 0
%rep 1500
            db (i * 7 + 3) & 0xFF
%assign i i+1
%endrep
leido       times 1500 db 0
            times 512 db 0
pila_fin:
fin:
