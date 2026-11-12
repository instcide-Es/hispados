; FSTEST.COM - los drivers de bloque no deben cambiar FS ni GS del programa.
; Uso: FSTEST fichero   (lo abre, lee 512 bytes y comprueba FS y GS)
        cpu 386
        bits 16
        org 0x100
        ; nombre del fichero: primer argumento
        mov si, 0x81
.sp:
        lodsb
        cmp al, ' '
        je .sp
        dec si
        mov dx, si
.e:
        lodsb
        cmp al, ' '
        ja .e
        mov byte [si-1], 0
        mov ax, 0x1234
        mov fs, ax
        mov ax, 0x5678
        mov gs, ax
        mov ax, 0x3D00
        int 0x21
        jc .nof
        mov bx, ax
        mov ah, 0x3F
        mov cx, 512
        mov dx, buf
        int 0x21
        jc .nof
        mov ah, 0x3E
        int 0x21
        mov ax, fs
        cmp ax, 0x1234
        jne .bad
        mov ax, gs
        cmp ax, 0x5678
        jne .bad
        mov dx, m_ok
        jmp .pr
.bad:
        mov dx, m_bad
        jmp .pr
.nof:
        mov dx, m_nof
.pr:
        mov ah, 9
        int 0x21
        mov ax, 0x4C00
        int 0x21
m_ok    db "FS y GS conservados", 13, 10, "$"
m_bad   db "FS o GS cambiados por la lectura", 13, 10, "$"
m_nof   db "FSTEST: no se puede leer el fichero", 13, 10, "$"
buf     times 512 db 0
