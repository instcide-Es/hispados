; =============================================================================
;  mdnativo - el núcleo de Hispa-Drive como programa de Linux, para las pruebas
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Los mismos archivos del emulador (programas/hispadrv/*.inc), sin ventana.
;
;  mdnativo -t PRUEBAS.BIN
;    las pruebas de una instrucción del 68000 (tests/hispadrv/sst68k.py las
;    pasa de JSON a este formato): para cada una, los registros y la memoria
;    de antes, una instrucción y lo de después. Memoria plana de 16 MB.
;    Escribe las que fallan y, al final, cuántas van bien y cuántas tardan
;    los ciclos que dice la prueba
;  mdnativo ROM CUADROS IMAGEN.ppm [GUION|-] [MEMORIA|-] [SONIDO|-] [SRAM]
;    corre la ROM CUADROS cuadros y deja la última imagen (320x224, PPM;
;    con «-», sin pintar ninguna: para medir)
;    GUION    dos octetos por cuadro: los botones del mando 1 (bit 0 arriba,
;             1 abajo, 2 izquierda, 3 derecha, 4 B, 5 C, 6 A, 7 Start, 8 Z,
;             9 Y, 10 X, 11 Mode); con el bit 15, mando de seis botones
;    MEMORIA  la RAM del 68000 (64 KB, en su orden), la del Z80 (8 KB), la
;             del cartucho (64 KB), la CRAM (128), la VSRAM (80) y la VRAM
;    SONIDO   las muestras (22.050 por segundo, 16 bits con signo, un canal);
;             «0»: sin hacer sonido (como en Hispa-Pentium, para medir)
;    SRAM     la RAM del cartucho al encender (como el .SRM del emulador)
;  Se hace solo con NASM (la cabecera ELF va escrita aquí; sin enlazador ni
;  biblioteca de C).
; =============================================================================
        bits 32
BASE        equ 0x08048000
        org BASE
        section .text
        db 0x7F, "ELF", 1, 1, 1, 0  ; ELF de 32 bits para x86
        times 8 db 0
        dw 2, 3                     ; ejecutable, 80386
        dd 1, _start, 52, 0, 0
        dw 52, 32, 1, 0, 0, 0
        dd 1, 0, BASE, BASE         ; un solo trozo: todo el archivo...
        dd fin_archivo - BASE
        dd fin_memoria - BASE       ; ...y detrás las variables
        dd 7, 0x1000
_start:
        mov ebp, esp
        cmp dword [ebp], 3
        jb near uso
        mov ebx, [ebp+8]
        cmp word [ebx], '-t'
        je near pruebas_cpu
        jmp mdmain
uso:
        mov ebx, 2
        mov eax, 1
        int 0x80
mal:
        mov ebx, 1
        mov eax, 1
        int 0x80

; abre el archivo EBX y lo lee entero en ECX (EDX como mucho) -> EAX=octetos
lee_archivo:
        push ecx
        push edx
        xor ecx, ecx
        mov eax, 5
        int 0x80
        pop edx
        pop ecx
        test eax, eax
        js mal
        mov ebx, eax
        push ecx
.l:
        test edx, edx
        jz .x
        push ecx
        push edx
        mov eax, 3
        int 0x80
        pop edx
        pop ecx
        test eax, eax
        jle .x
        add ecx, eax
        sub edx, eax
        jmp .l
.x:
        mov eax, 6
        int 0x80
        mov eax, ecx
        pop ecx
        sub eax, ecx
        ret

; ---- la salida ----
; ESI=cadena acabada en 0
pon_cad:
        push eax
.c:
        lodsb
        test al, al
        jz .x
        call pon_letra
        jmp .c
.x:
        pop eax
        ret
pon_letra:
        push edi
        mov edi, [sal_n]
        mov [sal_buf+edi], al
        inc dword [sal_n]
        pop edi
        cmp al, 10
        je vacia
        cmp dword [sal_n], 3000
        jae vacia
        ret
vacia:
        pushad
        mov ebx, 1
        mov ecx, sal_buf
        mov edx, [sal_n]
        mov eax, 4
        int 0x80
        mov dword [sal_n], 0
        popad
        ret
; EAX en hexadecimal (8 cifras)
pon_hex:
        pushad
        mov ecx, 8
.c:
        rol eax, 4
        push eax
        and al, 15
        add al, '0'
        cmp al, '9'
        jbe .d
        add al, 7
.d:
        call pon_letra
        pop eax
        dec ecx
        jnz .c
        popad
        ret
; EAX en decimal
pon_dec:
        pushad
        mov ebx, 10
        xor ecx, ecx
.c:
        xor edx, edx
        div ebx
        push edx
        inc ecx
        test eax, eax
        jnz .c
.p:
        pop eax
        add al, '0'
        call pon_letra
        dec ecx
        jnz .p
        popad
        ret

; =============================================================================
;  Las pruebas del procesador
;  El formato (todo en dd): por prueba, el nombre (dd largo y las letras,
;  rellenas a 4), lo de antes (d0-d7, a0-a6, usp, ssp, sr, pc, las dos
;  palabras ya leídas, cuántos octetos de memoria y sus parejas dirección,
;  valor), lo de después (lo mismo sin las dos palabras) y los ciclos.
; =============================================================================
pruebas_cpu:
        mov ebx, [ebp+12]
        mov ecx, pr_buf
        mov edx, PR_MAX
        call lee_archivo
        add eax, pr_buf
        mov [pr_fin], eax
        call m68k_tablas
        call md_rutinas
        mov byte [m_tas], 1
        mov eax, plana              ; toda la memoria, directa
        xor ebx, ebx
.p:
        mov [m_pl+ebx*4], eax
        mov [m_pe+ebx*4], eax
        mov [m_px+ebx*4], eax
        inc ebx
        cmp ebx, 256
        jb .p
        mov esi, pr_buf
.prueba:
        cmp esi, [pr_fin]
        jae near .resumen
        mov [pr_nombre], esi
        mov eax, [esi]
        add eax, 7
        and eax, ~3
        add esi, eax
        mov [pr_ini], esi
        ; la memoria de antes
        mov ecx, [esi+21*4]
        lea ebx, [esi+22*4]
.m:
        test ecx, ecx
        jz .m1
        mov edx, [ebx]
        and edx, 0xFFFFFF
        xor edx, 1
        mov al, [ebx+4]
        mov [plana+edx], al
        add ebx, 8
        dec ecx
        jmp .m
.m1:
        mov [pr_desp], ebx
        mov edx, [esi+18*4]         ; las dos palabras ya leídas, en el PC
        and edx, 0xFFFFFE
        mov eax, [esi+19*4]
        mov [plana+edx], ax
        mov eax, [esi+20*4]
        mov [plana+edx+2], ax
        ; los registros
        push esi
        mov edi, m_d
        mov ecx, 15
        rep movsd
        pop esi
        mov eax, [esi+17*4]         ; el SR
        push eax
        call m_ccr_pon
        pop eax
        mov al, ah
        and al, 0xA7
        mov [m_srh], al
        mov byte [m_parado], 0
        mov eax, [esi+15*4]         ; usp
        mov edx, [esi+16*4]         ; ssp
        test byte [m_srh], 0x20
        jz .u
        xchg eax, edx
.u:
        mov [m_a+7*4], eax
        mov [m_osp], edx
        mov eax, plana
        mov [m_pcbase], eax
        add eax, [esi+18*4]
        mov [m_esi], eax
        mov dword [m_resto], 0
        mov byte [m_ipl], 0
        mov eax, 1                  ; una instrucción
        push esi
        call m68k_correr
        pop esi
        mov eax, 1
        sub eax, [m_resto]
        mov [pr_ciclos], eax
        ; lo de después
        mov esi, [pr_desp]
        mov [pr_fin1], esi
        xor ebp, ebp                ; EBP = fallos de esta prueba
        mov edi, m_d
        xor ecx, ecx
.r:
        mov eax, [edi+ecx*4]
        cmp eax, [esi+ecx*4]
        je .r1
        call .fallo_reg
.r1:
        inc ecx
        cmp ecx, 15
        jb .r
        mov eax, [m_a+7*4]          ; usp y ssp
        mov edx, [m_osp]
        test byte [m_srh], 0x20
        jz .u2
        xchg eax, edx
.u2:
        mov ecx, 15
        cmp eax, [esi+15*4]
        je .u3
        call .fallo_reg
.u3:
        mov ecx, 16
        mov eax, edx
        cmp eax, [esi+16*4]
        je .u4
        call .fallo_reg
.u4:
        call m_sr_lee
        and eax, 0xFFFF
        mov ecx, 17
        cmp eax, [esi+17*4]
        je .u5
        call .fallo_reg
.u5:
        mov eax, [m_esi]
        sub eax, [m_pcbase]
        and eax, 0xFFFFFF
        mov edx, [esi+18*4]
        and edx, 0xFFFFFF
        mov ecx, 18
        cmp eax, edx
        je .u6
        mov eax, [m_esi]
        sub eax, [m_pcbase]
        call .fallo_reg
.u6:
        ; la memoria de después
        mov ecx, [esi+19*4]
        lea ebx, [esi+20*4]
.f:
        test ecx, ecx
        jz .f9
        mov edx, [ebx]
        and edx, 0xFFFFFF
        xor edx, 1
        mov al, [plana+edx]
        cmp al, [ebx+4]
        je .f1
        call .fallo_mem
.f1:
        add ebx, 8
        dec ecx
        jmp .f
.f9:
        mov eax, [ebx]              ; los ciclos
        mov [pr_ciclos_ok], eax
        add ebx, 4
        mov [pr_sig], ebx
        inc dword [pr_n]
        test ebp, ebp
        jnz .mal1
        inc dword [pr_bien]
        mov eax, [pr_ciclos]
        cmp eax, [pr_ciclos_ok]
        jne .malc
        inc dword [pr_cbien]
        jmp .limpia
.malc:
        cmp dword [pr_cmal_n], 5    ; (los cinco primeros, para verlos)
        jae .limpia
        inc dword [pr_cmal_n]
        mov esi, t_ciclos
        call pon_cad
        call .nombre
        mov esi, t_dio
        call pon_cad
        mov eax, [pr_ciclos]
        call pon_dec
        mov esi, t_quiere
        call pon_cad
        mov eax, [pr_ciclos_ok]
        call pon_dec
        mov al, 10
        call pon_letra
        jmp .limpia
.mal1:
        mov al, 10
        call pon_letra
.limpia:
        ; la memoria, a cero otra vez
        mov esi, [pr_ini]
        mov ecx, [esi+21*4]
        lea ebx, [esi+22*4]
        call .cero
        mov esi, [pr_fin1]
        mov ecx, [esi+19*4]
        lea ebx, [esi+20*4]
        call .cero
        mov esi, [pr_ini]
        mov edx, [esi+18*4]
        and edx, 0xFFFFFE
        mov dword [plana+edx], 0
        mov esi, [pr_sig]
        jmp .prueba
.cero:
        test ecx, ecx
        jz .c9
        mov edx, [ebx]
        and edx, 0xFFFFFF
        xor edx, 1
        mov byte [plana+edx], 0
        add ebx, 8
        dec ecx
        jmp .cero
.c9:
        ret
; ECX=registro EAX=lo que ha dado
.fallo_reg:
        push esi
        call .cabeza
        push eax
        mov esi, n_regs
        lea esi, [esi+ecx*4]
        call pon_cad
        mov al, '='
        call pon_letra
        pop eax
        call pon_hex
        mov al, '/'
        call pon_letra
        mov esi, [pr_fin1]
        mov eax, [esi+ecx*4]
        call pon_hex
        pop esi
        ret
.fallo_mem:
        push esi
        call .cabeza
        push eax
        mov esi, t_mem
        call pon_cad
        mov eax, [ebx]
        call pon_hex
        mov al, '='
        call pon_letra
        pop eax
        and eax, 0xFF
        call pon_hex
        mov al, '/'
        call pon_letra
        mov eax, [ebx+4]
        call pon_hex
        pop esi
        ret
.cabeza:
        push eax
        inc ebp
        cmp ebp, 1
        jne .cb
        inc dword [pr_mal]
        call .nombre
        mov al, ':'
        call pon_letra
.cb:
        mov al, ' '
        call pon_letra
        pop eax
        ret
.nombre:
        pushad
        mov esi, [pr_nombre]
        mov ecx, [esi]
        add esi, 4
.n:
        lodsb
        call pon_letra
        dec ecx
        jnz .n
        popad
        ret
.resumen:
        mov esi, t_res1
        call pon_cad
        mov eax, [pr_bien]
        call pon_dec
        mov al, '/'
        call pon_letra
        mov eax, [pr_n]
        call pon_dec
        mov esi, t_res2
        call pon_cad
        mov eax, [pr_cbien]
        call pon_dec
        mov al, 10
        call pon_letra
        xor ebx, ebx
        cmp dword [pr_mal], 0
        je .x
        inc ebx
.x:
        mov eax, 1
        int 0x80

t_res1      db "bien: ", 0
t_res2      db "  con los ciclos justos: ", 0
t_ciclos    db "ciclos ", 0
t_dio       db ": ", 0
t_quiere    db " (la prueba: ", 0
t_mem       db "mem ", 0
n_regs      db "d0", 0, 0, "d1", 0, 0, "d2", 0, 0, "d3", 0, 0, "d4", 0, 0, "d5", 0, 0
            db "d6", 0, 0, "d7", 0, 0, "a0", 0, 0, "a1", 0, 0, "a2", 0, 0, "a3", 0, 0
            db "a4", 0, 0, "a5", 0, 0, "a6", 0, 0, "usp", 0, "ssp", 0, "sr", 0, 0, "pc", 0, 0


mdmain:
        cmp dword [ebp], 4
        jb near uso
        mov ebx, [ebp+8]
        mov ecx, rom_buf
        mov edx, ROM_BUF
        call lee_archivo
        test eax, eax
        jz near mal
        mov dword [rom], rom_buf
        mov [rom_tam], eax
        ; los colores: 3 bits de rojo, 3 de verde, 2 de azul
        xor ebx, ebx
.k:
        mov eax, ebx
        and eax, 7
        shl eax, 5
        mov edx, ebx
        shr edx, 3
        and edx, 7
        shl edx, 2
        or eax, edx
        mov edx, ebx
        shr edx, 7
        and edx, 3
        or eax, edx
        mov [md_col9+ebx], al
        inc ebx
        cmp ebx, 512
        jb .k
        mov dword [psg_hz], 22050
        call md_rutinas
        call md_cargar
        call md_arrancar
        ; los cuadros
        mov esi, [ebp+12]
        xor eax, eax
.n:
        movzx edx, byte [esi]
        sub edx, '0'
        cmp edx, 9
        ja .n1
        imul eax, 10
        add eax, edx
        inc esi
        jmp .n
.n1:
        mov [ncuadros], eax
        ; el guion
        cmp dword [ebp], 5
        jb .sg
        mov ebx, [ebp+20]
        cmp byte [ebx], '-'
        je .sg
        mov ecx, guion
        mov edx, GUION_MAX
        call lee_archivo
        shr eax, 1
        mov [nguion], eax
.sg:
        ; el sonido
        cmp dword [ebp], 7
        jb .ss
        mov ebx, [ebp+28]
        cmp byte [ebx], '-'
        je .ss
        cmp byte [ebx], '0'         ; «0»: sin sonido (como en Hispa-Pentium)
        jne .ss1
        mov dword [psg_hz], 0
        call psg_frecuencia
        jmp .ss
.ss1:
        mov ecx, 0x241
        mov edx, 0o644
        mov eax, 5
        int 0x80
        test eax, eax
        js .ss
        mov [fd_son], eax
.ss:
        ; la RAM del cartucho
        cmp dword [ebp], 8
        jb .sv
        mov ebx, [ebp+32]
        mov ecx, sram
        mov edx, 0x10000
        call lee_archivo
.sv:
        mov byte [dibujar], 1
        mov ebx, [ebp+16]           ; (IMAGEN «-»: sin pintar, para medir)
        cmp word [ebx], '-'
        jne .dib
        mov byte [dibujar], 0
.dib:
        xor ebx, ebx
.cuadro:
        cmp ebx, [ncuadros]
        jae .fin
        xor eax, eax
        cmp ebx, [nguion]
        jae .sb
        mov ax, [guion+ebx*2]
.sb:
        mov ecx, eax
        shr ecx, 15
        mov [seis_md], cl
        and eax, 0x0FFF
        mov [pad_md], ax
        mov dword [psg_n], 0
        call md_cuadro
        cmp dword [fd_son], 0
        je .sb1
        pushad
        mov ebx, [fd_son]
        mov ecx, psg_buf
        mov edx, [psg_n]
        shl edx, 1
        mov eax, 4
        int 0x80
        popad
.sb1:
        inc ebx
        jmp .cuadro
.fin:
        cmp byte [dibujar], 0
        je near .sinppm
        ; la imagen, en PPM
        mov esi, imagen
        mov edi, ppm_buf
        mov ecx, 320 * 224
.p:
        lodsb
        movzx eax, al
        mov edx, eax                ; rojo: 3 bits
        shr edx, 5
        imul edx, 255
        push eax
        mov eax, edx
        xor edx, edx
        push ecx
        mov ecx, 7
        div ecx
        pop ecx
        stosb
        pop eax
        mov edx, eax                ; verde: 3 bits
        shr edx, 2
        and edx, 7
        imul edx, 255
        push eax
        mov eax, edx
        xor edx, edx
        push ecx
        mov ecx, 7
        div ecx
        pop ecx
        stosb
        pop eax
        and eax, 3                  ; azul: 2 bits
        imul eax, 85
        stosb
        dec ecx
        jnz .p
        mov ebx, [ebp+16]
        mov ecx, 0x241
        mov edx, 0o644
        mov eax, 5
        int 0x80
        test eax, eax
        js near mal
        mov ebx, eax
        mov ecx, ppm
        mov edx, ppm_n
        mov eax, 4
        int 0x80
        mov ecx, ppm_buf
        mov edx, 320 * 224 * 3
        mov eax, 4
        int 0x80
        mov eax, 6
        int 0x80
.sinppm:
        ; la memoria
        cmp dword [ebp], 6
        jb .sm
        mov ebx, [ebp+24]
        cmp byte [ebx], '-'
        je .sm
        xor ecx, ecx                ; (la del 68000, en su orden)
.u:
        mov ax, [md_ram+ecx]
        xchg al, ah
        mov [ppm_buf+ecx], ax
        add ecx, 2
        cmp ecx, 0x10000
        jb .u
        mov ecx, 0x241
        mov edx, 0o644
        mov eax, 5
        int 0x80
        test eax, eax
        js near mal
        mov ebx, eax
        mov ecx, ppm_buf
        mov edx, 0x10000
        mov eax, 4
        int 0x80
        mov ecx, z80_ram
        mov edx, 0x2000
        mov eax, 4
        int 0x80
        mov ecx, sram
        mov edx, 0x10000
        mov eax, 4
        int 0x80
        mov ecx, cram               ; (y la del VDP: CRAM, VSRAM y VRAM)
        mov edx, 128 + 80
        mov eax, 4
        int 0x80
        mov ecx, vram
        mov edx, 0x10000
        mov eax, 4
        int 0x80
        mov eax, 6
        int 0x80
.sm:
        xor ebx, ebx
        mov eax, 1
        int 0x80

ppm         db "P6", 10, "320 224", 10, "255", 10
ppm_n       equ $ - ppm

%include "m68k.inc"
%include "m68k_gen.inc"
%include "vdp.inc"
%include "bus.inc"
%include "md.inc"
%include "ym.inc"
%include "ym_tablas.inc"
%include "z80.inc"
%include "psg.inc"
fin_archivo:
%include "datos.inc"
PR_MAX      equ 64 * 1024 * 1024
sal_n       resd 1
pr_fin      resd 1
pr_nombre   resd 1
pr_ini      resd 1
pr_desp     resd 1
pr_fin1     resd 1
pr_sig      resd 1
pr_n        resd 1
pr_bien     resd 1
pr_mal      resd 1
pr_cbien    resd 1
pr_cmal_n   resd 1
pr_ciclos   resd 1
pr_ciclos_ok resd 1
sal_buf     resb 4096
GUION_MAX   equ 0x40000
ROM_BUF     equ 0x400000 * 2 + 0x20000
fd_son      resd 1
ncuadros    resd 1
nguion      resd 1
            alignb 4
guion       resb GUION_MAX
ppm_buf     resb 320 * 240 * 3
rom_buf     resb ROM_BUF
plana       resb 0x1000000 + 16
pr_buf      resb PR_MAX
fin_memoria:
