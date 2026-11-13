; EMMMISC.COM - pruebas sueltas del monitor V86 de HSPEMM:
;   EMMMISC A   línea A20 virtual: se cierra y se abre por el puerto 92h y por
;               el controlador del teclado, y se ve (o no) la HMA
;   EMMMISC H   HLT con interrupciones: el reloj sigue andando
;   EMMMISC C   leer CR0 y los registros de depuración (instrucciones
;               privilegiadas que el monitor emula)
;   EMMMISC P   intenta pasar a modo protegido por su cuenta (LGDT): el
;               monitor lo corta con un aviso
;   EMMMISC S   INT 21h AX=5800h-5803h: primer, mejor y último ajuste, y el
;               enlace de la memoria superior (también vale sin HSPEMM)
;   EMMMISC W   hace de Windows: INT 2Fh AX=1605h; si le dan la rutina de
;               apagar el modo V86, lo apaga, mira que está en modo real, lo
;               enciende y sigue
        cpu 386
        bits 16
        org 0x100

%macro SAY 1
        mov si, %1
        call puts
%endmacro

        mov si, 0x81
.a:
        lodsb
        cmp al, ' '
        je .a
        and al, 0xDF
        cmp al, 'A'
        je t_a20
        cmp al, 'H'
        je t_hlt
        cmp al, 'C'
        je t_cr
        cmp al, 'P'
        je t_pm
        cmp al, 'W'
        je t_win
        cmp al, 'S'
        je t_strat
        SAY m_use
        mov ax, 0x4C01
        int 0x21

; ---- A20 ----
t_a20:
        cli                         ; (con DOS=HIGH cada entrada al núcleo
        call a20_seen               ; vuelve a abrir A20: sin interrupciones)
        mov [was], al
        ; cerrar por el puerto 92h
        in al, 0x92
        and al, 0xFD
        out 0x92, al
        call a20_seen
        test al, al
        jz .st_1
        mov byte [step], 1
        jmp fail
.st_1:
        in al, 0x92
        test al, 2
        jz .st_2
        mov byte [step], 2
        jmp fail
.st_2:
        ; abrir por el puerto 92h
        in al, 0x92
        or al, 2
        out 0x92, al
        call a20_seen
        test al, al
        jnz .st_3
        mov byte [step], 3
        jmp fail
.st_3:
        ; la HMA guarda lo suyo mientras está cerrada
        mov ax, 0xFFFF
        mov es, ax
        mov ax, [es:0x0600]
        mov [hmaw], ax              ; (se devuelve al acabar)
        mov word [es:0x0600], 0x4D48
        in al, 0x92
        and al, 0xFD
        out 0x92, al
        push ds
        xor ax, ax
        mov ds, ax
        mov bx, [0x05F0]            ; lo que hay en el espejo (0000:05F0)
        pop ds
        cmp bx, [es:0x0600]         ; cerrada: FFFF:0600 es 0000:05F0
        je .st_4
        mov byte [step], 4
        jmp fail
.st_4:
        ; abrir por el controlador del teclado (D1h, DFh)
        mov ah, 0xDF
        call kbc_a20
        call a20_seen
        test al, al
        jnz .st_5
        mov byte [step], 5
        jmp fail
.st_5:
        cmp word [es:0x0600], 0x4D48
        je .st_6
        mov byte [step], 6
        jmp fail
.st_6:
        mov ax, [hmaw]
        mov [es:0x0600], ax
        ; y cerrar (D1h, DDh)
        mov ah, 0xDD
        call kbc_a20
        call a20_seen
        test al, al
        jz .st_7
        mov byte [step], 7
        jmp fail
.st_7:
        ; como estaba
        in al, 0x92
        and al, 0xFD
        cmp byte [was], 0
        je .set
        or al, 2
.set:
        out 0x92, al
        call a20_seen
        cmp al, [was]
        je .st_8
        mov byte [step], 8
        jmp fail
.st_8:
        sti
        SAY m_a20
        jmp done
kbc_a20:
        call .wait
        mov al, 0xD1
        out 0x64, al
        call .wait
        mov al, ah
        out 0x60, al
        call .wait
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
; -> AL = 1 si se ve la HMA (0000:0500 distinto de FFFF:0510)
a20_seen:
        push ds
        push es
        pushf
        cli
        xor ax, ax
        mov ds, ax
        dec ax
        mov es, ax
        mov ah, [0x0500]
        mov byte [0x0500], 0x55
        cmp byte [es:0x0510], 0x55
        jne .on
        mov byte [0x0500], 0xAA
        cmp byte [es:0x0510], 0xAA
        jne .on
        xor al, al
        jmp .r
.on:
        mov al, 1
.r:
        mov [0x0500], ah
        popf
        pop es
        pop ds
        ret

; ---- HLT ----
t_hlt:
        push ds
        xor ax, ax
        mov ds, ax
        mov bx, [0x046C]
        pop ds
        mov cx, 6
.h:
        sti
        hlt
        loop .h
        push ds
        xor ax, ax
        mov ds, ax
        mov ax, [0x046C]
        pop ds
        sub ax, bx
        cmp ax, 1
        jae .st_9
        mov byte [step], 9
        jmp fail
.st_9:
        cmp ax, 12
        jbe .st_10
        mov byte [step], 10
        jmp fail
.st_10:
        SAY m_hlt
        jmp done

; ---- CR0 y DR ----
t_cr:
        mov eax, cr0
        test al, 1
        jnz .st_11
        mov byte [step], 11
        jmp fail
.st_11:                     ; en modo V86, PE = 1
        mov ebx, eax
        mov cr0, eax                ; escribir lo mismo no hace daño
        mov eax, cr0
        cmp eax, ebx
        je .st_12
        mov byte [step], 12
        jmp fail
.st_12:
        mov eax, 0x00012340
        mov dr0, eax
        xor eax, eax
        mov eax, dr0
        cmp eax, 0x00012340
        je .st_13
        mov byte [step], 13
        jmp fail
.st_13:
        xor eax, eax
        mov dr0, eax
        mov eax, cr3
        test eax, eax
        jnz .st_14
        mov byte [step], 14
        jmp fail
.st_14:
        clts
        SAY m_cr
        jmp done

; ---- modo protegido por su cuenta ----
t_pm:
        SAY m_pm
        lgdt [gdtr]
        SAY m_pmbad                 ; no se debe llegar aquí
        mov ax, 0x4C01
        int 0x21

; ---- Windows ----
t_win:
        xor bx, bx
        mov es, bx
        xor si, si
        mov ds, si
        xor cx, cx
        xor dx, dx                  ; modo 386 mejorado
        mov di, 0x030A
        mov ax, 0x1605
        int 0x2F
        mov ax, ds
        push cs
        pop ds
        push cs
        pop es
        test cx, cx
        jnz .no
        mov [cb], si
        mov [cb+2], ax
        or ax, si
        jz .nocb
        ; apagar el modo V86
        cli
        xor ax, ax
        call far [cb]
        jnc .st_15
        mov byte [step], 15
        jmp fail
.st_15:
        smsw ax
        test al, 1
        jz .st_16
        mov byte [step], 16
        jmp fail
.st_16:                    ; tiene que ser modo real
        sti
        SAY m_wreal                 ; DOS funciona igual en modo real
        cli
        mov ax, 1
        call far [cb]
        jnc .st_17
        mov byte [step], 17
        jmp fail
.st_17:
        sti
        smsw ax
        test al, 1
        jnz .st_18
        mov byte [step], 18
        jmp fail
.st_18:
        mov ax, 0x1606
        xor dx, dx
        int 0x2F
        mov ax, 0xDE00              ; y el monitor sigue ahí (VCPI)
        int 0x67
        test ah, ah
        jz .st_19
        mov byte [step], 19
        jmp fail
.st_19:
        SAY m_wok
        jmp done
.no:
        mov ax, 0x1606
        xor dx, dx
        int 0x2F
        SAY m_wno
        jmp done
.nocb:
        mov ax, 0x1606
        xor dx, dx
        int 0x2F
        SAY m_wnocb
        jmp done

; ---- INT 21h AX=5800h-5803h: estrategia de reserva y enlace de UMB ----
; Se encoge el bloque del programa y se dejan dos huecos (uno grande y uno
; justo) entre bloques ocupados: el primer ajuste toma el grande (el primero),
; el mejor el justo y el último sale del final de la memoria libre.
t_strat:
        mov ax, 0x5800
        int 0x21
        jc fail
        test ax, ax
        jnz fail                    ; por omisión, primer ajuste abajo
        mov bx, 0x1000              ; el programa se queda con 64 KB
        mov ah, 0x4A
        int 0x21
        jc fail
        mov bx, 0x100
        call .alloc
        mov [blk], ax               ; A: hueco grande
        mov bx, 0x10
        call .alloc
        mov [blk+2], ax             ; B
        mov bx, 0x40
        call .alloc
        mov [blk+4], ax             ; C: hueco justo
        mov bx, 0x10
        call .alloc
        mov [blk+6], ax             ; D
        mov es, [blk]
        mov ah, 0x49
        int 0x21
        jc fail
        mov es, [blk+4]
        mov ah, 0x49
        int 0x21
        jc fail
        ; primer ajuste: en A
        mov bx, 0
        call .strat
        mov bx, 0x40
        call .alloc
        cmp ax, [blk]
        jne fail
        call .free
        ; mejor ajuste: en C
        mov bx, 1
        call .strat
        mov ax, 0x5800
        int 0x21
        cmp ax, 1
        jne fail
        mov bx, 0x40
        call .alloc
        cmp ax, [blk+4]
        jne fail
        call .free
        ; último ajuste: por encima de D y pegado al final de la memoria
        mov bx, 2
        call .strat
        mov bx, 0xFFFF
        mov ah, 0x48
        int 0x21
        jnc fail
        mov [big], bx               ; el mayor hueco (el de detrás de D)
        mov bx, 0x40
        call .alloc
        cmp ax, [blk+6]
        jbe fail
        mov dx, ax
        mov bx, 0xFFFF
        push dx
        mov ah, 0x48
        int 0x21
        pop dx
        jnc fail
        mov ax, [big]
        sub ax, 0x41                ; el hueco ha perdido el bloque y su MCB
        cmp bx, ax
        jne fail
        mov ax, dx
        call .free
        ; estrategia no válida
        mov ax, 0x5801
        mov bx, 3
        int 0x21
        jnc fail
        mov ax, 0x5801
        mov bx, 0xC0
        int 0x21
        jnc fail
        mov bx, 0
        call .strat
        ; enlace de la memoria superior
        mov ax, 0x5802
        int 0x21
        jc fail
        test al, al
        jnz fail                    ; sin enlazar al empezar
        mov ax, 0x5803
        mov bx, 1
        int 0x21
        jc .noumb
        mov ax, 0x5802
        int 0x21
        cmp al, 1
        jne fail
        ; solo arriba (40h)
        mov bx, 0x40
        call .strat
        mov bx, 0x08
        mov ah, 0x48
        int 0x21
        jc .nofree                  ; (puede estar llena)
        cmp ax, 0xA000
        jb fail
        call .free
        ; arriba primero y, si no cabe, abajo (80h)
        mov bx, 0x80
        call .strat
        mov bx, 0x2000              ; 128 KB: arriba no caben
        call .alloc
        cmp ax, 0xA000
        jae fail
        call .free
.nofree:
        mov bx, 0
        call .strat
        mov ax, 0x5803
        xor bx, bx
        int 0x21
        jc fail
        mov ax, 0x5802
        int 0x21
        test al, al
        jnz fail
        ; sin enlazar, lo de arriba no se da
        mov bx, 0x08
        call .alloc
        cmp ax, 0xA000
        jae fail
        call .free
        call .freeall
        SAY m_strat_u
        jmp done
.noumb:
        call .freeall
        SAY m_strat
        jmp done
.freeall:
        mov ax, [blk+2]
        call .free
        mov ax, [blk+6]
        call .free
        ret
.alloc:
        mov ah, 0x48
        int 0x21
        jc fail
        ret
.free:
        push es
        mov es, ax
        mov ah, 0x49
        int 0x21
        pop es
        jc fail
        ret
.strat:
        mov ax, 0x5801
        int 0x21
        jc fail
        ret

done:
        mov ax, 0x4C00
        int 0x21
fail:
        sti
        SAY m_fail
        mov dl, [step]
        shr dl, 4
        add dl, 'A'
        mov ah, 2
        int 0x21
        mov dl, [step]
        and dl, 15
        add dl, 'A'
        mov ah, 2
        int 0x21
        SAY m_crlf
        mov ax, 0x4C01
        int 0x21
puts:
        lodsb
        test al, al
        jz .r
        mov dl, al
        mov ah, 2
        int 0x21
        jmp puts
.r:
        ret

m_use       db "EMMMISC A|H|C|P|W|S", 13, 10, 0
m_strat     db "Estrategia de memoria OK (sin UMB)", 13, 10, 0
m_strat_u   db "Estrategia de memoria OK (con UMB)", 13, 10, 0
blk         dw 0, 0, 0, 0
big         dw 0
m_a20       db "A20 virtual OK", 13, 10, 0
m_hlt       db "HLT OK", 13, 10, 0
m_cr        db "CR0 y DR OK", 13, 10, 0
m_pm        db "Intento de modo protegido...", 13, 10, 0
m_pmbad     db "LGDT no se ha cortado", 13, 10, 0
m_wreal     db "Windows: en modo real", 13, 10, 0
m_wok       db "Windows: modo V86 apagado y encendido OK", 13, 10, 0
m_wno       db "Windows: rechazado (CX distinto de 0)", 13, 10, 0
m_wnocb     db "Windows: sin rutina de modo V86", 13, 10, 0
m_fail      db "EMMMISC FALLO en el paso ", 0
m_crlf      db 13, 10, 0
step        db 0
was         db 0
hmaw        dw 0
cb          dd 0
gdtr        dw 0x17
            dd 0
