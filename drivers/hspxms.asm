; =============================================================================
;  HSPXMS.SYS - gestor de memoria extendida XMS 3.0 de HispaDOS
;  Copyright (C) 2026 HispaDOS contributors. GPL-2.0-or-later.
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
total_kb    dd 0
hma_used    db 0
a20_global  db 0
a20_local   dw 0
            align 2
handles     times NHANDLES*H_LEN db 0
; GDT para INT 15h AH=87h
gdt         times 16 db 0
gdt_src     dw 0xFFFF, 0
            db 0, 0x93, 0, 0
gdt_dst     dw 0xFFFF, 0
            db 0, 0x93, 0, 0
            times 16 db 0
mv_len      dd 0
mv_src      dd 0
mv_dst      dd 0

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
        mov ax, 0x0300
        mov bx, 0x0100
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
f_query_free:
        push ecx
        call largest_free
        cmp eax, 0xFFFF
        jbe .a
        mov eax, 0xFFFF
.a:
        cmp edx, 0xFFFF
        jbe .d
        mov edx, 0xFFFF
.d:
        pop ecx
        xor bl, bl
        retf
f_query_free32:
        call largest_free
        mov ecx, [cs:total_kb]
        shl ecx, 10
        add ecx, HBASE - 1
        xor bl, bl
        retf

; -> EAX = mayor bloque libre (KB), EDX = libre total (KB)
largest_free:
        push ebx
        push esi
        push di
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

; ---- reservar ----
f_alloc:
        movzx edx, dx
f_alloc32:
        push ebx
        push ecx
        push esi
        push di
        ; buscar un handle libre
        mov di, handles
        mov cx, NHANDLES
.h:
        cmp byte [cs:di+H_USED], 0
        je .gothandle
        add di, H_LEN
        loop .h
        mov bl, 0xA1                ; no quedan handles
        jmp .fail
.gothandle:
        ; primer hueco suficiente
        xor ebx, ebx
        call gap_at
        cmp esi, edx
        jae .place
        push di
        mov di, handles
        mov cx, NHANDLES
.c:
        cmp byte [cs:di+H_USED], 0
        je .nc
        mov ebx, [cs:di+H_START]
        add ebx, [cs:di+H_SIZE]
        call gap_at
        cmp esi, edx
        jae .placep
.nc:
        add di, H_LEN
        loop .c
        pop di
        mov bl, 0xA0                ; toda la memoria está reservada
        jmp .fail
.placep:
        pop di
.place:
        mov [cs:di+H_START], ebx
        mov [cs:di+H_SIZE], edx
        mov byte [cs:di+H_LOCK], 0
        mov byte [cs:di+H_USED], 1
        mov dx, di                  ; el handle es el desplazamiento de la entrada
        pop di
        pop esi
        pop ecx
        pop ebx
        mov ax, 1
        xor bl, bl
        retf
.fail:
        pop di
        pop esi
        pop ecx
        add sp, 4                   ; EBX guardado: devolvemos el error en BL
        xor ax, ax
        retf

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

f_lock:
        push di
        call check_handle
        jc f_free.bad
        inc byte [cs:di+H_LOCK]
        mov ebx, [cs:di+H_START]
        shl ebx, 10
        add ebx, HBASE
        mov dx, bx
        shr ebx, 16
        xchg dx, bx                 ; DX:BX = dirección lineal
        pop di
        mov ax, 1
        retf

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
        mov dx, [cs:di+H_SIZE]
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

f_realloc:
        movzx ebx, bx
f_realloc32:
        push di
        push esi
        call check_handle
        jc .bad
        cmp byte [cs:di+H_LOCK], 0
        jne .locked
        cmp ebx, [cs:di+H_SIZE]
        jbe .set
        ; crecer en el sitio si el hueco siguiente basta
        push ebx
        mov ebx, [cs:di+H_START]
        add ebx, [cs:di+H_SIZE]
        call gap_at
        pop ebx
        add esi, [cs:di+H_SIZE]
        cmp esi, ebx
        jb .nomem
.set:
        mov [cs:di+H_SIZE], ebx
        pop esi
        pop di
        jmp ok
.bad:
        pop esi
        pop di
        mov bl, 0xA2
        jmp fail
.locked:
        pop esi
        pop di
        mov bl, 0xAB
        jmp fail
.nomem:
        pop esi
        pop di
        mov bl, 0xA0
        jmp fail

; ---- mover (0Bh): DS:SI -> {dd longitud, dw h. origen, dd desp., dw h. destino, dd desp.} ----
f_move:
        pushad
        push es
        mov ecx, [si]
        test cl, 1
        jnz .odd
        mov [cs:mv_len], ecx
        mov dx, [si+4]
        mov eax, [si+6]
        call lin_addr
        jc .badsrc
        mov [cs:mv_src], eax
        mov dx, [si+10]
        mov eax, [si+12]
        call lin_addr
        jc .baddst
        mov [cs:mv_dst], eax
.chunk:
        mov ecx, [cs:mv_len]
        jecxz .done
        cmp ecx, 0x10000
        jbe .c
        mov ecx, 0x10000
.c:
        sub [cs:mv_len], ecx
        push ecx
        mov eax, [cs:mv_src]
        mov [cs:gdt_src+2], ax
        shr eax, 16
        mov [cs:gdt_src+4], al
        mov [cs:gdt_src+7], ah
        mov eax, [cs:mv_dst]
        mov [cs:gdt_dst+2], ax
        shr eax, 16
        mov [cs:gdt_dst+4], al
        mov [cs:gdt_dst+7], ah
        shr ecx, 1                  ; palabras
        push cs
        pop es
        mov si, gdt
        mov ah, 0x87
        int 0x15
        pop ecx
        jc .err
        add [cs:mv_src], ecx
        add [cs:mv_dst], ecx
        jmp .chunk
.done:
        pop es
        popad
        mov ax, 1
        xor bl, bl
        retf
.odd:
        mov byte [cs:mv_err], 0xA7
        jmp .fail
.badsrc:
        mov byte [cs:mv_err], 0xA3
        jmp .fail
.baddst:
        mov byte [cs:mv_err], 0xA5
        jmp .fail
.err:
        mov byte [cs:mv_err], 0x82
.fail:
        pop es
        popad
        xor ax, ax
        mov bl, [cs:mv_err]
        retf
mv_err  db 0

; DX = handle (0 = memoria convencional, EAX = seg:desp), EAX = desplazamiento
; -> EAX = dirección lineal; CF si no es válido o se sale del bloque
lin_addr:
        test dx, dx
        jnz .h
        push ebx
        mov ebx, eax
        shr ebx, 16
        shl ebx, 4
        and eax, 0xFFFF
        add eax, ebx
        pop ebx
        clc
        ret
.h:
        push di
        call check_handle
        jc .bad
        push ebx
        mov ebx, [cs:di+H_SIZE]
        shl ebx, 10
        push eax
        add eax, [cs:mv_len]
        cmp eax, ebx
        pop eax
        pop ebx
        ja .bad
        push ebx
        mov ebx, [cs:di+H_START]
        shl ebx, 10
        add eax, ebx
        add eax, HBASE
        pop ebx
        pop di
        clc
        ret
.bad:
        pop di
        stc
        ret

; ---- línea A20 ----
a20_on:
        push ax
        mov ax, 0x2401
        int 0x15
        jnc .r
        in al, 0x92
        or al, 2
        and al, 0xFE
        out 0x92, al
.r:
        pop ax
        ret
a20_off:
        push ax
        mov ax, 0x2400
        int 0x15
        jnc .r
        in al, 0x92
        and al, 0xFC
        out 0x92, al
.r:
        pop ax
        ret
; -> AX = 1 si A20 está activa (0000:0500 frente a FFFF:0510)
a20_test:
        push ds
        push es
        push si
        push di
        push cx
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
