; MEMJUEGO.COM - programa de prueba de la memoria que ve un juego
; (tests/run_memjuegos_tests.sh). «MEMJUEGO etiqueta» escribe en la pantalla
; y añade al archivo \MEMJ.TXT de la unidad de trabajo una línea
;   MEMJ etiqueta PROG=n LIBRE=n MAYOR=n EMS=n XMS=n XMSMAX=n UMB=n VCPI=n
;     PROG     octetos del bloque de memoria convencional que le ha dado DOS
;              (lo que tendría un juego: un programa toma el mayor bloque)
;     LIBRE    PROG más los demás bloques libres de la memoria convencional
;     MAYOR    el mayor bloque que queda libre además del suyo
;     EMS      KB de memoria expandida libres (INT 67h AH=42h; 0 sin EMS)
;     XMS      KB de memoria extendida libres en total (XMS AH=08h, DX)
;     XMSMAX   KB del mayor bloque de XMS libre (AX)
;     UMB      párrafos del mayor bloque de memoria superior libre (XMS 10h)
;     VCPI     páginas de 4 KB libres para VCPI (INT 67h AX=DE03h)
; y termina (código 0).
        cpu 386
        bits 16
        org 0x100

        cld
        mov di, buf
        mov si, s_memj
        call pon
        ; la etiqueta: la cola de órdenes sin espacios delante
        movzx cx, byte [0x80]
        mov si, 0x81
.e1:
        jcxz .e2
        cmp byte [si], ' '
        jne .e3
        inc si
        dec cx
        jmp .e1
.e3:
        lodsb
        cmp al, ' '
        je .e2
        cmp al, 13
        je .e2
        stosb
        loop .e3
.e2:
        ; PROG: el bloque propio
        mov ax, cs
        dec ax
        mov es, ax
        movzx eax, word [es:3]
        shl eax, 4
        mov [prog], eax
        push cs
        pop es
        mov si, s_prog
        call pon
        call num
        ; los bloques libres de la memoria convencional (cadena de MCB)
        mov ah, 0x52
        int 0x21
        mov ax, [es:bx-2]
        xor edx, edx                ; suma de los libres
        xor ebp, ebp                ; el mayor libre
.m:
        cmp ax, 0xA000
        jae .mf
        mov es, ax
        mov bl, [es:0]
        cmp bl, 'M'
        je .m1
        cmp bl, 'Z'
        jne .mf
.m1:
        movzx ecx, word [es:3]
        cmp word [es:1], 0
        jne .m2
        shl ecx, 4
        add edx, ecx
        cmp ecx, ebp
        jbe .m3
        mov ebp, ecx
.m3:
        movzx ecx, word [es:3]
.m2:
        inc ax
        add ax, cx
        jc .mf
        cmp bl, 'Z'
        jne .m
.mf:
        push cs
        pop es
        mov eax, [prog]
        add eax, edx
        mov si, s_libre
        call pon
        call num
        mov eax, ebp
        mov si, s_mayor
        call pon
        call num
        ; EMS
        call emm_hay
        jc .noems
        mov ah, 0x42
        int 0x67
        test ah, ah
        jnz .noems0
        movzx eax, bx
        shl eax, 4
        jmp .ems
.noems0:
        xor eax, eax
        jmp .ems
.noems:
        xor eax, eax
.ems:
        mov si, s_ems
        call pon
        call num
        ; XMS
        xor eax, eax
        mov [xmsmax], eax
        mov [umb], eax
        mov ax, 0x4300
        int 0x2F
        cmp al, 0x80
        jne .noxms
        mov ax, 0x4310
        int 0x2F
        mov [xms], bx
        mov [xms+2], es
        push cs
        pop es
        mov ah, 0x08
        xor bl, bl
        call far [xms]
        movzx eax, ax
        mov [xmsmax], eax
        movzx eax, dx
        push eax
        mov ah, 0x10
        mov dx, 0xFFFF
        call far [xms]
        test ax, ax
        jnz .u1                     ; (no puede darlo: DX = el mayor)
        cmp bl, 0xB0
        jne .u0
        movzx edx, dx
        mov [umb], edx
        jmp .u0
.u1:
        mov ah, 0x11                ; (no debería: se devuelve)
        call far [xms]
.u0:
        pop eax
        jmp .xm
.noxms:
        xor eax, eax
.xm:
        mov si, s_xms
        call pon
        call num
        mov eax, [xmsmax]
        mov si, s_xmsmax
        call pon
        call num
        mov eax, [umb]
        mov si, s_umb
        call pon
        call num
        ; VCPI
        xor eax, eax
        call emm_hay
        jc .vc
        mov ax, 0xDE00
        int 0x67
        test ah, ah
        jnz .vc0
        mov ax, 0xDE03
        int 0x67
        test ah, ah
        jnz .vc0
        mov eax, edx
        jmp .vc
.vc0:
        xor eax, eax
.vc:
        mov si, s_vcpi
        call pon
        call num
        mov ax, 0x0A0D
        stosw
        mov [fin], di
        ; a la pantalla
        mov byte [di], '$'
        mov dx, buf
        mov ah, 0x09
        int 0x21
        ; y al final de \MEMJ.TXT
        mov dx, s_arch
        mov ax, 0x3D01
        int 0x21
        jnc .ab
        mov dx, s_arch
        xor cx, cx
        mov ah, 0x3C
        int 0x21
        jc .x
.ab:
        mov bx, ax
        mov ax, 0x4202
        xor cx, cx
        xor dx, dx
        int 0x21
        mov cx, [fin]
        sub cx, buf
        mov dx, buf
        mov ah, 0x40
        int 0x21
        mov ah, 0x3E
        int 0x21
.x:
        mov ax, 0x4C00
        int 0x21

; CF=0 si hay un gestor de memoria expandida (dispositivo EMMXXXX0)
emm_hay:
        push es
        push di
        push si
        push cx
        xor ax, ax
        mov es, ax
        mov es, [es:0x67*4+2]
        mov di, 10
        mov si, s_emmnom
        mov cx, 8
        repe cmpsb
        pop cx
        pop si
        pop di
        pop es
        je .si
        stc
        ret
.si:
        clc
        ret

; copia la cadena SI (acaba en 0) a DI
pon:
        lodsb
        test al, al
        jz .r
        stosb
        jmp pon
.r:
        ret

; EAX en decimal en DI
num:
        push ebx
        push ecx
        push edx
        mov ebx, 10
        xor cx, cx
.d:
        xor edx, edx
        div ebx
        push dx
        inc cx
        test eax, eax
        jnz .d
.p:
        pop ax
        add al, '0'
        stosb
        loop .p
        pop edx
        pop ecx
        pop ebx
        ret

s_memj      db "MEMJ ", 0
s_prog      db " PROG=", 0
s_libre     db " LIBRE=", 0
s_mayor     db " MAYOR=", 0
s_ems       db " EMS=", 0
s_xms       db " XMS=", 0
s_xmsmax    db " XMSMAX=", 0
s_umb       db " UMB=", 0
s_vcpi      db " VCPI=", 0
s_arch      db "\MEMJ.TXT", 0
s_emmnom    db "EMMXXXX0"
            align 4
prog        dd 0
xmsmax      dd 0
umb         dd 0
xms         dd 0
fin         dw 0
buf:
