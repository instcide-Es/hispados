; =============================================================================
;  Hispa-DOS - HSPUSB: controladora OHCI (USB 1.x de SiS, ALi, NEC, Apple,
;  AMD, NVIDIA...), parte residente
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Según «OpenHCI: Open Host Controller Interface Specification for USB» 1.0a.
;  Registros en memoria (hc_base): HcControl 04h, HcCommandStatus 08h,
;  HcInterruptStatus 0Ch, HcHCCA 18h, HcControlHeadED 20h, HcBulkHeadED 28h,
;  HcFmInterval 34h, HcFmNumber 3Ch, HcPeriodicStart 40h, HcRhDescriptorA
;  48h, HcRhStatus 50h, HcRhPortStatus 54h + 4·n.
;
;  Memoria de cada controladora (hc_mem): la HCCA (256 bytes, con las 32
;  entradas de la tabla de interrupción apuntando todas a la cadena de
;  tuberías: un informe por trama), el ED de control (+100h) y el ED de bulk
;  (+110h), que son los únicos de sus listas. Como control y bulk son
;  síncronos, esos dos ED se rellenan en cada transferencia con la dirección,
;  el endpoint y el paquete máximo del aparato que toque; mientras no
;  trabajan están con sKip. Un TD lleva una transferencia entera (hasta 4 KB:
;  dos páginas), no un paquete: control son tres TD, bulk uno.
;  Cada tubería de interrupción tiene su ED y dos TD que se turnan (uno
;  lleva el informe y el otro hace de cola vacía).
;  Sin interrupciones: todo se consulta (se mira si la cabeza del ED ha
;  alcanzado la cola).
; =============================================================================

o_edc       dd 0                    ; primera palabra del ED de la transferencia
o_fill      dd 0                    ; aviso de lista con trabajo: 2 control, 4 bulk
o_try       db 0

; primera palabra del ED con los datos de xf_* -> o_edc
o_edword:
        movzx eax, byte [xf_addr]
        movzx edx, byte [xf_ep]
        shl edx, 7
        or eax, edx
        cmp byte [xf_speed], 2
        jne .fs
        or eax, 1 << 13                 ; velocidad baja
.fs:
        movzx edx, word [xf_mps]
        shl edx, 16
        or eax, edx
        mov [o_edc], eax
        ret

; preparar el TD AL de la reserva: AH = sentido (0 SETUP, 1 salida, 2
; entrada), DL = testigo (2 DATA0, 3 DATA1), EBX = búfer (dirección
; física), CX = bytes. Queda enlazado con el siguiente.
o_td:
        push di
        push edx
        push esi
        movzx di, al
        shl di, 4
        add di, [cl_xtd]
        movzx esi, ah
        shl esi, 19
        movzx edx, dl
        shl edx, 24
        or esi, edx
        or esi, (1 << 18) | (7 << 21) | (0xF << 28)   ; vale corto, sin interrupción, sin usar
        mov [di], esi
        xor edx, edx
        xor esi, esi
        jcxz .z
        mov edx, ebx
        movzx esi, cx
        lea esi, [ebx+esi-1]
.z:
        mov [di+4], edx                 ; búfer
        mov [di+12], esi                ; su último byte
        push bx
        lea bx, [di+16]
        call phys
        pop bx
        mov [di+8], eax
        pop esi
        pop edx
        pop di
        ret

; ejecutar los xr_n TD de la reserva por el ED SI -> CF si falla
o_run:
        push si
        mov esi, 0x0C
        mov eax, 2                      ; (borrar el aviso de cola de hechos)
        call hc_wr
        pop si
        mov eax, [o_edc]
        or eax, 1 << 14
        mov [si], eax                   ; parado mientras se prepara
        mov bx, [cl_xtd]
        call phys
        mov [si+8], eax                 ; cabeza
        movzx ebx, byte [xr_n]
        shl ebx, 4
        add eax, ebx
        mov [si+4], eax                 ; cola: el TD que sigue al último
        mov [xr_p0], eax
        mov eax, [o_edc]
        mov [si], eax                   ; adelante
        push si
        mov eax, [o_fill]
        mov esi, 0x08
        call hc_wr
        pop si
        mov cx, [xr_tmo]
        call tmo_start
.w:
        sti
        mov eax, [si+8]
        test al, 1
        jnz .halt
        and eax, 0xFFFFFFF0
        cmp eax, [xr_p0]
        je .ok
        call tmo_check
        jnc .w
        or dword [si], 1 << 14
        call o_frame2                   ; que la controladora lo suelte
        stc
        ret
.halt:
        or dword [si], 1 << 14
        stc
        ret
.ok:
        or dword [si], 1 << 14
        clc
        ret

; esperar a que pasen dos tramas
o_frame2:
        push eax
        push ebx
        push ecx
        push esi
        mov bh, 2
        mov ecx, 400000
.n:
        mov esi, 0x3C
        call hc_rd
        mov bl, al
.l:
        dec ecx
        jz .r
        call mrd
        cmp al, bl
        je .l
        dec bh
        jnz .n
.r:
        pop esi
        pop ecx
        pop ebx
        pop eax
        ret

o_control:
        call o_edword
        mov dword [o_fill], 2
        mov bx, ctl_setup
        call phys
        mov ebx, eax
        mov cx, 8
        xor ax, ax                      ; TD 0: SETUP
        mov dl, 2
        call o_td
        mov byte [xr_n], 1
        mov cx, [ctl_setup+6]
        jcxz .st
        mov bx, [ctl_buf]
        call zone
        call phys
        mov ebx, eax
        mov cx, [ctl_setup+6]
        mov ax, 0x0201                  ; TD 1: datos de entrada
        mov dl, 3
        call o_td
        mov byte [xr_n], 2
        mov ah, 1                       ; estado: salida
        jmp .s2
.st:
        mov ah, 2                       ; sin datos: estado de entrada
.s2:
        mov al, [xr_n]
        xor cx, cx
        mov dl, 3
        call o_td
        inc byte [xr_n]
        mov si, [cur_hmem]
        add si, 0x100
        call o_run
        ret

o_bulk:
        call o_edword
        mov dword [o_fill], 4
        mov ebx, [xf_buf]
        mov cx, [xf_len]
        mov ah, 1
        cmp byte [bk_dir], 0
        je .o
        mov ah, 2
.o:
        xor al, al
        mov dl, [xr_tog]
        or dl, 2
        call o_td
        mov byte [xr_n], 1
        mov si, [cur_hmem]
        add si, 0x110
        call o_run
        jc .r
        mov eax, [si+8]                 ; el testigo siguiente lo lleva el ED
        shr al, 1
        and al, 1
        mov [xr_tog], al
        clc
.r:
        ret

; ---- tuberías de interrupción ----
section .init                   ; (solo al arrancar)
o_pipe_open:
        call c_pipe_blk
        call o_edword
        mov eax, [o_edc]
        or eax, (2 << 11) | (1 << 14)   ; de entrada; parado hasta que esté listo
        mov [di], eax
        push bx
        lea bx, [di+0x60]
        call phys
        pop bx
        mov [di+4], eax                 ; cola = cabeza: vacío
        mov [di+8], eax
        movzx si, byte [cur_hc]
        shl si, 2
        mov eax, [hc_per+si]
        mov [di+12], eax
        mov bx, di
        call phys
        mov [hc_per+si], eax
        push di
        push es
        push cs
        pop es
        mov di, [cur_hmem]              ; las 32 entradas de la HCCA
        mov cx, 32
        cld
        rep stosd
        pop es
        pop di
        and dword [di], ~(1 << 14)
        ret

section .text
o_pipe_queue:
        call c_pipe_blk
        movzx si, byte [ip_cur+bx]      ; el TD de cola pasa a llevar el informe
        shl si, 4
        add si, di
        add si, 0x60
        mov dword [si], (1 << 18) | (2 << 19) | (7 << 21) | (0xF << 28)
        call c_pipe_buf
        mov [si+4], eax
        movzx edx, word [xf_len]
        lea edx, [eax+edx-1]
        mov [si+12], edx
        xor byte [ip_cur+bx], 1
        movzx bx, byte [ip_cur+bx]      ; y el otro, de cola
        shl bx, 4
        add bx, di
        add bx, 0x60
        call phys
        mov [si+8], eax
        and dword [di+8], ~1            ; por si quedó parado tras un error
        mov [di+4], eax
        ret

; -> CF si el informe no ha llegado; si no, AX = bytes (FFFFh = error)
o_pipe_check:
        call c_pipe_blk
        mov eax, [di+8]
        and eax, 0xFFFFFFF0
        cmp eax, [di+4]
        je .done
        stc
        ret
.done:
        movzx si, byte [ip_cur+bx]
        xor si, 1
        shl si, 4
        add si, di
        add si, 0x60
        mov eax, [si]
        shr eax, 28
        jnz .err
        mov si, bx
        shl si, 1
        mov ax, [ip_len+si]
        push bx
        movzx bx, byte [ip_cur+bx]
        xor bx, 1
        shl bx, 4
        add bx, di
        mov edx, [bx+0x60+4]
        test edx, edx
        jz .full
        mov ecx, [bx+0x60+12]
        sub ecx, edx
        inc cx
        sub ax, cx
.full:
        pop bx
        clc
        ret
.err:
        mov ax, 0xFFFF
        clc
        ret

; ---- puertos raíz ----
o_psc:
        movzx esi, cl
        dec esi
        shl esi, 2
        add esi, 0x54
        ret
o_rp_get:
        push esi
        push edx
        call o_psc
        call hc_rd
        mov edx, eax
        and al, 3
        test edx, 1 << 16
        jz .a
        or al, 0x80
.a:
        pop edx
        pop esi
        ret
o_rp_ack:
        push eax
        push esi
        call o_psc
        mov eax, 0x001F0000             ; borrar los cambios
        call hc_wr
        pop esi
        pop eax
        ret
o_rp_reset:
        call o_psc
        call hc_rd
        test al, 1
        jz .no
        mov byte [o_try], 3             ; tres reinicios de 10 ms seguidos
.again:
        mov eax, 1 << 4
        call mwr
        mov cx, 3
        call tmo_start
.w:
        sti
        call mrd
        test eax, 1 << 20
        jnz .d
        call tmo_check
        jnc .w
.d:
        mov eax, 1 << 20
        call mwr
        dec byte [o_try]
        jnz .again
        call reset_rest
        call mrd
        test al, 1
        jz .no
        test al, 2
        jnz .en
        mov eax, 2                      ; habilitar
        call mwr
        call mrd
        test al, 2
        jz .no
.en:
        mov bl, 1
        test ah, 2                      ; bit 9: velocidad baja
        jz .sp
        mov bl, 2
.sp:
        mov [speed], bl
        mov eax, 0x001F0000
        call mwr
        clc
        ret
.no:
        stc
        ret

o_time:
        push esi
        mov esi, 0x3C
        call hc_rd
        shl ax, 3
        and ax, 0x3FFF
        pop esi
        ret
