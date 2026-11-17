; =============================================================================
;  IRREAL.COM - prueba del modo «irreal» (segmento FS con límite de 4 GB en
;  modo real) a través de las interrupciones del reloj.
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Pone FS con límite de 4 GB (como HSPRED para los registros de las tarjetas
;  Intel y Realtek, y como muchos juegos), escribe unos puntos y espera unos
;  tics entre uno y otro (con HispaBIOS, cada tic con la pantalla cambiada es
;  un viaje al firmware para dibujarla) y luego lee por encima del primer
;  megabyte con FS. Si el límite de FS no ha sobrevivido, la lectura da un
;  fallo de protección y el equipo se reinicia; si todo va bien, escribe
;  «IRREAL-BIEN». Lo usa tests/run_hispabios_tests.sh.
; =============================================================================
        cpu 386
        bits 16
        org 0x100

start:
        ; la base lineal de la GDT
        xor eax, eax
        mov ax, cs
        shl eax, 4
        add eax, gdt
        mov [gdtr + 2], eax
        ; FS con límite de 4 GB
        cli
        lgdt [gdtr]
        mov eax, cr0
        or al, 1
        mov cr0, eax
        jmp short .pm
.pm:
        mov bx, 8
        mov fs, bx
        and al, 0xFE
        mov cr0, eax
        jmp short .rm
.rm:
        xor bx, bx
        mov fs, bx
        sti
        ; puntos y tics: la pantalla cambia y el reloj la dibuja
        mov cx, 8
.punto:
        mov dl, '.'
        mov ah, 0x02
        int 0x21
        push cx
        mov cx, 3
.tic:
        push es
        xor ax, ax
        mov es, ax
        mov bx, [es:0x46C]
.espera:
        cmp bx, [es:0x46C]
        je .espera
        pop es
        loop .tic
        pop cx
        loop .punto
        ; la lectura por encima de 1 MB con FS
        mov ebx, 0x00100010
        mov eax, [fs:ebx]
        mov dx, msg_bien
        mov ah, 0x09
        int 0x21
        mov ax, 0x4C00
        int 0x21

msg_bien    db 13, 10, "IRREAL-BIEN", 13, 10, "$"

            align 8
gdt         dq 0
            dq 0x008F92000000FFFF       ; 08h: datos, límite de 4 GB
gdtr        dw 15
            dd 0
