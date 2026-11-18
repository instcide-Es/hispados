; =============================================================================
;  HSPRED.COM - controlador de red de Hispa-DOS (residente, «packet driver»)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Uso:  HSPRED [/INT:xx] [/INFO] [/AUTO] [/USB] [/DEPURA[:archivo]] [/U] [/?]
;        (en AUTOEXEC.BAT; vale LH)
;        /INT:xx  vector del packet driver, en hexadecimal (60 a 80); si no
;                 se dice, el primero libre desde el 60h (el 67h, el de la
;                 memoria expandida, nunca)
;        /INFO    tarjeta, MAC, enlace, IRQ, vector, contadores y la
;                 configuración IP; también con el controlador ya cargado
;        /AUTO    silencioso si no hay tarjeta: sale sin avisar (errorlevel 1)
;                 en vez de imprimir el aviso. Pensada para el AUTOEXEC.BAT,
;                 para que no moleste en un equipo sin red. (/Q es lo mismo.)
;        /USB     usar el adaptador USB-Ethernet (el que lleva HSPUSB) aunque
;                 haya una tarjeta PCI de las que se saben usar
;        /U       descargarlo de la memoria (si ningún programa lo usa)
;        /DEPURA[:archivo]  lo que HSPRED escribe en la pantalla se añade
;                 además a un archivo (HSPRED.LOG si no se dice otro), y la
;                 puesta en marcha de la tarjeta se cuenta paso a paso (la
;                 Yukon). Con /INFO, para mandar el informe a quien depura.
;        /MEMORIA, /PUERTOS  (Yukon) los registros, por memoria o por los
;                 puertos, sin probar cuál contesta
;        /SONDEO  (Yukon) sin IRQ: mirar la tarjeta en cada tic del reloj
;                 (18 veces por segundo). Por si la interrupción no llega.
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
;     También la Intel 82574L («e1000e», -device e1000e): misma rutina.
;   - Realtek RTL8111/8168/8101/8169 («r8169»): hecha a partir de la
;     documentación pública de la familia; QEMU no la tiene, pero funciona en
;     un equipo de verdad (Toshiba NB250-10N, 10EC:8136, desde el 15/11/2026).
;     /INFO dice la revisión PCI y la familia del chip. Ver docs/RED.md.
;   - AMD PCnet-PCI II / PCnet-FAST III (1022:2000; -device pcnet): la de
;     VirtualBox por omisión y la «vlance» de VMware. Registros por puertos
;     en modo de 32 bits (DWIO), estilo de software 2.
;   - DEC 21140/21143 «Tulip» (1011:0009/0019; -device tulip): el «Adaptador
;     de red heredado» de Hyper-V. Registros por puertos, MAC de la SROM,
;     filtro perfecto con la trama de configuración.
;    Estas dos últimas solo se han probado en QEMU (no en VirtualBox, VMware
;    ni Hyper-V): ver docs/RED.md, «En una máquina virtual».
;    Su parte residente va en secciones «de quita y pon» (ver más abajo): la
;    de la tarjeta que no está no se queda en la memoria.
;   - Marvell Yukon 2 «Optima» (88E8059, 11AB:4381): la del HP Mini 5103.
;     A partir de sky2 de Linux; QEMU no la emula (ver su sección, .yukon).
;     También de quita y pon.
;  Y, si no hay ninguna de ésas, un adaptador USB-Ethernet con la clase
;  estándar CDC-ECM, que lleva HSPUSB.SYS (lib/usb_red.inc): HSPRED lo
;  encuentra por INT 2Fh AX=DB00h y es una tarjeta más, también de quita y
;  pon (sección .usb). No tiene interrupción: HSPUSB mira si han llegado
;  tramas en cada tic del reloj y las entrega a us_rx.
;  Las que tienen «bus master» necesitan búferes en memoria física continua:
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

; Secciones. «.text» es lo residente común a todas las tarjetas (y, por ahora,
; las cuatro primeras); «.init», la instalación, que se descarta. Entre las
; dos van las tarjetas «de quita y pon» (PCnet, Tulip), cada una en su
; sección: todas se ensamblan para la MISMA dirección (ov_base, justo detrás
; de la pila: vfollows=.text) y la instalación deja en ese sitio solo la de
; la tarjeta encontrada (la primera ya está en su sitio; la segunda la copia
; allí su instalación, tu_init). Así una tarjeta nueva no le cuesta ni un
; octeto residente a las demás. Ojo: lo residente de estas secciones no puede
; nombrar nada de «.init» (tools/initcheck.py solo vigila «.text»).
        section .text
        section .pcnet follows=.text align=1
        section .tulip follows=.pcnet vfollows=.text align=1 valign=1
        section .usb follows=.tulip vfollows=.text align=1 valign=1
        section .yukon follows=.usb vfollows=.text align=1 valign=1
        section .init follows=.yukon
        section .text

NH          equ 8                   ; manejadores (tipos de paquete)
PILA        equ 512                 ; pila de la interrupción
VERSION     equ 1

; tarjetas
C_NE        equ 1
C_8139      equ 2
C_E1000     equ 3
C_8168      equ 4
C_PCNET     equ 5
C_TULIP     equ 6
C_USB       equ 7                   ; adaptador USB con CDC-ECM, por HSPUSB
C_YUKON     equ 8                   ; Marvell Yukon 2 Optima (88E8059)

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
; PCnet: 4 descriptores de recepción y 4 de envío (16 octetos cada uno, estilo
; de software 2), el bloque de inicialización (28 octetos), 4 búferes de
; recepción y uno de envío (el mismo para los 4 descriptores)
PN_RXD      equ 0
PN_TXD      equ 64
PN_INIT     equ 128
PN_RXB      equ 160
PN_BUF      equ 1536
PN_TXB      equ PN_RXB + 4 * PN_BUF
PN_SIZE     equ PN_TXB + 1536
PN_RMD1     equ 0x8000FA00          ; OWN y BCNT = -1536 (los bits 12-15, a uno)
; Tulip: 4 descriptores de recepción y 2 de envío (16 octetos cada uno), 4
; búferes de recepción y uno de envío (que sirve también para la trama de
; configuración, de 192 octetos)
TU_RXD      equ 0
TU_TXD      equ 64
TU_RXB      equ 96
TU_BUF      equ 1536
TU_TXB      equ TU_RXB + 4 * TU_BUF
TU_SIZE     equ TU_TXB + 1536
; CSR6 (modo): PS (bit 18: puerto MII/SYM, el de la 21140), HBD (19), SF (21:
; guardar la trama entera antes de enviarla) y el bit 25, que la 21140 quiere
; a uno. Sin modo promiscuo: el filtro es el perfecto de la trama de
; configuración. Los bits ST (13) y SR (1) arrancan el envío y la recepción.
TU_CSR6     equ 0x022C0000
TU_ST       equ 0x00002000
TU_SR       equ 0x00000002
; CSR7 (interrupciones): NIS (16) y AIS (15), los dos resúmenes, con RI (6:
; trama recibida) y RU (7: la tarjeta se ha quedado sin descriptores)
TU_CSR7     equ 0x000180C0

; Yukon 2: los tres anillos de elementos de 8 octetos tienen que empezar en
; una página (4 KB): 64 elementos de estado, 16 de recepción (4 búferes: el
; elemento i lleva el búfer i & 3) y 16 de envío (un búfer). dma_seg queda
; una página por debajo del primer anillo, y los cinco búferes los reparte
; yk_layout por los huecos: el que hay entre lo residente y el primer anillo
; (sin usar hasta ahora: hasta 4 KB de relleno), los de detrás de los anillos
; y, si no caben, detrás del último. Ocupa de 9,6 a 13,6 KB (eran 10 a 14).
YK_ST       equ 0x1000
YK_NST      equ 64
YK_RX       equ 0x2000
YK_NRX      equ 16
YK_TX       equ 0x3000
YK_NTX      equ 16
YK_BUF      equ 1520                ; cada búfer (múltiplo de 16)
YK_SIZE     equ 0x2800              ; lo que se pide (con LH; luego se recorta)
YK_RXLEN    equ 1520                ; lo que cabe en cada búfer de recepción

start:
        jmp init

; =============================================================================
;  Entrada del packet driver (el vector apunta aquí; la firma, 3 más allá)
; =============================================================================
pkt_entry:
        jmp strict near pkt_main    ; 3 octetos (strict: NASM 3 lo acortaría)
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
dma_z0      dw 0                    ; dónde empiezan (dentro de dma_seg)
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
; (lo nuevo, siempre al final: Configuración de Hispa Ventanas lee la MAC de
; aquí por su desplazamiento)
chip_id     dw 0                    ; PCnet: número de pieza (CSR88/89)
card_name   times 36 db 0           ; el nombre (lo copia la instalación)
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
;  Realtek RTL8168 / 8111 / 8101 / 8169. Probada en un equipo de verdad (el
;  Toshiba NB250-10N, 10EC:8136 rev. 05h); QEMU no la emula, así que aquí no
;  se toca nada sin probarlo en ese equipo. Lo que le falta frente a r8169 de
;  Linux para las revisiones modernas está en docs/RED.md.
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
        mov di, [dma_z0]
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
;  AMD PCnet-PCI II (Am79C970A) y PCnet-FAST III (Am79C973), 1022:2000: la
;  tarjeta por omisión de VirtualBox, la «vlance» de VMware y -device pcnet
;  de QEMU. Sección de quita y pon (ver arriba): solo queda residente si la
;  tarjeta es ésta.
;
;  Fuentes: las hojas de datos públicas de AMD (Am79C970A, Am79C973) y, para
;  el comportamiento, el controlador pcnet32 de Linux (GPL-2.0) y las
;  emulaciones de QEMU (hw/net/pcnet.c) y VirtualBox (DevPCNet).
;
;  Registros por los puertos de E/S (BAR0), en modo de 32 bits (DWIO): RDP en
;  +10h, RAP en +14h, RESET en +18h y BDP en +1Ch; todos los accesos, de 32
;  bits. Se elige un registro escribiendo su número en RAP y se lee o escribe
;  por RDP (los CSR) o por BDP (los BCR). Estilo de software 2 (BCR20):
;  descriptores de 16 octetos con direcciones de 32 bits.
;  Todas las rutinas dejan RAP como quieran: cada una lo pone antes de usarlo
;  (la interrupción también), y la única que no usa el CSR0 con la tarjeta en
;  marcha (pn_link) lo hace con las interrupciones cerradas.
; =============================================================================
        section .pcnet
ov_base:

; RAP = BL -> DX = RDP. Destroza EAX.
pn_rap:
        mov dx, [cs:iob]
        add dx, 0x14
        movzx eax, bl
        out dx, eax
        sub dx, 4
        ret

; CSR número BL -> AX (y EAX). Destroza DX.
pn_rcsr:
        call pn_rap
        in eax, dx
        ret

; CSR número BL = AX. Lo conserva todo.
pn_wcsr:
        push eax
        push dx
        push eax
        call pn_rap
        pop eax
        movzx eax, ax
        out dx, eax
        pop dx
        pop eax
        ret

; DS:SI = paquete, CX = longitud
pn_send:
        mov es, [cs:dma_seg]
        mov di, PN_TXB
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
        movzx di, byte [cs:tx_i]
        shl di, 4
        add di, PN_TXD
        neg cx                      ; BCNT: la longitud en complemento a dos
        mov dword [es:di+8], 0      ; (12 bits; los bits 12-15 quedan a uno)
        mov [es:di+4], cx
        mov word [es:di+6], 0x8300  ; OWN, STP y ENP: lo último que se escribe
        xor bl, bl
        mov ax, 0x0048              ; CSR0: IENA y TDMD (envía ya, sin esperar
        call pn_wcsr                ; al sondeo de la tarjeta)
        mov cx, 60000
.w:
        mov al, [es:di+7]
        test al, 0x80               ; OWN: todavía es de la tarjeta
        jz .d
        call io_wait
        loop .w
        stc
        ret
.d:
        inc byte [cs:tx_i]
        and byte [cs:tx_i], 3
        test al, 0x40               ; ERR (sin portadora, colisiones...)
        jz .ok
        stc
        ret
.ok:
        clc
        ret

pn_isr:
        xor bl, bl
        call pn_rcsr
        test al, 0x80               ; INTR: la tarjeta pide interrupción
        jz .no
        and ax, 0x7F00              ; quitar las marcas que se han visto (se
        or al, 0x40                 ; borran escribiendo un uno) y seguir con
        call pn_wcsr                ; IENA
        call pn_drain
        clc
        ret
.no:
        stc
        ret

; Se mira el anillo ENTERO, no solo el descriptor que toca: una PCnet de
; verdad los llena por orden, pero la emulación de QEMU (hw/net/pcnet.c,
; pcnet_receive; puede que también las que salen de ella), si el que le toca
; aún no es suyo, busca cualquier otro libre y salta a él. Si eso pasara justo
; mientras aquí se devuelven (anillo lleno, en plena descarga), quedaría uno
; sin usar en medio y, esperándolo a él, las tramas siguientes se quedarían
; paradas hasta que el anillo diera la vuelta. Es una precaución sacada de
; leer ese código (no se ha visto fallar). Tras 4 pasos sin encontrar nada,
; rx_i queda donde estaba.
pn_drain:
.l:
        mov es, [dma_seg]
        mov cx, 4
.s:
        movzx bx, byte [rx_i]
        shl bx, 4                   ; (PN_RXD = 0)
        mov eax, [es:bx+4]
        test eax, 0x80000000        ; OWN: todavía es de la tarjeta
        jz .got
        inc byte [rx_i]
        and byte [rx_i], 3
        loop .s
        ret
.got:
        and eax, 0x43000000         ; ERR, STP y ENP: entera en un búfer y sin
        cmp eax, 0x03000000         ; errores
        jne .bad
        mov cx, [es:bx+8]           ; MCNT: longitud con el CRC
        and cx, 0x0FFF
        sub cx, 4
        cmp cx, 14
        jb .bad
        cmp cx, 1518
        ja .bad
        push bx
        movzx si, byte [rx_i]
        imul si, si, PN_BUF
        add si, PN_RXB
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
        mov dword [es:bx+4], PN_RMD1    ; otra vez de la tarjeta
        inc byte [rx_i]
        and byte [rx_i], 3
        jmp .l

; Bloque de inicialización en EAX (dirección física): parar, darle a la
; tarjeta el bloque y las máscaras, INIT y esperar a IDON.
; -> CF = 1 si la tarjeta no lo acaba. Deja BL = 0.
pn_doinit:
        xor bl, bl
        push ax
        mov ax, 0x0004              ; CSR0: STOP (siempre antes de reprogramar)
        call pn_wcsr
        pop ax
        inc bl
        call pn_wcsr                ; CSR1: bloque, parte baja
        shr eax, 16
        inc bl
        call pn_wcsr                ; CSR2: parte alta
        inc bl
        mov ax, 0x4B00              ; CSR3: sin interrupción por BABL, MERR,
        call pn_wcsr                ; TINT ni IDON (quedan RINT y MISS)
        inc bl
        mov ax, 0x0915              ; CSR4: relleno automático de las tramas
        call pn_wcsr                ; cortas (APAD_XMT) y lo demás, tapado
        xor bl, bl
        mov ax, 0x0001              ; CSR0: INIT (lee el bloque por DMA)
        call pn_wcsr
        mov cx, 50000
.w:
        call pn_rcsr
        test ah, 0x01               ; IDON
        jnz .ok
        call io_wait
        loop .w
        stc
        ret
.ok:
        clc
        ret

pn_start:
        mov es, [dma_seg]
        mov edx, [dma_phys]
        xor di, di
        lea eax, [edx+PN_RXB]
.d:
        mov [es:di+PN_RXD], eax
        mov dword [es:di+PN_RXD+4], PN_RMD1
        lea ecx, [edx+PN_TXB]
        mov [es:di+PN_TXD], ecx     ; (lo demás ya está a cero)
        add eax, PN_BUF
        add di, 16
        cmp di, 4 * 16
        jb .d
        ; bloque de inicialización (el de 32 bits): modo 0 (normal: lo dirigido
        ; a esta tarjeta y la difusión), 4 y 4 descriptores (RLEN = TLEN = 2),
        ; la MAC, el filtro de multidifusión a cero y los dos anillos
        mov di, PN_INIT
        mov word [es:di+2], 0x2020
        mov eax, [mac]
        mov [es:di+4], eax
        mov ax, [mac+4]
        mov [es:di+8], ax
        mov [es:di+20], edx         ; (PN_RXD = 0)
        lea eax, [edx+PN_TXD]
        mov [es:di+24], eax
        lea eax, [edx+PN_INIT]
        call pn_doinit              ; (si no acabara, STRT no haría daño)
        mov ax, 0x0142              ; CSR0: quitar IDON, STRT e IENA
        call pn_wcsr
        ret

pn_stop:
        push bx
        xor bl, bl
        mov ax, 0x0004              ; STOP: para el DMA y las interrupciones
        call pn_wcsr
        pop bx
        ret

; El enlace: BCR4 (el LED 0, que de fábrica es el de enlace), bit 15 = estado
; del LED. La velocidad no se mira.
pn_link:
        push bx
        push dx
        push eax
        pushf
        cli
        mov bl, 4
        call pn_rap
        add dx, 0x0C                ; BDP
        in eax, dx
        mov bx, ax
        sub dx, 8                   ; RAP = 0, como lo espera el resto
        xor eax, eax
        out dx, eax
        popf
        shr bx, 15                  ; 1 = hay enlace
        pop eax
        mov ax, bx                  ; AH = 0: la velocidad no se sabe
        pop dx
        pop bx
        ret
pn_end:
PN_LEN      equ pn_end - ov_base

; =============================================================================
;  DEC 21140/21140A/21143 «Tulip» (1011:0009 y 1011:0019). La 21140A es lo que
;  Hyper-V (y antes Virtual PC) le da a un sistema antiguo como «Adaptador de
;  red heredado»; QEMU la emula con -device tulip (como 21143). Sección de
;  quita y pon: se ensambla para la misma dirección que la PCnet (ov_base) y
;  tu_init la copia allí si la tarjeta es ésta.
;
;  Fuentes: las hojas de datos públicas de DEC/Intel (21140A, 21143: «Hardware
;  Reference Manual») y, para el comportamiento, el controlador tulip de Linux
;  (drivers/net/ethernet/dec/tulip, GPL-2.0) y la emulación de QEMU
;  (hw/net/tulip.c). Nada copiado: lo que se hace igual que Linux se dice,
;  porque Linux es lo que se sabe que funciona en Hyper-V.
;
;  Registros CSR0-CSR15 por los puertos de E/S (BAR0), de 32 bits, cada 8
;  puertos (CSRn en +n*8). Dos anillos de descriptores de 4 palabras dobles:
;  +0 estado (bit 31 OWN: es de la tarjeta), +4 control (tamaño del búfer 1 en
;  los bits 0-10; bit 25 = último del anillo), +8 búfer 1, +12 búfer 2.
;  Lo que se hace pensando en cada emulación:
;   - Anillo cerrado por el bit «fin de anillo» (RER/TER) Y ADEMÁS el búfer 2
;     de cada descriptor apuntando al siguiente, con tamaño 0 (así lo deja
;     Linux): vale tanto si la emulación sigue el anillo como si sigue la
;     cadena, y QEMU no escribe en el búfer 2 si su tamaño es 0.
;   - Cada trama, en un solo descriptor con «primero» y «último» (FS y LS) a
;     la vez: la emulación de Microsoft no admite tramas partidas.
;   - La trama de configuración (filtro perfecto: 16 direcciones) se envía y
;     se espera ANTES de arrancar la recepción: la de Microsoft no recibe
;     nada hasta tenerla; QEMU deja pasar la difusión sin ella, pero no lo
;     dirigido a la tarjeta. Sin FS ni LS, como manda la hoja de datos.
;   - No se toca el CSR8 (en QEMU 8.2 escribirlo pisa el CSR9) ni se lee el
;     CSR12 (en la 21140 son las patillas de uso general, no el enlace): el
;     estado del enlace no se sabe.
; =============================================================================
        section .tulip
tu_base:

; CSR número BL = EAX. Destroza DX.
tu_wr:
        movzx dx, bl
        shl dx, 3
        add dx, [cs:iob]
        out dx, eax
        ret

; DS:SI = paquete, CX = longitud
tu_send:
        mov es, [cs:dma_seg]
        mov di, TU_TXB
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
        movzx eax, cx
        or eax, 0x60000000          ; LS y FS: la trama entera en este búfer
; EAX = control del descriptor (tamaño y marcas) -> enviar lo que hay en el
; búfer de envío y esperar a que la tarjeta lo suelte. CF = 1 si no lo hace.
tu_xmit:
        mov es, [cs:dma_seg]
        movzx di, byte [cs:tx_i]
        shl di, 4
        jz .n0
        or eax, 0x02000000          ; TER: el segundo es el último del anillo
.n0:
        add di, TU_TXD
        mov [es:di+4], eax
        mov dword [es:di], 0x80000000   ; OWN: de la tarjeta
        mov bl, 1
        call tu_wr                  ; CSR1: «mira el anillo de envío»
        mov cx, 60000
.w:
        test byte [es:di+3], 0x80
        jz .d
        call io_wait
        loop .w
        stc
        ret
.d:
        ; (los bits de error del estado no se miran: en una red emulada no
        ; dicen nada, y la portadora o las colisiones de una de verdad no las
        ; arregla reenviar desde aquí)
        xor byte [cs:tx_i], 1
        clc
        ret

tu_isr:
        mov dx, [iob]
        add dx, 5 * 8               ; CSR5: estado
        in eax, dx
        cmp eax, 0xFFFFFFFF
        je .no
        test eax, 0x00018000        ; NIS o AIS: alguna de las que se han
        jz .no                      ; pedido en el CSR7
        out dx, eax                 ; quitar las marcas vistas (con un uno)
        call tu_drain
        xor eax, eax
        mov bl, 2
        call tu_wr                  ; CSR2: «mira el anillo de recepción» (si
        clc                         ; se quedó sin descriptores, se había
        ret                         ; parado)
.no:
        stc
        ret

tu_drain:
.l:
        mov es, [dma_seg]
        movzx bx, byte [rx_i]
        shl bx, 4                   ; (TU_RXD = 0)
        mov eax, [es:bx]
        test eax, 0x80000000        ; OWN: todavía es de la tarjeta
        jnz .r
        mov ecx, eax
        and ax, 0x8300              ; ES (error), FS y LS: entera y sin
        cmp ax, 0x0300              ; errores
        jne .bad
        shr ecx, 16
        and cx, 0x3FFF              ; longitud con el CRC
        sub cx, 4
        cmp cx, 14
        jb .bad
        cmp cx, 1518
        ja .bad
        push bx
        movzx si, byte [rx_i]
        imul si, si, TU_BUF
        add si, TU_RXB
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
        mov dword [es:bx], 0x80000000   ; otra vez de la tarjeta
        inc byte [rx_i]
        and byte [rx_i], 3
        jmp .l
.r:
        ret

tu_start:
        mov es, [dma_seg]
        mov edx, [dma_phys]
        ; anillo de recepción
        xor di, di
        lea eax, [edx+TU_RXB]
        lea ecx, [edx+TU_RXD+16]    ; el descriptor siguiente
.d:
        mov dword [es:di], 0x80000000
        mov dword [es:di+4], TU_BUF
        mov [es:di+8], eax
        mov [es:di+12], ecx
        add eax, TU_BUF
        add ecx, 16
        add di, 16
        cmp di, 4 * 16
        jb .d
        or byte [es:di-16+7], 0x02  ; RER en el último...
        mov [es:di-16+12], edx      ; ...que apunta al primero
        ; anillo de envío: dos descriptores con el mismo búfer
        lea eax, [edx+TU_TXB]
        mov [es:TU_TXD+8], eax
        mov [es:TU_TXD+16+8], eax
        lea eax, [edx+TU_TXD+16]
        mov [es:TU_TXD+12], eax
        sub eax, 16
        mov [es:TU_TXD+16+12], eax
        mov bl, 4
        call tu_wr                  ; CSR4: anillo de envío
        mov eax, [dma_phys]
        mov bl, 3
        call tu_wr                  ; CSR3: anillo de recepción (TU_RXD = 0)
        ; trama de configuración, en el búfer de envío: 16 direcciones de 12
        ; octetos (cada palabra de la dirección, dos veces); como Linux, 15
        ; veces la de difusión y, la última, la de esta tarjeta
        mov di, TU_TXB
        mov cx, 15 * 6
        mov ax, 0xFFFF
        rep stosw
        mov si, mac
        mov cx, 3
.sf:
        lodsw
        stosw
        stosw
        loop .sf
        mov eax, TU_CSR6 | TU_ST
        mov bl, 6
        call tu_wr                  ; arranca solo el envío
        mov eax, 0x08000000 | 192   ; SET: trama de configuración, 192 octetos
        call tu_xmit                ; (y se espera a que la tarjeta la lea)
        setc cl                     ; Si no la ha querido (una emulación sin
        shl cl, 6                   ; filtro), modo promiscuo (PR, bit 6): el
                                    ; conmutador de una red emulada ya solo
                                    ; entrega lo de esta tarjeta y la difusión
        mov eax, 0xFFFFFFFF
        mov bl, 5
        call tu_wr                  ; quitar las marcas pendientes
        mov eax, TU_CSR7
        mov bl, 7
        call tu_wr
        mov eax, TU_CSR6 | TU_ST | TU_SR
        or al, cl
        dec bl
        call tu_wr                  ; y ahora, la recepción
        ret

tu_stop:
        push bx
        push dx
        push eax
        xor eax, eax
        mov bl, 7
        call tu_wr                  ; sin interrupciones
        mov eax, TU_CSR6
        dec bl
        call tu_wr                  ; envío y recepción parados
        mov eax, 1
        xor bl, bl
        call tu_wr                  ; y reinicio: el DMA ya no toca la memoria
        pop eax
        pop dx
        pop bx
        ret
tu_end:
TU_LEN      equ tu_end - tu_base

; =============================================================================
;  Adaptador USB-Ethernet con la clase estándar CDC-ECM. El aparato lo lleva
;  HSPUSB.SYS (enumeración, transferencias y sondeo: lib/usb_red.inc); aquí
;  solo está el puente con el packet driver. Sección de quita y pon: se
;  ensambla para la misma dirección que la PCnet (ov_base) y us_init la copia
;  allí si la «tarjeta» es ésta.
;
;  HSPUSB da (INT 2Fh AX=DB00h -> ES:DI) un bloque con la MAC (+0), el VID y
;  el PID (+6), la velocidad USB (+10), la clase de controladora (+11), el
;  enlace (+12), su rutina de envío (+14, lejana: DS:SI = trama, CX = octetos
;  -> CF) y el sitio donde se le deja el receptor (+18): us_rx, al que llama
;  (lejana) desde el tic del reloj con ES:DI = trama y CX = octetos, con las
;  interrupciones cerradas.
;  No hay interrupción de la tarjeta: el «IRQ» de lo común (irq_handler) se
;  queda en un vector libre al que no llama nadie, para que la descarga y lo
;  demás sigan igual que con las tarjetas PCI sin gastar un octeto residente.
; =============================================================================
        section .usb
us_base:
us_info     dd 0                    ; el bloque de HSPUSB
us_src      dd 0                    ; la trama que se está entregando

; DS:SI = paquete, CX = longitud -> CF si no sale (HSPUSB lo rellena a 60)
us_send:
        les bx, [cs:us_info]
        call far [es:bx+14]
        ret

; (no hay interrupción: no es nuestra)
us_isr:
        stc
        ret

; a recibir: se le deja a HSPUSB el receptor
us_start:
        push es
        push bx
        les bx, [cs:us_info]
        pushf
        cli
        mov word [es:bx+18], us_rx
        mov [es:bx+20], cs
        popf
        pop bx
        pop es
        ret

us_stop:
        push es
        push bx
        les bx, [cs:us_info]
        pushf
        cli
        mov dword [es:bx+18], 0
        popf
        pop bx
        pop es
        ret

; AL = enlace (FFh: HSPUSB no lo sabe), AH = 0 (la velocidad no se sabe)
us_link:
        push es
        push bx
        les bx, [cs:us_info]
        mov al, [es:bx+12]
        xor ah, ah
        pop bx
        pop es
        ret

; El receptor: llamada lejana de HSPUSB (desde su INT 08h), ES:DI = trama,
; CX = octetos, interrupciones cerradas. Con la pila propia, como en la
; interrupción de una tarjeta.
us_rx:
        push ds
        push cs
        pop ds
        mov [sv_ss], ss
        mov [sv_sp], sp
        mov ax, cs
        mov ss, ax
        mov sp, pila_top
        cld
        mov [us_src], di
        mov [us_src+2], es
        push cx
        push ds
        push es
        pop ds
        mov si, di
        push cs
        pop es
        mov di, rx_tmp
        mov cx, 7
        rep movsw
        pop ds
        pop cx
        call rx_find
        jc .r
        mov cx, [rx_len]
        push ds
        lds si, [us_src]
        rep movsb
        pop ds
        call rx_done
.r:
        lss sp, [sv_sp]
        pop ds
        retf
us_end:
US_LEN      equ us_end - us_base

; =============================================================================
;  Marvell Yukon 2 «Optima» (88E8059, 11AB:4381): la tarjeta de cable del HP
;  Mini 5103. Sección de quita y pon (se ensambla para ov_base; yk_init la
;  copia allí): a las demás tarjetas no les cuesta ni un octeto residente.
;
;  Marvell no publicó la documentación de esta familia: los registros y el
;  orden de las operaciones salen de leer el controlador sky2 de Linux
;  (drivers/net/ethernet/marvell/sky2.c y sky2.h, GPL-2.0); el código es
;  propio. QEMU no la emula: se prueba con un simulador del chip
;  (tests/yukon_sim.py) y en el equipo de verdad. Ver docs/RED.md.
;
;  Registros (16 KB, BAR0, en memoria): por memoria con el modo «irreal»
;  (e1_flat, como la e1000) o, si no se puede (modo V86), por los puertos
;  (BAR de E/S, 256 puertos): los 128 primeros son el bloque 0 y los otros
;  128 enseñan el bloque de 128 octetos que diga el registro RAP (puerto 0).
;  El MAC («GMAC») está en 2800h y el PHY se lee por él (SMI).
;
;  Tres anillos de «elementos de lista» de 8 octetos (dirección, longitud,
;  control y código; el bit 7 del código dice que el elemento vale):
;   - recepción: cada elemento es un búfer libre; se le dice a la tarjeta
;     hasta dónde hay (índice «put» de su unidad de precarga);
;   - envío: lo mismo con las tramas que salen;
;   - estado: lo escribe la tarjeta (una trama recibida con su longitud y su
;     estado, hasta dónde ha enviado...) y la interrupción los va leyendo.
; =============================================================================
        section .yukon
yk_base:

%macro yw8 2
        dw (%1) | 0x4000
        db %2
%endmacro
%macro yw16 2
        dw (%1) | 0x8000
        dw %2
%endmacro
%macro yw32 2
        dw (%1) | 0xC000
        dd %2
%endmacro

; ---- lo que /INFO lee del residente (yk_diag a yk_diag_end) ----
yk_diag:
yk_chip     db 0                    ; B2_CHIP_ID (BCh = Optima)
yk_rev      db 0                    ; revisión del chip
yk_lnk      db 0                    ; el PHY dice que hay enlace
yk_spd      db 0                    ; 1 = 10, 2 = 100, 3 = 1000 Mbit/s
yk_up       db 0                    ; el MAC está enviando y recibiendo
yk_run      db 0                    ; anillos en marcha (yk_start)
yk_rx_nxt   db 0                    ; búfer de la próxima trama (0 a 3)
yk_poll     db 0                    ; /SONDEO: sin IRQ, desde el reloj
yk_st_i     dw 0                    ; siguiente elemento del anillo de estado
yk_rx_put   dw 0                    ; siguiente elemento libre de recepción
yk_tx_prd   dw 0                    ; siguiente elemento libre de envío
yk_txdn     dw 0                    ; hasta dónde dice la tarjeta que ha enviado
yk_txto     dw 0                    ; envíos que la tarjeta no ha confirmado
yk_phst     dw 0                    ; el último registro 17 del PHY
yk_irqs     dd 0                    ; interrupciones atendidas
yk_stle     dd 0                    ; elementos de estado tratados
yk_lastis   dd 0                    ; la última causa de interrupción
yk_lastrx   dd 0                    ; el último estado de recepción del MAC
yk_diag_end:
YK_DIAG_LEN equ yk_diag_end - yk_diag

yk_bufs     dw 0, 0, 0, 0               ; (los pone yk_layout)
yk_txb      dw 0

; ---- registros ----
; DX = registro -> CF = 0: DX = su puerto (con RAP ya puesto); CF = 1: EBX =
; su dirección (registros en memoria; FS plano). Interrupciones cerradas.
yk_ea:
        cmp byte [cs:e_io], 0
        je .m
        push ax
        mov ax, dx
        mov dx, [cs:iob]
        cmp ax, 0x80
        jb .b0
        push ax
        shr ax, 7
        out dx, al                  ; RAP: el bloque de 128 octetos
        pop ax
        and ax, 0x7F
        or al, 0x80
.b0:
        add dx, ax
        pop ax
        clc
        ret
.m:
        xor bx, bx                  ; (por si el receptor de un programa ha
        mov fs, bx                  ; tocado FS: la base, a cero otra vez)
        movzx ebx, dx
        add ebx, [cs:mmio]
        stc
        ret

; DX = registro -> AL / AX / EAX. Conservan todo lo demás.
yk_r8:
        push ebx
        push dx
        pushf
        cli
        call yk_ea
        jc .m
        in al, dx
        jmp short yk_x
.m:
        mov al, [fs:ebx]
yk_x:
        popf
        pop dx
        pop ebx
        ret
yk_r16:
        push ebx
        push dx
        pushf
        cli
        call yk_ea
        jc .m
        in ax, dx
        jmp short yk_x
.m:
        mov ax, [fs:ebx]
        jmp short yk_x
yk_r32:
        push ebx
        push dx
        pushf
        cli
        call yk_ea
        jc .m
        in eax, dx
        jmp short yk_x
.m:
        mov eax, [fs:ebx]
        jmp short yk_x
; DX = registro, AL / AX / EAX = valor. Conservan todo.
yk_w8:
        push ebx
        push dx
        pushf
        cli
        call yk_ea
        jc .m
        out dx, al
        jmp short yk_x
.m:
        mov [fs:ebx], al
        jmp short yk_x
yk_w16:
        push ebx
        push dx
        pushf
        cli
        call yk_ea
        jc .m
        out dx, ax
        jmp short yk_x
.m:
        mov [fs:ebx], ax
        jmp short yk_x
yk_w32:
        push ebx
        push dx
        pushf
        cli
        call yk_ea
        jc .m
        out dx, eax
        jmp short yk_x
.m:
        mov [fs:ebx], eax
        jmp short yk_x

; DS:SI = tabla de escrituras (yw8, yw16, yw32) acabada en un cero
yk_tab:
.l:
        lodsw
        test ax, ax
        jz .r
        mov dx, ax
        and dh, 0x3F
        test ah, 0x80
        jnz .w
        lodsb
        call yk_w8
        jmp .l
.w:
        test ah, 0x40
        jnz .d
        lodsw
        call yk_w16
        jmp .l
.d:
        lodsd
        call yk_w32
        jmp .l
.r:
        ret

; ---- el PHY, por el MAC (SMI; el PHY es el 0) ----
; BL = registro -> AX; CF = 1 (y AX = FFFFh) si no contesta
yk_phy_r:
        push cx
        push dx
        movzx ax, bl
        shl ax, 6
        or al, 0x20                 ; leer
        mov dx, 0x2880              ; GM_SMI_CTRL
        call yk_w16
        mov cx, 5000
.w:
        call yk_r16
        test al, 0x10               ; el dato ya está
        jnz .ok
        call io_wait
        loop .w
        mov ax, 0xFFFF
        stc
        jmp short .r
.ok:
        mov dl, 0x84                ; GM_SMI_DATA
        call yk_r16
        clc
.r:
        pop dx
        pop cx
        ret

; BL = registro, AX = valor
yk_phy_w:
        push ax
        push cx
        push dx
        mov dx, 0x2884
        call yk_w16
        movzx ax, bl
        shl ax, 6
        mov dl, 0x80
        call yk_w16
        mov cx, 5000
.w:
        call yk_r16
        test al, 0x08               ; ocupado
        jz .r
        call io_wait
        loop .w
.r:
        pop dx
        pop cx
        pop ax
        ret

; ---- el enlace: lo que diga el PHY (registro 17), y el MAC detrás ----
; Con las interrupciones cerradas (o desde la interrupción). No usa DS.
yk_linkchk:
        mov bl, 19
        call yk_phy_r               ; las causas del PHY: se borran al leerlas
        mov bl, 17
        call yk_phy_r
        jc .r
        mov [cs:yk_phst], ax
        mov bx, ax
        and ax, 0x0C00              ; enlace, y velocidad y dúplex resueltos
        cmp ax, 0x0C00
        jne .dn
        shr bh, 6                   ; 0 = 10, 1 = 100, 2 = 1000
        inc bh
        mov [cs:yk_spd], bh
        mov byte [cs:yk_lnk], 1
        cmp byte [cs:yk_run], 0     ; (antes de arrancar, solo se mira)
        je .r
        cmp byte [cs:yk_up], 0
        jne .r
        ; el hueco entre tramas, según la velocidad; y a enviar y recibir (la
        ; velocidad y el dúplex los copia el MAC del PHY él solo)
        mov dx, 0x2818              ; GM_SERIAL_MODE
        call yk_r16
        and al, 0xE0
        or al, 0x18
        cmp bh, 3
        jne .ipg
        or al, 0x06
.ipg:
        call yk_w16
        mov dx, 0x2804              ; GM_GP_CTRL
        call yk_r16
        or ah, 0x18                 ; TX_ENA y RX_ENA
        call yk_w16
        mov dx, 0x0C3C              ; la luz del enlace
        mov al, 0x16
        call yk_w8
        mov byte [cs:yk_up], 1
.r:
        ret
.dn:
        mov byte [cs:yk_lnk], 0
        cmp byte [cs:yk_up], 0
        je .r
        mov dx, 0x2804
        call yk_r16
        and ah, 0xE7
        call yk_w16
        mov dx, 0x0C3C
        mov al, 0x01
        call yk_w8
        mov byte [cs:yk_up], 0
        ret

; -> AL = enlace, AH = velocidad (para INT 2Fh AX=DA01h y /INFO)
yk_link:
        push fs
        pushad
        call e1_flat
        pushf
        cli
        call yk_linkchk
        popf
        popad
        pop fs
        xor ax, ax
        cmp byte [cs:yk_lnk], 0
        je .r
        mov ah, [cs:yk_spd]
        inc ax
.r:
        ret

; ---- enviar: DS:SI = paquete, CX = longitud -> CF ----
yk_send:
        push fs
        call e1_flat
        cmp byte [cs:yk_up], 0
        jne .up
        pushf
        cli
        call yk_linkchk             ; ¿acaba de llegar el enlace?
        popf
        cmp byte [cs:yk_up], 0
        je .err
.up:
        mov es, [cs:dma_seg]
        mov di, [cs:yk_txb]
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
        mov bx, [cs:yk_tx_prd]
        shl bx, 3
        mov eax, [cs:dma_phys]
        movzx edx, word [cs:yk_txb]
        add eax, edx
        mov [es:bx+YK_TX], eax
        mov [es:bx+YK_TX+4], cx
        mov word [es:bx+YK_TX+6], 0xC180    ; fin de trama; OP_PACKET, vale
        mov ax, [cs:yk_tx_prd]
        inc ax
        and ax, YK_NTX - 1
        mov [cs:yk_tx_prd], ax
        mov dx, 0x06E4              ; índice «put» del envío
        call yk_w16
        ; esperar a que la tarjeta la haya leído (el búfer es uno solo): su
        ; índice «hecho» (Q_DONE) o el que da por el anillo de estado
        mov cx, 60000
.w:
        mov dx, 0x06A4
        call yk_r16
        cmp ax, [cs:yk_tx_prd]
        je .ok
        mov ax, [cs:yk_txdn]
        cmp ax, [cs:yk_tx_prd]
        je .ok
        call io_wait
        loop .w
        inc word [cs:yk_txto]       ; sin confirmar (se cuenta; /INFO lo dice)
.ok:
        pop fs
        clc
        ret
.err:
        pop fs
        stc
        ret

; ---- interrupción ----
yk_isr:
        call e1_flat
        mov dx, 0x001C              ; B0_Y2_SP_ISRC2: al leerlo, la tarjeta
        call yk_r32                 ; deja de pedir interrupción
        test eax, eax
        jz .no
        cmp eax, 0xFFFFFFFF
        je .no
        mov dx, 0x0024              ; B0_Y2_SP_EISR: las causas
        call yk_r32
        mov [yk_lastis], eax
        inc dword [yk_irqs]
        test al, 0x10               ; el PHY: ha cambiado el enlace
        jz .np
        call yk_linkchk
.np:
        call yk_drain
        mov dx, 0x0028              ; B0_Y2_SP_LISR: puede volver a pedirla
        call yk_r32
        cmp byte [yk_poll], 1       ; con /SONDEO esto es el reloj (IRQ 0):
        cmc                         ; siempre «no era mía», y sigue la BIOS
        ret
.no:
        mov dx, 0x002C              ; B0_Y2_SP_ICR
        mov eax, 2
        call yk_w32
        stc
        ret

; el anillo de estado, hasta donde ha escrito la tarjeta (STAT_PUT_IDX)
yk_drain:
        mov bp, 4 * YK_NST          ; (por si la tarjeta dijera disparates)
.o:
        mov dx, 0x0E9C
        call yk_r16
        cmp ax, [yk_st_i]
        je .r
        mov di, ax
.l:
        dec bp
        jz .r
        mov es, [dma_seg]
        mov bx, [yk_st_i]
        shl bx, 3
        mov al, [es:bx+YK_ST+7]     ; código
        test al, 0x80               ; ¿vale ya?
        jz .clr
        mov byte [es:bx+YK_ST+7], 0
        mov dx, [yk_st_i]
        inc dx
        and dx, YK_NST - 1
        mov [yk_st_i], dx
        inc dword [yk_stle]
        mov ecx, [es:bx+YK_ST]      ; estado
        mov dx, [es:bx+YK_ST+4]     ; longitud
        cmp al, 0xE0                ; OP_RXSTAT: una trama recibida
        je .rx
        cmp al, 0xE8                ; OP_TXINDEXLE: hasta dónde ha enviado
        jne .n
        and cx, 0x0FFF
        mov [yk_txdn], cx
        jmp short .n
.rx:
        mov [yk_lastrx], ecx
        mov eax, ecx
        mov cx, dx
        push di
        push bp
        call yk_rx
        pop bp
        pop di
.n:
        cmp di, [yk_st_i]
        jne .l
.clr:
        mov dx, 0x0E80              ; STAT_CTRL: interrupción de estado vista
        mov eax, 0x10
        call yk_w32
        jmp .o
.r:
        ret

; una trama recibida: EAX = estado del MAC, CX = longitud. Está en el búfer
; yk_rx_nxt (la tarjeta los llena en el orden en que se le dieron), que
; después se le vuelve a dar.
yk_rx:
        test eax, 0x187B            ; algún error (CRC, larga, corta, FIFO...)
        jnz .bad
        test ah, 0x01               ; recibida bien
        jz .again
        shr eax, 16
        and ax, 0x7FFF
        cmp ax, cx                  ; (si no coinciden, llegó cortada)
        jne .bad
        cmp cx, 14
        jb .bad
        cmp cx, 1518
        ja .bad
        movzx bx, byte [yk_rx_nxt]
        add bx, bx
        mov si, [yk_bufs+bx]
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
        jc .again
        mov cx, [rx_len]
        push ds
        mov ds, [dma_seg]
        rep movsb
        pop ds
        call rx_done
        jmp short .again
.bad:
        inc dword [st_ein]
.again:
        call yk_rx_add
        mov ax, [yk_rx_put]
        mov dx, 0x0464              ; índice «put» de la recepción
        jmp yk_w16

; el búfer yk_rx_nxt, al siguiente elemento libre de recepción
yk_rx_add:
        mov es, [dma_seg]
        movzx bx, byte [yk_rx_nxt]
        add bx, bx
        movzx eax, word [yk_bufs+bx]
        add eax, [dma_phys]
        mov bx, [yk_rx_put]
        shl bx, 3
        mov [es:bx+YK_RX], eax
        mov dword [es:bx+YK_RX+4], 0xC1000000 | YK_RXLEN    ; OP_PACKET, vale
        inc word [yk_rx_put]
        and word [yk_rx_put], YK_NRX - 1
        inc byte [yk_rx_nxt]
        and byte [yk_rx_nxt], 3
        ret

; ---- arrancar: unidad de estado, colas, anillos e interrupciones ----
; (el reinicio, el PHY y el MAC ya los ha dejado puestos yk_init)
yk_start:
        push fs
        call e1_flat
        mov si, yk_t_st1
        call yk_tab
        mov eax, [dma_phys]
        add eax, YK_ST
        mov dx, 0x0E88              ; STAT_LIST_ADDR_LO
        call yk_w32
        call yk_tab                 ; (yk_t_st2: sigue a la anterior)
        mov eax, [dma_phys]
        add eax, YK_TX
        mov dx, 0x06D8              ; lista de envío
        call yk_w32
        call yk_tab                 ; (yk_t_st3)
        mov eax, [dma_phys]
        add eax, YK_RX
        mov dx, 0x0458              ; lista de recepción
        call yk_w32
        call yk_tab                 ; (yk_t_st4)
        xor ax, ax
        mov [yk_st_i], ax
        mov [yk_rx_put], ax
        mov [yk_rx_nxt], al
        mov [yk_up], al
        mov cx, 4
.b:
        call yk_rx_add              ; los cuatro búferes, a la tarjeta
        loop .b
        mov ax, [yk_rx_put]
        mov dx, 0x0464
        call yk_w16
        ; envío: el primer elemento dice la parte alta de las direcciones (0)
        mov dword [es:YK_TX], 0
        mov dword [es:YK_TX+4], 0xA1000000  ; OP_ADDR64, vale
        mov word [yk_tx_prd], 1
        mov word [yk_txdn], 1
        mov byte [yk_run], 1
        pushf
        cli
        mov eax, 0x40000010         ; B0_IMSK: el anillo de estado y el PHY
        mov dx, 0x000C
        call yk_w32
        call yk_r32
        mov dl, 0x28                ; B0_Y2_SP_LISR (por si se quedó tapada)
        call yk_r32
        call yk_linkchk
        popf
        pop fs
        ret

yk_t_st1:
        yw32 0x0E80, 0x01           ; STAT_CTRL: reinicio de la unidad de estado
        yw32 0x0E80, 0x02
        dw 0
yk_t_st2:
        yw32 0x0E8C, 0              ; STAT_LIST_ADDR_HI
        yw16 0x0E84, YK_NST - 1     ; STAT_LAST_IDX
        yw16 0x0E98, 10             ; STAT_TX_IDX_TH
        yw8  0x0EAC, 16             ; STAT_FIFO_WM
        yw8  0x0EAD, 16             ; STAT_FIFO_ISR_WM
        yw32 0x0EC0, 125 * 1000     ; temporizadores (reloj de 125 MHz): envío,
        yw32 0x0ED0, 125 * 20       ; 1 ms; interrupción, 20 us; nivel, 100 us
        yw32 0x0EB0, 125 * 100
        yw32 0x0E80, 0x08           ; en marcha
        yw8  0x0EC8, 0x04
        yw8  0x0EB8, 0x04
        yw8  0x0ED8, 0x04
        ; cola de envío (Q_XA1, 680h): BMU y unidad de precarga (6D0h)
        yw32 0x06B4, 0x0016         ; Q_CSR: fuera el reinicio
        yw32 0x06B4, 0x0D28         ; en marcha
        yw32 0x06B4, 0x0080         ; y su FIFO
        yw32 0x06C0, 0x0600         ; Q_WM
        yw32 0x06D0, 0x01           ; precarga: reinicio
        yw32 0x06D0, 0x02
        yw32 0x06DC, 0              ; dirección de la lista (alta)
        dw 0
yk_t_st3:
        yw16 0x06D4, YK_NTX - 1     ; último índice
        yw32 0x06D0, 0x08           ; en marcha
        yw32 0x0C48, 0x01000000     ; sin quitar la etiqueta VLAN al recibir
        yw32 0x0D48, 0x01000000     ; ni ponerla al enviar
        ; cola de recepción (Q_R1, 400h) y su unidad de precarga (450h)
        yw32 0x0434, 0x0016
        yw32 0x0434, 0x0D28
        yw32 0x0434, 0x0080
        yw32 0x0440, 0x0600
        yw32 0x0440, 0x0080         ; Q_WM para PCI Express
        yw32 0x0450, 0x01
        yw32 0x0450, 0x02
        yw32 0x045C, 0
        dw 0
yk_t_st4:
        yw16 0x0454, YK_NRX - 1
        yw32 0x0450, 0x08
        yw32 0x0434, 0x4000         ; sin el resumen RSS en el anillo de estado
        yw16 0x0C54, (YK_RXLEN - 8) / 4     ; cortar las tramas más largas que
        yw32 0x0C48, 0x08000000     ; el búfer (si no, la recepción se cuelga)
        dw 0

; ---- parar (antes de soltar la memoria): sin interrupciones ni DMA ----
yk_stop:
        push fs
        pushad
        call e1_flat
        pushf
        cli
        mov si, yk_t_stop
        call yk_tab
        mov byte [yk_run], 0
        mov byte [yk_up], 0
        popf
        popad
        pop fs
        ret
yk_t_stop:
        yw32 0x000C, 0              ; B0_IMSK
        yw16 0x2804, 0x2012         ; GM_GP_CTRL: sin enviar ni recibir
        yw8  0x0C3C, 0x01           ; luz del enlace
        yw32 0x06B4, 0x0200         ; envío: parar
        yw32 0x0434, 0x0011         ; recepción: BMU y FIFO en reinicio
        yw32 0x0450, 0x01           ; y su precarga
        yw32 0x06B4, 0x0011
        yw32 0x06D0, 0x01
        yw32 0x0E80, 0x01           ; unidad de estado
        yw16 0x0004, 0x0100         ; B0_CTST: luz de «controlador cargado»
        yw8  0x0004, 0x01           ; y el chip entero, en reinicio
        dw 0
yk_end:
YK_LEN      equ yk_end - yk_base

; =============================================================================
;  Instalación (se descarta)
; =============================================================================
        section .init

init:
        cld
        call parse
        jnc .pok
        mov si, msg_usage
        call puts
        mov al, 1
        jmp salir
.pok:
        ; el banner (msg_title), salvo con /AUTO: así, en el AUTOEXEC.BAT de un
        ; equipo sin red, no se imprime nada (ni el banner ni el aviso); si hay
        ; tarjeta, igualmente se verá la línea «Tarjeta X, MAC...».
        cmp byte [o_auto], 0
        jne .no_banner
        mov si, msg_title
        call puts
.no_banner:
        cmp byte [o_help], 0
        je .nh
        mov si, msg_usage
        call puts
        mov al, 0
        jmp salir
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
        mov [cs:res_seg], ds
        pop ds
        cmp byte [card], C_YUKON
        jne .ny
        ; la Yukon: sus rutinas de registros a su sitio y lo que lleva
        ; contado el residente (mismo programa: mismos desplazamientos)
        call yk_place
        push ds
        mov ds, [res_seg]
        mov si, yk_diag
        mov di, yk_diag
        mov cx, YK_DIAG_LEN
        rep movsb
        pop ds
        mov byte [yk_live], 1
.ny:
        mov ax, 0xDA01
        int 0x2F
        mov [i_link], ax
        mov word [name_ptr], card_name  ; (el del residente, copiado)
        call show_info
        mov al, 0
        jmp salir
.alr:
        mov si, msg_already
        call puts
        mov al, 1
        jmp salir
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
        mov al, 1
        jmp salir
.ue:
        call puts
        mov al, 0
        jmp salir
.new:
        cmp byte [o_unload], 0
        je .find
        mov si, msg_notres
        call puts
        mov al, 1
        jmp salir
.find:
        smsw ax
        and al, 1
        mov [v86], al
        cmp byte [o_usb], 0         ; /USB: sin mirar las tarjetas PCI
        jne .usb
        call find_card
        jnc .found
.usb:
        call find_usb               ; ¿un adaptador USB que lleve HSPUSB?
        jnc .found
        ; no hay tarjeta de red. Con /AUTO (p. ej. en el AUTOEXEC.BAT), salir
        ; en silencio con errorlevel 1; sin /AUTO, avisar como siempre.
        cmp byte [o_auto], 0
        jne .silent
        mov si, msg_nocard
        call puts
        call list_net
.silent:
        mov al, 1
        jmp salir
.found:
        call card_init
        jnc .cok
        call puts                   ; SI = el motivo
        mov al, 1
        jmp salir
.cok:
        ; vector del packet driver
        call pick_vector
        jnc .vok
        mov si, msg_novec
        call puts
        mov al, 1
        jmp salir
.vok:
        call dma_setup
        jnc .dok
        call puts
        mov al, 1
        jmp salir
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
        cmp byte [card], C_USB
        jne .iv2
        mov al, [us_vec]            ; USB: sin IRQ (un vector libre: us_init)
.iv2:
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
        call log_flush
        jmp go_resident

; salir con el código AL (antes, lo de /DEPURA al archivo)
salir:
        call log_flush
        mov ah, 0x4C
        int 0x21

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
        cmp al, 'A'                 ; /AUTO
        je .auto
        cmp al, 'Q'                 ; /Q, lo mismo que /AUTO
        je .q
        cmp al, 'D'                 ; /DEPURA[:archivo]
        je .dbg
        cmp al, 'M'                 ; /MEMORIA
        je .ymem
        cmp al, 'P'                 ; /PUERTOS
        je .yio
        cmp al, 'S'                 ; /SONDEO
        je .poll
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
        mov al, [si]                ; ¿/USB?
        call upcase
        cmp al, 'S'
        jne .u1
        inc si
        lodsb
        call upcase
        cmp al, 'B'
        jne .bad
        mov byte [o_usb], 1
        jmp .end
.u1:
        mov byte [o_unload], 1
        jmp .end
.auto:
        ; /AUTO: pensada para el AUTOEXEC.BAT. Si no hay tarjeta, HSPRED sale
        ; en silencio (sin msg_nocard) con errorlevel 1, para no molestar en el
        ; arranque de un equipo sin red. Se escribe entera «/AUTO».
        lodsb
        call upcase
        cmp al, 'U'
        jne .bad
        lodsb
        call upcase
        cmp al, 'T'
        jne .bad
        lodsb
        call upcase
        cmp al, 'O'
        jne .bad
.q:
        mov byte [o_auto], 1
        jmp .end
.poll:
        mov byte [o_poll], 1
        jmp .word
.ymem:
        mov byte [o_ymode], 2
        jmp .word
.yio:
        mov byte [o_ymode], 1
.word:
        ; el resto de la palabra (letras)
        mov al, [si]
        call upcase
        cmp al, 'A'
        jb .end
        cmp al, 'Z'
        ja .end
        inc si
        jmp .word
.dbg:
        ; (el registro va detrás del programa: si DOS le ha dado un bloque
        ; justo, con LH por ejemplo, sin registro)
        mov ax, log_buf + LOG_MAX + 1024 + 15
        shr ax, 4
        mov dx, cs
        add ax, dx
        cmp ax, [0x02]              ; el final de la memoria del programa
        ja .dw
        mov byte [o_dbg], 1
.dw:
        mov al, [si]
        call upcase
        cmp al, 'A'
        jb .dn
        cmp al, 'Z'
        ja .dn
        inc si
        jmp .dw
.dn:
        cmp al, ':'
        je .df
        cmp al, '='
        jne .end
.df:
        inc si
        mov di, log_name
        mov cx, 63
.dc:
        mov al, [si]
        cmp al, ' '
        jbe .de
        cmp al, '/'
        je .de
        mov [di], al
        inc di
        inc si
        loop .dc
.de:
        mov byte [di], 0
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
o_auto      db 0
o_usb       db 0
o_int       db 0
o_dbg       db 0                    ; /DEPURA
o_ymode     db 0                    ; Yukon: 1 = /PUERTOS, 2 = /MEMORIA
o_poll      db 0                    ; Yukon: /SONDEO
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
        dw 0x8086, 0x10D3           ; 82574L («e1000e» de QEMU y de VMware):
        db C_E1000                  ; con descriptores clásicos, igual que las
                                    ; 8254x (ver e1_init)
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
        dw 0x1022, 0x2000           ; AMD PCnet-PCI II / FAST III (VirtualBox,
        db C_PCNET                  ; VMware «vlance», QEMU -device pcnet)
        dw 0x1022, 0x2001           ; AMD PCnet-Home (Am79C978)
        db C_PCNET
        dw 0x1011, 0x0009           ; DEC 21140/21140A (Hyper-V: «Adaptador
        db C_TULIP                  ; de red heredado»)
        dw 0x1011, 0x0019           ; DEC 21142/21143 (QEMU -device tulip)
        db C_TULIP
        dw 0x11AB, 0x4381           ; Marvell Yukon 88E8059 «Optima» (HP Mini
        db C_YUKON                  ; 5103)
        dw 0x11AB, 0x4382           ; Marvell Yukon 88E8079 «Optima 2»
        db C_YUKON
        dw 0
card_names  dw 0, name_ne, name_8139, name_e1000, name_8168, name_pcnet
            dw name_tulip, name_usb, name_yukon
; los nombres: el de la tarjeta encontrada se copia a card_name (residente)
name_ne     db "NE2000 PCI", 0
name_8139   db "Realtek RTL8139", 0
name_e1000  db "Intel PRO/1000 (e1000)", 0
name_e1000e db "Intel PRO/1000 (e1000e, 82574L)", 0
name_8168   db "Realtek RTL8168/8111", 0
name_pcnet  db "AMD PCnet", 0
name_pn70a  db "AMD PCnet-PCI II (Am79C970A)", 0
name_pn73   db "AMD PCnet-FAST III (Am79C973)", 0
name_pn75   db "AMD PCnet-FAST III (Am79C975)", 0
name_pn78   db "AMD PCnet-Home (Am79C978)", 0
name_tulip  db "DEC 2114x (Tulip)", 0
name_tu40   db "DEC 21140 (Tulip)", 0
name_tu43   db "DEC 21143 (Tulip)", 0
name_usb    db "Adaptador USB (CDC-ECM)", 0

; DS:SI = nombre -> card_name (como mucho 35 letras) y name_ptr
set_name:
        push ax
        push cx
        push di
        push es
        push ds
        pop es
        mov di, card_name
        mov cx, 35
.l:
        lodsb
        stosb
        test al, al
        loopnz .l
        mov byte [di], 0            ; (si era más largo; el 36.º es siempre 0)
        mov word [name_ptr], card_name
        pop es
        pop di
        pop cx
        pop ax
        ret

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
        mov si, [card_names+bx]
        call set_name
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
        cmp byte [card], C_USB
        je us_init
        call pci_d0
        ; IRQ
        mov bx, [pci_bdf]
        mov ax, 0xB108
        mov di, 0x3C
        int 0x1A
        mov [irq], cl
        ; Yukon con /SONDEO: sin IRQ. La «interrupción» es el reloj (IRQ 0,
        ; INT 08h): yk_isr mira la tarjeta en cada tic y deja seguir a la BIOS
        cmp byte [o_poll], 0
        je .irq
        cmp byte [card], C_YUKON
        jne .irq
        mov byte [irq], 0
        jmp .bars
.irq:
        mov si, msg_noirq
        cmp cl, 1
        jb .fail
        cmp cl, 15
        ja .fail
        cmp cl, 2
        je .fail
        ; puertos y memoria (BAR)
.bars:
        call find_bars
        ; E/S, memoria y bus master
        mov bx, [pci_bdf]
        mov ax, 0xB109
        mov di, 0x04
        int 0x1A
        or cx, 0x0007
        cmp byte [card], C_YUKON    ; Yukon: y la interrupción INTx permitida
        jne .cmd                    ; (bit 10 a cero), salvo con /SONDEO
        and ch, 0xFB
        cmp byte [o_poll], 0
        je .cmd
        or ch, 0x04
.cmd:
        mov ax, 0xB10C
        int 0x1A
        mov al, [card]
        cmp al, C_NE
        je ne_init
        cmp al, C_8139
        je r39_init
        cmp al, C_E1000
        je e1_init
        cmp al, C_8168
        je r68_init
        cmp al, C_PCNET
        je pn_init
        cmp al, C_YUKON
        je yk_init
        jmp tu_init
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
        ; la base de la GDT del modo «irreal» (e1_flat): sin esto quedaba en 0
        ; y el descriptor salía de los vectores 2 y 3 de la tabla de
        ; interrupciones. En QEMU sin KVM colaba (no comprueba los límites
        ; de los segmentos); en un procesador de verdad, o con KVM, el
        ; equipo se colgaba, y con HispaBIOS (UEFI) se reiniciaba.
        xor eax, eax
        mov ax, cs
        shl eax, 4
        add eax, gdt_tbl
        mov [gdtr+2], eax
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
        ; la 82574L, con su nombre (lo demás, igual: reinicio, RAL0/RAH0,
        ; anillos de descriptores clásicos e interrupción INTx, que es lo que
        ; hay mientras nadie active MSI ni los descriptores ampliados)
        cmp word [pci_dev], 0x10D3
        jne .n74
        mov si, name_e1000e
        call set_name
.n74:
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

; ---- AMD PCnet ----
pn_init:
        mov si, msg_noio
        cmp word [iob], 0
        je .f
        mov word [res_end], pn_end  ; lo residente llega hasta aquí
        ; Reinicio por programa (leer el registro RESET), en los dos anchos:
        ; la tarjeta puede estar en modo de 16 bits (WIO: como sale del
        ; encendido, y como la deja el reinicio en QEMU y VirtualBox) o en el
        ; de 32 (DWIO: en las de verdad el reinicio por programa no lo quita).
        ; El acceso del ancho que no toca no hace nada.
        mov dx, [iob]
        add dx, 0x14                ; RESET en WIO
        in ax, dx
        add dx, 4                   ; RESET en DWIO
        in eax, dx
        mov cx, 2
        call ticks_wait
        ; a DWIO: una escritura de 32 bits en RDP (si ya lo estaba, escribe
        ; un cero en el CSR0, que no hace nada)
        mov dx, [iob]
        add dx, 0x10
        xor eax, eax
        out dx, eax
        ; ¿contesta? Tras el reinicio el CSR0 vale 0004h (STOP)
        xor bl, bl
        call pn_rcsr
        mov si, msg_pn_no
        cmp ax, 0x0004
        jne .f
        call pn_wcsr                ; STOP, por si acaso
        ; estilo de software 2 (BCR20): PCnet-PCI, todo de 32 bits
        mov bl, 20
        call pn_rap
        add dx, 0x0C                ; BDP
        mov eax, 2
        out dx, eax
        ; BCR2: ASEL (elige sola el medio: par trenzado o AUI)
        mov bl, 2
        call pn_rap
        add dx, 0x0C
        in eax, dx
        or al, 0x02
        movzx eax, ax
        out dx, eax
        ; número de pieza: CSR88 (bits 12-15) y CSR89 (bits 0-11)
        mov bl, 89
        call pn_rcsr
        shl eax, 16
        push eax
        mov bl, 88
        call pn_rcsr
        pop ecx
        mov cx, ax
        shr ecx, 12
        mov [chip_id], cx
        call pn_setname
        ; la MAC: los 6 primeros octetos del APROM (los puertos +0 a +5; en
        ; DWIO se leen de 4 en 4)
        mov dx, [iob]
        in eax, dx
        mov [mac], eax
        add dx, 4
        in eax, dx
        mov [mac+4], ax
        call .macok
        jnc .mac
        ; si el APROM no la da, la que la tarjeta cargó en los CSR12-14
        mov bl, 12
        mov di, mac
.m:
        call pn_rcsr
        mov [di], ax
        add di, 2
        inc bl
        cmp bl, 15
        jb .m
        call .macok
        mov si, msg_pn_mac
        jc .f
.mac:
        ; Ensayo de la inicialización con un bloque de prueba (sin anillos:
        ; INIT solo carga los registros), para saber AHORA si la tarjeta la
        ; acaba (IDON) y poder decirlo; la de verdad se hace al quedarse
        ; residente (pn_start), con los búferes ya en su sitio.
        mov di, pn_tmp + 3
        and di, 0xFFFC              ; alineado a 4
        mov word [di+2], 0x2020
        mov eax, [mac]
        mov [di+4], eax
        mov ax, [mac+4]
        mov [di+8], ax
        xor eax, eax
        mov ax, cs
        shl eax, 4
        movzx edx, di
        add eax, edx
        call phys
        call pn_doinit
        pushf
        xor bl, bl
        mov ax, 0x0004              ; STOP otra vez, hasta pn_start
        call pn_wcsr
        popf
        mov si, msg_pn_idon
        jc .f
        mov word [c_send], pn_send
        mov word [c_isr], pn_isr
        mov word [c_stop], pn_stop
        mov word [c_link], pn_link
        mov word [c_start], pn_start
        mov word [need_dma], PN_SIZE
        mov word [dma_align], 16
        clc
        ret
.f:
        stc
        ret
; ¿la MAC vale? (ni todo ceros ni todo unos) -> CF = 1 si no
.macok:
        mov eax, [mac]
        movzx edx, word [mac+4]
        test eax, eax
        jnz .mk1
        test dx, dx
        jz .mkno
.mk1:
        cmp eax, 0xFFFFFFFF
        jne .mkok
        cmp dx, 0xFFFF
        je .mkno
.mkok:
        clc
        ret
.mkno:
        stc
        ret

; el nombre de la PCnet según su número de pieza (chip_id)
pn_setname:
        mov ax, [chip_id]
        mov si, name_pn70a
        cmp ax, 0x2621
        je .s
        mov si, name_pn73
        cmp ax, 0x2625
        je .s
        mov si, name_pn75
        cmp ax, 0x2627
        je .s
        mov si, name_pn78
        cmp ax, 0x2626
        je .s
        mov si, name_pcnet
.s:
        call set_name
        ret
pn_tmp      times 32 db 0

; ---- adaptador USB (CDC-ECM), por HSPUSB ----
; -> CF = 0 si HSPUSB tiene uno (us_blk = su bloque)
find_usb:
        push es
        mov ax, 0xDB00
        xor bx, bx
        xor di, di
        int 0x2F
        cmp al, 0xFF
        jne .no
        cmp bx, 0x5552              ; «UR»
        jne .no
        mov [us_blk], di
        mov [us_blk+2], es
        mov byte [card], C_USB
        mov ax, [es:di+6]
        mov [pci_ven], ax           ; (VID y PID del aparato USB)
        mov ax, [es:di+8]
        mov [pci_dev], ax
        mov ax, [es:di+10]
        mov [mmio], ax              ; (velocidad USB y clase de controladora)
        pop es
        mov si, name_usb
        call set_name
        clc
        ret
.no:
        pop es
        stc
        ret

us_init:
        ; su parte residente, a su sitio: en el archivo va detrás de las de
        ; la PCnet y la Tulip
        push ds
        pop es
        mov si, pn_end + TU_LEN     ; donde está en el archivo
        mov di, us_base             ; para donde está ensamblada
        mov cx, US_LEN
        rep movsb
        mov word [res_end], us_end
        mov eax, [us_blk]
        mov [us_info], eax
        ; la MAC
        push ds
        lds si, [us_blk]
        mov di, mac
        mov cx, 6
        rep movsb
        pop ds
        ; sin IRQ: irq_handler se queda en un vector libre (distinto del
        ; que vaya a ser el del packet driver) al que no llama nadie, y el
        ; «IRQ» es el 0 (quitarle la máscara al reloj no cambia nada)
        mov byte [irq], 0
        mov al, 0x80
.v:
        cmp al, 0x67
        je .vn
        cmp al, [o_int]
        je .vn
        call vec_free
        jnc .vok
.vn:
        dec al
        cmp al, 0x60
        jae .v
        mov si, msg_novec
        stc
        ret
.vok:
        mov [us_vec], al
        mov word [c_send], us_send
        mov word [c_isr], us_isr
        mov word [c_stop], us_stop
        mov word [c_link], us_link
        mov word [c_start], us_start
        mov word [need_dma], 0
        clc
        ret
us_blk      dd 0
us_vec      db 0

; ---- Marvell Yukon 2 Optima (88E8059) ----
; Lo que se hace una sola vez: reinicio y encendido del chip, el PHY (con la
; autonegociación) y el MAC. Los anillos, en yk_start (residente).

; la parte residente de la Yukon, a su sitio (yk_base): en el archivo va
; detrás de las de la PCnet, la Tulip y el USB. Y la base de la GDT del modo
; «irreal» (como en e1_init).
yk_place:
        push es
        push ds
        pop es
        mov si, pn_end + TU_LEN + US_LEN
        mov di, yk_base
        mov cx, YK_LEN
        rep movsb
        pop es
        xor eax, eax
        mov ax, cs
        shl eax, 4
        add eax, gdt_tbl
        mov [gdtr+2], eax
        ret

; con /DEPURA: «  [Yukon] texto XXXX» (SI = texto, AX = valor)
yk_tr:
        cmp byte [o_dbg], 0
        je .r
        push si
        mov si, msg_yk_tr
        call puts
        pop si
        call puts
        call hex4
        call crlf
.r:
        ret

; -> AL = B2_CHIP_ID leído con el modo de acceso que diga e_io (antes, los
; relojes del chip encendidos y fuera el reinicio, como hace sky2_init)
yk_probe:
        xor eax, eax
        mov dx, 0x1C80              ; PCI_DEV_REG3 (por la ventana de la
        call yk_w32                 ; configuración PCI, 1C00h)
        mov dx, 0x0004              ; B0_CTST
        mov al, 0x02                ; CS_RST_CLR
        call yk_w8
        mov dx, 0x011B              ; B2_CHIP_ID
        call yk_r8
        ret

; AL = identificador del chip -> CF = 1 si no es de la familia Yukon 2
yk_valid:
        cmp al, 0xB3
        jb .no
        cmp al, 0xBE
        ja .no
        clc
        ret
.no:
        stc
        ret

; DI = lo primero libre (dentro de dma_seg; los anillos van en 1000h, 2000h
; y 3000h) -> yk_bufs, yk_txb y need_dma: cada búfer, en el primer hueco
; donde quepa
yk_layout:
        push bp
        mov si, yk_huecos
        mov [si], di
        xor bx, bx                  ; 0-6: recepción; 8: envío
        mov dx, YK_TX + 8 * YK_NTX  ; hasta dónde llega
.h:
        lodsw
        mov cx, ax
        lodsw
        xchg ax, cx                 ; AX = desde, CX = hasta
.b:
        mov bp, ax
        add bp, YK_BUF
        cmp bp, cx
        ja .h
        mov [yk_bufs+bx], ax
        cmp bp, dx
        jbe .n
        mov dx, bp
.n:
        mov ax, bp
        add bx, 2
        cmp bx, 10
        jb .b
        mov [need_dma], dx
        pop bp
        ret
yk_huecos   dw 0, YK_ST
            dw YK_ST + 8 * YK_NST, YK_RX
            dw YK_RX + 8 * YK_NRX, YK_TX
            dw YK_TX + 8 * YK_NTX, 0xFFFF

yk_init:
        mov word [res_end], yk_end
        call yk_place
        mov al, [o_poll]
        mov [yk_poll], al
        push fs
        ; ¿cómo se llega a los registros? Por memoria (lo que hace Linux; no
        ; en modo V86) y, si no, por los puertos. Se prueban los dos (si se
        ; puede) leyendo el identificador del chip; /MEMORIA y /PUERTOS mandan.
        mov byte [yk_id_m], 0xFF
        mov byte [yk_id_p], 0xFF
        cmp byte [v86], 0
        jne .nm
        cmp dword [mmio], 0
        je .nm
        cmp byte [o_ymode], 1
        je .nm
        mov byte [e_io], 0
        call e1_flat
        call yk_probe
        mov [yk_id_m], al
.nm:
        cmp word [iob], 0
        je .np
        cmp byte [o_ymode], 2
        je .np
        mov byte [e_io], 1
        call yk_probe
        mov [yk_id_p], al
.np:
        xor ah, ah
        mov al, [yk_id_m]
        mov si, msg_yk_t_idm
        call yk_tr
        mov al, [yk_id_p]
        mov si, msg_yk_t_idp
        call yk_tr
        mov byte [e_io], 0
        mov al, [yk_id_m]
        call yk_valid
        jnc .acc
        mov byte [e_io], 1
        mov al, [yk_id_p]
        call yk_valid
        jnc .acc
        ; ninguno da un chip Yukon: si se ha pedido un modo, se sigue con él
        ; (para depurar); si no, no se carga
        mov al, [o_ymode]
        cmp al, 1
        je .acc
        mov byte [e_io], 0
        cmp al, 2
        je .acc
        pop fs
        mov si, msg_yk_noacc
        cmp byte [v86], 0
        je .f
        mov si, msg_yk_v86
.f:
        stc
        ret
.acc:
        call e1_flat
        call yk_probe
        mov [yk_chip], al
        mov dx, 0x011A              ; B2_MAC_CFG: la revisión, arriba
        call yk_r8
        shr al, 4
        mov [yk_rev], al
        mov ah, [yk_chip]
        mov si, msg_yk_t_chip
        call yk_tr
        ; solo la Optima (BCh) y la Optima 2 (BEh): las demás de la familia
        ; tienen otros pasos (PHY, búfer RAM, sumas) que aquí no están
        mov al, [yk_chip]
        cmp al, 0xBC
        je .chip
        cmp al, 0xBE
        je .chip
        cmp byte [o_ymode], 0       ; (con /MEMORIA o /PUERTOS, se intenta)
        jne .chip
        pop fs
        mov si, msg_yk_nochip
        call puts
        mov al, [yk_chip]
        call hex2
        mov si, msg_yk_nochip2
        stc
        ret
.chip:
        ; ---- reinicio (sky2_reset) ----
        mov si, yk_t_rst1
        call yk_tab
        mov dx, 0x1C06              ; estado PCI: quitar los errores apuntados
        call yk_r16
        or ax, 0xF900
        call yk_w16
        mov si, yk_t_rst2
        call yk_tab
        ; encendido (sky2_power_on), con el «control de energía avanzado»
        mov dx, 0x1C84              ; PCI_DEV_REG4: solo quedan los bits de ASPM
        call yk_r32
        and eax, 0x0000F000
        call yk_w32
        mov dx, 0x1C88              ; PCI_DEV_REG5
        call yk_r32
        and eax, 0x18000000
        call yk_w32
        mov si, yk_t_rst3
        call yk_tab
        mov dx, 0x015C              ; B2_GP_IO: la errata de la carrera del
        call yk_r32                 ; estado (dev. 4.107)
        or ah, 0x20
        call yk_w32
        call yk_r32
        mov si, yk_t_rst4
        call yk_tab
        ; lo propio de la Optima: el temporizador de detección de enlace del
        ; PHY (y, en la revisión 0, que el PHY de PCI Express no se apague)
        mov cx, 3                   ; 0,4 s
        cmp word [yk_chip], 0x00BC  ; ¿Optima, revisión 0?
        jne .nr0
        mov eax, 0x00800080
        mov dx, 0x0170              ; Y2_PEX_PHY_DATA: registro 80h, bit 7
        call yk_w32
        mov eax, 0x40080000         ; y el de depuración 8 (bit 12 a cero)
        call yk_w32
        mov cx, 10                  ; 1,1 s
.nr0:
        mov ax, cx
        shl ax, 4
        or al, 1                    ; reinicio de la detección de enlace
        mov dx, 0x0158              ; B2_TST_CTRL1: se puede escribir en la
        push ax                     ; configuración PCI
        mov al, 0x02
        call yk_w8
        pop ax
        mov dx, 0x1D68              ; PSM_CONFIG_REG4
        call yk_w16
        ; si la gestión de energía (PSM) estaba en marcha, el control del
        ; enlace PCI Express vuelve a como lo guardó
        mov dx, 0x1D64              ; PSM_CONFIG_REG3
        call yk_r16
        test al, 3
        jz .nps
        push ax
        mov al, 0x10                ; capacidad PCI Express
        call yk_cap
        pop ax
        jc .nps
        add dx, 0x1C00 + 0x10       ; su registro de control del enlace
        call yk_w16
.nps:
        mov si, yk_t_rst5
        call yk_tab
        ; la MAC (la carga el chip de su memoria al salir del reinicio)
        mov dx, 0x0100              ; B2_MAC_1
        mov di, mac
        mov cx, 6
.mc:
        call yk_r8
        mov [di], al
        inc di
        inc dx
        loop .mc
        mov ax, [mac+4]
        mov si, msg_yk_t_rst
        call yk_tr
        ; ---- MAC y PHY (sky2_mac_init) ----
        mov si, yk_t_mac1
        call yk_tab
        mov dx, 0x0F08              ; GMAC_IRQ_SRC (se borra al leerlo)
        call yk_r16
        ; el PHY, encendido (PCI_DEV_REG1, bit 26) y fuera de su reinicio
        mov dx, 0x0158
        mov al, 0x02
        call yk_w8
        mov dx, 0x1C40
        call yk_r32
        and eax, ~(1 << 26)
        call yk_w32
        mov dx, 0x0158
        mov al, 0x01
        call yk_w8
        mov dx, 0x1C40
        call yk_r32
        mov dx, 0x0F04              ; GPHY_CTRL
        mov al, 0x02
        call yk_w8
        ; ---- el PHY (sky2_phy_init) ----
        mov bl, 2
        call yk_phy_r
        mov [yk_phyid], ax
        mov si, msg_yk_t_phy
        call yk_tr
        mov bl, 3
        call yk_phy_r
        mov [yk_phyid+2], ax
        mov bl, 20                  ; control ampliado: reloj de 25 MHz hacia
        call yk_phy_r               ; el MAC y contadores de bajada de
        and ax, ~0x0F70             ; velocidad
        or ax, 0x0170
        call yk_phy_w
        mov bl, 16                  ; control propio: sin detección de energía
        call yk_phy_r               ; y cruce de pares automático
        and ax, ~0x0300
        or ax, 0x0060
        call yk_phy_w
        mov dx, 0x2804              ; GM_GP_CTRL: sin control de flujo; la
        mov ax, 0x2012              ; velocidad y el dúplex, los del PHY
        call yk_w16
        mov bl, 9                   ; 1000BASE-T: dúplex completo y medio
        mov ax, 0x0300
        call yk_phy_w
        mov bl, 4                   ; se anuncia 10 y 100, completo y medio
        mov ax, 0x01E1              ; (sin pausa)
        call yk_phy_w
        xor bl, bl                  ; reinicio del PHY y autonegociación
        mov ax, 0x9200
        call yk_phy_w
        cmp word [yk_chip], 0x00BC  ; Optima revisión 0: arreglo de la
        jne .nafe                   ; terminación (RDAC) del PHY
        mov bl, 22
        mov ax, 0x00FF
        call yk_phy_w
        mov bl, 24
        mov ax, 0x2800
        call yk_phy_w
        mov bl, 23
        mov ax, 0x2001
        call yk_phy_w
        mov bl, 22
        xor ax, ax
        call yk_phy_w
.nafe:
        mov bl, 18                  ; interrupciones del PHY: autonegociación
        mov ax, 0x6C00              ; acabada, y cambios de enlace, velocidad
        call yk_phy_w               ; y dúplex
        ; contadores del MAC a cero (se borran al leerlos en este modo)
        mov dx, 0x2888              ; GM_PHY_ADDR
        call yk_r16
        push ax
        or al, 0x20
        call yk_w16
        mov dx, 0x2900
.mib:
        call yk_r16
        add dx, 4
        cmp dx, 0x2A5C
        jbe .mib
        pop ax
        mov dx, 0x2888
        call yk_w16
        ; la dirección, en el MAC (las dos: la de las tramas y la de pausa)
        mov dx, 0x281C              ; GM_SRC_ADDR_1L
        call .addr
        mov dx, 0x2828              ; GM_SRC_ADDR_2L
        call .addr
        mov si, yk_t_mac2
        call yk_tab
        ; el búfer RAM (las Optima no lo tienen; si lo hubiera, se dice)
        mov dx, 0x011C              ; B2_E_0
        call yk_r8
        mov [yk_ram], al
        ; esperar al enlace (la autonegociación tarda de 2 a 4 s): así el
        ; primer CONFRED ya puede enviar. Como mucho 5 s (sin cable).
        mov cx, 45
.lw:
        pushf
        cli
        call yk_linkchk
        popf
        cmp byte [yk_lnk], 0
        jne .lok
        push cx
        mov cx, 1
        call ticks_wait
        pop cx
        loop .lw
.lok:
        mov ax, [yk_phst]
        mov si, msg_yk_t_lnk
        call yk_tr
        pop fs
        mov si, name_yukon
        cmp byte [yk_chip], 0xBE
        jne .nm2
        mov si, name_yukon2
.nm2:
        call set_name
        mov word [c_send], yk_send
        mov word [c_isr], yk_isr
        mov word [c_stop], yk_stop
        mov word [c_link], yk_link
        mov word [c_start], yk_start
        mov word [need_dma], YK_SIZE
        mov word [dma_align], 4096
        mov word [dma_lay], yk_layout
        clc
        ret
; la MAC a los tres registros de 16 bits que empiezan en DX (cada 4)
.addr:
        mov si, mac
        mov cx, 3
.a:
        lodsw
        call yk_w16
        add dx, 4
        loop .a
        ret

; AL = capacidad PCI -> DX = su sitio en la configuración; CF si no la hay
yk_cap:
        push bx
        push cx
        push di
        mov dh, al
        mov bx, [pci_bdf]
        mov ax, 0xB108
        mov di, 0x34
        int 0x1A
        mov dl, 48
.l:
        and cl, 0xFC
        jz .no
        movzx di, cl
        mov ax, 0xB109
        int 0x1A
        cmp cl, dh
        je .ok
        mov cl, ch
        dec dl
        jnz .l
.no:
        stc
        jmp short .r
.ok:
        mov dx, di
        clc
.r:
        pop di
        pop cx
        pop bx
        ret

yk_t_rst1:
        yw8  0x0E68, 0x08           ; B28_Y2_ASF_STAT_CMD: ASF en reinicio
        yw16 0x0004, 0x1000         ; B0_CTST: ASF desactivado
        yw8  0x0004, 0x01           ; reinicio por programa
        yw8  0x0004, 0x02
        yw8  0x0158, 0x02           ; B2_TST_CTRL1: escribir en la conf. PCI
        dw 0
yk_t_rst2:
        yw8  0x0004, 0x08           ; fuera el reinicio maestro
        yw32 0x1D04, 0xFFFFFFFF     ; errores de PCI Express (AER), borrados
        yw8  0x0007, 0xA6           ; B0_POWER_CTRL: alimentación de VCC
        yw32 0x0120, 0x01           ; B2_Y2_CLK_CTRL: sin dividir el reloj
        yw8  0x011D, 0x00           ; B2_Y2_CLK_GATE
        yw32 0x1C80, 0              ; PCI_DEV_REG3: todos los relojes
        dw 0
yk_t_rst3:
        yw32 0x1C94, 0              ; PCI_CFG_REG_1
        yw16 0x0004, 0x8000         ; B0_CTST: Y2_HW_WOL_ON
        dw 0
yk_t_rst4:
        yw16 0x0004, 0x0200         ; luz de «controlador cargado»
        yw8  0x0158, 0x01           ; conf. PCI, otra vez protegida
        yw8  0x0F10, 0x01           ; GMAC_LINK_CTRL: reinicio del enlace
        yw8  0x0F10, 0x02
        dw 0
yk_t_rst5:
        yw8  0x0158, 0x01
        yw32 0x0170, 0x40080000     ; PHY de PCI Express: gestión de energía
        yw32 0x0168, 0x01           ; B2_I2C_IRQ: ruido de I2C
        yw8  0x0138, 0x02           ; B2_TI_CTRL: temporizador parado
        yw8  0x0138, 0x01
        yw32 0x0E08, 0x01           ; B28_DPT_CTRL: sin sondeo de descriptores
        yw8  0x0E18, 0x02           ; GMAC_TI_ST_CTRL: sin marca de tiempo
        yw8  0x0E18, 0x01
        yw8  0x0210, 0x02           ; TXA_CTRL: árbitro de envío
        yw8  0x01A0, 0x02           ; B3_RI_CTRL: interfaz de RAM
        yw8  0x0190, 36             ; y sus doce plazos
        yw8  0x0191, 36
        yw8  0x0192, 36
        yw8  0x0193, 36
        yw8  0x0194, 36
        yw8  0x0195, 36
        yw8  0x0196, 36
        yw8  0x0197, 36
        yw8  0x0198, 36
        yw8  0x0199, 36
        yw8  0x019A, 36
        yw8  0x019B, 36
        yw32 0x000C, 0              ; B0_IMSK: sin interrupciones (yk_start)
        yw32 0x0014, 0              ; B0_HWE_IMSK: ni las de error de hardware
        yw8  0x0F0C, 0              ; GMAC_IRQ_MSK: ni las del MAC
        yw16 0x2834, 0              ; filtro de multidifusión, vacío
        yw16 0x2838, 0
        yw16 0x283C, 0
        yw16 0x2840, 0
        dw 0
yk_t_mac1:
        yw8  0x0F04, 0x01           ; GPHY_CTRL: reinicio del PHY
        yw8  0x0F04, 0x02
        yw8  0x0F00, 0x02           ; GMAC_CTRL: fuera el reinicio del MAC
        dw 0
yk_t_mac2:
        yw16 0x2808, 0x1000         ; GM_TX_CTRL: umbral de colisiones
        yw16 0x280C, 0xA000         ; GM_RX_CTRL: solo las tramas para esta
                                    ; dirección (y las de difusión y
                                    ; multidifusión), sin el CRC
        yw16 0x2810, 0xFFFF         ; GM_TX_FLOW_CTRL
        yw16 0x2814, 0xD7C4         ; GM_TX_PARAM
        yw16 0x2818, 0x221E         ; GM_SERIAL_MODE
        yw16 0x2850, 0              ; sin interrupciones de los contadores
        yw16 0x2854, 0
        yw16 0x2858, 0
        yw8  0x0C48, 0x02           ; RX_GMF_CTRL_T: FIFO de recepción
        yw32 0x0C48, 0x88           ; en marcha, tirando las tramas malas
        yw16 0x0C4C, 0x187B         ; RX_GMF_FL_MSK: cuáles son malas
        yw16 0x0C50, 0x0B           ; RX_GMF_FL_THR
        yw8  0x0D48, 0x02           ; TX_GMF_CTRL_T: FIFO de envío
        yw16 0x0D48, 0x08
        yw16 0x0C58, 128            ; umbrales de pausa (sin búfer RAM)
        yw16 0x0C5A, 96
        yw32 0x0D48, 0x40000000     ; guardar la trama entera antes de enviarla
        yw8  0x0F00, 0x04           ; GMAC_CTRL: sin pausa
        dw 0

yk_id_m     db 0xFF                 ; identificador leído por memoria
yk_id_p     db 0xFF                 ; y por los puertos (FFh: no se ha podido)
yk_ram      db 0
yk_live     db 0                    ; /INFO con el residente cargado
yk_phyid    dw 0, 0
res_seg     dw 0

; ---- /INFO: lo de la Yukon (también con el residente cargado) ----
yk_info:
        push fs
        call e1_flat
        mov dx, 0x011C              ; B2_E_0: el búfer RAM
        call yk_r8
        mov [yk_ram], al
        mov si, msg_yk_acc_m
        cmp byte [e_io], 0
        je .a
        mov si, msg_yk_acc_p
.a:
        call puts
        mov eax, [mmio]
        push eax
        shr eax, 16
        call hex4
        pop eax
        call hex4
        mov si, msg_yk_acc2
        call puts
        cmp byte [yk_live], 0
        jne .nid
        mov si, msg_yk_ids          ; (al cargar: lo leído por los dos caminos)
        call puts
        mov al, [yk_id_m]
        call hex2
        mov si, msg_yk_ids2
        call puts
        mov al, [yk_id_p]
        call hex2
        mov si, msg_yk_ids3
        call puts
.nid:
        mov si, msg_yk_chip
        call puts
        mov al, [yk_chip]
        call hex2
        mov si, msg_yk_chip2
        call puts
        mov al, [yk_rev]
        call hex2
        mov si, yk_dump             ; los registros
        call .dump
        ; el PHY (el 19 no: se borra al leerlo y es de la interrupción)
        mov si, msg_yk_phy
        call puts
        mov si, yk_phyregs
.p:
        lodsb
        cmp al, 0xFF
        je .pd
        mov bl, al
        call hex2
        mov al, '='
        call putc
        pushf
        cli
        call yk_phy_r
        popf
        call hex4
        mov al, ' '
        call putc
        jmp .p
.pd:
        call crlf
        ; lo que lleva contado el controlador
        mov si, yk_cnt
.c:
        lodsw
        test ax, ax
        jz .cd
        push si
        mov si, ax
        call puts
        pop si
        lodsw
        mov bx, ax
        lodsb
        cmp al, 4
        jne .c2
        mov ax, [bx+2]
        call hex4
.c2:
        mov ax, [bx]
        call hex4
        jmp .c
.cd:
        call crlf
        ; pistas
        cmp byte [yk_live], 0
        je .r
        mov dx, 0x0E9C              ; ¿elementos de estado sin leer?
        call yk_r16
        cmp ax, [yk_st_i]
        je .h2
        mov si, msg_yk_h_irq
        call puts
.h2:
        cmp word [yk_txto], 0
        je .h3
        mov si, msg_yk_h_tx
        call puts
.h3:
        cmp byte [yk_ram], 0
        je .r
        mov si, msg_yk_h_ram
        call puts
.r:
        pop fs
        ret
; SI = tabla (texto, registro, ancho): «texto» y el registro en hexadecimal.
; Ancho 1, 2 o 4; 5 = contador del MAC (32 bits en dos registros de 16)
.dump:
        lodsw
        test ax, ax
        jz .dr
        push si
        mov si, ax
        call puts
        pop si
        lodsw
        mov dx, ax
        lodsb
        cmp al, 1
        jne .d2
        call yk_r8
        call hex2
        jmp .dump
.d2:
        cmp al, 2
        jne .d4
        call yk_r16
        call hex4
        jmp .dump
.d4:
        cmp al, 4
        jne .d5
        call yk_r32
        jmp short .d32
.d5:
        add dx, 4
        call yk_r16
        shl eax, 16
        sub dx, 4
        call yk_r16
.d32:
        push eax
        shr eax, 16
        call hex4
        pop eax
        call hex4
        jmp .dump
.dr:
        ret

%macro ydump 3
        dw %1, %2
        db %3
%endmacro
yk_dump:
        ydump msg_yd_00, 0x0119, 1
        ydump msg_yd_01, 0x011C, 1
        ydump msg_yd_02, 0x011E, 1
        ydump msg_yd_03, 0x0004, 4
        ydump msg_yd_04, 0x0008, 4
        ydump msg_yd_05, 0x000C, 4
        ydump msg_yd_06, 0x0010, 4
        ydump msg_yd_07, 0x1C04, 4
        ydump msg_yd_08, 0x1C3C, 2
        ydump msg_yd_09, 0x1C40, 4
        ydump msg_yd_10, 0x1C80, 4
        ydump msg_yd_11, 0x1C84, 4
        ydump msg_yd_12, 0x1C88, 4
        ydump msg_yd_13, 0x1D68, 2
        ydump msg_yd_14, 0x2800, 2
        ydump msg_yd_15, 0x2804, 2
        ydump msg_yd_16, 0x280C, 2
        ydump msg_yd_17, 0x2818, 2
        ydump msg_yd_18, 0x0F00, 4
        ydump msg_yd_19, 0x0F04, 4
        ydump msg_yd_20, 0x0C48, 4
        ydump msg_yd_21, 0x0D48, 4
        ydump msg_yd_22, 0x0E80, 4
        ydump msg_yd_23, 0x0E9C, 2
        ydump msg_yd_24, 0x0E88, 4
        ydump msg_yd_25, 0x0434, 4
        ydump msg_yd_26, 0x0460, 2
        ydump msg_yd_27, 0x0464, 2
        ydump msg_yd_28, 0x0458, 4
        ydump msg_yd_29, 0x06B4, 4
        ydump msg_yd_30, 0x06E0, 2
        ydump msg_yd_31, 0x06E4, 2
        ydump msg_yd_32, 0x06A4, 2
        ydump msg_yd_33, 0x0E90, 2
        ydump msg_yd_34, 0x2900, 5
        ydump msg_yd_35, 0x2908, 5
        ydump msg_yd_36, 0x2918, 5
        ydump msg_yd_37, 0x2920, 5
        ydump msg_yd_38, 0x29B0, 5
        ydump msg_yd_39, 0x29C0, 5
        ydump msg_yd_40, 0x29C8, 5
        ydump msg_yd_41, 0x2A58, 5
        dw 0
msg_yd_00   db 13, 10, "Yukon, placa:    PMD=", 0
msg_yd_01   db " RAM=", 0
msg_yd_02   db " HW_RES=", 0
msg_yd_03   db " CTST=", 0
msg_yd_04   db 13, 10, "Yukon, interr.:  ISRC=", 0
msg_yd_05   db " IMSK=", 0
msg_yd_06   db " HWE_ISRC=", 0
msg_yd_07   db 13, 10, "Yukon, PCI:      CMD/STAT=", 0
msg_yd_08   db " IRQ=", 0
msg_yd_09   db " REG1=", 0
msg_yd_10   db 13, 10, "                 REG3=", 0
msg_yd_11   db " REG4=", 0
msg_yd_12   db " REG5=", 0
msg_yd_13   db " PSM4=", 0
msg_yd_14   db 13, 10, "Yukon, MAC:      GP_STAT=", 0
msg_yd_15   db " GP_CTRL=", 0
msg_yd_16   db " RX_CTRL=", 0
msg_yd_17   db " SERIAL=", 0
msg_yd_18   db 13, 10, "                 GMAC_CTRL=", 0
msg_yd_19   db " GPHY_CTRL=", 0
msg_yd_20   db 13, 10, "                 RX_GMF=", 0
msg_yd_21   db " TX_GMF=", 0
msg_yd_22   db 13, 10, "Yukon, estado:   CTRL=", 0
msg_yd_23   db " PUT=", 0
msg_yd_24   db " LISTA=", 0
msg_yd_25   db 13, 10, "Yukon, recep.:   CSR=", 0
msg_yd_26   db " GET=", 0
msg_yd_27   db " PUT=", 0
msg_yd_28   db " LISTA=", 0
msg_yd_29   db 13, 10, "Yukon, envío:    CSR=", 0
msg_yd_30   db " GET=", 0
msg_yd_31   db " PUT=", 0
msg_yd_32   db " HECHO=", 0
msg_yd_33   db " DICHO=", 0
msg_yd_34   db 13, 10, "Yukon, tramas:   rx_uni=", 0
msg_yd_35   db " rx_dif=", 0
msg_yd_36   db " rx_multi=", 0
msg_yd_37   db " rx_crc=", 0
msg_yd_38   db 13, 10, "                 rx_fifo=", 0
msg_yd_39   db " tx_uni=", 0
msg_yd_40   db " tx_dif=", 0
msg_yd_41   db " tx_fifo=", 0
msg_yk_phy  db 13, 10, "Yukon, PHY:      ", 0
yk_phyregs  db 0, 1, 2, 3, 4, 5, 9, 10, 16, 17, 18, 0xFF
%macro ycnt 3
        dw %1, %2
        db %3
%endmacro
yk_cnt:
        ycnt msg_yc_0, yk_irqs, 4
        ycnt msg_yc_1, yk_lastis, 4
        ycnt msg_yc_2, yk_stle, 4
        ycnt msg_yc_3, yk_st_i, 2
        ycnt msg_yc_4, yk_rx_put, 2
        ycnt msg_yc_5, yk_rx_nxt, 2
        ycnt msg_yc_6, yk_lastrx, 4
        ycnt msg_yc_7, yk_tx_prd, 2
        ycnt msg_yc_8, yk_txdn, 2
        ycnt msg_yc_9, yk_txto, 2
        ycnt msg_yc_a, yk_phst, 2
        ycnt msg_yc_b, yk_lnk, 2
        ycnt msg_yc_c, yk_up, 2
        ycnt msg_yc_d, dma_seg, 2
        ycnt msg_yc_e, dma_phys, 4
        dw 0
msg_yc_0    db "Yukon, driver:   irqs=", 0
msg_yc_1    db " causa=", 0
msg_yc_2    db " elem_estado=", 0
msg_yc_3    db " i_estado=", 0
msg_yc_4    db 13, 10, "                 rx_put=", 0
msg_yc_5    db " rx_bufer=", 0
msg_yc_6    db " rx_estado=", 0
msg_yc_7    db 13, 10, "                 tx_put=", 0
msg_yc_8    db " tx_dicho=", 0
msg_yc_9    db " tx_sin_confirmar=", 0
msg_yc_a    db 13, 10, "                 phy17=", 0
msg_yc_b    db " enlace/vel=", 0
msg_yc_c    db " mac/arrancado=", 0
msg_yc_d    db " dma_seg=", 0
msg_yc_e    db " dma_fis=", 0
msg_yk_acc_m db "Registros:       por memoria (modo «irreal»), en ", 0
msg_yk_acc_p db "Registros:       por los puertos (bloques con RAP); en memoria, ", 0
msg_yk_acc2 db "h", 13, 10, 0
msg_yk_ids  db "                 identificador leído por memoria: ", 0
msg_yk_ids2 db "h; por los puertos: ", 0
msg_yk_ids3 db "h (FF: no se pudo)", 13, 10, 0
msg_yk_chip db "Chip:            Yukon 2, identificador ", 0
msg_yk_chip2 db "h (BC = Optima), revisión ", 0
msg_yk_h_irq db "  (!) Hay elementos de estado sin leer: ¿no llega la interrupción?", 13, 10, 0
msg_yk_h_tx db "  (!) Hay envíos que la tarjeta no ha confirmado (HECHO/DICHO no avanzan).", 13, 10, 0
msg_yk_h_ram db "  (!) La tarjeta tiene búfer RAM y aquí no se programa.", 13, 10, 0
msg_yk_tr   db "  [Yukon] ", 0
msg_yk_t_idm db "identificador del chip por memoria: ", 0
msg_yk_t_idp db "identificador del chip por los puertos: ", 0
msg_yk_t_chip db "chip (identificador y revisión): ", 0
msg_yk_t_rst db "reinicio hecho; final de la MAC: ", 0
msg_yk_t_phy db "PHY encendido; su identificador (registro 2): ", 0
msg_yk_t_lnk db "PHY y MAC puestos; registro 17 del PHY (enlace = bit 10): ", 0
msg_yk_noacc db "La tarjeta Yukon no contesta ni por memoria ni por los puertos", 13, 10
            db "(HSPRED /DEPURA dice lo que se lee; /MEMORIA o /PUERTOS lo fuerzan).", 13, 10, 0
msg_yk_v86  db "En modo V86 (con HSPEMM u otro EMM386) a la tarjeta Yukon solo se llega por", 13, 10
            db "los puertos, y por ahí no contesta: arranque en «Normal».", 13, 10, 0
msg_yk_nochip db "Esta tarjeta Marvell Yukon 2 (chip ", 0
msg_yk_nochip2 db "h) todavía no se sabe usar: solo la Optima.", 13, 10, 0
name_yukon  db "Marvell Yukon 88E8059 (Optima)", 0
name_yukon2 db "Marvell Yukon Optima 2", 0

; ---- DEC Tulip ----
tu_init:
        mov si, msg_noio
        cmp word [iob], 0
        je .f
        ; su parte residente, a su sitio (tu_base, pegado a la pila): en el
        ; archivo va detrás de la de la PCnet, que aquí no hace falta
        push ds
        pop es
        mov si, pn_end              ; donde está en el archivo
        mov di, tu_base             ; para donde está ensamblada
        mov cx, TU_LEN
        rep movsb
        mov word [res_end], tu_end
        ; 21142/21143: despertarla (CFDD, registro 40h de la configuración
        ; PCI: bits 31 «dormida» y 30 «adormilada»). La 21140 no lo necesita
        ; (Linux tampoco se lo hace).
        cmp word [pci_dev], 0x0019
        jne .nw
        mov bx, [pci_bdf]
        mov di, 0x40
        xor ecx, ecx
        mov ax, 0xB10D
        int 0x1A
        mov cx, 1
        call ticks_wait
.nw:
        ; reinicio por programa (CSR0, bit 0) y modo del bus: ráfagas de 8
        ; palabras dobles alineadas a 8 y descriptores seguidos (DSL = 0)
        mov eax, 1
        xor bl, bl
        call tu_wr
        mov cx, 2
        call ticks_wait
        mov eax, 0x00004800
        xor bl, bl
        call tu_wr
        xor eax, eax
        mov bl, 7
        call tu_wr                  ; sin interrupciones
        mov eax, TU_CSR6
        dec bl
        call tu_wr                  ; parada, con el puerto ya elegido
        ; la SROM (93C46 o 93C66, por el CSR9), entera: 64 palabras
        call tu_ee_bits
        xor bx, bx
        mov di, tu_srom
.rd:
        call tu_ee_read
        mov [di], ax
        add di, 2
        inc bx
        cmp bx, 64
        jb .rd
        ; ¿dónde está la MAC? En el formato de DEC, en el octeto 20; las
        ; tarjetas antiguas la ponían al principio y repetida en el 16 (lo
        ; mismo que mira Linux: si los 8 primeros octetos son iguales que los
        ; 8 del 16, está en el 0)
        mov si, tu_srom
        mov di, tu_srom + 16
        mov cx, 8
        repe cmpsb
        mov si, tu_srom
        je .at
        mov si, tu_srom + 20
.at:
        call .take
        jnc .mac
        ; si ahí no hay una dirección que valga, en el otro sitio
        sub si, tu_srom
        xor si, 20                  ; 0 <-> 20
        add si, tu_srom
        call .take
        mov si, msg_tu_mac
        jc .f
.mac:
        mov si, name_tu40
        cmp word [pci_dev], 0x0009
        je .nm
        mov si, name_tu43
.nm:
        call set_name
        mov word [c_send], tu_send
        mov word [c_isr], tu_isr
        mov word [c_stop], tu_stop
        mov word [c_link], ne_link  ; no se sabe (ver arriba)
        mov word [c_start], tu_start
        mov word [need_dma], TU_SIZE
        mov word [dma_align], 32
        clc
        ret
.f:
        stc
        ret
; SI = 6 octetos de la SROM -> mac; CF = 1 si no vale (todo ceros, todo unos
; o de multidifusión). Conserva SI.
.take:
        push si
        mov di, mac
        mov cx, 6
        rep movsb
        pop si
        mov eax, [mac]
        movzx edx, word [mac+4]
        test al, 1
        jnz .tno
        or edx, eax
        jz .tno
        clc
        ret
.tno:
        stc
        ret

; La SROM serie por el CSR9: bit 11 (SR) la elige y bit 14 (RD) es «leer»;
; bit 0 CS, bit 1 reloj, bit 2 dato hacia la SROM, bit 3 dato de la SROM.
TU_EE       equ 0x4800
; CSR9 = EAX y una espera corta
tu_ee_out:
        push bx
        mov bl, 9
        call tu_wr
        call io_wait
        call io_wait
        pop bx
        ret
; enviar los CL bits altos de BX (el primero, el bit 15), con CS puesto
tu_ee_cmd:
.l:
        mov eax, TU_EE | 1
        shl bx, 1
        jnc .z
        or al, 4
.z:
        call tu_ee_out
        or al, 2                    ; flanco de subida: la SROM lee el bit
        call tu_ee_out
        dec cl
        jnz .l
        ret
; ¿cuántos bits de dirección tiene la SROM? Se le manda «leer» con una
; dirección de todo unos: la SROM contesta con un cero en cuanto ha recibido
; el último bit de la dirección. Si el cero sale tras el 6.º, es de 6 bits
; (93C46); si no, de 8 (93C66). Es la misma prueba que hace Linux.
tu_ee_bits:
        mov eax, TU_EE
        call tu_ee_out
        mov bx, 0xDF80              ; 1 (inicio), 10 (leer) y 6 unos
        mov cl, 9
        call tu_ee_cmd
        mov dx, [iob]
        add dx, 9 * 8
        in eax, dx
        mov byte [tu_abits], 6
        test al, 8
        jz .r
        mov byte [tu_abits], 8
.r:
        mov eax, TU_EE              ; soltar CS: se acaba la orden
        call tu_ee_out
        ret
; BX = número de palabra -> AX. Conserva BX.
tu_ee_read:
        push bx
        push cx
        push dx
        push bx
        mov eax, TU_EE
        call tu_ee_out
        mov bx, 0xC000              ; 1 (inicio) y 10 (leer)
        mov cl, 3
        call tu_ee_cmd
        pop bx
        mov cl, 16
        sub cl, [tu_abits]
        shl bx, cl                  ; la dirección, arriba
        mov cl, [tu_abits]
        call tu_ee_cmd
        mov eax, TU_EE | 1
        call tu_ee_out
        xor bx, bx
        mov cx, 16
.b:
        mov eax, TU_EE | 1 | 2      ; flanco de subida: la SROM pone el bit
        call tu_ee_out
        mov dx, [iob]
        add dx, 9 * 8
        in eax, dx
        shr al, 4                   ; el bit 3, al acarreo
        rcl bx, 1
        mov eax, TU_EE | 1
        call tu_ee_out
        loop .b
        mov eax, TU_EE
        call tu_ee_out
        mov ax, bx
        pop dx
        pop cx
        pop bx
        ret
tu_abits    db 6
tu_srom     times 128 db 0

need_dma    dw 0
dma_align   dw 16
dma_lay     dw 0                    ; reparto propio de los búferes (o 0)
res_end     dw resident_end         ; fin de lo residente (con la sección de
                                    ; quita y pon de la tarjeta, si la tiene)

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
        mov ax, [res_end]
        add ax, 15
        shr ax, 4
        mov [res_paras], ax
        mov cx, [need_dma]
        mov [dma_bytes], cx
        test cx, cx
        jz .ok
        mov word [dma_z0], 0
        ; alineación (en párrafos)
        mov bx, [dma_align]
        shr bx, 4
        dec bx                      ; máscara
        mov ax, cs
        cmp ax, 0xA000
        jae .low
        add ax, [res_paras]
        mov di, ax                  ; lo primero libre
        add ax, bx
        not bx
        and ax, bx
        call .lay
        mov ax, [dma_seg]
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
        mov di, ax
        mov bx, [dma_align]
        shr bx, 4
        dec bx
        add ax, bx
        not bx
        and ax, bx
        call .lay
        cmp word [dma_lay], 0
        je .phys
        ; lo que sobre del bloque, se devuelve
        mov es, [dma_own]
        mov bx, [need_dma]
        add bx, 15
        shr bx, 4
        add bx, [dma_seg]
        sub bx, [dma_own]
        mov ah, 0x4A
        int 0x21
        push cs
        pop es
.phys:
        ; dirección física y continuidad, página a página (desde dma_z0)
        movzx eax, word [dma_seg]
        shl eax, 4
        movzx ebx, word [dma_z0]
        add eax, ebx
        call phys
        sub eax, ebx
        mov [dma_phys], eax
        movzx ebx, word [dma_seg]
        shl ebx, 4
        movzx eax, word [dma_z0]
        add ebx, eax
        movzx ecx, word [dma_seg]
        shl ecx, 4
        movzx eax, word [need_dma]
        add ecx, eax
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
; AX = el segmento alineado; DI = lo primero libre -> dma_seg, dma_z0 y, si
; la tarjeta reparte ella sus búferes (dma_lay), need_dma y dma_bytes
.lay:
        mov [dma_seg], ax
        cmp word [dma_lay], 0
        je .lr
        mov bx, [dma_align]
        shr bx, 4
        sub ax, bx                  ; una alineación por debajo
        mov [dma_seg], ax
        sub di, ax
        shl di, 4                   ; DI = desplazamiento de lo primero libre
        mov [dma_z0], di
        call [dma_lay]              ; -> need_dma (hasta dónde llega)
        mov ax, [need_dma]
        sub ax, [dma_z0]
        mov [dma_bytes], ax
.lr:
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
        mov si, msg_s3u
        cmp byte [card], C_USB
        je .u
        mov si, msg_s3
        call puts
        movzx eax, byte [irq]
        call putdec
        mov si, msg_s4
.u:
        call puts
        mov al, [pkt_vec]
        call hex2
        mov si, msg_s5
        call puts
        ret

show_info:
        call show_info0
        cmp byte [card], C_YUKON
        jne .r
        call yk_info
.r:
        ret
show_info0:
        mov si, msg_i_card
        call puts
        mov si, [name_ptr]
        call puts
        call crlf
        cmp byte [card], C_USB
        jne .pci
        ; adaptador USB: VID:PID, velocidad y controladora; sin IRQ
        mov si, msg_i_usb
        call puts
        mov ax, [pci_ven]
        call hex4
        mov al, ':'
        call putc
        mov ax, [pci_dev]
        call hex4
        mov si, msg_u_fs
        cmp byte [mmio], 3
        jb .us
        mov si, msg_u_hs
        je .us
        mov si, msg_u_ss
.us:
        call puts
        movzx bx, byte [mmio+1]
        and bx, 3
        shl bx, 1
        mov si, [u_hc_tab+bx]
        call puts
        mov si, msg_i_usb2
        call puts
        jmp .npn2
.pci:
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
        ; la revisión (registro 08h de la configuración PCI): con el modelo,
        ; dice qué chip es exactamente
        mov si, msg_i_rev
        call puts
        mov bx, [pci_bdf]
        mov ax, 0xB108
        mov di, 0x08
        int 0x1A
        mov al, cl
        call hex2
        mov al, 'h'
        call putc
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
        push eax
        shr eax, 16
        call hex4
        pop eax
        call hex4
        ; y el «XID» que mira el controlador r8169 de Linux para saber la
        ; familia: los bits 20-31 de TxConfig, sin los 4 y 5
        mov si, msg_i_xid2
        call puts
        pop eax
        shr eax, 20
        and ax, 0x0FCF
        call hex4
        mov bx, r68_fam
.xf:
        mov dx, ax
        and dx, [bx]
        cmp dx, [bx+2]
        je .xh
        add bx, 6
        jmp .xf
.xh:
        mov si, [bx+4]
        call puts
        call crlf
.nv:
        ; PCnet: el número de pieza (2621h = Am79C970A, 2625h = Am79C973)
        cmp byte [card], C_PCNET
        jne .npn
        mov si, msg_i_pnid
        call puts
        mov ax, [chip_id]
        call hex4
        mov si, msg_i_mmh2
        call puts
.npn:
        mov si, msg_i_irq
        call puts
        movzx eax, byte [irq]
        call putdec
        cmp byte [card], C_YUKON
        jne .nsd
        cmp byte [irq], 0
        jne .nsd
        mov si, msg_i_poll
        call puts
.nsd:
        call crlf
.npn2:
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
        cmp byte [o_dbg], 0
        je .r
        ; /DEPURA: también al registro (en la memoria que hay detrás del
        ; programa; al archivo, al acabar cada línea)
        push bx
        mov bx, [log_n]
        mov [log_buf+bx], dl
        inc bx
        mov [log_n], bx
        cmp bx, LOG_MAX
        pop bx
        jae .f
        cmp dl, 10
        jne .r
.f:
        call log_flush
.r:
        pop dx
        pop ax
        ret

; lo que hay en el registro, al final del archivo de /DEPURA
log_flush:
        pusha
        mov cx, [log_n]
        jcxz .r
        mov dx, log_name
        mov ax, 0x3D02
        int 0x21
        jnc .op
        mov ah, 0x3C
        xor cx, cx
        int 0x21
        jc .z
.op:
        mov bx, ax
        mov ax, 0x4202
        xor cx, cx
        xor dx, dx
        int 0x21
        mov ah, 0x40
        mov cx, [log_n]
        mov dx, log_buf
        int 0x21
        mov ah, 0x3E
        int 0x21
.z:
        mov word [log_n], 0
.r:
        popa
        ret
LOG_MAX     equ 200                 ; (una línea, como mucho)
log_n       dw 0
log_name    db "HSPRED.LOG", 0
            times 64 - 11 db 0

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
msg_usage   db "Uso: HSPRED [/INT:xx] [/INFO] [/AUTO] [/USB] [/DEPURA[:archivo]] [/U]", 13, 10
            db "  /INT:xx  vector del packet driver (60 a 80, hexadecimal; si no, el", 13, 10
            db "           primero libre desde el 60)", 13, 10
            db "  /INFO    tarjeta, MAC, enlace, IRQ, vector y la configuración IP", 13, 10
            db "  /AUTO    sin tarjeta, sale en silencio (para el AUTOEXEC.BAT)", 13, 10
            db "  /USB     el adaptador USB aunque haya una tarjeta PCI que valga", 13, 10
            db "  /DEPURA  lo que sale por la pantalla, además a HSPRED.LOG (o al archivo", 13, 10
            db "           que se diga); con /INFO, el informe para quien depura", 13, 10
            db "  /U       descargarlo de la memoria", 13, 10
            db "Tarjetas: NE2000 PCI (RTL8029), Realtek RTL8139 y RTL8111/8168/8101, Intel", 13, 10
            db "PRO/1000 (e1000, 82574L), AMD PCnet (VirtualBox, VMware) y DEC 21140/21143", 13, 10
            db "(Hyper-V: adaptador heredado) y Marvell Yukon 88E8059 (HP Mini 5103; con", 13, 10
            db "/MEMORIA o /PUERTOS se elige por dónde van sus registros y con /SONDEO no", 13, 10
            db "usa IRQ). Y, con HSPUSB.SYS cargado, un adaptador", 13, 10
            db "USB-Ethernet con CDC-ECM (Realtek RTL8153). La IP: CONFRED (IPCONFIG).", 13, 10
            db "Sirve también para los programas de DOS que usan packet drivers (mTCP...).", 13, 10, 0
msg_already db "HSPRED ya está cargado (HSPRED /INFO lo enseña; /U lo descarga).", 13, 10, 0
msg_notres  db "HSPRED no está cargado.", 13, 10, 0
msg_unloaded db "HSPRED descargado.", 13, 10, 0
msg_noun1   db "No se puede descargar: hay otro programa residente cargado después.", 13, 10, 0
msg_noun2   db "No se puede descargar: hay un programa usando la red.", 13, 10, 0
msg_nocard  db "No se ha encontrado ninguna tarjeta de red que HSPRED sepa usar.", 13, 10
            db "(Un adaptador USB-Ethernet lo lleva HSPUSB.SYS, en CONFIG.SYS: tiene que", 13, 10
            db "estar cargado con /RED y con el adaptador conectado al arrancar.)", 13, 10, 0
msg_unk_eth db "  Hay una tarjeta Ethernet que todavía no se sabe usar: ", 0
msg_unk_wifi db "  Hay una tarjeta inalámbrica (Wi-Fi), que todavía no se sabe usar: ", 0
msg_pci_at  db " en ", 0
msg_noirq   db "La tarjeta no tiene IRQ asignado por la BIOS.", 13, 10, 0
msg_noio    db "La tarjeta no tiene puertos de E/S asignados.", 13, 10, 0
msg_noreset db "La tarjeta no termina de reiniciarse.", 13, 10, 0
msg_pn_no   db "La tarjeta PCnet no contesta por sus puertos (modo de 32 bits, DWIO).", 13, 10, 0
msg_pn_mac  db "La tarjeta PCnet no da su dirección MAC.", 13, 10, 0
msg_pn_idon db "La tarjeta PCnet no termina su inicialización (INIT sin IDON).", 13, 10, 0
msg_tu_mac  db "La tarjeta Tulip no da su dirección MAC (SROM).", 13, 10, 0
msg_nommio  db "La tarjeta no tiene sus registros en memoria asignados.", 13, 10, 0
msg_e1v86   db "Esta tarjeta e1000 solo da sus registros en memoria, y en modo V86 (con", 13, 10
            db "HSPEMM u otro EMM386) no se llega a ellos: arranque en «Normal».", 13, 10, 0
msg_novec   db "No hay ningún vector libre para el packet driver (60h-80h).", 13, 10, 0
msg_nodmamem db "No hay memoria convencional para los búferes de la tarjeta.", 13, 10, 0
msg_nocont  db "Los búferes de la tarjeta no quedan en memoria física continua.", 13, 10, 0
msg_s1      db "Tarjeta ", 0
msg_s2      db ", MAC ", 0
msg_s3      db ", IRQ ", 0
msg_s3u     db ", por HSPUSB, packet driver en INT ", 0
msg_s4      db ", packet driver en INT ", 0
msg_s5      db "h", 13, 10, 0
msg_i_card  db "Tarjeta:         ", 0
msg_i_pci   db "PCI:             ", 0
msg_i_io    db "Puertos:         ", 0
msg_i_mm    db "Registros:       ", 0
msg_i_mmh   db "h (registros por memoria)", 13, 10, 0
msg_i_mmp   db "h (registros por los puertos)", 13, 10, 0
msg_i_irq   db "IRQ:             ", 0
msg_i_poll  db " (/SONDEO: sin IRQ; se mira la tarjeta en cada tic del reloj)", 0
msg_i_rev   db ", revisión ", 0
msg_i_pnid  db "Chip (CSR88/89): ", 0
msg_i_xid   db "Chip (TxConfig): ", 0
msg_i_mmh2  db "h", 13, 10, 0
msg_i_xid2  db "h, XID ", 0
; familias de la RTL8168/8101/8169 por el XID (máscara, valor, nombre), en el
; orden en que las mira r8169 (Linux, rtl8169_get_mac_version)
r68_fam:
        dw 0x7CF, 0x541, r68_n_h
        dw 0x7C8, 0x548, r68_n_17
        dw 0x7CF, 0x502, r68_n_ep
        dw 0x7CF, 0x5C8, r68_n_g
        dw 0x7CF, 0x509, r68_n_g
        dw 0x7CF, 0x4C0, r68_n_g
        dw 0x7C8, 0x488, r68_n_f
        dw 0x7C8, 0x480, r68_n_f
        dw 0x7C8, 0x2C8, r68_n_evl
        dw 0x7C8, 0x2C0, r68_n_e
        dw 0x7C8, 0x288, r68_n_dp
        dw 0x7C8, 0x280, r68_n_d
        dw 0x7C0, 0x3C0, r68_n_c
        dw 0x7C8, 0x380, r68_n_b
        dw 0x7C8, 0x300, r68_n_b
        dw 0x7C8, 0x448, r68_n_06
        dw 0x7C8, 0x440, r68_n_02
        dw 0x7C8, 0x408, r68_n_05
        dw 0x7C8, 0x348, r68_n_2e
        dw 0x7C8, 0x248, r68_n_2e
        dw 0x7CF, 0x240, r68_n_01
        dw 0x7C8, 0x340, r68_n_1e
        dw 0xFC8, 0x980, r68_n_69
        dw 0xFC8, 0x180, r68_n_69
        dw 0xFC8, 0x100, r68_n_69
        dw 0xFC8, 0x040, r68_n_69
        dw 0xFC8, 0x008, r68_n_69
        dw 0, 0, r68_n_unk
r68_n_h     db "h (RTL8168H/8111H)", 0
r68_n_17    db "h (RTL8117)", 0
r68_n_ep    db "h (RTL8168EP/8111EP)", 0
r68_n_g     db "h (RTL8168G/8111G)", 0
r68_n_f     db "h (RTL8168F/8111F)", 0
r68_n_evl   db "h (RTL8168E-VL/8111E-VL)", 0
r68_n_e     db "h (RTL8168E/8111E)", 0
r68_n_dp    db "h (RTL8168DP/8111DP)", 0
r68_n_d     db "h (RTL8168D/8111D)", 0
r68_n_c     db "h (RTL8168C/8111C)", 0
r68_n_b     db "h (RTL8168B/8111B)", 0
r68_n_06    db "h (RTL8106E)", 0
r68_n_02    db "h (RTL8402)", 0
r68_n_05    db "h (RTL8105E)", 0
r68_n_2e    db "h (RTL8102E/8103E)", 0
r68_n_01    db "h (RTL8401)", 0
r68_n_1e    db "h (RTL8101E)", 0
r68_n_69    db "h (RTL8169/8110)", 0
r68_n_unk   db "h (familia sin identificar)", 0
msg_i_mac   db "Dirección MAC:   ", 0
msg_i_usb   db "USB:             ", 0
msg_u_fs    db ", velocidad plena (12 Mbit/s), por ", 0
msg_u_hs    db ", alta velocidad (480 Mbit/s), por ", 0
msg_u_ss    db ", supervelocidad (USB 3), por ", 0
u_hc_tab    dw msg_u_uhci, msg_u_ohci, msg_u_ehci, msg_u_xhci
msg_u_uhci  db "UHCI", 0
msg_u_ohci  db "OHCI", 0
msg_u_ehci  db "EHCI", 0
msg_u_xhci  db "xHCI", 0
msg_i_usb2  db " (lo lleva HSPUSB)", 13, 10
            db "IRQ:             no usa (HSPUSB mira si hay tramas en cada tic del reloj)", 13, 10, 0
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
; el registro de /DEPURA, en la memoria que queda detrás del programa (no
; ocupa en el archivo; como mucho una línea cada vez)
log_buf:
