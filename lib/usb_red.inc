; =============================================================================
;  Hispa-DOS - HSPUSB: red por USB, clase estándar CDC-ECM («Ethernet
;  Networking Control Model»)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Para los adaptadores USB-Ethernet que ofrecen la clase estándar (los
;  Realtek RTL8152/RTL8153 la traen en su configuración 2; la 1 es su modo
;  propio) y -device usb-net de QEMU (RNDIS en la 1, CDC-ECM en la 2).
;  Según «USB CDC 1.2» y «Subclass Specification for Ethernet Control Model
;  Devices 1.2» (usb.org):
;   - interfaz de comunicaciones 02h/06h, con el descriptor funcional
;     Ethernet (24h, subtipo 0Fh): iMACAddress es la cadena con la MAC (12
;     cifras hexadecimales);
;   - interfaz de datos 0Ah: sin extremos en su alternativa 0 y con dos bulk
;     (entrada y salida) en la 1. Cada transferencia es una trama Ethernet
;     entera (sin CRC), que acaba en un paquete corto;
;   - SET_ETHERNET_PACKET_FILTER (43h): qué tramas deja pasar el adaptador.
;
;  Aquí hay dos partes:
;   1) inicialización (.init, se descarta): reconocer el aparato mirando
;      TODAS sus configuraciones (role_net, net_parse), ponerlo en marcha
;      (net_setup) y dejar en la memoria el bloque residente;
;   2) el bloque residente (.red): solo se queda si hay adaptador. net_setup
;      lo copia a la zona de trabajo y lo ajusta (ver MCALL en hspusb.asm).
;
;  Hacia arriba es una «tarjeta» más de HSPRED (el packet driver), que lo
;  encuentra por INT 2Fh AX=DB00h:
;      -> AL = FFh, BX = 5552h («UR»), ES:DI = bloque de datos:
;         +0   la MAC (6 octetos)
;         +6   VID y PID (dos palabras)
;         +10  velocidad USB (1 plena, 3 alta, 4 súper)
;         +11  clase de la controladora (0 UHCI, 1 OHCI, 2 EHCI, 3 xHCI)
;         +12  enlace: FFh = no se sabe (no se lee la tubería de avisos)
;         +13  versión de este bloque (1)
;         +14  enviar (llamada lejana): DS:SI = trama, CX = octetos (14 a
;              1514) -> CF = 1 si no ha salido. Conserva los registros.
;         +18  receptor (lo escribe HSPRED; 0:0 = nadie): llamada lejana
;              con ES:DI = trama, CX = octetos, interrupciones cerradas;
;              tiene que conservar SS:SP
;         +22  tramas recibidas con error o descartadas; +24, envíos fallidos
;
;  Recepción: no hay interrupción. La tubería bulk de entrada se lleva como
;  una tubería más (PIPE_NET) con las rutinas de las de interrupción
;  (pipe_queue y las de cada controladora), con un búfer de una trama: se
;  deja pedida y se mira en cada tic del reloj (n_pump, desde INT 08h, con
;  «busy» tomado y la pila de HSPUSB). Si ha llegado una trama se entrega a
;  HSPRED, se pide la siguiente y se la espera hasta 2 ms (las tramas vienen
;  a ráfagas: la ventana de TCP de lib/red.inc son 4), como mucho 8 por tic.
;  En el xHCI, la llegada la avisa el anillo de eventos (n_event).
;  Envío: síncrono, como las transferencias de la memoria USB (una cada vez
;  en todo el driver): si HSPUSB está ocupado (busy), el envío falla y el
;  programa lo repite. Desde el receptor (dentro de n_pump) sí se puede
;  enviar.
;
;  Lo que NO hace: CDC-NCM, RNDIS, los modos propios (RTL8152/8153 nativo,
;  AX88179...), la tubería de avisos (enlace y velocidad), la
;  multidifusión, y conectar o quitar el adaptador con el driver cargado.
; =============================================================================

N_UTD       equ 24                  ; UHCI: TD de la recepción (24 x 64 = 1536)
N_RXFS      equ 1536                ; lo que se pide en cada recepción: un múltiplo
N_RXHS      equ 2048                ; del paquete (64; 512 o 1024), mayor que la trama
N_TICK      equ 8                   ; tramas por tic, como mucho
N_WAIT      equ 16                  ; espera de la trama siguiente (microtramas: 2 ms)
N_SBUF      equ 2048 + 1536         ; el búfer de los avisos (64), tras los de recepción y envío

section .init
; ---- reconocer el adaptador ----
; El aparato que se enumera (descriptor de su primera configuración en DESC,
; ya recorrido por parse_config): ¿es de red? Se miran todas sus
; configuraciones, porque los adaptadores traen la clase estándar en otra
; que la primera (la primera es RNDIS o el modo propio del fabricante).
; -> AL = 0 no es de red; 1 tomado (CDC-ECM; d_role = 9); 2 es de red pero
;    no se usa (d_role = 10 sin CDC-ECM, 11 ya hay otro, 6 ha fallado)
role_net:
        mov byte [np_seen], 0
        mov byte [np_idx], 0
        call net_parse
        jnc .found
        ; lo que ya tiene otro uso (teclado, ratón, mando, memoria) no se
        ; sigue mirando
        mov al, [p_kbd]
        or al, [p_mou]
        or al, [p_joy]
        or al, [p_msc]
        jnz .no
.cl:
        inc byte [np_idx]
        mov al, [np_idx]
        cmp al, [d_ncfg]
        jae .none
        ; descriptor de la configuración AL: 9 octetos y después entero
        movzx eax, al
        shl eax, 16
        or eax, 0x02000680
        mov [ctl_setup], eax
        mov dword [ctl_setup+4], 0x00090000
        call control
        jc .cl
        mov bx, DESC
        call zone
        mov ax, [cs:bx+2]
        cmp ax, 255
        jbe .tl
        mov ax, 255
.tl:
        mov [ctl_setup+6], ax
        call control
        jc .cl
        call net_parse
        jc .cl
        call net_iftab              ; /INFO: las interfaces de esta configuración
.found:
        cmp word [n_blk], 0
        je .set
        mov byte [d_role], 11       ; ya hay uno: solo se lleva el primero
        mov al, 2
        ret
.set:
        call net_setup
        jc .bad
        mov byte [d_role], 9
        mov al, 1
        ret
.bad:
        mov byte [d_role], 6
        mov al, 2
        ret
.none:
        cmp byte [np_seen], 0
        jne .unsup
        call net_known
        jc .unsup
.no:
        xor al, al
        ret
.unsup:
        ; de red, pero sin CDC-ECM: que se sepa cuál es (también sin /INFO)
        mov si, msg_netno
        call puts
        mov ax, [d_vid]
        call puthex4
        mov dl, ':'
        call putc
        mov ax, [d_pid]
        call puthex4
        mov si, msg_netno2
        call puts
        mov byte [d_role], 10
        mov al, 2
        ret

; Recorrer el descriptor de configuración que hay en DESC ([ctl_setup+6]
; octetos) buscando CDC-ECM -> CF = 0 si lo trae completo (np_*); si no,
; CF = 1 y [np_seen] = 1 si trae alguna otra clase de red (NCM, RNDIS)
net_parse:
        mov bx, DESC
        call zone
        mov si, bx
        movzx cx, byte [ctl_setup+6]
        add cx, bx
        mov al, [cs:bx+5]
        mov [np_cfg], al            ; bConfigurationValue
        xor ax, ax
        mov [np_mode], al
        mov [np_comm_ok], al
        mov [np_done], al
        mov [np_imac], al
        mov [np_int], al
        mov byte [np_union], 0xFF
.l:
        cmp si, cx
        jae .end
        mov al, [cs:si]
        cmp al, 2
        jb .end
        mov ah, [cs:si+1]
        cmp ah, 4
        jne .cs
        ; ---- interfaz ----
        mov byte [np_mode], 0
        mov dx, [cs:si+5]           ; DL = clase, DH = subclase
        mov bl, [cs:si+7]           ; protocolo
        cmp dl, 0x02
        jne .ndc
        cmp dh, 0x06
        jne .ncm
        ; comunicaciones, CDC-ECM
%ifdef RED_SIN_ECM
        jmp .other                  ; (pruebas: como un adaptador sin CDC-ECM)
%endif
        mov al, [cs:si+2]
        mov [np_comm], al
        mov byte [np_comm_ok], 1
        mov byte [np_mode], 1
        jmp .n
.ncm:
        cmp dh, 0x0D                ; CDC-NCM
        je .other
        cmp dh, 0x0E                ; CDC-MBIM
        je .other
        cmp dx, 0x0202              ; RNDIS: 02/02/FF
        jne .n
        cmp bl, 0xFF
        je .other
        jmp .n
.ndc:
        cmp dl, 0xE0                ; RNDIS: E0/01/03
        jne .nef
        cmp dh, 0x01
        jne .n
        cmp bl, 0x03
        je .other
        jmp .n
.nef:
        cmp dl, 0xEF                ; RNDIS: EF/04/01
        jne .dat
        cmp dh, 0x04
        jne .n
        cmp bl, 0x01
        je .other
        jmp .n
.other:
        mov byte [np_seen], 1
        jmp .n
.dat:
        cmp dl, 0x0A                ; interfaz de datos
        jne .n
        cmp byte [np_comm_ok], 0
        je .n
        cmp byte [np_done], 0
        jne .n
        mov al, [cs:si+2]
        cmp byte [np_union], 0xFF   ; (si la de comunicaciones dice cuál es
        je .du                      ; la suya, tiene que ser ésa)
        cmp al, [np_union]
        jne .n
.du:
        mov [np_data], al
        mov al, [cs:si+3]
        mov [np_alt], al
        mov byte [np_in], 0
        mov byte [np_out], 0
        mov byte [np_mode], 2
        jmp .n
.cs:
        cmp ah, 0x24                ; descriptor funcional (CS_INTERFACE)
        jne .ep
        cmp byte [np_mode], 1
        jne .n
        mov ah, [cs:si+2]
        cmp ah, 0x0F                ; Ethernet: iMACAddress
        jne .un
        cmp al, 4
        jb .n
        mov al, [cs:si+3]
        mov [np_imac], al
        jmp .n
.un:
        cmp ah, 0x06                ; unión: la interfaz de datos
        jne .n
        cmp al, 5
        jb .n
        mov al, [cs:si+4]
        mov [np_union], al
        jmp .n
.ep:
        cmp ah, 5
        jne .n
        mov dl, [cs:si+2]
        mov dh, dl
        and dh, 0x0F
        shl dh, 1                   ; DCI = 2 * número (+1 si es de entrada)
        mov di, [cs:si+4]
        and di, 0x7FF
        mov al, [cs:si+3]
        and al, 3
        cmp byte [np_mode], 1
        jne .ed
        ; en la de comunicaciones: el de interrupción de entrada (avisos)
        cmp al, 3
        jne .n
        test dl, 0x80
        jz .n
        inc dh
        mov [np_int], dh
        mov [np_int_mps], di
        mov al, [cs:si+6]
        mov [np_int_ival], al
        jmp .n
.ed:
        cmp byte [np_mode], 2
        jne .n
        cmp al, 2                   ; bulk
        jne .n
        test dl, 0x80
        jz .eo
        inc dh
        mov [np_in], dh
        mov [np_mps_in], di
        jmp .ec
.eo:
        mov [np_out], dh
        mov [np_mps_out], di
.ec:
        cmp byte [np_in], 0
        je .n
        cmp byte [np_out], 0
        je .n
        mov byte [np_done], 1       ; esta alternativa tiene los dos
        mov byte [np_mode], 0
.n:
        movzx ax, byte [cs:si]
        add si, ax
        jmp .l
.end:
        cmp byte [np_comm_ok], 0
        je .no
        mov byte [np_seen], 1       ; de red es
        cmp byte [np_done], 0
        je .no
        cmp byte [np_imac], 0
        je .no
        cmp word [np_mps_in], 0
        je .no
        cmp word [np_mps_out], 0
        je .no
        clc
        ret
.no:
        stc
        ret

; /INFO: clase/subclase/protocolo de las interfaces de la configuración
; que hay en DESC (la que trae CDC-ECM), en vez de las de la primera
net_iftab:
        mov bx, DESC
        call zone
        mov si, bx
        movzx cx, byte [ctl_setup+6]
        add cx, bx
        mov byte [n_if], 0
.l:
        cmp si, cx
        jae .r
        mov al, [cs:si]
        cmp al, 2
        jb .r
        cmp byte [cs:si+1], 4
        jne .n
        cmp byte [cs:si+3], 0
        jne .n
        movzx di, byte [n_if]
        cmp di, IF_MAX * 3
        jae .n
        mov al, [cs:si+5]
        mov [if_tab+di], al
        mov ax, [cs:si+6]
        mov [if_tab+di+1], ax
        add byte [n_if], 3
.n:
        movzx ax, byte [cs:si]
        add si, ax
        jmp .l
.r:
        ret

; ¿es [d_vid]:[d_pid] un adaptador USB-Ethernet conocido de los que no traen
; CDC-ECM (solo su modo propio)? CF si sí. Para poder decir «es de red y aún
; no se admite» en vez de «sin uso».
net_known:
        mov si, nk_tab
.l:
        mov ax, [si]
        test ax, ax
        jz .no
        add si, 4
        cmp ax, [d_vid]
        jne .l
        mov ax, [si-2]
        cmp ax, [d_pid]
        jne .l
        stc
        ret
.no:
        clc
        ret
nk_tab      dw 0x0BDA, 0x8150, 0x0BDA, 0x8152, 0x0BDA, 0x8153   ; Realtek
            dw 0x0BDA, 0x8155, 0x0BDA, 0x8156
            dw 0x0B95, 0x1720, 0x0B95, 0x1780, 0x0B95, 0x7720   ; ASIX AX88172/178/772
            dw 0x0B95, 0x772A, 0x0B95, 0x772B, 0x0B95, 0x7E2B
            dw 0x0B95, 0x1790, 0x0B95, 0x178A                   ; ASIX AX88179/178A
            dw 0x05AC, 0x1402                                   ; Apple (AX88772)
            dw 0x0424, 0x9500, 0x0424, 0x9730, 0x0424, 0xEC00   ; SMSC/Microchip LAN95xx
            dw 0x0424, 0x7500, 0x0424, 0x7800, 0x0424, 0x7850   ; LAN75xx/78xx
            dw 0x0A46, 0x9601, 0x0FE6, 0x9700                   ; Davicom DM9601
            dw 0x2001, 0x1A02, 0x2001, 0x4A00                   ; D-Link DUB-E100/DUB-1312
            dw 0x13B1, 0x0018, 0x13B1, 0x0041                   ; Linksys USB300M/USB3GIG
            dw 0x17EF, 0x304F, 0x17EF, 0x3062, 0x17EF, 0x7205   ; Lenovo (RTL8153)
            dw 0x17EF, 0x720C
            dw 0x045E, 0x07AB, 0x045E, 0x07C6, 0x045E, 0x0927   ; Microsoft (RTL8152/8153)
            dw 0x04E8, 0xA101                                   ; Samsung (RTL8153)
            dw 0x2357, 0x0601, 0x2357, 0x0602                   ; TP-Link UE300/UE330
            dw 0

; reservar CX octetos (alineación DX) que no crucen de página -> BX; CF si
; no caben
zalloc_np:
        push eax
        mov eax, [ztop]
        call zalloc
        jc .r
        push ax
        mov ax, bx
        sub ax, [mem_off]           ; (la zona empieza en una página física)
        and ax, 0x0FFF
        add ax, cx
        cmp ax, 0x1000
        pop ax
        jbe .ok
        mov [ztop], eax             ; cruza: se devuelve y se pide en página nueva
        mov dx, 4096
        call zalloc
        jmp .r
.ok:
        clc
.r:
        pop eax
        ret

; ---- poner en marcha el adaptador (np_*; aparato actual) ----
; -> CF y [fail_msg] si no se puede (la memoria reservada se devuelve)
net_setup:
        mov eax, [ztop]
        mov [ns_ztop], eax
        mov word [fail_msg], msg_nomem
        ; el bloque residente: de red_hold a la zona, ajustado a su sitio
        mov cx, RED_LEN
        mov dx, 2
        call zalloc
        jc .fail
        mov [ns_blk], bx
        push es
        push cs
        pop es
        mov si, red_hold
        mov di, bx
        mov cx, RED_LEN
        cld
        rep movsb
        pop es
        mov si, red_fix             ; las llamadas a .text (MCALL)
.fx:
        cmp si, red_fix_end
        jae .fxd
        mov di, [si]
        add di, bx
        sub [di], bx
        add si, 2
        jmp .fx
.fxd:
        mov bp, bx                  ; BP = el bloque, hasta el final
        cmp byte [cur_type], HCT_XHCI
        jne .cl
        ; xHCI: tres anillos (cada uno en su página: alineados a 100h)
        mov cx, 0x300
        mov dx, 0x100
        call zalloc
        jc .fail
        sub bx, [mem_off]
        mov [ds:bp+n_rxring], bx
        add bx, 0x100
        mov [ds:bp+n_txring], bx
        add bx, 0x100
        mov [ds:bp+n_sring], bx
        jmp .buf
.cl:
        ; clásicas: la cola y el TD de sus dos tuberías, seguidas (y los TD
        ; de la recepción en la UHCI). n_pblk: como si fueran las tuberías
        ; PIPE_NET y PIPE_NST de una tabla que empezara en la 0 (c_pipe_blk).
        mov cx, 2 * PIPE_BLK
        mov dx, 32
        call zalloc_np
        jc .fail
        sub bx, PIPE_NET * PIPE_BLK
        mov [n_pblk], bx
        cmp byte [cur_type], HCT_UHCI
        jne .buf
        mov cx, N_UTD * 16
        mov dx, 16
        call zalloc
        jc .fail
        mov [ds:bp+n_utd], bx
.buf:
        ; búferes: 2048 de recepción y, detrás, 1536 de envío y 64 de los
        ; avisos. Alineados a 2048, ninguno cruza de página.
        mov cx, N_SBUF + 64
        mov dx, 2048
        call zalloc
        jc .fail
        mov [n_rxbuf], bx
        add bx, 2048
        mov [ds:bp+n_txbuf], bx
        ; SET_CONFIGURATION de la configuración que trae CDC-ECM
        movzx eax, byte [np_cfg]
        shl eax, 16
        or eax, 0x0900
        xor edx, edx
        call ctl_req
        mov word [fail_msg], msg_cfg
        jc .fail
        ; la MAC: la cadena iMACAddress, 12 cifras hexadecimales en UTF-16
        movzx eax, byte [np_imac]
        shl eax, 16
        or eax, 0x03000680
        mov edx, 0x001A0409         ; español no hay: inglés (0409h), 26 octetos
        call ctl_req
        mov word [fail_msg], msg_netmac
        jc .fail
        mov bx, DESC
        call zone
        cmp byte [cs:bx], 26
        jb .fail
        cmp byte [cs:bx+1], 3
        jne .fail
        lea si, [bx+2]
        mov bp, [ns_blk]            ; (las peticiones de control gastan EBP)
        lea di, [bp+n_mac]
        mov cx, 6
.mac:
        call .hex
        jc .fail
        shl al, 4
        mov ah, al
        call .hex
        jc .fail
        or al, ah
        mov [di], al
        inc di
        loop .mac
        ; la interfaz de datos, a la alternativa que tiene los extremos
        cmp byte [np_alt], 0
        je .noalt
        movzx eax, byte [np_alt]
        shl eax, 16
        or eax, 0x0B01
        movzx edx, byte [np_data]
        call ctl_req
        mov word [fail_msg], msg_cfg
        jc .fail
.noalt:
        cmp byte [cur_type], HCT_XHCI
        jne .cfgd
        ; xHCI: Configure Endpoint con los dos bulk
        call clear_in
        mov bx, IN_CTX
        call zone
        mov eax, 1
        movzx ecx, byte [np_in]
        bts eax, ecx
        mov cl, [np_out]
        bts eax, ecx
        mov [cs:bx+4], eax
        add bx, [ctx_sz]
        movzx eax, byte [np_in]
        cmp al, [np_out]
        jae .mx
        mov al, [np_out]
.mx:
        call slot_fill
        mov byte [ep_ival], 0
        mov dword [ep_d4], 1024
        mov bp, [ns_blk]
        mov al, [np_in]
        mov dx, [ds:bp+n_rxring]
        mov cx, [np_mps_in]
        mov ah, 6                               ; bulk IN
        call set_ep
        mov al, [np_out]
        mov dx, [ds:bp+n_txring]
        mov cx, [np_mps_out]
        mov ah, 2                               ; bulk OUT
        call set_ep
        call cfg_ep_cmd
        mov word [fail_msg], msg_epcfg
        jc .fail
.cfgd:
        ; SET_ETHERNET_PACKET_FILTER: lo dirigido a esta tarjeta y la
        ; difusión. Es opcional en la norma (si lo rechaza no pasa nada),
        ; pero hay adaptadores que no dan enlace ni tramas hasta recibirlo.
        mov eax, 0x000C4321
        movzx edx, byte [np_comm]
        call ctl_req
        ; los datos del bloque
        mov bp, [ns_blk]
        mov ax, [d_vid]
        mov [ds:bp+n_vid], ax
        mov ax, [d_pid]
        mov [ds:bp+n_pid], ax
        mov al, [speed]
        mov [ds:bp+n_speed], al
        mov al, [cur_type]
        mov [ds:bp+n_hct], al
        lea ax, [bp+n_send]
        mov [ds:bp+n_sendp], ax
        mov [ds:bp+n_sendp+2], cs
        mov al, [np_out]
        mov [ds:bp+n_odci], al
        mov ax, [np_mps_out]
        mov [ds:bp+n_omps], ax
        cmp byte [cur_type], HCT_XHCI
        jne .ns
        mov al, [cur_slot]
        mov [ds:bp+n_slot], al
.ns:
        ; la tubería de recepción
        mov al, [cur_pool]
        mov [ip_pool+PIPE_NET], al
        mov al, [np_in]
        mov [ip_dci+PIPE_NET], al
        mov ax, [np_mps_in]
        mov [ip_mps+PIPE_NET*2], ax
        mov ax, N_RXHS
        cmp byte [speed], 3
        jae .len
        mov ax, N_RXFS
.len:
        mov [ip_len+PIPE_NET*2], ax
        mov bx, PIPE_NET
        call pipe_ready             ; clásicas: su cola, a la cadena periódica
        cmp byte [cur_type], HCT_EHCI
        jne .on
        ; EHCI: la cola va en la lista periódica (las mismas rutinas que el
        ; teclado). Sin lista no hay tubería...
        movzx si, byte [cur_hc]
        shl si, 2
        mov word [fail_msg], msg_nomem
        cmp dword [hc_fl+si], 0
        je .fail
        ; ...y, de alta velocidad, que se atienda en las 8 microtramas (un
        ; paquete de 512 en cada una), no solo en la primera
        cmp byte [speed], 3
        jb .on
        mov bx, PIPE_NET
        call c_pipe_blk
        or byte [di+8], 0xFF
.on:
        call net_status             ; la tubería de avisos, si se puede
        mov bp, [ns_blk]
        mov [n_blk], bp             ; desde ahora, en marcha
        clc
        ret
.fail:
        mov eax, [ns_ztop]
        mov [ztop], eax
        mov word [n_pblk], 0
        stc
        ret
; una cifra hexadecimal de la cadena (UTF-16) -> AL; CF si no lo es
.hex:
        mov al, [cs:si]
        add si, 2
        sub al, '0'
        jc .hno
        cmp al, 9
        jbe .hok
        and al, 0xDF                ; a mayúsculas ('A' - '0' = 11h)
        sub al, 7
        cmp al, 10
        jb .hno
        cmp al, 15
        ja .hno
.hok:
        clc
        ret
.hno:
        stc
        ret

; La tubería de avisos del adaptador (el extremo de interrupción de su
; interfaz de comunicaciones): por ella dice si hay enlace. No es
; imprescindible: si no la tiene o algo falla, se queda sin ella (enlace:
; no se sabe) y lo demás sigue igual.
net_status:
        cmp byte [np_int], 0
        je .r
        mov cx, [np_int_mps]
        test cx, cx
        jz .r
        cmp cx, 64
        ja .r
        cmp byte [cur_type], HCT_XHCI
        jne .cl
        ; xHCI: otro Configure Endpoint, que añade el extremo (si la
        ; controladora lo rechaza, los dos bulk se quedan como estaban)
        call clear_in
        mov bx, IN_CTX
        call zone
        mov eax, 1
        movzx ecx, byte [np_int]
        bts eax, ecx
        mov [cs:bx+4], eax
        add bx, [ctx_sz]
        movzx eax, byte [np_in]     ; entradas de contexto: el mayor de los tres
        cmp al, [np_out]
        jae .m1
        mov al, [np_out]
.m1:
        cmp al, [np_int]
        jae .m2
        mov al, [np_int]
.m2:
        call slot_fill
        mov al, [np_int_ival]
        call calc_ival
        mov [ep_ival], al
        movzx eax, word [np_int_mps]
        mov edx, eax
        shl edx, 16
        or eax, edx
        mov [ep_d4], eax
        mov bp, [ns_blk]
        mov al, [np_int]
        mov dx, [ds:bp+n_sring]
        mov cx, [np_int_mps]
        mov ah, 7                               ; interrupción IN
        call set_ep
        call cfg_ep_cmd
        jc .r
.cl:
        mov al, [cur_pool]
        mov [ip_pool+PIPE_NST], al
        mov al, [np_int]
        mov [ip_dci+PIPE_NST], al
        mov ax, [np_int_mps]
        mov [ip_mps+PIPE_NST*2], ax
        mov [ip_len+PIPE_NST*2], ax ; se pide un paquete: nunca llega de más
        mov bx, PIPE_NST
        call pipe_ready
        mov bp, [ns_blk]
        mov byte [ds:bp+n_son], 1
.r:
        ret

; Al acabar la inicialización, si hay adaptador: decirlo y atender INT 2Fh
; (AX=DB00h), por donde lo encuentra HSPRED, e INT 16h (ver n_int16)
net_ready:
        mov bp, [n_blk]
        test bp, bp
        jz .r
        mov si, msg_netok
        call puts
        lea si, [bp+n_mac]
        mov cx, 6
.m:
        mov al, [si]
        call puthex2
        inc si
        dec cx
        jz .md
        mov dl, ':'
        call putc
        jmp .m
.md:
        mov si, msg_netok2
        call puts
        push es
        mov ax, 0x352F
        int 0x21
        mov [ds:bp+n_old2f], bx
        mov [ds:bp+n_old2f+2], es
        pop es
        lea dx, [bp+n_int2f]
        mov ax, 0x252F
        int 0x21
        push es
        mov ax, 0x3516
        int 0x21
        mov [ds:bp+n_old16], bx
        mov [ds:bp+n_old16+2], es
        pop es
        lea dx, [bp+n_int16]
        mov ax, 0x2516
        int 0x21
.r:
        ret

np_seen     db 0                    ; se ha visto alguna interfaz de red
np_idx      db 0
np_mode     db 0                    ; 1 dentro de la de comunicaciones, 2 de la de datos
np_comm_ok  db 0
np_done     db 0
np_cfg      db 0
np_comm     db 0
np_data     db 0
np_union    db 0
np_alt      db 0
np_imac     db 0
np_in       db 0
np_out      db 0
np_int      db 0                    ; extremo de los avisos (0 = no tiene)
np_int_ival db 0
            align 2
np_mps_in   dw 0
np_mps_out  dw 0
np_int_mps  dw 0
ns_blk      dw 0
ns_ztop     dd 0
msg_netok   db "Adaptador de red USB preparado (CDC-ECM), MAC ", 0
msg_netok2  db ": cargue HSPRED.", 13, 10, 0
msg_netno   db "Adaptador de red USB ", 0
msg_netno2  db ": aún no se admite (no ofrece la clase estándar CDC-ECM).", 13, 10, 0
msg_netmac  db "el adaptador de red no da su dirección MAC.", 13, 10, 0

; las posiciones de las llamadas de .red a .text (las apunta MCALL)
red_fix:

; =============================================================================
;  El bloque residente (.red). Se ejecuta donde lo deja net_setup, con el CS
;  del driver. DI (o BX) = principio del bloque en todas sus rutinas.
; =============================================================================
section .red
        jmp near n_pump             ; +0: desde INT 08h
        jmp near n_event            ; +3: evento de transferencia del xHCI

; ---- sondeo, desde INT 08h (busy tomado, pila propia, A20 y modo irreal) ----
n_pump:
        pushad
        push ds
        push es
        push cs
        pop ds
        mov di, [n_blk]
        call n_stat                 ; los avisos (enlace)
        mov byte [di+n_cnt], N_TICK
.l:
        mov al, [di+n_st]
        cmp al, 2
        je .have
        cmp al, 3
        je .err
        cmp al, 1
        je .pend
        ; libre: pedir una trama, si HSPRED está escuchando
        cmp dword [di+n_rcv], 0
        je .r
        call n_queue
        mov byte [di+n_st], 1
.pend:
        call n_check                ; clásicas: ¿ha llegado? (xHCI: n_event)
        cmp byte [di+n_st], 1
        jne .l
        ; todavía no. Si en este tic ya se ha entregado alguna, lo normal es
        ; que venga otra detrás: se la espera un poco
        cmp byte [di+n_cnt], N_TICK
        je .r
        MCALL hc_time
        mov [di+n_t0], ax
        mov cx, 20000               ; (por si el reloj no avanzara)
.w:
        MCALL usb_poll
        call n_check
        cmp byte [di+n_st], 1
        jne .l
        MCALL hc_time
        sub ax, [di+n_t0]
        and ax, 0x3FFF
        cmp ax, N_WAIT
        jae .r
        loop .w
        jmp .r
.err:
        inc word [di+n_rxerr]
        mov al, [ip_pool+PIPE_NET]
        MCALL dev_hc
        cmp byte [cur_type], HCT_XHCI
        jne .next
        ; xHCI: el extremo se queda parado tras un error; reanudarlo
        mov al, [ip_dci+PIPE_NET]
        mov bx, [di+n_rxring]
        lea si, [di+n_rxidx]
        call n_xrec
        jmp .next
.have:
        mov cx, [di+n_got]
        cmp cx, 14
        jb .drop
        cmp cx, 1514
        ja .drop
        cmp dword [di+n_rcv], 0
        je .next
        ; a HSPRED: ES:DI = trama, CX = octetos, interrupciones cerradas
        mov byte [di+n_deliv], 1
        push di
        push ds
        mov si, di
        mov di, [n_rxbuf]
        push cs
        pop es
        cli
        call far [si+n_rcv]
        sti
        pop ds
        pop di
        cld
        mov byte [di+n_deliv], 0
        jmp .next
.drop:
        inc word [di+n_rxerr]
.next:
        mov byte [di+n_st], 0
        dec byte [di+n_cnt]
        jnz .l
.r:
        pop es
        pop ds
        popad
        ret

; pedir la trama siguiente (DS = CS, DI = bloque)
n_queue:
        mov al, [ip_pool+PIPE_NET]
        MCALL dev_hc
        mov bx, PIPE_NET
        mov al, [cur_type]
        cmp al, HCT_XHCI
        je n_xq
        cmp al, HCT_UHCI
        je n_uq
        MCALL pipe_queue            ; OHCI y EHCI: como una de interrupción
        ret

; ¿ha llegado? (controladoras clásicas) -> n_st = 2 y n_got, o 3 si hubo error
n_check:
        mov al, [ip_pool+PIPE_NET]
        MCALL dev_hc
        mov al, [cur_type]
        cmp al, HCT_XHCI
        je .r
        mov bx, PIPE_NET
        cmp al, HCT_UHCI
        jne .oe
        call n_ucheck
        jmp .res
.oe:
        movzx si, al
        shl si, 1
        push di
        call [c_pc_tab+si]          ; (dirección absoluta: vale desde aquí)
        pop di
.res:
        jc .r                       ; sigue pendiente
        mov [di+n_got], ax
        mov byte [di+n_st], 2
        cmp ax, 0xFFFF
        jne .r
        mov byte [di+n_st], 3
.r:
        ret

; ---- xHCI ----
; pedir la trama siguiente: un TRB normal en su anillo de entrada
n_xq:
        mov bx, [n_rxbuf]
        mov cx, [ip_len+PIPE_NET*2]
        mov dx, [di+n_rxring]
        lea si, [di+n_rxidx]
        mov al, [ip_dci+PIPE_NET]
; un TRB normal: BX = búfer (en CS), CX = octetos, DX = anillo (en la zona),
; SI = su índice y ciclo, AL = extremo (DCI); y el timbre. Conserva todo.
n_xput:
        pushad
        push ax
        MCALL phys
        movzx ecx, cx
        mov bx, dx
        xor edx, edx
        mov ebp, (1 << 10) | (1 << 5) | (1 << 2)    ; Normal, IOC, ISP
        MCALL put_trb
        pop ax
        mov ah, [di+n_slot]
        MCALL doorbell
        popad
        ret

; evento de transferencia en evt (desde hid_event): si es de la recepción
; o de los avisos del adaptador, se apunta (lo atenderá n_pump) y CF = 0; si
; no, CF = 1. Los del extremo de salida no son de aquí: los espera el envío
; (wait_xfer).
n_event:
        push di
        push ax
        mov di, [cs:n_blk]
        mov al, [cs:evt+15]
        cmp al, [cs:di+n_slot]      ; (0 si el adaptador no va por el xHCI)
        jne .no
        mov al, [cs:evt+14]
        and al, 0x1F
        cmp al, [cs:ip_dci+PIPE_NET]
        je .rx
        cmp byte [cs:di+n_son], 0
        je .no
        cmp al, [cs:ip_dci+PIPE_NST]
        jne .no
        ; avisos
        cmp byte [cs:di+n_sst], 1
        jne .eat
        mov byte [cs:di+n_sst], 3
        call .good
        jc .eat
        mov ax, [cs:ip_len+PIPE_NST*2]
        sub ax, [cs:evt+8]          ; menos lo que ha faltado por llegar
        mov [cs:di+n_sgot], ax
        mov byte [cs:di+n_sst], 2
        jmp .eat
.rx:
        cmp byte [cs:di+n_st], 1
        jne .eat
        mov byte [cs:di+n_st], 3
        call .good
        jc .eat
        mov ax, [cs:ip_len+PIPE_NET*2]
        sub ax, [cs:evt+8]
        mov [cs:di+n_got], ax
        mov byte [cs:di+n_st], 2
.eat:
        pop ax
        pop di
        clc
        ret
.no:
        pop ax
        pop di
        stc
        ret
; ¿transferencia correcta? (éxito, o paquete corto: lo normal) CF si no
.good:
        mov al, [cs:evt+11]
        cmp al, 1
        je .y
        cmp al, 13
        je .y
        stc
        ret
.y:
        clc
        ret

; ---- la tubería de avisos (interrupción): el enlace ----
; El adaptador avisa por ella de NETWORK_CONNECTION (A1h 00h, wValue = 1 si
; hay enlace, 0 si no) y de SPEED_CHANGE (A1h 2Ah, que no se usa). Se mira
; con cada sondeo. DS = CS, DI = bloque.
n_stat:
        cmp byte [di+n_son], 0
        je .r
        mov al, [ip_pool+PIPE_NST]
        MCALL dev_hc
        mov al, [di+n_sst]
        cmp al, 2
        je .got
        cmp al, 3
        je .err
        cmp al, 1
        jne .q
        cmp byte [cur_type], HCT_XHCI
        je .r                       ; (lo dirá n_event)
        mov bx, PIPE_NST
        movzx si, byte [cur_type]
        shl si, 1
        push di
        call [c_pc_tab+si]          ; (dirección absoluta: vale desde aquí)
        pop di
        jc .r                       ; sigue pendiente
        mov [di+n_sgot], ax
        cmp ax, 0xFFFF
        je .err
.got:
        mov byte [di+n_serr], 0
        cmp word [di+n_sgot], 8
        jb .q
        mov bx, [n_rxbuf]
        add bx, N_SBUF
        cmp word [bx], 0x00A1       ; aviso de clase a la interfaz, NETWORK_CONNECTION
        jne .q
        mov al, [bx+2]
        and al, 1
        mov [di+n_link], al
.q:
        mov byte [di+n_sst], 1
        cmp byte [cur_type], HCT_XHCI
        je .xq
        mov bx, PIPE_NST
        MCALL pipe_queue
.r:
        ret
.xq:
        mov bx, [n_rxbuf]
        add bx, N_SBUF
        mov cx, [ip_len+PIPE_NST*2]
        mov dx, [di+n_sring]
        lea si, [di+n_sidx]
        mov al, [ip_dci+PIPE_NST]
        jmp n_xput
.err:
        inc byte [di+n_serr]
        cmp byte [di+n_serr], 3
        jb .e1
        mov byte [di+n_son], 0      ; no hay manera: sin avisos
        mov byte [di+n_link], 0xFF  ; (enlace: no se sabe)
        ret
.e1:
        cmp byte [cur_type], HCT_XHCI
        jne .q
        mov al, [ip_dci+PIPE_NST]
        mov bx, [di+n_sring]
        lea si, [di+n_sidx]
        call n_xrec
        jmp .q

; reanudar un extremo parado: Reset Endpoint y Set TR Dequeue Pointer a
; donde va su anillo. AL = DCI, BX = anillo (en la zona), SI = su índice y
; ciclo. (Si no estaba parado, la controladora rechaza las órdenes y no
; pasa nada.)
n_xrec:
        pushad
        push di
        movzx ebp, byte [di+n_slot]
        shl ebp, 24
        movzx eax, al
        shl eax, 16
        or ebp, eax
        push ebp
        or ebp, 14 << 10            ; Reset Endpoint
        xor eax, eax
        xor edx, edx
        xor ecx, ecx
        push bx
        push si
        MCALL xhci_cmd
        pop si
        pop bx
        movzx ax, byte [si]
        shl ax, 4
        add bx, ax
        MCALL zone
        MCALL phys
        or al, [si+1]               ; estado del ciclo
        xor edx, edx
        xor ecx, ecx
        pop ebp
        or ebp, 16 << 10            ; Set TR Dequeue Pointer
        MCALL xhci_cmd
        pop di
        popad
        ret

; ---- UHCI: un TD por paquete (lib/usb_uhci.inc), así que la recepción de
; una trama son N_UTD seguidos, en profundidad, colgados de la cola de la
; tubería. Con el paquete corto del final la controladora deja la cola
; parada en ese TD (los demás se quedan sin usar). ----
n_uq:
        pushad
        MCALL c_xf_pipe             ; (xf_addr, xf_ep, xf_mps: los de la tubería BX)
        mov bp, di
        MCALL c_pipe_blk            ; DI = la cola de la tubería BX
        mov dword [di+4], 1         ; vacía mientras se preparan los TD
        mov bx, [n_rxbuf]
        MCALL phys
        mov edx, eax                ; EDX = búfer del paquete que toca
        mov si, [ds:bp+n_utd]
        mov cl, N_UTD
        mov ch, [ip_tog+PIPE_NET]
.t:
        mov eax, 1                  ; el último: fin
        cmp cl, 1
        je .lk
        lea bx, [si+16]
        MCALL phys
        or al, 4                    ; en profundidad: varios por trama
.lk:
        mov [si], eax
        movzx eax, word [xf_mps]
        dec eax
        shl eax, 21
        mov al, 0x69                ; IN
        movzx ebx, byte [xf_addr]
        shl ebx, 8
        or eax, ebx
        movzx ebx, byte [xf_ep]
        shl ebx, 15
        or eax, ebx
        movzx ebx, ch
        shl ebx, 19
        or eax, ebx
        mov [si+8], eax
        mov [si+12], edx
        ; activo, tres intentos, detectar el paquete corto
        mov dword [si+4], (1 << 23) | (3 << 27) | (1 << 29)
        movzx eax, word [xf_mps]
        add edx, eax
        xor ch, 1
        add si, 16
        dec cl
        jnz .t
        mov bx, [ds:bp+n_utd]
        MCALL phys
        mov [di+4], eax             ; a trabajar
        popad
        ret

; -> CF si la trama no ha llegado; si no, AX = octetos (FFFFh = error)
n_ucheck:
        push bx
        push cx
        push dx
        push si
        push ebp
        mov si, [di+n_utd]
        mov cx, N_UTD
        xor dx, dx                  ; octetos
        xor bl, bl                  ; TD completos
.l:
        mov eax, [si+4]
        test eax, 1 << 23
        jnz .pend
        test eax, 0x00760000        ; parado, búfer, desbordado, CRC/plazo, relleno
        jnz .err
        inc ax
        and ax, 0x7FF
        add dx, ax
        inc bl
        mov ebp, [si+8]
        shr ebp, 21
        inc bp
        and bp, 0x7FF
        cmp ax, bp
        jb .done                    ; paquete corto: fin de la trama
        add si, 16
        loop .l
.done:
        mov ax, dx
        jmp .tog
.err:
        mov ax, 0xFFFF
.tog:
        and bl, 1                   ; el testigo avanza con los TD completos
        xor [ip_tog+PIPE_NET], bl
        clc
        jmp .r
.pend:
        stc
.r:
        pop ebp
        pop si
        pop dx
        pop cx
        pop bx
        ret

; ---- enviar una trama (llamada LEJANA, de HSPRED) ----
; DS:SI = trama, CX = octetos (14 a 1514) -> CF = 1 si no ha salido.
; Conserva los registros y deja las interrupciones como estaban.
n_send:
        pushf
        pushad
        push ds
        push es
        push fs
        cld
        mov ax, cs
        mov es, ax
        mov bx, [cs:n_blk]
        mov byte [cs:bx+n_res], 0
        cmp cx, 14
        jb .out
        cmp cx, 1514
        ja .out
        cmp byte [cs:bx+n_deliv], 0
        jne .mine                   ; desde el receptor: ya estamos dentro
        cli
        cmp byte [cs:busy], 0
        jne .out                    ; HSPUSB está con otra cosa: que lo repita
        mov byte [cs:busy], 1
.mine:
        ; la trama, al búfer de envío; las cortas, rellenas hasta 60
        mov di, [cs:bx+n_txbuf]
        mov dx, cx
        rep movsb
        push cs
        pop ds
        mov cx, 60
        sub cx, dx
        jbe .np
        xor al, al
        rep stosb
        mov dx, 60
.np:
        ; una transferencia que acaba en un paquete lleno no se distingue
        ; del principio de una más larga: en vez del paquete vacío que pide
        ; la norma, un octeto más de relleno (lo que hace Linux en usbnet)
        push dx
        mov ax, dx
        xor dx, dx
        div word [bx+n_omps]
        test dx, dx
        pop dx
        jnz .nz
        mov byte [di], 0
        inc dx
.nz:
        mov [bx+n_txlen], dx
        mov di, bx
        cmp byte [di+n_deliv], 0
        jne .nest
        call n_enter
        jc .out
        call n_tx
        call n_leave
        jmp .out
.nest:
        sti
        call n_tx
.out:
        pop fs
        pop es
        pop ds
        mov bx, [cs:n_blk]
        mov al, [cs:bx+n_res]
        mov bp, sp
        and byte [bp+32], 0xFE      ; los indicadores guardados: CF = 0 si salió
        cmp al, 1
        je .ok
        or byte [bp+32], 1
.ok:
        popad
        popf
        retf

; la transferencia (DS = CS, DI = bloque; trama en n_txbuf, n_txlen octetos)
n_tx:
        mov al, [ip_pool+PIPE_NET]
        MCALL dev_hc
        cmp byte [cur_type], HCT_XHCI
        je .x
        ; clásicas: como «bulk» (lib/usb_hc.inc), con los datos del adaptador
        mov byte [bk_dir], 0
        mov bx, [di+n_txbuf]
        MCALL phys
        mov [xf_buf], eax
        mov ax, [di+n_txlen]
        mov [xf_len], ax
        mov al, [ip_pool+PIPE_NET]
        MCALL c_xf_dev
        mov word [xr_tmo], 40       ; 2 s
        mov al, [di+n_odci]
        shr al, 1
        mov [xf_ep], al
        mov ax, [di+n_omps]
        mov [xf_mps], ax
        mov al, [di+n_togo]
        mov [xr_tog], al
        movzx bx, byte [cur_type]
        shl bx, 1
        push di
        call [c_blk_tab+bx]         ; (dirección absoluta: vale desde aquí)
        pop di
        mov al, [xr_tog]            ; (mov no toca CF)
        mov [di+n_togo], al
        jc .bad
.ok:
        mov byte [di+n_res], 1
        ret
.bad:
        inc word [di+n_txerr]
        ret
.x:
        mov bx, [di+n_txbuf]
        mov cx, [di+n_txlen]
        mov dx, [di+n_txring]
        lea si, [di+n_txidx]
        mov al, [di+n_odci]
        call n_xput
        MCALL wait_xfer
        jnc .ok
        cmp byte [xf_code], 0xFF    ; sin respuesta: nada que reanudar
        je .bad
        mov al, [di+n_odci]
        mov bx, [di+n_txring]
        lea si, [di+n_txidx]
        call n_xrec
        jmp .bad

; Entrar a trabajar con las controladoras como lo hace INT 08h: pila propia
; (la suya: no entra mientras «busy» esté tomado), modo irreal y A20.
; «busy» ya lo ha tomado quien llama, con las interrupciones cerradas.
; DS = CS, DI = bloque -> CF = 1 si no se puede (modo V86 sin VCPI), con
; «busy» suelto otra vez. Las dos vuelven con DI = bloque.
n_enter:
        pop word [di+n_ret]
        mov [di+n_ss], ss
        mov [di+n_sp], sp
        mov ax, cs
        mov ss, ax
        mov sp, istack_top
        sti
        MCALL io_mode
        mov di, [n_blk]
        cmp byte [iomode], 1
        je .v86                     ; modo V86 con VCPI: sin modo irreal
        ja .no
        sgdt [di+n_gdtr]
        MCALL unreal
.v86:
        MCALL a20_open
        mov di, [n_blk]
        clc
        jmp [di+n_ret]
.no:
        cli
        mov ss, [di+n_ss]
        mov sp, [di+n_sp]
        mov byte [busy], 0
        stc
        jmp [di+n_ret]
; ...y salir (deja las interrupciones cerradas)
n_leave:
        pop word [di+n_ret]
        MCALL a20_restore
        mov di, [n_blk]
        cmp byte [iomode], 0
        jne .s
        cli
        lgdt [di+n_gdtr]
.s:
        cli
        mov ss, [di+n_ss]
        mov sp, [di+n_sp]
        mov byte [busy], 0
        jmp [di+n_ret]

; ---- INT 16h: sondear también cuando el programa mira el teclado ----
; Con el tic del reloj solo, una trama puede esperar 55 ms. Los programas de
; red de Hispa-DOS (lib/red.inc) miran el teclado (INT 16h, AH = 01h) en
; cada vuelta de sus esperas, por si se pulsa Esc: se aprovecha para mirar
; ahí también si ha llegado algo, y la respuesta se entrega enseguida. Solo
; mientras HSPRED esté escuchando; si HSPUSB está ocupado, nada.
n_int16:
        cmp ah, 0x01
        je .p
        cmp ah, 0x11
        je .p
.ch:
        ; a quien estuviera antes (sin gastar ningún registro)
        push dword 0
        push bp
        mov bp, sp
        push bx
        mov bx, [cs:n_blk]
        push dword [cs:bx+n_old16]
        pop dword [bp+2]
        pop bx
        pop bp
        retf
.p:
        push ds
        push di
        push cs
        pop ds
        cli
        mov di, [n_blk]
        cmp dword [di+n_rcv], 0
        je .x
        cmp byte [busy], 0
        jne .x
        mov byte [busy], 1
        pushad
        push es
        push fs
        call n_enter
        jc .y
        MCALL usb_poll
        mov di, [n_blk]
        call n_pump
        call n_leave
.y:
        pop fs
        pop es
        popad
.x:
        pop di
        pop ds
        jmp .ch

; ---- INT 2Fh AX=DB00h: ¿hay adaptador de red USB? ----
n_int2f:
        cmp ax, 0xDB00
        je .us
        ; a quien estuviera antes (sin gastar ningún registro)
        push dword 0
        push bp
        mov bp, sp
        push bx
        mov bx, [cs:n_blk]
        push dword [cs:bx+n_old2f]
        pop dword [bp+2]
        pop bx
        pop bp
        retf
.us:
        mov al, 0xFF
        mov bx, 0x5552              ; «UR»
        push cs
        pop es
        mov di, [cs:n_blk]
        add di, n_info
        iret

; ---- datos ----
            align 2
n_info:                             ; lo que ve HSPRED (ver arriba)
n_mac       times 6 db 0            ; +0
n_vid       dw 0                    ; +6
n_pid       dw 0                    ; +8
n_speed     db 0                    ; +10
n_hct       db 0                    ; +11
n_link      db 0xFF                 ; +12
            db 1                    ; +13 versión
n_sendp     dd 0                    ; +14 n_send
n_rcv       dd 0                    ; +18 el receptor de HSPRED
n_rxerr     dw 0                    ; +22
n_txerr     dw 0                    ; +24
n_got       dw 0                    ; octetos de la trama que ha llegado
n_txlen     dw 0
n_omps      dw 0                    ; extremo de salida: paquete máximo
n_txbuf     dw 0                    ; búfer de envío (en CS)
n_rxring    dw 0                    ; xHCI: anillos (en la zona)
n_txring    dw 0
n_utd       dw 0                    ; UHCI: los TD de la recepción (en CS)
n_sring     dw 0                    ; xHCI: anillo de los avisos
n_sgot      dw 0                    ; octetos del aviso que ha llegado
n_t0        dw 0
n_ss        dw 0
n_sp        dw 0
n_ret       dw 0
n_old2f     dd 0
n_old16     dd 0
n_gdtr      times 6 db 0
n_rxidx     db 0, 1                 ; xHCI: índice y ciclo de cada anillo
n_txidx     db 0, 1
n_sidx      db 0, 1
n_son       db 0                    ; hay tubería de avisos
n_sst       db 0                    ; avisos: 0 sin pedir, 1 pedido, 2 ha llegado, 3 error
n_serr      db 0                    ; errores seguidos
n_st        db 0                    ; recepción: 0 libre, 1 pedida, 2 ha llegado, 3 error
n_deliv     db 0                    ; dentro del receptor de HSPRED
n_cnt       db 0
n_res       db 0                    ; el envío ha salido
n_slot      db 0                    ; xHCI: slot del adaptador
n_odci      db 0                    ; extremo de salida
n_togo      db 0                    ; clásicas: su testigo de alternancia
red_end:
RED_LEN     equ red_end - red_start

section .init
red_fix_end:
