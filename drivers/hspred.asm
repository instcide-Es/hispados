; =============================================================================
;  HSPRED.COM - controlador de red de Hispa-DOS (residente, «packet driver»)
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Uso:  HSPRED [/INT:xx] [/INFO] [/U] [/?]      (en AUTOEXEC.BAT; vale LH)
;        /INT:xx  vector del packet driver, en hexadecimal (60 a 80); si no
;                 se dice, el primero libre desde el 60h (el 67h, el de la
;                 memoria expandida, nunca)
;        /INFO    tarjeta, MAC, enlace, IRQ, vector, contadores y la
;                 configuración IP; también con el controlador ya cargado
;        /U       descargarlo de la memoria (si ningún programa lo usa)
;
;  Interfaz: la especificación pública de «packet drivers» de FTP Software
;  (versión 1.09), la que usan mTCP, WATTCP, Arachne y muchos más. El vector
;  apunta a pkt_entry, que lleva la firma «PKT DRVR» 3 octetos después.
;  Funciones (AH): 01h driver_info, 02h access_type, 03h release_type,
;  04h send_pkt, 05h terminate, 06h get_address, 07h reset_interface,
;  14h set_rcv_mode (1, 2 y 3), 15h get_rcv_mode, 18h get_statistics; las
;  demás de la ampliada contestan con su error (sin multidifusión, no se
;  cambia la dirección). Errores: CF = 1 y DH = código.
;  Los paquetes que llegan se entregan como dice la especificación: primera
;  llamada al receptor con AX = 0 (BX = manejador, CX = longitud: devuelve
;  ES:DI, o 0:0 si no lo quiere), copia, y segunda con AX = 1 y DS:SI = el
;  búfer. Las llamadas se hacen desde la interrupción de la tarjeta, con la
;  pila propia de HSPRED (PILA octetos) y las interrupciones desactivadas.
;
;  Tarjetas (PCI; se usa la primera que se encuentre, en este orden):
;   - NE2000 PCI: Realtek RTL8029 y compatibles (la de QEMU, -device
;     ne2k_pci). Datos en la memoria de la tarjeta, por «DMA remoto»: sin
;     búferes en el PC (es la que menos ocupa).
;   - Realtek RTL8139 (-device rtl8139): anillo de recepción de 8 KB.
;   - Intel PRO/1000 de la familia 8254x («e1000», -device e1000): anillos
;     de descriptores. Registros por los puertos (IOADDR/IODATA) si la
;     tarjeta los tiene (las de verdad) o, si no, en memoria con el modo
;     «irreal» (como HD Audio en HSPSND; no en modo V86: ver docs/RED.md).
;   - Realtek RTL8111/8168/8101/8169 («r8169»): preparada a partir de la
;     documentación pública de la familia, SIN PROBAR (QEMU no la tiene);
;     es la del Lenovo A275 de las pruebas. Ver docs/RED.md.
;  Las tres con «bus master» necesitan búferes en memoria física continua:
;  van detrás de lo residente o, si HSPRED está en la memoria superior (LH),
;  en un bloque aparte de la memoria convencional (en modo V86 la superior
;  no es memoria física continua). En modo V86 la dirección física se pide
;  por VCPI (DE06h).
;
;  Para los programas de Hispa-DOS (CONFRED, PING, DESCARGAR, lib/red.inc):
;  INT 2Fh AH=DAh
;    AL=00h  -> AL = FFh si está, BX = 5248h («HR»), CL = vector del packet
;               driver, ES:DI = configuración IP compartida (CFG_*: la
;               guarda CONFRED o el primer programa que la consigue, por
;               DHCP o de RED.INI, y la leen los demás)
;    AL=01h  -> AL = enlace (0 no, 1 sí, FFh no se sabe), AH = velocidad
;               (0 no se sabe, 1 = 10, 2 = 100, 3 = 1000 Mbit/s)
;    AL=02h  descargar (BL = 1: aunque haya programas usándolo) -> AL = 0
;               hecho, 1 otro programa tiene los vectores, 2 en uso
;    AL=03h  -> ES:BX = datos del controlador (para /INFO)
; =============================================================================
        cpu 386
        bits 16
        org 0x100

        section .text
        section .init follows=.text
        section .text

NH          equ 8                   ; manejadores (tipos de paquete)
PILA        equ 512                 ; pila de la interrupción
VERSION     equ 1

; tarjetas
C_NE        equ 1
C_8139      equ 2
C_E1000     equ 3
C_8168      equ 4

; configuración IP compartida (cfg, 30 octetos)
CFG_FLAGS   equ 0                   ; bit 0 válida, bit 1 por DHCP
CFG_IP      equ 2
CFG_MASK    equ 6
CFG_GW      equ 10
CFG_DNS     equ 14
CFG_SRV     equ 18                  ; servidor DHCP
CFG_LEASE   equ 22                  ; concesión (segundos)
CFG_DATE    equ 26                  ; fecha en que se obtuvo (día, mes, año)
CFG_LEN     equ 30

; ---- búferes de DMA (desplazamientos dentro de dma_seg) ----
; RTL8139: anillo de recepción de 8 KB + 16 y un búfer de envío
R39_RX      equ 0
R39_TX      equ 8192 + 16
R39_SIZE    equ R39_TX + 1536
; e1000: 8 descriptores de cada (el anillo tiene que ser de 128 octetos o
; más) y 4 búferes de recepción: el descriptor i usa el búfer i & 3 y la
; tarjeta solo tiene 3 a la vez (ver e1_drain)
E1_RXD      equ 0
E1_TXD      equ 128
E1_RXB      equ 256
E1_TXB      equ E1_RXB + 4 * 2048
E1_SIZE     equ E1_TXB + 1536
; RTL8168: anillos alineados a 256 octetos; 4 de recepción y 2 de envío
R68_RXD     equ 0
R68_TXD     equ 256
R68_RXB     equ 512
R68_TXB     equ R68_RXB + 4 * 1536
R68_SIZE    equ R68_TXB + 1536
R68_BUF     equ 1536

start:
        jmp init

; =============================================================================
;  Entrada del packet driver (el vector apunta aquí; la firma, 3 más allá)
; =============================================================================
pkt_entry:
        jmp near pkt_main           ; 3 octetos
        db "PKT DRVR", 0

; =============================================================================
;  Datos residentes. De «info» a «info_end» es lo que /INFO copia del
;  residente (mismo programa: mismos desplazamientos).
; =============================================================================
info:
card        db 0
irq         db 0
pkt_vec     db 0
irq_vec     db 0
pic_was     db 0                    ; 1: el IRQ estaba enmascarado al cargar
e_io        db 0                    ; e1000: por los puertos (IOADDR/IODATA)
iob         dw 0                    ; puertos de la tarjeta
mmio        dd 0                    ; e1000: registros en memoria
pci_bdf     dw 0
pci_ven     dw 0
pci_dev     dw 0
mac         times 6 db 0
name_ptr    dw 0
rx_mode     dw 3
dma_seg     dw 0
dma_own     dw 0                    ; bloque aparte (cargado con LH) o 0
dma_phys    dd 0                    ; dirección física de dma_seg:0
res_paras   dw 0                    ; tamaño residente (párrafos, con el PSP)
dma_bytes   dw 0                    ; tamaño de los búferes de DMA
stats:
st_pin      dd 0
st_pout     dd 0
st_bin      dd 0
st_bout     dd 0
st_ein      dd 0
st_eout     dd 0
st_drop     dd 0
handles     times NH * 16 db 0      ; +0 en uso, +1 long. del tipo, +2 tipo
                                    ; (8), +10 receptor (seg:desp)
cfg         times CFG_LEN db 0
info_end:
INFO_LEN    equ info_end - info

old2f       dd 0
oldirq      dd 0
oldpkt      dd 0
c_send      dw 0                    ; rutinas de la tarjeta
c_isr       dw 0
c_stop      dw 0
c_link      dw 0
c_start     dw 0
rx_i        db 0                    ; siguiente descriptor de recepción
tx_i        db 0                    ; siguiente descriptor de envío
r39_off     dw 0                    ; RTL8139: posición en el anillo
ne_next     db 0                    ; NE2000: siguiente página que leer
rx_len      dw 0
rx_h        dw 0
rx_rcv      dd 0
rx_buf      dd 0
rx_tmp      times 18 db 0           ; cabecera del paquete que llega
sv_sp       dw 0
sv_ss       dw 0
gdtr        dw 15
            dd 0
gdt_tbl     dq 0
            dq 0x008F92000000FFFF   ; datos, base 0, límite 4 GB

name_ne     db "NE2000 PCI", 0
name_8139   db "Realtek RTL8139", 0
name_e1000  db "Intel PRO/1000 (e1000)", 0
name_8168   db "Realtek RTL8168/8111 (sin probar)", 0

; =============================================================================
;  Funciones del packet driver
; =============================================================================
pkt_main:
        push bp
        mov bp, sp                  ; [bp+2] IP, [bp+4] CS, [bp+6] indicadores
        push ds                     ; [bp-2] = DS del programa
        push cs
        pop ds
        cld
        cmp ah, 1
        je f_info
        cmp ah, 2
        je f_access
        cmp ah, 3
        je f_release
        cmp ah, 4
        je f_send
        cmp ah, 5
        je f_term
        cmp ah, 6
        je f_addr
        cmp ah, 7
        je f_reset
        cmp ah, 20
        je f_setmode
        cmp ah, 21
        je f_getmode
        cmp ah, 22
        je f_nomulti
        cmp ah, 23
        je f_nomulti
        cmp ah, 24
        je f_stats
        cmp ah, 25
        je f_setaddr
        mov dh, 11                  ; BAD_COMMAND
p_err:
        or byte [bp+6], 1
        pop ds
        pop bp
        iret
p_okds:
        mov [bp-2], cs              ; vuelve con DS = el nuestro (DS:SI)
p_ok:
        and byte [bp+6], 0xFE
        pop ds
        pop bp
        iret

f_info:
        mov bx, VERSION
        mov cx, 0x0100              ; clase 1 (Ethernet DIX), número 0
        mov dx, 1                   ; tipo
        mov si, [name_ptr]
        mov al, 2                   ; básicas y ampliadas
        jmp p_okds

; AL = clase, BX = tipo de interfaz, DL = número, DS:SI = tipo de paquete,
; CX = su longitud, ES:DI = receptor -> AX = manejador
f_access:
        mov dh, 2                   ; NO_CLASS
        cmp al, 1
        jne p_err
        mov dh, 3                   ; NO_TYPE
        cmp bx, 0xFFFF
        je .t
        cmp bx, 1
        jne p_err
.t:
        mov dh, 4                   ; NO_NUMBER
        test dl, dl
        jnz p_err
        mov dh, 5                   ; BAD_TYPE
        cmp cx, 8
        ja p_err
        push bx
        push cx
        push si
        push di
        push es
        ; ¿ya lo tiene otro? (mismo tipo y misma longitud)
        mov bx, handles
.u:
        cmp byte [bx], 0
        je .un
        cmp [bx+1], cl
        jne .un
        push cx
        push si
        push di
        push es
        lea di, [bx+2]
        push ds
        pop es                      ; ES:DI = el de la tabla
        mov ds, [bp-2]              ; DS:SI = el que piden
        repe cmpsb
        push cs
        pop ds
        pop es
        pop di
        pop si
        pop cx
        je .inuse
.un:
        add bx, 16
        cmp bx, handles + NH * 16
        jb .u
        ; uno libre
        mov bx, handles
.f:
        cmp byte [bx], 0
        je .got
        add bx, 16
        cmp bx, handles + NH * 16
        jb .f
        mov dh, 9                   ; NO_SPACE
        jmp .fail
.inuse:
        mov dh, 10                  ; TYPE_INUSE
.fail:
        pop es
        pop di
        pop si
        pop cx
        pop bx
        jmp p_err
.got:
        mov [bx+10], di             ; el receptor
        mov [bx+12], es
        mov [bx+1], cl
        lea di, [bx+2]
        push ds
        pop es
        mov ds, [bp-2]
        rep movsb
        push cs
        pop ds
        mov byte [bx], 1
        mov ax, bx
        sub ax, handles
        shr ax, 4
        inc ax                      ; manejador = 1..NH
        call irq_unmask
        pop es
        pop di
        pop si
        pop cx
        pop bx
        jmp p_ok

; BX = manejador -> BX = su entrada; CF (y DH = BAD_HANDLE) si no vale
chk_handle:
        mov dh, 1
        dec bx
        cmp bx, NH
        jae .bad
        shl bx, 4
        add bx, handles
        cmp byte [bx], 0
        je .bad
        clc
        ret
.bad:
        stc
        ret

f_release:
        push bx
        call chk_handle
        jc .e
        mov byte [bx], 0
        pop bx
        jmp p_ok
.e:
        pop bx
        jmp p_err

; DS:SI = paquete, CX = longitud
f_send:
        mov dh, 12                  ; CANT_SEND
        cmp cx, 14
        jb p_err
        cmp cx, 1514
        ja p_err
        call irq_unmask
        pushad
        push es
        mov ds, [bp-2]
        call [cs:c_send]
        push cs
        pop ds
        pop es
        popad
        jc .e
        inc dword [st_pout]
        movzx edx, cx
        add [st_bout], edx
        xor dx, dx
        jmp p_ok
.e:
        inc dword [st_eout]
        mov dh, 12
        jmp p_err

f_term:
        push bx
        call chk_handle
        pop bx
        jc p_err
        push ax
        push bx
        mov bl, 1
        call unload
        test al, al
        pop bx
        pop ax
        mov dh, 7                   ; CANT_TERMINATE
        jnz p_err
        jmp p_ok

; ES:DI = búfer, CX = su tamaño -> CX = 6
f_addr:
        push bx
        call chk_handle
        pop bx
        jc p_err
        mov dh, 9                   ; NO_SPACE
        cmp cx, 6
        jb p_err
        push si
        push di
        mov si, mac
        mov cx, 6
        rep movsb
        pop di
        pop si
        mov cx, 6
        jmp p_ok

f_reset:
        push bx
        call chk_handle
        pop bx
        jc p_err
        jmp p_ok

; CX = modo: 1 nada, 2 solo los dirigidos a esta tarjeta, 3 y los de
; difusión. Los demás (multidifusión, todos) no.
f_setmode:
        push bx
        call chk_handle
        pop bx
        jc p_err
        mov dh, 8                   ; BAD_MODE
        cmp cx, 1
        jb p_err
        cmp cx, 3
        ja p_err
        mov [rx_mode], cx
        jmp p_ok

f_getmode:
        push bx
        call chk_handle
        pop bx
        jc p_err
        mov ax, [rx_mode]
        jmp p_ok

f_nomulti:
        mov dh, 6                   ; NO_MULTICAST
        jmp p_err

f_stats:
        push bx
        call chk_handle
        pop bx
        jc p_err
        mov si, stats
        jmp p_okds

f_setaddr:
        mov dh, 13                  ; CANT_SET
        jmp p_err

; ---- el IRQ, sin máscara (hay BIOS que lo enmascaran si les llega uno que
; no esperan; se comprueba cada vez que se envía) ----
irq_unmask:
        push ax
        push cx
        pushf
        cli
        mov cl, [cs:irq]
        mov ah, 1
        cmp cl, 8
        jae .hi
        shl ah, cl
        in al, 0x21
        not ah
        and al, ah
        out 0x21, al
        jmp .r
.hi:
        sub cl, 8
        shl ah, cl
        in al, 0xA1
        not ah
        and al, ah
        out 0xA1, al
        in al, 0x21
        and al, 0xFB                ; la cascada (IRQ 2)
        out 0x21, al
.r:
        popf
        pop cx
        pop ax
        ret

; =============================================================================
;  Descargar (/U o terminate). BL = 1: aunque haya manejadores en uso.
;  DS = CS. -> AL = 0 hecho, 1 los vectores ya no son nuestros, 2 en uso
; =============================================================================
unload:
        push es
        push bx
        push cx
        push dx
        test bl, bl
        jnz .any
        mov bx, handles
.h:
        cmp byte [bx], 0
        jne .busy
        add bx, 16
        cmp bx, handles + NH * 16
        jb .h
.any:
        xor ax, ax
        mov es, ax
        mov dx, cs
        movzx bx, byte [pkt_vec]
        shl bx, 2
        cmp word [es:bx], pkt_entry
        jne .notours
        cmp [es:bx+2], dx
        jne .notours
        movzx bx, byte [irq_vec]
        shl bx, 2
        cmp word [es:bx], irq_handler
        jne .notours
        cmp [es:bx+2], dx
        jne .notours
        cmp word [es:0x2F*4], int2f
        jne .notours
        cmp [es:0x2F*4+2], dx
        jne .notours
        ; parar la tarjeta (antes de soltar sus búferes de DMA)
        call [c_stop]
        cli
        movzx bx, byte [pkt_vec]
        shl bx, 2
        mov eax, [oldpkt]
        mov [es:bx], eax
        movzx bx, byte [irq_vec]
        shl bx, 2
        mov eax, [oldirq]
        mov [es:bx], eax
        mov eax, [old2f]
        mov [es:0x2F*4], eax
        ; la máscara del IRQ, como estaba
        cmp byte [pic_was], 0
        je .nm
        mov cl, [irq]
        mov ah, 1
        cmp cl, 8
        jae .mh
        shl ah, cl
        in al, 0x21
        or al, ah
        out 0x21, al
        jmp .nm
.mh:
        sub cl, 8
        shl ah, cl
        in al, 0xA1
        or al, ah
        out 0xA1, al
.nm:
        sti
        mov ax, [dma_own]
        test ax, ax
        jz .nd
        mov es, ax
        mov ah, 0x49
        int 0x21
.nd:
        push cs
        pop es
        mov ah, 0x49
        int 0x21
        xor al, al
        jmp .r
.notours:
        mov al, 1
        jmp .r
.busy:
        mov al, 2
.r:
        pop dx
        pop cx
        pop bx
        pop es
        ret

; =============================================================================
;  INT 2Fh AH=DAh
; =============================================================================
int2f:
        cmp ah, 0xDA
        je .us
        jmp far [cs:old2f]
.us:
        cmp al, 0
        jne .n1
        mov al, 0xFF
        mov bx, 0x5248
        mov cl, [cs:pkt_vec]
        push cs
        pop es
        mov di, cfg
        iret
.n1:
        push ds
        push cs
        pop ds
        cmp al, 1
        jne .n2
        call [c_link]
        jmp .r
.n2:
        cmp al, 2
        jne .n3
        call unload
        jmp .r
.n3:
        cmp al, 3
        jne .r
        push cs
        pop es
        mov bx, info
.r:
        pop ds
        iret

; =============================================================================
;  Interrupción de la tarjeta
; =============================================================================
irq_handler:
        push ax
        push ds
        push cs
        pop ds
        mov [sv_ss], ss
        mov [sv_sp], sp
        mov ax, cs
        mov ss, ax
        mov sp, pila_top
        pushad
        push es
        push fs
        push gs
        cld
        call [c_isr]                ; CF = 1: no era nuestra
        pop gs
        pop fs
        pop es
        popad
        lss sp, [sv_sp]
        jc .chain
        mov al, 0x20
        cmp byte [irq], 8
        jb .m
        out 0xA0, al
.m:
        out 0x20, al
        pop ds
        pop ax
        iret
.chain:
        pop ds
        pop ax
        jmp far [cs:oldirq]

; ---- entregar un paquete ----
; rx_tmp = sus 14 primeros octetos, CX = longitud. DS = CS.
; -> CF = 1 si nadie lo quiere; si no, ES:DI = búfer del programa (y
; después hay que llamar a rx_done)
rx_find:
        mov [rx_len], cx
        cmp word [rx_mode], 1
        je .drop
        cmp word [rx_mode], 2
        jne .m
        test byte [rx_tmp], 1       ; difusión o multidifusión
        jnz .drop
.m:
        mov bx, handles
        mov dx, 1
.l:
        cmp byte [bx], 0
        je .n
        movzx cx, byte [bx+1]
        jcxz .hit
        lea si, [bx+2]
        mov di, rx_tmp + 12
        push ds
        pop es
        repe cmpsb
        je .hit
.n:
        inc dx
        add bx, 16
        cmp bx, handles + NH * 16
        jb .l
.drop:
        inc dword [st_drop]
        stc
        ret
.hit:
        mov [rx_h], dx
        mov eax, [bx+10]
        mov [rx_rcv], eax
        xor ax, ax
        mov bx, dx
        mov cx, [rx_len]
        xor di, di
        mov es, di
        call far [rx_rcv]
        push cs
        pop ds
        mov ax, es
        or ax, di
        jz .drop
        mov [rx_buf], di
        mov [rx_buf+2], es
        cld
        clc
        ret

rx_done:
        mov ax, 1
        mov bx, [rx_h]
        mov cx, [rx_len]
        movzx edx, cx
        add [st_bin], edx
        inc dword [st_pin]
        lds si, [rx_buf]
        call far [cs:rx_rcv]
        push cs
        pop ds
        cld
        ret

; espera corta (unos microsegundos) para los sondeos con plazo
io_wait:
        push ax
        in al, 0x80
        in al, 0x80
        pop ax
        ret

; =============================================================================
;  NE2000 (DP8390). Memoria de la tarjeta: envío en las páginas 40h-45h,
;  anillo de recepción 46h-80h.
; =============================================================================
NE_TX       equ 0x40
NE_START    equ 0x46
NE_STOP     equ 0x80

; DX = registro -> DX = puerto
%macro NEP 1
        mov dx, [cs:iob]
        add dx, %1
%endmacro

; DMA remoto: AX = dirección, CX = octetos, BL = orden (0Ah leer, 12h escribir)
ne_rdma:
        push ax
        NEP 0x0A
        mov al, cl
        out dx, al
        inc dx
        mov al, ch
        out dx, al
        pop ax
        NEP 0x08
        out dx, al
        inc dx
        mov al, ah
        out dx, al
        NEP 0
        mov al, bl
        out dx, al
        NEP 0x10
        ret

; esperar a que acabe el DMA remoto (RDC) y quitar la marca
ne_rdc:
        push cx
        push ax
        NEP 0x07
        mov cx, 20000
.w:
        in al, dx
        test al, 0x40
        jnz .ok
        call io_wait
        loop .w
.ok:
        mov al, 0x40
        out dx, al
        pop ax
        pop cx
        ret

; DS:SI = paquete, CX = longitud
ne_send:
        pushf
        cli
        mov bp, cx                  ; longitud real
        cmp cx, 60
        jae .l
        mov cx, 60
.l:
        inc cx
        and cx, 0xFFFE
        push cx                     ; lo que se envía (par)
        mov ax, NE_TX << 8
        mov bl, 0x12
        call ne_rdma
        mov cx, bp
        shr cx, 1
        rep outsw
        mov bx, bp                  ; palabras ya escritas: (bp + 1) / 2
        test bp, 1
        jz .ev
        lodsb
        xor ah, ah
        out dx, ax
.ev:
        inc bx
        shr bx, 1
        pop cx
        push cx
        shr cx, 1
        sub cx, bx
        jbe .np
        xor ax, ax
.pad:
        out dx, ax
        loop .pad
.np:
        call ne_rdc
        pop cx
        NEP 0x04                    ; TPSR
        mov al, NE_TX
        out dx, al
        inc dx                      ; TBCR
        mov al, cl
        out dx, al
        inc dx
        mov al, ch
        out dx, al
        NEP 0
        mov al, 0x26                ; enviar
        out dx, al
        NEP 0x07
        mov cx, 60000
.w:
        in al, dx
        test al, 0x0A               ; PTX o TXE
        jnz .d
        call io_wait
        loop .w
        popf
        stc
        ret
.d:
        mov ah, al
        mov al, 0x0A
        out dx, al
        popf
        test ah, 0x08
        jz .ok
        stc
        ret
.ok:
        clc
        ret

ne_isr:
        NEP 0x07
        in al, dx
        cmp al, 0xFF
        je .no
        test al, 0x15               ; PRX, RXE, OVW
        jz .no
        push ax
        and al, 0x05
        out dx, al
        call ne_drain
        pop ax
        test al, 0x10
        jz .ok
        ; desbordamiento del anillo: la secuencia del DP8390
        NEP 0
        mov al, 0x21
        out dx, al
        NEP 0x0A
        xor al, al
        out dx, al
        inc dx
        out dx, al
        NEP 0x0D
        mov al, 0x02
        out dx, al
        NEP 0
        mov al, 0x22
        out dx, al
        call ne_drain
        NEP 0x07
        mov al, 0x10
        out dx, al
        NEP 0x0D
        xor al, al
        out dx, al
.ok:
        clc
        ret
.no:
        stc
        ret

ne_drain:
.next:
        NEP 0
        mov al, 0x62                ; página 1
        out dx, al
        NEP 0x07                    ; CURR
        in al, dx
        mov ah, al
        NEP 0
        mov al, 0x22
        out dx, al
        cmp ah, [ne_next]
        je .r
        ; cabecera (4) y los 14 primeros octetos del paquete
        mov ah, [ne_next]
        xor al, al
        mov cx, 18
        mov bl, 0x0A
        call ne_rdma
        push ds
        pop es
        mov di, rx_tmp
        mov cx, 9
        rep insw
        call ne_rdc
        mov al, [rx_tmp+1]          ; página siguiente
        mov [ne_pg], al
        cmp al, NE_START
        jb .bad
        cmp al, NE_STOP
        jae .bad
        mov cx, [rx_tmp+2]
        sub cx, 4
        cmp cx, 14
        jb .skip
        cmp cx, 1518
        ja .bad
        push cx
        ; la cabecera Ethernet en rx_tmp (rx_find la mira)
        mov si, rx_tmp + 4
        mov di, rx_tmp
        mov cx, 7
        rep movsw
        pop cx
        call rx_find
        jc .skip
        mov ah, [ne_next]
        mov al, 4
        mov bp, cx
        inc cx
        and cx, 0xFFFE
        mov bl, 0x0A
        call ne_rdma
        mov cx, bp
        shr cx, 1
        rep insw
        test bp, 1
        jz .ev
        in ax, dx
        stosb
.ev:
        call ne_rdc
        call rx_done
.skip:
        mov al, [ne_pg]
        mov [ne_next], al
        jmp .bnry
.bad:
        ; cabecera imposible: se vacía el anillo
        inc dword [st_ein]
        NEP 0
        mov al, 0x62
        out dx, al
        NEP 0x07
        in al, dx
        mov ah, al
        NEP 0
        mov al, 0x22
        out dx, al
        mov al, ah
        mov [ne_next], al
        jmp .bnry
.bnry:
        dec al
        cmp al, NE_START
        jae .b
        mov al, NE_STOP - 1
.b:
        NEP 0x03
        out dx, al
        jmp .next
.r:
        ret
ne_pg       db 0

ne_stop:
        NEP 0x0F
        xor al, al
        out dx, al
        NEP 0
        mov al, 0x21
        out dx, al
        ret

ne_link:
        mov ax, 0x00FF              ; no se sabe
        ret

ne_start:
        NEP 0x07
        mov al, 0xFF
        out dx, al
        NEP 0x0F
        mov al, 0x15
        out dx, al
        NEP 0
        mov al, 0x22
        out dx, al
        NEP 0x0D                    ; TCR normal
        xor al, al
        out dx, al
        NEP 0x0C                    ; RCR: difusión
        mov al, 0x04
        out dx, al
        ret

; =============================================================================
;  Realtek RTL8139
; =============================================================================
r39_send:
        mov es, [cs:dma_seg]
        mov di, R39_TX
        mov bx, cx
        rep movsb
        mov cx, bx
        cmp cx, 60
        jae .l
        mov cx, 60
        sub cx, bx
        xor al, al
        rep stosb
        mov cx, 60
.l:
        movzx bx, byte [cs:tx_i]
        shl bx, 2
        mov dx, [cs:iob]
        add dx, 0x10
        add dx, bx
        movzx eax, cx
        out dx, eax                 ; TSDn: longitud, OWN = 0 -> envía
        mov cx, 60000
.w:
        in eax, dx
        test eax, 0x4000C000        ; TOK, TUN, TABT
        jnz .d
        call io_wait
        loop .w
        stc
        ret
.d:
        inc byte [cs:tx_i]
        and byte [cs:tx_i], 3
        test eax, 0x8000
        jz .e
        clc
        ret
.e:
        stc
        ret

r39_isr:
        mov dx, [iob]
        add dx, 0x3E
        in ax, dx
        test ax, ax
        jz .no
        cmp ax, 0xFFFF
        je .no
        out dx, ax                  ; quitar las marcas
        test ax, 0x0053
        jz .ok
        call r39_drain
.ok:
        clc
        ret
.no:
        stc
        ret

r39_drain:
.l:
        mov dx, [iob]
        add dx, 0x37
        in al, dx
        test al, 1                  ; BUFE: vacío
        jnz .r
        mov es, [dma_seg]
        mov bx, [r39_off]
        mov ax, [es:bx]             ; estado
        mov cx, [es:bx+2]           ; longitud con el CRC
        test al, 1
        jz .bad
        cmp cx, 18
        jb .bad
        cmp cx, 1522
        ja .bad
        push cx
        sub cx, 4
        push cx
        ; los 14 primeros octetos a rx_tmp (el anillo da la vuelta a los 8 KB)
        push ds
        pop es
        mov di, rx_tmp
        lea si, [bx+4]
        mov dx, 14
        call r39_copy
        pop cx
        call rx_find
        jc .skip
        mov si, [r39_off]
        add si, 4
        mov dx, [rx_len]
        call r39_copy
        call rx_done
.skip:
        pop cx
        mov ax, [r39_off]
        add ax, cx
        add ax, 4 + 3
        and ax, 0x1FFC
        mov [r39_off], ax
        sub ax, 16
        mov dx, [iob]
        add dx, 0x38                ; CAPR
        out dx, ax
        jmp .l
.bad:
        inc dword [st_ein]
        call r39_rxreset
.r:
        ret

; copiar DX octetos del anillo (desde SI, con la vuelta) a ES:DI
r39_copy:
        push ds
        push si
        mov ds, [cs:dma_seg]
        and si, 0x1FFF
        mov cx, 8192
        sub cx, si
        cmp cx, dx
        jbe .two
        mov cx, dx
        rep movsb
        jmp .r
.two:
        sub dx, cx
        rep movsb
        xor si, si
        mov cx, dx
        rep movsb
.r:
        pop si
        pop ds
        ret

r39_rxreset:
        mov dx, [iob]
        add dx, 0x37
        mov al, 0x04                ; solo envío
        out dx, al
        mov al, 0x0C
        out dx, al
        call r39_rcr
        mov word [r39_off], 0
        mov dx, [iob]
        add dx, 0x38
        mov ax, 0xFFF0
        out dx, ax
        ret

r39_rcr:
        mov dx, [iob]
        add dx, 0x44
        mov eax, 0x0000E70E         ; sin umbral, DMA sin límite, 8 KB,
        out dx, eax                 ; difusión, multidifusión y la nuestra
        ret

r39_start:
        mov dx, [iob]
        add dx, 0x30                ; RBSTART
        mov eax, [dma_phys]
        add eax, R39_RX
        out dx, eax
        mov eax, [dma_phys]
        add eax, R39_TX
        mov dx, [iob]
        add dx, 0x20
        mov cx, 4
.t:
        out dx, eax                 ; TSAD0-3: el mismo búfer
        add dx, 4
        loop .t
        mov dx, [iob]
        add dx, 0x37
        mov al, 0x0C                ; RE y TE
        out dx, al
        call r39_rcr
        mov dx, [iob]
        add dx, 0x40                ; TCR
        mov eax, 0x03000700
        out dx, eax
        mov dx, [iob]
        add dx, 0x08                ; MAR: toda la multidifusión
        mov eax, 0xFFFFFFFF
        out dx, eax
        add dx, 4
        out dx, eax
        mov dx, [iob]
        add dx, 0x38
        mov ax, 0xFFF0
        out dx, ax
        mov dx, [iob]
        add dx, 0x3E
        mov ax, 0xFFFF
        out dx, ax
        mov dx, [iob]
        add dx, 0x3C                ; IMR: ROK, RER, RXOVW, FOVW
        mov ax, 0x0053
        out dx, ax
        ret

r39_stop:
        mov dx, [iob]
        add dx, 0x3C
        xor ax, ax
        out dx, ax
        mov dx, [iob]
        add dx, 0x37
        mov al, 0x10                ; reinicio: para el DMA
        out dx, al
        ret

r39_link:
        mov dx, [iob]
        add dx, 0x64                ; BMSR
        in ax, dx
        test al, 4
        mov ax, 0x0000
        jz .r
        mov dx, [iob]
        add dx, 0x58                ; MSR: bit 3 = 10 Mbit/s
        in al, dx
        test al, 8
        mov ax, 0x0201
        jz .r
        mov ax, 0x0101
.r:
        ret

; =============================================================================
;  Intel e1000 (8254x)
; =============================================================================
; DX = registro -> EAX
e1_rd:
        cmp byte [cs:e_io], 0
        je .m
        push dx
        movzx eax, dx
        mov dx, [cs:iob]
        out dx, eax
        add dx, 4
        in eax, dx
        pop dx
        ret
.m:
        push ebx
        movzx ebx, dx
        add ebx, [cs:mmio]
        mov eax, [fs:ebx]
        pop ebx
        ret

; DX = registro, EAX = valor
e1_wr:
        cmp byte [cs:e_io], 0
        je .m
        push dx
        push eax
        movzx eax, dx
        mov dx, [cs:iob]
        out dx, eax                 ; IOADDR
        add dx, 4
        pop eax
        out dx, eax                 ; IODATA
        pop dx
        ret
.m:
        push ebx
        movzx ebx, dx
        add ebx, [cs:mmio]
        mov [fs:ebx], eax
        pop ebx
        ret

; FS = segmento plano de 4 GB (modo «irreal»; solo en modo real) y A20
; abierta. Conserva los indicadores. Con los registros por puertos no hace
; nada.
e1_flat:
        cmp byte [cs:e_io], 0
        jne .r
        push eax
        push bx
        pushf
        cli
        lgdt [cs:gdtr]
        mov eax, cr0
        or al, 1
        mov cr0, eax
        jmp short .pm
.pm:
        mov bx, 8
        mov fs, bx
        and al, 0xFE
        mov cr0, eax
        jmp short .rm
.rm:
        xor bx, bx
        mov fs, bx
        ; A20 (los registros suelen tener el bit 20 a uno)
        push ds
        push es
        mov ds, bx
        dec bx
        mov es, bx
        mov al, [0x500]
        not al
        mov [0x500], al
        cmp al, [es:0x510]
        not al
        mov [0x500], al
        jne .open
        in al, 0x92
        or al, 2
        and al, 0xFE
        out 0x92, al
.open:
        pop es
        pop ds
        popf
        pop bx
        pop eax
.r:
        ret

e1_send:
        push fs
        call e1_flat
        mov es, [cs:dma_seg]
        mov di, E1_TXB
        mov bx, cx
        rep movsb
        mov cx, bx
        cmp cx, 60
        jae .l
        mov cx, 60
        sub cx, bx
        xor al, al
        rep stosb
        mov cx, 60
.l:
        movzx bx, byte [cs:tx_i]
        shl bx, 4
        add bx, E1_TXD
        mov eax, [cs:dma_phys]
        add eax, E1_TXB
        mov [es:bx], eax
        mov dword [es:bx+4], 0
        mov [es:bx+8], cx           ; longitud
        mov word [es:bx+10], 0x0B00 ; CMD = EOP, IFCS, RS
        mov dword [es:bx+12], 0
        inc byte [cs:tx_i]
        and byte [cs:tx_i], 7
        movzx eax, byte [cs:tx_i]
        mov dx, 0x3818              ; TDT
        call e1_wr
        mov cx, 60000
.w:
        test byte [es:bx+12], 1     ; DD
        jnz .ok
        call io_wait
        loop .w
        pop fs
        stc
        ret
.ok:
        pop fs
        clc
        ret

e1_isr:
        call e1_flat
        mov dx, 0x00C0              ; ICR (se borra al leerlo)
        call e1_rd
        test eax, eax
        jz .no
        cmp eax, 0xFFFFFFFF
        je .no
        call e1_drain
        clc
        ret
.no:
        stc
        ret

e1_drain:
.l:
        mov es, [dma_seg]
        movzx bx, byte [rx_i]
        shl bx, 4
        add bx, E1_RXD
        mov al, [es:bx+12]          ; estado
        test al, 1                  ; DD
        jz .r
        mov cx, [es:bx+8]
        test al, 2                  ; EOP
        jz .bad
        cmp byte [es:bx+13], 0      ; errores
        jne .bad
        cmp cx, 14
        jb .bad
        cmp cx, 1518
        ja .bad
        push bx
        movzx si, byte [rx_i]
        and si, 3
        shl si, 11
        add si, E1_RXB
        push si
        push ds
        mov di, rx_tmp
        push ds
        pop es
        mov ds, [dma_seg]
        push cx
        mov cx, 7
        rep movsw
        pop cx
        pop ds
        call rx_find
        pop si
        jc .sk
        mov cx, [rx_len]
        push ds
        mov ds, [dma_seg]
        rep movsb
        pop ds
        call rx_done
.sk:
        pop bx
        jmp .next
.bad:
        inc dword [st_ein]
.next:
        mov es, [dma_seg]
        mov dword [es:bx+8], 0
        mov dword [es:bx+12], 0
        movzx eax, byte [rx_i]
        add al, 4
        and al, 7
        mov dx, 0x2818              ; RDT
        call e1_wr
        inc byte [rx_i]
        and byte [rx_i], 7
        jmp .l
.r:
        ret

e1_start:
        push fs
        call e1_flat
        mov es, [dma_seg]
        ; descriptores de recepción: el i, con el búfer i & 3
        xor bx, bx
.d:
        mov ax, bx
        and ax, 3
        shl ax, 11
        movzx eax, ax
        add eax, E1_RXB
        add eax, [dma_phys]
        mov di, bx
        shl di, 4
        mov [es:di+E1_RXD], eax
        mov dword [es:di+E1_RXD+4], 0
        mov dword [es:di+E1_RXD+8], 0
        mov dword [es:di+E1_RXD+12], 0
        mov dword [es:di+E1_TXD], 0
        mov dword [es:di+E1_TXD+4], 0
        mov dword [es:di+E1_TXD+8], 0
        mov dword [es:di+E1_TXD+12], 0
        inc bx
        cmp bx, 8
        jb .d
        mov eax, [dma_phys]
        add eax, E1_RXD
        mov dx, 0x2800              ; RDBAL
        call e1_wr
        xor eax, eax
        mov dx, 0x2804
        call e1_wr
        mov eax, 128
        mov dx, 0x2808              ; RDLEN
        call e1_wr
        xor eax, eax
        mov dx, 0x2810              ; RDH
        call e1_wr
        mov eax, 3
        mov dx, 0x2818              ; RDT: la tarjeta tiene 0, 1 y 2
        call e1_wr
        mov eax, [dma_phys]
        add eax, E1_TXD
        mov dx, 0x3800
        call e1_wr
        xor eax, eax
        mov dx, 0x3804
        call e1_wr
        mov eax, 128
        mov dx, 0x3808
        call e1_wr
        xor eax, eax
        mov dx, 0x3810
        call e1_wr
        mov dx, 0x3818
        call e1_wr
        mov eax, 0x0060200A         ; TIPG
        mov dx, 0x0410
        call e1_wr
        mov eax, 0x0004010A         ; TCTL: EN, PSP, CT = 10h, COLD = 40h
        mov dx, 0x0400
        call e1_wr
        mov eax, 0x04008002         ; RCTL: EN, BAM, 2 KB, sin el CRC
        mov dx, 0x0100
        call e1_wr
        mov dx, 0x00C0
        call e1_rd
        mov eax, 0x000000D0         ; IMS: RXT0, RXO, RXDMT0
        mov dx, 0x00D0
        call e1_wr
        pop fs
        ret

e1_stop:
        push fs
        call e1_flat
        mov eax, 0xFFFFFFFF
        mov dx, 0x00D8              ; IMC
        call e1_wr
        xor eax, eax
        mov dx, 0x0100
        call e1_wr
        mov dx, 0x0400
        call e1_wr
        pop fs
        ret

e1_link:
        push fs
        call e1_flat
        mov dx, 0x0008              ; STATUS
        call e1_rd
        pop fs
        test al, 2
        jz .no
        shr al, 6
        and al, 3
        cmp al, 2
        jb .s
        mov al, 2
.s:
        inc al
        mov ah, al
        mov al, 1
        ret
.no:
        xor ax, ax
        ret

; =============================================================================
;  Realtek RTL8168 / 8111 / 8101 / 8169 (sin probar: ver docs/RED.md)
; =============================================================================
r68_send:
        mov es, [cs:dma_seg]
        mov di, R68_TXB
        mov bx, cx
        rep movsb
        mov cx, bx
        cmp cx, 60
        jae .l
        mov cx, 60
        sub cx, bx
        xor al, al
        rep stosb
        mov cx, 60
.l:
        movzx bx, byte [cs:tx_i]
        shl bx, 4
        add bx, R68_TXD
        mov eax, [cs:dma_phys]
        add eax, R68_TXB
        mov [es:bx+8], eax
        mov dword [es:bx+12], 0
        mov dword [es:bx+4], 0
        movzx eax, cx
        or eax, 0xB0000000          ; OWN, FS, LS
        cmp byte [cs:tx_i], 1
        jne .ne
        or eax, 0x40000000          ; EOR: el último del anillo
.ne:
        mov [es:bx], eax
        mov dx, [cs:iob]
        add dx, 0x38                ; TxPoll: NPQ
        mov al, 0x40
        out dx, al
        mov cx, 60000
.w:
        test byte [es:bx+3], 0x80   ; OWN
        jz .ok
        call io_wait
        loop .w
        stc
        ret
.ok:
        xor byte [cs:tx_i], 1
        clc
        ret

r68_isr:
        mov dx, [iob]
        add dx, 0x3E
        in ax, dx
        test ax, ax
        jz .no
        cmp ax, 0xFFFF
        je .no
        out dx, ax
        test ax, 0x0053
        jz .ok
        call r68_drain
.ok:
        clc
        ret
.no:
        stc
        ret

r68_drain:
.l:
        mov es, [dma_seg]
        movzx bx, byte [rx_i]
        shl bx, 4
        add bx, R68_RXD
        mov eax, [es:bx]
        test eax, 0x80000000        ; OWN: todavía es de la tarjeta
        jnz .r
        mov ecx, eax
        and eax, 0x30200000         ; FS, LS y RES (error)
        cmp eax, 0x30000000
        jne .bad
        and cx, 0x3FFF
        sub cx, 4                   ; sin el CRC
        cmp cx, 14
        jb .bad
        cmp cx, 1518
        ja .bad
        push bx
        movzx si, byte [rx_i]
        imul si, si, R68_BUF
        add si, R68_RXB
        push si
        push ds
        mov di, rx_tmp
        push ds
        pop es
        mov ds, [dma_seg]
        push cx
        mov cx, 7
        rep movsw
        pop cx
        pop ds
        call rx_find
        pop si
        jc .sk
        mov cx, [rx_len]
        push ds
        mov ds, [dma_seg]
        rep movsb
        pop ds
        call rx_done
.sk:
        pop bx
        jmp .next
.bad:
        inc dword [st_ein]
.next:
        mov es, [dma_seg]
        mov eax, 0x80000000 | R68_BUF
        cmp byte [rx_i], 3
        jne .ne
        or eax, 0x40000000
.ne:
        mov [es:bx], eax
        inc byte [rx_i]
        and byte [rx_i], 3
        jmp .l
.r:
        ret

r68_start:
        mov es, [dma_seg]
        xor bx, bx
.d:
        mov di, bx
        shl di, 4
        mov eax, 0x80000000 | R68_BUF
        cmp bx, 3
        jne .ne
        or eax, 0x40000000
.ne:
        mov [es:di+R68_RXD], eax
        mov dword [es:di+R68_RXD+4], 0
        mov ax, bx
        imul ax, ax, R68_BUF
        movzx eax, ax
        add eax, R68_RXB
        add eax, [dma_phys]
        mov [es:di+R68_RXD+8], eax
        mov dword [es:di+R68_RXD+12], 0
        mov dword [es:di+R68_TXD], 0
        mov dword [es:di+R68_TXD+4], 0
        mov dword [es:di+R68_TXD+8], 0
        mov dword [es:di+R68_TXD+12], 0
        inc bx
        cmp bx, 4
        jb .d
        mov byte [es:R68_TXD+16+3], 0x40   ; EOR en el segundo de envío
        mov dx, [iob]
        add dx, 0x50                ; 9346CR: desbloquear
        mov al, 0xC0
        out dx, al
        mov dx, [iob]
        add dx, 0xDA                ; RxMaxSize
        mov ax, R68_BUF
        out dx, ax
        mov dx, [iob]
        add dx, 0xEC                ; MaxTxPacketSize (en 128 octetos)
        mov al, 0x3F
        out dx, al
        mov eax, [dma_phys]
        add eax, R68_TXD
        mov dx, [iob]
        add dx, 0x20                ; TNPDS
        out dx, eax
        add dx, 4
        xor eax, eax
        out dx, eax
        mov eax, [dma_phys]
        add eax, R68_RXD
        mov dx, [iob]
        add dx, 0xE4                ; RDSAR
        out dx, eax
        add dx, 4
        xor eax, eax
        out dx, eax
        mov dx, [iob]
        add dx, 0x37
        mov al, 0x0C                ; RE y TE
        out dx, al
        mov dx, [iob]
        add dx, 0x40
        mov eax, 0x03000700         ; TxConfig
        out dx, eax
        mov dx, [iob]
        add dx, 0x44
        mov eax, 0x0000E70E         ; RxConfig
        out dx, eax
        mov dx, [iob]
        add dx, 0x08
        mov eax, 0xFFFFFFFF
        out dx, eax
        add dx, 4
        out dx, eax
        mov dx, [iob]
        add dx, 0x50
        xor al, al
        out dx, al
        mov dx, [iob]
        add dx, 0x3E
        mov ax, 0xFFFF
        out dx, ax
        mov dx, [iob]
        add dx, 0x3C
        mov ax, 0x0053              ; ROK, RER, RDU, FIFO
        out dx, ax
        ret

r68_stop:
        mov dx, [iob]
        add dx, 0x3C
        xor ax, ax
        out dx, ax
        mov dx, [iob]
        add dx, 0x37
        mov al, 0x10
        out dx, al
        ret

r68_link:
        mov dx, [iob]
        add dx, 0x6C                ; PHYstatus
        in al, dx
        test al, 2                  ; LinkSts
        jz .no
        mov ah, 3
        test al, 0x10
        jnz .y
        mov ah, 2
        test al, 8
        jnz .y
        mov ah, 1
        test al, 4
        jnz .y
        mov ah, 0
.y:
        mov al, 1
        ret
.no:
        xor ax, ax
        ret

; =============================================================================
;  Lo último de la instalación (residente: los búferes de DMA caen encima de
;  la instalación). Pone en marcha la tarjeta y se queda residente.
; =============================================================================
go_resident:
        mov cx, [dma_bytes]
        jcxz .nz
        mov es, [dma_seg]
        xor di, di
        xor al, al
        rep stosb
.nz:
        sti
        call [c_start]
        mov dx, [res_paras]
        mov ax, 0x3100
        int 0x21
pila        times PILA db 0
pila_top:
resident_end:

; =============================================================================
;  Instalación (se descarta)
; =============================================================================
        section .init

init:
        cld
        mov si, msg_title
        call puts
        call parse
        jnc .pok
        mov si, msg_usage
        call puts
        mov ax, 0x4C01
        int 0x21
.pok:
        cmp byte [o_help], 0
        je .nh
        mov si, msg_usage
        call puts
        mov ax, 0x4C00
        int 0x21
.nh:
        ; ¿ya está?
        mov ax, 0xDA00
        xor bx, bx
        int 0x2F
        cmp al, 0xFF
        jne .new
        cmp bx, 0x5248
        jne .new
        cmp byte [o_unload], 0
        jne .unl
        cmp byte [o_info], 0
        je .alr
        ; /INFO con el residente: sus datos
        mov ax, 0xDA03
        int 0x2F
        push ds
        push es
        pop ds
        mov si, bx
        push cs
        pop es
        mov di, info
        mov cx, INFO_LEN
        rep movsb
        pop ds
        mov ax, 0xDA01
        int 0x2F
        mov [i_link], ax
        call show_info
        mov ax, 0x4C00
        int 0x21
.alr:
        mov si, msg_already
        call puts
        mov ax, 0x4C01
        int 0x21
.unl:
        mov ax, 0xDA02
        xor bl, bl
        int 0x2F
        mov si, msg_unloaded
        test al, al
        jz .ue
        mov si, msg_noun1
        cmp al, 1
        je .ue1
        mov si, msg_noun2
.ue1:
        call puts
        mov ax, 0x4C01
        int 0x21
.ue:
        call puts
        mov ax, 0x4C00
        int 0x21
.new:
        cmp byte [o_unload], 0
        je .find
        mov si, msg_notres
        call puts
        mov ax, 0x4C01
        int 0x21
.find:
        smsw ax
        and al, 1
        mov [v86], al
        call find_card
        jnc .found
        mov si, msg_nocard
        call puts
        call list_net
        mov ax, 0x4C01
        int 0x21
.found:
        call card_init
        jnc .cok
        call puts                   ; SI = el motivo
        mov ax, 0x4C01
        int 0x21
.cok:
        ; vector del packet driver
        call pick_vector
        jnc .vok
        mov si, msg_novec
        call puts
        mov ax, 0x4C01
        int 0x21
.vok:
        call dma_setup
        jnc .dok
        call puts
        mov ax, 0x4C01
        int 0x21
.dok:
        ; resumen
        mov ax, 0x00FF
        mov [i_link], ax
        cmp byte [o_info], 0
        je .short
        call [c_link]
        mov [i_link], ax
        call show_info
        jmp .hook
.short:
        call show_short
.hook:
        ; vectores
        mov al, [irq]
        add al, 8
        cmp byte [irq], 8
        jb .iv
        add al, 0x70 - 16
.iv:
        mov [irq_vec], al
        mov ah, 0x35
        int 0x21
        mov [oldirq], bx
        mov [oldirq+2], es
        mov al, [pkt_vec]
        mov ah, 0x35
        int 0x21
        mov [oldpkt], bx
        mov [oldpkt+2], es
        mov ax, 0x352F
        int 0x21
        mov [old2f], bx
        mov [old2f+2], es
        ; ¿el IRQ estaba enmascarado?
        mov cl, [irq]
        mov dx, 0x21
        cmp cl, 8
        jb .pm
        sub cl, 8
        mov dx, 0xA1
.pm:
        in al, dx
        shr al, cl
        and al, 1
        mov [pic_was], al
        mov dx, irq_handler
        mov al, [irq_vec]
        mov ah, 0x25
        int 0x21
        mov dx, pkt_entry
        mov al, [pkt_vec]
        mov ah, 0x25
        int 0x21
        mov dx, int2f
        mov ax, 0x252F
        int 0x21
        call irq_unmask
        ; liberar el entorno
        mov es, [0x2C]
        mov ah, 0x49
        int 0x21
        jmp go_resident

; -----------------------------------------------------------------------------
;  Opciones
; -----------------------------------------------------------------------------
parse:
        mov si, 0x81
.l:
        lodsb
        cmp al, ' '
        je .l
        cmp al, 9
        je .l
        cmp al, 13
        je .ok
        test al, al
        jz .ok
        cmp al, '/'
        je .opt
        cmp al, '-'
        je .opt
.bad:
        stc
        ret
.ok:
        clc
        ret
.opt:
        lodsb
        call upcase
        cmp al, '?'
        je .help
        cmp al, 'U'
        je .u
        cmp al, 'I'
        jne .bad
        lodsb
        call upcase
        cmp al, 'N'
        jne .bad
        lodsb
        call upcase
        cmp al, 'F'
        je .info
        cmp al, 'T'
        jne .bad
        lodsb
        cmp al, ':'
        je .x
        cmp al, '='
        jne .bad
.x:
        call hexbyte
        jc .bad
        cmp al, 0x60
        jb .bad
        cmp al, 0x80
        ja .bad
        mov [o_int], al
        jmp .l
.info:
        lodsb
        call upcase
        cmp al, 'O'
        jne .bad
        mov byte [o_info], 1
        jmp .end
.u:
        mov byte [o_unload], 1
        jmp .end
.help:
        mov byte [o_help], 1
.end:
        ; tras la opción: fin, espacio u otra
        mov al, [si]
        cmp al, ' '
        je .l
        cmp al, 9
        je .l
        cmp al, 13
        je .l
        cmp al, '/'
        je .l
        test al, al
        jz .l
        stc
        ret

; DS:SI = cifras hexadecimales -> AL; CF si no hay
hexbyte:
        xor ah, ah
        mov cx, 0
.l:
        mov al, [si]
        call upcase
        sub al, '0'
        jb .e
        cmp al, 9
        jbe .d
        sub al, 'A' - '0' - 10
        cmp al, 10
        jb .e
        cmp al, 15
        ja .e
.d:
        shl ah, 4
        or ah, al
        inc si
        inc cx
        cmp cx, 2
        jb .l
.e:
        mov al, ah
        cmp cx, 1
        ret                         ; CF si no hubo cifras

upcase:
        cmp al, 'a'
        jb .r
        cmp al, 'z'
        ja .r
        sub al, 32
.r:
        ret

o_help      db 0
o_info      db 0
o_unload    db 0
o_int       db 0
v86         db 0
i_link      dw 0

; -----------------------------------------------------------------------------
;  Buscar la tarjeta (BIOS PCI)
; -----------------------------------------------------------------------------
card_tab:
        dw 0x10EC, 0x8029
        db C_NE
        dw 0x1050, 0x0940           ; Winbond 89C940
        db C_NE
        dw 0x1050, 0x5A5A           ; Winbond W89C940F
        db C_NE
        dw 0x11F6, 0x1401           ; Compex RL2000
        db C_NE
        dw 0x8E2E, 0x3000           ; KTI ET32P2
        db C_NE
        dw 0x4A14, 0x5000           ; NetVin NV5000SC
        db C_NE
        dw 0x1106, 0x0926           ; VIA 86C926
        db C_NE
        dw 0x10BD, 0x0E34           ; SureCom NE34
        db C_NE
        dw 0x10EC, 0x8139
        db C_8139
        dw 0x10EC, 0x8138
        db C_8139
        dw 0x1186, 0x1300           ; D-Link DFE-538TX
        db C_8139
        dw 0x1113, 0x1211           ; Accton SMC1211TX
        db C_8139
        dw 0x8086, 0x100E           ; 82540EM (QEMU)
        db C_E1000
        dw 0x8086, 0x100F           ; 82545EM
        db C_E1000
        dw 0x8086, 0x1004           ; 82543GC
        db C_E1000
        dw 0x8086, 0x1015           ; 82540EM LOM
        db C_E1000
        dw 0x8086, 0x1016           ; 82540EP LOM
        db C_E1000
        dw 0x8086, 0x1017           ; 82540EP
        db C_E1000
        dw 0x8086, 0x101E           ; 82540EP (portátiles)
        db C_E1000
        dw 0x8086, 0x1026           ; 82545GM
        db C_E1000
        dw 0x8086, 0x1076           ; 82541GI
        db C_E1000
        dw 0x8086, 0x1078           ; 82541ER
        db C_E1000
        dw 0x8086, 0x107C           ; 82541PI
        db C_E1000
        dw 0x10EC, 0x8168           ; RTL8111/8168 (Lenovo A275)
        db C_8168
        dw 0x10EC, 0x8169
        db C_8168
        dw 0x10EC, 0x8167
        db C_8168
        dw 0x10EC, 0x8136           ; RTL8101E/8102E/8103E
        db C_8168
        dw 0x10EC, 0x8161
        db C_8168
        dw 0x1186, 0x4300           ; D-Link DGE-528T
        db C_8168
        dw 0
card_names  dw 0, name_ne, name_8139, name_e1000, name_8168

; -> CF = 0 y card, pci_bdf, pci_ven, pci_dev
find_card:
        mov ax, 0xB101
        xor edi, edi
        int 0x1A
        jc .no
        cmp edx, 0x20494350
        jne .no
        mov si, card_tab
.l:
        cmp word [si], 0
        je .no
        push si
        mov dx, [si]
        mov cx, [si+2]
        mov ax, 0xB102
        push si
        xor si, si
        int 0x1A
        pop si
        pop si
        jc .n
        test ah, ah
        jnz .n
        mov [pci_bdf], bx
        mov ax, [si]
        mov [pci_ven], ax
        mov ax, [si+2]
        mov [pci_dev], ax
        mov al, [si+4]
        mov [card], al
        movzx bx, al
        add bx, bx
        mov ax, [card_names+bx]
        mov [name_ptr], ax
        clc
        ret
.n:
        add si, 5
        jmp .l
.no:
        stc
        ret

; las tarjetas de red (clase 02h) que hay y que no se saben usar
list_net:
        mov ax, 0xB101
        xor edi, edi
        int 0x1A
        jc .r
        cmp edx, 0x20494350
        jne .r
        xor si, si
        mov byte [ln_any], 0
.c:
        push si
        mov ecx, 0x020000           ; Ethernet
        mov ax, 0xB103
        int 0x1A
        pop si
        jc .w
        test ah, ah
        jnz .w
        push si
        mov si, msg_unk_eth
        call show_dev
        pop si
        inc si
        jmp .c
.w:
        xor si, si
.c2:
        push si
        mov ecx, 0x028000           ; otra (Wi-Fi...)
        mov ax, 0xB103
        int 0x1A
        pop si
        jc .r
        test ah, ah
        jnz .r
        push si
        mov si, msg_unk_wifi
        call show_dev
        pop si
        inc si
        jmp .c2
.r:
        ret
ln_any      db 0

; BX = bdf; SI = texto -> «texto VVVV:DDDD»
show_dev:
        call puts
        mov di, 0
        mov ax, 0xB10A
        int 0x1A
        mov ax, cx
        call hex4
        mov al, ':'
        call putc
        shr ecx, 16
        mov ax, cx
        call hex4
        mov si, msg_pci_at
        call puts
        call put_bdf
        call crlf
        ret

; -----------------------------------------------------------------------------
;  Poner en marcha la tarjeta (sin recibir todavía: eso, en go_resident)
;  -> CF = 1 y SI = mensaje si no se puede
; -----------------------------------------------------------------------------
card_init:
        call pci_d0
        ; IRQ
        mov bx, [pci_bdf]
        mov ax, 0xB108
        mov di, 0x3C
        int 0x1A
        mov [irq], cl
        mov si, msg_noirq
        cmp cl, 1
        jb .fail
        cmp cl, 15
        ja .fail
        cmp cl, 2
        je .fail
        ; puertos y memoria (BAR)
        call find_bars
        ; E/S, memoria y bus master
        mov bx, [pci_bdf]
        mov ax, 0xB109
        mov di, 0x04
        int 0x1A
        or cx, 0x0007
        mov ax, 0xB10C
        int 0x1A
        mov al, [card]
        cmp al, C_NE
        je ne_init
        cmp al, C_8139
        je r39_init
        cmp al, C_E1000
        je e1_init
        jmp r68_init
.fail:
        stc
        ret

; BAR: el primero de puertos -> iob; el primero de memoria -> mmio
find_bars:
        mov di, 0x10
.l:
        mov bx, [pci_bdf]
        mov ax, 0xB10A
        int 0x1A
        test cl, 1
        jz .mem
        cmp word [iob], 0
        jne .n
        and cx, 0xFFFC
        mov [iob], cx
        jmp .n
.mem:
        test ecx, 0xFFFFFFF0
        jz .n
        cmp dword [mmio], 0
        jne .n
        mov eax, ecx
        and eax, 0xFFFFFFF0
        mov [mmio], eax
        and cl, 6
        cmp cl, 4                   ; de 64 bits: la parte alta, después
        jne .n
        add di, 4
        mov ax, 0xB10A
        int 0x1A
        test ecx, ecx
        jz .n
        mov dword [mmio], 0         ; por encima de 4 GB: no se llega
.n:
        add di, 4
        cmp di, 0x28
        jb .l
        ret

; si la tarjeta tiene gestión de energía y está dormida (D1-D3), a D0
pci_d0:
        mov bx, [pci_bdf]
        mov ax, 0xB109
        mov di, 0x06
        int 0x1A
        test cl, 0x10               ; lista de capacidades
        jz .r
        mov ax, 0xB108
        mov di, 0x34
        int 0x1A
        mov dl, 48
.l:
        and cl, 0xFC
        jz .r
        movzx di, cl
        mov ax, 0xB109
        int 0x1A
        cmp cl, 1                   ; PCI Power Management
        je .pm
        mov cl, ch
        dec dl
        jnz .l
        ret
.pm:
        add di, 4
        mov ax, 0xB109
        int 0x1A
        test cl, 3
        jz .r
        and cl, 0xFC
        mov ax, 0xB10C
        int 0x1A
        mov cx, 2
        call ticks_wait
.r:
        ret

; esperar CX tics del reloj (55 ms)
ticks_wait:
        push es
        push ax
        push bx
        xor ax, ax
        mov es, ax
        inc cx
.t:
        mov bx, [es:0x46C]
.w:
        sti
        cmp bx, [es:0x46C]
        je .w
        loop .t
        pop bx
        pop ax
        pop es
        ret

; ---- NE2000 ----
ne_init:
        mov si, msg_noio
        cmp word [iob], 0
        je .f
        ; reinicio
        NEP 0x1F
        in al, dx
        out dx, al
        mov cx, 2
        call ticks_wait
        NEP 0
        mov al, 0x21
        out dx, al
        NEP 0x0E                    ; DCR: palabras, normal, FIFO 8
        mov al, 0x49
        out dx, al
        NEP 0x0A
        xor al, al
        out dx, al
        inc dx
        out dx, al
        NEP 0x0F
        out dx, al                  ; IMR = 0
        NEP 0x07
        mov al, 0xFF
        out dx, al
        NEP 0x0C
        mov al, 0x20                ; monitor
        out dx, al
        NEP 0x0D
        mov al, 0x02                ; bucle interno
        out dx, al
        ; la MAC, de la PROM (en palabras: cada octeto, doble)
        xor ax, ax
        mov cx, 32
        mov bl, 0x0A
        call ne_rdma
        mov di, ne_prom
        push ds
        pop es
        mov cx, 16
        rep insw
        call ne_rdc
        mov si, ne_prom
        mov di, mac
        mov cx, 6
.m:
        lodsw
        stosb
        loop .m
        ; el anillo
        NEP 0x01
        mov al, NE_START
        out dx, al
        inc dx
        mov al, NE_STOP
        out dx, al
        inc dx                      ; BNRY
        mov al, NE_START
        out dx, al
        NEP 0x04
        mov al, NE_TX
        out dx, al
        ; página 1: MAC, CURR y multidifusión
        NEP 0
        mov al, 0x61
        out dx, al
        mov si, mac
        mov cx, 6
        NEP 1
.p:
        lodsb
        out dx, al
        inc dx
        loop .p
        mov al, NE_START + 1        ; CURR (DX = 7)
        out dx, al
        mov [ne_next], al
        inc dx
        mov cx, 8
        mov al, 0xFF
.mar:
        out dx, al
        inc dx
        loop .mar
        NEP 0
        mov al, 0x21
        out dx, al
        mov word [c_send], ne_send
        mov word [c_isr], ne_isr
        mov word [c_stop], ne_stop
        mov word [c_link], ne_link
        mov word [c_start], ne_start
        mov word [need_dma], 0
        clc
        ret
.f:
        stc
        ret
ne_prom     times 32 db 0

; ---- RTL8139 ----
r39_init:
        mov si, msg_noio
        cmp word [iob], 0
        je .f
        mov dx, [iob]
        add dx, 0x52                ; CONFIG1: encendida
        xor al, al
        out dx, al
        mov dx, [iob]
        add dx, 0x37
        mov al, 0x10                ; reinicio
        out dx, al
        mov cx, 1000
.w:
        in al, dx
        test al, 0x10
        jz .ok
        push cx
        mov cx, 1
        call ticks_wait
        pop cx
        loop .w
        mov si, msg_noreset
.f:
        stc
        ret
.ok:
        mov dx, [iob]
        mov di, mac
        push ds
        pop es
        mov cx, 6
.m:
        in al, dx
        stosb
        inc dx
        loop .m
        mov word [c_send], r39_send
        mov word [c_isr], r39_isr
        mov word [c_stop], r39_stop
        mov word [c_link], r39_link
        mov word [c_start], r39_start
        mov word [need_dma], R39_SIZE
        mov word [dma_align], 16
        clc
        ret

; ---- e1000 ----
e1_init:
        ; ¿registros por los puertos? (en QEMU no: solo en las de verdad)
        cmp word [iob], 0
        je .mm
        mov byte [e_io], 1
        mov dx, 0x0008
        call e1_rd
        test eax, eax
        jz .noio
        cmp eax, 0xFFFFFFFF
        jne .io
.noio:
        mov byte [e_io], 0
.mm:
        mov si, msg_nommio
        cmp dword [mmio], 0
        je .f
        mov si, msg_e1v86
        cmp byte [v86], 0
        jne .f
.io:
        push fs
        call e1_flat
        ; reinicio
        mov dx, 0x0000
        call e1_rd
        or eax, 0x04000000
        call e1_wr
        mov cx, 2
        call ticks_wait
        mov eax, 0xFFFFFFFF
        mov dx, 0x00D8              ; IMC
        call e1_wr
        mov dx, 0x00C0
        call e1_rd
        mov dx, 0x0000
        call e1_rd
        or eax, 0x00000060          ; SLU, ASDE
        and eax, 0x7FFFFFF7         ; sin PHY_RST ni LRST
        call e1_wr
        ; la MAC: RAL0/RAH0 (los carga la tarjeta de su EEPROM)
        mov dx, 0x5400
        call e1_rd
        mov [mac], eax
        mov dx, 0x5404
        call e1_rd
        mov [mac+4], ax
        cmp dword [mac], 0
        jne .mac
        ; si no, de la EEPROM (EERD)
        xor bx, bx
.ee:
        mov ax, bx
        shl ax, 8
        or al, 1
        movzx eax, ax
        mov dx, 0x0014
        call e1_wr
        mov cx, 10000
.ew:
        call e1_rd
        test al, 0x10
        jnz .ed
        call io_wait
        loop .ew
.ed:
        shr eax, 16
        mov di, bx
        add di, di
        mov [mac+di], ax
        inc bx
        cmp bx, 3
        jb .ee
        mov eax, [mac]
        mov dx, 0x5400
        call e1_wr
        movzx eax, word [mac+4]
        or eax, 0x80000000          ; AV
        mov dx, 0x5404
        call e1_wr
.mac:
        ; sin multidifusión (MTA a cero)
        mov dx, 0x5200
        xor eax, eax
.mta:
        call e1_wr
        add dx, 4
        cmp dx, 0x5400
        jb .mta
        pop fs
        mov word [c_send], e1_send
        mov word [c_isr], e1_isr
        mov word [c_stop], e1_stop
        mov word [c_link], e1_link
        mov word [c_start], e1_start
        mov word [need_dma], E1_SIZE
        mov word [dma_align], 128
        clc
        ret
.f:
        stc
        ret

; ---- RTL8168 ----
r68_init:
        mov si, msg_noio
        cmp word [iob], 0
        je .f
        mov dx, [iob]
        add dx, 0x37
        mov al, 0x10
        out dx, al
        mov cx, 1000
.w:
        in al, dx
        test al, 0x10
        jz .ok
        push cx
        mov cx, 1
        call ticks_wait
        pop cx
        loop .w
        mov si, msg_noreset
.f:
        stc
        ret
.ok:
        mov dx, [iob]
        mov di, mac
        push ds
        pop es
        mov cx, 6
.m:
        in al, dx
        stosb
        inc dx
        loop .m
        mov word [c_send], r68_send
        mov word [c_isr], r68_isr
        mov word [c_stop], r68_stop
        mov word [c_link], r68_link
        mov word [c_start], r68_start
        mov word [need_dma], R68_SIZE
        mov word [dma_align], 256
        clc
        ret

need_dma    dw 0
dma_align   dw 16

; -----------------------------------------------------------------------------
;  Vector del packet driver: /INT:xx o el primero libre desde el 60h
;  (libre = a cero o apuntando a un IRET). CF si no hay.
; -----------------------------------------------------------------------------
pick_vector:
        mov al, [o_int]
        test al, al
        jz .auto
        call vec_free
        jc .r
        mov [pkt_vec], al
        clc
.r:
        ret
.auto:
        mov al, 0x60
.l:
        cmp al, 0x67                ; el de EMS
        je .n
        call vec_free
        jnc .got
.n:
        inc al
        cmp al, 0x80
        jbe .l
        stc
        ret
.got:
        mov [pkt_vec], al
        clc
        ret

; AL = vector -> CF si está ocupado
vec_free:
        push ax
        push bx
        push es
        mov ah, 0x35
        int 0x21
        mov ax, es
        or ax, bx
        jz .free
        cmp byte [es:bx], 0xCF      ; IRET
        je .free
        stc
        jmp .r
.free:
        clc
.r:
        pop es
        pop bx
        pop ax
        ret

; -----------------------------------------------------------------------------
;  Búferes de DMA: detrás de lo residente o, si se ha cargado en la memoria
;  superior, en un bloque de la convencional. Comprueba que la memoria es
;  física y continua. -> res_paras, dma_seg, dma_phys; CF y SI si no.
; -----------------------------------------------------------------------------
dma_setup:
        mov ax, resident_end + 15
        shr ax, 4
        mov [res_paras], ax
        mov cx, [need_dma]
        mov [dma_bytes], cx
        test cx, cx
        jz .ok
        ; alineación (en párrafos)
        mov bx, [dma_align]
        shr bx, 4
        dec bx                      ; máscara
        mov ax, cs
        cmp ax, 0xA000
        jae .low
        add ax, [res_paras]
        add ax, bx
        not bx
        and ax, bx
        mov [dma_seg], ax
        mov dx, [need_dma]
        add dx, 15
        shr dx, 4
        add ax, dx
        mov dx, cs
        sub ax, dx
        mov [res_paras], ax
        jmp .phys
.low:
        ; LH: un bloque en la memoria convencional
        mov ax, 0x5800
        int 0x21
        mov [sv_strat], ax
        mov ax, 0x5802
        int 0x21
        mov [sv_link], al
        mov ax, 0x5803
        xor bx, bx
        int 0x21
        mov ax, 0x5801
        xor bx, bx                  ; el primero que quepa, abajo
        int 0x21
        mov bx, [need_dma]
        add bx, [dma_align]
        add bx, 15
        shr bx, 4
        mov ah, 0x48
        int 0x21
        pushf
        push ax
        mov ax, 0x5801
        mov bx, [sv_strat]
        int 0x21
        mov ax, 0x5803
        movzx bx, byte [sv_link]
        int 0x21
        pop ax
        popf
        mov si, msg_nodmamem
        jc .fail
        mov [dma_own], ax
        mov bx, [dma_align]
        shr bx, 4
        dec bx
        add ax, bx
        not bx
        and ax, bx
        mov [dma_seg], ax
.phys:
        ; dirección física y continuidad, página a página
        movzx eax, word [dma_seg]
        shl eax, 4
        call phys
        mov [dma_phys], eax
        movzx ebx, word [dma_seg]
        shl ebx, 4
        movzx ecx, word [need_dma]
        add ecx, ebx
        dec ecx                     ; último octeto
        and ebx, 0xFFFFF000
.pg:
        add ebx, 0x1000
        cmp ebx, ecx
        ja .ok
        mov eax, ebx
        call phys
        mov edx, ebx
        movzx esi, word [dma_seg]
        shl esi, 4
        sub edx, esi
        add edx, [dma_phys]
        cmp eax, edx
        je .pg
        mov si, msg_nocont
.fail:
        stc
        ret
.ok:
        clc
        ret
sv_strat    dw 0
sv_link     db 0

; EAX = dirección lineal (primer mega) -> EAX = física (VCPI en modo V86)
phys:
        cmp byte [v86], 0
        je .r
        push ecx
        push edx
        push eax
        push es
        xor cx, cx
        mov es, cx
        cmp dword [es:0x67*4], 0
        pop es
        je .x
        mov ecx, [esp]
        shr ecx, 12
        mov ax, 0xDE06
        int 0x67
        test ah, ah
        jnz .x
        pop eax
        and eax, 0xFFF
        and edx, 0xFFFFF000
        or eax, edx
        pop edx
        pop ecx
.r:
        ret
.x:
        pop eax
        pop edx
        pop ecx
        ret

; -----------------------------------------------------------------------------
;  Mensajes e informe
; -----------------------------------------------------------------------------
show_short:
        mov si, msg_s1
        call puts
        mov si, [name_ptr]
        call puts
        mov si, msg_s2
        call puts
        call put_mac
        mov si, msg_s3
        call puts
        movzx eax, byte [irq]
        call putdec
        mov si, msg_s4
        call puts
        mov al, [pkt_vec]
        call hex2
        mov si, msg_s5
        call puts
        ret

show_info:
        mov si, msg_i_card
        call puts
        mov si, [name_ptr]
        call puts
        call crlf
        mov si, msg_i_pci
        call puts
        mov bx, [pci_bdf]
        call put_bdf
        mov al, ' '
        call putc
        call putc
        mov ax, [pci_ven]
        call hex4
        mov al, ':'
        call putc
        mov ax, [pci_dev]
        call hex4
        call crlf
        cmp word [iob], 0
        je .nio
        mov si, msg_i_io
        call puts
        mov ax, [iob]
        call hex4
        mov al, 'h'
        call putc
        call crlf
.nio:
        cmp byte [card], C_E1000
        jne .nm
        mov si, msg_i_mm
        call puts
        mov eax, [mmio]
        push eax
        shr eax, 16
        call hex4
        pop eax
        call hex4
        mov si, msg_i_mmh
        cmp byte [e_io], 0
        je .mq
        mov si, msg_i_mmp
.mq:
        call puts
.nm:
        ; RTL8168: la versión del chip (TxConfig), para saber qué le falta
        cmp byte [card], C_8168
        jne .nv
        mov si, msg_i_xid
        call puts
        mov dx, [iob]
        add dx, 0x40
        in eax, dx
        push eax
        shr eax, 16
        call hex4
        pop eax
        call hex4
        mov si, msg_i_mmh2
        call puts
.nv:
        mov si, msg_i_irq
        call puts
        movzx eax, byte [irq]
        call putdec
        call crlf
        mov si, msg_i_mac
        call puts
        call put_mac
        call crlf
        mov si, msg_i_link
        call puts
        mov ax, [i_link]
        mov si, msg_l_unk
        cmp al, 0xFF
        je .lk
        mov si, msg_l_no
        test al, al
        jz .lk
        mov si, msg_l_yes
        call puts
        mov si, msg_sp_10
        cmp ah, 1
        je .lk
        mov si, msg_sp_100
        cmp ah, 2
        je .lk
        mov si, msg_sp_1000
        cmp ah, 3
        je .lk
        mov si, msg_empty
.lk:
        call puts
        call crlf
        mov si, msg_i_vec
        call puts
        mov al, [pkt_vec]
        call hex2
        mov si, msg_i_vec2
        call puts
        ; memoria
        mov si, msg_i_mem
        call puts
        movzx eax, word [res_paras]
        shl eax, 4
        call putdec
        mov si, msg_i_bytes
        call puts
        cmp word [dma_own], 0
        je .nl
        mov si, msg_i_dmalow
        call puts
        movzx eax, word [dma_bytes]
        call putdec
        mov si, msg_i_bytes
        call puts
.nl:
        call crlf
        ; programas que la usan y contadores
        mov si, msg_i_h
        call puts
        xor eax, eax
        mov bx, handles
.h:
        cmp byte [bx], 0
        je .hn
        inc ax
.hn:
        add bx, 16
        cmp bx, handles + NH * 16
        jb .h
        call putdec
        call crlf
        mov si, msg_i_pk1
        call puts
        mov eax, [st_pin]
        call putdec
        mov si, msg_i_pk2
        call puts
        mov eax, [st_pout]
        call putdec
        mov si, msg_i_pk3
        call puts
        mov eax, [st_drop]
        call putdec
        mov si, msg_i_pk4
        call puts
        mov eax, [st_ein]
        add eax, [st_eout]
        call putdec
        call crlf
        ; configuración IP
        mov si, msg_i_ip
        call puts
        test byte [cfg+CFG_FLAGS], 1
        jnz .ip
        mov si, msg_i_noip
        call puts
        ret
.ip:
        mov si, cfg + CFG_IP
        call put_ip
        mov si, msg_i_dhcp
        test byte [cfg+CFG_FLAGS], 2
        jnz .ipd
        mov si, msg_i_man
.ipd:
        call puts
        ret

put_mac:
        mov si, mac
        mov cx, 6
.l:
        lodsb
        call hex2
        dec cx
        jz .r
        mov al, ':'
        call putc
        jmp .l
.r:
        ret

; DS:SI = IP
put_ip:
        mov cx, 4
.l:
        movzx eax, byte [si]
        inc si
        call putdec
        dec cx
        jz .r
        mov al, '.'
        call putc
        jmp .l
.r:
        ret

; BX = bdf -> «bb:dd.f»
put_bdf:
        mov al, bh
        call hex2
        mov al, ':'
        call putc
        mov al, bl
        shr al, 3
        call hex2
        mov al, '.'
        call putc
        mov al, bl
        and al, 7
        add al, '0'
        call putc
        ret

putc:
        push ax
        push dx
        mov dl, al
        mov ah, 2
        int 0x21
        pop dx
        pop ax
        ret

puts:
        push ax
        push si
.l:
        lodsb
        test al, al
        jz .d
        call putc
        jmp .l
.d:
        pop si
        pop ax
        ret

crlf:
        push si
        mov si, msg_crlf
        call puts
        pop si
        ret

hex4:
        push ax
        mov al, ah
        call hex2
        pop ax
hex2:
        push ax
        shr al, 4
        call .d
        pop ax
        push ax
        and al, 15
        call .d
        pop ax
        ret
.d:
        add al, '0'
        cmp al, '9'
        jbe .p
        add al, 7
.p:
        jmp putc

; EAX en decimal
putdec:
        push eax
        push ebx
        push edx
        push cx
        mov ebx, 10
        xor cx, cx
.l:
        xor edx, edx
        div ebx
        push dx
        inc cx
        test eax, eax
        jnz .l
.p:
        pop ax
        add al, '0'
        call putc
        loop .p
        pop cx
        pop edx
        pop ebx
        pop eax
        ret

msg_title   db "HSPRED 0.1: controlador de red de Hispa-DOS (packet driver)", 13, 10, 0
msg_usage   db "Uso: HSPRED [/INT:xx] [/INFO] [/U]", 13, 10
            db "  /INT:xx  vector del packet driver (60 a 80, hexadecimal; si no, el", 13, 10
            db "           primero libre desde el 60)", 13, 10
            db "  /INFO    tarjeta, MAC, enlace, IRQ, vector y la configuración IP", 13, 10
            db "  /U       descargarlo de la memoria", 13, 10
            db "Tarjetas: NE2000 PCI (RTL8029), Realtek RTL8139, Intel PRO/1000 (e1000)", 13, 10
            db "y Realtek RTL8111/8168/8101 (sin probar). La IP: CONFRED (IPCONFIG).", 13, 10
            db "Sirve también para los programas de DOS que usan packet drivers (mTCP...).", 13, 10, 0
msg_already db "HSPRED ya está cargado (HSPRED /INFO lo enseña; /U lo descarga).", 13, 10, 0
msg_notres  db "HSPRED no está cargado.", 13, 10, 0
msg_unloaded db "HSPRED descargado.", 13, 10, 0
msg_noun1   db "No se puede descargar: hay otro programa residente cargado después.", 13, 10, 0
msg_noun2   db "No se puede descargar: hay un programa usando la red.", 13, 10, 0
msg_nocard  db "No se ha encontrado ninguna tarjeta de red que HSPRED sepa usar.", 13, 10, 0
msg_unk_eth db "  Hay una tarjeta Ethernet que todavía no se sabe usar: ", 0
msg_unk_wifi db "  Hay una tarjeta inalámbrica (Wi-Fi), que todavía no se sabe usar: ", 0
msg_pci_at  db " en ", 0
msg_noirq   db "La tarjeta no tiene IRQ asignado por la BIOS.", 13, 10, 0
msg_noio    db "La tarjeta no tiene puertos de E/S asignados.", 13, 10, 0
msg_noreset db "La tarjeta no termina de reiniciarse.", 13, 10, 0
msg_nommio  db "La tarjeta no tiene sus registros en memoria asignados.", 13, 10, 0
msg_e1v86   db "Esta tarjeta e1000 solo da sus registros en memoria, y en modo V86 (con", 13, 10
            db "HSPEMM u otro EMM386) no se llega a ellos: arranque en «Normal».", 13, 10, 0
msg_novec   db "No hay ningún vector libre para el packet driver (60h-80h).", 13, 10, 0
msg_nodmamem db "No hay memoria convencional para los búferes de la tarjeta.", 13, 10, 0
msg_nocont  db "Los búferes de la tarjeta no quedan en memoria física continua.", 13, 10, 0
msg_s1      db "Tarjeta ", 0
msg_s2      db ", MAC ", 0
msg_s3      db ", IRQ ", 0
msg_s4      db ", packet driver en INT ", 0
msg_s5      db "h", 13, 10, 0
msg_i_card  db "Tarjeta:         ", 0
msg_i_pci   db "PCI:             ", 0
msg_i_io    db "Puertos:         ", 0
msg_i_mm    db "Registros:       ", 0
msg_i_mmh   db "h (registros por memoria)", 13, 10, 0
msg_i_mmp   db "h (registros por los puertos)", 13, 10, 0
msg_i_irq   db "IRQ:             ", 0
msg_i_xid   db "Chip (TxConfig): ", 0
msg_i_mmh2  db "h", 13, 10, 0
msg_i_mac   db "Dirección MAC:   ", 0
msg_i_link  db "Enlace:          ", 0
msg_l_unk   db "no se sabe (esta tarjeta no lo dice)", 0
msg_l_no    db "no (¿está el cable puesto?)", 0
msg_l_yes   db "sí", 0
msg_sp_10   db ", 10 Mbit/s", 0
msg_sp_100  db ", 100 Mbit/s", 0
msg_sp_1000 db ", 1000 Mbit/s", 0
msg_empty   db 0
msg_i_vec   db "Packet driver:   INT ", 0
msg_i_vec2  db "h", 13, 10, 0
msg_i_mem   db "Memoria:         ", 0
msg_i_bytes db " octetos", 0
msg_i_dmalow db " residentes, y búferes en la convencional: ", 0
msg_i_h     db "Programas:       ", 0
msg_i_pk1   db "Paquetes:        recibidos ", 0
msg_i_pk2   db ", enviados ", 0
msg_i_pk3   db ", descartados ", 0
msg_i_pk4   db ", errores ", 0
msg_i_ip    db "Dirección IP:    ", 0
msg_i_noip  db "sin configurar (CONFRED la pide)", 13, 10, 0
msg_i_dhcp  db " (DHCP)", 13, 10, 0
msg_i_man   db " (a mano, RED.INI)", 13, 10, 0
msg_crlf    db 13, 10, 0
