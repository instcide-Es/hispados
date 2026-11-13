; =============================================================================
;  Hispa-DOS - HSPUSB: controladora EHCI (USB 2.0), parte residente
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Según «Enhanced Host Controller Interface Specification for USB» 1.0.
;  Registros operativos en memoria (hc_base = base + CAPLENGTH): USBCMD 00h,
;  USBSTS 04h, USBINTR 08h, FRINDEX 0Ch, CTRLDSSEGMENT 10h, PERIODICLISTBASE
;  14h, ASYNCLISTADDR 18h, CONFIGFLAG 40h, PORTSC 44h + 4·n.
;
;  Memoria de cada controladora (hc_mem): dos colas (QH) de 96 bytes en
;  anillo, que son toda su lista asíncrona: la cabecera (bit H, siempre
;  parada) y la de trabajo (+60h). Como control y bulk son síncronos, la de
;  trabajo se rellena en cada transferencia con la dirección, el endpoint,
;  la velocidad y el paquete máximo del aparato que toque (y, si es de USB
;  1.x y cuelga de un concentrador de alta velocidad, con la dirección y el
;  puerto de ese concentrador: la controladora hace entonces las
;  transacciones partidas, «split», con su traductor). Un qTD lleva una
;  transferencia entera (hasta 5 páginas): control son tres, bulk uno.
;  Las tuberías de interrupción (una cola y un qTD cada una) van en la lista
;  periódica, que solo se crea (4 KB) y se pone en marcha cuando hace falta
;  la primera: sus 1024 tramas apuntan todas a la cadena de tuberías
;  (microtrama 0 de cada trama; con traductor, fin en las microtramas 2-4).
;
;  Los puertos raíz son del EHCI (CONFIGFLAG = 1). Al reiniciar uno, si el
;  aparato no es de alta velocidad no queda habilitado: se cede a la
;  controladora compañera (UHCI u OHCI) con el bit Port Owner, y aparece en
;  el puerto de ella.
;  Sin interrupciones: todo se consulta.
; =============================================================================

e_last      dw 0
e_handed    db 0                    ; se ha cedido algún puerto a una compañera

; características y capacidades de la cola SI con los datos de xf_*.
; DL: bit 0 = endpoint de control, bit 1 = el testigo de alternancia lo
; pone cada qTD
e_qh_set:
        movzx eax, byte [xf_addr]
        movzx ecx, byte [xf_ep]
        shl ecx, 8
        or eax, ecx
        movzx ecx, word [xf_mps]
        shl ecx, 16
        or eax, ecx
        cmp byte [xf_speed], 3
        jb .lf
        or eax, 2 << 12                 ; alta velocidad
        jmp .dtc
.lf:
        cmp byte [xf_speed], 2
        jne .fs
        or eax, 1 << 12                 ; baja
.fs:
        test dl, 1
        jz .dtc
        or eax, 1 << 27                 ; control que no es de alta velocidad
.dtc:
        test dl, 2
        jz .w
        or eax, 1 << 14
.w:
        mov [si+4], eax
        mov eax, 1 << 30                ; una transacción por microtrama
        cmp byte [xf_speed], 3
        jae .c
        movzx ecx, byte [xf_tthub]      ; el traductor: concentrador y puerto
        shl ecx, 16
        or eax, ecx
        movzx ecx, byte [xf_ttport]
        shl ecx, 23
        or eax, ecx
.c:
        mov [si+8], eax
        ret

; las cinco páginas del búfer EBX en el qTD DI, y la parte alta a cero
e_bufs:
        push eax
        mov [di+12], ebx
        mov eax, ebx
        and eax, 0xFFFFF000
        add eax, 0x1000
        mov [di+16], eax
        add eax, 0x1000
        mov [di+20], eax
        add eax, 0x1000
        mov [di+24], eax
        add eax, 0x1000
        mov [di+28], eax
        xor eax, eax
        mov [di+32], eax
        mov [di+36], eax
        mov [di+40], eax
        mov [di+44], eax
        mov [di+48], eax
        pop eax
        ret

; preparar el qTD AL de la reserva: AH = PID (0 salida, 1 entrada, 2 SETUP),
; DL = testigo, EBX = búfer (dirección física), CX = bytes. Queda enlazado
; con el siguiente.
e_qtd:
        push di
        push esi
        push edx
        movzx di, al
        shl di, 6
        add di, [cl_xtd]
        movzx esi, cx
        shl esi, 16
        movzx eax, ah
        shl eax, 8
        or esi, eax
        movzx eax, dl
        shl eax, 31
        or esi, eax
        or esi, (3 << 10) | 0x80        ; tres intentos, activo
        mov [di+8], esi
        call e_bufs
        push bx
        lea bx, [di+64]
        call phys
        pop bx
        mov [di], eax
        mov dword [di+4], 1
        pop edx
        pop esi
        pop di
        ret

; ejecutar los xr_n qTD de la reserva por la cola SI -> CF si falla
e_run:
        movzx di, byte [xr_n]
        dec di
        shl di, 6
        add di, [cl_xtd]
        mov dword [di], 1               ; el último: fin
        mov [e_last], di
        mov dword [si+12], 0
        mov dword [si+20], 1
        mov dword [si+24], 0            ; ni activa ni parada
        mov bx, [cl_xtd]
        call phys
        mov [si+16], eax                ; siguiente qTD: a trabajar
        mov cx, [xr_tmo]
        call tmo_start
.w:
        sti
        mov bx, [cl_xtd]
        movzx cx, byte [xr_n]
.s:
        test byte [bx+8], 0x40
        jnz .halt
        add bx, 64
        loop .s
        test byte [di+8], 0x80
        jz .ok
        call tmo_check
        jnc .w
        call e_abort
.halt:
        stc
        ret
.ok:
        clc
        ret

; quitar de en medio una transferencia que no termina: la cola de trabajo
; sale del anillo, se espera a que la controladora lo confirme (timbre de
; avance de la lista asíncrona) y vuelve limpia
e_abort:
        pushad
        mov bx, [cur_hmem]
        call phys
        or al, 2
        mov [bx], eax
        xor esi, esi
        call hc_rd
        or eax, 0x40
        call mwr
        mov cx, 3
        call tmo_start
.w:
        mov esi, 4
        call hc_rd
        test al, 0x20
        jnz .d
        call tmo_check
        jnc .w
.d:
        mov eax, 0x20
        call mwr
        mov bx, [cur_hmem]
        add bx, 0x60
        mov dword [bx+12], 0
        mov dword [bx+16], 1
        mov dword [bx+20], 1
        mov dword [bx+24], 0
        call phys
        or al, 2
        mov bx, [cur_hmem]
        mov [bx], eax
        popad
        ret

e_control:
        mov si, [cur_hmem]
        add si, 0x60
        mov dl, 3
        call e_qh_set
        mov bx, ctl_setup
        call phys
        mov ebx, eax
        mov cx, 8
        mov ax, 0x0200                  ; qTD 0: SETUP
        xor dl, dl
        call e_qtd
        mov byte [xr_n], 1
        mov cx, [ctl_setup+6]
        jcxz .st
        mov bx, [ctl_buf]
        call zone
        call phys
        mov ebx, eax
        mov cx, [ctl_setup+6]
        mov ax, 0x0101                  ; qTD 1: datos de entrada
        mov dl, 1
        call e_qtd
        mov di, [cl_xtd]
        mov eax, [di+64]
        mov [di+64+4], eax              ; si llegan menos: derecho al estado
        mov byte [xr_n], 2
        xor ah, ah                      ; estado: salida
        jmp .s2
.st:
        mov ah, 1                       ; sin datos: estado de entrada
.s2:
        mov al, [xr_n]
        xor cx, cx
        xor ebx, ebx
        mov dl, 1
        call e_qtd
        inc byte [xr_n]
        call e_run
        ret

e_bulk:
        mov si, [cur_hmem]
        add si, 0x60
        mov dl, 2
        call e_qh_set
        mov ebx, [xf_buf]
        mov cx, [xf_len]
        xor ah, ah
        cmp byte [bk_dir], 0
        je .o
        mov ah, 1
.o:
        xor al, al
        mov dl, [xr_tog]
        call e_qtd
        mov byte [xr_n], 1
        call e_run
        jc .r
        mov di, [cl_xtd]
        mov eax, [di+8]                 ; el testigo siguiente queda en el qTD
        shr eax, 31
        mov [xr_tog], al
        clc
.r:
        ret

; ---- tuberías de interrupción ----
section .init                   ; (solo al arrancar)
e_pipe_open:
        call c_pipe_blk
        movzx si, byte [cur_hc]
        shl si, 2
        cmp dword [hc_fl+si], 0
        jne .have
        ; la primera de esta controladora: lista periódica, y en marcha
        call fl_alloc
        jc .r
        mov [hc_fl+si], eax
        mov dword [hc_per+si], 1
        call fl_fill
        call lphys
        push si
        mov esi, 0x14
        call hc_wr
        xor esi, esi
        call hc_rd
        or eax, 0x10
        call mwr
        pop si
.have:
        push si
        mov si, di
        xor dl, dl
        call e_qh_set
        pop si
        or dword [di+8], 0x01           ; microtrama 0
        cmp byte [xf_speed], 3
        jae .hs
        or dword [di+8], 0x1C00         ; con traductor: fin en las 2, 3 y 4
.hs:
        mov dword [di+12], 0
        mov dword [di+16], 1
        mov dword [di+20], 1
        mov dword [di+24], 0
        mov eax, [hc_per+si]
        mov [di], eax
        mov bx, di
        call phys
        or al, 2
        mov [hc_per+si], eax
        call fl_fill
        clc
.r:
        ret

section .text
e_pipe_queue:
        call c_pipe_blk
        push di
        add di, 0x60
        mov dword [di], 1
        mov dword [di+4], 1
        movzx eax, word [xf_len]
        shl eax, 16
        or eax, (3 << 10) | (1 << 8) | 0x80
        mov [di+8], eax
        call c_pipe_buf
        mov ebx, eax
        call e_bufs
        mov bx, di
        call phys
        pop di
        test byte [di+24], 0x40
        jz .nh
        and dword [di+24], 0x80000000   ; parada por un error: reanudarla
.nh:
        mov [di+16], eax
        ret

; -> CF si el informe no ha llegado; si no, AX = bytes (FFFFh = error)
e_pipe_check:
        call c_pipe_blk
        mov eax, [di+0x68]
        test al, 0x80
        jnz .pend
        test al, 0x40
        jnz .err
        shr eax, 16
        and ax, 0x7FFF                  ; lo que ha faltado por llegar
        mov si, bx
        shl si, 1
        mov dx, [ip_len+si]
        sub dx, ax
        mov ax, dx
        clc
        ret
.err:
        mov ax, 0xFFFF
        clc
        ret
.pend:
        stc
        ret

; ---- puertos raíz ----
e_psc:
        movzx esi, cl
        dec esi
        shl esi, 2
        add esi, 0x44
        ret
e_rp_get:
        push esi
        push edx
        call e_psc
        call hc_rd
        mov edx, eax
        xor al, al
        test dh, 0x20                   ; cedido a la compañera: no es nuestro
        jnz .r
        mov al, dl
        and al, 1
        test dl, 4
        jz .a
        or al, 2
.a:
        test dl, 2
        jz .r
        or al, 0x80
.r:
        pop edx
        pop esi
        ret
e_rp_ack:
        push eax
        push esi
        call e_psc
        call hc_rd
        or eax, 0x2A                    ; los cambios se borran escribiendo 1
        call mwr
        pop esi
        pop eax
        ret
e_rp_reset:
        call e_psc
        call hc_rd
        test al, 1
        jz .no
        test ah, 0x20
        jnz .no
        and eax, ~(0x2A | 4)
        or eax, 0x100                   ; reinicio: 50 ms al menos
        call mwr
        mov cx, 2
        call tick_wait
        call mrd
        and eax, ~(0x2A | 0x100)
        call mwr
        mov cx, 2
        call tmo_start
.w:
        call mrd
        test ah, 1
        jz .d
        call tmo_check
        jnc .w
.d:
        mov cx, 2000                    ; (hasta 2 ms para que quede habilitado)
.d2:
        call mrd
        test al, 4
        jnz .hs
        loop .d2
        test al, 1
        jz .no
        ; no es de alta velocidad: a la controladora compañera
        and eax, ~0x2A
        or eax, 1 << 13
        call mwr
        mov byte [e_handed], 1
.no:
        stc
        ret
.hs:
        mov byte [speed], 3
        or eax, 0x2A
        call mwr
        call reset_rest
        clc
        ret

e_time:
        push esi
        mov esi, 0x0C
        call hc_rd
        and ax, 0x3FFF
        pop esi
        ret
