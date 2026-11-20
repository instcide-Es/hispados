; =============================================================================
;  Hispa-Snes - el procesador de sonido: SPC700, sus temporizadores y puertos
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Escrito desde la documentación pública del SPC700 (juego de
;  instrucciones, banderas, ciclos) y de la unidad de sonido de la consola
;  (los registros de F0h a FFh, los tres temporizadores, los cuatro puertos
;  con el procesador principal). El arranque (los 64 octetos de FFC0h) es
;  propio: programas/hispasnes/arranque.s.
;
;    spc_reiniciar   como al encender (con el arranque)
;    spc_correr      EDI=ciclos (1.024.000 por segundo): ejecuta hasta
;                    gastarlos; vuelve con EDI = lo que sobra (0 o menos)
;
;  Mientras corre: BL=A BH=Y (BX=YA) CL=X CH=SP ESI=PC (16 bits) EDI=ciclos
;  EDX=dirección EAX, EBP=de paso. N y Z como en el 65C816: N el bit 7 de
;  s_n, Z cuando s_z es 0. La memoria son 64 KB planos (aram); de F0h a FFh
;  son los registros (spc_io_lee y spc_io_escribe) y, con el arranque
;  puesto, FFC0h-FFFFh se lee del arranque (lo que se escribe ahí va a la
;  RAM de debajo, que se guarda aparte: s_debajo).
; =============================================================================

%macro SF8 0
        movzx eax, byte [aram+esi]
        inc si
%endmacro
%macro SF16 0
        movzx eax, word [aram+esi]
        add si, 2
%endmacro
%macro SFIN 1
        sub edi, %1
        jle near spc_fuera
        movzx eax, byte [aram+esi]
        inc si
        jmp [spc_tabla+eax*4]
%endmacro
%macro SNZ 1
        mov [s_n], %1
        mov [s_z], %1
        mov byte [s_z+1], 0
%endmacro
; leer y escribir: EDX = dirección, EAX = el dato
%macro SLEE 0
        call s_rd
%endmacro
%macro SESC 0
        call s_wr
%endmacro

; ---- los modos: dejan la dirección en EDX ----
%macro S_DP 0
        SF8
        or eax, [s_dp]
        mov edx, eax
%endmacro
%macro S_DPX 0
        SF8
        add al, cl
        or eax, [s_dp]
        mov edx, eax
%endmacro
%macro S_DPY 0
        SF8
        add al, bh
        or eax, [s_dp]
        mov edx, eax
%endmacro
%macro S_ABS 0
        SF16
        mov edx, eax
%endmacro
%macro S_ABSX 0
        SF16
        movzx edx, cl
        add dx, ax
%endmacro
%macro S_ABSY 0
        SF16
        movzx edx, bh
        add dx, ax
%endmacro
%macro S_IX 0                       ; (X)
        movzx edx, cl
        or edx, [s_dp]
%endmacro
%macro S_IY 0                       ; (Y)
        movzx edx, bh
        or edx, [s_dp]
%endmacro
; EAX = dirección (8 bits) en la página directa -> EAX = la palabra de ahí
; (la parte alta, en la misma página)
%macro S_PALABRA_DP 0
        push edx
        mov edx, eax
        or edx, [s_dp]
        call s_rd
        push eax
        inc dl
        call s_rd
        shl eax, 8
        pop edx
        or eax, edx
        pop edx
%endmacro
%macro S_IDX 0                      ; [d+X]
        SF8
        add al, cl
        S_PALABRA_DP
        mov edx, eax
%endmacro
%macro S_IDY 0                      ; [d]+Y
        SF8
        S_PALABRA_DP
        movzx edx, bh
        add dx, ax
%endmacro
%macro S_IMM 0
%endmacro

; el operando de lectura, en AL: %1 = el modo (IMM: del código)
%macro S_OPER 1
%ifidn %1, IMM
        SF8
%else
        S_%1
        SLEE
%endif
%endmacro

; ---- operaciones con A ----
%macro S_LOG 3                      ; and/or/xor, modo, ciclos
        S_OPER %2
        %1 bl, al
        SNZ bl
        SFIN %3
%endmacro
; ADC y SBC: la suma con el acarreo (las banderas del x86: AF es la media)
%macro S_ADC8 1                     ; %1 = el registro destino (8 bits); AL = operando
        mov ah, [s_c]
        shr ah, 1
        adc %1, al
        seto [s_v]
        lahf
        mov al, ah
        and ah, 1
        mov [s_c], ah
        shr al, 4
        and al, 1
        mov [s_h], al
        SNZ %1
%endmacro
%macro S_SBC8 1
        mov ah, [s_c]
        xor ah, 1
        shr ah, 1
        sbb %1, al
        seto [s_v]
        lahf
        mov al, ah
        not ah
        and ah, 1
        mov [s_c], ah
        not al
        shr al, 4
        and al, 1
        mov [s_h], al
        SNZ %1
%endmacro
%macro S_CMP8 1                     ; %1 = registro (o «dl» con el valor ya puesto)
        mov ah, %1
        sub ah, al
        setnc [s_c]
        SNZ ah
%endmacro

; un grupo de ocho: OR, AND, EOR, CMP, ADC, SBC con todos sus modos
%macro S_GRUPO 2                    ; código base, operación (OR, AND...)
SOP %1+0x04
        S_A_%2 DP, 3
SOP %1+0x05
        S_A_%2 ABS, 4
SOP %1+0x06
        S_A_%2 IX, 3
SOP %1+0x07
        S_A_%2 IDX, 6
SOP %1+0x08
        S_A_%2 IMM, 2
SOP %1+0x14
        S_A_%2 DPX, 4
SOP %1+0x15
        S_A_%2 ABSX, 5
SOP %1+0x16
        S_A_%2 ABSY, 5
SOP %1+0x17
        S_A_%2 IDY, 6
SOP %1+0x09                         ; dd, ds
        S_DP                        ; (primero el origen)
        SLEE
        mov [s_tmp], al
        S_DP
        SLEE
        mov ah, [s_tmp]
        xchg al, ah                 ; AL = origen, AH = destino
        S_M_%2 6
SOP %1+0x18                         ; d, #i
        SF8
        mov [s_tmp], al
        S_DP
        SLEE
        mov ah, [s_tmp]
        xchg al, ah
        S_M_%2 5
SOP %1+0x19                         ; (X), (Y)
        S_IY
        SLEE
        mov [s_tmp], al
        S_IX
        SLEE
        mov ah, [s_tmp]
        xchg al, ah
        S_M_%2 5
%endmacro
%macro S_A_OR 2
        S_LOG or, %1, %2
%endmacro
%macro S_A_AND 2
        S_LOG and, %1, %2
%endmacro
%macro S_A_EOR 2
        S_LOG xor, %1, %2
%endmacro
%macro S_A_CMP 2
        S_OPER %1
        S_CMP8 bl
        SFIN %2
%endmacro
%macro S_A_ADC 2
        S_OPER %1
        S_ADC8 bl
        SFIN %2
%endmacro
%macro S_A_SBC 2
        S_OPER %1
        S_SBC8 bl
        SFIN %2
%endmacro
; en memoria: AL = origen, AH = destino, EDX = dirección del destino
%macro S_M_LOGG 2
        %1 ah, al
        mov al, ah
        SNZ al
        SESC
        SFIN %2
%endmacro
%macro S_M_OR 1
        S_M_LOGG or, %1
%endmacro
%macro S_M_AND 1
        S_M_LOGG and, %1
%endmacro
%macro S_M_EOR 1
        S_M_LOGG xor, %1
%endmacro
%macro S_M_CMP 1
        sub ah, al
        setnc [s_c]
        SNZ ah
        SFIN %1
%endmacro
%macro S_M_ADC 1
        mov [s_tmp], ah
        mov ah, [s_c]
        shr ah, 1
        mov ah, [s_tmp]
        adc ah, al
        mov [s_tmp], ah
        seto [s_v]
        lahf
        mov al, ah
        and ah, 1
        mov [s_c], ah
        shr al, 4
        and al, 1
        mov [s_h], al
        mov al, [s_tmp]
        SNZ al
        SESC
        SFIN %1
%endmacro
%macro S_M_SBC 1
        mov [s_tmp], ah
        mov ah, [s_c]
        xor ah, 1
        shr ah, 1
        mov ah, [s_tmp]
        sbb ah, al
        mov [s_tmp], ah
        seto [s_v]
        lahf
        mov al, ah
        not ah
        and ah, 1
        mov [s_c], ah
        not al
        shr al, 4
        and al, 1
        mov [s_h], al
        mov al, [s_tmp]
        SNZ al
        SESC
        SFIN %1
%endmacro

; ---- desplazamientos, INC y DEC: sobre AL ----
%macro S_R_ASL 0
        shl al, 1
        setc [s_c]
        SNZ al
%endmacro
%macro S_R_LSR 0
        shr al, 1
        setc [s_c]
        SNZ al
%endmacro
%macro S_R_ROL 0
        shr byte [s_c], 1
        rcl al, 1
        setc [s_c]
        SNZ al
%endmacro
%macro S_R_ROR 0
        shr byte [s_c], 1
        rcr al, 1
        setc [s_c]
        SNZ al
%endmacro
%macro S_R_INC 0
        inc al
        SNZ al
%endmacro
%macro S_R_DEC 0
        dec al
        SNZ al
%endmacro
%macro S_RMW 3                      ; operación, modo, ciclos
        S_%2
        SLEE
        S_R_%1
        SESC
        SFIN %3
%endmacro

; ---- saltos ----
%macro S_SALTO 0                    ; el desplazamiento del código, y salta
        movsx eax, byte [aram+esi]
        inc si
        add si, ax
%endmacro
%macro S_BR 1                       ; la prueba: ZF=1 si NO se salta
        %1
        jz %%no
        S_SALTO
        SFIN 4
%%no:
        inc si
        SFIN 2
%endmacro

%macro S_P_BPL 0
        mov al, [s_n]
        not al
        test al, 0x80
%endmacro
%macro S_P_BMI 0
        test byte [s_n], 0x80
%endmacro
%macro S_P_BVC 0
        mov al, [s_v]
        xor al, 1
        test al, al
%endmacro
%macro S_P_BVS 0
        test byte [s_v], 1
%endmacro
%macro S_P_BCC 0
        mov al, [s_c]
        xor al, 1
        test al, al
%endmacro
%macro S_P_BCS 0
        test byte [s_c], 1
%endmacro
%macro S_P_BNE 0
        cmp word [s_z], 0
%endmacro
%macro S_P_BEQ 0
        cmp word [s_z], 0
        sete al
        test al, al
%endmacro

%macro SOP 1
        align 4
%assign sn %1
s_op_%[sn]:
%endmacro

; ---- la pila (página 1) ----
%macro S_APILA 0                    ; AL
        movzx edx, ch
        or edx, 0x100
        SESC
        dec ch
%endmacro
%macro S_DESAPILA 0                 ; -> AL
        inc ch
        movzx edx, ch
        or edx, 0x100
        SLEE
%endmacro
%macro S_LLAMA 0                    ; EDX = a dónde: se apila el contador y salta
        push edx
        mov eax, esi
        shr eax, 8
        S_APILA
        mov eax, esi
        S_APILA
        pop esi
%endmacro

; =============================================================================
;  Las instrucciones
; =============================================================================
        S_GRUPO 0x00, OR
        S_GRUPO 0x20, AND
        S_GRUPO 0x40, EOR
        S_GRUPO 0x60, CMP
        S_GRUPO 0x80, ADC
        S_GRUPO 0xA0, SBC

%macro S_DESPL 2                    ; código base, operación
SOP %1+0x0B
        S_RMW %2, DP, 4
SOP %1+0x0C
        S_RMW %2, ABS, 5
SOP %1+0x1B
        S_RMW %2, DPX, 5
SOP %1+0x1C
        mov al, bl
        S_R_%2
        mov bl, al
        SFIN 2
%endmacro
        S_DESPL 0x00, ASL
        S_DESPL 0x20, ROL
        S_DESPL 0x40, LSR
        S_DESPL 0x60, ROR
        S_DESPL 0x80, DEC
        S_DESPL 0xA0, INC

; SET1 y CLR1 d.b, BBS y BBC d.b, r: el bit en los 3 de arriba del código
%assign b 0
%rep 8
SOP 0x02 | (b << 5)
        S_DP
        SLEE
        or al, 1 << b
        SESC
        SFIN 4
SOP 0x12 | (b << 5)
        S_DP
        SLEE
        and al, ~(1 << b)
        SESC
        SFIN 4
SOP 0x03 | (b << 5)
        S_DP
        SLEE
        test al, 1 << b
        jz .bno
        S_SALTO
        SFIN 7
.bno:
        inc si
        SFIN 5
SOP 0x13 | (b << 5)
        S_DP
        SLEE
        test al, 1 << b
        jnz .cno
        S_SALTO
        SFIN 7
.cno:
        inc si
        SFIN 5
%assign b b + 1
%endrep

; TCALL n
%assign b 0
%rep 16
SOP 0x01 | (b << 4)
        mov edx, 0xFFDE - b * 2
        SLEE
        push eax
        inc edx
        SLEE
        shl eax, 8
        pop edx
        or edx, eax
        S_LLAMA
        SFIN 8
%assign b b + 1
%endrep

SOP 0x00                            ; NOP
        SFIN 2
SOP 0x0A                            ; OR1 C, m.b
        call s_bit_lee
        or [s_c], al
        SFIN 5
SOP 0x2A                            ; OR1 C, /m.b
        call s_bit_lee
        xor al, 1
        or [s_c], al
        SFIN 5
SOP 0x4A                            ; AND1 C, m.b
        call s_bit_lee
        and [s_c], al
        SFIN 4
SOP 0x6A                            ; AND1 C, /m.b
        call s_bit_lee
        xor al, 1
        and [s_c], al
        SFIN 4
SOP 0x8A                            ; EOR1 C, m.b
        call s_bit_lee
        xor [s_c], al
        SFIN 5
SOP 0xAA                            ; MOV1 C, m.b
        call s_bit_lee
        mov [s_c], al
        SFIN 4
SOP 0xCA                            ; MOV1 m.b, C
        call s_bit_lee
        mov al, [s_bit_v]
        mov ah, [s_bit_m]
        not ah
        and al, ah
        cmp byte [s_c], 0
        je .c0
        or al, [s_bit_m]
.c0:
        SESC
        SFIN 6
SOP 0xEA                            ; NOT1 m.b
        call s_bit_lee
        mov al, [s_bit_v]
        xor al, [s_bit_m]
        SESC
        SFIN 5
SOP 0x0D                            ; PUSH PSW
        call s_psw_lee
        S_APILA
        SFIN 4
SOP 0x2D                            ; PUSH A
        mov al, bl
        S_APILA
        SFIN 4
SOP 0x4D                            ; PUSH X
        mov al, cl
        S_APILA
        SFIN 4
SOP 0x6D                            ; PUSH Y
        mov al, bh
        S_APILA
        SFIN 4
SOP 0x8E                            ; POP PSW
        S_DESAPILA
        call s_psw_pon
        SFIN 4
SOP 0xAE                            ; POP A
        S_DESAPILA
        mov bl, al
        SFIN 4
SOP 0xCE                            ; POP X
        S_DESAPILA
        mov cl, al
        SFIN 4
SOP 0xEE                            ; POP Y
        S_DESAPILA
        mov bh, al
        SFIN 4
SOP 0x0E                            ; TSET1 !a
        S_ABS
        SLEE
        mov ah, bl
        sub ah, al
        SNZ ah
        or al, bl
        SESC
        SFIN 6
SOP 0x4E                            ; TCLR1 !a
        S_ABS
        SLEE
        mov ah, bl
        sub ah, al
        SNZ ah
        mov ah, bl
        not ah
        and al, ah
        SESC
        SFIN 6
SOP 0x0F                            ; BRK
        mov edx, esi
        S_LLAMA
        call s_psw_lee
        S_APILA
        mov byte [s_b], 1
        mov byte [s_i], 0
        mov edx, 0xFFDE
        SLEE
        push eax
        inc edx
        SLEE
        shl eax, 8
        pop esi
        or esi, eax
        SFIN 8
SOP 0x10
        S_BR S_P_BPL
SOP 0x30
        S_BR S_P_BMI
SOP 0x50
        S_BR S_P_BVC
SOP 0x70
        S_BR S_P_BVS
SOP 0x90
        S_BR S_P_BCC
SOP 0xB0
        S_BR S_P_BCS
SOP 0xD0
        S_BR S_P_BNE
SOP 0xF0
        S_BR S_P_BEQ
SOP 0x2F                            ; BRA
        S_SALTO
        SFIN 4
SOP 0x1A                            ; DECW d
        SF8
        mov [s_tmp], al
        S_PALABRA_DP
        dec ax
        call s_palabra_dp_esc
        SFIN 6
SOP 0x3A                            ; INCW d
        SF8
        mov [s_tmp], al
        S_PALABRA_DP
        inc ax
        call s_palabra_dp_esc
        SFIN 6
SOP 0x5A                            ; CMPW YA, d
        SF8
        S_PALABRA_DP
        mov edx, ebx
        sub dx, ax
        setnc [s_c]
        mov [s_z], dx
        mov [s_n], dh
        SFIN 4
SOP 0x7A                            ; ADDW YA, d
        SF8
        S_PALABRA_DP
        mov edx, ebx                ; H: el acarreo del bit 11
        and edx, 0x0FFF
        mov ebp, eax
        and ebp, 0x0FFF
        add edx, ebp
        cmp edx, 0x0FFF
        seta [s_h]
        add bx, ax
        setc [s_c]
        seto [s_v]
        mov [s_z], bx
        mov [s_n], bh
        SFIN 5
SOP 0x9A                            ; SUBW YA, d
        SF8
        S_PALABRA_DP
        mov edx, ebx
        and edx, 0x0FFF
        mov ebp, eax
        and ebp, 0x0FFF
        cmp edx, ebp
        setae [s_h]
        sub bx, ax
        setnc [s_c]
        seto [s_v]
        mov [s_z], bx
        mov [s_n], bh
        SFIN 5
SOP 0xBA                            ; MOVW YA, d
        SF8
        S_PALABRA_DP
        mov bx, ax
        mov [s_z], ax
        mov [s_n], ah
        SFIN 5
SOP 0xDA                            ; MOVW d, YA (sin banderas)
        SF8
        mov [s_tmp], al
        mov ax, [s_z]
        push eax
        mov dl, [s_n]
        push edx
        mov eax, ebx
        call s_palabra_dp_esc
        pop edx
        mov [s_n], dl
        pop eax
        mov [s_z], ax
        SFIN 5
SOP 0x1D                            ; DEC X
        dec cl
        SNZ cl
        SFIN 2
SOP 0x3D                            ; INC X
        inc cl
        SNZ cl
        SFIN 2
SOP 0xDC                            ; DEC Y
        dec bh
        SNZ bh
        SFIN 2
SOP 0xFC                            ; INC Y
        inc bh
        SNZ bh
        SFIN 2
SOP 0x1E                            ; CMP X, !a
        S_ABS
        SLEE
        S_CMP8 cl
        SFIN 4
SOP 0x3E                            ; CMP X, d
        S_DP
        SLEE
        S_CMP8 cl
        SFIN 3
SOP 0xC8                            ; CMP X, #i
        SF8
        S_CMP8 cl
        SFIN 2
SOP 0x5E                            ; CMP Y, !a
        S_ABS
        SLEE
        S_CMP8 bh
        SFIN 4
SOP 0x7E                            ; CMP Y, d
        S_DP
        SLEE
        S_CMP8 bh
        SFIN 3
SOP 0xAD                            ; CMP Y, #i
        SF8
        S_CMP8 bh
        SFIN 2
SOP 0x1F                            ; JMP [!a+X]
        S_ABSX
        SLEE
        push eax
        inc dx
        SLEE
        shl eax, 8
        pop esi
        or esi, eax
        SFIN 6
SOP 0x5F                            ; JMP !a
        SF16
        mov esi, eax
        SFIN 3
SOP 0x3F                            ; CALL !a
        SF16
        mov edx, eax
        S_LLAMA
        SFIN 8
SOP 0x4F                            ; PCALL u
        SF8
        or eax, 0xFF00
        mov edx, eax
        S_LLAMA
        SFIN 6
SOP 0x6F                            ; RET
        S_DESAPILA
        push eax
        S_DESAPILA
        shl eax, 8
        pop esi
        or esi, eax
        SFIN 5
SOP 0x7F                            ; RETI
        S_DESAPILA
        call s_psw_pon
        S_DESAPILA
        push eax
        S_DESAPILA
        shl eax, 8
        pop esi
        or esi, eax
        SFIN 6
SOP 0x20                            ; CLRP
        mov dword [s_dp], 0
        SFIN 2
SOP 0x40                            ; SETP
        mov dword [s_dp], 0x100
        SFIN 2
SOP 0x60                            ; CLRC
        mov byte [s_c], 0
        SFIN 2
SOP 0x80                            ; SETC
        mov byte [s_c], 1
        SFIN 2
SOP 0xED                            ; NOTC
        xor byte [s_c], 1
        SFIN 3
SOP 0xE0                            ; CLRV (y H)
        mov byte [s_v], 0
        mov byte [s_h], 0
        SFIN 2
SOP 0xA0                            ; EI
        mov byte [s_i], 1
        SFIN 3
SOP 0xC0                            ; DI
        mov byte [s_i], 0
        SFIN 3
SOP 0x2E                            ; CBNE d, r
        S_DP
        SLEE
        cmp al, bl
        je .no
        S_SALTO
        SFIN 7
.no:
        inc si
        SFIN 5
SOP 0xDE                            ; CBNE d+X, r
        S_DPX
        SLEE
        cmp al, bl
        je .no
        S_SALTO
        SFIN 8
.no:
        inc si
        SFIN 6
SOP 0x6E                            ; DBNZ d, r
        S_DP
        SLEE
        dec al
        SESC
        test al, al
        jz .no
        S_SALTO
        SFIN 7
.no:
        inc si
        SFIN 5
SOP 0xFE                            ; DBNZ Y, r
        dec bh
        jz .no
        S_SALTO
        SFIN 6
.no:
        inc si
        SFIN 4
SOP 0x5D                            ; MOV X, A
        mov cl, bl
        SNZ cl
        SFIN 2
SOP 0x7D                            ; MOV A, X
        mov bl, cl
        SNZ bl
        SFIN 2
SOP 0xDD                            ; MOV A, Y
        mov bl, bh
        SNZ bl
        SFIN 2
SOP 0xFD                            ; MOV Y, A
        mov bh, bl
        SNZ bh
        SFIN 2
SOP 0x9D                            ; MOV X, SP
        mov cl, ch
        SNZ cl
        SFIN 2
SOP 0xBD                            ; MOV SP, X
        mov ch, cl
        SFIN 2
SOP 0x8D                            ; MOV Y, #i
        SF8
        mov bh, al
        SNZ al
        SFIN 2
SOP 0xCD                            ; MOV X, #i
        SF8
        mov cl, al
        SNZ al
        SFIN 2
SOP 0xE8                            ; MOV A, #i
        SF8
        mov bl, al
        SNZ al
        SFIN 2
%macro S_CARGA 3                    ; registro, modo, ciclos
        S_%2
        SLEE
        mov %1, al
        SNZ al
        SFIN %3
%endmacro
SOP 0xE6
        S_CARGA bl, IX, 3
SOP 0xE4
        S_CARGA bl, DP, 3
SOP 0xF4
        S_CARGA bl, DPX, 4
SOP 0xE5
        S_CARGA bl, ABS, 4
SOP 0xF5
        S_CARGA bl, ABSX, 5
SOP 0xF6
        S_CARGA bl, ABSY, 5
SOP 0xE7
        S_CARGA bl, IDX, 6
SOP 0xF7
        S_CARGA bl, IDY, 6
SOP 0xF8
        S_CARGA cl, DP, 3
SOP 0xF9
        S_CARGA cl, DPY, 4
SOP 0xE9
        S_CARGA cl, ABS, 4
SOP 0xEB
        S_CARGA bh, DP, 3
SOP 0xFB
        S_CARGA bh, DPX, 4
SOP 0xEC
        S_CARGA bh, ABS, 4
SOP 0xBF                            ; MOV A, (X)+
        S_IX
        SLEE
        mov bl, al
        SNZ al
        inc cl
        SFIN 4
%macro S_GUARDA 3                   ; registro, modo, ciclos (lee antes: el bus)
        S_%2
        SLEE
        mov al, %1
        SESC
        SFIN %3
%endmacro
SOP 0xC6
        S_GUARDA bl, IX, 4
SOP 0xC4
        S_GUARDA bl, DP, 4
SOP 0xD4
        S_GUARDA bl, DPX, 5
SOP 0xC5
        S_GUARDA bl, ABS, 5
SOP 0xD5
        S_GUARDA bl, ABSX, 6
SOP 0xD6
        S_GUARDA bl, ABSY, 6
SOP 0xC7
        S_GUARDA bl, IDX, 7
SOP 0xD7
        S_GUARDA bl, IDY, 7
SOP 0xD8
        S_GUARDA cl, DP, 4
SOP 0xD9
        S_GUARDA cl, DPY, 5
SOP 0xC9
        S_GUARDA cl, ABS, 5
SOP 0xCB
        S_GUARDA bh, DP, 4
SOP 0xDB
        S_GUARDA bh, DPX, 5
SOP 0xCC
        S_GUARDA bh, ABS, 5
SOP 0xAF                            ; MOV (X)+, A
        S_IX
        mov al, bl
        SESC
        inc cl
        SFIN 4
SOP 0xFA                            ; MOV dd, ds
        S_DP
        SLEE
        mov [s_tmp], al
        S_DP
        mov al, [s_tmp]
        SESC
        SFIN 5
SOP 0x8F                            ; MOV d, #i
        SF8
        mov [s_tmp], al
        S_DP
        SLEE
        mov al, [s_tmp]
        SESC
        SFIN 5
SOP 0xCF                            ; MUL YA
        mov al, bh
        mul bl
        mov bx, ax
        SNZ bh
        SFIN 9
SOP 0x9E                            ; DIV YA, X
        call s_div
        SFIN 12
SOP 0x9F                            ; XCN A
        rol bl, 4
        SNZ bl
        SFIN 5
SOP 0xDF                            ; DAA A
        cmp byte [s_c], 0
        jne .a
        cmp bl, 0x99
        jbe .b
.a:
        add bl, 0x60
        mov byte [s_c], 1
.b:
        cmp byte [s_h], 0
        jne .c
        mov al, bl
        and al, 0x0F
        cmp al, 9
        jbe .d
.c:
        add bl, 6
.d:
        SNZ bl
        SFIN 3
SOP 0xBE                            ; DAS A
        cmp byte [s_c], 0
        je .a
        cmp bl, 0x99
        jbe .b
.a:
        sub bl, 0x60
        mov byte [s_c], 0
.b:
        cmp byte [s_h], 0
        je .c
        mov al, bl
        and al, 0x0F
        cmp al, 9
        jbe .d
.c:
        sub bl, 6
.d:
        SNZ bl
        SFIN 3
SOP 0xEF                            ; SLEEP
        mov byte [s_parado], 1
        xor edi, edi
        jmp spc_fuera
SOP 0xFF                            ; STOP
        mov byte [s_parado], 1
        xor edi, edi
        jmp spc_fuera

        align 4
spc_tabla:
%assign i 0
%rep 256
        dd s_op_%[i]
%assign i i + 1
%endrep

; =============================================================================
;  Lo común
; =============================================================================
spc_correr:
        push eax
        push ebx
        push ecx
        push edx
        push ebp
        push esi
        mov [s_pres], edi
        cmp byte [s_parado], 0
        jne .parado
        movzx ebx, word [s_ya]
        movzx ecx, byte [s_x]
        mov ch, [s_sp]
        movzx esi, word [s_pc]
        movzx eax, byte [aram+esi]
        inc si
        jmp [spc_tabla+eax*4]
.parado:
        xor edi, edi
        jmp spc_fuera.x
spc_fuera:
        mov [s_ya], bx
        mov [s_x], cl
        mov [s_sp], ch
        mov [s_pc], si
.x:
        mov eax, [s_pres]           ; el tiempo, al día
        sub eax, edi
        add [spc_t], eax
        pop esi
        pop ebp
        pop edx
        pop ecx
        pop ebx
        pop eax
        ret

; el SPC700 como al encender: el arranque en FFC0h, los registros a cero
spc_reiniciar:
        pushad
        xor eax, eax
        mov [s_ya], eax
        mov [s_x], al
        mov byte [s_sp], 0xEF
        mov [s_dp], eax
        mov [s_n], al
        mov word [s_z], 1
        mov [s_c], al
        mov [s_v], al
        mov [s_h], al
        mov [s_i], al
        mov [s_b], al
        mov [s_parado], al
        mov [spc_t], eax
        mov [s_dsp_dir], al
        mov edi, s_ent
        stosd                       ; los puertos
        mov edi, s_sal
        stosd
        mov edi, s_tim
        mov ecx, S_TIM_TAM * 3 / 4
        rep stosd
        mov edi, aram               ; la RAM: a cero
        mov ecx, 0x10000 / 4
        rep stosd
        mov edi, s_debajo
        mov ecx, 64 / 4
        rep stosd
        mov byte [s_ipl], 0
        xor edi, edi                ; (para s_ahora: el tiempo, 0)
        mov [s_pres], edi
        mov al, 0x80                ; el arranque, puesto
        call spc_control
        mov word [s_pc], 0xFFC0
        popad
        ret

; -> AL = PSW
s_psw_lee:
        mov al, [s_n]
        and al, 0x80
        cmp byte [s_v], 0
        je .v
        or al, 0x40
.v:
        cmp dword [s_dp], 0
        je .p
        or al, 0x20
.p:
        cmp byte [s_b], 0
        je .b
        or al, 0x10
.b:
        cmp byte [s_h], 0
        je .h
        or al, 0x08
.h:
        cmp byte [s_i], 0
        je .i
        or al, 0x04
.i:
        cmp word [s_z], 0
        jne .z
        or al, 0x02
.z:
        or al, [s_c]
        movzx eax, al
        ret
; AL = PSW
s_psw_pon:
        mov [s_n], al
        mov ah, al
        shr ah, 6
        and ah, 1
        mov [s_v], ah
        xor edx, edx
        test al, 0x20
        jz .p
        mov edx, 0x100
.p:
        mov [s_dp], edx
        mov ah, al
        shr ah, 4
        and ah, 1
        mov [s_b], ah
        mov ah, al
        shr ah, 3
        and ah, 1
        mov [s_h], ah
        mov ah, al
        shr ah, 2
        and ah, 1
        mov [s_i], ah
        mov ah, al
        not ah
        shr ah, 1
        and ah, 1
        mov [s_z], ah
        mov byte [s_z+1], 0
        and al, 1
        mov [s_c], al
        ret

; (con el operando m.b en el código) -> AL = el bit (0/1); EDX = la dirección,
; [s_bit_v] = el octeto, [s_bit_m] = la máscara
s_bit_lee:
        SF16
        mov edx, eax
        shr eax, 13
        mov [s_tmp], al
        and edx, 0x1FFF
        SLEE
        mov [s_bit_v], al
        push ecx
        mov cl, [s_tmp]
        mov ah, 1
        shl ah, cl
        pop ecx
        mov [s_bit_m], ah
        and al, ah
        setnz al
        movzx eax, al
        ret

; AX = la palabra, [s_tmp] = su dirección en la página directa
s_palabra_dp_esc:
        push eax
        movzx edx, byte [s_tmp]
        or edx, [s_dp]
        call s_wr
        pop eax
        push eax
        mov al, ah
        inc dl
        call s_wr
        pop eax
        mov [s_z], ax
        mov [s_n], ah
        ret

; DIV YA, X: A = YA / X, Y = el resto (con lo que hace el procesador cuando
; no cabe: documentado)
s_div:
        push ecx
        movzx eax, bx               ; EAX = YA
        movzx ebp, cl               ; EBP = X
        cmp bh, cl                  ; V = Y >= X
        setae [s_v]
        mov dl, bh
        and dl, 0x0F
        mov dh, cl
        and dh, 0x0F
        cmp dl, dh                  ; H = (Y & 15) >= (X & 15)
        setae [s_h]
        movzx edx, bh
        lea ecx, [ebp+ebp]
        cmp edx, ecx
        jae .grande
        xor edx, edx                ; Y < 2X: la división de siempre
        div ebp
        mov bl, al
        mov bh, dl
        jmp .x
.grande:
        mov ecx, ebp                ; A = 255 - (YA - X*512) / (256 - X)
        shl ecx, 9                  ; Y = X + (YA - X*512) % (256 - X)
        sub eax, ecx
        mov ecx, 256
        sub ecx, ebp
        xor edx, edx
        div ecx
        mov ecx, 255
        sub ecx, eax
        mov bl, cl
        add edx, ebp
        mov bh, dl
.x:
        pop ecx
        SNZ bl
        ret

; ---- la memoria ----
; EDX = dirección -> EAX
s_rd:
        lea eax, [edx-0xF0]
        cmp eax, 0x10
        jb spc_io_lee
        movzx eax, byte [aram+edx]
        ret
; EDX = dirección, AL = dato
s_wr:
        push ebp
        lea ebp, [edx-0xF0]
        cmp ebp, 0x10
        jb .io
        cmp edx, 0xFFC0
        jae .alto
.ram:
        mov [aram+edx], al
        pop ebp
        ret
.alto:
        cmp byte [s_ipl], 0
        je .ram
        mov [s_debajo+edx-0xFFC0], al
        pop ebp
        ret
.io:
        pop ebp
        jmp spc_io_escribe

; ---- los registros de F0h a FFh ----
; (EDX = dirección; conservan todo menos EAX)
spc_io_lee:
        cmp byte [s_plano], 0       ; (las pruebas: 64 KB planos)
        jne .plano
        push edx
        sub edx, 0xF0
        cmp edx, 2
        jb .cero
        je .dirdsp
        cmp edx, 3
        je .dsp
        cmp edx, 8
        jb .puerto
        cmp edx, 0x0A
        jb .ram
        cmp edx, 0x0D
        jb .cero
        sub edx, 0x0D               ; FDh-FFh: los contadores (se borran al leer)
        push ebx
        mov ebx, edx
        call s_ahora
        call tim_ajusta
        imul ebx, S_TIM_TAM
        movzx eax, byte [s_tim+ebx+TI_SAL]
        mov byte [s_tim+ebx+TI_SAL], 0
        pop ebx
        pop edx
        ret
.cero:
        xor eax, eax
        pop edx
        ret
.dirdsp:
        movzx eax, byte [s_dsp_dir]
        pop edx
        ret
.ram:
        pop edx
        movzx eax, byte [aram+edx]
        ret
.puerto:
        movzx eax, byte [s_ent+edx-4]
        pop edx
        ret
.dsp:
        call s_ahora
        call dsp_hasta
        movzx eax, byte [s_dsp_dir]
        and eax, 0x7F
        movzx eax, byte [dsp_reg+eax]
        pop edx
        ret
.plano:
        movzx eax, byte [aram+edx]
        ret

spc_io_escribe:
        cmp byte [s_plano], 0
        jne .plano
        push edx
        push eax
        mov [aram+edx], al          ; (también va a la RAM de debajo)
        sub edx, 0xF0
        cmp edx, 1
        je .control
        cmp edx, 2
        je .dir
        cmp edx, 3
        je .dato
        cmp edx, 4
        jb .x
        cmp edx, 8
        jb .puerto
        cmp edx, 0x0A
        jb .x
        cmp edx, 0x0D
        jae .x
        sub edx, 0x0A               ; FAh-FCh: hasta dónde cuenta cada uno
        push ebx
        mov ebx, edx
        call s_ahora
        call tim_ajusta
        imul ebx, S_TIM_TAM
        mov eax, [esp+4]            ; (el dato)
        mov [s_tim+ebx+TI_OBJ], al
        pop ebx
        jmp .x
.control:
        call spc_control
        jmp .x
.dir:
        mov [s_dsp_dir], al
        jmp .x
.dato:
        test byte [s_dsp_dir], 0x80
        jnz .x
        call s_ahora
        call dsp_hasta
        movzx edx, byte [s_dsp_dir]
        mov eax, [esp]              ; (el dato)
        call dsp_escribe
        jmp .x
.puerto:
        mov [s_sal+edx-4], al
.x:
        pop eax
        pop edx
        ret
.plano:
        mov [aram+edx], al
        ret

; AL = lo que se escribe en el registro de control (F1h)
spc_control:
        pushad
        mov dl, al
        test dl, 0x10               ; borrar los puertos de entrada 0 y 1
        jz .p1
        mov word [s_ent], 0
.p1:
        test dl, 0x20               ;   y el 2 y el 3
        jz .p2
        mov word [s_ent+2], 0
.p2:
        xor ebx, ebx                ; los temporizadores: al encender, de cero
.t:
        call s_ahora
        call tim_ajusta
        mov ecx, ebx
        mov al, 1
        shl al, cl
        imul ecx, ebx, S_TIM_TAM
        test dl, al
        jz .apaga
        cmp byte [s_tim+ecx+TI_ENC], 0
        jne .sig
        mov byte [s_tim+ecx+TI_ENC], 1
        mov dword [s_tim+ecx+TI_INT], 0
        mov byte [s_tim+ecx+TI_SAL], 0
        jmp .sig
.apaga:
        mov byte [s_tim+ecx+TI_ENC], 0
.sig:
        inc ebx
        cmp ebx, 3
        jb .t
        ; el arranque, puesto o quitado
        mov al, dl
        shr al, 7
        cmp al, [s_ipl]
        je .x
        mov [s_ipl], al
        mov esi, aram + 0xFFC0
        mov edi, s_debajo
        test al, al
        jz .quita
        mov ecx, 64                 ; ponerlo: lo de debajo, aparte
        rep movsb
        mov esi, spc_arranque
        mov edi, aram + 0xFFC0
        mov ecx, 64
        rep movsb
        jmp .x
.quita:
        xchg esi, edi
        mov ecx, 64
        rep movsb
.x:
        popad
        ret

; -> EAX = el ciclo del SPC700 en este momento (con EDI, lo que queda)
s_ahora:
        mov eax, [s_pres]
        sub eax, edi
        add eax, [spc_t]
        ret

; EBX = temporizador (0-2), EAX = ahora: lo pone al día (conserva todo)
tim_ajusta:
        push edx                    ; (lo corriente: no ha pasado ni un paso
        push ecx                    ;  desde la última vez, y no hay nada que
        imul edx, ebx, S_TIM_TAM    ;  hacer)
        mov ecx, eax
        sub ecx, [s_tim+edx+TI_ULT]
        cmp ebx, 2
        je .t2
        cmp ecx, 128
        jmp .t
.t2:
        cmp ecx, 16
.t:
        pop ecx
        pop edx
        jae .paso
        ret
.paso:
        pushad
        mov ebp, ebx
        imul ebx, S_TIM_TAM
        mov ecx, 7                  ; los dos primeros, cada 128 ciclos
        cmp ebp, 2                  ;   (8.000 por segundo); el tercero, 16
        jne .a
        mov ecx, 4
.a:
        mov edx, eax
        sub edx, [s_tim+ebx+TI_ULT]
        shr edx, cl                 ; EDX = los pasos que han pasado
        mov esi, edx
        shl esi, cl
        add [s_tim+ebx+TI_ULT], esi
        cmp byte [s_tim+ebx+TI_ENC], 0
        je .x
        test edx, edx
        jz .x
        movzx ecx, byte [s_tim+ebx+TI_OBJ]
        test ecx, ecx
        jnz .b
        mov ecx, 256
.b:
        mov eax, [s_tim+ebx+TI_INT]
        add eax, edx
        xor edx, edx
        div ecx
        mov [s_tim+ebx+TI_INT], edx
        add [s_tim+ebx+TI_SAL], al
        and byte [s_tim+ebx+TI_SAL], 0x0F
.x:
        popad
        ret

S_TIM_TAM   equ 16                  ; cada temporizador (s_tim):
TI_ULT      equ 0                   ;   dd hasta qué ciclo está contado
TI_INT      equ 4                   ;   dd la cuenta de dentro
TI_OBJ      equ 8                   ;   db hasta dónde cuenta (0 = 256)
TI_SAL      equ 9                   ;   db el contador de 4 bits que se lee
TI_ENC      equ 10                  ;   db encendido

; el arranque (programas/hispasnes/arranque.s, ensamblado)
spc_arranque:
        db 0xCD, 0xEF, 0xBD, 0xE8, 0x00, 0xC6, 0x1D, 0xD0, 0xFC, 0x8F, 0xAA, 0xF4, 0x8F, 0xBB, 0xF5, 0x78
        db 0xCC, 0xF4, 0xD0, 0xFB, 0xF8, 0xF4, 0xBA, 0xF6, 0xDA, 0x00, 0xE4, 0xF5, 0xD8, 0xF4, 0xF0, 0x19
        db 0x8D, 0x00, 0x7E, 0xF4, 0xD0, 0xFC, 0xE4, 0xF5, 0xD7, 0x00, 0xCB, 0xF4, 0xFC, 0xD0, 0x02, 0xAB
        db 0x01, 0x7E, 0xF4, 0xF0, 0xF1, 0x10, 0xFA, 0x2F, 0xDB, 0xCD, 0x00, 0x1F, 0x00, 0x00, 0xC0, 0xFF
