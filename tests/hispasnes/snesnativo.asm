; =============================================================================
;  snesnativo - el núcleo de Hispa-Snes como programa de Linux, para las pruebas
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Los mismos archivos del emulador (programas/hispasnes/*.inc), sin ventana.
;
;  snesnativo -c CASOS     las pruebas de un solo paso del 65C816 (de
;                          tests/hispasnes/sst.py): memoria plana de 16 MB
;  snesnativo -s CASOS     lo mismo, del SPC700 (64 KB planos)
;  snesnativo ROM CUADROS IMAGEN.ppm [GUION|-] [SONIDO|-] [SRM|-] [MEMORIA]
;      carga una ROM, corre CUADROS cuadros con los botones del GUION (dos
;      octetos por cuadro: los 16 bits del mando 1, como $4218) y deja la
;      imagen (PPM de 256x224), el sonido (32.000 muestras por segundo, 16
;      bits con signo, estéreo), la RAM del cartucho (SRM) y la memoria
;      (MEMORIA: la RAM de 128 KB, la VRAM, la del sonido...)
;  Se hace solo con NASM (la cabecera ELF va escrita aquí; sin enlazador ni
;  biblioteca de C):  nasm -f bin -i programas/hispasnes/ -o snesnativo ...
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
        cmp word [ebx], '-c'
        je near prueba_cpu
        cmp word [ebx], '-s'
        je near prueba_spc
        jmp rom_modo

uso:
        mov ebx, 2
        mov eax, 1
        int 0x80
salir_mal:
        mov ebx, 1
        mov eax, 1
        int 0x80
salir_bien:
        xor ebx, ebx
        mov eax, 1
        int 0x80

; EBX = nombre -> ECX..: el archivo entero, en buf_casos; EAX = octetos
lee_archivo:
        push ecx
        xor ecx, ecx
        mov eax, 5
        int 0x80
        pop ecx
        test eax, eax
        js .mal
        mov ebx, eax
        push ecx
.l:
        mov edx, 0x100000
        mov eax, 3
        int 0x80
        test eax, eax
        jle .f
        add ecx, eax
        jmp .l
.f:
        mov eax, ecx
        pop ecx
        sub eax, ecx
        push eax
        mov eax, 6
        int 0x80
        pop eax
        ret
.mal:
        xor eax, eax
        ret

; ---- salida: texto ----
; ESI = cadena acabada en 0
pon_cad:
        pushad
        mov edi, esi
        xor ecx, ecx
.c:
        cmp byte [edi+ecx], 0
        je .e
        inc ecx
        jmp .c
.e:
        mov edx, ecx
        mov ecx, esi
        mov ebx, 1
        mov eax, 4
        int 0x80
        popad
        ret
; EAX = número, en hexadecimal (y un espacio)
pon_hex:
        pushad
        mov edi, txt_num
        mov ecx, 8
.c:
        rol eax, 4
        mov dl, al
        and dl, 15
        add dl, '0'
        cmp dl, '9'
        jbe .d
        add dl, 7
.d:
        mov [edi], dl
        inc edi
        dec ecx
        jnz .c
        mov word [edi], ' '
        mov esi, txt_num
        call pon_cad
        popad
        ret
pon_nl:
        push esi
        mov esi, t_nl
        call pon_cad
        pop esi
        ret

; =============================================================================
;  Las pruebas de un solo paso del 65C816
; =============================================================================
prueba_cpu:
        mov ebx, [ebp+12]
        mov ecx, buf_casos
        call lee_archivo
        add eax, buf_casos
        mov [casos_fin], eax
        mov eax, mem_plana          ; la memoria, plana
        xor ebx, ebx
.p:
        mov [pag_lee+ebx*4], eax
        mov [pag_esc+ebx*4], eax
        inc ebx
        cmp ebx, 2048
        jb .p
        mov esi, buf_casos
        mov dword [n_caso], 0
.caso:
        cmp esi, [casos_fin]
        jae near .fin
        ; el estado de antes
        mov edi, ini_regs
        mov ecx, 10
        rep movsd
        lodsd
        mov ecx, eax
.r:
        test ecx, ecx
        jz .r1
        lodsd
        mov edx, eax
        lodsd
        mov [mem_plana+edx], al
        dec ecx
        jmp .r
.r1:
        mov [p_fin], esi            ; (el estado de después)
        mov eax, [ini_regs+9*4]     ; E
        mov [f_e], al
        mov byte [f_m], 1
        mov byte [f_x], 1
        mov ecx, [ini_regs+4*4]
        mov ebp, [ini_regs+5*4]
        mov eax, [ini_regs+2*4]
        call cpu_p_pon
        mov eax, [ini_regs+1*4]
        mov [r_s], eax
        call cpu_modo_e             ; (en emulación: X e Y de 8 bits, S en 01xxh)
        mov [r_x], ecx
        mov [r_y], ebp
        mov eax, [ini_regs+3*4]
        mov [r_a], eax
        mov eax, [ini_regs+7*4]
        mov [r_d], eax
        call cpu_d_extra
        mov eax, [ini_regs+6*4]
        shl eax, 16
        mov [r_dbr], eax
        mov eax, [ini_regs+8*4]
        shl eax, 16
        mov [r_pbr], eax
        mov eax, [ini_regs]
        mov [r_pc], eax
        mov byte [nmi_pend], 0
        mov byte [irq_linea], 0
        mov byte [cpu_wai], 0
        mov byte [cpu_stp], 0
        ; ¿MVN o MVP? hasta que acabe
        mov edx, [ini_regs+8*4]
        shl edx, 16
        or edx, [ini_regs]
        movzx eax, byte [mem_plana+edx]
        mov [opcode], eax
        cmp al, 0x44
        je .mv
        cmp al, 0x54
        je .mv
        mov edi, 1
        call cpu_correr
        mov eax, 1
        sub eax, edi
        mov [ciclos], eax
        jmp .mira
.mv:
        mov dword [ciclos], 0
        mov ecx, 70000
.mv1:
        mov edi, 1
        call cpu_correr
        mov eax, [ini_regs]
        add eax, 3
        and eax, 0xFFFF
        cmp eax, [r_pc]
        je .mira
        dec ecx
        jnz .mv1
.mira:
        ; el estado de después
        mov esi, [p_fin]
        mov edi, fin_regs
        mov ecx, 10
        rep movsd
        mov byte [hay_mal], 0
        mov eax, [r_pc]
        mov ecx, 0
        call .compara
        mov eax, [r_s]
        mov ecx, 1
        call .compara
        call cpu_p_lee
        mov ecx, 2
        call .compara
        mov eax, [r_a]
        mov ecx, 3
        call .compara
        mov eax, [r_x]
        mov ecx, 4
        call .compara
        mov eax, [r_y]
        mov ecx, 5
        call .compara
        mov eax, [r_dbr]
        shr eax, 16
        mov ecx, 6
        call .compara
        mov eax, [r_d]
        mov ecx, 7
        call .compara
        mov eax, [r_pbr]
        shr eax, 16
        mov ecx, 8
        call .compara
        movzx eax, byte [f_e]
        mov ecx, 9
        call .compara
        lodsd
        mov ecx, eax
.m:
        test ecx, ecx
        jz .m1
        lodsd
        mov edx, eax
        lodsd
        movzx ebx, byte [mem_plana+edx]
        cmp eax, ebx
        je .m0
        push esi
        mov esi, t_malram
        call pon_cad
        mov eax, [n_caso]
        call pon_hex
        mov eax, edx
        call pon_hex
        mov eax, ebx
        call pon_hex
        mov eax, [esi-4+0]          ; (no: el esperado ya no está a mano)
        pop esi
        mov eax, [esi-4]
        call pon_hex
        call pon_nl
        mov byte [hay_mal], 1
.m0:
        dec ecx
        jmp .m
.m1:
        lodsd                       ; los ciclos
        cmp byte [hay_mal], 0
        jne .sig
        mov ebx, [opcode]
        cmp bl, 0x44
        je .sig
        cmp bl, 0x54
        je .sig
        cmp bl, 0xCB                ; (WAI y STP: el resto del tramo se va)
        je .sig
        cmp bl, 0xDB
        je .sig
        cmp eax, [ciclos]
        je .sig
        push esi
        push eax
        mov esi, t_ciclos
        call pon_cad
        mov eax, [n_caso]
        call pon_hex
        mov eax, [ciclos]
        call pon_hex
        pop eax
        call pon_hex
        call pon_nl
        pop esi
.sig:
        inc dword [n_caso]
        jmp .caso
.fin:
        jmp salir_bien
; (interna) EAX = lo mío, ECX = el número del registro
.compara:
        cmp eax, [fin_regs+ecx*4]
        je .c1
        push esi
        push eax
        mov esi, t_mal
        call pon_cad
        mov eax, [n_caso]
        call pon_hex
        mov esi, [nombres+ecx*4]
        call pon_cad
        pop eax
        call pon_hex
        mov eax, [fin_regs+ecx*4]
        call pon_hex
        mov eax, [ini_regs+ecx*4]
        call pon_hex
        call pon_nl
        pop esi
        mov byte [hay_mal], 1
.c1:
        ret

; =============================================================================
;  Las pruebas de un solo paso del SPC700 (64 KB planos, sin registros)
; =============================================================================
prueba_spc:
        mov ebx, [ebp+12]
        mov ecx, buf_casos
        call lee_archivo
        add eax, buf_casos
        mov [casos_fin], eax
        mov byte [s_plano], 1
        mov byte [s_ipl], 0
        mov esi, buf_casos
        mov dword [n_caso], 0
.caso:
        cmp esi, [casos_fin]
        jae near salir_bien
        mov edi, ini_regs
        mov ecx, 6
        rep movsd
        lodsd
        mov ecx, eax
.r:
        test ecx, ecx
        jz .r1
        lodsd
        mov edx, eax
        lodsd
        mov [aram+edx], al
        dec ecx
        jmp .r
.r1:
        mov [p_fin], esi
        mov eax, [ini_regs]
        mov [s_pc], eax
        mov eax, [ini_regs+4]
        mov [s_ya], al
        mov eax, [ini_regs+12]
        mov [s_ya+1], al
        mov eax, [ini_regs+8]
        mov [s_x], al
        mov eax, [ini_regs+16]
        mov [s_sp], al
        mov eax, [ini_regs+20]
        call s_psw_pon
        mov byte [s_parado], 0
        mov eax, [ini_regs]
        movzx eax, byte [aram+eax]
        mov [opcode], eax
        mov edi, 1
        call spc_correr
        mov eax, 1
        sub eax, edi
        mov [ciclos], eax
        mov esi, [p_fin]
        mov edi, fin_regs
        mov ecx, 6
        rep movsd
        mov byte [hay_mal], 0
        movzx eax, word [s_pc]
        mov ecx, 0
        call .compara
        movzx eax, byte [s_ya]
        mov ecx, 1
        call .compara
        movzx eax, byte [s_x]
        mov ecx, 2
        call .compara
        movzx eax, byte [s_ya+1]
        mov ecx, 3
        call .compara
        movzx eax, byte [s_sp]
        mov ecx, 4
        call .compara
        call s_psw_lee
        mov ecx, 5
        call .compara
        lodsd
        mov ecx, eax
.m:
        test ecx, ecx
        jz .m1
        lodsd
        mov edx, eax
        lodsd
        movzx ebx, byte [aram+edx]
        cmp eax, ebx
        je .m0
        push esi
        mov esi, t_malram
        call pon_cad
        mov eax, [n_caso]
        call pon_hex
        mov eax, edx
        call pon_hex
        mov eax, ebx
        call pon_hex
        pop esi
        mov eax, [esi-4]
        call pon_hex
        call pon_nl
        mov byte [hay_mal], 1
.m0:
        dec ecx
        jmp .m
.m1:
        lodsd
        cmp byte [hay_mal], 0
        jne .sig
        mov ebx, [opcode]
        cmp bl, 0xEF
        je .sig
        cmp bl, 0xFF
        je .sig
        cmp eax, [ciclos]
        je .sig
        push esi
        push eax
        mov esi, t_ciclos
        call pon_cad
        mov eax, [n_caso]
        call pon_hex
        mov eax, [ciclos]
        call pon_hex
        pop eax
        call pon_hex
        call pon_nl
        pop esi
.sig:
        inc dword [n_caso]
        jmp .caso
.compara:
        cmp eax, [fin_regs+ecx*4]
        je .c1
        push esi
        push eax
        mov esi, t_mal
        call pon_cad
        mov eax, [n_caso]
        call pon_hex
        mov esi, [nombres_s+ecx*4]
        call pon_cad
        pop eax
        call pon_hex
        mov eax, [fin_regs+ecx*4]
        call pon_hex
        mov eax, [ini_regs+ecx*4]
        call pon_hex
        call pon_nl
        pop esi
        mov byte [hay_mal], 1
.c1:
        ret
; =============================================================================
;  Una ROM: unos cuadros, y la imagen, el sonido y la memoria a archivos
; =============================================================================
rom_modo:
        cmp dword [ebp], 4
        jb near uso
        mov ebx, [ebp+8]
        mov ecx, rom_buf
        call lee_archivo
        test eax, eax
        jz near salir_mal
        mov dword [rom], rom_buf
        mov [rom_tam], eax
        call cart_cargar
        ; cuántos cuadros
        mov esi, [ebp+12]
        call num
        mov [ncuadros], eax
        ; el guion
        cmp dword [ebp], 5
        jb .sg
        mov ebx, [ebp+20]
        cmp byte [ebx], '-'
        je .sg
        mov ecx, guion
        call lee_archivo
        shr eax, 1
        mov [nguion], eax
.sg:
        ; el sonido
        cmp dword [ebp], 6
        jb .ss
        mov ebx, [ebp+24]
        cmp byte [ebx], '-'
        je .ss
        call crea
        mov [fd_son], eax
.ss:
        mov byte [ppu_15], 1
        mov dword [snd_hz_sal], 32000
        mov byte [snd_estereo], 1
        call ppu_tablas
        call snes_arrancar
        ; la partida guardada
        cmp dword [ebp], 7
        jb .sv
        mov ebx, [ebp+28]
        cmp byte [ebx], '-'
        je .sv
        mov ecx, sram
        call lee_archivo
.sv:
        ; lo que es
        mov esi, cart_titulo
        call pon_cad
        call pon_nl
        mov eax, [cart_mapa]
        call pon_hex
        mov eax, [cart_chip]
        call pon_hex
        mov eax, [sram_tam]
        call pon_hex
        mov eax, [cart_pal]
        call pon_hex
        call pon_nl
        xor ebx, ebx
.cuadro:
        cmp ebx, [ncuadros]
        jae .fin
        xor eax, eax
        cmp ebx, [nguion]
        jae .sb
        mov ax, [guion+ebx*2]
.sb:
        mov [pad], ax
        mov byte [dibujar], 1
        call snes_cuadro
        cmp dword [fd_son], 0
        je .sb1
        pushad
        mov ebx, [fd_son]
        mov ecx, snd_sal
        mov edx, [snd_sal_n]
        shl edx, 2
        mov eax, 4
        int 0x80
        popad
.sb1:
        inc ebx
        jmp .cuadro
.fin:
        ; la imagen: PPM de 256x224
        mov ebx, [ebp+16]
        call crea
        mov ebx, eax
        mov ecx, ppm
        mov edx, ppm_n
        mov eax, 4
        int 0x80
        push ebx
        mov esi, imagen15
        mov edi, rgb_buf
        mov ecx, 256 * 224
.px:
        lodsw
        movzx edx, ax
        mov eax, edx
        and eax, 0x1F
        call .cinco
        stosb
        mov eax, edx
        shr eax, 5
        and eax, 0x1F
        call .cinco
        stosb
        mov eax, edx
        shr eax, 10
        and eax, 0x1F
        call .cinco
        stosb
        dec ecx
        jnz .px
        pop ebx
        mov ecx, rgb_buf
        mov edx, 256 * 224 * 3
        mov eax, 4
        int 0x80
        mov eax, 6
        int 0x80
        ; la RAM del cartucho
        cmp dword [ebp], 7
        jb .sm
        mov ebx, [ebp+28]
        cmp byte [ebx], '-'
        je .sm
        cmp dword [sram_tam], 0
        je .sm
        call crea
        mov ebx, eax
        mov ecx, sram
        mov edx, [sram_tam]
        mov eax, 4
        int 0x80
        mov eax, 6
        int 0x80
.sm:
        ; la memoria
        cmp dword [ebp], 8
        jb .x
        mov ebx, [ebp+32]
        call crea
        mov ebx, eax
        mov ecx, wram
        mov edx, 0x20000
        mov eax, 4
        int 0x80
        mov ecx, vram
        mov edx, 0x10000
        mov eax, 4
        int 0x80
        mov ecx, cgram
        mov edx, 512
        mov eax, 4
        int 0x80
        mov ecx, oam
        mov edx, 544
        mov eax, 4
        int 0x80
        mov ecx, aram
        mov edx, 0x10000
        mov eax, 4
        int 0x80
        mov eax, 6
        int 0x80
.x:
        jmp salir_bien
.cinco:
        mov ah, al
        shl al, 3
        shr ah, 2
        or al, ah
        ret

; EBX = nombre -> EAX = archivo nuevo, para escribir
crea:
        mov ecx, 0x241
        mov edx, 0o644
        mov eax, 5
        int 0x80
        ret
; ESI = texto con un número decimal -> EAX
num:
        xor eax, eax
.n:
        movzx edx, byte [esi]
        sub edx, '0'
        cmp edx, 9
        ja .x
        imul eax, 10
        add eax, edx
        inc esi
        jmp .n
.x:
        ret
ppm         db "P6", 10, "256 224", 10, "255", 10
ppm_n       equ $ - ppm


t_nl        db 10, 0
t_mal       db "MAL ", 0
t_malram    db "MAL ram ", 0
t_ciclos    db "CICLOS ", 0
n_pc        db "pc ", 0
n_s         db "s ", 0
n_p         db "p ", 0
n_a         db "a ", 0
n_x         db "x ", 0
n_y         db "y ", 0
n_dbr       db "dbr ", 0
n_d         db "d ", 0
n_pbr       db "pbr ", 0
n_e         db "e ", 0
        align 4
nombres     dd n_pc, n_s, n_p, n_a, n_x, n_y, n_dbr, n_d, n_pbr, n_e
n_sp        db "sp ", 0
n_psw       db "psw ", 0
nombres_s   dd n_pc, n_a, n_x, n_y, n_sp, n_psw

%include "cpu.inc"
%include "memoria.inc"
%include "spc.inc"
%include "dsp.inc"
%include "ppu.inc"
%include "bus.inc"
%include "mapas.inc"
fin_archivo:
%include "datos.inc"
            alignb 4
ini_regs    resd 10
fin_regs    resd 10
p_fin       resd 1
casos_fin   resd 1
n_caso      resd 1
ciclos      resd 1
opcode      resd 1
hay_mal     resb 1
txt_num     resb 16
ncuadros    resd 1
nguion      resd 1
fd_son      resd 1
guion       resw 0x40000
rgb_buf     resb 256 * 224 * 3
            alignb 16
buf_casos:
rom_buf     resb 0x1000000
mem_plana   resb 0x1000000 + 16
fin_memoria:
