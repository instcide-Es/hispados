; =============================================================================
;  Hispa-DOS - ratones y paneles USB: análisis del descriptor de informe HID
;  (HID 1.11, apartado 6.2.2; tablas de usos «HID Usage Tables»). Saca dónde
;  van los botones y el movimiento en los informes de un ratón que no da (o
;  no se sabe si da) los informes de arranque de 3 bytes.
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Para qué: un ratón de arranque (interfaz 03/01/02) da, tras SET_PROTOCOL
;  (arranque), botones, X e Y en los bytes 0, 1 y 2. Pero hay paneles y
;  ratones que dicen ser de ratón (protocolo 2) sin la subclase de arranque,
;  o que no hacen caso de SET_PROTOCOL y siguen con sus informes «de
;  informe», que a veces llevan delante un identificador (Report ID). Leído
;  así, el identificador saldría como un botón pulsado. HSPUSB pregunta el
;  protocolo (GET_PROTOCOL) y, si no es el de arranque, usa este mapa.
;
;  Qué se busca: las entradas (Input) variables y relativas de la página 1
;  con los usos X (30h) e Y (31h), y los botones 1-3 de la página 9, todo en
;  el mismo informe (mismo identificador). Se respetan los identificadores
;  de informe (hasta 15), Push/Pop, las listas de usos, los rangos (Usage
;  Minimum/Maximum), los usos de 4 bytes (con la página dentro) y los
;  elementos largos (se saltan). Lo absoluto (tabletas, pantallas táctiles)
;  no vale: CF.
;
;  Solo hace falta al enumerar: en HSPUSB.SYS va en la parte de
;  inicialización. La extracción, en lib/hidratx.inc (residente). Lo usan
;  también las pruebas (tests/progs/ratontst.asm). Todo por DS.
; =============================================================================

HR_NIDS     equ 16                  ; identificadores de informe 0-15
HR_NLIST    equ 8                   ; usos en la lista local

; DS:SI = descriptor de informe, CX = bytes -> CF si no hay X e Y relativas;
; si las hay, el mapa en hr_* (lib/hidratx.inc)
hr_parse:
        pushad
        mov [hrp_end], si
        add [hrp_end], cx
        mov di, hrp_g
        mov cx, hrp_zend - hrp_g
.fz:
        mov byte [di], 0
        inc di
        loop .fz
        mov ax, 0xFFFF
        mov [hr_xo], ax
        mov [hr_yo], ax
        mov [hr_b1], ax
        mov [hr_b2], ax
        mov [hr_b3], ax
        mov byte [hrp_xr], 0xFF
        mov byte [hrp_yr], 0xFF
        mov byte [hrp_br], 0xFF
        call .lclear
.next:
        cmp si, [hrp_end]
        jae .done
        lodsb
        cmp al, 0xFE                ; elemento largo: se salta
        jne .short
        cmp si, [hrp_end]
        jae .done
        movzx bx, byte [si]         ; bDataSize
        add si, 2
        add si, bx
        jmp .next
.short:
        mov ah, al
        and al, 3
        cmp al, 3
        jne .sz
        mov al, 4
.sz:
        movzx cx, al                ; bytes de datos
        mov bx, si
        add bx, cx
        cmp bx, [hrp_end]
        ja .done
        ; el dato: sin signo en EDX, con signo en EBP
        xor edx, edx
        xor ebp, ebp
        jcxz .gotd
        cmp cl, 1
        jne .d2
        movzx edx, byte [si]
        movsx ebp, byte [si]
        jmp .gotd
.d2:
        cmp cl, 2
        jne .d4
        movzx edx, word [si]
        movsx ebp, word [si]
        jmp .gotd
.d4:
        mov edx, [si]
        mov ebp, edx
.gotd:
        add si, cx
        mov al, ah
        and al, 0xFC                ; etiqueta y tipo
        ; ---- globales ----
        cmp al, 0x04                ; Usage Page
        jne .g1
        mov [hrp_g+HRG_PAGE], dx
        jmp .next
.g1:
        cmp al, 0x14                ; Logical Minimum
        jne .g2
        xor bl, bl
        test ebp, ebp
        jns .g1s
        inc bl
.g1s:
        mov [hrp_g+HRG_NEG], bl
        jmp .next
.g2:
        cmp al, 0x74                ; Report Size
        jne .g3
        mov [hrp_g+HRG_SIZE], dx
        jmp .next
.g3:
        cmp al, 0x94                ; Report Count
        jne .g4
        mov [hrp_g+HRG_COUNT], dx
        jmp .next
.g4:
        cmp al, 0x84                ; Report ID
        jne .g5
        mov [hrp_g+HRG_RID], dl
        mov byte [hrp_ids], 1
        jmp .next
.g5:
        cmp al, 0xA4                ; Push
        jne .g6
        movzx bx, byte [hrp_sp]
        cmp bx, 4
        jae .next
        inc byte [hrp_sp]
        imul bx, HRG_SZ
        add bx, hrp_stack
        mov ax, [hrp_g]
        mov [bx], ax
        mov ax, [hrp_g+2]
        mov [bx+2], ax
        mov ax, [hrp_g+4]
        mov [bx+4], ax
        mov ax, [hrp_g+6]
        mov [bx+6], ax
        jmp .next
.g6:
        cmp al, 0xB4                ; Pop
        jne .l0
        cmp byte [hrp_sp], 0
        je .next
        dec byte [hrp_sp]
        movzx bx, byte [hrp_sp]
        imul bx, HRG_SZ
        add bx, hrp_stack
        mov ax, [bx]
        mov [hrp_g], ax
        mov ax, [bx+2]
        mov [hrp_g+2], ax
        mov ax, [bx+4]
        mov [hrp_g+4], ax
        mov ax, [bx+6]
        mov [hrp_g+6], ax
        jmp .next
        ; ---- locales ----
.l0:
        cmp al, 0x08                ; Usage
        jne .l1
        call .full_usage
        movzx bx, byte [hrp_nl]
        cmp bx, HR_NLIST
        jae .next
        inc byte [hrp_nl]
        shl bx, 2
        mov [hrp_list+bx], edx
        jmp .next
.l1:
        cmp al, 0x18                ; Usage Minimum
        jne .l2
        call .full_usage
        mov [hrp_umin], edx
        or byte [hrp_rng], 1
        jmp .next
.l2:
        cmp al, 0x28                ; Usage Maximum
        jne .m0
        call .full_usage
        mov [hrp_umax], edx
        or byte [hrp_rng], 2
        jmp .next
        ; ---- principales ----
.m0:
        cmp al, 0x80                ; Input
        jne .m1
        call .input
        call .lclear
        jmp .next
.m1:
        cmp al, 0x90                ; Output, Feature, Collection, End Collection
        je .mc
        cmp al, 0xB0
        je .mc
        cmp al, 0xA0
        je .mc
        cmp al, 0xC0
        jne .next
.mc:
        call .lclear
        jmp .next
.done:
        ; X e Y relativas, en el mismo informe; los botones, si van con ellas
        mov al, [hrp_xr]
        cmp al, 0xFF
        je .no
        cmp al, [hrp_yr]
        jne .no
        mov [hr_rid], al
        mov ah, [hrp_ids]
        mov [hr_hasid], ah
        cmp al, [hrp_br]
        je .ok
        mov word [hr_b1], 0xFFFF
        mov word [hr_b2], 0xFFFF
        mov word [hr_b3], 0xFFFF
.ok:
        popad
        clc
        ret
.no:
        popad
        stc
        ret

; uso con la página: EDX (1 o 2 bytes) -> página << 16 | uso
.full_usage:
        cmp cl, 4
        je .fr
        movzx eax, word [hrp_g+HRG_PAGE]
        shl eax, 16
        movzx edx, dx
        or edx, eax
.fr:
        ret

.lclear:
        mov byte [hrp_nl], 0
        mov byte [hrp_rng], 0
        ret

; una entrada (EDX = sus marcas): sus campos, uno a uno
.input:
        pushad
        movzx bx, byte [hrp_g+HRG_RID]
        cmp bx, HR_NIDS
        jae .ir
        shl bx, 1
        add bx, hrp_off             ; posición (bits) en ese informe
        mov [hrp_po], bx
        mov cx, [hrp_g+HRG_COUNT]
        mov ax, [hrp_g+HRG_SIZE]
        mov [hrp_fs], ax
        ; constantes y matrices: solo ocupan sitio
        test dl, 1
        jnz .skip
        test dl, 2
        jz .skip
        mov [hrp_flags], dl
        xor di, di                  ; campo
.f:
        cmp di, cx
        jae .ir
        cmp di, 64                  ; (más no tiene sentido en un ratón)
        jae .skip2
        call .usage_of              ; -> EAX
        call .field
        mov bx, [hrp_po]
        mov ax, [hrp_fs]
        add [bx], ax
        inc di
        jmp .f
.skip2:
        sub cx, di
.skip:
        mov ax, [hrp_g+HRG_SIZE]
        mul cx
        mov bx, [hrp_po]
        add [bx], ax
.ir:
        popad
        ret

; uso del campo DI -> EAX (0 si no tiene)
.usage_of:
        cmp byte [hrp_nl], 0
        je .ur
        movzx bx, byte [hrp_nl]
        dec bx
        cmp di, bx
        jbe .ul
        shl bx, 2
        mov eax, [hrp_list+bx]      ; el último se repite
        ret
.ul:
        mov bx, di
        shl bx, 2
        mov eax, [hrp_list+bx]
        ret
.ur:
        cmp byte [hrp_rng], 3
        jne .u0
        mov eax, [hrp_umin]
        movzx ebx, di
        add eax, ebx
        cmp eax, [hrp_umax]
        jbe .uok
        mov eax, [hrp_umax]
.uok:
        ret
.u0:
        xor eax, eax
        ret

; el campo de uso EAX está en el bit [[hrp_po]], de [hrp_fs] bits
.field:
        push bx
        mov bx, [hrp_po]
        mov bx, [bx]                ; su posición
        mov dl, [hrp_g+HRG_RID]
        cmp eax, 0x00090001         ; botones 1-3
        jb .fx
        cmp eax, 0x00090003
        ja .fx
        cmp byte [hrp_br], 0xFF
        je .fb
        cmp dl, [hrp_br]
        jne .fe                     ; (los de otro informe no valen)
.fb:
        mov [hrp_br], dl
        push si
        mov si, ax
        dec si
        shl si, 1
        mov [hr_b1+si], bx
        pop si
        jmp .fe
.fx:
        test byte [hrp_flags], 4    ; relativo
        jz .fe
        cmp eax, 0x00010030         ; X
        jne .fy
        cmp byte [hrp_xr], 0xFF
        jne .fe
        mov [hrp_xr], dl
        mov [hr_xo], bx
        mov al, [hrp_fs]
        mov [hr_xs], al
        mov al, [hrp_g+HRG_NEG]
        mov [hr_xsg], al
        jmp .fe
.fy:
        cmp eax, 0x00010031         ; Y
        jne .fe
        cmp byte [hrp_yr], 0xFF
        jne .fe
        mov [hrp_yr], dl
        mov [hr_yo], bx
        mov al, [hrp_fs]
        mov [hr_ys], al
        mov al, [hrp_g+HRG_NEG]
        mov [hr_ysg], al
.fe:
        pop bx
        ret

; estado global (lo que guardan Push y Pop)
HRG_PAGE    equ 0
HRG_SIZE    equ 2
HRG_COUNT   equ 4
HRG_RID     equ 6
HRG_NEG     equ 7
HRG_SZ      equ 8
hrp_end     dw 0
hrp_po      dw 0
hrp_fs      dw 0
hrp_flags   db 0
            align 4
hrp_g       times HRG_SZ db 0
hrp_stack   times 4 * HRG_SZ db 0
hrp_sp      db 0
hrp_ids     db 0                    ; hay identificadores de informe
hrp_nl      db 0
hrp_rng     db 0
hrp_xr      db 0                    ; informe de X, de Y y de los botones
hrp_yr      db 0
hrp_br      db 0
            align 4
hrp_list    times HR_NLIST dd 0
hrp_umin    dd 0
hrp_umax    dd 0
hrp_off     times HR_NIDS dw 0
hrp_zend:
