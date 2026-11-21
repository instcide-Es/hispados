; =============================================================================
;  Hispa-Drive - el sonido FM: el YM2612 (seis canales de cuatro operadores)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Escrito desde la documentación pública del chip (sus registros, los ocho
;  algoritmos, las envolventes con sus velocidades, la desafinación, el LFO,
;  el DAC del canal 6, el modo especial del canal 3 y los temporizadores):
;    - cada operador: una fase de 32 bits (los 10 de arriba, el seno) que
;      avanza según la nota (F-number y bloque), el múltiplo y la
;      desafinación; su salida es el seno de la fase (más la del operador
;      que lo modula) por la ganancia de su atenuación: la envolvente (10
;      bits, de 0,09375 dB), el nivel total y el LFO (si toca);
;    - la envolvente sube, baja hasta el nivel de sostén, sigue bajando y
;      se suelta, cada fase con su velocidad (y la escala por la nota),
;      como el chip: un paso cada tres muestras suyas (53.267 por segundo),
;      con las tablas de la documentación (tools/genym.py);
;    - las muestras salen directamente a la frecuencia de la tarjeta (no a
;      la del chip), y se suman a las del SN76489.
;  Sin hacer: el SSG-EG (la envolvente que se repite) y el modo CSM.
;
;    ym_reiniciar    todo callado
;    ym_frecuencia   [psg_hz] -> los pasos a esa frecuencia
;    ym_escribe      EDX=puerto (0-3) AL=octeto
;    ym_linea        una línea: los temporizadores y las muestras (ECX=la
;                    primera en psg_buf, EAX=cuántas)
; =============================================================================

OP_FASE     equ 0
OP_INC      equ 4                   ; lo que avanza por muestra (con el LFO)
OP_ENV      equ 8                   ; la envolvente (0-1023, 0 = a tope)
OP_EST      equ 12                  ; 0 sube, 1 baja, 2 sostén, 3 suelta, 4 callado
OP_TL       equ 16                  ; el nivel total (en pasos: TL x 8)
OP_SL       equ 20                  ; el nivel de sostén
OP_AR       equ 24                  ; las cuatro velocidades (en el orden de
OP_D1R      equ 28                  ;   OP_EST): 5 bits
OP_D2R      equ 32
OP_RR       equ 36                  ;   (la de soltar, ya a 5 bits: 2 RR + 1)
OP_KS       equ 40                  ; la escala por la nota (0-3)
OP_MUL      equ 44
OP_DT       equ 48
OP_AM       equ 52                  ; FFFFFFFFh si le toca el LFO de volumen
OP_KSR      equ 56                  ; lo que suma la nota a las velocidades
OP_INCB     equ 60                  ; lo que avanza sin el LFO
OP_TAM      equ 64
CH_FB       equ 256                 ; la realimentación del operador 1 (0-7)
CH_ALG      equ 260
CH_O1A      equ 264                 ; las dos últimas salidas del operador 1
CH_O1B      equ 268
CH_FNUM     equ 272
CH_BLK      equ 276
CH_KC       equ 280                 ; el código de la nota (5 bits)
CH_LR       equ 284                 ; los altavoces (0: no se oye)
CH_AMS      equ 288                 ; el LFO de volumen: desplazamiento (8 = nada)
CH_PMS      equ 292                 ; y el de tono (0-7)
CH_LATCH    equ 296                 ; lo escrito en A4h, a la espera de A0h
CH_PMK      equ 300                 ; el LFO de tono de ahora (x 65536)
CH_TAM      equ 320
YM_FM_HZ    equ 53267               ; las muestras por segundo del chip

; el orden de los operadores en los registros: +0 el 1, +4 el 3, +8 el 2,
; +12 el 4
ym_slot     db 0, 2, 1, 3
; el LFO: frecuencias (en centésimas de Hz) y el de tono, en 1/65536 a tope
ym_lfo_hz   dd 398, 556, 602, 637, 688, 963, 4810, 7220
ym_pm_tab   dd 0, 129, 254, 379, 531, 758, 1516, 3032
ym_ams_sh   db 8, 3, 1, 0

ym_reiniciar:
        pushad
        mov edi, ym_ini
        mov ecx, ym_fin - ym_ini
        xor eax, eax
        rep stosb
        mov ebx, ym_ch              ; los 24 operadores, callados
        mov ecx, 6
.c:
        xor edx, edx
.o:
        mov dword [ebx+edx+OP_EST], 4
        mov dword [ebx+edx+OP_ENV], 1023
        mov dword [ebx+edx+OP_RR], 1
        add edx, OP_TAM
        cmp edx, 4 * OP_TAM
        jb .o
        mov dword [ebx+CH_LR], 0xC0
        mov dword [ebx+CH_AMS], 8
        add ebx, CH_TAM
        dec ecx
        jnz .c
        mov byte [ym_estado], 0
        call ym_pag_pon
        call ym_frecuencia
        popad
        ret

; [psg_hz] -> los pasos de la fase, de la envolvente y del LFO
ym_frecuencia:
        pushad
        mov ecx, [psg_hz]
        test ecx, ecx
        jz .x
        mov eax, 4096 * YM_FM_HZ    ; la fase: (4096 x 53267 / Hz) en 16.16
        mov edx, eax
        shr edx, 16
        shl eax, 16
        div ecx
        mov [ym_k], eax
        mov eax, YM_FM_HZ * 65536 / 4 ; la envolvente: 53267/3 por segundo
        xor edx, edx
        shl eax, 2                  ; (YM_FM_HZ x 65536 no cabe en la
        adc edx, 0                  ; constante: se multiplica aquí)
        lea ecx, [ecx+ecx*2]
        div ecx
        mov [ym_egpaso], eax
        mov eax, [psg_hz]           ; el LFO: frecuencia x 2^32 / (Hz x 100)
        imul eax, 100
        mov [ym_tmp], eax
        xor ebx, ebx
.l:
        mov edx, [ym_lfo_hz+ebx*4]
        xor eax, eax
        div dword [ym_tmp]
        mov [ym_lfo_inc+ebx*4], eax
        inc ebx
        cmp ebx, 8
        jb .l
        xor ebx, ebx                ; las notas de los seis canales, otra vez
.c:
        call ym_nota
        inc ebx
        cmp ebx, 6
        jb .c
.x:
        popad
        ret

; EDX=puerto (0-3) AL=octeto (conserva todo menos EBP)
ym_escribe:
        push eax
        push ebx
        test edx, 1
        jnz .dato
        mov ebx, edx                ; la dirección del registro (y de qué parte)
        shr ebx, 1
        mov [ym_parte], bl
        mov [ym_dir], al
        jmp .x
.dato:
        movzx ebx, byte [ym_dir]
        cmp byte [ym_parte], 0
        je .p0
        or ebx, 0x100
.p0:
        mov [ym_regs+ebx], al
        cmp ebx, 0x30
        jb .global
        cmp ebx, 0x100
        jb .canal
        cmp ebx, 0x130
        jb .x
.canal:
        pushad
        call ym_reg_canal
        popad
        jmp .x
.global:
        cmp ebx, 0x24
        je .ta
        cmp ebx, 0x25
        je .ta
        cmp ebx, 0x26
        je .tb
        cmp ebx, 0x27
        je .t27
        cmp ebx, 0x28
        je .tecla
        cmp ebx, 0x2A
        je .dac
        cmp ebx, 0x2B
        je .dacon
        cmp ebx, 0x22
        je .lfo
        jmp .x
.lfo:
        mov [ym_lfo], al
        jmp .x
.dac:
        movzx eax, al
        sub eax, 128
        shl eax, 6
        mov [ym_dac], eax
        jmp .x
.dacon:
        shr al, 7
        mov [ym_dacon], al
        jmp .x
.tecla:
        pushad
        call ym_tecla
        popad
        jmp .x
.ta:
        movzx eax, byte [ym_regs+0x24]
        shl eax, 2
        mov bl, [ym_regs+0x25]
        and bl, 3
        or al, bl
        mov [ym_ta], eax
        jmp .x
.tb:
        movzx eax, byte [ym_regs+0x26]
        mov [ym_tb], eax
        jmp .x
.t27:
        ; bits 0 y 1: en marcha; 2 y 3: avisan; 4 y 5: quitan el aviso;
        ; 6 y 7: el modo del canal 3
        test al, 1
        jz .a0
        test byte [ym_t27], 1
        jnz .a0
        mov ebx, 1024
        sub ebx, [ym_ta]
        shl ebx, 16
        mov [ym_cta], ebx           ; (se carga al ponerlo en marcha)
.a0:
        test al, 2
        jz .a1
        test byte [ym_t27], 2
        jnz .a1
        mov ebx, 256
        sub ebx, [ym_tb]
        shl ebx, 20
        mov [ym_ctb], ebx
.a1:
        mov ah, [ym_t27]
        mov [ym_t27], al
        xor ah, al
        test ah, 0xC0
        jz .a2
        pushad
        mov ebx, 2                  ; (cambia el modo: las notas del canal 3)
        call ym_nota
        popad
.a2:
        mov ah, [ym_estado]
        test al, 0x10
        jz .r1
        and ah, ~1
.r1:
        test al, 0x20
        jz .r2
        and ah, ~2
.r2:
        cmp ah, [ym_estado]
        je .x
        mov [ym_estado], ah
        call ym_pag_pon
.x:
        pop ebx
        pop eax
        ret

; (28h) AL: canal (bits 0-2: 0-2 y 4-6) y operadores (bits 4-7)
ym_tecla:
        movzx ebx, al
        and ebx, 7
        cmp ebx, 3
        je .x
        cmp ebx, 7
        je .x
        cmp ebx, 4
        jb .c
        dec ebx
.c:
        imul ebx, CH_TAM
        lea esi, [ym_ch+ebx]
        mov cl, al
        shr cl, 4
        xor edx, edx                ; los operadores 1-4 (en el orden de la memoria)
.o:
        shr cl, 1
        jnc .suelta
        cmp dword [esi+OP_EST], 3   ; pulsar: si estaba suelto, empieza
        jb .s
        mov dword [esi+OP_FASE], 0
        mov dword [esi+OP_EST], 0
        mov eax, [esi+OP_AR]        ; (subida instantánea)
        lea eax, [eax*2]
        add eax, [esi+OP_KSR]
        cmp eax, 62
        jb .s
        mov dword [esi+OP_ENV], 0
        mov dword [esi+OP_EST], 1
        jmp .s
.suelta:
        cmp dword [esi+OP_EST], 3
        jae .s
        mov dword [esi+OP_EST], 3
.s:
        add esi, OP_TAM
        inc edx
        cmp edx, 4
        jb .o
.x:
        ret

; EBX=registro (con la parte en el bit 8), AL=valor: los de los canales
ym_reg_canal:
        mov ecx, ebx
        and ecx, 3
        cmp ecx, 3
        je near .x
        test ebx, 0x100
        jz .p
        add ecx, 3
.p:
        mov edi, ecx                ; EDI = canal
        imul ecx, CH_TAM
        lea esi, [ym_ch+ecx]        ; ESI = el canal
        movzx edx, bl
        cmp edx, 0xA0
        jae near .frec
        ; un operador: (registro >> 2) & 3 -> 1, 3, 2, 4
        mov ecx, edx
        shr ecx, 2
        and ecx, 3
        movzx ecx, byte [ym_slot+ecx]
        shl ecx, 6
        add ecx, esi                ; ECX = el operador
        and edx, 0xF0
        movzx eax, al
        cmp edx, 0x30
        jne .r40
        mov edx, eax                ; DT y MUL
        shr edx, 4
        and edx, 7
        mov [ecx+OP_DT], edx
        and eax, 15
        mov [ecx+OP_MUL], eax
        mov ebx, edi
        jmp ym_nota
.r40:
        cmp edx, 0x40
        jne .r50
        and eax, 0x7F
        shl eax, 3
        mov [ecx+OP_TL], eax
        ret
.r50:
        cmp edx, 0x50
        jne .r60
        mov edx, eax
        shr edx, 6
        mov [ecx+OP_KS], edx
        and eax, 0x1F
        mov [ecx+OP_AR], eax
        mov ebx, edi
        jmp ym_nota                 ; (la escala por la nota)
.r60:
        cmp edx, 0x60
        jne .r70
        xor edx, edx
        test al, 0x80
        jz .am
        dec edx
.am:
        mov [ecx+OP_AM], edx
        and eax, 0x1F
        mov [ecx+OP_D1R], eax
        ret
.r70:
        cmp edx, 0x70
        jne .r80
        and eax, 0x1F
        mov [ecx+OP_D2R], eax
        ret
.r80:
        cmp edx, 0x80
        jne .x
        mov edx, eax
        and edx, 15
        lea edx, [edx*2+1]
        mov [ecx+OP_RR], edx
        shr eax, 4
        shl eax, 5                  ; SL x 32 (el 15: abajo del todo)
        cmp eax, 15 * 32
        jne .sl
        mov eax, 1023
.sl:
        mov [ecx+OP_SL], eax
        ret
.frec:
        movzx eax, al
        mov ecx, edx
        and ecx, 0xFC
        cmp ecx, 0xA4
        jne .a0
        mov [esi+CH_LATCH], eax     ; A4h-A6h: bloque y F alta, para luego
        ret
.a0:
        cmp ecx, 0xA0
        jne .a8
        mov ecx, [esi+CH_LATCH]     ; A0h-A2h: la nota entera
        mov edx, ecx
        and ecx, 7
        shl ecx, 8
        or ecx, eax
        mov [esi+CH_FNUM], ecx
        shr edx, 3
        and edx, 7
        mov [esi+CH_BLK], edx
        mov ebx, edi
        jmp ym_nota
.a8:
        cmp ecx, 0xAC               ; el canal 3 en modo especial (solo la
        jne .a8b                    ; parte 1): AC-AE, para luego
        test ebx, 0x100
        jnz .x
        mov [ym_c3latch], eax
        ret
.a8b:
        cmp ecx, 0xA8               ; (aquí, EDI = el operador: 0 A8h...)
        jne .b0
        test ebx, 0x100
        jnz .x
        mov ecx, [ym_c3latch]       ; A8h (op. 3), A9h (op. 1), AAh (op. 2)
        mov edx, ecx
        and ecx, 7
        shl ecx, 8
        or ecx, eax
        shr edx, 3
        and edx, 7
        shl edx, 11
        or ecx, edx
        mov [ym_c3f+edi*4], ecx
        mov ebx, 2
        jmp ym_nota
.b0:
        cmp ecx, 0xB0
        jne .b4
        mov edx, eax
        and eax, 7
        mov [esi+CH_ALG], eax
        shr edx, 3
        and edx, 7
        mov [esi+CH_FB], edx
        ret
.b4:
        cmp ecx, 0xB4
        jne .x
        mov edx, eax
        and edx, 0xC0
        mov [esi+CH_LR], edx
        mov edx, eax
        shr edx, 4
        and edx, 3
        movzx edx, byte [ym_ams_sh+edx]
        mov [esi+CH_AMS], edx
        and eax, 7
        mov [esi+CH_PMS], eax
.x:
        ret

; EBX=canal (0-5): lo que avanzan sus operadores y la escala de las
; envolventes, según la nota (y el modo especial del canal 3)
ym_nota:
        pushad
        mov esi, ebx
        imul esi, CH_TAM
        add esi, ym_ch
        mov [ym_tmpch], esi
        xor edi, edi                ; EDI = operador (0-3, el 1 al 4)
.o:
        mov eax, [ym_tmpch]
        mov ecx, [eax+CH_FNUM]
        mov edx, [eax+CH_BLK]
        cmp ebx, 2                  ; ¿el canal 3, en modo especial?
        jne .n
        test byte [ym_t27], 0xC0
        jz .n
        cmp edi, 3
        je .n
        movzx eax, byte [.c3+edi]
        mov ecx, [ym_c3f+eax*4]
        mov edx, ecx
        shr edx, 11
        and ecx, 0x7FF
.n:
        ; el código de la nota: bloque, bit 10 y (10 y (9, 8 o 7)) o (no 10 y 9, 8 y 7)
        mov eax, edx
        shl eax, 2
        test ecx, 0x400
        jz .k0
        or eax, 2
        test ecx, 0x380
        jz .k1
        or eax, 1
        jmp .k1
.k0:
        mov ebp, ecx
        and ebp, 0x380
        cmp ebp, 0x380
        jne .k1
        or eax, 1
.k1:
        cmp edi, 3                  ; (el del canal: el del operador 4)
        jne .k2
        mov ebp, [ym_tmpch]
        mov [ebp+CH_KC], eax
.k2:
        push eax                    ; la escala de las envolventes
        push ecx
        mov ecx, 3
        sub ecx, [esi+OP_KS]
        mov ebp, eax
        shr ebp, cl
        mov [esi+OP_KSR], ebp
        pop ecx
        pop eax
        ; lo que avanza: ((F << bloque) >> 1) +- DT, x MUL (0: la mitad)
        push edx
        push ecx
        mov ebp, eax                ; EBP = código de la nota
        mov eax, ecx
        mov ecx, edx
        shl eax, cl
        shr eax, 1
        mov ecx, [esi+OP_DT]
        mov edx, ecx
        and edx, 3
        shl edx, 5
        add edx, ebp
        movzx edx, byte [ym_dt+edx]
        test ecx, 4
        jz .dt
        neg edx
.dt:
        add eax, edx
        and eax, 0x1FFFF
        mov ecx, [esi+OP_MUL]
        test ecx, ecx
        jnz .m
        shr eax, 1
        jmp .m1
.m:
        imul eax, ecx
.m1:
        mul dword [ym_k]            ; a la frecuencia de la tarjeta (x 2^12)
        shrd eax, edx, 16
        mov [esi+OP_INCB], eax
        mov [esi+OP_INC], eax
        pop ecx
        pop edx
        add esi, OP_TAM
        inc edi
        cmp edi, 4
        jb .o
        popad
        ret
.c3:    db 1, 2, 0                  ; los operadores 1, 2 y 3: A9h, AAh y A8h

; ---- un paso de las envolventes de los 24 operadores ----
ym_eg:
        pushad
        inc dword [ym_egcnt]
        mov ebx, [ym_egcnt]
        mov esi, ym_ch
        mov edi, 6
.c:
        push esi
        mov ebp, 4
.o:
        mov eax, [esi+OP_EST]
        cmp eax, 4
        jae .s
        mov ecx, [esi+OP_AR+eax*4]  ; la velocidad de la fase en la que va
        test ecx, ecx
        jz .s
        add ecx, ecx
        add ecx, [esi+OP_KSR]
        cmp ecx, 63
        jbe .r
        mov ecx, 63
.r:
        mov edx, ecx                ; EDX = la velocidad (0-63)
        movzx ecx, byte [ym_egsh+edx]
        push eax
        mov eax, 1
        shl eax, cl
        dec eax
        test ebx, eax
        pop eax
        jnz .s
        push ebx
        shr ebx, cl
        and ebx, 7
        movzx ecx, byte [ym_eginc+edx*8+ebx]
        pop ebx
        test ecx, ecx
        jz .s
        test eax, eax
        jnz .baja
        ; subiendo: env += (~env x inc) / 16
        mov edx, [esi+OP_ENV]
        not edx
        imul edx, ecx
        sar edx, 4
        add edx, [esi+OP_ENV]
        jg .a1
        xor edx, edx
        mov dword [esi+OP_EST], 1
.a1:
        mov [esi+OP_ENV], edx
        jmp .s
.baja:
        mov edx, [esi+OP_ENV]
        add edx, ecx
        cmp eax, 1
        jne .b1
        cmp edx, [esi+OP_SL]        ; bajando hasta el sostén
        jb .b1
        mov dword [esi+OP_EST], 2
.b1:
        cmp edx, 1023
        jb .b2
        mov edx, 1023
        cmp eax, 3                  ; (soltado y abajo del todo: callado)
        jne .b2
        mov dword [esi+OP_EST], 4
.b2:
        mov [esi+OP_ENV], edx
.s:
        add esi, OP_TAM
        dec ebp
        jnz .o
        pop esi
        add esi, CH_TAM
        dec edi
        jnz .c
        popad
        ret

; ---- el LFO: cada 32 muestras, el volumen y el tono de ahora ----
ym_lfo_paso:
        pushad
        xor eax, eax
        mov [ym_amv], eax
        test byte [ym_lfo], 8
        jz .pm
        movzx ecx, byte [ym_lfo]
        and ecx, 7
        mov eax, [ym_lfo_inc+ecx*4]
        shl eax, 5
        add [ym_lfo_fase], eax
        mov eax, [ym_lfo_fase]      ; el volumen: un triángulo de 0 a 126
        shr eax, 25
        cmp eax, 64
        jb .t
        xor eax, 127
.t:
        add eax, eax
        mov [ym_amv], eax
.pm:
        mov eax, [ym_lfo_fase]      ; el tono: un triángulo de -64 a 64
        shr eax, 24
        cmp eax, 128
        jb .t2
        xor eax, 255
.t2:
        sub eax, 64
        test byte [ym_lfo], 8
        jnz .t3
        xor eax, eax
.t3:
        mov ebx, eax                ; EBX = el triángulo
        mov esi, ym_ch
        mov edi, 6
.c:
        mov eax, [esi+CH_PMS]
        mov eax, [ym_pm_tab+eax*4]
        imul eax, ebx
        sar eax, 6
        cmp eax, [esi+CH_PMK]
        je .n
        mov [esi+CH_PMK], eax
        mov ecx, eax
        xor ebp, ebp
.o:
        mov eax, [esi+ebp+OP_INCB]  ; OP_INC = OP_INCB x (1 + PMK/65536)
        imul ecx
        shrd eax, edx, 16
        add eax, [esi+ebp+OP_INCB]
        mov [esi+ebp+OP_INC], eax
        add ebp, OP_TAM
        cmp ebp, 4 * OP_TAM
        jb .o
.n:
        add esi, CH_TAM
        dec edi
        jnz .c
        popad
        ret

; ---- un operador: ESI=él, %1=la modulación (registro, se suma a la fase
; en pasos de 1/1024 de vuelta) -> EAX=su salida (±8191). Gasta EDX ----
%macro YMOP 1
        mov eax, [esi+OP_FASE]
        add eax, [esi+OP_INC]
        mov [esi+OP_FASE], eax
        mov edx, [esi+OP_ENV]
        add edx, [esi+OP_TL]
        push ecx
        mov ecx, [esi+OP_AM]
        and ecx, [ym_amc]
        add edx, ecx
        pop ecx
        cmp edx, 1023
        jae %%cero
        shr eax, 22
        add eax, %1
        and eax, 1023
        movsx eax, word [ym_seno+eax*2]
        movzx edx, word [ym_exp+edx*2]
        imul eax, edx
        sar eax, 16
        jmp short %%x
%%cero:
        xor eax, eax
%%x:
%endmacro

; un canal: EBX=él -> EAX=su salida (±8191)
ym_canal:
        push esi
        push ecx
        push edi
        mov esi, ebx
        ; el LFO de volumen de este canal
        mov eax, [ym_amv]
        mov ecx, [esi+CH_AMS]
        shr eax, cl
        mov [ym_amc], eax
        ; el operador 1, con su realimentación
        xor ecx, ecx
        mov edx, [esi+CH_FB]
        test edx, edx
        jz .fb
        mov ecx, [esi+CH_O1A]
        add ecx, [esi+CH_O1B]
        neg edx
        add edx, 10
        xchg ecx, edx
        sar edx, cl
        mov ecx, edx
.fb:
        YMOP ecx
        mov ecx, [esi+CH_O1A]
        mov [esi+CH_O1B], ecx
        mov [esi+CH_O1A], eax
        mov edi, eax                ; EDI = el operador 1
        sar edi, 1                  ; (la modulación: la mitad)
        mov ecx, [esi+CH_ALG]
        jmp [.alg+ecx*4]
.alg:   dd .a0, .a1, .a2, .a3, .a4, .a5, .a6, .a7
.a0:                                ; 1 -> 2 -> 3 -> 4
        add esi, OP_TAM
        YMOP edi
        sar eax, 1
        mov ecx, eax
        add esi, OP_TAM
        YMOP ecx
        sar eax, 1
        mov ecx, eax
        add esi, OP_TAM
        YMOP ecx
        jmp .fin
.a1:                                ; (1 + 2) -> 3 -> 4
        add esi, OP_TAM
        YMOP 0
        sar eax, 1
        add edi, eax
        add esi, OP_TAM
        YMOP edi
        sar eax, 1
        mov ecx, eax
        add esi, OP_TAM
        YMOP ecx
        jmp .fin
.a2:                                ; (1 + (2 -> 3)) -> 4
        add esi, OP_TAM
        YMOP 0
        sar eax, 1
        mov ecx, eax
        add esi, OP_TAM
        YMOP ecx
        sar eax, 1
        add edi, eax
        add esi, OP_TAM
        YMOP edi
        jmp .fin
.a3:                                ; ((1 -> 2) + 3) -> 4
        add esi, OP_TAM
        YMOP edi
        sar eax, 1
        mov ecx, eax
        add esi, OP_TAM
        YMOP 0
        sar eax, 1
        add ecx, eax
        add esi, OP_TAM
        YMOP ecx
        jmp .fin
.a4:                                ; (1 -> 2) + (3 -> 4)
        add esi, OP_TAM
        YMOP edi
        mov edi, eax
        add esi, OP_TAM
        YMOP 0
        sar eax, 1
        mov ecx, eax
        add esi, OP_TAM
        YMOP ecx
        add eax, edi
        jmp .fin
.a5:                                ; 1 -> 2, 1 -> 3, 1 -> 4
        add esi, OP_TAM
        YMOP edi
        mov ecx, eax
        add esi, OP_TAM
        YMOP edi
        add ecx, eax
        add esi, OP_TAM
        YMOP edi
        add eax, ecx
        jmp .fin
.a6:                                ; (1 -> 2) + 3 + 4
        add esi, OP_TAM
        YMOP edi
        mov ecx, eax
        add esi, OP_TAM
        YMOP 0
        add ecx, eax
        add esi, OP_TAM
        YMOP 0
        add eax, ecx
        jmp .fin
.a7:                                ; 1 + 2 + 3 + 4
        mov ecx, edi
        add ecx, ecx
        add esi, OP_TAM
        YMOP 0
        add ecx, eax
        add esi, OP_TAM
        YMOP 0
        add ecx, eax
        add esi, OP_TAM
        YMOP 0
        add eax, ecx
.fin:
        cmp eax, 8191               ; (el canal no pasa de 14 bits)
        jle .f1
        mov eax, 8191
.f1:
        cmp eax, -8192
        jge .f2
        mov eax, -8192
.f2:
        pop edi
        pop ecx
        pop esi
        ret

; ¿suena algo en el canal EBX? (ZF=1: todos sus operadores, callados)
%macro YM_CALLADO 0
        cmp dword [ebx+OP_EST], 4
        jne %%x
        cmp dword [ebx+OP_TAM+OP_EST], 4
        jne %%x
        cmp dword [ebx+2*OP_TAM+OP_EST], 4
        jne %%x
        cmp dword [ebx+3*OP_TAM+OP_EST], 4
%%x:
%endmacro

; una línea: los temporizadores y las muestras. ECX=la primera de esta
; línea en psg_buf, EAX=cuántas
ym_linea:
        pushad
        push ecx
        push eax
        ; el reloj de los temporizadores: 53.267 muestras FM por segundo, unas
        ; 3,39 por línea (en 16.16: 222.351)
        mov edx, 222351
        mov bl, [ym_t27]
        mov bh, [ym_estado]
        test bl, 1
        jz .b
        sub [ym_cta], edx
        jg .b
        mov eax, 1024
        sub eax, [ym_ta]
        shl eax, 16
        add [ym_cta], eax
        test bl, 4
        jz .b
        or bh, 1
.b:
        test bl, 2
        jz .c
        sub [ym_ctb], edx
        jg .c
        mov eax, 256
        sub eax, [ym_tb]
        shl eax, 20                 ; (B cuenta de 16 en 16)
        add [ym_ctb], eax
        test bl, 8
        jz .c
        or bh, 2
.c:
        cmp bh, [ym_estado]
        je .m
        mov [ym_estado], bh
        call ym_pag_pon
.m:
        pop ebp                     ; EBP = cuántas muestras
        pop edi                     ; EDI = la primera
.muestra:
        test ebp, ebp
        jz near .x
        ; las envolventes, a su ritmo
        mov eax, [ym_egpaso]
        add [ym_egacc], eax
.e:
        cmp dword [ym_egacc], 0x10000
        jb .e1
        sub dword [ym_egacc], 0x10000
        call ym_eg
        jmp .e
.e1:
        inc byte [ym_lfo_n]
        test byte [ym_lfo_n], 31
        jnz .l
        call ym_lfo_paso
.l:
        ; los seis canales
        xor ecx, ecx                ; ECX = la suma
        mov ebx, ym_ch
        mov esi, 6
.cn:
        cmp esi, 1                  ; el sexto, con el DAC: lo que diga
        jne .cn1
        cmp byte [ym_dacon], 0
        je .cn1
        cmp dword [ebx+CH_LR], 0
        je .cn9
        add ecx, [ym_dac]
        jmp .cn9
.cn1:
        YM_CALLADO
        je .cn9
        call ym_canal
        cmp dword [ebx+CH_LR], 0
        je .cn9
        add ecx, eax
.cn9:
        add ebx, CH_TAM
        dec esi
        jnz .cn
        ; con el SN76489 (la mitad) y sin pasarse
        mov eax, ecx
        sar eax, 1
        lea eax, [eax+ecx]          ; FM x 1,5 / 2 = x 0,75
        sar eax, 1
        movsx edx, word [psg_buf+edi*2]
        sar edx, 1
        add eax, edx
        cmp eax, 32767
        jle .k1
        mov eax, 32767
.k1:
        cmp eax, -32768
        jge .k2
        mov eax, -32768
.k2:
        mov [psg_buf+edi*2], ax
        inc edi
        dec ebp
        jmp .muestra
.x:
        popad
        ret
