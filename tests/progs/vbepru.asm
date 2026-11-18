; =============================================================================
;  VBEPRU.COM - prueba de VBE (INT 10h AX = 4Fxxh) como lo usa Hispa Ventanas
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Para HispaBIOS (docs/HISPABIOS.md), que da VBE 2.0 sobre la pantalla del
;  firmware UEFI con un marco lineal de memoria normal por debajo de 4 GB.
;  Pregunta por la tarjeta (4F00h), busca en su lista un modo de 800x600 de
;  32 bits con marco lineal (4F01h), lo pone (4F02h), comprueba el modo puesto
;  (4F03h), escribe y lee el marco (con FS en modo «irreal»), deja pasar unos
;  tics (HispaBIOS lleva el marco a la pantalla en cada tic), pide un modo de
;  la VGA que con UEFI no hay (12h) y vuelve al texto. Escribe «VBEPRU-BIEN»
;  o «VBEPRU-MAL n» (n = el paso que falla). Lo usa
;  tests/run_hispabios_tests.sh.
; =============================================================================
        cpu 386
        bits 16
        org 0x100

start:
        ; FS con límite de 4 GB (modo «irreal»)
        xor eax, eax
        mov ax, cs
        shl eax, 4
        add eax, gdt
        mov [gdtr + 2], eax
        cli
        lgdt [gdtr]
        mov eax, cr0
        or al, 1
        mov cr0, eax
        jmp short .pm
.pm:    mov bx, 8
        mov fs, bx
        and al, 0xFE
        mov cr0, eax
        jmp short .rm
.rm:    xor bx, bx
        mov fs, bx
        sti

        mov byte [paso], '1'            ; 4F00h: la tarjeta
        mov dword [info], 'VBE2'
        mov ax, 0x4F00
        mov di, info
        int 0x10
        cmp ax, 0x004F
        jne mal
        cmp dword [info], 'VESA'
        jne mal
        cmp word [info + 4], 0x0200
        jb mal

        mov byte [paso], '2'            ; 4F01h: un modo de 800x600 de 32 bits
        les si, [info + 0x0E]
.modo:  mov cx, [es:si]
        add si, 2
        cmp cx, 0xFFFF
        je mal
        push es
        push si
        push cx
        push ds
        pop es
        mov ax, 0x4F01
        mov di, minfo
        int 0x10
        pop cx
        pop si
        pop es
        cmp ax, 0x004F
        jne .modo
        mov ax, [minfo]
        and ax, 0x0091                  ; hay, gráfico, con marco lineal
        cmp ax, 0x0091
        jne .modo
        cmp word [minfo + 0x12], 800
        jne .modo
        cmp word [minfo + 0x14], 600
        jne .modo
        cmp byte [minfo + 0x19], 32
        jne .modo
        cmp byte [minfo + 0x1B], 6      ; color directo
        jne .modo
        cmp dword [minfo + 0x1F], 0x08081008    ; rojo 8 en 16, verde 8 en 8
        jne .modo
        cmp word [minfo + 0x23], 0x0008         ; azul 8 en 0
        jne .modo
        cmp word [minfo + 0x10], 3200   ; octetos por línea
        jne .modo
        mov [modo], cx
        mov eax, [minfo + 0x28]
        test eax, eax
        jz mal
        mov [lfb], eax
        push ds
        pop es

        mov byte [paso], '3'            ; 4F02h: ponerlo, con marco lineal
        mov ax, 0x4F02
        mov bx, [modo]
        or bh, 0x40
        int 0x10
        cmp ax, 0x004F
        jne mal_texto

        mov byte [paso], '4'            ; 4F03h: el modo puesto
        mov ax, 0x4F03
        int 0x10
        cmp ax, 0x004F
        jne mal_texto
        mov ax, [modo]
        or ah, 0x40
        cmp bx, ax
        jne mal_texto

        mov byte [paso], '5'            ; el marco: recién puesto, a cero
        mov ebx, [lfb]
        cmp dword [fs:ebx], 0
        jne mal_texto
        cmp dword [fs:ebx + 800 * 600 * 4 - 4], 0
        jne mal_texto
        ; unas franjas de colores (para la captura) y lo que se lee
        xor ecx, ecx
.pinta: mov eax, ecx
        shr eax, 13                     ; cambia cada 8192 puntos
        and eax, 7
        mov eax, [colores + eax * 4]
        mov [fs:ebx + ecx * 4], eax
        inc ecx
        cmp ecx, 800 * 600
        jb .pinta
        mov byte [paso], '6'
        mov eax, [colores]
        cmp [fs:ebx], eax
        jne mal_texto
        mov eax, [colores + 2 * 4]      ; el último punto: franja 58 -> color 2
        cmp [fs:ebx + 800 * 600 * 4 - 4], eax
        jne mal_texto
        ; unos tics: HispaBIOS lleva el marco a la pantalla
        mov byte [paso], '7'
        xor ah, ah
        int 0x1A
        mov si, dx
.tic:   xor ah, ah
        int 0x1A
        sub dx, si
        cmp dx, 6
        jb .tic
        mov ebx, [lfb]                  ; (FS sigue valiendo tras los viajes)
        mov eax, [colores]
        cmp [fs:ebx], eax
        jne mal_texto

        mov byte [paso], '8'            ; un modo de la VGA que no hay: texto
        mov ax, 0x0012
        int 0x10
        mov ax, 0x0003
        int 0x10
        mov ax, 0x4F03
        int 0x10
        cmp bx, 0x0003
        jne mal
        mov dx, m_bien
        mov ah, 9
        int 0x21
        mov ax, 0x4C00
        int 0x21

mal_texto:
        mov ax, 0x0003
        int 0x10
mal:    push cs
        pop ds
        mov dx, m_mal
        mov ah, 9
        int 0x21
        mov ax, 0x4C01
        int 0x21

m_bien  db 'VBEPRU-BIEN', 13, 10, '$'
m_mal   db 'VBEPRU-MAL '
paso    db '0', 13, 10, '$'
        align 4
colores dd 0x00C8502D, 0x002D8CC8, 0x00F0C83C, 0x003CA050, 0x00FFFFFF, 0x00804020, 0x00205080, 0x00A0A0A0
modo    dw 0
lfb     dd 0
gdtr    dw 15
        dd 0
gdt     dq 0
        dq 0x008F92000000FFFF           ; datos, límite de 4 GB
info    times 512 db 0
minfo   times 256 db 0
