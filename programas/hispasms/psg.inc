; =============================================================================
;  Hispa-System - el sonido: el SN76489 de la consola (tres tonos y un ruido)
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Escrito desde la documentación pública del SN76489 y de la versión que
;  lleva la consola: ocho registros (para cada canal, el periodo de 10 bits o
;  el modo del ruido y la atenuación de 4 bits, en pasos de 2 dB), escritos
;  con un octeto «con registro» (bit 7 a 1) y otro «de datos»; el reloj es el
;  del procesador entre 16; el ruido va en un registro de desplazamiento de
;  16 bits (blanco: bits 0 y 3) a la velocidad que digan sus dos bits (o la
;  del tercer tono). Un periodo de 0 o 1 deja la salida quieta, arriba (así
;  tocan los juegos sonidos grabados, cambiando solo el volumen).
;
;    psg_reiniciar   callado
;    psg_escribe     (el puerto 7Fh) AL = el octeto
;    psg_linea       las muestras que tocan en esta línea, a psg_buf: [psg_hz]
;                    por segundo (las pone quien llama; 0 = sin sonido), 16
;                    bits con signo, un canal; [psg_n] dice cuántas hay
; =============================================================================

PSG_RELOJ   equ 3579545 / 16        ; tics por segundo
LINEAS_SEG  equ 15700               ; líneas por segundo (262 x 59,92)
PSG_MAX     equ 8192

        align 4
; la atenuación (0-15) -> la amplitud de un canal (2 dB por paso)
psg_amp     dw 7600, 6037, 4795, 3809, 3026, 2403, 1909, 1516
            dw 1204, 957, 760, 604, 480, 381, 303, 0

psg_reiniciar:
        pushad
        mov edi, psg_ini
        mov ecx, psg_fin - psg_ini
        xor eax, eax
        rep stosb
        mov dword [psg_vol], 0x0F0F0F0F
        mov dword [psg_lfsr], 0x8000
        mov dword [psg_n], 0
        call psg_frecuencia
        popad
        ret

; [psg_hz] -> [psg_paso]
psg_frecuencia:
        push eax
        push edx
        push ecx
        xor eax, eax
        mov ecx, [psg_hz]
        test ecx, ecx
        jz .x
        mov eax, PSG_RELOJ
        mov edx, eax
        shr edx, 16
        shl eax, 16
        div ecx
.x:
        mov [psg_paso], eax
        pop ecx
        pop edx
        pop eax
        ret

psg_escribe:
        push ebx
        movzx ebx, byte [psg_reg]
        test al, 0x80
        jz .datos
        mov bl, al                  ; con registro: el registro y 4 bits
        shr bl, 4
        and bl, 7
        mov [psg_reg], bl
        test bl, 1
        jnz .vol
        cmp bl, 6
        je .ruido
        shr ebx, 1                  ; el periodo: los 4 bits bajos
        and dword [psg_per+ebx*4], 0x3F0
        push eax
        and eax, 0x0F
        or [psg_per+ebx*4], eax
        pop eax
        pop ebx
        ret
.datos:
        test bl, 1
        jnz .vol
        cmp bl, 6
        je .ruido
        shr ebx, 1                  ; el periodo: los 6 bits altos
        and dword [psg_per+ebx*4], 0x00F
        push eax
        and eax, 0x3F
        shl eax, 4
        or [psg_per+ebx*4], eax
        pop eax
        pop ebx
        ret
.vol:
        shr ebx, 1
        push eax
        and al, 0x0F
        mov [psg_vol+ebx], al
        pop eax
        pop ebx
        ret
.ruido:
        push eax
        and al, 7
        mov [psg_ruido], al
        mov dword [psg_lfsr], 0x8000
        pop eax
        pop ebx
        ret

; las muestras de una línea
psg_linea:
        pushad
        mov eax, [psg_hz]
        test eax, eax
        jz near .x
        add [psg_acc], eax
.m:
        cmp dword [psg_acc], LINEAS_SEG
        jb near .x
        sub dword [psg_acc], LINEAS_SEG
        xor edi, edi                ; la suma
        xor ebx, ebx
.c:                                 ; los tres tonos
        mov eax, [psg_per+ebx*4]
        cmp eax, 1
        jbe .alto
        shl eax, 16
        cmp eax, [psg_paso]         ; más rápido que las muestras: a medias
        jb .medio
        mov edx, [psg_cnt+ebx*4]
        sub edx, [psg_paso]
.c1:
        jg .c2
        add edx, eax
        xor byte [psg_sal+ebx*4], 1
        test edx, edx
        jmp .c1
.c2:
        mov [psg_cnt+ebx*4], edx
        movzx ecx, byte [psg_vol+ebx]
        movzx ecx, word [psg_amp+ecx*2]
        cmp byte [psg_sal+ebx*4], 0
        jne .c3
        neg ecx
.c3:
        add edi, ecx
        jmp .c4
.alto:
        movzx ecx, byte [psg_vol+ebx]
        movzx ecx, word [psg_amp+ecx*2]
        add edi, ecx
        jmp .c4
.medio:
.c4:
        inc ebx
        cmp ebx, 3
        jb .c
        ; el ruido
        movzx eax, byte [psg_ruido]
        and eax, 3
        cmp eax, 3
        je .r3
        mov ecx, 0x20               ; (el registro avanza una vez por cada dos
        shl ecx, cl                 ; vueltas del contador)
        jmp .r4
.r3:
        mov ecx, [psg_per+2*4]
        shl ecx, 1
        test ecx, ecx
        jnz .r4
        mov ecx, 2
.r4:
        shl ecx, 16
        mov edx, [psg_cnt+3*4]
        sub edx, [psg_paso]
.r5:
        jg .r6
        add edx, ecx
        mov eax, [psg_lfsr]         ; un paso del registro
        mov ebp, eax
        test byte [psg_ruido], 4
        jz .per
        mov ebp, eax                ; blanco: bit 0 xor bit 3
        shr ebp, 3
        xor ebp, eax
.per:
        and ebp, 1
        shr eax, 1
        shl ebp, 15
        or eax, ebp
        mov [psg_lfsr], eax
        test edx, edx
        jmp .r5
.r6:
        mov [psg_cnt+3*4], edx
        movzx ecx, byte [psg_vol+3]
        movzx ecx, word [psg_amp+ecx*2]
        test byte [psg_lfsr], 1
        jnz .r7
        neg ecx
.r7:
        add edi, ecx
        mov eax, [psg_n]            ; la muestra
        cmp eax, PSG_MAX
        jae near .m
        mov [psg_buf+eax*2], di
        inc dword [psg_n]
        jmp .m
.x:
        popad
        ret
