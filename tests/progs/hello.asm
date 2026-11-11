; HELLO.COM - programa de prueba: saluda y muestra su cola de órdenes
        bits 16
        org 0x100
        mov dx, msg
        mov ah, 9
        int 0x21
        ; cola de órdenes (PSP:80h) por el handle 1
        movzx cx, byte [0x80]
        mov dx, 0x81
        mov bx, 1
        mov ah, 0x40
        int 0x21
        mov dx, nl
        mov ah, 9
        int 0x21
        mov ax, 0x4C07          ; código de retorno 7
        int 0x21
msg     db "Hello from a .COM program! Args:[$"
nl      db "]", 13, 10, "$"
