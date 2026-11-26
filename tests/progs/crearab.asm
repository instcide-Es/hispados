; CREARAB.COM - crear (3Ch, 5Ah y 16h) un archivo que está abierto: MS-DOS
; con SHARE lo niega (20h); vaciarlo dejaría al otro handle leyendo grupos
; ya libres. Lo abierto por FCB por el mismo proceso sí se recicla.
; Creating a file that is open elsewhere -> sharing violation.
        cpu 386
        bits 16
        org 0x100

        ; 1) CA.BIN con 3000 octetos (varios grupos)
        mov dx, fname
        xor cx, cx
        mov ah, 0x3C
        int 0x21
        jc fail
        mov bx, ax
        mov dx, buf
        mov cx, 3000
        mov ah, 0x40
        int 0x21
        jc fail
        mov ah, 0x3E
        int 0x21
        ; 2) abrirlo para leer y crearlo encima
        mov ax, 0x3D00
        mov dx, fname
        int 0x21
        jc fail
        mov [h1], ax
        mov dx, fname
        xor cx, cx
        mov ah, 0x3C
        int 0x21
        mov dx, m_3c_bad
        jnc .p2
        cmp ax, 0x20
        jne .p2
        mov dx, m_3c_ok
.p2:
        call say
        ; 5Ah (temporal) no choca: nombre nuevo. 16h (FCB) sobre el abierto
        mov dx, fcb
        mov ah, 0x16
        int 0x21
        mov dx, m_16_bad
        cmp al, 0xFF
        jne .p3
        mov dx, m_16_ok
.p3:
        call say
        ; 3) el handle abierto sigue leyendo lo suyo
        mov bx, [h1]
        mov dx, rbuf
        mov cx, 3000
        mov ah, 0x3F
        int 0x21
        mov dx, m_rd_bad
        jc .p4
        cmp ax, 3000
        jne .p4
        cmp byte [rbuf+2999], 'j'
        jne .p4
        mov dx, m_rd_ok
.p4:
        call say
        mov bx, [h1]
        mov ah, 0x3E
        int 0x21
        ; 4) cerrado: ahora sí
        mov dx, fname
        xor cx, cx
        mov ah, 0x3C
        int 0x21
        mov dx, m_cl_bad
        jc .p5
        mov bx, ax
        mov dx, buf
        mov cx, 1000
        mov ah, 0x40
        int 0x21
        mov ah, 0x3E
        int 0x21
        mov dx, m_cl_ok
.p5:
        call say
        ; 5) abierto por FCB (sin cerrar, como hacen los programas de FCB) y
        ;    creado con 3Ch por el mismo proceso: se permite
        mov dx, fcb
        mov ah, 0x0F
        int 0x21
        cmp al, 0
        jne fail
        mov dx, fname
        xor cx, cx
        mov ah, 0x3C
        int 0x21
        mov dx, m_fcb_bad
        jc .p6
        mov bx, ax
        mov dx, buf
        mov cx, 700
        mov ah, 0x40
        int 0x21
        mov ah, 0x3E
        int 0x21
        mov dx, m_fcb_ok
.p6:
        call say
        mov dx, m_end
        call say
        mov ax, 0x4C00
        int 0x21
fail:
        mov dx, m_fail
        call say
        mov ax, 0x4C01
        int 0x21
say:
        mov ah, 9
        int 0x21
        ret

h1      dw 0
fname   db "CA.BIN", 0
fcb     db 0, "CA      BIN"
        times 37 db 0
m_3c_ok   db "CREAR-ABIERTO: 3Ch denegado (20h)", 13, 10, "$"
m_3c_bad  db "CREAR-ABIERTO: 3Ch ACEPTADO con el archivo abierto", 13, 10, "$"
m_16_ok   db "CREAR-ABIERTO: 16h denegado", 13, 10, "$"
m_16_bad  db "CREAR-ABIERTO: 16h ACEPTADO con el archivo abierto", 13, 10, "$"
m_rd_ok   db "CREAR-ABIERTO: el handle abierto lee sus 3000 octetos", 13, 10, "$"
m_rd_bad  db "CREAR-ABIERTO: el handle abierto NO lee lo suyo", 13, 10, "$"
m_cl_ok   db "CREAR-ABIERTO: tras cerrar, 3Ch aceptado", 13, 10, "$"
m_cl_bad  db "CREAR-ABIERTO: tras cerrar, 3Ch NEGADO", 13, 10, "$"
m_fcb_ok  db "CREAR-ABIERTO: sobre un FCB propio sin cerrar, aceptado", 13, 10, "$"
m_fcb_bad db "CREAR-ABIERTO: sobre un FCB propio sin cerrar, NEGADO", 13, 10, "$"
m_end     db "Fin de CREARAB", 13, 10, "$"
m_fail    db "Error inesperado", 13, 10, "$"
buf:
%assign i 0
%rep 3000
        db 'a' + (i / 300)
%assign i i+1
%endrep
rbuf    times 3000 db 0
