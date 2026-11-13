; =============================================================================
;  Hispa-DOS - HSPUSB: las controladoras, parte de inicialización
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Va al final de HSPUSB.SYS, con los mensajes: nada de esto se usa después
;  del arranque, y sus últimos 4 KB se aprovechan entonces para el búfer de
;  datos del disco (ver «fin del inicio» en hspusb.asm).
;    pci_survey   qué controladoras hay (tabla hc_*) y si las usa la BIOS
;    mem_setup    la zona de memoria alineada, tras lo residente (res_end)
;    hc_setup_all tomar cada controladora: traspaso desde la BIOS, reinicio
;                 y puesta en marcha (u_setup, o_setup, e_setup; xhci_take)
;    usb_enum     enumerar lo que haya en los puertos raíz de todas
; =============================================================================

sv_type     db 0
x_have      db 0                    ; hay un xHCI que se puede llevar
x_hc        db 0                    ; cuál es en la tabla
any_busy    db 0                    ; la BIOS usa alguna controladora
e_eecp      db 0
e_hcs       dd 0
e_hcc       dd 0
e_cap       dd 0
o_fi        dd 0
hc_bdf      times HC_N dw 0
n_hc        db 0, 0, 0, 0           ; cuántas hay de cada clase

; ---- configuración PCI (BX = bus/dispositivo/función, DI = registro) ----
pci_rw:
        mov ax, 0xB109
        jmp pci_do
pci_rd:
        mov ax, 0xB10A
        jmp pci_do
pci_wb:
        mov ax, 0xB10B
        jmp pci_do
pci_ww:
        mov ax, 0xB10C
        jmp pci_do
pci_wd:
        mov ax, 0xB10D
pci_do:
        push bx
        push di
        push si
        int 0x1A
        pop si
        pop di
        pop bx
        call unreal                 ; (la BIOS puede haber tocado FS)
        ret
; BX = el de la controladora actual
cur_bdf:
        movzx bx, byte [cur_hc]
        shl bx, 1
        mov bx, [hc_bdf+bx]
        ret

; ---- qué controladoras USB hay, y cuáles está usando la BIOS ----
pci_survey:
        mov ax, 0xB101
        xor edi, edi
        int 0x1A
        jc .r
        cmp edx, 0x20494350
        jne .r
        mov ecx, 0x0C0330           ; xHCI primero: así se enumeran después
        mov al, HCT_XHCI            ; (alta velocidad antes que las compañeras)
        call .class
        mov ecx, 0x0C0320
        mov al, HCT_EHCI
        call .class
        mov ecx, 0x0C0300
        mov al, HCT_UHCI
        call .class
        mov ecx, 0x0C0310
        mov al, HCT_OHCI
        call .class
        call unreal
        xor bx, bx
.pl:
        cmp bl, [hc_n]
        jae .pd
        mov al, bl
        call hc_sel
        push bx
        call hc_probe
        pop bx
        mov al, [hc_flags+bx]
        test al, HCF_BAD
        jnz .pn
        and al, HCF_OWNED | HCF_RUN
        or [any_busy], al
.pn:
        inc bx
        jmp .pl
.pd:
        cmp byte [opt_info], 0
        je .r
        mov si, msg_hcs
        call puts
        xor bx, bx
.p:
        mov si, [hc_names+bx]
        call puts
        shr bx, 1
        movzx eax, byte [n_hc+bx]
        shl bx, 1
        call putdec
        add bx, 2
        cmp bx, 8
        jb .p
        call crlf
.r:
        ret
; ECX = clase PCI, AL = clase de controladora: a la tabla
.class:
        mov [sv_type], al
        xor si, si
.cl:
        push ecx
        push si
        mov ax, 0xB103
        int 0x1A
        pop si
        pop ecx
        jc .cd
        test ah, ah
        jnz .cd
        movzx di, byte [sv_type]
        inc byte [n_hc+di]
        movzx di, byte [hc_n]
        cmp di, HC_N
        jae .cn
        mov al, [sv_type]
        mov [hc_type+di], al
        shl di, 1
        mov [hc_bdf+di], bx
        inc byte [hc_n]
.cn:
        inc si
        cmp si, 16
        jb .cl
.cd:
        ret
hc_names    dw msg_hc0, msg_hc1, msg_hc2, msg_hc3
hc_tags     dw msg_t0, msg_t1, msg_t2, msg_t3
msg_hcs     db "Controladores USB:", 0
msg_hc0     db " UHCI ", 0
msg_hc1     db ", OHCI ", 0
msg_hc2     db ", EHCI ", 0
msg_hc3     db ", xHCI ", 0
msg_t0      db "UHCI ", 0
msg_t1      db "OHCI ", 0
msg_t2      db "EHCI ", 0
msg_t3      db "xHCI ", 0

; dirección de los registros de la controladora actual y uso que le da la
; BIOS -> hc_base, hc_flags (suya, en marcha, o no vale)
hc_probe:
        movzx si, byte [cur_hc]
        HCJMP hc_probe_tab
hc_probe_tab dw .uhci, .ohci, .ehci, .xhci
.bad:
        movzx si, byte [cur_hc]
        or byte [hc_flags+si], HCF_BAD
        ret
.setbase:                           ; ECX = base
        push si
        shl si, 2
        mov [hc_base+si], ecx
        pop si
        mov al, [cur_hc]
        call hc_sel
        ret
.uhci:
        call cur_bdf
        mov di, 0x20
        call pci_rd
        and ecx, 0x0000FFFC
        jz .bad
        call .setbase
        mov di, 0x04
        call pci_rw
        test cl, 1                  ; sin acceso a E/S: no la usa nadie
        jz .r
        mov di, 0xC0                ; LEGSUP: la BIOS emula el teclado (SMI)
        call pci_rw
        test cx, 0x001F
        jz .u1
        or byte [hc_flags+si], HCF_OWNED
.u1:
        mov dx, [cur_base]
        in ax, dx
        test al, 1
        jz .r
        or byte [hc_flags+si], HCF_RUN
.r:
        ret
.ohci:
        call cur_bdf
        mov di, 0x10
        call pci_rd
        and ecx, 0xFFFFF000
        jz .bad
        call .setbase
        mov di, 0x04
        call pci_rw
        test cl, 2                  ; sin acceso a memoria: no la usa nadie
        jz .r
        push si
        mov esi, 4
        call hc_rd
        pop si
        test ah, 1                  ; InterruptRouting: la lleva la BIOS (SMM)
        jz .o1
        or byte [hc_flags+si], HCF_OWNED
.o1:
        and al, 0xC0
        cmp al, 0x80                ; operativa
        jne .r
        or byte [hc_flags+si], HCF_RUN
        ret
.ehci:
        call cur_bdf
        mov di, 0x04
        call pci_rw
        push cx
        call e_caps
        pop cx
        jc .bad
        test cl, 2
        jz .r
        push si
        xor esi, esi
        call hc_rd
        pop si
        test al, 1                  ; Run/Stop
        jz .e1
        or byte [hc_flags+si], HCF_RUN
.e1:
        movzx di, byte [e_eecp]
        cmp di, 0x40
        jb .r
        call cur_bdf
        call pci_rd
        cmp cl, 1
        jne .r
        test ecx, 0x10000
        jz .r
        or byte [hc_flags+si], HCF_OWNED
        ret
.xhci:
        cmp byte [x_have], 0        ; solo se lleva un xHCI: el primero
        jne .bad
        call cur_bdf
        mov [bdf], bx
        call xhci_probe
        jc .bad
        mov byte [x_have], 1
        mov al, [cur_hc]
        mov [x_hc], al
        movzx si, al
        mov al, [maxports]
        mov [hc_np+si], al
        mov ecx, [xbase]
        call .setbase
        cmp byte [hc_owned], 0
        je .x1
        or byte [hc_flags+si], HCF_OWNED
.x1:
        cmp byte [hc_running], 0
        je .r
        or byte [hc_flags+si], HCF_RUN
        ret

; EHCI actual: registros de capacidad -> hc_base (operativos), hc_np,
; e_cap, e_hcs, e_hcc, e_eecp; CF si no tiene dirección. Deja activado el
; acceso a memoria.
e_caps:
        push si
        call cur_bdf
        mov di, 0x10
        call pci_rd
        and ecx, 0xFFFFFF00
        jz .no
        mov [e_cap], ecx
        mov di, 0x04
        call pci_rw
        or cx, 0x0006
        call pci_ww
        mov esi, [e_cap]
        call mrd
        movzx ecx, al               ; CAPLENGTH
        add ecx, [e_cap]
        movzx si, byte [cur_hc]
        shl si, 2
        mov [hc_base+si], ecx
        mov [cur_base], ecx
        mov esi, [e_cap]
        add esi, 4
        call mrd
        mov [e_hcs], eax
        and al, 0x0F
        movzx si, byte [cur_hc]
        mov [hc_np+si], al
        mov esi, [e_cap]
        add esi, 8
        call mrd
        mov [e_hcc], eax
        ; la capacidad USBLEGSUP (ID 1) en la lista de capacidades extendidas
        ; del espacio PCI (EECP; casi siempre es la primera, pero puede ir
        ; detrás de otra, p. ej. la del puerto de depuración) -> e_eecp, o 0
        movzx di, ah
        mov byte [e_eecp], 0
        mov dl, 8                   ; (no más de 8 eslabones)
.ec:
        cmp di, 0x40
        jb .ecd
        call cur_bdf
        call pci_rd
        cmp cl, 1
        jne .ecn
        mov ax, di
        mov [e_eecp], al
        jmp .ecd
.ecn:
        movzx di, ch                ; la siguiente
        dec dl
        jnz .ec
.ecd:
        pop si
        clc
        ret
.no:
        pop si
        stc
        ret

; /INFO: una controladora clásica y lo que hace la BIOS con ella
info_hc:
        cmp byte [opt_info], 0
        jne .go
        ret
.go:
        movzx bx, byte [cur_type]
        shl bx, 1
        mov si, [hc_tags+bx]
        call puts
        call cur_bdf
        xor di, di
        call pci_rw
        mov ax, cx
        call puthex4
        mov dl, ':'
        call putc
        mov di, 2
        call pci_rw
        mov ax, cx
        call puthex4
        mov si, msg_ic4
        call puts
        movzx bx, byte [cur_hc]
        mov al, [hc_flags+bx]
        and al, HCF_OWNED
        call info_ctrl.yn
        mov si, msg_ic5
        call puts
        mov al, [hc_flags+bx]
        and al, HCF_RUN
        call info_ctrl.yn
        mov si, msg_ic6
        call puts
        mov al, [boot_usb]
        call info_ctrl.yn
        call crlf
        ret

; /INFO: todas las controladoras
info_all:
        xor bx, bx
.l:
        cmp bl, [hc_n]
        jae .r
        mov al, bl
        call hc_sel
        push bx
        cmp byte [cur_type], HCT_XHCI
        jne .c
        cmp bl, [x_hc]
        jne .n
        cmp byte [x_have], 0
        je .n
        call info_ctrl
        jmp .n
.c:
        call info_hc
.n:
        pop bx
        inc bx
        jmp .l
.r:
        ret

; ---- fin del inicio: reparto final de la memoria ----
; - las UHCI que se han quedado sin aparatos en uso se aparcan;
; - si hay alguna UHCI aparcada hace falta una lista de tramas de reserva
;   (4 KB), y si hay xHCI, una o dos entradas de la reserva con memoria
;   (400h u 800h cada una) para lo que se conecte después: se reservan
;   detrás de lo demás (la inicialización, que no se queda, está por
;   encima, en CS:INIT_ORG).
init_fin:
        pushad
        push ds
        push cs
        pop ds
        xor bx, bx
        mov byte [.parked], 0
.pl:
        cmp bl, [hc_n]
        jae .pd
        test byte [hc_flags+bx], HCF_OURS
        jz .pn
        cmp byte [hc_type+bx], HCT_UHCI
        jne .pn
        mov al, bl
        call hc_sel
        call park_idle
        test byte [hc_flags+bx], HCF_PARK
        jz .pn
        mov byte [.parked], 1
.pn:
        inc bx
        jmp .pl
.pd:
        cmp dword [fl_free], 0
        je .p2
        mov byte [.parked], 0       ; ya hay lista de reserva
.p2:
        ; entradas con memoria que hacen falta (xHCI): una, o dos sin disco
        mov byte [.spare], 0
        cmp byte [hc_ours], 0
        je .s2
        mov al, 2
        sub al, [disk_found]
        xor bx, bx
.sl:
        cmp byte [pool_used+bx], 0
        jne .sn
        mov si, bx
        shl si, 1
        cmp word [pool_mem+si], 0xFFFF
        je .sn
        dec al                      ; una libre que ya la tiene
        jz .s2
.sn:
        inc bx
        cmp bx, POOL_N
        jb .sl
        mov [.spare], al
.s2:
        mov al, [.parked]
        or al, [.spare]
        jz .end
.plain:
        cmp byte [.parked], 0
        je .ps
        call fl_alloc
        jc .ps
        mov [fl_free], eax
.ps:
        cmp byte [.spare], 0
        je .end
        mov cx, [ctx_sz]
        shl cx, 5
        mov dx, cx
        call zalloc
        jc .end
        mov ax, bx
        sub ax, [mem_off]
        call .give
        dec byte [.spare]
        jmp .ps
.end:
        mov byte [z_closed], 1
        pop ds
        popad
        ret
; AX = memoria (en la zona) para la primera entrada libre que no la tenga
.give:
        xor bx, bx
.gl:
        cmp byte [pool_used+bx], 0
        jne .gn
        mov si, bx
        shl si, 1
        cmp word [pool_mem+si], 0xFFFF
        jne .gn
        mov [pool_mem+si], ax
        ret
.gn:
        inc bx
        cmp bx, POOL_N
        jb .gl
        ret
.parked     db 0
.spare      db 0

; ---- la zona de memoria (tras lo residente, en una página física) ----
; -> CF si no hay memoria
mem_setup:
        ; hasta dónde llega la memoria: lo dice el núcleo en la petición
        push ds
        lds bx, [cs:req]
        movzx eax, word [bx+0x10]
        shl eax, 4
        movzx edx, word [bx+0x0E]
        add eax, edx
        pop ds
        xor ebx, ebx
        mov bx, cs
        shl ebx, 4
        sub eax, ebx
        jbe .nolim
        cmp eax, res_end
        jbe .nolim
        mov [zlimit], eax
.nolim:
        ; la inicialización está en CS:INIT_ORG mientras tanto: la zona,
        ; por debajo
        cmp dword [zlimit], INIT_ORG
        jbe .lim
        mov dword [zlimit], INIT_ORG
.lim:
        mov eax, ebx
        add eax, res_end + 4095
        and eax, 0xFFFFF000
        sub eax, ebx
        mov [ztop], eax
        ; con alguna UHCI: una lista de tramas ya (la de la primera que
        ; tenga algo conectado, o la de reserva)
        cmp byte [n_hc+HCT_UHCI], 0
        je .nfl
        call fl_alloc
        jc .no
        mov [fl_free], eax
.nfl:
        mov eax, [ztop]
        mov [mem_off], ax
        mov ebx, COM_END
        cmp byte [x_have], 0
        je .sz
        mov ebx, X_END
.sz:
        add eax, ebx
        cmp eax, 0xFFF0
        ja .no
        cmp eax, [zlimit]
        ja .no
        mov [ztop], eax
        mov di, [mem_off]
        mov cx, bx
        xor al, al
        push es
        push cs
        pop es
        cld
        rep stosb
        pop es
        ; clásicas: la reserva de TD y las colas de las tuberías
        mov al, [n_hc+HCT_UHCI]
        or al, [n_hc+HCT_OHCI]
        or al, [n_hc+HCT_EHCI]
        jz .ok
        mov cx, XTD_BYTES
        mov dx, 64
        call zalloc
        jc .no
        mov [cl_xtd], bx
        mov cx, PIPE_N * PIPE_BLK
        mov dx, 32
        call zalloc
        jc .no
        mov [cl_pipe], bx
.ok:
        clc
        ret
.no:
        stc
        ret

; ---- tomar todas las controladoras ----
; -> CF si no se ha podido con ninguna
hc_setup_all:
        xor bx, bx
.l:
        cmp bl, [hc_n]
        jae .end
        test byte [hc_flags+bx], HCF_BAD
        jnz .n
        mov al, bl
        call hc_sel
        push bx
        call .one
        pop bx
        jc .n
        or byte [hc_flags+bx], HCF_OURS
.n:
        inc bx
        jmp .l
.end:
        xor bx, bx
.c:
        cmp bl, [hc_n]
        jae .none
        test byte [hc_flags+bx], HCF_OURS
        jnz .some
        inc bx
        jmp .c
.some:
        clc
        ret
.none:
        stc
        ret
.one:
        HCJMP hc_setup_tab
hc_setup_tab dw u_setup, o_setup, e_setup, xhci_take

; UHCI: la BIOS deja de usarla, reinicio, y aparcada hasta que haga falta
u_setup:
        call cur_bdf
        mov di, 0x04
        call pci_rw
        or cx, 0x0005               ; E/S y acceso al bus
        call pci_ww
        mov di, 0xC0                ; LEGSUP: sin emulación del teclado ni SMI
        mov cx, 0x8F00
        call pci_ww
        mov dx, [cur_base]
        mov ax, 0x0004              ; reinicio global: 10 ms al menos
        out dx, ax
        mov cx, 2
        call tick_wait
        mov dx, [cur_base]
        xor ax, ax
        out dx, ax
        mov cx, 1000
.d:
        in ax, dx
        loop .d
        mov ax, 0x0002              ; reinicio de la controladora
        out dx, ax
        mov cx, 0xFFFF
.hr:
        in ax, dx
        test al, 2
        jz .hd
        loop .hr
.hd:
        add dx, 4
        xor ax, ax
        out dx, ax                  ; USBINTR: sin interrupciones
        ; puertos: los registros con el bit 7 a uno (2 casi siempre)
        xor cl, cl
.np:
        inc cl
        call u_psc
        in ax, dx
        cmp ax, 0xFFFF
        je .npd
        test al, 0x80
        jz .npd
        cmp cl, 8
        jb .np
        inc cl
.npd:
        dec cl
        jz .bad
        movzx si, byte [cur_hc]
        mov [hc_np+si], cl
        mov cx, 16                  ; su cola de control y bulk
        mov dx, 16
        call zalloc
        jc .bad
        mov [cur_hmem], bx
        shl si, 1
        mov [hc_mem+si], bx
        shr si, 1
        or byte [hc_flags+si], HCF_PARK
        clc
        ret
.bad:
        stc
        ret

; OHCI: traspaso desde la BIOS (SMM), reinicio y en marcha
o_setup:
        call cur_bdf
        mov di, 0x04
        call pci_rw
        or cx, 0x0006               ; memoria y acceso al bus
        call pci_ww
        mov esi, 4
        call hc_rd
        test ah, 1                  ; InterruptRouting: es de la BIOS
        jz .own
        mov esi, 0x10
        mov eax, 1 << 30            ; (que le llegue el aviso de cambio)
        call hc_wr
        mov esi, 0x08
        mov eax, 8                  ; OwnershipChangeRequest
        call hc_wr
        mov cx, 20                  ; hasta 1 s
        call tmo_start
.ow:
        mov esi, 4
        call hc_rd
        test ah, 1
        jz .own
        call tmo_check
        jnc .ow
.own:
        mov esi, 0x14
        mov eax, 0xC000007F         ; sin interrupciones
        call hc_wr
        mov esi, 0x34
        call hc_rd
        and eax, 0x3FFF
        jnz .fi
        mov eax, 0x2EDF
.fi:
        mov [o_fi], eax
        mov esi, 4
        xor eax, eax                ; estado de reinicio: reinicia el bus
        call hc_wr
        mov cx, 2
        call tick_wait
        mov cx, 0x120               ; HCCA y los ED de control y bulk
        mov dx, 0x100
        call zalloc
        jc .bad
        mov [cur_hmem], bx
        movzx si, byte [cur_hc]
        shl si, 1
        mov [hc_mem+si], bx
        mov dword [bx+0x100], 1 << 14   ; parados (sKip)
        mov dword [bx+0x110], 1 << 14
        mov esi, 0x08
        mov eax, 1                  ; reinicio de la controladora
        call hc_wr
        mov cx, 5000
.hr:
        call mrd
        test al, 1
        jz .hd
        loop .hr
.hd:
        ; de suspendida a operativa, sin esperas (hay 2 ms de margen)
        mov bx, [cur_hmem]
        call phys
        mov esi, 0x18
        call hc_wr                  ; HcHCCA
        add eax, 0x100
        mov esi, 0x20
        call hc_wr                  ; HcControlHeadED
        add eax, 0x10
        mov esi, 0x28
        call hc_wr                  ; HcBulkHeadED
        mov eax, [o_fi]             ; HcFmInterval: mayor paquete = (FI - 210) * 6 / 7
        sub eax, 210
        mov edx, 6
        mul edx
        mov ecx, 7
        div ecx
        shl eax, 16
        or eax, [o_fi]
        or eax, 0x80000000
        mov esi, 0x34
        call hc_wr
        mov eax, [o_fi]             ; HcPeriodicStart: el 90 %
        mov edx, 9
        mul edx
        mov ecx, 10
        div ecx
        mov esi, 0x40
        call hc_wr
        mov esi, 4
        mov eax, 0xB7               ; operativa, las tres listas, control 4:1
        call hc_wr
        mov esi, 0x50
        mov eax, 0x10000            ; corriente a los puertos
        call hc_wr
        mov esi, 0x48
        call hc_rd
        cmp al, 15
        jbe .np
        mov al, 15
.np:
        movzx si, byte [cur_hc]
        mov [hc_np+si], al
        mov ch, al                  ; (y puerto a puerto, si se controla así)
        xor cl, cl
.pp:
        inc cl
        cmp cl, ch
        ja .ppd
        call o_psc
        mov eax, 0x100
        call hc_wr
        jmp .pp
.ppd:
        clc
        ret
.bad:
        stc
        ret

; EHCI: traspaso desde la BIOS (USBLEGSUP), reinicio, lista asíncrona en
; marcha y puertos para el EHCI (CONFIGFLAG)
e_setup:
        call e_caps
        jc .bad
        movzx di, byte [e_eecp]
        cmp di, 0x40
        jb .own
        call cur_bdf
        call pci_rd
        cmp cl, 1
        jne .own
        ; propiedad del sistema, siempre: hay BIOS (la de los Mac) que usan
        ; el EHCI con sus SMI sin decir que es suyo
        push ecx
        add di, 3
        mov cl, 1
        call pci_wb
        pop ecx
        test ecx, 0x10000           ; es de la BIOS: esperar a que lo suelte
        jz .smi
        mov cx, 20                  ; hasta 1 s
        call tmo_start
.ow:
        movzx di, byte [e_eecp]
        call pci_rd
        test ecx, 0x10000
        jz .smi
        call tmo_check
        jnc .ow
.smi:
        movzx di, byte [e_eecp]
        add di, 4
        mov ecx, 0xE0000000         ; sin SMI (y sus avisos pendientes, borrados)
        call pci_wd
.own:
        call unreal
        xor esi, esi
        call hc_rd
        and al, 0xFE                ; parar
        call mwr
        mov cx, 3
        call tmo_start
.h:
        mov esi, 4
        call hc_rd
        test ah, 0x10               ; HCHalted
        jnz .hd
        call tmo_check
        jnc .h
.hd:
        xor esi, esi
        mov eax, 2                  ; reinicio
        call hc_wr
        mov cx, 10
        call tmo_start
.rs:
        call mrd
        test al, 2
        jz .rd
        call tmo_check
        jnc .rs
        jmp .bad
.rd:
        mov esi, 0x08
        xor eax, eax
        call hc_wr                  ; USBINTR
        test byte [e_hcc], 1
        jz .n64
        mov esi, 0x10
        xor eax, eax
        call hc_wr                  ; CTRLDSSEGMENT
.n64:
        mov cx, 0xC0                ; cabecera y cola de trabajo
        mov dx, 0x20
        call zalloc
        jc .bad
        mov [cur_hmem], bx
        movzx si, byte [cur_hc]
        shl si, 1
        mov [hc_mem+si], bx
        mov di, bx
        call phys
        mov edx, eax                ; cabecera
        add eax, 0x60
        or al, 2
        mov [di], eax
        mov dword [di+4], (1 << 15) | (2 << 12) | (64 << 16)
        mov dword [di+8], 1 << 30
        mov dword [di+16], 1
        mov dword [di+20], 1
        mov dword [di+24], 0x40     ; parada: nunca trabaja
        mov eax, edx
        or al, 2
        mov [di+0x60], eax
        mov dword [di+0x64], (2 << 12) | (64 << 16)
        mov dword [di+0x68], 1 << 30
        mov dword [di+0x70], 1
        mov dword [di+0x74], 1
        mov eax, edx
        mov esi, 0x18
        call hc_wr                  ; ASYNCLISTADDR
        xor esi, esi
        mov eax, 0x00080021         ; en marcha, con la lista asíncrona
        call hc_wr
        mov esi, 0x40
        mov eax, 1                  ; CONFIGFLAG: los puertos, para el EHCI
        call hc_wr
        test byte [e_hcs], 0x10     ; corriente a los puertos, si se controla
        jz .ok
        movzx si, byte [cur_hc]
        mov ch, [hc_np+si]
        xor cl, cl
.pp:
        inc cl
        cmp cl, ch
        ja .ok
        call e_psc
        call hc_rd
        and eax, ~0x2A
        or ah, 0x10
        call mwr
        jmp .pp
.ok:
        clc
        ret
.bad:
        stc
        ret

; parar todas las controladoras tomadas (el driver no se queda)
hc_stop_all:
        call intel_unroute
        xor bx, bx
.l:
        cmp bl, [hc_n]
        jae .r
        test byte [hc_flags+bx], HCF_OURS
        jz .n
        mov al, bl
        call hc_sel
        push bx
        call .one
        pop bx
.n:
        inc bx
        jmp .l
.r:
        ret
.one:
        HCJMP hc_stop_tab
hc_stop_tab dw .uhci, .ohci, .ehci, xhci_stop
.uhci:
        mov dx, [cur_base]
        xor ax, ax
        out dx, ax
        ret
.ohci:
        mov esi, 4
        xor eax, eax
        call hc_wr
        ret
.ehci:
        xor esi, esi
        xor eax, eax
        call hc_wr
        ret

; ---- puertos: enumerar todo lo conectado, en todas las controladoras ----
; ¿hay algo conectado en algún puerto raíz? ZF = 1 si no
any_conn:
        xor bx, bx
.l:
        cmp bl, [hc_n]
        jae .none
        test byte [hc_flags+bx], HCF_OURS
        jz .n
        mov al, bl
        call hc_sel
        xor cl, cl
.p:
        inc cl
        cmp cl, [hc_np+bx]
        ja .n
        call rp_get
        test al, 1
        jz .p
        ret                         ; ZF = 0
.n:
        inc bx
        jmp .l
.none:
        xor bx, bx
        ret

usb_enum:
        mov word [fail_msg], msg_nodev
        ; dar tiempo a que los puertos se estabilicen
        mov cx, 4
        call tick_wait
        ; si aún no hay nada conectado, hasta un segundo más: tras reiniciar
        ; la controladora los puertos vuelven a dar corriente y una memoria
        ; de verdad tarda en presentarse (más aún si cambia de controladora)
        mov cx, 18
        call tmo_start
.wc:
        call any_conn
        jz .wn
        mov cx, 3                   ; algo hay: un poco más para los demás
        call tick_wait
        jmp .wgo
.wn:
        call tmo_check
        jnc .wc
.wgo:
        call enum_ports
        ; /U: la memoria de arranque tiene que aparecer. Puede tardar (vuelve
        ; a conectarse tras reiniciar la controladora, o cambia de una a
        ; otra): se la espera hasta unos 4 s mirando los puertos nuevos.
        cmp byte [boot_saved], 0
        je .end
        mov cx, 8
.bw:
        cmp byte [boot_match], 0
        jne .end
        push cx
        mov cx, 9
        call tick_wait
        call enum_ports
        pop cx
        loop .bw
.end:
        ; los receptores de Bluetooth que se hacen pasar por teclado o ratón
        ; (HID proxy), solo si no ha aparecido uno de verdad
        call proxy_retry
        mov al, [disk_found]
        or al, [k_on]
        or al, [m_on]
        or al, [j_on]
        or al, [j_on+1]
        jz .none
        clc
        ret
.none:
        stc
        ret

; enumerar los puertos raíz con algo conectado que no se hayan visto ya
; (port_done), controladora por controladora en el orden de la tabla: xHCI,
; EHCI (que cede a sus compañeras lo que no sea de alta velocidad) y, tras
; una pausa para que lo cedido aparezca en ellas, UHCI y OHCI
enum_ports:
        xor bx, bx
.hl:
        cmp bl, [hc_n]
        jae .end
        test byte [hc_flags+bx], HCF_OURS
        jz .hn
        mov al, bl
        call hc_sel
        cmp byte [hc_type+bx], HCT_EHCI
        jae .go
        cmp byte [e_handed], 0
        je .park
        mov byte [e_handed], 0
        mov cx, 4
        call tick_wait
.park:
        ; una UHCI aparcada solo se arranca si tiene algo conectado
        test byte [hc_flags+bx], HCF_PARK
        jz .go
        push bx
        call u_any
        pop bx
        jz .hn
        call u_start
        jc .hn
.go:
        push bx
        call info_ports
        pop bx
        xor cl, cl
.pl:
        inc cl
        cmp cl, [hc_np+bx]
        ja .hn
        call pd_index
        bt [port_done], di
        jc .pl
        call rp_get
        test al, 1
        jz .pl
        push bx
        push cx
        call port_enable
        pop cx
        pop bx
        jc .pl
        call pd_index
        bts [port_done], di
        push bx
        push cx
        call root_dev
        call enum_one
        pop cx
        pop bx
        mov al, bl
        call hc_sel
        jmp .pl
.hn:
        inc bx
        jmp .hl
.end:
        ret
; controladora BX, puerto CL -> DI = su bit en port_done (xHCI: hasta 255
; puertos; las demás, 16 cada una)
pd_index:
        movzx di, cl
        cmp byte [hc_type+bx], HCT_XHCI
        je .r
        and di, 15
        push bx
        shl bx, 4
        lea di, [bx+di+256]
        pop bx
.r:
        ret
port_done   times 32 + HC_N * 2 db 0

; /INFO: puertos de la controladora actual con algo conectado
info_ports:
        cmp byte [opt_info], 0
        jne .go
        ret
.go:
        pushad
        movzx bx, byte [cur_type]
        shl bx, 1
        mov si, [hc_tags+bx]
        call puts
        mov si, msg_iports
        call puts
        movzx bx, byte [cur_hc]
        xor cl, cl
.l:
        inc cl
        cmp cl, [hc_np+bx]
        ja .d
        call rp_get
        test al, 1
        jz .l
        mov dl, ' '
        call putc
        movzx eax, cl
        call putdec
        jmp .l
.d:
        mov si, msg_iports2
        call puts
        movzx eax, byte [hc_np+bx]
        call putdec
        mov si, msg_iports3
        call puts
        popad
        ret

msg_usage   db "DEVICE=HSPUSB.SYS [/F] [/U] [/INFO] [/V]", 13, 10
            db "  Lleva el USB sin la BIOS, con las cuatro clases de controladora (UHCI y", 13, 10
            db "  OHCI de USB 1.x, EHCI de USB 2.0 y xHCI de USB 3): memorias USB como", 13, 10
            db "  unidades de DOS, teclado, ratón, mandos de juego y concentradores.", 13, 10
            db "  (sin opciones) si la BIOS está usando el USB no se toca nada", 13, 10
            db "  /F     tomar el USB aunque lo use la BIOS (teclado y ratón pasan a HSPUSB)", 13, 10
            db "  /U     como /F, también si el sistema ha arrancado de una memoria USB", 13, 10
            db "  /INFO  enseñar las controladoras y el árbol de aparatos", 13, 10
            db "  /V     mensajes detallados", 13, 10
            db "  /?     esta ayuda (el driver no se carga)", 13, 10, 0
msg_nohc    db "No se ha encontrado ninguna controladora USB.", 13, 10, 0
msg_notake  db "No se ha podido tomar ninguna controladora USB.", 13, 10, 0
