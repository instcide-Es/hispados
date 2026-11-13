; =============================================================================
;  nesnativo - el núcleo de Hispa-Nes como programa de Linux, para las pruebas
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Los mismos archivos del emulador (programas/hispanes/*.inc), sin ventana:
;  carga una ROM, corre unos cuadros con los botones de un guion y deja la
;  imagen y la memoria en archivos. Sirve para probar el procesador en el
;  servidor en milisegundos y para saber qué tiene que verse en QEMU.
;
;  uso: nesnativo ROM CUADROS IMAGEN.pgm [GUION|-] [MEMORIA|-] [PC en hex]
;    GUION    un octeto por cuadro: los botones del mando 1
;    MEMORIA  la RAM (2 KB) y la del cartucho (8 KB), tal cual
;    PC       empieza ahí en vez de en el vector de «reset»
;  Con -DTRAZA escribe por la salida normal 12 octetos por instrucción: PC
;  (2), A, X, Y, P, S, 0 y el tiempo en puntos de imagen (4).
;  Se hace solo con NASM (la cabecera ELF va escrita aquí; sin enlazador ni
;  biblioteca de C):  nasm -f bin -i programas/hispanes/ -o nesnativo ...
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
        cmp dword [ebp], 4
        jb near .uso
        ; la ROM
        mov ebx, [ebp+8]
        xor ecx, ecx
        mov eax, 5
        int 0x80
        test eax, eax
        js near .mal
        mov [fd], eax
        mov ebx, eax
        mov ecx, cab
        mov edx, 16
        mov eax, 3
        int 0x80
        mov esi, cab
        call nes_cabecera
        jc near .mal
        cmp dword [rom_entren], 0
        je .se
        mov ebx, [fd]
        mov ecx, entren
        mov edx, 512
        mov eax, 3
        int 0x80
.se:
        mov dword [rom_prg], prg
        mov edx, [rom_prg_n]
        shl edx, 13
        mov ecx, prg
        call lee_todo
        mov dword [rom_chr], chr
        mov edx, [rom_chr_n]
        shl edx, 10
        jz .sc
        mov ecx, chr
        call lee_todo
.sc:
        call nes_arrancar
        cmp dword [rom_entren], 0
        je .se2
        mov esi, entren
        mov edi, sram + 0x1000
        mov ecx, 512
        rep movsb
.se2:
        ; cuántos cuadros
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
        xor ecx, ecx
        mov eax, 5
        int 0x80
        test eax, eax
        js .sg
        mov ebx, eax
        mov ecx, guion
        mov edx, GUION_MAX
        mov eax, 3
        int 0x80
        mov [nguion], eax
.sg:
        ; el PC
        cmp dword [ebp], 7
        jb .spc
        mov esi, [ebp+28]
        xor eax, eax
.h:
        movzx edx, byte [esi]
        test edx, edx
        jz .h2
        sub edx, '0'
        cmp edx, 9
        jbe .h1
        sub edx, 7
        and edx, 15
.h1:
        shl eax, 4
        or eax, edx
        inc esi
        jmp .h
.h2:
        mov [reg_pc], eax
.spc:
%ifdef TRAZA
        mov dword [traza_fn], traza
        mov dword [tr_p], tr_buf
%endif
        mov byte [dibujar], 1
        xor ebx, ebx
.cuadro:
        cmp ebx, [ncuadros]
        jae .fin
        xor eax, eax
        cmp ebx, [nguion]
        jae .sb
        mov al, [guion+ebx]
.sb:
        mov [pad_estado], al
        call nes_cuadro
        mov dword [apu_n], 0
        inc ebx
        jmp .cuadro
.fin:
%ifdef TRAZA
        call tr_vacia
%endif
        ; la imagen
        mov ebx, [ebp+16]
        mov ecx, 0x241              ; crear, truncar, escribir
        mov edx, 0o644
        mov eax, 5
        int 0x80
        test eax, eax
        js .mal
        mov ebx, eax
        mov ecx, pgm
        mov edx, pgm_n
        mov eax, 4
        int 0x80
        mov ecx, imagen
        mov edx, 256 * 240
        mov eax, 4
        int 0x80
        mov eax, 6
        int 0x80
        ; la memoria
        cmp dword [ebp], 6
        jb .sm
        mov ebx, [ebp+24]
        cmp byte [ebx], '-'
        je .sm
        mov ecx, 0x241
        mov edx, 0o644
        mov eax, 5
        int 0x80
        test eax, eax
        js .mal
        mov ebx, eax
        mov ecx, ram
        mov edx, 0x800
        mov eax, 4
        int 0x80
        mov ecx, sram
        mov edx, 0x2000
        mov eax, 4
        int 0x80
        mov eax, 6
        int 0x80
.sm:
        xor ebx, ebx
        mov eax, 1
        int 0x80
.uso:
        mov ebx, 2
        mov eax, 1
        int 0x80
.mal:
        mov ebx, 1
        mov eax, 1
        int 0x80

; lee EDX octetos del archivo en ECX
lee_todo:
        mov ebx, [fd]
.l:
        test edx, edx
        jz .x
        mov eax, 3
        int 0x80
        test eax, eax
        jle .x
        add ecx, eax
        sub edx, eax
        jmp .l
.x:
        ret

%ifdef TRAZA
traza:
        mov edi, [tr_p]
        mov eax, [reg_pc]
        stosw
        mov eax, [reg_a]            ; A, X, Y, S
        stosw
        shr eax, 16
        mov edx, eax
        stosb
        mov bh, [f_c]
        call p_compone
        stosb
        mov al, dh
        stosb
        xor al, al
        stosb
        mov eax, [cuadros]          ; y el tiempo, en puntos de imagen
        imul eax, 262 * 341
        imul edx, [linea], 341
        add eax, edx
        add eax, [punto_fin]
        sub eax, [cpu_resto]
        stosd
        mov [tr_p], edi
        cmp edi, tr_buf + TR_MAX
        jb .x
        call tr_vacia
.x:
        ret
tr_vacia:
        pushad
        mov ebx, 1
        mov ecx, tr_buf
        mov edx, [tr_p]
        sub edx, ecx
        mov eax, 4
        int 0x80
        mov dword [tr_p], tr_buf
        popad
        ret
%endif

pgm         db "P5", 10, "256 240", 10, "255", 10
pgm_n       equ $ - pgm

%include "cpu.inc"
%include "ppu.inc"
%include "apu.inc"
%include "mapas.inc"
fin_archivo:
%include "datos.inc"
GUION_MAX   equ 65536
TR_MAX      equ 65536
fd          resd 1
ncuadros    resd 1
nguion      resd 1
tr_p        resd 1
cab         resb 16
entren      resb 512
guion       resb GUION_MAX
tr_buf      resb TR_MAX + 16
prg         resb 2 * 1024 * 1024
chr         resb 1024 * 1024
fin_memoria:
