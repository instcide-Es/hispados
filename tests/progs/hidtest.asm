; HIDTEST.COM - prueba del analizador de descriptores de informe HID de los
; mandos (lib/hidjoyp.inc) y del paso de informes a estado (lib/hidjoyx.inc),
; el mismo código que lleva HSPUSB.SYS, con descriptores de mandos reales
; (tests/progs/hidmandos.inc) y un informe de ejemplo de cada uno.
; Escribe el mapa que saca y el estado que lee; tests/run_joy_tests.sh lo
; compara con lo que dicen los descriptores leídos a mano.
        cpu 386
        bits 16
        org 0x100

        mov si, casos
.c:
        mov bx, [si]
        test bx, bx
        jz .fin
        push si
        mov si, bx
        call puts
        pop si
        push si
        mov cx, [si+4]
        mov di, mapa
        mov si, [si+2]
        call hj_parse
        pop si
        jnc .ok
        push si
        mov si, m_no
        call puts
        pop si
        jmp .sig
.ok:
        push si
        call ver_mapa
        pop si
        ; estado inicial y el informe de ejemplo
        mov di, estado
        mov dword [di+HJS_EJES], 0x80808080
        mov word [di+HJS_EJES+4], 0
        mov byte [di+HJS_CRUZ], 0
        mov word [di+HJS_BTN], 0
        push si
        mov bx, [si+6]
        mov cx, [si+8]
        mov si, mapa
        ; el informe, al búfer de 64 bytes (como en HSPUSB)
        push si
        push di
        push cx
        mov si, bx
        mov di, bufer
        cld
        rep movsb
        pop cx
        pop di
        pop si
        mov bx, bufer
        call hj_extract
        jnc .ex
        mov si, m_otro
        call puts
        jmp .ex2
.ex:
        call ver_estado
.ex2:
        pop si
.sig:
        add si, 10
        jmp .c
.fin:
        ; el mando tipo Xbox 360 (mapa fijo)
        mov si, n_x360
        call puts
        mov di, mapa
        call hj_xbox360
        call ver_mapa
        mov di, estado
        mov dword [di+HJS_EJES], 0x80808080
        mov word [di+HJS_EJES+4], 0
        mov si, r_x360
        mov di, bufer
        mov cx, 20
        cld
        rep movsb
        mov si, mapa
        mov di, estado
        mov bx, bufer
        mov cx, 20
        call hj_extract
        call ver_estado
        ; un informe que no es de entrada (tipo 01h: los LED) no cambia nada
        mov byte [bufer], 1
        mov si, mapa
        mov di, estado
        mov bx, bufer
        mov cx, 3
        call hj_extract
        jnc .mal
        mov si, m_led
        call puts
.mal:
        ; un informe corto no toca los campos que no llegan
        mov byte [bufer], 0
        mov byte [bufer+2], 0x10    ; Start
        mov byte [bufer+3], 0
        mov si, mapa
        mov di, estado
        mov bx, bufer
        mov cx, 4
        call hj_extract
        mov si, m_corto
        call puts
        call ver_estado
        ; y lo que no es un mando: un teclado y un ratón
        mov si, d_kbd
        mov cx, d_kbd_len
        mov di, mapa
        call hj_parse
        jnc .k
        mov si, m_kbd
        call puts
.k:
        mov si, d_raton
        mov cx, d_raton_len
        mov di, mapa
        call hj_parse
        jnc .m
        mov si, m_raton
        call puts
.m:
        ; un descriptor cortado a la mitad no cuelga ni desborda
        mov si, d_ds4
        mov cx, 37
        mov di, mapa
        call hj_parse
        mov si, m_cortado
        call puts
        mov ax, 0x4C00
        int 0x21

; el mapa en una línea
ver_mapa:
        mov si, m_id
        call puts
        movzx eax, byte [mapa+HJM_ID]
        test byte [mapa+HJM_FLAGS], 1
        jnz .i
        mov eax, 0
        mov si, m_sinid
        call puts
        jmp .i2
.i:
        call putnum
.i2:
        mov si, m_ejes
        call puts
        movzx eax, byte [mapa+HJM_NAX]
        call putnum
        mov si, m_bot
        call puts
        movzx eax, byte [mapa+HJM_NBTN]
        call putnum
        call crlf
        mov al, ' '
        call putc
        xor bx, bx
        mov di, mapa+HJM_AX
.a:
        cmp word [di], 0xFFFF
        je .an
        mov al, ' '
        call putc
        mov si, bx
        shl si, 1
        mov ax, [n_ejes+si]
        call putc
        mov al, ah
        call putc
        mov al, '='
        call putc
        movzx eax, word [di]
        call putnum
        mov al, '/'
        call putc
        movzx eax, byte [di+HJA_BITS]
        call putnum
        test byte [di+HJA_FL], 1
        jz .a1
        mov al, 's'
        call putc
.a1:
        test byte [di+HJA_FL], 2
        jz .an
        mov al, 'i'
        call putc
.an:
        add di, HJA_SZ
        inc bx
        cmp bx, 6
        jb .a
        cmp word [mapa+HJM_HAT], 0xFFFF
        je .nh
        mov si, m_hat
        call puts
        movzx eax, word [mapa+HJM_HAT]
        call putnum
        mov al, '/'
        call putc
        movzx eax, byte [mapa+HJM_HAT+2]
        call putnum
.nh:
        cmp word [mapa+HJM_DPAD], 0xFFFF
        je .nd
        mov si, m_dpad
        call puts
        movzx eax, word [mapa+HJM_DPAD]
        call putnum
.nd:
        mov si, m_b1
        call puts
        xor bx, bx
.b:
        movzx eax, word [mapa+HJM_BTN+bx]
        cmp ax, 0xFFFF
        jne .b1
        mov al, '-'
        call putc
        jmp .b2
.b1:
        call putnum
.b2:
        mov al, ' '
        call putc
        add bx, 2
        cmp bx, 32
        jb .b
        call crlf
        ret

ver_estado:
        mov si, m_est
        call puts
        xor bx, bx
.e:
        movzx eax, byte [estado+HJS_EJES+bx]
        call putnum
        mov al, ' '
        call putc
        inc bx
        cmp bx, 6
        jb .e
        mov si, m_cruz
        call puts
        movzx eax, byte [estado+HJS_CRUZ]
        call putnum
        mov si, m_btn
        call puts
        mov ax, [estado+HJS_BTN]
        call puthex4
        call crlf
        ret

%include "util.inc"
%include "hidjoyx.inc"
%include "hidjoyp.inc"
%include "tests/progs/hidmandos.inc"

; nombre, descriptor, bytes, informe de ejemplo, bytes
casos:
        dw n_ds4, d_ds4, d_ds4_len, r_ds4, 10
        dw n_dsense, d_dsense, d_dsense_len, r_dsense, 11
        dw n_swpro, d_swpro, d_swpro_len, r_swpro, 12
        dw n_luna, d_luna, d_luna_len, r_luna, 10
        dw n_stadia, d_stadia, d_stadia_len, r_stadia, 10
        dw n_gen, d_gen, d_gen_len, r_gen, 8
        dw n_vuelo, d_vuelo, d_vuelo_len, r_vuelo, 6
        dw 0
n_ds4       db "DualShock 4:", 0
n_dsense    db "DualSense:", 0
n_swpro     db "Switch Pro:", 0
n_luna      db "Luna:", 0
n_stadia    db "Stadia:", 0
n_gen       db "Generico:", 0
n_vuelo     db "Vuelo:", 0
n_x360      db "Xbox 360:", 0
; informes de ejemplo (según el formato que describe cada descriptor)
; DS4: palanca izq. a la izquierda, der. abajo, cruceta izquierda, Cruz y L1,
;      L2 a 40h, R2 a fondo
r_ds4       db 0x01, 0x00, 0x80, 0x80, 0xFF, 0x26, 0x01, 0x00, 0x40, 0xFF
; DualSense: palanca izq. arriba, gatillo der. a fondo, cruceta suelta, Cuadrado
r_dsense    db 0x01, 0x80, 0x00, 0x80, 0x80, 0x00, 0xFF, 0x07, 0x18, 0x00, 0x00
; Switch Pro: botones 1 y 3, X = 0, Y = 65535, cruceta a la derecha
r_swpro     db 0x30, 0x05, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x80, 0x00, 0x80, 0x02
; Luna: botones 1, 2 y 12, cruceta arriba, X a la derecha, gatillos 10h y 20h
r_luna      db 0x01, 0x03, 0x08, 0x00, 0xFF, 0x80, 0x80, 0x80, 0x10, 0x20
; Stadia: cruceta suelta, botón 1, X a la izquierda, Y abajo, acelerador a fondo
r_stadia    db 0x03, 0x08, 0x00, 0x40, 0x01, 0xFF, 0x80, 0x80, 0x00, 0xFF
; mando genérico sin identificador de informe: X, Y de 8 bits con signo
; (-127 a 127), cruceta de 4 botones y 8 botones (con Push y Pop por medio)
d_gen:
        db 0x05, 0x01, 0x09, 0x05, 0xA1, 0x01           ; Game Pad
        db 0x09, 0x30, 0x09, 0x31, 0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x02, 0x81, 0x02
        db 0xA4                                         ; Push
        db 0x05, 0x09, 0x19, 0x01, 0x29, 0x08, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02
        db 0xB4                                         ; Pop (página 1, 8 bits)
        db 0x75, 0x01, 0x95, 0x04, 0x15, 0x00, 0x25, 0x01
        db 0x09, 0x90, 0x09, 0x91, 0x09, 0x92, 0x09, 0x93, 0x81, 0x02
        db 0x95, 0x04, 0x81, 0x03                       ; relleno
        db 0xFE, 0x02, 0x00, 0xAA, 0xBB                 ; un elemento largo: se salta
        db 0xC0
d_gen_len   equ $ - d_gen
; X = -127 (izquierda), Y = 127, botones 2 y 8, cruceta arriba y derecha
r_gen       db 0x81, 0x7F, 0x82, 0x05, 0, 0, 0, 0
; joystick de vuelo: X, Y de 10 bits, timón (Rz) y gases (Slider) de 8,
; cruceta de 4 posiciones (1-4, 0 = suelta) y 4 botones
d_vuelo:
        db 0x05, 0x01, 0x09, 0x04, 0xA1, 0x01           ; Joystick
        db 0x09, 0x01, 0xA1, 0x00                       ; Pointer, física
        db 0x09, 0x30, 0x09, 0x31, 0x15, 0x00, 0x26, 0xFF, 0x03, 0x75, 0x0A, 0x95, 0x02, 0x81, 0x02
        db 0xC0
        db 0x09, 0x39, 0x15, 0x01, 0x25, 0x04, 0x75, 0x04, 0x95, 0x01, 0x81, 0x42
        db 0x09, 0x35, 0x09, 0x36, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x02, 0x81, 0x02
        db 0x05, 0x09, 0x19, 0x01, 0x29, 0x04, 0x25, 0x01, 0x75, 0x01, 0x95, 0x04, 0x81, 0x02
        db 0x95, 0x04, 0x81, 0x01
        db 0xC0
d_vuelo_len equ $ - d_vuelo
; X = 1023, Y = 0, cruceta 3 (abajo), timón 0, gases 255, botón 3
r_vuelo     db 0xFF, 0x03, 0x30, 0x00, 0xFF, 0x04
; Xbox 360: arriba + A + RB, gatillo izq. 80h, X izq. -32768, Y izq. +32767
r_x360      db 0x00, 0x14, 0x01, 0x12, 0x80, 0x00, 0x00, 0x80, 0xFF, 0x7F
            db 0x00, 0x00, 0x00, 0x00, 0, 0, 0, 0, 0, 0
; teclado y ratón de arranque (los descriptores de ejemplo de HID 1.11, E.6 y E.10)
d_kbd:
        db 0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00, 0x25, 0x01
        db 0x75, 0x01, 0x95, 0x08, 0x81, 0x02, 0x95, 0x01, 0x75, 0x08, 0x81, 0x01, 0x95, 0x05, 0x75, 0x01
        db 0x05, 0x08, 0x19, 0x01, 0x29, 0x05, 0x91, 0x02, 0x95, 0x01, 0x75, 0x03, 0x91, 0x01, 0x95, 0x06
        db 0x75, 0x08, 0x15, 0x00, 0x25, 0x65, 0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00, 0xC0
d_kbd_len   equ $ - d_kbd
d_raton:
        db 0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x09, 0x01, 0xA1, 0x00, 0x05, 0x09, 0x19, 0x01, 0x29, 0x03
        db 0x15, 0x00, 0x25, 0x01, 0x95, 0x03, 0x75, 0x01, 0x81, 0x02, 0x95, 0x01, 0x75, 0x05, 0x81, 0x01
        db 0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x02, 0x81, 0x06
        db 0xC0, 0xC0
d_raton_len equ $ - d_raton
n_ejes      db "X ", "Y ", "X2", "Y2", "L ", "R "
m_no        db " NO es un mando", 13, 10, 0
m_otro      db "  el informe no es suyo", 13, 10, 0
m_id        db " id=", 0
m_sinid     db "no", 0
m_ejes      db " ejes=", 0
m_bot       db " botones=", 0
m_hat       db " hat=", 0
m_dpad      db " dpad=", 0
m_b1        db " b=", 0
m_est       db "  estado: ", 0
m_cruz      db "cruz=", 0
m_btn       db " botones=", 0
m_led       db "Xbox 360: informe de LED descartado", 13, 10, 0
m_corto     db "Xbox 360 corto:", 13, 10, 0
m_kbd       db "Teclado: no es un mando", 13, 10, 0
m_raton     db "Raton: no es un mando", 13, 10, 0
m_cortado   db "Descriptor cortado: sin cuelgue", 13, 10, 0
mapa        times HJM_SZ db 0
estado      times HJS_SZ db 0
bufer       times 72 db 0
u_numbuf    times 16 db 0
u_lbuf      times 80 db 0
