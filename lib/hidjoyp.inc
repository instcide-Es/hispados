; =============================================================================
;  Hispa-DOS - mandos USB: análisis del descriptor de informe HID (HID 1.11,
;  apartado 6.2.2, y las tablas de usos «HID Usage Tables»): saca el mapa
;  (HJM_*, lib/hidjoyx.inc) de un mando de juegos o un joystick.
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Solo hace falta al enumerar el dispositivo: en HSPUSB.SYS va en la parte de
;  inicialización, que se descarta. Lo incluyen también las pruebas
;  (tests/progs/hidtest.asm), que lo alimentan con descriptores de mandos
;  reales. Todo por DS.
;
;  Qué se busca: una colección de aplicación «Joystick» (página 1, uso 4),
;  «Game Pad» (5) o «Multi-axis Controller» (8) y, dentro, las entradas
;  variables con estos usos:
;     página 1: X (30h), Y (31h), Z (32h), Rx (33h), Ry (34h), Rz (35h),
;               Slider (36h), Dial (37h), Hat switch (39h) y la cruceta de
;               botones D-pad Up/Down/Right/Left (90h-93h)
;     página 2 (simulación): Rudder (BAh), Throttle (BBh), Accelerator (C4h),
;               Brake (C5h)
;     página 9: botones 1-16
;  Se respetan los identificadores de informe (Report ID), Push/Pop, las
;  listas de usos y los rangos (Usage Minimum/Maximum), los usos de 4 bytes
;  (con la página dentro) y los elementos largos (se saltan).
;
;  Reparto en los seis ejes del estado: X, Y; segunda palanca en X2/Y2 = Z/Rz
;  (lo habitual: PlayStation, Switch, Logitech...) o Rx/Ry si no hay Rz (como
;  el HID de los mandos de Xbox); gatillos = Rx/Ry (o freno/acelerador, o Z).
;  Un joystick de vuelo: X2 = Z o Rz (timón), Y2 = Slider/Dial (gases).
; =============================================================================

HJ_NRAW     equ 10                  ; X Y Z Rx Ry Rz Slider Dial freno acelerador
HJ_NIDS     equ 8
HJ_NLIST    equ 16

; DS:SI = descriptor de informe, CX = bytes, DS:DI = mapa (HJM_SZ bytes)
; -> CF si no es un mando de juegos; si lo es, AL = ejes, AH = botones
hj_parse:
        pushad
        mov [hjp_map], di
        mov [hjp_end], si
        add [hjp_end], cx
        ; mapa vacío: todos los campos a FFFFh
        push di
        mov cx, HJM_SZ
        mov al, 0xFF
.fm:
        mov [di], al
        inc di
        loop .fm
        pop di
        mov word [di+HJM_FLAGS], 0
        mov word [di+HJM_NAX], 0
        ; estado inicial
        mov di, hjp_raw
        mov cx, HJ_NRAW * HJA_SZ
.fr:
        mov byte [di], 0xFF
        inc di
        loop .fr
        mov di, hjp_g
        mov cx, hjp_zend - hjp_g
.fz:
        mov byte [di], 0
        inc di
        loop .fz
        mov word [hjp_sel], 0xFFFF
        call hjp_clear
.next:
        cmp si, [hjp_end]
        jae .done
        lodsb
        cmp al, 0xFE                ; elemento largo: se salta
        jne .short
        mov bx, si
        add bx, 2
        cmp bx, [hjp_end]
        ja .done
        lodsb
        movzx bx, al
        inc si
        add si, bx
        jmp .next
.short:
        mov bl, al
        and bx, 3
        cmp bl, 3
        jne .sz
        mov bl, 4
.sz:
        mov [hjp_isz], bl
        mov dx, si
        add dx, bx
        cmp dx, [hjp_end]
        ja .done
        xor edx, edx                ; EDX = dato sin signo, ECX = con signo
        xor ecx, ecx
        cmp bl, 1
        jb .gotd
        ja .d2
        movzx edx, byte [si]
        movsx ecx, byte [si]
        jmp .gotd
.d2:
        cmp bl, 2
        ja .d4
        movzx edx, word [si]
        movsx ecx, word [si]
        jmp .gotd
.d4:
        mov edx, [si]
        mov ecx, edx
.gotd:
        add si, bx
        and al, 0xFC
        ; ---- globales ----
        cmp al, 0x04
        jne .g1
        mov [hjp_g+G_PAGE], dx
        jmp .next
.g1:
        cmp al, 0x14
        jne .g2
        mov [hjp_g+G_LMIN], ecx
        jmp .next
.g2:
        cmp al, 0x24
        jne .g3
        mov [hjp_g+G_LMAX], ecx
        jmp .next
.g3:
        cmp al, 0x74
        jne .g4
        mov [hjp_g+G_SIZE], dx
        jmp .next
.g4:
        cmp al, 0x94
        jne .g5
        mov [hjp_g+G_COUNT], dx
        jmp .next
.g5:
        cmp al, 0x84
        jne .g6
        call hjp_setid
        jmp .next
.g6:
        cmp al, 0xA4                ; Push
        jne .g7
        cmp byte [hjp_sp], 4
        jae .next
        movzx bx, byte [hjp_sp]
        shl bx, 4
        add bx, hjp_stack
        mov di, hjp_g
        call hjp_cp16
        inc byte [hjp_sp]
        jmp .next
.g7:
        cmp al, 0xB4                ; Pop
        jne .l1
        cmp byte [hjp_sp], 0
        je .next
        dec byte [hjp_sp]
        movzx di, byte [hjp_sp]
        shl di, 4
        add di, hjp_stack
        mov bx, hjp_g
        call hjp_cp16
        jmp .next
        ; ---- locales ----
.l1:
        cmp al, 0x08                ; Usage
        jne .l2
        cmp byte [hjp_isz], 4
        je .l1b
        movzx edx, dx
.l1b:
        movzx bx, byte [hjp_ln]
        cmp bx, HJ_NLIST
        jae .next
        shl bx, 2
        mov [hjp_list+bx], edx
        inc byte [hjp_ln]
        jmp .next
.l2:
        cmp al, 0x18                ; Usage Minimum
        jne .l3
        mov [hjp_lmin], edx
        jmp .next
.l3:
        cmp al, 0x28                ; Usage Maximum
        jne .m1
        mov [hjp_lmax], edx
        mov byte [hjp_lrange], 1
        jmp .next
        ; ---- principales ----
.m1:
        cmp al, 0x80                ; Input
        jne .m2
        call hjp_input
        jc .done
        call hjp_clear
        jmp .next
.m2:
        cmp al, 0x90                ; Output
        je .mclr
        cmp al, 0xB0                ; Feature
        je .mclr
        cmp al, 0xA0                ; Collection
        jne .m3
        cmp byte [hjp_depth], 0
        jne .mc
        xor bx, bx
        call hjp_usage
        mov byte [hjp_app], 0
        mov ebx, eax
        shr ebx, 16
        cmp bx, 1
        jne .mc
        cmp ax, 4
        je .mapp
        cmp ax, 5
        je .mapp
        cmp ax, 8
        jne .mc
.mapp:
        mov byte [hjp_app], 1
        mov byte [hjp_seen], 1
.mc:
        inc byte [hjp_depth]
        jmp .mclr
.m3:
        cmp al, 0xC0                ; End Collection
        jne .next                   ; (los demás no cambian nada que importe)
        cmp byte [hjp_depth], 0
        je .mclr
        dec byte [hjp_depth]
        jnz .mclr
        mov byte [hjp_app], 0
.mclr:
        call hjp_clear
        jmp .next
.done:
        ; ---- reparto de los ejes ----
        mov di, [hjp_map]
        cmp byte [hjp_seen], 0
        je .bad
        mov al, 0
        mov ah, 0
        call .put                   ; X
        mov al, 1
        mov ah, 1
        call .put                   ; Y
        cmp word [hjp_raw+5*HJA_SZ], 0xFFFF
        je .norz
        cmp word [hjp_raw+2*HJA_SZ], 0xFFFF
        je .twist
        mov al, 2                   ; Z y Rz: la segunda palanca
        mov ah, 2
        call .put
        mov al, 5
        mov ah, 3
        call .put
        mov al, 3                   ; Rx y Ry: los gatillos
        mov ah, 4
        call .put
        mov al, 4
        mov ah, 5
        call .put
        jmp .trig
.twist:
        mov al, 5                   ; Rz sin Z: timón
        mov ah, 2
        call .put
        jmp .slider
.norz:
        cmp word [hjp_raw+3*HJA_SZ], 0xFFFF
        je .flight
        cmp word [hjp_raw+4*HJA_SZ], 0xFFFF
        je .flight
        mov al, 3                   ; Rx y Ry: la segunda palanca
        mov ah, 2
        call .put
        mov al, 4
        mov ah, 3
        call .put
        mov al, 2                   ; Z: los gatillos juntos
        mov ah, 4
        call .put
        jmp .trig
.flight:
        mov al, 2
        mov ah, 2
        call .put
.slider:
        mov al, 6
        mov ah, 3
        call .put
        mov al, 7
        mov ah, 3
        call .put
.trig:
        mov al, 8                   ; freno y acelerador, si quedan huecos
        mov ah, 4
        call .put
        mov al, 9
        mov ah, 5
        call .put
        ; ¿sirve? X e Y, o una cruceta
        mov al, [di+HJM_NAX]
        cmp word [di+HJM_AX], 0xFFFF
        je .nxy
        cmp word [di+HJM_AX+HJA_SZ], 0xFFFF
        jne .good
.nxy:
        cmp word [di+HJM_HAT], 0xFFFF
        jne .good
        cmp word [di+HJM_DPAD], 0xFFFF
        je .bad
.good:
        mov al, [hjp_usesid]
        mov [di+HJM_FLAGS], al
        mov ax, [hjp_sel]
        cmp ax, 0xFFFF
        jne .gid
        xor al, al
.gid:
        mov [di+HJM_ID], al
        mov ax, [di+HJM_NAX]        ; AL = ejes, AH = botones
        mov bp, sp
        mov [bp+28], ax             ; (el AX de pushad)
        popad
        clc
        ret
.bad:
        popad
        stc
        ret
; eje en bruto AL -> eje AH del mapa (si hay y el hueco está libre)
.put:
        push si
        push bx
        push cx
        movzx si, al
        imul si, HJA_SZ
        add si, hjp_raw
        cmp word [si], 0xFFFF
        je .pr
        movzx bx, ah
        imul bx, HJA_SZ
        add bx, HJM_AX
        add bx, di
        cmp word [bx], 0xFFFF
        jne .pr
        mov cx, HJA_SZ
.pc:
        mov al, [si]
        mov [bx], al
        inc si
        inc bx
        loop .pc
        inc byte [di+HJM_NAX]
.pr:
        pop cx
        pop bx
        pop si
        ret

; copiar 16 bytes de DS:DI a DS:BX
hjp_cp16:
        push cx
        push ax
        mov cx, 16
.l:
        mov al, [di]
        mov [bx], al
        inc di
        inc bx
        loop .l
        pop ax
        pop cx
        ret

; olvidar los usos locales (tras cada elemento principal)
hjp_clear:
        mov byte [hjp_ln], 0
        mov byte [hjp_lrange], 0
        mov dword [hjp_lmin], 0
        mov dword [hjp_lmax], 0
        ret

; Report ID (EDX): pasar a llevar la cuenta de bits de ese informe
hjp_setid:
        push bx
        mov byte [hjp_usesid], 1
        xor bx, bx
.l:
        cmp bl, [hjp_nids]
        jae .new
        cmp dl, [hjp_ids+bx]
        je .got
        inc bx
        jmp .l
.new:
        cmp bl, HJ_NIDS
        jb .add
        mov bl, 0xFF                ; demasiados: sus campos no se miran
        jmp .got
.add:
        mov [hjp_ids+bx], dl
        push bx
        shl bx, 1
        mov word [hjp_pos+bx], 0
        pop bx
        inc byte [hjp_nids]
.got:
        mov [hjp_cur], bl
        pop bx
        ret

; uso número BX de la lista del elemento principal en curso -> EAX
; (página en la mitad alta)
hjp_usage:
        push bx
        movzx ax, byte [hjp_ln]
        cmp bx, ax
        jae .rng
        shl bx, 2
        mov eax, [hjp_list+bx]
        jmp .pg
.rng:
        cmp byte [hjp_lrange], 0
        je .last
        sub bx, ax
        movzx eax, bx
        add eax, [hjp_lmin]
        cmp eax, [hjp_lmax]
        jbe .pg
        mov eax, [hjp_lmax]
        jmp .pg
.last:
        xor eax, eax
        mov bx, ax
        movzx bx, byte [hjp_ln]
        test bx, bx
        jz .pg
        dec bx
        shl bx, 2
        mov eax, [hjp_list+bx]
.pg:
        test eax, 0xFFFF0000
        jnz .r
        push edx
        movzx edx, word [hjp_g+G_PAGE]
        shl edx, 16
        or eax, edx
        pop edx
.r:
        pop bx
        ret

; elemento Input (EDX = sus bits): anotar los campos que interesan y avanzar
; la cuenta de bits del informe en curso. CF si el descriptor no tiene sentido.
hjp_input:
        pushad
        mov [hjp_iflags], dl
        movzx eax, word [hjp_g+G_SIZE]
        movzx ecx, word [hjp_g+G_COUNT]
        imul eax, ecx
        cmp eax, 0xFFFF
        ja .err
        mov [hjp_total], ax
        movzx bx, byte [hjp_cur]
        cmp bl, 0xFF
        je .ok                      ; informe que no se sigue
        shl bx, 1
        mov ax, [hjp_pos+bx]
        mov [hjp_fpos], ax
        mov cx, [hjp_total]
        add [hjp_pos+bx], cx
        jc .err
        cmp byte [hjp_app], 0
        je .ok
        test byte [hjp_iflags], 1   ; constante (relleno)
        jnz .ok
        test byte [hjp_iflags], 2   ; matriz (no variable): no se usa
        jz .ok
        cmp word [hjp_g+G_SIZE], 0
        je .ok
        cmp word [hjp_g+G_SIZE], 32
        ja .ok
        xor bx, bx                  ; BX = campo
.f:
        cmp bx, [hjp_g+G_COUNT]
        jae .ok
        cmp bx, 128
        jae .ok
        ; ¿del informe elegido? (el primero del que se anota algo)
        movzx si, byte [hjp_cur]
        movzx dx, byte [hjp_ids+si]
        cmp word [hjp_sel], 0xFFFF
        je .any
        cmp dx, [hjp_sel]
        jne .ok
.any:
        mov [hjp_fid], dx
        call hjp_usage              ; EAX = página:uso
        ; DX = bit del campo en el informe (con el byte del identificador)
        mov dx, bx
        imul dx, [hjp_g+G_SIZE]
        add dx, [hjp_fpos]
        cmp byte [hjp_usesid], 0
        je .noid
        add dx, 8
.noid:
        mov ecx, eax
        shr ecx, 16                 ; CX = página
        cmp cx, 1
        jne .p2
        cmp ax, 0x30
        jb .n
        cmp ax, 0x37
        ja .p1b
        sub ax, 0x30
        call .axis
        jmp .n
.p1b:
        cmp ax, 0x39
        jne .p1c
        mov di, [hjp_map]
        cmp word [di+HJM_HAT], 0xFFFF
        jne .n
        mov [di+HJM_HAT], dx
        mov al, [hjp_g+G_SIZE]
        mov [di+HJM_HAT+2], al
        mov eax, [hjp_g+G_LMIN]
        mov [di+HJM_HAT+4], eax
        mov ecx, [hjp_g+G_LMAX]
        sub ecx, eax
        xor al, al
        cmp ecx, 3
        jne .h8
        mov al, 1
.h8:
        mov [di+HJM_HAT+3], al
        call .sel
        jmp .n
.p1c:
        cmp ax, 0x90
        jb .n
        cmp ax, 0x93
        ja .n
        sub ax, 0x90                ; arriba, abajo, derecha, izquierda
        mov si, ax
        movzx si, byte [hjp_dpord+si]
        shl si, 1
        mov di, [hjp_map]
        add si, di
        mov [si+HJM_DPAD], dx
        call .sel
        jmp .n
.p2:
        cmp cx, 2
        jne .p9
        mov cl, 5
        cmp ax, 0xBA                ; timón: como Rz
        je .p2a
        mov cl, 6
        cmp ax, 0xBB                ; gases: como Slider
        je .p2a
        mov cl, 9
        cmp ax, 0xC4                ; acelerador
        je .p2a
        mov cl, 8
        cmp ax, 0xC5                ; freno
        jne .n
.p2a:
        movzx ax, cl
        call .axis
        jmp .n
.p9:
        cmp cx, 9
        jne .n
        test ax, ax
        jz .n
        cmp ax, 16
        ja .n
        mov di, [hjp_map]
        cmp al, [di+HJM_NBTN]
        jbe .nb
        mov [di+HJM_NBTN], al
.nb:
        dec ax
        mov si, ax
        shl si, 1
        add si, di
        cmp word [si+HJM_BTN], 0xFFFF
        jne .n
        mov [si+HJM_BTN], dx
        call .sel
.n:
        inc bx
        jmp .f
.ok:
        popad
        clc
        ret
.err:
        popad
        stc
        ret
; el informe del que se toma el mando queda elegido
.sel:
        push ax
        mov ax, [hjp_fid]
        mov [hjp_sel], ax
        pop ax
        ret
; eje en bruto AX (0-9), bit DX
.axis:
        push bx
        push ecx
        push esi
        mov bx, ax
        imul bx, HJA_SZ
        add bx, hjp_raw
        cmp word [bx], 0xFFFF
        jne .ar
        mov [bx+HJA_OFF], dx
        mov cl, [hjp_g+G_SIZE]
        mov [bx+HJA_BITS], cl
        ; ESI = el mayor valor sin signo del campo
        mov esi, 0xFFFFFFFF
        cmp cl, 32
        jae .am
        mov esi, 1
        shl esi, cl
        dec esi
.am:
        mov eax, [hjp_g+G_LMIN]
        mov ecx, [hjp_g+G_LMAX]
        cmp ecx, eax
        jg .ar1
        ; máximo <= mínimo: un máximo escrito con signo (25 FFh por 255)
        and ecx, esi
        cmp ecx, eax
        jg .ar1
        xor eax, eax
        mov ecx, esi
.ar1:
        mov byte [bx+HJA_FL], 0
        test eax, eax
        jns .au
        mov byte [bx+HJA_FL], 1
.au:
        mov [bx+HJA_MIN], eax
        sub ecx, eax
        mov [bx+HJA_RANGO], ecx
        call .sel
.ar:
        pop esi
        pop ecx
        pop bx
        ret

; mapa de un mando tipo Xbox 360 con cable (interfaz FFh/5Dh/01h). Su informe
; de entrada (20 bytes; formato publicado en muchas fuentes, p. ej. la
; documentación de XInput y del protocolo «XUSB»):
;   0: tipo 00h   1: longitud 14h
;   2: bit 0 arriba, 1 abajo, 2 izquierda, 3 derecha, 4 Start, 5 Back,
;      6 pulsador palanca izq., 7 pulsador palanca der.
;   3: bit 0 LB, 1 RB, 2 Guía, 4 A, 5 B, 6 X, 7 Y
;   4: gatillo izq.   5: gatillo der.   (0-255)
;   6-7: X izq.  8-9: Y izq.  10-11: X der.  12-13: Y der.  (con signo, 16
;        bits; la Y crece hacia arriba: se invierte)
; DS:DI = mapa
hj_xbox360:
        pushad
        mov si, hjp_x360
        mov cx, HJM_SZ
.l:
        mov al, [si]
        mov [di], al
        inc si
        inc di
        loop .l
        popad
        ret

%macro HJ_EJE16 2                   ; bit, invertido
        dw %1
        db 16, 1 | (%2 << 1)
        dd -32768, 65535
%endmacro
hjp_x360:
        db 1, 0, 6, 11              ; el byte 0 ha de ser 00h; 6 ejes, 11 botones
        HJ_EJE16 48, 0
        HJ_EJE16 64, 1
        HJ_EJE16 80, 0
        HJ_EJE16 96, 1
        dw 32
        db 8, 0
        dd 0, 255
        dw 40
        db 8, 0
        dd 0, 255
        dw 0xFFFF                   ; sin cruceta de posiciones
        db 0, 0
        dd 0
        dw 16, 17, 18, 19           ; cruceta: arriba, abajo, izquierda, derecha
        ; botones 1-11: A B X Y LB RB Back Start palanca izq., der., Guía
        dw 28, 29, 30, 31, 24, 25, 21, 20, 22, 23, 26
        times 5 dw 0xFFFF

hjp_dpord   db 0, 1, 3, 2
hjp_map     dw 0
hjp_end     dw 0
hjp_sel     dw 0xFFFF
hjp_fid     dw 0
hjp_fpos    dw 0
hjp_total   dw 0
hjp_iflags  db 0
hjp_isz     db 0
hjp_raw     times HJ_NRAW * HJA_SZ db 0xFF
; globales (16 bytes, lo que guardan Push y Pop)
G_PAGE      equ 0
G_SIZE      equ 2
G_COUNT     equ 4
G_LMIN      equ 8
G_LMAX      equ 12
hjp_g       times 16 db 0
hjp_stack   times 4 * 16 db 0
hjp_sp      db 0
hjp_depth   db 0
hjp_app     db 0
hjp_seen    db 0
hjp_usesid  db 0
hjp_nids    db 0
hjp_cur     db 0
hjp_ids     times HJ_NIDS db 0
hjp_pos     times HJ_NIDS dw 0
hjp_zend:
hjp_ln      db 0
hjp_lrange  db 0
hjp_list    times HJ_NLIST dd 0
hjp_lmin    dd 0
hjp_lmax    dd 0
