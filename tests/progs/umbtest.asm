; UMBTEST.COM - memoria superior por XMS (funciones 10h, 11h y 12h): pedir
; un bloque, escribir y leer en él, encogerlo, liberarlo y los errores.
; Dice también dónde está cargado el propio programa (para LH/LOADHIGH).
        cpu 386
        bits 16
        org 0x100

%macro SAY 1
        mov si, %1
        call puts
%endmacro

        SAY m_psp
        mov ax, cs
        call hex4
        mov ax, cs
        cmp ax, 0xA000
        jb .low
        SAY m_high
.low:
        SAY m_crlf
        mov ax, 0x4300
        int 0x2F
        cmp al, 0x80
        jne noxms
        mov ax, 0x4310
        int 0x2F
        mov [xms], bx
        mov [xms+2], es
        ; el mayor bloque
        mov ah, 0x10
        mov dx, 0xFFFF
        call far [xms]
        test ax, ax
        jnz fail                    ; no puede haber 1 MB
        cmp bl, 0xB1
        je none
        cmp bl, 0xB0
        jne fail
        mov [largest], dx
        cmp dx, 0x40
        jb none
        ; pedir 1 KB
        mov ah, 0x10
        mov dx, 0x40
        call far [xms]
        cmp ax, 1
        jne fail
        cmp bx, 0xA000
        jb fail
        cmp dx, 0x40
        jb fail
        mov [seg1], bx
        SAY m_seg
        mov ax, [seg1]
        call hex4
        SAY m_crlf
        ; escribir y leer
        mov es, [seg1]
        xor di, di
        mov cx, 0x200
        mov ax, 0x5AA5
        rep stosw
        xor di, di
        mov cx, 0x200
        repe scasw
        jne fail
        ; encoger a 512 bytes y pedir otro
        mov ah, 0x12
        mov dx, [seg1]
        mov bx, 0x20
        call far [xms]
        cmp ax, 1
        jne fail
        mov ah, 0x10
        mov dx, 0x10
        call far [xms]
        cmp ax, 1
        jne fail
        mov [seg2], bx
        cmp bx, [seg1]
        je fail
        ; liberar los dos: vuelve todo
        mov ah, 0x11
        mov dx, [seg1]
        call far [xms]
        cmp ax, 1
        jne fail
        mov ah, 0x11
        mov dx, [seg2]
        call far [xms]
        cmp ax, 1
        jne fail
        mov ah, 0x11
        mov dx, [seg2]
        call far [xms]
        test ax, ax
        jnz fail
        cmp bl, 0xB2
        jne fail
        mov ah, 0x10
        mov dx, 0xFFFF
        call far [xms]
        cmp dx, [largest]
        jne fail
        SAY m_ok
        mov ax, 0x4C00
        int 0x21
none:
        SAY m_none
        mov ax, 0x4C03
        int 0x21
noxms:
        SAY m_noxms
        mov ax, 0x4C02
        int 0x21
fail:
        push bx
        SAY m_fail
        pop ax
        call hex4
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
hex4:
        mov cx, 4
.l:
        rol ax, 4
        push ax
        and al, 0x0F
        add al, '0'
        cmp al, '9'
        jbe .p
        add al, 7
.p:
        mov dl, al
        mov ah, 2
        int 0x21
        pop ax
        loop .l
        ret

m_psp       db "UMBTEST cargado en ", 0
m_high      db " (memoria superior)", 0
m_seg       db "UMB en ", 0
m_ok        db "UMB test OK", 13, 10, 0
m_none      db "UMB: no hay memoria superior libre", 13, 10, 0
m_noxms     db "UMB: no hay gestor XMS", 13, 10, 0
m_fail      db "UMB FALLO BX=", 0
m_crlf      db 13, 10, 0
xms         dd 0
largest     dw 0
seg1        dw 0
seg2        dw 0
