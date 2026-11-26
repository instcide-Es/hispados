; =============================================================================
;  PAQUETE.COM - prueba de un «packet driver» de red (para la batería)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Hecho con la especificación pública de FTP Software («PC/TCP Packet Driver
;  Specification», revisión 1.09). Busca el controlador en las interrupciones
;  60h a 80h (la firma «PKT DRVR» detrás de los tres primeros octetos de su
;  rutina), le pregunta quién es (función 1), se apunta a recibir todos los
;  paquetes (2), lee la dirección de la tarjeta (6), manda una pregunta ARP
;  por 10.0.2.2 (4: es la pasarela de la red «user» de QEMU, que contesta) y
;  espera la respuesta un par de segundos. Al final se borra (3) y lo dice
;  todo en la pantalla:
;       Paquetes: INT 60h, clase 1, versión 11, «NE2000»
;       Paquetes: tarjeta 52:54:00:12:34:56
;       Paquetes: enviado; recibidos 1; respuesta ARP de 10.0.2.2: 52:55:0A:00:02:02
;  Sale con 0 si ha ido todo bien, 1 si no hay controlador y 2 si no llega
;  la respuesta.
; =============================================================================
        cpu 8086
        org 0x100

inicio:
        mov si, 0x60 * 4            ; busca el controlador
        xor ax, ax
        mov es, ax
        mov byte [vec], 0x60
.b:
        push es
        les di, [es:si]
        add di, 3
        push si
        mov si, firma
        mov cx, 8
        repe cmpsb
        pop si
        pop es
        je .hay
        add si, 4
        inc byte [vec]
        cmp byte [vec], 0x80
        jbe .b
        mov dx, m_no
        mov ah, 9
        int 0x21
        mov ax, 0x4C01
        int 0x21
.hay:
        mov al, [vec]               ; la llamada: INT n, que se escribe aquí
        mov [llama+1], al
        mov dx, m_ini
        call cad
        mov al, [vec]
        call hex2
        mov dx, m_h
        call cad
        ; 1: quién es
        mov ax, 0x01FF
        push ds
        call llama
        mov [cs:clase], ch
        mov [cs:vers], bx
        mov di, nombre              ; su nombre (DS:SI), a lo nuestro
        mov cx, 15
.n:
        lodsb
        mov [cs:di], al
        inc di
        test al, al
        loopnz .n
        pop ds
        mov dx, m_clase
        call cad
        mov al, [clase]
        call dec8
        mov dx, m_vers
        call cad
        mov ax, [vers]
        call dec8
        mov dx, m_nom
        call cad
        mov dx, nombre
        call cad0
        mov dx, m_nom2
        call cad
        ; 2: todos los paquetes, a `recibe`
        mov ah, 2
        mov al, [clase]
        mov bx, 0xFFFF
        xor dl, dl
        xor cx, cx                  ; (tipo de 0 octetos: todos)
        mov si, tipo
        push cs
        pop es
        mov di, recibe
        call llama
        jc .mal
        mov [asa], ax
        ; 6: la dirección de la tarjeta
        mov ah, 6
        mov bx, [asa]
        mov di, arp + 6
        mov cx, 6
        call llama
        jc .mal
        mov si, arp + 6             ; (y va también en la pregunta ARP)
        mov di, arp + 22
        mov cx, 6
        rep movsb
        mov dx, m_mac
        call cad
        mov si, arp + 6
        call mac
        mov dx, m_nl
        call cad
        ; 4: la pregunta ARP
        mov ah, 4
        mov si, arp
        mov cx, 60
        call llama
        jc .mal
        mov dx, m_env
        call cad
        ; y a esperar (unos 2 s: 36 tics del reloj)
        xor ax, ax
        mov es, ax
        mov bx, [es:0x46C]
.e:
        cmp byte [vista], 0
        jne .vista
        mov ax, [es:0x46C]
        sub ax, bx
        cmp ax, 36
        jb .e
.vista:
        mov dx, m_rec
        call cad
        mov ax, [n_rec]
        call dec8
        mov al, 2
        cmp byte [vista], 0
        je .suelta
        mov dx, m_resp
        call cad
        mov si, quien
        call mac
        mov al, 0
.suelta:
        push ax
        mov dx, m_nl
        call cad
        mov ah, 3                   ; 3: borrarse
        mov bx, [asa]
        call llama
        pop ax
        mov ah, 0x4C
        int 0x21
.mal:
        mov dx, m_mal
        call cad
        mov al, dh
        call hex2
        mov dx, m_nl
        call cad
        mov ax, 0x4C03
        int 0x21

; la llamada al controlador (el número se pone al principio)
llama:
        int 0x60
        ret

; lo llama el controlador al llegar un paquete: AX=0, ¿dónde lo dejo? (CX
; octetos; ES:DI = 0:0 si no se quiere); AX=1, ya está copiado en DS:SI
recibe:
        test ax, ax
        jnz .copiado
        push cs
        pop es
        mov di, buf
        cmp cx, 1514
        jbe .x
        xor di, di
        mov es, di
.x:
        retf
.copiado:
        push ax
        inc word [cs:n_rec]
        cmp word [cs:buf+12], 0x0608        ; ARP (08 06)
        jne .no
        cmp word [cs:buf+20], 0x0200        ; respuesta (00 02)
        jne .no
        cmp word [cs:buf+28], 0x000A        ; de 10.0.2.2
        jne .no
        cmp word [cs:buf+30], 0x0202
        jne .no
        push si
        push di
        push cx
        push ds
        push es
        push cs
        pop ds
        push cs
        pop es
        mov si, buf + 22
        mov di, quien
        mov cx, 6
        rep movsb
        mov byte [vista], 1
        pop es
        pop ds
        pop cx
        pop di
        pop si
.no:
        pop ax
        retf

; ---- la pantalla ----
cad:                                ; el texto de DX, acabado en «$»
        mov ah, 9
        int 0x21
        ret
cad0:                               ; el de DX, acabado en 0
        mov si, dx
.c:
        lodsb
        test al, al
        jz .x
        mov dl, al
        mov ah, 2
        int 0x21
        jmp .c
.x:
        ret
dec8:                               ; AL en decimal
        xor ah, ah
        mov bl, 10
        div bl
        push ax
        test al, al
        jz .u
        call dec8
.u:
        pop ax
        mov dl, ah
        add dl, '0'
        mov ah, 2
        int 0x21
        ret
hex2:                               ; AL en hexadecimal
        push ax
        mov cl, 4
        shr al, cl
        call .c
        pop ax
.c:
        and al, 15
        add al, '0'
        cmp al, '9'
        jbe .p
        add al, 7
.p:
        mov dl, al
        mov ah, 2
        int 0x21
        ret
mac:                                ; los 6 octetos de SI, con «:»
        mov cx, 6
.m:
        lodsb
        push cx
        call hex2
        pop cx
        cmp cx, 1
        je .x
        mov dl, ':'
        mov ah, 2
        int 0x21
.x:
        loop .m
        ret

firma   db "PKT DRVR"
m_no    db "Paquetes: no hay controlador$"
m_ini   db "Paquetes: INT $"
m_h     db "h$"
m_clase db ", clase $"
m_vers  db ", version $"
m_nom   db ", ", 34, "$"
m_nom2  db 34, 13, 10, "$"
m_mac   db "Paquetes: tarjeta $"
m_env   db "Paquetes: enviado$"
m_rec   db "; recibidos $"
m_resp  db "; respuesta ARP de 10.0.2.2: $"
m_mal   db 13, 10, "Paquetes: MAL, error $"
m_nl    db 13, 10, "$"
tipo    db 0
; la pregunta ARP: a todos, de nosotros; Ethernet, IP, 6 y 4, «pregunta»,
; nuestra dirección, 10.0.2.15, 0, 10.0.2.2
arp     db 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0, 0, 0, 0, 0, 0, 0x08, 0x06
        db 0, 1, 0x08, 0, 6, 4, 0, 1
        db 0, 0, 0, 0, 0, 0, 10, 0, 2, 15
        db 0, 0, 0, 0, 0, 0, 10, 0, 2, 2
        times 60 - ($ - arp) db 0
vec     db 0
clase   db 0
vista   db 0
vers    dw 0
asa     dw 0
n_rec   dw 0
quien   times 6 db 0
nombre  times 16 db 0
buf:
