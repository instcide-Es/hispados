; =============================================================================
;  Hispa-DOS - mandos USB: del informe al estado del mando (parte residente)
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Lo incluyen HSPUSB.SYS (residente) y las pruebas (tests/progs/hidtest.asm).
;  El «mapa» (HJM_*) lo prepara una vez lib/hidjoyp.inc a partir del
;  descriptor de informe HID (HID 1.11, 6.2.2) o, para los mandos tipo
;  Xbox 360, de su formato fijo. Con el mapa, cada informe que llega se pasa
;  al «estado» (HJS_*), que es lo que se entrega a HSPJOY (docs/MANDOS.md).
;  Todo se direcciona por DS (DS = segmento de este código y sus datos).
; =============================================================================

; ---- estado de un mando (lo que ve HSPJOY; docs/MANDOS.md) ----
HJS_TIPO    equ 0                   ; 0 libre, 1 USB HID, 2 USB tipo Xbox 360...
HJS_FLAGS   equ 1                   ; bit 0: presente
HJS_NAX     equ 2                   ; ejes que tiene (0-6)
HJS_NBTN    equ 3                   ; botones que tiene (0-16)
HJS_VID     equ 4                   ; USB: fabricante y producto
HJS_PID     equ 6
HJS_EJES    equ 8                   ; X, Y, X2, Y2, gatillo izq., gatillo der.
                                    ; (0-255; 128 = centro; gatillos: 0 = suelto)
HJS_CRUZ    equ 14                  ; cruceta: bit 0 arriba, 1 abajo, 2 izq., 3 der.
HJS_BTN     equ 16                  ; botones 1-16 (bit 0 = botón 1)
HJS_SZ      equ 18

; ---- datos de HSPJOY (INT 2Fh AX=D900h -> ES:DI; docs/MANDOS.md) ----
HJD_FIRMA   equ 0x00                ; "HJ"
HJD_VER     equ 0x02                ; versión de la estructura (1)
HJD_N       equ 0x03                ; mandos de la tabla
HJD_TAM     equ 0x04                ; bytes de cada uno
HJD_TABLA   equ 0x06                ; dw: la tabla de mandos
HJD_CBOT    equ 0x08                ; mando clásico: botones (bits 4-7, 0 = pulsado) y
                                    ;   ejes que hay (bits 0-3)
HJD_CEJES   equ 0x09                ; X1 Y1 X2 Y2 (0-255)
HJD_J1E     equ 0x0D                ; mando que hace de jugador 1 (FFh ninguno)
HJD_J2E     equ 0x0E
HJD_J1      equ 0x10                ; jugador 1 pedido (FFh automático)
HJD_J2      equ 0x11                ; jugador 2 pedido (FFh automático, FEh ninguno)
HJD_OPC     equ 0x12                ; bit 0 sondear en cada tic, 1 y 2 teclas de los jugadores
HJD_LPTT    equ 0x13                ; tipo de mando del cable de impresora
HJD_LPT     equ 0x14                ; dw puerto de impresora
HJD_GPORT   equ 0x16                ; dw puerto de juegos
HJD_GTMO    equ 0x18                ; dw espera máxima (vueltas)
HJD_CAL     equ 0x1A                ; calibración: dw mínimo, centro, máximo × 4 ejes
HJD_TECLAS  equ 0x32                ; teclas: 20 por jugador
HJD_GMASK   equ 0x5A                ; ejes del puerto de juegos que hay
HJD_GRAW    equ 0x5E                ; dw × 4: última medida del puerto de juegos

; ---- mapa de un mando: dónde va cada cosa en su informe ----
HJM_FLAGS   equ 0                   ; bit 0: el primer byte del informe ha de ser HJM_ID
HJM_ID      equ 1
HJM_NAX     equ 2
HJM_NBTN    equ 3
HJM_AX      equ 4                   ; 6 ejes de HJA_SZ bytes:
HJA_OFF     equ 0                   ;   dw bit del informe (FFFFh = no hay)
HJA_BITS    equ 2                   ;   db bits
HJA_FL      equ 3                   ;   db bit 0: con signo; bit 1: invertido
HJA_MIN     equ 4                   ;   dd mínimo lógico
HJA_RANGO   equ 8                   ;   dd máximo - mínimo (>= 1)
HJA_SZ      equ 12
HJM_HAT     equ HJM_AX + 6 * HJA_SZ ; cruceta de 8 (o 4) posiciones: dw bit, db bits,
                                    ;   db bit 0: de 4 posiciones; dd mínimo lógico
HJM_DPAD    equ HJM_HAT + 8         ; cruceta de 4 botones: dw bit × 4 (arriba, abajo,
                                    ;   izquierda, derecha)
HJM_BTN     equ HJM_DPAD + 8        ; dw bit × 16 (FFFFh = no hay)
HJM_SZ      equ HJM_BTN + 32

%ifndef HJ_SOLO_DEFS                ; (HSPJOY y MANDO solo quieren las definiciones)
; informe (DS:BX, CX bytes) -> estado (DS:DI) según el mapa (DS:SI).
; CF si el informe no es el del mando (otro identificador de informe).
hj_extract:
        pushad
        test byte [si+HJM_FLAGS], 1
        jz .go
        test cx, cx
        jz .no
        mov al, [bx]
        cmp al, [si+HJM_ID]
        jne .no
.go:
        cmp cx, 64
        jbe .l64
        mov cx, 64
.l64:
        shl cx, 3
        mov [hj_bits], cx
        mov [hj_rep], bx
        ; ---- ejes ----
        lea bx, [si+HJM_AX]
        xor dx, dx
.ax:
        mov ax, [bx+HJA_OFF]
        mov cl, [bx+HJA_BITS]
        call hj_field
        jc .axn
        test byte [bx+HJA_FL], 1
        jz .uns
        mov ch, 32                  ; extender el signo desde CL bits
        sub ch, cl
        mov cl, ch
        shl eax, cl
        sar eax, cl
.uns:
        sub eax, [bx+HJA_MIN]
        jns .p
        xor eax, eax
.p:
        mov ecx, [bx+HJA_RANGO]
        cmp eax, ecx
        jbe .q
        mov eax, ecx
.q:
        push edx
        mov edx, 255
        mul edx
        mov ebp, ecx
        shr ebp, 1
        add eax, ebp                ; redondeo
        adc edx, 0
        div ecx
        pop edx
        test byte [bx+HJA_FL], 2
        jz .ni
        not al
.ni:
        push di
        add di, dx
        mov [di+HJS_EJES], al
        pop di
.axn:
        add bx, HJA_SZ
        inc dx
        cmp dx, 6
        jb .ax
        ; ---- cruceta ----
        xor dl, dl
        mov ax, [si+HJM_HAT]
        mov cl, [si+HJM_HAT+2]
        call hj_field
        jc .nohat
        sub eax, [si+HJM_HAT+4]
        test byte [si+HJM_HAT+3], 1
        jz .h8
        shl eax, 1
.h8:
        cmp eax, 7
        ja .nohat
        mov bx, ax
        mov dl, [hj_hattab+bx]
.nohat:
        xor bx, bx
.dp:
        mov ax, [si+HJM_DPAD+bx]
        mov cl, 1
        call hj_field
        jc .dpn
        test al, 1
        jz .dpn
        mov cl, bl
        shr cl, 1
        mov al, 1
        shl al, cl
        or dl, al
.dpn:
        add bx, 2
        cmp bx, 8
        jb .dp
        mov [di+HJS_CRUZ], dl
        ; ---- botones ----
        xor dx, dx
        xor bx, bx
.bt:
        mov ax, [si+HJM_BTN+bx]
        mov cl, 1
        call hj_field
        jc .btn
        test al, 1
        jz .btn
        mov cl, bl
        shr cl, 1
        mov ax, 1
        shl ax, cl
        or dx, ax
.btn:
        add bx, 2
        cmp bx, 32
        jb .bt
        mov [di+HJS_BTN], dx
        popad
        clc
        ret
.no:
        popad
        stc
        ret

; campo del informe: AX = primer bit, CL = bits -> EAX (sin signo);
; CF si no hay campo o no cabe en el informe recibido
hj_field:
        push bx
        push edx
        push cx
        cmp ax, 0xFFFF
        je .no
        test cl, cl
        jz .no
        movzx dx, cl
        add dx, ax
        jc .no
        cmp dx, [hj_bits]
        ja .no
        mov bx, ax
        shr bx, 3
        add bx, [hj_rep]
        mov ch, cl
        mov cl, al
        and cl, 7
        mov eax, [bx]
        mov edx, [bx+4]
        shrd eax, edx, cl
        mov cl, ch
        cmp cl, 32
        jae .ok
        mov edx, 1
        shl edx, cl
        dec edx
        and eax, edx
.ok:
        pop cx
        pop edx
        pop bx
        clc
        ret
.no:
        xor eax, eax
        pop cx
        pop edx
        pop bx
        stc
        ret

hj_bits     dw 0
hj_rep      dw 0
; cruceta de 8 posiciones (0 = arriba, en el sentido del reloj) -> bits
hj_hattab   db 1, 9, 8, 10, 2, 6, 4, 5
%endif
