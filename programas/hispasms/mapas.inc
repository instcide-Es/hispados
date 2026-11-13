; =============================================================================
;  Hispa-System - los cartuchos: la ROM, el mapeador y la RAM con pila
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Escrito desde la documentación pública de los cartuchos de la consola:
;  - el mapeador de Sega: FFFDh, FFFEh y FFFFh eligen el banco de 16 KB de
;    0000h, 4000h y 8000h (el primer KB es siempre el del banco 0), y FFFCh
;    pone la RAM del cartucho (dos bancos de 16 KB, con pila en muchos) en
;    8000h-BFFFh: bit 3 encendida, bit 2 cuál de los dos. Esas cuatro
;    direcciones son también RAM de la consola (DFFCh-DFFFh);
;  - el de Codemasters: se escribe el banco en 0000h, 4000h u 8000h. Sus
;    juegos llevan en 7FE0h una cabecera con una suma y su contraria (7FE6h y
;    7FE8h): así se reconocen;
;  - una cabecera de 512 octetos delante (la de las copiadoras) se salta.
;  La memoria de la consola: 0000h-BFFFh el cartucho, C000h-DFFFh la RAM (8
;  KB) y E000h-FFFFh otra vez la RAM.
;
;    sms_tam_buf     EAX=tamaño del archivo -> EAX=lo que hay que reservar
;    sms_cargar      [rom]=la ROM leída en ese búfer, [rom_tam]=lo leído:
;                    quita la cabecera de copiadora, repite la ROM hasta una
;                    potencia de 2 y averigua el mapeador (map_forzar: 0
;                    averiguarlo, 1 Sega, 2 Codemasters)
;    sms_arrancar    enciende la consola (la memoria a cero, salvo la del
;                    cartucho: la pone quien llama, del .SAV)
;    sms_reiniciar   el botón de «reset»: el procesador y el mapeador
;    map_escribe     (lo llama el procesador) EDX=dirección AL=octeto
; =============================================================================

ROM_MIN     equ 0x4000

; EAX=tamaño -> EAX=búfer (una potencia de 2, 16 KB como poco, y 512 más)
sms_tam_buf:
        push ecx
        mov ecx, eax
        and ecx, 0x3FFF
        cmp ecx, 512
        jne .a
        sub eax, 512
.a:
        mov ecx, ROM_MIN
.b:
        cmp ecx, eax
        jae .c
        shl ecx, 1
        jmp .b
.c:
        lea eax, [ecx+512]
        pop ecx
        ret

sms_cargar:
        pushad
        mov eax, [rom_tam]
        mov ecx, eax
        and ecx, 0x3FFF
        cmp ecx, 512                ; la cabecera de las copiadoras
        jne .a
        sub eax, 512
        mov [rom_tam], eax
        add dword [rom], 512
.a:
        mov ecx, ROM_MIN            ; la potencia de 2
.b:
        cmp ecx, eax
        jae .c
        shl ecx, 1
        jmp .b
.c:
        mov edx, ecx
        shr edx, 14
        dec edx
        mov [rom_mask], edx
        mov esi, [rom]              ; lo que falta, repitiendo desde el principio
        mov edi, esi
        add edi, eax
        sub ecx, eax
        rep movsb
        ; el mapeador
        mov eax, [map_forzar]
        test eax, eax
        jnz .m
        mov eax, 1
        cmp dword [rom_tam], 0x8000
        jbe .m
        mov esi, [rom]
        movzx ecx, word [esi+0x7FE6]
        movzx edx, word [esi+0x7FE8]
        test ecx, ecx
        jz .m
        add ecx, edx
        test cx, cx
        jnz .m
        mov eax, 2
.m:
        mov [map_tipo], eax
        popad
        ret

sms_arrancar:
        pushad
        mov edi, ram
        mov ecx, 0x2000 / 4
        xor eax, eax
        rep stosd
        mov edi, vram
        mov ecx, (ram_fin - vram) / 4
        rep stosd
        mov edi, nada
        mov ecx, 0x400 / 4
        dec eax
        rep stosd
        mov byte [sram_usada], 0
        call z80_tablas
        call vdp_tablas
        mov dword [z_in], sms_in
        mov dword [z_out], sms_out
        call psg_reiniciar
        call sms_reiniciar
        popad
        ret

sms_reiniciar:
        pushad
        mov dword [map_reg], 0x02010000
        cmp dword [map_tipo], 2
        jne .a
        mov dword [map_reg], 0x00010000
.a:
        call map_pon
        call z80_reiniciar
        call vdp_reiniciar
        mov byte [io_ctl], 0xFF
        mov byte [mem_ctl], 0
        mov byte [pausa_ant], 0
        mov byte [nmi_pend], 0
        mov byte [irq_linea], 0
        popad
        ret

; la tabla de la memoria, según los registros del mapeador
map_pon:
        pushad
        mov eax, [map_tipo]
        test eax, eax
        jz near .x                  ; (memoria plana: la pone quien la usa)
        ; 0000h-3FFFh
        movzx eax, byte [map_reg+1]
        xor ebx, ebx
        call .rom16
        cmp dword [map_tipo], 2
        je .cm0
        mov eax, [rom]              ; el primer KB, siempre del banco 0
        mov [pg_lee], eax
        jmp .s1
.cm0:
        mov dword [pg_esc], 0
.s1:
        ; 4000h-7FFFh
        movzx eax, byte [map_reg+2]
        mov ebx, 16
        call .rom16
        cmp dword [map_tipo], 2
        jne .s2
        mov dword [pg_esc+16*4], 0
.s2:
        ; 8000h-BFFFh: ROM o la RAM del cartucho
        cmp dword [map_tipo], 2
        je .rom2
        mov al, [map_reg]
        test al, 0x08
        jz .rom2
        mov byte [sram_usada], 1
        mov eax, sram - 0x8000
        test byte [map_reg], 0x04
        jz .sr
        add eax, 0x4000
.sr:
        mov ebx, 32
.sr1:
        mov [pg_lee+ebx*4], eax
        mov [pg_esc+ebx*4], eax
        inc ebx
        cmp ebx, 48
        jb .sr1
        jmp .s3
.rom2:
        movzx eax, byte [map_reg+3]
        mov ebx, 32
        call .rom16
        cmp dword [map_tipo], 2
        jne .s3
        mov dword [pg_esc+32*4], 0
.s3:
        ; C000h-FFFFh: la RAM, dos veces
        mov eax, ram - 0xC000
        mov ebx, 48
.r:
        cmp ebx, 56
        jne .r1
        mov eax, ram - 0xE000
.r1:
        mov [pg_lee+ebx*4], eax
        mov [pg_esc+ebx*4], eax
        inc ebx
        cmp ebx, 64
        jb .r
        cmp dword [map_tipo], 1
        jne .s4
        mov dword [pg_esc+63*4], 0  ; (los registros del mapeador)
.s4:
        mov eax, [pg_lee]           ; la 64: la 0 otra vez
        sub eax, 0x10000
        mov [pg_lee+64*4], eax
.x:
        popad
        ret
; (interna) EAX=banco EBX=primer KB: 16 KB de ROM ahí (y escribir, a la basura)
.rom16:
        and eax, [rom_mask]
        shl eax, 14
        add eax, [rom]
        mov edx, ebx
        shl edx, 10
        sub eax, edx                ; [puntero + dirección] = el octeto
        mov ecx, 16
        mov esi, basura
        sub esi, edx
.r16:
        mov [pg_lee+ebx*4], eax
        mov [pg_esc+ebx*4], esi
        sub esi, 0x400
        inc ebx
        dec ecx
        jnz .r16
        ret

; EDX=dirección AL=octeto: lo que no va derecho a la memoria (conserva todo
; menos EBP)
map_escribe:
        cmp edx, 0xC000
        jae .ram
        cmp dword [map_tipo], 2     ; Codemasters: 0000h, 4000h u 8000h
        jne .x
        test edx, 0x3FFF
        jnz .x
        mov ebp, edx
        shr ebp, 14
        mov [map_reg+1+ebp], al
        jmp map_pon
.ram:
        mov ebp, edx
        and ebp, 0x1FFF
        mov [ram+ebp], al
        cmp edx, 0xFFFC
        jb .x
        cmp dword [map_tipo], 1
        jne .x
        mov ebp, edx
        sub ebp, 0xFFFC
        mov [map_reg+ebp], al
        jmp map_pon
.x:
        ret
