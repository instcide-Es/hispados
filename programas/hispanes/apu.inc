; =============================================================================
;  Hispa-Nes - el sonido: dos canales de pulsos, triángulo, ruido y muestras
;  Copyright (C) 2026 Hispa-DOS contributors. GPL-2.0-or-later.
;
;  Escrito desde la documentación pública del hardware: registros 4000h a
;  4017h, el secuenciador de cuadros (envolventes, barridos y contadores de
;  duración, con su interrupción), las tablas de duraciones y de periodos y
;  la mezcla aproximada por pesos.
;
;  El secuenciador avanza en su ciclo exacto (apu_paso: el procesador se para
;  justo ahí, ver corre_tramo en ppu.inc). Las muestras las saca apu_linea,
;  una vez por línea de imagen (113,67 ciclos del procesador), a 22.050 Hz,
;  de 8 bits sin signo, a apu_buf (apu_n dice cuántas hay; quien las recoge
;  lo pone a 0). Cada canal avanza por muestra con su propia
;  cuenta de ciclos, sin filtrar (suena algo áspero en los agudos).
;
;    apu_reiniciar   todo callado, como al encender
;    apu_escribir    AL -> registro EDX (4000h-4013h, 4015h, 4017h)
;    apu_estado      -> AL = registro 4015h (y baja la interrupción de cuadro)
;    apu_paso        un paso del secuenciador
;    apu_linea       el tiempo de una línea: sus muestras
; =============================================================================

APU_HZ      equ 22050
APU_MAX     equ 2048                ; muestras que caben en apu_buf
; un canal (los tres tipos usan la misma ficha; cada uno, lo que necesita)
C_DUTY      equ 0                   ; pulsos: ciclo. Ruido: modo corto
C_HALT      equ 4                   ; no cuenta la duración / envolvente en bucle
C_CONST     equ 8                   ; volumen fijo
C_VOL       equ 12                  ; volumen o periodo de la envolvente
C_SW_EN     equ 16                  ; barrido: activo, periodo, hacia abajo,
C_SW_PER    equ 20                  ;   desplazamiento, recargar y divisor
C_SW_NEG    equ 24
C_SW_SH     equ 28
C_SW_REL    equ 32
C_SW_DIV    equ 36
C_TIMER     equ 40                  ; periodo (ruido: ya en ciclos)
C_LEN       equ 44                  ; contador de duración
C_ENV_INI   equ 48                  ; envolvente: empezar, divisor y nivel
C_ENV_DIV   equ 52                  ;   (triángulo: recargar el lineal,
C_ENV_NIV   equ 56                  ;   - y contador lineal)
C_FASE      equ 60                  ; ciclos (x256) acumulados
C_SEC       equ 64                  ; paso de la secuencia / registro del ruido
C_ACT       equ 68                  ; canal encendido (4015h)
C_UNO       equ 72                  ; 1 en el primer canal de pulsos
C_TAM       equ 76

CIC_MUESTRA equ 20779               ; ciclos por muestra x256 (1789773/22050)
PTS_MUESTRA2 equ 487                ; puntos de imagen por muestra, x2
PTS_PASO    equ 22371               ; puntos por paso del secuenciador (7.457 ciclos: 240 por segundo)

        align 4
apu_dur     db 10, 254, 20, 2, 40, 4, 80, 6, 160, 8, 60, 10, 14, 12, 26, 14
            db 12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
apu_ciclo   db 0, 1, 0, 0, 0, 0, 0, 0,  0, 1, 1, 0, 0, 0, 0, 0
            db 0, 1, 1, 1, 1, 0, 0, 0,  1, 0, 0, 1, 1, 1, 1, 1
apu_tri     db 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0
            db 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
apu_ruido   dw 4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068
apu_dmc     dw 428, 380, 340, 320, 286, 254, 226, 214, 190, 160, 142, 128, 106, 84, 72, 54

apu_reiniciar:
        pushad
        mov edi, apu_ini
        mov ecx, apu_fin - apu_ini
        xor al, al
        rep stosb
        mov dword [can_p1+C_UNO], 1
        mov dword [can_ru+C_SEC], 1
        mov dword [can_ru+C_TIMER], 4
        mov dword [dmc_periodo], 428
        and byte [irq_linea], ~3
        popad
        ret

; ---- 4015h: qué canales siguen sonando y las interrupciones ----
apu_estado:
        xor eax, eax
        cmp dword [can_p1+C_LEN], 0
        je .a
        or al, 1
.a:
        cmp dword [can_p2+C_LEN], 0
        je .b
        or al, 2
.b:
        cmp dword [can_tr+C_LEN], 0
        je .c
        or al, 4
.c:
        cmp dword [can_ru+C_LEN], 0
        je .d
        or al, 8
.d:
        cmp dword [dmc_quedan], 0
        je .e
        or al, 0x10
.e:
        test byte [irq_linea], 1
        jz .f
        or al, 0x40
.f:
        test byte [irq_linea], 2
        jz .g
        or al, 0x80
.g:
        and byte [irq_linea], ~1
        ret

; ---- escribir en un registro ----
apu_escribir:
        push ebx
        push ecx
        push eax
        movzx ecx, al               ; ECX = el valor
        mov eax, edx
        sub eax, 0x4000
        cmp eax, 0x17
        ja .x
        mov ebx, can_p1
        cmp eax, 4
        jb .canal
        mov ebx, can_p2
        cmp eax, 8
        jb .canal
        mov ebx, can_tr
        cmp eax, 12
        jb .tri
        mov ebx, can_ru
        cmp eax, 16
        jb .ruido
        jmp [.otros+eax*4-16*4]
        align 4
.otros      dd .dmc0, .dmc1, .dmc2, .dmc3, .x, .w4015, .x, .w4017
.canal:
        and eax, 3
        jmp [.tpul+eax*4]
        align 4
.tpul       dd .p0, .p1, .p2, .p3
.p0:
        mov eax, ecx
        shr eax, 6
        mov [ebx+C_DUTY], eax
.env0:
        mov eax, ecx
        shr eax, 5
        and eax, 1
        mov [ebx+C_HALT], eax
        mov eax, ecx
        shr eax, 4
        and eax, 1
        mov [ebx+C_CONST], eax
        and ecx, 15
        mov [ebx+C_VOL], ecx
        jmp .x
.p1:
        mov eax, ecx
        shr eax, 7
        mov [ebx+C_SW_EN], eax
        mov eax, ecx
        shr eax, 4
        and eax, 7
        mov [ebx+C_SW_PER], eax
        mov eax, ecx
        shr eax, 3
        and eax, 1
        mov [ebx+C_SW_NEG], eax
        and ecx, 7
        mov [ebx+C_SW_SH], ecx
        mov dword [ebx+C_SW_REL], 1
        jmp .x
.p2:
        mov [ebx+C_TIMER], cl
        jmp .x
.p3:
        mov dword [ebx+C_SEC], 0
.dur:
        mov eax, ecx
        and eax, 7
        mov [ebx+C_TIMER+1], al
        mov dword [ebx+C_ENV_INI], 1
        cmp dword [ebx+C_ACT], 0
        je .x
        shr ecx, 3
        movzx eax, byte [apu_dur+ecx]
        mov [ebx+C_LEN], eax
        jmp .x
.tri:
        and eax, 3
        jz .t0
        cmp eax, 2
        je .p2
        jb .x
        jmp .dur                    ; 400Bh: periodo alto, duración y recarga
.t0:
        mov eax, ecx
        shr eax, 7
        mov [ebx+C_HALT], eax
        and ecx, 0x7F
        mov [ebx+C_VOL], ecx
        jmp .x
.ruido:
        and eax, 3
        jz .env0
        cmp eax, 2
        jb .x
        ja .r3
        mov eax, ecx
        shr eax, 7
        mov [ebx+C_DUTY], eax
        and ecx, 15
        movzx eax, word [apu_ruido+ecx*2]
        mov [ebx+C_TIMER], eax
        jmp .x
.r3:
        mov dword [ebx+C_ENV_INI], 1
        cmp dword [ebx+C_ACT], 0
        je .x
        shr ecx, 3
        movzx eax, byte [apu_dur+ecx]
        mov [ebx+C_LEN], eax
        jmp .x
.dmc0:
        mov [dmc_ctrl], cl
        and ecx, 15
        movzx eax, word [apu_dmc+ecx*2]
        mov [dmc_periodo], eax
        test byte [dmc_ctrl], 0x80
        jnz .x
        and byte [irq_linea], ~2
        jmp .x
.dmc1:
        and ecx, 0x7F
        mov [dmc_nivel], ecx
        jmp .x
.dmc2:
        shl ecx, 6
        add ecx, 0xC000
        mov [dmc_dir0], ecx
        jmp .x
.dmc3:
        shl ecx, 4
        inc ecx
        mov [dmc_largo0], ecx
        jmp .x
.w4015:
        and byte [irq_linea], ~2
        mov ebx, can_p1
        mov eax, 4
.en:
        shr ecx, 1
        setc byte [ebx+C_ACT]
        jc .en1
        mov dword [ebx+C_LEN], 0
.en1:
        add ebx, C_TAM
        dec eax
        jnz .en
        shr ecx, 1
        jc .den
        mov dword [dmc_quedan], 0
        jmp .x
.den:
        cmp dword [dmc_quedan], 0
        jne .x
        mov eax, [dmc_dir0]
        mov [dmc_dir], eax
        mov eax, [dmc_largo0]
        mov [dmc_quedan], eax
        call dmc_carga
        jmp .x
.w4017:
        mov [fc_modo], cl
        test cl, 0x40
        jz .f1
        and byte [irq_linea], ~1
.f1:
        mov eax, PTS_PASO           ; (corre_tramo ya había contado este tramo
        cmp [fc_acc], eax           ; entero; si acaba en un paso, ese paso ya
        je .f2                      ; no toca)
        xor eax, eax
.f2:
        add eax, edi                ; ...y ahora se cuenta desde aquí: al
        sub eax, 18                 ; acabar el tramo habrá pasado lo que queda
        mov [fc_acc], eax           ; (menos lo que tarda en hacer efecto: la
                                    ; escritura es el último ciclo de la
                                    ; instrucción y el contador arranca 3 después)
        mov dword [fc_paso], 0
        test cl, 0x80
        jz .x
        call apu_cuarto
        call apu_medio
.x:
        pop eax
        pop ecx
        pop ebx
        ret

; ---- el secuenciador: envolventes y contador lineal (cuatro veces por
;      cuadro) y duraciones y barridos (dos) ----
apu_cuarto:
        push eax
        push ebx
        mov ebx, can_p1
        call .env
        mov ebx, can_p2
        call .env
        mov ebx, can_ru
        call .env
        mov ebx, can_tr             ; el contador lineal del triángulo
        cmp dword [ebx+C_ENV_INI], 0
        je .l1
        mov eax, [ebx+C_VOL]
        mov [ebx+C_ENV_NIV], eax
        jmp .l2
.l1:
        cmp dword [ebx+C_ENV_NIV], 0
        je .l2
        dec dword [ebx+C_ENV_NIV]
.l2:
        cmp dword [ebx+C_HALT], 0
        jne .l3
        mov dword [ebx+C_ENV_INI], 0
.l3:
        pop ebx
        pop eax
        ret
.env:
        cmp dword [ebx+C_ENV_INI], 0
        je .e1
        mov dword [ebx+C_ENV_INI], 0
        mov dword [ebx+C_ENV_NIV], 15
        jmp .e3
.e1:
        cmp dword [ebx+C_ENV_DIV], 0
        je .e2
        dec dword [ebx+C_ENV_DIV]
        ret
.e2:
        cmp dword [ebx+C_ENV_NIV], 0
        je .e4
        dec dword [ebx+C_ENV_NIV]
        jmp .e3
.e4:
        cmp dword [ebx+C_HALT], 0
        je .e3
        mov dword [ebx+C_ENV_NIV], 15
.e3:
        mov eax, [ebx+C_VOL]
        mov [ebx+C_ENV_DIV], eax
        ret

apu_medio:
        push eax
        push ebx
        push ecx
        push edx
        mov ebx, can_p1
        mov edx, 4
.d:
        cmp dword [ebx+C_HALT], 0
        jne .d1
        cmp dword [ebx+C_LEN], 0
        je .d1
        dec dword [ebx+C_LEN]
.d1:
        add ebx, C_TAM
        dec edx
        jnz .d
        mov ebx, can_p1
        call .barrido
        mov ebx, can_p2
        call .barrido
        pop edx
        pop ecx
        pop ebx
        pop eax
        ret
.barrido:
        cmp dword [ebx+C_SW_DIV], 0
        jne .b2
        cmp dword [ebx+C_SW_EN], 0
        je .b2
        mov ecx, [ebx+C_SW_SH]
        test ecx, ecx
        jz .b2
        mov eax, [ebx+C_TIMER]
        cmp eax, 8
        jb .b2
        mov edx, eax
        shr edx, cl
        cmp dword [ebx+C_SW_NEG], 0
        je .sube
        sub eax, edx
        sub eax, [ebx+C_UNO]
        jns .pon
        xor eax, eax
        jmp .pon
.sube:
        add eax, edx
        cmp eax, 0x7FF
        ja .b2
.pon:
        mov [ebx+C_TIMER], eax
.b2:
        cmp dword [ebx+C_SW_DIV], 0
        je .rec
        cmp dword [ebx+C_SW_REL], 0
        jne .rec
        dec dword [ebx+C_SW_DIV]
        ret
.rec:
        mov eax, [ebx+C_SW_PER]
        mov [ebx+C_SW_DIV], eax
        mov dword [ebx+C_SW_REL], 0
        ret

; ---- un paso del secuenciador (lo llama corre_tramo cada 7.457 ciclos) ----
apu_paso:
        push eax
        mov eax, [fc_paso]
        test byte [fc_modo], 0x80
        jnz .cinco
        call apu_cuarto
        test al, 1
        jz .c1
        call apu_medio
.c1:
        cmp al, 3
        jne .c2
        test byte [fc_modo], 0x40
        jnz .c2
        or byte [irq_linea], 1      ; fin de la secuencia: interrupción
.c2:
        inc eax
        and eax, 3
        jmp .sig
.cinco:
        cmp al, 3
        je .s2
        call apu_cuarto
        cmp al, 1
        je .s1
        cmp al, 4
        jne .s2
.s1:
        call apu_medio
.s2:
        inc eax
        cmp eax, 5
        jb .sig
        xor eax, eax
.sig:
        mov [fc_paso], eax
        pop eax
        ret

; ---- el tiempo de una línea de imagen: las muestras que tocan ----
apu_linea:
        pushad
        add dword [mu_acc], PUNTOS_LINEA * 2
.m:
        cmp dword [mu_acc], PTS_MUESTRA2
        jb .x
        sub dword [mu_acc], PTS_MUESTRA2
        call apu_muestra
        jmp .m
.x:
        popad
        ret

; una muestra: avanza cada canal 81 ciclos y mezcla
apu_muestra:
        xor edi, edi                ; EDI = suma de los dos de pulsos
        mov ebx, can_p1
        call .pulso
        mov ebx, can_p2
        call .pulso
        imul edi, 296
        ; el triángulo
        mov ebx, can_tr
        mov eax, [ebx+C_TIMER]
        cmp eax, 2
        jb .tsal                    ; (más agudo de lo que se oye: quieto)
        cmp dword [ebx+C_LEN], 0
        je .tsal
        cmp dword [ebx+C_ENV_NIV], 0
        je .tsal
        inc eax
        shl eax, 8
        mov ecx, [ebx+C_FASE]
        add ecx, CIC_MUESTRA
        mov edx, [ebx+C_SEC]
.t1:
        cmp ecx, eax
        jb .t2
        sub ecx, eax
        inc edx
        jmp .t1
.t2:
        and edx, 31
        mov [ebx+C_SEC], edx
        mov [ebx+C_FASE], ecx
.tsal:
        mov edx, [ebx+C_SEC]
        movzx eax, byte [apu_tri+edx]
        imul eax, 335
        add edi, eax
        ; el ruido
        mov ebx, can_ru
        mov eax, [ebx+C_TIMER]
        shl eax, 8
        mov ecx, [ebx+C_FASE]
        add ecx, CIC_MUESTRA
        mov edx, [ebx+C_SEC]
.r1:
        cmp ecx, eax
        jb .r2
        sub ecx, eax
        mov esi, edx                ; el registro de 15 bits se desplaza
        mov ebp, edx
        shr ebp, 1
        cmp dword [ebx+C_DUTY], 0
        je .r0
        shr ebp, 5
.r0:
        xor esi, ebp
        and esi, 1
        shl esi, 14
        shr edx, 1
        or edx, esi
        jmp .r1
.r2:
        mov [ebx+C_SEC], edx
        mov [ebx+C_FASE], ecx
        test dl, 1
        jnz .rsal
        cmp dword [ebx+C_LEN], 0
        je .rsal
        mov eax, [ebx+C_VOL]
        cmp dword [ebx+C_CONST], 0
        jne .r3
        mov eax, [ebx+C_ENV_NIV]
.r3:
        imul eax, 194
        add edi, eax
.rsal:
        ; las muestras
        cmp byte [dmc_lleno], 0
        jne .dm
        cmp dword [dmc_bits], 0
        je .dsal
.dm:
        mov ecx, [dmc_fase]
        add ecx, CIC_MUESTRA
        mov eax, [dmc_periodo]
        shl eax, 8
.d1:
        cmp ecx, eax
        jb .d2
        sub ecx, eax
        call dmc_bit
        jmp .d1
.d2:
        mov [dmc_fase], ecx
.dsal:
        mov eax, [dmc_nivel]
        imul eax, 132
        add edi, eax
        ; fuera la componente continua y a 8 bits
        mov eax, [mu_cc]
        mov edx, edi
        shl edx, 8
        sub edx, eax
        sar edx, 9
        add eax, edx
        mov [mu_cc], eax
        sar eax, 8
        sub edi, eax
        sar edi, 7
        add edi, 128
        jns .n1
        xor edi, edi
.n1:
        cmp edi, 255
        jbe .n2
        mov edi, 255
.n2:
        mov eax, [apu_n]
        cmp eax, APU_MAX
        jae .lleno
        mov edx, edi
        mov [apu_buf+eax], dl
        inc dword [apu_n]
.lleno:
        ret
; (interna) EBX=canal de pulsos: avanza y suma su salida a EDI
.pulso:
        mov eax, [ebx+C_TIMER]
        cmp eax, 8
        jb .psal
        cmp dword [ebx+C_LEN], 0
        je .psal
        cmp dword [ebx+C_SW_NEG], 0 ; el barrido hacia arriba que se sale lo calla
        jne .pa
        mov ecx, [ebx+C_SW_SH]
        mov edx, eax
        shr edx, cl
        add edx, eax
        cmp edx, 0x7FF
        ja .psal
.pa:
        inc eax
        shl eax, 9
        mov ecx, [ebx+C_FASE]
        add ecx, CIC_MUESTRA
        mov edx, [ebx+C_SEC]
.p1:
        cmp ecx, eax
        jb .p2
        sub ecx, eax
        inc edx
        jmp .p1
.p2:
        and edx, 7
        mov [ebx+C_SEC], edx
        mov [ebx+C_FASE], ecx
        mov eax, [ebx+C_DUTY]
        cmp byte [apu_ciclo+eax*8+edx], 0
        je .psal
        mov eax, [ebx+C_VOL]
        cmp dword [ebx+C_CONST], 0
        jne .p3
        mov eax, [ebx+C_ENV_NIV]
.p3:
        add edi, eax
.psal:
        ret

; un bit del canal de muestras (modulación delta de 1 bit). Los octetos
; pasan por uno de reserva, que se vuelve a llenar en cuanto queda libre
dmc_bit:
        push eax
        cmp dword [dmc_bits], 0
        jne .hay
        cmp byte [dmc_lleno], 0
        je .x
        movzx eax, byte [dmc_reserva]
        mov [dmc_reg], eax
        mov dword [dmc_bits], 8
        mov byte [dmc_lleno], 0
        call dmc_carga
.hay:
        mov eax, [dmc_nivel]
        shr dword [dmc_reg], 1
        jnc .baja
        cmp eax, 125
        ja .f
        add eax, 2
        jmp .f
.baja:
        cmp eax, 2
        jb .f
        sub eax, 2
.f:
        mov [dmc_nivel], eax
        dec dword [dmc_bits]
.x:
        pop eax
        ret

; si el octeto de reserva está libre y quedan, lee otro de la memoria del
; cartucho; con el último vuelve a empezar o avisa con una interrupción
dmc_carga:
        cmp byte [dmc_lleno], 0
        jne .x
        cmp dword [dmc_quedan], 0
        je .x
        push eax
        push edx
        mov edx, [dmc_dir]
        mov eax, edx
        shr eax, 13
        mov eax, [pg+eax*4]
        mov al, [eax+edx]
        mov [dmc_reserva], al
        mov byte [dmc_lleno], 1
        inc edx
        or edx, 0x8000
        and edx, 0xFFFF
        mov [dmc_dir], edx
        dec dword [dmc_quedan]
        jnz .f
        test byte [dmc_ctrl], 0x40
        jz .irq
        mov eax, [dmc_dir0]
        mov [dmc_dir], eax
        mov eax, [dmc_largo0]
        mov [dmc_quedan], eax
        jmp .f
.irq:
        test byte [dmc_ctrl], 0x80
        jz .f
        or byte [irq_linea], 2
.f:
        pop edx
        pop eax
.x:
        ret
