; DPMI16.COM - cliente DPMI de 16 bits para probar HSPDPMI.
;   DPMI16        todas las pruebas (termina con ERRORLEVEL 55 si todo va bien)
;   DPMI16 E      fallo de protección general sin manejador
;   DPMI16 D      división por cero sin manejador
;   DPMI16 X      fallo de protección con un manejador que encadena con el
;                 del servidor
;   DPMI16 P      le pasa al servidor un selector que no vale (vuelta de la
;                 conmutación en crudo): el servidor lo termina con un mensaje
;   DPMI16 M      reserva memoria, descriptores, callback y vectores y termina
;                 sin liberar nada
;   DPMI16 C      espera una tecla con INT 21h AH=08h (para Ctrl+C)
;   DPMI16 N      ejecuta «DPMI16 M» como hijo (clientes anidados)
        cpu 386
        bits 16
        org 0x100

%macro RES 1
        call res
        db %1, 0
%endmacro
%define OKZ   cmp ax, ax
%define FAILZ or sp, sp

start:
        mov sp, stack_top
        mov [rmseg], cs
        mov al, [0x82]
        and al, 0xDF
        mov [mode], al
        mov bx, (prog_end - start + 0x100 + 15) / 16
        mov ah, 0x4A
        int 0x21
        ; memoria libre (para ver que no se pierde nada entre ejecuciones)
        mov ax, 0x4300
        int 0x2F
        cmp al, 0x80
        jne .noxms
        mov ax, 0x4310
        int 0x2F
        mov [xms], bx
        mov [xms+2], es
        mov ah, 0x08
        call far [xms]
        push dx
        mov si, s_xms
        call puts
        pop ax
        call hex4
        mov si, s_kb
        call puts
.noxms:
        mov ah, 0x48
        mov bx, 0xFFFF
        int 0x21
        mov si, s_dos
        call puts
        mov ax, bx
        call hex4
        mov si, s_crlf
        call puts
        ; ¿hay DPMI?
        mov ax, 0x1687
        int 0x2F
        test ax, ax
        jz .have
        mov si, s_nodpmi
        call puts
        mov ax, 0x4C01
        int 0x21
.have:
        mov [entry], di
        mov [entry+2], es
        push si
        push bx
        push cx
        push dx
        mov si, s_det
        call puts
        pop ax
        call hex4                   ; versión
        mov si, s_cpu
        call puts
        pop ax
        call hex2                   ; procesador
        mov si, s_32
        call puts
        pop ax
        call hex4                   ; banderas
        mov si, s_priv
        call puts
        pop ax
        push ax
        call hex4                   ; párrafos privados
        mov si, s_crlf
        call puts
        pop bx
        xor ax, ax
        mov es, ax
        test bx, bx
        jz .nopriv
        mov ah, 0x48
        int 0x21
        jc .fail
        mov es, ax
.nopriv:
        xor ax, ax                  ; cliente de 16 bits
        call far [entry]
        jnc pm
.fail:
        mov si, s_nopm
        call puts
        mov ax, 0x4C01
        int 0x21

; ================= modo protegido =================
pm:
        mov [pspsel], es
        mov [dssel], ds
        mov [cssel], cs
        push ds
        pop es
        mov al, [mode]
        cmp al, 'E'
        je mode_gp
        cmp al, 'D'
        je mode_div
        cmp al, 'X'
        je mode_chain
        cmp al, 'P'
        je mode_bad
        cmp al, 'M'
        je mode_leak
        cmp al, 'C'
        je mode_ctrlc
        cmp al, 'N'
        je mode_nest

        ; ---- entrada ----
        mov ax, cs
        and ax, 7
        cmp ax, 7                   ; selector de la LDT, anillo 3
        jne .f1
        mov ax, ss
        cmp ax, [dssel]             ; DS = SS como en modo real
        jne .f1
        mov es, [pspsel]
        cmp word [es:0], 0x20CD     ; el PSP
        jne .f1
        mov ax, [es:0x2C]           ; entorno: ahora es un selector
        test al, 4
        jz .f1
        lar bx, ax
        jnz .f1
        OKZ
.f1:
        push ds
        pop es
        RES "entrada en modo protegido"

        mov ax, 0x1686
        int 0x2F
        test ax, ax
        RES "INT 2Fh 1686h"

        mov ax, 0x0400
        int 0x31
        jc .f3
        cmp ax, 0x005A
        jne .f3
        test bl, 1
        jz .f3
        cmp dx, 0x0870
        jne .f3
        OKZ
        jmp .r3
.f3:
        FAILZ
.r3:
        RES "version (0400h)"

        ; ---- descriptores ----
        xor ax, ax
        mov cx, 1
        int 0x31
        jc .f4
        mov [sel1], ax
        mov bx, ax
        mov ax, 0x0007
        mov cx, 0x0001
        mov dx, 0x2340
        int 0x31
        jc .f4
        mov ax, 0x0006
        mov bx, [sel1]
        int 0x31
        jc .f4
        cmp cx, 1
        jne .f4
        cmp dx, 0x2340
        jne .f4
        mov ax, 0x0008
        mov bx, [sel1]
        mov cx, 1
        mov dx, 0xFFFF
        int 0x31
        jc .f4
        movzx ebx, word [sel1]
        lsl eax, ebx
        jnz .f4
        cmp eax, 0x1FFFF
        jne .f4
        mov ax, 0x0008              ; más de 1 MB sin alinear a página: error
        mov bx, [sel1]
        mov cx, 0x0010
        xor dx, dx
        int 0x31
        jnc .f4
        mov ax, 0x0009
        mov bx, [sel1]
        mov cx, 0x00F2
        int 0x31
        jc .f4
        mov ax, 0x0009              ; anillo 0: no se puede
        mov bx, [sel1]
        mov cx, 0x0092
        int 0x31
        jnc .f4
        mov ax, 0x000A
        mov bx, [sel1]
        int 0x31
        jc .f4
        mov [sel2], ax
        mov bx, ax
        mov ax, 0x0006
        int 0x31
        jc .f4
        cmp cx, 1
        jne .f4
        cmp dx, 0x2340
        jne .f4
        mov ax, 0x000B
        mov bx, [sel1]
        mov di, buf
        int 0x31
        jc .f4
        cmp word [buf], 0xFFFF
        jne .f4
        cmp word [buf+2], 0x2340
        jne .f4
        cmp byte [buf+4], 1
        jne .f4
        mov byte [buf+4], 2
        mov ax, 0x000C
        mov bx, [sel1]
        mov di, buf
        int 0x31
        jc .f4
        mov ax, 0x0006
        mov bx, [sel1]
        int 0x31
        jc .f4
        cmp cx, 2
        jne .f4
        mov ax, 0x0003
        int 0x31
        cmp ax, 8
        jne .f4
        mov ax, 0x0001
        mov bx, [sel2]
        int 0x31
        jc .f4
        mov ax, 0x0001
        mov bx, [sel1]
        int 0x31
        jc .f4
        mov ax, 0x0006              ; ya no existe
        mov bx, [sel1]
        int 0x31
        jnc .f4
        mov ax, 0x0001              ; ni se puede liberar otra vez
        mov bx, [sel1]
        int 0x31
        jnc .f4
        mov ax, 0x000D              ; un descriptor concreto (de los 15 primeros)
        mov bx, 0x007F
        int 0x31
        jc .f4
        mov ax, 0x000D              ; dos veces no
        mov bx, 0x007F
        int 0x31
        jnc .f4
        mov ax, 0x0007
        mov bx, 0x007F
        mov cx, 0x000B
        mov dx, 0x8000
        int 0x31
        jc .f4
        mov ax, 0x0008
        mov bx, 0x007F
        xor cx, cx
        mov dx, 0x0FFF
        int 0x31
        jc .f4
        mov ax, 0x007F
        mov es, ax
        mov ax, [es:0]              ; la memoria de pantalla
        mov [es:0], ax
        push ds
        pop es
        mov ax, 0x0001
        mov bx, 0x007F
        int 0x31
        jc .f4
        OKZ
        jmp .r4
.f4:
        FAILZ
.r4:
        RES "descriptores (0000h-000Ch)"

        ; ---- segmento de modo real -> descriptor, selector 40h ----
        mov ax, 0x0002
        mov bx, 0x0040
        int 0x31
        jc .f5
        mov [sel1], ax
        mov ax, 0x0002
        mov bx, 0x0040
        int 0x31
        jc .f5
        cmp ax, [sel1]
        jne .f5
        mov es, ax
        mov bx, [es:0x10]           ; lista de equipo de la BIOS
        mov ax, 0x0040
        mov es, ax
        cmp bx, [es:0x10]
        jne .f5
        OKZ
        jmp .r5
.f5:
        FAILZ
.r5:
        push ds
        pop es
        RES "segmento a descriptor (0002h) y selector 40h"

        ; ---- memoria de DOS y INT 21h simulada ----
        mov ax, 0x0100
        mov bx, 0x0010
        int 0x31
        jc .f6
        mov [dosseg], ax
        mov [dossel], dx
        mov es, dx
        xor di, di
        mov si, s_hola
        mov cx, s_hola_len
        rep movsb
        push ds
        pop es
        call clr_rmcs
        mov word [rmcs+0x1C], 0x0900
        mov ax, [dosseg]
        mov [rmcs+0x24], ax
        mov ax, 0x0300
        mov bx, 0x0021
        xor cx, cx
        mov di, rmcs
        int 0x31
        jc .f6
        mov ax, 0x0102
        mov bx, 0x0020
        mov dx, [dossel]
        int 0x31
        jc .f6
        movzx ebx, word [dossel]
        lsl eax, ebx
        cmp eax, 0x1FF
        jne .f6
        mov ax, 0x0101
        mov dx, [dossel]
        int 0x31
        jc .f6
        ; un bloque de más de 64 KB: un descriptor por cada 64 KB
        mov ax, 0x0100
        mov bx, 0x1800              ; 96 KB
        int 0x31
        jc .f6
        mov [dosseg], ax
        mov [dossel], dx
        mov bx, dx
        add bx, 8
        mov ax, 0x0006
        int 0x31
        jc .f6
        shl ecx, 16
        mov cx, dx
        movzx eax, word [dosseg]
        shl eax, 4
        add eax, 0x10000
        cmp eax, ecx                ; el segundo empieza 64 KB después
        jne .f6
        movzx ebx, word [dossel]
        add bx, 8
        lsl eax, ebx
        cmp eax, 0x7FFF
        jne .f6
        mov ax, 0x0102              ; a 160 KB: hace falta un tercero
        mov bx, 0x2800
        mov dx, [dossel]
        int 0x31
        jc .f6
        movzx ebx, word [dossel]
        add bx, 16
        lsl eax, ebx
        jnz .f6
        cmp eax, 0x7FFF
        jne .f6
        mov es, bx
        mov byte [es:0x7FFF], 0x5A  ; el último byte del bloque
        push ds
        pop es
        mov ax, 0x0102              ; a 32 KB: sobran dos
        mov bx, 0x0800
        mov dx, [dossel]
        int 0x31
        jc .f6
        mov bx, [dossel]
        add bx, 8
        mov ax, 0x0006
        int 0x31
        jnc .f6
        mov ax, 0x0101
        mov dx, [dossel]
        int 0x31
        jc .f6
        mov ax, 0x0101              ; dos veces no
        mov dx, [dossel]
        int 0x31
        jnc .f6
        OKZ
        jmp .r6
.f6:
        FAILZ
.r6:
        RES "memoria de DOS (0100h-0102h) y 0300h"

        ; ---- memoria extendida ----
        mov ax, 0x0500
        mov di, buf
        int 0x31
        jc .f7
        cmp dword [buf], 0x10000
        jb .f7
        mov ax, 0x0501
        mov bx, 0x0004              ; 256 KB
        xor cx, cx
        int 0x31
        jc .f7
        mov [memh], di
        mov [memh+2], si
        cmp bx, 0x10                ; por encima de 1 MB
        jb .f7
        mov [memaddr], cx
        mov [memaddr+2], bx
        xor ax, ax
        mov cx, 1
        int 0x31
        jc .f7
        mov [sel1], ax
        call .setbase
        jc .f7
        mov ax, 0x0008
        mov bx, [sel1]
        mov cx, 0x0007
        mov dx, 0xFFFF              ; 512 KB
        int 0x31
        jc .f7
        mov es, [sel1]
        mov dword [es:0], 0x11223344
        mov ebx, 0x3FFF0
        mov dword [es:ebx], 0x55667788
        push ds
        pop es
        mov ax, 0x0503              ; a 512 KB (hay que moverlo)
        mov bx, 0x0008
        xor cx, cx
        mov di, [memh]
        mov si, [memh+2]
        int 0x31
        jc .f7
        mov [memh], di
        mov [memh+2], si
        mov [memaddr], cx
        mov [memaddr+2], bx
        call .setbase
        jc .f7
        mov es, [sel1]
        cmp dword [es:0], 0x11223344
        jne .f7
        mov ebx, 0x3FFF0
        cmp dword [es:ebx], 0x55667788
        jne .f7
        mov ebx, 0x7FFF0
        mov dword [es:ebx], 0x99AABBCC
        cmp dword [es:ebx], 0x99AABBCC
        jne .f7
        push ds
        pop es
        mov ax, 0x0503              ; y a 64 KB (encoge en el sitio)
        mov bx, 0x0001
        xor cx, cx
        mov di, [memh]
        mov si, [memh+2]
        int 0x31
        jc .f7
        cmp cx, [memaddr]
        jne .f7
        cmp bx, [memaddr+2]
        jne .f7
        mov [memh], di
        mov [memh+2], si
        mov ax, 0x0502
        int 0x31
        jc .f7
        mov ax, 0x0502              ; dos veces no
        mov di, [memh]
        mov si, [memh+2]
        int 0x31
        jnc .f7
        mov ax, 0x0001
        mov bx, [sel1]
        int 0x31
        jc .f7
        OKZ
        jmp .r7
.setbase:
        mov ax, 0x0007
        mov bx, [sel1]
        mov dx, [memaddr]
        mov cx, [memaddr+2]
        int 0x31
        ret
.f7:
        push ds
        pop es
        FAILZ
.r7:
        RES "memoria extendida (0500h-0503h)"

        ; ---- vectores de modo real e interrupción simulada ----
        mov ax, 0x0200
        mov bl, 0x60
        int 0x31
        mov [old60], dx
        mov [old60+2], cx
        mov ax, 0x0201
        mov bl, 0x60
        mov cx, [rmseg]
        mov dx, rm_int60
        int 0x31
        jc .f8
        mov ax, 0x0200
        mov bl, 0x60
        int 0x31
        cmp dx, rm_int60
        jne .f8
        call clr_rmcs
        mov word [rmcs+0x1C], 0x1111
        mov ax, 0x0300
        mov bx, 0x0060
        xor cx, cx
        mov di, rmcs
        int 0x31
        jc .f8
        cmp word [rmcs+0x1C], 0x2345
        jne .f8
        mov ax, [rmseg]
        cmp [rmcs+0x10], ax         ; el manejador devolvió su CS en BX
        jne .f8
        mov ax, 0x6000              ; y por reflejo directo (INT 60h)
        int 0x60
        cmp ax, 0x7234
        jne .f8
        OKZ
        jmp .r8
.f8:
        FAILZ
.r8:
        RES "vectores de modo real (0200h/0201h), 0300h y reflejo"

        ; ---- llamada lejana con parámetros en la pila ----
        call clr_rmcs
        mov ax, [rmseg]
        mov [rmcs+0x2C], ax
        mov word [rmcs+0x2A], rm_far
        push word 0xBEEF
        mov ax, 0x0301
        xor bx, bx
        mov cx, 1
        mov di, rmcs
        int 0x31
        pop bx
        jc .f9
        cmp word [rmcs+0x1C], 0xBEEF
        jne .f9
        call clr_rmcs               ; 0302h: con marco IRET
        mov ax, [rmseg]
        mov [rmcs+0x2C], ax
        mov word [rmcs+0x2A], rm_int60
        mov word [rmcs+0x1C], 0x0001
        mov ax, 0x0302
        xor bx, bx
        xor cx, cx
        mov di, rmcs
        int 0x31
        jc .f9
        cmp word [rmcs+0x1C], 0x1235
        jne .f9
        OKZ
        jmp .r9
.f9:
        FAILZ
.r9:
        RES "llamadas lejanas (0301h con pila, 0302h)"

        ; ---- vectores de modo protegido ----
        mov ax, 0x0204
        mov bl, 0x61
        int 0x31
        mov [old61], dx
        mov [old61+2], cx
        mov ax, 0x0205
        mov bl, 0x61
        mov cx, cs
        mov dx, pm_int61
        int 0x31
        jc .f10
        mov ax, 0x0204
        mov bl, 0x61
        int 0x31
        cmp dx, pm_int61
        jne .f10
        mov bx, cs
        cmp cx, bx
        jne .f10
        xor ax, ax
        int 0x61
        cmp ax, 0x6161
        jne .f10
        ; manejador que encadena con el anterior (que refleja a modo real)
        mov ax, 0x0204
        mov bl, 0x60
        int 0x31
        mov [old60pm], dx
        mov [old60pm+2], cx
        mov ax, 0x0205
        mov bl, 0x60
        mov cx, cs
        mov dx, pm_int60
        int 0x31
        jc .f10
        mov ax, 0x0100
        int 0x60                    ; +1 aquí, +1234h en modo real
        cmp ax, 0x1335
        jne .f10
        mov ax, 0x0205
        mov bl, 0x60
        mov dx, [old60pm]
        mov cx, [old60pm+2]
        int 0x31
        mov ax, 0x0205
        mov bl, 0x61
        mov dx, [old61]
        mov cx, [old61+2]
        int 0x31
        jc .f10
        OKZ
        jmp .r10
.f10:
        FAILZ
.r10:
        RES "vectores de modo protegido (0204h/0205h) y encadenado"

        ; ---- callback de modo real ----
        push ds
        mov ax, 0x0303
        push cs
        pop ds
        mov si, pm_cb
        mov di, cbregs
        int 0x31
        pop ds
        jc .f11
        mov [cbaddr], dx
        mov [cbaddr+2], cx
        call clr_rmcs
        mov ax, [rmseg]
        mov [rmcs+0x2C], ax
        mov word [rmcs+0x2A], rm_callcb
        mov ax, 0x0301
        xor bx, bx
        xor cx, cx
        mov di, rmcs
        int 0x31
        jc .f11
        cmp word [rmcs+0x1C], 0x1001
        jne .f11
        cmp byte [cbflag], 1
        jne .f11
        mov ax, 0x0304
        mov dx, [cbaddr]
        mov cx, [cbaddr+2]
        int 0x31
        jc .f11
        mov ax, 0x0304              ; dos veces no
        mov dx, [cbaddr]
        mov cx, [cbaddr+2]
        int 0x31
        jnc .f11
        OKZ
        jmp .r11
.f11:
        FAILZ
.r11:
        RES "callback de modo real (0303h/0304h)"

        ; ---- INT 1Ch con manejador de modo protegido ----
        mov ax, 0x0200
        mov bl, 0x1C
        int 0x31
        mov [rm1c], dx
        mov [rm1c+2], cx
        mov ax, 0x0204
        mov bl, 0x1C
        int 0x31
        mov [old1c], dx
        mov [old1c+2], cx
        mov word [ticks], 0
        mov ax, 0x0205
        mov bl, 0x1C
        mov cx, cs
        mov dx, pm_int1c
        int 0x31
        jc .f12
        mov cx, 6
        call rm_wait_ticks          ; el reloj corre en modo real
        cmp word [ticks], 5
        jb .f12
        mov word [ticks], 0
        mov cx, 6
        call pm_wait_ticks          ; y en modo protegido
        cmp word [ticks], 5
        jb .f12
        mov ax, 0x0205
        mov bl, 0x1C
        mov dx, [old1c]
        mov cx, [old1c+2]
        int 0x31
        jc .f12
        mov ax, 0x0200              ; el vector de modo real, como estaba
        mov bl, 0x1C
        int 0x31
        cmp dx, [rm1c]
        jne .f12
        cmp cx, [rm1c+2]
        jne .f12
        mov word [ticks], 0
        mov cx, 3
        call pm_wait_ticks
        cmp word [ticks], 0
        jne .f12
        OKZ
        jmp .r12
.f12:
        FAILZ
.r12:
        RES "INT 1Ch hacia modo protegido (desde real y desde protegido)"

        ; ---- IRQ 0 con manejador de modo protegido ----
        mov ax, 0x0204
        mov bl, 0x08
        int 0x31
        mov [old8], dx
        mov [old8+2], cx
        mov word [ticks], 0
        mov ax, 0x0205
        mov bl, 0x08
        mov cx, cs
        mov dx, pm_int8
        int 0x31
        jc .f13
        mov cx, 6
        call pm_wait_ticks
        cmp word [ticks], 5
        jb .f13
        mov word [ticks], 0
        mov cx, 6
        call rm_wait_ticks
        cmp word [ticks], 5
        jb .f13
        mov ax, 0x0205
        mov bl, 0x08
        mov dx, [old8]
        mov cx, [old8+2]
        int 0x31
        jc .f13
        OKZ
        jmp .r13
.f13:
        FAILZ
.r13:
        RES "IRQ 0 con manejador de modo protegido"

        ; ---- interrupciones virtuales ----
        mov ax, 0x0901
        int 0x31
        mov ax, 0x0902
        int 0x31
        cmp al, 1
        jne .f14
        mov ax, 0x0900
        int 0x31
        cmp al, 1
        jne .f14
        pushf
        pop bx
        test bh, 2
        jnz .f14
        mov ax, 0x0901
        int 0x31
        cmp al, 0
        jne .f14
        pushf
        pop bx
        test bh, 2
        jz .f14
        OKZ
        jmp .r14
.f14:
        sti
        FAILZ
.r14:
        RES "interrupciones virtuales (0900h-0902h)"

        ; ---- conmutación en crudo ----
        mov ax, 0x0305
        int 0x31
        jc .f15
        test ax, ax
        jnz .f15
        mov [savepm], di
        mov [savepm+2], si
        xor al, al
        mov di, buf
        call far [savepm]           ; no hace nada, pero tiene que volver
        mov ax, 0x0306
        int 0x31
        jc .f15
        mov [rawpm], di
        mov [rawpm+2], si
        mov [rawrm], cx
        mov [rawrm+2], bx
        mov ax, [rmseg]
        mov cx, ax
        mov dx, ax
        mov bx, sp
        mov si, ax
        mov di, rm_raw
        jmp far [rawpm]
.raw_back:
        cmp byte [rawflag], 0x5A
        jne .f15
        mov ax, cs
        cmp ax, [cssel]
        jne .f15
        mov ax, ss
        cmp ax, [dssel]
        jne .f15
        OKZ
        jmp .r15
.f15:
        FAILZ
.r15:
        RES "conmutacion en crudo (0305h/0306h)"

        ; ---- páginas y memoria física ----
        mov ax, 0x0600
        mov bx, 0x0010
        xor cx, cx
        xor si, si
        mov di, 0x1000
        int 0x31
        jc .f16
        mov ax, 0x0601
        int 0x31
        jc .f16
        mov ax, 0x0604
        int 0x31
        jc .f16
        test bx, bx
        jnz .f16
        cmp cx, 0x1000
        jne .f16
        mov ax, 0x0800
        mov bx, 0x000B
        mov cx, 0x8000
        xor si, si
        mov di, 0x1000
        int 0x31
        jc .f16
        cmp bx, 0x000B
        jne .f16
        cmp cx, 0x8000
        jne .f16
        mov ax, 0x0A00              ; no hay API de fabricante
        mov si, s_hola
        int 0x31
        jnc .f16
        mov ax, 0x7777              ; función que no existe
        int 0x31
        jnc .f16
        cmp ax, 0x8001
        jne .f16
        OKZ
        jmp .r16
.f16:
        FAILZ
.r16:
        RES "paginas y memoria fisica (0600h-0604h, 0800h)"

        ; ---- excepción con manejador del cliente ----
        mov ax, 0x0202
        mov bl, 0x0D
        int 0x31
        mov [oldexc], dx
        mov [oldexc+2], cx
        mov ax, 0x0203
        mov bl, 0x0D
        mov cx, cs
        mov dx, exc_handler
        int 0x31
        jc .f17
        push es
        mov ax, 0x0FF4              ; descriptor sin reservar
.fault_here:
        mov es, ax                  ; fallo de protección, código 0FF4h
        FAILZ
.after_fault:
        pop es
        cmp byte [excflag], 1
        jne .f17
        mov ax, 0x0203
        mov bl, 0x0D
        mov dx, [oldexc]
        mov cx, [oldexc+2]
        int 0x31
        jc .f17
        OKZ
        jmp .r17
.f17:
        FAILZ
.r17:
        RES "excepcion con manejador del cliente (0202h/0203h)"

        ; ---- A20 cerrada a espaldas del XMS ----
        call clr_rmcs
        mov ax, [rmseg]
        mov [rmcs+0x2C], ax
        mov word [rmcs+0x2A], rm_a20off
        mov ax, 0x0301
        xor bx, bx
        xor cx, cx
        mov di, rmcs
        int 0x31
        jc .f19
        call clr_rmcs               ; ¿cómo está ahora? (XMS 07h)
        mov ax, [xms]
        mov [rmcs+0x2A], ax
        mov ax, [xms+2]
        mov [rmcs+0x2C], ax
        mov word [rmcs+0x1C], 0x0700
        mov ax, 0x0301
        xor bx, bx
        xor cx, cx
        mov di, rmcs
        int 0x31
        jc .f19
        cmp word [rmcs+0x1C], 1
        jne .f19
        OKZ
        jmp .r19
.f19:
        FAILZ
.r19:
        RES "A20 cerrada por el puerto 92h: se vuelve a abrir"

        ; ---- el reloj no se atrasa ----
        call rtc
        mov [sec0], al
.w1:
        call rtc
        cmp al, [sec0]
        je .w1
        mov [sec0], al
        mov ax, 0x0040
        mov es, ax
        mov ax, [es:0x6C]
        mov [tick0], ax
        mov byte [nsec], 0
.w2:
        mov ah, 0x0B
        int 0x21                    ; reflejada a DOS una y otra vez
        call rtc
        cmp al, [sec0]
        je .w2
        mov [sec0], al
        inc byte [nsec]
        cmp byte [nsec], 3
        jb .w2
        mov ax, [es:0x6C]
        sub ax, [tick0]
        push ds
        pop es
        push ax
        mov si, s_tics
        call puts
        pop ax
        push ax
        call hex4
        mov si, s_crlf
        call puts
        pop ax
        cmp ax, 48
        jb .f18
        cmp ax, 61
        ja .f18
        OKZ
        jmp .r18
.f18:
        FAILZ
.r18:
        RES "el reloj no se atrasa"

        mov si, s_fin
        call puts
        mov al, [nfail]
        call hex2
        mov si, s_crlf
        call puts
        mov ax, 0x4C37              ; ERRORLEVEL 55
        cmp byte [nfail], 0
        je .x
        mov al, 1
.x:
        int 0x21

; ---- modos especiales ----
mode_gp:
        mov si, s_gp
        call puts
        mov eax, 0x11111111
        mov ebx, 0x22222222
        mov cx, 0x0FF4              ; descriptor sin reservar
        mov es, cx
        jmp no_debia
mode_chain:
        mov si, s_chain
        call puts
        mov ax, 0x0202
        mov bl, 0x0D
        int 0x31
        mov [oldexc], dx
        mov [oldexc+2], cx
        mov ax, 0x0203
        mov bl, 0x0D
        mov cx, cs
        mov dx, exc_chainer
        int 0x31
        mov eax, 0x33333333
        mov cx, 0x0FEC              ; descriptor sin reservar
        mov es, cx
        jmp no_debia
mode_bad:
        mov si, s_bad
        call puts
        mov ax, 0x0306
        int 0x31
        mov [rawpm], di
        mov [rawpm+2], si
        mov [rawrm], cx
        mov [rawrm+2], bx
        mov ax, [rmseg]
        mov cx, ax
        mov dx, ax
        mov bx, sp
        mov si, ax
        mov di, rm_raw_bad
        jmp far [rawpm]
mode_div:
        mov si, s_div
        call puts
        xor cx, cx
        mov ax, 1
        xor dx, dx
        div cx
no_debia:
        mov si, s_nodebia
        call puts
        mov ax, 0x4C02
        int 0x21

mode_leak:
        mov ax, 0x0501
        mov bx, 0x0010              ; 1 MB
        xor cx, cx
        int 0x31
        jc .f
        xor ax, ax
        mov cx, 20
        int 0x31
        jc .f
        mov ax, 0x0100
        mov bx, 0x0100
        int 0x31
        jc .f
        push ds
        mov ax, 0x0303
        push cs
        pop ds
        mov si, pm_cb
        mov di, cbregs
        int 0x31
        pop ds
        jc .f
        mov ax, 0x0204
        mov bl, 0x1C
        int 0x31
        mov [old1c], dx
        mov [old1c+2], cx
        mov ax, 0x0205
        mov bl, 0x1C
        mov cx, cs
        mov dx, pm_int1c
        int 0x31
        jc .f
        mov ax, 0x0204
        mov bl, 0x08
        int 0x31
        mov [old8], dx
        mov [old8+2], cx
        mov ax, 0x0205
        mov bl, 0x08
        mov cx, cs
        mov dx, pm_int8
        int 0x31
        jc .f
        mov cx, 3
        call pm_wait_ticks
        mov si, s_leak
        call puts
        mov ax, 0x4C00
        int 0x21
.f:
        mov si, s_leakf
        call puts
        mov ax, 0x4C01
        int 0x21

mode_ctrlc:
        mov si, s_ctrlc
        call puts
.l:
        mov ah, 0x08
        int 0x21
        jmp .l

mode_nest:
        ; un vector propio antes de lanzar al hijo: tiene que seguir después
        mov ax, 0x0205
        mov bl, 0x61
        mov cx, cs
        mov dx, pm_int61
        int 0x31
        mov si, s_nest1
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
        xor cx, cx
        mov di, rmcs
        int 0x31
        jc .f
        test byte [rmcs+0x20], 1
        jnz .f
        xor ax, ax
        int 0x61
        cmp ax, 0x6161
        jne .f
        mov ax, 0x0501              ; y la memoria sigue funcionando
        xor bx, bx
        mov cx, 0x4000
        int 0x31
        jc .f
        mov si, s_nest2
        call puts
        mov ax, 0x4C00
        int 0x21
.f:
        mov si, s_nestf
        call puts
        mov ax, 0x4C01
        int 0x21

; ---- rutinas de modo protegido ----
pm_int61:
        mov ax, 0x6161
        iret
pm_int60:
        inc ax
        jmp far [cs:old60pm]
pm_int1c:
        push ds
        mov ds, [cs:dssel]
        inc word [ticks]
        pop ds
        jmp far [cs:old1c]
pm_int8:
        push ds
        mov ds, [cs:dssel]
        inc word [ticks]
        pop ds
        jmp far [cs:old8]
; callback: DS:SI = pila de modo real, ES:DI = registros de modo real
pm_cb:
        mov ax, [si]
        mov [es:di+0x2A], ax
        mov ax, [si+2]
        mov [es:di+0x2C], ax
        add word [es:di+0x2E], 4
        inc word [es:di+0x1C]
        mov byte [es:cbflag], 1
        iret
exc_chainer:
        jmp far [cs:oldexc]
exc_handler:
        push bp
        mov bp, sp
        push ds
        mov ds, [cs:dssel]
        cmp word [bp+8], pm.fault_here
        jne .x
        cmp word [bp+6], 0x0FF4     ; código de error
        jne .x
        mov word [bp+8], pm.after_fault
        mov byte [excflag], 1
.x:
        pop ds
        pop bp
        retf

; esperar CX tics del reloj de la BIOS en modo protegido
pm_wait_ticks:
        push es
        mov ax, 0x0040
        mov es, ax
        mov bx, [es:0x6C]
.l:
        mov ax, [es:0x6C]
        sub ax, bx
        cmp ax, cx
        jb .l
        pop es
        ret
; lo mismo en modo real (llamada lejana simulada)
rm_wait_ticks:
        call clr_rmcs
        mov [rmcs+0x18], cx
        mov ax, [rmseg]
        mov [rmcs+0x2C], ax
        mov word [rmcs+0x2A], rm_wait
        mov word [rmcs+0x20], 0x0202
        mov ax, 0x0301
        xor bx, bx
        xor cx, cx
        mov di, rmcs
        int 0x31
        ret

; segundos del reloj de tiempo real (INT 1Ah AH=02h simulada) -> AL
rtc:
        push es
        push ds
        pop es
        call clr_rmcs
        mov word [rmcs+0x1C], 0x0200
        mov ax, 0x0300
        mov bx, 0x001A
        xor cx, cx
        mov di, rmcs
        int 0x31
        mov al, [rmcs+0x15]
        pop es
        ret

clr_rmcs:
        push ax
        push cx
        push di
        mov di, rmcs
        mov cx, 0x32
        xor al, al
        rep stosb
        pop di
        pop cx
        pop ax
        ret

; ---- rutinas de modo real ----
rm_int60:
        add ax, 0x1234
        mov bx, cs
        iret
rm_far:
        push bp
        mov bp, sp
        mov ax, [bp+6]              ; el parámetro copiado de la pila protegida
        pop bp
        retf
rm_a20off:
        in al, 0x92
        and al, 0xFD
        out 0x92, al
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
rm_raw_bad:
        mov ax, 0x0FF4              ; DS: un descriptor sin reservar
        mov cx, [dssel]
        mov dx, cx
        mov bx, sp
        mov si, [cssel]
        mov di, no_debia
        jmp far [rawrm]
rm_raw:
        mov byte [rawflag], 0x5A
        mov ah, 0x30
        int 0x21                    ; estamos en modo real de verdad
        mov ax, [dssel]
        mov cx, ax
        mov dx, ax
        mov bx, sp
        mov si, [cssel]
        mov di, pm.raw_back
        jmp far [rawrm]

; ---- salida (vale en los dos modos) ----
putc:
        push ax
        push dx
        mov dl, al
        mov ah, 0x02
        int 0x21
        pop dx
        pop ax
        ret
puts:
        lodsb
        test al, al
        jz .x
        call putc
        jmp puts
.x:
        ret
hex4:
        push ax
        mov al, ah
        call hex2
        pop ax
hex2:
        push ax
        shr al, 4
        call .n
        pop ax
.n:
        and al, 0x0F
        add al, '0'
        cmp al, '9'
        jbe .p
        add al, 7
.p:
        jmp putc
; resultado de una prueba: ZF = 1 bien. El nombre va detrás de la llamada.
res:
        pop si
        pushf
        call puts
        popf
        push si
        jne .f
        mov si, s_ok
        jmp .p
.f:
        inc byte [nfail]
        mov si, s_fail
.p:
        jmp puts

s_xms       db "XMS libre ", 0
s_kb        db " KB, ", 0
s_dos       db "DOS libre ", 0
s_crlf      db 13, 10, 0
s_nodpmi    db "No hay servidor DPMI", 13, 10, 0
s_nopm      db "No se pudo entrar en modo protegido", 13, 10, 0
s_det       db "DPMI16: version ", 0
s_cpu       db " procesador ", 0
s_32        db " banderas ", 0
s_priv      db " privado ", 0
s_ok        db ": OK16", 13, 10, 0
s_fail      db ": FALLO16", 13, 10, 0
s_hola      db "Hola desde modo real (0300h, 16 bits)", 13, 10, "$"
s_hola_len  equ $ - s_hola
s_tics      db "tics en 3 segundos: ", 0
s_fin       db "DPMI16 terminado, fallos: ", 0
s_gp        db "DPMI16: provocando un fallo de proteccion", 13, 10, 0
s_chain     db "DPMI16: fallo de proteccion con manejador encadenado", 13, 10, 0
s_bad       db "DPMI16: selector no valido en una llamada al servidor", 13, 10, 0
s_div       db "DPMI16: provocando una division por cero", 13, 10, 0
s_nodebia   db "DPMI16: NO DEBIA LLEGAR AQUI", 13, 10, 0
s_leak      db "DPMI16: termino sin liberar nada", 13, 10, 0
s_leakf     db "DPMI16: FALLO al reservar", 13, 10, 0
s_ctrlc     db "DPMI16: esperando Ctrl+C", 13, 10, 0
s_nest1     db "DPMI16: lanzando un hijo DPMI", 13, 10, 0
s_nest2     db "DPMI16: el hijo ha terminado y el padre sigue: OK", 13, 10, 0
s_nestf     db "DPMI16: FALLO con el hijo", 13, 10, 0
child_name  db "A:\BIN\DPMI16.COM", 0
child_tail  db 2, " M", 13
pblk        dw 0, child_tail, 0, 0x5C, 0, 0x6C, 0

            align 2
rmseg       dw 0
xms         dd 0
entry       dd 0
pspsel      dw 0
dssel       dw 0
cssel       dw 0
sel1        dw 0
sel2        dw 0
dosseg      dw 0
dossel      dw 0
memh        dd 0
memaddr     dd 0
old60       dd 0
old60pm     dd 0
old61       dd 0
old1c       dd 0
old8        dd 0
rm1c        dd 0
oldexc      dd 0
cbaddr      dd 0
savepm      dd 0
rawpm       dd 0
rawrm       dd 0
ticks       dw 0
tick0       dw 0
mode        db 0
nfail       db 0
cbflag      db 0
rawflag     db 0
excflag     db 0
sec0        db 0
nsec        db 0
            align 2
rmcs        times 0x32 db 0
cbregs      times 0x32 db 0
buf         times 0x40 db 0
            times 0x400 db 0
stack_top:
prog_end:
