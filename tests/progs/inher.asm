; INHER.COM - el bit 7 del modo de 3Dh («no heredar»): el hijo no recibe ese
; handle; uno abierto sin él, sí.
;   INHER        padre: abre INHER.COM dos veces (80h y 00h) y se ejecuta a
;                sí mismo como hijo con los dos números de handle
;   INHER h1 h2  hijo: ¿son válidos? (4400h)
; No-inherit open flag.
        cpu 386
        bits 16
        org 0x100
        mov sp, pila_fin
        cmp byte [0x80], 0
        jne hijo
        mov bx, (fin - $$ + 0x100 + 15) / 16
        mov ah, 0x4A
        int 0x21
        mov ax, 0x3D80
        mov dx, nombre
        int 0x21
        jc mal
        add al, '0'
        mov [cola+2], al
        mov ax, 0x3D00
        mov dx, nombre
        int 0x21
        jc mal
        add al, '0'
        mov [cola+4], al
        mov [pb+4], cs
        mov ax, 0x4B00
        mov dx, nombre
        mov bx, pb
        int 0x21
        jc mal
        mov dx, m_padre
        jmp p
hijo:
        movzx bx, byte [0x82]
        sub bx, '0'
        mov ax, 0x4400
        int 0x21
        mov dx, m_h1no
        jc .b
        mov dx, m_h1si
.b:
        mov ah, 9
        int 0x21
        movzx bx, byte [0x84]
        sub bx, '0'
        mov ax, 0x4400
        int 0x21
        mov dx, m_h2no
        jc p
        mov dx, m_h2si
        jmp p
mal:
        mov dx, m_mal
p:
        mov ah, 9
        int 0x21
        mov ax, 0x4C00
        int 0x21
nombre  db "A:\INHER.COM", 0
cola    db 4, " x y", 13
pb      dw 0, cola, 0, 0, 0, 0, 0
m_padre db "INHER: padre fin", 13, 10, "$"
m_h1no  db "INHER: el de 80h no se hereda", 13, 10, "$"
m_h1si  db "INHER: el de 80h SE HEREDA", 13, 10, "$"
m_h2no  db "INHER: el de 00h NO se hereda", 13, 10, "$"
m_h2si  db "INHER: el de 00h se hereda", 13, 10, "$"
m_mal   db "INHER: fallo", 13, 10, "$"
pila    times 256 db 0
pila_fin:
fin:
