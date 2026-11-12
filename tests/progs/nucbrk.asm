; NUCBRK.COM - pruebas de Ctrl+C, INT 23h, INT 28h y teclado. Modo en la cola:
;   R  manejador de INT 23h que vuelve con RETF y CF=0: el programa sigue
;   A  manejador que vuelve con RETF y CF=1: DOS termina el programa
;   P  lanza "NUCBRK A" y escribe el código de retorno (4Dh)
;   O  escribe sin parar con la función 02h: Ctrl+C debe cortarlo
;   B  bucle con la función 0Bh: Ctrl+C debe cortarlo
;   I  cuenta las llamadas a INT 28h mientras espera una tecla (08h)
;   K  0Bh + 08h: escribe cada tecla entre corchetes hasta la 'q'
;   D  divide entre cero: DOS avisa y termina el programa
;   Z  lee de la consola con 3Fh hasta el fin de fichero (Ctrl+Z, Intro)
;   T  escribe "T:x<TAB>y": el tabulador llega a la columna 8
; Ctrl+C / INT 23h / INT 28h / keyboard tests.
        cpu 386
        bits 16
        org 0x100
        mov sp, pila_fin
        mov bx, fin + 15
        shr bx, 4
        mov ah, 0x4A
        int 0x21
        mov al, [0x82]
        and al, 0xDF
        cmp al, 'R'
        je modo_r
        cmp al, 'A'
        je modo_a
        cmp al, 'P'
        je modo_p
        cmp al, 'O'
        je modo_o
        cmp al, 'B'
        je modo_b
        cmp al, 'I'
        je modo_i
        cmp al, 'K'
        je modo_k
        cmp al, 'T'
        je modo_t
        cmp al, 'D'
        je modo_d
        cmp al, 'Z'
        je modo_z
        mov ax, 0x4C01
        int 0x21

; ---- R: RETF con CF=0 ----
modo_r:
        mov dx, h_sigue
        mov ax, 0x2523
        int 0x21
        mov dx, m_listo
        call puts
.l:
        mov ah, 8
        int 0x21
        cmp al, 'q'
        jne .l
        mov dx, m_r
        call puts
        mov al, [veces]
        add al, '0'
        call putc
        call crlf
        mov ax, 0x4C00
        int 0x21
h_sigue:
        inc byte [cs:veces]
        clc
        retf

; ---- A: RETF con CF=1 ----
modo_a:
        mov dx, h_aborta
        mov ax, 0x2523
        int 0x21
        mov dx, m_listo
        call puts
.l:
        mov ah, 8
        int 0x21
        cmp al, 'q'
        jne .l
        mov dx, m_amal
        call puts
        mov ax, 0x4C00
        int 0x21
h_aborta:
        stc
        retf

; ---- P: padre de "NUCBRK A" ----
modo_p:
        mov [pb+4], cs
        mov [pb+8], cs
        mov [pb+12], cs
        mov dx, yo
        mov bx, pb
        mov ax, 0x4B00
        int 0x21
        push cs
        pop ds
        push cs
        pop es
        cli
        push cs
        pop ss
        mov sp, pila_fin
        sti
        mov ah, 0x4D
        int 0x21
        push ax
        mov dx, m_p
        call puts
        pop ax
        call hex4
        call crlf
        mov ax, 0x4C00
        int 0x21

; ---- O: salida continua ----
modo_o:
        mov dx, m_listo
        call puts
        mov cx, 40                  ; como mucho ~40 x 2000 caracteres
.o1:
        push cx
        mov cx, 2000
.o2:
        mov dl, '.'
        mov ah, 2
        int 0x21
        loop .o2
        pop cx
        loop .o1
        mov dx, m_omal
        call puts
        mov ax, 0x4C00
        int 0x21

; ---- B: bucle con 0Bh ----
modo_b:
        mov dx, m_listo
        call puts
        xor ax, ax
        mov es, ax
        mov si, [es:0x46C]
.b:
        mov ah, 0x0B
        int 0x21
        mov ax, [es:0x46C]
        sub ax, si
        cmp ax, 110                 ; ~6 s
        jb .b
        mov dx, m_bmal
        call puts
        mov ax, 0x4C00
        int 0x21

; ---- I: INT 28h durante la espera ----
modo_i:
        mov ax, 0x3528
        int 0x21
        mov [ant28], bx
        mov [ant28+2], es
        mov dx, h_28
        mov ax, 0x2528
        int 0x21
        mov dx, m_listo
        call puts
        mov ah, 8
        int 0x21
        push ds
        lds dx, [ant28]
        mov ax, 0x2528
        int 0x21
        pop ds
        mov dx, m_iok
        cmp word [n28], 0
        jne .p
        mov dx, m_imal
.p:
        call puts
        mov ax, 0x4C00
        int 0x21
h_28:
        inc word [cs:n28]
        ; desde INT 28h se puede llamar a las funciones de DOS por encima de 0Ch
        push ax
        push bx
        mov ah, 0x51
        int 0x21
        pop bx
        pop ax
        jmp far [cs:ant28]

; ---- K: 0Bh + 08h ----
modo_k:
        mov dx, m_listo
        call puts
.k:
        mov ah, 0x0B
        int 0x21
        test al, al
        jz .k
        mov ah, 8
        int 0x21
        push ax
        mov al, '['
        call putc
        pop ax
        push ax
        call putc
        mov al, ']'
        call putc
        pop ax
        cmp al, 'q'
        jne .k
        call crlf
        mov dx, m_k
        call puts
        mov ax, 0x4C00
        int 0x21

; ---- D: división entre cero ----
modo_d:
        mov dx, m_listo
        call puts
        xor bx, bx
        mov ax, 1
        xor dx, dx
        div bx
        mov dx, m_dmal
        call puts
        mov ax, 0x4C00
        int 0x21

; ---- Z: leer la consola hasta Ctrl+Z ----
modo_z:
        mov dx, m_listo
        call puts
        xor di, di                  ; octetos leídos
.z:
        mov dx, bufz
        mov cx, 80
        xor bx, bx
        mov ah, 0x3F
        int 0x21
        jc .zmal
        test ax, ax
        jz .zfin
        add di, ax
        cmp byte [bufz], 'q'        ; la prueba se rinde con una 'q'
        jne .z
.zmal:
        mov dx, m_zmal
        call puts
        mov ax, 0x4C00
        int 0x21
.zfin:
        mov dx, m_z
        call puts
        mov ax, di
        call hex4
        call crlf
        mov ax, 0x4C00
        int 0x21

; ---- T: tabulador ----
modo_t:
        mov dx, m_t
        call puts
        mov ax, 0x4C00
        int 0x21

puts:
        mov ah, 9
        int 0x21
        ret
putc:
        mov dl, al
        mov ah, 2
        int 0x21
        ret
crlf:
        mov al, 13
        call putc
        mov al, 10
        jmp putc
hex4:
        mov cx, 4
.h:
        rol ax, 4
        push ax
        and al, 15
        add al, '0'
        cmp al, '9'
        jbe .d
        add al, 7
.d:
        call putc
        pop ax
        loop .h
        ret

m_listo db "BRK listo", 13, 10, "$"
m_r     db "BRK-R sigue, veces=$"
m_amal  db "BRK-A MAL: no ha terminado", 13, 10, "$"
m_p     db "BRK-P retorno=$"
m_omal  db 13, 10, "BRK-O MAL: sin corte", 13, 10, "$"
m_bmal  db "BRK-B MAL: sin corte", 13, 10, "$"
m_iok   db "BRK-I OK", 13, 10, "$"
m_imal  db "BRK-I FALLO: sin INT 28h", 13, 10, "$"
m_t     db 13, 10, "T:x", 9, "y", 13, 10, "$"
m_dmal  db "BRK-D MAL: sigue tras dividir entre cero", 13, 10, "$"
m_z     db "BRK-Z fin, leidos=$"
m_zmal  db "BRK-Z MAL: sin fin de fichero", 13, 10, "$"
m_k     db "BRK-K fin", 13, 10, "$"
yo      db "A:\BIN\NUCBRK.COM", 0
cola_a  db 2, " A", 13
pb      dw 0, cola_a, 0, 0x5C, 0, 0x6C, 0
bufz    times 82 db 0
veces   db 0
n28     dw 0
ant28   dd 0
        times 512 db 0
pila_fin:
fin:
