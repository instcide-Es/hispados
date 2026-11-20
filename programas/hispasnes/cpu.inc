; =============================================================================
;  Hispa-Snes - el procesador: 65C816 (el sucesor de 16 bits del 6502)
;  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
;
;  Escrito desde la documentación pública del 65C816 (el manual de su
;  fabricante: juego de instrucciones, modos de direccionamiento, banderas,
;  modo de emulación y modo nativo, y los ciclos de cada instrucción). Todas
;  las instrucciones, con el modo decimal, en los dos modos (emulación del
;  6502 y nativo) y con el acumulador y los índices de 8 o de 16 bits.
;
;    cpu_reiniciar   pone el procesador como al encender (modo de emulación,
;                    con el vector de «reset»)
;    cpu_correr      EDI=ciclos del procesador que se le dan: ejecuta hasta
;                    gastarlos (o hasta que alguien pida parar, cpu_romper).
;                    Vuelve con EDI = lo que sobra (0 o menos si los gastó)
;
;  Cada instrucción está escrita cinco veces, una por cada combinación de
;  modos (emulación; nativo con M y X a 0 o a 1), con su tabla de 256 saltos:
;  así ninguna mira en qué modo está. Al cambiar de modo (REP, SEP, XCE,
;  PLP, RTI) se pasa a la tabla que toca.
;
;  Mientras corre:  EBX=A (BL la parte baja, BH la alta: «B»)  ECX=X  EBP=Y
;  ESI=puntero a la instrucción (el contador de programa es ESI-[pcb])
;  EDI=ciclos que quedan  EDX=dirección  EAX=de paso. N y Z se guardan como
;  «el último resultado»: N es el bit 7 de f_n y Z vale cuando f_z es 0.
;
;  La memoria se lee y escribe con rd8, rd16, wr8 y wr16 (mapas.inc): EDX es
;  la dirección de 24 bits y EAX el dato; conservan los demás registros. Las
;  de «_b0» dan la vuelta dentro del banco 0 (la página directa y la pila).
;  El código se lee directamente con ESI: vale mientras siga dentro de la
;  misma zona de memoria (un banco de la ROM, la RAM), que es lo que hacen
;  los programas; los saltos vuelven a mirar la tabla de páginas.
; =============================================================================

; ---- leer del código ----
%macro F8 0
        movzx eax, byte [esi]
        inc esi
%endmacro
%macro F16 0
        movzx eax, word [esi]
        add esi, 2
%endmacro
%macro F24 0
        mov eax, [esi]
        and eax, 0xFFFFFF
        add esi, 3
%endmacro

; la instrucción siguiente, con la tabla de este modo
%macro DESPACHA 0
        movzx eax, byte [esi]
        inc esi
        jmp [tab_%[SUF] + eax*4]
%endmacro
; fin de una instrucción de %1 ciclos
%macro FIN 1
        sub edi, %1
        jle near cpu_fuera
        DESPACHA
%endmacro
; fin de una instrucción que cambia de modo: la tabla, otra vez
%macro FINM 1
        sub edi, %1
        jle near cpu_fuera
        jmp cpu_sigue
%endmacro

; N y Z de un resultado de 8 o de 16 bits
%macro NZ8 1
        mov [f_n], %1
        mov [f_z], %1
        mov byte [f_z+1], 0
%endmacro
%macro NZ16 1
        mov [f_z], %1
%ifidni %1, ax
        mov [f_n], ah
%elifidni %1, bx
        mov [f_n], bh
%elifidni %1, cx
        mov [f_n], ch
%elifidni %1, dx
        mov [f_n], dh
%else
 %error "NZ16: registro"
%endif
%endmacro

; ---- los modos de direccionamiento: dejan la dirección en EDX ----
; %1 = 1 si es para escribir (o leer y escribir): sin el ciclo de más al
; cruzar de página, que ya va contado. BZ = 1 si lo de 16 bits da la vuelta
; dentro del banco 0 (página directa y pila)
%macro AM_DP 1
        F8
        add ax, [r_d]
        mov edx, eax
        sub edi, [d_extra]
%assign BZ 1
%endmacro
; d,X y d,Y. En emulación, con la parte baja de D a 0, no sale de la página
%macro AM_DPIDX 1                   ; %1 = ecx o ebp
        F8
%if E
        cmp byte [r_d], 0
        jne %%n
        mov edx, %1
        add al, dl
        mov ah, [r_d+1]
        movzx edx, ax
        jmp %%x
%%n:
%endif
        add eax, %1
        add ax, [r_d]
        movzx edx, ax
%%x:
        sub edi, [d_extra]
%endmacro
%macro AM_DPX 1
        AM_DPIDX ecx
%assign BZ 1
%endmacro
%macro AM_DPY 1
        AM_DPIDX ebp
%assign BZ 1
%endmacro
; EDX = dirección de un puntero de 16 bits en el banco 0 -> EAX = el puntero
; (en emulación, con la parte baja de D a 0, la parte alta, en la misma página)
%macro PUNTERO_DP 0
%if E
        cmp byte [r_d], 0
        jne %%n
        call rd16_pag
        jmp %%x
%%n:
%endif
        call rd16_b0
%%x:
%endmacro
%macro AM_DPI 1                     ; (d)
        AM_DP 0
        PUNTERO_DP
        or eax, [r_dbr]
        mov edx, eax
%assign BZ 0
%endmacro
%macro AM_DPXI 1                    ; (d,X)
        AM_DPIDX ecx
        PUNTERO_DP
        or eax, [r_dbr]
        mov edx, eax
%assign BZ 0
%endmacro
; (d),Y: el ciclo de más al leer, siempre con los índices de 16 bits y con
; los de 8 si al sumar Y se cruza de página
%macro AM_DPIY 1                    ; (d),Y
        AM_DP 0
        PUNTERO_DP
        or eax, [r_dbr]
        lea edx, [eax+ebp]
        and edx, 0xFFFFFF
%if %1 = 0
 %if X8
        xor eax, edx
        test eax, 0xFFFF00
        jz %%n
        dec edi
%%n:
 %else
        dec edi
 %endif
%endif
%assign BZ 0
%endmacro
; EDX = dirección (banco 0) de un puntero de 24 bits -> EAX
%macro PUNTERO24 0
        call rd16_b0
        push eax
        inc dx
        inc dx
        call rd8
        shl eax, 16
        pop edx
        or eax, edx
%endmacro
%macro AM_DPIL 1                    ; [d]
        AM_DP 0
        PUNTERO24
        mov edx, eax
%assign BZ 0
%endmacro
%macro AM_DPILY 1                   ; [d],Y
        AM_DP 0
        PUNTERO24
        lea edx, [eax+ebp]
        and edx, 0xFFFFFF
%assign BZ 0
%endmacro
%macro AM_ABS 1
        F16
        or eax, [r_dbr]
        mov edx, eax
%assign BZ 0
%endmacro
%macro AM_ABSIDX 2                  ; %1 = para escribir, %2 = ecx o ebp
        F16
        or eax, [r_dbr]
        lea edx, [eax+%2]
        and edx, 0xFFFFFF
%if %1 = 0
 %if X8
        xor eax, edx
        test eax, 0xFFFF00
        jz %%n
        dec edi
%%n:
 %else
        dec edi
 %endif
%endif
%endmacro
%macro AM_ABSX 1
        AM_ABSIDX %1, ecx
%assign BZ 0
%endmacro
%macro AM_ABSY 1
        AM_ABSIDX %1, ebp
%assign BZ 0
%endmacro
%macro AM_LONG 1
        F24
        mov edx, eax
%assign BZ 0
%endmacro
%macro AM_LONGX 1
        F24
        lea edx, [eax+ecx]
        and edx, 0xFFFFFF
%assign BZ 0
%endmacro
%macro AM_SR 1                      ; d,S
        F8
        add ax, [r_s]
        mov edx, eax
%assign BZ 1
%endmacro
%macro AM_SRIY 1                    ; (d,S),Y
        F8
        add ax, [r_s]
        mov edx, eax
        call rd16_b0
        or eax, [r_dbr]
        lea edx, [eax+ebp]
        and edx, 0xFFFFFF
%assign BZ 0
%endmacro

; leer y escribir el dato de EDX: %1 = 1 de 8 bits, 0 de 16
%macro LEE 1
%if %1
        call rd8
%elif BZ
        call rd16_b0
%else
        call rd16
%endif
%endmacro
%macro ESCRIBE 1
%if %1
        call wr8
%elif BZ
        call wr16_b0
%else
        call wr16
%endif
%endmacro
; el operando, en EAX: %1 = el modo, %2 = 1 si es de 8 bits
%macro OPER 2
%ifidn %1, IMM
 %if %2
        F8
 %else
        F16
 %endif
%else
        AM_%1 0
        LEE %2
%endif
%endmacro

; ---- la pila (banco 0). %1 = 1: en emulación, da la vuelta en la página 1 ----
%macro APILA8 1                     ; AL
        mov edx, [r_s]
        call wr8
%if E && %1
        dec byte [r_s]
%else
        dec word [r_s]
%endif
%endmacro
%macro DESAPILA8 1                  ; -> EAX
%if E && %1
        inc byte [r_s]
%else
        inc word [r_s]
%endif
        mov edx, [r_s]
        call rd8
%endmacro
%macro APILA16 1                    ; AX: la parte alta primero
        push eax
        shr eax, 8
        APILA8 %1
        pop eax
        APILA8 %1
%endmacro
%macro DESAPILA16 1                 ; -> EAX
        DESAPILA8 %1
        push eax
        DESAPILA8 %1
        shl eax, 8
        pop edx
        or eax, edx
%endmacro
; (las instrucciones nuevas del 65C816 usan S de 16 bits también en
; emulación; al acabar, en emulación, la parte alta vuelve a ser 1)
%macro PILA_E 0
%if E
        mov byte [r_s+1], 1
%endif
%endmacro

; ---- saltos: EDX = el contador de programa nuevo ----
%macro SALTA16 0                    ; dentro del banco del programa
        or edx, [r_pbr]
        call cpu_salta
%endmacro
%macro SALTA24 0                    ; a otro banco (EDX de 24 bits)
        call cpu_salta
%endmacro
; EAX = el contador de programa (16 bits) en este momento
%macro PC16 0
        mov eax, esi
        sub eax, [pcb]
        and eax, 0xFFFF
%endmacro

; =============================================================================
;  Las instrucciones
; =============================================================================
; ---- de carga y de guardar ----
%macro I_LDA 2
        OPER %1, M8
%if M8
        mov bl, al
        NZ8 bl
        FIN %2
%else
        mov bx, ax
        NZ16 bx
        FIN %2+1
%endif
%endmacro
%macro I_LDX 2
        OPER %1, X8
        mov ecx, eax
%if X8
        NZ8 cl
        FIN %2
%else
        NZ16 cx
        FIN %2+1
%endif
%endmacro
%macro I_LDY 2
        OPER %1, X8
        mov ebp, eax
%if X8
        NZ8 al
        FIN %2
%else
        NZ16 ax
        FIN %2+1
%endif
%endmacro
%macro I_STA 2
        AM_%1 1
        mov eax, ebx
        ESCRIBE M8
%if M8
        FIN %2
%else
        FIN %2+1
%endif
%endmacro
%macro I_STX 2
        AM_%1 1
        mov eax, ecx
        ESCRIBE X8
%if X8
        FIN %2
%else
        FIN %2+1
%endif
%endmacro
%macro I_STY 2
        AM_%1 1
        mov eax, ebp
        ESCRIBE X8
%if X8
        FIN %2
%else
        FIN %2+1
%endif
%endmacro
%macro I_STZ 2
        AM_%1 1
        xor eax, eax
        ESCRIBE M8
%if M8
        FIN %2
%else
        FIN %2+1
%endif
%endmacro

; ---- lógicas y aritméticas ----
%macro I_LOG 3                      ; and, or, xor; modo; ciclos
        OPER %2, M8
%if M8
        %1 bl, al
        NZ8 bl
        FIN %3
%else
        %1 bx, ax
        NZ16 bx
        FIN %3+1
%endif
%endmacro
%macro I_AND 2
        I_LOG and, %1, %2
%endmacro
%macro I_ORA 2
        I_LOG or, %1, %2
%endmacro
%macro I_EOR 2
        I_LOG xor, %1, %2
%endmacro
%macro I_ADC 2
        OPER %1, M8
        cmp byte [f_d], 0
        jne %%bcd
        mov dl, [f_c]
        shr dl, 1
%if M8
        adc bl, al
        setc [f_c]
        seto [f_v]
        NZ8 bl
        FIN %2
%else
        adc bx, ax
        setc [f_c]
        seto [f_v]
        NZ16 bx
        FIN %2+1
%endif
%%bcd:
%if M8
        call adc_bcd8
        FIN %2
%else
        call adc_bcd16
        FIN %2+1
%endif
%endmacro
%macro I_SBC 2
        OPER %1, M8
        cmp byte [f_d], 0
        jne %%bcd
        mov dl, [f_c]
        xor dl, 1
        shr dl, 1
%if M8
        sbb bl, al
        setnc [f_c]
        seto [f_v]
        NZ8 bl
        FIN %2
%else
        sbb bx, ax
        setnc [f_c]
        seto [f_v]
        NZ16 bx
        FIN %2+1
%endif
%%bcd:
%if M8
        call sbc_bcd8
        FIN %2
%else
        call sbc_bcd16
        FIN %2+1
%endif
%endmacro
%macro I_CMP 2
        OPER %1, M8
%if M8
        mov dl, bl
        sub dl, al
        setnc [f_c]
        NZ8 dl
        FIN %2
%else
        mov edx, ebx
        sub dx, ax
        setnc [f_c]
        NZ16 dx
        FIN %2+1
%endif
%endmacro
%macro I_CPX 2
        OPER %1, X8
%if X8
        mov dl, cl
        sub dl, al
        setnc [f_c]
        NZ8 dl
        FIN %2
%else
        mov edx, ecx
        sub dx, ax
        setnc [f_c]
        NZ16 dx
        FIN %2+1
%endif
%endmacro
%macro I_CPY 2
        OPER %1, X8
        mov edx, ebp
%if X8
        sub dl, al
        setnc [f_c]
        NZ8 dl
        FIN %2
%else
        sub dx, ax
        setnc [f_c]
        NZ16 dx
        FIN %2+1
%endif
%endmacro
%macro I_BIT 2
        OPER %1, M8
%if M8
        mov dl, al
        and dl, bl
        mov [f_z], dl
        mov byte [f_z+1], 0
 %ifnidn %1, IMM
        mov [f_n], al
        shr al, 6
        and al, 1
        mov [f_v], al
 %endif
        FIN %2
%else
        mov edx, eax
        and dx, bx
        mov [f_z], dx
 %ifnidn %1, IMM
        mov [f_n], ah
        shr ah, 6
        and ah, 1
        mov [f_v], ah
 %endif
        FIN %2+1
%endif
%endmacro

; ---- leer, cambiar y escribir (en memoria o en A) ----
; la operación, sobre AL o AX (%1 = 1: 8 bits); deja N y Z
%macro R_ASL 1
%if %1
        shl al, 1
        setc [f_c]
        NZ8 al
%else
        shl ax, 1
        setc [f_c]
        NZ16 ax
%endif
%endmacro
%macro R_LSR 1
%if %1
        shr al, 1
        setc [f_c]
        NZ8 al
%else
        shr ax, 1
        setc [f_c]
        NZ16 ax
%endif
%endmacro
%macro R_ROL 1
        shr byte [f_c], 1
%if %1
        rcl al, 1
        setc [f_c]
        NZ8 al
%else
        rcl ax, 1
        setc [f_c]
        NZ16 ax
%endif
%endmacro
%macro R_ROR 1
        shr byte [f_c], 1
%if %1
        rcr al, 1
        setc [f_c]
        NZ8 al
%else
        rcr ax, 1
        setc [f_c]
        NZ16 ax
%endif
%endmacro
%macro R_INC 1
%if %1
        inc al
        NZ8 al
%else
        inc ax
        NZ16 ax
%endif
%endmacro
%macro R_DEC 1
%if %1
        dec al
        NZ8 al
%else
        dec ax
        NZ16 ax
%endif
%endmacro
%macro R_TSB 1                      ; Z = (A y M) = 0; M = M o A
%if %1
        mov [tmp_rmw], al
        and al, bl
        mov [f_z], al
        mov byte [f_z+1], 0
        mov al, [tmp_rmw]
        or al, bl
%else
        mov [tmp_rmw], ax
        and ax, bx
        mov [f_z], ax
        mov ax, [tmp_rmw]
        or ax, bx
%endif
%endmacro
%macro R_TRB 1                      ; Z = (A y M) = 0; M = M y no A
%if %1
        mov [tmp_rmw], al
        and al, bl
        mov [f_z], al
        mov byte [f_z+1], 0
        mov al, bl
        not al
        and al, [tmp_rmw]
%else
        mov [tmp_rmw], ax
        and ax, bx
        mov [f_z], ax
        mov eax, ebx
        not ax
        and ax, [tmp_rmw]
%endif
%endmacro
%macro I_RMW 3                      ; operación, modo, ciclos
        AM_%2 1
        LEE M8
        R_%1 M8
        ESCRIBE M8
%if M8
        FIN %3
%else
        FIN %3+2
%endif
%endmacro
%macro I_ASL 2
        I_RMW ASL, %1, %2
%endmacro
%macro I_LSR 2
        I_RMW LSR, %1, %2
%endmacro
%macro I_ROL 2
        I_RMW ROL, %1, %2
%endmacro
%macro I_ROR 2
        I_RMW ROR, %1, %2
%endmacro
%macro I_INC 2
        I_RMW INC, %1, %2
%endmacro
%macro I_DEC 2
        I_RMW DEC, %1, %2
%endmacro
%macro I_TSB 2
        I_RMW TSB, %1, %2
%endmacro
%macro I_TRB 2
        I_RMW TRB, %1, %2
%endmacro
%macro I_ACU 1                      ; ASL, LSR... sobre el acumulador
        mov eax, ebx
        R_%1 M8
%if M8
        mov bl, al
%else
        mov bx, ax
%endif
        FIN 2
%endmacro

; ---- los índices ----
%macro I_INX 0
%if X8
        inc cl
        NZ8 cl
%else
        inc cx
        NZ16 cx
%endif
        FIN 2
%endmacro
%macro I_DEX 0
%if X8
        dec cl
        NZ8 cl
%else
        dec cx
        NZ16 cx
%endif
        FIN 2
%endmacro
%macro I_INY 0
        mov eax, ebp
%if X8
        inc al
        NZ8 al
%else
        inc ax
        NZ16 ax
%endif
        mov ebp, eax
        FIN 2
%endmacro
%macro I_DEY 0
        mov eax, ebp
%if X8
        dec al
        NZ8 al
%else
        dec ax
        NZ16 ax
%endif
        mov ebp, eax
        FIN 2
%endmacro

; ---- pasar de un registro a otro ----
%macro A_INDICE 0                   ; EAX = A, al ancho de los índices (con N y Z)
        mov eax, ebx
%if X8
        movzx eax, al
        NZ8 al
%else
        movzx eax, ax
        NZ16 ax
%endif
%endmacro
%macro INDICE_A 1                   ; %1 = ecx o ebp -> A, al ancho de A
        mov eax, %1
%if M8
        mov bl, al
        NZ8 bl
%else
        mov bx, ax
        NZ16 bx
%endif
%endmacro

; ---- saltos condicionales ----
; %1: la prueba (deja ZF=1 si NO se salta)
%macro I_BR 1
        %1
        jz %%no
        movsx eax, byte [esi]
        inc esi
        sub esi, [pcb]
%if E
        mov edx, esi
%endif
        add esi, eax
        and esi, 0xFFFF
%if E
        xor edx, esi
        test edx, 0xFF00
        jz %%m
        dec edi
%%m:
%endif
        add esi, [pcb]
        FIN 3
%%no:
        inc esi
        FIN 2
%endmacro
%macro P_BPL 0
        mov al, [f_n]
        not al
        test al, 0x80
%endmacro
%macro P_BMI 0
        test byte [f_n], 0x80
%endmacro
%macro P_BVC 0
        mov al, [f_v]
        xor al, 1
        test al, al
%endmacro
%macro P_BVS 0
        test byte [f_v], 1
%endmacro
%macro P_BCC 0
        mov al, [f_c]
        xor al, 1
        test al, al
%endmacro
%macro P_BCS 0
        test byte [f_c], 1
%endmacro
%macro P_BNE 0
        cmp word [f_z], 0
%endmacro
%macro P_BEQ 0
        cmp word [f_z], 0
        sete al
        test al, al
%endmacro
%macro P_BRA 0
        or al, 1                    ; (ZF=0)
%endmacro

; =============================================================================
;  Las 256 instrucciones de un modo (E, M8 y X8 ya puestos)
; =============================================================================
%macro H 1
        align 4
%assign hn %1
h_%[SUF]_%[hn]:
%endmacro

%macro CUERPO 0
H 0x00                              ; BRK
        inc esi                     ; (el octeto de la firma)
        mov dl, 1
%if E
        mov eax, 0xFFFE
%else
        mov eax, 0xFFE6
%endif
        call cpu_int
        FINM 0
H 0x01
        I_ORA DPXI, 6
H 0x02                              ; COP
        inc esi
        mov dl, 1
%if E
        mov eax, 0xFFF4
%else
        mov eax, 0xFFE4
%endif
        call cpu_int
        FINM 0
H 0x03
        I_ORA SR, 4
H 0x04
        I_TSB DP, 5
H 0x05
        I_ORA DP, 3
H 0x06
        I_ASL DP, 5
H 0x07
        I_ORA DPIL, 6
H 0x08                              ; PHP
        call cpu_p_lee
        APILA8 1
        FIN 3
H 0x09
        I_ORA IMM, 2
H 0x0A
        I_ACU ASL
H 0x0B                              ; PHD
        mov eax, [r_d]
        APILA16 0
        PILA_E
        FIN 4
H 0x0C
        I_TSB ABS, 6
H 0x0D
        I_ORA ABS, 4
H 0x0E
        I_ASL ABS, 6
H 0x0F
        I_ORA LONG, 5
H 0x10
        I_BR P_BPL
H 0x11
        I_ORA DPIY, 5
H 0x12
        I_ORA DPI, 5
H 0x13
        I_ORA SRIY, 7
H 0x14
        I_TRB DP, 5
H 0x15
        I_ORA DPX, 4
H 0x16
        I_ASL DPX, 6
H 0x17
        I_ORA DPILY, 6
H 0x18                              ; CLC
        mov byte [f_c], 0
        FIN 2
H 0x19
        I_ORA ABSY, 4
H 0x1A
        I_ACU INC
H 0x1B                              ; TCS
%if E
        mov [r_s], bl
%else
        mov [r_s], bx
%endif
        FIN 2
H 0x1C
        I_TRB ABS, 6
H 0x1D
        I_ORA ABSX, 4
H 0x1E
        I_ASL ABSX, 7
H 0x1F
        I_ORA LONGX, 5
H 0x20                              ; JSR a
        F16
        push eax
        PC16
        dec ax
        APILA16 1
        pop edx
        SALTA16
        FIN 6
H 0x21
        I_AND DPXI, 6
H 0x22                              ; JSL al
        F24
        push eax
        mov eax, [r_pbr]
        shr eax, 16
        APILA8 0
        PC16
        dec ax
        APILA16 0
        PILA_E
        pop edx
        SALTA24
        FIN 8
H 0x23
        I_AND SR, 4
H 0x24
        I_BIT DP, 3
H 0x25
        I_AND DP, 3
H 0x26
        I_ROL DP, 5
H 0x27
        I_AND DPIL, 6
H 0x28                              ; PLP
        DESAPILA8 1
        call cpu_p_pon
        call cpu_irq_mira
        FINM 4
H 0x29
        I_AND IMM, 2
H 0x2A
        I_ACU ROL
H 0x2B                              ; PLD
        DESAPILA16 0
        PILA_E
        mov [r_d], ax
        NZ16 ax
        call cpu_d_extra
        FIN 5
H 0x2C
        I_BIT ABS, 4
H 0x2D
        I_AND ABS, 4
H 0x2E
        I_ROL ABS, 6
H 0x2F
        I_AND LONG, 5
H 0x30
        I_BR P_BMI
H 0x31
        I_AND DPIY, 5
H 0x32
        I_AND DPI, 5
H 0x33
        I_AND SRIY, 7
H 0x34
        I_BIT DPX, 4
H 0x35
        I_AND DPX, 4
H 0x36
        I_ROL DPX, 6
H 0x37
        I_AND DPILY, 6
H 0x38                              ; SEC
        mov byte [f_c], 1
        FIN 2
H 0x39
        I_AND ABSY, 4
H 0x3A
        I_ACU DEC
H 0x3B                              ; TSC
        mov bx, [r_s]
        NZ16 bx
        FIN 2
H 0x3C
        I_BIT ABSX, 4
H 0x3D
        I_AND ABSX, 4
H 0x3E
        I_ROL ABSX, 7
H 0x3F
        I_AND LONGX, 5
H 0x40                              ; RTI
        DESAPILA8 1
        call cpu_p_pon
        DESAPILA16 1
        push eax
%if E
        pop edx
        SALTA16
        call cpu_irq_mira
        FINM 6
%else
        DESAPILA8 1
        shl eax, 16
        pop edx
        or edx, eax
        SALTA24
        call cpu_irq_mira
        FINM 7
%endif
H 0x41
        I_EOR DPXI, 6
H 0x42                              ; WDM (dos octetos, no hace nada)
        inc esi
        FIN 2
H 0x43
        I_EOR SR, 4
H 0x44                              ; MVP
        mov dl, -1
        jmp cpu_mueve_%[SUF]
H 0x45
        I_EOR DP, 3
H 0x46
        I_LSR DP, 5
H 0x47
        I_EOR DPIL, 6
H 0x48                              ; PHA
%if M8
        mov eax, ebx
        APILA8 1
        FIN 3
%else
        mov eax, ebx
        APILA16 1
        FIN 4
%endif
H 0x49
        I_EOR IMM, 2
H 0x4A
        I_ACU LSR
H 0x4B                              ; PHK
        mov eax, [r_pbr]
        shr eax, 16
        APILA8 1
        FIN 3
H 0x4C                              ; JMP a
        F16
        mov edx, eax
        SALTA16
        FIN 3
H 0x4D
        I_EOR ABS, 4
H 0x4E
        I_LSR ABS, 6
H 0x4F
        I_EOR LONG, 5
H 0x50
        I_BR P_BVC
H 0x51
        I_EOR DPIY, 5
H 0x52
        I_EOR DPI, 5
H 0x53
        I_EOR SRIY, 7
H 0x54                              ; MVN
        mov dl, 1
        jmp cpu_mueve_%[SUF]
H 0x55
        I_EOR DPX, 4
H 0x56
        I_LSR DPX, 6
H 0x57
        I_EOR DPILY, 6
H 0x58                              ; CLI
        mov byte [f_i], 0
        call cpu_irq_mira
        FIN 2
H 0x59
        I_EOR ABSY, 4
H 0x5A                              ; PHY
        mov eax, ebp
%if X8
        APILA8 1
        FIN 3
%else
        APILA16 1
        FIN 4
%endif
H 0x5B                              ; TCD
        mov [r_d], bx
        NZ16 bx
        call cpu_d_extra
        FIN 2
H 0x5C                              ; JML al
        F24
        mov edx, eax
        SALTA24
        FIN 4
H 0x5D
        I_EOR ABSX, 4
H 0x5E
        I_LSR ABSX, 7
H 0x5F
        I_EOR LONGX, 5
H 0x60                              ; RTS
        DESAPILA16 1
        inc ax
        mov edx, eax
        SALTA16
        FIN 6
H 0x61
        I_ADC DPXI, 6
H 0x62                              ; PER
        F16
        mov edx, eax
        PC16
        add eax, edx
        APILA16 0
        PILA_E
        FIN 6
H 0x63
        I_ADC SR, 4
H 0x64
        I_STZ DP, 3
H 0x65
        I_ADC DP, 3
H 0x66
        I_ROR DP, 5
H 0x67
        I_ADC DPIL, 6
H 0x68                              ; PLA
%if M8
        DESAPILA8 1
        mov bl, al
        NZ8 bl
        FIN 4
%else
        DESAPILA16 1
        mov bx, ax
        NZ16 bx
        FIN 5
%endif
H 0x69
        I_ADC IMM, 2
H 0x6A
        I_ACU ROR
H 0x6B                              ; RTL
        DESAPILA16 0
        push eax
        DESAPILA8 0
        PILA_E
        shl eax, 16
        pop edx
        inc dx
        or edx, eax
        SALTA24
        FIN 6
H 0x6C                              ; JMP (a)
        F16
        mov edx, eax
        call rd16_b0
        mov edx, eax
        SALTA16
        FIN 5
H 0x6D
        I_ADC ABS, 4
H 0x6E
        I_ROR ABS, 6
H 0x6F
        I_ADC LONG, 5
H 0x70
        I_BR P_BVS
H 0x71
        I_ADC DPIY, 5
H 0x72
        I_ADC DPI, 5
H 0x73
        I_ADC SRIY, 7
H 0x74
        I_STZ DPX, 4
H 0x75
        I_ADC DPX, 4
H 0x76
        I_ROR DPX, 6
H 0x77
        I_ADC DPILY, 6
H 0x78                              ; SEI
        mov byte [f_i], 1
        FIN 2
H 0x79
        I_ADC ABSY, 4
H 0x7A                              ; PLY
%if X8
        DESAPILA8 1
        mov ebp, eax
        NZ8 al
        FIN 4
%else
        DESAPILA16 1
        mov ebp, eax
        NZ16 ax
        FIN 5
%endif
H 0x7B                              ; TDC
        mov bx, [r_d]
        NZ16 bx
        FIN 2
H 0x7C                              ; JMP (a,X)
        F16
        add ax, cx
        or eax, [r_pbr]
        mov edx, eax
        call rd16_banco
        mov edx, eax
        SALTA16
        FIN 6
H 0x7D
        I_ADC ABSX, 4
H 0x7E
        I_ROR ABSX, 7
H 0x7F
        I_ADC LONGX, 5
H 0x80
        I_BR P_BRA
H 0x81
        I_STA DPXI, 6
H 0x82                              ; BRL
        F16
        mov edx, eax
        PC16
        add edx, eax
        and edx, 0xFFFF
        lea esi, [edx]
        add esi, [pcb]
        FIN 4
H 0x83
        I_STA SR, 4
H 0x84
        I_STY DP, 3
H 0x85
        I_STA DP, 3
H 0x86
        I_STX DP, 3
H 0x87
        I_STA DPIL, 6
H 0x88
        I_DEY
H 0x89
        I_BIT IMM, 2
H 0x8A                              ; TXA
        INDICE_A ecx
        FIN 2
H 0x8B                              ; PHB
        mov eax, [r_dbr]
        shr eax, 16
        APILA8 0
        PILA_E
        FIN 3
H 0x8C
        I_STY ABS, 4
H 0x8D
        I_STA ABS, 4
H 0x8E
        I_STX ABS, 4
H 0x8F
        I_STA LONG, 5
H 0x90
        I_BR P_BCC
H 0x91
        I_STA DPIY, 6
H 0x92
        I_STA DPI, 5
H 0x93
        I_STA SRIY, 7
H 0x94
        I_STY DPX, 4
H 0x95
        I_STA DPX, 4
H 0x96
        I_STX DPY, 4
H 0x97
        I_STA DPILY, 6
H 0x98                              ; TYA
        INDICE_A ebp
        FIN 2
H 0x99
        I_STA ABSY, 5
H 0x9A                              ; TXS
%if E
        mov [r_s], cl
%else
        mov [r_s], cx
%endif
        FIN 2
H 0x9B                              ; TXY
        mov ebp, ecx
%if X8
        NZ8 cl
%else
        NZ16 cx
%endif
        FIN 2
H 0x9C
        I_STZ ABS, 4
H 0x9D
        I_STA ABSX, 5
H 0x9E
        I_STZ ABSX, 5
H 0x9F
        I_STA LONGX, 5
H 0xA0
        I_LDY IMM, 2
H 0xA1
        I_LDA DPXI, 6
H 0xA2
        I_LDX IMM, 2
H 0xA3
        I_LDA SR, 4
H 0xA4
        I_LDY DP, 3
H 0xA5
        I_LDA DP, 3
H 0xA6
        I_LDX DP, 3
H 0xA7
        I_LDA DPIL, 6
H 0xA8                              ; TAY
        A_INDICE
        mov ebp, eax
        FIN 2
H 0xA9
        I_LDA IMM, 2
H 0xAA                              ; TAX
        A_INDICE
        mov ecx, eax
        FIN 2
H 0xAB                              ; PLB
        DESAPILA8 0
        PILA_E
        NZ8 al
        shl eax, 16
        mov [r_dbr], eax
        FIN 4
H 0xAC
        I_LDY ABS, 4
H 0xAD
        I_LDA ABS, 4
H 0xAE
        I_LDX ABS, 4
H 0xAF
        I_LDA LONG, 5
H 0xB0
        I_BR P_BCS
H 0xB1
        I_LDA DPIY, 5
H 0xB2
        I_LDA DPI, 5
H 0xB3
        I_LDA SRIY, 7
H 0xB4
        I_LDY DPX, 4
H 0xB5
        I_LDA DPX, 4
H 0xB6
        I_LDX DPY, 4
H 0xB7
        I_LDA DPILY, 6
H 0xB8                              ; CLV
        mov byte [f_v], 0
        FIN 2
H 0xB9
        I_LDA ABSY, 4
H 0xBA                              ; TSX
        mov eax, [r_s]
%if X8
        movzx ecx, al
        NZ8 cl
%else
        mov ecx, eax
        NZ16 cx
%endif
        FIN 2
H 0xBB                              ; TYX
        mov ecx, ebp
%if X8
        NZ8 cl
%else
        NZ16 cx
%endif
        FIN 2
H 0xBC
        I_LDY ABSX, 4
H 0xBD
        I_LDA ABSX, 4
H 0xBE
        I_LDX ABSY, 4
H 0xBF
        I_LDA LONGX, 5
H 0xC0
        I_CPY IMM, 2
H 0xC1
        I_CMP DPXI, 6
H 0xC2                              ; REP
        F8
        not al
        push eax
        call cpu_p_lee
        pop edx
        and al, dl
        call cpu_p_pon
        call cpu_irq_mira
        FINM 3
H 0xC3
        I_CMP SR, 4
H 0xC4
        I_CPY DP, 3
H 0xC5
        I_CMP DP, 3
H 0xC6
        I_DEC DP, 5
H 0xC7
        I_CMP DPIL, 6
H 0xC8
        I_INY
H 0xC9
        I_CMP IMM, 2
H 0xCA
        I_DEX
H 0xCB                              ; WAI
        mov byte [cpu_wai], 1
        sub edi, 3
        jmp cpu_para
H 0xCC
        I_CPY ABS, 4
H 0xCD
        I_CMP ABS, 4
H 0xCE
        I_DEC ABS, 6
H 0xCF
        I_CMP LONG, 5
H 0xD0
        I_BR P_BNE
H 0xD1
        I_CMP DPIY, 5
H 0xD2
        I_CMP DPI, 5
H 0xD3
        I_CMP SRIY, 7
H 0xD4                              ; PEI
        AM_DP 0
        call rd16_b0
        APILA16 0
        PILA_E
        FIN 6
H 0xD5
        I_CMP DPX, 4
H 0xD6
        I_DEC DPX, 6
H 0xD7
        I_CMP DPILY, 6
H 0xD8                              ; CLD
        mov byte [f_d], 0
        FIN 2
H 0xD9
        I_CMP ABSY, 4
H 0xDA                              ; PHX
        mov eax, ecx
%if X8
        APILA8 1
        FIN 3
%else
        APILA16 1
        FIN 4
%endif
H 0xDB                              ; STP
        mov byte [cpu_stp], 1
        sub edi, 3
        jmp cpu_para
H 0xDC                              ; JML [a]
        F16
        mov edx, eax
        PUNTERO24
        mov edx, eax
        SALTA24
        FIN 6
H 0xDD
        I_CMP ABSX, 4
H 0xDE
        I_DEC ABSX, 7
H 0xDF
        I_CMP LONGX, 5
H 0xE0
        I_CPX IMM, 2
H 0xE1
        I_SBC DPXI, 6
H 0xE2                              ; SEP
        F8
        push eax
        call cpu_p_lee
        pop edx
        or al, dl
        call cpu_p_pon
        FINM 3
H 0xE3
        I_SBC SR, 4
H 0xE4
        I_CPX DP, 3
H 0xE5
        I_SBC DP, 3
H 0xE6
        I_INC DP, 5
H 0xE7
        I_SBC DPIL, 6
H 0xE8
        I_INX
H 0xE9
        I_SBC IMM, 2
H 0xEA                              ; NOP
        FIN 2
H 0xEB                              ; XBA
        xchg bl, bh
        NZ8 bl
        FIN 3
H 0xEC
        I_CPX ABS, 4
H 0xED
        I_SBC ABS, 4
H 0xEE
        I_INC ABS, 6
H 0xEF
        I_SBC LONG, 5
H 0xF0
        I_BR P_BEQ
H 0xF1
        I_SBC DPIY, 5
H 0xF2
        I_SBC DPI, 5
H 0xF3
        I_SBC SRIY, 7
H 0xF4                              ; PEA
        F16
        APILA16 0
        PILA_E
        FIN 5
H 0xF5
        I_SBC DPX, 4
H 0xF6
        I_INC DPX, 6
H 0xF7
        I_SBC DPILY, 6
H 0xF8                              ; SED
        mov byte [f_d], 1
        FIN 2
H 0xF9
        I_SBC ABSY, 4
H 0xFA                              ; PLX
%if X8
        DESAPILA8 1
        mov ecx, eax
        NZ8 al
        FIN 4
%else
        DESAPILA16 1
        mov ecx, eax
        NZ16 ax
        FIN 5
%endif
H 0xFB                              ; XCE
        mov al, [f_c]
        mov ah, [f_e]
        mov [f_e], al
        mov [f_c], ah
        call cpu_modo_e
        FINM 2
H 0xFC                              ; JSR (a,X)
        F16
        add ax, cx
        or eax, [r_pbr]
        push eax
        PC16
        dec ax
        APILA16 0
        PILA_E
        pop edx
        call rd16_banco
        mov edx, eax
        SALTA16
        FIN 8
H 0xFD
        I_SBC ABSX, 4
H 0xFE
        I_INC ABSX, 7
H 0xFF
        I_SBC LONGX, 5

; MVN y MVP (DL = +1 o -1): un octeto cada 7 ciclos, de [origen:X] a
; [destino:Y], hasta que A (de 16 bits siempre) pase de 0 a FFFFh. Si se
; acaba el tiempo a medias, la instrucción vuelve a empezar después (como
; en el procesador, que la repite entera por cada octeto)
        align 4
cpu_mueve_%[SUF]:
        movsx eax, dl
        mov [mv_paso], eax
        movzx eax, byte [esi]       ; el banco de destino
        shl eax, 16
        mov [r_dbr], eax
        movzx eax, byte [esi+1]     ; y el de origen
        shl eax, 16
        mov [mv_org], eax
        add esi, 2
.otro:
        mov edx, ecx
        or edx, [mv_org]
        call rd8
        mov edx, ebp
        or edx, [r_dbr]
        call wr8
        mov eax, [mv_paso]
%if X8
        add cl, al
        mov edx, ebp
        add dl, al
        mov ebp, edx
%else
        add cx, ax
        mov edx, ebp
        add dx, ax
        mov ebp, edx
%endif
        sub edi, 7
        dec bx
        cmp bx, 0xFFFF
        je .acaba
        test edi, edi
        jg .otro
        sub esi, 3                  ; (sin acabar: otra vez, en la vuelta siguiente)
        jmp cpu_fuera
.acaba:
        FIN 0
%endmacro

; ---- las cinco versiones y sus tablas ----
%macro GENERA 3                     ; E, M8, X8
%assign E %1
%assign M8 %2
%assign X8 %3
%define SUF %1%2%3
        CUERPO
        align 4
tab_%[SUF]:
%assign i 0
%rep 256
        dd h_%[SUF]_%[i]
%assign i i + 1
%endrep
%undef SUF
%endmacro

        GENERA 1, 1, 1
        GENERA 0, 0, 0
        GENERA 0, 0, 1
        GENERA 0, 1, 0
        GENERA 0, 1, 1

; =============================================================================
;  Lo común
; =============================================================================
; cpu_correr: EDI = ciclos -> EDI = los que sobran
cpu_correr:
        push eax
        push ebx
        push ecx
        push edx
        push ebp
        push esi
        mov ebx, [r_a]
        mov ecx, [r_x]
        mov ebp, [r_y]
        mov edx, [r_pc]
        or edx, [r_pbr]
        call cpu_salta
cpu_mira:                           ; (vuelve aquí cuando hay que mirar las
        cmp byte [cpu_stp], 0       ;  interrupciones)
        jne .quieto
        cmp byte [nmi_pend], 0
        jne .nmi
        cmp byte [irq_linea], 0
        je .wai
        mov byte [cpu_wai], 0       ; (una IRQ acaba con WAI aunque esté tapada)
        cmp byte [f_i], 0
        jne cpu_sigue
        xor dl, dl
        mov eax, 0xFFEE
        cmp byte [f_e], 0
        je .irq
        mov eax, 0xFFFE
.irq:
        call cpu_int
        jmp cpu_sigue
.nmi:
        mov byte [nmi_pend], 0
        mov byte [cpu_wai], 0
        xor dl, dl
        mov eax, 0xFFEA
        cmp byte [f_e], 0
        je .n1
        mov eax, 0xFFFA
.n1:
        call cpu_int
        jmp cpu_sigue
.wai:
        cmp byte [cpu_wai], 0
        je cpu_sigue
.quieto:                            ; parado (WAI o STP): se va el tiempo
        xor edi, edi
        jmp cpu_fuera
; sigue con la tabla del modo en el que esté
cpu_sigue:
        test edi, edi
        jle cpu_fuera
        call cpu_tabla
        movzx eax, byte [esi]
        inc esi
        jmp [edx+eax*4]
; WAI y STP: se para aquí (lo que queda del tramo se va)
cpu_para:
        xor edi, edi
cpu_fuera:
        cmp dword [cpu_romper], 0   ; ¿alguien ha pedido parar? (lo que quedaba
        je .x                       ;  vuelve a EDI)
        add edi, [cpu_romper]
        mov dword [cpu_romper], 0
        cmp byte [cpu_volver], 0    ; (para mirar las interrupciones: sigue;
        jne .v                      ;  para que el bucle de la consola vuelva
        test edi, edi               ;  a hacer sus cuentas: vuelve)
        jg cpu_mira
        jmp .x
.v:
        mov byte [cpu_volver], 0
.x:
        mov [r_a], ebx
        mov [r_x], ecx
        mov [r_y], ebp
        PC16
        mov [r_pc], eax
        pop esi
        pop ebp
        pop edx
        pop ecx
        pop ebx
        pop eax
        ret

; -> EDX = la tabla del modo
cpu_tabla:
        mov edx, tab_111
        cmp byte [f_e], 0
        jne .x
        movzx eax, byte [f_m]
        add eax, eax
        or al, [f_x]
        mov edx, [tablas+eax*4]
.x:
        ret
        align 4
tablas      dd tab_000, tab_001, tab_010, tab_011

; (tras CLI, PLP, REP, RTI) si hay una IRQ esperando y ya no está tapada, que
; el bucle la atienda: se para después de esta instrucción
cpu_irq_mira:
        cmp byte [irq_linea], 0
        je .x
        cmp byte [f_i], 0
        jne .x
        add [cpu_romper], edi       ; (lo que quedaba, para luego)
        mov edi, 1
.x:
        ret

; EDX = dirección de 24 bits: el contador de programa pasa a ser ella
cpu_salta:
        push eax
        mov eax, edx
        and eax, 0xFF0000
        mov [r_pbr], eax
        mov eax, edx
        shr eax, 13
        mov eax, [pag_lee+eax*4]
        test eax, eax
        jz .nada
        lea esi, [eax+edx]
        mov edx, [r_pbr]
        add eax, edx
        mov [pcb], eax
        pop eax
        ret
.nada:                              ; (código donde no hay memoria: STP)
        movzx eax, dx
        mov esi, pag_vacia
        mov [pcb], esi
        add esi, eax
        and edx, 0xFF0000
        pop eax
        ret

; una interrupción: EAX = su vector (en el banco 0), DL = 1 si es BRK o COP
; (en emulación, la bandera B en el estado apilado)
cpu_int:
        push eax
        mov [int_brk], dl
        cmp byte [f_e], 0
        jne .e
        mov eax, [r_pbr]            ; nativo: el banco, el contador y P
        shr eax, 16
        mov edx, [r_s]
        call wr8
        dec word [r_s]
        PC16
        mov edx, [r_s]
        push eax
        shr eax, 8
        call wr8
        dec word [r_s]
        pop eax
        mov edx, [r_s]
        call wr8
        dec word [r_s]
        call cpu_p_lee
        mov edx, [r_s]
        call wr8
        dec word [r_s]
        sub edi, 8
        jmp .vec
.e:
        PC16                        ; emulación: el contador y P (con B)
        mov edx, [r_s]
        push eax
        shr eax, 8
        call wr8
        dec byte [r_s]
        pop eax
        mov edx, [r_s]
        call wr8
        dec byte [r_s]
        call cpu_p_lee
        cmp byte [int_brk], 0
        jne .b
        and al, ~0x10
.b:
        mov edx, [r_s]
        call wr8
        dec byte [r_s]
        sub edi, 7
.vec:
        mov byte [f_i], 1
        mov byte [f_d], 0
        pop edx
        call rd16_b0
        mov edx, eax
        call cpu_salta
        ret

; -> AL = el registro P
cpu_p_lee:
        mov al, [f_n]
        and al, 0x80
        cmp byte [f_v], 0
        je .v
        or al, 0x40
.v:
        cmp byte [f_m], 0
        je .m
        or al, 0x20
.m:
        cmp byte [f_x], 0
        je .xx
        or al, 0x10
.xx:
        cmp byte [f_d], 0
        je .d
        or al, 0x08
.d:
        cmp byte [f_i], 0
        je .i
        or al, 0x04
.i:
        cmp word [f_z], 0
        jne .z
        or al, 0x02
.z:
        or al, [f_c]
        movzx eax, al
        ret

; AL = el registro P nuevo (en emulación, M y X siguen a 1). Con X a 1, la
; parte alta de X e Y, a 0
cpu_p_pon:
        mov [f_n], al
        mov ah, al
        shr ah, 6
        and ah, 1
        mov [f_v], ah
        cmp byte [f_e], 0
        jne .e
        mov ah, al
        shr ah, 5
        and ah, 1
        mov [f_m], ah
        mov ah, al
        shr ah, 4
        and ah, 1
        mov [f_x], ah
        jz .e
        and ecx, 0xFF
        and ebp, 0xFF
.e:
        mov ah, al
        shr ah, 3
        and ah, 1
        mov [f_d], ah
        mov ah, al
        shr ah, 2
        and ah, 1
        mov [f_i], ah
        mov ah, al
        not ah
        shr ah, 1
        and ah, 1
        mov [f_z], ah
        mov byte [f_z+1], 0
        and al, 1
        mov [f_c], al
        ret

; (XCE, reiniciar) con E a 1: M y X a 1, la parte alta de X, Y y S
cpu_modo_e:
        cmp byte [f_e], 0
        je .x
        mov byte [f_m], 1
        mov byte [f_x], 1
        and ecx, 0xFF
        and ebp, 0xFF
        mov byte [r_s+1], 1
.x:
        ret

; el ciclo de más de la página directa (con la parte baja de D distinta de 0)
cpu_d_extra:
        push eax
        xor eax, eax
        cmp byte [r_d], 0
        setne al
        mov [d_extra], eax
        pop eax
        ret

; el procesador como al encender
cpu_reiniciar:
        pushad
        xor eax, eax
        mov [r_a], eax
        mov [r_x], eax
        mov [r_y], eax
        mov [r_d], eax
        mov [d_extra], eax
        mov [r_dbr], eax
        mov [r_pbr], eax
        mov [cpu_romper], eax
        mov dword [r_s], 0x01FF
        mov byte [f_e], 1
        mov byte [f_m], 1
        mov byte [f_x], 1
        mov byte [f_i], 1
        mov byte [f_d], 0
        mov byte [f_c], 0
        mov byte [f_v], 0
        mov byte [f_n], 0
        mov word [f_z], 1
        mov byte [cpu_wai], 0
        mov byte [cpu_stp], 0
        mov byte [nmi_pend], 0
        mov edx, 0xFFFC
        call rd16_b0
        mov [r_pc], eax
        popad
        ret

; ---- el modo decimal (ADC y SBC con D a 1): cifra a cifra ----
; EAX = el operando; BL o BX = A. Dejan A, C, V, N y Z
adc_bcd8:
        mov byte [bcd_sub], 0
        jmp bcd8
sbc_bcd8:
        not eax
        mov byte [bcd_sub], 1
bcd8:
        and eax, 0xFF
        mov [bcd_m], eax
        movzx eax, bl
        mov [bcd_a], eax
        mov dword [bcd_n], 8
        call bcd_op
        mov bl, al
        NZ8 bl
        ret
adc_bcd16:
        mov byte [bcd_sub], 0
        jmp bcd16
sbc_bcd16:
        not eax
        mov byte [bcd_sub], 1
bcd16:
        and eax, 0xFFFF
        mov [bcd_m], eax
        movzx eax, bx
        mov [bcd_a], eax
        mov dword [bcd_n], 16
        call bcd_op
        mov bx, ax
        NZ16 bx
        ret
; [bcd_a] + [bcd_m] + C, cifra a cifra ([bcd_n] bits: 8 o 16); con
; [bcd_sub] (SBC, el operando ya complementado) se corrige restando 6 a la
; cifra que no se lleva nada. V sale como en binario, antes de corregir la
; última cifra -> EAX = el resultado; C y V puestos
bcd_op:
        push ebx
        push ecx
        push edx
        push esi
        push edi
        xor esi, esi                ; ESI = el resultado
        movzx edi, byte [f_c]       ; EDI = lo que se lleva
        xor ecx, ecx                ; CL = dónde va la cifra
.cifra:
        mov eax, [bcd_a]
        shr eax, cl
        and eax, 0x0F
        mov edx, [bcd_m]
        shr edx, cl
        and edx, 0x0F
        add eax, edx
        add eax, edi
        mov edx, [bcd_n]
        sub edx, 4
        cmp ecx, edx
        jne .nov
        push eax                    ; la última: V
        shl eax, cl
        or eax, esi
        mov edx, [bcd_a]
        xor eax, edx                ; A ^ R
        xor edx, [bcd_m]
        not edx                     ; ~(A ^ M)
        and eax, edx
        add cl, 3
        shr eax, cl
        sub cl, 3
        and al, 1
        mov [f_v], al
        pop eax
.nov:
        cmp byte [bcd_sub], 0
        jne .resta
        cmp eax, 9
        jbe .lleva
        add eax, 6
        jmp .lleva
.resta:
        cmp eax, 0x0F
        ja .lleva
        sub eax, 6
.lleva:
        xor edi, edi
        cmp eax, 0x0F
        jle .c
        inc edi
.c:
        and eax, 0x0F
        shl eax, cl
        or esi, eax
        add ecx, 4
        cmp ecx, [bcd_n]
        jb .cifra
        mov eax, edi
        mov [f_c], al
        mov eax, esi
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret
