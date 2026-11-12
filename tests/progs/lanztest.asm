; LANZTEST.COM - programa de prueba de los accesos directos de Hispa Ventanas
; (tests/run_ventanas4_tests.sh). Hace de «juego»: apunta en el archivo
; LANZ.TXT de la carpeta en la que arranca, y también en la pantalla,
;   ARGS=[...]   su cola de órdenes
;   DIR=X:\...   la unidad y la carpeta de trabajo
;   KB=nnn       la memoria convencional que le han dado (su bloque, en KB)
;   DPMI=SI|NO   si hay un servidor DPMI en memoria (INT 2Fh AX=1687h)
;   XMS=SI|NO    si hay gestor XMS
;   MIX=xx       el registro 30h (volumen general) del mezclador de una
;                Sound Blaster 16 en 220h, en hexadecimal
; y luego espera una tecla (salvo con el parámetro /N) y termina.
        cpu 386
        bits 16
        org 0x100

        cld
        mov di, buf
        mov si, s_args
        call pon
        movzx cx, byte [0x80]
        mov si, 0x81
        rep movsb
        mov al, ']'
        stosb
        call nl
        mov si, s_dir
        call pon
        mov ah, 0x19
        int 0x21
        add al, 'A'
        stosb
        mov ax, ':\'
        stosw
        mov si, di
        xor dl, dl
        mov ah, 0x47
        int 0x21
.d:
        cmp byte [di], 0
        je .df
        inc di
        jmp .d
.df:
        call nl
        mov si, s_kb
        call pon
        mov ax, cs                  ; el bloque de memoria del programa
        dec ax
        mov es, ax
        mov ax, [es:3]
        push cs
        pop es
        shr ax, 6                   ; párrafos -> KB
        call num
        call nl
        mov si, s_dpmi
        call pon
        mov ax, 0x1687
        int 0x2F
        push cs
        pop es
        test ax, ax
        call sino
        mov si, s_xms
        call pon
        mov ax, 0x4300
        int 0x2F
        cmp al, 0x80
        call sino
        mov si, s_mix
        call pon
        mov dx, 0x224
        mov al, 0x30
        out dx, al
        inc dx
        in al, dx
        call hex
        call nl
        ; a la pantalla y al archivo
        mov cx, di
        sub cx, buf
        push cx
        mov dx, buf
        mov bx, 1
        mov ah, 0x40
        int 0x21
        mov dx, nombre
        xor cx, cx
        mov ah, 0x3C
        int 0x21
        pop cx
        jc .sin
        mov bx, ax
        mov dx, buf
        mov ah, 0x40
        int 0x21
        mov ah, 0x3E
        int 0x21
.sin:
        mov si, 0x81                ; ¿/N? No espera
.n:
        lodsb
        cmp al, 13
        je .espera
        cmp al, '/'
        jne .n
        lodsb
        and al, 0xDF
        cmp al, 'N'
        je .fin
        jmp .n
.espera:
        mov dx, m_tecla
        mov ah, 0x09
        int 0x21
        xor ah, ah
        int 0x16
.fin:
        mov dx, m_fin
        mov ah, 0x09
        int 0x21
        mov ax, 0x4C00
        int 0x21

pon:
        lodsb
        test al, al
        jz .x
        stosb
        jmp pon
.x:
        ret
nl:
        mov ax, 0x0A0D
        stosw
        ret
sino:
        mov si, s_si
        je .p
        mov si, s_no
.p:
        call pon
        jmp nl
num:
        xor cx, cx
        mov bx, 10
.d:
        xor dx, dx
        div bx
        push dx
        inc cx
        test ax, ax
        jnz .d
.e:
        pop ax
        add al, '0'
        stosb
        loop .e
        ret
hex:
        push ax
        shr al, 4
        call .n
        pop ax
        and al, 0x0F
.n:
        add al, '0'
        cmp al, '9'
        jbe .s
        add al, 7
.s:
        stosb
        ret

s_args  db "ARGS=[", 0
s_dir   db "DIR=", 0
s_kb    db "KB=", 0
s_dpmi  db "DPMI=", 0
s_xms   db "XMS=", 0
s_mix   db "MIX=", 0
s_si    db "SI", 0
s_no    db "NO", 0
nombre  db "LANZ.TXT", 0
m_tecla db "LANZTEST: en marcha; pulse una tecla.", 13, 10, "$"
m_fin   db "LANZTEST: fin.", 13, 10, "$"
buf:
