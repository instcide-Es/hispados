; =============================================================================
;  macsim.inc - [CIRRUS] el códec Cirrus Logic CS4206 del MacBook Air 13" 2012
;  (MacBookAir5,2), simulado a partir de su volcado real (informe SONIDO.TXT
;  de Carlos). Para las pruebas en QEMU, que no tiene ese códec: las órdenes
;  al códec 0 las contesta esta tabla (y guarda lo que se escribe); el
;  formato y el número de flujo de sus DAC se pasan también al DAC 02h del
;  códec de QEMU, para que el flujo suene y la posición avance de verdad.
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Lo usan AUDICION /SIM:MAC y HSPSND /SIMMAC (solo para las pruebas). Quien
;  lo incluye pone mac_fwd_cmd (EAX = orden para el DAC 02h del códec real;
;  puede no hacer nada) y el espacio de MACSIM_VARS (en .bss o con MACSIM_DATA).
; =============================================================================
%macro MACSIM_VARS 0
mac_st      resb MAC_ROWS * MR_SZ
mac_coef    resw MAC_NCOEF
mac_gm      resb 1
mac_gd      resb 1
mac_gv      resb 1
mac_proc    resb 1
mac_fwd     resb 1
mac_afgp    resw 1
mac_cidx    resw 1
%endmacro
%macro MACSIM_DATA 0
mac_st      times MAC_ROWS * MR_SZ db 0
mac_coef    times MAC_NCOEF dw 0
mac_gm      db 0
mac_gd      db 0
mac_gv      db 0
mac_proc    db 0
mac_fwd     db 0
mac_afgp    dw 0
mac_cidx    dw 0
%endmacro
MAC_N       equ 0x16                ; nodos 00h-15h
; una fila por nodo 02h-15h: caps, caps del pin, amp. de entrada, amp. de
; salida, PCM, formatos, configuración por defecto, conexiones (n, 2 nodos),
; energía, formato, control del pin, amp. de salida, amp. de entrada, digital
MR_CAPS     equ 0
MR_PINC     equ 4
MR_IAMP     equ 8
MR_OAMP     equ 12
MR_PCM      equ 16
MR_FMTS     equ 20
MR_CFG      equ 24
MR_NC       equ 28
MR_C0       equ 29
MR_C1       equ 30
MR_PWR      equ 31                  ; (palabra)
MR_FMT      equ 33                  ; (palabra)
MR_CTL      equ 35
MR_AO       equ 36
MR_AI       equ 37
MR_DIG      equ 38                  ; (palabra)
MR_STR      equ 40                  ; flujo y canal (F06h/706h)
MR_SZ       equ 42
%macro MROW 15
        dd %1, %2, %3, %4, %5, %6, %7
        db %8, %9, %10
        dw %11, %12
        db %13, %14, %15
        dw 0
        db 0, 0
%endmacro
mac_rows:
        ;     caps      pin       amp-ent   amp-sal   PCM       formatos  config    n  c0    c1    energía formato ctl ampS ampE
        MROW 0x000D041D, 0, 0, 0x80017F73, 0x001E07F0, 3, 0, 0, 0, 0, 0x433, 0, 0, 0xF3, 0          ; 02 DAC
        MROW 0x000D041D, 0, 0, 0x80017F73, 0x001E07F0, 3, 0, 0, 0, 0, 0x433, 0, 0, 0xF3, 0          ; 03 DAC
        MROW 0x000D041D, 0, 0, 0x80017F73, 0x001E07F0, 3, 0, 0, 0, 0, 0x033, 0x4011, 0, 0x80, 0     ; 04 DAC
        MROW 0x0018051B, 0, 0x80033F33, 0, 0x001E01F5, 3, 0, 2, 0x0C, 0x12, 0x433, 0, 0, 0, 0xB3 ; 05 ADC
        MROW 0x0018051B, 0, 0x80033F33, 0, 0x001E01F5, 3, 0, 2, 0x0D, 0x0E, 0x433, 0, 0, 0, 0xB3 ; 06 ADC
        MROW 0x00180791, 0, 0, 0, 0x001E0570, 7, 0, 1, 0x0F, 0, 0x433, 0, 0, 0, 0                   ; 07 ADC digital
        MROW 0x00040611, 0, 0, 0, 0x001E07F0, 7, 0, 0, 0, 0, 0x433, 0, 0, 0, 0                      ; 08 DAC digital
        MROW 0x00410581, 0x1C, 0, 0, 0, 0, 0x012B4030, 1, 0x02, 0, 0x033, 0, 0, 0, 0                ; 09 auriculares
        MROW 0x00410581, 0x54, 0, 0, 0, 0, 0x400000F0, 1, 0x03, 0, 0x033, 0, 0, 0, 0                ; 0A sin conexión
        MROW 0x00410101, 0x50, 0, 0, 0, 0, 0x90100120, 1, 0x04, 0, 0, 0, 0x40, 0, 0                 ; 0B altavoz
        MROW 0x0041048B, 0x24, 0x00270300, 0, 0, 0, 0x400000F0, 0, 0, 0, 0x033, 0, 0, 0, 0          ; 0C
        MROW 0x0041048B, 0x1764, 0x00270300, 0, 0, 0, 0x90A00110, 0, 0, 0, 0x033, 0, 0, 0, 0        ; 0D micrófono
        MROW 0x0041000B, 0x20, 0x00270200, 0, 0, 0, 0x400000F0, 0, 0, 0, 0, 0, 0, 0, 0              ; 0E
        MROW 0x00410681, 0x24, 0, 0, 0, 0, 0x400000F0, 0, 0, 0, 0x033, 0, 0, 0, 0                   ; 0F
        MROW 0x00410301, 0x10, 0, 0, 0, 0, 0x400000F0, 1, 0x08, 0, 0, 0, 0, 0, 0                    ; 10
        MROW 0x00F00040, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0                                   ; 11 proceso
        MROW 0x0041000B, 0x20, 0x00270200, 0, 0, 0, 0x400000F0, 0, 0, 0, 0, 0, 0, 0, 0              ; 12
        MROW 0x00700000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0                                   ; 13 pitido
        MROW 0x00040611, 0, 0, 0, 0x001E07F0, 7, 0, 0, 0, 0, 0x433, 0, 0, 0, 0                         ; 14 DAC digital
        MROW 0x00410301, 0x10, 0, 0, 0, 0, 0x400000F0, 1, 0x14, 0, 0, 0, 0, 0, 0                    ; 15
MAC_ROWS    equ ($ - mac_rows) / MR_SZ
; los coeficientes del widget de proceso (11h), como los dejó el firmware
mac_coef0   dw 0x001E, 0x0008, 0x000A, 0x106A, 0x0007
MAC_NCOEF   equ 32

; poner el códec simulado como lo dejó el firmware de Apple
mac_reset:
        pusha
        push es
        push ds
        pop es
        mov si, mac_rows
        mov di, mac_st
        mov cx, MAC_ROWS * MR_SZ
        rep movsb
        mov di, mac_coef
        mov cx, MAC_NCOEF
        xor ax, ax
        rep stosw
        mov si, mac_coef0
        mov di, mac_coef
        mov cx, 5
        rep movsw
        mov byte [mac_gm], 0x08
        mov byte [mac_gd], 0x08
        mov byte [mac_gv], 0x00
        mov word [mac_afgp], 0x633
        mov byte [mac_proc], 0
        mov word [mac_cidx], 0
        mov byte [mac_fwd], 0
        pop es
        popa
        ret

; EAX = orden completa al códec 0 -> EAX = respuesta (nunca falla)
mac_cmd:
        push ebx
        push ecx
        push edx
        push esi
        push edi
        mov edx, eax
        shr edx, 20
        and edx, 0x7F               ; DL = nodo
        mov ecx, eax                ; ECX = la orden
        ; DI -> fila del nodo (02h-15h), o 0
        xor di, di
        cmp dl, 2
        jb .nr
        cmp dl, MAC_N - 1
        ja .nr
        movzx di, dl
        sub di, 2
        imul di, di, MR_SZ
        add di, mac_st
.nr:
        mov eax, ecx
        shr eax, 8
        and ax, 0xFFF               ; verbo de 12 bits
        mov bx, ax
        and bh, 0x0F
        cmp bh, 0x07
        je .v12
        cmp bh, 0x0F
        je .v12
        ; verbos de 4 bits (dato de 16)
        mov eax, ecx
        shr eax, 16
        and al, 0x0F
        cmp al, 0x02
        je .sfmt
        cmp al, 0x0A
        je .gfmt
        cmp al, 0x03
        je .samp
        cmp al, 0x0B
        je .gamp
        cmp al, 0x05
        je .sidx
        cmp al, 0x0D
        je .gidx
        cmp al, 0x04
        je .scoef
        cmp al, 0x0C
        je .gcoef
        jmp .z
.v12:
        cmp ax, 0xF00
        je .param
        cmp ax, 0xF20
        je .ssid
        cmp ax, 0xF02
        je .conn
        cmp ax, 0xF1C
        je .cfg
        cmp ax, 0xF09
        je .z                       ; nada enchufado
        cmp ax, 0xF15
        je .ggv
        cmp ax, 0xF16
        je .ggm
        cmp ax, 0xF17
        je .ggd
        cmp ax, 0x715
        je .sgv
        cmp ax, 0x716
        je .sgm
        cmp ax, 0x717
        je .sgd
        cmp ax, 0xF03
        je .gproc
        cmp ax, 0x703
        je .sproc
        cmp ax, 0xF05
        je .gpwr
        cmp ax, 0x705
        je .spwr
        test di, di
        jz .z
        cmp ax, 0xF07
        je .gctl
        cmp ax, 0x707
        je .sctl
        cmp ax, 0xF06
        je .gstr
        cmp ax, 0x706
        je .sstr
        cmp ax, 0xF0D
        je .gdig
        cmp ax, 0x70D
        je .sdig
        jmp .z                      ; lo demás: se acepta y contesta 0
; ---- parámetros ----
.param:
        movzx bx, cl                ; BL = parámetro
        test dl, dl
        jnz .p1
        mov eax, 0x10134206
        cmp bl, 0x00
        je .r
        mov eax, 0x00100302
        cmp bl, 0x02
        je .r
        mov eax, 0x00010001
        cmp bl, 0x04
        je .r
        jmp .z
.p1:
        cmp dl, 1
        jne .pw
        mov eax, 0x00020014
        cmp bl, 0x04
        je .r
        mov eax, 0x00000001
        cmp bl, 0x05
        je .r
        mov eax, 0x0001090E
        cmp bl, 0x08
        je .r
        mov eax, 0x001E07F0
        cmp bl, 0x0A
        je .r
        mov eax, 3
        cmp bl, 0x0B
        je .r
        mov eax, 0xC0000009
        cmp bl, 0x0F
        je .r
        mov eax, 4
        cmp bl, 0x11
        je .r
        jmp .z
.pw:
        test di, di
        jz .z
        mov eax, [di+MR_CAPS]
        cmp bl, 0x09
        je .r
        mov eax, [di+MR_PINC]
        cmp bl, 0x0C
        je .r
        mov eax, [di+MR_IAMP]
        cmp bl, 0x0D
        je .r
        mov eax, [di+MR_OAMP]
        cmp bl, 0x12
        je .r
        mov eax, [di+MR_PCM]
        cmp bl, 0x0A
        je .r
        mov eax, [di+MR_FMTS]
        cmp bl, 0x0B
        je .r
        movzx eax, byte [di+MR_NC]
        cmp bl, 0x0E
        je .r
        mov eax, 0x00001600         ; proceso: 22 coeficientes
        cmp bl, 0x10
        jne .pw2
        cmp dl, 0x11
        je .r
.pw2:
        mov eax, 0xC0000009
        cmp bl, 0x0F
        jne .z
        test byte [di+MR_CAPS+1], 0x04
        jnz .r
        jmp .z
.ssid:
        mov eax, 0x106B5600
        cmp dl, 1
        je .r
        jmp .z
.conn:
        test di, di
        jz .z
        test cl, cl
        jnz .z
        movzx eax, byte [di+MR_C1]
        shl eax, 8
        mov al, [di+MR_C0]
        jmp .r
.cfg:
        test di, di
        jz .z
        mov eax, [di+MR_CFG]
        jmp .r
; ---- GPIO del grupo de audio ----
.ggv:
        movzx eax, byte [mac_gv]
        jmp .r
.ggm:
        movzx eax, byte [mac_gm]
        jmp .r
.ggd:
        movzx eax, byte [mac_gd]
        jmp .r
.sgv:
        mov [mac_gv], cl
        jmp .z
.sgm:
        mov [mac_gm], cl
        jmp .z
.sgd:
        mov [mac_gd], cl
        jmp .z
; ---- widget de proceso ----
.gproc:
        cmp dl, 0x11
        jne .z
        movzx eax, byte [mac_proc]
        jmp .r
.sproc:
        cmp dl, 0x11
        jne .z
        mov [mac_proc], cl
        jmp .z
.sidx:
        cmp dl, 0x11
        jne .z
        mov [mac_cidx], cx
        jmp .z
.gidx:
        cmp dl, 0x11
        jne .z
        movzx eax, word [mac_cidx]
        jmp .r
.scoef:
        cmp dl, 0x11
        jne .z
        movzx bx, byte [mac_cidx]
        cmp bx, MAC_NCOEF
        jae .z
        shl bx, 1
        mov [mac_coef+bx], cx
        jmp .z
.gcoef:
        cmp dl, 0x11
        jne .z
        movzx bx, byte [mac_cidx]
        cmp bx, MAC_NCOEF
        jae .z
        shl bx, 1
        movzx eax, word [mac_coef+bx]
        jmp .r
; ---- energía ----
.gpwr:
        cmp dl, 1
        jne .gp2
        movzx eax, word [mac_afgp]
        jmp .r
.gp2:
        test di, di
        jz .z
        movzx eax, word [di+MR_PWR]
        jmp .r
.spwr:
        and cl, 0x0F
        mov al, cl
        shl al, 4
        or al, cl
        movzx ax, al
        cmp dl, 1
        jne .sp2
        mov [mac_afgp], ax
        jmp .z
.sp2:
        test di, di
        jz .z
        mov [di+MR_PWR], ax
        jmp .z
; ---- control del pin, flujo, digital ----
.gctl:
        movzx eax, byte [di+MR_CTL]
        jmp .r
.sctl:
        mov [di+MR_CTL], cl
        jmp .z
.gstr:
        movzx eax, byte [di+MR_STR]
        jmp .r
.sstr:
        mov [di+MR_STR], cl
        call .dac
        jnz .z
        mov eax, ecx
        jmp .fwd
.gdig:
        movzx eax, word [di+MR_DIG]
        jmp .r
.sdig:
        mov [di+MR_DIG], cl
        jmp .z
; ---- formato del convertidor ----
.sfmt:
        test di, di
        jz .z
        mov [di+MR_FMT], cx
        call .dac
        jnz .z
        mov eax, ecx
        jmp .fwd
.gfmt:
        test di, di
        jz .z
        movzx eax, word [di+MR_FMT]
        jmp .r
; ---- amplificadores (salida: un valor por lado; entrada: el mismo) ----
.samp:
        test di, di
        jz .z
        test ch, 0x80
        jz .sai
        mov [di+MR_AO], cl
        jmp .z
.sai:
        mov [di+MR_AI], cl
        jmp .z
.gamp:
        test di, di
        jz .z
        movzx eax, byte [di+MR_AO]
        test ch, 0x80
        jnz .r
        movzx eax, byte [di+MR_AI]
        jmp .r
; ---- a QEMU: la misma orden, a su DAC 02h (la primera vez, sin silencio) ----
.fwd:
        and eax, 0x000FFFFF
        or eax, 0x00200000
        push eax
        cmp byte [mac_fwd], 0
        jne .f1
        mov byte [mac_fwd], 1
        mov eax, 0x0023B04A         ; amplificador de salida a 0 dB
        call .send
.f1:
        pop eax
        call .send
.z:
        xor eax, eax
.r:
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret
.send:
        jmp mac_fwd_cmd             ; (lo pone quien incluye esto)
; ZF = 1 si el nodo DL es un DAC analógico (los que van a QEMU)
.dac:
        cmp dl, 2
        jb .dn
        cmp dl, 4
        ja .dn
        cmp dl, dl
        ret
.dn:
        push ax
        mov al, 1
        or al, al
        pop ax
        ret
