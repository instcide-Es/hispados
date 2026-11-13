; =============================================================================
;  smsnativo - el núcleo de Hispa-System como programa de Linux, para las pruebas
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Los mismos archivos del emulador (programas/hispasms/*.inc), sin ventana:
;  carga una ROM, corre unos cuadros con los botones de un guion y deja la
;  imagen y la memoria en archivos. Sirve para probar el procesador en el
;  servidor en segundos y para saber qué tiene que verse en QEMU.
;
;  uso: smsnativo ROM CUADROS IMAGEN.pgm [GUION|-] [MEMORIA|-] [SONIDO|-] [SAV]
;    GUION    un octeto por cuadro: los botones del mando 1 (bit 0 arriba,
;             1 abajo, 2 izquierda, 3 derecha, 4 botón 1, 5 botón 2) y el 7,
;             el de pausa
;    MEMORIA  la RAM (8 KB) y la del cartucho (32 KB), tal cual
;    SONIDO   las muestras (22.050 por segundo, 16 bits con signo, un canal)
;    SAV      una partida guardada, que se pone en la RAM del cartucho al
;             encender (como hace HISPASMS.HVP con el .SAV)
;  smsnativo -c PROGRAMA.COM
;    un programa de CP/M (las pruebas del Z80, zexdoc y zexall): memoria
;    plana de 64 KB, el programa en 100h y las funciones 2 y 9 del sistema
;    (escribir), que salen por la salida normal; acaba al saltar a 0
;  Se hace solo con NASM (la cabecera ELF va escrita aquí; sin enlazador ni
;  biblioteca de C):  nasm -f bin -i programas/hispasms/ -o smsnativo ...
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
        jb near .uso
        mov ebx, [ebp+8]
        cmp word [ebx], '-c'
        je near cpm
        ; la ROM
        xor ecx, ecx
        mov eax, 5
        int 0x80
        test eax, eax
        js near .mal
        mov [fd], eax
        mov dword [rom], rom_buf
        mov ecx, rom_buf
        mov edx, ROM_BUF
        call lee_todo
        mov eax, ecx
        sub eax, rom_buf
        mov [rom_tam], eax
        test eax, eax
        jz near .mal
        mov dword [psg_hz], 22050
        call sms_cargar
        call sms_arrancar
        cmp dword [ebp], 4
        jb near .uso
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
        ; el sonido
        cmp dword [ebp], 7
        jb .ss
        mov ebx, [ebp+28]
        cmp byte [ebx], '-'
        je .ss
        mov ecx, 0x241
        mov edx, 0o644
        mov eax, 5
        int 0x80
        test eax, eax
        js .ss
        mov [fd_son], eax
.ss:
        ; la partida guardada: a la RAM del cartucho
        cmp dword [ebp], 8
        jb .sv
        mov ebx, [ebp+32]
        xor ecx, ecx
        mov eax, 5
        int 0x80
        test eax, eax
        js .sv
        mov ebx, eax
        mov ecx, sram
        mov edx, 0x8000
        mov eax, 3
        int 0x80
.sv:
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
        mov ah, al
        and al, 0x3F
        mov [pad_estado], al
        shr ah, 7
        mov [pausa_tecla], ah
        call sms_cuadro
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
        mov dword [psg_n], 0
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
        mov edx, 256 * 192
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
        mov edx, 0x2000
        mov eax, 4
        int 0x80
        mov ecx, sram
        mov edx, 0x8000
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

; lee hasta EDX octetos del archivo en ECX (ECX queda detrás de lo leído)
lee_todo:
        mov ebx, [fd]
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
        ret

; ---- un programa de CP/M ----
cpm:
        mov ebx, [ebp+12]
        xor ecx, ecx
        mov eax, 5
        int 0x80
        test eax, eax
        js near _start.mal
        mov [fd], eax
        mov ecx, cpm_mem + 0x100
        mov edx, 0xE000
        call lee_todo
        call z80_tablas
        call z80_reiniciar
        mov dword [map_tipo], 0
        mov eax, cpm_mem            ; la memoria, plana
        xor ebx, ebx
.p:
        mov [pg_lee+ebx*4], eax
        mov [pg_esc+ebx*4], eax
        inc ebx
        cmp ebx, 64
        jb .p
        sub eax, 0x10000
        mov [pg_lee+64*4], eax
        mov [pg_esc+64*4], eax
        mov dword [cpm_mem], 0x76FED3       ; 0: OUT (FEh),A ; HALT
        mov byte [cpm_mem+5], 0xC3          ; 5: JP F000h (y en 6, la cima)
        mov word [cpm_mem+6], 0xF000
        mov dword [cpm_mem+0xF000], 0xC9FFD3    ; OUT (FFh),A ; RET
        mov dword [z_in], cpm_in
        mov dword [z_out], cpm_out
        mov dword [z_pc], 0x100
        mov word [z_sp], 0xF000
.c:
        mov edi, 1000000
        call z80_correr
        cmp byte [cpm_fin], 0
        je .c
        call cpm_vacia
        xor ebx, ebx
        mov eax, 1
        int 0x80

cpm_in:
        mov al, 0xFF
        ret
cpm_out:
        cmp dl, 0xFE
        jne .f
        mov byte [cpm_fin], 1
        ret
.f:
        pushad
        mov al, [z_c]
        cmp al, 2
        jne .s9
        mov al, [z_e]
        call cpm_letra
        jmp .x
.s9:
        cmp al, 9
        jne .x
        movzx esi, word [z_de]
.s:
        mov al, [cpm_mem+esi]
        cmp al, '$'
        je .x
        call cpm_letra
        inc si
        jmp .s
.x:
        popad
        ret
cpm_letra:
        mov edi, [cpm_n]
        mov [cpm_buf+edi], al
        inc dword [cpm_n]
        cmp al, 10
        je cpm_vacia
        cmp dword [cpm_n], 1000
        jae cpm_vacia
        ret
cpm_vacia:
        pushad
        mov ebx, 1
        mov ecx, cpm_buf
        mov edx, [cpm_n]
        mov eax, 4
        int 0x80
        mov dword [cpm_n], 0
        popad
        ret

%ifdef TRAZA
; 16 octetos por instrucción: PC, AF, BC, DE, HL, IX, IY, SP
z_traza:
        pushad
        mov edi, [tr_p]
        mov eax, esi
        and eax, 0xFFFF
        stosw
        mov esi, z_af
        movsd
        movsd
        movsd
        mov ax, [z_sp]
        stosw
        mov [tr_p], edi
        cmp edi, tr_buf + TR_MAX
        jb .x
        call tr_vacia
.x:
        popad
        mov ebp, esi                ; (FETCH8: z80.inc va más abajo)
        shr ebp, 10
        mov ebp, [pg_lee+ebp*4]
        movzx eax, byte [ebp+esi]
        inc esi
        inc byte [z_r]
        jmp [tab_op+eax*4]
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
traza:
        ret
%endif

pgm         db "P5", 10, "256 192", 10, "255", 10
pgm_n       equ $ - pgm

%include "z80.inc"
%include "vdp.inc"
%include "psg.inc"
%include "mapas.inc"
fin_archivo:
%include "datos.inc"
GUION_MAX   equ 65536
TR_MAX      equ 65536
ROM_BUF     equ 4 * 1024 * 1024
fd          resd 1
fd_son      resd 1
ncuadros    resd 1
nguion      resd 1
tr_p        resd 1
cpm_n       resd 1
cpm_fin     resb 1
            alignb 4
guion       resb GUION_MAX
tr_buf      resb TR_MAX + 16
cpm_buf     resb 1024
cpm_mem     resb 0x10000 + 16
rom_buf     resb ROM_BUF + 0x10000
fin_memoria:
