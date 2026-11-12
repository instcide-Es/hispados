; NUCDRV.SYS - controlador de prueba: en INIT escribe un rótulo con INT 21h
; y DESPUÉS lee su línea de CONFIG.SYS de la petición (como hacen muchos
; controladores). Si la consola es otro controlador (NUCCON.SYS), el núcleo
; no debe estropear la petición INIT al escribir el rótulo.
; Test driver: prints a banner via INT 21h, then reads its command line.
        cpu 386
        bits 16
        org 0
cab:
        dd 0xFFFFFFFF
        dw 0x8000
        dw estrategia
        dw interrupcion
        db "NUCDRV$ "
peticion dd 0

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
residente:

init:
        mov dx, rotulo
        mov ah, 9
        int 0x21
        ; ahora, la línea de órdenes (ES:BX+12h)
        les bx, [peticion]
        push ds
        lds si, [es:bx+0x12]
        mov cx, 40
.l:
        lodsb
        cmp al, 32
        jb .e
        mov dl, al
        mov ah, 2
        int 0x21
        loop .l
.e:
        pop ds
        mov dx, crlf
        mov ah, 9
        int 0x21
        les bx, [peticion]
        mov word [es:bx+0x0E], residente
        mov [es:bx+0x10], cs
        mov word [es:bx+3], 0x0100
        jmp fin
rotulo  db "NUCDRV: $"
crlf    db 13, 10, "$"
