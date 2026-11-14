; =============================================================================
;  Hispa-DOS - pila TCP/IP propia, mínima (para CONFRED, PING y DESCARGAR)
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Habla con cualquier «packet driver» (HSPRED u otro: especificación de FTP
;  Software 1.09, INT 60h-80h). Con HSPRED, la configuración IP se guarda en
;  el residente (INT 2Fh AX=DA00h) y la usan todos los programas; con otro
;  packet driver, cada programa la consigue al empezar.
;
;  Lo que hay: Ethernet II; ARP (pregunta, contesta y guarda RED_NARP
;  direcciones); IPv4 sin fragmentos; ICMP (contesta al eco y hace eco: PING);
;  UDP; DHCP (cliente: DISCOVER, OFFER, REQUEST, ACK); DNS (cliente, registros
;  A, por UDP); TCP con una sola conexión de cliente (abrir, enviar con espera
;  del acuse y reenvío cada segundo, recibir en orden con acuses inmediatos,
;  cerrar con FIN; ventana fija de RED_VENT octetos, MSS 1460).
;
;  Rutinas (DS = ES = CS; CF = 1 si falla y, si hay mensaje, SI = el texto):
;    red_abrir      buscar el packet driver, pedir los tipos IP y ARP, la MAC
;    red_cerrar     soltar los tipos (¡siempre antes de salir!: red_salir)
;    red_salir      AL = código de salida
;    red_config     AL = 0: la configuración que haya (la de HSPRED o, si no,
;                   la de RED.INI: DHCP o a mano); AL = 1: pedirla otra vez
;    red_sondear    atender los paquetes que han llegado
;    red_esperar    BX = octeto que mirar, CX = tics: esperar a que no sea 0
;    dns_buscar     DS:SI = nombre o a.b.c.d -> EAX = IP
;    ip_texto       DS:SI = «a.b.c.d» -> EAX = IP (SI avanza)
;    ip_poner       EAX = IP -> pantalla
;    icmp_eco       EAX = destino, BX = secuencia
;    tcp_abrir      EAX = IP, BX = puerto
;    tcp_enviar     DS:SI = datos, CX = octetos (1460 como mucho)
;    tcp_cerrar
;  Las IP se guardan en el orden de la red (el primer octeto, el más bajo).
; =============================================================================

RQ_N        equ 8                   ; paquetes que pueden esperar
RQ_SZ       equ 1520                ; cada uno: longitud (2) y el paquete
RED_NARP    equ 4
RED_VENT    equ 5840                ; ventana de TCP (4 segmentos)

        section .text

; ---- cambiar el orden de los octetos ----
bswap_eax:
        xchg ah, al
        ror eax, 16
        xchg ah, al
        ret

; EAX = tics del reloj de la BIOS (18,2 por segundo)
tic:
        push es
        xor eax, eax
        mov es, ax
        mov eax, [es:0x46C]
        pop es
        ret

; CF = 1 si se ha pulsado Esc (las demás teclas se tiran)
esc_pulsado:
        push ax
        mov ah, 1
        int 0x16
        jz .no
        xor ah, ah
        int 0x16
        cmp al, 27
        jne .no
        mov byte [red_esc], 1
        pop ax
        stc
        ret
.no:
        pop ax
        clc
        ret

; ---- llamar al packet driver (el número de la INT se pone en red_abrir) ----
pkt_int:
        db 0xCD
pkt_num db 0x60
        ret

; =============================================================================
;  El receptor: lo llama el packet driver desde su interrupción (DS no es el
;  nuestro). AX = 0: CX = longitud -> ES:DI = sitio (0:0 si no hay);
;  AX = 1: ya está copiado.
; =============================================================================
red_rcv:
        test ax, ax
        jnz .done
        cmp cx, RQ_SZ - 2
        ja .no
        cmp byte [cs:rq_cnt], RQ_N
        jae .no
        push ax
        movzx ax, byte [cs:rq_tail]
        imul di, ax, RQ_SZ
        add di, rq_buf
        mov [cs:di], cx
        add di, 2
        push cs
        pop es
        pop ax
        retf
.no:
        inc word [cs:rq_lost]
        xor di, di
        mov es, di
        retf
.done:
        inc byte [cs:rq_tail]
        cmp byte [cs:rq_tail], RQ_N
        jb .t
        mov byte [cs:rq_tail], 0
.t:
        inc byte [cs:rq_cnt]
        retf

; =============================================================================
;  Abrir y cerrar
; =============================================================================
red_abrir:
        cld
        ; ¿hay un packet driver? («PKT DRVR» 3 octetos tras el vector)
        mov al, 0x60
.v:
        push ax
        mov ah, 0x35
        int 0x21
        lea di, [bx+3]
        mov si, s_pktdrvr
        mov cx, 8
        repe cmpsb
        pop ax
        je .got
        inc al
        cmp al, 0x80
        jbe .v
        push cs
        pop es
        mov si, m_nopkt
        stc
        ret
.got:
        push cs
        pop es
        mov [pkt_num], al
        mov [r_vec], al
        ; nombre y clase
        push ds
        mov ax, 0x01FF
        xor bx, bx
        call pkt_int
        mov ax, ds
        pop ds
        jc .info_e
        cmp ch, 1                   ; Ethernet
        jne .noeth
        push ds
        mov ds, ax
        mov di, r_nombre
        mov cx, 39
.nc:
        lodsb
        test al, al
        jz .nce
        stosb
        loop .nc
.nce:
        pop ds
        mov byte [di], 0
.info_e:
        ; tipos IP (0800h) y ARP (0806h)
        mov si, s_tipo_ip
        call pkt_tipo
        jc .acc_e
        mov [h_ip], ax
        mov si, s_tipo_arp
        call pkt_tipo
        jc .acc_e
        mov [h_arp], ax
        ; la MAC
        mov ah, 6
        mov bx, [h_ip]
        mov di, my_mac
        mov cx, 6
        call pkt_int
        ; Ctrl+C: soltar los tipos antes de salir
        mov dx, red_ctrlc
        mov ax, 0x2523
        int 0x21
        ; ¿HSPRED? (la configuración compartida)
        mov ax, 0xDA00
        xor bx, bx
        push es
        int 0x2F
        cmp al, 0xFF
        jne .nh
        cmp bx, 0x5248
        jne .nh
        mov [hsp_cfg], di
        mov [hsp_cfg+2], es
.nh:
        pop es
        clc
        ret
.noeth:
        mov si, m_noeth
        stc
        ret
.acc_e:
        call red_cerrar
        mov si, m_acc
        stc
        ret

; SI = tipo (2 octetos) -> AX = manejador
pkt_tipo:
        mov ax, 0x0201              ; access_type, clase 1
        mov bx, 0xFFFF
        xor dl, dl
        mov cx, 2
        mov di, red_rcv
        call pkt_int
        ret

red_cerrar:
        push ax
        push bx
        push dx
        mov bx, [h_ip]
        test bx, bx
        jz .a
        mov ah, 3
        call pkt_int
        mov word [h_ip], 0
.a:
        mov bx, [h_arp]
        test bx, bx
        jz .r
        mov ah, 3
        call pkt_int
        mov word [h_arp], 0
.r:
        pop dx
        pop bx
        pop ax
        ret

red_salir:
        call red_cerrar
        mov ah, 0x4C
        int 0x21

red_ctrlc:
        push cs
        pop ds
        call red_cerrar
        mov ax, 0x4C03
        int 0x21

; =============================================================================
;  Atender lo que ha llegado
; =============================================================================
red_sondear:
        cmp byte [en_sondeo], 0
        jne .r
        pushad
        inc byte [en_sondeo]
.l:
        cmp byte [rq_cnt], 0
        je .fin
        movzx ax, byte [rq_head]
        imul si, ax, RQ_SZ
        add si, rq_buf
        mov cx, [si]
        add si, 2
        call eth_in
        inc byte [rq_head]
        cmp byte [rq_head], RQ_N
        jb .h
        mov byte [rq_head], 0
.h:
        dec byte [rq_cnt]
        jmp .l
.fin:
        dec byte [en_sondeo]
        popad
.r:
        ret

; BX = octeto que mirar, CX = tics como mucho -> CF = 1 si no llegó (o Esc)
red_esperar:
        push eax
        push edx
        call tic
        mov edx, eax
.l:
        cmp byte [bx], 0
        jne .ok
        call red_sondear
        call esc_pulsado
        jc .no
        cmp byte [bx], 0            ; (atender lo que llegó puede tardar más
        jne .ok                     ; que el plazo: lo que cuenta es que llegó)
        call tic
        sub eax, edx
        cmp eax, 0x10000
        jae .no                     ; (medianoche)
        cmp ax, cx
        jb .l
.no:
        pop edx
        pop eax
        stc
        ret
.ok:
        pop edx
        pop eax
        clc
        ret

; SI = trama, CX = longitud
eth_in:
        cmp cx, 14
        jb .r
        mov ax, [si+12]
        cmp ax, 0x0608
        je arp_in
        cmp ax, 0x0008
        je ip_in
.r:
        ret

; búfer para lo que se envía: el principal o, mientras se atiende lo que
; llega (respuestas, acuses), otro, para no estropear lo que se está montando
; -> DI
tx_base:
        mov di, tx
        cmp byte [en_sondeo], 0
        je .r
        mov di, txr
.r:
        ret

; DI = trama, CX = longitud -> CF
pkt_enviar:
        push si
        push ax
        mov si, di
        mov ah, 4
        call pkt_int
        pop ax
        pop si
        ret

; =============================================================================
;  ARP
; =============================================================================
arp_in:
        cmp cx, 42
        jb .r
        lea bx, [si+14]
        cmp dword [bx], 0x00080100  ; Ethernet, IPv4
        jne .r
        cmp word [bx+4], 0x0406
        jne .r
        mov eax, [bx+14]            ; quien pregunta o contesta
        test eax, eax
        jz .r
        mov edx, [my_ip]
        test edx, edx
        jz .r
        cmp word [bx+6], 0x0200     ; respuesta
        je .add
        cmp [bx+24], edx            ; ¿pregunta por nosotros?
        jne .r
        push bx
        lea si, [bx+8]
        call arp_poner
        pop bx
        ; contestar
        mov di, txa
        lea si, [bx+8]
        push si
        movsd
        movsw                       ; destino: el que pregunta
        call arp_cab
        mov word [di+6], 0x0200
        pop si
        push si
        lea di, [txa+32]
        movsd                       ; THA = SHA de la pregunta
        movsw
        movsd                       ; TPA = SPA de la pregunta
        pop si
        mov di, txa
        mov cx, 42
        call pkt_enviar
.r:
        ret
.add:
        lea si, [bx+8]
        jmp arp_poner

; DI = txa+6: la MAC de origen, el tipo y la cabecera ARP con nosotros como
; emisor (DI = txa+14 al volver)
arp_cab:
        mov si, my_mac
        movsd
        movsw
        mov word [di], 0x0608
        add di, 2
        mov dword [di], 0x00080100
        mov word [di+4], 0x0406
        mov word [di+6], 0x0100     ; pregunta (se cambia si es respuesta)
        push di
        add di, 8
        mov si, my_mac
        movsd
        movsw
        mov eax, [my_ip]
        stosd
        pop di
        ret

; EAX = IP, DS:SI = MAC -> a la tabla
arp_poner:
        push di
        call arp_buscar
        jnc .cp
        movzx di, byte [arp_sig]
        inc byte [arp_sig]
        and byte [arp_sig], RED_NARP - 1
        imul di, di, 10
        add di, arp_tab
        mov [di], eax
        add di, 4
.cp:
        push cx
        mov cx, 6
        rep movsb
        pop cx
        pop di
        ret

; EAX = IP -> DI = su MAC (CF si no está)
arp_buscar:
        push cx
        mov di, arp_tab
        mov cx, RED_NARP
.l:
        cmp [di], eax
        jne .n
        cmp dword [di], 0
        je .n
        add di, 4
        pop cx
        clc
        ret
.n:
        add di, 10
        loop .l
        pop cx
        stc
        ret

; EAX = IP del siguiente salto -> SI = MAC; CF si no contesta
arp_resolver:
        push di
        push cx
        push bx
        mov si, mac_todos
        cmp eax, 0xFFFFFFFF
        je .ok
        call arp_buscar
        jnc .okdi
        mov [arp_preg], eax
        mov bl, 4                   ; intentos
.otra:
        push bx
        ; pregunta
        mov di, txa
        mov si, mac_todos
        movsd
        movsw
        call arp_cab
        mov eax, [arp_preg]
        mov [txa+38], eax
        mov dword [txa+32], 0
        mov word [txa+36], 0
        mov di, txa
        mov cx, 42
        call pkt_enviar
        pop bx
        cmp byte [en_sondeo], 0     ; atendiendo un paquete: no se espera
        jne .no
        ; esperar la respuesta (medio segundo cada vez)
        call tic
        mov ecx, eax
.w:
        call red_sondear
        mov eax, [arp_preg]
        call arp_buscar
        jnc .okdi
        call esc_pulsado
        jc .no
        call tic
        sub eax, ecx
        cmp eax, 9
        jb .w
        dec bl
        jnz .otra
.no:
        pop bx
        pop cx
        pop di
        stc
        ret
.okdi:
        mov si, di
.ok:
        pop bx
        pop cx
        pop di
        clc
        ret

; =============================================================================
;  IP
; =============================================================================
ip_in:
        lea bx, [si+14]
        mov al, [bx]
        mov ah, al
        and al, 0xF0
        cmp al, 0x40
        jne .r
        and ah, 0x0F
        shl ah, 2                   ; longitud de la cabecera
        cmp ah, 20
        jb .r
        movzx dx, ah
        mov ax, [bx+2]
        xchg ah, al                 ; longitud total
        sub cx, 14
        cmp ax, cx
        ja .r
        cmp ax, dx
        jb .r
        ; sin fragmentos
        test byte [bx+6], 0x3F
        jnz .r
        cmp byte [bx+7], 0
        jne .r
        ; ¿para nosotros?
        mov ecx, [bx+16]
        mov edi, [my_ip]
        test edi, edi
        jz .si
        cmp ecx, edi
        je .si
        cmp ecx, 0xFFFFFFFF
        je .si
        mov edi, [my_mask]
        not edi
        or edi, [my_ip]
        cmp ecx, edi
        jne .r
.si:
        ; suma de comprobación de la cabecera
        push ax
        push dx
        mov si, bx
        mov cx, dx
        xor edx, edx
        call cks_sumar
        call cks_fin
        test ax, ax
        pop dx
        pop ax
        jnz .r
        mov ecx, [bx+12]
        mov [in_src], ecx
        mov cl, [bx+8]
        mov [in_ttl], cl
        sub ax, dx
        mov [in_len], ax
        add dx, bx
        mov [in_pay], dx
        mov al, [bx+9]
        cmp al, 1
        je icmp_in
        cmp al, 17
        je udp_in
        cmp al, 6
        je tcp_in
.r:
        ret

; EDX = suma; DS:SI = datos, CX = octetos -> EDX
cks_sumar:
        push eax
        push cx
        push si
        push cx
        shr cx, 1
        jz .imp
.l:
        lodsw
        movzx eax, ax
        add edx, eax
        loop .l
.imp:
        pop cx
        test cl, 1
        jz .r
        movzx eax, byte [si]
        add edx, eax
.r:
        pop si
        pop cx
        pop eax
        ret

; EDX = suma -> AX = complemento a uno, plegado a 16 bits
cks_fin:
        mov eax, edx
        shr eax, 16
        and edx, 0xFFFF
        add edx, eax
        mov eax, edx
        shr eax, 16
        add ax, dx
        not ax
        ret

; EAX = destino, BL = protocolo, CX = octetos de datos (ya en DI+34, DI =
; tx_base) -> CF
ip_enviar:
        pushad
        call tx_base
        mov [ip_dst], eax
        ; siguiente salto: el destino si está en nuestra red; si no, la puerta
        cmp eax, 0xFFFFFFFF
        je .res
        mov edx, eax
        xor edx, [my_ip]
        and edx, [my_mask]
        jz .res
        cmp dword [my_gw], 0
        je .res
        mov eax, [my_gw]
.res:
        call arp_resolver
        jc .e
        push cx
        movsd
        movsw                       ; MAC de destino
        mov si, my_mac
        movsd
        movsw
        mov ax, 0x0008
        stosw
        pop cx
        mov byte [di], 0x45
        mov byte [di+1], 0
        mov ax, cx
        add ax, 20
        xchg ah, al
        mov [di+2], ax
        inc word [ip_id]
        mov ax, [ip_id]
        xchg ah, al
        mov [di+4], ax
        mov word [di+6], 0x0040     ; no fragmentar
        mov byte [di+8], 64
        mov [di+9], bl
        mov word [di+10], 0
        mov eax, [my_ip]
        mov [di+12], eax
        mov eax, [ip_dst]
        mov [di+16], eax
        push cx
        mov si, di
        mov cx, 20
        xor edx, edx
        call cks_sumar
        call cks_fin
        mov [di+10], ax
        pop cx
        sub di, 14
        add cx, 34
        call pkt_enviar
        jc .e
        popad
        clc
        ret
.e:
        popad
        stc
        ret

; la suma de la seudocabecera de UDP y TCP: BL = protocolo, CX = longitud
; (EAX = destino) -> EDX
cks_seudo:
        push eax
        movzx edx, ax
        shr eax, 16
        add edx, eax
        mov eax, [my_ip]
        push eax
        shr eax, 16
        add edx, eax
        pop eax
        movzx eax, ax
        add edx, eax
        movzx eax, bl
        shl eax, 8
        add edx, eax
        mov ax, cx
        xchg ah, al
        movzx eax, ax
        add edx, eax
        pop eax
        ret

; =============================================================================
;  ICMP
; =============================================================================
icmp_in:
        mov si, [in_pay]
        mov cx, [in_len]
        cmp cx, 8
        jb .r
        mov al, [si]
        cmp al, 8
        je .eco
        test al, al
        jne .r
        ; respuesta a nuestro eco
        cmp word [si+4], 0x4853     ; identificador «HS»
        jne .r
        mov ax, [si+6]
        xchg ah, al
        cmp ax, [ping_seq]
        jne .r
        mov al, [in_ttl]
        mov [ping_ttl], al
        mov ax, [in_len]
        sub ax, 8
        mov [ping_oct], ax
        mov byte [ping_ok], 1
.r:
        ret
.eco:
        ; contestar con lo mismo
        cmp cx, 1472
        ja .r
        call tx_base
        add di, 34
        push cx
        rep movsb
        pop cx
        call tx_base
        mov byte [di+34], 0
        mov word [di+36], 0
        lea si, [di+34]
        xor edx, edx
        call cks_sumar
        call cks_fin
        mov [di+36], ax
        mov eax, [in_src]
        mov bl, 1
        call ip_enviar
        ret

; EAX = destino, BX = secuencia (32 octetos de datos)
icmp_eco:
        pushad
        mov [eco_dst], eax
        mov [ping_seq], bx
        call tx_base
        mov dword [di+34], 0x00000008
        mov word [di+38], 0x4853
        xchg bh, bl
        mov [di+40], bx
        push di
        add di, 42
        mov al, 'a'
        mov cx, 32
.d:
        stosb
        inc al
        cmp al, 'w'
        jbe .dn
        mov al, 'a'
.dn:
        loop .d
        pop di
        lea si, [di+34]
        mov cx, 40
        xor edx, edx
        call cks_sumar
        call cks_fin
        mov [di+36], ax
        mov eax, [eco_dst]
        mov bl, 1
        mov cx, 40
        call ip_enviar
        popad
        ret

; =============================================================================
;  UDP
; =============================================================================
udp_in:
        mov si, [in_pay]
        mov cx, [in_len]
        cmp cx, 8
        jb .r
        mov ax, [si+4]
        xchg ah, al
        cmp ax, 8
        jb .r
        cmp ax, cx
        ja .r
        sub ax, 8
        mov cx, ax
        mov ax, [si+2]
        xchg ah, al
        add si, 8
        cmp ax, 68
        je dhcp_in
        cmp ax, [dns_lport]
        je dns_in
.r:
        ret

; EAX = destino, BX = puerto de origen, DX = de destino, CX = octetos (en
; tx_base+42) -> CF
udp_enviar:
        push di
        push bx
        push cx
        call tx_base
        xchg bh, bl
        mov [di+34], bx
        xchg dh, dl
        mov [di+36], dx
        xchg dh, dl
        add cx, 8
        mov bx, cx
        xchg bh, bl
        mov [di+38], bx
        mov word [di+40], 0         ; sin suma (vale en IPv4)
        mov bl, 17
        call ip_enviar
        pop cx
        pop bx
        pop di
        ret

; =============================================================================
;  DHCP (cliente)
; =============================================================================
; SI = datos, CX = octetos
dhcp_in:
        cmp cx, 241
        jb .r
        cmp byte [si], 2            ; respuesta
        jne .r
        mov eax, [si+4]
        cmp eax, [dh_xid]
        jne .r
        mov eax, [si+28]
        cmp eax, [my_mac]
        jne .r
        cmp dword [si+236], 0x63538263
        jne .r
        mov eax, [si+16]
        mov [dh_yi], eax
        mov byte [dh_tipo], 0
        mov dword [dh_srv], 0
        mov dword [dh_mask], 0
        mov dword [dh_gw], 0
        mov dword [dh_dns], 0
        mov dword [dh_lease], 0
        mov bx, si
        add bx, cx                  ; fin
        add si, 240
.o:
        cmp si, bx
        jae .fin
        lodsb
        test al, al                 ; relleno
        jz .o
        cmp al, 255
        je .fin
        mov ah, al
        lodsb
        movzx cx, al
        mov di, si
        add si, cx
        cmp si, bx
        ja .fin
        cmp ah, 53
        jne .o1
        mov al, [di]
        mov [dh_tipo], al
.o1:
        cmp cx, 4
        jb .o
        mov edx, [di]
        cmp ah, 1
        jne .o2
        mov [dh_mask], edx
.o2:
        cmp ah, 3
        jne .o3
        mov [dh_gw], edx
.o3:
        cmp ah, 6
        jne .o4
        mov [dh_dns], edx
.o4:
        cmp ah, 54
        jne .o5
        mov [dh_srv], edx
.o5:
        cmp ah, 51
        jne .o
        mov eax, edx
        call bswap_eax
        mov [dh_lease], eax
        jmp .o
.fin:
        mov al, [dh_tipo]
        mov [dh_llego], al
.r:
        ret

; AL = tipo de mensaje (1 DISCOVER, 3 REQUEST) -> envía
dhcp_enviar:
        pushad
        call tx_base
        add di, 42
        push ax
        push di
        xor al, al
        mov cx, 300
        rep stosb
        pop di
        pop ax
        mov dword [di], 0x00060101
        mov edx, [dh_xid]
        mov [di+4], edx
        mov word [di+10], 0x0080    ; contestar por difusión
        mov si, my_mac
        push di
        add di, 28
        movsd
        movsw
        pop di
        mov dword [di+236], 0x63538263
        add di, 240
        mov byte [di], 53
        mov byte [di+1], 1
        mov [di+2], al
        add di, 3
        cmp al, 3
        jne .nr
        mov byte [di], 50           ; la dirección que nos ofrecieron
        mov byte [di+1], 4
        mov edx, [dh_ofrecida]
        mov [di+2], edx
        mov byte [di+6], 54         ; y de qué servidor
        mov byte [di+7], 4
        mov edx, [dh_ofsrv]
        mov [di+8], edx
        add di, 12
.nr:
        mov byte [di], 55           ; lo que se pide: máscara, puerta, DNS
        mov byte [di+1], 4          ; y concesión
        mov dword [di+2], 0x33060301
        mov byte [di+6], 0xFF       ; fin
        mov eax, 0xFFFFFFFF
        mov bx, 68
        mov dx, 67
        mov cx, 300
        call udp_enviar
        popad
        ret

; DHCP completo -> my_ip, my_mask, my_gw, my_dns, dh_srv, dh_lease; CF
dhcp_pedir:
        mov dword [my_ip], 0
        call tic
        xor eax, [my_mac+2]
        mov [dh_xid], eax
        mov byte [dh_intentos], 4
.desc:
        mov byte [dh_llego], 0
        mov al, 1
        call dhcp_enviar
.wo:
        mov bx, dh_llego
        mov cx, 36                  ; 2 s
        call red_esperar
        jc .otra
        cmp byte [dh_llego], 2      ; OFFER
        je .oferta
        mov byte [dh_llego], 0
        jmp .wo
.oferta:
        mov eax, [dh_yi]
        mov [dh_ofrecida], eax
        mov eax, [dh_srv]
        mov [dh_ofsrv], eax
        mov byte [dh_req], 3
.req:
        mov byte [dh_llego], 0
        mov al, 3
        call dhcp_enviar
.wa:
        mov bx, dh_llego
        mov cx, 36
        call red_esperar
        jc .reqotra
        mov al, [dh_llego]
        cmp al, 5                   ; ACK
        je .ack
        cmp al, 6                   ; NAK
        je .otra
        mov byte [dh_llego], 0
        jmp .wa
.reqotra:
        cmp byte [red_esc], 0
        jne .no
        dec byte [dh_req]
        jnz .req
.otra:
        cmp byte [red_esc], 0
        jne .no
        dec byte [dh_intentos]
        jnz .desc
.no:
        mov si, m_nodhcp
        stc
        ret
.ack:
        mov eax, [dh_yi]
        mov [my_ip], eax
        mov eax, [dh_mask]
        test eax, eax
        jnz .m
        mov eax, 0x00FFFFFF
.m:
        mov [my_mask], eax
        mov eax, [dh_gw]
        mov [my_gw], eax
        mov eax, [dh_dns]
        mov [my_dns], eax
        clc
        ret

; =============================================================================
;  Configuración: la de HSPRED, o RED.INI (DHCP o a mano)
; =============================================================================
; AL = 1: pedirla otra vez (DHCP) aunque HSPRED ya la tenga
red_config:
        mov [cf_renovar], al
        test al, al
        jnz .nueva
        cmp word [hsp_cfg+2], 0
        je .nueva
        push ds
        lds si, [hsp_cfg]
        test byte [si], 1
        jz .nv
        mov di, my_cfg
        mov cx, 30
        rep movsb
        pop ds
        call cfg_a_mi
        clc
        ret
.nv:
        pop ds
.nueva:
        call ini_leer
        cmp byte [ini_dhcp], 0
        je .mano
        call dhcp_pedir
        jc .r
        mov byte [my_cfg], 3
        mov eax, [dh_srv]
        mov [my_cfg+18], eax
        mov eax, [dh_lease]
        mov [my_cfg+22], eax
        jmp .dns
.mano:
        mov si, m_noip
        mov eax, [ini_ip]
        test eax, eax
        jz .e
        mov [my_ip], eax
        mov eax, [ini_mask]
        test eax, eax
        jnz .mm
        mov eax, 0x00FFFFFF
.mm:
        mov [my_mask], eax
        mov eax, [ini_gw]
        mov [my_gw], eax
        mov eax, [ini_dns]
        mov [my_dns], eax
        mov byte [my_cfg], 1
        mov dword [my_cfg+18], 0
        mov dword [my_cfg+22], 0
.dns:
        mov eax, [ini_dns]          ; DNS= en RED.INI manda
        test eax, eax
        jz .nd
        mov [my_dns], eax
.nd:
        mov eax, [my_ip]
        mov [my_cfg+2], eax
        mov eax, [my_mask]
        mov [my_cfg+6], eax
        mov eax, [my_gw]
        mov [my_cfg+10], eax
        mov eax, [my_dns]
        mov [my_cfg+14], eax
        mov ah, 0x2A
        int 0x21
        mov [my_cfg+26], dx         ; día y mes
        mov [my_cfg+28], cx         ; año
        ; a HSPRED, para los demás programas
        cmp word [hsp_cfg+2], 0
        je .ok
        push es
        les di, [hsp_cfg]
        mov si, my_cfg
        mov cx, 30
        cli
        rep movsb
        sti
        pop es
.ok:
        clc
.r:
        ret
.e:
        stc
        ret

cfg_a_mi:
        mov eax, [my_cfg+2]
        mov [my_ip], eax
        mov eax, [my_cfg+6]
        mov [my_mask], eax
        mov eax, [my_cfg+10]
        mov [my_gw], eax
        mov eax, [my_cfg+14]
        mov [my_dns], eax
        ret

; olvidar la configuración (en HSPRED también)
red_olvidar:
        mov byte [my_cfg], 0
        cmp word [hsp_cfg+2], 0
        je .r
        push es
        les di, [hsp_cfg]
        mov byte [es:di], 0
        pop es
.r:
        ret

; ---- \HISPADOS\RED.INI de la unidad de arranque ----
ini_leer:
        mov byte [ini_dhcp], 1
        mov ax, 0x3305
        int 0x21
        add dl, 'A' - 1
        mov [ini_ruta], dl
        mov dx, ini_ruta
        mov ax, 0x3D00
        int 0x21
        jc .r
        mov bx, ax
        mov dx, ini_buf
        mov cx, 2047
        mov ah, 0x3F
        int 0x21
        jnc .lei
        xor ax, ax
.lei:
        mov si, ini_buf
        add si, ax
        mov byte [si], 0
        mov ah, 0x3E
        int 0x21
        mov si, ini_buf
.linea:
        call ini_blancos
        mov al, [si]
        test al, al
        jz .r
        cmp al, ';'
        je .salta
        cmp al, '#'
        je .salta
        cmp al, '['
        je .salta
        ; la clave, en mayúsculas
        mov di, ini_clave
        mov cx, 15
.k:
        mov al, [si]
        cmp al, '='
        je .ke
        cmp al, 13
        je .salta
        cmp al, 10
        je .salta
        test al, al
        jz .r
        inc si
        cmp al, ' '
        je .k
        cmp al, 9
        je .k
        call upcase
        stosb
        loop .k
        jmp .salta
.ke:
        mov byte [di], 0
        inc si
.esp:
        cmp byte [si], ' '
        je .esp1
        cmp byte [si], 9
        jne .esp2
.esp1:
        inc si
        jmp .esp
.esp2:
        mov di, ini_claves
.bk:
        cmp byte [di], 0
        je .salta
        push si
        mov si, ini_clave
        push di
.cmp:
        mov al, [si]
        cmp al, [di]
        jne .dist
        inc si
        inc di
        test al, al
        jnz .cmp
        pop di
        pop si
        ; la encontrada: DI = nombre; después del cero, a dónde
.fin_n:
        cmp byte [di], 0
        je .fn
        inc di
        jmp .fin_n
.fn:
        mov bx, [di+1]
        test bx, bx
        jz .dhcp
        call ip_texto
        jc .salta
        mov [bx], eax
        jmp .salta
.dhcp:
        mov al, [si]
        call upcase
        mov byte [ini_dhcp], 0
        cmp al, 'N'
        je .salta
        cmp al, '0'
        je .salta
        mov byte [ini_dhcp], 1
        jmp .salta
.dist:
        pop di
        pop si
.sig:
        cmp byte [di], 0
        je .sn
        inc di
        jmp .sig
.sn:
        add di, 3
        jmp .bk
.salta:
        ; hasta el fin de la línea
        mov al, [si]
        test al, al
        jz .r
        inc si
        cmp al, 10
        jne .salta
        jmp .linea
.r:
        ret

ini_blancos:
        cmp byte [si], ' '
        je .s
        cmp byte [si], 9
        je .s
        cmp byte [si], 13
        je .s
        cmp byte [si], 10
        je .s
        ret
.s:
        inc si
        jmp ini_blancos

ini_claves  db "DHCP", 0
            dw 0
            db "IP", 0
            dw ini_ip
            db "MASCARA", 0
            dw ini_mask
            db "MASK", 0
            dw ini_mask
            db "PUERTA", 0
            dw ini_gw
            db "GATEWAY", 0
            dw ini_gw
            db "DNS", 0
            dw ini_dns
            db 0

; =============================================================================
;  Direcciones en texto
; =============================================================================
; DS:SI = «a.b.c.d» -> EAX; SI avanza. CF si no lo es.
ip_texto:
        push ebx
        push cx
        push si
        xor ebx, ebx
        mov cx, 4
.o:
        push ebx
        call parse_num
        pop ebx
        jc .no
        cmp eax, 255
        ja .no
        shrd ebx, eax, 8            ; (el primero acaba en el octeto bajo)
        dec cx
        jz .fin
        cmp byte [si], '.'
        jne .no
        inc si
        jmp .o
.fin:
        ; tras la dirección: fin, espacio, «/», «:» o fin de línea
        mov al, [si]
        cmp al, '.'
        je .no
        cmp al, '0'
        jb .ok
        cmp al, '9'
        jbe .no
        cmp al, 'A'
        jae .no
.ok:
        mov eax, ebx
        add sp, 2
        pop cx
        pop ebx
        clc
        ret
.no:
        pop si
        pop cx
        pop ebx
        stc
        ret

; EAX = IP -> pantalla
ip_poner:
        push eax
        push ebx
        push cx
        mov ebx, eax
        mov cx, 4
.l:
        movzx eax, bl
        call putnum
        shr ebx, 8
        dec cx
        jz .r
        mov al, '.'
        call putc
        jmp .l
.r:
        pop cx
        pop ebx
        pop eax
        ret

; =============================================================================
;  DNS (cliente)
; =============================================================================
; DS:SI = nombre (acaba en 0, espacio, «/» o «:») -> EAX = IP; CF y SI =
; mensaje si no se encuentra
dns_buscar:
        push si
        call ip_texto
        pop si
        jnc .r
        mov eax, [my_dns]
        test eax, eax
        jnz .hay
        mov si, m_nodns
        stc
.r:
        ret
.hay:
        ; la pregunta, en tx_base+42
        call tx_base
        add di, 42
        push di
        call tic
        mov [dns_id], ax
        mov [di], ax
        mov dword [di+2], 0x01000001 ; RD; una pregunta
        mov dword [di+6], 0
        mov word [di+10], 0
        add di, 12
.lab:
        mov bx, di                  ; aquí va la longitud de la etiqueta
        inc di
        xor cx, cx
.c:
        lodsb
        cmp al, '.'
        je .fl
        cmp al, ' '
        jbe .fn
        cmp al, '/'
        je .fn
        cmp al, ':'
        je .fn
        stosb
        inc cx
        cmp cx, 63
        ja .mal
        jmp .c
.fl:
        jcxz .mal
        mov [bx], cl
        jmp .lab
.fn:
        jcxz .mal
        mov [bx], cl
        mov byte [di], 0
        mov dword [di+1], 0x01000100 ; tipo A, clase IN
        add di, 5
        pop cx
        sub di, cx
        mov cx, di                  ; octetos
        call tic
        and ax, 0x0FFF
        or ax, 0xC000
        mov [dns_lport], ax
        mov byte [dns_int], 3
.otra:
        mov byte [dns_ok], 0
        mov eax, [my_dns]
        mov bx, [dns_lport]
        mov dx, 53
        push cx
        call udp_enviar
        pop cx
        push cx
        mov bx, dns_ok
        mov cx, 54                  ; 3 s
        call red_esperar
        pop cx
        jnc .lleg
        cmp byte [red_esc], 0
        jne .no
        dec byte [dns_int]
        jnz .otra
.no:
        mov si, m_dnsno
        stc
        ret
.lleg:
        cmp byte [dns_ok], 1
        jne .noexiste
        mov eax, [dns_res]
        clc
        ret
.noexiste:
        mov si, m_dnsnx
        stc
        ret
.mal:
        pop di
        mov si, m_dnsmal
        stc
        ret

; SI = datos, CX = octetos
dns_in:
        cmp cx, 12
        jb .r
        mov ax, [si]
        cmp ax, [dns_id]
        jne .r
        test byte [si+2], 0x80      ; respuesta
        jz .r
        mov bx, si
        add bx, cx                  ; fin
        mov byte [dns_ok], 2
        test byte [si+3], 0x0F      ; error
        jnz .r
        mov dx, [si+6]
        xchg dh, dl                 ; respuestas
        mov ax, [si+4]
        xchg ah, al                 ; preguntas
        add si, 12
.q:
        test ax, ax
        jz .a
        call dns_nombre
        jc .r
        add si, 4
        dec ax
        jmp .q
.a:
        test dx, dx
        jz .r
        call dns_nombre
        jc .r
        lea di, [si+10]
        cmp di, bx
        ja .r
        mov cx, [si+8]
        xchg ch, cl                 ; RDLENGTH
        cmp word [si], 0x0100       ; tipo A
        jne .sig
        cmp word [si+2], 0x0100     ; clase IN
        jne .sig
        cmp cx, 4
        jne .sig
        mov eax, [si+10]
        mov [dns_res], eax
        mov byte [dns_ok], 1
        ret
.sig:
        add si, 10
        add si, cx
        cmp si, bx
        ja .r
        dec dx
        jmp .a
.r:
        ret

; saltar un nombre (etiquetas o puntero) -> SI detrás; CF si se sale (BX = fin)
dns_nombre:
        cmp si, bx
        jae .e
        mov cl, [si]
        test cl, cl
        jz .fin
        mov ch, cl
        and ch, 0xC0
        cmp ch, 0xC0
        je .ptr
        movzx cx, cl
        inc si
        add si, cx
        jmp dns_nombre
.fin:
        inc si
        clc
        ret
.ptr:
        add si, 2
        clc
        ret
.e:
        stc
        ret

; =============================================================================
;  TCP (una conexión de cliente)
; =============================================================================
TF_FIN      equ 0x01
TF_SYN      equ 0x02
TF_RST      equ 0x04
TF_PSH      equ 0x08
TF_ACK      equ 0x10

; EDX = número de secuencia, BL = indicadores, CX = octetos de datos (en
; tx_base+54) -> envía
tcp_salir:
        pushad
        call tx_base
        mov ax, [t_lpuerto]
        xchg ah, al
        mov [di+34], ax
        mov ax, [t_rpuerto]
        xchg ah, al
        mov [di+36], ax
        mov eax, edx
        call bswap_eax
        mov [di+38], eax
        mov eax, [t_rcv]
        call bswap_eax
        mov [di+42], eax
        mov byte [di+46], 0x50      ; cabecera de 20
        mov [di+47], bl
        mov word [di+48], ((RED_VENT & 0xFF) << 8) | (RED_VENT >> 8)
        mov dword [di+50], 0
        add cx, 20
        test bl, TF_SYN
        jz .ns
        mov byte [di+46], 0x60      ; con la opción MSS (1460)
        mov dword [di+54], 0xB4050402
        add cx, 4
.ns:
        mov eax, [t_ip]
        mov bl, 6
        call cks_seudo
        lea si, [di+34]
        call cks_sumar
        call cks_fin
        mov [di+50], ax
        mov eax, [t_ip]
        call ip_enviar
        popad
        ret

; EAX = IP, BX = puerto -> CF (SI = mensaje)
tcp_abrir:
        mov [t_ip], eax
        mov [t_rpuerto], bx
        call tic
        mov dx, ax
        and ax, 0x3FFF
        or ax, 0xC000
        mov [t_lpuerto], ax
        shl eax, 12
        xor ax, dx
        mov [t_una], eax
        inc eax
        mov [t_snd], eax
        mov dword [t_rcv], 0
        mov byte [t_est], 1
        mov byte [t_fin], 0
        mov byte [t_rst], 0
        mov byte [t_cambio], 0
        mov byte [t_int], 6
.otra:
        mov edx, [t_una]
        mov bl, TF_SYN
        xor cx, cx
        call tcp_salir
        mov bx, t_cambio
        mov cx, 27                  ; 1,5 s
        call red_esperar
        jnc .resp
        cmp byte [red_esc], 0
        jne .no
        dec byte [t_int]
        jnz .otra
.no:
        mov byte [t_est], 0
        mov si, m_tcpno
        stc
        ret
.resp:
        cmp byte [t_est], 2
        je .ok
        mov byte [t_est], 0
        mov si, m_tcprst
        stc
        ret
.ok:
        clc
        ret

; DS:SI = datos, CX = octetos (hasta 1460) -> CF (SI = mensaje)
tcp_enviar:
        push di
        push cx
        mov di, tx + 54
        rep movsb
        pop cx
        pop di
        mov edx, [t_snd]
        mov [t_seg], edx
        movzx eax, cx
        add [t_snd], eax
        mov byte [t_int], 8
.otra:
        mov byte [t_acuse], 0
        mov edx, [t_seg]
        mov bl, TF_PSH | TF_ACK
        push cx
        call tcp_salir
        mov bx, t_acuse
        mov cx, 18                  ; 1 s
        call red_esperar
        pop cx
        jnc .ok
        cmp byte [t_rst], 0
        jne .rst
        cmp byte [red_esc], 0
        jne .no
        dec byte [t_int]
        jnz .otra
.no:
        mov si, m_tcpno
        stc
        ret
.rst:
        mov si, m_tcprst
        stc
        ret
.ok:
        cmp byte [t_rst], 0
        jne .rst
        clc
        ret

tcp_cerrar:
        cmp byte [t_est], 2
        jne .r
        mov edx, [t_snd]
        inc dword [t_snd]
        mov byte [t_int], 3
.otra:
        mov byte [t_acuse], 0
        mov bl, TF_FIN | TF_ACK
        xor cx, cx
        call tcp_salir
        mov bx, t_acuse
        mov cx, 18
        call red_esperar
        jnc .r
        cmp byte [red_esc], 0
        jne .r
        dec byte [t_int]
        jnz .otra
.r:
        mov byte [t_est], 0
        ret

; un segmento que llega
tcp_in:
        cmp byte [t_est], 0
        je .r
        mov si, [in_pay]
        mov cx, [in_len]
        cmp cx, 20
        jb .r
        mov eax, [in_src]
        cmp eax, [t_ip]
        jne .r
        mov ax, [si]
        xchg ah, al
        cmp ax, [t_rpuerto]
        jne .r
        mov ax, [si+2]
        xchg ah, al
        cmp ax, [t_lpuerto]
        jne .r
        mov al, [si+12]
        shr al, 4
        shl al, 2
        movzx dx, al                ; longitud de la cabecera
        cmp dx, cx
        ja .r
        mov bl, [si+13]             ; indicadores
        mov eax, [si+4]
        call bswap_eax
        mov [t_sseq], eax
        mov eax, [si+8]
        call bswap_eax
        mov [t_sack], eax
        test bl, TF_RST
        jz .nrst
        mov byte [t_rst], 1
        mov byte [t_est], 0
        mov byte [t_cambio], 1
        mov byte [t_acuse], 1
        mov byte [t_fin], 1
.r:
        ret
.nrst:
        cmp byte [t_est], 1
        jne .est
        ; esperando SYN+ACK
        mov al, bl
        and al, TF_SYN | TF_ACK
        cmp al, TF_SYN | TF_ACK
        jne .r
        mov eax, [t_sack]
        cmp eax, [t_snd]
        jne .r
        mov [t_una], eax
        mov eax, [t_sseq]
        inc eax
        mov [t_rcv], eax
        mov byte [t_est], 2
        mov byte [t_cambio], 1
        jmp .acuse
.est:
        mov byte [t_hay_ack], 0
        test bl, TF_ACK
        jz .datos
        ; t_una < ack <= t_snd: avanza
        mov eax, [t_sack]
        sub eax, [t_una]
        jz .todo
        js .todo
        mov eax, [t_snd]
        sub eax, [t_sack]
        js .todo
        mov eax, [t_sack]
        mov [t_una], eax
.todo:
        ; todo lo enviado tiene acuse (también si el acuse se repite: tras
        ; un reenvío, el que avanzó pudo llegar antes de mirarlo)
        mov eax, [t_una]
        cmp eax, [t_snd]
        jne .datos
        mov byte [t_acuse], 1
.datos:
        sub cx, dx                  ; octetos de datos
        jz .nd
        mov byte [t_hay_ack], 1
        mov eax, [t_sseq]
        cmp eax, [t_rcv]
        jne .nd                     ; fuera de orden: el acuse repetido basta
        movzx eax, cx
        add [t_rcv], eax
        add si, dx
        push bx
        push cx
        call [t_rutina]
        pop cx
        pop bx
        movzx eax, cx
.nd:
        test bl, TF_FIN
        jz .nf
        mov byte [t_hay_ack], 1
        ; el FIN va detrás de los datos de este segmento
        mov eax, [t_sseq]
        movzx edx, cx
        add eax, edx
        cmp eax, [t_rcv]
        jne .nf
        inc dword [t_rcv]
        mov byte [t_fin], 1
.nf:
        cmp byte [t_hay_ack], 0
        je .r2
.acuse:
        mov edx, [t_snd]
        mov bl, TF_ACK
        xor cx, cx
        call tcp_salir
.r2:
        ret

; =============================================================================
;  Datos
; =============================================================================
s_pktdrvr   db "PKT DRVR"
s_tipo_ip   db 0x08, 0x00
s_tipo_arp  db 0x08, 0x06
mac_todos   db 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
ini_ruta    db "A:\HISPADOS\RED.INI", 0
m_nopkt     db "No hay ningún controlador de red cargado: cargue HSPRED (o el packet", 13, 10
            db "driver de su tarjeta). HSPRED /INFO dice qué tarjeta tiene.", 0
m_noeth     db "El controlador de red no es de Ethernet.", 0
m_acc       db "El controlador de red no deja recibir los paquetes IP (¿otro programa?).", 0
m_nodhcp    db "No contesta ningún servidor DHCP (¿está el cable puesto?). Para poner la", 13, 10
            db "dirección a mano: DHCP=NO e IP=, MASCARA=, PUERTA= y DNS= en RED.INI.", 0
m_noip      db "RED.INI dice DHCP=NO pero no lleva la dirección (IP=).", 0
m_nodns     db "No hay servidor DNS (ponga DNS= en RED.INI).", 0
m_dnsno     db "El servidor DNS no contesta.", 0
m_dnsnx     db "No existe ese nombre (el servidor DNS no lo encuentra).", 0
m_dnsmal    db "Ese nombre no es válido.", 0
m_tcpno     db "El servidor no contesta.", 0
m_tcprst    db "El servidor ha rechazado o cortado la conexión.", 0

        section .bss
red_bss:
h_ip        resw 1
h_arp       resw 1
r_vec       resb 1
r_nombre    resb 40
my_mac      resb 6
            resb 2
my_ip       resd 1
my_mask     resd 1
my_gw       resd 1
my_dns      resd 1
my_cfg      resb 30
hsp_cfg     resd 1
cf_renovar  resb 1
en_sondeo   resb 1
red_esc     resb 1
rq_head     resb 1
rq_tail     resb 1
rq_cnt      resb 1
rq_lost     resw 1
ip_id       resw 1
ip_dst      resd 1
in_src      resd 1
in_pay      resw 1
in_len      resw 1
in_ttl      resb 1
arp_sig     resb 1
arp_preg    resd 1
arp_tab     resb RED_NARP * 10
ping_seq    resw 1
eco_dst     resd 1
ping_ok     resb 1
ping_ttl    resb 1
ping_oct    resw 1
dh_xid      resd 1
dh_yi       resd 1
dh_srv      resd 1
dh_mask     resd 1
dh_gw       resd 1
dh_dns      resd 1
dh_lease    resd 1
dh_ofrecida resd 1
dh_ofsrv    resd 1
dh_tipo     resb 1
dh_llego    resb 1
dh_intentos resb 1
dh_req      resb 1
ini_dhcp    resb 1
ini_ip      resd 1
ini_mask    resd 1
ini_gw      resd 1
ini_dns     resd 1
ini_clave   resb 16
dns_id      resw 1
dns_lport   resw 1
dns_ok      resb 1
dns_int     resb 1
dns_res     resd 1
t_ip        resd 1
t_rpuerto   resw 1
t_lpuerto   resw 1
t_snd       resd 1
t_una       resd 1
t_rcv       resd 1
t_seg       resd 1
t_sseq      resd 1
t_sack      resd 1
t_rutina    resw 1
t_est       resb 1
t_fin       resb 1
t_rst       resb 1
t_cambio    resb 1
t_acuse     resb 1
t_hay_ack   resb 1
t_int       resb 1
txa         resb 64
tx          resb 1514
txr         resb 1514
ini_buf     resb 2048
rq_buf      resb RQ_N * RQ_SZ
red_bss_fin:
        section .text
