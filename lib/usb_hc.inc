; =============================================================================
;  Hispa-DOS - HSPUSB: la capa común de «controladora» (parte residente)
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  HSPUSB lleva las cuatro clases de controladora USB que hay en un PC:
;    UHCI (USB 1.x de Intel y VIA; puertos de E/S)        lib/usb_uhci.inc
;    OHCI (USB 1.x de los demás; registros en memoria)    lib/usb_ohci.inc
;    EHCI (USB 2.0; registros en memoria)                 lib/usb_ehci.inc
;    xHCI (USB 3; registros en memoria)                   drivers/hspusb.asm
;  y varias a la vez (lo normal entre 2002 y 2012: un EHCI con dos a cuatro
;  UHCI u OHCI «compañeras» para los aparatos de USB 1.x).
;
;  Lo de arriba (enumeración, almacenamiento, teclado, ratón, mandos y
;  concentradores) no sabe qué controladora hay debajo. Solo usa estas
;  operaciones, que aquí se reparten según la clase de la controladora del
;  aparato:
;
;    control       petición de control al aparato actual (cur_pool)
;    bulk          transferencia bulk de la memoria USB (bk_pool)
;    pipe_queue    pedir el siguiente informe de una tubería de interrupción
;    usb_poll      recoger los informes que hayan llegado (-> pipe_done)
;    rp_get        estado de un puerto raíz (conectado, habilitado, cambio)
;    rp_ack        dar por visto el cambio de conexión de un puerto raíz
;    port_enable   reiniciar y habilitar un puerto raíz (-> velocidad)
;    hc_time       reloj de la controladora, en microtramas (125 us)
;    dev_free      dar de baja un aparato
;
;  En el xHCI la controladora lleva las direcciones, los testigos de
;  alternancia (DATA0/DATA1) y el troceo en paquetes; en las otras tres
;  («clásicas») los lleva este fichero: tabla de aparatos dv_*, datos de la
;  transferencia en curso xf_*, y una reserva común de descriptores de
;  transferencia (cl_xtd) para las transferencias de control y bulk, que
;  son síncronas (una cada vez, en todo el driver).
;
;  [USB-V86] Acceso a la memoria física, para quien adapte esto al modo V86:
;    mrd / mwr     (hspusb.asm) los ÚNICOS accesos a registros en memoria
;                  (MMIO): xHCI, OHCI y EHCI pasan todos por ahí (hc_rd y
;                  hc_wr solo suman la base de la controladora actual)
;    phys, lphys   (hspusb.asm) la ÚNICA traducción a dirección física (de
;                  CS:desplazamiento y de dirección lineal); todo lo que se
;                  da a una controladora (colas, descriptores, búferes,
;                  listas de tramas) sale de ahí. Nada cruza de página.
;    fl_seg        dirección lineal de una lista de tramas -> ES:DI, para
;                  rellenarla (queda fuera de los 64 KB de CS)
;    UHCI          no usa MMIO: solo puertos de E/S (IN/OUT) y memoria
;                  convencional por «phys»
; =============================================================================

HC_N        equ 8                   ; controladoras a la vez
HCT_UHCI    equ 0                   ; clase = interfaz de programación PCI / 10h
HCT_OHCI    equ 1
HCT_EHCI    equ 2
HCT_XHCI    equ 3
HCF_OURS    equ 0x01                ; tomada: la lleva HSPUSB
HCF_PARK    equ 0x02                ; UHCI parada y sin lista de tramas (nada conectado)
HCF_OWNED   equ 0x04                ; es de la BIOS (USB Legacy Support, SMM)
HCF_RUN     equ 0x08                ; la BIOS la tiene en marcha
HCF_BAD     equ 0x10                ; no se puede usar
XTD_N       equ 32                  ; UHCI: paquetes por tanda (un TD cada uno)
XTD_BYTES   equ XTD_N * 16          ; reserva de TD de control y bulk (qTD de EHCI: 3 de 64)
PIPE_BLK    equ 0xA0                ; cola + TD de una tubería (el TD, en +60h)

; ---- tabla de controladoras ----
hc_n        db 0
cur_hc      db 0                    ; la controladora actual (hc_sel)
cur_type    db HCT_XHCI
z_closed    db 0                    ; ya no se puede reservar memoria (fin del inicio)
cur_hmem    dw 0
cur_base    dd 0
hc_type     times HC_N db 0
hc_flags    times HC_N db 0
hc_np       times HC_N db 0         ; puertos raíz
hc_mem      times HC_N dw 0         ; sus estructuras (desplazamiento en CS)
hc_base     times HC_N dd 0         ; UHCI: puerto de E/S; OHCI y xHCI: registros;
                                    ; EHCI: registros operativos (base + CAPLENGTH)
hc_fl       times HC_N dd 0         ; UHCI y EHCI: lista de tramas (dirección lineal; 0 = sin ella)
hc_per      times HC_N dd 0         ; primer eslabón de la cadena periódica
fl_free     dd 0                    ; lista de tramas de reserva (para una UHCI aparcada)
ztop        dd 0                    ; fin de la memoria reservada (desde CS)
zlimit      dd 0xFFF0               ; hasta dónde hay memoria (desde CS)
cl_xtd      dw 0                    ; reserva de TD de control y bulk
cl_pipe     dw 0                    ; bloques de las tuberías (PIPE_BLK cada una)
hc_vec      dw 0

; ---- tabla de aparatos (una entrada por cada una de la reserva) ----
dv_hc       times POOL_N db 0       ; su controladora
dv_addr     times POOL_N db 0       ; clásicas: dirección USB (0 = recién reiniciado)
dv_speed    times POOL_N db 0       ; 1 plena, 2 baja, 3 alta, 4 súper
dv_mps0     times POOL_N db 8       ; paquete máximo del endpoint 0
dv_tthub    times POOL_N db 0       ; EHCI: concentrador de alta velocidad que le
dv_ttport   times POOL_N db 0       ;   traduce (dirección y puerto; 0 = ninguno)

; ---- transferencia clásica en curso ----
xf_addr     db 0
xf_ep       db 0
xf_speed    db 0
xf_tthub    db 0
xf_ttport   db 0
xr_n        db 0                    ; TD preparados
xr_tog      db 0                    ; testigo de alternancia del siguiente paquete
xr_tog0     db 0
xr_done     db 0                    ; TD completos
xr_short    db 0                    ; ha llegado un paquete corto
xf_mps      dw 0
xf_len      dw 0
xr_left     dw 0
xr_got      dw 0                    ; bytes transferidos
xr_tmo      dw 40                   ; plazo (tics): 2 s control, 5 s bulk
xf_buf      dd 0
xr_buf      dd 0
xr_p0       dd 0
bk_pool     db 0                    ; el aparato de la memoria USB
tog_in      db 0                    ; sus testigos de alternancia (clásicas)
tog_out     db 0
ip_cur      times PIPE_N db 0       ; OHCI: cuál de los dos TD de la tubería es el de cola

; elegir la controladora AL (conserva todos los registros)
hc_sel:
        push bx
        push ax
        movzx bx, al
        mov [cs:cur_hc], al
        mov al, [cs:hc_type+bx]
        mov [cs:cur_type], al
        shl bx, 1
        mov ax, [cs:hc_mem+bx]
        mov [cs:cur_hmem], ax
        shl bx, 1
        push eax
        mov eax, [cs:hc_base+bx]
        mov [cs:cur_base], eax
        pop eax
        pop ax
        pop bx
        ret

; elegir la controladora del aparato AL de la reserva
dev_hc:
        push bx
        push ax
        movzx bx, al
        mov al, [cs:dv_hc+bx]
        call hc_sel
        pop ax
        pop bx
        ret

; saltar a la rutina de la tabla %1 que toca a la clase de la controladora
; actual (el orden de las tablas: UHCI, OHCI, EHCI, xHCI)
%macro HCJMP 1
        push bx
        movzx bx, byte [cs:cur_type]
        shl bx, 1
        mov bx, [cs:%1+bx]
        mov [cs:hc_vec], bx
        pop bx
        jmp [cs:hc_vec]
%endmacro

; registro de la controladora actual (OHCI, EHCI): ESI = desplazamiento
; -> EAX; ESI queda con la dirección, para escribirlo después con mwr
hc_rd:
        add esi, [cs:cur_base]
        call mrd
        ret
hc_wr:
        add esi, [cs:cur_base]
        call mwr
        ret

; datos del aparato AL para una transferencia clásica (DS = CS)
c_xf_dev:
        push bx
        movzx bx, al
        mov al, [dv_addr+bx]
        mov [xf_addr], al
        mov al, [dv_speed+bx]
        mov [xf_speed], al
        mov al, [dv_tthub+bx]
        mov [xf_tthub], al
        mov al, [dv_ttport+bx]
        mov [xf_ttport], al
        pop bx
        ret

; aparato nuevo (cur_pool) en una controladora clásica: recién reiniciado
; contesta en la dirección 0; se le da la suya (la entrada de la reserva + 1)
; -> CF si no contesta. [speed], [tt_slot] y [tt_port] como en enum_one.
c_dev_open:
        movzx bx, byte [cur_pool]
        mov byte [dv_addr+bx], 0
        mov al, [speed]
        mov [dv_speed+bx], al
        mov ah, 8
        cmp al, 3
        jb .m
        mov ah, 64
.m:
        mov [dv_mps0+bx], ah
        movzx cx, ah
        mov [mps0], cx
        xor cx, cx
        cmp al, 3
        jae .tt
        mov cl, [tt_slot]           ; (en las clásicas, «slot» = dirección)
        mov ch, [tt_port]
.tt:
        mov [dv_tthub+bx], cl
        mov [dv_ttport+bx], ch
        movzx eax, bl
        inc eax
        shl eax, 16
        or eax, 0x0500              ; SET_ADDRESS
        mov [ctl_setup], eax
        mov dword [ctl_setup+4], 0
        call control
        jc .r
        movzx bx, byte [cur_pool]
        mov al, bl
        inc al
        mov [dv_addr+bx], al
        mov [cur_slot], al
        mov cx, 1                   ; (2 ms para que tome la dirección)
        call tick_wait
        clc
.r:
        ret

section .init                   ; (solo al arrancar)
; la tubería BX ya está rellena: si la controladora actual es clásica, abrirla
pipe_ready:
        cmp byte [cur_type], HCT_XHCI
        je .r
        call c_pipe_open
.r:
        ret
section .text

; ---- transferencia de control ----
; petición en ctl_setup (8 bytes); los datos (solo de entrada) van a
; [ctl_buf] de la zona. Aparato: el actual (cur_pool). -> CF si falla
control:
        cmp byte [cs:cur_type], HCT_XHCI
        je x_control
        pushad
        push ds
        push cs
        pop ds
        mov al, [cur_pool]
        call c_xf_dev
        mov byte [xf_ep], 0
        movzx bx, byte [cur_pool]
        movzx ax, byte [dv_mps0+bx]
        mov [xf_mps], ax
        mov word [xr_tmo], 40
        call .go
        pop ds
        popad
        ret
.go:
        HCJMP c_ctl_tab
c_ctl_tab   dw u_control, o_control, e_control

; ---- transferencia bulk de la memoria USB ----
; AL = 1 entrada / 0 salida, EBX = dirección física, CX = bytes -> CF si falla
bulk:
        push ax
        mov al, [cs:bk_pool]
        call dev_hc
        pop ax
        cmp byte [cs:cur_type], HCT_XHCI
        je x_bulk
        pushad
        push ds
        push cs
        pop ds
        mov [bk_dir], al
        mov [xf_buf], ebx
        mov [xf_len], cx
        mov al, [bk_pool]
        call c_xf_dev
        mov word [xr_tmo], 91       ; 5 s: un disco de verdad puede tardar
        mov al, [dci_in]
        mov bx, [mps_in]
        mov ah, [tog_in]
        cmp byte [bk_dir], 0
        jne .d
        mov al, [dci_out]
        mov bx, [mps_out]
        mov ah, [tog_out]
.d:
        shr al, 1
        mov [xf_ep], al
        mov [xf_mps], bx
        mov [xr_tog], ah
        call .go
        pushf
        mov al, [xr_tog]
        cmp byte [bk_dir], 0
        jne .ti
        mov [tog_out], al
        jmp .tk
.ti:
        mov [tog_in], al
.tk:
        popf
        pop ds
        popad
        ret
.go:
        HCJMP c_blk_tab
c_blk_tab   dw u_bulk, o_bulk, e_bulk

; ---- tuberías de interrupción (informes del teclado, el ratón y los mandos) ----
; bloque de la tubería BX -> DI (desplazamiento en CS)
c_pipe_blk:
        mov di, bx
        imul di, PIPE_BLK
        add di, [cl_pipe]
        ret
; búfer del informe de la tubería BX -> EAX = dirección física
c_pipe_buf:
        push bx
        shl bx, 6
        add bx, PIPE_BUF
        call zone
        call phys
        pop bx
        ret

; datos de la tubería BX para las rutinas clásicas (DS = CS)
c_xf_pipe:
        mov al, [ip_pool+bx]
        call c_xf_dev
        mov al, [ip_dci+bx]
        shr al, 1
        mov [xf_ep], al
        mov si, bx
        shl si, 1
        mov ax, [ip_mps+si]
        mov [xf_mps], ax
        mov ax, [ip_len+si]
        mov [xf_len], ax
        ret

section .init                   ; (solo al arrancar)
; abrir la tubería BX (ya rellena: ip_pool, ip_dci, ip_mps, ip_len) en una
; controladora clásica: su cola pasa a la cadena periódica
c_pipe_open:
        pushad
        mov al, [ip_pool+bx]
        call dev_hc
        call c_xf_pipe
        mov byte [ip_tog+bx], 0
        mov byte [ip_cur+bx], 0
        call .go
        popad
        ret
.go:
        HCJMP c_po_tab
c_po_tab    dw u_pipe_open, o_pipe_open, e_pipe_open
section .text

; pedir el siguiente informe de la tubería BX (conserva los registros)
pipe_queue:
        pushad
        push ds
        push cs
        pop ds
        mov al, [ip_pool+bx]
        call dev_hc
        cmp byte [cur_type], HCT_XHCI
        jne .c
        call x_pipe_queue
        jmp .r
.c:
        call c_xf_pipe
        call .go
.r:
        pop ds
        popad
        ret
.go:
        HCJMP c_pq_tab
c_pq_tab    dw u_pipe_queue, o_pipe_queue, e_pipe_queue

; recoger los informes que hayan llegado, de todas las controladoras: cada
; uno se atiende (pipe_done) y se pide el siguiente
usb_poll:
        cmp byte [cs:hc_ours], 0
        je .cl
.ev:
        call get_event
        jc .cl
        cmp al, 32
        jne .ev
        call hid_event
        jmp .ev
.cl:
        pushad
        push ds
        push cs
        pop ds
        xor bx, bx
.l:
        cmp byte [ip_on+bx], 0
        je .n
        mov al, [ip_pool+bx]
        call dev_hc
        cmp byte [cur_type], HCT_XHCI
        je .n
        call .chk
        jc .n                       ; aún no ha llegado
        cmp ax, 0xFFFF
        je .q                       ; error: se vuelve a pedir
        mov [ip_got], ax
        call pipe_done
.q:
        call pipe_queue
.n:
        inc bx
        cmp bx, PIPE_N
        jb .l
        pop ds
        popad
        ret
; -> CF si sigue pendiente; si no, AX = bytes (FFFFh = error)
.chk:
        HCJMP c_pc_tab
c_pc_tab    dw u_pipe_check, o_pipe_check, e_pipe_check

; ha llegado un informe de [ip_got] bytes por la tubería BX
pipe_done:
        cmp bx, 0
        je kbd_report
        cmp bx, 1
        je mouse_report
        push bx
        sub bx, 2
        call joy_report
        pop bx
        ret

; ---- puertos raíz de la controladora actual ----
; CL = puerto (desde 1) -> AL: bit 0 conectado, bit 1 habilitado, bit 7 ha
; cambiado la conexión (AH se pierde). Conserva lo demás.
rp_get:
        push ds
        push cs
        pop ds
        call .go
        pop ds
        ret
.go:
        HCJMP rp_get_tab
rp_get_tab  dw u_rp_get, o_rp_get, e_rp_get, x_rp_get
; dar por visto el cambio de conexión del puerto CL
rp_ack:
        push ds
        push cs
        pop ds
        call .go
        pop ds
        ret
.go:
        HCJMP rp_ack_tab
rp_ack_tab  dw u_rp_ack, o_rp_ack, e_rp_ack, x_rp_ack
; reiniciar el puerto CL y dejarlo habilitado -> [port], [speed]; CF si no
; hay nada o no se habilita (en un EHCI: también cuando el aparato es de
; USB 1.x y se cede a la controladora compañera)
port_enable:
        mov [cs:port], cl
        push cx
        push ds
        push cs
        pop ds
        call .go
        pop ds
        pop cx
        ret
.go:
        HCJMP rp_rst_tab
rp_rst_tab  dw u_rp_reset, o_rp_reset, e_rp_reset, x_rp_reset

; reloj de la controladora actual -> AX = microtramas (14 bits)
hc_time:
        HCJMP hc_time_tab
hc_time_tab dw u_time, o_time, e_time, x_time

; dar de baja el aparato AL de la reserva (xHCI: liberar su slot)
dev_free:
        push bx
        call dev_hc
        cmp byte [cs:cur_type], HCT_XHCI
        jne .r
        movzx bx, al
        mov al, [cs:dv_slot+bx]
        call slot_free
.r:
        pop bx
        ret

; ¿queda algún aparato en uso en la controladora AL? ZF = 1 si ninguno
hc_idle:
        push bx
        xor bx, bx
.l:
        cmp byte [cs:pool_used+bx], 0
        je .n
        cmp al, [cs:dv_hc+bx]
        jne .n
        or bl, 0xFF                 ; ZF = 0
        pop bx
        ret
.n:
        inc bx
        cmp bx, POOL_N
        jb .l
        xor bx, bx                  ; ZF = 1
        pop bx
        ret

; ---- reserva de memoria (tras lo residente; solo durante el inicio) ----
; CX = bytes, DX = alineación (potencia de 2, hasta 4096)
; -> BX = desplazamiento en CS, a cero; CF si no cabe
zalloc:
        push eax
        push ecx
        push edx
        push di
        push es
        cmp byte [cs:z_closed], 0
        jne .no
        movzx ecx, cx
        movzx edx, dx
        dec edx
        mov eax, [cs:ztop]
        movzx ebx, word [cs:mem_off]    ; (la zona empieza en una página física)
        sub eax, ebx
        add eax, edx
        not edx
        and eax, edx
        add eax, ebx
        mov ebx, eax
        add eax, ecx
        cmp eax, 0xFFF0
        ja .no
        cmp eax, [cs:zlimit]
        ja .no
        mov [cs:ztop], eax
        push cs
        pop es
        mov di, bx
        xor al, al
        cld
        rep stosb
        clc
        jmp .r
.no:
        stc
.r:
        pop es
        pop di
        pop edx
        pop ecx
        pop eax
        ret

; una lista de tramas nueva (4 KB en una página, puede quedar más allá de
; los 64 KB de CS) -> EAX = dirección lineal (la física la da lphys); CF si
; no cabe
fl_alloc:
        push ebx
        cmp byte [cs:z_closed], 0
        jne .no
        xor ebx, ebx
        mov bx, cs
        shl ebx, 4
        mov eax, [cs:ztop]
        add eax, ebx
        add eax, 0xFFF
        and eax, 0xFFFFF000
        push eax
        sub eax, ebx
        add eax, 0x1000
        cmp eax, [cs:zlimit]
        ja .no2
        mov [cs:ztop], eax
        pop eax
        clc
        pop ebx
        ret
.no2:
        pop eax
.no:
        stc
        pop ebx
        ret

; [USB-V86] dirección lineal EAX de una lista de tramas -> ES:DI
fl_seg:
        push eax
        shr eax, 4
        mov es, ax
        xor di, di
        pop eax
        ret

; rellenar la lista de tramas de la controladora actual: todas las tramas
; empiezan por su cadena periódica (hc_per)
fl_fill:
        push es
        push di
        push cx
        push eax
        push si
        movzx si, byte [cs:cur_hc]
        shl si, 2
        mov eax, [cs:hc_fl+si]
        test eax, eax
        jz .r
        call fl_seg
        mov eax, [cs:hc_per+si]
        mov cx, 1024
        cld
        rep stosd
.r:
        pop si
        pop eax
        pop cx
        pop di
        pop es
        ret

; pausa de CX tics del reloj (55 ms cada uno; el primero puede ser más corto)
tick_wait:
        sti
        call tmo_start
.l:
        call tmo_check
        jnc .l
        ret
