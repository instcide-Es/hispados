; =============================================================================
;  TECLADO.COM (KEYB) - el teclado español
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;       TECLADO         pone el teclado español (se queda en la memoria)
;       TECLADO /N      vuelve al normal (el de la BIOS, el americano)
;       TECLADO /9      lo pone por la interrupción del teclado (INT 9), como
;                       en un PC o un XT, aunque la BIOS sea de AT (para
;                       probar ese camino, o por si la BIOS no cumple)
;
;  La BIOS cree que el teclado es americano. TECLADO se queda residente
;  (unos 0,8 K) y ve cada tecla antes de que la BIOS haga nada con ella: si
;  es de las que cambian en el teclado español, mete él en la cola del
;  teclado la letra que toca y la BIOS ni se entera; las demás (las letras,
;  las flechas, las de función) pasan a la BIOS como siempre. En los AT y
;  posteriores se engancha a la «intercepción del teclado» de la BIOS (INT
;  15h/4Fh), y en los PC y XT, a la interrupción del teclado (INT 9): ver
;  más abajo por qué.
;
;  Lo que pone: ñ y Ñ, ç y Ç, º ª ¡ ¿, los signos en su sitio ( " · / ( ) = ?
;  ; : _ ' < > ), con AltGr (o Ctrl+Alt) | @ # ~ [ ] { } \ y ¬, y los acentos:
;  la tecla del acento no escribe nada hasta la vocal siguiente (´ ` ¨ ^, que
;  dan á à ä â...; con un espacio, el acento solo). La página 437 no tiene
;  las mayúsculas con tilde (salvo É): salen sin ella.
;
;  Es el teclado español de 102 teclas (el de los AT y posteriores). Con el
;  de 83 teclas de los PC y XT, que no tiene AltGr, sirve Ctrl+Alt.
;  Solo instrucciones del 8086.
; =============================================================================
        cpu 8086
        org 0x100

inicio:
        jmp instala

; ---- lo que se queda en la memoria ----
firma       db "HTEC"
activo      db 1
muerta      db 0                    ; el acento que espera su vocal (1 a 4)
anterior    dd 0                    ; la rutina a la que nos hemos enganchado
con_e0      db 0                    ; el código anterior fue E0h
altgr       db 0                    ; está pulsado el Alt de la derecha

; cada tecla: su código, y lo que da sola, con mayúsculas y con AltGr (0: lo
; que diga la BIOS; 1 a 4: un acento, que espera la vocal: ´ ` ¨ ^)
teclas:
        db 0x29, 0xA7, 0xA6, '\'    ; º ª
        db 0x02, '1', '!', '|'
        db 0x03, '2', '"', '@'
        db 0x04, '3', 0xFA, '#'     ; ·
        db 0x05, '4', '$', '~'
        db 0x06, '5', '%', 0
        db 0x07, '6', '&', 0xAA     ; ¬
        db 0x08, '7', '/', 0
        db 0x09, '8', '(', 0
        db 0x0A, '9', ')', 0
        db 0x0B, '0', '=', 0
        db 0x0C, "'", '?', 0
        db 0x0D, 0xAD, 0xA8, 0      ; ¡ ¿
        db 0x1A, 2, 4, '['
        db 0x1B, '+', '*', ']'
        db 0x27, 0xA4, 0xA5, 0      ; ñ Ñ
        db 0x28, 1, 3, '{'
        db 0x2B, 0x87, 0x80, '}'    ; ç Ç
        db 0x56, '<', '>', 0
        db 0x33, ',', ';', 0
        db 0x34, '.', ':', 0
        db 0x35, '-', '_', 0
        db 0
; las vocales (por el código de su tecla) y lo que dan con cada acento, en
; minúscula y en mayúscula; lo último, el acento solo (con un espacio)
vocales     db 0x1E, 0x12, 0x17, 0x18, 0x16, 0x39
acentos:
        db 0xA0, 0x82, 0xA1, 0xA2, 0xA3, "'", "A", 0x90, "IOU", "'"    ; ´
        db 0x85, 0x8A, 0x8D, 0x95, 0x97, "`", "AEIOU", "`"             ; `
        db 0x84, 0x89, 0x8B, 0x94, 0x81, '"', 0x8E, "EI", 0x99, 0x9A, '"'  ; ¨
        db 0x83, 0x88, 0x8C, 0x93, 0x96, "^", "AEIOU", "^"             ; ^

; ---- por dónde llegan las teclas ----
; En un AT (y en todo lo posterior), la BIOS lee la tecla del puerto 60h UNA
; vez y, antes de hacer nada con ella, llama a INT 15h con AH=4Fh y el código
; en AL: «¿la quieres tú?». Ahí nos enganchamos: si es de las nuestras,
; metemos la letra en la cola y contestamos que la tecla no cuenta (CF=0); si
; no, se deja pasar. Así el puerto se lee una sola vez, que es lo único
; fiable: en un 8042, leerlo otra vez puede dar ya la tecla siguiente (pasa
; en algunos emuladores, y con la cola llena), y la BIOS se descuadra.
int15:
        cmp ah, 0x4F
        jne .otro
        cmp byte [cs:activo], 0
        je .otro
        push ax
        push bx
        push cx
        push si
        push ds
        push cs
        pop ds
        call traduce
        pop ds
        pop si
        pop cx
        pop bx
        pop ax
        jnc .otro
        push bp                     ; nuestra: «no hagas nada con ella» (CF=0
        mov bp, sp                  ; en las banderas de quien llama)
        and byte [bp+6], 0xFE
        pop bp
        iret
.otro:
        jmp far [cs:anterior]
; En un PC o un XT, la BIOS no pregunta: hay que ponerse delante de ella en
; la interrupción del teclado (INT 9) y leer el puerto. Allí sí se puede leer
; dos veces (nosotros y, si la tecla no es nuestra, la BIOS): el puerto 60h
; de un XT es un registro que guarda el código hasta que se le da el pulso
; por el puerto 61h, cosa que solo hace quien se queda la tecla
int9:
        push ax
        push bx
        push cx
        push si
        push ds
        push cs
        pop ds
        cmp byte [activo], 0
        je .pasa
        in al, 0x60
        call traduce
        jnc .pasa
        in al, 0x61                 ; la tecla, atendida
        mov ah, al
        or al, 0x80
        out 0x61, al
        mov al, ah
        out 0x61, al
        mov al, 0x20
        out 0x20, al
        pop ds
        pop si
        pop cx
        pop bx
        pop ax
        iret
.pasa:
        pop ds
        pop si
        pop cx
        pop bx
        pop ax
        jmp far [cs:anterior]

; la tecla de código AL: si es de las que cambian, mete su letra en la cola
; del teclado (o apunta el acento) y vuelve con CF=1: la BIOS no tiene que
; verla. Si no, CF=0. DS = el nuestro. No conserva AX, BX, CX ni SI
traduce:
        ; las teclas «extendidas» llegan en dos veces: E0h y su código. No
        ; se cambian, pero hay que saber si el Alt que está pulsado es el de
        ; la derecha (AltGr), y de eso no todas las BIOS llevan la cuenta
        cmp al, 0xE0
        jne .n1
        mov byte [con_e0], 1
        jmp .no
.n1:
        xor bl, bl
        xchg bl, [con_e0]
        test bl, bl
        jz .n2
        cmp al, 0x38
        jne .n1b
        mov byte [altgr], 1
.n1b:
        cmp al, 0xB8
        jne .n1c
        mov byte [altgr], 0
.n1c:
        jmp .no
.n2:
        test al, 0x80               ; soltar una tecla: cosa de la BIOS
        jnz .n1c
        mov cl, al                  ; CL = la tecla
        push ds
        mov bx, 0x40
        mov ds, bx
        mov ah, [0x17]              ; las mayúsculas, Ctrl y Alt
        pop ds
        ; ¿sola (0), con mayúsculas (1) o con AltGr (2)?
        xor ch, ch
        cmp byte [altgr], 0
        jne .es_altgr
        mov al, ah
        and al, 0x0C
        jz .may
        cmp al, 0x0C                ; Ctrl+Alt también vale
        jne .n1c                    ; (Ctrl o Alt a secas: la BIOS)
.es_altgr:
        mov ch, 2
        jmp short .acento
.may:
        test ah, 0x03
        jz .acento
        mov ch, 1
.acento:
        ; si hay un acento esperando, esta tecla lo resuelve
        cmp byte [muerta], 0
        je .tabla
        mov si, vocales
        xor bx, bx
.v:
        cmp cl, [si+bx]
        je .vocal
        inc bx
        cmp bx, 6
        jb .v
        mov byte [muerta], 0        ; (otra cosa: el acento se pierde)
        jmp short .tabla
.vocal:
        mov al, [muerta]            ; la fila del acento: 12 letras cada una
        mov byte [muerta], 0
        dec al
        mov ch, 12
        xchg ch, ah                 ; (CH = las mayúsculas, a salvo)
        xchg al, ah
        mov al, 12
        mul ah
        mov si, ax
        add si, bx
        mov al, ch                  ; mayúscula: con la tecla o con el
        and al, 0x03                ; bloqueo (uno u otro, no los dos)
        jz .v0
        mov al, 1
.v0:
        test ch, 0x40
        jz .v1
        xor al, 1
.v1:
        test al, al
        jz .v2
        add si, 6
.v2:
        mov al, [acentos+si]
        jmp short .mete
.tabla:
        mov si, teclas
.t:
        cmp byte [si], 0
        je .no
        cmp cl, [si]
        je .esta
        add si, 4
        jmp .t
.esta:
        mov bl, ch
        xor bh, bh
        cmp cl, 0x27                ; la ñ y la ç son letras: el bloqueo de
        je .letra                   ; mayúsculas también les vale
        cmp cl, 0x2B
        jne .e1
.letra:
        cmp bl, 2
        je .e1
        test ah, 0x40
        jz .e1
        xor bl, 1
.e1:
        mov al, [si+bx+1]
        test al, al
        jz .no
        cmp al, 4
        ja .mete
        mov [muerta], al            ; un acento: espera a la vocal
        jmp .si
.mete:
        ; AL, a la cola del teclado de la BIOS (si cabe)
        push ds
        mov bx, 0x40
        mov ds, bx
        mov bx, [0x1C]
        mov si, bx
        add si, 2
        cmp si, 0x3E
        jb .m1
        mov si, 0x1E
.m1:
        cmp si, [0x1A]
        je .m2                      ; (llena)
        mov ah, cl
        mov [bx], ax
        mov [0x1C], si
.m2:
        pop ds
.si:
        stc
        ret
.no:
        clc
        ret
residente_fin:

; ---- lo que solo hace falta al instalar ----
instala:
        cld
        mov si, 0x81                ; ¿/N?
.a:
        lodsb
        cmp al, ' '
        je .a
        mov byte [quitar], 0
        cmp al, '/'
        jne .ya
        cmp byte [si], '9'          ; /9: por INT 9 aunque la BIOS pregunte
        jne .q
        mov byte [por9], 1
        jmp short .ya
.q:
        mov byte [quitar], 1
.ya:
        mov ax, 0x3515              ; ¿está ya en la memoria? (enganchado a
        int 0x21                    ; INT 15h o a INT 9, según la máquina)
        call .es_el
        je .esta
        mov ax, 0x3509
        int 0x21
        call .es_el
        jne .no_esta
.esta:
        mov al, [quitar]
        xor al, 1
        mov [es:activo], al
        mov byte [es:muerta], 0
        mov dx, m_espanol
        test al, al
        jnz .dice
        mov dx, m_normal
.dice:
        mov ah, 9
        int 0x21
        mov ax, 0x4C00
        int 0x21
; (interna) ¿es nuestra la rutina de ES:BX? ZF=1 si lo es
.es_el:
        cmp word [es:firma], 'HT'
        jne .e
        cmp word [es:firma+2], 'EC'
.e:
        ret
.no_esta:
        cmp byte [quitar], 0
        je .pon
        mov dx, m_no_esta
        jmp .dice
.pon:
        ; ¿pregunta la BIOS por cada tecla (INT 15h/4Fh)? Lo dice ella: INT
        ; 15h/C0h da su tabla de características, y el bit 4 del octeto 5 es
        ; ese. Las de los PC y XT no saben de C0h (vuelven con acarreo)
        cmp byte [por9], 0
        jne .por_int9
        mov ah, 0xC0
        stc
        int 0x15
        jc .por_int9
        test ah, ah
        jnz .por_int9
        test byte [es:bx+5], 0x10
        jz .por_int9
        mov ax, 0x3515
        int 0x21
        mov [anterior], bx
        mov [anterior+2], es
        mov dx, int15
        mov ax, 0x2515
        int 0x21
        jmp short .puesto
.por_int9:
        mov ax, 0x3509
        int 0x21
        mov [anterior], bx
        mov [anterior+2], es
        mov dx, int9
        mov ax, 0x2509
        int 0x21
.puesto:
        mov es, [0x2C]              ; el entorno no hace falta
        mov ah, 0x49
        int 0x21
        mov dx, m_espanol
        mov ah, 9
        int 0x21
        mov dx, residente_fin + 15
        mov cl, 4
        shr dx, cl
        mov ax, 0x3100
        int 0x21

quitar      db 0
por9        db 0
m_espanol   db "Teclado español.", 13, 10, "$"
m_normal    db "Teclado normal (el de la BIOS).", 13, 10, "$"
m_no_esta   db "TECLADO no está en la memoria.", 13, 10, "$"
