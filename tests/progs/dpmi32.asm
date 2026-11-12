; DPMI32.COM - cliente DPMI de 32 bits para probar HSPDPMI.
;   DPMI32        todas las pruebas (termina con ERRORLEVEL 56 si todo va bien)
;   DPMI32 E      fallo de protección general sin manejador
;   DPMI32 U      instrucción no válida sin manejador
;   DPMI32 N      ejecuta «DPMI16 M» como hijo (cliente de 16 bits anidado)
        cpu 586
        bits 16
        org 0x100

%macro RES 1
        call res
        db %1, 0
%endmacro
%define OKZ   cmp eax, eax
%define FAILZ or esp, esp

start:
        mov sp, stack_top
        mov [rmseg], cs
        mov al, [0x82]
        and al, 0xDF
        mov [mode], al
        mov bx, (prog_end - start + 0x100 + 15) / 16
        mov ah, 0x4A
        int 0x21
        mov ax, 0x1687
        int 0x2F
        test ax, ax
        jnz .no
        test bl, 1                  ; ¿clientes de 32 bits?
        jz .no
        mov [entry], di
        mov [entry+2], es
        mov bx, si
        xor ax, ax
        mov es, ax
        test bx, bx
        jz .nopriv
        mov ah, 0x48
        int 0x21
        jc .no
        mov es, ax
.nopriv:
        mov ax, 1                   ; cliente de 32 bits
        call far [entry]
        jnc .pm
.no:
        mov dx, s_nodpmi
        mov ah, 0x09
        int 0x21
        mov ax, 0x4C01
        int 0x21
.pm:
        ; todavía en un segmento de código de 16 bits: hacer uno de 32
        mov [pspsel], es
        mov [dssel], ds
        push ds
        pop es
        xor ax, ax
        mov cx, 2
        int 0x31
        jc .no2
        mov [cs32sel], ax
        add ax, 8
        mov [sssel], ax
        mov bx, cs
        mov ax, 0x000B
        mov edi, buf
        int 0x31
        jc .no2
        or byte [buf+6], 0x40       ; código de 32 bits
        mov bx, [cs32sel]
        mov ax, 0x000C
        int 0x31
        jc .no2
        mov bx, ds
        mov ax, 0x000B
        int 0x31
        jc .no2
        or byte [buf+6], 0x40       ; pila de 32 bits (misma base)
        mov bx, [sssel]
        mov ax, 0x000C
        int 0x31
        jc .no2
        movzx esp, sp
        mov ss, [sssel]
        push word [cs32sel]
        push word main32
        retf
.no2:
        mov ax, 0x4C03
        int 0x21

; ---- rutinas de modo real (16 bits) ----
rm_int60:
        add ax, 0x1234
        mov bx, cs
        iret
rm_far:
        push bp
        mov bp, sp
        mov ax, [bp+6]
        pop bp
        retf
rm_callcb:
        mov ax, 0x1000
        call far [cs:cbaddr]
        retf
rm_wait:
        push ds
        xor ax, ax
        mov ds, ax
        sti
        mov bx, [0x46C]
.l:
        mov ax, [0x46C]
        sub ax, bx
        cmp ax, cx
        jb .l
        pop ds
        retf
rm_raw:
        mov byte [rawflag], 0x5A
        mov ah, 0x30
        int 0x21
        mov ax, [dssel]
        mov cx, ax
        mov dx, [sssel]
        movzx ebx, sp
        mov si, [cs32sel]
        mov edi, main32.raw_back
        jmp far [rawrm]

; ================= 32 bits =================
        bits 32
main32:
        mov al, [mode]
        cmp al, 'E'
        je mode_gp
        cmp al, 'U'
        je mode_ud
        cmp al, 'N'
        je mode_nest

        mov esi, s_hello
        call puts

        ; ---- entrada ----
        mov eax, cs
        lar ebx, eax
        jnz .f1
        test ebx, 0x400000          ; segmento de 32 bits
        jz .f1
        and al, 7
        cmp al, 7
        jne .f1
        mov eax, esp
        shr eax, 16
        jnz .f1
        mov ax, 0x0400
        int 0x31
        jc .f1
        cmp ax, 0x005A
        jne .f1
        mov ax, 0x1686
        int 0x2F
        test ax, ax
        jnz .f1
        OKZ
        jmp .r1
.f1:
        FAILZ
.r1:
        RES "entrada, codigo y pila de 32 bits"

        ; ---- selector plano ----
        xor eax, eax
        mov ecx, 1
        int 0x31
        jc .f2
        mov [flat], ax
        mov ebx, eax
        mov ax, 0x0008
        mov cx, 0xFFFF
        mov dx, 0xFFFF
        int 0x31
        jc .f2
        mov ax, 0x0009
        mov bx, [flat]
        mov cx, 0xC0F2              ; datos, 32 bits, granularidad de página
        int 0x31
        jc .f2
        movzx ebx, word [flat]
        lsl eax, ebx
        cmp eax, 0xFFFFFFFF
        jne .f2
        mov fs, [flat]
        mov eax, [fs:0x410]
        mov bx, 0x0040
        mov gs, bx
        cmp eax, [gs:0x10]
        jne .f2
        OKZ
        jmp .r2
.f2:
        FAILZ
.r2:
        RES "selector plano de 4 GB (0000h, 0008h, 0009h)"

        ; ---- memoria extendida ----
        mov ax, 0x0500
        mov edi, buf
        int 0x31
        jc .f3
        cmp dword [buf], 0x100000
        jb .f3
        mov ax, 0x0501
        mov bx, 0x0008              ; 512 KB
        xor cx, cx
        int 0x31
        jc .f3
        mov [memh], di
        mov [memh+2], si
        shl ebx, 16
        mov bx, cx
        cmp ebx, 0x100000
        jb .f3
        mov dword [fs:ebx], 0x11223344
        mov dword [fs:ebx+0x7FFFC], 0x55667788
        mov ax, 0x0501              ; otro bloque, para que el primero no pueda crecer
        xor bx, bx
        mov cx, 0x1000
        int 0x31
        jc .f3
        mov [memh2], di
        mov [memh2+2], si
        shl ebx, 16
        mov bx, cx
        mov dword [fs:ebx], 0x0BADC0DE
        mov dword [fs:ebx+0xFFC], 0x600DF00D
        mov ax, 0x0503              ; a 1 MB
        mov bx, 0x0010
        xor cx, cx
        mov di, [memh]
        mov si, [memh+2]
        int 0x31
        jc .f3
        mov [memh], di
        mov [memh+2], si
        shl ebx, 16
        mov bx, cx
        cmp dword [fs:ebx], 0x11223344
        jne .f3
        cmp dword [fs:ebx+0x7FFFC], 0x55667788
        jne .f3
        mov dword [fs:ebx+0xFFFFC], 0x99AABBCC
        cmp dword [fs:ebx+0xFFFFC], 0x99AABBCC
        jne .f3
        ; el segundo bloque (4 KB) tiene otro justo detrás: al crecer hay
        ; que copiarlo a otro sitio
        mov ax, 0x0501
        xor bx, bx
        mov cx, 0x8000
        int 0x31
        jc .f3
        mov [memh3], di
        mov [memh3+2], si
        mov ax, 0x0503
        mov bx, 0x0001
        xor cx, cx
        mov di, [memh2]
        mov si, [memh2+2]
        int 0x31
        jc .f3
        mov [memh2], di
        mov [memh2+2], si
        shl ebx, 16
        mov bx, cx
        cmp dword [fs:ebx], 0x0BADC0DE
        jne .f3
        cmp dword [fs:ebx+0xFFC], 0x600DF00D
        jne .f3
        mov ax, 0x0502
        mov di, [memh3]
        mov si, [memh3+2]
        int 0x31
        jc .f3
        mov ax, 0x0502
        mov di, [memh]
        mov si, [memh+2]
        int 0x31
        jc .f3
        mov ax, 0x0502
        mov di, [memh2]
        mov si, [memh2+2]
        int 0x31
        jc .f3
        OKZ
        jmp .r3
.f3:
        FAILZ
.r3:
        RES "memoria extendida (0500h-0503h)"

        ; ---- memoria de DOS e INT 21h simulada ----
        mov ax, 0x0100
        mov bx, 0x0010
        int 0x31
        jc .f4
        mov [dosseg], ax
        mov [dossel], dx
        push es
        mov es, dx
        xor edi, edi
        mov esi, s_hola
        mov ecx, s_hola_len
        rep movsb
        pop es
        call clr_rmcs
        mov word [rmcs+0x1C], 0x0900
        mov ax, [dosseg]
        mov [rmcs+0x24], ax
        mov ax, 0x0300
        mov bx, 0x0021
        xor ecx, ecx
        mov edi, rmcs
        int 0x31
        jc .f4
        mov ax, 0x0101
        mov dx, [dossel]
        int 0x31
        jc .f4
        ; vector de modo real y reflejo directo
        mov ax, 0x0201
        mov bl, 0x60
        mov cx, [rmseg]
        mov dx, rm_int60
        int 0x31
        jc .f4
        mov eax, 0x6000
        int 0x60
        cmp ax, 0x7234
        jne .f4
        ; llamada lejana con un parámetro en la pila
        call clr_rmcs
        mov ax, [rmseg]
        mov [rmcs+0x2C], ax
        mov word [rmcs+0x2A], rm_far
        push word 0xBEEF
        mov ax, 0x0301
        xor ebx, ebx
        mov ecx, 1
        mov edi, rmcs
        int 0x31
        pop bx
        jc .f4
        cmp word [rmcs+0x1C], 0xBEEF
        jne .f4
        OKZ
        jmp .r4
.f4:
        FAILZ
.r4:
        RES "memoria de DOS, 0300h, 0301h y reflejo"

        ; ---- vectores de modo protegido ----
        mov ax, 0x0204
        mov bl, 0x60
        int 0x31
        mov [old60], edx
        mov [old60+4], cx
        mov ax, 0x0205
        mov bl, 0x60
        mov cx, cs
        mov edx, pm_int60
        int 0x31
        jc .f5
        mov eax, 0x0100
        int 0x60                    ; +1 aquí, +1234h en modo real
        cmp ax, 0x1335
        jne .f5
        mov ax, 0x0205
        mov bl, 0x60
        mov edx, [old60]
        mov cx, [old60+4]
        int 0x31
        jc .f5
        OKZ
        jmp .r5
.f5:
        FAILZ
.r5:
        RES "vectores de modo protegido (0204h/0205h) y encadenado"

        ; ---- callback ----
        push ds
        push cs
        pop ds
        mov ax, 0x0303
        mov esi, pm_cb
        mov edi, cbregs
        int 0x31
        pop ds
        jc .f6
        mov [cbaddr], dx
        mov [cbaddr+2], cx
        call clr_rmcs
        mov ax, [rmseg]
        mov [rmcs+0x2C], ax
        mov word [rmcs+0x2A], rm_callcb
        mov ax, 0x0301
        xor ebx, ebx
        xor ecx, ecx
        mov edi, rmcs
        int 0x31
        jc .f6
        cmp word [rmcs+0x1C], 0x1001
        jne .f6
        cmp byte [cbflag], 1
        jne .f6
        mov ax, 0x0304
        mov dx, [cbaddr]
        mov cx, [cbaddr+2]
        int 0x31
        jc .f6
        OKZ
        jmp .r6
.f6:
        FAILZ
.r6:
        RES "callback de modo real (0303h/0304h)"

        ; ---- IRQ 0 e INT 1Ch con manejadores de 32 bits ----
        mov ax, 0x0204
        mov bl, 0x08
        int 0x31
        mov [old8], edx
        mov [old8+4], cx
        mov ax, 0x0204
        mov bl, 0x1C
        int 0x31
        mov [old1c], edx
        mov [old1c+4], cx
        mov ax, 0x0205
        mov bl, 0x08
        mov cx, cs
        mov edx, pm_int8
        int 0x31
        jc .f7
        mov ax, 0x0205
        mov bl, 0x1C
        mov cx, cs
        mov edx, pm_int1c
        int 0x31
        jc .f7
        mov ecx, 6
        call pm_wait_ticks
        cmp dword [ticks8], 5
        jb .f7
        cmp dword [ticks1c], 5
        jb .f7
        mov dword [ticks8], 0
        mov dword [ticks1c], 0
        mov ecx, 6
        call rm_wait_ticks
        cmp dword [ticks8], 5
        jb .f7
        cmp dword [ticks1c], 5
        jb .f7
        mov ax, 0x0205
        mov bl, 0x08
        mov edx, [old8]
        mov cx, [old8+4]
        int 0x31
        jc .f7
        mov ax, 0x0205
        mov bl, 0x1C
        mov edx, [old1c]
        mov cx, [old1c+4]
        int 0x31
        jc .f7
        OKZ
        jmp .r7
.f7:
        FAILZ
.r7:
        RES "IRQ 0 e INT 1Ch con manejadores de 32 bits"

        ; ---- excepción con manejador ----
        mov ax, 0x0202
        mov bl, 0x0D
        int 0x31
        mov [oldexc], edx
        mov [oldexc+4], cx
        mov ax, 0x0203
        mov bl, 0x0D
        mov cx, cs
        mov edx, exc_handler
        int 0x31
        jc .f8
        push es
        mov eax, 0x0FF4
.fault_here:
        mov es, eax                 ; fallo de protección, código 0FF4h
        FAILZ
.after_fault:
        pop es
        cmp byte [excflag], 1
        jne .f8
        mov ax, 0x0203
        mov bl, 0x0D
        mov edx, [oldexc]
        mov cx, [oldexc+4]
        int 0x31
        jc .f8
        OKZ
        jmp .r8
.f8:
        FAILZ
.r8:
        RES "excepcion con manejador de 32 bits"

        ; ---- conmutación en crudo ----
        mov ax, 0x0305
        int 0x31
        jc .f9
        mov [savepm], edi
        mov [savepm+4], si
        xor al, al
        mov edi, buf
        call far [savepm]
        mov ax, 0x0306
        int 0x31
        jc .f9
        mov [rawpm], edi
        mov [rawpm+4], si
        mov [rawrm], cx
        mov [rawrm+2], bx
        mov ebp, 0x12345678
        movzx eax, word [rmseg]
        mov ecx, eax
        mov edx, eax
        mov ebx, esp
        mov esi, eax
        mov edi, rm_raw
        jmp far [rawpm]
.raw_back:
        mov fs, [flat]              ; (FS y GS vuelven a cero de la conmutación)
        cmp byte [rawflag], 0x5A
        jne .f9
        cmp ebp, 0x12345678
        jne .f9
        mov eax, cs
        cmp ax, [cs32sel]
        jne .f9
        OKZ
        jmp .r9
.f9:
        FAILZ
.r9:
        RES "conmutacion en crudo (0305h/0306h)"

        ; ---- el reloj no se atrasa ----
        call rtc
        mov [sec0], al
.w1:
        call rtc
        cmp al, [sec0]
        je .w1
        mov [sec0], al
        mov eax, [fs:0x46C]
        mov [tick0], eax
        mov byte [nsec], 0
.w2:
        mov ah, 0x0B
        int 0x21
        call rtc
        cmp al, [sec0]
        je .w2
        mov [sec0], al
        inc byte [nsec]
        cmp byte [nsec], 3
        jb .w2
        mov eax, [fs:0x46C]
        sub eax, [tick0]
        push eax
        mov esi, s_tics
        call puts
        pop eax
        push eax
        call hex2
        mov esi, s_crlf
        call puts
        pop eax
        cmp eax, 48
        jb .f10
        cmp eax, 61
        ja .f10
        OKZ
        jmp .r10
.f10:
        FAILZ
.r10:
        RES "el reloj no se atrasa"

        mov esi, s_fin
        call puts
        mov al, [nfail]
        call hex2
        mov esi, s_crlf
        call puts
        mov ax, 0x4C38              ; ERRORLEVEL 56
        cmp byte [nfail], 0
        je .x
        mov al, 1
.x:
        int 0x21

mode_gp:
        mov esi, s_gp
        call puts
        mov eax, 0xAAAA5555
        mov ecx, 0x0FF4
        mov es, ecx
        jmp no_debia
mode_ud:
        mov esi, s_ud
        call puts
        ud2
no_debia:
        mov esi, s_nodebia
        call puts
        mov ax, 0x4C02
        int 0x21

mode_nest:
        mov esi, s_nest1
        call puts
        mov ax, [rmseg]
        mov [pblk+4], ax
        mov [pblk+8], ax
        mov [pblk+12], ax
        call clr_rmcs
        mov word [rmcs+0x1C], 0x4B00
        mov word [rmcs+0x14], child_name
        mov word [rmcs+0x10], pblk
        mov ax, [rmseg]
        mov [rmcs+0x24], ax
        mov [rmcs+0x22], ax
        mov ax, 0x0300
        mov bx, 0x0021
        xor ecx, ecx
        mov edi, rmcs
        int 0x31
        jc .f
        test byte [rmcs+0x20], 1
        jnz .f
        mov ax, 0x0501
        mov bx, 0x0008
        xor cx, cx
        int 0x31
        jc .f
        mov esi, s_nest2
        call puts
        mov ax, 0x4C00
        int 0x21
.f:
        mov esi, s_nestf
        call puts
        mov ax, 0x4C01
        int 0x21

; ---- manejadores ----
pm_int60:
        inc eax
        jmp far [cs:old60]
pm_int8:
        push ds
        mov ds, [cs:dssel]
        inc dword [ticks8]
        pop ds
        jmp far [cs:old8]
pm_int1c:
        push ds
        mov ds, [cs:dssel]
        inc dword [ticks1c]
        pop ds
        jmp far [cs:old1c]
; DS:ESI = pila de modo real, ES:EDI = registros de modo real
pm_cb:
        mov ax, [esi]
        mov [es:edi+0x2A], ax
        mov ax, [esi+2]
        mov [es:edi+0x2C], ax
        add word [es:edi+0x2E], 4
        inc word [es:edi+0x1C]
        mov byte [es:cbflag], 1
        iretd
exc_handler:
        push ds
        mov ds, [cs:dssel]
        cmp dword [esp+16], main32.fault_here
        jne .x
        cmp dword [esp+12], 0x0FF4
        jne .x
        mov dword [esp+16], main32.after_fault
        mov byte [excflag], 1
.x:
        pop ds
        retf

pm_wait_ticks:
        mov ebx, [fs:0x46C]
.l:
        mov eax, [fs:0x46C]
        sub eax, ebx
        cmp eax, ecx
        jb .l
        ret
rm_wait_ticks:
        call clr_rmcs
        mov [rmcs+0x18], ecx
        mov ax, [rmseg]
        mov [rmcs+0x2C], ax
        mov word [rmcs+0x2A], rm_wait
        mov word [rmcs+0x20], 0x0202
        mov ax, 0x0301
        xor ebx, ebx
        xor ecx, ecx
        mov edi, rmcs
        int 0x31
        ret
rtc:
        call clr_rmcs
        mov word [rmcs+0x1C], 0x0200
        mov ax, 0x0300
        mov bx, 0x001A
        xor ecx, ecx
        mov edi, rmcs
        int 0x31
        mov al, [rmcs+0x15]
        ret
clr_rmcs:
        push eax
        push ecx
        push edi
        mov edi, rmcs
        mov ecx, 0x32
        xor al, al
        rep stosb
        pop edi
        pop ecx
        pop eax
        ret

putc:
        push eax
        push edx
        mov dl, al
        mov ah, 0x02
        int 0x21
        pop edx
        pop eax
        ret
puts:
        lodsb
        test al, al
        jz .x
        call putc
        jmp puts
.x:
        ret
hex2:
        push eax
        shr al, 4
        call .n
        pop eax
.n:
        and al, 0x0F
        add al, '0'
        cmp al, '9'
        jbe .p
        add al, 7
.p:
        jmp putc
res:
        pop esi
        pushfd
        call puts
        popfd
        push esi
        jne .f
        mov esi, s_ok
        jmp .p
.f:
        inc byte [nfail]
        mov esi, s_fail
.p:
        jmp puts

s_nodpmi    db "No hay servidor DPMI de 32 bits", 13, 10, "$"
s_hello     db "DPMI32: en modo protegido de 32 bits", 13, 10, 0
s_crlf      db 13, 10, 0
s_ok        db ": OK32", 13, 10, 0
s_fail      db ": FALLO32", 13, 10, 0
s_hola      db "Hola desde modo real (0300h, 32 bits)", 13, 10, "$"
s_hola_len  equ $ - s_hola
s_tics      db "tics en 3 segundos: ", 0
s_fin       db "DPMI32 terminado, fallos: ", 0
s_gp        db "DPMI32: provocando un fallo de proteccion", 13, 10, 0
s_ud        db "DPMI32: provocando una instruccion no valida", 13, 10, 0
s_nodebia   db "DPMI32: NO DEBIA LLEGAR AQUI", 13, 10, 0
s_nest1     db "DPMI32: lanzando un hijo DPMI de 16 bits", 13, 10, 0
s_nest2     db "DPMI32: el hijo ha terminado y el padre sigue: OK", 13, 10, 0
s_nestf     db "DPMI32: FALLO con el hijo", 13, 10, 0
child_name  db "A:\BIN\DPMI16.COM", 0
child_tail  db 2, " M", 13
pblk        dw 0, child_tail, 0, 0x5C, 0, 0x6C, 0

            align 4
entry       dd 0
rmseg       dw 0
pspsel      dw 0
dssel       dw 0
sssel       dw 0
cs32sel     dw 0
flat        dw 0
dosseg      dw 0
dossel      dw 0
memh        dd 0
memh2       dd 0
memh3       dd 0
cbaddr      dd 0
rawrm       dd 0
old60       dd 0
            dw 0
old8        dd 0
            dw 0
old1c       dd 0
            dw 0
oldexc      dd 0
            dw 0
savepm      dd 0
            dw 0
rawpm       dd 0
            dw 0
ticks8      dd 0
ticks1c     dd 0
tick0       dd 0
mode        db 0
nfail       db 0
cbflag      db 0
rawflag     db 0
excflag     db 0
sec0        db 0
nsec        db 0
            align 4
rmcs        times 0x32 db 0
cbregs      times 0x32 db 0
buf         times 0x40 db 0
            times 0x400 db 0
stack_top:
prog_end:
