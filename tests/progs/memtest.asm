; MEMTEST.COM - prueba de la memoria convencional que recibe un programa y de
; la estrategia de asignación (INT 21h AX=5800h/5801h: primer ajuste, mejor
; ajuste y último ajuste). Lo usa tests/run_memprog_tests.sh.
; Escribe «LIBRE nnn KB» (lo que mide un programa al arrancar: su propio
; bloque, que es el mayor libre) y una línea «Sn OK» o «Sn FALLO» por prueba.
        cpu 386
        bits 16
        org 0x100

        ; lo primero, antes de llamar al DOS (que la abre): la línea A20
        push ds
        push es
        xor ax, ax
        mov ds, ax
        dec ax
        mov es, ax
        cli
        mov al, [0x500]
        mov ah, al
        not al
        mov [0x500], al
        cmp al, [es:0x510]
        mov [0x500], ah
        sti
        pop es
        pop ds
        je .a20c
        mov byte [a20st], 1
.a20c:
        ; el bloque del programa (el mayor libre al cargarlo), en KB
        mov ax, cs
        dec ax
        mov es, ax
        mov ax, [es:3]              ; párrafos
        shr ax, 6
        push ax
        mov si, m_libre
        call puts
        pop ax
        call pdec
        mov si, m_kb
        call puts
        push cs
        pop es
        ; cómo estaba la línea A20 al arrancar (0000:0500 y FFFF:0510 son el
        ; mismo octeto si está cerrada). Con el DOS en la HMA, a un programa
        ; cargado en los primeros 64 KB se le entrega cerrada (EXEPACK).
        mov si, m_a20c
        cmp byte [a20st], 0
        je .a20
        mov si, m_a20a
.a20:
        call puts
        ; quedarse con 64 KB y soltar el resto
        mov bx, 0x1000
        mov ah, 0x4A
        int 0x21
        jc near fallo_grave

        ; S1: al empezar, primer ajuste (0)
        mov ax, 0x5800
        int 0x21
        jc .f1
        test ax, ax
        jnz .f1
        mov al, '1'
        call ok
        jmp .s2
.f1:
        mov al, '1'
        call ko
.s2:
        ; S2: primer ajuste: A queda justo detrás del programa
        mov bx, 0x100
        call alloc
        mov [blk_a], ax
        mov bx, cs
        add bx, 0x1001
        cmp ax, bx
        mov al, '2'
        call res

        ; S3: último ajuste: B queda al final de la memoria (su MCB es «Z»)
        mov ax, 0x5801
        mov bx, 2
        int 0x21
        jc .f3
        mov ax, 0x5800
        int 0x21
        cmp ax, 2
        jne .f3
        mov bx, 0x100
        call alloc
        mov [blk_b], ax
        dec ax
        mov es, ax
        cmp byte [es:0], 'Z'
        jne .f3
        cmp word [es:3], 0x100
        jne .f3
        ; y lo que queda libre sigue entero entre A y B
        mov ax, [blk_a]
        add ax, 0x100               ; MCB del hueco
        mov es, ax
        cmp word [es:1], 0
        jne .f3
        add ax, [es:3]
        add ax, 2
        cmp ax, [blk_b]
        jne .f3
        mov al, '3'
        call ok
        jmp .s4
.f3:
        mov al, '3'
        call ko
.s4:
        push cs
        pop es
        ; S4: mejor ajuste. Se dejan dos huecos: uno de 100h párrafos (A) y,
        ; más arriba, otro de 40h (D); 30h párrafos caben mejor en D
        mov ax, 0x5801
        xor bx, bx
        int 0x21
        mov bx, 0x10
        call alloc                  ; C
        mov bx, 0x40
        call alloc                  ; D
        mov [blk_d], ax
        mov bx, 0x10
        call alloc                  ; E
        mov es, [blk_a]
        mov ah, 0x49
        int 0x21
        mov es, [blk_d]
        mov ah, 0x49
        int 0x21
        push cs
        pop es
        mov ax, 0x5801
        mov bx, 1
        int 0x21
        mov bx, 0x30
        call alloc
        cmp ax, [blk_d]
        mov al, '4'
        call res

        ; S5: de nuevo primer ajuste: 30h párrafos van al primer hueco (A)
        mov ax, 0x5801
        xor bx, bx
        int 0x21
        mov bx, 0x30
        call alloc
        cmp ax, [blk_a]
        mov al, '5'
        call res

        ; S6: una estrategia que no existe da error y no cambia la que hay
        mov ax, 0x5801
        mov bx, 7
        int 0x21
        jnc .f6
        mov ax, 0x5800
        int 0x21
        test ax, ax
        jnz .f6
        mov al, '6'
        call ok
        jmp .s7
.f6:
        mov al, '6'
        call ko
.s7:
        ; S7: último ajuste con un bloque que no cabe: error 8 y BX = mayor
        mov ax, 0x5801
        mov bx, 2
        int 0x21
        mov bx, 0xFFFF
        mov ah, 0x48
        int 0x21
        jnc .f7
        cmp ax, 8
        jne .f7
        cmp bx, 0x1000              ; (queda mucho más de 64 KB)
        jb .f7
        mov al, '7'
        call ok
        jmp .fin
.f7:
        mov al, '7'
        call ko
.fin:
        mov ax, 0x5801              ; dejarlo como estaba
        xor bx, bx
        int 0x21
        mov ax, 0x4C00
        int 0x21

fallo_grave:
        mov si, m_grave
        call puts
        mov ax, 0x4C01
        int 0x21

; BX = párrafos -> AX = segmento (0 si no hay)
alloc:
        mov ah, 0x48
        int 0x21
        jnc .r
        xor ax, ax
.r:
        ret

; ZF de la comparación anterior: OK o FALLO de la prueba AL
res:
        jne ko
ok:
        mov [m_ok+1], al
        mov si, m_ok
        jmp puts
ko:
        mov [m_ko+1], al
        mov si, m_ko
        jmp puts

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

; AX en decimal
pdec:
        xor cx, cx
        mov bx, 10
.d:
        xor dx, dx
        div bx
        push dx
        inc cx
        test ax, ax
        jnz .d
.p:
        pop dx
        add dl, '0'
        mov ah, 2
        int 0x21
        loop .p
        ret

m_libre     db "LIBRE ", 0
m_kb        db " KB", 13, 10, 0
a20st       db 0
m_a20c      db "A20 CERRADA", 13, 10, 0
m_a20a      db "A20 ABIERTA", 13, 10, 0
m_ok        db "S? OK", 13, 10, 0
m_ko        db "S? FALLO", 13, 10, 0
m_grave     db "MEMTEST: no se puede reducir el bloque", 13, 10, 0
blk_a       dw 0
blk_b       dw 0
blk_d       dw 0
