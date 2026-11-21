; =============================================================================
;  Hispa-Drive - el cartucho, la memoria, los mandos y el Z80 de la consola
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Escrito desde la documentación pública de la consola de 16 bits de 1988
;  (el mapa de memoria, los puertos de los mandos, el bus del Z80):
;    000000-3FFFFF  el cartucho (y su RAM con pila, en 200000h)
;    A00000-A0FFFF  la memoria del Z80 (con el bus pedido): 8 KB de RAM, el
;                   YM2612 en A04000h
;    A10000-A1001F  la versión y los mandos; A11100h pedir el bus del Z80,
;                   A11200h su reinicio; A130F1h la RAM del cartucho
;    C00000-C0001F  el VDP (y el SN76489 en C00011h)
;    E00000-FFFFFF  64 KB de RAM (repetidos)
;  El Z80 (a 3,58 MHz) ve: 0000h-3FFFh su RAM (8 KB, dos veces), 4000h el
;  YM2612, 6000h el registro del banco (nueve escrituras de un bit), 7F11h
;  el SN76489 y 8000h-FFFFh una ventana de 32 KB a la memoria del 68000.
;
;    md_tam_buf      EAX=tamaño del archivo -> EAX=lo que hay que reservar
;    md_cargar       [rom]=lo leído, [rom_tam]=su tamaño: quita la cabecera
;                    de las copiadoras, deshace el formato .SMD (intercalado),
;                    hace la copia por palabras del 68000 y mira la cabecera
;                    (la RAM del cartucho, la región)
;    md_arrancar     enciende la consola
;    md_reiniciar    el botón de reinicio
;    md_lee8...      (las llama el 68000) lo que no es memoria directa
;    map_escribe     (la llama el Z80) EDX=dirección AL=octeto
; =============================================================================

ROM_MAXMD   equ 0x400000            ; 4 MB
RELOJ_68K   equ 7670453             ; (NTSC) 53,693175 MHz / 7
CICLOS_TH   equ 11000               ; 1,4 ms sin tocar TH: el mando de seis
                                    ; botones vuelve a empezar

; EAX=tamaño del archivo -> EAX=búfer: la ROM tal cual (por si es .SMD, al
; leerla), la copia por palabras (una potencia de 2) y 64 KB de margen
md_tam_buf:
        push ecx
        mov ecx, 0x20000
.b:
        cmp ecx, eax
        jae .c
        shl ecx, 1
        jmp .b
.c:
        lea eax, [eax+ecx+0x10000]
        pop ecx
        ret

md_cargar:
        pushad
        mov esi, [rom]
        mov eax, [rom_tam]
        mov ecx, eax                ; ¿una cabecera de 512 (copiadoras)?
        and ecx, 0x3FFF
        cmp ecx, 512
        jne .a
        sub eax, 512
        mov [rom_tam], eax
        add esi, 512
        mov [rom], esi
        ; ¿.SMD? (bloques de 16 KB: los impares primero y luego los pares)
        cmp byte [esi-512+8], 0xAA
        jne .a
        cmp byte [esi-512+9], 0xBB
        jne .a
        call .smd
.a:
        cmp dword [rom_tam], ROM_MAXMD
        jbe .a1
        mov dword [rom_tam], ROM_MAXMD
.a1:
        ; la potencia de 2 y la copia por palabras (al revés), detrás
        mov eax, [rom_tam]
        mov ecx, 0x20000
.b:
        cmp ecx, eax
        jae .c
        shl ecx, 1
        jmp .b
.c:
        mov [rom_pot], ecx
        mov edi, esi
        add edi, eax
        add edi, 3
        and edi, ~3
        mov [rom_sw], edi
        xor ebx, ebx                ; (repitiendo la ROM hasta la potencia)
.d:
        mov edx, ebx
.d1:
        cmp edx, eax
        jb .d2
        sub edx, eax
        jmp .d1
.d2:
        mov dx, [esi+edx]
        xchg dl, dh
        mov [edi+ebx], dx
        add ebx, 2
        cmp ebx, ecx
        jb .d
        ; la cabecera: la RAM del cartucho («RA» en 1B0h)
        mov dword [sram_ini], 0x200000
        mov dword [sram_fin], 0x20FFFF
        mov byte [sram_cab], 0
        cmp word [esi+0x1B0], 'RA'
        jne .r
        mov byte [sram_cab], 1
        mov eax, [esi+0x1B4]
        call .be32
        and eax, 0xFFFFFE
        cmp eax, 0x200000
        jb .r
        cmp eax, 0x3FFFFF
        ja .r
        mov ebx, eax
        mov eax, [esi+0x1B8]
        call .be32
        and eax, 0xFFFFFF
        cmp eax, ebx
        jb .r
        mov [sram_ini], ebx
        or eax, 1
        mov [sram_fin], eax
.r:
        ; la región (1F0h): J Japón, U América, E Europa (o una cifra)
        mov byte [region], 0x80     ; (lo normal: de fuera de Japón, NTSC)
        xor ecx, ecx
        xor edx, edx                ; DL: 1 J, 2 U, 4 E
.g:
        mov al, [esi+0x1F0+ecx]
        cmp al, 'J'
        jne .g1
        or dl, 1
.g1:
        cmp al, 'U'
        jne .g2
        or dl, 2
.g2:
        cmp al, 'E'
        jne .g3
        or dl, 4
.g3:
        cmp al, '0'
        jb .g4
        cmp al, '9'
        jbe .gc
        cmp al, 'A'
        jb .g4
        cmp al, 'F'
        ja .g4
        sub al, 7
.gc:
        sub al, '0'
        and al, 0x0D                ; (bit 0 Japón, 2 América, 3 Europa)
        test al, 1
        jz .gc1
        or dl, 1
.gc1:
        test al, 4
        jz .gc2
        or dl, 2
.gc2:
        test al, 8
        jz .g4
        or dl, 4
.g4:
        inc ecx
        cmp ecx, 3
        jb .g
        test dl, 2                  ; si vale para América, de América
        jnz .gx
        cmp dl, 1
        jne .ge
        mov byte [region], 0x00     ; solo Japón
        jmp .gx
.ge:
        test dl, 4
        jz .gx
        mov byte [region], 0xC0     ; solo Europa: PAL
.gx:
        cmp byte [region_forzar], 0
        je .gy
        mov al, [region_forzar]
        and al, 0xC0
        mov [region], al
.gy:
        popad
        ret
; EAX (cuatro octetos en orden del 68000) -> el número
.be32:
        xchg al, ah
        rol eax, 16
        xchg al, ah
        ret
; deshace el formato .SMD: ESI=la ROM, [rom_tam] (bloques de 16 KB; en cada
; uno, los primeros 8 KB son los octetos impares y los otros 8, los pares)
.smd:
        pushad
        mov ebx, esi
        mov ecx, [rom_tam]
        shr ecx, 14
        jz .s9
.s1:
        mov esi, ebx
        mov edi, smd_tmp
        push ecx
        mov ecx, 0x4000 / 4
        rep movsd
        pop ecx
        xor edx, edx
.s2:
        mov al, [smd_tmp+0x2000+edx]
        mov [ebx+edx*2], al
        mov al, [smd_tmp+edx]
        mov [ebx+edx*2+1], al
        inc edx
        cmp edx, 0x2000
        jb .s2
        add ebx, 0x4000
        dec ecx
        jnz .s1
.s9:
        popad
        ret

; =============================================================================
;  Encender y reiniciar
; =============================================================================
md_arrancar:
        pushad
        mov edi, md_ram
        mov ecx, 0x10000 / 4
        xor eax, eax
        rep stosd
        mov edi, z80_ram
        mov ecx, 0x2000 / 4
        rep stosd
        mov edi, md_nada
        mov ecx, 0x10000 / 4
        dec eax
        rep stosd
        mov byte [sram_usada], 0
        mov byte [sram_on], 0
        call m68k_tablas
        call z80_tablas
        call md_mapa
        mov dword [z_in], md_z80_in
        mov dword [z_out], md_z80_out
        call vdp_arrancar
        call psg_reiniciar
        call ym_reiniciar
        call md_reiniciar
        popad
        ret

md_reiniciar:
        pushad
        call md_mapa
        call m68k_reiniciar
        call z80_reiniciar
        call vdp_reiniciar
        mov byte [z80_bus], 0       ; el Z80, en reinicio y con el bus suyo
        mov byte [z80_reset], 1
        mov word [z80_banco], 0
        call z80_mapa
        mov byte [irq_linea], 0
        mov byte [nmi_pend], 0
        xor eax, eax
        mov [io_dat], eax
        mov [io_ctl], eax
        mov [th_fase], eax
        popad
        ret

; las tablas del 68000: la ROM (repetida hasta 4 MB), la RAM del cartucho,
; los puertos y la RAM
md_mapa:
        pushad
        xor ebx, ebx
.c:
        mov eax, ebx                ; el cartucho: 000000-3FFFFF
        shl eax, 16
        xor edx, edx
        div dword [rom_pot]
        mov eax, ebx
        shl eax, 16
        sub edx, eax
        add edx, [rom_sw]           ; (puntero tal que [p+dir] es el dato)
        mov [m_pl+ebx*4], edx
        mov [m_px+ebx*4], edx
        mov dword [m_pe+ebx*4], 0
        inc ebx
        cmp ebx, 0x40
        jb .c
.n:
        mov eax, ebx                ; lo que no es memoria: rutinas (y si se
        shl eax, 16                 ; ejecuta, FFFFh: la excepción F)
        mov edx, md_nada
        sub edx, eax
        mov dword [m_pl+ebx*4], 0
        mov dword [m_pe+ebx*4], 0
        mov [m_px+ebx*4], edx
        inc ebx
        cmp ebx, 0xE0
        jb .n
.r:
        mov eax, ebx                ; la RAM: E00000-FFFFFF
        shl eax, 16
        mov edx, md_ram
        sub edx, eax
        mov [m_pl+ebx*4], edx
        mov [m_pe+ebx*4], edx
        mov [m_px+ebx*4], edx
        inc ebx
        cmp ebx, 0x100
        jb .r
        call sram_mapa
        popad
        ret

; la RAM del cartucho, en el mapa si toca: con la cabecera, si la ROM no
; llega a ella (o el juego la enciende en A130F1h); sin cabecera, si la ROM
; es de 2 MB o menos (por si el juego la usa)
sram_mapa:
        pushad
        mov eax, [sram_ini]
        cmp dword [rom_tam], eax
        jbe .si
        cmp byte [sram_on], 0
        jne .si
        mov ebx, eax                ; tapada: otra vez la ROM
        shr ebx, 16
        mov ecx, [sram_fin]
        shr ecx, 16
.r:
        mov eax, ebx
        shl eax, 16
        xor edx, edx
        div dword [rom_pot]
        mov eax, ebx
        shl eax, 16
        sub edx, eax
        add edx, [rom_sw]
        mov [m_pl+ebx*4], edx
        mov [m_px+ebx*4], edx
        inc ebx
        cmp ebx, ecx
        jbe .r
        jmp .x
.si:
        mov ebx, eax
        shr ebx, 16
        mov ecx, [sram_fin]
        shr ecx, 16
.s:
        mov dword [m_pl+ebx*4], 0   ; (las rutinas lentas)
        mov dword [m_pe+ebx*4], 0
        inc ebx
        cmp ebx, ecx
        jbe .s
.x:
        popad
        ret

; =============================================================================
;  Las rutinas lentas del 68000: EDX=dirección (24 bits). Las de leer dan
;  EAX; las de escribir, AL/AX/EAX. Conservan todo menos EAX (al leer) y EBP
; =============================================================================
md_lee8:
        mov ebp, edx
        shr ebp, 16
        jmp [md_tl8+ebp*4]
md_lee16:
        mov ebp, edx
        shr ebp, 16
        jmp [md_tl16+ebp*4]
md_esc8:
        mov ebp, edx
        shr ebp, 16
        jmp [md_te8+ebp*4]
md_esc16:
        mov ebp, edx
        shr ebp, 16
        jmp [md_te16+ebp*4]
md_lee32:
        push edx
        call md_lee16
        shl eax, 16
        add edx, 2
        and edx, 0xFFFFFF
        push eax
        call md_lee16
        pop ebp
        and eax, 0xFFFF
        or eax, ebp
        pop edx
        ret
md_esc32:
        push edx
        push eax
        shr eax, 16
        call md_esc16
        mov eax, [esp]
        add edx, 2
        and edx, 0xFFFFFF
        call md_esc16
        pop eax
        pop edx
        ret

; las tablas de esas rutinas, por trozos de 64 KB
md_rutinas:
        pushad
        xor ebx, ebx
.c:
        mov dword [md_tl8+ebx*4], md_nada_l
        mov dword [md_tl16+ebx*4], md_nada_l
        mov dword [md_te8+ebx*4], md_nada_e
        mov dword [md_te16+ebx*4], md_nada_e
        cmp ebx, 0x40
        jae .c1
        mov dword [md_tl8+ebx*4], sram_l8
        mov dword [md_tl16+ebx*4], sram_l16
        mov dword [md_te8+ebx*4], sram_e8
        mov dword [md_te16+ebx*4], sram_e16
.c1:
        cmp ebx, 0xC0
        jb .c2
        cmp ebx, 0xDF
        ja .c2
        mov dword [md_tl8+ebx*4], vdp_l8
        mov dword [md_tl16+ebx*4], vdp_l16
        mov dword [md_te8+ebx*4], vdp_e8
        mov dword [md_te16+ebx*4], vdp_e16
.c2:
        inc ebx
        cmp ebx, 256
        jb .c
        mov dword [md_tl8+0xA0*4], z80a_l8
        mov dword [md_tl16+0xA0*4], z80a_l16
        mov dword [md_te8+0xA0*4], z80a_e8
        mov dword [md_te16+0xA0*4], z80a_e16
        mov dword [md_tl8+0xA1*4], io_l8
        mov dword [md_tl16+0xA1*4], io_l16
        mov dword [md_te8+0xA1*4], io_e8
        mov dword [md_te16+0xA1*4], io_e16
        popad
        ret

md_nada_l:
        mov eax, 0xFFFF
        ret
md_nada_e:
        ret

; ---- la RAM del cartucho (64 KB, octeto a octeto, en el orden del 68000) ----
sram_l8:
        mov ebp, edx
        sub ebp, [sram_ini]
        cmp ebp, 0xFFFF
        ja md_nada_l
        movzx eax, byte [sram+ebp]
        ret
sram_l16:
        mov ebp, edx
        sub ebp, [sram_ini]
        and ebp, ~1
        cmp ebp, 0xFFFE
        ja md_nada_l
        movzx eax, word [sram+ebp]
        xchg al, ah
        ret
sram_e8:
        mov ebp, edx
        sub ebp, [sram_ini]
        cmp ebp, 0xFFFF
        ja .x
        mov [sram+ebp], al
        mov byte [sram_usada], 1
.x:
        ret
sram_e16:
        mov ebp, edx
        sub ebp, [sram_ini]
        and ebp, ~1
        cmp ebp, 0xFFFE
        ja .x
        xchg al, ah
        mov [sram+ebp], ax
        xchg al, ah
        mov byte [sram_usada], 1
.x:
        ret

; ---- la memoria del Z80, vista desde el 68000 (con el bus pedido) ----
z80a_l8:
        mov ebp, edx
        and ebp, 0x7FFF
        cmp ebp, 0x4000
        jb .ram
        cmp ebp, 0x6000
        jb .ym
        mov eax, 0xFF
        ret
.ram:
        and ebp, 0x1FFF
        movzx eax, byte [z80_ram+ebp]
        ret
.ym:
        movzx eax, byte [ym_estado]
        ret
z80a_l16:                           ; (una palabra: el octeto dos veces)
        call z80a_l8
        mov ah, al
        ret
z80a_e8:
        push edx
        and edx, 0x7FFF
        call map_escribe
        pop edx
        ret
z80a_e16:                           ; (una palabra: el octeto alto)
        push eax
        mov al, ah
        call z80a_e8
        pop eax
        ret

; ---- la versión, los mandos y el Z80 (A10000-A1FFFF) ----
io_l16:
        call io_l8
        mov ah, al
        ret
io_l8:
        mov ebp, edx
        and ebp, 0xFFFF
        cmp ebp, 0x20
        jb .io
        and ebp, 0xFF00
        cmp ebp, 0x1100             ; A11100: el bus del Z80 (0: es nuestro)
        jne .n
        mov eax, 0xFF               ; (aunque esté en reinicio: así ningún
        cmp byte [z80_bus], 0       ; juego se queda esperando)
        je .n1
        and al, 0xFE
.n1:
        ret
.n:
        xor eax, eax
        ret
.io:
        shr ebp, 1
        cmp ebp, 0
        je .version
        cmp ebp, 1
        je .p1
        cmp ebp, 2
        je .p2
        cmp ebp, 4
        je .c1
        cmp ebp, 5
        je .c2
        mov eax, 0xFF
        cmp ebp, 3
        jne .x
        mov al, 0x7F
.x:
        ret
.version:
        movzx eax, byte [region]
        or al, 0x20                 ; (sin la unidad de CD)
        ret
.c1:
        movzx eax, byte [io_ctl]
        ret
.c2:
        movzx eax, byte [io_ctl+1]
        ret
.p1:
        push ebx
        xor ebx, ebx
        jmp .p
.p2:
        push ebx
        mov ebx, 1
.p:
        call mando_lee
        pop ebx
        ret

io_e16:
        push eax
        mov ebp, edx
        and ebp, 0xFF00
        cmp ebp, 0x1100             ; (A11100 y A11200: el octeto alto)
        je .alto
        cmp ebp, 0x1200
        je .alto
        call io_e8
        pop eax
        ret
.alto:
        mov al, ah
        call io_e8
        pop eax
        ret
io_e8:
        mov ebp, edx
        and ebp, 0xFFFF
        cmp ebp, 0x20
        jb .io
        push eax
        and ebp, 0xFF00
        cmp ebp, 0x1100
        je .bus
        cmp ebp, 0x1200
        je .reset
        cmp ebp, 0x3000             ; A130F1: la RAM del cartucho
        jne .x
        mov ebp, edx
        and ebp, 0xFF
        cmp ebp, 0xF1
        jne .x
        and al, 1
        cmp al, [sram_on]
        je .x
        mov [sram_on], al
        call sram_mapa
        jmp .x
.bus:
        and al, 1
        mov [z80_bus], al
        jmp .x
.reset:
        and al, 1
        xor al, 1                   ; 0 = en reinicio
        cmp al, [z80_reset]
        je .x
        mov [z80_reset], al
        test al, al
        jz .x
        call z80_reiniciar          ; (al soltarlo empieza de nuevo)
        call ym_reiniciar
.x:
        pop eax
        ret
.io:
        shr ebp, 1
        cmp ebp, 1
        je .d1
        cmp ebp, 2
        je .d2
        cmp ebp, 4
        je .k1
        cmp ebp, 5
        je .k2
        ret
.k1:
        mov [io_ctl], al
        ret
.k2:
        mov [io_ctl+1], al
        ret
.d1:
        push ebx
        xor ebx, ebx
        jmp .d
.d2:
        push ebx
        mov ebx, 1
.d:
        ; ¿baja TH (bit 6, de salida)? Los de seis botones cuentan las bajadas
        push eax
        push ecx
        mov ah, [io_dat+ebx]
        mov [io_dat+ebx], al
        test byte [io_ctl+ebx], 0x40
        jz .d9
        xor ah, al
        test ah, 0x40
        jz .d9
        call ciclos_ahora           ; (un cambio: mucho tiempo quieto vuelve
        mov ecx, eax                ; a empezar la cuenta)
        sub eax, [th_cuando+ebx*4]
        mov [th_cuando+ebx*4], ecx
        cmp eax, CICLOS_TH
        jb .d3
        mov byte [th_fase+ebx], 0
.d3:
        test byte [io_dat+ebx], 0x40
        jnz .d9
        inc byte [th_fase+ebx]      ; una bajada más (1-4, y vuelta)
        cmp byte [th_fase+ebx], 4
        jbe .d9
        mov byte [th_fase+ebx], 1
.d9:
        pop ecx
        pop eax
        pop ebx
        ret

; -> EAX=los ciclos del 68000 desde que se encendió (más o menos: vale para
; medir tiempos cortos)
ciclos_ahora:
        mov eax, [md_ciclos]
        add eax, [m_presup]
        sub eax, edi
        ret

; EBX=mando (0 o 1) -> AL=lo que se lee en su puerto. Los botones (pad):
; bit 0 arriba, 1 abajo, 2 izquierda, 3 derecha, 4 B, 5 C, 6 A, 7 Start,
; 8 Z, 9 Y, 10 X, 11 Mode (1 = pulsado). Con TH arriba: ?1CBRLDU; abajo:
; ?0SA00DU. El de seis botones, tras la tercera bajada: ?0SA0000 abajo y
; ?1CBMXYZ arriba; tras la cuarta, ?0SA1111 abajo
mando_lee:
        push ecx
        push edx
        call ciclos_ahora           ; (mucho sin tocar TH: la cuenta, a cero)
        sub eax, [th_cuando+ebx*4]
        cmp eax, CICLOS_TH
        jb .t
        mov byte [th_fase+ebx], 0
.t:
        movzx edx, word [pad_md+ebx*2]
        not edx                     ; (0 = pulsado)
        movzx ecx, byte [th_fase+ebx]
        cmp byte [seis_md+ebx], 0
        jne .s
        xor ecx, ecx                ; (de tres: nada de eso)
.s:
        mov al, 0x40                ; TH: si es de salida, el escrito; si
        test byte [io_ctl+ebx], 0x40 ; no, arriba
        jz .th
        mov al, [io_dat+ebx]
.th:
        test al, 0x40
        jz .bajo
        mov al, dl                  ; arriba: ?1CBRLDU...
        and al, 0x3F
        cmp ecx, 3
        jne .a1
        and al, 0x30                ; ...o ?1CBMXYZ (Z Y X M en los bits 8-11)
        mov ah, dh
        and ah, 0x0F
        or al, ah
.a1:
        or al, 0x40
        jmp .fin
.bajo:
        mov al, dl                  ; abajo: A (6) y Start (7) a los bits 4 y 5
        and al, 0xC0
        shr al, 2
        cmp ecx, 3
        je .fin                     ; ?0SA0000
        cmp ecx, 4
        jne .b3
        or al, 0x0F                 ; ?0SA1111
        jmp .fin
.b3:
        mov ah, dl                  ; ?0SA00DU
        and ah, 0x03
        or al, ah
.fin:
        ; los bits de salida, los escritos
        mov ah, [io_ctl+ebx]
        and ah, 0x7F
        mov dl, [io_dat+ebx]
        and dl, ah
        not ah
        and al, ah
        or al, dl
        and eax, 0xFF
        pop edx
        pop ecx
        ret

; =============================================================================
;  El Z80
; =============================================================================
; las tablas de la memoria del Z80 (trozos de 1 KB; la 64 es la 0 otra vez)
z80_mapa:
        pushad
        xor ebx, ebx
.r:
        mov eax, ebx                ; 0000-3FFF: la RAM, dos veces
        and eax, 7
        shl eax, 10
        add eax, z80_ram
        mov edx, ebx
        shl edx, 10
        sub eax, edx
        mov [pg_lee+ebx*4], eax
        mov [pg_esc+ebx*4], eax
        inc ebx
        cmp ebx, 16
        jb .r
.y:
        mov eax, ym_pag             ; 4000-7FFF: el estado del YM2612 al leer
        mov edx, ebx
        shl edx, 10
        sub eax, edx
        mov [pg_lee+ebx*4], eax
        mov dword [pg_esc+ebx*4], 0
        inc ebx
        cmp ebx, 32
        jb .y
        mov eax, [pg_lee]
        mov [pg_lee+64*4], eax
        sub dword [pg_lee+64*4], 0x10000
        mov eax, [pg_esc]
        mov [pg_esc+64*4], eax
        sub dword [pg_esc+64*4], 0x10000
        popad
        ; (sigue) la ventana de 8000h: el banco, en la ROM (en el orden
        ; normal) si cae en ella; si no, lo hace map_escribe y se lee FFh
z80_banco_pon:
        pushad
        movzx eax, word [z80_banco]
        shl eax, 15                 ; la dirección del 68000
        mov ebx, 32
.b:
        mov edx, ebx
        shl edx, 10
        mov ecx, eax
        add ecx, edx
        sub ecx, 0x8000             ; ECX = dirección del 68000 de este KB
        cmp ecx, 0x400000
        jae .nada
        cmp ecx, [rom_tam]
        jb .rom
        push eax
        push edx
        mov eax, ecx                ; (más allá: la ROM repetida)
        xor edx, edx
        div dword [rom_pot]
        mov ecx, edx
        pop edx
        pop eax
        cmp ecx, [rom_tam]
        jae .nada
.rom:
        add ecx, [rom]
        sub ecx, edx
        mov [pg_lee+ebx*4], ecx
        jmp .s
.nada:
        mov ecx, md_nada
        sub ecx, edx
        mov [pg_lee+ebx*4], ecx
.s:
        mov dword [pg_esc+ebx*4], 0
        inc ebx
        cmp ebx, 64
        jb .b
        popad
        ret

; EDX=dirección del Z80 AL=octeto: lo que no va derecho a su RAM (conserva
; todo menos EBP)
map_escribe:
        cmp edx, 0x4000
        jb .ram
        cmp edx, 0x6000
        jb .ym
        cmp edx, 0x6100
        jb .banco
        cmp edx, 0x7F00
        jb .x
        cmp edx, 0x8000
        jb .vdp
        ; la ventana: a la memoria del 68000
        push edx
        push eax
        movzx ebp, word [z80_banco]
        shl ebp, 15
        and edx, 0x7FFF
        add edx, ebp
        and edx, 0xFFFFFF
        mov ebp, edx
        shr ebp, 16
        mov ebp, [m_pe+ebp*4]
        test ebp, ebp
        jz .lento
        xor edx, 1
        mov [ebp+edx], al
        jmp .w9
.lento:
        call md_esc8
.w9:
        pop eax
        pop edx
.x:
        ret
.ram:
        mov ebp, edx
        and ebp, 0x1FFF
        mov [z80_ram+ebp], al
        ret
.ym:
        push edx
        and edx, 3
        call ym_escribe
        pop edx
        ret
.banco:
        push eax                    ; un bit más, por arriba (9 bits)
        and eax, 1
        shl eax, 8
        shr word [z80_banco], 1
        or [z80_banco], ax
        pop eax
        jmp z80_banco_pon
.vdp:
        cmp edx, 0x7F11
        jb .x
        cmp edx, 0x7F18
        jae .x
        test edx, 1
        jz .x
        jmp psg_escribe

md_z80_in:
        mov al, 0xFF
        ret
md_z80_out:
        ret

; el estado del YM2612 ha cambiado: lo que lee el Z80 en 4000h-7FFFh
ym_pag_pon:
        push eax
        push ecx
        push edi
        mov al, [ym_estado]
        mov ah, al
        mov ecx, eax
        shl eax, 16
        mov ax, cx
        mov edi, ym_pag
        mov ecx, 1024 / 4
        rep stosd
        pop edi
        pop ecx
        pop eax
        ret
