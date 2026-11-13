; HMAUSO.SYS - gestor XMS de pega para tests/run_hma_tests.sh: contesta a
; INT 2Fh 4300h/4310h como un gestor XMS, pero la HMA ya «está en uso»
; (función 01h -> AX=0, BL=91h). Con DOS=HIGH el núcleo tiene que decirlo y
; quedarse en la memoria convencional.
; Con /M (miente): da la HMA y dice que la línea A20 está abierta, pero la
; ha cerrado (puerto 92h). El núcleo no se lo puede creer: copiar el código
; a FFFF:0010 pisaría la tabla de interrupciones.
; Fake XMS manager whose HMA is always in use.
        cpu 386
        bits 16
        org 0
cab:
        dd 0xFFFFFFFF
        dw 0x8000
        dw estrategia
        dw interrupcion
        db "HMAUSO$ "
peticion dd 0
old2f    dd 0
miente   db 0

estrategia:
        mov [cs:peticion], bx
        mov [cs:peticion+2], es
        retf

interrupcion:
        pushad
        push ds
        push es
        push cs
        pop ds
        les bx, [peticion]
        cmp byte [es:bx+2], 0
        je init
        mov word [es:bx+3], 0x0100
fin:
        pop es
        pop ds
        popad
        retf

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
        mov bx, entrada
        iret
.n2:
        jmp far [cs:old2f]

entrada:
        jmp short .go
        nop
        nop
        nop
.go:
        test ah, ah
        jnz .n0
        mov ax, 0x0300              ; versión
        mov bx, 0x0350
        mov dx, 1                   ; hay HMA
        retf
.n0:
        cmp byte [cs:miente], 0
        je .n00
        cmp ah, 0x07                ; /M: todo sale «bien» (01h, 02h, 05h-07h)
        ja .n1
        mov ax, 1
        xor bl, bl
        retf
.n00:
        cmp ah, 0x01                ; pedir la HMA: ya está en uso
        jne .n1
        xor ax, ax
        mov bl, 0x91
        retf
.n1:
        xor ax, ax                  ; lo demás: función no implementada
        mov bl, 0x80
        retf
residente:

init:
        mov ax, 0x352F
        int 0x21
        mov [old2f], bx
        mov [old2f+2], es
        mov dx, int2f
        mov ax, 0x252F
        int 0x21
        mov dx, rotulo
        mov ah, 9
        int 0x21
        ; ¿/M en la línea de CONFIG.SYS?
        les bx, [peticion]
        push ds
        lds si, [es:bx+0x12]
        mov cx, 60
.sw:
        lodsb
        cmp al, 32
        jb .nsw
        cmp al, '/'
        jne .sw1
        mov al, [si]
        and al, 0xDF
        cmp al, 'M'
        jne .sw1
        mov byte [cs:miente], 1
.sw1:
        loop .sw
.nsw:
        pop ds
        cmp byte [miente], 0
        je .nm
        in al, 0x92                 ; cerrar A20
        and al, 0xFC
        out 0x92, al
        mov dx, rotulo2
        mov ah, 9
        int 0x21
.nm:
        les bx, [peticion]
        mov word [es:bx+0x0E], residente
        mov [es:bx+0x10], cs
        mov word [es:bx+3], 0x0100
        jmp fin
rotulo  db "HMAUSO: gestor XMS de prueba con la HMA en uso", 13, 10, "$"
rotulo2 db "HMAUSO /M: da la HMA con la linea A20 cerrada", 13, 10, "$"
