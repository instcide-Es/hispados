; =============================================================================
;  HSPUSB.SYS - USB nativo de Hispa-DOS: controlador xHCI + memorias USB
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  CONFIG.SYS:  DEVICE=C:\HISPADOS\HSPUSB.SYS [/F] [/U] [/INFO] [/V]
;     (sin opciones) seguro: si la BIOS está usando el USB no se toca nada. Se
;         entiende que lo usa si el controlador es suyo (USB Legacy Support),
;         si lo tiene en marcha, o si el sistema ha arrancado desde un disco
;         USB de la BIOS. Tampoco se pasan al xHCI los puertos de un EHCI que
;         la BIOS esté usando (Intel).
;     /F  tomar el controlador aunque lo esté usando la BIOS (la BIOS dejará
;         de atender sus dispositivos USB: el teclado y el ratón pasan a
;         HSPUSB). No vale si el sistema ha arrancado desde un disco USB.
;     /U  como /F, también cuando el sistema ha arrancado desde una memoria
;         USB de la BIOS: HSPUSB la reconoce (por su sector 0) y desde ese
;         momento atiende él la unidad 80h de INT 13h, con la geometría que
;         daba la BIOS. Riesgo: si la memoria de arranque o el teclado no
;         aparecen después (otro controlador, un dispositivo que no se
;         admite), el equipo se queda sin ellos hasta reiniciar (F5 al
;         arrancar salta CONFIG.SYS).
;     /INFO  diagnóstico en pantalla: controladores USB del equipo, estado
;         del xHCI y árbol de dispositivos (puerto, velocidad, VID:PID,
;         clase/subclase/protocolo de cada interfaz y el uso que se le da);
;         espera 15 s para poder hacer una foto.
;     /V  mensajes detallados
;
;  Busca un controlador xHCI (USB 3, el de los PCs actuales), lo inicializa y
;  enumera los dispositivos de los puertos y los que cuelgan de concentradores
;  (hubs; hasta HUB_DEPTH encadenados, HUB_N a la vez, POOL_N dispositivos):
;   - la primera memoria USB (clase 08h, SCSI, Bulk-Only Transport), que se
;     ofrece a DOS como una unidad nueva (FAT12/FAT16, con o sin particiones);
;   - un teclado USB (HID, protocolo de arranque): las teclas se entregan como
;     si vinieran del teclado PS/2 (orden D2h del controlador 8042), así que
;     funcionan la BIOS, KEYB y todos los programas;
;   - un ratón USB (o un panel táctil que ofrezca una interfaz de ratón de
;     arranque, 03/01/02; en un dispositivo con varias interfaces se elige
;     esa): se entrega a HSPRATON (INT 33h AX=48D0h) o, si no está, como
;     paquetes PS/2 (orden D3h del 8042).
;  Los informes del teclado y del ratón se recogen con la interrupción del
;  reloj (INT 08h). Usa el modo real "irreal" para los registros del xHCI, así
;  que no funciona en modo V86 (EMM386, Windows en modo extendido).
;
;  Concentradores (USB 2.0 cap. 11, xHCI 4.5/6.2.2): descriptor de
;  concentrador, Configure Endpoint con Hub = 1, número de puertos y TT Think
;  Time (un solo traductor: MTT = 0), PORT_POWER, GET_STATUS, PORT_RESET y,
;  para cada dispositivo, un Slot Context con la cadena de ruta, el puerto
;  del controlador y, si es de velocidad baja o plena detrás de un
;  concentrador de alta velocidad, el slot y el puerto del traductor (TT).
;  Los de USB 3 se tratan según la norma (descriptor 2Ah, SET_HUB_DEPTH),
;  sin haberse podido probar: QEMU solo ofrece un concentrador de USB 1.1.
;
;  Lo que NO hace (haría falta para un panel táctil sin interfaz de ratón de
;  arranque): leer el descriptor de informe HID (HID 1.11, 6.2.2) para saber
;  dónde van X, Y y los botones en un informe que no sea el de arranque. Los
;  modos multitáctiles propios de cada fabricante no están publicados y no se
;  usan. Tampoco hay EHCI/UHCI/OHCI: solo xHCI.
; =============================================================================
        cpu 386                     ; (sin BSWAP)
        bits 16
        org 0

; desplazamientos en la zona de memoria alineada a 4 KB (mem_off)
IN_CTX      equ 0x0000
CMD_RING    equ 0x0900
EVT_RING    equ 0x0A00
BIN_RING    equ 0x0B00
BOUT_RING   equ 0x0C00
KINT_RING   equ 0x0D00
MINT_RING   equ 0x0E00
ERST        equ 0x0F00
DCBAA       equ 0x0F40                ; MAX_SLOTS + 1 entradas de 8 bytes
EP0_RINGS   equ 0x1000                ; un anillo de EP0 por dispositivo
CBW_BUF     equ 0x1800
CSW_BUF     equ 0x1840
DESC        equ 0x1880
KBD_BUF     equ 0x1980
MOU_BUF     equ 0x19C0
BOOT_SEC    equ 0x1A00                ; sector 0 del disco de arranque de la BIOS (/U)
DATA        equ 0x2000
OUT_CTXS    equ 0x3000                ; contextos de salida: 800h por dispositivo
MEM_FIXED   equ 0x3000
POOL_N      equ 8                     ; dispositivos a la vez (concentradores incluidos)
MEM_SIZE    equ MEM_FIXED + POOL_N * 0x800
MAX_SLOTS   equ POOL_N
HUB_N       equ 4                     ; concentradores a la vez
HUB_DEPTH   equ 3                     ; niveles de concentradores encadenados
REP_DELAY   equ 9                   ; repetición: 0,5 s y unas 9 por segundo
REP_RATE    equ 2
RING_TRBS   equ 16
CHUNK_SECS  equ 8                   ; 4 KB por transferencia

header:
        dw 0xFFFF, 0xFFFF
        dw 0x0802                   ; bloque, sectores de 32 bits
        dw strategy
        dw interrupt
        db 1, "HSPUSB "

req         dd 0
xbase       dd 0
op_base     dd 0
rt_base     dd 0
db_base     dd 0
ctx_sz      dw 32
mem_off     dw 0
slot        db 0
port        db 0
speed       db 0
dci_in      db 0
dci_out     db 0
            db 0
mps_in      dw 0
mps_out     dw 0
part_start  dd 0
tag         dd 0
; estado de los anillos: índice y bit de ciclo
cmd_idx     db 0
cmd_cyc     db 1
bin_idx     db 0
bin_cyc     db 1
bout_idx    db 0
bout_cyc    db 1
evt_idx     db 0
evt_cyc     db 1
evt         times 16 db 0
; dispositivo que se está enumerando (lo usa control)
cur_slot    db 0
            db 0
cur_ep0     dw EP0_RINGS
cur_st      dw pool_st
; teclado y ratón USB
busy        db 1
k_on        db 0
k_slot      db 0
k_dci       db 0
k_len       dw 8
kint_idx    db 0
kint_cyc    db 1
m_on        db 0
m_slot      db 0
m_dci       db 0
            db 0
m_len       dw 4
mint_idx    db 0
mint_cyc    db 1
oldrep      times 8 db 0
newrep      times 8 db 0
rep_key     db 0
rep_cnt     db 0
old08       dd 0
sv_gdtr     dw 0
            dd 0
gdtr        dw 15
            dd 0
gdt_tbl     dq 0
            dw 0xFFFF, 0
            db 0, 0x92, 0xCF, 0
bpb         times 25 db 0
bpbarr      dw bpb
media_byte  db 0xF8

strategy:
        mov [cs:req], bx
        mov [cs:req+2], es
        retf

interrupt:
        pushad
        push ds
        push es
        push fs                     ; el modo irreal lo cambia: es del programa
        mov byte [cs:busy], 1
        ; la GDT del modo irreal, antes de usarlo por primera vez
        xor eax, eax
        mov ax, cs
        shl eax, 4
        add eax, gdt_tbl
        mov [cs:gdtr+2], eax
        call unreal
        call a20_open
        lds bx, [cs:req]
        mov al, [bx+2]
        cmp al, 0
        je init
        cmp al, 1
        je .media
        cmp al, 2
        je .bpb
        cmp al, 4
        je .read
        cmp al, 8
        je .write
        cmp al, 9
        je .write
.done:
        mov word [bx+3], 0x0100
.out:
        call a20_restore
        mov byte [cs:busy], 0
        pop fs
        pop es
        pop ds
        popad
        retf
.media:
        call usb_media
        lds bx, [cs:req]
        mov [bx+0x0E], al           ; 1 la misma, FFh cambiada o quitada
        jmp .done
.bpb:
        cmp byte [cs:gone], 0
        je .bpb1
        call try_back
        lds bx, [cs:req]
        jc .nrdy
.bpb1:
        mov word [bx+0x12], bpb
        mov [bx+0x14], cs
        jmp .done
.nrdy:
        mov word [bx+3], 0x8102     ; unidad no preparada
        jmp .out
.read:
        mov byte [cs:rw_dir], 1
        jmp .rw
.write:
        mov byte [cs:rw_dir], 0
.rw:
        cmp byte [cs:gone], 0
        je .rw1
        call try_back
        lds bx, [cs:req]
        jnc .rw1
        mov word [bx+0x12], 0
        jmp .nrdy
.rw1:
        movzx eax, word [bx+0x14]
        cmp ax, 0xFFFF
        jne .s
        mov eax, [bx+0x1A]
.s:
%ifdef USBTRACE
        push eax
        push dx
        mov dx, 0x3F8
        mov al, 'W'
        cmp byte [cs:rw_dir], 0
        je .tw
        mov al, 'R'
.tw:
        out dx, al
        pop dx
        pop eax
        push eax
        push dx
        mov dx, 0x3F8
        call trace_num
        mov al, ' '
        out dx, al
        pop dx
        pop eax
%endif
        add eax, [cs:part_start]
        mov [cs:rw_lba], eax
        mov cx, [bx+0x12]
        mov [cs:rw_left], cx
        mov ax, [bx+0x0E]
        mov [cs:rw_off], ax
        mov ax, [bx+0x10]
        mov [cs:rw_seg], ax
        call rw_run
        lds bx, [cs:req]
        jc .err
        jmp .done
.err:
        mov ax, [bx+0x12]
        sub ax, [cs:rw_left]
        mov [bx+0x12], ax
        mov word [bx+3], 0x810C     ; error general
        jmp .out

; leer (rw_dir = 1) o escribir rw_left sectores desde rw_lba en rw_seg:rw_off
; -> CF si falla (rw_left = los que quedaban)
rw_run:
        ; normalizado ya antes del primer trozo (4 KB no caben si el
        ; desplazamiento pasa de F000h: la copia daría la vuelta al segmento)
        mov ax, [cs:rw_off]
        shr ax, 4
        add [cs:rw_seg], ax
        and word [cs:rw_off], 15
.chunk:
        mov cx, [cs:rw_left]
        jcxz .ok
        cmp cx, CHUNK_SECS
        jbe .c
        mov cx, CHUNK_SECS
.c:
        mov [cs:rw_n], cx
        cmp byte [cs:rw_dir], 0
        jne .rd
        ; escritura: DOS -> búfer -> USB
        call copy_from_dos
        mov al, 0x2A
        call scsi_rw
        jc .r
        jmp .adv
.rd:
        mov al, 0x28
        call scsi_rw
        jc .r
        call copy_to_dos
.adv:
        movzx eax, word [cs:rw_n]
        add [cs:rw_lba], eax
        sub [cs:rw_left], ax
        shl ax, 9
        add [cs:rw_off], ax
        ; normalizar el puntero de DOS
        mov ax, [cs:rw_off]
        shr ax, 4
        add [cs:rw_seg], ax
        and word [cs:rw_off], 15
        jmp .chunk
.ok:
        clc
.r:
        ret

; =============================================================================
;  INT 13h del disco de arranque (opción /U). Cuando el sistema ha arrancado
;  desde una memoria USB que servía la BIOS y HSPUSB le quita el controlador,
;  la BIOS ya no puede leerla: desde ese momento la unidad 80h la atiende
;  HSPUSB por el xHCI, con la misma geometría que daba la BIOS.
; =============================================================================
int13:
        cmp dl, 0x80
        je .ours
        jmp far [cs:old13]
.ours:
        push ds
        push es
        push fs
        pushad
        mov bp, sp                  ; [bp+28] EAX, +24 ECX, +20 EDX, +16 EBX,
                                    ; +4 ESI, +0 EDI, +34 ES, +36 DS, +42 flags
        sti
        cld
        mov al, [cs:busy]
        push ax
        mov byte [cs:busy], 1
        smsw ax
        test al, 1
        jz .rm
        mov ah, 0x80                ; modo V86: no se puede
        jmp .ret
.rm:
        call unreal
        call a20_open
        mov ah, [bp+29]
        call i13_do
        call a20_restore
.ret:
        mov [cs:i13_last], ah
        mov [bp+29], ah
        and byte [bp+42], 0xFE
        test ah, ah
        jz .nc
        cmp byte [cs:i13_keep], 0   ; (AH=15h y 41h devuelven un valor, no un error)
        jne .nc
        or byte [bp+42], 1
.nc:
        mov byte [cs:i13_keep], 0
        pop ax
        mov [cs:busy], al
        popad
        pop fs
        pop es
        pop ds
        iret

; AH = función -> AH = estado (los demás resultados, en la pila de int13)
i13_do:
        cmp ah, 0x01
        je .last
        cmp ah, 0x02
        je .chs
        cmp ah, 0x03
        je .chs
        cmp ah, 0x08
        je .geom
        cmp ah, 0x15
        je .type
        cmp ah, 0x41
        je .ext
        cmp ah, 0x42
        je .lba
        cmp ah, 0x43
        je .lba
        cmp ah, 0x48
        je .parm
        ; reinicio, verificación, posicionado...: nada que hacer
        cmp ah, 0x00
        je .ok
        cmp ah, 0x04
        je .ok
        cmp ah, 0x0C
        je .ok
        cmp ah, 0x0D
        je .ok
        cmp ah, 0x10
        je .ok
        cmp ah, 0x11
        je .ok
        cmp ah, 0x44
        je .ok
        cmp ah, 0x47
        je .ok
        mov ah, 0x01                ; función no admitida
        ret
.ok:
        xor ah, ah
        ret
.last:
        mov ah, [cs:i13_last]
        ret
.geom:
        mov ax, [cs:i13_cx]
        mov [bp+24], ax
        mov ax, [cs:i13_dx]
        mov [bp+20], ax
        mov byte [bp+16], 0
        mov word [bp+0], 0          ; ES:DI = 0 (sin tabla de parámetros)
        mov word [bp+34], 0
        mov byte [bp+28], 0
        xor ah, ah
        ret
.type:
        mov eax, [cs:i13_total]
        mov [bp+20], ax             ; CX:DX = sectores
        shr eax, 16
        mov [bp+24], ax
        mov byte [cs:i13_keep], 1
        mov ah, 3
        ret
.ext:
        cmp word [bp+16], 0x55AA
        jne .bad
        mov word [bp+16], 0xAA55
        mov word [bp+24], 1         ; acceso por paquetes
        mov byte [bp+28], 0
        mov byte [cs:i13_keep], 1
        mov ah, 0x21                ; versión 1.1
        ret
.bad:
        mov ah, 0x01
        ret
.parm:
        mov ds, [bp+36]
        mov si, [bp+4]
        cmp word [si], 26
        jb .bad
        mov word [si], 26
        mov word [si+2], 0
        xor eax, eax
        mov [si+4], eax
        mov [si+8], eax
        mov [si+12], eax
        mov [si+20], eax
        mov eax, [cs:i13_total]
        mov [si+16], eax
        mov word [si+24], 512
        xor ah, ah
        ret
.chs:
        ; (cilindro * cabezas + cabeza) * sectores + sector - 1
        mov cx, [bp+24]
        movzx eax, cl
        and al, 0xC0
        shl ax, 2
        mov al, ch                  ; cilindro
        movzx ebx, byte [cs:i13_dx+1]
        inc ebx                     ; cabezas
        mul ebx
        movzx ebx, byte [bp+21]     ; cabeza
        add eax, ebx
        movzx ebx, byte [cs:i13_cx]
        and bl, 0x3F                ; sectores por pista
        jz .bad
        mul ebx
        and ecx, 0x3F
        jz .bad
        dec ecx
        add eax, ecx
        mov [cs:rw_lba], eax
        movzx ax, byte [bp+28]
        mov [cs:rw_left], ax
        mov ax, [bp+16]
        mov [cs:rw_off], ax
        mov ax, [bp+34]
        mov [cs:rw_seg], ax
        mov al, 1
        cmp byte [bp+29], 0x02
        je .go
        xor al, al
        jmp .go
.lba:
        mov ds, [bp+36]
        mov si, [bp+4]
        cmp dword [si+12], 0
        jne .range
        mov eax, [si+8]
        mov [cs:rw_lba], eax
        mov ax, [si+2]
        mov [cs:rw_left], ax
        mov ax, [si+4]
        mov [cs:rw_off], ax
        mov ax, [si+6]
        mov [cs:rw_seg], ax
        mov al, 1
        cmp byte [bp+29], 0x42
        je .go
        xor al, al
.go:
        mov [cs:rw_dir], al
        movzx eax, word [cs:rw_left]
        add eax, [cs:rw_lba]
        jc .range
        cmp eax, [cs:i13_total]
        ja .range
        push bp
        call rw_run
        pop bp
        jc .fail
        xor ah, ah
        ret
.range:
        mov ah, 0x04                ; sector no encontrado
        ret
.fail:
        mov ah, 0x80                ; sin respuesta
        ret
old13       dd 0
i13_cx      dw 0
i13_dx      dw 0
i13_total   dd 0
i13_last    db 0
i13_keep    db 0

; ---- cambio de la memoria USB ----
; -> AL = 1 la misma, FFh cambiada o quitada (DS cambia)
usb_media:
        push cs
        pop ds
        sti
        cmp byte [dport], 0
        je .same
        cmp byte [gone], 0
        jne .look
        call dport_sc
        test al, 1                  ; conectada
        jz .lost
        test eax, 1 << 17           ; CSC: se ha desconectado y conectado
        jnz .lost
        ; detrás de un concentrador: el estado de su puerto
        cmp byte [d_hub], 0xFF
        je .same
        mov al, [d_hub]
        mov [hp_hub], al
        mov al, [d_hport]
        mov [hp_port], al
        call hub_status
        jc .lost
        test al, 1
        jz .lost
        test dl, 1                  ; C_PORT_CONNECTION
        jz .same
.lost:
        call drop_disk
.look:
        call rescan
        mov al, 0xFF
        ret
.same:
        mov al, 1
        ret

; sin memoria: ¿han conectado una? (DS cambia; CF si sigue sin haber)
try_back:
        push cs
        pop ds
        sti
        call rescan
        cmp byte [gone], 1
        cmc
        ret

; ESI = PORTSC del puerto de la memoria, EAX = su valor
dport_sc:
        movzx esi, byte [dport]
port_sc:
        dec esi
        shl esi, 4
        add esi, [op_base]
        add esi, 0x400
        call mrd
        ret

; borrar los bits de cambio del puerto (ESI = PORTSC)
clr_csc:
        call mrd
        and eax, 0x0E00C3E0
        or eax, (1 << 9) | 0x00FE0000
        call mwr
        ret

; liberar el slot AL (Disable Slot) y su entrada de la tabla de contextos
slot_free:
        push bx
        push di
        push ax
        xor eax, eax
        xor edx, edx
        xor ecx, ecx
        pop bp
        push bp
        and ebp, 0xFF
        shl ebp, 24
        or ebp, 10 << 10            ; Disable Slot
        call xhci_cmd
        pop ax
        movzx di, al
        shl di, 3
        mov bx, DCBAA
        call zone
        add di, bx
        mov dword [cs:di], 0
        pop di
        pop bx
        ret

; la memoria ya no está: liberar su slot
drop_disk:
        mov al, [slot]
        call slot_free
        movzx bx, byte [dpool]
        mov byte [pool_used+bx], 0
        mov byte [gone], 1
        mov byte [disk_found], 0
        ret

; buscar una memoria en los puertos que acaban de conectarse (CSC) y en los
; de los concentradores que avisan de un cambio
rescan:
        mov al, [opt_verbose]
        mov ah, [opt_info]
        push ax
        mov byte [opt_verbose], 0   ; sin mensajes: estamos dentro de DOS
        mov byte [opt_info], 0
        mov byte [only_disk], 1
        xor cl, cl
.pl:
        inc cl
        cmp cl, [maxports]
        ja .hubs
        cmp byte [gone], 0
        je .r
        movzx esi, cl
        call port_sc
        test eax, 1 << 17
        jz .pl
        push cx
        call clr_csc
        ; lo que colgaba de este puerto ya no está: sus concentradores
        call hubs_drop
        call mrd
        test al, 1
        jz .nx
        call port_enable
        jc .nx
        call root_dev
        call enum_one
        call disk_back
.nx:
        pop cx
        jmp .pl
.hubs:
        xor bx, bx
.hl:
        cmp byte [gone], 0
        je .r
        cmp byte [h_used+bx], 0
        je .hn
        push bx
        mov bh, 1                   ; solo los puertos con cambio
        call hub_scan
        call disk_back
        pop bx
.hn:
        inc bx
        cmp bx, HUB_N
        jb .hl
.r:
        mov byte [only_disk], 0
        pop ax
        mov [opt_verbose], al
        mov [opt_info], ah
        ret

; tras enumerar en rescan: si ha aparecido una memoria, prepararla
disk_back:
        cmp byte [disk_found], 0
        je .r
        cmp byte [gone], 0
        je .r
        call disk_setup
        jc .bad
        mov byte [gone], 0
        ret
.bad:
        call drop_disk
.r:
        ret

; dar de baja los concentradores que colgaban del puerto raíz CL
hubs_drop:
        push bx
        push cx
        xor bx, bx
.l:
        cmp byte [h_used+bx], 0
        je .n
        cmp cl, [h_root+bx]
        jne .n
        mov byte [h_used+bx], 0
        push bx
        push cx
        mov al, [h_slot+bx]
        movzx bx, byte [h_pool+bx]
        mov byte [pool_used+bx], 0
        call slot_free
        pop cx
        pop bx
.n:
        inc bx
        cmp bx, HUB_N
        jb .l
        pop cx
        pop bx
        ret

; el dispositivo que se va a enumerar cuelga de un puerto del controlador
root_dev:
        mov dword [route], 0
        mov byte [depth], 0
        mov word [tt_slot], 0
        mov byte [par_hub], 0xFF
        mov byte [par_port], 0
        ret

; =============================================================================
;  Concentradores (USB 2.0 cap. 11; USB 3 cap. 10)
; =============================================================================
; petición de control al dispositivo actual: EAX/EDX = los 8 bytes
ctl_req:
        mov [ctl_setup], eax
        mov [ctl_setup+4], edx
        call control
        ret

; dirigir las peticiones de control al concentrador [hp_hub]
hub_sel:
        push bx
        movzx bx, byte [hp_hub]
        mov al, [h_slot+bx]
        mov [cur_slot], al
        mov al, [h_pool+bx]
        call pool_sel
        pop bx
        ret

; reserva: AL = entrada -> cur_pool, cur_ep0, cur_st, cur_out
pool_sel:
        push bx
        mov [cur_pool], al
        movzx bx, al
        shl bx, 1
        add bx, pool_st
        mov [cur_st], bx
        movzx bx, al
        shl bx, 8
        add bx, EP0_RINGS
        mov [cur_ep0], bx
        movzx bx, al
        shl bx, 11
        add bx, OUT_CTXS
        mov [cur_out], bx
        pop bx
        ret

; GET_STATUS del puerto [hp_port] del concentrador [hp_hub]
; -> AX = wPortStatus, DX = wPortChange; CF si falla
hub_status:
        push bx
        call hub_sel
        mov eax, 0x000000A3
        movzx edx, byte [hp_port]
        or edx, 0x00040000
        call ctl_req
        jc .r
        mov bx, DESC
        call zone
        mov ax, [cs:bx]
        mov dx, [cs:bx+2]
        clc
.r:
        pop bx
        ret

; SET_FEATURE (AH = 3) o CLEAR_FEATURE (AH = 1) de la característica AL en
; el puerto [hp_port] del concentrador [hp_hub]
hub_feat:
        push bx
        push ax
        call hub_sel
        pop ax
        movzx edx, al
        shl edx, 16
        mov dh, ah
        mov dl, 0x23
        mov eax, edx
        movzx edx, byte [hp_port]
        call ctl_req
        pop bx
        ret

; recorrer los puertos del concentrador BL. BH = 0: todo lo conectado;
; BH = 1: solo los puertos que avisan de un cambio de conexión
hub_scan:
        xor cl, cl
.pl:
        inc cl
        movzx di, bl
        cmp byte [h_used+di], 0
        je .end
        cmp cl, [h_np+di]
        ja .end
        push bx
        push cx
        call hub_port
        pop cx
        pop bx
        jmp .pl
.end:
        ret

; puerto CL del concentrador BL (BH como en hub_scan): si hay un dispositivo
; se reinicia el puerto y se enumera
hub_port:
        mov [hp_hub], bl
        mov [hp_port], cl
        mov [hp_mode], bh
        call hub_status
        jc .r
        cmp byte [hp_mode], 0
        je .a
        test dl, 1
        jz .r
.a:
        test dl, 1
        jz .b
        push ax
        mov ax, 0x0110              ; borrar C_PORT_CONNECTION
        call hub_feat
        pop ax
.b:
        test al, 1
        jz .r
        cmp byte [hp_mode], 0
        je .c
        call reset_rest             ; recién conectado: que se asiente
        call reset_rest
.c:
        ; USB 3: el puerto se habilita solo
        movzx di, byte [hp_hub]
        cmp byte [h_speed+di], 4
        jb .rst
        test al, 2
        jnz .en
.rst:
        mov ax, 0x0304              ; PORT_RESET
        call hub_feat
        jc .r
        mov cx, 10
.rw:
        push cx
        call reset_rest
        call hub_status
        pop cx
        jc .r
        test dl, 0x10               ; C_PORT_RESET
        jnz .rd
        loop .rw
.rd:
        mov ax, 0x0114              ; borrar C_PORT_RESET
        call hub_feat
        call reset_rest
        call hub_status
        jc .r
        test al, 2                  ; habilitado
        jz .r
.en:
        ; velocidad: la del puerto (USB 2: bit 9 baja, bit 10 alta)
        movzx di, byte [hp_hub]
        mov bl, 4
        cmp byte [h_speed+di], 4
        jae .sp
        mov bl, 2
        test ah, 2
        jnz .sp
        mov bl, 3
        test ah, 4
        jnz .sp
        mov bl, 1
.sp:
        mov [speed], bl
        mov al, [h_root+di]
        mov [port], al
        ; cadena de ruta: 4 bits por nivel con el puerto de cada concentrador
        mov cl, [h_depth+di]
        mov al, cl
        inc al
        mov [depth], al
        shl cl, 2
        movzx eax, byte [hp_port]
        shl eax, cl
        mov bx, di
        shl bx, 2
        or eax, [h_route+bx]
        mov [route], eax
        ; traductor de transacciones: un dispositivo de velocidad baja o
        ; plena detrás de un concentrador de alta velocidad usa el de ese
        ; concentrador (y lo hereda lo que cuelgue de él más abajo)
        xor ax, ax
        cmp byte [speed], 3
        jae .tt
        mov al, [h_slot+di]
        mov ah, [hp_port]
        cmp byte [h_speed+di], 3
        je .tt
        mov al, [h_ttslot+di]
        mov ah, [h_ttport+di]
.tt:
        mov [tt_slot], al
        mov [tt_port], ah
        mov ax, di
        mov [par_hub], al
        mov al, [hp_port]
        mov [par_port], al
        call enum_one
.r:
        ret

hp_hub      db 0
hp_port     db 0
hp_mode     db 0
new_hub     db 0
hubf        db 0
hub_np      db 0
hub_ttt     db 0
hub_pwr     db 0
h_used      times HUB_N db 0
h_slot      times HUB_N db 0
h_pool      times HUB_N db 0
h_np        times HUB_N db 0
h_speed     times HUB_N db 0
h_root      times HUB_N db 0
h_depth     times HUB_N db 0
h_ttslot    times HUB_N db 0
h_ttport    times HUB_N db 0
h_route     times HUB_N dd 0
; el dispositivo que se enumera: dónde cuelga
route       dd 0
depth       db 0
par_hub     db 0xFF
par_port    db 0
cur_pool    db 0
tt_slot     db 0
tt_port     db 0
d_hub       db 0xFF
d_hport     db 0
; reserva de dispositivos: anillo de EP0 y contexto de salida de cada uno
pool_max    db POOL_N
pool_used   times POOL_N db 0
pool_st     times POOL_N db 0, 1

; dejar el puerto CL habilitado (reiniciarlo si es USB 2) -> port, speed
port_enable:
        mov [port], cl
        movzx esi, cl
        call port_sc
        test al, 1
        jz .no
        test al, 2
        jnz .en
        and eax, 0x0E00C3E0
        or eax, (1 << 9) | (1 << 4) ; PP | PR
        call mwr
        mov cx, 10
        call tmo_start
.pr:
        call mrd
        test eax, 1 << 21           ; PRC
        jnz .prd
        call tmo_check
        jnc .pr
.prd:
        call reset_rest
        call clr_csc
        call mrd
        test al, 2
        jz .no
.en:
        shr eax, 10
        and al, 0x0F
        mov [speed], al
        clc
        ret
.no:
        stc
        ret

; pausa tras reiniciar un puerto: el dispositivo tiene 10 ms para reponerse
; antes de la primera orden (aquí entre 55 y 110 ms)
reset_rest:
        push cx
        mov cx, 2
        call tmo_start
.l:
        call tmo_check
        jnc .l
        pop cx
        ret

dport       db 0
dpool       db 0
gone        db 0
only_disk   db 0

%ifdef USBTRACE
; EAX -> decimal por COM1 (DX = 3F8h)
trace_num:
        push ebx
        push ecx
        push edx
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
        mov dx, 0x3F8
        out dx, al
        loop .p
        pop edx
        pop ecx
        pop ebx
        ret
%endif
rw_dir      db 0
opt_verbose_r db 0
rw_n        dw 0
rw_left     dw 0
rw_off      dw 0
rw_seg      dw 0
rw_lba      dd 0

; copiar rw_n sectores entre el búfer DATA y la memoria de DOS
copy_to_dos:
        push ds
        push es
        mov cx, [cs:rw_n]
        shl cx, 8
        mov si, [cs:mem_off]
        add si, DATA
        push cs
        pop ds
        les di, [cs:rw_off]
        cld
        rep movsw
        pop es
        pop ds
        ret
copy_from_dos:
        push ds
        push es
        mov cx, [cs:rw_n]
        shl cx, 8
        mov di, [cs:mem_off]
        add di, DATA
        push cs
        pop es
        lds si, [cs:rw_off]
        cld
        rep movsw
        pop es
        pop ds
        ret

; =============================================================================
;  Utilidades: modo irreal, MMIO, direcciones físicas
; =============================================================================
unreal:
        push eax
        push bx
        smsw ax
        test al, 1
        jnz .r
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
        sti
.r:
        pop bx
        pop eax
        ret

; EAX con los octetos al revés (BSWAP es del 486)
bswap_eax:
        xchg al, ah
        rol eax, 16
        xchg al, ah
        ret


; ---- línea A20 ----
; Los registros del controlador están por encima de 1 MB, casi siempre en una
; dirección con el bit 20 a uno: con A20 cerrada (la deja así cualquier
; programa que la abra y la cierre por el XMS, y hay BIOS que arrancan con
; ella cerrada) se leería y se escribiría en otra dirección. Se abre mientras
; el driver trabaja y se deja como estaba. Conservan todos los registros y
; los indicadores.
a20_open:
        pushf
        push ax
        cli
        inc byte [cs:a20_depth]
        cmp byte [cs:a20_depth], 1
        jne .r
        call a20_test
        jnz .r
        mov byte [cs:a20_closed], 1
        in al, 0x92
        or al, 2
        and al, 0xFE
        out 0x92, al
        call a20_test
        jnz .r
        mov ax, 0x2401
        int 0x15
.r:
        pop ax
        popf
        ret
a20_restore:
        pushf
        push ax
        cli
        dec byte [cs:a20_depth]
        jnz .r
        cmp byte [cs:a20_closed], 0
        je .r
        mov byte [cs:a20_closed], 0
        in al, 0x92
        and al, 0xFC
        out 0x92, al
        call a20_test
        jz .r
        mov ax, 0x2400
        int 0x15
.r:
        pop ax
        popf
        ret
; ZF = 1 si A20 está cerrada (0000:0500 y FFFF:0510 son el mismo octeto)
a20_test:
        push ds
        push es
        push ax
        xor ax, ax
        mov ds, ax
        dec ax
        mov es, ax
        mov al, [0x500]
        mov ah, al
        not al
        mov [0x500], al
        cmp al, [es:0x510]
        mov [0x500], ah
        jne .open
        cmp ah, [es:0x510]
        jne .open
        xor al, al                  ; ZF = 1
        jmp .x
.open:
        or al, 1                    ; ZF = 0
.x:
        pop ax
        pop es
        pop ds
        ret
a20_depth   db 0
a20_closed  db 0

; ESI = dirección absoluta
mrd:
        mov eax, [fs:esi]
        ret
mwr:
        mov [fs:esi], eax
        ret

; CS:BX -> EAX física
phys:
        push ebx
        movzx ebx, bx
        xor eax, eax
        mov ax, cs
        shl eax, 4
        add eax, ebx
        pop ebx
        ret

; BX = desplazamiento dentro de la zona -> BX = desplazamiento en CS
zone:
        add bx, [cs:mem_off]
        ret

; plazo: CX = ticks (18,2 por segundo)
tmo_start:
        push ds
        push ax
        xor ax, ax
        mov ds, ax
        mov ax, [0x46C]
        mov [cs:t0], ax
        mov [cs:tlen], cx
        pop ax
        pop ds
        ret
tmo_check:
        push ds
        push ax
        xor ax, ax
        mov ds, ax
        mov ax, [0x46C]
        sub ax, [cs:t0]
        cmp ax, [cs:tlen]
        pop ax
        pop ds
        cmc
        ret                         ; CF=1 si se agotó
t0          dw 0
tlen        dw 0

; =============================================================================
;  Anillos de TRB
; =============================================================================
; poner un TRB: BX = anillo (desplazamiento en la zona), SI = &índice,
; EAX/EDX/ECX = dwords 0-2, EBP = dword 3 (sin el bit de ciclo)
put_trb:
        push di
        push bx
        call zone
        movzx di, byte [cs:si]
        shl di, 4
        add di, bx
        mov [cs:di], eax
        mov [cs:di+4], edx
        mov [cs:di+8], ecx
        mov eax, ebp
        or al, [cs:si+1]            ; bit de ciclo
        mov [cs:di+12], eax
        inc byte [cs:si]
        cmp byte [cs:si], RING_TRBS - 1
        jb .r
        ; TRB de enlace al principio del anillo (cambia el ciclo)
        add di, 16
        call phys                   ; BX = inicio del anillo
        mov [cs:di], eax
        mov dword [cs:di+4], 0
        mov dword [cs:di+8], 0
        mov eax, (6 << 10) | 2      ; Link, Toggle Cycle
        or al, [cs:si+1]
        mov [cs:di+12], eax
        mov byte [cs:si], 0
        xor byte [cs:si+1], 1
.r:
        pop bx
        pop di
        ret

; tomar el siguiente evento del anillo -> evt, AL = tipo; CF si no hay
get_event:
        push bx
        push di
        push esi
        mov bx, EVT_RING
        call zone
        movzx di, byte [cs:evt_idx]
        shl di, 4
        add di, bx
        mov eax, [cs:di+12]
        and al, 1
        cmp al, [cs:evt_cyc]
        jne .none
        mov eax, [cs:di]
        mov [cs:evt], eax
        mov eax, [cs:di+4]
        mov [cs:evt+4], eax
        mov eax, [cs:di+8]
        mov [cs:evt+8], eax
        mov eax, [cs:di+12]
        mov [cs:evt+12], eax
        inc byte [cs:evt_idx]
        cmp byte [cs:evt_idx], RING_TRBS
        jb .nw
        mov byte [cs:evt_idx], 0
        xor byte [cs:evt_cyc], 1
.nw:
        ; ERDP = siguiente posición (y borrar EHB)
        movzx bx, byte [cs:evt_idx]
        shl bx, 4
        add bx, EVT_RING
        call zone
        call phys
        or al, 8
        mov esi, [cs:rt_base]
        add esi, 0x20 + 0x18
        call mwr
        add esi, 4
        xor eax, eax
        call mwr
        mov eax, [cs:evt+12]
        shr eax, 10
        and al, 0x3F
        pop esi
        pop di
        pop bx
        clc
        ret
.none:
        pop esi
        pop di
        pop bx
        stc
        ret

; esperar un evento del tipo AL -> evt; CF si vence el plazo. Los informes
; del teclado y del ratón que lleguen mientras tanto se atienden aquí.
wait_event:
        push ax
        push cx
        mov [cs:want], al
        mov cx, [cs:ev_tmo]
        call tmo_start
.l:
        call get_event
        jc .none
        cmp al, 32
        jne .chk
        call hid_event
        jnc .l
        mov al, 32
.chk:
        cmp al, [cs:want]
        je .ok
        jmp .l
.none:
        call tmo_check
        jnc .l
        pop cx
        pop ax
        stc
        ret
.ok:
        pop cx
        pop ax
        clc
        ret
want        db 0
ev_tmo      dw 40                   ; ~2 s (órdenes y control)

; timbre: AL = destino, AH = slot (0 = controlador)
doorbell:
        push esi
        push eax
        movzx esi, ah
        shl esi, 2
        add esi, [cs:db_base]
        movzx eax, al
        call mwr
        pop eax
        pop esi
        ret

; orden al controlador: EAX/EDX/ECX/EBP como put_trb -> CF si falla,
; AL = código de finalización, AH = slot
xhci_cmd:
        push bx
        push si
        mov bx, CMD_RING
        mov si, cmd_idx
        call put_trb
        xor ax, ax
        call doorbell
        mov al, 33                  ; Command Completion
        call wait_event
        jnc .got
        mov al, 0xFF
        jmp .r
.got:
        mov al, [cs:evt+11]         ; código
        mov ah, [cs:evt+15]         ; slot
        cmp al, 1
        je .r
        stc
.r:
        pop si
        pop bx
        ret

; esperar una transferencia -> CF si falla (éxito o paquete corto valen)
wait_xfer:
        mov byte [cs:xf_code], 0xFF
        mov al, 32
        call wait_event
        jc .r
        mov al, [cs:evt+11]
        mov [cs:xf_code], al
        cmp al, 1
        je .ok
        cmp al, 13
        je .ok
        stc
        ret
.ok:
        clc
.r:
        ret

; transferencia de control: setup en ctl_setup (8 bytes), datos en DESC
; (entrada, ctl_len bytes)
control:
        push bx
        push si
        mov eax, [cs:ctl_setup]
        mov edx, [cs:ctl_setup+4]
        mov ecx, 8
        mov ebp, (2 << 10) | (1 << 6)       ; Setup, datos inmediatos
        cmp word [cs:ctl_setup+6], 0
        je .nodata
        or ebp, 3 << 16                     ; TRT = IN
.nodata:
        mov bx, [cs:cur_ep0]
        mov si, [cs:cur_st]
        call put_trb
        cmp word [cs:ctl_setup+6], 0
        je .status
        mov bx, DESC
        call zone
        call phys
        xor edx, edx
        movzx ecx, word [cs:ctl_setup+6]
        mov ebp, (3 << 10) | (1 << 16)      ; Data, IN
        mov bx, [cs:cur_ep0]
        call put_trb
.status:
        xor eax, eax
        xor edx, edx
        xor ecx, ecx
        mov ebp, (4 << 10) | (1 << 5)       ; Status, IOC
        cmp word [cs:ctl_setup+6], 0
        jne .st
        or ebp, 1 << 16                     ; sin datos: estado IN
.st:
        mov bx, [cs:cur_ep0]
        mov si, [cs:cur_st]
        call put_trb
        mov al, 1
        mov ah, [cs:cur_slot]
        call doorbell
        call wait_xfer
        jnc .r
        cmp byte [cs:xf_code], 0xFF
        je .to
        call ep0_recover
.to:
        stc
.r:
        pop si
        pop bx
        ret
ctl_setup   dd 0, 0
xf_code     db 0

; Una petición de control rechazada (STALL) o con error deja parado el EP0
; en el controlador: hay que reanudarlo (Reset Endpoint) y saltarse lo que
; quedara de la transferencia (Set TR Dequeue Pointer), o las peticiones
; siguientes a ese dispositivo no saldrían nunca.
ep0_recover:
        pushad
        xor eax, eax
        xor edx, edx
        xor ecx, ecx
        movzx ebp, byte [cs:cur_slot]
        shl ebp, 24
        or ebp, (14 << 10) | (1 << 16)      ; Reset Endpoint, EP0
        call xhci_cmd
        mov si, [cs:cur_st]
        movzx bx, byte [cs:si]
        shl bx, 4
        add bx, [cs:cur_ep0]
        call zone
        call phys
        or al, [cs:si+1]                    ; estado del ciclo
        xor edx, edx
        xor ecx, ecx
        movzx ebp, byte [cs:cur_slot]
        shl ebp, 24
        or ebp, (16 << 10) | (1 << 16)      ; Set TR Dequeue Pointer, EP0
        call xhci_cmd
        popad
        ret

; transferencia bulk: AL = 1 entrada / 0 salida, EBX = dirección física,
; CX = bytes
bulk:
        push si
        push di
        mov edi, ebx
        mov [cs:bk_dir], al
        mov eax, edi
        xor edx, edx
        movzx ecx, cx
        mov ebp, (1 << 10) | (1 << 5) | (1 << 2)    ; Normal, IOC, ISP
        cmp byte [cs:bk_dir], 0
        je .out
        mov bx, BIN_RING
        mov si, bin_idx
        call put_trb
        mov al, [cs:dci_in]
        jmp .ring
.out:
        mov bx, BOUT_RING
        mov si, bout_idx
        call put_trb
        mov al, [cs:dci_out]
.ring:
        mov ah, [cs:slot]
        call doorbell
        ; un disco USB de verdad puede tardar segundos (arranque del motor,
        ; borrado de bloques de una memoria llena): 5 s, no 2
        mov word [cs:ev_tmo], 91
        call wait_xfer
        mov word [cs:ev_tmo], 40
        pop di
        pop si
        ret
bk_dir      db 0

; =============================================================================
;  Almacenamiento masivo (Bulk-Only Transport + SCSI)
; =============================================================================
; orden SCSI en scsi_cb (scsi_cblen bytes), datos: EBX física, ECX bytes,
; AL = 1 entrada -> CF si falla
scsi:
        push bx
        push di
        mov [cs:sc_dir], al
        mov [cs:sc_len], ecx
        mov [cs:sc_buf], ebx
        mov bx, CBW_BUF
        call zone
        mov di, bx
        mov dword [cs:di], 0x43425355       ; "USBC"
        inc dword [cs:tag]
        mov eax, [cs:tag]
        mov [cs:di+4], eax
        mov eax, [cs:sc_len]
        mov [cs:di+8], eax
        mov al, 0
        cmp byte [cs:sc_dir], 0
        je .f
        mov al, 0x80
.f:
        mov [cs:di+12], al
        mov byte [cs:di+13], 0              ; LUN 0
        mov al, [cs:scsi_cblen]
        mov [cs:di+14], al
        push si
        mov si, scsi_cb
        mov cx, 16
.cb:
        mov al, [cs:si]
        mov [cs:di+15], al
        inc si
        inc di
        loop .cb
        pop si
        mov bx, CBW_BUF
        call zone
        call phys
        mov ebx, eax
        mov cx, 31
        xor al, al
        call bulk
        jc .r
        mov ecx, [cs:sc_len]
        jecxz .csw
        mov ebx, [cs:sc_buf]
        mov al, [cs:sc_dir]
        call bulk
        jc .r
.csw:
        mov bx, CSW_BUF
        call zone
        mov di, bx
        call phys
        mov ebx, eax
        mov cx, 13
        mov al, 1
        call bulk
        jc .r
        cmp dword [cs:di], 0x53425355       ; "USBS"
        jne .bad
        cmp byte [cs:di+12], 0
        jne .bad
        clc
        jmp .r
.bad:
        stc
.r:
        pop di
        pop bx
        ret
sc_dir      db 0
sc_len      dd 0
sc_buf      dd 0
scsi_cblen  db 0
scsi_cb     times 16 db 0

; READ(10) (AL=28h) o WRITE(10) (AL=2Ah) de rw_n sectores en rw_lba con DATA
scsi_rw:
        push bx
        mov [cs:scsi_cb], al
        mov byte [cs:scsi_cblen], 10
        mov byte [cs:scsi_cb+1], 0
        mov eax, [cs:rw_lba]
        call bswap_eax
        mov [cs:scsi_cb+2], eax
        mov byte [cs:scsi_cb+6], 0
        mov ax, [cs:rw_n]
        xchg al, ah
        mov [cs:scsi_cb+7], ax
        mov byte [cs:scsi_cb+9], 0
        mov bx, DATA
        call zone
        call phys
        mov ebx, eax
        movzx ecx, word [cs:rw_n]
        shl ecx, 9
        mov al, 1
        cmp byte [cs:scsi_cb], 0x28
        je .d
        xor al, al
.d:
        call scsi
        pop bx
        ret

; =============================================================================
;  Teclado y ratón USB (HID, protocolo de arranque)
; =============================================================================
; evento de transferencia en evt: si es del teclado o del ratón se atiende y
; se vuelve a pedir el siguiente informe; CF si no es suyo
hid_event:
        push ax
        mov al, [cs:evt+14]
        and al, 0x1F                ; endpoint
        mov ah, [cs:evt+15]         ; slot
        cmp byte [cs:k_on], 0
        je .m
        cmp ah, [cs:k_slot]
        jne .m
        cmp al, [cs:k_dci]
        jne .m
        call ev_ok
        jc .kq
        call kbd_report
.kq:
        call kbd_queue
        pop ax
        clc
        ret
.m:
        cmp byte [cs:m_on], 0
        je .no
        cmp ah, [cs:m_slot]
        jne .no
        cmp al, [cs:m_dci]
        jne .no
        call ev_ok
        jc .mq
        call mouse_report
.mq:
        call mou_queue
        pop ax
        clc
        ret
.no:
        pop ax
        stc
        ret
; ¿transferencia correcta? (éxito o paquete corto)
ev_ok:
        cmp byte [cs:evt+11], 1
        je .y
        cmp byte [cs:evt+11], 13
        je .y
        stc
        ret
.y:
        clc
        ret

; pedir el siguiente informe
kbd_queue:
        pushad
        mov bx, KBD_BUF
        call zone
        call phys
        xor edx, edx
        movzx ecx, word [cs:k_len]
        mov ebp, (1 << 10) | (1 << 5) | (1 << 2)    ; Normal, IOC, ISP
        mov bx, KINT_RING
        mov si, kint_idx
        call put_trb
        mov al, [cs:k_dci]
        mov ah, [cs:k_slot]
        call doorbell
        popad
        ret
mou_queue:
        pushad
        mov bx, MOU_BUF
        call zone
        call phys
        xor edx, edx
        movzx ecx, word [cs:m_len]
        mov ebp, (1 << 10) | (1 << 5) | (1 << 2)
        mov bx, MINT_RING
        mov si, mint_idx
        call put_trb
        mov al, [cs:m_dci]
        mov ah, [cs:m_slot]
        call doorbell
        popad
        ret

; ---- teclado: comparar el informe con el anterior ----
kbd_report:
        pushad
        mov bx, KBD_BUF
        call zone
        xor di, di
.cp:
        mov al, [cs:bx+di]
        mov [cs:newrep+di], al
        inc di
        cmp di, 8
        jb .cp
        ; "demasiadas teclas" (01h): se ignora el informe
        mov di, 2
.ro:
        cmp byte [cs:newrep+di], 1
        je .r
        inc di
        cmp di, 8
        jb .ro
        ; modificadores
        mov dl, [cs:newrep]
        mov dh, [cs:oldrep]
        xor bx, bx
.mo:
        mov al, dl
        xor al, dh
        bt ax, bx
        jnc .mn
        mov al, [cs:modtab+bx]
        mov ah, 0x80                ; soltada
        bt dx, bx
        jnc .mb
        xor ah, ah                  ; pulsada
.mb:
        call send_key
.mn:
        inc bx
        cmp bx, 8
        jb .mo
        ; teclas soltadas
        mov di, 2
.rl:
        mov al, [cs:oldrep+di]
        cmp al, 4
        jb .rn
        mov si, newrep
        call in_rep
        jnc .rn
        cmp al, [cs:rep_key]
        jne .rk
        mov byte [cs:rep_key], 0
.rk:
        call usage_code
        mov ah, 0x80
        call send_key
.rn:
        inc di
        cmp di, 8
        jb .rl
        ; teclas pulsadas
        mov di, 2
.pl:
        mov al, [cs:newrep+di]
        cmp al, 4
        jb .pn
        mov si, oldrep
        call in_rep
        jnc .pn
        mov [cs:rep_key], al
        mov byte [cs:rep_cnt], REP_DELAY
        call usage_code
        xor ah, ah
        call send_key
.pn:
        inc di
        cmp di, 8
        jb .pl
        ; guardar
        xor di, di
.sv:
        mov al, [cs:newrep+di]
        mov [cs:oldrep+di], al
        inc di
        cmp di, 8
        jb .sv
.r:
        popad
        ret

; ¿está la tecla AL en las posiciones 2-7 del informe CS:SI? CF si NO está
in_rep:
        push bx
        mov bx, 2
.l:
        cmp al, [cs:si+bx]
        je .y
        inc bx
        cmp bx, 8
        jb .l
        pop bx
        stc
        ret
.y:
        pop bx
        clc
        ret

; uso HID AL -> AL = código de tecla (bit 7 = prefijo E0h; 0 = ninguno)
usage_code:
        cmp al, 0x65
        ja .no
        sub al, 4
        jb .no
        push bx
        movzx bx, al
        mov al, [cs:keytab+bx]
        pop bx
        ret
.no:
        xor al, al
        ret

; enviar la tecla AL (bit 7 = E0h) pulsada (AH = 0) o soltada (AH = 80h)
send_key:
        test al, al
        jz .r
        cmp byte [cs:kbc_ok], 0
        je soft_key
        push ax
        test al, 0x80
        jz .n
        mov al, 0xE0
        call kbc_put
.n:
        pop ax
        push ax
        and al, 0x7F
        or al, ah
        call kbc_put
        pop ax
.r:
        ret

; Sin controlador de teclado (8042) -los Mac, por ejemplo- no hay por dónde
; hacer llegar el código a la BIOS (INT 09h lee el puerto 60h). La tecla se
; resuelve aquí: primero se le ofrece a KEYB (INT 2Fh AX=AD81h, distribución
; española); si no la cambia, se traduce con la distribución estadounidense
; y se deja en el búfer de teclado de la BIOS (0040:001A), manteniendo los
; indicadores de Mayús, Ctrl, Alt y los bloqueos en 0040:0017.
; AL = código (bit 7 = prefijo E0h), AH = 0 pulsada / 80h soltada
soft_key:
        pushad
        push ds
        push es
        mov bl, al
        and bl, 0x7F
        mov bh, al
        and bh, 0x80
        mov dl, ah
        test bh, bh
        jz .k1
        push bx
        push dx
        mov bl, 0xE0
        mov ax, 0xAD81
        int 0x2F
        pop dx
        pop bx
.k1:
        push bx
        push dx
        or bl, dl
        mov ax, 0xAD81
        int 0x2F
        pop dx
        pop bx
        cmp ax, 0x4B01
        je .done
        mov ax, 0x40
        mov ds, ax
        ; Mayús, Ctrl y Alt
        xor al, al
        cmp bl, 0x36
        jne .m1
        mov al, 0x01
.m1:
        cmp bl, 0x2A
        jne .m2
        mov al, 0x02
.m2:
        cmp bl, 0x1D
        jne .m3
        mov al, 0x04
.m3:
        cmp bl, 0x38
        jne .m4
        mov al, 0x08
.m4:
        test al, al
        jz .nomod
        test dl, dl
        jnz .rel
        or [0x17], al
        jmp .done
.rel:
        not al
        and [0x17], al
        jmp .done
.nomod:
        ; bloqueos: cambian al pulsar (la repetición no cuenta)
        mov al, 0x40
        cmp bl, 0x3A
        je .tog
        mov al, 0x20
        cmp bl, 0x45
        je .tog
        mov al, 0x10
        cmp bl, 0x46
        je .tog
        test dl, dl
        jnz .done                   ; soltar las demás: nada
        jmp .key
.tog:
        test dl, dl
        jnz .trel
        test [0x18], al
        jnz .done
        or [0x18], al
        xor [0x17], al
        jmp .done
.trel:
        not al
        and [0x18], al
        jmp .done
.key:
        mov dh, [0x17]
        ; Ctrl+Alt+Supr: reinicio en caliente
        mov al, dh
        and al, 0x0C
        cmp al, 0x0C
        jne .nocad
        cmp bl, 0x53
        jne .nocad
        mov word [0x72], 0x1234
        jmp 0xFFFF:0x0000
.nocad:
        test bh, bh
        jz .plain
        ; teclas extendidas: Intro y / del teclado numérico, cursor
        mov cx, 0x1C0D
        cmp bl, 0x1C
        je .put
        mov cx, 0x352F
        cmp bl, 0x35
        je .put
        cmp bl, 0x47
        jb .done
        cmp bl, 0x53
        ja .done
        jmp .nav
.plain:
        cmp bl, 0x47
        jb .p1
        cmp bl, 0x53
        ja .p1
        ; teclado numérico: cifras con BloqNum (Mayús lo invierte)
        cmp bl, 0x4A
        je .kpc
        cmp bl, 0x4E
        je .kpc
        mov al, dh
        and al, 3
        jz .ns
        mov al, 0x20
.ns:
        xor al, dh
        test al, 0x20
        jz .nav
.kpc:
        movzx si, bl
        mov cl, [cs:kp_tab+si-0x47]
        mov ch, bl
        jmp .put
.nav:
        mov ch, bl
        xor cl, cl
        test dh, 4
        jz .put
        movzx si, bl
        mov ch, [cs:nav_ctrl+si-0x47]
        jmp .put
.p1:
        ; F1-F10 (con Mayús, Ctrl o Alt cambian de código)
        cmp bl, 0x3B
        jb .p2
        cmp bl, 0x44
        ja .p2
        mov ch, bl
        xor cl, cl
        test dh, 8
        jz .f1
        add ch, 0x68 - 0x3B
        jmp .put
.f1:
        test dh, 4
        jz .f2
        add ch, 0x5E - 0x3B
        jmp .put
.f2:
        test dh, 3
        jz .put
        add ch, 0x54 - 0x3B
        jmp .put
.p2:
        cmp bl, 0x57                ; F11 y F12
        jb .p3
        cmp bl, 0x58
        ja .p3
        mov ch, bl
        add ch, 0x85 - 0x57
        xor cl, cl
        test dh, 3
        jz .put
        add ch, 2
        jmp .put
.p3:
        cmp bl, 0x56                ; la tecla de más junto a Mayús
        jne .p4
        mov cx, 0x565C
        test dh, 3
        jz .put
        mov cl, '|'
        jmp .put
.p4:
        cmp bl, 0x39
        ja .done
        mov ch, bl
        movzx si, bl
        mov cl, [cs:asc_lo+si]
        mov al, [cs:asc_hi+si]
        test dh, 8
        jz .na
        ; Alt: sin carácter (las cifras, con otro código)
        xor cl, cl
        cmp bl, 0x02
        jb .put
        cmp bl, 0x0D
        ja .put
        add ch, 0x76
        jmp .put
.na:
        test dh, 4
        jz .nc
        ; Ctrl + letra: 01h-1Ah; unas pocas más
        cmp cl, 'a'
        jb .c1
        cmp cl, 'z'
        ja .c1
        and cl, 0x1F
        jmp .put
.c1:
        mov si, ctrl_keys
.c2:
        mov al, [cs:si]
        test al, al
        jz .done
        cmp al, bl
        je .c3
        add si, 2
        jmp .c2
.c3:
        mov cl, [cs:si+1]
        jmp .put
.nc:
        cmp cl, 'a'
        jb .sym
        cmp cl, 'z'
        ja .sym
        ; letras: mayúscula con Mayús o con Bloq Mayús (no con las dos)
        mov ah, dh
        and ah, 3
        jz .l1
        mov ah, 0x40
.l1:
        xor ah, dh
        test ah, 0x40
        jz .put
        mov cl, al
        jmp .put
.sym:
        test dh, 3
        jz .put
        mov cl, al
.put:
        mov ax, cx
        test ax, ax
        jz .done
        ; al búfer de teclado de la BIOS
        pushf
        cli
        mov si, [0x1C]
        mov di, si
        add di, 2
        mov ax, [0x82]
        test ax, ax
        jnz .b1
        mov ax, 0x3E
.b1:
        cmp di, ax
        jb .b2
        mov di, [0x80]
        test di, di
        jnz .b2
        mov di, 0x1E
.b2:
        cmp di, [0x1A]
        je .bfull                   ; lleno: la tecla se pierde
        mov [si], cx
        mov [0x1C], di
.bfull:
        popf
.done:
        pop es
        pop ds
        popad
        ret
; caracteres de los códigos 00h-39h, sin y con Mayús (teclado estadounidense)
asc_lo  db 0, 27, "1234567890-=", 8, 9, "qwertyuiop[]", 13, 0, "asdfghjkl;'`", 0
        db "\zxcvbnm,./", 0, "*", 0, " "
asc_hi  db 0, 27, "!@#$%^&*()_+", 8, 0, "QWERTYUIOP{}", 13, 0, 'ASDFGHJKL:"~', 0
        db "|ZXCVBNM<>?", 0, "*", 0, " "
kp_tab  db "789-456+1230."
nav_ctrl db 0x77, 0x8D, 0x84, 0x8E, 0x73, 0x8F, 0x74, 0x90, 0x75, 0x91, 0x76, 0x92, 0x93
ctrl_keys db 0x1A, 0x1B, 0x1B, 0x1D, 0x2B, 0x1C, 0x1C, 0x0A, 0x0E, 0x7F, 0x39, 0x20, 0x01, 0x1B, 0

; poner AL en el búfer de salida del 8042 como si viniera del teclado
; (orden D2h) o del ratón PS/2 (orden D3h)
kbc_put:
        push bx
        mov bl, 0xD2
        call kbc_out
        pop bx
        ret
kbc_aux:
        push bx
        mov bl, 0xD3
        call kbc_out
        pop bx
        ret
kbc_out:
        cmp byte [cs:kbc_ok], 0
        jne .go
        ret
.go:
        push cx
        push ax
        sti
        call kbc_drain              ; que se haya leído el anterior
        call kbc_ibf
        mov al, bl
        out 0x64, al
        call kbc_ibf
        pop ax
        push ax
        out 0x60, al
        call kbc_drain              ; IRQ 1 / 12: la BIOS lo lee
        pop ax
        pop cx
        ret
kbc_ok      db 1                    ; hay controlador de teclado (8042)
kbc_ibf:
        push ax
        mov cx, 0xFFFF
.l:
        in al, 0x64
        test al, 2
        jz .r
        loop .l
.r:
        pop ax
        ret
kbc_drain:
        push ax
        mov cx, 0xFFFF
.l:
        in al, 0x64
        test al, 1
        jz .r
        loop .l
.r:
        pop ax
        ret

; ---- ratón ----
mouse_report:
        pushad
        push es
        mov bx, MOU_BUF
        call zone
        mov si, bx
        movzx bx, byte [cs:si]      ; botones
        and bl, 7
        movsx cx, byte [cs:si+1]    ; x
        movsx dx, byte [cs:si+2]    ; y (positivo hacia abajo)
        ; ¿está HSPRATON?
        xor ax, ax
        mov es, ax
        cmp dword [es:0x33*4], 0
        je .ps2
        push bx
        push cx
        push dx
        mov ax, 0x48D2
        int 0x33
        pop dx
        pop cx
        pop bx
        cmp ax, 0x5253
        jne .ps2
        mov ax, 0x48D0
        int 0x33
        jmp .r
.ps2:
        ; paquete PS/2: botones | 08h | signos, x, y (positivo hacia arriba)
        neg dx
        mov al, bl
        or al, 8
        test cx, cx
        jns .sx
        or al, 0x10
.sx:
        test dx, dx
        jns .sy
        or al, 0x20
.sy:
        call kbc_aux
        mov al, cl
        call kbc_aux
        mov al, dl
        call kbc_aux
.r:
        pop es
        popad
        ret

; ---- INT 08h: recoger informes y repetir la tecla mantenida ----
int08:
        pushf
        call far [cs:old08]
        cmp byte [cs:busy], 0
        jne .ret
        mov byte [cs:busy], 1
        ; pila propia: lo que sigue (los informes, INT 33h con la rutina del
        ; programa, las interrupciones que entren mientras tanto) gasta
        ; bastante más de lo que se le puede pedir a la pila del programa
        ; interrumpido. busy impide que se entre dos veces.
        mov [cs:sv_ss], ss
        mov [cs:sv_sp], sp
        push cs
        pop ss
        mov sp, istack_top
        sti
        pushad
        push ds
        push es
        push fs
        smsw ax
        test al, 1
        jnz .out                    ; modo V86: no se puede
        sgdt [cs:sv_gdtr]
        call unreal
        call a20_open
.ev:
        call get_event
        jc .rep
        cmp al, 32
        jne .ev
        call hid_event
        jmp .ev
.rep:
        cmp byte [cs:rep_key], 0
        je .g
        dec byte [cs:rep_cnt]
        jnz .g
        mov byte [cs:rep_cnt], REP_RATE
        mov al, [cs:rep_key]
        call usage_code
        xor ah, ah
        call send_key
.g:
        call a20_restore
        cli
        lgdt [cs:sv_gdtr]
.out:
        pop fs
        pop es
        pop ds
        popad
        cli
        mov ss, [cs:sv_ss]
        mov sp, [cs:sv_sp]
        mov byte [cs:busy], 0
.ret:
        iret
sv_ss       dw 0
sv_sp       dw 0
            align 2
istack      times 1024 db 0
istack_top:

; códigos de tecla (juego 1 del PC) de los usos HID 04h-65h; bit 7 = E0h
keytab:
        db 0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23  ; a-h
        db 0x17, 0x24, 0x25, 0x26, 0x32, 0x31, 0x18, 0x19  ; i-p
        db 0x10, 0x13, 0x1F, 0x14, 0x16, 0x2F, 0x11, 0x2D  ; q-x
        db 0x15, 0x2C                                      ; y z
        db 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B  ; 1-0
        db 0x1C, 0x01, 0x0E, 0x0F, 0x39                    ; Intro Esc Retroceso Tab Espacio
        db 0x0C, 0x0D, 0x1A, 0x1B, 0x2B, 0x2B, 0x27, 0x28  ; - = [ ] \ # ; '
        db 0x29, 0x33, 0x34, 0x35, 0x3A                    ; ` , . / BloqMayús
        db 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x57, 0x58 ; F1-F12
        db 0xB7, 0x46, 0x00                                ; ImprPant BloqDespl Pausa
        db 0xD2, 0xC7, 0xC9, 0xD3, 0xCF, 0xD1              ; Insert Inicio RePág Supr Fin AvPág
        db 0xCD, 0xCB, 0xD0, 0xC8                          ; flechas: der izq abajo arriba
        db 0x45, 0xB5, 0x37, 0x4A, 0x4E, 0x9C              ; BloqNum / * - + Intro (numérico)
        db 0x4F, 0x50, 0x51, 0x4B, 0x4C, 0x4D, 0x47, 0x48, 0x49, 0x52, 0x53 ; 1-9 0 . (numérico)
        db 0x56, 0xDD                                      ; < > (teclado español), Aplicación
modtab  db 0x1D, 0x2A, 0x38, 0xDB, 0x9D, 0x36, 0xB8, 0xDC  ; Ctrl Mayús Alt Win (izq, der)

resident_end:

; =============================================================================
;  Inicialización (se descarta)
; =============================================================================
init:
        ; opciones /F y /V en la línea de CONFIG.SYS
        push ds
        push bx
        lds si, [bx+0x12]
.opt:
        lodsb
        cmp al, 13
        je .optd
        cmp al, 10
        je .optd
        test al, al
        jz .optd
        cmp al, '/'
        jne .opt
        lodsb
        and al, 0xDF
        cmp al, 'F'
        jne .nf
        mov byte [cs:opt_force], 1
.nf:
        cmp al, 'U'
        jne .nu
        mov byte [cs:opt_force], 1
        mov byte [cs:opt_boot], 1
.nu:
        cmp al, 'I'
        jne .ni
        mov byte [cs:opt_info], 1
.ni:
        cmp al, 'V'
        jne .opt
        mov byte [cs:opt_verbose], 1
        mov byte [cs:opt_verbose_r], 1
        jmp .opt
.optd:
        pop bx
        pop ds
        push cs
        pop ds
        mov si, msg_title
        call puts
        call pci_survey
        call xhci_setup
        jc .fail
        call usb_enum
        jc .fail
        xor cl, cl
.cc:
        inc cl
        cmp cl, [maxports]
        ja .ccd
        movzx esi, cl
        call port_sc
        call clr_csc
        jmp .cc
.ccd:
        mov byte [units], 0
        cmp byte [boot_saved], 0
        je .nb
        cmp byte [boot_match], 0
        jne .bootd
        mov si, msg_bootlost
        call puts
        jmp .hid
.bootd:
        ; la memoria de arranque: desde ahora su INT 13h es nuestro (no se
        ; ofrece además como unidad nueva: sería el mismo disco dos veces)
        mov ax, 0x3513
        int 0x21
        mov [old13], bx
        mov [old13+2], es
        mov dx, int13
        mov ax, 0x2513
        int 0x21
        mov byte [dport], 0
        mov si, msg_bootok
        call puts
        jmp .hid
.nb:
        cmp byte [disk_found], 0
        je .hid
        call disk_setup
        jnc .disk
        mov si, [fail_msg]
        call puts
        jmp .hid
.disk:
        mov byte [units], 1
        lds bx, [cs:req]
        mov al, [bx+0x16]
        add al, 'A'
        mov [cs:msg_drv], al
        push cs
        pop ds
        mov si, msg_disk
        call puts
        mov eax, [cap_mb]
        call putdec
        mov si, msg_mbas
        call puts
.hid:
        mov al, [k_on]
        or al, [m_on]
        jnz .hok
        mov al, [units]
        or al, [boot_match]
        jz .fail
        jmp .resp
.hok:
        cmp byte [k_on], 0
        je .h1
        mov si, msg_kbd
        call puts
        ; las teclas se entregan por el controlador de teclado (8042)
        in al, 0x64
        cmp al, 0xFF
        jne .k8042
        mov byte [kbc_ok], 0
        mov si, msg_no8042
        call puts
.k8042:
        call kbd_queue
.h1:
        cmp byte [m_on], 0
        je .h2
        mov si, msg_mouse
        call puts
        call mou_queue
.h2:
        ; reloj: recoger los informes
        mov ax, 0x3508
        int 0x21
        mov [old08], bx
        mov [old08+2], es
        mov dx, int08
        mov ax, 0x2508
        int 0x21
.resp:
        ; respuesta al núcleo: unidades, BPB y memoria residente
        ; memoria residente: los contextos de los dispositivos en uso y una
        ; entrada libre (dos si no hay memoria USB: ella y su concentrador)
        ; para lo que se conecte después
        xor bx, bx
        xor al, al
.pm:
        cmp byte [cs:pool_used+bx], 0
        je .pn
        mov al, bl
        inc al
.pn:
        inc bx
        cmp bx, POOL_N
        jb .pm
        inc al
        cmp byte [cs:disk_found], 0
        jne .pk
        inc al
.pk:
        cmp al, POOL_N
        jbe .pj
        mov al, POOL_N
.pj:
        mov [cs:pool_max], al
        movzx dx, al
        shl dx, 11
        add dx, MEM_FIXED
        add dx, [cs:mem_off]
        lds bx, [cs:req]
        mov al, [cs:units]
        mov [bx+0x0D], al
        mov [bx+0x0E], dx
        mov [bx+0x10], cs
        mov word [bx+0x12], bpbarr
        mov [bx+0x14], cs
        jmp .done
.fail:
        push cs
        pop ds
        mov si, [fail_msg]
        call puts
        ; el driver se descarga: el controlador no puede seguir en marcha
        ; con sus anillos y contextos en una memoria que ya no es nuestra
        call xhci_stop
        lds bx, [cs:req]
        mov byte [bx+0x0D], 0
        mov word [bx+0x0E], 0
        mov [bx+0x10], cs
.done:
        call info_pause
        lds bx, [cs:req]
        mov word [bx+3], 0x0100
        call a20_restore
        mov byte [cs:busy], 0
        pop fs
        pop es
        pop ds
        popad
        retf

; /INFO: dejar el informe en pantalla unos segundos (para la foto)
info_pause:
        cmp byte [cs:opt_info], 0
        jne .go
        ret
.go:
        push cs
        pop ds
        mov si, msg_pause
        call puts
        sti
        mov cx, 273                 ; 15 s
        call tmo_start
.l:
        mov ah, 1
        int 0x16
        jnz .key
        call tmo_check
        jnc .l
        jmp .r
.key:
        xor ah, ah
        int 0x16
.r:
        call unreal
        ret

; ---- qué controladores USB hay (y si la BIOS usa los EHCI) ----
pci_survey:
        mov ax, 0xB101
        xor edi, edi
        int 0x1A
        jc .r
        cmp edx, 0x20494350
        jne .r
        mov ecx, 0x0C0300
        call .count
        mov [n_hc], al
        mov ecx, 0x0C0310
        call .count
        mov [n_hc+1], al
        mov ecx, 0x0C0320
        call .count
        mov [n_hc+2], al
        mov ecx, 0x0C0330
        call .count
        mov [n_hc+3], al
        call unreal
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
        cmp byte [ehci_busy], 0
        je .nl
        mov si, msg_ehcibusy
        call puts
.nl:
        call crlf
.r:
        ret
; ECX = clase -> AL = cuántos hay
.count:
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
        cmp ecx, 0x0C0320
        jne .cn
        push ecx
        push si
        call ehci_one
        pop si
        pop ecx
.cn:
        inc si
        cmp si, 8
        jb .cl
.cd:
        mov ax, si
        ret

; EHCI en BX (bus/dispositivo/función): ¿lo tiene en marcha la BIOS, o es
; suyo (USBLEGSUP)? -> ehci_busy
ehci_one:
        mov [e_bdf], bx
        mov ax, 0xB109
        mov di, 0x04
        int 0x1A
        jc .r
        test cl, 2                  ; sin acceso a memoria: no está en uso
        jz .r
        mov bx, [e_bdf]
        mov ax, 0xB10A
        mov di, 0x10
        int 0x1A
        jc .r
        and ecx, 0xFFFFFFF0
        jz .r
        call unreal
        mov esi, ecx
        call mrd
        movzx ebx, al               ; CAPLENGTH
        push esi
        add esi, 8
        call mrd                    ; HCCPARAMS: EECP en los bits 15-8
        pop esi
        mov [e_eecp], ah
        add esi, ebx
        call mrd                    ; USBCMD
        test al, 1                  ; Run/Stop
        jnz .busy
        movzx di, byte [e_eecp]
        cmp di, 0x40
        jb .r
        mov bx, [e_bdf]
        mov ax, 0xB10A
        int 0x1A
        jc .r
        cmp cl, 1
        jne .r
        test ecx, 0x10000
        jz .r
.busy:
        mov byte [ehci_busy], 1
.r:
        ret
e_bdf       dw 0
e_eecp      db 0
ehci_busy   db 0
n_hc        db 0, 0, 0, 0
hc_names    dw msg_hc0, msg_hc1, msg_hc2, msg_hc3
msg_hcs     db "Controladores USB:", 0
msg_hc0     db " UHCI ", 0
msg_hc1     db ", OHCI ", 0
msg_hc2     db ", EHCI ", 0
msg_hc3     db ", xHCI ", 0
msg_ehcibusy db " (la BIOS usa el EHCI)", 0

; ¿Ha arrancado el sistema desde un disco USB que maneja la BIOS? Si HSPUSB
; le quita el controlador a la BIOS, ese disco deja de existir y el sistema
; se queda sin su unidad de arranque. Se mira la unidad 80h con las
; extensiones de INT 13h (EDD 3.0: tipo de interfaz "USB"); si la BIOS no lo
; dice, se supone que sí cuando la BIOS tiene en marcha algún controlador USB.
bios_usbdisk:
        mov byte [boot_usb], 0
        mov ax, 0x3305
        xor dl, dl
        int 0x21
        cmp dl, 3
        jb .r                       ; arranque desde disquete
        mov ah, 0x41
        mov bx, 0x55AA
        mov dl, 0x80
        int 0x13
        jc .unk
        cmp bx, 0xAA55
        jne .unk
        push es
        push cs
        pop es
        mov di, edd_buf
        mov cx, 0x4A / 2 ; (hueco de sobra)
        xor ax, ax
        cld
        rep stosw
        pop es
        mov word [edd_buf], 0x42
        mov ah, 0x48
        mov dl, 0x80
        mov si, edd_buf
        int 0x13
        jc .unk
        cmp word [edd_buf], 0x42
        jb .unk
        cmp word [edd_buf+0x1E], 0xBEDD
        jne .unk
        mov eax, [edd_buf+0x28]
        cmp eax, 'USB '
        je .usb
        and eax, 0x00FFFFFF
        cmp eax, 'USB'
        je .usb
        cmp al, 'A'                 ; "ATA", "SATA", "SCSI"...: no es USB
        jae .r
.unk:
        mov al, [hc_running]
        or al, [hc_owned]
        or al, [ehci_busy]
        jz .r
.usb:
        mov byte [boot_usb], 1
.r:
        call unreal
        ret

; /U: guardar el sector 0 y la geometría del disco de arranque de la BIOS
; (para reconocerlo después entre las memorias USB y seguir sirviéndolo)
boot_save:
        push es
        push cs
        pop es
        mov bx, BOOT_SEC
        add bx, [mem_off]
        mov ax, 0x0201
        mov cx, 1
        mov dx, 0x0080
        int 0x13
        pop es
        jc .r
        push es
        mov ah, 8
        mov dl, 0x80
        int 0x13
        pop es
        jc .r
        mov [i13_cx], cx
        mov [i13_dx], dx
        mov byte [boot_saved], 1
.r:
        call unreal
        ret

; /INFO: el controlador y lo que hace la BIOS con él
info_ctrl:
        cmp byte [opt_info], 0
        jne .go
        ret
.go:
        mov si, msg_ic1
        call puts
        mov bx, [bdf]
        mov ax, 0xB109
        xor di, di
        int 0x1A
        mov ax, cx
        call puthex4
        mov dl, ':'
        call putc
        mov bx, [bdf]
        mov ax, 0xB109
        mov di, 2
        int 0x1A
        mov ax, cx
        call puthex4
        call unreal
        mov si, msg_ic2
        call puts
        mov esi, [xbase]
        call mrd
        shr eax, 16
        call puthex4
        mov si, msg_ic3
        call puts
        movzx eax, word [ctx_sz]
        call putdec
        mov si, msg_ic4
        call puts
        mov al, [hc_owned]
        call .yn
        mov si, msg_ic5
        call puts
        mov al, [hc_running]
        call .yn
        mov si, msg_ic6
        call puts
        mov al, [boot_usb]
        call .yn
        call crlf
        ret
.yn:
        mov si, msg_si
        test al, al
        jnz .y
        mov si, msg_no
.y:
        call puts
        ret
msg_ic1     db "xHCI ", 0
msg_ic2     db " v", 0
msg_ic3     db " ctx ", 0
msg_ic4     db ". BIOS: suyo ", 0
msg_ic5     db ", en marcha ", 0
msg_ic6     db ", arranque USB ", 0
msg_si      db "sí", 0
msg_no      db "no", 0
hc_running  db 0
hc_owned    db 0
boot_usb    db 0
boot_saved  db 0
boot_match  db 0
opt_boot    db 0
opt_info    db 0
            align 2
edd_buf     times 0x4A db 0

; ---- encontrar e inicializar el controlador xHCI ----
xhci_setup:
        mov word [fail_msg], msg_nox
        smsw ax
        test al, 1
        jz .rm
        mov word [fail_msg], msg_v86
        stc
        ret
.rm:
        mov ax, 0xB101
        xor edi, edi
        int 0x1A
        jc .no
        cmp edx, 0x20494350
        jne .no
        mov ax, 0xB103
        mov ecx, 0x0C0330
        xor si, si
        int 0x1A
        jc .no
        mov [bdf], bx
        mov ax, 0xB10A
        mov di, 0x10
        int 0x1A
        mov [bar_lo], cl
        and ecx, 0xFFFFFFF0
        mov [xbase], ecx
        jz .no
        ; la mitad alta solo existe si el BAR es de 64 bits (tipo 10b); en
        ; uno de 32 bits lo que hay en 14h es otro BAR distinto
        mov al, [bar_lo]
        and al, 6
        cmp al, 4
        jne .lo
        mov bx, [bdf]
        mov ax, 0xB10A
        mov di, 0x14
        int 0x1A
        test ecx, ecx
        jz .lo
        mov word [fail_msg], msg_high
        stc
        ret
.lo:
        mov bx, [bdf]
        mov ax, 0xB109
        mov di, 0x04
        int 0x1A
        or cx, 0x0006
        mov bx, [bdf]
        mov ax, 0xB10C
        mov di, 0x04
        int 0x1A
        ; GDT del modo irreal y zona de memoria alineada a 4 KB
        xor eax, eax
        mov ax, cs
        shl eax, 4
        mov ebx, eax
        add eax, gdt_tbl
        mov [gdtr+2], eax
        mov eax, ebx
        add eax, file_end + 4095        ; tras todo el fichero: el código de inicio sigue en uso
        and eax, 0xFFFFF000
        sub eax, ebx
        mov [mem_off], ax
        mov di, ax
        mov cx, MEM_SIZE / 2
        xor ax, ax
        push es
        push cs
        pop es
        cld
        rep stosw
        pop es
        call unreal
        mov si, msg_found
        call puts
        mov eax, [xbase]
        call puthex8
        call crlf
        ; registros de capacidad
        mov esi, [xbase]
        call mrd
        movzx ebx, al               ; CAPLENGTH
        mov eax, [xbase]
        add eax, ebx
        mov [op_base], eax
        mov esi, [xbase]
        add esi, 0x04
        call mrd
        shr eax, 24
        mov [maxports], al
        mov esi, [xbase]
        add esi, 0x10
        call mrd
        mov [hcc1], eax
        test al, 4
        jz .c32
        mov word [ctx_sz], 64
.c32:
        mov esi, [xbase]
        add esi, 0x14
        call mrd
        and eax, 0xFFFFFFFC
        add eax, [xbase]
        mov [db_base], eax
        mov esi, [xbase]
        add esi, 0x18
        call mrd
        and eax, 0xFFFFFFE0
        add eax, [xbase]
        mov [rt_base], eax
        ; ¿la BIOS usa el controlador? Es suyo (USB Legacy Support) o lo
        ; tiene en marcha: sin /F no se toca. Y si el sistema ha arrancado de
        ; una memoria USB de la BIOS, solo con /U.
        mov esi, [op_base]
        call mrd
        and al, 1
        mov [hc_running], al
        call legacy_find
        jc .nown
        call mrd
        test eax, 0x10000
        jz .nown
        mov byte [hc_owned], 1
.nown:
        call bios_usbdisk
        call info_ctrl
        cmp byte [opt_boot], 0
        jne .take
        mov word [fail_msg], msg_bootusb
        cmp byte [boot_usb], 0
        jne .refuse
        cmp byte [opt_force], 0
        jne .take
        mov word [fail_msg], msg_bios
        mov al, [hc_running]
        or al, [hc_owned]
        jz .take
.refuse:
        stc
        ret
.take:
        cmp byte [boot_usb], 0
        je .tk
        call boot_save
.tk:
        call legacy_handoff
        ; parar y reiniciar
        mov esi, [op_base]
        call mrd
        and al, 0xFE
        call mwr
        mov cx, 10
        call tmo_start
.h:
        mov esi, [op_base]
        add esi, 4
        call mrd
        test al, 1                  ; HCHalted
        jnz .halted
        call tmo_check
        jnc .h
.halted:
        ; dejar que la BIOS gaste los eventos que le queden (hay BIOS, como
        ; SeaBIOS, que sondean su teclado USB con el reloj y escribirían en
        ; nuestros registros después del reinicio)
        sti
        mov cx, 3
        call tmo_start
.bw:
        call tmo_check
        jnc .bw
        mov esi, [op_base]
        mov eax, 2                  ; HCRST
        call mwr
        mov cx, 20
        call tmo_start
.rs:
        call mrd
        test al, 2
        jz .rsd
        call tmo_check
        jnc .rs
        mov word [fail_msg], msg_reset
        stc
        ret
.rsd:
        mov byte [hc_ours], 1
        mov esi, [op_base]
        add esi, 4
.cnr:
        call mrd
        test eax, 0x800             ; Controller Not Ready
        jz .ready
        call tmo_check
        jnc .cnr
.ready:
        ; slots, DCBAA, anillo de órdenes y anillo de eventos
        mov esi, [op_base]
        add esi, 0x38
        mov eax, MAX_SLOTS
        call mwr
        mov bx, DCBAA
        call zone
        call phys
        mov esi, [op_base]
        add esi, 0x30
        call mwr
        add esi, 4
        xor eax, eax
        call mwr
        call scratch_setup
        jc .r
        call intel_routes
        mov bx, CMD_RING
        call zone
        call phys
        or al, 1
        mov esi, [op_base]
        add esi, 0x18
        call mwr
        add esi, 4
        xor eax, eax
        call mwr
        mov bx, EVT_RING
        call zone
        call phys
        mov bx, ERST
        call zone
        mov [cs:bx], eax
        mov dword [cs:bx+4], 0
        mov dword [cs:bx+8], RING_TRBS
        mov esi, [rt_base]
        add esi, 0x20 + 0x08        ; ERSTSZ
        mov eax, 1
        call mwr
        mov bx, EVT_RING
        call zone
        call phys
        mov esi, [rt_base]
        add esi, 0x20 + 0x18        ; ERDP
        call mwr
        add esi, 4
        xor eax, eax
        call mwr
        mov bx, ERST
        call zone
        call phys
        mov esi, [rt_base]
        add esi, 0x20 + 0x10        ; ERSTBA
        call mwr
        add esi, 4
        xor eax, eax
        call mwr
        ; en marcha (sin interrupciones: se consulta el anillo de eventos)
        mov esi, [op_base]
        mov eax, 1
        call mwr
        mov cx, 10
        call tmo_start
        add esi, 4
.run:
        call mrd
        test al, 1
        jz .running
        call tmo_check
        jnc .run
.running:
        clc
.r:
        ret
.no:
        stc
        ret

; parar el controlador (si lo hemos reiniciado nosotros)
xhci_stop:
        cmp byte [hc_ours], 0
        je .r
        mov esi, [op_base]
        call mrd
        and al, 0xFE
        call mwr
        mov cx, 10
        call tmo_start
.h:
        mov esi, [op_base]
        add esi, 4
        call mrd
        test al, 1
        jnz .r
        call tmo_check
        jnc .h
.r:
        ret
hc_ours     db 0
bar_lo      db 0

; Páginas de trabajo del controlador («scratchpad»). Los xHCI de verdad (los
; de Intel entre ellos) piden en HCSPARAMS2 un número de páginas de 4 KB para
; su uso; hay que dárselas en DCBAA[0] o no arrancan, o escriben donde apunte
; la dirección 0. El de QEMU no pide ninguna. Se toman de la memoria
; extendida (XMS), que el controlador alcanza por dirección física.
scratch_setup:
        mov esi, [xbase]
        add esi, 0x08
        call mrd
        mov ebx, eax
        shr ebx, 27                 ; bits 31-27: parte baja
        mov ecx, eax
        shr ecx, 21
        and ecx, 0x1F               ; bits 25-21: parte alta
        shl ecx, 5
        or ebx, ecx
%ifdef FORCE_SCRATCH
        mov ebx, FORCE_SCRATCH      ; (para probar este camino en QEMU)
%endif
        test ebx, ebx
        jz .ok
        mov [n_scratch], bx
        mov word [fail_msg], msg_scratch
        mov ax, 0x4300
        int 0x2F
        cmp al, 0x80
        jne .err
        push es
        mov ax, 0x4310
        int 0x2F
        mov [xms], bx
        mov [xms+2], es
        pop es
        ; 2 páginas para la tabla + n páginas + 4 KB para alinear
        mov dx, [n_scratch]
        add dx, 3
        shl dx, 2
        mov ah, 0x09
        call far [xms]
        test ax, ax
        jz .err
        mov ah, 0x0C                ; bloquear: DX:BX = dirección lineal
        call far [xms]
        test ax, ax
        jz .err
        shl edx, 16
        mov dx, bx
        add edx, 0xFFF
        and edx, 0xFFFFF000
        mov [scr_base], edx
        call unreal                 ; (A20 ya está abierta: a20_open)
        mov edi, [scr_base]
        movzx ecx, word [n_scratch]
        add ecx, 2
        shl ecx, 10
        xor eax, eax
.z:
        mov [fs:edi], eax
        add edi, 4
        dec ecx
        jnz .z
        mov edi, [scr_base]
        mov eax, edi
        add eax, 0x2000
        movzx ecx, word [n_scratch]
.t:
        mov [fs:edi], eax
        mov dword [fs:edi+4], 0
        add edi, 8
        add eax, 0x1000
        dec ecx
        jnz .t
        mov bx, DCBAA
        call zone
        mov eax, [scr_base]
        mov [cs:bx], eax
        mov dword [cs:bx+4], 0
.ok:
        clc
        ret
.err:
        stc
        ret
n_scratch   dw 0
xms         dd 0
scr_base    dd 0

; Intel (series 7 y 8, Bay Trail): los puertos USB 2 salen de fábrica por el
; controlador EHCI; hay que pasarlos al xHCI (registros XUSB2PR y USB3_PSSEN
; con sus máscaras), como hacen los demás sistemas.
intel_routes:
        ; si la BIOS está usando el EHCI (su teclado o su disco USB), los
        ; puertos se quedan donde están salvo que se pida con /F o /U
        cmp byte [opt_force], 0
        jne .go
        cmp byte [ehci_busy], 0
        jne .r
.go:
        mov bx, [bdf]
        mov ax, 0xB109
        xor di, di
        int 0x1A
        cmp cx, 0x8086
        jne .r
        mov bx, [bdf]
        mov ax, 0xB10A
        mov di, 0xDC                ; USB3PRM
        int 0x1A
        mov bx, [bdf]
        mov ax, 0xB10D
        mov di, 0xD8                ; USB3_PSSEN
        int 0x1A
        mov bx, [bdf]
        mov ax, 0xB10A
        mov di, 0xD4                ; USB2PRM
        int 0x1A
        mov bx, [bdf]
        mov ax, 0xB10D
        mov di, 0xD0                ; XUSB2PR
        int 0x1A
.r:
        ret

; registro USBLEGSUP del controlador -> ESI; CF si no lo tiene
legacy_find:
        mov eax, [hcc1]
        shr eax, 16
        jz .no
        shl eax, 2
        add eax, [xbase]
        mov esi, eax
.cap:
        call mrd
        cmp al, 1
        je .leg
        shr eax, 8
        and eax, 0xFF
        jz .no
        shl eax, 2
        add esi, eax
        jmp .cap
.leg:
        clc
        ret
.no:
        stc
        ret

; ceder la propiedad del controlador desde la BIOS (si la tiene)
legacy_handoff:
        call legacy_find
        jc .ok
        call mrd
        test eax, 0x10000           ; propiedad de la BIOS
        jz .ok
        or eax, 0x1000000           ; propiedad del sistema
        call mwr
        mov cx, 20
        call tmo_start
.w:
        call mrd
        test eax, 0x10000
        jz .done
        call tmo_check
        jnc .w
.done:
        ; desactivar las SMI de la BIOS
        add esi, 4
        mov eax, 0xE0000000
        call mwr
.ok:
        ret

; ---- puertos: enumerar todos los dispositivos conectados ----
usb_enum:
        mov word [fail_msg], msg_nodev
        ; dar tiempo a que los puertos se estabilicen
        mov cx, 4
        call tmo_start
.wt:
        call tmo_check
        jnc .wt
        ; si aún no hay nada conectado, hasta un segundo más: tras reiniciar
        ; el controlador los puertos vuelven a dar corriente y una memoria
        ; de verdad tarda en presentarse (más aún si cambia de EHCI a xHCI)
        mov cx, 18
        call tmo_start
.wc:
        xor cl, cl
.wp:
        inc cl
        cmp cl, [maxports]
        ja .wn
        movzx esi, cl
        call port_sc
        test al, 1
        jz .wp
        mov cx, 3                   ; algo hay: un poco más para los demás
        call tmo_start
.wd:
        call tmo_check
        jnc .wd
        jmp .wgo
.wn:
        call tmo_check
        jnc .wc
.wgo:
        call info_ports
        call enum_ports
        ; /U: la memoria de arranque tiene que aparecer. Puede tardar (vuelve
        ; a conectarse tras reiniciar el controlador, o pasa del EHCI al
        ; xHCI): se la espera hasta unos 4 s mirando los puertos nuevos.
        cmp byte [boot_saved], 0
        je .end
        mov cx, 8
.bw:
        cmp byte [boot_match], 0
        jne .end
        push cx
        mov cx, 9
        call tmo_start
.bt:
        call tmo_check
        jnc .bt
        call enum_ports
        pop cx
        loop .bw
        jmp .end

; enumerar los puertos del controlador con algo conectado que no se hayan
; visto ya (port_done)
enum_ports:
        xor cl, cl
.pl:
        inc cl
        cmp cl, [maxports]
        ja .end
        movzx bx, cl
        bt [port_done], bx
        jc .pl
        movzx esi, cl
        dec esi
        shl esi, 4
        add esi, [op_base]
        add esi, 0x400
        call mrd
        test al, 1                  ; conectado
        jz .pl
        test al, 2                  ; habilitado (USB 3 lo hace solo)
        jnz .enabled
        ; reiniciar el puerto (USB 2)
        and eax, 0x0E00C3E0
        or eax, (1 << 9) | (1 << 4) ; PP | PR
        call mwr
        push cx
        mov cx, 10
        call tmo_start
.pr:
        call mrd
        test eax, 1 << 21           ; PRC
        jnz .prd
        call tmo_check
        jnc .pr
.prd:
        call reset_rest
        pop cx
        call mrd
        and eax, 0x0E00C3E0
        or eax, (1 << 9) | 0x00FE0000   ; borrar los cambios
        call mwr
        call mrd
        test al, 2
        jz .pl
.enabled:
        mov [port], cl
        shr eax, 10
        and al, 0x0F
        mov [speed], al
        movzx bx, cl
        bts [port_done], bx
        push cx
        call root_dev
        call enum_one
        pop cx
        jmp .pl
.end:
        ret
port_done   times 32 db 0

usb_enum.end:
        mov al, [disk_found]
        or al, [k_on]
        or al, [m_on]
        jz .none
        clc
        ret
.none:
        stc
        ret

; enumerar el dispositivo que acaba de habilitarse: [port] (puerto del
; controlador), [speed], [route], [depth], [tt_slot], [tt_port]. Si es un
; concentrador se enumera también lo que cuelga de él.
enum_one:
        mov byte [d_have], 0
        mov byte [d_role], 6
        ; una entrada libre de la reserva
        xor bx, bx
.pf:
        cmp bl, [pool_max]
        jae .full
        cmp byte [pool_used+bx], 0
        je .pg
        inc bx
        jmp .pf
.full:
        mov word [fail_msg], msg_full
        call info_line
        ret
.pg:
        mov al, bl
        call pool_sel
        mov si, [cur_st]
        mov byte [si], 0
        mov byte [si+1], 1
        mov bx, [cur_ep0]
        mov cx, 0x100
        call zero_zone
        mov bx, [cur_out]
        mov cx, 0x800
        call zero_zone
        cmp byte [opt_verbose], 0
        je .q1
        mov si, msg_port
        call puts
        movzx eax, byte [port]
        call putdec
        mov si, msg_speed
        call puts
        movzx eax, byte [speed]
        call putdec
        call crlf
.q1:
        ; Enable Slot
        xor eax, eax
        xor edx, edx
        xor ecx, ecx
        mov ebp, 9 << 10
        call xhci_cmd
        mov word [fail_msg], msg_slot
        jnc .slok
        call show_cc
        call info_line
        jmp .err
.slok:
        mov [cur_slot], ah
        ; DCBAA[slot] = contexto de salida
        mov bx, [cur_out]
        call zone
        call phys
        movzx di, byte [cur_slot]
        shl di, 3
        mov bx, DCBAA
        call zone
        add di, bx
        mov [cs:di], eax
        mov dword [cs:di+4], 0
        ; contexto de entrada: slot y EP0
        call clear_in
        mov bx, IN_CTX
        call zone
        mov dword [cs:bx+4], 3                  ; A0 | A1
        add bx, [ctx_sz]
        mov eax, 1                              ; una entrada de contexto
        call slot_fill
        add bx, [ctx_sz]
        ; tamaño máximo de paquete de EP0 según la velocidad
        mov eax, 8
        cmp byte [speed], 3
        jb .m
        mov eax, 64
        cmp byte [speed], 4
        jb .m
        mov eax, 512
.m:
        mov [mps0], ax
        shl eax, 16
        or eax, (3 << 1) | (4 << 3)             ; CErr=3, tipo Control
        mov [cs:bx+4], eax
        push bx
        mov bx, [cur_ep0]
        call zone
        call phys
        pop bx
        or al, 1
        mov [cs:bx+8], eax
        mov dword [cs:bx+12], 0
        mov dword [cs:bx+16], 8
        ; Address Device
        mov bx, IN_CTX
        call zone
        call phys
        xor edx, edx
        xor ecx, ecx
        movzx ebp, byte [cur_slot]
        shl ebp, 24
        or ebp, 11 << 10
        call xhci_cmd
        mov word [fail_msg], msg_addr
        jnc .adok
        call show_cc
        jmp .dis
.adok:
        ; descriptor de dispositivo: primero 8 bytes (tamaño de paquete de EP0)
        mov dword [ctl_setup], 0x01000680
        mov dword [ctl_setup+4], 0x00080000
        call control
        mov word [fail_msg], msg_desc
        jc .dis
        cmp byte [speed], 4
        jae .fulld
        mov bx, DESC
        call zone
        movzx ax, byte [cs:bx+7]
        cmp ax, 8
        jb .fulld
        cmp ax, [mps0]
        je .fulld
        mov [mps0], ax
        ; Evaluate Context con el tamaño de paquete real
        call clear_in
        mov bx, IN_CTX
        call zone
        mov dword [cs:bx+4], 2                  ; A1
        add bx, [ctx_sz]
        add bx, [ctx_sz]
        movzx eax, word [mps0]
        shl eax, 16
        or eax, (3 << 1) | (4 << 3)
        mov [cs:bx+4], eax
        mov bx, IN_CTX
        call zone
        call phys
        xor edx, edx
        xor ecx, ecx
        movzx ebp, byte [cur_slot]
        shl ebp, 24
        or ebp, 13 << 10
        call xhci_cmd
        jc .dis
.fulld:
        mov dword [ctl_setup], 0x01000680
        mov dword [ctl_setup+4], 0x00120000
        call control
        jc .dis
        mov bx, DESC
        call zone
        mov ax, [cs:bx+8]
        mov [d_vid], ax
        mov ax, [cs:bx+10]
        mov [d_pid], ax
        mov byte [n_if], 0
        mov byte [d_have], 1
        ; descriptor de configuración: primero 9 bytes, después entero
        mov dword [ctl_setup], 0x02000680
        mov dword [ctl_setup+4], 0x00090000
        call control
        jc .dis
        mov bx, DESC
        call zone
        mov ax, [cs:bx+2]
        cmp ax, 255
        jbe .tl
        mov ax, 255
.tl:
        mov [ctl_setup+6], ax
        call control
        jc .dis
        call parse_config
        mov byte [d_role], 0
        ; ¿qué papel tiene?
        cmp byte [p_hub], 0
        je .nohub
        call role_hub
        jc .bad
        mov byte [d_role], 5
        movzx bx, byte [cur_pool]
        mov byte [pool_used+bx], 1
        call info_line
        mov bl, [new_hub]
        xor bh, bh
        call hub_scan
        ret
.nohub:
        cmp byte [p_msc], 0
        je .hid
        cmp byte [disk_found], 0
        jne .hid
        call role_disk
        jc .bad
        mov byte [d_role], 1
        jmp .used
.hid:
        cmp byte [only_disk], 0
        jne .dis
        mov byte [want_k], 0
        mov byte [want_m], 0
        cmp byte [p_kbd], 0
        je .hm
        cmp byte [k_on], 0
        jne .hm
        mov byte [want_k], 1
.hm:
        cmp byte [p_mou], 0
        je .hc
        cmp byte [m_on], 0
        jne .hc
        mov byte [want_m], 1
.hc:
        mov al, [want_k]
        or al, [want_m]
        jz .dis
        call role_hid
        jc .bad
        mov al, [want_k]
        add al, al
        add al, [want_m]            ; 2 teclado, 1 ratón, 3 los dos
        mov ah, 3
        cmp al, 1
        je .hr
        mov ah, 2
        cmp al, 2
        je .hr
        mov ah, 4
.hr:
        mov [d_role], ah
.used:
        movzx bx, byte [cur_pool]
        mov byte [pool_used+bx], 1
        call info_line
        ret
.bad:
        mov byte [d_role], 6
.dis:
        ; no se usa: liberar el slot
        call info_line
        mov al, [cur_slot]
        call slot_free
.err:
        ret

; contexto del slot (CS:BX) del dispositivo que se enumera: EAX = número de
; entradas de contexto. Cadena de ruta, velocidad, puerto del controlador y
; traductor de transacciones; con [hubf], los datos de concentrador.
slot_fill:
        push edx
        shl eax, 27
        movzx edx, byte [speed]
        shl edx, 20
        or eax, edx
        or eax, [route]
        cmp byte [hubf], 0
        je .a
        or eax, 1 << 26                         ; Hub
.a:
        mov [cs:bx], eax
        movzx eax, byte [port]
        shl eax, 16
        movzx edx, byte [hub_np]
        shl edx, 24                             ; número de puertos
        or eax, edx
        mov [cs:bx+4], eax
        movzx eax, byte [tt_slot]
        movzx edx, byte [tt_port]
        shl edx, 8
        or eax, edx
        movzx edx, byte [hub_ttt]
        shl edx, 16                             ; TT Think Time
        or eax, edx
        mov [cs:bx+8], eax
        pop edx
        ret

; concentrador: configurarlo, decírselo al controlador (Hub = 1, puertos,
; TT Think Time) y dar corriente a sus puertos -> [new_hub]; CF si falla.
; Se deja en el modo de un solo traductor (interfaz alternativa 0), así que
; MTT = 0, que es lo que pide la especificación en ese caso.
role_hub:
        mov word [fail_msg], msg_hubmax
        cmp byte [depth], HUB_DEPTH
        jae .err
        xor bx, bx
.f:
        cmp byte [h_used+bx], 0
        je .got
        inc bx
        cmp bx, HUB_N
        jb .f
        jmp .err
.got:
        mov [new_hub], bl
        call set_config
        mov word [fail_msg], msg_cfg
        jc .err
%ifdef STALL_TEST
        ; (prueba) una petición que el concentrador rechaza: lo que sigue
        ; solo funciona si el EP0 se reanuda
        mov eax, 0x000055A0
        mov edx, 0x00080000
        call ctl_req
        jc .st1
        mov word [fail_msg], msg_hubdesc
        jmp .err
.st1:
%endif
        ; descriptor de concentrador (29h; 2Ah en USB 3)
        mov eax, 0x290006A0
        cmp byte [speed], 4
        jb .d2
        mov eax, 0x2A0006A0
.d2:
        mov edx, 0x000C0000
        call ctl_req
        mov word [fail_msg], msg_hubdesc
        jc .err
        mov bx, DESC
        call zone
        mov al, [cs:bx+2]                       ; bNbrPorts
        test al, al
        jz .err
        cmp al, 15
        jbe .np
        mov al, 15
.np:
        mov [hub_np], al
        mov al, [cs:bx+5]                       ; bPwrOn2PwrGood (2 ms)
        mov [hub_pwr], al
        mov al, [cs:bx+3]                       ; wHubCharacteristics
        shr al, 5
        and al, 3
        cmp byte [speed], 3
        je .tt
        xor al, al
.tt:
        mov [hub_ttt], al
        call clear_in
        mov bx, IN_CTX
        call zone
        mov dword [cs:bx+4], 1                  ; A0: solo el slot
        add bx, [ctx_sz]
        mov byte [hubf], 1
        mov eax, 1
        call slot_fill
        mov byte [hubf], 0
        call cfg_ep_cmd
        mov word [fail_msg], msg_epcfg
        jnc .cok
        call show_cc
        mov word [hub_np], 0                    ; (y hub_ttt)
        jmp .err
.cok:
        movzx bx, byte [new_hub]
        mov byte [h_used+bx], 1
        mov al, [cur_slot]
        mov [h_slot+bx], al
        mov al, [cur_pool]
        mov [h_pool+bx], al
        mov al, [hub_np]
        mov [h_np+bx], al
        mov [d_np], al
        mov al, [speed]
        mov [h_speed+bx], al
        mov al, [port]
        mov [h_root+bx], al
        mov al, [depth]
        mov [h_depth+bx], al
        mov al, [tt_slot]
        mov [h_ttslot+bx], al
        mov al, [tt_port]
        mov [h_ttport+bx], al
        shl bx, 2
        mov eax, [route]
        mov [h_route+bx], eax
        mov word [hub_np], 0                    ; (y hub_ttt)
        mov al, [new_hub]
        mov [hp_hub], al
        ; USB 3: SET_HUB_DEPTH
        cmp byte [speed], 4
        jb .pw
        movzx eax, byte [depth]
        shl eax, 16
        or eax, 0x0C20
        xor edx, edx
        call ctl_req
.pw:
        ; corriente a los puertos (PORT_POWER) y esperar a que sea estable
        mov cl, 1
.pl:
        cmp cl, [d_np]
        ja .pd
        mov [hp_port], cl
        push cx
        mov ax, 0x0308
        call hub_feat
        pop cx
        inc cl
        jmp .pl
.pd:
        movzx ax, byte [hub_pwr]
        shl ax, 1
        mov cl, 55
        div cl
        movzx cx, al
        add cx, 3
        call tmo_start
.w:
        call tmo_check
        jnc .w
        clc
        ret
.err:
        stc
        ret

; ---- /INFO: una línea por dispositivo ----
info_line:
        cmp byte [opt_info], 0
        jne .go
        ret
.go:
        pushad
        mov cl, [depth]
        inc cl
.in:
        mov dl, ' '
        call putc
        mov dl, ' '
        call putc
        dec cl
        jnz .in
        movzx eax, byte [port]
        call putdec
        mov ebx, [route]
        mov cl, [depth]
.rt:
        test cl, cl
        jz .rtd
        mov dl, '.'
        call putc
        mov eax, ebx
        and eax, 15
        call putdec
        shr ebx, 4
        dec cl
        jmp .rt
.rtd:
        mov dl, ' '
        call putc
        movzx bx, byte [speed]
        cmp bx, 4
        jbe .spk
        xor bx, bx
.spk:
        shl bx, 1
        mov dl, [spd_tab+bx]
        call putc
        mov dl, [spd_tab+bx+1]
        call putc
        mov dl, ' '
        call putc
        cmp byte [d_have], 0
        je .fail
        mov ax, [d_vid]
        call puthex4
        mov dl, ':'
        call putc
        mov ax, [d_pid]
        call puthex4
        ; interfaces: clase/subclase/protocolo
        xor bx, bx
.if:
        cmp bl, [n_if]
        jae .ifd
        mov dl, ' '
        call putc
        mov al, [if_tab+bx]
        call puthex2
        mov dl, '/'
        call putc
        mov al, [if_tab+bx+1]
        call puthex2
        mov dl, '/'
        call putc
        mov al, [if_tab+bx+2]
        call puthex2
        add bx, 3
        jmp .if
.ifd:
        mov si, msg_arrow
        call puts
        movzx bx, byte [d_role]
        shl bx, 1
        mov si, [role_tab+bx]
        call puts
        cmp byte [d_role], 5
        jne .nh
        movzx eax, byte [d_np]
        call putdec
        mov si, msg_nports
        call puts
.nh:
        cmp byte [d_role], 6
        je .fmsg
        call crlf
        jmp .r
.fail:
        mov si, msg_r6
        call puts
.fmsg:
        mov dl, ' '
        call putc
        mov si, [fail_msg]
        call puts
.r:
        popad
        ret

; /INFO: puertos del controlador con algo conectado
info_ports:
        cmp byte [opt_info], 0
        jne .go
        ret
.go:
        pushad
        mov si, msg_iports
        call puts
        xor cl, cl
.l:
        inc cl
        cmp cl, [maxports]
        ja .d
        movzx esi, cl
        call port_sc
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
        movzx eax, byte [maxports]
        call putdec
        mov si, msg_iports3
        call puts
        popad
        ret

putc:
        push ax
        mov ah, 2
        int 0x21
        pop ax
        ret
puthex4:
        push ax
        mov al, ah
        call puthex2
        pop ax
puthex2:
        push ax
        push dx
        push ax
        shr al, 4
        call .n
        pop ax
        and al, 15
        call .n
        pop dx
        pop ax
        ret
.n:
        add al, '0'
        cmp al, '9'
        jbe .p
        add al, 7
.p:
        mov dl, al
        call putc
        ret

d_have      db 0
d_role      db 0
d_np        db 0
n_if        db 0
d_vid       dw 0
d_pid       dw 0
IF_MAX      equ 8
if_tab      times IF_MAX * 3 db 0
spd_tab     db "??", "FS", "LS", "HS", "SS"
role_tab    dw msg_r0, msg_r1, msg_r2, msg_r3, msg_r4, msg_r5, msg_r6
msg_arrow   db " -> ", 0
msg_r0      db "sin uso", 0
msg_r1      db "disco", 0
msg_r2      db "teclado", 0
msg_r3      db "ratón", 0
msg_r4      db "teclado y ratón", 0
msg_r5      db "concentrador de ", 0
msg_r6      db "ERROR", 0
msg_nports  db " puertos", 0
msg_iports  db "Puertos con algo conectado:", 0
msg_iports2 db " (de ", 0
msg_iports3 db "). Velocidad: LS baja, FS plena, HS alta, SS súper.", 13, 10, 0

; /V: código de finalización de la orden que ha fallado (AL; FFh = sin respuesta)
show_cc:
        jnc .r
        pushf
        push ax
        cmp byte [opt_verbose], 0
        je .p
        push ax
        mov si, msg_cc
        call puts
        pop ax
        movzx eax, al
        call putdec
        call crlf
.p:
        pop ax
        popf
.r:
        ret

; SET_CONFIGURATION con el valor del descriptor de configuración en DESC
set_config:
        mov bx, DESC
        call zone
        movzx eax, byte [cs:bx+5]
        shl eax, 16
        or eax, 0x0900
        mov [ctl_setup], eax
        mov dword [ctl_setup+4], 0
        call control
        ret

; memoria USB: endpoints bulk
role_disk:
        call set_config
        mov word [fail_msg], msg_cfg
        jc .err
        ; anillos bulk desde el principio (puede haberlos usado otra memoria)
        mov word [bin_idx], 0x0100  ; índice 0, ciclo 1
        mov word [bout_idx], 0x0100
        mov bx, BIN_RING
        mov cx, 0x100
        call zero_zone
        mov bx, BOUT_RING
        mov cx, 0x100
        call zero_zone
        mov al, [cur_slot]
        mov [slot], al
        mov al, [t_in]
        mov [dci_in], al
        mov al, [t_out]
        mov [dci_out], al
        mov ax, [t_mps_in]
        mov [mps_in], ax
        mov ax, [t_mps_out]
        mov [mps_out], ax
        ; Configure Endpoint: bulk IN y bulk OUT
        call clear_in
        mov bx, IN_CTX
        call zone
        mov eax, 1
        mov cl, [dci_in]
        mov edx, 1
        shl edx, cl
        or eax, edx
        mov cl, [dci_out]
        mov edx, 1
        shl edx, cl
        or eax, edx
        mov [cs:bx+4], eax
        add bx, [ctx_sz]
        movzx eax, byte [dci_in]
        cmp al, [dci_out]
        jae .mx
        mov al, [dci_out]
.mx:
        call slot_fill
        mov byte [ep_ival], 0
        mov dword [ep_d4], 1024
        mov al, [dci_in]
        mov dx, BIN_RING
        mov cx, [mps_in]
        mov ah, 6                               ; bulk IN
        call set_ep
        mov al, [dci_out]
        mov dx, BOUT_RING
        mov cx, [mps_out]
        mov ah, 2                               ; bulk OUT
        call set_ep
        call cfg_ep_cmd
        mov word [fail_msg], msg_epcfg
        jc .err
        ; con /U solo vale la memoria de la que arrancó el sistema
        cmp byte [boot_saved], 0
        je .any
        cmp byte [boot_match], 0
        jne .any
        call boot_cmp
        mov word [fail_msg], msg_notboot
        jc .err
        mov byte [boot_match], 1
.any:
        mov byte [disk_found], 1
        mov al, [port]
        mov [dport], al
        mov al, [cur_pool]
        mov [dpool], al
        mov al, [par_hub]
        mov [d_hub], al
        mov al, [par_port]
        mov [d_hport], al
        call dport_sc
        call clr_csc                ; la conexión de ahora ya está vista
        clc
.err:
        ret

; ¿es esta memoria el disco de arranque de la BIOS? Se compara su sector 0
; con el que se leyó por INT 13h antes de tomar el controlador. CF si no.
boot_cmp:
        call disk_ready
        jc .r
        push es
        push cs
        pop es
        mov si, DATA
        add si, [mem_off]
        mov di, BOOT_SEC
        add di, [mem_off]
        mov cx, 256
        cld
        repe cmpsw
        pop es
        je .ok
        stc
        ret
.ok:
        clc
.r:
        ret

; Configure Endpoint del slot actual con el contexto de entrada
cfg_ep_cmd:
        mov bx, IN_CTX
        call zone
        call phys
        xor edx, edx
        xor ecx, ecx
        movzx ebp, byte [cur_slot]
        shl ebp, 24
        or ebp, 12 << 10
        call xhci_cmd
        ret

; teclado y/o ratón (want_k, want_m): protocolo de arranque y endpoints
role_hid:
        call set_config
        jc .err
        cmp byte [want_k], 0
        je .m1
        movzx eax, byte [pk_if]
        call set_boot
.m1:
        cmp byte [want_m], 0
        je .m2
        movzx eax, byte [pm_if]
        call set_boot
.m2:
        ; endpoints de interrupción
        call clear_in
        mov bx, IN_CTX
        call zone
        mov eax, 1
        xor dl, dl                              ; DCI mayor
        cmp byte [want_k], 0
        je .a1
        mov cl, [pk_dci]
        bts eax, ecx
        mov dl, cl
.a1:
        cmp byte [want_m], 0
        je .a2
        mov cl, [pm_dci]
        bts eax, ecx
        cmp cl, dl
        jbe .a2
        mov dl, cl
.a2:
        mov [cs:bx+4], eax
        add bx, [ctx_sz]
        movzx eax, dl
        call slot_fill
        cmp byte [want_k], 0
        je .e1
        mov al, [pk_ival]
        call calc_ival
        mov [ep_ival], al
        movzx eax, word [pk_mps]
        mov edx, eax
        shl edx, 16
        or eax, edx
        mov [ep_d4], eax
        mov al, [pk_dci]
        mov dx, KINT_RING
        mov cx, [pk_mps]
        mov ah, 7                               ; interrupción IN
        call set_ep
.e1:
        cmp byte [want_m], 0
        je .e2
        mov al, [pm_ival]
        call calc_ival
        mov [ep_ival], al
        movzx eax, word [pm_mps]
        mov edx, eax
        shl edx, 16
        or eax, edx
        mov [ep_d4], eax
        mov al, [pm_dci]
        mov dx, MINT_RING
        mov cx, [pm_mps]
        mov ah, 7
        call set_ep
.e2:
        call cfg_ep_cmd
        jc .err
        mov al, [cur_slot]
        cmp byte [want_k], 0
        je .o1
        mov [k_slot], al
        mov ah, [pk_dci]
        mov [k_dci], ah
        mov cx, [pk_mps]
        call cap_len
        mov [k_len], cx
        mov byte [k_on], 1
.o1:
        cmp byte [want_m], 0
        je .o2
        mov [m_slot], al
        mov ah, [pm_dci]
        mov [m_dci], ah
        mov cx, [pm_mps]
        call cap_len
        mov [m_len], cx
        mov byte [m_on], 1
.o2:
        clc
.err:
        ret
cap_len:
        cmp cx, 64
        jbe .r
        mov cx, 64
.r:
        ret

; SET_PROTOCOL (arranque) y SET_IDLE (0) a la interfaz EAX; los errores no
; importan (hay dispositivos que no admiten SET_IDLE)
set_boot:
        push eax
        shl eax, 0
        mov dword [ctl_setup], 0x00000B21
        mov [ctl_setup+4], eax
        call control
        pop eax
        mov dword [ctl_setup], 0x00000A21
        mov [ctl_setup+4], eax
        call control
        ret

; bInterval AL -> exponente del intervalo del xHCI (AL)
calc_ival:
        cmp byte [speed], 3
        jae .hs
        mov ah, 3                   ; FS/LS: milisegundos -> 2^n * 125 us
.l:
        cmp al, 1
        jbe .d
        shr al, 1
        inc ah
        jmp .l
.d:
        cmp ah, 10
        jbe .k
        mov ah, 10
.k:
        mov al, ah
        ret
.hs:
        test al, al
        jz .r
        dec al
        cmp al, 15
        jbe .r
        mov al, 15
.r:
        ret

; poner a cero CX bytes de la zona desde BX
zero_zone:
        push di
        push es
        call zone
        mov di, bx
        push cs
        pop es
        xor al, al
        cld
        rep stosb
        pop es
        pop di
        ret

want_k      db 0
want_m      db 0
disk_found  db 0
            db 0
mps0        dw 8
cur_out     dw OUT_CTXS
ep_ival     db 0
            db 0
ep_d4       dd 1024

clear_in:
        push di
        push cx
        push es
        mov di, IN_CTX
        add di, [mem_off]
        push cs
        pop es
        mov cx, 0x900 / 2
        xor ax, ax
        rep stosw
        pop es
        pop cx
        pop di
        ret

; contexto del endpoint DCI AL: AH = tipo, DX = anillo, CX = paquete máximo
set_ep:
        push bx
        movzx bx, al
        inc bx
        imul bx, [ctx_sz]
        add bx, IN_CTX
        call zone
        movzx eax, ah
        shl eax, 3
        or eax, 3 << 1                          ; CErr = 3
        movzx ecx, cx
        shl ecx, 16
        or eax, ecx
        mov [cs:bx+4], eax
        push bx
        mov bx, dx
        call zone
        call phys
        pop bx
        or al, 1
        mov [cs:bx+8], eax
        mov dword [cs:bx+12], 0
        mov eax, [cs:ep_d4]
        mov [cs:bx+16], eax
        movzx eax, byte [cs:ep_ival]
        shl eax, 16
        mov [cs:bx], eax
        pop bx
        ret

; recorrer el descriptor de configuración: memoria (08/06/50, bulk), teclado
; (03/01/01) y ratón (03/01/02) con su endpoint de interrupción de entrada
parse_config:
        mov bx, DESC
        call zone
        mov si, bx
        movzx cx, byte [ctl_setup+6]
        add cx, bx
        xor ax, ax
        mov [p_msc], al
        mov [p_kbd], al
        mov [p_mou], al
        mov [t_in], al
        mov [t_out], al
        mov [ifk], al
        mov [p_hub], al
.l:
        cmp si, cx
        jae .end
        mov al, [cs:si]
        test al, al
        jz .end
        mov ah, [cs:si+1]
        cmp ah, 4                   ; interfaz
        jne .ep
        mov byte [ifk], 0
        mov al, [cs:si+2]
        mov [ifn], al
        ; /INFO: clase/subclase/protocolo de cada interfaz (sin alternativas)
        cmp byte [cs:si+3], 0
        jne .nif
        movzx di, byte [n_if]
        cmp di, IF_MAX * 3
        jae .nif
        mov al, [cs:si+5]
        mov [if_tab+di], al
        mov ax, [cs:si+6]
        mov [if_tab+di+1], ax
        add byte [n_if], 3
.nif:
        cmp byte [cs:si+5], 9
        jne .nh
        mov byte [p_hub], 1
        jmp .n
.nh:
        cmp byte [cs:si+5], 8
        jne .hid
        cmp byte [cs:si+6], 6
        jne .n
        cmp byte [cs:si+7], 0x50
        jne .n
        mov byte [ifk], 1
        jmp .n
.hid:
        cmp byte [cs:si+5], 3
        jne .n
        cmp byte [cs:si+6], 1       ; subclase de arranque
        jne .n
        mov al, [cs:si+7]
        cmp al, 1
        jb .n
        cmp al, 2
        ja .n
        inc al
        mov [ifk], al               ; 2 teclado, 3 ratón
        jmp .n
.ep:
        cmp ah, 5                   ; endpoint
        jne .n
        mov dl, [cs:si+2]           ; dirección
        mov dh, dl
        and dh, 0x0F
        shl dh, 1                   ; DCI = 2 * número (+1 si es de entrada)
        test dl, 0x80
        jz .o
        inc dh
.o:
        mov di, [cs:si+4]
        and di, 0x7FF
        mov al, [cs:si+3]
        and al, 3
        cmp byte [ifk], 1
        jne .ek
        cmp al, 2                   ; bulk
        jne .n
        test dl, 0x80
        jz .bo
        mov [t_in], dh
        mov [t_mps_in], di
        jmp .n
.bo:
        mov [t_out], dh
        mov [t_mps_out], di
        jmp .n
.ek:
        cmp al, 3                   ; interrupción
        jne .n
        test dl, 0x80
        jz .n
        mov al, [cs:si+6]           ; bInterval
        cmp byte [ifk], 2
        jne .em
        cmp byte [p_kbd], 0
        jne .n
        mov [pk_dci], dh
        mov [pk_mps], di
        mov [pk_ival], al
        mov ah, [ifn]
        mov [pk_if], ah
        mov byte [p_kbd], 1
        jmp .n
.em:
        cmp byte [ifk], 3
        jne .n
        cmp byte [p_mou], 0
        jne .n
        mov [pm_dci], dh
        mov [pm_mps], di
        mov [pm_ival], al
        mov ah, [ifn]
        mov [pm_if], ah
        mov byte [p_mou], 1
.n:
        movzx ax, byte [cs:si]
        add si, ax
        jmp .l
.end:
        cmp byte [t_in], 0
        je .r
        cmp byte [t_out], 0
        je .r
        mov byte [p_msc], 1
.r:
        ret
ifk         db 0
p_hub       db 0
ifn         db 0
p_msc       db 0
p_kbd       db 0
p_mou       db 0
t_in        db 0
t_out       db 0
            db 0
t_mps_in    dw 0
t_mps_out   dw 0
pk_dci      db 0
pk_ival     db 0
pk_if       db 0
            db 0
pk_mps      dw 0
pm_dci      db 0
pm_ival     db 0
pm_if       db 0
            db 0
pm_mps      dw 0

; ---- disco: capacidad, partición y BPB ----
disk_setup:
        call disk_ready
        jc .err
        mov bx, DATA
        call zone
        jmp disk_sec0
.err:
        stc
        ret
; la memoria preparada, su capacidad y su sector 0 en DATA; CF si falla
disk_ready:
        mov word [fail_msg], msg_scsi
        ; TEST UNIT READY. Si falla se pide el motivo (REQUEST SENSE: borra
        ; el aviso de «recién conectado») y se espera un poco: un lector de
        ; tarjetas o un disco duro USB tardan en estar preparados.
        mov cx, 12
.tur:
        push cx
        mov dword [scsi_cb], 0
        mov dword [scsi_cb+4], 0
        mov byte [scsi_cblen], 6
        xor ebx, ebx
        xor ecx, ecx
        mov al, 0
        call scsi
        pop cx
        jnc .ready
        push cx
        mov dword [scsi_cb], 0x03
        mov dword [scsi_cb+4], 18
        mov byte [scsi_cblen], 6
        mov bx, DATA
        call zone
        call phys
        mov ebx, eax
        mov ecx, 18
        mov al, 1
        call scsi
        mov cx, 4
        call tmo_start
.tw:
        call tmo_check
        jnc .tw
        pop cx
        loop .tur
.ready:
        ; READ CAPACITY(10)
        mov dword [scsi_cb], 0x25
        mov dword [scsi_cb+4], 0
        mov word [scsi_cb+8], 0
        mov byte [scsi_cblen], 10
        mov bx, DATA
        call zone
        call phys
        mov ebx, eax
        mov ecx, 8
        mov al, 1
        call scsi
        jc .err
        mov bx, DATA
        call zone
        mov eax, [cs:bx]
        call bswap_eax
        inc eax
        mov [i13_total], eax
        shr eax, 11
        mov [cap_mb], eax
        mov eax, [cs:bx+4]
        call bswap_eax
        cmp eax, 512
        mov word [fail_msg], msg_bsize
        jne .err
        ; sector 0: ¿tabla de particiones o FAT directamente?
        mov dword [part_start], 0
        mov dword [rw_lba], 0
        mov word [rw_n], 1
        mov al, 0x28
        call scsi_rw
        mov word [fail_msg], msg_scsi
        jc .err
        clc
        ret
.err:
        stc
        ret
disk_sec0:
        cmp word [cs:bx+510], 0xAA55
        mov word [fail_msg], msg_nofat
        jne .err
        cmp word [cs:bx+0x0B], 512
        jne .mbr
        cmp byte [cs:bx+0x26], 0x29             ; BPB extendido: sin particiones
        je .bpb
        cmp byte [cs:bx+0x15], 0xF0
        jae .bpb
.mbr:
        mov si, bx
        add si, 0x1BE
        mov cx, 4
.pe:
        mov al, [cs:si+4]
        cmp al, 0x01
        je .gotp
        cmp al, 0x04
        je .gotp
        cmp al, 0x06
        je .gotp
        cmp al, 0x0E
        je .gotp
        add si, 16
        loop .pe
        jmp .err
.gotp:
        mov eax, [cs:si+8]
        mov [part_start], eax
        mov [rw_lba], eax
        mov word [rw_n], 1
        mov al, 0x28
        call scsi_rw
        jc .err
        mov bx, DATA
        call zone
        cmp word [cs:bx+0x0B], 512
        jne .err
.bpb:
        ; ¿de verdad FAT12/FAT16? Un FAT32 o un NTFS sin particiones también
        ; llevan 512 en +0Bh y F8h en +15h, pero no tienen sectores por FAT
        ; de 16 bits ni entradas de directorio raíz: al núcleo no se le da
        ; un BPB así
        mov word [fail_msg], msg_nofat
        cmp byte [cs:bx+0x0D], 0            ; sectores por clúster
        je .err
        cmp word [cs:bx+0x0E], 0            ; sectores reservados
        je .err
        cmp byte [cs:bx+0x10], 0            ; número de FAT
        je .err
        cmp word [cs:bx+0x11], 0            ; entradas del directorio raíz
        je .err
        cmp word [cs:bx+0x16], 0            ; sectores por FAT
        je .err
        mov si, bx
        add si, 0x0B
        mov di, bpb
        mov cx, 25
.cp:
        mov al, [cs:si]
        mov [cs:di], al
        inc si
        inc di
        loop .cp
        clc
        ret
.err:
        stc
        ret

; ---- texto ----
puts:
        push ax
        push dx
.l:
        mov dl, [cs:si]
        test dl, dl
        jz .d
        mov ah, 2
        int 0x21
        inc si
        jmp .l
.d:
        pop dx
        pop ax
        ret
crlf:
        push si
        mov si, msg_crlf
        call puts
        pop si
        ret
putdec:
        push ebx
        push ecx
        push edx
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
        pop dx
        add dl, '0'
        mov ah, 2
        int 0x21
        loop .p
        pop edx
        pop ecx
        pop ebx
        ret
puthex8:
        push cx
        mov cx, 8
.h:
        rol eax, 4
        push eax
        and al, 15
        add al, '0'
        cmp al, '9'
        jbe .d
        add al, 7
.d:
        mov dl, al
        mov ah, 2
        int 0x21
        pop eax
        loop .h
        pop cx
        ret

bdf         dw 0
maxports    db 0
opt_force   db 0
opt_verbose db 0
hcc1        dd 0
cap_mb      dd 0
fail_msg    dw 0
msg_title   db "HSPUSB 0.1 - USB nativo de Hispa-DOS (xHCI)", 13, 10, 0
msg_found   db "Controlador xHCI en ", 0
msg_port    db "Dispositivo en el puerto ", 0
msg_speed   db ", velocidad ", 0
msg_disk    db "Disco USB: ", 0
msg_cc      db "xHCI: código de finalización ", 0
msg_kbd     db "Teclado USB preparado.", 13, 10, 0
msg_mouse   db "Ratón USB preparado.", 13, 10, 0
units       db 0
msg_mbas    db " MB en la unidad "
msg_drv     db "?:", 13, 10, 0
msg_no8042  db "Sin controlador de teclado (8042): las teclas van al búfer de la BIOS.", 13, 10, 0
msg_scratch db "El controlador xHCI necesita memoria extendida: cargue HSPXMS.SYS", 13, 10
            db "antes que HSPUSB.SYS en CONFIG.SYS.", 13, 10, 0
msg_nox     db "No se ha encontrado ningún controlador xHCI (USB 3).", 13, 10, 0
msg_v86     db "No funciona con EMM386 (modo V86).", 13, 10, 0
msg_high    db "Los registros del xHCI están por encima de 4 GB: no se admite.", 13, 10, 0
msg_bios    db "La BIOS está usando el controlador USB (teclado USB o unidad de", 13, 10
            db "arranque): no se toca. Use /F para tomarlo igualmente.", 13, 10, 0
msg_bootusb db "El sistema ha arrancado desde una memoria USB que maneja la BIOS:", 13, 10
            db "HSPUSB no toma el controlador (la BIOS perdería el disco de arranque).", 13, 10
            db "Con /U lo toma y atiende él ese disco; si algo falla habrá que reiniciar.", 13, 10, 0
msg_bootok  db "Disco de arranque USB atendido por HSPUSB (INT 13h).", 13, 10, 0
msg_bootlost db "AVISO: la memoria de arranque no aparece entre los dispositivos USB.", 13, 10
            db "Si era USB, el sistema ya no puede leerla: reinicie sin /U.", 13, 10, 0
msg_notboot db "no es la memoria de arranque.", 13, 10, 0
msg_full    db "USB: demasiados dispositivos.", 13, 10, 0
msg_hubmax  db "USB: demasiados concentradores encadenados.", 13, 10, 0
msg_hubdesc db "USB: el concentrador no da su descriptor.", 13, 10, 0
msg_pause   db "Pausa de 15 s para la foto (una tecla la acorta)...", 13, 10, 0
msg_reset   db "El controlador xHCI no se reinicia.", 13, 10, 0
msg_nodev   db "No hay ninguna memoria, teclado ni ratón USB que se pueda usar.", 13, 10, 0
msg_slot    db "USB: ha fallado Enable Slot.", 13, 10, 0
msg_addr    db "USB: ha fallado Address Device.", 13, 10, 0
msg_desc    db "USB: no se pueden leer los descriptores.", 13, 10, 0
msg_cfg     db "USB: ha fallado Set Configuration.", 13, 10, 0
msg_epcfg   db "USB: ha fallado Configure Endpoint.", 13, 10, 0
msg_scsi    db "El disco USB no responde.", 13, 10, 0
msg_bsize   db "El disco USB no tiene sectores de 512 bytes.", 13, 10, 0
msg_nofat   db "El disco USB no tiene un sistema de archivos FAT12/FAT16.", 13, 10, 0
msg_crlf    db 13, 10, 0
file_end:
