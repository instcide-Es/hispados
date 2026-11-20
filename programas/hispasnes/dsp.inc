; =============================================================================
;  Hispa-Snes - el DSP del sonido: 8 voces de muestras BRR, eco y ruido
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Escrito desde la documentación pública del DSP de la consola: sus 128
;  registros, las muestras comprimidas en BRR (bloques de 9 octetos: 16
;  muestras de 4 bits con su desplazamiento y su filtro), la envolvente (ADSR
;  y GAIN, con su tabla de velocidades), la modulación del tono, el ruido,
;  el eco (con su filtro de 8 coeficientes) y el volumen. Saca 32.000
;  muestras por segundo en estéreo; entre muestra y muestra, una recta (no la
;  curva de la consola).
;
;    dsp_reiniciar
;    dsp_escribe    EDX = registro (00h-7Fh), AL = dato
;    dsp_hasta      EAX = ciclo del SPC700: hace las muestras que tocan hasta
;                   él (una cada 32 ciclos)
;    dsp_cuadro     al acabar un cuadro: las muestras, a la frecuencia que
;                   pide la ventana ([snd_hz_sal]: 0 = ninguna), en snd_sal
;                   (16 bits con signo; estéreo o no según [snd_estereo])
; =============================================================================
VZ_TAM       equ 96                  ; lo de cada voz (dsp_voz + 96*voz)
VZ_DIR       equ 0                   ;   dd el bloque BRR de ahora
VZ_POS       equ 4                   ;   dd la muestra del bloque (0-15)
VZ_CNT       equ 8                   ;   dd lo que lleva el tono (12 bits)
VZ_ENV       equ 12                  ;   dd la envolvente (0-7FFh)
VZ_ECNT      equ 16                  ;   dd la cuenta de la envolvente
VZ_H0        equ 20                  ;   dd la muestra de antes y la de ahora
VZ_H1        equ 24
VZ_P1        equ 28                  ;   dd las dos anteriores del filtro BRR
VZ_P2        equ 32
VZ_SAL       equ 36                  ;   dd lo último que ha sacado
VZ_MODO      equ 40                  ;   db 0 callada, 1 ataque, 2 caída,
VZ_FIN       equ 41                  ;      3 sostenida, 4 suelta
VZ_BUCLE     equ 42                  ;   db el bloque acaba / vuelve
VZ_DEC       equ 48                  ;   dw x 16 las muestras del bloque
MODO_NADA   equ 0
MODO_ATAQUE equ 1
MODO_CAIDA  equ 2
MODO_SOST   equ 3
MODO_SUELTA equ 4
DSP_BUF     equ 1200                ; muestras de un cuadro (con holgura)

dsp_reiniciar:
        pushad
        xor eax, eax
        mov edi, dsp_reg
        mov ecx, 128 / 4
        rep stosd
        mov edi, dsp_voz
        mov ecx, VZ_TAM * 8 / 4
        rep stosd
        mov edi, eco_hist
        mov ecx, 16
        rep stosd
        mov [dsp_t], eax
        mov [dsp_n], eax
        mov [eco_pos], eax
        mov [eco_i], eax
        mov dword [ruido], 0x4000
        mov edi, env_sig                    ; (cuándo le toca a cada velocidad)
        mov ecx, 32 * 2
        rep stosd
        mov edi, env_res
        mov ecx, 32 / 4
        rep stosd
        mov byte [dsp_reg+0x6C], 0xE0   ; FLG: reinicio, silencio, sin eco
        popad
        ret

; EDX = registro, AL = dato
dsp_escribe:
        pushad
        and edx, 0x7F
        mov [dsp_reg+edx], al
        cmp edx, 0x4C
        je .kon
        cmp edx, 0x7C
        jne .x
        mov byte [dsp_reg+0x7C], 0  ; escribir en ENDX lo borra
        jmp .x
.kon:
        movzx ebx, al
        xor ecx, ecx
.k:
        bt ebx, ecx
        jnc .ks
        call voz_empieza
.ks:
        inc ecx
        cmp ecx, 8
        jb .k
.x:
        popad
        ret

; (interna) ECX = la voz: empieza (KON)
voz_empieza:
        pushad
        imul edi, ecx, VZ_TAM
        add edi, dsp_voz
        mov eax, ecx
        shl eax, 4
        movzx edx, byte [dsp_reg+eax+4]     ; SRCN: la muestra
        movzx eax, byte [dsp_reg+0x5D]      ; DIR: la tabla de muestras
        shl eax, 8
        lea eax, [eax+edx*4]
        and eax, 0xFFFF
        movzx eax, word [aram+eax]          ; la dirección del principio
        mov [edi+VZ_DIR], eax
        xor eax, eax
        mov [edi+VZ_POS], eax
        mov [edi+VZ_CNT], eax
        mov [edi+VZ_ENV], eax
        mov [edi+VZ_ECNT], eax
        mov [edi+VZ_H0], eax
        mov [edi+VZ_H1], eax
        mov [edi+VZ_P1], eax
        mov [edi+VZ_P2], eax
        mov byte [edi+VZ_MODO], MODO_ATAQUE
        btr dword [dsp_reg+0x7C], ecx       ; (ENDX)
        call brr_bloque
        popad
        ret

; (interna) EDI = la voz: descodifica el bloque BRR de [VZ_DIR]
brr_bloque:
        pushad
        mov esi, [edi+VZ_DIR]
        and esi, 0xFFFF
        movzx eax, byte [aram+esi]          ; la cabecera
        mov edx, eax
        and dl, 1
        mov [edi+VZ_FIN], dl
        mov edx, eax
        shr dl, 1
        and dl, 1
        mov [edi+VZ_BUCLE], dl
        mov ecx, eax
        shr ecx, 4                          ; CL = el desplazamiento
        shr eax, 2
        and eax, 3
        mov [brr_filtro], eax
        lea ebx, [edi+VZ_DEC]
        mov ebp, 16
        xor edx, edx                        ; (la mitad: alta o baja)
.m:
        mov eax, esi
        inc eax
        push edx
        shr edx, 1
        add eax, edx
        pop edx
        and eax, 0xFFFF
        movzx eax, byte [aram+eax]
        test edx, 1
        jnz .bajo
        shr eax, 4
.bajo:
        and eax, 0x0F
        shl eax, 28                         ; con signo
        sar eax, 28
        cmp cl, 12
        ja .grande
        shl eax, cl
        sar eax, 1
        jmp .filtro
.grande:
        sar eax, 31                         ; (13-15: -2048 o 0)
        and eax, -2048
.filtro:
        push ecx
        push edx
        mov ecx, [edi+VZ_P1]
        mov edx, [edi+VZ_P2]
        cmp dword [brr_filtro], 1
        jb .f9
        je .f1
        cmp dword [brr_filtro], 2
        je .f2
        ; filtro 3: + p1*2 + (-p1*13)>>6 - p2 + (p2*3)>>4
        add eax, ecx
        add eax, ecx
        push ecx
        imul ecx, ecx, -13
        sar ecx, 6
        add eax, ecx
        pop ecx
        sub eax, edx
        lea edx, [edx+edx*2]
        sar edx, 4
        add eax, edx
        jmp .f9
.f1:                                        ; + p1 + (-p1)>>4
        add eax, ecx
        neg ecx
        sar ecx, 4
        add eax, ecx
        jmp .f9
.f2:                                        ; + p1*2 + (-p1*3)>>5 - p2 + p2>>4
        add eax, ecx
        add eax, ecx
        push ecx
        imul ecx, ecx, -3
        sar ecx, 5
        add eax, ecx
        pop ecx
        sub eax, edx
        sar edx, 4
        add eax, edx
.f9:
        pop edx
        pop ecx
        ; a 16 bits, y luego a 15 (como el DSP)
        cmp eax, 32767
        jle .c1
        mov eax, 32767
.c1:
        cmp eax, -32768
        jge .c2
        mov eax, -32768
.c2:
        shl eax, 17
        sar eax, 17
        push dword [edi+VZ_P1]
        pop dword [edi+VZ_P2]
        mov [edi+VZ_P1], eax
        mov [ebx], ax
        add ebx, 2
        inc edx
        dec ebp
        jnz .m
        popad
        ret

; EAX = ciclo del SPC700: las muestras hasta él
dsp_hasta:
        push eax
        push edx
.l:
        mov edx, eax
        sub edx, [dsp_t]
        cmp edx, 32
        jl .x
        add dword [dsp_t], 32
        call dsp_muestra
        jmp .l
.x:
        pop edx
        pop eax
        ret

; (interna) una muestra (estéreo) de las 8 voces
dsp_muestra:
        pushad
        xor eax, eax
        mov [mez_l], eax
        mov [mez_r], eax
        mov [eco_l], eax
        mov [eco_r], eax
        ; el ruido
        movzx eax, byte [dsp_reg+0x6C]
        and eax, 0x1F
        call env_toca
        jnc .r1
        mov eax, [ruido]
        mov edx, eax
        shr edx, 1
        xor edx, eax
        and edx, 1
        shl edx, 14
        shr eax, 1
        or eax, edx
        mov [ruido], eax
.r1:
        inc dword [dsp_cuenta]
        xor ecx, ecx                        ; ECX = la voz
        mov edi, dsp_voz
.voz:
        cmp byte [edi+VZ_MODO], MODO_NADA
        je near .sig
        mov ebx, ecx
        shl ebx, 4                          ; EBX = sus registros
        ; KOFF: a soltar
        movzx eax, byte [dsp_reg+0x5C]
        bt eax, ecx
        jnc .k1
        mov byte [edi+VZ_MODO], MODO_SUELTA
.k1:
        test byte [dsp_reg+0x6C], 0x80      ; FLG: el reinicio, todas fuera
        jz .k2
        mov byte [edi+VZ_MODO], MODO_SUELTA
        mov dword [edi+VZ_ENV], 0
.k2:
        ; la muestra: el ruido o la recta entre la de antes y la de ahora
        movzx eax, byte [dsp_reg+0x3D]      ; NON
        bt eax, ecx
        jnc .int
        mov eax, [ruido]
        shl eax, 17
        sar eax, 16
        sar eax, 1
        jmp .env
.int:
        mov eax, [edi+VZ_H1]
        sub eax, [edi+VZ_H0]
        imul eax, [edi+VZ_CNT]
        sar eax, 12
        add eax, [edi+VZ_H0]
.env:
        imul eax, [edi+VZ_ENV]
        sar eax, 11
        mov [edi+VZ_SAL], eax
        mov edx, eax
        sar edx, 7
        mov [dsp_reg+ebx+9], dl             ; OUTX
        ; al volumen de cada lado
        movsx edx, byte [dsp_reg+ebx]       ; VOL izquierda
        imul edx, eax
        sar edx, 7
        add [mez_l], edx
        movzx esi, byte [dsp_reg+0x4D]      ; (EON: también al eco)
        bt esi, ecx
        jnc .e1
        add [eco_l], edx
.e1:
        movsx edx, byte [dsp_reg+ebx+1]     ; VOL derecha
        imul edx, eax
        sar edx, 7
        add [mez_r], edx
        bt esi, ecx
        jnc .e2
        add [eco_r], edx
.e2:
        ; la envolvente
        call voz_envolvente
        mov eax, [edi+VZ_ENV]
        shr eax, 4
        mov [dsp_reg+ebx+8], al             ; ENVX
        ; el tono (con la modulación de la voz de antes)
        movzx eax, word [dsp_reg+ebx+2]
        and eax, 0x3FFF
        test ecx, ecx
        jz .t1
        movzx edx, byte [dsp_reg+0x2D]      ; PMON
        bt edx, ecx
        jnc .t1
        mov edx, [edi+VZ_SAL-VZ_TAM]
        sar edx, 5
        imul edx, eax
        sar edx, 10
        add eax, edx
        jns .t0
        xor eax, eax
.t0:
        cmp eax, 0x3FFF
        jbe .t1
        mov eax, 0x3FFF
.t1:
        add eax, [edi+VZ_CNT]
.avanza:
        cmp eax, 0x1000
        jb .t2
        sub eax, 0x1000
        push eax
        call voz_siguiente
        pop eax
        cmp byte [edi+VZ_MODO], MODO_NADA
        je .t2
        jmp .avanza
.t2:
        mov [edi+VZ_CNT], eax
.sig:
        add edi, VZ_TAM
        inc ecx
        cmp ecx, 8
        jb .voz
        ; el eco
        call dsp_eco
        ; el volumen general y la salida
        movsx eax, byte [dsp_reg+0x0C]      ; MVOL izquierda
        imul eax, [mez_l]
        sar eax, 7
        movsx edx, byte [dsp_reg+0x2C]      ; EVOL izquierda
        imul edx, [eco_sal_l]
        sar edx, 7
        add eax, edx
        add eax, eax                        ; (al doble: las muestras son de 15 bits)
        call .satura
        mov ebx, eax
        movsx eax, byte [dsp_reg+0x1C]
        imul eax, [mez_r]
        sar eax, 7
        movsx edx, byte [dsp_reg+0x3C]
        imul edx, [eco_sal_r]
        sar edx, 7
        add eax, edx
        add eax, eax
        call .satura
        test byte [dsp_reg+0x6C], 0x40      ; FLG: silencio
        jz .s
        xor eax, eax
        xor ebx, ebx
.s:
        mov edx, [dsp_n]
        cmp edx, DSP_BUF
        jae .x
        mov [dsp_buf+edx*4], bx
        mov [dsp_buf+edx*4+2], ax
        inc dword [dsp_n]
.x:
        popad
        ret
.satura:
        cmp eax, 32767
        jle .s1
        mov eax, 32767
.s1:
        cmp eax, -32768
        jge .s2
        mov eax, -32768
.s2:
        ret

; (interna) EDI = la voz, ECX = su número: la muestra siguiente (y el bloque
; siguiente si se acaba este)
voz_siguiente:
        push eax
        push edx
        mov eax, [edi+VZ_H1]
        mov [edi+VZ_H0], eax
        mov edx, [edi+VZ_POS]
        movsx eax, word [edi+VZ_DEC+edx*2]
        mov [edi+VZ_H1], eax
        inc edx
        cmp edx, 16
        jb .x
        xor edx, edx
        cmp byte [edi+VZ_FIN], 0
        je .otro
        bts dword [dsp_reg+0x7C], ecx       ; ENDX
        cmp byte [edi+VZ_BUCLE], 0
        jne .vuelve
        mov byte [edi+VZ_MODO], MODO_NADA    ; (sin vuelta: se calla)
        mov dword [edi+VZ_ENV], 0
        mov [edi+VZ_POS], edx
        jmp .y
.vuelve:
        push ecx
        mov eax, ecx
        shl eax, 4
        movzx ecx, byte [dsp_reg+eax+4]
        movzx eax, byte [dsp_reg+0x5D]
        shl eax, 8
        lea eax, [eax+ecx*4+2]
        and eax, 0xFFFF
        movzx eax, word [aram+eax]          ; la dirección de la vuelta
        mov [edi+VZ_DIR], eax
        pop ecx
        jmp .dec
.otro:
        add dword [edi+VZ_DIR], 9
        and dword [edi+VZ_DIR], 0xFFFF
.dec:
        call brr_bloque
.x:
        mov [edi+VZ_POS], edx
.y:
        pop edx
        pop eax
        ret

; (interna) EAX = velocidad (0-31) -> CF=1 si en esta muestra le toca (la
; tabla de periodos de la consola, en muestras)
env_toca:
        and eax, 31
        jz .no
        push edx
        mov edx, [dsp_cuenta]
        cmp [env_vista+eax*4], edx  ; (ya mirada en esta muestra: lo mismo)
        je .ya
        mov [env_vista+eax*4], edx
        mov byte [env_res+eax], 0
        cmp edx, [env_sig+eax*4]    ; ¿le toca? (desde cuándo le tocaba)
        jb .ya
        mov byte [env_res+eax], 1
        push ecx
        movzx ecx, word [env_periodos+eax*2]
        add ecx, edx                ; la siguiente, un periodo después
        mov [env_sig+eax*4], ecx
        pop ecx
.ya:
        cmp byte [env_res+eax], 1
        pop edx
        jne .no
        stc
        ret
.no:
        clc
        ret
        align 2
env_periodos dw 0, 2048, 1536, 1280, 1024, 768, 640, 512, 384, 320, 256, 192, 160
             dw 128, 96, 80, 64, 48, 40, 32, 24, 20, 16, 12, 10, 8, 6, 5, 4, 3, 2, 1

; (interna) EDI = la voz, EBX = sus registros: la envolvente, un paso
voz_envolvente:
        pushad
        mov edx, [edi+VZ_ENV]
        movzx eax, byte [edi+VZ_MODO]
        cmp eax, MODO_SUELTA
        jne .a
        sub edx, 8                          ; suelta: -8 en cada muestra
        jg .pon
        xor edx, edx
        mov byte [edi+VZ_MODO], MODO_NADA
        jmp .pon
.a:
        movzx esi, byte [dsp_reg+ebx+5]     ; ADSR1
        test esi, 0x80
        jz near .gain
        cmp eax, MODO_ATAQUE
        jne .d
        mov eax, esi
        and eax, 0x0F
        lea eax, [eax*2+1]
        cmp eax, 31
        jne .a1
        add edx, 1024                       ; (el más rápido: de golpe)
        jmp .a2
.a1:
        call env_toca
        jnc .pon
        add edx, 32
.a2:
        cmp edx, 0x7FF
        jb .pon
        mov edx, 0x7FF
        mov byte [edi+VZ_MODO], MODO_CAIDA
        jmp .pon
.d:
        cmp eax, MODO_CAIDA
        jne .s
        mov eax, esi
        shr eax, 4
        and eax, 7
        lea eax, [eax*2+16]
        call env_toca
        jnc .dsl
        call .baja
.dsl:
        movzx eax, byte [dsp_reg+ebx+6]     ; el nivel de sostén
        shr eax, 5
        inc eax
        shl eax, 8
        cmp edx, eax
        ja .pon
        mov byte [edi+VZ_MODO], MODO_SOST
        jmp .pon
.s:
        movzx eax, byte [dsp_reg+ebx+6]     ; sostenida: baja con SR
        and eax, 0x1F
        call env_toca
        jnc .pon
        call .baja
        jmp .pon
.gain:
        movzx esi, byte [dsp_reg+ebx+7]     ; GAIN
        test esi, 0x80
        jnz .g1
        mov edx, esi                        ; directo
        and edx, 0x7F
        shl edx, 4
        jmp .pon
.g1:
        mov eax, esi
        and eax, 0x1F
        call env_toca
        jnc .pon
        mov eax, esi
        shr eax, 5
        and eax, 3
        jz .glin
        cmp eax, 1
        je .gexp
        cmp eax, 2
        je .gsube
        add edx, 32                         ; subida doblada
        cmp edx, 0x600 + 32
        jb .gtope
        sub edx, 24
        jmp .gtope
.gsube:
        add edx, 32
        jmp .gtope
.glin:
        sub edx, 32
        jmp .gtope
.gexp:
        call .baja
.gtope:
        test edx, edx
        jns .g2
        xor edx, edx
.g2:
        cmp edx, 0x7FF
        jbe .pon
        mov edx, 0x7FF
.pon:
        mov [edi+VZ_ENV], edx
        popad
        ret
.baja:                                      ; la bajada exponencial
        test edx, edx
        jz .b
        lea eax, [edx-1]
        shr eax, 8
        inc eax
        sub edx, eax
        jns .b
        xor edx, edx
.b:
        ret

; (interna) el eco de esta muestra: lee lo que hay en el búfer, lo pasa por
; el filtro y escribe lo nuevo
dsp_eco:
        pushad
        movzx esi, byte [dsp_reg+0x6D]      ; ESA
        shl esi, 8
        add esi, [eco_pos]
        and esi, 0xFFFF
        movsx eax, word [aram+esi]          ; lo de antes, al historial
        sar eax, 1
        lea edx, [esi+2]
        and edx, 0xFFFF
        movsx edx, word [aram+edx]
        sar edx, 1
        mov ecx, [eco_i]
        mov [eco_hist+ecx*8], eax
        mov [eco_hist+ecx*8+4], edx
        ; el filtro: 8 coeficientes (C0 el más viejo)
        xor ebx, ebx                        ; EBX = izquierda, EBP = derecha
        xor ebp, ebp
        xor edi, edi
.f:
        mov eax, ecx
        add eax, edi
        inc eax
        and eax, 7
        mov edx, edi
        shl edx, 4
        movsx edx, byte [dsp_reg+edx+0x0F]
        push edx
        imul edx, [eco_hist+eax*8]
        sar edx, 6
        add ebx, edx
        pop edx
        imul edx, [eco_hist+eax*8+4]
        sar edx, 6
        add ebp, edx
        inc edi
        cmp edi, 8
        jb .f
        inc dword [eco_i]
        and dword [eco_i], 7
        mov [eco_sal_l], ebx
        mov [eco_sal_r], ebp
        ; lo nuevo: la entrada + el filtro * EFB
        test byte [dsp_reg+0x6C], 0x20      ; FLG: sin escribir el eco
        jnz .avanza
        movsx eax, byte [dsp_reg+0x0D]      ; EFB
        push eax
        imul eax, ebx
        sar eax, 7
        add eax, [eco_l]
        call dsp_muestra.satura
        and eax, ~1
        mov [aram+esi], ax
        pop eax
        imul eax, ebp
        sar eax, 7
        add eax, [eco_r]
        call dsp_muestra.satura
        and eax, ~1
        lea edx, [esi+2]
        and edx, 0xFFFF
        mov [aram+edx], ax
.avanza:
        add dword [eco_pos], 4
        movzx eax, byte [dsp_reg+0x7D]      ; EDL: 2 KB cada uno
        and eax, 0x0F
        shl eax, 11
        jnz .l
        mov eax, 4
.l:
        cmp [eco_pos], eax
        jb .x
        mov dword [eco_pos], 0
.x:
        popad
        ret

; al acabar el cuadro: las muestras del DSP (32.000 por segundo) a la
; frecuencia de la salida
dsp_cuadro:
        pushad
        mov dword [snd_sal_n], 0
        mov ecx, [dsp_n]
        mov dword [dsp_n], 0
        mov ebx, [snd_hz_sal]
        test ebx, ebx
        jz .x
        test ecx, ecx
        jz .x
        ; el paso (16.16): 32000 / Hz
        mov eax, 32000
        shl eax, 16
        xor edx, edx
        div ebx
        mov ebx, eax
        mov edx, [snd_fase]
        mov edi, snd_sal
        shl ecx, 16                         ; ECX = el final (16.16)
.m:
        cmp edx, ecx
        jae .f
        mov eax, edx
        shr eax, 16
        mov esi, [dsp_buf+eax*4]
        cmp byte [snd_estereo], 0
        je .mono
        mov [edi], esi
        add edi, 4
        jmp .s
.mono:
        movsx eax, si
        sar esi, 16
        add eax, esi
        sar eax, 1
        mov [edi], ax
        add edi, 2
.s:
        inc dword [snd_sal_n]
        add edx, ebx
        cmp dword [snd_sal_n], SND_SAL_MAX
        jb .m
.f:
        sub edx, ecx
        mov [snd_fase], edx
.x:
        popad
        ret
SND_SAL_MAX equ 2048
