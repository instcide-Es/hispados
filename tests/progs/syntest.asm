; SYNTEST.COM - [RUEDA] prueba del touchpad Synaptics en modo absoluto de
; HSPRATON (el borde derecho hace de rueda) sin touchpad: con HSPRATON
; /DIRECTO /SYNPRUEBA, mete paquetes absolutos de 6 bytes en el puerto
; auxiliar del 8042 (orden D3h, como HSPUSB) y mira lo que da INT 33h:
;  - un dedo en el centro que se mueve: el puntero se mueve (a la derecha y
;    hacia abajo);
;  - un dedo que empieza en el borde derecho y baja 500 unidades: 5 pasos de
;    rueda hacia abajo (función 03h, BH) y nada de movimiento; que sube 300:
;    3 hacia arriba;
;  - un toque (el bit del gesto): una pulsación del botón izquierdo.
; tests/run_rueda_tests.sh compara lo que escribe.
; Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
        cpu 386
        bits 16
        org 0x100

DEDO    equ 0x20
GESTO   equ 0x04

        mov ax, 0x48D2
        int 0x33
        cmp ax, 0x5253
        je .hay
        mov dx, m_nohay
        mov ah, 9
        int 0x21
        mov ax, 0x4C01
        int 0x21
.hay:
        mov ax, 0x11
        xor cx, cx
        int 0x33
        mov dx, m_wno
        test cl, 1
        jz .w
        mov dx, m_wsi
.w:
        mov ah, 9
        int 0x21
        mov ax, 3                   ; (se vacía lo que hubiera)
        int 0x33
        mov [x0], cx
        mov [y0], dx
        ; ---- el puntero: dedo en 3000,3000; 10 paquetes de +16, -16 ----
        mov cx, 3000
        mov dx, 3000
        mov bl, DEDO
        call paquete
        mov si, 10
.p1:
        add cx, 16
        sub dx, 16
        call paquete
        dec si
        jnz .p1
        xor bl, bl                  ; se levanta
        call paquete
        call espera
        mov ax, 3
        int 0x33
        push bx
        sub cx, [x0]
        sub dx, [y0]
        push dx
        mov dx, m_pun
        call pcad
        mov ax, cx
        call psig
        mov dx, m_dy
        call pcad
        pop ax
        call psig
        mov dx, m_rue
        call pcad
        pop bx
        movsx ax, bh
        call psig
        call crlf
        ; ---- el borde: en 5300,4000 y baja 500 en 10 paquetes ----
        mov ax, 3
        int 0x33
        mov [x0], cx
        mov [y0], dx
        mov cx, 5300
        mov dx, 4000
        mov bl, DEDO
        call paquete
        mov si, 10
.p2:
        sub dx, 50
        call paquete
        dec si
        jnz .p2
        xor bl, bl
        call paquete
        call borde
        ; y sube 300 (empieza más abajo: 5200,2000)
        mov ax, 3
        int 0x33
        mov [x0], cx
        mov [y0], dx
        mov cx, 5200
        mov dx, 2000
        mov bl, DEDO
        call paquete
        mov si, 6
.p3:
        add dx, 50
        call paquete
        dec si
        jnz .p3
        xor bl, bl
        call paquete
        call borde
        ; ---- un toque: el gesto, y se suelta ----
        mov ax, 5
        xor bx, bx
        int 0x33                    ; (contadores a cero)
        mov cx, 3000
        mov dx, 3000
        mov bl, GESTO
        call paquete
        call espera
        mov ax, 3
        int 0x33
        push bx
        xor bl, bl
        call paquete
        call espera
        mov dx, m_toq
        call pcad
        pop ax
        and ax, 7
        call psig
        mov ax, 5
        xor bx, bx
        int 0x33
        push ax
        mov dx, m_pul
        call pcad
        mov ax, bx
        call psig
        mov dx, m_tras
        call pcad
        pop ax
        and ax, 7
        call psig
        call crlf
        mov ax, 0x4C00
        int 0x21

; lo del borde: la rueda y si se ha movido el puntero
borde:
        call espera
        mov ax, 3
        int 0x33
        push bx
        sub cx, [x0]
        sub dx, [y0]
        push dx
        mov dx, m_bor
        call pcad
        pop dx
        or cx, dx
        mov dx, m_quieto
        jz .q
        mov dx, m_movido
.q:
        call pcad
        mov dx, m_rue
        call pcad
        pop bx
        movsx ax, bh
        call psig
        call crlf
        ret

; un paquete absoluto: CX = X, DX = Y, BL = dedo, gesto y botones
paquete:
        pusha
        mov al, bl
        or al, 0x80
        call aux
        mov al, dh                  ; Y 8-11 | X 8-11
        shl al, 4
        mov ah, ch
        and ah, 0x0F
        or al, ah
        call aux
        mov al, 60                  ; presión
        test bl, DEDO
        jnz .z
        xor al, al
.z:
        call aux
        mov al, 0xC0                ; 11, Y12, X12
        test dh, 0x10
        jz .y
        or al, 0x20
.y:
        test ch, 0x10
        jz .x
        or al, 0x10
.x:
        call aux
        mov al, cl
        call aux
        mov al, dl
        call aux
        popa
        ret

; AL al búfer de salida del 8042 como si viniera del ratón (D3h); se espera
; a que la IRQ 12 (HSPRATON) lo haya leído
aux:
        push cx
        push ax
        call ibf
        mov al, 0xD3
        out 0x64, al
        call ibf
        pop ax
        out 0x60, al
        mov cx, 0xFFFF
.l:
        in al, 0x64
        test al, 1
        jz .r
        loop .l
.r:
        pop cx
        ret
ibf:
        push cx
        mov cx, 0xFFFF
.l:
        in al, 0x64
        test al, 2
        jz .r
        loop .l
.r:
        pop cx
        ret

; dos tics del reloj
espera:
        push es
        xor ax, ax
        mov es, ax
        mov ax, [es:0x46C]
        add ax, 3
.l:
        sti
        hlt
        cmp ax, [es:0x46C]
        jns .l
        pop es
        ret

pcad:
        push ax
        mov ah, 9
        int 0x21
        pop ax
        ret
; AX con signo
psig:
        pusha
        test ax, ax
        jns .p
        push ax
        mov dl, '-'
        mov ah, 2
        int 0x21
        pop ax
        neg ax
.p:
        xor cx, cx
        mov bx, 10
.d:
        xor dx, dx
        div bx
        push dx
        inc cx
        test ax, ax
        jnz .d
.o:
        pop dx
        add dl, '0'
        mov ah, 2
        int 0x21
        loop .o
        popa
        ret
crlf:
        mov dx, m_crlf
        jmp pcad

x0      dw 0
y0      dw 0
m_nohay db "HSPRATON no está cargado.", 13, 10, "$"
m_wsi   db "Rueda: sí", 13, 10, "$"
m_wno   db "Rueda: no", 13, 10, "$"
m_pun   db "Puntero: dx=$"
m_dy    db " dy=$"
m_rue   db " rueda=$"
m_bor   db "Borde: puntero $"
m_quieto db "quieto$"
m_movido db "movido$"
m_toq   db "Toque: botones=$"
m_pul   db " pulsaciones=$"
m_tras  db " y luego botones=$"
m_crlf  db 13, 10, "$"
