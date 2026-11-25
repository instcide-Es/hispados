; =============================================================================
;  Hispa-8086 - el sector de particiones del disco duro (MBR)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Es el primer sector del disco duro: la BIOS lo carga en 0000:7C00. Se
;  aparta a 0000:0600, busca en la tabla de particiones (detrás del código,
;  en el octeto 446) la que está marcada como activa, carga su primer sector
;  en 0000:7C00 y salta a él con DL = la unidad y SI = su fila de la tabla,
;  como hace el de MS-DOS.
;
;  Aquí solo va el código (446 octetos): la tabla y la firma las pone y las
;  conserva quien lo instala (PARTIR y SISTEMA).
;  Solo instrucciones del 8086.
; =============================================================================
        cpu 8086
        bits 16
        org 0x0600

TABLA       equ 0x0600 + 446

inicio:
        cli
        xor ax, ax
        mov ss, ax
        mov sp, 0x7C00
        mov ds, ax
        mov es, ax
        sti
        cld
        mov si, 0x7C00              ; a 0000:0600, para dejar sitio al sector
        mov di, 0x0600              ; de arranque de la partición
        mov cx, 256
        rep movsw
        jmp 0:sigue
sigue:
        mov si, TABLA
        mov cx, 4
.b:
        cmp byte [si], 0x80         ; ¿la activa?
        je .esta
        add si, 16
        loop .b
        mov si, m_no_activa
        jmp short error
.esta:
        mov di, 3                   ; tres intentos
.lee:
        mov dh, [si+1]              ; su primer sector: cara, sector y cilindro
        mov cx, [si+2]
        mov bx, 0x7C00
        mov ax, 0x0201
        int 0x13
        jnc .leido
        xor ax, ax
        int 0x13
        dec di
        jnz .lee
        mov si, m_disco
        jmp short error
.leido:
        cmp word [0x7C00 + 510], 0xAA55
        jne .sin
        jmp 0:0x7C00
.sin:
        mov si, m_sin_sistema
; escribe el mensaje de DS:SI, espera una tecla y vuelve a arrancar
error:
        lodsb
        test al, al
        jz .fin
        mov ah, 0x0E
        mov bx, 7
        int 0x10
        jmp error
.fin:
        xor ax, ax
        int 0x16
        int 0x19

m_no_activa db "No hay partición activa", 0
m_disco     db "Error de disco", 0
m_sin_sistema db "La partición no tiene sistema", 0

        times 446 - ($ - $$) db 0
