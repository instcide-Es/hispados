; =============================================================================
;  HSPUSB.SYS - USB nativo de Hispa-DOS: controlador xHCI + memorias USB
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  CONFIG.SYS:  DEVICE=C:\DRIVERS\HSPUSB.SYS [/F] [/V]
;     /F  tomar el controlador aunque lo esté usando la BIOS (la BIOS dejará
;         de atender sus dispositivos USB: teclado USB, memoria de arranque)
;     /V  mensajes detallados
;
;  Busca un controlador xHCI (USB 3, el de los PCs actuales), lo inicializa y
;  enumera los dispositivos de los puertos:
;   - la primera memoria USB (clase 08h, SCSI, Bulk-Only Transport), que se
;     ofrece a DOS como una unidad nueva (FAT12/FAT16, con o sin particiones);
;   - un teclado USB (HID, protocolo de arranque): las teclas se entregan como
;     si vinieran del teclado PS/2 (orden D2h del controlador 8042), así que
;     funcionan la BIOS, KEYB y todos los programas;
;   - un ratón USB: se entrega a HSPRATON (INT 33h AX=48D0h) o, si no está,
;     como paquetes PS/2 (orden D3h del 8042).
;  Los informes del teclado y del ratón se recogen con la interrupción del
;  reloj (INT 08h). Usa el modo real "irreal" para los registros del xHCI, así
;  que no funciona en modo V86 (EMM386, Windows en modo extendido).
; =============================================================================
        cpu 486
        bits 16
        org 0

; desplazamientos en la zona de memoria alineada a 4 KB (mem_off)
IN_CTX      equ 0x0000
CMD_RING    equ 0x0900
EVT_RING    equ 0x0A00
EP0_RING    equ 0x0B00
BIN_RING    equ 0x0C00
BOUT_RING   equ 0x0D00
ERST        equ 0x0E00
DCBAA       equ 0x0E40                ; 4 slots
KBD_BUF     equ 0x0E80
MOU_BUF     equ 0x0EC0
OUT_CTX     equ 0x1000
CBW_BUF         equ 0x1800
CSW_BUF         equ 0x1840
DESC        equ 0x1880
EP0_RING1   equ 0x1A00
EP0_RING2   equ 0x1B00
KINT_RING   equ 0x1C00
MINT_RING   equ 0x1D00
DATA        equ 0x2000
OUT_CTX1    equ 0x3000
OUT_CTX2    equ 0x3800
MEM_SIZE    equ 0x4000
MAX_SLOTS   equ 4
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
ep0_idx     db 0
ep0_cyc     db 1
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
cur_ep0     dw EP0_RING
cur_st      dw ep0_idx
; estado de EP0 de los otros dispositivos
ep0_1       db 0, 1
ep0_2       db 0, 1
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
        mov byte [cs:busy], 1
        call unreal
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
        mov byte [cs:busy], 0
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
        jc .err
        jmp .adv
.rd:
        mov al, 0x28
        call scsi_rw
        jc .err
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
        jmp .done
.err:
        mov ax, [bx+0x12]
        sub ax, [cs:rw_left]
        mov [bx+0x12], ax
        mov word [bx+3], 0x810C     ; error general
        jmp .out

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

; la memoria ya no está: liberar su slot
drop_disk:
        xor eax, eax
        xor edx, edx
        xor ecx, ecx
        movzx ebp, byte [slot]
        shl ebp, 24
        or ebp, 10 << 10            ; Disable Slot
        call xhci_cmd
        movzx di, byte [slot]
        shl di, 3
        mov bx, DCBAA
        call zone
        add di, bx
        mov dword [cs:di], 0
        mov byte [gone], 1
        mov byte [disk_found], 0
        ret

; buscar una memoria en los puertos que acaban de conectarse (CSC)
rescan:
        mov al, [opt_verbose]
        push ax
        mov byte [opt_verbose], 0   ; sin mensajes: estamos dentro de DOS
        mov byte [only_disk], 1
        xor cl, cl
.pl:
        inc cl
        cmp cl, [maxports]
        ja .r
        cmp byte [gone], 0
        je .r
        movzx esi, cl
        call port_sc
        test eax, 1 << 17
        jz .pl
        push cx
        call clr_csc
        call mrd
        test al, 1
        jz .nx
        call port_enable
        jc .nx
        mov al, [dpool]
        mov [npool], al
        mov word [bin_idx], 0x0100  ; índice 0, ciclo 1
        mov word [bout_idx], 0x0100
        mov bx, BIN_RING
        mov cx, 0x100
        call zero_zone
        mov bx, BOUT_RING
        mov cx, 0x100
        call zero_zone
        call enum_one
        cmp byte [disk_found], 0
        je .nx
        call disk_setup
        jc .bad
        mov byte [gone], 0
        jmp .nx
.bad:
        call drop_disk
.nx:
        pop cx
        jmp .pl
.r:
        mov byte [only_disk], 0
        pop ax
        mov [opt_verbose], al
        ret

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
        mov cx, 40                  ; ~2 s
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
        mov al, 32
        call wait_event
        jc .r
        mov al, [cs:evt+11]
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
        pop si
        pop bx
        ret
ctl_setup   dd 0, 0

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
        call wait_xfer
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
        bswap eax
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
        cli
        lgdt [cs:sv_gdtr]
.out:
        pop fs
        pop es
        pop ds
        popad
        mov byte [cs:busy], 0
.ret:
        iret

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
        cmp byte [units], 0
        je .fail
        jmp .resp
.hok:
        cmp byte [k_on], 0
        je .h1
        mov si, msg_kbd
        call puts
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
        lds bx, [cs:req]
        mov al, [cs:units]
        mov [bx+0x0D], al
        mov ax, [cs:mem_off]
        add ax, MEM_SIZE
        mov [bx+0x0E], ax
        mov [bx+0x10], cs
        mov word [bx+0x12], bpbarr
        mov [bx+0x14], cs
        jmp .done
.fail:
        push cs
        pop ds
        mov si, [fail_msg]
        call puts
        lds bx, [cs:req]
        mov byte [bx+0x0D], 0
        mov word [bx+0x0E], 0
        mov [bx+0x10], cs
.done:
        lds bx, [cs:req]
        mov word [bx+3], 0x0100
        mov byte [cs:busy], 0
        pop es
        pop ds
        popad
        retf

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
        and ecx, 0xFFFFFFF0
        mov [xbase], ecx
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
        ; ¿la BIOS usa el controlador? (USB Legacy Support)
        call legacy_handoff
        jc .r
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

; ceder la propiedad del controlador desde la BIOS (si la tiene)
legacy_handoff:
        mov eax, [hcc1]
        shr eax, 16
        jz .ok
        shl eax, 2
        add eax, [xbase]
        mov esi, eax
.cap:
        call mrd
        cmp al, 1
        je .leg
        shr eax, 8
        and eax, 0xFF
        jz .ok
        shl eax, 2
        add esi, eax
        jmp .cap
.leg:
        test eax, 0x10000           ; propiedad de la BIOS
        jz .ok
        cmp byte [opt_force], 0
        jne .take
        mov word [fail_msg], msg_bios
        stc
        ret
.take:
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
        clc
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
        mov byte [npool], 0
        xor cl, cl
.pl:
        inc cl
        cmp cl, [maxports]
        ja .end
        cmp byte [npool], 3
        jae .end
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
        push cx
        call enum_one
        pop cx
        jmp .pl
.end:
        mov al, [disk_found]
        or al, [k_on]
        or al, [m_on]
        jz .none
        clc
        ret
.none:
        stc
        ret

; enumerar el dispositivo de [port] con la entrada [npool] de la reserva
enum_one:
        movzx bx, byte [npool]
        shl bx, 1
        mov ax, [pool_ep0+bx]
        mov [cur_ep0], ax
        mov ax, [pool_st+bx]
        mov [cur_st], ax
        mov ax, [pool_out+bx]
        mov [cur_out], ax
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
        movzx eax, byte [speed]
        shl eax, 20
        or eax, 1 << 27                         ; una entrada de contexto
        mov [cs:bx], eax
        movzx eax, byte [port]
        shl eax, 16
        mov [cs:bx+4], eax
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
        jc .dis
        ; descriptor de dispositivo: primero 8 bytes (tamaño de paquete de EP0)
        mov dword [ctl_setup], 0x01000680
        mov dword [ctl_setup+4], 0x00080000
        call control
        mov word [fail_msg], msg_desc
        jc .dis
        cmp byte [speed], 4
        jae .full
        mov bx, DESC
        call zone
        movzx ax, byte [cs:bx+7]
        cmp ax, 8
        jb .full
        cmp ax, [mps0]
        je .full
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
.full:
        mov dword [ctl_setup], 0x01000680
        mov dword [ctl_setup+4], 0x00120000
        call control
        jc .dis
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
        ; ¿qué papel tiene?
        cmp byte [p_msc], 0
        je .hid
        cmp byte [disk_found], 0
        jne .hid
        call role_disk
        jc .dis
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
        jc .dis
.used:
        inc byte [npool]
        ret
.dis:
        ; no se usa: liberar el slot
        xor eax, eax
        xor edx, edx
        xor ecx, ecx
        movzx ebp, byte [cur_slot]
        shl ebp, 24
        or ebp, 10 << 10
        call xhci_cmd
        movzx di, byte [cur_slot]
        shl di, 3
        mov bx, DCBAA
        call zone
        add di, bx
        mov dword [cs:di], 0
.err:
        ret

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
        shl eax, 27
        movzx edx, byte [speed]
        shl edx, 20
        or eax, edx
        mov [cs:bx], eax
        movzx eax, byte [port]
        shl eax, 16
        mov [cs:bx+4], eax
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
        mov byte [disk_found], 1
        mov al, [port]
        mov [dport], al
        mov al, [npool]
        mov [dpool], al
        call dport_sc
        call clr_csc                ; la conexión de ahora ya está vista
        clc
.err:
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
        shl eax, 27
        movzx edx, byte [speed]
        shl edx, 20
        or eax, edx
        mov [cs:bx], eax
        movzx eax, byte [port]
        shl eax, 16
        mov [cs:bx+4], eax
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

npool       db 0
want_k      db 0
want_m      db 0
disk_found  db 0
            db 0
mps0        dw 8
cur_out     dw OUT_CTX
pool_ep0    dw EP0_RING, EP0_RING1, EP0_RING2
pool_st     dw ep0_idx, ep0_1, ep0_2
pool_out    dw OUT_CTX, OUT_CTX1, OUT_CTX2
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
        mov word [fail_msg], msg_scsi
        ; TEST UNIT READY (hasta 5 intentos)
        mov cx, 5
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
        bswap eax
        inc eax
        shr eax, 11
        mov [cap_mb], eax
        mov eax, [cs:bx+4]
        bswap eax
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
        mov bx, DATA
        call zone
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
msg_nox     db "No se ha encontrado ningún controlador xHCI (USB 3).", 13, 10, 0
msg_v86     db "No funciona con EMM386 (modo V86).", 13, 10, 0
msg_high    db "Los registros del xHCI están por encima de 4 GB: no se admite.", 13, 10, 0
msg_bios    db "La BIOS está usando el controlador USB (teclado USB o unidad de", 13, 10
            db "arranque). Use /F para tomarlo igualmente.", 13, 10, 0
msg_reset   db "El controlador xHCI no se reinicia.", 13, 10, 0
msg_nodev   db "No hay ninguna memoria, teclado ni ratón USB conectado.", 13, 10, 0
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
