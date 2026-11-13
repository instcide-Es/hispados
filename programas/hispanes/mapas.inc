; =============================================================================
;  Hispa-Nes - los cartuchos: qué memoria se ve en cada sitio
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Escrito desde la documentación pública de cada circuito de cartucho (lo
;  que en el formato iNES se llama «mapper»). Están:
;     0  sin cambio de bancos (programa de 16 o 32 KB, dibujos de 8 KB)
;     1  registro en serie de 5 bits: bancos de programa de 16 KB y de
;        dibujos de 4 KB, y el espejo de las tablas de nombres
;     2  banco de programa de 16 KB en 8000h (el último, fijo en C000h)
;     3  banco de dibujos de 8 KB
;     4  bancos de programa de 8 KB y de dibujos de 1 y 2 KB, espejo y
;        contador de líneas con interrupción
;     7  banco de programa de 32 KB y una sola tabla de nombres
;    66  banco de programa de 32 KB y de dibujos de 8 KB
;
;    nes_cabecera    ESI=los 16 octetos de la cabecera iNES: rellena rom_* ->
;                    CF=1 si no es una ROM o el cartucho no está (EAX=número)
;    nes_arrancar    con la ROM ya en rom_prg y rom_chr: enciende la consola
;    nes_reset       el botón de «reset»
;    map_escribir    (puntero) AL -> EDX, de 8000h en adelante
;    map_linea       (puntero) una línea de imagen más (para el 4)
;  Lo que usan los cartuchos:
;    prg_8k          EAX=trozo (0-3: 8000h, A000h, C000h, E000h) EDX=banco de 8 KB
;    prg_16k         EAX=0 (8000h) o 1 (C000h), EDX=banco de 16 KB
;    chr_1k          EAX=trozo (0-7) EDX=banco de 1 KB
;    chr_nk          EAX=primer trozo ECX=cuántos EDX=primer banco de 1 KB
;    espejo          EAX=0 horizontal, 1 vertical, 2 solo la primera tabla,
;                    3 solo la segunda, 4 las cuatro
;  Los bancos se cuentan desde 0 y dan la vuelta si se piden más de los que
;  hay (los negativos cuentan desde el final: -1 es el último).
; =============================================================================

ESP_H       equ 0
ESP_V       equ 1
ESP_UNA0    equ 2
ESP_UNA1    equ 3
ESP_CUATRO  equ 4

        align 4
map_lista   dd 0, m0_ini, 1, m1_ini, 2, m2_ini, 3, m3_ini, 4, m4_ini, 7, m7_ini, 66, m66_ini
MAP_N       equ ($ - map_lista) / 8
esp_tabla   db 0, 0, 1, 1,   0, 1, 0, 1,   0, 0, 0, 0,   1, 1, 1, 1,   0, 1, 2, 3

; ESI=cabecera iNES -> CF=1 si no vale (EAX=0 no es una ROM, EAX=1 cartucho
; que no está: su número, en rom_mapa)
nes_cabecera:
        push ebx
        push ecx
        xor eax, eax
        cmp dword [esi], 0x1A53454E ; "NES", 1Ah
        jne .mal
        movzx ebx, byte [esi+4]     ; programa, en bancos de 16 KB
        test ebx, ebx
        jz .mal
        shl ebx, 1
        mov [rom_prg_n], ebx        ; (aquí, de 8 KB)
        movzx ebx, byte [esi+5]     ; dibujos, en bancos de 8 KB (0: son RAM)
        shl ebx, 3
        mov [rom_chr_n], ebx        ; (aquí, de 1 KB)
        movzx ebx, byte [esi+6]
        mov ecx, ebx
        and ecx, 1
        test bl, 8
        jz .e
        mov ecx, ESP_CUATRO
.e:
        mov [rom_espejo], ecx
        mov ecx, ebx
        shr ecx, 1
        and ecx, 1
        mov [rom_bateria], ecx
        mov ecx, ebx
        shr ecx, 2
        and ecx, 1
        mov [rom_entren], ecx       ; 512 octetos delante del programa
        shr ebx, 4
        movzx ecx, byte [esi+7]
        cmp dword [esi+12], 0       ; las cabeceras antiguas tienen basura
        jne .m                      ; detrás: entonces solo vale el octeto 6
        and ecx, 0xF0
        or ebx, ecx
.m:
        mov [rom_mapa], ebx
        xor ecx, ecx
.b:
        cmp [map_lista+ecx*8], ebx
        je .hay
        inc ecx
        cmp ecx, MAP_N
        jb .b
        mov eax, 1
.mal:
        stc
        jmp .x
.hay:
        mov eax, [map_lista+ecx*8+4]
        mov [map_ini], eax
        clc
.x:
        pop ecx
        pop ebx
        ret

; enciende la consola (la ROM, ya cargada)
nes_arrancar:
        pushad
        call ppu_tablas
        mov edi, ram                ; la memoria, a cero
        mov ecx, ram_fin - ram
        xor al, al
        rep stosb
        cmp dword [rom_chr_n], 0
        setz byte [chr_es_ram]
        jnz .chr
        mov dword [rom_chr], chr_ram
        mov dword [rom_chr_n], 8
.chr:
        mov dword [pg], ram
        mov dword [pg+4], nada8k - 0x2000
        mov dword [pg+8], nada8k - 0x4000
        mov dword [pg+12], sram - 0x6000
        mov dword [pg+32], ram - 0x10000
        call nes_reset
        popad
        ret

nes_reset:
        pushad
        mov dword [map_escribir], map_nada
        mov dword [map_linea], map_nada
        and byte [irq_linea], ~4
        mov eax, [rom_espejo]
        call espejo
        call [map_ini]
        call ppu_reiniciar
        call apu_reiniciar
        call cpu_reiniciar
        popad
        ret

map_nada:
        ret

; ---- lo que usan los cartuchos ----
prg_8k:
        push eax
        push edx
        push ecx
        mov ecx, eax
        mov eax, edx
        test eax, eax
        jns .p
        add eax, [rom_prg_n]
.p:
        xor edx, edx
        div dword [rom_prg_n]
        shl edx, 13
        add edx, [rom_prg]
        mov eax, ecx
        shl eax, 13
        add eax, 0x8000
        sub edx, eax
        mov [pg+ecx*4+16], edx
        pop ecx
        pop edx
        pop eax
        ret

prg_16k:
        push eax
        push edx
        shl eax, 1
        shl edx, 1
        call prg_8k
        inc eax
        inc edx
        call prg_8k
        pop edx
        pop eax
        ret

chr_1k:
        push eax
        push edx
        push ecx
        mov ecx, eax
        mov eax, edx
        xor edx, edx
        div dword [rom_chr_n]
        shl edx, 10
        add edx, [rom_chr]
        mov [chr_pg+ecx*4], edx
        pop ecx
        pop edx
        pop eax
        ret

chr_nk:
        push eax
        push ecx
        push edx
.b:
        call chr_1k
        inc eax
        inc edx
        dec ecx
        jnz .b
        pop edx
        pop ecx
        pop eax
        ret

espejo:
        pushad
        lea esi, [esp_tabla+eax*4]
        xor ecx, ecx
.b:
        movzx eax, byte [esi+ecx]
        shl eax, 10
        add eax, nt_ram
        mov [nt_pg+ecx*4], eax
        inc ecx
        cmp ecx, 4
        jb .b
        popad
        ret

; (interna) el programa entero a la vista (16 KB repetidos, o los primeros
; 32) y los ocho primeros KB de dibujos
todo_fijo:
        pushad
        xor eax, eax
        xor edx, edx
.p:
        call prg_8k
        inc eax
        inc edx
        cmp eax, 4
        jb .p
        xor eax, eax
        xor edx, edx
        mov ecx, 8
        call chr_nk
        popad
        ret

; ---- 0: nada que cambiar ----
m0_ini:
        jmp todo_fijo

; ---- 2: el banco de 16 KB de 8000h ----
m2_ini:
        call todo_fijo
        mov eax, 1
        mov edx, -1
        call prg_16k
        mov dword [map_escribir], .w
        ret
.w:
        push eax
        push edx
        movzx edx, al
        xor eax, eax
        call prg_16k
        pop edx
        pop eax
        ret

; ---- 3: el banco de dibujos de 8 KB ----
m3_ini:
        call todo_fijo
        mov dword [map_escribir], .w
        ret
.w:
        pushad
        movzx edx, al
        shl edx, 3
        xor eax, eax
        mov ecx, 8
        call chr_nk
        popad
        ret

; ---- 7: 32 KB de programa y una sola tabla de nombres ----
m7_ini:
        call todo_fijo
        mov dword [map_escribir], .w
        xor eax, eax
.w:
        pushad
        movzx ebx, al
        mov edx, ebx
        and edx, 7
        shl edx, 1
        xor eax, eax
        call prg_16k
        inc eax
        inc edx
        call prg_16k
        mov eax, ebx
        shr eax, 4
        and eax, 1
        add eax, ESP_UNA0
        call espejo
        popad
        ret

; ---- 66: 32 KB de programa y 8 KB de dibujos ----
m66_ini:
        call todo_fijo
        mov dword [map_escribir], .w
        ret
.w:
        pushad
        movzx ebx, al
        mov edx, ebx
        shr edx, 4
        and edx, 3
        shl edx, 1
        xor eax, eax
        call prg_16k
        inc eax
        inc edx
        call prg_16k
        mov edx, ebx
        and edx, 3
        shl edx, 3
        xor eax, eax
        mov ecx, 8
        call chr_nk
        popad
        ret

; ---- 1: registro en serie ----
m1_ini:
        mov dword [m1_serie], 0
        mov dword [m1_cuenta], 0
        mov dword [m1_reg], 0x0C    ; control: último banco fijo en C000h
        mov dword [m1_reg+4], 0
        mov dword [m1_reg+8], 0
        mov dword [m1_reg+12], 0
        mov dword [map_escribir], .w
        jmp m1_pone
.w:
        pushad
        test al, 0x80
        jz .bit
        mov dword [m1_serie], 0     ; bit 7: empezar de nuevo
        mov dword [m1_cuenta], 0
        or dword [m1_reg], 0x0C
        call m1_pone
        popad
        ret
.bit:
        and eax, 1
        mov ecx, [m1_cuenta]
        shl eax, cl
        or [m1_serie], eax
        inc ecx
        mov [m1_cuenta], ecx
        cmp ecx, 5
        jb .x
        shr edx, 13                 ; el quinto: al registro que diga la dirección
        and edx, 3
        mov eax, [m1_serie]
        mov [m1_reg+edx*4], eax
        mov dword [m1_serie], 0
        mov dword [m1_cuenta], 0
        call m1_pone
.x:
        popad
        ret
m1_pone:
        pushad
        mov ebx, [m1_reg]           ; control
        mov eax, ebx
        and eax, 3
        movzx eax, byte [.esp+eax]
        cmp dword [rom_espejo], ESP_CUATRO
        je .se
        call espejo
.se:
        ; los dibujos: 8 KB o dos de 4 KB
        test bl, 0x10
        jnz .c4
        mov edx, [m1_reg+4]
        and edx, 0x1E
        shl edx, 2
        xor eax, eax
        mov ecx, 8
        call chr_nk
        jmp .prg
.c4:
        mov edx, [m1_reg+4]
        shl edx, 2
        xor eax, eax
        mov ecx, 4
        call chr_nk
        mov edx, [m1_reg+8]
        shl edx, 2
        mov eax, 4
        call chr_nk
.prg:
        ; el programa: en los cartuchos de 512 KB, el bit 4 del banco de
        ; dibujos elige la mitad
        xor esi, esi
        cmp dword [rom_prg_n], 32
        jbe .p0
        mov esi, [m1_reg+4]
        and esi, 0x10
.p0:
        mov edx, [m1_reg+12]
        and edx, 0x0F
        mov ecx, ebx
        shr ecx, 2
        and ecx, 3
        cmp ecx, 2
        jae .p16
        and edx, 0x0E               ; 32 KB
        or edx, esi
        xor eax, eax
        call prg_16k
        inc eax
        inc edx
        call prg_16k
        jmp .x
.p16:
        je .p2
        or edx, esi                 ; 3: se cambia 8000h, el último en C000h
        xor eax, eax
        call prg_16k
        mov edx, 0x0F
        or edx, esi
        cmp dword [rom_prg_n], 32
        jae .p3a
        mov edx, -1
.p3a:
        mov eax, 1
        call prg_16k
        jmp .x
.p2:
        or edx, esi                 ; 2: el primero en 8000h, se cambia C000h
        mov eax, 1
        call prg_16k
        mov edx, esi
        xor eax, eax
        call prg_16k
.x:
        popad
        ret
.esp        db ESP_UNA0, ESP_UNA1, ESP_V, ESP_H

; ---- 4: bancos de 8 KB y contador de líneas ----
m4_ini:
        pushad
        mov edi, m4_reg
        mov ecx, m4_fin - m4_reg
        xor al, al
        rep stosb
        mov byte [m4_reg+6], 0
        mov byte [m4_reg+7], 1
        mov dword [map_escribir], m4_w
        mov dword [map_linea], m4_lin
        call m4_pone
        popad
        ret
m4_w:
        pushad
        movzx ebx, al
        mov eax, edx
        shr eax, 13
        and eax, 3                  ; 8000h, A000h, C000h, E000h
        shl eax, 1
        and edx, 1
        or eax, edx
        jmp [.t+eax*4]
        align 4
.t          dd .sel, .dato, .esp, .x, .cnt, .rec, .no, .si
.sel:
        mov [m4_sel], bl
        call m4_pone
        jmp .x
.dato:
        movzx eax, byte [m4_sel]
        and eax, 7
        mov [m4_reg+eax], bl
        call m4_pone
        jmp .x
.esp:
        cmp dword [rom_espejo], ESP_CUATRO
        je .x
        mov eax, ebx
        and eax, 1
        xor eax, 1
        call espejo
        jmp .x
.cnt:
        mov [m4_tope], bl
        jmp .x
.rec:
        mov byte [m4_rec], 1
        jmp .x
.no:
        mov byte [m4_irq], 0
        and byte [irq_linea], ~4
        jmp .x
.si:
        mov byte [m4_irq], 1
.x:
        popad
        ret
m4_pone:
        pushad
        ; el programa
        movzx ebx, byte [m4_sel]
        movzx edx, byte [m4_reg+6]
        xor eax, eax
        test bl, 0x40
        jz .p1
        mov eax, 2
.p1:
        call prg_8k
        xor eax, 2
        mov edx, -2
        call prg_8k
        mov eax, 1
        movzx edx, byte [m4_reg+7]
        call prg_8k
        mov eax, 3
        mov edx, -1
        call prg_8k
        ; los dibujos: dos de 2 KB y cuatro de 1 KB (o al revés)
        xor esi, esi
        test bl, 0x80
        jz .c1
        mov esi, 4
.c1:
        mov eax, esi
        movzx edx, byte [m4_reg]
        and edx, 0xFE
        mov ecx, 2
        call chr_nk
        lea eax, [esi+2]
        movzx edx, byte [m4_reg+1]
        and edx, 0xFE
        call chr_nk
        xor esi, 4
        xor ecx, ecx
.c2:
        lea eax, [esi+ecx]
        movzx edx, byte [m4_reg+2+ecx]
        call chr_1k
        inc ecx
        cmp ecx, 4
        jb .c2
        popad
        ret
; una línea de imagen más
m4_lin:
        cmp byte [m4_rec], 0
        jne .carga
        cmp byte [m4_cnt], 0
        je .carga
        dec byte [m4_cnt]
        jmp .mira
.carga:
        push eax
        mov al, [m4_tope]
        mov [m4_cnt], al
        mov byte [m4_rec], 0
        pop eax
.mira:
        cmp byte [m4_cnt], 0
        jne .x
        cmp byte [m4_irq], 0
        je .x
        or byte [irq_linea], 4
.x:
        ret
