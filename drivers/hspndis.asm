; =============================================================================
;  HSPNDIS.COM - módulo de protocolo NDIS 2 de Hispa-DOS
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Une Hispa-DOS con el driver NDIS 2 («MAC», un .DOS) de una tarjeta de red,
;  a través del gestor de protocolos (PROTMAN$). Escrito con la especificación
;  pública 3Com/Microsoft «LAN Manager Network Driver Interface Specification
;  2.0.1» (1990) y la Ralf Brown's Interrupt List (INT 21h 4402h): sin código
;  de nadie.
;
;  Uso (AUTOEXEC.BAT, con el gestor de protocolos —HSPPMAN.SYS, el nuestro,
;  u otro PROTMAN$— y el MAC en el CONFIG.SYS):
;      HSPNDIS [/P]
;  En PROTOCOL.INI, una sección [HSPNDIS] con BINDINGS = <sección del MAC>.
;  Se queda residente y da en INT 60h la interfaz de packet driver (la que
;  usan CONFRED, PING, DESCARGAR, CONSOLA y ACTUALIZAR, y los programas de
;  packet driver de otros). El MAC no se puede soltar: no se descarga.
;  /P: prueba: tras unirse, envía una petición ARP a 10.0.2.2 (la puerta de
;  QEMU) y dice cuántas tramas han llegado.
; =============================================================================
        cpu 386
        bits 16
        org 0x100

start:
        jmp main

; ---- tabla de características comunes (especificación 2.0.1, 4.2) ----
        align 2
cct:
        dw 64                       ; +00 tamaño
        db 2, 0                     ; +02 NDIS 2.0
        dw 0                        ; +04
        db 0, 1                     ; +06 versión del módulo 0.1
        dd 2                        ; +08 se une por abajo (a un MAC)
cct_name:
        db "HSPNDIS", 0, 0, 0, 0, 0, 0, 0, 0, 0     ; +0C (16)
        db 3, 0                     ; +1C arriba: red, privado
        db 1, 1                     ; +1E abajo: MAC, interfaz MAC
cct_id  dw 0                        ; +20 lo pone PROTMAN
cct_ds  dw 0                        ; +22 nuestro DS
        dw sys_entry                ; +24 entrada de peticiones del sistema
cct_cs1 dw 0
        dd 0                        ; +28 características propias: ninguna
        dd 0                        ; +2C estado propio: ninguno
        dd 0                        ; +30 despacho de arriba: ninguno
        dw lower                    ; +34 nuestro despacho de abajo
cct_cs2 dw 0
        dd 0, 0                     ; +38 reservado

; ---- despacho de abajo (lo que nos llama el MAC; 4.8) ----
lower:
        dw cct                      ; +00 vuelta a la tabla común
low_cs  dw 0
        dd 1                        ; +04 tramas sin LLC
        dw p_reqconf                ; +08 RequestConfirm
low_c1  dw 0
        dw p_txconf                 ; +0C TransmitConfirm
low_c2  dw 0
        dw p_lookahead              ; +10 ReceiveLookahead
low_c3  dw 0
        dw p_indcomplete            ; +14 IndicationComplete
low_c4  dw 0
        dw p_rxchain                ; +18 ReceiveChain
low_c5  dw 0
        dw p_status                 ; +1C Status
low_c6  dw 0

mac_cct     dd 0                    ; la tabla común del MAC (de Bind)
mac_ds      dw 0
mac_udisp   dd 0                    ; su despacho de arriba
rx_tramas   dw 0
rx_arp      dw 0
bind_st     dw 0xFFFF

; ---- lo que nos llama el MAC: convenio Pascal lejano, nosotros quitamos los
; parámetros; el último que se mete es el DS del protocolo ----
p_reqconf:
        xor ax, ax
        retf 12
p_txconf:
        mov byte [cs:tx_busy], 0    ; el búfer de envío vuelve a estar libre
        xor ax, ax
        retf 10
p_indcomplete:
        xor ax, ax
        retf 4
p_status:
        xor ax, ax
        retf 12

; ReceiveLookahead(MACID, FrameSize, BytesAvail, Buffer, Indicate, ProtDS)
;   [bp+6] ProtDS, [bp+8] Indicate, [bp+12] Buffer, [bp+16] BytesAvail,
;   [bp+18] FrameSize, [bp+20] MACID
p_lookahead:
        push bp
        mov bp, sp
        pushad
        push ds
        push es
        push fs
        push cs
        pop ds
        inc word [rx_tramas]
        mov cx, [bp+18]
        cmp cx, 14
        jb .no
        cmp cx, 1514
        ja .no
        cmp word [bp+16], 14
        jb .no
        lfs si, [bp+12]
        cmp word [fs:si+12], 0x0608
        jne .na
        inc word [rx_arp]
.na:
        call buscar_handle          ; -> BX = manejador (CF si nadie lo quiere)
        jc .no
        call pedir_sitio            ; receptor, AX=0 -> ES:DI
        jc .no
        mov cx, [bp+18]
        cmp cx, [bp+16]
        ja .transfer
        push di
        push cx
.cp:                                ; todo está en el lookahead: se copia
        mov al, [fs:si]
        mov [es:di], al
        inc si
        inc di
        loop .cp
        pop cx
        pop di
        jmp .entregar
.transfer:                          ; TransferData del resto, hasta ES:DI
        mov word [tdd], 1
        mov word [tdd+2], 0
        mov [tdd+4], cx
        mov [tdd+6], di
        mov [tdd+8], es
        push es
        push di
        push cx
        push cs
        push word copiados
        push word 0
        push cs
        push word tdd
        push word [mac_ds]
        les bx, [mac_udisp]
        call far [es:bx+12]
        pop cx
        pop di
        pop es
        test ax, ax
        jnz .no
        mov bx, [h_actual]
.entregar:
        call entregar               ; receptor, AX=1
        pop fs
        pop es
        pop ds
        popad
        pop bp
        xor ax, ax
        retf 14
.no:
        pop fs
        pop es
        pop ds
        popad
        pop bp
        mov ax, 3                   ; FRAME_NOT_RECOGNIZED
        retf 14

; ReceiveChain(MACID, FrameSize, ReqHandle, RxBufDescr, Indicate, ProtDS)
;   [bp+6] ProtDS, [bp+8] Indicate, [bp+12] RxBufDescr, [bp+16] ReqHandle,
;   [bp+18] FrameSize, [bp+20] MACID. Se copia todo antes de volver.
p_rxchain:
        push bp
        mov bp, sp
        pushad
        push ds
        push es
        push fs
        push gs
        push cs
        pop ds
        inc word [rx_tramas]
        mov cx, [bp+18]
        cmp cx, 14
        jb .no
        cmp cx, 1514
        ja .no
        lgs bx, [bp+12]             ; RxDataCount, y bloques de 6
        cmp word [gs:bx], 0
        je .no
        cmp word [gs:bx+2], 14      ; la cabecera, en el primer bloque
        jb .no
        lfs si, [gs:bx+4]
        push bx
        call buscar_handle
        mov [h_actual], bx
        pop bx
        jc .no
        push bx
        mov bx, [h_actual]
        call pedir_sitio
        pop bx
        jc .no
        push di
        mov dx, [gs:bx]             ; cuántos bloques
        add bx, 2
        mov cx, [bp+18]             ; lo que queda por copiar
.blq:
        test dx, dx
        jz .fin
        jcxz .fin
        push cx
        mov ax, [gs:bx]             ; RxDataLen
        lfs si, [gs:bx+2]           ; RxDataPtr
        cmp ax, cx
        jbe .l
        mov ax, cx
.l:
        mov cx, ax
.c:
        jcxz .ce
        mov al, [fs:si]
        mov [es:di], al
        inc si
        inc di
        dec cx
        jmp .c
.ce:
        pop cx
        sub cx, [gs:bx]             ; (si el bloque era mayor, CX pasa de 0:
        jae .sg                     ;  ya está todo)
        xor cx, cx
.sg:
        add bx, 6
        dec dx
        jmp .blq
.fin:
        pop di
        mov cx, [bp+18]
        mov bx, [h_actual]
        call entregar
        pop gs
        pop fs
        pop es
        pop ds
        popad
        pop bp
        xor ax, ax
        retf 16
.no:
        pop gs
        pop fs
        pop es
        pop ds
        popad
        pop bp
        mov ax, 3
        retf 16

; FS:SI = trama -> BX = manejador (1..NH) que quiere su tipo; CF si ninguno
buscar_handle:
        push cx
        push di
        mov bx, 1
        mov di, handles
.l:
        cmp byte [di+H_USED], 0
        je .sig
        movzx cx, byte [di+H_TLEN]
        push si
        push di
        add si, 12
        add di, H_TIPO
.c:
        jcxz .igual
        mov al, [fs:si]
        cmp al, [di]
        jne .dist
        inc si
        inc di
        dec cx
        jmp .c
.igual:
        pop di
        pop si
        mov [h_actual], bx
        clc
        jmp .r
.dist:
        pop di
        pop si
.sig:
        add di, H_LEN
        inc bx
        cmp bx, NH
        jbe .l
        stc
.r:
        pop di
        pop cx
        ret

; BX = manejador, CX = longitud: el receptor, AX=0 -> ES:DI (CF si 0:0)
pedir_sitio:
        push bx
        push cx
        push si
        push bp
        push ds
        imul si, bx, H_LEN
        add si, handles - H_LEN
        xor ax, ax
        call far [si+H_RECV]
        pop ds
        pop bp
        pop si
        pop cx
        pop bx
        mov ax, es
        or ax, di
        jnz .ok
        stc
        ret
.ok:
        clc
        ret

; BX = manejador, CX = longitud, ES:DI = la trama ya copiada: AX=1, DS:SI
entregar:
        push ds
        push bp
        imul si, bx, H_LEN
        add si, handles - H_LEN
        push word [si+H_RECV+2]
        push word [si+H_RECV]
        mov bp, sp
        push es
        pop ds
        mov si, di
        mov ax, 1
        call far [ss:bp]
        add sp, 4
        pop bp
        pop ds
        ret

; =============================================================================
;  La interfaz de packet driver (FTP Software, 1.09/1.11), clase 1 (Ethernet):
;  1 driver_info, 2 access_type, 3 release_type, 4 send_pkt, 5 terminate,
;  6 get_address, 7 reset_interface. Errores: CF y DH = código.
; =============================================================================
pd_int:
        jmp short pd_main
        nop
        db "PKT DRVR", 0
pd_main:
        sti
        cld
        cmp ah, 1
        je pd_info
        cmp ah, 2
        je pd_access
        cmp ah, 3
        je pd_release
        cmp ah, 4
        je pd_send
        cmp ah, 5
        je pd_terminate
        cmp ah, 6
        je pd_address
        cmp ah, 7
        je pd_ok
        mov dh, 11                  ; BAD_COMMAND
pd_err:
        stc
        retf 2
pd_ok:
        clc
        retf 2
pd_terminate:
        mov dh, 7                   ; CANT_TERMINATE
        jmp pd_err
pd_info:
        mov bx, 1                   ; versión
        mov cx, 0x0100              ; clase 1, número 0
        mov dx, 1
        push cs
        pop ds
        mov si, pd_nombre
        mov al, 1                   ; funciones básicas
        jmp pd_ok
; AL = clase, BX = tipo, DL = número, DS:SI = plantilla, CX = su largo,
; ES:DI = receptor -> AX = manejador
pd_access:
        cmp al, 1
        je .cl
        mov dh, 2                   ; NO_CLASS
        jmp pd_err
.cl:
        cmp cx, 8
        jbe .tl
        mov dh, 5                   ; BAD_TYPE
        jmp pd_err
.tl:
        push bx
        push si
        push di
        push cx
        mov bx, handles
        mov ax, 1
.b:
        cmp byte [cs:bx+H_USED], 0
        je .libre
        add bx, H_LEN
        inc ax
        cmp ax, NH
        jbe .b
        pop cx
        pop di
        pop si
        pop bx
        mov dh, 9                   ; NO_SPACE
        jmp pd_err
.libre:
        mov byte [cs:bx+H_USED], 1
        mov [cs:bx+H_TLEN], cl
        mov [cs:bx+H_RECV], di
        mov [cs:bx+H_RECV+2], es
        lea di, [bx+H_TIPO]
.ct:
        jcxz .ctf
        push ax
        lodsb
        mov [cs:di], al
        pop ax
        inc di
        dec cx
        jmp .ct
.ctf:
        pop cx
        pop di
        pop si
        pop bx
        jmp pd_ok
; BX = manejador
pd_release:
        call pd_handle
        jc pd_err
        mov byte [cs:si+H_USED], 0
        jmp pd_ok
; BX = manejador -> SI = su entrada; CF y DH = BAD_HANDLE
pd_handle:
        cmp bx, 1
        jb .mal
        cmp bx, NH
        ja .mal
        imul si, bx, H_LEN
        add si, handles - H_LEN
        cmp byte [cs:si+H_USED], 0
        je .mal
        clc
        ret
.mal:
        mov dh, 1
        stc
        ret
; BX = manejador, ES:DI = sitio, CX = su largo -> CX = 6
pd_address:
        push si
        call pd_handle
        pop si
        jc pd_err
        cmp cx, 6
        jae .ok
        mov dh, 9                   ; NO_SPACE
        jmp pd_err
.ok:
        push si
        push di
        mov si, mi_mac
        mov cx, 6
.c:
        mov al, [cs:si]
        stosb
        inc si
        loop .c
        pop di
        pop si
        mov cx, 6
        jmp pd_ok
; DS:SI = trama, CX = largo
pd_send:
        cmp cx, 1514
        jbe .l
        mov dh, 12                  ; CANT_SEND
        jmp pd_err
.l:
        pushad
        push es
        ; esperar a que el MAC suelte el envío anterior (como mucho ~1 s)
        mov edx, 0x00400000
.esp:
        cmp byte [cs:tx_busy], 0
        je .libre
        dec edx
        jnz .esp
        mov byte [cs:tx_busy], 0
.libre:
        push cs
        pop es
        mov di, txbuf
        push cx
        rep movsb
        pop cx
        cmp cx, 60
        jae .tam
        push cx
        neg cx
        add cx, 60
        xor al, al
        rep stosb
        pop cx
        mov cx, 60
.tam:
        mov word [cs:txd2], 0
        mov dword [cs:txd2+2], 0
        mov word [cs:txd2+6], 1
        mov word [cs:txd2+8], 0
        mov [cs:txd2+10], cx
        mov word [cs:txd2+12], txbuf
        mov [cs:txd2+14], cs
        mov byte [cs:tx_busy], 1
        push word [cs:cct_id]
        push word 1                 ; ReqHandle 1: TransmitConfirm si va en cola
        push cs
        push word txd2
        push word [cs:mac_ds]
        les bx, [cs:mac_udisp]
        call far [es:bx+8]
        cmp ax, 2                   ; REQUEST_QUEUED: el confirm lo suelta
        je .cola
        mov byte [cs:tx_busy], 0
        test ax, ax
        jz .cola
        pop es
        popad
        mov dh, 12
        jmp pd_err
.cola:
        pop es
        popad
        jmp pd_ok

NH          equ 4                   ; manejadores (tipos) a la vez
H_USED      equ 0
H_TLEN      equ 1
H_TIPO      equ 2                   ; hasta 8 octetos
H_RECV      equ 10                  ; el receptor (lejano)
H_LEN       equ 14
handles     times NH*H_LEN db 0
h_actual    dw 0
copiados    dw 0
tdd         times 10 db 0
txd2        times 16 db 0
tx_busy     db 0
mi_mac      times 6 db 0
pd_nombre   db "HSPNDIS (NDIS 2)", 0
txbuf       times 1514 db 0

; Peticiones del sistema (5.3): System(Param1 dd, Param2 dd, Param3 dw,
; Opcode dw, TargetDS dw): [bp+6] DS, [bp+8] opcode, [bp+10] Param3,
; [bp+12] Param2, [bp+16] Param1
sys_entry:
        push bp
        mov bp, sp
        push ds
        push es
        push bx
        push cx
        push si
        push di
        mov ds, [bp+6]
        cmp word [bp+8], 1          ; InitiateBind
        jne .nof
        cmp dword [bp+12], 0
        je .nocct
        ; Bind al MAC: Bind(nuestra tabla, &su tabla, 0, 2, DS del MAC)
        les bx, [bp+12]
        mov ax, [es:bx+0x22]
        mov [mac_ds], ax
        push ds
        push word cct
        push ds
        push word mac_cct
        push word 0
        push word 2
        push ax
        call far [es:bx+0x24]
        mov [bind_st], ax
        test ax, ax
        jnz .r
        les bx, [mac_cct]
        mov eax, [es:bx+0x30]
        mov [mac_udisp], eax
        xor ax, ax
        jmp .r
.nocct:
        xor ax, ax
        jmp .r
.nof:
        mov ax, 8                   ; INVALID_FUNCTION
.r:
        pop di
        pop si
        pop cx
        pop bx
        pop es
        pop ds
        pop bp
        retf 14

; General request al MAC: AX = opcode, CX = Param1, EDX = Param2 -> AX estado
mac_request:
        push word [cct_id]          ; ProtID
        push word 0                 ; ReqHandle 0: sin confirmación
        push cx                     ; Param1
        push edx                    ; Param2
        push ax                     ; Opcode
        push word [mac_ds]
        les bx, [mac_udisp]
        call far [es:bx+4]
        ret

fin_residente:

; =============================================================================
;  Lo que no se queda residente
; =============================================================================
main:
        mov ax, cs
        mov [cct_ds], ax
        mov [cct_cs1], ax
        mov [cct_cs2], ax
        mov [low_cs], ax
        mov [low_c1], ax
        mov [low_c2], ax
        mov [low_c3], ax
        mov [low_c4], ax
        mov [low_c5], ax
        mov [low_c6], ax
        mov dx, m_titulo
        call puts
        ; ¿/P?
        mov si, 0x81
.arg:
        lodsb
        cmp al, 13
        je .argf
        and al, 0xDF
        cmp al, 'P'
        jne .arg
        mov byte [prueba], 1
        jmp .arg
.argf:
        ; dejar solo nuestro trozo (BindAndStart carga PROTMAN.EXE)
        mov bx, (fin_total - start + 0x100 + 15) / 16 + 64
        mov ah, 0x4A
        int 0x21
        mov sp, pila_fin
        ; PROTMAN$
        mov dx, n_protman
        mov ax, 0x3D02
        int 0x21
        jnc .abierto
        mov dx, m_noprotman
        jmp error
.abierto:
        mov [h], ax
        mov word [rb], 4            ; GetProtocolManagerLinkage
        call ioctl
        jc ioerr_main
        mov eax, [rb+4]
        mov [pm_entry], eax
        mov ax, [rb+12]
        mov [pm_ds], ax
        mov word [rb], 1            ; GetProtocolManagerInfo
        call ioctl
        jc ioerr_main
        mov eax, [rb+4]
        mov [pm_img], eax
        mov ah, 0x3E
        mov bx, [h]
        int 0x21
        ; las uniones: BINDINGS de nuestra sección en la imagen
        call buscar_bindings
        jnc .hayb
        mov dx, m_nobind
        jmp error
.hayb:
        ; RegisterModule(cct, lista de uniones), por la entrada directa
        mov word [rb], 2
        mov word [rb+4], cct
        mov [rb+6], cs
        mov word [rb+8], blist
        mov [rb+10], cs
        call pm_call
        mov dx, m_reg
        call puts_estado
        test ax, ax
        jnz salir1
        ; BindAndStart(FailingModules)
        mov word [rb], 3
        mov word [rb+4], fallan
        mov [rb+6], cs
        mov dword [rb+8], 0
        call pm_call
        mov dx, m_bas
        call puts_estado
        mov dx, m_bind
        mov ax, [bind_st]
        call puts_estado
        cmp dword [mac_udisp], 0
        jne .unido
        mov dx, m_fallan
        call puts
        mov si, fallan
        call puts0
        mov al, ' '
        call putc
        mov si, fallan+16
        call puts0
        call crlf
        jmp salir1
.unido:
        ; la dirección de la tarjeta (características propias del MAC, +24h)
        les bx, [mac_cct]
        les bx, [es:bx+0x28]
        lea si, [bx+0x24]
        mov di, mi_mac
        mov cx, 6
.cm:
        mov al, [es:si]
        mov [di], al
        inc si
        inc di
        loop .cm
        mov dx, m_mac
        call puts
        mov si, mi_mac
        mov cx, 6
.pm:
        lodsb
        call hex8
        loop .pm
        call crlf
        ; los «service flags» del MAC (características propias, +40h)
        mov dx, m_flags
        call puts
        les bx, [mac_cct]
        les bx, [es:bx+0x28]
        mov eax, [es:bx+0x40]
        push ax
        shr eax, 16
        call hex16
        pop ax
        call hex16
        call crlf
        ; OpenAdapter (si lo pide: bit 11)
        test word [es:bx+0x40], 0x0800
        jz .noopen
        mov ax, 4
        xor cx, cx
        xor edx, edx
        call mac_request
        mov dx, m_open
        call puts_estado
.noopen:
        ; SetPacketFilter: directas y difusión
        mov ax, 7
        mov cx, 3
        xor edx, edx
        call mac_request
        mov dx, m_filtro
        call puts_estado
        cmp byte [prueba], 0
        je residente
        ; la prueba: ARP a 10.0.2.2
        mov si, mi_mac
        mov di, arp_trama+6
        mov cx, 6
        rep movsb
        mov si, mi_mac
        mov di, arp_trama+22
        mov cx, 6
        rep movsb
        mov word [txd], 0           ; sin datos inmediatos
        mov dword [txd+2], 0
        mov word [txd+6], 1
        mov word [txd+8], 0         ; tipo 0 y reservado
        mov word [txd+10], 60
        mov word [txd+12], arp_trama
        mov [txd+14], cs
        push word [cct_id]
        push word 0
        push cs
        push word txd
        push word [mac_ds]
        les bx, [mac_udisp]
        call far [es:bx+8]          ; TransmitChain
        mov dx, m_tx
        call puts_estado
        ; esperar un segundo (18 tics)
        mov ah, 0
        int 0x1A
        mov bx, dx
.esp:
        sti
        hlt
        mov ah, 0
        int 0x1A
        sub dx, bx
        cmp dx, 18
        jb .esp
        mov dx, m_rx
        call puts
        mov ax, [rx_tramas]
        call hex16
        mov dx, m_rxarp
        call puts
        mov ax, [rx_arp]
        call hex16
        call crlf
        ; sin cerrar el MAC no podemos irnos: el filtro a 0 y nos quedamos
        mov ax, 7
        xor cx, cx
        xor edx, edx
        call mac_request
        jmp residente2
residente:
        ; el packet driver en INT 60h (si no hay ya uno)
        mov ax, 0x3560
        int 0x21
        lea di, [bx+3]
        mov si, pd_int+3
        mov cx, 8
        repe cmpsb
        push cs
        pop es
        jne .instala
        mov dx, m_yahay
        call puts
        jmp salir1
.instala:
        mov dx, pd_int
        mov ax, 0x2560
        int 0x21
        mov dx, m_int60
        call puts
residente2:
        mov dx, m_resid
        call puts
        mov dx, (fin_residente - start + 0x100 + 15) / 16
        mov ax, 0x3100
        int 0x21
ioerr_main:
        mov dx, m_ioerr
error:
        call puts
salir1:
        mov ax, 0x4C01
        int 0x21

ioctl:
        mov word [rb+2], 0xFFFF
        mov ax, 0x4402
        mov bx, [h]
        mov cx, 14
        mov dx, rb
        int 0x21
        jc .r
        cmp word [rb+2], 0
        je .r
        stc
.r:
        ret

; ProtManEntry(ReqBlock dd, TargetDS dw): AX = estado
pm_call:
        mov word [rb+2], 0xFFFF
        push cs
        push word rb
        push word [pm_ds]
        call far [pm_entry]
        push cs
        pop ds
        ret

; buscar en la imagen de PROTOCOL.INI la sección HSPNDIS y su BINDINGS ->
; blist (cuántas y los nombres de 16); CF si no hay
buscar_bindings:
        lfs si, [pm_img]
.mod:
        mov eax, fs
        shl eax, 16
        mov ax, si
        test eax, eax
        jz .no
        lea bx, [si+8]
        mov di, cct_name
        call igual16
        je .esta
        mov eax, [fs:si]
        mov si, ax
        shr eax, 16
        mov fs, ax
        jmp .mod
.esta:
        lea di, [si+24]             ; la primera clave
.kw:
        lea bx, [di+8]
        push di
        mov di, n_bindings
        call igual16
        pop di
        je .bind
        mov eax, [fs:di]
        test eax, eax
        jz .no
        mov di, ax                  ; (las claves van en el mismo segmento)
        jmp .kw
.bind:
        mov cx, [fs:di+24]
        cmp cx, 4
        jbe .n
        mov cx, 4
.n:
        jcxz .no
        mov [blist], cx
        lea bx, [di+26]
        mov di, blist+2
.p:
        cmp word [fs:bx], 1         ; cadena
        jne .no
        push bx
        push di
        add bx, 4
        push cx
        mov cx, 15
.c:
        mov al, [fs:bx]
        mov [di], al
        test al, al
        jz .cz
        inc bx
        inc di
        loop .c
.cz:
        pop cx
        pop di
        pop bx
        add di, 16
        mov ax, [fs:bx+2]
        add ax, 4
        add bx, ax
        loop .p
        clc
        ret
.no:
        stc
        ret

; ¿FS:BX (ASCIZ) igual a DS:DI (ASCIZ)? (como mucho 16) -> ZF
igual16:
        push cx
        mov cx, 16
.l:
        mov al, [fs:bx]
        cmp al, [di]
        jne .r
        test al, al
        jz .r
        inc bx
        inc di
        loop .l
        cmp al, al
.r:
        pop cx
        ret

; DX = texto, AX = estado: «texto XXXX» y salto de línea
puts_estado:
        push ax
        call puts
        pop ax
        push ax
        call hex16
        call crlf
        pop ax
        ret
puts:
        push ax
        mov ah, 9
        int 0x21
        pop ax
        ret
puts0:
        lodsb
        test al, al
        jz .r
        call putc
        jmp puts0
.r:
        ret
putc:
        push dx
        push ax
        mov dl, al
        mov ah, 2
        int 0x21
        pop ax
        pop dx
        ret
crlf:
        mov al, 13
        call putc
        mov al, 10
        jmp putc
hex16:
        push ax
        mov al, ah
        call hex8
        pop ax
hex8:
        push ax
        shr al, 4
        call hex4
        pop ax
hex4:
        and al, 15
        add al, '0'
        cmp al, '9'
        jbe .p
        add al, 7
.p:
        jmp putc

m_titulo    db "HSPNDIS 0.1 - protocolo NDIS 2 de Hispa-DOS", 13, 10, "$"
m_noprotman db "No hay gestor de protocolos (PROTMAN$): falta PROTMAN en el CONFIG.SYS", 13, 10, "$"
m_ioerr     db "PROTMAN$ no contesta a la consulta (IOCTL)", 13, 10, "$"
m_nobind    db "PROTOCOL.INI: falta la sección [HSPNDIS] con BINDINGS = <la tarjeta>", 13, 10, "$"
m_reg       db "Registro (RegisterModule): $"
m_bas       db "Unión (BindAndStart): $"
m_bind      db "Bind con la tarjeta: $"
m_fallan    db "No se ha podido unir: $"
m_mac       db "Tarjeta unida. Dirección: $"
m_flags     db "Service flags del MAC: $"
m_open      db "Abrir la tarjeta (OpenAdapter): $"
m_filtro    db "Filtro de paquetes: $"
m_tx        db "Envío (TransmitChain): $"
m_rx        db "Tramas recibidas: $"
m_rxarp     db ", de ellas ARP: $"
m_yahay     db "Ya hay un packet driver en INT 60h: no se instala", 13, 10, "$"
m_int60     db "Packet driver en INT 60h.", 13, 10, "$"
m_resid     db "HSPNDIS queda residente.", 13, 10, "$"
n_protman   db "PROTMAN$", 0
n_bindings  db "BINDINGS", 0
prueba      db 0
h           dw 0
pm_entry    dd 0
pm_ds       dw 0
pm_img      dd 0
rb          times 14 db 0
blist       dw 0
            times 4*16 db 0
fallan      times 32 db 0
txd         times 16 db 0
arp_trama:
            db 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF   ; a todos
            times 6 db 0                            ; de: nosotros
            db 0x08, 0x06                           ; ARP
            db 0x00, 0x01, 0x08, 0x00, 6, 4, 0x00, 0x01
            times 6 db 0                            ; nuestra MAC
            db 10, 0, 2, 15                         ; nuestra IP
            times 6 db 0
            db 10, 0, 2, 2                          ; ¿quién tiene 10.0.2.2?
            times 18 db 0                           ; hasta 60
            align 16
            times 512 db 0
pila_fin:
fin_total:
