; =============================================================================
;  Hispa-DOS - ratones y paneles USB: el mapa de su informe (lo saca
;  hr_parse, lib/hidraton.inc) y la lectura de cada informe con él. Parte
;  residente de HSPUSB.SYS (y de tests/progs/ratontst.asm).
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
; =============================================================================

; el mapa: posiciones en bits desde el principio de los datos del informe
; (detrás del identificador, si lo hay); FFFFh = no está
hr_on       db 0                    ; 1 = leer los informes con el mapa
hr_hasid    db 0                    ; los informes llevan identificador delante
hr_rid      db 0                    ; el del informe del movimiento
hr_xs       db 0                    ; bits de X y de Y
hr_ys       db 0
hr_xsg      db 0                    ; con signo (Logical Minimum negativo)
hr_ysg      db 0
            db 0
hr_xo       dw 0xFFFF
hr_yo       dw 0xFFFF
hr_b1       dw 0xFFFF               ; botones 1 (izquierdo), 2 (derecho) y 3
hr_b2       dw 0xFFFF
hr_b3       dw 0xFFFF

; CS:SI = informe -> CF si es otro informe (otro identificador); si no, BX =
; botones (bit 0 izquierdo, 1 derecho, 2 central), CX = movimiento en X y
; DX en Y (positivo hacia abajo), con signo y sin recortar
hr_extract:
        cmp byte [cs:hr_hasid], 0
        je .d
        mov al, [cs:si]
        cmp al, [cs:hr_rid]
        jne .no
        inc si
.d:
        xor bx, bx
        mov ax, [cs:hr_b3]
        call .bit
        or bl, al
        shl bl, 1
        mov ax, [cs:hr_b2]
        call .bit
        or bl, al
        shl bl, 1
        mov ax, [cs:hr_b1]
        call .bit
        or bl, al
        mov ax, [cs:hr_yo]
        mov cl, [cs:hr_ys]
        mov ch, [cs:hr_ysg]
        call .field
        mov dx, ax
        mov ax, [cs:hr_xo]
        mov cl, [cs:hr_xs]
        mov ch, [cs:hr_xsg]
        call .field
        mov cx, ax
        clc
        ret
.no:
        stc
        ret
; un botón: AX = su bit (FFFFh: no lo hay) -> AL = 0 o 1
.bit:
        cmp ax, 0xFFFF
        je .b0
        push cx
        push dx
        mov cx, 1
        call .field
        and al, 1
        pop dx
        pop cx
        ret
.b0:
        xor al, al
        ret
; AX = bit, CL = bits (hasta 24; de más, los de abajo), CH = con signo ->
; AX (recortado a lo que cabe en 16 bits con signo)
.field:
        cmp ax, 0xFFFF
        je .f0
        push bx
        push si
        push edx
        mov bx, ax
        shr bx, 3
        and bx, 0x3F                ; (dentro de los 64 bytes del búfer)
        mov edx, [cs:si+bx]
        and al, 7
        xchg cl, al
        shr edx, cl
        mov cl, al
        cmp cl, 24
        jbe .fs
        mov cl, 24
.fs:
        mov eax, 1
        shl eax, cl
        dec eax
        and edx, eax                ; el campo, sin signo
        test ch, ch
        jz .fu
        inc eax
        shr eax, 1                  ; su bit de signo
        test edx, eax
        jz .fu
        shl eax, 1
        sub edx, eax                ; negativo
.fu:
        cmp edx, 32767
        jle .f1
        mov edx, 32767
.f1:
        cmp edx, -32768
        jge .f2
        mov edx, -32768
.f2:
        mov ax, dx
        pop edx
        pop si
        pop bx
        ret
.f0:
        xor ax, ax
        ret
