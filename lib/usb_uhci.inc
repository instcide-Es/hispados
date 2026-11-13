; =============================================================================
;  Hispa-DOS - HSPUSB: controladora UHCI (USB 1.x de Intel y VIA), residente
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Según «Universal Host Controller Interface (UHCI) Design Guide» 1.1 (Intel).
;  Registros en puertos de E/S (hc_base): USBCMD 00h, USBSTS 02h, USBINTR 04h,
;  FRNUM 06h, FRBASEADD 08h, SOFMOD 0Ch, PORTSC 10h + 2·n.
;
;  Planificación: las 1024 entradas de la lista de tramas apuntan al mismo
;  sitio, la cadena de colas (QH) de la controladora:
;       tuberías de interrupción (una cola y un TD cada una) -> cola de
;       control y bulk (hc_mem) -> fin
;  Los informes se piden en todas las tramas (1 ms), que nunca es menos de
;  lo que pide el aparato. Control y bulk son síncronos: los TD (uno por
;  paquete, hasta XTD_N por tanda, en profundidad) se cuelgan de la cola, se
;  espera a que la controladora los termine y se descuelgan.
;  Sin interrupciones: todo se consulta (USBINTR = 0).
;
;  Una UHCI sin nada conectado se queda «aparcada»: parada y sin lista de
;  tramas (4 KB que no se gastan). El estado de sus puertos se sigue viendo,
;  y si aparece algo se arranca con la lista de reserva (u_start).
; =============================================================================

; puerto de E/S del registro AL -> DX
u_reg:
        mov dx, [cur_base]
        add dl, al
        adc dh, 0
        ret
; PORTSC del puerto CL -> DX
u_psc:
        push ax
        mov al, cl
        dec al
        shl al, 1
        add al, 0x10
        call u_reg
        pop ax
        ret

; añadir un TD a la tanda: AL = PID (2Dh SETUP, 69h IN, E1h OUT), EBX =
; dirección física del búfer, CX = bytes. El testigo de alternancia sale de
; xr_tog (y cambia).
u_td:
        push di
        push edx
        mov dl, al
        movzx di, byte [xr_n]
        shl di, 4
        add di, [cl_xtd]
        mov eax, (1 << 23) | (3 << 27)  ; activo, tres intentos
        cmp byte [xf_speed], 2
        jne .fs
        or eax, 1 << 26                 ; aparato de velocidad baja
.fs:
        cmp dl, 0x69
        jne .o
        or eax, 1 << 29                 ; entrada: detectar el paquete corto
.o:
        mov [di+4], eax
        movzx eax, cx
        dec eax
        and eax, 0x7FF
        shl eax, 21
        mov al, dl
        movzx edx, byte [xf_addr]
        shl edx, 8
        or eax, edx
        movzx edx, byte [xf_ep]
        shl edx, 15
        or eax, edx
        movzx edx, byte [xr_tog]
        shl edx, 19
        or eax, edx
        mov [di+8], eax
        mov [di+12], ebx
        xor byte [xr_tog], 1
        inc byte [xr_n]
        pop edx
        pop di
        ret

; ejecutar los xr_n TD de la tanda -> CF si falla (error o plazo);
; xr_done = TD completos, xr_got = bytes, xr_short = 1 si acabó en corto
u_run:
        mov bx, [cl_xtd]
        call phys
        mov [xr_p0], eax
        movzx cx, byte [xr_n]
        mov di, bx
.lk:
        add eax, 16
        mov edx, eax
        or edx, 4                       ; en profundidad: varios por trama
        cmp cx, 1
        jne .l2
        mov edx, 1                      ; el último: fin
.l2:
        mov [di], edx
        add di, 16
        loop .lk
        mov di, [cur_hmem]
        mov eax, [xr_p0]
        mov [di+4], eax                 ; elemento de la cola: a trabajar
        mov cx, [xr_tmo]
        call tmo_start
.w:
        sti
        call u_scan
        cmp al, 1
        je .ok
        cmp al, 2
        je .bad
        call tmo_check
        jnc .w
.bad:
        mov dword [di+4], 1
        call u_frame2                   ; que la controladora suelte los TD
        stc
        ret
.ok:
        mov dword [di+4], 1
        clc
        ret

; estado de la tanda -> AL = 0 en curso, 1 terminada, 2 error
u_scan:
        push si
        push cx
        push edx
        mov byte [xr_done], 0
        mov byte [xr_short], 0
        mov word [xr_got], 0
        mov si, [cl_xtd]
        movzx cx, byte [xr_n]
.l:
        mov eax, [si+4]
        test eax, 1 << 23
        jnz .wait
        test eax, 0x00760000            ; parado, búfer, desbordado, CRC/plazo, relleno
        jnz .err
        inc ax
        and ax, 0x7FF
        add [xr_got], ax
        inc byte [xr_done]
        mov edx, [si+8]
        shr edx, 21
        inc dx
        and dx, 0x7FF
        cmp ax, dx
        jb .short
        add si, 16
        loop .l
        mov al, 1
        jmp .r
.short:
        mov byte [xr_short], 1
        mov al, 1
        jmp .r
.wait:
        xor al, al
        jmp .r
.err:
        mov al, 2
.r:
        pop edx
        pop cx
        pop si
        ret

; esperar a que pasen dos tramas (o un tiempo prudente si está parada)
u_frame2:
        push ax
        push bx
        push ecx
        push dx
        mov al, 6
        call u_reg
        mov bh, 2
        mov ecx, 400000
.n:
        in ax, dx
        mov bl, al
.l:
        dec ecx
        jz .r
        in ax, dx
        cmp al, bl
        je .l
        dec bh
        jnz .n
.r:
        pop dx
        pop ecx
        pop bx
        pop ax
        ret

; ---- control: tres fases (SETUP, datos de entrada, estado) ----
u_control:
        mov byte [xr_n], 0
        mov byte [xr_tog], 0
        mov bx, ctl_setup
        call phys
        mov ebx, eax
        mov cx, 8
        mov al, 0x2D
        call u_td
        call u_run
        jc .r
        mov cx, [ctl_setup+6]
        mov [xr_left], cx
        mov byte [xr_tog], 1
        jcxz .st
        mov bx, [ctl_buf]
        call zone
        call phys
        mov [xr_buf], eax
.d:
        mov byte [xr_n], 0
.d1:
        mov cx, [xr_left]
        jcxz .d2
        cmp cx, [xf_mps]
        jbe .d3
        mov cx, [xf_mps]
.d3:
        sub [xr_left], cx
        mov ebx, [xr_buf]
        movzx eax, cx
        add [xr_buf], eax
        mov al, 0x69
        call u_td
        cmp byte [xr_n], XTD_N
        jb .d1
.d2:
        call u_run
        jc .r
        cmp byte [xr_short], 0
        jne .st
        cmp word [xr_left], 0
        jne .d
.st:
        mov byte [xr_n], 0
        mov byte [xr_tog], 1
        mov al, 0x69                    ; sin datos: el estado es de entrada
        cmp word [ctl_setup+6], 0
        je .s1
        mov al, 0xE1
.s1:
        xor cx, cx
        xor ebx, ebx
        call u_td
        call u_run
.r:
        ret

; ---- bulk: bk_dir, xf_buf, xf_len, xf_ep, xf_mps, xr_tog ----
u_bulk:
.b:
        mov byte [xr_n], 0
        mov al, [xr_tog]
        mov [xr_tog0], al
.b1:
        mov cx, [xf_len]
        jcxz .b2
        cmp cx, [xf_mps]
        jbe .b3
        mov cx, [xf_mps]
.b3:
        sub [xf_len], cx
        mov ebx, [xf_buf]
        movzx eax, cx
        add [xf_buf], eax
        mov al, 0xE1
        cmp byte [bk_dir], 0
        je .b4
        mov al, 0x69
.b4:
        call u_td
        cmp byte [xr_n], XTD_N
        jb .b1
.b2:
        cmp byte [xr_n], 0
        je .ok
        call u_run
        pushf                           ; el testigo avanza con los TD completos
        mov al, [xr_done]
        and al, 1
        xor al, [xr_tog0]
        mov [xr_tog], al
        popf
        jc .r
        cmp byte [xr_short], 0
        jne .ok
        cmp word [xf_len], 0
        jne .b
.ok:
        clc
.r:
        ret

; ---- tuberías de interrupción ----
section .init                   ; (solo al arrancar)
u_pipe_open:
        call c_pipe_blk
        movzx si, byte [cur_hc]
        shl si, 2
        mov eax, [hc_per+si]
        mov [di], eax
        mov dword [di+4], 1
        mov bx, di
        call phys
        or al, 2                        ; es una cola
        mov [hc_per+si], eax
        call fl_fill
        ret

section .text
u_pipe_queue:
        call c_pipe_blk
        lea si, [di+0x60]
        mov dword [si], 1
        mov eax, (1 << 23) | (3 << 27)
        cmp byte [xf_speed], 2
        jne .fs
        or eax, 1 << 26
.fs:
        mov [si+4], eax
        movzx eax, word [xf_len]
        dec eax
        and eax, 0x7FF
        shl eax, 21
        mov al, 0x69
        movzx edx, byte [xf_addr]
        shl edx, 8
        or eax, edx
        movzx edx, byte [xf_ep]
        shl edx, 15
        or eax, edx
        movzx edx, byte [ip_tog+bx]
        shl edx, 19
        or eax, edx
        mov [si+8], eax
        call c_pipe_buf
        mov [si+12], eax
        mov bx, si
        call phys
        mov [di+4], eax
        ret

; -> CF si el informe no ha llegado; si no, AX = bytes (FFFFh = error)
u_pipe_check:
        call c_pipe_blk
        mov eax, [di+0x64]
        test eax, 1 << 23
        jnz .pend
        test eax, 0x00760000
        jnz .err
        xor byte [ip_tog+bx], 1
        inc ax
        and ax, 0x7FF
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
u_rp_get:
        push dx
        call u_psc
        in ax, dx
        mov ah, al
        and al, 1                       ; conectado
        test ah, 4
        jz .a
        or al, 2                        ; habilitado
.a:
        test ah, 2
        jz .b
        or al, 0x80                     ; ha cambiado la conexión
.b:
        pop dx
        ret
u_rp_ack:
        push ax
        push dx
        call u_psc
        in ax, dx
        and ax, 0x1004                  ; (conservar habilitado y suspendido)
        or ax, 0x000A                   ; borrar los cambios (se escriben a 1)
        out dx, ax
        pop dx
        pop ax
        ret
u_rp_reset:
        call u_psc
        in ax, dx
        test al, 1
        jz .no
        mov ax, 0x0200                  ; reinicio del puerto: 50 ms al menos
        out dx, ax
        mov cx, 2
        call tick_wait
        mov cl, [port]
        call u_psc
        xor ax, ax
        out dx, ax
        mov cx, 500                     ; un respiro antes de habilitarlo
.d:
        in ax, dx
        loop .d
        mov cx, 20
.en:
        mov ax, 0x000E                  ; habilitar y borrar los cambios
        out dx, ax
        push cx
        mov cx, 500
.d2:
        in ax, dx
        loop .d2
        pop cx
        in ax, dx
        test al, 4
        jnz .ok
        test al, 1
        jz .no
        loop .en
.no:
        stc
        ret
.ok:
        mov bl, 1
        test ah, 1                      ; bit 8: velocidad baja
        jz .sp
        mov bl, 2
.sp:
        mov [speed], bl
        call reset_rest
        clc
        ret

u_time:
        push dx
        push ds
        push cs
        pop ds
        mov al, 6
        call u_reg
        in ax, dx
        shl ax, 3
        and ax, 0x3FFF
        pop ds
        pop dx
        ret

; ---- arrancar y aparcar ----
; poner en marcha la UHCI actual con una lista de tramas (la de reserva o,
; durante el inicio, una nueva) -> CF si no hay lista
u_start:
        pushad
        push ds
        push cs
        pop ds
        movzx si, byte [cur_hc]
        mov eax, [fl_free]
        test eax, eax
        jnz .have
        call fl_alloc
        jc .r
        jmp .set
.have:
        mov dword [fl_free], 0
.set:
        shl si, 2
        mov [hc_fl+si], eax
        mov bx, [cur_hmem]              ; cola de control y bulk, vacía
        mov dword [bx], 1
        mov dword [bx+4], 1
        call phys
        or al, 2
        mov [hc_per+si], eax
        call fl_fill
        mov al, 8
        call u_reg
        mov eax, [hc_fl+si]
        call lphys
        out dx, eax                     ; FRBASEADD
        sub dx, 2
        xor ax, ax
        out dx, ax                      ; FRNUM
        sub dx, 4
        mov ax, 0x003F
        out dx, ax                      ; USBSTS: borrar
        sub dx, 2
        mov ax, 0x00C1                  ; en marcha, configurada, paquetes de 64
        out dx, ax
        shr si, 2
        and byte [hc_flags+si], ~HCF_PARK
        clc
.r:
        pop ds
        popad
        ret

; parar la UHCI actual y dejar libre su lista de tramas
u_park:
        pushad
        push ds
        push cs
        pop ds
        movzx si, byte [cur_hc]
        test byte [hc_flags+si], HCF_PARK
        jnz .r
        xor al, al
        call u_reg
        xor ax, ax
        out dx, ax
        add dx, 2
        mov cx, 0xFFFF
.h:
        in ax, dx
        test al, 0x20                   ; HCHalted
        jnz .hd
        loop .h
.hd:
        or byte [hc_flags+si], HCF_PARK
        shl si, 2
        mov eax, [hc_fl+si]
        mov dword [hc_fl+si], 0
        cmp dword [fl_free], 0
        jne .r
        mov [fl_free], eax
.r:
        pop ds
        popad
        ret

; ¿tiene algo conectado la UHCI actual? ZF = 1 si no
u_any:
        push bx
        push cx
        push ax
        xor cl, cl
        movzx bx, byte [cs:cur_hc]
.l:
        inc cl
        cmp cl, [cs:hc_np+bx]
        ja .none
        call rp_get
        test al, 1
        jz .l
        pop ax
        pop cx
        pop bx
        ret                             ; ZF = 0
.none:
        xor cl, cl                      ; ZF = 1
        pop ax
        pop cx
        pop bx
        ret
