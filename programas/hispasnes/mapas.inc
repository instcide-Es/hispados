; =============================================================================
;  Hispa-Snes - los cartuchos: la cabecera, los mapas de memoria y su RAM
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  La cabecera interna del cartucho (64 octetos) está en FFC0h de su mapa:
;  en 7FC0h de la ROM si es LoROM, en FFC0h si es HiROM y en 40FFC0h si es
;  ExHiROM. Se miran las tres y se queda la que más pinta tiene (la suma y
;  su complemento, el modo, el vector de arranque, el título). Las copiadoras
;  dejaban delante 512 octetos suyos: se saltan.
;
;    snes_tam_buf   EAX=octetos del archivo -> EAX=memoria para la ROM
;    cart_cargar    con [rom] y [rom_tam]: la cabecera, el mapa y la RAM.
;                   Deja [cart_mapa] (1 LoROM, 2 HiROM, 3 ExHiROM),
;                   [cart_chip] (0 ninguno; si no, cuál: CHIP_*),
;                   [sram_tam], [cart_pal] y el título en cart_titulo
; =============================================================================
MAPA_LO     equ 1
MAPA_HI     equ 2
MAPA_EXHI   equ 3
CHIP_DSP    equ 1                   ; los coprocesadores que no se emulan
CHIP_GSU    equ 2
CHIP_OBC    equ 3
CHIP_SA1    equ 4
CHIP_SDD1   equ 5
CHIP_RTC    equ 6
CHIP_OTRO   equ 7
SRAM_MAX    equ 0x20000             ; 128 KB como mucho
PT_NADA     equ 0                   ; pag_tipo: lo que no es memoria directa
PT_REG      equ 1                   ;   los registros (2000h-5FFFh)
PT_SRAM     equ 2                   ;   la RAM del cartucho, si es de menos de 8 KB

; EAX = lo que mide el archivo -> EAX = la memoria que hace falta (la ROM sin
; la cabecera de la copiadora, hasta la potencia de 2 siguiente)
snes_tam_buf:
        push ecx
        mov ecx, eax
        and ecx, 0x3FF
        cmp ecx, 0x200
        jne .a
        sub eax, 0x200
.a:
        mov ecx, 0x8000
.b:
        cmp ecx, eax
        jae .c
        shl ecx, 1
        jmp .b
.c:
        mov eax, ecx
        add eax, 16
        pop ecx
        ret

cart_cargar:
        pushad
        ; la cabecera de la copiadora: fuera (se corre la ROM hacia delante)
        mov eax, [rom_tam]
        and eax, 0x3FF
        cmp eax, 0x200
        jne .sincab
        mov esi, [rom]
        lea edi, [esi]
        add esi, 0x200
        mov ecx, [rom_tam]
        sub ecx, 0x200
        mov [rom_tam], ecx
        rep movsb
.sincab:
        ; la potencia de 2 (P) y el espejo de lo que pasa de ella
        mov eax, [rom_tam]
        mov ecx, 0x8000
.p:
        cmp ecx, eax
        jae .p1
        shl ecx, 1
        jmp .p
.p1:
        mov [rom_mascara], ecx
        dec dword [rom_mascara]
        cmp ecx, eax
        je .espejo_ya
        ; (ECX = la potencia de 2 de arriba; lo que falta desde rom_tam se
        ;  rellena repitiendo lo que pasa de la de abajo)
        mov edx, ecx
        shr edx, 1                  ; EDX = la de abajo
        mov ebx, eax
        sub ebx, edx                ; EBX = lo que pasa de ella
        mov edi, eax                ; desde el final...
.e:
        cmp edi, ecx
        jae .espejo_ya
        mov eax, edi
        sub eax, edx
        push edx
        xor edx, edx
        div ebx
        pop eax                     ; EAX = la de abajo, EDX = resto
        add edx, eax
        mov esi, [rom]
        mov al, [esi+edx]
        mov [esi+edi], al
        mov edx, ecx
        shr edx, 1
        inc edi
        jmp .e
.espejo_ya:
        ; las tres cabeceras posibles
        mov dword [mejor_p], -1
        mov dword [cart_mapa], MAPA_LO
        mov eax, 0x7FC0
        mov edx, MAPA_LO
        call cab_puntos
        mov eax, 0xFFC0
        mov edx, MAPA_HI
        call cab_puntos
        mov eax, 0x40FFC0
        mov edx, MAPA_EXHI
        call cab_puntos
        ; lo que dice la elegida
        mov esi, [rom]
        add esi, [cab_dir]
        mov edi, cart_titulo        ; el título (21 letras), sin los espacios
        mov ecx, 21                 ; de detrás
        push esi
.t:
        lodsb
        cmp al, ' '
        jb .t0
        cmp al, 0x7E
        jbe .t1
.t0:
        mov al, ' '
.t1:
        stosb
        dec ecx
        jnz .t
.t2:
        cmp edi, cart_titulo
        je .t3
        cmp byte [edi-1], ' '
        jne .t3
        dec edi
        jmp .t2
.t3:
        mov byte [edi], 0
        pop esi
        ; la RAM del cartucho: 1 << n KB
        movzx ecx, byte [esi+0x18]
        xor eax, eax
        test ecx, ecx
        jz .s1
        cmp ecx, 7
        jbe .s0
        mov ecx, 7
.s0:
        mov eax, 1024
        shl eax, cl
.s1:
        mov [sram_tam], eax
        ; el coprocesador (el tipo, octeto 16h: la cifra baja 3 o más)
        mov dword [cart_chip], 0
        movzx eax, byte [esi+0x16]
        mov edx, eax
        and edx, 0x0F
        cmp edx, 3
        jb .c9
        cmp edx, 6
        ja .c9
        shr eax, 4
        movzx eax, byte [chips+eax]
        mov [cart_chip], eax
.c9:
        ; PAL o NTSC (el país, octeto 19h)
        movzx eax, byte [esi+0x19]
        xor edx, edx
        cmp eax, 2
        jb .r
        cmp eax, 0x0C
        jbe .rp
        cmp eax, 0x11
        jne .r
.rp:
        inc edx
.r:
        mov [cart_pal], edx
        call cart_mapear
        popad
        ret

; (los coprocesadores, por la cifra alta del tipo)
chips       db CHIP_DSP, CHIP_GSU, CHIP_OBC, CHIP_SA1, CHIP_SDD1, CHIP_RTC, CHIP_OTRO, CHIP_OTRO
            db CHIP_OTRO, CHIP_OTRO, CHIP_OTRO, CHIP_OTRO, CHIP_OTRO, CHIP_OTRO, CHIP_OTRO, CHIP_OTRO

; EAX = dónde estaría la cabecera en la ROM, EDX = su mapa: sus puntos; si
; son más que los de la mejor, pasa a serlo
cab_puntos:
        pushad
        mov ebx, eax
        add ebx, 0x40
        cmp ebx, [rom_tam]
        ja .x                       ; (la ROM no llega)
        mov esi, [rom]
        add esi, eax
        xor ebp, ebp                ; los puntos
        movzx ecx, word [esi+0x1C]  ; complemento + suma = FFFFh
        movzx ebx, word [esi+0x1E]
        add ecx, ebx
        cmp ecx, 0xFFFF
        jne .a
        add ebp, 4
.a:
        movzx ecx, byte [esi+0x15]  ; el modo
        and ecx, 0xEF               ; (sin el bit de la ROM rápida)
        cmp edx, MAPA_LO
        jne .h
        cmp ecx, 0x20
        je .m
        cmp ecx, 0x22
        je .m
        cmp ecx, 0x23
        je .m
        jmp .v
.h:
        cmp edx, MAPA_HI
        jne .eh
        cmp ecx, 0x21
        je .m
        cmp ecx, 0x2A
        je .m
        jmp .v
.eh:
        cmp ecx, 0x25
        jne .v
.m:
        add ebp, 3
.v:
        movzx ecx, word [esi+0x3C]  ; el vector de arranque
        cmp ecx, 0x8000
        jb .w
        inc ebp
        ; y lo primero que hace (las instrucciones de empezar de siempre)
        mov ebx, eax
        and ebx, ~0xFFFF
        cmp edx, MAPA_LO
        jne .v1
        mov ebx, eax
        and ebx, ~0x7FFF
        sub ecx, 0x8000
.v1:
        add ebx, ecx
        cmp ebx, [rom_tam]
        jae .w
        mov edi, [rom]
        movzx ecx, byte [edi+ebx]
        cmp cl, 0x78                ; SEI
        je .w1
        cmp cl, 0x18                ; CLC
        je .w1
        cmp cl, 0x38                ; SEC
        je .w1
        cmp cl, 0x9C                ; STZ
        je .w1
        cmp cl, 0x4C                ; JMP
        je .w1
        cmp cl, 0x5C                ; JML
        je .w1
        cmp cl, 0xC2                ; REP
        je .w1
        cmp cl, 0xE2                ; SEP
        jne .w
.w1:
        add ebp, 2
.w:
        mov ecx, 21                 ; el título: letras que se pueden leer
        xor ebx, ebx
.ti:
        mov bl, [esi]
        inc esi
        cmp bl, 0x20
        jb .tx
        cmp bl, 0x7E
        ja .tx
        dec ecx
        jnz .ti
        inc ebp
.tx:
        cmp ebp, [mejor_p]
        jle .x
        mov [mejor_p], ebp
        mov [cab_dir], eax
        mov [cart_mapa], edx
.x:
        popad
        ret

; ---- los mapas de memoria: pag_lee, pag_esc y pag_tipo ----
cart_mapear:
        pushad
        xor ebx, ebx                ; EBX = el trozo de 8 KB (0..2047)
.trozo:
        mov eax, ebx
        shr eax, 3                  ; EAX = el banco
        mov ecx, ebx
        and ecx, 7                  ; ECX = el trozo dentro del banco
        mov edx, eax
        and edx, 0x7F               ; EDX = el banco sin el bit alto
        xor esi, esi                ; ESI = lo que se lee (0: aparte)
        xor edi, edi                ; EDI = lo que se escribe
        mov byte [pag_tipo+ebx], PT_NADA
        cmp eax, 0x7E               ; la RAM de 128 KB
        je .wram
        cmp eax, 0x7F
        je .wram
        cmp edx, 0x40
        jae .alto
        ; bancos 00-3F y 80-BF
        test ecx, ecx
        jz .wbajo                   ; 0000-1FFF: la RAM (sus 8 KB primeros)
        cmp ecx, 3
        jb .reg                     ; 2000-5FFF: los registros
        je .seis                    ; 6000-7FFF
        ; 8000-FFFF: la ROM
        call .rom_alta
        jmp .pon
.seis:
        cmp dword [cart_mapa], MAPA_LO
        je .pon                     ; (en LoROM, nada)
        cmp dword [sram_tam], 0     ; HiROM: la RAM del cartucho en 20-3F
        je .pon
        cmp edx, 0x20
        jb .pon
        mov eax, edx
        and eax, 0x1F
        shl eax, 13
        call .sram
        jmp .pon
.alto:
        ; bancos 40-7D y C0-FF
        cmp dword [cart_mapa], MAPA_LO
        jne .hialto
        cmp ecx, 4
        jae .lo_rom
        cmp dword [sram_tam], 0     ; LoROM: la RAM del cartucho en 70-7D y
        je .lo_rom                  ; F0-FF, de 0000 a 7FFF
        cmp edx, 0x70
        jb .lo_rom
        mov eax, edx
        sub eax, 0x70
        shl eax, 15
        mov esi, ecx
        shl esi, 13
        add eax, esi
        xor esi, esi
        call .sram
        jmp .pon
.lo_rom:
        call .rom_alta              ; (0000-7FFF: lo mismo que 8000-FFFF)
        jmp .pon
.hialto:
        ; HiROM y ExHiROM: los 64 KB del banco, de la ROM
        mov eax, ebx
        shr eax, 3
        and eax, 0x3F
        shl eax, 16
        mov esi, ecx
        shl esi, 13
        add eax, esi
        cmp dword [cart_mapa], MAPA_EXHI
        jne .h1
        mov esi, ebx                ; ExHiROM: 40-7D son los 4 MB de arriba
        shr esi, 3
        test esi, 0x80
        jnz .h1
        add eax, 0x400000
.h1:
        call .rom_pon
        jmp .pon
.wram:
        sub eax, 0x7E
        shl eax, 16
        mov esi, ecx
        shl esi, 13
        add eax, esi
        add eax, wram
        jmp .directo
.wbajo:
        mov eax, wram
        jmp .directo
.reg:
        mov byte [pag_tipo+ebx], PT_REG
        jmp .pon
.directo:                           ; EAX = dónde está el trozo
        mov esi, ebx
        shl esi, 13
        sub eax, esi
        mov esi, eax
        mov edi, eax
.pon:
        mov [pag_lee+ebx*4], esi
        mov [pag_esc+ebx*4], edi
        inc ebx
        cmp ebx, 2048
        jb .trozo
        popad
        ret
; (internas) la ROM de 8000-FFFF de un banco 00-3F/80-BF o de LoROM: deja
; ESI (EDI sigue a 0)
.rom_alta:
        cmp dword [cart_mapa], MAPA_LO
        jne .ra_hi
        mov eax, edx                ; LoROM: 32 KB por banco
        shl eax, 15
        mov esi, ecx
        and esi, 3
        shl esi, 13
        add eax, esi
        jmp .rom_pon
.ra_hi:
        mov eax, edx                ; HiROM: la mitad de arriba del banco
        and eax, 0x3F
        shl eax, 16
        mov esi, ecx
        shl esi, 13
        add eax, esi
        cmp dword [cart_mapa], MAPA_EXHI
        jne .rom_pon
        mov esi, ebx                ; ExHiROM: 00-3F son los 4 MB de arriba
        shr esi, 3
        test esi, 0x80
        jnz .rom_pon
        add eax, 0x400000
; EAX = desplazamiento en la ROM -> ESI (con el espejo)
.rom_pon:
        and eax, [rom_mascara]
        add eax, [rom]
        mov esi, ebx
        shl esi, 13
        sub eax, esi
        mov esi, eax
        ret
; EAX = desplazamiento en la RAM del cartucho -> ESI y EDI (o aparte, si es
; de menos de 8 KB)
.sram:
        cmp dword [sram_tam], 0x2000
        jb .sram_poca
        mov esi, [sram_tam]
        dec esi
        and eax, esi
        add eax, sram
        mov esi, ebx
        shl esi, 13
        sub eax, esi
        mov esi, eax
        mov edi, eax
        ret
.sram_poca:
        mov byte [pag_tipo+ebx], PT_SRAM
        xor esi, esi
        xor edi, edi
        ret
