; JOYTEST.COM - prueba de HSPJOY (tests/run_joy_tests.sh): lo que ve un juego
; por INT 15h AH=84h, el mando clásico y la tabla de mandos de la interfaz
; propia (INT 2Fh AH=D9h).
;   JOYTEST                 lo escribe una vez
;   JOYTEST S desp bytes    antes cambia los datos del simulador de HSPJOYT
;                           (hexadecimal: desplazamiento y bytes)
;   JOYTEST W tics          durante esos tics, escribe la línea de INT 15h
;                           cada vez que cambia
;   JOYTEST R               comprueba que INT 15h conserva los registros
;   JOYTEST P desp bytes    pulsaciones en el simulador: pone cada byte en ese
;                           desplazamiento durante 3 tics y lo quita otros 3
        cpu 386
        bits 16
        org 0x100

        cld
        mov si, 0x81
        call blancos
        mov al, [si]
        and al, 0xDF
        cmp al, 'S'
        je poke
        cmp al, 'W'
        je mirar
        cmp al, 'R'
        je regs
        cmp al, 'P'
        je pulsos
una:
        call int15
        call clasico
        call tabla
        mov ax, 0x4C00
        int 0x21

; ---- JOYTEST S desp bytes ----
poke:
        inc si
        call hex
        mov bx, ax                  ; desplazamiento
        push bx
        mov ax, 0xD97F
        xor di, di
        int 0x2F
        pop bx
        test di, di
        jnz .ok
        mov si, m_nosim
        call puts
        mov ax, 0x4C01
        int 0x21
.ok:
        add di, bx
.l:
        call blancos
        cmp byte [si], 13
        je .fin
        call hex
        cli
        stosb
        sti
        jmp .l
.fin:
        push cs
        pop es
        jmp una

; ---- JOYTEST P desp bytes ----
pulsos:
        inc si
        call hex
        mov bx, ax
        push bx
        mov ax, 0xD97F
        xor di, di
        int 0x2F
        pop bx
        test di, di
        jz .fin
        add di, bx
.l:
        call blancos
        cmp byte [si], 13
        je .fin
        call hex
        mov [es:di], al
        call .tres
        mov byte [es:di], 0
        call .tres
        jmp .l
.fin:
        mov ax, 0x4C00
        int 0x21
.tres:
        push es
        xor ax, ax
        mov es, ax
        mov bx, [es:0x46C]
.t:
        mov ax, [es:0x46C]
        sub ax, bx
        cmp ax, 3
        jb .t
        pop es
        ret

; ---- JOYTEST W tics ----
mirar:
        inc si
        call blancos
        xor bx, bx
.n:
        mov al, [si]
        sub al, '0'
        cmp al, 9
        ja .go
        imul bx, 10
        xor ah, ah
        add bx, ax
        inc si
        jmp .n
.go:
        mov [tics], bx
        mov si, m_mirando
        call puts
        xor ax, ax
        mov es, ax
        mov ax, [es:0x46C]
        mov [t0], ax
.l:
        mov ah, 0x84
        xor dx, dx
        int 0x15
        jc .sig
        mov [b_nuevo], al
        mov ah, 0x84
        mov dx, 1
        int 0x15
        mov [e_nuevo], ax
        mov [e_nuevo+2], bx
        mov [e_nuevo+4], cx
        mov [e_nuevo+6], dx
        mov si, e_nuevo - 1
        mov di, e_viejo - 1
        mov cx, 9
        push es
        push ds
        pop es
        repe cmpsb
        pop es
        je .sig
        mov si, e_nuevo - 1
        mov di, e_viejo - 1
        mov cx, 9
        push es
        push ds
        pop es
        rep movsb
        pop es
        call int15
.sig:
        mov ax, [es:0x46C]
        sub ax, [t0]
        cmp ax, [tics]
        jb .l
        mov si, m_fin
        call puts
        mov ax, 0x4C00
        int 0x21

; ---- JOYTEST R ----
regs:
        mov si, 0x1234
        mov di, 0x5678
        mov bp, 0x9ABC
        mov bx, 0x1111
        mov cx, 0x2222
        mov ah, 0x84
        xor dx, dx
        int 0x15
        jc .mal
        cmp si, 0x1234
        jne .mal
        cmp di, 0x5678
        jne .mal
        cmp bp, 0x9ABC
        jne .mal
        cmp bx, 0x1111
        jne .mal
        cmp cx, 0x2222
        jne .mal
        test dx, dx
        jnz .mal
        mov ax, ds
        mov bx, cs
        cmp ax, bx
        jne .mal
        ; subfunción desconocida: CF
        mov ah, 0x84
        mov dx, 7
        int 0x15
        jnc .mal
        ; la interfaz propia: ¿está?
        mov ax, 0xD900
        int 0x2F
        cmp al, 0xFF
        jne .mal
        cmp bx, 0x484A
        jne .mal
        cmp word [es:di], 0x4A48    ; "HJ"
        jne .mal
        push cs
        pop es
        ; un mando que no existe: CF
        mov ax, 0xD902
        mov dl, 9
        int 0x2F
        jnc .mal
        mov si, m_regs
        call puts
        mov ax, 0x4C00
        int 0x21
.mal:
        push cs
        pop es
        mov si, m_regsf
        call puts
        mov ax, 0x4C01
        int 0x21

; lo que ve un juego por INT 15h
int15:
        mov si, m_i15
        call puts
        mov ah, 0x84
        xor dx, dx
        int 0x15
        jnc .ok
        mov si, m_cf
        call puts
        ret
.ok:
        mov si, m_i15b
        call puts
        call hex2
        mov ah, 0x84
        mov dx, 1
        int 0x15
        push dx
        push cx
        push bx
        mov si, m_x1
        call .n
        pop ax
        mov si, m_y1
        call .n
        pop ax
        mov si, m_x2
        call .n
        pop ax
        mov si, m_y2
        call .n
        call crlf
        ret
.n:
        call puts
        movzx eax, ax
        call putnum
        ret

; el mando clásico de la interfaz propia
clasico:
        mov ax, 0xD900
        int 0x2F
        push cs
        pop es
        cmp al, 0xFF
        je .ok
        ret                         ; no está HSPJOY
.ok:
        mov ax, 0xD901
        int 0x2F
        push cx
        push bx
        mov si, m_clas
        call puts
        call hex2
        pop bx
        mov si, m_x1
        call puts
        movzx eax, bl
        call putnum
        mov si, m_y1
        call puts
        movzx eax, bh
        call putnum
        pop bx
        mov si, m_x2
        call puts
        movzx eax, bl
        call putnum
        mov si, m_y2
        call puts
        movzx eax, bh
        call putnum
        call crlf
        ret

; la tabla de mandos
tabla:
        xor dl, dl
.l:
        push dx
        mov ax, 0xD902
        int 0x2F
        jc .n
        cmp al, 0xFF
        jne .n
        test byte [es:di+HJS_FLAGS], 1
        jz .n
        mov si, m_mando
        call puts
        pop dx
        push dx
        movzx eax, dl
        inc ax
        call putnum
        mov si, m_tipo
        call puts
        movzx eax, byte [es:di+HJS_TIPO]
        call putnum
        mov si, m_na
        call puts
        movzx eax, byte [es:di+HJS_NAX]
        call putnum
        mov al, '/'
        call putc
        movzx eax, byte [es:di+HJS_NBTN]
        call putnum
        mov si, m_ejes
        call puts
        xor bx, bx
.e:
        movzx eax, byte [es:di+HJS_EJES+bx]
        call putnum
        mov al, ' '
        call putc
        inc bx
        cmp bx, 6
        jb .e
        mov si, m_cruz
        call puts
        movzx eax, byte [es:di+HJS_CRUZ]
        call putnum
        mov si, m_btn
        call puts
        mov ax, [es:di+HJS_BTN]
        call puthex4
        call crlf
.n:
        pop dx
        inc dl
        cmp dl, 6
        jb .l
        push cs
        pop es
        ; las órdenes que ha recibido el mando de PlayStation simulado
        mov ax, 0xD97F
        xor di, di
        int 0x2F
        test di, di
        jz .r
        cmp byte [es:di+9], 8
        jne .r
        mov si, m_psx
        call puts
        mov al, [es:di+0x22]
        call hex2
        mov al, ' '
        call putc
        mov al, [es:di+0x23]
        call hex2
        call crlf
.r:
        push cs
        pop es
        ret

blancos:
        cmp byte [si], ' '
        jne .r
        inc si
        jmp blancos
.r:
        ret
; número hexadecimal en SI -> AX
hex:
        call blancos
        xor ax, ax
.l:
        mov bl, [si]
        cmp bl, '0'
        jb .r
        cmp bl, '9'
        jbe .d
        and bl, 0xDF
        cmp bl, 'A'
        jb .r
        cmp bl, 'F'
        ja .r
        sub bl, 7
.d:
        sub bl, '0'
        shl ax, 4
        or al, bl
        inc si
        jmp .l
.r:
        ret
; AL en hexadecimal
hex2:
        push ax
        push cx
        mov cx, 2
.l:
        rol al, 4
        push ax
        and al, 15
        add al, '0'
        cmp al, '9'
        jbe .d
        add al, 7
.d:
        call putc
        pop ax
        loop .l
        pop cx
        pop ax
        ret

%define HJ_SOLO_DEFS
%include "hidjoyx.inc"
%include "util.inc"

m_i15       db "INT15: ", 0
m_i15b      db "botones=", 0
m_cf        db "sin mando (CF)", 13, 10, 0
m_clas      db "Clasico: botones=", 0
m_x1        db " X1=", 0
m_y1        db " Y1=", 0
m_x2        db " X2=", 0
m_y2        db " Y2=", 0
m_mando     db "Mando ", 0
m_tipo      db ": tipo=", 0
m_na        db " ", 0
m_ejes      db " ejes=", 0
m_cruz      db "cruz=", 0
m_btn       db " botones=", 0
m_psx       db "PSX ordenes=", 0
m_nosim     db "HSPJOYT no esta cargado", 13, 10, 0
m_fin       db "Fin de la espera", 13, 10, 0
m_mirando   db "Mirando", 13, 10, 0
m_regs      db "Registros conservados", 13, 10, 0
m_regsf     db "REGISTROS CAMBIADOS", 13, 10, 0
tics        dw 0
t0          dw 0
b_viejo     db 0xEE
e_viejo     dw 0xEEEE, 0, 0, 0
b_nuevo     db 0
e_nuevo     dw 0, 0, 0, 0
u_numbuf    times 16 db 0
u_lbuf      times 84 db 0
