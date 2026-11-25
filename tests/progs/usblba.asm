; USBLBA.COM - INT 25h en la memoria USB de HSPUSB (X:): el sector 0 se lee;
; uno que con el comienzo de la partición (2048) da la vuelta a los 32 bits
; (FFFFF800h) tiene que fallar, no leer el MBR del disco.
; HSPUSB: LBA wrap-around on the DOS path.
        cpu 386
        bits 16
        org 0x100
        mov dword [pk_sec], 0
        call lee
        mov dx, m_0bad
        jc .a
        cmp word [buf+510], 0xAA55
        jne .a
        mov dx, m_0ok
.a:
        mov ah, 9
        int 0x21
        mov word [buf+510], 0
        mov dword [pk_sec], 0xFFFFF800
        call lee
        mov dx, m_wok
        jc .b
        mov dx, m_wbad
        cmp word [buf+510], 0xAA55
        je .b
        mov dx, m_wraro
.b:
        mov ah, 9
        int 0x21
        mov ax, 0x4C00
        int 0x21
lee:
        mov [pk_seg], ds
        mov al, 23                  ; X:
        mov cx, 0xFFFF
        mov bx, pk
        int 0x25
        pop dx                      ; (las banderas que deja INT 25h)
        ret
pk:
pk_sec  dd 0
        dw 1
        dw buf
pk_seg  dw 0
m_0ok   db "USBLBA: sector 0 leido", 13, 10, "$"
m_0bad  db "USBLBA: sector 0 NO leido", 13, 10, "$"
m_wok   db "USBLBA: FFFFF800h rechazado", 13, 10, "$"
m_wbad  db "USBLBA: FFFFF800h ha leido el MBR", 13, 10, "$"
m_wraro db "USBLBA: FFFFF800h aceptado", 13, 10, "$"
buf     times 512 db 0
