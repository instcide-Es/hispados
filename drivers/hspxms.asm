; =============================================================================
;  HSPXMS.SYS - gestor de memoria extendida XMS 3.0 de Hispa-DOS
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  CONFIG.SYS:  DEVICE=C:\DRIVERS\HSPXMS.SYS
;  Implementa la especificación XMS 3.0 (funciones 00h-0Fh y 88h-8Fh).
;  Los movimientos se hacen con INT 15h AH=87h (servicio de la BIOS), de modo
;  que no necesita modo protegido propio. La memoria extendida empieza tras
;  la HMA (1 MB + 64 KB). UMB (10h-12h) no se implementan (las dará HSPEMM).
; =============================================================================
        cpu 386
        bits 16
        org 0

NHANDLES    equ 32
HBASE       equ 0x110000            ; dirección lineal de la memoria XMS

; entrada de la tabla de handles
H_START     equ 0                   ; KB desde HBASE (dd)
H_SIZE      equ 4                   ; KB (dd)
H_LOCK      equ 8                   ; contador de bloqueos
H_USED      equ 9
H_LEN       equ 10

header:
        dw 0xFFFF, 0xFFFF
        dw 0xA000
        dw strategy
        dw interrupt
        db "XMSXXXX0"

req         dd 0
old2f       dd 0
old15       dd 0
total_kb    dd 0
hma_used    db 0
a20_global  db 0
a20_local   dw 0
            align 2
handles     times NHANDLES*H_LEN db 0
strategy:
        mov [cs:req], bx
        mov [cs:req+2], es
        retf

interrupt:
        push ds
        push bx
        lds bx, [cs:req]
        cmp byte [bx+2], 0
        jne .done
        pop bx
        pop ds
        jmp init
.done:
        mov word [bx+3], 0x0100
        pop bx
        pop ds
        retf

; ---- INT 15h: la memoria extendida es del XMS (como HIMEM) ----
; AH=88h devuelve 0 KB: así nadie la usa a espaldas del gestor (DOSX de
; Windows, por ejemplo, tomaría memoria que ya está repartida)
int15:
        cmp ah, 0x88
        je .zero
        jmp far [cs:old15]
.zero:
        xor ax, ax
        push bp
        mov bp, sp
        and byte [bp+6], 0xFE       ; CF = 0
        pop bp
        iret

; ---- INT 2Fh: instalación y punto de entrada ----
int2f:
        cmp ax, 0x4300
        jne .n1
        mov al, 0x80
        iret
.n1:
        cmp ax, 0x4310
        jne .n2
        push cs
        pop es
        mov bx, xms_entry
        iret
.n2:
        jmp far [cs:old2f]

; ---- punto de entrada XMS (llamada lejana, AH = función) ----
xms_entry:
        jmp short .go               ; la especificación pide jmp corto + 3 nop
        nop
        nop
        nop
.go:
        cmp ah, 0x00
        je f_version
        cmp ah, 0x01
        je f_req_hma
        cmp ah, 0x02
        je f_rel_hma
        cmp ah, 0x03
        je f_gen_a20
        cmp ah, 0x04
        je f_gdis_a20
        cmp ah, 0x05
        je f_len_a20
        cmp ah, 0x06
        je f_ldis_a20
        cmp ah, 0x07
        je f_query_a20
        cmp ah, 0x08
        je f_query_free
        cmp ah, 0x09
        je f_alloc
        cmp ah, 0x0A
        je f_free
        cmp ah, 0x0B
        je f_move
        cmp ah, 0x0C
        je f_lock
        cmp ah, 0x0D
        je f_unlock
        cmp ah, 0x0E
        je f_info
        cmp ah, 0x0F
        je f_realloc
        cmp ah, 0x10
        je f_no_umb
        cmp ah, 0x88
        je f_query_free32
        cmp ah, 0x89
        je f_alloc32
        cmp ah, 0x8E
        je f_info32
        cmp ah, 0x8F
        je f_realloc32
        xor ax, ax
        mov bl, 0x80                ; función no implementada
        retf

fail:
        xor ax, ax
        retf
ok:
        mov ax, 1
        xor bl, bl
        retf

f_version:
        mov ax, 0x0300              ; XMS 3.0
        mov bx, 0x0350              ; revisión 3.50: el instalador de Windows
                                    ; rechaza los drivers anteriores a HIMEM 3
        mov dx, 1
        retf

f_req_hma:
        cmp byte [cs:hma_used], 0
        jne .busy
        mov byte [cs:hma_used], 1
        jmp ok
.busy:
        mov bl, 0x91
        jmp fail
f_rel_hma:
        cmp byte [cs:hma_used], 0
        je .no
        mov byte [cs:hma_used], 0
        jmp ok
.no:
        mov bl, 0x93
        jmp fail

f_gen_a20:
        mov byte [cs:a20_global], 1
        call a20_on
        jmp ok
f_gdis_a20:
        mov byte [cs:a20_global], 0
        cmp word [cs:a20_local], 0
        jne ok
        call a20_off
        jmp ok
f_len_a20:
        inc word [cs:a20_local]
        call a20_on
        jmp ok
f_ldis_a20:
        cmp word [cs:a20_local], 0
        je ok
        dec word [cs:a20_local]
        jnz ok
        cmp byte [cs:a20_global], 0
        jne ok
        call a20_off
        jmp ok
f_query_a20:
        call a20_test
        xor bl, bl
        retf

f_no_umb:
        xor ax, ax
        mov bl, 0xB1                ; no hay UMB disponibles
        xor dx, dx
        retf

; ---- consultas de memoria libre ----
; 08h: AX = mayor bloque libre, DX = libre total (KB, como mucho 65535).
; Solo cambian AX, DX y BL: las mitades altas de EAX y EDX se conservan
; (un cliente de 16 bits que use registros de 32 no espera perderlas).
f_query_free:
        push ecx
        push eax
        push edx
        call largest_free
        cmp eax, 0xFFFF
        jbe .a
        mov eax, 0xFFFF
.a:
        cmp edx, 0xFFFF
        jbe .d
        mov edx, 0xFFFF
.d:
        push bp
        mov bp, sp
        mov [bp+2], dx
        mov [bp+6], ax
        pop bp
        pop edx
        pop eax
        pop ecx
        xor bl, bl
        test ax, ax
        jnz .r
        mov bl, 0xA0                ; toda la memoria está reservada
.r:
        retf
f_query_free32:
        call largest_free
        mov ecx, [cs:total_kb]
        shl ecx, 10
        add ecx, HBASE - 1
        xor bl, bl
        test eax, eax
        jnz .r
        mov bl, 0xA0
.r:
        retf

; -> EAX = mayor bloque libre (KB), EDX = libre total (KB)
largest_free:
        push ebx
        push esi
        push di
        push cx
        xor eax, eax
        mov edx, [cs:total_kb]
        ; libre total = total - usado
        mov di, handles
        mov cx, NHANDLES
.u:
        cmp byte [cs:di+H_USED], 0
        je .nu
        sub edx, [cs:di+H_SIZE]
.nu:
        add di, H_LEN
        loop .u
        ; mayor hueco: candidatos = 0 y el final de cada bloque usado
        xor ebx, ebx
        call gap_at
        mov eax, esi
        mov di, handles
        mov cx, NHANDLES
.c:
        cmp byte [cs:di+H_USED], 0
        je .nc
        mov ebx, [cs:di+H_START]
        add ebx, [cs:di+H_SIZE]
        push eax
        call gap_at
        pop eax
        cmp esi, eax
        jbe .nc
        mov eax, esi
.nc:
        add di, H_LEN
        loop .c
        pop cx
        pop di
        pop esi
        pop ebx
        ret

; EBX = KB de inicio -> ESI = tamaño del hueco libre que empieza ahí (0 si no)
gap_at:
        push cx
        push di
        mov esi, [cs:total_kb]
        cmp ebx, esi
        jae .zero
        sub esi, ebx
        mov di, handles
        mov cx, NHANDLES
.l:
        cmp byte [cs:di+H_USED], 0
        je .n
        ; ¿el bloque cubre EBX?
        push eax
        mov eax, [cs:di+H_START]
        cmp eax, ebx
        ja .after
        add eax, [cs:di+H_SIZE]
        cmp eax, ebx
        pop eax
        ja .zero
        jmp .n
.after:
        ; empieza después: limita el hueco
        sub eax, ebx
        cmp eax, esi
        jae .na
        mov esi, eax
.na:
        pop eax
.n:
        add di, H_LEN
        loop .l
        pop di
        pop cx
        ret
.zero:
        xor esi, esi
        pop di
        pop cx
        ret

; EDX = KB -> EBX = inicio del primer hueco suficiente; CF si no lo hay
; (cambia ESI)
find_gap:
        push cx
        push di
        xor ebx, ebx
        call gap_at
        cmp esi, edx
        jae .ok
        mov di, handles
        mov cx, NHANDLES
.c:
        cmp byte [cs:di+H_USED], 0
        je .nc
        mov ebx, [cs:di+H_START]
        add ebx, [cs:di+H_SIZE]
        call gap_at
        cmp esi, edx
        jae .ok
.nc:
        add di, H_LEN
        loop .c
        pop di
        pop cx
        stc
        ret
.ok:
        pop di
        pop cx
        clc
        ret

; ---- reservar ----
; 09h: DX = KB -> DX = handle (la mitad alta de EDX se conserva)
f_alloc:
        push edx
        movzx edx, dx
        call alloc_core
        push bp
        mov bp, sp
        mov [bp+2], dx
        pop bp
        pop edx
        retf
f_alloc32:
        call alloc_core
        retf

; EDX = KB -> AX = 1, DX = handle, BL = 0; o AX = 0, BL = error
alloc_core:
        push ecx
        push esi
        push di
        push ebx
        ; buscar un handle libre
        mov di, handles
        mov cx, NHANDLES
.h:
        cmp byte [cs:di+H_USED], 0
        je .gothandle
        add di, H_LEN
        loop .h
        mov al, 0xA1                ; no quedan handles
        jmp .fail
.gothandle:
        call find_gap
        mov al, 0xA0                ; toda la memoria está reservada
        jc .fail
        mov [cs:di+H_START], ebx
        mov [cs:di+H_SIZE], edx
        mov byte [cs:di+H_LOCK], 0
        mov byte [cs:di+H_USED], 1
        mov dx, di                  ; el handle es el desplazamiento de la entrada
        pop ebx
        pop di
        pop esi
        pop ecx
        mov ax, 1
        xor bl, bl
        ret
.fail:
        pop ebx
        mov bl, al
        pop di
        pop esi
        pop ecx
        xor ax, ax
        ret

; DX = handle -> DI = entrada; CF si no es válido
check_handle:
        cmp dx, handles
        jb .bad
        cmp dx, handles + NHANDLES*H_LEN
        jae .bad
        mov di, dx
        sub di, handles
        push ax
        push dx
        mov ax, di
        xor dx, dx
        push cx
        mov cx, H_LEN
        div cx
        pop cx
        test dx, dx
        pop dx
        pop ax
        jnz .bad
        mov di, dx
        cmp byte [cs:di+H_USED], 0
        je .bad
        clc
        ret
.bad:
        stc
        ret

f_free:
        push di
        call check_handle
        jc .bad
        cmp byte [cs:di+H_LOCK], 0
        jne .locked
        mov byte [cs:di+H_USED], 0
        pop di
        jmp ok
.bad:
        pop di
        mov bl, 0xA2
        jmp fail
.locked:
        pop di
        mov bl, 0xAB
        jmp fail

; 0Ch: DX = handle -> DX:BX = dirección lineal (solo cambian AX, BX y DX)
f_lock:
        push di
        call check_handle
        jc f_free.bad
        cmp byte [cs:di+H_LOCK], 0xFF
        je .ovf
        inc byte [cs:di+H_LOCK]
        push eax
        mov eax, [cs:di+H_START]
        shl eax, 10
        add eax, HBASE
        mov bx, ax
        shr eax, 16
        mov dx, ax
        pop eax
        pop di
        mov ax, 1
        retf
.ovf:
        pop di
        mov bl, 0xAC                ; el contador de bloqueos se desborda
        jmp fail

f_unlock:
        push di
        call check_handle
        jc f_free.bad
        cmp byte [cs:di+H_LOCK], 0
        je .nl
        dec byte [cs:di+H_LOCK]
        pop di
        jmp ok
.nl:
        pop di
        mov bl, 0xAA
        jmp fail

f_info:
        push di
        call check_handle
        jc f_free.bad
        push cx
        call free_handles
        mov bh, [cs:di+H_LOCK]
        mov bl, cl
        pop cx
        mov dx, 0xFFFF              ; un bloque de más de 64 MB no cabe en DX
        cmp dword [cs:di+H_SIZE], 0xFFFF
        jae .big
        mov dx, [cs:di+H_SIZE]
.big:
        pop di
        mov ax, 1
        retf
f_info32:
        push di
        call check_handle
        jc f_free.bad
        call free_handles
        mov bh, [cs:di+H_LOCK]
        mov edx, [cs:di+H_SIZE]
        pop di
        mov ax, 1
        xor bl, bl
        retf

; -> CX = handles libres
free_handles:
        push di
        push ax
        mov di, handles
        mov ax, NHANDLES
        xor cx, cx
.l:
        cmp byte [cs:di+H_USED], 0
        jne .n
        inc cx
.n:
        add di, H_LEN
        dec ax
        jnz .l
        pop ax
        pop di
        ret

; ---- marco de pila de los movimientos ----
; Todo el estado de un movimiento (y la GDT de INT 15h AH=87h) va en la pila:
; así una interrupción que llame al XMS en medio de un movimiento largo (una
; caché de disco, un residente) no estropea el que estaba en marcha.
MV_LEN      equ -4                  ; bytes que faltan
MV_SRC      equ -8                  ; dirección lineal de origen
MV_DST      equ -12
MV_CH       equ -16                 ; tamaño de cada trozo
MV_A20      equ -18                 ; A20 estaba activa al empezar
MV_BACK     equ -19                 ; 1 = de atrás hacia delante
MV_GDT      equ -68                 ; 48 bytes
MV_FRAME    equ 68

%macro MV_ENTER 0
        pushad
        push ds
        push es
        mov bp, sp
        sub sp, MV_FRAME
%endmacro
; AL = código de error (0 = bien) -> AX = 1/0, BL = código; el resto intacto
%macro MV_LEAVE 0
        mov sp, bp
        pop es
        pop ds
        mov bp, sp
        mov [bp+16], al             ; BL de pushad
        popad
        cmp bl, 1
        sbb ax, ax
        neg ax
%endmacro

; 0Fh: cambiar el tamaño (BX = KB); 8Fh: EBX = KB. Si el bloque no puede
; crecer donde está, se muda a otro hueco (con su contenido).
f_realloc:
        push ebx
        movzx ebx, bx
        call realloc_core
        push bp
        mov bp, sp
        mov [bp+2], bl
        pop bp
        pop ebx
        retf
f_realloc32:
        call realloc_core
        retf

realloc_core:
        MV_ENTER
        mov al, 0xA2
        call check_handle
        jc .out
        mov al, 0xAB
        cmp byte [cs:di+H_LOCK], 0
        jne .out
        mov edx, ebx                ; tamaño nuevo
        cmp edx, [cs:di+H_SIZE]
        jbe .set
        ; crecer en el sitio si el hueco siguiente basta
        mov ebx, [cs:di+H_START]
        add ebx, [cs:di+H_SIZE]
        call gap_at
        add esi, [cs:di+H_SIZE]
        cmp esi, edx
        jae .set
        ; mudarse a otro hueco
        call find_gap
        mov al, 0xA0
        jc .out
        mov eax, [cs:di+H_SIZE]
        shl eax, 10
        mov [bp+MV_LEN], eax
        mov eax, [cs:di+H_START]
        shl eax, 10
        add eax, HBASE
        mov [bp+MV_SRC], eax
        mov eax, ebx
        shl eax, 10
        add eax, HBASE
        mov [bp+MV_DST], eax
        push ebx
        push edx
        push di
        call mv_run
        pop di
        pop edx
        pop ebx
        jc .out
        mov [cs:di+H_START], ebx
.set:
        mov [cs:di+H_SIZE], edx
        xor al, al
.out:
        MV_LEAVE
        ret

; ---- mover (0Bh): DS:SI -> {dd longitud, dw h. origen, dd desp., dw h. destino, dd desp.} ----
f_move:
        MV_ENTER
        mov ecx, [si]
        mov al, 0xA7                ; longitud no válida (impar)
        test cl, 1
        jnz .out
        mov [bp+MV_LEN], ecx
        mov dx, [si+4]
        mov eax, [si+6]
        call lin_addr
        mov [bp+MV_SRC], eax
        mov al, 0xA3                ; handle de origen (A4h: desplazamiento)
        jc .bad
        mov dx, [si+10]
        mov eax, [si+12]
        call lin_addr
        mov [bp+MV_DST], eax
        mov al, 0xA5                ; handle de destino (A6h: desplazamiento)
        jc .bad
        call mv_run
        jc .out
        xor al, al
        jmp .out
.bad:
        add al, bl
.out:
        MV_LEAVE
        retf

; mover [bp+MV_LEN] bytes de [bp+MV_SRC] a [bp+MV_DST] (direcciones lineales)
; -> CF y AL = código de error. Cambia todos los registros menos BP.
; Si origen y destino se solapan y el origen está antes, se copia de atrás
; hacia delante en trozos no mayores que la distancia (INT 15h copia siempre
; hacia delante); con menos de 256 bytes de distancia se rechaza (A8h).
mv_run:
        mov ecx, [bp+MV_LEN]
        mov byte [bp+MV_BACK], 0
        mov dword [bp+MV_CH], 0x10000
        mov ebx, [bp+MV_DST]
        sub ebx, [bp+MV_SRC]
        jbe .dirok                  ; destino antes del origen (o el mismo)
        cmp ebx, ecx
        jae .dirok                  ; no se solapan
        mov al, 0xA8
        cmp ebx, 256
        jb .ferr
        and bl, 0xFE
        cmp ebx, 0x10000
        jbe .bk
        mov ebx, 0x10000
.bk:
        mov [bp+MV_CH], ebx
        mov byte [bp+MV_BACK], 1
        add [bp+MV_SRC], ecx
        add [bp+MV_DST], ecx
.dirok:
        call a20_test
        mov [bp+MV_A20], al
        ; GDT: dos descriptores de datos (origen y destino); el resto a cero
        push ss
        pop es
        lea di, [bp+MV_GDT]
        xor ax, ax
        mov cx, 24
        cld
        rep stosw
        mov word [bp+MV_GDT+0x10], 0xFFFF
        mov byte [bp+MV_GDT+0x15], 0x93
        mov word [bp+MV_GDT+0x18], 0xFFFF
        mov byte [bp+MV_GDT+0x1D], 0x93
.chunk:
        mov ecx, [bp+MV_LEN]
        test ecx, ecx
        jz .done
        cmp ecx, [bp+MV_CH]
        jbe .c
        mov ecx, [bp+MV_CH]
.c:
        sub [bp+MV_LEN], ecx
        cmp byte [bp+MV_BACK], 0
        je .fw
        sub [bp+MV_SRC], ecx
        sub [bp+MV_DST], ecx
.fw:
        mov eax, [bp+MV_SRC]
        mov [bp+MV_GDT+0x12], ax
        shr eax, 16
        mov [bp+MV_GDT+0x14], al
        mov [bp+MV_GDT+0x17], ah
        mov eax, [bp+MV_DST]
        mov [bp+MV_GDT+0x1A], ax
        shr eax, 16
        mov [bp+MV_GDT+0x1C], al
        mov [bp+MV_GDT+0x1F], ah
        push ecx
        push bp
        shr ecx, 1                  ; palabras
        push ss
        pop es
        lea si, [bp+MV_GDT]
        mov ah, 0x87
        int 0x15
        pop bp
        pop ecx
        jc .err
        cmp byte [bp+MV_BACK], 0
        jne .chunk
        add [bp+MV_SRC], ecx
        add [bp+MV_DST], ecx
        jmp .chunk
.done:
        call .a20fix
        clc
        ret
.err:
        mov bl, 0x82                ; fallo de la línea A20
        cmp ah, 3
        je .e1
        mov bl, 0x8F                ; error del driver (paridad, excepción)
.e1:
        call .a20fix
        mov al, bl
.ferr:
        stc
        ret
; hay BIOS (las del AT) que dejan A20 cerrada al volver de INT 15h AH=87h:
; si estaba abierta se vuelve a abrir (el DOS en la HMA, Windows)
.a20fix:
        cmp byte [bp+MV_A20], 0
        je .ar
        call a20_test
        test ax, ax
        jnz .ar
        call a20_on
.ar:
        ret

; DX = handle (0 = memoria convencional, EAX = seg:desp), EAX = desplazamiento,
; ECX = longitud -> EAX = dirección lineal; CF si no vale, con BL = 0 (handle)
; o 1 (el desplazamiento o la longitud se salen del bloque)
lin_addr:
        test dx, dx
        jnz .h
        push ebx
        mov ebx, eax
        shr ebx, 16
        shl ebx, 4
        movzx eax, ax
        add eax, ebx
        pop ebx
        clc
        ret
.h:
        push di
        call check_handle
        jc .badh
        push edx
        mov edx, [cs:di+H_SIZE]
        shl edx, 10
        push eax
        add eax, ecx
        jc .bado
        cmp eax, edx
        ja .bado
        pop eax
        mov edx, [cs:di+H_START]
        shl edx, 10
        lea eax, [eax+edx+HBASE]
        pop edx
        pop di
        clc
        ret
.bado:
        pop eax
        pop edx
        pop di
        mov bl, 1
        stc
        ret
.badh:
        pop di
        mov bl, 0
        stc
        ret

; ---- línea A20 ----
; Se prueba primero la BIOS (INT 15h AX=2401h/2400h), después el controlador
; del teclado (el método del AT: las BIOS de 1990 no tienen esa función ni
; el puerto 92h) y por último el puerto 92h; tras cada intento se comprueba.
a20_on:
        push ax
        call a20_test
        test ax, ax
        jnz .r
        mov ax, 0x2401
        int 0x15
        call a20_test
        test ax, ax
        jnz .r
        mov ah, 0xDF
        call a20_kbc
        call a20_test
        test ax, ax
        jnz .r
        in al, 0x92
        or al, 2
        and al, 0xFE
        out 0x92, al
.r:
        pop ax
        ret
a20_off:
        push ax
        call a20_test
        test ax, ax
        jz .r
        mov ax, 0x2400
        int 0x15
        call a20_test
        test ax, ax
        jz .r
        mov ah, 0xDD
        call a20_kbc
        call a20_test
        test ax, ax
        jz .r
        in al, 0x92
        and al, 0xFC
        out 0x92, al
.r:
        pop ax
        ret
; puerto de salida del 8042: AH = DFh (A20 abierta) o DDh (cerrada)
a20_kbc:
        pushf
        cli
        in al, 0x64
        cmp al, 0xFF                ; no hay controlador de teclado
        je .r
        call .wait
        mov al, 0xD1
        out 0x64, al
        call .wait
        mov al, ah
        out 0x60, al
        call .wait
        mov al, 0xFF                ; orden nula: da tiempo a que cambie
        out 0x64, al
        call .wait
.r:
        popf
        ret
.wait:
        push cx
        xor cx, cx
.w:
        in al, 0x64
        test al, 2
        loopnz .w
        pop cx
        ret
; -> AX = 1 si A20 está activa (0000:0500 frente a FFFF:0510)
a20_test:
        push ds
        push es
        push si
        push di
        push cx
        pushf
        cli
        xor ax, ax
        mov ds, ax
        dec ax
        mov es, ax
        mov si, 0x0500
        mov di, 0x0510
        mov al, [si]
        push ax
        mov byte [si], 0x55
        cmp byte [es:di], 0x55
        jne .on
        mov byte [si], 0xAA
        cmp byte [es:di], 0xAA
        jne .on
        xor cx, cx
        jmp .r
.on:
        mov cx, 1
.r:
        pop ax
        mov [si], al
        mov ax, cx
        popf
        pop cx
        pop di
        pop si
        pop es
        pop ds
        ret

resident_end:

; -----------------------------------------------------------------------------
;  Inicialización
; -----------------------------------------------------------------------------
init:
        pushad
        push ds
        push es
        lds bx, [cs:req]
        ; ¿ya hay un gestor XMS?
        mov ax, 0x4300
        int 0x2F
        cmp al, 0x80
        jne .free
        mov dx, msg_already
        call print
        mov word [bx+0x0E], 0
        mov [bx+0x10], cs
        jmp .out
.free:
        ; tamaño de la memoria extendida (sin la HMA)
        mov ax, 0xE801
        int 0x15
        jc .old
        test ax, ax
        jnz .e8
        mov ax, cx
        mov bx, dx
.e8:
        movzx eax, ax
        movzx ebx, bx
        test eax, eax
        jz .old                     ; respuesta vacía: preguntar a la antigua
        ; lo de encima de 16 MB solo sigue a lo de debajo si no hay hueco
        ; (hay BIOS con «memory hole at 15M-16M»: AX < 3C00h)
        cmp eax, 0x3C00
        jb .got
        shl ebx, 6
        add eax, ebx
        jmp .got
.old:
        mov ah, 0x88
        int 0x15
        movzx eax, ax
.got:
        sub eax, 64                 ; la HMA no cuenta
        jae .pos
        xor eax, eax
.pos:
        mov [cs:total_kb], eax
        ; enganchar INT 2Fh
        push es
        mov ax, 0x352F
        int 0x21
        mov [cs:old2f], bx
        mov [cs:old2f+2], es
        pop es
        push ds
        push cs
        pop ds
        mov dx, int2f
        mov ax, 0x252F
        int 0x21
        pop ds
        ; INT 15h AH=88h (después de medir la memoria)
        push es
        mov ax, 0x3515
        int 0x21
        mov [cs:old15], bx
        mov [cs:old15+2], es
        pop es
        push ds
        push cs
        pop ds
        mov dx, int15
        mov ax, 0x2515
        int 0x21
        pop ds
        call a20_on
        ; mensaje
        push ds
        push cs
        pop ds
        mov dx, msg_ok
        mov ah, 9
        int 0x21
        mov eax, [total_kb]
        call print_dec
        mov dx, msg_kb
        mov ah, 9
        int 0x21
        pop ds
        lds bx, [cs:req]
        mov word [bx+0x0E], resident_end
        mov [bx+0x10], cs
.out:
        mov word [bx+3], 0x0100
        pop es
        pop ds
        popad
        retf

print:
        push ds
        push cs
        pop ds
        mov ah, 9
        int 0x21
        pop ds
        ret

; EAX -> decimal
print_dec:
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
        ret

msg_already db "HSPXMS: ya hay un gestor XMS instalado", 13, 10, "$"
msg_ok      db "HSPXMS 0.1: gestor XMS 3.0, $"
msg_kb      db " KB de memoria extendida", 13, 10, "$"
