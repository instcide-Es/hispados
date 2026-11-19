; XMSHUECO.COM - con el mapa de E820SIM.SYS (huecos de la BIOS en 512 MB,
; 2 MB, y en 768 MB + 16 KB, 4 KB), reserva toda la memoria XMS en bloques de
; como mucho 48 MB, los bloquea y mira que ninguno caiga en un hueco; después
; un bloque que acaba justo en el hueco no puede crecer (no hay sitio en
; otro trozo). Para tests/run_xmshueco_tests.sh.
; Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
        cpu 386
        bits 16
        org 0x100
MAXB    equ 40
        mov ax, 0x4300
        int 0x2F
        cmp al, 0x80
        jne nox
        mov ax, 0x4310
        int 0x2F
        mov [xms], bx
        mov [xms+2], es
        push cs
        pop es
        ; libre al empezar
        mov ah, 0x88
        call far [xms]
        mov [libre], edx
        mov [mayor], eax
        mov dx, m_libre
        call puts
        mov eax, [libre]
        call dec32
        mov dx, m_mayor
        call puts
        mov eax, [mayor]
        call dec32
        mov dx, m_kb
        call puts
        ; reservar hasta que no quede
.bucle:
        cmp word [nb], MAXB
        jae .fin
        mov ah, 0x88
        call far [xms]
        test eax, eax
        jz .fin
        cmp eax, 49152
        jbe .t
        mov eax, 49152
.t:
        mov [tam], eax
        mov edx, eax
        mov ah, 0x89
        call far [xms]
        test ax, ax
        jz .fin
        mov bx, [nb]
        shl bx, 1
        mov [hs+bx], dx
        inc word [nb]
        mov ah, 0x0C
        call far [xms]
        test ax, ax
        jz .malb
        shl edx, 16
        mov dx, bx
        mov eax, [tam]
        add [suma], eax
        shl eax, 10
        add eax, edx                ; final
        ; ¿pisa [0x20000000, 0x20200000) o [0x30004000, 0x30005000)?
        mov ebx, 0x20000000
        mov ecx, 0x20200000
        call pisa
        mov ebx, 0x30004000
        mov ecx, 0x30005000
        call pisa
        jmp .bucle
.malb:
        mov byte [mal], 1
.fin:
        mov dx, m_n1
        call puts
        movzx eax, word [nb]
        call dec32
        mov dx, m_n2
        call puts
        mov eax, [suma]
        call dec32
        mov dx, m_kb
        call puts
        cmp byte [mal], 0
        jne .s1
        mov dx, m_bien
        call puts
.s1:
        mov eax, [suma]
        cmp eax, [libre]
        jne .s2
        mov dx, m_toda
        call puts
.s2:
        call libera
        ; el mayor bloque acaba justo en el hueco de 512 MB
        mov ah, 0x88
        call far [xms]
        mov [tam], eax
        mov edx, eax
        mov ah, 0x89
        call far [xms]
        test ax, ax
        jz .out
        mov [hs], dx
        mov word [nb], 1
        mov ah, 0x0C
        call far [xms]
        shl edx, 16
        mov dx, bx
        mov eax, [tam]
        shl eax, 10
        add eax, edx
        cmp eax, 0x20000000
        jne .s3
        mov dx, m_justo
        call puts
.s3:
        mov dx, [hs]
        mov ah, 0x0D
        call far [xms]
        mov dx, [hs]
        mov ebx, [tam]
        add ebx, 1024
        mov ah, 0x8F
        call far [xms]
        test ax, ax
        jnz .s4
        cmp bl, 0xA0
        jne .s4
        mov dx, m_nocrece
        call puts
.s4:
        call libera
.out:
        mov ax, 0x4C00
        int 0x21
nox:
        mov dx, m_nox
        call puts
        mov ax, 0x4C01
        int 0x21

; EDX = principio, EAX = final, [EBX, ECX) = hueco
pisa:
        cmp edx, ecx
        jae .r
        cmp eax, ebx
        jbe .r
        mov byte [mal], 1
        push eax
        push dx
        mov dx, m_pisa
        call puts
        pop dx
        mov eax, edx
        call hex8
        mov dx, m_crlf
        call puts
        pop eax
.r:
        ret

libera:
.l:
        cmp word [nb], 0
        je .r
        dec word [nb]
        mov bx, [nb]
        shl bx, 1
        mov dx, [hs+bx]
        push dx
        mov ah, 0x0D
        call far [xms]
        pop dx
        mov ah, 0x0A
        call far [xms]
        jmp .l
.r:
        ret

puts:
        mov ah, 9
        int 0x21
        ret
dec32:
        mov ebx, 10
        xor cx, cx
.l:
        xor edx, edx
        div ebx
        push dx
        inc cx
        test eax, eax
        jnz .l
.p:
        pop dx
        add dl, '0'
        mov ah, 2
        int 0x21
        loop .p
        ret
hex8:
        mov cx, 8
.l:
        rol eax, 4
        push eax
        and al, 15
        add al, '0'
        cmp al, '9'
        jbe .d
        add al, 7
.d:
        mov dl, al
        mov ah, 2
        int 0x21
        pop eax
        loop .l
        ret

m_nox    db "XMSHUECO: no hay gestor XMS", 13, 10, "$"
m_libre  db "XMSHUECO: libre $"
m_mayor  db " KB, mayor bloque $"
m_kb     db " KB", 13, 10, "$"
m_n1     db "XMSHUECO: $"
m_n2     db " bloques, $"
m_bien   db "XMSHUECO: ning", 0xA3, "n bloque en los huecos de la BIOS", 13, 10, "$"
m_toda   db "XMSHUECO: se ha podido reservar toda la memoria libre", 13, 10, "$"
m_justo  db "XMSHUECO: el mayor bloque acaba justo en el hueco de 512 MB", 13, 10, "$"
m_nocrece db "XMSHUECO: ese bloque no crece dentro del hueco", 13, 10, "$"
m_pisa   db "XMSHUECO: bloque en un hueco de la BIOS en $"
m_crlf   db 13, 10, "$"
xms      dd 0
libre    dd 0
mayor    dd 0
tam      dd 0
suma     dd 0
nb       dw 0
mal      db 0
hs       times MAXB dw 0
