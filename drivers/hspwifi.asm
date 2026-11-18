; =============================================================================
;  HSPWIFI.COM - controlador Wi-Fi de Hispa-486 (residente, packet driver)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Uso:  HSPWIFI [/V] [/PANTALLA] [/REG:archivo] [/INT:xx] [/JUNTO] [/SIMULA]
;        HSPWIFI /U        descargarlo
;  /JUNTO: convivir con HSPRED ya cargado (para depurar el Wi-Fi por la
;  consola remota de la red por cable): HSPWIFI no contesta a INT 2Fh AH=DAh
;  y su packet driver queda en otro vector; las órdenes de red siguen
;  usando el de HSPRED.
;
;  Para las tarjetas Broadcom BCM43224/43225 (PCI 14E4:4353, 14E4:4357), con
;  WPA2-Personal. Se presenta como un packet driver Ethernet corriente (la
;  misma interfaz que HSPRED, también la INT 2Fh AH=DAh), así que CONFRED,
;  PING, DESCARGAR, ACTUALIZAR, CONSOLA y los demás funcionan sin cambios.
;  La red y la contraseña se eligen con la orden WIFI (ver docs/WIFI.md).
;
;  Este archivo es solo la parte de modo real: lo residente son unos 4 KB.
;  El controlador de verdad (la radio, 802.11 y WPA2) es HSPWIFI.BIN, un
;  programa de 32 bits que se carga en un bloque de memoria extendida (XMS).
;  Para cada llamada, el residente pasa a modo protegido con su propia tabla
;  de segmentos (base = el bloque: el núcleo no se reubica), llama al núcleo
;  y vuelve a modo real. Por eso NO funciona en modo V86 (con HSPEMM): lo
;  dice y no se carga. Con HSPDPMI sí (HSPDPMI vuelve a modo real de verdad
;  para las interrupciones).
;
;  La tarjeta no usa su interrupción (se deja apagada en el PCI): el núcleo
;  la mira en cada tic del reloj (INT 08h) y cada vez que un programa consulta
;  el teclado (INT 16h, AH=01h u 11h), como hace HSPUSB con la red por USB.
;
;  Archivos, en \HISPADOS de la unidad de arranque:
;    HSPWIFI.BIN   el núcleo (wifi/, ver docs/WIFI.md)
;    BCM43XX.FW    microcódigo de Broadcom (brcm/bcm43xx-0.fw de linux-firmware)
;    BCM43XXH.FW   su índice (brcm/bcm43xx_hdr-0.fw)
;
;  INT 2Fh AH=DAh: lo mismo que HSPRED (ver drivers/hspred.asm).
;  INT 2Fh AH=DCh (para la orden WIFI y Hispa Ventanas):
;    AL=00h -> AL = FFh, BX = 5746h («WF»), CX = versión
;    AL=01h  llamar al núcleo: CX = función (wifi/nucleo/llamadas.h, de la 2
;            a la 14), DS:SI = petición (dobles palabras; las direcciones,
;            lineales: segmento*16+desplazamiento) -> DX:AX = resultado
;    AL=02h -> ES:BX = datos del residente: +0 el archivo del registro
;            (ASCIIZ, vacío si no hay), +64 opciones (bit 0 simulada, bit 1
;            /V, bit 2 /PANTALLA, bit 3 /JUNTO)
;    AL=03h  descargar (BL = 1: aunque haya programas usándolo) -> AL = 0
;            hecho, 1 otro residente encima, 2 en uso
; =============================================================================
        cpu 586
        bits 16
        org 0x100

        section .text
        section .init follows=.text
        section .text

NH          equ 8                   ; manejadores (tipos de paquete)
PILA        equ 768
VERSION     equ 1
C_WIFI      equ 8                   ; «tarjeta» para quien lea los datos de /INFO
CFG_LEN     equ 30
BLOQUE_KB   equ 2048                ; memoria extendida para el núcleo (más 64 KB
                                    ; para alinear su base)

; funciones del núcleo (wifi/nucleo/llamadas.h)
LL_INICIA   equ 0
LL_ENCIENDE equ 1
LL_SONDEA   equ 2
LL_RECIBE   equ 3
LL_ENVIA    equ 4
LL_ESTADO   equ 9
LL_REGISTRO equ 10
LL_PARA     equ 13
LL_MAC      equ 14
LL_MONTA    equ 15

; selectores de la tabla propia
SEL_C32     equ 0x08                ; código de 32 bits, base = el bloque
SEL_D32     equ 0x10                ; datos de 32 bits, base = el bloque
SEL_C16     equ 0x18                ; código de 16 bits, base = este segmento
SEL_D16     equ 0x20                ; datos de 16 bits (para volver a modo real)

start:
        jmp init

; =============================================================================
;  Entrada del packet driver (el vector apunta aquí; la firma, 3 más allá)
; =============================================================================
pkt_entry:
        jmp near pkt_main           ; 3 octetos
        db "PKT DRVR", 0

; =============================================================================
;  Datos residentes. De «info» a «info_end», con los mismos desplazamientos
;  que en HSPRED (Configuración de Hispa Ventanas lee de aquí la MAC).
; =============================================================================
info:
card        db C_WIFI
irq         db 0
pkt_vec     db 0
irq_vec     db 0
pic_was     db 0
e_io        db 0
iob         dw 0
mmio        dd 0
pci_bdf     dw 0
pci_ven     dw 0
pci_dev     dw 0
mac         times 6 db 0
name_ptr    dw card_name
rx_mode     dw 3
dma_seg     dw 0
dma_own     dw 0
dma_phys    dd 0
res_paras   dw 0
dma_bytes   dw 0
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
chip_id     dw 0
card_name   db "Wi-Fi Broadcom BCM43224", 0
            times 36 - ($ - card_name) db 0
info_end:

; lo que lee la orden WIFI (INT 2Fh AX=DC02h)
datos_wifi:
reg_ruta    times 64 db 0
opciones    db 0                    ; bit 0 /SIMULA, 1 /V, 2 /PANTALLA, 3 /JUNTO

old2f       dd 0
old08       dd 0
old16       dd 0
oldpkt      dd 0
xms         dd 0                    ; la entrada del gestor XMS
xms_h       dw 0                    ; el bloque del núcleo
en_nucleo   db 0                    ; dentro de una llamada al núcleo
en_sondeo   db 0                    ; repartiendo lo recibido
a20_puesta  db 0
quedan      db 0
rx_len      dw 0
rx_h        dw 0
rx_rcv      dd 0
rx_buf      dd 0
rx_tmp      times 18 db 0
sv_sp       dw 0
sv_ss       dw 0
rm_sp       dw 0                    ; la pila de modo real durante una llamada
rm_ss       dw 0
pet         times 4 dd 0            ; petición de lo residente
pet_fuera   times 4 dd 0            ; la de envío y la del estado (fuera del sondeo)

; para pasar a modo protegido y volver
        align 4
entrada32   dd 0                    ; llamada lejana de 32 bits al núcleo
            dw SEL_C32
vuelta_rm   dw nuc.rm               ; salto lejano de vuelta a modo real
            dw 0                    ; (este segmento: lo pone la instalación)
bloque      dd 0                    ; dirección física del bloque
pila32      dd 0
gdtr        dw 5 * 8 - 1
            dd 0
idtr        dw 32 * 8 - 1
            dd 0
gdt_ant     dw 0
            dd 0
idt_ant     dw 0
            dd 0
        align 8
gdt         dq 0
            dq 0x00CF9A000000FFFF   ; código de 32 bits (la base, al instalar)
            dq 0x00CF92000000FFFF   ; datos de 32 bits
            dq 0x00009A000000FFFF   ; código de 16 bits
            dq 0x000092000000FFFF   ; datos de 16 bits

; =============================================================================
;  Llamar al núcleo: EAX = función, EDX = dirección lineal de la petición
;  -> EAX = resultado (-97 si ya se estaba dentro). DS = CS. Conserva lo demás.
; =============================================================================
nuc:
        pushf
        cli
        cmp byte [en_nucleo], 0
        je .libre
        mov eax, -97
        popf
        ret
.libre:
        mov byte [en_nucleo], 1
        push ebx
        push ecx
        push edx
        push esi
        push edi
        push ebp
        push ds
        push es
        push fs
        push gs
        ; la línea A20, abierta (por el puerto 92h, sin tocar el bit del reinicio)
        push ax
        in al, 0x92
        mov [a20_puesta], al
        test al, 2
        jnz .a20
        or al, 2
        and al, 0xFE
        out 0x92, al
.a20:
        pop ax
        mov [rm_ss], ss
        mov [rm_sp], sp
        sgdt [gdt_ant]
        sidt [idt_ant]
        lgdt [gdtr]
        lidt [idtr]
        mov ebx, [bloque]
        mov ecx, cr0
        or cl, 1
        mov cr0, ecx
        jmp SEL_C16:.pm
.pm:
        mov cx, SEL_D32
        mov ds, cx
        mov es, cx
        mov fs, cx
        mov gs, cx
        mov ss, cx
        mov esp, [cs:pila32]
        o32 call far [cs:entrada32]
        ; de vuelta (código de 16 bits, modo protegido): EAX = resultado
        mov cx, SEL_D16
        mov ds, cx
        mov es, cx
        mov fs, cx
        mov gs, cx
        mov ss, cx
        mov ecx, cr0
        and cl, 0xFE
        mov cr0, ecx
        jmp far [cs:vuelta_rm]
.rm:
        mov cx, cs
        mov ds, cx
        lss sp, [rm_sp]
        movzx esp, sp
        lidt [idt_ant]
        lgdt [gdt_ant]
        test byte [a20_puesta], 2
        jnz .a20b
        push ax
        mov al, [a20_puesta]
        and al, 0xFE
        out 0x92, al
        pop ax
.a20b:
        pop gs
        pop fs
        pop es
        pop ds
        pop ebp
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        mov byte [en_nucleo], 0
        popf
        ret

; =============================================================================
;  Mirar la tarjeta y repartir lo recibido. DS = CS, en la pila propia, con
;  las interrupciones cerradas.
; =============================================================================
sondea:
        mov eax, LL_SONDEA
        xor edx, edx
        call nuc
        test eax, eax
        js .r
        test al, 1
        jz .r
        mov byte [quedan], 8        ; como mucho 8 tramas por pasada
.mas:
        xor edx, edx
        mov dx, cs
        shl edx, 4
        lea eax, [edx + rx_trama]
        mov [pet], eax
        add edx, pet
        mov eax, LL_RECIBE
        call nuc
        cmp eax, 14
        jl .r
        mov cx, ax
        push cx
        push cs
        pop es
        mov si, rx_trama
        mov di, rx_tmp
        mov cx, 7
        cld
        rep movsw
        pop cx
        call rx_find
        jc .sig
        mov cx, [rx_len]
        mov si, rx_trama
        rep movsb
        call rx_done
.sig:
        dec byte [quedan]
        jnz .mas
.r:
        ret

; desde una interrupción: cambia a la pila propia y sondea (si no se está ya
; dentro). No toca ningún registro.
sondeo_int:
        cmp byte [cs:en_sondeo], 0
        jne .no
        cmp byte [cs:en_nucleo], 0
        jne .no
        mov byte [cs:en_sondeo], 1
        push ds
        push cs
        pop ds
        mov [sv_ss], ss
        mov [sv_sp], sp
        push cs
        pop ss
        mov sp, pila_top
        pushad
        push es
        push fs
        push gs
        cld
        call sondea
        pop gs
        pop fs
        pop es
        popad
        lss sp, [sv_sp]
        pop ds
        mov byte [cs:en_sondeo], 0
.no:
        ret

int08:
        pushf
        call far [cs:old08]
        cli
        call sondeo_int
        iret

int16:
        cmp ah, 0x01
        je .p
        cmp ah, 0x11
        je .p
        jmp far [cs:old16]
.p:
        pushf
        cli
        call sondeo_int
        popf
        jmp far [cs:old16]

; =============================================================================
;  Funciones del packet driver (como en HSPRED)
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
        push eax
        push edx
        xor eax, eax
        mov ax, [bp-2]
        shl eax, 4
        movzx edx, si
        add eax, edx
        pushf
        cli
        mov [pet_fuera], eax        ; (pet la usa el sondeo: aquí, otra)
        movzx eax, cx
        mov [pet_fuera+4], eax
        xor edx, edx
        mov dx, cs
        shl edx, 4
        add edx, pet_fuera
        mov eax, LL_ENVIA
        call nuc
        popf
        test eax, eax
        pop edx
        pop eax
        jnz .e
        inc dword [st_pout]
        push edx
        movzx edx, cx
        add [st_bout], edx
        pop edx
        xor dh, dh
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

; ---- entregar un paquete (como en HSPRED) ----
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

; =============================================================================
;  Descargar. BL = 1: aunque haya programas usándolo.
;  -> AL = 0 hecho, 1 otro programa tiene los vectores, 2 en uso
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
        cmp word [es:0x2F*4], int2f
        jne .notours
        cmp [es:0x2F*4+2], dx
        jne .notours
        cmp word [es:0x08*4], int08
        jne .notours
        cmp [es:0x08*4+2], dx
        jne .notours
        cmp word [es:0x16*4], int16
        jne .notours
        cmp [es:0x16*4+2], dx
        jne .notours
        ; apagar la radio
        mov eax, LL_PARA
        xor edx, edx
        call nuc
        cli
        movzx bx, byte [pkt_vec]
        shl bx, 2
        mov eax, [oldpkt]
        mov [es:bx], eax
        mov eax, [old2f]
        mov [es:0x2F*4], eax
        mov eax, [old08]
        mov [es:0x08*4], eax
        mov eax, [old16]
        mov [es:0x16*4], eax
        sti
        ; soltar el bloque de memoria extendida
        mov dx, [xms_h]
        mov ah, 0x0D                ; desbloquear
        call far [xms]
        mov dx, [xms_h]
        mov ah, 0x0A                ; liberar
        call far [xms]
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
;  INT 2Fh: AH=DAh (como HSPRED) y AH=DCh (el Wi-Fi)
; =============================================================================
int2f:
        cmp ah, 0xDA
        je .red
        cmp ah, 0xDC
        je .wifi
.otro:
        jmp far [cs:old2f]
.red:
        ; con /JUNTO la interfaz de HSPRED es de HSPRED (el de la red por cable)
        test byte [cs:opciones], 8
        jnz .otro
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
        call enlace
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
.wifi:
        cmp al, 0
        jne .w1
        mov al, 0xFF
        mov bx, 0x5746
        mov cx, VERSION
        iret
.w1:
        cmp al, 1
        jne .w2
        ; llamar al núcleo: CX = función (2 a 14), DS:SI = petición
        cmp cx, 2
        jb .wno
        cmp cx, 14
        ja .wno
        push ebx
        push ds
        xor ebx, ebx
        mov bx, ds
        shl ebx, 4
        movzx edx, si
        add edx, ebx
        movzx eax, cx
        push cs
        pop ds
        call nuc
        pop ds
        pop ebx
        mov edx, eax
        shr edx, 16
        iret
.wno:
        mov ax, 0xFFFF
        mov dx, ax
        iret
.w2:
        cmp al, 2
        jne .w3
        push cs
        pop es
        mov bx, datos_wifi
        iret
.w3:
        cmp al, 3
        jne .w4
        push ds
        push cs
        pop ds
        call unload
        pop ds
.w4:
        iret

; AL = enlace (1 conectado a una red, 0 no), AH = 0 (velocidad: no se dice)
enlace:
        push edx
        push ebx
        xor edx, edx
        mov dx, cs
        shl edx, 4
        lea eax, [edx + rx_trama]
        ; (el estado se pide en el búfer de recepción: se usa con las
        ; interrupciones cerradas y fuera del sondeo)
        pushf
        cli
        cmp byte [en_sondeo], 0
        jne .nose
        mov [pet_fuera], eax
        mov dword [pet_fuera+4], 64
        add edx, pet_fuera
        mov eax, LL_ESTADO
        call nuc
        mov al, [rx_trama]          ; el estado: 5 = conectado
        popf
        cmp al, 5
        sete al
        xor ah, ah
        pop ebx
        pop edx
        ret
.nose:
        popf
        mov ax, 0x00FF
        pop ebx
        pop edx
        ret

rx_trama    times 1514 db 0
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
.uso:
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
        mov ax, 0xDC00
        xor bx, bx
        int 0x2F
        cmp al, 0xFF
        jne .new
        cmp bx, 0x5746
        jne .new
        cmp byte [o_unload], 0
        jne .unl
        mov si, msg_already
        jmp fin_mal
.unl:
        mov ax, 0xDC03
        xor bl, bl
        int 0x2F
        mov si, msg_unloaded
        test al, al
        jz .ue
        mov si, msg_noun1
        cmp al, 1
        je fin_mal
        mov si, msg_noun2
        jmp fin_mal
.ue:
        call puts
        mov ax, 0x4C00
        int 0x21
.new:
        mov si, msg_notres
        cmp byte [o_unload], 0
        jne fin_mal
        ; otro controlador de red con la interfaz de HSPRED (con /JUNTO se
        ; convive con él: HSPRED se queda con INT 2Fh AH=DAh)
        test byte [opciones], 8
        jnz .sin_red
        mov ax, 0xDA00
        xor bx, bx
        int 0x2F
        mov si, msg_hspred
        cmp al, 0xFF
        je fin_mal
.sin_red:
        ; modo V86: no se puede pasar a modo protegido por nuestra cuenta
        smsw ax
        mov si, msg_v86
        test al, 1
        jnz fin_mal
        ; gestor XMS
        mov ax, 0x4300
        int 0x2F
        mov si, msg_noxms
        cmp al, 0x80
        jne fin_mal
        mov ax, 0x4310
        int 0x2F
        mov [xms], bx
        mov [xms+2], es
        push cs
        pop es
        ; el vector del packet driver
        call pick_vector
        mov si, msg_novec
        jc fin_mal
        ; la unidad de arranque, para las rutas
        mov ax, 0x3305
        int 0x21
        add dl, 'A' - 1
        mov [ruta_bin], dl
        mov [ruta_fw], dl
        mov [ruta_fwh], dl
        ; el núcleo: abrir y leer la cabecera
        mov dx, ruta_bin
        mov ax, 0x3D00
        int 0x21
        mov si, msg_nobin
        jc fin_mal_ruta
        mov [fh], ax
        mov bx, ax
        mov dx, tbuf
        mov cx, 32
        mov ah, 0x3F
        int 0x21
        jc .mal_bin
        cmp ax, 32
        jne .mal_bin
        cmp dword [tbuf+4], 0x31465748  ; «HWF1»
        je .cab
.mal_bin:
        mov si, msg_badbin
        jmp fin_mal
.cab:
        ; la memoria: el bloque entero
        mov ah, 0x09                ; pedir bloque
        mov dx, BLOQUE_KB + 64      ; (y 64 KB para alinear: ver abajo)
        call far [xms]
        cmp ax, 1
        mov si, msg_nomem
        jne fin_mal
        mov [xms_h], dx
        mov ah, 0x0C                ; bloquear -> DX:BX = dirección
        call far [xms]
        cmp ax, 1
        mov si, msg_nomem
        jne fin_mal_xms
        mov [bloque], bx
        mov [bloque+2], dx
        ; La base del núcleo se alinea a 64 KB dentro del bloque. Tiene que
        ; ser así: el núcleo ve la memoria desplazada (puntero = dirección
        ; física - base) y el controlador de la tarjeta alinea sus anillos de
        ; DMA (a 4 y a 8 KB) por el puntero; con una base sin alinear, la
        ; dirección física no quedaría alineada (y la tarjeta lo exige).
        mov eax, [bloque]
        neg eax
        and eax, 0xFFFF
        mov [relleno], eax
        add [bloque], eax
        mov ah, 0x03                ; A20 abierta (global)
        call far [xms]
        ; copiar el núcleo al bloque
        mov bx, [fh]
        xor cx, cx
        xor dx, dx
        mov ax, 0x4200
        int 0x21
        mov dword [x_des], 0
        mov dword [x_tam], 0
        call copia_archivo
        mov si, msg_badbin
        jc fin_mal_xms
        mov bx, [fh]
        mov ah, 0x3E
        int 0x21
        mov eax, [x_tam]
        cmp eax, [tbuf_cab+8]
        mov si, msg_badbin
        jne fin_mal_xms
        ; el microcódigo, detrás de los datos del núcleo (alineado a 4 KB)
        mov eax, [tbuf_cab+12]
        add eax, 4095
        and eax, ~4095
        mov [x_des], eax
        test byte [opciones], 1
        jnz .sin_fw
        mov [p_ini+8], eax          ; fw_des[0]
        mov dx, ruta_fw
        call carga_fw
        ; Sin el microcódigo de Broadcom se sigue: la Atheros (ath9k) no lo
        ; necesita; si la tarjeta es una Broadcom, el núcleo dirá que falta.
        jc .fw_falta
        mov eax, [x_tam]
        mov [p_ini+16], eax         ; fw_tam[0]
        mov eax, [x_des]
        add eax, 15
        and eax, ~15
        mov [x_des], eax
        mov [p_ini+12], eax         ; fw_des[1]
        mov dx, ruta_fwh
        call carga_fw
        mov si, msg_nofw
        jc fin_mal_xms_ruta
        mov eax, [x_tam]
        mov [p_ini+20], eax
        jmp .sin_fw
.fw_falta:
        xor eax, eax
        mov [p_ini+8], eax          ; fw_des[0..1] y fw_tam[0..1] a cero
        mov [p_ini+12], eax
        mov [p_ini+16], eax
        mov [p_ini+20], eax
.sin_fw:
        ; la tabla de segmentos
        mov eax, [bloque]
        mov [gdt+8+2], ax
        mov [gdt+16+2], ax
        shr eax, 16
        mov [gdt+8+4], al
        mov [gdt+16+4], al
        mov [gdt+8+7], ah
        mov [gdt+16+7], ah
        xor eax, eax
        mov ax, cs
        mov [vuelta_rm+2], ax
        shl eax, 4
        mov [gdt+24+2], ax
        mov [gdt+32+2], ax
        mov ebx, eax
        shr eax, 16
        mov [gdt+24+4], al
        mov [gdt+32+4], al
        lea eax, [ebx + gdt]
        mov [gdtr+2], eax
        mov eax, [tbuf_cab+16]
        mov [pila32], eax
        mov eax, [tbuf_cab+20]
        add eax, [bloque]
        mov [idtr+2], eax
        ; paso 1: preparar el núcleo y buscar la tarjeta (tras cada paso, el
        ; registro va a la pantalla y al archivo: si el siguiente se cuelga,
        ; queda escrito hasta dónde se llegó)
        mov eax, [bloque]
        mov [p_ini], eax
        mov dword [p_ini+4], BLOQUE_KB * 1024
        movzx eax, byte [opciones]
        mov [p_ini+24], eax
        xor edx, edx
        mov dx, cs
        shl edx, 4
        add edx, p_ini
        mov eax, LL_INICIA
        call nuc
        push eax
        call vuelca_registro
        pop eax
        test eax, eax
        jnz .falla
        ; paso 2: leer el bus interno de la tarjeta y montar el controlador
        mov eax, LL_MONTA
        xor edx, edx
        call nuc
        push eax
        call vuelca_registro
        pop eax
        test eax, eax
        jnz .falla
        ; paso 3: encender la radio
        mov eax, LL_ENCIENDE
        xor edx, edx
        call nuc
        push eax
        call vuelca_registro
        pop eax
        test eax, eax
        jnz .falla
        ; la MAC
        xor edx, edx
        mov dx, cs
        shl edx, 4
        lea eax, [edx + mac]
        mov [pet], eax
        add edx, pet
        mov eax, LL_MAC
        call nuc
        test byte [opciones], 1
        jz .nom
        mov si, n_sim
        mov di, card_name
.cn:
        lodsb
        stosb
        test al, al
        jnz .cn
.nom:
        ; «Tarjeta X, MAC ..., packet driver en INT 60h»
        mov si, msg_card
        call puts
        mov si, card_name
        call puts
        mov si, msg_mac
        call puts
        call put_mac
        mov si, msg_pkt
        call puts
        mov al, [pkt_vec]
        call hex2
        mov si, msg_h
        call puts
        mov si, msg_sigue
        call puts
        ; los vectores
        mov al, [pkt_vec]
        mov ah, 0x35
        int 0x21
        mov [oldpkt], bx
        mov [oldpkt+2], es
        mov ax, 0x352F
        int 0x21
        mov [old2f], bx
        mov [old2f+2], es
        mov ax, 0x3508
        int 0x21
        mov [old08], bx
        mov [old08+2], es
        mov ax, 0x3516
        int 0x21
        mov [old16], bx
        mov [old16+2], es
        push cs
        pop es
        mov al, [pkt_vec]
        mov ah, 0x25
        mov dx, pkt_entry
        int 0x21
        mov ax, 0x252F
        mov dx, int2f
        int 0x21
        mov ax, 0x2516
        mov dx, int16
        int 0x21
        mov ax, 0x2508
        mov dx, int08
        int 0x21
        ; soltar el entorno y quedarse
        mov es, [0x2C]
        mov ah, 0x49
        int 0x21
        mov dx, resident_end + 15
        shr dx, 4
        mov [res_paras], dx
        mov ax, 0x3100
        int 0x21
.falla:
        ; EAX = el código; el porqué ya está en el registro
        push eax
        mov si, msg_fallo
        call puts
        pop eax
        neg eax
        call putdec
        mov si, msg_fallo2
        call puts
        cmp byte [o_v], 0
        jne .yav
        cmp byte [reg_ruta], 0
        jne .yav
        mov si, msg_fallo3
        call puts
.yav:
        jmp fin_mal_xms_s

fin_mal_xms_ruta:
        call puts
        mov si, dx
        call puts
        mov si, msg_nl
fin_mal_xms:
        call puts
fin_mal_xms_s:
        mov dx, [xms_h]
        mov ah, 0x0D
        call far [xms]
        mov dx, [xms_h]
        mov ah, 0x0A
        call far [xms]
        mov ax, 0x4C01
        int 0x21
fin_mal_ruta:
        call puts
        mov si, dx
        call puts
        mov si, msg_nl
fin_mal:
        call puts
        mov ax, 0x4C01
        int 0x21

; ---- abrir DX y copiarlo al bloque en x_des (que avanza). CF si falla ----
carga_fw:
        mov ax, 0x3D00
        int 0x21
        jc .r
        mov [fh], ax
        mov dword [x_tam], 0
        call copia_archivo
        pushf
        mov bx, [fh]
        mov ah, 0x3E
        int 0x21
        popf
.r:
        ret

; el archivo [fh], desde donde esté hasta el final, a bloque + x_des
copia_archivo:
.l:
        mov bx, [fh]
        mov dx, tbuf
        mov cx, TBUF
        mov ah, 0x3F
        int 0x21
        jc .mal
        test ax, ax
        jz .fin
        ; las primeras palabras del núcleo (la cabecera), aparte
        cmp dword [x_des], 0
        jne .nc
        push ax
        mov si, tbuf
        mov di, tbuf_cab
        mov cx, 16
        rep movsw
        pop ax
.nc:
        movzx eax, ax
        add [x_tam], eax
        inc ax
        and ax, 0xFFFE              ; XMS mueve un número par de octetos
        mov [x_mov], eax
        mov word [x_mov+4], 0       ; origen: memoria convencional
        mov word [x_mov+6], tbuf
        mov [x_mov+8], cs
        mov dx, [xms_h]
        mov [x_mov+10], dx
        mov edx, [x_des]
        add edx, [relleno]          ; (dentro del bloque de XMS, tras el relleno)
        mov [x_mov+12], edx
        mov edx, [x_des]
        add edx, eax
        ; no pasarse del bloque
        cmp edx, BLOQUE_KB * 1024 - 0x80000
        ja .mal
        mov [x_des], edx
        mov si, x_mov
        mov ah, 0x0B
        call far [xms]
        cmp ax, 1
        jne .mal
        jmp .l
.fin:
        clc
        ret
.mal:
        stc
        ret

; ---- el registro del núcleo: a la pantalla (/V) y al archivo (/REG) ----
vuelca_registro:
        mov word [reg_fh], 0
        cmp byte [reg_ruta], 0
        je .l
        ; abrir para añadir; si no existe, crearlo
        mov dx, reg_ruta
        mov ax, 0x3D02
        int 0x21
        jnc .ab
        mov dx, reg_ruta
        xor cx, cx
        mov ah, 0x3C
        int 0x21
        jc .l
.ab:
        mov [reg_fh], ax
        mov bx, ax
        xor cx, cx
        xor dx, dx
        mov ax, 0x4202
        int 0x21
.l:
        xor edx, edx
        mov dx, cs
        shl edx, 4
        lea eax, [edx + tbuf]
        mov [pet], eax
        mov dword [pet+4], TBUF
        add edx, pet
        mov eax, LL_REGISTRO
        call nuc
        test eax, eax
        jle .fin
        mov cx, ax
        cmp byte [o_v], 0
        je .nov
        push cx
        mov dx, tbuf
        mov bx, 1
        mov ah, 0x40
        int 0x21
        pop cx
.nov:
        mov bx, [reg_fh]
        test bx, bx
        jz .l
        mov dx, tbuf
        mov ah, 0x40
        int 0x21
        jmp .l
.fin:
        mov bx, [reg_fh]
        test bx, bx
        jz .r
        mov ah, 0x3E
        int 0x21
.r:
        ret

; ---- opciones ----
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
        cmp al, 'D'
        je .u
        cmp al, 'V'
        je .v
        cmp al, 'P'
        je .p
        cmp al, 'S'
        je .s
        cmp al, 'R'
        je .r
        cmp al, 'I'
        je .i
        cmp al, 'J'
        je .j
        jmp .bad
.help:
        mov byte [o_help], 1
        jmp .skip
.u:
        mov byte [o_unload], 1
        jmp .skip
.v:
        mov byte [o_v], 1
        or byte [opciones], 2
        jmp .skip
.p:
        or byte [opciones], 4
        jmp .skip
.s:
        or byte [opciones], 1
        jmp .skip
.j:
        or byte [opciones], 8
        jmp .skip
.r:
        ; /REG:archivo
        call .hasta_dp
        jc .bad
        mov di, reg_ruta
        mov cx, 63
.rc:
        mov al, [si]
        cmp al, ' '
        jbe .rf
        cmp al, '/'
        je .rf
        call upcase
        stosb
        inc si
        loop .rc
.rf:
        mov byte [di], 0
        cmp byte [reg_ruta], 0
        je .bad
        jmp .l
.i:
        ; /INT:xx
        call .hasta_dp
        jc .bad
        call hexbyte
        jc .bad
        cmp al, 0x60
        jb .bad
        cmp al, 0x80
        ja .bad
        mov [o_int], al
        jmp .l
.skip:
        ; el resto de la palabra (/SIMULA, /PANTALLA...)
        mov al, [si]
        cmp al, ' '
        jbe .l
        cmp al, '/'
        je .l
        inc si
        jmp .skip
.hasta_dp:
        lodsb
        cmp al, ':'
        je .dp
        cmp al, '='
        je .dp
        cmp al, ' '
        ja .hasta_dp
        stc
        ret
.dp:
        clc
        ret

; dos cifras hexadecimales en DS:SI -> AL (SI avanza); CF si no lo son
hexbyte:
        push bx
        xor bx, bx
        mov cx, 2
.l:
        lodsb
        call upcase
        sub al, '0'
        jb .bad
        cmp al, 9
        jbe .d
        sub al, 7
        cmp al, 10
        jb .bad
        cmp al, 15
        ja .bad
.d:
        shl bl, 4
        or bl, al
        loop .l
        mov al, bl
        pop bx
        clc
        ret
.bad:
        pop bx
        stc
        ret

upcase:
        cmp al, 'a'
        jb .r
        cmp al, 'z'
        ja .r
        sub al, 32
.r:
        ret

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

hex2:
        push ax
        shr al, 4
        call .n
        pop ax
        and al, 15
.n:
        add al, '0'
        cmp al, '9'
        jbe .p
        add al, 7
.p:
        jmp putc

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

; EAX sin signo, en decimal
putdec:
        push eax
        push ebx
        push cx
        push edx
        mov ebx, 10
        xor cx, cx
.d:
        xor edx, edx
        div ebx
        push dx
        inc cx
        test eax, eax
        jnz .d
.p:
        pop ax
        add al, '0'
        call putc
        loop .p
        pop edx
        pop cx
        pop ebx
        pop eax
        ret

o_help      db 0
o_unload    db 0
o_v         db 0
o_int       db 0
fh          dw 0
reg_fh      dw 0
x_des       dd 0
x_tam       dd 0                    ; octetos de verdad del archivo copiado
relleno     dd 0                    ; del principio del bloque de XMS a la base alineada
x_mov       times 16 db 0           ; longitud, origen (0, puntero), destino (bloque, desplazamiento)
p_ini       times 7 dd 0            ; struct ll_inicio
tbuf_cab    times 32 db 0
ruta_bin    db "A:\HISPADOS\HSPWIFI.BIN", 0
ruta_fw     db "A:\HISPADOS\BCM43XX.FW", 0
ruta_fwh    db "A:\HISPADOS\BCM43XXH.FW", 0
n_sim       db "Wi-Fi simulado (pruebas)", 0

msg_title   db "HSPWIFI 0.1: controlador Wi-Fi de Hispa-486 (packet driver)", 13, 10, 0
msg_usage   db "Uso: HSPWIFI [/V] [/PANTALLA] [/REG:archivo] [/INT:xx]", 13, 10
            db "     HSPWIFI /U", 13, 10
            db "  /V          enseña el registro de la puesta en marcha", 13, 10
            db "  /PANTALLA   el registro, además, directo a la pantalla (por si se cuelga)", 13, 10
            db "  /REG:arch   guarda el registro en ese archivo (se añade al final)", 13, 10
            db "  /INT:xx     vector del packet driver (60 a 80; si no, el primero libre)", 13, 10
            db "  /JUNTO      convive con HSPRED (la red sigue yendo por el cable)", 13, 10
            db "  /U          lo descarga", 13, 10
            db "Tarjetas: Broadcom BCM43224 y BCM43225. Después, la orden WIFI elige la red.", 13, 10, 0
msg_already db "HSPWIFI ya está cargado.", 13, 10, 0
msg_unloaded db "HSPWIFI descargado.", 13, 10, 0
msg_noun1   db "No se puede descargar: otro programa residente se cargó después.", 13, 10, 0
msg_noun2   db "No se puede descargar: hay programas usando la red.", 13, 10, 0
msg_notres  db "HSPWIFI no está cargado.", 13, 10, 0
msg_hspred  db "Ya hay un controlador de red cargado (HSPRED). Descárguelo antes: HSPRED /U", 13, 10, 0
msg_v86     db "HSPWIFI no funciona con HSPEMM ni otro gestor de memoria expandida (modo V86).", 13, 10
            db "Quite HSPEMM.SYS de CONFIG.SYS para usar el Wi-Fi.", 13, 10, 0
msg_noxms   db "HSPWIFI necesita memoria extendida: cargue HSPXMS.SYS en CONFIG.SYS.", 13, 10, 0
msg_novec   db "No queda ningún vector libre para el packet driver (60h-80h).", 13, 10, 0
msg_nobin   db "Falta el archivo ", 0
msg_badbin  db "El archivo HSPWIFI.BIN no vale (dañado o de otra versión).", 13, 10, 0
msg_nomem   db "No hay 2,1 MB de memoria extendida libres para HSPWIFI.", 13, 10, 0
msg_nofw    db "Falta el microcódigo de la tarjeta: ", 0
msg_nl      db 13, 10, 0
msg_card    db "Tarjeta ", 0
msg_mac     db ", MAC ", 0
msg_pkt     db ", packet driver en INT ", 0
msg_h       db "h", 13, 10, 0
msg_sigue   db "Para elegir la red: WIFI REDES y WIFI CONECTAR nombre", 13, 10, 0
msg_fallo   db "El Wi-Fi no se ha podido poner en marcha (código ", 0
msg_fallo2  db ").", 13, 10, 0
msg_fallo3  db "Para ver por qué: HSPWIFI /V  (o /REG:C:\WIFI.LOG para guardarlo)", 13, 10, 0

TBUF        equ 4096
tbuf:
